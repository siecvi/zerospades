/*
 Copyright (c) 2013 yvt
 based on code of pysnip (c) Mathias Kaerlev 2011-2012.

 This file is part of OpenSpades.

 OpenSpades is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 OpenSpades is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with OpenSpades.  If not, see <http://www.gnu.org/licenses/>.

 */

#include "Client.h"

#include <Core/ConcurrentDispatch.h>
#include <Core/Settings.h>
#include <Core/Strings.h>

#include "BloodMarks.h"
#include "CTFGameMode.h"
#include "Corpse.h"
#include "IGameMode.h"
#include "Player.h"
#include "TCGameMode.h"

#include "ClientPlayer.h"
#include "ILocalEntity.h"

#include "GameMap.h"
#include "Grenade.h"
#include "Weapon.h"
#include "World.h"

#include "NetClient.h"

DEFINE_SPADES_SETTING(cg_fov, "68");
DEFINE_SPADES_SETTING(cg_horizontalFov, "0");
DEFINE_SPADES_SETTING(cg_classicZoom, "0");
DEFINE_SPADES_SETTING(cg_spectatorZoomScale, "4");
DEFINE_SPADES_SETTING(cg_thirdperson, "0");
DEFINE_SPADES_SETTING(cg_manualFocus, "0");
DEFINE_SPADES_SETTING(cg_depthOfFieldAmount, "1");
DEFINE_SPADES_SETTING(cg_shake, "1");
DEFINE_SPADES_SETTING(cg_debugBlockCursor, "0");
DEFINE_SPADES_SETTING(cg_debugPlayerHitboxes, "0");

SPADES_SETTING(cg_ragdoll);
SPADES_SETTING(cg_hurtScreenEffects);
SPADES_SETTING(cg_orientationSmoothing);

namespace spades {
	namespace client {

#pragma mark - Drawing

		ClientCameraMode Client::GetCameraMode() {
			if (!world)
				return ClientCameraMode::None;
			stmp::optional<Player&> p = world->GetLocalPlayer();
			if (!p)
				return ClientCameraMode::NotJoined;

			if (!p->IsSpectator() && p->IsAlive()) {
				// There exists an alive (non-spectator) local player
				if (cg_thirdperson && world->GetNumPlayers() <= 1)
					return ClientCameraMode::ThirdPersonLocal;
				return ClientCameraMode::FirstPersonLocal;
			} else {
				// The local player is dead or a spectator
				if (followCameraState.enabled) {
					bool isAlive = world->GetPlayer(followedPlayerId)->IsAlive();
					if (followCameraState.firstPerson && isAlive)
						return ClientCameraMode::FirstPersonFollow;
					else
						return ClientCameraMode::ThirdPersonFollow;
				} else {
					if (p->IsSpectator()) {
						return ClientCameraMode::Free;
					} else {
						// Look at your own cadaver!
						return ClientCameraMode::ThirdPersonLocal;
					}
				}
			}
		}

		int Client::GetCameraTargetPlayerId() {
			switch (GetCameraMode()) {
				case ClientCameraMode::None: SPUnreachable();
				case ClientCameraMode::NotJoined:
				case ClientCameraMode::Free:
				case ClientCameraMode::FirstPersonLocal:
				case ClientCameraMode::ThirdPersonLocal:
					SPAssert(world);
					return world->GetLocalPlayerIndex().value();
				case ClientCameraMode::FirstPersonFollow:
				case ClientCameraMode::ThirdPersonFollow:
					return followedPlayerId;
			}
			SPUnreachable();
		}

		Player& Client::GetCameraTargetPlayer() {
			return world->GetPlayer(GetCameraTargetPlayerId()).value();
		}

		bool Client::IsInFirstPersonView(int playerId) {
			return IsFirstPerson(GetCameraMode()) && GetCameraTargetPlayerId() == playerId;
		}

		float Client::GetLocalFireVibration() {
			float localFireVibration = time - localFireVibrationTime;
			localFireVibration = 1.0F - (localFireVibration / 0.1F);
			if (localFireVibration < 0.0F)
				localFireVibration = 0.0F;
			return localFireVibration;
		}

		float Client::GetAimDownZoomScale() {
			int playerId = GetCameraTargetPlayerId();
			Player& p = world->GetPlayer(playerId).value();
			if (!p.IsAlive() || !p.IsToolWeapon())
				return 1.0F;

			float adsState = clientPlayers[playerId]->GetAimDownState();
			float delta = 0.8F;
			switch (p.GetWeapon().GetWeaponType()) {
				case SMG_WEAPON: delta = 0.8F; break;
				case RIFLE_WEAPON: delta = 1.4F; break;
				case SHOTGUN_WEAPON: delta = 0.4F; break;
			}

			if (cg_classicZoom)
				delta = 1.0F;

			return 1.0F + (3.0F - 2.0F * powf(adsState, 1.5F)) * powf(adsState, 3.0F) * adsState;
		}

		SceneDefinition Client::CreateSceneDefinition() {
			SPADES_MARK_FUNCTION();

			SceneDefinition def;
			def.time = (unsigned int)(time * 1000.0F);
			def.denyCameraBlur = true;
			def.zFar = 160.0F;

			// Limit the range of cg_fov
			// (note: comparsion with a NaN always results in false)
			if (!((float)cg_fov < 110.0F))
				cg_fov = 110.0F;
			if (!((float)cg_fov > 45.0F))
				cg_fov = 45.0F;

			if (world) {
				IntVector3 fogColor = world->GetFogColor();
				renderer->SetFogColor(ConvertColorRGB(fogColor));

				int shakeLevel = cg_shake;

				auto& freeState = freeCameraState;
				auto& sharedState = followAndFreeCameraState;

				float roll = 0.0F;
				float scale = 1.0F;
				float vibPitch = 0.0F;
				float vibYaw = 0.0F;

				float sw = renderer->ScreenWidth();
				float sh = renderer->ScreenHeight();
				float ratio = (sw / sh);
				float fov = DEG2RAD(cg_fov);

				if (cg_horizontalFov) {
					def.fovX = fov;
					def.fovY = 2.0F * atanf(tanf(def.fovX * 0.5F) / ratio);
				} else {
					def.fovY = fov;
					def.fovX = 2.0F * atanf(tanf(def.fovY * 0.5F) * ratio);
				}

				def.blurVignette = 0.0F;

				switch (GetCameraMode()) {
					case ClientCameraMode::None: SPUnreachable();
					case ClientCameraMode::NotJoined: {
						def.viewOrigin = MakeVector3(256, 256, 4);
						def.viewAxis[0] = MakeVector3(-1, 0, 0);
						def.viewAxis[1] = MakeVector3(0, 1, 0);
						def.viewAxis[2] = MakeVector3(0, 0, 1);

						def.zNear = 0.05F;
						def.skipWorld = false;
						break;
					}
					case ClientCameraMode::FirstPersonLocal:
					case ClientCameraMode::FirstPersonFollow: {
						int playerId = GetCameraTargetPlayerId();
						Player& p = world->GetPlayer(playerId).value();
						Handle<ClientPlayer> clientPlayer = clientPlayers[playerId];

						Matrix4 eyeMatrix = clientPlayer->GetEyeMatrix();
						def.viewOrigin = eyeMatrix.GetOrigin();
						def.viewAxis[0] = -eyeMatrix.GetAxis(0);
						def.viewAxis[1] = -eyeMatrix.GetAxis(2);
						def.viewAxis[2] = eyeMatrix.GetAxis(1);

						if (shakeLevel >= 1) {
							float fireVibration = GetLocalFireVibration();
							fireVibration *= fireVibration;

							if (p.IsToolSpade())
								fireVibration *= 0.4F;

							roll += (SampleRandomFloat() - SampleRandomFloat()) * 0.03F * fireVibration;
							scale += SampleRandomFloat() * 0.04F * fireVibration;
							vibPitch += fireVibration * (1.0F - fireVibration) * 0.01F;
							vibYaw += sinf(fireVibration * M_PI_F * 2.0F) * 0.001F;

							def.radialBlur += fireVibration * 0.05F;

							// sprint bob
							{
								float sp = SmoothStep(clientPlayer->GetSprintState());
								float walkPrg = p.GetWalkAnimationProgress();
								float walkAng = walkPrg * M_PI_F * 2.0F;

								vibYaw += sinf(walkAng) * 0.01F * sp;
								roll -= sinf(walkAng) * 0.005F * sp;

								float per = cosf(walkAng);
								per = per * per;
								per *= per;
								per *= per;
								vibPitch += per * 0.01F * sp;

								if (shakeLevel >= 2) {
									vibYaw += coherentNoiseSamplers[0].Sample(walkPrg * 2.5F) * 0.005F * sp;
									vibPitch += coherentNoiseSamplers[1].Sample(walkPrg * 2.5F) * 0.01F * sp;
									roll += coherentNoiseSamplers[2].Sample(walkPrg * 2.5F) * 0.008F * sp;
									scale += 0.1F * sp;
								}
							}
						}

						// for 1st view, camera blur can be used
						def.denyCameraBlur = false;

						// DoF when doing ADS
						float per = GetAimDownState();
						per *= per * per;
						def.depthOfFieldFocalLength = per * 13.0F + 0.054F;

						// Hurt effect
						if (cg_hurtScreenEffects) {
							float hpper = p.GetHealth() / 100.0F;
							float timeSinceLastHurt = time - lastHurtTime;
							if (time >= lastHurtTime && timeSinceLastHurt < 0.15F) {
								float per = 1.0F - (timeSinceLastHurt / 0.15F);
								per *= 0.5F - hpper * 0.3F;
								def.blurVignette += per * 6.0F;
							}
							if (time >= lastHurtTime && timeSinceLastHurt < 0.2F) {
								float per = 1.0F - (timeSinceLastHurt / 0.2F);
								per *= 0.5F - hpper * 0.3F;
								def.saturation *= std::max(0.0F, 1.0F - per * 4.0F);
							}
						}

						// Apply ADS zoom
						scale /= GetAimDownZoomScale();

						// Update initial floating camera pos
						freeState.position = def.viewOrigin;
						freeState.velocity = MakeVector3(0, 0, 0);

						// Update initial floating camera angle
						Vector3 o = -def.viewAxis[2];
						sharedState.yaw = atan2f(o.y, o.x);
						sharedState.pitch = -atan2f(o.z, o.GetLength2D());
						break;
					}
					case ClientCameraMode::ThirdPersonLocal:
					case ClientCameraMode::ThirdPersonFollow: {
						Player& player = GetCameraTargetPlayer();

						Vector3 center = player.GetEye();
						if (!player.IsAlive()) {
							if (player.IsLocalPlayer() && lastLocalCorpse && cg_ragdoll)
								center = lastLocalCorpse->GetCenter();

							center.z -= 2.25F;
						}

						IntVector3 lp = center.Floor();
						if (map->IsSolidWrapped(lp.x, lp.y, lp.z)) {
							float z = center.z;
							while (z > center.z - 5.0F) {
								if (!map->IsSolidWrapped(lp.x, lp.y, (int)floorf(z))) {
									center.z = z;
									break;
								} else {
									z--;
								}
							}
						}

						float distance = 5.0F;
						if (player.IsLocalPlayer()
							&& !player.IsSpectator()
							&& !player.IsAlive()) { // deathcam.
							float timeSinceDeath = time - lastAliveTime;
							distance -= 3.0F * expf(-timeSinceDeath * 1.0F);
						}

						Vector3 eye = center;
						eye.x += cosf(sharedState.pitch) * cosf(sharedState.yaw) * distance;
						eye.y += cosf(sharedState.pitch) * sinf(sharedState.yaw) * distance;
						eye.z -= sinf(sharedState.pitch) * distance;

						// Prevent the camera from being behind a wall
						GameMap::RayCastResult res;
						res = map->CastRay2(center, (eye - center).Normalize(), 32);
						if (res.hit && !res.startSolid) {
							float dist = (res.hitPos - center).GetLength();
							float curDist = (eye - center).GetLength();
							dist -= 0.3F; // near clip plane
							if (curDist > dist) {
								float diff = curDist - dist;
								eye += (center - eye).Normalize() * diff;
							}
						}

						Vector3 front = (center - eye).Normalize();
						Vector3 up = {0, 0, -1};

						def.viewOrigin = eye;
						def.viewAxis[0] = -Vector3::Cross(up, front).Normalize();
						def.viewAxis[1] = -Vector3::Cross(front, def.viewAxis[0]);
						def.viewAxis[2] = front;

						// Update initial floating camera pos
						freeState.position = def.viewOrigin;
						freeState.velocity = MakeVector3(0, 0, 0);
						break;
					}
					case ClientCameraMode::Free: {
						// spectator view (noclip view)
						Vector3 center = freeState.position;
						Vector3 front;
						Vector3 up = {0, 0, -1};

						front.x = cosf(sharedState.pitch) * -cosf(sharedState.yaw);
						front.y = cosf(sharedState.pitch) * -sinf(sharedState.yaw);
						front.z = sinf(sharedState.pitch);

						def.viewOrigin = center;
						def.viewAxis[0] = -Vector3::Cross(up, front).Normalize();
						def.viewAxis[1] = -Vector3::Cross(front, def.viewAxis[0]);
						def.viewAxis[2] = front;

						// Apply spectator zoom
						scale /= Mix(1.0F, cg_spectatorZoomScale,
							SmoothStep(spectatorZoomState));

						def.denyCameraBlur = false;
						break;
					}
				}

				// Add vibration effects
				if (shakeLevel >= 1) {
					float nadeVib = grenadeVibration;
					if (nadeVib > 0.0F) {
						nadeVib *= 10.0F;
						if (nadeVib > 1.0F)
							nadeVib = 1.0F;
						roll += (SampleRandomFloat() - SampleRandomFloat()) * 0.2F * nadeVib;
						vibPitch += (SampleRandomFloat() - SampleRandomFloat()) * 0.1F * nadeVib;
						vibYaw += (SampleRandomFloat() - SampleRandomFloat()) * 0.1F * nadeVib;
						scale -= (SampleRandomFloat() - SampleRandomFloat()) * 0.1F * nadeVib;

						def.radialBlur += nadeVib * 0.1F;
					}
				}

				if (shakeLevel >= 2) {
					float nadeVib = grenadeVibrationSlow;
					if (nadeVib > 0.0F) {
						nadeVib *= 4.0F;
						if (nadeVib > 1.0F)
							nadeVib = 1.0F;
						nadeVib *= nadeVib;

						roll += coherentNoiseSamplers[0].Sample(time * 8.0F) * 0.2F * nadeVib;
						vibPitch += coherentNoiseSamplers[1].Sample(time * 12.0F) * 0.1F * nadeVib;
						vibYaw += coherentNoiseSamplers[2].Sample(time * 11.0F) * 0.1F * nadeVib;
					}
				}

				// Add roll / scale
				{
					Vector3 right = def.viewAxis[0];
					Vector3 up = def.viewAxis[1];

					def.viewAxis[0] = right * cosf(roll) - up * sinf(roll);
					def.viewAxis[1] = up * cosf(roll) + right * sinf(roll);

					def.fovX = 2.0F * atanf(tanf(def.fovX * 0.5F) * scale);
					def.fovY = 2.0F * atanf(tanf(def.fovY * 0.5F) * scale);
				}

				// Add pitch (up/down)
				{
					Vector3 u = def.viewAxis[1];
					Vector3 v = def.viewAxis[2];

					def.viewAxis[1] = u * cosf(vibPitch) - v * sinf(vibPitch);
					def.viewAxis[2] = v * cosf(vibPitch) + u * sinf(vibPitch);
				}

				// Add yaw (left/right)
				{
					Vector3 u = def.viewAxis[0];
					Vector3 v = def.viewAxis[2];

					def.viewAxis[0] = u * cosf(vibYaw) - v * sinf(vibYaw);
					def.viewAxis[2] = v * cosf(vibYaw) + u * sinf(vibYaw);
				}

				// Need to move the far plane because there's no vertical fog
				if (def.viewOrigin.z < 0.0F)
					def.zFar -= def.viewOrigin.z;

				if ((bool)cg_manualFocus) {
					// Depth of field is manually controlled
					def.depthOfFieldNearBlurStrength = def.depthOfFieldFarBlurStrength =
					  0.5F * (float)cg_depthOfFieldAmount;
					def.depthOfFieldFocalLength = focalLength;
				} else {
					def.depthOfFieldNearBlurStrength = cg_depthOfFieldAmount;
					def.depthOfFieldFarBlurStrength = 0.0F;
				}

				def.zNear = 0.05F;
				def.skipWorld = false;
			} else {
				SPAssert(GetCameraMode() == ClientCameraMode::None);

				// Let there be darkness
				def.viewOrigin = MakeVector3(0, 0, 0);
				def.viewAxis[0] = MakeVector3(1, 0, 0);
				def.viewAxis[1] = MakeVector3(0, 0, -1);
				def.viewAxis[2] = MakeVector3(0, 0, 1);

				def.fovY = def.fovX = 0.0F;

				def.zNear = 0.05F;
				def.skipWorld = true;

				renderer->SetFogColor(MakeVector3(0, 0, 0));
			}

			SPAssert(!def.viewOrigin.IsNaN());

			def.radialBlur = std::min(def.radialBlur, 1.0F);

			return def;
		}

		void Client::AddGrenadeToScene(Grenade& g) {
			SPADES_MARK_FUNCTION();

			if (g.GetPosition().z > 63.0F)
				return; // work-around for water refraction problem

			Handle<IModel> model = renderer->RegisterModel("Models/Weapons/Grenade/Grenade.kv6");

			// Move the grenade slightly so that it doesn't look like sinking in the ground
			Vector3 position = g.GetPosition();
			position.z -= 0.03F * 3.0F;

			ModelRenderParam param;
			param.matrix = Matrix4::Translate(position);
			param.matrix = param.matrix * g.GetOrientation().ToRotationMatrix();
			param.matrix = param.matrix * Matrix4::Scale(0.03F);
			renderer->RenderModel(*model, param);
		}

		void Client::AddDebugObjectToScene(const spades::OBB3& obb, const Vector4& color) {
			const auto& m = obb.m;
			Vector3 v[2][2][2];
			v[0][0][0] = (m * MakeVector3(0, 0, 0)).GetXYZ();
			v[0][0][1] = (m * MakeVector3(0, 0, 1)).GetXYZ();
			v[0][1][0] = (m * MakeVector3(0, 1, 0)).GetXYZ();
			v[0][1][1] = (m * MakeVector3(0, 1, 1)).GetXYZ();
			v[1][0][0] = (m * MakeVector3(1, 0, 0)).GetXYZ();
			v[1][0][1] = (m * MakeVector3(1, 0, 1)).GetXYZ();
			v[1][1][0] = (m * MakeVector3(1, 1, 0)).GetXYZ();
			v[1][1][1] = (m * MakeVector3(1, 1, 1)).GetXYZ();

			renderer->AddDebugLine(v[0][0][0], v[1][0][0], color);
			renderer->AddDebugLine(v[0][0][1], v[1][0][1], color);
			renderer->AddDebugLine(v[0][1][0], v[1][1][0], color);
			renderer->AddDebugLine(v[0][1][1], v[1][1][1], color);

			renderer->AddDebugLine(v[0][0][0], v[0][1][0], color);
			renderer->AddDebugLine(v[0][0][1], v[0][1][1], color);
			renderer->AddDebugLine(v[1][0][0], v[1][1][0], color);
			renderer->AddDebugLine(v[1][0][1], v[1][1][1], color);

			renderer->AddDebugLine(v[0][0][0], v[0][0][1], color);
			renderer->AddDebugLine(v[0][1][0], v[0][1][1], color);
			renderer->AddDebugLine(v[1][0][0], v[1][0][1], color);
			renderer->AddDebugLine(v[1][1][0], v[1][1][1], color);
		}

		void Client::AddMapObjectsToScene() {
			SPADES_MARK_FUNCTION();

			stmp::optional<IGameMode&> mode = world->GetMode();
			if (!mode)
				return;

			Handle<IModel> base = renderer->RegisterModel("Models/MapObjects/CheckPoint.kv6");
			Handle<IModel> intel = renderer->RegisterModel("Models/MapObjects/Intel.kv6");

			if (mode->ModeType() == IGameMode::m_CTF) {
				auto& ctf = dynamic_cast<CTFGameMode&>(mode.value());
				for (int tId = 0; tId < 2; tId++) {
					CTFGameMode::Team& team = ctf.GetTeam(tId);

					ModelRenderParam param;
					param.customColor = ConvertColorRGB(world->GetTeamColor(tId));

					// draw base
					param.matrix = Matrix4::Translate(team.basePos);
					param.matrix = param.matrix * Matrix4::Scale(0.3F);
					renderer->RenderModel(*base, param);

					// draw both flags
					if (!ctf.GetTeam(1 - tId).hasIntel) {
						param.matrix = Matrix4::Translate(team.flagPos);
						param.matrix = param.matrix * Matrix4::Rotate(MakeVector3(0, 0, 1), time);
						param.matrix = param.matrix * Matrix4::Scale(0.1F);
						renderer->RenderModel(*intel, param);
					}
				}
			} else if (mode->ModeType() == IGameMode::m_TC) {
				auto& tc = dynamic_cast<TCGameMode&>(mode.value());
				for (int i = 0; i < tc.GetNumTerritories(); i++) {
					TCGameMode::Territory& t = tc.GetTerritory(i);
					IntVector3 col = (t.ownerTeamId >= NEUTRAL_TEAM)
					                   ? MakeIntVector3(255, 255, 255)
					                   : world->GetTeamColor(t.ownerTeamId);

					ModelRenderParam param;
					param.customColor = ConvertColorRGB(col);

					// draw base
					param.matrix = Matrix4::Translate(t.pos);
					param.matrix = param.matrix * Matrix4::Scale(0.3F);
					renderer->RenderModel(*base, param);
				}
			}
		}

		void Client::DrawScene() {
			SPADES_MARK_FUNCTION();

			renderer->StartScene(lastSceneDef);

			if (world) {
				stmp::optional<Player&> maybePlayer = world->GetLocalPlayer();

				for (size_t i = 0; i < world->GetNumPlayerSlots(); i++) {
					if (world->GetPlayer(static_cast<unsigned int>(i))) {
						SPAssert(clientPlayers[i]);
						clientPlayers[i]->AddToScene();
					}
				}

				for (const auto& nade : world->GetAllGrenades())
					AddGrenadeToScene(*nade);

				for (const auto& c : corpses) {
					if ((c->GetCenter() - lastSceneDef.viewOrigin).GetSquaredLength2D() > FOG_DISTANCE_SQ)
						continue;
					c->AddToScene();
				}

				// draw map objects
				AddMapObjectsToScene();

				for (const auto& ent : localEntities)
					ent->Render3D();

				bloodMarks->Draw();

				if (maybePlayer) { // localplayer exists
					Player& p = maybePlayer.value();

					// draw block cursor
					if (p.IsToolBlock() && p.IsReadyToUseTool() && CanLocalPlayerUseTool()) {
						bool blockCursorActive = p.IsBlockCursorActive();
						bool blockCursorDragging = p.IsBlockCursorDragging();

						if (blockCursorActive || blockCursorDragging) {
							std::vector<IntVector3> cells;
							IntVector3 curPos = p.GetBlockCursorPos();
							IntVector3 dragPos = p.GetBlockCursorDragPos();
							if (blockCursorDragging)
								cells = world->CubeLine(dragPos, curPos, 64);
							else
								cells.push_back(curPos);

							int blocks = static_cast<int>(cells.size());
							bool valid = blocks <= p.GetNumBlocks();
							bool active = blockCursorActive && valid;
							bool debugCursor = cg_debugBlockCursor;

							Handle<IModel> curLine = renderer->RegisterModel("Models/MapObjects/BlockCursorLine.kv6");

							for (const auto& v : cells) {
								const Vector3 blockF = MakeVector3(v);
								const Vector3 color(1.0F, valid, active);

								ModelRenderParam param;
								param.ghost = true;
								param.opacity = active ? 0.5F : 0.25F;
								param.customColor = color;
								param.matrix = Matrix4::Translate(blockF + 0.5F);
								param.matrix = param.matrix * Matrix4::Scale(0.1F);

								if (debugCursor) {
									AddDebugObjectToScene(
									  AABB3(blockF.x, blockF.y, blockF.z, 1, 1, 1),
									  MakeVector4(color.x, color.y, color.z, 1));
									continue;
								}

								// hide cursor if needed to stop z-fighting
								if (blocks > 2 && map->IsSolid(v.x, v.y, v.z))
									continue;

								renderer->RenderModel(*curLine, param);
							}
						}
					}
				}
			}

			for (const auto& lights : flashDlights)
				renderer->AddLight(lights);
			flashDlightsOld.clear();
			flashDlightsOld.swap(flashDlights);

			// draw player hottrack
			// FIXME: don't use debug line
			if (cg_debugPlayerHitboxes) {
				auto hottracked = HotTrackedPlayer();
				if (hottracked) {
					Player& player = std::get<0>(*hottracked);
					hitTag_t tag = std::get<1>(*hottracked);

					Vector4 col = MakeVector4(1, 1, 1, 1);
					Vector4 col2 = ConvertColorRGBA(player.GetColor());

					Player::HitBoxes hb = player.GetHitBoxes(cg_orientationSmoothing); // interpolated
					AddDebugObjectToScene(hb.head, (tag & hit_Head) ? col : col2);
					AddDebugObjectToScene(hb.torso, (tag & hit_Torso) ? col : col2);
					AddDebugObjectToScene(hb.limbs[0], (tag & hit_Legs) ? col : col2);
					AddDebugObjectToScene(hb.limbs[1], (tag & hit_Legs) ? col : col2);
					AddDebugObjectToScene(hb.limbs[2], (tag & hit_Arms) ? col : col2);
				}
			}

			renderer->EndScene();
		}

		void Client::UpdateMatrices() {
			lastViewProjectionScreenMatrix =
				(Matrix4::Scale(renderer->ScreenWidth() * 0.5F, renderer->ScreenHeight() * -0.5F, 1)
					* Matrix4::Translate(1, -1, 1))
				* lastSceneDef.ToOpenGLProjectionMatrix()
				* lastSceneDef.ToViewMatrix();
		}

		bool Client::Project(const spades::Vector3& worldPos, spades::Vector2& scrPos) {
			Vector4 scrHomV = lastViewProjectionScreenMatrix * worldPos;
			if (scrHomV.z <= 0.0F) {
				scrHomV.w = 0.001F;
				return false;
			}
			scrPos = MakeVector2(scrHomV.x, scrHomV.y) / scrHomV.w;
			return true;
		}
	} // namespace client
} // namespace spades
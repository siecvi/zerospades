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
#include "GameMap.h"
#include "Grenade.h"
#include "IGameMode.h"
#include "Player.h"
#include "TCGameMode.h"
#include "Weapon.h"
#include "World.h"

#include "ClientPlayer.h"
#include "ILocalEntity.h"

#include "NetClient.h"

DEFINE_SPADES_SETTING(cg_fov, "68");
DEFINE_SPADES_SETTING(cg_horizontalFov, "0");
DEFINE_SPADES_SETTING(cg_classicZoomedFov, "0");
DEFINE_SPADES_SETTING(cg_thirdperson, "0");
DEFINE_SPADES_SETTING(cg_manualFocus, "0");
DEFINE_SPADES_SETTING(cg_depthOfFieldAmount, "1");
DEFINE_SPADES_SETTING(cg_shake, "1");

SPADES_SETTING(cg_ragdoll);

namespace spades {
	namespace client {

#pragma mark - Drawing

		ClientCameraMode Client::GetCameraMode() {
			if (!world)
				return ClientCameraMode::None;
			stmp::optional<Player&> p = world->GetLocalPlayer();
			if (!p)
				return ClientCameraMode::NotJoined;

			if (p->IsAlive() && !p->IsSpectator()) {
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
				case ClientCameraMode::None:
				case ClientCameraMode::NotJoined:
				case ClientCameraMode::Free: SPUnreachable();
				case ClientCameraMode::FirstPersonLocal:
				case ClientCameraMode::ThirdPersonLocal:
					SPAssert(world);
					return world->GetLocalPlayerIndex().value();
				case ClientCameraMode::FirstPersonFollow:
				case ClientCameraMode::ThirdPersonFollow: return followedPlayerId;
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
			float localFireVibration = 0.0F;
			localFireVibration = time - localFireVibrationTime;
			localFireVibration = 1.0F - localFireVibration / 0.1F;
			if (localFireVibration < 0.0F)
				localFireVibration = 0.0F;
			return localFireVibration;
		}

		float Client::GetAimDownZoomScale() {
			Player& p = GetCameraTargetPlayer();
			if (!p.IsToolWeapon() || !p.IsAlive())
				return 1.0F;

			ClientPlayer& clientPlayer = *clientPlayers[p.GetId()];

			// I don't even know if this is entirely legal
			float delta = 0.8F;
			switch (p.GetWeapon().GetWeaponType()) {
				case SMG_WEAPON: delta = 0.8F; break;
				case RIFLE_WEAPON: delta = 1.4F; break;
				case SHOTGUN_WEAPON: delta = 0.4F; break;
			}

			if (cg_classicZoomedFov)
				delta = 1.0F;

			float ads = clientPlayer.GetAimDownState();

			return 1.0F + (3.0F - 2.0F * powf(ads, 1.5F)) * powf(ads, 3.0F) * delta;
		}

		SceneDefinition Client::CreateSceneDefinition() {
			SPADES_MARK_FUNCTION();

			SceneDefinition def;
			def.time = (unsigned int)(time * 1000.0F);
			def.denyCameraBlur = true;
			def.zFar = 160.0F;

			// Limit the range of cg_fov
			cg_fov = Clamp((float)cg_fov, 45.0F, 180.0F);

			auto sw = renderer->ScreenWidth();
			auto sh = renderer->ScreenHeight();

			float ratio = (sw / sh);
			float fov = DEG2RAD(cg_fov);

			int shakeLevel = cg_shake;

			if (world) {
				IntVector3 fogColor = world->GetFogColor();
				renderer->SetFogColor(ConvertColorRGB(fogColor));

				def.blurVignette = 0.0F;

				float roll = 0.0F;
				float scale = 1.0F;
				float vibPitch = 0.0F;
				float vibYaw = 0.0F;

				switch (GetCameraMode()) {
					case ClientCameraMode::None: SPUnreachable();
					case ClientCameraMode::NotJoined:
						def.viewOrigin = MakeVector3(256, 256, 4);
						def.viewAxis[0] = MakeVector3(-1, 0, 0);
						def.viewAxis[1] = MakeVector3(0, 1, 0);
						def.viewAxis[2] = MakeVector3(0, 0, 1);

						if (!cg_horizontalFov) {
							def.fovY = fov;
							def.fovX = 2.0F * atanf(tanf(def.fovY * 0.5F) * ratio);
						} else {
							def.fovX = fov;
							def.fovY = 2.0F * atanf(tanf(def.fovX * 0.5F) / ratio);
						}

						def.zNear = 0.05F;
						def.skipWorld = false;
						break;
					case ClientCameraMode::FirstPersonLocal:
					case ClientCameraMode::FirstPersonFollow: {
						Player& p = GetCameraTargetPlayer();

						Matrix4 eyeMatrix = clientPlayers[p.GetId()]->GetEyeMatrix();

						def.viewOrigin = eyeMatrix.GetOrigin();
						def.viewAxis[0] = -eyeMatrix.GetAxis(0);
						def.viewAxis[1] = -eyeMatrix.GetAxis(2);
						def.viewAxis[2] = eyeMatrix.GetAxis(1);

						if (!cg_horizontalFov) {
							def.fovY = fov;
							def.fovX = 2.0F * atanf(tanf(def.fovY * 0.5F) * ratio);
						} else {
							def.fovX = fov;
							def.fovY = 2.0F * atanf(tanf(def.fovX * 0.5F) / ratio);
						}

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
								float sp = SmoothStep(GetSprintState());
								float wv = p.GetWalkAnimationProgress();

								vibYaw += sinf(wv * M_PI_F * 2.0F) *  0.01F * sp;
								roll -= sinf(wv * M_PI_F * 2.0F) * 0.005F * sp;
								float per = cosf(wv * M_PI_F * 2.0F);
								per = per * per * per * per;
								vibPitch += per * 0.01F * sp;

								if (shakeLevel >= 2) {
									vibYaw += coherentNoiseSamplers[0].Sample(wv * 2.5F) * 0.005F * sp;
									vibPitch += coherentNoiseSamplers[1].Sample(wv * 2.5F) * 0.01F * sp;
									roll += coherentNoiseSamplers[2].Sample(wv * 2.5F) * 0.008F * sp;
									scale += sp * 0.1F;
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
						{
							auto hpper = p.GetHealth() / 100.0F;
							float wTime = world->GetTime();
							if (wTime - lastHurtTime < 0.15F && wTime >= lastHurtTime) {
								float per = 1.0F - (wTime - lastHurtTime) / 0.15F;
								per *= 0.5F - hpper * 0.3F;
								def.blurVignette += per * 6.0F;
							}
							if (wTime - lastHurtTime < 0.2F && wTime >= lastHurtTime) {
								float per = 1.0F - (wTime - lastHurtTime) / 0.2F;
								per *= 0.5F - hpper * 0.3F;
								def.saturation *= std::max(0.0F, 1.0F - per * 4.0F);
							}
						}

						// Apply ADS zoom
						scale /= GetAimDownZoomScale();

						// Update initial floating camera pos
						freeCameraState.position = def.viewOrigin;
						freeCameraState.velocity = MakeVector3(0, 0, 0);

						// Update initial floating camera view
						Vector3 o = def.viewAxis[2];
						followAndFreeCameraState.yaw = atan2f(o.y, o.x) + DEG2RAD(180);
						followAndFreeCameraState.pitch = atan2f(o.z, o.GetLength2D());
						break;
					}
					case ClientCameraMode::ThirdPersonLocal:
					case ClientCameraMode::ThirdPersonFollow: {
						auto localplayer = world->GetLocalPlayer();
						Player& player = GetCameraTargetPlayer();
						Vector3 center = player.GetEye();

						if (!player.IsAlive()) {
							if (cg_ragdoll && lastLocalCorpse && &player == localplayer)
								center = lastLocalCorpse->GetCenter();

							center.z -= 2.0F;
						}

						auto lp = center.Floor();
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
						if (&player == localplayer && !localplayer->IsSpectator()
							&& !localplayer->IsAlive()) { // deathcam.
							float timeSinceDeath = time - lastAliveTime;
							distance -= 3.0F * expf(-timeSinceDeath * 1.0F);
						}

						auto& state = followAndFreeCameraState;
						Vector3 eye = center;
						eye.x += cosf(state.yaw) * cosf(state.pitch) * distance;
						eye.y += sinf(state.yaw) * cosf(state.pitch) * distance;
						eye.z -= sinf(state.pitch) * distance;

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

						if (!cg_horizontalFov) {
							def.fovY = fov;
							def.fovX = 2.0F * atanf(tanf(def.fovY * 0.5F) * ratio);
						} else {
							def.fovX = fov;
							def.fovY = 2.0F * atanf(tanf(def.fovX * 0.5F) / ratio);
						}

						// Update initial floating camera pos
						freeCameraState.position = def.viewOrigin;
						freeCameraState.velocity = MakeVector3(0, 0, 0);
						break;
					}
					case ClientCameraMode::Free: {
						// spectator view (noclip view)
						Vector3 center = freeCameraState.position;
						Vector3 front;
						Vector3 up = {0, 0, -1};

						auto& state = followAndFreeCameraState;
						front.x = -cosf(state.yaw) * cosf(state.pitch);
						front.y = -sinf(state.yaw) * cosf(state.pitch);
						front.z = sinf(state.pitch);

						def.viewOrigin = center;
						def.viewAxis[0] = -Vector3::Cross(up, front).Normalize();
						def.viewAxis[1] = -Vector3::Cross(front, def.viewAxis[0]);
						def.viewAxis[2] = front;

						if (!cg_horizontalFov) {
							def.fovY = fov;
							def.fovX = 2.0F * atanf(tanf(def.fovY * 0.5F) * ratio);
						} else {
							def.fovX = fov;
							def.fovY = 2.0F * atanf(tanf(def.fovX * 0.5F) / ratio);
						}

						def.denyCameraBlur = false;
						break;
					}
				}

				// Add vibration effects
				{
					float nadeVib = grenadeVibration;
					if (nadeVib > 0.0F) {
						if (shakeLevel >= 1) {
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
				}
				{
					float nadeVib = grenadeVibrationSlow;
					if (nadeVib > 0.0F) {
						if (shakeLevel >= 2) {
							nadeVib *= 4.0F;
							if (nadeVib > 1.0F)
								nadeVib = 1.0F;
							nadeVib *= nadeVib;

							roll += coherentNoiseSamplers[0].Sample(time * 8.0F) * 0.2F * nadeVib;
							vibPitch += coherentNoiseSamplers[1].Sample(time * 12.0F) * 0.1F * nadeVib;
							vibYaw += coherentNoiseSamplers[2].Sample(time * 11.0F) * 0.1F * nadeVib;
						}
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

				def.fovY = fov;
				def.fovX = 2.0F * atanf(tanf(def.fovY * 0.5F) * ratio);

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
			Matrix4 mat = g.GetOrientation().ToRotationMatrix() * Matrix4::Scale(0.03F);
			mat = Matrix4::Translate(position) * mat;
			param.matrix = mat;

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

		void Client::DrawCTFObjects() {
			SPADES_MARK_FUNCTION();

			CTFGameMode& mode = dynamic_cast<CTFGameMode&>(world->GetMode().value());
			Handle<IModel> base = renderer->RegisterModel("Models/MapObjects/CheckPoint.kv6");
			Handle<IModel> intel = renderer->RegisterModel("Models/MapObjects/Intel.kv6");

			for (int tId = 0; tId < 2; tId++) {
				CTFGameMode::Team& team = mode.GetTeam(tId);
				auto col = world->GetTeam(tId).color;

				ModelRenderParam param;
				param.customColor = ConvertColorRGB(col);

				// draw base
				param.matrix = Matrix4::Translate(team.basePos);
				param.matrix = param.matrix * Matrix4::Scale(0.3F);
				renderer->RenderModel(*base, param);

				// draw flag
				if (!mode.GetTeam(1 - tId).hasIntel) {
					param.matrix = Matrix4::Translate(team.flagPos);
					param.matrix = param.matrix * Matrix4::Scale(0.1F);
					param.matrix *= Matrix4::Rotate(MakeVector3(0, 0, 1), time);
					renderer->RenderModel(*intel, param);
				}
			}
		}

		void Client::DrawTCObjects() {
			SPADES_MARK_FUNCTION();

			TCGameMode& mode = dynamic_cast<TCGameMode&>(world->GetMode().value());
			Handle<IModel> base = renderer->RegisterModel("Models/MapObjects/CheckPoint.kv6");

			for (int tId = 0; tId < mode.GetNumTerritories(); tId++) {
				TCGameMode::Territory& t = mode.GetTerritory(tId);
				IntVector3 col = (t.ownerTeamId == 2) ? MakeIntVector3(255)
				    : world->GetTeam(t.ownerTeamId).color;

				ModelRenderParam param;
				param.customColor = ConvertColorRGB(col);

				// draw base
				param.matrix = Matrix4::Translate(t.pos);
				param.matrix = param.matrix * Matrix4::Scale(0.3F);
				renderer->RenderModel(*base, param);
			}
		}

		void Client::DrawScene() {
			SPADES_MARK_FUNCTION();

			renderer->StartScene(lastSceneDef);

			if (world) {
				stmp::optional<Player&> p = world->GetLocalPlayer();

				for (size_t i = 0; i < world->GetNumPlayerSlots(); i++) {
					if (world->GetPlayer(static_cast<unsigned int>(i))) {
						SPAssert(clientPlayers[i]);
						clientPlayers[i]->AddToScene();
					}
				}

				for (const auto& nade : world->GetAllGrenades())
					AddGrenadeToScene(*nade);

				for (const auto& c : corpses) {
					if ((c->GetCenter() - lastSceneDef.viewOrigin).GetLength2D() > FOG_DISTANCE)
						continue;
					c->AddToScene();
				}

				IGameMode& mode = *world->GetMode();
				if (IGameMode::m_CTF == mode.ModeType())
					DrawCTFObjects();
				else if (IGameMode::m_TC == mode.ModeType())
					DrawTCObjects();

				for (const auto& ent : localEntities)
					ent->Render3D();

				bloodMarks->Draw();

				// Draw block cursor
				if (p) {
					if (p->IsAlive() && p->IsToolBlock() && p->IsReadyToUseTool() &&
					    (p->IsBlockCursorActive() || p->IsBlockCursorDragging()) &&
					    CanLocalPlayerUseTool()) {
						std::vector<IntVector3> blocks;
						auto curPos = p->GetBlockCursorPos();
						auto dragPos = p->GetBlockCursorDragPos();
						if (p->IsBlockCursorDragging())
							blocks = world->CubeLine(curPos, dragPos, 64);
						else
							blocks.push_back(curPos);

						bool active = p->IsBlockCursorActive();
						int numBlocks = (int)blocks.size();

						Handle<IModel> curLine = renderer->RegisterModel("Models/MapObjects/BlockCursorLine.kv6");

						for (const auto& v : blocks) {
							Vector3 const color(
							  /* Red   (X) */ 1.0F,
							  /* Green (Y) */ (numBlocks > p->GetNumBlocks()) ? 0.0F : 1.0F,
							  /* Blue  (Z) */ active ? 1.0F : 0.0F);

							bool solid = (numBlocks > 1) && map->IsSolid(v.x, v.y, v.z);
							ModelRenderParam param;
							param.ghost = true;
							param.opacity = (active && !solid) ? 0.5F : 0.25F;
							param.customColor = color;
							param.matrix = Matrix4::Translate(MakeVector3(v) + 0.5F);

							// Make cursor larger if needed to stop z-fighting
							param.matrix = param.matrix * Matrix4::Scale(1.0F / 10.0F + (solid ? 0.0005F : 0.0F));
							renderer->RenderModel(*curLine, param);

							numBlocks--;
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
			auto hottracked = HotTrackedPlayer();
			if (hottracked) {
				Player& player = std::get<0>(*hottracked);
				hitTag_t tag = std::get<1>(*hottracked);

				Vector4 color = ConvertColorRGBA(player.GetColor());
				Vector4 color2 = MakeVector4(1, 1, 1, 1);

				Player::HitBoxes hb = player.GetHitBoxes();
				AddDebugObjectToScene(hb.head, (tag & hit_Head) ? color2 : color);
				AddDebugObjectToScene(hb.torso, (tag & hit_Torso) ? color2 : color);
				AddDebugObjectToScene(hb.limbs[0], (tag & hit_Legs) ? color2 : color);
				AddDebugObjectToScene(hb.limbs[1], (tag & hit_Legs) ? color2 : color);
				AddDebugObjectToScene(hb.limbs[2], (tag & hit_Arms) ? color2 : color);
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

		bool Client::Project(spades::Vector3 v, spades::Vector3& out) {
			Vector4 screenHomV = lastViewProjectionScreenMatrix * v;
			if (screenHomV.z <= 0.0F) {
				screenHomV.w = 0.001F;
				return false;
			}
			out = screenHomV.GetXYZ() / screenHomV.w;
			return true;
		}
	} // namespace client
} // namespace spades
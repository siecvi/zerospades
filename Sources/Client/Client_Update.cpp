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

#include "IAudioChunk.h"
#include "IAudioDevice.h"

#include "BloodMarks.h"
#include "CenterMessageView.h"
#include "ChatWindow.h"
#include "ClientPlayer.h"
#include "ClientUI.h"
#include "Corpse.h"
#include "FallingBlock.h"
#include "HurtRingView.h"
#include "ILocalEntity.h"
#include "LimboView.h"
#include "MapView.h"
#include "PaletteView.h"
#include "Tracer.h"

#include "GameMap.h"
#include "Grenade.h"
#include "Weapon.h"
#include "World.h"

#include "NetClient.h"

DEFINE_SPADES_SETTING(cg_ragdoll, "1");
SPADES_SETTING(cg_blood);
DEFINE_SPADES_SETTING(cg_ejectBrass, "1");
DEFINE_SPADES_SETTING(cg_hitFeedbackSoundGain, "0.2");
DEFINE_SPADES_SETTING(cg_tracers, "1");
DEFINE_SPADES_SETTING(cg_tracersFirstPerson, "1");
DEFINE_SPADES_SETTING(cg_analyze, "0");
DEFINE_SPADES_SETTING(cg_scoreMessages, "0");

SPADES_SETTING(cg_alerts);
SPADES_SETTING(cg_centerMessage);
SPADES_SETTING(cg_shake);
SPADES_SETTING(cg_holdAimDownSight);

namespace spades {
	namespace client {

#pragma mark - World States

		float Client::GetSprintState() {
			stmp::optional<ClientPlayer&> p = GetLocalClientPlayer();
			if (!p)
				return 0.0F;
			return p->GetSprintState();
		}

		float Client::GetAimDownState() {
			stmp::optional<ClientPlayer&> p = GetLocalClientPlayer();
			if (!p)
				return 0.0F;
			return p->GetAimDownState();
		}

		bool Client::CanLocalPlayerUseTool() {
			if (!world || !world->GetLocalPlayer() || !world->GetLocalPlayer()->IsAlive())
				return false;

			// Player is unable to use a tool while/soon after sprinting
			if (GetSprintState() > 0 || world->GetLocalPlayer()->GetInput().sprint)
				return false;

			stmp::optional<ClientPlayer&> p = GetLocalClientPlayer();

			// Player is unable to use a tool while switching to another tool
			if (p.value().IsChangingTool())
				return false;

			return true;
		}

		bool Client::CanLocalPlayerUseWeapon() {
			if (!CanLocalPlayerUseTool())
				return false;

			// Player is unable to use a weapon while reloading (except shotgun)
			if (world->GetLocalPlayer()->IsAwaitingReloadCompletion() &&
			    !world->GetLocalPlayer()->GetWeapon().IsReloadSlow())
				return false;

			return true;
		}

		stmp::optional<ClientPlayer&> Client::GetLocalClientPlayer() {
			if (!world || !world->GetLocalPlayerIndex())
				return {};

			return clientPlayers.at(static_cast<std::size_t>(*world->GetLocalPlayerIndex()));
		}

#pragma mark - World Actions

		/** Captures the color of the block player is looking at. */
		void Client::CaptureColor() {
			if (!world)
				return;
			stmp::optional<Player&> p = world->GetLocalPlayer();
			if (!p || !p->IsAlive())
				return;

			uint32_t col;
			IntVector3 pos;
			if (!world->GetMap()->CastRay(p->GetEye(), p->GetFront(), FOG_DISTANCE, pos)) {
				auto c = world->GetFogColor();
				col = c.x | c.y << 8 | c.z << 16;
			} else {
				col = world->GetMap()->GetColorWrapped(pos.x, pos.y, pos.z);
			}

			p->SetHeldBlockColor(IntVectorFromColor(col));
			net->SendHeldBlockColor();
		}

		void Client::SetSelectedTool(Player::ToolType type, bool quiet) {
			if (type == world->GetLocalPlayer()->GetTool())
				return;
			lastTool = world->GetLocalPlayer()->GetTool();
			hasLastTool = true;

			world->GetLocalPlayer()->SetTool(type);

			// TODO: We should send tool after raise cooldown, not before...
			net->SendTool();

			if (!quiet) {
				Handle<IAudioChunk> c = audioDevice->RegisterSound("Sounds/Weapons/SwitchLocal.opus");
				audioDevice->PlayLocal(c.GetPointerOrNull(), MakeVector3(0.4F, -0.3F, 0.5F), AudioParam());
			}
		}

#pragma mark - World Update

		void Client::UpdateWorld(float dt) {
			SPADES_MARK_FUNCTION();

			stmp::optional<Player&> p = world->GetLocalPlayer();

			if (p) {
				// disable input when UI is open
				if (scriptedUI->NeedsInput()) {
					weapInput.primary = false;
					if (p->IsSpectator() || !p->IsToolWeapon())
						weapInput.secondary = false;
				}

				if (p->IsSpectator())
					UpdateLocalSpectator(dt);
				else
					UpdateLocalPlayer(dt);

				if (p->IsAlive() && !p->IsSpectator()) {
					// send position packet - 1 per second
					if (time - lastPosSentTime > 1.0F) {
						net->SendPosition(p->GetPosition());
						lastPosSentTime = time;
					}
				}
			}

#if 0
			// dynamic time step
			// physics diverges from server
			world->Advance(dt);
#else
			// accurately resembles server's physics
			// but not smooth
			if (dt > 0.0F)
				worldSubFrame += dt;

			float frameStep = 1.0F / 60.0F;
			while (worldSubFrame >= frameStep) {
				world->Advance(frameStep);
				worldSubFrame -= frameStep;
			}
#endif

			// update player view (doesn't affect physics/game logics)
			for (const auto& clientPlayer : clientPlayers) {
				if (clientPlayer)
					clientPlayer->Update(dt);
			}

			// corpse never accesses audio nor renderer, so
			// we can do it in the separate thread
			class CorpseUpdateDispatch : public ConcurrentDispatch {
				Client& client;
				float dt;

			public:
				CorpseUpdateDispatch(Client& c, float dt) : client{c}, dt{dt} {}
				void Run() override {
					for (const auto& c : client.corpses) {
						for (int i = 0; i < 4; i++)
							c->Update(dt / 4.0F);
					}
				}
			};
			CorpseUpdateDispatch corpseDispatch{*this, dt};
			corpseDispatch.Start();

			// local entities should be done in the client thread
			{
				decltype(localEntities)::iterator it;
				std::vector<decltype(it)> its;
				for (it = localEntities.begin(); it != localEntities.end(); it++) {
					if (!(*it)->Update(dt))
						its.push_back(it);
				}
				for (const auto& it : its)
					localEntities.erase(it);
			}

			bloodMarks->Update(dt);
			corpseDispatch.Join();

			if (grenadeVibration > 0.0F) {
				grenadeVibration -= dt;
				if (grenadeVibration < 0.0F)
					grenadeVibration = 0.0F;
			}

			if (grenadeVibrationSlow > 0.0F) {
				grenadeVibrationSlow -= dt;
				if (grenadeVibrationSlow < 0.0F)
					grenadeVibrationSlow = 0.0F;
			}

			if (hitFeedbackIconState > 0.0F) {
				hitFeedbackIconState -= dt * 4.0F;
				if (hitFeedbackIconState < 0.0F)
					hitFeedbackIconState = 0.0F;
			}

			if (debugHitTestZoom) {
				debugHitTestZoomState += dt * 10.0F;
				if (debugHitTestZoomState > 1.0F)
					debugHitTestZoomState = 1.0F;
			} else {
				debugHitTestZoomState -= dt * 10.0F;
				if (debugHitTestZoomState < 0.0F)
					debugHitTestZoomState = 0.0F;
			}
		}

		/** Handles movement of spectating local player. */
		void Client::UpdateLocalSpectator(float dt) {
			SPADES_MARK_FUNCTION();

			auto& sharedState = followAndFreeCameraState;
			auto& freeState = freeCameraState;

			Vector3 lastPos = freeState.position;
			freeState.velocity *= powf(0.3F, dt);
			freeState.position += freeState.velocity * dt;

			if (freeState.position.x < 0.0F)
				freeState.velocity.x = fabsf(freeState.velocity.x) * 0.2F;
			if (freeState.position.y < 0.0F)
				freeState.velocity.y = fabsf(freeState.velocity.y) * 0.2F;
			if (freeState.position.x > (float)GetWorld()->GetMap()->Width())
				freeState.velocity.x = fabsf(freeState.velocity.x) * -0.2F;
			if (freeState.position.y > (float)GetWorld()->GetMap()->Height())
				freeState.velocity.y = fabsf(freeState.velocity.y) * -0.2F;

			freeState.position = lastPos + freeState.velocity * dt;

			GameMap::RayCastResult minResult;
			float minDist = 1.E+10F;
			Vector3 minShift;

			auto const direction = freeState.position - lastPos;

			// check collision
			if (freeState.velocity.GetLength() < 0.01F) {
				freeState.position = lastPos;
				freeState.velocity *= 0.0F;
			} else {
				for (int sx = -1; sx <= 1; sx++)
				for (int sy = -1; sy <= 1; sy++)
				for (int sz = -1; sz <= 1; sz++) {
					Vector3 shift = {sx * 0.1F, sy * 0.1F, sz * 0.1F};

					GameMap::RayCastResult res;
					res = map->CastRay2(lastPos + shift, direction, 32);
					if (res.hit && !res.startSolid &&
						Vector3::Dot(res.hitPos - freeState.position - shift, direction) < 0.0F) {
						float dist = Vector3::Dot(res.hitPos - freeState.position - shift,
								                  direction.Normalize());
						if (dist < minDist) {
							minResult = res;
							minDist = dist;
							minShift = shift;
						}
					}
				}
			}

			if (minDist < 1.E+9F) {
				Vector3 hitPos = minResult.hitPos - minShift;
				Vector3 normal = MakeVector3(minResult.normal);
				freeState.position = hitPos + (normal * 0.05F);

				// bounce
				float dot = Vector3::Dot(freeState.velocity, normal);
				freeState.velocity -= normal * (dot * 1.25F);
			}

			// acceleration
			Vector3 front;
			Vector3 up = {0, 0, -1};

			front.x = -cosf(sharedState.yaw) * cosf(sharedState.pitch);
			front.y = -sinf(sharedState.yaw) * cosf(sharedState.pitch);
			front.z = sinf(sharedState.pitch);

			Vector3 right = -Vector3::Cross(up, front).Normalize();
			Vector3 up2 = Vector3::Cross(right, front).Normalize();

			float f = dt * 10.0F;
			if (playerInput.sprint)
				f *= 3.0F;
			else if (playerInput.sneak)
				f *= 0.75F;

			front *= f;
			right *= f;
			up2 *= f;

			if (playerInput.moveForward)
				freeState.velocity += front;
			else if (playerInput.moveBackward)
				freeState.velocity -= front;

			if (playerInput.moveLeft)
				freeState.velocity -= right;
			else if (playerInput.moveRight)
				freeState.velocity += right;

			if (playerInput.jump)
				freeState.velocity += up2;
			else if (playerInput.crouch)
				freeState.velocity -= up2;

			SPAssert(freeState.velocity.GetLength() < 100.0F);
		}

		/** Handles movement of joined local player. */
		void Client::UpdateLocalPlayer(float dt) {
			SPADES_MARK_FUNCTION();

			Player& player = GetWorld()->GetLocalPlayer().value();
			Weapon& weapon = player.GetWeapon();

			PlayerInput inp = playerInput;
			WeaponInput winp = weapInput;

			Vector3 vel = player.GetVelocity();
			if (vel.GetLength2D() < 0.1F)
				inp.sprint = false;

			// Can't use a tool while sprinting or switching to another tool, etc.
			if (!CanLocalPlayerUseTool()) {
				winp.primary = false;
				winp.secondary = false;
			}

			// Disable weapon while reloading (except shotgun)
			if (player.GetTool() == Player::ToolWeapon) {
				if (player.IsAwaitingReloadCompletion() && !weapon.IsReloadSlow()) {
					winp.primary = false;
					winp.secondary = false;
				}
			}

			player.SetInput(inp);
			player.SetWeaponInput(winp);

			// Uncrouching may be prevented by an obstacle
			inp.crouch = player.GetInput().crouch;

			// send player input
			{
				PlayerInput sentInput = inp;
				WeaponInput sentWeaponInput = winp;

				// FIXME: send only there are any changed?
				net->SendPlayerInput(sentInput);
				net->SendWeaponInput(sentWeaponInput);
			}

			if (hasDelayedReload) {
				world->GetLocalPlayer()->Reload();
				net->SendReload();
				hasDelayedReload = false;
			}

			WeaponInput actualWeapInput = player.GetWeaponInput();

			// there is a possibility that player has respawned or something.
			if (!(actualWeapInput.secondary && player.IsToolWeapon() && player.IsAlive())
				&& !(cg_holdAimDownSight && weapInput.secondary)) {
				weapInput.secondary = false; // stop aiming down
			}

			// is the selected tool no longer usable (ex. out of ammo)?
			if (!player.IsToolSelectable(player.GetTool())) {
				// release mouse button before auto-switching tools
				winp.primary = false;
				winp.secondary = false;
				weapInput = winp;
				net->SendWeaponInput(weapInput);
				actualWeapInput = winp = player.GetWeaponInput();

				// select another tool
				Player::ToolType t = player.GetTool();
				do {
					switch (t) {
						case Player::ToolSpade: t = Player::ToolGrenade; break;
						case Player::ToolBlock: t = Player::ToolSpade; break;
						case Player::ToolWeapon: t = Player::ToolBlock; break;
						case Player::ToolGrenade: t = Player::ToolWeapon; break;
					}
				} while (!world->GetLocalPlayer()->IsToolSelectable(t));
				SetSelectedTool(t);
			}

			// send orientation packet - 20 per second
			Vector3 curFront = player.GetFront();
			if (curFront != lastFront && time - lastOriSentTime > (1.0F / 20.0F)) {
				net->SendOrientation(curFront);
				lastOriSentTime = time;
				lastFront = curFront;
			}

			lastScore = world->GetPlayerScore(player.GetId());

			// show block count when building block lines.
			if (player.IsAlive() && player.IsBlockCursorDragging()) {
				if (player.IsBlockCursorActive()) {
					int blocks = world->CubeLine(player.GetBlockCursorDragPos(),
						player.GetBlockCursorPos(), 64).size();
					auto msg = _TrN("Client", "{0} block", "{0} blocks", blocks);
					auto type = (blocks > player.GetNumBlocks())
						? AlertType::Warning : AlertType::Notice;
					ShowAlert(msg, type, 0.0F, true);
				} else { // invalid
					auto msg = _Tr("Client", "-- blocks");
					ShowAlert(msg, AlertType::Warning, 0.0F, true);
				}
			}

			if (player.IsAlive())
				lastAliveTime = time;

			if (player.GetHealth() < lastHealth) { // ouch!
				lastHealth = player.GetHealth();
				lastHurtTime = world->GetTime();

				Handle<IAudioChunk> c;
				switch (SampleRandomInt(0, 3)) {
					case 0: c = audioDevice->RegisterSound("Sounds/Weapons/Impacts/FleshLocal1.opus");
						break;
					case 1: c = audioDevice->RegisterSound("Sounds/Weapons/Impacts/FleshLocal2.opus");
						break;
					case 2: c = audioDevice->RegisterSound("Sounds/Weapons/Impacts/FleshLocal3.opus");
						break;
					case 3: c = audioDevice->RegisterSound("Sounds/Weapons/Impacts/FleshLocal4.opus");
						break;
				}
				audioDevice->PlayLocal(c.GetPointerOrNull(), AudioParam());

				float hpper = player.GetHealth() / 100.0F;
				int cnt = 18 - (int)(hpper * 8.0F);
				hurtSprites.resize(std::max(cnt, 6));
				for (size_t i = 0; i < hurtSprites.size(); i++) {
					HurtSprite& spr = hurtSprites[i];
					spr.angle = SampleRandomFloat() * M_PI_F * 2.0F;
					spr.scale = 0.2F + SampleRandomFloat() * SampleRandomFloat() * 0.7F;
					spr.horzShift = SampleRandomFloat();
					spr.strength = 0.3F + SampleRandomFloat() * 0.7F;
					if (hpper > 0.5F)
						spr.strength *= 1.5F - hpper;
				}
			} else {
				if (player.GetHealth() > lastHealth && lastHealth > 0)
					lastHealTime = world->GetTime();

				lastHealth = player.GetHealth();
			}
		}

#pragma mark - IWorldListener Handlers

		void Client::PlayerObjectSet(int id) {
			if (clientPlayers[id])
				clientPlayers[id] = nullptr;

			stmp::optional<Player&> p = world->GetPlayer(id);
			if (p)
				clientPlayers[id] = Handle<ClientPlayer>::New(*p, *this);
		}

		void Client::PlayerJumped(spades::client::Player& p) {
			SPADES_MARK_FUNCTION();

			if (!IsMuted()) {
				Handle<IAudioChunk> c =
				  p.GetWade() ? audioDevice->RegisterSound("Sounds/Player/WaterJump.opus")
				              : audioDevice->RegisterSound("Sounds/Player/Jump.opus");
				audioDevice->Play(c.GetPointerOrNull(), p.GetOrigin(), AudioParam());
			}
		}

		void Client::PlayerLanded(spades::client::Player& p, bool hurt) {
			SPADES_MARK_FUNCTION();

			if (!IsMuted()) {
				Handle<IAudioChunk> c =
				  hurt ? audioDevice->RegisterSound("Sounds/Player/FallHurt.opus")
				       : (p.GetWade() ? audioDevice->RegisterSound("Sounds/Player/WaterLand.opus")
				                      : audioDevice->RegisterSound("Sounds/Player/Land.opus"));
				audioDevice->Play(c.GetPointerOrNull(), p.GetOrigin(), AudioParam());
			}
		}

		void Client::PlayerMadeFootstep(spades::client::Player& p) {
			SPADES_MARK_FUNCTION();

			if (!IsMuted()) {
				std::array<const char*, 4> snds = {
				  "Sounds/Player/Footstep1.opus", "Sounds/Player/Footstep2.opus",
				  "Sounds/Player/Footstep3.opus", "Sounds/Player/Footstep4.opus"
				};
				std::array<const char*, 6> rsnds = {
				  "Sounds/Player/Run1.opus", "Sounds/Player/Run2.opus",
				  "Sounds/Player/Run3.opus", "Sounds/Player/Run4.opus",
				  "Sounds/Player/Run5.opus", "Sounds/Player/Run6.opus"
				};
				std::array<const char*, 4> wsnds = {
				  "Sounds/Player/Wade1.opus", "Sounds/Player/Wade2.opus",
				  "Sounds/Player/Wade3.opus", "Sounds/Player/Wade4.opus"
				};

				bool sprinting = clientPlayers[p.GetId()]
					? (clientPlayers[p.GetId()]->GetSprintState() > 0.5F) : false;

				Handle<IAudioChunk> c =
				  audioDevice->RegisterSound(SampleRandomElement(p.GetWade() ? wsnds : snds));
				audioDevice->Play(c.GetPointerOrNull(), p.GetOrigin(), AudioParam());
				if (sprinting && !p.GetWade()) {
					AudioParam param;
					param.volume *= clientPlayers[p.GetId()]->GetSprintState();
					c = audioDevice->RegisterSound(SampleRandomElement(rsnds));
					audioDevice->Play(c.GetPointerOrNull(), p.GetOrigin(), param);
				}
			}
		}

		void Client::PlayerFiredWeapon(spades::client::Player& p) {
			SPADES_MARK_FUNCTION();

			if (p.IsLocalPlayer())
				localFireVibrationTime = time;

			clientPlayers.at(p.GetId())->FiredWeapon();
		}

		void Client::PlayerEjectedBrass(spades::client::Player& p) {
			SPADES_MARK_FUNCTION();

			clientPlayers.at(p.GetId())->EjectedBrass();
		}

		void Client::PlayerDryFiredWeapon(spades::client::Player& p) {
			SPADES_MARK_FUNCTION();

			if (!IsMuted()) {
				Handle<IAudioChunk> c = audioDevice->RegisterSound("Sounds/Weapons/DryFire.opus");
				if (p.IsLocalPlayer())
					audioDevice->PlayLocal(c.GetPointerOrNull(), MakeVector3(0.4F, -0.3F, 0.5F),
					                       AudioParam());
				else
					audioDevice->Play(c.GetPointerOrNull(),
					                  p.GetEye() + p.GetFront() * 0.5F - p.GetUp() * 0.3F +
					                    p.GetRight() * 0.4F,
					                  AudioParam());
			}
		}

		void Client::PlayerReloadingWeapon(spades::client::Player& p) {
			SPADES_MARK_FUNCTION();

			clientPlayers.at(p.GetId())->ReloadingWeapon();
		}

		void Client::PlayerReloadedWeapon(spades::client::Player& p) {
			SPADES_MARK_FUNCTION();

			clientPlayers.at(p.GetId())->ReloadedWeapon();
		}

		void Client::PlayerChangedTool(spades::client::Player& p) {
			SPADES_MARK_FUNCTION();

			if (p.IsLocalPlayer())
				return; // played by ClientPlayer::Update

			if (!IsMuted()) {
				Handle<IAudioChunk> c = audioDevice->RegisterSound("Sounds/Weapons/Switch.opus");
				audioDevice->Play(c.GetPointerOrNull(),
				                  p.GetEye() + p.GetFront() * 0.5F - p.GetUp() * 0.3F +
				                    p.GetRight() * 0.4F,
				                  AudioParam());
			}
		}

		void Client::PlayerRestocked(spades::client::Player& p) {
			if (!IsMuted()) {
				Handle<IAudioChunk> c;
				if (p.IsLocalPlayer()) {
					c = audioDevice->RegisterSound("Sounds/Weapons/RestockLocal.opus");
					audioDevice->PlayLocal(c.GetPointerOrNull(), MakeVector3(0.4F, -0.3F, 0.5F),
					                       AudioParam());
				} else {
					c = audioDevice->RegisterSound("Sounds/Weapons/Restock.opus");
					audioDevice->Play(c.GetPointerOrNull(),
					                  p.GetEye() + p.GetFront() * 0.5F - p.GetUp() * 0.3F +
					                    p.GetRight() * 0.4F,
					                  AudioParam());
				}
			}
		}

		void Client::PlayerThrewGrenade(spades::client::Player& p,
		                                stmp::optional<const Grenade&> g) {
			SPADES_MARK_FUNCTION();

			if (!IsMuted()) {
				Handle<IAudioChunk> c =
				  audioDevice->RegisterSound("Sounds/Weapons/Grenade/Throw.opus");
				if (p.IsLocalPlayer()) {
					if (g)
						net->SendGrenade(*g);

					audioDevice->PlayLocal(c.GetPointerOrNull(), MakeVector3(0.4F, 0.1F, 0.3F),
					                       AudioParam());
				} else {
					audioDevice->Play(c.GetPointerOrNull(),
					                  p.GetEye() + p.GetFront() * 0.9F - p.GetUp() * 0.2F +
					                    p.GetRight() * 0.3F,
					                  AudioParam());
				}
			}
		}

		void Client::PlayerMissedSpade(spades::client::Player& p) {
			SPADES_MARK_FUNCTION();

			if (!IsMuted()) {
				Handle<IAudioChunk> c =
				  audioDevice->RegisterSound("Sounds/Weapons/Spade/Miss.opus");
				if (p.IsLocalPlayer())
					audioDevice->PlayLocal(c.GetPointerOrNull(), MakeVector3(0.2F, -0.1F, 0.7F),
					                       AudioParam());
				else
					audioDevice->Play(c.GetPointerOrNull(),
					                  p.GetOrigin() + p.GetFront() * 0.9F + p.GetUp() * 1.25F,
					                  AudioParam());
			}
		}

		void Client::PlayerHitBlockWithSpade(spades::client::Player& p, Vector3 hitPos,
		                                     IntVector3 blockPos, IntVector3 normal) {
			SPADES_MARK_FUNCTION();

			Vector3 shiftedHitPos = hitPos + (MakeVector3(normal) * 0.05F);
			uint32_t col = map->GetColor(blockPos.x, blockPos.y, blockPos.z);
			EmitBlockFragments(shiftedHitPos, IntVectorFromColor(col));

			bool isLocal = p.IsLocalPlayer();
			if (isLocal)
				localFireVibrationTime = time;

			if (!IsMuted()) {
				Handle<IAudioChunk> c =
				  audioDevice->RegisterSound("Sounds/Weapons/Spade/HitBlock.opus");
				if (isLocal)
					audioDevice->PlayLocal(c.GetPointerOrNull(), MakeVector3(0.1F, -0.1F, 1.2F),
					                       AudioParam());
				else
					audioDevice->Play(c.GetPointerOrNull(), shiftedHitPos, AudioParam());
			}
		}

		void Client::PlayerKilledPlayer(spades::client::Player& killer,
			spades::client::Player& victim, KillType kt) {
			// Don't play hit sound on local: see BullethitPlayer
			if (kt == KillTypeWeapon || kt == KillTypeHeadshot && !victim.IsLocalPlayer()) {
				if (!IsMuted()) {
					Handle<IAudioChunk> c;
					switch (SampleRandomInt(0, 2)) {
						case 0: c = audioDevice->RegisterSound("Sounds/Weapons/Impacts/Flesh1.opus");
							break;
						case 1: c = audioDevice->RegisterSound("Sounds/Weapons/Impacts/Flesh2.opus");
							break;
						case 2: c = audioDevice->RegisterSound("Sounds/Weapons/Impacts/Flesh3.opus");
							break;
					}

					AudioParam param;
					param.volume = 4.0F;
					audioDevice->Play(c.GetPointerOrNull(), victim.GetEye(), param);
				}
			}

			// The local player is dead; initialize the look-you-are-dead cam
			if (victim.IsLocalPlayer()) {
				followCameraState.enabled = false;

				Vector3 o = victim.GetFront();
				followAndFreeCameraState.yaw = atan2f(o.y, o.x) + DEG2RAD(180);
				followAndFreeCameraState.pitch = DEG2RAD(30);
			}

			// Register local kills
			if (&killer != &victim && killer.IsLocalPlayer()) {
				curKills++;
				curStreak++;
			}

			// Register local deaths
			if (victim.IsLocalPlayer()) {
				curDeaths++;
				lastStreak = curStreak;
				if (curStreak > bestStreak)
					bestStreak = curStreak;
				curStreak = 0;
			}

			// emit blood (also for local player)
			// FIXME: emiting blood for either
			// client-side or server-side hit?
			switch (kt) {
				case KillTypeGrenade:
				case KillTypeHeadshot:
				case KillTypeMelee:
				case KillTypeWeapon: Bleed(victim.GetEye()); break;
				default: break;
			}

			// create ragdoll corpse
			if (cg_ragdoll && !victim.IsSpectator()) {
				auto corp = stmp::make_unique<Corpse>(*renderer, *map, victim);

				if (victim.IsLocalPlayer())
					lastLocalCorpse = corp.get();

				if (&killer != &victim && kt != KillTypeGrenade) {
					Vector3 dir = victim.GetPosition() - killer.GetPosition();
					dir = dir.Normalize();
					if (kt == KillTypeMelee) {
						dir *= 6.0F;
					} else {
						if (killer.IsWeaponSMG())
							dir *= 2.8F;
						else if (killer.IsWeaponShotgun())
							dir *= 4.5F;
						else
							dir *= 3.5F;
					}

					corp->AddImpulse(dir);
				} else if (kt == KillTypeGrenade) {
					corp->AddImpulse(MakeVector3(0, 0, -4.0F - SampleRandomFloat() * 4.0F));
				}

				corp->AddImpulse(victim.GetVelocity() * 32.0F);
				corpses.emplace_back(std::move(corp));

				if (corpses.size() > corpseHardLimit)
					corpses.pop_front();
				else if (corpses.size() > corpseSoftLimit)
					RemoveInvisibleCorpses();
			}

			// add chat message
			std::string s, cause;

			s += ChatWindow::TeamColorMessage(killer.GetName(), killer.GetTeamId());
			s += " [";
			Weapon& w = killer.GetWeapon(); // only used in case of KillTypeWeapon
			switch (kt) {
				case KillTypeWeapon: cause += _Tr("Client", w.GetName().c_str()); break;
				case KillTypeFall: cause += _Tr("Client", "Fall"); break;
				case KillTypeMelee: cause += _Tr("Client", "Melee"); break;
				case KillTypeGrenade: cause += _Tr("Client", "Grenade"); break;
				case KillTypeHeadshot: cause += _Tr("Client", "Headshot"); break;
				case KillTypeTeamChange: cause += _Tr("Client", "Team Change"); break;
				case KillTypeClassChange: cause += _Tr("Client", "Weapon Change"); break;
				default: cause += "???"; break;
			}

			if (&killer != &victim && killer.IsTeamMate(&victim))
				s += ChatWindow::ColoredMessage(cause, MsgColorFriendlyFire);
			else if (killer.IsLocalPlayer() || victim.IsLocalPlayer())
				s += ChatWindow::ColoredMessage(cause, MsgColorGray);
			else
				s += cause;
			s += "] ";

			if (&killer != &victim)
				s += ChatWindow::TeamColorMessage(victim.GetName(), victim.GetTeamId());

			killfeedWindow->AddMessage(s);

			// log to netlog
			if (&killer != &victim) {
				NetLog("%s (%s) [%s] %s (%s)", killer.GetName().c_str(),
				       world->GetTeam(killer.GetTeamId()).name.c_str(), cause.c_str(),
				       victim.GetName().c_str(), world->GetTeam(victim.GetTeamId()).name.c_str());
			} else {
				NetLog("%s (%s) [%s]", killer.GetName().c_str(),
				       world->GetTeam(killer.GetTeamId()).name.c_str(), cause.c_str());
			}

			// show big message if player is involved
			if (&killer != &victim) {
				if (killer.IsLocalPlayer() || victim.IsLocalPlayer()) {
					std::string msg;
					if (killer.IsLocalPlayer()) {
						if ((int)cg_centerMessage == 2)
							msg = _Tr("Client", "You've killed {0}", victim.GetName());
					} else {
						msg = _Tr("Client", "You were killed by {0}", killer.GetName());
					}
					centerMessageView->AddMessage(msg);

					if (killer.IsLocalPlayer() && cg_scoreMessages) {
						std::string s;
						s += ChatWindow::ColoredMessage("+1", MsgColorSysInfo);
						s += " point for neutralizing an enemy";
						chatWindow->AddMessage(s);
					}
				}
			}
		}

		void Client::BulletHitPlayer(spades::client::Player& hurtPlayer, HitType type,
		                             spades::Vector3 hitPos, spades::client::Player& by,
		                             std::unique_ptr<IBulletHitScanState>& stateCell) {
			SPADES_MARK_FUNCTION();

			SPAssert(type != HitTypeBlock);

			// spatter blood
			{
				bool const byLocalPlayer = by.IsLocalPlayer();
				float const distance = (by.GetEye() - hitPos).GetLength();
				Vector3 const direction = (by.GetEye() - hitPos).Normalize();

				float frontSpeed = 8.0F;
				float backSpeed = 0.0F;

				if (type == HitTypeMelee) {
					// Blunt
					frontSpeed = 1.5F;
				} else if (by.IsWeaponRifle()) {
					// Penetrating
					frontSpeed = 1.0F;
					backSpeed = 21.0F;
				} else if (by.IsWeaponSMG() && distance < 20.0F * SampleRandomFloat()) {
					// Penetrating
					frontSpeed = 1.0F;
					backSpeed = 12.0F;
				}

				if (frontSpeed > 0.0F)
					bloodMarks->Spatter(hitPos, direction * frontSpeed, byLocalPlayer);
				if (backSpeed > 0.0F)
					bloodMarks->Spatter(hitPos, direction * -backSpeed, byLocalPlayer);
			}

			// don't bleed local player
			if (!IsInFirstPersonView(hurtPlayer.GetId()))
				Bleed(hitPos);

			if (hurtPlayer.IsLocalPlayer()) {
				// don't player hit sound now;
				// local bullet impact sound is
				// played by checking the decrease of HP
				return;
			}

			// This function gets called for each pellet. We want to play these sounds no more than
			// once for each instance of firing. `BulletHitScanState`, stored in `stateCell`, tells
			// whether we have played each sound for the current firing session.
			struct BulletHitScanState : IBulletHitScanState {
				bool hasPlayedNormalHitSound = false;
				bool hasPlayedHeadshotSound = false;
			};

			if (!stateCell)
				stateCell = stmp::make_unique<BulletHitScanState>();

			auto& hitScanState = dynamic_cast<BulletHitScanState&>(*stateCell);

			if (!IsMuted() && !hitScanState.hasPlayedNormalHitSound) {
				Handle<IAudioChunk> c;

				if (type == HitTypeMelee) {
					c = audioDevice->RegisterSound("Sounds/Weapons/Spade/HitPlayer.opus");
					audioDevice->Play(c.GetPointerOrNull(), hitPos, AudioParam());
				} else {
					switch (SampleRandomInt(0, 2)) {
						case 0:
							c = audioDevice->RegisterSound("Sounds/Weapons/Impacts/Flesh1.opus");
							break;
						case 1:
							c = audioDevice->RegisterSound("Sounds/Weapons/Impacts/Flesh2.opus");
							break;
						case 2:
							c = audioDevice->RegisterSound("Sounds/Weapons/Impacts/Flesh3.opus");
							break;
					}

					AudioParam param;
					param.volume = 4.0F;
					audioDevice->Play(c.GetPointerOrNull(), hitPos, param);
				}

				hitScanState.hasPlayedNormalHitSound = true;
			}

			if (by.IsLocalPlayer()) {
				net->SendHit(hurtPlayer.GetId(), type);

				if (type != HitTypeMelee) {
					DamageIndicator damages;
					damages.damage = by.GetWeapon().GetDamage(type);
					damages.fade = 2.0F;
					damages.position = hitPos;
					damages.velocity = RandomAxis() * 4.0F;
					damageIndicators.push_back(damages);
				}

				if ((bool)cg_analyze) {
					char buf[256];

					std::string hitType;
					switch (type) {
						case HitTypeTorso: hitType = "Body"; break;
						case HitTypeHead: hitType = "Head"; break;
						case HitTypeArms:
						case HitTypeLegs: hitType = "Limb"; break;
						default: hitType = "Melee"; break;
					}

					auto playerName = ChatWindow::TeamColorMessage(hurtPlayer.GetName(), hurtPlayer.GetTeamId());
					int dist = (int)(by.GetPosition() - hurtPlayer.GetPosition()).GetLength();
					float dt = (world->GetTime() - lastHitTime) * 1000;

					if (dt > 0.0F && lastHitTime > 0.0F)
						sprintf(buf, "Bullet hit %s dist: %d blocks dT: %.0fms %s",
						        playerName.c_str(), dist, dt, hitType.c_str());
					else
						sprintf(buf, "Bullet hit %s dist: %d blocks dT: NA %s",
						        playerName.c_str(), dist, hitType.c_str());

					scriptedUI->RecordChatLog(buf);
					chatWindow->AddMessage(buf);
				}

				if (type == HitTypeHead && !hitScanState.hasPlayedHeadshotSound) {
					Handle<IAudioChunk> c =
					  audioDevice->RegisterSound("Sounds/Feedback/HeadshotFeedback.opus");
					AudioParam param;
					param.volume = cg_hitFeedbackSoundGain;
					audioDevice->PlayLocal(c.GetPointerOrNull(), param);

					hitScanState.hasPlayedHeadshotSound = true;
				}

				lastHitTime = world->GetTime();
				hitFeedbackIconState = 1.0F;
				hitFeedbackFriendly = by.IsTeamMate(&hurtPlayer);
			}
		}

		void Client::BulletHitBlock(Vector3 hitPos, IntVector3 blockPos, IntVector3 normal) {
			SPADES_MARK_FUNCTION();

			Vector3 shiftedHitPos = hitPos + (MakeVector3(normal) * 0.05F);

			if (blockPos.z == 63) {
				BulletHitWaterSurface(shiftedHitPos);

				if (!IsMuted()) {
					AudioParam param;
					param.volume = 2.0F;
					param.pitch = 0.9F + SampleRandomFloat() * 0.2F;

					Handle<IAudioChunk> c;
					switch (SampleRandomInt(0, 3)) {
						case 0:
							c = audioDevice->RegisterSound("Sounds/Weapons/Impacts/Water1.opus");
							break;
						case 1:
							c = audioDevice->RegisterSound("Sounds/Weapons/Impacts/Water2.opus");
							break;
						case 2:
							c = audioDevice->RegisterSound("Sounds/Weapons/Impacts/Water3.opus");
							break;
						case 3:
							c = audioDevice->RegisterSound("Sounds/Weapons/Impacts/Water4.opus");
							break;
					}
					audioDevice->Play(c.GetPointerOrNull(), shiftedHitPos, param);
				}
			} else {
				uint32_t col = map->GetColor(blockPos.x, blockPos.y, blockPos.z);
				EmitBlockFragments(shiftedHitPos, IntVectorFromColor(col));

				if (!IsMuted()) {
					AudioParam param;
					param.volume = 2.0F;

					Handle<IAudioChunk> c;
					c = audioDevice->RegisterSound("Sounds/Weapons/Impacts/Block.opus");
					audioDevice->Play(c.GetPointerOrNull(), shiftedHitPos, param);

					param.pitch = 0.9F + SampleRandomFloat() * 0.2F;
					switch (SampleRandomInt(0, 3)) {
						case 0:
							c = audioDevice->RegisterSound("Sounds/Weapons/Impacts/Ricochet1.opus");
							break;
						case 1:
							c = audioDevice->RegisterSound("Sounds/Weapons/Impacts/Ricochet2.opus");
							break;
						case 2:
							c = audioDevice->RegisterSound("Sounds/Weapons/Impacts/Ricochet3.opus");
							break;
						case 3:
							c = audioDevice->RegisterSound("Sounds/Weapons/Impacts/Ricochet4.opus");
							break;
					}
					audioDevice->Play(c.GetPointerOrNull(), shiftedHitPos, param);
				}
			}
		}

		void Client::AddBulletTracer(spades::client::Player& player,
			spades::Vector3 muzzlePos, spades::Vector3 hitPos) {
			SPADES_MARK_FUNCTION();

			if (!cg_tracers)
				return;

			bool isFirstPerson = IsInFirstPersonView(player.GetId());

			// If disabled, do not display tracers for bullets fired by the local player
			if (!cg_tracersFirstPerson && isFirstPerson)
				return;

			// The line segment containing `muzzlePos` and `hitPos` represents the accurate
			// trajectory of the fired bullet (as far as the game physics is concerned), but
			// displaying it as-is would make it seem like it was fired from a skull gun. Rewrite
			// the starting point with the visual muzzle point of the current weapon skin.
			Handle<ClientPlayer> clientPlayer = clientPlayers[player.GetId()];
			muzzlePos = clientPlayer->ShouldRenderInThirdPersonView()
			              ? clientPlayer->GetMuzzlePosition()
			              : clientPlayer->GetMuzzlePositionInFirstPersonView();

			float vel;
			switch (player.GetWeapon().GetWeaponType()) {
				case RIFLE_WEAPON: vel = 700.0F; break;
				case SMG_WEAPON: vel = 360.0F; break;
				case SHOTGUN_WEAPON: vel = 550.0F; break;
				default: vel = 0.0F; break;
			}

			// Not to give the false illusion that the bullets travel slow
			if (isFirstPerson)
				vel *= 2.0F;

			AddLocalEntity(stmp::make_unique<Tracer>(*this, muzzlePos, hitPos, vel));
			AddLocalEntity(stmp::make_unique<MapViewTracer>(muzzlePos, hitPos, vel));
		}

		void Client::BlocksFell(std::vector<IntVector3> blocks) {
			SPADES_MARK_FUNCTION();

			if (blocks.empty())
				return;

			AddLocalEntity(stmp::make_unique<FallingBlock>(this, blocks));

			if (!IsMuted()) {
				Vector3 origin = MakeVector3(blocks[0]) + 0.5F;
				Handle<IAudioChunk> c = audioDevice->RegisterSound("Sounds/Misc/BlockFall.opus");
				audioDevice->Play(c.GetPointerOrNull(), origin, AudioParam());
			}
		}

		void Client::GrenadeBounced(const Grenade& g) {
			SPADES_MARK_FUNCTION();

			Vector3 origin = g.GetPosition();
			if (!IsMuted() && origin.z < 63.0F) {
				Handle<IAudioChunk> c =
				  audioDevice->RegisterSound("Sounds/Weapons/Grenade/Bounce.opus");
				audioDevice->Play(c.GetPointerOrNull(), origin, AudioParam());
			}
		}

		void Client::GrenadeDroppedIntoWater(const Grenade& g) {
			SPADES_MARK_FUNCTION();

			if (!IsMuted()) {
				Handle<IAudioChunk> c =
				  audioDevice->RegisterSound("Sounds/Weapons/Grenade/DropWater.opus");
				audioDevice->Play(c.GetPointerOrNull(), g.GetPosition(), AudioParam());
			}
		}

		void Client::GrenadeExploded(const Grenade& g) {
			SPADES_MARK_FUNCTION();

			Vector3 origin = g.GetPosition();

			if (origin.z > 63.0F) {
				if (!IsMuted()) {
					Handle<IAudioChunk> c;

					c = audioDevice->RegisterSound("Sounds/Weapons/Grenade/WaterExplode.opus");
					AudioParam param;
					param.volume = 10.0F;
					audioDevice->Play(c.GetPointerOrNull(), origin, param);

					c = audioDevice->RegisterSound("Sounds/Weapons/Grenade/WaterExplodeFar.opus");
					param.volume = 6.0F;
					param.referenceDistance = 10.0F;
					audioDevice->Play(c.GetPointerOrNull(), origin, param);

					c = audioDevice->RegisterSound("Sounds/Weapons/Grenade/WaterExplodeStereo.opus");
					param.volume = 2.0F;
					audioDevice->Play(c.GetPointerOrNull(), origin, param);
				}

				GrenadeExplosionUnderwater(origin);
			} else {
				GrenadeExplosion(origin);

				if (!IsMuted()) {
					Handle<IAudioChunk> c, cs;

					switch (SampleRandomInt(0, 1)) {
						case 0:
							c = audioDevice->RegisterSound("Sounds/Weapons/Grenade/Explode1.opus");
							cs = audioDevice->RegisterSound(
							  "Sounds/Weapons/Grenade/ExplodeStereo1.opus");
							break;
						case 1:
							c = audioDevice->RegisterSound("Sounds/Weapons/Grenade/Explode2.opus");
							cs = audioDevice->RegisterSound(
							  "Sounds/Weapons/Grenade/ExplodeStereo2.opus");
							break;
					}

					AudioParam param;
					param.volume = 30.0F;
					param.referenceDistance = 5.0F;
					audioDevice->Play(c.GetPointerOrNull(), origin, param);

					param.referenceDistance = 1.0F;
					audioDevice->Play(cs.GetPointerOrNull(), origin, param);

					c = audioDevice->RegisterSound("Sounds/Weapons/Grenade/ExplodeFar.opus");
					param.volume = 6.0F;
					param.referenceDistance = 40.0F;
					audioDevice->Play(c.GetPointerOrNull(), origin, param);

					c = audioDevice->RegisterSound("Sounds/Weapons/Grenade/ExplodeFarStereo.opus");
					param.referenceDistance = 10.0F;
					audioDevice->Play(c.GetPointerOrNull(), origin, param);

					// debri sound
					c = audioDevice->RegisterSound("Sounds/Weapons/Grenade/Debris.opus");
					param.volume = 5.0F;
					param.referenceDistance = 3.0F;
					IntVector3 outPos;
					Vector3 soundPos = origin;
					if (world->GetMap()->CastRay(soundPos, MakeVector3(0, 0, 1), 8.0F, outPos))
						soundPos.z = (float)outPos.z - 0.2F;
					audioDevice->Play(c.GetPointerOrNull(), soundPos, param);
				}
			}
		}

		void Client::LocalPlayerPulledGrenadePin() {
			SPADES_MARK_FUNCTION();

			if (!IsMuted()) {
				Handle<IAudioChunk> c =
				  audioDevice->RegisterSound("Sounds/Weapons/Grenade/Fire.opus");
				audioDevice->PlayLocal(c.GetPointerOrNull(), MakeVector3(0.4F, -0.3F, 0.5F),
				                       AudioParam());
			}
		}

		void Client::LocalPlayerBlockAction(spades::IntVector3 v, BlockActionType type) {
			SPADES_MARK_FUNCTION();
			net->SendBlockAction(v, type);
		}
		void Client::LocalPlayerCreatedLineBlock(spades::IntVector3 v1, spades::IntVector3 v2) {
			SPADES_MARK_FUNCTION();
			net->SendBlockLine(v1, v2);
		}

		void Client::LocalPlayerHurt(HurtType type, bool sourceGiven, spades::Vector3 source) {
			SPADES_MARK_FUNCTION();

			if (sourceGiven) {
				stmp::optional<Player&> p = world->GetLocalPlayer();
				if (!p)
					return;
				Vector3 rel = source - p->GetEye();
				rel.z = 0.0F;
				rel = rel.Normalize();
				hurtRingView->Add(rel);
			}
		}

		void Client::LocalPlayerBuildError(BuildFailureReason reason) {
			SPADES_MARK_FUNCTION();

			switch (reason) {
				case BuildFailureReason::InsufficientBlocks:
					ShowAlert(_Tr("Client", "Insufficient blocks."), AlertType::Error);
					break;
				case BuildFailureReason::InvalidPosition:
					ShowAlert(_Tr("Client", "You cannot place a block there."), AlertType::Error);
					break;
			}
		}
	} // namespace client
} // namespace spades
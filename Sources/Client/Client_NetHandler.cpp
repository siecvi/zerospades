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

#include <cstdlib>

#include "Client.h"

#include <Core/Settings.h>
#include <Core/Strings.h>

#include "IAudioChunk.h"
#include "IAudioDevice.h"

#include "CTFGameMode.h"
#include "GameProperties.h"
#include "IGameMode.h"
#include "TCGameMode.h"

#include "CenterMessageView.h"
#include "ChatWindow.h"
#include "ClientUI.h"
#include "LimboView.h"

#include "GameMap.h"
#include "World.h"

#include "NetClient.h"

DEFINE_SPADES_SETTING(cg_clearCorpseOnRespawn, "0");
DEFINE_SPADES_SETTING(cg_centerMessage, "2");
SPADES_SETTING(cg_playerName);

namespace spades {
	namespace client {

#pragma mark - Server Packet Handlers

		void Client::LocalPlayerCreated() {
			renderer->UpdateFlatGameMap();

			hotBarIconState = 1.0F;

			// reset input
			playerInput.jump = PlayerInput().jump;
			weapInput = WeaponInput();

			// set loadout
			Player& p = world->GetLocalPlayer().value();
			limbo->SetSelectedTeam(p.GetTeamId());
			limbo->SetSelectedWeapon(p.GetWeaponType());
			inGameLimbo = false;
		}

		void Client::JoinedGame() {
			// Note: A localplayer doesn't exist yet

			// reset input
			playerInput = PlayerInput();
			weapInput = WeaponInput();

			// prepare the spectator view
			followCameraState.enabled = false;
			freeCameraState.position = MakeVector3(256, 256, 4);
			freeCameraState.velocity = MakeVector3(0, 0, 0);
			followAndFreeCameraState.yaw = DEG2RAD(90);
			followAndFreeCameraState.pitch = DEG2RAD(89);

			// welcome players
			centerMessageView->AddMessage(
			  _Tr("Client", "Welcome to the server, {0}!", cg_playerName.CString()));

			// play intro sound
			Handle<IAudioChunk> c = audioDevice->RegisterSound("Sounds/Feedback/Intro.opus");
			audioDevice->PlayLocal(c.GetPointerOrNull(), AudioParam());
		}

		void Client::TeamCapturedTerritory(int teamId, int terId) {
			auto& tr = dynamic_cast<TCGameMode&>(*world->GetMode()).GetTerritory(terId);
			int old = tr.ownerTeamId;

			std::string msg;
			std::string teamName = world->GetTeamName(teamId);
			std::string oldTeamName = world->GetTeamName(old);

			{
				if (old >= NEUTRAL_TEAM) {
					msg = _Tr("Client", "{0} captured an Neutral Territory", teamName);
				} else {
					msg = _Tr("Client", "{0} captured {1}'s Territory", teamName, oldTeamName);
				}
				NetLog("%s", msg.c_str());
				if (cg_centerMessage)
					centerMessageView->AddMessage(msg);
			}
			{
				teamName = chatWindow->TeamColorMessage(teamName, teamId);
				if (old >= NEUTRAL_TEAM) {
					msg = _Tr("Client", "{0} captured an neutral territory", teamName);
				} else {
					oldTeamName = chatWindow->TeamColorMessage(oldTeamName, old);
					msg = _Tr("Client", "{0} captured {1}'s territory", teamName, oldTeamName);
				}
				chatWindow->AddMessage(msg);
			}

			stmp::optional<Player&> localplayer = world->GetLocalPlayer();
			if (localplayer && !IsMuted()) {
				Handle<IAudioChunk> c = (teamId == localplayer->GetTeamId())
				    ? audioDevice->RegisterSound("Sounds/Feedback/TC/YourTeamCaptured.opus")
				    : audioDevice->RegisterSound("Sounds/Feedback/TC/EnemyCaptured.opus");
				audioDevice->PlayLocal(c.GetPointerOrNull(), AudioParam());
			}
		}

		void Client::PlayerCapturedIntel(Player& p) {
			std::string msg;

			int teamId = p.GetTeamId();
			int otherTeamId = 1 - teamId;
			std::string teamName = world->GetTeamName(otherTeamId);

			{
				msg = _Tr("Client", "{0} captured {1}'s Intel.", p.GetName(), teamName);
				NetLog("%s", msg.c_str());
				if (cg_centerMessage)
					centerMessageView->AddMessage(msg);
			}
			{
				msg = _Tr("Client", "{0} captured {1}'s intel",
				          chatWindow->TeamColorMessage(p.GetName(), teamId),
				          chatWindow->TeamColorMessage(teamName, otherTeamId));
				chatWindow->AddMessage(msg);
			}

			stmp::optional<Player&> localplayer = world->GetLocalPlayer();
			if (localplayer && !IsMuted()) {
				Handle<IAudioChunk> c = (teamId == localplayer->GetTeamId())
				    ? audioDevice->RegisterSound("Sounds/Feedback/CTF/YourTeamCaptured.opus")
				    : audioDevice->RegisterSound("Sounds/Feedback/CTF/EnemyCaptured.opus");
				audioDevice->PlayLocal(c.GetPointerOrNull(), AudioParam());
			}
		}

		void Client::PlayerPickedIntel(Player& p) {
			std::string msg;

			int teamId = p.GetTeamId();
			int otherTeamId = 1 - teamId;
			std::string teamName = world->GetTeamName(otherTeamId);

			{
				msg = _Tr("Client", "{0} picked up {1}'s Intel.", p.GetName(), teamName);
				NetLog("%s", msg.c_str());
				if (cg_centerMessage)
					centerMessageView->AddMessage(msg);
			}
			{
				msg = _Tr("Client", "{0} picked up {1}'s intel",
				          chatWindow->TeamColorMessage(p.GetName(), teamId),
				          chatWindow->TeamColorMessage(teamName, otherTeamId));
				chatWindow->AddMessage(msg);
			}

			if (!IsMuted()) {
				Handle<IAudioChunk> c =
				  audioDevice->RegisterSound("Sounds/Feedback/CTF/PickedUp.opus");
				audioDevice->PlayLocal(c.GetPointerOrNull(), AudioParam());
			}
		}

		void Client::PlayerDropIntel(Player& p) {
			std::string msg;

			int teamId = p.GetTeamId();
			int otherTeamId = 1 - teamId;
			std::string teamName = world->GetTeamName(otherTeamId);

			{
				msg = _Tr("Client", "{0} dropped {1}'s Intel", p.GetName(), teamName);
				NetLog("%s", msg.c_str());
				if (cg_centerMessage)
					centerMessageView->AddMessage(msg);
			}
			{
				msg = _Tr("Client", "{0} dropped {1}'s intel",
				          chatWindow->TeamColorMessage(p.GetName(), teamId),
				          chatWindow->TeamColorMessage(teamName, otherTeamId));
				chatWindow->AddMessage(msg);
			}
		}

		void Client::PlayerCreatedBlock(Player& p) {
			SPADES_MARK_FUNCTION();

			if (!IsMuted()) {
				Handle<IAudioChunk> c = audioDevice->RegisterSound("Sounds/Weapons/Block/Build.opus");
				audioDevice->Play(c.GetPointerOrNull(), p.GetEye() + p.GetFront(), AudioParam());
			}
		}

		void Client::PlayBlockDestroySound(spades::Vector3 pos) {
			SPADES_MARK_FUNCTION();

			if (!IsMuted()) {
				Handle<IAudioChunk> c = audioDevice->RegisterSound("Sounds/Misc/BlockDestroy.opus");
				audioDevice->Play(c.GetPointerOrNull(), pos, AudioParam());
			}
		}

		void Client::PlayerDestroyedBlockWithWeaponOrTool(spades::IntVector3 pos) {
			SPAssert(map);

			if (!map->IsSolid(pos.x, pos.y, pos.z))
				return;

			PlayBlockDestroySound(MakeVector3(pos) + 0.5F);
			EmitBlockDestroyFragments(pos);
		}

		void Client::PlayerDiggedBlock(spades::IntVector3 pos) {
			SPAssert(map);

			PlayBlockDestroySound(MakeVector3(pos) + 0.5F);

			for (int z = pos.z - 1; z <= pos.z + 1; z++) {
				if (z < 0 || z >= map->GroundDepth())
					continue;
				if (!map->IsSolid(pos.x, pos.y, z))
					continue;

				EmitBlockDestroyFragments(MakeIntVector3(pos.x, pos.y, z));
			}
		}

		void Client::GrenadeDestroyedBlock(spades::IntVector3 pos) {
			SPAssert(map);

			for (int x = pos.x - 1; x <= pos.x + 1; x++)
			for (int y = pos.y - 1; y <= pos.y + 1; y++)
			for (int z = pos.z - 1; z <= pos.z + 1; z++) {
				if (!map->IsValidMapCoord(x, y, z) || z >= map->GroundDepth())
					continue;
				if (!map->IsSolid(x, y, z))
					continue;

				EmitBlockDestroyFragments(MakeIntVector3(x, y, z));
			}
		}

		void Client::PlayerLeaving(Player& p) {
			// Choose the next player if a follow cam is active on this player
			if (FollowsNonLocalPlayer(GetCameraMode()) && &GetCameraTargetPlayer() == &p) {
				FollowNextPlayer(false);

				// Still unable to find a substitute?
				if (&GetCameraTargetPlayer() == &p)
					followCameraState.enabled = false; // Turn off the follow cam mode
			}

			std::string msg;
			{
				msg = _Tr("Client", "Player {0} has left", p.GetName());
				NetLog("%s", msg.c_str());
				scriptedUI->RecordChatLog(msg, ConvertColorRGBA(p.GetColor()));
			}
			{
				msg = chatWindow->TeamColorMessage(p.GetName(), p.GetTeamId());
				msg = _Tr("Client", "Player {0} has left", msg);
				chatWindow->AddMessage(msg);
			}

			RemoveCorpseForPlayer(p.GetId());
		}

		void Client::PlayerJoinedTeam(Player& p) {
			std::string msg;
			std::string teamName = p.IsSpectator()
				? _Tr("Client", "Spectator") : p.GetTeamName();

			{
				msg = _Tr("Client", "{0} joined {1} team", p.GetName(), teamName);
				NetLog("%s", msg.c_str());
				scriptedUI->RecordChatLog(msg, ConvertColorRGBA(p.GetColor()));
			}
			{
				msg = chatWindow->TeamColorMessage(teamName, p.GetTeamId());
				msg = _Tr("Client", "{0} joined {1} team", p.GetName(), msg);
				chatWindow->AddMessage(msg);
			}
		}

		void Client::PlayerSpawned(Player& p) {
			if (net->GetGameProperties()->isGameModeArena || cg_clearCorpseOnRespawn)
				RemoveCorpseForPlayer(p.GetId());
		}

		void Client::TeamWon(int teamId) {
			std::string msg;
			std::string teamName = world->GetTeamName(teamId);

			{
				msg = _Tr("Client", "{0} Wins!", teamName);
				NetLog("%s", msg.c_str());
				scriptedUI->RecordChatLog(msg);
				centerMessageView->AddMessage(msg);
			}
			{
				msg = chatWindow->TeamColorMessage(teamName, teamId);
				msg = _Tr("Client", "{0} wins!", msg);
				chatWindow->AddMessage(msg);
			}

			stmp::optional<Player&> localplayer = world->GetLocalPlayer();
			if (localplayer) {
				Handle<IAudioChunk> c = (teamId == localplayer->GetTeamId())
				                          ? audioDevice->RegisterSound("Sounds/Feedback/Win.opus")
				                          : audioDevice->RegisterSound("Sounds/Feedback/Lose.opus");
				audioDevice->PlayLocal(c.GetPointerOrNull(), AudioParam());
			}
		}

		void Client::MarkWorldUpdate() { upsCounter.MarkFrame(); }
	} // namespace client
} // namespace spades
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
#include "GameMap.h"
#include "GameProperties.h"
#include "IGameMode.h"
#include "TCGameMode.h"
#include "World.h"

#include "CenterMessageView.h"
#include "ChatWindow.h"
#include "ClientUI.h"

#include "NetClient.h"

DEFINE_SPADES_SETTING(cg_centerMessage, "2");
SPADES_SETTING(cg_playerName);
SPADES_SETTING(cg_scoreMessages);

namespace spades {
	namespace client {

#pragma mark - Server Packet Handlers

		void Client::LocalPlayerCreated() {
			freeCameraState.position = GetLastSceneDef().viewOrigin;
			weapInput = WeaponInput();
			playerInput.jump = PlayerInput().jump;
		}

		void Client::JoinedGame() {
			// Note: A localplayer doesn't exist yet

			// Welcome players
			auto msg = std::string(cg_playerName);
			msg = _Tr("Client", "Welcome to the server, {0}!", msg);
			centerMessageView->AddMessage(msg);

			// Play intro sound
			Handle<IAudioChunk> c = audioDevice->RegisterSound("Sounds/Feedback/Intro.opus");
			audioDevice->PlayLocal(c.GetPointerOrNull(), AudioParam());

			// Prepare the spectate mode
			followCameraState.enabled = false;
			freeCameraState.position = GetLastSceneDef().viewOrigin;
			freeCameraState.velocity = MakeVector3(0, 0, 0);
			followAndFreeCameraState.yaw = -DEG2RAD(90);
			followAndFreeCameraState.pitch = DEG2RAD(89);
		}

		void Client::PlayerCreatedBlock(Player& p) {
			SPADES_MARK_FUNCTION();

			if (!IsMuted()) {
				Handle<IAudioChunk> c = audioDevice->RegisterSound("Sounds/Weapons/Block/Build.opus");
				audioDevice->Play(c.GetPointerOrNull(), p.GetEye() + p.GetFront(), AudioParam());
			}
		}

		void Client::TeamCapturedTerritory(int teamId, int terId) {
			TCGameMode::Territory& tr =
			  dynamic_cast<TCGameMode&>(*world->GetMode()).GetTerritory(terId);

			std::string msg;
			auto teamName = chatWindow->TeamColorMessage(world->GetTeamName(teamId), teamId);

			int old = tr.ownerTeamId;
			if (old < 2) {
				auto otherTeam = chatWindow->TeamColorMessage(world->GetTeamName(old), old);
				msg = _Tr("Client", "{0} captured {1}'s territory", teamName, otherTeam);
			} else {
				msg = _Tr("Client", "{0} captured an neutral territory", teamName);
			}
			chatWindow->AddMessage(msg);

			if ((int)cg_centerMessage != 0) {
				teamName = world->GetTeamName(teamId);
				if (old < 2) {
					auto otherTeam = world->GetTeamName(old);
					msg = _Tr("Client", "{0} captured {1}'s Territory", teamName, otherTeam);
				} else {
					msg = _Tr("Client", "{0} captured an Neutral Territory", teamName);
				}
				NetLog("%s", msg.c_str());
				centerMessageView->AddMessage(msg);
			}

			if (world->GetLocalPlayer() && !IsMuted()) {
				Handle<IAudioChunk> c =
				  (teamId == world->GetLocalPlayer()->GetTeamId())
				    ? audioDevice->RegisterSound("Sounds/Feedback/TC/YourTeamCaptured.opus")
				    : audioDevice->RegisterSound("Sounds/Feedback/TC/EnemyCaptured.opus");
				audioDevice->PlayLocal(c.GetPointerOrNull(), AudioParam());
			}
		}

		void Client::PlayerCapturedIntel(Player& p) {
			std::string msg;

			int teamId = p.GetTeamId();
			int otherTeamId = 1 - teamId;
			auto teamName = world->GetTeamName(otherTeamId);

			if (p.IsLocalPlayer() && cg_scoreMessages) {
				std::string s;
				s += ChatWindow::ColoredMessage("+10", MsgColorSysInfo);
				s += " points for capturing the enemy flag";
				chatWindow->AddMessage(s);
			}

			{
				msg =
				  _Tr("Client", "{0} captured {1}'s intel",
				          chatWindow->TeamColorMessage(p.GetName(), teamId),
				          chatWindow->TeamColorMessage(teamName, otherTeamId));
				chatWindow->AddMessage(msg);
			}

			if ((int)cg_centerMessage != 0) {
				msg = _Tr("Client", "{0} captured {1}'s Intel.", p.GetName(), teamName);
				NetLog("%s", msg.c_str());
				centerMessageView->AddMessage(msg);
			}

			if (world->GetLocalPlayer() && !IsMuted()) {
				Handle<IAudioChunk> c =
				  (p.GetTeamId() == world->GetLocalPlayer()->GetTeamId())
				    ? audioDevice->RegisterSound("Sounds/Feedback/CTF/YourTeamCaptured.opus")
				    : audioDevice->RegisterSound("Sounds/Feedback/CTF/EnemyCaptured.opus");
				audioDevice->PlayLocal(c.GetPointerOrNull(), AudioParam());
			}
		}

		void Client::PlayerPickedIntel(Player& p) {
			std::string msg;

			int teamId = p.GetTeamId();
			int otherTeamId = 1 - teamId;
			auto teamName = world->GetTeamName(otherTeamId);

			{
				msg = _Tr("Client", "{0} picked up {1}'s intel",
				          chatWindow->TeamColorMessage(p.GetName(), teamId),
				          chatWindow->TeamColorMessage(teamName, otherTeamId));
				chatWindow->AddMessage(msg);
			}

			if ((int)cg_centerMessage != 0) {
				msg = _Tr("Client", "{0} picked up {1}'s Intel.", p.GetName(), teamName);
				NetLog("%s", msg.c_str());
				centerMessageView->AddMessage(msg);
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
			auto teamName = world->GetTeamName(otherTeamId);

			{
				msg = _Tr("Client", "{0} dropped {1}'s intel",
				          chatWindow->TeamColorMessage(p.GetName(), teamId),
				          chatWindow->TeamColorMessage(teamName, otherTeamId));
				chatWindow->AddMessage(msg);
			}

			if ((int)cg_centerMessage != 0) {
				msg = _Tr("Client", "{0} dropped {1}'s Intel", p.GetName(), teamName);
				NetLog("%s", msg.c_str());
				centerMessageView->AddMessage(msg);
			}
		}

		void Client::PlayerDestroyedBlockWithWeaponOrTool(spades::IntVector3 pos) {
			SPAssert(map);

			if (!map->IsSolid(pos.x, pos.y, pos.z))
				return;

			Handle<IAudioChunk> c = audioDevice->RegisterSound("Sounds/Misc/BlockDestroy.opus");
			if (!IsMuted())
				audioDevice->Play(c.GetPointerOrNull(), MakeVector3(pos) + 0.5F, AudioParam());

			uint32_t col = map->GetColor(pos.x, pos.y, pos.z);
			EmitBlockDestroyFragments(pos, IntVectorFromColor(col));
		}

		void Client::PlayerDiggedBlock(spades::IntVector3 pos) {
			Handle<IAudioChunk> c = audioDevice->RegisterSound("Sounds/Misc/BlockDestroy.opus");
			if (!IsMuted())
				audioDevice->Play(c.GetPointerOrNull(), MakeVector3(pos) + 0.5F, AudioParam());

			SPAssert(map);

			for (int z = pos.z - 1; z <= pos.z + 1; z++) {
				if (z < 0 || z > 61)
					continue;
				if (!map->IsSolid(pos.x, pos.y, z))
					continue;

				uint32_t col = map->GetColor(pos.x, pos.y, pos.z);
				EmitBlockDestroyFragments(MakeIntVector3(pos.x, pos.y, z), IntVectorFromColor(col));
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
				msg = chatWindow->TeamColorMessage(p.GetName(), p.GetTeamId());
				msg = _Tr("Client", "Player {0} has left", msg);
				chatWindow->AddMessage(msg);
			}
			{
				msg = _Tr("Client", "Player {0} has left", p.GetName());

				NetLog("%s", msg.c_str());
				scriptedUI->RecordChatLog(msg, MakeVector4(p.GetColor()) / 255.0F);
			}

			RemoveCorpseForPlayer(p.GetId());
		}

		void Client::PlayerJoinedTeam(Player& p) {
			std::string msg;
			auto teamName = p.IsSpectator() ? _Tr("Client", "Spectator") : p.GetTeamName();

			{
				msg = chatWindow->TeamColorMessage(teamName, p.GetTeamId());
				msg = _Tr("Client", "{0} joined {1} team", p.GetName(), msg);
				chatWindow->AddMessage(msg);
			}
			{
				msg = _Tr("Client", "{0} joined {1} team", p.GetName(), teamName);

				NetLog("%s", msg.c_str());
				scriptedUI->RecordChatLog(msg, MakeVector4(p.GetColor()) / 255.0F);
			}
		}

		void Client::PlayerSpawned(Player& p) {
			RemoveCorpseForPlayer(p.GetId());
		}

		void Client::GrenadeDestroyedBlock(spades::IntVector3 pos) {
			SPAssert(map);

			int range = 1;
			for (int x = pos.x - range; x <= pos.x + range; x++)
			for (int y = pos.y - range; y <= pos.y + range; y++)
			for (int z = pos.z - range; z <= pos.z + range; z++) {
				if (z < 0 || z > 61 || x < 0 || x >= map->Width() || y < 0 ||
					y >= map->Height())
					continue;
				if (!map->IsSolid(x, y, z))
					continue;

				uint32_t col = map->GetColor(pos.x, pos.y, pos.z);
				EmitBlockDestroyFragments(MakeIntVector3(x, y, z), IntVectorFromColor(col));
			}
		}

		void Client::TeamWon(int teamId) {
			std::string msg;
			auto teamName = world->GetTeamName(teamId);

			{
				msg = chatWindow->TeamColorMessage(teamName, teamId);
				msg = _Tr("Client", "{0} wins!", msg);
				chatWindow->AddMessage(msg);
			}
			{
				msg = _Tr("Client", "{0} Wins!", teamName);
				NetLog("%s", msg.c_str());
				scriptedUI->RecordChatLog(msg);
				centerMessageView->AddMessage(msg);
			}

			if (world->GetLocalPlayer()) {
				Handle<IAudioChunk> c = (teamId == world->GetLocalPlayer()->GetTeamId())
				                          ? audioDevice->RegisterSound("Sounds/Feedback/Win.opus")
				                          : audioDevice->RegisterSound("Sounds/Feedback/Lose.opus");
				audioDevice->PlayLocal(c.GetPointerOrNull(), AudioParam());
			}
		}

		void Client::MarkWorldUpdate() { upsCounter.MarkFrame(); }
	} // namespace client
} // namespace spades
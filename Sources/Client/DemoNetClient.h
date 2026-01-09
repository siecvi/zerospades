/*
 Copyright (c) 2013 yvt

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

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "DemoPlayer.h"
#include "NetClient.h"
#include "PhysicsConstants.h"
#include "Player.h"
#include <Core/Math.h>

namespace spades {
	namespace client {
		class Client;
		class GameMapLoader;
		struct GameProperties;

		/**
		 * DemoNetClient provides a NetClient-like interface that reads packets from
		 * a demo file instead of from the network. This allows the Client class to
		 * play back recorded demos with minimal changes.
		 */
		class DemoNetClient {
			Client* client;
			NetClientStatus status;
			std::unique_ptr<DemoPlayer> demoPlayer;
			std::unique_ptr<GameMapLoader> mapLoader;
			std::shared_ptr<GameProperties> properties;
			std::string statusString;
			int protocolVersion;
			float lastFrameTime;

			// Packet handling state (mirrors NetClient)
			std::vector<std::vector<char>> savedPackets;
			std::vector<Vector3> savedPlayerPos;
			std::vector<Vector3> savedPlayerFront;
			std::vector<int> savedPlayerTeam;
			IntVector3 temporaryPlayerBlockColor;

			// Debug tracking for map loading
			size_t expectedMapSize;
			size_t receivedMapBytes;

			// The player ID that was recorded as local player (for follow-cam)
			int recordedLocalPlayerId;

			stmp::optional<World&> GetWorld();
			Player& GetPlayer(int);
			stmp::optional<Player&> GetPlayerOrNull(int);
			Player& GetLocalPlayer();
			stmp::optional<Player&> GetLocalPlayerOrNull();

			void ProcessPacket(const std::vector<char>& data);
			void HandleGamePacket(class NetPacketReader& reader);
			void MapLoaded();

		public:
			DemoNetClient(Client* client);
			~DemoNetClient();

			/**
			 * Opens a demo file for playback.
			 * @param filename Path to the demo file
			 * @return true if successful
			 */
			bool OpenDemo(const std::string& filename);

			/**
			 * Process demo packets for the current frame.
			 * @param dt Delta time since last frame
			 */
			void DoEvents(float dt);

			NetClientStatus GetStatus() { return status; }
			std::string GetStatusString();
			float GetMapReceivingProgress();
			std::shared_ptr<GameProperties>& GetGameProperties() { return properties; }

			// Playback controls
			void Pause() { if (demoPlayer) demoPlayer->Pause(); }
			void Resume() { if (demoPlayer) demoPlayer->Resume(); }
			void TogglePause() { if (demoPlayer) demoPlayer->TogglePause(); }
			void SetSpeed(float speed) { if (demoPlayer) demoPlayer->SetSpeed(speed); }
			void Seek(float time);
			void SeekToBeginning();
			float GetTime() const { return demoPlayer ? demoPlayer->GetTime() : 0.0f; }
			float GetDuration() const { return demoPlayer ? demoPlayer->GetDuration() : 0.0f; }
			bool IsFinished() const { return demoPlayer ? demoPlayer->IsFinished() : true; }
			bool IsPaused() const { return demoPlayer ? demoPlayer->IsPaused() : false; }
			float GetSpeed() const { return demoPlayer ? demoPlayer->GetSpeed() : 1.0f; }
			int GetRecordedLocalPlayerId() const { return recordedLocalPlayerId; }

			// Stub methods (no-op in demo mode - we don't send anything to a server)
			void Disconnect() { status = NetClientStatusNotConnected; }
			int GetPing() { return 0; }
			float GetPacketLoss() { return 0.0f; }
			float GetPacketThrottle() { return 1.0f; }
			double GetDownlinkBps() { return 0.0; }
			double GetUplinkBps() { return 0.0; }

			// Send methods are all no-ops in demo mode
			void SendJoin(int, WeaponType, std::string, int) {}
			void SendPosition(Vector3) {}
			void SendOrientation(Vector3) {}
			void SendPlayerInput(PlayerInput) {}
			void SendWeaponInput(WeaponInput) {}
			void SendHit(int, HitType) {}
			void SendGrenade(const Grenade&) {}
			void SendTool() {}
			void SendHeldBlockColor() {}
			void SendBlockAction(IntVector3, BlockActionType) {}
			void SendBlockLine(IntVector3, IntVector3) {}
			void SendChat(std::string, bool) {}
			void SendReload() {}
			void SendTeamChange(int) {}
			void SendWeaponChange(WeaponType) {}

			// Demo recording is not applicable in playback mode
			bool StartDemoRecording(const std::string& = "") { return false; }
			void StopDemoRecording() {}
			bool IsDemoRecording() const { return false; }
			float GetDemoRecordingTime() const { return 0.0f; }
			uint64_t GetDemoPacketCount() const { return 0; }
			const std::string& GetDemoFilename() const {
				static std::string empty;
				return empty;
			}
		};
	} // namespace client
} // namespace spades

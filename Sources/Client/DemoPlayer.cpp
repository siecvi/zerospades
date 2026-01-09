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

#include "DemoPlayer.h"
#include <Core/Debug.h>

namespace spades {
	namespace client {

		DemoPlayer::DemoPlayer()
		    : isOpen(false),
		      paused(false),
		      finished(false),
		      protocolVersion(0),
		      playbackTime(0.0f),
		      duration(0.0f),
		      speed(1.0f),
		      currentPacketIndex(0) {}

		DemoPlayer::~DemoPlayer() {
			Close();
		}

		bool DemoPlayer::Open(const std::string& fname) {
			SPADES_MARK_FUNCTION();

			Close();

			file.open(fname, std::ios::binary | std::ios::in);
			if (!file.is_open()) {
				SPLog("Failed to open demo file: %s", fname.c_str());
				return false;
			}

			filename = fname;

			if (!ReadHeader()) {
				file.close();
				return false;
			}

			if (!PreloadPackets()) {
				file.close();
				return false;
			}

			isOpen = true;
			paused = false;
			finished = false;
			playbackTime = 0.0f;
			currentPacketIndex = 0;

			SPLog("Opened demo file: %s (protocol %d, %.1f seconds, %zu packets)",
			      filename.c_str(), protocolVersion, duration, packets.size());

			return true;
		}

		void DemoPlayer::Close() {
			SPADES_MARK_FUNCTION();

			if (file.is_open())
				file.close();

			isOpen = false;
			paused = false;
			finished = false;
			playbackTime = 0.0f;
			duration = 0.0f;
			currentPacketIndex = 0;
			packets.clear();
			filename.clear();
		}

		bool DemoPlayer::ReadHeader() {
			SPADES_MARK_FUNCTION();

			uint8_t header[2];
			file.read(reinterpret_cast<char*>(header), 2);

			if (!file.good() || file.gcount() != 2) {
				SPLog("Failed to read demo header");
				return false;
			}

			if (header[0] != FILE_VERSION) {
				SPLog("Unsupported demo file version: %d (expected %d)",
				      header[0], FILE_VERSION);
				return false;
			}

			protocolVersion = header[1];
			if (protocolVersion != 3 && protocolVersion != 4) {
				SPLog("Unsupported protocol version: %d", protocolVersion);
				return false;
			}

			return true;
		}

		bool DemoPlayer::PreloadPackets() {
			SPADES_MARK_FUNCTION();

			packets.clear();
			duration = 0.0f;

			while (file.good() && !file.eof()) {
				float timestamp;
				file.read(reinterpret_cast<char*>(&timestamp), sizeof(float));
				if (!file.good() || file.gcount() != sizeof(float))
					break;

				uint16_t length;
				file.read(reinterpret_cast<char*>(&length), sizeof(uint16_t));
				if (!file.good() || file.gcount() != sizeof(uint16_t))
					break;

				if (length == 0 || length > 65535) {
					SPLog("Invalid packet length: %u", length);
					break;
				}

				DemoPacket packet;
				packet.timestamp = timestamp;
				packet.data.resize(length);
				file.read(packet.data.data(), length);

				if (!file.good() || file.gcount() != length)
					break;

				packets.push_back(std::move(packet));
				duration = timestamp;
			}

			if (packets.empty()) {
				SPLog("No packets found in demo file");
				return false;
			}

			return true;
		}

		int DemoPlayer::Update(float dt, const PacketHandler& handler) {
			if (!isOpen || finished || paused)
				return 0;

			playbackTime += dt * speed;

			int dispatched = 0;
			while (currentPacketIndex < packets.size()) {
				const auto& packet = packets[currentPacketIndex];
				if (packet.timestamp > playbackTime)
					break;

				handler(packet.data);
				currentPacketIndex++;
				dispatched++;
			}

			if (currentPacketIndex >= packets.size())
				finished = true;

			return dispatched;
		}

		void DemoPlayer::Seek(float time) {
			if (!isOpen)
				return;

			playbackTime = std::max(0.0f, std::min(time, duration));
			finished = false;

			// Find the packet index for the new time
			currentPacketIndex = 0;
			for (size_t i = 0; i < packets.size(); i++) {
				if (packets[i].timestamp > playbackTime)
					break;
				currentPacketIndex = i;
			}
		}

		void DemoPlayer::FastForward(float seconds) {
			Seek(playbackTime + seconds);
		}

		void DemoPlayer::Pause() {
			paused = true;
		}

		void DemoPlayer::Resume() {
			paused = false;
		}

		void DemoPlayer::TogglePause() {
			paused = !paused;
		}

		void DemoPlayer::SetSpeed(float s) {
			speed = std::max(0.1f, std::min(s, 10.0f));
		}

		const std::vector<char>& DemoPlayer::GetPacket(size_t index) const {
			static std::vector<char> empty;
			if (index >= packets.size())
				return empty;
			return packets[index].data;
		}

		void DemoPlayer::Reset() {
			if (!isOpen)
				return;
			playbackTime = 0.0f;
			currentPacketIndex = 0;
			finished = false;
			paused = false;
		}

		const std::vector<char>& DemoPlayer::PeekNextPacket() const {
			static std::vector<char> empty;
			if (!isOpen || currentPacketIndex >= packets.size())
				return empty;
			return packets[currentPacketIndex].data;
		}

		void DemoPlayer::AdvancePacket() {
			if (!isOpen)
				return;
			if (currentPacketIndex < packets.size())
				currentPacketIndex++;
			if (currentPacketIndex >= packets.size())
				finished = true;
		}

	} // namespace client
} // namespace spades

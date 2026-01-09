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

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#define MKDIR(path) mkdir(path, 0755)
#endif

#include "DemoRecorder.h"
#include <Core/Debug.h>

namespace spades {
	namespace client {

		DemoRecorder::DemoRecorder()
		    : recording(false), packetCount(0), fileSize(0) {}

		DemoRecorder::~DemoRecorder() {
			if (recording)
				StopRecording();
		}

		bool DemoRecorder::StartRecording(const std::string& fname, int protocolVersion) {
			SPADES_MARK_FUNCTION();

			if (recording) {
				SPLog("Already recording, stopping previous recording");
				StopRecording();
			}

			filename = fname;

			file.open(filename, std::ios::binary | std::ios::out);
			if (!file.is_open()) {
				SPLog("Failed to open demo file for writing: %s", filename.c_str());
				return false;
			}

			// Write header: file version and protocol version
			uint8_t header[2];
			header[0] = FILE_VERSION;
			header[1] = static_cast<uint8_t>(protocolVersion);
			file.write(reinterpret_cast<const char*>(header), 2);

			if (!file.good()) {
				SPLog("Failed to write demo header");
				file.close();
				return false;
			}

			fileSize = 2;
			packetCount = 0;
			stopwatch.Reset();
			recording = true;

			SPLog("Started demo recording: %s (protocol version %d)", filename.c_str(), protocolVersion);
			return true;
		}

		void DemoRecorder::StopRecording() {
			SPADES_MARK_FUNCTION();

			if (!recording)
				return;

			file.flush();
			file.close();
			recording = false;

			SPLog("Stopped demo recording: %s (%llu packets, %llu bytes, %.1f seconds)",
			      filename.c_str(), (unsigned long long)packetCount,
			      (unsigned long long)fileSize, GetRecordingTime());
		}

		void DemoRecorder::RecordPacket(const char* data, size_t length) {
			SPADES_MARK_FUNCTION_DEBUG();

			if (!recording || length == 0 || length > 65535)
				return;

			// Write timestamp (4 bytes, float)
			float timestamp = static_cast<float>(stopwatch.GetTime());
			file.write(reinterpret_cast<const char*>(&timestamp), sizeof(float));

			// Write packet length (2 bytes, uint16)
			uint16_t len = static_cast<uint16_t>(length);
			file.write(reinterpret_cast<const char*>(&len), sizeof(uint16_t));

			// Write packet data
			file.write(data, length);

			if (file.good()) {
				packetCount++;
				fileSize += sizeof(float) + sizeof(uint16_t) + length;
			} else {
				SPLog("Failed to write packet to demo file");
			}
		}

		float DemoRecorder::GetRecordingTime() const {
			if (!recording)
				return 0.0f;
			return static_cast<float>(const_cast<Stopwatch&>(stopwatch).GetTime());
		}

		std::string DemoRecorder::GenerateFilename() {
			auto now = std::chrono::system_clock::now();
			auto time = std::chrono::system_clock::to_time_t(now);
			auto tm = std::localtime(&time);

			std::ostringstream oss;
			oss << "Demos/demo_"
			    << std::put_time(tm, "%Y%m%d_%H%M%S")
			    << ".dem";

			// Ensure the Demos directory exists
			MKDIR("Demos");

			return oss.str();
		}

	} // namespace client
} // namespace spades

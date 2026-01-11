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

#include <cstring>

#include "CTFGameMode.h"
#include "Client.h"
#include "DemoNetClient.h"
#include "GameMap.h"
#include "GameMapLoader.h"
#include "GameProperties.h"
#include "Grenade.h"
#include "Player.h"
#include "TCGameMode.h"
#include "Weapon.h"
#include "World.h"
#include <Core/CP437.h>
#include <Core/Debug.h>
#include <Core/Exception.h>
#include <Core/Settings.h>
#include <Core/Strings.h>
#include <Core/TMPUtils.h>

namespace spades {
	namespace client {

		namespace {
			enum { BLUE_FLAG = 0, GREEN_FLAG = 1, BLUE_BASE = 2, GREEN_BASE = 3 };
			enum PacketType {
				PacketTypePositionData = 0,
				PacketTypeOrientationData = 1,
				PacketTypeWorldUpdate = 2,
				PacketTypeInputData = 3,
				PacketTypeWeaponInput = 4,
				PacketTypeSetHP = 5,
				PacketTypeGrenadePacket = 6,
				PacketTypeSetTool = 7,
				PacketTypeSetColour = 8,
				PacketTypeExistingPlayer = 9,
				PacketTypeShortPlayerData = 10,
				PacketTypeMoveObject = 11,
				PacketTypeCreatePlayer = 12,
				PacketTypeBlockAction = 13,
				PacketTypeBlockLine = 14,
				PacketTypeStateData = 15,
				PacketTypeKillAction = 16,
				PacketTypeChatMessage = 17,
				PacketTypeMapStart = 18,
				PacketTypeMapChunk = 19,
				PacketTypePlayerLeft = 20,
				PacketTypeTerritoryCapture = 21,
				PacketTypeProgressBar = 22,
				PacketTypeIntelCapture = 23,
				PacketTypeIntelPickup = 24,
				PacketTypeIntelDrop = 25,
				PacketTypeRestock = 26,
				PacketTypeFogColour = 27,
				PacketTypeWeaponReload = 28,
				PacketTypeChangeTeam = 29,
				PacketTypeChangeWeapon = 30,
				PacketTypePlayerProperties = 64,
			};

			const char UTFSign = -1;

			std::string DecodeString(std::string s) {
				if (s.size() > 0 && s[0] == UTFSign)
					return s.substr(1);
				return CP437::Decode(s);
			}

			PlayerInput ParsePlayerInput(uint8_t bits) {
				PlayerInput inp;
				inp.moveForward = (bits & (1 << 0)) != 0;
				inp.moveBackward = (bits & (1 << 1)) != 0;
				inp.moveLeft = (bits & (1 << 2)) != 0;
				inp.moveRight = (bits & (1 << 3)) != 0;
				inp.jump = (bits & (1 << 4)) != 0;
				inp.crouch = (bits & (1 << 5)) != 0;
				inp.sneak = (bits & (1 << 6)) != 0;
				inp.sprint = (bits & (1 << 7)) != 0;
				return inp;
			}

			WeaponInput ParseWeaponInput(uint8_t bits) {
				WeaponInput inp;
				inp.primary = ((bits & (1 << 0)) != 0);
				inp.secondary = ((bits & (1 << 1)) != 0);
				return inp;
			}
		} // namespace

		// NetPacketReader for demo playback (simplified version of NetClient's reader)
		class NetPacketReader {
			std::vector<char> data;
			size_t pos;

		public:
			NetPacketReader(const std::vector<char>& inData) : data(inData), pos(1) {}

			unsigned int GetTypeRaw() { return static_cast<unsigned int>(static_cast<uint8_t>(data[0])); }
			PacketType GetType() { return static_cast<PacketType>(GetTypeRaw()); }

			uint32_t ReadInt() {
				if (pos + 4 > data.size())
					SPRaise("Received packet truncated");
				uint32_t value = 0;
				value |= ((uint32_t)(uint8_t)data[pos++]);
				value |= ((uint32_t)(uint8_t)data[pos++]) << 8;
				value |= ((uint32_t)(uint8_t)data[pos++]) << 16;
				value |= ((uint32_t)(uint8_t)data[pos++]) << 24;
				return value;
			}

			uint16_t ReadShort() {
				if (pos + 2 > data.size())
					SPRaise("Received packet truncated");
				uint32_t value = 0;
				value |= ((uint32_t)(uint8_t)data[pos++]);
				value |= ((uint32_t)(uint8_t)data[pos++]) << 8;
				return (uint16_t)value;
			}

			uint8_t ReadByte() {
				if (pos >= data.size())
					SPRaise("Received packet truncated");
				return (uint8_t)data[pos++];
			}

			float ReadFloat() {
				union { float f; uint32_t v; };
				v = ReadInt();
				return f;
			}

			IntVector3 ReadIntColor() {
				IntVector3 col;
				col.z = ReadByte();
				col.y = ReadByte();
				col.x = ReadByte();
				return col;
			}

			IntVector3 ReadIntVector3() {
				IntVector3 v;
				v.x = ReadInt();
				v.y = ReadInt();
				v.z = ReadInt();
				return v;
			}

			Vector3 ReadVector3() {
				Vector3 v;
				v.x = ReadFloat();
				v.y = ReadFloat();
				v.z = ReadFloat();
				return v;
			}

			std::size_t GetLength() { return data.size(); }
			std::size_t GetPosition() { return pos; }
			std::size_t GetNumRemainingBytes() { return data.size() - pos; }
			std::vector<char> GetData() { return data; }

			std::string ReadData(size_t siz) {
				if (pos + siz > data.size())
					SPRaise("Received packet truncated");
				std::string s = std::string(data.data() + pos, siz);
				pos += siz;
				return s;
			}

			std::string ReadRemainingData() {
				return std::string(data.data() + pos, data.size() - pos);
			}

			std::string ReadString(size_t siz) {
				return DecodeString(ReadData(siz).c_str());
			}

			std::string ReadRemainingString() {
				return DecodeString(ReadRemainingData().c_str());
			}

			void DumpDebug() {
				char buf[512];
				std::string str;
				int bytes = (int)data.size();
				sprintf(buf, "Demo Packet 0x%02x [len=%d]", (int)GetType(), bytes);
				str += buf;
				if (bytes > 64) bytes = 64;
				for (int i = 0; i < bytes; i++) {
					sprintf(buf, " %02x", (unsigned int)(unsigned char)data[i]);
					str += buf;
				}
				SPLog("%s", str.c_str());
			}
		};

		DemoNetClient::DemoNetClient(Client* c) : client(c), status(NetClientStatusNotConnected),
		    protocolVersion(0), lastFrameTime(0.0f), expectedMapSize(0), receivedMapBytes(0),
		    recordedLocalPlayerId(-1) {
			SPADES_MARK_FUNCTION();

			demoPlayer.reset(new DemoPlayer());

			const int slots = 256;
			savedPlayerPos.resize(slots);
			savedPlayerFront.resize(slots);
			savedPlayerTeam.resize(slots);
			std::fill(savedPlayerTeam.begin(), savedPlayerTeam.end(), -1);

			statusString = _Tr("NetClient", "Not connected");
		}

		DemoNetClient::~DemoNetClient() {
			SPADES_MARK_FUNCTION();

			if (demoPlayer)
				demoPlayer->Close();
		}

		bool DemoNetClient::OpenDemo(const std::string& filename) {
			SPADES_MARK_FUNCTION();

			if (!demoPlayer->Open(filename)) {
				statusString = _Tr("NetClient", "Failed to open demo file");
				return false;
			}

			protocolVersion = demoPlayer->GetProtocolVersion();

			// Create game properties based on protocol version
			ProtocolVersion protoVer = (protocolVersion == 4)
				? ProtocolVersion::v076 : ProtocolVersion::v075;
			properties.reset(new GameProperties(protoVer));

			status = NetClientStatusConnecting;
			statusString = _Tr("NetClient", "Loading demo");

			SPLog("Opened demo: %s (protocol %d)", filename.c_str(), protocolVersion);
			return true;
		}

		void DemoNetClient::DoEvents(float dt) {
			SPADES_MARK_FUNCTION();

			if (status == NetClientStatusNotConnected)
				return;

			lastFrameTime = dt;

			// Process packets from the demo
			demoPlayer->Update(dt, [this](const std::vector<char>& data) {
				ProcessPacket(data);
			});

			// Check if demo finished - auto-pause when complete
			if (demoPlayer->IsFinished() && status == NetClientStatusConnected && !demoPlayer->IsPaused()) {
				SPLog("Demo playback finished");
				statusString = _Tr("NetClient", "Demo finished - press P to replay");
				demoPlayer->Pause();
			}
		}

		void DemoNetClient::ProcessPacket(const std::vector<char>& data) {
			SPADES_MARK_FUNCTION();

			if (data.empty())
				return;

			NetPacketReader reader(data);

			try {
				if (status == NetClientStatusConnecting) {
					int type = reader.GetType();
					if (type == PacketTypeMapStart) {
						auto mapSize = reader.ReadInt();
						expectedMapSize = mapSize;
						receivedMapBytes = 0;
						SPLog("Demo map size: %lu", (unsigned long)mapSize);

						mapLoader.reset(new GameMapLoader());
						status = NetClientStatusReceivingMap;
						statusString = _Tr("NetClient", "Loading map from demo");
					}
				} else if (status == NetClientStatusReceivingMap) {
					SPAssert(mapLoader);

					int type = reader.GetType();
					if (type == PacketTypeMapChunk) {
						std::vector<char> dt = reader.GetData();
						size_t chunkSize = dt.size() - 1;
						receivedMapBytes += chunkSize;
						mapLoader->AddRawChunk(dt.data() + 1, chunkSize);
					} else if (type == PacketTypeStateData) {
						status = NetClientStatusConnected;
						statusString = _Tr("NetClient", "Playing demo");

						try {
							MapLoaded();
						} catch (const std::exception& ex) {
							SPLog("Map loading error: %s", ex.what());
							status = NetClientStatusNotConnected;
							statusString = _Tr("NetClient", "Error loading map");
							throw;
						}

						HandleGamePacket(reader);
					} else {
						// Save packet for later processing
						savedPackets.push_back(reader.GetData());
					}
				} else if (status == NetClientStatusConnected) {
					HandleGamePacket(reader);
				}
			} catch (const std::exception& ex) {
				int type = reader.GetType();
				reader.DumpDebug();
				SPLog("Exception handling demo packet 0x%02x: %s", type, ex.what());
			}
		}

		stmp::optional<World&> DemoNetClient::GetWorld() {
			return client->GetWorld();
		}

		stmp::optional<Player&> DemoNetClient::GetPlayerOrNull(int pId) {
			SPADES_MARK_FUNCTION();
			if (!GetWorld())
				return {};
			if (pId < 0 || pId >= static_cast<int>(GetWorld()->GetNumPlayerSlots()))
				return {};
			return GetWorld()->GetPlayer(pId);
		}

		Player& DemoNetClient::GetPlayer(int pId) {
			SPADES_MARK_FUNCTION();
			if (!GetWorld())
				SPRaise("Invalid player ID %d: no world", pId);
			if (pId < 0 || pId >= static_cast<int>(GetWorld()->GetNumPlayerSlots()))
				SPRaise("Invalid player ID %d: out of range", pId);
			if (!GetWorld()->GetPlayer(pId))
				SPRaise("Invalid player ID %d: doesn't exist", pId);
			return GetWorld()->GetPlayer(pId).value();
		}

		stmp::optional<Player&> DemoNetClient::GetLocalPlayerOrNull() {
			SPADES_MARK_FUNCTION();
			if (!GetWorld())
				return {};
			return GetWorld()->GetLocalPlayer();
		}

		Player& DemoNetClient::GetLocalPlayer() {
			SPADES_MARK_FUNCTION();
			stmp::optional<Player&> maybePlayer = GetLocalPlayerOrNull();
			if (!maybePlayer)
				SPRaise("Failed to get local player: doesn't exist");
			return maybePlayer.value();
		}

		void DemoNetClient::HandleGamePacket(NetPacketReader& r) {
			SPADES_MARK_FUNCTION();

			int packetType = r.GetType();

			switch (packetType) {
				case PacketTypePositionData: {
					auto p = GetLocalPlayerOrNull();
					if (!p) break;
					if (r.GetLength() != 13) break;
					p->RepositionPlayer(r.ReadVector3());
				} break;
				case PacketTypeOrientationData: {
					auto p = GetLocalPlayerOrNull();
					if (!p) break;
					Vector3 o = r.ReadVector3();
					if (o.GetSquaredLength() < 0.01F) break;
					o = o.Normalize();
					p->SetOrientation(o);
				} break;
				case PacketTypeWorldUpdate: {
					int bytesPerEntry = 24;
					if (protocolVersion == 4)
						bytesPerEntry++;

					client->MarkWorldUpdate();

					int entries = static_cast<int>(r.GetLength() / bytesPerEntry);
					for (int i = 0; i < entries; i++) {
						int idx = i;
						if (protocolVersion == 4) {
							idx = r.ReadByte();
							if (idx < 0 || idx >= properties->GetMaxNumPlayerSlots())
								SPRaise("Invalid player ID %d in WorldUpdate", idx);
						}

						Vector3 pos = r.ReadVector3();
						Vector3 front = r.ReadVector3();

						if (GetWorld()) {
							auto p = GetWorld()->GetPlayer(idx);
							auto localPlayer = GetLocalPlayerOrNull();
							if (p && (!localPlayer || &*p != &*localPlayer)
								&& p->IsAlive() && !p->IsSpectator()) {
								p->RepositionPlayer(pos);
								p->SetOrientation(front);
							}
						}

						savedPlayerPos.at(idx) = pos;
						savedPlayerFront.at(idx) = front;
					}
				} break;
				case PacketTypeInputData:
					if (!GetWorld()) break;
					{
						auto p = GetPlayerOrNull(r.ReadByte());
						PlayerInput inp = ParsePlayerInput(r.ReadByte());
						if (!p) break;
						// In demo mode there's no local player (spectating), so check before comparing
						auto localPlayer = GetLocalPlayerOrNull();
						if (localPlayer && &*p == &*localPlayer) {
							if (inp.jump) p->PlayerJump();
							break;
						}
						p->SetInput(inp);
					}
					break;
				case PacketTypeWeaponInput:
					if (!GetWorld()) break;
					{
						auto p = GetPlayerOrNull(r.ReadByte());
						WeaponInput inp = ParseWeaponInput(r.ReadByte());
						if (!p) break;
						// In demo mode there's no local player (spectating), so check before comparing
						auto localPlayer = GetLocalPlayerOrNull();
						if (localPlayer && &*p == &*localPlayer) break;
						p->SetWeaponInput(inp);
					}
					break;
				case PacketTypeSetHP: {
					auto p = GetLocalPlayerOrNull();
					if (!p) break;
					int hp = r.ReadByte();
					int type = r.ReadByte();
					Vector3 source = r.ReadVector3();
					p->SetHP(hp, type ? HurtTypeWeapon : HurtTypeFall, source);
				} break;
				case PacketTypeGrenadePacket:
					if (!GetWorld()) break;
					{
						int pId = r.ReadByte();
						float fuse = r.ReadFloat();
						Vector3 pos = r.ReadVector3();
						Vector3 vel = r.ReadVector3();
						Grenade* g = new Grenade(*GetWorld(), pos, vel, fuse);
						GetWorld()->AddGrenade(std::unique_ptr<Grenade>{g});
					}
					break;
				case PacketTypeSetTool: {
					auto p = GetPlayerOrNull(r.ReadByte());
					int tool = r.ReadByte();
					if (!p) break;
					switch (tool) {
						case 0: p->SetTool(Player::ToolSpade); break;
						case 1: p->SetTool(Player::ToolBlock); break;
						case 2: p->SetTool(Player::ToolWeapon); break;
						case 3: p->SetTool(Player::ToolGrenade); break;
						default: SPRaise("Invalid tool type: %d", tool);
					}
				} break;
				case PacketTypeSetColour: {
					stmp::optional<Player&> p = GetPlayerOrNull(r.ReadByte());
					IntVector3 color = r.ReadIntColor();
					if (p)
						p->SetHeldBlockColor(color);
					else
						temporaryPlayerBlockColor = color;
				} break;
				case PacketTypeExistingPlayer:
					if (!GetWorld()) break;
					{
						int pId = r.ReadByte();
						if (pId < 0 || pId >= properties->GetMaxNumPlayerSlots()) {
							SPLog("Ignoring invalid player ID %d in ExistingPlayer", pId);
							break;
						}

						int team = r.ReadByte();
						int weapon = r.ReadByte();
						int tool = r.ReadByte();
						int score = r.ReadInt();
						IntVector3 color = r.ReadIntColor();
						std::string name = TrimSpaces(r.ReadRemainingString());

						WeaponType wType;
						switch (weapon) {
							case 0: wType = RIFLE_WEAPON; break;
							case 1: wType = SMG_WEAPON; break;
							case 2: wType = SHOTGUN_WEAPON; break;
							default: SPRaise("Invalid weapon: %d", weapon);
						}

						auto p = stmp::make_unique<Player>(*GetWorld(), pId, wType, team);
						p->SetPosition(savedPlayerPos.at(pId));
						p->SetHeldBlockColor(color);

						switch (tool) {
							case 0: p->SetTool(Player::ToolSpade); break;
							case 1: p->SetTool(Player::ToolBlock); break;
							case 2: p->SetTool(Player::ToolWeapon); break;
							case 3: p->SetTool(Player::ToolGrenade); break;
							default: SPRaise("Invalid tool type: %d", tool);
						}

						GetWorld()->SetPlayer(pId, std::move(p));

						auto& pers = GetWorld()->GetPlayerPersistent(pId);
						pers.name = name;
						pers.score = score;

						savedPlayerTeam.at(pId) = team;
					}
					break;
				case PacketTypeMoveObject: {
					if (!GetWorld()) SPRaise("No world");

					int type = r.ReadByte();
					int state = r.ReadByte();
					Vector3 pos = r.ReadVector3();

					stmp::optional<IGameMode&> mode = GetWorld()->GetMode();
					if (mode && mode->ModeType() == IGameMode::m_CTF) {
						auto& ctf = dynamic_cast<CTFGameMode&>(mode.value());
						CTFGameMode::Team& team1 = ctf.GetTeam(0);
						CTFGameMode::Team& team2 = ctf.GetTeam(1);

						switch (type) {
							case BLUE_FLAG: team1.flagPos = pos; break;
							case BLUE_BASE: team1.basePos = pos; break;
							case GREEN_FLAG: team2.flagPos = pos; break;
							case GREEN_BASE: team2.basePos = pos; break;
						}
					} else if (mode && mode->ModeType() == IGameMode::m_TC) {
						auto& tc = dynamic_cast<TCGameMode&>(mode.value());
						if (type < tc.GetNumTerritories()) {
							TCGameMode::Territory& t = tc.GetTerritory(type);
							t.pos = pos;
							t.ownerTeamId = state;
						}
					}
				} break;
				case PacketTypeCreatePlayer: {
					if (!GetWorld()) SPRaise("No world");

					int pId = r.ReadByte();
					int weapon = r.ReadByte();
					int team = r.ReadByte();
					Vector3 pos = r.ReadVector3();
					std::string name = TrimSpaces(r.ReadRemainingString());

					if (pId < 0 || pId >= properties->GetMaxNumPlayerSlots()) {
						SPLog("Ignoring invalid player ID %d", pId);
						break;
					}

					WeaponType wType;
					switch (weapon) {
						case 0: wType = RIFLE_WEAPON; break;
						case 1: wType = SMG_WEAPON; break;
						case 2: wType = SHOTGUN_WEAPON; break;
						default: SPRaise("Invalid weapon: %d", weapon);
					}

					auto p = stmp::make_unique<Player>(*GetWorld(), pId, wType, team);
					pos.z -= 2.4F;
					p->SetPosition(pos);

					GetWorld()->SetPlayer(pId, std::move(p));

					if (!name.empty())
						GetWorld()->GetPlayerPersistent(pId).name = name;

					Player& pRef = GetWorld()->GetPlayer(pId).value();

					// In demo mode, user is always spectator - track all team changes
					if (savedPlayerTeam.at(pId) != team) {
						client->PlayerJoinedTeam(pRef);
						savedPlayerTeam.at(pId) = team;
					}

					client->PlayerSpawned(pRef);
				} break;
				case PacketTypeBlockAction: {
					stmp::optional<Player&> p = GetPlayerOrNull(r.ReadByte());
					int action = r.ReadByte();
					IntVector3 pos = r.ReadIntVector3();

					std::vector<IntVector3> cells;
					if (action == BlockActionCreate) {
						if (!p) {
							GetWorld()->CreateBlock(pos, temporaryPlayerBlockColor);
						} else {
							GetWorld()->CreateBlock(pos, p->GetBlockColor());
							if (!GetWorld()->GetMap()->IsSolidWrapped(pos.x, pos.y, pos.z)) {
								p->UseBlocks(1);
								if (p->IsLocalPlayer())
									client->RegisterPlacedBlocks(1);
							}
							client->PlayerCreatedBlock(*p);
						}
					} else if (action == BlockActionTool) {
						cells.push_back(pos);
						GetWorld()->DestroyBlock(cells);
						if (p && p->IsToolSpade())
							p->GotBlock();
						client->PlayerDestroyedBlockWithWeaponOrTool(pos);
					} else if (action == BlockActionDig) {
						for (int z = -1; z <= 1; z++)
							cells.push_back(MakeIntVector3(pos.x, pos.y, pos.z + z));
						GetWorld()->DestroyBlock(cells);
						client->PlayerDiggedBlock(pos);
					} else if (action == BlockActionGrenade) {
						for (int x = -1; x <= 1; x++)
						for (int y = -1; y <= 1; y++)
						for (int z = -1; z <= 1; z++)
							cells.push_back(MakeIntVector3(pos.x + x, pos.y + y, pos.z + z));
						GetWorld()->DestroyBlock(cells);
						client->GrenadeDestroyedBlock(pos);
					}
				} break;
				case PacketTypeBlockLine: {
					stmp::optional<Player&> p = GetPlayerOrNull(r.ReadByte());
					IntVector3 pos1 = r.ReadIntVector3();
					IntVector3 pos2 = r.ReadIntVector3();

					auto cells = GetWorld()->CubeLine(pos1, pos2, 50);
					for (const auto& c : cells) {
						if (!GetWorld()->GetMap()->IsSolid(c.x, c.y, c.z))
							GetWorld()->CreateBlock(c, p ? p->GetBlockColor() : temporaryPlayerBlockColor);
					}

					if (p) {
						int blocks = static_cast<int>(cells.size());
						p->UseBlocks(blocks);
						if (p->IsLocalPlayer())
							client->RegisterPlacedBlocks(blocks);
						client->PlayerCreatedBlock(*p);
					}
				} break;
				case PacketTypeStateData:
					if (!GetWorld()) break;
					{
						int pId = r.ReadByte();
						IntVector3 fogColor = r.ReadIntColor();

						IntVector3 teamColors[2];
						teamColors[0] = r.ReadIntColor();
						teamColors[1] = r.ReadIntColor();

						std::string teamNames[2];
						teamNames[0] = r.ReadString(10);
						teamNames[1] = r.ReadString(10);

						World::Team& t1 = GetWorld()->GetTeam(0);
						World::Team& t2 = GetWorld()->GetTeam(1);
						t1.color = teamColors[0];
						t2.color = teamColors[1];
						t1.name = teamNames[0];
						t2.name = teamNames[1];

						GetWorld()->SetFogColor(fogColor);
						// In demo mode, don't set local player - we're spectating
						// Store the recorded player ID for potential follow-cam
						recordedLocalPlayerId = pId;

						int mode = r.ReadByte();
						if (mode == CTFGameMode::m_CTF) {
							auto ctf = stmp::make_unique<CTFGameMode>();

							CTFGameMode::Team& team1 = ctf->GetTeam(0);
							CTFGameMode::Team& team2 = ctf->GetTeam(1);

							team1.score = r.ReadByte();
							team2.score = r.ReadByte();
							ctf->SetCaptureLimit(r.ReadByte());

							int intelFlags = r.ReadByte();
							team1.hasIntel = (intelFlags & 1) != 0;
							team2.hasIntel = (intelFlags & 2) != 0;

							if (team2.hasIntel) {
								team2.carrierId = r.ReadByte();
								r.ReadData(11);
							} else {
								team1.flagPos = r.ReadVector3();
							}

							if (team1.hasIntel) {
								team1.carrierId = r.ReadByte();
								r.ReadData(11);
							} else {
								team2.flagPos = r.ReadVector3();
							}

							team1.basePos = r.ReadVector3();
							team2.basePos = r.ReadVector3();

							GetWorld()->SetMode(std::move(ctf));
						} else {
							auto tc = stmp::make_unique<TCGameMode>(*GetWorld());

							int trNum = r.ReadByte();
							for (int i = 0; i < trNum; i++) {
								TCGameMode::Territory t{*tc};
								t.pos = r.ReadVector3();
								int state = r.ReadByte();
								t.ownerTeamId = state;
								t.progressBasePos = 0.0F;
								t.progressStartTime = 0.0F;
								t.progressRate = 0.0F;
								t.capturingTeamId = -1;
								tc->AddTerritory(t);
							}

							GetWorld()->SetMode(std::move(tc));
						}
						client->JoinedGame();
					}
					break;
				case PacketTypeKillAction: {
					int victimId = r.ReadByte();
					int killerId = r.ReadByte();
					int kt = r.ReadByte();
					int respawnTime = r.ReadByte();

					KillType type;
					switch (kt) {
						case 0: type = KillTypeWeapon; break;
						case 1: type = KillTypeHeadshot; break;
						case 2: type = KillTypeMelee; break;
						case 3: type = KillTypeGrenade; break;
						case 4: type = KillTypeFall; break;
						case 5: type = KillTypeTeamChange; break;
						case 6: type = KillTypeClassChange; break;
						default: SPInvalidEnum("kt", kt);
					}
					switch (type) {
						case KillTypeFall:
						case KillTypeTeamChange:
						case KillTypeClassChange: killerId = victimId; break;
						default: break;
					}

					auto victim = GetPlayerOrNull(victimId);
					auto killer = GetPlayerOrNull(killerId);
					if (!victim || !killer) {
						SPLog("Demo: KillAction skipped - player not found (victim=%d, killer=%d)", victimId, killerId);
						break;
					}
					victim->KilledBy(type, *killer, respawnTime);
					if (killerId != victimId)
						GetWorld()->GetPlayerPersistent(killerId).score++;
				} break;
				case PacketTypeChatMessage: {
					int playerId = r.ReadByte();
					int type = r.ReadByte();
					std::string msg = TrimSpaces(r.ReadRemainingString());

					if (type == 2) { // System
						client->ServerSentMessage(false, msg);
						properties->HandleServerMessage(msg);
					} else if (type == 0 || type == 1) { // All or Team
						stmp::optional<Player&> p = GetPlayerOrNull(playerId);
						if (p) {
							client->PlayerSentChatMessage(*p, (type == 0), msg);
						} else {
							client->ServerSentMessage((type == 1), msg);
						}
					}
				} break;
				case PacketTypePlayerLeft: {
					int pId = r.ReadByte();
					auto p = GetPlayerOrNull(pId);
					if (p)
						client->PlayerLeaving(*p);
					GetWorld()->GetPlayerPersistent(pId).score = 0;
					if (pId >= 0 && pId < (int)savedPlayerTeam.size())
						savedPlayerTeam[pId] = -1;
					GetWorld()->SetPlayer(pId, NULL);
				} break;
				case PacketTypeTerritoryCapture: {
					int territoryId = r.ReadByte();
					bool winning = r.ReadByte() != 0;
					int state = r.ReadByte();

					stmp::optional<IGameMode&> mode = GetWorld()->GetMode();
					if (!mode || mode->ModeType() != IGameMode::m_TC) break;

					auto& tc = dynamic_cast<TCGameMode&>(*mode);
					if (territoryId >= tc.GetNumTerritories()) break;

					client->TeamCapturedTerritory(state, territoryId);

					TCGameMode::Territory& t = tc.GetTerritory(territoryId);
					t.ownerTeamId = state;
					t.progressBasePos = 0.0F;
					t.progressRate = 0.0F;
					t.progressStartTime = 0.0F;
					t.capturingTeamId = -1;

					if (winning)
						client->TeamWon(state);
				} break;
				case PacketTypeProgressBar: {
					int territoryId = r.ReadByte();
					int capturingTeam = r.ReadByte();
					int rate = (int8_t)r.ReadByte();
					float progress = r.ReadFloat();

					stmp::optional<IGameMode&> mode = GetWorld()->GetMode();
					if (!mode || mode->ModeType() != IGameMode::m_TC) break;

					auto& tc = dynamic_cast<TCGameMode&>(*mode);
					if (territoryId >= tc.GetNumTerritories()) break;

					TCGameMode::Territory& t = tc.GetTerritory(territoryId);
					t.progressBasePos = progress;
					t.progressRate = (float)rate * TC_CAPTURE_RATE;
					t.progressStartTime = GetWorld()->GetTime();
					t.capturingTeamId = capturingTeam;
				} break;
				case PacketTypeIntelCapture: {
					if (!GetWorld()) break;

					stmp::optional<IGameMode&> mode = GetWorld()->GetMode();
					if (!mode || mode->ModeType() != IGameMode::m_CTF) break;

					int pId = r.ReadByte();
					auto p = GetPlayerOrNull(pId);
					if (!p) break;
					int teamId = p->GetTeamId();

					auto& ctf = dynamic_cast<CTFGameMode&>(mode.value());
					CTFGameMode::Team& team = ctf.GetTeam(teamId);
					team.score++;
					team.hasIntel = false;

					client->PlayerCapturedIntel(*p);
					GetWorld()->GetPlayerPersistent(pId).score += 10;

					bool winning = r.ReadByte() != 0;
					if (winning) {
						client->TeamWon(teamId);
						ctf.ResetIntelHoldingStatus();
					}
				} break;
				case PacketTypeIntelPickup: {
					stmp::optional<IGameMode&> mode = GetWorld()->GetMode();
					if (!mode || mode->ModeType() != IGameMode::m_CTF) break;

					int pId = r.ReadByte();
					auto p = GetPlayerOrNull(pId);
					if (!p) break;

					auto& ctf = dynamic_cast<CTFGameMode&>(mode.value());
					CTFGameMode::Team& team = ctf.GetTeam(p->GetTeamId());
					team.hasIntel = true;
					team.carrierId = pId;
					client->PlayerPickedIntel(*p);
				} break;
				case PacketTypeIntelDrop: {
					stmp::optional<IGameMode&> mode = GetWorld()->GetMode();
					if (!mode || mode->ModeType() != IGameMode::m_CTF) break;

					int pId = r.ReadByte();
					auto p = GetPlayerOrNull(pId);
					if (!p) break;
					int teamId = p->GetTeamId();

					auto& ctf = dynamic_cast<CTFGameMode&>(mode.value());
					ctf.GetTeam(teamId).hasIntel = false;
					ctf.GetTeam(1 - teamId).flagPos = r.ReadVector3();
					client->PlayerDropIntel(*p);
				} break;
				case PacketTypeRestock: {
					r.ReadByte();
					auto p = GetLocalPlayerOrNull();
					if (p) p->Restock();
				} break;
				case PacketTypeFogColour: {
					if (GetWorld()) {
						r.ReadByte();
						GetWorld()->SetFogColor(r.ReadIntColor());
					}
				} break;
				case PacketTypeWeaponReload: {
					auto p = GetPlayerOrNull(r.ReadByte());
					if (!p) break;
					auto localPlayer = GetLocalPlayerOrNull();
					if (&*p != localPlayer) {
						p->Reload();
					} else {
						int clip = r.ReadByte();
						int reserve = r.ReadByte();
						p->ReloadDone(clip, reserve);
					}
				} break;
				case PacketTypeChangeTeam:
				case PacketTypeChangeWeapon:
					// These packets are ignored in demo playback
					break;
				case PacketTypePlayerProperties: {
					int subId = r.ReadByte();
					int pId = r.ReadByte();
					int hp = r.ReadByte();
					int blocks = r.ReadByte();
					int grenades = r.ReadByte();
					int clip = r.ReadByte();
					int reserve = r.ReadByte();
					int score = r.ReadByte();

					auto p = GetPlayerOrNull(pId);
					if (!p) break;
					Weapon& w = p->GetWeapon();

					// In demo mode, use recorded local player ID for restock
					if (pId == recordedLocalPlayerId)
						p->Restock(hp, grenades, blocks);
					w.Restock(clip, reserve);
					GetWorld()->GetPlayerPersistent(pId).score = score;
				} break;
				default:
					SPLog("Demo: dropped unknown packet %d", (int)r.GetType());
					break;
			}
		}

		void DemoNetClient::MapLoaded() {
			SPADES_MARK_FUNCTION();

			SPAssert(mapLoader);

			std::unique_ptr<GameMapLoader> loader = std::move(mapLoader);

			SPLog("Waiting for demo map decoding... (received %zu bytes)", receivedMapBytes);
			loader->MarkEOF();
			loader->WaitComplete();
			GameMap* map = loader->TakeGameMap().Unmanage();
			SPLog("Demo map decoded successfully.");

			World* w = new World(properties);
			w->SetMap(map);
			map->Release();
			SPLog("Demo world initialized.");

			client->SetWorld(w);

			SPAssert(GetWorld());

			SPLog("Processing %d saved demo packets...", (int)savedPackets.size());

			std::fill(savedPlayerTeam.begin(), savedPlayerTeam.end(), -1);

			try {
				for (const auto& packets : savedPackets) {
					NetPacketReader r(packets);
					HandleGamePacket(r);
				}
				savedPackets.clear();
				SPLog("Demo packets processed.");
			} catch (...) {
				savedPackets.clear();
				throw;
			}
		}

		float DemoNetClient::GetMapReceivingProgress() {
			if (status != NetClientStatusReceivingMap || !mapLoader)
				return 0.0f;
			return mapLoader->GetProgress();
		}

		std::string DemoNetClient::GetStatusString() {
			if (status == NetClientStatusReceivingMap && mapLoader) {
				float progress = mapLoader->GetProgress();
				return Format("{0} ({1}%)", statusString, (int)(progress * 100.0f));
			}
			return statusString;
		}

		void DemoNetClient::Seek(float time) {
			if (!demoPlayer) return;

			// Adjust playback time - note that seeking backward may result in
			// inconsistent world state since packets won't be re-applied
			demoPlayer->Seek(time);
			statusString = _Tr("NetClient", "Playing demo");
		}

		void DemoNetClient::SeekToBeginning() {
			if (!demoPlayer) return;

			// Seek to beginning of timeline without reloading the world
			// Note: World state may be inconsistent, but allows video-player-like scrubbing
			demoPlayer->Seek(0.0f);
			demoPlayer->Resume();
			statusString = _Tr("NetClient", "Playing demo");
		}

	} // namespace client
} // namespace spades

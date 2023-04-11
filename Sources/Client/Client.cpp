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

#include <cstdarg>
#include <cstdlib>
#include <ctime>

#include "Client.h"
#include "Fonts.h"
#include <Core/FileManager.h>
#include <Core/IStream.h>
#include <Core/Settings.h>
#include <Core/Strings.h>

#include "IAudioChunk.h"
#include "IAudioDevice.h"

#include "CenterMessageView.h"
#include "ChatWindow.h"
#include "ClientPlayer.h"
#include "ClientUI.h"
#include "HurtRingView.h"
#include "LimboView.h"
#include "MapView.h"
#include "PaletteView.h"
#include "ScoreboardView.h"
#include "TCProgressView.h"

#include "BloodMarks.h"
#include "Corpse.h"
#include "SmokeSpriteEntity.h"

#include "GameMap.h"
#include "Weapon.h"
#include "World.h"

#include "NetClient.h"

DEFINE_SPADES_SETTING(cg_chatBeep, "1");
DEFINE_SPADES_SETTING(cg_alerts, "1");
DEFINE_SPADES_SETTING(cg_alertSounds, "1");
DEFINE_SPADES_SETTING(cg_serverAlert, "1");
DEFINE_SPADES_SETTING(cg_privateMessageAlert, "0");
DEFINE_SPADES_SETTING(cg_skipDeadPlayersWhenDead, "1");
DEFINE_SPADES_SETTING(cg_ignoreChatMessages, "0");
DEFINE_SPADES_SETTING(cg_smallFont, "0");

SPADES_SETTING(cg_playerName);

namespace spades {
	namespace client {

		Client::Client(Handle<IRenderer> r, Handle<IAudioDevice> audioDev,
		               const ServerAddress& host, Handle<FontManager> fontManager)
		    : playerName(cg_playerName.operator std::string().substr(0, 15)),
		      logStream(nullptr),
		      hostname(host),
		      renderer(r),
		      audioDevice(audioDev),

		      time(0.0F),
		      readyToClose(false),

		      worldSubFrame(0.0F),
		      frameToRendererInit(5),
		      timeSinceInit(0.0F),
		      hasLastTool(false),
		      lastPosSentTime(0.0F),
		      lastOriSentTime(0.0F),
		      lastAliveTime(0.0F),
		      lastHitTime(0.0F),
		      lastScore(0),
		      curKills(0),
		      curDeaths(0),
		      curStreak(0),
		      lastStreak(0),
		      bestStreak(0),
		      hasDelayedReload(false),
		      localFireVibrationTime(-1.0F),
		      grenadeVibration(0.0F),
		      grenadeVibrationSlow(0.0F),
		      scoreboardVisible(false),
		      flashlightOn(false),
		      hitFeedbackIconState(0.0F),
		      hitFeedbackFriendly(false),
		      debugHitTestZoomState(0.0F),
		      debugHitTestZoom(false),
		      focalLength(20.0F),
		      targetFocalLength(20.0F),
		      autoFocusEnabled(true),
		      inGameLimbo(false),
		      fontManager(fontManager),
		      alertDisappearTime(-10000.0F),
		      lastLocalCorpse(nullptr),
		      corpseSoftLimit(6),
		      corpseHardLimit(16),
		      nextScreenShotIndex(0),
		      nextMapShotIndex(0) {
			SPADES_MARK_FUNCTION();
			SPLog("Initializing...");

			renderer->SetFogColor(MakeVector3(0, 0, 0));
			renderer->SetFogDistance(128.0F);

			auto* chatFont = cg_smallFont ? &fontManager->GetSmallFont() : &fontManager->GetGuiFont();
			auto* centerFont = cg_smallFont ? &fontManager->GetMediumFont() : &fontManager->GetLargeFont();

			chatWindow = stmp::make_unique<ChatWindow>(this, &GetRenderer(), chatFont, false);
			killfeedWindow = stmp::make_unique<ChatWindow>(this, &GetRenderer(), chatFont, true);

			hurtRingView = stmp::make_unique<HurtRingView>(this);
			centerMessageView = stmp::make_unique<CenterMessageView>(this, centerFont);
			mapView = stmp::make_unique<MapView>(this, false);
			largeMapView = stmp::make_unique<MapView>(this, true);
			scoreboard = stmp::make_unique<ScoreboardView>(this);
			limbo = stmp::make_unique<LimboView>(this);
			paletteView = stmp::make_unique<PaletteView>(this);
			tcView = stmp::make_unique<TCProgressView>(*this);
			scriptedUI = Handle<ClientUI>::New(renderer.GetPointerOrNull(),
				audioDev.GetPointerOrNull(), fontManager.GetPointerOrNull(), this);

			bloodMarks = stmp::make_unique<BloodMarks>(*this);

			renderer->SetGameMap(nullptr);
		}

		void Client::SetWorld(spades::client::World* w) {
			SPADES_MARK_FUNCTION();

			if (world.get() == w)
				return;

			scriptedUI->CloseUI();

			RemoveAllCorpses();
			RemoveAllLocalEntities();

			lastHealth = 0;
			lastHurtTime = -100.0F;
			hurtRingView->ClearAll();

			scoreboardVisible = false;
			flashlightOn = false;
			debugHitTestZoom = false;

			clientPlayers.clear();

			if (world) {
				world->SetListener(nullptr);
				renderer->SetGameMap(nullptr);
				audioDevice->SetGameMap(nullptr);
				world = nullptr;
				map = nullptr;
			}
			world.reset(w);
			if (world) {
				SPLog("World set");

				// initialize player view objects
				clientPlayers.resize(world->GetNumPlayerSlots());
				for (size_t i = 0; i < world->GetNumPlayerSlots(); i++) {
					auto p = world->GetPlayer(static_cast<unsigned int>(i));
					if (p)
						clientPlayers[i] = Handle<ClientPlayer>::New(*p, *this);
					else
						clientPlayers[i] = nullptr;
				}

				world->SetListener(this);
				map = world->GetMap();
				renderer->SetGameMap(map);
				audioDevice->SetGameMap(map.GetPointerOrNull());
				NetLog("------ World Loaded ------");
			} else {
				SPLog("World removed");
				NetLog("------ World Unloaded ------");
			}

			limbo->SetSelectedTeam(2);
			limbo->SetSelectedWeapon(RIFLE_WEAPON);

			worldSubFrame = 0.0F;
			worldSetTime = time;
			inGameLimbo = false;
		}

		Client::~Client() {
			SPADES_MARK_FUNCTION();

			NetLog("Disconnecting");

			if (logStream) {
				SPLog("Closing netlog");
				logStream.reset();
			}

			if (net) {
				SPLog("Disconnecting");
				net->Disconnect();
				net.reset();
			}

			SPLog("Disconnected");

			RemoveAllLocalEntities();
			RemoveAllCorpses();

			renderer->SetGameMap(nullptr);
			audioDevice->SetGameMap(nullptr);

			clientPlayers.clear();

			scriptedUI->ClientDestroyed();
			tcView.reset();
			limbo.reset();
			scoreboard.reset();
			mapView.reset();
			largeMapView.reset();
			chatWindow.reset();
			killfeedWindow.reset();
			paletteView.reset();
			centerMessageView.reset();
			hurtRingView.reset();
			world.reset();
		}

		/** Initiate an initialization which likely to take some time */
		void Client::DoInit() {
			renderer->Init();
			SmokeSpriteEntity::Preload(renderer.GetPointerOrNull());

			renderer->RegisterImage("Gfx/Bullet/7.62mm.png");
			renderer->RegisterImage("Gfx/Bullet/9mm.png");
			renderer->RegisterImage("Gfx/Bullet/12gauge.png");
			renderer->RegisterImage("Gfx/Ball.png");
			renderer->RegisterImage("Gfx/HurtRing.png");
			renderer->RegisterImage("Gfx/HurtSprite.png");
			renderer->RegisterImage("Gfx/Spotlight.jpg");
			renderer->RegisterImage("Gfx/White.tga");
			renderer->RegisterImage("Textures/Fluid.png");
			renderer->RegisterImage("Textures/WaterExpl.png");
			audioDevice->RegisterSound("Sounds/Feedback/Chat.opus");
			audioDevice->RegisterSound("Sounds/Misc/SwitchMapZoom.opus");
			audioDevice->RegisterSound("Sounds/Misc/OpenMap.opus");
			audioDevice->RegisterSound("Sounds/Misc/CloseMap.opus");
			audioDevice->RegisterSound("Sounds/Player/Flashlight.opus");
			audioDevice->RegisterSound("Sounds/Player/Footstep1.opus");
			audioDevice->RegisterSound("Sounds/Player/Footstep2.opus");
			audioDevice->RegisterSound("Sounds/Player/Footstep3.opus");
			audioDevice->RegisterSound("Sounds/Player/Footstep4.opus");
			audioDevice->RegisterSound("Sounds/Player/Wade1.opus");
			audioDevice->RegisterSound("Sounds/Player/Wade2.opus");
			audioDevice->RegisterSound("Sounds/Player/Wade3.opus");
			audioDevice->RegisterSound("Sounds/Player/Wade4.opus");
			audioDevice->RegisterSound("Sounds/Player/Run1.opus");
			audioDevice->RegisterSound("Sounds/Player/Run2.opus");
			audioDevice->RegisterSound("Sounds/Player/Run3.opus");
			audioDevice->RegisterSound("Sounds/Player/Run4.opus");
			audioDevice->RegisterSound("Sounds/Player/Run5.opus");
			audioDevice->RegisterSound("Sounds/Player/Run6.opus");
			audioDevice->RegisterSound("Sounds/Player/Jump.opus");
			audioDevice->RegisterSound("Sounds/Player/Land.opus");
			audioDevice->RegisterSound("Sounds/Player/WaterJump.opus");
			audioDevice->RegisterSound("Sounds/Player/WaterLand.opus");
			audioDevice->RegisterSound("Sounds/Weapons/Switch.opus");
			audioDevice->RegisterSound("Sounds/Weapons/SwitchLocal.opus");
			audioDevice->RegisterSound("Sounds/Weapons/Restock.opus");
			audioDevice->RegisterSound("Sounds/Weapons/RestockLocal.opus");
			audioDevice->RegisterSound("Sounds/Weapons/AimDownSightLocal.opus");
			renderer->RegisterModel("Models/MapObjects/Intel.kv6");
			renderer->RegisterModel("Models/MapObjects/CheckPoint.kv6");
			renderer->RegisterModel("Models/MapObjects/BlockCursorLine.kv6");
			renderer->RegisterModel("Models/Player/Rifle/Dead.kv6");
			renderer->RegisterModel("Models/Player/Rifle/Arm.kv6");
			renderer->RegisterModel("Models/Player/Rifle/UpperArm.kv6");
			renderer->RegisterModel("Models/Player/Rifle/LegCrouch.kv6");
			renderer->RegisterModel("Models/Player/Rifle/TorsoCrouch.kv6");
			renderer->RegisterModel("Models/Player/Rifle/Leg.kv6");
			renderer->RegisterModel("Models/Player/Rifle/Torso.kv6");
			renderer->RegisterModel("Models/Player/Rifle/Arms.kv6");
			renderer->RegisterModel("Models/Player/Rifle/Head.kv6");
			renderer->RegisterModel("Models/Player/Shotgun/Dead.kv6");
			renderer->RegisterModel("Models/Player/Shotgun/Arm.kv6");
			renderer->RegisterModel("Models/Player/Shotgun/UpperArm.kv6");
			renderer->RegisterModel("Models/Player/Shotgun/LegCrouch.kv6");
			renderer->RegisterModel("Models/Player/Shotgun/TorsoCrouch.kv6");
			renderer->RegisterModel("Models/Player/Shotgun/Leg.kv6");
			renderer->RegisterModel("Models/Player/Shotgun/Torso.kv6");
			renderer->RegisterModel("Models/Player/Shotgun/Arms.kv6");
			renderer->RegisterModel("Models/Player/Shotgun/Head.kv6");
			renderer->RegisterModel("Models/Player/SMG/Dead.kv6");
			renderer->RegisterModel("Models/Player/SMG/Arm.kv6");
			renderer->RegisterModel("Models/Player/SMG/UpperArm.kv6");
			renderer->RegisterModel("Models/Player/SMG/LegCrouch.kv6");
			renderer->RegisterModel("Models/Player/SMG/TorsoCrouch.kv6");
			renderer->RegisterModel("Models/Player/SMG/Leg.kv6");
			renderer->RegisterModel("Models/Player/SMG/Torso.kv6");
			renderer->RegisterModel("Models/Player/SMG/Arms.kv6");
			renderer->RegisterModel("Models/Player/SMG/Head.kv6");
			renderer->RegisterModel("Models/Weapons/Spade/Spade.kv6");
			renderer->RegisterModel("Models/Weapons/Block/Block.kv6");
			renderer->RegisterModel("Models/Weapons/Grenade/Grenade.kv6");
			renderer->RegisterModel("Models/Weapons/Rifle/Weapon.kv6");
			renderer->RegisterModel("Models/Weapons/Rifle/WeaponNoMagazine.kv6");
			renderer->RegisterModel("Models/Weapons/Rifle/Magazine.kv6");
			renderer->RegisterModel("Models/Weapons/Shotgun/Weapon.kv6");
			renderer->RegisterModel("Models/Weapons/Shotgun/WeaponNoPump.kv6");
			renderer->RegisterModel("Models/Weapons/Shotgun/Pump.kv6");
			renderer->RegisterModel("Models/Weapons/SMG/Weapon.kv6");
			renderer->RegisterModel("Models/Weapons/SMG/WeaponNoMagazine.kv6");
			renderer->RegisterModel("Models/Weapons/SMG/Magazine.kv6");

			if (mumbleLink.Init())
				SPLog("Mumble linked");
			else
				SPLog("Mumble link failed");

			mumbleLink.SetContext(hostname.ToString(false));
			mumbleLink.SetIdentity(playerName);

			SPLog("Started connecting to '%s'", hostname.ToString().c_str());
			net = stmp::make_unique<NetClient>(this);
			net->Connect(hostname);

			// decide log file name
			std::string fn = hostname.ToString(false);
			std::string fn2;
			{
				time_t t;
				struct tm tm;
				::time(&t);
				tm = *localtime(&t);
				char buf[256];
				sprintf(buf, "%04d%02d%02d%02d%02d%02d_",
					tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
					tm.tm_hour, tm.tm_min, tm.tm_sec);
				fn2 = buf;
			}
			for (size_t i = 0; i < fn.size(); i++) {
				char c = fn[i];
				if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
					fn2 += c;
				else
					fn2 += '_';
			}
			fn2 = "NetLogs/" + fn2 + ".log";

			try {
				logStream = FileManager::OpenForWriting(fn2.c_str());
				SPLog("Netlog Started at '%s'", fn2.c_str());
			} catch (const std::exception& ex) {
				SPLog("Failed to open netlog file '%s' (%s)", fn2.c_str(), ex.what());
			}
		}

		void Client::RunFrame(float dt) {
			SPADES_MARK_FUNCTION();

			fpsCounter.MarkFrame();

			// waiting for renderer initialization
			if (frameToRendererInit > 0) {
				DrawStartupScreen();

				frameToRendererInit--;
				if (frameToRendererInit == 0) {
					DoInit();
				} else {
					return;
				}
			}

			timeSinceInit += std::min(dt, 0.03F);

			// update network
			try {
				if (net->GetStatus() == NetClientStatusConnected)
					net->DoEvents(0);
				else
					net->DoEvents(10);
			} catch (const std::exception& ex) {
				if (net->GetStatus() == NetClientStatusNotConnected) {
					SPLog("Disconnected because of error:\n%s", ex.what());
					NetLog("Disconnected because of error:\n%s", ex.what());
					throw;
				} else {
					SPLog("Exception while processing network packets (ignored):\n%s", ex.what());
				}
			}

			hurtRingView->Update(dt);
			centerMessageView->Update(dt);
			mapView->Update(dt);
			largeMapView->Update(dt);

			UpdateDamageIndicators(dt);
			UpdateAutoFocus(dt);

			if (world) {
				UpdateWorld(dt);
				mumbleLink.Update(world->GetLocalPlayer().get_pointer());
			} else {
				renderer->SetFogColor(MakeVector3(0, 0, 0));
			}

			chatWindow->Update(dt);
			killfeedWindow->Update(dt);
			limbo->Update(dt);

			// The loading screen
			if (net->GetStatus() == NetClientStatusReceivingMap) {
				// Apply temporal smoothing on the progress value
				float progress = net->GetMapReceivingProgress();

				if (mapReceivingProgressSmoothed > progress)
					mapReceivingProgressSmoothed = progress;
				else
					mapReceivingProgressSmoothed =
					  Mix(mapReceivingProgressSmoothed, progress, 1.0F - powf(0.05F, dt));
			} else {
				mapReceivingProgressSmoothed = 0.0F;
			}

			// CreateSceneDefinition also can be used for sounds
			SceneDefinition sceneDef = CreateSceneDefinition();
			lastSceneDef = sceneDef;
			UpdateMatrices();

			// Update sounds
			try {
				audioDevice->Respatialize(sceneDef.viewOrigin,
					sceneDef.viewAxis[2],  sceneDef.viewAxis[1]);
			} catch (const std::exception& ex) {
				SPLog("Audio subsystem returned error (ignored):\n%s", ex.what());
			}

			// render scene
			DrawScene();

			// draw 2d
			Draw2D();

			// draw scripted GUI
			scriptedUI->RunFrame(dt);
			if (scriptedUI->WantsClientToBeClosed())
				readyToClose = true;

			// reset all "delayed actions" (in case we forget to reset these)
			hasDelayedReload = false;

			time += dt;
		}

		void Client::RunFrameLate(float dt) {
			SPADES_MARK_FUNCTION();

			// Well done!
			renderer->FrameDone();
			renderer->Flip();
		}

		bool Client::IsLimboViewActive() {
			if (world) {
				if (!world->GetLocalPlayer())
					return true;
				else if (inGameLimbo)
					return true;
			}

			return false;
		}

		void Client::SpawnPressed() {
			WeaponType weap = limbo->GetSelectedWeapon();
			int team = limbo->GetSelectedTeam();
			if (team == 2)
				team = 255;

			if (!world->GetLocalPlayer() || world->GetLocalPlayer()->IsSpectator()) {
				// join
				if (team == 255) {
					// weaponId doesn't matter for spectators, but
					// NetClient doesn't like invalid weapon ID
					weap = WeaponType::RIFLE_WEAPON;
				}
				net->SendJoin(team, weap, playerName, lastScore);
			} else {
				Player& p = world->GetLocalPlayer().value();
				if (p.GetTeamId() != team)
					net->SendTeamChange(team);
				if (team != 255 && p.GetWeapon().GetWeaponType() != weap)
					net->SendWeaponChange(weap);
			}

			inGameLimbo = false;
		}

		void Client::ShowAlert(const std::string& contents, AlertType type) {
			if (!cg_alerts) {
				chatWindow->AddMessage(contents);
				if (type != AlertType::Notice)
				PlayAlertSound();
				return;
			}

			float timeout;
			switch (type) {
				case AlertType::Notice: timeout = 2.5F; break;
				case AlertType::Warning:
				case AlertType::Error: timeout = 3.0F; break;
				default: timeout = 0.0F; break;
			}
			ShowAlert(contents, type, timeout);
		}

		void Client::ShowAlert(const std::string& contents, AlertType type, float timeout, bool quiet) {
			alertType = type;
			alertContents = contents;
			alertDisappearTime = time + timeout;
			alertAppearTime = time;

			if (type != AlertType::Notice && !quiet)
				PlayAlertSound();
		}

		void Client::PlayAlertSound() {
			Handle<IAudioChunk> c = audioDevice->RegisterSound("Sounds/Feedback/Alert.opus");
			AudioParam params;
			params.volume = (float)cg_alertSounds;
			audioDevice->PlayLocal(c.GetPointerOrNull(), params);
		}

		/** Records chat message/game events to the log file. */
		void Client::NetLog(const char* format, ...) {
			char buf[4096];
			va_list va;
			va_start(va, format);
			vsnprintf(buf, sizeof(buf), format, va);
			va_end(va);
			std::string str = buf;

			time_t t;
			struct tm tm;
			::time(&t);
			tm = *localtime(&t);
			std::string timeStr = asctime(&tm);
			timeStr.resize(timeStr.size() - 1); // remove '\n'

			snprintf(buf, sizeof(buf), "%s %s\n", timeStr.c_str(), str.c_str());
			buf[sizeof(buf) - 1] = 0;

			std::string outStr = EscapeControlCharacters(buf);
			printf("%s", outStr.c_str());

			if (logStream)
				logStream->Write(outStr);
		}

#pragma mark - Snapshots

		void Client::TakeMapShot() {
			try {
				std::string name = MapShotPath();
				{
					auto stream = FileManager::OpenForWriting(name.c_str());
					try {
						const Handle<GameMap>& map = GetWorld()->GetMap();
						if (!map)
							SPRaise("No map loaded");
						map->Save(stream.get());
					} catch (...) {
						throw;
					}
				}

				std::string msg = _Tr("Client", "Map saved: {0}", name);
				ShowAlert(msg, AlertType::Notice);
				SPLog("Map saved: %s", name.c_str());
			} catch (const Exception& ex) {
				std::string msg = _Tr("Client", "Saving map failed: ");
				msg += ex.GetShortMessage();
				ShowAlert(msg, AlertType::Error);
				SPLog("Saving map failed: %s", ex.what());
			} catch (const std::exception& ex) {
				std::string msg = _Tr("Client", "Saving map failed: ");
				msg += ex.what();
				ShowAlert(msg, AlertType::Error);
				SPLog("Saving map failed: %s", ex.what());
			}
		}

		std::string Client::MapShotPath() {
			char buf[256];

			const int maxShotIndex = 10000;
			for (int i = 0; i < maxShotIndex; i++) {
				sprintf(buf, "Mapshots/shot%04d.vxl", nextScreenShotIndex);
				if (FileManager::FileExists(buf)) {
					nextScreenShotIndex++;
					if (nextScreenShotIndex >= maxShotIndex)
						nextScreenShotIndex = 0;
					continue;
				}

				return buf;
			}

			SPRaise("No free file name");
		}

#pragma mark - Chat Messages

		void Client::PlayerSentChatMessage(Player& p, bool global, const std::string& msg) {
			// filters player messages (but they can still be found in logs)
			bool ignoreMessages = cg_ignoreChatMessages && !p.IsLocalPlayer();

			if (!ignoreMessages) {
				std::string s;
				if (global)
					s = _Tr("Client", p.IsAlive() ? "[Global] " : "*DEAD* ");
				s += ChatWindow::TeamColorMessage(p.GetName(), p.GetTeamId());
				s += ": ";
				s += msg;

				chatWindow->AddMessage(s);
			}
			{
				std::string s;
				if (global)
					s = "[Global] ";
				s += p.GetName();
				s += ": ";
				s += msg;

				scriptedUI->RecordChatLog(s, ConvertColorRGBA(p.GetColor()));
			}

			std::string teamName = p.IsSpectator() ? "Spectator" : p.GetTeamName();
			NetLog("[%s] %s (%s): %s", global ? "Global" : "Team",
				p.GetName().c_str(), teamName.c_str(), msg.c_str());

			if (!IsMuted() && !ignoreMessages) {
				Handle<IAudioChunk> c = audioDevice->RegisterSound("Sounds/Feedback/Chat.opus");
				AudioParam params;
				params.volume = (float)cg_chatBeep;
				audioDevice->PlayLocal(c.GetPointerOrNull(), params);
			}
		}

		void Client::ServerSentMessage(bool system, const std::string& msg) {
			NetLog("%s", msg.c_str());
			scriptedUI->RecordChatLog(msg);

			if (cg_serverAlert) {
				if (msg.substr(0, 3) == "N% ") {
					ShowAlert(msg.substr(3), AlertType::Notice);
					return;
				}
				if (msg.substr(0, 3) == "!% ") {
					ShowAlert(msg.substr(3), AlertType::Error);
					return;
				}
				if (msg.substr(0, 3) == "%% ") {
					ShowAlert(msg.substr(3), AlertType::Warning);
					return;
				}
				if (msg.substr(0, 3) == "C% ") {
					centerMessageView->AddMessage(msg.substr(3));
					return;
				}
			}

			// Display system messages in yellow
			if (system) {
				chatWindow->AddMessage(ChatWindow::ColoredMessage(msg, MsgColorSysInfo));
				return;
			}

			// Display private messages in green
			if (msg.substr(0, 8) == "PM from ") {
				std::string s = "PM from " + msg.substr(8);

				// Display private messages as alerts
				if (cg_privateMessageAlert) {
					ShowAlert(s, AlertType::Notice);
					return;
				}

				chatWindow->AddMessage(ChatWindow::ColoredMessage(s, MsgColorGreen));
				return;
			}

			chatWindow->AddMessage(msg);
		}

#pragma mark - Follow / Spectate

		void Client::FollowNextPlayer(bool reverse) {
			SPAssert(world->GetLocalPlayer());

			auto& localPlayer = *world->GetLocalPlayer();
			int myTeam = localPlayer.GetTeamId();

			bool localPlayerIsSpectator = localPlayer.IsSpectator();

			int nextId = FollowsNonLocalPlayer(GetCameraMode())
			               ? followedPlayerId
			               : world->GetLocalPlayerIndex().value();
			do {
				reverse ? --nextId : ++nextId;

				if (nextId >= static_cast<int>(world->GetNumPlayerSlots()))
					nextId = 0;
				if (nextId < 0)
					nextId = static_cast<int>(world->GetNumPlayerSlots() - 1);

				stmp::optional<Player&> p = world->GetPlayer(nextId);
				if (!p || p->IsSpectator())
					continue; // Do not follow a non-existent player or spectator
				if (!localPlayerIsSpectator && p->GetTeamId() != myTeam)
					continue; // Skip enemies unless the local player is a spectator
				if (!localPlayerIsSpectator && !p->IsAlive() && cg_skipDeadPlayersWhenDead)
					continue; // Skip dead players unless the local player is not a spectator

				// Do not follow a player with an invalid state
				if (p->GetFront().GetSquaredLength() < 0.01F)
					continue;

				break;
			} while (nextId != followedPlayerId);

			followedPlayerId = nextId;
			followCameraState.enabled = (followedPlayerId != world->GetLocalPlayerIndex());
		}
	} // namespace client
} // namespace spades
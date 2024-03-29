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

#include "Client.h"

#include <Core/Bitmap.h>
#include <Core/ConcurrentDispatch.h>
#include <Core/FileManager.h>
#include <Core/Settings.h>
#include <Core/Strings.h>

#include "IAudioChunk.h"
#include "IAudioDevice.h"

#include "CenterMessageView.h"
#include "ChatWindow.h"
#include "ClientPlayer.h"
#include "ClientUI.h"
#include "Fonts.h"
#include "GameProperties.h"
#include "HitTestDebugger.h"
#include "HurtRingView.h"
#include "IFont.h"
#include "IGameMode.h"
#include "LimboView.h"
#include "MapView.h"
#include "PaletteView.h"
#include "ScoreboardView.h"
#include "TCProgressView.h"

#include "GameMap.h"
#include "Weapon.h"
#include "World.h"

#include "NetClient.h"

DEFINE_SPADES_SETTING(cg_hitIndicator, "1");
DEFINE_SPADES_SETTING(cg_debugAim, "0");
SPADES_SETTING(cg_keyReloadWeapon);
SPADES_SETTING(cg_keyJump);
SPADES_SETTING(cg_keyAttack);
SPADES_SETTING(cg_keyAltAttack);
SPADES_SETTING(cg_keyCrouch);
SPADES_SETTING(cg_keyLimbo);
DEFINE_SPADES_SETTING(cg_screenshotFormat, "jpeg");
DEFINE_SPADES_SETTING(cg_stats, "0");
DEFINE_SPADES_SETTING(cg_statsBackground, "1");
DEFINE_SPADES_SETTING(cg_playerStats, "0");
DEFINE_SPADES_SETTING(cg_playerStatsShowPlacedBlocks, "0");
DEFINE_SPADES_SETTING(cg_playerStatsHeight, "70");
DEFINE_SPADES_SETTING(cg_hideHud, "0");
DEFINE_SPADES_SETTING(cg_hudColor, "0");
DEFINE_SPADES_SETTING(cg_hudColorR, "255");
DEFINE_SPADES_SETTING(cg_hudColorG, "255");
DEFINE_SPADES_SETTING(cg_hudColorB, "255");
DEFINE_SPADES_SETTING(cg_hudAmmoStyle, "0");
DEFINE_SPADES_SETTING(cg_hudSafezoneX, "1");
DEFINE_SPADES_SETTING(cg_hudSafezoneY, "1");
DEFINE_SPADES_SETTING(cg_hudPlayerCount, "0");
DEFINE_SPADES_SETTING(cg_playerNames, "2");
DEFINE_SPADES_SETTING(cg_playerNameX, "0");
DEFINE_SPADES_SETTING(cg_playerNameY, "0");
DEFINE_SPADES_SETTING(cg_dbgHitTestSize, "128");
DEFINE_SPADES_SETTING(cg_damageIndicators, "1");
DEFINE_SPADES_SETTING(cg_hurtScreenEffects, "1");
DEFINE_SPADES_SETTING(cg_respawnSoundGain, "1");

SPADES_SETTING(cg_smallFont);
SPADES_SETTING(cg_minimapSize);

namespace spades {
	namespace client {

		enum class ScreenshotFormat { JPG, TGA, PNG };

		namespace {
			ScreenshotFormat GetScreenshotFormat(const std::string& format) {
				if (EqualsIgnoringCase(format, "jpeg")) {
					return ScreenshotFormat::JPG;
				} else if (EqualsIgnoringCase(format, "tga")) {
					return ScreenshotFormat::TGA;
				} else if (EqualsIgnoringCase(format, "png")) {
					return ScreenshotFormat::PNG;
				} else {
					const auto& defaultValue = cg_screenshotFormat.GetDescriptor().defaultValue;
					SPLog("Invalid screenshot format: \"%s\", resetting to \"%s\"",
						format.c_str(), defaultValue.c_str());
					cg_screenshotFormat = defaultValue;
					return GetScreenshotFormat(defaultValue);
				}
			}

			std::string TrKey(const std::string& name) {
				if (name == "LeftMouseButton")
					return "LMB";
				else if (name == "RightMouseButton")
					return "RMB";
				else if (name == "Control")
					return "CTRL";
				else if (name.empty())
					return _Tr("Client", "Unbound");
				else
					return ToUpperCase(name);
			}
		} // namespace

		void Client::TakeScreenShot(bool sceneOnly) {
			SceneDefinition sceneDef = CreateSceneDefinition();
			lastSceneDef = sceneDef;
			UpdateMatrices();

			// render scene
			flashDlights = flashDlightsOld;
			DrawScene();

			// draw 2d
			if (!sceneOnly)
				Draw2D();

			// Well done!
			renderer->FrameDone();

			Handle<Bitmap> bmp = renderer->ReadBitmap();

			try {
				auto name = ScreenShotPath();
				bmp->Save(name);

				std::string msg = sceneOnly
					? _Tr("Client", "Sceneshot saved: {0}", name)
					: _Tr("Client", "Screenshot saved: {0}", name);
				ShowAlert(msg, AlertType::Notice);

				Handle<IAudioChunk> c = audioDevice->RegisterSound("Sounds/Feedback/Screenshot.opus");
				audioDevice->PlayLocal(c.GetPointerOrNull(), AudioParam());
			} catch (const Exception& ex) {
				auto msg = _Tr("Client", "Screenshot failed: ");
				msg += ex.GetShortMessage();
				ShowAlert(msg, AlertType::Error);
				SPLog("Screenshot failed: %s", ex.what());
			} catch (const std::exception& ex) {
				auto msg = _Tr("Client", "Screenshot failed: ");
				msg += ex.what();
				ShowAlert(msg, AlertType::Error);
				SPLog("Screenshot failed: %s", ex.what());
			}
		}

		std::string Client::ScreenShotPath() {
			char bufJpg[256], bufTga[256], bufPng[256];

			const int maxShotIndex = 10000;
			for (int i = 0; i < maxShotIndex; i++) {
				sprintf(bufJpg, "Screenshots/shot%04d.jpg", nextScreenShotIndex);
				sprintf(bufTga, "Screenshots/shot%04d.tga", nextScreenShotIndex);
				sprintf(bufPng, "Screenshots/shot%04d.png", nextScreenShotIndex);
				if (FileManager::FileExists(bufJpg) ||
					FileManager::FileExists(bufTga) ||
				    FileManager::FileExists(bufPng)) {
					nextScreenShotIndex++;
					if (nextScreenShotIndex >= maxShotIndex)
						nextScreenShotIndex = 0;
					continue;
				}

				switch (GetScreenshotFormat(cg_screenshotFormat)) {
					case ScreenshotFormat::JPG: return bufJpg;
					case ScreenshotFormat::TGA: return bufTga;
					case ScreenshotFormat::PNG: return bufPng;
				}
				SPAssert(false);
			}

			SPRaise("No free file name");
		}

#pragma mark - HUD Drawings

		void Client::DrawSplash() {
			float sw = renderer->ScreenWidth();
			float sh = renderer->ScreenHeight();

			renderer->SetColorAlphaPremultiplied(MakeVector4(0, 0, 0, 1));
			renderer->DrawImage(nullptr, AABB2(0, 0, sw, sh));

			Handle<IImage> img = renderer->RegisterImage("Gfx/Title/Logo.png");
			float scale = fabsf(sinf(time));
			Vector2 size = {img->GetWidth(), img->GetHeight()};
			size *= std::min(1.0F, sw / size.x);
			size *= std::min(1.0F, sh / size.y);
			size *= 1.0F - (scale * (scale * 0.25F));

			Vector2 pos = (MakeVector2(sw, sh) - size) * 0.5F;

			renderer->SetColorAlphaPremultiplied(MakeVector4(1, 1, 1, 1));
			renderer->DrawImage(img, AABB2(pos.x, pos.y, size.x, size.y));
		}

		void Client::DrawStartupScreen() {
			float sw = renderer->ScreenWidth();
			float sh = renderer->ScreenHeight();

			DrawSplash();

			IFont& font = fontManager->GetGuiFont();
			std::string str = _Tr("Client", "NOW LOADING");
			Vector2 size = font.Measure(str);
			Vector2 pos = (MakeVector2(sw, sh) - 16.0F) - size;
			font.DrawShadow(str, pos, 1.0F, MakeVector4(1, 1, 1, 1), MakeVector4(0, 0, 0, 0.5));

			renderer->FrameDone();
			renderer->Flip();
		}

		void Client::DrawPlayingTime() {
			float sw = renderer->ScreenWidth();
			float sh = renderer->ScreenHeight();

			float spacing = cg_smallFont ? 40.0F : 50.0F;
			float y = ((int)cg_stats >= 2) ? spacing : 30.0F;

			int now = (int)time;
			int mins = now / 60;
			int secs = now - mins * 60;
			char buf[64];
			sprintf(buf, "%d:%.2d", mins, secs);
			IFont& font = fontManager->GetHeadingFont();
			Vector2 size = font.Measure(buf);
			Vector2 pos = MakeVector2((sw - size.x) * 0.5F, y - size.y);
			font.DrawShadow(buf, pos, 1.0F, MakeVector4(1, 1, 1, 1), MakeVector4(0, 0, 0, 0.5));
		}

		void Client::DrawAlivePlayersCount() {
			if (world->GetNumPlayers() <= 1)
				return;

			float sw = renderer->ScreenWidth();
			float sh = renderer->ScreenHeight();

			float teamBarWidth = 30.0F;
			float teamBarHeight = 30.0F;

			float spacing = cg_smallFont ? 40.0F : 50.0F;

			float y = 8.0F;
			if (((int)cg_hudPlayerCount >= 2 && (int)cg_stats < 2 && cg_stats) ||
			    ((int)cg_hudPlayerCount < 2 && (int)cg_stats >= 2 && (int)cg_stats < 3))
				y = cg_smallFont ? 20.0F : 30.0F;
			if ((int)cg_hudPlayerCount < 2 && scoreboardVisible)
				y = ((int)cg_stats >= 2) ? spacing : 30.0F;

			float teamBarTop = ((int)cg_hudPlayerCount < 2)
				? y : ((sh - y) - teamBarHeight);

			IFont& font = fontManager->GetMediumFont();
			Vector2 pos, size;
			std::string str;

			// draw team bar
			renderer->SetColorAlphaPremultiplied(AdjustColor(ConvertColorRGBA(world->GetTeamColor(0)), 1, 0.2F));
			renderer->DrawImage(nullptr, AABB2(sw * 0.5F, teamBarTop, -teamBarWidth, teamBarHeight));
			renderer->SetColorAlphaPremultiplied(AdjustColor(ConvertColorRGBA(world->GetTeamColor(1)), 1, 0.2F));
			renderer->DrawImage(nullptr, AABB2((sw * 0.5F) + teamBarWidth, teamBarTop, -teamBarWidth, teamBarHeight));

			// draw outline
			renderer->SetColorAlphaPremultiplied(MakeVector4(0, 0, 0, 0.2F));
			renderer->DrawOutlinedRect((sw * 0.5F) - teamBarWidth, teamBarTop,
			                           (sw * 0.5F) + teamBarWidth, teamBarTop + teamBarHeight);

			// draw player count
			str = ToString(world->GetNumPlayersAlive(0));
			size = font.Measure(str);
			pos.x = ((sw - teamBarWidth) * 0.5F) - size.x * 0.5F;
			pos.y = teamBarTop;
			font.Draw(str, pos + MakeVector2(0, 2), 1.0F, MakeVector4(0, 0, 0, 0.5));
			font.Draw(str, pos, 1.0F, MakeVector4(1, 1, 1, 1));

			str = ToString(world->GetNumPlayersAlive(1));
			size = font.Measure(str);
			pos.x = ((sw + teamBarWidth) * 0.5F) - size.x * 0.5F;
			pos.y = teamBarTop;
			font.Draw(str, pos + MakeVector2(0, 2), 1.0F, MakeVector4(0, 0, 0, 0.5));
			font.Draw(str, pos, 1.0F, MakeVector4(1, 1, 1, 1));
		}

		void Client::DrawHurtSprites() {
			float per = (time - lastHurtTime) / 1.5F;
			if (per < 0.0F || per > 1.0F)
				return;

			Handle<IImage> img = renderer->RegisterImage("Gfx/HurtSprite.png");
			Vector2 size = {img->GetWidth(), img->GetHeight()};

			Vector2 scrSize = {renderer->ScreenWidth(), renderer->ScreenHeight()};
			Vector2 scrCenter = scrSize * 0.5F;

			float radius = scrSize.GetLength() * 0.5F;

			for (const auto& spr : hurtSprites) {
				float alpha = spr.strength - per;
				if (alpha < 0.0F)
					continue;
				if (alpha > 1.0F)
					alpha = 1.0F;

				float c = cosf(spr.angle);
				float s = sinf(spr.angle);

				Vector2 radDir = {c, s};
				Vector2 angDir = {-s, c};
				float siz = spr.scale * radius;
				Vector2 base = radDir * radius + scrCenter;
				Vector2 centVect = radDir * (-siz);
				Vector2 sideVect1 = angDir * (siz * 4.0F * (spr.horzShift));
				Vector2 sideVect2 = angDir * (siz * 4.0F * (spr.horzShift - 1.0F));

				Vector2 v1 = base + centVect + sideVect1;
				Vector2 v2 = base + centVect + sideVect2;
				Vector2 v3 = base + sideVect1;

				renderer->SetColorAlphaPremultiplied(MakeVector4(0, 0, 0, alpha));
				renderer->DrawImage(img, v1, v2, v3, AABB2(0, 8, size.x, size.y));
			}
		}

		void Client::DrawHurtScreenEffect() {
			SPADES_MARK_FUNCTION();

			float sw = renderer->ScreenWidth();
			float sh = renderer->ScreenHeight();

			Player& p = GetWorld()->GetLocalPlayer().value();

			float hpper = p.GetHealth() / 100.0F;

			const float fadeOutTime = 0.35F;
			float timeSinceLastHurt = time - lastHurtTime;

			if (time >= lastHurtTime && timeSinceLastHurt < fadeOutTime) {
				float per = timeSinceLastHurt / fadeOutTime;
				per = 1.0F - per;
				per *= 0.3F + (1.0F - hpper) * 0.7F;
				per = std::min(per, 0.9F);
				per = 1.0F - per;
				renderer->MultiplyScreenColor({1, per, per});

				per = (1.0F - per) * 0.1F;
				renderer->SetColorAlphaPremultiplied({per, 0, 0, per});
				renderer->DrawImage(nullptr, AABB2(0, 0, sw, sh));
			}
		}

		Vector4 Client::GetPlayerColor(Player& player) {
			Vector4 playerColor = MakeVector4(1, 1, 1, 1);

			Vector3 eye = lastSceneDef.viewOrigin;
			Vector3 dir = player.GetEye() - eye;

			float dist = dir.GetSquaredLength2D();
			dir = dir.Normalize();

			// do map raycast
			GameMap::RayCastResult mapResult;
			mapResult = map->CastRay2(eye, dir, 256);

			if (dist > FOG_DISTANCE_SQ) {
				playerColor = MakeVector4(1, 0.75, 0, 1);
			} else if (mapResult.hit && (mapResult.hitPos - eye).GetSquaredLength2D() < dist) {
				playerColor = ConvertColorRGBA(player.GetColor());
			}

			return playerColor;
		}

		void Client::DrawPlayerName(Player& player, Vector4 color) {
			SPADES_MARK_FUNCTION();

			Vector3 origin = player.GetEye();
			origin.z -= 0.45F; // above player head

			Vector2 scrPos;
			if (Project(origin, scrPos)) {
				scrPos.x += (int)cg_playerNameX;
				scrPos.y += (int)cg_playerNameY;

				char buf[64];
				auto nameStr = player.GetName();
				sprintf(buf, "%s", nameStr.c_str());
				if (cg_playerNames == 1) {
					Vector3 diff = origin - lastSceneDef.viewOrigin;
					float dist = diff.GetLength2D();
					if (dist <= FOG_DISTANCE)
						sprintf(buf, "%s [%.1f]", nameStr.c_str(), dist);
				}

				IFont& font = cg_smallFont
					? fontManager->GetSmallFont()
					: fontManager->GetGuiFont();

				Vector2 size = font.Measure(buf);
				scrPos.x -= size.x * 0.5F;
				scrPos.y -= size.y;

				float luminosity = color.x + color.y + color.z;
				Vector4 shadowColor = (luminosity > 0.9F)
					? MakeVector4(0, 0, 0, 0.8F)
					: MakeVector4(1, 1, 1, 0.8F);

				font.DrawShadow(buf, scrPos, 1.0F, color, shadowColor);
			}
		}

		void Client::DrawHottrackedPlayerName() {
			SPADES_MARK_FUNCTION();

			auto hottracked = HotTrackedPlayer();
			if (hottracked) {
				Player& player = std::get<0>(*hottracked);
				DrawPlayerName(player, MakeVector4(1, 1, 1, 1));
			}
		}

		void Client::DrawPubOVL() {
			SPADES_MARK_FUNCTION();

			Player& player = GetCameraTargetPlayer();

			for (size_t i = 0; i < world->GetNumPlayerSlots(); i++) {
				auto maybePlayer = world->GetPlayer(static_cast<unsigned int>(i));
				if (maybePlayer == player || !maybePlayer)
					continue;

				Player& p = maybePlayer.value();
				if (p.IsSpectator() || !p.IsAlive())
					continue;

				// Do not draw a player with an invalid state
				if (p.GetFront().GetSquaredLength() < 0.01F)
					continue;

				DrawPlayerName(p, GetPlayerColor(p));
			}
		}

		void Client::DrawDebugAim() {
			SPADES_MARK_FUNCTION();

			float sw = renderer->ScreenWidth();
			float sh = renderer->ScreenHeight();

			Player& p = GetCameraTargetPlayer();

			Weapon& weapon = p.GetWeapon();
			float spread = weapon.GetSpread();
			if (GetAimDownZoomScale() == 1)
				spread *= 2;

			float fovY = tanf(lastSceneDef.fovY * 0.5F);
			float spreadDistance = spread * (sh * 0.5F) / fovY;

			AABB2 boundary(0, 0, 0, 0);
			boundary.min += spreadDistance;
			boundary.max -= spreadDistance;

			Vector2 center;
			center.x = sw * 0.5F;
			center.y = sh * 0.5F;

			Vector2 p1 = center;
			Vector2 p2 = center;

			p1.x += (int)floorf(boundary.min.x);
			p1.y += (int)floorf(boundary.min.y);
			p2.x += (int)ceilf(boundary.max.x);
			p2.y += (int)ceilf(boundary.max.y);

			renderer->SetColorAlphaPremultiplied(MakeVector4(0, 0, 0, 1));
			renderer->DrawOutlinedRect(p1.x, p1.y, p2.x, p2.y);

			renderer->SetColorAlphaPremultiplied(MakeVector4(1, 1, 1, 1));
			renderer->DrawOutlinedRect(p1.x + 1, p1.y + 1, p2.x - 1, p2.y - 1);
		}

		void Client::DrawFirstPersonHUD() {
			SPADES_MARK_FUNCTION();

			float sw = renderer->ScreenWidth();
			float sh = renderer->ScreenHeight();

			int playerId = GetCameraTargetPlayerId();
			Player& p = world->GetPlayer(playerId).value();

			clientPlayers[playerId]->Draw2D();

			if (cg_hitIndicator && hitFeedbackIconState > 0.0F) {
				Handle<IImage> img = renderer->RegisterImage("Gfx/HitFeedback.png");
				Vector2 size = {img->GetWidth(), img->GetHeight()};

				Vector4 color = hitFeedbackFriendly
					? MakeVector4(0.02F, 1, 0.02F, 1)
					: MakeVector4(1, 0.02F, 0.04F, 1);

				renderer->SetColorAlphaPremultiplied(color * hitFeedbackIconState);
				renderer->DrawImage(img, (MakeVector2(sw, sh) - size) * 0.5F);
			}

			if (cg_debugAim && p.IsToolWeapon())
				DrawDebugAim();
		}

		void Client::DrawJoinedAlivePlayerHUD() {
			SPADES_MARK_FUNCTION();

			float sw = renderer->ScreenWidth();
			float sh = renderer->ScreenHeight();

			float ratio = (sw / sh);
			float safeZoneXMin = 0.75F;
			if (ratio < 0.26F)
				safeZoneXMin = 0.28F;
			else if (ratio < 0.56F)
				safeZoneXMin = 0.475F;

			float safeZoneX = Clamp((float)cg_hudSafezoneX, safeZoneXMin, 1.0F);
			float safeZoneY = Clamp((float)cg_hudSafezoneY, 0.85F, 1.0F);

			float x = (sw - 16.0F) * safeZoneX;
			float y = (sh - 16.0F) * safeZoneY;

			// draw damage rings
			hurtRingView->Draw();

			Player& p = GetWorld()->GetLocalPlayer().value();
			Weapon& weapon = p.GetWeapon();
			Player::ToolType tool = p.GetTool();

			Handle<IImage> ammoIcon;
			float iw, ih, spacing = 1.0F;
			int clipNum, clipSize, stockNum, stockMax;

			int ammoStyle = cg_hudAmmoStyle;

			IntVector3 col;
			switch ((int)cg_hudColor) {
				case 1: col = p.GetColor(); break; // team color
				case 2: col = MakeIntVector3(120, 200, 255); break; // light blue
				case 3: col = MakeIntVector3(0, 100, 255); break; // blue
				case 4: col = MakeIntVector3(230, 100, 255); break; // purple
				case 5: col = MakeIntVector3(255, 50, 50); break; // red
				case 6: col = MakeIntVector3(255, 120, 50); break; // orange
				case 7: col = MakeIntVector3(255, 255, 0); break; // yellow
				case 8: col = MakeIntVector3(0, 255, 0); break; // green
				case 9: col = MakeIntVector3(0, 255, 120); break; // aqua
				case 10: col = MakeIntVector3(255, 120, 150); break; // pink
				default: // custom
					col.x = (int)cg_hudColorR;
					col.y = (int)cg_hudColorG;
					col.z = (int)cg_hudColorB;
					break;
			}

			Vector4 color = ConvertColorRGBA(col);
			Vector4 shadowColor = MakeVector4(0, 0, 0, 0.5);

			switch (tool) {
				case Player::ToolSpade:
				case Player::ToolBlock:
					ih = iw = 0.0F;
					clipNum = stockNum = p.GetNumBlocks();
					clipSize = stockMax = 50;
					break;
				case Player::ToolGrenade:
					ih = iw = 0.0F;
					clipNum = stockNum = p.GetNumGrenades();
					clipSize = stockMax = 3;
					break;
				case Player::ToolWeapon: {
					switch (weapon.GetWeaponType()) {
						case RIFLE_WEAPON:
							ammoIcon = renderer->RegisterImage("Gfx/Bullet/7.62mm.png");
							iw = 6.0F;
							ih = iw * 4.0F;
							break;
						case SMG_WEAPON:
							ammoIcon = renderer->RegisterImage("Gfx/Bullet/9mm.png");
							iw = 4.0F;
							ih = iw * 4.0F;
							spacing = 0.0F;
							break;
						case SHOTGUN_WEAPON:
							ammoIcon = renderer->RegisterImage("Gfx/Bullet/12gauge.png");
							iw = 8.0F;
							ih = iw * 2.5F;
							break;
						default: SPInvalidEnum("weapon.GetWeaponType()", weapon.GetWeaponType());
					}

					clipNum = weapon.GetAmmo();
					clipSize = weapon.GetClipSize();
					clipSize = std::max(clipSize, clipNum);

					// draw ammo icon
					if (ammoStyle < 1) {
						for (int i = 0; i < clipSize; i++) {
							float ix = x - ((float)(i + 1) * (iw + spacing));
							float iy = y - ih;

							renderer->SetColorAlphaPremultiplied((clipNum >= i + 1)
								? color : (color * MakeVector4(0.4F, 0.4F, 0.4F, 1)));
							renderer->DrawImage(ammoIcon, AABB2(ix, iy, iw, ih));
						}
					}

					stockNum = weapon.GetStock();
					stockMax = weapon.GetMaxStock();
				} break;
				default: SPInvalidEnum("p.GetTool()", tool);
			}

			// draw "press ... to reload"
			if (tool == Player::ToolWeapon) {
				std::string msg = "";
				if (weapon.IsAwaitingReloadCompletion())
					msg = _Tr("Client", "Reloading");
				else if (stockNum > 0 && clipNum < (clipSize / 4))
					msg = _Tr("Client", "Press [{0}] to Reload", TrKey(cg_keyReloadWeapon));

				if (!msg.empty()) {
					IFont& font = fontManager->GetGuiFont();
					Vector2 size = font.Measure(msg);
					Vector2 pos = MakeVector2((sw - size.x) * 0.5F, sh * (2.0F / 3.0F));
					font.DrawShadow(msg, pos, 1.0F, MakeVector4(1, 1, 1, 1), shadowColor);
				}
			}

			// draw remaining ammo counter
			{
				float per = Clamp((float)stockNum / (float)(stockMax / 3), 0.0F, 1.0F);
				Vector4 col = color + (MakeVector4(1, 0, 0, 1) - color) * (1.0F - per);

				auto stockStr = ToString(stockNum);
				if (ammoStyle >= 1 && tool == Player::ToolWeapon)
					stockStr = ToString(clipNum) + "-" + stockStr;

				IFont& font = fontManager->GetSquareDesignFont();
				Vector2 size = font.Measure(stockStr);
				Vector2 pos = MakeVector2(x, y) - size;
				if (ammoStyle < 1)
					pos.y -= ih;

				font.DrawShadow(stockStr, pos, 1.0F, col, shadowColor);
			}

			// draw player health
			{
				int hp = p.GetHealth(); // current player health
				int maxHealth = 100; // server doesn't send this
				float hpFrac = Clamp((float)hp / (float)maxHealth, 0.0F, 1.0F);
				Vector4 col = color + (MakeVector4(1, 0, 0, 1) - color) * (1.0F - hpFrac);

				float hurtTime = time - lastHurtTime;
				hurtTime = 1.0F - (hurtTime / 0.25F);
				if (hurtTime < 0.0F)
					hurtTime = 0.0F;
				col = col + (MakeVector4(1, 1, 1, 1) - col) * hurtTime;

				auto healthStr = ToString(hp);
				IFont& font = fontManager->GetSquareDesignFont();
				Vector2 size = font.Measure(healthStr);
				Vector2 pos = MakeVector2(sw - x, y - size.y);
				font.DrawShadow(healthStr, pos, 1.0F, col, shadowColor);
			}

			if (tool == Player::ToolBlock)
				paletteView->Draw();
		}

		void Client::DrawHitTestDebugger() {
			SPADES_MARK_FUNCTION();

			auto* debugger = world->GetHitTestDebugger();
			if (!debugger)
				return;

			auto bmp = debugger->GetBitmap();
			if (bmp) {
				auto img = renderer->CreateImage(*bmp);
				debugHitTestImage.Set(img.GetPointerOrNull());
			}

			if (debugHitTestImage) {
				float sw = renderer->ScreenWidth();
				float sh = renderer->ScreenHeight();

				float cfgWndSize = cg_dbgHitTestSize;
				Vector2 wndSize = {cfgWndSize, cfgWndSize};

				Vector2 zoomedSize = {512, 512};
				if (sw < zoomedSize.x || sh < zoomedSize.y)
					zoomedSize *= 0.75F;

				if (debugHitTestZoom) {
					float per = debugHitTestZoomState;
					per = 1.0F - per;
					per *= per;
					per = 1.0F - per;
					per = Mix(0.75F, 1.0F, per);
					zoomedSize = Mix(MakeVector2(0, 0), zoomedSize, per);
					wndSize = zoomedSize;
				}

				AABB2 outRect((sw - wndSize.x) - 8.0F, (sh - wndSize.y) - 68.0F, wndSize.x, wndSize.y);
				if (debugHitTestZoom) {
					outRect.min = MakeVector2((sw - zoomedSize.x) * 0.5F, (sh - zoomedSize.y) * 0.5F);
					outRect.max = MakeVector2((sw + zoomedSize.x) * 0.5F, (sh + zoomedSize.y) * 0.5F);
				}

				float alpha = debugHitTestZoom ? debugHitTestZoomState : 1.0F;
				renderer->SetColorAlphaPremultiplied(MakeVector4(alpha, alpha, alpha, alpha));
				renderer->DrawImage(debugHitTestImage, outRect, AABB2(128, 512 - 128, 256, 256 - 512)); // flip Y axis

				renderer->SetColorAlphaPremultiplied(MakeVector4(0, 0, 0, alpha));
				renderer->DrawOutlinedRect(outRect.min.x - 1, outRect.min.y - 1, outRect.max.x + 1, outRect.max.y + 1);
			}
		}

		void Client::DrawPlayerStats() {
			SPADES_MARK_FUNCTION();

			IFont& font = cg_smallFont
				? fontManager->GetSmallFont()
				: fontManager->GetGuiFont();

			float sh = renderer->ScreenHeight();

			float x = 8.0F;
			float y = sh * 0.5F;
			y -= (float)cg_playerStatsHeight;

			float lh = cg_smallFont ? 12.0F : 20.0F;
			auto addLine = [&](const char* format, ...) {
				char buf[256];
				va_list va;
				va_start(va, format);
				vsnprintf(buf, sizeof(buf), format, va);
				va_end(va);

				Vector2 pos = MakeVector2(x, y);
				y += lh;
				font.DrawShadow(buf, pos, 1.0F, MakeVector4(1, 1, 1, 1),
				                MakeVector4(0, 0, 0, 0.8F));
			};

			addLine("K/D Ratio: %.3g", curKills / float(std::max(1, curDeaths)));
			addLine("Kill Streak: %d", curStreak);
			addLine("Best Streak: %d", bestStreak);
			addLine("Melee Kills: %d", meleeKills);
			addLine("Grenade Kills: %d", grenadeKills);

			if (cg_playerStatsShowPlacedBlocks)
				addLine("Blocks Placed: %d", placedBlocks);
		}

		void Client::UpdateDamageIndicators(float dt) {
			for (auto it = damageIndicators.begin();
			     it != damageIndicators.end();) {
				DamageIndicator& ent = *it;
				ent.fade -= dt;
				if (ent.fade < 0) {
					std::list<DamageIndicator>::iterator tmp = it++;
					damageIndicators.erase(tmp);
					continue;
				}

				ent.position.x += ent.velocity.x * dt;
				ent.position.y += ent.velocity.y * dt;
				ent.position.z -= 2.0F * dt;

				++it;
			}
		}

		void Client::DrawDamageIndicators() {
			SPADES_MARK_FUNCTION();

			for (const auto& damages : damageIndicators) {
				float fade = damages.fade;
				if (fade > 1.0F)
					fade = 1.0F;

				Vector2 scrPos;
				if (Project(damages.position, scrPos)) {
					int damage = damages.damage;

					auto damageStr = "-" + ToString(damage);
					IFont& font = fontManager->GetGuiFont();
					Vector2 size = font.Measure(damageStr);
					scrPos.x -= size.x * 0.5F;
					scrPos.y -= size.y;

					float per = 1.0F - (damage / 100.0F);
					font.DrawShadow(damageStr, scrPos, 1.0F, MakeVector4(1, per, per, fade),
					                MakeVector4(0, 0, 0, 0.25F * fade));
				}
			}
		}

		void Client::DrawDeadPlayerHUD() {
			SPADES_MARK_FUNCTION();

			float sw = renderer->ScreenWidth();
			float sh = renderer->ScreenHeight();

			Player& p = GetWorld()->GetLocalPlayer().value();

			std::string msg;
			int secs = (int)p.GetTimeToRespawn();
			if (secs > 0) {
				static int lastCount = 0;
				if (lastCount != secs) {
					if (secs <= 3) {
						Handle<IAudioChunk> c = (secs == 1)
							? audioDevice->RegisterSound("Sounds/Feedback/Beep1.opus")
							: audioDevice->RegisterSound("Sounds/Feedback/Beep2.opus");
						AudioParam param;
						param.volume = cg_respawnSoundGain;
						audioDevice->PlayLocal(c.GetPointerOrNull(), param);
					}

					lastCount = secs;
				}

				msg = _Tr("Client", "Respawning in: {0}", secs);
			} else {
				msg = _Tr("Client", "Waiting for respawn");
			}

			if (!msg.empty()) {
				IFont& font = fontManager->GetGuiFont();
				Vector2 size = font.Measure(msg);
				Vector2 pos = MakeVector2((sw - size.x) * 0.5F, sh / 3.0F);
				font.DrawShadow(msg, pos, 1.0F, MakeVector4(1, 1, 1, 1), MakeVector4(0, 0, 0, 0.5));
			}
		}

		void Client::DrawSpectateHUD() {
			SPADES_MARK_FUNCTION();

			IFont& font = cg_smallFont
				? fontManager->GetSmallFont()
				: fontManager->GetGuiFont();

			float sw = renderer->ScreenWidth();
			float sh = renderer->ScreenHeight();

			Player& p = GetWorld()->GetLocalPlayer().value();
			bool localPlayerIsSpectator = p.IsSpectator();

			float x = sw - 8.0F;
			float minY = sh * 0.5F;
			minY -= 64.0F;

			float y = cg_minimapSize;
			if (y < minY)
				y = minY;
			if (y > 256.0F)
				y = 256.0F;
			y += 32.0F;

			float lh = cg_smallFont ? 12.0F : 20.0F;
			auto addLine = [&](const std::string& text) {
				Vector2 pos = MakeVector2(x, y);
				pos.x -= font.Measure(text).x;
				y += lh;
				font.DrawShadow(text, pos, 1.0F, MakeVector4(1, 1, 1, 1),
				                MakeVector4(0, 0, 0, 0.5));
			};

			auto cameraMode = GetCameraMode();

			if (HasTargetPlayer(cameraMode)) {
				int playerId = GetCameraTargetPlayerId();

				addLine(_Tr("Client", "Following {0} [#{1}]",
					  world->GetPlayerName(playerId), playerId));
			}

			y += lh * 0.5F;

			// Help messages (make sure to synchronize these with the keyboard input handler)
			if (FollowsNonLocalPlayer(cameraMode)) {
				if (GetCameraTargetPlayer().IsAlive())
					addLine(_Tr("Client", "[{0}] Cycle camera mode", TrKey(cg_keyJump)));

				addLine(_Tr("Client", "[{0}/{1}] Next/Prev player",
					TrKey(cg_keyAttack), TrKey(cg_keyAltAttack)));

				if (localPlayerIsSpectator)
					addLine(_Tr("Client", "[{0}] Unfollow", TrKey(cg_keyReloadWeapon)));
			} else {
				addLine(_Tr("Client", "[{0}/{1}] Follow a player",
					TrKey(cg_keyAttack), TrKey(cg_keyAltAttack)));
			}

			if (cameraMode == ClientCameraMode::Free)
				addLine(_Tr("Client", "[{0}/{1}] Go up/down",
					TrKey(cg_keyJump), TrKey(cg_keyCrouch)));

			y += lh * 0.5F;

			if (localPlayerIsSpectator && !inGameLimbo)
				addLine(_Tr("Client", "[{0}] Select Team/Weapon", TrKey(cg_keyLimbo)));
		}

		void Client::DrawAlert() {
			SPADES_MARK_FUNCTION();

			float sw = renderer->ScreenWidth();
			float sh = renderer->ScreenHeight();

			float fade = 1.0F - (time - alertDisappearTime);
			fade = std::min(fade, 1.0F);
			if (fade <= 0.0F)
				return;

			float borderFade = (time - alertAppearTime) / 0.5F;
			borderFade = Clamp(1.0F - borderFade, 0.0F, 1.0F);

			Handle<IImage> alertIcon = renderer->RegisterImage("Gfx/AlertIcon.png");

			IFont& font = fontManager->GetGuiFont();
			Vector2 textSize = font.Measure(alertContents);
			Vector2 contentsSize = textSize;
			contentsSize.y = std::max(contentsSize.y, 16.0F);
			if (alertType != AlertType::Notice)
				contentsSize.x += 22.0F;

			// add margin
			const float margin = 4.0F;
			contentsSize += margin * 2.0F;
			contentsSize = contentsSize.Floor();

			Vector2 pos = MakeVector2(sw, sh) - contentsSize;
			pos *= MakeVector2(0.5F, 0.7F);
			pos.y += 40.0F;
			pos = pos.Floor();

			Vector4 color;
			switch (alertType) {
				case AlertType::Notice: color = MakeVector4(0, 0, 0, 1); break;
				case AlertType::Warning: color = MakeVector4(1, 1, 0, 1); break;
				case AlertType::Error: color = MakeVector4(1, 0, 0, 1); break;
				default: color = MakeVector4(0, 0, 0, 1); break;
			}

			Vector4 shadowColor = MakeVector4(0, 0, 0, 0.5F);

			float x = pos.x - margin;
			float y = pos.y;
			float w = pos.x + contentsSize.x + margin;
			float h = pos.y + contentsSize.y;

			// draw background
			renderer->SetColorAlphaPremultiplied(shadowColor * fade);
			renderer->DrawFilledRect(x + 1, y + 1, w - 1, h - 1);

			// draw border
			renderer->SetColorAlphaPremultiplied(color * fade * (1.0F - borderFade));
			renderer->DrawOutlinedRect(x, y, w, h);

			// draw fading border
			float scale = 8.0F * (1.0F - borderFade);
			renderer->SetColorAlphaPremultiplied(color * borderFade);
			renderer->DrawOutlinedRect(x - scale, y - scale, w + scale, h + scale);

			// draw alert icon
			if (alertType != AlertType::Notice) {
				Vector2 iconPos = pos;
				iconPos.x += margin;
				iconPos.y += (contentsSize.y - 16.0F) * 0.5F;

				renderer->SetColorAlphaPremultiplied(color * fade);
				renderer->DrawImage(alertIcon, iconPos);
			}

			// draw text
			Vector2 textPos = pos;
			textPos.x += (contentsSize.x - textSize.x) - margin;
			textPos.y += (contentsSize.y - textSize.y) * 0.5F;

			color = MakeVector4(1, 1, 1, fade);
			shadowColor.w = 0.5F * fade;

			font.DrawShadow(alertContents, textPos, 1.0F, color, shadowColor);
		}

		void Client::Draw2DWithWorld() {
			SPADES_MARK_FUNCTION();

			for (const auto& ent : localEntities)
				ent->Render2D();

			float sw = renderer->ScreenWidth();
			float sh = renderer->ScreenHeight();

			// fade the map when the world starts (draw)
			float timeSinceWorldStart = time - worldSetTime;
			float fade = Clamp(1.0F - (timeSinceWorldStart - 1.0F) / 2.5F, 0.0F, 1.0F);
			if (fade > 0.0F) {
				renderer->SetColorAlphaPremultiplied(MakeVector4(0, 0, 0, fade));
				renderer->DrawImage(nullptr, AABB2(0, 0, sw, sh));
			}

			stmp::optional<Player&> p = GetWorld()->GetLocalPlayer();
			if (p) { // joined local player
				if (cg_hurtScreenEffects) {
					DrawHurtSprites();
					DrawHurtScreenEffect();
				}

				bool localPlayerIsSpectator = p->IsSpectator();
				if (!localPlayerIsSpectator) {
					if (cg_playerNames)
						DrawHottrackedPlayerName();
					if (cg_damageIndicators)
						DrawDamageIndicators();
				} else {
					DrawPubOVL();
				}

				if (!cg_hideHud) {
					tcView->Draw();

					if (cg_hudPlayerCount)
						DrawAlivePlayersCount();
					if (IsFirstPerson(GetCameraMode()))
						DrawFirstPersonHUD();

					// draw map
					bool largeMap = largeMapView->IsZoomed();
					if (!largeMap)
						mapView->Draw();

					if (!localPlayerIsSpectator) { // player is not spectator
						if (cg_playerStats)
							DrawPlayerStats();
						if (!p->IsToolBlock() && !debugHitTestZoom)
							DrawHitTestDebugger();

						if (p->IsAlive()) {
							DrawJoinedAlivePlayerHUD();
						} else {
							DrawDeadPlayerHUD();
							DrawSpectateHUD();
						}
					} else {
						DrawSpectateHUD();
					}

					chatWindow->Draw();
					killfeedWindow->Draw();
					DrawAlert();

					if (debugHitTestZoom)
						DrawHitTestDebugger();

					// large map view should come in front
					if (largeMap)
						largeMapView->Draw();
				} else if (AcceptsTextInput() || chatWindow->IsExpanded()) {
					// chat bypass cg_hideHud
					chatWindow->Draw();
				}

				centerMessageView->Draw();
				if (scoreboardVisible) {
					scoreboard->Draw();
					DrawPlayingTime();
				}

				// --- end "player is there" render
			} else {
				// world exists, but no local player: not joined

				scoreboard->Draw();
				centerMessageView->Draw();
				DrawAlert();
			}

			if (IsLimboViewActive())
				limbo->Draw();

			if (cg_stats && !cg_hideHud)
				DrawStats();
		}

		void Client::Draw2DWithoutWorld() {
			SPADES_MARK_FUNCTION();

			DrawSplash();

			// no world; loading?
			float sw = renderer->ScreenWidth();
			float sh = renderer->ScreenHeight();

			float prgBarW = 440.0F;
			float prgBarH = 8.0F;
			float prgBarX = (sw - prgBarW) * 0.5F;
			float prgBarY = sh - 50.0F;

			// draw background bar
			renderer->SetColorAlphaPremultiplied(MakeVector4(0.2F, 0.2F, 0.2F, 1));
			renderer->DrawImage(nullptr, AABB2(prgBarX, prgBarY, prgBarW, prgBarH));

			// draw progress bar
			if (net->GetStatus() == NetClientStatusReceivingMap) {
				float progress = mapReceivingProgressSmoothed;
				float progressBarMaxWidth = prgBarW * progress;

				Vector4 color = MakeVector4(0, 0.5, 1, 1);
				Vector4 darkCol = color * 0.5F;
				darkCol.w = color.w;

				for (float x = 0.0F; x < progressBarMaxWidth; x += 1.0F) {
					float per = x / progressBarMaxWidth;
					renderer->SetColorAlphaPremultiplied(darkCol + (color - darkCol) * per);
					renderer->DrawImage(nullptr, AABB2(prgBarX + x, prgBarY, 1.0F, prgBarH));
				}
			} else { // draw indeterminate progress bar
				const float progressPosition = fmodf(timeSinceInit * 0.7F, 1.0F);
				const float centerX = progressPosition * (prgBarW + 400.0F) - 200.0F;
				for (float x = 0.0F; x < prgBarW; x += 1.0F) {
					float opacity = 1.0F - fabsf(x - centerX) / 200.0F;
					opacity = std::max(opacity, 0.0F) * 0.5F + 0.05F;
					renderer->SetColorAlphaPremultiplied(MakeVector4(0.5, 0.5, 0.5, 1) * opacity);
					renderer->DrawImage(nullptr, AABB2(prgBarX + x, prgBarY, 1.0F, prgBarH));
				}
			}

			// draw net status
			auto statusStr = net->GetStatusString();
			IFont& font = fontManager->GetGuiFont();
			Vector2 size = font.Measure(statusStr);
			Vector2 pos = MakeVector2((sw - size.x) * 0.5F, prgBarY - 10.0F - size.y);
			font.Draw(statusStr, pos, 1.0F, MakeVector4(0.5, 0.5, 0.5, 1));
		}

		void Client::DrawStats() {
			SPADES_MARK_FUNCTION();

			// only draw stats when scoreboard is visible
			if (!scoreboardVisible && (int)cg_stats >= 3)
				return;

			float sw = renderer->ScreenWidth();
			float sh = renderer->ScreenHeight();

			char buf[256];
			std::string str;

			{
				auto fps = (int)fpsCounter.GetFps();
				if (fps <= 0) {
					str += "fps:NA";
				} else {
					sprintf(buf, "%dfps", fps);
					str += buf;
				}
			}
			{
				// Display world updates per second
				auto ups = (int)upsCounter.GetFps();
				if (ups <= 0) {
					str += ", ups:NA";
				} else {
					sprintf(buf, ", %dups", ups);
					str += buf;
				}
			}

			if (net) {
				auto ping = net->GetPing();
				auto upbps = (int)(net->GetUplinkBps() / 1000);
				auto downbps = (int)(net->GetDownlinkBps() / 1000);
				sprintf(buf, ", ping: %dms, up/down: %d/%dkbps", ping, upbps, downbps);
				str += buf;
			}

			// add margin
			const float margin = 4.0F;
			IFont& font = cg_smallFont
				? fontManager->GetSmallFont()
				: fontManager->GetGuiFont();
			Vector2 size = font.Measure(str) + (margin * 2.0F);
			Vector2 pos = MakeVector2(sw, sh) - size;
			pos *= MakeVector2(0.5F, ((int)cg_stats < 2) ? 1.0F : 0.0F);
			pos.y += ((int)cg_stats < 2) ? (margin * 0.5F) : -(margin * 0.5F);

			Vector4 color = MakeVector4(1, 1, 1, 1);
			Vector4 shadowColor = MakeVector4(0, 0, 0, 0.5);

			if (cg_statsBackground) {
				float x = pos.x;
				float y = pos.y + (margin * 0.5F);
				float w = pos.x + size.x;
				float h = pos.y + size.y - (margin * 0.5F);

				// draw background
				renderer->SetColorAlphaPremultiplied(shadowColor);
				renderer->DrawFilledRect(x + 1, y + 1, w - 1, h - 1);

				// draw outline
				renderer->SetColorAlphaPremultiplied(MakeVector4(0, 0, 0, 0.8F));
				renderer->DrawOutlinedRect(x, y, w, h);
			}

			// draw text
			font.DrawShadow(str, pos + margin, 1.0F, color, shadowColor);
		}

		void Client::Draw2D() {
			SPADES_MARK_FUNCTION();

			if (GetWorld())
				Draw2DWithWorld();
			else
				Draw2DWithoutWorld();
		}
	} // namespace client
} // namespace spades
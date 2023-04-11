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
DEFINE_SPADES_SETTING(cg_playerStats, "0");
DEFINE_SPADES_SETTING(cg_hideHud, "0");
DEFINE_SPADES_SETTING(cg_hudAmmoStyle, "0");
DEFINE_SPADES_SETTING(cg_hudBorderX, "16");
DEFINE_SPADES_SETTING(cg_hudBorderY, "16");
DEFINE_SPADES_SETTING(cg_playerNames, "2");
DEFINE_SPADES_SETTING(cg_playerNameX, "0");
DEFINE_SPADES_SETTING(cg_playerNameY, "0");
DEFINE_SPADES_SETTING(cg_dbgHitTestSize, "128");
DEFINE_SPADES_SETTING(cg_damageIndicators, "1");
DEFINE_SPADES_SETTING(cg_hurtScreenEffects, "1");

SPADES_SETTING(cg_minimapSize);

namespace spades {
	namespace client {

		enum class ScreenshotFormat { JPG, TGA, PNG };

		namespace {
			ScreenshotFormat GetScreenshotFormat(const std::string& format) {
				if (EqualsIgnoringCase(format, "jpeg"))
					return ScreenshotFormat::JPG;
				else if (EqualsIgnoringCase(format, "tga"))
					return ScreenshotFormat::TGA;
				else if (EqualsIgnoringCase(format, "png"))
					return ScreenshotFormat::PNG;
				else
					SPRaise("Invalid screenshot format: %s", format.c_str());
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

			int now = (int)world->GetTime();
			int mins = now / 60;
			int secs = now - mins * 60;
			char buf[64];
			sprintf(buf, "%d:%.02d", mins, secs);
			IFont& font = fontManager->GetMediumFont();
			Vector2 size = font.Measure(buf);
			Vector2 pos = MakeVector2((sw - size.x) * 0.5F, 48.0F - size.y);
			font.DrawShadow(buf, pos, 1.0F, MakeVector4(1, 1, 1, 1), MakeVector4(0, 0, 0, 0.5));
		}

		void Client::DrawHurtSprites() {
			float per = (world->GetTime() - lastHurtTime) / 1.5F;
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

			float wTime = world->GetTime();
			float timeSinceLastHurt = wTime - lastHurtTime;

			const float fadeOutTime = 0.35F;
			if (wTime >= lastHurtTime && timeSinceLastHurt < fadeOutTime) {
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

			float dist = dir.GetLength2D();
			dir = dir.Normalize();

			// do map raycast
			GameMap::RayCastResult mapResult;
			mapResult = map->CastRay2(eye, dir, 256);

			if (dist > FOG_DISTANCE) {
				playerColor = MakeVector4(1, 0.75, 0, 1);
			} else if (mapResult.hit) {
				float hitDist = (mapResult.hitPos - eye).GetLength2D();
				if (hitDist < FOG_DISTANCE && hitDist < dist)
					playerColor = ConvertColorRGBA(player.GetColor());
			}

			return playerColor;
		}

		void Client::DrawPlayerName(Player& player, Vector4 color) {
			SPADES_MARK_FUNCTION();

			Vector3 origin = player.GetEye();
			origin.z -= 0.45F; // above player head

			Vector3 posxyz;
			if (Project(origin, posxyz)) {
				Vector2 pos = {posxyz.x, posxyz.y};
				pos.x += (int)cg_playerNameX;
				pos.y += (int)cg_playerNameY;

				char buf[64];
				auto nameStr = player.GetName();
				sprintf(buf, "%s", nameStr.c_str());
				if (cg_playerNames == 1) {
					Vector3 diff = origin - lastSceneDef.viewOrigin;
					float dist = diff.GetLength2D();
					if (dist < FOG_DISTANCE)
						sprintf(buf, "%s [%.1f]", nameStr.c_str(), dist);
				}

				IFont& font = fontManager->GetGuiFont();
				Vector2 size = font.Measure(buf);
				pos.x -= size.x * 0.5F;
				pos.y -= size.y;

				float luminosity = color.x + color.y + color.z;
				Vector4 shadowColor = (luminosity > 0.9F)
					? MakeVector4(0, 0, 0, 0.8F)
					: MakeVector4(1, 1, 1, 0.8F);

				font.DrawShadow(buf, pos, 1.0F, color, shadowColor);
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
				auto maybePlayer = world->GetPlayer(i);
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

			float x = sw - (int)cg_hudBorderX;
			float y = sh - (int)cg_hudBorderY;

			// Draw damage rings
			hurtRingView->Draw();

			Player& p = GetWorld()->GetLocalPlayer().value();

			Weapon& weap = p.GetWeapon();
			Player::ToolType tool = p.GetTool();

			Handle<IImage> ammoIcon;
			float iw, ih, spacing = 1.0F;
			int clipNum, clipSize, stockNum, stockMax;

			Vector4 color = MakeVector4(1, 1, 1, 1);
			Vector4 shadowColor = MakeVector4(0, 0, 0, 0.5);

			int ammoStyle = cg_hudAmmoStyle;

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
					switch (weap.GetWeaponType()) {
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
						default: SPInvalidEnum("weap.GetWeaponType()", weap.GetWeaponType());
					}

					clipNum = weap.GetAmmo();
					clipSize = weap.GetClipSize();
					clipSize = std::max(clipSize, clipNum);

					if (ammoStyle != 1) {
						for (int i = 0; i < clipSize; i++) {
							float ix = x - ((float)(i + 1) * (iw + spacing));
							float iy = y - ih;

							renderer->SetColorAlphaPremultiplied((clipNum >= i + 1)
								? color : MakeVector4(0.4F, 0.4F, 0.4F, 1));
							renderer->DrawImage(ammoIcon, AABB2(ix, iy, iw, ih));
						}
					}

					stockNum = weap.GetStock();
					stockMax = weap.GetMaxStock();
				} break;
				default: SPInvalidEnum("p.GetTool()", tool);
			}

			// draw "press ... to reload"
			if (tool == Player::ToolWeapon) {
				std::string msg = "";
				if (weap.IsReloading() || p.IsAwaitingReloadCompletion())
					msg = _Tr("Client", "Reloading");
				else if (stockNum > 0 && clipNum < (clipSize / 4))
					msg = _Tr("Client", "Press [{0}] to Reload", TrKey(cg_keyReloadWeapon));

				if (!msg.empty()) {
					IFont& font = fontManager->GetGuiFont();
					Vector2 size = font.Measure(msg);
					Vector2 pos = MakeVector2((sw - size.x) * 0.5F, sh * (2.0F / 3.0F));
					font.DrawShadow(msg, pos, 1.0F, color, shadowColor);
				}
			}

			// draw remaining ammo counter
			{
				float per = std::min((2.0F * stockNum) / (float)stockMax, 1.0F);
				color = MakeVector4(1, per, per, 1);

				auto stockStr = ToString(stockNum);
				if (ammoStyle == 1 && tool == Player::ToolWeapon)
					stockStr = ToString(clipNum) + "-" + stockStr;

				IFont& font = fontManager->GetSquareDesignFont();
				Vector2 size = font.Measure(stockStr);
				Vector2 pos = MakeVector2(x, y) - size;
				if (ammoStyle != 1)
					pos.y -= ih;

				font.DrawShadow(stockStr, pos, 1.0F, color, shadowColor);
			}

			// draw player health
			{
				int hp = p.GetHealth();
				float per = hp / 100.0F;
				color = MakeVector4(1, per, per, 1);

				auto healthStr = ToString(hp);
				IFont& font = fontManager->GetSquareDesignFont();
				Vector2 size = font.Measure(healthStr);
				Vector2 pos = MakeVector2(sw - x, y - size.y);
				font.DrawShadow(healthStr, pos, 1.0F, color, shadowColor);
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
				renderer->DrawOutlinedRect(outRect.min.x - 1, outRect.min.y - 1, outRect.max.x + 1, outRect.max.y + 1);
			}
		}

		void Client::DrawPlayerStats() {
			SPADES_MARK_FUNCTION();

			IFont& font = fontManager->GetSmallFont();

			float sh = renderer->ScreenHeight();

			float x = 8.0F;
			float y = sh * 0.5F;
			y -= 64.0F;

			auto addLine = [&](const char* format, ...) {
				char buf[256];
				va_list va;
				va_start(va, format);
				vsnprintf(buf, sizeof(buf), format, va);
				va_end(va);

				Vector2 pos = MakeVector2(x, y);
				y += 16.0F;
				font.DrawShadow(buf, pos, 1.0F, MakeVector4(1, 1, 1, 0.8F),
				                MakeVector4(0, 0, 0, 0.8F));
			};

			addLine("K/D Ratio: %.3g", curKills / float(std::max(1, curDeaths)));
			addLine("Kill Streak: %d", curStreak);
			addLine("Last Streak: %d", lastStreak);
			addLine("Best Streak: %d", bestStreak);
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

				ent.position += ent.velocity * dt;
				ent.velocity.z += 32.0F * dt * -0.25F;

				++it;
			}
		}

		void Client::DrawDamageIndicators() {
			SPADES_MARK_FUNCTION();

			for (const auto& damages : damageIndicators) {
				float fade = damages.fade;
				if (fade > 1.0F)
					fade = 1.0F;

				Vector3 posxyz;
				if (Project(damages.position, posxyz)) {
					Vector2 pos = {posxyz.x, posxyz.y};

					int damage = damages.damage;

					auto damageStr = "-" + ToString(damage);
					IFont& font = fontManager->GetGuiFont();
					Vector2 size = font.Measure(damageStr);
					pos.x -= size.x * 0.5F;
					pos.y -= size.y;

					float per = 1.0F - (damage / 100.0F);
					font.DrawShadow(damageStr, pos, 1.0F, MakeVector4(1, per, per, fade),
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
						audioDevice->PlayLocal(c.GetPointerOrNull(), AudioParam());
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

			IFont& font = fontManager->GetGuiFont();

			float sw = renderer->ScreenWidth();
			float sh = renderer->ScreenHeight();

			float x = sw - 8.0F;
			float minY = sh * 0.5F;
			minY -= 64.0F;

			float y = cg_minimapSize;
			if (y < minY)
				y = minY;
			if (y > 256.0F)
				y = 256.0F;
			y += 32.0F;

			auto addLine = [&](const std::string& text) {
				Vector2 pos = MakeVector2(x, y);
				pos.x -= font.Measure(text).x;
				y += 20.0F;
				font.DrawShadow(text, pos, 1.0F, MakeVector4(1, 1, 1, 1),
				                MakeVector4(0, 0, 0, 0.5));
			};

			auto cameraMode = GetCameraMode();

			if (HasTargetPlayer(cameraMode)) {
				int playerId = GetCameraTargetPlayerId();

				addLine(_Tr("Client", "Following {0} [#{1}]",
					  world->GetPlayerName(playerId), playerId));
			}

			y += 10.0F;

			// Help messages (make sure to synchronize these with the keyboard input handler)
			if (FollowsNonLocalPlayer(cameraMode)) {
				if (GetCameraTargetPlayer().IsAlive())
					addLine(_Tr("Client", "[{0}] Cycle camera mode", TrKey(cg_keyJump)));

				addLine(_Tr("Client", "[{0}/{1}] Next/Prev player",
					TrKey(cg_keyAttack), TrKey(cg_keyAltAttack)));

				if (GetWorld()->GetLocalPlayer()->IsSpectator())
					addLine(_Tr("Client", "[{0}] Unfollow", TrKey(cg_keyReloadWeapon)));
			} else {
				addLine(_Tr("Client", "[{0}/{1}] Follow a player",
					TrKey(cg_keyAttack), TrKey(cg_keyAltAttack)));
			}

			if (cameraMode == ClientCameraMode::Free)
				addLine(_Tr("Client", "[{0}/{1}] Go up/down",
					TrKey(cg_keyJump), TrKey(cg_keyCrouch)));

			y += 10.0F;

			if (GetWorld()->GetLocalPlayer()->IsSpectator() && !inGameLimbo)
				addLine(_Tr("Client", "[{0}] Select Team/Weapon", TrKey(cg_keyLimbo)));
		}

		void Client::DrawAlert() {
			SPADES_MARK_FUNCTION();

			float sw = renderer->ScreenWidth();
			float sh = renderer->ScreenHeight();

			const float fadeOutTime = 1.0F;
			float fade = 1.0F - ((time - alertDisappearTime) / fadeOutTime);
			fade = std::min(fade, 1.0F);
			if (fade <= 0.0F)
				return;

			float borderFade = 1.0F - ((time - alertAppearTime) / 0.5F);
			borderFade = Clamp(borderFade, 0.0F, 1.0F);

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

			Vector4 shadowColor = MakeVector4(0, 0, 0, 0.5F * fade);

			float x = pos.x - margin;
			float y = pos.y;
			float w = pos.x + contentsSize.x + margin;
			float h = pos.y + contentsSize.y;

			// draw background
			renderer->SetColorAlphaPremultiplied(shadowColor);
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
			textPos.y += ((contentsSize.y - textSize.y) * 0.5F) - 1.0F;

			color = MakeVector4(1, 1, 1, 1) * fade;
			font.DrawShadow(alertContents, textPos, 1.0F, color, shadowColor);
		}

		void Client::Draw2DWithWorld() {
			SPADES_MARK_FUNCTION();

			for (const auto& ent : localEntities)
				ent->Render2D();

			float sw = renderer->ScreenWidth();
			float sh = renderer->ScreenHeight();

			// fade the map (draw)
			float fade = Clamp((world->GetTime() - 1.0F) / 2.2F, 0.0F, 1.0F);
			if (fade < 1.0F) {
				renderer->SetColorAlphaPremultiplied(MakeVector4(0, 0, 0, 1.0F - fade));
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
				} else {
					// chat bypass cg_hideHud
					if (AcceptsTextInput() || chatWindow->IsExpanded())
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

			float prgW = 440.0F;
			float prgH = 8.0F;
			float prgX = (sw - prgW) * 0.5F;
			float prgY = sh - 48.0F;

			auto statusStr = net->GetStatusString();
			IFont& font = fontManager->GetGuiFont();
			Vector2 size = font.Measure(statusStr);
			Vector2 pos = MakeVector2((sw - size.x) * 0.5F, prgY - 10.0F);
			pos.y -= size.y;

			Vector4 grayCol = MakeVector4(0.5, 0.5, 0.5, 1);
			Vector3 blueCol = MakeVector3(0, 0.5, 1);

			font.Draw(statusStr, pos, 1.0F, grayCol);

			// background bar
			renderer->SetColorAlphaPremultiplied(grayCol * 0.5F);
			renderer->DrawImage(nullptr, AABB2(prgX, prgY, prgW, prgH));

			// Normal progress bar
			if (net->GetStatus() == NetClientStatusReceivingMap) {
				float prg = mapReceivingProgressSmoothed;

				float w = prgW * prg;
				for (float x = 0; x < w; x++) {
					float tempperc = x / w;
					Vector3 color = Mix(blueCol * 0.25F, blueCol, tempperc);
					renderer->SetColorAlphaPremultiplied(MakeVector4(color.x, color.y, color.z, 1));
					renderer->DrawImage(nullptr, AABB2(prgX + x, prgY, 1.0F, prgH));
				}
			} else { // Indeterminate progress bar
				float pos = timeSinceInit / 3.6F;
				pos -= floorf(pos);
				float centX = pos * (prgW + 400.0F) - 200.0F;

				for (float x = 0; x < prgW; x++) {
					float op = 1.0F - fabsf(x - centX) / 200.0F;
					op = std::max(op, 0.0F) * 0.5F + 0.05F;
					renderer->SetColorAlphaPremultiplied(grayCol * op);
					renderer->DrawImage(nullptr, AABB2(prgX + x, prgY, 1.0F, prgH));
				}
			}

			DrawAlert();
		}

		void Client::DrawStats() {
			SPADES_MARK_FUNCTION();

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
			IFont& font = fontManager->GetGuiFont();
			Vector2 size = font.Measure(str) + (margin * 2.0F);
			Vector2 pos = MakeVector2(sw, sh) - size;
			pos.x *= 0.5F;
			pos.y += margin;

			float x = pos.x;
			float y = pos.y + margin;
			float w = pos.x + size.x;
			float h = pos.y + size.y - margin;

			Vector4 color = MakeVector4(1, 1, 1, 1);
			Vector4 shadowColor = MakeVector4(0, 0, 0, 0.5);

			// draw background
			renderer->SetColorAlphaPremultiplied(shadowColor);
			renderer->DrawFilledRect(x + 1, y + 1, w - 1, h - 1);

			// draw border
			renderer->SetColorAlphaPremultiplied(MakeVector4(0, 0, 0, 1));
			renderer->DrawOutlinedRect(x, y, w, h);

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
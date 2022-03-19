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

#include "CTFGameMode.h"
#include "CenterMessageView.h"
#include "ChatWindow.h"
#include "ClientPlayer.h"
#include "ClientUI.h"
#include "Corpse.h"
#include "FallingBlock.h"
#include "Fonts.h"
#include "GameProperties.h"
#include "HitTestDebugger.h"
#include "HurtRingView.h"
#include "IFont.h"
#include "IGameMode.h"
#include "ILocalEntity.h"
#include "LimboView.h"
#include "MapView.h"
#include "PaletteView.h"
#include "ParticleSpriteEntity.h"
#include "ScoreboardView.h"
#include "SmokeSpriteEntity.h"
#include "TCProgressView.h"
#include "Tracer.h"

#include "GameMap.h"
#include "Grenade.h"
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
DEFINE_SPADES_SETTING(cg_playerNames, "2");
DEFINE_SPADES_SETTING(cg_playerNameX, "0");
DEFINE_SPADES_SETTING(cg_playerNameY, "0");
DEFINE_SPADES_SETTING(cg_hudBorderX, "16");
DEFINE_SPADES_SETTING(cg_hudBorderY, "16");
DEFINE_SPADES_SETTING(cg_netGraphSize, "128");
DEFINE_SPADES_SETTING(cg_dbgHitTestSize, "128");
DEFINE_SPADES_SETTING(cg_damageIndicators, "1");

SPADES_SETTING(cg_minimapSize);

namespace spades {
	namespace client {

		enum class ScreenshotFormat { JPG, TGA, PNG };

		namespace {
			ScreenshotFormat GetScreenshotFormat(std::string format) {
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
			std::string msg = sceneOnly ? "Sceneshot" : "Screenshot";

			try {
				auto name = ScreenShotPath();
				bmp->Save(name);

				msg += _Tr("Client", " saved: {0}", name);
				ShowAlert(msg, AlertType::Notice);

				Handle<IAudioChunk> c = audioDevice->RegisterSound("Sounds/Feedback/Screenshot.opus");
				audioDevice->PlayLocal(c.GetPointerOrNull(), AudioParam());
			} catch (const Exception& ex) {
				msg += _Tr("Client", " failed: ");
				msg += ex.GetShortMessage();
				ShowAlert(msg, AlertType::Error);
				SPLog("Screenshot failed: %s", ex.what());
			} catch (const std::exception& ex) {
				msg += _Tr("Client", " failed: ");
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
			Vector2 siz = {img->GetWidth(), img->GetHeight()};
			siz *= std::min(1.0F, sw / siz.x);
			siz *= std::min(1.0F, sh / siz.y);
			siz *= 1.0F - (scale * (scale * 0.25F));

			renderer->SetColorAlphaPremultiplied(MakeVector4(1, 1, 1, 1));
			renderer->DrawImage(img, AABB2((sw - siz.x) * 0.5F, (sh - siz.y) * 0.5F, siz.x, siz.y));
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
			IFont& font = fontManager->GetMediumFont();
			int now = (int)world->GetTime();
			auto str = _Tr("Client", "Playing for {0}m{1}s", ToString(now / 60), ToString(now % 60));
			auto size = font.Measure(str);
			auto pos = MakeVector2((renderer->ScreenWidth() - size.x) * 0.5F, 48.0F - size.y);
			font.DrawShadow(str, pos, 1.0F, MakeVector4(1, 1, 1, 1), MakeVector4(0, 0, 0, 0.5));
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
				renderer->DrawImage(img, v1, v2, v3, AABB2(0, 8.0F, size.x, size.y));
			}
		}

		void Client::DrawHurtScreenEffect() {
			SPADES_MARK_FUNCTION();

			float sw = renderer->ScreenWidth();
			float sh = renderer->ScreenHeight();

			Player& p = GetWorld()->GetLocalPlayer().value();

			float wTime = world->GetTime();
			const float fadeOutTime = 0.35F;
			if (wTime - lastHurtTime < fadeOutTime && wTime >= lastHurtTime) {
				float per = (wTime - lastHurtTime) / fadeOutTime;
				per = 1.0F - per;
				per *= 0.3F + (1.0F - p.GetHealth() / 100.0F) * 0.7F;
				per = std::min(per, 0.9F);
				per = 1.0F - per;
				renderer->MultiplyScreenColor({1, per, per});

				float a = (1.0F - per) * 0.1F;
				renderer->SetColorAlphaPremultiplied(MakeVector4(a, 0, 0, a));
				renderer->DrawImage(nullptr, AABB2(0, 0, sw, sh));
			}
		}

		Vector4 Client::GetPlayerColor(Player& p) {
			Vector4 playerColor = MakeVector4(1, 1, 1, 1);

			auto origin = lastSceneDef.viewOrigin;

			if (!map->CanSee(p.GetEye(), origin, FOG_DISTANCE))
				playerColor = ModifyColor(p.GetColor());
			if (int((origin - p.GetEye()).GetLength2D()) > FOG_DISTANCE)
				playerColor = MakeVector4(1, 0.75, 0, 1);

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
				auto playerNameStr = player.GetName();
				sprintf(buf, "%s", playerNameStr.c_str());
				if ((int)cg_playerNames == 1) {
					Vector3 diff = (origin - lastSceneDef.viewOrigin);
					if (diff.GetLength2D() <= FOG_DISTANCE)
						sprintf(buf, "%s [%.1f]", playerNameStr.c_str(), diff.GetLength());
				}

				IFont& font = fontManager->GetGuiFont();
				auto size = font.Measure(buf);
				pos.x -= size.x * 0.5F;
				pos.y -= size.y;

				renderer->SetColorAlphaPremultiplied(MakeVector4(0, 0, 0, 0.25));
				renderer->DrawFilledRect(pos.x - 2.0F, pos.y + 2.0F, pos.x + size.x + 2.0F,
				                         pos.y + size.y - 2.0F);

				font.DrawShadow(buf, pos, 1.0F, color, MakeVector4(0, 0, 0, 0.5));
			}
		}

		void Client::DrawHottrackedPlayerName() {
			SPADES_MARK_FUNCTION();

			Player& p = GetWorld()->GetLocalPlayer().value();
			if (p.IsSpectator())
				return;

			auto hottracked = HotTrackedPlayer();
			if (hottracked) {
				Player& player = std::get<0>(*hottracked);
				DrawPlayerName(player, MakeVector4(1, 1, 1, 1));
			}
		}

		void Client::DrawPUBOVL() {
			SPADES_MARK_FUNCTION();

			for (size_t i = 0; i < world->GetNumPlayerSlots(); i++) {
				auto maybePlayer = world->GetPlayer(i);
				if (!maybePlayer)
					continue;

				Player& p = maybePlayer.value();
				if (&p == world->GetLocalPlayer())
					continue;
				if (p.IsSpectator() || !p.IsAlive())
					continue;

				if (!p.GetFront().IsValid())
					continue; // exclude invisible players

				DrawPlayerName(p, GetPlayerColor(p));
			}
		}

		void Client::DrawDebugAim(float x, float y, float w, float h) {
			SPADES_MARK_FUNCTION();

			Player& p = GetCameraTargetPlayer();

			Weapon& weapon = p.GetWeapon();
			float spread = weapon.GetSpread();
			if (GetAimDownZoomScale() == 1)
				spread *= 2;

			float size = h * 0.5F;
			float fovY = tanf(lastSceneDef.fovY * 0.5F);

			AABB2 boundary(0, 0, 0, 0);
			boundary.min += spread / fovY * size;
			boundary.max -= spread / fovY * size;

			IntVector3 center;
			center.x = (int)(w * 0.5F);
			center.y = (int)size;

			IntVector3 p1 = center;
			IntVector3 p2 = center;

			p1.x += (int)floorf(boundary.min.x);
			p1.y += (int)floorf(boundary.min.y);
			p2.x += (int)ceilf(boundary.max.x);
			p2.y += (int)ceilf(boundary.max.y);

			renderer->SetColorAlphaPremultiplied(MakeVector4(0, 0, 0, 1));
			renderer->DrawOutlinedRect((float)p1.x, (float)p1.y, (float)p2.x, (float)p2.y);

			renderer->SetColorAlphaPremultiplied(MakeVector4(1, 1, 1, 1));
			renderer->DrawOutlinedRect(p1.x + 1.0F, p1.y + 1.0F, p2.x - 1.0F, p2.y - 1.0F);
		}

		void Client::DrawFirstPersonHUD(float x, float y, float w, float h) {
			SPADES_MARK_FUNCTION();

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
				renderer->DrawImage(img, (MakeVector2(w, h) - size) * 0.5F);
			}

			if (cg_debugAim && p.IsToolWeapon())
				DrawDebugAim(x, y, w, h);
		}

		void Client::DrawJoinedAlivePlayerHUD(float x, float y, float w, float h) {
			SPADES_MARK_FUNCTION();

			// Draw damage rings
			hurtRingView->Draw();

			Player& p = GetWorld()->GetLocalPlayer().value();

			Weapon& weap = p.GetWeapon();
			Handle<IImage> ammoIcon;
			float iw, ih, spacing = 1.0F;
			int clipNum, clipSize, stockNum;

			switch (p.GetTool()) {
				case Player::ToolSpade:
				case Player::ToolBlock:
					stockNum = p.GetNumBlocks();
					break;
				case Player::ToolGrenade:
					stockNum = p.GetNumGrenades();
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
						default: SPInvalidEnum("weap->GetWeaponType()", weap.GetWeaponType());
					}

					clipNum = weap.GetAmmo();
					clipSize = weap.GetClipSize();
					clipSize = std::max(clipSize, clipNum);

					for (int i = 0; i < clipSize; i++) {
						float ix = w - x - (float)(i + 1) * (iw + spacing);
						float iy = h - y - ih;

						renderer->SetColorAlphaPremultiplied((clipNum >= i + 1)
							? MakeVector4(1, 1, 1, 1) : MakeVector4(0.4F, 0.4F, 0.4F, 1));
						renderer->DrawImage(ammoIcon, AABB2(ix, iy, iw, ih));
					}

					stockNum = weap.GetStock();
				} break;
				default:
					ih = 0.0F;
					clipNum = clipSize = 0;
					SPInvalidEnum("p->GetTool()", p.GetTool());
			}

			float per = std::min((2.0F * clipNum) / (clipSize / 2), 1.0F);
			auto numberColor = MakeVector4(1, 1, per, 1);

			IFont& font = fontManager->GetHudFont();
			auto stockStr = ToString(stockNum);
			Vector2 size = font.Measure(stockStr);
			Vector2 pos = MakeVector2(w - x, h - y - ih) - size;
			font.DrawShadow(stockStr, pos, 1.0F, numberColor, MakeVector4(0, 0, 0, 0.5));

			// draw "press ... to reload"
			{
				std::string msg = "";

				switch (p.GetTool()) {
					case Player::ToolBlock:
						if (p.GetNumBlocks() == 0)
							msg = _Tr("Client", "Out of Blocks");
						break;
					case Player::ToolGrenade:
						if (p.GetNumGrenades() == 0)
							msg = _Tr("Client", "Out of Grenades");
						break;
					case Player::ToolWeapon: {
						if (weap.IsReloading() || p.IsAwaitingReloadCompletion())
							msg = _Tr("Client", "Reloading");
						else if (weap.GetAmmo() == 0 && weap.GetStock() == 0)
							msg = _Tr("Client", "Out of Ammo");
						else if (weap.GetStock() > 0 && weap.GetAmmo() < weap.GetClipSize() / 4)
							msg = _Tr("Client", "Press [{0}] to Reload", TrKey(cg_keyReloadWeapon));
					} break;
					default:; // no message
				}

				if (!msg.empty()) {
					IFont& font = fontManager->GetGuiFont();
					Vector2 size = font.Measure(msg);
					Vector2 pos = MakeVector2((w - size.x) * 0.5F, h * 2.0F / 3.0F);
					font.DrawShadow(msg, pos, 1.0F, MakeVector4(1, 1, 1, 1),
					                MakeVector4(0, 0, 0, 0.5));
				}
			}

			if (p.IsToolBlock())
				paletteView->Draw();

			DrawHealth(x, y, w, h);
		}

		void Client::DrawHitTestDebugger(float x, float y, float w, float h) {
			SPADES_MARK_FUNCTION();

			auto* debugger = world->GetHitTestDebugger();
			if (!debugger)
				return;

			auto bmp = debugger->GetBitmap();
			if (bmp)
				debugHitTestImage.Set(renderer->CreateImage(*bmp).GetPointerOrNull());

			if (debugHitTestImage) {
				float cfgWndSize = cg_dbgHitTestSize;
				Vector2 wndSize = {cfgWndSize, cfgWndSize};

				Vector2 zoomedSize = {512, 512};
				if (w < zoomedSize.x || h < zoomedSize.y)
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

				AABB2 outRect((w - wndSize.x) - 8.0F, (h - wndSize.y) - 68.0F - y, wndSize.x, wndSize.y);
				if (debugHitTestZoom) {
					outRect.min = MakeVector2((w - zoomedSize.x) * 0.5F, (h - zoomedSize.y) * 0.5F);
					outRect.max = MakeVector2((w + zoomedSize.x) * 0.5F, (h + zoomedSize.y) * 0.5F);
				}

				float alpha = debugHitTestZoom ? debugHitTestZoomState : 1.0F;
				renderer->SetColorAlphaPremultiplied(MakeVector4(alpha, alpha, alpha, alpha));
				renderer->DrawImage(debugHitTestImage, outRect, AABB2(128, 512 - 128, 256, 256 - 512)); // flip Y axis
				renderer->DrawOutlinedRect(outRect.min.x - 1, outRect.min.y - 1, outRect.max.x + 1, outRect.max.y + 1);
			}
		}

		void Client::DrawPlayerStats(float w, float h) {
			SPADES_MARK_FUNCTION();

			IFont& font = fontManager->GetSmallFont();

			float x = w - 8.0F;
			float y = cg_minimapSize;
			if (y < 32)
				y = 32;
			if (y > 256)
				y = 256;
			y += 32;

			auto addLine = [&](const char* format, ...) {
				char buf[256];
				va_list va;
				va_start(va, format);
				vsnprintf(buf, sizeof(buf), format, va);
				va_end(va);

				auto pos = MakeVector2(x, y);
				pos.x -= font.Measure(buf).x;
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

		void Client::DrawDamageIndicators(float x, float y, float w, float h) {
			SPADES_MARK_FUNCTION();

			for (const auto& damages : damageIndicators) {
				float fade = damages.fade;
				if (fade > 1.0F)
					fade = 1.0F;

				Vector3 posxyz;
				if (Project(damages.position, posxyz)) {
					Vector2 pos = {posxyz.x, posxyz.y};

					int damage = damages.damage;
					auto damageStr = ToString(damage);

					IFont& font = fontManager->GetGuiFont();
					Vector2 size = font.Measure(damageStr);
					pos.x -= size.x * 0.5F;
					pos.y -= size.y;

					float per = std::min((100 - damage) / 100.0F, 1.0F);
					font.DrawShadow(damageStr, pos, 1.0F, MakeVector4(1, per, per, fade),
					                MakeVector4(0, 0, 0, 0.25F * fade));
				}
			}
		}

		void Client::DrawDeadPlayerHUD(float x, float y, float w, float h) {
			SPADES_MARK_FUNCTION();

			Player& p = GetWorld()->GetLocalPlayer().value();

			std::string msg;
			int secs = (int)(p.GetRespawnTime() - world->GetTime());
			if (secs > 0) {
				static int lastCount = 0;
				if (lastCount != secs) {
					if (secs <= 3) {
						Handle<IAudioChunk> c =
						  (secs == 1) ? audioDevice->RegisterSound("Sounds/Feedback/Beep1.opus")
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
				IFont& font = fontManager->GetMediumFont();
				Vector2 size = font.Measure(msg);
				Vector2 pos = MakeVector2((w - size.x) * 0.5F, h / 3.0F);
				font.DrawShadow(msg, pos, 1.0F, MakeVector4(1, 1, 1, 1), MakeVector4(0, 0, 0, 0.5));
			}
		}

		void Client::DrawSpectateHUD(float x, float y, float w, float h) {
			SPADES_MARK_FUNCTION();

			IFont& font = fontManager->GetGuiFont();

			float textX = w - y;
			float textY = (float)cg_minimapSize + 64.0F;

			auto addLine = [&](const std::string& text) {
				auto pos = MakeVector2(textX, textY);
				pos.x -= font.Measure(text).x;
				textY += 20.0F;
				font.DrawShadow(text, pos, 1.0F, MakeVector4(1, 1, 1, 1),
				                MakeVector4(0, 0, 0, 0.5));
			};

			auto cameraMode = GetCameraMode();

			if (HasTargetPlayer(cameraMode)) {
				auto targetId = GetCameraTargetPlayerId();

				addLine(_Tr("Client", "Following {0} [#{1}]",
					  world->GetPlayerName(targetId), targetId));
			}

			textY += 10.0F;

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

			textY += 10.0F;

			if (GetWorld()->GetLocalPlayer()->IsSpectator() && !inGameLimbo)
				addLine(_Tr("Client", "[{0}] Choose a Team/Weapon", TrKey(cg_keyLimbo)));
		}

		void Client::DrawAlert(float w, float h) {
			SPADES_MARK_FUNCTION();

			IFont& font = fontManager->GetGuiFont();

			const float fadeOutTime = 1.0F;
			float fade = 1.0F - (time - alertDisappearTime) / fadeOutTime;
			fade = std::min(fade, 1.0F);
			if (fade <= 0.0F)
				return;

			float borderFade = 1.0F - (time - alertAppearTime) * 1.5F;
			borderFade = Clamp(borderFade, 0.0F, 1.0F);

			Handle<IImage> alertIcon = renderer->RegisterImage("Gfx/AlertIcon.png");

			Vector2 txtSiz = font.Measure(alertContents);
			Vector2 cntsSiz = txtSiz;
			cntsSiz.y = std::max(cntsSiz.y, 16.0F);

			if (alertType != AlertType::Notice)
				cntsSiz.x += 22.0F;

			// add margin
			const float margin = 8.0F;
			cntsSiz += margin * 2.0F;
			cntsSiz.x = floorf(cntsSiz.x);
			cntsSiz.y = floorf(cntsSiz.y);

			Vector2 pos = MakeVector2(w, h) - cntsSiz;
			pos *= MakeVector2(0.5F, 0.7F);
			pos.y += 40.0F;

			pos.x = floorf(pos.x);
			pos.y = floorf(pos.y);

			Vector4 color;
			switch (alertType) {
				case AlertType::Notice: color = Vector4(0, 0, 0, 1); break;
				case AlertType::Warning: color = Vector4(1, 1, 0, 1); break;
				case AlertType::Error: color = Vector4(1, 0, 0, 1); break;
				default: color = Vector4(0, 0, 0, 1); break;
			}
			Vector4 shadow = {0, 0, 0, 0.5F * fade};

			float bw = 1.0F;
			float bh = 6.0F;

			// draw background
			renderer->SetColorAlphaPremultiplied(shadow);
			renderer->DrawFilledRect(pos.x, pos.y + bh, pos.x + cntsSiz.x, pos.y + cntsSiz.y - bh);

			// draw border
			renderer->SetColorAlphaPremultiplied(color * fade * (1.0F - borderFade));
			renderer->DrawOutlinedRect(pos.x - bw, pos.y - bw + bh, pos.x + cntsSiz.x + bw,
			                           pos.y + cntsSiz.y + bw - bh);

			bw += 8.0F * (1.0F - borderFade);
			renderer->SetColorAlphaPremultiplied(color * borderFade);
			renderer->DrawOutlinedRect(pos.x - bw, pos.y - bw + bh, pos.x + cntsSiz.x + bw,
			                           pos.y + cntsSiz.y + bw - bh);

			// draw alert icon
			if (alertType != AlertType::Notice) {
				renderer->SetColorAlphaPremultiplied(color * fade);
				renderer->DrawImage(alertIcon, MakeVector2(pos.x + margin,
					pos.y + (cntsSiz.y - 16.0F) * 0.5F));
			}

			// draw text
			float x = pos.x + (cntsSiz.x - txtSiz.x) - margin;
			float y = pos.y + (cntsSiz.y - txtSiz.y) - margin - 1.0F;

			color = MakeVector4(1, 1, 1, 1) * fade;
			font.DrawShadow(alertContents, MakeVector2(x, y), 1.0F, color, shadow);
		}

		void Client::DrawHealth(float x, float y, float w, float h) {
			SPADES_MARK_FUNCTION();

			Player& p = GetWorld()->GetLocalPlayer().value();

			int hp = p.GetHealth();

			float per = std::min(hp / 100.0F, 1.0F);
			auto numberColor = MakeVector4(1, per, per, 1);

			IFont& font = fontManager->GetHudFont();
			auto healthStr = ToString(hp);
			Vector2 size = font.Measure(healthStr);
			Vector2 pos = MakeVector2(x, h - y);
			pos.y -= size.y;
			font.DrawShadow(healthStr, pos, 1.0F, numberColor, MakeVector4(0, 0, 0, 0.5));
		}

		void Client::Draw2DWithWorld() {
			SPADES_MARK_FUNCTION();

			for (const auto& ent : localEntities)
				ent->Render2D();

			float x = cg_hudBorderX;
			float y = cg_hudBorderY;
			float sw = renderer->ScreenWidth();
			float sh = renderer->ScreenHeight();

			// TODO: this should be done only for chat window https://github.com/yvt/openspades/issues/810
			bool shouldDraw = !cg_hideHud || AcceptsTextInput();

			// black fade
			float fade = Clamp((world->GetTime() - 1.0F) / 2.2F, 0.0F, 1.0F);
			if (fade < 1.0F) {
				renderer->SetColorAlphaPremultiplied(MakeVector4(0, 0, 0, 1.0F - fade));
				renderer->DrawImage(nullptr, AABB2(0, 0, sw, sh));
			}

			stmp::optional<Player&> p = GetWorld()->GetLocalPlayer();
			if (p) { // joined local player
				DrawHurtSprites();
				DrawHurtScreenEffect();

				if (cg_playerNames)
					DrawHottrackedPlayerName();
				if (cg_damageIndicators)
					DrawDamageIndicators(x, y, sw, sh);

				if (shouldDraw) {
					tcView->Draw();

					if (IsFirstPerson(GetCameraMode()))
						DrawFirstPersonHUD(x, y, sw, sh);

					if (!p->IsSpectator()) { // player is not spectator
						if (p->IsAlive()) {
							DrawJoinedAlivePlayerHUD(x, y, sw, sh);
						} else {
							DrawDeadPlayerHUD(x, y, sw, sh);
							DrawSpectateHUD(x, y, sw, sh);
						}

						if (cg_playerStats)
							DrawPlayerStats(sw, sh);
					} else {
						DrawSpectateHUD(x, y, sw, sh);
						DrawPUBOVL();
					}

					chatWindow->Draw();
					killfeedWindow->Draw();

					DrawAlert(sw, sh);

					if (!p->IsSpectator() && !p->IsToolBlock() || debugHitTestZoom)
						DrawHitTestDebugger(x, y, sw, sh);

					// map view should come in front
					if (largeMapView->IsZoomed())
						largeMapView->Draw();
					else
						mapView->Draw();
				}

				// --- end "player is there" render
			} else {
				// world exists, but no local player: not joined

				DrawAlert(sw, sh);
			}

			if (shouldDraw)
				centerMessageView->Draw();

			if (scoreboardVisible || !p) {
				DrawPlayingTime();
				scoreboard->Draw();
			}

			if (IsLimboViewActive())
				limbo->Draw();

			if (cg_stats)
				DrawStats(sw, sh);
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

			IFont& font = fontManager->GetGuiFont();

			auto str = net->GetStatusString();
			auto sz = font.Measure(str);

			Vector4 grayCol = {0.5, 0.5, 0.5, 1};
			Vector3 blueCol = {0, 0.5, 1};

			font.Draw(str, MakeVector2((sw - sz.x) * 0.5F, prgY - sz.y - 10.0F), 1.0F, grayCol);

			// background bar
			renderer->SetColorAlphaPremultiplied(grayCol * 0.5F);
			renderer->DrawImage(nullptr, AABB2(prgX, prgY, prgW, prgH));

			// Normal progress bar
			if (net->GetStatus() == NetClientStatusReceivingMap) {
				auto prg = mapReceivingProgressSmoothed;

				float w = prgW * prg;
				for (float x = 0; x < w; x++) {
					float tempperc = x / w;
					Vector3 color = Mix(blueCol * 0.25F, blueCol, tempperc);
					renderer->SetColorAlphaPremultiplied(MakeVector4(color));
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

			DrawAlert(sw, sh);
		}

		void Client::DrawStats(float w, float h) {
			SPADES_MARK_FUNCTION();

			char buf[256];
			std::string str;

			{
				auto fps = (int)fpsCounter.GetFps();
				if (fps > 0) {
					sprintf(buf, "%dfps, ", fps);
					str += buf;
				}
			}
			{
				// Display world updates per second
				auto ups = (int)upsCounter.GetFps();
				if (ups > 0) {
					sprintf(buf, "%dups, ", ups);
					str += buf;
				}
			}

			if (net) {
				auto ping = net->GetPing();
				auto upbps = (int)net->GetUplinkBps() / 1000;
				auto downbps = (int)net->GetDownlinkBps() / 1000;
				sprintf(buf, "ping: %dms, up/down: %d/%dkbps", ping, upbps, downbps);
				str += buf;
			}

			IFont& font = fontManager->GetGuiFont();

			Vector2 margin = {4.0F, 4.0F};

			auto size = font.Measure(str) + (margin * 2.0F);
			auto pos = (MakeVector2(w, h) - size) * MakeVector2(0.5F, 1.0F);
			pos.y += margin.y;

			auto shadow = MakeVector4(0, 0, 0, 0.5);

			renderer->SetColorAlphaPremultiplied(shadow);
			renderer->DrawFilledRect(pos.x, pos.y + margin.y, pos.x + size.x,
			                         pos.y + size.y - margin.y);
			renderer->SetColorAlphaPremultiplied(MakeVector4(0, 0, 0, 1));
			renderer->DrawOutlinedRect(pos.x, pos.y + margin.y, pos.x + size.x,
			                           pos.y + size.y - margin.y);

			font.DrawShadow(str, pos + margin, 1.0F, MakeVector4(1, 1, 1, 1), shadow);
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

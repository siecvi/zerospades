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

#include <cctype>

#include "ChatWindow.h"
#include "Client.h"
#include "IFont.h"
#include "IRenderer.h"
#include "World.h"
#include <Core/Debug.h>
#include <Core/Exception.h>
#include <Core/Math.h>
#include <Core/Settings.h>

DEFINE_SPADES_SETTING(cg_chatHeight, "30");
DEFINE_SPADES_SETTING(cg_killfeedHeight, "26");

SPADES_SETTING(cg_smallFont);

namespace spades {
	namespace client {

		ChatWindow::ChatWindow(Client* clin, IRenderer* r, IFont* fnt, bool killfeed)
		    : client(clin), renderer(r), font(fnt), killfeed(killfeed) {
			firstY = 0.0F;
			killImages.push_back(renderer->RegisterImage("Gfx/Killfeed/a-Rifle.png").GetPointerOrNull());
			killImages.push_back(renderer->RegisterImage("Gfx/Killfeed/b-SMG.png").GetPointerOrNull());
			killImages.push_back(renderer->RegisterImage("Gfx/Killfeed/c-Shotgun.png").GetPointerOrNull());
			killImages.push_back(renderer->RegisterImage("Gfx/Killfeed/d-Headshot.png").GetPointerOrNull());
			killImages.push_back(renderer->RegisterImage("Gfx/Killfeed/e-Melee.png").GetPointerOrNull());
			killImages.push_back(renderer->RegisterImage("Gfx/Killfeed/f-Grenade.png").GetPointerOrNull());
			killImages.push_back(renderer->RegisterImage("Gfx/Killfeed/g-Falling.png").GetPointerOrNull());
			killImages.push_back(renderer->RegisterImage("Gfx/Killfeed/h-Teamchange.png").GetPointerOrNull());
			killImages.push_back(renderer->RegisterImage("Gfx/Killfeed/i-Classchange.png").GetPointerOrNull());
			killImages.push_back(renderer->RegisterImage("Gfx/Killfeed/j-Airborne.png").GetPointerOrNull());
			killImages.push_back(renderer->RegisterImage("Gfx/Killfeed/k-Noscope.png").GetPointerOrNull());
		}
		ChatWindow::~ChatWindow() {}

		float ChatWindow::GetWidth() { return renderer->ScreenWidth() * 0.5F; }

		float ChatWindow::GetNormalHeight() {
			float prop = killfeed ? (float)cg_killfeedHeight : (float)cg_chatHeight;

			return renderer->ScreenHeight() * prop * 0.01F;
		}

		float ChatWindow::GetBufferHeight() {
			if (killfeed) {
				return GetNormalHeight();
			} else {
				// Take up the remaining height
				float prop = 100.0F - (float)cg_killfeedHeight;

				return renderer->ScreenHeight() * prop * 0.01F - 100.0F;
			}
		}

		float ChatWindow::GetLineHeight() { return cg_smallFont ? 12.0F : 20.0F; }

		static bool isWordChar(char c) { return isalnum(c) || c == '\''; }

		std::string ChatWindow::KillImage(int kt, int weapon) {
			std::string tmp = "xx";
			tmp[0] = MsgImage;
			switch (kt) {
				case KillTypeWeapon:
					switch (weapon) {
						case RIFLE_WEAPON:
						case SMG_WEAPON:
						case SHOTGUN_WEAPON:
							tmp[1] = 'a' + weapon; break;
						default: return "";
					}
					break;
				case KillTypeHeadshot:
				case KillTypeMelee:
				case KillTypeGrenade:
				case KillTypeFall:
				case KillTypeTeamChange:
				case KillTypeClassChange:
				case 7:
				case 8: tmp[1] = 'a' + 2 + kt; break;
				default: return "";
			}
			return tmp;
		}
		IImage* ChatWindow::GetKillImage(char index) {
			int real = index - 'a';
			if (real >= 0 && real < (int)killImages.size())
				return killImages[real];
			return NULL;
		}

		void ChatWindow::AddMessage(const std::string& msg) {
			SPADES_MARK_FUNCTION();

			float lh = GetLineHeight();
			entries.push_front(ChatEntry(msg, lh, 15.0F));

			firstY -= lh;
		}

		std::string ChatWindow::ColoredMessage(const std::string& msg, char c) {
			SPADES_MARK_FUNCTION_DEBUG();
			std::string s;
			s += c;
			s += msg;
			s += MsgColorRestore;
			return s;
		}

		std::string ChatWindow::TeamColorMessage(const std::string& msg, int team) {
			SPADES_MARK_FUNCTION_DEBUG();
			switch (team) {
				case 0: return ColoredMessage(msg, MsgColorTeam1);
				case 1: return ColoredMessage(msg, MsgColorTeam2);
				case 2: return ColoredMessage(msg, MsgColorTeam3);
				default: return msg;
			}
		}

		Vector4 ChatWindow::GetColor(char c) {
			World* w = client ? client->GetWorld() : NULL;
			switch (c) {
				case MsgColorTeam1:
					return w ? ConvertColorRGBA(w->GetTeamColor(0)) : MakeVector4(0, 1, 0, 1);
				case MsgColorTeam2:
					return w ? ConvertColorRGBA(w->GetTeamColor(1)) : MakeVector4(0, 0, 1, 1);
				case MsgColorTeam3:
					return w ? ConvertColorRGBA(w->GetTeamColor(2)) : MakeVector4(1, 1, 0, 1);
				case MsgColorRed: return MakeVector4(1, 0.25, 0.25, 1);
				case MsgColorGreen: return MakeVector4(0, 1, 0, 1);
				case MsgColorYellow: return MakeVector4(1, 1, 0.5, 1);
				case MsgColorGray: return MakeVector4(0.8F, 0.8F, 0.8F, 1);
				default: return MakeVector4(1, 1, 1, 1);
			}
		}

		void ChatWindow::Update(float dt) {
			if (firstY < 0.0F) {
				firstY += dt * std::max(100.0F, -firstY);
				if (firstY > 0.0F)
					firstY = 0.0F;
			}

			float normalHeight = GetNormalHeight();
			float bufferHeight = GetBufferHeight();
			float y = firstY;

			for (auto it = entries.begin(); it != entries.end();) {
				ChatEntry& ent = *it;
				if (y + ent.height > bufferHeight) {
					ent.bufferFade -= dt * 4.0F;
					if (ent.bufferFade < 0.0F) {
						// evict from the buffer
						std::list<ChatEntry>::iterator tmp = it++;
						entries.erase(tmp);
						continue;
					}
				}

				if (y + ent.height > normalHeight) {
					ent.fade = std::max(ent.fade - dt * 4.0F, 0.0F);
				} else if (y + ent.height > 0.0F) {
					ent.fade = std::min(ent.fade + dt * 4.0F, 1.0F);
					ent.bufferFade = std::min(ent.bufferFade + dt * 4.0F, 1.0F);
				}

				ent.timeFade -= dt;
				if (ent.timeFade < 0.0F)
					ent.timeFade = 0.0F;

				y += ent.height;
				++it;
			}
		}

		void ChatWindow::Draw() {
			SPADES_MARK_FUNCTION();

			float winW = GetWidth();
			float winH = expanded ? GetBufferHeight() : GetNormalHeight();
			float winX = 8.0F;
			float winY = killfeed ? 8.0F : renderer->ScreenHeight() - winH - 64.0F;
			float lh = GetLineHeight();
			float y = firstY;

			Vector4 shadowColor = { 0, 0, 0, 0.8F };
			Vector4 brightShadowColor = { 1, 1, 1, 0.8F };

			std::string ch = "aaaaaa"; // let's not make a new object for each character.
			// note: UTF-8's longest character is 6 bytes

			// Draw a box behind text when expanded
			if (expanded) {
				float x1 = winX - 4.0F;
				float y1 = winY + y;
				float x2 = winW + 16.0F;
				float y2 = winH + 16.0F - y;

				renderer->SetColorAlphaPremultiplied(MakeVector4(0, 0, 0, 0.5F));
				renderer->DrawFilledRect(x1 + 1, y1 + 1, x2 + x1 - 1, y2 + y1 - 1);
				renderer->SetColorAlphaPremultiplied(MakeVector4(0, 0, 0, 0.75F));
				renderer->DrawOutlinedRect(x1, y1, x2 + x1, y2 + y1);
			}

			std::list<ChatEntry>::iterator it;
			for (it = entries.begin(); it != entries.end(); ++it) {
				ChatEntry& ent = *it;

				const auto& msg = ent.msg;
				Vector4 color = GetColor(MsgColorRestore);

				float tx = 0.0F, ty = y;
				float fade = ent.fade;

				if (expanded) { // Display out-dated messages
					fade = ent.bufferFade;
				} else {
					if (ent.timeFade < 1.0F)
						fade *= ent.timeFade;
				}

				if (fade < 0.01F)
					goto endDrawLine; // Skip rendering invisible messages

				brightShadowColor.w = shadowColor.w = (killfeed ? 0.4F : 0.8F) * fade;
				color.w = fade;

				for (size_t i = 0; i < msg.size(); i++) {
					if (msg[i] == '\r' || msg[i] == '\n') {
						tx = 0.0F;
						ty += lh;
					} else if (msg[i] <= MsgColorMax && msg[i] >= 1) {
						if (msg[i] == MsgImage) {
							IImage* img = NULL;
							if (i + 1 < msg.size() && (img = GetKillImage(msg[i + 1]))) {
								Vector4 colorP = color;
								colorP.x *= colorP.w;
								colorP.y *= colorP.w;
								colorP.z *= colorP.w;
								renderer->SetColorAlphaPremultiplied(colorP);
								renderer->DrawImage(
								  img, MakeVector2(floorf(tx + winX), floorf(ty + winY)));
								tx += img->GetWidth();
								++i;
							}
						} else {
							color = GetColor(msg[i]);
							color.w = fade;
						}
					} else {
						size_t ln = 0;
						GetCodePointFromUTF8String(msg, i, &ln);
						ch.resize(ln);
						for (size_t k = 0; k < ln; k++)
							ch[k] = msg[i + k];
						i += ln - 1;

						Vector2 pos = MakeVector2(tx + winX, ty + winY);
						float luminosity = color.x + color.y + color.z;
						Vector4 shadow = (luminosity > 0.9F) ? shadowColor : brightShadowColor;

						if (killfeed) {
							font->DrawShadow(ch, pos + MakeVector2(1, 1), 1.0F, shadow, shadow);
							font->Draw(ch, pos, 1.0F, color);
						} else {
							font->DrawShadow(ch, pos, 1.0F, color, shadow);
						}

						tx += font->Measure(ch).x;
					}
				}

			endDrawLine:
				y += ent.height;
			}
		}
	} // namespace client
} // namespace spades
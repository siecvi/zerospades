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

		float ChatWindow::GetLineHeight() { return cg_smallFont ? 16.0F : 20.0F; }

		static bool isWordChar(char c) { return isalnum(c) || c == '\''; }

		void ChatWindow::AddMessage(const std::string& msg) {
			SPADES_MARK_FUNCTION();

			// get visible message string
			std::string str;
			float x = 0.0F, maxW = GetWidth();
			float lh = GetLineHeight(), h = lh;
			size_t wordStart = std::string::npos;
			size_t wordStartOutPos = 0;

			for (size_t i = 0; i < msg.size(); i++) {
				if (msg[i] > MsgColorMax && msg[i] != 13 && msg[i] != 10) {
					if (isWordChar(msg[i])) {
						if (wordStart == std::string::npos) {
							wordStart = msg.size();
							wordStartOutPos = str.size();
						}
					} else {
						wordStart = std::string::npos;
					}

					float w = font->Measure(std::string(&msg[i], 1)).x;
					if (x + w > maxW) {
						if (wordStart != std::string::npos && wordStart != str.size()) {
							// adding a part of word.
							// do word wrapping
							std::string s = msg.substr(wordStart, i - wordStart + 1);
							float nw = font->Measure(s).x;
							if (nw <= maxW) { // word wrap succeeds
								w = nw;
								x = w;
								h += lh;
								str.insert(wordStartOutPos, "\n");

								goto didWordWrap;
							}
						}
						x = 0;
						h += lh;
						str += 13;
					}
					x += w;
					str += msg[i];
				didWordWrap:;
				} else if (msg[i] == 13 || msg[i] == 10) {
					x = 0;
					h += lh;
					str += 13;
				} else {
					str += msg[i];
				}
			}

			entries.push_front(ChatEntry(msg, h, 15.0F));

			firstY -= h;
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
					return w ? ConvertColorRGBA(w->GetTeam(0).color) : MakeVector4(0, 1, 0, 1);
				case MsgColorTeam2:
					return w ? ConvertColorRGBA(w->GetTeam(1).color) : MakeVector4(0, 0, 1, 1);
				case MsgColorTeam3:
					return w ? ConvertColorRGBA(w->GetTeam(2).color) : MakeVector4(1, 1, 0, 1);
				case MsgColorRed: return MakeVector4(1, 0, 0, 1);
				case MsgColorGreen: return MakeVector4(0, 1, 0, 1);
				case MsgColorGray: return MakeVector4(0.5, 0.5, 0.5, 1);
				case MsgColorSysInfo: return MakeVector4(1, 1, 0.5, 1);
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
			float winX = 4.0F;
			float winY = killfeed ? 8.0F : renderer->ScreenHeight() - winH - 64.0F;
			float linH = GetLineHeight();
			float y = firstY;

			Vector4 shadowColor = { 0, 0, 0, 0.8F };
			Vector4 brightShadowColor = { 1, 1, 1, 0.8F };

			std::string ch = "aaaaaa"; // let's not make a new object for each character.
			// note: UTF-8's longest character is 6 bytes

			if (expanded) { // Draw a box behind text when expanded
				renderer->SetColorAlphaPremultiplied(MakeVector4(0, 0, 0, 0.5F));
				renderer->DrawImage(nullptr, AABB2(2.0F, winY + y, winW + 16.0F, winH + 16.0F - y));
			}

			for (const auto& ent : entries) {
				const auto& msg = ent.msg;

				float fade = ent.fade;
				if (expanded) { // Display out-dated messages
					fade = ent.bufferFade;
				} else {
					if (ent.timeFade < 1.0F)
						fade *= ent.timeFade;
				}

				if (fade < 0.01F)
					goto endDrawLine; // Skip rendering invisible messages

				Vector4 color = GetColor(MsgColorRestore);
				color.w = fade;
				brightShadowColor.w = shadowColor.w = 0.8F * fade;

				float tx = 0.0F, ty = y;
				for (size_t i = 0; i < msg.size(); i++) {
					if (msg[i] == 13 || msg[i] == 10) {
						tx = 0.0F;
						ty += linH;
					} else if (msg[i] <= MsgColorMax && msg[i] >= 1) {
						color = GetColor(msg[i]);
						color.w = fade;
					} else {
						size_t ln = 0;
						GetCodePointFromUTF8String(msg, i, &ln);
						ch.resize(ln);
						for (size_t k = 0; k < ln; k++)
							ch[k] = msg[i + k];
						i += ln - 1;

						float luminosity = color.x + color.y + color.z;
						font->DrawShadow(ch, MakeVector2(tx + winX, ty + winY), 1.0F,
							color, (luminosity > 0.9F) ? shadowColor : brightShadowColor);
						tx += font->Measure(ch).x;
					}
				}

			endDrawLine:
				y += ent.height;
			}
		}
	} // namespace client
} // namespace spades
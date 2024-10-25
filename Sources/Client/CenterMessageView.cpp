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

#include "CenterMessageView.h"
#include "Client.h"
#include "IFont.h"
#include "IRenderer.h"
#include <Core/Debug.h>
#include <Core/Settings.h>

DEFINE_SPADES_SETTING(cg_centerMessageSmallFont, "0");

namespace spades {
	namespace client {
		CenterMessageView::CenterMessageView(Client* client, IFont* font)
		    : renderer(client->GetRenderer()), font(font) {
			SPADES_MARK_FUNCTION();

			for (int i = 0; i < 2; i++)
				lineUsing.push_back(false);
		}
		CenterMessageView::~CenterMessageView() {}

		int CenterMessageView::GetFreeLine() {
			for (size_t i = 0; i < lineUsing.size(); i++) {
				if (!lineUsing[i])
					return (int)i;
			}

			// remove oldest entry
			int l = entries.front().line;
			entries.pop_front();
			return l;
		}

		float CenterMessageView::GetLineHeight() {
			return cg_centerMessageSmallFont ? 24.0F : 34.0F;
		}

		void CenterMessageView::AddMessage(const std::string& msg) {
			SPADES_MARK_FUNCTION();

			Entry entry;
			entry.msg = msg;
			entry.fade = 10.0F;
			entry.line = GetFreeLine();
			lineUsing[entry.line] = true;
			entries.push_back(entry);
		}

		void CenterMessageView::Update(float dt) {
			SPADES_MARK_FUNCTION();

			for (auto it = entries.begin(); it != entries.end();) {
				Entry& ent = *it;
				ent.fade -= dt;
				if (ent.fade < 0) {
					lineUsing[ent.line] = false;
					std::list<Entry>::iterator tmp = it++;
					entries.erase(tmp);
					continue;
				}
				++it;
			}
		}

		void CenterMessageView::Draw() {
			SPADES_MARK_FUNCTION();

			float sw = renderer.ScreenWidth();
			float lh = GetLineHeight() + 4.0F;

			for (const auto& ent : entries) {
				const auto& msg = ent.msg;

				float fade = ent.fade;
				if (fade > 1.0F)
					fade = 1.0F;

				Vector2 size = font->Measure(msg);

				float x = (sw - size.x) * 0.5F;
				float y = 100.0F + lh * (float)ent.line;

				Vector4 shadow = {0, 0, 0, fade * 0.5F};
				Vector4 color = {1, 1, 1, fade};

				font->DrawShadow(msg, MakeVector2(x, y), 1.0F, color, shadow);
			}
		}
	} // namespace client
} // namespace spades
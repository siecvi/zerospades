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

#include "HurtRingView.h"
#include "Client.h"
#include "IImage.h"
#include "IRenderer.h"
#include "Player.h"
#include "World.h"

#include <Core/Debug.h>

namespace spades {
	namespace client {
		HurtRingView::HurtRingView(Client* cli) : client(cli), renderer(cli->GetRenderer()) {
			SPADES_MARK_FUNCTION();

			image = renderer.RegisterImage("Gfx/HurtRing.png");
		}

		HurtRingView::~HurtRingView() {}

		void HurtRingView::ClearAll() { items.clear(); }

		void HurtRingView::Add(spades::Vector3 dir) {
			SPADES_MARK_FUNCTION();

			Item item;
			item.dir = dir;
			item.fade = 1.0F;
			items.push_back(item);
		}

		void HurtRingView::Update(float dt) {
			SPADES_MARK_FUNCTION();

			std::vector<std::list<Item>::iterator> its;
			for (auto it = items.begin(); it != items.end(); it++) {
				Item& ent = *it;
				ent.fade -= dt;
				if (ent.fade < 0)
					its.push_back(it);
			}
			for (const auto& it : its)
				items.erase(it);
		}

		void HurtRingView::Draw() {
			SPADES_MARK_FUNCTION();

			World* w = client->GetWorld();
			if (!w) {
				ClearAll();
				return;
			}

			auto p = w->GetLocalPlayer();
			if (!p || !p->IsAlive()) {
				ClearAll();
				return;
			}

			Vector3 o = p->GetFront2D();

			float sw = renderer.ScreenWidth();
			float sh = renderer.ScreenHeight();
			float hurtRingSize = sh * 0.3F;
			float cx = sw * 0.5F;
			float cy = sh * 0.5F;
			static const float coords[][2] = {{-1, 1}, {1, 1}, {-1, 0}};
			const AABB2 inRect{0.0F, 0.0F, image->GetWidth(), image->GetHeight()};

			for (const auto& item : items) {
				float fade = item.fade * 2.0F;
				if (fade > 1.0F)
					fade = 1.0F;

				renderer.SetColorAlphaPremultiplied(MakeVector4(fade, fade, fade, 0));

				Vector3 dir = -item.dir;
				float c = dir.x * o.x + dir.y * o.y;
				float s = dir.y * o.x - dir.x * o.y;

				Vector2 vt[3];
				for (int i = 0; i < 3; i++) {
					vt[i] = MakeVector2(
						coords[i][0] * c - coords[i][1] * s,
						coords[i][0] * s + coords[i][1] * c
					);
					vt[i] = vt[i] * hurtRingSize + MakeVector2(cx, cy);
				}

				renderer.DrawImage(image, vt[0], vt[1], vt[2], inRect);
			}
		}
	} // namespace client
} // namespace spades
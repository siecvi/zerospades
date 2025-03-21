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

#include <Core/Settings.h>

#include "Client.h"
#include "IImage.h"
#include "IRenderer.h"
#include "NetClient.h"
#include "PaletteView.h"
#include "Player.h"
#include "World.h"

#define PALETTE_SIZE 16

DEFINE_SPADES_SETTING(cg_keyPaletteLeft, "Left");
DEFINE_SPADES_SETTING(cg_keyPaletteRight, "Right");
DEFINE_SPADES_SETTING(cg_keyPaletteUp, "Up");
DEFINE_SPADES_SETTING(cg_keyPaletteDown, "Down");
DEFINE_SPADES_SETTING(cg_hudPaletteSize, "128");

namespace spades {
	namespace client {
		static IntVector3 SanitizeCol(IntVector3 col) {
			if (col.x < 0) col.x = 0;
			if (col.y < 0) col.y = 0;
			if (col.z < 0) col.z = 0;
			return col;
		}

		PaletteView::PaletteView(Client* client) : client(client), renderer(client->GetRenderer()) {
			IntVector3 cols[PALETTE_SIZE] = { // Extended palette to 256 colors
			  {128, 128, 128}, {256, 0, 0},     {256, 128, 0},   {256, 256, 0},
			  {256, 256, 128}, {128, 256, 0},   {0, 256, 0},     {0, 256, 128},
			  {0, 256, 256},   {128, 256, 256}, {0, 128, 256},   {0, 0, 256},
			  {128, 0, 256},   {256, 0, 256},   {256, 128, 256}, {256, 0, 128}
			};

			auto def = MakeIntVector3(256, 256, 256);
			for (const auto& col : cols) {
				for (int j = 1; j < PALETTE_SIZE; j += 2)
					colors.push_back(SanitizeCol((col * j) / PALETTE_SIZE - 1));
				for (int j = 1; j < PALETTE_SIZE; j += 2)
					colors.push_back(col + (((def - col) * j) / PALETTE_SIZE - 1));
			}

			time = 0.0F;
			defaultColor = 3;
		}

		PaletteView::~PaletteView() {}

		int PaletteView::GetSelectedIndex() {
			World* w = client->GetWorld();
			if (!w)
				return -1;
			stmp::optional<Player&> p = w->GetLocalPlayer();
			if (!p)
				return -1;

			IntVector3 col = p->GetBlockColor();
			for (int i = 0; i < (int)colors.size(); i++) {
				if (col == colors[i])
					return i;
			}
			return -1;
		}

		int PaletteView::GetSelectedOrDefaultIndex() {
			int c = GetSelectedIndex();
			if (c == -1)
				return defaultColor;
			else
				return c;
		}

		bool PaletteView::KeyInput(const std::string& name, bool down) {
			if (EqualsIgnoringCase(name, cg_keyPaletteLeft)) {
				paletteInput.left = down;
				return true;
			} else if (EqualsIgnoringCase(name, cg_keyPaletteRight)) {
				paletteInput.right = down;
				return true;
			} else if (EqualsIgnoringCase(name, cg_keyPaletteUp)) {
				paletteInput.up = down;
				return true;
			} else if (EqualsIgnoringCase(name, cg_keyPaletteDown)) {
				paletteInput.down = down;
				return true;
			}
			return false;
		}

		void PaletteView::Update(float dt) {
			time += dt;

			if (time < 0.1F)
				return;

			if (paletteInput.left || paletteInput.right || paletteInput.up || paletteInput.down) {
				int c = GetSelectedOrDefaultIndex();
				int cols = static_cast<int>(colors.size());

				bool changed = false;

				// handle horizontal navigation
				if (paletteInput.left || paletteInput.right) {
					if (paletteInput.left) {
						if (c == 0)
							c = cols - 1;
						else
							c--;
					} else if (paletteInput.right) {
						if (c == cols - 1)
							c = 0;
						else
							c++;
					}

					changed = true;
				}

				// handle vertical navigation
				if (paletteInput.up || paletteInput.down) {
					if (paletteInput.up) {
						if (c < PALETTE_SIZE)
							c += cols - PALETTE_SIZE;
						else
							c -= PALETTE_SIZE;
					} else if (paletteInput.down) {
						if (c >= cols - PALETTE_SIZE)
							c -= cols - PALETTE_SIZE;
						else
							c += PALETTE_SIZE;
					}

					changed = true;
				}

				// apply changes if any
				if (changed) {
					client->SetBlockColor(colors[c]);
					time = 0.0F; // reset
				}
			}
		}

		void PaletteView::Draw() {
			float sw = renderer.ScreenWidth();
			float sh = renderer.ScreenHeight();

			float wndSize = cg_hudPaletteSize;
			float rectSize = wndSize / PALETTE_SIZE;

			float winX = (sw - wndSize) - 8.0F;
			float winY = (sh - wndSize) - 64.0F;

			int sel = GetSelectedIndex();
			for (size_t phase = 0; phase < 2; phase++) {
				for (size_t i = 0; i < colors.size(); i++) {
					if ((sel == i) != (phase == 1))
						continue;

					int row = static_cast<int>(i / PALETTE_SIZE);
					int col = static_cast<int>(i % PALETTE_SIZE);

					float x = winX + rectSize * col;
					float y = winY + rectSize * row;
					float w = x + rectSize;
					float h = y + rectSize;

					renderer.SetColorAlphaPremultiplied(ConvertColorRGBA(colors[i]));
					renderer.DrawFilledRect(x + 1, y + 1, w - 1, h - 1);

					if (sel == i) {
						float p = float((int(client->GetTime() * 4.0F)) & 1);
						renderer.SetColorAlphaPremultiplied(MakeVector4(p, p, p, 1));
						renderer.DrawOutlinedRect(x, y, w, h);
					}
				}
			}
		}
	} // namespace client
} // namespace spades
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

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "CTFGameMode.h"
#include "Client.h"
#include "Fonts.h"
#include "IFont.h"
#include "IImage.h"
#include "IRenderer.h"
#include "MapView.h"
#include "NetClient.h"
#include "Player.h"
#include "ScoreboardView.h"
#include "TCGameMode.h"
#include "World.h"
#include <Core/Debug.h>
#include <Core/Settings.h>
#include <Core/Strings.h>

SPADES_SETTING(cg_minimapPlayerColor);

namespace spades {
	namespace client {

		static const Vector4 white = {1, 1, 1, 1};
		static const Vector4 spectatorIdColor = {210.0F / 255, 210.0F / 255, 210.0F / 255, 1}; // Grey
		static const Vector4 spectatorTextColor = {220.0F / 255, 220.0F / 255, 0, 1}; // Goldish yellow
		static const auto spectatorTeamId = 255;       // Spectators have a team id of 255

		ScoreboardView::ScoreboardView(Client* client)
		    : client(client), renderer(client->GetRenderer()) {
			SPADES_MARK_FUNCTION();
			world = nullptr;
			tc = nullptr;
			ctf = nullptr;

			// Use GUI font if spectator string has special chars
			auto spectatorString = _TrN("Client", "Spectator{1}", "Spectators{1}", "", "");
			auto hasSpecialChar =
			  std::find_if(spectatorString.begin(), spectatorString.end(), [](char ch) {
				  return !(isalnum(static_cast<unsigned char>(ch)) || ch == '_');
			  }) != spectatorString.end();

			spectatorFont = hasSpecialChar ? client->fontManager->GetMediumFont()
			                               : client->fontManager->GetSquareDesignFont();
		}

		ScoreboardView::~ScoreboardView() {}

		int ScoreboardView::GetTeamScore(int team) const {
			if (ctf) {
				return ctf->GetTeam(team).score;
			} else if (tc) {
				int cnt = tc->GetNumTerritories();
				int num = 0;
				for (int i = 0; i < cnt; i++) {
					if (tc->GetTerritory(i).ownerTeamId == team)
						num++;
				}
				return num;
			} else {
				return 0;
			}
		}

		Vector4 ScoreboardView::GetTeamColor(int team) {
			return MakeVector4(world->GetTeam(team).color) / 255.0F;
		}

		void ScoreboardView::Draw() {
			SPADES_MARK_FUNCTION();

			world = client->GetWorld();
			if (!world)
				return; // no world

			// TODO: `ctf` and `tc` are only valid throughout the method call's
			//       duration. Move them to a new context type
			auto mode = world->GetMode();
			ctf = IGameMode::m_CTF == mode->ModeType()
			        ? dynamic_cast<CTFGameMode*>(mode.get_pointer())
			        : NULL;
			tc = IGameMode::m_TC == mode->ModeType()
				? dynamic_cast<TCGameMode*>(mode.get_pointer())
				: NULL;

			Handle<IImage> img;
			IFont& font = client->fontManager->GetSquareDesignFont();
			Vector2 pos, size;
			std::string str;

			float sw = renderer.ScreenWidth();
			float sh = renderer.ScreenHeight();

			float spacingW = 8.0F;
			float contentsWidth = sw + spacingW;
			float maxContentsWidth = 800.0F + spacingW;
			if (contentsWidth >= maxContentsWidth)
				contentsWidth = maxContentsWidth;

			float spacingH = 156.0F;
			float contentsH = sh - spacingH;
			float maxContentsH = 600.0F - spacingH;
			if (contentsH >= maxContentsH)
				contentsH = maxContentsH;

			float teamBarY = (sh - contentsH) * 0.5F;
			float teamBarH = 60.0F;
			float contentsLeft = (sw - contentsWidth) * 0.5F;
			float contentsRight = contentsLeft + contentsWidth;
			float playersHeight = 300.0F - teamBarH;
			float spectatorsH = 78.0F;
			float playersTop = teamBarY + teamBarH;
			float playersBottom = playersTop + playersHeight;

			// draw shadow
			img = renderer.RegisterImage("Gfx/Scoreboard/TopShadow.tga");
			size.y = 32.0F;
			renderer.SetColorAlphaPremultiplied(MakeVector4(0, 0, 0, 0.2F));
			renderer.DrawImage(img, AABB2(0, teamBarY - size.y, sw, size.y));
			renderer.SetColorAlphaPremultiplied(MakeVector4(0, 0, 0, 0.2F));
			renderer.DrawImage(img, AABB2(0, playersBottom + size.y, sw, -size.y));

			// draw team bar
			renderer.SetColorAlphaPremultiplied(AdjustColor(GetTeamColor(0), 0.8F, 0.3F));
			renderer.DrawImage(nullptr, AABB2(0, teamBarY, sw * 0.5F, teamBarH));
			renderer.SetColorAlphaPremultiplied(AdjustColor(GetTeamColor(1), 0.8F, 0.3F));
			renderer.DrawImage(nullptr, AABB2(sw * 0.5F, teamBarY, sw * 0.5F, teamBarH));

			img = renderer.RegisterImage("Gfx/Scoreboard/Grunt.png");
			size.x = 120.0F;
			size.y = 60.0F;
			renderer.DrawImage(img, AABB2(contentsLeft, teamBarY + teamBarH - size.y, size.x, size.y));
			renderer.DrawImage(img, AABB2(contentsRight, teamBarY + teamBarH - size.y, -size.x, size.y));

			str = world->GetTeam(0).name;
			pos.x = contentsLeft + 110.0F;
			pos.y = teamBarY + 5.0F;
			font.Draw(str, pos + MakeVector2(0, 2), 1.0F, MakeVector4(0, 0, 0, 0.5));
			font.Draw(str, pos, 1.0F, white);

			str = world->GetTeam(1).name;
			size = font.Measure(str);
			pos.x = contentsRight - 110.0F - size.x;
			pos.y = teamBarY + 5.0F;
			font.Draw(str, pos + MakeVector2(0, 2), 1.0F, MakeVector4(0, 0, 0, 0.5));
			font.Draw(str, pos, 1.0F, white);

			// draw scores
			int capLimit;
			if (ctf)
				capLimit = ctf->GetCaptureLimit();
			else if (tc)
				capLimit = tc->GetNumTerritories();
			else
				capLimit = -1;

			if (capLimit != -1) {
				str = Format("{0}-{1}", GetTeamScore(0), capLimit);
				pos.x = sw * 0.5F - font.Measure(str).x - 15.0F;
				pos.y = teamBarY + 5.0F;
				font.Draw(str, pos, 1.0F, Vector4(1.0F, 1.0F, 1.0F, 0.5F));

				str = Format("{0}-{1}", GetTeamScore(1), capLimit);
				pos.x = sw * 0.5F + 15.0F;
				pos.y = teamBarY + 5.0F;
				font.Draw(str, pos, 1.0F, Vector4(1.0F, 1.0F, 1.0F, 0.5F));
			}

			// players background
			auto areSpectatorsPr = AreSpectatorsPresent();
			img = renderer.RegisterImage("Gfx/Scoreboard/PlayersBg.png");
			renderer.SetColorAlphaPremultiplied(MakeVector4(0, 0, 0, 1.f));
			renderer.DrawImage(img, AABB2(0, playersTop, sw,
				playersHeight + (areSpectatorsPr ? spectatorsH : 0)));

			// draw players
			DrawPlayers(0, contentsLeft, playersTop, (contentsRight - contentsLeft) * 0.5F, playersHeight);
			DrawPlayers(1, sw * 0.5F, playersTop, (contentsRight - contentsLeft) * 0.5F, playersHeight);
			if (areSpectatorsPr)
				DrawSpectators(playersBottom, sw * 0.5F);
		}

		struct ScoreboardEntry {
			int id;
			int score;
			std::string name;
			bool alive;
			bool operator<(const ScoreboardEntry& ent) const { return score > ent.score; }
		};

		extern int palette[32][3];

		void ScoreboardView::DrawPlayers(int team, float left, float top, float width, float height) {
			IFont& font = client->fontManager->GetGuiFont();
			float rowHeight = 24.0F;
			char buf[256];
			Vector2 size;
			int maxRows = (int)floorf(height / rowHeight);
			int numPlayers = 0;
			int cols;

			std::vector<ScoreboardEntry> entries;
			for (size_t i = 0; i < world->GetNumPlayerSlots(); i++) {
				auto maybePlayer = world->GetPlayer(i);
				if (!maybePlayer)
					continue;
				Player& player = maybePlayer.value();
				if (player.GetTeamId() != team)
					continue;

				ScoreboardEntry ent;
				ent.name = player.GetName();
				ent.score = world->GetPlayerScore(i);
				ent.alive = player.IsAlive();
				ent.id = i;
				entries.push_back(ent);

				numPlayers++;
			}

			std::sort(entries.begin(), entries.end());

			cols = (numPlayers + maxRows - 1) / maxRows;
			if (cols == 0)
				cols = 1;
			maxRows = (numPlayers + cols - 1) / cols;

			int row = 0, col = 0;
			float colWidth = (float)width / (float)cols;

			for (int i = 0; i < numPlayers; i++) {
				ScoreboardEntry& ent = entries[i];

				float rowY = top + 6.0F + row * rowHeight;
				float colX = left + colWidth * (float)col;

				sprintf(buf, "#%d", ent.id); // FIXME: 1-base?
				size = font.Measure(buf);

				if (cg_minimapPlayerColor) {
					IntVector3 colorplayer = MakeIntVector3(palette[ent.id][0], palette[ent.id][1], palette[ent.id][2]);
					Vector4 colorplayerF = ModifyColor(colorplayer) * 1.0F;
					font.Draw(buf, MakeVector2(colX + 35.0F - size.x, rowY), 1.0F, colorplayerF);
				} else {
					font.Draw(buf, MakeVector2(colX + 35.0F - size.x, rowY), 1.0F, white);
				}

				Vector4 color = ent.alive ? white : MakeVector4(0.5, 0.5, 0.5, 1);
				if (stmp::make_optional(ent.id) == world->GetLocalPlayerIndex())
					color = GetTeamColor(team);

				font.Draw(ent.name, MakeVector2(colX + 45.0F, rowY), 1.0F, color);

				sprintf(buf, "%d", ent.score);
				size = font.Measure(buf);
				font.Draw(buf, MakeVector2(colX + colWidth - 10.0F - size.x, rowY), 1.0F, white);

				// display intel
				IGameMode& mode = *world->GetMode();
				if (mode.ModeType() == IGameMode::m_CTF) {
					auto& ctfMode = static_cast<CTFGameMode&>(mode);
					if (ctfMode.PlayerHasIntel(*world, *world->GetPlayer(ent.id))) {
						Handle<IImage> img = renderer.RegisterImage("Gfx/Map/Intel.png");
						float pulse = std::max(0.5F, fabsf(sinf(world->GetTime() * 4.0F)));
						renderer.SetColorAlphaPremultiplied(MakeVector4(1, 1, 1, 1) * pulse);
						renderer.DrawImage(img, AABB2(colX + colWidth - 10.0F - size.x - 18.0F,
						                              rowY + 2.0F, 16.0F, 16.0F));
					}
				}

				row++;
				if (row >= maxRows) {
					col++;
					row = 0;
				}
			}
		}

		void ScoreboardView::DrawSpectators(float top, float centerX) const {
			IFont& font = client->fontManager->GetGuiFont();
			char buf[256];
			std::vector<ScoreboardEntry> entries;

			static const auto xPixelSpectatorOffset = 20.0F;

			int numSpectators = 0;
			float totalPixelWidth = 0;
			for (size_t i = 0; i < world->GetNumPlayerSlots(); i++) {
				auto maybePlayer = world->GetPlayer(i);
				if (!maybePlayer)
					continue;

				Player& player = maybePlayer.value();
				if (player.GetTeamId() != spectatorTeamId)
					continue;

				ScoreboardEntry ent;
				ent.name = player.GetName();
				ent.id = i;
				entries.push_back(ent);

				numSpectators++;

				// Measure total width in pixels so that we can center align all the spectators
				sprintf(buf, "#%d", ent.id);
				totalPixelWidth += font.Measure(buf).x + font.Measure(ent.name).x + xPixelSpectatorOffset;
			}

			if (numSpectators == 0)
				return;

			strcpy(buf, _TrN("Client", "Spectator{1}", "Spectators{1}", numSpectators, "").c_str());

			auto isSquareFont = spectatorFont == &client->fontManager->GetSquareDesignFont();
			auto sizeSpecString = spectatorFont->Measure(buf);
			spectatorFont->Draw(
			  buf, MakeVector2(centerX - sizeSpecString.x / 2, top + (isSquareFont ? 0 : 10)), 1.0F,
			  spectatorTextColor);

			auto yOffset = top + sizeSpecString.y;
			auto halfTotalX = totalPixelWidth / 2;
			auto currentXoffset = centerX - halfTotalX;

			for (const auto& ent : entries) {
				sprintf(buf, "#%d", ent.id);
				font.Draw(buf, MakeVector2(currentXoffset, yOffset), 1.0F, spectatorIdColor);

				auto sizeName = font.Measure(ent.name);
				auto sizeID = font.Measure(buf);
				font.Draw(ent.name, MakeVector2(currentXoffset + sizeID.x + 5.0F, yOffset), 1.0F, white);

				currentXoffset += sizeID.x + sizeName.x + xPixelSpectatorOffset;
			}
		}

		bool ScoreboardView::AreSpectatorsPresent() const {
			for (size_t i = 0; i < world->GetNumPlayerSlots(); i++) {
				auto p = world->GetPlayer(i);
				if (p && p.value().GetTeamId() == spectatorTeamId)
					return true;
			}

			return false;
		}
	} // namespace client
} // namespace spades
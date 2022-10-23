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

#include "TCProgressView.h"
#include "Client.h"
#include "Fonts.h"
#include "IFont.h"
#include "IRenderer.h"
#include "TCGameMode.h"
#include "World.h"
#include <Core/Strings.h>

namespace spades {
	namespace client {
		struct TCProgressState {
			int team1, team2;
			float progress; // 0 = team1 owns
		};

		TCProgressView::TCProgressView(Client& client)
		    : client(client), renderer(client.GetRenderer()) {
			lastTerritoryId = -1;
		}

		TCProgressView::~TCProgressView() {}

		static TCProgressState StateForTerritory(TCGameMode::Territory& t, int myTeam) {
			TCProgressState state;
			if (t.capturingTeamId == -1) {
				state.team1 = t.ownerTeamId;
				state.team2 = NEUTRAL_TEAM;
				state.progress = 0.0F;
			} else {
				state.team1 = t.ownerTeamId;
				state.team2 = t.capturingTeamId;
				state.progress = t.GetProgress();

				if (state.team2 == myTeam) {
					std::swap(state.team1, state.team2);
					state.progress = 1.0F - state.progress;
				}
			}
			return state;
		}

		void TCProgressView::Draw() {
			World* w = client.GetWorld();
			if (!w) {
				lastTerritoryId = -1;
				return;
			}

			stmp::optional<IGameMode&> mode = w->GetMode();
			if (!mode || IGameMode::m_TC != mode->ModeType())
				return;

			TCGameMode& tc = dynamic_cast<TCGameMode&>(mode.value());

			float sw = renderer.ScreenWidth();
			float sh = renderer.ScreenHeight();

			stmp::optional<Player&> maybePlayer = w->GetLocalPlayer();
			if (maybePlayer && !maybePlayer.value().IsSpectator() &&
			    maybePlayer.value().IsAlive()) {
				Player& p = maybePlayer.value();

				// show approaching territory
				stmp::optional<TCGameMode::Territory&> nearTerritory;
				int nearTerritoryId = 0;
				float distance = 0.0F;
				int myTeam = p.GetTeamId();

				for (int i = 0; i < tc.GetNumTerritories(); i++) {
					TCGameMode::Territory& t = tc.GetTerritory(i);
					Vector3 diff = t.pos - p.GetEye();
					if (fabsf(diff.x) < TC_CAPTURE_DISTANCE &&
					    fabsf(diff.y) < TC_CAPTURE_DISTANCE &&
					    fabsf(diff.z) < TC_CAPTURE_DISTANCE) {
						float dist = diff.GetSquaredLength();
						if (!nearTerritory || dist < distance) {
							nearTerritory = t;
							nearTerritoryId = i;
							distance = dist;
						}
					}
				}

				float fade = 1.0F;
				if (nearTerritory) {
					lastTerritoryId = nearTerritoryId;
					lastCaptureTime = w->GetTime();
				} else if (lastTerritoryId != -1) {
					float wTime = w->GetTime();
					float timeSinceLastCapture = wTime - lastCaptureTime;
					const float fadeOutTime = 2.0F;
					if (wTime >= lastCaptureTime && timeSinceLastCapture < fadeOutTime) {
						fade = 1.0F - timeSinceLastCapture / fadeOutTime;
						nearTerritory = &tc.GetTerritory(lastTerritoryId);
					}
				}

				if (nearTerritory) {
					TCProgressState state = StateForTerritory(*nearTerritory, myTeam);

					int ownerTeam = nearTerritory->ownerTeamId;

					float prgW = 440.0F;
					float prgH = 8.0F;
					float prgX = (sw - prgW) * 0.5F;
					float prgY = sh - 64.0F;

					Vector4 col = MakeVector4(1, 1, 1, 1) * 0.5F;

					// background bar
					if (ownerTeam != NEUTRAL_TEAM)
						col = ConvertColorRGBA(w->GetTeamColor(ownerTeam));
					renderer.SetColorAlphaPremultiplied(col * fade);
					renderer.DrawImage(nullptr, AABB2(prgX, prgY, prgW, prgH));

					// progress bar
					float prg = 1.0F - state.progress;
					if (state.team1 != NEUTRAL_TEAM)
						col = ConvertColorRGBA(w->GetTeamColor(state.team1));
					else if (state.team2 != NEUTRAL_TEAM)
						col = ConvertColorRGBA(w->GetTeamColor(state.team2));
					renderer.SetColorAlphaPremultiplied(col * (fade * 0.8F));
					renderer.DrawImage(nullptr, AABB2(prgX, prgY, prgW * prg, prgH));

					IFont& font = client.fontManager->GetGuiFont();

					std::string str;
					if (ownerTeam == NEUTRAL_TEAM) {
						str = _Tr("Client", "Neutral Territory");
					} else {
						str = w->GetTeam(ownerTeam).name;
						str = _Tr("Client", "{0}'s Territory", str);
					}

					Vector2 size = font.Measure(str);
					Vector2 pos = Vector2((sw - size.x) * 0.5F, prgY - 8.0F - size.y);

					font.DrawShadow(str, pos, 1.0F, MakeVector4(1, 1, 1, fade),
					                MakeVector4(0, 0, 0, 0.5F * fade));
				}
			} else {
				// unable to show nearby territory
				lastTerritoryId = -1;
			}
		}
	} // namespace client
} // namespace spades
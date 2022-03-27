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
#include <utility>

#include "CTFGameMode.h"
#include "Client.h"
#include "Fonts.h"
#include "GameMap.h"
#include "IImage.h"
#include "IRenderer.h"
#include "MapView.h"
#include "Player.h"
#include "TCGameMode.h"
#include "Weapon.h"
#include "World.h"
#include <Core/Settings.h>
#include <Core/TMPUtils.h>

DEFINE_SPADES_SETTING(cg_minimapSize, "128");
DEFINE_SPADES_SETTING(cg_minimapPlayerColor, "1");
DEFINE_SPADES_SETTING(cg_minimapPlayerIcon, "1");

using std::pair;
using stmp::optional;

namespace spades {
	namespace client {
		namespace {
			optional<pair<Vector2, Vector2>> ClipLineSegment(
				const pair<Vector2, Vector2>& inLine,  const Plane2& plane) {
				const float distance1 = plane.GetDistanceTo(inLine.first);
				const float distance2 = plane.GetDistanceTo(inLine.second);
				int bits = (distance1 > 0 ? 1 : 0) | (distance2 > 0 ? 2 : 0);
				switch (bits) {
					case 0: return {};
					case 3: return inLine;
				}

				const float frac = distance1 / (distance1 - distance2);
				Vector2 intersection = Mix(inLine.first, inLine.second, frac);
				if (bits == 1)
					return std::make_pair(inLine.first, intersection);
				else
					return std::make_pair(intersection, inLine.second);
			}

			optional<pair<Vector2, Vector2>> ClipLineSegment(const pair<Vector2,
				Vector2>& inLine, const AABB2& rect) {
				optional<pair<Vector2, Vector2>> line =
				  ClipLineSegment(inLine, Plane2{1, 0, -rect.GetMinX()});
				if (!line)
					return line;
				line = ClipLineSegment(*line, Plane2{-1, 0, rect.GetMaxX()});
				if (!line)
					return line;
				line = ClipLineSegment(*line, Plane2{0, 1, -rect.GetMinY()});
				if (!line)
					return line;
				line = ClipLineSegment(*line, Plane2{0, -1, rect.GetMaxY()});
				return line;
			}
		} // namespace

		MapView::MapView(Client* c, bool largeMap)
		    : client(c), renderer(c->GetRenderer()), largeMap(largeMap) {
			scaleMode = 2;
			actualScale = 1.0F;
			lastScale = 1.0F;
			zoomed = false;
			zoomState = 0.0F;
		}

		MapView::~MapView() {}

		void MapView::Update(float dt) {
			float scale = 0.0F;
			switch (scaleMode) {
				case 0: scale = 1.0F / 4.0F; break; // 400%
				case 1: scale = 1.0F / 2.0F; break; // 200%
				case 2: scale = 1.0F; break;        // 100%
				case 3: scale = 2.0F; break;        // 50%
				default: SPAssert(false);
			}

			if (actualScale != scale) {
				float spd = fabsf(scale - lastScale) * 10.0F;
				spd = std::max(spd, 0.2F);
				spd *= dt;
				if (scale > actualScale) {
					actualScale += spd;
					if (actualScale > scale)
						actualScale = scale;
				} else {
					actualScale -= spd;
					if (actualScale < scale)
						actualScale = scale;
				}
			}

			if (zoomed) {
				zoomState += dt * 10.0F;
				if (zoomState > 1.0F)
					zoomState = 1.0F;
			} else {
				zoomState -= dt * 10.0F;
				if (zoomState < 0.0F)
					zoomState = 0.0F;
			}
		}

		Vector2 MapView::Project(const Vector2& pos) const {
			Vector2 scrPos;
			scrPos.x = (pos.x - inRect.GetMinX()) / inRect.GetWidth();
			scrPos.x = (scrPos.x * outRect.GetWidth()) + outRect.GetMinX();
			scrPos.y = (pos.y - inRect.GetMinY()) / inRect.GetHeight();
			scrPos.y = (scrPos.y * outRect.GetHeight()) + outRect.GetMinY();
			return scrPos;
		}

		void MapView::DrawIcon(spades::Vector3 pos, IImage& img, float rotation) {
			if (pos.x < inRect.GetMinX() || pos.x > inRect.GetMaxX() ||
				pos.y < inRect.GetMinY() || pos.y > inRect.GetMaxY())
				return;

			Vector2 scrPos = Project(Vector2{pos.x, pos.y});
			float c = (rotation != 0.0F) ? cosf(rotation) : 1.0F;
			float s = (rotation != 0.0F) ? sinf(rotation) : 0.0F;
			static const float coords[][2] = {{-1, -1}, {1, -1}, {-1, 1}};
			Vector2 u = MakeVector2(img.GetWidth() * 0.5F, 0.0F);
			Vector2 v = MakeVector2(0.0F, img.GetHeight() * 0.5F);

			Vector2 vt[3];
			for (int i = 0; i < 3; i++) {
				Vector2 ss = u * coords[i][0] + v * coords[i][1];
				vt[i].x = scrPos.x + ss.x * c - ss.y * s;
				vt[i].y = scrPos.y + ss.x * s + ss.y * c;
			}

			renderer.DrawImage(img, vt[0], vt[1], vt[2],
				AABB2(0, 0, img.GetWidth(), img.GetHeight()));
		}

		void MapView::SwitchScale() {
			scaleMode = (scaleMode + 1) % 4;
			lastScale = actualScale;
		}

		bool MapView::ToggleZoom() {
			zoomed = !zoomed;
			return zoomed;
		}

		std::string MapView::MapCoords(int x, int y) {
			SPADES_MARK_FUNCTION_DEBUG();

			x = div(x, 64).quot;
			y = div(y, 64).quot + 1;

			char buf[8];

			switch (x) {
				case 0: sprintf(buf, "A%i", y); break;
				case 1: sprintf(buf, "B%i", y); break;
				case 2: sprintf(buf, "C%i", y); break;
				case 3: sprintf(buf, "D%i", y); break;
				case 4: sprintf(buf, "E%i", y); break;
				case 5: sprintf(buf, "F%i", y); break;
				case 6: sprintf(buf, "G%i", y); break;
				case 7: sprintf(buf, "H%i", y); break;
				default: return std::string("XY");
			}

			return std::string(buf);
		}

		// definite a palette of 32 color in RGB code
		int palette[32][3] = {
		  {0, 0, 0},       // 0  Black			#000000
		  {255, 255, 255}, // 1  White			#FFFFFF
		  {128, 128, 128}, // 2  Dark Grey		#808080
		  {255, 255, 0},   // 3  Yellow			#FFFF00
		  {0, 255, 255},   // 4  Cyan			#00FFFF
		  {255, 0, 255},   // 5  Magenta		#FF00FF
		  {255, 0, 0},     // 6  Red			#FF0000
		  {0, 255, 0},     // 7  Bright Green	#00FF00
		  {0, 0, 255},     // 8  Blue			#0000FF
		  {128, 0, 0},     // 9  Dark Red		#800000
		  {0, 128, 0},     // 10 Green			#008000
		  {0, 0, 128},     // 11 Navy Blue		#000080
		  {128, 128, 0},   // 12 Olive			#808000
		  {128, 0, 128},   // 13 Purple			#800080
		  {0, 128, 128},   // 14 Teal			#008080
		  {255, 128, 0},   // 15 Orange			#FF8000
		  {255, 0, 128},   // 16 Pink			#FF0080
		  {128, 0, 255},   // 17 Violet			#8000FF
		  {0, 128, 255},   // 18 Bluette		#0080FF
		  {128, 255, 0},   // 19 Lime Green		#80FF00
		  {0, 255, 128},   // 20 Spring Green	#00FF80
		  {255, 128, 128}, // 21 Salmon			#FF8080
		  {128, 255, 128}, // 22 Light Green	#80FF80
		  {128, 128, 255}, // 23 light Blue		#8080FF
		  {128, 255, 255}, // 24 Light Cyan		#80FFFF
		  {255, 255, 128}, // 25 Light Yellow	#FFFF80
		  {255, 128, 255}, // 26 Light Magenta	#FF80FF
		  {165, 42, 42},   // 27 Maroon			#A52A2A
		  {255, 69, 0},    // 28 Scarlet		#FF4500
		  {255, 165, 0},   // 29 Orange			#FFA500
		  {139, 69, 19},   // 30 Brown			#8B4513
		  {210, 105, 30},  // 31 Chocolate		#D2691E
		};

		void MapView::Draw() {
			World* world = client->GetWorld();
			if (!world)
				return;

			auto cameraMode = client->GetCameraMode();

			// The player to focus on
			stmp::optional<Player&> focusPlayerPtr;
			Vector3 focusPlayerPos;
			float focusPlayerAngle;

			if (HasTargetPlayer(cameraMode)) {
				Player& player = client->GetCameraTargetPlayer();
				Vector3 front = player.GetFront2D();

				focusPlayerPos = player.GetPosition();
				focusPlayerAngle = atan2(front.x, -front.y);
				focusPlayerPtr = player;
			} else if (cameraMode == ClientCameraMode::Free) {
				focusPlayerPos = client->freeCameraState.position;
				focusPlayerAngle = client->followAndFreeCameraState.yaw - M_PI_F * 0.5F;
				focusPlayerPtr = world->GetLocalPlayer();
			} else {
				return;
			}

			// The local player (this is important for access control)
			if (!world->GetLocalPlayer())
				return;

			Player& localPlayer = world->GetLocalPlayer().value();
			Player& focusPlayer = focusPlayerPtr.value();

			if (largeMap && zoomState < 0.0001F)
				return;

			auto sw = renderer.ScreenWidth();
			auto sh = renderer.ScreenHeight();

			Handle<GameMap> map = world->GetMap();
			SPAssert(map);

			Vector2 mapSize = MakeVector2((float)map->Width(), (float)map->Height());

			float cfgMapSize = Clamp((float)cg_minimapSize, 32.0F, 256.0F);
			Vector2 mapWndSize = {cfgMapSize, cfgMapSize};

			Vector2 center = {focusPlayerPos.x, focusPlayerPos.y};
			center = Mix(center, mapSize * 0.5F, zoomState);

			Vector2 zoomedSize = {512, 512};
			if (sw < zoomedSize.x || sh < zoomedSize.y)
				zoomedSize *= 0.75F;

			if (largeMap) {
				float per = zoomState;
				per = 1.0F - per;
				per *= per;
				per = 1.0F - per;
				per = Mix(0.75F, 1.0F, per);
				zoomedSize = Mix(MakeVector2(0, 0), zoomedSize, per);
				mapWndSize = zoomedSize;
			}

			Vector2 inRange = mapWndSize * 0.5F * actualScale;
			AABB2 inRect(center - inRange, center + inRange);

			if (largeMap) {
				inRect.min = MakeVector2(0, 0);
				inRect.max = mapSize;
			} else {
				if (inRect.GetMinX() < 0.0F)
					inRect = inRect.Translated(-inRect.GetMinX(), 0);
				if (inRect.GetMinY() < 0.0F)
					inRect = inRect.Translated(0, -inRect.GetMinY());
				if (inRect.GetMaxX() > mapSize.x)
					inRect = inRect.Translated(mapSize.x - inRect.GetMaxX(), 0);
				if (inRect.GetMaxY() > mapSize.y)
					inRect = inRect.Translated(0, mapSize.y - inRect.GetMaxY());
			}

			AABB2 outRect((sw - mapWndSize.x) - 8.0F, 8.0F, mapWndSize.x, mapWndSize.y);
			if (largeMap) {
				outRect.min = MakeVector2((sw - zoomedSize.x) * 0.5F, (sh - zoomedSize.y) * 0.5F);
				outRect.max = MakeVector2((sw + zoomedSize.x) * 0.5F, (sh + zoomedSize.y) * 0.5F);
			}

			float alpha = largeMap ? zoomState : 1.0F;

			// draw map
			renderer.SetColorAlphaPremultiplied(MakeVector4(1, 1, 1, 1) * alpha);
			renderer.DrawFlatGameMap(outRect, inRect);

			this->inRect = inRect;
			this->outRect = outRect;

			// draw grid
			Vector2 gridSize = mapSize / 8.0F;

			renderer.SetColorAlphaPremultiplied(MakeVector4(0.8F, 0.8F, 0.8F, 0.8F * alpha));
			for (float x = gridSize.x; x < mapSize.x; x += gridSize.x) {
				float wx = (x - inRect.GetMinX()) / inRect.GetWidth();
				if (wx < 0.0F || wx >= 1.0F)
					continue;
				wx = (wx * outRect.GetWidth()) + outRect.GetMinX();
				renderer.DrawImage(nullptr, MakeVector2(wx, outRect.GetMinY()),
				                   AABB2(0, 0, 1, outRect.GetHeight()));
			}
			for (float y = gridSize.y; y < mapSize.y; y += gridSize.y) {
				float wy = (y - inRect.GetMinY()) / inRect.GetHeight();
				if (wy < 0.0F || wy >= 1.0F)
					continue;
				wy = (wy * outRect.GetHeight()) + outRect.GetMinY();
				renderer.DrawImage(nullptr, MakeVector2(outRect.GetMinX(), wy),
				                   AABB2(0, 0, outRect.GetWidth(), 1));
			}

			// Draw grid label
			Handle<IImage> mapFont = renderer.RegisterImage("Gfx/Fonts/MapFont.tga");

			for (int i = 0; i < 8; i++) {
				float startX = (float)i * gridSize.x;
				float endX = startX + gridSize.x;

				if (startX > inRect.GetMaxX() || endX < inRect.GetMinX())
					continue;

				float fade = std::min((std::min(endX, inRect.GetMaxX())
					  - std::max(startX, inRect.GetMinX()))
					  / (endX - startX) * 2.0F, 1.0F);

				renderer.SetColorAlphaPremultiplied(MakeVector4(1, 1, 1, 1) *
				                                    (fade * 0.8F * alpha));

				float center = std::max(startX, inRect.GetMinX());
				center = 0.5F * (center + std::min(endX, inRect.GetMaxX()));

				float wx = (center - inRect.GetMinX()) / inRect.GetWidth();
				wx = (wx * outRect.GetWidth()) + outRect.GetMinX();

				float fntX = static_cast<float>((i & 3) * 8);
				float fntY = static_cast<float>((i >> 2) * 8);

				renderer.DrawImage(mapFont, MakeVector2(wx - 4, outRect.GetMinY() + 4),
				                   AABB2(fntX, fntY, 8, 8));
			}

			for (int i = 0; i < 8; i++) {
				float startY = (float)i * gridSize.y;
				float endY = startY + gridSize.y;

				if (startY > inRect.GetMaxY() || endY < inRect.GetMinY())
					continue;

				float fade = std::min((std::min(endY, inRect.GetMaxY())
					- std::max(startY, inRect.GetMinY()))
					/ (endY - startY) * 2.0F, 1.0F);

				renderer.SetColorAlphaPremultiplied(MakeVector4(1, 1, 1, 1) *
				                                    (fade * 0.8F * alpha));

				float center = std::max(startY, inRect.GetMinY());
				center = 0.5F * (center + std::min(endY, inRect.GetMaxY()));

				float wy = (center - inRect.GetMinY()) / inRect.GetHeight();
				wy = (wy * outRect.GetHeight()) + outRect.GetMinY();

				float fntX = static_cast<float>((i & 3) * 8);
				float fntY = static_cast<float>((i >> 2) * 8 + 16);

				renderer.DrawImage(mapFont, MakeVector2(outRect.GetMinX() + 4, wy - 4),
				                   AABB2(fntX, fntY, 8, 8));
			}

			// draw border
			renderer.SetColorAlphaPremultiplied(MakeVector4(0, 0, 0, alpha));
			renderer.DrawOutlinedRect(outRect.GetMinX() - 1, outRect.GetMinY() - 1,
			                          outRect.GetMaxX() + 1, outRect.GetMaxY() + 1);

			// draw objects
			Handle<IImage> playerIcon = renderer.RegisterImage("Gfx/Map/Player.png");
			Handle<IImage> viewIcon = localPlayer.IsScoped()
				? renderer.RegisterImage("Gfx/Map/ViewADS.png")
				: renderer.RegisterImage("Gfx/Map/View.png");

			// draw player's icon
			for (size_t i = 0; i < world->GetNumPlayerSlots(); i++) {
				auto maybePlayer = world->GetPlayer(i);
				if (!maybePlayer)
					continue; // The player is non-existent

				Player& p = maybePlayer.value();
				if (!p.IsAlive())
					continue; // The player is dead
				if (!localPlayer.IsSpectator() && !localPlayer.IsTeamMate(&p))
					continue; // Don't draw enemies when not spectating a player
				if (p.IsSpectator() && &p == &localPlayer && HasTargetPlayer(cameraMode))
					continue; // Don't draw when spectating a player
				if (p.IsSpectator() && &p != &localPlayer)
					continue; // Don't draw other spectators

				if (cg_minimapPlayerIcon) {
					switch (p.GetWeaponType()) {
						case RIFLE_WEAPON:
							playerIcon = renderer.RegisterImage("Gfx/Map/Rifle.png");
							break;
						case SMG_WEAPON:
							playerIcon = renderer.RegisterImage("Gfx/Map/SMG.png");
							break;
						case SHOTGUN_WEAPON:
							playerIcon = renderer.RegisterImage("Gfx/Map/Shotgun.png");
							break;
						default: playerIcon = renderer.RegisterImage("Gfx/Map/Player.png"); break;
					}
				}

				IntVector3 iconColor =
				  cg_minimapPlayerColor
				    ? MakeIntVector3(palette[i][0], palette[i][1], palette[i][2])
				    : p.GetColor();
				Vector4 iconColorF = ModifyColor(iconColor) * alpha;

				Vector3 fwd = p.GetFront2D();
				float ang = atan2f(fwd.x, -fwd.y);
				if (p.IsSpectator() && cameraMode == ClientCameraMode::Free)
					ang = focusPlayerAngle;

				// Draw the focused player's view
				if (&p == &focusPlayer) {
					renderer.SetColorAlphaPremultiplied(iconColorF * 0.9F);
					DrawIcon(p.IsSpectator() ? focusPlayerPos : p.GetPosition(), *viewIcon, ang);
				}

				renderer.SetColorAlphaPremultiplied(iconColorF);
				DrawIcon((&p == &focusPlayer) ? focusPlayerPos : p.GetPosition(), *playerIcon, ang);
			}

			stmp::optional<IGameMode&> mode = world->GetMode();
			if (mode && IGameMode::m_CTF == mode->ModeType()) {
				CTFGameMode& ctf = dynamic_cast<CTFGameMode&>(*mode);
				Handle<IImage> intelIcon = renderer.RegisterImage("Gfx/Map/Intel.png");
				Handle<IImage> baseIcon = renderer.RegisterImage("Gfx/Map/CommandPost.png");
				for (int tId = 0; tId < 2; tId++) {
					CTFGameMode::Team& team = ctf.GetTeam(tId);
					Vector4 teamColorF = ModifyColor(world->GetTeam(tId).color) * alpha;

					// draw base
					renderer.SetColorAlphaPremultiplied(teamColorF);
					DrawIcon(team.basePos, *baseIcon);

					// draw flag
					if (!ctf.GetTeam(1 - tId).hasIntel) {
						renderer.SetColorAlphaPremultiplied(teamColorF);
						DrawIcon(team.flagPos, *intelIcon);
					} else if (localPlayer.GetTeamId() == (1 - tId)) {
						// local player's team is carrying
						size_t cId = ctf.GetTeam(1 - tId).carrier;

						// in some game modes, carrier becomes invalid
						if (cId < world->GetNumPlayerSlots()) {
							auto carrier = world->GetPlayer(cId);
							if (carrier && carrier->IsTeamMate(&localPlayer)) {
								float pulse = std::max(0.5F, fabsf(sinf(world->GetTime() * 4.0F)));
								renderer.SetColorAlphaPremultiplied(teamColorF * pulse);
								DrawIcon(carrier->GetPosition(), *intelIcon);
							}
						}
					}
				}
			} else if (mode && IGameMode::m_TC == mode->ModeType()) {
				TCGameMode& tc = dynamic_cast<TCGameMode&>(*mode);
				Handle<IImage> baseIcon = renderer.RegisterImage("Gfx/Map/CommandPost.png");
				for (int i = 0; i < tc.GetNumTerritories(); i++) {
					TCGameMode::Territory& t = tc.GetTerritory(i);
					IntVector3 teamColor = (t.ownerTeamId < 2)
					                         ? world->GetTeam(t.ownerTeamId).color
					                         : MakeIntVector3(128);

					Vector4 teamColorF = ModifyColor(teamColor) * alpha;
					renderer.SetColorAlphaPremultiplied(teamColorF);
					DrawIcon(t.pos, *baseIcon);
				}
			}

			// draw tracers
			Handle<IImage> tracerImg = renderer.RegisterImage("Gfx/Ball.png");
			const float tracerW = 2.0F;
			const AABB2 tracerInRect{0.0F, 0.0F, tracerImg->GetWidth(), tracerImg->GetHeight()};

			for (const auto& localEntity : client->localEntities) {
				auto* const tracer = dynamic_cast<MapViewTracer*>(localEntity.get());
				if (!tracer)
					continue;

				const auto line1 = tracer->GetLineSegment();
				if (!line1)
					continue;

				auto line2 = ClipLineSegment(std::make_pair(Vector2{(*line1).first.x, (*line1).first.y},
				                                 Vector2{(*line1).second.x, (*line1).second.y}), inRect);
				if (!line2)
					continue;

				auto& line3 = *line2;
				line3.first = Project(line3.first);
				line3.second = Project(line3.second);

				if (line3.first == line3.second)
					continue;

				Vector2 normal = (line3.second - line3.first).Normalize();
				normal = {-normal.y, normal.x};

				{
					const Vector2 vt[] = {
						line3.first - normal * tracerW,
					    line3.first + normal * tracerW,
			            line3.second - normal * tracerW
					};

					renderer.SetColorAlphaPremultiplied(Vector4{1, 0.6F, 0.8F, 1} * alpha);
					renderer.DrawImage(tracerImg, vt[0], vt[1], vt[2], tracerInRect);
				}
			}

			if (!largeMap) {
				IFont& font = client->fontManager->GetGuiFont();
				auto msg = MapCoords((int)focusPlayerPos.x, (int)focusPlayerPos.y);
				Vector2 pos = {(outRect.min.x + outRect.max.x) * 0.5F, outRect.max.y + 2.0F};
				pos.x -= font.Measure(msg).x * 0.5F;
				font.DrawShadow(msg, pos, 1.0F, MakeVector4(1, 1, 1, 0.8F),
				                MakeVector4(0, 0, 0, 0.8F));
			}
		}

		MapViewTracer::MapViewTracer(Vector3 p1, Vector3 p2, float bulletVel)
		    : startPos(p1), velocity(bulletVel) {
			// Z coordinate doesn't matter in MapView
			p1.z = 0.0F;
			p2.z = 0.0F;

			dir = (p2 - p1).Normalize();
			length = (p2 - p1).GetLength();

			// in MapView it looks slower than it is actually, so compensate for that
			bulletVel *= 4.0F;

			const float maxTimeSpread = 1.0F / 10.0F;
			const float shutterTime = 1.0F / 10.0F;

			visibleLength = shutterTime * velocity;
			curDistance = -visibleLength;

			// Randomize the starting position within the range of the shutter
			// time. However, make sure the tracer is displayed for at least one frame.
			curDistance += std::min(length + visibleLength,
				maxTimeSpread * SampleRandomFloat() * velocity);

			firstUpdate = true;
		}

		bool MapViewTracer::Update(float dt) {
			if (!firstUpdate) {
				curDistance += dt * velocity;
				if (curDistance > length)
					return false;
			}

			firstUpdate = false;
			return true;
		}

		stmp::optional<std::pair<Vector3, Vector3>> MapViewTracer::GetLineSegment() {
			float startDist = curDistance;
			float endDist = curDistance + visibleLength;
			startDist = std::max(startDist, 0.0F);
			endDist = std::min(endDist, length);
			if (startDist >= endDist)
				return {};

			Vector3 pos1 = startPos + dir * startDist;
			Vector3 pos2 = startPos + dir * endDist;
			return std::make_pair(pos1, pos2);
		}

		MapViewTracer::~MapViewTracer() {}
	} // namespace client
} // namespace spades

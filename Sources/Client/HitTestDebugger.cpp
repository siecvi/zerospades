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

#include "GameMap.h"
#include "HitTestDebugger.h"
#include "Player.h"
#include "Weapon.h"
#include "World.h"
#include <Core/Strings.h>
#include <Draw/SWPort.h>
#include <Draw/SWRenderer.h>

namespace spades {
	namespace client {
		class HitTestDebugger::Port : public draw::SWPort {
			Handle<Bitmap> bmp;

		public:
			Port() {
				SPADES_MARK_FUNCTION();
				bmp = Handle<Bitmap>::New(512, 512);
			}
			Bitmap& GetFramebuffer() override { return *bmp; }
			void Swap() override {} // nothing to do here
		};

		HitTestDebugger::HitTestDebugger(World* world) : world(world) {
			SPADES_MARK_FUNCTION();
			port = Handle<Port>::New();
			renderer = Handle<draw::SWRenderer>::New(port.Cast<draw::SWPort>()).Cast<IRenderer>();
			renderer->Init();

			GameMap* map = world->GetMap().GetPointerOrNull();
			if (map != nullptr)
				renderer->SetGameMap(map);
		}

		HitTestDebugger::~HitTestDebugger() {
			SPADES_MARK_FUNCTION();

			renderer->Shutdown();
		}

		void HitTestDebugger::SaveImage(const std::map<int, PlayerHit>& hits,
		                                const std::vector<Vector3>& bullets) {
			SPADES_MARK_FUNCTION();

			renderer->SetFogColor(MakeVector3(0, 0, 0));
			renderer->SetFogDistance(128.0F);

			stmp::optional<Player&> localPlayer = world->GetLocalPlayer();

			if (!localPlayer) {
				SPLog("HitTestDebugger failure: Local player is null");
				return;
			}

			SceneDefinition def;
			def.viewOrigin = localPlayer->GetEye();
			def.viewAxis[0] = localPlayer->GetRight();
			def.viewAxis[1] = localPlayer->GetUp();
			def.viewAxis[2] = localPlayer->GetFront();

			auto toViewCoord = [&](const Vector3& targPos) {
				Vector3 targetViewPos;
				targetViewPos.x = Vector3::Dot(targPos - def.viewOrigin, def.viewAxis[0]);
				targetViewPos.y = Vector3::Dot(targPos - def.viewOrigin, def.viewAxis[1]);
				targetViewPos.z = Vector3::Dot(targPos - def.viewOrigin, def.viewAxis[2]);
				return targetViewPos;
			};

			auto numPlayers = world->GetNumPlayerSlots();

			// fit FoV to include all possibly hit players
			float range = 0.2F;
			for (std::size_t i = 0; i < numPlayers; i++) {
				auto p = world->GetPlayer(static_cast<unsigned int>(i));
				if (!p || p == localPlayer)
					continue;
				if (!p->IsAlive() || p->IsSpectator())
					continue;
				if (p->GetTeamId() == localPlayer->GetTeamId())
					continue;
				if (!p->RayCastApprox(def.viewOrigin, def.viewAxis[2]))
					continue;

				auto vc = toViewCoord(p->GetEye());
				if (vc.GetPoweredLength() > 130.0F * 130.0F)
					continue;
				if (vc.z < -3.0F)
					continue;

				vc.z = std::max(vc.z, 0.1F);

				const float bodySize = 3.5F;
				if (fabsf(vc.x) > bodySize + 2.5F || fabsf(vc.y) > bodySize + 2.5F)
					continue;

				float prange = std::max(fabsf(vc.x), fabsf(vc.y)) + bodySize;
				prange = atanf(prange / vc.z) * 2.0F;
				range = std::max(range, prange);
			}

			// fit FoV to include all bullets
			for (const auto& v : bullets) {
				auto vc = toViewCoord(v + def.viewOrigin);
				vc /= vc.z;
				auto prange = atanf(std::max(fabsf(vc.x), fabsf(vc.y)) * 1.5F) * 2.0F;
				range = std::max(range, prange);
			}

			def.fovX = def.fovY = range;

			def.skipWorld = false;

			def.zNear = 0.05F;
			def.zFar = 200.0F;

			// start rendering
			const Handle<GameMap>& map = world->GetMap();
			if (!def.skipWorld)
				renderer->SetGameMap(&*map);
			renderer->StartScene(def);

			auto drawBox = [&](const OBB3& box, Vector4 color) {
				SPADES_MARK_FUNCTION();

				const auto& m = box.m;
				renderer->AddDebugLine((m * Vector3(0, 0, 0)).GetXYZ(),
				                       (m * Vector3(0, 0, 1)).GetXYZ(), color);
				renderer->AddDebugLine((m * Vector3(0, 1, 0)).GetXYZ(),
				                       (m * Vector3(0, 1, 1)).GetXYZ(), color);
				renderer->AddDebugLine((m * Vector3(1, 0, 0)).GetXYZ(),
				                       (m * Vector3(1, 0, 1)).GetXYZ(), color);
				renderer->AddDebugLine((m * Vector3(1, 1, 0)).GetXYZ(),
				                       (m * Vector3(1, 1, 1)).GetXYZ(), color);

				renderer->AddDebugLine((m * Vector3(0, 0, 0)).GetXYZ(),
				                       (m * Vector3(0, 1, 0)).GetXYZ(), color);
				renderer->AddDebugLine((m * Vector3(0, 1, 0)).GetXYZ(),
				                       (m * Vector3(1, 1, 0)).GetXYZ(), color);
				renderer->AddDebugLine((m * Vector3(1, 1, 0)).GetXYZ(),
				                       (m * Vector3(1, 0, 0)).GetXYZ(), color);
				renderer->AddDebugLine((m * Vector3(1, 0, 0)).GetXYZ(),
				                       (m * Vector3(0, 0, 0)).GetXYZ(), color);

				renderer->AddDebugLine((m * Vector3(0, 0, 1)).GetXYZ(),
				                       (m * Vector3(0, 1, 1)).GetXYZ(), color);
				renderer->AddDebugLine((m * Vector3(0, 1, 1)).GetXYZ(),
				                       (m * Vector3(1, 1, 1)).GetXYZ(), color);
				renderer->AddDebugLine((m * Vector3(1, 1, 1)).GetXYZ(),
				                       (m * Vector3(1, 0, 1)).GetXYZ(), color);
				renderer->AddDebugLine((m * Vector3(1, 0, 1)).GetXYZ(),
				                       (m * Vector3(0, 0, 1)).GetXYZ(), color);
			};

			auto getColor = [](int count) {
				SPADES_MARK_FUNCTION();

				switch (count) {
					case 0: return Vector4(0.5F, 0.5F, 0.5F, 1.0F);
					case 1: return Vector4(1.0F, 0.0F, 0.0F, 1.0F);
					case 2: return Vector4(1.0F, 1.0F, 0.0F, 1.0F);
					case 3: return Vector4(0.0F, 1.0F, 0.0F, 1.0F);
					case 4: return Vector4(0.0F, 1.0F, 1.0F, 1.0F);
					case 5: return Vector4(0.0F, 0.0F, 1.0F, 1.0F);
					case 6: return Vector4(1.0F, 0.0F, 1.0F, 1.0F);
					default: return Vector4(1.0F, 1.0F, 1.0F, 1.0F);
				}
			};

			for (std::size_t i = 0; i < numPlayers; i++) {
				auto p = world->GetPlayer(static_cast<unsigned int>(i));
				if (!p || p == localPlayer)
					continue;
				if (!p->IsAlive() || p->IsSpectator())
					continue;

				if (!p->RayCastApprox(def.viewOrigin, def.viewAxis[2]))
					continue;
				if ((p->GetEye() - def.viewOrigin).GetPoweredLength() > 130.0F * 130.0F)
					continue;

				auto hitboxes = p->GetHitBoxes();
				PlayerHit hit;
				{
					auto it = hits.find(static_cast<int>(i));
					if (it != hits.end())
						hit = it->second;
				}

				int numHits = hit.numHeadHits + hit.numTorsoHits;
				for (std::size_t i = 0; i < 3; i++)
					numHits += hit.numLimbHits[i];

				if (numHits > 0) {
					drawBox(hitboxes.head, getColor(hit.numHeadHits));
					drawBox(hitboxes.torso, getColor(hit.numTorsoHits));
					for (std::size_t i = 0; i < 3; i++)
						drawBox(hitboxes.limbs[i], getColor(hit.numLimbHits[i]));
				}
			}

			renderer->EndScene();

			// draw crosshair
			float size = renderer->ScreenWidth();

			renderer->SetColorAlphaPremultiplied(Vector4(1.0F, 0.0F, 0.0F, 0.9F));
			renderer->DrawImage(nullptr, AABB2(size * 0.5F - 1.0F, 0.0F, 2.0F, size));
			renderer->DrawImage(nullptr, AABB2(0.0F, size * 0.5F - 1.0F, size, 2.0F));

			// draw bullet vectors
			float fov = tanf(def.fovY * 0.5F);
			for (const auto& v : bullets) {
				auto vc = toViewCoord(v + def.viewOrigin);
				vc /= vc.z * fov;
				float x = floorf(size * (0.5F + 0.5F * vc.x));
				float y = floorf(size * (0.5F - 0.5F * vc.y));
				renderer->SetColorAlphaPremultiplied(Vector4(1.0F, 0.6F, 0.2F, 0.9F));
				renderer->DrawImage(nullptr, AABB2(x - 1.0F, y - 1.0F, 3.0F, 3.0F));
				renderer->SetColorAlphaPremultiplied(Vector4(1.0F, 1.0F, 0.0F, 0.9F));
				renderer->DrawImage(nullptr, AABB2(x, y, 1.0F, 1.0F));
			}

			renderer->FrameDone();

			// display image
			Handle<Bitmap> bmp = renderer->ReadBitmap();
			displayShot.Set(bmp.GetPointerOrNull());

			renderer->Flip();
		}

		Handle<Bitmap> HitTestDebugger::GetBitmap() {
			Handle<Bitmap> bmp = displayShot;
			displayShot.Set(nullptr);
			return bmp;
		}
	} // namespace client
} // namespace spades

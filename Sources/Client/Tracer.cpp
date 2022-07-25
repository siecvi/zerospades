//
//  Tracer.cpp
//  OpenSpades
//
//  Created by Tomoaki Kawada on 8/30/13.
//  Copyright (c) 2013 yvt.jp. All rights reserved.
//

#include <algorithm>

#include "Client.h"
#include "IRenderer.h"
#include "Tracer.h"
#include <Core/Settings.h>
#include <Draw/SWRenderer.h>

DEFINE_SPADES_SETTING(cg_tracerLights, "0");

namespace spades {
	namespace client {
		Tracer::Tracer(Client& _client, Vector3 p1, Vector3 p2, float bulletVel, bool shotgun)
		    : client(_client), startPos(p1), velocity(bulletVel), shotgun(shotgun) {
			dir = (p2 - p1).Normalize();
			length = (p2 - p1).GetLength();

			velocity *= 0.5F; // make it slower for visual effect

			const float maxTimeSpread = 1.0F / 30.0F;
			const float shutterTime = 1.0F / 30.0F;

			visibleLength = shutterTime * velocity;
			curDistance = -visibleLength;

			// Randomize the starting position within the range of the shutter
			// time. However, make sure the tracer is displayed for at least one frame.
			curDistance +=
			  std::min(length + visibleLength, maxTimeSpread * SampleRandomFloat() * velocity);

			firstUpdate = true;

			image = client.GetRenderer().RegisterImage("Gfx/Ball.png");
		}

		bool Tracer::Update(float dt) {
			if (!firstUpdate) {
				curDistance += dt * velocity;
				if (curDistance > length)
					return false;
			}
			firstUpdate = false;
			return true;
		}

		void Tracer::Render3D() {
			Vector4 col;
			if (shotgun)
				col = {0, 0, 0, 0.25};
			else
				col = {1, 0.6F, 0.2F, 0};

			IRenderer& r = client.GetRenderer();
			if (dynamic_cast<draw::SWRenderer*>(&r)) {
				// SWRenderer doesn't support long sprites (yet)
				float startDist = curDistance;
				float endDist = curDistance + visibleLength;

				startDist = std::max(startDist, 0.0F);
				endDist = std::min(endDist, length);
				if (startDist >= endDist)
					return;

				Vector3 pos1 = startPos + dir * startDist;
				Vector3 pos2 = startPos + dir * endDist;

				col.w = 1.0F;
				r.AddDebugLine(pos1, pos2, col);
			} else {
				SceneDefinition sceneDef = client.GetLastSceneDef();

				for (float step = 0.0F; step <= 1.0F; step += 0.1F) {
					float startDist = curDistance;
					float endDist = curDistance + visibleLength;

					float midDist = (startDist + endDist) * 0.5F;
					startDist = Mix(startDist, midDist, step);
					endDist = Mix(endDist, midDist, step);

					startDist = std::max(startDist, 0.0F);
					endDist = std::min(endDist, length);
					if (startDist >= endDist)
						continue;

					Vector3 pos1 = startPos + dir * startDist;
					Vector3 pos2 = startPos + dir * endDist;

					float distToCamera = (pos2 - sceneDef.viewOrigin).GetLength();
					float radius = 0.002F * distToCamera;

					r.SetColorAlphaPremultiplied(col * 0.4F);
					r.AddLongSprite(*image, pos1, pos2, radius);
				}

				// Add subtle dynamic light
				if (cg_tracerLights && !shotgun) {
					float startDist = curDistance;
					float endDist = curDistance + visibleLength;

					startDist = std::max(startDist, 0.0F);
					endDist = std::min(endDist, length);
					if (startDist >= endDist)
						return;

					Vector3 pos1 = startPos + dir * startDist;
					Vector3 pos2 = startPos + dir * endDist;

					DynamicLightParam light;
					light.origin = pos1;
					light.point2 = pos2;
					light.color = MakeVector3(col.x, col.y, col.z) * 0.1F *
					              ((endDist - startDist) / visibleLength);
					light.radius = 10.0F;
					light.type = DynamicLightTypeLinear;
					light.image = nullptr;
					r.AddLight(light);
				}
			}
		}

		Tracer::~Tracer() {}
	} // namespace client
} // namespace spades
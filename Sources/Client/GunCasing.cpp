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

#include "GunCasing.h"
#include "Client.h"
#include "GameMap.h"
#include "IAudioChunk.h"
#include "IAudioDevice.h"
#include "IRenderer.h"
#include "ParticleSpriteEntity.h"
#include "World.h"
#include <Core/Settings.h>

SPADES_SETTING(cg_particles);

namespace spades {
	namespace client {
		GunCasing::GunCasing(Client* client, IModel* model, IAudioChunk* dropSound,
		                     IAudioChunk* waterSound, Vector3 pos, Vector3 dir, Vector3 flyDir)
		    : client(client),
		      renderer(client->GetRenderer()),
		      model(model),
		      dropSound(dropSound),
		      waterSound(waterSound) {

			if (dropSound)
				dropSound->AddRef();
			if (waterSound)
				waterSound->AddRef();

			Vector3 up = MakeVector3(0, 0, 1);
			Vector3 right = Vector3::Cross(dir, up).Normalize();
			up = Vector3::Cross(right, dir);

			matrix = Matrix4::FromAxis(right, dir, up, pos);
			velocity = flyDir * 10.0F;
			rotDir = SampleRandom() & 3; // random initial dir
			time = 1.0F;
		}
		GunCasing::~GunCasing() {
			if (dropSound)
				dropSound->Release();
			if (waterSound)
				waterSound->Release();
		}

		bool GunCasing::Update(float dt) {
			time -= 1.0F / 5.0F * dt;

			const auto& viewOrigin = client->GetLastSceneDef().viewOrigin;
			float distSqr = (viewOrigin - matrix.GetOrigin()).GetSquaredLength2D();
			if (distSqr > FOG_DISTANCE_SQ)
				return false;

			if (time <= 0.0F)
				return false;

			Matrix4 lastMat = matrix;

			matrix = matrix * Matrix4::Rotate(MakeVector3(1, 0, 0),
				((rotDir & 1) ? 1.0F : -1.0F) * dt * 5.0F);
			matrix = matrix * Matrix4::Rotate(MakeVector3(0, 1, 0),
				((rotDir & 2) ? 1.0F : -1.0F) * dt * 5.0F);
			matrix = Matrix4::Translate(velocity * dt) * matrix;
			velocity.z += dt * 32.0F;

			const Handle<GameMap>& map = client->GetWorld()->GetMap();
			SPAssert(map);

			// Collision
			IntVector3 lp = matrix.GetOrigin().Floor();

			if (lp.z >= 63) { // dropped into water
				if (waterSound) {
					if (!client->IsMuted() && distSqr < CASING_BOUNCE_DIST_SQ) {
						IAudioDevice& dev = client->GetAudioDevice();
						AudioParam param;
						param.referenceDistance = 0.6F;
						param.pitch = 0.9F + SampleRandomFloat() * 0.2F;
						dev.Play(waterSound, lastMat.GetOrigin(), param);
					}
					waterSound = NULL;
				}

				if (cg_particles && distSqr < CASING_BOUNCE_DIST_SQ) {
					Handle<IImage> img = renderer.RegisterImage("Gfx/White.tga");

					Vector4 col = {1, 1, 1, 0.6F};
					Vector3 pt = matrix.GetOrigin();
					pt.z = 62.99F;

					int splats = SampleRandomInt(1, 3);
					for (int i = 0; i < splats; i++) {
						auto ent = stmp::make_unique<ParticleSpriteEntity>(*client, img, col);
						ent->SetTrajectory(pt,
							MakeVector3(
								SampleRandomFloat() - SampleRandomFloat(),
								SampleRandomFloat() - SampleRandomFloat(),
								-SampleRandomFloat()) * 2.0F, 1.0F, 0.4F
						);
						ent->SetRotation(SampleRandomFloat() * M_PI_F * 2.0F);
						ent->SetRadius(0.1F + SampleRandomFloat() * SampleRandomFloat() * 0.1F);
						ent->SetLifeTime(2.0F, 0.0F, 1.0F);
						client->AddLocalEntity(std::move(ent));
					}
				}

				return false;
			}

			if (map->ClipWorld(lp.x, lp.y, lp.z)) { // hit a wall
				IntVector3 lp2 = lastMat.GetOrigin().Floor();
				if (lp.z != lp2.z &&
				    ((lp.x == lp2.x && lp.y == lp2.y) || !map->ClipWorld(lp.x, lp.y, lp2.z))) {
					velocity.z = -velocity.z;
					if (lp2.z < lp.z) { // ground hit
						if (dropSound) {
							if (!client->IsMuted() && distSqr < CASING_BOUNCE_DIST_SQ) {
								IAudioDevice& dev = client->GetAudioDevice();
								AudioParam param;
								param.referenceDistance = 0.6F;
								dev.Play(dropSound, lastMat.GetOrigin(), param);
							}
							dropSound = NULL;
						}
					}
				} else if (lp.x != lp2.x &&
					((lp.y == lp2.y && lp.z == lp2.z) || !map->ClipWorld(lp2.x, lp.y, lp.z))) {
					velocity.x = -velocity.x;
				} else if (lp.y != lp2.y &&
					((lp.x == lp2.x && lp.z == lp2.z) || !map->ClipWorld(lp.x, lp2.y, lp.z))) {
					velocity.y = -velocity.y;
				} else {
					return false;
				}

				matrix = lastMat;
				velocity *= 0.46F; // lose some velocity due to friction
				rotDir = (rotDir + 1) % 4;
				time -= 0.36F;
			}

			return true;
		}

		void GunCasing::Render3D() {
			ModelRenderParam param;
			param.matrix = matrix * Matrix4::Scale(0.0125F);
			renderer.RenderModel(*model, param);
		}
	} // namespace client
} // namespace spades
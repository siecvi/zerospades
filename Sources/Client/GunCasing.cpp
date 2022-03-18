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

			up = MakeVector3(0, 0, -1);
			rotAxis = Vector3::Cross(up, flyDir.Normalize());

			groundTime = 0.0F;
			onGround = false;
			velocity = flyDir * 10.0F;
			rotSpeed = 40.0F;
		}
		GunCasing::~GunCasing() {
			if (dropSound)
				dropSound->Release();
			if (waterSound)
				waterSound->Release();
		}

		bool GunCasing::Update(float dt) {
			if (onGround) {
				groundTime += dt;
				if (groundTime > 2.0F)
					return false;

				const Handle<GameMap>& map = client->GetWorld()->GetMap();
				if (!map->ClipWorld(groundPos.x, groundPos.y, groundPos.z))
					return false;
			} else {
				Matrix4 lastMat = matrix;

				matrix = matrix * Matrix4::Rotate(rotAxis, dt * rotSpeed);
				matrix = Matrix4::Translate(velocity * dt) * matrix;
				velocity.z += dt * 32.0F;

				IntVector3 lp = matrix.GetOrigin().Floor();
				Handle<GameMap> m = client->GetWorld()->GetMap();

				Vector3 eye = client->GetLastSceneDef().viewOrigin;
				float dist = (eye - matrix.GetOrigin()).GetPoweredLength();

				if (lp.z >= 63) { // dropped into water
					if (dist < 40.0F * 40.0F) {
						if (waterSound) {
							if (client->IsMuted()) {
								IAudioDevice& dev = client->GetAudioDevice();
								AudioParam param;
								param.referenceDistance = 0.6F;
								param.pitch = 0.9F + SampleRandomFloat() * 0.2F;
								dev.Play(waterSound, lastMat.GetOrigin(), param);
							}

							waterSound = NULL;
						}

						Handle<IImage> img = client->GetRenderer().RegisterImage("Gfx/White.tga");

						Vector4 col = {1, 1, 1, 0.8F};
						Vector3 pt = matrix.GetOrigin();
						pt.z = 62.99F;

						int splats = SampleRandomInt(0, 2);
						for (int i = 0; i < splats; i++) {
							auto ent = stmp::make_unique<ParticleSpriteEntity>(*client, img, col);
							ent->SetTrajectory(pt, MakeVector3(SampleRandomFloat() - SampleRandomFloat(),
													SampleRandomFloat() - SampleRandomFloat(),
													-SampleRandomFloat()) * 2.0F, 1.0F, 0.4F);
							ent->SetRotation(SampleRandomFloat() * M_PI_F * 2.0F);
							ent->SetRadius(0.1F + SampleRandomFloat() * SampleRandomFloat() * 0.1F);
							ent->SetLifeTime(2.0F, 0.0F, 1.0F);
							client->AddLocalEntity(std::move(ent));
						}
					}

					return false;
				}

				if (m->ClipWorld(lp.x, lp.y, lp.z)) { // hit a wall
					IntVector3 lp2 = lastMat.GetOrigin().Floor();
					if (lp.z != lp2.z &&
					    ((lp.x == lp2.x && lp.y == lp2.y) || !m->ClipWorld(lp.x, lp.y, lp2.z))) {
						velocity.z = -velocity.z;
						if (lp2.z < lp.z) { // ground hit
							if (velocity.GetLength() < 0.5F + dt * 100.0F && !dropSound) {
								// stick to ground
								onGround = true;
								groundPos = lp;

								// move to surface
								float z = matrix.GetOrigin().z;
								float shift = z - floorf(z);
								matrix = Matrix4::Translate(0, 0, -shift) * matrix;
							} else {
								if (dropSound) {
									if (dist < 40.0F * 40.0F && !client->IsMuted()) {
										IAudioDevice& dev = client->GetAudioDevice();
										AudioParam param;
										param.referenceDistance = 0.6F;
										dev.Play(dropSound, lastMat.GetOrigin(), param);
									}

									dropSound = NULL;
								}
							}
						}
					} else if (lp.x != lp2.x &&
							((lp.y == lp2.y && lp.z == lp2.z) || !m->ClipWorld(lp2.x, lp.y, lp.z)))
						velocity.x = -velocity.x;
					else if (lp.y != lp2.y &&
					         ((lp.x == lp2.x && lp.z == lp2.z) || !m->ClipWorld(lp.x, lp2.y, lp.z)))
						velocity.y = -velocity.y;
					else
						return false;

					if (!onGround) {
						matrix = lastMat;
						velocity *= 0.2F;
						velocity += RandomAxis() * 0.1F;
						rotAxis = RandomAxis().Normalize();
						rotSpeed *= 0.2F;
					}
				}
			}
			return true;
		}

		void GunCasing::Render3D() {
			ModelRenderParam param;
			param.matrix = matrix * Matrix4::Scale(0.007F);

			if (groundTime > 1.0F) { // sink
				float move = (groundTime - 1.0F) * 0.05F;
				param.matrix = Matrix4::Translate(0, 0, move) * param.matrix;
			}

			renderer.RenderModel(*model, param);
		}
	} // namespace client
} // namespace spades
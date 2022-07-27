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

#include "FallingBlock.h"
#include "Client.h"
#include "GameMap.h"
#include "IModel.h"
#include "IAudioChunk.h"
#include "IAudioDevice.h"
#include "IRenderer.h"
#include "ParticleSpriteEntity.h"
#include "SmokeSpriteEntity.h"
#include "World.h"
#include <Core/Debug.h>
#include <Core/Exception.h>
#include <limits.h>

namespace spades {
	namespace client {

		FallingBlock::FallingBlock(Client* client, std::vector<IntVector3> blocks) : client(client) {
			if (blocks.empty())
				SPRaise("No block given");

			// find min/max
			int maxX, maxY, maxZ;
			maxX = maxY = maxZ = -1;
			int minX, minY, minZ;
			minX = minY = minZ = INT_MAX;

			uint64_t xSum, ySum, zSum;
			xSum = ySum = zSum = 0;

			numBlocks = (int)blocks.size();

			for (const auto& v : blocks) {
				if (v.x < minX) minX = v.x;
				if (v.y < minY) minY = v.y;
				if (v.z < minZ) minZ = v.z;
				if (v.x > maxX) maxX = v.x;
				if (v.y > maxY) maxY = v.y;
				if (v.z > maxZ) maxZ = v.z;

				xSum += v.x;
				ySum += v.y;
				zSum += v.z;
			}

			const Handle<GameMap>& map = client->GetWorld()->GetMap();
			SPAssert(map);

			// build voxel model
			vmodel = new VoxelModel(maxX - minX + 1, maxY - minY + 1, maxZ - minZ + 1);

			for (const auto& v : blocks) {
				uint32_t col = map->GetColor(v.x, v.y, v.z);
				col = map->GetColorJit(col); // jit the colour
				col &= 0xFFFFFF; // use the default material
				vmodel->SetSolid(v.x - minX, v.y - minY, v.z - minZ, col);
			}

			// center of gravity
			Vector3 origin;
			origin.x = (float)minX - (float)xSum / (float)numBlocks;
			origin.y = (float)minY - (float)ySum / (float)numBlocks;
			origin.z = (float)minZ - (float)zSum / (float)numBlocks;
			vmodel->SetOrigin(origin);

			Vector3 matTrans = MakeVector3((float)minX, (float)minY, (float)minZ);
			matTrans += 0.5F; // voxelmodel's (0,0,0) origins on block center
			matTrans -= origin; // cancel origin
			matrix = Matrix4::Translate(matTrans);

			// build renderer model
			model = client->GetRenderer().CreateModel(*vmodel).Unmanage();

			velocity = {0.0F, 0.0F, 0.0F};
			rotation = SampleRandom() & 3;
			alpha = 1.0F;
		}

		FallingBlock::~FallingBlock() {
			model->Release();
			vmodel->Release();
		}

		bool FallingBlock::Update(float dt) {
			alpha -= 1.0F / 5.0F * dt;

			Vector3 origin = matrix.GetOrigin();

			float dist = (client->GetLastSceneDef().viewOrigin - origin).GetLength2D();
			if (dist > FOG_DISTANCE)
				return false;

			bool usePrecisePhysics = false;
			if (dist < 16.0F && numBlocks < 512)
				usePrecisePhysics = true;

			if (MoveBlock(dt) == 2) {
				if (!client->IsMuted() && dist < 40.0F) {
					IAudioDevice& dev = client->GetAudioDevice();
					Handle<IAudioChunk> c = dev.RegisterSound("Sounds/Player/Bounce.opus");
					dev.Play(c.GetPointerOrNull(), origin, AudioParam());
				}
			}

			// destroy
			if (alpha <= 0.0F) {
				int w = vmodel->GetWidth();
				int h = vmodel->GetHeight();
				int d = vmodel->GetDepth();

				Matrix4 vmat = matrix * Matrix4::Translate(vmodel->GetOrigin());

				// block center
				Vector3 vmOrigin = vmat.GetOrigin();
				Vector3 vmAxis1 = vmat.GetAxis(0);
				Vector3 vmAxis2 = vmat.GetAxis(1);
				Vector3 vmAxis3 = vmat.GetAxis(2);

				// this could get annoying with some server scripts..
				client->PlayBlockDestroySound(vmOrigin.Floor());

				Handle<IImage> img = client->GetRenderer().RegisterImage("Gfx/White.tga");

				auto* getRandom = SampleRandomFloat;

				for (int x = 0; x < w; x++) {
					Vector3 p1 = vmOrigin + vmAxis1 * (float)x;
					for (int y = 0; y < h; y++) {
						Vector3 p2 = p1 + vmAxis2 * (float)y;
						for (int z = 0; z < d; z++) {
							if (!vmodel->IsSolid(x, y, z))
								continue;
							// inner voxel?
							if (x > 0 && y > 0 && z > 0 && x < w - 1 && y < h - 1 && z < d - 1 &&
							    vmodel->IsSolid(x - 1, y, z) && vmodel->IsSolid(x + 1, y, z) &&
							    vmodel->IsSolid(x, y - 1, z) && vmodel->IsSolid(x, y + 1, z) &&
							    vmodel->IsSolid(x, y, z - 1) && vmodel->IsSolid(x, y, z + 1))
								continue;

							uint32_t col = vmodel->GetColor(x, y, z);
							Vector4 color = ConvertColorRGBA(IntVectorFromColor(col));

							Vector3 p3 = p2 + vmAxis3 * (float)z;

							for (int i = 0; i < 4; i++) {
								auto ent = stmp::make_unique<ParticleSpriteEntity>(
									*client, img.GetPointerOrNull(), color);
								ent->SetTrajectory(p3, RandomAxis() * 13.0F, 1.0F, 0.6F);
								ent->SetRadius(0.35F + getRandom() * getRandom() * 0.1F);
								ent->SetLifeTime(2.0F, 0.0F, 1.0F);
								if (usePrecisePhysics)
									ent->SetBlockHitAction(BlockHitAction::BounceWeak);
								client->AddLocalEntity(std::move(ent));
							}

							{
								auto ent = stmp::make_unique<SmokeSpriteEntity>(
									*client, color, 70.0F);
								ent->SetTrajectory(p3, RandomAxis() * 0.2F, 1.0F, 0.0F);
								ent->SetRotation(getRandom() * M_PI_F * 2.0F);
								ent->SetRadius(1.0F, 0.5F);
								ent->SetBlockHitAction(BlockHitAction::Ignore);
								ent->SetLifeTime(1.0F + getRandom() * 0.5F, 0.0F, 1.0F);
								client->AddLocalEntity(std::move(ent));
							}
						}
					}
				}

				return false;
			}

			return true;
		}

		int FallingBlock::MoveBlock(float fsynctics) {
			SPADES_MARK_FUNCTION();

			Matrix4 lastMat = matrix;

			matrix = matrix * Matrix4::Rotate(MakeVector3(1, 0, 0),
				((rotation & 1) ? 1 : -1) * fsynctics * DEG2RAD(45.0F));
			matrix = matrix * Matrix4::Rotate(MakeVector3(0, 1, 0),
				((rotation & 2) ? 1 : -1) * fsynctics * DEG2RAD(45.0F));
			matrix = Matrix4::Translate(velocity * fsynctics) * matrix;
			velocity.z += fsynctics * 32.0F;

			// Collision
			IntVector3 lp = matrix.GetOrigin().Floor();

			const Handle<GameMap>& map = client->GetWorld()->GetMap();
			SPAssert(map);

			int ret = 0;
			if (map->ClipWorld(lp.x, lp.y, lp.z)) {
				ret = 1; // hit a wall

				if (fabsf(velocity.GetLength()) > BOUNCE_SOUND_THRESHOLD)
					ret = 2; // play sound

				IntVector3 lp2 = lastMat.GetOrigin().Floor();
				if (lp.z != lp2.z &&
				    ((lp.x == lp2.x && lp.y == lp2.y) || !map->ClipWorld(lp.x, lp.y, lp2.z)))
					velocity.z = -velocity.z;
				else if (lp.x != lp2.x &&
				         ((lp.y == lp2.y && lp.z == lp2.z) || !map->ClipWorld(lp2.x, lp.y, lp.z)))
					velocity.x = -velocity.x;
				else if (lp.y != lp2.y &&
				         ((lp.x == lp2.x && lp.z == lp2.z) || !map->ClipWorld(lp.x, lp2.y, lp.z)))
					velocity.y = -velocity.y;

				matrix = lastMat;  // set back to old position
				velocity *= 0.46F; // lose some velocity due to friction
				rotation++;
				rotation &= 3;
				alpha -= 0.36F;
			}

			if (alpha <= 0.0F)
				ret = 2; // play sound

			return ret;
		}

		void FallingBlock::Render3D() {
			ModelRenderParam param;
			param.ghost = true;
			param.opacity = std::max(0.25F, (1.0F * alpha));
			param.matrix = matrix;
			client->GetRenderer().RenderModel(*model, param);
		}
	} // namespace client
} // namespace spades
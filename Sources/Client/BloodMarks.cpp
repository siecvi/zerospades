/*
 Copyright (c) 2021 yvt
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
#include <array>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <tuple>
#include <vector>

#include <Core/Math.h>
#include <Core/Settings.h>
#include <Core/TMPUtils.h>
#include <Core/VoxelModel.h>

#include "BloodMarks.h"

#include "Client.h"
#include "GameMap.h"
#include "IRenderer.h"
#include "World.h"

using std::array;
using std::get;
using std::size_t;
using std::tuple;
using std::uint8_t;
using std::vector;
using stmp::optional;

SPADES_SETTING(cg_blood);

namespace spades {
	namespace client {
		namespace {
			constexpr size_t kNumFloorMarkVariations = 8;
			constexpr size_t kNumWallMarkVariations = 4;
			constexpr size_t kNumWallMarkAnimationFrames = 16;
			constexpr int kMarkResolution = 64;

			constexpr size_t kNumMarkSoftLimit = 64;
			constexpr size_t kNumMarkHardLimit = 32; // + `kNumMarkSoftLimit`
			constexpr size_t kLocalPlayerMarkAllocation = 48;
			constexpr size_t kNumEmergencySlots = 16;

			static_assert(kLocalPlayerMarkAllocation < kNumMarkSoftLimit,
			              "kLocalPlayerMarkAllocation < kNumMarkSoftLimit");

			struct Assets {
				static const Assets& Get();
				Assets();

				vector<Handle<VoxelModel>> models;
				size_t lowSpeedFloorMarkStart;
				size_t highSpeedFloorMarkStart;
				size_t wallMarkStart;

				array<Matrix4, 12> orientations;

				size_t SampleModelIndexForLowSpeedFloorMark() const {
					return this->lowSpeedFloorMarkStart +
					       SampleRandomInt<size_t>(0, kNumFloorMarkVariations - 1);
				}
				size_t SampleModelIndexForHighSpeedFloorMark() const {
					return this->highSpeedFloorMarkStart +
					       SampleRandomInt<size_t>(0, kNumFloorMarkVariations - 1);
				}

				size_t SampleModelIndexStartForWallMark() const {
					return this->wallMarkStart +
					       SampleRandomInt<size_t>(0, kNumWallMarkVariations - 1) *
					         kNumWallMarkAnimationFrames;
				}
			};

			struct Mark {
				Vector3 position;
				/** An index into `Assets::orientations` */
				size_t orientationIndex;
				/** An index into `Assets::models` */
				size_t modelIndexStart;
				size_t modelIndexLenM1;
				bool byLocalPlayer;
				array<IntVector3, 4> anchors;
				float fade = 0.0F;
				float time = 0.0F;
				float score;
			};

			bool IsBloodMarksEnabled() { return cg_blood.operator int() >= 2; }

			/** Project a particle affected by gravity and return the position where the collision
			 * with the terrain occurs */
			GameMap::RayCastResult ProjectArc(Vector3 position, Vector3 velocity, float maxTime, GameMap& map) {
				GameMap::RayCastResult result;
				result.hit = false;

				Vector3 startPosition = position;

				constexpr float kTimeStep = 1.0F / 60.0F;
				for (float time = 0.0F; time < maxTime; time += kTimeStep) {
					float const newTime = time + kTimeStep;
					Vector3 newPosition = startPosition + velocity * time;
					newPosition.z += newTime * newTime * (32.0F / 2.0F);

					auto const direction = newPosition - position;
					int const maxSteps = direction.Floor().GetManhattanLength() + 1;
					
					result = map.CastRay2(position, direction, maxSteps);
					if (result.hit) { // Filter out-of-range hit
						float hitDistance = Vector3::Dot(result.hitPos - position, direction);
						float rayLength = Vector3::Dot(direction, direction);
						if (hitDistance > rayLength)
							result.hit = false;
					}

					if (result.hit)
						break;

					position = newPosition;
				}

				return result;
			}
		} // namespace

		struct BloodMarksImpl {
			Client& client;
			const Assets& assets;
			vector<Handle<IModel>> models;
			vector<optional<Mark>> marks;
			vector<size_t> tmpIndices;
			size_t markNextIndex = 0;

			BloodMarksImpl(Client& client) : client{client}, assets{Assets::Get()} {}
		};

		const Assets& Assets::Get() {
			// Create it on first use, not during the program startup
			static Assets global;
			return global;
		}

		Assets::Assets() {
			struct Simulator {
				array<array<uint8_t, kMarkResolution>, kMarkResolution> heightMap;

				void ClearAndRespatter(bool highSpeed) {
					heightMap = decltype(heightMap){};

					if (highSpeed) {
						for (size_t k = 0; k < 20; ++k) {
							// Approximated gaussian distribution
							float x = SampleRandomFloat() + SampleRandomFloat() + SampleRandomFloat();
							float y = SampleRandomFloat() + SampleRandomFloat() + SampleRandomFloat();
							int quantity = SampleRandomInt(1, 4);
							x = (x / 3.0F) * float(kMarkResolution - 2) + 1.0F;
							y = (y / 3.0F) * float(kMarkResolution - 2) + 1.0F;

							x += (float(kMarkResolution) * 0.5f - x) * (float(quantity) / 20.0F);
							y += (float(kMarkResolution) * 0.5f - y) * (float(quantity) / 20.0F);

							size_t ix = size_t(x) % kMarkResolution,
							       iy = size_t(y) % kMarkResolution;
							heightMap.at(iy).at(ix) += quantity;
						}
					} else {
						for (size_t k = 0; k < 6; ++k) {
							// Approximated gaussian distribution
							float x = SampleRandomFloat() + SampleRandomFloat() +
							          SampleRandomFloat() + SampleRandomFloat() +
							          SampleRandomFloat();
							float y = SampleRandomFloat() + SampleRandomFloat() +
							          SampleRandomFloat() + SampleRandomFloat() +
							          SampleRandomFloat();
							int quantity = SampleRandomInt(1, 12);
							x = (x / 5.0F) * float(kMarkResolution - 2) + 1.0F;
							y = (y / 5.0F) * float(kMarkResolution - 2) + 1.0F;

							x += (float(kMarkResolution) * 0.5f - x) * (float(quantity) / 15.0F);
							y += (float(kMarkResolution) * 0.5f - y) * (float(quantity) / 15.0F);

							size_t ix = size_t(x) % kMarkResolution,
							       iy = size_t(y) % kMarkResolution;
							heightMap.at(iy).at(ix) += quantity;
						}
					}
				}

				void Update(bool inclined) {
					auto const oldHeightMap = this->heightMap;
					for (size_t y = 1; y < kMarkResolution - 1; ++y) {
						for (size_t x = 1; x < kMarkResolution - 1; ++x) {
							if (oldHeightMap.at(y).at(x) > 1) {
								heightMap.at(y).at(x)--;
								if (inclined && SampleRandomInt<size_t>(0, 2) > 0) {
									heightMap.at(y + 1).at(x)++;
								} else {
									// Purposefully utilizes the wrap-around behavior of
									// unsigned types
									size_t dir = SampleRandomInt<size_t>(0, 3);
									size_t dx = size_t(dir & 2) - size_t(1), dy = 0;
									if (dir & 1) {
										dy = dx;
										dx = 0;
									}
									heightMap.at(y + dx).at(x + dy)++;
								}
							}
						}
					}
				}

				Handle<VoxelModel> ToModel() {
					auto model = Handle<VoxelModel>::New(kMarkResolution, 1, kMarkResolution);
					for (size_t y = 0; y < kMarkResolution; ++y)
					for (size_t x = 0; x < kMarkResolution; ++x) {
						if (heightMap.at(y).at(x) > 0)
							model->SetSolid(x, 0, y, 0);
					}

					model->SetOrigin(Vector3{float(kMarkResolution) * -0.5F, 0.0F,
					                         float(kMarkResolution) * -0.5F});
					return model;
				}
			};

			Simulator sim;

			lowSpeedFloorMarkStart = models.size();
			for (size_t i = 0; i < kNumFloorMarkVariations; ++i) {
				sim.ClearAndRespatter(false);
				for (int k = 0; k < 20; ++k)
					sim.Update(false);

				this->models.push_back(sim.ToModel());
			}

			highSpeedFloorMarkStart = models.size();
			for (size_t i = 0; i < kNumFloorMarkVariations; ++i) {
				sim.ClearAndRespatter(true);
				for (int k = 0; k < 20; ++k)
					sim.Update(false);

				this->models.push_back(sim.ToModel());
			}

			wallMarkStart = models.size();
			for (size_t i = 0; i < kNumWallMarkVariations; ++i) {
				sim.ClearAndRespatter(true);
				for (size_t k = 0; k < kNumWallMarkAnimationFrames; ++k) {
					sim.Update(true);
					this->models.push_back(sim.ToModel());
				}
			}

			// Wall
			orientations.at(0) = Matrix4::Translate(0.0F, -0.2F, 0.0F); // make it just visible

			// Floor and ceiling
			orientations.at(4) = Matrix4::Rotate(Vector3{1, 0, 0}, -M_PI_F * 0.5F) *
			                     Matrix4::Translate(0.0F, -0.2F, 0.0F);
			orientations.at(8) = Matrix4::Rotate(Vector3{1, 0, 0}, M_PI_F * 0.5F) *
			                     Matrix4::Translate(0.0F, -0.2F, 0.0F);

			for (size_t i = 0; i < 12; i += 4) {
				for (size_t k = 1; k < 4; ++k) {
					orientations.at(i + k) =
					  Matrix4::Rotate(Vector3{0, 0, 1}, M_PI_F * 0.5F * float(k)) *
					  orientations.at(i);
				}
			}

			for (size_t i = 0; i < 12; ++i) {
				orientations.at(i) =
				  Matrix4::Scale(1.0F / float(kMarkResolution)) * orientations.at(i);
			}
		}

		BloodMarks::BloodMarks(Client& client) : impl{new BloodMarksImpl{client}} {
			// Materialize the models
			BloodMarksImpl& impl = *this->impl;
			const Assets& assets = impl.assets;
			std::transform(assets.models.begin(), assets.models.end(),
			               std::back_inserter(impl.models), [&](Handle<VoxelModel> const& model) {
				               return client.GetRenderer().CreateModel(*model);
			               });

			// Reserve slots
			for (size_t i = 0; i < kNumMarkSoftLimit + kNumMarkHardLimit + kNumEmergencySlots; ++i)
				impl.marks.emplace_back();
		}

		BloodMarks::~BloodMarks() {}

		void BloodMarks::Clear() {
			for (auto& e : this->impl->marks)
				e.reset();
		}

		void BloodMarks::Spatter(const Vector3& position, const Vector3& velocity, bool byLocal) {
			BloodMarksImpl& impl = *this->impl;
			Client& client = impl.client;
			if (!client.GetWorld() || !client.GetWorld()->GetMap() || !IsBloodMarksEnabled())
				return;

			Handle<GameMap> map = client.GetWorld()->GetMap();

			bool isHighSpeed = velocity.GetLength() > 3.0F;

			array<float, 5> energyDistribution;
			for (float& e : energyDistribution)
				e = SampleRandomFloat() + 0.3F;

			{
				float const scale =
				  float(energyDistribution.size()) /
				  std::accumulate(energyDistribution.begin(), energyDistribution.end(), 0.0F);
				for (float& e : energyDistribution)
					e *= scale;
			}

			for (const float& e : energyDistribution) {
				for (int attempt = 0; attempt < 3; ++attempt) {
					// Draw a trajectory
					Vector3 particleVelocity = velocity;
					particleVelocity *= e;
					particleVelocity += RandomAxis() * (velocity.GetLength() * 0.2F);

					GameMap::RayCastResult const res = ProjectArc(position, particleVelocity, 1.4F, *map);
					if (!res.hit)
						continue;
					if (res.hitBlock.z >= 62)
						break; // Water

					// Find the anchor voxels
					array<IntVector3, 4> anchors;
					anchors.at(0) = (res.hitPos - 0.5F).Floor();
					if (res.normal.x != 0)
						anchors.at(0).x = res.hitBlock.x;
					if (res.normal.y != 0)
						anchors.at(0).y = res.hitBlock.y;
					if (res.normal.z != 0)
						anchors.at(0).z = res.hitBlock.z;

					for (size_t i = 1; i < 4; ++i) {
						size_t bits = i;
						if (res.normal.x != 0)
							bits = (bits << 1);
						else if (res.normal.y != 0)
							bits = ((bits | (bits << 1)) & 0B101);

						anchors.at(i) = anchors.at(0);
						anchors.at(i).x += int(bits) & 1;
						anchors.at(i).y += int(bits >> 1) & 1;
						anchors.at(i).z += int(bits >> 2) & 1;
					}

					// Are the anchor voxels present
					bool const anchorPresent =
					  std::all_of(anchors.begin(), anchors.end(), [&](const IntVector3& p) {
						  return map->IsSolidWrapped(p.x, p.y, p.z);
					  });
					if (!anchorPresent)
						continue;

					// Place the new mark
					bool found = false;
					for (size_t i = 0; i < impl.marks.size(); ++i) {
						if (!impl.marks.at(impl.markNextIndex)) {
							found = true;
							break;
						}

						if ((++impl.markNextIndex) == impl.marks.size())
							impl.markNextIndex = 0;
					}

					if (!found)
						return; // Out of slots

					// Fill the slot
					impl.marks.at(impl.markNextIndex).reset(Mark{}); // FIXME: #972

					Mark& mark = impl.marks.at(impl.markNextIndex).value();
					mark.position = res.hitPos;
					mark.anchors = anchors;
					mark.byLocalPlayer = byLocal;

					if (res.normal.z != 0) {
						mark.orientationIndex = (res.normal.z > 0) ? SampleRandomInt<size_t>(8, 11)
						                                           : SampleRandomInt<size_t>(4, 7);
						mark.modelIndexStart =
						  isHighSpeed ? impl.assets.SampleModelIndexForHighSpeedFloorMark()
						              : impl.assets.SampleModelIndexForLowSpeedFloorMark();
						mark.modelIndexLenM1 = 0;
					} else {
						if (res.normal.y > 0)
							mark.orientationIndex = 0;
						else if (res.normal.y < 0)
							mark.orientationIndex = 2;
						else if (res.normal.x > 0)
							mark.orientationIndex = 3;
						else
							mark.orientationIndex = 1;

						mark.modelIndexStart = impl.assets.SampleModelIndexStartForWallMark();
						mark.modelIndexLenM1 = kNumWallMarkAnimationFrames - 1;
					}

					break;
				}
			}
		}

		void BloodMarks::Update(float dt) {
			BloodMarksImpl& impl = *this->impl;
			Client& client = impl.client;
			if (!client.GetWorld() || !client.GetWorld()->GetMap() || !IsBloodMarksEnabled()) {
				this->Clear();
				return;
			}

			Handle<GameMap> const map = client.GetWorld()->GetMap();

			vector<stmp::optional<Mark>>& marks = impl.marks;

			vector<size_t>& tmpIndices = impl.tmpIndices;
			tmpIndices.resize(marks.size());

			// Evaluate each mark's importance for eviction
			SceneDefinition const sceneDef = client.GetLastSceneDef();
			for (auto& slot : marks) {
				if (slot) {
					Mark& mark = slot.value();

					float distance = (mark.position - sceneDef.viewOrigin).GetLength();
					if (distance > 150.0F) // Distance culled
						distance += (distance - 150.0F) * 10.0F;

					// Looking away?
					float dot = Vector3::Dot((mark.position - sceneDef.viewOrigin).Normalize(),
					                         sceneDef.viewAxis[2]);
					dot = std::max(0.5F - dot, 0.0F) * 20.0F;

					mark.score = distance + dot + mark.fade * 50.0F + mark.time * 2.0F;
				}
			}

			// Hard eviction
			size_t numMarks = std::count_if(
			  marks.begin(), marks.end(), [](const stmp::optional<Mark>& slot) { return !!slot; });
			if (numMarks > kNumMarkSoftLimit + kNumMarkHardLimit) {
				{
					size_t outIndex = 0;
					for (size_t i = 0; i < marks.size(); ++i) {
						if (marks.at(i))
							tmpIndices.at(outIndex++) = i;
					}
					assert(outIndex == numMarks);
				}

				// Choose the `numEvicted` elements with the largest `score`s
				// ("largest" hence the reversed comparer)
				size_t numEvicted = numMarks - (kNumMarkSoftLimit + kNumMarkHardLimit);
				std::partial_sort(tmpIndices.begin(), tmpIndices.begin() + numEvicted,
				                  tmpIndices.begin() + numMarks,
				                  [&](const size_t& a, const size_t& b) {
					                  return marks.at(a).get().score > marks.at(b).get().score;
				                  });

				for (size_t i = 0; i < numEvicted; ++i)
					marks.at(tmpIndices.at(i)).reset();

				numMarks = kNumMarkSoftLimit + kNumMarkHardLimit;
			}

			// Soft eviction
			size_t const numByLocalPlayerMarks =
			  std::count_if(marks.begin(), marks.end(), [](const stmp::optional<Mark>& slot) {
				  return slot && slot.get().byLocalPlayer;
			  });

			if (numByLocalPlayerMarks > kLocalPlayerMarkAllocation) {
				// A certain number of slots are reserved for by-local-player marks. If there are
				// more than that number of by-local-player marks, they are counted as non-by-local-
				// player marks.
				{
					size_t outIndex = 0;
					for (size_t i = 0; i < marks.size(); ++i) {
						if (marks.at(i) && marks.at(i).get().byLocalPlayer)
							tmpIndices.at(outIndex++) = i;
					}
					assert(outIndex == numByLocalPlayerMarks);
				}

				// Choose the `numExceeded` elements with the largest `score`s
				// ("largest" hence the reversed comparer)
				size_t const numExceeded = numByLocalPlayerMarks - kLocalPlayerMarkAllocation;
				std::partial_sort(tmpIndices.begin(), tmpIndices.begin() + numExceeded,
				                  tmpIndices.begin() + numByLocalPlayerMarks,
				                  [&](const size_t& a, const size_t& b) {
					                  return marks.at(a).get().score > marks.at(b).get().score;
				                  });

				// Assign negative infinity to the `kLocalPlayerMarkAllocation` elements with
				// the least `score`s
				for (size_t i = numExceeded; i < numByLocalPlayerMarks; ++i)
					marks.at(tmpIndices.at(i)).get().score = -INFINITY;
			}

			if (numMarks > kNumMarkSoftLimit) {
				{
					size_t outIndex = 0;
					for (size_t i = 0; i < marks.size(); ++i) {
						if (marks.at(i))
							tmpIndices.at(outIndex++) = i;
					}
					assert(outIndex == numMarks);
				}

				// Choose the `numEvicted` elements with the largest `score`s
				// ("largest" hence the reversed comparer)
				size_t const numEvicted = numMarks - kNumMarkSoftLimit;
				std::partial_sort(tmpIndices.begin(), tmpIndices.begin() + numEvicted,
				                  tmpIndices.begin() + numMarks,
				                  [&](const size_t& a, const size_t& b) {
					                  return marks.at(a).get().score > marks.at(b).get().score;
				                  });

				for (size_t i = 0; i < numEvicted; ++i) {
					Mark& mark = marks.at(tmpIndices.at(i)).value();
					mark.fade += dt;
					if (mark.fade >= 1.0F)
						marks.at(tmpIndices.at(i)).reset();
				}
			}

			// Miscellaneous
			for (auto& slot : marks) {
				if (slot) {
					Mark& mark = slot.value();
					mark.time += dt;

					// Check the anchor voxels' existence
					const auto& anchors = mark.anchors;
					bool const anchorPresent =
					  std::all_of(anchors.begin(), anchors.end(), [&](const IntVector3& p) {
						  return map->IsSolidWrapped(p.x, p.y, p.z);
					  });

					if (!anchorPresent)
						slot.reset();
				}
			}
		}

		void BloodMarks::Draw() {
			if (!IsBloodMarksEnabled())
				return;

			BloodMarksImpl& impl = *this->impl;
			Client& client = impl.client;
			IRenderer& r = client.GetRenderer();

			ModelRenderParam param;
			param.customColor = Vector3{0.7F, 0.07F, 0.01F}; // hemoglobin

			for (auto& slot : impl.marks) {
				if (slot) {
					Mark& mark = slot.value();

					// Decide the model for this mark
					float frame = std::min(std::max(mark.time * 5.0F, 0.0F), float(mark.modelIndexLenM1));
					size_t modelIndex = mark.modelIndexStart + size_t(frame);
					IModel& model = *impl.models.at(modelIndex);

					// Render the model
					param.matrix = impl.assets.orientations.at(mark.orientationIndex);

					if (mark.fade > 0.0F) {
						// Sink into the terrain
						// TODO: ... which turned out to be not a great idea
						param.matrix *= Matrix4::Translate(0.0F, mark.fade * -0.3F, 0.0F);
					}

					param.matrix = Matrix4::Translate(mark.position) * param.matrix;

					r.RenderModel(model, param);
				}
			}
		}
	} // namespace client
} // namespace spades
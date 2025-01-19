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

#pragma once

#include <vector>

#include "ILocalEntity.h"
#include <Core/Math.h>
#include <Core/VoxelModel.h>

namespace spades {
	namespace client {
		class IRenderer;
		class Client;
		class IModel;
		class IAudioChunk;
		class FallingBlock : public ILocalEntity {
			Client* client;
			IRenderer& renderer;
			IModel* model;
			VoxelModel* vmodel;
			Matrix4 matrix;
			Vector3 velocity;
			int rotDir;
			float time;
			int numBlocks;

			IAudioChunk* bounceSound;

		public:
			FallingBlock(Client*, IAudioChunk* bounceSound, std::vector<IntVector3> blocks);
			~FallingBlock();

			bool Update(float dt) override;
			void Render3D() override;
		};
	} // namespace client
} // namespace spades
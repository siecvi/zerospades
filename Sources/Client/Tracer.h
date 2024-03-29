//
//  Tracer.h
//  OpenSpades
//
//  Created by Tomoaki Kawada on 8/30/13.
//  Copyright (c) 2013 yvt.jp. All rights reserved.
//

#pragma once

#include "ILocalEntity.h"
#include <Core/Math.h>
#include <Core/RefCountedObject.h>

namespace spades {
	namespace client {
		class Client;
		class IImage;
		class Tracer : public ILocalEntity {
			Client& client;
			Handle<IImage> image;
			Vector3 startPos, dir;
			float length;
			float curDistance;
			float visibleLength;
			float velocity;
			bool firstUpdate;
			bool shotgun;

		public:
			Tracer(Client&, Vector3 p1, Vector3 p2, float bulletVel, bool shotgun);
			~Tracer();

			bool Update(float dt) override;
			void Render3D() override;
		};
	} // namespace client
} // namespace spades
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

#include "Grenade.h"
#include "GameMap.h"
#include "IWorldListener.h"
#include "PhysicsConstants.h"
#include "World.h"
#include <Core/Debug.h>

namespace spades {
	namespace client {
		Grenade::Grenade(World& w, Vector3 pos, Vector3 vel, float fuse) : world{w} {
			SPADES_MARK_FUNCTION();

			position = pos;
			velocity = vel;
			this->fuse = fuse;
			orientation = Quaternion{0.0F, 0.0F, 0.0F, 1.0F};
		}

		Grenade::~Grenade() { SPADES_MARK_FUNCTION(); }

		bool Grenade::Update(float dt) {
			SPADES_MARK_FUNCTION();

			fuse -= dt;
			if (fuse < 0.0F) {
				Explode();
				return true;
			}

			int ret = MoveGrenade(dt);
			if (ret == 2) {
				if (world.GetListener())
					world.GetListener()->GrenadeBounced(*this);
			} else if (ret == -1) {
				if (world.GetListener())
					world.GetListener()->GrenadeDroppedIntoWater(*this);
			}

			// do rotation
			float f = dt * 32.0F;
			Vector3 rotAxis = {0.0F, 0.0F, 0.0F};
			if (fabsf(velocity.x) > BOUNCE_SOUND_THRESHOLD ||
			    fabsf(velocity.y) > BOUNCE_SOUND_THRESHOLD) {
				rotAxis.x -= f * velocity.y * DEG2RAD(45);
				rotAxis.y += f * velocity.x * DEG2RAD(45);
				orientation = Quaternion::MakeRotation(rotAxis) * orientation;
				orientation = orientation.Normalize();
			}

			return false;
		}

		void Grenade::Explode() {
			SPADES_MARK_FUNCTION();

			if (world.GetListener())
				world.GetListener()->GrenadeExploded(*this);
		}

		int Grenade::MoveGrenade(float fsynctics) {
			SPADES_MARK_FUNCTION();

			Vector3 oldPos = position; // old position

			float f = fsynctics * 32.0F;
			velocity.z += fsynctics;
			position += velocity * f;

			Handle<GameMap> m = world.GetMap();

			// Collision
			IntVector3 lp = position.Floor();
			IntVector3 lp2 = oldPos.Floor();

			int ret = 0; // we didn't hit anything, no collision
			if (lp.z >= 63 && lp2.z < 63)
				ret = -1; // under water

			if (m->ClipWorld(lp.x, lp.y, lp.z)) {
				ret = 1; // hit a wall
				if (fabsf(velocity.x) > BOUNCE_SOUND_THRESHOLD ||
				    fabsf(velocity.y) > BOUNCE_SOUND_THRESHOLD ||
				    fabsf(velocity.z) > BOUNCE_SOUND_THRESHOLD)
					ret = 2; // play sound

				if (lp.z != lp2.z && ((lp.x == lp2.x && lp.y == lp2.y)
					|| !m->ClipWorld(lp.x, lp.y, lp2.z)))
					velocity.z = -velocity.z;
				else if (lp.x != lp2.x && ((lp.y == lp2.y && lp.z == lp2.z)
					|| !m->ClipWorld(lp2.x, lp.y, lp.z)))
					velocity.x = -velocity.x;
				else if (lp.y != lp2.y && ((lp.x == lp2.x && lp.z == lp2.z)
					|| !m->ClipWorld(lp.x, lp2.y, lp.z)))
					velocity.y = -velocity.y;

				position = oldPos; // set back to old position
				velocity *= 0.36F; // lose some velocity due to friction
			}

			return ret;
		}
	} // namespace client
} // namespace spades
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

#include "ParticleSpriteEntity.h"

#include "Fonts.h"
#include "GameMap.h"
#include "World.h"
#include <Core/Debug.h>

namespace spades {
	namespace client {
		ParticleSpriteEntity::ParticleSpriteEntity(Client& client, Handle<IImage> img, Vector4 col)
		    : renderer(client.GetRenderer()), image(img), color(col) {
			position = MakeVector3(0, 0, 0);
			velocity = MakeVector3(0, 0, 0);
			radius = 1.0F;
			radiusVelocity = 0.0F;
			angle = 0.0F;
			rotationVelocity = 0.0F;
			velocityDamp = 1.0F;
			gravityScale = 1.0F;
			lifetime = 1.0F;
			radiusDamp = 1.0F;
			time = 0.0F;
			fadeInDuration = 0.1F;
			fadeOutDuration = 0.5F;
			additive = false;
			blockHitAction = BlockHitAction::Delete;

			if (client.GetWorld())
				map = client.GetWorld()->GetMap();
			else
				map = NULL;
		}

		ParticleSpriteEntity::~ParticleSpriteEntity() {}

		void ParticleSpriteEntity::SetLifeTime(float lifeTime, float fadeIn, float fadeOut) {
			lifetime = lifeTime;
			fadeInDuration = fadeIn;
			fadeOutDuration = fadeOut;
		}
		void ParticleSpriteEntity::SetTrajectory(Vector3 pos, Vector3 vel, float damp, float grav) {
			position = pos;
			velocity = vel;
			velocityDamp = damp;
			gravityScale = grav;
		}

		void ParticleSpriteEntity::SetRotation(float initialAng, float angleVel) {
			angle = initialAng;
			rotationVelocity = angleVel;
		}

		void ParticleSpriteEntity::SetRadius(float initialRad, float radVel, float damp) {
			radius = initialRad;
			radiusVelocity = radVel;
			radiusDamp = damp;
		}

		bool ParticleSpriteEntity::Update(float dt) {
			SPADES_MARK_FUNCTION_DEBUG();

			time += dt;
			if (time > lifetime)
				return false;

			Vector3 lastPos = position; // old position

			position += velocity * dt;
			velocity.z += 32.0F * dt * gravityScale;

			// TODO: control clip action
			if (blockHitAction != BlockHitAction::Ignore && map) {
				IntVector3 lp = position.Floor();
				if (map->ClipWorld(lp.x, lp.y, lp.z)) {
					if (blockHitAction == BlockHitAction::Delete) {
						return false;
					} else {
						IntVector3 lp2 = lastPos.Floor();
						if (lp.z != lp2.z && ((lp.x == lp2.x && lp.y == lp2.y)
							|| !map->ClipWorld(lp.x, lp.y, lp2.z)))
							velocity.z = -velocity.z;
						else if (lp.x != lp2.x && ((lp.y == lp2.y && lp.z == lp2.z)
							|| !map->ClipWorld(lp2.x, lp.y, lp.z)))
							velocity.x = -velocity.x;
						else if (lp.y != lp2.y && ((lp.x == lp2.x && lp.z == lp2.z)
							|| !map->ClipWorld(lp.x, lp2.y, lp.z)))
							velocity.y = -velocity.y;

						position = lastPos; // set back to old position
						velocity *= 0.46F;  // lose some velocity due to friction
						radius *= 0.75F;
					}
				}
			}

			// radius
			if (radiusVelocity != 0.0F)
				radius += radiusVelocity * dt;
			if (rotationVelocity != 0.0F)
				angle += rotationVelocity * dt;
			if (velocityDamp != 1.0F)
				velocity *= powf(velocityDamp, dt);
			if (radiusDamp != 1.0F)
				radiusVelocity *= powf(radiusDamp, dt);

			return true;
		}

		void ParticleSpriteEntity::Render3D() {
			SPADES_MARK_FUNCTION_DEBUG();

			float fade = 1.0F;
			if (time < fadeInDuration)
				fade *= time / fadeInDuration;
			if (time > lifetime - fadeOutDuration)
				fade *= (lifetime - time) / fadeOutDuration;

			Vector4 col = color;
			col.w *= fade;

			// premultiplied alpha!
			col.x *= col.w;
			col.y *= col.w;
			col.z *= col.w;

			if (additive)
				col.w = 0.0F;

			renderer.SetColorAlphaPremultiplied(col);
			renderer.AddSprite(*image, position, radius, angle);
		}

		void ParticleSpriteEntity::SetImage(Handle<IImage> newImage) { image = newImage; }
	} // namespace client
} // namespace spades
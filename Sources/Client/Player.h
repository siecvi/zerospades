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

#pragma once

#include <memory>

#include "PhysicsConstants.h"
#include <Core/Math.h>

namespace spades {
	namespace client {
		class World;
		class Weapon;

		struct PlayerInput {
			bool moveForward : 1;
			bool moveBackward : 1;
			bool moveLeft : 1;
			bool moveRight : 1;
			bool jump : 1;
			bool crouch : 1;
			bool sneak : 1;
			bool sprint : 1;

			PlayerInput()
			    : moveForward(false),
			      moveBackward(false),
			      moveLeft(false),
			      moveRight(false),
			      jump(false),
			      crouch(false),
			      sneak(false),
			      sprint(false) {}
		};

		struct WeaponInput {
			bool primary : 1;
			bool secondary : 1;
			WeaponInput() : primary(false), secondary(false) {}
		};

		class Player {
		public:
			enum ToolType { ToolSpade = 0, ToolBlock, ToolWeapon, ToolGrenade };
			struct HitBoxes {
				OBB3 torso;
				OBB3 limbs[3];
				OBB3 head;
			};

		private:
			World& world;

			Vector3 position;
			Vector3 velocity;
			Vector3 orientation;
			Vector3 eye;
			PlayerInput input;
			WeaponInput weapInput;
			bool airborne;
			bool wade;
			ToolType tool;

			WeaponType weaponType;
			std::unique_ptr<Weapon> weapon;
			int playerId;
			int teamId;
			IntVector3 color; // obsolete

			int health;
			int grenades;
			int blockStocks;
			IntVector3 blockColor;

			// for making footsteps
			float moveDistance;
			int moveSteps;

			float lastClimbTime;
			float lastJumpTime;

			// tools
			float nextSpadeTime;
			float nextDigTime;
			bool firstDig;
			float nextGrenadeTime;
			float nextBlockTime;
			bool holdingGrenade;
			float grenadeTime;
			bool blockCursorActive;
			bool blockCursorDragging;
			IntVector3 blockCursorPos;
			IntVector3 blockCursorDragPos;
			bool lastSingleBlockBuildSeqDone;
			float lastReloadingTime;

			bool pendingPlaceBlock;
			bool pendingRestockBlock;
			bool canPending;
			IntVector3 pendingPlaceBlockPos;

			// for local players, completion of reload is notified to client
			bool reloadingServerSide;

			float respawnTime;

			void MoveCorpse(float fsynctics);
			void MovePlayer(float fsynctics);
			void BoxClipMove(float fsynctics);
			bool TryUncrouch();

			void UseSpade(bool dig);
			void FireWeapon();
			void ThrowGrenade();

		public:
			Player(World&, int pId, WeaponType wType, 
				int tId, Vector3 pos, IntVector3 col);
			Player(const Player&) = delete;
			void operator=(const Player&) = delete;

			~Player();

			int GetId() { return playerId; }
			Weapon& GetWeapon();
			WeaponType GetWeaponType() { return weaponType; }
			int GetTeamId() { return teamId; }
			bool IsTeamMate(Player* p) { return teamId == p->teamId; }
			bool IsSpectator() { return teamId >= 2; }
			std::string GetTeamName();
			std::string GetName();
			int GetScore();
			IntVector3 GetColor();
			IntVector3 GetBlockColor() { return blockColor; }
			ToolType GetTool() { return tool; }
			bool IsLocalPlayer();

			PlayerInput GetInput() { return input; }
			WeaponInput GetWeaponInput() { return weapInput; }
			void SetInput(PlayerInput);
			void SetWeaponInput(WeaponInput);
			void SetTool(ToolType);
			void SetHeldBlockColor(IntVector3 c) { blockColor = c; }
			void PlayerJump();
			bool IsBlockCursorActive() { return blockCursorActive; }
			bool IsBlockCursorDragging() { return blockCursorDragging; }
			IntVector3 GetBlockCursorPos() { return blockCursorPos; }
			IntVector3 GetBlockCursorDragPos() { return blockCursorDragPos; }
			bool IsReadyToUseTool();
			bool IsToolSelectable(ToolType);

			// ammo counts
			int GetNumBlocks() { return blockStocks; }
			int GetNumGrenades() { return grenades; }
			void Reload();
			void ReloadDone(int clip, int stock);
			void Restock();
			void GotBlock();

			bool IsToolSpade() { return tool == ToolSpade; }
			bool IsToolBlock() { return tool == ToolBlock; }
			bool IsToolWeapon() { return tool == ToolWeapon; }
			bool IsToolGrenade() { return tool == ToolGrenade; }

			bool IsZoomed() { return tool == ToolWeapon && weapInput.secondary; }
			bool IsWalking() { return input.moveForward || input.moveBackward || input.moveLeft || input.moveRight; }

			bool IsAwaitingReloadCompletion() { return reloadingServerSide; }

			void RepositionPlayer(const Vector3&);
			void SetPosition(const Vector3&);
			void SetOrientation(const Vector3&);
			void SetVelocity(const Vector3&);
			void Turn(float longitude, float latitude);

			void SetHP(int hp, HurtType, Vector3);

			void SetWeaponType(WeaponType weap);
			void SetTeam(int tId) { teamId = tId; }
			void UseBlocks(int c) { blockStocks = std::max(blockStocks - c, 0); }

			/** makes player's health 0. */
			void KilledBy(KillType, Player& killer, int respawnTime);

			bool IsAlive() { return health > 0; }
			/** @return world time to respawn */
			float GetTimeToRespawn();
			/** Returns player's health (local player only) */
			int GetHealth() { return health; }

			Vector3 GetPosition() { return position; }
			Vector3 GetFront();
			Vector3 GetFront2D();
			Vector3 GetRight();
			Vector3 GetUp();
			Vector3 GetEye() { return eye; }
			Vector3 GetOrigin(); // actually not origin at all!
			Vector3 GetVelocity() { return velocity; }
			int GetMoveSteps() { return moveSteps; }

			World& GetWorld() { return world; }

			bool GetWade() { return GetOrigin().z > 62.0F; }
			bool IsOnGroundOrWade() {
				return (velocity.z >= 0.0F && velocity.z < 0.017F) && !airborne;
			}

			void Update(float dt);

			float GetTimeToNextSpade();
			float GetTimeToNextDig();
			float GetTimeToNextBlock();
			float GetTimeToNextGrenade();

			float GetToolPrimaryDelay();
			float GetToolSecondaryDelay();

			float GetSpadeAnimationProgress();
			float GetDigAnimationProgress();
			bool IsFirstDig() const { return firstDig; }

			float GetGrenadeCookTime();
			bool IsCookingGrenade() { return tool == ToolGrenade && holdingGrenade; }

			float GetWalkAnimationProgress() {
				return moveDistance * 0.5F + (float)(moveSteps)*0.5F;
			}

			// hit tests
			HitBoxes GetHitBoxes();

			/** Does approximated ray casting.
			 * @param dir normalized direction vector.
			 * @return true if ray may hit the player. */
			bool RayCastApprox(Vector3 start, Vector3 dir, float tolerance = 3.0F);

			bool OverlapsWith(const AABB3&);
			bool OverlapsWithBlock(const IntVector3&);
			bool Collision3D(IntVector3, float distance = 3.0F);
		};
	} // namespace client
} // namespace spades
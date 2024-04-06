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
 along with OpenSpades.	 If not, see <http://www.gnu.org/licenses/>.

 */

namespace spades {
	class ViewSMGSkin : BasicViewWeapon {
		private AudioDevice@ audioDevice;
		private Model@ gunModel;
		private Model@ magazineModel;
		private Model@ rearSightModel;
		private Model@ frontSightModel;
		private Model@ frontPinSightModel;
		private Model@ dotSightModel;

		private AudioChunk@[] fireSounds(4);
		private AudioChunk@ fireFarSound;
		private AudioChunk@ fireStereoSound;
		private AudioChunk@[] fireSmallReverbSounds(4);
		private AudioChunk@[] fireLargeReverbSounds(4);
		private AudioChunk@ reloadSound;

		// Constants

		// Attachment Points
		private Vector3 magazineAttachment = Vector3(0, 0, 0);
		private Vector3 rearSightAttachment = Vector3(0.04F, -9.0F, -4.9F);
		private Vector3 frontSightAttachment = Vector3(0.05F, 5.0F, -4.85F);
		private Vector3 frontPinSightAttachment = Vector3(0.025F, 5.0F, -4.875F);

		// Scales
		private float globalScale = 0.033F;
		private float magazineScale = 0.5F;
		private float rearSightScale = 0.08F;
		private float frontSightScale = 0.1F;
		private float frontPinSightScale = 0.05F;

		// A bunch of springs.
		private ViewWeaponSpring recoilVerticalSpring = ViewWeaponSpring(300, 24);
		private ViewWeaponSpring recoilBackSpring = ViewWeaponSpring(100, 16);
		private ViewWeaponSpring recoilRotationSpring = ViewWeaponSpring(50, 8);
		private ViewWeaponSpring horizontalSwingSpring = ViewWeaponSpring(100, 12);
		private ViewWeaponSpring verticalSwingSpring = ViewWeaponSpring(100, 12);
		private ViewWeaponSpring reloadPitchSpring = ViewWeaponSpring(150, 12, 0);
		private ViewWeaponSpring reloadRollSpring = ViewWeaponSpring(150, 16, 0);
		private ViewWeaponSpring reloadOffsetSpring = ViewWeaponSpring(150, 12, 0);
		private ViewWeaponSpring sprintSpring = ViewWeaponSpring(100, 10, 0);
		private ViewWeaponSpring raiseSpring = ViewWeaponSpring(200, 20, 1);

		// A bunch of events.
		private ViewWeaponEvent magazineTouched = ViewWeaponEvent();
		private ViewWeaponEvent magazineRemoved = ViewWeaponEvent();
		private ViewWeaponEvent magazineInserted = ViewWeaponEvent();
		private ViewWeaponEvent chargingHandlePulled = ViewWeaponEvent();

		// A bunch of states.
		private double lastSprintState = 0;
		private double lastRaiseState = 0;

		Matrix4 AdjustToReload(Matrix4 mat) {
			if (reloadProgress < 0.6) {
				reloadPitchSpring.desired = 0.6;
				reloadRollSpring.desired = 0.6;
			} else if (reloadProgress < 0.9) {
				reloadPitchSpring.desired = 0;
				reloadRollSpring.desired = -0.6;
			} else {
				reloadPitchSpring.desired = 0;
				reloadRollSpring.desired = 0;
			}

			if (magazineTouched.WasActivated()) {
				magazineTouched.Acknowledge();
				reloadPitchSpring.velocity = 4;
			}

			if (magazineRemoved.WasActivated()) {
				magazineRemoved.Acknowledge();
				reloadPitchSpring.velocity = -4;
			}

			if (magazineInserted.WasActivated()) {
				magazineInserted.Acknowledge();
				reloadPitchSpring.velocity = 8;
			}

			if (chargingHandlePulled.WasActivated()) {
				chargingHandlePulled.Acknowledge();
				reloadPitchSpring.velocity = 5;
				reloadOffsetSpring.velocity = 1;
			}

			float sp = 1.0F - AimDownSightStateSmooth;

			mat *= CreateEulerAnglesMatrix(Vector3(0, 0.6, 0) * reloadRollSpring.position * sp);
			mat *= CreateEulerAnglesMatrix(Vector3(-0.25, 0, 0) * reloadPitchSpring.position * sp);
			mat *= CreateTranslateMatrix(Vector3(0, -1, 0) * reloadOffsetSpring.position * sp);
			mat *= CreateTranslateMatrix(Vector3(0, 0, -globalScale) * reloadPitchSpring.position * sp);

			return mat;
		}

		Vector3 GetMagazineOffset() {
			Vector3 offsetPos = Vector3(0, -6, 8);

			if (reloadProgress < 0.2) {
				return magazineAttachment;
			} else if (reloadProgress < 0.25) {
				magazineRemoved.Activate();
				float per = Min(1.0, (reloadProgress-0.2) / (0.25-0.2));
				return Mix(magazineAttachment, offsetPos, SmoothStep(per));
			} else if (reloadProgress < 0.4) {
				return offsetPos;
			} else if (reloadProgress < 0.5) {
				float per = Min(1.0, (reloadProgress-0.4) / (0.5-0.4));
				return Mix(offsetPos, magazineAttachment, SmoothStep(per));
			} else {
				magazineInserted.Activate();
				return magazineAttachment;
			}
		}

		Vector3 GetLeftHandOffset() {
			Vector3 leftHandOffset = Vector3(1, 3, 0.5);
			Vector3 magazineOffset = GetMagazineOffset() + Vector3(0, 0, 4);

			Vector3 chargingHandleOffset = Vector3(4, 5, 0);
			Vector3 handlePullingOffset = chargingHandleOffset + Vector3(0, -6, 0);

			if (reloadProgress < 0.1) {
				float per = Min(1.0, reloadProgress / 0.1);
				return Mix(leftHandOffset, magazineOffset, SmoothStep(per));
			} else if (reloadProgress < 0.6) {
				magazineTouched.Activate();
				return magazineOffset;
			} else if (reloadProgress < 0.8) {
				float per = Min(1.0, (reloadProgress-0.6) / (0.8-0.6));
				return Mix(magazineOffset, chargingHandleOffset, SmoothStep(per));
			} else if (reloadProgress < 0.82) {
				return chargingHandleOffset;
			} else if (reloadProgress < 0.87) {
				chargingHandlePulled.Activate();
				float per = Min(1.0, (reloadProgress-0.82) / (0.87-0.82));
				return Mix(chargingHandleOffset, handlePullingOffset, SmoothStep(per));
			} else if (reloadProgress < 1.0) {
				float per = Min(1.0, (reloadProgress-0.87) / (1.0-0.87));
				return Mix(handlePullingOffset, leftHandOffset, SmoothStep(per));
			} else {
				return leftHandOffset;
			}
		}

		Vector3 GetRightHandOffset() {
			Vector3 rightHandOffset = Vector3(0, -8, 2);

			// sprint animation
			rightHandOffset -= Vector3(0, 3, 0.5) * sprintSpring.position;

			return rightHandOffset;
		}

		ViewSMGSkin(Renderer@ r, AudioDevice@ dev) {
			super(r);
			@audioDevice = dev;

			// load models
			@gunModel = renderer.RegisterModel("Models/Weapons/SMG/WeaponNoMagazine.kv6");
			@magazineModel = renderer.RegisterModel("Models/Weapons/SMG/Magazine.kv6");
			@rearSightModel = renderer.RegisterModel("Models/Weapons/SMG/RearSight.kv6");
			@frontSightModel = renderer.RegisterModel("Models/Weapons/SMG/FrontSight.kv6");
			@frontPinSightModel = renderer.RegisterModel("Models/Weapons/SMG/FrontPinSight.kv6");
			@dotSightModel = renderer.RegisterModel("Models/Weapons/SMG/DotSight.kv6");

			// load sounds
			@fireSounds[0] = dev.RegisterSound("Sounds/Weapons/SMG/V2Local1.opus");
			@fireSounds[1] = dev.RegisterSound("Sounds/Weapons/SMG/V2Local2.opus");
			@fireSounds[2] = dev.RegisterSound("Sounds/Weapons/SMG/V2Local3.opus");
			@fireSounds[3] = dev.RegisterSound("Sounds/Weapons/SMG/V2Local4.opus");
			@fireFarSound = dev.RegisterSound("Sounds/Weapons/SMG/FireFar.opus");
			@fireStereoSound = dev.RegisterSound("Sounds/Weapons/SMG/FireStereo.opus");
			@fireSmallReverbSounds[0] = dev.RegisterSound("Sounds/Weapons/SMG/V2AmbienceSmall1.opus");
			@fireSmallReverbSounds[1] = dev.RegisterSound("Sounds/Weapons/SMG/V2AmbienceSmall2.opus");
			@fireSmallReverbSounds[2] = dev.RegisterSound("Sounds/Weapons/SMG/V2AmbienceSmall3.opus");
			@fireSmallReverbSounds[3] = dev.RegisterSound("Sounds/Weapons/SMG/V2AmbienceSmall4.opus");
			@fireLargeReverbSounds[0] = dev.RegisterSound("Sounds/Weapons/SMG/V2AmbienceLarge1.opus");
			@fireLargeReverbSounds[1] = dev.RegisterSound("Sounds/Weapons/SMG/V2AmbienceLarge2.opus");
			@fireLargeReverbSounds[2] = dev.RegisterSound("Sounds/Weapons/SMG/V2AmbienceLarge3.opus");
			@fireLargeReverbSounds[3] = dev.RegisterSound("Sounds/Weapons/SMG/V2AmbienceLarge4.opus");
			@reloadSound = dev.RegisterSound("Sounds/Weapons/SMG/ReloadLocal.opus");

			// load images
			@scopeImage = renderer.RegisterImage("Gfx/SMG.png");

			raiseSpring.position = 1;
		}

		void Update(float dt) {
			BasicViewWeapon::Update(dt);

			recoilVerticalSpring.damping = Mix(16, 24, AimDownSightState);
			recoilBackSpring.damping = Mix(12, 20, AimDownSightState);
			recoilRotationSpring.damping = Mix(8, 16, AimDownSightState);

			recoilVerticalSpring.Update(dt);
			recoilBackSpring.Update(dt);
			recoilRotationSpring.Update(dt);

			horizontalSwingSpring.velocity += swing.x * 60 * dt * 2;
			verticalSwingSpring.velocity += swing.z * 60 * dt * 2;
			horizontalSwingSpring.Update(dt);
			verticalSwingSpring.Update(dt);

			reloadPitchSpring.Update(dt);
			reloadRollSpring.Update(dt);
			reloadOffsetSpring.Update(dt);

			sprintSpring.Update(dt);
			raiseSpring.Update(dt);

			bool isSprinting;
			if (sprintState >= 1)
				isSprinting = true;
			else if (sprintState > lastSprintState)
				isSprinting = true;
			else if (sprintState < lastSprintState)
				isSprinting = false;
			else if (sprintState <= 0)
				isSprinting = false;
			else
				isSprinting = false;
			lastSprintState = sprintState;
			sprintSpring.desired = isSprinting ? 1 : 0;

			bool isRaised;
			if (raiseState >= 1)
				isRaised = true;
			else if (raiseState > lastRaiseState)
				isRaised = true;
			else if (raiseState < lastRaiseState)
				isRaised = false;
			else if (raiseState <= 0)
				isRaised = false;
			else
				isRaised = false;
			lastRaiseState = raiseState;
			raiseSpring.desired = isRaised ? 0 : 1;
		}

		void WeaponFired() {
			BasicViewWeapon::WeaponFired();

			if (!IsMuted) {
				Vector3 origin = Vector3(0.4F, -0.3F, 0.5F);
				AudioParam param;
				param.volume = 8.0F;
				audioDevice.PlayLocal(fireSounds[GetRandom(fireSounds.length)], origin, param);

				param.volume = 8.0F * environmentRoom;
				audioDevice.PlayLocal((environmentSize < 0.5F)
					? fireSmallReverbSounds[GetRandom(fireSmallReverbSounds.length)]
					: fireLargeReverbSounds[GetRandom(fireLargeReverbSounds.length)],
					origin, param);

				param.referenceDistance = 4.0F;
				param.volume = 1.0F;
				audioDevice.PlayLocal(fireFarSound, origin, param);
				param.referenceDistance = 1.0F;
				audioDevice.PlayLocal(fireStereoSound, origin, param);
			}

			recoilVerticalSpring.velocity += 0.75;
			recoilBackSpring.velocity += 0.75;
			recoilRotationSpring.velocity += (GetRandom() * 2 - 1);
		}

		void ReloadingWeapon() {
			magazineTouched.Reset();
			magazineRemoved.Reset();
			magazineInserted.Reset();
			chargingHandlePulled.Reset();

			if (!IsMuted) {
				Vector3 origin = Vector3(0.4F, -0.3F, 0.5F);
				AudioParam param;
				param.volume = 0.2F;
				audioDevice.PlayLocal(reloadSound, origin, param);
			}
		}

		// (override BasicViewWeapon::GetZPos())
		float GetZPos() { return 0.2F - AimDownSightStateSmooth * 0.038F; }

		// (override BasicViewWeapon::GetViewWeaponMatrix())
		Matrix4 GetViewWeaponMatrix() {
			Matrix4 mat;

			float sp = 1.0F - AimDownSightStateSmooth;

			// sprint animation
			mat = CreateEulerAnglesMatrix(Vector3(0.3F, -0.1F, -0.55F) * sprintSpring.position * sp) * mat;
			mat = CreateTranslateMatrix(Vector3(0.23F, -0.05F, 0.15F) * sprintSpring.position * sp) * mat;

			// raise gun animation
			mat = CreateRotateMatrix(Vector3(0, 0, 1), raiseSpring.position * -1.3F * sp) * mat;
			mat = CreateRotateMatrix(Vector3(0, 1, 0), raiseSpring.position * 0.2F * sp) * mat;
			mat = CreateTranslateMatrix(Vector3(0.1F, -0.3F, 0.1F) * raiseSpring.position * sp) * mat;

			// recoil animation
			Vector3 recoilRot(0, 0, 0);
			recoilRot.x = -1.0F * recoilVerticalSpring.position;
			recoilRot.y = 0.3F * recoilRotationSpring.position;
			recoilRot.z = 0.3F * recoilRotationSpring.position;
			Vector3 recoilOffset = Vector3(0, 0, -0.1) * recoilVerticalSpring.position * sp;
			recoilOffset -= Vector3(0, 0.5, 0) * recoilBackSpring.position * (1.0F - readyState);
			mat = CreateEulerAnglesMatrix(recoilRot * sp) * mat;
			mat = mat * CreateTranslateMatrix(recoilOffset);

			Vector3 trans(0, 0, 0);
			trans += Vector3(-0.13F * sp, 0.5F, GetZPos());
			trans += swing * sp;
			mat = CreateTranslateMatrix(trans) * mat;

			// twist the gun when strafing
			Vector3 swingRot(0, 0, 0);
			swingRot.z += 2.0F * horizontalSwingSpring.position;
			swingRot.x -= 2.0F * verticalSwingSpring.position;
			mat = mat * CreateEulerAnglesMatrix(swingRot * sp);

			Vector3 pivot = Vector3(0.05F, 0.0F, 0.05F);
			Vector3 sightPos = (frontSightAttachment - pivot) * globalScale;
			mat = AdjustToAlignSight(mat, sightPos, AimDownSightStateSmooth);
			mat = AdjustToReload(mat);

			return mat;
		}

		// IWeaponSkin3 (override BasicViewWeapon::{get_MuzzlePosition, get_CaseEjectPosition})
		Vector3 MuzzlePosition { get { return eyeMatrix * GetViewWeaponMatrix() * Vector3(0.0F, 0.35F, -0.075F); } }
		Vector3 CaseEjectPosition { get { return eyeMatrix * GetViewWeaponMatrix() * Vector3(-0.025F, -0.175F, -0.1F); } }

		void Draw2D() {
			BasicViewWeapon::Draw2D();
		}

		void AddToScene() {
			if (cg_pngScope.BoolValue and AimDownSightStateSmooth > 0.99F) {
				LeftHandPosition = Vector3(0.0F, 0.0F, 0.0F);
				RightHandPosition = Vector3(0.0F, 0.0F, 0.0F);
				return;
			}

			Matrix4 mat = GetViewWeaponMatrix()
				* CreateScaleMatrix(globalScale);

			Vector3 leftHand, rightHand;
			leftHand = mat * GetLeftHandOffset();
			rightHand = mat * GetRightHandOffset();

			ModelRenderParam param;
			param.depthHack = true;

			Matrix4 weapMatrix = eyeMatrix * mat;

			// draw weapon
			param.matrix = weapMatrix;
			renderer.AddModel(gunModel, param);

			// draw magazine
			param.matrix = weapMatrix
				* CreateTranslateMatrix(GetMagazineOffset());
			renderer.AddModel(magazineModel, param);

			// draw sights
			param.matrix = weapMatrix
				* CreateTranslateMatrix(rearSightAttachment)
				* CreateScaleMatrix(rearSightScale);
			renderer.AddModel(rearSightModel, param); // rear

			param.matrix = weapMatrix
				* CreateTranslateMatrix(frontSightAttachment)
				* CreateScaleMatrix(frontSightScale);
			renderer.AddModel(frontSightModel, param); // front

			param.matrix = weapMatrix
				* CreateTranslateMatrix(frontPinSightAttachment)
				* CreateScaleMatrix(frontPinSightScale);
			renderer.AddModel(frontPinSightModel, param); // front pin
			renderer.AddModel(dotSightModel, param); // front pin (emissive)

			LeftHandPosition = leftHand;
			RightHandPosition = rightHand;
		}
	}

	IWeaponSkin@ CreateViewSMGSkin(Renderer@ r, AudioDevice@ dev) {
		return ViewSMGSkin(r, dev);
	}
}

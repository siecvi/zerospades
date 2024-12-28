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
	class ViewShotgunSkin : BasicViewWeapon {
		private AudioDevice@ audioDevice;
		private Model@ gunModel;
		private Model@ pumpModel;
		private Model@ rearSightModel;
		private Model@ frontSightModel;
		private Model@ dotSightModel;

		private AudioChunk@ fireSound;
		private AudioChunk@ fireFarSound;
		private AudioChunk@ fireStereoSound;
		private AudioChunk@ fireSmallReverbSound;
		private AudioChunk@ fireLargeReverbSound;
		private AudioChunk@ reloadSound;
		private AudioChunk@ cockSound;

		// Constants

		// Attachment Points
		private Vector3 pumpAttachment = Vector3(0, 0, 0);
		private Vector3 rearSightAttachment = Vector3(0.025F, -9.0F, -4.4F);
		private Vector3 frontSightAttachment = Vector3(0.025F, 8.5F, -4.4F);

		// Scales
		private float globalScale = 0.033F;
		private float rearSightScale = 0.05F;
		private float frontSightScale = 0.05F;

		// A bunch of springs.
		private ViewWeaponSpring recoilVerticalSpring = ViewWeaponSpring(200, 24);
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
		private ViewWeaponEvent pumpHandlePulled = ViewWeaponEvent();
		private ViewWeaponEvent shellInserted = ViewWeaponEvent();

		// A bunch of states.
		private double lastSprintState = 0;
		private double lastRaiseState = 0;

		Matrix4 AdjustToReload(Matrix4 mat) {
			float reload = reloadProgress * 0.5F;

			if (reload < 0.6) {
				reloadPitchSpring.desired = 0.4;
				reloadRollSpring.desired = 0.4;
			} else {
				reloadPitchSpring.desired = 0;
				reloadRollSpring.desired = 0;
			}

			if (shellInserted.WasActivated()) {
				shellInserted.Acknowledge();
				reloadPitchSpring.velocity = 5;
				reloadRollSpring.velocity = 5;
				reloadOffsetSpring.velocity = -1;
			}

			if (pumpHandlePulled.WasActivated()) {
				pumpHandlePulled.Acknowledge();
				reloadPitchSpring.velocity = 5;
				reloadRollSpring.velocity = -1;
				reloadOffsetSpring.velocity = 0.5;
			}

			float sp = 1.0F - AimDownSightStateSmooth;

			mat *= CreateEulerAnglesMatrix(Vector3(0, 0.6, 0) * reloadRollSpring.position * sp);
			mat *= CreateEulerAnglesMatrix(Vector3(-0.25, 0, 0) * reloadPitchSpring.position * sp);
			mat *= CreateTranslateMatrix(Vector3(0, -1, 0) * reloadOffsetSpring.position * sp);
			mat *= CreateTranslateMatrix(Vector3(0, 0, -globalScale) * reloadPitchSpring.position * sp);

			return mat;
		}

		Vector3 GetPumpOffset() {
			float reload = reloadProgress * 0.5F;

			Vector3 handlePullingOffset = pumpAttachment + Vector3(0, -2, 0);

			float tim = 1.0F - readyState;
			if (tim < 0.0F) {
				// might be right after reloading
				if (ammo >= clipSize and reload > 0.5F and reload < 1.0F) {
					tim = reload - 0.5F;
					if (tim < 0.05F) {
						return pumpAttachment;
					} else if (tim < 0.12F) {
						float per = (tim - 0.05F) / (0.12F-0.05F);
						return Mix(pumpAttachment, handlePullingOffset, SmoothStep(per));
					} else if (tim < 0.26F) {
						pumpHandlePulled.Activate();
						return handlePullingOffset;
					} else if (tim < 0.36F) {
						float per = Min(1.0F, (tim-0.26F) / (0.36F-0.26F));
						return Mix(handlePullingOffset, pumpAttachment, SmoothStep(per));
					} else {
						pumpHandlePulled.Reset();
						return pumpAttachment;
					}
				} else {
					pumpHandlePulled.Reset();
					return pumpAttachment;
				}
			} else if (tim < 0.2F) {
				pumpHandlePulled.Reset();
				return pumpAttachment;
			} else if (tim < 0.3F) {
				float per = (tim - 0.2F) / 0.1F;
				return Mix(pumpAttachment, handlePullingOffset, SmoothStep(per));
			} else if (tim < 0.42F) {
				return handlePullingOffset;
			} else if (tim < 0.52F) {
				pumpHandlePulled.Activate();
				float per = ((tim - 0.42F) / (0.52F-0.42F));
				return Mix(handlePullingOffset, pumpAttachment, SmoothStep(per));
			} else {
				pumpHandlePulled.Reset();
				return pumpAttachment;
			}
		}

		Vector3 GetLeftHandOffset() {
			float reload = reloadProgress * 0.5F;

			Vector3 leftHandOffset = Vector3(1, 3, 0.5);
			Vector3 loadOffset = Vector3(1, -2, 1);
			Vector3 pumpHandleOffset = leftHandOffset + GetPumpOffset();

			float tim = 1.0F - readyState;
			if (tim < 0.0F) {
				if (reload < 0.2F) {
					float per = Min(1.0F, reload / 0.2F);
					return Mix(pumpHandleOffset, loadOffset, SmoothStep(per));
				} else if (reload < 0.36F) {
					shellInserted.Activate();
					return loadOffset;
				} else if (reload < 0.5F) {
					float per = Min(1.0F, (reload-0.36F) / (0.5F-0.36F));
					return Mix(loadOffset, pumpHandleOffset, SmoothStep(per));
				} else {
					return pumpHandleOffset;
				}
			} else {
				return pumpHandleOffset;
			}
		}

		Vector3 GetRightHandOffset() {
			Vector3 rightHandOffset = Vector3(0, -8, 2);

			// sprint animation
			rightHandOffset -= Vector3(0, 3, 0.5) * sprintSpring.position;

			return rightHandOffset;
		}

		ViewShotgunSkin(Renderer@ r, AudioDevice@ dev) {
			super(r);
			@audioDevice = dev;

			// load models
			@gunModel = renderer.RegisterModel("Models/Weapons/Shotgun/WeaponNoPump.kv6");
			@pumpModel = renderer.RegisterModel("Models/Weapons/Shotgun/Pump.kv6");
			@frontSightModel = renderer.RegisterModel("Models/Weapons/Shotgun/FrontSight.kv6");
			@rearSightModel = renderer.RegisterModel("Models/Weapons/Shotgun/RearSight.kv6");
			@dotSightModel = renderer.RegisterModel("Models/Weapons/Shotgun/DotSight.kv6");

			// load sounds
			@fireSound = dev.RegisterSound("Sounds/Weapons/Shotgun/FireLocal.opus");
			@fireFarSound = dev.RegisterSound("Sounds/Weapons/SMG/FireFar.opus");
			@fireStereoSound = dev.RegisterSound("Sounds/Weapons/Shotgun/FireStereo.opus");
			@fireSmallReverbSound = dev.RegisterSound("Sounds/Weapons/Shotgun/V2AmbienceSmall.opus");
			@fireLargeReverbSound = dev.RegisterSound("Sounds/Weapons/Shotgun/V2AmbienceLarge.opus");
			@reloadSound = dev.RegisterSound("Sounds/Weapons/Shotgun/ReloadLocal.opus");
			@cockSound = dev.RegisterSound("Sounds/Weapons/Shotgun/CockLocal.opus");

			// load images
			@scopeImage = renderer.RegisterImage("Gfx/Shotgun.png");

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

			bool isSprinting = sprintState >= 1 or sprintState > lastSprintState;
			bool isRaised = raiseState >= 1 or raiseState > lastRaiseState;

			sprintSpring.desired = isSprinting ? 1 : 0;
			raiseSpring.desired = isRaised ? 0 : 1;

			lastSprintState = sprintState;
			lastRaiseState = raiseState;
		}

		void WeaponFired() {
			BasicViewWeapon::WeaponFired();

			if (!IsMuted) {
				Vector3 origin = Vector3(0.4F, -0.3F, 0.5F);
				AudioParam param;
				param.volume = 8.0F;
				audioDevice.PlayLocal(fireSound, origin, param);

				param.volume = 8.0F * environmentRoom;
				audioDevice.PlayLocal((environmentSize < 0.5F)
					? fireSmallReverbSound : fireLargeReverbSound,
					origin, param);

				param.volume = 2.0F;
				audioDevice.PlayLocal(fireFarSound, origin, param);
				audioDevice.PlayLocal(fireStereoSound, origin, param);
			}

			recoilVerticalSpring.velocity += 2.5;
			recoilBackSpring.velocity += 2.5;
			recoilRotationSpring.velocity += (GetRandom() * 2 - 1);
		}

		void ReloadingWeapon() {
			shellInserted.Reset();

			if (!IsMuted) {
				Vector3 origin = Vector3(0.4F, -0.3F, 0.5F);
				AudioParam param;
				param.volume = 0.5F;
				audioDevice.PlayLocal(reloadSound, origin, param);
			}
		}

		void ReloadedWeapon() {
			pumpHandlePulled.Reset();

			if (!IsMuted) {
				Vector3 origin = Vector3(0.4F, -0.3F, 0.5F);
				AudioParam param;
				param.volume = 0.2F;
				audioDevice.PlayLocal(cockSound, origin, param);
			}
		}

		// (override BasicViewWeapon::GetZPos())
		float GetZPos() { return 0.2F - AimDownSightStateSmooth * 0.0535F; }

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
			recoilOffset -= Vector3(0, 1.0, 0) * recoilBackSpring.position;
			mat = CreateEulerAnglesMatrix(recoilRot * sp) * mat;
			mat = mat * CreateTranslateMatrix(recoilOffset);

			// add weapon offset and sway
			Vector3 trans(0, 0, 0);
			trans += Vector3(-0.13F * sp, 0.5F, GetZPos());
			trans += swing * sp;

			// manual adjustment
			trans.x += cg_viewWeaponX.FloatValue * sp;
			trans.y += cg_viewWeaponY.FloatValue * sp;
			trans.z += cg_viewWeaponZ.FloatValue * sp;

			mat = CreateTranslateMatrix(trans) * mat;

			// twist the gun when strafing
			Vector3 swingRot(0, 0, 0);
			swingRot.z += 2.0F * horizontalSwingSpring.position;
			swingRot.x -= 2.0F * verticalSwingSpring.position;
			mat = mat * CreateEulerAnglesMatrix(swingRot * sp);

			Vector3 pivot = Vector3(0.025F, 0.0F, 0.025F);
			Vector3 sightPos = (frontSightAttachment - pivot) * globalScale;
			mat = AdjustToAlignSight(mat, sightPos, AimDownSightStateSmooth);
			mat = AdjustToReload(mat);

			return mat;
		}

		// IWeaponSkin3 (override BasicViewWeapon::{get_MuzzlePosition, get_CaseEjectPosition})
		Vector3 MuzzlePosition { get { return GetViewWeaponMatrix() * Vector3(0.0F, 0.4F, -0.1F); } }
		Vector3 CaseEjectPosition { get { return GetViewWeaponMatrix() * Vector3(-0.025F, -0.1F, -0.1F); } }

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

			// draw pump
			param.matrix = weapMatrix
				* CreateTranslateMatrix(GetPumpOffset());
			renderer.AddModel(pumpModel, param);

			// draw sights
			param.matrix = weapMatrix
				* CreateTranslateMatrix(rearSightAttachment)
				* CreateScaleMatrix(rearSightScale);
			renderer.AddModel(rearSightModel, param); // rear

			param.matrix = weapMatrix
				* CreateTranslateMatrix(frontSightAttachment)
				* CreateScaleMatrix(frontSightScale);
			renderer.AddModel(frontSightModel, param); // front pin
			renderer.AddModel(dotSightModel, param); // front pin (emissive)

			LeftHandPosition = leftHand;
			RightHandPosition = rightHand;
		}
	}

	IWeaponSkin@ CreateViewShotgunSkin(Renderer@ r, AudioDevice@ dev) {
		return ViewShotgunSkin(r, dev);
	}
}

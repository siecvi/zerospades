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

namespace spades {
    class ViewShotgunSkin : BasicViewWeapon {
        private AudioDevice@ audioDevice;
        private Model@ gunModel;
        private Model@ pumpModel;
        private Model@ sightModel1;
        private Model@ sightModel2;

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
        private float pumpScale = 0.5F;
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
        private Vector3 swingFromSpring = Vector3();

        // A bunch of events.
        private ViewWeaponEvent pumpTouched = ViewWeaponEvent();
        private ViewWeaponEvent pumpHandlePulled = ViewWeaponEvent();
		private ViewWeaponEvent shellInserted = ViewWeaponEvent();

        // A bunch of states.
        private double lastSprintState = 0;
        private double lastRaiseState = 0;

        Matrix4 AdjustToReload(Matrix4 mat) {
			float reload = reloadProgress * 0.5F;

            if (reload < 0.4) {
                reloadPitchSpring.desired = 0.4;
                reloadRollSpring.desired = 0.4;
            } else if (reload < 0.5) {
                reloadPitchSpring.desired = 0;
                reloadRollSpring.desired = -0.4;
            } else {
                reloadPitchSpring.desired = 0;
                reloadRollSpring.desired = 0;
            }

            if (pumpTouched.WasActivated()) {
                pumpTouched.Acknowledge();
                reloadPitchSpring.velocity = 4;
            }

			if (shellInserted.WasActivated()) {
                shellInserted.Acknowledge();
                reloadPitchSpring.velocity = 8;
				reloadOffsetSpring.velocity = -1;
            }

            if (pumpHandlePulled.WasActivated()) {
                pumpHandlePulled.Acknowledge();
                reloadPitchSpring.velocity = 5;
                reloadOffsetSpring.velocity = 0.5;
            }

            mat *= CreateEulerAnglesMatrix(Vector3(0, 0.6, 0) * reloadRollSpring.position);
            mat *= CreateEulerAnglesMatrix(Vector3(-0.25, 0, 0) * reloadPitchSpring.position);
            mat *= CreateTranslateMatrix(Vector3(0, -1, 0) * reloadOffsetSpring.position);

            return mat;
        }

        Vector3 GetPumpOffset() {
			float tim = 1.0F - readyState;
			float reload = reloadProgress * 0.5F;

			Vector3 pullOffset = pumpAttachment - Vector3(0, 2, 0);

			if (tim < 0.0F) {
				// might be right after reloading
				if (ammo >= clipSize and reload < 1.0F and reload > 0.5F) {
					tim = reload - 0.5F;

					if (tim < 0.05F) {
						return pumpAttachment;
					} else if (tim < 0.12F) {
						pumpHandlePulled.Activate();
						float per = (tim - 0.05F) / (0.12F-0.05F);
						return Mix(pumpAttachment, pullOffset, SmoothStep(per));
					} else if (tim < 0.26F) {
						return pullOffset;
					} else if (tim < 0.36F) {
						float per = 1.0F - ((tim - 0.26F) / (0.36F-0.26F));
						return Mix(pumpAttachment, pullOffset, SmoothStep(per));
					}
				}
			} else if (tim < 0.2F) {
				return pumpAttachment;
			} else if (tim < 0.3F) {
				float per = (tim - 0.2F) / (0.3F-0.2F);
				return Mix(pumpAttachment, pullOffset, SmoothStep(per));
			} else if (tim < 0.42F) {
				return pullOffset;
			} else if (tim < 0.52F) {
				pumpHandlePulled.Activate();
				float per = ((tim - 0.42F) / (0.52F-0.42F));
				return Mix(pullOffset, pumpAttachment, SmoothStep(per));
			} else {
				pumpHandlePulled.Reset();
				return pumpAttachment;
			}

			return pumpAttachment;
        }

        Vector3 GetLeftHandOffset() {
			float reload = reloadProgress * 0.5F;

			Vector3 leftHandOffset = Vector3(1, 3, 0.5);
			Vector3 loadOffset = Vector3(1, -2, 1);

            if (reload < 0.2) {
				float per = Min(1.0, reload / 0.2);
                return Mix(leftHandOffset, loadOffset, SmoothStep(per));
            } else if (reload < 0.35) {
				shellInserted.Activate();
                return loadOffset;
			} else if (reload < 0.5) {
				float per = Min(1.0, (reload-0.35) / (0.5-0.35));
                return Mix(loadOffset, leftHandOffset, SmoothStep(per));
            } else {
                return leftHandOffset + GetPumpOffset();
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
            @gunModel = renderer.RegisterModel("Models/Weapons/Shotgun/WeaponNoPump.kv6");
            @pumpModel = renderer.RegisterModel("Models/Weapons/Shotgun/Pump.kv6");
            @sightModel1 = renderer.RegisterModel("Models/Weapons/Shotgun/Sight1.kv6");
            @sightModel2 = renderer.RegisterModel("Models/Weapons/Shotgun/Sight2.kv6");

            @fireSound = dev.RegisterSound("Sounds/Weapons/Shotgun/FireLocal.opus");
            @fireFarSound = dev.RegisterSound("Sounds/Weapons/Shotgun/FireFar.opus");
            @fireStereoSound = dev.RegisterSound("Sounds/Weapons/Shotgun/FireStereo.opus");
            @reloadSound = dev.RegisterSound("Sounds/Weapons/Shotgun/ReloadLocal.opus");
            @cockSound = dev.RegisterSound("Sounds/Weapons/Shotgun/CockLocal.opus");

            @fireSmallReverbSound = dev.RegisterSound("Sounds/Weapons/Shotgun/V2AmbienceSmall.opus");
            @fireLargeReverbSound = dev.RegisterSound("Sounds/Weapons/Shotgun/V2AmbienceLarge.opus");

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
            horizontalSwingSpring.Update(dt);
            verticalSwingSpring.velocity += swing.z * 60 * dt * 2;
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

            swingFromSpring = Vector3(horizontalSwingSpring.position, 0, verticalSwingSpring.position);
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
					? fireSmallReverbSound : fireLargeReverbSound, origin, param);

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
			pumpTouched.Reset();
			pumpHandlePulled.Reset();

            if (!IsMuted) {
                Vector3 origin = Vector3(0.4F, -0.3F, 0.5F);
                AudioParam param;
                param.volume = 0.2F;
                audioDevice.PlayLocal(cockSound, origin, param);
            }
        }

        float GetZPos() { return 0.2F - AimDownSightStateSmooth * 0.0535F; }

		Matrix4 GetViewWeaponMatrix() {
            Matrix4 mat;

			// sprint animation
			mat = CreateEulerAnglesMatrix(Vector3(0.3F, -0.1F, -0.55F) * sprintSpring.position) * mat;
            mat = CreateTranslateMatrix(Vector3(0.23F, -0.05F, 0.15F) * sprintSpring.position) * mat;

			// raise gun animation
            mat = CreateRotateMatrix(Vector3(0, 0, 1), raiseSpring.position * -1.3F) * mat;
            mat = CreateRotateMatrix(Vector3(0, 1, 0), raiseSpring.position * 0.2F) * mat;
            mat = CreateTranslateMatrix(Vector3(0.1F, -0.3F, 0.1F) * raiseSpring.position) * mat;

			float sp = 1.0F - AimDownSightStateSmooth;

			// recoil animation
            Vector3 recoilRot = Vector3(-2.5, 0.3, 0.3) * recoilVerticalSpring.position;
            Vector3 recoilOffset = Vector3(0, 0, -0.1) * recoilVerticalSpring.position;
            recoilOffset -= Vector3(0, 1.2, 0) * recoilBackSpring.position;
            mat = CreateEulerAnglesMatrix(recoilRot * sp) * mat;
            mat = mat * CreateTranslateMatrix(recoilOffset);

            Vector3 trans(0, 0, 0);
            trans += Vector3(-0.13F * sp, 0.5F, GetZPos());
            trans += swing * GetMotionGain();
            mat = CreateTranslateMatrix(trans) * mat;

			// twist the gun when strafing
			Vector3 strafeRot = Vector3(-2.0*swingFromSpring.z, 0, 2.0*swingFromSpring.x);
            mat = mat * CreateEulerAnglesMatrix(strafeRot * sp);

			Vector3 pivot = Vector3(0.025F, 0.0F, -0.025F);
			Vector3 sightPos = (frontSightAttachment - pivot) * globalScale;
			mat = AdjustToAlignSight(mat, sightPos, AimDownSightStateSmooth);
			mat = AdjustToReload(mat);

            return mat;
        }

		// IWeaponSkin3 (override BasicViewWeapon::{get_MuzzlePosition, get_CaseEjectPosition})
        Vector3 MuzzlePosition { get { return eyeMatrix * GetViewWeaponMatrix() * Vector3(0.0F, 0.4F, -0.1F); } }
        Vector3 CaseEjectPosition { get { return eyeMatrix * GetViewWeaponMatrix() * Vector3(0.0F, -0.1F, -0.1F); } }

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

            // draw sights
			param.matrix = weapMatrix
				* CreateTranslateMatrix(rearSightAttachment)
				* CreateScaleMatrix(rearSightScale);
            renderer.AddModel(sightModel2, param); // rear

            param.matrix = weapMatrix
				* CreateTranslateMatrix(frontSightAttachment)
				* CreateScaleMatrix(frontSightScale);
            renderer.AddModel(sightModel1, param); // front pin

			// draw pump
            param.matrix = weapMatrix
				* CreateTranslateMatrix(GetPumpOffset());
            renderer.AddModel(pumpModel, param);

			LeftHandPosition = leftHand;
            RightHandPosition = rightHand;
        }
    }

    IWeaponSkin@ CreateViewShotgunSkin(Renderer@ r, AudioDevice@ dev) {
        return ViewShotgunSkin(r, dev);
    }
}
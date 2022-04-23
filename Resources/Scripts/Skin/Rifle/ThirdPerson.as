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
    class ThirdPersonRifleSkin : IToolSkin, IThirdPersonToolSkin, IWeaponSkin, IWeaponSkin2,  IWeaponSkin3 {
        private float sprintState;
        private float raiseState;
        private Vector3 teamColor;
        private bool muted = true;
        private Matrix4 originMatrix;
        private float aimDownSightState;
		private float readyState;
        private bool reloading;
        private float reloadProgress;
        private int ammo, clipSize;

        private float environmentRoom;
        private float environmentSize;
        private float environmentDistance;
        private Vector3 soundOrigin;

        float SprintState { set { sprintState = value; } }
        float RaiseState { set { raiseState = value; } }
        bool IsMuted { set { muted = value; } }
        Vector3 TeamColor { set { teamColor = value; } }
        Matrix4 OriginMatrix { set { originMatrix = value; } }
		float PitchBias { get { return 0.0F; } }
        float AimDownSightState { set { aimDownSightState = value; } }

        bool IsReloading { set { reloading = value; } }
        float ReloadProgress { set { reloadProgress = value; } }
        int Ammo { set { ammo = value; } }
        int ClipSize { set { clipSize = value; } }
		float ReadyState { set { readyState = value; } }

        // IWeaponSkin2
        void SetSoundEnvironment(float room, float size, float distance) {
            environmentRoom = room;
            environmentSize = size;
            environmentDistance = distance;
        }
        Vector3 SoundOrigin { set { soundOrigin = value; } }

		// IWeaponSkin3
        Vector3 MuzzlePosition { get { return originMatrix * Vector3(0.35F, -1.85F, -0.125F); } }
        Vector3 CaseEjectPosition { get { return originMatrix * Vector3(0.35F, -0.7F, -0.125F); } }

        private Renderer@ renderer;
        private AudioDevice@ audioDevice;
        private Model@ model;
        private AudioChunk@[]  fireSounds(3);
        private AudioChunk@ fireFarSound;
        private AudioChunk@ fireStereoSound;
        private AudioChunk@ fireSmallReverbSound;
        private AudioChunk@ fireLargeReverbSound;
        private AudioChunk@ reloadSound;

        ThirdPersonRifleSkin(Renderer@ r, AudioDevice@ dev) {
            @renderer = r;
            @audioDevice = dev;
            @model = renderer.RegisterModel("Models/Weapons/Rifle/Weapon.kv6");

            @fireSounds[0] = dev.RegisterSound("Sounds/Weapons/Rifle/Fire1.opus");
            @fireSounds[1] = dev.RegisterSound("Sounds/Weapons/Rifle/Fire2.opus");
            @fireSounds[2] = dev.RegisterSound("Sounds/Weapons/Rifle/Fire3.opus");
            @fireFarSound = dev.RegisterSound("Sounds/Weapons/Rifle/FireFar.opus");
            @fireStereoSound = dev.RegisterSound("Sounds/Weapons/Rifle/FireStereo.opus");
            @reloadSound = dev.RegisterSound("Sounds/Weapons/Rifle/Reload.opus");

            @fireSmallReverbSound = dev.RegisterSound("Sounds/Weapons/Rifle/V2AmbienceSmall.opus");
            @fireLargeReverbSound = dev.RegisterSound("Sounds/Weapons/Rifle/V2AmbienceLarge.opus");
        }

        void Update(float dt) {}

        void WeaponFired() {
            if (!muted) {
                Vector3 origin = soundOrigin;
                AudioParam param;
                param.volume = 20.0F;
                audioDevice.Play(fireSounds[GetRandom(fireSounds.length)], origin, param);

                param.volume = 20.0F * environmentRoom;
				audioDevice.Play((environmentSize < 0.5F)
					? fireSmallReverbSound : fireLargeReverbSound, origin, param);

                param.volume = 1.0F;
                param.referenceDistance = 26.0F;
                audioDevice.Play(fireFarSound, origin, param);
                param.referenceDistance = 4.0F;
                param.volume = 1.0F;
                audioDevice.Play(fireStereoSound, origin, param);
            }
        }

        void ReloadingWeapon() {
            if (!muted) {
                Vector3 origin = soundOrigin;
                AudioParam param;
                param.volume = 0.2F;
                audioDevice.Play(reloadSound, origin, param);
            }
        }

        void ReloadedWeapon() {}

        void AddToScene() {
            Matrix4 mat = CreateScaleMatrix(0.05F);
            mat = mat * CreateScaleMatrix(-1, -1, 1);
            mat = CreateTranslateMatrix(0.35F, -1.0F, 0.0F) * mat;

            ModelRenderParam param;
            param.matrix = originMatrix * mat;
            renderer.AddModel(model, param);
        }
    }

    IWeaponSkin@ CreateThirdPersonRifleSkin(Renderer@ r, AudioDevice@ dev) {
        return ThirdPersonRifleSkin(r, dev);
    }
}
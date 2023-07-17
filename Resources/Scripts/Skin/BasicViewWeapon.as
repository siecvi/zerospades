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
    class ViewWeaponSpring {
        double position = 0;
        double desired = 0;
        double velocity = 0;
        double frequency = 1;
        double damping = 1;

        ViewWeaponSpring() {}

        ViewWeaponSpring(double f, double d) {
            frequency = f;
            damping = d;
        }

        ViewWeaponSpring(double f, double d, double des) {
            frequency = f;
            damping = d;
            desired = des;
        }

        void Update(double updateLength) {
            double timeStep = 1.0 / 240.0;

            // forces updates into at least 240 fps.
            for (double timeLeft = updateLength; timeLeft > 0; timeLeft -= timeStep) {
                double dt = Min(timeStep, timeLeft);
                double acceleration = (desired - position) * frequency;
                velocity = velocity + acceleration * dt;
                velocity -= velocity * damping * dt;
                position = position + velocity * dt;
            }
        }
    }

    class ViewWeaponEvent {
        bool activated = false;
        bool acknowledged = false;

        void Activate() {
            if (not acknowledged)
                activated = true;
        }

        bool WasActivated() {
            return acknowledged ? false : activated;
        }

        void Acknowledge() {
            acknowledged = true;
        }

        void Reset() {
            activated = false;
            acknowledged = false;
        }
    }

    class BasicViewWeapon : IToolSkin, IViewToolSkin, IWeaponSkin, IWeaponSkin2, IWeaponSkin3 {
        // IToolSkin
        protected float sprintState;
        protected float raiseState;
        protected Vector3 teamColor;
        protected bool muted;

        float SprintState {
            set { sprintState = value; }
            get { return sprintState; }
        }

        float RaiseState {
            set { raiseState = value; }
            get { return raiseState; }
        }

        Vector3 TeamColor {
            set { teamColor = value; }
            get { return teamColor; }
        }

        bool IsMuted {
            set { muted = value; }
            get { return muted; }
        }

        // IWeaponSkin
        protected float aimDownSightState;
        protected float aimDownSightStateSmooth;
        protected float readyState;
        protected bool reloading;
        protected float reloadProgress;
        protected int ammo, clipSize;
        protected float localFireVibration;
        protected float sprintStateSmooth;

        float AimDownSightState {
            set {
                aimDownSightState = value;
                aimDownSightStateSmooth = SmoothStep(value);
            }
            get { return aimDownSightState; }
        }

        float AimDownSightStateSmooth {
            get { return aimDownSightStateSmooth; }
        }

        bool IsReloading {
            set { reloading = value; }
            get { return reloading; }
        }
        float ReloadProgress {
            set { reloadProgress = value; }
            get { return reloadProgress; }
        }
        int Ammo {
            set { ammo = value; }
            get { return ammo; }
        }
        int ClipSize {
            set { clipSize = value; }
            get { return clipSize; }
        }

        float ReadyState {
            set { readyState = value; }
            get { return readyState; }
        }

        // IViewToolSkin
        protected Matrix4 eyeMatrix;
        protected Vector3 swing;
        protected Vector3 leftHand;
        protected Vector3 rightHand;

        Matrix4 EyeMatrix {
            set { eyeMatrix = value; }
            get { return eyeMatrix; }
        }

        Vector3 Swing {
            set { swing = value; }
            get { return swing; }
        }

        Vector3 LeftHandPosition {
            set { leftHand = value; }
            get { return leftHand; }
        }
        Vector3 RightHandPosition {
            get { return rightHand; }
            set { rightHand = value; }
        }

        // IWeaponSkin2
        protected float environmentRoom;
        protected float environmentSize;

        void SetSoundEnvironment(float room, float size, float distance) {
            environmentRoom = room;
            environmentSize = size;
        }
        // set_SoundOrigin is not called for first-person skin scripts
        Vector3 SoundOrigin { set {} }

        // IWeaponSkin3
        Vector3 MuzzlePosition { get { return eyeMatrix * GetViewWeaponMatrix() * Vector3(0.0F, 0.35F, -0.05F); } }
        Vector3 CaseEjectPosition { get { return eyeMatrix * GetViewWeaponMatrix() * Vector3(0.0F, -0.1F, -0.05F); } }

        protected Renderer@ renderer;

        protected Image@ sightImage;
        protected Image@ scopeImage;
        protected Image@ dotSightImage;

        protected ConfigItem cg_crosshair("cg_crosshair", "1");
        protected ConfigItem cg_crosshairColor("cg_crosshairColor", "0");
        protected ConfigItem cg_crosshairColorR("cg_crosshairColorR", "255");
        protected ConfigItem cg_crosshairColorG("cg_crosshairColorG", "255");
        protected ConfigItem cg_crosshairColorB("cg_crosshairColorB", "255");
        protected ConfigItem cg_crosshairAlpha("cg_crosshairAlpha", "255");
        protected ConfigItem cg_crosshairLines("cg_crosshairLines", "1");
        protected ConfigItem cg_crosshairGap("cg_crosshairGap", "4");
        protected ConfigItem cg_crosshairSize("cg_crosshairSize", "5");
        protected ConfigItem cg_crosshairThickness("cg_crosshairThickness", "1");
        protected ConfigItem cg_crosshairTStyle("cg_crosshairTStyle", "0");
        protected ConfigItem cg_crosshairDot("cg_crosshairDot", "0");
        protected ConfigItem cg_crosshairDotAlpha("cg_crosshairDotAlpha", "255");
        protected ConfigItem cg_crosshairDotThickness("cg_crosshairDotThickness", "1");
        protected ConfigItem cg_crosshairOutline("cg_crosshairOutline", "1");
        protected ConfigItem cg_crosshairOutlineAlpha("cg_crosshairOutlineAlpha", "255");
        protected ConfigItem cg_crosshairOutlineThickness("cg_crosshairOutlineThickness", "1");
        protected ConfigItem cg_crosshairOutlineRounded("cg_crosshairOutlineRounded", "0");
        protected ConfigItem cg_crosshairDynamic("cg_crosshairDynamic", "1");
        protected ConfigItem cg_crosshairDynamicSplitDist("cg_crosshairDynamicSplitdist", "7");

        protected ConfigItem cg_pngScope("cg_pngScope", "0");
        protected ConfigItem cg_crosshairScopeColor("cg_crosshairScopeColor", "0");
        protected ConfigItem cg_crosshairScopeColorR("cg_crosshairScopeColorR", "255");
        protected ConfigItem cg_crosshairScopeColorG("cg_crosshairScopeColorG", "255");
        protected ConfigItem cg_crosshairScopeColorB("cg_crosshairScopeColorB", "255");
        protected ConfigItem cg_crosshairScopeAlpha("cg_crosshairScopeAlpha", "255");
        protected ConfigItem cg_crosshairScopeLines("cg_crosshairScopeLines", "1");
        protected ConfigItem cg_crosshairScopeGap("cg_crosshairScopeGap", "4");
        protected ConfigItem cg_crosshairScopeSize("cg_crosshairScopeSize", "5");
        protected ConfigItem cg_crosshairScopeThickness("cg_crosshairScopeThickness", "1");
        protected ConfigItem cg_crosshairScopeTStyle("cg_crosshairScopeTStyle", "0");
        protected ConfigItem cg_crosshairScopeDot("cg_crosshairScopeDot", "0");
        protected ConfigItem cg_crosshairScopeDotAlpha("cg_crosshairScopeDotAlpha", "255");
        protected ConfigItem cg_crosshairScopeDotThickness("cg_crosshairScopeDotThickness", "1");
        protected ConfigItem cg_crosshairScopeOutline("cg_crosshairScopeOutline", "1");
        protected ConfigItem cg_crosshairScopeOutlineAlpha("cg_crosshairScopeOutlineAlpha", "255");
        protected ConfigItem cg_crosshairScopeOutlineThickness("cg_crosshairScopeOutlineThickness", "1");
        protected ConfigItem cg_crosshairScopeOutlineRounded("cg_crosshairScopeOutlineRounded", "0");
        protected ConfigItem cg_crosshairScopeDynamic("cg_crosshairScopeDynamic", "1");
        protected ConfigItem cg_crosshairScopeDynamicSplitDist("cg_crosshairScopeDynamicSplitdist", "7");

        BasicViewWeapon(Renderer@ renderer) {
            @this.renderer = renderer;
            localFireVibration = 0.0F;
            @scopeImage = renderer.RegisterImage("Gfx/Rifle.png");
            @sightImage = renderer.RegisterImage("Gfx/Target.png");
            @dotSightImage = renderer.RegisterImage("Gfx/DotSight.tga");
        }

        float GetLocalFireVibration() { return localFireVibration; }
        float GetMotionGain() { return 1.0F - AimDownSightStateSmooth * 0.4F; }
        float GetZPos() { return 0.2F - AimDownSightStateSmooth * 0.05F; }

        Vector3 GetLocalFireVibrationOffset() {
            float vib = GetLocalFireVibration();
            float motion = GetMotionGain();
            Vector3 hip(0.0F, 0.0F, 0.0F);
            hip.x = sin(vib * PiF * 2.0F) * 0.008F * motion;
            hip.y = vib * (vib - 1.0F) * 0.14F * motion;
            hip.z = vib * (1.0F - vib) * 0.03F * motion;
            Vector3 ads = Vector3(0.0F, vib * (vib - 1.0F) * vib * 0.3F * motion, 0.0F);
            return Mix(hip, ads, AimDownSightStateSmooth);
        }

        // Creates a rotation matrix from euler angles (in the form of a Vector3) x-y-z
        Matrix4 CreateEulerAnglesMatrix(Vector3 angles) {
            Matrix4 mat = CreateRotateMatrix(Vector3(1, 0, 0), angles.x);
            mat = CreateRotateMatrix(Vector3(0, 1, 0), angles.y) * mat;
            mat = CreateRotateMatrix(Vector3(0, 0, 1), angles.z) * mat;
            return mat;
        }

        // rotates gun matrix to ensure the sight is in the center of screen (0, ?, 0)
        Matrix4 AdjustToAlignSight(Matrix4 mat, Vector3 sightPos, float fade) {
            Vector3 p = mat * sightPos;
            mat = CreateRotateMatrix(Vector3(0, 1, 1), atan(p.x / p.y) * fade) * mat;
            mat = CreateRotateMatrix(Vector3(-1, 0, 0), atan(p.z / p.y) * fade) * mat;
            return mat;
        }

        Matrix4 GetViewWeaponMatrix() {
            Matrix4 mat;

            if (sprintStateSmooth > 0.0F) {
                mat = CreateRotateMatrix(Vector3(0, 1, 0), sprintStateSmooth * -0.1F) * mat;
                mat = CreateRotateMatrix(Vector3(1, 0, 0), sprintStateSmooth * 0.3F) * mat;
                mat = CreateRotateMatrix(Vector3(0, 0, 1), sprintStateSmooth * -0.55F) * mat;
                mat = CreateTranslateMatrix(Vector3(0.23F, -0.05F, 0.15F) * sprintStateSmooth) * mat;
            }

            if (raiseState < 1.0F) {
                float putdown = 1.0F - raiseState;
                mat = CreateRotateMatrix(Vector3(0, 0, 1), putdown * -1.3F) * mat;
                mat = CreateRotateMatrix(Vector3(0, 1, 0), putdown * 0.2F) * mat;
                mat = CreateTranslateMatrix(Vector3(0.1F, -0.3F, 0.1F) * putdown) * mat;
            }

            float sp = 1.0F - AimDownSightStateSmooth;

            if (readyState < 1.0F) {
                float per = SmoothStep(1.0F - readyState);
                mat = CreateTranslateMatrix(Vector3(-0.25F * sp, -0.5F, 0.25F * sp) * per * 0.1F) * mat;
                mat = CreateRotateMatrix(Vector3(-1, 0, 0), per * 0.05F * sp) * mat;
            }

            Vector3 trans(0.0F, 0.0F, 0.0F);
            trans += Vector3(-0.13F * sp, 0.5F, GetZPos());
            trans += swing * GetMotionGain();
            mat = CreateTranslateMatrix(trans) * mat;

            return mat;
        }

        void Update(float dt) {
            if (localFireVibration > 0.0F) {
				localFireVibration -= dt * 10.0F;
                if (localFireVibration < 0.0F)
                    localFireVibration = 0.0F;
            }

            float sprintStateSS = sprintState * sprintState;
            if (sprintStateSS > sprintStateSmooth)
                sprintStateSmooth = Mix(sprintStateSmooth, sprintStateSS, 1.0F - pow(0.001F, dt));
            else
                sprintStateSmooth = sprintStateSS;
        }

        void WeaponFired() {
            localFireVibration = 1.0F;
        }

        void AddToScene() {}
        void ReloadingWeapon() {}
        void ReloadedWeapon() {}

        void Draw2D() {
            float sw = renderer.ScreenWidth;
            float sh = renderer.ScreenHeight;

            Vector2 scrCenter = Vector2(sw, sh) * 0.5F;

			// draw scope
            if (AimDownSightStateSmooth > 0.99F) {
                IntVector3 col;
                col.x = cg_crosshairScopeColorR.IntValue;
                col.y = cg_crosshairScopeColorG.IntValue;
                col.z = cg_crosshairScopeColorB.IntValue;

                Vector4 color = ConvertColorRGBA(col);
                color.w = Clamp(cg_crosshairScopeAlpha.IntValue, 0, 255) / 255.0F;

                if (cg_pngScope.IntValue == 1) { // draw classic png scope
                    Vector2 imgSize = Vector2(scopeImage.Width, scopeImage.Height);
                    imgSize *= Max(1.0F, sw / 800.0F);
                    imgSize *= Min(1.0F, sh / 600.0F);
                    imgSize *= Max(0.25F * (1.0F - readyState) + 1.0F, 1.0F);

                    Vector2 imgPos = scrCenter - (imgSize * 0.5F);

                    renderer.ColorNP = Vector4(1.0F, 1.0F, 1.0F, color.w);
                    renderer.DrawImage(scopeImage, AABB2(imgPos.x, imgPos.y, imgSize.x, imgSize.y));
                } else if (cg_pngScope.IntValue == 2) { // draw dot png scope
					Vector2 imgSize = Vector2(dotSightImage.Width, dotSightImage.Height);
					Vector2 imgPos = scrCenter - (imgSize * 0.5F);
                    renderer.ColorNP = color;
                    renderer.DrawImage(dotSightImage, imgPos);
                } else if (cg_pngScope.IntValue == 3) { // draw custom crosshair scope
					bool drawLines = cg_crosshairScopeLines.BoolValue;
                    bool useTStyle = cg_crosshairScopeTStyle.BoolValue;
                    float lineGap = cg_crosshairScopeGap.FloatValue;
                    if (cg_crosshairScopeDynamic.BoolValue) {
						float maxDist = cg_crosshairScopeDynamicSplitDist.FloatValue;
						lineGap += localFireVibration * maxDist;
					}

                    float lineLength = cg_crosshairScopeSize.FloatValue;
                    float lineThickness = Max(1.0F, cg_crosshairScopeThickness.FloatValue);
                    bool drawDot = cg_crosshairScopeDot.BoolValue;
                    float dotAlpha = Clamp(cg_crosshairScopeDotAlpha.IntValue, 0, 255) / 255.0F;
                    float dotThickness = Max(1.0F, cg_crosshairScopeDotThickness.FloatValue);
                    bool drawOutline = cg_crosshairScopeOutline.BoolValue;
                    float outlineAlpha = Clamp(cg_crosshairScopeOutlineAlpha.IntValue, 0, 255) / 255.0F;
                    float outlineThickness = Max(1.0F, cg_crosshairScopeOutlineThickness.FloatValue);
                    bool outlineRounded = cg_crosshairScopeOutlineRounded.BoolValue;

                    DrawCrosshair(renderer, scrCenter,
						drawLines, useTStyle, lineGap, lineLength, lineThickness, color,
                        drawDot, dotAlpha, dotThickness,
                        drawOutline, outlineAlpha, outlineThickness, outlineRounded);
                }

                return; // do not draw the target when aiming
            }

            IntVector3 col;
            switch (cg_crosshairColor.IntValue) {
                case 1: col = IntVector3(250, 50, 50); break; // red
                case 2: col = IntVector3(50, 250, 50); break; // green
                case 3: col = IntVector3(50, 50, 250); break; // blue
                case 4: col = IntVector3(250, 250, 50); break; // yellow
                case 5: col = IntVector3(50, 250, 250); break; // cyan
                case 6: col = IntVector3(250, 50, 250); break; // pink
                default: // custom
                    col.x = cg_crosshairColorR.IntValue;
                    col.y = cg_crosshairColorG.IntValue;
                    col.z = cg_crosshairColorB.IntValue;
                    break;
            }

            Vector4 color = ConvertColorRGBA(col);
            color.w = Clamp(cg_crosshairAlpha.IntValue, 0, 255) / 255.0F;

            if (cg_crosshair.BoolValue) { // draw custom crosshair
				bool drawLines = cg_crosshairLines.BoolValue;
                bool useTStyle = cg_crosshairTStyle.BoolValue;
                float lineGap = cg_crosshairGap.FloatValue;
                 if (cg_crosshairDynamic.BoolValue) {
					float maxDist = cg_crosshairDynamicSplitDist.FloatValue;
					lineGap += sprintState * maxDist;
					lineGap += localFireVibration * maxDist;
					lineGap += (1.0F - raiseState) * maxDist;
				}

                float lineLength = cg_crosshairSize.FloatValue;
                float lineThickness = Max(1.0F, cg_crosshairThickness.FloatValue);
                bool drawDot = cg_crosshairDot.BoolValue;
                float dotAlpha = Clamp(cg_crosshairDotAlpha.IntValue, 0, 255) / 255.0F;
                float dotThickness = Max(1.0F, cg_crosshairDotThickness.FloatValue);
                bool drawOutline = cg_crosshairOutline.BoolValue;
                float outlineAlpha = Clamp(cg_crosshairOutlineAlpha.IntValue, 0, 255) / 255.0F;
                float outlineThickness = Max(1.0F, cg_crosshairOutlineThickness.FloatValue);
                bool outlineRounded = cg_crosshairOutlineRounded.BoolValue;

                DrawCrosshair(renderer, scrCenter,
					drawLines, useTStyle, lineGap, lineLength, lineThickness, color,
					drawDot, dotAlpha, dotThickness,
                    drawOutline, outlineAlpha, outlineThickness, outlineRounded);
            } else { // draw default target
				Vector2 imgSize = Vector2(sightImage.Width, sightImage.Height);
				Vector2 imgPos = scrCenter - (imgSize * 0.5F);
                renderer.ColorNP = color;
                renderer.DrawImage(sightImage, imgPos);
            }
        }
    }
}
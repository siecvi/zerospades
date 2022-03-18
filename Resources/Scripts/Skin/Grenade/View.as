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
	class ViewGrenadeSkin : IToolSkin, IViewToolSkin, IGrenadeSkin {
		private float sprintState;
		private float raiseState;
		private Vector3 teamColor;
		private Matrix4 eyeMatrix;
		private Vector3 swing;
		private Vector3 leftHand;
		private Vector3 rightHand;
		private float cookTime;
		private float readyState;

		private float sprintStateSmooth;

		float SprintState { set { sprintState = value; } }
		float RaiseState { set { raiseState = value; } }
		Vector3 TeamColor { set { teamColor = value; } }
		bool IsMuted { set {} } // nothing to do
		Matrix4 EyeMatrix { set { eyeMatrix = value; } }
		Vector3 Swing { set { swing = value; } }
		Vector3 LeftHandPosition { get { return leftHand; } }
		Vector3 RightHandPosition { get { return rightHand; } }
		float CookTime { set { cookTime = value; } }
		float ReadyState { set { readyState = value; } }

		private Renderer@ renderer;
		private AudioDevice@ audioDevice;
		private Model@ model;
		private Image@ sightImage;

		ViewGrenadeSkin(Renderer@ r, AudioDevice@ dev) {
			@renderer = r;
			@audioDevice = dev;
			@model = renderer.RegisterModel("Models/Weapons/Grenade/Grenade.kv6");
			@sightImage = renderer.RegisterImage("Gfx/Sight.tga");
		}

		void Update(float dt) {
			float sprintStateSS = sprintState * sprintState;
			if (sprintStateSS > sprintStateSmooth)
				sprintStateSmooth = Mix(sprintStateSmooth, sprintStateSS, 1.0F - pow(0.001F, dt));
			else
				sprintStateSmooth = sprintStateSS;
		}

		void AddToScene() {
			Matrix4 mat = CreateScaleMatrix(0.033F);

			if (readyState > 0.9999F) {
				float bring = 0.0F;
				float pin = 0.0F;
				float side = 0.0F;

				bring = Min((readyState - 1.0F) * 2.0F, 1.0F);
				bring = 1.0F - bring;
				bring = 1.0F - bring * bring;

				if (cookTime > 0.0001F) {
					pin = Min(cookTime * 8.0F, 2.0F);

					if (pin > 1.0F) {
						side += pin - 1.0F;
						bring -= (pin - 1.0F) * 2.0F;
					}
				}

				if (sprintStateSmooth > 0.0F) {
					mat = CreateRotateMatrix(Vector3(0.0F, 0.0F, 1.0F), sprintStateSmooth * -0.3F) * mat;
					mat = CreateTranslateMatrix(Vector3(0.1F, -0.2F, -0.05F) * sprintStateSmooth) * mat;
				}

				mat = CreateTranslateMatrix(Vector3(-0.1F, -0.3F, 0.1F) * (1.0F - raiseState)) * mat;
				mat = CreateTranslateMatrix(swing) * mat;

				Matrix4 leftHandMat = mat;
				leftHandMat = CreateTranslateMatrix(-0.3F + side * 0.5F, 0.7F - bring * 0.05F, 0.3F - bring * 0.05F) * leftHandMat;
				leftHand = leftHandMat * Vector3(12.0F, -1.0F, 8.0F);

				mat = CreateTranslateMatrix(-0.3F - side * 0.8F, 0.7F - bring * 0.1F, 0.3F - bring * 0.15F) * mat;
				rightHand = mat * Vector3(-3.0F, 1.0F, 5.0F);

				Vector3 pinOffset = leftHandMat * Vector3(1.0F, 2.0F, 0.0F);
				Vector3 throwPos = leftHandMat * Vector3(-0.3F, 0.0F, 4.0F);

				if (pin < 1.0F)
					leftHand = Mix(leftHand, pinOffset, pin);
				else
					leftHand = Mix(pinOffset, throwPos, pin - 1.0F);

				ModelRenderParam param;
				param.depthHack = true;
				param.matrix = eyeMatrix * CreateTranslateMatrix(-0.05F, 0.1F, 0.08F) * mat;
				renderer.AddModel(model, param);
			} else { // throwing
                float per = readyState;
                per = Min(per * 3.0F, 1.0F);

                // left hand shouldn't be visible
				leftHand = Mix(leftHand, Vector3(0.5F, 0.5F, 0.6F), per);

                float p2 = per - 0.6F;
                p2 = 0.9F - p2 * p2 * 2.5F;
                rightHand = Vector3(-0.2F, p2, -0.9F + per * 1.8F);
            }
		}

		void Draw2D() {
			renderer.ColorNP = Vector4(1.0F, 1.0F, 1.0F, 1.0F);
			renderer.DrawImage(sightImage,
				Vector2((renderer.ScreenWidth - sightImage.Width) * 0.5F,
						(renderer.ScreenHeight - sightImage.Height) * 0.5F));
		}
	}

	IGrenadeSkin@ CreateViewGrenadeSkin(Renderer@ r, AudioDevice@ dev) {
		return ViewGrenadeSkin(r, dev);
	}
}
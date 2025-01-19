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
	class ViewSpadeSkin : IToolSkin, IViewToolSkin, ISpadeSkin {
		private float sprintState;
		private float raiseState;
		private Vector3 teamColor;
		private Matrix4 eyeMatrix;
		private Vector3 swing;
		private Vector3 leftHand;
		private Vector3 rightHand;
		private SpadeActionType actionType;
		private float actionProgress;
		private float sprintStateSmooth;

		float SprintState { set { sprintState = value; } }
		float RaiseState { set { raiseState = value; } }
		Vector3 TeamColor { set { teamColor = value; } }
		bool IsMuted { set {} } // nothing to do
		Matrix4 EyeMatrix { set { eyeMatrix = value; } }
		Vector3 Swing { set { swing = value; } }
		Vector3 LeftHandPosition { get { return leftHand; } }
		Vector3 RightHandPosition { get { return rightHand; } }
		SpadeActionType ActionType { set { actionType = value; } }
		float ActionProgress { set { actionProgress = value; } }

		private Renderer@ renderer;
		private AudioDevice@ audioDevice;
		private Model@ spadeModel;
		private Model@ pickaxeModel;
		private Model@ model;
		private Image@ sightImage;

		protected ConfigItem cg_viewWeaponX("cg_viewWeaponX");
		protected ConfigItem cg_viewWeaponY("cg_viewWeaponY");
		protected ConfigItem cg_viewWeaponZ("cg_viewWeaponZ");
		protected ConfigItem cg_viewWeaponSide("cg_viewWeaponSide");

		ViewSpadeSkin(Renderer@ r, AudioDevice@ dev) {
			@renderer = r;
			@audioDevice = dev;
			@spadeModel	= renderer.RegisterModel("Models/Weapons/Spade/Spade.kv6");
			@pickaxeModel = renderer.RegisterModel("Models/Weapons/Spade/Pickaxe.kv6");
			@model = @spadeModel;
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

			float weapSide = Clamp(cg_viewWeaponSide.FloatValue, -1.0F, 1.0F);

			if (sprintStateSmooth > 0.0F or raiseState < 1.0F) {
				float per = Max(sprintStateSmooth, 1.0F - raiseState);
				mat = CreateRotateMatrix(Vector3(0.0F, 1.0F, 0.0F), per * 1.3F) * mat;
				mat = CreateTranslateMatrix(Vector3(0.3F, -0.4F, -0.1F) * per) * mat;
			}
			mat = CreateTranslateMatrix(0.0F, (1.0F - raiseState) * -0.3F, 0.0F) * mat;

			if (actionType == spades::SpadeActionType::Bash) {
				@model = @pickaxeModel;

				float per = SmoothStep(1.0F - actionProgress);
				mat = CreateRotateMatrix(Vector3(1, 0, 0), per * 1.7F) * mat;
				mat = CreateTranslateMatrix(per * 0.2F * weapSide, per * 0.3F, 0.0F) * mat;
			} else if (actionType == spades::SpadeActionType::DigStart or actionType == spades::SpadeActionType::Dig) {
				@model = @spadeModel;

				float f = SmoothStep(1.0F - actionProgress);
				float f2;
				if (f >= 0.6F) {
					f2 = 0.0F;
					f = 1.0F - f;
				} else if (f >= 0.3F) {
					f2 = 0.6F - f;
					f = 0.4F;
				} else if (f >= 0.1F) {
					f2 = 0.3F;
					f = 0.4F;
				} else {
					f2 = f * 3.0F;
					f *= 4.0F;
				}

				mat = CreateTranslateMatrix(Vector3(f2 * weapSide, f * -0.2F, -f2 * 0.25F)) * mat;
				mat = CreateRotateMatrix(Vector3(1, 0, 0), f / 0.32F) * mat;
				mat = CreateRotateMatrix(Vector3(0, 1, 0), -f * weapSide) * mat;
			}

			// add weapon offset
			Vector3 trans(0.0F, 0.0F, 0.0F);
			trans += Vector3(-0.3F, 0.7F, 0.3F);

			// manual adjustment
			trans.x += cg_viewWeaponX.FloatValue;
			trans.y += cg_viewWeaponY.FloatValue;
			trans.z += cg_viewWeaponZ.FloatValue;
			trans.x *= weapSide;

			// add weapon sway
			trans += swing;

			mat = CreateTranslateMatrix(trans) * mat;

			// hands offset
			leftHand = mat * Vector3(0.0F, 0.0F, 7.0F);
			rightHand = mat * Vector3(0.0F, 0.0F, -2.0F);

			ModelRenderParam param;
			param.depthHack = true;
			param.matrix = eyeMatrix * mat;
			renderer.AddModel(model, param);
		}

		void Draw2D() {
			renderer.ColorNP = Vector4(1.0F, 1.0F, 1.0F, 1.0F);
			renderer.DrawImage(sightImage,
				Vector2((renderer.ScreenWidth - sightImage.Width) * 0.5F,
						(renderer.ScreenHeight - sightImage.Height) * 0.5F));
		}
	}

	ISpadeSkin@ CreateViewSpadeSkin(Renderer@ r, AudioDevice@ dev) {
		return ViewSpadeSkin(r, dev);
	}
}
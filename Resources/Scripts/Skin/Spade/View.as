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

			if (sprintStateSmooth > 0.0F or raiseState < 1.0F) {
				float per = Max(sprintStateSmooth, 1.0F - raiseState);
				mat = CreateRotateMatrix(Vector3(0.0F, 1.0F, 0.0F), per * 1.3F) * mat;
				mat = CreateTranslateMatrix(Vector3(0.3F, -0.4F, -0.1F) * per) * mat;
			}
			mat = CreateTranslateMatrix(0.0F, (1.0F - raiseState) * -0.3F, 0.0F) * mat;

			if (actionType == spades::SpadeActionType::Bash) {
				@model = @pickaxeModel;

				float per = 1.0F - actionProgress;
				mat = CreateRotateMatrix(Vector3(1, 0, 0), per * 1.7F) * mat;
				mat = CreateTranslateMatrix(0.0F, per * 0.3F, 0.0F) * mat;
			} else if (actionType == spades::SpadeActionType::DigStart or actionType == spades::SpadeActionType::Dig) {
				@model = @spadeModel;

				float per = actionProgress;

				// some tunes
				const float readyFront = -1.2F;
				const float digAngle = 0.6F;
				const float readyAngle = 0.6F;

				float angle;
				float front = readyFront;
				float side = 1.0F;

				if (per < 0.5F) {
					if (actionType == spades::SpadeActionType::DigStart) {
						// bringing to the dig position
						per = 4.0F * per * per;
						angle = per * readyAngle;
						side = per;
						front = per * readyFront;
					} else {
						// soon after digging
						angle = readyAngle;
						per = (0.5F - per) / 0.5F;
						per *= per;
						per *= per;
						angle += per * digAngle;
						front += per * 2.0F;
					}
				} else {
					per = (per - 0.5F) / 0.5F;
					per = 1.0F - (1.0F - per) * (1.0F - per);
					angle = readyAngle + per * digAngle;
					front += per * 2.0F;
				}

				mat = CreateRotateMatrix(Vector3(1, 0, 0), angle) * mat;
				mat = CreateRotateMatrix(Vector3(0, 0, 1), front * 0.125F) * mat;

				side *= 0.3F;
				front *= 0.1F;

				mat = CreateTranslateMatrix(side, front, front * 0.2F) * mat;
			}
			
			// add weapon offset and sway
			Vector3 trans(0.0F, 0.0F, 0.0F);
			trans += Vector3(-0.3F, 0.7F, 0.3F);
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
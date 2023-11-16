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
	class ViewBlockSkin : IToolSkin, IViewToolSkin, IBlockSkin {
		private float sprintState;
		private float raiseState;
		private Vector3 teamColor;
		private Matrix4 eyeMatrix;
		private Vector3 swing;
		private Vector3 leftHand;
		private Vector3 rightHand;
		private Vector3 blockColor;
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
		Vector3 BlockColor { set { blockColor = value; } }
		float ReadyState { set { readyState = value; } }

		private Renderer@ renderer;
		private AudioDevice@ audioDevice;
		private Model@ model;
		private Image@ sightImage;

		ViewBlockSkin(Renderer@ r, AudioDevice@ dev) {
			@renderer = r;
			@audioDevice = dev;
			@model = renderer.RegisterModel("Models/Weapons/Block/Block.kv6");
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

			if (sprintStateSmooth > 0.0F) {
				mat = CreateRotateMatrix(Vector3(0.0F, 0.0F, 1.0F), sprintStateSmooth * -0.3F) * mat;
				mat = CreateTranslateMatrix(Vector3(0.1F, -0.4F, -0.05F) * sprintStateSmooth) * mat;
			}

			mat = CreateTranslateMatrix(Vector3(-0.1F, -0.3F, 0.2F) * (1.0F - raiseState)) * mat;

			if (readyState < 0.99F)
				mat = CreateTranslateMatrix(Vector3(-0.25F, 0.0F, 0.4F) * (1.0F - readyState)) * mat;

			mat = CreateTranslateMatrix(-0.3F, 0.7F, 0.3F) * mat;
			mat = CreateTranslateMatrix(swing) * mat;

			leftHand = mat * Vector3(5.0F, -1.0F, 4.0F);
			rightHand = mat * Vector3(-5.5F, 3.0F, -5.0F);

			ModelRenderParam param;
			param.matrix = eyeMatrix * mat;
			param.customColor = blockColor;
			param.depthHack = true;
			renderer.AddModel(model, param);
		}

		void Draw2D() {
			renderer.ColorNP = Vector4(1.0F, 1.0F, 1.0F, 1.0F);
			renderer.DrawImage(sightImage,
				Vector2((renderer.ScreenWidth - sightImage.Width) * 0.5F,
						(renderer.ScreenHeight - sightImage.Height) * 0.5F));
		}
	}

	IBlockSkin@ CreateViewBlockSkin(Renderer@ r, AudioDevice@ dev) {
		return ViewBlockSkin(r, dev);
	}
}
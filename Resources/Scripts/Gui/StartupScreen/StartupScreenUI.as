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

#include "MainMenu.as"

namespace spades {

	class StartupScreenUI {
		private Renderer@ renderer;
		private AudioDevice@ audioDevice;
		FontManager@ fontManager;
		StartupScreenHelper@ helper;

		spades::ui::UIManager@ manager;

		StartupScreenMainMenu@ mainMenu;

		bool shouldExit = false;

		StartupScreenUI(Renderer@ renderer, AudioDevice@ audioDevice,
			FontManager@ fontManager, StartupScreenHelper@ helper) {
			@this.renderer = renderer;
			@this.audioDevice = audioDevice;
			@this.fontManager = fontManager;
			@this.helper = helper;

			SetupRenderer();

			@manager = spades::ui::UIManager(renderer, audioDevice);
			@manager.RootElement.Font = fontManager.GuiFont;
			Init();
		}

		private void Init() {
			@mainMenu = StartupScreenMainMenu(this);
			mainMenu.Bounds = manager.RootElement.Bounds;
			manager.RootElement.AddChild(mainMenu);
		}

		void Reload() {
			// Delete StartupScreenMainMenu
			@manager.RootElement.GetChildren()[0].Parent = null;

			// Reload entire the startup screen while
			// preserving the state as much as possible
			auto@ state = mainMenu.GetState();
			Init();
			mainMenu.SetState(state);
		}

		void SetupRenderer() {
			if (manager !is null)
				manager.KeyPanic();
		}

		void MouseEvent(float x, float y) { manager.MouseEvent(x, y); }
		void WheelEvent(float x, float y) { manager.WheelEvent(x, y); }
		void KeyEvent(string key, bool down) { manager.KeyEvent(key, down); }
		void TextInputEvent(string text) { manager.TextInputEvent(text); }
		void TextEditingEvent(string text, int start, int len) {
			manager.TextEditingEvent(text, start, len);
		}

		bool AcceptsTextInput() { return manager.AcceptsTextInput; }
		AABB2 GetTextInputRect() { return manager.TextInputRect; }

		void RunFrame(float dt) {
			renderer.ColorNP = Vector4(0.0F, 0.0F, 0.0F, 1.0F);
			renderer.DrawImage(null, AABB2(0.0F, 0.0F, renderer.ScreenWidth, renderer.ScreenHeight));

			float fade = Clamp(manager.Time * 2.0F, 0.0F, 1.0F);

			// draw title logo
			Image@ img = renderer.RegisterImage("Gfx/Title/LogoSmall.png");
			renderer.ColorNP = Vector4(1.0F, 1.0F, 1.0F, 1.0F * fade);
			renderer.DrawImage(img, AABB2(10.0F + (2.0F - img.Width) * (1.0F - fade), 10.0F, img.Width, img.Height));

			Font@ font = manager.RootElement.Font;
			string text = "Version 0.0.5";
			Vector2 position(225.0F, 34.0F + (2.0F - img.Width) * (1.0F - fade));

			float pulse = sin(abs(manager.Time * 2.0F));
			font.DrawShadow(text, position, 1.0F, Vector4(0.0F, 0.0F, 0.0F, pulse), Vector4(0.5F, 1.0F, 0.5F, pulse));
			font.DrawShadow(text, position, 1.0F, Vector4(0.7F, 1.0F, 0.7F, fade), Vector4(0.5F, 1.0F, 0.5F, fade));

			manager.RunFrame(dt);
			manager.Render();
		}

		void RunFrameLate(float dt) {
			renderer.FrameDone();
			renderer.Flip();
		}

		void Closing() { shouldExit = true; }
		bool WantsToBeClosed() { return shouldExit; }
	}

	StartupScreenUI@ CreateStartupScreenUI(Renderer@ renderer, AudioDevice@ audioDevice, FontManager@ fontManager, StartupScreenHelper@ helper) {
		return StartupScreenUI(renderer, audioDevice, fontManager, helper);
	}
}
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
	class MessageBoxScreen : spades::ui::UIElement {
		private float ContentsTop, ContentsHeight;

		spades::ui::EventHandler@ Closed;
		int ResultIndex = -1;

		private spades::ui::UIElement@ owner;

		MessageBoxScreen(spades::ui::UIElement@ owner, string text, string[]@ buttons, float height = 200.0F) {
			super(owner.Manager);
			@this.owner = owner;
			@Font = Manager.RootElement.Font;
			this.Bounds = owner.Bounds;

			float sw = Manager.ScreenWidth;
			float sh = Manager.ScreenHeight;

			float contentsWidth = sw - 16.0F;
			float maxContentsWidth = 800.0F;
			if (contentsWidth > maxContentsWidth)
				contentsWidth = maxContentsWidth;

			float contentsLeft = (sw - contentsWidth) * 0.5F;
			ContentsHeight = height;
			ContentsTop = (sh - ContentsHeight) * 0.5F;

			{
				spades::ui::Label label(Manager);
				label.BackgroundColor = Vector4(0.0F, 0.0F, 0.0F, 0.9F);
				label.Bounds = AABB2(0.0F, ContentsTop - 13.0F, Size.x, ContentsHeight + 27.0F);
				AddChild(label);
			}

			for (uint i = 0; i < buttons.length; ++i) {
				spades::ui::Button button(Manager);
				button.Caption = buttons[i];
				button.Bounds = AABB2(contentsLeft + contentsWidth -
										  (150.0F + 10.0F) * (buttons.length - i) + 10.0F,
									  ContentsTop + ContentsHeight - 30.0F, 150.0F, 30.0F);

				MessageBoxScreenButtonHandler handler;
				@handler.screen = this;
				handler.resultIndex = int(i);
				@button.Activated = spades::ui::EventHandler(handler.OnPressed);
				AddChild(button);
			}
			{
				spades::ui::TextViewer viewer(Manager);
				AddChild(viewer);
				viewer.Bounds =
					AABB2(contentsLeft, ContentsTop, contentsWidth, ContentsHeight - 40.0F);
				viewer.Text = text;
			}
		}

		private void OnClosed() {
			if (Closed !is null)
				Closed(this);
		}

		void EndDialog(int result) {
			ResultIndex = result;
			Close();
		}

		void Close() {
			owner.Enable = true;
			owner.Parent.RemoveChild(this);
			OnClosed();
		}

		void Run() {
			owner.Enable = false;
			owner.Parent.AddChild(this);
		}

		void Render() {
			Renderer@ r = Manager.Renderer;
			Vector2 pos = ScreenPosition;
			Vector2 size = Size;

			r.ColorNP = Vector4(1.0F, 1.0F, 1.0F, 0.07F);
			r.DrawImage(null, AABB2(pos.x, pos.y + ContentsTop - 14.0F, size.x, 1.0F));
			r.DrawImage(null, AABB2(pos.x, pos.y + ContentsTop + ContentsHeight + 14.0F, size.x, 1.0F));

			UIElement::Render();
		}
	}

	class MessageBoxScreenButtonHandler {
		MessageBoxScreen@ screen;
		int resultIndex;

		void OnPressed(spades::ui::UIElement@) { screen.EndDialog(resultIndex); }
	}

	class AlertScreen : MessageBoxScreen {
		AlertScreen(spades::ui::UIElement@ owner, string text, float height = 200.0F) {
			super(owner, text, array<string> = {_Tr("MessageBox", "OK")}, height);
		}

		void HotKey(string key) {
			if (IsEnabled and (key == "Enter" or key == "Escape")) {
				EndDialog(0);
			} else {
				UIElement::HotKey(key);
			}
		}
	}

	class ConfirmScreen : MessageBoxScreen {
		ConfirmScreen(spades::ui::UIElement@ owner, string text, float height = 200.0F) {
			super(owner, text, array<string> = {_Tr("MessageBox", "OK"), _Tr("MessageBox", "Cancel")}, height);
		}

		bool get_Result() property { return ResultIndex == 0; }

		void HotKey(string key) {
			if (IsEnabled and key == "Enter") {
				EndDialog(0);
			} else if (IsEnabled and key == "Escape") {
				EndDialog(1);
			} else {
				UIElement::HotKey(key);
			}
		}
	}
}

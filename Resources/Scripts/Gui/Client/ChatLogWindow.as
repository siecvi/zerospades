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

#include "ChatSayWindow.as"

namespace spades {

    class ChatLogSayWindow : ClientChatWindow {
        ChatLogWindow@ owner;

        ChatLogSayWindow(ChatLogWindow@ own, bool isTeamChat) {
            super(own.ui, isTeamChat);
            @owner = own;
        }

        void Close() {
            owner.SayWindowClosed();
            @this.Parent = null;
        }
    }

    class ChatLogWindow : spades::ui::UIElement {
        float contentsTop, contentsHeight;

        ClientUI@ ui;
        private ClientUIHelper@ helper;

        private spades::ui::TextViewer@ viewer;
        ChatLogSayWindow@ sayWindow;

        private spades::ui::UIElement@ sayButton1;
        private spades::ui::UIElement@ sayButton2;

        ChatLogWindow(ClientUI@ ui) {
            super(ui.manager);
            @this.ui = ui;
            @this.helper = ui.helper;

            @Font = Manager.RootElement.Font;
            this.Bounds = Manager.RootElement.Bounds;

			auto sw = Manager.ScreenWidth;
            auto sh = Manager.ScreenHeight;

            float spacing = 16.0F;
			float contentsWidth = sw - spacing;
			float maxContentsWidth = 800.0F - spacing;
			if (contentsWidth >= maxContentsWidth)
				contentsWidth = maxContentsWidth;

			float contentsLeft = (sw - contentsWidth) * 0.5F;
			contentsHeight = sh - 200.0F;
			contentsTop = (sh - contentsHeight - 106.0F) * 0.5F;

            {
                spades::ui::Label label(Manager);
                label.BackgroundColor = Vector4(0, 0, 0, 0.4F);
                label.Bounds = Bounds;
                AddChild(label);
            }
            {
                spades::ui::Label label(Manager);
                label.BackgroundColor = Vector4(0, 0, 0, 0.8F);
                label.Bounds = AABB2(0.0F, contentsTop - 13.0F, Size.x, contentsHeight + 27.0F);
                AddChild(label);
            }
            {
                spades::ui::Button button(Manager);
                button.Caption = _Tr("Client", "Close");
                button.Bounds = AABB2(contentsLeft + contentsWidth - 150.0F,
					contentsTop + contentsHeight - 30.0F, 150.0F, 30.0F);
                @button.Activated = spades::ui::EventHandler(this.OnOkPressed);
                AddChild(button);
            }
            {
                spades::ui::Button button(Manager);
                button.Caption = _Tr("Client", "Say Global");
                button.Bounds = AABB2(contentsLeft,
					contentsTop + contentsHeight - 30.0F, 150.0F, 30.0F);
                @button.Activated = spades::ui::EventHandler(this.OnGlobalChat);
                AddChild(button);
                @this.sayButton1 = button;
            }
            {
                spades::ui::Button button(Manager);
                button.Caption = _Tr("Client", "Say Team");
                button.Bounds = AABB2(contentsLeft + 155.0F,
					contentsTop + contentsHeight - 30.0F, 150.0F, 30.0F);
                @button.Activated = spades::ui::EventHandler(this.OnTeamChat);
                AddChild(button);
                @this.sayButton2 = button;
            }
            {
                spades::ui::TextViewer viewer(Manager);
                AddChild(viewer);
                viewer.Bounds = AABB2(contentsLeft, contentsTop,
					contentsWidth, contentsHeight - 40.0F);
                @this.viewer = viewer;
            }
        }

        void ScrollToEnd() {
            viewer.Layout();
            viewer.ScrollToEnd();
        }

        void Close() { @ui.ActiveUI = null; }

        void SayWindowClosed() {
            @sayWindow = null;
            sayButton1.Enable = true;
            sayButton2.Enable = true;
        }

        private void OnOkPressed(spades::ui::UIElement@ sender) { Close(); }

		private void OnChat(bool isTeam) {
			if (sayWindow !is null) {
				sayWindow.IsTeamChat = true;
				return;
			}

			sayButton1.Enable = false;
			sayButton2.Enable = false;
			ChatLogSayWindow wnd(this, isTeam);
			AddChild(wnd);
			wnd.Bounds = this.Bounds;
			@this.sayWindow = wnd;
			@Manager.ActiveElement = wnd.field;
		}

        private void OnTeamChat(spades::ui::UIElement@ sender) { OnChat(true); }
		private void OnGlobalChat(spades::ui::UIElement@ sender) { OnChat(false); }

        void HotKey(string key) {
            if (sayWindow !is null) {
                UIElement::HotKey(key);
                return;
            }

			if (IsEnabled and (key == "Escape"))
                Close();
            else if (IsEnabled and (key == "y"))
                OnTeamChat(this);
            else if (IsEnabled and (key == "t"))
                OnGlobalChat(this);
            else
                UIElement::HotKey(key);
        }

        void Record(string text, Vector4 color) {
            color.x = Mix(color.x, 1.0F, 0.5F);
            color.y = Mix(color.y, 1.0F, 0.5F);
            color.z = Mix(color.z, 1.0F, 0.5F);
            color.w = 1.0F;
            viewer.AddLine(text, this.IsVisible, color);
        }

        void Render() {
            Vector2 pos = ScreenPosition;
            Vector2 size = Size;
            Renderer@ r = Manager.Renderer;

            r.ColorNP = Vector4(1, 1, 1, 0.08F);
            r.DrawImage(null, AABB2(pos.x, pos.y + contentsTop - 15.0F, size.x, 1.0F));
            r.DrawImage(null, AABB2(pos.x, pos.y + contentsTop + contentsHeight + 15.0F, size.x, 1.0F));
            r.ColorNP = Vector4(1, 1, 1, 0.2F);
            r.DrawImage(null, AABB2(pos.x, pos.y + contentsTop - 14.0F, size.x, 1.0F));
            r.DrawImage(null, AABB2(pos.x, pos.y + contentsTop + contentsHeight + 14.0F, size.x, 1.0F));

            UIElement::Render();
        }
    }
}
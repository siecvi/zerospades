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

#include "CountryFlags.as"

namespace spades {

    class ServerListItem : spades::ui::ButtonBase {
        MainScreenServerItem@ item;
        FlagIconRenderer@ flagIconRenderer;
        ServerListItem(spades::ui::UIManager@ manager, MainScreenServerItem@ item) {
            super(manager);
            @this.item = item;
            @flagIconRenderer = FlagIconRenderer(manager.Renderer);
        }
        void Render() {
			Renderer@ r = Manager.Renderer;
			Vector2 pos	= ScreenPosition;
			Vector2 size = Size;

			Vector4 bgcolor = Vector4(1, 1, 1, 0);
            Vector4 fgcolor = Vector4(1, 1, 1, 1);
            if (item.Favorite) {
                bgcolor = Vector4(0.3F, 0.3f, 1.0F, 0.1F);
                fgcolor = Vector4(220.0F / 255.0F, 220.0F / 255.0F, 0, 1);
            }

			if (Pressed and Hover)
                bgcolor.w = 0.3F;
            else if (Hover)
                bgcolor.w = 0.15F;

			r.ColorNP = bgcolor;
			r.DrawImage(null, AABB2(pos.x, pos.y, size.x, size.y));

			Vector4 col(1, 1, 1, 1);
			if (!Hover and (item.NumPlayers == 0)) {
				col.w *= 0.5F;
				fgcolor *= 0.5F;
			}

			// Draw server name
			Font.Draw(item.Name, ScreenPosition + Vector2(4.0F, 2.0F), 1.0F, fgcolor);

			// Draw server slots
			string playersStr = ToString(item.NumPlayers) + "/" + ToString(item.MaxPlayers);
			Vector4 playersCol = col;
			if (item.NumPlayers >= item.MaxPlayers)
				playersCol = Vector4(1.0F, 0.7F, 0.7F, col.w);
			else if (item.NumPlayers >= item.MaxPlayers * 3 / 4)
				playersCol = Vector4(1.0F, 1.0F, 0.7F, col.w);
			else if (item.NumPlayers == 0)
				playersCol = Vector4(0.7F, 0.7F, 1.0F, col.w);
			Font.Draw(playersStr, pos + Vector2(279.0F - Font.Measure(playersStr).x * 0.5F, 2.0F), 1.0F, playersCol);

			// Draw map name
			Font.Draw(item.MapName, pos + Vector2(330.0F, 2.0F), 1.0F, col);

			// Draw server gamemode
			Font.Draw(item.GameMode, pos + Vector2(470.0F, 2.0F), 1.0F, col);

			// Draw server protocol
			Font.Draw(item.Protocol, pos + Vector2(570.0F, 2.0F), 1.0F, col);

			// Draw server country flag icon
			flagIconRenderer.DrawIcon(item.Country, pos + Vector2(640.0F, size.y * 0.5F));

			// Draw server ping
			string pingStr = ToString(item.Ping);
			Vector4 pingCol(Min((2.0F * item.Ping) / 300.0F, 1.0F), Min((2.0F * (300 - item.Ping)) / 300.0F, 1.0F), 0.1F, col.w);
			Font.Draw(pingStr, pos + Vector2(710.0F - Font.Measure(pingStr).x, 2.0F), 1.0F, pingCol);
		}
	}

    funcdef void ServerListItemEventHandler(ServerListModel@ sender, MainScreenServerItem@ item);

    class ServerListModel : spades::ui::ListViewModel {
        spades::ui::UIManager@ manager;
        MainScreenServerItem @[] @list;

        ServerListItemEventHandler @ItemActivated;
        ServerListItemEventHandler @ItemDoubleClicked;
        ServerListItemEventHandler @ItemRightClicked;

        ServerListModel(spades::ui::UIManager @manager, MainScreenServerItem @[] @list) {
            @this.manager = manager;
            @this.list = list;
        }
        int NumRows {
            get { return int(list.length); }
        }
        private void OnItemClicked(spades::ui::UIElement@ sender) {
            ServerListItem@ item = cast<ServerListItem>(sender);
            if (ItemActivated !is null)
                ItemActivated(this, item.item);
        }
        private void OnItemDoubleClicked(spades::ui::UIElement@ sender) {
            ServerListItem@ item = cast<ServerListItem>(sender);
            if (ItemDoubleClicked !is null)
                ItemDoubleClicked(this, item.item);
        }
        private void OnItemRightClicked(spades::ui::UIElement@ sender) {
            ServerListItem@ item = cast<ServerListItem>(sender);
            if (ItemRightClicked !is null)
                ItemRightClicked(this, item.item);
        }
        spades::ui::UIElement@ CreateElement(int row) {
            ServerListItem i(manager, list[row]);
            @i.Activated = spades::ui::EventHandler(this.OnItemClicked);
            @i.DoubleClicked = spades::ui::EventHandler(this.OnItemDoubleClicked);
            @i.RightClicked = spades::ui::EventHandler(this.OnItemRightClicked);
            return i;
        }
        void RecycleElement(spades::ui::UIElement@ elem) {}
    }

    class ServerListHeader : spades::ui::ButtonBase {
        string Text;
        ServerListHeader(spades::ui::UIManager @manager) { super(manager); }
        void OnActivated() { ButtonBase::OnActivated(); }
        void Render() {
            Renderer@ r = Manager.Renderer;
            Vector2 pos = ScreenPosition;
            Vector2 size = Size;

            if (Pressed and Hover)
                r.ColorNP = Vector4(1.f, 1.f, 1.f, 0.3f);
            else if (Hover)
                r.ColorNP = Vector4(1.f, 1.f, 1.f, 0.15f);
            else
                r.ColorNP = Vector4(1.f, 1.f, 1.f, 0.0f);
            r.DrawImage(null, AABB2(pos.x - 2.f, pos.y, size.x, size.y));

            Font.Draw(Text, ScreenPosition + Vector2(0.f, 2.f), 1.f, Vector4(1, 1, 1, 1));
        }
    }

    class MainScreenServerListLoadingView : spades::ui::UIElement {
        MainScreenServerListLoadingView(spades::ui::UIManager@ manager) { super(manager); }
        void Render() {
            Renderer@ r = Manager.Renderer;
            Vector2 pos = ScreenPosition;
            Vector2 size = Size;
            Font@ font = this.Font;
            string text = _Tr("MainScreen", "Loading...");
            Vector2 txtSize = font.Measure(text);
            Vector2 txtPos;
            txtPos = pos + (size - txtSize) * 0.5f;

            font.Draw(text, txtPos, 1.f, Vector4(1, 1, 1, 0.8));
        }
    }

    class MainScreenServerListErrorView : spades::ui::UIElement {
        MainScreenServerListErrorView(spades::ui::UIManager@ manager) { super(manager); }
        void Render() {
            Renderer@ r = Manager.Renderer;
            Vector2 pos = ScreenPosition;
            Vector2 size = Size;
            Font@ font = this.Font;
            string text = _Tr("MainScreen", "Failed to fetch the server list.");
            Vector2 txtSize = font.Measure(text);
            Vector2 txtPos;
            txtPos = pos + (size - txtSize) * 0.5F;

            font.Draw(text, txtPos, 1.0F, Vector4(1, 1, 1, 0.8));
        }
    }
}
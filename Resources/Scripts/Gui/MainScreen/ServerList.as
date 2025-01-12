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

#include "CountryFlags.as"

namespace spades {

	class ServerListItem : spades::ui::ButtonBase {
		MainScreenHelper@ helper;
		MainScreenServerItem@ item;
		FlagIconRenderer@ flagIconRenderer;
		ServerListItem(spades::ui::UIManager@ manager, MainScreenHelper@ helper, MainScreenServerItem@ item) {
			super(manager);
			@this.helper = helper;
			@this.item = item;
			@flagIconRenderer = FlagIconRenderer(manager.Renderer);
		}
		void Render() {
			Renderer@ r = Manager.Renderer;
			Vector2 pos	= ScreenPosition;
			Vector2 size = Size;

			// adjust based on screen width
			float maxContentsWidth = 750.0F;
			float scaleF = Min(Manager.ScreenWidth / maxContentsWidth, 1.0F);

			float itemOffsetX = 2.0F;
			float itemOffsetY = 2.0F;
			float flagIconOffsetX = (itemOffsetX + 12.0F) * scaleF;
			float nameOffsetX = (itemOffsetX + 24.0F) * scaleF;
			float slotsOffsetX = 335.0F * scaleF;
			float mapNameOffsetX = 372.0F * scaleF;
			float gameModeOffsetX = 580.0F * scaleF;
			float protocolOffsetX = 658.0F * scaleF;
			float pingOffsetX = size.x - itemOffsetX;

			Vector4 bgcolor = Vector4(1.0F, 1.0F, 1.0F, 0.0F);
			Vector4 fgcolor = Vector4(1.0F, 1.0F, 1.0F, 1.0F);

			if (item.Favorite) {
				bgcolor = Vector4(0.3F, 0.3F, 1.0F, 0.1F);
				fgcolor = Vector4(220, 220, 0, 255) / 255.0F;
			}

			if (Pressed and Hover) {
				bgcolor.w = 0.3F;
			} else if (Hover) {
				bgcolor.w = 0.15F;
			}

			r.ColorNP = bgcolor;
			r.DrawImage(null, AABB2(pos.x + 1.0F, pos.y + 1.0F, size.x, size.y));

			Vector4 col = Vector4(1.0F, 1.0F, 1.0F, 1.0F);
			if (not Hover and not item.Favorite and item.NumPlayers == 0) {
				col.w *= 0.5F;
				fgcolor.w *= 0.5F;
			}

			// Draw server country flag icon
			r.ColorNP = col;
			flagIconRenderer.DrawIcon(item.Country, pos + Vector2(flagIconOffsetX, itemOffsetY + (size.y * 0.5F)));

			// Draw server name
			Font.Draw(item.Name, pos + Vector2(nameOffsetX, itemOffsetY), 1.0F, fgcolor);

			// Draw server slots
			string playersStr = ToString(item.NumPlayers) + "/" + ToString(item.MaxPlayers);
			Vector4 playersCol = col;
			if (item.NumPlayers >= item.MaxPlayers)
				playersCol = Vector4(1.0F, 0.7F, 0.7F, col.w);
			else if (item.NumPlayers >= item.MaxPlayers * 3 / 4)
				playersCol = Vector4(1.0F, 1.0F, 0.7F, col.w);
			else if (item.NumPlayers == 0)
				playersCol = Vector4(0.7F, 0.7F, 1.0F, col.w);
			Font.Draw(playersStr, pos + Vector2(slotsOffsetX - Font.Measure(playersStr).x * 0.5F, itemOffsetY), 1.0F, playersCol);

			// Draw map name
			Font.Draw(item.MapName, pos + Vector2(mapNameOffsetX, itemOffsetY), 1.0F, col);

			// Draw server gamemode
			Font.Draw(item.GameMode, pos + Vector2(gameModeOffsetX - Font.Measure(item.GameMode).x * 0.5F, itemOffsetY), 1.0F, col);

			// Draw server protocol
			Font.Draw(item.Protocol, pos + Vector2(protocolOffsetX - Font.Measure(item.Protocol).x * 0.5F, itemOffsetY), 1.0F, col);

			// Draw server ping
			int ping = helper.GetServerPing(item.Address);
			string pingStr = (ping == -1) ? "?" : ToString(ping);

			Vector4 pingCol = Vector4(1.0F, 1.0F, 1.0F, col.w);
			if (ping != -1) {
				int maxPing = 300;
				float ratio = float(Min(ping, maxPing)) / float(maxPing);
				float hue = (1.0F - ratio) * 240.0F / 360.0F;
				Vector3 gradient = HSV(hue, 0.9F, 1.0F);
				pingCol.x = gradient.x;
				pingCol.y = gradient.y;
				pingCol.z = gradient.z;
			}

			Font.Draw(pingStr, pos + Vector2(pingOffsetX - Font.Measure(pingStr).x, itemOffsetY), 1.0F, pingCol);
		}
	}

	funcdef void ServerListItemEventHandler(ServerListModel@ sender, MainScreenServerItem@ item);

	class ServerListModel : spades::ui::ListViewModel {
		spades::ui::UIManager@ manager;
		MainScreenHelper@ helper;
		MainScreenServerItem @[] @list;
		ServerListItem@[]@ itemElements;

		ServerListItemEventHandler@ ItemActivated;
		ServerListItemEventHandler@ ItemDoubleClicked;
		ServerListItemEventHandler@ ItemRightClicked;

		ServerListModel(spades::ui::UIManager@ manager, MainScreenHelper@ helper, MainScreenServerItem@[]@ list) {
			@this.manager = manager;
			@this.helper = helper;
			@this.list = list;

			@this.itemElements = array<ServerListItem@>();
			for (uint i = list.length; i > 0; --i)
				itemElements.insertLast(null);
		}

		void ReplaceList(MainScreenServerItem@[]@ list) {
			Assert(list.length == this.list.length);
			@this.list = list;

			// Kinda dirty hack
			for (uint i = 0, count = list.length; i < count; ++i) {
				if (itemElements[i] !is null)
					@itemElements[i].item = list[i];
			}
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
			if (itemElements[row] is null) {
				ServerListItem i(manager, helper, list[row]);
				@i.Activated = spades::ui::EventHandler(this.OnItemClicked);
				@i.DoubleClicked = spades::ui::EventHandler(this.OnItemDoubleClicked);
				@i.RightClicked = spades::ui::EventHandler(this.OnItemRightClicked);
				@itemElements[row] = i;
			}
			return itemElements[row];
		}
		void RecycleElement(spades::ui::UIElement@ elem) {}
	}

	class ServerListHeader : spades::ui::ButtonBase {
		string Text;
		Vector2 Alignment = Vector2(0.0F, 0.5F);
		ServerListHeader(spades::ui::UIManager@ manager) { super(manager); }
		void OnActivated() { ButtonBase::OnActivated(); }
		void Render() {
			Renderer@ r = Manager.Renderer;
			Vector2 pos = ScreenPosition;
			Vector2 size = Size;
			Font@ font = this.Font;

			Vector2 txtSize = font.Measure(Text);
			Vector2 txtPos = pos + (size - txtSize) * Alignment;

			if (Alignment.x == 0.0F)
				txtPos.x += 2.0F;

			if (Pressed and Hover)
				r.ColorNP = Vector4(1.0F, 1.0F, 1.0F, 0.3F);
			else if (Hover)
				r.ColorNP = Vector4(1.0F, 1.0F, 1.0F, 0.15F);
			else
				r.ColorNP = Vector4(1.0F, 1.0F, 1.0F, 0.0F);
			r.DrawImage(null, AABB2(pos.x, pos.y, size.x, size.y));

			Font.Draw(Text, txtPos, 1.0F, Vector4(1.0F, 1.0F, 1.0F, 1.0F));
		}
	}

	class MainScreenServerListLoadingView : spades::ui::UIElement {
		MainScreenServerListLoadingView(spades::ui::UIManager@ manager) { super(manager); }
		void Render() {
			Renderer@ r = Manager.Renderer;
			Vector2 pos = ScreenPosition;
			Vector2 size = Size;
			Font@ font = this.Font;
			string text = _Tr("MainScreen", "Fetching server list...");

			Vector2 txtSize = font.Measure(text);
			Vector2 txtPos = pos + (size - txtSize) * 0.5F;

			font.Draw(text, txtPos, 1.0F, Vector4(1.0F, 1.0F, 1.0F, 0.8F));
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
			Vector2 txtPos = pos + (size - txtSize) * 0.5F;

			font.Draw(text, txtPos, 1.0F, Vector4(1.0F, 1.0F, 1.0F, 0.8F));
		}
	}
}

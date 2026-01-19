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

	string StripDemoPath(string path) {
		// Remove "Demos/" prefix
		if (path.length > 6 and path.substr(0, 6) == "Demos/")
			path = path.substr(6);
		// Remove ".dem" suffix
		if (path.length > 4 and path.substr(path.length - 4) == ".dem")
			path = path.substr(0, path.length - 4);
		return path;
	}

	class DemoListItem : spades::ui::ButtonBase {
		string filename; // full path

		DemoListItem(spades::ui::UIManager@ manager, string filename) {
			super(manager);
			this.filename = filename;
		}

		void Render() {
			Renderer@ r = Manager.Renderer;
			Vector2 pos = ScreenPosition;
			Vector2 size = Size;

			Vector4 bgcolor = Vector4(1.0F, 1.0F, 1.0F, 0.0F);
			Vector4 fgcolor = Vector4(1.0F, 1.0F, 1.0F, 1.0F);

			if (Pressed and Hover) {
				bgcolor.w = 0.3F;
			} else if (Hover) {
				bgcolor.w = 0.15F;
			}

			r.ColorNP = bgcolor;
			r.DrawImage(null, AABB2(pos.x + 1.0F, pos.y + 1.0F, size.x, size.y));

			string display = StripDemoPath(filename);
			Font.Draw(display, pos + Vector2(4.0F, 2.0F), 1.0F, fgcolor);
		}
	}

	funcdef void DemoListItemEventHandler(DemoListModel@ sender, string filename);

	class DemoListModel : spades::ui::ListViewModel {
		spades::ui::UIManager@ manager;
		string[] list;
		DemoListItem@[] itemElements;

		DemoListItemEventHandler@ ItemActivated;
		DemoListItemEventHandler@ ItemDoubleClicked;

		DemoListModel(spades::ui::UIManager@ manager, string[]@ list) {
			@this.manager = manager;
			this.list = list;

			itemElements.resize(list.length);
		}

		int NumRows {
			get { return int(list.length); }
		}

		private void OnItemClicked(spades::ui::UIElement@ sender) {
			DemoListItem@ item = cast<DemoListItem>(sender);
			if (ItemActivated !is null)
				ItemActivated(this, item.filename);
		}

		private void OnItemDoubleClicked(spades::ui::UIElement@ sender) {
			DemoListItem@ item = cast<DemoListItem>(sender);
			if (ItemDoubleClicked !is null)
				ItemDoubleClicked(this, item.filename);
		}

		spades::ui::UIElement@ CreateElement(int row) {
			if (itemElements[row] is null) {
				DemoListItem i(manager, list[row]);
				@i.Activated = spades::ui::EventHandler(this.OnItemClicked);
				@i.DoubleClicked = spades::ui::EventHandler(this.OnItemDoubleClicked);
				@itemElements[row] = i;
			}
			return itemElements[row];
		}

		void RecycleElement(spades::ui::UIElement@ elem) {}
	}

	class DemoListHeader : spades::ui::UIElement {
		string Text;
		DemoListHeader(spades::ui::UIManager@ manager) { super(manager); }
		void Render() {
			Renderer@ r = Manager.Renderer;
			Vector2 pos = ScreenPosition;
			Vector2 size = Size;

			Font.Draw(Text, pos + Vector2(2.0F, (size.y - Font.Measure(Text).y) * 0.5F), 1.0F,
			          Vector4(1.0F, 1.0F, 1.0F, 1.0F));
		}
	}

}

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

#include "Button.as"

namespace spades {
	namespace ui {

		class SimpleTabStripItem : ButtonBase {
			UIElement@ linkedElement;

			SimpleTabStripItem(UIManager@ manager, UIElement@ linkedElement) {
				super(manager);
				@this.linkedElement = linkedElement;
			}

			void MouseDown(MouseButton button, Vector2 clientPosition) {
				PlayActivateSound();
				OnActivated();
			}

			void Render() {
				this.Toggled = linkedElement.Visible;

				Renderer@ r = Manager.Renderer;
				Vector2 pos = ScreenPosition;
				Vector2 size = Size;
				Vector4 textColor(0.9F, 0.9F, 0.9F, 1.0F);

				if (Toggled) {
					r.ColorNP = Vector4(0.9F, 0.9F, 0.9F, 1.0F);
					textColor = Vector4(0.0F, 0.0F, 0.0F, 1.0F);
				} else if (Hover) {
					r.ColorNP = Vector4(1.0F, 1.0F, 1.0F, 0.3F);
				} else {
					r.ColorNP = Vector4(1.0F, 1.0F, 1.0F, 0.0F);
				}
				r.DrawImage(null, AABB2(pos.x, pos.y, size.x, size.y));

				Vector2 txtSize = Font.Measure(Caption);
				Font.Draw(Caption, pos + (size - txtSize) * 0.5F, 1.0F, textColor);
			}
		}

		class SimpleTabStrip : UIElement {
			private float nextX = 0.0F;

			EventHandler@ Changed;

			SimpleTabStrip(UIManager@ manager) { super(manager); }

			private void OnChanged() {
				if (Changed !is null)
					Changed(this);
			}

			private void OnItemActivated(UIElement @sender) {
				SimpleTabStripItem@ item = cast<SimpleTabStripItem>(sender);
				UIElement@ linked = item.linkedElement;
				UIElement @[] @children = this.GetChildren();
				for (uint i = 0, count = children.length; i < count; i++) {
					SimpleTabStripItem@ otherItem = cast<SimpleTabStripItem>(children[i]);
					otherItem.linkedElement.Visible = (otherItem.linkedElement is linked);
				}
				OnChanged();
			}

			void AddItem(string title, UIElement@ linkedElement) {
				SimpleTabStripItem item(this.Manager, linkedElement);
				item.Caption = title;
				float w = this.Font.Measure(title).x + 18.0F;
				item.Bounds = AABB2(nextX, 0.0F, w, 24.0F);
				nextX += w + 4.0F;

				@item.Activated = EventHandler(this.OnItemActivated);

				this.AddChild(item);
			}

			void Render() {
				UIElement::Render();

				Renderer@ r = Manager.Renderer;
				Vector2 pos = ScreenPosition;
				Vector2 size = Size;

				r.ColorNP = Vector4(0.9F, 0.9F, 0.9F, 1.0F);
				r.DrawImage(null, AABB2(pos.x, pos.y + 24.0F, size.x, 1.0F));
			}
		}
	}
}

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

#include "UIFramework.as"

namespace spades {
	namespace ui {

		enum ScrollBarOrientation { Horizontal, Vertical }

		class ScrollBarBase : UIElement {
			double MinValue = 0.0;
			double MaxValue = 100.0;
			double Value = 0.0;
			double SmallChange = 1.0;
			double LargeChange = 20.0;
			EventHandler@ Changed;

			ScrollBarBase(UIManager@ manager) { super(manager); }

			void ScrollBy(double delta) { ScrollTo(Value + delta); }

			void ScrollTo(double val) {
				val = Clamp(val, MinValue, MaxValue);
				if (val == Value)
					return;
				Value = val;
				OnChanged();
			}

			void OnChanged() {
				if (Changed !is null)
					Changed(this);
			}

			ScrollBarOrientation Orientation {
				get {
					if (Size.x > Size.y)
						return spades::ui::ScrollBarOrientation::Horizontal;
					else
						return spades::ui::ScrollBarOrientation::Vertical;
				}
			}
		}

		class ScrollBarTrackBar : UIElement {
			private ScrollBar@ scrollBar;
			private bool dragging = false;
			private double startValue;
			private float startCursorPos;
			private bool hover = false;

			ScrollBarTrackBar(ScrollBar@ scrollBar) {
				super(scrollBar.Manager);
				@this.scrollBar = scrollBar;
				IsMouseInteractive = true;
			}

			private float GetCursorPos(Vector2 pos) {
				if (scrollBar.Orientation == spades::ui::ScrollBarOrientation::Horizontal)
					return pos.x + Position.x;
				else
					return pos.y + Position.y;
			}

			void MouseDown(MouseButton button, Vector2 clientPosition) {
				if (button != spades::ui::MouseButton::LeftMouseButton)
					return;
				if (scrollBar.TrackBarMovementRange < 0.0001F)
					return; // immobile

				dragging = true;
				startValue = scrollBar.Value;
				startCursorPos = GetCursorPos(clientPosition);
			}
			void MouseMove(Vector2 clientPosition) {
				if (dragging) {
					double val = startValue;
					float delta = GetCursorPos(clientPosition) - startCursorPos;
					val += delta * (scrollBar.MaxValue - scrollBar.MinValue) /
						   double(scrollBar.TrackBarMovementRange);
					scrollBar.ScrollTo(val);
				}
			}
			void MouseUp(MouseButton button, Vector2 clientPosition) {
				if (button != spades::ui::MouseButton::LeftMouseButton)
					return;
				dragging = false;
			}
			void MouseEnter() {
				hover = true;
				UIElement::MouseEnter();
			}
			void MouseLeave() {
				hover = false;
				UIElement::MouseLeave();
			}

			void Render() {
				Renderer@ r = Manager.Renderer;
				Vector2 pos = ScreenPosition;
				Vector2 size = Size;

				if (scrollBar.Orientation == spades::ui::ScrollBarOrientation::Horizontal) {
					pos.y += 4.0F;
					size.y -= 8.0F;
				} else {
					pos.x += 4.0F;
					size.x -= 8.0F;
				}

				if (dragging)
					r.ColorNP = Vector4(1.0F, 1.0F, 1.0F, 0.4F);
				else if (hover)
					r.ColorNP = Vector4(1.0F, 1.0F, 1.0F, 0.2F);
				else
					r.ColorNP = Vector4(1.0F, 1.0F, 1.0F, 0.1F);
				r.DrawImage(null, AABB2(pos.x, pos.y, size.x, size.y));
			}
		}

		class ScrollBarFill : ButtonBase {
			private ScrollBarBase@ scrollBar;
			private bool up;

			ScrollBarFill(ScrollBarBase@ scrollBar, bool up) {
				super(scrollBar.Manager);
				@this.scrollBar = scrollBar;
				IsMouseInteractive = true;
				Repeat = true;
				this.up = up;
			}

			void PlayMouseEnterSound() {} // suppress
			void PlayActivateSound() {} // suppress
			void Render() {} // nothing to draw
		}

		class ScrollBarButton : ButtonBase {
			private ScrollBar@ scrollBar;
			private bool up;
			private Image@ image;

			ScrollBarButton(ScrollBar@ scrollBar, bool up) {
				super(scrollBar.Manager);
				@this.scrollBar = scrollBar;
				IsMouseInteractive = true;
				Repeat = true;
				this.up = up;
				@image = Manager.Renderer.RegisterImage("Gfx/UI/ScrollArrow.png");
			}

			void PlayMouseEnterSound() {} // suppress
			void PlayActivateSound() {} // suppress

			void Render() {
				Renderer@ r = Manager.Renderer;
				Vector2 pos = ScreenPosition;
				Vector2 size = Size;
				pos += size * 0.5F;
				float siz = image.Width * 0.5F;
				AABB2 srcRect(0.0F, 0.0F, image.Width, image.Height);

				if (Pressed and Hover)
					r.ColorNP = Vector4(1.0F, 1.0F, 1.0F, 0.6F);
				else if (Hover)
					r.ColorNP = Vector4(1.0F, 1.0F, 1.0F, 0.4F);
				else
					r.ColorNP = Vector4(1.0F, 1.0F, 1.0F, 0.2F);

				if (scrollBar.Orientation == spades::ui::ScrollBarOrientation::Horizontal) {
					if (up) {
						r.DrawImage(image, Vector2(pos.x + siz, pos.y - siz),
									Vector2(pos.x + siz, pos.y + siz),
									Vector2(pos.x - siz, pos.y - siz), srcRect);
					} else {
						r.DrawImage(image, Vector2(pos.x - siz, pos.y + siz),
									Vector2(pos.x - siz, pos.y - siz),
									Vector2(pos.x + siz, pos.y + siz), srcRect);
					}
				} else {
					if (up) {
						r.DrawImage(image, Vector2(pos.x - siz, pos.y + siz),
									Vector2(pos.x + siz, pos.y + siz),
									Vector2(pos.x - siz, pos.y - siz), srcRect);
					} else {
						r.DrawImage(image, Vector2(pos.x - siz, pos.y - siz),
									Vector2(pos.x + siz, pos.y - siz),
									Vector2(pos.x - siz, pos.y + siz), srcRect);
					}
				}
			}
		}

		class ScrollBar : ScrollBarBase {
			private ScrollBarTrackBar@ trackBar;
			private ScrollBarFill@ fill1;
			private ScrollBarFill@ fill2;
			private ScrollBarButton@ button1;
			private ScrollBarButton@ button2;

			private float ButtonSize = 16.0F;

			ScrollBar(UIManager@ manager) {
				super(manager);

				@trackBar = ScrollBarTrackBar(this);
				AddChild(trackBar);

				@fill1 = ScrollBarFill(this, false);
				@fill1.Activated = EventHandler(this.LargeDown);
				AddChild(fill1);
				@fill2 = ScrollBarFill(this, true);
				@fill2.Activated = EventHandler(this.LargeUp);
				AddChild(fill2);

				@button1 = ScrollBarButton(this, false);
				@button1.Activated = EventHandler(this.SmallDown);
				AddChild(button1);
				@button2 = ScrollBarButton(this, true);
				@button2.Activated = EventHandler(this.SmallUp);
				AddChild(button2);
			}

			private void LargeDown(UIElement@ e) { ScrollBy(-LargeChange); }
			private void LargeUp(UIElement@ e) { ScrollBy(LargeChange); }
			private void SmallDown(UIElement@ e) { ScrollBy(-SmallChange); }
			private void SmallUp(UIElement@ e) { ScrollBy(SmallChange); }

			void OnChanged() {
				Layout();
				ScrollBarBase::OnChanged();
				Layout();
			}

			void Layout() {
				Vector2 size = Size;
				if (Orientation == spades::ui::ScrollBarOrientation::Horizontal) {
					button1.Bounds = AABB2(0, 0, ButtonSize, size.y);
					button2.Bounds = AABB2(size.x - ButtonSize, 0, ButtonSize, size.y);
					fill1.Bounds = AABB2(ButtonSize, 0, TrackBarPosition - ButtonSize, size.y);
					fill2.Bounds = AABB2(TrackBarPosition + TrackBarLength, 0,
						size.x - ButtonSize - TrackBarPosition - TrackBarLength, size.y);
					trackBar.Bounds = AABB2(TrackBarPosition, 0, TrackBarLength, size.y);
				} else {
					button1.Bounds = AABB2(0, 0, size.x, ButtonSize);
					button2.Bounds = AABB2(0, size.y - ButtonSize, size.x, ButtonSize);
					fill1.Bounds = AABB2(0, ButtonSize, size.x, TrackBarPosition - ButtonSize);
					fill2.Bounds = AABB2(0, TrackBarPosition + TrackBarLength, size.x,
						size.y - ButtonSize - TrackBarPosition - TrackBarLength);
					trackBar.Bounds = AABB2(0, TrackBarPosition, size.x, TrackBarLength);
				}
			}

			void OnResized() {
				Layout();
				UIElement::OnResized();
			}

			float Length {
				get {
					if (Orientation == spades::ui::ScrollBarOrientation::Horizontal)
						return Size.x;
					else
						return Size.y;
				}
			}

			float TrackBarAreaLength { get { return Length - ButtonSize - ButtonSize; } }
			float TrackBarLength { get { return Max(TrackBarAreaLength * (LargeChange / (MaxValue - MinValue + LargeChange)), 16.0F); } }
			float TrackBarMovementRange { get { return TrackBarAreaLength - TrackBarLength; } }

			float TrackBarPosition {
				get {
					if (MaxValue == MinValue)
						return ButtonSize;
					return float((Value - MinValue) / (MaxValue - MinValue) * TrackBarMovementRange) + ButtonSize;
				}
			}

			void Render() {
				Layout();
				ScrollBarBase::Render();
			}
		}
	}
}

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

#include "ScrollBar.as"

namespace spades {
    namespace ui {

        class SliderKnob : UIElement {
            private Slider@ slider;
            private bool dragging = false;
            private double startValue;
            private float startCursorPos;
            private bool hover = false;

            SliderKnob(Slider@ slider) {
                super(slider.Manager);
                @this.slider = slider;
                IsMouseInteractive = true;
            }

            private float GetCursorPos(Vector2 pos) { return pos.x + Position.x; }

            void MouseDown(MouseButton button, Vector2 clientPosition) {
                if (button != spades::ui::MouseButton::LeftMouseButton)
                    return;
                dragging = true;
                startValue = slider.Value;
                startCursorPos = GetCursorPos(clientPosition);
            }
            void MouseMove(Vector2 clientPosition) {
                if (dragging) {
                    double val = startValue;
                    float delta = GetCursorPos(clientPosition) - startCursorPos;
                    val += delta * (slider.MaxValue - slider.MinValue) /
                           double(slider.TrackBarMovementRange);
                    slider.ScrollTo(val);
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

                if (hover)
                    r.ColorNP = Vector4(1.0F, 1.0F, 1.0F, 0.5F);
                else
                    r.ColorNP = Vector4(1.0F, 1.0F, 1.0F, 0.3F);
                r.DrawImage(null, AABB2(pos.x + size.x * 0.5F - 3.0F, pos.y, 6.0F, size.y));

                r.ColorNP = Vector4(0.0F, 0.0F, 0.0F, 0.6F);
                r.DrawImage(null, AABB2(pos.x + size.x * 0.5F - 2.0F, pos.y + 1.0F, 4.0F, size.y - 2.0F));
            }
        }

        class Slider : ScrollBarBase {
            private SliderKnob@ knob;
            private ScrollBarFill@ fill1;
            private ScrollBarFill@ fill2;

            Slider(UIManager@ manager) {
                super(manager);

                @knob = SliderKnob(this);
                AddChild(knob);

                @fill1 = ScrollBarFill(this, false);
                @fill1.Activated = EventHandler(this.LargeDown);
                AddChild(fill1);
                @fill2 = ScrollBarFill(this, true);
                @fill2.Activated = EventHandler(this.LargeUp);
                AddChild(fill2);
            }

            private void LargeDown(UIElement@ e) { ScrollBy(-LargeChange); }
            private void LargeUp(UIElement@ e) {
                ScrollBy(LargeChange);
            }

            void OnChanged() {
                Layout();
                ScrollBarBase::OnChanged();
                Layout();
            }

            void Layout() {
                Vector2 size = Size;
                float tPos = TrackBarPosition;
                float tLen = TrackBarLength;
                fill1.Bounds = AABB2(0.0F, 0.0F, tPos, size.y);
                fill2.Bounds = AABB2(tPos + tLen, 0.0F, size.x - tPos - tLen, size.y);
                knob.Bounds = AABB2(tPos, 0.0F, tLen, size.y);
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

            float TrackBarAreaLength { get { return Length; } }
            float TrackBarLength { get { return 16.0F; } }
            float TrackBarMovementRange { get { return TrackBarAreaLength - TrackBarLength; } }
            float TrackBarPosition { get { return float((Value - MinValue) / (MaxValue - MinValue) * TrackBarMovementRange); } }

            void Render() {
                Layout();

                Renderer@ r = Manager.Renderer;
                Vector2 pos = ScreenPosition;
                Vector2 size = Size;

                r.ColorNP = Vector4(1.0F, 1.0F, 1.0F, 0.1F);
                r.DrawImage(null, AABB2(pos.x, pos.y + size.y * 0.5F - 3.0F, size.x, 6.0F));

                r.ColorNP = Vector4(0.0F, 0.0F, 0.0F, 0.2F);
                r.DrawImage(null, AABB2(pos.x + 1.0F, pos.y + size.y * 0.5F - 2.0F, size.x - 2.0F, 4.0F));

                ScrollBarBase::Render();
            }
        }
    }
}
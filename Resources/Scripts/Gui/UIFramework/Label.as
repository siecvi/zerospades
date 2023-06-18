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

#include "UIFramework.as"

namespace spades {
    namespace ui {

        class Label : UIElement {
            string Text;
            Vector4 BackgroundColor = Vector4(0, 0, 0, 0);
            Vector4 TextColor = Vector4(1, 1, 1, 1);
            Vector2 Alignment = Vector2(0.0F, 0.0F);
            float TextScale = 1.0F;

            Label(UIManager@ manager) { super(manager); }
            void Render() {
                Renderer@ r = Manager.Renderer;
                Vector2 pos = ScreenPosition;
                Vector2 size = Size;

                if (BackgroundColor.w > 0.0F) {
                    r.ColorNP = BackgroundColor;
                    r.DrawImage(null, AABB2(pos.x, pos.y, size.x, size.y));
                }

                if (Text.length > 0) {
                    Font@ font = this.Font;
                    string text = this.Text;
                    Vector2 textSize = font.Measure(text) * TextScale;
                    Vector2 textPos = pos + (size - textSize) * Alignment;

                    font.Draw(text, textPos, TextScale, TextColor);
                }
            }
        }
    }
}

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

namespace spades {
    // AngelScript doesn't seem to support user-defined template functions...
    uint Min(uint a, uint b) { return (a < b) ? a : b; }
    uint Max(uint a, uint b) { return (a > b) ? a : b; }
    int Min(int a, int b) { return (a < b) ? a : b; }
    int Max(int a, int b) { return (a > b) ? a : b; }
    float Min(float a, float b) { return (a < b) ? a : b; }
    float Max(float a, float b) { return (a > b) ? a : b; }
    uint Clamp(uint v, uint lo, uint hi) { return Min(Max(v, lo), hi); }
    int Clamp(int v, int lo, int hi) { return Min(Max(v, lo), hi); }
    float Clamp(float v, float lo, float hi) { return Min(Max(v, lo), hi); }

    void DrawCrosshairRect(Renderer@ r, int x, int y, int w, int h, Vector4 color,
        bool drawOutline, float outlineAlpha, float outlineThickness, bool outlineRounded) {
        if (drawOutline) {
            r.ColorNP = Vector4(0.0F, 0.0F, 0.0F, outlineAlpha);
            if (outlineRounded) {
                r.DrawFilledRect(x, y-outlineThickness, w, y);
                r.DrawFilledRect(x, h, w, h+outlineThickness);
            } else {
                r.DrawFilledRect(x-outlineThickness, y-outlineThickness, w+outlineThickness, y);
                r.DrawFilledRect(x-outlineThickness, h, w+outlineThickness, h+outlineThickness);
            }
            r.DrawFilledRect(x-outlineThickness, y, x, h);
            r.DrawFilledRect(w, y, w+outlineThickness, h);
        }

        r.ColorNP = color;
        r.DrawFilledRect(x, y, w, h);
    }

    void DrawCrosshair(Renderer@ r, Vector2 pos,
		bool drawLines, bool useTStyle, float gap, float size, float thickness, Vector4 color,
        bool drawDot, float dotAlpha, float dotThickness,
        bool drawOutline, float outlineAlpha, float outlineThickness, bool outlineRounded) {
        int cx = int(pos.x);
        int cy = int(pos.y);

        int barGap = int(gap);
        int barSize = int(size);
        int barThickness = int(thickness);

        int x = cx - barThickness / 2;
        int y = cy - barThickness / 2;
        int w = x + barThickness;
        int h = y + barThickness;

        if (drawLines) {
            // draw horizontal crosshair lines
            int innerLeft = x - barGap;
            int innerRight = w + barGap;
            int outerLeft = innerLeft - barSize;
            int outerRight = innerRight + barSize;
            DrawCrosshairRect(r, outerLeft, y, innerLeft, h, color,
                drawOutline, outlineAlpha, outlineThickness, outlineRounded); // left
            DrawCrosshairRect(r, innerRight, y, outerRight, h, color,
                drawOutline, outlineAlpha, outlineThickness, outlineRounded); // right

            // draw vertical crosshair lines
            int innerTop = y - barGap;
            int innerBottom = h + barGap;
            int outerTop = innerTop - barSize;
            int outerBottom = innerBottom + barSize;
            if (not useTStyle)
                DrawCrosshairRect(r, x, outerTop, w, innerTop, color,
                    drawOutline, outlineAlpha, outlineThickness, outlineRounded); // top
            DrawCrosshairRect(r, x, innerBottom, w, outerBottom, color,
                drawOutline, outlineAlpha, outlineThickness, outlineRounded); // bottom
        }

        // draw center dot
        if (drawDot) {
            color.w = dotAlpha;
            barThickness = int(dotThickness);
            x = cx - barThickness / 2;
            y = cy - barThickness / 2;
            w = x + barThickness;
            h = y + barThickness;
            DrawCrosshairRect(r, x, y, w, h, color,
                drawOutline, outlineAlpha, outlineThickness, outlineRounded);
        }
    }

    /** Returns the byte index for a certain character index. */
    int GetByteIndexForString(string s, int charIndex, int start = 0) {
        int len = s.length;

        while (start < len and charIndex > 0) {
            int c = s[start];

            if ((c & 0x80) == 0) {
                charIndex--;
                start += 1;
            } else if ((c & 0xE0) == 0xC0) {
                charIndex--;
                start += 2;
            } else if ((c & 0xF0) == 0xE0) {
                charIndex--;
                start += 3;
            } else if ((c & 0xF8) == 0xF0) {
                charIndex--;
                start += 4;
            } else if ((c & 0xFC) == 0xF8) {
                charIndex--;
                start += 5;
            } else if ((c & 0xFE) == 0xFC) {
                charIndex--;
                start += 6;
            } else {
                // invalid!
                charIndex--;
                start++;
            }
        }

        if (start > len)
            start = len;

        return start;
    }

    /** Returns the byte index for a certain character index. */
    int GetCharIndexForString(string s, int byteIndex, int start = 0) {
        int len = s.length;
        int charIndex = 0;

        while (start < len and start < byteIndex and byteIndex > 0) {
            int c = s[start];

            if ((c & 0x80) == 0) {
                charIndex++;
                start += 1;
            } else if ((c & 0xE0) == 0xC0) {
                charIndex++;
                start += 2;
            } else if ((c & 0xF0) == 0xE0) {
                charIndex++;
                start += 3;
            } else if ((c & 0xF8) == 0xF0) {
                charIndex++;
                start += 4;
            } else if ((c & 0xFC) == 0xF8) {
                charIndex++;
                start += 5;
            } else if ((c & 0xFE) == 0xFC) {
                charIndex++;
                start += 6;
            } else {
                // invalid!
                charIndex++;
                start++;
            }
        }

        return charIndex;
    }
 }

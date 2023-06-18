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
 	float Clamp(float v, float lo, float hi) { return Min(Max(v, lo), hi); }
 	int Clamp(int v, int lo, int hi) { return Min(Max(v, lo), hi); }
 	uint Clamp(uint v, uint lo, uint hi) { return Min(Max(v, lo), hi); }

	void DrawFilledRect(Renderer@ r, float x0, float y0, float x1, float y1) {
		r.DrawImage(null, AABB2(x0, y0, x1 - x0, y1 - y0));
	}
	void DrawOutlinedRect(Renderer@ r, float x0, float y0, float x1, float y1) {
		DrawFilledRect(r, x0, y0, x1, y0 + 1);         // top
		DrawFilledRect(r, x0, y1 - 1, x1, y1);	       // bottom
		DrawFilledRect(r, x0, y0 + 1, x0 + 1, y1 - 1); // left
		DrawFilledRect(r, x1 - 1, y0 + 1, x1, y1 - 1); // right
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

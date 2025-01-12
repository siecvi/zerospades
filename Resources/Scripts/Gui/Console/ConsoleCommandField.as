/*
 Copyright (c) 2019 yvt

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

#include "../Client/FieldWithHistory.as"

namespace spades {

	uint StringCommonPrefixLength(string a, string b) {
		for (uint i = 0, ln = Min(a.length, b.length); i < ln; i++) {
			if (ToLower(a[i]) != ToLower(b[i]))
				return i;
		}
		return Min(a.length, b.length);
	}

	/** Displys console command candidates. */
	class ConsoleCommandFieldCandiateView : spades::ui::UIElement {
		ConsoleCommandCandidate[]@ candidates;
		ConsoleCommandFieldCandiateView(spades::ui::UIManager@ manager,
			ConsoleCommandCandidate[]@ candidates) {
			super(manager);
			@this.candidates = candidates;
		}
		void Render() {
			float maxNameLen = 0.0F;
			float maxDescLen = 0.0F;
			Font@ font = this.Font;
			Renderer@ r = this.Manager.Renderer;
			float rowHeight = 24.0F;

			for (uint i = 0, len = candidates.length; i < len; i++) {
				maxNameLen = Max(maxNameLen, font.Measure(candidates[i].Name).x);
				maxDescLen = Max(maxDescLen, font.Measure(candidates[i].Description).x);
			}

			float w = maxNameLen + maxDescLen + 20.0F;
			float h = float(candidates.length) * rowHeight;

			Vector2 pos = this.ScreenPosition;
			pos.y += 2.0F;

			// draw background
			r.ColorNP = Vector4(0.0F, 0.0F, 0.0F, 0.75F);
			r.DrawFilledRect(pos.x + 1, pos.y - 10.0F + 1, pos.x + w - 1, pos.y - 10.0F + h - 1);

			r.ColorNP = Vector4(1.0F, 1.0F, 1.0F, 0.1F);
			r.DrawOutlinedRect(pos.x, pos.y - 10.0F, pos.x + w, pos.y - 10.0F + h);

			// draw cvar list
			Vector4 nameColor = Vector4(1.0F, 1.0F, 1.0F, 0.7F);
			Vector4 nameShadow = Vector4(0.0F, 0.0F, 0.0F, 0.3F);
			Vector4 descColor = Vector4(1.0F, 1.0F, 1.0F, 1.0F);
			Vector4 descShadow = Vector4(0.0F, 0.0F, 0.0F, 0.4F);

			for (uint i = 0, len = candidates.length; i < len; i++) {
				float y = float(i) * rowHeight - 10.0F;
				Vector2 namePos = pos + Vector2(5.0F, y);
				Vector2 descPos = pos + Vector2(15.0F + maxNameLen, y);

				font.DrawShadow(candidates[i].Name, namePos, 1.0F, nameColor, nameShadow);
				font.DrawShadow(candidates[i].Description, descPos, 1.0F, descColor, descShadow);
			}
		}
	}

	/** A `Field` with a command name autocompletion feature. */
	class ConsoleCommandField : FieldWithHistory {
		private ConsoleCommandFieldCandiateView@ valueView;
		private ConsoleHelper@ helper;

		ConsoleCommandField(spades::ui::UIManager@ manager,
			array<spades::ui::CommandHistoryItem@> @history,
			ConsoleHelper@ helper) {
			super(manager, history);

			@this.helper = helper;
		}

		void OnChanged() {
			FieldWithHistory::OnChanged();

			if (valueView !is null)
				@valueView.Parent = null;

			// Find the command name part
			int whitespace = Text.findFirst(" ");
			if (whitespace < 0)
				whitespace = int(Text.length);

			if (whitespace > 0) {
				string input = Text.substr(0, whitespace);
				ConsoleCommandCandidateIterator@ it = helper.AutocompleteCommandName(input);
				ConsoleCommandCandidate[]@ candidates = {};

				for (uint i = 0; i < 16; ++i) {
					if (!it.MoveNext())
						break;
					candidates.insertLast(it.Current);
				}

				if (candidates.length > 0) {
					@valueView = ConsoleCommandFieldCandiateView(this.Manager, candidates);
					valueView.Bounds = AABB2(0.0F, Size.y + 10.0F, 0.0F, 0.0F);
					@valueView.Parent = this;
				}
			}
		}

		void Clear() {
			FieldWithHistory::Clear();
			OnChanged();
		}

		void KeyDown(string key) {
			if (key == "Tab") {
				// Find the command name part
				int whitespace = Text.findFirst(" ");
				if (whitespace < 0)
					whitespace = int(Text.length);

				if (SelectionLength == 0 and SelectionStart <= whitespace) {
					// Command name auto completion
					string input = Text.substr(0, whitespace);
					ConsoleCommandCandidateIterator@ it = helper.AutocompleteCommandName(input);
					string commonPart;
					bool foundOne = false;
					while (it.MoveNext()) {
						string name = it.Current.Name;
						if (StringCommonPrefixLength(input, name) == input.length) {
							if (not foundOne) {
								commonPart = name;
								foundOne = true;
							}

							uint commonLen = StringCommonPrefixLength(commonPart, name);
							commonPart = commonPart.substr(0, commonLen);
						}
					}

					if (commonPart.length > input.length) {
						Text = commonPart + Text.substr(whitespace);
						Select(commonPart.length, 0);
					}
				}
			} else {
				FieldWithHistory::KeyDown(key);
			}
		}
	}
}

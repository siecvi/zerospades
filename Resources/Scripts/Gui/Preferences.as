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

	class PreferenceViewOptions {
		bool GameActive = false;
	}

	class PreferenceView : spades::ui::UIElement {
		private spades::ui::UIElement@ owner;

		private PreferenceTab @[] tabs;
		float ContentsLeft, ContentsWidth;
		float ContentsTop, ContentsHeight;
		float ContentsRight;

		int SelectedTabIndex = 0;

		spades::ui::EventHandler@ Closed;

		PreferenceView(spades::ui::UIElement@ owner,
			PreferenceViewOptions@ options, FontManager@ fontManager) {
			super(owner.Manager);
			@this.owner = owner;
			this.Bounds = owner.Bounds;

			float sw = Manager.ScreenWidth;
			float sh = Manager.ScreenHeight;

			ContentsWidth = sw - 16.0F;
			float maxContentsWidth = 800.0F;
			if (ContentsWidth > maxContentsWidth)
				ContentsWidth = maxContentsWidth;

			ContentsHeight = sh - 8.0F;
			float maxContentsHeight = 550.0F;
			if (ContentsHeight > maxContentsHeight)
				ContentsHeight = maxContentsHeight;

			ContentsTop = (sh - ContentsHeight) * 0.5F;
			ContentsLeft = (sw - ContentsWidth) * 0.5F;
			ContentsRight = ContentsLeft + ContentsWidth;

			{
				spades::ui::Label label(Manager);
				label.BackgroundColor = Vector4(0.0F, 0.0F, 0.0F, 0.9F);
				label.Bounds = AABB2(0.0F, ContentsTop - 13.0F, Size.x, ContentsHeight + 27.0F);
				AddChild(label);
			}

			AddTab(GameOptionsPanel(Manager, options, fontManager), _Tr("Preferences", "Game Options"));
			AddTab(RendererOptionsPanel(Manager, options, fontManager), _Tr("Preferences", "Graphics Settings"));
			AddTab(ControlOptionsPanel(Manager, options, fontManager), _Tr("Preferences", "Controls"));
			AddTab(MiscOptionsPanel(Manager, options, fontManager), _Tr("Preferences", "Misc"));

			{
				PreferenceTabButton button(Manager);
				button.Caption = _Tr("Preferences", "Back");
				button.HotKeyText = _Tr("Client", "[Esc]");
				button.Bounds = AABB2(ContentsLeft + 10.0F,
					ContentsTop + 10.0F + float(tabs.length) * 32.0F + 5.0F, 150.0F, 30.0F);
				@button.Activated = spades::ui::EventHandler(this.OnClosePressed);
				AddChild(button);
			}

			UpdateTabs();
		}

		private void AddTab(spades::ui::UIElement@ view, string caption) {
			PreferenceTab tab(this, view);
			tab.TabButton.Caption = caption;
			tab.TabButton.Bounds = AABB2(ContentsLeft + 10.0F,
				ContentsTop + 10.0F + float(tabs.length) * 32.0F, 150.0F, 30.0F);
			@tab.TabButton.Activated = spades::ui::EventHandler(this.OnTabButtonActivated);
			tab.View.Bounds = AABB2((ContentsLeft + 2.0F) + 150.0F,
				ContentsTop + 10.0F, ContentsWidth, ContentsHeight - 20.0F);
			tab.View.Visible = false;
			AddChild(tab.View);
			AddChild(tab.TabButton);
			tabs.insertLast(tab);
		}

		private void OnTabButtonActivated(spades::ui::UIElement@ sender) {
			for (uint i = 0; i < tabs.length; i++) {
				if (cast<spades::ui::UIElement>(tabs[i].TabButton) is sender) {
					SelectedTabIndex = i;
					UpdateTabs();
				}
			}
		}

		private void UpdateTabs() {
			for (uint i = 0; i < tabs.length; i++) {
				PreferenceTab@ tab = tabs[i];
				bool selected = SelectedTabIndex == int(i);
				tab.TabButton.Toggled = selected;
				tab.View.Visible = selected;
			}
		}

		private void OnClosePressed(spades::ui::UIElement@ sender) { Close(); }

		private void OnClosed() {
			if (Closed !is null)
				Closed(this);
		}

		void HotKey(string key) {
			if (key == "Escape") {
				Close();
			} else {
				UIElement::HotKey(key);
			}
		}

		void Render() {
			Renderer@ r = Manager.Renderer;
			Vector2 pos = ScreenPosition;
			Vector2 size = Size;

			r.ColorNP = Vector4(1.0F, 1.0F, 1.0F, 0.07F);
			r.DrawImage(null, AABB2(pos.x, pos.y + ContentsTop - 14.0F, size.x, 1.0F));
			r.DrawImage(null, AABB2(pos.x, pos.y + ContentsTop + ContentsHeight + 14.0F, size.x, 1.0F));

			UIElement::Render();
		}

		void Close() {
			owner.Enable = true;
			@this.Parent = null;
			OnClosed();
		}

		void Run() {
			owner.Enable = false;
			owner.Parent.AddChild(this);
		}
	}

	class PreferenceTabButton : spades::ui::Button {
		PreferenceTabButton(spades::ui::UIManager@ manager) {
			super(manager);
			Alignment = Vector2(0.0F, 0.5F);
		}
	}

	class PreferenceTab {
		spades::ui::UIElement@ View;
		PreferenceTabButton@ TabButton;

		PreferenceTab(PreferenceView@ parent, spades::ui::UIElement@ view) {
			@View = view;
			@TabButton = PreferenceTabButton(parent.Manager);
			TabButton.Toggle = true;
		}
	}

	class ConfigField : spades::ui::Field {
		ConfigItem@ config;
		ConfigField(spades::ui::UIManager manager, string configName) {
			super(manager);
			@config = ConfigItem(configName);
			this.Text = config.StringValue;
		}

		void OnChanged() {
			Field::OnChanged();
			config = this.Text;
		}
	}

	class ConfigNumberFormatter {
		int digits;
		string suffix;
		string prefix;
		float prescale = 1;
		ConfigNumberFormatter(int digits, string suffix) {
			this.digits = digits;
			this.suffix = suffix;
			this.prefix = "";
		}
		ConfigNumberFormatter(int digits, string suffix, string prefix) {
			this.digits = digits;
			this.suffix = suffix;
			this.prefix = prefix;
		}
		ConfigNumberFormatter(int digits, string suffix, string prefix, float prescale) {
			this.digits = digits;
			this.suffix = suffix;
			this.prefix = prefix;
			this.prescale = prescale;
		}
		private string FormatInternal(float value) {
			if (value < 0.0F)
				return "-" + Format(-value);

			value *= prescale;

			// do rounding
			float rounding = 0.5F;
			for (int i = digits; i > 0; i--)
				rounding *= 0.1F;
			value += rounding;

			int intPart = int(value);
			string s = ToString(intPart);
			if (digits > 0) {
				s += ".";
				for (int i = digits; i > 0; i--) {
					value -= float(intPart);
					value *= 10.0F;
					intPart = int(value);
					if (intPart > 9)
						intPart = 9;
					s += ToString(intPart);
				}
			}
			s += suffix;
			return s;
		}
		string Format(float value) { return prefix + FormatInternal(value); }
	}

	class ConfigViewmodelSideFormatter : ConfigNumberFormatter {
		ConfigViewmodelSideFormatter() {
			super(1, "");
		}

		string Format(float value) {
			if (value == -1)
				return _Tr("Preferences", "Left");
			else if (value == 0)
				return _Tr("Preferences", "Center");
			else if (value == 1)
				return _Tr("Preferences", "Right");
			else
				return ConfigNumberFormatter::Format(value);
		}
	}

	class ConfigFOVFormatter : ConfigNumberFormatter {
		ConfigFOVFormatter() {
			super(0, "");
		}

		string Format(float value) {
			if (value == 68)
				return _Tr("Preferences", "Default");
			else if (value >= 110)
				return _Tr("Preferences", "Quake Pro");
			else
				return ConfigNumberFormatter::Format(value);
		}
	}

	class ConfigRenderScaleFormatter : ConfigNumberFormatter {
		ConfigRenderScaleFormatter() {
			super(0, "");
		}

		string Format(float value) {
			if (value == 1)
				return _Tr("Preferences", "Bicubic");
			else if (value == 2)
				return _Tr("Preferences", "Bilinear");
			else
				return _Tr("Preferences", "Nearest Neighbour");
		}
	}

	class ConfigSensScaleFormatter : ConfigNumberFormatter {
		ConfigSensScaleFormatter() {
			super(0, "");
		}

		string Format(float value) {
			if (value == 1)
				return "Voxlap";
			else if (value == 2)
				return "BetterSpades";
			else if (value == 3)
				return "Quake/Source";
			else if (value == 4)
				return "Overwatch";
			else if (value == 5)
				return "Valorant";
			else
				return _Tr("Preferences", "Default");
		}
	}

	class ConfigHUDColorFormatter : ConfigNumberFormatter {
		ConfigHUDColorFormatter() {
			super(0, "");
		}

		string Format(float value) {
			if (value == 1)
				return _Tr("Preferences", "Team Color");
			else if (value == 2)
				return _Tr("Preferences", "Light Blue");
			else if (value == 3)
				return _Tr("Preferences", "Blue");
			else if (value == 4)
				return _Tr("Preferences", "Purple");
			else if (value == 5)
				return _Tr("Preferences", "Red");
			else if (value == 6)
				return _Tr("Preferences", "Orange");
			else if (value == 7)
				return _Tr("Preferences", "Yellow");
			else if (value == 8)
				return _Tr("Preferences", "Green");
			else if (value == 9)
				return _Tr("Preferences", "Aqua");
			else if (value == 10)
				return _Tr("Preferences", "Pink");
			else
				return _Tr("Preferences", "Custom");
		}
	}

	class ConfigTargetColorFormatter : ConfigNumberFormatter {
		ConfigTargetColorFormatter() {
			super(0, "");
		}

		string Format(float value) {
			if (value == 1)
				return _Tr("Preferences", "Red");
			else if (value == 2)
				return _Tr("Preferences", "Green");
			else if (value == 3)
				return _Tr("Preferences", "Blue");
			else if (value == 4)
				return _Tr("Preferences", "Yellow");
			else if (value == 5)
				return _Tr("Preferences", "Cyan");
			else if (value == 6)
				return _Tr("Preferences", "Pink");
			else
				return _Tr("Preferences", "Custom");
		}
	}

	class ConfigScopeTypeFormatter : ConfigNumberFormatter {
		ConfigScopeTypeFormatter() {
			super(0, "");
		}

		string Format(float value) {
			if (value == 1)
				return _Tr("Preferences", "Classic");
			else if (value == 2)
				return _Tr("Preferences", "Dot Sight");
			else if (value == 3)
				return _Tr("Preferences", "Custom");
			else
				return _Tr("Preferences", "Iron Sight");
		}
	}

	class ConfigSlider : spades::ui::Slider {
		ConfigItem@ config;
		float stepSize;
		spades::ui::Label@ label;
		ConfigNumberFormatter@ formatter;

		ConfigSlider(spades::ui::UIManager manager, string configName, float minValue,
					 float maxValue, float stepValue, ConfigNumberFormatter@ formatter) {
			super(manager);
			@config = ConfigItem(configName);
			this.MinValue = minValue;
			this.MaxValue = maxValue;
			this.Value = Clamp(config.FloatValue, minValue, maxValue);
			this.stepSize = stepValue;
			@this.formatter = formatter;

			// compute large change
			int steps = int((maxValue - minValue) / stepValue);
			steps = (steps + 9) / 10;
			this.LargeChange = float(steps) * stepValue;

			@label = spades::ui::Label(Manager);
			label.Alignment = Vector2(0.5F, 0.5F);
			AddChild(label);
			UpdateLabel();
		}

		void OnResized() {
			Slider::OnResized();
			label.Bounds = AABB2(0.0F, 0.0F, Size.x, Size.y);
		}

		void UpdateLabel() { label.Text = formatter.Format(config.FloatValue); }

		void DoRounding() {
			float v = float(this.Value - this.MinValue);
			v = floor((v / stepSize) + 0.5) * stepSize;
			v += float(this.MinValue);
			this.Value = v;
		}

		void OnChanged() {
			Slider::OnChanged();
			DoRounding();
			config = this.Value;
			UpdateLabel();
		}
	}

	uint8 ToUpper(uint8 c) {
		if (c >= uint8(0x61) and c <= uint8(0x7a)) {
			return uint8(c - 0x61 + 0x41);
		} else {
			return c;
		}
	}
	class ConfigHotKeyField : spades::ui::UIElement {
		ConfigItem@ config;
		private bool hover;
		spades::ui::EventHandler@ KeyBound;

		ConfigHotKeyField(spades::ui::UIManager manager, string configName) {
			super(manager);
			@config = ConfigItem(configName);
			IsMouseInteractive = true;
		}

		string BoundKey {
			get { return config.StringValue; }
			set { config = value; }
		}

		void KeyDown(string key) {
			if (IsFocused) {
				if (key != "Escape") {
					if (key == " ") {
						key = "Space";
					} else if (key == "BackSpace" or key == "Delete") {
						key = ""; // unbind
					}
					config = key;
					KeyBound(this);
				}
				@Manager.ActiveElement = null;
				AcceptsFocus = false;
			} else {
				UIElement::KeyDown(key);
			}
		}

		void MouseDown(spades::ui::MouseButton button, Vector2 clientPosition) {
			if (not AcceptsFocus) {
				AcceptsFocus = true;
				@Manager.ActiveElement = this;
				return;
			}
			if (IsFocused) {
				if (button == spades::ui::MouseButton::LeftMouseButton) {
					config = "LeftMouseButton";
				} else if (button == spades::ui::MouseButton::RightMouseButton) {
					config = "RightMouseButton";
				} else if (button == spades::ui::MouseButton::MiddleMouseButton) {
					config = "MiddleMouseButton";
				} else if (button == spades::ui::MouseButton::MouseButton4) {
					config = "MouseButton4";
				} else if (button == spades::ui::MouseButton::MouseButton5) {
					config = "MouseButton5";
				}
				KeyBound(this);
				@Manager.ActiveElement = null;
				AcceptsFocus = false;
			}
		}

		void MouseEnter() { hover = true; }
		void MouseLeave() { hover = false; }

		void Render() {
			// render background
			Renderer@ r = Manager.Renderer;
			Vector2 pos = ScreenPosition;
			Vector2 size = Size;

			r.ColorNP = Vector4(0.0F, 0.0F, 0.0F, IsFocused ? 0.3F : 0.1F);
			r.DrawImage(null, AABB2(pos.x, pos.y, size.x, size.y));

			if (IsFocused)
				r.ColorNP = Vector4(1.0F, 1.0F, 1.0F, 0.2F);
			else if (hover)
				r.ColorNP = Vector4(1.0F, 1.0F, 1.0F, 0.1F);
			else
				r.ColorNP = Vector4(1.0F, 1.0F, 1.0F, 0.06F);
			r.DrawOutlinedRect(pos.x, pos.y, pos.x + size.x, pos.y + size.y);

			Font@ font = this.Font;
			string text = IsFocused
				? _Tr("Preferences", "Press Key to Bind or [Escape] to Cancel...")
				: config.StringValue;

			Vector4 color = Vector4(1.0F, 1.0F, 1.0F, 1.0F);
			if (IsFocused) {
				color.w = abs(sin(Manager.Time * 2.0F));
			} else {
				AcceptsFocus = false;
			}

			if (text.length == 0) {
				text = _Tr("Preferences", "Unbound");
				color.w *= 0.3F;
			} else if (text == "LeftMouseButton") {
				text = _Tr("Preferences", "Left Mouse Button");
			} else if (text == "RightMouseButton") {
				text = _Tr("Preferences", "Right Mouse Button");
			} else if (text == "MiddleMouseButton") {
				text = _Tr("Preferences", "Middle Mouse Button");
			} else if (text == "MouseButton4") {
				text = _Tr("Preferences", "Mouse Button 4");
			} else if (text == "MouseButton5") {
				text = _Tr("Preferences", "Mouse Button 5");
			} else if (!IsFocused) {
				for (uint i = 0, len = text.length; i < len; i++)
					text[i] = ToUpper(text[i]);
				text = _Tr("Client", text);
			}

			Vector2 txtSize = font.Measure(text);
			Vector2 txtPos = pos + (size - txtSize) * 0.5F;

			font.Draw(text, txtPos, 1.0F, color);
		}
	}

	class ConfigSimpleToggleButton : spades::ui::RadioButton {
		ConfigItem@ config;
		int value;
		ConfigSimpleToggleButton(spades::ui::UIManager manager,
			string caption, string configName, int value) {
			super(manager);
			@config = ConfigItem(configName);
			this.Caption = caption;
			this.value = value;
			this.Toggle = true;
			this.Toggled = config.IntValue == value;
		}

		void OnActivated() {
			RadioButton::OnActivated();
			this.Toggled = true;
			config = value;
		}

		void Render() {
			this.Toggled = config.IntValue == value;
			RadioButton::Render();
		}
	}

	class ConfigTarget : spades::ui::UIElement {
		private ConfigItem cg_target("cg_target", "1");
		private ConfigItem cg_targetLines("cg_targetLines", "1");
		private ConfigItem cg_targetColor("cg_targetColor", "0");
		private ConfigItem cg_targetColorR("cg_targetColorR", "255");
		private ConfigItem cg_targetColorG("cg_targetColorG", "255");
		private ConfigItem cg_targetColorB("cg_targetColorB", "255");
		private ConfigItem cg_targetAlpha("cg_targetAlpha", "255");
		private ConfigItem cg_targetGap("cg_targetGap", "4");
		private ConfigItem cg_targetSizeHorizontal("cg_targetSizeHorizontal", "5");
		private ConfigItem cg_targetSizeVertical("cg_targetSizeVertical", "5");
		private ConfigItem cg_targetThickness("cg_targetThickness", "1");
		private ConfigItem cg_targetTStyle("cg_targetTStyle", "0");
		private ConfigItem cg_targetDot("cg_targetDot", "0");
		private ConfigItem cg_targetDotColorR("cg_targetDotColorR", "255");
		private ConfigItem cg_targetDotColorG("cg_targetDotColorG", "255");
		private ConfigItem cg_targetDotColorB("cg_targetDotColorB", "255");
		private ConfigItem cg_targetDotAlpha("cg_targetDotAlpha", "255");
		private ConfigItem cg_targetDotThickness("cg_targetDotThickness", "1");
		private ConfigItem cg_targetOutline("cg_targetOutline", "1");
		private ConfigItem cg_targetOutlineColorR("cg_targetOutlineColorR", "0");
		private ConfigItem cg_targetOutlineColorG("cg_targetOutlineColorG", "0");
		private ConfigItem cg_targetOutlineColorB("cg_targetOutlineColorB", "0");
		private ConfigItem cg_targetOutlineAlpha("cg_targetOutlineAlpha", "255");
		private ConfigItem cg_targetOutlineThickness("cg_targetOutlineThickness", "1");
		private ConfigItem cg_targetOutlineRoundedStyle("cg_targetOutlineRoundedStyle", "0");
		private ConfigItem cg_targetDynamic("cg_targetDynamic", "1");
		private ConfigItem cg_targetDynamicSplitDist("cg_targetDynamicSplitdist", "7");

		ConfigTarget(spades::ui::UIManager@ manager) {
			super(manager);
		}

		private void OnResetPressed(spades::ui::UIElement@ sender) {
			cg_targetLines.StringValue = cg_targetLines.DefaultValue;
			cg_targetColor.StringValue = cg_targetColor.DefaultValue;
			cg_targetColorR.StringValue = cg_targetColorR.DefaultValue;
			cg_targetColorG.StringValue = cg_targetColorG.DefaultValue;
			cg_targetColorB.StringValue = cg_targetColorB.DefaultValue;
			cg_targetAlpha.StringValue = cg_targetAlpha.DefaultValue;
			cg_targetGap.StringValue = cg_targetGap.DefaultValue;
			cg_targetSizeHorizontal.StringValue = cg_targetSizeHorizontal.DefaultValue;
			cg_targetSizeVertical.StringValue = cg_targetSizeVertical.DefaultValue;
			cg_targetThickness.StringValue = cg_targetThickness.DefaultValue;
			cg_targetTStyle.StringValue = cg_targetTStyle.DefaultValue;
			cg_targetDot.StringValue = cg_targetDot.DefaultValue;
			cg_targetDotColorR.StringValue = cg_targetDotColorR.DefaultValue;
			cg_targetDotColorG.StringValue = cg_targetDotColorG.DefaultValue;
			cg_targetDotColorB.StringValue = cg_targetDotColorB.DefaultValue;
			cg_targetDotAlpha.StringValue = cg_targetDotAlpha.DefaultValue;
			cg_targetDotThickness.StringValue = cg_targetDotThickness.DefaultValue;
			cg_targetOutline.StringValue = cg_targetOutline.DefaultValue;
			cg_targetOutlineColorR.StringValue = cg_targetOutlineColorR.DefaultValue;
			cg_targetOutlineColorG.StringValue = cg_targetOutlineColorG.DefaultValue;
			cg_targetOutlineColorB.StringValue = cg_targetOutlineColorB.DefaultValue;
			cg_targetOutlineAlpha.StringValue = cg_targetOutlineAlpha.DefaultValue;
			cg_targetOutlineThickness.StringValue = cg_targetOutlineThickness.DefaultValue;
			cg_targetOutlineRoundedStyle.StringValue = cg_targetOutlineRoundedStyle.DefaultValue;
			cg_targetDynamic.StringValue = cg_targetDynamic.DefaultValue;
			cg_targetDynamicSplitDist.StringValue = cg_targetDynamicSplitDist.DefaultValue;
		}

		private void OnRandomizePressed(spades::ui::UIElement@ sender) {
			cg_targetColor.StringValue = cg_targetColor.DefaultValue; // reset
			cg_targetColorR.IntValue = GetRandom(0, 255);
			cg_targetColorG.IntValue = GetRandom(0, 255);
			cg_targetColorB.IntValue = GetRandom(0, 255);
			cg_targetAlpha.IntValue = GetRandom(50, 255);
			cg_targetGap.IntValue = GetRandom(1, 10);
			cg_targetSizeHorizontal.IntValue = GetRandom(1, 10);
			cg_targetSizeVertical.IntValue = cg_targetSizeHorizontal.IntValue;
			cg_targetThickness.IntValue = GetRandom(1, 4);
			cg_targetDot.IntValue = GetRandom(2);
			cg_targetDotColorR.IntValue = GetRandom(0, 255);
			cg_targetDotColorG.IntValue = GetRandom(0, 255);
			cg_targetDotColorB.IntValue = GetRandom(0, 255);
			cg_targetDotAlpha.IntValue = GetRandom(50, 255);
			cg_targetDotThickness.IntValue = cg_targetThickness.IntValue;
			cg_targetOutline.IntValue = GetRandom(2);
			cg_targetOutlineColorR.IntValue = GetRandom(0, 255);
			cg_targetOutlineColorG.IntValue = GetRandom(0, 255);
			cg_targetOutlineColorB.IntValue = GetRandom(0, 255);
			cg_targetOutlineAlpha.IntValue = GetRandom(50, 255);
			cg_targetOutlineRoundedStyle.IntValue = GetRandom(2);
		}

		void Render() {
			Renderer@ r = Manager.Renderer;
			Vector2 pos = ScreenPosition;
			Vector2 size = Size;
			Vector2 center = pos + size * 0.5F;

			TargetParam param;
			int targetType = cg_target.IntValue;

			IntVector3 col;
			switch (cg_targetColor.IntValue) {
				case 1: col = IntVector3(250, 50, 50); break; // red
				case 2: col = IntVector3(50, 250, 50); break; // green
				case 3: col = IntVector3(50, 50, 250); break; // blue
				case 4: col = IntVector3(250, 250, 50); break; // yellow
				case 5: col = IntVector3(50, 250, 250); break; // cyan
				case 6: col = IntVector3(250, 50, 250); break; // pink
				default: // custom
					col.x = cg_targetColorR.IntValue;
					col.y = cg_targetColorG.IntValue;
					col.z = cg_targetColorB.IntValue;
					break;
			}

			Vector4 color = ConvertColorRGBA(col);
			color.w = Clamp(cg_targetAlpha.IntValue, 0, 255) / 255.0F;

			// draw preview background
			if (cg_target.BoolValue) {
				float luminosity = color.x + color.y + color.z;
				float opacity = 1.0F - luminosity;
				if (cg_targetOutline.BoolValue and targetType == 2)
					r.ColorNP = Vector4(0.6F, 0.6F, 0.6F, 0.9F);
				else
					r.ColorNP = Vector4(opacity, opacity, opacity, 0.6F);
			} else {
				r.ColorNP = Vector4(0.0F, 0.0F, 0.0F, 0.6F);
			}
			r.DrawImage(null, AABB2(pos.x, pos.y, size.x, size.y));

			// draw preview border
			r.ColorNP = Vector4(1.0F, 1.0F, 1.0F, 0.1F);
			r.DrawOutlinedRect(pos.x, pos.y, pos.x + size.x, pos.y + size.y);

			// draw target
			if (targetType == 1) { // draw default target
				Image@ sightImage = r.RegisterImage("Gfx/Sight.tga");
				Vector2 imgSize = Vector2(sightImage.Width, sightImage.Height);
				r.ColorNP = color;
				r.DrawImage(sightImage, center - (imgSize * 0.5F));
			} else if (targetType == 2) { // draw custom target
				param.lineColor = color;
				param.drawLines = cg_targetLines.BoolValue;
				param.useTStyle = cg_targetTStyle.BoolValue;
				param.lineGap = Clamp(cg_targetGap.FloatValue, -10.0F, 10.0F);
				param.lineLength.x = Clamp(cg_targetSizeHorizontal.FloatValue, 0.0F, 10.0F);
				param.lineLength.y = Clamp(cg_targetSizeVertical.FloatValue, 0.0F, 10.0F);
				param.lineThickness = Clamp(cg_targetThickness.FloatValue, 1.0F, 4.0F);

				param.drawDot = cg_targetDot.BoolValue;
				col.x = cg_targetDotColorR.IntValue;
				col.y = cg_targetDotColorG.IntValue;
				col.z = cg_targetDotColorB.IntValue;
				color = ConvertColorRGBA(col);
				color.w = Clamp(cg_targetDotAlpha.IntValue, 0, 255) / 255.0F;
				param.dotColor = color;
				param.dotThickness = Clamp(cg_targetDotThickness.FloatValue, 1.0F, 4.0F);

				param.drawOutline = cg_targetOutline.BoolValue;
				param.useRoundedStyle = cg_targetOutlineRoundedStyle.BoolValue;
				col.x = cg_targetOutlineColorR.IntValue;
				col.y = cg_targetOutlineColorG.IntValue;
				col.z = cg_targetOutlineColorB.IntValue;
				color = ConvertColorRGBA(col);
				color.w = Clamp(cg_targetOutlineAlpha.IntValue, 0, 255) / 255.0F;
				param.outlineColor = color;
				param.outlineThickness = Clamp(cg_targetOutlineThickness.FloatValue, 1.0F, 4.0F);

				DrawTarget(r, center, param);
			} else {
				Font@ font = this.Font;
				string text = _Tr("Preferences", "No Preview Available.");
				Vector2 txtPos = pos + (size - font.Measure(text)) * 0.5F;
				font.Draw(text, txtPos, 1.0F, Vector4(1.0F, 1.0F, 1.0F, 0.5F));
			}
		}
	}

	class ConfigScope : spades::ui::UIElement {
		private ConfigItem cg_pngScope("cg_pngScope", "0");
		private ConfigItem cg_scopeLines("cg_scopeLines", "1");
		private ConfigItem cg_scopeColor("cg_scopeColor", "0");
		private ConfigItem cg_scopeColorR("cg_scopeColorR", "255");
		private ConfigItem cg_scopeColorG("cg_scopeColorG", "0");
		private ConfigItem cg_scopeColorB("cg_scopeColorB", "255");
		private ConfigItem cg_scopeAlpha("cg_scopeAlpha", "255");
		private ConfigItem cg_scopeGap("cg_scopeGap", "4");
		private ConfigItem cg_scopeSizeHorizontal("cg_scopeSizeHorizontal", "5");
		private ConfigItem cg_scopeSizeVertical("cg_scopeSizeVertical", "5");
		private ConfigItem cg_scopeThickness("cg_scopeThickness", "1");
		private ConfigItem cg_scopeTStyle("cg_scopeTStyle", "0");
		private ConfigItem cg_scopeDot("cg_scopeDot", "0");
		private ConfigItem cg_scopeDotColorR("cg_scopeDotColorR", "0");
		private ConfigItem cg_scopeDotColorG("cg_scopeDotColorG", "0");
		private ConfigItem cg_scopeDotColorB("cg_scopeDotColorB", "0");
		private ConfigItem cg_scopeDotAlpha("cg_scopeDotAlpha", "255");
		private ConfigItem cg_scopeDotThickness("cg_scopeDotThickness", "1");
		private ConfigItem cg_scopeOutline("cg_scopeOutline", "1");
		private ConfigItem cg_scopeOutlineColorR("cg_scopeOutlineColorR", "0");
		private ConfigItem cg_scopeOutlineColorG("cg_scopeOutlineColorG", "0");
		private ConfigItem cg_scopeOutlineColorB("cg_scopeOutlineColorB", "0");
		private ConfigItem cg_scopeOutlineAlpha("cg_scopeOutlineAlpha", "255");
		private ConfigItem cg_scopeOutlineThickness("cg_scopeOutlineThickness", "1");
		private ConfigItem cg_scopeOutlineRoundedStyle("cg_scopeOutlineRoundedStyle", "0");
		private ConfigItem cg_scopeDynamic("cg_scopeDynamic", "1");
		private ConfigItem cg_scopeDynamicSplitDist("cg_scopeDynamicSplitdist", "7");

		ConfigScope(spades::ui::UIManager@ manager) {
			super(manager);
		}

		private void OnResetPressed(spades::ui::UIElement@ sender) {
			cg_scopeLines.StringValue = cg_scopeLines.DefaultValue;
			cg_scopeColor.StringValue = cg_scopeColor.DefaultValue;
			cg_scopeColorR.StringValue = cg_scopeColorR.DefaultValue;
			cg_scopeColorG.StringValue = cg_scopeColorG.DefaultValue;
			cg_scopeColorB.StringValue = cg_scopeColorB.DefaultValue;
			cg_scopeAlpha.StringValue = cg_scopeAlpha.DefaultValue;
			cg_scopeGap.StringValue = cg_scopeGap.DefaultValue;
			cg_scopeSizeHorizontal.StringValue = cg_scopeSizeHorizontal.DefaultValue;
			cg_scopeSizeVertical.StringValue = cg_scopeSizeVertical.DefaultValue;
			cg_scopeThickness.StringValue = cg_scopeThickness.DefaultValue;
			cg_scopeTStyle.StringValue = cg_scopeTStyle.DefaultValue;
			cg_scopeDot.StringValue = cg_scopeDot.DefaultValue;
			cg_scopeDotColorR.StringValue = cg_scopeDotColorR.DefaultValue;
			cg_scopeDotColorG.StringValue = cg_scopeDotColorG.DefaultValue;
			cg_scopeDotColorB.StringValue = cg_scopeDotColorB.DefaultValue;
			cg_scopeDotAlpha.StringValue = cg_scopeDotAlpha.DefaultValue;
			cg_scopeDotThickness.StringValue = cg_scopeDotThickness.DefaultValue;
			cg_scopeOutline.StringValue = cg_scopeOutline.DefaultValue;
			cg_scopeOutlineColorR.StringValue = cg_scopeOutlineColorR.DefaultValue;
			cg_scopeOutlineColorG.StringValue = cg_scopeOutlineColorG.DefaultValue;
			cg_scopeOutlineColorB.StringValue = cg_scopeOutlineColorB.DefaultValue;
			cg_scopeOutlineAlpha.StringValue = cg_scopeOutlineAlpha.DefaultValue;
			cg_scopeOutlineThickness.StringValue = cg_scopeOutlineThickness.DefaultValue;
			cg_scopeOutlineRoundedStyle.StringValue = cg_scopeOutlineRoundedStyle.DefaultValue;
			cg_scopeDynamic.StringValue = cg_scopeDynamic.DefaultValue;
			cg_scopeDynamicSplitDist.StringValue = cg_scopeDynamicSplitDist.DefaultValue;
		}

		private void OnRandomizePressed(spades::ui::UIElement@ sender) {
			cg_scopeColor.StringValue = cg_scopeColor.DefaultValue;
			cg_scopeColorR.IntValue = GetRandom(0, 255);
			cg_scopeColorG.IntValue = GetRandom(0, 255);
			cg_scopeColorB.IntValue = GetRandom(0, 255);
			cg_scopeAlpha.IntValue = GetRandom(50, 255);
			cg_scopeGap.IntValue = GetRandom(1, 10);
			cg_scopeSizeHorizontal.IntValue = GetRandom(1, 10);
			cg_scopeSizeVertical.IntValue = cg_scopeSizeHorizontal.IntValue;
			cg_scopeThickness.IntValue = GetRandom(1, 4);
			cg_scopeDot.IntValue = GetRandom(2);
			cg_scopeDotColorR.IntValue = GetRandom(0, 255);
			cg_scopeDotColorG.IntValue = GetRandom(0, 255);
			cg_scopeDotColorB.IntValue = GetRandom(0, 255);
			cg_scopeDotAlpha.IntValue = GetRandom(50, 255);
			cg_scopeDotThickness.IntValue = cg_scopeThickness.IntValue;
			cg_scopeOutline.IntValue = GetRandom(2);
			cg_scopeOutlineColorR.IntValue = GetRandom(0, 255);
			cg_scopeOutlineColorG.IntValue = GetRandom(0, 255);
			cg_scopeOutlineColorB.IntValue = GetRandom(0, 255);
			cg_scopeOutlineAlpha.IntValue = GetRandom(50, 255);
			cg_scopeOutlineRoundedStyle.IntValue = GetRandom(2);
		}

		void Render() {
			Renderer@ r = Manager.Renderer;
			Vector2 pos = ScreenPosition;
			Vector2 size = Size;
			Vector2 center = pos + size * 0.5F;

			TargetParam param;
			int scopeType = cg_pngScope.IntValue;

			IntVector3 col;
			col.x = cg_scopeColorR.IntValue;
			col.y = cg_scopeColorG.IntValue;
			col.z = cg_scopeColorB.IntValue;

			Vector4 color = ConvertColorRGBA(col);
			color.w = Clamp(cg_scopeAlpha.IntValue, 0, 255) / 255.0F;

			// draw preview background
			if (scopeType >= 2) {
				float luminosity = color.x + color.y + color.z;
				float opacity = 1.0F - luminosity;
				if (cg_scopeOutline.BoolValue and scopeType == 3)
					r.ColorNP = Vector4(0.6F, 0.6F, 0.6F, 0.9F);
				else
					r.ColorNP = Vector4(opacity, opacity, opacity, 0.6F);
			} else if (scopeType < 2) {
				r.ColorNP = Vector4(0.0F, 0.0F, 0.0F, 0.6F);
			}
			r.DrawImage(null, AABB2(pos.x, pos.y, size.x, size.y));

			// draw preview border
			r.ColorNP = Vector4(1.0F, 1.0F, 1.0F, 0.1F);
			r.DrawOutlinedRect(pos.x, pos.y, pos.x + size.x, pos.y + size.y);

			// draw target
			if (scopeType == 2) { // draw dot png scope
				Image@ dotSightImage = r.RegisterImage("Gfx/DotSight.tga");
				Vector2 imgSize = Vector2(dotSightImage.Width, dotSightImage.Height);
				r.ColorNP = color;
				r.DrawImage(dotSightImage, center - (imgSize * 0.5F));
			} else if (scopeType == 3) { // draw custom target scope
				param.lineColor = color;
				param.drawLines = cg_scopeLines.BoolValue;
				param.useTStyle = cg_scopeTStyle.BoolValue;
				param.lineGap = Clamp(cg_scopeGap.FloatValue, -10.0F, 10.0F);
				param.lineLength.x = Clamp(cg_scopeSizeHorizontal.FloatValue, 0.0F, 10.0F);
				param.lineLength.y = Clamp(cg_scopeSizeVertical.FloatValue, 0.0F, 10.0F);
				param.lineThickness = Clamp(cg_scopeThickness.FloatValue, 1.0F, 4.0F);

				param.drawDot = cg_scopeDot.BoolValue;
				col.x = cg_scopeDotColorR.IntValue;
				col.y = cg_scopeDotColorG.IntValue;
				col.z = cg_scopeDotColorB.IntValue;
				color = ConvertColorRGBA(col);
				color.w = Clamp(cg_scopeDotAlpha.IntValue, 0, 255) / 255.0F;
				param.dotColor = color;
				param.dotThickness = Clamp(cg_scopeDotThickness.FloatValue, 1.0F, 4.0F);

				param.drawOutline = cg_scopeOutline.BoolValue;
				param.useRoundedStyle = cg_scopeOutlineRoundedStyle.BoolValue;
				col.x = cg_scopeOutlineColorR.IntValue;
				col.y = cg_scopeOutlineColorG.IntValue;
				col.z = cg_scopeOutlineColorB.IntValue;
				color = ConvertColorRGBA(col);
				color.w = Clamp(cg_scopeOutlineAlpha.IntValue, 0, 255) / 255.0F;
				param.outlineColor = color;
				param.outlineThickness = Clamp(cg_scopeOutlineThickness.FloatValue, 1.0F, 4.0F);

				DrawTarget(r, center, param);
			} else {
				Font@ font = this.Font;
				string text = _Tr("Preferences", "No Preview Available.");
				Vector2 txtPos = pos + (size - font.Measure(text)) * 0.5F;
				font.Draw(text, txtPos, 1.0F, Vector4(1.0F, 1.0F, 1.0F, 0.5F));
			}
		}
	}

	class StandardPreferenceLayouterModel : spades::ui::ListViewModel {
		private spades::ui::UIElement @[] @items;
		StandardPreferenceLayouterModel(spades::ui::UIElement @[] @items) { @this.items = items; }
		int NumRows { get { return int(items.length); } }
		spades::ui::UIElement@ CreateElement(int row) { return items[row]; }
		void RecycleElement(spades::ui::UIElement@ elem) {}
	}
	class StandardPreferenceLayouter {
		spades::ui::UIElement@ Parent;
		private spades::ui::UIElement @[] items;
		private ConfigHotKeyField @[] hotkeyItems;
		private FontManager@ fontManager;

		float FieldX, FieldWidth, FieldHeight;

		StandardPreferenceLayouter(spades::ui::UIElement@ parent, FontManager@ fontManager) {
			@Parent = parent;
			@this.fontManager = fontManager;

			float sw = Parent.Manager.ScreenWidth;
			float sh = Parent.Manager.ScreenHeight;

			FieldX = sw - 440.0F;
			float maxFieldX = 250.0F;
			if (FieldX > maxFieldX)
				FieldX = maxFieldX;

			FieldWidth = sw - 400.0F;
			float maxFieldWidth = 320.0F;
			if (FieldWidth > maxFieldWidth)
				FieldWidth = maxFieldWidth;

			FieldHeight = sh - 8.0F;
			float maxFieldHeight = 550.0F;
			if (FieldHeight > maxFieldHeight)
				FieldHeight = maxFieldHeight;
		}

		private spades::ui::UIElement@ CreateItem() {
			spades::ui::UIElement elem(Parent.Manager);
			elem.Size = Vector2(FieldWidth, 32.0F);
			items.insertLast(elem);
			return elem;
		}

		private void OnKeyBound(spades::ui::UIElement@ sender) {
			// unbind already bound key
			ConfigHotKeyField@ bindField = cast<ConfigHotKeyField>(sender);
			string key = bindField.BoundKey;
			for (uint i = 0; i < hotkeyItems.length; i++) {
				ConfigHotKeyField@ f = hotkeyItems[i];
				if (f !is bindField) {
					if (f.BoundKey == key)
						f.BoundKey = "";
				}
			}
		}

		void AddHeading(string text) {
			spades::ui::UIElement@ container = CreateItem();

			spades::ui::Label label(Parent.Manager);
			label.Text = text;
			label.Alignment = Vector2(0.5F, 0.5F);
			@label.Font = fontManager.HeadingFont;
			label.Bounds = AABB2(10.0F, 0.0F, FieldX + FieldWidth - 10.0F, 32.0F);
			container.AddChild(label);
		}

		ConfigField@ AddInputField(string caption, string configName, bool enabled = true) {
			spades::ui::UIElement@ container = CreateItem();

			spades::ui::Label label(Parent.Manager);
			label.Text = caption;
			label.Alignment = Vector2(0.0F, 0.5F);
			label.Bounds = AABB2(10.0F, 0.0F, FieldX + FieldWidth - 10.0F, 32.0F);
			container.AddChild(label);

			ConfigField field(Parent.Manager, configName);
			field.Bounds = AABB2(FieldX, 1.0F, FieldWidth, 30.0F);
			field.Enable = enabled;
			container.AddChild(field);

			return field;
		}

		ConfigSlider@ AddSliderField(string caption, string configName, float minRange, float maxRange,
			float step, ConfigNumberFormatter@ formatter, bool enabled = true) {
			spades::ui::UIElement@ container = CreateItem();

			spades::ui::Label label(Parent.Manager);
			label.Text = caption;
			label.Alignment = Vector2(0.0F, 0.5F);
			label.Bounds = AABB2(10.0F, 0.0F, FieldX + FieldWidth - 10.0F, 32.0F);
			container.AddChild(label);

			ConfigSlider slider(Parent.Manager, configName, minRange, maxRange, step, formatter);
			slider.Bounds = AABB2(FieldX, 4.0F, FieldWidth, 24.0F);
			slider.Enable = enabled;
			container.AddChild(slider);

			return slider;
		}

		ConfigSlider@ AddVolumeSlider(string caption, string configName) {
			return AddSliderField(caption, configName, 0, 1, 0.01, ConfigNumberFormatter(0, "%", "", 100));
		}

		void AddSliderGroup(string caption, array<string> labels,
			float minRange, float maxRange, float step, int digits,
			array<string> prefix, bool enabled = true) {
			spades::ui::UIElement@ container = CreateItem();

			spades::ui::Label label(Parent.Manager);
			label.Text = caption;
			label.Alignment = Vector2(0.0F, 0.5F);
			label.Bounds = AABB2(10.0F, 0.0F, FieldX + FieldWidth - 10.0F, 32.0F);
			container.AddChild(label);

			float width = (FieldWidth - (labels.length - 1)) / labels.length;
			for (uint i = 0; i < labels.length; ++i) {
				ConfigSlider slider(Parent.Manager, labels[i], minRange, maxRange, step,
					ConfigNumberFormatter(digits, "", prefix[i]));
				slider.Bounds = AABB2(FieldX + float(i) * (width + 1.0F), 4.0F, width, 24.0F);
				slider.Enable = enabled;
				container.AddChild(slider);
			}
		}

		void AddRGBSlider(string caption, array<string> labels, bool enabled = true) {
			AddSliderGroup(caption, labels, 0, 255, 1, 0, array<string> = { "R: ", "G: ", "B: "});
		}

		void AddControl(string caption, string configName, bool enabled = true) {
			spades::ui::UIElement@ container = CreateItem();

			spades::ui::Label label(Parent.Manager);
			label.Text = caption;
			label.Alignment = Vector2(0.0F, 0.5F);
			label.Bounds = AABB2(10.0F, 0.0F, FieldX + FieldWidth - 10.0F, 32.0F);
			container.AddChild(label);

			ConfigHotKeyField field(Parent.Manager, configName);
			field.Bounds = AABB2(FieldX, 1.0F, FieldWidth, 30.0F);
			field.Enable = enabled;
			container.AddChild(field);

			@field.KeyBound = spades::ui::EventHandler(this.OnKeyBound);
			hotkeyItems.insertLast(field);
		}

		void AddChoiceField(string caption, string configName,
			array<string> labels, array<int> values, bool enabled = true) {
			spades::ui::UIElement@ container = CreateItem();

			spades::ui::Label label(Parent.Manager);
			label.Text = caption;
			label.Alignment = Vector2(0.0F, 0.5F);
			label.Bounds = AABB2(10.0F, 0.0F, FieldX + FieldWidth - 10.0F, 32.0F);
			container.AddChild(label);

			float width = (FieldWidth - (labels.length - 1)) / labels.length;
			for (uint i = 0; i < labels.length; ++i) {
				ConfigSimpleToggleButton field(Parent.Manager, labels[i], configName, values[i]);
				field.Bounds = AABB2(FieldX + float(i) * (width + 1.0F), 1.0F, width, 30.0F);
				field.Enable = enabled;
				container.AddChild(field);
			}
		}

		void AddToggleField(string caption, string configName, bool enabled = true) {
			AddChoiceField(caption, configName,
						   array<string> = {_Tr("Preferences", "ON"),
											_Tr("Preferences", "OFF")},
						   array<int> = {1, 0}, enabled);
		}

		void AddPlusMinusField(string caption, string configName, bool enabled = true) {
			AddChoiceField(caption, configName,
						   array<string> = {_Tr("Preferences", "ON"),
											_Tr("Preferences", "REVERSED"),
											_Tr("Preferences", "OFF")},
						   array<int> = {1, -1, 0}, enabled);
		}

		void AddTargetPreview() {
			spades::ui::UIElement@ container = CreateItem();

			ConfigTarget field(Parent.Manager);
			field.Bounds = AABB2(10.0F, 0.0F, FieldX + FieldWidth - 10.0F, 64.0F);
			container.AddChild(field);

			spades::ui::SimpleButton resetButton(Parent.Manager);
			resetButton.Caption = _Tr("Preferences", "Reset");
			resetButton.Bounds = AABB2(10.0F + 2.0F, 2.0F, 50.0F, 20.0F);
			@resetButton.Activated = spades::ui::EventHandler(field.OnResetPressed);
			container.AddChild(resetButton);

			spades::ui::SimpleButton randomizeButton(Parent.Manager);
			randomizeButton.Caption = _Tr("Preferences", "Randomize");
			randomizeButton.Bounds = AABB2(FieldX + FieldWidth - 80.0F - 2.0F, 2.0F, 80.0F, 20.0F);
			@randomizeButton.Activated = spades::ui::EventHandler(field.OnRandomizePressed);
			container.AddChild(randomizeButton);
		}

		void AddScopePreview() {
			spades::ui::UIElement@ container = CreateItem();

			ConfigScope field(Parent.Manager);
			field.Bounds = AABB2(10.0F, 0.0F, FieldX + FieldWidth - 10.0F, 64.0F);
			container.AddChild(field);

			spades::ui::SimpleButton resetButton(Parent.Manager);
			resetButton.Caption = _Tr("Preferences", "Reset");
			resetButton.Bounds = AABB2(10.0F + 2.0F, 2.0F, 50.0F, 20.0F);
			@resetButton.Activated = spades::ui::EventHandler(field.OnResetPressed);
			container.AddChild(resetButton);

			spades::ui::SimpleButton randomizeButton(Parent.Manager);
			randomizeButton.Caption = _Tr("Preferences", "Randomize");
			randomizeButton.Bounds = AABB2(FieldX + FieldWidth - 80.0F - 2.0F, 2.0F, 80.0F, 20.0F);
			@randomizeButton.Activated = spades::ui::EventHandler(field.OnRandomizePressed);
			container.AddChild(randomizeButton);
		}

		void FinishLayout() {
			spades::ui::ListView list(Parent.Manager);
			@list.Model = StandardPreferenceLayouterModel(items);
			list.RowHeight = 32.0F;
			list.Bounds = AABB2(10.0F, 0.0F, FieldX + (FieldWidth + 30.0F) - 10.0F, FieldHeight - 20.0F);
			Parent.AddChild(list);
		}
	}

	class GameOptionsPanel : spades::ui::UIElement {
		GameOptionsPanel(spades::ui::UIManager@ manager,
			PreferenceViewOptions@ options, FontManager@ fontManager) {
			super(manager);

			StandardPreferenceLayouter layouter(this, fontManager);

			layouter.AddHeading(_Tr("Preferences", "Player Information"));
			ConfigField@ nameField = layouter.AddInputField(_Tr("Preferences", "Player Name"),
				"cg_playerName", not options.GameActive);
			nameField.MaxLength = 15;
			nameField.DenyNonAscii = false;

			layouter.AddHeading(_Tr("Preferences", "Effects"));
			layouter.AddChoiceField(_Tr("Preferences", "Blood"), "cg_blood",
									array<string> = {_Tr("Preferences", "MARKS"),
													 _Tr("Preferences", "NORMAL"),
													 _Tr("Preferences", "OFF")},
									array<int> = {2, 1, 0});
			layouter.AddToggleField(_Tr("Preferences", "Ragdoll Corpses"), "cg_ragdoll");
			layouter.AddToggleField(_Tr("Preferences", "Corpse Line Collision"), "r_corpseLineCollision");
			layouter.AddToggleField(_Tr("Preferences", "Bullet Tracers"), "cg_tracers");
			layouter.AddToggleField(_Tr("Preferences", "Firstperson Tracers"), "cg_tracersFirstPerson");
			layouter.AddToggleField(_Tr("Preferences", "Eject Bullet Casings"), "cg_ejectBrass");
			layouter.AddChoiceField(_Tr("Preferences", "Animations"), "cg_animations",
									array<string> = {_Tr("Preferences", "FAST"),
													 _Tr("Preferences", "NORMAL"),
													 _Tr("Preferences", "OFF")},
									array<int> = {2, 1, 0});
			layouter.AddToggleField(_Tr("Preferences", "Hurt Screen Effects"), "cg_hurtScreenEffects");
			layouter.AddChoiceField(_Tr("Preferences", "Camera Shake"), "cg_shake",
									array<string> = {_Tr("Preferences", "MORE"),
													 _Tr("Preferences", "NORMAL"),
													 _Tr("Preferences", "OFF")},
									array<int> = {2, 1, 0});
			layouter.AddChoiceField(_Tr("Preferences", "Particles"), "cg_particles",
									array<string> = {_Tr("Preferences", "MORE"),
													 _Tr("Preferences", "NORMAL"),
													 _Tr("Preferences", "OFF")},
									array<int> = {2, 1, 0});

			layouter.AddHeading(_Tr("Preferences", "Feedbacks"));
			layouter.AddChoiceField(_Tr("Preferences", "Center Messages"), "cg_centerMessage",
									array<string> = {_Tr("Preferences", "NORMAL"),
													 _Tr("Preferences", "LESS")},
									array<int> = {2, 0});
			layouter.AddChoiceField(_Tr("Preferences", "Center Messages Size"), "cg_centerMessageSmallFont",
									array<string> = {_Tr("Preferences", "NORMAL"),
													 _Tr("Preferences", "SMALL")},
									array<int> = {0, 1}, not options.GameActive);
			layouter.AddToggleField(_Tr("Preferences", "Ignore Chat Messages"), "cg_ignoreChatMessages");
			layouter.AddToggleField(_Tr("Preferences", "Ignore Private Messages"), "cg_ignorePrivateMessages");
			layouter.AddSliderField(_Tr("Preferences",	"Master Volume"), "s_volume",
			0, 100, 1, ConfigNumberFormatter(0, "%"));
			layouter.AddVolumeSlider(_Tr("Preferences", "Death Camera Volume"), "cg_deathSoundGain");
			layouter.AddVolumeSlider(_Tr("Preferences", "Respawn Beep Volume"), "cg_respawnSoundGain");
			layouter.AddVolumeSlider(_Tr("Preferences", "Hit Feedback Volume"), "cg_hitFeedbackSoundGain");
			layouter.AddVolumeSlider(_Tr("Preferences", "Headshot Feedback Volume"), "cg_headshotFeedbackSoundGain");
			layouter.AddToggleField(_Tr("Preferences", "Environmental Audio"), "cg_environmentalAudio");
			layouter.AddVolumeSlider(_Tr("Preferences", "Chat Notify Sounds"), "cg_chatBeep");
			layouter.AddToggleField(_Tr("Preferences", "Show Alerts"), "cg_alerts");
			layouter.AddVolumeSlider(_Tr("Preferences", "Alert Sounds"), "cg_alertSounds");
			layouter.AddToggleField(_Tr("Preferences", "Hit Analyzer"), "cg_hitAnalyze");
			layouter.AddToggleField(_Tr("Preferences", "Hit Indicator"), "cg_hitIndicator");
			layouter.AddToggleField(_Tr("Preferences", "Damage Indicator"), "cg_damageIndicators");

			layouter.AddHeading(_Tr("Preferences", "AoS 0.75/0.76 Compatibility"));
			layouter.AddToggleField(_Tr("Preferences", "Allow Unicode"), "cg_unicode");
			layouter.AddToggleField(_Tr("Preferences", "Server Alert"), "cg_serverAlert");

			layouter.AddHeading(_Tr("Preferences", "Misc"));
			layouter.AddSliderField(_Tr("Preferences", "Field of View"), "cg_fov",
			45, 110, 1, ConfigFOVFormatter());
			layouter.AddToggleField(_Tr("Preferences", "Horizontal FOV"), "cg_horizontalFov");
			layouter.AddToggleField(_Tr("Preferences", "Classic Zoom"), "cg_classicZoom");
			layouter.AddToggleField(_Tr("Preferences", "Debug Hit Detection"), "cg_debugHitTest");
			layouter.AddSliderField(_Tr("Preferences", "Hit Test Debugger Size"), "cg_dbgHitTestSize",
			64, 256, 8, ConfigNumberFormatter(0, "px"));
			layouter.AddControl(_Tr("Preferences", "Toggle Hit Debugger Zoom"), "cg_keyToggleHitTestZoom");		
			layouter.AddSliderField(_Tr("Preferences", "Hit Debugger Fade Time"), "cg_dbgHitTestFadeTime",
			1, 20, 1, ConfigNumberFormatter(0, "s"));		
			layouter.AddToggleField(_Tr("Preferences", "Debug Weapon Spread"), "cg_debugAim");
			layouter.AddSliderField(_Tr("Preferences", "Viewmodel Alignment"), "cg_viewWeaponSide",
			-1, 1, 0.1, ConfigViewmodelSideFormatter());
			layouter.AddControl(_Tr("Preferences", "Switch Viewmodel Handedness"), "cg_keyToggleLeftHand");
			layouter.AddToggleField(_Tr("Preferences", "Classic Viewmodel"), "cg_classicViewWeapon");
			layouter.AddToggleField(_Tr("Preferences", "Classic Player Model"), "cg_classicPlayerModels");

			layouter.AddHeading(_Tr("Preferences", "Heads-Up Display"));
			layouter.AddToggleField(_Tr("Preferences", "Hide HUD"), "cg_hideHud");
			layouter.AddChoiceField(_Tr("Preferences", "HUD Ammo Style"), "cg_hudAmmoStyle",
									array<string> = {_Tr("Preferences", "NORMAL"),
													 _Tr("Preferences", "SIMPLE")},
									array<int> = {0, 1});
			layouter.AddChoiceField(_Tr("Preferences", "Show Alive Player Count"), "cg_hudPlayerCount",
									array<string> = {_Tr("Preferences", "OFF"),
													 _Tr("Preferences", "Top"),
													 _Tr("Preferences", "Bottom")},
									array<int> = {0, 1, 2});
			layouter.AddChoiceField(_Tr("Preferences", "Show Statistics"), "cg_stats",
									array<string> = {_Tr("Preferences", "OFF"),
													 _Tr("Preferences", "Top"),
													 _Tr("Preferences", "Bottom")},
									array<int> = {0, 2, 1});
			layouter.AddToggleField(_Tr("Preferences", "Show Player Statistics"), "cg_playerStats");
			layouter.AddToggleField(_Tr("Preferences", "Show Placed Blocks"), "cg_playerStatsShowPlacedBlocks");
			layouter.AddSliderField(_Tr("Preferences", "Player Statistics Height"), "cg_playerStatsHeight",
			0, 100, 1, ConfigNumberFormatter(0, "px"));
			layouter.AddSliderGroup(_Tr("Preferences", "HUD Edge Positions"),
			array<string> = { "cg_hudSafezoneX", "cg_hudSafezoneY"},
			0.2, 1, 0.01, 2, array<string> = { "X: ", "Y: "});
			layouter.AddSliderField(_Tr("Preferences", "HUD Color"), "cg_hudColor",
			0, 10, 1, ConfigHUDColorFormatter());
			layouter.AddRGBSlider(_Tr("Preferences", "Custom Color"),
			array<string> = { "cg_hudColorR", "cg_hudColorG", "cg_hudColorB"});
			layouter.AddSliderField(_Tr("Preferences", "Chat Height"), "cg_chatHeight",
			10, 100, 1, ConfigNumberFormatter(0, "px"));
			layouter.AddSliderField(_Tr("Preferneces", "Killfeed Height"), "cg_killfeedHeight",
			10, 100, 1, ConfigNumberFormatter(0, "px"));
			layouter.AddSliderField(_Tr("Preferences", "Chat Fade Time"), "cg_chatFadeTime",
			5, 20, 1, ConfigNumberFormatter(0, "s"));
			layouter.AddSliderField(_Tr("Preferences", "Killfeed Fade Time"), "cg_killfeedFadeTime",
			5, 20, 1, ConfigNumberFormatter(0, "s"));
			layouter.AddToggleField(_Tr("Preferences", "Killfeed Icons"), "cg_killfeedIcons");
			layouter.AddToggleField(_Tr("Preferences", "Show dominations"), "cg_killfeedStreaks");
			layouter.AddToggleField(_Tr("Preferences", "Small HUD Font"), "cg_smallFont", not options.GameActive);

			layouter.AddHeading(_Tr("Preferences", "Minimap"));
			layouter.AddSliderField(_Tr("Preferences", "Minimap Size"), "cg_minimapSize",
			128, 256, 8, ConfigNumberFormatter(0, "px"));
			layouter.AddSliderField(_Tr("Preferences", "Minimap Scale Mode"), "cg_minimapScaleMode",
			0, 3, 1, ConfigNumberFormatter(0, ""));
			layouter.AddSliderField(_Tr("Preferences", "Minimap Opacity"), "cg_minimapOpacity",
			0.1, 1, 0.1, ConfigNumberFormatter(1, ""));
			layouter.AddToggleField(_Tr("Preferences", "Use Weapon Icons"),	 "cg_minimapPlayerIcon");
			layouter.AddToggleField(_Tr("Preferences", "Use Random Colors"), "cg_minimapPlayerColor");
			layouter.AddChoiceField(_Tr("Preferences", "Show Map Location"), "cg_minimapCoords",
									array<string> = {_Tr("Preferences", "OFF"),
													 _Tr("Preferences", "Bottom"),
													 _Tr("Preferences", "Side")},
									array<int> = {0, 1, 2});
			layouter.AddToggleField(_Tr("Preferences", "Show Player Names"), "cg_minimapPlayerNames");

			layouter.AddHeading(_Tr("Preferences", "Target"));
			layouter.AddTargetPreview();
			layouter.AddHeading("");
			layouter.AddChoiceField(_Tr("Preferences", "Target Type"), "cg_target",
									array<string> = {_Tr("Preferences", "OFF"),
													 _Tr("Preferences", "Default"),
													 _Tr("Preferences", "Custom")},
									array<int> = {0, 1, 2});
			layouter.AddToggleField(_Tr("Preferences", "Lines"), "cg_targetLines");
			layouter.AddSliderField(_Tr("Preferences", "Color"), "cg_targetColor",
			0, 6, 1, ConfigTargetColorFormatter());
			layouter.AddRGBSlider(_Tr("Preferences", "Custom Color"),
			array<string> = { "cg_targetColorR", "cg_targetColorG", "cg_targetColorB"});
			layouter.AddSliderField(_Tr("Preferences", "Opacity"), "cg_targetAlpha",
			0, 255, 1, ConfigNumberFormatter(0, ""));
			layouter.AddSliderGroup(_Tr("Preferences", "Length"),
			array<string> = { "cg_targetSizeHorizontal", "cg_targetSizeVertical"},
			1, 10, 1, 0, array<string> = { "", "" });
			layouter.AddSliderField(_Tr("Preferences", "Thickness"), "cg_targetThickness",
			1, 4, 1, ConfigNumberFormatter(0, "px"));
			layouter.AddSliderField(_Tr("Preferences", "Gap"), "cg_targetGap",
			-10, 10, 1, ConfigNumberFormatter(0, "px"));
			layouter.AddToggleField(_Tr("Preferences", "Dynamic"), "cg_targetDynamic");
			layouter.AddSliderField(_Tr("Preferences", "Dynamic Split Dist"), "cg_targetDynamicSplitdist",
			1, 20, 1, ConfigNumberFormatter(0, ""));
			layouter.AddToggleField(_Tr("Preferences", "Outline"), "cg_targetOutline");
			layouter.AddRGBSlider(_Tr("Preferences", "Outline Color"),
			array<string> = { "cg_targetOutlineColorR", "cg_targetOutlineColorG", "cg_targetOutlineColorB"});
			layouter.AddSliderField(_Tr("Preferences", "Outline Opacity"), "cg_targetOutlineAlpha",
			0, 255, 1, ConfigNumberFormatter(0, ""));
			layouter.AddSliderField(_Tr("Preferences", "Outline Thickness"), "cg_targetOutlineThickness",
			1, 4, 1, ConfigNumberFormatter(0, "px"));
			layouter.AddToggleField(_Tr("Preferences", "Rounded Corners Style"), "cg_targetOutlineRoundedStyle");
			layouter.AddToggleField(_Tr("Preferences", "Center Dot"), "cg_targetDot");
			layouter.AddRGBSlider(_Tr("Preferences", "Center Dot Color"),
			array<string> = { "cg_targetDotColorR", "cg_targetDotColorG", "cg_targetDotColorB"});
			layouter.AddSliderField(_Tr("Preferences", "Center Dot Opacity"), "cg_targetDotAlpha",
			0, 255, 1, ConfigNumberFormatter(0, ""));
			layouter.AddSliderField(_Tr("Preferences", "Center Dot Thickness"), "cg_targetDotThickness",
			1, 4, 1, ConfigNumberFormatter(0, "px"));
			layouter.AddToggleField(_Tr("Preferences", "T Style"), "cg_targetTStyle");

			layouter.AddHeading(_Tr("Preferences", "Scope"));
			layouter.AddScopePreview();
			layouter.AddHeading("");
			layouter.AddSliderField(_Tr("Preferences", "Scope Type"), "cg_pngScope",
			0, 3, 1, ConfigScopeTypeFormatter());
			layouter.AddToggleField(_Tr("Preferences", "Lines"), "cg_scopeLines");
			layouter.AddRGBSlider(_Tr("Preferences", "Color"),
			array<string> = { "cg_scopeColorR", "cg_scopeColorG", "cg_scopeColorB"});
			layouter.AddSliderField(_Tr("Preferences", "Opacity"), "cg_scopeAlpha",
			0, 255, 1, ConfigNumberFormatter(0, ""));
			layouter.AddSliderGroup(_Tr("Preferences", "Length"),
			array<string> = { "cg_scopeSizeHorizontal", "cg_scopeSizeVertical"},
			1, 10, 1, 0, array<string> = { "", "" });
			layouter.AddSliderField(_Tr("Preferences", "Thickness"), "cg_scopeThickness",
			1, 4, 1, ConfigNumberFormatter(0, "px"));
			layouter.AddSliderField(_Tr("Preferences", "Gap"), "cg_scopeGap",
			-10, 10, 1, ConfigNumberFormatter(0, "px"));
			layouter.AddToggleField(_Tr("Preferences", "Dynamic"), "cg_scopeDynamic");
			layouter.AddSliderField(_Tr("Preferences", "Dynamic Split Dist"), "cg_scopeDynamicSplitdist",
			1, 20, 1, ConfigNumberFormatter(0, ""));
			layouter.AddToggleField(_Tr("Preferences", "Outline"), "cg_scopeOutline");
			layouter.AddRGBSlider(_Tr("Preferences", "Outline Color"),
			array<string> = { "cg_scopeOutlineColorR", "cg_scopeOutlineColorG", "cg_scopeOutlineColorB"});
			layouter.AddSliderField(_Tr("Preferences", "Outline Opacity"), "cg_scopeOutlineAlpha",
			0, 255, 1, ConfigNumberFormatter(0, ""));
			layouter.AddSliderField(_Tr("Preferences", "Outline Thickness"), "cg_scopeOutlineThickness",
			1, 4, 1, ConfigNumberFormatter(0, "px"));
			layouter.AddToggleField(_Tr("Preferences", "Rounded Corners Style"), "cg_scopeOutlineRoundedStyle");
			layouter.AddToggleField(_Tr("Preferences", "Center Dot"), "cg_scopeDot");
			layouter.AddRGBSlider(_Tr("Preferences", "Center Dot Color"),
			array<string> = { "cg_scopeDotColorR", "cg_scopeDotColorG", "cg_scopeDotColorB"});
			layouter.AddSliderField(_Tr("Preferences", "Center Dot Opacity"), "cg_scopeDotAlpha",
			0, 255, 1, ConfigNumberFormatter(0, ""));
			layouter.AddSliderField(_Tr("Preferences", "Center Dot Thickness"), "cg_scopeDotThickness",
			1, 4, 1, ConfigNumberFormatter(0, "px"));
			layouter.AddToggleField(_Tr("Preferences", "T Style"), "cg_scopeTStyle");

			layouter.FinishLayout();
		}
	}

	class RendererOptionsPanel : spades::ui::UIElement {
		RendererOptionsPanel(spades::ui::UIManager@ manager,
			PreferenceViewOptions@ options, FontManager@ fontManager) {
			super(manager);

			StandardPreferenceLayouter layouter(this, fontManager);

			layouter.AddHeading(_Tr("Preferences", "General"));
			layouter.AddSliderField(_Tr("Preferences", "Render Scale"), "r_scale",
			0.2, 1, 0.01, ConfigNumberFormatter(0, "%", "", 100));
			layouter.AddSliderField(_Tr("Preferences", "Render Scaling Filter"), "r_scaleFilter",
			0, 2, 1, ConfigRenderScaleFormatter());
			layouter.AddToggleField(_Tr("Preferences", "Rendering Statistics"), "r_debugTiming");
			layouter.AddToggleField(_Tr("Preferences", "Allow CPU Rendering"), "r_allowSoftwareRendering");

			layouter.AddHeading(_Tr("Preferences", "World"));
			layouter.AddToggleField(_Tr("Preferences", "Dynamic Lights"), "r_dlights");
			layouter.AddToggleField(_Tr("Preferences", "Tracers Lights"), "cg_tracerLights");
			layouter.AddToggleField(_Tr("Preferences", "Depth Prepass"), "r_depthPrepass");
			layouter.AddToggleField(_Tr("Preferences", "Occlusion Querying"), "r_occlusionQuery");

			layouter.AddHeading(_Tr("Preferences", "Post-processing"));
			layouter.AddToggleField(_Tr("Preferences", "Depth Of Field"), "r_depthOfField");
			layouter.AddToggleField(_Tr("Preferences", "Camera Blur"), "r_cameraBlur");
			layouter.AddToggleField(_Tr("Preferences", "FXAA Anti-Aliasing"), "r_fxaa");
			layouter.AddToggleField(_Tr("Preferences", "Lens Flares"), "r_lensFlare");
			layouter.AddToggleField(_Tr("Preferences", "Flares Lights"), "r_lensFlareDynamic");
			layouter.AddToggleField(_Tr("Preferences", "Color Correction"), "r_colorCorrection");
			layouter.AddSliderField(_Tr("Preferences", "Sharpening"), "r_sharpen",
			0, 1, 0.1, ConfigNumberFormatter(1, ""));
			layouter.AddSliderField(_Tr("Preferences", "Saturation"), "r_saturation",
			0, 2, 0.1, ConfigNumberFormatter(1, ""));
			layouter.AddSliderField(_Tr("Preferences", "Exposure"), "r_exposureValue",
			-18, 18, 0.1, ConfigNumberFormatter(1, "EV"));

			layouter.AddHeading(_Tr("Preferences", "Software Renderer"));
			layouter.AddSliderField(_Tr("Preferences", "Thread Count"), "r_swNumThreads",
			1, 16, 1, ConfigNumberFormatter(0, _Tr("Preferences", " Threads")));
			layouter.AddSliderField(_Tr("Preferences", "Undersampling"), "r_swUndersampling",
			0, 4, 1, ConfigNumberFormatter(0, "x"));

			layouter.FinishLayout();
		}
	}

	class ControlOptionsPanel : spades::ui::UIElement {
		ControlOptionsPanel(spades::ui::UIManager@ manager,
			PreferenceViewOptions@ options, FontManager@ fontManager) {
			super(manager);

			StandardPreferenceLayouter layouter(this, fontManager);

			layouter.AddHeading(_Tr("Preferences", "Weapons/Tools"));
			layouter.AddControl(_Tr("Preferences", "Attack"), "cg_keyAttack");
			layouter.AddControl(_Tr("Preferences", "Alt. Attack"), "cg_keyAltAttack");
			layouter.AddToggleField(_Tr("Preferences", "Hold Aim Down Sight"), "cg_holdAimDownSight");
			layouter.AddSliderField(_Tr("Preferences", "Mouse Sensitivity Type"), "cg_mouseSensScale",
			0, 5, 1, ConfigSensScaleFormatter());
			layouter.AddSliderField(_Tr("Preferences", "Mouse Sensitivity"), "cg_mouseSensitivity",
			0.1, 10, 0.1, ConfigNumberFormatter(1, ""));
			layouter.AddSliderField(_Tr("Preferences", "ADS Mouse Sens. Scale"), "cg_zoomedMouseSensScale",
			0.05, 3, 0.05, ConfigNumberFormatter(2, "x"));
			layouter.AddToggleField(_Tr("Preferences", "Mouse Acceleration"), "cg_mouseAccel");
			layouter.AddSliderField(_Tr("Preferences", "Exponential Power"), "cg_mouseExpPower",
			0.5, 1.5, 0.02, ConfigNumberFormatter(2, "", "^"));
			layouter.AddToggleField(_Tr("Preferences", "Invert Y-axis Mouse Input"), "cg_invertMouseY");
			layouter.AddControl(_Tr("Preferences", "Reload"), "cg_keyReloadWeapon");
			layouter.AddControl(_Tr("Preferences", "Equip Spade"), "cg_keyToolSpade");
			layouter.AddControl(_Tr("Preferences", "Equip Block"), "cg_keyToolBlock");
			layouter.AddControl(_Tr("Preferences", "Equip Weapon"), "cg_keyToolWeapon");
			layouter.AddControl(_Tr("Preferences", "Equip Grenade"), "cg_keyToolGrenade");
			layouter.AddControl(_Tr("Preferences", "Last Used Tool"), "cg_keyLastTool");
			layouter.AddPlusMinusField(_Tr("Preferences", "Switch Tools by Wheel"), "cg_switchToolByWheel");

			layouter.AddHeading(_Tr("Preferences", "Movement"));
			layouter.AddControl(_Tr("Preferences", "Move Forward"), "cg_keyMoveForward");
			layouter.AddControl(_Tr("Preferences", "Move Backward"), "cg_keyMoveBackward");
			layouter.AddControl(_Tr("Preferences", "Move Left"), "cg_keyMoveLeft");
			layouter.AddControl(_Tr("Preferences", "Move Right"), "cg_keyMoveRight");
			layouter.AddControl(_Tr("Preferences", "Crouch"), "cg_keyCrouch");
			layouter.AddControl(_Tr("Preferences", "Sneak"), "cg_keySneak");
			layouter.AddControl(_Tr("Preferences", "Jump"), "cg_keyJump");
			layouter.AddControl(_Tr("Preferences", "Sprint"), "cg_keySprint");

			layouter.AddHeading(_Tr("Preferences", "Color Palette"));
			layouter.AddControl(_Tr("Preferences", "Capture Color"), "cg_keyCaptureColor");
			layouter.AddControl(_Tr("Preferences", "Navigate up"), "cg_keyPaletteUp");
			layouter.AddControl(_Tr("Preferences", "Navigate down"), "cg_keyPaletteDown");
			layouter.AddControl(_Tr("Preferences", "Navigate left"), "cg_keyPaletteLeft");
			layouter.AddControl(_Tr("Preferences", "Navigate right"), "cg_keyPaletteRight");

			layouter.AddHeading(_Tr("Preferences", "Misc"));
			layouter.AddControl(_Tr("Preferences", "Scoreboard"), "cg_keyScoreboard");
			layouter.AddToggleField(_Tr("Preferences", "Hold Large Map"), "cg_holdMapZoom");
			layouter.AddControl(_Tr("Preferences", "Minimap Scale"), "cg_keyChangeMapScale");
			layouter.AddControl(_Tr("Preferences", "Toggle Map"), "cg_keyToggleMapZoom");
			layouter.AddControl(_Tr("Preferences", "Flashlight"), "cg_keyFlashlight");
			layouter.AddControl(_Tr("Preferences", "Global Chat"), "cg_keyGlobalChat");
			layouter.AddControl(_Tr("Preferences", "Team Chat"), "cg_keyTeamChat");
			layouter.AddControl(_Tr("Preferences", "Chat Zoom"), "cg_keyZoomChatLog");
			layouter.AddControl(_Tr("Preferences", "Limbo Menu"), "cg_keyLimbo");
			layouter.AddControl(_Tr("Preferences", "Save Map"), "cg_keySaveMap");
			layouter.AddControl(_Tr("Preferences", "Save Sceneshot"), "cg_keySceneshot");
			layouter.AddControl(_Tr("Preferences", "Save Screenshot"), "cg_keyScreenshot");

			layouter.FinishLayout();
		}
	}

	class MiscOptionsPanel : spades::ui::UIElement {
		spades::ui::Label@ msgLabel;
		spades::ui::Button@ enableButton;

		private ConfigItem cl_showStartupWindow("cl_showStartupWindow");

		MiscOptionsPanel(spades::ui::UIManager@ manager,
			PreferenceViewOptions@ options, FontManager@ fontManager) {
			super(manager);

			{
				spades::ui::Button e(Manager);
				e.Bounds = AABB2(40.0F, 10.0F, 400.0F, 30.0F);
				e.Caption = _Tr("Preferences", "Enable Startup Window");
				@e.Activated = spades::ui::EventHandler(this.OnEnableClicked);
				AddChild(e);
				@enableButton = e;
			}

			{
				spades::ui::Label label(Manager);
				label.Bounds = AABB2(40.0F, 50.0F, 0.0F, 0.0F);
				label.Text = "Hoge";
				AddChild(label);
				@msgLabel = label;
			}

			UpdateState();
		}

		void UpdateState() {
			bool enabled = cl_showStartupWindow.BoolValue;
			msgLabel.Text = enabled
				? _Tr("Preferences", "Quit and restart OpenSpades to access the startup window.")
				: _Tr("Preferences", "Some settings only can be changed in the startup window.");
			enableButton.Enable = not enabled;
		}

		private void OnEnableClicked(spades::ui::UIElement@) {
			cl_showStartupWindow.IntValue = 1;
			UpdateState();
		}
	}
}

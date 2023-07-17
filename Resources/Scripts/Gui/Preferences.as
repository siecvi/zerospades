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
            AddTab(RendererOptionsPanel(Manager, options, fontManager), _Tr("Preferences", "Renderer Settings"));
            AddTab(ControlOptionsPanel(Manager, options, fontManager), _Tr("Preferences", "Controls"));
            AddTab(MiscOptionsPanel(Manager, options, fontManager), _Tr("Preferences", "Misc"));

            {
                PreferenceTabButton button(Manager);
                button.Caption = _Tr("Preferences", "Back");
				button.HotKeyText = "[Esc]";
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
                return "Quake/Source";
            else if (value == 2)
                return "Overwatch";
            else if (value == 3)
                return "Valorant";
            else
                return _Tr("Preferences", "Default");
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
            DrawOutlinedRect(r, pos.x, pos.y, pos.x + size.x, pos.y + size.y);

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

            if (text == " " or text == "Space") {
                text = _Tr("Preferences", "Space");
            } else if (text.length == 0) {
                text = _Tr("Preferences", "Unbound");
                color.w *= 0.3F;
            } else if (text == "Shift") {
                text = _Tr("Preferences", "Shift");
            } else if (text == "Control") {
                text = _Tr("Preferences", "Control");
            } else if (text == "Meta") {
                text = _Tr("Preferences", "Meta");
            } else if (text == "Alt") {
                text = _Tr("Preferences", "Alt");
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
            label.Bounds = AABB2(10.0F, 0.0F, FieldX + FieldWidth, 32.0F);
            container.AddChild(label);
        }

        ConfigField@ AddInputField(string caption, string configName, bool enabled = true) {
            spades::ui::UIElement@ container = CreateItem();

            spades::ui::Label label(Parent.Manager);
            label.Text = caption;
            label.Alignment = Vector2(0.0F, 0.5F);
            label.Bounds = AABB2(10.0F, 0.0F, FieldX + FieldWidth, 24.0F);
            container.AddChild(label);

            ConfigField field(Parent.Manager, configName);
            field.Bounds = AABB2(FieldX, 1.0F, FieldWidth, 24.0F);
			field.TextOrigin.y *= 0.5F;
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
            label.Bounds = AABB2(10.0F, 0.0F, FieldX + FieldWidth, 32.0F);
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

        void AddControl(string caption, string configName, bool enabled = true) {
            spades::ui::UIElement@ container = CreateItem();

            spades::ui::Label label(Parent.Manager);
            label.Text = caption;
            label.Alignment = Vector2(0.0F, 0.5F);
            label.Bounds = AABB2(10.0F, 0.0F, FieldX + FieldWidth, 32.0F);
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
            label.Bounds = AABB2(10.0F, 0.0F, FieldX + FieldWidth, 32.0F);
            container.AddChild(label);

            for (uint i = 0; i < labels.length; ++i) {
                ConfigSimpleToggleButton field(Parent.Manager, labels[i], configName, values[i]);
                field.Bounds = AABB2(FieldX + FieldWidth / labels.length * i, 1.0F,
                                     (FieldWidth) / labels.length, 30.0F);
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

        void FinishLayout() {
            spades::ui::ListView list(Parent.Manager);
            @list.Model = StandardPreferenceLayouterModel(items);
            list.RowHeight = 32.0F;
            list.Bounds = AABB2(10.0F, 0.0F, FieldX + (FieldWidth + 20.0F), FieldHeight - 20.0F);
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
            layouter.AddToggleField(_Tr("Preferences", "Animations"), "cg_animations");
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
            layouter.AddSliderField(_Tr("Preferences", "Center Messages Scale"), "cg_centerMessageScale",
            0.75, 1, 0.01, ConfigNumberFormatter(2, ""));
            layouter.AddToggleField(_Tr("Preferences", "Ignore Chat Messages"), "cg_ignoreChatMessages");
            layouter.AddToggleField(_Tr("Preferences", "Hit Analyze Messages"), "cg_hitAnalyze");
            layouter.AddVolumeSlider(_Tr("Preferences", "Chat Notify Sounds"), "cg_chatBeep");
            layouter.AddToggleField(_Tr("Preferences", "Show Alerts"), "cg_alerts");
            layouter.AddVolumeSlider(_Tr("Preferences", "Alert Sounds"), "cg_alertSounds");
            layouter.AddToggleField(_Tr("Preferences", "Hit Indicator"), "cg_hitIndicator");
            layouter.AddToggleField(_Tr("Preferences", "Damage Indicator"), "cg_damageIndicators");

            layouter.AddHeading(_Tr("Preferences", "AoS 0.75/0.76 Compatibility"));
            layouter.AddToggleField(_Tr("Preferences", "Allow Unicode"), "cg_unicode");
            layouter.AddToggleField(_Tr("Preferences", "Server Alert"), "cg_serverAlert");

            layouter.AddHeading(_Tr("Preferences", "Misc"));
            layouter.AddSliderField(_Tr("Preferences", "Field of View"), "cg_fov",
            45, 90, 1, ConfigNumberFormatter(0, "°"));
            layouter.AddToggleField(_Tr("Preferences", "Horizontal FOV"), "cg_horizontalFov");
            layouter.AddToggleField(_Tr("Preferences", "Classic Zoom"), "cg_classicZoom");
            layouter.AddSliderField(_Tr("Preferences",  "Master Volume"), "s_volume",
            0, 100, 1, ConfigNumberFormatter(0, "%"));
            layouter.AddVolumeSlider(_Tr("Preferences", "Hitmarker Volume"), "cg_hitMarkSoundGain");
            layouter.AddVolumeSlider(_Tr("Preferences", "Hit Feedback Volume"), "cg_hitFeedbackSoundGain");
            layouter.AddToggleField(_Tr("Preferences", "Environmental Audio"), "cg_environmentalAudio");
            layouter.AddToggleField(_Tr("Preferences", "Show Statistics"), "cg_stats");
            layouter.AddToggleField(_Tr("Preferences", "Show Player Statistics"), "cg_playerStats");
            layouter.AddToggleField(_Tr("Preferences", "Debug Hit Detection"), "cg_debugHitTest");
            layouter.AddSliderField(_Tr("Preferences", "Hit Test Debugger Size"), "cg_dbgHitTestSize",
            64, 256, 8, ConfigNumberFormatter(0, "px"));
            layouter.AddControl(_Tr("Preferences", "Toggle Hit Test Zoom"), "cg_keyToggleHitTestZoom");
            layouter.AddToggleField(_Tr("Preferences", "Debug Weapon Spread"), "cg_debugAim");
            layouter.AddToggleField(_Tr("Preferences", "Classic Viewmodel"), "cg_classicViewWeapon");

            layouter.AddHeading(_Tr("Preferences", "Minimap"));
            layouter.AddSliderField(_Tr("Preferences", "Minimap Size"), "cg_minimapSize",
            128, 256, 8, ConfigNumberFormatter(0, "px"));
            layouter.AddToggleField(_Tr("Preferences", "Use Weapon Icons"),  "cg_minimapPlayerIcon");
            layouter.AddToggleField(_Tr("Preferences", "Use Random Colors"), "cg_minimapPlayerColor");

            layouter.AddHeading(_Tr("Preferences", "Heads-Up Display"));
            layouter.AddToggleField(_Tr("Preferences", "Hide HUD"), "cg_hideHud");
            layouter.AddSliderField(_Tr("Preferences", "Hud Horizontal Border"), "cg_hudBorderX",
            2, 320, 2, ConfigNumberFormatter(0, "px"));
            layouter.AddSliderField(_Tr("Preferences", "Hud Vertical Border"), "cg_hudBorderY",
            2, 240, 2, ConfigNumberFormatter(0, "px"));
            layouter.AddSliderField(_Tr("Preferences", "Chat Height"), "cg_chatHeight",
            10, 100, 1, ConfigNumberFormatter(0, "px"));
            layouter.AddSliderField(_Tr("Preferneces", "Killfeed Height"), "cg_killfeedHeight",
            10, 100, 1, ConfigNumberFormatter(0, "px"));

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
            0, 3, 1, ConfigSensScaleFormatter());
            layouter.AddSliderField(_Tr("Preferences", "Mouse Sensitivity"), "cg_mouseSensitivity",
            0.1, 10, 0.1, ConfigNumberFormatter(1, ""));
            layouter.AddSliderField(_Tr("Preferences", "ADS Mouse Sens. Scale"), "cg_zoomedMouseSensScale",
            0.05, 3, 0.05, ConfigNumberFormatter(2, "x"));
            layouter.AddToggleField(_Tr("Preferences", "Mouse Acceleration"), "cg_mouseAccel");
            layouter.AddSliderField(_Tr("Preferences", "Exponential Power"), "cg_mouseExpPower",
            0.5, 1.5, 0.02, ConfigNumberFormatter(2, "", "^"));
            layouter.AddToggleField(_Tr("Preferences", "Invert Y-axis Mouse Input"), "cg_invertMouseY");
            layouter.AddControl(_Tr("Preferences", "Reload"), "cg_keyReloadWeapon");
            layouter.AddControl(_Tr("Preferences", "Capture Color"), "cg_keyCaptureColor");
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

            layouter.AddHeading(_Tr("Preferences", "Misc"));
            layouter.AddToggleField(_Tr("Preferences", "Hold Large Map"), "cg_holdMapZoom");
            layouter.AddControl(_Tr("Preferences", "Minimap Scale"), "cg_keyChangeMapScale");
            layouter.AddControl(_Tr("Preferences", "Toggle Map"), "cg_keyToggleMapZoom");
            layouter.AddControl(_Tr("Preferences", "Flashlight"), "cg_keyFlashlight");
            layouter.AddControl(_Tr("Preferences", "Global Chat"), "cg_keyGlobalChat");
            layouter.AddControl(_Tr("Preferences", "Team Chat"), "cg_keyTeamChat");
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

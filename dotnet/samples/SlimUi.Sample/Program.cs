using SlimUi;

var demo = new SlimUiDemo();
return SlimApplication.Run("Slim UI Demo", 960, 720, demo.Build);

internal sealed class SlimUiDemo
{
    private const int IdSidebar = 80;
    private const int IdShaderImage = 94;
    private const int IdShaderPause = 95;
    private const int IdShaderSpeed = 96;
    private const int IdBtnClick = 101;
    private const int IdBtnDefault = 102;
    private const int IdBtnDisabled = 103;
    private const int IdTooltipInfo = 104;
    private const int IdTooltipButton = 105;
    private const int IdContainerTabs = 106;
    private const int IdChkA = 111;
    private const int IdChkB = 112;
    private const int IdRadio1 = 121;
    private const int IdRadio2 = 122;
    private const int IdRadio3 = 123;
    private const int IdEdit = 131;
    private const int IdHotkey = 141;
    private const int IdGamepadShortcut = 142;
    private const int IdComboShort = 151;
    private const int IdComboLong = 152;
    private const int IdSlider = 161;
    private const int IdSpinner = 171;
    private const int IdSwatch = 181;
    private const int IdComboScheme = 192;
    private const int IdComboTypo = 193;
    private const int IdLblScheme = 194;
    private const int IdLblTypo = 195;
    private const int IdProgress = 201;
    private const int IdTextView = 221;
    private const int IdNavChrome = 240;
    private const int IdScrollViewDemo = 251;
    private const int IdVStackDemo = 252;
    private const int IdHStackDemo = 253;
    private const int IdFlowDemo = 254;
    private const int IdBtnHs1 = 261;
    private const int IdBtnHs2 = 262;
    private const int IdBtnHs3 = 263;

    private const int SectionBasic = 0;
    private const int SectionChoices = 1;
    private const int SectionTextInput = 2;
    private const int SectionContainers = 3;
    private const int SectionFlex = 4;
    private const int SectionDeclContainers = 5;
    private const int SectionShader = 6;
    private const int SectionCount = 7;

    private const string TextViewSeed =
        "Multi-line text view with soft-wrap.\n\n" +
        "Try Shift+arrows to select, Ctrl+C/V/X for clipboard, Ctrl+Z to undo, " +
        "Ctrl+A to select all. Home/End/PageUp/PageDown navigate within the wrapped " +
        "layout. Right-click for the context menu.\n\n" +
        "This is editable. Add a line of your own, then resize the window to see it wrap.";

    private static readonly SlimNavItem[] NavItems =
    [
        new("Basic controls", "\uE71D", 0xFF4CC2FF),
        new("Choices", "\uE73E", 0xFF4CC2FF),
        new("Text input", "\uE932", 0xFF4CC2FF),
        new("Containers", "\uE7C4", 0xFF4CC2FF),
        new("Flex", "\uE8A9", 0xFF4CC2FF),
        new("Decl containers", "\uE8B7", 0xFF4CC2FF),
        new("Shader", "\uE790", 0xFF4CC2FF),
    ];

    private static readonly string[] SchemeNames = ["blue-dark", "light", "dark"];
    private static readonly string[] TypographyNames = ["serif", "default"];

    private int _section = SectionBasic;
    private int _containerTab;
    private string _schemeName = "light";
    private string _typographyName = "default";
    private SlimImage _logo;
    private bool _shaderPaused;
    private float _shaderSpeed = 1;

    private uint Accent => _schemeName switch
    {
        "light" => 0xFF0078D4,
        "dark" => 0xFFCC6E1F,
        _ => 0xFF4CC2FF,
    };
    private uint SidebarColor => _schemeName == "light" ? 0xFFF3F3F3 : 0xFF2A2A2A;
    private uint SecondaryText => _schemeName == "light" ? 0xFF444444 : 0xFF999999;

    public void Build(SlimUiBuilder ui)
    {
        EnsureLogo(ui.Window);

        ui.ClearCanvas();

        float w = ui.ClientWidth;
        float h = ui.ClientHeight;
        float pad = 28;
        float gap = 28;
        float navW = 208;
        float ch = 28;
        float lh = 18;
        float labelGap = 4;
        float barH = ch + lh + labelGap;
        float barY = h - barH - pad;

        ui.ContainerAt(IdSidebar, 0, 0, navW, h).StyleColor(SlimStyleColor.Background, SidebarColor);
        ui.TitleAt(22, 24, navW - 44, 32, "Slim UI");
        ui.LabelAt(22, 60, navW - 44, lh, "Control gallery");
        ui.LastStyleColor(SlimStyleColor.Text, SecondaryText);

        SlimControl nav = default;
        nav = ui.NavListAt(IdNavChrome, 12, 100, navW - 24, h - 100, 44, NavItems, _section, onChanged: () =>
        {
            int selected = nav.SelectedIndex;
            if (selected < 0 || selected >= SectionCount || selected == _section) return;
            _section = selected;
            ui.Window.RequestRebuild();
        });
        nav.FontSize(11).StyleColor(SlimStyleColor.Fill, Accent).StyleColor(SlimStyleColor.Border, SidebarColor);
        ui.SetMinSize(IdNavChrome, width: navW - 24);

        float contentX = navW + gap;
        float contentY = pad;
        float contentW = w - contentX - pad;
        float contentH = barY - 14 - contentY;
        ui.SetNavContentWheelRegion(IdNavChrome, contentX, contentY, contentW, contentH);

        switch (_section)
        {
            case SectionBasic:
                BuildSectionBasic(ui, contentX, contentY, contentW, contentH);
                break;
            case SectionChoices:
                BuildSectionChoices(ui, contentX, contentY, contentW, contentH);
                break;
            case SectionTextInput:
                BuildSectionTextInput(ui, contentX, contentY, contentW, contentH);
                break;
            case SectionContainers:
                BuildSectionContainers(ui, contentX, contentY, contentW, contentH);
                break;
            case SectionFlex:
                BuildSectionFlex(ui, contentX, contentY, contentW, contentH);
                break;
            case SectionDeclContainers:
                BuildSectionDeclContainers(ui, contentX, contentY, contentW, contentH);
                break;
            case SectionShader:
                BuildSectionShader(ui, contentX, contentY, contentW);
                break;
        }

        BuildBottomBar(ui, w, barY, ch, lh, labelGap, gap);
        SampleShader.Configure(ui.Window, IdShaderImage, IdEdit, _section == SectionShader, _shaderPaused, _shaderSpeed);
    }

    private void BuildSectionBasic(SlimUiBuilder ui, float cx, float cy, float cw, float chTotal)
    {
        SectionLayout layout = MakeLayout(cx, cy, cw);

        EmitSectionTitle(ui, ref layout, "Basic controls", "Small building blocks. Ready to make your own.");

        EmitLabel(ui, ref layout, "Label");
        ui.LabelAt(layout.X, layout.Y, layout.W, layout.ControlHeight, "Hello, Slim UI. Try a control, then change the theme.");
        AdvanceRow(ref layout, layout.ControlHeight);

        EmitLabel(ui, ref layout, "Buttons");
        float btnW = 110;
        float btnGap = 8;
        ui.ButtonAt(IdBtnClick, layout.X, layout.Y, btnW, layout.ControlHeight, "Click me");
        ui.ButtonAt(IdBtnDefault, layout.X + btnW + btnGap, layout.Y, btnW, layout.ControlHeight, "Default", isDefault: true);
        ui.ButtonAt(IdBtnDisabled, layout.X + (btnW + btnGap) * 2, layout.Y, btnW, layout.ControlHeight, "Disabled", enabled: false);
        ui.SetMinSize(IdBtnClick, width: btnW);
        ui.SetMinSize(IdBtnDefault, width: btnW);
        ui.SetMinSize(IdBtnDisabled, width: btnW);
        AdvanceRow(ref layout, layout.ControlHeight);

        EmitLabel(ui, ref layout, "Tooltips");
        ui.TooltipAt(IdTooltipInfo, layout.X, layout.Y + 5, 18, 18,
            "A little context, right where you need it. Tooltips work on their own or on any control.");
        ui.LabelAt(layout.X + 28, layout.Y, layout.W - 28, layout.ControlHeight, "Hover the ? icon or the button.");
        AdvanceRow(ref layout, layout.ControlHeight);

        ui.ButtonAt(IdTooltipButton, layout.X, layout.Y, 160, layout.ControlHeight, "Button tooltip")
            .Tooltip("Buttons can explain themselves, too. Move away to dismiss this tooltip.");
        ui.SetMinSize(IdTooltipButton, width: 160);
        AdvanceRow(ref layout, layout.ControlHeight);

        EmitLabel(ui, ref layout, "Progress bar");
        float barH = 8;
        SlimControl progress = ui.ProgressAt(IdProgress, layout.X, layout.Y, layout.W, barH, 0, 100, 40);
        AdvanceRow(ref layout, barH);

        EmitLabel(ui, ref layout, "Slider — drag to update the progress bar");
        SlimControl slider = default;
        slider = ui.SliderAt(IdSlider, layout.X, layout.Y, layout.W, layout.ControlHeight, 0, 100, 40, onChanged: () =>
        {
            progress.SetProgress(slider.Number);
        });
        AdvanceRow(ref layout, layout.ControlHeight);
    }

    private void BuildSectionShader(SlimUiBuilder ui, float cx, float cy, float cw)
    {
        SectionLayout layout = MakeLayout(cx, cy, cw);
        EmitSectionTitle(ui, ref layout, "Shader", "Live effects. The same interactive controls underneath.");
        layout.Y += layout.RowGap - layout.LabelGap;

        // Match the native demo's whole-pixel effect bounds at any DPI.
        float scale = ui.DpiScale;
        float PixelAligned(float value) => MathF.Floor(value * scale) / scale;
        float x = PixelAligned(layout.X);
        float imageW = PixelAligned(300);
        float imageH = PixelAligned(_logo.Width > 0 ? 300f * _logo.Height / _logo.Width : 140);
        ui.ImageAt(x, PixelAligned(layout.Y), imageW, imageH, _logo, IdShaderImage);
        AdvanceRow(ref layout, imageH);

        EmitLabel(ui, ref layout, "Text input");
        ui.TextBoxAt(IdEdit, x, PixelAligned(layout.Y), PixelAligned(layout.W), PixelAligned(layout.ControlHeight),
            placeholder: "Type something…");
        layout.Y += layout.ControlHeight + layout.RowGap * 1.5f;

        ui.ButtonAt(IdShaderPause, layout.X, layout.Y, 110, layout.ControlHeight, _shaderPaused ? "Resume" : "Pause", onClick: () =>
        {
            _shaderPaused = !_shaderPaused;
            ui.Window.RequestRebuild();
        });
        SlimControl speed = default;
        speed = ui.SliderAt(IdShaderSpeed, layout.X + 126, layout.Y, layout.W - 126, layout.ControlHeight,
            0, 300, _shaderSpeed * 100, onChanged: () =>
        {
            _shaderSpeed = speed.Number / 100;
            SampleShader.Configure(ui.Window, IdShaderImage, IdEdit, true, _shaderPaused, _shaderSpeed);
        });
    }

    private void BuildSectionChoices(SlimUiBuilder ui, float cx, float cy, float cw, float chTotal)
    {
        string[] shortItems = ["Red", "Green", "Blue", "Yellow"];
        string[] longItems =
        [
            "Aegean", "American Bobtail", "American Curl",
            "American Ringtail", "American Shorthair", "American Wirehair",
            "Aphrodite Giant", "Arabian Mau", "Asian cat",
            "Asian Semi-longhair", "Australian Mist", "Bambino",
            "Bengal", "Birman", "British Shorthair",
        ];

        SectionLayout layout = MakeLayout(cx, cy, cw);
        EmitSectionTitle(ui, ref layout, "Choices", "Pick one, select a few, or find your color.");

        float colGap = 24;
        float colW = (layout.W - colGap) * 0.5f;
        float colLeft = layout.X;
        float colRight = layout.X + colW + colGap;
        float yLeft = layout.Y;
        float yRight = layout.Y;

        ui.LabelAt(colLeft, yLeft, colW, layout.LabelHeight, "Checkboxes");
        yLeft += layout.LabelHeight + layout.LabelGap;
        ui.CheckboxAt(IdChkA, colLeft, yLeft, colW, layout.ControlHeight, "Option A");
        yLeft += layout.ControlHeight + 2;
        ui.CheckboxAt(IdChkB, colLeft, yLeft, colW, layout.ControlHeight, "Option B (checked)", isChecked: true);
        yLeft += layout.ControlHeight + layout.RowGap;

        ui.LabelAt(colLeft, yLeft, colW, layout.LabelHeight, "Radio group");
        yLeft += layout.LabelHeight + layout.LabelGap;
        ui.RadioAt(IdRadio1, colLeft, yLeft, colW, layout.ControlHeight, "Alpha", groupId: 1, isChecked: true);
        yLeft += layout.ControlHeight + 2;
        ui.RadioAt(IdRadio2, colLeft, yLeft, colW, layout.ControlHeight, "Beta", groupId: 1);
        yLeft += layout.ControlHeight + 2;
        ui.RadioAt(IdRadio3, colLeft, yLeft, colW, layout.ControlHeight, "Gamma", groupId: 1);
        yLeft += layout.ControlHeight + layout.RowGap;

        ui.LabelAt(colRight, yRight, colW, layout.LabelHeight, "Combobox");
        yRight += layout.LabelHeight + layout.LabelGap;
        ui.ComboBoxAt(IdComboShort, colRight, yRight, colW, layout.ControlHeight, shortItems, selected: 1);
        yRight += layout.ControlHeight + layout.RowGap;

        ui.LabelAt(colRight, yRight, colW, layout.LabelHeight, "Combobox (long — scroll)");
        yRight += layout.LabelHeight + layout.LabelGap;
        ui.ComboBoxAt(IdComboLong, colRight, yRight, colW, layout.ControlHeight, longItems, selected: 7)
            .PopupWidth(colW * 1.5f);
        yRight += layout.ControlHeight + layout.RowGap;

        ui.LabelAt(colRight, yRight, colW, layout.LabelHeight, "🎨 Color swatch");
        yRight += layout.LabelHeight + layout.LabelGap;
        ui.SwatchAt(IdSwatch, colRight, yRight, 140, layout.ControlHeight, 0xFF0078D4);
    }

    private void BuildSectionTextInput(SlimUiBuilder ui, float cx, float cy, float cw, float chTotal)
    {
        SectionLayout layout = MakeLayout(cx, cy, cw);
        layout.RowGap = 8;
        EmitSectionTitle(ui, ref layout, "Text input", "Text, numbers, and shortcuts.");

        EmitLabel(ui, ref layout, "Edit");
        ui.TextBoxAt(IdEdit, layout.X, layout.Y, layout.W, layout.ControlHeight, placeholder: "Type something…");
        AdvanceRow(ref layout, layout.ControlHeight);

        float colW = (layout.W - 12) * 0.5f;
        float rightX = layout.X + colW + 12;
        ui.LabelAt(layout.X, layout.Y, colW, layout.LabelHeight, "Keyboard shortcut");
        ui.LabelAt(rightX, layout.Y, colW, layout.LabelHeight, "Gamepad shortcut");
        layout.Y += layout.LabelHeight + layout.LabelGap;
        ui.KeyboardShortcutRecorderAt(IdHotkey, layout.X, layout.Y, colW, layout.ControlHeight, 0x71, prompt: "Press a key combination...");
        ui.GamepadShortcutRecorderAt(IdGamepadShortcut, rightX, layout.Y, colW, layout.ControlHeight, 0x4020, prompt: "Press gamepad buttons...");
        AdvanceRow(ref layout, layout.ControlHeight);

        EmitLabel(ui, ref layout, "Spinner");
        ui.SpinnerAt(IdSpinner, layout.X, layout.Y, 120, layout.ControlHeight, 0, 999, 42)
            .Tooltip("Hold an arrow to keep changing the value.");
        AdvanceRow(ref layout, layout.ControlHeight);

        EmitLabel(ui, ref layout, "Textview (multi-line, soft-wrap)");
        float tvH = Math.Max(cy + chTotal - layout.Y, 80);
        ui.TextViewAt(IdTextView, layout.X, layout.Y, layout.W, tvH, TextViewSeed, capacity: 4096);
        ui.SetMinClientSize(height: layout.Y + 80 + layout.LabelHeight + layout.LabelGap + layout.ControlHeight + 42);
    }

    private void BuildSectionContainers(SlimUiBuilder ui, float cx, float cy, float cw, float chTotal)
    {
        SectionLayout layout = MakeLayout(cx, cy, cw);
        EmitSectionTitle(ui, ref layout, "Containers", "Explore groups, tabs, and layouts.");

        float sectionBottom = cy + chTotal;
        float scrollY = layout.Y;
        float scrollH = Math.Max(sectionBottom - layout.Y, 120);
        ui.ScrollViewAt(IdScrollViewDemo, layout.X, layout.Y, layout.W, scrollH);

        float gutter = 12;
        float padLeft = 8;
        float childX = layout.X + padLeft;
        float childW = layout.W - padLeft - gutter;
        float y = layout.Y + 8;
        const float subsectionTitleH = 22;
        const float subsectionTitleGap = 8;
        const float subsectionGap = 24;

        void EmitSubsectionTitle(string text)
        {
            ui.LabelAt(childX, y, childW, subsectionTitleH, text);
            ui.LastTextStyle(13, semibold: true);
            ui.LastParent(IdScrollViewDemo);
            y += subsectionTitleH + subsectionTitleGap;
        }

        EmitSubsectionTitle("Tabs");

        string[] tabItems = ["Overview", "Details", "Output"];
        float tabsH = 118;
        float tabH = 34;
        float tabGap = 6;
        float contentGap = 8;
        SlimControl tabs = default;
        tabs = ui.TabsAt(IdContainerTabs, childX, y, childW, tabsH, tabH, tabItems,
            selected: _containerTab,
            onChanged: () =>
            {
                int selected = tabs.SelectedIndex;
                if (selected < 0 || selected >= tabItems.Length || selected == _containerTab) return;
                _containerTab = selected;
                ui.Window.RequestRebuild();
            },
            tabGap: tabGap,
            contentGap: contentGap);
        ui.LastParent(IdScrollViewDemo);

        float paneX = childX + 14;
        float paneY = y + tabH + contentGap + 12;
        float paneW = childW - 28;
        string line1 = "Snapshot";
        string line2 = "Three child controls are clipped to the selected tab pane.";
        if (_containerTab == 1)
        {
            line1 = "Container details";
            line2 = "Tab content is parented to the tab control and drawn below the strip.";
        }
        else if (_containerTab == 2)
        {
            line1 = "Output";
            line2 = "The panel uses the view background while tabs use nav highlights.";
        }

        ui.LabelAt(paneX, paneY, paneW, layout.LabelHeight, line1);
        ui.LastParent(IdContainerTabs);
        ui.LabelAt(paneX, paneY + layout.LabelHeight + 8, paneW, layout.LabelHeight, line2);
        ui.LastParent(IdContainerTabs);
        y += tabsH + subsectionGap;

        EmitSubsectionTitle("Vertical stack");

        float rowH = layout.ControlHeight + 4;
        float vstackH = rowH * 3;
        ui.ContainerAt(IdVStackDemo, childX, y, childW, vstackH);
        ui.LastParent(IdScrollViewDemo);
        string[] texts = ["First label", "Second label", "Third label"];
        float yV = y;
        foreach (string text in texts)
        {
            ui.LabelAt(childX, yV, childW, layout.ControlHeight, text);
            ui.LastParent(IdVStackDemo);
            yV += rowH;
        }

        y += vstackH + subsectionGap;

        EmitSubsectionTitle("Horizontal stack");

        float hstackBtnW = 100;
        float hstackGap = 8;
        ui.ContainerAt(IdHStackDemo, childX, y, childW, layout.ControlHeight);
        ui.LastParent(IdScrollViewDemo);
        int[] ids = [IdBtnHs1, IdBtnHs2, IdBtnHs3];
        string[] labels = ["One", "Two", "Three"];
        for (int i = 0; i < 3; i++)
        {
            ui.ButtonAt(ids[i], childX + (hstackBtnW + hstackGap) * i, y, hstackBtnW, layout.ControlHeight, labels[i]);
            ui.LastParent(IdHStackDemo);
            ui.SetMinSize(ids[i], width: hstackBtnW);
        }

        y += layout.ControlHeight + subsectionGap;

        EmitSubsectionTitle("Flow");

        float imgH = 56;
        float imgGap = 8;
        float imgW = _logo.IsValid && _logo.Height > 0 ? imgH * _logo.Width / _logo.Height : imgH;
        int perRow = Math.Max(1, (int)((childW + imgGap) / (imgW + imgGap)));
        const int count = 8;
        int rows = (count + perRow - 1) / perRow;
        float flowH = rows * imgH + (rows - 1) * imgGap;

        ui.ContainerAt(IdFlowDemo, childX, y, childW, flowH);
        ui.LastParent(IdScrollViewDemo);
        ui.SetFlow(IdFlowDemo);
        for (int i = 0; i < count; i++)
        {
            int row = i / perRow;
            int col = i % perRow;
            ui.ImageAt(childX + (imgW + imgGap) * col, y + (imgH + imgGap) * row, imgW, imgH, _logo);
            ui.LastParent(IdFlowDemo);
            ui.LastMinSize(imgW, imgH);
        }

        y += flowH + subsectionGap;
        ui.SetScrollViewContentHeight(IdScrollViewDemo, y - scrollY);
    }

    private void BuildSectionFlex(SlimUiBuilder ui, float cx, float cy, float cw, float chTotal)
    {
        SectionLayout layout = MakeLayout(cx, cy, cw);
        EmitSectionTitle(ui, ref layout, "Flex", "Resize to see controls grow and shrink.");

        if (!ui.PushRootRect(layout.X, layout.Y, layout.W, 0, layout.RowGap)) return;

        ui.Label("Horizontal grow — middle button absorbs leftover width:");
        ui.Row(3, 8, () =>
        {
            ui.Button("Left (pref=120)").Preferred(120);
            ui.Button("Middle (grow=1)").Grow();
            ui.Button("Right (pref=120)").Preferred(120);
        });

        ui.Label("Horizontal cap — three grow=1 buttons, each capped at max=180:");
        ui.Row(3, 8, () =>
        {
            ui.Button("A").Grow().Max(180);
            ui.Button("B").Grow().Max(180);
            ui.Button("C").Grow().Max(180);
        });

        ui.Label("Horizontal shrink — pref 200/400/600 overflow; each loses width in proportion to its basis:");
        ui.Row(3, 8, () =>
        {
            ui.Button("Pref 200").Preferred(200);
            ui.Button("Pref 400").Preferred(400);
            ui.Button("Pref 600").Preferred(600);
        });

        float consumed = ui.CurrentContainerConsumedHeight();
        ui.PopContainer();

        float captionY = layout.Y + consumed + layout.RowGap;
        ui.LabelAt(layout.X, captionY, layout.W, layout.LabelHeight, "Vertical grow — middle row (edit) fills remaining height:");
        float vY = captionY + layout.LabelHeight + layout.LabelGap;
        float remaining = Math.Max(0, cy + chTotal - vY);

        if (!ui.PushRootRect(layout.X, vY, layout.W, remaining, 8)) return;
        ui.Stack(() =>
        {
            ui.Label("— Header (intrinsic height) —");
            ui.TextView(
                "Textview, grow=1: absorbs all leftover vertical space.\n" +
                "Try resizing the window — height tracks the section, but\n" +
                "the minimum stays at ~3 lines (SUI_DEFAULT_TEXTVIEW_H).",
                capacity: 512,
                key: "flex-v-body").Grow();
            ui.Label("— Footer (intrinsic height) —");
        });
        ui.PopContainer();

        float vstackMinH = 2 * layout.LabelHeight + 80 + 2 * 8;
        float sectionMinH = 62 + layout.RowGap + consumed + layout.RowGap + layout.LabelHeight + layout.LabelGap + vstackMinH;
        float barReserve = 14 + layout.ControlHeight + layout.LabelHeight + layout.LabelGap + 28;
        ui.SetMinClientSize(height: cy + sectionMinH + barReserve);
    }

    private void BuildSectionDeclContainers(SlimUiBuilder ui, float cx, float cy, float cw, float chTotal)
    {
        SectionLayout layout = MakeLayout(cx, cy, cw);
        EmitSectionTitle(ui, ref layout, "Declarative containers", "Nested layouts. Scrolling and clipping.");

        float remainingY = layout.Y;
        float remainingH = Math.Max(0, cy + chTotal - remainingY);

        if (!ui.PushRootRect(layout.X, remainingY, layout.W, remainingH, 8)) return;

        ui.ScrollView(layout.W, remainingH * 0.62f, 6, () =>
        {
            ui.Label("Scrollview demo");
            ui.Label("Resize narrower for scrollbar.");
            ui.Row(3, 8, () =>
            {
                ui.Button("One");
                ui.Button("Two");
                ui.Button("Three");
            });
            for (int i = 0; i < 24; i++)
            {
                ui.Label($"Row {i + 1}");
            }
        }, key: "decl-sv");

        ui.ClipBox(layout.W, remainingH * 0.30f, 6, () =>
        {
            ui.Label("Clipbox demo");
            ui.Label("Overflow is clipped (no scrollbar).");
            for (int i = 0; i < 8; i++)
            {
                ui.Label($"Clipped row {i + 1}");
            }
        }, key: "decl-clip");

        ui.PopContainer();

        float sectionMinH = 62 + layout.RowGap + 120 + 8 + 100;
        float barReserve = 14 + layout.ControlHeight + layout.LabelHeight + layout.LabelGap + 28;
        ui.SetMinClientSize(width: cx + 340 + 12, height: cy + sectionMinH + barReserve);
    }

    private void BuildBottomBar(SlimUiBuilder ui, float windowWidth, float barY, float controlHeight, float labelHeight, float labelGap, float contentGap)
    {
        float comboW = 140;
        float gap = 8;
        float rightTotal = comboW * 2 + gap;
        float rightX = windowWidth - rightTotal - 28;
        float schemeX = rightX;
        float typoX = rightX + comboW + gap;

        int selectedScheme = Math.Max(0, Array.IndexOf(SchemeNames, _schemeName));
        int selectedTypography = Math.Max(0, Array.IndexOf(TypographyNames, _typographyName));

        ui.LabelAt(schemeX, barY, comboW, labelHeight, "Theme", IdLblScheme).StyleColor(SlimStyleColor.Text, SecondaryText);
        SlimControl schemeCombo = default;
        schemeCombo = ui.ComboBoxAt(IdComboScheme, schemeX, barY + labelHeight + labelGap, comboW, controlHeight, SchemeNames, selectedScheme, onChanged: () =>
        {
            int selected = schemeCombo.SelectedIndex;
            if (selected < 0 || selected >= SchemeNames.Length) return;
            _schemeName = SchemeNames[selected];
            ui.Window.SetScheme(_schemeName);
            ui.Window.RequestRebuild();
        });

        ui.LabelAt(typoX, barY, comboW, labelHeight, "Typography", IdLblTypo).StyleColor(SlimStyleColor.Text, SecondaryText);
        SlimControl typographyCombo = default;
        typographyCombo = ui.ComboBoxAt(IdComboTypo, typoX, barY + labelHeight + labelGap, comboW, controlHeight, TypographyNames, selectedTypography, onChanged: () =>
        {
            int selected = typographyCombo.SelectedIndex;
            if (selected < 0 || selected >= TypographyNames.Length) return;
            _typographyName = TypographyNames[selected];
            ui.Window.SetTypography(_typographyName);
            ui.Window.RequestRebuild();
        });

        const SlimAnchor anchor = SlimAnchor.Right | SlimAnchor.Bottom;
        float leadRightMargin = comboW + gap + 28 + contentGap;
        float trailRightMargin = 28;
        float labelBottomMargin = labelGap + controlHeight + 28 + 14;
        float comboBottomMargin = 28 + 14;

        ui.SetAnchor(IdLblScheme, anchor, leadRightMargin, labelBottomMargin);
        ui.SetAnchor(IdComboScheme, anchor, leadRightMargin, comboBottomMargin);
        ui.SetAnchor(IdLblTypo, anchor, trailRightMargin, labelBottomMargin);
        ui.SetAnchor(IdComboTypo, anchor, trailRightMargin, comboBottomMargin);
    }

    private void EnsureLogo(SlimWindow window)
    {
        if (_logo.IsValid) return;

        string? path = FindLogoPath();
        if (path is not null)
        {
            _logo = window.LoadImage(path);
        }
    }

    private static string? FindLogoPath()
    {
        DirectoryInfo? directory = new(AppContext.BaseDirectory);
        while (directory is not null)
        {
            string candidate = Path.Combine(directory.FullName, "logo.png");
            if (File.Exists(candidate)) return candidate;
            directory = directory.Parent;
        }

        return null;
    }

    private static SectionLayout MakeLayout(float x, float y, float width)
    {
        return new SectionLayout(x, y, width, 18, 28, 4, 14);
    }

    private static void EmitLabel(SlimUiBuilder ui, ref SectionLayout layout, string text)
    {
        ui.LabelAt(layout.X, layout.Y, layout.W, layout.LabelHeight, text);
        ui.LastTextStyle(semibold: true);
        layout.Y += layout.LabelHeight + layout.LabelGap;
    }

    private void EmitSectionTitle(SlimUiBuilder ui, ref SectionLayout layout, string text, string description)
    {
        ui.TitleAt(layout.X, layout.Y, layout.W, 36, text);
        layout.Y += 40;
        ui.LabelAt(layout.X, layout.Y, layout.W, 22, description);
        ui.LastStyleColor(SlimStyleColor.Text, SecondaryText);
        layout.Y += 22 + layout.RowGap;
    }

    private static void AdvanceRow(ref SectionLayout layout, float controlHeight)
    {
        layout.Y += controlHeight + layout.RowGap;
    }

    private struct SectionLayout(float x, float y, float width, float labelHeight, float controlHeight, float labelGap, float rowGap)
    {
        public float X = x;
        public float Y = y;
        public float W = width;
        public float LabelHeight = labelHeight;
        public float ControlHeight = controlHeight;
        public float LabelGap = labelGap;
        public float RowGap = rowGap;
    }
}

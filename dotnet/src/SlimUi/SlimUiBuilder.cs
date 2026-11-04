namespace SlimUi;

public sealed class SlimUiBuilder
{
    internal SlimUiBuilder(SlimWindow window)
    {
        Window = window;
    }

    public SlimWindow Window { get; }

    public float ClientWidth => Window.ClientWidth;

    public float ClientHeight => Window.ClientHeight;

    public float DpiScale => Window.DpiScale;

    public void ClearCanvas()
    {
        NativeMethods.sui_net_clear_page(Window.NativeHandle);
    }

    public void SetMinClientSize(float width = -1, float height = -1)
    {
        NativeMethods.sui_net_set_min_client_size(Window.NativeHandle, width, height);
    }

    public bool PushRootRect(float x, float y, float width, float height, float gap)
    {
        return NativeMethods.sui_net_push_root_rect(Window.NativeHandle, x, y, width, height, gap) != 0;
    }

    public float CurrentContainerConsumedHeight()
    {
        return NativeMethods.sui_net_current_container_consumed_height(Window.NativeHandle);
    }

    public void PopContainer()
    {
        NativeMethods.sui_net_pop_container(Window.NativeHandle);
    }

    public void LastParent(int parentId)
    {
        NativeMethods.sui_net_last_parent(Window.NativeHandle, parentId);
    }

    public void LastTextStyle(float fontSize = 0, bool semibold = false)
    {
        NativeMethods.sui_net_last_text_style(Window.NativeHandle, fontSize, semibold ? 1 : 0);
    }

    public void LastStyleColor(SlimStyleColor property, uint color)
    {
        NativeMethods.sui_net_last_style_color(Window.NativeHandle, (uint)property, color);
    }

    public void LastFontFamily(string? family)
    {
        NativeMethods.sui_net_last_font_family(Window.NativeHandle, family);
    }

    public void LastMinSize(float width = -1, float height = -1)
    {
        NativeMethods.sui_net_last_min_size(Window.NativeHandle, width, height);
    }

    public void SetParent(int id, int parentId)
    {
        NativeMethods.sui_net_set_parent(Window.NativeHandle, id, parentId);
    }

    public void SetMinSize(int id, float width = -1, float height = -1)
    {
        NativeMethods.sui_net_set_control_min_size(Window.NativeHandle, id, width, height);
    }

    public void SetDisabled(int id, bool disabled = true)
    {
        NativeMethods.sui_net_set_control_disabled(Window.NativeHandle, id, disabled ? 1 : 0);
    }

    public void SetFlow(int id, bool flow = true)
    {
        NativeMethods.sui_net_set_control_flow(Window.NativeHandle, id, flow ? 1 : 0);
    }

    public void SetAnchor(int id, SlimAnchor anchor, float rightMargin, float bottomMargin)
    {
        NativeMethods.sui_net_set_anchor(Window.NativeHandle, id, (uint)anchor, rightMargin, bottomMargin);
    }

    public void SetScrollViewContentHeight(int id, float height)
    {
        NativeMethods.sui_net_set_scrollview_content_height(Window.NativeHandle, id, height);
    }

    public void SetNavContentWheelRegion(int id, float x, float y, float width, float height, bool enabled = true)
    {
        NativeMethods.sui_net_nav_set_content_wheel_region(
            Window.NativeHandle, id, x, y, width, height, enabled ? 1 : 0);
    }

    public void Root(float padding = 16, float gap = 8, Action? content = null)
    {
        NativeMethods.sui_net_begin(Window.NativeHandle, padding, gap);
        RunContainer(content);
    }

    public void Root(SlimInset inset, float gap = 8, Action? content = null)
    {
        NativeMethods.sui_net_begin_rect(Window.NativeHandle, inset.Left, inset.Top, inset.Right, inset.Bottom, gap);
        RunContainer(content);
    }

    public void Stack(Action content)
    {
        ArgumentNullException.ThrowIfNull(content);
        NativeMethods.sui_net_vstack(Window.NativeHandle);
        RunContainer(content);
    }

    public void Group(float gap, Action content)
    {
        ArgumentNullException.ThrowIfNull(content);
        NativeMethods.sui_net_vgroup(Window.NativeHandle, gap);
        RunContainer(content);
    }

    public void Row(int columns, float gap, Action content)
    {
        if (columns <= 0) throw new ArgumentOutOfRangeException(nameof(columns));
        ArgumentNullException.ThrowIfNull(content);
        NativeMethods.sui_net_hstack(Window.NativeHandle, columns, gap);
        RunContainer(content);
    }

    public void Flow(float itemHeight, float gap, Action content)
    {
        ArgumentNullException.ThrowIfNull(content);
        NativeMethods.sui_net_flow(Window.NativeHandle, itemHeight, gap);
        RunContainer(content);
    }

    public void RadioGroup(string? key, Action content)
    {
        ArgumentNullException.ThrowIfNull(content);
        NativeMethods.sui_net_radio_group(Window.NativeHandle, Window.NextId(key));
        RunContainer(content);
    }

    public void ScrollView(float width, float height, float gap, Action content, string? key = null)
    {
        ArgumentNullException.ThrowIfNull(content);
        NativeMethods.sui_net_scrollview(Window.NativeHandle, Window.NextId(key), width, height, gap);
        RunContainer(content);
    }

    public void ClipBox(float width, float height, float gap, Action content, string? key = null)
    {
        ArgumentNullException.ThrowIfNull(content);
        NativeMethods.sui_net_clipbox(Window.NativeHandle, Window.NextId(key), width, height, gap);
        RunContainer(content);
    }

    public SlimControl Title(string text)
    {
        NativeMethods.sui_net_title(Window.NativeHandle, text);
        return new SlimControl(Window, -1);
    }

    public SlimControl Label(string text)
    {
        NativeMethods.sui_net_label(Window.NativeHandle, text);
        return new SlimControl(Window, -1);
    }

    public SlimControl Tooltip(string text, string? key = null)
    {
        ArgumentNullException.ThrowIfNull(text);
        int id = Window.NextId(key);
        NativeMethods.sui_net_tooltip(Window.NativeHandle, id, text);
        return new SlimControl(Window, id);
    }

    public SlimControl Button(string text, Action? onClick = null, bool isDefault = false, bool enabled = true, string? key = null)
    {
        int id = Window.NextId(key);
        NativeMethods.sui_net_button(Window.NativeHandle, id, text, isDefault ? 1 : 0, enabled ? 1 : 0);
        Window.RegisterClick(id, onClick);
        return new SlimControl(Window, id);
    }

    public SlimControl Checkbox(string text, bool isChecked = false, Action? onChanged = null, string? key = null)
    {
        int id = Window.NextId(key);
        NativeMethods.sui_net_checkbox(Window.NativeHandle, id, text, isChecked ? 1 : 0);
        Window.RegisterChange(id, onChanged);
        return new SlimControl(Window, id);
    }

    public SlimControl Radio(string text, bool isChecked = false, Action? onChanged = null, string? key = null)
    {
        int id = Window.NextId(key);
        NativeMethods.sui_net_radio(Window.NativeHandle, id, text, isChecked ? 1 : 0);
        Window.RegisterChange(id, onChanged);
        return new SlimControl(Window, id);
    }

    public SlimControl TextBox(string text = "", string placeholder = "", bool numericOnly = false, string? key = null)
    {
        int id = Window.NextId(key);
        NativeMethods.sui_net_edit(Window.NativeHandle, id, text, placeholder, numericOnly ? 1 : 0);
        return new SlimControl(Window, id);
    }

    public SlimControl ComboBox(IReadOnlyList<string> items, int selected = 0, Action? onChanged = null, string? key = null)
    {
        ArgumentNullException.ThrowIfNull(items);
        int id = Window.NextId(key);
        NativeMethods.ComboBox(Window.NativeHandle, id, items, selected);
        Window.RegisterChange(id, onChanged);
        return new SlimControl(Window, id);
    }

    public SlimControl Slider(float min, float max, float value, float step = 1, Action? onChanged = null, string? key = null)
    {
        int id = Window.NextId(key);
        NativeMethods.sui_net_slider(Window.NativeHandle, id, min, max, value, step);
        Window.RegisterChange(id, onChanged);
        return new SlimControl(Window, id);
    }

    public SlimControl Progress(float min, float max, float value, string? key = null)
    {
        int id = Window.NextId(key);
        NativeMethods.sui_net_progress(Window.NativeHandle, id, min, max, value);
        return new SlimControl(Window, id);
    }

    public SlimControl Spinner(float min, float max, float value, float step = 1, Action? onChanged = null, string? key = null)
    {
        int id = Window.NextId(key);
        NativeMethods.sui_net_spinner(Window.NativeHandle, id, min, max, value, step);
        Window.RegisterChange(id, onChanged);
        return new SlimControl(Window, id);
    }

    public SlimControl TextView(string text, int capacity = 4096, bool readOnly = false, string? key = null)
    {
        int id = Window.NextId(key);
        NativeMethods.sui_net_textview(Window.NativeHandle, id, text, capacity, readOnly ? 1 : 0);
        return new SlimControl(Window, id);
    }

    public SlimControl NavList(IReadOnlyList<string> items, int selected = 0, float width = 180, float height = 320, float itemHeight = 44, Action? onChanged = null, string? key = null)
    {
        ArgumentNullException.ThrowIfNull(items);
        int id = Window.NextId(key);
        NativeMethods.sui_net_nav_list(Window.NativeHandle, id, width, height, itemHeight, selected);
        foreach (string item in items)
        {
            NativeMethods.sui_net_nav_add_item(Window.NativeHandle, id, item, "", 0xFF4CC2FF);
        }
        Window.RegisterChange(id, onChanged);
        return new SlimControl(Window, id);
    }

    public SlimControl LabelAt(float x, float y, float width, float height, string text, int id = -1)
    {
        NativeMethods.sui_net_label_at(Window.NativeHandle, id, x, y, width, height, text);
        return new SlimControl(Window, id);
    }

    public SlimControl TitleAt(float x, float y, float width, float height, string text, int id = -1)
    {
        NativeMethods.sui_net_title_at(Window.NativeHandle, id, x, y, width, height, text);
        return new SlimControl(Window, id);
    }

    public SlimControl TooltipAt(int id, float x, float y, float width, float height, string text)
    {
        ArgumentNullException.ThrowIfNull(text);
        NativeMethods.sui_net_tooltip_at(Window.NativeHandle, id, x, y, width, height, text);
        return new SlimControl(Window, id);
    }

    public SlimControl ButtonAt(int id, float x, float y, float width, float height, string text, Action? onClick = null, bool isDefault = false, bool enabled = true)
    {
        NativeMethods.sui_net_button_at(Window.NativeHandle, id, x, y, width, height, text, isDefault ? 1 : 0, enabled ? 1 : 0);
        Window.RegisterClick(id, onClick);
        return new SlimControl(Window, id);
    }

    public SlimControl CheckboxAt(int id, float x, float y, float width, float height, string text, bool isChecked = false, Action? onChanged = null)
    {
        NativeMethods.sui_net_checkbox_at(Window.NativeHandle, id, x, y, width, height, text, isChecked ? 1 : 0);
        Window.RegisterChange(id, onChanged);
        return new SlimControl(Window, id);
    }

    public SlimControl RadioAt(int id, float x, float y, float width, float height, string text, int groupId, bool isChecked = false, Action? onChanged = null)
    {
        NativeMethods.sui_net_radio_at(Window.NativeHandle, id, x, y, width, height, text, groupId, isChecked ? 1 : 0);
        Window.RegisterChange(id, onChanged);
        return new SlimControl(Window, id);
    }

    public SlimControl TextBoxAt(int id, float x, float y, float width, float height, string text = "", string placeholder = "", bool numericOnly = false)
    {
        NativeMethods.sui_net_edit_at(Window.NativeHandle, id, x, y, width, height, text, placeholder, numericOnly ? 1 : 0);
        return new SlimControl(Window, id);
    }

    public SlimControl ShortcutRecorderAt(
        int id,
        float x,
        float y,
        float width,
        float height,
        SlimShortcutKind kind,
        uint virtualKey = 0,
        uint modifiers = 0,
        uint scanCode = 0,
        bool extended = false,
        uint mouseButton = 0,
        uint gamepadButtons = 0,
        string? prompt = null,
        Action? onChanged = null)
    {
        NativeMethods.sui_net_shortcut_recorder_at(Window.NativeHandle, id, x, y, width, height,
            (int)kind, virtualKey, modifiers, scanCode, extended ? 1 : 0, mouseButton, gamepadButtons, prompt);
        Window.RegisterChange(id, onChanged);
        return new SlimControl(Window, id);
    }

    public SlimControl Tabs(IReadOnlyList<string> items, int selected = 0, float height = 180, float tabHeight = 34, Action? onChanged = null, string? key = null, float tabGap = 4, float contentGap = 6)
    {
        ArgumentNullException.ThrowIfNull(items);
        int id = Window.NextId(key);
        NativeMethods.sui_net_tabs(Window.NativeHandle, id, height, tabHeight, selected);
        NativeMethods.sui_net_tabs_set_spacing(Window.NativeHandle, id, tabGap, contentGap);
        foreach (string item in items)
        {
            NativeMethods.sui_net_tabs_add_item(Window.NativeHandle, id, item);
        }

        Window.RegisterChange(id, onChanged);
        return new SlimControl(Window, id);
    }

    public SlimControl KeyboardShortcutRecorderAt(int id, float x, float y, float width, float height, uint virtualKey, uint modifiers = 0, string? prompt = null, Action? onChanged = null)
    {
        return ShortcutRecorderAt(id, x, y, width, height, SlimShortcutKind.Keyboard,
            virtualKey: virtualKey, modifiers: modifiers, prompt: prompt, onChanged: onChanged);
    }

    public SlimControl GamepadShortcutRecorderAt(int id, float x, float y, float width, float height, uint gamepadButtons, string? prompt = null, Action? onChanged = null)
    {
        return ShortcutRecorderAt(id, x, y, width, height, SlimShortcutKind.Gamepad,
            gamepadButtons: gamepadButtons, prompt: prompt, onChanged: onChanged);
    }

    public SlimControl ComboBoxAt(int id, float x, float y, float width, float height, IReadOnlyList<string> items, int selected = 0, Action? onChanged = null)
    {
        ArgumentNullException.ThrowIfNull(items);
        NativeMethods.ComboBoxAt(Window.NativeHandle, id, x, y, width, height, items, selected);
        Window.RegisterChange(id, onChanged);
        return new SlimControl(Window, id);
    }

    public SlimControl SliderAt(int id, float x, float y, float width, float height, float min, float max, float value, float step = 1, Action? onChanged = null)
    {
        NativeMethods.sui_net_slider_at(Window.NativeHandle, id, x, y, width, height, min, max, value, step);
        Window.RegisterChange(id, onChanged);
        return new SlimControl(Window, id);
    }

    public SlimControl ProgressAt(int id, float x, float y, float width, float height, float min, float max, float value)
    {
        NativeMethods.sui_net_progress_at(Window.NativeHandle, id, x, y, width, height, min, max, value);
        return new SlimControl(Window, id);
    }

    public SlimControl SpinnerAt(int id, float x, float y, float width, float height, float min, float max, float value, float step = 1, Action? onChanged = null)
    {
        NativeMethods.sui_net_spinner_at(Window.NativeHandle, id, x, y, width, height, min, max, value, step);
        Window.RegisterChange(id, onChanged);
        return new SlimControl(Window, id);
    }

    public SlimControl TextViewAt(int id, float x, float y, float width, float height, string text, int capacity = 4096, bool readOnly = false)
    {
        NativeMethods.sui_net_textview_at(Window.NativeHandle, id, x, y, width, height, text, capacity, readOnly ? 1 : 0);
        return new SlimControl(Window, id);
    }

    public SlimControl SwatchAt(int id, float x, float y, float width, float height, uint color)
    {
        NativeMethods.sui_net_swatch_at(Window.NativeHandle, id, x, y, width, height, color);
        return new SlimControl(Window, id);
    }

    public SlimControl ContainerAt(int id, float x, float y, float width, float height)
    {
        NativeMethods.sui_net_container_at(Window.NativeHandle, id, x, y, width, height);
        return new SlimControl(Window, id);
    }

    public SlimControl ScrollViewAt(int id, float x, float y, float width, float height, float contentHeight = 0)
    {
        NativeMethods.sui_net_scrollview_at(Window.NativeHandle, id, x, y, width, height, contentHeight);
        return new SlimControl(Window, id);
    }

    public SlimControl ImageAt(float x, float y, float width, float height, SlimImage image, int id = -1, bool circleMask = false)
    {
        NativeMethods.sui_net_image_at(Window.NativeHandle, id, x, y, width, height, image.Id, circleMask ? 1 : 0);
        return new SlimControl(Window, id);
    }

    public SlimControl NavListAt(int id, float x, float y, float width, float height, float itemHeight, IReadOnlyList<SlimNavItem> items, int selected = 0, Action? onChanged = null)
    {
        ArgumentNullException.ThrowIfNull(items);
        NativeMethods.sui_net_nav_list_at(Window.NativeHandle, id, x, y, width, height, itemHeight, selected);
        foreach (SlimNavItem item in items)
        {
            NativeMethods.sui_net_nav_add_item(Window.NativeHandle, id, item.Text, item.Icon, item.IconColor);
        }

        Window.RegisterChange(id, onChanged);
        return new SlimControl(Window, id);
    }

    public SlimControl TabsAt(int id, float x, float y, float width, float height, float tabHeight, IReadOnlyList<string> items, int selected = 0, Action? onChanged = null, float tabGap = 4, float contentGap = 6)
    {
        ArgumentNullException.ThrowIfNull(items);
        NativeMethods.sui_net_tabs_at(Window.NativeHandle, id, x, y, width, height, tabHeight, selected);
        NativeMethods.sui_net_tabs_set_spacing(Window.NativeHandle, id, tabGap, contentGap);
        foreach (string item in items)
        {
            NativeMethods.sui_net_tabs_add_item(Window.NativeHandle, id, item);
        }

        Window.RegisterChange(id, onChanged);
        return new SlimControl(Window, id);
    }

    private void RunContainer(Action? content)
    {
        try
        {
            content?.Invoke();
        }
        finally
        {
            NativeMethods.sui_net_end(Window.NativeHandle);
        }
    }
}

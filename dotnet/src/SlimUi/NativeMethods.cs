using System.Runtime.InteropServices;
using System.Text;

namespace SlimUi;

internal static class NativeMethods
{
    private const string Dll = "SlimUi.Native";

    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    internal delegate void BuildCallback(IntPtr window, IntPtr userData);

    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    internal delegate void EventCallback(IntPtr window, int controlId, int eventKind, IntPtr userData);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Unicode)]
    internal static extern IntPtr sui_net_window_create(
        string title,
        int width,
        int height,
        BuildCallback buildCallback,
        EventCallback eventCallback,
        IntPtr userData);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern void sui_net_window_destroy(IntPtr window);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern void sui_net_window_show(IntPtr window);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern void sui_net_window_close(IntPtr window);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern void sui_net_window_rebuild(IntPtr window);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern int sui_net_app_run();

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern float sui_net_client_width(IntPtr window);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern float sui_net_client_height(IntPtr window);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern float sui_net_dpi_scale(IntPtr window);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern void sui_net_clear_page(IntPtr window);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Ansi)]
    internal static extern int sui_net_set_scheme(IntPtr window, string name);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Ansi)]
    internal static extern int sui_net_set_typography(IntPtr window, string name);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Unicode)]
    internal static extern int sui_net_load_image(IntPtr window, string path);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern int sui_net_image_width(IntPtr window, int imageId);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern int sui_net_image_height(IntPtr window, int imageId);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern int sui_net_push_root_rect(IntPtr window, float x, float y, float width, float height, float gap);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern float sui_net_current_container_consumed_height(IntPtr window);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern void sui_net_pop_container(IntPtr window);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern void sui_net_set_min_client_size(IntPtr window, float width, float height);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern void sui_net_set_control_min_size(IntPtr window, int id, float width, float height);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern void sui_net_last_min_size(IntPtr window, float width, float height);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern void sui_net_set_control_disabled(IntPtr window, int id, int disabled);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern void sui_net_set_parent(IntPtr window, int id, int parentId);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern void sui_net_last_parent(IntPtr window, int parentId);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern void sui_net_last_text_style(IntPtr window, float fontSize, int bold);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern void sui_net_set_text_style(IntPtr window, int id, float fontSize, int bold);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern void sui_net_set_style_color(IntPtr window, int id, uint property, uint color);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern void sui_net_last_style_color(IntPtr window, uint property, uint color);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Unicode)]
    internal static extern void sui_net_set_font_family(IntPtr window, int id, string? family);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Unicode)]
    internal static extern void sui_net_last_font_family(IntPtr window, string? family);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern void sui_net_set_control_flow(IntPtr window, int id, int flow);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern void sui_net_set_anchor(IntPtr window, int id, uint flags, float rightMargin, float bottomMargin);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern void sui_net_set_scrollview_content_height(IntPtr window, int id, float height);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern void sui_net_begin(IntPtr window, float insetDip, float gapDip);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern void sui_net_begin_rect(IntPtr window, float leftDip, float topDip, float rightDip, float bottomDip, float gapDip);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern void sui_net_end(IntPtr window);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern void sui_net_vstack(IntPtr window);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern void sui_net_vgroup(IntPtr window, float gapDip);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern void sui_net_hstack(IntPtr window, int children, float gapDip);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern void sui_net_flow(IntPtr window, float itemHeightDip, float gapDip);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern void sui_net_radio_group(IntPtr window, int groupId);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern void sui_net_scrollview(IntPtr window, int id, float widthDip, float heightDip, float gapDip);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern void sui_net_clipbox(IntPtr window, int id, float widthDip, float heightDip, float gapDip);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Unicode)]
    internal static extern int sui_net_label(IntPtr window, string text);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Unicode)]
    internal static extern int sui_net_title(IntPtr window, string text);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Unicode)]
    internal static extern int sui_net_tooltip(IntPtr window, int id, string text);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Unicode)]
    internal static extern int sui_net_button(IntPtr window, int id, string text, int isDefault, int enabled);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Unicode)]
    internal static extern int sui_net_checkbox(IntPtr window, int id, string text, int isChecked);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Unicode)]
    internal static extern int sui_net_radio(IntPtr window, int id, string text, int isChecked);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Unicode)]
    internal static extern int sui_net_edit(IntPtr window, int id, string text, string placeholder, int numericOnly);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    private static extern int sui_net_combobox(IntPtr window, int id, IntPtr items, int itemCount, int selected);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern int sui_net_slider(IntPtr window, int id, float min, float max, float value, float step);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern int sui_net_progress(IntPtr window, int id, float min, float max, float value);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern int sui_net_spinner(IntPtr window, int id, float min, float max, float value, float step);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Unicode)]
    internal static extern int sui_net_textview(IntPtr window, int id, string text, int capacityChars, int readOnly);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern int sui_net_nav_list(IntPtr window, int id, float widthDip, float heightDip, float itemHeightDip, int selected);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern int sui_net_tabs(IntPtr window, int id, float heightDip, float tabHeightDip, int selected);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Unicode)]
    internal static extern void sui_net_nav_add_item(IntPtr window, int id, string text, string icon, uint iconColor);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Unicode)]
    internal static extern void sui_net_tabs_add_item(IntPtr window, int id, string text);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern void sui_net_tabs_set_spacing(IntPtr window, int id, float tabGapDip, float contentGapDip);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Unicode)]
    internal static extern int sui_net_label_at(IntPtr window, int id, float x, float y, float width, float height, string text);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Unicode)]
    internal static extern int sui_net_title_at(IntPtr window, int id, float x, float y, float width, float height, string text);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Unicode)]
    internal static extern int sui_net_tooltip_at(IntPtr window, int id, float x, float y, float width, float height, string text);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Unicode)]
    internal static extern int sui_net_button_at(IntPtr window, int id, float x, float y, float width, float height, string text, int isDefault, int enabled);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Unicode)]
    internal static extern int sui_net_checkbox_at(IntPtr window, int id, float x, float y, float width, float height, string text, int isChecked);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Unicode)]
    internal static extern int sui_net_radio_at(IntPtr window, int id, float x, float y, float width, float height, string text, int groupId, int isChecked);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Unicode)]
    internal static extern int sui_net_edit_at(IntPtr window, int id, float x, float y, float width, float height, string text, string placeholder, int numericOnly);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Unicode)]
    internal static extern int sui_net_shortcut_recorder_at(IntPtr window, int id, float x, float y, float width, float height, int kind, uint virtualKey, uint modifiers, uint scanCode, int extended, uint mouseButton, uint gamepadButtons, string? prompt);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    private static extern int sui_net_combobox_at(IntPtr window, int id, float x, float y, float width, float height, IntPtr items, int itemCount, int selected);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern int sui_net_slider_at(IntPtr window, int id, float x, float y, float width, float height, float min, float max, float value, float step);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern int sui_net_progress_at(IntPtr window, int id, float x, float y, float width, float height, float min, float max, float value);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern int sui_net_spinner_at(IntPtr window, int id, float x, float y, float width, float height, float min, float max, float value, float step);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Unicode)]
    internal static extern int sui_net_textview_at(IntPtr window, int id, float x, float y, float width, float height, string text, int capacityChars, int readOnly);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern int sui_net_swatch_at(IntPtr window, int id, float x, float y, float width, float height, uint color);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern int sui_net_container_at(IntPtr window, int id, float x, float y, float width, float height);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern int sui_net_scrollview_at(IntPtr window, int id, float x, float y, float width, float height, float contentHeight);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern int sui_net_image_at(IntPtr window, int id, float x, float y, float width, float height, int imageId, int circleMask);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern int sui_net_nav_list_at(IntPtr window, int id, float x, float y, float width, float height, float itemHeight, int selected);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern void sui_net_nav_set_content_wheel_region(IntPtr window, int id, float x, float y, float width, float height, int enabled);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern int sui_net_tabs_at(IntPtr window, int id, float x, float y, float width, float height, float tabHeight, int selected);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern void sui_net_last_grow(IntPtr window, int grow);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern void sui_net_last_shrink(IntPtr window, int shrink);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern void sui_net_last_pref(IntPtr window, float widthDip, float heightDip);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern void sui_net_last_max(IntPtr window, float widthDip, float heightDip);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern int sui_net_get_checked(IntPtr window, int id);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern int sui_net_get_selected(IntPtr window, int id);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern float sui_net_get_number(IntPtr window, int id);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Unicode)]
    internal static extern int sui_net_get_text(IntPtr window, int id, StringBuilder buffer, int bufferChars);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern void sui_net_set_progress(IntPtr window, int id, float value);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
    internal static extern void sui_net_set_combobox_popup_width(IntPtr window, int id, float widthDip);

    [DllImport(Dll, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Unicode)]
    internal static extern void sui_net_set_tooltip(IntPtr window, int id, string? text);

    internal static int ComboBox(IntPtr window, int id, IReadOnlyList<string> items, int selected)
    {
        return ComboBoxCore(items, itemPtrs => sui_net_combobox(window, id, itemPtrs, items.Count, selected));
    }

    internal static int ComboBoxAt(IntPtr window, int id, float x, float y, float width, float height, IReadOnlyList<string> items, int selected)
    {
        return ComboBoxCore(items, itemPtrs => sui_net_combobox_at(window, id, x, y, width, height, itemPtrs, items.Count, selected));
    }

    private static int ComboBoxCore(IReadOnlyList<string> items, Func<IntPtr, int> add)
    {
        var itemPtrs = new IntPtr[items.Count];
        IntPtr arrayPtr = IntPtr.Zero;

        try
        {
            for (int i = 0; i < items.Count; i++)
            {
                itemPtrs[i] = Marshal.StringToHGlobalUni(items[i]);
            }

            arrayPtr = Marshal.AllocHGlobal(IntPtr.Size * itemPtrs.Length);
            Marshal.Copy(itemPtrs, 0, arrayPtr, itemPtrs.Length);
            return add(arrayPtr);
        }
        finally
        {
            if (arrayPtr != IntPtr.Zero)
            {
                Marshal.FreeHGlobal(arrayPtr);
            }

            foreach (IntPtr itemPtr in itemPtrs)
            {
                if (itemPtr != IntPtr.Zero)
                {
                    Marshal.FreeHGlobal(itemPtr);
                }
            }
        }
    }
}

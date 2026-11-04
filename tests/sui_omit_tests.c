/* The toolkit with every optional control left out, as a small installer builds it: what stays still draws and takes input. */
#define SUI_FEATURE_CUSTOM_CONTROLS
#define SUI_NO_COMBOBOX
#define SUI_NO_TABS
#define SUI_NO_LIST
#define SUI_NO_NAV_LIST
#define SUI_NO_SPINNER
#define SUI_NO_SLIDER
#define SUI_NO_COLOR_SWATCH
#define SUI_NO_IMAGE
#define SUI_NO_SHORTCUT_RECORDER
#define SUI_NO_TOOLTIP
#define SUI_NO_SCROLLVIEW
#define SUI_IMPLEMENTATION
#include <slim_ui/slim_ui.h>
#include "utest.h"

UTEST_STATE();

int main(int argc, const char* const argv[]) { return utest_main(argc, argv); }

static void count_click(int id, void* data) {
    (void)id;
    ++*(int*)data;
}

UTEST(omit, the_controls_that_stay_draw_and_take_input) {
    static SuiControlHost host;
    SuiRenderer renderer;
    int clicks = 0;
    HWND hwnd = CreateWindowExW(0, L"STATIC", L"", WS_POPUP, 0, 0, 320, 200, NULL, NULL, GetModuleHandleW(NULL), NULL);
    ASSERT_TRUE(hwnd != NULL);
    sui_renderer_init_fields(&renderer);
    ASSERT_TRUE(sui_renderer_init(&renderer, hwnd));
    sui_ctrl_host_init(&host, hwnd, &renderer, &sui_scheme_light, 96);
    host.on_click = count_click;
    host.on_click_data = &clicks;
    ASSERT_EQ(0, sui_ctrl_add_button(&host, 1, 10, 10, 100, 30, L"Next", 1));
    ASSERT_EQ(1, sui_ctrl_add_checkbox(&host, 2, 10, 50, 100, 24, L"Option", 0));
    ASSERT_EQ(2, sui_ctrl_add_edit(&host, 3, 10, 80, 200, 30, L"C:\\Program Files", NULL, 0));
    ASSERT_EQ(3, sui_ctrl_add_textview(&host, 4, 10, 120, 200, 40, L"one\ntwo\nthree\nfour\nfive\nsix\nseven\neight", 64, 1));

    sui_ctrl_on_mouse_move(&host, 20, 20);
    sui_ctrl_on_lbutton_down(&host, 20, 20);
    sui_ctrl_on_lbutton_up(&host, 20, 20);
    ASSERT_EQ(1, clicks);
    sui_ctrl_on_keydown(&host, VK_TAB);

    sui_renderer_begin(&renderer);
    sui_renderer_clear(&renderer, sui_scheme_light.background);
    sui_ctrl_draw_all(&host);
    sui_renderer_end(&renderer);

    sui_ctrl_on_mouse_wheel(&host, 40, 130, -120); /* a text view scrolls without the scroll view control */
    ASSERT_GT(host.controls[3].d.textview.scroll_offset, 0.0f);

    sui_ctrl_host_release(&host);
    sui_renderer_release(&renderer);
    DestroyWindow(hwnd);
}

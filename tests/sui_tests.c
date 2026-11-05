/* sui_tests.c — slim-ui baseline regression tests.
 *
 * Built and run via build-tests.ps1 in this directory.
 * Framework: utest.h (Unlicense, sheredom/utest.h).
 *
 * Scope: pin down behavior we want to preserve while later phases
 * rewrite UIA, redesign sui_begin, and harden the safety bugs. Tests that
 * encode a Phase 1+ outcome are written but wrapped in UTEST_SKIP so they
 * surface as expected work without failing the suite today.
 */

#include <stdint.h>
#include <stdlib.h>

static size_t test_sui_allocations;
static void* test_sui_malloc(size_t size) {
    ++test_sui_allocations;
    return malloc(size);
}
#define SUI_MALLOC(n) test_sui_malloc(n)
#define SUI_FREE(p) free(p)

static uint32_t test_shortcut_gamepad_buttons;
#define SUI_SHORTCUT_GAMEPAD_BUTTONS_PROVIDER() test_shortcut_gamepad_buttons
#define SUI_FEATURE_CUSTOM_CONTROLS
#define SUI_FEATURE_DECL
#define SUI_IMPLEMENTATION
#include <slim_ui/slim_ui.h>

#include <string.h>
#include <stdint.h>
#include <wchar.h>

#include "utest.h"
#include "win32_test_support.h"

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

UTEST_STATE();

int main(int argc, const char* const argv[]) {
    return utest_main(argc, argv);
}

/* =========================================================================
 * Pure-math tests — no HWND, no renderer, no host required.
 * ========================================================================= */

/* ---- SuiLayout (sui_layout.h) ----------------------------------------- */

UTEST(layout, col_starts_at_y_and_advances_by_size_plus_gap) {
    SuiLayout L = sui_layout_col(10.0f, 20.0f, 100.0f, 200.0f, 5.0f);
    float p1 = sui_layout_next(&L, 30.0f);
    float p2 = sui_layout_next(&L, 40.0f);
    ASSERT_NEAR(20.0f, p1, 0.0001f);
    ASSERT_NEAR(55.0f, p2, 0.0001f); /* 20 + 30 + 5 */
    ASSERT_NEAR(80.0f, L.used, 0.0001f); /* 30 + 5 + 40 + 5 */
}

UTEST(layout, row_starts_at_x_and_advances) {
    SuiLayout L = sui_layout_row(10.0f, 20.0f, 100.0f, 30.0f, 4.0f);
    float p1 = sui_layout_next(&L, 25.0f);
    float p2 = sui_layout_next(&L, 35.0f);
    ASSERT_NEAR(10.0f, p1, 0.0001f);
    ASSERT_NEAR(39.0f, p2, 0.0001f); /* 10 + 25 + 4 */
}

UTEST(layout, fill_returns_remaining_height_for_col) {
    SuiLayout L = sui_layout_col(0.0f, 0.0f, 100.0f, 200.0f, 0.0f);
    sui_layout_next(&L, 50.0f);
    sui_layout_next(&L, 30.0f);
    ASSERT_NEAR(120.0f, sui_layout_fill(&L), 0.0001f);
}

UTEST(layout, fill_returns_zero_when_overflowed) {
    SuiLayout L = sui_layout_col(0.0f, 0.0f, 100.0f, 50.0f, 0.0f);
    sui_layout_next(&L, 80.0f);
    ASSERT_NEAR(0.0f, sui_layout_fill(&L), 0.0001f);
}

/* ---- sui_color_lerp --------------------------------------------------- */

UTEST(color, lerp_t_zero_returns_a) {
    ASSERT_EQ((uint32_t)0xff0000u, sui_color_lerp(0xff0000u, 0x00ff00u, 0.0f));
}

UTEST(color, lerp_t_one_returns_b) {
    /* Phase 6 fix: endpoint clamp (t >= 1.0f returns b) eliminates the +0.5f
     * bias that previously caused e.g. 0xff→0x00 at t=1 to give 0x01. */
    ASSERT_EQ((uint32_t)0x00ff00u, sui_color_lerp(0xff0000u, 0x00ff00u, 1.0f));
    ASSERT_EQ((uint32_t)0x000000u, sui_color_lerp(0xff0000u, 0x000000u, 1.0f));
    ASSERT_EQ((uint32_t)0xffffffu, sui_color_lerp(0x000000u, 0xffffffu, 1.0f));
}

UTEST(color, lerp_t_half_is_midpoint) {
    /* (0xff,0,0) and (0,0xff,0) → (0x80,0x80,0) with banker-style +0.5f rounding. */
    ASSERT_EQ((uint32_t)0x808000u, sui_color_lerp(0xff0000u, 0x00ff00u, 0.5f));
}

UTEST(color, lerp_same_value_invariant) {
    /* Lerping a value with itself should give the same value regardless of t. */
    ASSERT_EQ((uint32_t)0x123456u, sui_color_lerp(0x123456u, 0x123456u, 0.3f));
    ASSERT_EQ((uint32_t)0x123456u, sui_color_lerp(0x123456u, 0x123456u, 0.7f));
}

/* ---- tabs content transition math ------------------------------------- */

UTEST(tabs, content_offsets_follow_selected_direction) {
    SuiControl c;
    memset(&c, 0, sizeof(c));
    c.type = SUI_CTRL_TABS;
    c.w = 200.0f;
    c.h = 120.0f;
    c.d.tabs.tab_height = 20.0f;
    c.d.tabs.content_anim_active = 1;
    c.d.tabs.content_anim_dir = 1;
    c.d.tabs.content_anim_from = 2;
    c.d.tabs.content_anim_to = 1;
    c.d.tabs.content_anim_t = 0.25f;

    ASSERT_TRUE(sui_ctrl_tabs_transition_includes(&c, 2));
    ASSERT_TRUE(sui_ctrl_tabs_transition_includes(&c, 1));
    ASSERT_FALSE(sui_ctrl_tabs_transition_includes(&c, 0));
    {
        float p = sui_ctrl_tabs_content_progress(&c);
        ASSERT_NEAR(-36.0f * p, sui_ctrl_tabs_content_offset_for(&c, 2), 0.0001f);
        ASSERT_NEAR(36.0f * (1.0f - p), sui_ctrl_tabs_content_offset_for(&c, 1), 0.0001f);
    }
}

UTEST(tabs, content_tick_clears_transition_pair) {
    SuiControl c;
    memset(&c, 0, sizeof(c));
    c.type = SUI_CTRL_TABS;
    c.d.tabs.content_anim_active = 1;
    c.d.tabs.content_anim_dir = -1;
    c.d.tabs.content_anim_from = 0;
    c.d.tabs.content_anim_to = 1;
    c.d.tabs.content_anim_t = 0.9f;

    ASSERT_TRUE(sui_ctrl_tabs_content_tick(&c, 0.2f));
    ASSERT_EQ(0, c.d.tabs.content_anim_active);
    ASSERT_EQ(0, c.d.tabs.content_anim_dir);
    ASSERT_EQ(-1, c.d.tabs.content_anim_from);
    ASSERT_EQ(-1, c.d.tabs.content_anim_to);
    ASSERT_NEAR(1.0f, c.d.tabs.content_anim_t, 0.0001f);
}

/* ---- combobox popup placement / animation ---------------------------- */

UTEST(combobox, popup_is_separate_and_prefers_below_when_it_fits) {
    RECT bounds = { 0, 0, 1440, 900 };
    LONG x = 0, y = 0;
    int h = 0, opens_above = -1;

    sui_combo_popup_place_in_bounds(
        100, 200, 220, 32, 220, 256, 32, 6,
        &bounds, &x, &y, &h, &opens_above);

    ASSERT_EQ(100, (int)x);
    ASSERT_EQ(238, (int)y); /* control bottom (232) + 6px gap */
    ASSERT_EQ(256, h);
    ASSERT_EQ(0, opens_above);
}

UTEST(combobox, wider_popup_clamps_to_work_area_without_changing_anchor_control) {
    RECT bounds = { 0, 0, 500, 400 };
    LONG x = 0, y = 0;
    int h = 0, opens_above = -1;

    sui_combo_popup_place_in_bounds(
        400, 100, 80, 32, 160, 96, 32, 6,
        &bounds, &x, &y, &h, &opens_above);

    ASSERT_EQ(340, (int)x); /* wider popup aligns its right edge at the work-area edge */
    ASSERT_EQ(138, (int)y);
    ASSERT_EQ(96, h);
    ASSERT_EQ(0, opens_above);
}

UTEST(combobox, popup_flips_above_before_crossing_work_area_bottom) {
    RECT bounds = { 0, 0, 1440, 900 };
    LONG x = 0, y = 0;
    int h = 0, opens_above = 0;

    sui_combo_popup_place_in_bounds(
        100, 700, 220, 32, 220, 256, 32, 6,
        &bounds, &x, &y, &h, &opens_above);

    ASSERT_EQ(100, (int)x);
    ASSERT_EQ(438, (int)y); /* control top (700) - gap (6) - height (256) */
    ASSERT_EQ(256, h);
    ASSERT_EQ(1, opens_above);
}

UTEST(combobox, constrained_popup_keeps_whole_rows_and_scrolls) {
    RECT bounds = { 0, 0, 500, 400 };
    LONG x = 0, y = 0;
    int h = 0, opens_above = 0;

    sui_combo_popup_place_in_bounds(
        430, 190, 120, 32, 120, 256, 32, 6,
        &bounds, &x, &y, &h, &opens_above);

    ASSERT_EQ(380, (int)x); /* right edge clamped to the work area */
    ASSERT_EQ(24, (int)y);
    ASSERT_EQ(160, h);      /* five complete rows fit above */
    ASSERT_EQ(1, opens_above);
}

UTEST(combobox, popup_fades_and_lifts_toward_its_resting_position) {
    SuiComboPopup popup;
    memset(&popup, 0, sizeof(popup));
    popup.anchor_y = 238;
    popup.motion_offset = 5.0f;
    popup.full_window_h = 256;

    popup.anim_t = 0.0f;
    ASSERT_EQ(0, sui_combo_popup_alpha(&popup));
    ASSERT_EQ(233, sui_combo_popup_window_y(&popup));
    ASSERT_NEAR(256.0f, sui_combo_popup_visible_height(&popup), 0.0001f);
    ASSERT_NEAR(0.0f, sui_combo_popup_visible_top(&popup, 256.0f), 0.0001f);

    popup.anim_t = 1.0f;
    ASSERT_EQ(255, sui_combo_popup_alpha(&popup));
    ASSERT_EQ(238, sui_combo_popup_window_y(&popup));

    popup.opens_above = 1;
    popup.anim_t = 0.0f;
    ASSERT_EQ(243, sui_combo_popup_window_y(&popup));
}

UTEST(combobox, popup_shadow_d2d_layers_reach_peak_alpha) {
    const int layer_count = 11;
    float composited_alpha = 0.0f;
    float first_opacity;
    float last_opacity;
    int i;

    first_opacity = sui_combo_popup_shadow_layer_opacity(0, layer_count);
    last_opacity = sui_combo_popup_shadow_layer_opacity(
        layer_count - 1, layer_count);
    for (i = 0; i < layer_count; i++) {
        float layer_opacity = sui_combo_popup_shadow_layer_opacity(
            i, layer_count);
        composited_alpha += (1.0f - composited_alpha) * layer_opacity;
    }

    ASSERT_TRUE(first_opacity > 0.0f);
    ASSERT_TRUE(last_opacity > first_opacity);
    ASSERT_NEAR((float)SUI_COMBO_SHADOW_PEAK_ALPHA / 255.0f,
                composited_alpha, 0.0001f);
}

UTEST(tooltip, popup_shadow_d2d_layers_reach_peak_alpha) {
    const int layer_count = 9;
    float composited_alpha = 0.0f;
    float first_opacity;
    float last_opacity;
    int i;

    first_opacity = sui__tooltip_shadow_layer_opacity(0, layer_count);
    last_opacity = sui__tooltip_shadow_layer_opacity(
        layer_count - 1, layer_count);
    for (i = 0; i < layer_count; i++) {
        float layer_opacity = sui__tooltip_shadow_layer_opacity(
            i, layer_count);
        composited_alpha += (1.0f - composited_alpha) * layer_opacity;
    }

    ASSERT_TRUE(first_opacity > 0.0f);
    ASSERT_TRUE(last_opacity > first_opacity);
    ASSERT_NEAR((float)SUI_TOOLTIP_SHADOW_PEAK_ALPHA / 255.0f,
                composited_alpha, 0.0001f);
}

UTEST(combobox, selected_item_stays_highlighted_until_real_hover) {
    ASSERT_EQ(4, sui_combo_popup_highlighted_item(4, -1, 8));
    ASSERT_EQ(6, sui_combo_popup_highlighted_item(4, 6, 8));
    ASSERT_EQ(4, sui_combo_popup_highlighted_item(4, 9, 8));
    ASSERT_EQ(-1, sui_combo_popup_highlighted_item(9, -1, 8));
}

UTEST(combobox, open_state_maps_to_pressed_until_popup_teardown) {
    SuiControl ctrl;
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.type = SUI_CTRL_COMBOBOX;

    sui_ctrl_combo_set_open_state(&ctrl, 1);
    ASSERT_EQ(1, ctrl.d.combo.open);
    ASSERT_TRUE((ctrl.state & SUI_CTRL_PRESSED) != 0);

    sui_ctrl_combo_set_open_state(&ctrl, 0);
    ASSERT_EQ(0, ctrl.d.combo.open);
    ASSERT_FALSE((ctrl.state & SUI_CTRL_PRESSED) != 0);
}

UTEST(combobox, arrow_keeps_direction_and_bounces_down_then_up) {
    SuiComboPopup popup;
    memset(&popup, 0, sizeof(popup));

    popup.anim_phase = SUI_COMBO_PHASE_OPEN;
    popup.anim_t = 0.5f;
    ASSERT_NEAR(SUI_COMBO_ARROW_OPEN_TRAVEL,
                sui_combo_popup_arrow_bounce_offset(&popup, 1.0f), 0.0001f);

    popup.anim_phase = SUI_COMBO_PHASE_CLOSE;
    popup.anim_t = 0.5f;
    ASSERT_NEAR(-SUI_COMBO_ARROW_CLOSE_TRAVEL,
                sui_combo_popup_arrow_bounce_offset(&popup, 1.0f), 0.0001f);

    popup.anim_phase = SUI_COMBO_PHASE_NONE;
    ASSERT_NEAR(0.0f, sui_combo_popup_arrow_bounce_offset(&popup, 1.0f), 0.0001f);
}

UTEST(combobox, scroll_indicator_is_visible_from_first_opening_frame) {
    SuiComboPopup popup;
    memset(&popup, 0, sizeof(popup));

    popup.anim_phase = SUI_COMBO_PHASE_OPEN;
    popup.anim_t = 0.0f;
    ASSERT_NEAR(0.72f, sui_combo_popup_scrollbar_opacity(&popup), 0.0001f);

    popup.anim_phase = SUI_COMBO_PHASE_NONE;
    popup.sb_reveal = 0.4f;
    ASSERT_NEAR(0.636f, sui_combo_popup_scrollbar_opacity(&popup), 0.0001f);

    popup.mouse_inside = 1;
    ASSERT_NEAR(0.72f, sui_combo_popup_scrollbar_opacity(&popup), 0.0001f);

    popup.dragging = 1;
    ASSERT_NEAR(0.82f, sui_combo_popup_scrollbar_opacity(&popup), 0.0001f);
}

UTEST(combobox, initial_scroll_keeps_viewport_edges_on_whole_rows) {
    float offset = sui_combo_popup_initial_scroll(20, 4, 42.0f, 336.0f);

    ASSERT_NEAR(42.0f, offset, 0.0001f);
    ASSERT_NEAR(0.0f, offset - (float)(int)(offset / 42.0f) * 42.0f, 0.0001f);
}

/* ---- sui__streq / sui__strncpy_z ------------------------------------- */

UTEST(streq, equal_strings_returns_true) {
    ASSERT_TRUE(sui__streq("foo", "foo"));
    ASSERT_TRUE(sui__streq("", ""));
}

UTEST(streq, different_strings_returns_false) {
    ASSERT_FALSE(sui__streq("foo", "bar"));
    ASSERT_FALSE(sui__streq("foo", "foobar"));
    ASSERT_FALSE(sui__streq("foobar", "foo"));
}

UTEST(streq, null_pointer_returns_false) {
    /* Both nulls and one null both yield 0 per current implementation. */
    ASSERT_FALSE(sui__streq(NULL, "foo"));
    ASSERT_FALSE(sui__streq("foo", NULL));
    ASSERT_FALSE(sui__streq(NULL, NULL));
}

UTEST(strncpy_z, basic_copy_terminates) {
    char dst[8];
    memset(dst, 0xAA, sizeof(dst));
    sui__strncpy_z(dst, "hello", 8);
    ASSERT_STREQ("hello", dst);
}

UTEST(strncpy_z, truncates_to_cap_minus_one) {
    char dst[4];
    memset(dst, 0xAA, sizeof(dst));
    sui__strncpy_z(dst, "hello", 4);
    ASSERT_STREQ("hel", dst);
    ASSERT_EQ('\0', dst[3]);
}

UTEST(strncpy_z, zero_cap_does_not_write) {
    char dst[4];
    dst[0] = 'x'; dst[1] = 'y'; dst[2] = 'z'; dst[3] = 'w';
    sui__strncpy_z(dst, "hello", 0);
    ASSERT_EQ('x', dst[0]);
}

UTEST(strncpy_z, empty_src_terminates) {
    char dst[8];
    memset(dst, 0xAA, sizeof(dst));
    sui__strncpy_z(dst, "", 8);
    ASSERT_EQ('\0', dst[0]);
}

UTEST(strncpy_z, null_src_treated_as_empty) {
    char dst[8];
    memset(dst, 0xAA, sizeof(dst));
    sui__strncpy_z(dst, NULL, 8);
    ASSERT_EQ('\0', dst[0]);
}

/* ---- Scheme registry -------------------------------------------------- */

UTEST(scheme, builtin_light_and_dark_present_after_lazy_init) {
    /* sui_scheme_find lazily registers light/dark on first call. */
    ASSERT_TRUE(sui_scheme_find("light") != NULL);
    ASSERT_TRUE(sui_scheme_find("dark")  != NULL);
}

UTEST(scheme, find_missing_returns_null) {
    ASSERT_TRUE(sui_scheme_find("__nonexistent_XYZ__") == NULL);
}

UTEST(scheme, find_null_name_returns_null) {
    ASSERT_TRUE(sui_scheme_find(NULL) == NULL);
}

UTEST(scheme, register_then_find_returns_copy) {
    SuiColorScheme s;
    const SuiColorScheme* got;
    memset(&s, 0, sizeof(s));
    s.background = 0x123456u;
    ASSERT_GE(sui_scheme_register("__sui_test_a__", &s), 0);
    got = sui_scheme_find("__sui_test_a__");
    ASSERT_TRUE(got != NULL);
    ASSERT_EQ((uint32_t)0x123456u, got->background);
    sui_scheme_unregister("__sui_test_a__");
}

UTEST(scheme, register_overwrites_existing_slot) {
    SuiColorScheme s1, s2;
    int slot1, slot2;
    memset(&s1, 0, sizeof(s1));
    memset(&s2, 0, sizeof(s2));
    s1.background = 0x111111u;
    s2.background = 0x222222u;
    slot1 = sui_scheme_register("__sui_test_b__", &s1);
    slot2 = sui_scheme_register("__sui_test_b__", &s2);
    ASSERT_EQ(slot1, slot2);
    ASSERT_EQ((uint32_t)0x222222u, sui_scheme_find("__sui_test_b__")->background);
    sui_scheme_unregister("__sui_test_b__");
}

UTEST(scheme, list_includes_registered_name) {
    SuiColorScheme s;
    const char* names[SUI_MAX_SCHEMES];
    int n, i, found = 0;
    memset(&s, 0, sizeof(s));
    sui_scheme_register("__sui_test_c__", &s);
    n = sui_scheme_list(names, SUI_MAX_SCHEMES);
    for (i = 0; i < n; i++) {
        if (strcmp(names[i], "__sui_test_c__") == 0) { found = 1; break; }
    }
    sui_scheme_unregister("__sui_test_c__");
    ASSERT_TRUE(found);
}

UTEST(scheme, unregister_removes_entry) {
    SuiColorScheme s;
    memset(&s, 0, sizeof(s));
    sui_scheme_register("__sui_test_d__", &s);
    ASSERT_TRUE(sui_scheme_find("__sui_test_d__") != NULL);
    ASSERT_EQ(1, sui_scheme_unregister("__sui_test_d__"));
    ASSERT_TRUE(sui_scheme_find("__sui_test_d__") == NULL);
}

/* =========================================================================
 * Host-fixture tests — invisible top-level HWND + populated SuiControlHost.
 * Renderer is field-initialized only (no D2D factory created) so text-
 * measuring helpers short-circuit to 0; tests stay deterministic.
 * ========================================================================= */

struct host_fx {
    HWND hwnd;
    SuiRenderer renderer;
    SuiControlHost host;
};

static const wchar_t* SUI_TEST_WND_CLASS = L"SuiTestHost";

UTEST_F_SETUP(host_fx) {
    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = SUI_TEST_WND_CLASS;
    utest_fixture->hwnd = sui_test_window_acquire(&wc, L"",
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        WS_POPUP, -32000, -32000, 400, 300);
    ASSERT_TRUE(utest_fixture->hwnd != NULL);

    sui_renderer_init_fields(&utest_fixture->renderer);
    /* Wire the renderer to the test HWND so any DPI lookup is consistent
     * with what sui_begin would see. dwrite_factory stays NULL — text
     * measuring helpers handle that as zero-width. */
    utest_fixture->renderer.hwnd = utest_fixture->hwnd;

    memset(&utest_fixture->host, 0, sizeof(utest_fixture->host));
    utest_fixture->host.hwnd = utest_fixture->hwnd;
    utest_fixture->host.renderer = &utest_fixture->renderer;
    utest_fixture->host.scheme = sui_scheme_find("light");
    utest_fixture->host.dpi = 96;
    utest_fixture->host.focused_control = -1;
    utest_fixture->host.hover_control = -1;
    utest_fixture->host.tooltip_active_ctrl = -1;
}

UTEST_F_TEARDOWN(host_fx) {
    sui_ctrl_host_release(&utest_fixture->host);
    sui_test_window_release(utest_fixture->hwnd);
}

static void test_spinner_changed(int id, void* data) {
    (void)id;
    ++*(int*)data;
}

UTEST_F(host_fx, spinner_hold_delays_repeats_and_release_has_no_extra_step) {
    SuiControlHost* host = &utest_fixture->host;
    int changes = 0;
    int idx = sui_ctrl_add_spinner(host, 1, 0, 0, 100, 40, 0, 100, 10, 1);
    DWORD due;
    host->on_click = test_spinner_changed;
    host->on_click_data = &changes;
    sui_ctrl_on_lbutton_down(host, 90, 10);
    ASSERT_EQ(11.0f, host->controls[idx].d.spinner.value);
    ASSERT_EQ(1, changes);
    ASSERT_EQ(host->hwnd, GetCapture());
    due = host->spinner_repeat_due;
    sui_ctrl_spinner_repeat_tick(host, due - 1);
    ASSERT_EQ(1, changes);
    sui_ctrl_spinner_repeat_tick(host, due);
    ASSERT_EQ(12.0f, host->controls[idx].d.spinner.value);
    ASSERT_EQ(due + SUI_SPINNER_REPEAT_INTERVAL_MS, host->spinner_repeat_due);
    sui_ctrl_spinner_repeat_tick(host, due); /* stale duplicate */
    ASSERT_EQ(2, changes);
    host->spinner_repeat_due = GetTickCount();
    sui_ctrl_on_timer(host, SUI_SPINNER_REPEAT_TIMER_ID); /* real dispatcher */
    ASSERT_EQ(13.0f, host->controls[idx].d.spinner.value);
    sui_ctrl_on_lbutton_up(host, 90, 10);
    sui_ctrl_on_timer(host, SUI_SPINNER_REPEAT_TIMER_ID);
    ASSERT_EQ(3, changes);
    ASSERT_EQ(0, host->spinner_repeat_control);
    ASSERT_NE(host->hwnd, GetCapture());
    ASSERT_FALSE(KillTimer(host->hwnd, SUI_SPINNER_REPEAT_TIMER_ID));
}

UTEST_F(host_fx, spinner_hold_clamps_both_directions_and_stops_at_bounds) {
    SuiControlHost* host = &utest_fixture->host;
    int changes = 0;
    int i;
    int idx = sui_ctrl_add_spinner(host, 1, 0, 0, 100, 40, -0.5f, 0.5f, 0, 0.3f);
    host->on_click = test_spinner_changed;
    host->on_click_data = &changes;
    sui_ctrl_on_lbutton_down(host, 90, 10);
    sui_ctrl_spinner_repeat_tick(host, host->spinner_repeat_due);
    ASSERT_EQ(0.5f, host->controls[idx].d.spinner.value);
    ASSERT_EQ(0, host->spinner_repeat_control);
    ASSERT_EQ(2, changes);
    sui_ctrl_on_lbutton_down(host, 90, 10); /* already at maximum */
    ASSERT_EQ(2, changes);
    sui_ctrl_on_lbutton_down(host, 90, 30);
    for (i = 0; i < 3; ++i)
        sui_ctrl_spinner_repeat_tick(host, host->spinner_repeat_due);
    ASSERT_EQ(-0.5f, host->controls[idx].d.spinner.value);
    ASSERT_EQ(6, changes);
    ASSERT_FALSE(KillTimer(host->hwnd, SUI_SPINNER_REPEAT_TIMER_ID));
}

UTEST_F(host_fx, spinner_hold_cancels_when_pointer_leaves_original_arrow) {
    SuiControlHost* host = &utest_fixture->host;
    int idx = sui_ctrl_add_spinner(host, 1, 0, 0, 100, 40, 0, 100, 10, 1);
    sui_ctrl_on_lbutton_down(host, 90, 10);
    sui_ctrl_on_mouse_move(host, 90, 30); /* other arrow must not reverse a hold */
    ASSERT_EQ(0, host->spinner_repeat_control);
    sui_ctrl_on_mouse_move(host, 90, 10);
    sui_ctrl_on_timer(host, SUI_SPINNER_REPEAT_TIMER_ID);
    ASSERT_EQ(11.0f, host->controls[idx].d.spinner.value);
    sui_ctrl_on_lbutton_down(host, 90, 10);
    sui_ctrl_on_mouse_leave(host);
    ASSERT_EQ(0, host->spinner_repeat_control);
    sui_ctrl_on_lbutton_down(host, 90, 10);
    sui_ctrl_on_mouse_move(host, 120, 10);
    ASSERT_EQ(0, host->spinner_repeat_control);
}

UTEST_F(host_fx, spinner_hold_cancels_on_focus_capture_and_modal_changes) {
    SuiControlHost* host = &utest_fixture->host;
    UINT messages[] = { WM_KILLFOCUS, WM_CAPTURECHANGED, WM_CANCELMODE };
    int i;
    LRESULT result = 0;
    sui_ctrl_add_spinner(host, 1, 0, 0, 100, 40, 0, 100, 10, 1);
    for (i = 0; i < 3; ++i) {
        sui_ctrl_on_lbutton_down(host, 90, 10);
        sui_default_msg(host, host->hwnd, messages[i], 0, 0, &result);
        ASSERT_EQ(0, host->spinner_repeat_control);
        ASSERT_NE(host->hwnd, GetCapture());
        ASSERT_FALSE(KillTimer(host->hwnd, SUI_SPINNER_REPEAT_TIMER_ID));
    }
    sui_ctrl_on_lbutton_down(host, 90, 10);
    sui_ctrl_set_focus(host, -1);
    ASSERT_EQ(0, host->spinner_repeat_control);
    sui_ctrl_on_lbutton_down(host, 90, 10);
    ReleaseCapture(); /* manual dispatch consumers also stop at the next tick */
    sui_ctrl_spinner_repeat_tick(host, host->spinner_repeat_due);
    ASSERT_EQ(0, host->spinner_repeat_control);
}

UTEST_F(host_fx, spinner_hold_stops_when_disabled_hidden_rebuilt_or_released) {
    SuiControlHost* host = &utest_fixture->host;
    int flags[] = { SUI_CTRL_DISABLED, SUI_CTRL_HIDDEN };
    int i;
    int idx = sui_ctrl_add_spinner(host, 1, 0, 0, 100, 40, 0, 100, 10, 1);
    for (i = 0; i < 2; ++i) {
        float value;
        sui_ctrl_on_lbutton_down(host, 90, 10);
        value = host->controls[idx].d.spinner.value;
        host->controls[idx].state |= flags[i];
        sui_ctrl_spinner_repeat_tick(host, host->spinner_repeat_due);
        ASSERT_EQ(0, host->spinner_repeat_control);
        ASSERT_EQ(value, host->controls[idx].d.spinner.value);
        host->controls[idx].state &= ~flags[i];
    }
    sui_ctrl_on_lbutton_down(host, 90, 10);
    sui_ctrl_clear_page(host);
    idx = sui_ctrl_add_spinner(host, 2, 0, 0, 100, 40, 0, 100, 50, 1);
    sui_ctrl_on_timer(host, SUI_SPINNER_REPEAT_TIMER_ID);
    ASSERT_EQ(50.0f, host->controls[idx].d.spinner.value);
    sui_ctrl_on_lbutton_down(host, 90, 10);
    sui_ctrl_host_release(host);
    ASSERT_EQ(0, host->spinner_repeat_control);
    ASSERT_NE(host->hwnd, GetCapture());
    ASSERT_FALSE(KillTimer(host->hwnd, SUI_SPINNER_REPEAT_TIMER_ID));
}

static void test_spinner_rebuild_on_change(int id, void* data) {
    SuiControlHost* host = (SuiControlHost*)data;
    (void)id;
    sui_ctrl_clear_page(host);
    sui_ctrl_add_spinner(host, 2, 0, 0, 100, 40, 0, 100, 50, 1);
}

UTEST_F(host_fx, spinner_hold_callback_can_rebuild_on_first_or_repeated_step) {
    SuiControlHost* host = &utest_fixture->host;
    int i;
    sui_ctrl_add_spinner(host, 1, 0, 0, 100, 40, 0, 100, 10, 1);
    host->on_click_data = host;
    for (i = 0; i < 2; ++i) {
        host->on_click = i ? NULL : test_spinner_rebuild_on_change;
        sui_ctrl_on_lbutton_down(host, 90, 10);
        if (i) {
            host->on_click = test_spinner_rebuild_on_change;
            sui_ctrl_spinner_repeat_tick(host, host->spinner_repeat_due);
        }
        ASSERT_EQ(50.0f, host->controls[0].d.spinner.value);
        ASSERT_EQ(0, host->spinner_repeat_control);
        ASSERT_EQ(0, host->controls[0].state & SUI_CTRL_PRESSED);
        ASSERT_NE(host->hwnd, GetCapture());
        ASSERT_FALSE(KillTimer(host->hwnd, SUI_SPINNER_REPEAT_TIMER_ID));
    }
}

UTEST_F(host_fx, spinner_second_click_can_repeat_but_text_edit_does_not) {
    SuiControlHost* host = &utest_fixture->host;
    int idx = sui_ctrl_add_spinner(host, 1, 0, 0, 100, 40, 0, 100, 10, 1);
    sui_ctrl_on_lbutton_down(host, 90, 10);
    sui_ctrl_on_lbutton_up(host, 90, 10);
    sui_ctrl_on_lbutton_dblclk(host, 90, 10);
    sui_ctrl_spinner_repeat_tick(host, host->spinner_repeat_due);
    ASSERT_EQ(13.0f, host->controls[idx].d.spinner.value);
    sui_ctrl_on_lbutton_up(host, 90, 10);
    sui_ctrl_on_lbutton_down(host, 20, 10);
    ASSERT_EQ(1, host->controls[idx].d.spinner.editing);
    ASSERT_EQ(0, host->spinner_repeat_control);
    sui_ctrl_on_lbutton_up(host, 20, 10);
}

UTEST_F(host_fx, spinner_repeat_uses_no_toolkit_allocations_after_preparation) {
    SuiControlHost* host = &utest_fixture->host;
    size_t allocations;
    int i, changes = 0;
    int idx = sui_ctrl_add_spinner(host, 1, 0, 0, 100, 40, 0, 10000, 0, 1);
    host->on_click = test_spinner_changed;
    host->on_click_data = &changes;
    sui_ctrl_on_lbutton_down(host, 90, 10);
    sui_ctrl_spinner_repeat_tick(host, host->spinner_repeat_due);
    allocations = test_sui_allocations;
    for (i = 0; i < 1000; ++i)
        sui_ctrl_spinner_repeat_tick(host, host->spinner_repeat_due);
    ASSERT_EQ(allocations, test_sui_allocations);
    ASSERT_EQ(1002, changes);
    ASSERT_EQ(1002.0f, host->controls[idx].d.spinner.value);
    sui_ctrl_on_lbutton_up(host, 90, 10);
}

UTEST_F(host_fx, tooltip_timer_shows_without_further_mouse_movement_and_leave_hides) {
    SuiControlHost* host = &utest_fixture->host;
    int idx = sui_ctrl_add_button(host, 43, 0, 0, 120, 28, L"Help", 0);
    int shown, hidden;
    ASSERT_GE(idx, 0);
    sui_ctrl_set_tooltip(host, 43, L"Tooltip timer regression");
    sui_ctrl_on_mouse_move(host, 10, 10);
    ASSERT_EQ(idx, host->tooltip_active_ctrl);
    ASSERT_EQ(-1, host->tooltip_visible_ctrl);

    /* Advance the dwell deadline, then dispatch a timer with no second mouse move. */
    host->tooltip_hover_start_ms = GetTickCount() - SUI_TOOLTIP_DELAY_MS;
    sui_ctrl_on_timer(host, SUI_TOOLTIP_TIMER_ID);
    shown = host->tooltip_visible_ctrl == idx && host->tooltip_hwnd &&
        IsWindowVisible(host->tooltip_hwnd);
    sui_ctrl_on_mouse_leave(host);
    hidden = host->tooltip_active_ctrl == -1 && host->tooltip_visible_ctrl == -1 &&
        !IsWindowVisible(host->tooltip_hwnd);
    /* A queued tick after leave must not resurrect the tooltip. */
    sui_ctrl_on_timer(host, SUI_TOOLTIP_TIMER_ID);
    hidden = hidden && host->tooltip_visible_ctrl == -1;
    sui_ctrl_destroy_tooltip_popup(host);
    ASSERT_TRUE(shown);
    ASSERT_TRUE(hidden);
}

UTEST(shortcut, bluetooth_basic_report_does_not_invent_edge_fn) {
    BYTE basic[78] = {0};
    BYTE enhanced[78] = {0};
    BYTE usb[64] = {0};
    basic[0] = 0x01;
    basic[5] = 0x08;
    basic[7] = 0x11; /* PS plus vendor-defined bit 4. */
    enhanced[0] = 0x31;
    enhanced[9] = 0x08;
    enhanced[11] = 0x10;
    usb[0] = 0x01;
    usb[8] = 0x08;
    usb[10] = 0x10;

    ASSERT_EQ((uint32_t)SUI_GAMEPAD_GUIDE,
        sui_ctrl_shortcut_map_dualsense_report(basic, sizeof(basic)));
    ASSERT_EQ((uint32_t)SUI_SHORTCUT_GAMEPAD_EDGE_FN_LEFT,
        sui_ctrl_shortcut_map_dualsense_report(enhanced, sizeof(enhanced)));
    ASSERT_EQ((uint32_t)SUI_SHORTCUT_GAMEPAD_EDGE_FN_LEFT,
        sui_ctrl_shortcut_map_dualsense_report(usb, sizeof(usb)));
}

static void shortcut_test_committed(int id, void* user_data) {
    if (id == 42) ++*(int*)user_data;
}

UTEST_F(host_fx, gamepad_recorder_timer_captures_chord_and_commits_on_release) {
    SuiControlHost* host = &utest_fixture->host;
    int commits = 0;
    int idx = sui_ctrl_add_shortcut_recorder_gamepad(host, 42,
        0, 0, 160, 28, 0x0400u, L"Press gamepad buttons...");
    SuiControl* control;
    ASSERT_GE(idx, 0);
    control = &host->controls[idx];
    host->on_click = shortcut_test_committed;
    host->on_click_data = &commits;
    sui_ctrl_shortcut_set_recording(host, control, 1);

    /* Drive the production dispatcher, not the recorder helper: tooltip and
     * recorder timers share one HWND and must route to different handlers. */
    test_shortcut_gamepad_buttons = 0x1000u; /* Cross / A */
    sui_ctrl_on_timer(host, SUI_SHORTCUT_RECORD_TIMER_ID);
    ASSERT_EQ(0x1000u, control->d.shortcut.candidate_gamepad_buttons);
    test_shortcut_gamepad_buttons |= 0x0100u; /* L1 / LB */
    sui_ctrl_on_timer(host, SUI_SHORTCUT_RECORD_TIMER_ID);
    ASSERT_EQ(0x1100u, control->d.shortcut.candidate_gamepad_buttons);
    ASSERT_EQ(0, commits);

    test_shortcut_gamepad_buttons = 0;
    sui_ctrl_on_timer(host, SUI_TOOLTIP_TIMER_ID);
    ASSERT_TRUE(control->d.shortcut.recording);
    ASSERT_EQ(0, commits);
    sui_ctrl_on_timer(host, SUI_SHORTCUT_RECORD_TIMER_ID);
    ASSERT_FALSE(control->d.shortcut.recording);
    ASSERT_EQ(0x1100u, control->d.shortcut.gamepad_buttons);
    ASSERT_EQ(1, commits);
    sui_ctrl_on_timer(host, SUI_SHORTCUT_RECORD_TIMER_ID);
    ASSERT_EQ(1, commits);
}

/* Grayscale, smoothed both ways and fitted to the pixel grid. */
static int draws_even_strokes(SuiRenderer* r) {
    ID2D1RenderTarget* rt = (ID2D1RenderTarget*)r->render_target; IDWriteRenderingParams* params = NULL; int even;
    if (rt->lpVtbl->GetTextAntialiasMode(rt) != D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE) return 0;
    rt->lpVtbl->GetTextRenderingParams(rt, &params);
    if (!params) return 0;
    even = params->lpVtbl->GetRenderingMode(params) == DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC &&
        params->lpVtbl->GetGridFitMode(params) == DWRITE_GRID_FIT_MODE_ENABLED;
    params->lpVtbl->Release(params);
    return even;
}

UTEST_F(host_fx, window_and_popup_renderers_draw_even_strokes) {
    SuiRenderer renderer;

    sui_renderer_init_fields(&renderer);
    ASSERT_TRUE(sui_renderer_init(&renderer, utest_fixture->hwnd));
    ASSERT_TRUE(draws_even_strokes(&renderer));
    sui_renderer_release(&renderer);
    ASSERT_TRUE(renderer.text_params == NULL);

    ASSERT_TRUE(sui__popup_renderer_init(&renderer, utest_fixture->hwnd));
    ASSERT_TRUE(draws_even_strokes(&renderer));
    sui_renderer_release(&renderer);
}

/* Hinted text advances by whole pixels, so what is measured is whole too. */
UTEST_F(host_fx, text_is_laid_out_on_whole_pixels) {
    SuiRenderer renderer;
    float width;

    sui_renderer_init_fields(&renderer);
    ASSERT_TRUE(sui_renderer_init(&renderer, utest_fixture->hwnd));
    width = sui_renderer_measure_text_width(&renderer, L"Single-line label, measured", 0, 13.0f);
    ASSERT_GT(width, 10.0f);
    ASSERT_EQ(width, (float)(int)width);
    sui_renderer_release(&renderer);
}

/* ---- Control list lifecycle ------------------------------------------ */

UTEST_F(host_fx, ctrl_add_returns_index_zero_on_first_add) {
    int idx = sui_ctrl_add_label(&utest_fixture->host, 1, 0, 0, 100, 20, L"hello");
    ASSERT_EQ(0, idx);
    ASSERT_EQ(1, utest_fixture->host.control_count);
}

UTEST_F(host_fx, ctrl_add_increments_count_for_each_add) {
    sui_ctrl_add_label(&utest_fixture->host, 1, 0, 0, 100, 20, L"a");
    sui_ctrl_add_label(&utest_fixture->host, 2, 0, 0, 100, 20, L"b");
    sui_ctrl_add_label(&utest_fixture->host, 3, 0, 0, 100, 20, L"c");
    ASSERT_EQ(3, utest_fixture->host.control_count);
}

UTEST_F(host_fx, ctrl_find_returns_matching_control) {
    SuiControl* c;
    sui_ctrl_add_label(&utest_fixture->host, 7, 0, 0, 100, 20, L"x");
    c = sui_ctrl_find(&utest_fixture->host, 7);
    ASSERT_TRUE(c != NULL);
    ASSERT_EQ(7, c->id);
    ASSERT_EQ((int)SUI_CTRL_LABEL, (int)c->type);
}

UTEST_F(host_fx, ctrl_find_missing_id_returns_null) {
    sui_ctrl_add_label(&utest_fixture->host, 1, 0, 0, 100, 20, L"x");
    ASSERT_TRUE(sui_ctrl_find(&utest_fixture->host, 999) == NULL);
}

UTEST_F(host_fx, clear_page_resets_count_to_persistent) {
    sui_ctrl_add_label(&utest_fixture->host, 1, 0, 0, 100, 20, L"a");
    sui_ctrl_add_label(&utest_fixture->host, 2, 0, 0, 100, 20, L"b");
    ASSERT_EQ(2, utest_fixture->host.control_count);
    sui_ctrl_clear_page(&utest_fixture->host);
    ASSERT_EQ(0, utest_fixture->host.control_count);
    ASSERT_EQ(-1, utest_fixture->host.focused_control);
}

UTEST_F(host_fx, clear_page_keeps_persistent_controls) {
    SuiControlHost* host = &utest_fixture->host;
    sui_ctrl_add_label(host, 1, 0, 0, 100, 20, L"persist");
    host->persistent_control_count = 1; /* mark the label as persistent */
    sui_ctrl_add_label(host, 2, 0, 0, 100, 20, L"transient");
    ASSERT_EQ(2, host->control_count);
    sui_ctrl_clear_page(host);
    ASSERT_EQ(1, host->control_count);
    ASSERT_TRUE(sui_ctrl_find(host, 1) != NULL);
}

UTEST_F(host_fx, mouse_leave_clears_list_hover_row) {
    SuiControlHost* host = &utest_fixture->host;
    int idx = sui_ctrl_add_list(host, 9, 0, 0, 180, 80, 20.0f, 20.0f, 8);
    SuiControl* list;
    ASSERT_TRUE(idx >= 0);

    list = &host->controls[idx];
    list->state |= SUI_CTRL_HOVER;
    list->d.list.hover_row = 3;
    host->hover_control = idx;

    sui_ctrl_on_mouse_leave(host);

    ASSERT_EQ(-1, list->d.list.hover_row);
    ASSERT_EQ(0u, list->state & SUI_CTRL_HOVER);
    ASSERT_EQ(-1, host->hover_control);
}

UTEST_F(host_fx, style_color_overrides_fallback_and_clears_with_page) {
    SuiControlHost* host = &utest_fixture->host;
    int idx = sui_ctrl_add_label(host, 17, 0, 0, 100, 20, L"styled");
    SuiControl* c = &host->controls[idx];
    ASSERT_TRUE(c->style == NULL);
    ASSERT_EQ((uint32_t)0x112233u, sui_ctrl_style_color(c, SUI_STYLE_TEXT_COLOR, 0x112233u));

    sui_ctrl_set_style_color(host, 17, SUI_STYLE_TEXT_COLOR, 0x00ABCDEFu);
    ASSERT_TRUE(c->style != NULL);
    ASSERT_EQ((uint32_t)0x00ABCDEFu, sui_ctrl_style_color(c, SUI_STYLE_TEXT_COLOR, 0x112233u));

    sui_ctrl_clear_page(host);
    ASSERT_TRUE(c->style == NULL);
}

UTEST_F(host_fx, font_family_override_is_explicit_and_clearable) {
    SuiControlHost* host = &utest_fixture->host;
    int idx = sui_ctrl_add_label(host, 18, 0, 0, 100, 20, L"styled");
    SuiControl* c = &host->controls[idx];
    ASSERT_TRUE(sui_ctrl_style_font_family(c) == NULL);

    sui_ctrl_set_font_family(host, 18, L"Cambria");
    ASSERT_TRUE(sui_ctrl_style_font_family(c) != NULL);
    ASSERT_EQ(0, wcscmp(L"Cambria", sui_ctrl_style_font_family(c)));

    sui_ctrl_set_font_family(host, 18, NULL);
    ASSERT_TRUE(sui_ctrl_style_font_family(c) == NULL);
}

UTEST_F(host_fx, tabs_indicator_snaps_to_initial_selected_item) {
    SuiControlHost* host = &utest_fixture->host;
    int idx = sui_ctrl_add_tabs(host, 301, 0, 0, 200, 80, 34, 1);
    SuiControl* c = &host->controls[idx];
    float tx = 0.0f, tw = 0.0f;
    float s = sui_ctrl_dpi_scale(host);

    sui_ctrl_tabs_add_item(host, 301, L"One");
    sui_ctrl_tabs_add_item(host, 301, L"Two");

    ASSERT_TRUE(sui_ctrl_tabs_indicator_target(host, c, &tx, &tw));
    ASSERT_TRUE(tx > 0.0f);
    ASSERT_NEAR(16.0f * s, tw, 0.0001f);

    sui_ctrl_tabs_sync_indicator(host, c, 1);
    ASSERT_EQ(1, c->d.tabs.indicator_ready);
    ASSERT_EQ(0, c->d.tabs.indicator_anim_active);
    ASSERT_NEAR(tx, c->d.tabs.indicator_x, 0.0001f);
    ASSERT_NEAR(tw, c->d.tabs.indicator_w, 0.0001f);
    ASSERT_NEAR(tx, c->d.tabs.indicator_to_x, 0.0001f);
    ASSERT_NEAR(tw, c->d.tabs.indicator_to_w, 0.0001f);

    sui_ctrl_clear_page(host);
}

/* The panel's border is one stroke: walking in from its corner along the diagonal crosses one dark band. */
UTEST_F(host_fx, tabs_panel_corner_has_one_stroke) {
    enum { size = 160 };
    SuiControlHost* host = &utest_fixture->host;
    SuiColorScheme scheme = *host->scheme;
    SuiRenderer renderer;
    SuiRect content;
    BITMAPINFO bi;
    RECT bounds = { 0, 0, size, size };
    const unsigned char* bits = NULL;
    HDC dc = CreateCompatibleDC(NULL);
    HBITMAP bitmap;
    int i, idx, strokes = 0, in_stroke = 0;

    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = size;
    bi.bmiHeader.biHeight = -size;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bitmap = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, (void**)&bits, NULL, 0);
    ASSERT_TRUE(bitmap != NULL);
    SelectObject(dc, bitmap);
    ASSERT_TRUE(sui__popup_renderer_init(&renderer, utest_fixture->hwnd));
    ASSERT_TRUE(SUCCEEDED(((ID2D1DCRenderTarget*)renderer.render_target)->lpVtbl->BindDC(
        (ID2D1DCRenderTarget*)renderer.render_target, dc, &bounds)));

    scheme.background = 0xFFFFFF;
    scheme.btn_area_border = 0x000000;
    host->scheme = &scheme;
    host->renderer = &renderer;
    host->dpi = 384;   /* 1-pixel strokes round large corners: a second curve would stand clear of the first */
    idx = sui_ctrl_add_tabs(host, 301, 10, 10, 140, 140, 20, 0);
    ASSERT_TRUE(sui_ctrl_tabs_content_rect(&host->controls[idx], &content));
    sui_renderer_begin(&renderer);
    sui_renderer_clear(&renderer, 0xFFFFFF);
    sui_ctrl_draw_all(host);
    sui_renderer_end(&renderer);
    GdiFlush();

    for (i = 0; i < 40; i++) {
        int dark = bits[(((int)content.top + i) * size + (int)content.left + i) * 4 + 1] < 224;
        strokes += dark && !in_stroke;
        in_stroke = dark;
    }

    sui_ctrl_clear_page(host);
    host->renderer = &utest_fixture->renderer;
    host->scheme = sui_scheme_find("light");
    sui_renderer_release(&renderer);
    DeleteDC(dc);
    DeleteObject(bitmap);
    ASSERT_EQ(1, strokes);
}

UTEST_F(host_fx, tabs_indicator_resting_width_is_adjustable) {
    SuiControlHost* host = &utest_fixture->host;
    int idx = sui_ctrl_add_tabs(host, 305, 0, 0, 200, 80, 34, 0);
    SuiControl* c = &host->controls[idx];
    float tx = 0.0f, tw = 0.0f;
    float s = sui_ctrl_dpi_scale(host);

    sui_ctrl_tabs_add_item(host, 305, L"One");
    sui_ctrl_tabs_add_item(host, 305, L"Two");

    ASSERT_TRUE(sui_ctrl_tabs_indicator_target(host, c, &tx, &tw));
    ASSERT_NEAR(16.0f * s, tw, 0.0001f);

    sui_ctrl_tabs_set_indicator_width(host, 305, 32.0f * s);
    ASSERT_TRUE(sui_ctrl_tabs_indicator_target(host, c, &tx, &tw));
    ASSERT_NEAR(32.0f * s, tw, 0.0001f);

    sui_ctrl_clear_page(host);
}

UTEST_F(host_fx, tabs_indicator_stretches_then_compresses_toward_new_selected_item) {
    SuiControlHost* host = &utest_fixture->host;
    int idx = sui_ctrl_add_tabs(host, 302, 0, 0, 200, 80, 34, 0);
    SuiControl* c = &host->controls[idx];
    float from_x, from_w, to_x = 0.0f, to_w = 0.0f, to_r;
    float peak_w, compress_w;

    sui_ctrl_tabs_add_item(host, 302, L"One");
    sui_ctrl_tabs_add_item(host, 302, L"Two");
    sui_ctrl_tabs_sync_indicator(host, c, 1);
    from_x = c->d.tabs.indicator_x;
    from_w = c->d.tabs.indicator_w;

    c->d.tabs.selected = 1;
    ASSERT_TRUE(sui_ctrl_tabs_indicator_target(host, c, &to_x, &to_w));
    to_r = to_x + to_w;
    ASSERT_NEAR(from_w, to_w, 0.0001f);
    sui_ctrl_tabs_start_indicator_transition(host, c);

    ASSERT_EQ(1, sui_ctrl_tabs_indicator_tick(host, c, 0.33333334f));
    ASSERT_EQ(1, c->d.tabs.indicator_anim_active);
    ASSERT_NEAR(from_x, c->d.tabs.indicator_x, 0.1f);
    ASSERT_NEAR(to_r, c->d.tabs.indicator_x + c->d.tabs.indicator_w, 0.1f);
    ASSERT_TRUE(c->d.tabs.indicator_w > from_w + 1.0f);
    peak_w = c->d.tabs.indicator_w;

    ASSERT_EQ(1, sui_ctrl_tabs_indicator_tick(host, c, 0.33333334f));
    ASSERT_EQ(1, c->d.tabs.indicator_anim_active);
    ASSERT_TRUE(c->d.tabs.indicator_x > from_x);
    ASSERT_NEAR(to_r, c->d.tabs.indicator_x + c->d.tabs.indicator_w, 0.1f);
    ASSERT_TRUE(c->d.tabs.indicator_w < peak_w);
    ASSERT_TRUE(c->d.tabs.indicator_w > to_w);
    compress_w = c->d.tabs.indicator_w;

    ASSERT_EQ(1, sui_ctrl_tabs_indicator_tick(host, c, 1.0f));
    ASSERT_NEAR(to_x, c->d.tabs.indicator_x, 0.0001f);
    ASSERT_NEAR(to_w, c->d.tabs.indicator_w, 0.0001f);
    ASSERT_TRUE(c->d.tabs.indicator_w < compress_w);
    ASSERT_EQ(0, c->d.tabs.indicator_anim_active);
    ASSERT_EQ(0, sui_ctrl_tabs_indicator_tick(host, c, 1.0f));

    sui_ctrl_clear_page(host);
}

UTEST_F(host_fx, tabs_indicator_stretches_when_moving_left) {
    SuiControlHost* host = &utest_fixture->host;
    int idx = sui_ctrl_add_tabs(host, 303, 0, 0, 200, 80, 34, 1);
    SuiControl* c = &host->controls[idx];
    float from_x, from_w, from_r, to_x = 0.0f, to_w = 0.0f, to_r;
    float peak_w;

    sui_ctrl_tabs_add_item(host, 303, L"One");
    sui_ctrl_tabs_add_item(host, 303, L"Two");
    sui_ctrl_tabs_sync_indicator(host, c, 1);
    from_x = c->d.tabs.indicator_x;
    from_w = c->d.tabs.indicator_w;
    from_r = from_x + from_w;

    c->d.tabs.selected = 0;
    ASSERT_TRUE(sui_ctrl_tabs_indicator_target(host, c, &to_x, &to_w));
    to_r = to_x + to_w;
    ASSERT_NEAR(from_w, to_w, 0.0001f);
    sui_ctrl_tabs_start_indicator_transition(host, c);

    ASSERT_EQ(1, sui_ctrl_tabs_indicator_tick(host, c, 0.33333334f));
    ASSERT_EQ(1, c->d.tabs.indicator_anim_active);
    ASSERT_NEAR(to_x, c->d.tabs.indicator_x, 0.1f);
    ASSERT_NEAR(from_r, c->d.tabs.indicator_x + c->d.tabs.indicator_w, 0.1f);
    ASSERT_TRUE(c->d.tabs.indicator_w > from_w + 1.0f);
    peak_w = c->d.tabs.indicator_w;

    ASSERT_EQ(1, sui_ctrl_tabs_indicator_tick(host, c, 0.33333334f));
    ASSERT_EQ(1, c->d.tabs.indicator_anim_active);
    ASSERT_NEAR(to_x, c->d.tabs.indicator_x, 0.1f);
    ASSERT_TRUE(c->d.tabs.indicator_x + c->d.tabs.indicator_w < from_r);
    ASSERT_TRUE(c->d.tabs.indicator_x + c->d.tabs.indicator_w > to_r);
    ASSERT_TRUE(c->d.tabs.indicator_w < peak_w);
    ASSERT_TRUE(c->d.tabs.indicator_w > to_w);

    ASSERT_EQ(1, sui_ctrl_tabs_indicator_tick(host, c, 1.0f));
    ASSERT_NEAR(to_x, c->d.tabs.indicator_x, 0.0001f);
    ASSERT_NEAR(to_w, c->d.tabs.indicator_w, 0.0001f);
    ASSERT_EQ(0, c->d.tabs.indicator_anim_active);

    sui_ctrl_clear_page(host);
}

UTEST_F(host_fx, tabs_indicator_survives_page_rebuild_before_first_tick) {
    SuiControlHost* host = &utest_fixture->host;
    int idx = sui_ctrl_add_tabs(host, 304, 0, 0, 200, 80, 34, 0);
    SuiControl* c = &host->controls[idx];
    float from_x, from_w, to_x = 0.0f, to_w = 0.0f, to_r;

    sui_ctrl_tabs_add_item(host, 304, L"One");
    sui_ctrl_tabs_add_item(host, 304, L"Two");
    sui_ctrl_tabs_sync_indicator(host, c, 1);
    from_x = c->d.tabs.indicator_x;
    from_w = c->d.tabs.indicator_w;
    c->d.tabs.selected = 1;
    ASSERT_TRUE(sui_ctrl_tabs_indicator_target(host, c, &to_x, &to_w));
    to_r = to_x + to_w;
    ASSERT_NEAR(from_w, to_w, 0.0001f);
    sui_ctrl_tabs_start_indicator_transition(host, c);

    sui_ctrl_clear_page(host);

    idx = sui_ctrl_add_tabs(host, 304, 0, 0, 200, 80, 34, 1);
    c = &host->controls[idx];
    sui_ctrl_tabs_add_item(host, 304, L"One");
    sui_ctrl_tabs_add_item(host, 304, L"Two");

    ASSERT_EQ(1, c->d.tabs.indicator_ready);
    ASSERT_EQ(1, c->d.tabs.indicator_anim_active);
    ASSERT_NEAR(from_x, c->d.tabs.indicator_x, 0.0001f);
    ASSERT_NEAR(from_w, c->d.tabs.indicator_w, 0.0001f);

    ASSERT_EQ(1, sui_ctrl_tabs_indicator_tick(host, c, 0.33333334f));
    ASSERT_EQ(1, c->d.tabs.indicator_anim_active);
    ASSERT_NEAR(from_x, c->d.tabs.indicator_x, 0.1f);
    ASSERT_NEAR(to_r, c->d.tabs.indicator_x + c->d.tabs.indicator_w, 0.1f);
    ASSERT_TRUE(c->d.tabs.indicator_w > from_w + 1.0f);

    sui_ctrl_clear_page(host);
}

UTEST_F(host_fx, list_flat_mode_selects_first_item_by_default) {
    SuiControlHost* host = &utest_fixture->host;
    int idx = sui_ctrl_add_list(host, 201, 0, 0, 160, 80, 24, 18, 2);
    SuiControl* c = &host->controls[idx];

    ASSERT_EQ(0, sui_ctrl_list_add_item(host, 201, 100, L"EN", L"market", L"mercado"));
    ASSERT_EQ(1, sui_ctrl_list_add_item(host, 201, 101, L"FR", L"bonjour", L"hello"));
    ASSERT_EQ((int)SUI_CTRL_LIST, (int)c->type);
    ASSERT_EQ(2, c->d.list.row_count);
    ASSERT_EQ(100, sui_ctrl_list_get_selected_item(host, 201));
    ASSERT_EQ(1, sui_ctrl_list_set_selected_item(host, 201, 101));
    ASSERT_EQ(101, sui_ctrl_list_get_selected_item(host, 201));

    sui_ctrl_clear_page(host);
}

UTEST_F(host_fx, list_item_indent_defaults_zero_and_is_settable) {
    SuiControlHost* host = &utest_fixture->host;
    int idx = sui_ctrl_add_list(host, 204, 0, 0, 160, 80, 24, 18, 2);
    SuiControl* c = &host->controls[idx];

    ASSERT_NEAR(0.0f, c->d.list.item_indent, 0.0001f);
    sui_ctrl_list_set_item_indent(host, 204, 18.0f);
    ASSERT_NEAR(18.0f, c->d.list.item_indent, 0.0001f);
    sui_ctrl_list_set_item_indent(host, 204, -4.0f);
    ASSERT_NEAR(0.0f, c->d.list.item_indent, 0.0001f);

    sui_ctrl_clear_page(host);
}

UTEST_F(host_fx, combobox_owner_subclass_supports_two_hosts_on_one_root) {
    SuiControlHost* first = &utest_fixture->host;
    SuiControlHost* second = (SuiControlHost*)sui_malloc(sizeof(*second));
    SuiComboPopup* first_popup = (SuiComboPopup*)sui_malloc(sizeof(*first_popup));
    SuiComboPopup* second_popup = (SuiComboPopup*)sui_malloc(sizeof(*second_popup));
    SuiComboOwnerState* state;
    WNDPROC original = (WNDPROC)GetWindowLongPtrW(utest_fixture->hwnd, GWLP_WNDPROC);

    ASSERT_NE((void*)NULL, second);
    ASSERT_NE((void*)NULL, first_popup);
    ASSERT_NE((void*)NULL, second_popup);
    memset(second, 0, sizeof(*second));
    memset(first_popup, 0, sizeof(*first_popup));
    memset(second_popup, 0, sizeof(*second_popup));
    second->hwnd = utest_fixture->hwnd;
    second->renderer = &utest_fixture->renderer;
    second->scheme = utest_fixture->host.scheme;
    second->dpi = 96;
    first->combo_popup = first_popup;
    second->combo_popup = second_popup;

    sui_combo_owner_subclass_install(first);
    sui_combo_owner_subclass_install(second);

    state = (SuiComboOwnerState*)GetPropW(utest_fixture->hwnd,
                                          SUI_COMBO_OWNER_STATE_PROP);
    ASSERT_NE((void*)NULL, state);
    ASSERT_EQ(2, state->host_count);
    ASSERT_EQ((void*)sui_combo_owner_subclass_proc,
              (void*)GetWindowLongPtrW(utest_fixture->hwnd, GWLP_WNDPROC));
    ASSERT_NE((void*)sui_combo_owner_subclass_proc, (void*)state->orig_proc);

    /* Releasing one host must leave the other registration and root subclass
     * intact; releasing the last host must restore the original WndProc. */
    sui_ctrl_host_release(first);
    state = (SuiComboOwnerState*)GetPropW(utest_fixture->hwnd,
                                          SUI_COMBO_OWNER_STATE_PROP);
    ASSERT_NE((void*)NULL, state);
    ASSERT_EQ(1, state->host_count);
    ASSERT_EQ(second, state->hosts[0]);
    ASSERT_EQ((void*)sui_combo_owner_subclass_proc,
              (void*)GetWindowLongPtrW(utest_fixture->hwnd, GWLP_WNDPROC));

    sui_ctrl_host_release(second);
    ASSERT_EQ((void*)NULL,
              (void*)GetPropW(utest_fixture->hwnd, SUI_COMBO_OWNER_STATE_PROP));
    ASSERT_EQ((void*)original,
              (void*)GetWindowLongPtrW(utest_fixture->hwnd, GWLP_WNDPROC));
    sui_free(second);
}

UTEST_F(host_fx, list_options_default_empty_and_are_settable) {
    SuiControlHost* host = &utest_fixture->host;
    int idx = sui_ctrl_add_list(host, 206, 0, 0, 160, 80, 24, 18, 2);
    SuiControl* c = &host->controls[idx];

    ASSERT_EQ((uint32_t)SUI_LIST_OPTION_NONE, c->d.list.options);
    sui_ctrl_list_set_options(host, 206, SUI_LIST_OPTION_SINGLE_LINE_TAIL_ELLIPSIS);
    ASSERT_EQ((uint32_t)SUI_LIST_OPTION_SINGLE_LINE_TAIL_ELLIPSIS, c->d.list.options);
}

UTEST_F(host_fx, list_sectioned_keyboard_skips_section_headers) {
    SuiControlHost* host = &utest_fixture->host;
    int idx = sui_ctrl_add_list(host, 202, 0, 0, 160, 80, 24, 18, 4);
    SuiControl* c = &host->controls[idx];

    sui_ctrl_list_add_section(host, 202, L"Today");
    sui_ctrl_list_add_item(host, 202, 10, L"EN", L"substantial", L"important");
    sui_ctrl_list_add_section(host, 202, L"Yesterday");
    sui_ctrl_list_add_item(host, 202, 20, L"ES", L"abarrotada", L"crowded");

    ASSERT_EQ(10, sui_ctrl_list_get_selected_item(host, 202));
    ASSERT_EQ(1, sui_ctrl_list_on_keydown(host, c, VK_DOWN));
    ASSERT_EQ(20, sui_ctrl_list_get_selected_item(host, 202));
    ASSERT_EQ(1, sui_ctrl_list_on_keydown(host, c, VK_UP));
    ASSERT_EQ(10, sui_ctrl_list_get_selected_item(host, 202));

    sui_ctrl_clear_page(host);
}

UTEST_F(host_fx, list_near_end_reports_when_scroll_reaches_threshold) {
    SuiControlHost* host = &utest_fixture->host;
    int idx = sui_ctrl_add_list(host, 203, 0, 0, 160, 60, 20, 18, 4);
    SuiControl* c = &host->controls[idx];
    int i;

    for (i = 0; i < 20; ++i) {
        wchar_t text[16];
        wsprintfW(text, L"word %d", i);
        sui_ctrl_list_add_item(host, 203, i, L"EN", text, L"");
    }

    ASSERT_EQ(0, sui_ctrl_list_is_near_end(host, 203));
    c->d.list.scroll_offset = 200.0f;
    ASSERT_EQ(1, sui_ctrl_list_is_near_end(host, 203));

    sui_ctrl_clear_page(host);
}

typedef struct sui__test_virtual_list_data {
    int get_row_calls;
    int row_at_offset_calls;
    int row_for_item_calls;
} sui__test_virtual_list_data;

static void sui__test_virtual_list_get_row(SuiControl* ctrl, int row_index,
                                           SuiListRow* out_row, void* userdata) {
    sui__test_virtual_list_data* data = (sui__test_virtual_list_data*)userdata;
    (void)ctrl;
    if (data) data->get_row_calls++;
    memset(out_row, 0, sizeof(*out_row));
    if (row_index == 0 || row_index == 3) {
        out_row->kind = SUI_LIST_ROW_SECTION;
        sui__wstrncpy_z(out_row->text, row_index == 0 ? L"Today" : L"Yesterday", 96);
        out_row->item_index = -1;
        return;
    }
    out_row->kind = SUI_LIST_ROW_ITEM;
    out_row->item_index = row_index == 1 ? 100 : (row_index == 2 ? 101 : 200);
    sui__wstrncpy_z(out_row->leading, L"EN", 16);
    wsprintfW(out_row->text, L"word %d", out_row->item_index);
}

static int sui__test_virtual_list_row_at_offset(SuiControl* ctrl, float offset,
                                                float* out_row_top, void* userdata) {
    sui__test_virtual_list_data* data = (sui__test_virtual_list_data*)userdata;
    float top = 0.0f;
    int i;
    (void)ctrl;
    if (data) data->row_at_offset_calls++;
    for (i = 0; i < 5; ++i) {
        float h = (i == 0 || i == 3) ? 18.0f : 24.0f;
        if (offset >= top && offset < top + h) {
            if (out_row_top) *out_row_top = top;
            return i;
        }
        top += h;
    }
    if (out_row_top) *out_row_top = 84.0f;
    return 4;
}

static int sui__test_virtual_list_row_for_item(SuiControl* ctrl, int item_index, void* userdata) {
    sui__test_virtual_list_data* data = (sui__test_virtual_list_data*)userdata;
    (void)ctrl;
    if (data) data->row_for_item_calls++;
    if (item_index == 100) return 1;
    if (item_index == 101) return 2;
    if (item_index == 200) return 4;
    return -1;
}

UTEST_F(host_fx, list_virtual_mode_uses_provider_without_materialized_rows) {
    SuiControlHost* host = &utest_fixture->host;
    sui__test_virtual_list_data data;
    int idx = sui_ctrl_add_list(host, 205, 0, 0, 160, 48, 24, 18, 2);
    SuiControl* c = &host->controls[idx];
    memset(&data, 0, sizeof(data));

    sui_ctrl_list_set_virtual(host, 205, 5, 108.0f,
                              sui__test_virtual_list_get_row,
                              sui__test_virtual_list_row_at_offset,
                              sui__test_virtual_list_row_for_item,
                              &data);

    ASSERT_EQ(0, c->d.list.row_count);
    ASSERT_EQ(5, sui_ctrl_list_row_count(c));
    ASSERT_NEAR(108.0f, sui_ctrl_list_content_height(c), 0.0001f);
    ASSERT_EQ(1, sui_ctrl_list_set_selected_item(host, 205, 200));
    ASSERT_EQ(200, sui_ctrl_list_get_selected_item(host, 205));
    ASSERT_EQ(2, sui_ctrl_list_row_at(c, 43.0f));
    ASSERT_TRUE(data.get_row_calls > 0);
    ASSERT_TRUE(data.row_at_offset_calls > 0);
    ASSERT_TRUE(data.row_for_item_calls > 0);

    sui_ctrl_clear_page(host);
}

typedef struct sui__test_virtual_combo_data {
    int get_item_calls;
} sui__test_virtual_combo_data;

static void sui__test_virtual_combo_get_item(SuiControl* ctrl, int item_index,
                                             SuiComboItemView* out_item, void* userdata) {
    sui__test_virtual_combo_data* data = (sui__test_virtual_combo_data*)userdata;
    (void)ctrl;
    if (data) data->get_item_calls++;
    memset(out_item, 0, sizeof(*out_item));
    wsprintfW(out_item->text, L"Item %d", item_index);
    out_item->has_icon = (item_index % 2) == 0;
}

UTEST_F(host_fx, combobox_virtual_mode_uses_provider_without_materialized_items) {
    SuiControlHost* host = &utest_fixture->host;
    const wchar_t* seed[] = { L"seed" };
    sui__test_virtual_combo_data data;
    SuiComboItemView item;
    int idx = sui_ctrl_add_combobox(host, 206, 0, 0, 220, 28, seed, 1, 0);
    SuiControl* c = &host->controls[idx];
    memset(&data, 0, sizeof(data));

    sui_ctrl_combo_set_virtual(host, 206, 100000, 99999,
                               sui__test_virtual_combo_get_item, &data);

    ASSERT_EQ(1, c->d.combo.item_count);
    ASSERT_EQ(100000, sui_ctrl_combo_count(c));
    ASSERT_EQ(99999, c->d.combo.selected);
    ASSERT_EQ(0, wcscmp(L"Item 99999", c->text));
    sui_ctrl_combo_get_item_view(c, 12345, &item);
    ASSERT_EQ(0, wcscmp(L"Item 12345", item.text));
    ASSERT_TRUE(data.get_item_calls >= 2);

    sui_ctrl_clear_page(host);
}

UTEST_F(host_fx, combobox_popup_width_can_differ_from_closed_control) {
    SuiControlHost* host = &utest_fixture->host;
    const wchar_t* items[] = { L"Alpha", L"Beta" };
    int idx = sui_ctrl_add_combobox(host, 208, 0, 0, 220, 28, items, 2, 0);
    SuiControl* c = &host->controls[idx];

    ASSERT_NEAR(220.0f, sui_ctrl_combo_popup_width(c), 0.0001f);

    sui_ctrl_combo_set_popup_width(host, 208, 440.0f);
    ASSERT_NEAR(220.0f, c->w, 0.0001f);
    ASSERT_NEAR(440.0f, c->d.combo.popup_width, 0.0001f);
    ASSERT_NEAR(440.0f, sui_ctrl_combo_popup_width(c), 0.0001f);

    sui_ctrl_combo_set_popup_width(host, 208, 0.0f);
    ASSERT_NEAR(0.0f, c->d.combo.popup_width, 0.0001f);
    ASSERT_NEAR(220.0f, sui_ctrl_combo_popup_width(c), 0.0001f);

    sui_ctrl_clear_page(host);
}

UTEST_F(host_fx, open_combobox_pressed_state_survives_pointer_release) {
    SuiControlHost* host = &utest_fixture->host;
    const wchar_t* items[] = { L"Alpha", L"Beta" };
    int idx = sui_ctrl_add_combobox(host, 207, 0, 0, 220, 28, items, 2, 0);
    SuiControl* c = &host->controls[idx];

    sui_ctrl_combo_set_open_state(c, 1);
    sui_ctrl_on_lbutton_up(host, 400, 400);

    ASSERT_EQ(1, c->d.combo.open);
    ASSERT_TRUE((c->state & SUI_CTRL_PRESSED) != 0);

    sui_ctrl_combo_set_open_state(c, 0);
    sui_ctrl_clear_page(host);
}

UTEST_F(host_fx, declarative_last_style_helpers_mutate_last_control) {
    SuiControlHost* host = &utest_fixture->host;
    int idx = sui_ctrl_add_button(host, 19, 0, 0, 100, 28, L"Button", 0);
    SuiControl* c = &host->controls[idx];

    sui_last_style_color(host, SUI_STYLE_BG_COLOR, 0x00112233u);
    sui_last_font_family(host, L"Consolas");

    ASSERT_EQ((uint32_t)0x00112233u, sui_ctrl_style_color(c, SUI_STYLE_BG_COLOR, 0));
    ASSERT_TRUE(sui_ctrl_style_font_family(c) != NULL);
    ASSERT_EQ(0, wcscmp(L"Consolas", sui_ctrl_style_font_family(c)));
}

static int sui__test_click_count = 0;
static int sui__test_click_id = 0;

static void sui__test_on_click(int ctrl_id, void* user_data) {
    (void)user_data;
    sui__test_click_count++;
    sui__test_click_id = ctrl_id;
}

UTEST_F(host_fx, nav_content_wheel_is_opt_in_accumulates_and_does_not_wrap) {
    SuiControlHost* host = &utest_fixture->host;
    int idx = sui_ctrl_add_nav_list(host, 90, 0, 0, 100, 180, 40, 0);
    SuiControl* nav = &host->controls[idx];
    host->on_click = sui__test_on_click;
    sui__test_click_count = 0;
    sui__test_click_id = 0;
    sui_ctrl_nav_add_item(host, 90, L"One", L"", 0);
    sui_ctrl_nav_add_item(host, 90, L"Two", L"", 0);
    sui_ctrl_nav_add_item(host, 90, L"Three", L"", 0);

    /* Outside-nav wheel input does nothing until the content region opts in. */
    sui_ctrl_on_mouse_wheel(host, 250, 50, -WHEEL_DELTA);
    ASSERT_EQ(0, nav->d.nav.selected);
    ASSERT_EQ(0, sui__test_click_count);

    sui_ctrl_nav_set_content_wheel_region(host, 90, 200, 0, 300, 180, 1);
    sui_ctrl_on_mouse_wheel(host, 250, 50, -WHEEL_DELTA / 2);
    ASSERT_EQ(0, nav->d.nav.selected);
    sui_ctrl_on_mouse_wheel(host, 250, 50, -WHEEL_DELTA / 2);
    ASSERT_EQ(1, nav->d.nav.selected);
    ASSERT_EQ(1, sui__test_click_count);
    ASSERT_EQ(90, sui__test_click_id);

    /* Wheel-up selects the previous item; the ends do not wrap. */
    sui_ctrl_on_mouse_wheel(host, 250, 50, WHEEL_DELTA);
    ASSERT_EQ(0, nav->d.nav.selected);
    ASSERT_EQ(2, sui__test_click_count);
    sui_ctrl_on_mouse_wheel(host, 250, 50, WHEEL_DELTA);
    ASSERT_EQ(0, nav->d.nav.selected);
    ASSERT_EQ(2, sui__test_click_count);

    /* The configured rectangle, not the whole host, owns the fallback. */
    sui_ctrl_on_mouse_wheel(host, 550, 50, -WHEEL_DELTA);
    ASSERT_EQ(0, nav->d.nav.selected);
}

UTEST_F(host_fx, nav_content_wheel_is_disabled_by_any_overflow_in_region) {
    SuiControlHost* host = &utest_fixture->host;
    int nav_idx = sui_ctrl_add_nav_list(host, 91, 0, 0, 100, 180, 40, 0);
    SuiControl* nav = &host->controls[nav_idx];
    int scroll_idx;
    SuiControl* scroll;
    host->on_click = sui__test_on_click;
    sui__test_click_count = 0;
    sui_ctrl_nav_add_item(host, 91, L"One", L"", 0);
    sui_ctrl_nav_add_item(host, 91, L"Two", L"", 0);
    sui_ctrl_nav_set_content_wheel_region(host, 91, 200, 0, 300, 180, 1);

    /* The pointer is over empty content, but an overflowing scroller anywhere
     * in the opted-in region suppresses section navigation for the page. */
    scroll_idx = sui_ctrl_add_scrollview(host, 92, 200, 0, 100, 100, 101);
    scroll = &host->controls[scroll_idx];
    sui_ctrl_on_mouse_wheel(host, 450, 50, -WHEEL_DELTA);
    ASSERT_EQ(0, nav->d.nav.selected);
    ASSERT_EQ(0, sui__test_click_count);

    /* A scroll-capable control whose content fits has no scroll range and does
     * not block the optional wheel navigation. */
    sui_ctrl_set_scrollview_content_height(host, 92, 100);
    ASSERT_NEAR(100.0f, scroll->d.scroll.content_h, 0.0001f);
    sui_ctrl_on_mouse_wheel(host, 450, 50, -WHEEL_DELTA);
    ASSERT_EQ(1, nav->d.nav.selected);
    ASSERT_EQ(1, sui__test_click_count);
}

UTEST_F(host_fx, tabs_add_items_preserves_selected) {
    SuiControlHost* host = &utest_fixture->host;
    int idx = sui_ctrl_add_tabs(host, 77, 10, 20, 240, 120, 34, 1);
    SuiControl* tabs = &host->controls[idx];

    sui_ctrl_tabs_add_item(host, 77, L"General");
    sui_ctrl_tabs_add_item(host, 77, L"Advanced");

    ASSERT_EQ((int)SUI_CTRL_TABS, (int)tabs->type);
    ASSERT_EQ(2, tabs->d.tabs.item_count);
    ASSERT_EQ(1, tabs->d.tabs.selected);
    ASSERT_EQ(0, wcscmp(L"Advanced", tabs->d.tabs.items[1].text));
}

UTEST_F(host_fx, tabs_content_rect_starts_below_tab_strip) {
    SuiControlHost* host = &utest_fixture->host;
    SuiRect content;
    int idx = sui_ctrl_add_tabs(host, 78, 10, 20, 240, 120, 34, 0);
    SuiControl* tabs = &host->controls[idx];

    ASSERT_TRUE(sui_ctrl_tabs_content_rect(tabs, &content));
    ASSERT_NEAR(10.0f, content.left, 0.0001f);
    ASSERT_NEAR(60.0f, content.top, 0.0001f);
    ASSERT_NEAR(250.0f, content.right, 0.0001f);
    ASSERT_NEAR(140.0f, content.bottom, 0.0001f);
}

UTEST_F(host_fx, tabs_spacing_is_adjustable_and_affects_hit_test) {
    SuiControlHost* host = &utest_fixture->host;
    SuiRect content;
    float first_x, first_w, second_x, second_w;
    int idx = sui_ctrl_add_tabs(host, 81, 10, 20, 240, 120, 34, 0);
    SuiControl* tabs = &host->controls[idx];

    sui_ctrl_tabs_set_spacing(host, 81, 10.0f, 12.0f);
    sui_ctrl_tabs_add_item(host, 81, L"One");
    sui_ctrl_tabs_add_item(host, 81, L"Two");

    ASSERT_TRUE(sui_ctrl_tabs_content_rect(tabs, &content));
    ASSERT_NEAR(66.0f, content.top, 0.0001f);
    sui_ctrl_tabs_item_rect(host, tabs, 0, &first_x, &first_w);
    sui_ctrl_tabs_item_rect(host, tabs, 1, &second_x, &second_w);
    ASSERT_NEAR(10.0f, second_x - (first_x + first_w), 0.0001f);
    ASSERT_EQ(-1, sui_ctrl_tabs_item_at(
        host, tabs, first_x + first_w + 5.0f, 30.0f));
    ASSERT_EQ(1, sui_ctrl_tabs_item_at(
        host, tabs, second_x + second_w * 0.5f, 30.0f));
}

UTEST_F(host_fx, tabs_click_changes_selected_and_fires_change) {
    SuiControlHost* host = &utest_fixture->host;
    float second_x, second_w;
    int click_x;
    int idx = sui_ctrl_add_tabs(host, 79, 10, 20, 240, 120, 34, 0);
    SuiControl* tabs = &host->controls[idx];
    host->on_click = sui__test_on_click;
    host->on_click_data = NULL;
    sui__test_click_count = 0;
    sui__test_click_id = 0;

    sui_ctrl_tabs_add_item(host, 79, L"One");
    sui_ctrl_tabs_add_item(host, 79, L"Two");
    sui_ctrl_tabs_add_item(host, 79, L"Three");

    sui_ctrl_tabs_item_rect(host, tabs, 1, &second_x, &second_w);
    click_x = (int)(second_x + second_w * 0.5f);
    sui_ctrl_on_mouse_move(host, click_x, 30);
    sui_ctrl_on_lbutton_down(host, click_x, 30);
    sui_ctrl_on_lbutton_up(host, click_x, 30);

    ASSERT_EQ(1, tabs->d.tabs.selected);
    ASSERT_EQ(1, sui__test_click_count);
    ASSERT_EQ(79, sui__test_click_id);
}

UTEST_F(host_fx, tabs_content_transition_click_left_enters_from_right) {
    SuiControlHost* host = &utest_fixture->host;
    int idx = sui_ctrl_add_tabs(host, 83, 10, 20, 240, 120, 34, 2);
    SuiControl* tabs = &host->controls[idx];

    sui_ctrl_tabs_add_item(host, 83, L"One");
    sui_ctrl_tabs_add_item(host, 83, L"Two");
    sui_ctrl_tabs_add_item(host, 83, L"Three");

    {
        float ix = 0.0f, iw = 0.0f;
        int mx;
        sui_ctrl_tabs_item_rect(host, tabs, 1, &ix, &iw);
        mx = (int)(ix + iw * 0.5f);
        sui_ctrl_on_lbutton_down(host, mx, 30);
        sui_ctrl_on_lbutton_up(host, mx, 30);
    }

    ASSERT_EQ(1, tabs->d.tabs.selected);
    ASSERT_EQ(1, tabs->d.tabs.content_anim_active);
    ASSERT_EQ(1, tabs->d.tabs.content_anim_dir);
    ASSERT_NEAR(43.2f, sui_ctrl_tabs_content_new_offset(tabs), 0.0001f);
    ASSERT_NEAR(0.0f, sui_ctrl_tabs_content_old_offset(tabs), 0.0001f);
    ASSERT_NEAR(0.0f, sui_ctrl_tabs_content_new_opacity(tabs), 0.0001f);
    ASSERT_NEAR(1.0f, sui_ctrl_tabs_content_old_opacity(tabs), 0.0001f);
    ASSERT_NEAR(1.0f, sui_ctrl_tabs_content_opacity_for(tabs, 2), 0.0001f);
    ASSERT_NEAR(0.0f, sui_ctrl_tabs_content_opacity_for(tabs, 1), 0.0001f);
    ASSERT_EQ(1, sui_ctrl_tabs_content_tick(tabs, 0.5f));
    {
        float p = sui_ctrl_tabs_content_progress(tabs);
        ASSERT_NEAR(43.2f * (1.0f - p), sui_ctrl_tabs_content_new_offset(tabs), 0.0001f);
        ASSERT_NEAR(-43.2f * p, sui_ctrl_tabs_content_old_offset(tabs), 0.0001f);
        ASSERT_NEAR(p, sui_ctrl_tabs_content_new_opacity(tabs), 0.0001f);
        ASSERT_NEAR(1.0f - p, sui_ctrl_tabs_content_old_opacity(tabs), 0.0001f);
    }
}

UTEST_F(host_fx, tabs_content_transition_key_right_enters_from_left) {
    SuiControlHost* host = &utest_fixture->host;
    int idx = sui_ctrl_add_tabs(host, 84, 10, 20, 240, 120, 34, 1);
    SuiControl* tabs = &host->controls[idx];

    sui_ctrl_tabs_add_item(host, 84, L"One");
    sui_ctrl_tabs_add_item(host, 84, L"Two");
    sui_ctrl_tabs_add_item(host, 84, L"Three");

    sui_ctrl_set_focus(host, idx);
    sui_ctrl_on_keydown(host, VK_RIGHT);

    ASSERT_EQ(2, tabs->d.tabs.selected);
    ASSERT_EQ(1, tabs->d.tabs.content_anim_active);
    ASSERT_EQ(-1, tabs->d.tabs.content_anim_dir);
    ASSERT_NEAR(-43.2f, sui_ctrl_tabs_content_new_offset(tabs), 0.0001f);
    ASSERT_NEAR(0.0f, sui_ctrl_tabs_content_old_offset(tabs), 0.0001f);
    ASSERT_NEAR(0.0f, sui_ctrl_tabs_content_new_opacity(tabs), 0.0001f);
    ASSERT_NEAR(1.0f, sui_ctrl_tabs_content_old_opacity(tabs), 0.0001f);
    ASSERT_EQ(1, sui_ctrl_tabs_content_tick(tabs, 0.5f));
    {
        float p = sui_ctrl_tabs_content_progress(tabs);
        ASSERT_NEAR(-43.2f * (1.0f - p), sui_ctrl_tabs_content_new_offset(tabs), 0.0001f);
        ASSERT_NEAR(43.2f * p, sui_ctrl_tabs_content_old_offset(tabs), 0.0001f);
        ASSERT_NEAR(p, sui_ctrl_tabs_content_new_opacity(tabs), 0.0001f);
        ASSERT_NEAR(1.0f - p, sui_ctrl_tabs_content_old_opacity(tabs), 0.0001f);
    }
}

UTEST_F(host_fx, tabs_vertical_content_transition_moves_and_fades_by_direction) {
    SuiControlHost* host = &utest_fixture->host;
    int idx = sui_ctrl_add_tabs(host, 87, 10, 20, 240, 300, 34, 0);
    SuiControl* tabs = &host->controls[idx];

    tabs->d.tabs.content_anim_active = 1;
    tabs->d.tabs.content_anim_dir = 1;
    tabs->d.tabs.content_anim_vertical = 1;
    tabs->d.tabs.content_anim_t = 0.0f;
    tabs->d.tabs.content_anim_travel = 24.0f;

    ASSERT_NEAR(0.0f, sui_ctrl_tabs_content_new_offset(tabs), 0.0001f);
    ASSERT_NEAR(0.0f, sui_ctrl_tabs_content_old_offset(tabs), 0.0001f);
    ASSERT_NEAR(24.0f, sui_ctrl_tabs_content_new_offset_y(tabs), 0.0001f);
    ASSERT_NEAR(0.0f, sui_ctrl_tabs_content_old_offset_y(tabs), 0.0001f);
    ASSERT_NEAR(0.0f, sui_ctrl_tabs_content_new_opacity(tabs), 0.0001f);
    ASSERT_NEAR(1.0f, sui_ctrl_tabs_content_old_opacity(tabs), 0.0001f);

    ASSERT_EQ(1, sui_ctrl_tabs_content_tick(tabs, 0.5f));
    {
        float p = sui_ctrl_tabs_content_progress(tabs);
        ASSERT_NEAR(24.0f * (1.0f - p), sui_ctrl_tabs_content_new_offset_y(tabs), 0.0001f);
        ASSERT_NEAR(-24.0f * p, sui_ctrl_tabs_content_old_offset_y(tabs), 0.0001f);
        ASSERT_NEAR(0.5f, p, 0.0001f);
        ASSERT_NEAR(0.25f, sui_ctrl_tabs_content_new_opacity(tabs), 0.0001f);
        ASSERT_NEAR(1.0f - 0.5f / 0.70f, sui_ctrl_tabs_content_old_opacity(tabs), 0.0001f);
    }

    tabs->d.tabs.content_anim_t = 0.70f;
    ASSERT_NEAR(0.49f, sui_ctrl_tabs_content_new_opacity(tabs), 0.0001f);
    ASSERT_NEAR(0.0f, sui_ctrl_tabs_content_old_opacity(tabs), 0.0001f);

    tabs->d.tabs.content_anim_t = 0.99f;
    ASSERT_NEAR(0.9801f, sui_ctrl_tabs_content_new_opacity(tabs), 0.0001f);

    tabs->d.tabs.content_anim_t = 1.0f;
    ASSERT_NEAR(1.0f, sui_ctrl_tabs_content_new_opacity(tabs), 0.0001f);
}

UTEST_F(host_fx, tabs_content_transition_survives_page_rebuild) {
    SuiControlHost* host = &utest_fixture->host;
    int idx = sui_ctrl_add_tabs(host, 85, 10, 20, 240, 120, 34, 0);
    SuiControl* tabs = &host->controls[idx];

    tabs->d.tabs.content_anim_active = 1;
    tabs->d.tabs.content_anim_dir = 1;
    tabs->d.tabs.content_anim_vertical = 1;
    tabs->d.tabs.content_anim_t = 0.25f;
    tabs->d.tabs.content_anim_travel = 24.0f;

    {
        float offset_before_rebuild = sui_ctrl_tabs_content_new_offset_y(tabs);

        sui_ctrl_clear_page(host);
        idx = sui_ctrl_add_tabs(host, 85, 10, 20, 240, 120, 34, 1);
        tabs = &host->controls[idx];

        ASSERT_EQ(1, tabs->d.tabs.content_anim_active);
        ASSERT_EQ(1, tabs->d.tabs.content_anim_dir);
        ASSERT_EQ(1, tabs->d.tabs.content_anim_vertical);
        ASSERT_NEAR(0.25f, tabs->d.tabs.content_anim_t, 0.0001f);
        ASSERT_NEAR(24.0f, tabs->d.tabs.content_anim_travel, 0.0001f);
        ASSERT_NEAR(0.0f, sui_ctrl_tabs_content_new_offset(tabs), 0.0001f);
        ASSERT_NEAR(offset_before_rebuild,
                    sui_ctrl_tabs_content_new_offset_y(tabs), 0.0001f);
    }
}

UTEST_F(host_fx, tabs_active_transition_detection_survives_page_rebuild) {
    SuiControlHost* host = &utest_fixture->host;
    int idx = sui_ctrl_add_tabs(host, 86, 10, 20, 240, 120, 34, 0);
    SuiControl* tabs = &host->controls[idx];

    ASSERT_EQ(0, sui_ctrl_tabs_host_has_active_transition(host));
    tabs->d.tabs.content_anim_active = 1;
    tabs->d.tabs.content_anim_dir = -1;
    tabs->d.tabs.content_anim_t = 0.5f;
    ASSERT_EQ(1, sui_ctrl_tabs_host_has_active_transition(host));

    sui_ctrl_clear_page(host);
    ASSERT_EQ(1, sui_ctrl_tabs_host_has_active_transition(host));

    idx = sui_ctrl_add_tabs(host, 86, 10, 20, 240, 120, 34, 1);
    tabs = &host->controls[idx];
    ASSERT_EQ(1, sui_ctrl_tabs_host_has_active_transition(host));

    ASSERT_EQ(1, sui_ctrl_tabs_content_tick(tabs, 1.0f));
    ASSERT_EQ(0, sui_ctrl_tabs_host_has_active_transition(host));
}

UTEST_F(host_fx, tabs_arrow_key_changes_selected) {
    SuiControlHost* host = &utest_fixture->host;
    int idx = sui_ctrl_add_tabs(host, 80, 10, 20, 240, 120, 34, 0);
    SuiControl* tabs = &host->controls[idx];
    host->on_click = sui__test_on_click;
    sui__test_click_count = 0;

    sui_ctrl_tabs_add_item(host, 80, L"One");
    sui_ctrl_tabs_add_item(host, 80, L"Two");
    sui_ctrl_tabs_add_item(host, 80, L"Three");

    sui_ctrl_set_focus(host, idx);
    sui_ctrl_on_keydown(host, VK_RIGHT);

    ASSERT_EQ(1, tabs->d.tabs.selected);
    ASSERT_EQ(1, sui__test_click_count);
}

/* ---- Edit control cursor invariants ---------------------------------- */
/* These keydown tests don't depend on the renderer's dwrite factory —
 * sui_ctrl_edit_text_width returns 0 when factory is NULL, so scroll-to-
 * caret is a no-op and the cursor math is observable in isolation. */

UTEST_F(host_fx, edit_add_places_cursor_at_text_end) {
    int idx = sui_ctrl_add_edit(&utest_fixture->host, 1, 0, 0, 100, 24,
                                L"abc", NULL, 0);
    SuiControl* c = &utest_fixture->host.controls[idx];
    ASSERT_EQ(3, c->d.edit.cursor);
    ASSERT_EQ(-1, c->d.edit.sel_start);
    ASSERT_EQ(-1, c->d.edit.sel_end);
}

UTEST_F(host_fx, edit_long_initial_text_and_stale_indices_are_clamped) {
    wchar_t text[SUI_CTRL_TEXT_MAX * 2];
    SuiControlHost* host = &utest_fixture->host;
    int i;
    int idx;
    SuiControl* c;
    for (i = 0; i < (int)(sizeof(text) / sizeof(text[0])) - 1; ++i)
        text[i] = L'a';
    text[i] = L'\0';

    idx = sui_ctrl_add_edit(host, 71, 0, 0, 100, 24, text, NULL, 0);
    c = &host->controls[idx];
    ASSERT_EQ(SUI_CTRL_TEXT_MAX - 1, (int)wcslen(c->text));
    ASSERT_EQ(SUI_CTRL_TEXT_MAX - 1, c->d.edit.cursor);
    ASSERT_EQ(SUI_CTRL_TEXT_MAX - 1, c->d.edit.drag_sel_anchor);

    sui__wstrncpy_z(c->text, L"x", SUI_CTRL_TEXT_MAX);
    c->d.edit.cursor = 999;
    c->d.edit.sel_start = 500;
    c->d.edit.sel_end = 700;
    c->d.edit.drag_sel_anchor = 800;
    ASSERT_EQ(1, sui_ctrl_edit_on_char(host, idx, L'y'));
    ASSERT_EQ(0, wcscmp(c->text, L"xy"));
    ASSERT_EQ(2, c->d.edit.cursor);
    ASSERT_TRUE(c->d.edit.sel_start == -1 || c->d.edit.sel_start <= 2);
    ASSERT_TRUE(c->d.edit.sel_end == -1 || c->d.edit.sel_end <= 2);
    ASSERT_TRUE(c->d.edit.drag_sel_anchor <= 2);
    sui_ctrl_edit_release(c);
}

UTEST_F(host_fx, edit_vk_left_decrements_cursor) {
    int idx = sui_ctrl_add_edit(&utest_fixture->host, 1, 0, 0, 100, 24,
                                L"abc", NULL, 0);
    SuiControl* c = &utest_fixture->host.controls[idx];
    sui_ctrl_edit_on_keydown(&utest_fixture->host, idx, VK_LEFT);
    ASSERT_EQ(2, c->d.edit.cursor);
    sui_ctrl_edit_on_keydown(&utest_fixture->host, idx, VK_LEFT);
    ASSERT_EQ(1, c->d.edit.cursor);
}

UTEST_F(host_fx, edit_vk_left_clamps_at_zero) {
    int idx = sui_ctrl_add_edit(&utest_fixture->host, 1, 0, 0, 100, 24,
                                L"abc", NULL, 0);
    SuiControl* c = &utest_fixture->host.controls[idx];
    c->d.edit.cursor = 0;
    sui_ctrl_edit_on_keydown(&utest_fixture->host, idx, VK_LEFT);
    ASSERT_EQ(0, c->d.edit.cursor);
}

UTEST_F(host_fx, edit_vk_right_increments_cursor) {
    int idx = sui_ctrl_add_edit(&utest_fixture->host, 1, 0, 0, 100, 24,
                                L"abc", NULL, 0);
    SuiControl* c = &utest_fixture->host.controls[idx];
    c->d.edit.cursor = 0;
    sui_ctrl_edit_on_keydown(&utest_fixture->host, idx, VK_RIGHT);
    ASSERT_EQ(1, c->d.edit.cursor);
}

UTEST_F(host_fx, edit_vk_right_clamps_at_len) {
    int idx = sui_ctrl_add_edit(&utest_fixture->host, 1, 0, 0, 100, 24,
                                L"abc", NULL, 0);
    SuiControl* c = &utest_fixture->host.controls[idx];
    /* Already at end from add. */
    sui_ctrl_edit_on_keydown(&utest_fixture->host, idx, VK_RIGHT);
    ASSERT_EQ(3, c->d.edit.cursor);
}

UTEST_F(host_fx, edit_vk_home_jumps_to_zero) {
    int idx = sui_ctrl_add_edit(&utest_fixture->host, 1, 0, 0, 100, 24,
                                L"abcde", NULL, 0);
    SuiControl* c = &utest_fixture->host.controls[idx];
    sui_ctrl_edit_on_keydown(&utest_fixture->host, idx, VK_HOME);
    ASSERT_EQ(0, c->d.edit.cursor);
}

UTEST_F(host_fx, edit_vk_end_jumps_to_len) {
    int idx = sui_ctrl_add_edit(&utest_fixture->host, 1, 0, 0, 100, 24,
                                L"abcde", NULL, 0);
    SuiControl* c = &utest_fixture->host.controls[idx];
    c->d.edit.cursor = 0;
    sui_ctrl_edit_on_keydown(&utest_fixture->host, idx, VK_END);
    ASSERT_EQ(5, c->d.edit.cursor);
}

/* ---- Declarative API: HSTACK equal-share ----------------------------- */

UTEST_F(host_fx, decl_hstack_three_children_divides_width_equally) {
    SuiControlHost* host = &utest_fixture->host;
    RECT cr;
    SuiContainer* hs;
    float expected_child_w;
    int depth_before;

    /* Pin the client rect we'll be measuring against. */
    GetClientRect(host->hwnd, &cr);
    ASSERT_GT(cr.right, 0);
    ASSERT_GT(cr.bottom, 0);

    sui_begin(host, sui_inset(0.0f), 0.0f, SUI_CLEAR_PAGE);
    depth_before = host->container_depth;
    sui_hstack(host, 3, 0.0f);
    ASSERT_EQ(depth_before + 1, host->container_depth);

    hs = &host->container_stack[host->container_depth - 1];
    ASSERT_EQ((int)SUI_CONT_HSTACK, (int)hs->type);
    /* gap=0 → child_w = parent_w / 3 exactly. */
    expected_child_w = (float)cr.right / 3.0f;
    ASSERT_NEAR(expected_child_w, hs->child_w, 0.01f);
}

/* ---- Flex distribution: HSTACK ----------------------------------------- */

UTEST_F(host_fx, hstack_grow_distributes_leftover_proportionally) {
    /* Window 400x300 (from SETUP), inset=0, gap=0, DPI=96 (s=1).
     * Two empty-text labels with grow=1 each: pref_w=0 for both.
     * available = 400, leftover = 400 - 0 = 400.
     * Each child gets 400 * (1/2) = 200. */
    SuiControlHost* host = &utest_fixture->host;
    int i0, i1;

    sui_begin(host, sui_inset(0.0f), 0.0f, SUI_CLEAR_PAGE);
    sui_hstack(host, 2, 0.0f);

    i0 = sui_label(host, L"");
    host->controls[i0].grow = 1;

    i1 = sui_label(host, L"");
    host->controls[i1].grow = 1;

    sui_end(host); /* triggers sui__decl_distribute_hstack */

    ASSERT_NEAR(200.0f, host->controls[i0].w, 1.0f);
    ASSERT_NEAR(200.0f, host->controls[i1].w, 1.0f);
    ASSERT_NEAR(0.0f,   host->controls[i0].x, 1.0f);
    ASSERT_NEAR(200.0f, host->controls[i1].x, 1.0f);
}

UTEST_F(host_fx, hstack_grow_unequal_shares_leftover_by_ratio) {
    /* grow=1 and grow=3: total_grow=4.
     * leftover=400: c[0] gets 400*(1/4)=100, c[1] gets 400*(3/4)=300. */
    SuiControlHost* host = &utest_fixture->host;
    int i0, i1;

    sui_begin(host, sui_inset(0.0f), 0.0f, SUI_CLEAR_PAGE);
    sui_hstack(host, 2, 0.0f);

    i0 = sui_label(host, L"");
    host->controls[i0].grow = 1;

    i1 = sui_label(host, L"");
    host->controls[i1].grow = 3;

    sui_end(host);

    ASSERT_NEAR(100.0f, host->controls[i0].w, 1.0f);
    ASSERT_NEAR(300.0f, host->controls[i1].w, 1.0f);
    ASSERT_NEAR(0.0f,   host->controls[i0].x, 1.0f);
    ASSERT_NEAR(100.0f, host->controls[i1].x, 1.0f);
}

UTEST_F(host_fx, hstack_max_w_caps_growth) {
    /* Both grow=1, c[0].max_w=50. Distribution is single-pass:
     * c[0] raw=200 → capped to 50; c[1] raw=200 → stays 200.
     * Surplus from the cap is NOT redistributed (CSS flex analogy).
     * cursor: c[0].x=0, c[1].x=50 (= 50 + gap=0). */
    SuiControlHost* host = &utest_fixture->host;
    int i0, i1;

    sui_begin(host, sui_inset(0.0f), 0.0f, SUI_CLEAR_PAGE);
    sui_hstack(host, 2, 0.0f);

    i0 = sui_label(host, L"");
    host->controls[i0].grow  = 1;
    host->controls[i0].max_w = 50.0f;

    i1 = sui_label(host, L"");
    host->controls[i1].grow = 1;

    sui_end(host);

    ASSERT_NEAR(50.0f,  host->controls[i0].w, 1.0f);
    ASSERT_NEAR(200.0f, host->controls[i1].w, 1.0f);
    ASSERT_NEAR(0.0f,   host->controls[i0].x, 1.0f);
    ASSERT_NEAR(50.0f,  host->controls[i1].x, 1.0f);
}

/* ---- Flex distribution: VSTACK ----------------------------------------- */

UTEST_F(host_fx, vstack_grow_distributes_leftover_proportionally) {
    /* Window 400x300, inset=0, gap=0. Root.h=300 → vstack.h=300.
     * Two empty-text labels added via sui_label, grow=1 each.
     * sui_label routes through sui__decl_place, which increments both
     * flex_slot_count AND n_total_children — the distribute guard is
     * satisfied without manually patching struct state.
     * pref_h = max(pref_h=0, eff_h=SUI_DEFAULT_LABEL_H=18) = 18.
     * available = 300 - 0 = 300; total_pref = 36; leftover = 264.
     * Each child gets 18 + 264*(1/2) = 150. */
    SuiControlHost* host = &utest_fixture->host;
    int i0, i1;

    sui_begin(host, sui_inset(0.0f), 0.0f, SUI_CLEAR_PAGE);
    sui_vstack(host);

    i0 = sui_label(host, L"");
    host->controls[i0].grow = 1;

    i1 = sui_label(host, L"");
    host->controls[i1].grow = 1;

    sui_end(host);

    ASSERT_NEAR(150.0f, host->controls[i0].h, 1.0f);
    ASSERT_NEAR(150.0f, host->controls[i1].h, 1.0f);
    ASSERT_NEAR(0.0f,   host->controls[i0].y, 1.0f);
    ASSERT_NEAR(150.0f, host->controls[i1].y, 1.0f);
}

UTEST_F(host_fx, decl_begin_resets_control_count) {
    SuiControlHost* host = &utest_fixture->host;
    sui_ctrl_add_label(host, 1, 0, 0, 100, 20, L"junk");
    ASSERT_EQ(1, host->control_count);
    sui_begin(host, sui_inset(0.0f), 0.0f, SUI_CLEAR_PAGE);
    ASSERT_EQ(0, host->control_count);
}

UTEST_F(host_fx, decl_begin_pushes_root_container) {
    SuiControlHost* host = &utest_fixture->host;
    SuiContainer* root;
    sui_begin(host, sui_inset(8.0f), 4.0f, SUI_CLEAR_PAGE);
    ASSERT_EQ(1, host->container_depth);
    root = &host->container_stack[0];
    ASSERT_EQ((int)SUI_CONT_ROOT, (int)root->type);
    /* symmetric inset: all four pad fields equal */
    ASSERT_NEAR(8.0f, root->pad_l, 0.0001f);
    ASSERT_NEAR(8.0f, root->pad_t, 0.0001f);
    ASSERT_NEAR(8.0f, root->pad_r, 0.0001f);
    ASSERT_NEAR(8.0f, root->pad_b, 0.0001f);
    ASSERT_NEAR(4.0f, root->gap,   0.0001f);
}

UTEST_F(host_fx, begin_clear_page_routes_to_ctrl_clear_page) {
    SuiControlHost* host = &utest_fixture->host;
    sui_ctrl_add_label(host, 1, 0, 0, 100, 20, L"persist");
    host->persistent_control_count = 1;
    sui_ctrl_add_label(host, 2, 0, 0, 100, 20, L"transient");
    ASSERT_EQ(2, host->control_count);
    sui_begin(host, sui_inset(0.0f), 0.0f, SUI_CLEAR_PAGE);
    ASSERT_EQ(1, host->control_count); /* transient erased; persistent kept */
}

UTEST_F(host_fx, begin_clear_none_preserves_control_count) {
    SuiControlHost* host = &utest_fixture->host;
    sui_ctrl_add_label(host, 1, 0, 0, 100, 20, L"a");
    sui_ctrl_add_label(host, 2, 0, 0, 100, 20, L"b");
    ASSERT_EQ(2, host->control_count);
    sui_begin(host, sui_inset(0.0f), 0.0f, SUI_CLEAR_NONE);
    ASSERT_EQ(2, host->control_count);
}

UTEST_F(host_fx, begin_asymmetric_inset_sets_root_pads) {
    /* DPI=96 → s=1.0, so DIP values pass through unchanged */
    SuiControlHost* host = &utest_fixture->host;
    SuiRect inset = {5.0f, 10.0f, 15.0f, 20.0f};
    SuiContainer* root;
    sui_begin(host, inset, 0.0f, SUI_CLEAR_PAGE);
    root = &host->container_stack[0];
    ASSERT_NEAR(5.0f,  root->pad_l, 0.0001f);
    ASSERT_NEAR(10.0f, root->pad_t, 0.0001f);
    ASSERT_NEAR(15.0f, root->pad_r, 0.0001f);
    ASSERT_NEAR(20.0f, root->pad_b, 0.0001f);
    ASSERT_NEAR(5.0f,  root->x,     0.0001f); /* x = pad_l */
    ASSERT_NEAR(10.0f, root->y,     0.0001f); /* y = pad_t */
}

UTEST_F(host_fx, begin_px_skips_dpi_scaling) {
    SuiControlHost* host = &utest_fixture->host;
    SuiRect rect_px = {10.0f, 20.0f, 210.0f, 120.0f};
    SuiContainer* root = sui_begin_px(host, rect_px, 8.0f, SUI_CLEAR_PAGE);
    ASSERT_TRUE(root != NULL);
    ASSERT_NEAR(10.0f,  root->x,     0.0001f);
    ASSERT_NEAR(20.0f,  root->y,     0.0001f);
    ASSERT_NEAR(200.0f, root->w,     0.0001f); /* right - left */
    ASSERT_NEAR(100.0f, root->h,     0.0001f); /* bottom - top */
    ASSERT_NEAR(8.0f,   root->gap,   0.0001f);
    ASSERT_NEAR(0.0f,   root->pad_l, 0.0001f); /* no pad in px variant */
    ASSERT_NEAR(0.0f,   root->pad_r, 0.0001f);
}

/* =========================================================================
 * Phase-1 (safety) — surrogate-pair cursor, numeric_only, NULL guards.
 * ========================================================================= */

/* U+1F600 GRINNING FACE — UTF-16 surrogate pair: 0xD83D 0xDE00.
 * Layout: [hi][lo][a][b][c][\0]  → cursor positions 0, 2, 3, 4, 5 are
 * valid (codepoint boundaries); positions 1 (between halves) is NOT. */
static void sui__set_edit_text_with_surrogate(SuiControl* c) {
    c->text[0] = (wchar_t)0xD83D;
    c->text[1] = (wchar_t)0xDE00;
    c->text[2] = L'a';
    c->text[3] = L'b';
    c->text[4] = L'c';
    c->text[5] = L'\0';
}

UTEST_F(host_fx, edit_vk_left_skips_low_surrogate_half) {
    SuiControlHost* host = &utest_fixture->host;
    int idx = sui_ctrl_add_edit(host, 1, 0, 0, 100, 24, L"", NULL, 0);
    SuiControl* c = &host->controls[idx];
    sui__set_edit_text_with_surrogate(c);
    c->d.edit.cursor = 5;
    /* "abc" backwards three times → cursor at 2 (just past the pair). */
    sui_ctrl_edit_on_keydown(host, idx, VK_LEFT);
    sui_ctrl_edit_on_keydown(host, idx, VK_LEFT);
    sui_ctrl_edit_on_keydown(host, idx, VK_LEFT);
    ASSERT_EQ(2, c->d.edit.cursor);
    /* One more VK_LEFT must skip BOTH halves of the pair, landing at 0,
     * not at 1 (which would be between high and low surrogate). */
    sui_ctrl_edit_on_keydown(host, idx, VK_LEFT);
    ASSERT_EQ(0, c->d.edit.cursor);
}

UTEST_F(host_fx, edit_vk_right_skips_high_surrogate_half) {
    SuiControlHost* host = &utest_fixture->host;
    int idx = sui_ctrl_add_edit(host, 1, 0, 0, 100, 24, L"", NULL, 0);
    SuiControl* c = &host->controls[idx];
    sui__set_edit_text_with_surrogate(c);
    c->d.edit.cursor = 0;
    /* One VK_RIGHT must advance past BOTH surrogate halves to 2, not
     * stop at 1 (between high and low). */
    sui_ctrl_edit_on_keydown(host, idx, VK_RIGHT);
    ASSERT_EQ(2, c->d.edit.cursor);
}

UTEST_F(host_fx, edit_vk_back_removes_full_surrogate_pair) {
    SuiControlHost* host = &utest_fixture->host;
    int idx = sui_ctrl_add_edit(host, 1, 0, 0, 100, 24, L"", NULL, 0);
    SuiControl* c = &host->controls[idx];
    sui__set_edit_text_with_surrogate(c);
    c->d.edit.cursor = 2; /* just past the pair */
    sui_ctrl_edit_on_keydown(host, idx, VK_BACK);
    /* Both surrogate halves should be deleted, leaving "abc". */
    ASSERT_EQ(0, c->d.edit.cursor);
    ASSERT_EQ((int)wcslen(c->text), 3);
    ASSERT_EQ(L'a', c->text[0]);
}

UTEST_F(host_fx, edit_vk_delete_removes_full_surrogate_pair) {
    SuiControlHost* host = &utest_fixture->host;
    int idx = sui_ctrl_add_edit(host, 1, 0, 0, 100, 24, L"", NULL, 0);
    SuiControl* c = &host->controls[idx];
    sui__set_edit_text_with_surrogate(c);
    c->d.edit.cursor = 0;
    sui_ctrl_edit_on_keydown(host, idx, VK_DELETE);
    /* Both surrogate halves should be deleted, leaving "abc". */
    ASSERT_EQ(0, c->d.edit.cursor);
    ASSERT_EQ((int)wcslen(c->text), 3);
    ASSERT_EQ(L'a', c->text[0]);
}

UTEST_F(host_fx, edit_numeric_only_accepts_first_decimal_point) {
    SuiControlHost* host = &utest_fixture->host;
    int idx = sui_ctrl_add_edit(host, 1, 0, 0, 100, 24, L"12", NULL, 1);
    SuiControl* c = &host->controls[idx];
    int accepted = sui_ctrl_edit_on_char(host, idx, L'.');
    ASSERT_TRUE(accepted);
    ASSERT_EQ(3, (int)wcslen(c->text));
    ASSERT_EQ(L'.', c->text[2]);
}

UTEST_F(host_fx, edit_numeric_only_rejects_second_decimal_point) {
    SuiControlHost* host = &utest_fixture->host;
    int idx = sui_ctrl_add_edit(host, 1, 0, 0, 100, 24, L"1.2", NULL, 1);
    SuiControl* c = &host->controls[idx];
    int len_before = (int)wcslen(c->text);
    int accepted = sui_ctrl_edit_on_char(host, idx, L'.');
    ASSERT_FALSE(accepted);
    ASSERT_EQ(len_before, (int)wcslen(c->text));
}

static void sui__set_textview_text_with_surrogate(SuiControl* c) {
    c->d.textview.buf[0] = (wchar_t)0xD83D;
    c->d.textview.buf[1] = (wchar_t)0xDE00;
    c->d.textview.buf[2] = L'a';
    c->d.textview.buf[3] = L'b';
    c->d.textview.buf[4] = L'c';
    c->d.textview.buf[5] = L'\0';
    c->d.textview.len = 5;
}

UTEST_F(host_fx, textview_vk_left_skips_low_surrogate_half) {
    SuiControlHost* host = &utest_fixture->host;
    int idx = sui_ctrl_add_textview(host, 1, 0, 0, 100, 100, NULL, 32, 0);
    SuiControl* c = &host->controls[idx];
    sui__set_textview_text_with_surrogate(c);
    c->d.textview.cursor = 2; /* just past the surrogate pair */
    sui_ctrl_textview_on_keydown(host, idx, VK_LEFT);
    ASSERT_EQ(0, c->d.textview.cursor); /* both halves skipped */
}

UTEST_F(host_fx, textview_vk_right_skips_high_surrogate_half) {
    SuiControlHost* host = &utest_fixture->host;
    int idx = sui_ctrl_add_textview(host, 1, 0, 0, 100, 100, NULL, 32, 0);
    SuiControl* c = &host->controls[idx];
    sui__set_textview_text_with_surrogate(c);
    c->d.textview.cursor = 0;
    sui_ctrl_textview_on_keydown(host, idx, VK_RIGHT);
    ASSERT_EQ(2, c->d.textview.cursor); /* both halves skipped */
}

/* =========================================================================
 * Phase-2 (UIA) — now-live tests (implementations shipped in Phase 2).
 * ========================================================================= */

UTEST_F(host_fx, control_provider_qi_returns_fragment_interface) {
    /* Create a control provider and verify QI for IRawElementProviderFragment. */
    GUID frag_iid = SUI_IID_IRawElementProviderFragment;
    SuiUiaProvider* prov;
    void* frag = NULL;
    HRESULT hr;
    typedef ULONG (STDMETHODCALLTYPE *RelFn)(void*);

    sui_ctrl_add_label(&utest_fixture->host, 1, 0, 0, 100, 20, L"ok");
    prov = (SuiUiaProvider*)sui_uia_create_control_provider(&utest_fixture->host, 0);
    ASSERT_NE((void*)NULL, (void*)prov);
    ASSERT_NE((void*)NULL, (const void*)prov->lpFragVtbl);

    hr = prov->lpVtbl->QueryInterface(prov, &frag_iid, &frag);
    ASSERT_EQ((HRESULT)S_OK, hr);
    ASSERT_NE((void*)NULL, frag);
    /* QI must return &p->lpFragVtbl (the fragment vtable pointer lives inside SuiUiaProvider) */
    ASSERT_EQ((const void*)&prov->lpFragVtbl, frag);

    /* Release QI AddRef via Fragment vtable slot 2 (drops ref to 1) */
    { void** v = *(void***)frag; ((RelFn)v[2])(frag); }
    /* Release creation ref via Simple vtable slot 2 (drops to 0, frees) */
    { void** v = *(void***)prov; ((RelFn)v[2])(prov); }
}

UTEST_F(host_fx, fragment_lookup_returns_callable_fragment_and_root_id_matches_windows) {
    SuiControlHost* host = &utest_fixture->host;
    SuiUiaProvider* provider;
    void* fragment;
    const SuiUiaProviderFragmentVtbl* vtbl;
    SAFEARRAY* runtime_id = NULL;
    ASSERT_EQ(-25, SUI_UIA_RootObjectId);
    sui_ctrl_add_button(host, 41, 0, 0, 100, 30, L"Action", 0);
    fragment = sui_uia_get_ctrl_provider(host, 0);
    provider = (SuiUiaProvider*)host->uia_providers[0];
    ASSERT_NE((void*)NULL, fragment);
    ASSERT_EQ((void*)&provider->lpFragVtbl, fragment);
    vtbl = *(const SuiUiaProviderFragmentVtbl**)fragment;
    ASSERT_EQ((HRESULT)S_OK, vtbl->GetRuntimeId(fragment, &runtime_id));
    ASSERT_NE((void*)NULL, runtime_id);
    SafeArrayDestroy(runtime_id);
    vtbl->Release(fragment);
}

UTEST_F(host_fx, uia_reports_enabled_state_and_full_textview_content) {
    SuiControlHost* host = &utest_fixture->host;
    SuiUiaProvider* provider;
    VARIANT value;
    const wchar_t* content = L"Readable observation details";
    int idx = sui_ctrl_add_textview(host, 41, 0, 0, 200, 100, content, 256, 0);
    provider = (SuiUiaProvider*)sui_uia_create_control_provider(host, idx);
    ASSERT_EQ((HRESULT)S_OK, provider->lpVtbl->GetPropertyValue(provider, SUI_UIA_IsEnabledPropertyId, &value));
    ASSERT_EQ(VT_BOOL, value.vt);
    ASSERT_EQ(VARIANT_TRUE, value.boolVal);
    host->controls[idx].state |= SUI_CTRL_DISABLED;
    ASSERT_EQ((HRESULT)S_OK, provider->lpVtbl->GetPropertyValue(provider, SUI_UIA_IsEnabledPropertyId, &value));
    ASSERT_EQ(VARIANT_FALSE, value.boolVal);
    ASSERT_EQ((HRESULT)S_OK, provider->lpVtbl->GetPropertyValue(provider, SUI_UIA_NamePropertyId, &value));
    ASSERT_EQ(VT_BSTR, value.vt);
    ASSERT_EQ(0, lstrcmpW(content, value.bstrVal));
    VariantClear(&value);
    provider->lpVtbl->Release(provider);
}

UTEST_F(host_fx, clear_page_nullifies_provider_slots_past_persistent) {
    /* Verify clear_page nulls transient slots while preserving persistent ones.
     * Arrangement: idx 0 = persistent, idx 1 = transient. */
    SuiControlHost* host = &utest_fixture->host;
    void* prov0;
    void* prov1;
    typedef ULONG (STDMETHODCALLTYPE *RelFn)(void*);

    sui_ctrl_add_label(host, 1, 0, 0, 100, 20, L"persist");
    host->persistent_control_count = 1;
    sui_ctrl_add_label(host, 2, 0, 0, 100, 20, L"transient");
    ASSERT_EQ(2, host->control_count);

    prov0 = sui_uia_create_control_provider(host, 0);
    prov1 = sui_uia_create_control_provider(host, 1);
    ASSERT_NE((void*)NULL, prov0);
    ASSERT_NE((void*)NULL, prov1);
    host->uia_providers[0] = prov0;
    host->uia_providers[1] = prov1;

    sui_ctrl_clear_page(host);
    ASSERT_NE((void*)NULL, host->uia_providers[0]); /* persistent kept */
    ASSERT_EQ((void*)NULL, host->uia_providers[1]); /* transient cleared */
    ASSERT_EQ(1, host->control_count);

    /* cleanup the surviving persistent provider */
    { void** v = *(void***)prov0; ((RelFn)v[2])(prov0); }
    host->uia_providers[0] = NULL;
}

/* =========================================================================
 * Phase-2 (UIA) remaining SKIPs — require live UIA event subscriber or
 * are structural wiring verified by code review rather than unit test.
 * ========================================================================= */

UTEST(phase2_uia, wm_getobject_dispatch_wired_in_demo_and_consumers) {
    UTEST_SKIP("Phase 2 (UIA rewrite): demo and consumer WndProcs must "
               "dispatch WM_GETOBJECT to UiaReturnRawElementProvider for the "
               "host provider; currently dead-on-arrival in production.");
}

UTEST_F(host_fx, host_release_detaches_retained_uia_providers) {
    SuiControlHost* host = &utest_fixture->host;
    SuiUiaProvider* ctrl_provider;
    SuiUiaHostProviderFull* host_provider;
    VARIANT value;
    int options = 0;
    HRESULT hr;

    sui_ctrl_add_label(host, 1, 0, 0, 100, 20, L"retained");
    ctrl_provider = (SuiUiaProvider*)sui_uia_create_control_provider(host, 0);
    host_provider = (SuiUiaHostProviderFull*)sui_uia_create_host_provider(host);
    ASSERT_NE((void*)NULL, ctrl_provider);
    ASSERT_NE((void*)NULL, host_provider);
    host->uia_providers[0] = ctrl_provider;
    host->uia_host_provider = host_provider;

    /* Simulate references retained by UI Automation beyond host teardown. */
    ctrl_provider->lpVtbl->AddRef(ctrl_provider);
    host_provider->lpVtbl->AddRef(host_provider);
    ASSERT_EQ((HRESULT)S_OK,
              ctrl_provider->lpVtbl->get_ProviderOptions(ctrl_provider, &options));
    ASSERT_EQ(SUI_UIA_ProviderOptions_ServerSideProvider |
                  SUI_UIA_ProviderOptions_UseComThreading,
              options);

    sui_ctrl_host_release(host);
    ASSERT_EQ((void*)NULL, host->uia_providers[0]);
    ASSERT_EQ((void*)NULL, host->uia_host_provider);
    ASSERT_EQ((void*)NULL, ctrl_provider->host);
    ASSERT_EQ((void*)NULL, host_provider->host);

    hr = ctrl_provider->lpVtbl->GetPropertyValue(
        ctrl_provider, SUI_UIA_NamePropertyId, &value);
    ASSERT_EQ(SUI_UIA_E_ELEMENTNOTAVAILABLE, hr);
    hr = host_provider->lpVtbl->GetPropertyValue(
        host_provider, SUI_UIA_IsControlElementPropertyId, &value);
    ASSERT_EQ(SUI_UIA_E_ELEMENTNOTAVAILABLE, hr);

    ctrl_provider->lpVtbl->Release(ctrl_provider);
    host_provider->lpVtbl->Release(host_provider);
}

UTEST(phase2_uia, focus_change_fires_property_changed_event) {
    UTEST_SKIP("Phase 2 (UIA rewrite): when sui_ctrl_set_focus changes the "
               "focused control, UiaRaiseAutomationPropertyChangedEvent must "
               "be called for UIA_HasKeyboardFocusPropertyId on both the "
               "losing and gaining providers (if loaded).");
}

UTEST(phase2_uia, value_change_fires_property_changed_event) {
    UTEST_SKIP("Phase 2 (UIA rewrite): edit/spinner/slider value commits must "
               "call UiaRaiseAutomationPropertyChangedEvent for "
               "UIA_ValueValuePropertyId / UIA_RangeValueValuePropertyId so "
               "screen-reader live regions update without polling.");
}

/* =========================================================================
 * Phase-7b (undo/redo stack) — promoted from SKIP.
 * ========================================================================= */

UTEST_F(host_fx, edit_ctrl_y_redoes_last_undone_edit) {
    SuiControlHost* host = &utest_fixture->host;
    int idx = sui_ctrl_add_edit(host, 1, 0, 0, 200, 30, L"", NULL, 0);
    SuiControl* c = &host->controls[idx];

    /* Save "" then change to "abc" and save that too. */
    sui_ctrl_edit_save_undo(c);
    wcsncpy(c->text, L"abc", 4); c->text[3] = L'\0'; c->d.edit.cursor = 3;
    sui_ctrl_edit_save_undo(c);
    /* Change to "xyz" without saving — simulates user editing. */
    wcsncpy(c->text, L"xyz", 4); c->text[3] = L'\0'; c->d.edit.cursor = 3;

    /* Undo must restore "abc". */
    sui_ctrl_edit_undo(c);
    ASSERT_EQ(0, wcscmp(c->text, L"abc"));
    ASSERT_EQ(3, c->d.edit.cursor);

    /* Redo must restore "xyz". */
    sui_ctrl_edit_redo(c);
    ASSERT_EQ(0, wcscmp(c->text, L"xyz"));
    ASSERT_EQ(3, c->d.edit.cursor);

    sui_ctrl_edit_release(c);
}

UTEST_F(host_fx, textview_ctrl_y_redoes_last_undone_edit) {
    SuiControlHost* host = &utest_fixture->host;
    int idx = sui_ctrl_add_textview(host, 1, 0, 0, 200, 100, NULL, 64, 0);
    SuiControl* c = &host->controls[idx];

    /* Set buf to "hello" then save undo. */
    sui__wmove(c->d.textview.buf, L"hello", 5);
    c->d.textview.buf[5] = L'\0';
    c->d.textview.len    = 5;
    c->d.textview.cursor = 5;
    sui__textview_save_undo(c);

    /* Change buf to "world" without saving. */
    sui__wmove(c->d.textview.buf, L"world", 5);
    c->d.textview.buf[5] = L'\0';
    c->d.textview.len    = 5;
    c->d.textview.cursor = 3;

    /* Undo must restore "hello". */
    sui__textview_undo(c);
    ASSERT_EQ(0, wcscmp(c->d.textview.buf, L"hello"));
    ASSERT_EQ(5, c->d.textview.cursor);

    /* Redo must restore "world". */
    sui__textview_redo(c);
    ASSERT_EQ(0, wcscmp(c->d.textview.buf, L"world"));
    ASSERT_EQ(3, c->d.textview.cursor);

    sui_ctrl_textview_release(c);
}

UTEST_F(host_fx, edit_save_undo_clears_redo_stack) {
    SuiControlHost* host = &utest_fixture->host;
    int idx = sui_ctrl_add_edit(host, 1, 0, 0, 200, 30, L"", NULL, 0);
    SuiControl* c = &host->controls[idx];

    /* Push a snapshot, change text, then undo — fills redo stack. */
    sui_ctrl_edit_save_undo(c);
    wcsncpy(c->text, L"abc", 4); c->text[3] = L'\0'; c->d.edit.cursor = 3;
    sui_ctrl_edit_undo(c);
    ASSERT_EQ(1, c->d.edit.redo_top);

    /* A new save must wipe the redo stack. */
    sui_ctrl_edit_save_undo(c);
    ASSERT_EQ(0, c->d.edit.redo_top);

    sui_ctrl_edit_release(c);
}

UTEST_F(host_fx, textview_save_undo_clears_redo_slot) {
    SuiControlHost* host = &utest_fixture->host;
    int idx = sui_ctrl_add_textview(host, 1, 0, 0, 200, 100, NULL, 64, 0);
    SuiControl* c = &host->controls[idx];

    /* Set buf, save undo, change, then undo — fills redo slot. */
    sui__wmove(c->d.textview.buf, L"hello", 5);
    c->d.textview.buf[5] = L'\0'; c->d.textview.len = 5; c->d.textview.cursor = 5;
    sui__textview_save_undo(c);
    sui__wmove(c->d.textview.buf, L"world", 5);
    c->d.textview.buf[5] = L'\0'; c->d.textview.len = 5; c->d.textview.cursor = 5;
    sui__textview_undo(c);
    ASSERT_NE(-1, c->d.textview.redo_len); /* redo holds "world" */

    /* A new save must wipe the redo slot. */
    sui__textview_save_undo(c);
    ASSERT_EQ(-1, c->d.textview.redo_len);

    sui_ctrl_textview_release(c);
}

/* =========================================================================
 * Phase-7c (named-param wrappers) — slider_p and spinner_p.
 * ========================================================================= */

UTEST_F(host_fx, slider_p_sets_correct_range_and_value) {
    SuiControlHost* host = &utest_fixture->host;
    SuiSliderParams p = { .min=10.0f, .max=200.0f, .value=75.0f, .step=5.0f };
    int idx = sui_ctrl_add_slider_p(host, 1, 0, 0, 200, 20, p);
    SuiControl* c = &host->controls[idx];
    ASSERT_EQ(1, idx >= 0);
    ASSERT_TRUE(c->d.slider.min   == 10.0f);
    ASSERT_TRUE(c->d.slider.max   == 200.0f);
    ASSERT_TRUE(c->d.slider.value == 75.0f);
    ASSERT_TRUE(c->d.slider.step  == 5.0f);
}

UTEST_F(host_fx, spinner_p_sets_correct_range_and_value) {
    SuiControlHost* host = &utest_fixture->host;
    SuiSpinnerParams p = { .min=1.0f, .max=999.0f, .value=42.0f, .step=1.0f };
    int idx = sui_ctrl_add_spinner_p(host, 1, 0, 0, 80, 28, p);
    SuiControl* c = &host->controls[idx];
    ASSERT_EQ(1, idx >= 0);
    ASSERT_TRUE(c->d.spinner.min   == 1.0f);
    ASSERT_TRUE(c->d.spinner.max   == 999.0f);
    ASSERT_TRUE(c->d.spinner.value == 42.0f);
    ASSERT_TRUE(c->d.spinner.step  == 1.0f);
}

/* =========================================================================
 * Phase-7d (allocator hook) — default allocator smoke tests.
 * The override path (SUI_MALLOC / SUI_FREE macros) is a compile-time
 * substitution and must be verified by defining the macros before the
 * include in a separate TU; these tests cover the default-path contract.
 * ========================================================================= */

UTEST(allocator, malloc_returns_nonnull_for_small_size) {
    void* p = sui_malloc(64);
    ASSERT_NE((void*)NULL, p);
    sui_free(p);
}

UTEST(allocator, free_null_does_not_crash) {
    sui_free(NULL); /* must be a no-op */
    ASSERT_TRUE(1); /* reached here without crash */
}

/* =========================================================================
 * Phase-3 (perf caches) — sui__fmt_cache_*, sui__layout_cache_*,
 * sui__text_hash.  Tests use fake pointer values since
 * the cache module never calls Release internally.
 * ========================================================================= */

UTEST(text_cache, format_cache_hit_on_same_key) {
    SuiTextFormatCache c;
    void *fake = (void*)(intptr_t)0xDEAD;
    void *displaced;
    memset(&c, 0, sizeof(c));
    displaced = sui__fmt_cache_set(&c, fake, L"Segoe UI", 14.0f, 400, 0, 0);
    ASSERT_EQ((void*)NULL, displaced); /* slot was empty */
    ASSERT_EQ(fake, sui__fmt_cache_lookup(&c, L"Segoe UI", 14.0f, 400, 0, 0));
}

UTEST(text_cache, format_cache_miss_on_different_family) {
    SuiTextFormatCache c;
    void *fake = (void*)(intptr_t)0xBEEF;
    memset(&c, 0, sizeof(c));
    sui__fmt_cache_set(&c, fake, L"Segoe UI", 14.0f, 400, 0, 0);
    ASSERT_EQ((void*)NULL, sui__fmt_cache_lookup(&c, L"Consolas", 14.0f, 400, 0, 0));
}

UTEST(text_cache, format_cache_miss_on_different_size_dip) {
    SuiTextFormatCache c;
    void *fake = (void*)(intptr_t)0xBEEF;
    memset(&c, 0, sizeof(c));
    sui__fmt_cache_set(&c, fake, L"Segoe UI", 14.0f, 400, 0, 0);
    ASSERT_EQ((void*)NULL, sui__fmt_cache_lookup(&c, L"Segoe UI", 16.0f, 400, 0, 0));
}

UTEST(text_cache, format_cache_miss_on_different_weight) {
    SuiTextFormatCache c;
    void *fake = (void*)(intptr_t)0xBEEF;
    memset(&c, 0, sizeof(c));
    sui__fmt_cache_set(&c, fake, L"Segoe UI", 14.0f, 400, 0, 0);
    ASSERT_EQ((void*)NULL, sui__fmt_cache_lookup(&c, L"Segoe UI", 14.0f, 700, 0, 0));
}

UTEST(text_cache, format_cache_miss_on_different_italic) {
    SuiTextFormatCache c;
    void *fake = (void*)(intptr_t)0xBEEF;
    memset(&c, 0, sizeof(c));
    sui__fmt_cache_set(&c, fake, L"Segoe UI", 14.0f, 400, 0, 0);
    ASSERT_EQ((void*)NULL, sui__fmt_cache_lookup(&c, L"Segoe UI", 14.0f, 400, 1, 0));
}

UTEST(text_cache, format_cache_miss_on_different_bold_title) {
    SuiTextFormatCache c;
    void *fake = (void*)(intptr_t)0xBEEF;
    memset(&c, 0, sizeof(c));
    sui__fmt_cache_set(&c, fake, L"Segoe UI", 14.0f, 400, 0, 0);
    ASSERT_EQ((void*)NULL, sui__fmt_cache_lookup(&c, L"Segoe UI", 14.0f, 400, 0, 1));
}

UTEST(text_cache, format_cache_set_returns_displaced_for_release) {
    SuiTextFormatCache c;
    void *fmtA = (void*)(intptr_t)0xA000;
    void *fmtB = (void*)(intptr_t)0xB000;
    void *d1, *d2;
    memset(&c, 0, sizeof(c));
    d1 = sui__fmt_cache_set(&c, fmtA, L"Segoe UI", 14.0f, 400, 0, 0);
    d2 = sui__fmt_cache_set(&c, fmtB, L"Consolas",  12.0f, 700, 0, 1);
    ASSERT_EQ((void*)NULL, d1); /* slot was empty — nothing to Release */
    ASSERT_EQ(fmtA, d2);        /* caller must Release fmtA */
}

UTEST(text_cache, layout_cache_lru_evicts_oldest_slot) {
    SuiTextLayoutCache c;
    void *ptrs[SUI_LAYOUT_CACHE_SLOTS + 1];
    void *displaced;
    int i;
    memset(&c, 0, sizeof(c));
    for (i = 0; i < SUI_LAYOUT_CACHE_SLOTS + 1; i++)
        ptrs[i] = (void*)(intptr_t)(0x1000 + i);
    /* Fill all 8 slots — each displaces nothing (empty). */
    for (i = 0; i < SUI_LAYOUT_CACHE_SLOTS; i++) {
        displaced = sui__layout_cache_store(&c, ptrs[i], (void*)(intptr_t)0xF0F,
                                             (UINT32)(i + 1), (i + 1),
                                             100.0f, 50.0f);
        ASSERT_EQ((void*)NULL, displaced);
    }
    /* 9th entry must evict slot 0 (lowest tick = 1). */
    displaced = sui__layout_cache_store(&c, ptrs[SUI_LAYOUT_CACHE_SLOTS],
                                         (void*)(intptr_t)0xF0F,
                                         (UINT32)(SUI_LAYOUT_CACHE_SLOTS + 1),
                                         (SUI_LAYOUT_CACHE_SLOTS + 1),
                                         100.0f, 50.0f);
    ASSERT_EQ(ptrs[0], displaced); /* oldest evicted */
}

UTEST(text_cache, layout_cache_drop_by_fmt_clears_matching_entries) {
    SuiTextLayoutCache c;
    void *fmtA = (void*)(intptr_t)0xAAA;
    void *fmtB = (void*)(intptr_t)0xBBB;
    void *lA0  = (void*)(intptr_t)0x100;
    void *lA1  = (void*)(intptr_t)0x101;
    void *lB0  = (void*)(intptr_t)0x200;
    void *dropped[SUI_LAYOUT_CACHE_SLOTS];
    int n;
    memset(&c, 0, sizeof(c));
    sui__layout_cache_store(&c, lA0, fmtA, 0x1u, 3, 100.0f, 50.0f);
    sui__layout_cache_store(&c, lB0, fmtB, 0x2u, 3, 100.0f, 50.0f);
    sui__layout_cache_store(&c, lA1, fmtA, 0x3u, 4, 200.0f, 50.0f);
    memset(dropped, 0, sizeof(dropped));
    n = sui__layout_cache_drop_by_fmt(&c, fmtA, dropped);
    ASSERT_EQ(2, n);
    /* Both fmtA entries cleared; fmtB entry survives. */
    ASSERT_EQ((void*)NULL, sui__layout_cache_lookup(&c, 0x1u, 3, fmtA, 100.0f, 50.0f));
    ASSERT_EQ((void*)NULL, sui__layout_cache_lookup(&c, 0x3u, 4, fmtA, 200.0f, 50.0f));
    ASSERT_EQ(lB0,         sui__layout_cache_lookup(&c, 0x2u, 3, fmtB, 100.0f, 50.0f));
    /* Caller would Release dropped[0] and dropped[1]. */
    ASSERT_NE((void*)NULL, dropped[0]);
    ASSERT_NE((void*)NULL, dropped[1]);
}

UTEST(phase3_perf, hit_test_4096_char_textview_under_1ms) {
    UTEST_SKIP("Phase 3 (perf caches): HitTestPoint on a 4096-char textview "
               "must complete in under 1ms at 200% DPI once the layout cache "
               "is wired into the textview hit-test path. Requires a live "
               "IDWriteFactory — cannot run headless.");
}

UTEST_F(host_fx, layout_cache_flush_nullifies_all_slots) {
    /* Seed fake layout pointers directly (out=NULL — no Release on fake ptrs). */
    int i;
    for (i = 0; i < SUI_LAYOUT_CACHE_SLOTS; i++) {
        utest_fixture->host.layout_cache.slots[i].layout   = (void*)(intptr_t)(0x100 + i);
        utest_fixture->host.layout_cache.slots[i].fmt      = (void*)(intptr_t)0xFFF;
        utest_fixture->host.layout_cache.slots[i].hash     = (UINT32)(i + 1);
        utest_fixture->host.layout_cache.slots[i].text_len = 3;
    }
    sui__flush_layout_cache(&utest_fixture->host.layout_cache, NULL);
    for (i = 0; i < SUI_LAYOUT_CACHE_SLOTS; i++)
        ASSERT_EQ((void*)NULL, utest_fixture->host.layout_cache.slots[i].layout);
}

UTEST(phase3_perf, format_cache_released_on_renderer_release) {
    UTEST_SKIP("Phase 3 (perf caches): sui_renderer_release must Release the "
               "cached IDWriteTextFormat*. Requires a live IDWriteFactory to "
               "verify refcount — cannot run headless.");
}

/* =========================================================================
 * Phase-5 (memory model) — sizeof probes + heap-release guards.
 * ========================================================================= */

UTEST(phase5_memory, sizeof_ctrl_after_union_shrink) {
    /* Phase 5a guard: nav.items and combo.items are now 8-B heap pointers.
     * The edit struct (~670 B) dominates the residual union. SuiControl must
     * be well under 2 KB; regression here means a fat inline field crept back. */
    ASSERT_LE((int)sizeof(SuiNavItem),  160);   /* 64+4+4 wchar_t + padding */
    ASSERT_LT((int)sizeof(SuiControl), 2048);   /* was ~5400 B pre-5a */
    ASSERT_GT((int)sizeof(SuiControlHost), (int)sizeof(SuiControl));
}

UTEST_F(host_fx, nav_release_frees_heap_items) {
    int idx = sui_ctrl_add_nav_list(&utest_fixture->host, 1, 0,0,100,300, 28.0f, 0);
    ASSERT_GE(idx, 0);
    ASSERT_NE((void*)NULL, (void*)utest_fixture->host.controls[idx].d.nav.items);
    sui_ctrl_nav_release(&utest_fixture->host.controls[idx]);
    ASSERT_EQ((void*)NULL, (void*)utest_fixture->host.controls[idx].d.nav.items);
    ASSERT_EQ(0, utest_fixture->host.controls[idx].d.nav.item_count);
}

UTEST_F(host_fx, combo_release_frees_heap_items) {
    const wchar_t* opts[] = { L"Alpha", L"Beta" };
    int idx = sui_ctrl_add_combobox(&utest_fixture->host, 2, 0,0,120,30, opts, 2, 0);
    ASSERT_GE(idx, 0);
    ASSERT_NE((void*)NULL, (void*)utest_fixture->host.controls[idx].d.combo.items);
    sui_ctrl_combo_release(&utest_fixture->host.controls[idx]);
    ASSERT_EQ((void*)NULL, (void*)utest_fixture->host.controls[idx].d.combo.items);
    ASSERT_EQ(0, utest_fixture->host.controls[idx].d.combo.item_count);
}

UTEST_F(host_fx, clear_page_frees_nav_items_for_non_persistent_slots) {
    int idx;
    utest_fixture->host.persistent_control_count = 0;
    idx = sui_ctrl_add_nav_list(&utest_fixture->host, 3, 0,0,100,300, 28.0f, 0);
    ASSERT_GE(idx, 0);
    ASSERT_NE((void*)NULL, (void*)utest_fixture->host.controls[idx].d.nav.items);
    sui_ctrl_clear_page(&utest_fixture->host);
    /* Slot was freed by clear_page; sui_ctrl_nav_release nulls the pointer. */
    ASSERT_EQ((void*)NULL, (void*)utest_fixture->host.controls[idx].d.nav.items);
}

UTEST_F(host_fx, host_release_frees_combo_items_for_all_slots) {
    UTEST_SKIP("Phase 5a: verify sui_ctrl_host_release frees combo items — "
               "skipped to avoid double-free conflict with host_fx teardown; "
               "cover with a standalone fixture or AddressSanitizer run.");
}

/* =========================================================================
 * Phase-7 (ergonomics) — sui_default_msg dispatcher.
 * ========================================================================= */

UTEST_F(host_fx, default_msg_unhandled_returns_zero) {
    LRESULT r = 0xDEAD;
    int handled = sui_default_msg(&utest_fixture->host, utest_fixture->hwnd,
                                  WM_NULL, 0, 0, &r);
    ASSERT_EQ(0, handled);
    ASSERT_EQ((LRESULT)0xDEAD, r); /* result untouched */
}

UTEST_F(host_fx, default_msg_erasebkgnd_returns_handled_and_true) {
    LRESULT r = 0;
    int handled = sui_default_msg(&utest_fixture->host, utest_fixture->hwnd,
                                  WM_ERASEBKGND, 0, 0, &r);
    ASSERT_EQ(1, handled);
    ASSERT_EQ((LRESULT)TRUE, r);
}

UTEST_F(host_fx, default_msg_mouseleave_clears_hover) {
    LRESULT r = 0;
    int handled;
    utest_fixture->host.hover_control = 0;
    handled = sui_default_msg(&utest_fixture->host, utest_fixture->hwnd,
                              WM_MOUSELEAVE, 0, 0, &r);
    ASSERT_EQ(1, handled);
    ASSERT_EQ(-1, utest_fixture->host.hover_control);
}

UTEST_F(host_fx, default_msg_setfocus_sets_focus_and_caret) {
    LRESULT r = 0;
    int handled;
    utest_fixture->host.window_has_focus = 0;
    utest_fixture->host.caret_visible    = 0;
    handled = sui_default_msg(&utest_fixture->host, utest_fixture->hwnd,
                              WM_SETFOCUS, 0, 0, &r);
    ASSERT_EQ(1, handled);
    ASSERT_EQ(1, utest_fixture->host.window_has_focus);
    ASSERT_EQ(1, utest_fixture->host.caret_visible);
}

UTEST_F(host_fx, default_msg_killfocus_clears_window_has_focus) {
    LRESULT r = 0;
    int handled;
    utest_fixture->host.window_has_focus = 1;
    handled = sui_default_msg(&utest_fixture->host, utest_fixture->hwnd,
                              WM_KILLFOCUS, 0, 0, &r);
    ASSERT_EQ(1, handled);
    ASSERT_EQ(0, utest_fixture->host.window_has_focus);
}

UTEST_F(host_fx, tabs_indicator_color_uses_scheme_accent_fallback) {
    SuiControlHost* host = &utest_fixture->host;
    SuiColorScheme scheme = *host->scheme;
    int idx = sui_ctrl_add_tabs(host, 306, 0, 0, 200, 80, 34, 0);
    SuiControl* c = &host->controls[idx];

    scheme.btn_default_border = 0x5E5E5Eu;
    host->scheme = &scheme;

    ASSERT_EQ((uint32_t)0x5E5E5Eu, sui_ctrl_tabs_indicator_color(host, c));

    sui_ctrl_clear_page(host);
}

UTEST_F(host_fx, default_msg_keydown_returns_handled) {
    LRESULT r = 0;
    int handled = sui_default_msg(&utest_fixture->host, utest_fixture->hwnd,
                                  WM_KEYDOWN, VK_TAB, 0, &r);
    ASSERT_EQ(1, handled);
    ASSERT_EQ((LRESULT)0, r);
}

/* =========================================================================
 * Safety bug fixes (post-GPT-5.5 review)
 * ========================================================================= */

/* P1 — textview add: successful call has non-null buffers.
 * Guards against regression where alloc failure returned idx instead of -1
 * (buf=NULL control left live in the array). */
UTEST_F(host_fx, add_textview_success_has_nonnull_buffers) {
    SuiControlHost* host = &utest_fixture->host;
    int before = host->control_count;
    int idx = sui_ctrl_add_textview(host, 9001, 0.0f, 0.0f, 200.0f, 80.0f,
                                    L"hello", 64, 0);
    ASSERT_GE(idx, 0);
    ASSERT_EQ(before + 1, host->control_count);
    ASSERT_TRUE(host->controls[idx].d.textview.buf      != NULL);
    ASSERT_TRUE(host->controls[idx].d.textview.undo_buf != NULL);
    ASSERT_TRUE(host->controls[idx].d.textview.redo_buf != NULL);
}

/* P2 — sui_begin_px must reset intrinsic_min_w/h (was missing unlike sui_begin).
 * Stale values from a previous layout would corrupt WM_GETMINMAXINFO. */
UTEST_F(host_fx, begin_px_resets_intrinsic_mins) {
    SuiRect r = {0.0f, 0.0f, 800.0f, 600.0f};
    utest_fixture->host.intrinsic_min_w = 999.0f;
    utest_fixture->host.intrinsic_min_h = 888.0f;
    sui_begin_px(&utest_fixture->host, r, 4.0f, SUI_CLEAR_NONE);
    ASSERT_NEAR(0.0f, utest_fixture->host.intrinsic_min_w, 0.001f);
    ASSERT_NEAR(0.0f, utest_fixture->host.intrinsic_min_h, 0.001f);
}

/* P2 — combobox: null item entry must produce empty string, not uninitialized memory. */
UTEST_F(host_fx, combobox_null_item_entry_is_empty_string) {
    const wchar_t* items[] = { L"Alpha", NULL, L"Gamma" };
    int idx = sui_ctrl_add_combobox(&utest_fixture->host, 9002,
                                    0.0f, 0.0f, 100.0f, 30.0f,
                                    items, 3, 0);
    ASSERT_GE(idx, 0);
    ASSERT_EQ(L'\0', utest_fixture->host.controls[idx].d.combo.items[1][0]);
}

/* P2 — clear_page closes open combo popup before freeing items (use-after-free).
 * Needs a live popup HWND to exercise the destroy path — skip in headless suite. */
UTEST_F(host_fx, clear_page_closes_combo_popup_before_freeing_items) {
    UTEST_SKIP("Phase 2: needs live combo popup HWND — headless suite cannot create it");
}

/* =========================================================================
 * Continuous corners through the renderer: a software DC render target over a 32-bit DIB, one pixel a unit. Every rounded rect
 * the renderer draws (a fill, a border, a focus ring's inner edge, a clip's masked corners) has the continuous corner of
 * sui_corner.h: pixels a pixel away from an edge are wholly in or out, edge pixels within Direct2D's antialiasing of their share
 * (its own rounded rect sits up to 68 levels from the exact share of its circle), and each pixel where a circular corner's share
 * differs from the curve's by half a pixel or more nearer the curve's. A radius of 120 px puts them 2 px apart near each corner's ends.
 * ========================================================================= */
typedef struct LegacyCanvas { SuiRenderer r; HDC dc; HBITMAP bmp, old; uint32_t* bits; int w, h; } LegacyCanvas;
static int legacy_canvas_open(LegacyCanvas* c, int w, int h) {
    ID2D1Factory* fac = NULL; ID2D1DCRenderTarget* rt = NULL; D2D1_RENDER_TARGET_PROPERTIES rtp; BITMAPINFO bi; RECT rc;
    memset(c, 0, sizeof(*c)); sui_renderer_init_fields(&c->r); c->w = w; c->h = h;
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, SUI_IID_REF(IID_ID2D1Factory), NULL, (void**)&fac)) || !fac) return 0;
    c->r.d2d_factory = fac;
    memset(&rtp, 0, sizeof(rtp)); rtp.type = 1 /* D2D1_RENDER_TARGET_TYPE_SOFTWARE */;
    rtp.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM; rtp.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
    rtp.dpiX = rtp.dpiY = 96.0f;
    if (FAILED(fac->lpVtbl->CreateDCRenderTarget(fac, &rtp, &rt)) || !rt) return 0;
    c->r.render_target = rt;
    memset(&bi, 0, sizeof(bi)); bi.bmiHeader.biSize = sizeof(bi.bmiHeader); bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    c->dc = CreateCompatibleDC(NULL);
    if (!c->dc) return 0;
    c->bmp = CreateDIBSection(c->dc, &bi, DIB_RGB_COLORS, (void**)&c->bits, NULL, 0);
    if (!c->bmp || !c->bits) return 0;
    c->old = (HBITMAP)SelectObject(c->dc, c->bmp);
    rc.left = 0; rc.top = 0; rc.right = w; rc.bottom = h;
    return SUCCEEDED(rt->lpVtbl->BindDC(rt, c->dc, &rc));
}
static void legacy_canvas_close(LegacyCanvas* c) {
    sui_renderer_release(&c->r);   /* the brush, the shapes, the target, the factory */
    if (c->dc) { if (c->bmp) { SelectObject(c->dc, c->old); DeleteObject(c->bmp); } DeleteDC(c->dc); }
}
static void legacy_begin(LegacyCanvas* c) {
    ID2D1RenderTarget* rt = (ID2D1RenderTarget*)c->r.render_target; D2D1_COLOR_F clear = {0, 0, 0, 0};
    rt->lpVtbl->BeginDraw(rt); rt->lpVtbl->Clear(rt, &clear);
}
static int legacy_end(LegacyCanvas* c) {
    ID2D1RenderTarget* rt = (ID2D1RenderTarget*)c->r.render_target;
    const int ok = SUCCEEDED(rt->lpVtbl->EndDraw(rt, NULL, NULL));
    GdiFlush();
    return ok;
}
/* The shapes: kind 0 the rounded rect, 1 the ring a centred stroke of width w covers, 2 the focus ring's hole (the rounded rect w
 * inside the rect, radius r - w), 3 the rect outside the rounded rect; `circular` with circular corners instead of the curve. */
typedef struct LegacyShape { SuiCornerRect b; double r, w; int kind, circular; } LegacyShape;
static int legacy_in_round(SuiCornerRect b, double r, int circular, double x, double y) {
    double dx, dy;
    if (!circular) return sui_corner_round_rect_contains(b, r, x, y);
    if (x < b.x || y < b.y || x > b.x + b.width || y > b.y + b.height) return 0;
    r = sui_corner_radius(b.width, b.height, r);
    dx = fmax(fmax(b.x + r - x, x - (b.x + b.width - r)), 0.0); dy = fmax(fmax(b.y + r - y, y - (b.y + b.height - r)), 0.0);
    return dx * dx + dy * dy <= r * r;
}
static int legacy_inside(const LegacyShape* s, double x, double y) {
    SuiCornerRect o = s->b, i = s->b; const double k = s->w / 2;
    switch (s->kind) {
    case 0: return legacy_in_round(s->b, s->r, s->circular, x, y);
    case 1:
        o.x -= k; o.y -= k; o.width += s->w; o.height += s->w; i.x += k; i.y += k; i.width -= s->w; i.height -= s->w;
        return legacy_in_round(o, s->r + k, s->circular, x, y) && !legacy_in_round(i, s->r - k > 0 ? s->r - k : 0, s->circular, x, y);
    case 2:
        i.x += s->w; i.y += s->w; i.width -= 2 * s->w; i.height -= 2 * s->w;
        return legacy_in_round(i, s->r - s->w > 0 ? s->r - s->w : 0, s->circular, x, y);
    default:
        return x >= s->b.x && y >= s->b.y && x <= s->b.x + s->b.width && y <= s->b.y + s->b.height &&
               !legacy_in_round(s->b, s->r, s->circular, x, y);
    }
}
static double legacy_share(const LegacyShape* s, int px, int py) {
    int n = 0, i, j;
    n = legacy_inside(s, px, py) + legacy_inside(s, px + 1, py) + legacy_inside(s, px, py + 1) + legacy_inside(s, px + 1, py + 1);
    if (n == 0 || n == 4) return n / 4.0;
    for (n = 0, j = 0; j < 8; ++j) for (i = 0; i < 8; ++i) n += legacy_inside(s, px + (i + .5) / 8, py + (j + .5) / 8);
    return n / 64.0;
}
static int legacy_uniform(const LegacyShape* s, int px, int py) {   /* 1 or 0 at every corner of the 3 x 3 pixels around it, else -1 */
    int n = 0, i, j;
    for (j = -1; j <= 2; ++j) for (i = -1; i <= 2; ++i) n += legacy_inside(s, px + i, py + j);
    return n == 16 ? 1 : n == 0 ? 0 : -1;
}
/* `hole`: the pixels the shape covers are the ones left clear (the focus ring's hole); only its inside is compared. */
static void legacy_compare(const LegacyCanvas* c, LegacyShape s, int hole, unsigned* checked, unsigned* disputed, unsigned* wrong) {
    LegacyShape circle = s; int x, y; circle.circular = 1;
    for (y = 0; y < c->h; ++y) for (x = 0; x < c->w; ++x) {
        const int a = (int)(c->bits[(size_t)y * c->w + x] >> 24), away = legacy_uniform(&s, x, y);
        double want = away >= 0 ? away : legacy_share(&s, x, y), round = legacy_share(&circle, x, y), off, other;
        int bad;
        if (hole) { if (want == 0 && round == 0) continue; want = 1 - want; round = 1 - round; }
        off = fabs(a - 255 * want); other = fabs(a - 255 * round);
        bad = away >= 0 && !hole ? off != 0 : off > 80;
        if (hole && away == 1) bad = a != 0;
        if (fabs(want - round) >= .5) { ++*disputed; bad = bad || off >= other; }
        ++*checked;
        if (bad && (*wrong)++ < 6) printf("kind %d at %d,%d: alpha %d, the curve %.3f, a circular corner %.3f\n", s.kind, x, y, a, want, round);
    }
}
UTEST(renderer_corners, every_rounded_rect_the_renderer_draws_is_the_continuous_corner) {
    LegacyCanvas c; LegacyShape s; unsigned checked = 0, disputed[4] = {0, 0, 0, 0}, wrong = 0, builds; int k;
    const uint32_t white = 0xFFFFFF;
    ASSERT_TRUE(legacy_canvas_open(&c, 400, 400));
    s.b.x = 10; s.b.y = 10; s.b.width = 380; s.b.height = 380; s.r = 120; s.w = 6; s.circular = 0;
    for (k = 0; k < 4; ++k) {
        legacy_begin(&c);
        if (k == 0) sui_renderer_fill_rounded_rect(&c.r, 10, 10, 380, 380, 120, white);
        if (k == 1) sui_renderer_draw_rounded_rect(&c.r, 10, 10, 380, 380, 120, white, 6);
        if (k == 2) sui_renderer_draw_filled_ring(&c.r, 10, 10, 380, 380, 120, 6, white);
        if (k == 3) sui_renderer_mask_outside_rounded_rect(&c.r, 10, 10, 380, 380, 120, white);
        ASSERT_TRUE(legacy_end(&c));
        s.kind = k; legacy_compare(&c, s, k == 2, &checked, &disputed[k], &wrong);
    }
    printf("renderer corners: %u pixels; where a circular corner differs: fill %u, border %u, focus ring %u, mask %u; %u wrong\n",
           checked, disputed[0], disputed[1], disputed[2], disputed[3], wrong);
    for (k = 0; k < 4; ++k) EXPECT_GT(disputed[k], 50u);
    EXPECT_EQ(0u, wrong);
    builds = c.r.shape_cache.builds;                               /* a shape seen again builds nothing */
    legacy_begin(&c); sui_renderer_fill_rounded_rect(&c.r, 30, 40, 380, 380, 120, white); ASSERT_TRUE(legacy_end(&c));
    EXPECT_EQ(builds, c.r.shape_cache.builds);
    legacy_canvas_close(&c);
}

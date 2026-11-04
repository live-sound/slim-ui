/* sui_ctrl_omit.h — what the host's dispatch calls on the controls a build leaves out with SUI_NO_*.
 * The sui_ctrl_add_* of a left-out control is gone with it, so no such control exists and none of these runs for one. They only keep the
 * shared code in sui_controls.h compiling, and the compiler removes them with the branches that call them.
 * Included from slim_ui.h under #ifdef SUI_IMPLEMENTATION — no include guard.
 */
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4100) /* the stubs ignore their parameters */
#endif

#ifdef SUI_NO_COMBOBOX
static int  sui_ctrl_combo_is_virtual(SuiControl* c) { return 0; }
static void sui_ctrl_combo_get_item_view(SuiControl* c, int item_index, SuiComboItemView* out_item) {}
static void sui_ctrl_draw_combobox(SuiControlHost* host, SuiControl* ctrl) {}
static void sui_combo_popup_open(SuiControlHost* host, int ctrl_index) {}
static void sui_combo_popup_close(SuiControlHost* host) {}
static int  sui_combo_find_open(SuiControlHost* host) { return -1; }
static int  sui_ctrl_combo_on_keydown(SuiControlHost* host, SuiControl* c, WPARAM vk) { return 0; }
static void sui_ctrl_combo_release(SuiControl* c) {}
#endif

#ifdef SUI_NO_TABS
static float sui__tabs_absf(float v) { return v < 0.0f ? -v : v; }
static int   sui_ctrl_tabs_content_rect(const SuiControl* c, SuiRect* out) { return 0; }
static int   sui_ctrl_tabs_push_content_clip(SuiControlHost* host, SuiControl* tabs) { return 0; }
static void  sui_ctrl_tabs_pop_content_clip(SuiControlHost* host, SuiControl* tabs) {}
static int   sui_ctrl_tabs_item_at(SuiControlHost* host, SuiControl* tabs, float mx, float my) { return -1; }
static void  sui_ctrl_tabs_release_content_bitmap(SuiControl* c) {}
static void  sui_ctrl_tabs_release_host_transitions(SuiControlHost* host) {}
static void  sui_ctrl_tabs_stash_transition(SuiControlHost* host, SuiControl* c) {}
static int   sui_ctrl_tabs_content_tick(SuiControl* ctrl, float step) { return 0; }
static int   sui_ctrl_tabs_host_has_active_transition(SuiControlHost* host) { return 0; }
static float sui_ctrl_tabs_content_new_offset(SuiControl* ctrl) { return 0.0f; }
static float sui_ctrl_tabs_content_old_offset(SuiControl* ctrl) { return 0.0f; }
static float sui_ctrl_tabs_content_new_offset_y(SuiControl* ctrl) { return 0.0f; }
static float sui_ctrl_tabs_content_old_offset_y(SuiControl* ctrl) { return 0.0f; }
static float sui_ctrl_tabs_content_new_opacity(SuiControl* ctrl) { return 1.0f; }
static float sui_ctrl_tabs_content_old_opacity(SuiControl* ctrl) { return 0.0f; }
static void  sui_ctrl_tabs_start_indicator_transition(SuiControlHost* host, SuiControl* ctrl) {}
static int   sui_ctrl_tabs_indicator_tick(SuiControlHost* host, SuiControl* ctrl, float step) { return 0; }
static int   sui_ctrl_tabs_text_hover_tick(SuiControl* ctrl, float step) { return 0; }
static void  sui_ctrl_draw_tabs(SuiControlHost* host, SuiControl* ctrl) {}
static int   sui_ctrl_tabs_key_target(SuiControl* c, WPARAM vk, int* out_next, int* out_dir) { return 0; }
static int   sui_ctrl_tabs_on_keydown(SuiControlHost* host, SuiControl* c, WPARAM vk) { return 0; }
static int   sui_ctrl_tabs_update_hover(SuiControlHost* host, SuiControl* tabs, int is_hit, float mx, float my) { return 0; }
static void  sui_ctrl_tabs_release(SuiControl* c) {}
#endif

#ifdef SUI_NO_LIST
static void  sui_ctrl_list_get_row_view(SuiControl* c, int row_index, SuiListRow* out_row) {}
static float sui_ctrl_list_content_height(SuiControl* c) { return 0.0f; }
static int   sui_ctrl_list_scrollbar_geom(SuiControlHost* host, SuiControl* c, float* sb_x, float* sb_y, float* sb_w, float* sb_h,
                                          float* thumb_y, float* thumb_h, float* max_scroll) { return 0; }
static int   sui_ctrl_list_hit_thumb(SuiControlHost* host, SuiControl* c, float mx, float my) { return 0; }
static int   sui_ctrl_list_row_at(SuiControl* c, float my) { return -1; }
static int   sui_ctrl_list_update_hover(SuiControl* c, int is_hit, float my) { return 0; }
static int   sui_ctrl_list_on_keydown(SuiControlHost* host, SuiControl* c, WPARAM vk) { return 0; }
static void  sui_ctrl_draw_list(SuiControlHost* host, SuiControl* ctrl) {}
static void  sui_ctrl_list_release(SuiControl* c) {}
#endif

#ifdef SUI_NO_NAV_LIST
static void sui_ctrl_draw_nav_list(SuiControlHost* host, SuiControl* ctrl) {}
static int  sui_ctrl_nav_on_keydown(SuiControlHost* host, SuiControl* c, WPARAM vk) { return 0; }
static int  sui_ctrl_nav_update_hover(SuiControl* nav, int is_hit, float my) { return 0; }
static void sui_ctrl_nav_release(SuiControl* c) {}
#endif

#ifdef SUI_NO_SPINNER
#define SUI_SPINNER_REPEAT_TIMER_ID 0x5555
static int  sui__spinner_hit_pos(SuiRenderer* r, const SuiControl* ctrl, const wchar_t* val_buf, float text_w, float mx) { return 0; }
static void sui_ctrl_spinner_clamp(SuiControl* c) {}
static int  sui_ctrl_spinner_arrow_hit(const SuiControl* c, float s, float mx, float my, int* in_up) { return 0; }
static void sui_ctrl_spinner_stop_repeat(SuiControlHost* host) {}
static void sui_ctrl_spinner_repeat_tick(SuiControlHost* host, DWORD now) {}
static void sui_ctrl_spinner_start_repeat(SuiControlHost* host, int index, int in_up) {}
static void sui_ctrl_spinner_begin_edit(SuiControl* c) {}
static void sui_ctrl_spinner_commit(SuiControlHost* host, SuiControl* c) {}
static void sui_ctrl_spinner_on_char(SuiControlHost* host, SuiControl* c, wchar_t ch) {}
static void sui_ctrl_draw_spinner(SuiControlHost* host, SuiControl* ctrl) {}
static int  sui_ctrl_spinner_on_keydown(SuiControlHost* host, SuiControl* c, WPARAM vk) { return 0; }
static int  sui_ctrl_spinner_update_hover(SuiControl* sp, int is_hit, float mx, float my, float s) { return 0; }
#endif

#ifdef SUI_NO_SLIDER
static void sui_ctrl_slider_update_from_mouse(SuiControl* c, float mx) {}
static void sui_ctrl_draw_slider(SuiControlHost* host, SuiControl* ctrl) {}
static int  sui_ctrl_slider_on_keydown(SuiControlHost* host, SuiControl* c, WPARAM vk) { return 0; }
#endif

#ifdef SUI_NO_COLOR_SWATCH
static void sui_ctrl_draw_color_swatch(SuiControlHost* host, SuiControl* ctrl) {}
static BOOL sui__swatch_pick_color(SuiControlHost* host, SuiControl* ctrl) { return FALSE; }
#endif

#ifdef SUI_NO_IMAGE
static void sui_ctrl_draw_image(SuiControlHost* host, SuiControl* ctrl) {}
#endif

#ifdef SUI_NO_SHORTCUT_RECORDER
#define SUI_SHORTCUT_RECORD_TIMER_ID 0x5554
static void sui_ctrl_shortcut_on_raw_input(SuiControlHost* host, HRAWINPUT input) {}
static void sui_ctrl_shortcut_on_gamepad_device_change(void) {}
static void sui_ctrl_shortcut_cancel_recording(SuiControlHost* host, SuiControl* c, int notify) {}
static void sui_ctrl_draw_shortcut_recorder(SuiControlHost* host, SuiControl* ctrl) {}
static void sui_ctrl_shortcut_activate(SuiControlHost* host, SuiControl* c) {}
static int  sui_ctrl_shortcut_on_keydown(SuiControlHost* host, int idx, WPARAM vk, LPARAM lp) { return 0; }
static int  sui_ctrl_shortcut_on_keyup(SuiControlHost* host, int idx, WPARAM vk) { return 0; }
static int  sui_ctrl_shortcut_on_mouse_down(SuiControlHost* host, UINT mouse_button) { return 0; }
static int  sui_ctrl_shortcut_on_mouse_up(SuiControlHost* host, UINT mouse_button) { return 0; }
static void sui_ctrl_shortcut_poll_gamepad_recorders(SuiControlHost* host) {}
#endif

#ifdef SUI_NO_TOOLTIP
static void sui_ctrl_draw_tooltip(SuiControlHost* host, SuiControl* ctrl) {}
static void sui_ctrl_hide_tooltip_popup(SuiControlHost* host) {}
static void sui_ctrl_destroy_tooltip_popup(SuiControlHost* host) {}
static void sui_ctrl_show_tooltip_popup(SuiControlHost* host, int ctrl_idx) {}
#endif

#ifdef SUI_NO_SCROLLVIEW
#define SUI_SCROLLVIEW_SB_HOT_W 12.0f
static int  sui_ctrl_scrollview_thumb(SuiControlHost* host, SuiControl* c, float* sb_x, float* sb_y, float* sb_w, float* sb_h,
                                      float* thumb_y, float* thumb_h, float* max_scroll) { return 0; }
static int  sui_ctrl_scrollview_hit_thumb(SuiControlHost* host, SuiControl* c, float mx, float my) { return 0; }
static void sui_ctrl_draw_scrollview(SuiControlHost* host, SuiControl* ctrl) {}
static void sui_ctrl_draw_scrollview_chrome(SuiControlHost* host, SuiControl* ctrl) {}
static void sui_ctrl_scrollview_apply_offset(SuiControlHost* host, const D2D1_MATRIX_3X2_F* base, float dx, float dy) {}
static void sui_ctrl_scrollview_save_transform(SuiControlHost* host, D2D1_MATRIX_3X2_F* out) {}
static void sui_ctrl_scrollview_restore_transform(SuiControlHost* host, const D2D1_MATRIX_3X2_F* m) {}
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif

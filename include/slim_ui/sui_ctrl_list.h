/* sui_ctrl_list.h -- Scrollable selectable list with optional section rows.
 * Included from slim_ui.h under #ifdef SUI_IMPLEMENTATION -- no include guard.
 */

/* Forward decl - defined in sui_controls.h (included after this header). */
static void sui_ctrl_start_hover_anim(SuiControlHost* host);

#define SUI_LIST_SB_THIN_W    6.0f
#define SUI_LIST_SB_RIGHT_PAD 4.0f
#define SUI_LIST_SB_TRACK_PAD 4.0f
#define SUI_LIST_SB_HOT_W     12.0f
#define SUI_LIST_THUMB_MIN    24.0f

static int sui_ctrl_add_list(SuiControlHost* host, int id, float x, float y, float w, float h,
                             float item_height, float section_height, int row_capacity) {
    SuiListRow* rows;
    SuiControl* c;
    int idx;
    if (row_capacity <= 0) row_capacity = 128;
    rows = (SuiListRow*)sui_malloc((size_t)row_capacity * sizeof(SuiListRow));
    if (!rows) return -1;
    memset(rows, 0, (size_t)row_capacity * sizeof(SuiListRow));
    idx = sui_ctrl_add(host, SUI_CTRL_LIST, id, x, y, w, h, NULL);
    if (idx < 0) { sui_free(rows); return -1; }
    c = &host->controls[idx];
    c->d.list.rows = rows;
    c->d.list.row_count = 0;
    c->d.list.row_capacity = row_capacity;
    c->d.list.selected_row = -1;
    c->d.list.hover_row = -1;
    c->d.list.item_height = item_height;
    c->d.list.section_height = section_height;
    c->d.list.item_indent = 0.0f;
    c->d.list.options = SUI_LIST_OPTION_NONE;
    c->d.list.scroll_offset = 0.0f;
    c->d.list.sb_reveal = 0.0f;
    c->d.list.sb_dragging = 0;
    c->d.list.sb_drag_anchor = 0.0f;
    c->d.list.sb_drag_start_scroll = 0.0f;
    c->d.list.near_end_px = 160;
    return idx;
}

static int sui_ctrl_list_is_virtual(SuiControl* c) {
    return c && c->type == SUI_CTRL_LIST && c->d.list.virtual_get_row && c->d.list.virtual_row_count >= 0;
}

static int sui_ctrl_list_row_count(SuiControl* c) {
    if (!c || c->type != SUI_CTRL_LIST) return 0;
    return sui_ctrl_list_is_virtual(c) ? c->d.list.virtual_row_count : c->d.list.row_count;
}

static void sui_ctrl_list_get_row_view(SuiControl* c, int row_index, SuiListRow* out_row) {
    if (!out_row) return;
    memset(out_row, 0, sizeof(*out_row));
    if (!c || c->type != SUI_CTRL_LIST || row_index < 0) return;
    if (sui_ctrl_list_is_virtual(c)) {
        if (row_index >= c->d.list.virtual_row_count) return;
        c->d.list.virtual_get_row(c, row_index, out_row, c->d.list.virtual_data);
        return;
    }
    if (row_index >= c->d.list.row_count || !c->d.list.rows) return;
    *out_row = c->d.list.rows[row_index];
}

static void sui_ctrl_list_set_virtual(SuiControlHost* host, int ctrl_id, int row_count,
                                      float content_height,
                                      SuiListVirtualGetRowFn get_row,
                                      SuiListVirtualRowAtOffsetFn row_at_offset,
                                      SuiListVirtualRowForItemFn row_for_item,
                                      void* userdata) {
    SuiControl* c = sui_ctrl_find(host, ctrl_id);
    if (!c || c->type != SUI_CTRL_LIST) return;
    c->d.list.virtual_row_count = row_count > 0 ? row_count : 0;
    c->d.list.virtual_content_height = content_height > 0.0f ? content_height : 0.0f;
    c->d.list.virtual_get_row = get_row;
    c->d.list.virtual_row_at_offset = row_at_offset;
    c->d.list.virtual_row_for_item = row_for_item;
    c->d.list.virtual_data = userdata;
    if (c->d.list.selected_row >= c->d.list.virtual_row_count)
        c->d.list.selected_row = -1;
    if (c->d.list.hover_row >= c->d.list.virtual_row_count)
        c->d.list.hover_row = -1;
    if (c->d.list.virtual_row_count == 0) {
        c->d.list.selected_row = -1;
        c->d.list.hover_row = -1;
    }
}

static void sui_ctrl_list_clear(SuiControlHost* host, int ctrl_id) {
    SuiControl* c = sui_ctrl_find(host, ctrl_id);
    if (!c || c->type != SUI_CTRL_LIST) return;
    c->d.list.row_count = 0;
    c->d.list.selected_row = -1;
    c->d.list.hover_row = -1;
    c->d.list.scroll_offset = 0.0f;
    c->d.list.sb_reveal = 0.0f;
    c->d.list.sb_dragging = 0;
    c->d.list.sb_drag_anchor = 0.0f;
    c->d.list.sb_drag_start_scroll = 0.0f;
    c->d.list.virtual_row_count = 0;
    c->d.list.virtual_content_height = 0.0f;
    c->d.list.virtual_get_row = NULL;
    c->d.list.virtual_row_at_offset = NULL;
    c->d.list.virtual_row_for_item = NULL;
    c->d.list.virtual_data = NULL;
}

static int sui_ctrl_list_add_row(SuiControl* c, int kind, int item_index,
                                 const wchar_t* leading, const wchar_t* text,
                                 const wchar_t* detail) {
    SuiListRow* row;
    if (!c || c->type != SUI_CTRL_LIST) return -1;
    if (c->d.list.row_count >= c->d.list.row_capacity) {
        int new_capacity = c->d.list.row_capacity > 0 ? c->d.list.row_capacity * 2 : 128;
        SuiListRow* new_rows = (SuiListRow*)sui_malloc((size_t)new_capacity * sizeof(SuiListRow));
        if (!new_rows) return -1;
        memset(new_rows, 0, (size_t)new_capacity * sizeof(SuiListRow));
        if (c->d.list.rows && c->d.list.row_count > 0)
            memcpy(new_rows, c->d.list.rows, (size_t)c->d.list.row_count * sizeof(SuiListRow));
        if (c->d.list.rows) sui_free(c->d.list.rows);
        c->d.list.rows = new_rows;
        c->d.list.row_capacity = new_capacity;
    }
    row = &c->d.list.rows[c->d.list.row_count];
    memset(row, 0, sizeof(*row));
    row->kind = kind;
    row->item_index = item_index;
    sui__wstrncpy_z(row->leading, leading ? leading : L"", 16);
    sui__wstrncpy_z(row->text, text ? text : L"", 96);
    sui__wstrncpy_z(row->detail, detail ? detail : L"", 96);
    return c->d.list.row_count++;
}

static int sui_ctrl_list_add_section(SuiControlHost* host, int ctrl_id, const wchar_t* text) {
    SuiControl* c = sui_ctrl_find(host, ctrl_id);
    return sui_ctrl_list_add_row(c, SUI_LIST_ROW_SECTION, -1, L"", text, L"");
}

static int sui_ctrl_list_add_item(SuiControlHost* host, int ctrl_id, int item_index,
                                  const wchar_t* leading, const wchar_t* text,
                                  const wchar_t* detail) {
    SuiControl* c = sui_ctrl_find(host, ctrl_id);
    int row = sui_ctrl_list_add_row(c, SUI_LIST_ROW_ITEM, item_index, leading, text, detail);
    if (c && c->type == SUI_CTRL_LIST && c->d.list.selected_row < 0 && row >= 0)
        c->d.list.selected_row = row;
    return row;
}

static void sui_ctrl_list_set_item_indent(SuiControlHost* host, int ctrl_id, float indent) {
    SuiControl* c = sui_ctrl_find(host, ctrl_id);
    if (!c || c->type != SUI_CTRL_LIST) return;
    c->d.list.item_indent = indent > 0.0f ? indent : 0.0f;
}

static void sui_ctrl_list_set_options(SuiControlHost* host, int ctrl_id, uint32_t options) {
    SuiControl* c = sui_ctrl_find(host, ctrl_id);
    if (!c || c->type != SUI_CTRL_LIST) return;
    c->d.list.options = options;
}

static float sui_ctrl_list_row_height(SuiControl* c, int row) {
    SuiListRow view;
    if (!c || c->type != SUI_CTRL_LIST || row < 0) return 0.0f;
    if (sui_ctrl_list_is_virtual(c)) {
        if (row >= c->d.list.virtual_row_count) return 0.0f;
        sui_ctrl_list_get_row_view(c, row, &view);
        return view.kind == SUI_LIST_ROW_SECTION ? c->d.list.section_height : c->d.list.item_height;
    }
    if (row >= c->d.list.row_count) return 0.0f;
    return c->d.list.rows[row].kind == SUI_LIST_ROW_SECTION
        ? c->d.list.section_height
        : c->d.list.item_height;
}

static float sui_ctrl_list_content_height(SuiControl* c) {
    float h = 0.0f;
    int i;
    if (!c || c->type != SUI_CTRL_LIST) return 0.0f;
    if (sui_ctrl_list_is_virtual(c)) return c->d.list.virtual_content_height;
    for (i = 0; i < c->d.list.row_count; i++)
        h += sui_ctrl_list_row_height(c, i);
    return h;
}

static void sui_ctrl_list_clamp_scroll(SuiControl* c) {
    float max_scroll;
    if (!c || c->type != SUI_CTRL_LIST) return;
    max_scroll = sui_ctrl_list_content_height(c) - c->h;
    if (max_scroll < 0.0f) max_scroll = 0.0f;
    if (c->d.list.scroll_offset < 0.0f) c->d.list.scroll_offset = 0.0f;
    if (c->d.list.scroll_offset > max_scroll) c->d.list.scroll_offset = max_scroll;
}

static int sui_ctrl_list_scrollbar_geom(SuiControlHost* host, SuiControl* c,
                                        float* sb_x, float* sb_y,
                                        float* sb_w, float* sb_h,
                                        float* thumb_y, float* thumb_h,
                                        float* max_scroll) {
    float s;
    float content_h;
    float ms;
    float track_h;
    float th;
    if (!host || !c || c->type != SUI_CTRL_LIST) return 0;
    s = sui_ctrl_dpi_scale(host);
    content_h = sui_ctrl_list_content_height(c);
    ms = content_h - c->h;
    if (ms <= 0.0f || content_h <= 0.0f) {
        if (max_scroll) *max_scroll = 0.0f;
        return 0;
    }
    track_h = c->h - SUI_LIST_SB_TRACK_PAD * 2.0f * s;
    if (track_h <= 0.0f) {
        if (max_scroll) *max_scroll = 0.0f;
        return 0;
    }
    th = track_h * (c->h / content_h);
    if (th < SUI_LIST_THUMB_MIN * s) th = SUI_LIST_THUMB_MIN * s;
    if (th > track_h) th = track_h;
    if (sb_x) *sb_x = c->x + c->w - SUI_LIST_SB_THIN_W * s - SUI_LIST_SB_RIGHT_PAD * s;
    if (sb_y) *sb_y = c->y + SUI_LIST_SB_TRACK_PAD * s;
    if (sb_w) *sb_w = SUI_LIST_SB_THIN_W * s;
    if (sb_h) *sb_h = track_h;
    if (thumb_h) *thumb_h = th;
    if (thumb_y) *thumb_y = c->y + SUI_LIST_SB_TRACK_PAD * s
        + (track_h - th) * (c->d.list.scroll_offset / ms);
    if (max_scroll) *max_scroll = ms;
    return 1;
}

static int sui_ctrl_list_hit_thumb(SuiControlHost* host, SuiControl* c,
                                   float mx, float my) {
    float s;
    float gutter_left;
    float sb_x, sb_y, sb_w, sb_h, thumb_y, thumb_h, ms;
    if (!host || !c || c->type != SUI_CTRL_LIST) return 0;
    s = sui_ctrl_dpi_scale(host);
    gutter_left = c->x + c->w - SUI_LIST_SB_HOT_W * s;
    if (mx < gutter_left || mx > c->x + c->w) return 0;
    if (!sui_ctrl_list_scrollbar_geom(host, c, &sb_x, &sb_y, &sb_w, &sb_h,
                                      &thumb_y, &thumb_h, &ms)) return 0;
    return (my >= thumb_y && my <= thumb_y + thumb_h) ? 1 : 0;
}

static int sui_ctrl_list_is_near_end(SuiControlHost* host, int ctrl_id) {
    SuiControl* c = sui_ctrl_find(host, ctrl_id);
    float remaining;
    if (!c || c->type != SUI_CTRL_LIST) return 0;
    sui_ctrl_list_clamp_scroll(c);
    remaining = sui_ctrl_list_content_height(c) - (c->d.list.scroll_offset + c->h);
    return remaining <= (float)c->d.list.near_end_px ? 1 : 0;
}

static int sui_ctrl_list_get_selected_item(SuiControlHost* host, int ctrl_id) {
    SuiControl* c = sui_ctrl_find(host, ctrl_id);
    SuiListRow row;
    if (!c || c->type != SUI_CTRL_LIST) return -1;
    if (c->d.list.selected_row < 0 || c->d.list.selected_row >= sui_ctrl_list_row_count(c)) return -1;
    sui_ctrl_list_get_row_view(c, c->d.list.selected_row, &row);
    if (row.kind != SUI_LIST_ROW_ITEM) return -1;
    return row.item_index;
}

static int sui_ctrl_list_set_selected_item(SuiControlHost* host, int ctrl_id, int item_index) {
    SuiControl* c = sui_ctrl_find(host, ctrl_id);
    int i;
    if (!c || c->type != SUI_CTRL_LIST) return 0;
    if (sui_ctrl_list_is_virtual(c) && c->d.list.virtual_row_for_item) {
        i = c->d.list.virtual_row_for_item(c, item_index, c->d.list.virtual_data);
        if (i >= 0 && i < c->d.list.virtual_row_count) {
            c->d.list.selected_row = i;
            return 1;
        }
        return 0;
    }
    for (i = 0; i < sui_ctrl_list_row_count(c); i++) {
        SuiListRow row;
        sui_ctrl_list_get_row_view(c, i, &row);
        if (row.kind == SUI_LIST_ROW_ITEM && row.item_index == item_index) {
            c->d.list.selected_row = i;
            return 1;
        }
    }
    return 0;
}

static int sui_ctrl_list_row_at(SuiControl* c, float my) {
    float y, local;
    int i;
    float row_top;
    if (!c || c->type != SUI_CTRL_LIST) return -1;
    local = my - c->y + c->d.list.scroll_offset;
    if (sui_ctrl_list_is_virtual(c) && c->d.list.virtual_row_at_offset) {
        i = c->d.list.virtual_row_at_offset(c, local, &row_top, c->d.list.virtual_data);
        return (i >= 0 && i < c->d.list.virtual_row_count) ? i : -1;
    }
    y = 0.0f;
    for (i = 0; i < sui_ctrl_list_row_count(c); i++) {
        float rh = sui_ctrl_list_row_height(c, i);
        if (local >= y && local < y + rh) return i;
        y += rh;
    }
    return -1;
}

static int sui_ctrl_list_update_hover(SuiControl* c, int is_hit, float my) {
    int old_hover, row;
    SuiListRow view;
    if (!c || c->type != SUI_CTRL_LIST) return 0;
    old_hover = c->d.list.hover_row;
    row = is_hit ? sui_ctrl_list_row_at(c, my) : -1;
    if (row >= 0) {
        sui_ctrl_list_get_row_view(c, row, &view);
        if (view.kind != SUI_LIST_ROW_ITEM || view.item_index < 0) row = -1;
    }
    c->d.list.hover_row = row;
    return old_hover != row;
}

static int sui_ctrl_list_select_delta(SuiControlHost* host, SuiControl* c, int delta) {
    int row;
    int row_count;
    SuiListRow view;
    if (!c || c->type != SUI_CTRL_LIST) return 0;
    row_count = sui_ctrl_list_row_count(c);
    if (row_count == 0) return 0;
    row = c->d.list.selected_row;
    if (row < 0) row = delta > 0 ? -1 : row_count;
    do {
        row += delta;
        if (row >= 0 && row < row_count) {
            sui_ctrl_list_get_row_view(c, row, &view);
            if (view.kind == SUI_LIST_ROW_ITEM && view.item_index >= 0) break;
        }
    } while (row >= 0 && row < row_count);
    if (row < 0 || row >= row_count) return 0;
    c->d.list.selected_row = row;
    c->d.list.sb_reveal = 1.0f;
    sui_ctrl_start_hover_anim(host);
    if (host->on_click) host->on_click(c->id, host->on_click_data);
    InvalidateRect(host->hwnd, NULL, FALSE);
    return 1;
}

static int sui_ctrl_list_on_keydown(SuiControlHost* host, SuiControl* c, WPARAM vk) {
    if (vk == VK_UP) return sui_ctrl_list_select_delta(host, c, -1);
    if (vk == VK_DOWN) return sui_ctrl_list_select_delta(host, c, 1);
    if (vk == VK_RETURN) {
        if (host->on_click) host->on_click(c->id, host->on_click_data);
        InvalidateRect(host->hwnd, NULL, FALSE);
        return 1;
    }
    return 0;
}

static void sui_ctrl_draw_list(SuiControlHost* host, SuiControl* ctrl) {
    SuiRenderer* r = host->renderer;
    const SuiColorScheme* sc = host->scheme;
    float s = sui_ctrl_dpi_scale(host);
    float y;
    float row_top = 0.0f;
    float content_h;
    int i, row_count;
    uint32_t bg_color = sui_ctrl_style_color(ctrl, SUI_STYLE_BG_COLOR, sc->background);
    uint32_t selected_color = sui_ctrl_style_color(ctrl, SUI_STYLE_SELECTED_COLOR, sc->nav_selected);
    uint32_t hover_color = sui_ctrl_style_color(ctrl, SUI_STYLE_HOVER_COLOR, sc->nav_hover);
    uint32_t text_color = sui_ctrl_style_color(ctrl, SUI_STYLE_TEXT_COLOR, sc->text);
    uint32_t detail_color = sui_ctrl_style_color(ctrl, SUI_STYLE_PLACEHOLDER_COLOR, sc->text_secondary);
    uint32_t section_color = sui_ctrl_style_color(ctrl, SUI_STYLE_ACCENT_COLOR, sc->text_secondary);
    uint32_t border_color = sui_ctrl_style_color(ctrl, SUI_STYLE_BORDER_COLOR, sc->nav_border);
    float pad_x = 12.0f * s;
    float leading_w = 52.0f * s;
    float sbw = SUI_LIST_SB_THIN_W * s;

    if (ctrl->state & SUI_CTRL_HIDDEN) return;
    sui_ctrl_list_clamp_scroll(ctrl);
    content_h = sui_ctrl_list_content_height(ctrl);

    sui_renderer_fill_rounded_rect(r, ctrl->x, ctrl->y, ctrl->w, ctrl->h, 8.0f * s, bg_color);
    sui_renderer_push_clip(r, ctrl->x, ctrl->y, ctrl->w, ctrl->h);
    row_count = sui_ctrl_list_row_count(ctrl);
    if (sui_ctrl_list_is_virtual(ctrl) && ctrl->d.list.virtual_row_at_offset) {
        i = ctrl->d.list.virtual_row_at_offset(ctrl, ctrl->d.list.scroll_offset, &row_top, ctrl->d.list.virtual_data);
        if (i < 0) i = 0;
    } else {
        i = 0;
        row_top = 0.0f;
    }
    y = ctrl->y + row_top - ctrl->d.list.scroll_offset;
    for (; i < row_count; i++) {
        SuiListRow row_view;
        SuiListRow* row = &row_view;
        float rh = sui_ctrl_list_row_height(ctrl, i);
        if (rh <= 0.0f) continue;
        sui_ctrl_list_get_row_view(ctrl, i, row);
        if (y + rh >= ctrl->y && y <= ctrl->y + ctrl->h) {
            if (row->kind == SUI_LIST_ROW_SECTION) {
                sui_renderer_draw_text_middle_ex(r, row->text,
                    ctrl->x + pad_x, y, ctrl->w - pad_x * 2.0f, rh,
                    section_color, 1, 13.0f, sui_ctrl_style_font_family(ctrl));
            } else {
                float indent = ctrl->d.list.item_indent;
                float row_x = ctrl->x + indent;
                float row_w = ctrl->w - indent;
                if (row_w < 0.0f) row_w = 0.0f;
                float text_x = row_x + pad_x;
                float text_w = row_w - pad_x * 2.0f;
                if (i == ctrl->d.list.selected_row)
                    sui_renderer_fill_rounded_rect(r, row_x + 4.0f * s, y + 3.0f * s,
                        row_w - 8.0f * s, rh - 6.0f * s, 6.0f * s, selected_color);
                else if (i == ctrl->d.list.hover_row)
                    sui_renderer_fill_rounded_rect(r, row_x + 4.0f * s, y + 3.0f * s,
                        row_w - 8.0f * s, rh - 6.0f * s, 6.0f * s, hover_color);
                if (row->leading[0]) {
                    sui_renderer_draw_text_middle_ex(r, row->leading,
                        ctrl->x + pad_x, y, leading_w, rh,
                        text_color, 0, 16.0f, L"Segoe UI Emoji");
                    text_x += leading_w;
                    text_w -= leading_w;
                }
                if (row->detail[0]) {
                    if (ctrl->d.list.options & SUI_LIST_OPTION_SINGLE_LINE_TAIL_ELLIPSIS) {
                        sui_renderer_draw_text_singleline_tail_ellipsis_weight_alpha_ex(r, row->text,
                            text_x, y + 7.0f * s, text_w - sbw, rh * 0.50f,
                            text_color, 1.0f, 400, 12.0f, sui_ctrl_style_font_family(ctrl), 0);
                        sui_renderer_draw_text_singleline_tail_ellipsis_weight_alpha_ex(r, row->detail,
                            text_x, y + rh * 0.52f, text_w - sbw, rh * 0.36f,
                            detail_color, 1.0f, 400, 10.5f, sui_ctrl_style_font_family(ctrl), 0);
                    } else {
                        sui_renderer_draw_text_ex(r, row->text,
                            text_x, y + 7.0f * s, text_w - sbw, rh * 0.50f,
                            text_color, 0, 12.0f, sui_ctrl_style_font_family(ctrl));
                        sui_renderer_draw_text_ex(r, row->detail,
                            text_x, y + rh * 0.52f, text_w - sbw, rh * 0.36f,
                            detail_color, 0, 10.5f, sui_ctrl_style_font_family(ctrl));
                    }
                } else {
                    if (ctrl->d.list.options & SUI_LIST_OPTION_SINGLE_LINE_TAIL_ELLIPSIS) {
                        sui_renderer_draw_text_singleline_tail_ellipsis_weight_alpha_ex(r, row->text,
                            text_x, y, text_w - sbw, rh,
                            text_color, 1.0f, 400, 12.0f, sui_ctrl_style_font_family(ctrl), 1);
                    } else {
                        sui_renderer_draw_text_middle_ex(r, row->text,
                            text_x, y, text_w - sbw, rh,
                            text_color, 0, 12.0f, sui_ctrl_style_font_family(ctrl));
                    }
                }
            }
        }
        y += rh;
        if (y > ctrl->y + ctrl->h) break;
    }
    sui_renderer_pop_clip(r);
    sui_renderer_draw_rounded_rect(r, ctrl->x, ctrl->y, ctrl->w, ctrl->h, 8.0f * s, border_color, 1.0f);
    if (content_h > ctrl->h) {
        float reveal = ctrl->hover_t;
        if (ctrl->d.list.sb_reveal > reveal) reveal = ctrl->d.list.sb_reveal;
        if (ctrl->d.list.sb_dragging) reveal = 1.0f;
        if (reveal > 0.0f) {
            float sb_x, sb_y, sb_h, thumb_y, thumb_h, ms;
            if (sui_ctrl_list_scrollbar_geom(host, ctrl, &sb_x, &sb_y, &sbw, &sb_h,
                                             &thumb_y, &thumb_h, &ms)) {
                uint32_t thumb_color =
                    sui_ctrl_style_color(ctrl, SUI_STYLE_ACCENT_COLOR, sc->check_border);
                sui_renderer_fill_rounded_rect_alpha(r, sb_x, thumb_y,
                    sbw, thumb_h, sbw * 0.5f, thumb_color, reveal);
            }
        }
    }
    sui_ctrl_draw_kb_focus_ring(host, ctrl, 4.0f * s);
}

static void sui_ctrl_list_release(SuiControl* c) {
    if (c->type != SUI_CTRL_LIST) return;
    if (c->d.list.rows) { sui_free(c->d.list.rows); c->d.list.rows = NULL; }
    c->d.list.row_count = 0;
    c->d.list.row_capacity = 0;
    c->d.list.virtual_row_count = 0;
    c->d.list.virtual_content_height = 0.0f;
    c->d.list.virtual_get_row = NULL;
    c->d.list.virtual_row_at_offset = NULL;
    c->d.list.virtual_row_for_item = NULL;
    c->d.list.virtual_data = NULL;
}

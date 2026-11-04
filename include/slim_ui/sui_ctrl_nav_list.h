/* sui_ctrl_nav_list.h -- Vertical sidebar navigation list with icon + text items.
 * Included from slim_ui.h under #ifdef SUI_IMPLEMENTATION -- no include guard.
 */

/* ── Constructor ─────────────────────────────────────────────────────────────── */

static int sui_ctrl_add_nav_list(SuiControlHost* host, int id, float x, float y, float w, float h,
                                  float item_height, int selected) {
    SuiNavItem* items;
    SuiControl* c;
    int idx;
    items = (SuiNavItem*)sui_malloc(SUI_MAX_ITEMS * sizeof(SuiNavItem));
    if (!items) return -1;
    /* sui_ctrl_add zeroes the control slot — assign items pointer after. */
    idx = sui_ctrl_add(host, SUI_CTRL_NAV_LIST, id, x, y, w, h, NULL);
    if (idx < 0) { sui_free(items); return -1; }
    c = &host->controls[idx];
    c->d.nav.items         = items;
    c->d.nav.item_height   = item_height;
    c->d.nav.selected      = selected;
    c->d.nav.item_count    = 0;
    c->d.nav.hover_item    = -1;
    c->d.nav.scroll_offset = 0;
    return idx;
}

/* ── Add item helper ─────────────────────────────────────────────────────────── */

static void sui_ctrl_nav_add_item(SuiControlHost* host, int ctrl_id,
                                   const wchar_t* text, const wchar_t* icon, uint32_t icon_color) {
    SuiControl* c = sui_ctrl_find(host, ctrl_id);
    SuiNavItem* item;
    if (!c || c->type != SUI_CTRL_NAV_LIST) return;
    if (c->d.nav.item_count >= SUI_MAX_ITEMS) return;
    item = &c->d.nav.items[c->d.nav.item_count++];
    wcsncpy(item->text, text ? text : L"", 63);
    item->text[63] = L'\0';
    wcsncpy(item->icon, icon ? icon : L"", 3);
    item->icon[3] = L'\0';
    item->icon_color = icon_color;
}

/* Opt in to changing the selected nav item with the mouse wheel while the
 * pointer is over an external content rectangle. Coordinates are physical
 * pixels, matching the imperative control factories. The wheel fallback is
 * suppressed for the entire rectangle whenever any visible Slim UI vertical
 * scroller in it has overflow. */
static void sui_ctrl_nav_set_content_wheel_region(SuiControlHost* host, int ctrl_id,
                                                   float x, float y, float w, float h,
                                                   int enabled) {
    SuiControl* c = sui_ctrl_find(host, ctrl_id);
    if (!c || c->type != SUI_CTRL_NAV_LIST) return;
    c->d.nav.content_wheel_enabled = (enabled && w > 0.0f && h > 0.0f) ? 1 : 0;
    c->d.nav.content_wheel_delta = 0;
    c->d.nav.content_wheel_x = x;
    c->d.nav.content_wheel_y = y;
    c->d.nav.content_wheel_w = w;
    c->d.nav.content_wheel_h = h;
}

/* ── Draw ────────────────────────────────────────────────────────────────────── */

static void sui_ctrl_draw_nav_list(SuiControlHost* host, SuiControl* ctrl) {
    SuiRenderer* r = host->renderer;
    const SuiColorScheme* sc = host->scheme;
    ID2D1RenderTarget* rt = (ID2D1RenderTarget*)r->render_target;
    float s = sui_ctrl_dpi_scale(host);
    /* item_height is in pixels (already DPI-scaled by caller) — consistent
     * with ctrl->x/y/w/h. Internal padding/radius values below are in DIPs
     * and get the * s multiplier. */
    float ih = ctrl->d.nav.item_height;
    float pad_x = 10.0f * s;
    float chip_size = 24.0f * s;
    float chip_pad  = (ih - chip_size) * 0.5f;  /* vertical center within item */
    float chip_rad  = 4.0f * s;
    float text_x, text_y, text_h;
    float sbw = 8.0f * s;
    float visible_h = ctrl->h;
    float total_h;
    float max_scroll;
    int i, first_item, last_item;
    D2D1_RECT_F clip;
    uint32_t bg_color = sui_ctrl_style_color(ctrl, SUI_STYLE_BG_COLOR, sc->nav_bg);
    uint32_t selected_color = sui_ctrl_style_color(ctrl, SUI_STYLE_SELECTED_COLOR, sc->nav_selected);
    uint32_t hover_color = sui_ctrl_style_color(ctrl, SUI_STYLE_HOVER_COLOR, sc->nav_hover);
    uint32_t text_color = sui_ctrl_style_color(ctrl, SUI_STYLE_TEXT_COLOR, sc->text);
    uint32_t border_color = sui_ctrl_style_color(ctrl, SUI_STYLE_BORDER_COLOR, sc->nav_border);
    float font_size = sui_ctrl_style_font_size(ctrl, 13.0f);
    const wchar_t* font_family = sui_ctrl_style_font_family(ctrl);
    int bold = (ctrl->state & SUI_CTRL_BOLD) ? 1 : 0;

    if (ctrl->state & SUI_CTRL_HIDDEN) return;
    if (!rt) return;

    total_h = (float)ctrl->d.nav.item_count * ih;

    /* Clamp scroll */
    max_scroll = total_h - visible_h;
    if (max_scroll < 0.0f) max_scroll = 0.0f;
    if (ctrl->d.nav.scroll_offset < 0.0f) ctrl->d.nav.scroll_offset = 0.0f;
    if (ctrl->d.nav.scroll_offset > max_scroll) ctrl->d.nav.scroll_offset = max_scroll;

    /* Background */
    sui_renderer_fill_rect(r, ctrl->x, ctrl->y, ctrl->w, ctrl->h, bg_color);

    /* Clip items to control bounds */
    clip.left   = ctrl->x;
    clip.top    = ctrl->y;
    clip.right  = ctrl->x + ctrl->w;
    clip.bottom = ctrl->y + ctrl->h;
    rt->lpVtbl->PushAxisAlignedClip(rt, &clip, 0);

    /* Determine visible item range */
    first_item = (int)(ctrl->d.nav.scroll_offset / ih);
    if (first_item < 0) first_item = 0;
    last_item = (int)((ctrl->d.nav.scroll_offset + visible_h) / ih) + 1;
    if (last_item > ctrl->d.nav.item_count) last_item = ctrl->d.nav.item_count;

    for (i = first_item; i < last_item; i++) {
        SuiNavItem* item = &ctrl->d.nav.items[i];
        float item_y = ctrl->y + (float)i * ih - ctrl->d.nav.scroll_offset;
        int is_selected = (ctrl->d.nav.selected == i);
        int is_hovered  = (ctrl->d.nav.hover_item == i);

        /* Item background */
        if (is_selected) {
            sui_renderer_fill_rect(r, ctrl->x, item_y, ctrl->w, ih, selected_color);
        } else if (is_hovered) {
            sui_renderer_fill_rect(r, ctrl->x, item_y, ctrl->w, ih, hover_color);
        }

        /* Icon chip */
        if (item->icon[0] != L'\0') {
            float chip_x = ctrl->x + pad_x;
            float chip_y = item_y + chip_pad;

            /* Chip background */
            sui_renderer_fill_rounded_rect(r, chip_x, chip_y, chip_size, chip_size, chip_rad,
                                           sui_ctrl_style_color(ctrl, SUI_STYLE_FILL_COLOR, item->icon_color));

            /* Icon glyph centered on chip using Segoe Fluent Icons */
            if (r->dwrite_factory && r->render_target) {
                IDWriteFactory* dw = (IDWriteFactory*)r->dwrite_factory;
                IDWriteTextFormat* icon_fmt = NULL;
                float icon_pt = 12.0f * s;  /* icon size in DIPs */
                const wchar_t* icon_family = (r->typography.icon.family[0] != L'\0')
                    ? r->typography.icon.family : L"Segoe Fluent Icons";
                int icon_weight = r->typography.icon.weight;
                if (icon_weight < 100) icon_weight = DWRITE_FONT_WEIGHT_NORMAL;
                dw->lpVtbl->CreateTextFormat(dw, icon_family, NULL,
                    icon_weight, DWRITE_FONT_STYLE_NORMAL,
                    DWRITE_FONT_STRETCH_NORMAL, icon_pt, L"", &icon_fmt);
                if (icon_fmt) {
                    ID2D1SolidColorBrush* br2; D2D1_COLOR_F ic; D2D1_RECT_F rc2;
                    ic.r = 1.0f; ic.g = 1.0f; ic.b = 1.0f; ic.a = 1.0f;
                    icon_fmt->lpVtbl->SetTextAlignment(icon_fmt, DWRITE_TEXT_ALIGNMENT_CENTER);
                    icon_fmt->lpVtbl->SetParagraphAlignment(icon_fmt, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                    if (SUCCEEDED(rt->lpVtbl->CreateSolidColorBrush(rt, &ic, NULL, &br2))) {
                        rc2.left   = chip_x;
                        rc2.top    = chip_y;
                        rc2.right  = chip_x + chip_size;
                        rc2.bottom = chip_y + chip_size;
                        rt->lpVtbl->DrawText(rt, item->icon, (UINT32)wcslen(item->icon),
                            icon_fmt, &rc2, (ID2D1Brush*)br2,
                            D2D1_DRAW_TEXT_OPTIONS_NONE, DWRITE_MEASURING_MODE_NATURAL);
                        br2->lpVtbl->Base.Base.Release((IUnknown*)br2);
                    }
                    icon_fmt->lpVtbl->Release(icon_fmt);
                }
            }

            text_x = ctrl->x + pad_x + chip_size + pad_x;
        } else {
            text_x = ctrl->x + pad_x;
        }

        /* Item text — vertically centered, left-aligned with larger font */
        text_h = ih;
        text_y = item_y;
        {
            float text_w = ctrl->w - (text_x - ctrl->x) - pad_x;
            if (total_h > visible_h) text_w -= sbw + 2.0f * s;
            if (text_w > 0.0f)
                sui_renderer_draw_text_middle_ex(r, item->text,
                    text_x, text_y, text_w, text_h,
                    text_color, bold, font_size, font_family);
        }
    }

    rt->lpVtbl->PopAxisAlignedClip(rt);

    /* Right border line */
    sui_renderer_draw_line(r,
        ctrl->x + ctrl->w - 1.0f, ctrl->y,
        ctrl->x + ctrl->w - 1.0f, ctrl->y + ctrl->h,
        border_color);

    /* Scrollbar (only if content overflows) */
    if (total_h > visible_h) {
        float sb_x    = ctrl->x + ctrl->w - sbw - 2.0f * s;
        float sb_y    = ctrl->y;
        float sb_h    = visible_h;
        float ratio   = visible_h / total_h;
        float thumb_h = sb_h * ratio;
        float thumb_y;
        if (thumb_h < 20.0f * s) thumb_h = 20.0f * s;
        if (max_scroll > 0.0f)
            thumb_y = sb_y + (sb_h - thumb_h) * (ctrl->d.nav.scroll_offset / max_scroll);
        else
            thumb_y = sb_y;
        sui_renderer_fill_rounded_rect(r, sb_x, sb_y, sbw, sb_h, 3.0f * s,
                                       sui_ctrl_style_color(ctrl, SUI_STYLE_BG_COLOR, sc->btn_area));
        sui_renderer_fill_rounded_rect(r, sb_x + s, thumb_y, sbw - 2.0f * s, thumb_h,
                                       3.0f * s,
                                       sui_ctrl_style_color(ctrl, SUI_STYLE_ACCENT_COLOR, sc->check_border));
    }

    sui_ctrl_draw_kb_focus_ring(host, ctrl, 4.0f * s);
}

/* ---- Keyboard handler ---------------------------------------------------- */

static int sui_ctrl_nav_on_keydown(SuiControlHost* host, SuiControl* c, WPARAM vk) {
    if (c->d.nav.item_count == 0) return 0;
    if (vk == VK_UP) {
        int prev = c->d.nav.selected - 1;
        if (prev < 0) prev = c->d.nav.item_count - 1;
        c->d.nav.content_wheel_delta = 0;
        c->d.nav.selected = prev;
        if (host->on_click) host->on_click(c->id, host->on_click_data);
        InvalidateRect(host->hwnd, NULL, FALSE);
        return 1;
    } else if (vk == VK_DOWN) {
        int next = c->d.nav.selected + 1;
        if (next >= c->d.nav.item_count) next = 0;
        c->d.nav.content_wheel_delta = 0;
        c->d.nav.selected = next;
        if (host->on_click) host->on_click(c->id, host->on_click_data);
        InvalidateRect(host->hwnd, NULL, FALSE);
        return 1;
    } else if (vk == VK_RETURN) {
        if (host->on_click) host->on_click(c->id, host->on_click_data);
        InvalidateRect(host->hwnd, NULL, FALSE);
        return 1;
    }
    return 0;
}

/* ---- Mouse hover update — returns 1 if hover state changed --------------- */

static int sui_ctrl_nav_update_hover(SuiControl* nav, int is_hit, float my) {
    int old_hover = nav->d.nav.hover_item;
    int new_hover = -1;
    if (is_hit) {
        float ih  = nav->d.nav.item_height;
        int   idx = (int)((my - nav->y + nav->d.nav.scroll_offset) / ih);
        if (idx >= 0 && idx < nav->d.nav.item_count)
            new_hover = idx;
    }
    nav->d.nav.hover_item = new_hover;
    return (new_hover != old_hover) ? 1 : 0;
}

/* ── Heap release ─────────────────────────────────────────────────────────────── */

static void sui_ctrl_nav_release(SuiControl* c) {
    if (c->type != SUI_CTRL_NAV_LIST) return;
    if (c->d.nav.items) { sui_free(c->d.nav.items); c->d.nav.items = NULL; }
    c->d.nav.item_count = 0;
}

/* sui_ctrl_combobox.h -- Combobox control with dropdown popup window.
 * Included from slim_ui.h under #ifdef SUI_IMPLEMENTATION.
 */

/* ---- Popup state struct (definition matches forward-decl in slim_ui.h) --- */

typedef struct SuiComboPopupIconSlot {
    int index;
    SuiImageHandle image;
} SuiComboPopupIconSlot;

typedef struct SuiComboPopupCpuIconSlot {
    int index;
    int width;
    int height;
    uint8_t* pixels; /* BGRA premultiplied */
} SuiComboPopupCpuIconSlot;

typedef struct SuiComboPopup {
    HWND          hwnd;
    HWND          shadow_hwnd;
    SuiRenderer   renderer;
    SuiControlHost* owner_host;
    int           owner_ctrl_index;
    SuiImageHandle* item_icons;
    int           item_icon_count;
    SuiComboPopupIconSlot icon_cache[16];
    int           icon_cache_next;
    SuiComboPopupCpuIconSlot cpu_icon_cache[16];
    int           cpu_icon_cache_next;
    int           layered; /* per-pixel alpha popup for exact antialiased corners */
    float         item_height;
    float         scroll_offset;
    int           hover_item;
    int           dragging;
    float         drag_start_y;
    float         drag_start_offset;
    int           opening; /* 1 = popup just opened; ignore first WM_LBUTTONUP */
    int           mouse_inside;
    int           tracking_mouse_leave;
    float         sb_reveal;
    /* Suppress hover highlight until the cursor actually moves. WM_MOUSEMOVE
     * fires automatically when a window appears under a stationary cursor; we
     * record the screen-space cursor position at open time and refuse to update
     * hover_item until cursor reports a different position. */
    int           mouse_armed;
    LONG          open_cursor_x;
    LONG          open_cursor_y;

    /* The menu is a separate, full-size surface. Opening/closing only changes
     * its opacity and gives it a short directional lift toward its resting
     * position; it never expands through the closed combobox. */
    int           anim_phase;        /* SUI_COMBO_PHASE_* */
    float         anim_t;            /* 0 = hidden, 1 = fully visible */
    double        anim_last_ms;      /* monotonic milliseconds */
    double        scrollbar_start_ms;
    float         motion_offset;     /* physical px travelled toward final position */
    float         corner_radius;     /* physical px, copied from closed control */
    int           popup_w;           /* list surface width (constant) */
    int           full_window_h;     /* list surface height (constant while open) */
    int           pos_x;             /* list surface left X (constant) */
    int           anchor_y;          /* final list surface top Y */
    int           opens_above;       /* 1 = menu is above the closed combobox */
    int           shadow_margin;     /* transparent px around the custom shadow */
    int           shadow_offset_y;   /* downward shadow displacement */

    /* Owner-window subclass state (installed on the root HWND for the lifetime
     * of the popup so top-level moves are observed even when host->hwnd is a
     * child/embedded SlimUI surface). */
    HWND          owner_subclass_hwnd;
    int           owner_subclass_registered;
} SuiComboPopup;

#define SUI_COMBO_PHASE_NONE         0
#define SUI_COMBO_PHASE_OPEN         1
#define SUI_COMBO_PHASE_CLOSE        2

#define SUI_COMBO_OPEN_DUR_S    0.14f
#define SUI_COMBO_CLOSE_DUR_S   0.10f
#define SUI_COMBO_SHADOW_PEAK_ALPHA 32
#define SUI_COMBO_ARROW_OPEN_TRAVEL  4.0f
#define SUI_COMBO_ARROW_CLOSE_TRAVEL 3.0f

static double sui_combo_popup_now_ms(const SuiComboPopup* popup) {
#ifdef SUI_FEATURE_VSYNC
    if(popup && popup->owner_host && popup->owner_host->vsync.dispatching)
        return popup->owner_host->vsync.frame_seconds*1000.0;
    return sui__vsync_now_seconds()*1000.0;
#else
    (void)popup;
    return (double)GetTickCount64();
#endif
}
#ifdef SUI_FEATURE_VSYNC
static void sui_combo_popup_vsync_tick(void* user,float dt_ms);
static int sui_combo_popup_start_clock(SuiComboPopup* popup) {
    return popup && popup->owner_host && sui_vsync_attach(&popup->owner_host->vsync,
        sui_combo_popup_vsync_tick,popup);
}
#endif

static float sui_combo_radius(float dpi_scale) {
    return 4.0f * dpi_scale;
}

static float sui_combo_popup_gap(float dpi_scale) {
    return 6.0f * dpi_scale;
}

static float sui_combo_popup_anim_progress(const SuiComboPopup* popup) {
    float t;
    if (!popup) return 0.0f;
    t = popup->anim_t;
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    return t * t * (3.0f - 2.0f * t); /* smoothstep */
}

static float sui_combo_popup_visible_height(const SuiComboPopup* popup) {
    if (!popup) return 0.0f;
    return (float)popup->full_window_h;
}

static float sui_combo_popup_visible_top(const SuiComboPopup* popup, float visible_h) {
    (void)popup;
    (void)visible_h;
    return 0.0f;
}

static int sui_combo_popup_alpha(const SuiComboPopup* popup) {
    float progress = sui_combo_popup_anim_progress(popup);
    int alpha = (int)(progress * 255.0f + 0.5f);
    if (alpha < 0) alpha = 0;
    if (alpha > 255) alpha = 255;
    return alpha;
}

static float sui_combo_popup_shadow_layer_opacity(int layer_index,
                                                  int layer_count) {
    float peak, previous_t, target_t, previous_alpha, target_alpha;
    if (layer_count <= 0 || layer_index < 0 || layer_index >= layer_count)
        return 0.0f;
    peak = (float)SUI_COMBO_SHADOW_PEAK_ALPHA / 255.0f;
    previous_t = (float)layer_index / (float)layer_count;
    target_t = (float)(layer_index + 1) / (float)layer_count;
    previous_alpha = peak * previous_t * previous_t;
    target_alpha = peak * target_t * target_t;
    /* Convert the desired cumulative alpha into the source-over opacity for
     * this rounded-rectangle layer. */
    return (target_alpha - previous_alpha) / (1.0f - previous_alpha);
}

static int sui_combo_popup_window_y(const SuiComboPopup* popup) {
    float progress, direction, y;
    if (!popup) return 0;
    progress = sui_combo_popup_anim_progress(popup);
    direction = popup->opens_above ? 1.0f : -1.0f;
    y = (float)popup->anchor_y
        + direction * popup->motion_offset * (1.0f - progress);
    return (int)(y + (y >= 0.0f ? 0.5f : -0.5f));
}

static int sui_combo_popup_point_visible(SuiComboPopup* popup, float x, float y) {
    RECT rc;
    float visible_h, visible_top;
    if (!popup || !popup->hwnd) return 0;
    if (!GetClientRect(popup->hwnd, &rc)) return 0;
    visible_h = sui_combo_popup_visible_height(popup);
    if (visible_h <= 0.0f) return 0;
    visible_top = sui_combo_popup_visible_top(popup, visible_h);
    return x >= 0.0f && x < (float)(rc.right - rc.left) &&
           y >= visible_top && y < visible_top + visible_h;
}

static void sui_combo_popup_apply_region(HWND hwnd, int width, int height,
                                         float radius_px) {
    if (!hwnd || width <= 0 || height <= 0) return;
    (void)radius_px;
    SetWindowRgn(hwnd, NULL, FALSE);
}

static int sui_combo_popup_cursor_inside(SuiComboPopup* popup) {
    POINT pt;
    if (!popup || !popup->hwnd) return 0;
    if (!GetCursorPos(&pt)) return 0;
    ScreenToClient(popup->hwnd, &pt);
    return sui_combo_popup_point_visible(popup, (float)pt.x, (float)pt.y);
}

static void sui_combo_popup_track_mouse_leave(SuiComboPopup* popup) {
    TRACKMOUSEEVENT tme;
    if (!popup || !popup->hwnd || popup->tracking_mouse_leave) return;
    memset(&tme, 0, sizeof(tme));
    tme.cbSize = sizeof(tme);
    tme.dwFlags = TME_LEAVE;
    tme.hwndTrack = popup->hwnd;
    if (TrackMouseEvent(&tme))
        popup->tracking_mouse_leave = 1;
}

static void sui_combo_popup_update_mouse_inside(SuiComboPopup* popup, int invalidate) {
    int inside;
    if (!popup || !popup->hwnd) return;
    inside = sui_combo_popup_cursor_inside(popup);
    if (inside) sui_combo_popup_track_mouse_leave(popup);
    if (popup->mouse_inside != inside) {
        popup->mouse_inside = inside;
        if (invalidate) InvalidateRect(popup->hwnd, NULL, FALSE);
    }
}

static void sui_combo_popup_reveal_scrollbar(SuiComboPopup* popup) {
    if (!popup || !popup->hwnd) return;
    popup->sb_reveal = 1.0f;
    popup->scrollbar_start_ms = sui_combo_popup_now_ms(popup);
#ifdef SUI_FEATURE_VSYNC
    if(!sui_combo_popup_start_clock(popup)) popup->sb_reveal=0;
#else
    SetTimer(popup->hwnd, 2, 16, NULL);
#endif
}

static float sui_combo_popup_scrollbar_opacity(const SuiComboPopup* popup) {
    float reveal;
    if (!popup) return 0.0f;
    if (popup->dragging) return 0.82f;
    if (popup->anim_phase == SUI_COMBO_PHASE_OPEN || popup->mouse_inside)
        return 0.72f;
    reveal = popup->sb_reveal;
    if (reveal < 0.0f) reveal = 0.0f;
    if (reveal > 1.0f) reveal = 1.0f;
    /* Keep the affordance present without letting hover turn it into a heavy
     * bar. Wheel/key activity only raises the quiet idle opacity slightly. */
    return 0.58f + reveal * 0.14f;
}

static int sui_combo_popup_highlighted_item(int selected_item,
                                            int hover_item,
                                            int item_count) {
    if (hover_item >= 0 && hover_item < item_count) return hover_item;
    if (selected_item >= 0 && selected_item < item_count) return selected_item;
    return -1;
}

/* ---- Constructor --------------------------------------------------------- */

static int sui_ctrl_add_combobox(SuiControlHost* host, int id, float x, float y, float w, float h,
                                 const wchar_t** items, int item_count, int selected) {
    wchar_t (*items_buf)[64];
    SuiControl* c;
    int idx, i, n;
    items_buf = (wchar_t (*)[64])sui_malloc(SUI_MAX_ITEMS * 64 * sizeof(wchar_t));
    if (!items_buf) return -1;
    memset(items_buf, 0, SUI_MAX_ITEMS * 64 * sizeof(wchar_t));
    /* sui_ctrl_add zeroes the control slot — assign items pointer after. */
    idx = sui_ctrl_add(host, SUI_CTRL_COMBOBOX, id, x, y, w, h, L"");
    if (idx < 0) { sui_free(items_buf); return -1; }
    c = &host->controls[idx];
    c->d.combo.items = items_buf;
    n = item_count < SUI_MAX_ITEMS ? item_count : SUI_MAX_ITEMS;
    for (i = 0; i < n; i++) {
        if (items && items[i])
            wcsncpy(c->d.combo.items[i], items[i], 63);
        c->d.combo.items[i][63] = L'\0';
    }
    c->d.combo.item_count      = n;
    c->d.combo.selected        = (selected >= 0 && selected < n) ? selected : 0;
    c->d.combo.open            = 0;
    c->d.combo.hover_item      = -1;
    c->d.combo.dropdown_scroll = 0.0f;
    c->d.combo.popup_width     = 0.0f;
    c->d.combo.popup_radius    = 0.0f;
    c->d.combo.popup_hwnd      = NULL;
    /* Set ctrl->text to selected item */
    if (n > 0)
        wcsncpy(c->text, c->d.combo.items[c->d.combo.selected], SUI_CTRL_TEXT_MAX - 1);
    c->text[SUI_CTRL_TEXT_MAX - 1] = L'\0';
    return idx;
}

static int sui_ctrl_combo_is_virtual(SuiControl* c) {
    return c && c->type == SUI_CTRL_COMBOBOX && c->d.combo.virtual_get_item && c->d.combo.virtual_count >= 0;
}

static int sui_ctrl_combo_count(SuiControl* c) {
    if (!c || c->type != SUI_CTRL_COMBOBOX) return 0;
    return sui_ctrl_combo_is_virtual(c) ? c->d.combo.virtual_count : c->d.combo.item_count;
}

static void sui_ctrl_combo_get_item_view(SuiControl* c, int item_index, SuiComboItemView* out_item) {
    if (!out_item) return;
    memset(out_item, 0, sizeof(*out_item));
    if (!c || c->type != SUI_CTRL_COMBOBOX || item_index < 0) return;
    if (sui_ctrl_combo_is_virtual(c)) {
        if (item_index >= c->d.combo.virtual_count) return;
        c->d.combo.virtual_get_item(c, item_index, out_item, c->d.combo.virtual_data);
        out_item->text[63] = L'\0';
        out_item->icon_path[259] = L'\0';
        return;
    }
    if (item_index >= c->d.combo.item_count || !c->d.combo.items) return;
    sui__wstrncpy_z(out_item->text, c->d.combo.items[item_index], 64);
    if (c->d.combo.item_icon_paths && c->d.combo.item_icon_paths[item_index][0]) {
        sui__wstrncpy_z(out_item->icon_path, c->d.combo.item_icon_paths[item_index], 260);
        out_item->has_icon = 1;
    }
    if (c->d.combo.item_icons) {
        out_item->icon = c->d.combo.item_icons[item_index];
        if (out_item->icon.data) out_item->has_icon = 1;
    }
}

static void sui_ctrl_combo_refresh_text(SuiControl* c) {
    SuiComboItemView item;
    int n;
    if (!c || c->type != SUI_CTRL_COMBOBOX) return;
    n = sui_ctrl_combo_count(c);
    if (n <= 0) {
        c->d.combo.selected = 0;
        c->text[0] = L'\0';
        return;
    }
    if (c->d.combo.selected < 0) c->d.combo.selected = 0;
    if (c->d.combo.selected >= n) c->d.combo.selected = n - 1;
    sui_ctrl_combo_get_item_view(c, c->d.combo.selected, &item);
    sui__wstrncpy_z(c->text, item.text, SUI_CTRL_TEXT_MAX);
}

static void sui_ctrl_combo_set_open_state(SuiControl* c, int open) {
    if (!c || c->type != SUI_CTRL_COMBOBOX) return;
    c->d.combo.open = open ? 1 : 0;
    if (open)
        c->state |= SUI_CTRL_PRESSED;
    else
        c->state &= ~SUI_CTRL_PRESSED;
}

static float sui_combo_popup_arrow_bounce_offset(const SuiComboPopup* popup,
                                                 float dpi_scale) {
    float t, direction, travel;
    if (!popup) return 0.0f;
    if (popup->anim_phase == SUI_COMBO_PHASE_OPEN) {
        t = popup->anim_t;
        direction = 1.0f;
        travel = SUI_COMBO_ARROW_OPEN_TRAVEL;
    } else if (popup->anim_phase == SUI_COMBO_PHASE_CLOSE) {
        t = 1.0f - popup->anim_t;
        direction = -1.0f;
        travel = SUI_COMBO_ARROW_CLOSE_TRAVEL;
    } else {
        return 0.0f;
    }
    if (t <= 0.0f || t >= 1.0f) return 0.0f;
    return direction * (travel * dpi_scale) * (4.0f * t * (1.0f - t));
}

static float sui_ctrl_combo_arrow_bounce_offset(SuiControlHost* host,
                                                SuiControl* ctrl,
                                                float dpi_scale) {
    SuiComboPopup* popup;
    int ci;
    if (!host || !ctrl || !ctrl->d.combo.open) return 0.0f;
    popup = host->combo_popup;
    if (!popup) return 0.0f;
    ci = popup->owner_ctrl_index;
    if (ci < 0 || ci >= host->control_count || &host->controls[ci] != ctrl)
        return 0.0f;
    return sui_combo_popup_arrow_bounce_offset(popup, dpi_scale);
}

static void sui_ctrl_combo_set_virtual(SuiControlHost* host, int ctrl_id, int item_count,
                                       int selected,
                                       SuiComboVirtualGetItemFn get_item,
                                       void* userdata) {
    SuiControl* c = sui_ctrl_find(host, ctrl_id);
    if (!c || c->type != SUI_CTRL_COMBOBOX) return;
    c->d.combo.virtual_count = item_count > 0 ? item_count : 0;
    c->d.combo.virtual_get_item = get_item;
    c->d.combo.virtual_data = userdata;
    c->d.combo.selected = selected;
    sui_ctrl_combo_refresh_text(c);
}

static float sui_ctrl_combo_popup_width(const SuiControl* c) {
    if (!c || c->type != SUI_CTRL_COMBOBOX) return 0.0f;
    return c->d.combo.popup_width > 0.0f ? c->d.combo.popup_width : c->w;
}

/* Set a physical-pixel popup width independent of the closed control.
 * Non-positive values restore the default of following the control width.
 * An already-open popup keeps its current surface until the next opening. */
static void sui_ctrl_combo_set_popup_width(SuiControlHost* host, int ctrl_id,
                                           float popup_width) {
    SuiControl* c = sui_ctrl_find(host, ctrl_id);
    if (!c || c->type != SUI_CTRL_COMBOBOX) return;
    c->d.combo.popup_width = popup_width > 0.0f ? popup_width : 0.0f;
}

static int sui_ctrl_combo_ensure_icon_slots(SuiControl* c) {
    if (!c || c->type != SUI_CTRL_COMBOBOX) return 0;
    if (!c->d.combo.item_icon_paths) {
        c->d.combo.item_icon_paths =
            (wchar_t (*)[260])sui_malloc(SUI_MAX_ITEMS * 260 * sizeof(wchar_t));
        if (!c->d.combo.item_icon_paths) return 0;
        memset(c->d.combo.item_icon_paths, 0, SUI_MAX_ITEMS * 260 * sizeof(wchar_t));
    }
    if (!c->d.combo.item_icons) {
        c->d.combo.item_icons =
            (SuiImageHandle*)sui_malloc(SUI_MAX_ITEMS * sizeof(SuiImageHandle));
        if (!c->d.combo.item_icons) return 0;
        memset(c->d.combo.item_icons, 0, SUI_MAX_ITEMS * sizeof(SuiImageHandle));
    }
    return 1;
}

static void sui_combo_draw_text_middle_single_line(SuiRenderer* r, const wchar_t* str,
    float x, float y, float w, float h, uint32_t color, int bold, float size,
    const wchar_t* family) {
    ID2D1RenderTarget* rt; IDWriteFactory* dw; IDWriteTextFormat* fmt;
    IDWriteTextLayout* layout;
    ID2D1SolidColorBrush* br;
    float cfs; D2D1_POINT_2F origin;
    if (!r || !r->render_target || !str || w <= 0.0f || h <= 0.0f) return;
    rt = (ID2D1RenderTarget*)r->render_target;
    dw = (IDWriteFactory*)r->dwrite_factory;
    if (!dw) return;
    br = sui__brush(r, color, 1.0f);
    if (!br) return;
    fmt = sui__resolve_text_fmt_ex(r, bold, size, family, &cfs);
    if (!fmt) return;
    if (FAILED(sui__text_layout(dw, str, (UINT32)wcslen(str), fmt, w, h, &layout)) || !layout)
        return;
    layout->lpVtbl->SetTextAlignment(layout, 0);
    layout->lpVtbl->SetParagraphAlignment(layout, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    layout->lpVtbl->SetWordWrapping(layout, DWRITE_WORD_WRAPPING_NO_WRAP);
    origin.x = x; origin.y = y - cfs * 0.04f;
    rt->lpVtbl->DrawTextLayout(rt, origin, layout, (ID2D1Brush*)br, 0);
    layout->lpVtbl->Release(layout);
}

static void sui_combo_draw_fallback_icon(SuiRenderer* r, float x, float y, float size,
                                         uint32_t bg, uint32_t fg) {
    float rad = size * 0.22f;
    float dot = size * 0.20f;
    float gap = size * 0.14f;
    float left = x + size * 0.26f;
    float top = y + size * 0.26f;
    sui_renderer_fill_rounded_rect(r, x, y, size, size, rad, bg);
    sui_renderer_fill_rounded_rect(r, left, top, dot, dot, dot * 0.35f, fg);
    sui_renderer_fill_rounded_rect(r, left + dot + gap, top, dot, dot, dot * 0.35f, fg);
    sui_renderer_fill_rounded_rect(r, left, top + dot + gap, dot, dot, dot * 0.35f, fg);
    sui_renderer_fill_rounded_rect(r, left + dot + gap, top + dot + gap, dot, dot, dot * 0.35f, fg);
}

/* ---- Draw (closed combobox) --------------------------------------------- */

static void sui_ctrl_draw_combobox_with_radius(SuiControlHost* host, SuiControl* ctrl,
                                               float rad) {
    SuiRenderer* r = host->renderer;
    const SuiColorScheme* sc = host->scheme;
    float s   = sui_ctrl_dpi_scale(host);
    float pad = 6.0f * s;
    int focused  = (ctrl->state & SUI_CTRL_FOCUSED) != 0;
    int disabled = (ctrl->state & SUI_CTRL_DISABLED) != 0;
    float arrow_area = 20.0f * s;
    float chevron_half = 4.0f * s;
    float chevron_drop = 3.5f * s;
    float chevron_thickness = 1.15f * s;
    float arrow_cx, arrow_cy;
    uint32_t text_col, fill_col;
    float bar_w    = 3.0f * s;
    float text_lpad;
    float text_x, text_w;
    SuiImageHandle selected_icon = {0};
    uint32_t bg_color = sui_ctrl_style_color(ctrl, SUI_STYLE_BG_COLOR, sc->input_bg);
    uint32_t disabled_bg = sui_ctrl_style_color(ctrl, SUI_STYLE_BG_COLOR, sc->nav_bg);
    uint32_t text_color = sui_ctrl_style_color(ctrl, SUI_STYLE_TEXT_COLOR, sc->input_text);
    uint32_t disabled_text = sui_ctrl_style_color(ctrl, SUI_STYLE_TEXT_COLOR, sc->text_secondary);
    uint32_t border_base = sui_ctrl_style_color(ctrl, SUI_STYLE_BORDER_COLOR, sc->input_border);
    uint32_t disabled_border = sui_ctrl_style_color(ctrl, SUI_STYLE_BORDER_COLOR, sc->nav_border);
    uint32_t hover_border = sui_ctrl_style_color(ctrl, SUI_STYLE_HOVER_COLOR, sc->btn_default_border);
    uint32_t pressed_bg = sui_ctrl_style_color(ctrl, SUI_STYLE_PRESS_COLOR, sc->btn_press);
    uint32_t pressed_border = sui_color_lerp(border_base, text_color, 0.28f);
    uint32_t arrow_color = sui_ctrl_style_color(ctrl, SUI_STYLE_ACCENT_COLOR, sc->text_secondary);
    const wchar_t* font_family = sui_ctrl_style_font_family(ctrl);
    float font_size = sui_ctrl_style_font_size(ctrl, r->font_size);
    int bold = (ctrl->state & SUI_CTRL_BOLD) ? 1 : 0;
    SuiComboItemView selected_item;

    if (ctrl->state & SUI_CTRL_HIDDEN) return;
    ctrl->d.combo.popup_radius = rad;

    fill_col  = disabled ? disabled_bg
                         : ((ctrl->state & SUI_CTRL_PRESSED) ? pressed_bg : bg_color);
    text_col  = disabled ? disabled_text : text_color;
    /* Closed button text lpad — must match popup item text lpad so there is no
     * visual jump on open. */
    text_lpad = 10.0f * s;
    sui_ctrl_combo_get_item_view(ctrl, ctrl->d.combo.selected, &selected_item);

    sui_renderer_fill_rounded_rect(r, ctrl->x, ctrl->y, ctrl->w, ctrl->h, rad, fill_col);

    sui_renderer_draw_rounded_rect(r, ctrl->x, ctrl->y, ctrl->w, ctrl->h, rad,
        disabled ? disabled_border
                 : ((ctrl->state & SUI_CTRL_PRESSED)
                        ? pressed_border
                        : sui_color_lerp(border_base, hover_border, ctrl->hover_t)),
        1.0f);

    if (selected_item.text[0]) {
        ID2D1RenderTarget* rt = (ID2D1RenderTarget*)r->render_target;
        if (rt) {
            D2D1_RECT_F clip;
            text_x = ctrl->x + text_lpad;
            text_w = ctrl->w - arrow_area - text_lpad - pad;
            selected_icon = selected_item.icon;
            if (selected_item.has_icon) {
                float icon_size = 18.0f * s;
                float icon_gap = 8.0f * s;
                float icon_y = ctrl->y + (ctrl->h - icon_size) * 0.5f;
                if (selected_icon.data) {
                    sui_renderer_draw_image(r, selected_icon, text_x, icon_y, icon_size, icon_size);
                } else {
                    sui_combo_draw_fallback_icon(r, text_x, icon_y, icon_size,
                        sui_color_lerp(fill_col, text_col, 0.10f), text_col);
                }
                text_x += icon_size + icon_gap;
                text_w -= icon_size + icon_gap;
                if (text_w < 0.0f) text_w = 0.0f;
            }
            clip.left   = ctrl->x + pad;
            clip.top    = ctrl->y;
            clip.right  = ctrl->x + ctrl->w - arrow_area - pad;
            clip.bottom = ctrl->y + ctrl->h;
            rt->lpVtbl->PushAxisAlignedClip(rt, &clip, 0);
            sui_combo_draw_text_middle_single_line(r, selected_item.text,
                text_x, ctrl->y, text_w, ctrl->h,
                text_col, bold, font_size, font_family);
            rt->lpVtbl->PopAxisAlignedClip(rt);
        }
    }

    /* Fluent-style dropdown chevron. */
    arrow_cx = ctrl->x + ctrl->w - text_lpad - chevron_half;
    arrow_cy = ctrl->y + ctrl->h / 2.0f
        + sui_ctrl_combo_arrow_bounce_offset(host, ctrl, s);
    { float lx = arrow_cx - chevron_half;
      float rx = arrow_cx + chevron_half;
      float y0 = arrow_cy - chevron_drop * 0.5f;
      float y1 = arrow_cy + chevron_drop * 0.5f;
      float cap_r = chevron_thickness * 0.5f;
      sui_renderer_draw_line_thick(r, lx, y0, arrow_cx, y1, chevron_thickness, arrow_color);
      sui_renderer_draw_line_thick(r, arrow_cx, y1, rx, y0, chevron_thickness, arrow_color);
      sui_renderer_fill_ellipse(r, lx, y0, cap_r, arrow_color);
      sui_renderer_fill_ellipse(r, arrow_cx, y1, cap_r, arrow_color);
      sui_renderer_fill_ellipse(r, rx, y0, cap_r, arrow_color);
    }

    /* The closed field keeps keyboard focus while its separate menu is open. */
    sui_ctrl_draw_kb_focus_ring(host, ctrl, rad);
}

static void sui_ctrl_draw_combobox(SuiControlHost* host, SuiControl* ctrl) {
    float s = sui_ctrl_dpi_scale(host);
    sui_ctrl_draw_combobox_with_radius(host, ctrl, sui_combo_radius(s));
}

/* ---- Forward declarations for popup ---------------------------------------- */

static void sui_combo_popup_close(SuiControlHost* host);
static void sui_combo_popup_destroy_internal(SuiControlHost* host);
static void sui_combo_popup_anim_step(SuiComboPopup* popup);

/* ---- Popup placement -------------------------------------------------------- */

static HWND sui_combo_owner_root(SuiControlHost* host) {
    HWND root;
    if (!host || !host->hwnd) return NULL;
    root = GetAncestor(host->hwnd, GA_ROOT);
    return root ? root : host->hwnd;
}

static void sui_combo_popup_place_in_bounds(int ctrl_left, int ctrl_top,
                                            int ctrl_w, int ctrl_h,
                                            int popup_w, int desired_h,
                                            int item_h, int gap,
                                            const RECT* bounds,
                                            LONG* out_x, LONG* out_y,
                                            int* out_h, int* out_opens_above) {
    int below_y, above_bottom, room_below, room_above;
    int opens_above, room, popup_h, x, y;

    if (!bounds || !out_x || !out_y || !out_h || !out_opens_above) return;
    if (ctrl_w < 0) ctrl_w = 0;
    if (ctrl_h < 0) ctrl_h = 0;
    if (popup_w < 1) popup_w = ctrl_w > 0 ? ctrl_w : 1;
    if (desired_h < 1) desired_h = item_h > 0 ? item_h : 1;
    if (item_h < 1) item_h = 1;
    if (gap < 0) gap = 0;

    below_y = ctrl_top + ctrl_h + gap;
    above_bottom = ctrl_top - gap;
    room_below = bounds->bottom - below_y;
    room_above = above_bottom - bounds->top;
    if (room_below < 0) room_below = 0;
    if (room_above < 0) room_above = 0;

    /* Prefer the familiar downward menu. Flip above only when the requested
     * height would cross the work-area bottom and the upper side fits; if
     * neither side fits, use the roomier side and keep whole rows visible. */
    if (room_below >= desired_h) {
        opens_above = 0;
    } else if (room_above >= desired_h) {
        opens_above = 1;
    } else {
        opens_above = room_above > room_below;
    }

    room = opens_above ? room_above : room_below;
    popup_h = desired_h;
    if (popup_h > room) {
        int rows = room / item_h;
        popup_h = rows > 0 ? rows * item_h : room;
    }
    if (popup_h < 1) popup_h = 1;

    x = ctrl_left;
    if (x + popup_w > bounds->right) x = bounds->right - popup_w;
    if (x < bounds->left) x = bounds->left;
    y = opens_above ? above_bottom - popup_h : below_y;
    if (y < bounds->top) y = bounds->top;
    if (y + popup_h > bounds->bottom) y = bounds->bottom - popup_h;

    *out_x = (LONG)x;
    *out_y = (LONG)y;
    *out_h = popup_h;
    *out_opens_above = opens_above;
}

static void sui_combo_popup_compute_placement_at(SuiControl* ctrl,
                                                 LONG client_x, LONG client_y,
                                                 int popup_w, int desired_h,
                                                 int item_h, int gap,
                                                 LONG* out_x, LONG* out_y,
                                                 int* out_h,
                                                 int* out_opens_above) {
    RECT bounds;
    HMONITOR monitor;
    MONITORINFO mi;
    POINT pt;

    pt.x = client_x + (LONG)ctrl->x;
    pt.y = client_y + (LONG)ctrl->y;
    bounds.left = 0;
    bounds.top = 0;
    bounds.right = GetSystemMetrics(SM_CXSCREEN);
    bounds.bottom = GetSystemMetrics(SM_CYSCREEN);
    monitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    if (monitor) {
        memset(&mi, 0, sizeof(mi));
        mi.cbSize = sizeof(mi);
        if (GetMonitorInfoW(monitor, &mi))
            bounds = mi.rcWork;
    }

    sui_combo_popup_place_in_bounds(
        pt.x, pt.y, (int)(ctrl->w + 0.5f), (int)(ctrl->h + 0.5f),
        popup_w, desired_h, item_h, gap, &bounds,
        out_x, out_y, out_h, out_opens_above);
}

static void sui_combo_popup_compute_placement(SuiControlHost* host,
                                              SuiControl* ctrl,
                                              int popup_w, int desired_h,
                                              int item_h, int gap,
                                              LONG* out_x, LONG* out_y,
                                              int* out_h,
                                              int* out_opens_above) {
    POINT client_origin;

    client_origin.x = 0;
    client_origin.y = 0;
    MapWindowPoints(host->hwnd, NULL, &client_origin, 1);
    sui_combo_popup_compute_placement_at(
        ctrl, client_origin.x, client_origin.y,
        popup_w, desired_h, item_h, gap,
        out_x, out_y, out_h, out_opens_above);
}

static void sui_combo_popup_apply_animation_position(SuiComboPopup* popup) {
    int y;
    if (!popup || !popup->hwnd) return;
    y = sui_combo_popup_window_y(popup);
    SetWindowPos(popup->hwnd, NULL, popup->pos_x, y, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                 SWP_NOSENDCHANGING);
    if (popup->shadow_hwnd) {
        SetWindowPos(popup->shadow_hwnd, popup->hwnd,
                     popup->pos_x - popup->shadow_margin,
                     y - popup->shadow_margin, 0, 0,
                     SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER |
                     SWP_NOSENDCHANGING);
    }
}

static void sui_combo_popup_reposition_at(SuiControlHost* host,
                                          LONG client_x, LONG client_y) {
    SuiComboPopup* popup;
    SuiControl* ctrl;
    LONG x, y;
    int popup_h, opens_above;
    int gap;

    if (!host) return;
    popup = host->combo_popup;
    if (!popup || !popup->hwnd) return;
    if (popup->owner_ctrl_index < 0 ||
        popup->owner_ctrl_index >= host->control_count) return;

    ctrl = &host->controls[popup->owner_ctrl_index];
    gap = (int)(sui_combo_popup_gap(sui_ctrl_dpi_scale(host)) + 0.5f);
    sui_combo_popup_compute_placement_at(
        ctrl, client_x, client_y, popup->popup_w, popup->full_window_h,
        (int)(popup->item_height + 0.5f), gap,
        &x, &y, &popup_h, &opens_above);
    popup->pos_x = x;
    popup->anchor_y = y;
    popup->opens_above = opens_above;
    (void)popup_h; /* Open-time sizing already guarantees the menu fits. */
    sui_combo_popup_apply_animation_position(popup);
}

static void sui_combo_popup_reposition(SuiControlHost* host) {
    POINT client_origin;
    if (!host) return;
    client_origin.x = 0;
    client_origin.y = 0;
    MapWindowPoints(host->hwnd, NULL, &client_origin, 1);
    sui_combo_popup_reposition_at(host, client_origin.x, client_origin.y);
}

static void sui_combo_popup_reposition_for_moving(SuiControlHost* host,
                                                  HWND moving_hwnd,
                                                  RECT* moving_rect) {
    RECT current_root;
    POINT current_client;
    LONG client_dx, client_dy;

    if (!host || !moving_rect) return;
    if (!moving_hwnd) moving_hwnd = sui_combo_owner_root(host);
    current_client.x = 0;
    current_client.y = 0;
    MapWindowPoints(host->hwnd, NULL, &current_client, 1);
    GetWindowRect(moving_hwnd, &current_root);
    client_dx = current_client.x - current_root.left;
    client_dy = current_client.y - current_root.top;
    sui_combo_popup_reposition_at(host,
                                  moving_rect->left + client_dx,
                                  moving_rect->top + client_dy);
}

/* ---- Owner-window subclass: keep popup attached / close on invalid geometry - */

#define SUI_COMBO_OWNER_STATE_PROP L"SuiComboOwnerState"
#define SUI_COMBO_OWNER_MAX_HOSTS  32

/* A root HWND can contain more than one Slim UI host (a settings window
 * can embed another panel this way). Keep one subclass per root and fan
 * owner messages out to every open combobox instead of making each host replace
 * the root WndProc/property independently. */
typedef struct SuiComboOwnerState {
    WNDPROC orig_proc;
    SuiControlHost* hosts[SUI_COMBO_OWNER_MAX_HOSTS];
    int host_count;
    int dispatch_depth;
    int pending_free;
} SuiComboOwnerState;

static int sui_combo_owner_state_find(const SuiComboOwnerState* state,
                                      const SuiControlHost* host) {
    int i;
    if (!state || !host) return -1;
    for (i = 0; i < state->host_count; ++i)
        if (state->hosts[i] == host) return i;
    return -1;
}

static LRESULT CALLBACK sui_combo_owner_subclass_proc(HWND hwnd, UINT msg,
                                                      WPARAM wp, LPARAM lp) {
    SuiComboOwnerState* state =
        (SuiComboOwnerState*)GetPropW(hwnd, SUI_COMBO_OWNER_STATE_PROP);
    SuiControlHost* hosts[SUI_COMBO_OWNER_MAX_HOSTS];
    WNDPROC orig;
    int count = 0;
    int free_state = 0;
    int i;

    if (!state) return DefWindowProcW(hwnd, msg, wp, lp);
    orig = state->orig_proc;
    count = state->host_count;
    if (count > SUI_COMBO_OWNER_MAX_HOSTS) count = SUI_COMBO_OWNER_MAX_HOSTS;
    for (i = 0; i < count; ++i) hosts[i] = state->hosts[i];
    state->dispatch_depth++;

    for (i = 0; i < count; ++i) {
        SuiControlHost* host = hosts[i];
        SuiComboPopup* popup = host ? host->combo_popup : NULL;
        if (!popup || !popup->hwnd || !popup->owner_subclass_registered ||
            popup->owner_subclass_hwnd != hwnd)
            continue;

        switch (msg) {
            /* Owner position changed — popup is screen-positioned, so recompute
             * its anchor instead of leaving it behind during a window drag. */
            case WM_MOVING:
                sui_combo_popup_reposition_for_moving(host, hwnd, (RECT*)lp);
                break;
            case WM_WINDOWPOSCHANGED: {
                WINDOWPOS* pos = (WINDOWPOS*)lp;
                if (pos && !(pos->flags & SWP_NOSIZE)) {
                    sui_combo_popup_destroy_internal(host);
                } else if (!pos || !(pos->flags & SWP_NOMOVE)) {
                    sui_combo_popup_reposition(host);
                }
                break;
            }
            case WM_MOVE:
                sui_combo_popup_reposition(host);
                break;

            /* Owner layout changed — control coordinates may be rebuilt, so
             * close immediately rather than using stale control geometry. */
            case WM_SIZING:
            case WM_SIZE:
            case WM_DPICHANGED:
            case WM_DESTROY:
            case WM_NCDESTROY:
                sui_combo_popup_destroy_internal(host);
                break;
        }
    }

    state->dispatch_depth--;
    if (msg == WM_NCDESTROY) {
        RemovePropW(hwnd, SUI_COMBO_OWNER_STATE_PROP);
        state->pending_free = 1;
    }
    if (state->pending_free && state->dispatch_depth == 0) free_state = 1;
    if (free_state) sui_free(state);

    if (orig) return CallWindowProcW(orig, hwnd, msg, wp, lp);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void sui_combo_owner_subclass_install(SuiControlHost* host) {
    SuiComboPopup* popup;
    SuiComboOwnerState* state;
    HWND target;
    WNDPROC current;
    int created = 0;
    if (!host) return;
    popup = host->combo_popup;
    if (!popup || popup->owner_subclass_registered) return;
    target = sui_combo_owner_root(host);
    if (!target) return;

    state = (SuiComboOwnerState*)GetPropW(target, SUI_COMBO_OWNER_STATE_PROP);
    if (!state) {
        current = (WNDPROC)GetWindowLongPtrW(target, GWLP_WNDPROC);
        /* A missing property with our WndProc already installed cannot be
         * recovered without knowing the true predecessor; refuse recursion. */
        if (!current || current == sui_combo_owner_subclass_proc) return;
        state = (SuiComboOwnerState*)sui_malloc(sizeof(*state));
        if (!state) return;
        memset(state, 0, sizeof(*state));
        state->orig_proc = current;
        if (!SetPropW(target, SUI_COMBO_OWNER_STATE_PROP, (HANDLE)state)) {
            sui_free(state);
            return;
        }
        SetLastError(ERROR_SUCCESS);
        if (!SetWindowLongPtrW(target, GWLP_WNDPROC,
                               (LONG_PTR)sui_combo_owner_subclass_proc) &&
            GetLastError() != ERROR_SUCCESS) {
            RemovePropW(target, SUI_COMBO_OWNER_STATE_PROP);
            sui_free(state);
            return;
        }
        created = 1;
    }

    if (sui_combo_owner_state_find(state, host) < 0) {
        if (state->host_count >= SUI_COMBO_OWNER_MAX_HOSTS) {
            if (created) {
                SetWindowLongPtrW(target, GWLP_WNDPROC, (LONG_PTR)state->orig_proc);
                RemovePropW(target, SUI_COMBO_OWNER_STATE_PROP);
                sui_free(state);
            }
            return;
        }
        state->hosts[state->host_count++] = host;
    }
    popup->owner_subclass_hwnd = target;
    popup->owner_subclass_registered = 1;
}

static void sui_combo_owner_subclass_uninstall(SuiControlHost* host) {
    SuiComboPopup* popup;
    SuiComboOwnerState* state;
    HWND target;
    int idx;
    if (!host) return;
    popup = host->combo_popup;
    if (!popup || !popup->owner_subclass_registered) return;
    target = popup->owner_subclass_hwnd ? popup->owner_subclass_hwnd : host->hwnd;
    popup->owner_subclass_hwnd = NULL;
    popup->owner_subclass_registered = 0;
    if (!target) return;

    state = (SuiComboOwnerState*)GetPropW(target, SUI_COMBO_OWNER_STATE_PROP);
    if (!state) return;
    idx = sui_combo_owner_state_find(state, host);
    if (idx >= 0) {
        int i;
        for (i = idx; i + 1 < state->host_count; ++i)
            state->hosts[i] = state->hosts[i + 1];
        state->hosts[--state->host_count] = NULL;
    }

    if (state->host_count == 0 &&
        (WNDPROC)GetWindowLongPtrW(target, GWLP_WNDPROC) ==
            sui_combo_owner_subclass_proc) {
        SetWindowLongPtrW(target, GWLP_WNDPROC, (LONG_PTR)state->orig_proc);
        RemovePropW(target, SUI_COMBO_OWNER_STATE_PROP);
        if (state->dispatch_depth > 0)
            state->pending_free = 1;
        else
            sui_free(state);
    }
}

/* ---- Scrollbar geometry helper ------------------------------------------- */

/* Returns 1 if a scrollbar is needed, and fills out thumb_y/thumb_h/max_scroll. */
static int sui_combo_sb_geom(float item_h, int item_count, float visible_h, float scroll_offset,
                             float s, float* out_thumb_y, float* out_thumb_h,
                             float* out_max_scroll) {
    float total_h    = (float)item_count * item_h;
    float max_scroll = total_h - visible_h;
    if (max_scroll <= 0.5f) { *out_max_scroll = 0.0f; return 0; }
    *out_max_scroll = max_scroll;
    { float thumb_h  = visible_h / total_h * visible_h;
      float min_thumb = 20.0f * s;
      if (thumb_h < min_thumb) thumb_h = min_thumb;
      if (thumb_h > visible_h) thumb_h = visible_h;
      *out_thumb_h = thumb_h;
      *out_thumb_y = scroll_offset / max_scroll * (visible_h - thumb_h);
    }
    return 1;
}

/* ---- Scroll popup to keep item hi visible -------------------------------- */

static void sui_combo_scroll_to_item(SuiComboPopup* popup, int hi, int item_count) {
    RECT prc;
    float vis_h, item_top, item_bot, max_s;
    if (!popup || !popup->hwnd) return;
    GetClientRect(popup->hwnd, &prc);
    vis_h    = (float)(prc.bottom - prc.top);
    item_top = (float)hi * popup->item_height;
    item_bot = item_top + popup->item_height;
    max_s    = (float)item_count * popup->item_height - vis_h;
    if (max_s < 0.0f) max_s = 0.0f;
    if (item_top < popup->scroll_offset)
        popup->scroll_offset = item_top;
    else if (item_bot > popup->scroll_offset + vis_h)
        popup->scroll_offset = item_bot - vis_h;
    if (popup->scroll_offset < 0.0f) popup->scroll_offset = 0.0f;
    if (popup->scroll_offset > max_s) popup->scroll_offset = max_s;
}

static float sui_combo_popup_initial_scroll(int item_count, int selected,
                                            float item_h, float popup_h) {
    float center, max_scroll;
    int first_row;
    if (item_count <= 0 || item_h <= 0.0f || popup_h <= 0.0f)
        return 0.0f;
    if (selected < 0) selected = 0;
    if (selected >= item_count) selected = item_count - 1;

    max_scroll = (float)item_count * item_h - popup_h;
    if (max_scroll <= 0.0f) return 0.0f;
    center = (float)selected * item_h - popup_h * 0.5f + item_h * 0.5f;
    if (center < 0.0f) center = 0.0f;
    if (center > max_scroll) center = max_scroll;

    /* An even-sized viewport cannot center a row exactly. Snap to the nearest
     * full row so neither edge starts with the half rows seen in the popup. */
    first_row = (int)(center / item_h + 0.5f);
    center = (float)first_row * item_h;
    if (center > max_scroll) center = max_scroll;
    return center;
}

static SuiImageHandle sui_combo_load_image_file(SuiRenderer* r, const wchar_t* path) {
    SuiImageHandle img = {0};
#ifdef SUI_FEATURE_PNG
    HANDLE file;
    LARGE_INTEGER size;
    DWORD read_bytes;
    uint8_t* bytes;
    if (!r || !path || !path[0]) return img;
    file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return img;
    size.QuadPart = 0;
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 || size.QuadPart > 16 * 1024 * 1024) {
        CloseHandle(file);
        return img;
    }
    bytes = (uint8_t*)sui_malloc((size_t)size.QuadPart);
    if (!bytes) {
        CloseHandle(file);
        return img;
    }
    read_bytes = 0;
    if (ReadFile(file, bytes, (DWORD)size.QuadPart, &read_bytes, NULL) &&
        read_bytes == (DWORD)size.QuadPart) {
        img = sui_renderer_load_image(r, bytes, (size_t)size.QuadPart);
    }
    sui_free(bytes);
    CloseHandle(file);
#else
    (void)r; (void)path;
#endif
    return img;
}

static void sui_combo_popup_free_icons(SuiComboPopup* popup) {
    int i;
    if (!popup) return;
    if (popup->item_icons) {
        for (i = 0; i < popup->item_icon_count; ++i) {
            if (popup->item_icons[i].data)
                sui_renderer_free_image(&popup->renderer, popup->item_icons[i]);
        }
        sui_free(popup->item_icons);
        popup->item_icons = NULL;
        popup->item_icon_count = 0;
    }
    for (i = 0; i < 16; ++i) {
        if (popup->icon_cache[i].image.data)
            sui_renderer_free_image(&popup->renderer, popup->icon_cache[i].image);
        popup->icon_cache[i].image.data = NULL;
        popup->icon_cache[i].image.width = 0;
        popup->icon_cache[i].image.height = 0;
        popup->icon_cache[i].index = -1;
        if (popup->cpu_icon_cache[i].pixels)
            sui_free(popup->cpu_icon_cache[i].pixels);
        popup->cpu_icon_cache[i].pixels = NULL;
        popup->cpu_icon_cache[i].width = 0;
        popup->cpu_icon_cache[i].height = 0;
        popup->cpu_icon_cache[i].index = -1;
    }
    popup->icon_cache_next = 0;
    popup->cpu_icon_cache_next = 0;
}

static void sui_combo_popup_load_icons(SuiComboPopup* popup, SuiControl* ctrl) {
    int i, n;
    if (!popup || !ctrl || !ctrl->d.combo.item_icon_paths) return;
    if (sui_ctrl_combo_is_virtual(ctrl)) return;
    n = ctrl->d.combo.item_count;
    if (n <= 0) return;
    popup->item_icons = (SuiImageHandle*)sui_malloc((size_t)n * sizeof(SuiImageHandle));
    if (!popup->item_icons) return;
    memset(popup->item_icons, 0, (size_t)n * sizeof(SuiImageHandle));
    popup->item_icon_count = n;
    for (i = 0; i < n; ++i) {
        if (ctrl->d.combo.item_icon_paths[i][0])
            popup->item_icons[i] = sui_combo_load_image_file(&popup->renderer, ctrl->d.combo.item_icon_paths[i]);
    }
}

static SuiImageHandle sui_combo_popup_icon_for_item(SuiComboPopup* popup, int item_index,
                                                    const wchar_t* icon_path) {
    int i, slot;
    if (!popup || !icon_path || !icon_path[0]) {
        SuiImageHandle empty = {0};
        return empty;
    }
    for (i = 0; i < 16; ++i) {
        if (popup->icon_cache[i].index == item_index &&
            popup->icon_cache[i].image.data)
            return popup->icon_cache[i].image;
    }
    slot = popup->icon_cache_next;
    if (slot < 0 || slot >= 16) slot = 0;
    if (popup->icon_cache[slot].image.data)
        sui_renderer_free_image(&popup->renderer, popup->icon_cache[slot].image);
    popup->icon_cache[slot].image = sui_combo_load_image_file(&popup->renderer, icon_path);
    popup->icon_cache[slot].index = popup->icon_cache[slot].image.data ? item_index : -1;
    if (popup->icon_cache[slot].image.data)
        popup->icon_cache_next = (slot + 1) % 16;
    return popup->icon_cache[slot].image;
}

static int sui_combo_load_cpu_icon_file(SuiComboPopupCpuIconSlot* out_icon,
                                        const wchar_t* path) {
#ifdef SUI_FEATURE_PNG
    HANDLE file;
    LARGE_INTEGER size;
    DWORD read_bytes;
    uint8_t* bytes;
    HGLOBAL hg;
    void* hg_ptr;
    IStream* stream = NULL;
    IWICImagingFactory* wic = NULL;
    IWICBitmapDecoder* decoder = NULL;
    IWICBitmapFrameDecode* frame = NULL;
    IWICFormatConverter* converter = NULL;
    UINT32 w = 0, h = 0;
    UINT32 stride, byte_count;
    uint8_t* pixels = NULL;
    HRESULT hr;
    int ok = 0;

    if (!out_icon || !path || !path[0]) return 0;
    file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return 0;
    size.QuadPart = 0;
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 || size.QuadPart > 16 * 1024 * 1024) {
        CloseHandle(file);
        return 0;
    }
    bytes = (uint8_t*)sui_malloc((size_t)size.QuadPart);
    if (!bytes) {
        CloseHandle(file);
        return 0;
    }
    read_bytes = 0;
    if (!ReadFile(file, bytes, (DWORD)size.QuadPart, &read_bytes, NULL) ||
        read_bytes != (DWORD)size.QuadPart) {
        sui_free(bytes);
        CloseHandle(file);
        return 0;
    }
    CloseHandle(file);

    hg = GlobalAlloc(GMEM_MOVEABLE, (SIZE_T)size.QuadPart);
    if (!hg) { sui_free(bytes); return 0; }
    hg_ptr = GlobalLock(hg);
    if (!hg_ptr) { GlobalFree(hg); sui_free(bytes); return 0; }
    memcpy(hg_ptr, bytes, (size_t)size.QuadPart);
    GlobalUnlock(hg);
    sui_free(bytes);

    hr = CreateStreamOnHGlobal(hg, TRUE, &stream);
    if (FAILED(hr) || !stream) { GlobalFree(hg); return 0; }

    hr = CoCreateInstance(SUI_IID_REF(CLSID_WICImagingFactory), NULL, CLSCTX_INPROC_SERVER,
                          SUI_IID_REF(IID_IWICImagingFactory), (void**)&wic);
    if (FAILED(hr) || !wic) goto cleanup;
    hr = wic->lpVtbl->CreateDecoderFromStream(wic, stream, NULL,
             (DWORD)WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr) || !decoder) goto cleanup;
    hr = decoder->lpVtbl->GetFrame(decoder, 0, &frame);
    if (FAILED(hr) || !frame) goto cleanup;
    hr = wic->lpVtbl->CreateFormatConverter(wic, &converter);
    if (FAILED(hr) || !converter) goto cleanup;
    hr = converter->lpVtbl->Initialize(converter, (IWICBitmapSource*)frame,
             &GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, NULL, 0.0,
             WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) goto cleanup;
    hr = converter->lpVtbl->Base.GetSize((IWICBitmapSource*)converter, &w, &h);
    if (FAILED(hr) || w == 0 || h == 0 || w > 1024 || h > 1024) goto cleanup;
    stride = w * 4u;
    byte_count = stride * h;
    pixels = (uint8_t*)sui_malloc((size_t)byte_count);
    if (!pixels) goto cleanup;
    hr = converter->lpVtbl->Base.CopyPixels((IWICBitmapSource*)converter, NULL,
                                            stride, byte_count, pixels);
    if (FAILED(hr)) goto cleanup;
    out_icon->pixels = pixels;
    out_icon->width = (int)w;
    out_icon->height = (int)h;
    pixels = NULL;
    ok = 1;

cleanup:
    if (pixels) sui_free(pixels);
    if (converter) converter->lpVtbl->Base.Release((IWICBitmapSource*)converter);
    if (frame)     frame->lpVtbl->Base.Release((IWICBitmapSource*)frame);
    if (decoder)   decoder->lpVtbl->Release(decoder);
    if (wic)       wic->lpVtbl->Release(wic);
    if (stream) {
#ifdef __cplusplus
        ((::IStream*)stream)->Release();
#else
        ((IUnknown*)stream)->lpVtbl->Release((IUnknown*)stream);
#endif
    }
    return ok;
#else
    (void)out_icon; (void)path;
    return 0;
#endif
}

static SuiComboPopupCpuIconSlot* sui_combo_popup_cpu_icon_cached(SuiComboPopup* popup,
                                                                 int item_index) {
    int i;
    if (!popup) return NULL;
    for (i = 0; i < 16; ++i) {
        if (popup->cpu_icon_cache[i].index == item_index &&
            popup->cpu_icon_cache[i].pixels)
            return &popup->cpu_icon_cache[i];
    }
    return NULL;
}

static SuiComboPopupCpuIconSlot* sui_combo_popup_cpu_icon_for_item(SuiComboPopup* popup,
                                                                  int item_index,
                                                                  const wchar_t* icon_path) {
    int slot;
    SuiComboPopupCpuIconSlot* cached;
    if (!popup || !icon_path || !icon_path[0]) return NULL;
    cached = sui_combo_popup_cpu_icon_cached(popup, item_index);
    if (cached) return cached;
    slot = popup->cpu_icon_cache_next;
    if (slot < 0 || slot >= 16) slot = 0;
    if (popup->cpu_icon_cache[slot].pixels)
        sui_free(popup->cpu_icon_cache[slot].pixels);
    popup->cpu_icon_cache[slot].pixels = NULL;
    popup->cpu_icon_cache[slot].width = 0;
    popup->cpu_icon_cache[slot].height = 0;
    popup->cpu_icon_cache[slot].index = -1;
    if (!sui_combo_load_cpu_icon_file(&popup->cpu_icon_cache[slot], icon_path))
        return NULL;
    popup->cpu_icon_cache[slot].index = item_index;
    popup->cpu_icon_cache_next = (slot + 1) % 16;
    return &popup->cpu_icon_cache[slot];
}

static void sui_combo_popup_prepare_visible_icons(HWND hwnd, SuiComboPopup* popup) {
    RECT rc;
    SuiControlHost* host;
    SuiControl* ctrl;
    float item_h, visible_h;
    int first_i, last_i, item_count, i;

    if (!hwnd || !popup || !popup->owner_host) return;
    host = popup->owner_host;
    if (popup->owner_ctrl_index < 0 || popup->owner_ctrl_index >= host->control_count)
        return;
    ctrl = &host->controls[popup->owner_ctrl_index];
    if (!sui_ctrl_combo_is_virtual(ctrl)) return;

    GetClientRect(hwnd, &rc);
    item_h = popup->item_height;
    visible_h = (float)(rc.bottom - rc.top);
    item_count = sui_ctrl_combo_count(ctrl);
    if (item_h <= 0.0f || visible_h <= 0.0f || item_count <= 0) return;

    first_i = (int)(popup->scroll_offset / item_h);
    if (first_i < 0) first_i = 0;
    last_i = (int)((popup->scroll_offset + visible_h) / item_h) + 1;
    if (last_i > item_count) last_i = item_count;

    for (i = first_i; i < last_i; i++) {
        SuiComboItemView item;
        sui_ctrl_combo_get_item_view(ctrl, i, &item);
        if (item.icon_path[0]) {
            if (popup->layered)
                (void)sui_combo_popup_cpu_icon_for_item(popup, i, item.icon_path);
            else
                (void)sui_combo_popup_icon_for_item(popup, i, item.icon_path);
        }
    }
}

/* ---- Popup paint ----------------------------------------------------------- */

static void sui_combo_popup_draw(HWND hwnd, SuiComboPopup* popup, int clear_opaque) {
    RECT rc;
    SuiControlHost* host;
    SuiControl* ctrl;
    SuiRenderer* r;
    const SuiColorScheme* sc;
    float s, pad, text_lpad, item_h, visible_h, visible_top, pw, sb_w;
    float thumb_y, thumb_h, max_scroll, radius;
    int i, first_i, last_i, item_count, need_sb, is_focused, highlighted_item;

    host = popup->owner_host;
    if (!host || popup->owner_ctrl_index < 0
              || popup->owner_ctrl_index >= host->control_count) return;

    ctrl = &host->controls[popup->owner_ctrl_index];
    r    = &popup->renderer;
    sc   = host->scheme;

    s         = (float)sui_get_window_dpi(hwnd) / 96.0f;
    pad       = 8.0f * s;
    /* Matches closed combo button — no visual jump on open. */
    text_lpad = 10.0f * s;
    item_h    = popup->item_height;
    GetClientRect(hwnd, &rc);
    pw        = (float)(rc.right - rc.left);
    visible_h = sui_combo_popup_visible_height(popup);
    visible_top = sui_combo_popup_visible_top(popup, visible_h);
    sb_w      = 4.0f * s;
    radius    = popup->corner_radius > 0.0f ? popup->corner_radius : sui_combo_radius(s);
    is_focused = (host->focused_control == popup->owner_ctrl_index);

    item_count = sui_ctrl_combo_count(ctrl);
    highlighted_item = sui_combo_popup_highlighted_item(
        ctrl->d.combo.selected, popup->hover_item, item_count);
    need_sb = sui_combo_sb_geom(item_h, item_count, visible_h,
                                popup->scroll_offset, s,
                                &thumb_y, &thumb_h, &max_scroll);

    if (clear_opaque)
        sui_renderer_clear(r, sui_ctrl_style_color(ctrl, SUI_STYLE_BG_COLOR, sc->input_bg));
    sui_renderer_fill_rounded_rect(r, 0.0f, visible_top, pw, visible_h, radius,
        sui_ctrl_style_color(ctrl, SUI_STYLE_BG_COLOR, sc->input_bg));

    first_i = (int)(popup->scroll_offset / item_h);
    if (first_i < 0) first_i = 0;
    last_i = (int)((popup->scroll_offset + visible_h) / item_h) + 1;
    if (last_i > item_count) last_i = item_count;
    for (i = first_i; i < last_i; i++) {
        float iy = visible_top + (float)i * item_h - popup->scroll_offset;
        SuiComboItemView item;
        if (iy + item_h < visible_top || iy > visible_top + visible_h) continue;
        sui_ctrl_combo_get_item_view(ctrl, i, &item);

        if (i == highlighted_item)
            sui_renderer_fill_rect(r, 0.0f, iy, pw, item_h,
                sui_ctrl_style_color(ctrl, SUI_STYLE_HOVER_COLOR, sc->nav_hover));

        {
            ID2D1RenderTarget* rt = (ID2D1RenderTarget*)r->render_target;
            float text_x = text_lpad;
            float text_right = pw - pad - (need_sb ? (sb_w + 6.0f * s) : 0.0f);
            SuiImageHandle icon = {0};
            SuiComboPopupCpuIconSlot* cpu_icon = NULL;
            if (popup->layered && item.icon_path[0])
                cpu_icon = sui_combo_popup_cpu_icon_cached(popup, i);
            if (!popup->layered && popup->item_icons && i < popup->item_icon_count) {
                icon = popup->item_icons[i];
            } else if (!popup->layered && sui_ctrl_combo_is_virtual(ctrl) && item.icon_path[0]) {
                icon = sui_combo_popup_icon_for_item(popup, i, item.icon_path);
            }
            if (item.has_icon) {
                float icon_size = 18.0f * s;
                float icon_gap = 8.0f * s;
                float icon_y = iy + (item_h - icon_size) * 0.5f;
                if (icon.data) {
                    sui_renderer_draw_image(r, icon, text_x, icon_y, icon_size, icon_size);
                } else if (!cpu_icon) {
                    uint32_t text_color = sui_ctrl_style_color(ctrl, SUI_STYLE_TEXT_COLOR, sc->text);
                    sui_combo_draw_fallback_icon(r, text_x, icon_y, icon_size,
                        sui_color_lerp(sui_ctrl_style_color(ctrl, SUI_STYLE_BG_COLOR, sc->input_bg),
                                       text_color, 0.10f),
                        text_color);
                }
                text_x += icon_size + icon_gap;
            }
            if (rt && text_right > text_x) {
                D2D1_RECT_F clip;
                clip.left = text_x;
                clip.top = iy;
                clip.right = text_right;
                clip.bottom = iy + item_h;
                rt->lpVtbl->PushAxisAlignedClip(rt, &clip, 0);
                sui_combo_draw_text_middle_single_line(r, item.text,
                    text_x, iy, text_right - text_x, item_h,
                    sui_ctrl_style_color(ctrl, SUI_STYLE_TEXT_COLOR, sc->text),
                    (ctrl->state & SUI_CTRL_BOLD) ? 1 : 0,
                    sui_ctrl_style_font_size(ctrl, r->font_size),
                    sui_ctrl_style_font_family(ctrl));
                rt->lpVtbl->PopAxisAlignedClip(rt);
            }
        }
    }

    /* Paint the scroll affordance with the popup's very first frame. Once the
     * opening animation settles it can fade normally when the pointer is out. */
    if (need_sb) {
        float reveal = sui_combo_popup_scrollbar_opacity(popup);
        if (reveal > 0.0f) {
            float sb_x  = pw - sb_w - 2.0f * s;
            float sb_rad = sb_w / 2.0f;
            float inset  = 3.0f * s;
            uint32_t thumb_color = sui_ctrl_style_color(
                ctrl, SUI_STYLE_BORDER_COLOR,
                sui_color_lerp(sc->check_border, sc->text, 0.16f));
            sui_renderer_fill_rounded_rect_alpha(r, sb_x, visible_top + thumb_y + inset,
                sb_w, thumb_h - inset * 2.0f, sb_rad,
                thumb_color, reveal);
        }
    }

    /* Outline the open list with the same selected/focus stroke color used by
     * active inputs, while keyboard focus itself remains on the closed field. */
    sui_renderer_draw_rounded_rect(r, 0.5f, visible_top + 0.5f, pw - 1.0f, visible_h - 1.0f,
        radius, sui_ctrl_style_color(ctrl, SUI_STYLE_ACCENT_COLOR, sc->input_focus_border), 1.0f);
}

static void sui_combo_popup_paint(HWND hwnd, SuiComboPopup* popup) {
    if (!popup) return;
    sui_renderer_begin(&popup->renderer);
    sui_combo_popup_draw(hwnd, popup, 1);
    sui_renderer_end(&popup->renderer);
}

static void sui_combo_popup_mask_rounded_alpha(void* bits, int w, int h, float radius,
                                              float visible_top, float visible_h) {
    uint8_t* px = (uint8_t*)bits;
    float r, rr, left_c, right_c, top_c, bottom_c, visible_bottom;
    int x, y;
    static const float samples[4][2] = {
        {0.25f, 0.25f}, {0.75f, 0.25f}, {0.25f, 0.75f}, {0.75f, 0.75f}
    };

    if (!px || w <= 0 || h <= 0 || radius <= 0.0f || visible_h <= 0.0f) return;
    if (visible_top < 0.0f) visible_top = 0.0f;
    visible_bottom = visible_top + visible_h;
    if (visible_bottom > (float)h) visible_bottom = (float)h;
    if (visible_top >= visible_bottom) return;
    visible_h = visible_bottom - visible_top;
    r = radius;
    if (r > (float)w * 0.5f) r = (float)w * 0.5f;
    if (r > visible_h * 0.5f) r = visible_h * 0.5f;
    rr = r * r;
    left_c = r;
    right_c = (float)w - r;
    top_c = visible_top + r;
    bottom_c = visible_bottom - r;

    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            int si, inside = 0;
            uint8_t* p = px + ((size_t)y * (size_t)w + (size_t)x) * 4u;
            for (si = 0; si < 4; si++) {
                float sx = (float)x + samples[si][0];
                float sy = (float)y + samples[si][1];
                float dx = 0.0f;
                float dy = 0.0f;
                if (sy < visible_top || sy >= visible_bottom)
                    continue;
                if (sx < left_c) dx = left_c - sx;
                else if (sx > right_c) dx = sx - right_c;
                if (sy < top_c) dy = top_c - sy;
                else if (sy > bottom_c) dy = sy - bottom_c;
                if (dx == 0.0f || dy == 0.0f || dx * dx + dy * dy <= rr)
                    inside++;
            }
            if (inside == 0) {
                p[0] = p[1] = p[2] = p[3] = 0;
            } else if (inside < 4) {
                unsigned int b = p[0], g = p[1], rch = p[2], a = p[3];
                p[0] = (uint8_t)((b * (unsigned int)inside + 2u) / 4u);
                p[1] = (uint8_t)((g * (unsigned int)inside + 2u) / 4u);
                p[2] = (uint8_t)((rch * (unsigned int)inside + 2u) / 4u);
                p[3] = (uint8_t)((a * (unsigned int)inside + 2u) / 4u);
            }
        }
    }
}

static void sui_combo_popup_blend_cpu_icon(uint8_t* dst, int dst_w, int dst_h,
                                           const SuiComboPopupCpuIconSlot* icon,
                                           int left, int top, int size_px) {
    int x, y;
    if (!dst || !icon || !icon->pixels || dst_w <= 0 || dst_h <= 0 ||
        icon->width <= 0 || icon->height <= 0 || size_px <= 0)
        return;
    for (y = 0; y < size_px; y++) {
        int dy = top + y;
        int sy;
        if (dy < 0 || dy >= dst_h) continue;
        sy = (y * icon->height) / size_px;
        if (sy < 0) sy = 0;
        if (sy >= icon->height) sy = icon->height - 1;
        for (x = 0; x < size_px; x++) {
            int dx = left + x;
            int sx;
            const uint8_t* s;
            uint8_t* d;
            unsigned int sa, inv_a;
            if (dx < 0 || dx >= dst_w) continue;
            sx = (x * icon->width) / size_px;
            if (sx < 0) sx = 0;
            if (sx >= icon->width) sx = icon->width - 1;
            s = icon->pixels + ((size_t)sy * (size_t)icon->width + (size_t)sx) * 4u;
            sa = s[3];
            if (sa == 0) continue;
            d = dst + ((size_t)dy * (size_t)dst_w + (size_t)dx) * 4u;
            if (sa == 255) {
                d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = 255;
            } else {
                inv_a = 255u - sa;
                d[0] = (uint8_t)(s[0] + (d[0] * inv_a + 127u) / 255u);
                d[1] = (uint8_t)(s[1] + (d[1] * inv_a + 127u) / 255u);
                d[2] = (uint8_t)(s[2] + (d[2] * inv_a + 127u) / 255u);
                d[3] = (uint8_t)(sa + (d[3] * inv_a + 127u) / 255u);
            }
        }
    }
}

static void sui_combo_popup_composite_visible_cpu_icons(HWND hwnd, SuiComboPopup* popup,
                                                        void* bits, int w, int h) {
    RECT rc;
    SuiControlHost* host;
    SuiControl* ctrl;
    float s, text_lpad, item_h, visible_h, icon_size;
    int first_i, last_i, item_count, i, icon_px;

    if (!hwnd || !popup || !bits || !popup->owner_host) return;
    host = popup->owner_host;
    if (popup->owner_ctrl_index < 0 || popup->owner_ctrl_index >= host->control_count)
        return;
    ctrl = &host->controls[popup->owner_ctrl_index];
    GetClientRect(hwnd, &rc);
    s = (float)sui_get_window_dpi(hwnd) / 96.0f;
    text_lpad = 10.0f * s;
    item_h = popup->item_height;
    visible_h = (float)(rc.bottom - rc.top);
    item_count = sui_ctrl_combo_count(ctrl);
    icon_size = 18.0f * s;
    icon_px = (int)(icon_size + 0.5f);
    if (item_h <= 0.0f || visible_h <= 0.0f || item_count <= 0 || icon_px <= 0)
        return;

    first_i = (int)(popup->scroll_offset / item_h);
    if (first_i < 0) first_i = 0;
    last_i = (int)((popup->scroll_offset + visible_h) / item_h) + 1;
    if (last_i > item_count) last_i = item_count;

    for (i = first_i; i < last_i; i++) {
        float iy = (float)i * item_h - popup->scroll_offset;
        SuiComboItemView item;
        SuiComboPopupCpuIconSlot* icon;
        if (iy + item_h < 0 || iy > visible_h) continue;
        sui_ctrl_combo_get_item_view(ctrl, i, &item);
        if (!item.icon_path[0]) continue;
        icon = sui_combo_popup_cpu_icon_cached(popup, i);
        if (!icon) continue;
        sui_combo_popup_blend_cpu_icon((uint8_t*)bits, w, h, icon,
            (int)(text_lpad + 0.5f),
            (int)(iy + (item_h - icon_size) * 0.5f + 0.5f),
            icon_px);
    }
}

static int sui_combo_popup_render_layered(HWND hwnd, SuiComboPopup* popup) {
    SuiRenderer* r;
    HDC screen_dc, mem_dc;
    HBITMAP bmp, old_bmp;
    BITMAPINFO bi;
    void* bits;
    RECT wr, cr, bind_rc;
    POINT dst, src;
    SIZE size;
    BLENDFUNCTION blend;
    ID2D1RenderTarget* rt;
    HRESULT hr;
    int w, h, ok = 0;

    if (!hwnd || !popup) return 0;
    r = &popup->renderer;
    if (!r->render_target) return 0;
    GetClientRect(hwnd, &cr);
    w = cr.right - cr.left;
    h = cr.bottom - cr.top;
    if (w <= 0 || h <= 0) return 0;

    screen_dc = GetDC(NULL);
    if (!screen_dc) return 0;
    mem_dc = CreateCompatibleDC(screen_dc);
    if (!mem_dc) { ReleaseDC(NULL, screen_dc); return 0; }

    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    bmp = CreateDIBSection(screen_dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!bmp) {
        DeleteDC(mem_dc);
        ReleaseDC(NULL, screen_dc);
        return 0;
    }
    old_bmp = (HBITMAP)SelectObject(mem_dc, bmp);
    memset(bits, 0, (size_t)w * (size_t)h * 4u);

    bind_rc.left = 0;
    bind_rc.top = 0;
    bind_rc.right = w;
    bind_rc.bottom = h;
    if (SUCCEEDED(((ID2D1DCRenderTarget*)r->render_target)->lpVtbl->BindDC(
            (ID2D1DCRenderTarget*)r->render_target, mem_dc, &bind_rc))) {
        sui_combo_popup_prepare_visible_icons(hwnd, popup);
        rt = (ID2D1RenderTarget*)r->render_target;
        rt->lpVtbl->BeginDraw(rt);
        sui_combo_popup_draw(hwnd, popup, 0);
        hr = rt->lpVtbl->EndDraw(rt, NULL, NULL);
        if (SUCCEEDED(hr)) {
            float visible_h = sui_combo_popup_visible_height(popup);
            float visible_top = sui_combo_popup_visible_top(popup, visible_h);
            sui_combo_popup_composite_visible_cpu_icons(hwnd, popup, bits, w, h);
            sui_combo_popup_mask_rounded_alpha(bits, w, h, popup->corner_radius,
                                               visible_top, visible_h);
            GetWindowRect(hwnd, &wr);
            dst.x = wr.left;
            dst.y = wr.top;
            src.x = 0;
            src.y = 0;
            size.cx = w;
            size.cy = h;
            blend.BlendOp = AC_SRC_OVER;
            blend.BlendFlags = 0;
            blend.SourceConstantAlpha = (BYTE)sui_combo_popup_alpha(popup);
            blend.AlphaFormat = AC_SRC_ALPHA;
            ok = UpdateLayeredWindow(hwnd, screen_dc, &dst, &size,
                mem_dc, &src, 0, &blend, ULW_ALPHA) ? 1 : 0;
        }
    }

    SelectObject(mem_dc, old_bmp);
    DeleteObject(bmp);
    DeleteDC(mem_dc);
    ReleaseDC(NULL, screen_dc);
    return ok;
}

static int sui_combo_popup_render_shadow(SuiComboPopup* popup) {
    SuiRenderer* r;
    HDC screen_dc, mem_dc;
    HBITMAP bmp, old_bmp;
    BITMAPINFO bi;
    void* bits;
    RECT wr, bind_rc;
    POINT dst, src;
    SIZE size;
    BLENDFUNCTION blend;
    ID2D1RenderTarget* rt;
    ID2D1SolidColorBrush* brush = NULL;
    D2D1_COLOR_F clear_color, shadow_color;
    D2D1_ROUNDED_RECT rr;
    HRESULT hr;
    float left, top, blur;
    int margin, w, h, layer_count, i, drew_shadow = 0, ok = 0;

    if (!popup || !popup->shadow_hwnd || popup->popup_w <= 0 ||
        popup->full_window_h <= 0)
        return 0;
    r = &popup->renderer;
    if (!r->render_target) return 0;
    margin = popup->shadow_margin;
    if (margin < 1) return 0;
    w = popup->popup_w + margin * 2;
    h = popup->full_window_h + margin * 2;
    left = (float)margin;
    top = (float)(margin + popup->shadow_offset_y);
    blur = (float)(margin - popup->shadow_offset_y);
    if (blur < 1.0f) blur = 1.0f;

    screen_dc = GetDC(NULL);
    if (!screen_dc) return 0;
    mem_dc = CreateCompatibleDC(screen_dc);
    if (!mem_dc) {
        ReleaseDC(NULL, screen_dc);
        return 0;
    }

    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    bmp = CreateDIBSection(screen_dc, &bi, DIB_RGB_COLORS,
                          (void**)&bits, NULL, 0);
    if (!bmp) {
        DeleteDC(mem_dc);
        ReleaseDC(NULL, screen_dc);
        return 0;
    }
    old_bmp = (HBITMAP)SelectObject(mem_dc, bmp);
    memset(bits, 0, (size_t)w * (size_t)h * 4u);

    bind_rc.left = 0;
    bind_rc.top = 0;
    bind_rc.right = w;
    bind_rc.bottom = h;
    if (SUCCEEDED(((ID2D1DCRenderTarget*)r->render_target)->lpVtbl->BindDC(
            (ID2D1DCRenderTarget*)r->render_target, mem_dc, &bind_rc))) {
        rt = (ID2D1RenderTarget*)r->render_target;
        clear_color.r = clear_color.g = clear_color.b = clear_color.a = 0.0f;
        shadow_color.r = shadow_color.g = shadow_color.b = 0.0f;
        shadow_color.a = 0.0f;
        layer_count = (int)(blur + 0.5f);
        if (layer_count < 4) layer_count = 4;
        if (layer_count > 32) layer_count = 32;

        rt->lpVtbl->BeginDraw(rt);
        rt->lpVtbl->Clear(rt, &clear_color);
        if (SUCCEEDED(rt->lpVtbl->CreateSolidColorBrush(
                rt, &shadow_color, NULL, &brush)) && brush) {
            for (i = 0; i < layer_count; i++) {
                float t = (float)(i + 1) / (float)layer_count;
                float spread = blur * (1.0f - t);
                shadow_color.a = sui_combo_popup_shadow_layer_opacity(
                    i, layer_count);
                brush->lpVtbl->SetColor(brush, &shadow_color);
                rr.rect.left = left - spread;
                rr.rect.top = top - spread;
                rr.rect.right = left + (float)popup->popup_w + spread;
                rr.rect.bottom = top + (float)popup->full_window_h + spread;
                rr.radiusX = popup->corner_radius + spread;
                rr.radiusY = popup->corner_radius + spread;
                sui__fill_round_once(r, rr, (ID2D1Brush*)brush);
            }
            drew_shadow = 1;
            brush->lpVtbl->Base.Base.Release((IUnknown*)brush);
            brush = NULL;
        }
        hr = rt->lpVtbl->EndDraw(rt, NULL, NULL);
        if (drew_shadow && SUCCEEDED(hr)) {
            GetWindowRect(popup->shadow_hwnd, &wr);
            dst.x = wr.left;
            dst.y = wr.top;
            src.x = 0;
            src.y = 0;
            size.cx = w;
            size.cy = h;
            blend.BlendOp = AC_SRC_OVER;
            blend.BlendFlags = 0;
            blend.SourceConstantAlpha = (BYTE)sui_combo_popup_alpha(popup);
            blend.AlphaFormat = AC_SRC_ALPHA;
            ok = UpdateLayeredWindow(popup->shadow_hwnd, screen_dc,
                &dst, &size, mem_dc, &src, 0, &blend, ULW_ALPHA) ? 1 : 0;
        }
    }

    SelectObject(mem_dc, old_bmp);
    DeleteObject(bmp);
    DeleteDC(mem_dc);
    ReleaseDC(NULL, screen_dc);
    return ok;
}

static void sui_combo_popup_render_now(HWND hwnd, SuiComboPopup* popup) {
    if (!hwnd || !popup) return;
    if (popup->shadow_hwnd)
        sui_combo_popup_render_shadow(popup);
    if (popup->layered)
        sui_combo_popup_render_layered(hwnd, popup);
    else
        sui_combo_popup_paint(hwnd, popup);
    ValidateRect(hwnd, NULL);
}

/* ---- Popup WndProc --------------------------------------------------------- */

static LRESULT CALLBACK sui_combo_popup_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    SuiComboPopup* popup = (SuiComboPopup*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;

    case WM_NCHITTEST: {
        POINT pt;
        if (!popup) return HTTRANSPARENT;
        pt.x = (int)(short)LOWORD(lp);
        pt.y = (int)(short)HIWORD(lp);
        ScreenToClient(hwnd, &pt);
        return sui_combo_popup_point_visible(popup, (float)pt.x, (float)pt.y)
            ? HTCLIENT
            : HTTRANSPARENT;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        if (popup) sui_combo_popup_render_now(hwnd, popup);
        return 0;
    }

    case WM_MOUSEMOVE: {
        int mx, my, new_hover;
        float s, item_h, visible_h, visible_top, thumb_y, thumb_h, max_scroll;
        float sb_w, pw;
        SuiControlHost* host;
        SuiControl* ctrl;
        int item_count;
        if (!popup) return 0;
        if (!popup->mouse_inside) {
            popup->mouse_inside = 1;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        sui_combo_popup_track_mouse_leave(popup);
        /* Suppress hover until the cursor actually moves — Windows sends a
         * synthetic WM_MOUSEMOVE when the popup appears under a stationary
         * cursor and we don't want that to highlight an item. */
        if (!popup->mouse_armed) {
            POINT cp; GetCursorPos(&cp);
            if (cp.x == popup->open_cursor_x && cp.y == popup->open_cursor_y)
                return 0;
            popup->mouse_armed = 1;
        }
        host = popup->owner_host;
        if (!host || popup->owner_ctrl_index < 0
                  || popup->owner_ctrl_index >= host->control_count)
            return 0;
        ctrl = &host->controls[popup->owner_ctrl_index];
        item_count = sui_ctrl_combo_count(ctrl);
        mx = (int)(short)LOWORD(lp);
        my = (int)(short)HIWORD(lp);
        s         = (float)sui_get_window_dpi(hwnd) / 96.0f;
        item_h    = popup->item_height;
        visible_h = sui_combo_popup_visible_height(popup);
        visible_top = sui_combo_popup_visible_top(popup, visible_h);
        pw        = (float)popup->popup_w;
        sb_w      = 4.0f * s;

        if (popup->dragging) {
            float total_h  = (float)item_count * item_h;
            float ms       = total_h - visible_h;
            float th, range, dy;
            if (ms < 0.0f) ms = 0.0f;
            th = visible_h / total_h * visible_h;
            if (th < 20.0f * s) th = 20.0f * s;
            if (th > visible_h)  th = visible_h;
            range = visible_h - th;
            dy    = ((float)my - visible_top) - popup->drag_start_y;
            if (range > 0.5f)
                popup->scroll_offset = popup->drag_start_offset + dy * ms / range;
            if (popup->scroll_offset < 0.0f) popup->scroll_offset = 0.0f;
            if (popup->scroll_offset > ms)   popup->scroll_offset = ms;
            popup->hover_item = -1;
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }

        /* Suppress item hover when cursor is over scrollbar zone */
        if (sui_combo_sb_geom(item_h, item_count, visible_h,
                               popup->scroll_offset, s,
                               &thumb_y, &thumb_h, &max_scroll)) {
            if ((float)mx >= pw - sb_w - 4.0f * s) {
                if (popup->hover_item != -1) {
                    popup->hover_item = -1;
                    InvalidateRect(hwnd, NULL, FALSE);
                }
                return 0;
            }
        }

        my = (int)((float)my - visible_top);
        new_hover = (int)((float)(my + (int)popup->scroll_offset) / item_h);
        if (new_hover < 0) new_hover = 0;
        if (new_hover >= item_count) new_hover = item_count - 1;

        if (new_hover != popup->hover_item) {
            popup->hover_item = new_hover;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }

    case WM_MOUSELEAVE:
        if (popup) {
            popup->mouse_inside = 0;
            popup->tracking_mouse_leave = 0;
            if (popup->hover_item != -1) popup->hover_item = -1;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;

    case WM_LBUTTONDOWN: {
        int mx, my;
        float s, item_h, visible_h, visible_top, thumb_y, thumb_h, max_scroll, pw;
        SuiControlHost* host;
        SuiControl* ctrl;
        int item_count;
        if (!popup) return 0;
        popup->opening = 0;
        host = popup->owner_host;
        if (!host || popup->owner_ctrl_index < 0
                  || popup->owner_ctrl_index >= host->control_count)
            return 0;
        ctrl = &host->controls[popup->owner_ctrl_index];
        item_count = sui_ctrl_combo_count(ctrl);
        mx = (int)(short)LOWORD(lp);
        my = (int)(short)HIWORD(lp);
        s         = (float)sui_get_window_dpi(hwnd) / 96.0f;
        item_h    = popup->item_height;
        visible_h = sui_combo_popup_visible_height(popup);
        visible_top = sui_combo_popup_visible_top(popup, visible_h);
        pw        = (float)popup->popup_w;

        if (!sui_combo_sb_geom(item_h, item_count, visible_h,
                                popup->scroll_offset, s,
                                &thumb_y, &thumb_h, &max_scroll))
            return 0;

        /* Only intercept clicks in the scrollbar zone */
        if ((float)mx < pw - 4.0f * s - 4.0f * s) return 0;

        { float fmy = (float)my - visible_top;
          if (fmy >= thumb_y && fmy < thumb_y + thumb_h) {
              popup->dragging          = 1;
              popup->drag_start_y      = fmy;
              popup->drag_start_offset = popup->scroll_offset;
              SetCapture(hwnd);
          } else {
              float dir = (fmy < thumb_y) ? -1.0f : 1.0f;
              popup->scroll_offset += dir * visible_h;
              if (popup->scroll_offset < 0.0f)         popup->scroll_offset = 0.0f;
              if (popup->scroll_offset > max_scroll) popup->scroll_offset = max_scroll;
              InvalidateRect(hwnd, NULL, FALSE);
          }
        }
        return 0;
    }

    case WM_LBUTTONUP: {
        SuiControlHost* host;
        SuiControl* ctrl;
        SuiComboItemView item;
        int item_count;
        if (!popup) return 0;
        if (popup->dragging) {
            popup->dragging = 0;
            ReleaseCapture();
            return 0;
        }
        if (popup->opening) {
            popup->opening = 0;
            return 0;
        }
        host = popup->owner_host;
        if (!host || popup->owner_ctrl_index < 0
                  || popup->owner_ctrl_index >= host->control_count)
            return 0;
        ctrl = &host->controls[popup->owner_ctrl_index];
        item_count = sui_ctrl_combo_count(ctrl);
        if (popup->hover_item >= 0 && popup->hover_item < item_count) {
            ctrl->d.combo.selected = popup->hover_item;
            sui_ctrl_combo_get_item_view(ctrl, ctrl->d.combo.selected, &item);
            sui__wstrncpy_z(ctrl->text, item.text, SUI_CTRL_TEXT_MAX);
            sui_combo_popup_close(host);
            if (host->on_click)
                host->on_click(ctrl->id, host->on_click_data);
        }
        return 0;
    }

    case WM_CAPTURECHANGED:
        if (popup) popup->dragging = 0;
        return 0;

    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;

    case WM_ACTIVATEAPP:
        if (!wp && popup && popup->owner_host)
            sui_combo_popup_close(popup->owner_host);
        return 0;

    case WM_MOUSEWHEEL: {
        SuiControlHost* host;
        SuiControl* ctrl;
        int item_count;
        int delta;
        if (!popup) return 0;
        host = popup->owner_host;
        if (!host || popup->owner_ctrl_index < 0
                  || popup->owner_ctrl_index >= host->control_count)
            return 0;
        ctrl = &host->controls[popup->owner_ctrl_index];
        item_count = sui_ctrl_combo_count(ctrl);
        delta = GET_WHEEL_DELTA_WPARAM(wp);
        { float item_h = popup->item_height;
          float visible_h = sui_combo_popup_visible_height(popup);
          float max_scroll = (float)item_count * item_h - visible_h;
          if (max_scroll < 0) max_scroll = 0;
          popup->scroll_offset -= (float)delta / 120.0f * item_h;
          if (popup->scroll_offset < 0.0f) popup->scroll_offset = 0.0f;
          if (popup->scroll_offset > max_scroll) popup->scroll_offset = max_scroll;
          sui_combo_popup_reveal_scrollbar(popup);
          InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }

    case WM_TIMER:
        if (popup && wp == 1) {
            sui_combo_popup_anim_step(popup);
            return 0;
        } else if (popup && wp == 2) {
            popup->sb_reveal -= 16.6667f / 1000.0f;
            if (popup->sb_reveal <= 0.0f) {
                popup->sb_reveal = 0.0f;
                KillTimer(hwnd, 2);
            }
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        break;

    case WM_DESTROY:
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static LRESULT CALLBACK sui_combo_popup_shadow_wndproc(HWND hwnd, UINT msg,
                                                       WPARAM wp, LPARAM lp) {
    (void)wp;
    (void)lp;
    if (msg == WM_NCHITTEST) return HTTRANSPARENT;
    if (msg == WM_ERASEBKGND) return 1;
    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* ---- Register popup window class ----------------------------------------- */

static void sui_combo_popup_register(void) {
    static int registered = 0;
    WNDCLASSEXW wc;
    if (registered) return;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.style         = 0;
    wc.lpfnWndProc   = sui_combo_popup_wndproc;
    wc.hInstance     = GetModuleHandleW(NULL);
    wc.lpszClassName = L"SuiComboPopup";
    wc.hCursor       = LoadCursorW(NULL, MAKEINTRESOURCEW(32512)); /* IDC_ARROW */
    RegisterClassExW(&wc);

    memset(&wc, 0, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.style         = 0;
    wc.lpfnWndProc   = sui_combo_popup_shadow_wndproc;
    wc.hInstance     = GetModuleHandleW(NULL);
    wc.lpszClassName = L"SuiComboPopupShadow";
    RegisterClassExW(&wc);
    registered = 1;
}

/* ---- Open popup ----------------------------------------------------------- */

static void sui_combo_popup_open(SuiControlHost* host, int ctrl_index) {
    SuiControl* ctrl;
    SuiComboPopup* popup;
    float s, item_h, popup_h, popup_w;
    LONG popup_x, popup_y;
    int max_visible = 8;
    int visible_items;
    int pw, ph, item_h_px, gap_px;
    int item_count;
    int opens_above;
    HWND popup_owner;

    if (ctrl_index < 0 || ctrl_index >= host->control_count) return;
    ctrl = &host->controls[ctrl_index];
    item_count = sui_ctrl_combo_count(ctrl);
    if (item_count <= 0) return;

    sui_combo_popup_register();

    /* Allocate popup state lazily */
    if (!host->combo_popup) {
        host->combo_popup = (SuiComboPopup*)sui_malloc(sizeof(SuiComboPopup));
        if (!host->combo_popup) return;
        memset(host->combo_popup, 0, sizeof(SuiComboPopup));
    }
    popup = host->combo_popup;

    /* Finish teardown of a previous menu so its owner does not retain the
     * persistent pressed/open state when another combobox is opened. */
    if (popup->hwnd)
        sui_combo_popup_destroy_internal(host);
    popup->anim_phase = SUI_COMBO_PHASE_NONE;
    sui_combo_popup_free_icons(popup);
    /* Release any old renderer */
    if (sui_renderer_is_init(&popup->renderer))
        sui_renderer_release(&popup->renderer);

    s       = (float)host->dpi / 96.0f;
    item_h  = ctrl->h;   /* same height as combobox for uniform feel */
    popup->corner_radius = ctrl->d.combo.popup_radius > 0.0f
        ? ctrl->d.combo.popup_radius
        : sui_combo_radius(s);

    visible_items = item_count < max_visible ? item_count : max_visible;
    popup_h = item_h * (float)visible_items;
    popup_w = sui_ctrl_combo_popup_width(ctrl);

    pw = (int)(popup_w + 0.5f);
    ph = (int)(popup_h + 0.5f);
    item_h_px = (int)(item_h + 0.5f);
    if (item_h_px < 1) item_h_px = 1;
    gap_px = (int)(sui_combo_popup_gap(s) + 0.5f);

    /* Place the menu as a separate rectangle across a fixed gap. It stays
     * below when the requested height fits the monitor work area, otherwise it
     * flips above. On unusually small work areas, whole rows are clipped from
     * the viewport height and remain reachable through scrolling. */
    sui_combo_popup_compute_placement(
        host, ctrl, pw, ph, item_h_px, gap_px,
        &popup_x, &popup_y, &ph, &opens_above);
    popup_h = (float)ph;

    popup_owner = sui_combo_owner_root(host);
    popup->layered = 1;

    popup->hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        L"SuiComboPopup", NULL,
        WS_POPUP,
        popup_x, popup_y, pw, ph,
        popup_owner ? popup_owner : host->hwnd, NULL, GetModuleHandleW(NULL), NULL);

    if (!popup->hwnd) return;

    /* Store popup pointer in window user data */
    SetWindowLongPtrW(popup->hwnd, GWLP_USERDATA, (LONG_PTR)popup);
    if (!popup->layered)
        sui_combo_popup_apply_region(popup->hwnd, pw, ph, popup->corner_radius);

    /* Initialize fields */
    popup->owner_host       = host;
    popup->owner_ctrl_index = ctrl_index;
    popup->item_height      = item_h;
    popup->hover_item       = -1;
    popup->opening          = 1;
    popup->mouse_inside     = 0;
    popup->tracking_mouse_leave = 0;
    popup->sb_reveal        = 1.0f;
    popup->dragging         = 0;
    popup->drag_start_y     = 0.0f;
    popup->drag_start_offset = 0.0f;

    /* Record screen-space cursor pos so we can ignore the synthetic
     * WM_MOUSEMOVE that fires when the popup window appears under a stationary
     * cursor. mouse_armed flips to 1 the first time the cursor reports a
     * different position. */
    popup->mouse_armed   = 0;
    { POINT cp; GetCursorPos(&cp);
      popup->open_cursor_x = cp.x;
      popup->open_cursor_y = cp.y;
    }

    popup->popup_w       = pw;
    popup->full_window_h = ph;
    popup->pos_x         = popup_x;
    popup->anchor_y      = popup_y;
    popup->opens_above   = opens_above;
    popup->motion_offset = 5.0f * s;
    popup->shadow_hwnd   = NULL;
    popup->shadow_margin = (int)(14.0f * s + 0.5f);
    popup->shadow_offset_y = (int)(3.0f * s + 0.5f);

    /* Open at the final scroll position so rows do not slide through the
     * focused field. The selected row starts centered when possible. */
    popup->scroll_offset = sui_combo_popup_initial_scroll(
        item_count, ctrl->d.combo.selected, item_h, popup_h);

    /* Init D2D DC renderer for the per-pixel alpha popup. */
    if (!sui__popup_renderer_init(&popup->renderer, popup->hwnd)) {
        DestroyWindow(popup->hwnd);
        popup->hwnd = NULL;
        return;
    }
    if (host->renderer)
        sui_renderer_apply_typography(&popup->renderer, &host->renderer->typography);
    sui_combo_popup_load_icons(popup, ctrl);

    /* Use a separate per-pixel-alpha HWND for a broad, clean shadow. Native
     * CS_DROPSHADOW artifacts at layered rounded corners on some Windows
     * builds, including opaque pixels at the lower-right edge. */
    popup->shadow_hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
        L"SuiComboPopupShadow", NULL, WS_POPUP,
        popup_x - popup->shadow_margin,
        popup_y - popup->shadow_margin,
        pw + popup->shadow_margin * 2,
        ph + popup->shadow_margin * 2,
        popup_owner ? popup_owner : host->hwnd, NULL,
        GetModuleHandleW(NULL), NULL);

    /* Mark combobox as open */
    sui_ctrl_combo_set_open_state(ctrl, 1);
    ctrl->d.combo.popup_hwnd = popup->hwnd;

    /* Prepare the first frame before showing the HWND, so the desktop never
     * presents an erased or unpainted popup during first open. */
    popup->anim_t       = 0.0f;
    popup->anim_last_ms = sui_combo_popup_now_ms(popup);
    popup->anim_phase   = SUI_COMBO_PHASE_OPEN;

    /* Subclass owner so geometry changes / outside clicks auto-close us. */
    sui_combo_owner_subclass_install(host);

    sui_combo_popup_apply_animation_position(popup);
    sui_combo_popup_update_mouse_inside(popup, 0);
    sui_combo_popup_render_now(popup->hwnd, popup);

    /* Show popup without activating host */
    if (popup->shadow_hwnd)
        ShowWindow(popup->shadow_hwnd, SW_SHOWNA);
    ShowWindow(popup->hwnd, SW_SHOWNA);
    sui_combo_popup_apply_animation_position(popup);
    UpdateWindow(popup->hwnd);

    /* Every popup, including a single-row list, gets the same subtle lift. */
#ifdef SUI_FEATURE_VSYNC
    if(!sui_combo_popup_start_clock(popup)) {
        popup->anim_t=1; popup->anim_phase=SUI_COMBO_PHASE_NONE;
        sui_combo_popup_apply_animation_position(popup);
        sui_combo_popup_render_now(popup->hwnd,popup);
    }
#else
    SetTimer(popup->hwnd, 1, 16, NULL);
#endif
}

/* ---- Animation step (shared clock, or WM_TIMER for legacy opt-out) -------- */
/*
 * Open: fade + short lift away from the closed combobox.
 * Close: reverse the same motion, then destroy the popup.
 */

static void sui_combo_popup_invalidate_owner_control(SuiComboPopup* popup) {
    SuiControlHost* host;
    SuiControl* ctrl;
    RECT rc;
    if (!popup) return;
    host = popup->owner_host;
    if (!host || !host->hwnd || popup->owner_ctrl_index < 0 ||
        popup->owner_ctrl_index >= host->control_count)
        return;
    ctrl = &host->controls[popup->owner_ctrl_index];
    rc.left = (LONG)ctrl->x - 2;
    rc.top = (LONG)ctrl->y - 2;
    rc.right = (LONG)(ctrl->x + ctrl->w + 0.5f) + 2;
    rc.bottom = (LONG)(ctrl->y + ctrl->h + 0.5f) + 2;
    InvalidateRect(host->hwnd, &rc, FALSE);
}

static void sui_combo_popup_anim_step(SuiComboPopup* popup) {
    double now;
    float dt_s;
    SuiControlHost* host;

    if (!popup || !popup->hwnd) return;
    host = popup->owner_host;
    if (!host) return;

    now = sui_combo_popup_now_ms(popup);
    if(now < popup->anim_last_ms) return; /* queued frame predates this transition */

    switch (popup->anim_phase) {
    case SUI_COMBO_PHASE_OPEN: {
        dt_s = (float)(now - popup->anim_last_ms) / 1000.0f;
        popup->anim_last_ms = now;
        if (dt_s < 0) dt_s = 0;
        popup->anim_t += dt_s / SUI_COMBO_OPEN_DUR_S;
        if (popup->anim_t >= 1.0f) {
            popup->anim_t = 1.0f;
            popup->anim_phase = SUI_COMBO_PHASE_NONE;
            KillTimer(popup->hwnd, 1);
            sui_combo_popup_reveal_scrollbar(popup);
        }
        break;
    }
    case SUI_COMBO_PHASE_CLOSE: {
        dt_s = (float)(now - popup->anim_last_ms) / 1000.0f;
        popup->anim_last_ms = now;
        if (dt_s < 0) dt_s = 0;
        popup->anim_t -= dt_s / SUI_COMBO_CLOSE_DUR_S;
        if (popup->anim_t <= 0.0f) {
            popup->anim_t = 0.0f;
            popup->anim_phase = SUI_COMBO_PHASE_NONE;
            KillTimer(popup->hwnd, 1);
            sui_combo_popup_destroy_internal(host);
            return;
        }
        break;
    }
    default:
        KillTimer(popup->hwnd, 1);
        return;
    }

    sui_combo_popup_apply_animation_position(popup);
    sui_combo_popup_render_now(popup->hwnd, popup);
    sui_combo_popup_invalidate_owner_control(popup);
}

#ifdef SUI_FEATURE_VSYNC
static void sui_combo_popup_vsync_tick(void* user,float dt_ms) {
    SuiComboPopup* popup=(SuiComboPopup*)user;
    SuiControlHost* host=popup->owner_host;
    int animated=popup->anim_phase!=SUI_COMBO_PHASE_NONE;
    (void)dt_ms;
    if(popup->sb_reveal>0) {
        double elapsed=sui_combo_popup_now_ms(popup)-popup->scrollbar_start_ms;
        if(elapsed<0) elapsed=0;
        popup->sb_reveal=elapsed>=1000 ? 0 : (float)(1-elapsed/1000.0);
        if(!animated) InvalidateRect(popup->hwnd,NULL,FALSE);
    }
    if(animated) sui_combo_popup_anim_step(popup);
    /* Closing may synchronously destroy/rebuild the popup. */
    if(host->combo_popup!=popup || !popup->hwnd) return;
    if(popup->anim_phase==SUI_COMBO_PHASE_NONE && popup->sb_reveal<=0)
        sui_vsync_detach(&host->vsync,sui_combo_popup_vsync_tick,popup);
}
#endif

/* ---- Internal: actually destroy popup window ----------------------------- */

static void sui_combo_popup_destroy_internal(SuiControlHost* host) {
    SuiComboPopup* popup = host->combo_popup;
    int ci;
    SuiControl* ctrl;

    if (!popup || !popup->hwnd) return;

#ifdef SUI_FEATURE_VSYNC
    sui_vsync_detach(&host->vsync,sui_combo_popup_vsync_tick,popup);
#endif
    /* Unregister this host first so teardown side-effects cannot re-enter it;
     * other hosts sharing the root remain registered. */
    sui_combo_owner_subclass_uninstall(host);

    KillTimer(popup->hwnd, 1);
    KillTimer(popup->hwnd, 2);
    popup->anim_phase = SUI_COMBO_PHASE_NONE;

    ShowWindow(popup->hwnd, SW_HIDE);
    if (popup->shadow_hwnd)
        ShowWindow(popup->shadow_hwnd, SW_HIDE);
    sui_combo_popup_free_icons(popup);
    sui_renderer_release(&popup->renderer);

    /* Reset open flag on the owning control */
    ci = popup->owner_ctrl_index;
    if (ci >= 0 && ci < host->control_count) {
        ctrl = &host->controls[ci];
        sui_ctrl_combo_set_open_state(ctrl, 0);
        ctrl->d.combo.popup_hwnd = NULL;
    }

    DestroyWindow(popup->hwnd);
    popup->hwnd = NULL;
    if (popup->shadow_hwnd) {
        DestroyWindow(popup->shadow_hwnd);
        popup->shadow_hwnd = NULL;
    }

    InvalidateRect(host->hwnd, NULL, FALSE);
}

/* ---- Close popup (starts fade/lift animation) ---------------------------- */

static void sui_combo_popup_close(SuiControlHost* host) {
    SuiComboPopup* popup;
    if (!host) return;
    popup = host->combo_popup;
    if (!popup || !popup->hwnd) return;

    /* Already closing */
    if (popup->anim_phase == SUI_COMBO_PHASE_CLOSE)
        return;

    popup->dragging     = 0;
    popup->hover_item   = -1;
    popup->anim_last_ms = sui_combo_popup_now_ms(popup);

    if (popup->anim_t > 0.0f) {
        popup->anim_phase = SUI_COMBO_PHASE_CLOSE;
#ifdef SUI_FEATURE_VSYNC
        if(!sui_combo_popup_start_clock(popup)) sui_combo_popup_destroy_internal(host);
#else
        SetTimer(popup->hwnd, 1, 16, NULL);
#endif
    } else {
        sui_combo_popup_destroy_internal(host);
    }
}

/* ---- Find which control has the popup open -------------------------------- */

static int sui_combo_find_open(SuiControlHost* host) {
    int i;
    for (i = 0; i < host->control_count; i++) {
        if (host->controls[i].type == SUI_CTRL_COMBOBOX && host->controls[i].d.combo.open)
            return i;
    }
    return -1;
}

/* ---- Keyboard handler ---------------------------------------------------- */

static int sui_ctrl_combo_on_keydown(SuiControlHost* host, SuiControl* c, WPARAM vk) {
    int idx = (int)(c - host->controls);
    if (c->d.combo.open) {
        SuiComboPopup* popup = host->combo_popup;
        int hi = popup ? popup->hover_item : -1;
        int n  = sui_ctrl_combo_count(c);
        if (vk == VK_ESCAPE) {
            sui_combo_popup_close(host);
            return 1;
        } else if (vk == VK_RETURN) {
            if (hi >= 0 && hi < n) {
                SuiComboItemView item;
                c->d.combo.selected = hi;
                sui_ctrl_combo_get_item_view(c, hi, &item);
                sui__wstrncpy_z(c->text, item.text, SUI_CTRL_TEXT_MAX);
                sui_combo_popup_close(host);
                if (host->on_click) host->on_click(c->id, host->on_click_data);
            } else {
                sui_combo_popup_close(host);
            }
            return 1;
        } else if (vk == VK_UP || vk == VK_DOWN || vk == VK_HOME || vk == VK_END
                   || vk == VK_PRIOR || vk == VK_NEXT) {
            if (!popup) return 1;
            if (vk == VK_HOME) {
                hi = 0;
            } else if (vk == VK_END) {
                hi = n - 1;
            } else if (vk == VK_PRIOR || vk == VK_NEXT) {
                RECT prc; int page;
                if (hi < 0) hi = c->d.combo.selected;
                GetClientRect(popup->hwnd, &prc);
                page = (int)((float)(prc.bottom - prc.top) / popup->item_height);
                if (page < 1) page = 1;
                if (vk == VK_PRIOR) hi = (hi > page) ? hi - page : 0;
                else                hi = (hi + page < n) ? hi + page : n - 1;
            } else {
                if (hi < 0) hi = c->d.combo.selected;
                if (vk == VK_UP)   hi = (hi > 0)     ? hi - 1 : 0;
                else               hi = (hi < n - 1) ? hi + 1 : n - 1;
            }
            popup->hover_item = hi;
            sui_combo_scroll_to_item(popup, hi, n);
            sui_combo_popup_reveal_scrollbar(popup);
            InvalidateRect(popup->hwnd, NULL, FALSE);
            return 1;
        }
    } else {
        if (vk == VK_SPACE || vk == VK_RETURN) {
            sui_combo_popup_open(host, idx);
            InvalidateRect(host->hwnd, NULL, FALSE);
            return 1;
        } else if (vk == VK_UP || vk == VK_DOWN) {
            int n = sui_ctrl_combo_count(c);
            SuiComboItemView item;
            if (vk == VK_UP && c->d.combo.selected > 0)
                c->d.combo.selected--;
            else if (vk == VK_DOWN && c->d.combo.selected < n - 1)
                c->d.combo.selected++;
            else
                return 1;
            sui_ctrl_combo_get_item_view(c, c->d.combo.selected, &item);
            sui__wstrncpy_z(c->text, item.text, SUI_CTRL_TEXT_MAX);
            if (host->on_click) host->on_click(c->id, host->on_click_data);
            InvalidateRect(host->hwnd, NULL, FALSE);
            return 1;
        }
    }
    return 0;
}

/* ── Heap release ─────────────────────────────────────────────────────────────── */

static void sui_ctrl_combo_release(SuiControl* c) {
    if (c->type != SUI_CTRL_COMBOBOX) return;
    if (c->d.combo.items) { sui_free(c->d.combo.items); c->d.combo.items = NULL; }
    if (c->d.combo.item_icon_paths) { sui_free(c->d.combo.item_icon_paths); c->d.combo.item_icon_paths = NULL; }
    if (c->d.combo.item_icons) { sui_free(c->d.combo.item_icons); c->d.combo.item_icons = NULL; }
    c->d.combo.item_count = 0;
    c->d.combo.virtual_count = 0;
    c->d.combo.virtual_get_item = NULL;
    c->d.combo.virtual_data = NULL;
}

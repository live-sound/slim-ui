/* sui_ctrl_spinner.h -- Numeric spinner control: constructor, draw, and input handlers.
 * Included from slim_ui.h under #ifdef SUI_IMPLEMENTATION.
 */

/* Forward decls — parse_buf is defined further down but used by selection helpers. */
static float sui_ctrl_spinner_parse_buf(const wchar_t* buf, int len);

/* ---- Constructor --------------------------------------------------------- */

static int sui_ctrl_add_spinner(SuiControlHost* host, int id, float x, float y, float w, float h,
                                float min, float max, float value, float step) {
    int idx = sui_ctrl_add(host, SUI_CTRL_SPINNER, id, x, y, w, h, L"");
    if (idx >= 0) {
        SuiControl* c = &host->controls[idx];
        c->d.spinner.min        = min;
        c->d.spinner.max        = max;
        c->d.spinner.value      = value;
        c->d.spinner.step       = step;
        c->d.spinner.hover_up   = 0;
        c->d.spinner.hover_down = 0;
        c->d.spinner.editing    = 0;
        c->d.spinner.edit_len   = 0;
        c->d.spinner.edit_buf[0] = L'\0';
        c->d.spinner.cursor      = 0;
        c->d.spinner.sel_start   = -1;
        c->d.spinner.sel_end     = -1;
        c->d.spinner.drag_anchor = 0;
    }
    return idx;
}

static int sui_ctrl_add_spinner_p(SuiControlHost* host, int id, float x, float y, float w, float h,
                                   SuiSpinnerParams p) {
    return sui_ctrl_add_spinner(host, id, x, y, w, h, p.min, p.max, p.value, p.step);
}

/* ---- Selection helpers --------------------------------------------------- */

static int sui__spinner_has_sel(const SuiControl* c) {
    return c->d.spinner.sel_start >= 0
        && c->d.spinner.sel_end   >= 0
        && c->d.spinner.sel_start != c->d.spinner.sel_end;
}

static void sui__spinner_clear_sel(SuiControl* c) {
    c->d.spinner.sel_start = -1;
    c->d.spinner.sel_end   = -1;
}

/* Delete the selected range (if any). Returns 1 if a deletion happened.
 * After the call, cursor sits at the left edge of the former selection. */
static int sui__spinner_delete_sel(SuiControl* c) {
    int a, b, n, i;
    if (!sui__spinner_has_sel(c)) return 0;
    a = c->d.spinner.sel_start < c->d.spinner.sel_end
        ? c->d.spinner.sel_start : c->d.spinner.sel_end;
    b = c->d.spinner.sel_start < c->d.spinner.sel_end
        ? c->d.spinner.sel_end   : c->d.spinner.sel_start;
    n = c->d.spinner.edit_len - b;
    for (i = 0; i < n; i++)
        c->d.spinner.edit_buf[a + i] = c->d.spinner.edit_buf[b + i];
    c->d.spinner.edit_len -= (b - a);
    c->d.spinner.edit_buf[c->d.spinner.edit_len] = L'\0';
    c->d.spinner.cursor = a;
    sui__spinner_clear_sel(c);
    c->d.spinner.value = sui_ctrl_spinner_parse_buf(
        c->d.spinner.edit_buf, c->d.spinner.edit_len);
    return 1;
}

/* Insert one wide-char at cursor, shifting tail right. */
static void sui__spinner_insert_at_cursor(SuiControl* c, wchar_t ch) {
    int i;
    if (c->d.spinner.edit_len >= 30) return;
    for (i = c->d.spinner.edit_len; i > c->d.spinner.cursor; i--)
        c->d.spinner.edit_buf[i] = c->d.spinner.edit_buf[i - 1];
    c->d.spinner.edit_buf[c->d.spinner.cursor] = ch;
    c->d.spinner.edit_len++;
    c->d.spinner.edit_buf[c->d.spinner.edit_len] = L'\0';
    c->d.spinner.cursor++;
}

/* Geometry helpers — the val_buf is rendered centered, so we compute the
 * left edge from its measured width. */
static float sui__spinner_text_left(SuiRenderer* r, const SuiControl* ctrl,
                                    const wchar_t* val_buf, float text_w) {
    float font_size = sui_ctrl_style_font_size(ctrl, r->font_size);
    int bold = (ctrl->state & SUI_CTRL_BOLD) ? 1 : 0;
    float text_pixels = sui_renderer_measure_text_width_ex(r, val_buf, bold, font_size,
        sui_ctrl_style_font_family(ctrl));
    return ctrl->x + (text_w - text_pixels) / 2.0f;
}

/* Return position 0..edit_len whose caret x is closest to mx. */
static int sui__spinner_hit_pos(SuiRenderer* r, const SuiControl* ctrl,
                                const wchar_t* val_buf, float text_w, float mx) {
    int len = ctrl->d.spinner.edit_len;
    float text_left = sui__spinner_text_left(r, ctrl, val_buf, text_w);
    int   best_i = 0;
    float click_x = mx - text_left;
    float best_d  = click_x < 0.0f ? -click_x : click_x;
    int   i;
    wchar_t prefix[34];
    for (i = 1; i <= len; i++) {
        float w, dist;
        int j;
        for (j = 0; j < i; j++) prefix[j] = val_buf[j];
        prefix[i] = L'\0';
        w = sui_renderer_measure_text_width_ex(r, prefix,
            (ctrl->state & SUI_CTRL_BOLD) ? 1 : 0,
            sui_ctrl_style_font_size(ctrl, r->font_size),
            sui_ctrl_style_font_family(ctrl));
        dist = w - click_x;
        if (dist < 0.0f) dist = -dist;
        if (dist < best_d) { best_d = dist; best_i = i; }
    }
    return best_i;
}

/* ---- Clamp helper -------------------------------------------------------- */

static void sui_ctrl_spinner_clamp(SuiControl* c) {
    if (c->d.spinner.value < c->d.spinner.min) c->d.spinner.value = c->d.spinner.min;
    if (c->d.spinner.value > c->d.spinner.max) c->d.spinner.value = c->d.spinner.max;
}

/* ---- Arrow area geometry helper ----------------------------------------- */
/* Returns 1 if mx,my is in the arrow panel, sets *in_up accordingly. */
static int sui_ctrl_spinner_arrow_hit(const SuiControl* c, float s, float mx, float my, int* in_up) {
    float arrow_w = c->w * 0.30f;
    float ax = c->x + c->w - arrow_w;
    if (mx < ax || mx >= c->x + c->w) return 0;
    if (my < c->y  || my >= c->y + c->h) return 0;
    *in_up = (my < c->y + c->h / 2.0f);
    (void)s;
    return 1;
}

/* ---- Arrow press-and-hold ------------------------------------------------ */

#define SUI_SPINNER_REPEAT_TIMER_ID 0x5555
#define SUI_SPINNER_REPEAT_DELAY_MS 400
#define SUI_SPINNER_REPEAT_INTERVAL_MS 60

static void sui_ctrl_spinner_stop_repeat(SuiControlHost* host) {
    int index = host->spinner_repeat_control - 1;
    if (index < 0) return;
    /* Clear ownership before ReleaseCapture, which can synchronously reenter. */
    host->spinner_repeat_control = 0;
    host->spinner_repeat_direction = 0;
    host->spinner_repeat_fast = 0;
    host->spinner_repeat_due = 0;
    KillTimer(host->hwnd, SUI_SPINNER_REPEAT_TIMER_ID);
    if (index < host->control_count && host->controls[index].type == SUI_CTRL_SPINNER)
        host->controls[index].state &= ~SUI_CTRL_PRESSED;
    if (GetCapture() == host->hwnd) ReleaseCapture();
}

static void sui_ctrl_spinner_repeat_step(SuiControlHost* host) {
    int index = host->spinner_repeat_control - 1;
    SuiControl* c;
    float old_value;
    if (index < 0) return;
    if (index >= host->control_count || host->focused_control != index ||
        GetCapture() != host->hwnd) {
        sui_ctrl_spinner_stop_repeat(host);
        return;
    }
    c = &host->controls[index];
    if (c->type != SUI_CTRL_SPINNER ||
        (c->state & (SUI_CTRL_HIDDEN | SUI_CTRL_DISABLED)) ||
        !(c->state & SUI_CTRL_PRESSED)) {
        sui_ctrl_spinner_stop_repeat(host);
        return;
    }
    old_value = c->d.spinner.value;
    c->d.spinner.value += host->spinner_repeat_direction * c->d.spinner.step;
    sui_ctrl_spinner_clamp(c);
    /* No idle wakeups or duplicate change callbacks at either bound. */
    if (old_value == c->d.spinner.value || c->d.spinner.value <= c->d.spinner.min ||
        c->d.spinner.value >= c->d.spinner.max)
        sui_ctrl_spinner_stop_repeat(host);
    if (old_value == c->d.spinner.value) return;
    InvalidateRect(host->hwnd, NULL, FALSE);
    /* Last operation: the callback may rebuild controls or release the host. */
    if (host->on_click) host->on_click(c->id, host->on_click_data);
}

static void sui_ctrl_spinner_repeat_tick(SuiControlHost* host, DWORD now) {
    if (!host->spinner_repeat_control) return;
    /* KillTimer does not remove already queued WM_TIMER messages. */
    if ((LONG)(now - host->spinner_repeat_due) < 0) return;
    host->spinner_repeat_due = now + SUI_SPINNER_REPEAT_INTERVAL_MS;
    if (!host->spinner_repeat_fast) {
        host->spinner_repeat_fast = 1;
        if (!SetTimer(host->hwnd, SUI_SPINNER_REPEAT_TIMER_ID,
                      SUI_SPINNER_REPEAT_INTERVAL_MS, NULL)) {
            sui_ctrl_spinner_stop_repeat(host);
            return;
        }
    }
    sui_ctrl_spinner_repeat_step(host);
}

static void sui_ctrl_spinner_start_repeat(SuiControlHost* host, int index, int in_up) {
    SuiControl* c = &host->controls[index];
    if (c->d.spinner.editing) { c->d.spinner.editing = 0; sui_ctrl_spinner_clamp(c); }
    SetCapture(host->hwnd);
    c->state |= SUI_CTRL_PRESSED;
    host->spinner_repeat_control = index + 1;
    host->spinner_repeat_direction = in_up ? 1 : -1;
    host->spinner_repeat_fast = 0;
    host->spinner_repeat_due = GetTickCount() + SUI_SPINNER_REPEAT_DELAY_MS;
    if (!SetTimer(host->hwnd, SUI_SPINNER_REPEAT_TIMER_ID,
                  SUI_SPINNER_REPEAT_DELAY_MS, NULL)) {
        /* Preserve the ordinary click even if the OS cannot allocate a timer. */
        float old_value = c->d.spinner.value;
        c->d.spinner.value += (in_up ? 1 : -1) * c->d.spinner.step;
        sui_ctrl_spinner_clamp(c);
        sui_ctrl_spinner_stop_repeat(host);
        InvalidateRect(host->hwnd, NULL, FALSE);
        if (old_value != c->d.spinner.value && host->on_click)
            host->on_click(c->id, host->on_click_data);
        return;
    }
    sui_ctrl_spinner_repeat_step(host);
}

/* ---- Inline-edit helpers ------------------------------------------------- */

static float sui_ctrl_spinner_parse_buf(const wchar_t* buf, int len) {
    float result = 0.0f, fdiv = 10.0f;
    int i = 0, neg = 0, in_frac = 0;
    if (len <= 0) return 0.0f;
    if (buf[0] == L'-') { neg = 1; i = 1; }
    for (; i < len; i++) {
        wchar_t ch = buf[i];
        if (ch == L'.') { in_frac = 1; continue; }
        if (ch < L'0' || ch > L'9') continue;
        if (in_frac) { result += (float)(ch - L'0') / fdiv; fdiv *= 10.0f; }
        else         { result = result * 10.0f + (float)(ch - L'0'); }
    }
    return neg ? -result : result;
}

static void sui_ctrl_spinner_begin_edit(SuiControl* c) {
    if (c->d.spinner.editing) return;
    c->d.spinner.edit_saved = c->d.spinner.value;
    c->d.spinner.editing    = 1;
    /* pre-fill with current value so the user can see and backspace-edit it */
    if (c->d.spinner.step >= 1.0f) {
        wsprintfW(c->d.spinner.edit_buf, L"%d", (int)(c->d.spinner.value + 0.5f));
    } else {
        int whole = (int)(c->d.spinner.value);
        int frac  = (int)((c->d.spinner.value - (float)whole) * 10.0f + 0.5f);
        if (frac >= 10) { whole++; frac = 0; }
        wsprintfW(c->d.spinner.edit_buf, L"%d.%d", whole, frac);
    }
    c->d.spinner.edit_len = (int)wcslen(c->d.spinner.edit_buf);
    /* Excel/Win convention: select all on entry so first keystroke replaces. */
    c->d.spinner.cursor    = c->d.spinner.edit_len;
    c->d.spinner.sel_start = 0;
    c->d.spinner.sel_end   = c->d.spinner.edit_len;
}

/* Commit typed value: clamp to [min,max] and fire on_click.
 * Empty buffer reverts to the saved value. */
static void sui_ctrl_spinner_commit(SuiControlHost* host, SuiControl* c) {
    if (!c->d.spinner.editing) return;
    c->d.spinner.editing = 0;
    if (c->d.spinner.edit_len == 0)
        c->d.spinner.value = c->d.spinner.edit_saved;
    else
        sui_ctrl_spinner_clamp(c);
    if (host->on_click) host->on_click(c->id, host->on_click_data);
    InvalidateRect(host->hwnd, NULL, FALSE);
}

static void sui_ctrl_spinner_on_char(SuiControlHost* host, SuiControl* c, wchar_t ch) {
    if (ch == L'\b') {
        if (!c->d.spinner.editing) return;
        if (sui__spinner_has_sel(c)) {
            sui__spinner_delete_sel(c);
        } else if (c->d.spinner.cursor > 0) {
            int i, n;
            c->d.spinner.cursor--;
            n = c->d.spinner.edit_len - c->d.spinner.cursor - 1;
            for (i = 0; i < n; i++)
                c->d.spinner.edit_buf[c->d.spinner.cursor + i]
                    = c->d.spinner.edit_buf[c->d.spinner.cursor + i + 1];
            c->d.spinner.edit_len--;
            c->d.spinner.edit_buf[c->d.spinner.edit_len] = L'\0';
            c->d.spinner.value = sui_ctrl_spinner_parse_buf(
                c->d.spinner.edit_buf, c->d.spinner.edit_len);
        }
        InvalidateRect(host->hwnd, NULL, FALSE);
        return;
    }
    if (ch >= L'0' && ch <= L'9') {
        if (!c->d.spinner.editing) sui_ctrl_spinner_begin_edit(c);
        sui__spinner_delete_sel(c);
        sui__spinner_insert_at_cursor(c, ch);
        c->d.spinner.value = sui_ctrl_spinner_parse_buf(
            c->d.spinner.edit_buf, c->d.spinner.edit_len);
        InvalidateRect(host->hwnd, NULL, FALSE);
        return;
    }
    if ((ch == L'.' || ch == L',') && c->d.spinner.step < 1.0f) {
        int i, has_dot = 0;
        if (!c->d.spinner.editing) sui_ctrl_spinner_begin_edit(c);
        /* Selection-replace would normally allow re-entering the dot, but
         * scan the post-deletion buffer to keep "one dot only" invariant. */
        sui__spinner_delete_sel(c);
        for (i = 0; i < c->d.spinner.edit_len; i++)
            if (c->d.spinner.edit_buf[i] == L'.') { has_dot = 1; break; }
        if (!has_dot) {
            sui__spinner_insert_at_cursor(c, L'.');
            InvalidateRect(host->hwnd, NULL, FALSE);
        }
        return;
    }
    if (ch == L'-' && c->d.spinner.min < 0.0f) {
        if (!c->d.spinner.editing) sui_ctrl_spinner_begin_edit(c);
        sui__spinner_delete_sel(c);
        /* Only allow '-' as the first character. */
        if (c->d.spinner.cursor == 0
            && (c->d.spinner.edit_len == 0 || c->d.spinner.edit_buf[0] != L'-')) {
            sui__spinner_insert_at_cursor(c, L'-');
            c->d.spinner.value = sui_ctrl_spinner_parse_buf(
                c->d.spinner.edit_buf, c->d.spinner.edit_len);
            InvalidateRect(host->hwnd, NULL, FALSE);
        }
    }
}

/* ---- Draw ---------------------------------------------------------------- */

static void sui_ctrl_draw_spinner(SuiControlHost* host, SuiControl* ctrl) {
    SuiRenderer* r = host->renderer;
    const SuiColorScheme* sc = host->scheme;
    float s = sui_ctrl_dpi_scale(host), rad = 4.0f * s;
    float arrow_w = ctrl->w * 0.30f;
    float text_w  = ctrl->w - arrow_w;
    float ax = ctrl->x + text_w;
    float mid_y = ctrl->y + ctrl->h / 2.0f;
    wchar_t val_buf[34];
    float tri_half, tri_h, cx_arrow;
    uint32_t border_color;
    uint32_t bg_color = sui_ctrl_style_color(ctrl, SUI_STYLE_BG_COLOR, sc->input_bg);
    uint32_t border_base = sui_ctrl_style_color(ctrl, SUI_STYLE_BORDER_COLOR, sc->input_border);
    uint32_t hover_color = sui_ctrl_style_color(ctrl, SUI_STYLE_HOVER_COLOR, sc->btn_hover);
    uint32_t accent_color = sui_ctrl_style_color(ctrl, SUI_STYLE_ACCENT_COLOR, sc->input_focus_border);
    uint32_t text_color = sui_ctrl_style_color(ctrl, SUI_STYLE_TEXT_COLOR, sc->input_text);
    uint32_t subtle_text = sui_ctrl_style_color(ctrl, SUI_STYLE_TEXT_COLOR, sc->text_secondary);
    uint32_t selection_color = sui_ctrl_style_color(ctrl, SUI_STYLE_SELECTION_COLOR, sc->input_selection);
    float font_size = sui_ctrl_style_font_size(ctrl, r->font_size);
    const wchar_t* font_family = sui_ctrl_style_font_family(ctrl);
    int bold = (ctrl->state & SUI_CTRL_BOLD) ? 1 : 0;

    if (ctrl->state & SUI_CTRL_HIDDEN) return;

    /* Background */
    sui_renderer_fill_rounded_rect(r, ctrl->x, ctrl->y, ctrl->w, ctrl->h, rad, bg_color);

    /* Arrow area hover fill — clipped to the rounded spinner body and drawn
     * before the border so it cannot cover the focus stroke. */
    if (ctrl->d.spinner.hover_up) {
        sui_renderer_push_clip(r, ax, ctrl->y, arrow_w, ctrl->h / 2.0f);
        sui_renderer_fill_rounded_rect(r, ctrl->x, ctrl->y, ctrl->w, ctrl->h, rad, hover_color);
        sui_renderer_pop_clip(r);
    }

    if (ctrl->d.spinner.hover_down) {
        sui_renderer_push_clip(r, ax, mid_y, arrow_w, ctrl->h / 2.0f);
        sui_renderer_fill_rounded_rect(r, ctrl->x, ctrl->y, ctrl->w, ctrl->h, rad, hover_color);
        sui_renderer_pop_clip(r);
    }

    /* Border — focus color when editing, eased brighter on hover otherwise
     * (matches edit/textview/hotkey hover semantics). */
    border_color = ctrl->d.spinner.editing
        ? accent_color
        : sui_color_lerp(border_base,
            sui_ctrl_style_color(ctrl, SUI_STYLE_ACCENT_COLOR, sc->btn_default_border),
            ctrl->hover_t);
    sui_renderer_draw_rounded_rect(r, ctrl->x, ctrl->y, ctrl->w, ctrl->h, rad, border_color, 1.0f);

    /* Separator line between text area and arrow area */
    sui_renderer_draw_line(r, ax, ctrl->y + 3.0f*s, ax, ctrl->y + ctrl->h - 3.0f*s, border_base);

    /* Value text — centered; selection rect drawn under it, caret on top. */
    if (ctrl->d.spinner.editing) {
        int len = ctrl->d.spinner.edit_len < 32 ? ctrl->d.spinner.edit_len : 31;
        int i;
        for (i = 0; i < len; i++) val_buf[i] = ctrl->d.spinner.edit_buf[i];
        val_buf[len] = L'\0';
    } else if (ctrl->d.spinner.step >= 1.0f) {
        wsprintfW(val_buf, L"%d", (int)(ctrl->d.spinner.value + 0.5f));
    } else {
        int whole = (int)(ctrl->d.spinner.value);
        int frac  = (int)((ctrl->d.spinner.value - (float)whole) * 10.0f + 0.5f);
        if (frac >= 10) { whole++; frac = 0; }
        wsprintfW(val_buf, L"%d.%d", whole, frac);
    }

    /* Selection rect — drawn before text so it acts as a backdrop. */
    if (ctrl->d.spinner.editing && sui__spinner_has_sel(ctrl)) {
        int a = ctrl->d.spinner.sel_start < ctrl->d.spinner.sel_end
            ? ctrl->d.spinner.sel_start : ctrl->d.spinner.sel_end;
        int b = ctrl->d.spinner.sel_start < ctrl->d.spinner.sel_end
            ? ctrl->d.spinner.sel_end   : ctrl->d.spinner.sel_start;
        wchar_t pref[34];
        float text_left = sui__spinner_text_left(r, ctrl, val_buf, text_w);
        float w_a, w_b;
        float sel_top    = ctrl->y + 2.0f * s;
        float sel_bottom = ctrl->y + ctrl->h - 2.0f * s;
        int j;
        for (j = 0; j < a; j++) pref[j] = val_buf[j];
        pref[a] = L'\0';
        w_a = sui_renderer_measure_text_width_ex(r, pref, bold, font_size, font_family);
        for (j = 0; j < b; j++) pref[j] = val_buf[j];
        pref[b] = L'\0';
        w_b = sui_renderer_measure_text_width_ex(r, pref, bold, font_size, font_family);
        sui_renderer_fill_rect(r, text_left + w_a, sel_top,
                               w_b - w_a, sel_bottom - sel_top,
                               selection_color);
    }

    sui_renderer_draw_text_centered_ex(r, val_buf,
        ctrl->x, ctrl->y, text_w, ctrl->h, text_color, bold, font_size, font_family);

    /* Caret — at cursor position (text is centered so we measure the prefix). */
    if (ctrl->d.spinner.editing) {
        wchar_t pref[34];
        float caret_h = ctrl->h * 0.55f;
        float caret_y = ctrl->y + (ctrl->h - caret_h) / 2.0f;
        float text_left = sui__spinner_text_left(r, ctrl, val_buf, text_w);
        float prefix_w;
        float caret_x;
        float caret_min_x = ctrl->x + 2.0f * s;
        float caret_max_x = ax - 2.0f * s;
        int cur = ctrl->d.spinner.cursor;
        int j;
        if (cur < 0) cur = 0;
        if (cur > ctrl->d.spinner.edit_len) cur = ctrl->d.spinner.edit_len;
        for (j = 0; j < cur; j++) pref[j] = val_buf[j];
        pref[cur] = L'\0';
        prefix_w  = sui_renderer_measure_text_width_ex(r, pref, bold, font_size, font_family);
        caret_x   = text_left + prefix_w;
        if (caret_x < caret_min_x) caret_x = caret_min_x;
        if (caret_x > caret_max_x) caret_x = caret_max_x;
        sui_renderer_fill_rect(r, caret_x, caret_y, 1.5f * s, caret_h,
                               accent_color);
    }

    /* Up triangle (▲) — centered in top half of arrow area */
    cx_arrow = ax + arrow_w / 2.0f;
    tri_half = 4.0f * s;
    tri_h    = 3.5f * s;
    {
        float ty_center = ctrl->y + ctrl->h / 4.0f;
        /* tip */   float px = cx_arrow,          py = ty_center - tri_h / 2.0f;
        /* left */  float lx = cx_arrow - tri_half, ly = ty_center + tri_h / 2.0f;
        /* right */ float rx = cx_arrow + tri_half, ry = ly;
        uint32_t arrow_clr = subtle_text;
        sui_renderer_draw_line(r, px, py, lx, ly, arrow_clr);
        sui_renderer_draw_line(r, lx, ly, rx, ry, arrow_clr);
        sui_renderer_draw_line(r, rx, ry, px, py, arrow_clr);
    }

    /* Down triangle (▼) — centered in bottom half of arrow area */
    {
        float ty_center = ctrl->y + ctrl->h * 3.0f / 4.0f;
        /* tip */   float px = cx_arrow,          py = ty_center + tri_h / 2.0f;
        /* left */  float lx = cx_arrow - tri_half, ly = ty_center - tri_h / 2.0f;
        /* right */ float rx = cx_arrow + tri_half, ry = ly;
        uint32_t arrow_clr = subtle_text;
        sui_renderer_draw_line(r, px, py, lx, ly, arrow_clr);
        sui_renderer_draw_line(r, lx, ly, rx, ry, arrow_clr);
        sui_renderer_draw_line(r, rx, ry, px, py, arrow_clr);
    }

    sui_ctrl_draw_kb_focus_ring(host, ctrl, rad);
}

/* ---- Keyboard handler ---------------------------------------------------- */

static int sui_ctrl_spinner_on_keydown(SuiControlHost* host, SuiControl* c, WPARAM vk) {
    if (vk == VK_RETURN) {
        if (c->d.spinner.editing) sui_ctrl_spinner_commit(host, c);
        return 1;
    }
    if (vk == VK_ESCAPE) {
        if (c->d.spinner.editing) {
            c->d.spinner.value   = c->d.spinner.edit_saved;
            c->d.spinner.editing = 0;
            InvalidateRect(host->hwnd, NULL, FALSE);
        }
        return 1;
    }
    if (vk == VK_UP) {
        if (c->d.spinner.editing) { c->d.spinner.editing = 0; sui_ctrl_spinner_clamp(c); }
        c->d.spinner.value += c->d.spinner.step;
        sui_ctrl_spinner_clamp(c);
        if (host->on_click) host->on_click(c->id, host->on_click_data);
        InvalidateRect(host->hwnd, NULL, FALSE);
        return 1;
    }
    if (vk == VK_DOWN) {
        if (c->d.spinner.editing) { c->d.spinner.editing = 0; sui_ctrl_spinner_clamp(c); }
        c->d.spinner.value -= c->d.spinner.step;
        sui_ctrl_spinner_clamp(c);
        if (host->on_click) host->on_click(c->id, host->on_click_data);
        InvalidateRect(host->hwnd, NULL, FALSE);
        return 1;
    }
    /* Cursor / selection navigation when editing. Shift extends selection,
     * plain arrows move the caret and collapse any selection to that side. */
    if (c->d.spinner.editing
        && (vk == VK_LEFT || vk == VK_RIGHT || vk == VK_HOME || vk == VK_END || vk == VK_DELETE)) {
        int shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        int new_cursor = c->d.spinner.cursor;
        if (vk == VK_LEFT) {
            if (!shift && sui__spinner_has_sel(c)) {
                int a = c->d.spinner.sel_start < c->d.spinner.sel_end
                    ? c->d.spinner.sel_start : c->d.spinner.sel_end;
                new_cursor = a;
                sui__spinner_clear_sel(c);
            } else if (new_cursor > 0) {
                new_cursor--;
            }
        } else if (vk == VK_RIGHT) {
            if (!shift && sui__spinner_has_sel(c)) {
                int b = c->d.spinner.sel_start > c->d.spinner.sel_end
                    ? c->d.spinner.sel_start : c->d.spinner.sel_end;
                new_cursor = b;
                sui__spinner_clear_sel(c);
            } else if (new_cursor < c->d.spinner.edit_len) {
                new_cursor++;
            }
        } else if (vk == VK_HOME) {
            new_cursor = 0;
            if (!shift) sui__spinner_clear_sel(c);
        } else if (vk == VK_END) {
            new_cursor = c->d.spinner.edit_len;
            if (!shift) sui__spinner_clear_sel(c);
        } else /* VK_DELETE */ {
            if (sui__spinner_has_sel(c)) {
                sui__spinner_delete_sel(c);
            } else if (c->d.spinner.cursor < c->d.spinner.edit_len) {
                int i, n = c->d.spinner.edit_len - c->d.spinner.cursor - 1;
                for (i = 0; i < n; i++)
                    c->d.spinner.edit_buf[c->d.spinner.cursor + i]
                        = c->d.spinner.edit_buf[c->d.spinner.cursor + i + 1];
                c->d.spinner.edit_len--;
                c->d.spinner.edit_buf[c->d.spinner.edit_len] = L'\0';
                c->d.spinner.value = sui_ctrl_spinner_parse_buf(
                    c->d.spinner.edit_buf, c->d.spinner.edit_len);
            }
            InvalidateRect(host->hwnd, NULL, FALSE);
            return 1;
        }
        if (shift) {
            if (!sui__spinner_has_sel(c)) {
                c->d.spinner.sel_start = c->d.spinner.cursor;
            }
            c->d.spinner.sel_end = new_cursor;
        }
        c->d.spinner.cursor = new_cursor;
        InvalidateRect(host->hwnd, NULL, FALSE);
        return 1;
    }
    /* Ctrl+A — select all when editing. */
    if (c->d.spinner.editing && vk == 'A' && (GetKeyState(VK_CONTROL) & 0x8000)) {
        c->d.spinner.sel_start = 0;
        c->d.spinner.sel_end   = c->d.spinner.edit_len;
        c->d.spinner.cursor    = c->d.spinner.edit_len;
        InvalidateRect(host->hwnd, NULL, FALSE);
        return 1;
    }
    return 0;
}

/* ---- Mouse hover update — returns 1 if hover state changed --------------- */

static int sui_ctrl_spinner_update_hover(SuiControl* sp, int is_hit, float mx, float my, float s) {
    int in_up    = 0;
    int was_up   = sp->d.spinner.hover_up;
    int was_dn   = sp->d.spinner.hover_down;
    if (is_hit && sui_ctrl_spinner_arrow_hit(sp, s, mx, my, &in_up)) {
        sp->d.spinner.hover_up   = in_up;
        sp->d.spinner.hover_down = !in_up;
    } else {
        sp->d.spinner.hover_up   = 0;
        sp->d.spinner.hover_down = 0;
    }
    return (sp->d.spinner.hover_up != was_up || sp->d.spinner.hover_down != was_dn) ? 1 : 0;
}

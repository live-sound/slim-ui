/* sui_ctrl_button.h — Button control: draw and constructor.
 * Included from slim_ui.h under #ifdef SUI_IMPLEMENTATION.
 */

/* ── Control helpers (defined here — used by all subsequent per-control headers) ── */

static float sui_ctrl_dpi_scale(const SuiControlHost* host) { return host->dpi / 96.0f; }

/* Keyboard focus ring — drawn as filled geometry (outer RR minus inner RR) so the ring
 * width is perfectly uniform at straight edges and corners. Tab focus only, never on click. */
static void sui_ctrl_draw_kb_focus_ring(SuiControlHost* host, SuiControl* ctrl, float rad) {
    float s = sui_ctrl_dpi_scale(host);
    float g = 4.0f * s;
    if ((ctrl->state & SUI_CTRL_FOCUSED) && host->keyboard_focus && !(ctrl->state & SUI_CTRL_DISABLED)) {
        float rx = (float)(int)(ctrl->x - g + 0.5f);
        float ry = (float)(int)(ctrl->y - g + 0.5f);
        float rw = (float)(int)(ctrl->w + g * 2.0f + 0.5f);
        float rh = (float)(int)(ctrl->h + g * 2.0f + 0.5f);
        sui_renderer_draw_filled_ring(host->renderer, rx, ry, rw, rh, rad + g, 2.5f,
            sui_ctrl_style_color(ctrl, SUI_STYLE_ACCENT_COLOR, host->scheme->text));
    }
}

static int sui_ctrl_add(SuiControlHost* host, SuiCtrlType type, int id,
                         float x, float y, float w, float h, const wchar_t* text) {
    SuiControl* c;
    if (host->control_count >= SUI_MAX_CONTROLS) return -1;
    c = &host->controls[host->control_count];
    memset(c, 0, sizeof(*c));
    c->type = type; c->id = id;
    c->parent_id = -1;
    c->x = x; c->y = y; c->w = w; c->h = h;
    /* min_w/min_h stay at 0 from memset. sui_ctrl_intrinsic_size supplies
     * the per-type floor (e.g. HOTKEY ~80dp); callers who want a hard
     * fixed-visual floor (e.g. a 200dp settings hotkey, a 32dp swatch)
     * set c->min_w/min_h explicitly after add. Atoms with no inherent
     * intrinsic (IMAGE, COLOR_SWATCH) collapse to 0 unless the caller
     * pins them — that's deliberate, and lets layout-fill atoms shrink
     * with the window. */
    c->shrink = 1; /* Phase 2b flex default: controls shrink when overflow. Other flex fields stay 0 from memset. */
    c->font_size = 0.0f;
    if (text) wcsncpy(c->text, text, SUI_CTRL_TEXT_MAX - 1);
    return host->control_count++;
}

static void sui_ctrl_set_font_size(SuiControlHost* host, int id, float font_size) {
    SuiControl* c = sui_ctrl_find(host, id);
    if (c) c->font_size = font_size;
}

static SuiControlStyle* sui_ctrl_ensure_style(SuiControl* c) {
    if (!c) return NULL;
    if (!c->style) {
        c->style = (SuiControlStyle*)sui_malloc(sizeof(SuiControlStyle));
        if (c->style) memset(c->style, 0, sizeof(SuiControlStyle));
    }
    return c->style;
}

static void sui_ctrl_style_release(SuiControl* c) {
    if (c && c->style) {
        sui_free(c->style);
        c->style = NULL;
    }
}

static void sui_ctrl_apply_style_color(SuiControl* c, uint32_t prop, uint32_t color) {
    SuiControlStyle* st = sui_ctrl_ensure_style(c);
    if (!st) return;
    switch (prop) {
    case SUI_STYLE_TEXT_COLOR:        st->text_color = color; break;
    case SUI_STYLE_BG_COLOR:          st->bg_color = color; break;
    case SUI_STYLE_BORDER_COLOR:      st->border_color = color; break;
    case SUI_STYLE_HOVER_COLOR:       st->hover_color = color; break;
    case SUI_STYLE_PRESS_COLOR:       st->press_color = color; break;
    case SUI_STYLE_SELECTED_COLOR:    st->selected_color = color; break;
    case SUI_STYLE_FILL_COLOR:        st->fill_color = color; break;
    case SUI_STYLE_ACCENT_COLOR:      st->accent_color = color; break;
    case SUI_STYLE_PLACEHOLDER_COLOR: st->placeholder_color = color; break;
    case SUI_STYLE_SELECTION_COLOR:   st->selection_color = color; break;
    default: return;
    }
    st->mask |= prop;
}

static void sui_ctrl_set_style_color(SuiControlHost* host, int id, uint32_t prop, uint32_t color) {
    sui_ctrl_apply_style_color(sui_ctrl_find(host, id), prop, color);
}

static void sui_ctrl_apply_font_family(SuiControl* c, const wchar_t* family) {
    SuiControlStyle* st = sui_ctrl_ensure_style(c);
    if (!st) return;
    if (family && family[0]) {
        sui__wstrncpy_z(st->font_family, family, 64);
        st->mask |= SUI_STYLE_FONT_FAMILY;
    } else {
        st->font_family[0] = L'\0';
        st->mask &= ~SUI_STYLE_FONT_FAMILY;
    }
}

static void sui_ctrl_set_font_family(SuiControlHost* host, int id, const wchar_t* family) {
    sui_ctrl_apply_font_family(sui_ctrl_find(host, id), family);
}

static void sui_ctrl_set_z(SuiControlHost* host, int id, int z) {
    SuiControl* c = sui_ctrl_find(host, id);
    if (c) c->z = z;
}

static void sui_ctrl_set_parent(SuiControlHost* host, int child_id, int parent_id) {
    SuiControl* c = sui_ctrl_find(host, child_id);
    if (c) c->parent_id = parent_id;
}

static void sui_ctrl_set_button_colors(SuiControlHost* host, int id,
                                       uint32_t face, uint32_t border, uint32_t text,
                                       uint32_t hover, uint32_t press) {
    SuiControl* c = sui_ctrl_find(host, id);
    if (!c || c->type != SUI_CTRL_BUTTON) return;
    c->d.button.face = face;
    c->d.button.border = border;
    c->d.button.text = text;
    c->d.button.hover = hover;
    c->d.button.press = press;
    c->d.button.use_custom_colors = 1;
}

/* ── Button constructor ─────────────────────────────────────────────────────── */

static int sui_ctrl_add_button(SuiControlHost* host, int id, float x, float y, float w, float h,
                                const wchar_t* text, int is_default) {
    int idx = sui_ctrl_add(host, SUI_CTRL_BUTTON, id, x, y, w, h, text);
    if (idx >= 0 && is_default) host->controls[idx].state |= SUI_CTRL_DEFAULT;
    return idx;
}

/* ── Button draw ────────────────────────────────────────────────────────────── */

static void sui_ctrl_draw_button(SuiControlHost* host, SuiControl* ctrl) {
    SuiRenderer* r = host->renderer;
    const SuiColorScheme* sc = host->scheme;
    float s = sui_ctrl_dpi_scale(host), rad = 4.0f * s;
    uint32_t fill, border, text_color;
    float font_size = sui_ctrl_style_font_size(ctrl, r->font_size);
    const wchar_t* font_family = sui_ctrl_style_font_family(ctrl);
    int bold = (ctrl->state & SUI_CTRL_BOLD) ? 1 : 0;

    uint32_t btn_face = sui_ctrl_style_color(ctrl, SUI_STYLE_BG_COLOR,
        ctrl->d.button.use_custom_colors ? ctrl->d.button.face : sc->btn_face);
    uint32_t btn_border = sui_ctrl_style_color(ctrl, SUI_STYLE_BORDER_COLOR,
        ctrl->d.button.use_custom_colors ? ctrl->d.button.border : sc->btn_border);
    uint32_t btn_text = sui_ctrl_style_color(ctrl, SUI_STYLE_TEXT_COLOR,
        ctrl->d.button.use_custom_colors ? ctrl->d.button.text : sc->btn_text);
    uint32_t btn_hover = sui_ctrl_style_color(ctrl, SUI_STYLE_HOVER_COLOR,
        ctrl->d.button.use_custom_colors ? ctrl->d.button.hover : sc->btn_hover);
    uint32_t btn_press = sui_ctrl_style_color(ctrl, SUI_STYLE_PRESS_COLOR,
        ctrl->d.button.use_custom_colors ? ctrl->d.button.press : sc->btn_press);

    if (ctrl->state & SUI_CTRL_HIDDEN) return;

    if (ctrl->state & SUI_CTRL_DISABLED) {
        fill = btn_face; border = btn_border;
        text_color = sui_ctrl_style_color(ctrl, SUI_STYLE_TEXT_COLOR, sc->text_secondary);
    } else if (ctrl->state & SUI_CTRL_PRESSED) {
        fill = btn_press; border = btn_border; text_color = btn_text;
    } else {
        fill = sui_color_lerp(btn_face, btn_hover, ctrl->hover_t);
        border = btn_border; text_color = btn_text;
    }

    sui_renderer_fill_rounded_rect(r, ctrl->x, ctrl->y, ctrl->w, ctrl->h, rad, fill);

    if ((ctrl->state & SUI_CTRL_DEFAULT) && !(ctrl->state & SUI_CTRL_DISABLED))
        sui_renderer_draw_rounded_rect(r, ctrl->x, ctrl->y, ctrl->w, ctrl->h, rad,
            sui_ctrl_style_color(ctrl, SUI_STYLE_ACCENT_COLOR, sc->btn_default_border), 1.5f);
    else
        sui_renderer_draw_rounded_rect(r, ctrl->x, ctrl->y, ctrl->w, ctrl->h, rad,
            border, 1.0f);

    sui_renderer_draw_text_centered_ex(r, ctrl->text,
        ctrl->x, ctrl->y, ctrl->w, ctrl->h, text_color, bold, font_size, font_family);

    sui_ctrl_draw_kb_focus_ring(host, ctrl, rad);
}

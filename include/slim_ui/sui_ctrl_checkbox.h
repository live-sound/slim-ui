/* sui_ctrl_checkbox.h — Checkbox control: draw and constructor.
 * Included from slim_ui.h under #ifdef SUI_IMPLEMENTATION.
 */

/* ── Checkbox constructor ───────────────────────────────────────────────────── */

static int sui_ctrl_add_checkbox(SuiControlHost* host, int id, float x, float y, float w, float h,
                                  const wchar_t* text, int checked) {
    int idx = sui_ctrl_add(host, SUI_CTRL_CHECKBOX, id, x, y, w, h, text);
    if (idx >= 0 && checked) host->controls[idx].state |= SUI_CTRL_CHECKED;
    return idx;
}

/* ── Checkbox draw ──────────────────────────────────────────────────────────── */

static void sui_ctrl_draw_checkbox(SuiControlHost* host, SuiControl* ctrl) {
    SuiRenderer* r = host->renderer;
    const SuiColorScheme* sc = host->scheme;
    float s = sui_ctrl_dpi_scale(host);
    float bsz = 16.0f * s, rad = 3.0f * s;
    float gap = SUI_CHECKBOX_GAP * s;
    float bx = ctrl->x, by = ctrl->y + (ctrl->h - bsz) / 2.0f;
    int checked = (ctrl->state & SUI_CTRL_CHECKED) != 0;
    uint32_t border_base = sui_ctrl_style_color(ctrl, SUI_STYLE_BORDER_COLOR,
        sui_color_lerp(sc->input_border, sc->text, 0.28f));
    uint32_t accent = sui_ctrl_style_color(ctrl, SUI_STYLE_ACCENT_COLOR, sc->btn_default_border);
    uint32_t text_color = (ctrl->state & SUI_CTRL_DISABLED)
        ? sui_ctrl_style_color(ctrl, SUI_STYLE_TEXT_COLOR, sc->text_secondary)
        : sui_ctrl_style_color(ctrl, SUI_STYLE_TEXT_COLOR, sc->text);
    const wchar_t* font_family = sui_ctrl_style_font_family(ctrl);

    if (ctrl->state & SUI_CTRL_HIDDEN) return;

    sui_renderer_draw_rounded_rect(r, bx, by, bsz, bsz, rad,
        sui_color_lerp(border_base, accent, ctrl->hover_t), 1.0f);

    if (checked) {
        uint32_t mark_color = (ctrl->state & SUI_CTRL_DISABLED)
            ? sui_ctrl_style_color(ctrl, SUI_STYLE_TEXT_COLOR, sc->text_secondary)
            : sui_ctrl_style_color(ctrl, SUI_STYLE_ACCENT_COLOR, text_color);
        float cx = bx + bsz * 0.16f, cy = by + bsz * 0.50f;
        float mx = bx + bsz * 0.39f, my = by + bsz * 0.70f;
        float ex = bx + bsz * 0.84f, ey = by + bsz * 0.24f;
        float thickness = 2.5f * s;
        float cap_r    = thickness * 0.5f;
        sui_renderer_draw_line_thick(r, cx, cy, mx, my, thickness, mark_color);
        sui_renderer_draw_line_thick(r, mx, my, ex, ey, thickness, mark_color);
        sui_renderer_fill_ellipse(r, cx, cy, cap_r, mark_color);
        sui_renderer_fill_ellipse(r, mx, my, cap_r, mark_color);
        sui_renderer_fill_ellipse(r, ex, ey, cap_r, mark_color);
    }

    sui_renderer_draw_text_middle_ex(r, ctrl->text, bx + bsz + gap,
        ctrl->y,
        ctrl->w - bsz - gap, ctrl->h,
        text_color, (ctrl->state & SUI_CTRL_BOLD) ? 1 : 0,
        sui_ctrl_style_font_size(ctrl, r->font_size), font_family);

    if ((ctrl->state & SUI_CTRL_FOCUSED) && host->keyboard_focus && !(ctrl->state & SUI_CTRL_DISABLED)) {
        float g   = 4.0f * s;
        float tw  = sui_renderer_measure_text_width_ex(r, ctrl->text,
            (ctrl->state & SUI_CTRL_BOLD) ? 1 : 0,
            sui_ctrl_style_font_size(ctrl, r->font_size), font_family);
        float rw  = bsz + gap + tw;
        if (rw > ctrl->w) rw = ctrl->w;
        /* Snap to whole pixels so 2px stroke edges land on integer boundaries */
        float rx  = (float)(int)(ctrl->x - g + 0.5f);
        float ry  = (float)(int)(by        - g + 0.5f);
        float rw2 = (float)(int)(rw + g*2.0f + 0.5f);
        float rh2 = (float)(int)(bsz + g*2.0f + 0.5f);
        sui_renderer_draw_filled_ring(r, rx, ry, rw2, rh2, rad + g, 2.5f,
            sui_ctrl_style_color(ctrl, SUI_STYLE_ACCENT_COLOR, sc->text));
    }
}

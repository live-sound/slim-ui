/* sui_ctrl_radio.h -- Radio button control: constructor, selection helpers, and draw.
 * Included from slim_ui.h under #ifdef SUI_IMPLEMENTATION.
 */

/* ---- Constructor --------------------------------------------------------- */

static int sui_ctrl_add_radio(SuiControlHost* host, int id, float x, float y, float w, float h,
                              const wchar_t* text, int group_id, int checked) {
    int idx = sui_ctrl_add(host, SUI_CTRL_RADIO, id, x, y, w, h, text);
    if (idx >= 0) {
        host->controls[idx].d.radio.group_id = group_id;
        if (checked) host->controls[idx].state |= SUI_CTRL_CHECKED;
    }
    return idx;
}

/* ---- Select (uncheck all in group, check this one) ----------------------- */
static void sui_uia_notify_checked_changed(SuiControlHost* host, int idx, int old_checked);

static void sui_ctrl_radio_select(SuiControlHost* host, int idx) {
    int i, gid;
    if (idx < 0 || idx >= host->control_count) return;
    gid = host->controls[idx].d.radio.group_id;
    for (i = 0; i < host->control_count; i++) {
        if (host->controls[i].type == SUI_CTRL_RADIO &&
            host->controls[i].d.radio.group_id == gid) {
            int old_checked = (host->controls[i].state & SUI_CTRL_CHECKED) != 0;
            if (i == idx) host->controls[i].state |= SUI_CTRL_CHECKED;
            else host->controls[i].state &= ~SUI_CTRL_CHECKED;
            sui_uia_notify_checked_changed(host, i, old_checked);
        }
    }
}

/* ---- Navigate within group (dir: -1 prev, +1 next), returns new index --- */

static int sui_ctrl_radio_nav(SuiControlHost* host, int idx, int dir) {
    int i, gid, best, count, start;
    if (idx < 0 || idx >= host->control_count) return idx;
    gid = host->controls[idx].d.radio.group_id;

    /* Collect indices for this group, find next/prev wrapping */
    count = host->control_count;
    best  = idx;
    start = (idx + dir + count) % count;
    for (i = 0; i < count; i++) {
        int j = (start + i * dir + count * count) % count;
        if (j < 0) j += count;
        if (j == idx) break; /* wrapped all the way around */
        if (host->controls[j].type == SUI_CTRL_RADIO &&
            host->controls[j].d.radio.group_id == gid &&
            !(host->controls[j].state & (SUI_CTRL_HIDDEN | SUI_CTRL_DISABLED))) {
            best = j;
            break;
        }
    }
    return best;
}

/* ---- Draw ---------------------------------------------------------------- */

static void sui_ctrl_draw_radio(SuiControlHost* host, SuiControl* ctrl) {
    SuiRenderer* r = host->renderer;
    const SuiColorScheme* sc = host->scheme;
    float s = sui_ctrl_dpi_scale(host);
    float diameter = 16.0f * s;
    float gap = SUI_CHECKBOX_GAP * s;
    float radius   = diameter / 2.0f;
    float cx = ctrl->x + radius;
    float cy = ctrl->y + ctrl->h / 2.0f;
    int checked = (ctrl->state & SUI_CTRL_CHECKED) != 0;
    uint32_t bg_color = sui_ctrl_style_color(ctrl, SUI_STYLE_BG_COLOR, sc->input_bg);
    uint32_t border_base = sui_ctrl_style_color(ctrl, SUI_STYLE_BORDER_COLOR,
        sui_color_lerp(sc->input_border, sc->text, 0.28f));
    uint32_t accent = sui_ctrl_style_color(ctrl, SUI_STYLE_ACCENT_COLOR, sc->btn_default_border);
    uint32_t text_color = (ctrl->state & SUI_CTRL_DISABLED)
        ? sui_ctrl_style_color(ctrl, SUI_STYLE_TEXT_COLOR, sc->text_secondary)
        : sui_ctrl_style_color(ctrl, SUI_STYLE_TEXT_COLOR, sc->text);
    const wchar_t* font_family = sui_ctrl_style_font_family(ctrl);

    if (ctrl->state & SUI_CTRL_HIDDEN) return;

    if (checked) {
        uint32_t mark_color = (ctrl->state & SUI_CTRL_DISABLED)
            ? sui_ctrl_style_color(ctrl, SUI_STYLE_TEXT_COLOR, sc->text_secondary)
            : sui_ctrl_style_color(ctrl, SUI_STYLE_ACCENT_COLOR, text_color);
        sui_renderer_draw_ellipse(r, cx, cy, radius,
            sui_color_lerp(border_base, accent, ctrl->hover_t),
            1.0f);
        sui_renderer_fill_ellipse(r, cx, cy, 4.0f * s, mark_color);
    } else {
        /* Empty circle with border */
        sui_renderer_fill_ellipse(r, cx, cy, radius, bg_color);
        sui_renderer_draw_ellipse(r, cx, cy, radius,
            sui_color_lerp(border_base, accent, ctrl->hover_t),
            1.0f);
    }

    sui_renderer_draw_text_middle_ex(r, ctrl->text,
        ctrl->x + diameter + gap,
        ctrl->y,
        ctrl->w - diameter - gap, ctrl->h,
        text_color, (ctrl->state & SUI_CTRL_BOLD) ? 1 : 0,
        sui_ctrl_style_font_size(ctrl, r->font_size), font_family);

    if ((ctrl->state & SUI_CTRL_FOCUSED) && host->keyboard_focus && !(ctrl->state & SUI_CTRL_DISABLED)) {
        float g   = 4.0f * s;
        float ry2 = ctrl->y + (ctrl->h - diameter) / 2.0f;
        float tw  = sui_renderer_measure_text_width_ex(r, ctrl->text,
            (ctrl->state & SUI_CTRL_BOLD) ? 1 : 0,
            sui_ctrl_style_font_size(ctrl, r->font_size), font_family);
        float rw  = diameter + gap + tw;
        if (rw > ctrl->w) rw = ctrl->w;
        /* Snap to whole pixels so 2px stroke edges land on integer boundaries */
        float rx  = (float)(int)(ctrl->x - g + 0.5f);
        float ry  = (float)(int)(ry2      - g + 0.5f);
        float rw2 = (float)(int)(rw + g*2.0f + 0.5f);
        float rh2 = (float)(int)(diameter + g*2.0f + 0.5f);
        sui_renderer_draw_filled_ring(r, rx, ry, rw2, rh2, 3.0f*s + g, 2.5f,
            sui_ctrl_style_color(ctrl, SUI_STYLE_ACCENT_COLOR, sc->text));
    }
}

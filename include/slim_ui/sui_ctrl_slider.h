/* sui_ctrl_slider.h -- Horizontal slider control: constructor, draw, and input handlers.
 * Included from slim_ui.h under #ifdef SUI_IMPLEMENTATION.
 */

/* ---- Constructor --------------------------------------------------------- */

static int sui_ctrl_add_slider(SuiControlHost* host, int id, float x, float y, float w, float h,
                               float min, float max, float value, float step) {
    int idx = sui_ctrl_add(host, SUI_CTRL_SLIDER, id, x, y, w, h, L"");
    if (idx >= 0) {
        SuiControl* c = &host->controls[idx];
        c->d.slider.min      = min;
        c->d.slider.max      = max;
        c->d.slider.value    = value;
        c->d.slider.step     = step;
        c->d.slider.dragging = 0;
    }
    return idx;
}

static int sui_ctrl_add_slider_p(SuiControlHost* host, int id, float x, float y, float w, float h,
                                  SuiSliderParams p) {
    return sui_ctrl_add_slider(host, id, x, y, w, h, p.min, p.max, p.value, p.step);
}

/* ---- Value-from-mouse helper --------------------------------------------- */

static void sui_ctrl_slider_update_from_mouse(SuiControl* c, float mx) {
    float ratio = (mx - c->x) / c->w;
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;
    c->d.slider.value = c->d.slider.min + ratio * (c->d.slider.max - c->d.slider.min);
    if (c->d.slider.step > 0.0f) {
        c->d.slider.value = c->d.slider.min +
            (float)(int)((c->d.slider.value - c->d.slider.min) / c->d.slider.step + 0.5f)
            * c->d.slider.step;
        if (c->d.slider.value > c->d.slider.max)
            c->d.slider.value = c->d.slider.max;
    }
}

/* ---- Draw ---------------------------------------------------------------- */

static void sui_ctrl_draw_slider(SuiControlHost* host, SuiControl* ctrl) {
    SuiRenderer* r = host->renderer;
    const SuiColorScheme* sc = host->scheme;
    float s = sui_ctrl_dpi_scale(host);
    float track_h = 4.0f * s;
    float thumb_r = (8.0f + 2.0f * ctrl->hover_t) * s;
    float ty, ratio, thumb_x, fill_w;
    uint32_t track_color = sui_ctrl_style_color(ctrl, SUI_STYLE_BG_COLOR, sc->slider_track);
    uint32_t fill_color = sui_ctrl_style_color(ctrl, SUI_STYLE_FILL_COLOR, sc->slider_fill);
    uint32_t thumb_color = sui_ctrl_style_color(ctrl, SUI_STYLE_ACCENT_COLOR, sc->slider_thumb);
    uint32_t hover_color = sui_ctrl_style_color(ctrl, SUI_STYLE_HOVER_COLOR, sc->btn_default_border);

    if (ctrl->state & SUI_CTRL_HIDDEN) return;

    /* Vertical center of track */
    ty = ctrl->y + (ctrl->h - track_h) / 2.0f;

    /* Compute thumb position */
    ratio = (ctrl->d.slider.max > ctrl->d.slider.min)
          ? (ctrl->d.slider.value - ctrl->d.slider.min) / (ctrl->d.slider.max - ctrl->d.slider.min)
          : 0.0f;
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;
    thumb_x = ctrl->x + ratio * ctrl->w;
    fill_w  = ratio * ctrl->w;
    fill_color = sui_color_lerp(fill_color, hover_color, ctrl->hover_t);
    thumb_color = sui_color_lerp(thumb_color, hover_color, ctrl->hover_t);

    /* Track background — full width, rounded ends */
    sui_renderer_fill_rounded_rect(r, ctrl->x, ty, ctrl->w, track_h, track_h / 2.0f, track_color);

    /* Filled portion — clip to fill_w, then draw full rounded track with fill color */
    if (fill_w > 0.0f) {
        ID2D1RenderTarget* rt = (ID2D1RenderTarget*)r->render_target;
        if (rt) {
            D2D1_RECT_F clip;
            clip.left = ctrl->x; clip.top = ty;
            clip.right = ctrl->x + fill_w; clip.bottom = ty + track_h;
            rt->lpVtbl->PushAxisAlignedClip(rt, &clip, 0);
            sui_renderer_fill_rounded_rect(r, ctrl->x, ty, ctrl->w, track_h,
                track_h / 2.0f, fill_color);
            rt->lpVtbl->PopAxisAlignedClip(rt);
        }
    }

    /* Thumb circle */
    sui_renderer_fill_ellipse(r, thumb_x, ctrl->y + ctrl->h / 2.0f, thumb_r, thumb_color);

    sui_ctrl_draw_kb_focus_ring(host, ctrl, 4.0f * s);
}

/* ---- Keyboard handler ---------------------------------------------------- */

static int sui_ctrl_slider_on_keydown(SuiControlHost* host, SuiControl* c, WPARAM vk) {
    float kdelta;
    switch (vk) {
    case VK_LEFT: case VK_DOWN:
        kdelta = c->d.slider.step > 0.0f ? c->d.slider.step
               : (c->d.slider.max - c->d.slider.min) / 10.0f;
        c->d.slider.value -= kdelta;
        if (c->d.slider.value < c->d.slider.min) c->d.slider.value = c->d.slider.min;
        break;
    case VK_RIGHT: case VK_UP:
        kdelta = c->d.slider.step > 0.0f ? c->d.slider.step
               : (c->d.slider.max - c->d.slider.min) / 10.0f;
        c->d.slider.value += kdelta;
        if (c->d.slider.value > c->d.slider.max) c->d.slider.value = c->d.slider.max;
        break;
    case VK_HOME: c->d.slider.value = c->d.slider.min; break;
    case VK_END:  c->d.slider.value = c->d.slider.max; break;
    default: return 0;
    }
    if (host->on_click) host->on_click(c->id, host->on_click_data);
    InvalidateRect(host->hwnd, NULL, FALSE);
    return 1;
}

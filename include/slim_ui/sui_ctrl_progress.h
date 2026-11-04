/* sui_ctrl_progress.h -- Horizontal progress bar control.
 * Included from slim_ui.h under #ifdef SUI_IMPLEMENTATION -- no include guard.
 * Delegates rendering to sui_renderer_draw_progress.
 */

/* ---- Constructor --------------------------------------------------------- */

static int sui_ctrl_add_progress(SuiControlHost* host, int id,
                                  float x, float y, float w, float h,
                                  float min, float max, float value) {
    int idx = sui_ctrl_add(host, SUI_CTRL_PROGRESS, id, x, y, w, h, L"");
    if (idx >= 0) {
        SuiControl* c = &host->controls[idx];
        c->d.progress.min   = min;
        c->d.progress.max   = max;
        c->d.progress.value = value;
    }
    return idx;
}

/* ---- Value setter (convenience — callers may also write d.progress.value) */

static void sui_ctrl_progress_set(SuiControlHost* host, int id, float value) {
    SuiControl* c = sui_ctrl_find(host, id);
    if (!c || c->type != SUI_CTRL_PROGRESS) return;
    if (value < c->d.progress.min) value = c->d.progress.min;
    if (value > c->d.progress.max) value = c->d.progress.max;
    c->d.progress.value = value;
}

/* ---- Draw ---------------------------------------------------------------- */

static void sui_ctrl_draw_progress(SuiControlHost* host, SuiControl* ctrl) {
    SuiRenderer* r = host->renderer;
    const SuiColorScheme* sc = host->scheme;
    float ratio;

    if (ctrl->state & SUI_CTRL_HIDDEN) return;

    ratio = (ctrl->d.progress.max > ctrl->d.progress.min)
          ? (ctrl->d.progress.value - ctrl->d.progress.min)
            / (ctrl->d.progress.max - ctrl->d.progress.min)
          : 0.0f;
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;

    sui_renderer_draw_progress(r, ctrl->x, ctrl->y, ctrl->w, ctrl->h,
                               ratio,
                               sui_ctrl_style_color(ctrl, SUI_STYLE_FILL_COLOR, sc->progress_fill),
                               sui_ctrl_style_color(ctrl, SUI_STYLE_BG_COLOR, sc->progress_bg));
}

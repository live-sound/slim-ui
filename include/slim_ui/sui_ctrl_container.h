/* sui_ctrl_container.h — invisible layout box that scopes z-order for its
 * children. Children are added separately and parented via sui_ctrl_set_parent.
 * Included from slim_ui.h under #ifdef SUI_IMPLEMENTATION.
 */

/* ── Container constructor ──────────────────────────────────────────────── */

static int sui_ctrl_add_container(SuiControlHost* host, int id,
                                   float x, float y, float w, float h) {
    /* d.container.no_clip defaults to 0 (clip on) via sui_ctrl_add's memset. */
    return sui_ctrl_add(host, SUI_CTRL_CONTAINER, id, x, y, w, h, L"");
}

/* Toggle clipping: clip != 0 (default) clips children to the container rect
 * for both drawing and hit-testing; clip == 0 makes the container a pure
 * z-scope so children may render and receive hits outside the rect. */
static void sui_ctrl_set_container_clip(SuiControlHost* host, int id, int clip) {
    SuiControl* c = sui_ctrl_find(host, id);
    if (!c || c->type != SUI_CTRL_CONTAINER) return;
    c->d.container.no_clip = clip ? 0 : 1;
}

/* ── Container draw ─────────────────────────────────────────────────────── */

static void sui_ctrl_draw_container(SuiControlHost* host, SuiControl* ctrl) {
    float s = sui_ctrl_dpi_scale(host), rad = 4.0f * s;
    if (ctrl->state & SUI_CTRL_HIDDEN) return;
    if (sui_ctrl_style_has(ctrl, SUI_STYLE_BG_COLOR))
        sui_renderer_fill_rounded_rect(host->renderer, ctrl->x, ctrl->y, ctrl->w, ctrl->h, rad,
            sui_ctrl_style_color(ctrl, SUI_STYLE_BG_COLOR, 0));
    if (sui_ctrl_style_has(ctrl, SUI_STYLE_BORDER_COLOR))
        sui_renderer_draw_rounded_rect(host->renderer, ctrl->x, ctrl->y, ctrl->w, ctrl->h, rad,
            sui_ctrl_style_color(ctrl, SUI_STYLE_BORDER_COLOR, 0), 1.0f);
}

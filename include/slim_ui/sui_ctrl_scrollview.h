/* sui_ctrl_scrollview.h — Scrollview control: vertically scrolling container.
 *
 * Children with parent_id == this scrollview's id are drawn with a translate-Y
 * transform of -scroll_y and clipped to the scrollview rect. The user calls
 * sui_ctrl_set_scrollview_content_height() with the logical content height in
 * DIPs; an overlay scrollbar appears on the right edge when content overflows.
 *
 * Conventions:
 *  - Place children using absolute coordinates inside the scrollview's rect
 *    (first child y == scrollview->y, content extends downward).
 *  - Reserve ~12dp on the right edge of the scrollview for the scrollbar
 *    when laying out children.
 *  - Update content_h whenever children are added/removed/repositioned.
 *
 * Included from slim_ui.h under #ifdef SUI_IMPLEMENTATION.
 */

#define SUI_SCROLLVIEW_SB_THIN_W    4.0f   /* track / thumb visual width (DIPs) */
#define SUI_SCROLLVIEW_SB_RIGHT_PAD 4.0f   /* gap from scrollview's right edge (DIPs) */
#define SUI_SCROLLVIEW_SB_TRACK_PAD 4.0f   /* top/bottom inset of track (DIPs) */
#define SUI_SCROLLVIEW_SB_HOT_W     12.0f  /* hit zone width on right gutter (DIPs) */
#define SUI_SCROLLVIEW_THUMB_MIN    20.0f  /* minimum thumb height (DIPs) */

/* ── Scrollview constructor ───────────────────────────────────────────────── */

static int sui_ctrl_add_scrollview(SuiControlHost* host, int id,
                                    float x, float y, float w, float h,
                                    float content_h) {
    int idx = sui_ctrl_add(host, SUI_CTRL_SCROLLVIEW, id, x, y, w, h, L"");
    if (idx >= 0) {
        SuiControl* c = &host->controls[idx];
        c->d.scroll.scroll_y          = 0.0f;
        c->d.scroll.content_h         = content_h;
        c->d.scroll.track_dragging    = 0;
        c->d.scroll.track_drag_anchor = 0.0f;
        c->d.scroll.drag_start_scroll = 0.0f;
    }
    return idx;
}

/* Set logical content height. Clamps scroll_y so it never points past content.
 * Call after laying out children whose total height changed. */
static void sui_ctrl_set_scrollview_content_height(SuiControlHost* host,
                                                    int id, float content_h) {
    SuiControl* c = sui_ctrl_find(host, id);
    float max_scroll;
    if (!c || c->type != SUI_CTRL_SCROLLVIEW) return;
    c->d.scroll.content_h = content_h;
    max_scroll = content_h - c->h;
    if (max_scroll < 0.0f) max_scroll = 0.0f;
    if (c->d.scroll.scroll_y > max_scroll) c->d.scroll.scroll_y = max_scroll;
    if (c->d.scroll.scroll_y < 0.0f)       c->d.scroll.scroll_y = 0.0f;
}

/* ── Scrollview geometry helper ───────────────────────────────────────────── */

/* Compute scrollbar track and thumb rectangles. Returns 1 if content overflows
 * (scrollbar should be shown), 0 otherwise. Out-pointers may be NULL. */
static int sui_ctrl_scrollview_thumb(SuiControlHost* host, SuiControl* c,
                                      float* sb_x, float* sb_y,
                                      float* sb_w, float* sb_h,
                                      float* thumb_y, float* thumb_h,
                                      float* max_scroll) {
    float s   = sui_ctrl_dpi_scale(host);
    float pad = SUI_SCROLLVIEW_SB_TRACK_PAD * s;
    float w   = SUI_SCROLLVIEW_SB_THIN_W * s;
    float visible = c->h - pad * 2.0f;
    float ratio, th;
    float ms = c->d.scroll.content_h - c->h;
    if (ms <= 0.0f || c->d.scroll.content_h <= 0.0f) {
        if (max_scroll) *max_scroll = 0.0f;
        return 0;
    }
    ratio = c->h / c->d.scroll.content_h;
    th = visible * ratio;
    if (th < SUI_SCROLLVIEW_THUMB_MIN * s) th = SUI_SCROLLVIEW_THUMB_MIN * s;
    if (th > visible) th = visible;
    if (sb_x)       *sb_x = c->x + c->w - w - SUI_SCROLLVIEW_SB_RIGHT_PAD * s;
    if (sb_y)       *sb_y = c->y + pad;
    if (sb_w)       *sb_w = w;
    if (sb_h)       *sb_h = visible;
    if (thumb_h)    *thumb_h = th;
    if (thumb_y)    *thumb_y = (c->y + pad) + (visible - th) * (c->d.scroll.scroll_y / ms);
    if (max_scroll) *max_scroll = ms;
    return 1;
}

/* Returns 1 if (mx, my) hits the scrollbar gutter (wider hit zone than the
 * visible thumb for ergonomics) AND the y is within the current thumb. */
static int sui_ctrl_scrollview_hit_thumb(SuiControlHost* host, SuiControl* c,
                                          float mx, float my) {
    float s   = sui_ctrl_dpi_scale(host);
    float hot = SUI_SCROLLVIEW_SB_HOT_W * s;
    float gutter_left = c->x + c->w - hot;
    float sb_x, sb_y, sb_w, sb_h, thumb_y, thumb_h, ms;
    if (mx < gutter_left || mx > c->x + c->w) return 0;
    if (!sui_ctrl_scrollview_thumb(host, c, &sb_x, &sb_y, &sb_w, &sb_h,
                                   &thumb_y, &thumb_h, &ms)) return 0;
    if (my < thumb_y || my > thumb_y + thumb_h) return 0;
    return 1;
}

/* ── Scrollview draw ──────────────────────────────────────────────────────── */

static void sui_ctrl_draw_scrollview(SuiControlHost* host, SuiControl* ctrl) {
    if (ctrl->state & SUI_CTRL_HIDDEN) return;
    if (sui_ctrl_style_has(ctrl, SUI_STYLE_BG_COLOR)) {
        sui_renderer_fill_rect(host->renderer, ctrl->x, ctrl->y, ctrl->w, ctrl->h,
            sui_ctrl_style_color(ctrl, SUI_STYLE_BG_COLOR, host->scheme->background));
    }
    if (sui_ctrl_style_has(ctrl, SUI_STYLE_BORDER_COLOR)) {
        sui_renderer_draw_rounded_rect(host->renderer, ctrl->x, ctrl->y, ctrl->w, ctrl->h, 0.0f,
            sui_ctrl_style_color(ctrl, SUI_STYLE_BORDER_COLOR, host->scheme->btn_area_border), 1.0f);
    }
}

/* Overlay scrollbar drawn AFTER children (after pop_clip in dispatcher) so it
 * sits above content and isn't clipped to a nested rect. */
static void sui_ctrl_draw_scrollview_chrome(SuiControlHost* host, SuiControl* ctrl) {
    SuiRenderer* r = host->renderer;
    const SuiColorScheme* sc = host->scheme;
    float sb_x, sb_y, sb_w, sb_h, thumb_y, thumb_h, ms;
    float reveal;
    if (ctrl->state & SUI_CTRL_HIDDEN) return;
    if (!sui_ctrl_scrollview_thumb(host, ctrl, &sb_x, &sb_y, &sb_w, &sb_h,
                                    &thumb_y, &thumb_h, &ms)) return;
    reveal = 0.55f + 0.45f * ctrl->hover_t;
    if (ctrl->d.scroll.track_dragging) reveal = 1.0f;
    sui_renderer_fill_rounded_rect_alpha(r, sb_x, thumb_y, sb_w, thumb_h,
        sb_w * 0.5f,
        sui_ctrl_style_color(ctrl, SUI_STYLE_ACCENT_COLOR, sc->check_border),
        reveal);
}

/* ── Scrollview transform helpers ─────────────────────────────────────────── */

/* Offset the current D2D transform by (dx, dy) on top of `base`. slim-ui
 * uses translate-only transforms; we just adjust the translation row. */
static void sui_ctrl_scrollview_apply_offset(SuiControlHost* host,
                                              const D2D1_MATRIX_3X2_F* base,
                                              float dx, float dy) {
    ID2D1RenderTarget* rt = (ID2D1RenderTarget*)host->renderer->render_target;
    D2D1_MATRIX_3X2_F m;
    if (!rt) return;
    m = *base;
    m._31 += dx;
    m._32 += dy;
    rt->lpVtbl->SetTransform(rt, &m);
}

static void sui_ctrl_scrollview_save_transform(SuiControlHost* host,
                                                D2D1_MATRIX_3X2_F* out) {
    ID2D1RenderTarget* rt = (ID2D1RenderTarget*)host->renderer->render_target;
    if (!rt) {
        out->_11 = 1.0f; out->_12 = 0.0f;
        out->_21 = 0.0f; out->_22 = 1.0f;
        out->_31 = 0.0f; out->_32 = 0.0f;
        return;
    }
    rt->lpVtbl->GetTransform(rt, out);
}

static void sui_ctrl_scrollview_restore_transform(SuiControlHost* host,
                                                   const D2D1_MATRIX_3X2_F* m) {
    ID2D1RenderTarget* rt = (ID2D1RenderTarget*)host->renderer->render_target;
    if (!rt) return;
    rt->lpVtbl->SetTransform(rt, m);
}

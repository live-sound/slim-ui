/* sui_layout.h — Imperative layout engine.
 * Included from slim_ui.h under #ifdef SUI_IMPLEMENTATION — no include guard.
 * Must be included after sui_d2d.h and sui_renderer.h.
 */

/* Column stacker: items advance downward (Y). `height` is the total vertical
 * extent available — required only if the caller plans to call sui_layout_fill
 * to compute remaining space; otherwise pass 0. */
static SuiLayout sui_layout_col(float x, float y, float width, float height, float gap) {
    SuiLayout lay;
    memset(&lay, 0, sizeof(lay));
    lay.x = x; lay.y = y; lay.width = width; lay.height = height; lay.gap = gap; lay.dir = 0;
    return lay;
}

/* Row packer: items advance rightward (X). `height` is the row's vertical
 * extent (rarely used by the layout itself; carried for symmetry). */
static SuiLayout sui_layout_row(float x, float y, float width, float height, float gap) {
    SuiLayout lay;
    memset(&lay, 0, sizeof(lay));
    lay.x = x; lay.y = y; lay.width = width; lay.height = height; lay.gap = gap; lay.dir = 1;
    return lay;
}

static float sui_layout_next(SuiLayout* lay, float size) {
    float pos;
    if (lay->dir == 0) { pos = lay->y; lay->y += size + lay->gap; }
    else { pos = lay->x; lay->x += size + lay->gap; }
    lay->used += size + lay->gap;
    return pos;
}

/* Sizes a control to its rendered text width (DWrite-measured) plus `slack`
 * pixels of horizontal padding, places it at the layout's current x, and
 * advances. Updates `ctrl->x` and `ctrl->w` in place; the caller owns y/h.
 *
 * Row layouts only — the function advances along the primary axis, and
 * "fit to text" semantics are width-based. For col layouts, advance by the
 * font's line-height directly via sui_layout_next instead.
 *
 * `slack` covers the control's intrinsic horizontal padding (e.g., ~24-28 px
 * for a button, ~4 px for a bare label). Returns the start x. */
#ifdef SUI_FEATURE_CUSTOM_CONTROLS
static float sui_layout_fit_text_width(SuiLayout* lay, SuiRenderer* renderer,
                                       SuiControlHost* host, int ctrl_id, float slack) {
    SuiControl* c = sui_ctrl_find(host, ctrl_id);
    float tw = slack;
    float pos;
    if (c && c->text[0] && renderer->dwrite_factory) {
        IDWriteFactory* dw = (IDWriteFactory*)renderer->dwrite_factory;
        IDWriteTextFormat* fmt = (IDWriteTextFormat*)renderer->font_normal;
        if (fmt) {
            IDWriteTextLayout* layout = NULL;
            HRESULT hr = sui__text_layout(dw, c->text,
                (UINT32)wcslen(c->text), fmt, 10000.0f, 100.0f, &layout);
            if (SUCCEEDED(hr) && layout) {
                DWRITE_TEXT_METRICS metrics;
                memset(&metrics, 0, sizeof(metrics));
                layout->lpVtbl->GetMetrics(layout, &metrics);
                tw = metrics.widthIncludingTrailingWhitespace + slack;
                layout->lpVtbl->Release(layout);
            }
        }
    }
    pos = sui_layout_next(lay, tw);
    if (c) { c->x = pos; c->w = tw; }
    return pos;
}
#endif /* SUI_FEATURE_CUSTOM_CONTROLS */

/* Returns space remaining along the layout's primary axis (height for col,
 * width for row). Use to size a stretchy "fill the rest" item. Returns 0 if
 * the extent wasn't set or if items have already overflowed. */
static float sui_layout_fill(SuiLayout* lay) {
    float total = (lay->dir == 0) ? lay->height : lay->width;
    float remaining = total - lay->used;
    return (remaining > 0) ? remaining : 0;
}

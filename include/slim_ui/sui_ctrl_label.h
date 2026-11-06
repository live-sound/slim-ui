/* sui_ctrl_label.h -- Static text label. Not focusable, no input handling.
 * Included from slim_ui.h under #ifdef SUI_IMPLEMENTATION.
 */

static int sui_ctrl_add_label(SuiControlHost* host, int id, float x, float y, float w, float h,
                              const wchar_t* text) {
    return sui_ctrl_add(host, SUI_CTRL_LABEL, id, x, y, w, h, text);
}

static void sui_ctrl_draw_label(SuiControlHost* host, SuiControl* ctrl) {
    /* Labels always draw single-line + clip on overflow. Wrapping into a
     * second line under tight widths used to clip into adjacent rows;
     * clip-on-overflow is the contract instead. The legacy `font_size < 0`
     * opt-in for singleline is preserved as a no-op (negation flips it
     * back positive below). */
    float font_size = ctrl->font_size != 0.0f ? (ctrl->font_size < 0.0f ? -ctrl->font_size : ctrl->font_size) : host->renderer->font_size;
    int   bold      = (ctrl->state & SUI_CTRL_BOLD) ? 1 : 0;
    if (ctrl->state & (SUI_CTRL_HIDDEN | SUI_CTRL_LABEL_NO_DRAW)) return;
    sui_renderer_draw_text_middle_singleline_ex(host->renderer, ctrl->text,
        ctrl->x, ctrl->y, ctrl->w, ctrl->h,
        (ctrl->state & SUI_CTRL_DISABLED)
            ? sui_ctrl_style_color(ctrl, SUI_STYLE_TEXT_COLOR, host->scheme->text_secondary)
            : sui_ctrl_style_color(ctrl, SUI_STYLE_TEXT_COLOR, host->scheme->text),
        bold, font_size, sui_ctrl_style_font_family(ctrl));
}

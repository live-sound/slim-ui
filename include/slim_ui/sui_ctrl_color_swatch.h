/* sui_ctrl_color_swatch.h -- Clickable color preview rectangle.
 * Included from slim_ui.h under #ifdef SUI_IMPLEMENTATION.
 */
#include <commdlg.h>

static int sui_ctrl_add_color_swatch(SuiControlHost* host, int id, float x, float y, float w, float h,
                                     uint32_t color) {
    int idx = sui_ctrl_add(host, SUI_CTRL_COLOR_SWATCH, id, x, y, w, h, L"");
    if (idx >= 0) host->controls[idx].d.swatch.color = color;
    return idx;
}

static void sui_ctrl_draw_color_swatch(SuiControlHost* host, SuiControl* ctrl) {
    SuiRenderer* r = host->renderer;
    const SuiColorScheme* sc = host->scheme;
    float s = sui_ctrl_dpi_scale(host), rad = 4.0f * s;

    if (ctrl->state & SUI_CTRL_HIDDEN) return;

    /* White base so semi-transparent swatches look correct */
    sui_renderer_fill_rounded_rect(r, ctrl->x, ctrl->y, ctrl->w, ctrl->h, rad,
        sui_ctrl_style_color(ctrl, SUI_STYLE_BG_COLOR, 0xFFFFFF));
    sui_renderer_fill_rounded_rect(r, ctrl->x, ctrl->y, ctrl->w, ctrl->h, rad, ctrl->d.swatch.color);
    sui_renderer_draw_rounded_rect(r, ctrl->x, ctrl->y, ctrl->w, ctrl->h, rad,
        (ctrl->state & SUI_CTRL_HOVER)
            ? sui_ctrl_style_color(ctrl, SUI_STYLE_HOVER_COLOR, sc->btn_default_border)
            : sui_ctrl_style_color(ctrl, SUI_STYLE_BORDER_COLOR, sc->input_border),
        1.0f);

    sui_ctrl_draw_kb_focus_ring(host, ctrl, rad);
}

static BOOL sui__swatch_pick_color(SuiControlHost* host, SuiControl* ctrl) {
    typedef BOOL (WINAPI *PFN_ChooseColorW)(CHOOSECOLORW*);
    static PFN_ChooseColorW pfn = NULL;
    static BOOL tried = FALSE;
    if (!tried) {
        tried = TRUE;
        HMODULE hm = LoadLibraryW(L"comdlg32.dll");
        if (hm) pfn = (PFN_ChooseColorW)(void*)GetProcAddress(hm, "ChooseColorW");
    }
    if (!pfn) return FALSE;

    static COLORREF customColors[16];
    static BOOL customInit = FALSE;
    if (!customInit) { memset(customColors, 0, sizeof(customColors)); customInit = TRUE; }

    uint32_t sc = ctrl->d.swatch.color;
    COLORREF cur = RGB((sc >> 16) & 0xFF, (sc >> 8) & 0xFF, sc & 0xFF);

    CHOOSECOLORW cc;
    memset(&cc, 0, sizeof(cc));
    cc.lStructSize  = sizeof(cc);
    cc.hwndOwner    = host->hwnd;
    cc.lpCustColors = customColors;
    cc.rgbResult    = cur;
    cc.Flags        = CC_FULLOPEN | CC_RGBINIT;

    if (!pfn(&cc)) return FALSE;

    ctrl->d.swatch.color = ((uint32_t)GetRValue(cc.rgbResult) << 16) |
                           ((uint32_t)GetGValue(cc.rgbResult) << 8)  |
                            (uint32_t)GetBValue(cc.rgbResult);
    return TRUE;
}

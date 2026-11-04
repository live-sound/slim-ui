/* sui_ctrl_tooltip.h -- Compact info marker with hover tooltip text.
 * Included from slim_ui.h under #ifdef SUI_IMPLEMENTATION.
 */

#define SUI_TOOLTIP_SHADOW_PEAK_ALPHA 16
#define SUI_TOOLTIP_SHADOW_MARGIN_DIP 8.0f
#define SUI_TOOLTIP_SHADOW_OFFSET_Y_DIP 2.0f

static float sui__tooltip_shadow_layer_opacity(int layer_index,
                                                int layer_count) {
    float peak, previous_t, target_t, previous_alpha, target_alpha;
    if (layer_count <= 0 || layer_index < 0 || layer_index >= layer_count)
        return 0.0f;
    peak = (float)SUI_TOOLTIP_SHADOW_PEAK_ALPHA / 255.0f;
    previous_t = (float)layer_index / (float)layer_count;
    target_t = (float)(layer_index + 1) / (float)layer_count;
    previous_alpha = peak * previous_t * previous_t;
    target_alpha = peak * target_t * target_t;
    return (target_alpha - previous_alpha) / (1.0f - previous_alpha);
}

static int sui_ctrl_add_tooltip(SuiControlHost* host, int id, float x, float y, float w, float h,
                                const wchar_t* tooltip) {
    int idx = sui_ctrl_add(host, SUI_CTRL_TOOLTIP, id, x, y, w, h, L"?");
    if (idx >= 0) {
        sui__wstrncpy_z(host->controls[idx].tooltip_text, tooltip, SUI_TOOLTIP_TEXT_MAX);
        host->controls[idx].tooltip = host->controls[idx].tooltip_text[0]
            ? host->controls[idx].tooltip_text
            : NULL;
        host->controls[idx].min_w = w;
        host->controls[idx].min_h = h;
    }
    return idx;
}

static void sui_ctrl_draw_tooltip(SuiControlHost* host, SuiControl* ctrl) {
    SuiRenderer* r = host->renderer;
    const SuiColorScheme* sc = host->scheme;
    float s = sui_ctrl_dpi_scale(host);
    float size = ctrl->w < ctrl->h ? ctrl->w : ctrl->h;
    float radius = size * 0.5f - 0.5f * s;
    float cx = ctrl->x + ctrl->w * 0.5f;
    float cy = ctrl->y + ctrl->h * 0.5f;
    uint32_t border;
    uint32_t fill;
    float font_size;

    if (ctrl->state & SUI_CTRL_HIDDEN) return;
    if (radius < 1.0f) radius = 1.0f;

    fill = (ctrl->state & SUI_CTRL_HOVER)
        ? sui_color_lerp(sui_ctrl_style_color(ctrl, SUI_STYLE_BG_COLOR, sc->input_bg),
                         sui_ctrl_style_color(ctrl, SUI_STYLE_HOVER_COLOR, sc->nav_hover),
                         ctrl->hover_t)
        : sui_ctrl_style_color(ctrl, SUI_STYLE_BG_COLOR, sc->input_bg);
    border = (ctrl->state & SUI_CTRL_HOVER)
        ? sui_ctrl_style_color(ctrl, SUI_STYLE_ACCENT_COLOR, sc->btn_default_border)
        : sui_ctrl_style_color(ctrl, SUI_STYLE_BORDER_COLOR, sc->input_border);
    font_size = sui_ctrl_style_font_size(ctrl, 10.0f);

    sui_renderer_fill_ellipse(r, cx, cy, radius, fill);
    sui_renderer_draw_ellipse(r, cx, cy, radius, border, 1.0f);
    sui_renderer_draw_text_centered_ex(r, ctrl->text,
        ctrl->x, ctrl->y - 0.5f * s, ctrl->w, ctrl->h,
        sui_ctrl_style_color(ctrl, SUI_STYLE_TEXT_COLOR, sc->text_secondary),
        (ctrl->state & SUI_CTRL_BOLD) ? 1 : 0, font_size, sui_ctrl_style_font_family(ctrl));
}

static const wchar_t* sui__tooltip_class_name(void) {
    return L"SlimUiTooltipPopup";
}

static int sui__tooltip_register_class(void);
static LRESULT CALLBACK sui__tooltip_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

static int sui__tooltip_render_layered(SuiControlHost* host, SuiControl* ctrl, int x, int y, int w, int h) {
    SuiRenderer* r;
    const SuiColorScheme* sc;
    HDC screen_dc, mem_dc;
    HBITMAP bmp, old_bmp;
    BITMAPINFO bi;
    void* bits;
    RECT bind_rc;
    POINT dst, src;
    SIZE size;
    BLENDFUNCTION blend;
    ID2D1RenderTarget* rt;
    ID2D1SolidColorBrush* shadow_brush = NULL;
    D2D1_COLOR_F clear_color, shadow_color;
    D2D1_ROUNDED_RECT shadow_rect;
    HRESULT hr;
    float s, pad_x, pad_y, radius, font_size, shadow_blur;
    float bx, by, bw, bh;
    int shadow_margin, shadow_offset_y, shadow_layers, i;
    int ok = 0;
    uint32_t bg_color;
    uint32_t border_color;
    uint32_t text_color;
    const wchar_t* font_family;
    int bold;

    if (!host || !host->tooltip_hwnd || !host->tooltip_text || !host->tooltip_text[0]) return 0;
    r = &host->tooltip_renderer;
    sc = host->scheme;
    if (!r->render_target || !sc || w <= 0 || h <= 0) return 0;

    screen_dc = GetDC(NULL);
    if (!screen_dc) return 0;
    mem_dc = CreateCompatibleDC(screen_dc);
    if (!mem_dc) { ReleaseDC(NULL, screen_dc); return 0; }

    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    bmp = CreateDIBSection(screen_dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!bmp) {
        DeleteDC(mem_dc);
        ReleaseDC(NULL, screen_dc);
        return 0;
    }
    old_bmp = (HBITMAP)SelectObject(mem_dc, bmp);
    memset(bits, 0, (size_t)w * (size_t)h * 4u);

    bind_rc.left = 0; bind_rc.top = 0; bind_rc.right = w; bind_rc.bottom = h;
    if (SUCCEEDED(((ID2D1DCRenderTarget*)r->render_target)->lpVtbl->BindDC(
            (ID2D1DCRenderTarget*)r->render_target, mem_dc, &bind_rc))) {
        s = sui_ctrl_dpi_scale(host);
        pad_x = 10.0f * s;
        pad_y = 7.0f * s;
        radius = 7.0f * s;
        font_size = sui_ctrl_style_font_size(ctrl, 10.0f);
        font_family = sui_ctrl_style_font_family(ctrl);
        bold = (ctrl->state & SUI_CTRL_BOLD) ? 1 : 0;
        bg_color = sui_ctrl_style_color(ctrl, SUI_STYLE_BG_COLOR, sc->input_bg);
        border_color = sui_ctrl_style_color(ctrl, SUI_STYLE_BORDER_COLOR, sc->input_border);
        text_color = sui_ctrl_style_color(ctrl, SUI_STYLE_TEXT_COLOR, sc->text);
        shadow_margin = (int)(SUI_TOOLTIP_SHADOW_MARGIN_DIP * s + 0.5f);
        shadow_offset_y = (int)(SUI_TOOLTIP_SHADOW_OFFSET_Y_DIP * s + 0.5f);
        if (shadow_margin < 1) shadow_margin = 1;
        if (shadow_offset_y < 0) shadow_offset_y = 0;
        if (shadow_offset_y >= shadow_margin) shadow_offset_y = shadow_margin - 1;
        shadow_blur = (float)(shadow_margin - shadow_offset_y);
        shadow_layers = (int)(shadow_blur + 0.5f);
        if (shadow_layers < 4) shadow_layers = 4;
        if (shadow_layers > 24) shadow_layers = 24;
        bx = (float)shadow_margin;
        by = (float)shadow_margin;
        bw = (float)(w - shadow_margin * 2);
        bh = (float)(h - shadow_margin * 2);
        if (bh < 1.0f) bh = 1.0f;
        if (bw < 1.0f) bw = 1.0f;

        rt = (ID2D1RenderTarget*)r->render_target;
        rt->lpVtbl->BeginDraw(rt);
        clear_color.r = clear_color.g = clear_color.b = clear_color.a = 0.0f;
        rt->lpVtbl->Clear(rt, &clear_color);
        /* Keep the shadow in this per-pixel-alpha surface so showing a tooltip
         * still needs only one non-activating HWND. The tint follows the host
         * background hue while remaining dark enough in the light scheme. */
        shadow_color.r = ((float)((sc->background >> 16) & 0xFF) / 255.0f) * 0.16f;
        shadow_color.g = ((float)((sc->background >> 8) & 0xFF) / 255.0f) * 0.16f;
        shadow_color.b = ((float)(sc->background & 0xFF) / 255.0f) * 0.16f;
        shadow_color.a = 0.0f;
        shadow_brush = sui__brush(r, sc->background, 0.0f);
        if (shadow_brush) {
            for (i = 0; i < shadow_layers; i++) {
                float t = (float)(i + 1) / (float)shadow_layers;
                float spread = shadow_blur * (1.0f - t);
                shadow_color.a = sui__tooltip_shadow_layer_opacity(i, shadow_layers);
                shadow_brush->lpVtbl->SetColor(shadow_brush, &shadow_color);
                shadow_rect.rect.left = bx - spread;
                shadow_rect.rect.top = by + (float)shadow_offset_y - spread;
                shadow_rect.rect.right = bx + bw + spread;
                shadow_rect.rect.bottom = by + (float)shadow_offset_y + bh + spread;
                shadow_rect.radiusX = radius + spread;
                shadow_rect.radiusY = radius + spread;
                sui__fill_round_once(r, shadow_rect, (ID2D1Brush*)shadow_brush);
            }
        }
        sui_renderer_fill_rounded_rect(r, bx, by, bw, bh, radius, bg_color);
        sui_renderer_draw_rounded_rect(r, bx + 0.5f * s, by + 0.5f * s,
            bw - s, bh - s, radius, border_color, 1.0f);
        sui_renderer_draw_text_ex(r, host->tooltip_text, bx + pad_x, by + pad_y,
            bw - pad_x * 2.0f, bh - pad_y * 2.0f, text_color, bold, font_size, font_family);
        hr = rt->lpVtbl->EndDraw(rt, NULL, NULL);
        if (SUCCEEDED(hr)) {
            dst.x = x; dst.y = y;
            src.x = 0; src.y = 0;
            size.cx = w; size.cy = h;
            blend.BlendOp = AC_SRC_OVER;
            blend.BlendFlags = 0;
            blend.SourceConstantAlpha = 255;
            blend.AlphaFormat = AC_SRC_ALPHA;
            ok = UpdateLayeredWindow(host->tooltip_hwnd, screen_dc, &dst, &size,
                mem_dc, &src, 0, &blend, ULW_ALPHA) ? 1 : 0;
        }
    }

    SelectObject(mem_dc, old_bmp);
    DeleteObject(bmp);
    DeleteDC(mem_dc);
    ReleaseDC(NULL, screen_dc);
    return ok;
}

static int sui__tooltip_register_class(void) {
    static int registered = 0;
    WNDCLASSW wc;
    ATOM atom;
    if (registered) return 1;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = sui__tooltip_wndproc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.lpszClassName = sui__tooltip_class_name();
    atom = RegisterClassW(&wc);
    if (!atom && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return 0;
    registered = 1;
    return 1;
}

static LRESULT CALLBACK sui__tooltip_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    SuiControlHost* host = (SuiControlHost*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (msg == WM_NCCREATE) {
        CREATESTRUCTW* cs = (CREATESTRUCTW*)lp;
        host = (SuiControlHost*)cs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)host);
    }
    switch (msg) {
    case WM_ERASEBKGND:
        return TRUE;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_NCHITTEST:
        return HTTRANSPARENT;
    case WM_NCDESTROY:
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static int sui__tooltip_ensure_window(SuiControlHost* host) {
    if (!host) return 0;
    if (host->tooltip_hwnd) return 1;
    if (!sui__tooltip_register_class()) return 0;

    host->tooltip_hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        sui__tooltip_class_name(), L"", WS_POPUP,
        0, 0, 16, 16, host->hwnd, NULL, GetModuleHandleW(NULL), host);
    if (!host->tooltip_hwnd) return 0;

    if (!sui__popup_renderer_init(&host->tooltip_renderer, host->tooltip_hwnd)) {
        DestroyWindow(host->tooltip_hwnd);
        host->tooltip_hwnd = NULL;
        return 0;
    }
    if (host->renderer)
        sui_renderer_apply_typography(&host->tooltip_renderer, &host->renderer->typography);
    return 1;
}

static void sui_ctrl_hide_tooltip_popup(SuiControlHost* host) {
    if (!host) return;
    if (host->tooltip_hwnd) ShowWindow(host->tooltip_hwnd, SW_HIDE);
    host->tooltip_visible_ctrl = -1;
    host->tooltip_text = NULL;
}

static void sui_ctrl_destroy_tooltip_popup(SuiControlHost* host) {
    if (!host) return;
    KillTimer(host->hwnd, SUI_TOOLTIP_TIMER_ID);
    if (host->tooltip_hwnd) {
        sui_renderer_release(&host->tooltip_renderer);
        DestroyWindow(host->tooltip_hwnd);
        host->tooltip_hwnd = NULL;
    }
    host->tooltip_active_ctrl = -1;
    host->tooltip_visible_ctrl = -1;
    host->tooltip_hover_start_ms = 0;
    host->tooltip_text = NULL;
}

static void sui__tooltip_calc_rect(SuiControlHost* host, SuiControl* ctrl,
                                   int* out_x, int* out_y, int* out_w, int* out_h) {
    RECT work;
    HMONITOR mon;
    MONITORINFO mi;
    POINT anchor;
    POINT left_anchor;
    float s, pad_x, pad_y, gap, margin, font_size, anchor_w, anchor_font_size, anchor_text_w;
    float work_w, max_w, min_w, text_w, text_h, tip_w, tip_h, text_area_w;
    int panel_x, panel_y, panel_w, panel_h, shadow_margin;
    int x, y, w, h;
    const wchar_t* font_family;
    int bold;

    s = sui_ctrl_dpi_scale(host);
    pad_x = 10.0f * s;
    pad_y = 7.0f * s;
    gap = 10.0f * s;
    margin = 8.0f * s;
    font_size = sui_ctrl_style_font_size(ctrl, 10.0f);
    font_family = sui_ctrl_style_font_family(ctrl);
    bold = (ctrl->state & SUI_CTRL_BOLD) ? 1 : 0;

    mon = MonitorFromWindow(host->hwnd, MONITOR_DEFAULTTONEAREST);
    memset(&mi, 0, sizeof(mi));
    mi.cbSize = sizeof(mi);
    if (mon && GetMonitorInfoW(mon, &mi)) work = mi.rcWork;
    else SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);

    work_w = (float)(work.right - work.left);
    min_w = 96.0f * s;
    max_w = 320.0f * s;
    if (max_w > work_w - margin * 2.0f) max_w = work_w - margin * 2.0f;
    if (max_w < min_w) min_w = max_w;
    if (min_w < 48.0f * s) min_w = 48.0f * s;

    text_w = sui_renderer_measure_text_width_ex(host->renderer, ctrl->tooltip,
        bold, font_size, font_family);
    tip_w = text_w + pad_x * 2.0f;
    if (tip_w < min_w) tip_w = min_w;
    if (tip_w > max_w) tip_w = max_w;
    text_area_w = tip_w - pad_x * 2.0f;
    if (text_area_w < 16.0f * s) text_area_w = 16.0f * s;

    text_h = sui_renderer_measure_text_height_ex(host->renderer, ctrl->tooltip,
        text_area_w, bold, font_size, font_family);
    if (text_h <= 0.0f) text_h = 18.0f * s;
    tip_h = text_h + pad_y * 2.0f;
    if (tip_h < 30.0f * s) tip_h = 30.0f * s;

    panel_w = (int)(tip_w + 0.5f);
    panel_h = (int)(tip_h + 0.5f);
    if (panel_w < 1) panel_w = 1;
    if (panel_h < 1) panel_h = 1;
    shadow_margin = (int)(SUI_TOOLTIP_SHADOW_MARGIN_DIP * s + 0.5f);
    if (shadow_margin < 1) shadow_margin = 1;
    w = panel_w + shadow_margin * 2;
    h = panel_h + shadow_margin * 2;

    anchor_w = ctrl->w;
    if ((ctrl->type == SUI_CTRL_CHECKBOX || ctrl->type == SUI_CTRL_RADIO) && ctrl->text[0]) {
        anchor_text_w = sui_renderer_measure_text_width_ex(host->renderer, ctrl->text,
            (ctrl->state & SUI_CTRL_BOLD) ? 1 : 0,
            sui_ctrl_style_font_size(ctrl, host->renderer->font_size),
            sui_ctrl_style_font_family(ctrl));
        anchor_w = SUI_CHECKBOX_BOX_W * s + SUI_CHECKBOX_GAP * s + anchor_text_w;
    } else if (ctrl->type == SUI_CTRL_LABEL && ctrl->text[0]) {
        anchor_font_size = ctrl->font_size != 0.0f
            ? (ctrl->font_size < 0.0f ? -ctrl->font_size : ctrl->font_size)
            : host->renderer->font_size;
        anchor_w = sui_renderer_measure_text_width_ex(host->renderer, ctrl->text,
            (ctrl->state & SUI_CTRL_BOLD) ? 1 : 0,
            anchor_font_size, sui_ctrl_style_font_family(ctrl));
    }
    if (anchor_w < 0.0f) anchor_w = 0.0f;
    if (anchor_w > ctrl->w) anchor_w = ctrl->w;

    anchor.x = (LONG)(ctrl->x + anchor_w + gap);
    anchor.y = (LONG)(ctrl->y + ctrl->h * 0.5f);
    left_anchor.x = (LONG)(ctrl->x - gap);
    left_anchor.y = anchor.y;
    ClientToScreen(host->hwnd, &anchor);
    ClientToScreen(host->hwnd, &left_anchor);

    panel_x = anchor.x;
    panel_y = anchor.y - panel_h / 2;
    x = panel_x - shadow_margin;
    y = panel_y - shadow_margin;
    if (x + w > work.right - (int)margin)
        x = left_anchor.x - panel_w - shadow_margin;
    if (x < work.left + (int)margin) x = work.left + (int)margin;
    if (x + w > work.right - (int)margin) x = work.right - (int)margin - w;
    if (y < work.top + (int)margin) y = work.top + (int)margin;
    if (y + h > work.bottom - (int)margin) y = work.bottom - (int)margin - h;

    *out_x = x;
    *out_y = y;
    *out_w = w;
    *out_h = h;
}

static void sui_ctrl_show_tooltip_popup(SuiControlHost* host, int ctrl_idx) {
    SuiControl* ctrl;
    int x, y, w, h;
    if (!host || ctrl_idx < 0 || ctrl_idx >= host->control_count) return;
    ctrl = &host->controls[ctrl_idx];
    if (!ctrl->tooltip || !ctrl->tooltip[0]) return;
    if (!sui__tooltip_ensure_window(host)) return;

    host->tooltip_text = ctrl->tooltip;
    sui__tooltip_calc_rect(host, ctrl, &x, &y, &w, &h);
    if (sui__tooltip_render_layered(host, ctrl, x, y, w, h)) {
        SetWindowPos(host->tooltip_hwnd, HWND_TOPMOST, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    } else {
        sui_ctrl_hide_tooltip_popup(host);
    }
}

/* sui_renderer.h — SuiRenderer init/release/resize/begin/end/clear/fill/draw functions
 * Both D2D and GDI paths.
 * Included from slim_ui.h under #ifdef SUI_IMPLEMENTATION — no include guard.
 */
#ifndef SUI_NO_CRT
#include "sui_corner.h"   /* the continuous rounded rect's path (sui_corner_path_round_rect); it needs the C runtime's math */
#endif
static void sui__shape_cache_release(SuiShapeCache* c);

static void sui_renderer_init_fields(SuiRenderer* r) {
    memset(r, 0, sizeof(*r));
    r->typography = sui_typography_default;
    /* Legacy mirror tracks typography.body. */
    r->font_size = r->typography.body.size_pt;
    sui__wstrncpy_z(r->font_family, r->typography.body.family, 64);
}

static void sui_renderer_set_font(SuiRenderer* r, const wchar_t* family, float size);
static void sui_renderer_apply_typography(SuiRenderer* r, const SuiTypography* typo);

/* Typography weight/style helpers — keep all per-call CreateTextFormat sites
 * honouring the active typography role (don't hardcode FW_NORMAL/FW_BOLD). */
static int sui__body_weight(const SuiRenderer* r, int bold) {
    int w = r->typography.body.weight;
    if (!bold) return w;
    return w >= 700 ? w : 700;
}
static int sui__body_style(const SuiRenderer* r) {
    return r->typography.body.italic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL;
}
static int sui__title_style(const SuiRenderer* r) {
    return r->typography.title.italic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL;
}

/* UI text is grayscale, smoothed both ways and fitted to the pixel grid, on whole-pixel spacing. Smoothing both ways keeps strokes even: hinting
 * alone snaps horizontal strokes to whole pixels and leaves stems nearly twice as heavy. The grid fit keeps baselines and x-heights on pixel
 * rows, and a GDI-natural layout gives every glyph a whole-pixel advance, so a letter looks the same wherever it falls. ClearType's colored
 * edges look wrong on some panels and on an HDR desktop. Everything measured or drawn goes through here, so widths, carets and hit tests
 * agree with the pixels. */
static HRESULT sui__text_layout(IDWriteFactory* dw, const wchar_t* str, UINT32 length, IDWriteTextFormat* fmt, float w, float h, IDWriteTextLayout** out) {
    return dw->lpVtbl->CreateGdiCompatibleTextLayout(dw, str, length, fmt, w, h, 1.0f, NULL, TRUE, out);
}

static void sui__apply_text_style(const SuiRenderer* r, ID2D1RenderTarget* rt) {
    rt->lpVtbl->SetTextAntialiasMode(rt, D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    if (r->text_params) rt->lpVtbl->SetTextRenderingParams(rt, (IDWriteRenderingParams*)r->text_params);
}

/* The style for a new render target. Without IDWriteFactory2 the target keeps its default rendering, in grayscale. */
static void sui__renderer_text_style(SuiRenderer* r) {
    IDWriteFactory* dw = (IDWriteFactory*)r->dwrite_factory; IDWriteFactory2* dw2 = NULL;
    if (SUCCEEDED(dw->lpVtbl->QueryInterface(dw, SUI_IID_REF(IID_IDWriteFactory2), (void**)&dw2)) && dw2) {
        dw2->lpVtbl->CreateCustomRenderingParams(dw2, 1.8f, 0.5f, 1.0f, 0.0f, 0, DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC,
            DWRITE_GRID_FIT_MODE_ENABLED, (IDWriteRenderingParams**)&r->text_params);
        dw2->lpVtbl->Release(dw2);
    }
    sui__apply_text_style(r, (ID2D1RenderTarget*)r->render_target);
}

/* D2D COM in C: use lpVtbl->Method(obj, ...) */
static int sui_renderer_init(SuiRenderer* r, HWND hwnd) {
    ID2D1Factory* fac = NULL; ID2D1HwndRenderTarget* tgt = NULL;
    IDWriteFactory* dw = NULL; HRESULT hr; RECT rc;
    D2D1_RENDER_TARGET_PROPERTIES rtp; D2D1_HWND_RENDER_TARGET_PROPERTIES hp;
    D2D1_SIZE_U sz; D2D1_PIXEL_FORMAT pf;
    r->hwnd = hwnd;
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, SUI_IID_REF(IID_ID2D1Factory), NULL, (void**)&fac);
    if (FAILED(hr) || !fac) return 0;
    GetClientRect(hwnd, &rc);
    sz.width = (UINT32)(rc.right - rc.left); sz.height = (UINT32)(rc.bottom - rc.top);
    pf.format = DXGI_FORMAT_B8G8R8A8_UNORM; pf.alphaMode = 3;
    memset(&rtp, 0, sizeof(rtp)); rtp.type = D2D1_RENDER_TARGET_TYPE_DEFAULT;
    rtp.pixelFormat = pf; rtp.dpiX = 96.0f; rtp.dpiY = 96.0f; rtp.usage = D2D1_RENDER_TARGET_USAGE_NONE;
    rtp.minLevel = D2D1_FEATURE_LEVEL_DEFAULT;
    memset(&hp, 0, sizeof(hp)); hp.hwnd = hwnd; hp.pixelSize = sz;
    hp.presentOptions = D2D1_PRESENT_OPTIONS_IMMEDIATELY;
    hr = fac->lpVtbl->CreateHwndRenderTarget(fac, &rtp, &hp, &tgt);
    if (FAILED(hr) || !tgt) { fac->lpVtbl->Release(fac); return 0; }
    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, SUI_IID_REF(IID_IDWriteFactory), (IUnknown**)&dw);
    if (FAILED(hr) || !dw) { tgt->lpVtbl->Base.Release((ID2D1RenderTarget*)tgt); fac->lpVtbl->Release(fac); return 0; }
    r->d2d_factory = fac; r->render_target = tgt; r->dwrite_factory = dw;
    sui__renderer_text_style(r);
    sui_renderer_set_font(r, r->font_family, r->font_size);
    return 1;
}

static int sui__popup_renderer_init(SuiRenderer* r, HWND hwnd) {
    ID2D1Factory* fac = NULL;
    IDWriteFactory* dw = NULL;
    ID2D1DCRenderTarget* tgt = NULL;
    D2D1_RENDER_TARGET_PROPERTIES rtp;
    D2D1_PIXEL_FORMAT pf;
    HRESULT hr;

    sui_renderer_init_fields(r);
    r->hwnd = hwnd;
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, SUI_IID_REF(IID_ID2D1Factory), NULL, (void**)&fac);
    if (FAILED(hr) || !fac) return 0;
    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, SUI_IID_REF(IID_IDWriteFactory), (IUnknown**)&dw);
    if (FAILED(hr) || !dw) { fac->lpVtbl->Release(fac); return 0; }

    pf.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    pf.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
    memset(&rtp, 0, sizeof(rtp));
    rtp.type = D2D1_RENDER_TARGET_TYPE_DEFAULT;
    rtp.pixelFormat = pf;
    rtp.dpiX = 96.0f;
    rtp.dpiY = 96.0f;
    rtp.usage = D2D1_RENDER_TARGET_USAGE_NONE;
    rtp.minLevel = D2D1_FEATURE_LEVEL_DEFAULT;
    hr = fac->lpVtbl->CreateDCRenderTarget(fac, &rtp, &tgt);
    if (FAILED(hr) || !tgt) {
        dw->lpVtbl->Release(dw);
        fac->lpVtbl->Release(fac);
        return 0;
    }

    r->d2d_factory = fac;
    r->dwrite_factory = dw;
    r->render_target = tgt;
    /* This premultiplied-alpha target backs both tooltips and layered combo popups. */
    sui__renderer_text_style(r);
    sui_renderer_set_font(r, r->font_family, r->font_size);
    return 1;
}

static void sui_renderer_release(SuiRenderer* r) {
    if (r->scratch_brush) { ((ID2D1SolidColorBrush*)r->scratch_brush)->lpVtbl->Base.Base.Release((IUnknown*)r->scratch_brush); r->scratch_brush=NULL; }
    if (r->font_title) { ((IDWriteTextFormat*)r->font_title)->lpVtbl->Release((IDWriteTextFormat*)r->font_title); r->font_title=NULL; }
    if (r->font_bold) { ((IDWriteTextFormat*)r->font_bold)->lpVtbl->Release((IDWriteTextFormat*)r->font_bold); r->font_bold=NULL; }
    if (r->font_normal) { ((IDWriteTextFormat*)r->font_normal)->lpVtbl->Release((IDWriteTextFormat*)r->font_normal); r->font_normal=NULL; }
#ifdef SUI_FEATURE_CUSTOM_CONTROLS
    if (r->format_cache.fmt) { ((IDWriteTextFormat*)r->format_cache.fmt)->lpVtbl->Release((IDWriteTextFormat*)r->format_cache.fmt); r->format_cache.fmt=NULL; }
#endif
    if (r->text_params) { ((IDWriteRenderingParams*)r->text_params)->lpVtbl->Release((IDWriteRenderingParams*)r->text_params); r->text_params=NULL; }
    if (r->dwrite_factory) { ((IDWriteFactory*)r->dwrite_factory)->lpVtbl->Release((IDWriteFactory*)r->dwrite_factory); r->dwrite_factory=NULL; }
    sui__shape_cache_release(&r->shape_cache);     /* factory resources: before the factory goes */
    if (r->render_target) { ((ID2D1HwndRenderTarget*)r->render_target)->lpVtbl->Base.Release((ID2D1RenderTarget*)r->render_target); r->render_target=NULL; }
    if (r->d2d_factory) { ((ID2D1Factory*)r->d2d_factory)->lpVtbl->Release((ID2D1Factory*)r->d2d_factory); r->d2d_factory=NULL; }
}

/* Get the cached scratch brush, recoloured to (rgb, alpha). Lazily creates
 * one on first use. Returns NULL on failure. Caller MUST NOT release. */
static ID2D1SolidColorBrush* sui__brush(SuiRenderer* r, uint32_t rgb, float alpha) {
    ID2D1SolidColorBrush* br;
    ID2D1RenderTarget* rt;
    D2D1_COLOR_F c;
    if (!r->render_target) return NULL;
    rt = (ID2D1RenderTarget*)r->render_target;
    c.r=(float)((rgb>>16)&0xFF)/255.0f;
    c.g=(float)((rgb>>8)&0xFF)/255.0f;
    c.b=(float)(rgb&0xFF)/255.0f;
    c.a=alpha;
    br = (ID2D1SolidColorBrush*)r->scratch_brush;
    if (!br) {
        if (FAILED(rt->lpVtbl->CreateSolidColorBrush(rt, &c, NULL, &br))) return NULL;
        r->scratch_brush = br;
        return br;
    }
    br->lpVtbl->SetColor(br, &c);
    return br;
}

static void sui_renderer_resize(SuiRenderer* r, int w, int h) {
    if (r->render_target) {
        D2D1_SIZE_U sz; sz.width = (UINT32)w; sz.height = (UINT32)h;
        ((ID2D1HwndRenderTarget*)r->render_target)->lpVtbl->Resize((ID2D1HwndRenderTarget*)r->render_target, &sz);
    }
}

static void sui_renderer_begin(SuiRenderer* r) {
    if (r->render_target) ((ID2D1RenderTarget*)r->render_target)->lpVtbl->BeginDraw((ID2D1RenderTarget*)r->render_target);
}

static void sui_renderer_end(SuiRenderer* r) {
    if (!r->render_target) return;
    { HRESULT hr = ((ID2D1RenderTarget*)r->render_target)->lpVtbl->EndDraw((ID2D1RenderTarget*)r->render_target, NULL, NULL);
      if (hr == (HRESULT)0x8899000C) { /* D2DERR_RECREATE_TARGET */
          HWND hw = r->hwnd;
          SuiTypography saved_typo = r->typography;
          sui_renderer_release(r);
          sui_renderer_init(r, hw);
          sui_renderer_apply_typography(r, &saved_typo);
      }
    }
}

static void sui_renderer_clear(SuiRenderer* r, uint32_t rgb) {
    if (!r->render_target) return;
    { D2D1_COLOR_F c; c.r=(float)((rgb>>16)&0xFF)/255.0f; c.g=(float)((rgb>>8)&0xFF)/255.0f;
      c.b=(float)(rgb&0xFF)/255.0f; c.a=1.0f;
      ((ID2D1RenderTarget*)r->render_target)->lpVtbl->Clear((ID2D1RenderTarget*)r->render_target, &c); }
}

static int sui_renderer_is_init(const SuiRenderer* r) { return r->render_target != NULL; }
static void sui_renderer_set_dc(SuiRenderer* r, HDC dc) { (void)r; (void)dc; }

/* Rebuild cached IDWriteTextFormats from r->typography. The render target is
 * fixed at 96 DPI, so we scale point sizes to scaled-DIPs via dpi/72. */
static void sui__renderer_rebuild_formats(SuiRenderer* r) {
    IDWriteFactory* dw;
    float dpi_scale, fs_body, fs_title;
    int body_bold_weight;
    int body_style, title_style;
    HRESULT hr2;

    dw = (IDWriteFactory*)r->dwrite_factory;
    if (!dw) return;

    if (r->font_normal) { ((IDWriteTextFormat*)r->font_normal)->lpVtbl->Release((IDWriteTextFormat*)r->font_normal); r->font_normal=NULL; }
    if (r->font_bold)   { ((IDWriteTextFormat*)r->font_bold)->lpVtbl->Release((IDWriteTextFormat*)r->font_bold);   r->font_bold=NULL; }
    if (r->font_title)  { ((IDWriteTextFormat*)r->font_title)->lpVtbl->Release((IDWriteTextFormat*)r->font_title); r->font_title=NULL; }

    dpi_scale = (float)sui_get_window_dpi(r->hwnd) / 72.0f;
    fs_body  = r->typography.body.size_pt  * dpi_scale;
    fs_title = r->typography.title.size_pt * dpi_scale;
    body_style  = r->typography.body.italic  ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL;
    title_style = r->typography.title.italic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL;
    /* Bold variant: use body weight if already >=700, else 700. */
    body_bold_weight = r->typography.body.weight >= 700 ? r->typography.body.weight : 700;

    /* format_cache may hold a pre-rebuild entry. On the next sui__resolve_fmt
     * call with changed params the cache misses and a new format is created;
     * entries whose params still match remain valid. layout_cache needs no
     * flush: font_normal's address changes here, so all layout entries keyed
     * by the old fmt pointer miss naturally. */
    dw->lpVtbl->CreateTextFormat(dw, r->typography.body.family, NULL,
        (int)r->typography.body.weight, body_style,
        DWRITE_FONT_STRETCH_NORMAL, fs_body, L"", (IDWriteTextFormat**)&r->font_normal);
    dw->lpVtbl->CreateTextFormat(dw, r->typography.body.family, NULL,
        (int)body_bold_weight, body_style,
        DWRITE_FONT_STRETCH_NORMAL, fs_body, L"", (IDWriteTextFormat**)&r->font_bold);
    /* Title: prefer title role family, fall back to body family on failure. */
    hr2 = dw->lpVtbl->CreateTextFormat(dw, r->typography.title.family, NULL,
        (int)r->typography.title.weight, title_style,
        DWRITE_FONT_STRETCH_NORMAL, fs_title, L"", (IDWriteTextFormat**)&r->font_title);
    if (FAILED(hr2) || !r->font_title)
        dw->lpVtbl->CreateTextFormat(dw, r->typography.body.family, NULL,
            (int)r->typography.title.weight, title_style,
            DWRITE_FONT_STRETCH_NORMAL, fs_title, L"", (IDWriteTextFormat**)&r->font_title);
}

static void sui_renderer_set_font(SuiRenderer* r, const wchar_t* family, float size) {
    /* Updates the body role; title and icon roles stay as configured. */
    sui__wstrncpy_z(r->typography.body.family, family, 64);
    r->typography.body.size_pt = size;
    sui__wstrncpy_z(r->font_family, family, 64);
    r->font_size = size;
    sui__renderer_rebuild_formats(r);
}

static void sui_renderer_apply_typography(SuiRenderer* r, const SuiTypography* typo) {
    if (!typo) return;
    r->typography = *typo;
    sui__wstrncpy_z(r->font_family, typo->body.family, 64);
    r->font_size = typo->body.size_pt;
    sui__renderer_rebuild_formats(r);
}

static uint32_t sui_color_lerp(uint32_t a, uint32_t b, float t) {
    int ra, ga, ba_c, rb, gb, bb, r, g, bv;
    if (t <= 0.0f) return a;
    if (t >= 1.0f) return b;
    ra = (a>>16)&0xFF; ga = (a>>8)&0xFF; ba_c = a&0xFF;
    rb = (b>>16)&0xFF; gb = (b>>8)&0xFF; bb   = b&0xFF;
    r  = ra   + (int)((rb - ra)   * t + 0.5f);
    g  = ga   + (int)((gb - ga)   * t + 0.5f);
    bv = ba_c + (int)((bb - ba_c) * t + 0.5f);
    return ((uint32_t)r<<16)|((uint32_t)g<<8)|(uint32_t)bv;
}

static void sui_renderer_fill_rect(SuiRenderer* r, float x, float y, float w, float h, uint32_t rgb) {
    ID2D1RenderTarget* rt; ID2D1SolidColorBrush* br; D2D1_RECT_F rc;
    br = sui__brush(r, rgb, 1.0f);
    if (!br) return;
    rt = (ID2D1RenderTarget*)r->render_target;
    rc.left=x; rc.top=y; rc.right=x+w; rc.bottom=y+h;
    rt->lpVtbl->FillRectangle(rt, &rc, (ID2D1Brush*)br);
}

/* Push an axis-aligned clip rect on the render target. Subsequent draw and
 * hit-test geometry outside the rect is suppressed until pop_clip. Pairs
 * must nest correctly. */
static void sui_renderer_push_clip(SuiRenderer* r, float x, float y, float w, float h) {
    ID2D1RenderTarget* rt; D2D1_RECT_F rc;
    if (!r->render_target) return;
    rt = (ID2D1RenderTarget*)r->render_target;
    rc.left=x; rc.top=y; rc.right=x+w; rc.bottom=y+h;
    rt->lpVtbl->PushAxisAlignedClip(rt, &rc, 0 /* D2D1_ANTIALIAS_MODE_PER_PRIMITIVE */);
}

static void sui_renderer_pop_clip(SuiRenderer* r) {
    ID2D1RenderTarget* rt;
    if (!r->render_target) return;
    rt = (ID2D1RenderTarget*)r->render_target;
    rt->lpVtbl->PopAxisAlignedClip(rt);
}

static int sui_renderer_push_opacity(SuiRenderer* r, float x, float y, float w, float h, float opacity) {
    ID2D1RenderTarget* rt;
    D2D1_LAYER_PARAMETERS params;
    if (!r || !r->render_target) return 0;
    if (w <= 0.0f || h <= 0.0f) return 0;
    if (opacity >= 0.999f) return 0;
    if (opacity <= 0.0f) opacity = 0.0f;
    rt = (ID2D1RenderTarget*)r->render_target;
    memset(&params, 0, sizeof(params));
    params.contentBounds.left = x;
    params.contentBounds.top = y;
    params.contentBounds.right = x + w;
    params.contentBounds.bottom = y + h;
    params.maskTransform._11 = 1.0f;
    params.maskTransform._22 = 1.0f;
    params.opacity = opacity;
    rt->lpVtbl->PushLayer(rt, &params, NULL);
    return 1;
}

static void sui_renderer_pop_opacity(SuiRenderer* r) {
    ID2D1RenderTarget* rt;
    if (!r || !r->render_target) return;
    rt = (ID2D1RenderTarget*)r->render_target;
    rt->lpVtbl->PopLayer(rt);
}

#ifdef SUI_FEATURE_CUSTOM_CONTROLS
/* Returns an IDWriteTextFormat* owned by r->format_cache — caller must NOT Release.
 * For the large-bold path (bold_title=1): tries `family` first, falls back to
 * r->typography.body.family. Returns NULL if dwrite_factory is absent. */
static IDWriteTextFormat *sui__resolve_fmt(SuiRenderer *r,
                                            const wchar_t *family, float size_dip,
                                            int weight, int italic, int bold_title)
{
    IDWriteFactory *dw = (IDWriteFactory *)r->dwrite_factory;
    IDWriteTextFormat *fmt;
    void *displaced;
    if (!dw) return NULL;
    fmt = (IDWriteTextFormat *)sui__fmt_cache_lookup(&r->format_cache,
                                                      family, size_dip,
                                                      weight, italic, bold_title);
    if (fmt) return fmt;
    fmt = NULL;
    if (bold_title) {
        HRESULT hr2 = dw->lpVtbl->CreateTextFormat(dw, family, NULL,
            weight, italic, DWRITE_FONT_STRETCH_NORMAL, size_dip, L"", &fmt);
        if (FAILED(hr2) || !fmt)
            dw->lpVtbl->CreateTextFormat(dw, r->typography.body.family, NULL,
                weight, italic, DWRITE_FONT_STRETCH_NORMAL, size_dip, L"", &fmt);
    } else {
        dw->lpVtbl->CreateTextFormat(dw, family, NULL,
            weight, italic, DWRITE_FONT_STRETCH_NORMAL, size_dip, L"", &fmt);
    }
    if (!fmt) return NULL;
    displaced = sui__fmt_cache_set(&r->format_cache, fmt, family, size_dip, weight, italic, bold_title);
    if (displaced) ((IDWriteTextFormat *)displaced)->lpVtbl->Release((IDWriteTextFormat *)displaced);
    return fmt;
}

static IDWriteTextFormat *sui__resolve_text_fmt_ex(SuiRenderer *r, int bold, float size,
                                                    const wchar_t *family, float *size_dip_out) {
    float dpi = (float)sui_get_window_dpi(r->hwnd);
    float use_size = size > 0.0f ? size : r->font_size;
    float cfs = use_size * dpi / 72.0f;
    int custom_size = (size > 0.0f && size != r->font_size);
    int custom_family = (family && family[0]);
    if (size_dip_out) *size_dip_out = cfs;
    if (custom_family) {
        return sui__resolve_fmt(r, family, cfs,
                                sui__body_weight(r, bold), sui__body_style(r), 0);
    }
    if (custom_size) {
        if (bold && size > r->font_size * 1.5f) {
            int tw = (int)r->typography.title.weight, ts = sui__title_style(r);
            return sui__resolve_fmt(r, r->typography.title.family, cfs, tw, ts, 1);
        }
        return sui__resolve_fmt(r, r->typography.body.family, cfs,
                                sui__body_weight(r, bold), sui__body_style(r), 0);
    }
    return bold ? (IDWriteTextFormat*)r->font_bold : (IDWriteTextFormat*)r->font_normal;
}
#endif /* SUI_FEATURE_CUSTOM_CONTROLS */

static void sui_renderer_draw_text(SuiRenderer* r, const wchar_t* str, float x, float y, float w, float h,
                                  uint32_t color, int bold, float size) {
    ID2D1RenderTarget* rt; ID2D1SolidColorBrush* br; D2D1_RECT_F rc;
    IDWriteTextFormat* fmt; int custom;
    if (!r->render_target || !str) return;
    rt = (ID2D1RenderTarget*)r->render_target;
    br = sui__brush(r, color, 1.0f);
    if (!br) return;
    custom = (size > 0 && size != r->font_size);
    if (custom) {
        float cfs = size * (float)sui_get_window_dpi(r->hwnd) / 72.0f;
        if (bold && size > r->font_size * 1.5f) {
            int tw = (int)r->typography.title.weight, ts = sui__title_style(r);
            fmt = sui__resolve_fmt(r, r->typography.title.family, cfs, tw, ts, 1);
        } else {
            fmt = sui__resolve_fmt(r, r->typography.body.family, cfs,
                                    sui__body_weight(r, bold), sui__body_style(r), 0);
        }
    } else {
        fmt = bold ? (IDWriteTextFormat*)r->font_bold : (IDWriteTextFormat*)r->font_normal;
    }
    if (fmt) {
        rc.left=x; rc.top=y; rc.right=x+w; rc.bottom=y+h;
        rt->lpVtbl->DrawText(rt, str, (UINT32)wcslen(str), fmt, &rc, (ID2D1Brush*)br,
            D2D1_DRAW_TEXT_OPTIONS_NONE, DWRITE_MEASURING_MODE_GDI_NATURAL);
        /* fmt is cache-owned when custom — do not Release */
    }
}

static void sui_renderer_draw_text_ex(SuiRenderer* r, const wchar_t* str, float x, float y, float w, float h,
                                      uint32_t color, int bold, float size, const wchar_t* family) {
    ID2D1RenderTarget* rt; IDWriteTextFormat* fmt; ID2D1SolidColorBrush* br; D2D1_RECT_F rc;
    if (!r->render_target || !str) return;
    rt = (ID2D1RenderTarget*)r->render_target;
    br = sui__brush(r, color, 1.0f);
    if (!br) return;
    fmt = sui__resolve_text_fmt_ex(r, bold, size, family, NULL);
    if (fmt) {
        rc.left=x; rc.top=y; rc.right=x+w; rc.bottom=y+h;
        rt->lpVtbl->DrawText(rt, str, (UINT32)wcslen(str), fmt, &rc, (ID2D1Brush*)br,
            D2D1_DRAW_TEXT_OPTIONS_NONE, DWRITE_MEASURING_MODE_GDI_NATURAL);
    }
}

/* Measure the height of text when wrapped to a given width. Returns 0 on failure. */
static float sui_renderer_measure_text_height(SuiRenderer* r, const wchar_t* str, float w, int bold, float size) {
    IDWriteFactory* dw; IDWriteTextFormat* fmt; IDWriteTextLayout* layout;
    DWRITE_TEXT_METRICS metrics; int custom; float h;
    if (!r->dwrite_factory || !str) return 0;
    dw = (IDWriteFactory*)r->dwrite_factory;
    custom = (size > 0 && size != r->font_size);
    if (custom) {
        float cfs = size * (float)sui_get_window_dpi(r->hwnd) / 72.0f;
        if (bold && size > r->font_size * 1.5f) {
            int tw = (int)r->typography.title.weight, ts = sui__title_style(r);
            fmt = sui__resolve_fmt(r, r->typography.title.family, cfs, tw, ts, 1);
        } else {
            fmt = sui__resolve_fmt(r, r->typography.body.family, cfs,
                                    sui__body_weight(r, bold), sui__body_style(r), 0);
        }
    } else {
        fmt = bold ? (IDWriteTextFormat*)r->font_bold : (IDWriteTextFormat*)r->font_normal;
    }
    if (!fmt) return 0;
    if (FAILED(sui__text_layout(dw, str, (UINT32)wcslen(str), fmt, w, 10000.0f, &layout)))
        return 0;
    h = 0;
    memset(&metrics, 0, sizeof(metrics));
    if (SUCCEEDED(layout->lpVtbl->GetMetrics(layout, &metrics))) h = metrics.height;
    layout->lpVtbl->Release(layout);
    /* fmt is cache-owned when custom — do not Release */
    return h;
}

static float sui_renderer_measure_text_height_ex(SuiRenderer* r, const wchar_t* str, float w,
                                                 int bold, float size, const wchar_t* family) {
    IDWriteFactory* dw; IDWriteTextFormat* fmt; IDWriteTextLayout* layout;
    DWRITE_TEXT_METRICS metrics; float h;
    if (!r->dwrite_factory || !str) return 0;
    dw = (IDWriteFactory*)r->dwrite_factory;
    fmt = sui__resolve_text_fmt_ex(r, bold, size, family, NULL);
    if (!fmt) return 0;
    if (FAILED(sui__text_layout(dw, str, (UINT32)wcslen(str), fmt, w, 10000.0f, &layout)))
        return 0;
    h = 0;
    memset(&metrics, 0, sizeof(metrics));
    if (SUCCEEDED(layout->lpVtbl->GetMetrics(layout, &metrics))) h = metrics.height;
    layout->lpVtbl->Release(layout);
    return h;
}

/* Measure the natural (unwrapped) width of a single-line text string. Returns 0 on failure. */
static float sui_renderer_measure_text_width(SuiRenderer* r, const wchar_t* str, int bold, float size) {
    IDWriteFactory* dw; IDWriteTextFormat* fmt; IDWriteTextLayout* layout;
    DWRITE_TEXT_METRICS metrics; int custom; float w;
    if (!r->dwrite_factory || !str || !str[0]) return 0;
    dw = (IDWriteFactory*)r->dwrite_factory;
    custom = (size > 0 && size != r->font_size);
    if (custom) {
        float cfs = size * (float)sui_get_window_dpi(r->hwnd) / 72.0f;
        if (bold && size > r->font_size * 1.5f) {
            int tw = (int)r->typography.title.weight, ts = sui__title_style(r);
            fmt = sui__resolve_fmt(r, r->typography.title.family, cfs, tw, ts, 1);
        } else {
            fmt = sui__resolve_fmt(r, r->typography.body.family, cfs,
                                    sui__body_weight(r, bold), sui__body_style(r), 0);
        }
    } else {
        fmt = bold ? (IDWriteTextFormat*)r->font_bold : (IDWriteTextFormat*)r->font_normal;
    }
    if (!fmt) return 0;
    if (FAILED(sui__text_layout(dw, str, (UINT32)wcslen(str), fmt, 10000.0f, 10000.0f, &layout)))
        return 0;
    w = 0;
    memset(&metrics, 0, sizeof(metrics));
    if (SUCCEEDED(layout->lpVtbl->GetMetrics(layout, &metrics))) w = metrics.width;
    layout->lpVtbl->Release(layout);
    /* fmt is cache-owned when custom — do not Release */
    return w;
}

static float sui_renderer_measure_text_width_ex(SuiRenderer* r, const wchar_t* str,
                                                int bold, float size, const wchar_t* family) {
    IDWriteFactory* dw; IDWriteTextFormat* fmt; IDWriteTextLayout* layout;
    DWRITE_TEXT_METRICS metrics; float w;
    if (!r->dwrite_factory || !str || !str[0]) return 0;
    dw = (IDWriteFactory*)r->dwrite_factory;
    fmt = sui__resolve_text_fmt_ex(r, bold, size, family, NULL);
    if (!fmt) return 0;
    if (FAILED(sui__text_layout(dw, str, (UINT32)wcslen(str), fmt, 10000.0f, 10000.0f, &layout)))
        return 0;
    w = 0;
    memset(&metrics, 0, sizeof(metrics));
    if (SUCCEEDED(layout->lpVtbl->GetMetrics(layout, &metrics))) w = metrics.width;
    layout->lpVtbl->Release(layout);
    return w;
}

static float sui_renderer_measure_text_width_weight_ex(SuiRenderer* r, const wchar_t* str,
                                                       int weight, float size, const wchar_t* family) {
    IDWriteFactory* dw; IDWriteTextFormat* fmt; IDWriteTextLayout* layout;
    DWRITE_TEXT_METRICS metrics; float w, dpi, cfs;
    const wchar_t* use_family;
    if (!r->dwrite_factory || !str || !str[0]) return 0;
    dw = (IDWriteFactory*)r->dwrite_factory;
    dpi = (float)sui_get_window_dpi(r->hwnd);
    cfs = (size > 0.0f ? size : r->font_size) * dpi / 72.0f;
    use_family = (family && family[0]) ? family : r->typography.body.family;
    fmt = sui__resolve_fmt(r, use_family, cfs, weight, sui__body_style(r), 0);
    if (!fmt) return 0;
    if (FAILED(sui__text_layout(dw, str, (UINT32)wcslen(str), fmt, 10000.0f, 10000.0f, &layout)))
        return 0;
    w = 0;
    memset(&metrics, 0, sizeof(metrics));
    if (SUCCEEDED(layout->lpVtbl->GetMetrics(layout, &metrics))) w = metrics.width;
    layout->lpVtbl->Release(layout);
    return w;
}

/* Draw text horizontally and vertically centered within the given rect.
 * Uses cached font_normal when size matches body; alignment is applied to
 * a per-call IDWriteTextLayout so the cached format is never mutated. */
static void sui_renderer_draw_text_centered(SuiRenderer* r, const wchar_t* str,
    float x, float y, float w, float h, uint32_t color, float size) {
    ID2D1RenderTarget* rt; IDWriteFactory* dw; IDWriteTextFormat* fmt;
    IDWriteTextLayout* layout; ID2D1SolidColorBrush* br;
    int custom; float dpi, cfs; D2D1_POINT_2F origin;
    if (!r->render_target || !str) return;
    rt = (ID2D1RenderTarget*)r->render_target;
    dw = (IDWriteFactory*)r->dwrite_factory;
    if (!dw) return;
    br = sui__brush(r, color, 1.0f);
    if (!br) return;
    dpi = (float)sui_get_window_dpi(r->hwnd);
    custom = (size > 0 && size != r->font_size);
    if (custom) {
        cfs = size * dpi / 72.0f;
        fmt = sui__resolve_fmt(r, r->typography.body.family, cfs,
                                sui__body_weight(r, 0), sui__body_style(r), 0);
        if (!fmt) return;
    } else {
        fmt = (IDWriteTextFormat*)r->font_normal;
        cfs = r->typography.body.size_pt * dpi / 72.0f;
        if (!fmt) return;
    }
    if (FAILED(sui__text_layout(dw, str, (UINT32)wcslen(str), fmt, w, h, &layout)) || !layout)
        return;
    layout->lpVtbl->SetTextAlignment(layout, DWRITE_TEXT_ALIGNMENT_CENTER);
    layout->lpVtbl->SetParagraphAlignment(layout, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    /* Nudge up slightly — DWrite's vertical center includes descenders, looks low visually */
    origin.x = x; origin.y = y - cfs * 0.06f;
    rt->lpVtbl->DrawTextLayout(rt, origin, layout, (ID2D1Brush*)br, 0);
    layout->lpVtbl->Release(layout);
    /* fmt is cache-owned when custom — do not Release */
}

static void sui_renderer_draw_text_centered_ex(SuiRenderer* r, const wchar_t* str,
    float x, float y, float w, float h, uint32_t color, int bold, float size,
    const wchar_t* family) {
    ID2D1RenderTarget* rt; IDWriteFactory* dw; IDWriteTextFormat* fmt;
    IDWriteTextLayout* layout; ID2D1SolidColorBrush* br;
    float cfs; D2D1_POINT_2F origin;
    if (!r->render_target || !str) return;
    rt = (ID2D1RenderTarget*)r->render_target;
    dw = (IDWriteFactory*)r->dwrite_factory;
    if (!dw) return;
    br = sui__brush(r, color, 1.0f);
    if (!br) return;
    fmt = sui__resolve_text_fmt_ex(r, bold, size, family, &cfs);
    if (!fmt) return;
    if (FAILED(sui__text_layout(dw, str, (UINT32)wcslen(str), fmt, w, h, &layout)) || !layout)
        return;
    layout->lpVtbl->SetTextAlignment(layout, DWRITE_TEXT_ALIGNMENT_CENTER);
    layout->lpVtbl->SetParagraphAlignment(layout, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    origin.x = x; origin.y = y - cfs * 0.06f;
    rt->lpVtbl->DrawTextLayout(rt, origin, layout, (ID2D1Brush*)br, 0);
    layout->lpVtbl->Release(layout);
}

static void sui_renderer_draw_text_middle(SuiRenderer* r, const wchar_t* str,
    float x, float y, float w, float h, uint32_t color, int bold, float size) {
    ID2D1RenderTarget* rt; IDWriteFactory* dw; IDWriteTextFormat* fmt;
    IDWriteTextLayout* layout; ID2D1SolidColorBrush* br;
    int custom; float dpi, cfs; D2D1_POINT_2F origin;
    if (!r->render_target || !str) return;
    rt = (ID2D1RenderTarget*)r->render_target;
    dw = (IDWriteFactory*)r->dwrite_factory;
    if (!dw) return;
    br = sui__brush(r, color, 1.0f);
    if (!br) return;
    dpi = (float)sui_get_window_dpi(r->hwnd);
    custom = (size > 0 && size != r->font_size);
    if (custom) {
        cfs = size * dpi / 72.0f;
        if (bold && size > r->font_size * 1.5f) {
            int tw = (int)r->typography.title.weight, ts = sui__title_style(r);
            fmt = sui__resolve_fmt(r, r->typography.title.family, cfs, tw, ts, 1);
        } else {
            fmt = sui__resolve_fmt(r, r->typography.body.family, cfs,
                                    sui__body_weight(r, bold), sui__body_style(r), 0);
        }
        if (!fmt) return;
    } else {
        fmt = bold ? (IDWriteTextFormat*)r->font_bold : (IDWriteTextFormat*)r->font_normal;
        cfs = r->typography.body.size_pt * dpi / 72.0f;
        if (!fmt) return;
    }
    if (FAILED(sui__text_layout(dw, str, (UINT32)wcslen(str), fmt, w, h, &layout)) || !layout)
        return;
    layout->lpVtbl->SetTextAlignment(layout, 0);
    layout->lpVtbl->SetParagraphAlignment(layout, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    origin.x = x; origin.y = y - cfs * 0.04f;
    rt->lpVtbl->DrawTextLayout(rt, origin, layout, (ID2D1Brush*)br, 0);
    layout->lpVtbl->Release(layout);
    /* fmt is cache-owned when custom — do not Release */
}

static void sui_renderer_draw_text_middle_ex(SuiRenderer* r, const wchar_t* str,
    float x, float y, float w, float h, uint32_t color, int bold, float size,
    const wchar_t* family) {
    ID2D1RenderTarget* rt; IDWriteFactory* dw; IDWriteTextFormat* fmt;
    IDWriteTextLayout* layout; ID2D1SolidColorBrush* br;
    float cfs; D2D1_POINT_2F origin;
    if (!r->render_target || !str) return;
    rt = (ID2D1RenderTarget*)r->render_target;
    dw = (IDWriteFactory*)r->dwrite_factory;
    if (!dw) return;
    br = sui__brush(r, color, 1.0f);
    if (!br) return;
    fmt = sui__resolve_text_fmt_ex(r, bold, size, family, &cfs);
    if (!fmt) return;
    if (FAILED(sui__text_layout(dw, str, (UINT32)wcslen(str), fmt, w, h, &layout)) || !layout)
        return;
    layout->lpVtbl->SetTextAlignment(layout, 0);
    layout->lpVtbl->SetParagraphAlignment(layout, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    origin.x = x; origin.y = y - cfs * 0.04f;
    rt->lpVtbl->DrawTextLayout(rt, origin, layout, (ID2D1Brush*)br, 0);
    layout->lpVtbl->Release(layout);
}

static void sui_renderer_draw_text_middle_weight_ex(SuiRenderer* r, const wchar_t* str,
    float x, float y, float w, float h, uint32_t color, int weight, float size,
    const wchar_t* family) {
    ID2D1RenderTarget* rt; IDWriteFactory* dw; IDWriteTextFormat* fmt;
    IDWriteTextLayout* layout; ID2D1SolidColorBrush* br;
    float cfs, dpi; D2D1_POINT_2F origin;
    const wchar_t* use_family;
    if (!r->render_target || !str) return;
    rt = (ID2D1RenderTarget*)r->render_target;
    dw = (IDWriteFactory*)r->dwrite_factory;
    if (!dw) return;
    br = sui__brush(r, color, 1.0f);
    if (!br) return;
    dpi = (float)sui_get_window_dpi(r->hwnd);
    cfs = (size > 0.0f ? size : r->font_size) * dpi / 72.0f;
    use_family = (family && family[0]) ? family : r->typography.body.family;
    fmt = sui__resolve_fmt(r, use_family, cfs, weight, sui__body_style(r), 0);
    if (!fmt) return;
    if (FAILED(sui__text_layout(dw, str, (UINT32)wcslen(str), fmt, w, h, &layout)) || !layout)
        return;
    layout->lpVtbl->SetTextAlignment(layout, 0);
    layout->lpVtbl->SetParagraphAlignment(layout, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    origin.x = x; origin.y = y - cfs * 0.04f;
    rt->lpVtbl->DrawTextLayout(rt, origin, layout, (ID2D1Brush*)br, 0);
    layout->lpVtbl->Release(layout);
}

static void sui_renderer_draw_text_middle_singleline(SuiRenderer* r, const wchar_t* str,
    float x, float y, float w, float h, uint32_t color, int bold, float size) {
    ID2D1RenderTarget* rt;
    D2D1_RECT_F clip;
    if (!r->render_target || !str) return;
    rt = (ID2D1RenderTarget*)r->render_target;
    clip.left = x; clip.top = y; clip.right = x + w; clip.bottom = y + h;
    rt->lpVtbl->PushAxisAlignedClip(rt, &clip, 0);
    sui_renderer_draw_text_middle(r, str, x, y, 4096.0f, h, color, bold, size);
    rt->lpVtbl->PopAxisAlignedClip(rt);
}

static void sui_renderer_draw_text_middle_singleline_ex(SuiRenderer* r, const wchar_t* str,
    float x, float y, float w, float h, uint32_t color, int bold, float size,
    const wchar_t* family) {
    ID2D1RenderTarget* rt;
    D2D1_RECT_F clip;
    if (!r->render_target || !str) return;
    rt = (ID2D1RenderTarget*)r->render_target;
    clip.left = x; clip.top = y; clip.right = x + w; clip.bottom = y + h;
    rt->lpVtbl->PushAxisAlignedClip(rt, &clip, 0);
    sui_renderer_draw_text_middle_ex(r, str, x, y, 4096.0f, h, color, bold, size, family);
    rt->lpVtbl->PopAxisAlignedClip(rt);
}

static void sui_renderer_draw_text_middle_singleline_weight_ex(SuiRenderer* r, const wchar_t* str,
    float x, float y, float w, float h, uint32_t color, int weight, float size,
    const wchar_t* family) {
    ID2D1RenderTarget* rt;
    D2D1_RECT_F clip;
    if (!r->render_target || !str) return;
    rt = (ID2D1RenderTarget*)r->render_target;
    clip.left = x; clip.top = y; clip.right = x + w; clip.bottom = y + h;
    rt->lpVtbl->PushAxisAlignedClip(rt, &clip, 0);
    sui_renderer_draw_text_middle_weight_ex(r, str, x, y, 4096.0f, h, color,
                                            weight, size, family);
    rt->lpVtbl->PopAxisAlignedClip(rt);
}

/* Draw one non-wrapping line and replace overflow at the trailing edge with an
 * ellipsis. Alignment lives on the per-call layout so cached text formats stay
 * immutable and can safely be shared by controls. */
static void sui_renderer_draw_text_singleline_tail_ellipsis_weight_alpha_ex(
    SuiRenderer* r, const wchar_t* str,
    float x, float y, float w, float h, uint32_t color, float alpha,
    int weight, float size, const wchar_t* family, int middle) {
    ID2D1RenderTarget* rt;
    IDWriteFactory* dw;
    IDWriteTextFormat* fmt;
    IDWriteTextLayout* layout;
    IDWriteInlineObject* ellipsis = NULL;
    ID2D1SolidColorBrush* br;
    DWRITE_TRIMMING trimming;
    D2D1_POINT_2F origin;
    float dpi, cfs;
    const wchar_t* use_family;
    if (!r || !r->render_target || !r->dwrite_factory || !str || !str[0] || w <= 0.0f || h <= 0.0f)
        return;
    rt = (ID2D1RenderTarget*)r->render_target;
    dw = (IDWriteFactory*)r->dwrite_factory;
    br = sui__brush(r, color, alpha);
    if (!br) return;
    dpi = (float)sui_get_window_dpi(r->hwnd);
    cfs = (size > 0.0f ? size : r->font_size) * dpi / 72.0f;
    use_family = (family && family[0]) ? family : r->typography.body.family;
    fmt = sui__resolve_fmt(r, use_family, cfs, weight, sui__body_style(r), 0);
    if (!fmt) return;
    if (FAILED(sui__text_layout(dw, str, (UINT32)wcslen(str), fmt, w, h, &layout)) || !layout)
        return;
    layout->lpVtbl->SetTextAlignment(layout, 0);
    layout->lpVtbl->SetParagraphAlignment(layout,
        middle ? DWRITE_PARAGRAPH_ALIGNMENT_CENTER : 0);
    layout->lpVtbl->SetWordWrapping(layout, DWRITE_WORD_WRAPPING_NO_WRAP);
    trimming.granularity = DWRITE_TRIMMING_GRANULARITY_CHARACTER;
    trimming.delimiter = 0;
    trimming.delimiterCount = 0;
    if (SUCCEEDED(dw->lpVtbl->CreateEllipsisTrimmingSign(dw, fmt, &ellipsis)) && ellipsis)
        layout->lpVtbl->SetTrimming(layout, &trimming, ellipsis);
    origin.x = x;
    origin.y = middle ? y - cfs * 0.04f : y;
    rt->lpVtbl->DrawTextLayout(rt, origin, layout, (ID2D1Brush*)br, 0);
    if (ellipsis) ellipsis->lpVtbl->Release(ellipsis);
    layout->lpVtbl->Release(layout);
}

static void sui_renderer_draw_line(SuiRenderer* r, float x1, float y1, float x2, float y2, uint32_t color) {
    ID2D1RenderTarget* rt; ID2D1SolidColorBrush* br;
    D2D1_POINT_2F p1, p2;
    br = sui__brush(r, color, 1.0f);
    if (!br) return;
    rt = (ID2D1RenderTarget*)r->render_target;
    p1.x=x1; p1.y=y1; p2.x=x2; p2.y=y2;
    rt->lpVtbl->DrawLine(rt, p1, p2, (ID2D1Brush*)br, 1.0f, NULL);
}

static void sui_renderer_draw_line_thick(SuiRenderer* r, float x1, float y1, float x2, float y2,
                                         float thickness, uint32_t color) {
    ID2D1RenderTarget* rt; ID2D1SolidColorBrush* br;
    D2D1_POINT_2F p1, p2;
    br = sui__brush(r, color, 1.0f);
    if (!br) return;
    rt = (ID2D1RenderTarget*)r->render_target;
    p1.x=x1; p1.y=y1; p2.x=x2; p2.y=y2;
    rt->lpVtbl->DrawLine(rt, p1, p2, (ID2D1Brush*)br, thickness, NULL);
}

/* ---- continuous rounded shapes ------------------------------------------------------------------------------------------------
 * Every rounded rect the renderer draws has the continuous corner of sui_corner.h (the radius clamped to half the shorter side).
 * A shape is path geometry at the origin in the renderer's shape cache (SuiShapeCache), built on first use and drawn under a
 * translation, so a shape seen again builds nothing; Direct2D's own rounded rect draws what the factory cannot build, and every
 * rounded rect of a build without the C runtime (SUI_NO_CRT: the shared path needs its math). */
enum { SUI__SHAPE_FILL, SUI__SHAPE_BORDER, SUI__SHAPE_INNER_RING, SUI__SHAPE_OUTSIDE };
/* A rounded rect's radius: clamped to half the shorter side, 0 when it is not positive. */
static float sui__round_radius(float w, float h, float radius) {
    const float half = 0.5f * (w < h ? w : h);
    return radius > 0 && half > 0 ? (radius < half ? radius : half) : 0.0f;
}
#ifndef SUI_NO_CRT
/* The path's contours as a path geometry, filled alternate (a ring's two contours make the ring). NULL on failure. */
static ID2D1PathGeometry* sui__path_geometry(ID2D1Factory* fac, const SuiCornerPath* path) {
    ID2D1PathGeometry* g = NULL; ID2D1GeometrySink* sink = NULL; HRESULT hr; size_t i;
    if (!fac || FAILED(fac->lpVtbl->CreatePathGeometry(fac, &g)) || !g) return NULL;
    if (FAILED(g->lpVtbl->Open(g, &sink)) || !sink) { g->lpVtbl->Release(g); return NULL; }
    for (i = 0; i < path->count; i++) {
        const SuiCornerCommand* c = &path->commands[i];
        D2D1_POINT_2F a; D2D1_BEZIER_SEGMENT b;
        a.x = (float)c->p[0].x; a.y = (float)c->p[0].y;
        switch (c->op) {
        case SUI_CORNER_MOVE: sink->lpVtbl->BeginFigure(sink, a, 0 /* D2D1_FIGURE_BEGIN_FILLED */); break;
        case SUI_CORNER_LINE: sink->lpVtbl->AddLines(sink, &a, 1); break;
        case SUI_CORNER_CUBIC:
            b.point1 = a; b.point2.x = (float)c->p[1].x; b.point2.y = (float)c->p[1].y;
            b.point3.x = (float)c->p[2].x; b.point3.y = (float)c->p[2].y;
            sink->lpVtbl->AddBeziers(sink, &b, 1); break;
        default: sink->lpVtbl->EndFigure(sink, 1 /* D2D1_FIGURE_END_CLOSED */); break;
        }
    }
    hr = sink->lpVtbl->Close(sink);
    sink->lpVtbl->Release(sink);
    if (FAILED(hr)) { g->lpVtbl->Release(g); return NULL; }
    return g;
}
/* The w x h shape of `kind` at (x, y), radius r (clamped), into `path`. FILL: the continuous rounded rect. BORDER: what a stroke of
 * width `ring` centred on that rect's edge covers, as a ring between the shape ring/2 outside it and ring/2 inside it (its inner
 * radius r - ring/2, as CSS insets a border's inner edge). INNER_RING: a ring of width `ring` inside the rect, its inner edge the
 * continuous rounded rect `ring` inside with radius r - ring and its outer edge that edge's parallel curve, so the ring keeps its
 * width all the way round. OUTSIDE: the rect less the rounded rect (what lies outside its corners). */
static int sui__shape_path(SuiCornerPath* path, float x, float y, float w, float h, float r, float ring, int kind) {
    SuiCornerRect b, in; double k;
    b.x = x; b.y = y; b.width = w; b.height = h;
    switch (kind) {
    case SUI__SHAPE_FILL: return sui_corner_path_round_rect(path, b, r);
    case SUI__SHAPE_BORDER:
        k = 0.5 * ring;
        in.x = x - k; in.y = y - k; in.width = w + ring; in.height = h + ring;
        if (!sui_corner_path_round_rect(path, in, r + k)) return 0;
        in.x = x + k; in.y = y + k; in.width = w - ring; in.height = h - ring;
        return !(in.width > 0 && in.height > 0) || sui_corner_path_round_rect(path, in, r - k > 0 ? r - k : 0);
    case SUI__SHAPE_INNER_RING:
        if (!sui_corner_path_round_rect_offset(path, b, r, ring)) return 0;
        in.x = x + ring; in.y = y + ring; in.width = w - 2 * ring; in.height = h - 2 * ring;
        return !(in.width > 0 && in.height > 0) || sui_corner_path_round_rect(path, in, r - ring > 0 ? r - ring : 0);
    default:
        return sui_corner_path_round_rect(path, b, 0) && sui_corner_path_round_rect(path, b, r);
    }
}
static ID2D1PathGeometry* sui__make_shape(ID2D1Factory* fac, float x, float y, float w, float h, float r, float ring, int kind) {
    SuiCornerCommand storage[2 * SUI_CORNER_OFFSET_PATH_CAPACITY]; SuiCornerPath path;
    sui_corner_path_init(&path, storage, 2 * SUI_CORNER_OFFSET_PATH_CAPACITY);
    return sui__shape_path(&path, x, y, w, h, r, ring, kind) ? sui__path_geometry(fac, &path) : NULL;
}
#else
static ID2D1PathGeometry* sui__make_shape(ID2D1Factory* fac, float x, float y, float w, float h, float r, float ring, int kind) {
    (void)fac; (void)x; (void)y; (void)w; (void)h; (void)r; (void)ring; (void)kind;
    return NULL;
}
#endif
/* The cached w x h shape at the origin (radius already clamped), built into the least recently used slot on a miss. */
static ID2D1PathGeometry* sui__shape(SuiRenderer* r, float w, float h, float radius, float ring, int kind) {
    SuiShapeCache* c = &r->shape_cache; SuiShapeCacheEntry* victim = &c->slots[0]; ID2D1PathGeometry* made; int i;
    for (i = 0; i < SUI_SHAPE_CACHE_SLOTS; i++) {
        SuiShapeCacheEntry* e = &c->slots[i];
        if (e->geometry && e->w == w && e->h == h && e->radius == radius && e->ring == ring && e->kind == kind) {
            e->tick = ++c->tick; return (ID2D1PathGeometry*)e->geometry;
        }
        if (!e->geometry ? victim->geometry != NULL : (victim->geometry && e->tick < victim->tick)) victim = e;
    }
    made = sui__make_shape((ID2D1Factory*)r->d2d_factory, 0, 0, w, h, radius, ring, kind);
    if (!made) return NULL;
    if (victim->geometry) ((ID2D1PathGeometry*)victim->geometry)->lpVtbl->Release(victim->geometry);
    victim->geometry = made; victim->w = w; victim->h = h; victim->radius = radius; victim->ring = ring; victim->kind = kind;
    victim->tick = ++c->tick; ++c->builds;
    return made;
}
static void sui__shape_cache_release(SuiShapeCache* c) {
    int i;
    for (i = 0; i < SUI_SHAPE_CACHE_SLOTS; i++)
        if (c->slots[i].geometry) { ((ID2D1PathGeometry*)c->slots[i].geometry)->lpVtbl->Release(c->slots[i].geometry); c->slots[i].geometry = NULL; }
}
/* Fills the cached shape of kind at (x, y) with `br`, under a translation of the target's transform. 0 when it cannot be built. */
static int sui__fill_shape(SuiRenderer* r, float x, float y, float w, float h, float radius, float ring, int kind, ID2D1Brush* br) {
    ID2D1RenderTarget* rt = (ID2D1RenderTarget*)r->render_target; ID2D1PathGeometry* g; D2D1_MATRIX_3X2_F m, t;
    if (!(w > 0) || !(h > 0) || !(g = sui__shape(r, w, h, radius, ring, kind))) return 0;
    rt->lpVtbl->GetTransform(rt, &m); t = m;
    t._31 = x * m._11 + y * m._21 + m._31; t._32 = x * m._12 + y * m._22 + m._32;
    rt->lpVtbl->SetTransform(rt, &t);
    rt->lpVtbl->FillGeometry(rt, (ID2D1Geometry*)g, br, NULL);
    rt->lpVtbl->SetTransform(rt, &m);
    return 1;
}
/* A rounded rect drawn once, its shape built and released here (a layered window's soft shadow: many sizes, drawn when the window
 * is prepared, which would only churn the cache). */
static void sui__fill_round_once(SuiRenderer* r, D2D1_ROUNDED_RECT rr, ID2D1Brush* br) {
    ID2D1RenderTarget* rt = (ID2D1RenderTarget*)r->render_target; ID2D1PathGeometry* g;
    const float w = rr.rect.right - rr.rect.left, h = rr.rect.bottom - rr.rect.top;
    const float k = sui__round_radius(w, h, rr.radiusX);
    g = k > 0 ? sui__make_shape((ID2D1Factory*)r->d2d_factory, rr.rect.left, rr.rect.top, w, h, k, 0, SUI__SHAPE_FILL) : NULL;
    if (!g) { rt->lpVtbl->FillRoundedRectangle(rt, &rr, br); return; }
    rt->lpVtbl->FillGeometry(rt, (ID2D1Geometry*)g, br, NULL);
    g->lpVtbl->Release(g);
}

static void sui__fill_round(SuiRenderer* r, float x, float y, float w, float h, float radius, uint32_t color, float alpha) {
    ID2D1RenderTarget* rt; ID2D1SolidColorBrush* br; D2D1_ROUNDED_RECT rr; float k;
    br = sui__brush(r, color, alpha);
    if (!br) return;
    rt = (ID2D1RenderTarget*)r->render_target;
    k = sui__round_radius(w, h, radius);
    if (k > 0 && sui__fill_shape(r, x, y, w, h, k, 0, SUI__SHAPE_FILL, (ID2D1Brush*)br)) return;
    rr.rect.left=x; rr.rect.top=y; rr.rect.right=x+w; rr.rect.bottom=y+h;
    rr.radiusX=k; rr.radiusY=k;
    rt->lpVtbl->FillRoundedRectangle(rt, &rr, (ID2D1Brush*)br);
}
static void sui_renderer_fill_rounded_rect(SuiRenderer* r, float x, float y, float w, float h,
                                           float radius, uint32_t color) {
    sui__fill_round(r, x, y, w, h, radius, color, 1.0f);
}

static void sui_renderer_fill_rounded_rect_alpha(SuiRenderer* r, float x, float y, float w, float h,
                                                 float radius, uint32_t color, float alpha) {
    if (alpha <= 0.001f) return;
    if (alpha > 1.0f) alpha = 1.0f;
    sui__fill_round(r, x, y, w, h, radius, color, alpha);
}

/* The rect's border: a stroke of width `stroke` centred on its edge, its corners the continuous corner's ring (SUI__SHAPE_BORDER). */
static void sui_renderer_draw_rounded_rect(SuiRenderer* r, float x, float y, float w, float h,
                                           float radius, uint32_t color, float stroke) {
    ID2D1RenderTarget* rt; ID2D1SolidColorBrush* br; D2D1_ROUNDED_RECT rr; float k;
    br = sui__brush(r, color, 1.0f);
    if (!br) return;
    rt = (ID2D1RenderTarget*)r->render_target;
    k = sui__round_radius(w, h, radius);
    if (k > 0 && stroke > 0 && sui__fill_shape(r, x, y, w, h, k, stroke, SUI__SHAPE_BORDER, (ID2D1Brush*)br)) return;
    rr.rect.left=x; rr.rect.top=y; rr.rect.right=x+w; rr.rect.bottom=y+h;
    rr.radiusX=k; rr.radiusY=k;
    rt->lpVtbl->DrawRoundedRectangle(rt, &rr, (ID2D1Brush*)br, stroke, NULL);
}

/* Paints what lies outside the rounded rect's corners (a clip's corners masked in the background color). */
static void sui_renderer_mask_outside_rounded_rect(SuiRenderer* r, float x, float y, float w, float h,
                                                   float radius, uint32_t color) {
    ID2D1Factory* fac = (ID2D1Factory*)r->d2d_factory;
    ID2D1RenderTarget* rt = (ID2D1RenderTarget*)r->render_target;
    ID2D1RectangleGeometry* rect_geo = NULL; ID2D1RoundedRectangleGeometry* round_geo = NULL; ID2D1GeometryGroup* group = NULL;
    ID2D1Geometry* geoms[2]; ID2D1SolidColorBrush* br; D2D1_ROUNDED_RECT rounded; float k;
    if (!fac || !rt || w <= 0.0f || h <= 0.0f) return;
    k = sui__round_radius(w, h, radius);
    if (!(k > 0) || !(br = sui__brush(r, color, 1.0f))) return;
    if (sui__fill_shape(r, x, y, w, h, k, 0, SUI__SHAPE_OUTSIDE, (ID2D1Brush*)br)) return;
    /* without the shared path: the rect less Direct2D's rounded rect */
    rounded.rect.left = x; rounded.rect.top = y; rounded.rect.right = x + w; rounded.rect.bottom = y + h;
    rounded.radiusX = rounded.radiusY = k;
    if (FAILED(fac->lpVtbl->CreateRectangleGeometry(fac, &rounded.rect, &rect_geo))) return;
    if (SUCCEEDED(fac->lpVtbl->CreateRoundedRectangleGeometry(fac, &rounded, &round_geo))) {
        geoms[0] = (ID2D1Geometry*)rect_geo; geoms[1] = (ID2D1Geometry*)round_geo;
        if (SUCCEEDED(fac->lpVtbl->CreateGeometryGroup(fac, D2D1_FILL_MODE_ALTERNATE, geoms, 2, &group))) {
            rt->lpVtbl->FillGeometry(rt, (ID2D1Geometry*)group, (ID2D1Brush*)br, NULL);
            group->lpVtbl->Release((void*)group);
        }
        round_geo->lpVtbl->Release((void*)round_geo);
    }
    rect_geo->lpVtbl->Release((void*)rect_geo);
}

/* A focus ring of width ring_w inside the rect (SUI__SHAPE_INNER_RING): its inner edge the element's continuous corner and its
 * outer edge that corner's parallel curve, uniform in width at the straight edges and around the corners. */
static void sui_renderer_draw_filled_ring(SuiRenderer* r, float x, float y, float w, float h,
                                          float outer_radius, float ring_w, uint32_t color) {
    ID2D1SolidColorBrush* br; float k;
    if (!r->d2d_factory || !r->render_target || !(ring_w > 0)) return;
    br = sui__brush(r, color, 1.0f);
    if (!br) return;
    k = sui__round_radius(w, h, outer_radius);
    if (!sui__fill_shape(r, x, y, w, h, k, ring_w, SUI__SHAPE_INNER_RING, (ID2D1Brush*)br)) {
        D2D1_ROUNDED_RECT rr;   /* without the shared path: Direct2D's rounded rect stroked on the ring's middle */
        rr.rect.left = x + 0.5f * ring_w; rr.rect.top = y + 0.5f * ring_w;
        rr.rect.right = x + w - 0.5f * ring_w; rr.rect.bottom = y + h - 0.5f * ring_w;
        rr.radiusX = rr.radiusY = k - 0.5f * ring_w > 0 ? k - 0.5f * ring_w : 0.0f;
        ((ID2D1RenderTarget*)r->render_target)->lpVtbl->DrawRoundedRectangle((ID2D1RenderTarget*)r->render_target, &rr, (ID2D1Brush*)br, ring_w, NULL);
    }
}

#ifdef SUI_FEATURE_ANIMATIONS
static void sui_renderer_draw_progress(SuiRenderer* r, float x, float y, float w, float h,
                                       float progress, uint32_t fill_color, uint32_t bg_color) {
    ID2D1RenderTarget* rt = (ID2D1RenderTarget*)r->render_target;
    float radius = h / 2.0f;
    if (radius > 6.0f) radius = 6.0f;
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;

    /* Track — full rounded rect */
    sui_renderer_fill_rounded_rect(r, x, y, w, h, radius, bg_color);

    /* Filled portion — clip to progress width */
    if (progress > 0.0f && rt) {
        D2D1_RECT_F clip;
        clip.left = x; clip.top = y;
        clip.right = x + w * progress; clip.bottom = y + h;
        rt->lpVtbl->PushAxisAlignedClip(rt, &clip, 0);
        sui_renderer_fill_rounded_rect(r, x, y, w, h, radius, fill_color);
        rt->lpVtbl->PopAxisAlignedClip(rt);
    }
}
#else
static void sui_renderer_draw_progress(SuiRenderer* r, float x, float y, float w, float h,
                                       float progress, uint32_t fill_color, uint32_t bg_color) {
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    sui_renderer_fill_rect(r, x, y, w, h, bg_color);
    if (progress > 0.0f) sui_renderer_fill_rect(r, x, y, w * progress, h, fill_color);
}
#endif

/* D2D image support — decoded via WIC, uploaded as ID2D1Bitmap.
 * WIC outputs PBGRA32 (premultiplied BGRA); CreateBitmapFromWicBitmap uploads
 * it directly without any manual pixel-swizzle loop. */
static SuiImageHandle sui_renderer_load_image(SuiRenderer* r, const uint8_t* png, size_t png_size) {
    SuiImageHandle img = {NULL, 0, 0};
#ifdef SUI_FEATURE_PNG
    IWICImagingFactory*    wic       = NULL;
    IWICBitmapDecoder*     decoder   = NULL;
    IWICBitmapFrameDecode* frame     = NULL;
    IWICFormatConverter*   converter = NULL;
    IStream*               stream    = NULL;
    HGLOBAL                hg;
    void*                  p;
    ID2D1Bitmap*           bmp = NULL;
    HRESULT                hr;

    if (!r->render_target || !png || png_size == 0) return img;

    hr = CoCreateInstance(SUI_IID_REF(CLSID_WICImagingFactory), NULL, CLSCTX_INPROC_SERVER,
                          SUI_IID_REF(IID_IWICImagingFactory), (void**)&wic);
    if (FAILED(hr) || !wic) return img;

    hg = GlobalAlloc(GMEM_MOVEABLE, png_size);
    if (!hg) goto cleanup;
    p = GlobalLock(hg);
    if (!p) { GlobalFree(hg); goto cleanup; }
    memcpy(p, png, png_size);
    GlobalUnlock(hg);
    hr = CreateStreamOnHGlobal(hg, TRUE, &stream);
    if (FAILED(hr) || !stream) { GlobalFree(hg); goto cleanup; }

    hr = wic->lpVtbl->CreateDecoderFromStream(wic, stream, NULL,
             (DWORD)WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr) || !decoder) goto cleanup;

    hr = decoder->lpVtbl->GetFrame(decoder, 0, &frame);
    if (FAILED(hr) || !frame) goto cleanup;

    { UINT32 w = 0, h = 0;
      hr = frame->lpVtbl->Base.GetSize((IWICBitmapSource*)frame, &w, &h);
      if (FAILED(hr)) goto cleanup;
      img.width = (int)w; img.height = (int)h;
    }

    hr = wic->lpVtbl->CreateFormatConverter(wic, &converter);
    if (FAILED(hr) || !converter) goto cleanup;

    hr = converter->lpVtbl->Initialize(converter, (IWICBitmapSource*)frame,
             &GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, NULL, 0.0,
             WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) goto cleanup;

    { D2D1_BITMAP_PROPERTIES bp;
      memset(&bp, 0, sizeof(bp));
      bp.pixelFormat.format    = DXGI_FORMAT_B8G8R8A8_UNORM;
      bp.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
      hr = ((ID2D1RenderTarget*)r->render_target)->lpVtbl->CreateBitmapFromWicBitmap(
               (ID2D1RenderTarget*)r->render_target,
               (IWICBitmapSource*)converter, &bp, &bmp);
      if (SUCCEEDED(hr) && bmp) img.data = bmp;
    }

cleanup:
    if (converter) converter->lpVtbl->Base.Release((IWICBitmapSource*)converter);
    if (frame)     frame->lpVtbl->Base.Release((IWICBitmapSource*)frame);
    if (decoder)   decoder->lpVtbl->Release(decoder);
    if (stream) {
#ifdef __cplusplus
        ((::IStream*)stream)->Release();   /* system IUnknown lacks lpVtbl in C++ TUs */
#else
        ((IUnknown*)stream)->lpVtbl->Release((IUnknown*)stream);
#endif
    }
    if (wic)       wic->lpVtbl->Release(wic);
#else
    (void)r; (void)png; (void)png_size;
#endif
    return img;
}

static void sui_renderer_draw_image(SuiRenderer* r, SuiImageHandle img, float x, float y, float w, float h) {
#ifdef SUI_FEATURE_PNG
    if (!r->render_target || !img.data) return;
    { D2D1_RECT_F dest; dest.left=x; dest.top=y; dest.right=x+w; dest.bottom=y+h;
      ((ID2D1RenderTarget*)r->render_target)->lpVtbl->DrawBitmap((ID2D1RenderTarget*)r->render_target,
          (ID2D1Bitmap*)img.data, &dest, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, NULL); }
#else
    (void)r; (void)img; (void)x; (void)y; (void)w; (void)h;
#endif
}

static void sui_renderer_free_image(SuiRenderer* r, SuiImageHandle img) {
#ifdef SUI_FEATURE_PNG
    (void)r;
    if (img.data) ((ID2D1Bitmap*)img.data)->lpVtbl->Base.Base.Release((IUnknown*)img.data);
#else
    (void)r; (void)img;
#endif
}

static void sui_renderer_fill_ellipse(SuiRenderer* r, float cx, float cy, float radius, uint32_t color) {
    ID2D1RenderTarget* rt; ID2D1SolidColorBrush* br; D2D1_ELLIPSE el;
    br = sui__brush(r, color, 1.0f);
    if (!br) return;
    rt = (ID2D1RenderTarget*)r->render_target;
    el.point.x = cx; el.point.y = cy; el.radiusX = radius; el.radiusY = radius;
    rt->lpVtbl->FillEllipse(rt, &el, (ID2D1Brush*)br);
}

static void sui_renderer_draw_ellipse(SuiRenderer* r, float cx, float cy, float radius, uint32_t color, float stroke) {
    ID2D1RenderTarget* rt; ID2D1SolidColorBrush* br; D2D1_ELLIPSE el;
    br = sui__brush(r, color, 1.0f);
    if (!br) return;
    rt = (ID2D1RenderTarget*)r->render_target;
    el.point.x = cx; el.point.y = cy; el.radiusX = radius; el.radiusY = radius;
    rt->lpVtbl->DrawEllipse(rt, &el, (ID2D1Brush*)br, stroke, NULL);
}


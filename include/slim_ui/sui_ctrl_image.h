/* sui_ctrl_image.h — Bitmap display control. Not focusable, no input.
 * Included from slim_ui.h under #ifdef SUI_IMPLEMENTATION.
 *
 * Drawn via sui_renderer_draw_image, with the dest rect computed from
 * the chosen SuiImageScale mode (FILL stretches, FIT letterboxes,
 * CLIP covers and crops). Default mode is SUI_IMG_FIT.
 *
 * circle_mask is reserved for future circular clipping; for now use a
 * pre-masked image (e.g. a PNG with alpha around a circular portrait).
 */

static int sui_ctrl_add_image(SuiControlHost* host, int id, float x, float y, float w, float h,
                              SuiImageHandle img, int circle_mask) {
    int idx = sui_ctrl_add(host, SUI_CTRL_IMAGE, id, x, y, w, h, L"");
    if (idx >= 0) {
        host->controls[idx].d.image.img = img;
        host->controls[idx].d.image.circle_mask = circle_mask;
        host->controls[idx].d.image.scale = SUI_IMG_FILL;
    }
    return idx;
}

static void sui_ctrl_image_set(SuiControlHost* host, int id, SuiImageHandle img) {
    SuiControl* c = sui_ctrl_find(host, id);
    if (c && c->type == SUI_CTRL_IMAGE) {
        c->d.image.img = img;
    }
}

static void sui_ctrl_image_set_scale(SuiControlHost* host, int id, SuiImageScale scale) {
    SuiControl* c = sui_ctrl_find(host, id);
    if (c && c->type == SUI_CTRL_IMAGE) {
        c->d.image.scale = (int)scale;
    }
}

static void sui_ctrl_draw_image(SuiControlHost* host, SuiControl* ctrl) {
    SuiRenderer* r = host->renderer;
    if (ctrl->state & SUI_CTRL_HIDDEN) return;
    if (sui_ctrl_style_has(ctrl, SUI_STYLE_BG_COLOR)) {
        sui_renderer_fill_rect(r, ctrl->x, ctrl->y, ctrl->w, ctrl->h,
            sui_ctrl_style_color(ctrl, SUI_STYLE_BG_COLOR, host->scheme->background));
    }
    if (ctrl->d.image.img.data) {
        SuiImageHandle img = ctrl->d.image.img;
        float dx = ctrl->x, dy = ctrl->y, dw = ctrl->w, dh = ctrl->h;
        int   need_clip = 0;
        if (img.width > 0 && img.height > 0 && ctrl->d.image.scale != SUI_IMG_FILL) {
            float img_aspect = (float)img.width  / (float)img.height;
            float box_aspect = ctrl->w           / ctrl->h;
            int   wider      = img_aspect > box_aspect;
            if (ctrl->d.image.scale == SUI_IMG_FIT) {
                /* Letterbox: smaller scale factor wins, center inside box. */
                if (wider) { dw = ctrl->w; dh = ctrl->w / img_aspect; }
                else       { dh = ctrl->h; dw = ctrl->h * img_aspect; }
            } else { /* SUI_IMG_CLIP */
                /* Cover: larger scale factor wins, center, crop overflow via clip. */
                if (wider) { dh = ctrl->h; dw = ctrl->h * img_aspect; }
                else       { dw = ctrl->w; dh = ctrl->w / img_aspect; }
                need_clip = 1;
            }
            dx = ctrl->x + (ctrl->w - dw) * 0.5f;
            dy = ctrl->y + (ctrl->h - dh) * 0.5f;
        }
        if (need_clip && host->renderer && host->renderer->render_target) {
            ID2D1RenderTarget* rt = (ID2D1RenderTarget*)host->renderer->render_target;
            D2D1_RECT_F clip;
            clip.left = ctrl->x; clip.top = ctrl->y;
            clip.right = ctrl->x + ctrl->w; clip.bottom = ctrl->y + ctrl->h;
            rt->lpVtbl->PushAxisAlignedClip(rt, &clip, 0 /* D2D1_ANTIALIAS_MODE_PER_PRIMITIVE */);
            sui_renderer_draw_image(host->renderer, img, dx, dy, dw, dh);
            rt->lpVtbl->PopAxisAlignedClip(rt);
        } else {
            sui_renderer_draw_image(host->renderer, img, dx, dy, dw, dh);
        }
        if (sui_ctrl_style_has(ctrl, SUI_STYLE_BORDER_COLOR)) {
            sui_renderer_draw_rounded_rect(r, ctrl->x, ctrl->y, ctrl->w, ctrl->h, 0.0f,
                sui_ctrl_style_color(ctrl, SUI_STYLE_BORDER_COLOR, host->scheme->btn_area_border), 1.0f);
        }
        return;
    }
    if (ctrl->d.image.circle_mask) {
        float radius = (ctrl->w < ctrl->h ? ctrl->w : ctrl->h) * 0.5f;
        float cx = ctrl->x + ctrl->w * 0.5f;
        float cy = ctrl->y + ctrl->h * 0.5f;
        uint32_t fill = sui_ctrl_style_color(ctrl, SUI_STYLE_BG_COLOR, host->scheme->nav_selected);
        uint32_t border = sui_ctrl_style_color(ctrl, SUI_STYLE_BORDER_COLOR, host->scheme->nav_border);
        uint32_t text = sui_ctrl_style_color(ctrl, SUI_STYLE_TEXT_COLOR, host->scheme->text_secondary);
        sui_renderer_fill_ellipse(r, cx, cy, radius, fill);
        sui_renderer_draw_ellipse(r, cx, cy, radius, border, 1.0f);
        if (r->dwrite_factory && r->render_target) {
            ID2D1RenderTarget* rt = (ID2D1RenderTarget*)r->render_target;
            IDWriteFactory* dw = (IDWriteFactory*)r->dwrite_factory;
            IDWriteTextFormat* icon_fmt = NULL;
            ID2D1SolidColorBrush* br = NULL;
            D2D1_COLOR_F c;
            D2D1_RECT_F rc;
            float icon_pt = 14.0f * sui_ctrl_dpi_scale(host);
            const wchar_t* icon_family = (host->renderer->typography.icon.family[0] != L'\0')
                ? host->renderer->typography.icon.family : L"Segoe Fluent Icons";
            int icon_weight = host->renderer->typography.icon.weight;
            if (icon_weight < 100) icon_weight = DWRITE_FONT_WEIGHT_NORMAL;
            if (SUCCEEDED(dw->lpVtbl->CreateTextFormat(dw, icon_family, NULL,
                icon_weight, DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL, icon_pt, L"", &icon_fmt))) {
                icon_fmt->lpVtbl->SetTextAlignment(icon_fmt, DWRITE_TEXT_ALIGNMENT_CENTER);
                icon_fmt->lpVtbl->SetParagraphAlignment(icon_fmt, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                c.r=(float)((text>>16)&0xFF)/255.0f;
                c.g=(float)((text>>8)&0xFF)/255.0f;
                c.b=(float)(text&0xFF)/255.0f;
                c.a=1.0f;
                if (SUCCEEDED(rt->lpVtbl->CreateSolidColorBrush(rt, &c, NULL, &br))) {
                    rc.left = ctrl->x; rc.top = ctrl->y;
                    rc.right = ctrl->x + ctrl->w; rc.bottom = ctrl->y + ctrl->h;
                    rt->lpVtbl->DrawText(rt, L"\xE77B", 1, icon_fmt, &rc, (ID2D1Brush*)br,
                        D2D1_DRAW_TEXT_OPTIONS_NONE, DWRITE_MEASURING_MODE_NATURAL);
                    br->lpVtbl->Base.Base.Release((IUnknown*)br);
                }
                icon_fmt->lpVtbl->Release(icon_fmt);
            }
        }
    }
}

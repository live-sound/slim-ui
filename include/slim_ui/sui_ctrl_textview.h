/* sui_ctrl_textview.h — Multi-line text view: scrollable, optionally editable,
 * soft-wrap to control width. Backing store is heap-allocated via sui_malloc;
 * the host frees it on clear_page (for discarded slots) and host_release.
 *
 * Multi-line text view: optional readonly mode, soft-wrap, selection,
 * clipboard, undo. Mirrors sui_ctrl_edit's editable-text plumbing.
 *
 * Included from slim_ui.h under #ifdef SUI_IMPLEMENTATION.
 */

/* Forward decl — defined in sui_controls.h (included after this header). */
static void sui_ctrl_start_hover_anim(SuiControlHost* host);

/* ── Internal helpers ──────────────────────────────────────────────────────── */

/* Build a layout sized to the inner content width.
 * Caller must Release — the cache holds its own ref; AddRef balances the caller's Release. */
static IDWriteTextLayout* sui__textview_make_layout(SuiControlHost* host, SuiControl* ctrl) {
    SuiRenderer* r = host->renderer;
    IDWriteFactory* dw;
    IDWriteTextFormat* fmt;
    IDWriteTextLayout* layout = NULL;
    float s, pad, sbw, text_w;
    int n;
    int bold;
    float font_size;
    const wchar_t* font_family;
    const wchar_t* buf;
    UINT32 h;
    void *displaced;
    if (!r->dwrite_factory || !r->font_normal) return NULL;
    dw  = (IDWriteFactory*)r->dwrite_factory;
    bold = (ctrl->state & SUI_CTRL_BOLD) ? 1 : 0;
    font_size = sui_ctrl_style_font_size(ctrl, r->font_size);
    font_family = sui_ctrl_style_font_family(ctrl);
    fmt = sui__resolve_text_fmt_ex(r, bold, font_size, font_family, NULL);
    if (!fmt) return NULL;
    s   = sui_ctrl_dpi_scale(host);
    pad = 8.0f * s;
    sbw = 10.0f * s;
    text_w = ctrl->w - pad * 2.0f - sbw;
    if (text_w < 1.0f) text_w = 1.0f;
    buf = ctrl->d.textview.buf ? ctrl->d.textview.buf : L"";
    n   = (int)ctrl->d.textview.len;
    h   = sui__text_hash(buf);
    layout = (IDWriteTextLayout *)sui__layout_cache_lookup(&host->layout_cache,
                                                            h, n, fmt, text_w, 100000.0f);
    if (layout) {
        layout->lpVtbl->AddRef(layout);
        return layout;
    }
    if (FAILED(sui__text_layout(dw, buf, (UINT32)n, fmt, text_w, 100000.0f, &layout)) || !layout)
        return NULL;
    displaced = sui__layout_cache_store(&host->layout_cache, layout, fmt, h, n, text_w, 100000.0f);
    if (displaced) { IDWriteTextLayout *d = (IDWriteTextLayout *)displaced; d->lpVtbl->Release(d); }
    layout->lpVtbl->AddRef(layout);
    return layout;
}

static void sui__textview_update_content_height(SuiControlHost* host, SuiControl* ctrl) {
    IDWriteTextLayout* layout = sui__textview_make_layout(host, ctrl);
    if (!layout) return;
    { DWRITE_TEXT_METRICS m; memset(&m, 0, sizeof(m));
      if (SUCCEEDED(layout->lpVtbl->GetMetrics(layout, &m)))
          ctrl->d.textview.content_height = m.height;
    }
    layout->lpVtbl->Release(layout);
}

/* ── Selection / undo helpers ─────────────────────────────────────────────── */

static int sui_ctrl_textview_has_sel(SuiControl* c) {
    return c->d.textview.sel_start >= 0 && c->d.textview.sel_start != c->d.textview.sel_end;
}

static void sui__textview_clear_sel(SuiControl* c) {
    c->d.textview.sel_start = -1;
    c->d.textview.sel_end   = -1;
}

/* Save current buf+cursor to undo_buf and clear any pending redo. */
static void sui__textview_save_undo(SuiControl* c) {
    int n = c->d.textview.len;
    if (!c->d.textview.undo_buf || !c->d.textview.buf) return;
    if (n > c->d.textview.capacity - 1) n = c->d.textview.capacity - 1;
    sui__wmove(c->d.textview.undo_buf, c->d.textview.buf, n);
    c->d.textview.undo_buf[n] = L'\0';
    c->d.textview.undo_len    = n;
    c->d.textview.undo_cursor = c->d.textview.cursor;
    c->d.textview.redo_len    = -1;
}

/* Restore from undo_buf; save current state to redo_buf. */
static void sui__textview_undo(SuiControl* c) {
    int tmp_len, tmp_cur;
    if (!c->d.textview.buf || !c->d.textview.undo_buf || !c->d.textview.redo_buf) return;
    if (c->d.textview.undo_len < 0) return;
    tmp_len = c->d.textview.len;
    tmp_cur = c->d.textview.cursor;
    sui__wmove(c->d.textview.redo_buf, c->d.textview.buf, tmp_len);
    c->d.textview.redo_buf[tmp_len] = L'\0';
    c->d.textview.redo_len    = tmp_len;
    c->d.textview.redo_cursor = tmp_cur;
    sui__wmove(c->d.textview.buf, c->d.textview.undo_buf, c->d.textview.undo_len);
    c->d.textview.buf[c->d.textview.undo_len] = L'\0';
    c->d.textview.len    = c->d.textview.undo_len;
    c->d.textview.cursor = c->d.textview.undo_cursor;
    c->d.textview.undo_len = -1;
    sui__textview_clear_sel(c);
}

/* Restore from redo_buf; save current state back to undo_buf. */
static void sui__textview_redo(SuiControl* c) {
    int tmp_len, tmp_cur;
    if (!c->d.textview.buf || !c->d.textview.undo_buf || !c->d.textview.redo_buf) return;
    if (c->d.textview.redo_len < 0) return;
    tmp_len = c->d.textview.len;
    tmp_cur = c->d.textview.cursor;
    sui__wmove(c->d.textview.undo_buf, c->d.textview.buf, tmp_len);
    c->d.textview.undo_buf[tmp_len] = L'\0';
    c->d.textview.undo_len    = tmp_len;
    c->d.textview.undo_cursor = tmp_cur;
    sui__wmove(c->d.textview.buf, c->d.textview.redo_buf, c->d.textview.redo_len);
    c->d.textview.buf[c->d.textview.redo_len] = L'\0';
    c->d.textview.len    = c->d.textview.redo_len;
    c->d.textview.cursor = c->d.textview.redo_cursor;
    c->d.textview.redo_len = -1;
    sui__textview_clear_sel(c);
}

/* Delete the [a, b) range from buf; returns new len. Cursor placed at a. */
static void sui__textview_delete_range(SuiControl* c, int a, int b) {
    int len = c->d.textview.len;
    if (a < 0) a = 0;
    if (b > len) b = len;
    if (a >= b) return;
    sui__wmove(c->d.textview.buf + a, c->d.textview.buf + b, len - b);
    c->d.textview.len = len - (b - a);
    c->d.textview.buf[c->d.textview.len] = L'\0';
    c->d.textview.cursor = a;
}

static void sui__textview_delete_sel(SuiControl* c) {
    int a, b;
    if (!sui_ctrl_textview_has_sel(c)) return;
    a = c->d.textview.sel_start < c->d.textview.sel_end ? c->d.textview.sel_start : c->d.textview.sel_end;
    b = c->d.textview.sel_start < c->d.textview.sel_end ? c->d.textview.sel_end   : c->d.textview.sel_start;
    sui__textview_delete_range(c, a, b);
    sui__textview_clear_sel(c);
}

/* Insert a wchar_t run at the cursor. Truncates if it would exceed capacity. */
static void sui__textview_insert(SuiControl* c, const wchar_t* src, int n) {
    int len = c->d.textview.len;
    int cap = c->d.textview.capacity;
    int max_ins, cur;
    if (!c->d.textview.buf || cap < 1 || n <= 0) return;
    max_ins = (cap - 1) - len;
    if (n > max_ins) n = max_ins;
    if (n <= 0) return;
    cur = c->d.textview.cursor;
    sui__wmove(c->d.textview.buf + cur + n, c->d.textview.buf + cur, len - cur);
    sui__wmove(c->d.textview.buf + cur, src, n);
    c->d.textview.len = len + n;
    c->d.textview.buf[c->d.textview.len] = L'\0';
    c->d.textview.cursor = cur + n;
}

/* ── Clipboard ────────────────────────────────────────────────────────────── */

static void sui_ctrl_textview_copy(SuiControlHost* host, SuiControl* c) {
    int a, b, n;
    HGLOBAL hg;
    wchar_t* p;
    if (!sui_ctrl_textview_has_sel(c)) return;
    a = c->d.textview.sel_start < c->d.textview.sel_end ? c->d.textview.sel_start : c->d.textview.sel_end;
    b = c->d.textview.sel_start < c->d.textview.sel_end ? c->d.textview.sel_end   : c->d.textview.sel_start;
    n = b - a;
    if (n <= 0) return;
    if (!OpenClipboard(host->hwnd)) return;
    hg = GlobalAlloc(GMEM_MOVEABLE, (size_t)(n + 1) * sizeof(wchar_t));
    if (hg) {
        p = (wchar_t*)GlobalLock(hg);
        if (p) {
            sui__wmove(p, c->d.textview.buf + a, n);
            p[n] = L'\0';
            GlobalUnlock(hg);
            EmptyClipboard();
            SetClipboardData(CF_UNICODETEXT, hg);
        }
    }
    CloseClipboard();
}

static void sui_ctrl_textview_cut(SuiControlHost* host, SuiControl* c) {
    if (c->d.textview.readonly) return;
    if (!sui_ctrl_textview_has_sel(c)) return;
    sui_ctrl_textview_copy(host, c);
    sui__textview_save_undo(c);
    sui__textview_delete_sel(c);
}

static void sui_ctrl_textview_paste(SuiControlHost* host, SuiControl* c) {
    HANDLE hd;
    if (c->d.textview.readonly) return;
    if (!OpenClipboard(host->hwnd)) return;
    hd = GetClipboardData(CF_UNICODETEXT);
    if (hd) {
        wchar_t* src = (wchar_t*)GlobalLock(hd);
        if (src) {
            int src_len = (int)wcslen(src);
            sui__textview_save_undo(c);
            if (sui_ctrl_textview_has_sel(c)) sui__textview_delete_sel(c);
            /* Normalize \r\n -> \n on the fly via a heap copy. */
            { wchar_t* norm = (wchar_t*)sui_malloc((size_t)(src_len + 1) * sizeof(wchar_t));
              if (norm) {
                  int i, j = 0;
                  for (i = 0; i < src_len; i++) {
                      if (src[i] == L'\r' && i + 1 < src_len && src[i + 1] == L'\n') continue;
                      norm[j++] = src[i];
                  }
                  norm[j] = L'\0';
                  sui__textview_insert(c, norm, j);
                  sui_free(norm);
              }
            }
            GlobalUnlock(hd);
        }
    }
    CloseClipboard();
}

/* ── Word boundaries (reuse edit helpers semantics, but on heap buf) ──────── */

static int sui__textview_is_word_char(wchar_t ch) {
    return ch != L' ' && ch != L'\t' && ch != L'\n';
}

static int sui__textview_word_left(const wchar_t* t, int len, int pos) {
    if (pos > len) pos = len;
    if (pos <= 0) return 0;
    while (pos > 0 && !sui__textview_is_word_char(t[pos - 1])) pos--;
    while (pos > 0 &&  sui__textview_is_word_char(t[pos - 1])) pos--;
    return pos;
}

static int sui__textview_word_right(const wchar_t* t, int len, int pos) {
    if (pos < 0) pos = 0;
    while (pos < len &&  sui__textview_is_word_char(t[pos])) pos++;
    while (pos < len && !sui__textview_is_word_char(t[pos])) pos++;
    return pos;
}

static int sui__textview_word_start(const wchar_t* t, int pos) {
    while (pos > 0 && sui__textview_is_word_char(t[pos - 1])) pos--;
    return pos;
}

static int sui__textview_word_end(const wchar_t* t, int len, int pos) {
    while (pos < len && sui__textview_is_word_char(t[pos])) pos++;
    return pos;
}

/* ── Hit-testing via DWrite ───────────────────────────────────────────────── */

/* Returns char index for a click point given in local (control-relative) coords.
 * (lx, ly) are relative to the control's top-left; the caller need not
 * pre-subtract padding/scroll — we do that here. */
static int sui__textview_hit_point(SuiControlHost* host, SuiControl* ctrl, float lx, float ly) {
    IDWriteTextLayout* layout;
    float s = sui_ctrl_dpi_scale(host);
    float pad = 8.0f * s;
    float text_x = lx - pad;
    float text_y = ly - pad + ctrl->d.textview.scroll_offset;
    BOOL  is_trailing = FALSE, is_inside = FALSE;
    DWRITE_HIT_TEST_METRICS m;
    int   result;
    if (text_x < 0.0f) text_x = 0.0f;
    if (text_y < 0.0f) text_y = 0.0f;
    layout = sui__textview_make_layout(host, ctrl);
    if (!layout) return 0;
    memset(&m, 0, sizeof(m));
    layout->lpVtbl->HitTestPoint(layout, text_x, text_y, &is_trailing, &is_inside, &m);
    result = (int)m.textPosition + (is_trailing ? (int)m.length : 0);
    layout->lpVtbl->Release(layout);
    if (result < 0) result = 0;
    if (result > ctrl->d.textview.len) result = ctrl->d.textview.len;
    return result;
}

/* Caret rect (top-left + height) for char position `pos`, relative to text
 * origin (i.e. before adding ctrl->x + pad / ctrl->y + pad - scroll_offset). */
static void sui__textview_caret_metrics(IDWriteTextLayout* layout, int pos,
                                        float* out_x, float* out_y, float* out_h) {
    DWRITE_HIT_TEST_METRICS m;
    FLOAT cx = 0.0f, cy = 0.0f;
    memset(&m, 0, sizeof(m));
    layout->lpVtbl->HitTestTextPosition(layout, (UINT32)pos, FALSE, &cx, &cy, &m);
    if (out_x) *out_x = (float)cx;
    if (out_y) *out_y = (float)cy;
    if (out_h) *out_h = (float)m.height;
}

/* ── Scroll to caret ──────────────────────────────────────────────────────── */

static void sui__textview_scroll_to_caret(SuiControlHost* host, SuiControl* ctrl) {
    IDWriteTextLayout* layout = sui__textview_make_layout(host, ctrl);
    float s = sui_ctrl_dpi_scale(host), pad = 8.0f * s;
    float visible_h = ctrl->h - pad * 2.0f;
    float cy = 0.0f, ch = 16.0f;
    if (!layout) return;
    sui__textview_caret_metrics(layout, ctrl->d.textview.cursor, NULL, &cy, &ch);
    layout->lpVtbl->Release(layout);
    if (cy + ch - ctrl->d.textview.scroll_offset > visible_h)
        ctrl->d.textview.scroll_offset = cy + ch - visible_h;
    if (cy < ctrl->d.textview.scroll_offset)
        ctrl->d.textview.scroll_offset = cy;
    if (ctrl->d.textview.scroll_offset < 0.0f) ctrl->d.textview.scroll_offset = 0.0f;
}

/* ── Constructor ──────────────────────────────────────────────────────────── */

static int sui_ctrl_add_textview(SuiControlHost* host, int id,
                                 float x, float y, float w, float h,
                                 const wchar_t* initial_text,
                                 int capacity_chars,
                                 int readonly) {
    int idx;
    int init_len;
    if (capacity_chars < 1) capacity_chars = 1;
    idx = sui_ctrl_add(host, SUI_CTRL_TEXTVIEW, id, x, y, w, h, NULL);
    if (idx < 0) return idx;
    {
        SuiControl* c = &host->controls[idx];
        c->d.textview.buf      = (wchar_t*)sui_malloc((size_t)capacity_chars * sizeof(wchar_t));
        c->d.textview.undo_buf = (wchar_t*)sui_malloc((size_t)capacity_chars * sizeof(wchar_t));
        c->d.textview.redo_buf = (wchar_t*)sui_malloc((size_t)capacity_chars * sizeof(wchar_t));
        c->d.textview.capacity = capacity_chars;
        if (!c->d.textview.buf || !c->d.textview.undo_buf || !c->d.textview.redo_buf) {
            if (c->d.textview.buf)      { sui_free(c->d.textview.buf);      c->d.textview.buf = NULL; }
            if (c->d.textview.undo_buf) { sui_free(c->d.textview.undo_buf); c->d.textview.undo_buf = NULL; }
            if (c->d.textview.redo_buf) { sui_free(c->d.textview.redo_buf); c->d.textview.redo_buf = NULL; }
            c->d.textview.capacity = 0;
            host->control_count = idx; /* roll back the slot — buf is null, don't leave a live control */
            return -1;
        }
        c->d.textview.buf[0]      = L'\0';
        c->d.textview.undo_buf[0] = L'\0';
        c->d.textview.redo_buf[0] = L'\0';
        c->d.textview.len         = 0;
        c->d.textview.undo_len    = -1;
        c->d.textview.redo_len    = -1;
        c->d.textview.cursor      = 0;
        c->d.textview.sel_start   = -1;
        c->d.textview.sel_end     = -1;
        c->d.textview.drag_sel_anchor = 0;
        c->d.textview.undo_cursor = 0;
        c->d.textview.redo_cursor = 0;
        c->d.textview.readonly    = readonly ? 1 : 0;
        c->d.textview.scroll_offset  = 0.0f;
        c->d.textview.content_height = 0.0f;
        if (initial_text) {
            init_len = (int)wcslen(initial_text);
            if (init_len > capacity_chars - 1) init_len = capacity_chars - 1;
            sui__wmove(c->d.textview.buf, initial_text, init_len);
            c->d.textview.buf[init_len] = L'\0';
            c->d.textview.len = init_len;
            c->d.textview.cursor = init_len;
        }
    }
    return idx;
}

/* ── Public mutators ──────────────────────────────────────────────────────── */

static void sui_ctrl_textview_set_text(SuiControlHost* host, int id, const wchar_t* text) {
    SuiControl* c = sui_ctrl_find(host, id);
    int n;
    if (!c || c->type != SUI_CTRL_TEXTVIEW || !c->d.textview.buf) return;
    n = text ? (int)wcslen(text) : 0;
    if (n > c->d.textview.capacity - 1) n = c->d.textview.capacity - 1;
    if (n > 0) sui__wmove(c->d.textview.buf, text, n);
    c->d.textview.buf[n] = L'\0';
    c->d.textview.len = n;
    c->d.textview.cursor = 0;
    sui__textview_clear_sel(c);
    c->d.textview.scroll_offset  = 0.0f;
    c->d.textview.content_height = 0.0f;
}

static const wchar_t* sui_ctrl_textview_get_text(SuiControlHost* host, int id) {
    SuiControl* c = sui_ctrl_find(host, id);
    if (!c || c->type != SUI_CTRL_TEXTVIEW) return L"";
    return c->d.textview.buf ? c->d.textview.buf : L"";
}

static void sui_ctrl_textview_set_readonly(SuiControlHost* host, int id, int readonly) {
    SuiControl* c = sui_ctrl_find(host, id);
    if (!c || c->type != SUI_CTRL_TEXTVIEW) return;
    c->d.textview.readonly = readonly ? 1 : 0;
}

/* ── Draw ─────────────────────────────────────────────────────────────────── */

static void sui_ctrl_draw_textview(SuiControlHost* host, SuiControl* ctrl) {
    SuiRenderer* r = host->renderer;
    const SuiColorScheme* sc = host->scheme;
    float s = sui_ctrl_dpi_scale(host), rad = 4.0f * s;
    float pad = 8.0f * s, sbw = 10.0f * s;
    float text_x, text_y_origin, visible_h;
    int focused = (ctrl->state & SUI_CTRL_FOCUSED) != 0;
    ID2D1RenderTarget* rt;
    IDWriteTextLayout* layout = NULL;
    uint32_t bg_color = sui_ctrl_style_color(ctrl, SUI_STYLE_BG_COLOR, sc->input_bg);
    uint32_t border_base = sui_ctrl_style_color(ctrl, SUI_STYLE_BORDER_COLOR, sc->input_border);
    uint32_t accent_color = sui_ctrl_style_color(ctrl, SUI_STYLE_ACCENT_COLOR, sc->input_focus_border);
    uint32_t hover_border = sui_ctrl_style_color(ctrl, SUI_STYLE_ACCENT_COLOR, sc->btn_default_border);
    uint32_t text_color = sui_ctrl_style_color(ctrl, SUI_STYLE_TEXT_COLOR, sc->input_text);
    uint32_t selection_color = sui_ctrl_style_color(ctrl, SUI_STYLE_SELECTION_COLOR, sc->input_selection);

    if (ctrl->state & SUI_CTRL_HIDDEN) return;
    if (!ctrl->d.textview.buf) return;

    sui_renderer_fill_rounded_rect(r, ctrl->x, ctrl->y, ctrl->w, ctrl->h, rad, bg_color);
    sui_renderer_draw_rounded_rect(r, ctrl->x, ctrl->y, ctrl->w, ctrl->h, rad,
        focused ? accent_color
                : sui_color_lerp(border_base, hover_border, ctrl->hover_t),
        focused ? 2.0f : 1.0f);

    visible_h = ctrl->h - pad * 2.0f;

    layout = sui__textview_make_layout(host, ctrl);
    if (layout) {
        DWRITE_TEXT_METRICS m; memset(&m, 0, sizeof(m));
        if (SUCCEEDED(layout->lpVtbl->GetMetrics(layout, &m)))
            ctrl->d.textview.content_height = m.height;
    }

    /* Clamp scroll */
    {
        float max_scroll = ctrl->d.textview.content_height - visible_h;
        if (max_scroll < 0.0f) max_scroll = 0.0f;
        if (ctrl->d.textview.scroll_offset > max_scroll) ctrl->d.textview.scroll_offset = max_scroll;
        if (ctrl->d.textview.scroll_offset < 0.0f)        ctrl->d.textview.scroll_offset = 0.0f;
    }

    rt = (ID2D1RenderTarget*)r->render_target;
    if (!rt) { if (layout) layout->lpVtbl->Release(layout); return; }

    text_x        = ctrl->x + pad;
    text_y_origin = ctrl->y + pad - ctrl->d.textview.scroll_offset;

    /* Clip text + selection to inner rect */
    {
        D2D1_RECT_F clip;
        clip.left   = ctrl->x + pad;
        clip.top    = ctrl->y + pad;
        clip.right  = ctrl->x + ctrl->w - sbw;
        clip.bottom = ctrl->y + ctrl->h - pad;
        rt->lpVtbl->PushAxisAlignedClip(rt, &clip, 0);

        /* Selection rectangles */
        if (focused && layout && sui_ctrl_textview_has_sel(ctrl)) {
            int a = ctrl->d.textview.sel_start < ctrl->d.textview.sel_end
                  ? ctrl->d.textview.sel_start : ctrl->d.textview.sel_end;
            int b = ctrl->d.textview.sel_start < ctrl->d.textview.sel_end
                  ? ctrl->d.textview.sel_end   : ctrl->d.textview.sel_start;
            UINT32 needed = 0;
            DWRITE_HIT_TEST_METRICS local[16];
            DWRITE_HIT_TEST_METRICS* rects = local;
            DWRITE_HIT_TEST_METRICS* heap_rects = NULL;
            HRESULT hr = layout->lpVtbl->HitTestTextRange(layout,
                (UINT32)a, (UINT32)(b - a), 0.0f, 0.0f,
                local, 16, &needed);
            if (hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER) && needed > 16) {
                heap_rects = (DWRITE_HIT_TEST_METRICS*)sui_malloc(
                    (size_t)needed * sizeof(DWRITE_HIT_TEST_METRICS));
                if (heap_rects) {
                    rects = heap_rects;
                    hr = layout->lpVtbl->HitTestTextRange(layout,
                        (UINT32)a, (UINT32)(b - a), 0.0f, 0.0f,
                        heap_rects, needed, &needed);
                }
            }
            if (SUCCEEDED(hr)) {
                UINT32 i;
                for (i = 0; i < needed; i++) {
                    sui_renderer_fill_rect(r,
                        text_x + (float)rects[i].left,
                        text_y_origin + (float)rects[i].top,
                        (float)rects[i].width,
                        (float)rects[i].height,
                        selection_color);
                }
            }
            if (heap_rects) sui_free(heap_rects);
        }

        /* Text — draw with our layout so positioning matches caret hit-tests. */
        if (ctrl->d.textview.len > 0 && layout) {
            ID2D1SolidColorBrush* br = sui__brush(r, text_color, 1.0f);
            if (br) {
                D2D1_POINT_2F origin;
                origin.x = text_x;
                origin.y = text_y_origin;
                rt->lpVtbl->DrawTextLayout(rt, origin, layout,
                    (ID2D1Brush*)br, 0);
            }
        }

        /* Caret */
        if (focused && layout && !ctrl->d.textview.readonly
                && host->caret_visible && host->window_has_focus) {
            float cx = 0.0f, cy = 0.0f, ch = 14.0f * s;
            sui__textview_caret_metrics(layout, ctrl->d.textview.cursor, &cx, &cy, &ch);
            sui_renderer_fill_rect(r, text_x + cx, text_y_origin + cy,
                1.5f * s, ch, accent_color);
        }

        rt->lpVtbl->PopAxisAlignedClip(rt);
    }

    if (layout) layout->lpVtbl->Release(layout);

    /* Thin overlay scrollbar — combobox-popup style. Visible only on hover or
     * recent keyboard / wheel / drag activity (sb_reveal). */
    if (ctrl->d.textview.content_height > visible_h) {
        float reveal = ctrl->hover_t;
        if (ctrl->d.textview.sb_reveal > reveal) reveal = ctrl->d.textview.sb_reveal;
        if (reveal > 0.0f) {
            float thin_w   = 4.0f * s;
            float sb_x     = ctrl->x + ctrl->w - thin_w - 4.0f * s;
            float sb_y     = ctrl->y + pad;
            float sb_h     = visible_h;
            float ratio    = visible_h / ctrl->d.textview.content_height;
            float thumb_h  = sb_h * ratio;
            float max_scroll = ctrl->d.textview.content_height - visible_h;
            float thumb_y;
            float inset    = 3.0f * s;
            uint32_t color;
            if (thumb_h < 20.0f * s) thumb_h = 20.0f * s;
            thumb_y = sb_y + (sb_h - thumb_h) * (ctrl->d.textview.scroll_offset / max_scroll);
            color = sui_ctrl_style_color(ctrl, SUI_STYLE_ACCENT_COLOR, sc->check_border);
            sui_renderer_fill_rounded_rect_alpha(r, sb_x, thumb_y + inset,
                thin_w, thumb_h - inset * 2.0f, thin_w / 2.0f, color, reveal);
        }
    }

    sui_ctrl_draw_kb_focus_ring(host, ctrl, rad);
}

/* ── Mouse ────────────────────────────────────────────────────────────────── */

static int sui_ctrl_textview_on_lbutton_down(SuiControlHost* host, int idx, int mx, int my) {
    SuiControl* c = &host->controls[idx];
    int   shift_down = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    int   pos = sui__textview_hit_point(host, c, (float)mx - c->x, (float)my - c->y);
    if (shift_down) {
        c->d.textview.sel_start = c->d.textview.drag_sel_anchor;
        c->d.textview.sel_end   = pos;
    } else {
        c->d.textview.drag_sel_anchor = pos;
        sui__textview_clear_sel(c);
    }
    c->d.textview.cursor = pos;
    host->caret_visible  = 1;
    return 1;
}

static int sui_ctrl_textview_on_lbutton_dblclk(SuiControlHost* host, int idx, int mx, int my) {
    SuiControl* c   = &host->controls[idx];
    int pos         = sui__textview_hit_point(host, c, (float)mx - c->x, (float)my - c->y);
    int len         = c->d.textview.len;
    int char_idx    = (pos < len) ? pos : (len > 0 ? len - 1 : 0);
    if (len > 0 && sui__textview_is_word_char(c->d.textview.buf[char_idx])) {
        int a = sui__textview_word_start(c->d.textview.buf, pos);
        int b = sui__textview_word_end(c->d.textview.buf, len, pos);
        c->d.textview.sel_start       = a;
        c->d.textview.sel_end         = b;
        c->d.textview.cursor          = b;
        c->d.textview.drag_sel_anchor = a;
    }
    host->caret_visible = 1;
    return 1;
}

/* ── Keyboard ─────────────────────────────────────────────────────────────── */

static int sui__textview_pos_at_y(SuiControlHost* host, SuiControl* c,
                                  IDWriteTextLayout* layout, float x_hint, float target_y) {
    BOOL is_trailing = FALSE, is_inside = FALSE;
    DWRITE_HIT_TEST_METRICS m; memset(&m, 0, sizeof(m));
    if (target_y < 0.0f) target_y = 0.0f;
    if (target_y > 100000.0f) target_y = 100000.0f;
    layout->lpVtbl->HitTestPoint(layout, x_hint, target_y, &is_trailing, &is_inside, &m);
    {
        int result = (int)m.textPosition + (is_trailing ? (int)m.length : 0);
        if (result < 0) result = 0;
        if (result > c->d.textview.len) result = c->d.textview.len;
        (void)host;
        return result;
    }
}

static int sui_ctrl_textview_on_keydown(SuiControlHost* host, int idx, WPARAM vk) {
    SuiControl* c = &host->controls[idx];
    int ctrl_down  = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    int shift_down = (GetKeyState(VK_SHIFT)   & 0x8000) != 0;
    int len = c->d.textview.len;
    int handled = 1;
    int new_pos = c->d.textview.cursor;
    int extend_sel = shift_down;
    int saved_anchor = c->d.textview.cursor;

    /* Reveal scrollbar — any keystroke against an editable/focused textview
     * counts as activity (caret may move, scroll-to-caret may run). */
    c->d.textview.sb_reveal = 1.0f;
    sui_ctrl_start_hover_anim(host);

    if (extend_sel && c->d.textview.sel_start < 0)
        c->d.textview.sel_start = c->d.textview.cursor;

    switch (vk) {
    case VK_LEFT:
        if (ctrl_down) new_pos = sui__textview_word_left(c->d.textview.buf, len, new_pos);
        else           new_pos = sui__codepoint_left(c->d.textview.buf, new_pos);
        if (extend_sel) c->d.textview.sel_end = new_pos;
        else { if (sui_ctrl_textview_has_sel(c))
                   new_pos = c->d.textview.sel_start < c->d.textview.sel_end
                             ? c->d.textview.sel_start : c->d.textview.sel_end;
               sui__textview_clear_sel(c); }
        c->d.textview.cursor = new_pos;
        break;
    case VK_RIGHT:
        if (ctrl_down) new_pos = sui__textview_word_right(c->d.textview.buf, len, new_pos);
        else           new_pos = sui__codepoint_right(c->d.textview.buf, new_pos, len);
        if (extend_sel) c->d.textview.sel_end = new_pos;
        else { if (sui_ctrl_textview_has_sel(c))
                   new_pos = c->d.textview.sel_start < c->d.textview.sel_end
                             ? c->d.textview.sel_end : c->d.textview.sel_start;
               sui__textview_clear_sel(c); }
        c->d.textview.cursor = new_pos;
        break;
    case VK_UP:
    case VK_DOWN: {
        IDWriteTextLayout* layout = sui__textview_make_layout(host, c);
        if (layout) {
            float cx = 0.0f, cy = 0.0f, ch = 16.0f;
            sui__textview_caret_metrics(layout, c->d.textview.cursor, &cx, &cy, &ch);
            { float ty = (vk == VK_UP) ? (cy - ch * 0.5f) : (cy + ch * 1.5f);
              new_pos = sui__textview_pos_at_y(host, c, layout, cx, ty);
            }
            layout->lpVtbl->Release(layout);
        }
        if (extend_sel) c->d.textview.sel_end = new_pos;
        else            sui__textview_clear_sel(c);
        c->d.textview.cursor = new_pos;
        break;
    }
    case VK_PRIOR:
    case VK_NEXT: {
        IDWriteTextLayout* layout = sui__textview_make_layout(host, c);
        if (layout) {
            float s = sui_ctrl_dpi_scale(host), pad = 8.0f * s;
            float visible_h = c->h - pad * 2.0f;
            float cx = 0.0f, cy = 0.0f, ch = 16.0f;
            float dy = (vk == VK_PRIOR) ? -visible_h : visible_h;
            sui__textview_caret_metrics(layout, c->d.textview.cursor, &cx, &cy, &ch);
            new_pos = sui__textview_pos_at_y(host, c, layout, cx, cy + dy);
            c->d.textview.scroll_offset += dy;
            layout->lpVtbl->Release(layout);
        }
        if (extend_sel) c->d.textview.sel_end = new_pos;
        else            sui__textview_clear_sel(c);
        c->d.textview.cursor = new_pos;
        break;
    }
    case VK_HOME:
        if (ctrl_down) {
            new_pos = 0;
        } else {
            /* Visual-line start: HitTestPoint at x=0, current y. */
            IDWriteTextLayout* layout = sui__textview_make_layout(host, c);
            if (layout) {
                float cx = 0.0f, cy = 0.0f, ch = 16.0f;
                sui__textview_caret_metrics(layout, c->d.textview.cursor, &cx, &cy, &ch);
                new_pos = sui__textview_pos_at_y(host, c, layout, 0.0f, cy + ch * 0.5f);
                layout->lpVtbl->Release(layout);
            }
        }
        if (extend_sel) c->d.textview.sel_end = new_pos;
        else            sui__textview_clear_sel(c);
        c->d.textview.cursor = new_pos;
        break;
    case VK_END:
        if (ctrl_down) {
            new_pos = len;
        } else {
            IDWriteTextLayout* layout = sui__textview_make_layout(host, c);
            if (layout) {
                float cx = 0.0f, cy = 0.0f, ch = 16.0f;
                sui__textview_caret_metrics(layout, c->d.textview.cursor, &cx, &cy, &ch);
                new_pos = sui__textview_pos_at_y(host, c, layout, 1.0e6f, cy + ch * 0.5f);
                layout->lpVtbl->Release(layout);
            }
        }
        if (extend_sel) c->d.textview.sel_end = new_pos;
        else            sui__textview_clear_sel(c);
        c->d.textview.cursor = new_pos;
        break;
    case VK_BACK:
        if (c->d.textview.readonly) { handled = 1; break; }
        sui__textview_save_undo(c);
        if (sui_ctrl_textview_has_sel(c)) sui__textview_delete_sel(c);
        else if (ctrl_down && c->d.textview.cursor > 0) {
            int wp = sui__textview_word_left(c->d.textview.buf, len, c->d.textview.cursor);
            sui__textview_delete_range(c, wp, c->d.textview.cursor);
        } else if (c->d.textview.cursor > 0) {
            int prev = sui__codepoint_left(c->d.textview.buf, c->d.textview.cursor);
            sui__textview_delete_range(c, prev, c->d.textview.cursor);
        }
        break;
    case VK_DELETE:
        if (c->d.textview.readonly) { handled = 1; break; }
        sui__textview_save_undo(c);
        if (sui_ctrl_textview_has_sel(c)) sui__textview_delete_sel(c);
        else if (ctrl_down && c->d.textview.cursor < len) {
            int wp = sui__textview_word_right(c->d.textview.buf, len, c->d.textview.cursor);
            sui__textview_delete_range(c, c->d.textview.cursor, wp);
        } else if (c->d.textview.cursor < len) {
            int next = sui__codepoint_right(c->d.textview.buf, c->d.textview.cursor, len);
            sui__textview_delete_range(c, c->d.textview.cursor, next);
        }
        break;
    case VK_RETURN:
        if (c->d.textview.readonly) { handled = 0; break; }
        sui__textview_save_undo(c);
        if (sui_ctrl_textview_has_sel(c)) sui__textview_delete_sel(c);
        { wchar_t nl = L'\n'; sui__textview_insert(c, &nl, 1); }
        break;
    default:
        if (ctrl_down) {
            switch (vk) {
            case 'A':
                if (len > 0) {
                    c->d.textview.sel_start = 0;
                    c->d.textview.sel_end   = len;
                    c->d.textview.cursor    = len;
                }
                break;
            case 'C': sui_ctrl_textview_copy(host, c); break;
            case 'X': sui_ctrl_textview_cut (host, c); break;
            case 'V': sui_ctrl_textview_paste(host, c); break;
            case 'Z':
                if (!c->d.textview.readonly) sui__textview_undo(c);
                break;
            case 'Y':
                if (!c->d.textview.readonly) sui__textview_redo(c);
                break;
            default: handled = 0; break;
            }
        } else handled = 0;
        break;
    }

    if (handled) {
        len = c->d.textview.len;
        if (c->d.textview.cursor < 0)   c->d.textview.cursor = 0;
        if (c->d.textview.cursor > len) c->d.textview.cursor = len;
        if (extend_sel && c->d.textview.sel_start == c->d.textview.sel_end)
            sui__textview_clear_sel(c);
        sui__textview_update_content_height(host, c);
        sui__textview_scroll_to_caret(host, c);
        host->caret_visible = 1;
        InvalidateRect(host->hwnd, NULL, FALSE);
    }
    (void)saved_anchor;
    return handled;
}

static int sui_ctrl_textview_on_char(SuiControlHost* host, int idx, wchar_t ch) {
    SuiControl* c = &host->controls[idx];
    if (c->d.textview.readonly) return 0;
    if (ch == L'\r') ch = L'\n';
    /* Reject all control chars except newline. Tab is reserved for focus
     * navigation; a Tab keystroke that lands here arrived via WM_CHAR after
     * VK_TAB moved focus to (or within) this textview, and would otherwise
     * replace any pre-existing selection with a literal '\t'. */
    if (ch < 32 && ch != L'\n') return 0;
    if (ch == 127) return 0;
    sui__textview_save_undo(c);
    if (sui_ctrl_textview_has_sel(c)) sui__textview_delete_sel(c);
    sui__textview_insert(c, &ch, 1);
    sui__textview_update_content_height(host, c);
    sui__textview_scroll_to_caret(host, c);
    host->caret_visible = 1;
    InvalidateRect(host->hwnd, NULL, FALSE);
    return 1;
}

/* ── Cleanup hook: called from clear_page (for discarded slots) and
 *     host_release (for every slot). ────────────────────────────────────────── */

static void sui_ctrl_textview_release(SuiControl* c) {
    if (c->type != SUI_CTRL_TEXTVIEW) return;
    if (c->d.textview.buf)      { sui_free(c->d.textview.buf);      c->d.textview.buf      = NULL; }
    if (c->d.textview.undo_buf) { sui_free(c->d.textview.undo_buf); c->d.textview.undo_buf = NULL; }
    if (c->d.textview.redo_buf) { sui_free(c->d.textview.redo_buf); c->d.textview.redo_buf = NULL; }
    c->d.textview.capacity = 0;
    c->d.textview.len      = 0;
}

/* sui_ctrl_edit.h -- Text edit control: constructor, draw, keyboard, mouse.
 * Included from slim_ui.h under #ifdef SUI_IMPLEMENTATION.
 */

/* ---- Internal wchar_t memmove (avoids CRT dependency) ------------------- */

/* Move n wchar_t elements from src to dst. Handles overlap correctly. */
static void sui__wmove(wchar_t* dst, const wchar_t* src, int n) {
    int i;
    if (dst < src) {
        for (i = 0; i < n; i++) dst[i] = src[i];
    } else if (dst > src) {
        for (i = n - 1; i >= 0; i--) dst[i] = src[i];
    }
}

/* ---- Constructor --------------------------------------------------------- */

static int sui_ctrl_add_edit(SuiControlHost* host, int id, float x, float y, float w, float h,
                             const wchar_t* text, const wchar_t* placeholder, int numeric_only) {
    int idx = sui_ctrl_add(host, SUI_CTRL_EDIT, id, x, y, w, h, text);
    if (idx >= 0) {
        SuiControl* c = &host->controls[idx];
        /* sui_ctrl_add stores at most SUI_CTRL_TEXT_MAX - 1 characters. Base
         * cursor state on the stored text, not the potentially longer input. */
        int len = (int)wcslen(c->text);
        c->d.edit.cursor          = len;
        c->d.edit.sel_start       = -1;
        c->d.edit.sel_end         = -1;
        c->d.edit.drag_sel_anchor = len;
        c->d.edit.scroll_x        = 0.0f;
        c->d.edit.max_len   = 0;
        c->d.edit.numeric_only = numeric_only;
        if (placeholder) wcsncpy(c->d.edit.placeholder, placeholder, 63);
        c->d.edit.placeholder[63] = L'\0';
        /* Alloc after sui_ctrl_add (unlike nav/combo): edit degrades gracefully
         * without undo_pool — Ctrl+Z/Y simply no-op. */
        c->d.edit.undo_pool = (wchar_t (*)[SUI_CTRL_TEXT_MAX])sui_malloc(
            2 * SUI_EDIT_UNDO_DEPTH * sizeof(*c->d.edit.undo_pool));
        c->d.edit.undo_top  = 0;
        c->d.edit.redo_top  = 0;
        if (c->d.edit.undo_pool)
            memset(c->d.edit.undo_pool, 0,
                   2 * SUI_EDIT_UNDO_DEPTH * sizeof(*c->d.edit.undo_pool));
        memset(c->d.edit.undo_cur, 0, sizeof(c->d.edit.undo_cur));
    }
    return idx;
}

/* ---- Text measurement helper --------------------------------------------- */

/* Returns the DWrite pixel width of str[0..pos). Returns 0 if pos <= 0. */
static float sui_ctrl_edit_text_width(SuiRenderer* r, SuiControl* ctrl, const wchar_t* str, int pos) {
    IDWriteFactory* dw; IDWriteTextLayout* layout;
    IDWriteTextFormat* fmt;
    DWRITE_TEXT_METRICS metrics;
    float w = 0.0f;
    if (pos <= 0 || !str || !r->dwrite_factory || !r->font_normal) return 0.0f;
    { int len = (int)wcslen(str); if (pos > len) pos = len; }
    dw = (IDWriteFactory*)r->dwrite_factory;
    fmt = sui__resolve_text_fmt_ex(r, (ctrl && (ctrl->state & SUI_CTRL_BOLD)) ? 1 : 0,
        ctrl ? sui_ctrl_style_font_size(ctrl, r->font_size) : r->font_size,
        ctrl ? sui_ctrl_style_font_family(ctrl) : NULL, NULL);
    if (!fmt) return 0.0f;
    if (FAILED(sui__text_layout(dw, str, (UINT32)pos,
            fmt, 10000.0f, 1000.0f, &layout)))
        return 0.0f;
    memset(&metrics, 0, sizeof(metrics));
    if (SUCCEEDED(layout->lpVtbl->GetMetrics(layout, &metrics)))
        w = metrics.widthIncludingTrailingWhitespace;
    layout->lpVtbl->Release(layout);
    return w;
}

/* ---- Word boundary helpers ----------------------------------------------- */

/* Returns 1 if `ch` is a word separator for Ctrl+Arrow movement.
 * Covers space + common ASCII punctuation found in paths and identifiers.
 * No CRT dependency — pure table lookup over ASCII range. */
static int sui__is_word_sep(wchar_t ch) {
    /* Fast path: space is the most common separator */
    if (ch == L' ') return 1;
    /* ASCII punctuation that acts as a word boundary */
    if (ch >= 0x21 && ch <= 0x7E) {
        /* exclude alphanumerics and underscore (part of identifiers) */
        if ((ch >= L'A' && ch <= L'Z') ||
            (ch >= L'a' && ch <= L'z') ||
            (ch >= L'0' && ch <= L'9') ||
             ch == L'_') return 0;
        return 1;
    }
    return 0;
}

/* Move left past separators, then past non-separators. */
static int sui_ctrl_edit_word_left(const wchar_t* text, int pos) {
    if (pos <= 0) return 0;
    while (pos > 0 && sui__is_word_sep(text[pos - 1])) pos--;
    while (pos > 0 && !sui__is_word_sep(text[pos - 1])) pos--;
    return pos;
}

/* Move right past non-separators, then past separators. */
static int sui_ctrl_edit_word_right(const wchar_t* text, int pos) {
    int len = (int)wcslen(text);
    while (pos < len && !sui__is_word_sep(text[pos])) pos++;
    while (pos < len &&  sui__is_word_sep(text[pos])) pos++;
    return pos;
}

/* Expand left to start of token (non-separator run) containing pos. */
static int sui_ctrl_edit_word_start(const wchar_t* text, int pos) {
    while (pos > 0 && !sui__is_word_sep(text[pos - 1])) pos--;
    return pos;
}

/* Expand right to end of token (non-separator run) containing pos. */
static int sui_ctrl_edit_word_end(const wchar_t* text, int pos) {
    int len = (int)wcslen(text);
    while (pos < len && !sui__is_word_sep(text[pos])) pos++;
    return pos;
}

/* ---- Selection helpers ---------------------------------------------------- */

/* Controls are intentionally plain structs and consumers sometimes replace
 * text directly. Clamp all dependent indices before pointer arithmetic so a
 * shorter replacement cannot leave a stale cursor/selection out of bounds. */
static void sui_ctrl_edit_normalize_indices(SuiControl* c) {
    int len;
    if (!c) return;
    len = (int)wcslen(c->text);
    if (c->d.edit.cursor < 0) c->d.edit.cursor = 0;
    if (c->d.edit.cursor > len) c->d.edit.cursor = len;
    if (c->d.edit.drag_sel_anchor < 0) c->d.edit.drag_sel_anchor = 0;
    if (c->d.edit.drag_sel_anchor > len) c->d.edit.drag_sel_anchor = len;
    if (c->d.edit.sel_start < 0 || c->d.edit.sel_end < 0) {
        c->d.edit.sel_start = -1;
        c->d.edit.sel_end = -1;
    } else {
        if (c->d.edit.sel_start > len) c->d.edit.sel_start = len;
        if (c->d.edit.sel_end > len) c->d.edit.sel_end = len;
    }
}

static int sui_ctrl_edit_has_sel(SuiControl* c) {
    return c->d.edit.sel_start >= 0 && c->d.edit.sel_start != c->d.edit.sel_end;
}

/* Delete the selected range, place cursor at the lower bound, clear selection. */
static void sui_ctrl_edit_delete_sel(SuiControl* c) {
    int a, b, len;
    sui_ctrl_edit_normalize_indices(c);
    if (!sui_ctrl_edit_has_sel(c)) return;
    a = c->d.edit.sel_start < c->d.edit.sel_end ? c->d.edit.sel_start : c->d.edit.sel_end;
    b = c->d.edit.sel_start < c->d.edit.sel_end ? c->d.edit.sel_end   : c->d.edit.sel_start;
    len = (int)wcslen(c->text);
    /* shift text left */
    sui__wmove(c->text + a, c->text + b, len - b + 1);
    c->d.edit.cursor    = a;
    c->d.edit.sel_start = -1;
    c->d.edit.sel_end   = -1;
}

/* Push current text+cursor onto the undo stack and clear the redo stack.
 * If undo is full (SUI_EDIT_UNDO_DEPTH entries), the oldest entry is dropped. */
static void sui_ctrl_edit_save_undo(SuiControl* c) {
    int D = SUI_EDIT_UNDO_DEPTH;
    int i;
    sui_ctrl_edit_normalize_indices(c);
    if (!c->d.edit.undo_pool) return;
    if (c->d.edit.undo_top == D) {
        for (i = 0; i < D - 1; i++) {
            sui__wmove(c->d.edit.undo_pool[i], c->d.edit.undo_pool[i + 1], SUI_CTRL_TEXT_MAX);
            c->d.edit.undo_cur[i] = c->d.edit.undo_cur[i + 1];
        }
        c->d.edit.undo_top = D - 1;
    }
    wcsncpy(c->d.edit.undo_pool[c->d.edit.undo_top], c->text, SUI_CTRL_TEXT_MAX - 1);
    c->d.edit.undo_pool[c->d.edit.undo_top][SUI_CTRL_TEXT_MAX - 1] = L'\0';
    c->d.edit.undo_cur[c->d.edit.undo_top] = c->d.edit.cursor;
    c->d.edit.undo_top++;
    c->d.edit.redo_top = 0;
}

/* Pop the most recent undo entry; push current state to the redo stack. */
static void sui_ctrl_edit_undo(SuiControl* c) {
    int D = SUI_EDIT_UNDO_DEPTH;
    int ri;
    if (!c->d.edit.undo_pool || c->d.edit.undo_top == 0) return;
    if (c->d.edit.redo_top < D) {
        ri = D + c->d.edit.redo_top;
        wcsncpy(c->d.edit.undo_pool[ri], c->text, SUI_CTRL_TEXT_MAX - 1);
        c->d.edit.undo_pool[ri][SUI_CTRL_TEXT_MAX - 1] = L'\0';
        c->d.edit.undo_cur[ri] = c->d.edit.cursor;
        c->d.edit.redo_top++;
    }
    c->d.edit.undo_top--;
    wcsncpy(c->text, c->d.edit.undo_pool[c->d.edit.undo_top], SUI_CTRL_TEXT_MAX - 1);
    c->text[SUI_CTRL_TEXT_MAX - 1] = L'\0';
    c->d.edit.cursor    = c->d.edit.undo_cur[c->d.edit.undo_top];
    c->d.edit.sel_start = -1;
    c->d.edit.sel_end   = -1;
    sui_ctrl_edit_normalize_indices(c);
}

/* Pop the most recent redo entry; push current state back onto the undo stack. */
static void sui_ctrl_edit_redo(SuiControl* c) {
    int D = SUI_EDIT_UNDO_DEPTH;
    if (!c->d.edit.undo_pool || c->d.edit.redo_top == 0) return;
    if (c->d.edit.undo_top < D) {
        wcsncpy(c->d.edit.undo_pool[c->d.edit.undo_top], c->text, SUI_CTRL_TEXT_MAX - 1);
        c->d.edit.undo_pool[c->d.edit.undo_top][SUI_CTRL_TEXT_MAX - 1] = L'\0';
        c->d.edit.undo_cur[c->d.edit.undo_top] = c->d.edit.cursor;
        c->d.edit.undo_top++;
    }
    c->d.edit.redo_top--;
    wcsncpy(c->text, c->d.edit.undo_pool[D + c->d.edit.redo_top], SUI_CTRL_TEXT_MAX - 1);
    c->text[SUI_CTRL_TEXT_MAX - 1] = L'\0';
    c->d.edit.cursor    = c->d.edit.undo_cur[D + c->d.edit.redo_top];
    c->d.edit.sel_start = -1;
    c->d.edit.sel_end   = -1;
    sui_ctrl_edit_normalize_indices(c);
}

/* Free the undo pool allocated in sui_ctrl_add_edit. */
static void sui_ctrl_edit_release(SuiControl* c) {
    if (c->type != SUI_CTRL_EDIT) return;
    if (c->d.edit.undo_pool) { sui_free(c->d.edit.undo_pool); c->d.edit.undo_pool = NULL; }
}

/* ---- Clipboard helpers ---------------------------------------------------- */

static void sui_ctrl_edit_copy(SuiControlHost* host, SuiControl* c) {
    int a, b, n;
    sui_ctrl_edit_normalize_indices(c);
    if (!sui_ctrl_edit_has_sel(c)) return;
    a = c->d.edit.sel_start < c->d.edit.sel_end ? c->d.edit.sel_start : c->d.edit.sel_end;
    b = c->d.edit.sel_start < c->d.edit.sel_end ? c->d.edit.sel_end   : c->d.edit.sel_start;
    n = b - a;
    if (n <= 0) return;
    if (OpenClipboard(host->hwnd)) {
        HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, (size_t)(n + 1) * sizeof(wchar_t));
        if (hg) {
            wchar_t* p = (wchar_t*)GlobalLock(hg);
            if (p) {
                wcsncpy(p, c->text + a, (size_t)n);
                p[n] = L'\0';
                GlobalUnlock(hg);
                EmptyClipboard();
                if (!SetClipboardData(CF_UNICODETEXT, hg)) GlobalFree(hg);
            } else {
                GlobalFree(hg);
            }
        }
        CloseClipboard();
    }
}

static void sui_ctrl_edit_cut(SuiControlHost* host, SuiControl* c) {
    if (!sui_ctrl_edit_has_sel(c)) return;
    sui_ctrl_edit_copy(host, c);
    sui_ctrl_edit_save_undo(c);
    sui_ctrl_edit_delete_sel(c);
}

static void sui_ctrl_edit_paste(SuiControlHost* host, SuiControl* c) {
    int cur_len;
    sui_ctrl_edit_normalize_indices(c);
    cur_len = (int)wcslen(c->text);
    if (!OpenClipboard(host->hwnd)) return;
    { HANDLE hd = GetClipboardData(CF_UNICODETEXT);
      if (hd) {
          wchar_t* src = (wchar_t*)GlobalLock(hd);
          if (src) {
              int src_len = (int)wcslen(src);
              int max_ins;
              sui_ctrl_edit_save_undo(c);
              if (sui_ctrl_edit_has_sel(c)) {
                  sui_ctrl_edit_delete_sel(c);
                  cur_len = (int)wcslen(c->text);
              }
              max_ins = (SUI_CTRL_TEXT_MAX - 1) - cur_len;
              if (src_len > max_ins) src_len = max_ins;
              if (src_len > 0) {
                  int cur = c->d.edit.cursor;
                  sui__wmove(c->text + cur + src_len, c->text + cur, cur_len - cur + 1);
                  wcsncpy(c->text + cur, src, (size_t)src_len);
                  c->d.edit.cursor += src_len;
              }
              GlobalUnlock(hd);
          }
      }
    }
    CloseClipboard();
}

/* ---- Scroll helper -------------------------------------------------------- */

static float sui_ctrl_edit_text_pad(SuiControlHost* host) {
    return SUI_EDIT_TEXT_PAD * sui_ctrl_dpi_scale(host);
}

/* Adjust scroll_x so that the caret (at cursor position) is visible inside the
 * clipped inner area of width (ctrl->w - 2*pad). */
static void sui_ctrl_edit_scroll_to_caret(SuiControlHost* host, SuiControl* c) {
    float pad   = sui_ctrl_edit_text_pad(host);
    float inner = c->w - pad * 2.0f;
    float cx;
    sui_ctrl_edit_normalize_indices(c);
    cx = sui_ctrl_edit_text_width(host->renderer, c, c->text, c->d.edit.cursor);
    /* caret is at cx relative to text origin */
    if (cx - c->d.edit.scroll_x > inner - 2.0f)
        c->d.edit.scroll_x = cx - inner + 4.0f;
    if (cx - c->d.edit.scroll_x < 0.0f)
        c->d.edit.scroll_x = cx;
    if (c->d.edit.scroll_x < 0.0f) c->d.edit.scroll_x = 0.0f;
}

/* ---- Draw function -------------------------------------------------------- */

static void sui_ctrl_draw_edit_with_radius(SuiControlHost* host, SuiControl* ctrl,
                                           float rad) {
    SuiRenderer* r = host->renderer;
    const SuiColorScheme* sc = host->scheme;
    float s   = sui_ctrl_dpi_scale(host);
    float pad = sui_ctrl_edit_text_pad(host);
    int focused = (ctrl->state & SUI_CTRL_FOCUSED) != 0;
    int has_text;
    ID2D1RenderTarget* rt;
    D2D1_RECT_F clip;
    uint32_t bg_color = sui_ctrl_style_color(ctrl, SUI_STYLE_BG_COLOR, sc->input_bg);
    uint32_t border_base = sui_ctrl_style_color(ctrl, SUI_STYLE_BORDER_COLOR, sc->input_border);
    uint32_t accent = sui_ctrl_style_color(ctrl, SUI_STYLE_ACCENT_COLOR, sc->input_focus_border);
    uint32_t hover_border = sui_ctrl_style_color(ctrl, SUI_STYLE_HOVER_COLOR, sc->btn_default_border);
    uint32_t text_color = sui_ctrl_style_color(ctrl, SUI_STYLE_TEXT_COLOR, sc->input_text);
    uint32_t placeholder_color = sui_ctrl_style_color(ctrl, SUI_STYLE_PLACEHOLDER_COLOR, sc->input_placeholder);
    uint32_t selection_color = sui_ctrl_style_color(ctrl, SUI_STYLE_SELECTION_COLOR, sc->input_selection);
    float font_size = sui_ctrl_style_font_size(ctrl, r->font_size);
    const wchar_t* font_family = sui_ctrl_style_font_family(ctrl);
    int bold = (ctrl->state & SUI_CTRL_BOLD) ? 1 : 0;

    if (ctrl->state & SUI_CTRL_HIDDEN) return;
    sui_ctrl_edit_normalize_indices(ctrl);

    /* Background */
    sui_renderer_fill_rounded_rect(r, ctrl->x, ctrl->y, ctrl->w, ctrl->h, rad, bg_color);

    /* Border — blue when focused, eased brighter on hover, default otherwise */
    sui_renderer_draw_rounded_rect(r, ctrl->x, ctrl->y, ctrl->w, ctrl->h, rad,
        focused ? accent
                : sui_color_lerp(border_base, hover_border, ctrl->hover_t),
        focused ? 2.0f : 1.0f);

    rt = (ID2D1RenderTarget*)r->render_target;
    if (!rt) return;

    /* Clip to inner text area */
    clip.left   = ctrl->x + pad;
    clip.top    = ctrl->y;
    clip.right  = ctrl->x + ctrl->w - pad;
    clip.bottom = ctrl->y + ctrl->h;
    rt->lpVtbl->PushAxisAlignedClip(rt, &clip, 0);

    has_text = (ctrl->text[0] != L'\0');

    if (!has_text && !focused) {
        /* Placeholder */
        if (ctrl->d.edit.placeholder[0])
            sui_renderer_draw_text_middle_ex(r, ctrl->d.edit.placeholder,
                ctrl->x + pad, ctrl->y,
                ctrl->w - pad * 2.0f, ctrl->h,
                placeholder_color, bold, font_size, font_family);
    } else if (has_text || focused) {
        float text_x = ctrl->x + pad - ctrl->d.edit.scroll_x;
        int len = (int)wcslen(ctrl->text);

        /* Selection highlight — only when focused */
        if (focused && sui_ctrl_edit_has_sel(ctrl)) {
            int a = ctrl->d.edit.sel_start < ctrl->d.edit.sel_end
                    ? ctrl->d.edit.sel_start : ctrl->d.edit.sel_end;
            int b = ctrl->d.edit.sel_start < ctrl->d.edit.sel_end
                    ? ctrl->d.edit.sel_end   : ctrl->d.edit.sel_start;
            float sx = text_x + sui_ctrl_edit_text_width(r, ctrl, ctrl->text, a);
            float ex = text_x + sui_ctrl_edit_text_width(r, ctrl, ctrl->text, b);
            float sel_top    = ctrl->y + 2.0f * s;
            float sel_bottom = ctrl->y + ctrl->h - 2.0f * s;
            /* Clamp to clip rect */
            if (sx < clip.left)  sx = clip.left;
            if (ex > clip.right) ex = clip.right;
            if (ex > sx)
                sui_renderer_fill_rect(r, sx, sel_top, ex - sx, sel_bottom - sel_top,
                    selection_color);
        }

        /* Text */
        if (has_text)
            sui_renderer_draw_text_middle_ex(r, ctrl->text,
                text_x, ctrl->y,
                (float)(len + 1) * 20.0f,  /* wide enough — clipped by PushAxisAlignedClip */
                ctrl->h,
                text_color, bold, font_size, font_family);

        /* Caret */
        if (focused && host->caret_visible && host->window_has_focus) {
            float cx = text_x + sui_ctrl_edit_text_width(r, ctrl, ctrl->text, ctrl->d.edit.cursor);
            float caret_w = 1.5f * s;
            float caret_top    = ctrl->y + 3.0f * s;
            float caret_bottom = ctrl->y + ctrl->h - 3.0f * s;
            sui_renderer_fill_rect(r, cx, caret_top, caret_w, caret_bottom - caret_top,
                accent);
        }
    }

    rt->lpVtbl->PopAxisAlignedClip(rt);

    sui_ctrl_draw_kb_focus_ring(host, ctrl, rad);
}

static void sui_ctrl_draw_edit(SuiControlHost* host, SuiControl* ctrl) {
    float s = sui_ctrl_dpi_scale(host);
    sui_ctrl_draw_edit_with_radius(host, ctrl, 4.0f * s);
}

/* ---- Keyboard handler ---------------------------------------------------- */

static int sui_ctrl_edit_on_keydown(SuiControlHost* host, int idx, WPARAM vk) {
    SuiControl* c = &host->controls[idx];
    int ctrl_down  = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    int shift_down = (GetKeyState(VK_SHIFT)   & 0x8000) != 0;
    int len;
    int handled = 1;

    sui_ctrl_edit_normalize_indices(c);
    len = (int)wcslen(c->text);

    switch (vk) {
    case VK_LEFT: {
        int new_pos = c->d.edit.cursor;
        if (ctrl_down)
            new_pos = sui_ctrl_edit_word_left(c->text, new_pos);
        else
            new_pos = sui__codepoint_left(c->text, new_pos);
        if (shift_down) {
            if (c->d.edit.sel_start < 0) c->d.edit.sel_start = c->d.edit.cursor;
            c->d.edit.sel_end = new_pos;
        } else {
            /* If selection exists, jump to the lower bound */
            if (!shift_down && sui_ctrl_edit_has_sel(c)) {
                new_pos = c->d.edit.sel_start < c->d.edit.sel_end
                          ? c->d.edit.sel_start : c->d.edit.sel_end;
            }
            c->d.edit.sel_start = -1;
            c->d.edit.sel_end   = -1;
        }
        c->d.edit.cursor = new_pos;
        break;
    }
    case VK_RIGHT: {
        int new_pos = c->d.edit.cursor;
        if (ctrl_down)
            new_pos = sui_ctrl_edit_word_right(c->text, new_pos);
        else
            new_pos = sui__codepoint_right(c->text, new_pos, len);
        if (shift_down) {
            if (c->d.edit.sel_start < 0) c->d.edit.sel_start = c->d.edit.cursor;
            c->d.edit.sel_end = new_pos;
        } else {
            if (!shift_down && sui_ctrl_edit_has_sel(c)) {
                new_pos = c->d.edit.sel_start < c->d.edit.sel_end
                          ? c->d.edit.sel_end : c->d.edit.sel_start;
            }
            c->d.edit.sel_start = -1;
            c->d.edit.sel_end   = -1;
        }
        c->d.edit.cursor = new_pos;
        break;
    }
    case VK_HOME: {
        int new_pos = 0;
        if (shift_down) {
            if (c->d.edit.sel_start < 0) c->d.edit.sel_start = c->d.edit.cursor;
            c->d.edit.sel_end = new_pos;
        } else {
            c->d.edit.sel_start = -1;
            c->d.edit.sel_end   = -1;
        }
        c->d.edit.cursor = new_pos;
        break;
    }
    case VK_END: {
        int new_pos = len;
        if (shift_down) {
            if (c->d.edit.sel_start < 0) c->d.edit.sel_start = c->d.edit.cursor;
            c->d.edit.sel_end = new_pos;
        } else {
            c->d.edit.sel_start = -1;
            c->d.edit.sel_end   = -1;
        }
        c->d.edit.cursor = new_pos;
        break;
    }
    case VK_BACK:
        sui_ctrl_edit_save_undo(c);
        if (sui_ctrl_edit_has_sel(c)) {
            sui_ctrl_edit_delete_sel(c);
        } else if (ctrl_down && c->d.edit.cursor > 0) {
            int new_pos = sui_ctrl_edit_word_left(c->text, c->d.edit.cursor);
            sui__wmove(c->text + new_pos, c->text + c->d.edit.cursor, len - c->d.edit.cursor + 1);
            c->d.edit.cursor = new_pos;
        } else if (c->d.edit.cursor > 0) {
            int cur = c->d.edit.cursor;
            int prev = sui__codepoint_left(c->text, cur);
            sui__wmove(c->text + prev, c->text + cur, len - cur + 1);
            c->d.edit.cursor = prev;
        }
        break;
    case VK_DELETE:
        sui_ctrl_edit_save_undo(c);
        if (sui_ctrl_edit_has_sel(c)) {
            sui_ctrl_edit_delete_sel(c);
        } else if (c->d.edit.cursor < len) {
            int cur = c->d.edit.cursor;
            int next = sui__codepoint_right(c->text, cur, len);
            sui__wmove(c->text + cur, c->text + next, len - next + 1);
        }
        break;
    default:
        if (ctrl_down) {
            switch (vk) {
            case 'A':
                c->d.edit.sel_start = 0;
                c->d.edit.sel_end   = len;
                c->d.edit.cursor    = len;
                break;
            case 'C':
                sui_ctrl_edit_copy(host, c);
                break;
            case 'X':
                sui_ctrl_edit_cut(host, c);
                break;
            case 'V':
                sui_ctrl_edit_paste(host, c);
                break;
            case 'Z':
                sui_ctrl_edit_undo(c);
                break;
            case 'Y':
                sui_ctrl_edit_redo(c);
                break;
            default:
                handled = 0;
                break;
            }
        } else {
            handled = 0;
        }
        break;
    }

    if (handled) {
        /* Clamp cursor */
        len = (int)wcslen(c->text);
        if (c->d.edit.cursor < 0)   c->d.edit.cursor = 0;
        if (c->d.edit.cursor > len) c->d.edit.cursor = len;
        sui_ctrl_edit_scroll_to_caret(host, c);
        host->caret_visible = 1;
        InvalidateRect(host->hwnd, NULL, FALSE);
    }
    return handled;
}

/* ---- Character input handler --------------------------------------------- */

static int sui_ctrl_edit_on_char(SuiControlHost* host, int idx, wchar_t ch) {
    SuiControl* c = &host->controls[idx];
    int len;

    if (ch < 32 || ch == 127) return 0;  /* control characters */
    sui_ctrl_edit_normalize_indices(c);

    /* Numeric-only filter: digits + at most one decimal point */
    if (c->d.edit.numeric_only) {
        if (!(ch >= L'0' && ch <= L'9') && ch != L'.') return 0;
        if (ch == L'.') {
            const wchar_t* t;
            for (t = c->text; *t; t++) if (*t == L'.') return 0;
        }
    }

    len = (int)wcslen(c->text);
    if (len >= SUI_CTRL_TEXT_MAX - 1) return 0;

    /* Max length guard (if set) */
    if (c->d.edit.max_len > 0 && len >= c->d.edit.max_len) return 0;

    sui_ctrl_edit_save_undo(c);

    if (sui_ctrl_edit_has_sel(c)) {
        sui_ctrl_edit_delete_sel(c);
        len = (int)wcslen(c->text);
    }

    /* Insert character at cursor */
    { int cur = c->d.edit.cursor;
      sui__wmove(c->text + cur + 1, c->text + cur, len - cur + 1);
      c->text[cur] = ch;
      c->d.edit.cursor++;
    }

    sui_ctrl_edit_scroll_to_caret(host, c);
    host->caret_visible = 1;
    InvalidateRect(host->hwnd, NULL, FALSE);
    return 1;
}

/* ---- Left-button-down handler -------------------------------------------- */

static int sui_ctrl_edit_on_lbutton_down(SuiControlHost* host, int idx, int mx, int my) {
    SuiControl* c = &host->controls[idx];
    float pad      = sui_ctrl_edit_text_pad(host);
    float click_x  = (float)mx - c->x - pad + c->d.edit.scroll_x;
    int   len      = (int)wcslen(c->text);
    int   best     = 0;
    float best_dist;
    int   i;
    int   shift_down = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    (void)my;

    sui_ctrl_edit_normalize_indices(c);

    best_dist = click_x < 0.0f ? -click_x : click_x;
    for (i = 1; i <= len; i++) {
            float edge = sui_ctrl_edit_text_width(host->renderer, c, c->text, i);
        float dist = edge - click_x;
        if (dist < 0.0f) dist = -dist;
        if (dist < best_dist) { best_dist = dist; best = i; }
    }

    if (shift_down) {
        /* Extend selection from anchor to clicked position */
        c->d.edit.sel_start = c->d.edit.drag_sel_anchor;
        c->d.edit.sel_end   = best;
    } else {
        c->d.edit.drag_sel_anchor = best;
        c->d.edit.sel_start       = -1;
        c->d.edit.sel_end         = -1;
    }
    c->d.edit.cursor    = best;
    host->caret_visible = 1;
    return 1;
}

/* ---- Double-click handler: select word ----------------------------------- */

static int sui_ctrl_edit_on_lbutton_dblclk(SuiControlHost* host, int idx, int mx, int my) {
    SuiControl* c = &host->controls[idx];
    float pad     = sui_ctrl_edit_text_pad(host);
    float click_x = (float)mx - c->x - pad + c->d.edit.scroll_x;
    int   len     = (int)wcslen(c->text);
    int   best    = 0;
    float best_d  = click_x < 0.0f ? -click_x : click_x;
    int   i;
    (void)my;

    sui_ctrl_edit_normalize_indices(c);

    for (i = 1; i <= len; i++) {
            float edge = sui_ctrl_edit_text_width(host->renderer, c, c->text, i);
        float dist = edge - click_x;
        if (dist < 0.0f) dist = -dist;
        if (dist < best_d) { best_d = dist; best = i; }
    }

    /* Expand to word boundaries if we clicked on a non-separator char */
    { int char_idx = (best < len) ? best : (len > 0 ? len - 1 : 0);
      if (len > 0 && !sui__is_word_sep(c->text[char_idx])) {
          int a = sui_ctrl_edit_word_start(c->text, best);
          int b = sui_ctrl_edit_word_end(c->text, best);
          c->d.edit.sel_start       = a;
          c->d.edit.sel_end         = b;
          c->d.edit.cursor          = b;
          c->d.edit.drag_sel_anchor = a;
      }
    }

    host->caret_visible = 1;
    return 1;
}

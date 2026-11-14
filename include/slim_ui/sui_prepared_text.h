/* Internal text resources for an explicitly prepared renderer. No eviction or
 * allocating fallback during painting; callers reopen preparation on changes. */
#define SUI_PREPARED_FORMATS 16
#define SUI_PREPARED_LAYOUTS 128
#define SUI_PREPARED_TEXT_LENGTH 2048
enum {
    SUI_TEXT_CENTER = 1, SUI_TEXT_MIDDLE = 2, SUI_TEXT_NO_WRAP = 4,
    SUI_TEXT_ELLIPSIS = 8, SUI_TEXT_NATURAL = 16
};
static int sui__prepared_text_equal(const wchar_t* a, const wchar_t* b, UINT32 length) {
    UINT32 i;
    for (i = 0; i < length; ++i) if (a[i] != b[i]) return 0;
    return 1;
}
#ifdef SUI_FEATURE_CUSTOM_CONTROLS
typedef struct SuiPreparedLayout {
    IDWriteTextLayout* layout;
    IDWriteTextFormat* format;
    wchar_t* text;
    UINT32 length, hash;
    float width, height;
    unsigned style;
    int used;
} SuiPreparedLayout;
typedef struct SuiPreparedText {
    SuiTextFormatCache formats[SUI_PREPARED_FORMATS];
    SuiPreparedLayout layouts[SUI_PREPARED_LAYOUTS];
    unsigned format_count, layout_count, formats_created, layouts_created;
    unsigned rejected;
    int preparing, disabled;
} SuiPreparedText;

static void sui__prepared_layouts_clear(SuiRenderer* r) {
    SuiPreparedText* p = r->prepared_text;
    unsigned i;
    if (!p) return;
    for (i = 0; i < p->layout_count; ++i) {
        p->layouts[i].layout->lpVtbl->Release(p->layouts[i].layout);
        sui_free(p->layouts[i].text);
    }
    p->layout_count = 0;
    p->rejected = 0;
    p->preparing = 1;
}
static void sui__prepared_text_clear(SuiRenderer* r) {
    SuiPreparedText* p = r->prepared_text;
    unsigned i;
    if (!p) return;
    sui__prepared_layouts_clear(r);
    for (i = 0; i < p->format_count; ++i)
        ((IDWriteTextFormat*)p->formats[i].fmt)->lpVtbl->Release((IDWriteTextFormat*)p->formats[i].fmt);
    p->format_count = 0;
}
static int sui__prepared_text_begin(SuiRenderer* r, int reset) {
    if (!r->prepared_text) {
        r->prepared_text = (SuiPreparedText*)sui_malloc(sizeof(SuiPreparedText));
        if (!r->prepared_text) return 0;
        memset(r->prepared_text, 0, sizeof(SuiPreparedText));
    }
    if (reset) sui__prepared_layouts_clear(r);
    else if (!r->prepared_text->preparing) {
        unsigned i;
        for (i = 0; i < r->prepared_text->layout_count; ++i) r->prepared_text->layouts[i].used = 0;
    }
    r->prepared_text->preparing = 1;
    r->prepared_text->disabled = 0;
    r->prepared_text->rejected = 0;
    return 1;
}
static int sui__prepared_text_end(SuiRenderer* r) {
    unsigned i;
    SuiPreparedText* p = r->prepared_text;
    if (!p || p->disabled) return 1;
    if (p->preparing) for (i = 0; i < p->layout_count;) {
        if (p->layouts[i].used) { ++i; continue; }
        p->layouts[i].layout->lpVtbl->Release(p->layouts[i].layout);
        sui_free(p->layouts[i].text);
        p->layouts[i] = p->layouts[--p->layout_count];
    }
    p->preparing = 0;
    return p->rejected == 0;
}
static void sui__prepared_text_disable(SuiRenderer* r) {
    if (!r->prepared_text || r->prepared_text->disabled) return;
    sui__prepared_text_clear(r);
    r->prepared_text->disabled = 1;
    r->prepared_text->preparing = 0;
}
static void sui__prepared_text_release(SuiRenderer* r) {
    if (!r->prepared_text) return;
    sui__prepared_text_clear(r);
    sui_free(r->prepared_text);
    r->prepared_text = NULL;
}
static IDWriteTextFormat* sui__prepared_format(SuiRenderer* r, const wchar_t* family,
        float size, int weight, int italic, int title) {
    SuiPreparedText* p = r->prepared_text;
    IDWriteFactory* dw = (IDWriteFactory*)r->dwrite_factory;
    IDWriteTextFormat* fmt = NULL;
    unsigned i;
    for (i = 0; i < p->format_count; ++i) {
        fmt = (IDWriteTextFormat*)sui__fmt_cache_lookup(&p->formats[i], family, size, weight, italic, title);
        if (fmt) return fmt;
    }
    if (!p->preparing || p->format_count == SUI_PREPARED_FORMATS || wcslen(family) >= 64) {
        ++p->rejected; return NULL;
    }
    if (FAILED(dw->lpVtbl->CreateTextFormat(dw, family, NULL, weight, italic,
            DWRITE_FONT_STRETCH_NORMAL, size, L"", &fmt)) || !fmt) {
        if (!title || FAILED(dw->lpVtbl->CreateTextFormat(dw, r->typography.body.family, NULL,
                weight, italic, DWRITE_FONT_STRETCH_NORMAL, size, L"", &fmt)) || !fmt) {
            ++p->rejected; return NULL;
        }
    }
    sui__fmt_cache_set(&p->formats[p->format_count++], fmt, family, size, weight, italic, title);
    ++p->formats_created;
    return fmt;
}
#endif

/* Returned layouts carry a caller reference, like CreateTextLayout. Style is
 * part of the key: cached layouts are immutable after preparation. */
static HRESULT sui__renderer_text_layout(SuiRenderer* r, const wchar_t* text, UINT32 length,
        IDWriteTextFormat* fmt, float width, float height, unsigned style, IDWriteTextLayout** out) {
    IDWriteFactory* dw = (IDWriteFactory*)r->dwrite_factory;
    IDWriteTextLayout* layout = NULL;
    HRESULT hr;
#ifdef SUI_FEATURE_CUSTOM_CONTROLS
    SuiPreparedText* p = r->prepared_text && !r->prepared_text->disabled ? r->prepared_text : NULL;
    UINT32 hash = 0;
    unsigned i;
    if (p) {
        hash = sui__text_hash(text);
        for (i = 0; i < p->layout_count; ++i) {
            SuiPreparedLayout* e = &p->layouts[i];
            if (e->hash == hash && e->length == length && e->format == fmt &&
                e->width == width && e->height == height && e->style == style &&
                sui__prepared_text_equal(e->text, text, length)) {
                e->used = 1;
                e->layout->lpVtbl->AddRef(e->layout); *out = e->layout; return S_OK;
            }
        }
        if (!p->preparing || p->layout_count == SUI_PREPARED_LAYOUTS || length > SUI_PREPARED_TEXT_LENGTH) {
            ++p->rejected; *out = NULL; return E_OUTOFMEMORY;
        }
    }
#endif
    hr = (style & SUI_TEXT_NATURAL)
        ? dw->lpVtbl->CreateTextLayout(dw, text, length, fmt, width, height, &layout)
        : sui__text_layout(dw, text, length, fmt, width, height, &layout);
    if (FAILED(hr) || !layout) { *out = NULL; return hr; }
    layout->lpVtbl->SetTextAlignment(layout, (style & SUI_TEXT_CENTER) ? DWRITE_TEXT_ALIGNMENT_CENTER : 0);
    layout->lpVtbl->SetParagraphAlignment(layout, (style & SUI_TEXT_MIDDLE) ? DWRITE_PARAGRAPH_ALIGNMENT_CENTER : 0);
    layout->lpVtbl->SetWordWrapping(layout, (style & SUI_TEXT_NO_WRAP) ? DWRITE_WORD_WRAPPING_NO_WRAP : DWRITE_WORD_WRAPPING_WRAP);
    if (style & SUI_TEXT_ELLIPSIS) {
        IDWriteInlineObject* sign = NULL;
        DWRITE_TRIMMING trim = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 };
        if (SUCCEEDED(dw->lpVtbl->CreateEllipsisTrimmingSign(dw, fmt, &sign)) && sign) {
            layout->lpVtbl->SetTrimming(layout, &trim, sign);
            sign->lpVtbl->Release(sign);
        }
    }
#ifdef SUI_FEATURE_CUSTOM_CONTROLS
    if (p) {
        SuiPreparedLayout* e = &p->layouts[p->layout_count];
        wchar_t* copy = (wchar_t*)sui_malloc((length + 1) * sizeof(wchar_t));
        if (!copy) { ++p->rejected; layout->lpVtbl->Release(layout); *out = NULL; return E_OUTOFMEMORY; }
        sui__wstrncpy_z(copy, text, length + 1);
        e->layout = layout; e->format = fmt; e->text = copy; e->length = length; e->hash = hash;
        e->width = width; e->height = height; e->style = style;
        e->used = 1;
        ++p->layout_count; ++p->layouts_created;
        layout->lpVtbl->AddRef(layout);
    }
#endif
    *out = layout; return S_OK;
}

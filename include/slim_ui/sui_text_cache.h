/* sui_text_cache.h — perf caches for slim-ui (Phase 3)
 *
 * Static-functions-only. Included from slim_ui.h inside the
 * SUI_IMPLEMENTATION + SUI_FEATURE_CUSTOM_CONTROLS gate, after sui_d2d.h and
 * before sui_renderer.h.
 *
 * Design: all functions return displaced COM pointers for the caller to
 * Release — this module never calls Release, keeping it testable with fake
 * pointer values.
 */

#ifndef SUI_TEXT_CACHE_H
#define SUI_TEXT_CACHE_H

/* ── Hash ─────────────────────────────────────────────────────────────────── */

/* FNV-1a 32-bit hash over a NUL-terminated wchar_t string. */
static UINT32 sui__text_hash(const wchar_t *text)
{
    UINT32 h = 2166136261u;
    if (!text) return h;
    while (*text) {
        h ^= (UINT32)(unsigned char)( *text        & 0xFF);
        h *= 16777619u;
        h ^= (UINT32)(unsigned char)((*text >> 8) & 0xFF);
        h *= 16777619u;
        ++text;
    }
    return h;
}

/* ── Format cache ─────────────────────────────────────────────────────────── */

/* Returns cached IDWriteTextFormat* on hit, NULL on miss. */
static void *sui__fmt_cache_lookup(const SuiTextFormatCache *c,
                                    const wchar_t *family, float size_dip,
                                    int weight, int italic, int bold_title)
{
    const wchar_t *a, *b;
    if (!c->fmt)                  return NULL;
    if (c->size_dip   != size_dip)   return NULL;
    if (c->weight     != weight)     return NULL;
    if (c->italic     != italic)     return NULL;
    if (c->bold_title != bold_title) return NULL;
    a = c->family; b = family;
    while (*a && *b && *a == *b) { ++a; ++b; }
    if (*a != *b) return NULL;
    return c->fmt;
}

/* Stores new format. Returns displaced IDWriteTextFormat* (NULL if empty).
 * Caller must Release the returned pointer. */
static void *sui__fmt_cache_set(SuiTextFormatCache *c, void *fmt,
                                 const wchar_t *family, float size_dip,
                                 int weight, int italic, int bold_title)
{
    void *displaced = c->fmt;
    int i = 0;
    c->fmt        = fmt;
    c->size_dip   = size_dip;
    c->weight     = weight;
    c->italic     = italic;
    c->bold_title = bold_title;
    while (family[i] && i < 63) { c->family[i] = family[i]; ++i; }
    c->family[i] = 0;
    return displaced;
}

/* ── Layout cache ─────────────────────────────────────────────────────────── */

/* Returns cached IDWriteTextLayout* on hit (bumps tick), NULL on miss. */
static void *sui__layout_cache_lookup(SuiTextLayoutCache *c, UINT32 hash,
                                       int text_len, void *fmt,
                                       float max_w, float max_h)
{
    int i;
    for (i = 0; i < SUI_LAYOUT_CACHE_SLOTS; i++) {
        SuiTextLayoutEntry *s = &c->slots[i];
        if (!s->layout)              continue;
        if (s->hash     != hash)     continue;
        if (s->text_len != text_len) continue;
        if (s->fmt      != fmt)      continue;
        if (s->max_w    != max_w)    continue;
        if (s->max_h    != max_h)    continue;
        s->tick = ++c->tick;
        return s->layout;
    }
    return NULL;
}

/* Returns the index of the LRU slot (prefers empty, else oldest tick). */
static int sui__layout_cache_find_slot(const SuiTextLayoutCache *c)
{
    int i, best = 0;
    for (i = 0; i < SUI_LAYOUT_CACHE_SLOTS; i++) {
        if (!c->slots[i].layout) return i;
        if (c->slots[i].tick < c->slots[best].tick) best = i;
    }
    return best;
}

/* Stores layout in the LRU slot. Returns displaced IDWriteTextLayout* (or
 * NULL). Caller must Release the returned pointer. */
static void *sui__layout_cache_store(SuiTextLayoutCache *c, void *layout,
                                      void *fmt, UINT32 hash, int text_len,
                                      float max_w, float max_h)
{
    int idx = sui__layout_cache_find_slot(c);
    SuiTextLayoutEntry *s = &c->slots[idx];
    void *displaced = s->layout;
    s->layout   = layout;
    s->fmt      = fmt;
    s->hash     = hash;
    s->text_len = text_len;
    s->max_w    = max_w;
    s->max_h    = max_h;
    s->tick     = ++c->tick;
    return displaced;
}

/* Nullifies all entries whose fmt pointer matches. Writes displaced layout
 * pointers to out[0..return_value-1]. Caller must Release each non-NULL
 * out[i]. Pass out=NULL only in test contexts that use fake pointers. */
static int sui__layout_cache_drop_by_fmt(SuiTextLayoutCache *c, void *fmt,
                                          void **out)
{
    int i, n = 0;
    for (i = 0; i < SUI_LAYOUT_CACHE_SLOTS; i++) {
        SuiTextLayoutEntry *s = &c->slots[i];
        if (s->fmt != fmt || !s->layout) continue;
        if (out) out[n] = s->layout;
        s->layout = NULL;
        s->fmt    = NULL;
        s->hash   = 0;
        n++;
    }
    return n;
}

/* Nullifies ALL layout entries. Writes displaced layout pointers to
 * out[0..return_value-1]. Caller must Release each non-NULL out[i].
 * Pass out=NULL only in test contexts that use fake pointers. */
static int sui__flush_layout_cache(SuiTextLayoutCache *c, void **out)
{
    int i, n = 0;
    for (i = 0; i < SUI_LAYOUT_CACHE_SLOTS; i++) {
        SuiTextLayoutEntry *s = &c->slots[i];
        if (!s->layout) continue;
        if (out) out[n] = s->layout;
        s->layout   = NULL;
        s->fmt      = NULL;
        s->hash     = 0;
        s->text_len = 0;
        n++;
    }
    return n;
}

#endif /* SUI_TEXT_CACHE_H */

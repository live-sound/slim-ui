/* sui_decl.h — Declarative immediate-mode container API.
 * Included from slim_ui.h under SUI_IMPLEMENTATION + SUI_FEATURE_CUSTOM_CONTROLS,
 * after sui_controls.h (depends on sui_ctrl_add_* and SuiContainer).
 *
 * Usage:
 *   sui_begin(host, sui_inset(20.0f), 16.0f, SUI_CLEAR_PAGE);
 *     sui_hstack(host, 2, 20.0f); {
 *       sui_vstack(host); {
 *         sui_label(host, L"Buttons");
 *         sui_flow(host, 28.0f, 8.0f); {
 *           sui_button(host, ID_OK, L"OK", 0);
 *           sui_button(host, ID_CANCEL, L"Cancel", 0);
 *         } sui_end(host);
 *       } sui_end(host);
 *       sui_vstack(host); { ... } sui_end(host);
 *     } sui_end(host);
 *   // (root left on stack — sui_begin will reset on next call)
 *
 * Bottom-bar / absolute-coord controls can be added via the imperative
 * sui_ctrl_add_* API after the declarative tree without conflict.
 */

/* Button flags — pass as third arg to sui_button */
#define SUI_BTN_DEFAULT  1
#define SUI_BTN_DISABLED 2

/* Default control sizes (DIP). Multiplied by host->dpi/96 internally.
 * SUI_DEFAULT_LABEL_H, SUI_DEFAULT_CTRL_H, SUI_DEFAULT_PROG_H, SUI_BUTTON_HPAD,
 * SUI_CHECKBOX_BOX_W, SUI_CHECKBOX_GAP, and SUI_CHECKBOX_H are defined in slim_ui.h. */
#define SUI_DEFAULT_BTN_PAD    28.0f  /* horizontal slack for fit-to-text buttons */
#define SUI_CHECK_BOX_SLACK    (SUI_CHECKBOX_BOX_W + SUI_CHECKBOX_GAP + 6.0f)  /* box + gap + wrap slack */
#define SUI_LABEL_SLACK         4.0f  /* slack to prevent label text wrapping at intrinsic */
#define SUI_DEFAULT_CTRL_MIN_W 80.0f  /* intrinsic floor for non-text controls */
#define SUI_DEFAULT_TEXTVIEW_H 80.0f  /* min textview height ≈ 3 visible lines; grow=1 expands further */
#define SUI_DEFAULT_NAV_ITEM_H 36.0f  /* default nav-list item row height (DIP) */

/* ── Internal helpers ─────────────────────────────────────────────────────── */

static SuiContainer* sui__cont_top(SuiControlHost* host) {
    if (!host || host->container_depth <= 0) return NULL;
    return &host->container_stack[host->container_depth - 1];
}

static SuiContainer* sui__cont_push(SuiControlHost* host) {
    SuiContainer* c;
    if (!host || host->container_depth >= SUI_MAX_CONTAINER_DEPTH) return NULL;
    c = &host->container_stack[host->container_depth++];
    memset(c, 0, sizeof(*c));
    c->parent_ctrl_id = -1;
    return c;
}

static float sui__decl_dpi_s(const SuiControlHost* host) {
    return (host && host->dpi) ? (float)host->dpi / 96.0f : 1.0f;
}

/* Measure the natural (unwrapped) pixel width of `text` using the body font.
 * Routes through host->layout_cache (8-slot LRU) — no allocation on cache hit.
 * Returns 0 on empty text or when renderer has no dwrite_factory. */
static float sui__decl_measure_w(SuiControlHost* host, const wchar_t* text) {
    IDWriteFactory* dw;
    IDWriteTextFormat* fmt;
    IDWriteTextLayout* layout;
    DWRITE_TEXT_METRICS m;
    HRESULT hr;
    UINT32 hash;
    int text_len;
    float result;
    void* cached;
    void* displaced;
    SuiRenderer* r = host ? host->renderer : NULL;
    if (!r || !text || !*text) return 0.0f;
    if (!r->dwrite_factory || !r->font_normal) return 0.0f;
    dw      = (IDWriteFactory*)r->dwrite_factory;
    fmt     = (IDWriteTextFormat*)r->font_normal;
    text_len = (int)wcslen(text);
    hash    = sui__text_hash(text);
    cached  = sui__layout_cache_lookup(&host->layout_cache, hash, text_len,
                                       fmt, 10000.0f, 100.0f);
    if (cached) {
        memset(&m, 0, sizeof(m));
        ((IDWriteTextLayout*)cached)->lpVtbl->GetMetrics((IDWriteTextLayout*)cached, &m);
        return m.widthIncludingTrailingWhitespace;
    }
    layout = NULL;
    hr = sui__text_layout(dw, text, (UINT32)text_len,
                                      fmt, 10000.0f, 100.0f, &layout);
    if (FAILED(hr) || !layout) return 0.0f;
    memset(&m, 0, sizeof(m));
    layout->lpVtbl->GetMetrics(layout, &m);
    result = m.widthIncludingTrailingWhitespace;
    /* Hand layout ownership to cache. On eviction the cache returns the old
     * pointer via sui__layout_cache_store; release it here. */
    displaced = sui__layout_cache_store(&host->layout_cache, layout, fmt,
                                        hash, text_len, 10000.0f, 100.0f);
    if (displaced)
        ((IDWriteTextLayout*)displaced)->lpVtbl->Release((IDWriteTextLayout*)displaced);
    return result;
}

/* Grow `parent->intrinsic_w` from a child item with measured width `w`.
 * VSTACK/FLOW/ROOT take the max. HSTACK uses equal-share, so min row width
 * is `n * max_child + (n-1)*gap` — track max, not sum. Called by
 * sui__decl_place and sui_end. */
static void sui__decl_feed_intrinsic(SuiContainer* parent, float w) {
    if (!parent || w <= 0.0f) return;
    if (parent->type == SUI_CONT_HSTACK) {
        float live;
        if (w > parent->child_max) parent->child_max = w;
        live = (float)parent->hstack_n * parent->child_max;
        if (parent->hstack_n > 1) live += (float)(parent->hstack_n - 1) * parent->gap;
        if (live > parent->intrinsic_w) parent->intrinsic_w = live;
    } else {
        if (w > parent->intrinsic_w) parent->intrinsic_w = w;
    }
}

/* Mirror root container's running intrinsic_w/h into host->intrinsic_min_w/h
 * (which sui_handle_wm_getminmaxinfo reads). The root is never popped — top-
 * level callers do `sui_begin … sui_end` once per top-level container — so
 * root's branch in sui_end doesn't fire. We bubble continuously instead.
 * `cursor_y - y` is the height consumed so far; subtract one trailing gap. */
static void sui__decl_bubble_root(SuiControlHost* host) {
    SuiContainer* root;
    float total_w, consumed_h, total_h;
    if (!host || host->container_depth <= 0) return;
    root = &host->container_stack[0];
    total_w = root->intrinsic_w + root->pad_l + root->pad_r;
    consumed_h = root->cursor_y - root->y;
    if (consumed_h > 0.0f) consumed_h -= root->gap;
    if (consumed_h < 0.0f) consumed_h = 0.0f;
    total_h = consumed_h + root->pad_t + root->pad_b;
    if (total_w > host->intrinsic_min_w) host->intrinsic_min_w = total_w;
    if (total_h > host->intrinsic_min_h) host->intrinsic_min_h = total_h;
}

/* Place a control of size (w_in × h_in) in the current container. Returns
 * x/y/w/h to pass to the underlying sui_ctrl_add_*. w_in/h_in <= 0 means
 * "use container default" (full container width for vstack/root, child slot
 * for hstack, fallback for flow).
 *
 * `intrinsic_w_in` is the content-derived minimum width — what the control
 * needs to look right (e.g. measured text width). It feeds the parent's
 * intrinsic_w *independently* of placement. Pass 0 if the control has no
 * inherent floor (a fill control whose content fits any width). */
static void sui__decl_place(SuiControlHost* host, float w_in, float h_in,
                            float intrinsic_w_in,
                            float* out_x, float* out_y,
                            float* out_w, float* out_h) {
    SuiContainer* c = sui__cont_top(host);
    float w, h;
    *out_x = 0.0f; *out_y = 0.0f; *out_w = w_in; *out_h = h_in;
    if (!c) return;
    w = (w_in > 0.0f) ? w_in : 0.0f;
    h = (h_in > 0.0f) ? h_in : 0.0f;
    if (c->type == SUI_CONT_VSTACK || c->type == SUI_CONT_ROOT ||
        c->type == SUI_CONT_RADIO_GROUP ||
        c->type == SUI_CONT_SCROLLVIEW || c->type == SUI_CONT_CLIPBOX) {
        if (w <= 0.0f) w = c->w;
        *out_x = c->cursor_x;
        *out_y = c->cursor_y;
        *out_w = w; *out_h = h;
        c->cursor_y += h + c->gap;
        c->n_total_children++;
    } else if (c->type == SUI_CONT_HSTACK) {
        if (w <= 0.0f) w = c->child_w;
        *out_x = c->x + (float)c->hstack_idx * (c->child_w + c->gap);
        *out_y = c->y;
        *out_w = w; *out_h = h;
        c->hstack_idx++;
        c->n_total_children++;
        if (h > c->row_max_h) c->row_max_h = h;
    } else if (c->type == SUI_CONT_FLOW) {
        if (h <= 0.0f) h = c->item_h;
        if (w <= 0.0f) w = c->item_h * 4.0f;
        if (c->cursor_x > c->x && c->cursor_x + w > c->x + c->w) {
            /* wrap */
            c->cursor_x = c->x;
            c->cursor_y += c->row_max_h + c->gap;
            c->row_max_h = c->item_h;
        }
        *out_x = c->cursor_x;
        *out_y = c->cursor_y;
        *out_w = w; *out_h = h;
        c->cursor_x += w + c->gap;
        if (h > c->row_max_h) c->row_max_h = h;
    }
    if (intrinsic_w_in > 0.0f) sui__decl_feed_intrinsic(c, intrinsic_w_in);
    sui__decl_bubble_root(host);
}

/* Phase 2b: record `idx` as an atom flex slot of the top container. Called
 * by control wrappers after sui_ctrl_add_*. Only HSTACK and VSTACK get
 * recorded (the only containers with two-pass distribute). FLOW children,
 * ROOT/RADIO_GROUP children, and overflow past SUI_MAX_FLEX_SLOTS are not
 * recorded — they keep legacy single-pass placement. Recording is
 * unconditional (cheap); distribute sweeps slots for flex hints. */
static void sui__decl_record_atom(SuiControlHost* host, int idx) {
    SuiContainer* parent;
    int i, pcid = -1;
    if (!host || idx < 0 || idx >= host->control_count) return;
    /* Walk up the container stack and bind this atom to the nearest
     * container that declared a reparent target (sui_scrollview /
     * sui_clipbox). Mirrors sui_radio's RADIO_GROUP lookup so nested
     * vstack/hstack inside a scrollview still reparent correctly. */
    for (i = host->container_depth - 1; i >= 0; --i) {
        if (host->container_stack[i].parent_ctrl_id >= 0) {
            pcid = host->container_stack[i].parent_ctrl_id;
            break;
        }
    }
    if (pcid >= 0) host->controls[idx].parent_id = pcid;
    parent = sui__cont_top(host);
    if (!parent) return;
    if (parent->type != SUI_CONT_HSTACK && parent->type != SUI_CONT_VSTACK) return;
    if (parent->flex_slot_count >= SUI_MAX_FLEX_SLOTS) return;
    parent->flex_slot_ids[parent->flex_slot_count++] = idx;
}

/* Phase 2b: redistribute slot widths in a HSTACK with at least one flex
 * hint set. Sweeps slots for grow > 0, pref_w > 0, or max_w > 0; if none,
 * leaves the legacy equal-share placement (already written to c->x/c->w by
 * sui__decl_place). Otherwise:
 *   - basis = pref_w (or effective if 0)
 *   - leftover = available − Σbasis; if > 0 and Σgrow > 0, share leftover by grow
 *   - if leftover < 0, shrink by shrink·basis when present; otherwise fall
 *     back to grow·basis so children declaring only `grow` still shrink.
 *     Clamped to effective_size floor.
 *   - max_w caps every slot
 * Requires every hstack child to be an atom (flex_slot_count == hstack_n);
 * mixed atom/container hstacks fall back to legacy placement. */
static void sui__decl_distribute_hstack(SuiControlHost* host, SuiContainer* h) {
    int   i, n;
    int   has_hint = 0;
    float total_pref = 0.0f, total_grow = 0.0f, total_shrink_basis = 0.0f;
    float available, leftover, cursor;
    float pref[SUI_MAX_FLEX_SLOTS];
    float final_w[SUI_MAX_FLEX_SLOTS];
    if (!host || !h) return;
    n = h->flex_slot_count;
    if (n <= 0 || n != h->hstack_n) return;
    for (i = 0; i < n; ++i) {
        SuiControl* c = &host->controls[h->flex_slot_ids[i]];
        if (c->grow > 0 || c->pref_w > 0.0f || c->max_w > 0.0f) { has_hint = 1; break; }
    }
    if (!has_hint) return;
    available = h->w - (float)(n - 1) * h->gap;
    if (available < 0.0f) available = 0.0f;
    for (i = 0; i < n; ++i) {
        SuiControl* c = &host->controls[h->flex_slot_ids[i]];
        float pw = 0.0f, ph = 0.0f;
        sui_ctrl_pref_size(host, c, &pw, &ph, 0);
        pref[i] = pw;
        total_pref += pw;
        total_grow += (float)c->grow;
        if (c->shrink > 0) total_shrink_basis += pw * (float)c->shrink;
    }
    leftover = available - total_pref;
    if (leftover > 0.0f && total_grow > 0.0f) {
        for (i = 0; i < n; ++i) {
            SuiControl* c = &host->controls[h->flex_slot_ids[i]];
            float share = (float)c->grow / total_grow;
            float w = pref[i] + leftover * share;
            if (c->max_w > 0.0f && w > c->max_w) w = c->max_w;
            final_w[i] = w;
        }
    } else if (leftover < 0.0f && (total_shrink_basis > 0.0f || total_grow > 0.0f)) {
        /* Shrink. Prefer explicit shrink weights; fall back to grow weights
         * so hstacks declaring only `grow` still shrink with the parent
         * (CSS-flex symmetry). Clamped to effective_size floor. */
        float deficit = -leftover;
        int use_grow_fallback = (total_shrink_basis <= 0.0f);
        float basis_sum;
        if (use_grow_fallback) {
            basis_sum = 0.0f;
            for (i = 0; i < n; ++i) {
                SuiControl* c = &host->controls[h->flex_slot_ids[i]];
                basis_sum += pref[i] * (float)c->grow;
            }
        } else {
            basis_sum = total_shrink_basis;
        }
        for (i = 0; i < n; ++i) {
            SuiControl* c = &host->controls[h->flex_slot_ids[i]];
            float weight = use_grow_fallback ? (float)c->grow : (float)c->shrink;
            float basis = pref[i] * weight;
            float share = (basis_sum > 0.0f) ? basis / basis_sum : 0.0f;
            float w = pref[i] - deficit * share;
            float fw = 0.0f, fh = 0.0f;
            sui_ctrl_effective_size(host, c, &fw, &fh, 0);
            if (w < fw) w = fw;
            final_w[i] = w;
        }
    } else {
        for (i = 0; i < n; ++i) {
            SuiControl* c = &host->controls[h->flex_slot_ids[i]];
            float w = pref[i];
            if (c->max_w > 0.0f && w > c->max_w) w = c->max_w;
            final_w[i] = w;
        }
    }
    cursor = h->x;
    for (i = 0; i < n; ++i) {
        SuiControl* c = &host->controls[h->flex_slot_ids[i]];
        c->x = cursor;
        c->w = final_w[i];
        cursor += final_w[i] + h->gap;
    }
}

/* Phase 2b: redistribute slot heights in a VSTACK with at least one flex
 * hint set. Mirrors distribute_hstack on Y axis. Skipped when v->h == 0
 * (Phase 2b first cut: only VSTACKs directly under ROOT inherit h). After
 * distribution v->cursor_y is updated so sui_end's consumed_h derivation
 * reflects the new layout. */
static void sui__decl_distribute_vstack(SuiControlHost* host, SuiContainer* v) {
    int   i, n;
    int   has_hint = 0;
    float total_pref = 0.0f, total_grow = 0.0f, total_shrink_basis = 0.0f;
    float available, leftover, cursor;
    float pref[SUI_MAX_FLEX_SLOTS];
    float final_h[SUI_MAX_FLEX_SLOTS];
    if (!host || !v) return;
    n = v->flex_slot_count;
    if (n <= 0) return;
    if (v->h <= 0.0f) return;
    /* Refuse distribution when a non-atom sibling exists; atom-only y
     * positions would silently desync from the actual layout. Caller is
     * expected to keep flex VSTACKs all-atoms (Phase 2b first cut). */
    if (n != v->n_total_children) return;
    for (i = 0; i < n; ++i) {
        SuiControl* c = &host->controls[v->flex_slot_ids[i]];
        if (c->grow > 0 || c->pref_h > 0.0f || c->max_h > 0.0f) { has_hint = 1; break; }
    }
    if (!has_hint) return;
    available = v->h - (float)(n - 1) * v->gap;
    if (available < 0.0f) available = 0.0f;
    for (i = 0; i < n; ++i) {
        SuiControl* c = &host->controls[v->flex_slot_ids[i]];
        float pw = 0.0f, ph = 0.0f;
        sui_ctrl_pref_size(host, c, &pw, &ph, 0);
        pref[i] = ph;
        total_pref += ph;
        total_grow += (float)c->grow;
        if (c->shrink > 0) total_shrink_basis += ph * (float)c->shrink;
    }
    leftover = available - total_pref;
    if (leftover > 0.0f && total_grow > 0.0f) {
        for (i = 0; i < n; ++i) {
            SuiControl* c = &host->controls[v->flex_slot_ids[i]];
            float share = (float)c->grow / total_grow;
            float hh = pref[i] + leftover * share;
            if (c->max_h > 0.0f && hh > c->max_h) hh = c->max_h;
            final_h[i] = hh;
        }
    } else if (leftover < 0.0f && (total_shrink_basis > 0.0f || total_grow > 0.0f)) {
        /* Shrink. Prefer explicit shrink weights; fall back to grow weights
         * so vstacks declaring only `grow` still shrink with the parent
         * (CSS-flex symmetry). Clamped to effective_size floor. */
        float deficit = -leftover;
        int use_grow_fallback = (total_shrink_basis <= 0.0f);
        float basis_sum;
        if (use_grow_fallback) {
            basis_sum = 0.0f;
            for (i = 0; i < n; ++i) {
                SuiControl* c = &host->controls[v->flex_slot_ids[i]];
                basis_sum += pref[i] * (float)c->grow;
            }
        } else {
            basis_sum = total_shrink_basis;
        }
        for (i = 0; i < n; ++i) {
            SuiControl* c = &host->controls[v->flex_slot_ids[i]];
            float weight = use_grow_fallback ? (float)c->grow : (float)c->shrink;
            float basis = pref[i] * weight;
            float share = (basis_sum > 0.0f) ? basis / basis_sum : 0.0f;
            float hh = pref[i] - deficit * share;
            float fw = 0.0f, fh = 0.0f;
            sui_ctrl_effective_size(host, c, &fw, &fh, 0);
            if (hh < fh) hh = fh;
            final_h[i] = hh;
        }
    } else {
        for (i = 0; i < n; ++i) {
            SuiControl* c = &host->controls[v->flex_slot_ids[i]];
            float hh = pref[i];
            if (c->max_h > 0.0f && hh > c->max_h) hh = c->max_h;
            final_h[i] = hh;
        }
    }
    cursor = v->y;
    for (i = 0; i < n; ++i) {
        SuiControl* c = &host->controls[v->flex_slot_ids[i]];
        c->y = cursor;
        c->h = final_h[i];
        cursor += final_h[i] + v->gap;
    }
    v->cursor_y = cursor;
}

/* ── Public API: sui_begin / sui_end / containers ─────────────────────────── */

/* Clear the control list (per `clear`), reset container state, and push a ROOT
 * container sized to the client rect inset by `inset_dip` (DIPs, DPI-scaled).
 * `gap_dip` is the default vertical gap between top-level children. */
static void sui_begin(SuiControlHost* host, SuiRect inset_dip, float gap_dip, SuiClearMode clear) {
    RECT cr;
    SuiContainer* root;
    float s, W, H, pl, pt, pr, pb, gap;
    if (!host) return;
    if (clear == SUI_CLEAR_PAGE)
        sui_ctrl_clear_page(host);
    GetClientRect(host->hwnd, &cr);
    s   = sui__decl_dpi_s(host);
    W   = (float)cr.right;
    H   = (float)cr.bottom;
    pl  = inset_dip.left   * s;
    pt  = inset_dip.top    * s;
    pr  = inset_dip.right  * s;
    pb  = inset_dip.bottom * s;
    gap = gap_dip * s;
    host->container_depth  = 0;
    host->intrinsic_min_w  = 0.0f;
    host->intrinsic_min_h  = 0.0f;
    root = sui__cont_push(host);
    if (!root) return;
    root->type     = SUI_CONT_ROOT;
    root->x        = pl;
    root->y        = pt;
    root->w        = (W > pl + pr) ? (W - pl - pr) : 0.0f;
    root->h        = (H > pt + pb) ? (H - pt - pb) : 0.0f;
    root->cursor_x = root->x;
    root->cursor_y = root->y;
    root->gap      = gap;
    root->pad_l    = pl;
    root->pad_t    = pt;
    root->pad_r    = pr;
    root->pad_b    = pb;
}

/* Variant of sui_begin where `rect_px` and `gap_px` are already in physical
 * pixels. Resets container state and pushes a ROOT at the given pixel rect.
 * Used when the caller has pre-computed pixel geometry (e.g. section-level
 * layout). Returns the new root container, or NULL on depth overflow. */
static SuiContainer* sui_begin_px(SuiControlHost* host, SuiRect rect_px, float gap_px, SuiClearMode clear) {
    SuiContainer* root;
    if (!host) return NULL;
    if (clear == SUI_CLEAR_PAGE)
        sui_ctrl_clear_page(host);
    host->container_depth = 0;
    host->intrinsic_min_w = 0.0f;
    host->intrinsic_min_h = 0.0f;
    root = sui__cont_push(host);
    if (!root) return NULL;
    root->type     = SUI_CONT_ROOT;
    root->x        = rect_px.left;
    root->y        = rect_px.top;
    root->w        = (rect_px.right  > rect_px.left) ? (rect_px.right  - rect_px.left) : 0.0f;
    root->h        = (rect_px.bottom > rect_px.top)  ? (rect_px.bottom - rect_px.top)  : 0.0f;
    root->cursor_x = root->x;
    root->cursor_y = root->y;
    root->gap      = gap_px;
    /* pad fields stay 0 (zeroed by sui__cont_push) */
    return root;
}

/* Inherit x/y/w from `parent` for vstack-style child containers (vstack,
 * vgroup, radio_group). Used by sui_vstack, sui_vgroup, sui_radio_group.
 * Also propagates `h` when the parent is ROOT — Phase 2b vertical flex-grow
 * works only at the outermost VSTACK directly under ROOT (everywhere else
 * sets c->h = 0, which sui__decl_distribute_vstack interprets as
 * "distribution unavailable, fall back to legacy intrinsic-stack"). */
static void sui__decl_vstack_inherit(SuiContainer* c, SuiContainer* parent) {
    if (parent->type == SUI_CONT_HSTACK) {
        c->x = parent->x + (float)parent->hstack_idx * (parent->child_w + parent->gap);
        c->y = parent->y;
        c->w = parent->child_w;
        c->h = 0.0f;
    } else {
        c->x = parent->cursor_x;
        c->y = parent->cursor_y;
        c->w = parent->w;
        c->h = (parent->type == SUI_CONT_ROOT) ? parent->h : 0.0f;
    }
    c->cursor_x = c->x;
    c->cursor_y = c->y;
}

/* Push a vertical stacker. Inherits width from parent (full width in
 * vstack/root, slot width in hstack). Items advance Y with parent's gap. */
static void sui_vstack(SuiControlHost* host) {
    SuiContainer* parent = sui__cont_top(host);
    SuiContainer* c;
    if (!parent) return;
    c = sui__cont_push(host);
    if (!c) return;
    c->type = SUI_CONT_VSTACK;
    c->gap  = parent->gap;
    sui__decl_vstack_inherit(c, parent);
    parent->n_total_children++;
}

/* Push a vstack with explicit gap (DIP). Use to group label+control or a
 * cluster of related items more tightly than the parent's default gap. */
static void sui_vgroup(SuiControlHost* host, float gap_dip) {
    SuiContainer* parent = sui__cont_top(host);
    SuiContainer* c;
    if (!parent) return;
    c = sui__cont_push(host);
    if (!c) return;
    c->type = SUI_CONT_VSTACK;
    c->gap  = gap_dip * sui__decl_dpi_s(host);
    sui__decl_vstack_inherit(c, parent);
    parent->n_total_children++;
}

/* Push a radio group. Behaves as a tight vstack and stores `group_id` so
 * inner sui_radio() calls don't have to repeat it. Default gap = 4 DIP. */
static void sui_radio_group(SuiControlHost* host, int group_id) {
    SuiContainer* parent = sui__cont_top(host);
    SuiContainer* c;
    if (!parent) return;
    c = sui__cont_push(host);
    if (!c) return;
    c->type           = SUI_CONT_RADIO_GROUP;
    c->gap            = 4.0f * sui__decl_dpi_s(host);
    c->radio_group_id = group_id;
    sui__decl_vstack_inherit(c, parent);
    parent->n_total_children++;
}

/* Push a horizontal stacker with `n_children` equal-width slots separated
 * by `gap_dip`. Children read their slot via parent->hstack_idx. */
static void sui_hstack(SuiControlHost* host, int n_children, float gap_dip) {
    SuiContainer* parent = sui__cont_top(host);
    SuiContainer* c;
    float s, gap;
    if (!parent || n_children <= 0) return;
    c = sui__cont_push(host);
    if (!c) return;
    s   = sui__decl_dpi_s(host);
    gap = gap_dip * s;
    c->type       = SUI_CONT_HSTACK;
    c->gap        = gap;
    c->hstack_n   = n_children;
    c->hstack_idx = 0;
    if (parent->type == SUI_CONT_HSTACK) {
        c->x = parent->x + (float)parent->hstack_idx * (parent->child_w + parent->gap);
        c->y = parent->y;
        c->w = parent->child_w;
    } else {
        c->x = parent->cursor_x;
        c->y = parent->cursor_y;
        c->w = parent->w;
    }
    {
        float inner = c->w - (float)(n_children - 1) * gap;
        c->child_w = (inner > 0.0f) ? (inner / (float)n_children) : 0.0f;
    }
    c->cursor_x = c->x;
    c->cursor_y = c->y;
    parent->n_total_children++;
}

/* Push a flow container. Children advance X with auto-wrap. `item_h_dip`
 * is the default row height (auto-grows if a child sets a taller h). */
static void sui_flow(SuiControlHost* host, float item_h_dip, float gap_dip) {
    SuiContainer* parent = sui__cont_top(host);
    SuiContainer* c;
    float s;
    if (!parent) return;
    c = sui__cont_push(host);
    if (!c) return;
    s = sui__decl_dpi_s(host);
    c->type      = SUI_CONT_FLOW;
    c->gap       = gap_dip * s;
    c->item_h    = item_h_dip * s;
    c->row_max_h = c->item_h;
    if (parent->type == SUI_CONT_HSTACK) {
        c->x = parent->x + (float)parent->hstack_idx * (parent->child_w + parent->gap);
        c->y = parent->y;
        c->w = parent->child_w;
    } else {
        c->x = parent->cursor_x;
        c->y = parent->cursor_y;
        c->w = parent->w;
    }
    c->cursor_x = c->x;
    c->cursor_y = c->y;
    parent->n_total_children++;
}

/* Push a scrollview container. Wraps sui_ctrl_add_scrollview: descendant
 * atoms (and their nested vstack/hstack/flow children) are reparented to
 * the scrollview's id via sui__decl_record_atom's walk-up. content_h is
 * committed at sui_end from the container's advanced cursor.
 *
 * Reserves SUI_SCROLLVIEW_SB_HOT_W on the right for the overlay scrollbar
 * — children placed inside see a content-area width narrower than the
 * scrollview rect by that gutter, so labels and controls clear the thumb
 * hit zone.
 *
 * `id` must be >= 0. -1 is the parent_ctrl_id sentinel and would silently
 * disable child reparenting if accepted.
 *
 * `w_dip` / `h_dip` are taken as fixed extents; the parent container's
 * width distribution does not flow into the scrollview's rect. Size it
 * deliberately at the call site. */
static void sui_scrollview(SuiControlHost* host, int id,
                           float w_dip, float h_dip, float gap_dip) {
    SuiContainer* parent = sui__cont_top(host);
    SuiContainer* c;
    float s, w, h, x, y, ow, oh;
    int idx;
    if (!host || !parent || id < 0) return;
    s = sui__decl_dpi_s(host);
    w = w_dip * s;
    h = h_dip * s;
    sui__decl_place(host, w, h, w, &x, &y, &ow, &oh);
    idx = sui_ctrl_add_scrollview(host, id, x, y, ow, oh, 0.0f);
    if (idx < 0) return;
    sui__decl_record_atom(host, idx);
    c = sui__cont_push(host);
    if (!c) return;
    c->type           = SUI_CONT_SCROLLVIEW;
    c->parent_ctrl_id = id;
    c->x              = x;
    c->y              = y;
    c->w              = ow - SUI_SCROLLVIEW_SB_HOT_W * s;
    if (c->w < 0.0f) c->w = 0.0f;
    c->h              = oh;
    c->cursor_x       = c->x;
    c->cursor_y       = c->y;
    c->gap            = gap_dip * s;
}

/* Push a clipping container. Wraps sui_ctrl_add_container: behaves like
 * a fixed-rect viewport whose descendants are reparented to its id, so
 * sui_ctrl_draw_subtree clips them to its rect. No scrollbar, no scroll
 * offset — content that overflows is simply clipped.
 *
 * `id` must be >= 0 (same constraint as sui_scrollview). */
static void sui_clipbox(SuiControlHost* host, int id,
                        float w_dip, float h_dip, float gap_dip) {
    SuiContainer* parent = sui__cont_top(host);
    SuiContainer* c;
    float s, w, h, x, y, ow, oh;
    int idx;
    if (!host || !parent || id < 0) return;
    s = sui__decl_dpi_s(host);
    w = w_dip * s;
    h = h_dip * s;
    sui__decl_place(host, w, h, w, &x, &y, &ow, &oh);
    idx = sui_ctrl_add_container(host, id, x, y, ow, oh);
    if (idx < 0) return;
    sui__decl_record_atom(host, idx);
    c = sui__cont_push(host);
    if (!c) return;
    c->type           = SUI_CONT_CLIPBOX;
    c->parent_ctrl_id = id;
    c->x              = x;
    c->y              = y;
    c->w              = ow;
    c->h              = oh;
    c->cursor_x       = c->x;
    c->cursor_y       = c->y;
    c->gap            = gap_dip * s;
}

/* Pop the topmost container. Bubbles intrinsic width and consumed height
 * into its parent. */
static void sui_end(SuiControlHost* host) {
    SuiContainer* self;
    SuiContainer* parent;
    float consumed_h = 0.0f;
    if (!host || host->container_depth <= 0) return;
    self = &host->container_stack[host->container_depth - 1];

    if (self->type == SUI_CONT_VSTACK || self->type == SUI_CONT_ROOT ||
        self->type == SUI_CONT_RADIO_GROUP) {
        if (self->type == SUI_CONT_VSTACK)
            sui__decl_distribute_vstack(host, self);
        consumed_h = self->cursor_y - self->y;
        if (consumed_h > 0.0f) consumed_h -= self->gap;  /* trailing gap from last item */
        if (consumed_h < 0.0f) consumed_h = 0.0f;
    } else if (self->type == SUI_CONT_SCROLLVIEW ||
               self->type == SUI_CONT_CLIPBOX) {
        /* Content height = how far the cursor advanced from the top edge,
         * minus the trailing gap after the last child. Scrollview commits
         * that to its underlying control so the scrollbar thumb is sized.
         * Both container kinds expose their *fixed* self->h to the parent
         * — they are clipped/scrolled viewports, not flowing containers. */
        float content_h = self->cursor_y - self->y;
        if (content_h > 0.0f) content_h -= self->gap;
        if (content_h < 0.0f) content_h = 0.0f;
        if (self->type == SUI_CONT_SCROLLVIEW && self->parent_ctrl_id >= 0)
            sui_ctrl_set_scrollview_content_height(host, self->parent_ctrl_id, content_h);
        consumed_h = self->h;
    } else if (self->type == SUI_CONT_FLOW) {
        consumed_h = self->cursor_y - self->y;
        if (self->cursor_x > self->x) consumed_h += self->row_max_h;
        if (consumed_h < 0.0f) consumed_h = 0.0f;
    } else if (self->type == SUI_CONT_HSTACK) {
        float live;
        sui__decl_distribute_hstack(host, self);
        consumed_h = self->row_max_h;
        live = (float)self->hstack_n * self->child_max;
        if (self->hstack_n > 1) live += (float)(self->hstack_n - 1) * self->gap;
        if (live > self->intrinsic_w) self->intrinsic_w = live;
    }

    host->container_depth--;
    if (host->container_depth <= 0) {
        /* Popped root */
        float total_w = self->intrinsic_w + self->pad_l + self->pad_r;
        float total_h = consumed_h + self->pad_t + self->pad_b;
        if (total_w > host->intrinsic_min_w) host->intrinsic_min_w = total_w;
        if (total_h > host->intrinsic_min_h) host->intrinsic_min_h = total_h;
        return;
    }
    parent = &host->container_stack[host->container_depth - 1];

    sui__decl_feed_intrinsic(parent, self->intrinsic_w);

    if (parent->type == SUI_CONT_HSTACK) {
        if (consumed_h > parent->row_max_h) parent->row_max_h = consumed_h;
        parent->hstack_idx++;
    } else if (parent->type == SUI_CONT_FLOW) {
        if (parent->cursor_x > parent->x &&
            parent->cursor_x + self->intrinsic_w > parent->x + parent->w) {
            parent->cursor_x = parent->x;
            parent->cursor_y += parent->row_max_h + parent->gap;
            parent->row_max_h = parent->item_h;
        }
        parent->cursor_x += self->intrinsic_w + parent->gap;
        if (consumed_h > parent->row_max_h) parent->row_max_h = consumed_h;
    } else {
        /* VSTACK or ROOT */
        parent->cursor_y += consumed_h + parent->gap;
    }
    sui__decl_bubble_root(host);
}

/* ── Control wrappers ─────────────────────────────────────────────────────── */

/* Button sized to its label text + slack. Pass SUI_BTN_DEFAULT and/or
 * SUI_BTN_DISABLED in `flags`. */
static int sui_button(SuiControlHost* host, int id, const wchar_t* text, int flags) {
    float s, tw, w, h, x, y, ow, oh;
    int idx;
    if (!host) return -1;
    s  = sui__decl_dpi_s(host);
    tw = sui__decl_measure_w(host, text);
    w  = tw + SUI_DEFAULT_BTN_PAD * s;
    h  = SUI_DEFAULT_CTRL_H * s;
    sui__decl_place(host, w, h, w, &x, &y, &ow, &oh);
    idx = sui_ctrl_add_button(host, id, x, y, ow, oh, text,
                              (flags & SUI_BTN_DEFAULT) ? 1 : 0);
    if (idx >= 0 && (flags & SUI_BTN_DISABLED))
        host->controls[idx].state |= SUI_CTRL_DISABLED;
    sui__decl_record_atom(host, idx);
    return idx;
}

/* Label, full container width. Intrinsic = measured text width so root
 * widens for long labels. */
static int sui_label(SuiControlHost* host, const wchar_t* text) {
    float s, h, x, y, ow, oh, tw;
    if (!host) return -1;
    s  = sui__decl_dpi_s(host);
    h  = SUI_DEFAULT_LABEL_H * s;
    tw = sui__decl_measure_w(host, text);
    sui__decl_place(host, 0.0f, h, tw + SUI_LABEL_SLACK * s, &x, &y, &ow, &oh);
    {
        int idx = sui_ctrl_add_label(host, -1, x, y, ow, oh, text);
        sui__decl_record_atom(host, idx);
        return idx;
    }
}

static int sui_tooltip(SuiControlHost* host, int id, const wchar_t* tooltip) {
    float s, size, x, y, ow, oh;
    int idx;
    if (!host) return -1;
    s = sui__decl_dpi_s(host);
    size = SUI_DEFAULT_TOOLTIP_SIZE * s;
    sui__decl_place(host, size, size, size, &x, &y, &ow, &oh);
    idx = sui_ctrl_add_tooltip(host, id, x, y, ow, oh, tooltip);
    sui__decl_record_atom(host, idx);
    return idx;
}

static int sui_checkbox(SuiControlHost* host, int id, const wchar_t* text, int checked) {
    float s, h, x, y, ow, oh, tw, iw;
    if (!host) return -1;
    s  = sui__decl_dpi_s(host);
    h  = SUI_DEFAULT_CTRL_H * s;
    tw = sui__decl_measure_w(host, text);
    iw = SUI_CHECK_BOX_SLACK * s + tw;
    sui__decl_place(host, 0.0f, h, iw, &x, &y, &ow, &oh);
    {
        int idx = sui_ctrl_add_checkbox(host, id, x, y, ow, oh, text, checked);
        sui__decl_record_atom(host, idx);
        return idx;
    }
}

/* Radio button. Picks up group_id from the nearest enclosing
 * sui_radio_group on the container stack. Falls back to 0 if none. */
static int sui_radio(SuiControlHost* host, int id, const wchar_t* text, int checked) {
    float s, h, x, y, ow, oh, tw, iw;
    int group_id = 0;
    int i;
    if (!host) return -1;
    for (i = host->container_depth - 1; i >= 0; --i) {
        if (host->container_stack[i].type == SUI_CONT_RADIO_GROUP) {
            group_id = host->container_stack[i].radio_group_id;
            break;
        }
    }
    s  = sui__decl_dpi_s(host);
    h  = SUI_DEFAULT_CTRL_H * s;
    tw = sui__decl_measure_w(host, text);
    iw = SUI_CHECK_BOX_SLACK * s + tw;
    sui__decl_place(host, 0.0f, h, iw, &x, &y, &ow, &oh);
    {
        int idx = sui_ctrl_add_radio(host, id, x, y, ow, oh, text, group_id, checked);
        sui__decl_record_atom(host, idx);
        return idx;
    }
}

static int sui_edit(SuiControlHost* host, int id, const wchar_t* text,
                    const wchar_t* placeholder, int numeric_only) {
    float s, h, x, y, ow, oh;
    if (!host) return -1;
    s = sui__decl_dpi_s(host);
    h = SUI_DEFAULT_CTRL_H * s;
    sui__decl_place(host, 0.0f, h, SUI_DEFAULT_CTRL_MIN_W * s, &x, &y, &ow, &oh);
    {
        int idx = sui_ctrl_add_edit(host, id, x, y, ow, oh, text, placeholder, numeric_only);
        sui__decl_record_atom(host, idx);
        return idx;
    }
}

static int sui_combobox(SuiControlHost* host, int id,
                        const wchar_t** items, int item_count, int selected) {
    float s, h, x, y, ow, oh;
    if (!host) return -1;
    s = sui__decl_dpi_s(host);
    h = SUI_DEFAULT_CTRL_H * s;
    sui__decl_place(host, 0.0f, h, SUI_DEFAULT_CTRL_MIN_W * s, &x, &y, &ow, &oh);
    {
        int idx = sui_ctrl_add_combobox(host, id, x, y, ow, oh, items, item_count, selected);
        sui__decl_record_atom(host, idx);
        return idx;
    }
}

static int sui_slider(SuiControlHost* host, int id,
                      float minv, float maxv, float value, float step) {
    float s, h, x, y, ow, oh;
    if (!host) return -1;
    s = sui__decl_dpi_s(host);
    h = SUI_DEFAULT_CTRL_H * s;
    sui__decl_place(host, 0.0f, h, SUI_DEFAULT_CTRL_MIN_W * s, &x, &y, &ow, &oh);
    {
        int idx = sui_ctrl_add_slider(host, id, x, y, ow, oh, minv, maxv, value, step);
        sui__decl_record_atom(host, idx);
        return idx;
    }
}

static int sui_slider_p(SuiControlHost* host, int id, SuiSliderParams p) {
    return sui_slider(host, id, p.min, p.max, p.value, p.step);
}

static int sui_progress(SuiControlHost* host, int id,
                        float minv, float maxv, float value) {
    float s, h, x, y, ow, oh;
    if (!host) return -1;
    s = sui__decl_dpi_s(host);
    h = SUI_DEFAULT_PROG_H * s;
    sui__decl_place(host, 0.0f, h, SUI_DEFAULT_CTRL_MIN_W * s, &x, &y, &ow, &oh);
    {
        int idx = sui_ctrl_add_progress(host, id, x, y, ow, oh, minv, maxv, value);
        sui__decl_record_atom(host, idx);
        return idx;
    }
}

static int sui_spinner(SuiControlHost* host, int id,
                       float minv, float maxv, float value, float step) {
    float s, h, x, y, ow, oh;
    if (!host) return -1;
    s = sui__decl_dpi_s(host);
    h = SUI_DEFAULT_CTRL_H * s;
    sui__decl_place(host, 0.0f, h, SUI_DEFAULT_CTRL_MIN_W * s, &x, &y, &ow, &oh);
    {
        int idx = sui_ctrl_add_spinner(host, id, x, y, ow, oh, minv, maxv, value, step);
        sui__decl_record_atom(host, idx);
        return idx;
    }
}

static int sui_spinner_p(SuiControlHost* host, int id, SuiSpinnerParams p) {
    return sui_spinner(host, id, p.min, p.max, p.value, p.step);
}

/* Color swatch — explicit width (DIP). Default control height. */
static int sui_swatch(SuiControlHost* host, int id, float w_dip, uint32_t color) {
    float s, w, h, x, y, ow, oh;
    if (!host) return -1;
    s = sui__decl_dpi_s(host);
    w = w_dip * s;
    h = SUI_DEFAULT_CTRL_H * s;
    sui__decl_place(host, w, h, w, &x, &y, &ow, &oh);
    {
        int idx = sui_ctrl_add_color_swatch(host, id, x, y, ow, oh, color);
        sui__decl_record_atom(host, idx);
        return idx;
    }
}

static int sui_shortcut_keyboard(SuiControlHost* host, int id, UINT vk, UINT modifiers) {
    float s, h, x, y, ow, oh;
    if (!host) return -1;
    s = sui__decl_dpi_s(host);
    h = SUI_DEFAULT_CTRL_H * s;
    sui__decl_place(host, 0.0f, h, SUI_DEFAULT_CTRL_MIN_W * s, &x, &y, &ow, &oh);
    {
        int idx = sui_ctrl_add_shortcut_recorder_keyboard(host, id, x, y, ow, oh,
                                                          vk, modifiers, NULL);
        sui__decl_record_atom(host, idx);
        return idx;
    }
}

static int sui_shortcut_gamepad(SuiControlHost* host, int id, uint32_t buttons) {
    float s, h, x, y, ow, oh;
    if (!host) return -1;
    s = sui__decl_dpi_s(host);
    h = SUI_DEFAULT_CTRL_H * s;
    sui__decl_place(host, 0.0f, h, SUI_DEFAULT_CTRL_MIN_W * s, &x, &y, &ow, &oh);
    {
        int idx = sui_ctrl_add_shortcut_recorder_gamepad(host, id, x, y, ow, oh,
                                                         buttons, NULL);
        sui__decl_record_atom(host, idx);
        return idx;
    }
}

/* Multi-line text view. Takes full container width and a default min
 * height of SUI_DEFAULT_TEXTVIEW_H DIPs (≈3 visible lines). The canonical
 * use is as a vstack child with `sui_last_grow(1)` so flex distribution
 * fills the remaining vertical space — without grow, the textview stays
 * at the minimum height. `capacity_chars` is the editable buffer size
 * (must hold initial_text plus any user-typed content + NUL). Pass
 * `readonly=1` for license / EULA / log-viewer scenarios where
 * selection + Ctrl+C is wanted but typing is not. */
static int sui_textview(SuiControlHost* host, int id,
                        const wchar_t* initial_text,
                        int capacity_chars, int readonly) {
    float s, h, x, y, ow, oh;
    if (!host) return -1;
    s = sui__decl_dpi_s(host);
    h = SUI_DEFAULT_TEXTVIEW_H * s;
    sui__decl_place(host, 0.0f, h, SUI_DEFAULT_CTRL_MIN_W * s, &x, &y, &ow, &oh);
    {
        int idx = sui_ctrl_add_textview(host, id, x, y, ow, oh,
                                        initial_text, capacity_chars, readonly);
        sui__decl_record_atom(host, idx);
        return idx;
    }
}

/* Bitmap display. Explicit width + height in DIPs (image controls have
 * no natural intrinsic size — the asset's aspect is arbitrary, so
 * callers always know the slot dimensions). Inherits the imperative
 * default scale (SUI_IMG_FILL); call sui_ctrl_image_set_scale after
 * creation to switch to FIT or CLIP. */
static int sui_image(SuiControlHost* host, int id,
                     float w_dip, float h_dip,
                     SuiImageHandle img, int circle_mask) {
    float s, w, h, x, y, ow, oh;
    if (!host) return -1;
    s = sui__decl_dpi_s(host);
    w = w_dip * s;
    h = h_dip * s;
    sui__decl_place(host, w, h, w, &x, &y, &ow, &oh);
    {
        int idx = sui_ctrl_add_image(host, id, x, y, ow, oh, img, circle_mask);
        sui__decl_record_atom(host, idx);
        return idx;
    }
}

/* Vertical sidebar nav list. Items are added separately via
 * sui_ctrl_nav_add_item(host, id, text, icon, icon_color) after this
 * call — the wrapper only constructs the empty list. `item_h_dip` of 0
 * substitutes SUI_DEFAULT_NAV_ITEM_H. Width is explicit (DIPs); height
 * is explicit too because the nav list is most often the left column of
 * an outer HSTACK and the consumer pins it to the section content
 * height. To let flex stretch the nav vertically inside a vstack, pair
 * with `sui_last_grow(1)`. */
static int sui_nav_list(SuiControlHost* host, int id,
                        float w_dip, float h_dip,
                        float item_h_dip, int selected) {
    float s, w, h, ih, x, y, ow, oh;
    if (!host) return -1;
    s  = sui__decl_dpi_s(host);
    w  = w_dip  * s;
    h  = h_dip  * s;
    ih = (item_h_dip > 0.0f ? item_h_dip : SUI_DEFAULT_NAV_ITEM_H) * s;
    sui__decl_place(host, w, h, w, &x, &y, &ow, &oh);
    {
        int idx = sui_ctrl_add_nav_list(host, id, x, y, ow, oh, ih, selected);
        sui__decl_record_atom(host, idx);
        return idx;
    }
}

/* Horizontal tabs with an owned content panel below the tab strip. Items are
 * added separately via sui_ctrl_tabs_add_item(host, id, text). The wrapper
 * spans the current container width and takes an explicit total height; place
 * selected-tab content inside the returned control's content rect and set its
 * parent_id to the tab control id. */
static int sui_tabs(SuiControlHost* host, int id,
                    float h_dip, float tab_h_dip, int selected) {
    float s, h, th, x, y, ow, oh;
    if (!host) return -1;
    s  = sui__decl_dpi_s(host);
    h  = h_dip * s;
    th = (tab_h_dip > 0.0f ? tab_h_dip : SUI_DEFAULT_TAB_H) * s;
    sui__decl_place(host, 0.0f, h, 120.0f * s, &x, &y, &ow, &oh);
    {
        int idx = sui_ctrl_add_tabs(host, id, x, y, ow, oh, th, selected);
        sui__decl_record_atom(host, idx);
        return idx;
    }
}

/* ── Phase 2b flex helpers ─────────────────────────────────────────────────
 * Mutate the most-recently-added control's flex hints. They feed
 * sui__decl_distribute_hstack / _vstack at sui_end. To set hints on a
 * control that isn't the latest add, mutate by id:
 *   int id = sui_button(host, ID, L"OK", 0);
 *   host->controls[id].grow = 1;
 * Helpers are no-ops on empty hosts. */

static void sui_last_grow(SuiControlHost* host, int grow) {
    if (!host || host->control_count <= 0) return;
    host->controls[host->control_count - 1].grow = grow;
}

static void sui_last_shrink(SuiControlHost* host, int shrink) {
    if (!host || host->control_count <= 0) return;
    host->controls[host->control_count - 1].shrink = shrink;
}

static void sui_last_style_color(SuiControlHost* host, uint32_t prop, uint32_t color) {
    if (!host || host->control_count <= 0) return;
    sui_ctrl_apply_style_color(&host->controls[host->control_count - 1], prop, color);
}

static void sui_last_font_family(SuiControlHost* host, const wchar_t* family) {
    if (!host || host->control_count <= 0) return;
    sui_ctrl_apply_font_family(&host->controls[host->control_count - 1], family);
}

/* Pref size is in DIPs and DPI-scaled internally so values match the
 * declarative API's other size args (gap_dip, item_h_dip, etc.). 0 leaves
 * the field untouched relative to the field's prior value (record_atom
 * runs *before* this helper, so a 0 here just keeps the flag at 0). */
static void sui_last_pref(SuiControlHost* host, float w_dip, float h_dip) {
    SuiControl* c;
    float s;
    if (!host || host->control_count <= 0) return;
    s = sui__decl_dpi_s(host);
    c = &host->controls[host->control_count - 1];
    c->pref_w = w_dip * s;
    c->pref_h = h_dip * s;
}

static void sui_last_max(SuiControlHost* host, float w_dip, float h_dip) {
    SuiControl* c;
    float s;
    if (!host || host->control_count <= 0) return;
    s = sui__decl_dpi_s(host);
    c = &host->controls[host->control_count - 1];
    c->max_w = w_dip * s;
    c->max_h = h_dip * s;
}

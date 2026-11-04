/* sui_ctrl_tabs.h -- Horizontal tabs with a content panel below.
 * Included from slim_ui.h under #ifdef SUI_IMPLEMENTATION -- no include guard.
 */

static void sui_ctrl_tabs_adopt_transition(SuiControlHost* host, SuiControl* c);

static uint32_t sui_ctrl_tabs_indicator_color(SuiControlHost* host, const SuiControl* ctrl) {
    uint32_t fallback = (host && host->scheme) ? host->scheme->btn_default_border : 0x0078D4u;
    return sui_ctrl_style_color(ctrl, SUI_STYLE_ACCENT_COLOR, fallback);
}

static int sui_ctrl_add_tabs(SuiControlHost* host, int id, float x, float y, float w, float h,
                             float tab_height, int selected) {
    SuiTabItem* items;
    SuiControl* c;
    float s;
    int idx;
    items = (SuiTabItem*)sui_malloc(SUI_MAX_ITEMS * sizeof(SuiTabItem));
    if (!items) return -1;
    s = sui_ctrl_dpi_scale(host);
    idx = sui_ctrl_add(host, SUI_CTRL_TABS, id, x, y, w, h, NULL);
    if (idx < 0) { sui_free(items); return -1; }
    c = &host->controls[idx];
    c->d.tabs.items      = items;
    c->d.tabs.item_count = 0;
    c->d.tabs.selected   = selected < 0 ? 0 : selected;
    c->d.tabs.hover_item = -1;
    c->d.tabs.tab_height = tab_height > 0.0f ? tab_height : (SUI_DEFAULT_TAB_H * s);
    c->d.tabs.tab_gap    = SUI_DEFAULT_TAB_GAP * s;
    c->d.tabs.content_gap = SUI_DEFAULT_TAB_CONTENT_GAP * s;
    c->d.tabs.item_min_width = 48.0f * s;
    c->d.tabs.item_hpad = 6.0f * s;
    c->d.tabs.indicator_rest_w = 16.0f * s;
    c->d.tabs.indicator_x = 0.0f;
    c->d.tabs.indicator_w = 0.0f;
    c->d.tabs.indicator_from_x = 0.0f;
    c->d.tabs.indicator_from_w = 0.0f;
    c->d.tabs.indicator_to_x = 0.0f;
    c->d.tabs.indicator_to_w = 0.0f;
    c->d.tabs.indicator_anim_t = 1.0f;
    c->d.tabs.indicator_ready = 0;
    c->d.tabs.indicator_anim_active = 0;
    c->d.tabs.content_bitmap = NULL;
    c->d.tabs.content_bitmap_w = 0.0f;
    c->d.tabs.content_bitmap_h = 0.0f;
    c->d.tabs.content_anim_active = 0;
    c->d.tabs.content_anim_dir = 0;
    c->d.tabs.content_anim_vertical = 0;
    c->d.tabs.content_anim_from = -1;
    c->d.tabs.content_anim_to = -1;
    c->d.tabs.content_anim_t = 1.0f;
    c->d.tabs.content_anim_travel = SUI_TAB_CONTENT_SLIDE_DP * s;
    sui_ctrl_tabs_adopt_transition(host, c);
    return idx;
}

static void sui_ctrl_tabs_add_item(SuiControlHost* host, int ctrl_id, const wchar_t* text) {
    SuiControl* c = sui_ctrl_find(host, ctrl_id);
    SuiTabItem* item;
    if (!c || c->type != SUI_CTRL_TABS) return;
    if (c->d.tabs.item_count >= SUI_MAX_ITEMS) return;
    item = &c->d.tabs.items[c->d.tabs.item_count++];
    wcsncpy(item->text, text ? text : L"", 63);
    item->text[63] = L'\0';
    item->hover_line = 0.0f;
}

static void sui_ctrl_tabs_set_spacing(SuiControlHost* host, int ctrl_id, float tab_gap, float content_gap) {
    SuiControl* c = sui_ctrl_find(host, ctrl_id);
    if (!c || c->type != SUI_CTRL_TABS) return;
    c->d.tabs.tab_gap = tab_gap > 0.0f ? tab_gap : 0.0f;
    c->d.tabs.content_gap = content_gap > 0.0f ? content_gap : 0.0f;
}

static void sui_ctrl_tabs_set_item_metrics(SuiControlHost* host, int ctrl_id,
                                           float item_min_width, float item_hpad) {
    SuiControl* c = sui_ctrl_find(host, ctrl_id);
    if (!c || c->type != SUI_CTRL_TABS) return;
    if (item_min_width > 0.0f) c->d.tabs.item_min_width = item_min_width;
    if (item_hpad > 0.0f) c->d.tabs.item_hpad = item_hpad;
}

static void sui_ctrl_tabs_set_indicator_width(SuiControlHost* host, int ctrl_id,
                                              float indicator_width) {
    SuiControl* c = sui_ctrl_find(host, ctrl_id);
    if (!c || c->type != SUI_CTRL_TABS) return;
    if (indicator_width > 0.0f) c->d.tabs.indicator_rest_w = indicator_width;
}

static int sui_ctrl_tabs_content_rect(const SuiControl* c, SuiRect* out) {
    float th, gap;
    if (!c || c->type != SUI_CTRL_TABS || !out) return 0;
    th = c->d.tabs.tab_height;
    gap = c->d.tabs.content_gap;
    if (th < 0.0f) th = 0.0f;
    if (th > c->h) th = c->h;
    if (gap < 0.0f) gap = 0.0f;
    out->left = c->x;
    out->top = c->y + th + gap;
    out->right = c->x + c->w;
    out->bottom = c->y + c->h;
    if (out->top > out->bottom) out->top = out->bottom;
    return 1;
}

static float sui_ctrl_tabs_content_radius(SuiControlHost* host, const SuiControl* c) {
    (void)c;
    return 4.0f * sui_ctrl_dpi_scale(host);
}

static int sui_ctrl_tabs_push_content_clip(SuiControlHost* host, SuiControl* tabs) {
    SuiRect content;
    float w, h;
    if (!host || !host->renderer || !tabs || tabs->type != SUI_CTRL_TABS) return 0;
    if (!sui_ctrl_tabs_content_rect(tabs, &content)) return 0;
    w = content.right - content.left;
    h = content.bottom - content.top;
    if (w <= 0.0f || h <= 0.0f) return 0;
    sui_renderer_push_clip(host->renderer, content.left, content.top, w, h);
    return 1;
}

static void sui_ctrl_tabs_pop_content_clip_ex(SuiControlHost* host, SuiControl* tabs,
                                              uint32_t outside_color, uint32_t border_color,
                                              float radius, float stroke) {
    SuiRect content;
    float w, h;
    if (!host || !host->renderer) return;
    sui_renderer_pop_clip(host->renderer);
    if (!tabs || tabs->type != SUI_CTRL_TABS) return;
    if (!sui_ctrl_tabs_content_rect(tabs, &content)) return;
    w = content.right - content.left;
    h = content.bottom - content.top;
    if (w <= 0.0f || h <= 0.0f) return;
    sui_renderer_mask_outside_rounded_rect(host->renderer, content.left, content.top,
                                           w, h, radius, outside_color);
    sui_renderer_draw_rounded_rect(host->renderer, content.left, content.top,
                                   w, h, radius, border_color, stroke);
}

static void sui_ctrl_tabs_pop_content_clip(SuiControlHost* host, SuiControl* tabs) {
    const SuiColorScheme* sc;
    uint32_t outside_color, border_color;
    if (!host || !tabs) return;
    sc = host->scheme;
    outside_color = sc ? sc->background : 0xFFFFFF;
    border_color = sui_ctrl_style_color(tabs, SUI_STYLE_BORDER_COLOR,
                                        sc ? sc->btn_area_border : 0xDFDFDF);
    sui_ctrl_tabs_pop_content_clip_ex(host, tabs, outside_color, border_color,
                                      sui_ctrl_tabs_content_radius(host, tabs), 1.0f);
}

static float sui_ctrl_tabs_text_width(SuiControlHost* host, SuiControl* c, int item_index) {
    float font_size;
    const wchar_t* family;
    if (!host || !c || c->type != SUI_CTRL_TABS) return 0.0f;
    if (!host->renderer || !c->d.tabs.items) return 0.0f;
    if (item_index < 0 || item_index >= c->d.tabs.item_count) return 0.0f;
    if (!c->d.tabs.items[item_index].text[0]) return 0.0f;
    font_size = c->font_size > 0.0f ? c->font_size : host->renderer->font_size;
    family = sui_ctrl_style_font_family(c);
    if (c->state & SUI_CTRL_BOLD) {
        return sui_renderer_measure_text_width_ex(host->renderer, c->d.tabs.items[item_index].text,
                                                  1, font_size, family);
    } else if (c->state & SUI_CTRL_SEMIBOLD) {
        return sui_renderer_measure_text_width_weight_ex(host->renderer, c->d.tabs.items[item_index].text,
                                                         600, font_size, family);
    } else if (c->state & SUI_CTRL_MEDIUM) {
        return sui_renderer_measure_text_width_weight_ex(host->renderer, c->d.tabs.items[item_index].text,
                                                         500, font_size, family);
    }
    return sui_renderer_measure_text_width_ex(host->renderer, c->d.tabs.items[item_index].text,
                                              0, font_size, family);
}

static float sui_ctrl_tabs_item_width(SuiControlHost* host, SuiControl* c, int item_index) {
    float s = sui_ctrl_dpi_scale(host);
    float min_w = c->d.tabs.item_min_width > 0.0f ? c->d.tabs.item_min_width : 48.0f * s;
    float pad_w = (c->d.tabs.item_hpad > 0.0f ? c->d.tabs.item_hpad : 6.0f * s) * 2.0f;
    float tw = sui_ctrl_tabs_text_width(host, c, item_index);
    tw += pad_w;
    return tw > min_w ? tw : min_w;
}

static float sui_ctrl_tabs_total_natural_width(SuiControlHost* host, SuiControl* c) {
    float total = 0.0f;
    float gap = c->d.tabs.tab_gap;
    int i;
    if (gap < 0.0f) gap = 0.0f;
    for (i = 0; i < c->d.tabs.item_count; i++)
        total += sui_ctrl_tabs_item_width(host, c, i);
    if (c->d.tabs.item_count > 1)
        total += gap * (float)(c->d.tabs.item_count - 1);
    return total;
}

static void sui_ctrl_tabs_item_rect(SuiControlHost* host, SuiControl* c, int item_index,
                                    float* out_x, float* out_w) {
    int count = c->d.tabs.item_count;
    float gap = c->d.tabs.tab_gap;
    float total = sui_ctrl_tabs_total_natural_width(host, c);
    float x = c->x;
    int i;
    if (gap < 0.0f) gap = 0.0f;
    if (count <= 0) {
        *out_x = c->x;
        *out_w = 0.0f;
        return;
    }
    if (total > c->w && c->w > 0.0f) {
        float gaps_w = gap * (float)(count - 1);
        float item_w = (c->w - gaps_w) / (float)count;
        if (item_w < 0.0f) { item_w = c->w / (float)count; gap = 0.0f; }
        *out_w = item_w;
        *out_x = c->x + (item_w + gap) * (float)item_index;
        return;
    }
    /* Keep the strip centered when the control has spare width; the content
     * panel still spans the full control width below the tabs. */
    if (c->w > total)
        x += (c->w - total) * 0.5f;
    for (i = 0; i < item_index; i++)
        x += sui_ctrl_tabs_item_width(host, c, i) + gap;
    *out_x = x;
    *out_w = sui_ctrl_tabs_item_width(host, c, item_index);
}

static int sui_ctrl_tabs_item_at(SuiControlHost* host, SuiControl* tabs, float mx, float my) {
    int i;
    if (!tabs || tabs->type != SUI_CTRL_TABS) return -1;
    if (my < tabs->y || my >= tabs->y + tabs->d.tabs.tab_height) return -1;
    if (mx < tabs->x || mx >= tabs->x + tabs->w) return -1;
    for (i = 0; i < tabs->d.tabs.item_count; i++) {
        float ix, iw;
        sui_ctrl_tabs_item_rect(host, tabs, i, &ix, &iw);
        if (mx >= ix && mx < ix + iw) return i;
    }
    return -1;
}

static float sui__tabs_absf(float v) {
    return v < 0.0f ? -v : v;
}

static float sui__tabs_clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static float sui__tabs_lerpf(float a, float b, float t) {
    return a + (b - a) * t;
}

static float sui__tabs_cubic_bezier(float t, float x1, float y1, float x2, float y2) {
    float lo, hi, u;
    int i;
    t = sui__tabs_clampf(t, 0.0f, 1.0f);
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    lo = 0.0f;
    hi = 1.0f;
    u = 0.5f;
    for (i = 0; i < 10; i++) {
        float inv, x;
        u = (lo + hi) * 0.5f;
        inv = 1.0f - u;
        x = 3.0f * inv * inv * u * x1 +
            3.0f * inv * u * u * x2 +
            u * u * u;
        if (x < t) lo = u;
        else hi = u;
    }
    u = (lo + hi) * 0.5f;
    {
        float inv = 1.0f - u;
        return 3.0f * inv * inv * u * y1 +
               3.0f * inv * u * u * y2 +
               u * u * u;
    }
}

static void sui_ctrl_tabs_indicator_clear(SuiControl* ctrl) {
    ctrl->d.tabs.indicator_x = 0.0f;
    ctrl->d.tabs.indicator_w = 0.0f;
    ctrl->d.tabs.indicator_from_x = 0.0f;
    ctrl->d.tabs.indicator_from_w = 0.0f;
    ctrl->d.tabs.indicator_to_x = 0.0f;
    ctrl->d.tabs.indicator_to_w = 0.0f;
    ctrl->d.tabs.indicator_anim_t = 1.0f;
    ctrl->d.tabs.indicator_ready = 0;
    ctrl->d.tabs.indicator_anim_active = 0;
}

static void sui_ctrl_tabs_indicator_set_final(SuiControl* ctrl, float x, float w) {
    ctrl->d.tabs.indicator_x = x;
    ctrl->d.tabs.indicator_w = w;
    ctrl->d.tabs.indicator_from_x = x;
    ctrl->d.tabs.indicator_from_w = w;
    ctrl->d.tabs.indicator_to_x = x;
    ctrl->d.tabs.indicator_to_w = w;
    ctrl->d.tabs.indicator_anim_t = 1.0f;
    ctrl->d.tabs.indicator_ready = 1;
    ctrl->d.tabs.indicator_anim_active = 0;
}

static void sui_ctrl_tabs_release_bitmap(void** bitmap) {
    typedef ULONG (STDMETHODCALLTYPE *SuiReleaseFn)(void*);
    if (bitmap && *bitmap) {
        ((SuiReleaseFn)(*(void***)*bitmap)[2])(*bitmap);
        *bitmap = NULL;
    }
}

static void sui_ctrl_tabs_release_content_bitmap(SuiControl* c) {
    if (!c || c->type != SUI_CTRL_TABS) return;
    sui_ctrl_tabs_release_bitmap(&c->d.tabs.content_bitmap);
    c->d.tabs.content_bitmap_w = 0.0f;
    c->d.tabs.content_bitmap_h = 0.0f;
}

static void sui_ctrl_tabs_release_host_transitions(SuiControlHost* host) {
    int i;
    if (!host) return;
    for (i = 0; i < SUI_MAX_TAB_TRANSITIONS; i++) {
        if (host->tab_transitions[i].in_use)
            sui_ctrl_tabs_release_bitmap(&host->tab_transitions[i].bitmap);
        host->tab_transitions[i].in_use = 0;
        host->tab_transitions[i].ctrl_id = 0;
        host->tab_transitions[i].bitmap_w = 0.0f;
        host->tab_transitions[i].bitmap_h = 0.0f;
        host->tab_transitions[i].content_active = 0;
        host->tab_transitions[i].dir = 0;
        host->tab_transitions[i].vertical = 0;
        host->tab_transitions[i].from = -1;
        host->tab_transitions[i].to = -1;
        host->tab_transitions[i].t = 0.0f;
        host->tab_transitions[i].travel = 0.0f;
        host->tab_transitions[i].indicator_ready = 0;
        host->tab_transitions[i].indicator_anim_active = 0;
        host->tab_transitions[i].indicator_x = 0.0f;
        host->tab_transitions[i].indicator_w = 0.0f;
        host->tab_transitions[i].indicator_from_x = 0.0f;
        host->tab_transitions[i].indicator_from_w = 0.0f;
        host->tab_transitions[i].indicator_to_x = 0.0f;
        host->tab_transitions[i].indicator_to_w = 0.0f;
        host->tab_transitions[i].indicator_anim_t = 1.0f;
    }
}

static void sui_ctrl_tabs_adopt_transition(SuiControlHost* host, SuiControl* c) {
    int i;
    if (!host || !c || c->type != SUI_CTRL_TABS || c->id < 0) return;
    for (i = 0; i < SUI_MAX_TAB_TRANSITIONS; i++) {
        SuiTabTransition* tr = &host->tab_transitions[i];
        if (!tr->in_use || tr->ctrl_id != c->id) continue;
        if (tr->content_active) {
            sui_ctrl_tabs_release_content_bitmap(c);
            c->d.tabs.content_bitmap = tr->bitmap;
            c->d.tabs.content_bitmap_w = tr->bitmap_w;
            c->d.tabs.content_bitmap_h = tr->bitmap_h;
            c->d.tabs.content_anim_dir = tr->dir;
            c->d.tabs.content_anim_vertical = tr->vertical;
            c->d.tabs.content_anim_from = tr->from;
            c->d.tabs.content_anim_to = tr->to;
            c->d.tabs.content_anim_t = tr->t;
            c->d.tabs.content_anim_travel = tr->travel;
            c->d.tabs.content_anim_active = 1;
        }
        if (tr->indicator_ready) {
            c->d.tabs.indicator_x = tr->indicator_x;
            c->d.tabs.indicator_w = tr->indicator_w;
            c->d.tabs.indicator_from_x = tr->indicator_from_x;
            c->d.tabs.indicator_from_w = tr->indicator_from_w;
            c->d.tabs.indicator_to_x = tr->indicator_to_x;
            c->d.tabs.indicator_to_w = tr->indicator_to_w;
            c->d.tabs.indicator_anim_t = tr->indicator_anim_t;
            c->d.tabs.indicator_ready = 1;
            c->d.tabs.indicator_anim_active = tr->indicator_anim_active;
        }
        tr->bitmap = NULL;
        tr->in_use = 0;
        tr->ctrl_id = 0;
        tr->bitmap_w = 0.0f;
        tr->bitmap_h = 0.0f;
        tr->content_active = 0;
        tr->dir = 0;
        tr->vertical = 0;
        tr->from = -1;
        tr->to = -1;
        tr->t = 0.0f;
        tr->travel = 0.0f;
        tr->indicator_ready = 0;
        tr->indicator_anim_active = 0;
        tr->indicator_x = 0.0f;
        tr->indicator_w = 0.0f;
        tr->indicator_from_x = 0.0f;
        tr->indicator_from_w = 0.0f;
        tr->indicator_to_x = 0.0f;
        tr->indicator_to_w = 0.0f;
        tr->indicator_anim_t = 1.0f;
        return;
    }
}

static void sui_ctrl_tabs_stash_transition(SuiControlHost* host, SuiControl* c) {
    int i, slot = -1;
    int content_active, indicator_ready;
    if (!host || !c || c->type != SUI_CTRL_TABS || c->id < 0) return;
    content_active = c->d.tabs.content_anim_active && c->d.tabs.content_anim_t < 1.0f;
    indicator_ready = c->d.tabs.indicator_ready;
    if (!content_active && !indicator_ready) return;
    for (i = 0; i < SUI_MAX_TAB_TRANSITIONS; i++) {
        if (host->tab_transitions[i].in_use && host->tab_transitions[i].ctrl_id == c->id) {
            slot = i;
            break;
        }
        if (slot < 0 && !host->tab_transitions[i].in_use)
            slot = i;
    }
    if (slot < 0) return;
    {
        SuiTabTransition* tr = &host->tab_transitions[slot];
        if (tr->in_use)
            sui_ctrl_tabs_release_bitmap(&tr->bitmap);
        tr->in_use = 1;
        tr->ctrl_id = c->id;
        tr->content_active = content_active;
        if (content_active) {
            tr->bitmap = c->d.tabs.content_bitmap;
            tr->bitmap_w = c->d.tabs.content_bitmap_w;
            tr->bitmap_h = c->d.tabs.content_bitmap_h;
            tr->dir = c->d.tabs.content_anim_dir;
            tr->vertical = c->d.tabs.content_anim_vertical;
            tr->from = c->d.tabs.content_anim_from;
            tr->to = c->d.tabs.content_anim_to;
            tr->t = c->d.tabs.content_anim_t;
            tr->travel = c->d.tabs.content_anim_travel;
            c->d.tabs.content_bitmap = NULL;
            c->d.tabs.content_bitmap_w = 0.0f;
            c->d.tabs.content_bitmap_h = 0.0f;
        } else {
            tr->bitmap = NULL;
            tr->bitmap_w = 0.0f;
            tr->bitmap_h = 0.0f;
            tr->dir = 0;
            tr->vertical = 0;
            tr->from = -1;
            tr->to = -1;
            tr->t = 1.0f;
            tr->travel = 0.0f;
        }
        tr->indicator_ready = indicator_ready;
        tr->indicator_anim_active = c->d.tabs.indicator_anim_active;
        tr->indicator_x = c->d.tabs.indicator_x;
        tr->indicator_w = c->d.tabs.indicator_w;
        tr->indicator_from_x = c->d.tabs.indicator_from_x;
        tr->indicator_from_w = c->d.tabs.indicator_from_w;
        tr->indicator_to_x = c->d.tabs.indicator_to_x;
        tr->indicator_to_w = c->d.tabs.indicator_to_w;
        tr->indicator_anim_t = c->d.tabs.indicator_anim_t;
    }
}

static int sui_ctrl_tabs_content_tick(SuiControl* ctrl, float step) {
    if (!ctrl || ctrl->type != SUI_CTRL_TABS || !ctrl->d.tabs.content_anim_active) return 0;
    if (step < 0.0f) step = 0.0f;
    if (step > 1.0f) step = 1.0f;
    ctrl->d.tabs.content_anim_t += step;
    if (ctrl->d.tabs.content_anim_t >= 1.0f) {
        ctrl->d.tabs.content_anim_t = 1.0f;
        ctrl->d.tabs.content_anim_active = 0;
        ctrl->d.tabs.content_anim_dir = 0;
        ctrl->d.tabs.content_anim_vertical = 0;
        ctrl->d.tabs.content_anim_from = -1;
        ctrl->d.tabs.content_anim_to = -1;
        sui_ctrl_tabs_release_content_bitmap(ctrl);
        return 1;
    }
    return 1;
}

static int sui_ctrl_tabs_host_has_active_transition(SuiControlHost* host) {
    int i;
    if (!host) return 0;
    for (i = 0; i < host->control_count; i++) {
        SuiControl* c = &host->controls[i];
        if (c->type == SUI_CTRL_TABS &&
            ((c->d.tabs.content_anim_active && c->d.tabs.content_anim_t < 1.0f) ||
             c->d.tabs.indicator_anim_active))
            return 1;
    }
    for (i = 0; i < SUI_MAX_TAB_TRANSITIONS; i++) {
        if (host->tab_transitions[i].in_use &&
            ((host->tab_transitions[i].content_active && host->tab_transitions[i].t < 1.0f) ||
             host->tab_transitions[i].indicator_anim_active))
            return 1;
    }
    return 0;
}

static float sui_ctrl_tabs_content_progress(SuiControl* ctrl) {
    float t;
    if (!ctrl || ctrl->type != SUI_CTRL_TABS) return 1.0f;
    t = ctrl->d.tabs.content_anim_t;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    // The vertical transition is linear over its 160 ms, like a macOS settings pane's.
    if (ctrl->d.tabs.content_anim_vertical) return t;
    return sui__tabs_cubic_bezier(t, 0.0f, 0.0f, 0.0f, 1.0f);
}

static float sui_ctrl_tabs_content_new_offset(SuiControl* ctrl) {
    SuiRect content;
    float w, p, travel;
    if (!ctrl || ctrl->type != SUI_CTRL_TABS || !ctrl->d.tabs.content_anim_active) return 0.0f;
    if (ctrl->d.tabs.content_anim_vertical) return 0.0f;
    if (ctrl->d.tabs.content_anim_dir == 0) return 0.0f;
    if (!sui_ctrl_tabs_content_rect(ctrl, &content)) return 0.0f;
    w = content.right - content.left;
    if (w <= 0.0f) return 0.0f;
    travel = ctrl->d.tabs.content_anim_travel > 0.0f
        ? ctrl->d.tabs.content_anim_travel
        : SUI_TAB_CONTENT_SLIDE_DP;
    if (travel > w * 0.18f) travel = w * 0.18f;
    p = sui_ctrl_tabs_content_progress(ctrl);
    return (float)ctrl->d.tabs.content_anim_dir * travel * (1.0f - p);
}

static float sui_ctrl_tabs_content_old_offset(SuiControl* ctrl) {
    SuiRect content;
    float w, p, travel;
    if (!ctrl || ctrl->type != SUI_CTRL_TABS || !ctrl->d.tabs.content_anim_active) return 0.0f;
    if (ctrl->d.tabs.content_anim_vertical) return 0.0f;
    if (ctrl->d.tabs.content_anim_dir == 0) return 0.0f;
    if (!sui_ctrl_tabs_content_rect(ctrl, &content)) return 0.0f;
    w = content.right - content.left;
    if (w <= 0.0f) return 0.0f;
    travel = ctrl->d.tabs.content_anim_travel > 0.0f
        ? ctrl->d.tabs.content_anim_travel
        : SUI_TAB_CONTENT_SLIDE_DP;
    if (travel > w * 0.18f) travel = w * 0.18f;
    p = sui_ctrl_tabs_content_progress(ctrl);
    return (float)(-ctrl->d.tabs.content_anim_dir) * travel * p;
}

static float sui_ctrl_tabs_content_new_offset_y(SuiControl* ctrl) {
    SuiRect content;
    float h, p, travel;
    if (!ctrl || ctrl->type != SUI_CTRL_TABS || !ctrl->d.tabs.content_anim_active) return 0.0f;
    if (!ctrl->d.tabs.content_anim_vertical || ctrl->d.tabs.content_anim_dir == 0) return 0.0f;
    if (!sui_ctrl_tabs_content_rect(ctrl, &content)) return 0.0f;
    h = content.bottom - content.top;
    if (h <= 0.0f) return 0.0f;
    travel = ctrl->d.tabs.content_anim_travel > 0.0f
        ? ctrl->d.tabs.content_anim_travel
        : SUI_TAB_CONTENT_SLIDE_DP;
    if (travel > h * 0.18f) travel = h * 0.18f;
    p = sui_ctrl_tabs_content_progress(ctrl);
    return (float)ctrl->d.tabs.content_anim_dir * travel * (1.0f - p);
}

static float sui_ctrl_tabs_content_old_offset_y(SuiControl* ctrl) {
    SuiRect content;
    float h, p, travel;
    if (!ctrl || ctrl->type != SUI_CTRL_TABS || !ctrl->d.tabs.content_anim_active) return 0.0f;
    if (!ctrl->d.tabs.content_anim_vertical || ctrl->d.tabs.content_anim_dir == 0) return 0.0f;
    if (!sui_ctrl_tabs_content_rect(ctrl, &content)) return 0.0f;
    h = content.bottom - content.top;
    if (h <= 0.0f) return 0.0f;
    travel = ctrl->d.tabs.content_anim_travel > 0.0f
        ? ctrl->d.tabs.content_anim_travel
        : SUI_TAB_CONTENT_SLIDE_DP;
    if (travel > h * 0.18f) travel = h * 0.18f;
    p = sui_ctrl_tabs_content_progress(ctrl);
    return (float)(-ctrl->d.tabs.content_anim_dir) * travel * p;
}

static float sui_ctrl_tabs_content_new_opacity(SuiControl* ctrl) {
    float t;
    if (!ctrl || ctrl->type != SUI_CTRL_TABS || !ctrl->d.tabs.content_anim_active) return 1.0f;
    if (ctrl->d.tabs.content_anim_vertical) {
        // Keep incoming section content subdued through most of the slide,
        // then resolve it to full opacity exactly at the endpoint.
        t = ctrl->d.tabs.content_anim_t;
        if (t <= 0.0f) return 0.0f;
        if (t >= 1.0f) return 1.0f;
        return t * t;
    }
    return sui_ctrl_tabs_content_progress(ctrl);
}

static float sui_ctrl_tabs_content_old_opacity(SuiControl* ctrl) {
    float t;
    if (!ctrl || ctrl->type != SUI_CTRL_TABS || !ctrl->d.tabs.content_anim_active) return 0.0f;
    if (ctrl->d.tabs.content_anim_vertical) {
        // Keep the 160 ms motion, but clear the outgoing
        // pane a little earlier so its text does not linger behind the new pane.
        t = ctrl->d.tabs.content_anim_t / 0.70f;
        if (t >= 1.0f) return 0.0f;
        if (t <= 0.0f) return 1.0f;
        return 1.0f - t;
    }
    return 1.0f - sui_ctrl_tabs_content_progress(ctrl);
}

static int sui_ctrl_tabs_transition_includes(SuiControl* ctrl, int tab_index) {
    if (!ctrl || ctrl->type != SUI_CTRL_TABS) return 0;
    if (!ctrl->d.tabs.content_anim_active || ctrl->d.tabs.content_anim_t >= 1.0f) return 0;
    return tab_index == ctrl->d.tabs.content_anim_from ||
           tab_index == ctrl->d.tabs.content_anim_to;
}

static float sui_ctrl_tabs_content_offset_for(SuiControl* ctrl, int tab_index) {
    if (!sui_ctrl_tabs_transition_includes(ctrl, tab_index)) return 0.0f;
    if (tab_index == ctrl->d.tabs.content_anim_from)
        return sui_ctrl_tabs_content_old_offset(ctrl);
    if (tab_index == ctrl->d.tabs.content_anim_to)
        return sui_ctrl_tabs_content_new_offset(ctrl);
    return 0.0f;
}

static float sui_ctrl_tabs_content_opacity_for(SuiControl* ctrl, int tab_index) {
    if (!sui_ctrl_tabs_transition_includes(ctrl, tab_index)) return 1.0f;
    if (tab_index == ctrl->d.tabs.content_anim_from)
        return sui_ctrl_tabs_content_old_opacity(ctrl);
    if (tab_index == ctrl->d.tabs.content_anim_to)
        return sui_ctrl_tabs_content_new_opacity(ctrl);
    return 1.0f;
}

static int sui_ctrl_tabs_indicator_target_for_item(SuiControlHost* host, SuiControl* ctrl,
                                                   int item_index, float* out_x, float* out_w) {
    float ix, iw;
    float s, text_pad, text_box_w, text_w, line_w, center_x;
    if (!host || !ctrl || ctrl->type != SUI_CTRL_TABS || !out_x || !out_w) return 0;
    if (ctrl->d.tabs.item_count <= 0) return 0;
    if (item_index < 0 || item_index >= ctrl->d.tabs.item_count) return 0;

    sui_ctrl_tabs_item_rect(host, ctrl, item_index, &ix, &iw);
    if (iw <= 0.0f) return 0;

    s = sui_ctrl_dpi_scale(host);
    text_pad = ctrl->d.tabs.item_hpad > 0.0f ? ctrl->d.tabs.item_hpad : 6.0f * s;
    text_box_w = iw - text_pad * 2.0f;
    if (text_box_w < 0.0f) {
        text_pad = 0.0f;
        text_box_w = iw;
    }
    text_w = sui_ctrl_tabs_text_width(host, ctrl, item_index);
    if (text_w <= 0.0f || text_w > text_box_w) text_w = text_box_w;
    line_w = ctrl->d.tabs.indicator_rest_w > 0.0f
        ? ctrl->d.tabs.indicator_rest_w
        : 16.0f * s;
    if (line_w > text_box_w) line_w = text_box_w;
    if (line_w < 0.0f) line_w = 0.0f;

    center_x = ix + text_pad + text_w * 0.5f;
    *out_x = center_x - line_w * 0.5f;
    if (*out_x < ix) *out_x = ix;
    if (*out_x + line_w > ix + iw) *out_x = ix + iw - line_w;
    *out_w = line_w;
    return 1;
}

static int sui_ctrl_tabs_indicator_target(SuiControlHost* host, SuiControl* ctrl,
                                          float* out_x, float* out_w) {
    if (!ctrl || ctrl->type != SUI_CTRL_TABS) return 0;
    return sui_ctrl_tabs_indicator_target_for_item(host, ctrl, ctrl->d.tabs.selected,
                                                  out_x, out_w);
}

static void sui_ctrl_tabs_sync_indicator(SuiControlHost* host, SuiControl* ctrl, int snap) {
    float tx, tw;
    if (!ctrl || ctrl->type != SUI_CTRL_TABS) return;
    if (!sui_ctrl_tabs_indicator_target(host, ctrl, &tx, &tw)) {
        sui_ctrl_tabs_indicator_clear(ctrl);
        return;
    }
    if (snap || !ctrl->d.tabs.indicator_ready) {
        sui_ctrl_tabs_indicator_set_final(ctrl, tx, tw);
    } else if (!ctrl->d.tabs.indicator_anim_active &&
               (sui__tabs_absf(tx - ctrl->d.tabs.indicator_x) > 0.1f ||
                sui__tabs_absf(tw - ctrl->d.tabs.indicator_w) > 0.1f)) {
        sui_ctrl_tabs_indicator_set_final(ctrl, tx, tw);
    }
}

static void sui_ctrl_tabs_start_indicator_transition(SuiControlHost* host, SuiControl* ctrl) {
    float tx, tw;
    if (!ctrl || ctrl->type != SUI_CTRL_TABS) return;
    if (!sui_ctrl_tabs_indicator_target(host, ctrl, &tx, &tw)) {
        sui_ctrl_tabs_indicator_clear(ctrl);
        return;
    }
    if (!ctrl->d.tabs.indicator_ready) {
        sui_ctrl_tabs_indicator_set_final(ctrl, tx, tw);
        return;
    }
    if (sui__tabs_absf(tx - ctrl->d.tabs.indicator_x) <= 0.1f &&
        sui__tabs_absf(tw - ctrl->d.tabs.indicator_w) <= 0.1f) {
        sui_ctrl_tabs_indicator_set_final(ctrl, tx, tw);
        return;
    }
    ctrl->d.tabs.indicator_from_x = ctrl->d.tabs.indicator_x;
    ctrl->d.tabs.indicator_from_w = ctrl->d.tabs.indicator_w;
    ctrl->d.tabs.indicator_to_x = tx;
    ctrl->d.tabs.indicator_to_w = tw;
    ctrl->d.tabs.indicator_anim_t = 0.0f;
    ctrl->d.tabs.indicator_ready = 1;
    ctrl->d.tabs.indicator_anim_active = 1;
}

static int sui_ctrl_tabs_indicator_tick(SuiControlHost* host, SuiControl* ctrl, float step) {
    float tx, tw;
    int target_changed;
    if (!ctrl || ctrl->type != SUI_CTRL_TABS) return 0;
    if (!sui_ctrl_tabs_indicator_target(host, ctrl, &tx, &tw)) {
        sui_ctrl_tabs_indicator_clear(ctrl);
        return 0;
    }
    if (!ctrl->d.tabs.indicator_ready) {
        sui_ctrl_tabs_indicator_set_final(ctrl, tx, tw);
        return 0;
    }
    if (step < 0.0f) step = 0.0f;
    if (step > 1.0f) step = 1.0f;

    target_changed =
        sui__tabs_absf(tx - ctrl->d.tabs.indicator_to_x) > 0.1f ||
        sui__tabs_absf(tw - ctrl->d.tabs.indicator_to_w) > 0.1f;
    if (target_changed || !ctrl->d.tabs.indicator_anim_active) {
        if (sui__tabs_absf(tx - ctrl->d.tabs.indicator_x) <= 0.1f &&
            sui__tabs_absf(tw - ctrl->d.tabs.indicator_w) <= 0.1f) {
            sui_ctrl_tabs_indicator_set_final(ctrl, tx, tw);
            return 0;
        }
        ctrl->d.tabs.indicator_from_x = ctrl->d.tabs.indicator_x;
        ctrl->d.tabs.indicator_from_w = ctrl->d.tabs.indicator_w;
        ctrl->d.tabs.indicator_to_x = tx;
        ctrl->d.tabs.indicator_to_w = tw;
        ctrl->d.tabs.indicator_anim_t = 0.0f;
        ctrl->d.tabs.indicator_anim_active = 1;
    }

    if (ctrl->d.tabs.indicator_anim_active) {
        float from_l = ctrl->d.tabs.indicator_from_x;
        float from_r = from_l + ctrl->d.tabs.indicator_from_w;
        float to_l = ctrl->d.tabs.indicator_to_x;
        float to_r = to_l + ctrl->d.tabs.indicator_to_w;
        float phase, left, right;

        ctrl->d.tabs.indicator_anim_t += step;
        if (ctrl->d.tabs.indicator_anim_t >= 1.0f) {
            sui_ctrl_tabs_indicator_set_final(ctrl, tx, tw);
            return 1;
        }

        if (ctrl->d.tabs.indicator_anim_t <= 0.33333334f) {
            phase = sui__tabs_cubic_bezier(ctrl->d.tabs.indicator_anim_t / 0.33333334f,
                                           0.9f, 0.1f, 1.0f, 0.2f);
            if (to_l >= from_l) {
                left = from_l;
                right = sui__tabs_lerpf(from_r, to_r, phase);
            } else {
                left = sui__tabs_lerpf(from_l, to_l, phase);
                right = from_r;
            }
        } else {
            phase = sui__tabs_cubic_bezier((ctrl->d.tabs.indicator_anim_t - 0.33333334f) / 0.66666666f,
                                           0.1f, 0.9f, 0.2f, 1.0f);
            if (to_l >= from_l) {
                left = sui__tabs_lerpf(from_l, to_l, phase);
                right = to_r;
            } else {
                left = to_l;
                right = sui__tabs_lerpf(from_r, to_r, phase);
            }
        }
        if (right < left + 1.0f) right = left + 1.0f;
        ctrl->d.tabs.indicator_x = left;
        ctrl->d.tabs.indicator_w = right - left;
        return 1;
    }

    sui_ctrl_tabs_indicator_set_final(ctrl, tx, tw);
    return 0;
}

static float sui_ctrl_tabs_item_hover_line(const SuiControl* ctrl, int item_index,
                                           int top_align_text) {
    float hover_line;
    (void)top_align_text;
    if (!ctrl) return 0.0f;
    if (!ctrl->d.tabs.items || item_index < 0 || item_index >= ctrl->d.tabs.item_count)
        return 0.0f;
    hover_line = ctrl->d.tabs.items[item_index].hover_line;
    if (hover_line <= 0.001f) return 0.0f;
    if (hover_line > 1.0f) return 1.0f;
    return hover_line;
}

static int sui_ctrl_tabs_text_hover_tick(SuiControl* ctrl, float step) {
    int i, animating = 0;
    if (!ctrl || ctrl->type != SUI_CTRL_TABS || !ctrl->d.tabs.items) return 0;
    if (step < 0.0f) step = 0.0f;
    if (step > 1.0f) step = 1.0f;
    for (i = 0; i < ctrl->d.tabs.item_count; i++) {
        SuiTabItem* item = &ctrl->d.tabs.items[i];
        float target = (i == ctrl->d.tabs.hover_item) ? 1.0f : 0.0f;
        float hover_line = item->hover_line;
        float delta;
        float amount;
        if (hover_line < 0.0f || hover_line > 1.0f) hover_line = 0.0f;
        delta = target - hover_line;
        if (delta > -0.001f && delta < 0.001f) {
            item->hover_line = target;
            continue;
        }
        amount = 0.30f * step;
        if (delta > 0.0f) {
            hover_line += amount;
            if (hover_line > target) hover_line = target;
        } else {
            hover_line -= amount;
            if (hover_line < target) hover_line = target;
        }
        item->hover_line = hover_line;
        animating = 1;
    }
    return animating;
}

static float sui_ctrl_tabs_indicator_line_h(SuiControlHost* host) {
    float s = sui_ctrl_dpi_scale(host);
    float line_h = 3.0f * s;
    return line_h < 2.0f ? 2.0f : line_h;
}

static float sui_ctrl_tabs_indicator_line_y(SuiControlHost* host, SuiControl* ctrl,
                                            float tab_h, int top_align_text) {
    float s = sui_ctrl_dpi_scale(host);
    float line_h = sui_ctrl_tabs_indicator_line_h(host);
    float line_y = ctrl->y + tab_h - line_h;
    if (top_align_text) {
        SuiRenderer* r = host->renderer;
        float font_size = ctrl->font_size > 0.0f ? ctrl->font_size : (r ? r->font_size : 0.0f);
        float closer_y = ctrl->y + (4.0f * s) + (font_size * (4.0f / 3.0f) * s) + (12.0f * s);
        if (closer_y < line_y) line_y = closer_y;
    }
    return line_y;
}

static int sui_ctrl_tabs_is_dark_color(uint32_t color) {
    uint32_t rgb = color & 0xFFFFFFu;
    int r = (int)((rgb >> 16) & 0xFFu);
    int g = (int)((rgb >> 8) & 0xFFu);
    int b = (int)(rgb & 0xFFu);
    return (r * 299 + g * 587 + b * 114) < 128000;
}

static void sui_ctrl_tabs_draw_text_top_singleline_weight_ex(SuiRenderer* r, const wchar_t* str,
    float x, float y, float w, float h, uint32_t color, float alpha, int weight, float size,
    const wchar_t* family) {
    ID2D1RenderTarget* rt;
    IDWriteFactory* dw;
    IDWriteTextFormat* fmt;
    IDWriteTextLayout* layout;
    ID2D1SolidColorBrush* br;
    float cfs, dpi;
    const wchar_t* use_family;
    D2D1_POINT_2F origin;
    if (!r->render_target || !str) return;
    if (alpha <= 0.001f) return;
    if (alpha > 1.0f) alpha = 1.0f;
    rt = (ID2D1RenderTarget*)r->render_target;
    dw = (IDWriteFactory*)r->dwrite_factory;
    if (!dw) return;
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
    layout->lpVtbl->SetParagraphAlignment(layout, 0);
    layout->lpVtbl->SetWordWrapping(layout, DWRITE_WORD_WRAPPING_NO_WRAP);
    origin.x = x;
    origin.y = y;
    rt->lpVtbl->DrawTextLayout(rt, origin, layout, (ID2D1Brush*)br, 0);
    layout->lpVtbl->Release(layout);
}

static void sui_ctrl_tabs_draw_text_middle_weight_alpha_ex(SuiRenderer* r, const wchar_t* str,
    float x, float y, float w, float h, uint32_t color, float alpha, int weight,
    float size, const wchar_t* family) {
    ID2D1RenderTarget* rt;
    IDWriteFactory* dw;
    IDWriteTextFormat* fmt;
    IDWriteTextLayout* layout;
    ID2D1SolidColorBrush* br;
    float cfs, dpi;
    const wchar_t* use_family;
    D2D1_POINT_2F origin;
    if (!r->render_target || !str) return;
    if (alpha <= 0.001f) return;
    if (alpha > 1.0f) alpha = 1.0f;
    rt = (ID2D1RenderTarget*)r->render_target;
    dw = (IDWriteFactory*)r->dwrite_factory;
    if (!dw) return;
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
    layout->lpVtbl->SetParagraphAlignment(layout, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    origin.x = x;
    origin.y = y - cfs * 0.04f;
    rt->lpVtbl->DrawTextLayout(rt, origin, layout, (ID2D1Brush*)br, 0);
    layout->lpVtbl->Release(layout);
}

static void sui_ctrl_tabs_draw_text_middle_singleline_weight_alpha_ex(SuiRenderer* r,
    const wchar_t* str, float x, float y, float w, float h, uint32_t color,
    float alpha, int weight, float size, const wchar_t* family) {
    ID2D1RenderTarget* rt;
    D2D1_RECT_F clip;
    if (!r->render_target || !str) return;
    rt = (ID2D1RenderTarget*)r->render_target;
    clip.left = x;
    clip.top = y;
    clip.right = x + w;
    clip.bottom = y + h;
    rt->lpVtbl->PushAxisAlignedClip(rt, &clip, 0);
    sui_ctrl_tabs_draw_text_middle_weight_alpha_ex(r, str, x, y, 4096.0f, h, color,
                                                   alpha, weight, size, family);
    rt->lpVtbl->PopAxisAlignedClip(rt);
}

static void sui_ctrl_draw_tabs_ex(SuiControlHost* host, SuiControl* ctrl,
                                  int draw_content_panel, int draw_focus_ring,
                                  int top_align_text) {
    SuiRenderer* r = host->renderer;
    const SuiColorScheme* sc = host->scheme;
    float s = sui_ctrl_dpi_scale(host);
    float rad = 8.0f * s;
    float tab_h = ctrl->d.tabs.tab_height;
    SuiRect content;
    uint32_t panel_fill = sui_ctrl_style_color(ctrl, SUI_STYLE_BG_COLOR, sc->background);
    uint32_t content_border_color = sui_ctrl_style_color(ctrl, SUI_STYLE_BORDER_COLOR, sc->btn_area_border);
    uint32_t tab_text_color = sui_ctrl_style_color(ctrl, SUI_STYLE_TEXT_COLOR, sc->text);
    uint32_t indicator_color = sui_ctrl_tabs_indicator_color(host, ctrl);
    const wchar_t* font_family = sui_ctrl_style_font_family(ctrl);
    int i;

    if (ctrl->state & SUI_CTRL_HIDDEN) return;
    if (!r) return;
    if (tab_h < 0.0f) tab_h = 0.0f;
    if (tab_h > ctrl->h) tab_h = ctrl->h;

    if (draw_content_panel &&
        sui_ctrl_tabs_content_rect(ctrl, &content) && content.bottom > content.top) {
        float content_w = content.right - content.left;
        float content_h = content.bottom - content.top;
        sui_renderer_fill_rounded_rect(r, content.left, content.top, content_w, content_h,
                                       rad, panel_fill);
        sui_renderer_draw_rounded_rect(r, content.left, content.top, content_w, content_h,
                                       rad, content_border_color, 1.0f);
    }

    for (i = 0; i < ctrl->d.tabs.item_count; i++) {
        float ix, iw;
        sui_ctrl_tabs_item_rect(host, ctrl, i, &ix, &iw);
        if (iw <= 0.0f) continue;

        {
            float text_pad = ctrl->d.tabs.item_hpad > 0.0f ? ctrl->d.tabs.item_hpad : 6.0f * s;
            float text_w = iw - 2.0f * text_pad;
            float hover_line = sui_ctrl_tabs_item_hover_line(ctrl, i, top_align_text);
            if (text_w > 0.0f) {
                float font_size = ctrl->font_size > 0.0f ? ctrl->font_size : r->font_size;
                int weight;
                if (ctrl->state & SUI_CTRL_BOLD) {
                    weight = sui__body_weight(r, 1);
                } else if (ctrl->state & SUI_CTRL_SEMIBOLD) {
                    weight = 600;
                } else if (ctrl->state & SUI_CTRL_MEDIUM) {
                    weight = 500;
                } else {
                    weight = sui__body_weight(r, 0);
                }
                if (top_align_text) {
                    float text_top = 4.0f * s;
                    float text_h = tab_h - text_top;
                    if (text_h < 0.0f) text_h = tab_h;
                    sui_ctrl_tabs_draw_text_top_singleline_weight_ex(r, ctrl->d.tabs.items[i].text,
                        ix + text_pad, ctrl->y + text_top, text_w, text_h, tab_text_color, 1.0f,
                        weight, font_size, font_family);
                } else {
                    sui_ctrl_tabs_draw_text_middle_singleline_weight_alpha_ex(r, ctrl->d.tabs.items[i].text,
                        ix + text_pad, ctrl->y, text_w, tab_h, tab_text_color, 1.0f,
                        weight, font_size, font_family);
                }
            }
            if (hover_line > 0.001f &&
                (i != ctrl->d.tabs.selected || ctrl->d.tabs.indicator_anim_active)) {
                float hover_x, hover_w;
                if (sui_ctrl_tabs_indicator_target_for_item(host, ctrl, i, &hover_x, &hover_w) &&
                    hover_w > 0.0f) {
                    float line_h = sui_ctrl_tabs_indicator_line_h(host);
                    float line_y = sui_ctrl_tabs_indicator_line_y(host, ctrl, tab_h, top_align_text);
                    uint32_t hover_color = sui_ctrl_tabs_is_dark_color(panel_fill) ? 0xFFFFFFu : 0x000000u;
                    sui_renderer_fill_rounded_rect_alpha(r, hover_x, line_y, hover_w, line_h,
                                                         line_h * 0.5f, hover_color,
                                                         hover_line * 0.15f);
                }
            }
        }
    }

    sui_ctrl_tabs_sync_indicator(host, ctrl, 0);
    if (ctrl->d.tabs.indicator_ready && ctrl->d.tabs.indicator_w > 0.0f) {
        float line_h = sui_ctrl_tabs_indicator_line_h(host);
        float line_y = sui_ctrl_tabs_indicator_line_y(host, ctrl, tab_h, top_align_text);
        sui_renderer_fill_rounded_rect(r, ctrl->d.tabs.indicator_x, line_y,
                                       ctrl->d.tabs.indicator_w, line_h,
                                       line_h * 0.5f, indicator_color);
    }

    if (draw_focus_ring)
        sui_ctrl_draw_kb_focus_ring(host, ctrl, rad);
}

static void sui_ctrl_draw_tabs(SuiControlHost* host, SuiControl* ctrl) {
    sui_ctrl_draw_tabs_ex(host, ctrl, 1, 1, 0);
}

static int sui_ctrl_tabs_key_target(SuiControl* c, WPARAM vk, int* out_next, int* out_dir) {
    int next;
    int dir = 0;
    if (!c || c->type != SUI_CTRL_TABS || c->d.tabs.item_count <= 0) return 0;
    if (!out_next || !out_dir) return 0;
    if (c->d.tabs.selected < 0 || c->d.tabs.selected >= c->d.tabs.item_count)
        c->d.tabs.selected = 0;
    next = c->d.tabs.selected;
    if (vk == VK_LEFT || vk == VK_UP) {
        next--;
        if (next < 0) next = c->d.tabs.item_count - 1;
        dir = 1;
    } else if (vk == VK_RIGHT || vk == VK_DOWN) {
        next++;
        if (next >= c->d.tabs.item_count) next = 0;
        dir = -1;
    } else if (vk == VK_HOME) {
        next = 0;
        dir = next < c->d.tabs.selected ? 1 : -1;
    } else if (vk == VK_END) {
        next = c->d.tabs.item_count - 1;
        dir = next < c->d.tabs.selected ? 1 : -1;
    } else {
        return 0;
    }
    if (next == c->d.tabs.selected) return 0;
    *out_next = next;
    *out_dir = dir;
    return 1;
}

static int sui_ctrl_tabs_on_keydown(SuiControlHost* host, SuiControl* c, WPARAM vk) {
    int next, dir;
    if (!sui_ctrl_tabs_key_target(c, vk, &next, &dir)) return 0;
    (void)dir;
    c->d.tabs.selected = next;
    sui_ctrl_tabs_start_indicator_transition(host, c);
    if (host->on_click) host->on_click(c->id, host->on_click_data);
    InvalidateRect(host->hwnd, NULL, FALSE);
    return 1;
}

static int sui_ctrl_tabs_update_hover(SuiControlHost* host, SuiControl* tabs, int is_hit,
                                      float mx, float my) {
    int old_hover;
    int new_hover = -1;
    if (is_hit)
        new_hover = sui_ctrl_tabs_item_at(host, tabs, mx, my);
    old_hover = tabs->d.tabs.hover_item;
    tabs->d.tabs.hover_item = new_hover;
    return old_hover != new_hover;
}

static void sui_ctrl_tabs_release(SuiControl* c) {
    if (c->type != SUI_CTRL_TABS) return;
    sui_ctrl_tabs_release_content_bitmap(c);
    if (c->d.tabs.items) { sui_free(c->d.tabs.items); c->d.tabs.items = NULL; }
    c->d.tabs.item_count = 0;
}

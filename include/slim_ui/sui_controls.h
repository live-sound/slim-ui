/* sui_controls.h -- Controls orchestrator: host init, input dispatch, draw-all.
 * Included from slim_ui.h under #ifdef SUI_IMPLEMENTATION -- no include guard.
 * Requires SUI_FEATURE_CUSTOM_CONTROLS.
 */

#define SUI_CARET_TIMER_ID  0x5549
#define SUI_HOVER_TIMER_ID  0x5552

/* Hover and activity fades: a frame's time clamped to 1..100 ms; each tick returns whether one more frame is needed, the final
 * settling frame included. */
static inline float sui__hover_frame_ms(float ms) { return ms<1 ? 1 : ms>100 ? 100 : ms; }
static inline int sui__hover_value_tick(float* value,float target,float ms) {
    float delta=target-*value,step=sui__hover_frame_ms(ms)*(0.25f/16.6667f);
    if(step>1) step=1;
    if(delta<-.005f || delta>.005f) { *value+=delta*step; return 1; }
    *value=target; return 0;
}
static inline int sui__scroll_reveal_tick(float* value,float ms) {
    if(*value<=0) return 0;
    *value-=sui__hover_frame_ms(ms)/1000;
    if(*value<0) *value=0;
    return 1;
}

/* UIA helpers — defined in sui_uia_providers.h (same TU, included after this).
 * Forward-declared here so sui_ctrl_clear_page / sui_ctrl_set_focus /
 * sui_ctrl_host_release / sui_default_msg can call them without a circular
 * include dependency. */
static void    sui_uia_disconnect_ctrl_providers(SuiControlHost* host, int from_idx, int to_idx);
static void    sui_uia_disconnect_host_provider(SuiControlHost* host);
static void    sui_uia_notify_focus_changed(SuiControlHost* host, int old_idx, int new_idx);
static LRESULT sui_uia_handle_getobject(SuiControlHost* host, WPARAM wp, LPARAM lp);

/* Renderer helper — forward-declared so sui_handle_wm_dpichanged can rebuild
 * text formats after DPI changes. Defined in sui_renderer.h. */
static void sui__renderer_rebuild_formats(SuiRenderer* r);

/* ---- Hover animation tick — returns 1 if any control is still animating -- */

static int sui_ctrl_hover_tick(SuiControlHost* host, float dt_ms) {
    int i, animating = 0;
    float indicator_step, tab_text_step;
    dt_ms = sui__hover_frame_ms(dt_ms);
    indicator_step = dt_ms / SUI_TAB_INDICATOR_ANIM_MS;
    if (indicator_step > 1.0f) indicator_step = 1.0f;
    tab_text_step = dt_ms / SUI_TAB_TEXT_HOVER_ANIM_MS;
    if (tab_text_step > 1.0f) tab_text_step = 1.0f;
    for (i = 0; i < host->control_count; i++) {
        SuiControl* c = &host->controls[i];
        float target = (c->state & SUI_CTRL_HOVER) ? 1.0f : 0.0f;
        animating |= sui__hover_value_tick(&c->hover_t, target, dt_ms);
        /* Scrollbar reveal — set to 1 by keyboard/wheel activity,
         * decays linearly so the bar lingers ~1s after last input then fades. */
        if (c->type == SUI_CTRL_TEXTVIEW) animating |= sui__scroll_reveal_tick(&c->d.textview.sb_reveal, dt_ms);
        if (c->type == SUI_CTRL_LIST) animating |= sui__scroll_reveal_tick(&c->d.list.sb_reveal, dt_ms);
        if (c->type == SUI_CTRL_TABS) {
            float content_step = dt_ms /
                (c->d.tabs.content_anim_vertical
                    ? SUI_TAB_VERTICAL_CONTENT_ANIM_MS
                    : SUI_TAB_CONTENT_ANIM_MS);
            if (content_step > 1.0f) content_step = 1.0f;
            animating |= sui_ctrl_tabs_indicator_tick(host, c, indicator_step);
            animating |= sui_ctrl_tabs_content_tick(c, content_step);
            animating |= sui_ctrl_tabs_text_hover_tick(c, tab_text_step);
        }
    }
    return animating;
}

#ifdef SUI_FEATURE_VSYNC
static void sui_ctrl_hover_vsync_tick(void* user, float dt_ms) {
    SuiControlHost* host = (SuiControlHost*)user;
    if (!host) return;
    if (!sui_ctrl_hover_tick(host, dt_ms)) host->hover_vsync_active = 0;
    InvalidateRect(host->hwnd, NULL, FALSE);
}

static void sui_ctrl_start_hover_anim(SuiControlHost* host) {
    if (!host || host->hover_vsync_active) return;
    host->hover_vsync_active = sui_vsync_attach(&host->vsync, sui_ctrl_hover_vsync_tick, host);
    if (!host->hover_vsync_active) {
        sui_ctrl_hover_tick(host, 10000.0f);
        InvalidateRect(host->hwnd, NULL, FALSE);
    }
}

static void sui_ctrl_on_vsync(SuiControlHost* host) {
    if (!host) return;
    sui_vsync_dispatch(&host->vsync);
    if (!host->hover_vsync_active) {
        sui_vsync_detach(&host->vsync, sui_ctrl_hover_vsync_tick, host);
    }
}
#else
static void sui_ctrl_start_hover_anim(SuiControlHost* host) {
    if (!host) return;
    SetTimer(host->hwnd, SUI_HOVER_TIMER_ID, 16, NULL);
}
#endif

static void sui_ctrl_tooltip_update(SuiControlHost* host, int hover_idx);

/* ---- Mouse leave — clear hover bits and start fade-out timer ------------- */

static void sui_ctrl_on_mouse_leave(SuiControlHost* host) {
    int i, changed = 0;
    sui_ctrl_spinner_stop_repeat(host);
    for (i = 0; i < host->control_count; i++) {
        SuiControl* c = &host->controls[i];
        if (c->state & SUI_CTRL_HOVER) {
            c->state &= ~SUI_CTRL_HOVER;
            changed = 1;
        }
        if (c->type == SUI_CTRL_NAV_LIST && c->d.nav.hover_item != -1) {
            c->d.nav.hover_item = -1;
            changed = 1;
        } else if (c->type == SUI_CTRL_LIST && c->d.list.hover_row != -1) {
            c->d.list.hover_row = -1;
            changed = 1;
        } else if (c->type == SUI_CTRL_TABS && c->d.tabs.hover_item != -1) {
            c->d.tabs.hover_item = -1;
            changed = 1;
        } else if (c->type == SUI_CTRL_SPINNER && (c->d.spinner.hover_up || c->d.spinner.hover_down)) {
            c->d.spinner.hover_up = 0;
            c->d.spinner.hover_down = 0;
            changed = 1;
        }
    }
    host->hover_control = -1;
    sui_ctrl_tooltip_update(host, -1);
    if (changed) sui_ctrl_start_hover_anim(host);
}

/* ---- Timer dispatch — call from WM_TIMER --------------------------------- */

static void sui_ctrl_on_timer(SuiControlHost* host, UINT_PTR id) {
    if (id == SUI_CARET_TIMER_ID) {
        host->caret_visible = !host->caret_visible;
        InvalidateRect(host->hwnd, NULL, FALSE);
    } else if (id == SUI_HOVER_TIMER_ID) {
        if (!sui_ctrl_hover_tick(host, 16.6667f)) KillTimer(host->hwnd, SUI_HOVER_TIMER_ID);
        InvalidateRect(host->hwnd, NULL, FALSE);
    } else if (id == SUI_TOOLTIP_TIMER_ID) {
        sui_ctrl_tooltip_update(host, host->tooltip_active_ctrl);
    } else if (id == SUI_SHORTCUT_RECORD_TIMER_ID) {
        sui_ctrl_shortcut_poll_gamepad_recorders(host);
    } else if (id == SUI_SPINNER_REPEAT_TIMER_ID) {
        sui_ctrl_spinner_repeat_tick(host, GetTickCount());
    }
}

/* ---- Host init ---------------------------------------------------------- */

static void sui_ctrl_host_init(SuiControlHost* host, HWND hwnd, SuiRenderer* renderer,
                                const SuiColorScheme* scheme, UINT dpi) {
    memset(host, 0, sizeof(*host));
    host->hwnd = hwnd;
    host->renderer = renderer;
    host->scheme = scheme;
    host->dpi = dpi;
    host->hover_control = -1;
    host->focused_control = -1;
    host->tooltip_active_ctrl = -1;
    host->tooltip_visible_ctrl = -1;
    host->tooltip_hover_start_ms = 0;
#ifdef SUI_FEATURE_VSYNC
    sui_vsync_init(&host->vsync, hwnd);
    host->hover_vsync_active = 0;
#endif
}

/* ---- DPI change handler -------------------------------------------------- */

/* Update host->dpi and rebuild renderer text formats for a new DPI.
 * Call from WM_DPICHANGED BEFORE rebuilding controls or resizing the window.
 * After returning, call SetWindowPos (using lp RECT*), sui_renderer_resize,
 * rebuild controls, then InvalidateRect — the same sequence as WM_SIZE.
 *
 * Usage:
 *   case WM_DPICHANGED:
 *       sui_handle_wm_dpichanged(&host, renderer, wp);
 *       // SetWindowPos, resize renderer, rebuild controls, InvalidateRect
 *       return 0;
 */
static void sui_handle_wm_dpichanged(SuiControlHost* host, SuiRenderer* r, WPARAM wp) {
    if (!host) return;
    host->dpi = HIWORD(wp);
    if (r) sui__renderer_rebuild_formats(r);
}

/* ---- Clear page --------------------------------------------------------- */

static void sui_ctrl_clear_page(SuiControlHost* host) {
    int i;
    sui_ctrl_spinner_stop_repeat(host);
#ifndef SUI_NO_COMBOBOX
    /* If a combo popup is open and its owner is a non-persistent control, destroy
     * it before freeing combo items — otherwise the popup's paint handler would
     * dereference freed items_buf (use-after-free). */
    if (host->combo_popup && host->combo_popup->hwnd &&
        host->combo_popup->owner_ctrl_index >= host->persistent_control_count)
        sui_combo_popup_destroy_internal(host);
#endif
    sui_ctrl_tooltip_update(host, -1);
    /* Free heap-allocated control buffers in slots about to be discarded
     * (those past persistent_control_count). Persistent slots keep their buffers. */
    for (i = host->persistent_control_count; i < host->control_count; i++) {
        if (host->controls[i].type == SUI_CTRL_SHORTCUT_RECORDER)
            sui_ctrl_shortcut_cancel_recording(host, &host->controls[i], 1);
        sui_ctrl_textview_release(&host->controls[i]);
        sui_ctrl_nav_release(&host->controls[i]);
        sui_ctrl_list_release(&host->controls[i]);
        sui_ctrl_tabs_stash_transition(host, &host->controls[i]);
        sui_ctrl_tabs_release(&host->controls[i]);
        sui_ctrl_combo_release(&host->controls[i]);
        sui_ctrl_edit_release(&host->controls[i]);
        sui_ctrl_style_release(&host->controls[i]);
    }
    /* Disconnect + release UIA providers for discarded (non-persistent) slots before
     * trimming control_count, so stale pointers never reach an AT tool. */
    sui_uia_disconnect_ctrl_providers(host, host->persistent_control_count, host->control_count);
    for (i = 0; i < host->persistent_control_count; i++)
        host->controls[i].state &= ~(SUI_CTRL_FOCUSED | SUI_CTRL_HOVER | SUI_CTRL_PRESSED);
    host->control_count = host->persistent_control_count;
    host->focused_control = -1;
    host->hover_control = -1;
}

/* ---- Z-order sort: stable ascending by z within one parent scope -------- */

static int sui_ctrl_z_order_scope(SuiControlHost* host, int parent_id,
                                   int out[SUI_MAX_CONTROLS]) {
    int n = host->control_count, i, j, key, count = 0;
    for (i = 0; i < n; i++)
        if (host->controls[i].parent_id == parent_id) out[count++] = i;
    for (i = 1; i < count; i++) {
        key = out[i];
        for (j = i; j > 0 && host->controls[out[j-1]].z > host->controls[key].z; j--)
            out[j] = out[j-1];
        out[j] = key;
    }
    return count;
}

/* ---- Hit test (recursive, descends into containers) --------------------- */

static int sui_ctrl_hit_test_scope(SuiControlHost* host, int parent_id,
                                    float mx, float my) {
    int order[SUI_MAX_CONTROLS];
    int n = sui_ctrl_z_order_scope(host, parent_id, order), i;
    for (i = n - 1; i >= 0; i--) {
        SuiControl* c = &host->controls[order[i]];
        if (c->state & (SUI_CTRL_HIDDEN | SUI_CTRL_DISABLED)) continue;
        if (c->type == SUI_CTRL_CONTAINER) {
            int hit;
            /* Clipping containers gate hits to their rect; pure z-scopes
             * (no_clip) recurse regardless of cursor position. */
            if (!c->d.container.no_clip) {
                if (mx < c->x || mx >= c->x + c->w
                        || my < c->y || my >= c->y + c->h)
                    continue;
            }
            hit = sui_ctrl_hit_test_scope(host, c->id, mx, my);
            if (hit >= 0) return hit;
            continue; /* container itself is non-interactive */
        }
        if (c->type == SUI_CTRL_SCROLLVIEW) {
            int hit;
            /* Gate to scrollview rect (clipped). */
            if (mx < c->x || mx >= c->x + c->w
                    || my < c->y || my >= c->y + c->h)
                continue;
            /* Scrollbar thumb wins — return scrollview index so on_lbutton_down
             * can start a drag. */
            if (sui_ctrl_scrollview_hit_thumb(host, c, mx, my))
                return order[i];
            /* Translate cursor into content space and recurse. */
            hit = sui_ctrl_hit_test_scope(host, c->id, mx,
                                           my + c->d.scroll.scroll_y);
            if (hit >= 0) return hit;
            continue; /* empty content area is non-interactive */
        }
        if (c->type == SUI_CTRL_TABS) {
            SuiRect content;
            int hit;
            if (mx < c->x || mx >= c->x + c->w
                    || my < c->y || my >= c->y + c->h)
                continue;
            if (sui_ctrl_tabs_item_at(host, c, mx, my) >= 0)
                return order[i];
            if (sui_ctrl_tabs_content_rect(c, &content)
                    && mx >= content.left && mx < content.right
                    && my >= content.top && my < content.bottom) {
                hit = sui_ctrl_hit_test_scope(host, c->id, mx, my);
                if (hit >= 0) return hit;
            }
            continue; /* empty tab content area is non-interactive */
        }
        if (mx < c->x || mx >= c->x + c->w || my < c->y || my >= c->y + c->h)
            continue;
        /* Decorative / non-interactive controls pass hits through. */
        if (c->type == SUI_CTRL_LABEL || c->type == SUI_CTRL_IMAGE
                || c->type == SUI_CTRL_PROGRESS) continue;
        return order[i];
    }
    return -1;
}

static int sui_ctrl_hit_test(SuiControlHost* host, float mx, float my) {
    return sui_ctrl_hit_test_scope(host, -1, mx, my);
}

static int sui_ctrl_tooltip_hit_test_scope(SuiControlHost* host, int parent_id,
                                           float mx, float my) {
    int order[SUI_MAX_CONTROLS];
    int n = sui_ctrl_z_order_scope(host, parent_id, order), i;
    for (i = n - 1; i >= 0; i--) {
        SuiControl* c = &host->controls[order[i]];
        int inside;
        if (c->state & (SUI_CTRL_HIDDEN | SUI_CTRL_DISABLED)) continue;
        if (c->type == SUI_CTRL_CONTAINER) {
            int hit;
            inside = (mx >= c->x && mx < c->x + c->w && my >= c->y && my < c->y + c->h);
            if (!inside && !c->d.container.no_clip) continue;
            hit = sui_ctrl_tooltip_hit_test_scope(host, c->id, mx, my);
            if (hit >= 0) return hit;
            if (inside && c->tooltip && c->tooltip[0]) return order[i];
            continue;
        }
        if (c->type == SUI_CTRL_SCROLLVIEW) {
            int hit;
            if (mx < c->x || mx >= c->x + c->w
                    || my < c->y || my >= c->y + c->h)
                continue;
            hit = sui_ctrl_tooltip_hit_test_scope(host, c->id, mx,
                                                  my + c->d.scroll.scroll_y);
            if (hit >= 0) return hit;
            if (c->tooltip && c->tooltip[0]) return order[i];
            continue;
        }
        if (c->type == SUI_CTRL_TABS) {
            SuiRect content;
            int hit;
            inside = (mx >= c->x && mx < c->x + c->w && my >= c->y && my < c->y + c->h);
            if (!inside) continue;
            if (sui_ctrl_tabs_content_rect(c, &content)
                    && mx >= content.left && mx < content.right
                    && my >= content.top && my < content.bottom) {
                hit = sui_ctrl_tooltip_hit_test_scope(host, c->id, mx, my);
                if (hit >= 0) return hit;
            }
            if (c->tooltip && c->tooltip[0]) return order[i];
            continue;
        }
        if (mx < c->x || mx >= c->x + c->w || my < c->y || my >= c->y + c->h)
            continue;
        if (c->tooltip && c->tooltip[0]) return order[i];
    }
    return -1;
}

static int sui_ctrl_tooltip_hit_test(SuiControlHost* host, float mx, float my) {
    return sui_ctrl_tooltip_hit_test_scope(host, -1, mx, my);
}

/* ---- Next focusable ----------------------------------------------------- */

static int sui_ctrl_next_focusable(SuiControlHost* host, int from, int dir) {
    int i, idx;
    for (i = 0; i < host->control_count; i++) {
        idx = (from + (i + 1) * dir + host->control_count * host->control_count) % host->control_count;
        if (idx < 0) idx += host->control_count;
        if (host->controls[idx].state & (SUI_CTRL_HIDDEN | SUI_CTRL_DISABLED))
            continue;
        if (host->controls[idx].type == SUI_CTRL_LABEL
            || host->controls[idx].type == SUI_CTRL_TOOLTIP
            || host->controls[idx].type == SUI_CTRL_IMAGE
            || host->controls[idx].type == SUI_CTRL_PROGRESS
            || host->controls[idx].type == SUI_CTRL_CONTAINER
            || host->controls[idx].type == SUI_CTRL_SCROLLVIEW)
            continue;
        /* Skip unchecked radios in a group -- only checked radio is a tab stop */
        if (host->controls[idx].type == SUI_CTRL_RADIO
            && !(host->controls[idx].state & SUI_CTRL_CHECKED))
            continue;
        return idx;
    }
    return from;
}

/* ---- Set focus ---------------------------------------------------------- */

static void sui_ctrl_set_focus(SuiControlHost* host, int index) {
    int old = host->focused_control, i;
    int old_id = (old >= 0 && old < host->control_count) ? host->controls[old].id : -1;
    int new_id = (index >= 0 && index < host->control_count) ? host->controls[index].id : -1;
    if (old != index) sui_ctrl_spinner_stop_repeat(host);
    for (i = 0; i < host->control_count; i++)
        host->controls[i].state &= ~SUI_CTRL_FOCUSED;
    if (old >= 0 && old < host->control_count) {
        if (host->controls[old].type == SUI_CTRL_EDIT
                || host->controls[old].type == SUI_CTRL_TEXTVIEW)
            KillTimer(host->hwnd, SUI_CARET_TIMER_ID);
        if (host->controls[old].type == SUI_CTRL_SPINNER)
            sui_ctrl_spinner_commit(host, &host->controls[old]);
        if (host->controls[old].type == SUI_CTRL_SHORTCUT_RECORDER)
            sui_ctrl_shortcut_cancel_recording(host, &host->controls[old], 1);
    }
    host->focused_control = index;
    if (index >= 0 && index < host->control_count) {
        host->controls[index].state |= SUI_CTRL_FOCUSED;
        if (host->controls[index].type == SUI_CTRL_EDIT && host->window_has_focus) {
            host->caret_visible = 1;
            SetTimer(host->hwnd, SUI_CARET_TIMER_ID, GetCaretBlinkTime(), NULL);
        }
        if (host->controls[index].type == SUI_CTRL_TEXTVIEW
                && !host->controls[index].d.textview.readonly
                && host->window_has_focus) {
            host->caret_visible = 1;
            SetTimer(host->hwnd, SUI_CARET_TIMER_ID, GetCaretBlinkTime(), NULL);
        }
    }
    /* Fire focus change callback if id actually changed */
    if (old_id != new_id && host->on_focus_change) {
        host->on_focus_change(old_id, new_id, host->on_focus_change_data);
    }
    /* Notify UIA if focus moved — fires HasKeyboardFocus property-change events
     * on the losing and gaining providers (no-op when UIA isn't loaded). */
    if (old != index)
        sui_uia_notify_focus_changed(host, old, index);
}

/* ---- Draw all (recursive, descends into containers) -------------------- */

static void sui_ctrl_draw_subtree(SuiControlHost* host, int parent_id);

static void sui_ctrl_tabs_draw_content_bitmap_xy(SuiControlHost* host, SuiControl* tabs,
                                                 float offset_x, float offset_y,
                                                 float opacity) {
    ID2D1RenderTarget* rt;
    ID2D1Bitmap* bitmap;
    SuiRect content;
    D2D1_RECT_F dst;
    float w, h;
    if (!host || !host->renderer || !tabs || tabs->type != SUI_CTRL_TABS) return;
    if (!tabs->d.tabs.content_bitmap) return;
    if (!host->renderer->render_target) return;
    if (opacity <= 0.001f) return;
    if (opacity > 1.0f) opacity = 1.0f;
    if (!sui_ctrl_tabs_content_rect(tabs, &content)) return;
    w = tabs->d.tabs.content_bitmap_w > 0.0f
        ? tabs->d.tabs.content_bitmap_w
        : content.right - content.left;
    h = tabs->d.tabs.content_bitmap_h > 0.0f
        ? tabs->d.tabs.content_bitmap_h
        : content.bottom - content.top;
    if (w <= 0.0f || h <= 0.0f) return;
    rt = (ID2D1RenderTarget*)host->renderer->render_target;
    bitmap = (ID2D1Bitmap*)tabs->d.tabs.content_bitmap;
    dst.left = content.left + offset_x;
    dst.top = content.top + offset_y;
    dst.right = dst.left + w;
    dst.bottom = dst.top + h;
    rt->lpVtbl->DrawBitmap(rt, bitmap, &dst, opacity,
                           D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, NULL);
}

static void sui_ctrl_tabs_draw_content_bitmap(SuiControlHost* host, SuiControl* tabs,
                                              float offset_x, float opacity) {
    sui_ctrl_tabs_draw_content_bitmap_xy(host, tabs, offset_x, 0.0f, opacity);
}

static void sui_ctrl_tabs_draw_content_subtree_xy(SuiControlHost* host, SuiControl* tabs,
                                                  float offset_x, float offset_y,
                                                  float opacity) {
    SuiRect content;
    int opacity_layer = 0;
    if (!host || !tabs || tabs->type != SUI_CTRL_TABS) return;
    if (opacity <= 0.001f) return;
    if (opacity > 1.0f) opacity = 1.0f;
    if (!sui_ctrl_tabs_content_rect(tabs, &content)) return;
    opacity_layer = sui_renderer_push_opacity(host->renderer, content.left, content.top,
                                              content.right - content.left,
                                              content.bottom - content.top,
                                              opacity);
    if (sui__tabs_absf(offset_x) > 0.01f || sui__tabs_absf(offset_y) > 0.01f) {
        D2D1_MATRIX_3X2_F saved;
        sui_ctrl_scrollview_save_transform(host, &saved);
        sui_ctrl_scrollview_apply_offset(host, &saved, offset_x, offset_y);
        sui_ctrl_draw_subtree(host, tabs->id);
        sui_ctrl_scrollview_restore_transform(host, &saved);
    } else {
        sui_ctrl_draw_subtree(host, tabs->id);
    }
    if (opacity_layer)
        sui_renderer_pop_opacity(host->renderer);
}

static void sui_ctrl_tabs_draw_content_subtree(SuiControlHost* host, SuiControl* tabs,
                                               float offset_x, float opacity) {
    sui_ctrl_tabs_draw_content_subtree_xy(host, tabs, offset_x, 0.0f, opacity);
}

/* Draws one control the usual way, without its children. A paint function calls this to get the control's own pixels. */
static void sui_ctrl_draw_self(SuiControlHost* host, SuiControl* c) {
    switch (c->type) {
        case SUI_CTRL_BUTTON:       sui_ctrl_draw_button(host, c); break;
        case SUI_CTRL_CHECKBOX:     sui_ctrl_draw_checkbox(host, c); break;
        case SUI_CTRL_RADIO:        sui_ctrl_draw_radio(host, c); break;
        case SUI_CTRL_EDIT:         sui_ctrl_draw_edit(host, c); break;
        case SUI_CTRL_COMBOBOX:     sui_ctrl_draw_combobox(host, c); break;
        case SUI_CTRL_SLIDER:       sui_ctrl_draw_slider(host, c); break;
        case SUI_CTRL_SPINNER:      sui_ctrl_draw_spinner(host, c); break;
        case SUI_CTRL_COLOR_SWATCH: sui_ctrl_draw_color_swatch(host, c); break;
        case SUI_CTRL_LABEL:        sui_ctrl_draw_label(host, c); break;
        case SUI_CTRL_TOOLTIP:      sui_ctrl_draw_tooltip(host, c); break;
        case SUI_CTRL_NAV_LIST:     sui_ctrl_draw_nav_list(host, c); break;
        case SUI_CTRL_LIST:         sui_ctrl_draw_list(host, c); break;
        case SUI_CTRL_TABS:         sui_ctrl_draw_tabs(host, c); break;
        case SUI_CTRL_IMAGE:        sui_ctrl_draw_image(host, c); break;
        case SUI_CTRL_SHORTCUT_RECORDER: sui_ctrl_draw_shortcut_recorder(host, c); break;
        case SUI_CTRL_PROGRESS:     sui_ctrl_draw_progress(host, c); break;
        case SUI_CTRL_CONTAINER:    sui_ctrl_draw_container(host, c); break;
        case SUI_CTRL_TEXTVIEW:     sui_ctrl_draw_textview(host, c); break;
        case SUI_CTRL_SCROLLVIEW:   sui_ctrl_draw_scrollview(host, c); break;
    }
}

/* Hands one control's drawing to fn, which can call sui_ctrl_draw_self: into a bitmap, say, to run a shader over the result. Input, focus
 * and hit testing don't change. Rebuilding the control clears this. */
static void sui_ctrl_set_paint(SuiControlHost* host, int id, SuiCtrlPaintFn fn, void* user) {
    SuiControl* c = sui_ctrl_find(host, id);
    if (c) { c->paint = fn; c->paint_user = user; }
}

static void sui_ctrl_draw_subtree(SuiControlHost* host, int parent_id) {
    int order[SUI_MAX_CONTROLS];
    int n = sui_ctrl_z_order_scope(host, parent_id, order), i;
    for (i = 0; i < n; i++) {
        SuiControl* c = &host->controls[order[i]];
        if (c->paint) {
            if (!(c->state & SUI_CTRL_HIDDEN)) c->paint(host, c, c->paint_user);
        } else {
            sui_ctrl_draw_self(host, c);
        }
        if (c->type == SUI_CTRL_CONTAINER) {
            int clipped = !c->d.container.no_clip;
            if (clipped)
                sui_renderer_push_clip(host->renderer, c->x, c->y, c->w, c->h);
            sui_ctrl_draw_subtree(host, c->id);
            if (clipped)
                sui_renderer_pop_clip(host->renderer);
        }
        if (c->type == SUI_CTRL_SCROLLVIEW) {
            D2D1_MATRIX_3X2_F saved;
            sui_renderer_push_clip(host->renderer, c->x, c->y, c->w, c->h);
            sui_ctrl_scrollview_save_transform(host, &saved);
            sui_ctrl_scrollview_apply_offset(host, &saved,
                                              0.0f, -c->d.scroll.scroll_y);
            sui_ctrl_draw_subtree(host, c->id);
            sui_ctrl_scrollview_restore_transform(host, &saved);
            sui_renderer_pop_clip(host->renderer);
            sui_ctrl_draw_scrollview_chrome(host, c);
        }
        if (c->type == SUI_CTRL_TABS) {
            if (sui_ctrl_tabs_push_content_clip(host, c)) {
                float new_dx = sui_ctrl_tabs_content_new_offset(c);
                float new_dy = sui_ctrl_tabs_content_new_offset_y(c);
                if (c->d.tabs.content_anim_active && c->d.tabs.content_bitmap) {
                    float old_dx = sui_ctrl_tabs_content_old_offset(c);
                    float old_dy = sui_ctrl_tabs_content_old_offset_y(c);
                    sui_ctrl_tabs_draw_content_bitmap_xy(host, c, old_dx, old_dy,
                                                         sui_ctrl_tabs_content_old_opacity(c));
                }
                sui_ctrl_tabs_draw_content_subtree_xy(host, c, new_dx, new_dy,
                                                     sui_ctrl_tabs_content_new_opacity(c));
                sui_ctrl_tabs_pop_content_clip(host, c);
            }
        }
    }
}

static void* sui_ctrl_tabs_capture_content_bitmap_ex(SuiControlHost* host, SuiControl* tabs,
                                                     float* out_w, float* out_h,
                                                     SuiTabContentPaintFn paint_fn,
                                                     void* paint_user,
                                                     int tab_index) {
    SuiRenderer* r;
    ID2D1RenderTarget* original_rt;
    ID2D1RenderTarget* offscreen_rt;
    ID2D1BitmapRenderTarget* bitmap_rt = NULL;
    ID2D1Bitmap* bitmap = NULL;
    void* original_brush;
    void* offscreen_brush;
    SuiRect content;
    struct { float width, height; } desired_size;
    D2D1_MATRIX_3X2_F m;
    D2D1_PIXEL_FORMAT bitmap_format;
    D2D1_COLOR_F clear_color;
    float w, h;
    HRESULT hr;
    if (out_w) *out_w = 0.0f;
    if (out_h) *out_h = 0.0f;
    if (!host || !tabs || tabs->type != SUI_CTRL_TABS) return NULL;
    r = host->renderer;
    if (!r || !r->render_target) return NULL;
    if (!sui_ctrl_tabs_content_rect(tabs, &content)) return NULL;
    w = content.right - content.left;
    h = content.bottom - content.top;
    if (w <= 0.0f || h <= 0.0f) return NULL;

    original_rt = (ID2D1RenderTarget*)r->render_target;
    desired_size.width = w;
    desired_size.height = h;
    bitmap_format.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    bitmap_format.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
    hr = original_rt->lpVtbl->CreateCompatibleRenderTarget(
        original_rt, &desired_size, NULL,
        &bitmap_format, 0, (void**)&bitmap_rt);
    if (FAILED(hr) || !bitmap_rt) return NULL;

    offscreen_rt = (ID2D1RenderTarget*)bitmap_rt;
    original_brush = r->scratch_brush;
    r->render_target = bitmap_rt;
    r->scratch_brush = NULL;

    sui__apply_text_style(r, offscreen_rt);
    offscreen_rt->lpVtbl->BeginDraw(offscreen_rt);
    clear_color.r = 0.0f;
    clear_color.g = 0.0f;
    clear_color.b = 0.0f;
    clear_color.a = 0.0f;
    offscreen_rt->lpVtbl->Clear(offscreen_rt, &clear_color);
    m._11 = 1.0f; m._12 = 0.0f;
    m._21 = 0.0f; m._22 = 1.0f;
    m._31 = -content.left; m._32 = -content.top;
    offscreen_rt->lpVtbl->SetTransform(offscreen_rt, &m);
    if (paint_fn) {
        paint_fn(host, tabs, tab_index, paint_user);
    } else {
        sui_ctrl_draw_subtree(host, tabs->id);
    }
    hr = offscreen_rt->lpVtbl->EndDraw(offscreen_rt, NULL, NULL);

    offscreen_brush = r->scratch_brush;
    if (offscreen_brush) {
        typedef ULONG (STDMETHODCALLTYPE *SuiReleaseFn)(void*);
        ((SuiReleaseFn)(*(void***)offscreen_brush)[2])(offscreen_brush);
    }
    r->render_target = original_rt;
    r->scratch_brush = original_brush;

    if (SUCCEEDED(hr))
        hr = bitmap_rt->lpVtbl->GetBitmap(bitmap_rt, &bitmap);
    bitmap_rt->lpVtbl->Base.Release((ID2D1RenderTarget*)bitmap_rt);
    if (FAILED(hr) || !bitmap) return NULL;
    if (out_w) *out_w = w;
    if (out_h) *out_h = h;
    return bitmap;
}

static void* sui_ctrl_tabs_capture_content_bitmap(SuiControlHost* host, SuiControl* tabs,
                                                  float* out_w, float* out_h) {
    return sui_ctrl_tabs_capture_content_bitmap_ex(host, tabs, out_w, out_h, NULL, NULL,
                                                  tabs ? tabs->d.tabs.selected : -1);
}

static void sui_ctrl_tabs_start_content_transition_axis_ex(SuiControlHost* host, SuiControl* tabs,
                                                           int old_selected, int new_selected,
                                                           int dir, int vertical, float travel,
                                                           SuiTabContentPaintFn paint_fn,
                                                           void* paint_user) {
    if (!host || !tabs || tabs->type != SUI_CTRL_TABS) return;
    if (old_selected == new_selected) return;
    if (dir == 0) dir = new_selected < old_selected ? 1 : -1;
    sui_ctrl_tabs_release_content_bitmap(tabs);
    tabs->d.tabs.content_bitmap = sui_ctrl_tabs_capture_content_bitmap_ex(
        host, tabs, &tabs->d.tabs.content_bitmap_w, &tabs->d.tabs.content_bitmap_h,
        paint_fn, paint_user, old_selected);
    tabs->d.tabs.content_anim_active = 1;
    tabs->d.tabs.content_anim_dir = dir > 0 ? 1 : -1;
    tabs->d.tabs.content_anim_vertical = vertical ? 1 : 0;
    tabs->d.tabs.content_anim_from = old_selected;
    tabs->d.tabs.content_anim_to = new_selected;
    tabs->d.tabs.content_anim_t = 0.0f;
    if (travel > 0.0f) tabs->d.tabs.content_anim_travel = travel;
    sui_ctrl_start_hover_anim(host);
}

static void sui_ctrl_tabs_start_content_transition_ex(SuiControlHost* host, SuiControl* tabs,
                                                      int old_selected, int new_selected,
                                                      int dir,
                                                      SuiTabContentPaintFn paint_fn,
                                                      void* paint_user) {
    float travel = tabs ? SUI_TAB_CONTENT_SLIDE_DP * sui_ctrl_dpi_scale(host) : 0.0f;
    sui_ctrl_tabs_start_content_transition_axis_ex(host, tabs, old_selected, new_selected,
                                                   dir, 0, travel, paint_fn, paint_user);
}

static void sui_ctrl_tabs_start_content_transition(SuiControlHost* host, SuiControl* tabs,
                                                   int old_selected, int new_selected,
                                                   int dir) {
    sui_ctrl_tabs_start_content_transition_ex(host, tabs, old_selected, new_selected,
                                              dir, NULL, NULL);
}

static void sui_ctrl_draw_all(SuiControlHost* host) {
    if (sui_ctrl_tabs_host_has_active_transition(host))
        sui_ctrl_start_hover_anim(host);
    sui_ctrl_draw_subtree(host, -1);
}

/* ---- Min window size: row-aware bounding box ---------------------------
 *
 * sui_ctrl_min_size returns the smallest (w, h) below which slim-ui can
 * no longer render the control tree without clipping. The result drives
 * sui_handle_wm_getminmaxinfo's ptMinTrackSize.
 *
 * Width is row-aware: top-level controls (and each container's children)
 * are grouped by Y-band overlap; each row's width is
 *     row_w = max_left_edge + max_right_extent
 * and min_w = max over rows. Height keeps the simple top_h + bot_ext
 * stack — UI rows naturally separate on Y, so summing top-positioned and
 * bottom-anchored heights is correct.
 *
 * Each control contributes its *intrinsic* size — text + chrome for
 * atoms, recursive group min for containers/scrollviews — clamped up by
 * the per-control floor c->min_w / c->min_h: effective_w =
 * max(intrinsic_w, c->min_w). Same convention on Y. Most controls leave
 * min_w/min_h = 0 so the intrinsic does the talking; atom-style controls
     * without a text-derived intrinsic (swatch/image/shortcut recorder) get min_w = w
 * at add-time so they still pin to their declared size.
 *
 * Recursion via sui_ctrl_intrinsic_size is bounded by
 * SUI_MAX_CONTAINER_DEPTH to guard against parent_id cycles in a buggy
 * consumer.
 *
 * Consumers can pin a hard floor via SuiControlHost::min_client_w (picked
 * up by sui_handle_wm_getminmaxinfo before this function is called) if
 * the auto-derived floor isn't desired. */

static void sui_ctrl_set_anchor(SuiControlHost* host, int id, uint32_t flags,
                                float right_margin, float bottom_margin) {
    SuiControl* c = sui_ctrl_find(host, id);
    if (!c) return;
    c->state &= ~(uint32_t)(SUI_CTRL_ANCHOR_RIGHT | SUI_CTRL_ANCHOR_BOTTOM);
    c->state |= flags & (uint32_t)(SUI_CTRL_ANCHOR_RIGHT | SUI_CTRL_ANCHOR_BOTTOM);
    c->right_margin  = right_margin;
    c->bottom_margin = bottom_margin;
}

/* Forward decls — recursion: intrinsic_size → group_min_size → effective_size → intrinsic_size. */
static void sui_ctrl_intrinsic_size(SuiControlHost* host, SuiControl* c,
                                     float* iw, float* ih, int depth);
static void sui_ctrl_group_min_size(SuiControlHost* host, int parent_id,
                                     float origin_x, float origin_y,
                                     float* out_w, float* out_h, int depth);

/* Effective contribution to row/column min-size scans. The floor is
 * max(intrinsic, c->min_w) — c->w is the assigned LAYOUT width and no
 * longer doubles as a declared minimum. Atom-style controls without a
 * text-derived intrinsic (swatch/image/shortcut recorder) get min_w = w at add-time
 * so they still pin to their declared size. Controls with a meaningful
 * intrinsic (button/checkbox/label/combobox/textview…) leave min_w = 0
 * and let the intrinsic do the talking — this lets two-column layouts
 * (col_w = 0.5 * content_w) shrink without ratcheting upward. */
static void sui_ctrl_effective_size(SuiControlHost* host, SuiControl* c,
                                     float* ew, float* eh, int depth) {
    float iw = 0.0f, ih = 0.0f;
    sui_ctrl_intrinsic_size(host, c, &iw, &ih, depth);
    *ew = (c->min_w > iw) ? c->min_w : iw;
    *eh = (c->min_h > ih) ? c->min_h : ih;
}

/* Phase 2b: returns the control's preferred size for flex distribution.
 * pref_w/pref_h fall back to effective (max(min,intrinsic)) when 0. The
 * returned pref is never below the effective floor so distribution can
 * trust it as the starting basis. Same units as effective: scaled px. */
static void sui_ctrl_pref_size(SuiControlHost* host, SuiControl* c,
                                float* pw, float* ph, int depth) {
    float ew = 0.0f, eh = 0.0f;
    sui_ctrl_effective_size(host, c, &ew, &eh, depth);
    *pw = (c->pref_w > 0.0f) ? c->pref_w : ew;
    *ph = (c->pref_h > 0.0f) ? c->pref_h : eh;
    if (*pw < ew) *pw = ew;
    if (*ph < eh) *ph = eh;
}

static void sui_ctrl_intrinsic_size(SuiControlHost* host, SuiControl* c,
                                     float* iw, float* ih, int depth) {
    float s = sui_ctrl_dpi_scale(host);
    SuiRenderer* r = host->renderer;
    float fs = c->font_size > 0.0f ? c->font_size : (r ? r->font_size : 14.0f);
    const wchar_t* family = sui_ctrl_style_font_family(c);
    int bold = (c->state & SUI_CTRL_BOLD) ? 1 : 0;
    float w = 0.0f, h = 0.0f;
    if (depth >= SUI_MAX_CONTAINER_DEPTH) {
        if (iw) *iw = c->w;
        if (ih) *ih = c->h;
        return;
    }
    switch (c->type) {
    case SUI_CTRL_BUTTON: {
        float tw = (r && c->text[0]) ? sui_renderer_measure_text_width_ex(r, c->text, bold, fs, family) : 0.0f;
        w = tw + SUI_BUTTON_HPAD * s;
        h = (c->h > 0.0f) ? c->h : SUI_DEFAULT_CTRL_H * s;
        break;
    }
    case SUI_CTRL_LABEL: {
        w = (r && c->text[0]) ? sui_renderer_measure_text_width_ex(r, c->text, bold, fs, family) : 0.0f;
        h = (c->h > 0.0f) ? c->h : SUI_DEFAULT_LABEL_H * s;
        break;
    }
    case SUI_CTRL_TOOLTIP: {
        w = SUI_DEFAULT_TOOLTIP_SIZE * s;
        h = SUI_DEFAULT_TOOLTIP_SIZE * s;
        break;
    }
    case SUI_CTRL_CHECKBOX:
    case SUI_CTRL_RADIO: {
        float tw = (r && c->text[0]) ? sui_renderer_measure_text_width_ex(r, c->text, bold, fs, family) : 0.0f;
        w = SUI_CHECKBOX_BOX_W * s + SUI_CHECKBOX_GAP * s + tw;  /* box + gap + text */
        h = (c->h > 0.0f) ? c->h : SUI_CHECKBOX_H * s;
        break;
    }
    case SUI_CTRL_COMBOBOX: {
        int i; float maxw = 0.0f;
        if (r) {
            if (sui_ctrl_combo_is_virtual(c)) {
                SuiComboItemView item;
                sui_ctrl_combo_get_item_view(c, c->d.combo.selected, &item);
                if (item.text[0])
                    maxw = sui_renderer_measure_text_width_ex(r, item.text, bold, fs, family);
            } else {
                for (i = 0; i < c->d.combo.item_count; i++) {
                    const wchar_t* it = c->d.combo.items[i];
                    if (it && it[0]) {
                        float tw = sui_renderer_measure_text_width_ex(r, it, bold, fs, family);
                        if (tw > maxw) maxw = tw;
                    }
                }
            }
        }
        if (sui_ctrl_combo_is_virtual(c)) {
            w = maxw + 26.0f * s + SUI_COMBO_ARROW_W * s + SUI_COMBO_TEXT_PAD * s;
        } else {
            w = maxw + SUI_COMBO_ARROW_W * s + SUI_COMBO_TEXT_PAD * s;  /* chevron + padding */
        }
        h = (c->h > 0.0f) ? c->h : SUI_DEFAULT_CTRL_H * s;
        break;
    }
    case SUI_CTRL_CONTAINER: {
        float gw = 0.0f, gh = 0.0f;
        sui_ctrl_group_min_size(host, c->id, c->x, c->y, &gw, &gh, depth + 1);
        w = gw;
        h = gh;
        break;
    }
    case SUI_CTRL_SCROLLVIEW: {
        float gw = 0.0f, gh = 0.0f;
        /* Reserve the documented ~12dp hot zone (sui_ctrl_scrollview.h) so
         * children laid out with the same convention can't clip at min_w. */
        float gutter = SUI_SCROLLVIEW_SB_HOT_W * s;
        sui_ctrl_group_min_size(host, c->id, c->x, c->y, &gw, &gh, depth + 1);
        w = gw + gutter;
        /* Viewport floor — content scrolls, so the visible height is
         * bounded by a small constant rather than the assigned c->h. */
        h = 120.0f * s;
        break;
    }
    case SUI_CTRL_TEXTVIEW: {
        /* Textviews soft-wrap; min width is unconstrained. Min height is
         * a small viewport floor — mirrors the demo's tv_min (80dp). */
        w = 0.0f;
        h = 80.0f * s;
        break;
    }
    case SUI_CTRL_SLIDER:
    case SUI_CTRL_EDIT:
    case SUI_CTRL_SPINNER:
        /* No text-derived intrinsic; use a baseline floor matching the
         * declarative API's SUI_DEFAULT_CTRL_MIN_W so imperative-built
         * windows don't collapse to zero width. */
        w = 80.0f * s;
        h = (c->h > 0.0f) ? c->h : SUI_DEFAULT_CTRL_H * s;
        break;
    case SUI_CTRL_PROGRESS:
        w = 80.0f * s;
        h = (c->h > 0.0f) ? c->h : SUI_DEFAULT_PROG_H * s;
        break;
    case SUI_CTRL_TABS:
        w = 120.0f * s;
        h = (c->h > 0.0f) ? c->h : (SUI_DEFAULT_TAB_H + SUI_DEFAULT_TAB_CONTENT_GAP + 80.0f) * s;
        break;
    case SUI_CTRL_SHORTCUT_RECORDER:
        /* Recordable indicator chip. ~80dp keeps the chip legible at min;
         * callers wanting a hard fixed visual width (e.g. 200dp settings
         * hotkey) set c->min_w explicitly. */
        w = 80.0f * s;
        h = (c->h > 0.0f) ? c->h : SUI_DEFAULT_CTRL_H * s;
        break;
    /* nav, swatch, image: no derivable intrinsic. SWATCH/IMAGE collapse
     * to 0 unless the caller pins c->min_w explicitly — lets layout-fill
     * atoms shrink and gives fixed-visual atoms a hard floor on opt-in. */
    default:
        w = 0.0f;
        h = 0.0f;
        break;
    }
    if (iw) *iw = w;
    if (ih) *ih = h;
}

/* Group children of `parent_id` (or top-level if -1) by Y-band overlap;
 * per row compute row_w = max_left_edge + max_right_extent. Children's
 * positions are stored in absolute coords, so callers pass `origin_x/y`
 * to convert to parent-local edges. Out is parent-local min size.
 *
 * If the parent container has SUI_CTRL_FLOW, the width scan switches to
 * row_w = max(child.effective_w) — child positions encode the *current*
 * width's column count and would otherwise force the floor to N cells.
 * Height still uses the same top+bot-stacks logic; the caller is
 * expected to recompute child positions on WM_SIZE so the column count
 * shrinks naturally.
 *
 * For top-level scans (parent_id == -1) where the host opts in via
 * `min_size_strategy = SUI_MIN_SIZE_FLEX_ROWS`, the row scan is
 * replaced with a Y-slice scan: for each control's top edge Y, sum the
 * non-anchored ew of every control active at that Y plus (n-1)*4dp,
 * then add max(anchored ew + right_margin). max across slices wins.
 * This is required when the consumer rebuilds children on WM_SIZE
 * with W-scaled positions (e.g. col_w = 0.5 * content_w) — without
 * it, max(ex+ew) tracks W down and the window can shrink past
 * intrinsic content. Y-slice (vs. Y-band-expansion) is needed because
 * a full-height sidebar would otherwise merge every row into one band
 * and over-estimate by summing every control. */
static void sui_ctrl_group_min_size(SuiControlHost* host, int parent_id,
                                     float origin_x, float origin_y,
                                     float* out_w, float* out_h, int depth) {
    int n = host->control_count, i, j, m = 0;
    int   idx[SUI_MAX_CONTROLS];
    float ex[SUI_MAX_CONTROLS], ey[SUI_MAX_CONTROLS];
    float ew[SUI_MAX_CONTROLS], eh[SUI_MAX_CONTROLS];
    char  done[SUI_MAX_CONTROLS];
    float max_row_w = 0.0f, top_h = 0.0f, bot_ext = 0.0f;
    int   parent_is_flow = 0;
    /* FLEX_ROWS applies at every non-FLOW level so containers (HSTACK,
     * VSTACK, scrollviews) whose children scale with W don't track W
     * down via the default Y-band scan's max(ex+ew). */
    int   use_flex_rows = (host->min_size_strategy == SUI_MIN_SIZE_FLEX_ROWS);
    /* Conservative inter-control gap for the slice scan. Matches the demo's
     * typical horizontal `gap` (16dp) and exceeds smaller per-button gaps,
     * so the floor errs slightly large rather than clipping. */
    float flex_gap = 16.0f * (host->dpi > 0 ? (float)host->dpi / 96.0f : 1.0f);

    if (parent_id >= 0) {
        SuiControl* p = sui_ctrl_find(host, parent_id);
        if (p && (p->state & SUI_CTRL_FLOW)) parent_is_flow = 1;
    }

    for (i = 0; i < n; i++) {
        SuiControl* c = &host->controls[i];
        float cw, ch;
        if (c->parent_id != parent_id) continue;
        if (c->state & SUI_CTRL_HIDDEN) continue;
        sui_ctrl_effective_size(host, c, &cw, &ch, depth);
        idx[m] = i;
        ex[m]  = c->x - origin_x;
        ey[m]  = c->y - origin_y;
        ew[m]  = cw;
        eh[m]  = ch;
        done[m] = 0;
        m++;
    }

    /* Width — FLOW: max child width (positions ignored). */
    if (parent_is_flow) {
        for (i = 0; i < m; i++) {
            if (ew[i] > max_row_w) max_row_w = ew[i];
        }
        goto height_scan;
    }

    /* Width — FLEX_ROWS: per Y-slice, sum non-anchored ew + max anchored
     * ew+rm. Y-band expansion is unsuitable here because a full-height
     * sidebar (e.g. nav list) would merge every row into one band, summing
     * every control. Slicing at each control's top edge gives the actual
     * "controls active simultaneously at this Y" set; max across slices is
     * the no-overlap minimum window width. Default Y-band scan is skipped
     * because it tracks W down for W-scaled column layouts. */
    if (use_flex_rows) {
        for (i = 0; i < m; i++) {
            SuiControl* ci = &host->controls[idx[i]];
            float seed_y = ci->y - origin_y;
            float left_sum = 0.0f, right_max = 0.0f;
            int   left_count = 0;
            for (j = 0; j < m; j++) {
                SuiControl* cj = &host->controls[idx[j]];
                /* Overlap test uses c->y / c->h (actual rendered span) — eh
                 * from effective_size collapses to 0 for atoms with no
                 * intrinsic height (e.g. NAV_LIST), which would drop the
                 * sidebar from every slice and let the button rows
                 * underestimate the floor. */
                float jy0 = cj->y - origin_y, jy1 = jy0 + cj->h;
                if (jy0 > seed_y || jy1 <= seed_y) continue;
                if (cj->state & SUI_CTRL_ANCHOR_RIGHT) {
                    float rx = ew[j] + cj->right_margin;
                    if (rx > right_max) right_max = rx;
                } else {
                    left_sum += ew[j];
                    left_count++;
                }
            }
            {
                float lpart = left_sum + (left_count > 1
                    ? (float)(left_count - 1) * flex_gap : 0.0f);
                float rw = lpart + right_max;
                if (rw > max_row_w) max_row_w = rw;
            }
        }
        goto height_scan;
    }

    /* Width — default: group by Y-band overlap; per row, row_w = max-left +
     * max-right. Stable when child positions don't scale with W. */
    for (i = 0; i < m; i++) {
        float y0, y1, left = 0.0f, right = 0.0f;
        int expanded;
        if (done[i]) continue;
        y0 = ey[i]; y1 = ey[i] + eh[i];
        do {
            expanded = 0;
            for (j = 0; j < m; j++) {
                float jy0 = ey[j], jy1 = ey[j] + eh[j];
                if (done[j]) continue;
                if (jy0 < y1 && jy1 > y0) {
                    if (jy0 < y0) { y0 = jy0; expanded = 1; }
                    if (jy1 > y1) { y1 = jy1; expanded = 1; }
                }
            }
        } while (expanded);
        for (j = 0; j < m; j++) {
            SuiControl* cj;
            float jy0 = ey[j], jy1 = ey[j] + eh[j];
            if (done[j]) continue;
            if (jy0 >= y1 || jy1 <= y0) continue;
            done[j] = 1;
            cj = &host->controls[idx[j]];
            if (cj->state & SUI_CTRL_ANCHOR_RIGHT) {
                float rx = ew[j] + cj->right_margin;
                if (rx > right) right = rx;
            } else {
                float lx = ex[j] + ew[j];
                if (lx > left) left = lx;
            }
        }
        {
            float rw = left + right;
            if (rw > max_row_w) max_row_w = rw;
        }
    }

height_scan:
    /* Height — top + bot stacks (rows naturally separate on Y axis). */
    for (i = 0; i < m; i++) {
        SuiControl* c = &host->controls[idx[i]];
        if (c->state & SUI_CTRL_ANCHOR_BOTTOM) {
            float by = eh[i] + c->bottom_margin;
            if (by > bot_ext) bot_ext = by;
        } else {
            float ty = ey[i] + eh[i];
            if (ty > top_h) top_h = ty;
        }
    }
    if (out_w) *out_w = max_row_w;
    if (out_h) *out_h = top_h + bot_ext;
}

static void sui_ctrl_min_size(SuiControlHost* host, float* min_w, float* min_h) {
    sui_ctrl_group_min_size(host, /*parent_id=*/-1, 0.0f, 0.0f, min_w, min_h, /*depth=*/0);
}

/* ---- WM_GETMINMAXINFO helper: clamp window's ptMinTrackSize -------------
 *
 * Call from the consumer's WndProc on WM_GETMINMAXINFO to forbid the user
 * from dragging the window smaller than what slim-ui can render without
 * same-z overlap. Adds the non-client frame via AdjustWindowRectExForDpi
 * (Win10 1607+) with runtime fallback to AdjustWindowRectEx (96 DPI).
 *
 * Per-axis dispatch:
 *   pin set   →  use pin (overrides scan + bubble for that axis).
 *   pin unset →  use max(scan, declarative-tree intrinsic_min_*).
 *
 * Pin is an *override* per axis, not a floor merged with the scan: the
 * scan is unreliable for sections that grow controls (e.g. vstack
 * distribute writes grown c->h on the grow child and grown c->y on
 * trailing siblings; default Y-band scan + a full-height sidebar merges
 * every row → left = max(c->x + c->w) tracks W via grown hstack
 * buttons). In those cases the pin is the only stable floor; merging
 * with scan would let the (unreliable) scan keep the floor pinned to
 * the current window dimension and block any shrink. Pin per axis so
 * consumers can pin only the axis whose scan they can't reconstruct
 * (e.g. Choices pins W only — H scan is stable). */

static void sui_handle_wm_getminmaxinfo(SuiControlHost* host, HWND hwnd, LPARAM lparam) {
    typedef BOOL (WINAPI *AdjustForDpiFn)(LPRECT, DWORD, BOOL, DWORD, UINT);
    static AdjustForDpiFn s_adjust_for_dpi;
    static int s_adjust_resolved;
    MINMAXINFO* mmi = (MINMAXINFO*)lparam;
    float mw = 0.0f, mh = 0.0f;
    int pin_w, pin_h;
    DWORD style, exstyle;
    RECT rc;
    LONG total_w, total_h;
    if (!host || !hwnd || !mmi) return;
    /* Per-axis OVERRIDE: pin set → use pin (replaces scan + bubble for
     * that axis); pin unset → use max(scan, bubble). See the contract
     * comment above the function for why pin must override, not merge
     * with, the scan (vstack-grow / nav-induced Y-band merge corrupt
     * the scan for grown sections, so MAX(pin, scan) = scan and the
     * window can never shrink). Consumers pin only the axis whose scan
     * they can't reconstruct; the unpinned axis still gets the scan +
     * bubble floor so no-overlap is preserved. */
    sui_ctrl_min_size(host, &mw, &mh);
    pin_w = host->min_client_w > 0.0f;
    pin_h = host->min_client_h > 0.0f;
    if (pin_w)                            mw = host->min_client_w;
    else if (host->intrinsic_min_w > mw)  mw = host->intrinsic_min_w;
    if (pin_h)                            mh = host->min_client_h;
    else if (host->intrinsic_min_h > mh)  mh = host->intrinsic_min_h;
    /* Pad detection: at current client size, measure the gap between the
     * rightmost/bottommost non-anchored top-level control and the client
     * edge.  That gap is the implicit section right/bottom padding which
     * is invisible to both the FLEX_ROWS slice scan (sums widths but
     * ignores X origins) and the default Y-band scan (max(ex+ew) only
     * reaches the last control, not the window edge).  Convergence is
     * stable because non-anchored controls scale with content_w; the
     * gap stays invariant as the user drags toward the floor. */
    {
        RECT cr;
        if (GetClientRect(hwnd, &cr) && cr.right > 0 && cr.bottom > 0) {
            float cw = (float)cr.right;
            float ch = (float)cr.bottom;
            float max_right = 0.0f, max_bot = 0.0f;
            int i;
            for (i = 0; i < host->control_count; i++) {
                SuiControl* c = &host->controls[i];
                if (c->parent_id != -1) continue;
                if (c->state & SUI_CTRL_HIDDEN) continue;
                if (!(c->state & SUI_CTRL_ANCHOR_RIGHT)) {
                    float r = c->x + c->w;
                    if (r > max_right) max_right = r;
                }
                if (!(c->state & SUI_CTRL_ANCHOR_BOTTOM)) {
                    float b = c->y + c->h;
                    if (b > max_bot) max_bot = b;
                }
            }
            if (!pin_w && cw > max_right) mw += (cw - max_right);
            if (!pin_h && ch > max_bot)   mh += (ch - max_bot);
        }
    }
    style   = (DWORD)GetWindowLongW(hwnd, GWL_STYLE);
    exstyle = (DWORD)GetWindowLongW(hwnd, GWL_EXSTYLE);
    rc.left = 0; rc.top = 0;
    rc.right  = (LONG)mw;
    rc.bottom = (LONG)mh;
    if (!s_adjust_resolved) {
        s_adjust_for_dpi = (AdjustForDpiFn)GetProcAddress(
            GetModuleHandleW(L"user32.dll"), "AdjustWindowRectExForDpi");
        s_adjust_resolved = 1;
    }
    if (s_adjust_for_dpi)
        s_adjust_for_dpi(&rc, style, FALSE, exstyle, sui_get_window_dpi(hwnd));
    else
        AdjustWindowRectEx(&rc, style, FALSE, exstyle);
    total_w = rc.right - rc.left;
    total_h = rc.bottom - rc.top;
    if (total_w > mmi->ptMinTrackSize.x) mmi->ptMinTrackSize.x = total_w;
    if (total_h > mmi->ptMinTrackSize.y) mmi->ptMinTrackSize.y = total_h;
}

/* ---- Tooltip ------------------------------------------------------------ */

static void sui_ctrl_set_tooltip(SuiControlHost* host, int ctrl_id, const wchar_t* text) {
    SuiControl* c = sui_ctrl_find(host, ctrl_id);
    if (c) {
        sui__wstrncpy_z(c->tooltip_text, text, SUI_TOOLTIP_TEXT_MAX);
        c->tooltip = c->tooltip_text[0] ? c->tooltip_text : NULL;
    }
}

static void sui_ctrl_tooltip_update(SuiControlHost* host, int hover_idx) {
    DWORD now;
    int valid = (hover_idx >= 0 && hover_idx < host->control_count
        && host->controls[hover_idx].tooltip && host->controls[hover_idx].tooltip[0]);

    if (!valid) {
        host->tooltip_active_ctrl = -1;
        host->tooltip_hover_start_ms = 0;
        KillTimer(host->hwnd, SUI_TOOLTIP_TIMER_ID);
        sui_ctrl_hide_tooltip_popup(host);
        return;
    }

    now = GetTickCount();
    if (hover_idx != host->tooltip_active_ctrl) {
        host->tooltip_active_ctrl = hover_idx;
        host->tooltip_hover_start_ms = now;
        SetTimer(host->hwnd, SUI_TOOLTIP_TIMER_ID, 40, NULL);
        sui_ctrl_hide_tooltip_popup(host);
        return;
    }

    if ((DWORD)(now - host->tooltip_hover_start_ms) >= SUI_TOOLTIP_DELAY_MS) {
        KillTimer(host->hwnd, SUI_TOOLTIP_TIMER_ID);
        if (host->tooltip_visible_ctrl != hover_idx) {
            host->tooltip_visible_ctrl = hover_idx;
            sui_ctrl_show_tooltip_popup(host, hover_idx);
        }
    } else {
        SetTimer(host->hwnd, SUI_TOOLTIP_TIMER_ID, 40, NULL);
    }
}

/* ---- Mouse move --------------------------------------------------------- */

static void sui_ctrl_on_mouse_move(SuiControlHost* host, int mx, int my) {
    int hit, i, changed = 0;

    for (i = 0; i < host->control_count; i++) {
        SuiControl* list = &host->controls[i];
        if (list->type != SUI_CTRL_LIST) continue;
        if (!list->d.list.sb_dragging) continue;
        {
            float sb_x, sb_y, sb_w, sb_h, thumb_y, thumb_h, ms;
            float dy = (float)my - list->d.list.sb_drag_anchor;
            float new_scroll = list->d.list.sb_drag_start_scroll;
            if (sui_ctrl_list_scrollbar_geom(host, list, &sb_x, &sb_y, &sb_w, &sb_h,
                                             &thumb_y, &thumb_h, &ms)) {
                float track_travel = sb_h - thumb_h;
                if (track_travel > 0.0f && ms > 0.0f)
                    new_scroll = list->d.list.sb_drag_start_scroll + dy * ms / track_travel;
                if (new_scroll < 0.0f) new_scroll = 0.0f;
                if (new_scroll > ms) new_scroll = ms;
                list->d.list.scroll_offset = new_scroll;
                list->d.list.sb_reveal = 1.0f;
            }
            InvalidateRect(host->hwnd, NULL, FALSE);
        }
        return;
    }

    /* Scrollview thumb drag — per-control state, no host-field aliasing. */
    for (i = 0; i < host->control_count; i++) {
        SuiControl* sv = &host->controls[i];
        if (sv->type != SUI_CTRL_SCROLLVIEW) continue;
        if (!sv->d.scroll.track_dragging) continue;
        {
            float sb_x, sb_y, sb_w, sb_h, thumb_y, thumb_h, ms;
            float dy        = (float)my - sv->d.scroll.track_drag_anchor;
            float new_scroll = sv->d.scroll.drag_start_scroll;
            if (sui_ctrl_scrollview_thumb(host, sv, &sb_x, &sb_y, &sb_w, &sb_h,
                                           &thumb_y, &thumb_h, &ms)) {
                float track_travel = sb_h - thumb_h;
                if (track_travel > 0.0f && ms > 0.0f)
                    new_scroll = sv->d.scroll.drag_start_scroll + dy * ms / track_travel;
                if (new_scroll < 0.0f) new_scroll = 0.0f;
                if (new_scroll > ms)   new_scroll = ms;
                sv->d.scroll.scroll_y = new_scroll;
            }
            InvalidateRect(host->hwnd, NULL, FALSE);
        }
        return;
    }

    if (host->dragging_scrollbar) {
        SuiControl* c = &host->controls[host->dragging_scrollbar - 1];
        float visible_h  = c->h - 16.0f * sui_ctrl_dpi_scale(host);
        float content_h  = c->d.textview.content_height;
        float max_scroll = content_h - visible_h;
        float dy         = (float)my - host->drag_start_y;
        float ratio      = (content_h > 0.0f) ? visible_h / content_h : 1.0f;
        float thumb_h    = visible_h * ratio;
        float new_scroll = host->drag_start_scroll;
        if (max_scroll > 0 && visible_h - thumb_h > 0)
            new_scroll = host->drag_start_scroll + dy * max_scroll / (visible_h - thumb_h);
        if (new_scroll < 0)          new_scroll = 0;
        if (new_scroll > max_scroll) new_scroll = max_scroll < 0 ? 0 : max_scroll;
        c->d.textview.scroll_offset = new_scroll;
        c->d.textview.sb_reveal     = 1.0f;
        InvalidateRect(host->hwnd, NULL, FALSE);
        return;
    }

    /* Slider drag -- mouse is captured, bypass normal hit test */
    if (host->focused_control >= 0 && host->focused_control < host->control_count) {
        SuiControl* sl = &host->controls[host->focused_control];
        if (sl->type == SUI_CTRL_SLIDER && sl->d.slider.dragging) {
            sui_ctrl_slider_update_from_mouse(sl, (float)mx);
            if (host->on_click) host->on_click(sl->id, host->on_click_data);
            InvalidateRect(host->hwnd, NULL, FALSE);
            return;
        }
    }

    /* Edit-mode drag selection — mouse is captured; at most one branch fires. */
    if (host->edit_dragging && host->focused_control >= 0 && host->focused_control < host->control_count) {
        SuiControl* fc = &host->controls[host->focused_control];
        if (fc->type == SUI_CTRL_TEXTVIEW) {
            int pos = sui__textview_hit_point(host, fc,
                (float)mx - fc->x, (float)my - fc->y);
            fc->d.textview.sel_start = fc->d.textview.drag_sel_anchor;
            fc->d.textview.sel_end   = pos;
            fc->d.textview.cursor    = pos;
            sui__textview_scroll_to_caret(host, fc);
            InvalidateRect(host->hwnd, NULL, FALSE);
            return;
        } else if (fc->type == SUI_CTRL_SPINNER && fc->d.spinner.editing) {
            wchar_t val_buf[34];
            int len = fc->d.spinner.edit_len < 32 ? fc->d.spinner.edit_len : 31;
            float arrow_w = fc->w * 0.30f;
            float text_w  = fc->w - arrow_w;
            int ii, pos;
            for (ii = 0; ii < len; ii++) val_buf[ii] = fc->d.spinner.edit_buf[ii];
            val_buf[len] = L'\0';
            pos = sui__spinner_hit_pos(host->renderer, fc, val_buf, text_w, (float)mx);
            fc->d.spinner.sel_start = fc->d.spinner.drag_anchor;
            fc->d.spinner.sel_end   = pos;
            fc->d.spinner.cursor    = pos;
            InvalidateRect(host->hwnd, NULL, FALSE);
            return;
        } else if (fc->type == SUI_CTRL_EDIT) {
            float pad_e   = sui_ctrl_edit_text_pad(host);
            float click_x = (float)mx - fc->x - pad_e + fc->d.edit.scroll_x;
            int   len_e   = (int)wcslen(fc->text);
            int   best    = 0;
            float best_d  = click_x < 0.0f ? -click_x : click_x;
            int   j;
            for (j = 1; j <= len_e; j++) {
                float edge = sui_ctrl_edit_text_width(host->renderer, fc, fc->text, j);
                float dist = edge - click_x;
                if (dist < 0.0f) dist = -dist;
                if (dist < best_d) { best_d = dist; best = j; }
            }
            fc->d.edit.sel_start = fc->d.edit.drag_sel_anchor;
            fc->d.edit.sel_end   = best;
            fc->d.edit.cursor    = best;
            sui_ctrl_edit_scroll_to_caret(host, fc);
            InvalidateRect(host->hwnd, NULL, FALSE);
            return;
        }
    }

    hit = sui_ctrl_hit_test(host, (float)mx, (float)my);

    if (host->spinner_repeat_control) {
        int in_up = 0;
        if (hit != host->spinner_repeat_control - 1 ||
            !sui_ctrl_spinner_arrow_hit(&host->controls[hit], sui_ctrl_dpi_scale(host),
                                       (float)mx, (float)my, &in_up) ||
            (in_up ? 1 : -1) != host->spinner_repeat_direction)
            sui_ctrl_spinner_stop_repeat(host);
    }

    for (i = 0; i < host->control_count; i++) {
        if ((host->controls[i].state & SUI_CTRL_HOVER) && i != hit) {
            host->controls[i].state &= ~SUI_CTRL_HOVER;
            changed = 1;
        }
    }
    if (hit >= 0 && !(host->controls[hit].state & SUI_CTRL_HOVER)) {
        host->controls[hit].state |= SUI_CTRL_HOVER;
        changed = 1;
    }

    if (host->hover_control != hit) {
        host->hover_control = hit;
        changed = 1;
    }

    sui_ctrl_tooltip_update(host, sui_ctrl_tooltip_hit_test(host, (float)mx, (float)my));

    /* Spinner sub-control hover */
    { float s_sp = sui_ctrl_dpi_scale(host);
      for (i = 0; i < host->control_count; i++)
          if (host->controls[i].type == SUI_CTRL_SPINNER)
              if (sui_ctrl_spinner_update_hover(&host->controls[i], i == hit,
                                                (float)mx, (float)my, s_sp))
                  changed = 1;
    }

    /* Nav list item hover */
    for (i = 0; i < host->control_count; i++)
        if (host->controls[i].type == SUI_CTRL_NAV_LIST)
            if (sui_ctrl_nav_update_hover(&host->controls[i], i == hit, (float)my))
                changed = 1;

    /* Generic list item hover */
    for (i = 0; i < host->control_count; i++)
        if (host->controls[i].type == SUI_CTRL_LIST)
            if (sui_ctrl_list_update_hover(&host->controls[i], i == hit, (float)my))
                changed = 1;

    /* Horizontal tab item hover */
    for (i = 0; i < host->control_count; i++)
        if (host->controls[i].type == SUI_CTRL_TABS)
            if (sui_ctrl_tabs_update_hover(host, &host->controls[i], i == hit, (float)mx, (float)my))
                changed = 1;

    if (changed) {
        sui_ctrl_start_hover_anim(host);
        InvalidateRect(host->hwnd, NULL, FALSE);
    }
}

/* ---- Left button down --------------------------------------------------- */

static void sui_ctrl_on_lbutton_down(SuiControlHost* host, int mx, int my) {
    int hit = sui_ctrl_hit_test(host, (float)mx, (float)my);
    sui_ctrl_spinner_stop_repeat(host);
    host->keyboard_focus = 0;  /* mouse click clears keyboard focus ring */
    if (sui_ctrl_shortcut_on_mouse_down(host, SUI_SHORTCUT_MOUSE_LEFT)) return;
    if (hit < 0) {
        { int open_ci = sui_combo_find_open(host);
          if (open_ci >= 0)
              sui_combo_popup_close(host);
        }
        if (host->focused_control >= 0) {
            sui_ctrl_set_focus(host, -1);
            InvalidateRect(host->hwnd, NULL, FALSE);
        }
        return;
    }

    if (host->controls[hit].type == SUI_CTRL_TEXTVIEW) {
        SuiControl* c = &host->controls[hit];
        float s = sui_ctrl_dpi_scale(host), sbw = 10.0f * s;
        float sb_x = c->x + c->w - sbw - 2.0f * s;
        if ((float)mx >= sb_x && c->d.textview.content_height > c->h - 16.0f * s) {
            host->dragging_scrollbar = hit + 1;
            host->drag_start_y       = (float)my;
            host->drag_start_scroll  = c->d.textview.scroll_offset;
            SetCapture(host->hwnd);
            return;
        }
        sui_ctrl_set_focus(host, hit);
        sui_ctrl_textview_on_lbutton_down(host, hit, mx, my);
        host->edit_dragging = 1;
        SetCapture(host->hwnd);
        InvalidateRect(host->hwnd, NULL, FALSE);
        return;
    }

    /* Scrollview: hit_test_scope returns the scrollview index when the cursor
     * is over the scrollbar thumb. Start a drag. Anchor and start-scroll live
     * in d.scroll so they don't alias textview's host->drag_start_* fields. */
    if (host->controls[hit].type == SUI_CTRL_SCROLLVIEW) {
        SuiControl* c = &host->controls[hit];
        c->d.scroll.track_dragging    = 1;
        c->d.scroll.track_drag_anchor = (float)my;
        c->d.scroll.drag_start_scroll = c->d.scroll.scroll_y;
        SetCapture(host->hwnd);
        InvalidateRect(host->hwnd, NULL, FALSE);
        return;
    }

    /* Slider: start drag immediately on any click within the control */
    if (host->controls[hit].type == SUI_CTRL_SLIDER) {
        SuiControl* c = &host->controls[hit];
        sui_ctrl_set_focus(host, hit);
        c->d.slider.dragging = 1;
        sui_ctrl_slider_update_from_mouse(c, (float)mx);
        if (host->on_click) host->on_click(c->id, host->on_click_data);
        SetCapture(host->hwnd);
        InvalidateRect(host->hwnd, NULL, FALSE);
        return;
    }

    /* Spinner: text-area click enters edit mode + positions cursor;
     * arrow click steps immediately and repeats while held */
    if (host->controls[hit].type == SUI_CTRL_SPINNER) {
        SuiControl* c = &host->controls[hit];
        int in_up = 0;
        sui_ctrl_set_focus(host, hit);
        /* A focus callback can rebuild the page; do not use a replaced slot. */
        if (hit >= host->control_count || host->focused_control != hit ||
            host->controls[hit].type != SUI_CTRL_SPINNER) return;
        if (sui_ctrl_spinner_arrow_hit(c, sui_ctrl_dpi_scale(host), (float)mx, (float)my, &in_up)) {
            sui_ctrl_spinner_start_repeat(host, hit, in_up);
            return;
        } else {
            wchar_t val_buf[34];
            int len, ii, pos;
            float arrow_w = c->w * 0.30f;
            float text_w  = c->w - arrow_w;
            if (!c->d.spinner.editing) sui_ctrl_spinner_begin_edit(c);
            len = c->d.spinner.edit_len < 32 ? c->d.spinner.edit_len : 31;
            for (ii = 0; ii < len; ii++) val_buf[ii] = c->d.spinner.edit_buf[ii];
            val_buf[len] = L'\0';
            pos = sui__spinner_hit_pos(host->renderer, c, val_buf, text_w, (float)mx);
            c->d.spinner.cursor      = pos;
            c->d.spinner.sel_start   = pos;
            c->d.spinner.sel_end     = pos;
            c->d.spinner.drag_anchor = pos;
            host->edit_dragging      = 1;
        }
        SetCapture(host->hwnd);
        c->state |= SUI_CTRL_PRESSED;
        InvalidateRect(host->hwnd, NULL, FALSE);
        return;
    }

    /* Close any open combobox popup when clicking outside it */
    { int open_ci = sui_combo_find_open(host);
      if (open_ci >= 0 && hit != open_ci)
          sui_combo_popup_close(host);
    }

    if (host->controls[hit].type == SUI_CTRL_LIST &&
            sui_ctrl_list_hit_thumb(host, &host->controls[hit], (float)mx, (float)my)) {
        SuiControl* list = &host->controls[hit];
        list->d.list.sb_dragging = 1;
        list->d.list.sb_drag_anchor = (float)my;
        list->d.list.sb_drag_start_scroll = list->d.list.scroll_offset;
        list->d.list.sb_reveal = 1.0f;
        SetCapture(host->hwnd);
        InvalidateRect(host->hwnd, NULL, FALSE);
        return;
    }

    /* Combobox: toggle popup open/close */
    if (host->controls[hit].type == SUI_CTRL_COMBOBOX) {
        SuiControl* c = &host->controls[hit];
        sui_ctrl_set_focus(host, hit);
        if (c->d.combo.open) {
            sui_combo_popup_close(host);
        } else {
            sui_combo_popup_open(host, hit);
        }
        InvalidateRect(host->hwnd, NULL, FALSE);
        return;
    }

    host->controls[hit].state |= SUI_CTRL_PRESSED;
    sui_ctrl_set_focus(host, hit);
    if (host->controls[hit].type == SUI_CTRL_EDIT) {
        sui_ctrl_edit_on_lbutton_down(host, hit, mx, my);
        host->edit_dragging = 1;
    }
    SetCapture(host->hwnd);
    InvalidateRect(host->hwnd, NULL, FALSE);
}

/* ---- Left button up ----------------------------------------------------- */

static void sui_ctrl_on_lbutton_up(SuiControlHost* host, int mx, int my) {
    int i, hit;

    sui_ctrl_spinner_stop_repeat(host);
    host->edit_dragging = 0;
    if (sui_ctrl_shortcut_on_mouse_up(host, SUI_SHORTCUT_MOUSE_LEFT)) return;

    /* End slider drag */
    if (host->focused_control >= 0 && host->focused_control < host->control_count) {
        SuiControl* sl = &host->controls[host->focused_control];
        if (sl->type == SUI_CTRL_SLIDER && sl->d.slider.dragging) {
            sl->d.slider.dragging = 0;
            sl->state &= ~SUI_CTRL_PRESSED;
            ReleaseCapture();
            if (host->on_click)
                host->on_click(sl->id, host->on_click_data);
            InvalidateRect(host->hwnd, NULL, FALSE);
            return;
        }
    }

    {
        int j, dragging = 0;
        for (j = 0; j < host->control_count; j++) {
            SuiControl* list = &host->controls[j];
            if (list->type == SUI_CTRL_LIST && list->d.list.sb_dragging) {
                list->d.list.sb_dragging = 0;
                list->d.list.sb_reveal = 1.0f;
                dragging = 1;
            }
        }
        if (dragging) {
            ReleaseCapture();
            sui_ctrl_start_hover_anim(host);
            InvalidateRect(host->hwnd, NULL, FALSE);
            return;
        }
    }

    /* End any in-flight scrollview thumb drag before generic dispatch. */
    {
        int j, dragging = 0;
        for (j = 0; j < host->control_count; j++) {
            SuiControl* sv = &host->controls[j];
            if (sv->type == SUI_CTRL_SCROLLVIEW && sv->d.scroll.track_dragging) {
                sv->d.scroll.track_dragging = 0;
                dragging = 1;
            }
        }
        if (dragging) {
            ReleaseCapture();
            InvalidateRect(host->hwnd, NULL, FALSE);
            return;
        }
    }

    ReleaseCapture();

    if (host->dragging_scrollbar) {
        host->dragging_scrollbar = 0;
        return;
    }

    hit = sui_ctrl_hit_test(host, (float)mx, (float)my);

    for (i = 0; i < host->control_count; i++) {
        if (host->controls[i].state & SUI_CTRL_PRESSED) {
            /* An open combobox is a persistent pressed state, not the
             * transient pointer-down state cleared by mouse-up. Its popup
             * teardown clears the flag after the close animation. */
            if (host->controls[i].type == SUI_CTRL_COMBOBOX &&
                host->controls[i].d.combo.open)
                continue;
            host->controls[i].state &= ~SUI_CTRL_PRESSED;
            if (host->controls[i].type == SUI_CTRL_TABS)
                sui_ctrl_start_hover_anim(host);
            if (i == hit) {
                if (host->controls[i].type == SUI_CTRL_BUTTON) {
                    if (host->on_click)
                        host->on_click(host->controls[i].id, host->on_click_data);
                } else if (host->controls[i].type == SUI_CTRL_CHECKBOX) {
                    host->controls[i].state ^= SUI_CTRL_CHECKED;
                    if (host->on_click)
                        host->on_click(host->controls[i].id, host->on_click_data);
                } else if (host->controls[i].type == SUI_CTRL_RADIO) {
                    sui_ctrl_radio_select(host, i);
                    if (host->on_click)
                        host->on_click(host->controls[i].id, host->on_click_data);
                } else if (host->controls[i].type == SUI_CTRL_COLOR_SWATCH) {
                    if (sui__swatch_pick_color(host, &host->controls[i]) && host->on_click)
                        host->on_click(host->controls[i].id, host->on_click_data);
                } else if (host->controls[i].type == SUI_CTRL_SHORTCUT_RECORDER) {
                    sui_ctrl_shortcut_activate(host, &host->controls[i]);
                } else if (host->controls[i].type == SUI_CTRL_NAV_LIST) {
                    SuiControl* nav = &host->controls[i];
                    float ih = nav->d.nav.item_height;
                    int idx  = (int)(((float)my - nav->y + nav->d.nav.scroll_offset) / ih);
                    if (idx >= 0 && idx < nav->d.nav.item_count && idx != nav->d.nav.selected) {
                        nav->d.nav.content_wheel_delta = 0;
                        nav->d.nav.selected = idx;
                        if (host->on_click)
                            host->on_click(nav->id, host->on_click_data);
                    }
                } else if (host->controls[i].type == SUI_CTRL_LIST) {
                    SuiControl* list = &host->controls[i];
                    int row = sui_ctrl_list_row_at(list, (float)my);
                    SuiListRow row_view;
                    memset(&row_view, 0, sizeof(row_view));
                    if (row >= 0)
                        sui_ctrl_list_get_row_view(list, row, &row_view);
                    if (row >= 0 && row_view.kind == SUI_LIST_ROW_ITEM &&
                            row_view.item_index >= 0 && row != list->d.list.selected_row) {
                        list->d.list.selected_row = row;
                        if (host->on_click)
                            host->on_click(list->id, host->on_click_data);
                    }
                } else if (host->controls[i].type == SUI_CTRL_TABS) {
                    SuiControl* tabs = &host->controls[i];
                    int idx = sui_ctrl_tabs_item_at(host, tabs, (float)mx, (float)my);
                    if (idx >= 0 && idx < tabs->d.tabs.item_count && idx != tabs->d.tabs.selected) {
                        int old_selected = tabs->d.tabs.selected;
                        int dir = idx < old_selected ? 1 : -1;
                        sui_ctrl_tabs_start_content_transition(host, tabs, old_selected, idx, dir);
                        tabs->d.tabs.selected = idx;
                        sui_ctrl_tabs_start_indicator_transition(host, tabs);
                        sui_ctrl_start_hover_anim(host);
                        if (host->on_click)
                            host->on_click(tabs->id, host->on_click_data);
                    }
                }
            }
        }
    }
    InvalidateRect(host->hwnd, NULL, FALSE);
}

static int sui_ctrl_on_shortcut_mouse_button_down(SuiControlHost* host, UINT mouse_button) {
    if (sui_ctrl_shortcut_on_mouse_down(host, mouse_button)) return 1;
    return 0;
}

static int sui_ctrl_on_shortcut_mouse_button_up(SuiControlHost* host, UINT mouse_button) {
    if (sui_ctrl_shortcut_on_mouse_up(host, mouse_button)) return 1;
    return 0;
}

/* ---- Left button double-click ------------------------------------------- */

static void sui_ctrl_on_lbutton_dblclk(SuiControlHost* host, int mx, int my) {
    int hit = sui_ctrl_hit_test(host, (float)mx, (float)my);
    if (hit < 0) return;
    if (host->controls[hit].type == SUI_CTRL_SPINNER) {
        int in_up;
        if (sui_ctrl_spinner_arrow_hit(&host->controls[hit], sui_ctrl_dpi_scale(host),
                                      (float)mx, (float)my, &in_up))
            sui_ctrl_on_lbutton_down(host, mx, my);
        return;
    }
    if (host->controls[hit].type == SUI_CTRL_EDIT) {
        sui_ctrl_set_focus(host, hit);
        sui_ctrl_edit_on_lbutton_dblclk(host, hit, mx, my);
        sui_ctrl_edit_scroll_to_caret(host, &host->controls[hit]);
        InvalidateRect(host->hwnd, NULL, FALSE);
    } else if (host->controls[hit].type == SUI_CTRL_TEXTVIEW) {
        sui_ctrl_set_focus(host, hit);
        sui_ctrl_textview_on_lbutton_dblclk(host, hit, mx, my);
        InvalidateRect(host->hwnd, NULL, FALSE);
    } else if (host->controls[hit].type != SUI_CTRL_SHORTCUT_RECORDER) {
        /* Only text has a double click of its own, so to anything else this is one more click. Not to a recorder: its first click
         * started recording, and this one would become the shortcut. */
        sui_ctrl_on_lbutton_down(host, mx, my);
    }
}

/* ---- Right button up / context menu ------------------------------------- */

static void sui_ctrl_on_rbutton_up(SuiControlHost* host, int mx, int my) {
    int hit = sui_ctrl_hit_test(host, (float)mx, (float)my);
    if (hit < 0) return;
    if (host->controls[hit].type != SUI_CTRL_EDIT
            && host->controls[hit].type != SUI_CTRL_TEXTVIEW) return;

    sui_ctrl_set_focus(host, hit);
    InvalidateRect(host->hwnd, NULL, FALSE);

    { SuiControl* c = &host->controls[hit];
      int is_tv    = (c->type == SUI_CTRL_TEXTVIEW);
      int readonly = is_tv ? c->d.textview.readonly : 0;
      int has_sel  = is_tv ? sui_ctrl_textview_has_sel(c)
                  :         sui_ctrl_edit_has_sel(c);
      int has_text = is_tv ? (c->d.textview.len > 0)
                  :         (c->text[0] != L'\0');
      int has_clip = IsClipboardFormatAvailable(CF_UNICODETEXT);
      HMENU menu   = CreatePopupMenu();
      POINT pt     = { mx, my };
      UINT  cmd;

      AppendMenuW(menu, MF_STRING | ((has_sel && !readonly) ? 0 : MF_GRAYED), 1001, L"Cut");
      AppendMenuW(menu, MF_STRING | ((has_sel || has_text)  ? 0 : MF_GRAYED), 1002, L"Copy");
      AppendMenuW(menu, MF_STRING | ((has_clip && !readonly) ? 0 : MF_GRAYED), 1003, L"Paste");

      ClientToScreen(host->hwnd, &pt);
      cmd = (UINT)TrackPopupMenu(menu,
          TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY,
          pt.x, pt.y, 0, host->hwnd, NULL);
      DestroyMenu(menu);

      if (is_tv) {
          if      (cmd == 1001) sui_ctrl_textview_cut  (host, c);
          else if (cmd == 1002) sui_ctrl_textview_copy (host, c);
          else if (cmd == 1003) sui_ctrl_textview_paste(host, c);
          if (cmd == 1001 || cmd == 1003) {
              sui__textview_update_content_height(host, c);
              sui__textview_scroll_to_caret(host, c);
              host->caret_visible = 1;
          }
      } else {
          if      (cmd == 1001) sui_ctrl_edit_cut  (host, c);
          else if (cmd == 1002) sui_ctrl_edit_copy (host, c);
          else if (cmd == 1003) sui_ctrl_edit_paste(host, c);
          if (cmd == 1001 || cmd == 1003) {
              sui_ctrl_edit_scroll_to_caret(host, c);
              host->caret_visible = 1;
          }
      }
      if (cmd) InvalidateRect(host->hwnd, NULL, FALSE);
    }
}

/* ---- Mouse wheel -------------------------------------------------------- */

static int sui__ctrl_rects_overlap(float ax, float ay, float aw, float ah,
                                   float bx, float by, float bw, float bh) {
    return aw > 0.0f && ah > 0.0f && bw > 0.0f && bh > 0.0f
        && ax < bx + bw && ax + aw > bx
        && ay < by + bh && ay + ah > by;
}

/* Return the current vertical overflow in pixels for controls that own a
 * wheel-scroll range. A non-positive result means there is no scrolling to
 * protect from content-wheel navigation. */
static float sui__ctrl_vertical_overflow(SuiControlHost* host, SuiControl* c) {
    float visible_h;
    if (!host || !c) return 0.0f;
    switch (c->type) {
    case SUI_CTRL_TEXTVIEW:
        visible_h = c->h - 16.0f * sui_ctrl_dpi_scale(host);
        return c->d.textview.content_height - visible_h;
    case SUI_CTRL_NAV_LIST:
        return (float)c->d.nav.item_count * c->d.nav.item_height - c->h;
    case SUI_CTRL_LIST:
        return sui_ctrl_list_content_height(c) - c->h;
    case SUI_CTRL_SCROLLVIEW:
        return c->d.scroll.content_h - c->h;
    default:
        return 0.0f;
    }
}

static int sui__ctrl_is_effectively_hidden(SuiControlHost* host, SuiControl* c) {
    int parent_id;
    int depth = 0;
    if (!host || !c || (c->state & SUI_CTRL_HIDDEN)) return 1;
    parent_id = c->parent_id;
    while (parent_id >= 0 && depth++ < SUI_MAX_CONTROLS) {
        SuiControl* parent = sui_ctrl_find(host, parent_id);
        if (!parent) break;
        if (parent->state & SUI_CTRL_HIDDEN) return 1;
        parent_id = parent->parent_id;
    }
    return 0;
}

static int sui__ctrl_nav_content_has_vertical_overflow(SuiControlHost* host,
                                                        SuiControl* nav) {
    int i;
    for (i = 0; i < host->control_count; ++i) {
        SuiControl* c = &host->controls[i];
        if (c == nav) continue;
        if (!sui__ctrl_rects_overlap(nav->d.nav.content_wheel_x,
                                     nav->d.nav.content_wheel_y,
                                     nav->d.nav.content_wheel_w,
                                     nav->d.nav.content_wheel_h,
                                     c->x, c->y, c->w, c->h)) continue;
        if (sui__ctrl_vertical_overflow(host, c) <= 0.0f) continue;
        if (!sui__ctrl_is_effectively_hidden(host, c)) return 1;
    }
    return 0;
}

/* Select one adjacent nav item after a complete wheel detent over an opted-in
 * content region. This is deliberately a fallback: any visible overflowing
 * vertical control anywhere in the region disables it, even if the pointer is
 * currently over an empty part of the page. */
static void sui__ctrl_nav_on_content_wheel(SuiControlHost* host,
                                            int mx, int my, int delta) {
    SuiControl* nav = NULL;
    int nav_index = -1;
    int i;
    int next;
    if (!host || delta == 0) return;
    for (i = 0; i < host->control_count; ++i) {
        SuiControl* candidate = &host->controls[i];
        if (candidate->type != SUI_CTRL_NAV_LIST ||
            !candidate->d.nav.content_wheel_enabled ||
            (candidate->state & (SUI_CTRL_HIDDEN | SUI_CTRL_DISABLED))) continue;
        if ((float)mx < candidate->d.nav.content_wheel_x ||
            (float)mx >= candidate->d.nav.content_wheel_x + candidate->d.nav.content_wheel_w ||
            (float)my < candidate->d.nav.content_wheel_y ||
            (float)my >= candidate->d.nav.content_wheel_y + candidate->d.nav.content_wheel_h) continue;
        if (!nav || candidate->z > nav->z ||
            (candidate->z == nav->z && i > nav_index)) {
            nav = candidate;
            nav_index = i;
        }
    }
    if (!nav) return;
    if (sui__ctrl_nav_content_has_vertical_overflow(host, nav)) {
        nav->d.nav.content_wheel_delta = 0;
        return;
    }

    /* High-resolution wheels may deliver fractions of WHEEL_DELTA. Do not let
     * opposite directions cancel stale partial input from an earlier gesture. */
    if ((nav->d.nav.content_wheel_delta < 0 && delta > 0) ||
        (nav->d.nav.content_wheel_delta > 0 && delta < 0)) {
        nav->d.nav.content_wheel_delta = 0;
    }
    nav->d.nav.content_wheel_delta += delta;
    if (nav->d.nav.content_wheel_delta > -WHEEL_DELTA &&
        nav->d.nav.content_wheel_delta < WHEEL_DELTA) return;

    next = nav->d.nav.selected + (nav->d.nav.content_wheel_delta > 0 ? -1 : 1);
    nav->d.nav.content_wheel_delta = 0;
    if (next < 0 || next >= nav->d.nav.item_count || next == nav->d.nav.selected) return;

    nav->d.nav.selected = next;
    InvalidateRect(host->hwnd, NULL, FALSE);
    if (host->on_click) host->on_click(nav->id, host->on_click_data);
}

static void sui_ctrl_on_mouse_wheel(SuiControlHost* host, int mx, int my, int delta) {
    int hit = sui_ctrl_hit_test(host, (float)mx, (float)my);
    if (hit >= 0 && host->controls[hit].type == SUI_CTRL_TEXTVIEW) {
        SuiControl* c = &host->controls[hit];
        float s = sui_ctrl_dpi_scale(host);
        float scroll_amount = 40.0f * s;
        float visible_h = c->h - 16.0f * s;
        float max_scroll = c->d.textview.content_height - visible_h;
        c->d.textview.scroll_offset -= (float)delta / 120.0f * scroll_amount;
        if (max_scroll < 0.0f) max_scroll = 0.0f;
        if (c->d.textview.scroll_offset < 0.0f)        c->d.textview.scroll_offset = 0.0f;
        if (c->d.textview.scroll_offset > max_scroll)  c->d.textview.scroll_offset = max_scroll;
        c->d.textview.sb_reveal = 1.0f;
        sui_ctrl_start_hover_anim(host);
        InvalidateRect(host->hwnd, NULL, FALSE);
    }
    if (hit >= 0 && host->controls[hit].type == SUI_CTRL_NAV_LIST) {
        SuiControl* nav = &host->controls[hit];
        float scroll_amount = nav->d.nav.item_height;
        float total_h       = (float)nav->d.nav.item_count * scroll_amount;
        float max_scroll    = total_h - nav->h;
        if (max_scroll > 0.0f) {
            nav->d.nav.scroll_offset -= (float)delta / 120.0f * scroll_amount;
            if (nav->d.nav.scroll_offset < 0.0f) nav->d.nav.scroll_offset = 0.0f;
            if (nav->d.nav.scroll_offset > max_scroll) nav->d.nav.scroll_offset = max_scroll;
            InvalidateRect(host->hwnd, NULL, FALSE);
        }
    }
    if (hit >= 0 && host->controls[hit].type == SUI_CTRL_LIST) {
        SuiControl* list = &host->controls[hit];
        float scroll_amount = list->d.list.item_height > 0.0f ? list->d.list.item_height : 40.0f * sui_ctrl_dpi_scale(host);
        float max_scroll = sui_ctrl_list_content_height(list) - list->h;
        if (max_scroll < 0.0f) max_scroll = 0.0f;
        if (max_scroll > 0.0f) {
            list->d.list.scroll_offset -= (float)delta / 120.0f * scroll_amount;
            if (list->d.list.scroll_offset < 0.0f) list->d.list.scroll_offset = 0.0f;
            if (list->d.list.scroll_offset > max_scroll) list->d.list.scroll_offset = max_scroll;
            list->d.list.sb_reveal = 1.0f;
            sui_ctrl_start_hover_anim(host);
            InvalidateRect(host->hwnd, NULL, FALSE);
        }
    }
    /* Scrollview: scan all SUI_CTRL_SCROLLVIEW controls; pick the topmost-z
     * one whose rect contains the cursor. Hit-test alone returns -1 when the
     * cursor is over empty content area, so we look up scrollviews directly.
     * Skip when an earlier branch already consumed or owns the wheel target
     * (textview / nav_list / tabs); otherwise a control nested inside a scrollview would scroll
     * both at once. */
    if (hit < 0 || (host->controls[hit].type != SUI_CTRL_TEXTVIEW
                 && host->controls[hit].type != SUI_CTRL_NAV_LIST
                 && host->controls[hit].type != SUI_CTRL_LIST
                 && host->controls[hit].type != SUI_CTRL_TABS)) {
        int j, best = -1, best_z = 0;
        for (j = 0; j < host->control_count; j++) {
            SuiControl* sv = &host->controls[j];
            if (sv->type != SUI_CTRL_SCROLLVIEW) continue;
            if (sv->state & (SUI_CTRL_HIDDEN | SUI_CTRL_DISABLED)) continue;
            if ((float)mx < sv->x || (float)mx >= sv->x + sv->w
                    || (float)my < sv->y || (float)my >= sv->y + sv->h) continue;
            if (best < 0 || sv->z >= best_z) { best = j; best_z = sv->z; }
        }
        if (best >= 0) {
            SuiControl* sv = &host->controls[best];
            float s = sui_ctrl_dpi_scale(host);
            float scroll_amount = 40.0f * s;
            float max_scroll = sv->d.scroll.content_h - sv->h;
            if (max_scroll < 0.0f) max_scroll = 0.0f;
            sv->d.scroll.scroll_y -= (float)delta / 120.0f * scroll_amount;
            if (sv->d.scroll.scroll_y < 0.0f)        sv->d.scroll.scroll_y = 0.0f;
            if (sv->d.scroll.scroll_y > max_scroll)  sv->d.scroll.scroll_y = max_scroll;
            InvalidateRect(host->hwnd, NULL, FALSE);
        }
    }
    sui__ctrl_nav_on_content_wheel(host, mx, my, delta);
}

/* ---- Key down ----------------------------------------------------------- */

static void sui_ctrl_on_keydown_ex(SuiControlHost* host, WPARAM vk, LPARAM lp) {
    /* Let the focused edit control handle the key first */
    { int fc = host->focused_control;
      if (fc >= 0 && fc < host->control_count && host->controls[fc].type == SUI_CTRL_EDIT) {
          if (sui_ctrl_edit_on_keydown(host, fc, vk)) return;
      }
      if (fc >= 0 && fc < host->control_count && host->controls[fc].type == SUI_CTRL_TEXTVIEW) {
          if (sui_ctrl_textview_on_keydown(host, fc, vk)) return;
      }
      if (fc >= 0 && fc < host->control_count && host->controls[fc].type == SUI_CTRL_SHORTCUT_RECORDER) {
          if (sui_ctrl_shortcut_on_keydown(host, fc, vk, lp)) return;
      }
    }

    /* Per-control key dispatch */
    if (host->focused_control >= 0 && host->focused_control < host->control_count) {
        SuiControl* c = &host->controls[host->focused_control];
        if (c->type == SUI_CTRL_SLIDER   && sui_ctrl_slider_on_keydown(host, c, vk))  return;
        if (c->type == SUI_CTRL_SPINNER  && sui_ctrl_spinner_on_keydown(host, c, vk)) return;
        if (c->type == SUI_CTRL_COMBOBOX && sui_ctrl_combo_on_keydown(host, c, vk))   return;
        if (c->type == SUI_CTRL_NAV_LIST && sui_ctrl_nav_on_keydown(host, c, vk))     return;
        if (c->type == SUI_CTRL_LIST     && sui_ctrl_list_on_keydown(host, c, vk))    return;
        if (c->type == SUI_CTRL_TABS) {
            int next, dir;
            if (sui_ctrl_tabs_key_target(c, vk, &next, &dir)) {
                int old_selected = c->d.tabs.selected;
                sui_ctrl_tabs_start_content_transition(host, c, old_selected, next, dir);
                c->d.tabs.selected = next;
                sui_ctrl_tabs_start_indicator_transition(host, c);
                if (host->on_click) host->on_click(c->id, host->on_click_data);
                InvalidateRect(host->hwnd, NULL, FALSE);
                return;
            }
        }
        if (c->type == SUI_CTRL_TABS     && sui_ctrl_tabs_on_keydown(host, c, vk))    {
            return;
        }
    }

    switch (vk) {
    case VK_TAB:
        /* Close any open combobox popup before tabbing away */
        { int open_ci = sui_combo_find_open(host);
          if (open_ci >= 0) sui_combo_popup_close(host);
        }
        host->keyboard_focus = 1;
        if (GetKeyState(VK_SHIFT) & 0x8000)
            sui_ctrl_set_focus(host, sui_ctrl_next_focusable(host, host->focused_control, -1));
        else
            sui_ctrl_set_focus(host, sui_ctrl_next_focusable(host, host->focused_control, 1));
        InvalidateRect(host->hwnd, NULL, FALSE);
        break;
    case VK_SPACE:
        if (host->focused_control >= 0 && host->focused_control < host->control_count) {
            SuiControl* c = &host->controls[host->focused_control];
            if (c->type == SUI_CTRL_BUTTON) {
                c->state |= SUI_CTRL_PRESSED;
                InvalidateRect(host->hwnd, NULL, FALSE);
            } else if (c->type == SUI_CTRL_CHECKBOX) {
                c->state ^= SUI_CTRL_CHECKED;
                if (host->on_click)
                    host->on_click(c->id, host->on_click_data);
                InvalidateRect(host->hwnd, NULL, FALSE);
            }
        }
        break;
    case VK_LEFT:
    case VK_UP:
        if (host->focused_control >= 0 && host->focused_control < host->control_count &&
            host->controls[host->focused_control].type == SUI_CTRL_RADIO) {
            int fc = host->focused_control;
            int nxt = sui_ctrl_radio_nav(host, fc, -1);
            if (nxt != fc) {
                sui_ctrl_radio_select(host, nxt);
                sui_ctrl_set_focus(host, nxt);
                if (host->on_click)
                    host->on_click(host->controls[nxt].id, host->on_click_data);
                InvalidateRect(host->hwnd, NULL, FALSE);
            }
        }
        break;
    case VK_RIGHT:
    case VK_DOWN:
        if (host->focused_control >= 0 && host->focused_control < host->control_count &&
            host->controls[host->focused_control].type == SUI_CTRL_RADIO) {
            int fc = host->focused_control;
            int nxt = sui_ctrl_radio_nav(host, fc, 1);
            if (nxt != fc) {
                sui_ctrl_radio_select(host, nxt);
                sui_ctrl_set_focus(host, nxt);
                if (host->on_click)
                    host->on_click(host->controls[nxt].id, host->on_click_data);
                InvalidateRect(host->hwnd, NULL, FALSE);
            }
        }
        break;
    case VK_RETURN: {
        int i;
        for (i = 0; i < host->control_count; i++) {
            if ((host->controls[i].state & SUI_CTRL_DEFAULT) && !(host->controls[i].state & SUI_CTRL_DISABLED)
                && !(host->controls[i].state & SUI_CTRL_HIDDEN) && host->controls[i].type == SUI_CTRL_BUTTON) {
                host->controls[i].state |= SUI_CTRL_PRESSED;
                InvalidateRect(host->hwnd, NULL, FALSE);
                break;
            }
        }
        break;
    }
    case VK_ESCAPE:
        if (host->on_click)
            host->on_click(IDCANCEL, host->on_click_data);
        break;
    }
}

static void sui_ctrl_on_keydown(SuiControlHost* host, WPARAM vk) {
    sui_ctrl_on_keydown_ex(host, vk, 0);
}

/* ---- Key up ------------------------------------------------------------- */

static void sui_ctrl_on_keyup(SuiControlHost* host, WPARAM vk) {
    if (host->focused_control >= 0 && host->focused_control < host->control_count) {
        SuiControl* c = &host->controls[host->focused_control];
        if (c->type == SUI_CTRL_SHORTCUT_RECORDER &&
            sui_ctrl_shortcut_on_keyup(host, host->focused_control, vk)) {
            return;
        }
    }
    if (vk == VK_SPACE && host->focused_control >= 0 && host->focused_control < host->control_count) {
        SuiControl* c = &host->controls[host->focused_control];
        if (c->type == SUI_CTRL_BUTTON && (c->state & SUI_CTRL_PRESSED)) {
            c->state &= ~SUI_CTRL_PRESSED;
            if (host->on_click)
                host->on_click(c->id, host->on_click_data);
            InvalidateRect(host->hwnd, NULL, FALSE);
        }
    }
    if (vk == VK_RETURN) {
        int i;
        for (i = 0; i < host->control_count; i++) {
            if ((host->controls[i].state & SUI_CTRL_DEFAULT) && (host->controls[i].state & SUI_CTRL_PRESSED)
                && host->controls[i].type == SUI_CTRL_BUTTON) {
                host->controls[i].state &= ~SUI_CTRL_PRESSED;
                if (host->on_click)
                    host->on_click(host->controls[i].id, host->on_click_data);
                InvalidateRect(host->hwnd, NULL, FALSE);
                break;
            }
        }
    }
}

/* ---- Char input --------------------------------------------------------- */

static void sui_ctrl_on_char(SuiControlHost* host, wchar_t ch) {
    int fc = host->focused_control;
    if (fc < 0 || fc >= host->control_count) return;
    if (host->controls[fc].type == SUI_CTRL_EDIT)
        sui_ctrl_edit_on_char(host, fc, ch);
    else if (host->controls[fc].type == SUI_CTRL_TEXTVIEW)
        sui_ctrl_textview_on_char(host, fc, ch);
    else if (host->controls[fc].type == SUI_CTRL_SPINNER)
        sui_ctrl_spinner_on_char(host, &host->controls[fc], ch);
}

/* ---- Cursor ------------------------------------------------------------- */

static HCURSOR sui_ctrl_get_cursor(SuiControlHost* host, int mx, int my) {
    int hit = sui_ctrl_hit_test(host, (float)mx, (float)my);
    if (hit >= 0) {
        switch (host->controls[hit].type) {
            case SUI_CTRL_EDIT:
            case SUI_CTRL_TEXTVIEW:
                return LoadCursorW(NULL, MAKEINTRESOURCEW(32513)); /* IDC_IBEAM */
            case SUI_CTRL_SHORTCUT_RECORDER:
                return LoadCursorW(NULL, MAKEINTRESOURCEW(32512)); /* IDC_ARROW */
            case SUI_CTRL_SPINNER: {
                /* ibeam only over the text area, not the arrow buttons */
                int dummy = 0;
                if (!sui_ctrl_spinner_arrow_hit(&host->controls[hit],
                        sui_ctrl_dpi_scale(host), (float)mx, (float)my, &dummy))
                    return LoadCursorW(NULL, MAKEINTRESOURCEW(32513)); /* IDC_IBEAM */
                break;
            }
            default:
                break;
        }
    }
    return LoadCursorW(NULL, MAKEINTRESOURCEW(32512)); /* IDC_ARROW */
}

/* ---- Host release ------------------------------------------------------- */

static void sui_ctrl_host_release(SuiControlHost* host) {
    int i;
    if (!host) return;
    sui_ctrl_spinner_stop_repeat(host);
    KillTimer(host->hwnd, SUI_CARET_TIMER_ID);
    KillTimer(host->hwnd, SUI_HOVER_TIMER_ID);
    KillTimer(host->hwnd, SUI_TOOLTIP_TIMER_ID);
    KillTimer(host->hwnd, SUI_SHORTCUT_RECORD_TIMER_ID);
#ifdef SUI_FEATURE_VSYNC
    if (host->hover_vsync_active) {
        sui_vsync_detach(&host->vsync, sui_ctrl_hover_vsync_tick, host);
        host->hover_vsync_active = 0;
    }
    sui_vsync_stop(&host->vsync);
#endif
    sui_ctrl_destroy_tooltip_popup(host);
#ifndef SUI_NO_COMBOBOX
    if (host->combo_popup) {
        SuiComboPopup* popup = host->combo_popup;
        if (popup->hwnd)
            sui_combo_popup_destroy_internal(host);
        else
            sui_combo_owner_subclass_uninstall(host);
        sui_combo_popup_free_icons(popup);
        sui_renderer_release(&popup->renderer);
        if (popup->shadow_hwnd) {
            DestroyWindow(popup->shadow_hwnd);
            popup->shadow_hwnd = NULL;
        }
        sui_free(popup);
        host->combo_popup = NULL;
    }
#endif
    /* Disconnect first so retained COM references become inert before host
     * storage can be released, then drop the host's cached reference. */
    sui_uia_disconnect_host_provider(host);
    /* Disconnect + release per-control providers via the helper in sui_uia_providers.h */
    sui_uia_disconnect_ctrl_providers(host, 0, SUI_MAX_CONTROLS);
    /* Release all cached IDWriteTextLayout* before the renderer tears down. */
    {
        void *lc_out[SUI_LAYOUT_CACHE_SLOTS];
        int j, n = sui__flush_layout_cache(&host->layout_cache, lc_out);
        typedef ULONG (STDMETHODCALLTYPE *RelFn)(void*);
        for (j = 0; j < n; j++)
            ((RelFn)(*(void***)lc_out[j])[2])(lc_out[j]);
    }
    /* Free heap-allocated buffers across every slot — covers nav/tabs/combo/textview.
     * The union lives in `controls[]` regardless of `control_count`; clear_page
     * may have moved slots out of the live range. */
    for (i = 0; i < SUI_MAX_CONTROLS; i++) {
        if (host->controls[i].type == SUI_CTRL_SHORTCUT_RECORDER)
            sui_ctrl_shortcut_cancel_recording(host, &host->controls[i], 0);
        sui_ctrl_textview_release(&host->controls[i]);
        sui_ctrl_nav_release(&host->controls[i]);
        sui_ctrl_list_release(&host->controls[i]);
        sui_ctrl_tabs_release(&host->controls[i]);
        sui_ctrl_combo_release(&host->controls[i]);
        sui_ctrl_edit_release(&host->controls[i]);
        sui_ctrl_style_release(&host->controls[i]);
    }
    sui_ctrl_tabs_release_host_transitions(host);
}

/* sui_default_msg — forward the ~15 Win32 messages that every slim-ui consumer
 * wires identically.  Call at the top of your WndProc before DefWindowProcW.
 *
 * Returns 1 if the message was handled and *result has been set; the caller
 * should return *result immediately.  Returns 0 to fall through.
 *
 * Usage:
 *   LRESULT r = 0;
 *   if (sui_default_msg(&host, hwnd, msg, wp, lp, &r)) return r;
 *   ...app-specific messages...
 *   return DefWindowProcW(hwnd, msg, wp, lp);
 *
 * WM_GETMINMAXINFO is intentionally excluded — it is only safe for
 * absolute-coordinate layouts.  Call sui_handle_wm_getminmaxinfo explicitly
 * where appropriate.  WM_PAINT / WM_SIZE / WM_DESTROY are also excluded — they
 * require app-specific work.  WM_DPICHANGED: call sui_handle_wm_dpichanged then
 * SetWindowPos + sui_renderer_resize + rebuild controls + InvalidateRect.
 */
static int sui_default_msg(SuiControlHost* host, HWND hwnd,
                           UINT msg, WPARAM wp, LPARAM lp, LRESULT* result) {
    POINT pt;
    TRACKMOUSEEVENT tme;
    switch (msg) {
    case WM_ERASEBKGND:
        *result = TRUE;
        return 1;
    case WM_MOUSEMOVE:
        sui_ctrl_on_mouse_move(host, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        SetCursor(sui_ctrl_get_cursor(host, GET_X_LPARAM(lp), GET_Y_LPARAM(lp)));
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hwnd;
        tme.dwHoverTime = 0;
        TrackMouseEvent(&tme);
        *result = 0; return 1;
    case WM_MOUSELEAVE:
        sui_ctrl_on_mouse_leave(host);
        *result = 0; return 1;
    case WM_LBUTTONDOWN:
        SetFocus(hwnd);
        sui_ctrl_on_lbutton_down(host, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        *result = 0; return 1;
    case WM_LBUTTONDBLCLK:
        SetFocus(hwnd);
        sui_ctrl_on_lbutton_dblclk(host, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        *result = 0; return 1;
    case WM_LBUTTONUP:
        sui_ctrl_on_lbutton_up(host, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        *result = 0; return 1;
    case WM_RBUTTONDOWN:
        SetFocus(hwnd);
        if (sui_ctrl_on_shortcut_mouse_button_down(host, SUI_SHORTCUT_MOUSE_RIGHT)) {
            *result = 0; return 1;
        }
        break;
    case WM_RBUTTONUP:
        if (sui_ctrl_on_shortcut_mouse_button_up(host, SUI_SHORTCUT_MOUSE_RIGHT)) {
            *result = 0; return 1;
        }
        break;
    case WM_MBUTTONDOWN:
        SetFocus(hwnd);
        if (sui_ctrl_on_shortcut_mouse_button_down(host, SUI_SHORTCUT_MOUSE_MIDDLE)) {
            *result = 0; return 1;
        }
        break;
    case WM_MBUTTONUP:
        if (sui_ctrl_on_shortcut_mouse_button_up(host, SUI_SHORTCUT_MOUSE_MIDDLE)) {
            *result = 0; return 1;
        }
        break;
    case WM_XBUTTONDOWN: {
        UINT mouse_button = (HIWORD(wp) == XBUTTON2) ? SUI_SHORTCUT_MOUSE_X2 : SUI_SHORTCUT_MOUSE_X1;
        SetFocus(hwnd);
        if (sui_ctrl_on_shortcut_mouse_button_down(host, mouse_button)) {
            *result = TRUE; return 1;
        }
        break;
    }
    case WM_XBUTTONUP: {
        UINT mouse_button = (HIWORD(wp) == XBUTTON2) ? SUI_SHORTCUT_MOUSE_X2 : SUI_SHORTCUT_MOUSE_X1;
        if (sui_ctrl_on_shortcut_mouse_button_up(host, mouse_button)) {
            *result = TRUE; return 1;
        }
        break;
    }
    case WM_CONTEXTMENU:
        if (host->focused_control >= 0 && host->focused_control < host->control_count) {
            SuiControl* c = &host->controls[host->focused_control];
            if (c->type == SUI_CTRL_SHORTCUT_RECORDER && c->d.shortcut.recording) {
                *result = 0; return 1;
            }
        }
        pt.x = GET_X_LPARAM(lp); pt.y = GET_Y_LPARAM(lp);
        ScreenToClient(hwnd, &pt);
        sui_ctrl_on_rbutton_up(host, pt.x, pt.y);
        *result = 0; return 1;
    case WM_MOUSEWHEEL:
        pt.x = GET_X_LPARAM(lp); pt.y = GET_Y_LPARAM(lp);
        ScreenToClient(hwnd, &pt);
        sui_ctrl_on_mouse_wheel(host, pt.x, pt.y, GET_WHEEL_DELTA_WPARAM(wp));
        *result = 0; return 1;
    case WM_SETFOCUS:
        host->window_has_focus = 1;
        host->caret_visible    = 1;
        SetTimer(hwnd, SUI_CARET_TIMER_ID, 500, NULL);
        *result = 0; return 1;
    case WM_KILLFOCUS:
        sui_ctrl_spinner_stop_repeat(host);
        host->window_has_focus = 0;
        KillTimer(hwnd, SUI_CARET_TIMER_ID);
        InvalidateRect(hwnd, NULL, FALSE);
        *result = 0; return 1;
    case WM_CAPTURECHANGED:
    case WM_CANCELMODE:
        sui_ctrl_spinner_stop_repeat(host);
        break;
    case WM_TIMER:
        sui_ctrl_on_timer(host, wp);
        *result = 0; return 1;
    case WM_INPUT:
        sui_ctrl_shortcut_on_raw_input(host, (HRAWINPUT)lp);
        *result = DefWindowProcW(hwnd, msg, wp, lp);
        return 1;
    case WM_INPUT_DEVICE_CHANGE:
    case WM_DEVICECHANGE:
        sui_ctrl_shortcut_on_gamepad_device_change();
        *result = DefWindowProcW(hwnd, msg, wp, lp);
        return 1;
    case WM_KEYDOWN:
        sui_ctrl_on_keydown_ex(host, wp, lp);
        *result = 0; return 1;
    case WM_SYSKEYDOWN:
        sui_ctrl_on_keydown_ex(host, wp, lp);
        *result = 0; return 1;
    case WM_KEYUP:
        sui_ctrl_on_keyup(host, wp);
        *result = 0; return 1;
    case WM_SYSKEYUP:
        sui_ctrl_on_keyup(host, wp);
        *result = 0; return 1;
    case WM_CHAR:
        sui_ctrl_on_char(host, (wchar_t)wp);
        *result = 0; return 1;
    case WM_GETOBJECT:
        *result = sui_uia_handle_getobject(host, wp, lp);
        return 1;
    }
    return 0;
}

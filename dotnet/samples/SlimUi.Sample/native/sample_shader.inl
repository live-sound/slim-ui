/* Included by the sample's bridge build, after SuiNetWindow is defined. */
static void SampleShaderFrame(void* user, float dt_ms) {
    SuiNetWindow* window = (SuiNetWindow*)user;
    window->shader.seconds += dt_ms * 0.001f * window->shader.speed;
    InvalidateRect(window->hwnd, NULL, FALSE);
}

static void SampleShaderUpdateClock(SuiNetWindow* window) {
    SampleShaderState& shader = window->shader;
    bool wanted = shader.active && !shader.paused && !IsIconic(window->hwnd);
    if (wanted == shader.running) return;
    if (wanted) {
        shader.running = sui_vsync_attach(&window->host.vsync, SampleShaderFrame, window) != 0;
    } else {
        sui_vsync_detach(&window->host.vsync, SampleShaderFrame, window);
        shader.running = false;
    }
}

static void SampleShaderRelease(SuiNetWindow* window) {
    window->shader.active = false;
    SampleShaderUpdateClock(window);
    demo_shader_release(window->shader.effect);
    window->shader.effect = nullptr;
}

static void SampleShaderPaint(SuiControlHost* host, SuiControl* control, void* user) {
    SuiNetWindow* window = (SuiNetWindow*)user;
    SampleShaderState& shader = window->shader;
    const int kind = control->type == SUI_CTRL_IMAGE ? DEMO_SHADER_DUSK : DEMO_SHADER_GLOW;
    const float margin = kind == DEMO_SHADER_GLOW ? (float)(int)(14.0f * window->dpi / 96.0f) : 0.0f;
    void* screen = window->renderer.render_target;
    if (!shader.effect) shader.effect = demo_shader_create();
    void* bitmap = demo_shader_begin(shader.effect, screen, kind,
        control->x, control->y, control->w, control->h, margin);
    if (!bitmap) {
        sui_ctrl_draw_self(host, control);
        return;
    }
    window->renderer.render_target = bitmap;
    sui__apply_text_style(&window->renderer, (ID2D1RenderTarget*)bitmap);
    sui_ctrl_draw_self(host, control);
    window->renderer.render_target = screen;
    demo_shader_end(shader.effect, kind, shader.seconds);
}

SUI_NET_API void SUI_NET_CALL sui_sample_shader_configure(SuiNetWindow* window,
    int image_id, int edit_id, int active, int paused, float speed) {
    if (!window || !window->host_ready) return;
    window->shader.active = active != 0;
    window->shader.paused = paused != 0;
    window->shader.speed = speed;
    if (active) {
        sui_ctrl_set_paint(&window->host, image_id, SampleShaderPaint, window);
        sui_ctrl_set_paint(&window->host, edit_id, SampleShaderPaint, window);
    }
    SampleShaderUpdateClock(window);
    InvalidateRect(window->hwnd, NULL, FALSE);
}

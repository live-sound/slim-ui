#include <windows.h>
#include <windowsx.h>
#include <stdint.h>

#include <algorithm>
#include <new>
#include <string>
#include <unordered_map>
#include <vector>

#define SUI_FEATURE_CUSTOM_CONTROLS
#define SUI_FEATURE_DECL
#define SUI_FEATURE_PNG
#define SUI_FEATURE_VSYNC
#define SUI_IMPLEMENTATION
#include <slim_ui/slim_ui.h>

#define SUI_NET_API extern "C" __declspec(dllexport)
#define SUI_NET_CALL __stdcall

struct SuiNetWindow;

#ifdef SUI_NET_SAMPLE_SHADER
#include "../../samples/SlimUi.Sample/native/sample_shader.h"
#endif

typedef void (SUI_NET_CALL *SuiNetBuildCallback)(SuiNetWindow* window, void* user_data);
typedef void (SUI_NET_CALL *SuiNetEventCallback)(SuiNetWindow* window, int control_id, int event_kind, void* user_data);

enum SuiNetEventKind {
    SUI_NET_EVENT_CLICK = 1,
    SUI_NET_EVENT_CHANGE = 2,
    SUI_NET_EVENT_CLOSE = 3
};

struct SuiNetControlState {
    int type = -1;
    std::wstring text;
    int cursor = -1;
    int checked = -1;
    int selected = -1;
    float value = 0.0f;
    bool has_value = false;
    UINT vk = 0;
    UINT modifiers = 0;
    UINT scan_code = 0;
    int extended = 0;
    UINT mouse_button = 0;
    int shortcut_kind = 0;
    uint32_t gamepad_buttons = 0;
    int recording = 0;
    bool has_shortcut = false;
    uint32_t swatch_color = 0;
    bool has_swatch_color = false;
};

struct SuiNetWindow {
    HWND hwnd = NULL;
    SuiRenderer renderer = {};
    SuiControlHost host = {};
    const SuiColorScheme* scheme = NULL;
    UINT dpi = 96;
    bool renderer_ready = false;
    bool host_ready = false;
    bool released = false;
    bool counted = false;
    bool com_initialized = false;
    LONG callback_depth = 0;
    LONG deferred_destroy = 0;
    LONG in_wm_destroy = 0;
    DWORD owner_thread_id = 0;
    SuiNetBuildCallback build_callback = NULL;
    SuiNetEventCallback event_callback = NULL;
    void* user_data = NULL;
    std::unordered_map<int, SuiNetControlState> states;
    std::unordered_map<int, std::wstring> tooltips;
    std::vector<SuiImageHandle> images;
#ifdef SUI_NET_SAMPLE_SHADER
    SampleShaderState shader;
#endif
};

#ifdef SUI_NET_SAMPLE_SHADER
#include "../../samples/SlimUi.Sample/native/sample_shader.inl"
#endif

static const wchar_t* kSuiNetClassName = L"SlimUiNetWindow";
static const UINT kSuiNetDeferredDestroyMessage = WM_APP + 0x5A0;
static const UINT_PTR kSuiNetDeferredDestroyTimer = 0x5A10;
static int g_window_count = 0;
static bool g_dpi_awareness_set = false;

static LRESULT CALLBACK SuiNetWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

static const SuiTypography g_demo_serif_typography = {
    { L"Cambria",            13.0f, 400, 0 },
    { L"Cambria",            13.0f, 600, 0 },
    { L"Cambria",            26.0f, 700, 0 },
    { L"Segoe Fluent Icons", 13.0f, 400, 0 },
};

static const SuiColorScheme g_demo_blue_dark_scheme = {
    0x1E1E1E, 0xE0E0E0, 0x999999,
    0x3A3A3A, 0x555555, 0xE0E0E0, 0x444444, 0x333333, 0x4CC2FF,
    0x999999, 0x4CC2FF, 0xFFFFFF,
    0x2D2D2D, 0x555555, 0xE0E0E0, 0x4CC2FF, 0x264F78, 0x666666,
    0x555555, 0x4CC2FF, 0x4CC2FF,
    0x2A2A2A, 0x262626, 0x3A3A3A, 0x3D3D3D,
    0x404040, 0x5CBF60,
    0x2D2D2D, 0x3D3D3D,
};

static void SuiNetRegisterDemoThemeParts() {
    static bool registered = false;
    if (registered) return;
    registered = true;
    sui_scheme_register("blue-dark", &g_demo_blue_dark_scheme);
    sui_typography_register("serif", &g_demo_serif_typography);
}

static float SuiNetScale(SuiNetWindow* window) {
    return window && window->dpi ? (float)window->dpi / 96.0f : 1.0f;
}

static float SuiNetDip(SuiNetWindow* window, float value) {
    return value * SuiNetScale(window);
}

static SuiRect SuiNetDipRect(SuiNetWindow* window, float x, float y, float w, float h) {
    float s = SuiNetScale(window);
    SuiRect r = { x * s, y * s, (x + w) * s, (y + h) * s };
    return r;
}

static int SuiNetControlIndex(SuiNetWindow* window, int id) {
    if (!window || !window->host_ready) return -1;
    SuiControl* c = sui_ctrl_find(&window->host, id);
    return c ? (int)(c - window->host.controls) : -1;
}

static SuiImageHandle SuiNetImage(SuiNetWindow* window, int image_id) {
    if (!window || image_id <= 0 || image_id > (int)window->images.size()) {
        SuiImageHandle empty = {};
        return empty;
    }
    return window->images[(size_t)image_id - 1];
}

static const wchar_t* SuiNetStoreTooltip(SuiNetWindow* window, int id, const wchar_t* text) {
    if (!window || id < 0) return NULL;
    if (!text || !*text) {
        window->tooltips.erase(id);
        return NULL;
    }

    std::wstring& value = window->tooltips[id];
    value = text;
    return value.c_str();
}

static void SuiNetSetControlTooltip(SuiNetWindow* window, int id, const wchar_t* text) {
    const wchar_t* stored = SuiNetStoreTooltip(window, id, text);
    if (!window || !window->host_ready || id < 0) return;

    SuiControl* c = sui_ctrl_find(&window->host, id);
    if (!c) return;

    int idx = (int)(c - window->host.controls);
    int was_active = window->host.tooltip_active_ctrl == idx;
    sui_ctrl_set_tooltip(&window->host, id, stored);

    if (was_active) {
        sui_ctrl_tooltip_update(&window->host, -1);
        if (stored) sui_ctrl_tooltip_update(&window->host, idx);
    }
}

static bool SuiNetRegisterClass() {
    static bool registered = false;
    if (registered) return true;

    WNDCLASSW wc = {};
    wc.lpfnWndProc = SuiNetWndProc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.lpszClassName = kSuiNetClassName;
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;

    ATOM atom = RegisterClassW(&wc);
    if (!atom && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
    registered = true;
    return true;
}

static void SuiNetCaptureControl(SuiNetWindow* window, const SuiControl* c) {
    if (!window || !c || c->id < 0) return;

    SuiNetControlState& state = window->states[c->id];
    state.type = (int)c->type;

    switch (c->type) {
    case SUI_CTRL_EDIT:
        state.text = c->text;
        state.cursor = c->d.edit.cursor;
        break;
    case SUI_CTRL_TEXTVIEW:
        if (c->d.textview.buf) {
            state.text.assign(c->d.textview.buf, c->d.textview.len);
            state.cursor = c->d.textview.cursor;
        }
        break;
    case SUI_CTRL_CHECKBOX:
    case SUI_CTRL_RADIO:
        state.checked = (c->state & SUI_CTRL_CHECKED) ? 1 : 0;
        break;
    case SUI_CTRL_COMBOBOX:
        state.selected = c->d.combo.selected;
        break;
    case SUI_CTRL_NAV_LIST:
        state.selected = c->d.nav.selected;
        break;
    case SUI_CTRL_TABS:
        state.selected = c->d.tabs.selected;
        break;
    case SUI_CTRL_SLIDER:
        state.value = c->d.slider.value;
        state.has_value = true;
        break;
    case SUI_CTRL_SPINNER:
        state.value = c->d.spinner.value;
        state.has_value = true;
        break;
    case SUI_CTRL_PROGRESS:
        state.value = c->d.progress.value;
        state.has_value = true;
        break;
    case SUI_CTRL_SHORTCUT_RECORDER:
        state.shortcut_kind = c->d.shortcut.kind;
        state.vk = c->d.shortcut.vk;
        state.modifiers = c->d.shortcut.modifiers;
        state.scan_code = c->d.shortcut.scan_code;
        state.extended = c->d.shortcut.extended;
        state.mouse_button = c->d.shortcut.mouse_button;
        state.gamepad_buttons = c->d.shortcut.gamepad_buttons;
        state.recording = c->d.shortcut.recording;
        state.has_shortcut = true;
        break;
    case SUI_CTRL_COLOR_SWATCH:
        state.swatch_color = c->d.swatch.color;
        state.has_swatch_color = true;
        break;
    default:
        break;
    }
}

static void SuiNetCaptureState(SuiNetWindow* window) {
    if (!window || !window->host_ready) return;
    for (int i = 0; i < window->host.control_count; ++i) {
        SuiNetCaptureControl(window, &window->host.controls[i]);
    }
}

static void SuiNetApplyState(SuiNetWindow* window, int control_index) {
    if (!window || control_index < 0 || control_index >= window->host.control_count) return;

    SuiControl* c = &window->host.controls[control_index];
    auto it = window->states.find(c->id);
    if (it == window->states.end()) return;

    SuiNetControlState& state = it->second;
    if (state.type != (int)c->type) return;

    switch (c->type) {
    case SUI_CTRL_EDIT: {
        sui__wstrncpy_z(c->text, state.text.c_str(), SUI_CTRL_TEXT_MAX);
        int len = (int)wcslen(c->text);
        c->d.edit.cursor = state.cursor >= 0 ? std::min(state.cursor, len) : len;
        c->d.edit.sel_start = -1;
        c->d.edit.sel_end = -1;
        c->d.edit.drag_sel_anchor = c->d.edit.cursor;
        sui_ctrl_edit_scroll_to_caret(&window->host, c);
        break;
    }
    case SUI_CTRL_TEXTVIEW: {
        if (!c->d.textview.buf || c->d.textview.capacity <= 0) break;
        int len = (int)std::min<size_t>(state.text.size(), (size_t)c->d.textview.capacity - 1);
        sui__wmove(c->d.textview.buf, state.text.c_str(), len);
        c->d.textview.buf[len] = L'\0';
        c->d.textview.len = len;
        c->d.textview.cursor = state.cursor >= 0 ? std::min(state.cursor, len) : len;
        c->d.textview.sel_start = -1;
        c->d.textview.sel_end = -1;
        c->d.textview.drag_sel_anchor = c->d.textview.cursor;
        sui__textview_update_content_height(&window->host, c);
        break;
    }
    case SUI_CTRL_CHECKBOX:
    case SUI_CTRL_RADIO:
        if (state.checked >= 0) {
            if (state.checked) c->state |= SUI_CTRL_CHECKED;
            else c->state &= ~SUI_CTRL_CHECKED;
        }
        break;
    case SUI_CTRL_COMBOBOX:
        if (state.selected >= 0 && state.selected < c->d.combo.item_count) {
            c->d.combo.selected = state.selected;
            sui__wstrncpy_z(c->text, c->d.combo.items[state.selected], SUI_CTRL_TEXT_MAX);
        }
        break;
    case SUI_CTRL_NAV_LIST:
        if (state.selected >= 0 && state.selected < c->d.nav.item_count) {
            c->d.nav.selected = state.selected;
        }
        break;
    case SUI_CTRL_TABS:
        if (state.selected >= 0 && state.selected < c->d.tabs.item_count) {
            c->d.tabs.selected = state.selected;
        }
        break;
    case SUI_CTRL_SLIDER:
        if (state.has_value) c->d.slider.value = state.value;
        break;
    case SUI_CTRL_SPINNER:
        if (state.has_value) {
            c->d.spinner.value = state.value;
            sui_ctrl_spinner_clamp(c);
        }
        break;
    case SUI_CTRL_PROGRESS:
        if (state.has_value) c->d.progress.value = state.value;
        break;
    case SUI_CTRL_SHORTCUT_RECORDER:
        if (state.has_shortcut && state.shortcut_kind == c->d.shortcut.kind) {
            c->d.shortcut.vk = state.vk;
            c->d.shortcut.modifiers = state.modifiers;
            c->d.shortcut.scan_code = state.scan_code;
            c->d.shortcut.extended = state.extended;
            c->d.shortcut.mouse_button = state.mouse_button;
            c->d.shortcut.gamepad_buttons = state.gamepad_buttons;
            c->d.shortcut.recording = state.recording;
            sui_ctrl_shortcut_sync_display(c);
            if (c->d.shortcut.recording) {
                sui__wstrncpy_z(c->text, c->d.shortcut.prompt, SUI_CTRL_TEXT_MAX);
                if (c->d.shortcut.kind == SUI_SHORTCUT_GAMEPAD)
                    SetTimer(window->host.hwnd, SUI_SHORTCUT_RECORD_TIMER_ID, 30, NULL);
            }
        }
        break;
    case SUI_CTRL_COLOR_SWATCH:
        if (state.has_swatch_color) {
            c->d.swatch.color = state.swatch_color;
        }
        break;
    default:
        break;
    }
}

static void SuiNetInvokeBuild(SuiNetWindow* window) {
    if (!window || !window->build_callback) return;
    SuiNetBuildCallback callback = window->build_callback;
    void* user_data = window->user_data;
    InterlockedIncrement(&window->callback_depth);
    callback(window, user_data);
    InterlockedDecrement(&window->callback_depth);
}

static void SuiNetInvokeEvent(SuiNetWindow* window, int control_id, int event_kind) {
    if (!window || !window->event_callback) return;
    SuiNetEventCallback callback = window->event_callback;
    void* user_data = window->user_data;
    InterlockedIncrement(&window->callback_depth);
    callback(window, control_id, event_kind, user_data);
    InterlockedDecrement(&window->callback_depth);
}

static void SuiNetRebuild(SuiNetWindow* window) {
    if (!window || !window->host_ready || !window->build_callback) return;
    SuiNetCaptureState(window);
    SuiNetInvokeBuild(window);
}

static void SuiNetRelease(SuiNetWindow* window) {
    if (!window || window->released) return;
#ifdef SUI_NET_SAMPLE_SHADER
    SampleShaderRelease(window);
#endif
    SuiNetCaptureState(window);
    if (window->host_ready) {
        sui_ctrl_host_release(&window->host);
        window->host_ready = false;
    }
    if (window->renderer_ready) {
        for (SuiImageHandle img : window->images) {
            sui_renderer_free_image(&window->renderer, img);
        }
        window->images.clear();
        sui_renderer_release(&window->renderer);
        window->renderer_ready = false;
    }
    if (window->com_initialized) {
        CoUninitialize();
        window->com_initialized = false;
    }
    window->released = true;
}

static void SuiNetRequestDeferredDestroy(SuiNetWindow* window) {
    if (!window) return;
    InterlockedExchange(&window->deferred_destroy, 1);
    /* Managed Dispose frees its delegate/GCHandle immediately after the native
     * call returns. Stop all later callbacks before handing teardown to the
     * HWND's owning UI thread. */
    window->build_callback = NULL;
    window->event_callback = NULL;
    window->user_data = NULL;
    if (window->hwnd &&
        InterlockedCompareExchange(&window->in_wm_destroy, 0, 0) == 0) {
        PostMessageW(window->hwnd, kSuiNetDeferredDestroyMessage, 0, 0);
    }
}

static void SuiNetOnClick(int control_id, void* user_data) {
    SuiNetWindow* window = (SuiNetWindow*)user_data;
    if (!window) return;

    SuiControl* c = sui_ctrl_find(&window->host, control_id);
    int event_kind = SUI_NET_EVENT_CHANGE;
    if (c) {
        SuiNetCaptureControl(window, c);
        if (c->type == SUI_CTRL_BUTTON) event_kind = SUI_NET_EVENT_CLICK;
    }

    SuiNetInvokeEvent(window, control_id, event_kind);
}

static bool SuiNetInitializeWindow(SuiNetWindow* window, HWND hwnd) {
    if (!window) return false;

    HRESULT co_hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    window->com_initialized = SUCCEEDED(co_hr);
    window->hwnd = hwnd;
    window->owner_thread_id = GetCurrentThreadId();
    window->dpi = sui_get_window_dpi(hwnd);
    SuiNetRegisterDemoThemeParts();
    window->scheme = sui_scheme_find("light");
    if (!window->scheme) window->scheme = &sui_scheme_light;

    sui_renderer_init_fields(&window->renderer);
    if (!sui_renderer_init(&window->renderer, hwnd)) return false;
    window->renderer_ready = true;

    sui_ctrl_host_init(&window->host, hwnd, &window->renderer, window->scheme, window->dpi);
    window->host.on_click = SuiNetOnClick;
    window->host.on_click_data = window;
    window->host.min_size_strategy = SUI_MIN_SIZE_FLEX_ROWS;
    window->host_ready = true;

    window->counted = true;
    ++g_window_count;
    SuiNetRebuild(window);
    return true;
}

static LRESULT CALLBACK SuiNetWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    SuiNetWindow* window = (SuiNetWindow*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    if (msg == WM_NCCREATE) {
        CREATESTRUCTW* cs = (CREATESTRUCTW*)lp;
        window = (SuiNetWindow*)cs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)window);
        if (window) window->hwnd = hwnd;
    }

    switch (msg) {
    case WM_CREATE:
        if (!SuiNetInitializeWindow(window, hwnd)) return -1;
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        if (window && window->renderer_ready) {
            sui_renderer_begin(&window->renderer);
            sui_renderer_clear(&window->renderer, window->scheme->background);
            sui_ctrl_draw_all(&window->host);
            sui_renderer_end(&window->renderer);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_GETMINMAXINFO:
        if (window && window->host_ready) sui_handle_wm_getminmaxinfo(&window->host, hwnd, lp);
        return 0;

    case WM_SIZE:
#ifdef SUI_NET_SAMPLE_SHADER
        if (window && window->host_ready) SampleShaderUpdateClock(window);
#endif
        if (window && window->renderer_ready && wp != SIZE_MINIMIZED) {
            RECT cr;
            GetClientRect(hwnd, &cr);
            sui_renderer_resize(&window->renderer, cr.right, cr.bottom);
            SuiNetRebuild(window);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;

    case WM_DPICHANGED:
        if (window && window->host_ready) {
            window->dpi = HIWORD(wp);
            const RECT* nr = (const RECT*)lp;
            sui_handle_wm_dpichanged(&window->host, &window->renderer, wp);
            SetWindowPos(hwnd, NULL, nr->left, nr->top, nr->right - nr->left, nr->bottom - nr->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            RECT cr;
            GetClientRect(hwnd, &cr);
            sui_renderer_resize(&window->renderer, cr.right, cr.bottom);
            SuiNetRebuild(window);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;

    case WM_SUI_VSYNC:
        if (window && window->host_ready) {
            sui_ctrl_on_vsync(&window->host);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;

    case kSuiNetDeferredDestroyMessage:
        if (window &&
            InterlockedCompareExchange(&window->deferred_destroy, 0, 0) != 0) {
            if (InterlockedCompareExchange(&window->callback_depth, 0, 0) > 0) {
                /* A nested message pump can dispatch the posted request before
                 * the managed callback unwinds. A timer retries without
                 * deleting storage still used by the callback/core dispatch. */
                SetTimer(hwnd, kSuiNetDeferredDestroyTimer, 1, NULL);
                return 0;
            }
            /* WM_NCDESTROY owns the final delete for this deferred path. Do
             * not access window after DestroyWindow returns. */
            DestroyWindow(hwnd);
        }
        return 0;

    case WM_TIMER:
        if (wp == kSuiNetDeferredDestroyTimer && window &&
            InterlockedCompareExchange(&window->deferred_destroy, 0, 0) != 0) {
            if (InterlockedCompareExchange(&window->callback_depth, 0, 0) > 0)
                return 0;
            KillTimer(hwnd, kSuiNetDeferredDestroyTimer);
            DestroyWindow(hwnd);
            return 0;
        }
        break;

    case WM_DESTROY:
        if (window) {
            InterlockedExchange(&window->in_wm_destroy, 1);
            SuiNetInvokeEvent(window, -1, SUI_NET_EVENT_CLOSE);
            InterlockedExchange(&window->in_wm_destroy, 0);
            SuiNetRelease(window);
            if (window->counted) {
                window->counted = false;
                --g_window_count;
            }
        }
        if (g_window_count <= 0) PostQuitMessage(0);
        return 0;

    case WM_NCDESTROY:
        if (window) window->hwnd = NULL;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        {
            bool delete_window = window &&
                InterlockedCompareExchange(&window->deferred_destroy, 0, 0) != 0;
            LRESULT result = DefWindowProcW(hwnd, msg, wp, lp);
            if (delete_window) delete window;
            return result;
        }
    }

    if (window && window->host_ready) {
        LRESULT result = 0;
        if (sui_default_msg(&window->host, hwnd, msg, wp, lp, &result)) {
            return result;
        }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

SUI_NET_API SuiNetWindow* SUI_NET_CALL sui_net_window_create(
    const wchar_t* title,
    int width,
    int height,
    SuiNetBuildCallback build_callback,
    SuiNetEventCallback event_callback,
    void* user_data) {

    if (!g_dpi_awareness_set) {
        sui_enable_dpi_awareness();
        g_dpi_awareness_set = true;
    }
    if (!SuiNetRegisterClass()) return NULL;

    SuiNetWindow* window = new (std::nothrow) SuiNetWindow();
    if (!window) return NULL;
    window->build_callback = build_callback;
    window->event_callback = event_callback;
    window->user_data = user_data;

    const int dpi = (int)sui_get_window_dpi(NULL);

    HWND hwnd = CreateWindowExW(
        0,
        kSuiNetClassName,
        title ? title : L"Slim UI",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        MulDiv(width > 0 ? width : 900, dpi, 96),
        MulDiv(height > 0 ? height : 640, dpi, 96),
        NULL,
        NULL,
        GetModuleHandleW(NULL),
        window);

    if (!hwnd) {
        SuiNetRelease(window);
        delete window;
        return NULL;
    }
    return window;
}

SUI_NET_API void SUI_NET_CALL sui_net_window_destroy(SuiNetWindow* window) {
    if (!window) return;
    if (InterlockedCompareExchange(&window->callback_depth, 0, 0) > 0 ||
        (window->hwnd && window->owner_thread_id != GetCurrentThreadId())) {
        SuiNetRequestDeferredDestroy(window);
        return;
    }
    window->build_callback = NULL;
    window->event_callback = NULL;
    window->user_data = NULL;
    if (window->hwnd) {
        if (!DestroyWindow(window->hwnd)) {
            /* Never delete storage while a live HWND still points at it. */
            SuiNetRequestDeferredDestroy(window);
            return;
        }
    } else {
        SuiNetRelease(window);
    }
    delete window;
}

SUI_NET_API void SUI_NET_CALL sui_net_window_show(SuiNetWindow* window) {
    if (!window || !window->hwnd) return;
    ShowWindow(window->hwnd, SW_SHOW);
    UpdateWindow(window->hwnd);
}

SUI_NET_API void SUI_NET_CALL sui_net_window_close(SuiNetWindow* window) {
    if (!window || !window->hwnd) return;
    PostMessageW(window->hwnd, WM_CLOSE, 0, 0);
}

SUI_NET_API void SUI_NET_CALL sui_net_window_rebuild(SuiNetWindow* window) {
    if (!window || !window->hwnd) return;
    SuiNetRebuild(window);
    InvalidateRect(window->hwnd, NULL, FALSE);
}

SUI_NET_API int SUI_NET_CALL sui_net_app_run(void) {
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}

SUI_NET_API float SUI_NET_CALL sui_net_client_width(SuiNetWindow* window) {
    if (!window || !window->hwnd) return 0.0f;
    RECT cr;
    GetClientRect(window->hwnd, &cr);
    return (float)cr.right / SuiNetScale(window);
}

SUI_NET_API float SUI_NET_CALL sui_net_client_height(SuiNetWindow* window) {
    if (!window || !window->hwnd) return 0.0f;
    RECT cr;
    GetClientRect(window->hwnd, &cr);
    return (float)cr.bottom / SuiNetScale(window);
}

SUI_NET_API float SUI_NET_CALL sui_net_dpi_scale(SuiNetWindow* window) {
    return SuiNetScale(window);
}

SUI_NET_API void SUI_NET_CALL sui_net_clear_page(SuiNetWindow* window) {
    if (!window || !window->host_ready) return;
    sui_ctrl_clear_page(&window->host);
    window->host.container_depth = 0;
    window->host.intrinsic_min_w = 0.0f;
    window->host.intrinsic_min_h = 0.0f;
    window->host.min_client_w = 0.0f;
    window->host.min_client_h = 0.0f;
    window->host.min_size_strategy = SUI_MIN_SIZE_FLEX_ROWS;
}

SUI_NET_API int SUI_NET_CALL sui_net_set_scheme(SuiNetWindow* window, const char* name) {
    if (!window || !window->host_ready) return 0;
    SuiNetRegisterDemoThemeParts();
    const SuiColorScheme* scheme = sui_scheme_find(name ? name : "");
    if (!scheme) return 0;
    window->scheme = scheme;
    window->host.scheme = scheme;
    if (window->hwnd) InvalidateRect(window->hwnd, NULL, FALSE);
    return 1;
}

SUI_NET_API int SUI_NET_CALL sui_net_set_typography(SuiNetWindow* window, const char* name) {
    if (!window || !window->renderer_ready) return 0;
    SuiNetRegisterDemoThemeParts();
    const SuiTypography* typography = sui_typography_find(name ? name : "");
    if (!typography) return 0;
    sui_renderer_apply_typography(&window->renderer, typography);
    if (window->hwnd) InvalidateRect(window->hwnd, NULL, FALSE);
    return 1;
}

SUI_NET_API int SUI_NET_CALL sui_net_load_image(SuiNetWindow* window, const wchar_t* path) {
    if (!window || !window->renderer_ready || !path || !*path) return 0;

    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return 0;

    LARGE_INTEGER size = {};
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 || size.QuadPart > 64LL * 1024LL * 1024LL) {
        CloseHandle(file);
        return 0;
    }

    std::vector<uint8_t> bytes((size_t)size.QuadPart);
    DWORD read = 0;
    BOOL ok = ReadFile(file, bytes.data(), (DWORD)bytes.size(), &read, NULL);
    CloseHandle(file);
    if (!ok || read != bytes.size()) return 0;

    SuiImageHandle img = sui_renderer_load_image(&window->renderer, bytes.data(), bytes.size());
    if (!img.data) return 0;
    window->images.push_back(img);
    return (int)window->images.size();
}

SUI_NET_API int SUI_NET_CALL sui_net_image_width(SuiNetWindow* window, int image_id) {
    return SuiNetImage(window, image_id).width;
}

SUI_NET_API int SUI_NET_CALL sui_net_image_height(SuiNetWindow* window, int image_id) {
    return SuiNetImage(window, image_id).height;
}

SUI_NET_API int SUI_NET_CALL sui_net_push_root_rect(SuiNetWindow* window, float x, float y, float w, float h, float gap) {
    if (!window || !window->host_ready) return 0;
    SuiRect r = SuiNetDipRect(window, x, y, w, h);
    return sui_begin_px(&window->host, r, SuiNetDip(window, gap), SUI_CLEAR_NONE) ? 1 : 0;
}

SUI_NET_API float SUI_NET_CALL sui_net_current_container_consumed_height(SuiNetWindow* window) {
    if (!window || !window->host_ready || window->host.container_depth <= 0) return 0.0f;
    SuiContainer* c = &window->host.container_stack[window->host.container_depth - 1];
    float consumed = c->cursor_y - c->y;
    if (consumed > 0.0f) consumed -= c->gap;
    if (consumed < 0.0f) consumed = 0.0f;
    return consumed / SuiNetScale(window);
}

SUI_NET_API void SUI_NET_CALL sui_net_pop_container(SuiNetWindow* window) {
    if (!window || !window->host_ready || window->host.container_depth <= 0) return;
    window->host.container_depth--;
}

SUI_NET_API void SUI_NET_CALL sui_net_set_min_client_size(SuiNetWindow* window, float width, float height) {
    if (!window || !window->host_ready) return;
    if (width >= 0.0f) window->host.min_client_w = SuiNetDip(window, width);
    if (height >= 0.0f) window->host.min_client_h = SuiNetDip(window, height);
}

SUI_NET_API void SUI_NET_CALL sui_net_set_control_min_size(SuiNetWindow* window, int id, float width, float height) {
    int idx = SuiNetControlIndex(window, id);
    if (idx < 0) return;
    if (width >= 0.0f) window->host.controls[idx].min_w = SuiNetDip(window, width);
    if (height >= 0.0f) window->host.controls[idx].min_h = SuiNetDip(window, height);
}

SUI_NET_API void SUI_NET_CALL sui_net_last_min_size(SuiNetWindow* window, float width, float height) {
    if (!window || !window->host_ready || window->host.control_count <= 0) return;
    SuiControl* c = &window->host.controls[window->host.control_count - 1];
    if (width >= 0.0f) c->min_w = SuiNetDip(window, width);
    if (height >= 0.0f) c->min_h = SuiNetDip(window, height);
}

SUI_NET_API void SUI_NET_CALL sui_net_set_control_disabled(SuiNetWindow* window, int id, int disabled) {
    int idx = SuiNetControlIndex(window, id);
    if (idx < 0) return;
    if (disabled) window->host.controls[idx].state |= SUI_CTRL_DISABLED;
    else window->host.controls[idx].state &= ~SUI_CTRL_DISABLED;
}

SUI_NET_API void SUI_NET_CALL sui_net_set_parent(SuiNetWindow* window, int id, int parent_id) {
    int idx = SuiNetControlIndex(window, id);
    if (idx >= 0) window->host.controls[idx].parent_id = parent_id;
}

SUI_NET_API void SUI_NET_CALL sui_net_last_parent(SuiNetWindow* window, int parent_id) {
    if (!window || !window->host_ready || window->host.control_count <= 0) return;
    window->host.controls[window->host.control_count - 1].parent_id = parent_id;
}

SUI_NET_API void SUI_NET_CALL sui_net_last_text_style(SuiNetWindow* window, float font_size, int bold) {
    if (!window || !window->host_ready || window->host.control_count <= 0) return;
    SuiControl* c = &window->host.controls[window->host.control_count - 1];
    if (font_size > 0.0f) c->font_size = font_size;
    if (bold) c->state |= SUI_CTRL_BOLD;
    else c->state &= ~SUI_CTRL_BOLD;
}

SUI_NET_API void SUI_NET_CALL sui_net_set_text_style(SuiNetWindow* window, int id, float font_size, int bold) {
    int idx = SuiNetControlIndex(window, id);
    if (idx < 0) return;
    SuiControl* c = &window->host.controls[idx];
    if (font_size > 0.0f) c->font_size = font_size;
    if (bold >= 0) {
        if (bold) c->state |= SUI_CTRL_BOLD;
        else c->state &= ~SUI_CTRL_BOLD;
    }
    InvalidateRect(window->hwnd, NULL, FALSE);
}

SUI_NET_API void SUI_NET_CALL sui_net_set_style_color(SuiNetWindow* window, int id, uint32_t prop, uint32_t color) {
    if (!window || !window->host_ready) return;
    sui_ctrl_set_style_color(&window->host, id, prop, color);
    InvalidateRect(window->hwnd, NULL, FALSE);
}

SUI_NET_API void SUI_NET_CALL sui_net_last_style_color(SuiNetWindow* window, uint32_t prop, uint32_t color) {
    if (!window || !window->host_ready || window->host.control_count <= 0) return;
    sui_last_style_color(&window->host, prop, color);
    InvalidateRect(window->hwnd, NULL, FALSE);
}

SUI_NET_API void SUI_NET_CALL sui_net_set_font_family(SuiNetWindow* window, int id, const wchar_t* family) {
    if (!window || !window->host_ready) return;
    sui_ctrl_set_font_family(&window->host, id, family);
    InvalidateRect(window->hwnd, NULL, FALSE);
}

SUI_NET_API void SUI_NET_CALL sui_net_last_font_family(SuiNetWindow* window, const wchar_t* family) {
    if (!window || !window->host_ready || window->host.control_count <= 0) return;
    sui_last_font_family(&window->host, family);
    InvalidateRect(window->hwnd, NULL, FALSE);
}

SUI_NET_API void SUI_NET_CALL sui_net_set_control_flow(SuiNetWindow* window, int id, int flow) {
    int idx = SuiNetControlIndex(window, id);
    if (idx < 0) return;
    if (flow) window->host.controls[idx].state |= SUI_CTRL_FLOW;
    else window->host.controls[idx].state &= ~SUI_CTRL_FLOW;
}

SUI_NET_API void SUI_NET_CALL sui_net_set_anchor(SuiNetWindow* window, int id, uint32_t flags, float right_margin, float bottom_margin) {
    if (!window || !window->host_ready) return;
    sui_ctrl_set_anchor(&window->host, id, flags, SuiNetDip(window, right_margin), SuiNetDip(window, bottom_margin));
}

SUI_NET_API void SUI_NET_CALL sui_net_set_scrollview_content_height(SuiNetWindow* window, int id, float height) {
    if (!window || !window->host_ready) return;
    sui_ctrl_set_scrollview_content_height(&window->host, id, SuiNetDip(window, height));
}

SUI_NET_API int SUI_NET_CALL sui_net_label_at(SuiNetWindow* window, int id, float x, float y, float w, float h, const wchar_t* text) {
    if (!window || !window->host_ready) return -1;
    SuiRect r = SuiNetDipRect(window, x, y, w, h);
    return sui_ctrl_add_label(&window->host, id, r.left, r.top, r.right - r.left, r.bottom - r.top, text ? text : L"");
}

SUI_NET_API int SUI_NET_CALL sui_net_title_at(SuiNetWindow* window, int id, float x, float y, float w, float h, const wchar_t* text) {
    int idx = sui_net_label_at(window, id, x, y, w, h, text);
    if (idx >= 0) {
        window->host.controls[idx].font_size = SUI_TITLE_PT;
        window->host.controls[idx].state |= SUI_CTRL_BOLD;
    }
    return idx;
}

SUI_NET_API int SUI_NET_CALL sui_net_tooltip_at(SuiNetWindow* window, int id, float x, float y, float w, float h, const wchar_t* text) {
    if (!window || !window->host_ready) return -1;
    SuiRect r = SuiNetDipRect(window, x, y, w, h);
    int idx = sui_ctrl_add_tooltip(&window->host, id, r.left, r.top, r.right - r.left, r.bottom - r.top, NULL);
    if (idx >= 0) SuiNetSetControlTooltip(window, id, text);
    return idx;
}

SUI_NET_API int SUI_NET_CALL sui_net_button_at(SuiNetWindow* window, int id, float x, float y, float w, float h, const wchar_t* text, int is_default, int enabled) {
    if (!window || !window->host_ready) return -1;
    SuiRect r = SuiNetDipRect(window, x, y, w, h);
    int idx = sui_ctrl_add_button(&window->host, id, r.left, r.top, r.right - r.left, r.bottom - r.top, text ? text : L"", is_default);
    if (idx >= 0 && !enabled) window->host.controls[idx].state |= SUI_CTRL_DISABLED;
    return idx;
}

SUI_NET_API int SUI_NET_CALL sui_net_checkbox_at(SuiNetWindow* window, int id, float x, float y, float w, float h, const wchar_t* text, int checked) {
    if (!window || !window->host_ready) return -1;
    SuiRect r = SuiNetDipRect(window, x, y, w, h);
    int idx = sui_ctrl_add_checkbox(&window->host, id, r.left, r.top, r.right - r.left, r.bottom - r.top, text ? text : L"", checked);
    SuiNetApplyState(window, idx);
    return idx;
}

SUI_NET_API int SUI_NET_CALL sui_net_radio_at(SuiNetWindow* window, int id, float x, float y, float w, float h, const wchar_t* text, int group_id, int checked) {
    if (!window || !window->host_ready) return -1;
    SuiRect r = SuiNetDipRect(window, x, y, w, h);
    int idx = sui_ctrl_add_radio(&window->host, id, r.left, r.top, r.right - r.left, r.bottom - r.top, text ? text : L"", group_id, checked);
    SuiNetApplyState(window, idx);
    return idx;
}

SUI_NET_API int SUI_NET_CALL sui_net_edit_at(SuiNetWindow* window, int id, float x, float y, float w, float h, const wchar_t* text, const wchar_t* placeholder, int numeric_only) {
    if (!window || !window->host_ready) return -1;
    SuiRect r = SuiNetDipRect(window, x, y, w, h);
    int idx = sui_ctrl_add_edit(&window->host, id, r.left, r.top, r.right - r.left, r.bottom - r.top, text ? text : L"", placeholder ? placeholder : L"", numeric_only);
    SuiNetApplyState(window, idx);
    return idx;
}

SUI_NET_API int SUI_NET_CALL sui_net_shortcut_recorder_at(SuiNetWindow* window, int id, float x, float y, float w, float h,
                                                          int kind, uint32_t virtual_key, uint32_t modifiers,
                                                          uint32_t scan_code, int extended,
                                                          uint32_t mouse_button,
                                                          uint32_t gamepad_buttons,
                                                          const wchar_t* prompt) {
    if (!window || !window->host_ready) return -1;
    SuiRect r = SuiNetDipRect(window, x, y, w, h);
    int idx = -1;
    if (kind == SUI_SHORTCUT_GAMEPAD) {
        idx = sui_ctrl_add_shortcut_recorder_gamepad(&window->host, id,
            r.left, r.top, r.right - r.left, r.bottom - r.top,
            gamepad_buttons, prompt);
    } else {
        idx = sui_ctrl_add_shortcut_recorder_keyboard_physical(&window->host, id,
            r.left, r.top, r.right - r.left, r.bottom - r.top,
            virtual_key, modifiers, scan_code, extended, mouse_button, prompt);
    }
    SuiNetApplyState(window, idx);
    return idx;
}

SUI_NET_API int SUI_NET_CALL sui_net_combobox_at(SuiNetWindow* window, int id, float x, float y, float w, float h, const wchar_t** items, int item_count, int selected) {
    if (!window || !window->host_ready) return -1;
    SuiRect r = SuiNetDipRect(window, x, y, w, h);
    int idx = sui_ctrl_add_combobox(&window->host, id, r.left, r.top, r.right - r.left, r.bottom - r.top, items, item_count, selected);
    SuiNetApplyState(window, idx);
    return idx;
}

SUI_NET_API int SUI_NET_CALL sui_net_slider_at(SuiNetWindow* window, int id, float x, float y, float w, float h, float minv, float maxv, float value, float step) {
    if (!window || !window->host_ready) return -1;
    SuiRect r = SuiNetDipRect(window, x, y, w, h);
    SuiSliderParams p = {};
    p.min = minv; p.max = maxv; p.value = value; p.step = step;
    int idx = sui_ctrl_add_slider_p(&window->host, id, r.left, r.top, r.right - r.left, r.bottom - r.top, p);
    SuiNetApplyState(window, idx);
    return idx;
}

SUI_NET_API int SUI_NET_CALL sui_net_progress_at(SuiNetWindow* window, int id, float x, float y, float w, float h, float minv, float maxv, float value) {
    if (!window || !window->host_ready) return -1;
    SuiRect r = SuiNetDipRect(window, x, y, w, h);
    int idx = sui_ctrl_add_progress(&window->host, id, r.left, r.top, r.right - r.left, r.bottom - r.top, minv, maxv, value);
    SuiNetApplyState(window, idx);
    return idx;
}

SUI_NET_API int SUI_NET_CALL sui_net_spinner_at(SuiNetWindow* window, int id, float x, float y, float w, float h, float minv, float maxv, float value, float step) {
    if (!window || !window->host_ready) return -1;
    SuiRect r = SuiNetDipRect(window, x, y, w, h);
    SuiSpinnerParams p = {};
    p.min = minv; p.max = maxv; p.value = value; p.step = step;
    int idx = sui_ctrl_add_spinner_p(&window->host, id, r.left, r.top, r.right - r.left, r.bottom - r.top, p);
    SuiNetApplyState(window, idx);
    return idx;
}

SUI_NET_API int SUI_NET_CALL sui_net_textview_at(SuiNetWindow* window, int id, float x, float y, float w, float h, const wchar_t* text, int capacity_chars, int readonly) {
    if (!window || !window->host_ready) return -1;
    SuiRect r = SuiNetDipRect(window, x, y, w, h);
    int idx = sui_ctrl_add_textview(&window->host, id, r.left, r.top, r.right - r.left, r.bottom - r.top, text ? text : L"", capacity_chars, readonly);
    SuiNetApplyState(window, idx);
    return idx;
}

SUI_NET_API int SUI_NET_CALL sui_net_swatch_at(SuiNetWindow* window, int id, float x, float y, float w, float h, uint32_t color) {
    if (!window || !window->host_ready) return -1;
    SuiRect r = SuiNetDipRect(window, x, y, w, h);
    int idx = sui_ctrl_add_color_swatch(&window->host, id, r.left, r.top, r.right - r.left, r.bottom - r.top, color);
    if (idx >= 0) window->host.controls[idx].min_w = r.right - r.left;
    SuiNetApplyState(window, idx);
    return idx;
}

SUI_NET_API int SUI_NET_CALL sui_net_container_at(SuiNetWindow* window, int id, float x, float y, float w, float h) {
    if (!window || !window->host_ready) return -1;
    SuiRect r = SuiNetDipRect(window, x, y, w, h);
    return sui_ctrl_add_container(&window->host, id, r.left, r.top, r.right - r.left, r.bottom - r.top);
}

SUI_NET_API int SUI_NET_CALL sui_net_scrollview_at(SuiNetWindow* window, int id, float x, float y, float w, float h, float content_h) {
    if (!window || !window->host_ready) return -1;
    SuiRect r = SuiNetDipRect(window, x, y, w, h);
    return sui_ctrl_add_scrollview(&window->host, id, r.left, r.top, r.right - r.left, r.bottom - r.top, SuiNetDip(window, content_h));
}

SUI_NET_API int SUI_NET_CALL sui_net_image_at(SuiNetWindow* window, int id, float x, float y, float w, float h, int image_id, int circle_mask) {
    if (!window || !window->host_ready) return -1;
    SuiRect r = SuiNetDipRect(window, x, y, w, h);
    int idx = sui_ctrl_add_image(&window->host, id, r.left, r.top, r.right - r.left, r.bottom - r.top, SuiNetImage(window, image_id), circle_mask);
    if (idx >= 0) {
        window->host.controls[idx].min_w = r.right - r.left;
        window->host.controls[idx].min_h = r.bottom - r.top;
    }
    return idx;
}

SUI_NET_API int SUI_NET_CALL sui_net_nav_list_at(SuiNetWindow* window, int id, float x, float y, float w, float h, float item_height, int selected) {
    if (!window || !window->host_ready) return -1;
    SuiRect r = SuiNetDipRect(window, x, y, w, h);
    int idx = sui_ctrl_add_nav_list(&window->host, id, r.left, r.top, r.right - r.left, r.bottom - r.top, SuiNetDip(window, item_height), selected);
    SuiNetApplyState(window, idx);
    return idx;
}

SUI_NET_API void SUI_NET_CALL sui_net_nav_set_content_wheel_region(
    SuiNetWindow* window, int id, float x, float y, float w, float h, int enabled) {
    if (!window || !window->host_ready) return;
    SuiRect r = SuiNetDipRect(window, x, y, w, h);
    sui_ctrl_nav_set_content_wheel_region(
        &window->host, id,
        r.left, r.top, r.right - r.left, r.bottom - r.top, enabled);
}

SUI_NET_API int SUI_NET_CALL sui_net_tabs_at(SuiNetWindow* window, int id, float x, float y, float w, float h, float tab_height, int selected) {
    if (!window || !window->host_ready) return -1;
    SuiRect r = SuiNetDipRect(window, x, y, w, h);
    int idx = sui_ctrl_add_tabs(&window->host, id, r.left, r.top, r.right - r.left, r.bottom - r.top, SuiNetDip(window, tab_height), selected);
    SuiNetApplyState(window, idx);
    return idx;
}

SUI_NET_API void SUI_NET_CALL sui_net_begin(SuiNetWindow* window, float inset_dip, float gap_dip) {
    if (!window || !window->host_ready) return;
    sui_begin(&window->host, sui_inset(inset_dip), gap_dip, SUI_CLEAR_PAGE);
    window->host.min_size_strategy = SUI_MIN_SIZE_FLEX_ROWS;
}

SUI_NET_API void SUI_NET_CALL sui_net_begin_rect(SuiNetWindow* window, float left_dip, float top_dip, float right_dip, float bottom_dip, float gap_dip) {
    if (!window || !window->host_ready) return;
    SuiRect inset = { left_dip, top_dip, right_dip, bottom_dip };
    sui_begin(&window->host, inset, gap_dip, SUI_CLEAR_PAGE);
    window->host.min_size_strategy = SUI_MIN_SIZE_FLEX_ROWS;
}

SUI_NET_API void SUI_NET_CALL sui_net_end(SuiNetWindow* window) {
    if (!window || !window->host_ready) return;
    sui_end(&window->host);
}

SUI_NET_API void SUI_NET_CALL sui_net_vstack(SuiNetWindow* window) {
    if (!window || !window->host_ready) return;
    sui_vstack(&window->host);
}

SUI_NET_API void SUI_NET_CALL sui_net_vgroup(SuiNetWindow* window, float gap_dip) {
    if (!window || !window->host_ready) return;
    sui_vgroup(&window->host, gap_dip);
}

SUI_NET_API void SUI_NET_CALL sui_net_hstack(SuiNetWindow* window, int children, float gap_dip) {
    if (!window || !window->host_ready) return;
    sui_hstack(&window->host, children, gap_dip);
}

SUI_NET_API void SUI_NET_CALL sui_net_flow(SuiNetWindow* window, float item_height_dip, float gap_dip) {
    if (!window || !window->host_ready) return;
    sui_flow(&window->host, item_height_dip, gap_dip);
}

SUI_NET_API void SUI_NET_CALL sui_net_radio_group(SuiNetWindow* window, int group_id) {
    if (!window || !window->host_ready) return;
    sui_radio_group(&window->host, group_id);
}

SUI_NET_API void SUI_NET_CALL sui_net_scrollview(SuiNetWindow* window, int id, float width_dip, float height_dip, float gap_dip) {
    if (!window || !window->host_ready) return;
    sui_scrollview(&window->host, id, width_dip, height_dip, gap_dip);
}

SUI_NET_API void SUI_NET_CALL sui_net_clipbox(SuiNetWindow* window, int id, float width_dip, float height_dip, float gap_dip) {
    if (!window || !window->host_ready) return;
    sui_clipbox(&window->host, id, width_dip, height_dip, gap_dip);
}

SUI_NET_API int SUI_NET_CALL sui_net_label(SuiNetWindow* window, const wchar_t* text) {
    if (!window || !window->host_ready) return -1;
    return sui_label(&window->host, text ? text : L"");
}

SUI_NET_API int SUI_NET_CALL sui_net_title(SuiNetWindow* window, const wchar_t* text) {
    int idx = sui_net_label(window, text);
    if (idx >= 0) {
        SuiControl* c = &window->host.controls[idx];
        c->state |= SUI_CTRL_BOLD;
        c->font_size = SUI_TITLE_PT;
    }
    return idx;
}

SUI_NET_API int SUI_NET_CALL sui_net_tooltip(SuiNetWindow* window, int id, const wchar_t* text) {
    if (!window || !window->host_ready) return -1;
    int idx = sui_tooltip(&window->host, id, NULL);
    if (idx >= 0) SuiNetSetControlTooltip(window, id, text);
    return idx;
}

SUI_NET_API int SUI_NET_CALL sui_net_button(SuiNetWindow* window, int id, const wchar_t* text, int is_default, int enabled) {
    if (!window || !window->host_ready) return -1;
    int flags = (is_default ? SUI_BTN_DEFAULT : 0) | (enabled ? 0 : SUI_BTN_DISABLED);
    return sui_button(&window->host, id, text ? text : L"", flags);
}

SUI_NET_API int SUI_NET_CALL sui_net_checkbox(SuiNetWindow* window, int id, const wchar_t* text, int checked) {
    if (!window || !window->host_ready) return -1;
    int idx = sui_checkbox(&window->host, id, text ? text : L"", checked);
    SuiNetApplyState(window, idx);
    return idx;
}

SUI_NET_API int SUI_NET_CALL sui_net_radio(SuiNetWindow* window, int id, const wchar_t* text, int checked) {
    if (!window || !window->host_ready) return -1;
    int idx = sui_radio(&window->host, id, text ? text : L"", checked);
    SuiNetApplyState(window, idx);
    return idx;
}

SUI_NET_API int SUI_NET_CALL sui_net_edit(SuiNetWindow* window, int id, const wchar_t* text, const wchar_t* placeholder, int numeric_only) {
    if (!window || !window->host_ready) return -1;
    int idx = sui_edit(&window->host, id, text ? text : L"", placeholder ? placeholder : L"", numeric_only);
    SuiNetApplyState(window, idx);
    return idx;
}

SUI_NET_API int SUI_NET_CALL sui_net_combobox(SuiNetWindow* window, int id, const wchar_t** items, int item_count, int selected) {
    if (!window || !window->host_ready) return -1;
    int idx = sui_combobox(&window->host, id, items, item_count, selected);
    SuiNetApplyState(window, idx);
    return idx;
}

SUI_NET_API int SUI_NET_CALL sui_net_slider(SuiNetWindow* window, int id, float minv, float maxv, float value, float step) {
    if (!window || !window->host_ready) return -1;
    int idx = sui_slider(&window->host, id, minv, maxv, value, step);
    SuiNetApplyState(window, idx);
    return idx;
}

SUI_NET_API int SUI_NET_CALL sui_net_progress(SuiNetWindow* window, int id, float minv, float maxv, float value) {
    if (!window || !window->host_ready) return -1;
    int idx = sui_progress(&window->host, id, minv, maxv, value);
    SuiNetApplyState(window, idx);
    return idx;
}

SUI_NET_API int SUI_NET_CALL sui_net_spinner(SuiNetWindow* window, int id, float minv, float maxv, float value, float step) {
    if (!window || !window->host_ready) return -1;
    int idx = sui_spinner(&window->host, id, minv, maxv, value, step);
    SuiNetApplyState(window, idx);
    return idx;
}

SUI_NET_API int SUI_NET_CALL sui_net_textview(SuiNetWindow* window, int id, const wchar_t* text, int capacity_chars, int readonly) {
    if (!window || !window->host_ready) return -1;
    int idx = sui_textview(&window->host, id, text ? text : L"", capacity_chars, readonly);
    SuiNetApplyState(window, idx);
    return idx;
}

SUI_NET_API int SUI_NET_CALL sui_net_nav_list(SuiNetWindow* window, int id, float width_dip, float height_dip, float item_height_dip, int selected) {
    if (!window || !window->host_ready) return -1;
    int idx = sui_nav_list(&window->host, id, width_dip, height_dip, item_height_dip, selected);
    SuiNetApplyState(window, idx);
    return idx;
}

SUI_NET_API int SUI_NET_CALL sui_net_tabs(SuiNetWindow* window, int id, float height_dip, float tab_height_dip, int selected) {
    if (!window || !window->host_ready) return -1;
    int idx = sui_tabs(&window->host, id, height_dip, tab_height_dip, selected);
    SuiNetApplyState(window, idx);
    return idx;
}

SUI_NET_API void SUI_NET_CALL sui_net_nav_add_item(SuiNetWindow* window, int id, const wchar_t* text, const wchar_t* icon, uint32_t icon_color) {
    if (!window || !window->host_ready) return;
    sui_ctrl_nav_add_item(&window->host, id, text ? text : L"", icon ? icon : L"", icon_color);
    SuiControl* c = sui_ctrl_find(&window->host, id);
    if (c) SuiNetApplyState(window, (int)(c - window->host.controls));
}

SUI_NET_API void SUI_NET_CALL sui_net_tabs_add_item(SuiNetWindow* window, int id, const wchar_t* text) {
    if (!window || !window->host_ready) return;
    sui_ctrl_tabs_add_item(&window->host, id, text ? text : L"");
    SuiControl* c = sui_ctrl_find(&window->host, id);
    if (c) SuiNetApplyState(window, (int)(c - window->host.controls));
}

SUI_NET_API void SUI_NET_CALL sui_net_tabs_set_spacing(SuiNetWindow* window, int id, float tab_gap_dip, float content_gap_dip) {
    if (!window || !window->host_ready) return;
    sui_ctrl_tabs_set_spacing(&window->host, id, SuiNetDip(window, tab_gap_dip), SuiNetDip(window, content_gap_dip));
}

SUI_NET_API void SUI_NET_CALL sui_net_last_grow(SuiNetWindow* window, int grow) {
    if (!window || !window->host_ready) return;
    sui_last_grow(&window->host, grow);
}

SUI_NET_API void SUI_NET_CALL sui_net_last_shrink(SuiNetWindow* window, int shrink) {
    if (!window || !window->host_ready) return;
    sui_last_shrink(&window->host, shrink);
}

SUI_NET_API void SUI_NET_CALL sui_net_last_pref(SuiNetWindow* window, float width_dip, float height_dip) {
    if (!window || !window->host_ready) return;
    sui_last_pref(&window->host, width_dip, height_dip);
}

SUI_NET_API void SUI_NET_CALL sui_net_last_max(SuiNetWindow* window, float width_dip, float height_dip) {
    if (!window || !window->host_ready) return;
    sui_last_max(&window->host, width_dip, height_dip);
}

SUI_NET_API int SUI_NET_CALL sui_net_get_checked(SuiNetWindow* window, int id) {
    if (!window) return 0;
    SuiControl* c = window->host_ready ? sui_ctrl_find(&window->host, id) : NULL;
    if (c && (c->type == SUI_CTRL_CHECKBOX || c->type == SUI_CTRL_RADIO)) {
        return (c->state & SUI_CTRL_CHECKED) ? 1 : 0;
    }
    auto it = window->states.find(id);
    return it != window->states.end() && it->second.checked > 0 ? 1 : 0;
}

SUI_NET_API int SUI_NET_CALL sui_net_get_selected(SuiNetWindow* window, int id) {
    if (!window) return 0;
    SuiControl* c = window->host_ready ? sui_ctrl_find(&window->host, id) : NULL;
    if (c && c->type == SUI_CTRL_COMBOBOX) return c->d.combo.selected;
    if (c && c->type == SUI_CTRL_NAV_LIST) return c->d.nav.selected;
    if (c && c->type == SUI_CTRL_TABS) return c->d.tabs.selected;
    auto it = window->states.find(id);
    return it != window->states.end() && it->second.selected >= 0 ? it->second.selected : 0;
}

SUI_NET_API float SUI_NET_CALL sui_net_get_number(SuiNetWindow* window, int id) {
    if (!window) return 0.0f;
    SuiControl* c = window->host_ready ? sui_ctrl_find(&window->host, id) : NULL;
    if (c && c->type == SUI_CTRL_SLIDER) return c->d.slider.value;
    if (c && c->type == SUI_CTRL_SPINNER) return c->d.spinner.value;
    if (c && c->type == SUI_CTRL_PROGRESS) return c->d.progress.value;
    auto it = window->states.find(id);
    return it != window->states.end() && it->second.has_value ? it->second.value : 0.0f;
}

SUI_NET_API int SUI_NET_CALL sui_net_get_text(SuiNetWindow* window, int id, wchar_t* buffer, int buffer_chars) {
    if (!window || !buffer || buffer_chars <= 0) return 0;
    buffer[0] = L'\0';

    std::wstring value;
    SuiControl* c = window->host_ready ? sui_ctrl_find(&window->host, id) : NULL;
    if (c && c->type == SUI_CTRL_EDIT) value = c->text;
    else if (c && c->type == SUI_CTRL_TEXTVIEW && c->d.textview.buf) value.assign(c->d.textview.buf, c->d.textview.len);
    else {
        auto it = window->states.find(id);
        if (it != window->states.end()) value = it->second.text;
    }

    int len = (int)std::min<size_t>(value.size(), (size_t)buffer_chars - 1);
    if (len > 0) sui__wmove(buffer, value.c_str(), len);
    buffer[len] = L'\0';
    return len;
}

SUI_NET_API void SUI_NET_CALL sui_net_set_progress(SuiNetWindow* window, int id, float value) {
    if (!window) return;
    SuiControl* c = window->host_ready ? sui_ctrl_find(&window->host, id) : NULL;
    if (c && c->type == SUI_CTRL_PROGRESS) {
        sui_ctrl_progress_set(&window->host, id, value);
    }
    SuiNetControlState& state = window->states[id];
    state.type = SUI_CTRL_PROGRESS;
    state.value = value;
    state.has_value = true;
    if (window->hwnd) InvalidateRect(window->hwnd, NULL, FALSE);
}

SUI_NET_API void SUI_NET_CALL sui_net_set_combobox_popup_width(SuiNetWindow* window,
                                                               int id,
                                                               float width_dip) {
    if (!window || !window->host_ready) return;
    sui_ctrl_combo_set_popup_width(&window->host, id,
                                   width_dip > 0.0f ? SuiNetDip(window, width_dip) : 0.0f);
}

SUI_NET_API void SUI_NET_CALL sui_net_set_tooltip(SuiNetWindow* window, int id, const wchar_t* text) {
    SuiNetSetControlTooltip(window, id, text);
}

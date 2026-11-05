/* The smallest Slim UI app: a label and a button that counts clicks, in about 60 KB with no extra DLLs.
 *
 * It gets there three ways: controls at exact positions, every unused control left out, and no C runtime.
 * Without the C runtime there's no startup code, so it brings its own entry point, memcpy and memset.
 *
 * Run build-tiny.cmd from a Visual Studio developer prompt to build it. */
#define SUI_FEATURE_CUSTOM_CONTROLS
#define SUI_NO_CRT
#define SUI_NO_COMBOBOX
#define SUI_NO_TABS
#define SUI_NO_LIST
#define SUI_NO_NAV_LIST
#define SUI_NO_SPINNER
#define SUI_NO_SLIDER
#define SUI_NO_COLOR_SWATCH
#define SUI_NO_IMAGE
#define SUI_NO_SHORTCUT_RECORDER
#define SUI_NO_TOOLTIP
#define SUI_NO_SCROLLVIEW
#define SUI_IMPLEMENTATION
#include <slim_ui/slim_ui.h>

#pragma function(memcpy, memset)
void* memcpy(void* dst, const void* src, size_t n) {
    unsigned char* d = dst;
    const unsigned char* s = src;
    while (n--) *d++ = *s++;
    return dst;
}
void* memset(void* dst, int value, size_t n) {
    unsigned char* d = dst;
    while (n--) *d++ = (unsigned char)value;
    return dst;
}
int _fltused = 0;

enum { ID_LABEL = 1, ID_BUTTON };

static SuiRenderer    renderer;
static SuiControlHost host;
static wchar_t        message[64] = L"Hello from Slim UI";
static int            clicks;

static void build(void) {
    float s = (float)host.dpi / 96.0f;
    sui_ctrl_clear_page(&host);
    sui_ctrl_add_label(&host, ID_LABEL, 16 * s, 16 * s, 300 * s, 28 * s, message);
    sui_ctrl_add_button(&host, ID_BUTTON, 16 * s, 48 * s, 120 * s, 32 * s, L"Click me", 1);
}

static void on_click(int id, void* data) {
    (void)data;
    if (id == ID_BUTTON) {
        wsprintfW(message, L"Clicks: %d", ++clicks);
        build();
        InvalidateRect(host.hwnd, NULL, FALSE);
    }
}

static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    LRESULT result = 0;
    if (host.hwnd && sui_default_msg(&host, hwnd, msg, wp, lp, &result)) return result;

    switch (msg) {
    case WM_CREATE:
        sui_renderer_init_fields(&renderer);
        sui_renderer_init(&renderer, hwnd);
        sui_ctrl_host_init(&host, hwnd, &renderer, &sui_scheme_light, sui_get_window_dpi(hwnd));
        host.on_click = on_click;
        return 0;
    case WM_SIZE:
        sui_renderer_resize(&renderer, LOWORD(lp), HIWORD(lp));
        build();
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        sui_renderer_begin(&renderer);
        sui_renderer_clear(&renderer, host.scheme->background);
        sui_ctrl_draw_all(&host);
        sui_renderer_end(&renderer);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        sui_ctrl_host_release(&host);
        sui_renderer_release(&renderer);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void __cdecl __security_init_cookie(void);

void WINAPI WinMainCRTStartup(void) {
    WNDCLASSW window_class = { 0 };
    MSG msg;

    __security_init_cookie();
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = GetModuleHandleW(NULL);
    window_class.lpszClassName = L"Hello";
    sui_enable_dpi_awareness();
    RegisterClassW(&window_class);
    ShowWindow(CreateWindowExW(0, L"Hello", L"Hello", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                               420, 240, NULL, NULL, window_class.hInstance, NULL), SW_SHOWDEFAULT);
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    ExitProcess(0);
}

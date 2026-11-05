<p align="center">
  <img src="logo.svg" alt="Slim UI" width="276">
</p>

Slim UI is a UI toolkit for Windows apps, written in C. Here's what it gives you:

- **Very small apps.** A Hello World window can be as small as 60 KB. The demo, with every control in it, is about 360 KB.
- **One file to ship.** Link statically and your app is a single `.exe` with no extra DLLs. All it loads are libraries that already come with Windows.
- **Customize anything.** Set colors and fonts for the whole app or for a single control. If you need a control to work differently, change it. Slim UI is open source and each control is one small file of plain C (the slider is about 120 lines), so you or an LLM can rewrite it or build your own version.
- **Smooth animation.** Hover effects, tab switches and drop-down lists move in step with your display's refresh rate (vsync), so a 120 Hz screen gets 120 frames a second. When nothing is moving, the animation clock sleeps.
- **GPU effects.** Run your own animated pixel shaders on any control.
- **Sharp on every display.** Everything is drawn with Direct2D at the display's own scale, and text is fitted to the pixel grid so it stays crisp.
- **Nothing to install.** Add one folder of header files to your project. It works from C, C++ and C#.
- **Ready for everyone.** Keyboard navigation, screen reader support, and light and dark themes are built in.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/demo-dark.png">
  <img src="docs/demo-light.png" alt="The demo app, showing checkboxes, radio buttons, drop-down lists and a side menu">
</picture>

## What's included

| Kind | Controls |
|---|---|
| Actions | Buttons |
| Choices | Checkboxes, radio buttons, drop-down lists, sliders, number spinners, color swatches |
| Text | Labels, text fields and multi-line text views, with undo and redo |
| Lists and menus | Lists, tabs, side menus |
| Layout | Containers, scrolling areas |
| Extras | Images, progress bars, tooltips, keyboard and gamepad shortcut recorders |

Slim UI runs on Windows only. A window holds up to 64 controls by default. To raise that, define `SUI_MAX_CONTROLS` before including the header, the same way in every file that includes it. Each extra control costs about 1.6 KB of memory.

## Try the demo

You need Visual Studio 2022 with the C++ tools, and CMake.

```powershell
git clone https://github.com/live-sound/slim-ui.git
cd slim-ui\demo
.\build.ps1
.\build\Release\sui_demo.exe
```

## Your first window

This is a complete program. It shows a label and a button, and the label counts your clicks.

```c
#define SUI_FEATURE_CUSTOM_CONTROLS
#define SUI_FEATURE_DECL
#define SUI_FEATURE_VSYNC
#define SUI_IMPLEMENTATION
#include <slim_ui/slim_ui.h>

enum { ID_BUTTON = 1 };

typedef struct App {
    SuiRenderer    renderer;
    SuiControlHost host;
    wchar_t        message[64];
    int            clicks;
} App;

/* Describes the whole window. Call it again whenever something changes. */
static void build(App* app) {
    SuiControlHost* host = &app->host;
    sui_begin(host, sui_inset(16.0f), 8.0f, SUI_CLEAR_PAGE);
    sui_label(host, app->message);
    sui_button(host, ID_BUTTON, L"Click me", SUI_BTN_DEFAULT);
    sui_end(host);
}

static void on_click(int id, void* data) {
    App* app = data;
    if (id == ID_BUTTON) {
        wsprintfW(app->message, L"Clicks: %d", ++app->clicks);
        build(app);
        InvalidateRect(app->host.hwnd, NULL, FALSE);
    }
}

static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    App* app = (App*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    LRESULT result = 0;

    /* Mouse, keyboard, focus, timers and screen readers are handled here. */
    if (app && sui_default_msg(&app->host, hwnd, msg, wp, lp, &result)) return result;

    switch (msg) {
    case WM_CREATE:
        app = ((CREATESTRUCTW*)lp)->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)app);
        sui_renderer_init_fields(&app->renderer);
        sui_renderer_init(&app->renderer, hwnd);
        sui_ctrl_host_init(&app->host, hwnd, &app->renderer, &sui_scheme_light, GetDpiForWindow(hwnd));
        app->host.on_click = on_click;
        app->host.on_click_data = app;
        return 0;
    case WM_SIZE:
        sui_renderer_resize(&app->renderer, LOWORD(lp), HIWORD(lp));
        build(app);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        sui_renderer_begin(&app->renderer);
        sui_renderer_clear(&app->renderer, app->host.scheme->background);
        sui_ctrl_draw_all(&app->host);
        sui_renderer_end(&app->renderer);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_SUI_VSYNC:
        sui_ctrl_on_vsync(&app->host);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    case WM_DESTROY:
        sui_ctrl_host_release(&app->host);
        sui_renderer_release(&app->renderer);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE prev, PWSTR args, int show) {
    static App app = { .message = L"Hello from Slim UI" };
    WNDCLASSW window_class = { .lpfnWndProc = window_proc, .hInstance = instance, .lpszClassName = L"Hello" };
    MSG msg;

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    RegisterClassW(&window_class);
    ShowWindow(CreateWindowExW(0, L"Hello", L"Hello", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                               420, 240, NULL, NULL, instance, &app), show);
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
```

Save it as `hello.c`, put Slim UI in a `slim-ui` folder next to it, and add this `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.21)
project(hello C)

# Puts the C runtime inside hello.exe, so there are no extra DLLs to ship.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_subdirectory(slim-ui)

add_executable(hello WIN32 hello.c)
target_link_libraries(hello PRIVATE slim_ui)
```

Then build and run it:

```powershell
cmake -B build
cmake --build build --config MinSizeRel
.\build\MinSizeRel\hello.exe
```

If you'd rather skip CMake, run this from a Visual Studio developer prompt instead:

```bat
cl /O1 /utf-8 /std:c11 /I slim-ui\include hello.c d2d1.lib dwrite.lib dwmapi.lib user32.lib gdi32.lib ole32.lib shell32.lib windowscodecs.lib
```

## How it works

A window needs two things, and `hello.c` creates both in `WM_CREATE`:

- a **renderer** (`SuiRenderer`), which does the drawing
- a **host** (`SuiControlHost`), which holds the controls and keeps track of hover, focus and animation

The rest is an ordinary window procedure with four additions:

1. `sui_default_msg` goes first. It handles the mouse, the keyboard, focus and screen readers, so you don't have to.
2. `WM_SIZE` calls `build`, because where controls go depends on the size of the window.
3. `WM_PAINT` draws everything with `sui_ctrl_draw_all`.
4. `WM_SUI_VSYNC` moves animations on by one frame.

`build` describes the window using your app's data. When the data changes, call it again, as `on_click` does above. This recreates the controls, so keep anything you need to remember in your own struct, including what someone has typed.

A finished app should also handle `WM_DPICHANGED`, for when the window moves to a monitor with a different scale. The [full window procedure](docs/reference.md#full-wndproc-wiring) in the reference shows how.

## Placing controls

There are two ways to do it, and you can use both in the same window.

**Automatic layout** is what `hello.c` uses. You put controls in rows and columns, and Slim UI works out where everything goes. Sizes scale with the display on their own. It needs `SUI_FEATURE_DECL`.

```c
sui_begin(host, sui_inset(16.0f), 8.0f, SUI_CLEAR_PAGE);
    sui_label(host, L"Settings");
    sui_hstack(host, 2, 12.0f);
        sui_checkbox(host, ID_AUTOSTART, L"Start with Windows", 1);
        sui_checkbox(host, ID_NOTIFY, L"Show notifications", 0);
    sui_end(host);
    sui_button(host, ID_SAVE, L"Save", SUI_BTN_DEFAULT);
sui_end(host);
```

**Exact positions** are for when you want full control. Each control gets a rectangle in physical pixels, so scaling for the display is up to you.

```c
sui_ctrl_add_label(host, ID_TITLE, 16, 16, 300, 24, L"Settings");
sui_ctrl_add_checkbox(host, ID_AUTOSTART, 16, 48, 300, 28, L"Start with Windows", 1);
sui_ctrl_add_button(host, ID_SAVE, 16, 84, 120, 32, L"Save", 1);
```

## Everyday tasks

**Read a value.** Find the control by its ID:

```c
float volume = sui_ctrl_find(host, ID_VOLUME)->d.slider.value;
int autostart = (sui_ctrl_find(host, ID_AUTOSTART)->state & SUI_CTRL_CHECKED) != 0;
const wchar_t* name = sui_ctrl_find(host, ID_NAME)->text;
```

**Switch to dark mode:**

```c
app->host.scheme = &sui_scheme_dark;
InvalidateRect(hwnd, NULL, FALSE);
```

**Add a tooltip:**

```c
sui_ctrl_set_tooltip(host, ID_SAVE, L"Saves your changes.");
```

**Restyle a single control:**

```c
sui_ctrl_set_style_color(host, ID_SAVE, SUI_STYLE_BG_COLOR, 0xFF8D28);
sui_ctrl_set_font_family(host, ID_SAVE, L"Cambria");
```

**Change the font everywhere:**

```c
SuiTypography fonts = sui_typography_default;
wcscpy_s(fonts.body.family, 64, L"Cambria");
sui_renderer_apply_typography(&app->renderer, &fonts);
```

**Change how a control works.** Each control has its own file, such as `include/slim_ui/sui_ctrl_slider.h`, with its drawing and input handling together. Edit it yourself, or ask an LLM to change it or write a new version for you.

**Make your program smaller.** Here's what each step does to a Hello World window. All three are a single `.exe` with no extra DLLs.

| Hello World | Size |
|---|---|
| `hello.c` from this page, with everything on | 380 KB |
| Only a label and a button | 179 KB |
| No C runtime either | 60.5 KB |

To leave out controls you don't use, define `SUI_NO_TABS`, `SUI_NO_LIST` and others like them before including the header. Automatic layout needs those controls, so this only works with exact positions. [`examples/tiny.c`](examples/tiny.c) is the smallest version, and `examples\build-tiny.cmd` builds it from a Visual Studio developer prompt.

## Shaders

Slim UI draws with Direct2D, so a pixel shader can run on any control. `sui_ctrl_set_paint` hands one control's drawing to your function:

```c
sui_ctrl_set_paint(host, ID_LOGO, paint_with_shader, app);
```

Your function calls `sui_ctrl_draw_self` to draw the control into a bitmap, then runs the shader over that bitmap on its way to the screen. Nothing else about the control changes, so a text field with a shader still takes typing, focus and clicks.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/shader-dark.png">
  <img src="docs/shader-light.png" alt="The demo's Shader page: the logo rippling under a bar of light, and a text field with light running around its border">
</picture>

The demo's Shader page does this with an animated ripple on an image and a glowing border on a text field. Start with `PaintWithShader` in [`demo/main.cpp`](demo/main.cpp). The shaders are [`demo/shader_ripple.hlsl`](demo/shader_ripple.hlsl) and [`demo/shader_glow.hlsl`](demo/shader_glow.hlsl), and [`demo/shader_effect.cpp`](demo/shader_effect.cpp) connects them to Direct2D. That code has its own file because the real Direct2D headers can't share one with `slim_ui.h`.

## Using it from C#

The `dotnet` folder wraps Slim UI for .NET 8, so you can build the same windows from C# without writing any Win32 code. Its [README](dotnet/README.md) explains how.

## Learn more

- The [reference](docs/reference.md) covers every function, control, option and limit.
- [`demo/main.cpp`](demo/main.cpp) is a full app that uses every control.
- `tests\build-tests.ps1` builds and runs the tests.

## License

Slim UI is free to use under the [MIT License](LICENSE).

# Slim UI reference

Every function, control, option and limit. New here? Start with the [README](../README.md).

Pure C, header-only Direct2D + DirectWrite UI toolkit for Windows 7 or later. No CRT
required, no third-party deps beyond Windows itself, single-file
implementation idiom.

## Table of contents

- [Overview](#overview)
- [Quick start](#quick-start)
- [.NET wrapper](#net-wrapper)
- [Feature macros](#feature-macros)
- [Architecture](#architecture)
- [Core types](#core-types)
- [Renderer](#renderer)
- [Control host](#control-host)
- [Controls — imperative API](#controls--imperative-api)
- [Controls — declarative API](#controls--declarative-api)
- [Undo / Redo](#undo--redo)
- [Accessibility (UIA)](#accessibility-uia)
- [Layout helpers](#layout-helpers)
- [Vsync animation pump](#vsync-animation-pump)
- [Theming (color schemes)](#theming-color-schemes)
- [Typography](#typography)
- [Full WndProc wiring](#full-wndproc-wiring)
- [Constants reference](#constants-reference)
- [Repository layout](#repository-layout)
- [License](#license)

## Overview

Slim UI draws Win32 windows with Direct2D + DirectWrite instead of GDI or
common controls. It owns layout, hit testing, focus, animation, tooltips,
and accessibility. You wire it into a single `WNDPROC` and feed it the
window messages you already handle.

Two complementary APIs sit on the same control set:

- **Imperative**: `sui_ctrl_add_*(host, id, x, y, w, h, ...)` — explicit
  pixel rects, you own placement.
- **Declarative**: `sui_begin / sui_vstack / sui_hstack / sui_flow / sui_end`
  with `sui_button / sui_label / sui_checkbox / ...` — auto-layout, the
  toolkit derives intrinsic min size for `WM_GETMINMAXINFO`.

Both styles can coexist in the same window.

## Quick start

```c
#define SUI_FEATURE_CUSTOM_CONTROLS   /* unlock the control host + controls */
#define SUI_FEATURE_VSYNC             /* opt-in vblank-locked animation pump */
#define SUI_FEATURE_DECL              /* opt-in declarative API + flex distribution (omit for imperative-only consumers) */
#define SUI_IMPLEMENTATION            /* emit definitions in this TU */
#include <slim_ui/slim_ui.h>
```

Include `slim_ui.h` without `SUI_IMPLEMENTATION` in every other TU that
needs the types.

Before creating windows, call `sui_enable_dpi_awareness()`. It tries per-monitor
v2, then per-monitor v1, then system DPI awareness on Windows 7. An awareness
mode already set by a manifest or the application is preserved.
Use `sui_get_window_dpi(hwnd)` to read window DPI with a system-DPI fallback.

```c
typedef struct App {
    SuiRenderer    renderer;
    SuiControlHost host;
    SuiColorScheme scheme;
    UINT           dpi;
} App;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    App* a = (App*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    switch (msg) {
    case WM_CREATE:
        a = (App*)((CREATESTRUCTW*)lp)->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)a);
        a->dpi    = sui_get_window_dpi(hwnd);
        a->scheme = sui_scheme_light;
        sui_renderer_init_fields(&a->renderer);
        sui_renderer_init(&a->renderer, hwnd);
        sui_ctrl_host_init(&a->host, hwnd, &a->renderer, &a->scheme, a->dpi);
        a->host.on_click = OnClick;
        a->host.on_click_data = a;
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps; BeginPaint(hwnd, &ps);
        sui_renderer_begin(&a->renderer);
        sui_renderer_clear(&a->renderer, a->scheme.background);
        Build(a);                         /* declarative tree */
        sui_ctrl_draw_all(&a->host);
        sui_renderer_end(&a->renderer);
        EndPaint(hwnd, &ps);
        return 0;
    }
    /* ...mouse / keyboard / vsync forwarding -- see "Full WndProc wiring" */
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
```

The complete reference app lives in `demo/main.cpp`. It exercises every
control across seven sections: Basic, Choices, Text input, Containers, Flex,
Decl containers, and Shader. `examples/tiny.c` is the smallest possible app.

## .NET wrapper

`dotnet/` contains the C# wrapper for Slim UI:

- `dotnet/native/` builds `SlimUi.Native.dll`, a small exported C ABI over
  the header-only Slim UI toolkit.
- `dotnet/src/SlimUi/` contains the `net8.0-windows` C# wrapper.
- `dotnet/samples/SlimUi.Sample/` contains the .NET demo. It matches the
  native `demo/main.cpp`: same sections, chrome, theme and typography
  pickers, logo flow, and resize behavior.

Window dimensions passed to `SlimWindow` / `SlimApplication.Run` are outer-window
sizes in DIPs. The bridge scales them for the initial display; client dimensions
and the native handle are valid during the first build callback.

The gallery uses a sample-specific native bridge with the same shader effects
as the C++ demo. Its build copies `SlimUi.Sample.Native.dll` as `SlimUi.Native.dll`
and includes `logo.png`; the regular wrapper DLL does not include demo effects.

Build the wrapper and sample from `dotnet/`:

```powershell
.\build.ps1
```

Run the .NET demo:

```powershell
dotnet run --project .\samples\SlimUi.Sample\SlimUi.Sample.csproj -c Release
```

Publish the .NET demo with Native AOT:

```powershell
dotnet publish .\samples\SlimUi.Sample\SlimUi.Sample.csproj -c Release -p:PublishAot=true
```

Native AOT is supported for the managed wrapper and sample, but the
published app still ships with `SlimUi.Native.dll`; it is AOT-compatible,
not a single fully static executable.

## Feature macros

| Macro                          | Effect                                                                                     |
|--------------------------------|--------------------------------------------------------------------------------------------|
| `SUI_IMPLEMENTATION`           | Emit definitions. Define in **exactly one** TU.                                            |
| `SUI_FEATURE_CUSTOM_CONTROLS`  | Compile `SuiControl`, `SuiControlHost`, and every `sui_ctrl_*`.                              |
| `SUI_FEATURE_DECL`             | Compile the declarative container API (`sui_begin / sui_vstack / sui_hstack / sui_end / sui_<atom>` and flex distribution). Opt-in; imperative consumers can skip it. |
| `SUI_FEATURE_VSYNC`            | Compile the `SuiVsyncState` vblank pump and route hover animation through `WM_SUI_VSYNC`.   |
| `SUI_NO_CRT`                   | Allocate via `HeapAlloc` instead of `malloc`. Lets the toolkit ship in CRT-free executables.|
| `SUI_MALLOC(n)` / `SUI_FREE(p)` | Override the allocator. Define **both** before including `slim_ui.h` in the implementation TU. Default: `SUI_NO_CRT` → `HeapAlloc`/`HeapFree`; otherwise `malloc`/`free`. |

Optional controls can be left out of a build by defining any of these before `slim_ui.h` in the implementation translation unit: `SUI_NO_COMBOBOX`, `SUI_NO_TABS`, `SUI_NO_LIST`, `SUI_NO_NAV_LIST`, `SUI_NO_SPINNER`, `SUI_NO_SLIDER`, `SUI_NO_COLOR_SWATCH`, `SUI_NO_IMAGE`, `SUI_NO_SHORTCUT_RECORDER`, `SUI_NO_TOOLTIP`, `SUI_NO_SCROLLVIEW`. These remove the corresponding constructors and exclude shared drawing, event and UIA paths for the omitted controls. Accessibility for enabled controls is preserved, including text views. The size reduction depends on the build and controls used. These flags do not combine with `SUI_FEATURE_DECL`, whose builders call those controls.

`SUI_FEATURE_VSYNC` is included by `slim_ui.h` near the top so
`SuiControlHost` can embed a per-window vsync state. Without it, hover
animation falls back to a `WM_TIMER` (`SUI_HOVER_TIMER_ID`) at ~60 Hz.

## Architecture

```
┌──────────────────┐    ┌──────────────────┐    ┌──────────────────┐
│ SuiRenderer      │ ◄── │ SuiControlHost   │ ◄── │ Your WndProc     │
│  D2D HWND target │    │  controls[]      │    │  forwards events │
│  DWrite formats  │    │  hit/focus/anim  │    │  draws on PAINT  │
│  scratch brush   │    │  scheme + typo   │    └──────────────────┘
└──────────────────┘    └──────────────────┘
                              │
                              ├─ imperative: sui_ctrl_add_*  (+ explicit rect)
                              └─ declarative: sui_begin → sui_button / ... → sui_end
```

- One `SuiRenderer` per HWND (owns the D2D HWND target + DWrite text formats).
- One `SuiControlHost` per HWND (owns the control array, focus, hover,
  tooltip window, combo popup, vsync pump, UIA providers).
- `SuiColorScheme` and `SuiTypography` are plain value types, swapped in
  via pointers / `sui_renderer_apply_typography`.

Controls are stored in a fixed-size array on the host (`SUI_MAX_CONTROLS`).
There is no allocator; rebuild the tree on `WM_SIZE` / `WM_DPICHANGED`.

## Core types

### `SuiRenderer`

Owns the D2D HWND render target, three DirectWrite text formats (body,
bold, title), and a single scratch `ID2D1SolidColorBrush` recolored per
draw call. Lives in `slim_ui.h`. Text is grayscale, smoothed both ways and
fitted to the pixel grid, which keeps strokes even and baselines on pixel rows.
Every layout is a GDI-natural one, on whole-pixel spacing, so what is measured
matches what is drawn.

### `SuiControl`

Tagged union of every control. `state` is a bitmask of `SuiCtrlState`
flags (`SUI_CTRL_HOVER | SUI_CTRL_PRESSED | SUI_CTRL_FOCUSED |
SUI_CTRL_DISABLED | SUI_CTRL_HIDDEN | SUI_CTRL_CHECKED | SUI_CTRL_DEFAULT |
SUI_CTRL_BOLD | SUI_CTRL_ANCHOR_RIGHT | SUI_CTRL_ANCHOR_BOTTOM |
SUI_CTRL_FLOW | SUI_CTRL_LABEL_NO_DRAW`).
`SUI_CTRL_LABEL_NO_DRAW` lets a host paint a label itself while retaining its accessible text and bounds.
`SUI_CTRL_TEXT_MAX` defaults to 128; define it before including the header to allow longer control text, consistently across translation units.
`hover_t` is a 0–1 float driven by the hover animation timer (or vsync
pump) for smooth color/border transitions. `parent_id` and `z` form the
container/z-order tree (`-1` = top-level).

#### Flex sizing fields

Each control carries six layout hints, all defaulting to 0 (with
`shrink = 1`) so unset controls keep the legacy elastic behavior:

| Field           | Meaning                                                                                          |
|-----------------|--------------------------------------------------------------------------------------------------|
| `min_w / min_h` | Hard floor used by `sui_ctrl_min_size` and by flex shrink — the row scan picks `max(intrinsic, min_w/h)`. |
| `pref_w / pref_h` | Preferred size for two-pass distribution. `0` means "use the intrinsic floor" (text-derived width or per-type default). |
| `max_w / max_h` | Cap applied during distribution (grow stops here). `0` means unbounded. |
| `grow`          | Non-zero share of leftover space when `Σpref < available`. Distributed proportionally to each slot's `grow`. Also acts as fallback shrink basis when `Σpref > available` and no slot sets `shrink`, so growers shrink with the parent. |
| `shrink`        | Non-zero share of the deficit when `Σpref > available`. Defaults to `1` so atoms shrink under overflow. |

Units match `c->w` / `c->min_w`: scaled physical pixels. The declarative
helpers (`sui_last_pref`, `sui_last_max`) DPI-scale DIPs internally
before writing.

`HSTACK` and `VSTACK` containers run a two-pass distribute at `sui_end`:
gather pref → distribute leftover (or shrink deficit) → write final
`x/w` (HSTACK) or `y/h` (VSTACK). VSTACK distribute is a Phase 2b first
cut: it only fires for atom-only `VSTACK`s directly under the `ROOT`
container (so the inherited `parent->h` is non-zero). Mixed
atom/container `VSTACK` siblings fall back to the legacy intrinsic
stack.

All controls stay elastic by default; callers that want a fixed visual
floor (e.g. a 200dp settings hotkey, a fixed swatch, a fixed image) set
`c->min_w` / `c->min_h` after add. Atoms (`SUI_CTRL_COLOR_SWATCH`,
`SUI_CTRL_IMAGE`) have no derivable intrinsic and collapse to 0 in row
scans without an explicit pin; `SUI_CTRL_SHORTCUT_RECORDER` has an
`~80dp × 28dp` intrinsic floor so
the chip stays legible at min.

### `SuiControlHost`

```c
typedef struct SuiControlHost {
    HWND                  hwnd;
    SuiRenderer          *renderer;
    const SuiColorScheme *scheme;
    UINT                  dpi;

    SuiControl controls[SUI_MAX_CONTROLS];
    int        control_count;
    int        focused_control;        /* index, not id */
    int        hover_control;

    SuiCtrlClickFn   on_click;          /* fires for button/checkbox/radio/swatch/slider/nav/spinner */
    SuiFocusChangeFn on_focus_change;
    void            *on_click_data;
    void            *on_focus_change_data;

    SuiVsyncState vsync;                /* per-host pump (SUI_FEATURE_VSYNC) */
    int           hover_vsync_active;

    float min_client_w, min_client_h;   /* per-axis OVERRIDE for sui_handle_wm_getminmaxinfo */
    int   min_size_strategy;            /* SUI_MIN_SIZE_FLEX_ROWS for column rebuild on WM_SIZE */
    /* ...declarative container stack, intrinsic_min_w/h, tooltip + combo popup, UIA providers */
} SuiControlHost;
```

### `SuiColorScheme`

Packed-`AARRGGBB` palette covering every control role: background, text,
buttons, checkbox, edit, slider, nav list, progress, button-area panel.
Two built-ins: `sui_scheme_light`, `sui_scheme_dark`. Defined in
`slim_ui.h`.

### `SuiTypography`

Four roles (`body`, `subtitle`, `title`, `icon`); each has `family[64]`,
`size_pt`, `weight` (CSS 100–900), `italic`. Default registered as
`"default"`; `sui_typography_default` is the const struct.

## Renderer

| Function                                               | Purpose                                                                |
|--------------------------------------------------------|------------------------------------------------------------------------|
| `sui_renderer_init_fields(r)`                          | Zero-init field defaults (font family/size). Call before `sui_renderer_init`. |
| `sui_renderer_init(r, hwnd)`                           | Create the D2D factory + HWND render target + DWrite factory.          |
| `sui_renderer_release(r)`                              | Release every COM object.                                              |
| `sui_renderer_resize(r, w, h)`                         | Forward `WM_SIZE` to the render target.                                |
| `sui_renderer_begin(r) / sui_renderer_end(r)`          | `BeginDraw` / `EndDraw`. `_end` auto-recreates the target on `D2DERR_RECREATE_TARGET`. |
| `sui_renderer_clear(r, color)`                         | Clear to packed `0xAARRGGBB`.                                          |
| `sui_renderer_set_font(r, family, size_pt)`            | Update body role + rebuild text formats.                               |
| `sui_renderer_apply_typography(r, &typo)`              | Swap all four roles at once.                                           |

Drawing primitives (all DPI-agnostic, caller passes already-scaled DIPs):

```c
sui_renderer_fill_rect / sui_renderer_fill_rounded_rect / sui_renderer_draw_rounded_rect
sui_renderer_fill_ellipse / sui_renderer_draw_ellipse
sui_renderer_draw_line / sui_renderer_draw_filled_ring / sui_renderer_draw_progress
sui_renderer_push_clip / sui_renderer_pop_clip
sui_renderer_draw_text / _centered / _middle / _middle_singleline
sui_renderer_measure_text_width / _height
sui_renderer_load_image / sui_renderer_draw_image / sui_renderer_free_image
sui_color_lerp(c0, c1, t)        /* premultiplied AARRGGBB lerp */
```

Rounded rects (fills, borders, `sui_renderer_draw_filled_ring`'s inner edge and
`sui_renderer_mask_outside_rounded_rect`'s corners) have the continuous corner of
`sui_corner.h`. Each shape is path geometry built on first use into a 32-shape
cache in the renderer and drawn translated, so a shape seen again builds nothing.
A `SUI_NO_CRT` build draws Direct2D's circular rounded rect.

`sui_renderer_load_image` decodes via WIC and returns a `SuiImageHandle`
suitable for `sui_ctrl_add_image`. Free with `sui_renderer_free_image`.

## Control host

### Lifecycle

```c
sui_ctrl_host_init(&host, hwnd, &renderer, &scheme, dpi);
host.on_click            = OnClick;        /* int ctrl_id, void* user */
host.on_click_data       = userdata;
host.on_focus_change     = OnFocus;        /* int old_id, int new_id, void* user */
host.on_focus_change_data= userdata;
/* ...build controls (imperative or declarative) */
sui_ctrl_host_release(&host);              /* WM_DESTROY */
```

`sui_ctrl_clear_page(&host)` truncates the array back to
`persistent_control_count` (set this once after constructing controls
that survive across page rebuilds — e.g. a sidebar — then call
`sui_ctrl_clear_page` and rebuild only the page body).

### Drawing

`sui_ctrl_draw_all(&host)` walks the control tree in z-order, applying
hover-color interpolation via `hover_t`. Call between
`sui_renderer_begin` and `sui_renderer_end`.

### Event handlers (forward from your WNDPROC)

`sui_default_msg` handles the ~15 messages every slim-ui consumer wires
identically.  Call it at the top of your WndProc:

```c
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    LRESULT r = 0;
    if (sui_default_msg(&host, hwnd, msg, wp, lp, &r)) return r;
    switch (msg) {
    case WM_PAINT: /* ...render... */ return 0;
    case WM_SIZE:  /* ...resize... */ return 0;
    case WM_DESTROY: sui_ctrl_host_release(&host); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
```

`sui_default_msg` covers: `WM_ERASEBKGND`, `WM_MOUSEMOVE` (+ `SetCursor` +
`TrackMouseEvent`), `WM_MOUSELEAVE`, `WM_LBUTTONDOWN` / `DBLCLK` / `UP`,
`WM_CONTEXTMENU`, `WM_MOUSEWHEEL`, `WM_SETFOCUS` / `WM_KILLFOCUS` (caret
timer via `SUI_CARET_TIMER_ID`), `WM_CAPTURECHANGED` / `WM_CANCELMODE` (cancel
spinner holds, then fall through), `WM_TIMER`, `WM_KEYDOWN` / `UP`,
`WM_CHAR`, `WM_GETOBJECT`.

`WM_GETMINMAXINFO` is **not** included — it is only safe for
absolute-coordinate layouts.  Call `sui_handle_wm_getminmaxinfo` explicitly
where appropriate.

For consumers that need to intercept individual messages before the default
handling, the per-message calls are:

| Window message     | Host call                                                            |
|--------------------|----------------------------------------------------------------------|
| `WM_ERASEBKGND`    | return `TRUE`                                                        |
| `WM_MOUSEMOVE`     | `sui_ctrl_on_mouse_move(&host, x, y)` + `SetCursor` + `TrackMouseEvent` |
| `WM_MOUSELEAVE`    | `sui_ctrl_on_mouse_leave(&host)`                                     |
| `WM_LBUTTONDOWN`   | `SetFocus(hwnd)` + `sui_ctrl_on_lbutton_down(&host, x, y)`           |
| `WM_LBUTTONUP`     | `sui_ctrl_on_lbutton_up(&host, x, y)`                                |
| `WM_LBUTTONDBLCLK` | `SetFocus(hwnd)` + `sui_ctrl_on_lbutton_dblclk(&host, x, y)`        |
| `WM_CONTEXTMENU`   | `sui_ctrl_on_rbutton_up(&host, x, y)` (after `ScreenToClient`)       |
| `WM_MOUSEWHEEL`    | `sui_ctrl_on_mouse_wheel(&host, x, y, GET_WHEEL_DELTA_WPARAM(wp))`   |
| `WM_SETFOCUS`      | `host.window_has_focus=1; host.caret_visible=1; SetTimer(hwnd, SUI_CARET_TIMER_ID, 500, NULL)` |
| `WM_KILLFOCUS`     | `sui_ctrl_spinner_stop_repeat(&host); host.window_has_focus=0; KillTimer(hwnd, SUI_CARET_TIMER_ID); InvalidateRect(hwnd,NULL,FALSE)` |
| `WM_CAPTURECHANGED` / `WM_CANCELMODE` | `sui_ctrl_spinner_stop_repeat(&host)` then normal window processing |
| `WM_KEYDOWN`       | `sui_ctrl_on_keydown(&host, vk)`                                     |
| `WM_KEYUP`         | `sui_ctrl_on_keyup(&host, vk)`                                       |
| `WM_CHAR`          | `sui_ctrl_on_char(&host, (wchar_t)wp)`                               |
| `WM_TIMER`         | `sui_ctrl_on_timer(&host, wp)`                                       |
| `WM_GETOBJECT`     | `sui_uia_handle_getobject(&host, wp, lp)`                            |
| `WM_SUI_VSYNC`     | `sui_ctrl_on_vsync(&host);` then invalidate |
| `WM_GETMINMAXINFO` | `sui_handle_wm_getminmaxinfo(&host, hwnd, lp)` (declarative trees only) |

`sui_ctrl_get_cursor(&host, x, y)` returns the appropriate `HCURSOR` for
the control under the cursor (text I-beam over edits, hand over
buttons, etc.); set it from `WM_MOUSEMOVE`.

### Tooltip

```c
int idx = sui_ctrl_add_button(&host, ID_RESET, x, y, w, h, L"Reset", 0);
sui_ctrl_set_tooltip(&host, ID_RESET, L"Resets all settings to defaults.");

sui_ctrl_add_tooltip(&host, ID_HELP, x, y, 16 * s, 16 * s,
                     L"Shown from a compact '?' info marker.");
```

Tooltips use SlimUI's own layered Win32 popup window
(`SlimUiTooltipPopup`), not `COMCTL32` `TOOLTIPS_CLASS`. The popup is
rendered with Direct2D as a rounded rectangle with a DPI-scaled,
per-pixel-alpha shadow and is shared by the host. The shadow peaks at 16/255
opacity (about 6%) with an 8-DIP margin and 2-DIP downward offset for a light,
tight shadow.
Its text is drawn like the main window's; only the antialiased rounded
perimeter and shadow remain translucent.
`sui_ctrl_set_tooltip` copies text into the control, so callers do not
need to keep the source string alive. `sui_ctrl_add_tooltip` creates a
compact `?` info marker control (`SUI_CTRL_TOOLTIP`) whose only purpose
is to expose a hover tooltip.

## Controls — imperative API

Every factory returns the index in `host.controls[]`, or `-1` on
overflow (`SUI_MAX_CONTROLS`). Coordinates are physical pixels — the
caller is responsible for DPI scaling.

```c
int sui_ctrl_add_button       (host, id, x, y, w, h, text, is_default);
int sui_ctrl_add_checkbox     (host, id, x, y, w, h, text, checked);
int sui_ctrl_add_radio        (host, id, x, y, w, h, text, group_id, checked);
int sui_ctrl_add_label        (host, id, x, y, w, h, text);
int sui_ctrl_add_edit         (host, id, x, y, w, h, text, placeholder, numeric_only);
int sui_ctrl_add_combobox     (host, id, x, y, w, h, items[], count, selected);
int sui_ctrl_add_slider       (host, id, x, y, w, h, min, max, value, step);
int sui_ctrl_add_slider_p     (host, id, x, y, w, h, SuiSliderParams);
int sui_ctrl_add_spinner      (host, id, x, y, w, h, min, max, value, step);
int sui_ctrl_add_spinner_p    (host, id, x, y, w, h, SuiSpinnerParams);
int sui_ctrl_add_color_swatch (host, id, x, y, w, h, color);
int sui_ctrl_add_nav_list     (host, id, x, y, w, h, item_height, selected);
int sui_ctrl_add_progress     (host, id, x, y, w, h, min, max, value);
int sui_ctrl_add_image        (host, id, x, y, w, h, SuiImageHandle, circle_mask);
int sui_ctrl_add_shortcut_recorder_keyboard_physical(host, id, x, y, w, h, vk, modifiers, scan_code, extended, mouse_button, prompt);
int sui_ctrl_add_shortcut_recorder_gamepad(host, id, x, y, w, h, buttons, prompt);
int sui_ctrl_add_container    (host, id, x, y, w, h);
int sui_ctrl_add_textview     (host, id, x, y, w, h, initial_text, capacity_chars, readonly);
int sui_ctrl_add_scrollview   (host, id, x, y, w, h, content_h);
int sui_ctrl_add_tooltip      (host, id, x, y, w, h, tooltip);

void sui_ctrl_nav_add_item    (host, ctrl_id, text, icon, icon_color);
void sui_ctrl_nav_set_content_wheel_region(host, ctrl_id, x, y, w, h, enabled);
void sui_ctrl_combo_set_popup_width(host, ctrl_id, popup_w);
void sui_ctrl_image_set       (host, id, SuiImageHandle);
void sui_ctrl_image_set_scale (host, id, SuiImageScale);
void sui_ctrl_progress_set    (host, id, value);
void sui_ctrl_textview_set_text    (host, id, text);
const wchar_t* sui_ctrl_textview_get_text(host, id);
void sui_ctrl_textview_set_readonly(host, id, readonly);
void sui_ctrl_set_scrollview_content_height(host, id, content_h);
void sui_ctrl_set_font_size   (host, idx, size_pt);
void sui_ctrl_set_z           (host, idx, z);
void sui_ctrl_set_parent      (host, idx, parent_id);
void sui_ctrl_set_button_colors(host, idx, face, border, text, hover, press);
void sui_ctrl_set_anchor      (host, id, flags, right_margin, bottom_margin);
void sui_ctrl_set_container_clip(host, id, clip);
void sui_ctrl_set_tooltip     (host, ctrl_id, text);
void sui_ctrl_set_paint       (host, id, fn, user);
void sui_ctrl_draw_self       (host, ctrl);
SuiControl* sui_ctrl_find     (host, id);
```

`sui_ctrl_set_paint` hands one control's drawing to `fn`, a
`void (*)(SuiControlHost* host, SuiControl* ctrl, void* user)`. `fn` runs where the
control would have been drawn, so clipping and z-order still apply. It isn't called
for a hidden control. `sui_ctrl_draw_self` draws the control the usual way, without
its children, so `fn` can point `renderer.render_target` at a bitmap, draw the control
into it, restore the target and run a pixel shader over the result. Input, focus and
hit testing don't change. Rebuilding the control clears the function. `demo/main.cpp`
has a working example in `PaintWithShader`.

`sui_ctrl_nav_set_content_wheel_region` is an opt-in navigation fallback for
settings-style pages. A complete wheel detent inside the external content
rectangle selects one adjacent nav item without wrapping and fires the nav's
normal `on_click` callback. High-resolution wheel deltas are accumulated. If
any visible `TEXTVIEW`, `NAV_LIST`, `LIST`, or `SCROLLVIEW` overlapping
that rectangle has vertical overflow, the fallback is disabled for the whole
page so wheel input remains dedicated to scrolling. Imperative coordinates are
physical pixels, like the control factories.

Combobox choices appear in a separate scrollable rounded popup across a
small gap from the closed control. The popup prefers the space below, flips
above before crossing the monitor work area, and opens and closes with a
short directional fade/lift animation. With `SUI_FEATURE_VSYNC`, popup motion and
scrollbar fading subscribe to the owner host's shared frame source; closing
unsubscribes before destroying the popup. Capacity failure settles the animation
immediately. Opt-out builds retain the timer fallback. By default the popup follows the closed
control's width; `sui_ctrl_combo_set_popup_width` gives it an independent
physical-pixel width, and a non-positive value restores the default. The new
width takes effect the next time the popup opens. While the popup is visible, the closed
control uses its pressed-state fill and a darker neutral stroke; its downward
chevron bounces down on open and up on close without reversing direction. The
popup uses the selected/focus stroke color and a Direct2D-rendered per-pixel
shadow, and a needed scroll indicator remains visible from its first frame. The initial
viewport snaps to whole rows so its top and bottom entries are not clipped.
The selected row stays highlighted until genuine pointer movement hovers a
different row; leaving the list restores the selected-row highlight.

`SuiSliderParams` and `SuiSpinnerParams` are `{ float min, max, value, step }` structs.
Use with the `_p` variants for readable call sites where positional `min/max/value/step`
order would be ambiguous.

Spinner arrows step immediately on press, then repeat after 400 ms at 60 ms
intervals while held over the same arrow. Each changed value fires `on_click`;
the value stays within `min`/`max`, and reaching a bound stops repetition without
duplicate callbacks. Release, leaving the arrow, loss of focus/capture, modal
cancellation, page rebuild, or host release cancels the hold. Inline text editing
and keyboard arrows remain available. `sui_default_msg` routes the repeat timer
and cancellation messages; manual message dispatchers must forward
`SUI_SPINNER_REPEAT_TIMER_ID` to `sui_ctrl_on_timer` and call
`sui_ctrl_spinner_stop_repeat` on `WM_CAPTURECHANGED`, `WM_CANCELMODE`, and
`WM_KILLFOCUS`.

In C (C99 compound literal + designated initializer):
```c
sui_ctrl_add_slider_p(&host, ID_VOLUME, x, y, w, h,
    (SuiSliderParams){ .min=0, .max=100, .value=50, .step=1 });
```

In C++ (C++20 aggregate designated initializer — requires `/std:c++20`):
```cpp
sui_ctrl_add_slider_p(&host, ID_VOLUME, x, y, w, h,
    SuiSliderParams{ .min=0, .max=100, .value=50, .step=1 });
```

### Containers and z-order

`SUI_CTRL_CONTAINER` controls form a parent/child tree via `parent_id`.
Children are clipped to the container rect by default; set
`sui_ctrl_set_container_clip(host, id, 0)` for a pure z-scope (e.g. a
floating panel that should not crop a drop shadow).

`SUI_CTRL_SCROLLVIEW` is a vertically-scrolling container variant.
Children with `parent_id` set to its id are translated by `-scroll_y`
and clipped to its rect; an overlay scrollbar appears on the right edge
when content overflows. Reserve ~12 DIP on the right when laying out
children, place them at absolute coordinates (first child at
`scrollview->y`, growing downward), and call
`sui_ctrl_set_scrollview_content_height` whenever child layout changes.
Mouse wheel and thumb drag are wired automatically.

`z` decides draw order (higher z draws on top). Ties resolve by
insertion order, so newly added controls overlap older siblings.

## Controls — declarative API

Builds the same controls but derives placement and intrinsic min size
from the container stack. Call between `sui_begin` and `sui_end`; the
host computes `intrinsic_min_w / _h` which `sui_handle_wm_getminmaxinfo`
returns to Windows.

```c
sui_begin(host, sui_inset(16.0f), /*gap_dip*/ 8.0f, SUI_CLEAR_PAGE);

  sui_label(host, L"Settings");

  sui_hstack(host, /*n_children*/ 2, /*gap_dip*/ 12.0f);
    sui_vstack(host);
      sui_checkbox(host, ID_CHK_AUTOSTART, L"Launch at login", 1);
      sui_checkbox(host, ID_CHK_NOTIFY,    L"Show notifications", 0);
    sui_end(host);

    sui_vgroup(host, /*gap_dip*/ 6.0f);
      sui_label(host, L"Theme");
      const wchar_t* themes[] = { L"Light", L"Dark", L"System" };
      sui_combobox(host, ID_THEME, themes, 3, 0);
    sui_end(host);
  sui_end(host);

  sui_radio_group(host, /*group_id*/ 1);
    sui_radio(host, ID_R1, L"Standard", 1);
    sui_radio(host, ID_R2, L"Quality",  0);
  sui_end(host);

  sui_flow(host, /*item_h_dip*/ 28.0f, /*gap_dip*/ 6.0f);
    for (int i = 0; i < n_tags; i++)
        sui_button(host, ID_TAG + i, tags[i], 0);
  sui_end(host);

sui_end(host);
```

| Function                                       | Container kind                                              |
|------------------------------------------------|-------------------------------------------------------------|
| `sui_begin(host, inset, gap, clear)`           | Root (only one — call once per build). `inset` is `SuiRect` in DIPs (`sui_inset(f)` for symmetric); `clear` is `SUI_CLEAR_PAGE` (safe page reset) or `SUI_CLEAR_NONE` (append). |
| `sui_begin_px(host, rect, gap, clear)`         | Variant where `rect` and `gap` are already in physical pixels — use when the caller has pre-computed pixel geometry. Returns the root `SuiContainer*`. |
| `sui_vstack(host)`                             | Vertical column, inherits parent gap.                       |
| `sui_vgroup(host, gap)`                        | Vertical column with explicit gap.                          |
| `sui_hstack(host, n, gap)`                     | Equal-share horizontal row of `n` slots.                    |
| `sui_flow(host, item_h, gap)`                  | Wrap-as-needed row of fixed-height items.                   |
| `sui_radio_group(host, group_id)`              | Vertical column; nested `sui_radio` picks up the group.     |
| `sui_scrollview(host, id, w_dip, h_dip, gap)`  | Fixed-size vertical column wrapping `SUI_CTRL_SCROLLVIEW`; content height auto-computed from descendants and committed via `sui_ctrl_set_scrollview_content_height` at `sui_end`. Reserves ~12 DIP right gutter for the overlay scrollbar. |
| `sui_clipbox(host, id, w_dip, h_dip, gap)`     | Fixed-size vertical column wrapping `SUI_CTRL_CONTAINER`; descendants are clipped to the rect, no scrollbar. |
| `sui_end(host)`                                | Pop one container (pops twice for `vstack`-inside-`hstack`). |

`sui_scrollview` and `sui_clipbox` push real controls onto `host->controls[]`
and reparent every descendant atom to that control's id (walked up the
container stack), so nested decl trees inside them draw under the
scrollview clip/transform. Both take an explicit `w_dip` / `h_dip`
because the parent's distribution doesn't currently flow into them —
treat them as fixed-rect containers.

Leaf factories: `sui_button(host, id, text, flags)`, `sui_label`,
`sui_checkbox`, `sui_radio`, `sui_edit`, `sui_combobox`,
`sui_slider` / `sui_slider_p(host, id, SuiSliderParams)`,
`sui_progress`,
`sui_spinner` / `sui_spinner_p(host, id, SuiSpinnerParams)`,
`sui_swatch`, `sui_shortcut_keyboard`, `sui_shortcut_gamepad`,
`sui_textview(host, id, text, capacity, readonly)`,
`sui_image(host, id, w_dip, h_dip, img, circle_mask)`,
`sui_tooltip(host, id, tooltip)`,
`sui_nav_list(host, id, w_dip, h_dip, item_h_dip, selected)`. Button
flags are `SUI_BTN_DEFAULT | SUI_BTN_DISABLED`.

`sui_textview` defaults to `SUI_DEFAULT_TEXTVIEW_H` (≈3 lines) and is
designed to pair with `sui_last_grow(host, 1)` to absorb leftover vstack
height — that's the pattern the imperative API forced consumers to
hand-compute as `h = parent_h - sibling_h_sum - gaps`. `sui_image` and
`sui_nav_list` take explicit dimensions because neither has an
intrinsic size derivable from a text label; pair with the flex helpers
above to grow / shrink them.

### Flex hint helpers

After a leaf factory returns, four post-add helpers mutate the
most-recently-added control's flex fields. They feed the
`HSTACK`/`VSTACK` two-pass distribute at `sui_end`:

```c
sui_hstack(host, 3, /*gap_dip*/ 8.0f);
  sui_button(host, ID_LEFT,   L"Left",   0); sui_last_pref(host, 120.0f, 0.0f);
  sui_button(host, ID_MIDDLE, L"Middle", 0); sui_last_grow(host, 1);
  sui_button(host, ID_RIGHT,  L"Right",  0); sui_last_pref(host, 120.0f, 0.0f);
sui_end(host);
/* Sides stay at 120 DIP; middle absorbs the leftover row width. */
```

| Helper                          | Effect                                                               |
|---------------------------------|----------------------------------------------------------------------|
| `sui_last_grow(host, grow)`     | Set `grow` on the latest control.                                    |
| `sui_last_shrink(host, shrink)` | Override `shrink` (default `1`).                                     |
| `sui_last_pref(host, w, h)`     | Set `pref_w / pref_h` (DIPs, DPI-scaled internally; `0` = unset).    |
| `sui_last_max(host, w, h)`      | Set `max_w / max_h` (DIPs, DPI-scaled internally; `0` = unbounded).  |

To set hints on a control that isn't the latest add, mutate by id:
`int id = sui_button(host, ID, L"OK", 0); host->controls[id].grow = 1;`.
HSTACK and VSTACK record up to `SUI_MAX_FLEX_SLOTS` direct atom
children for distribution; overflow children and non-atom siblings keep
legacy single-pass placement.

The declarative pass also feeds `host.intrinsic_min_w / _h`, which
`sui_handle_wm_getminmaxinfo` honors so the user cannot shrink past
content. **Caveat**: imperative consumers that rebuild on `WM_SIZE`
with W-scaled column positions (e.g. `col_w = 0.5 * content_w`) need
help — the default `max(ex+ew)` row scan tracks W down with the
columns, letting the user shrink past intrinsic content. Three
options, in order of preference:

1. Set `host.min_size_strategy = SUI_MIN_SIZE_FLEX_ROWS`. The
   top-level scan switches to per-Y-slice: for each control's top
   edge, width = `sum(non-anchored ew) + (n-1)*4dp + max(anchored ew
   + right_margin)` over controls active at that Y; max across slices
   wins. Content-derived, no hard-coded number.
2. Set `host.min_client_w / _h` to a per-axis OVERRIDE. When set (> 0),
   that axis uses the pinned value and ignores the scan + bubble; when
   unset (0), the axis falls back to `max(scan, bubble)`. Use when the
   scan can't be trusted on that axis — e.g. width when controls
   reposition with `content_w` and `FLEX_ROWS` still doesn't catch
   everything, or height when a `vstack` grow branch rewrites child
   `c->h / c->y` so the row scan tracks H upward instead of staying at
   content. Pin only the unreliable axis; the other keeps its scan +
   bubble floor so no-overlap is preserved.
3. Skip `sui_handle_wm_getminmaxinfo` entirely. The OS lets the user
   shrink to `GetSystemMetrics(SM_CX/CYMINTRACK)`.

## Undo / Redo

Both editable control types support Ctrl+Z (undo) and Ctrl+Y (redo) out of
the box — no extra wiring required beyond the standard `WM_KEYDOWN` path
(handled by `sui_default_msg` / `sui_ctrl_on_keydown`).

### `SUI_CTRL_EDIT` — multi-level undo

The edit control maintains two explicit stacks: an undo stack and a redo
stack, each up to `SUI_EDIT_UNDO_DEPTH = 8` entries deep. Each entry
stores a complete copy of the text (up to 255 wchar_t) and the cursor
position at that point.

**Snapshot-based workflow**: the host application decides when to push a
snapshot. Call `sui_ctrl_edit_save_undo` before any change that the user
should be able to undo as a discrete step:

```c
SuiControl* c = sui_ctrl_find(&host, ID_EDIT);

/* before an external programmatic change (e.g. auto-fill) */
sui_ctrl_edit_save_undo(c);
wcsncpy(c->text, L"auto-filled text", SUI_MAX_TEXT - 1);
c->text[SUI_MAX_TEXT - 1] = L'\0';
c->d.edit.cursor = (int)wcslen(c->text);
InvalidateRect(host.hwnd, NULL, FALSE);
```

Interactive edits (character insert, delete, Backspace, cut, paste, clear)
call `sui_ctrl_edit_save_undo` internally so normal typing is already
tracked without any extra code.

**Invariant**: `sui_ctrl_edit_save_undo` always clears the redo stack.
Any new edit made after an undo discards the forward history, which is the
standard behavior of every text editor.

When the undo stack is full (`undo_top == SUI_EDIT_UNDO_DEPTH`), the oldest
entry is shifted out to make room for the new one. The redo stack is bounded
by the same depth.

You can also call the helpers directly from code — useful when the keyboard
shortcut path is not available (e.g. in headless tests or from a custom
context menu):

```c
sui_ctrl_edit_undo(c);   /* pop undo stack, push current to redo */
sui_ctrl_edit_redo(c);   /* pop redo stack, push current to undo */
```

Both are no-ops when the corresponding stack is empty or when undo pool
allocation failed at control creation.

### `SUI_CTRL_TEXTVIEW` — single-level undo

The textview control keeps exactly one undo slot and one redo slot. Each
keypress or paste in a non-readonly textview saves the state before the
edit, so Ctrl+Z immediately restores the previous content and Ctrl+Y
re-applies the undone change.

```
user types → save_undo → modify text
Ctrl+Z     → restore saved text (old text moves to redo slot)
Ctrl+Y     → restore redo text (current text moves back to undo slot)
```

Saving a new undo entry (any keypress) clears the redo slot, so Ctrl+Y is
only available immediately after Ctrl+Z with no intervening edits.

Readonly textviews (`sui_ctrl_add_textview(..., readonly=1)`) never save
undo state; Ctrl+Z / Ctrl+Y are silently ignored.

### Constants reference

| Constant               | Value | Meaning                                          |
|------------------------|-------|--------------------------------------------------|
| `SUI_EDIT_UNDO_DEPTH`  | 8     | Max undo (and redo) stack depth for `SUI_CTRL_EDIT`. |

## Accessibility (UIA)

slim-ui exposes a UIA (UI Automation) provider tree so screen readers and
other AT (assistive technology) tools can enumerate controls, query their
properties, and respond to focus/value changes.

Control properties include enabled and offscreen state. A textview exposes
its complete buffer as its accessible name, including readonly content. Checkbox controls expose Toggle, editable fields expose Value, and radio buttons expose SelectionItem. Enter activates the focused button, falling back to the default button when focus is elsewhere.

### Provider architecture

Each `SuiControlHost` has:

- **Host provider** (`SuiUiaHostProviderFull`) — implements
  `IRawElementProviderSimple`, `IRawElementProviderFragment`, and
  `IRawElementProviderFragmentRoot`. Root of the control tree for the window.
- **Per-control providers** (`SuiUiaProvider`) — lazy-created and cached in
  `host->uia_providers[]`. Implement `IRawElementProviderSimple`,
  `IRawElementProviderFragment`, and up to two pattern interfaces
  (`IInvokeProvider`, `IToggleProvider`, `IValueProvider`, etc.) depending on
  control type.

Providers are loaded from `uiautomationcore.dll` at runtime — there is no
static link to `uiautomationcore.lib`. If the DLL is unavailable (e.g. in an
installer's reduced environment) all UIA calls are silent no-ops.

### Wiring in the consumer WndProc

Add `WM_GETOBJECT` dispatch to every window that owns a `SuiControlHost`:

```c
case WM_GETOBJECT:
    return sui_uia_handle_getobject(&host, wParam, lParam);
```

`sui_uia_handle_getobject` returns `UiaReturnRawElementProvider(...)` for
`lParam == UiaRootObjectId` (`-25`) and `0` for all other object IDs (letting the
default procedure handle MSAA compat).

### Navigation and runtime IDs

Each per-control Fragment's `Navigate` returns:

| Direction      | Returns                                   |
|---------------|-------------------------------------------|
| Parent         | Host Fragment (`&host_provider->lpFragVtbl`) |
| PreviousSibling | Provider for `control_index - 1`         |
| NextSibling    | Provider for `control_index + 1`         |
| First/LastChild | NULL (controls are leaf nodes)           |

`GetRuntimeId` returns a 2-element `VT_I4` SAFEARRAY
`[UiaAppendRuntimeId (3), control.id]`. UIA prepends the HWND-based
segment so the full runtime ID is stable across host rebuilds as long as
`control.id` stays the same.

### Provider lifecycle and cache invalidation

- `sui_ctrl_clear_page` — disconnects and releases providers for discarded
  (non-persistent) slots before trimming `control_count`. Stale pointers
  never reach AT tools.
- `sui_ctrl_host_release` — disconnects and releases all providers (host +
  per-control) before freeing the host. Calls `UiaDisconnectProvider` via
  dynamic lookup; silently skips if the DLL isn't loaded.

### Property-change events

Focus changes fire `UiaRaiseAutomationPropertyChangedEvent` with
`UIA_HasKeyboardFocusPropertyId` (30008) on both the losing and gaining
providers whenever `sui_ctrl_set_focus` moves focus, plus an automation focus-change event for the new control. Checkbox and radio changes publish their checked or selected state; selecting a radio also raises an element-selected event. Value-change events
for edit, spinner, and slider are planned for a later phase.

## Layout helpers

`SuiLayout` is an alternative to the declarative API for windows that
want imperative control over column/row stepping without managing pixel
offsets by hand.

```c
SuiLayout col = sui_layout_col(x, y, w, h, gap_dip);
float lbl_y = sui_layout_next(&col, label_h);
float btn_y = sui_layout_next(&col, button_h);
float fill  = sui_layout_fill(&col);   /* remaining space */
```

`sui_layout_row` is the horizontal equivalent.
`sui_layout_fit_text_width(renderer, text, padding)` returns a width
that fits the text plus padding via `sui_renderer_measure_text_width`.

## Vsync animation pump

When `SUI_FEATURE_VSYNC` is defined, each host keeps a `SuiVsyncState`
subscription to one shared frame source per UI thread. `sui_vsync_init`
prepares the source, including its hidden notification window and worker. Instances
in different translation units discover the same source. Up to 64 prepared
hosts and eight callbacks per host are supported; attach returns zero on
exhaustion or platform initialization failure.

```c
if (!sui_vsync_attach(&host.vsync, my_tick, my_user)) { /* settle without animation */ }
sui_vsync_detach(&host.vsync, my_tick, my_user); /* no worker join */
```

The source uses an interruptible high-resolution waitable timer (with an older
Windows fallback), aligned to available DWM timing. It schedules at the fastest
active subscriber display's refresh rate and reconfigures on monitor moves and
display changes. A host's display rate is read once per display and again after a
display or settings change, so attaching and detaching around idle reads nothing (the
system's mode query allocates). Timing information is advisory; this
is not a presentation-completion fence or a guarantee of variable-refresh pacing.
It blocks without periodic wakeups when no subscriber is active. Hidden/embedded
hosts can call `sui_vsync_set_monitor(&vs, monitor)` when their presentation target
changes; passing `NULL` restores automatic HWND-based display selection.

Only one source notification and one notification per subscribed state may be
pending. All states receive the same QPC frame timestamp. Call
`sui_ctrl_on_vsync(&host)` in `WM_SUI_VSYNC`; it dispatches the host callbacks
and removes completed control animations. For a standalone state, call
`sui_vsync_dispatch(&vs)`. Dispatch every state attached to that HWND. Repeated
dispatch of the same timestamp is harmless. `dt_ms` retains sub-millisecond
precision and actual elapsed time, including stalls.

Callbacks may attach/detach themselves or other callbacks. Newly attached
callbacks start on the next dispatch. Nested dispatch is ignored. The state
being dispatched must remain alive until dispatch returns; defer host destruction.
All lifecycle operations and callbacks belong to the HWND owner thread.

Call `sui_vsync_stop(&vs)` before freeing the state (`sui_ctrl_host_release`
does this). The last prepared host stops and releases the shared source. Its
worker waits on interruptible kernel objects, never on the UI thread or
`DwmFlush`. Per-subscriber detach does not destroy or recreate clock resources.

## Theming (color schemes)

```c
host.scheme = &sui_scheme_dark;            /* swap on the host */
sui_scheme_register("blue-dark", &mine);   /* register a custom scheme */
const SuiColorScheme* s = sui_scheme_find("blue-dark");
int n = sui_scheme_list(names, SUI_MAX_SCHEMES);
sui_scheme_unregister("blue-dark");
```

The first `sui_scheme_*` call seeds the registry with `"light"` and
`"dark"` (`sui__scheme_registry_init`). `SUI_MAX_SCHEMES = 16`.

To pick suitable accent tints from your app code, the project default is
**blue tint for light mode, orange tint for dark mode**.

## Typography

```c
sui_typography_register("serif", &my_typo);
const SuiTypography* t = sui_typography_find("serif");
sui_renderer_apply_typography(&renderer, t);   /* rebuilds DWrite formats */
```

`SuiTypography` covers four roles (body, subtitle, title, icon). The
built-in `"default"` is `sui_typography_default` (Segoe UI / Segoe Fluent
Icons). `SUI_MAX_TYPOGRAPHIES = 16`.

## Full WndProc wiring

Minimal skeleton using `sui_default_msg` — copy and adapt. See
`demo/main.cpp` for the complete file with declarative builds,
DPI handling, and edit-state preservation across `WM_SIZE`.

```c
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    AppState* a = (AppState*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    LRESULT r = 0;

    /* sui_default_msg handles: WM_ERASEBKGND, WM_MOUSE*, WM_LBUTTON*,
     * WM_CONTEXTMENU, WM_MOUSEWHEEL, WM_SETFOCUS/KILLFOCUS (caret timer),
     * WM_TIMER, WM_KEY*, WM_CHAR, WM_GETOBJECT.                          */
    if (a && sui_default_msg(&a->host, hwnd, msg, wp, lp, &r)) return r;

    switch (msg) {
    case WM_CREATE: { /* renderer_init, host_init, on_click wiring, BuildControls */ break; }
    case WM_PAINT: {
        PAINTSTRUCT ps; BeginPaint(hwnd, &ps);
        sui_renderer_begin(&a->renderer);
        sui_renderer_clear(&a->renderer, a->scheme.background);
        Build(a);                              /* declarative tree (or imperative on WM_CREATE only) */
        sui_ctrl_draw_all(&a->host);
        sui_renderer_end(&a->renderer);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_SIZE: {
        RECT cr; GetClientRect(hwnd, &cr);
        sui_renderer_resize(&a->renderer, cr.right, cr.bottom);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    case WM_DPICHANGED: {
        a->dpi = HIWORD(wp); a->host.dpi = a->dpi;
        const RECT* nr = (const RECT*)lp;
        SetWindowPos(hwnd, NULL, nr->left, nr->top,
                     nr->right - nr->left, nr->bottom - nr->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    case WM_GETMINMAXINFO: sui_handle_wm_getminmaxinfo(&a->host, hwnd, lp); return 0;

    case WM_SUI_VSYNC:
        sui_ctrl_on_vsync(&a->host);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_DESTROY:
        sui_ctrl_host_release(&a->host);
        sui_renderer_release(&a->renderer);
        PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
```

## Constants reference

| Constant                       | Value           | Meaning                                                  |
|--------------------------------|-----------------|----------------------------------------------------------|
| `SUI_MAX_CONTROLS`             | 64              | Per-host control array size. To change it, define it before including `slim_ui.h`, the same way in every file that includes it. Each control costs about 1.6 KB. |
| `SUI_MAX_ITEMS`                | 32              | Per-combobox / per-nav-list item count.                  |
| `SUI_MAX_CONTAINER_DEPTH`      | 8               | Declarative container stack depth.                       |
| `SUI_MAX_FLEX_SLOTS`           | 16              | Per-container atom-child slots tracked for two-pass distribute. Overflow children fall back to legacy placement. |
| `SUI_EDIT_UNDO_DEPTH`          | 8               | Undo and redo stack depth for `SUI_CTRL_EDIT`.           |
| `SUI_MAX_SCHEMES`              | 16              | Color-scheme registry slots.                             |
| `SUI_MAX_TYPOGRAPHIES`         | 16              | Typography registry slots.                               |
| `SUI_SCHEME_NAME_MAX`          | 32              | Max bytes for a registered scheme/typography name.       |
| `SUI_VSYNC_MAX_TICKERS`        | 8               | Per-vsync subscriber capacity.                           |
| `SUI_TITLE_PT / SUBTITLE_PT / CONTROL_PT` | 22 / 11 / 11 | Default font sizes.                              |
| `SUI_CARET_TIMER_ID`           | `0x5549`        | Edit caret blink timer.                                  |
| `SUI_HOVER_TIMER_ID`           | `0x5552`        | Hover-anim timer (used when `SUI_FEATURE_VSYNC` is off). |
| `SUI_TOOLTIP_TIMER_ID`         | `0x5553`        | Tooltip timer; independent of shortcut recording. |
| `SUI_SHORTCUT_RECORD_TIMER_ID` | `0x5554`        | Gamepad recorder polling; dispatched separately from tooltips. |
| `WM_SUI_VSYNC`                 | `WM_APP + 100`  | Vsync wake-up message.                                   |
| `SUI_BTN_DEFAULT`              | bit flag        | `sui_button` flag — accent border, Enter activates.       |
| `SUI_BTN_DISABLED`             | bit flag        | `sui_button` flag — greyed out + no input.                |
| `SUI_IMG_FILL`                 | 0               | `SuiImageScale` — stretch to fill, ignore aspect (default). |
| `SUI_IMG_FIT`                  | 1               | `SuiImageScale` — letterbox, preserve aspect.             |
| `SUI_IMG_CLIP`                 | 2               | `SuiImageScale` — cover + crop overflow, preserve aspect. |
| `SUI_CTRL_ANCHOR_RIGHT`        | 256             | Pinned to right edge — `sui_ctrl_min_size` uses `c.w + right_margin` instead of `c.x + c.w`. |
| `SUI_CTRL_ANCHOR_BOTTOM`       | 512             | Pinned to bottom edge — `sui_ctrl_min_size` uses `c.h + bottom_margin` instead of `c.y + c.h`. |
| `SUI_CTRL_FLOW`                | 4096            | Container flag — `sui_ctrl_min_size` reports row width as `max(child.effective_w)` instead of summing children across a Y band. Use on flow/wrap containers whose child positions encode the *current* width's column count. |

## Repository layout

```
slim-ui/
├── README.md                          ← introduction
├── AGENTS.md                          ← instructions for AI coding agents
├── LICENSE                            ← MIT
├── logo.svg, logo.png                 ← the logo; the demo embeds the PNG
├── docs/                              ← this file and the README's screenshots
├── CMakeLists.txt                     ← the `slim_ui` interface target, the demo and the tests
├── demo/main.cpp                      ← reference app, exercises every control
├── demo/shader_effect.cpp, shader_*.hlsl ← the demo's pixel shaders
├── examples/tiny.c                    ← the smallest app
├── dotnet/                            ← C#/.NET wrapper, native bridge, AOT-capable sample
├── tests/                             ← regression suite (`tests/build-tests.ps1`)
└── include/slim_ui/
    ├── slim_ui.h                      ← umbrella header + scheme/typo registry
    ├── sui_renderer.h                 ← D2D + DWrite primitives
    ├── sui_corner.h                   ← continuous rounded corner paths
    ├── sui_controls.h                 ← host init, dispatch, draw_all
    ├── sui_decl.h                     ← declarative API + flex (opt-in: SUI_FEATURE_DECL)
    ├── sui_layout.h                   ← SuiLayout col/row helpers
    ├── sui_vsync.h                    ← vsync pump (opt-in)
    ├── sui_d2d.h                      ← D2D / DWrite C-mode IID glue
    ├── sui_uia.h / sui_uia_providers.h← UI Automation providers
    ├── sui_ctrl_omit.h                ← stand-ins for controls left out with SUI_NO_*
    └── sui_ctrl_*.h                   ← per-control add/draw/event handlers
```

## License

Slim UI is released under the [MIT License](../LICENSE). `tests/utest.h` is
sheredom's utest.h (Unlicense).

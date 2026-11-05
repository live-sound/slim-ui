# Slim UI .NET wrapper

This directory contains the C#/.NET wrapper for Slim UI. It lets C#
applications create native Slim UI windows without writing Win32 message
plumbing, while still using the same controls, renderer, DPI behavior,
theme registry, typography registry, and rebuild model as the C toolkit.

The wrapper is intentionally thin:

- `native/` builds `SlimUi.Native.dll`, a small exported C ABI over the
  header-only Slim UI source.
- `src/SlimUi/` contains the managed wrapper targeting `net8.0-windows`.
- `samples/SlimUi.Sample/` contains the C# demo. It should replicate the
  native `../demo/main.cpp` gallery, including its Shader page. The sample's
  native bridge reuses `demo/shader_effect.cpp`; the general wrapper does not
  include these demo effects.

## Requirements

- Windows.
- .NET SDK 8 or newer.
- CMake.
- MSVC C++ build tools and a Windows SDK.

The managed wrapper supports Native AOT, but the app still ships with
`SlimUi.Native.dll`. The result is AOT-compatible, not one fully static
single-file executable.

## Build

Run from this directory:

```powershell
.\build.ps1
```

This builds:

- `native/build/Release/SlimUi.Native.dll`
- `native/build/Release/SlimUi.Sample.Native.dll` (the gallery's bridge)
- `src/SlimUi/bin/Release/net8.0-windows/SlimUi.dll`
- `samples/SlimUi.Sample/bin/Release/net8.0-windows/SlimUi.Sample.exe`

Run the demo:

```powershell
dotnet run --project .\samples\SlimUi.Sample\SlimUi.Sample.csproj -c Release
```

Publish the demo with Native AOT:

```powershell
dotnet publish .\samples\SlimUi.Sample\SlimUi.Sample.csproj -c Release -p:PublishAot=true
```

The AOT executable is emitted under:

```text
samples\SlimUi.Sample\bin\Release\net8.0-windows\win-x64\publish\SlimUi.Sample.exe
```

That publish directory must also contain `SlimUi.Native.dll`.
For the gallery, the build copies `SlimUi.Sample.Native.dll` under that name,
alongside `logo.png`. Other applications use the regular `SlimUi.Native.dll`.

Run the native bridge and managed startup regression checks with `./test.ps1`.

## Runtime model

`SlimWindow` owns one native HWND through `SlimUi.Native.dll`. The native
bridge owns:

- Win32 class registration and window creation.
- Direct2D / DirectWrite renderer initialization.
- `SuiControlHost` initialization and release.
- DPI changes.
- resize rebuilds.
- `sui_default_msg` message routing.
- UIA forwarding through the Slim UI host.
- shared frame-clock dispatch (one prepared native source per UI thread; idle animations unsubscribe).
- state preservation across tree rebuilds.

Window width and height are outer-window sizes in DIPs, scaled for the initial
display just like the native demo. The first build callback already has a valid
`NativeHandle`, `ClientWidth`, and `ClientHeight`.

The C# build callback runs whenever the native side rebuilds the control
tree, including window creation, resize, DPI changes, and explicit
`RequestRebuild()` calls. Treat the callback as declarative: create the
current UI tree from state every time.

## Basic API

The simple path uses the declarative layout helpers exposed through
`SlimUiBuilder`:

```csharp
SlimApplication.Run("Settings", 900, 640, ui =>
{
    ui.Root(padding: 18, gap: 10, () =>
    {
        ui.Title("Settings");

        SlimControl name = ui.TextBox(
            placeholder: "Display name",
            key: "name");

        SlimControl launch = ui.Checkbox(
            "Launch at login",
            isChecked: true,
            key: "launch");

        ui.Button("Save", isDefault: true, onClick: () =>
        {
            Console.WriteLine($"{name.Text}: {launch.Checked}");
        });
    });
});
```

Use `key:` for controls whose generated ID must stay stable across
rebuilds. For manual demo parity, use explicit integer IDs.

## Containers

Declarative containers map to the Slim UI declarative API:

```csharp
ui.Root(padding: 16, gap: 8, () =>
{
    ui.Row(columns: 2, gap: 12, () =>
    {
        ui.Group(gap: 8, () =>
        {
            ui.Label("Profile");
            ui.TextBox(key: "name");
        });

        ui.Group(gap: 8, () =>
        {
            ui.Label("Options");
            ui.Checkbox("Enable sync", key: "sync");
        });
    });
});
```

Available container helpers:

| Method | Purpose |
| --- | --- |
| `Root(...)` | Clears the page and pushes the root container. |
| `Stack(...)` | Pushes a vertical stack. |
| `Group(gap, ...)` | Pushes a tighter vertical group. |
| `Row(columns, gap, ...)` | Pushes an HSTACK with fixed column count. |
| `Flow(itemHeight, gap, ...)` | Pushes a wrapping flow container. |
| `RadioGroup(key, ...)` | Pushes a radio group container. |
| `ScrollView(width, height, gap, ...)` | Pushes a scrollview control/container. |
| `ClipBox(width, height, gap, ...)` | Pushes a clipped container without scrollbar. |

Flex hints are exposed on `SlimControl`:

```csharp
ui.Row(3, 8, () =>
{
    ui.Button("Left").Preferred(120);
    ui.Button("Middle").Grow();
    ui.Button("Right").Preferred(120);
});
```

Supported hints:

- `Grow(int grow = 1)`
- `Shrink(int shrink)`
- `Preferred(float width, float height = 0)`
- `Max(float width, float height = 0)`

## Controls

Current managed control coverage:

| C# method | Native control |
| --- | --- |
| `Label`, `LabelAt` | `SUI_CTRL_LABEL` |
| `Title`, `TitleAt` | bold/title label |
| `Button`, `ButtonAt` | `SUI_CTRL_BUTTON` |
| `Checkbox`, `CheckboxAt` | `SUI_CTRL_CHECKBOX` |
| `Radio`, `RadioAt` | `SUI_CTRL_RADIO` |
| `TextBox`, `TextBoxAt` | `SUI_CTRL_EDIT` |
| `KeyboardShortcutRecorderAt` | `SUI_CTRL_SHORTCUT_RECORDER` |
| `GamepadShortcutRecorderAt` | `SUI_CTRL_SHORTCUT_RECORDER` |
| `ComboBox`, `ComboBoxAt` | `SUI_CTRL_COMBOBOX` |
| `Slider`, `SliderAt` | `SUI_CTRL_SLIDER` |
| `Progress`, `ProgressAt` | `SUI_CTRL_PROGRESS` |
| `Spinner`, `SpinnerAt` | `SUI_CTRL_SPINNER` |
| `TextView`, `TextViewAt` | `SUI_CTRL_TEXTVIEW` |
| `SwatchAt` | `SUI_CTRL_COLOR_SWATCH` |
| `ImageAt` | `SUI_CTRL_IMAGE` |
| `NavList`, `NavListAt` | `SUI_CTRL_NAV_LIST` |
| `ContainerAt` | `SUI_CTRL_CONTAINER` |
| `ScrollViewAt` | `SUI_CTRL_SCROLLVIEW` |

Spinner arrows repeat after a 400 ms hold, then every 60 ms. Each changed
value raises `Change`/`onChanged`; release or cancellation stops repetition.
Rebuilding preserves the value and cancels the active hold.

`SlimControl` exposes:

- `Checked`
- `SelectedIndex`
- `Number`
- `Text`
- `SetProgress(value)`
- `PopupWidth(width)` for an independent combobox-popup width in DIPs
- flex hint helpers listed above

## Manual canvas API

The C# wrapper also exposes an explicit-placement surface so the .NET demo
can mirror the native SUI demo. Use this for visual testbeds and precise
ports, not as the default app style.

Manual methods use DIPs in C#. The native bridge converts them to physical
pixels before calling the C imperative `sui_ctrl_add_*` APIs. This differs
from raw Slim UI C, where imperative coordinates are already physical
pixels.

Useful manual helpers:

```csharp
ui.ClearCanvas();

float w = ui.ClientWidth;
float h = ui.ClientHeight;

ui.LabelAt(20, 20, 300, 18, "Label");
ui.ButtonAt(101, 20, 48, 110, 28, "Click me");
ui.ProgressAt(201, 20, 90, 360, 22, 0, 100, 40);

ui.SetMinSize(101, width: 110);
ui.SetAnchor(201, SlimAnchor.Right | SlimAnchor.Bottom, 10, 24);
```

Manual helpers include:

- client metrics: `ClientWidth`, `ClientHeight`, `DpiScale`
- tree control: `ClearCanvas`, `PushRootRect`, `PopContainer`
- min-size: `SetMinClientSize`, `SetMinSize`, `LastMinSize`
- hierarchy: `SetParent`, `LastParent`, `SetFlow`
- anchoring: `SetAnchor`
- scrollview sizing: `SetScrollViewContentHeight`
- optional nav fallback: `SetNavContentWheelRegion` changes adjacent nav items
  on wheel input only while the configured page has no overflowing Slim UI
  vertical control

## Images

Load images through the window, then pass the handle to `ImageAt`:

```csharp
SlimImage logo = ui.Window.LoadImage(path);
ui.ImageAt(20, 20, 56, 56, logo);
```

Image loading happens in the native bridge through Slim UI's WIC-backed
`sui_renderer_load_image`. Loaded native image handles are released with
the native renderer.

## Themes and typography

The wrapper exposes Slim UI scheme and typography switching:

```csharp
ui.Window.SetScheme("dark");
ui.Window.SetTypography("default");
```

The native bridge registers the same demo extras as the C demo:

- scheme: `"blue-dark"`
- typography: `"serif"`

The demo uses combo boxes to switch between the registered demo options.

## State preservation

Slim UI rebuilds its control tree during resize and DPI changes. The
bridge captures and reapplies persistent state by control ID.

Currently preserved:

- edit text and cursor
- textview text and cursor
- checkbox and radio checked state
- combobox and nav selected index
- slider value
- spinner value
- progress value
- shortcut recorder value
- color swatch color

If a new native control stores user-modifiable state in `SuiControl::d`,
add it to both `SuiNetCaptureControl` and `SuiNetApplyState` in
`native/src/slim_ui_net.cpp`.

## Native AOT notes

The wrapper is AOT-friendly because it uses explicit P/Invoke calls and
does not depend on reflection-based discovery.

Use:

```powershell
dotnet publish .\samples\SlimUi.Sample\SlimUi.Sample.csproj -c Release -p:PublishAot=true
```

The published app still needs `SlimUi.Native.dll` next to the executable.
If the executable starts and immediately exits, check that the DLL was
copied into the publish directory and matches the current native build.

## Keeping the wrapper in sync

The .NET wrapper is part of Slim UI's public surface inside this repo.
Keep it aligned with the C source:

- Changes to public controls should update `NativeMethods.cs`,
  `SlimUiBuilder.cs`, and the C ABI in `native/src/slim_ui_net.cpp`.
- Changes to runtime state should update bridge preservation logic.
- Changes to demo-covered behavior should update
  `samples/SlimUi.Sample/Program.cs` so the .NET demo continues to
  replicate `../demo/main.cpp`.
- Changes to theme, typography, image, DPI, lifecycle, event, or layout
  contracts should be reflected here when the wrapper exposes or depends
  on them.

For new controls, follow this order:

1. Add or update the C Slim UI control.
2. Add the C ABI function in `native/src/slim_ui_net.cpp`.
3. Add the P/Invoke declaration in `src/SlimUi/NativeMethods.cs`.
4. Add a comfortable managed wrapper method in `SlimUiBuilder`.
5. Preserve state if the control has user-modifiable state.
6. Add the control to the native demo and the .NET demo.
7. Run `.\build.ps1` and, when relevant, the Native AOT publish command.

## Troubleshooting

`DllNotFoundException: SlimUi.Native`

- Run `.\build.ps1` first.
- Check that `SlimUi.Native.dll` exists next to the managed executable.
- For AOT publish, check the `publish\` directory.

Native AOT fails with WinForms/trimming errors

- The Slim UI sample should not use WinForms.
- Ensure `UseWindowsForms` is not enabled in the sample project.

State resets after resizing

- Use stable `key:` values for declarative controls or explicit IDs for
  manual controls.
- If the control is new, add capture/apply logic in the native bridge.

Layout differs from the native demo

- Compare against `../demo/main.cpp`.
- The C demo's imperative coordinates are physical pixels. The .NET
  manual APIs take DIPs and scale internally.

Combobox opening/closing and scrollbar fades share the owner window's frame source. Popup teardown removes its subscription before releasing native resources.

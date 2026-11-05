using SlimUi;

int builds = 0;
float firstWidth = 0;
float firstHeight = 0;
IntPtr firstHandle = IntPtr.Zero;
SlimControl edit = default;
using var window = new SlimWindow("Slim UI managed regression", 960, 720, ui =>
{
    if (builds++ == 0)
    {
        firstHandle = ui.Window.NativeHandle;
        firstWidth = ui.ClientWidth;
        firstHeight = ui.ClientHeight;
    }
    ui.ClearCanvas();
    edit = ui.TextBoxAt(1, 20, 20, 200, 28, "First build works");
});

if (firstHandle == IntPtr.Zero || firstHandle != window.NativeHandle)
    throw new Exception("The first build must have the live native handle.");
if (firstWidth < 900 || firstWidth > 960 || firstHeight < 640 || firstHeight > 720)
    throw new Exception($"Expected a 960 x 720 DIP window; first client size was {firstWidth} x {firstHeight}.");
if (firstWidth != window.ClientWidth || firstHeight != window.ClientHeight)
    throw new Exception("The first build must use the initial client size.");
if (edit.Text != "First build works")
    throw new Exception("Controls from the first build were not created.");

window.RequestRebuild();
if (builds < 2 || edit.Text != "First build works")
    throw new Exception("Explicit rebuild must still work after initialization.");
Console.WriteLine($"PASS: first build, initial size ({firstWidth} x {firstHeight} DIPs, scale {window.DpiScale}), and rebuild.");

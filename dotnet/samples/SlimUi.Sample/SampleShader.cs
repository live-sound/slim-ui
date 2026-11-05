using System.Runtime.InteropServices;
using SlimUi;

// Only the gallery's native DLL contains this entry point.
internal static class SampleShader
{
    [DllImport("SlimUi.Native", CallingConvention = CallingConvention.StdCall)]
    private static extern void sui_sample_shader_configure(
        IntPtr window, int imageId, int editId, int active, int paused, float speed);

    internal static void Configure(SlimWindow window, int imageId, int editId, bool active, bool paused, float speed)
    {
        sui_sample_shader_configure(window.NativeHandle, imageId, editId, active ? 1 : 0, paused ? 1 : 0, speed);
    }
}

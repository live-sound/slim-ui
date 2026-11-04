namespace SlimUi;

public static class SlimApplication
{
    public static int Run(string title, int width, int height, Action<SlimUiBuilder> build)
    {
        using var window = new SlimWindow(title, width, height, build);
        return Run(window);
    }

    public static int Run(SlimWindow window)
    {
        ArgumentNullException.ThrowIfNull(window);
        window.Show();
        return NativeMethods.sui_net_app_run();
    }
}

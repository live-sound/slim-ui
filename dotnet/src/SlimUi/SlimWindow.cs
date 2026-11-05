using System.Runtime.InteropServices;
using System.Text;

namespace SlimUi;

public sealed class SlimWindow : IDisposable
{
    private readonly Action<SlimUiBuilder> _build;
    private readonly NativeMethods.BuildCallback _buildCallback;
    private readonly NativeMethods.EventCallback _eventCallback;
    private readonly Dictionary<int, Action> _clickHandlers = new();
    private readonly Dictionary<int, Action> _changeHandlers = new();
    private readonly Dictionary<string, int> _keyedIds = new(StringComparer.Ordinal);
    private readonly GCHandle _selfHandle;
    private IntPtr _native;
    private int _nextAutoId = 1000;
    private int _nextKeyedId = 100000;
    private bool _disposed;

    public SlimWindow(string title, int width, int height, Action<SlimUiBuilder> build)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(title);
        _build = build ?? throw new ArgumentNullException(nameof(build));
        _buildCallback = HandleBuild;
        _eventCallback = HandleEvent;
        _selfHandle = GCHandle.Alloc(this);

        _native = NativeMethods.sui_net_window_create(
            title,
            width,
            height,
            _buildCallback,
            _eventCallback,
            GCHandle.ToIntPtr(_selfHandle));

        if (_native == IntPtr.Zero)
        {
            _selfHandle.Free();
            throw new InvalidOperationException("Slim UI native window creation failed.");
        }
    }

    public IntPtr NativeHandle => _native;

    public float ClientWidth
    {
        get
        {
            ThrowIfDisposed();
            return NativeMethods.sui_net_client_width(_native);
        }
    }

    public float ClientHeight
    {
        get
        {
            ThrowIfDisposed();
            return NativeMethods.sui_net_client_height(_native);
        }
    }

    public float DpiScale
    {
        get
        {
            ThrowIfDisposed();
            return NativeMethods.sui_net_dpi_scale(_native);
        }
    }

    public void Show()
    {
        ThrowIfDisposed();
        NativeMethods.sui_net_window_show(_native);
    }

    public void Close()
    {
        if (!_disposed && _native != IntPtr.Zero)
        {
            NativeMethods.sui_net_window_close(_native);
        }
    }

    public void RequestRebuild()
    {
        ThrowIfDisposed();
        NativeMethods.sui_net_window_rebuild(_native);
    }

    public bool SetScheme(string name)
    {
        ThrowIfDisposed();
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        return NativeMethods.sui_net_set_scheme(_native, name) != 0;
    }

    public bool SetTypography(string name)
    {
        ThrowIfDisposed();
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        return NativeMethods.sui_net_set_typography(_native, name) != 0;
    }

    public SlimImage LoadImage(string path)
    {
        ThrowIfDisposed();
        ArgumentException.ThrowIfNullOrWhiteSpace(path);
        int id = NativeMethods.sui_net_load_image(_native, path);
        if (id == 0) return default;
        return new SlimImage(id, NativeMethods.sui_net_image_width(_native, id), NativeMethods.sui_net_image_height(_native, id));
    }

    internal int NextId(string? key)
    {
        ThrowIfDisposed();
        if (string.IsNullOrWhiteSpace(key))
        {
            return _nextAutoId++;
        }

        if (_keyedIds.TryGetValue(key, out int id))
        {
            return id;
        }

        id = _nextKeyedId++;
        _keyedIds.Add(key, id);
        return id;
    }

    internal void RegisterClick(int id, Action? handler)
    {
        if (handler is not null) _clickHandlers[id] = handler;
    }

    internal void RegisterChange(int id, Action? handler)
    {
        if (handler is not null) _changeHandlers[id] = handler;
    }

    internal bool GetChecked(int id)
    {
        ThrowIfDisposed();
        return NativeMethods.sui_net_get_checked(_native, id) != 0;
    }

    internal int GetSelected(int id)
    {
        ThrowIfDisposed();
        return NativeMethods.sui_net_get_selected(_native, id);
    }

    internal float GetNumber(int id)
    {
        ThrowIfDisposed();
        return NativeMethods.sui_net_get_number(_native, id);
    }

    internal string GetText(int id)
    {
        ThrowIfDisposed();
        var buffer = new StringBuilder(4096);
        NativeMethods.sui_net_get_text(_native, id, buffer, buffer.Capacity);
        return buffer.ToString();
    }

    internal void SetProgress(int id, float value)
    {
        ThrowIfDisposed();
        NativeMethods.sui_net_set_progress(_native, id, value);
    }

    internal void SetComboBoxPopupWidth(int id, float width)
    {
        ThrowIfDisposed();
        if (id < 0) return;
        NativeMethods.sui_net_set_combobox_popup_width(_native, id, width);
    }

    internal void SetTooltip(int id, string? text)
    {
        ThrowIfDisposed();
        if (id < 0) return;
        NativeMethods.sui_net_set_tooltip(_native, id, text);
    }

    internal void SetTextStyle(int id, float fontSize, int semibold)
    {
        ThrowIfDisposed();
        if (id < 0) return;
        NativeMethods.sui_net_set_text_style(_native, id, fontSize, semibold);
    }

    internal void SetStyleColor(int id, SlimStyleColor property, uint color)
    {
        ThrowIfDisposed();
        if (id < 0) return;
        NativeMethods.sui_net_set_style_color(_native, id, (uint)property, color);
    }

    internal void SetFontFamily(int id, string? family)
    {
        ThrowIfDisposed();
        if (id < 0) return;
        NativeMethods.sui_net_set_font_family(_native, id, family);
    }

    internal void LastGrow(int grow) => NativeMethods.sui_net_last_grow(_native, grow);

    internal void LastShrink(int shrink) => NativeMethods.sui_net_last_shrink(_native, shrink);

    internal void LastPreferred(float width, float height) => NativeMethods.sui_net_last_pref(_native, width, height);

    internal void LastMax(float width, float height) => NativeMethods.sui_net_last_max(_native, width, height);

    internal void LastStyleColor(SlimStyleColor property, uint color) => NativeMethods.sui_net_last_style_color(_native, (uint)property, color);

    internal void LastFontFamily(string? family) => NativeMethods.sui_net_last_font_family(_native, family);

    public void Dispose()
    {
        if (_disposed) return;
        _disposed = true;

        if (_native != IntPtr.Zero)
        {
            NativeMethods.sui_net_window_destroy(_native);
            _native = IntPtr.Zero;
        }

        if (_selfHandle.IsAllocated)
        {
            _selfHandle.Free();
        }
    }

    private void HandleBuild(IntPtr window, IntPtr userData)
    {
        if (_disposed) return;

        // Creation can invoke the first build before the P/Invoke call returns.
        _native = window;

        _nextAutoId = 1000;
        _clickHandlers.Clear();
        _changeHandlers.Clear();

        _build(new SlimUiBuilder(this));
    }

    private void HandleEvent(IntPtr window, int controlId, int eventKind, IntPtr userData)
    {
        if (_disposed) return;

        switch ((SlimEventKind)eventKind)
        {
            case SlimEventKind.Click:
                if (_clickHandlers.TryGetValue(controlId, out Action? click)) click();
                break;
            case SlimEventKind.Change:
                if (_changeHandlers.TryGetValue(controlId, out Action? change)) change();
                break;
            case SlimEventKind.Close:
                break;
        }
    }

    private void ThrowIfDisposed()
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
    }
}

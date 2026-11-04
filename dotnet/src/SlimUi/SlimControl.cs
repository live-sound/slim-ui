namespace SlimUi;

public readonly struct SlimControl
{
    private readonly SlimWindow? _window;

    internal SlimControl(SlimWindow window, int id)
    {
        _window = window;
        Id = id;
    }

    public int Id { get; }

    public bool Checked => RequireWindow().GetChecked(Id);

    public int SelectedIndex => RequireWindow().GetSelected(Id);

    public float Number => RequireWindow().GetNumber(Id);

    public string Text => RequireWindow().GetText(Id);

    public SlimControl Grow(int grow = 1)
    {
        RequireWindow().LastGrow(grow);
        return this;
    }

    public SlimControl Shrink(int shrink)
    {
        RequireWindow().LastShrink(shrink);
        return this;
    }

    public SlimControl Preferred(float width, float height = 0)
    {
        RequireWindow().LastPreferred(width, height);
        return this;
    }

    public SlimControl Max(float width, float height = 0)
    {
        RequireWindow().LastMax(width, height);
        return this;
    }

    public SlimControl Tooltip(string? text)
    {
        RequireWindow().SetTooltip(Id, text);
        return this;
    }

    public SlimControl TextStyle(float fontSize = 0, bool? semibold = null)
    {
        RequireWindow().SetTextStyle(Id, fontSize, semibold.HasValue ? (semibold.Value ? 1 : 0) : -1);
        return this;
    }

    public SlimControl FontSize(float fontSize)
    {
        RequireWindow().SetTextStyle(Id, fontSize, -1);
        return this;
    }

    public SlimControl Semibold(bool semibold = true)
    {
        RequireWindow().SetTextStyle(Id, 0, semibold ? 1 : 0);
        return this;
    }

    public SlimControl FontFamily(string? family)
    {
        RequireWindow().SetFontFamily(Id, family);
        return this;
    }

    public SlimControl StyleColor(SlimStyleColor property, uint color)
    {
        RequireWindow().SetStyleColor(Id, property, color);
        return this;
    }

    public void SetProgress(float value)
    {
        RequireWindow().SetProgress(Id, value);
    }

    public SlimControl PopupWidth(float width)
    {
        RequireWindow().SetComboBoxPopupWidth(Id, width);
        return this;
    }

    private SlimWindow RequireWindow()
    {
        return _window ?? throw new InvalidOperationException("This control handle is not attached to a SlimWindow.");
    }
}

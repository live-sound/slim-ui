namespace SlimUi;

public enum SlimStyleColor : uint
{
    Text = 1u << 0,
    Background = 1u << 1,
    Border = 1u << 2,
    Hover = 1u << 3,
    Press = 1u << 4,
    Selected = 1u << 5,
    Fill = 1u << 6,
    Accent = 1u << 7,
    Placeholder = 1u << 8,
    Selection = 1u << 9
}

namespace SlimUi;

public readonly record struct SlimInset(float Left, float Top, float Right, float Bottom)
{
    public static SlimInset All(float value) => new(value, value, value, value);
}

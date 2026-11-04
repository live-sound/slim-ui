namespace SlimUi;

public readonly struct SlimImage
{
    internal SlimImage(int id, int width, int height)
    {
        Id = id;
        Width = width;
        Height = height;
    }

    public int Id { get; }

    public int Width { get; }

    public int Height { get; }

    public bool IsValid => Id > 0;
}

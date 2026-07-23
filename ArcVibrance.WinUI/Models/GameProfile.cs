namespace ArcVibrance.Models;

public sealed class GameProfile
{
    public string ExecutablePath { get; set; } = string.Empty;
    public int SaturationPercent { get; set; } = 150;
    public bool Enabled { get; set; } = true;
}

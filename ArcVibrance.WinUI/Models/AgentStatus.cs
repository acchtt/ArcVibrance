namespace ArcVibrance.Models;

public sealed record AgentStatus(
    bool AgentRunning,
    bool RuntimeInitialized,
    bool GameActive,
    int ActiveProfileIndex,
    int AppliedSaturationPercent,
    string ActiveExecutableName)
{
    public static AgentStatus Disconnected { get; } = new(false, false, false, -1, 100, string.Empty);
}

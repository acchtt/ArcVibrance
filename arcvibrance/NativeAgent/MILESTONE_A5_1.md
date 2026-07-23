# Milestone A5.1 - Coordinated tray shutdown

Fixes tray Exit so it closes both ArcVibrance.exe and ArcVibrance.Agent.exe.

The agent now locates the hidden or visible UI window by its registered window class,
posts WM_CLOSE to it, then performs the existing agent shutdown path. The runtime still
restores desktop saturation during agent destruction.

## Validation

1. Launch ArcVibrance.exe and confirm both processes are running.
2. Leave the UI open and choose Exit ArcVibrance from the tray icon.
3. Confirm both processes close and desktop saturation returns to 100%.
4. Repeat with the UI hidden/minimized.

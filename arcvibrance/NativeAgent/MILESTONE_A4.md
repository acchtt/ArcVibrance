# Milestone A4 — Agent-owned tray

- ArcVibrance.Agent.exe owns the notification-area icon.
- Double-click or **Open ArcVibrance** launches/restores the settings UI.
- Closing ArcVibrance.exe exits only the settings UI; the agent continues monitoring.
- **Exit ArcVibrance** restores desktop saturation and stops the agent.
- Game/desktop notifications are emitted by the agent.
- Windows autostart targets ArcVibrance.Agent.exe directly.

## Validation
1. Start ArcVibrance.exe and verify one tray icon appears.
2. Close the UI and verify the tray icon and switching remain active.
3. Double-click the tray icon and verify the UI opens.
4. Switch into/out of a configured game and verify notifications.
5. Toggle autostart and verify the Run entry points to ArcVibrance.Agent.exe.
6. Choose Exit ArcVibrance and verify saturation returns to 100% and the agent exits.

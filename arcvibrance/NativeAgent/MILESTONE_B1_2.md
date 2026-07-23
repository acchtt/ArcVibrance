# Milestone B1.2 - Close behavior

- Added an explicit **When closing the window** choice.
- **Minimize to tray** preserves the background agent and is the default.
- **Exit ArcVibrance completely** asks the agent to restore desktop saturation and shut down, then closes the UI.
- The choice is persisted per user under `HKCU\Software\ArcVibrance`.

## Validation

1. Select Minimize to tray, click X, and reopen from the tray.
2. Select Exit completely, click X, and confirm both processes close.
3. Confirm desktop saturation returns to 100% on complete exit.
4. Reopen ArcVibrance and confirm the selected behavior remains saved.

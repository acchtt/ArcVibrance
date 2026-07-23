# Milestone A3 - Separate background agent

## Architecture

- `ArcVibrance.exe`: profile editor, settings window, and temporary tray UI.
- `ArcVibrance.Agent.exe`: exclusive owner of IGCL, foreground detection, profile matching, and saturation restoration.
- `\\.\pipe\ArcVibrance.Agent.v1`: local named-pipe IPC.

The UI starts the agent when needed. Hiding or closing the UI leaves the agent running. Selecting **Exit** from the tray shuts down the agent and restores desktop saturation.

## IPC commands

- GetStatus
- ReloadProfiles
- ReapplyActiveProfile
- RestoreDesktop
- Shutdown

## Windows validation

1. Build both targets and verify both executables are in the same output directory.
2. Launch `ArcVibrance.exe`; verify `ArcVibrance.Agent.exe` appears in Task Manager.
3. Activate a configured game and verify saturation changes.
4. Hide the UI and verify switching continues.
5. Edit an active profile and verify the agent reloads and reapplies it.
6. Add and remove profiles and verify the agent reflects the saved list.
7. End only `ArcVibrance.exe` in Task Manager and verify the agent remains active.
8. Reopen the UI and verify it reconnects to the existing agent.
9. Choose tray **Exit** and verify both processes stop and saturation returns to 100%.

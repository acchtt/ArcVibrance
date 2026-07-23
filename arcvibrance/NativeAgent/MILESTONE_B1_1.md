# Milestone B1.1 - Native UI Refresh

This milestone modernizes the existing Win32 interface without changing the validated agent, IPC, profile storage, or Intel color backend.

## Changes

- Wider dashboard-oriented main window.
- Live agent/profile/saturation status card.
- Quick settings card with startup toggle and profile reload action.
- Larger owner-drawn profile list with improved spacing.
- Larger profile-management actions and clearer labels.
- Existing tray, agent, backend, logging, and profile behavior preserved.

## Windows validation

1. Build `Release | x64` normally.
2. Confirm the main window opens at the new larger size.
3. Confirm live game and saturation status updates every two seconds.
4. Confirm **Reload profiles** succeeds.
5. Add, edit, remove, and double-click profiles.
6. Confirm closing the window leaves the agent/tray running.
7. Confirm tray Exit closes both processes and restores desktop saturation.

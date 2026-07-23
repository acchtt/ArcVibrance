# Milestone A2 - Runtime extraction

This revision moves display-control runtime behavior out of `MainWindow.cpp` into
`ArcVibranceRuntime` while intentionally keeping the application as one executable.

## Runtime responsibilities

- Owns IGCL initialization and shutdown.
- Matches the foreground executable against the profile collection.
- Applies the selected saturation value.
- Restores normal desktop saturation.
- Tracks the active profile and applied runtime status.
- Handles profile edits and removals without exposing IGCL to the UI.

## UI responsibilities

- Owns controls, profile editing, tray presentation, and notifications.
- Forwards foreground events to the runtime.
- Renders `RuntimeStatus` returned by the runtime.

## Validation checklist

1. Configure two applications with visibly different saturation values.
2. Switch between each application and the desktop.
3. Edit the active application's saturation while it is foreground.
4. Delete both an active profile and a profile before the active list index.
5. Exit from the tray and verify desktop saturation returns to 100%.

A3 can now host `ArcVibranceRuntime` in a separate `ArcVibrance.Agent.exe` without
moving the IGCL and profile-selection logic again.

# Milestone A1 — Event-driven runtime

This revision is the first safe step toward the planned Core / Agent / UI split.

## Implemented

- Added `ForegroundWatcher`, a small RAII wrapper around `SetWinEventHook`.
- Foreground changes now wake ArcVibrance through `WM_FOREGROUND_CHANGED` instead of waiting for a 250 ms poll.
- Retained a low-frequency 2-second recovery timer for missed shell events, process transitions, and driver/session edge cases.
- The foreground hook is installed after IGCL/profile initialization and removed before IGCL shutdown.
- The currently active window is evaluated immediately at startup.
- Fixed a malformed duplicate `IsWindow` condition in the shutdown path.

## Runtime behavior

1. Windows emits `EVENT_SYSTEM_FOREGROUND`.
2. `ForegroundWatcher` posts `WM_FOREGROUND_CHANGED` to the ArcVibrance window.
3. The existing profile matcher selects a configured executable.
4. The existing `IGCLManager` applies the profile saturation.
5. A 2-second timer only acts as a recovery check.

No IGCL behavior or profile file format was changed in this revision.

## Next revision

Move the matching/application block out of `MainWindow.cpp` into an `ArcVibranceRuntime` component. That component will become the basis of `ArcVibrance.Agent.exe`, while the current window transitions into a settings client.

# ArcVibrance C1.16

- Made game icon loading resilient by retrying the Windows thumbnail provider and falling back to direct executable icon extraction through Win32 APIs.
- Added a successful-icon cache and a delayed second profile enrichment attempt to avoid transient letter placeholders.
- Hid profile initials whenever a real icon is present so transparent icon regions no longer reveal the fallback letter underneath.
- Repaired the Windows startup entry so it always targets the current `ArcVibrance.Agent.exe`, clears stale disabled `StartupApproved` state when explicitly re-enabled, and reports failures in the UI.
- Made tray creation non-fatal during Windows sign-in. The native agent now keeps monitoring and IPC alive while retrying until Explorer's notification area is ready.
- Updated visible and executable version metadata to C1.16 / 1.16.0.

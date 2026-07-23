# Milestone A5 — Production hardening

## Added

- Thread-safe agent log at `%LOCALAPPDATA%\\ArcVibrance\\Logs\\ArcVibrance.Agent.log`.
- Versioned named-pipe request/response headers with request correlation IDs.
- Explicit IPC errors for invalid requests, protocol mismatch, profile-load failure, and runtime failure.
- Atomic profile saving through `profiles.txt.tmp` plus `MoveFileEx(..., MOVEFILE_WRITE_THROUGH)`.
- CTest profile/path matching regression tests.
- More startup, profile reload, IPC, IGCL, tray, and shutdown diagnostics.

## Build

Build `Release | x64` normally. CMake continues to emit:

- `ArcVibrance.exe`
- `ArcVibrance.Agent.exe`

To run tests from a CMake build directory:

```text
ctest -C Release --output-on-failure
```

## Validation checklist

1. Launch normally and verify both executables run.
2. Confirm the agent log is created under Local AppData.
3. Switch between desktop and two profiles.
4. Edit a profile repeatedly, then inspect `profiles.txt` for complete entries.
5. Close the UI and reopen it from the tray.
6. Exit from the agent tray and confirm desktop restoration.
7. Run CTest and confirm `ProfileMatching` passes.

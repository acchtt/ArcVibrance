# ArcVibrance C1.5

- Treats `ArcVibrance.Agent.exe` as required WinUI content.
- Copies the native agent into both normal build and publish output.
- Passes the exact native build path to MSBuild from `build.ps1`.
- Verifies non-empty UI and agent executables in `dist\x64`.
- Copies the agent beside developer `bin` outputs as a fallback.

# ArcVibrance — C# / WinUI 3 migration

This revision replaces the hand-painted Win32 settings interface with a C# WinUI 3 frontend while retaining the proven native C++ agent and Intel Arc backend.

## Architecture

- `ArcVibrance.WinUI/` — C#/.NET 8, WinUI 3, Windows App SDK 2.2.
- `NativeAgent/` — existing foreground monitor, Intel color backend, tray icon, profile runtime, and named-pipe server.
- `%LOCALAPPDATA%\ArcVibrance\profiles.txt` — unchanged profile format, so existing profiles migrate automatically.
- `\\.\pipe\ArcVibrance.Agent.v1` — unchanged binary IPC protocol.

The frontend executable is named `ArcVibrance.exe`; the backend remains `ArcVibrance.Agent.exe`.

## Requirements

- Windows 10 1809 or later; Windows 11 recommended.
- Visual Studio 2022 with **WinUI application development** and **Desktop development with C++** workloads.
- CMake 3.20 or later.
- .NET 8 SDK.
- Windows App SDK runtime 2.2 for framework-dependent builds.

## Build

The build script automatically stops running local ArcVibrance UI and agent processes from this project folder before publishing. This prevents hidden or minimized instances from locking `ArcVibrance.exe`.


Open PowerShell in this folder:

```powershell
.\build.ps1 -Configuration Release -Platform x64
```

The combined application is written to `dist\x64`. Always launch
`dist\x64\ArcVibrance.exe`; the build verifies that `ArcVibrance.Agent.exe`
is beside it. The native agent is also copied to the WinUI `bin` output so
launching from Visual Studio works.

For a larger deployment that bundles .NET and the Windows App SDK runtime:

```powershell
.\build.ps1 -Configuration Release -Platform x64 -SelfContained
```

## Visual Studio

Open `ArcVibrance.sln` to work on the WinUI frontend. The native agent remains a CMake project and is built by `build.ps1` or through Visual Studio's **Open Folder** CMake support.

## Migration status

Implemented in this conversion:

- Profile loading/saving compatible with the C++ format.
- Add executable picker and drag/drop.
- Per-profile enable switch, delete menu, and inline editor.
- Slider plus a custom inline numeric saturation control.
- Agent start, status polling, reload, reapply, and desktop restore.
- Start-with-Windows, close behavior, and theme registry compatibility, including startup-entry repair and delayed tray creation during sign-in.
- Single-instance activation and tray-agent reopen behavior.
- Resilient executable icon loading with thumbnail retries and direct Win32 icon extraction fallback.
- Automatic game-name detection from Steam/Epic manifests, executable metadata, known executable aliases, installation folders, and cleaned filenames.
- Optional custom profile names stored separately in `%LOCALAPPDATA%\ArcVibrance\profile-names.json`.
- Isolated Profiles, Settings, and About pages with no Win32 repaint flicker.
- Fully theme-aware light surfaces, controls, profile cards, editor, footer, status panel, and native title-bar buttons.

The previous C++ UI is preserved under `NativeAgent/LegacyWin32UI/` for reference but is no longer built.

# ArcVibrance

Per-game saturation profiles for Intel® Arc™ graphics.

ArcVibrance is a lightweight Windows utility that automatically applies a configurable color-vibrance profile when a game starts and restores the desktop color state when the game closes.

Current release candidate: **C1.18.1**

## Features

- Per-game saturation profiles from 0% to 300%
- Automatic profile activation based on the foreground game executable
- Automatic restoration of desktop color settings
- Lightweight native monitoring agent and system-tray operation
- Steam game detection and executable icon extraction
- Windows startup support
- Light and dark WinUI 3 interface

## Requirements

- Windows 10 or Windows 11, 64-bit
- Intel Arc graphics hardware
- A current Intel graphics driver

## Download

Validated Windows builds will be published on the GitHub Releases page.

## Building from source

ArcVibrance contains two main components:

- `ArcVibrance.WinUI` — .NET 8 / WinUI 3 desktop interface
- `NativeAgent` — native C++ monitoring and Intel color-control agent

Recommended tools:

- Visual Studio 2022
- Desktop development with C++ workload
- .NET 8 SDK
- Windows App SDK / WinUI 3 tooling
- CMake

Run from PowerShell:

```powershell
./build.ps1
```

## License

The source code is licensed under the MIT License.

The ArcVibrance name, logo, icon, and visual brand assets are not granted for use in unofficial or modified distributions.

## Disclaimer

ArcVibrance is an independent project and is not affiliated with, endorsed by, or sponsored by Intel Corporation. Intel and Intel Arc are trademarks of Intel Corporation or its subsidiaries.

# ArcVibrance

**Per-game saturation profiles for Intel® Arc™ graphics.**

ArcVibrance is a lightweight Windows utility that automatically applies a configurable color-vibrance profile when a game starts and restores the desktop color state when the game closes.

> Current release candidate: **C1.18.1**

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

ArcVibrance is currently designed specifically for Intel Arc graphics. Other GPU vendors are not supported.

## Download

Validated Windows builds will be published on the [GitHub Releases](https://github.com/acchtt/ArcVibrance/releases) page after final Release x64 testing on Windows.

## Source release status

The public repository setup, licensing, and C1.18.1 release notes are live. The application source tree and compiled Windows package are being prepared for the first complete release commit.

## Building from source

ArcVibrance contains two main components:

- `ArcVibrance.WinUI` — .NET 8 / WinUI 3 desktop interface
- `NativeAgent` — native C++ monitoring and Intel color-control agent

Recommended build environment:

- Visual Studio 2022
- Desktop development with C++ workload
- .NET 8 SDK
- Windows App SDK / WinUI 3 tooling
- CMake

From PowerShell:

```powershell
./build.ps1
```

The solution can also be opened directly with `ArcVibrance.sln` after the source tree is published.

## Reporting issues

Please include your Windows version, Intel Arc GPU model, Intel graphics-driver version, affected game, reproduction steps, and relevant ArcVibrance logs.

## Licensing

ArcVibrance-authored source code is licensed under the [MIT License](LICENSE).

The **ArcVibrance** name, logo, icon, and visual brand assets are covered by the separate [brand usage notice](BRAND_USAGE.md).

Intel Graphics Control Library materials retain their original Intel copyright and license terms. See [third-party notices](THIRD_PARTY_NOTICES.md).

## Disclaimer

ArcVibrance is an independent project and is not affiliated with, endorsed by, or sponsored by Intel Corporation. Intel and Intel Arc are trademarks of Intel Corporation or its subsidiaries.

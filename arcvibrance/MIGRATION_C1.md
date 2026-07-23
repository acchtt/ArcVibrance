# C1 — WinUI 3 frontend migration

## Goals

1. Remove the performance and flicker cost of owner-drawn Win32 controls.
2. Preserve the existing native monitoring and Intel Arc backend.
3. Keep profile and registry settings backward-compatible.
4. Match the approved neon dashboard using XAML rather than manual GDI painting.

## Boundary

The migration deliberately keeps hardware control in `ArcVibrance.Agent.exe`. This reduces risk: WinUI can be iterated independently while the native runtime continues to own foreground detection, tray behavior, saturation application, and recovery.

## Next migration phases

- C1.1: Build and runtime verification on the user's Windows machine.
- C1.2: polish animations, responsive breakpoints, and accessibility.
- C1.3: monitor selection support in the agent protocol.
- C1.4: installer and automatic update strategy.

## C1.1 restore compatibility fix

The direct `Microsoft.Windows.SDK.BuildTools` reference was updated to `10.0.26100.4654` so it matches the minimum version required by `Microsoft.WindowsAppSDK 2.2.0`. This removes the `NU1605` package-downgrade restore failure.


## C1.3 reliability fixes

- The WinUI frontend explicitly launches and verifies the native agent.
- The tray and named-pipe server start before Intel backend initialization.
- Failed backend initialization no longer terminates the tray process; it retries every 10 seconds.
- Closing to tray verifies that the tray agent is alive before hiding the UI.
- Custom WinUI button templates reset hover, pressed, and focus states deterministically.


## C1.4 build-lock fix

`build.ps1` now stops running ArcVibrance UI and native-agent processes whose executable paths belong to the current project before building. This prevents MSBuild errors MSB3021/MSB3027 when a hidden or minimized UI instance locks the publish output. Processes with the same name outside the project directory are left untouched.

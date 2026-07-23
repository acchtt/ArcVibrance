# Milestone B0.2 - Color backend abstraction

## Purpose

The runtime no longer depends directly on Intel IGCL. It consumes the
hardware-neutral `IColorBackend` interface.

## Components

- `IColorBackend`: stable runtime-facing color-control contract.
- `IntelColorBackend`: current production backend wrapping `IGCLManager`.
- `ColorBackendFactory`: single backend-selection point.
- `ArcVibranceRuntime`: accepts an injected backend and contains no IGCL calls.

## Behavior

The default factory still creates `IntelColorBackend`, so this milestone is
intended to preserve all existing Arc GPU behavior.

## Extension path

A future backend only needs to implement `IColorBackend`, for example:

- `AmdColorBackend`
- `NvidiaColorBackend`
- `VirtualColorBackend` for tests
- a dynamic DLL-backed adapter

Backend detection or plugin discovery belongs in `ColorBackendFactory`; it does
not require modifications to the runtime, IPC, tray, or UI.

## Validation

1. Build Release x64.
2. Confirm both ArcVibrance executables are produced.
3. Confirm the agent initializes Intel IGCL as before.
4. Test game-to-desktop and game-to-game profile switching.
5. Test tray exit and desktop saturation restoration.

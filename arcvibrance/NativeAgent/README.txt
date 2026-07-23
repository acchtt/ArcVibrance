ArcVibrance v0.2 refactor
========================

1. Back up your project.
2. Replace your existing main.cpp with the main.cpp in this folder.
3. Copy AppState.*, Settings.*, Theme.*, TrayIcon.*, and MainWindow.* into the project root.
4. Add the new .cpp files to add_executable(ArcVibrance ...) in CMakeLists.txt.
   See CMakeLists_sources.txt.
5. Keep the existing IGCLManager, GameMonitor, GameProfile, and IGCL source files.
6. Reconfigure and build:

   cmake -S . -B build
   cmake --build build --config Release

Architecture
------------
AppState       Shared application state and control IDs
Settings       Windows startup registry setting
Theme          Segoe UI fonts and later dark-theme resources
TrayIcon       Notification-area icon and menu
MainWindow     Main UI, profile editor, commands, and game monitor
main           Windows entry point and message loop

Notes
-----
- --startup now launches the window hidden.
- The UI font is created once and destroyed once.
- The profile editor no longer creates and immediately deletes the shared font.
- The application still uses the default Windows icon. The custom ArcVibrance resource icon is the next isolated step.

B2.8B.1 fixes:
- Removed hard-edged Win32 button regions; all owner-drawn buttons now use anti-aliased painting over the correct parent background.
- Fixed custom slider hover flicker by preventing the native trackbar from repainting during pointer movement.
- Added direct click/drag coordinate mapping using the visible custom track bounds.
- Preserved mouse wheel and keyboard adjustment behavior.

B2.8B.2.2: Fixed the invisible/flickering custom slider by drawing into the actual back buffer and removing redundant repaint cycles.

B2.8C UI revision:
- Restored and aligned the numeric saturation input.
- Rounded outlined value field and editor panel.
- Cyan-violet-pink gradient primary buttons matching the slider.

B2.9.4 flicker fixes:
- Numeric saturation changes now update a private custom-control value rather than repainting the stock STATIC text layer.
- Profiles and Settings use double-buffered drawing and custom mouse-state handling to prevent hover/click flashes.

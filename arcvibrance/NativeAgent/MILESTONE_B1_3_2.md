# ArcVibrance B1.3.2

Fixes a profile editor crash caused by recursive WM_THEMECHANGED handling.

- The profile editor still receives the selected System/Light/Dark theme.
- WM_THEMECHANGED now only invalidates and redraws controls.
- SetWindowTheme is no longer called from inside WM_THEMECHANGED.

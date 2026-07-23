# B2.9.2 - Navigation and page repaint stabilization

- Creates the Settings page controls hidden at their final positions so they never paint over the Profiles page during startup.
- Fully erases the page background after switching pages, preventing stale light-themed group-box pixels and duplicated controls.
- Keeps Profiles and Settings permanently visible in the sidebar.
- Makes clicks on the already-active page a true no-op.
- Suppresses navigation double-click handling and background erasing on owner-drawn buttons.
- Keeps the active navigation gradient visually stable while pressed.
- Removes the default COLOR_WINDOW class brush to prevent white flashes during repaint.

# Milestone B2.9.4 - Flicker-free navigation and numeric spinner

- Moved the custom saturation value out of the stock STATIC text renderer.
- Removed redundant SetWindowText/WM_SETTEXT repaint cycles from slider and spinner updates.
- Kept the spinner fully double-buffered and invalidated it without background erasure.
- Double-buffered owner-drawn buttons so gradients, icons, and labels appear in one frame.
- Took direct ownership of Profiles/Settings mouse press, release, hover, and double-click handling.
- Prevented the stock BUTTON hot/pressed renderer from repainting beneath sidebar navigation.
- Removed rounded child-window regions from navigation buttons to eliminate corner flashes.

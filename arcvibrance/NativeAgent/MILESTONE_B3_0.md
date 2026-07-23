# Milestone B3.0 - Neon dashboard redesign

This revision restructures the Win32 UI around the supplied ArcVibrance dashboard reference while preserving the existing Intel backend, profile storage, IPC, monitoring, tray, and startup behavior.

## Implemented

- New neon-gradient sidebar with a vector ArcVibrance mark.
- Profiles, Settings, and About navigation with matching line icons.
- Separate page hosts to keep page controls isolated.
- Larger profile dashboard with compact Add Game action.
- Borderless profile list made from individual rounded cards.
- Cyan-to-violet-to-magenta selected-card border.
- Full executable paths, saturation accents, enable switches, and overflow menus on cards.
- Dashed drag-and-drop zone.
- Quick Actions card using the existing edit, remove, and reload commands.
- Taller inline editor matching the reference proportions.
- Profile status switch in the editor.
- Dedicated About page; the unnecessary App Assets panel was not added.
- Darker navy palette with subtle cyan, violet, and magenta background glows.

## Notes

The native title bar remains in place in this Win32 revision. A completely frameless title bar, richer shadows, and animated transitions are better suited to a XAML or QML frontend and can be introduced later without changing the backend protocol.

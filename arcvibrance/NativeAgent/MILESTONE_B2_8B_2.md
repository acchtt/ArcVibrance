# B2.8B.2 — Slider repaint and interaction fix

- Uses an off-screen bitmap for flicker-free slider painting.
- Removes native tick/selection styles from the custom-painted trackbar.
- Prevents profile-list redraws during thumb tracking; the selected card refreshes only when tracking finishes.
- Avoids duplicate slider notifications when the pointer remains on the same value.
- Separates the numeric field from the slider bounds so it cannot cover the thumb.

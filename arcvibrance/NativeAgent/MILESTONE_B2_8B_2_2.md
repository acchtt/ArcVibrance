# B2.8B.2.2 — Slider visibility and repaint fix

- Draws the entire custom slider into the off-screen buffer before presenting it.
- Prevents the final buffer copy from erasing the gradient track and thumb.
- Removes duplicate invalidation during click, drag, wheel, and keyboard updates.
- Avoids setting the same slider value again inside the profile commit path.
- Keeps profile-card redraw deferred until drag completion.

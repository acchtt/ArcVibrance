# B2.8B — Smooth buttons and custom saturation slider

- Applies rounded window regions to every owner-drawn action button so no native square corners remain around the anti-aliased artwork.
- Replaces the visible native trackbar painting with an ArcVibrance custom-rendered gradient slider.
- Keeps the Win32 trackbar's existing range, keyboard, mouse, focus, accessibility, TBM messages, and WM_HSCROLL behavior.
- Uses a cyan → violet → pink track with a smooth outlined thumb and subtle glow.
- Does not change profile persistence, individual profile toggles, IPC, monitoring, or the Intel color backend.

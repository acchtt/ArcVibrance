# B2.9.1 - Sidebar navigation repaint fix

- Keeps both Profiles and Settings navigation buttons visible on every page.
- Prevents inactive sidebar buttons from being painted over during page changes.
- Avoids rebuilding the active page when its navigation button is clicked again.
- Batches page visibility updates to reduce settings-page flicker.
- Preserves icon rendering and outline-free inactive navigation styling.

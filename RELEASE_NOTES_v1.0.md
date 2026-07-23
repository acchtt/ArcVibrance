# ArcVibrance v1.0

First stable public release.

## Highlights

- Per-game saturation profiles for Intel Arc graphics.
- Automatic game detection and desktop-color restoration.
- Native background monitoring agent with tray integration.
- Steam executable detection and improved game-icon loading.
- Windows startup support.
- Refined dark and light WinUI 3 themes.
- Rounded neon ArcVibrance branding across the app, tray, taskbar, About page, and footer.

## v1.0 updates

- Fixed a startup XAML resource-resolution failure introduced during the light-theme accent pass.
- Preserved the updated Save Changes button and sidebar tagline colors.
- Ensured required resources exist in both light and dark theme dictionaries.
- Corrected the Website, GitHub, and Updates links in the desktop interface.
- Added the `acchtt` creator credit to the About page and project website.

## Requirements

- Windows 10 or Windows 11, 64-bit
- Intel Arc GPU
- Current Intel graphics driver

## Validation

Built and verified as a self-contained Windows x64 package by GitHub Actions.

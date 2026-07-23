# ArcVibrance C1.18.1

First public release candidate.

## Highlights

- Per-game saturation profiles for Intel Arc graphics.
- Automatic game detection and desktop-color restoration.
- Native background monitoring agent with tray integration.
- Steam executable detection and improved game-icon loading.
- Windows startup support.
- Refined dark and light WinUI 3 themes.
- Rounded neon ArcVibrance branding across the app, tray, taskbar, About page, and footer.

## C1.18.1 hotfix

- Fixed a startup XAML resource-resolution failure introduced during the light-theme accent pass.
- Preserved the updated Save Changes button and sidebar tagline colors.
- Ensured required resources exist in both light and dark theme dictionaries.
- Corrected the Website, GitHub, and Updates links in the desktop interface.
- Added the `acchtt` creator credit to the About page and project website.

## Requirements

- Windows 10 or Windows 11, 64-bit
- Intel Arc GPU
- Current Intel graphics driver

## Validation note

The source was prepared in a Linux environment. Build and test the Release x64 binaries on Windows before attaching them to a public GitHub Release.

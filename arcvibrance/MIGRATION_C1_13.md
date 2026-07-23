# ArcVibrance C1.13

- Added offline game-name detection for existing and newly added executable profiles.
- Detection order: Steam manifest, Epic Games manifest, known executable aliases, Windows version metadata, installation folder, cleaned filename.
- Profile cards and the editor now show the detected game name while retaining the executable filename and full path.
- Added Rename profile, Use detected name, and Detect game name again actions to each profile menu.
- Added Detect game names to the global Profile tools menu.
- Custom names are saved separately in `%LOCALAPPDATA%\ArcVibrance\profile-names.json`; the native `profiles.txt` format and C++ agent remain unchanged.

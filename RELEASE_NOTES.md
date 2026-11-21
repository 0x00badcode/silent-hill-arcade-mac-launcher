# v0.6.1 — Mac launcher preview

An unofficial launcher for Silent Hill The Arcade on Apple silicon Macs. **The game is not included.** Import your own complete copy of the supported June 2022 standalone Windows build on first launch.

- Native Mac file picker for a game folder, its `KSHG.exe`, or a complete ZIP archive.
- Bundled Wine runtime with a fix for a reproduced Rosetta transition crash.
- Mouse aiming with an aligned in-game crosshair; one shot per click.
- Reload with **R** or right-click, fullscreen with **F**, pause with **Esc**.
- Original 4:3 presentation and cinematic rendering adaptations.
- Imported files and settings stored in Application Support; original source files are left untouched.

Download the **arm64 DMG**, drag the app into Applications, then open it. See **LAUNCHER.md** for import instructions and controls. Press **1 on the number row** at the title screen to start.

This preview is **not notarized**. Its filename includes `UNNOTARIZED`; macOS may require per-app first-launch approval. A normal signed public release is still pending Developer ID signing and Apple notarization. Runtime license/source redistribution requirements also need to be completed before a public binary release; see `THIRD_PARTY.md`.

Tested on an M4 Mac with macOS 26.6.2. The patched runtime passed 11 stress runs, and a game session ran for more than 19 minutes without the earlier automatic-restart workaround. Other Mac models and a complete campaign playthrough have not been verified.

This project is not affiliated with Konami.

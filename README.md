# Silent Hill The Arcade — Mac launcher

An unofficial launcher for playing Silent Hill The Arcade on an Apple silicon Mac with a mouse. It bundles a Wine runtime, adapts the cabinet controls, and imports your game files on first launch.

**The game is not included.** Supply your own complete copy of the supported **June 2022 standalone Windows build**.

## Play

1. Download the **macOS arm64 DMG** from this repository’s **Releases** page, when available.
2. Open the DMG, drag **Silent Hill The Arcade.app** into **Applications**, and open it.
3. Choose the complete game folder, `KSHG.exe` inside that folder, or a ZIP containing the complete game. Wait for import, then press **1 on the number row** at the title screen.

After the first import, double-click the app to play. Wine, Python, and Homebrew do not need to be installed by players.

Read **[LAUNCHER.md](LAUNCHER.md)** for installation, requirements, import instructions, controls, and troubleshooting.

## Features

- Mouse aiming with an in-game crosshair and one shot per click.
- Reload with **R** or right-click, fullscreen with **F**, and pause with **Esc**.
- Original 4:3 picture and cinematic rendering adaptations.
- A patched Wine runtime addressing a reproduced Rosetta mode-transition crash.
- A separate local game profile, with compatibility edits applied to the imported copy.

## Compatibility and release status

This **0.6.1 preview** targets Apple silicon Macs running macOS 13 or later, with Rosetta 2. Testing used an Apple M4 and macOS 26.6.2. Intel Macs are not supported by this build. A complete campaign playthrough remains unverified.

The current test DMG is named **UNNOTARIZED**. A standard public download still needs Developer ID signing, Apple notarization, and the runtime license/source materials described in [THIRD_PARTY.md](THIRD_PARTY.md). See [RELEASING.md](RELEASING.md) before publishing a binary release.

## Build and package

Developers need Python 3.9 or newer, Apple’s command-line developer tools, and the supported [Gcenx Wine Staging 11.21 bundle](https://github.com/Gcenx/macOS_Wine_builds/releases/tag/11.21). No game files are needed to build the launcher.

```sh
python3 build-launcher.py \
  --wine-app '/Applications/Wine Staging.app' \
  --output 'build/Silent Hill The Arcade.app'

python3 package-release.py \
  --app 'build/Silent Hill The Arcade.app' \
  --test
```

The builder validates the runtime and applies the Rosetta patch. The packager signs the native components, creates a DMG in `dist`, verifies it, and writes a SHA-256 checksum. The `--test` download is explicitly unnotarized. [RELEASING.md](RELEASING.md) explains the Developer ID and notarization workflow.

Generated app bundles, disk images, downloaded runtimes, and local game files are excluded from Git. Attach the finished DMG and checksum as GitHub Release assets.

## Project files

| File | Purpose |
| --- | --- |
| [LAUNCHER.md](LAUNCHER.md) | Player guide |
| [RELEASING.md](RELEASING.md) | Build, signing, notarization, and GitHub publishing |
| [RELEASE_NOTES.md](RELEASE_NOTES.md) | Preview release-note draft |
| [Source/README.md](Source/README.md) | Launcher, adapter, and validation evidence |
| [Source/WineRosetta/README.md](Source/WineRosetta/README.md) | Crash diagnosis and reproducible runtime patch |
| [THIRD_PARTY.md](THIRD_PARTY.md) | Runtime licenses and source references |

Original launcher, adapter, and packaging code use the [MIT license](LICENSE). Third-party components retain their own licenses. Silent Hill The Arcade belongs to its respective rights holders; this project is not affiliated with Konami.

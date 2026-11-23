# Silent Hill The Arcade for Mac

An unofficial launcher to play Silent Hill The Arcade on a Mac with mouse aiming, a crosshair, fullscreen, and pause controls.

**The game is not included.** You need a complete copy of the **June 2022 standalone Windows build**.

## Requirements

- **Apple silicon Mac** (M1 or newer), **macOS 13 or later**, and [Rosetta 2](https://support.apple.com/en-us/102527). Intel Macs are not supported.
- **8 GB free space**, or **12 GB** when importing a ZIP or RAR.

Wine is included. You do not need Python, Homebrew, or a Windows virtual machine.

## How to play

1. Download the **macOS arm64 DMG** from **Releases** when available. Choose the DMG, not the “Source code” download.
2. Open it, drag **Silent Hill The Arcade.app** into **Applications**, then open the app.
3. Choose your **complete game folder**, **ZIP**, or **RAR**, then click **Import Game**. You can also choose a folder containing one game archive, or select `KSHG.exe` inside an extracted game folder.
4. Wait for setup. At the title screen, click inside the game window and press **1 on the number row** to start. Press **1** again to skip the Player 2 wait.

The extracted folder must contain `KSHG.exe`, its DLLs, `Data`, and `Shader`. The executable alone is not enough. ZIP and RAR files are unpacked automatically; extract 7z archives first.

Setup can take a few minutes. **After the first import, just double-click the app to play.** Your original files are left untouched.

## Controls

| Action | Control |
| --- | --- |
| Start | **1** on the number row |
| Aim | Move the mouse |
| Shoot | **Left-click** |
| Reload | **R** or **right-click** |
| Fullscreen / windowed | **F** |
| Pause / resume | **Esc**; **OK** also resumes |
| Quit | Return to windowed mode with **F**, then close the window |

## Important information

- **This preview is not notarized by Apple.** If macOS blocks it, use [Apple’s per-app approval instructions](https://support.apple.com/en-us/102445) if you trust the download.
- Fullscreen preserves **4:3**. Black bars on wider screens are normal.
- Tested on an **M4 with macOS 26.6.2**. Other Mac models and a full campaign playthrough remain unverified.
- “Unsupported version” or “missing files” means you need the supported build and its complete folder.

For problems, use **Show Log** and include the log, Mac model, and macOS version in a GitHub issue. Local files and logs are stored in:

```text
~/Library/Application Support/Silent Hill The Arcade/
```

[MIT license](LICENSE) · [Runtime credits and licenses](NOTICE) · Not affiliated with Konami.

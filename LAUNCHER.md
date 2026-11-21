# Silent Hill The Arcade — Mac launcher

Play Silent Hill The Arcade on an Apple silicon Mac with a mouse. The launcher includes its compatibility runtime and mouse controls. **It does not include the game.** You need your own complete copy of the supported Windows game files.

## What you need

- An Apple silicon Mac (M1 or newer). Intel Macs are not supported by this launcher build.
- macOS 13 or later. Testing so far used an M4 Mac with macOS 26.6.2; other Mac and macOS combinations have not been verified.
- Rosetta 2. If it is missing, follow [Apple’s Rosetta installation instructions](https://support.apple.com/en-us/102527).
- The **June 2022 standalone Windows build** of Silent Hill The Arcade. Other executable versions are rejected during import.
- At least **8 GB of free space**, or **12 GB when importing a ZIP**, to leave room for temporary files and the local game profile.

You do not need to install Wine, Python, Homebrew, or a Windows virtual machine.

## Install the launcher

1. Download the macOS **arm64 DMG** from the project’s GitHub **Releases** page. The automatically generated “Source code” downloads are for developers.
2. Open the DMG and drag **Silent Hill The Arcade.app** into **Applications**.
3. Eject the disk image, then open the app from Applications.

**Test-build signing:** a download with `UNNOTARIZED` in its name has not completed Apple’s signing and notarization process. macOS may block its first launch. If you choose to run that test build, use the per-app approval described in [Apple’s instructions for opening apps safely](https://support.apple.com/en-us/102445). The normal public-release experience requires a Developer ID-signed, notarized download.

## Import your game once

The first launch opens a file picker. Choose any one of these:

- The complete game folder containing `KSHG.exe`, its Windows DLLs, `Data`, and `Shader`.
- `KSHG.exe` inside that complete folder.
- A **ZIP archive** containing the complete game folder, including an archive with a containing folder around it.

Click **Import Game** and wait for setup to finish. The launcher verifies the supported executable version and checks that the expected game assets are present. It copies the required files into your Mac’s Application Support folder and applies the Mac compatibility changes to that copy. Your selected source files are left untouched.

An executable by itself is not enough. For RAR, 7z, or other archive formats, extract the archive first and select the extracted game folder. If the file picker is cancelled, the launcher closes; open it again when you are ready.

The game opens after import. Initial setup can take a few minutes. **On later launches, just double-click the app.** You can move or remove the source folder after a successful import.

## Start playing

Click inside the game window. At the title screen, when **Press the START button** appears, press **1 on the number row**. The cabinet is set to Free Play, so no coin is needed. Press **1** again to skip the wait for Player 2.

The startup scenes can loop while the game waits for Start. The mouse crosshair appears when the game accepts aiming input.

| Action | Control |
| --- | --- |
| Start Player 1 / skip Player 2 wait | **1** on the number row |
| Aim | Move the mouse |
| Shoot | **Left-click** |
| Reload | **R** or **right-click** |
| Toggle fullscreen | **F** |
| Pause | **Esc** |
| Resume from the pause dialog | **Esc** or **OK** |
| Quit | Return to windowed mode with **F**, then close the game window |

Fullscreen preserves the game’s original **4:3** picture. Black bars on wider screens are expected. Use **F** for fullscreen so the crosshair and shot coordinates follow the game’s display mode.

## Troubleshooting

**“Unsupported version” during import:** choose the June 2022 standalone build. The launcher checks both `KSHG.exe` and the cabinet input DLL. Renaming an executable does not make another version compatible.

**“Missing” file or folder during import:** choose the complete extracted game folder. Keep its directory structure intact. A partial download, the executable alone, or a folder of symbolic links will not work.

**The title scenes loop:** click in the game window and press **1** on the number row when the Start prompt appears.

**No crosshair or no shots after changing apps:** return to the game, click inside its window, and move the mouse over the picture. Make sure a player has joined and gameplay has resumed.

**The game exits or will not launch:** if the launcher offers **Show Log**, use it. Include the launch log, your Mac model, macOS version, and the point in the game where the problem occurred in a GitHub issue. The launcher does not automatically restart after a crash.

Open Finder, choose **Go → Go to Folder**, and paste this location to find your imported files and logs:

```text
~/Library/Application Support/Silent Hill The Arcade/
```

- `Game` contains the imported copy with Mac compatibility changes.
- `wine` contains the local Windows profile and writable cabinet data.
- `last-launch.log` and `wine-run.log` describe the most recent session.

To import another copy, quit the game and launcher, move the `Game` folder out of this directory, then open the launcher again. Keep the old folder until the replacement imports successfully. Removing the app from Applications does not remove these local files.

## Current testing scope

The runtime includes a fix for a reproduced Wine/Rosetta mode-transition crash. The original runtime failed 5 of 9 stress runs; the patched runtime passed all 11. A separate game session ran for more than 19 minutes through multiple cinematics and user-controlled gameplay without restarting. A complete campaign playthrough and testing on additional Mac models are still outstanding.

This is an unofficial compatibility launcher and is not affiliated with Konami. The game files must be supplied separately by the player.

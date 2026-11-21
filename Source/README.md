# Launcher source

`Launcher.m` is the native Apple silicon entry point and game importer. `GamePatch.json` lists the supported input hashes, exact compatibility edits, output hashes, and expected asset paths. `play.sh` launches one game session using the imported files and an isolated Wine profile. `BundleResources` contains launcher-specific resources and the prebuilt Windows display/input adapter; it contains no game executable or game assets.

The adapter source is `arcade_display.c` with exports in `arcade_display.def`. Rebuild it with LLVM-MinGW's 32-bit compiler:

```
i686-w64-mingw32-clang -O2 -shared arcade_display.c arcade_display.def -ld3d9 -lgdi32 -luser32 -lstrmiids -luuid -lole32 -o macd.dll
```

The adapter binary in `BundleResources` comes from the tested local app. `patch_game.py` is the earlier standalone reference patcher; players use the native importer and do not need Python. `WineRosetta` contains the CPU transition patch, pinned upstream/reference sources, LGPL text, and stress-test evidence.

`import-validation.json` records successful original-folder and nested-ZIP imports, version and asset rejection, source preservation, and invalid-archive/symbolic-link handling. ZIP testing used a small fixture with real supported executable files and placeholder asset paths to isolate extraction; the folder import used the complete local game and verified the exact patched executable hashes. `dependency-validation.json` records the native-library portability scan: 588 native files, with no absolute dependencies outside Apple's system library paths.

Use the project-root `build-launcher.py` to build a fresh app from this source and an existing supported Wine Staging 11.21 app. Then use `package-release.py` to create a test or notarized DMG. See the project-root `RELEASING.md` for the complete publishing workflow.

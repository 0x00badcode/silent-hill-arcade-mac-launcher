# Publishing the Mac launcher

The playable local app is fixed for the reproduced Wine/Rosetta crash. The public launcher adds one-time import of the player’s own game files. The public download contains the compatibility runtime and adapter, with no game executable, Windows game DLLs, cinematics, sound, or level data.

`LAUNCHER.md` is the player-facing guide. Include it in the repository and attach it to the release. The packaging tool also puts a copy inside the DMG.

## Current release status

Version **0.6.1** is a test release. This Mac has **no valid Developer ID Application signing identity installed**. A local ad hoc signature is useful for testing but does not provide the standard Gatekeeper experience on someone else’s Mac. A test DMG is explicitly named `UNNOTARIZED`.

For normal distribution outside the Mac App Store, use a Developer ID Application certificate and Apple notarization. The bundled Wine engine includes many native executables and libraries; signing only the outer app is insufficient. The packaging tool signs native code and nested bundles from the inside out, then signs the app. Both Wine loader executables receive the executable-memory and library-loading exceptions they need. The runtime’s other native executable helpers receive the library-loading exception; the native launcher receives neither exception. No debugging entitlement is enabled.

## Create a test download

From this directory, with Python 3.9 or newer installed:

```sh
python3 package-release.py --test
```

The script creates a compressed DMG in `dist`, verifies its integrity and code signatures, and writes its SHA-256 checksum. The disk image contains the app, an Applications shortcut, the usage guide, and notices. This mode does not contact Apple or publish anything. It refuses to overwrite an existing download.

## Sign and notarize a public download

1. Use your Apple Developer account to install a **Developer ID Application** certificate and its private key in your login Keychain. Check it with `security find-identity -v -p codesigning`. Account enrollment, certificate creation, and credential entry must be completed by the publisher.
2. Store notarization credentials with `xcrun notarytool store-credentials "silent-hill-notary"`. Enter the requested credentials yourself in Terminal. Do not put passwords or private keys in this repository.
3. Run the packaging tool with the exact identity shown by Keychain:

```sh
python3 package-release.py \
  --identity 'Developer ID Application: YOUR NAME (TEAMID)' \
  --notary-profile 'silent-hill-notary'
```

This signs all native components with the supplied identity and hardened runtime, creates and signs the DMG, submits it to Apple, downloads the notarization log, checks for acceptance, staples the ticket, and assesses the DMG with Gatekeeper. A rejected submission stops the process. Apple signing and notarization have **not** been tested with a Developer ID certificate on this machine yet.

Review the saved notary log even after acceptance. Test the downloaded, quarantined DMG on another Apple silicon Mac with no existing profile, including import and the next launch. Test both a new installation and an upgrade. Change the bundle identifier in `Contents/Info.plist` to your own stable identifier before your first signed public release.

## Publish on GitHub

Create a repository containing the launcher source, adapter source, Wine patch and provenance, usage guide, publishing guide, license, and third-party notices. Keep generated `.app` bundles and `.dmg` downloads out of Git history using the included `.gitignore`. Supply the corresponding source and license notices required by the bundled runtime and its dependencies as described in `THIRD_PARTY.md`.

Create a tag such as `v0.6.1` and a GitHub Release. Upload the **notarized DMG**, its `.sha256` file, and `LAUNCHER.md` as release assets. For an explicitly unnotarized preview, mark the release as a prerelease and describe the signing limitation in the release notes. GitHub requires each release asset to be under **2 GiB**; the packaging tool checks this limit.

The player’s flow is: download DMG → drag app into Applications → open app → import their game once → play. Future launches open the installed app directly. macOS can still show its normal first-open confirmation, and Rosetta may need installing on a Mac that has never used it.

## Build the launcher again

The bundle includes editable sources in `Contents/Resources/Source`. The repository also includes `build-launcher.py` and all launcher-specific bundle resources. With Python 3.9 or newer and Apple’s command-line developer tools, build a new app using an existing Gcenx Wine Staging 11.21 bundle:

```sh
python3 build-launcher.py \
  --wine-app '/path/to/Wine Staging.app' \
  --output 'build/Silent Hill The Arcade.app'
```

The builder checks the exact supported runtime DLL, applies the Rosetta patch if needed, copies the runtime and launcher resources, compiles the native launcher, and creates a local ad hoc signature. It requires no game files. Package that rebuilt app with:

```sh
python3 package-release.py --app 'build/Silent Hill The Arcade.app' --test
```

Use the same `--app` argument with the identity and notarization options when producing a public download. To compile only the native launcher:

```sh
xcrun clang -fobjc-arc -O2 -Wall -Wextra -Werror \
  -Wno-deprecated-declarations -target arm64-apple-macos13.0 \
  -framework Cocoa -framework UniformTypeIdentifiers \
  Source/Launcher.m -o 'Silent Hill The Arcade.app/Contents/MacOS/Play'
```

Copy `Source/GamePatch.json` into `Contents/Resources/GamePatch.json` after editing it. The launch script uses the imported `Game` folder in Application Support. Any change inside the bundle requires re-signing and a new package. The Wine CPU bridge has its own hash-checked rebuild script and original/reference source under `Source/WineRosetta`.

## Official references

- [Apple: packaging Mac software](https://developer.apple.com/documentation/xcode/packaging-mac-software-for-distribution)
- [Apple: Developer ID certificates](https://developer.apple.com/help/account/certificates/create-developer-id-certificates)
- [Apple: customizing notarization](https://developer.apple.com/documentation/security/customizing-the-notarization-workflow)
- [GitHub: releases and asset limits](https://docs.github.com/en/repositories/releasing-projects-on-github/about-releases)

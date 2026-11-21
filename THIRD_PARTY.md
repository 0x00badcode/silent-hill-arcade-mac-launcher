# Third-party runtime and source references

The project’s MIT license covers its original launcher, display/input adapter, packaging tool, and documentation. It does not replace third-party licenses and does not cover the commercial game. No game files are included in the launcher download.

## Wine and the Rosetta patch

The bundled engine is **Gcenx Wine Staging 11.21**, released October 31, 2026. Wine is distributed under the GNU LGPL. `Source/WineRosetta/COPYING.LIB` contains the LGPL 2.1 text; the source snapshots retain their original copyright notices.

- [Runtime package and build information](https://github.com/Gcenx/macOS_Wine_builds/tree/11.21)
- [Wine 11.21 source](https://github.com/wine-mirror/wine/tree/wine-11.21)
- [Wine Staging patches](https://github.com/wine-staging/wine-staging/tree/v11.21)
- [Published Rosetta workaround source, pinned revision](https://github.com/dttdrv/wiage/blob/3f52c197d9fc9569a09d224b861e3010b397cb03/dlls/wow64cpu/cpu.c)

The bundled `wow64cpu.dll` has been changed. `Source/WineRosetta` supplies the original DLL, the exact hash-checked patch script, original and reference C source, provenance, and test evidence. This allows the modified DLL to be reproduced or replaced. The runtime remains a separate component inside `Contents/Resources/Wine Staging.app`; the native launcher is not linked against Wine.

## Runtime dependencies

Gcenx’s package also contains GStreamer 1.28.1, Wine Mono, Wine Gecko, and native dependencies supplied through MacPorts. Their licenses are separate from the launcher license. Keep upstream copyright and license notices when redistributing them. Their source and licensing information are available from the component projects and the package build recipes:

- [GStreamer source and licensing](https://gstreamer.freedesktop.org/documentation/additional/licensing.html)
- [Wine Mono](https://github.com/wine-mono/wine-mono)
- [Wine Gecko](https://gitlab.winehq.org/wine/wine-gecko)
- [Gcenx MacPorts Wine build overlay](https://github.com/Gcenx/macports-wine)
- [MacPorts dependency recipes](https://github.com/macports/macports-ports)

Native dependency families in this package include MoltenVK and Vulkan Loader, SDL2, GNU TLS/Nettle/GMP, FreeType, ICU, gettext, libiconv, libxml2/libxslt, Brotli, bzip2, libffi, libidn2, libinotify, LZ4, XZ, p11-kit, libpcap, libpng, libtasn1, libunistring, zlib, Zstandard, and MacPorts legacy support. GStreamer also includes its own transitive dependencies.

This index identifies the runtime supplier and source projects; it is not a complete corresponding-source archive or an audit of every transitive library. Before a public binary release, assemble the applicable upstream license notices and provide the corresponding source required by the runtime components’ licenses, including the precise dependency versions from the supplier’s build recipes. Keep that source material available alongside the release. The DMG packaging and Apple notarization steps do not establish license compliance.

## Game and trademarks

Silent Hill The Arcade and its game assets belong to their respective rights holders. Players supply the supported game files themselves. This is an unofficial compatibility project and is not affiliated with Konami.

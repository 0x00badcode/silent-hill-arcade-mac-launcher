#!/bin/bash
set -euo pipefail
umask 077

state=$1
resources="$(cd -- "$(dirname -- "$0")" && pwd -P)"
wine_bin="$resources/Wine Staging.app/Contents/Resources/wine/bin"
assets="$state/Game"
prefix="$state/wine"
game="$prefix/drive_c/Arcade"

export PATH=/usr/bin:/bin:/usr/sbin:/sbin
export HOME="$state/home"
export WINEPREFIX="$prefix"
export WINEDLLOVERRIDES='d3d9,dsound=b;mscoree,mshtml=;winemenubuilder.exe='
export WINEDEBUG='-all,err+module,err+seh,err+winediag,err+quartz'
export MVK_CONFIG_LOG_LEVEL=0
export GST_DEBUG=0
export GST_REGISTRY_1_0="$state/gstreamer-registry.bin"

if ! /usr/bin/arch -x86_64 /usr/bin/true; then
    echo 'Rosetta is required to run the bundled Wine engine.' >&2
    exit 1
fi
mkdir -p "$HOME"
if [[ ! -f "$state/ready" ]]; then
    "$wine_bin/wine" wineboot --init
    "$wine_bin/wine" reg add 'HKCU\Software\Wine\Direct3D' /v renderer /d gl /f
    "$wine_bin/wineserver" -w
    touch "$state/ready"
fi

# Keep Windows drive mappings and standard folders inside this game's directory.
for drive in "$prefix/dosdevices/"*; do
    if [[ -L "$drive" && "${drive##*/}" != c: ]]; then
        unlink "$drive"
    fi
done
ln -s '../drive_c' "$prefix/dosdevices/z:"
for profile in "$prefix/drive_c/users/"*; do
    [[ -d "$profile" && "${profile##*/}" != Public ]] || continue
    for item in "$profile/"*; do
        if [[ -L "$item" ]]; then
            unlink "$item"
            mkdir "$item"
        fi
    done
    controls="$profile/AppData/Roaming/bemani_config"
    mkdir -p "$controls"
    if [[ ! -f "$controls/sha_v01.cfg" ]]; then
        cp "$resources/sha_v01.cfg" "$controls/sha_v01.cfg"
    fi
done

# Keep the writable cabinet state and logs separate from the bundled game assets.
mkdir -p "$game/Data"
for source in "$assets/"*; do
    name=${source##*/}
    if [[ "$name" != Data ]]; then
        ln -sfn "$source" "$game/$name"
    fi
done
for source in "$assets/Data/"*; do
    name=${source##*/}
    if [[ "$name" != acdata ]]; then
        ln -sfn "$source" "$game/Data/$name"
    fi
done
if [[ ! -d "$game/Data/acdata" ]]; then
    cp -R "$assets/Data/acdata" "$game/Data/acdata"
fi

# Existing profiles carry Wine's builtin DLLs. Refresh the patched CPU bridge
# before launching so profiles created by earlier app versions receive the fix.
cpu_bridge="$wine_bin/../lib/wine/x86_64-windows/wow64cpu.dll"
profile_bridge="$prefix/drive_c/windows/system32/wow64cpu.dll"
if ! cmp -s "$cpu_bridge" "$profile_bridge"; then
    cp "$cpu_bridge" "$profile_bridge"
fi

cd "$game"
run_log="$state/wine-run.log"
echo 'Starting Silent Hill'
result=0
"$wine_bin/wine" start /d 'C:\Arcade' /wait 'C:\Arcade\KSHG.exe' > "$run_log" 2>&1 || result=$?
cat "$run_log"
exit "$result"

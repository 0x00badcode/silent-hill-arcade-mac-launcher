#!/usr/bin/env python3
"""Apply the Mac compatibility patches to the June 2022 standalone binaries."""
from hashlib import sha256
from pathlib import Path
import sys

PATCHES = {
    "KSHG.exe": (
        "9b8a6c1bf81f9d690e2bedb94e3ab2844313a83758923c22704da3241e492995",
        [
            # Load the display and mouse adapter, which forwards to Wine’s Direct3D 9.
            (0x2C6D96, "64 33 64 39 2e 64 6c 6c", "6d 61 63 64 2e 64 6c 6c"),
            # Preserve display formats while allowing the Mac's available modes.
            (0x08582, "75 06", "eb 13"),
            # Choose the existing windowed renderer at startup.
            (0x2476C, "e8 4f 2a fe ff", "e8 2f 28 fe ff"),
            (0x24793, "6a 00", "6a 01"),
            # Permit the modified executable to pass its game-file check.
            (0xD7073, "74 0d", "90 90"),
            # Use the original uncalibrated path for mouse coordinates. The caller still handles off-screen reload.
            (0xD1303, "75 0e", "90 90"),
            # Ignore a video frame when DirectShow supplies a null source buffer.
            (0x00250, "bcef2700", "cfef2700"),
            (0x01BE5, "8b 97 60 01 00 00", "e8d2e3270090"),
            (0x27FFBC, "00" * 19, "8b976001000085d2750883c404e9cc1cd8ffc3"),
            # The graph is still stopped here. Wine's asynchronous StopWhenReady can stop it after Run.
            (0x01EBE, "50 ff 52 3c", "90 90 90 90"),
        ],
    ),
    "shaiolib.CRK.dll": (
        "b83a517531472de188d5ce887901884f5d205776d79fd9fd02f3ec783a5c4456",
        [
            # The window name passed by this build is the ANSI string KSHG.
            (0xB1E4, "46 69 6e 64 57 69 6e 64 6f 77 57 00", "46 69 6e 64 57 69 6e 64 6f 77 41 00"),
            # Leave the Escape key to the pause handler instead of exiting the cabinet process.
            (0x05F6, "6a 00 ff 15 00 b0 00 10", "c2 0c 00 90 90 90 90 90"),
        ],
    ),
}


def patch(source, destination):
    for name, (expected_hash, edits) in PATCHES.items():
        data = bytearray((source / name).read_bytes())
        if sha256(data).hexdigest() != expected_hash:
            raise ValueError(f"Unexpected version of {name}")
        for offset, old_hex, new_hex in edits:
            old = bytes.fromhex(old_hex)
            new = bytes.fromhex(new_hex)
            if len(old) != len(new) or data[offset:offset + len(old)] != old:
                raise ValueError(f"Unexpected bytes at {offset:#x} in {name}")
            data[offset:offset + len(old)] = new
        (destination / name).write_bytes(data)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("Usage: patch_game.py ORIGINAL_FOLDER OUTPUT_FOLDER")
    patch(Path(sys.argv[1]), Path(sys.argv[2]))

# Rosetta transition crash fix — app 0.6.0

The prior game logs reported Wine reads of `00002DC1` at `7BF41239`, or `00002EC5` at `7BF41135`. A 32-bit Direct3D9 adapter loop reproduced the same fault outside the game. The crash dump shows `CS=0107` (32-bit mode) inside Wine's 64-bit transition routine. This makes a RIP-relative load use a small absolute address instead.

The bundled Wine Staging 11.21 `wow64cpu.dll` used far jumps for both mode transitions. This patch applies the instruction sequence from CodeWeavers workaround 20760: a far call into 64-bit code, removal of its eight-byte return frame, and a far return to 32-bit code. Both syscall and Unix-call paths receive the change. The full-context `iretq` path remains in place.

The patch checks the original file's SHA-256 and the exact bytes at every edit location. Small leaf stubs occupy verified padding in the executable section. It keeps Wine's exported functions, imports, original transition-gate addresses, and context layout compatible. `provenance.json` pins the original Wine source and the published workaround source. `COPYING.LIB` accompanies the Wine source snapshots.

To rebuild the patched DLL from this directory:

```
python3 patch_wow64cpu.py wow64cpu-original.dll wow64cpu.dll
```

The expected output SHA-256 is `b6d99d573fd94f714545542d39ff0bc9124c186bfd8fc552d819326eb8e93214`.

The launcher copies the bundled CPU bridge into existing Wine profiles before starting the game. It launches once and reports the exit status. The earlier crash-triggered restart loop has been removed.

## Verification

`validation.json` records the original-versus-patched results and the actual game session. `Tests` contains small 32-bit reproducers, their output, and original-runtime crash dumps. The context probe repeatedly suspends a worker, reads its CPU context, and resumes it while that worker initializes Direct3D9 adapters. The exception probe exercises handled exceptions alongside the same graphics work. Each run uses a separate test profile from the user's game profile.

To compile a probe with LLVM-MinGW, use `i686-w64-mingw32-clang -O2 context_probe.c -ld3d9 -o context-probe.exe` (substitute the source name for the other probes). Each accepts a duration in seconds and a Windows report-file path. Launch it with the bundled Wine engine and an isolated `WINEPREFIX`.

The signed release contains the production display adapter. The game verification used a separate adapter build that recorded frame heartbeats, movie connection results, and a few frames. It automatically started Player 1 once; aiming, shooting, and reloading were controlled by the user. The diagnostic additions are excluded from the release.

These checks target the reproduced transition fault. They do not constitute a complete campaign playthrough.

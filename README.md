# SimGolf RE

Reverse engineering of **Sid Meier's SimGolf** (Firaxis/Maxis/EA, 2002, v1.03) on Windows 11.

This repository contains **no game files**: no executables, DLLs, data, screenshots or ISO images. You need
your own copy of the game (the original CD/ISO). Everything game-derived is regenerated locally from it.

## What is here

- **Portable runtime**: extract the CD without running the installer, remove SafeDisc from your own copy,
  and run windowed on Windows 11 with no registry entries (`re/analysis/PORTABLE_RUNTIME.md`).
- **Shim** (`shim/`): a `winmm.dll` proxy that fixes Win11 compatibility (windowed mode, a Bink crash),
  applies signature-checked patches (`patches/`), and hosts reimplementations (`SG_HOOK`).
- **Matching decompilation** (`re/match/`): C++ that recompiles with the original compiler
  (Visual C++ 6, cl 12.00.8168) to the game's instructions, checked by `re/tools/match.py`.
- **Verification tooling**: Frida A/B diffs, record/replay of real calls in Unicorn, INT3 coverage census
  over canonical scenarios, a three-axis scoreboard and a C0..C4 confidence gate (`re/CONFIDENCE.md`).
- **Test suite** (`tests/`, pytest): boots the real game windowed and drives it without touching the mouse.

## Setup (short)

1. Put your ISO in the repo root as `SM_SIMGOLF.iso` and follow `re/analysis/PORTABLE_RUNTIME.md`
   (7-Zip + unshield extraction, SafeDisc removal of your copy, `shim\build.bat`).
2. Optional, for matching: extract `VC98\` from your own Visual Studio 6 disc to `tools/vc6/vc98`.
3. Regenerate test references from your copy: `py -3.12 re/tools/capture_goldens.py`.
4. `py -3.12 -m pytest -m static`, then `-m runtime`.

Tool requirements: Python 3.12 (frida, pefile, capstone, unicorn, pywinauto, pillow, numpy, pytest),
MSVC Build Tools 2022 (x86), Ghidra 12.

## Status

See `ROADMAP.md`. The working conventions for contributors (and for Claude Code) are in `CLAUDE.md`.

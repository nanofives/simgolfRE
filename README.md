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
2. Optional, for matching: extract `VC98\` from your own Visual Studio 6 disc to `tools/vc6/vc98`, and copy
   `mspdb60.dll`, `msobj10.dll`, `msdis110.dll`, `msenc10.dll`, `rcdll.dll` from the disc's `Common\MSDev98\bin`
   into `tools/vc6/vc98/bin` (cl.exe does not start without them).
3. Register the compatibility layer for the exe (per user, no admin): value `~ DWM8And16BitMitigation` named by the
   full path of `original\golf_clean.exe` under `HKCU\Software\Microsoft\Windows NT\CurrentVersion\AppCompatFlags\Layers`.
   Without it a plain launch exits with code 0 a few seconds after the window opens (`game.py` sets `__COMPAT_LAYER`
   itself, so only `Play SimGolf.bat` and test_plain_launch_without_frida depend on it).
4. Regenerate test references from your copy: `py -3.12 re/tools/capture_goldens.py`.
5. `py -3.12 -m pytest -m static`, then `-m runtime`.

Tool requirements: Python 3.12 (frida 17.22.1, pefile, capstone, unicorn, pywinauto, pillow, numpy, pytest),
MSVC Build Tools 2022 (x86), Ghidra 12. Pin frida: 17.22.2 and later return garbage for x87 double returns of
thiscall NativeFunctions (Random_next RED in tests/test_diff_tool.py; 17.9.11 to 17.22.1 GREEN, bisected 2026-10-12).
MCP servers (`.mcp.json`): Ghidra 12.0.3 at `..\TD5RE\ghidra\ghidra_12.0.3_PUBLIC` (the stock GitHub release zip) with
`headless-mcp` beside it (`pip install -e .`, pulls pyghidra); frida MCP from
`pip install git+https://github.com/0xhackerfren/frida-game-hacking-mcp "mcp<2" "frida==17.22.1"` (it imports
`mcp.server.fastmcp`, which mcp 2.x removed).

## Status

See `ROADMAP.md`. The working conventions for contributors (and for Claude Code) are in `CLAUDE.md`.

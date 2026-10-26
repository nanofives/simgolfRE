# S5.1 merge lines

The step's own files (gditext.cpp / .h refactor, fttext.cpp / .h, test_fttext.py, widths_check.py) are already in the shared
tree. The integrator applies the two shared-file changes below; both are proven in the mirror (`log/roadmap/S5/1_mirror.py`
applies exactly these two edits to a HEAD export, builds port/build/S5_1, S5_1_w32 and S5_1_noft).

## 1. port/CMakeLists.txt
Append the whole of `log/roadmap/S5/1_cmake_block.txt` at the END of the file (after the portsim_runner32 block: it adds
POST_BUILD copies to portsim_c, portsim_runner32, portsim_layout_test and portsim_core_test, so those targets must exist
above it; each is guarded with `if(TARGET ...)`).

What it does: option PORTSIM_FREETYPE (default ON); `find_package(freetype CONFIG QUIET)` with the install passed in from
outside, nothing hard-coded: PORTSIM_FREETYPE_ROOT (cache, default `$ENV{SG_FREETYPE_ROOT}`) = an install prefix or a vcpkg
`installed` directory (the triplet x64-windows / x86-windows is picked by the architecture), or the caller's own
CMAKE_TOOLCHAIN_FILE (vcpkg.cmake), CMAKE_PREFIX_PATH or freetype_DIR. Found: portsim links Freetype::Freetype (PRIVATE),
defines PORTSIM_HAVE_FREETYPE=1, and a shared FreeType's DLL is copied next to the targets above. Not found (no root given,
as on a host without FreeType) or `-DPORTSIM_FREETYPE=OFF`: status line `portsim: no FreeType (module fttext throws
MissingResource)` and everything builds. The GDI provider stays the port's default either way (fttext is opt-in, through
`fttext.attach`).

To link it in the integrator's builds on LIVING-PC: set `SG_FREETYPE_ROOT=C:\Users\maria\vcpkg\installed` in the environment
port/test/build.py configures from, or pass `-DPORTSIM_FREETYPE_ROOT=C:/Users/maria/vcpkg/installed` once per build dir (the
value is cached). Without that, those builds stay FreeType-free, as on other machines. Proven in the mirror with fresh caches
(`1_build_all_fresh.log`): x64 and Win32 given the root find 2.14.3 in vcpkg's x64-windows / x86-windows and copy
freetype.dll next to portsim_c.dll and portsim_runner32.exe; the third build, given no root, reports no FreeType and builds.

LICENSE: FreeType is used under the FreeType License (FTL; the other choice, GPL-2.0-or-later, is not taken). The FTL
requires the documentation of a product that uses it to carry a credit, for example: "Portions of this software are
copyright (c) 2026 The FreeType Project (www.freetype.org). All rights reserved." (the full text is in vcpkg's
installed/<triplet>/share/freetype/copyright). When the port ships a binary that links FreeType, its README / about text must
carry that line.

## 2. port/src/sim/modules.inc
Append one line at the end:

```
SG_MODULE(fttext)
```

Then touch port/src/registry.cpp (MSBuild does not rebuild it when only modules.inc changes).

## 3. Notes for the other S5 steps (no merge action)
- S5.2 / textlab.cpp: NO change needed (no one-line edit: GdiRecorder's base GdiTextMetrics is kept). It compiles unchanged against the refactored gditext.h (`1_mirror.py --with-textlab --build noft`,
  `1_build_textlab_check.log`). GdiTextMetrics keeps its default constructor and a virtual, non-final draw_glyphs;
  gditext's registered entries (Text_put, drawMarkupRun, measureTextWidth, gditext.fontWidth / fontMetrics / createFont /
  textPut) now look the provider up as `MarkupTextMetrics` (the shared base), which GdiRecorder still is. With an
  FtTextMetrics attached, textlab.recordStart takes its WrapRecorder branch, which is not a MarkupTextMetrics, so those
  entries would throw MissingResource{"Text_put"} while it records: an FtRecorder deriving from FtTextMetrics (as GdiRecorder
  derives from GdiTextMetrics) avoids it.
- S5.3: FtTextMetrics::draw_glyphs returns false (fttext.cpp); faces, sizes (FT_Set_Pixel_Sizes(ppem, ppem)) and the v35
  library are in FtTextMetrics::Impl (Font::ft is the sized FT_Face per FontRef, Font::synthetic_bold). GDI's advances do
  not change with lfQuality 0 / 3 / 4 / 5 / 6 (`1_quality_probe.log`), only the raster does (S5.2's U-1510).

## 4. Gates measured in the mirror (HEAD 4bfefc2a + this step)
- segment gate x64: 205 / 205 PASS, 0 digests differ from log/p5/segs_x64_loc.tsv and from log/roadmap/S0/1_segs_x64.tsv
  (`1_segs_x64.tsv`, `1_segs_x64_compare.log`).
- boot gate: HEAD's 61 scenarios 58 PASS / 2 KNOWN / 1 NO_FRAMES, 0 rows differ from S0 (`1_boot_compare.log`).
- test_gditext.py 22 passed 1 skipped, and 23 passed with SG_EMU_ORACLE=1; test_fttext.py 22 passed.

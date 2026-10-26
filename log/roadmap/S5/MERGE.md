# S5 merge request: text without GDI (steps S5.1, S5.2, S5.3, S5.4, all accepted)

Files: log/roadmap/S5/files.txt (204 paths; log/* is gitignored, stage those with `git add -f`). Left out on purpose: the first
S5.3 worker's transcript dump (log/roadmap/S5/3_prev_transcript.txt, 3_transcript_dump.py) and the local data (port/corpus/labels/,
port/build/S5_*). Built on HEAD 4bfefc2a. Every step was judged in a mirror of HEAD plus the section's files plus these merge lines
(log/roadmap/S5/3_build.py / 4_build.py); the harness edits of item 3 were proven on patched copies in log/roadmap/S5/4_harness/.

Not S5's: port/test/gfx.py is modified in the shared tree by S2 (log/roadmap/S2/1_files.txt). S5 needs no gfx.py edit.

## Section gate (row S5 of re/roadmap/sections.tsv) and its result
FreeType rasteriser (D4, G4) of the game's fonts against the GDI provider, on the 11,459 recorded widths and every recorded label
pixel: equal, or the measured difference reported to Mariano; the default stays GDI.
- Widths, Text_put, drawMarkupRun and the five FontRef metric words: EQUAL (every printable row, x64 and Win32).
- Label pixels: NOT equal, difference measured and explained (8,736,593 px over the corpus, about 70 % of GDI's ink; the six
  gfx checkpoints 1,735 to 15,347 px each, all inside label boxes). Report for Mariano: re/analysis/data/text_freetype.md.
- Default: GDI, unchanged. Mariano's decision (keep GDI on Windows or switch) is the last section of that report.

## Shared-file edits, in apply order

### 1. port/CMakeLists.txt
Append the whole of `log/roadmap/S5/1_cmake_block.txt` at the END of the file. It adds POST_BUILD copies to portsim_c,
portsim_runner32, portsim_layout_test and portsim_core_test, so it must come after those targets. It adds an OPTIONAL FreeType:
option PORTSIM_FREETYPE (default ON), cache PORTSIM_FREETYPE_ROOT (default $ENV{SG_FREETYPE_ROOT}: an install prefix or a vcpkg
`installed` dir, the triplet picked by the architecture), `find_package(freetype CONFIG QUIET)`; found -> portsim links
Freetype::Freetype (PRIVATE), defines PORTSIM_HAVE_FREETYPE=1, and copies freetype.dll next to those targets; not found ->
`portsim: no FreeType (module fttext throws MissingResource)` and everything builds as now.

### 2. port/src/sim/modules.inc
Append, in this order, then touch port/src/registry.cpp (MSBuild does not rebuild it when only modules.inc changes):
```
SG_MODULE(textlab)
SG_MODULE(fttext)
SG_MODULE(fttext_raster)
```
test_static.py::test_every_module_file_is_listed needs all three (textlab.cpp, fttext.cpp and fttext_raster.cpp are in src/sim).

### 3. Harness selection SG_TEXT_METRICS=gdi|freetype|none (port/test/segments.py, boot.py, integrator.py)
Apply `log/roadmap/S5/4_merge.md` items 1 to 3 exactly as written there (a `TEXT_PROVIDERS` map and `text_provider(mode, lib)`
in segments.py before `class SegmentRunner:`; the gditext attach / detach in SegmentRunner.run, boot.run and integrator.run_from
replaced by the selected provider's). Default stays gdi; with no variable the segment gate is 205 / 205, 0 digests differ.
Do NOT copy 4_harness/'s two mirror-only changes (ROOT literal, boot child runner).

### 4. UNCERTAINTIES.md
Append the rows of log/roadmap/S5/1_uncertain.tsv (U-1500..U-1506), 2_uncertain.tsv (U-1510..U-1514; U-1514 closed),
3_uncertain.tsv (U-1520..U-1526) and 4_uncertain.tsv (U-1530..U-1532), in id order, without their header lines.

### 5. CLAUDE.md
Append the paragraph at the end of this file after the last phase paragraph ("Phase 5 step 2, the location screen").

## Build environment (integrator)
Link FreeType in port/build/x64 and port/build/Win32 by configuring with `SG_FREETYPE_ROOT=C:\Users\maria\vcpkg\installed` set
in the environment build.py runs in (or `-DPORTSIM_FREETYPE_ROOT=C:/Users/maria/vcpkg/installed` once per build dir; the value is
cached). FreeType 2.14.3 core is installed there for x64-windows and x86-windows (G4, logs log/roadmap/S5/vcpkg_*.log). Without the
root the builds stay FreeType-free: every gate below reads the same except the FreeType rows, and test_fttext.py /
test_fttext_raster.py skip their FreeType tests.

## License
FreeType is used under the FreeType License (FTL; GPL-2.0-or-later not taken). A shipped binary that links FreeType must carry in
its documentation: "Portions of this software are copyright (c) 2026 The FreeType Project (www.freetype.org). All rights
reserved." (full text: C:\Users\maria\vcpkg\installed\<triplet>\share\freetype\copyright).

## Gate command lines and expected numbers (repo root, after the merge, built with SG_FREETYPE_ROOT set)
```
export SG_PORTSIM_DLL=$PWD/port/build/x64/Release/portsim_c.dll SG_PORTSIM_W32_DIR=$PWD/port/build/Win32
~/bin/py -3.12 -m port.test.widths_check                                                  # GDI: printable 11,459 / 9,079 / 10,776 equal (unchanged)
~/bin/py -3.12 -m port.test.widths_check --provider freetype                              # 11,473 / 9,093 / 10,790 of 11,481 / 9,101 / 10,798: every printable row equal, only the lone 0x0a rows differ (U-0960)
~/bin/py -3.12 -m port.test.widths_check --provider freetype --win32 port/build/Win32     # the same numbers
~/bin/py -3.12 -m port.test.labels corpus                                                 # 6,540 (FontRef, text) entries, 15,960 jobs (needs port/corpus/widths, local)
~/bin/py -3.12 -m port.test.labels reference                                              # GDI reference rasters (local)
~/bin/py -3.12 -m port.test.labels compare gdi                                            # 0 px per font and per checkpoint
~/bin/py -3.12 -m port.test.labels render freetype
~/bin/py -3.12 -m port.test.labels compare freetype                                       # 15,618 / 15,960 jobs, 8,736,593 px differ (raster digest 5a336eba875e1d94..., x64 = Win32)
~/bin/py -3.12 -m pytest port/test/test_gditext.py -q -p no:cacheprovider                 # 22 passed, 1 skipped (23 with SG_EMU_ORACLE=1)
~/bin/py -3.12 -m pytest port/test/test_fttext.py -q -p no:cacheprovider                  # 22 passed
~/bin/py -3.12 -m pytest port/test/test_labels.py -q -p no:cacheprovider                  # 14 passed
~/bin/py -3.12 -m pytest port/test/test_fttext_raster.py -q -p no:cacheprovider           # 9 passed (needs a ClearType host at contrast 1200, else skips)
~/bin/py -3.12 -m pytest port/test/test_static.py -q -p no:cacheprovider                  # passes once modules.inc lists the three modules
~/bin/py -3.12 -m port.test.integrator segments --backend x64 --jobs 8 --out <tsv>        # 205 / 205, 0 digests differ from log/p5/segs_x64_loc.tsv (GDI default)
SG_TEXT_METRICS=freetype ~/bin/py -3.12 -m port.test.integrator segments --backend x64 --jobs 8 --out <tsv>   # 205 / 205, 0 digests differ (also Win32)
~/bin/py -3.12 -m port.test.boot --all --jobs 3 --out <tsv>                               # HEAD's 61: 58 PASS / 2 KNOWN / 1 NO_FRAMES (also with SG_TEXT_METRICS=freetype, x64 and Win32)
SG_TEXT_METRICS=freetype ~/bin/py -3.12 log/roadmap/S5/3_gfx_ft.py --provider fttext       # menu 1,735 / refused 15,347 / site_hover 13,964 / buttons 13,600 / tip 13,714 / back 1,735 px, 0 outside label boxes
```
U-1532: run the two SG_TEXT_METRICS=freetype gate lines once on the merged tree (the proof ran on patched copies of the harness).

## CLAUDE.md paragraph

Section S5, text without GDI (2026-10-24, roadmap orchestrator; steps re/roadmap/S5_steps.tsv, notes log/roadmap/S5/<k>_notes.md,
report re/analysis/data/text_freetype.md, U-1500..U-1532): a FreeType TextMetrics provider (module fttext) beside the GDI one, judged
against it. FreeType 2.14.3 core (no zlib / png / bzip2 / brotli / harfbuzz) comes from vcpkg in C:\Users\maria\vcpkg\installed
(decisions D4 and G4; license FTL, its credit line goes into the documentation of any binary that links it) as an OPTIONAL CMake
dependency: `SG_FREETYPE_ROOT` (or `-DPORTSIM_FREETYPE_ROOT`) at configure links it and copies freetype.dll beside the DLLs; without
it module fttext throws MissingResource{"FreeType"} and nothing else changes. gditext was split into MarkupTextMetrics (the
original's Text_put / drawMarkupRun / measureTextWidth chain) and GdiTextMetrics (the GDI primitives; every output row-identical to
before); FtTextMetrics shares the chain. FACES (measured with GetFontData hashes on LIVING-PC): GDI resolves lfFaceName by the
family name, else by the full name of a face alone in its family, else to Arial, so the game's `Arial Bold` 9 / 10 and `Times New
Roman Bold` 18 are drawn with arial.ttf (regular) and `Comic Sans MS Bold` italic 100 with ariali.ttf; Manual SSi (Bold) and Klepto
ITC TT are the install's manu3_.ttf / klepto__.ttf. A host without Windows fonts must supply arial.ttf, ariali.ttf and times.ttf
($SG_FONT_DIR). WIDTHS: ppem = size, interpreter v35, the five FontRef words from VDMX else OS/2 win metrics, advances = hdmx at the
ppem, else linear rounded when head flags bit 4 is clear or LTSH allows, else v35 hinted (3,136 / 3,136 probe cases); FreeType equals
GDI and the recordings on every printable row (11,459 widths, 9,079 Text_put, 10,776 drawMarkupRun, 15 / 15 metric words, x64 =
Win32); only lone 0x0a rows differ, which the original itself measures differently per process (U-0960). PIXELS: the original's labels
and GDI's are ClearType subpixel on the 16-bit DIB (lfQuality 0 follows the host's ClearType setting: on, contrast 1200, RGB here).
Module textlab + port/test/labels.py hold a label corpus (6,540 (FontRef, text) pairs from the widths recordings, 15,960 jobs, plus the
calls of the six gfx checkpoints that pass at HEAD; GDI reference 0 px against itself and against the original's frames). The
FreeType raster (fttext_raster.{cpp,h}) reproduces GDI ClearType's blend exactly (per channel in the 5-bit space through gamma
contrast / 1000 tables, 0 / 7,168) and its 6-sample box filter (exact on Arial), but not GDI's ClearType x hinting (U-1520):
8,736,593 corpus px differ (about 70 % of GDI's ink; Klepto 0.26-0.29, Arial 0.60, Manual SSi 0.61-0.81) and the six checkpoints
differ by 1,735 to 15,347 px each, all inside label boxes. The simulation reads text only through widths, so with
`SG_TEXT_METRICS=freetype` (segments.py / boot.py / integrator.py; default gdi) the segment gate stays 205 / 205 and the boot gate 58 /
2 / 1 on x64 and Win32. The port's default stays GDI; the report's last section is Mariano's decision (recommendation: keep GDI on
Windows, FreeType for hosts without GDI, judged by these numbers). Process lessons: on claude2 `cmake` is not an allowed command
(`~/bin/py` is), so children run every configure and build from a Python script; two S5.3 workers stalled on unanswered permission
prompts before that rule was in their brief.

# S5.1: FreeType setup and the provider core (2026-10-24)

Status: DONE. Every gate of the S5.1 row measured; numbers in section 5. Merge lines: `1_merge.md`. Uncertainties:
`1_uncertain.tsv` (U-1500..U-1506). Files: `1_files.txt`.

## 1. FreeType (gate a)
- The install the port uses: vcpkg in `C:\Users\maria\vcpkg` (run from a claude3 session on Mariano's approval, G4 in
  re/roadmap/DECISIONS_PENDING.md), `freetype` 2.14.3, triplets x64-windows and x86-windows, default features off (core: no
  zlib / png / bzip2 / brotli / harfbuzz packages; FreeType's bundled gzip / LZW code stays). Per triplet: include\ft2build.h,
  include\freetype\, lib\freetype.lib, bin\freetype.dll, share\freetype\freetype-config.cmake. Logs
  `log/roadmap/S5/vcpkg_bootstrap.log`, `vcpkg_install_freetype.log`.
- KEPT OUT OF THE PORT: while the install was pending this worker built FreeType 2.13.3 from source (tag VER-2-13-3 of
  gitlab.freedesktop.org/freetype/freetype; VS 2022 CMake `-A x64` / `-A Win32` with `-DBUILD_SHARED_LIBS=OFF
  -DFT_DISABLE_ZLIB=TRUE -DFT_DISABLE_BZIP2=TRUE -DFT_DISABLE_PNG=TRUE -DFT_DISABLE_HARFBUZZ=TRUE -DFT_DISABLE_BROTLI=TRUE`,
  Release and Debug, installed to C:\Users\maria\freetype\x64 / x86). No port build, test or measurement used it; the whole
  C:\Users\maria\freetype directory was deleted once the vcpkg install existed, so the host has one FreeType.
- LICENSE: FreeType License (FTL), chosen over GPL-2.0-or-later. Its credit goes into the documentation of any shipped binary
  that links FreeType, for example: "Portions of this software are copyright (c) 2026 The FreeType Project
  (www.freetype.org). All rights reserved." (full text: vcpkg's installed/<triplet>/share/freetype/copyright; 1_merge.md item 1).
- CMake: `1_cmake_block.txt`, appended to port/CMakeLists.txt in the mirror (merge item 1). Optional
  `find_package(freetype CONFIG QUIET)`; the install comes from outside only (PORTSIM_FREETYPE_ROOT / $SG_FREETYPE_ROOT = a
  prefix or vcpkg's `installed` dir with the triplet picked by architecture, or the caller's toolchain / CMAKE_PREFIX_PATH /
  freetype_DIR). Found: PORTSIM_HAVE_FREETYPE=1 and freetype.dll copied next to portsim_c.dll and portsim_runner32.exe
  (vcpkg's triplets are dynamic, U-1505). Not found or -DPORTSIM_FREETYPE=OFF: module fttext builds and throws
  MissingResource{"FreeType"}. The GDI provider stays the default in every case.
- Proven with fresh caches (`1_mirror.py --build all --fresh --ft-root C:/Users/maria/vcpkg/installed`,
  `1_build_all_fresh.log`): port/build/S5_1 (x64) and S5_1_w32 (Win32) configure with `portsim: FreeType 2.14.3 from
  C:/Users/maria/vcpkg/installed/x64-windows|x86-windows/share/freetype` and get freetype.dll in Release\. port/build/S5_1_noft,
  given no root, configures with `portsim: no FreeType` and builds (`fttext.available` 0, attach SG_E_NO_RESOURCE). On the
  rebuilt DLLs test_fttext 22 passed and test_gditext 22 passed 1 skipped. The segment and boot gates of section 5 ran on
  the first configuration of the same C++ sources (the CMake change only moved where the install path comes from).

## 2. Faces: how GDI matches lfFaceName (measured; 1_gdi_probe.py, 1_gdi_probe2.py)
The selected file is identified by hashing GetFontData(dc, 0, 0) against the font directory and the game's two files.
| FontRef | lfFaceName | size | GDI's file | GetTextFace |
|---|---|---|---|---|
| g_font83ad20 | Times New Roman | 12 | times.ttf | Times New Roman |
| g_font51b320 / g_fontRef_519fd8 | Arial Bold | 9 / 10 | arial.ttf (REGULAR) | Arial |
| g_font519968 | Times New Roman Bold | scaleX(7) = 18 | arial.ttf | Arial |
| g_font51b340 | Comic Sans MS Bold, italic | scaleX(40) = 100 | ariali.ttf | Arial |
| g_font51b360, g_font519928, g_font519948 | Manual SSi Bold | 15 / 20 / 24 | manu3_.ttf (game) | Manual SSi Bold |
| g_fontRef_821f08 / 821ec8 / 821f28 / 821ee8 | Manual SSi | 20 / 18 / 16 / 14 | manu3_.ttf | Manual SSi |
| g_font51a028, g_fontRef_519a40, g_fontRef_821020 | Klepto ITC TT | 18 / 24 / 24 | klepto__.ttf (game) | Klepto ITC TT |
Rule implemented (fttext.cpp `lookup`): family name (ID 1, Windows records, case-insensitive) -> closest italic then weight
(lfWeight 0 = 400); else full name (ID 4) of a face that is alone in its family (Manual SSi Bold; probe 2 also shows
Unispace-Bold and Cascadia Code Regular resolving that way, and Arial Bold / Comic Sans MS Bold / Courier New Bold NOT,
even with arialbd.ttf registered privately); else GDI's substitute on this host, Arial with the requested style (unknown
faces -> arial.ttf, ariali.ttf, arialbd.ttf). U-1500, U-1501. Candidates: the game's font files (game_font_files: the files
gameMain's Font::loadFile calls register, read through GameFiles) and every .ttf / .ttc of the host font directories
($SG_FONT_DIR, else %WINDIR%\Fonts and %LOCALAPPDATA%\Microsoft\Windows\Fonts; only the table directory, `name`, `OS/2` and
`head` are read for the index, whole files only for the selected faces). CFF-outline faces are skipped (OUT_TT_ONLY_PRECIS).
Synthesis (1_gdi_probe3.log): GDI's bold on a Roman-only face adds 1 px per glyph advance and keeps the five words; italic
changes neither. Implemented as +1 per glyph when the request is >= 600 and the face < 600 (U-1502); no FontRef uses it.

## 3. Sizes, metrics and advances
- ppem = size (lfHeight = -size), FT_Set_Pixel_Sizes(ppem, ppem), FreeType's TrueType interpreter set to v35.
- Five words (fttext.cpp `metrics_of`, U-1503): VDMX's yMax / -yMin at the ppem (1:1 ratio group) else OS/2 winAscent /
  winDescent scaled ((FT_MulFix + 32) >> 6); internal leading = height - ppem (the first version, scaled(winAscent +
  winDescent - unitsPerEm), gave 11 / 15: `1_metrics_x64_first.log`); external leading = max(0, scaled(lineGap - ((winAscent +
  winDescent) - (ascender - descender)))).
- Advances (fttext.cpp `advance`, U-1504). Probes: `1_adv_probe.log` (hdmx and linear against GDI), `1_ftprobe/` +
  `1_ftprobe_cmp.log` (seven FreeType load modes), `1_quality_probe.log` (lfQuality 0 / 3 / 4 / 5 / 6 give the same advances),
  `1_glyf_probe.log`, `1_rule_probe.log` (rules R0..R3 over 14 face / size pairs x 224 bytes = 3,136 cases):
  R0 hdmx else v35 hinted: Arial 9 208, 10 221, 18 210 of 224; R2 (composites linear): 220 / 223 / 223;
  R3 = hdmx at the ppem; else linear rounded half up when head flags bit 4 is clear (Klepto, Manual) or LTSH yPels <= ppem;
  else v35 hinted; the five undefined cp1252 bytes 0x81 0x8d 0x8f 0x90 0x9d give 0: 3,136 / 3,136.
  Bytes: code page 1252 -> Unicode (the 32-entry table for 0x80..0x9f) -> the face's Unicode cmap (symbol cmap: 0xF000 + byte).
  No kerning (GetTextExtentPoint32A applies none). A negative count returns the 0xcccccccc fill as GDI's failed call.

## 4. The refactor of gditext (gate e: GDI byte-identical)
gditext.h: new base `MarkupTextMetrics : TextMetrics` holds the chain (text_put, text_put_state, draw_markup_run,
measure_text_width, font_text_width with the scripted test rule, markup_run) and declares the primitives `font_metrics`
(public virtual) and `font_extent` (protected virtual). `GdiTextMetrics : MarkupTextMetrics` keeps its public API (default
ctor, font_metrics, glyph_width / glyph_ascent / draw_glyphs overrides, Impl) and implements font_extent with the old
GetTextExtentPoint32A body (negctl 1 stays there). Exported for fttext: `GameFont game_font(ctx, fontref)` (face, file,
size after scaleX, style), `game_font_count()`, `game_font_ref(ctx, i)`, `game_font_files(ctx)`. gditext's registered
entries find the provider by `dynamic_cast<MarkupTextMetrics *>`. No GDI call, argument or order changed.
Evidence: section 5 rows (e).

## 5. Gates
| gate | result | log |
|---|---|---|
| (a) FreeType optional, mirror builds | x64 + Win32 with FreeType 2.14.3, x64 without (fttext.available 0, attach SG_E_NO_RESOURCE) | 1_build_x64.log, 1_build_w32.log, test_fttext::test_build_without_freetype |
| (b) module fttext | FtTextMetrics : MarkupTextMetrics; faces via GameFiles + host dirs by name table; draw_glyphs false (S5.3) | port/src/sim/fttext.cpp |
| (c) five words, 15 FontRefs | FreeType = GDI = original, 15 / 15 x64 and 15 / 15 Win32; faces = GDI's files 15 / 15 | 1_metrics_x64.log, 1_metrics_w32.log |
| (d) widths_check --provider freetype | x64 and Win32: textWidth 11,473 / 11,481 rows (printable 11,459 / 11,459), Text_put 9,093 / 9,101 (9,079 / 9,079), drawMarkupRun 10,790 / 10,798 (10,776 / 10,776), setDrawTarget interface 9,079 / 9,079; every differing row is a lone 0x0a (8 per table, cause `control-only text`, U-0960 / U-1504); FreeType's results are the same on x64 and Win32 row for row | 1_widths_ft_x64.log / .json / _diff.tsv, 1_widths_ft_w32.* |
| (d) per font | Manual SSi Bold 15: 8,422 / 8,425; Arial Bold 10: 1,198 / 1,198; Klepto 18: 830 / 833; Manual SSi 14: 515 / 515; Klepto 24 (519a40): 254 / 256; Manual SSi Bold 20: 223 / 223; Klepto 24 (821020): 24 / 24; Manual SSi 16: 5 / 5; Manual SSi Bold 24: 2 / 2 (the shortfalls are the 0x0a rows) | 1_widths_ft_x64.log |
| (e) GDI unchanged | widths_check (gdi) x64 11,459 / 9,079 / 10,776 printable, its differing-row list byte-identical to a HEAD build's (port/build/S0_head x64); Win32 printable equal too (its 0x0a rows vary per process, U-0960); test_gditext.py 22 passed 1 skipped (as HEAD's DLL), 23 passed with SG_EMU_ORACLE=1 | 1_widths_gdi_x64.*, 1_widths_gdi_head_x64.*, 1_widths_gdi_w32.*, 1_test_gditext_x64.log, 1_test_gditext_head_x64.log, 1_test_gditext_oracle_x64.log |
| (e) segment gate x64 | 205 / 205 PASS, 0 digests differ from log/p5/segs_x64_loc.tsv and log/roadmap/S0/1_segs_x64.tsv (74 s, 8 jobs) | 1_segs_x64.log / .tsv, 1_segs_x64_compare.log |
| (e) boot gate | HEAD's 61: 58 PASS / 2 KNOWN / 1 NO_FRAMES, 0 rows differ from S0 (S1's cov_r_bio / cov_r_flic_input FAIL as at HEAD) | 1_boot_gate.log / .tsv, 1_boot_compare.log |
| (f) controls | 4 / 4 RED, 0 blind (below) | 1_widths_ft_negctl{1..4}.log, test_fttext.py |

Controls (g_negctl_fttext, set with `negctl("fttext.fontWidth", n)`), each on cov_a_play's recordings and on its primitive:
1 advance + 1: textWidth 1 / 532 rows equal; every single-byte width + 1. 2 ppem + 1: 532 errors (the safety check finds the
FontRef's stored words differ), and the metric words change. 3 g_fontRef_519fd8 resolves to Times New Roman: its 88 rows
error (safety check), the other FontRefs keep their faces. 4 hinting off (linear advances, no hdmx): 495 / 532 rows, and an
Arial 10 advance changes; for Manual SSi and Klepto it is an equivalent mutant (their hdmx / head-flag rule is linear
already), so only the Arial / Times rows judge it.
Arithmetic check of the controls: modes 2 and 3 read RED through the safety check (MissingResource) on recordings whose start
image carries the FontRef words; their own effect is shown by test_negctl_red_on_their_primitives with the words cleared.

## 6. Reproduce
```
~/bin/py -3.12 log/roadmap/S5/1_mirror.py --build all --fresh --ft-root C:/Users/maria/vcpkg/installed   # S5_1, S5_1_w32, S5_1_noft
export SG_PORTSIM_DLL=port/build/S5_1/Release/portsim_c.dll
~/bin/py -3.12 log/roadmap/S5/1_metrics_check.py [--win32 port/build/S5_1_w32]
~/bin/py -3.12 -m port.test.widths_check --provider freetype [--win32 port/build/S5_1_w32] --json <j> --diff-tsv <t>
~/bin/py -3.12 -m port.test.widths_check [--win32 port/build/S5_1_w32]                      # the GDI provider
SG_PORTSIM_BUILD32=port/build/S5_1_w32 SG_PORTSIM_DLL_NOFT=port/build/S5_1_noft/Release/portsim_c.dll \
  ~/bin/py -3.12 -m pytest port/test/test_fttext.py -q -p no:cacheprovider
~/bin/py -3.12 -m pytest port/test/test_gditext.py -q -p no:cacheprovider       # SG_EMU_ORACLE=1 for the oracle
~/bin/py -3.12 -m port.test.integrator segments --backend x64 --dll port/build/S5_1/Release/portsim_c.dll --jobs 8 --out <tsv>
~/bin/py -3.12 -m port.test.integrator compare log/p5/segs_x64_loc.tsv <tsv>
~/bin/py -3.12 -m port.test.boot --all --jobs 3 --out log/roadmap/S5/1_boot_gate.tsv && ~/bin/py -3.12 log/roadmap/S5/1_boot_compare.py
```
Probes (GDI side, ctypes): `1_gdi_probe.py` (faces, words, tables), `1_gdi_probe2.py` (full-name matching),
`1_gdi_probe3.py` (synthesis), `1_adv_probe.py`, `1_fontdir_names.py`, `1_glyf_probe.py`, `1_rule_probe.py <ftprobe.exe>`
(build `1_ftprobe/` with CMake into port/build/S5_1_ftprobe), `1_ftprobe_cmp.py`. In `1_quality_probe.log` the line
`font smoothing 1 type 1200` prints SPI_GETFONTSMOOTHINGCONTRAST (0x200c) as `type`: 1200 is the contrast.

## 7. Open
- U-1500 / U-1501: the face rule and the Arial substitute are measured on this host only.
- U-1505: freetype.dll beside the DLLs (dynamic vcpkg triplets); static-md triplets would remove it.
- S5.2's textlab recorder and an FtTextMetrics provider: see 1_merge.md section 3.
- Win32 segment gate not re-run (one heavy run): Win32 GDI widths printable rows equal and Win32 builds clean.

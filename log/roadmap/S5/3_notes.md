# S5.3 notes: the FreeType raster against GDI ClearType (third worker, 2026-10-24)

Status: DONE as a measured model (all gates measured on the final build, 2026-10-24). FtTextMetrics::draw_glyphs draws with a FreeType raster that reproduces GDI ClearType's
blend exactly and its coverage approximately; the remaining difference is measured per font and per checkpoint, and its cause
(GDI's own ClearType x hinting, which FreeType has no mode for) is stated (U-1520). Continued from 3_handover.md. Uncertainties
`3_uncertain.tsv` (U-1520..U-1526), files `3_files.txt`, merge `3_merge.md` (one line: SG_MODULE(fttext_raster), plus S5.1 / S5.2's lines).

## 0. Builds (no bare cmake on claude2: everything through Python)
- `~/bin/py -3.12 log/roadmap/S5/3_build.py mirror --build x64|Win32|noft|both`: HEAD's port/ + re/model/include (git archive)
  into port/build/S5_3_src, plus S5.1's gditext / fttext, S5.2's textlab and this step's files; mirror-only shared edits:
  modules.inc + `SG_MODULE(fttext)` + `SG_MODULE(textlab)` (registry.cpp touched), port/CMakeLists.txt + 1_cmake_block.txt.
  Builds port/build/S5_3 (x64), S5_3_w32 (Win32 + portsim_runner32), S5_3_noft (x64, -DPORTSIM_FREETYPE=OFF: compiles, the
  raster's no-FreeType branch included). FreeType root $SG_FREETYPE_ROOT, default C:/Users/maria/vcpkg/installed.
- `3_build.py probe`: log/roadmap/S5/3_probe (s5probe.dll, a C ABI over FreeType for the Python fits; development only) into
  port/build/S5_3_probe.

## 1. Blend (exact)
Per channel on the 5-5-5 word: f = top 5 bits of the text colour, b = the surface's 5 bits, F = dec[f << 3], D = dec[b << 3],
m = D + floor(((F - D) round(level 65536 / 6) + 32768) / 65536), stored enc[m] >> 3; dec / enc = 8-bit gamma tables of gamma
contrast / 1000, rounded half up; level 0 keeps the word, level 6 stores f. Evidence: 3_blend_check.py 0 / 7,168 against the
first worker's GDI table 3_blend555.npy (den 1024 and up; den 6 gives 20); the C++ entry fttext_raster.blend5 equals that table
0 / 7,168 (3_blend_contrast.log) and test_fttext_raster.py::test_blend_equals_gdi draws with the GDI provider white on black
(7 distinct 5-bit values = the levels) and on all 1,024 grey (f, b) pairs: every channel of every pixel equals fttext_raster.blend5.
Contrasts 1000 / 1100 / 1300 / 1400 change 1,052 / 692 / 660 / 1,125 of the 7,168 entries, so the test discriminates.

## 2. Levels (box filter, exact where the coverage is)
Each subpixel's level 0..6 = set samples of a 6-sample box centred on it, the glyph scanned at 6 samples per pixel in x:
R [6p-2, 6p+4), G [6p, 6p+6), B [6p+2, 6p+8) (orientation RGB). Evidence: 3_edges.py decodes GDI's level maps back to
per-subpixel counts 0..2 under this model; the decode succeeds on 94 / 94 printable Arial Bold 10 glyphs (a different filter
would leave counts outside 0..2), and on only 13 / 94 Manual SSi Bold 15 and 12 / 94 Klepto 18 glyphs, whose levels are
averages over subrows (section 3) (3_decode_all.log). Overlap (3_overlap.py --combine): one TextOutA call = the glyphs' level maps SUMMED and clamped at 6,
then one blend: 0 differing levels on five overlap strings ('fjjff/\\', 'WaVAfjTo', 'AVWAfj' ...); the max rule differs.

## 3. y (hinting and smoothing)
FreeType's interpreter v40 hints the outline (FT_LOAD_TARGET_NORMAL); its rows equal GDI's on Arial Bold 10. Faces without a
gasp table (Manual SSi, Klepto ITC TT) show fractional horizontal edges in GDI (Manual SSi Bold 15 'H': crossbar levels 5 then
1 over two rows): the raster scans 6 subrows per pixel row there and takes round half up (count / 6) (U-1522: rule from the
gasp spec's GASP_SYMMETRIC_SMOOTHING bit, subrow count fitted; 4..16 give totals within 1 %).

## 4. x (the part GDI does not let FreeType match)
FreeType variants measured per glyph against GDI's level maps (3_fit.py, 3_fit2.py, 3_phase.py, 3_fity.py, 3_mix.py). Levels
differing over the 94 printable ASCII glyphs, outline hinted by v40 (its own x), each face at its own subrow count (Arial 1,
the others 6), white on black (3_mix_rules_v40.log columns zero_ownx / half_ownx / min_ownx; the adopted rule in bold):
| face | at the pen | centred in the advance | BW-leftmost-point shift |
|---|---|---|---|
| Arial Bold 10 (arial.ttf) | 3,637 | **2,722** | 4,640 |
| Manual SSi Bold 15 | 7,987 | 8,201 | **5,668** |
| Manual SSi 14 | 7,132 | 7,297 | **4,772** |
| Manual SSi Bold 20 | 11,404 | 11,377 | **8,784** |
| Klepto ITC TT 18 | **3,350** | 6,562 | 8,814 |
Other variants were worse (interpreter v35 for the drawn outline: Arial centred 4,769, Klepto 9,488, 3_fity.py --interp 35;
the unhinted x with v35 y: columns zero / half / min of the same log).
No single offset makes Arial's hinted glyphs exact (3_phase.py: 'H', 'o', 'A', 'n', 'm', 'e', 'a' have no exact offset); the
per-glyph best offset on Manual SSi varies by +/-1.5 px (3_place.py, 3_ink.py: centre sd 2.3 samples whatever the hinting
mode, x ppem 1..6 x). Adopted rule (U-1521, fitted on these three faces): head flags bit 4 set -> centred by (6 x compatible
advance - 6 x linear advance) / 2 samples; linear advances with an hdmx table -> shifted by 6 x the x displacement v35 hinting
gives the outline's leftmost point; otherwise the pen. With it, about 20 of Manual SSi Bold 15's glyphs still sit one pixel
right of the rule (3_mix_corr_51b360.log) and many glyphs differ in stem phase: GDI's ClearType x hinting (U-1520).

## 5. Implementation
- port/src/sim/fttext_raster.cpp + port/include/portsim/sim/fttext_raster.h (standalone over a sized FT_Face): glyph_levels
  (load, x 6 / y V transform, mono scan with the hinting's dropout flags, the box), accumulate (sum clamp 6), blend555, mix5,
  y_smoothing (gasp), set_interpreter; module `fttext_raster` with the test entry fttext_raster.blend5 (SG_MODULE line in
  3_merge.md: test_static wants every src/sim/*.cpp to be a listed module).
- port/src/sim/fttext.cpp (wiring only): FtTextMetrics::draw_glyphs; a second FT library at interpreter v40 with one raster
  face per font (same bytes and ppem as the metric face, which stays v35 for the advances); the glyph lookup factored out of
  advance() (code page 1252 -> cmap, unchanged results: widths_check equal to S5.1's); pens = the advances font_extent sums
  (synthetic bold +1 included); the five undefined bytes draw nothing (U-1524). Negative controls in
  g_negctl_fttext (set through `negctl("fttext.fontWidth", n)`): 5 glyph origin + 1 px, 6 colour red bit 3 flipped (one 5-bit
  step), 7 raster hinting off. fttext.h comment updated.
- No FtRecorder was needed: S5.2's checkpoint corpus is recorded with GDI, and compare replays it through textlab.draw.
- Not modelled (U-1525): GDI's synthetic emboldening (no FontRef uses it); synthetic italic is not needed (the style-2 font
  resolves to ariali.ttf).

## 6. Gates (mirror port/build/S5_3 / S5_3_w32, HEAD 4bfefc2a + S5.1 + S5.2 + S5.3 files)
| gate | result | log |
|---|---|---|
| (a) draw_glyphs | TextOutA model on the RGB555 surface: TA_BASELINE pen, colour, ClearType levels, y smoothing per gasp, compatible-width placement, one blend per run | port/src/sim/fttext*.cpp |
| (b) labels.py compare freetype | corpus 15,618 / 15,960 jobs, 8,736,593 px differ; x64 = Win32 per font; rasters byte-identical x64 / Win32 / second x64 run (digest 5a336eba875e1d94...) | 3_compare_ft_x64.log, 3_compare_ft_w32.log, 3_render_ft_*.log |
| (b) per font | px differ (share of GDI's ink px): Manual SSi Bold 15 7,339,388 (0.77), Manual SSi Bold 20 391,631 (0.70), Manual SSi 14 282,308 (0.71), Manual SSi 16 90,227 (0.81), Manual SSi Bold 24 22,359 (0.61), Arial Bold 10 176,256 (0.60), Klepto 24 (821020) 200,488 (0.29), Klepto 24 (519a40) 108,411 (0.26), Klepto 18 125,525 (0.28); 8,339,111 of the 8,736,593 differing px (95 %) differ by 4 or more 5-bit steps in some channel | 3_diffstats.tsv |
| (b) checkpoints (replayed calls vs the GDI run's words) | menu 3,369; refused / site_hover / buttons 20,138; tip 20,252; back 20,780 px; vs the original's frame (on-frame calls) 1,462 / 3,424 / 3,165 / 2,895 / 3,009 / 1,272 | 3_compare_ft_x64.log |
| (c) six gfx checkpoints booted with fttext attached (no GDI) | menu 1,735 px (label_continue 357, label_new 528, label_theme 190, label_champ 390, label_sandbox 270, buttons = their labels, logo 0, rest 0); refused 15,347; site_hover 13,964; buttons 13,600; tip 13,714; back 1,735; every differing px inside S5.2's label boxes (0 outside); boots PASS / NO_FRAMES as at HEAD; Win32 identical; the same runner with gditext gives 0 px on all six | 3_gfx_ft_x64.log / .json, 3_gfx_ft_w32.*, 3_gfx_gdi_x64.* |
| (d) controls | 3 / 3 RED: 5 origin + 1 px changes 15,626 jobs, corpus px vs GDI 8.74 M -> 13.34 M; 6 colour changes 15,737, -> 11.32 M; 7 hinting off changes 15,602, -> 10.46 M; every checkpoint changes under each. Blind subsets: texts of spaces only (8 per base / inverse config), sweep jobs whose colour equals the background (315; 189 under 6), 3 shadow jobs | 3_controls_x64.log, 3_controls.json |
| GDI unchanged | labels.py compare gdi 0 / 0 px; widths_check --provider freetype = S5.1's (11,473 / 9,093 / 10,790 of 11,481 / 9,101 / 10,798; only lone 0x0a rows differ) | 3_compare_gdi_x64.log, 3_widths_ft_x64.log |
| tests | test_fttext_raster.py 9 passed (55 s); test_fttext.py 22 passed (SG_PORTSIM_DLL_NOFT=port/build/S5_3_noft: the no-FreeType test runs); port/test/test_static.py over the mirror tree 6 / 6 (3_static_mirror.py: the shared tree holds siblings' unlisted modules) | 3_test_fttext_raster_x64.log, 3_test_fttext_x64.log, 3_static_mirror.log |
| segment gate x64 | 205 / 205 PASS, 0 digests differ from log/p5/segs_x64_loc.tsv (86 s, final build) | 3_segs_x64.log / .tsv, 3_segs_x64_compare.log |
| boot gate | HEAD's 61: 58 PASS / 2 KNOWN / 1 NO_FRAMES, 0 rows differ from S0 (extra rows as S5.1: cov_r_bio / cov_r_flic_input FAIL) | 3_boot_gate.log / .tsv, 3_boot_compare.log |
Controls' arithmetic: mode 5 moves every glyph one pixel (6 samples), mode 6 flips the 5-bit red LSB of every inked pixel,
mode 7 changes Arial's and Klepto's hinted outlines; each raises the corpus difference to GDI, so none is an equivalent
mutant over the judged data apart from the listed blind subsets.

## 7. For the D4 decision (lead)
The FreeType provider gives the same widths, metrics and layout as GDI (S5.1) and labels whose blend and filter are GDI's, but
about 70 % of the ink pixels differ (95 % of those by 4 or more 5-bit steps in some channel) because GDI's x hinting is not reproducible with FreeType. A
pixel-exact frame gate therefore needs the GDI provider on Windows; FreeType is the provider for hosts without GDI, judged by
these measured numbers (pinned in test_fttext_raster.py) rather than by equality.

## 8. Reproduce
```
~/bin/py -3.12 log/roadmap/S5/3_build.py mirror --build both        # and --build noft
export SG_PORTSIM_DLL=$PWD/port/build/S5_3/Release/portsim_c.dll SG_PORTSIM_W32_DIR=$PWD/port/build/S5_3_w32
~/bin/py -3.12 -m port.test.labels render freetype                   # rasters/freetype_x64.npz (and --backend win32 --w32-dir ..)
~/bin/py -3.12 -m port.test.labels compare freetype [--backend win32 --w32-dir port/build/S5_3_w32]
~/bin/py -3.12 log/roadmap/S5/3_diffstats.py --out log/roadmap/S5/3_diffstats.tsv
~/bin/py -3.12 log/roadmap/S5/3_controls.py
~/bin/py -3.12 log/roadmap/S5/3_gfx_ft.py --provider fttext [--backend win32 --w32-dir port/build/S5_3_w32]
~/bin/py -3.12 -m pytest port/test/test_fttext_raster.py -v -p no:cacheprovider
~/bin/py -3.12 -m port.test.integrator segments --backend x64 --dll port/build/S5_3/Release/portsim_c.dll --jobs 8 --out <tsv>
~/bin/py -3.12 -m port.test.boot --all --jobs 3 --out log/roadmap/S5/3_boot_gate.tsv && ~/bin/py -3.12 log/roadmap/S5/3_boot_compare.py
```
Fit tools (GDI side through ctypes, FreeType through s5probe.dll): 3_gdi.py, 3_gdi_levels.py, 3_edges.py, 3_bwcmp.py,
3_fit.py, 3_fit2.py, 3_phase.py, 3_fity.py, 3_ink.py, 3_place.py, 3_mix.py (--rules, --correlate), 3_overlap.py, 3_job_cmp.py.

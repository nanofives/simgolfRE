# S5.2 notes: label pixel corpus and the GDI reference raster (2026-10-24)

Status: DONE. Gate (a)-(e) met on a private mirror build of HEAD 4bfefc2a + this step's two sources.

## What was built
- **Module textlab** (port/src/sim/textlab.cpp, port/include/portsim/sim/textlab.h). Provider-neutral entries over whatever
  TextMetrics the context has attached:
  `textlab.draw(fontref, text, n, x, y, colorref, surf, w, h)` (draw_glyphs on a w x h RGB555 surface in the address space),
  `textlab.batch(jobs, count, out, out_size)` (many jobs, each on its own filled surface, JOB FORMAT in textlab.cpp),
  `textlab.width` / `textlab.ascent` (glyph_width / glyph_ascent), `textlab.recordStart` / `recordStop` (the checkpoint call
  recorder, RECORD FORMAT in textlab.cpp; written to the files overlay as `$textlab/calls.bin`). No provider attached =
  MissingResource{"text provider"}. Negative controls g_negctl_textlab 1..4 (textlab.h).
  The recorder for the GDI provider is a GdiTextMetrics subclass (GdiRecorder): gditext's registered Text_put /
  drawMarkupRun / measureTextWidth dynamic_cast the context's provider to GdiTextMetrics, so a plain wrapper made the boot
  FAIL (first attempt, 0 calls recorded). Other providers are wrapped (WrapRecorder). See 2_merge.md for what this needs
  from S5.1's refactor.
- **port/test/labels.py**: `corpus | render | reference | checkpoints | facts | compare <provider>` (docstring).
- **port/test/test_labels.py**: 14 tests (3 static), 14 pass in 49 s with both private builds.
- Tools in log/roadmap/S5/: 2_mirror.py (HEAD `git archive` of port/ + re/model/include into port/build/S5_2_src, modules.inc
  + `SG_MODULE(textlab)`, builds port/build/S5_2 and port/build/S5_2_w32), 2_colours.py (the colour scan).

## (a) The corpus (port/corpus/labels/, local)
- corpus.json: every distinct (FontRef, drawn text) of the 10 widths recordings (`*.widths.tsv` Font::textWidth bytes,
  `*.textput.tsv` Text_put text in the base font +0x5c): **6,540 entries in 9 FontRefs** (0x00519928 89, 0x00519948 1,
  0x00519a40 84, 0x00519fd8 385, 0x0051a028 97, 0x0051b360 5,598, 0x00821020 12, 0x00821ee8 272, 0x00821f28 2); left
  out 44 texts with a tab or a C0 control byte, 21 empty. '_' drawn as ' ' (the run drawer, c3et.cpp allocTextBuffer at
  0x0047683e). The other 6 of the game's 15 FontRefs draw no recorded text.
- Surfaces are sized from the recording and the FontRef words the original stored (cov_a_play's first segment start
  image): w = recorded width + 2 mx (+ one font height per token character), h = FontRef +0xc + 8, baseline 4 + FontRef
  +0x18, pen mx = 4 + height / 2. Never from the provider, so every provider draws on identical surfaces.
- Colours: 2_colours.py reads the colour argument at all **414** call sites of drawText4049d0 0x004049d0, drawTextScaled
  0x00404a20, drawTextSurface 0x00404ad0, drawCenteredText 0x00404b70, drawCenteredText2 0x00404bc0 (argument 4) and
  Widget_setQuad 0x00476310 (argument 1; shadow argument 2): 289 immediates, **21 distinct direct colour words**
  (0x80000000 x138, 0x80007fff x48, 0x80002108 x16, 0x80007d08 x18, ...; 2_colours.tsv), 125 register / memory (U-1513).
  Conversion = SetRunColour's 5-5-5 split (c3et.cpp, the 5-5-5 arms of the display-format tests at 0x004769ac (red) and 0x0047697f (green): r = (c >> 7) & 0xf8, g = (c >> 2) & 0xf8,
  b = (c << 3) & 0xff), as gfx.cpp draw_text_string does.
- Configurations, 15,960 jobs: base (every entry, 0x80007fff on 0x0000), inverse (every entry, 0x80000000 on 0x7fff), sweep
  (each font's 6 most called texts x 21 colours x backgrounds 0x0000 0x7fff 0x4210 = 2,835), shadow (the 45 sweep texts with
  drawCenteredText2's shadow setQuad(colour, 1, 0, 1) at 0x00404bc0..0x00404bd1: shadow at (+0, +1) first, stand-in colour
  0x0000, U-1512).
- Checkpoint label calls: `labels.py checkpoints` boots sandbox_basic (to `menu`) and cov_b_location (whole trace) with
  gfx attached and the recorder on: 2,527 and 26,937 draw_glyphs calls; per checkpoint the visible calls (before the
  snapshot, one per font/colour/origin/text/pre-box) 12 / 75 / 75 / 75 / 76 / 78, of which **on the frame** (last at their
  box, GDI post words = the original's frame words) menu 5, refused 11, site_hover 10, buttons 9, tip 10, back 4. Boxes =
  the ink (provider redraw on 0x0000 and 0x7fff scratch surfaces) + 4 words. summary.x64.json lists every on-frame call
  (box, font, colorref, origin, text). Recordings x64 == Win32 byte for byte (sha256 2b122920... sandbox_basic,
  f1d4ffc0... cov_b_location). The snapshots equal the pinned frames (0 px).

## (c) Reference
`labels.py reference` (2_reference.log): GDI twice on x64 and twice on Win32, all four digests
a225778f96dabf91c6f1cce6e08a5783d9b4a595a00e5535b21e9662d7101112 (also in a separate earlier x64 process); per-font job counts
in reference.json; rasters in rasters/gdi_x64.npz and gdi_win32.npz.

## (d) Measured rendering facts (2_facts.log, port/corpus/labels/facts.json)
- Host (SystemParametersInfoW GET only): SPI_GETFONTSMOOTHING 1, SPI_GETFONTSMOOTHINGTYPE 2 (ClearType), contrast 1200,
  orientation 1 (RGB), SPI_GETCLEARTYPE 1.
- The GDI provider's raster is **subpixel antialiased (ClearType) on the 16-bit DIB**: base 6,532 subpixel + 8 mono (texts of
  spaces), inverse the same, sweep 2,520 subpixel + 315 mono (the colour equals the background, or space texts).
- The original's labels in the six PASS checkpoints are the same: every on-frame call classifies subpixel; of the changed
  pixels only 1,579 / 4,901 (menu) ... 591 / 4,458 (refused) take the text colour exactly, the rest are fringe blends with the
  background. Colours seen: 0x808080 (0x80004210) and 0x404040 (0x80002108) on the menu / back, 0x000000, 0x404040, 0xf8f8f8 on
  the location screen.
- lfQuality, per font only (GdiProbe in labels.py, FR_PRIVATE fonts; its DEFAULT raster equals the provider's on 45 / 45):
  DEFAULT = DRAFT = PROOF = CLEARTYPE on 45 / 45; NONANTIALIASED equal on 3 (all mono), ANTIALIASED on 3 (grey 37, mono 8),
  CLEARTYPE_NATURAL on 19. So the game's lfQuality 0 resolves to the session's ClearType: the host setting decides the raster
  (dependence on other settings documented, not measured: U-1510, U-1511). Nothing system-wide was changed.

## (e) compare and controls
`labels.py compare <provider> [--backend win32 --w32-dir ...] [--negctl N]`: per font and per configuration differing jobs /
pixels against rasters/gdi_x64.npz; per checkpoint px against the GDI run's post-call words (all visible calls) and against
the original's frame (on-frame calls). GDI: 0 / 0 on x64 and Win32. Controls (logs 2_compare_gdi_x64_n<N>.log):
| N | what | corpus jobs / px | checkpoints px vs GDI run (menu..back) | vs original |
|---|---|---|---|---|
| 1 | last glyph +1 px | 13,776 / 747,568 | 702, 3,190, 3,190, 3,190, 3,211, 3,324 | 308..441 |
| 2 | colour one RGB555 red step | 15,737 / 6,846,558 | 8,663 .. 18,614 | 1,331..3,052 |
| 3 | last character dropped | 13,776 / 642,922 | 742 .. 2,877 | 257..367 |
| 4 | whole run +1 px | 15,626 / 14,262,459 | 11,507 .. 41,319 | 4,084..5,139 |
4 / 4 RED; blind subsets recorded (U-1514: 2,184 jobs ending in a space are equivalent under 1 and 3).

## Reproduce
```
~/bin/py -3.12 log/roadmap/S5/2_mirror.py --build both
export SG_PORTSIM_DLL=$PWD/port/build/S5_2/Release/portsim_c.dll SG_PORTSIM_W32_DIR=$PWD/port/build/S5_2_w32
~/bin/py -3.12 log/roadmap/S5/2_colours.py > log/roadmap/S5/2_colours.tsv
~/bin/py -3.12 -m port.test.labels corpus
~/bin/py -3.12 -m port.test.labels reference --w32-dir port/build/S5_2_w32
~/bin/py -3.12 -m port.test.labels checkpoints            # and --backend win32 --w32-dir port/build/S5_2_w32
~/bin/py -3.12 -m port.test.labels facts
~/bin/py -3.12 -m port.test.labels compare gdi [--negctl N] [--backend win32 --w32-dir port/build/S5_2_w32]
~/bin/py -3.12 -m pytest port/test/test_labels.py -p no:cacheprovider
```
No segment or boot gate was run (light runs only). The checkpoint recording boots are two port boots (no game).

## For S5.3
- Target: ClearType at this host's parameters on a 16-bit 5-5-5 DIB, blended with the destination (backgrounds matter: the
  sweep has three). `labels.py compare freetype` works once module fttext registers `fttext.attach` / `fttext.detach`;
  the checkpoint replay uses the generic WrapRecorder path only for recording, compare itself needs no recorder.
- Open: U-1510..U-1514 (2_uncertain.tsv).

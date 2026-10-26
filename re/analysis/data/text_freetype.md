# Text without GDI: FreeType against GDI (roadmap S5, 2026-10-24)

For Mariano. The port draws and measures its labels through a `TextMetrics` provider. Two exist: `gditext` (Windows GDI, the
default) and `fttext` (FreeType, no Windows API). This page puts them side by side and ends with the decision you have to take.
Sources: log/roadmap/S5/1_notes.md (widths, metrics), 2_notes.md (label corpus), 3_notes.md (raster), 4_notes.md (selection and
gates). All numbers were measured on LIVING-PC (ClearType on, contrast 1200, RGB orientation).

## 1. Side by side

| Measure | GDI | FreeType | Verdict |
|---|---|---|---|
| Text widths (Font::textWidth), 11,459 recorded printable rows | 11,459 / 11,459 | 11,459 / 11,459, x64 = Win32 | equal |
| Text_put 9,079 rows, drawMarkupRun 10,776 rows | equal | equal | equal |
| The five FontRef metric words, 15 FontRefs | reference | 15 / 15 equal, x64 and Win32 | equal |
| Label pixels, corpus of 6,540 texts (15,960 jobs, RGB555) | reference (ClearType) | 8,736,593 px differ; about 70 % of GDI's ink pixels, 95 % of those by 4 or more 5-bit steps | NOT equal |
| Blend of a ClearType pixel | reference | exact (0 of 7,168 table entries differ) | equal |
| Segment gate, 205 segments, x64 | 205 PASS, 0 digests differ | 205 PASS, 0 digests differ | equal |
| Segment gate, Win32 | (HEAD baseline) | 205 PASS, equal to FreeType x64 row for row | equal |
| Boot gate, HEAD's 61 scenarios, x64 and Win32 | 58 PASS / 2 KNOWN / 1 NO_FRAMES | 58 / 2 / 1, 0 rows differ from the baseline | equal |
| Six PASS gfx checkpoints (menu, 5 location screens) | 0 px differ | menu 1,735 px; refused 15,347; site_hover 13,964; buttons 13,600; tip 13,714; back 1,735; every differing pixel inside a label box, 0 outside; Win32 identical | NOT equal |

Why the sim does not notice: the simulation reads text only through widths (hit tests, layout), and the widths are equal. The
drawn pixels are not simulation state. Why the pixels differ: GDI's ClearType hints glyph x positions in a way FreeType has no mode
for (U-1520). The blend and the subpixel filter are GDI's; the stem phase of many glyphs is not.

## 2. Font sources

| FontRef family | Face GDI picks | Where a host gets it |
|---|---|---|
| Manual SSi (and Bold), 7 FontRefs | manu3_.ttf | the game install (read through GameFiles) |
| Klepto ITC TT, 3 FontRefs | klepto__.ttf | the game install |
| Arial Bold 9 / 10, Times New Roman Bold 18 | arial.ttf (regular) | **the host font directory** |
| Comic Sans MS Bold italic 100 | ariali.ttf | **the host font directory** |
| Times New Roman 12 | times.ttf | the host font directory |

Arial, Arial Italic and Times New Roman are not in the install. A non-Windows host must supply them (the directory is
`$SG_FONT_DIR`, else the Windows font folders). Substitutes were not measured (U-1531). Faces are matched by their `name` tables,
the way GDI matches the face name.

## 3. License

FreeType is used under the FreeType License (FTL), not the GPL option. Any shipped binary that links it must carry this credit in
its documentation: "Portions of this software are copyright (c) 2026 The FreeType Project (www.freetype.org). All rights reserved."
(full text in vcpkg's installed/<triplet>/share/freetype/copyright). FreeType is an optional dependency: without it the module
builds and every call reports a missing resource, and GDI stays the default. vcpkg's triplets are dynamic, so freetype.dll ships
next to the port DLLs (U-1505).

## 4. Selecting the provider

`SG_TEXT_METRICS=gdi` (default) | `freetype` | `none` for SegmentRunner, boot.py and integrator.py (gfx.py follows through
boot.run). The edits are written as merge lines in log/roadmap/S5/4_merge.md and were proven in a mirror: with no variable every
output is unchanged (segment gate 205 / 205, 0 digests differ). A probe shows the selection really attaches `fttext`: the ppem + 1
control reads RED under freetype only (log/roadmap/S5/4_provider_probe.log).

## 5. Open U-rows

U-1500 / U-1501 face rule and Arial substitute measured on this host only. U-1505 freetype.dll beside the DLLs. U-1510 / U-1511 /
U-1523 the raster depends on the host's ClearType settings (contrast, orientation). U-1520 GDI's x geometry is not reproduced.
U-1521 / U-1522 the placement and y-smoothing rules are fitted on three faces. U-1524 / U-1525 bytes and bold synthesis not
measured (no recorded use). U-1530 controls 1 and 3 are blind on the segment gate. U-1531 non-Windows substitute fonts.
U-1532 re-run the proof after the merge.

## 6. The decision

Switch the default to FreeType, or keep GDI?

**Recommendation: keep GDI on Windows.** FreeType matches GDI on everything the simulation reads (widths, metrics, all 205
segments, the boot gate) but not on drawn pixels: 6 of 6 gfx checkpoints that PASS pixel-exactly with GDI differ with FreeType
(1,735 to 15,347 px, all in label boxes). Switching the default would turn six passing frame checks into failures for no gain on
Windows. FreeType is the right provider for a host without GDI, where it is judged by these measured numbers rather than by
pixel equality, and it is selectable today with `SG_TEXT_METRICS=freetype`.

S5 status (lead: claude2 session cmvmhbwva2wm1o115fq2gena7; updated 2026-10-24)
steps: 4 planned, 4 accepted (S5.1..S5.4), 0 running, 0 held
live workers: none (all children closed)
game slots held: none (no step needs the game)
FreeType: G4 answered: 2.14.3 core by vcpkg at C:/Users/maria/vcpkg/installed/{x64,x86}-windows (run from claude3; logs log/roadmap/S5/vcpkg_*.log); license FTL (credit to record) or GPL-2.0+; optional CMake target, GDI default unchanged. S5.1's own 2.13.3 build under C:/Users/maria/freetype is not used
last event: 2026-10-24 S5.4 accepted and closed; every step accepted; MERGE.md and files.txt (204 paths) written; DONE reported to the orchestrator. Decision for Mariano: re/analysis/data/text_freetype.md section 6 (keep GDI on Windows recommended)
reference: the GDI provider (module gditext, 38a): widths 11,459 / Text_put 9,079 / drawMarkupRun 10,776 equal to the recordings
next: the orchestrator merges (log/roadmap/S5/MERGE.md); Mariano decides the default (text_freetype.md section 6)
uncertainty ids: S5.k gets U-15(k-1)0..U-15(k-1)9
decision to bring to Mariano at the end: switch the port's text default from GDI to FreeType, or keep GDI (D4)

## Workers
| step | session | spawned | state |
|---|---|---|---|
| S5.1 | cmvmhj2og2xeto115lb79n2x5 | 2026-10-24 | accepted, closed |
| S5.2 | cmvmhj7s72xgdo115rcswhj5g | 2026-10-24 | accepted, closed |

## Accepted
- S5.2 (2026-10-24): label corpus 6,540 (FontRef, text) in 9 FontRefs, 15,960 jobs + the six PASS gfx checkpoints; GDI reference
  identical in two runs x64 and Win32; `labels.py compare gdi` 0 px per font and checkpoint; controls 4/4 RED (U-1514 blind subset);
  FINDING: GDI and the original's labels are ClearType subpixel on the 16-bit DIB (lfQuality 0 follows the host's ClearType setting).
  Merge: SG_MODULE(textlab) (2_merge.md); depends on GdiTextMetrics staying subclassable (S5.1 told).
| S5.3 | cmvml3pcr35t1o115d9sv3p05 | 2026-10-24 | closed (permission prompt) |
| S5.3 | cmvmni04v3et5o115aqr3szyu | 2026-10-24 | closed (prompt for bare cmake) |
| S5.3 | cmvmok13n3k7fo115t73nxomi | 2026-10-24 | accepted, closed |
| S5.4 | cmvmqndrr3pv5o115kh4djy46 | 2026-10-24 | accepted, closed |
- S5.1 (2026-10-24): FreeType 2.14.3 (vcpkg) optional in CMake (SG_FREETYPE_ROOT / PORTSIM_FREETYPE_ROOT, nothing hard-coded;
  no FreeType -> fttext throws MissingResource, GDI default); gditext refactored into MarkupTextMetrics + GdiTextMetrics, GDI
  outputs row-identical to HEAD; FtTextMetrics faces = GDI's files 15/15 (Arial Bold / Times New Roman Bold -> arial.ttf,
  Comic Sans MS Bold italic -> ariali.ttf on this host), metric words 15/15, widths all printable rows equal x64 + Win32
  (only lone 0x0a rows differ, U-0960); controls 4/4 RED. FTL credit in 1_merge.md. Merge: 1_cmake_block.txt, SG_MODULE(fttext).
- S5.3 (2026-10-24, third worker): FreeType raster in fttext_raster.{cpp,h} wired into FtTextMetrics::draw_glyphs; blend equal
  to GDI ClearType (0 / 7,168, all 1,024 grey pairs); box filter exact on Arial; corpus 15,618 / 15,960 jobs differ, 8,736,593 px
  (70 % of GDI ink; Klepto 0.26-0.29, Arial 0.60, Manual SSi 0.61-0.81); checkpoints with fttext 1,735 / 15,347 / 13,964 /
  13,600 / 13,714 / 1,735 px, all inside label boxes; x64 = Win32; controls 3/3 RED; merge SG_MODULE(fttext_raster).
- S5.4 (2026-10-24): SG_TEXT_METRICS=gdi|freetype|none as merge lines (4_merge.md, proven on patched harness copies); with
  FreeType segments 205/205 x64 + Win32 (0 digests differ), boot 58/2/1 x64 + Win32 (0 rows differ); gfx checkpoints as S5.3;
  report re/analysis/data/text_freetype.md (recommendation: keep GDI on Windows). U-1530..U-1532.

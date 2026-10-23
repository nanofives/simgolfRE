# Phase 5, step 1: the software 16-bit graphics backend

Goal (re/PORT_PLAN.md, phase 5): the port draws what the original draws, into a 16-bit frame buffer it owns, from the
user's own game files, and every frame it draws is judged pixel by pixel against the original's checkpoint frames. This
step builds the backend's skeleton and one frame end to end: the title menu of `sandbox_basic`'s `menu` checkpoint.

Status (2026-10-22): see "Result" at the end.

## 1. What the original does (the specification)

The original draws with the exe's Image / SpriteRef / Window objects over jgld.dll's Surface, Sprite, Palette and Font,
and jgld draws through GDI into DIB sections. The facts this step relies on, each from a C3/C4 source:

| Stage | Original | Source |
|---|---|---|
| PCX load | `loadImage` 0x00475840 -> `loadImageData` 0x00478cd0 -> `decodePcx8` 0x00495100 (the exe `Image` class's slot +0x10 returns 0, so the 16-bit decoder never runs for it) | shim/src/re/c3ee.cpp `LoadImage_re`, `DecodePaletted_re` |
| Image surface | 8 bpp top-down DIB section, pitch (w + 3) & ~3 | jgld `allocDrawBuffer` 0x10007e40 note |
| Palette | the file's last 0x300 bytes, `count` entries from `ofs`, -> `Palette::setEntries` -> the DIB colour table (`SetDIBColorTable`) | c3ee.cpp, c3dm.cpp |
| 8 -> RGB555 | `(R >> 3) << 10 | (G >> 3) << 5 | B >> 3` (jgld `Palette::apply` table +0x40c; GDI's StretchBlt from the 8-bit DIB to the 16-bit one gives the same words: measured, section 5) | re/match/jgld_app.cpp |
| Root surface | `[f_stateptr_4c1570]` = g_rootWindow + 0x274, a 16 bpp top-down DIB, format word 0 (RGB555), BkMode TRANSPARENT, TextAlign TA_BASELINE, 800 x 600 | c3dk.cpp allocDrawBuffer, c3fv.cpp gameMain |
| Full-screen copy | `Surface_blit3(image, Root, 0, 0, 800, 600, 0, 0, W, H)` -> jgld `Surface::stretchTo` = StretchBlt SRCCOPY; 1:1 at 800 x 600 | c3h.cpp `SurfaceBlit3_re`, c3dg.cpp |
| Button sprites | `showTitleScreen` 0x0043cd70 (gameMain+0x41f) loads interface\TitleUnSel and interface\TitleMO (flags 2) and crops 7 rectangles of each into g_spriteRef_53b7e0[0..13] with `Surface_blit` 0x00473bf0 -> `cropSprite` 0x00492000 (kind 1, trim 1: rows / columns entirely 0xff removed, left / top trim at +0x20 / +0x24) | c3fb.cpp `ShowTitleScreen_re`, c3ee.cpp `CropSprite_re` |
| Sprite draw | `Image_draw` 0x00473e60: at (m_20 * t0 / t2 + x, m_24 * t1 / t2 + y), (t0, t1, t2) = (1, 1, 1) at the menu -> jgld `Sprite::drawB` -> `draw16_1001bc40`: clip to the destination, bytes >= 0xfe transparent, others the sprite palette's RGB555 word | c3df.cpp, re/match/jgld_blit16.cpp |
| Text | `drawCenteredText` 0x00404b70: `setQuad(Root, colour, -1, 2, 2)`, x' = x + (0 - width) / 2 (C division), width = `drawMarkupRun` (GetTextExtentPoint32A); the run drawer 0x004767a0 draws at y + (ascent - internal leading) of the current font with `TextOutA`, colour bit 31 = a direct RGB555 word, `SetTextColor(RGB((c >> 7) & 0xf8, (c >> 2) & 0xf8, (c << 3) & 0xff))` | c3dq.cpp, c3df.cpp, c3et.cpp, gditext.cpp |
| Present | `Window::refreshFull` 0x00480c80 -> redrawDrawList -> `Window::blitToTarget` 0x00480dc0 copies the root surface into the screen image; `Display::present` BitBlts it to the window. The checkpoint frame is jgld's back buffer (scenario_ab.py `checkpoint_frame`), so it holds what the last refreshFull copied | agent survey, c3dw.cpp |

Not determined at this step (not needed by the menu frame): whether the screen image 0x0083a7b8 and the display's back
buffer `[disp + 0x148]` are one surface (jgld_1.md), the root window's screen offset when it is not 800 x 600, the span
(RLE) Sprite form gated by 0x101284c4, and the cursor image drawTooltip composites when 0x0083aac4 is set (no cursor is
in any back-buffer frame of the menu).

## 2. The backend (port/src/sim/gfx.cpp, port/include/portsim/sim/gfx.h)

A per-context `Gfx` object, attached like the text provider (`set_gfx(ctx, Gfx*)` / `gfx(ctx)`); with none attached
the drawing calls stay drops and nothing changes (the segment and boot gates run without one).

Model, owned by the port, keyed by the original object's 32-bit address so the frame code keeps passing the same
arguments it passes today:

- `Surface16`: w, h, RGB555 words (top-down), a clip rect. The root window's surface and the back buffer are two of them.
- `Image8`: an 8-bit image (w, h, pixels) and its palette as 256 RGB555 words (what the DIB colour table gives GDI).
- `Sprite`: a cropped Image8 with its trims (+0x20 / +0x24 of the SpriteRef) and its source palette.
- `Font`: the FontRef address; the glyphs and widths come from the TextMetrics provider.
- the current draw target's font list (setDrawTarget's four slots).

The game objects in GameState are never written by the backend: its state is outside the address space, so the sim
hashes and every segment digest are unchanged by construction (tested: section 4).

Dispatch: the frame code already calls every drawing function by its hooks.csv name (`tc("Image_draw", ...)`) and
tick.cpp resolves them as drops. With a Gfx attached, `TkBase::call` hands a drop to the backend when the backend has a
body for that name. The backend returns what the drop returned (the original's results of these calls feed no simulation
state: that is why they are drops), never throws into the frame (a failure is recorded in the backend's log and the
call behaves as the drop), and records every drop name it has no body for, so a frame drawn with an unsupported call is
reported as such instead of being compared blind.

Bodies of step 1: `showTitleScreen` (the button sprites; the intro movie stays the audio / video backend's),
`loadImage` (8-bit RLE PCX, palette argument or flags 2), `Surface_blit` (cropSprite), `Surface_blit3` (1:1 copy; any
scaling is recorded unsupported), `Image_draw` (Sprite onto Surface16 with the 0xfe test), `setDrawTarget` (font
slots), `drawCenteredText` (plain text: no markup token, no tab; anything else recorded unsupported), and
`Window::refreshFull` (root surface -> back buffer). `showTitleScreen` is added to the boot path at gameMain+0x41f as a
drop (it was not called at all), so without a Gfx the boot is unchanged.

Text: the original rasterises with GDI (TextOutA, lfQuality 0 = the host's smoothing setting), so the glyph pixels are a
host service like the widths: the `TextMetrics` interface gets `draw_text(ctx, fontref, text, x, y, colorref, Surface16&)`,
which the GDI provider implements by drawing into a 16-bit top-down DIB section holding the surface's words (BkMode
TRANSPARENT, TA_BASELINE, the same font object it measures with) and copying them back. Without a provider that draws,
the call is recorded unsupported. A portable rasteriser (the font files' TrueType outlines) is a later decision of this
phase together with the widths (U-0960 / U-0961); the gate below tells whether it reproduces GDI.

Assets: every image is read at run time through the context's `GameFiles` (the user's install); no asset byte, palette
or glyph is copied into the repository, and the backend never writes a file into the install.

## 3. The gate

A checkpoint frame of the original is the back buffer at the clock call where the scenario took the checkpoint:
`summary.json`'s checkpoint `status.tgt` is the count of main-thread timeGetTime calls, and the trace's tgt record of that
index (port/corpus/traces/<scenario>.trace) gives its clock ordinal P. The port snapshots its back buffer when it first
consumes a trace item past P (a CURSOR read or a dispatch with position > P; the trace's CURSOR records carry their
position, now kept in FrameInput::cursor_pos), so the frame code has done exactly the drawing the original had done.
Snapshots are read back through the files overlay (`$gfx/snapshot_<k>.555`, 800 x 600 little-endian words).

Comparison: re/tools/imgcmp.py `exact_diff(port, reference, regions)` expands the port's words like scenario_ab.py
((v << 3) | (v >> 2)) and counts differing pixels in the whole frame and per named region. The menu's regions: each
button's destination rectangle, each label's text box, and the background outside all of them. Pass = 0 differing
pixels in every region.

Negative controls (`g_negctl_gfx`, read RED by port/test/test_gfx.py): 1 Image_draw one pixel to the right (a wrong
blit), 2 one palette entry's RGB555 word off by one in the background image (a wrong palette entry), 3 the transparency
test `> 0xfe` instead of `>= 0xfe`, 4 refreshFull skipped (the back buffer keeps the previous present). Each must change
the frame; 1 and 2 are the two the plan of this step requires.

The segment gate (205 / 205, digests unchanged) and the boot gate run with no Gfx attached and must stay as they are;
test_gfx.py also checks that a boot run with the Gfx attached gives the same per-frame sim hashes as one without.

## 4. Order of work after this step

1. DONE (section 6): the location screen (`courseActionMenu`, cov_b_location's checkpoints buttons / site_hover / tip /
   refused / back). Next: the other title-menu screens (difficulty, theme packs, the load browser, pick a pro).
   The inventory measured before step 2: Measured inventory (c3fw.cpp `CourseActionMenu_re`): 10 drawCenteredText, 8
   Surface_stretchBlit, 7 setDrawTarget, 6 Image_draw, 3 drawText, 3 drawCenteredText2, 3 Window::refreshFull, 2
   Surface_blit, 2 loadImage into a frame-local Image array (vector ctor / dtor, a palette cache), and one each of
   drawPanelBackdrop, drawFrameR3, drawPanelS2, fillRegion, drawWindow. New primitives: scaled stretch blits, the panel
   nine-patches over the g_spriteRef_562368 / 59d920 frame pieces, region fills, left-aligned and second-style centred
   text. The port's `course_action_menu` (saveload.cpp) was written simulation-only with no drawing call left in it, so
   the calls go back in at their places around its pumps. cov_b_location has no frame record (boot.run returns
   NO_FRAMES before running), so its gate needs a boot run that stops on the last trace record instead.
2. The in-game frame: Terrain.dll's OpenGL renderer behind TerrainApi (its own sub-plan), then the sprite queue,
   panels and text with markup.
3. Text without GDI: compare a font-file rasteriser with the GDI provider on every recorded label before switching.
4. Audio.

## 5. Result

2026-10-22. The title menu frame of sandbox_basic's `menu` checkpoint (clock ordinal 16585, tgt call 3434, T 1052010) is
pixel-exact: 0 of 480,000 pixels differ, and every region (7 buttons, 5 label boxes, the background) is at 0, on x64 and
Win32 (port/test/test_gfx.py, 9 tests, about 10 s). The frame is drawn from the user's install at run time through
GameFiles (interface\TitleBASE.pcx, TitleUnSel.pcx, TitleMO.pcx and the two Pop_upOK window-frame files) and the host GDI
text provider; no unsupported drawing call happens before the snapshot.

Measured before the C++: a Python prototype (8-bit PCX decode, palette `>> 3`, the 14 crops with bytes >= 0xfe
transparent, TextOutA with the Klepto ITC TT font at -18 on a 16-bit DIB) gave 0 differing pixels. Each layer was checked
alone first: the background alone matched all 380,398 pixels no sprite or label covers, and the sprites left only the label
pixels (5,823).

Negative controls: 1 (Image_draw one pixel right) 49,162 px RED, 2 (one palette word off by one) 1,261 px RED, 4
(refreshFull not presenting) 480,000 px RED; 3 (`> 0xfe`) is an equivalent mutant on this frame, since the three title
images hold no byte 0xfe (U-1070).

Gates: segments 205 / 205 PASS on x64 and Win32, 0 digests differ from phase 4's (log/p5/segs_x64_gfx.tsv,
segs_win32_gfx.tsv against log/p4/segs_*_quit.tsv), x64 = Win32; boot 58 PASS, 2 KNOWN, 1 NO_FRAMES, with every
scenario's status and record count equal to phase 4's (log/p5/boot_gate.tsv). With the backend attached the whole
sandbox_basic boot still PASSES every record (the backend writes nothing into the GameState). Port suite one file per
process (log/p5/suite_by_file.log): 35 of 36 files PASS. test_commands stops with a pytest INTERNALERROR MemoryError, as it
did before this step (log/p4/suite_by_file2). Run one test per process (log/p5/test_commands_each.log), every
non-skipped test passes. Repository static suite: 600 passed.

Port changes: module `gfx` (port/src/sim/gfx.cpp, portsim/sim/gfx.h); TextMetrics gained glyph_width / glyph_ascent /
draw_glyphs (commands.h), implemented by GdiTextMetrics; tick.cpp hands drops to the backend when one is attached and
reports trace progress (FrameInput / FrameIo `cursor_pos`, TraceInput dispatches) for snapshots; the boot calls
showTitleScreen at gameMain+0x41f as a drop. Harness: port/test/gfx.py (render, compare, `-m port.test.gfx <scenario>
<checkpoint>`), re/tools/imgcmp.py `exact_diff` / `rgb555_to_rgb`. U-1068 (text pixels follow the host's font
smoothing), U-1069 (what the menu does not settle), U-1070 (the 0xfe test is not judged yet).

## 6. Step 2: the location screen

2026-10-22. All five checkpoints of cov_b_location equal the original's back buffer in every pixel, on x64 and Win32
(port/test/test_gfx.py, 17 tests in about 31 s): `refused` (Phoenix at $150,000 against $100,000: the insufficient-funds
dialog), `site_hover` (the nearest site's wide plate, its three label lines and the blinking pin), `buttons` (Save / Load
after the scenario's date write), `tip` (the Save Game label) and `back` (the title menu again). No unsupported call occurs
before any of the snapshots.

What the backend gained (port/src/sim/gfx.cpp, each body citing its C3 / C4 source):
- loadInterfaceArt(1)'s image half, the part this screen reads: the tail loads choose{Parkland,Desert,Tropical,Links}Buttons,
  ChooseAlphaButtons_A, TacksandArrow{,_A} (each replaces the whole working Image, so the tail does not depend on the loads
  before it), plus s_TransPopups / TransPopups / TransPopups_A for the dialog. The menu's port function calls it after its
  table (`loadInterfaceArt.images`); a draw of any sprite it does not cut is reported unsupported.
- courseActionMenu's own Image and Surface array (WorldButton.pcx, WorldBase.pcx), named by `gfx_local_key` (a range no
  GameState address reaches). The port's course_action_menu (saveload.cpp) was written without drawing; its draw calls are
  back at the original's places, around the two cursor reads, as `gx(...)` calls that cost one test without a backend.
- Surface_fillRegion (1:1 StretchBlt), Surface_stretchBlit = jgld Sprite::bltS -> blitPair1 (the blend
  `((dst8 * m) >> 8 + src8) >> 3` per channel, ORed, from the matched asm; 0xff in either sprite skips; no trim offset),
  drawText4049d0 (the run drawer at x), drawPanelBackdrop (its Terrain::drawLine bar is OpenGL, not in the back buffer:
  U-1071), and drawWindow for a dialog with no golfer and mode -2: drawFrameR3 (16-rounding, blitPanel's tiles through
  Sprite::drawG = draw16_10028630, whose byte 0xf8 shades the destination through block 1 of build16BitColorTable,
  computed in the backend; the eight border pieces blended over their masks) and drawMessageBuffer (per-character widths,
  word breaks, the line height from the FontRef's words). drawTextSurface's palette-index shadow is not modelled (U-1072);
  this screen never takes it.

Port changes beyond the backend:
- TraceInput applies the trace's harness memory writes made before a cursor record when a modal loop reads that cursor
  (before, only at dispatches, H1's wait or the frame's end). cov_b_location writes g_date while the location screen waits
  with no dispatch, so the port kept drawing date 0 for the rest of the screen (one button where the original drew two).
  Control tick.frame 7 restores the old timing and reads RED; the 205 segment digests and the boot gate are unchanged.
- A boot trace with no frame head (cov_b_location's capture stays on the location screen) now runs the boot over the whole
  trace (it returned before running anything); there only, a Blocking pump past the last dispatch makes the idle passes the
  original made, one per unread CURSOR record (a pass that reads no cursor ends the run), and the snapshots still pending
  when the recorded input runs out take the back buffer as it is (gfx_end_of_input).
- boot.run(to_end=True) runs a scenario with no frame record instead of returning NO_FRAMES before running.

Controls (all RED): gfx 5 (the blend ignores its mask) on buttons, gfx 6 (the shade byte drawn as a colour) on refused,
tick.frame 7 on buttons. Gates: segments 205 / 205 on x64 and Win32, 0 digests differ from phase 4's (log/p5/segs_*_loc.tsv);
boot 58 PASS, 2 KNOWN, 1 NO_FRAMES, every scenario's status and record count equal (log/p5/boot_gate_loc.tsv).

# S3.18 notes: the bulletin board's photos and the bio EditBox (I14's last 7 UNSUPPORTED rows)

Step row S3.18 of re/roadmap/S3_steps.tsv (wave 7). Base: the S3.17 tree (`17_assemble.py --dst S3_18` = git archive ee6efc1d +
12_assemble.py + S3.17's edits). Mirror: port/build/S3_18, x64 build in it (port/test/build.py, SG_FREETYPE_ROOT). Env for every
command below: `cd port/build/S3_18; source ../../../log/roadmap/S3/18_env.sh` (a copy of 17_env.sh). Delivery: 18_edits.py
(18_make_edits.py), 18_new/, 18_assemble.py, 18_merge.md, 18_uncertain.tsv (U-1840..U-1844). Heavy runs only with the wave 7 token
(W7_HEAVY_RULE.md).

## 1. What the 7 rows were

I14's census (log/roadmap/merges/I14/census.tsv): cov_b_holes18 long_holes / holes / hired / limit / end UNSUPPORTED on
`showBulletinBoard: a photo (loadBmpSurface of a saveBmp capture of the course, roadmap S3)` and cov_r_bio exit / end on
`showCharacterEditor.bio: the bio EditBox's drawing (window tree) is not modelled`. Both are CUMULATIVE reasons (framegate lists every
unsupported call before the snapshot): no row's pixels come from either drawing.
- The board is modal inside P5 (showBulletinBoard waits in pumpInput483bd0 for a click or key, c3em.cpp 0x0046f015..0x0046f03f), and
  every cov_b_holes18 checkpoint is an H1WAIT; the frame after the board repaints the view and HUD.
- The bio EditBox is drawn only inside the bio loop (c3fx.cpp 0x00439edd..0x00439ef7: showScreen, redrawAll, leaveModal483d80,
  pumpMessages, Window::refreshFull; no clock call, so no checkpoint can fall inside it), and the editor's next pass blits the whole
  800 x 600 background over it: cov_r_bio bio1_done / bio2_done (after the two bio entries) are 0 px in I14.

## 2. The photo (what the original does)

The photo is NOT narrateShot's SimFoto file. showBulletinBoard (c3em.cpp ShowBulletinBoard_re) on its first call with a pending photo
position ([0x004e3dbc] != -1) and the point on screen: saveBmp(0x00519cd4 = the root window's surface, name, rect) (0x0046e974), name =
`snapshots\accomp` (0x004e3e7c) + __itoa(photo index) + `.bmp` (0x004c4e40), rect = {bx, by, bx + 0xc8, by + 0xa0} with bx = clamp(sx -
0x64, 0, 0x258) (0x0046e899), by = clamp(sy - 0x64, 0, 0x1b8) (0x0046e8af). saveBmp (c3ea.cpp SaveBmp_re): fopen(name, `wb` 0x004c7828),
NULL returns 2 (0x0047580f); 24-bit, no row padding, bytes (w << 3) & 0xff, then for display format != 1 (w >> 2) & 0xf8 and (w >> 7) &
0xf8 (0x004756d8..0x00475755), flipBmpRows (0x0047578f), file header `BM` / size + 0x36 / 0x36, info header 0x28 with biSizeImage 0.
The board, per listed event: the name `snapshots\accomp<record>.bmp` built in g_textBuffer (0x0046ec3a..0x0046eccd), loadBmpSurface(photo,
name) (0x0046ecd6), Surface_blitThunk(photo, root, 0, 0, x, y - 0xa6, 0xc8, 0xa0) (0x0046ed1b), the pin. The photo Image is constructed
ONCE before the list (0x0046ea34) and destroyed after the wait (0x0046f15b): loadBmpSurface's FitSurface re-creates the surface only
for a larger bitmap (c3ee.cpp 0x00475275..0x004752ca), and a missing file returns 6 before touching it (0x00475234), so a missing file
draws the previous event's photo again; the first event with no file draws nothing (Surface_blit3 returns 7 for a NULL handle,
c3h.cpp 0x00475d18).
FACT: original\ (extracted from the cab), every instances\ copy and the cab have NO `snapshots` directory, so in every pinned
recording the capture's fopen fails and no photo exists. iso/data1.hdr's directory table lists `Snapshots` (offset 586002, between
`SimsFX\Female` and `Sounds`); a real install may hold the folder (U-1840).

## 3. The port and its results

- gfx_bulletin.cpp: body `showBulletinBoard.capture(sx, sy, digits)` builds the name from the two string pools and the caller's digit
  buffer, returns when the files have no such directory (FindFirstFileA of `snapshots/*` empty: what fopen `wb` meets), else encodes
  the root rectangle as saveBmp does and writes it through ctx.files() (U-1841). `showBulletinBoard.photo` reads the name from
  g_textBuffer (dialogs.cpp builds it there before the call, as the original), decodes it as loadBmpSurface (24-bit and the 8-bit
  reloadBmp body, flipBmpRows for a positive height, FitSurface growth only) into the board's ONE photo surface (family state, reset by
  `.board`), blits (0, 0, 0xc8, 0xa0) 1:1 to (x, y - 0xa6) clipped (U-1843), then the pin as before. No unsupported report is left.
- dialogs.cpp (S3.16's file; approved by the S3 lead 2026-10-29): one gx call where the original calls saveBmp, delivered as its
  own pass of 18_assemble.py after the other edits (18_merge.md section 2b); its anchor occurs exactly once on the base and on the
  base with S3.16's 16_edits.py applied (18_assemble.py check_dialogs_with_s316, run at every assembly).
- The file goes through the GameFiles write overlay: in the harness a DirGameFiles without write-through keeps it in memory, in the
  app AppFiles::write puts it under the --data directory (port/app/saves.cpp: the install view is never written). Nothing is
  written into an install.
- Tests, port/test/test_gfx_s3_18.py x64: 23 passed + 10 oracle tests skipped by default (18_test_x64.log); SG_EMU_ORACLE=1 `-k
  original`: 10 passed (18_test_oracle.log): the ORIGINAL saveBmp under Unicorn (cmdemu: handle slots +0x20 / +0xd8 / +0xdc and display
  slot +0xb4 as stubs, fopen / fwrite / fclose / new / delete stubbed, flipBmpRows real, the rect from the ORIGINAL clamp) writes the same
  bytes as the port's capture at 5 positions (centre, top-left clamp, bottom-right, past both clamps, negative); the ORIGINAL
  loadBmpSurface (loadBmp / reloadBmp / flipBmpRows real, Surface_createFull stubbed into a zeroed surface inside the data image) stores
  the same words as the port's photo for a 24-bit bottom-up, a 24-bit top-down, a 37 x 21 and an 8-bit file; a missing file returns 6
  with no Surface_createFull.
  The lead's conditions: test_capture_writes_no_gamestate_byte[True / False] (all 0x381174 GameState bytes equal after the capture
  and after the photo; the BMP exists only with a `snapshots` directory); test_capture_over_an_install_leaves_it_untouched (a real
  install directory WITH `snapshots` and an older accomp1.bmp: every file and directory hashes the same afterwards, the overlay
  holds the new BMP equal to the saveBmp model, and the board's photo drawn from it equals its decode).
- framegate, light per-scenario runs (`~/bin/py -3.12 -m port.test.framegate <scenario> --no-probe --out 18_fg/new_<scenario>.tsv`),
  compared with I14's census rows. Every row keeps I14's px exactly. Owners by S3.17's region rule (hud_* and toolbar one-level px
  S3.15, bottom_panel and view S3.16):

| scenario / checkpoint | I14 | S3.18 | px left (one level / beyond) | regions with px | owner |
|---|---|---|---|---|---|
| cov_b_holes18 long_holes | UNSUPPORTED | FAIL | 90 (90 / 0) | hud 27, toolbar 12, view 51 | S3.15 (HUD), S3.16 (view) |
| cov_b_holes18 holes | UNSUPPORTED | FAIL | 2,916 (1,347 / 1,569) | hud_meters 452 (372 beyond), hud_title 10, hud_message 81, toolbar 12, view 2,361 (1,186 beyond) | S3.15 (HUD), S3.16 (view) |
| cov_b_holes18 hired | UNSUPPORTED | FAIL | 86 (86 / 0) | hud 27, toolbar 12, view 47 | S3.15, S3.16 |
| cov_b_holes18 limit | UNSUPPORTED | FAIL | 87 (87 / 0) | hud 28, toolbar 12, view 47 | S3.15, S3.16 |
| cov_b_holes18 end | UNSUPPORTED | FAIL | 110 (110 / 0) | hud 29, toolbar 12, bottom_panel 22, view 47 | S3.15, S3.16 |
| cov_r_bio exit | UNSUPPORTED | UNSUPPORTED (HELD -> S3.19) | 107,138 (2,595 / 104,543) | = the `panel` row (FAIL in I14, no bio reason): hud_title 3,321, hud_message 568, view 103,227 | S3.15 (HUD), S3.16 (view) |
| cov_r_bio end | UNSUPPORTED | UNSUPPORTED (HELD -> S3.19) | 130 (129 / 1) | hud 33, toolbar 6, view 91 (as `loaded`, 116) | S3.15, S3.16 |

  cov_b_holes18 zoom1 (FAIL 210) and cov_r_bio loaded / panel / editor / bio1_done / bio2_done are unchanged too (18_fg/*.tsv).
- test_gfx_dialogs2.py `-k bulletin` (S2.11's board frame cov_r_s2_screens `bulletin`, all 480,000 px judged, and bulletin
  controls 1..4): 6 passed in 84 s (18_test_dialogs2_x64.log). The whole file is 27 tests of about 4.5 min each; a first full run
  was stopped after 6 passing items to free the wave 7 token (its other 21 items judge the results strip, SimFoto and ball frames).
- port/test/test_static.py: 6 passed (18_test_static.log). gfx.check: 0 duplicates (test_bodies_registered_without_duplicates).
- Not run (heavy, held for I15): segments / boot gates (no backend there, and the capture call is a gx drop: nothing changes without
  a backend), Win32, the census.

## 4. Controls (gfx.family.bulletin)

| mode | what | where | result |
|---|---|---|---|
| 5 | capture green as (w >> 3) & 0xfc | test_capture_negctl_red | RED |
| 6 | capture rows top-down | test_capture_negctl_red | RED |
| 7 | capture ignores a missing directory | test_capture_directory_negctl_red | RED (a file appears) |
| 8 | photo surface dropped at every event | test_photo_negctl_red | RED |
| 9 | photo one pixel lower | test_photo_negctl_red | RED |
| 10 | no flip of a positive-height bitmap | test_photo_negctl_red | RED |

6 / 6 RED in unit tests; all six are blind on every pinned frame (no `snapshots` directory, and the board is on no H1WAIT checkpoint):
only a recording with the folder present would judge them on frames (U-1840).

## 5. The bio EditBox: HELD -> S3.19 (the S3 lead's decision, 2026-10-29; facts for S3.19)

The 2 cov_r_bio rows (exit, end) stay UNSUPPORTED on `showCharacterEditor.bio: the bio EditBox's drawing (window tree) is not
modelled`, a cumulative reason (U-1845): the EditBox's drawing is on no checkpoint, and their px equal the `panel` / `loaded` rows'.
Measured facts for S3.19:
- The bio press (control 20, non-members only) in showCharacterEditor (c3fx.cpp, 0x00439dca): drawPanelS2(0x1d8, 0x68, 0x13c, 0xd8, 1)
  (0x00439dd8..0x00439deb), pixelRed / Green / Blue of the screen surface at (0x258, 0x78) (0x00439df3..0x00439e24), colour =
  Palette::nearest(g 0x0083ad0c, 0xff, 0xff, 0xff) (0x00439e29..0x00439e3f), EditBox::create(g_characterEditorEditBox 0x005a9f78, 0x1e0,
  0x70, 0x12c, 0xc8, root, 0) (0x00439e44..0x00439e62), setCaretActive(1), resizeBuffer(0x1f4), setText(record + 0x30), [+0x584] = colour
  (0x00439e9d), [+0x590] = 1, [+0x594] = 2, [+0x598] = 0, [+0x59c] = 4 (0x00439ea3..0x00439ebd), Window::show(0) (0x00439ec7).
- The loop 0x00439edd..0x00439f0a: showScreen, redrawAll, leaveModal483d80, pumpMessages, Window::refreshFull(root, 0, 0), until the
  button word or key 0xd. No clock call, so no checkpoint can be inside it; cov_r_bio posts its keys from the scenario agent's doAction
  at leaveModal483d80 (CLAUDE.md, roadmap S1). After it: strcat of [+0x574] into the record, EditBox::releaseState.
- Measured on I14 and on port/build/S3_18: bio1_done and bio2_done (the editor after each bio entry, MODAL:waitTicks) are 0 px:
  the editor's next pass (showCharacterEditor.background = Surface_blitThunk of the 800 x 600 background) covers the EditBox.
- EditBox::drawContents 0x004864f0 (C2, no SG_HOOK; c2 note re/analysis/ui/004864f0_EditBox_drawContents.md; SMSG's EditBox::on_redraw
  is a 100% match, reading reference only): Window::paint 0x004808c0 with [+0x584]; [+0x598] passes of Widget_drawBevelBoxR 0x004795d0
  with [+0x590] / [+0x594], each shrinking the rectangle by 1; the rectangle shrunk by the inset [+0x59c]; wrapToLines 0x004862b0 (32
  line starts), lineIndexAt 0x00486330 of the caret; visible lines = (height + line height - text height) / line height (Widget_value
  0x00477580 / Widget_get 0x00477560 for the metrics); it WRITES the first visible line [+0x57c] (0x0048662e, 0x0048663d: the caret's
  line kept in view); drawTextLines 0x004782d0 from that line; when [+0x5a8] (caret visible) a Widget_drawVLine 0x00478be0 at the caret's
  x = inset + border + drawMarkupRun 0x00477280 width, colour = the text colour table's first entry.
- The port: chareditor.cpp bio_entry (S1.4) runs the EditBox's edits and makes the `showCharacterEditor.bio` drop after drawPanelS2; it
  never writes [+0x57c] (a GameState word: a click in a bio longer than the box maps through it, U-1844) and keeps [+0x5a8] = 1 from
  setCaretActive (the original's blink is a window-tree timer on another thread).
- What is missing in the backend: a surface per window (the EditBox's own, 0x12c x 0xc8 at (0x1e0, 0x70)), the markup text drawers on
  a non-root surface (gfx_text.cpp), Widget_drawBevelBoxR / Widget_drawVLine bodies, and the window tree's composition into the back
  buffer at Window::refreshFull (gfx.cpp). Not edited here (the lead's instruction).

## 6. Open

U-1840 (whether a real install has `snapshots`), U-1841 (the capture is a backend-only file write), U-1842 (loadBmp's buffer beyond
the file), U-1843 (jgld's blit of a source smaller than 0xc8 x 0xa0), U-1844 (the bio EditBox, HELD -> S3.19), U-1845 (the cumulative reason).

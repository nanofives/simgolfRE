# S3.6 + S3.6b merge request (rebased onto HEAD b3d61e41 + I6's waves)

S3.6b rebases S3.6 (log/roadmap/S3/6_merge.md) and adds the two employee panels' drawing. This file says what changed from
6_merge.md; everything it does not mention there still holds.

## 0. Base it was measured on

`log/roadmap/S3/6b_mirror.py` makes port/build/S3_6b_src from a frozen copy of the integrator I6's mirror port/build/I6/port
(git archive of HEAD b3d61e41 = S2 waves 1-4 + S3 wave 1, with S6 wave 2, S3 waves 2 / 3 and S2 wave 5 applied by I6;
the copy's sha256 is in port/build/S3_6b_src/I6_SNAPSHOT), puts the six own files in, adds the two family lines and
applies 6_edits.py's EDITS with exact-match anchors. Builds: port/build/S3_head2/port/build/S3_6b (x64), S3_6b_w32 (Win32).
Every 6_edits.py anchor matched exactly once on I6's tree: the shared-file edits of 6_merge.md section 3b apply unchanged
after I6's merge.

## 1. Own files (copy as they are; log/roadmap/S3/6b_files.txt)

port/src/sim/gfx_hud.cpp, port/include/portsim/sim/gfx_hud.h (unchanged from S3.6), port/test/test_gfx_hud.py,
port/src/sim/gfx_pixel.cpp, port/include/portsim/sim/gfx_pixel.h (unchanged), port/test/test_gfx_pixel.py (unchanged).

## 2. Changes against 6_merge.md

- Section 2 (Widget_fillRect when S2.13 is in the tree) is DONE in the own file: S2 wave 5's `reports` family is in the tree,
  so gfx_pixel.cpp registers Widget_writePixel only (its fillRect copy and control 2 are deleted). The integrator applies
  nothing for it. gfx.check reports 0 duplicate bodies (test_gfx_pixel.py test_family_registered).
- Section 3b, edit 7: unchanged text; the two reported drops `drawEmployeeViewPanel.draw` / `drawEmployeeDetailPanel.draw` are
  drawn now (gfx_hud.cpp, U-1359 closed).
- Section 4 (UNCERTAINTIES.md): append the 10 rows of 6_uncertain.tsv as before, plus the 5 rows of
  log/roadmap/S3/6b_uncertain.tsv (U-1380..U-1384), and set U-1359's status to `resolved` with this text appended:
  `drawn by S3.6b (gfx_hud.cpp b_view_panel / b_detail_panel); evidence: the original's drawing calls under the emulator
  oracle equal the port's on 240 + 239 varied images, and ui_tour people's bottom panel equals the reference (U-1380, U-1381).`
- Section 5 (CLAUDE.md paragraph): replace its sentence `The golfer list and detail panels (drawEmployeeViewPanel /
  drawEmployeeDetailPanel) are handed over and reported, not drawn.` by the paragraph of section 4 below, and its
  `GATE MEASURE: the port draws no terrain or scene yet, so` by `GATE MEASURE: framegate --no-live (no terrain; S3.2's terrain
  is judged under G2) and no scene sprites, so`.
- Nothing for modules.inc, tick.cpp, commands.cpp or CMakeLists.txt. log/roadmap/S3/6b_edits.py is empty.

## 3. Interaction with S3.5's merge (5_merge.md, 5_merge_edits.py)

S3.5's EDITS (gfx_hud.h palette_of / half_bright_table, gfx_hud.cpp family_surface and the closing namespace) and EDITS_HUD
(the hole flag, amenity_row) anchor on S3.6's text; every one of those anchors occurs exactly once in this step's gfx_hud.cpp
/ gfx_hud.h (checked with 5_merge_edits.py's lists), so S3.5 applies after this merge unchanged. The detail panel's employee
figures need no edit after S3.5: gfx_hud.cpp calls a registered `addSprite` body when the tree holds one (S3.5's scene
family registers it) and reports otherwise (U-1382).

## 4. CLAUDE.md sentence (replaces the one named in section 2)

The golfer list and the employee detail panel (drawEmployeeViewPanel / drawEmployeeDetailPanel, S3.6b) are drawn from
c3fb.cpp / c3fc.cpp over the state commands_layout.inc hands over before its own writes (the detail panel's scroll step and
this frame's list are recomputed); names come through g_textBuffer with the buffer put back. No pinned frame holds the golfer
list or a selected employee, so their evidence is an EMULATOR CALL ORACLE (test_gfx_hud.py, SG_EMU_ORACLE=1): the original
functions under Unicorn with every drawing callee a recorder give the same calls (callee, image, position, colour, text) as
the port's `gfx_hud.recordPanels` log on 240 varied segment start images per panel; the default suite pins the port's log
digests; controls 11..15 RED. The detail panel's employee figures (addSprite) and its circle (drawCircle) go to S3.5 / S3.8.

## 5. Gate numbers (log/roadmap/S3/6b_notes.md section 4)

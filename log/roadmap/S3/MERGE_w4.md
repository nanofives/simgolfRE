# S3 merge, wave 4 (S3.6 + S3.6b: Widget_writePixel, in-game Surface_blit3, HUD panels, the employee panels)

Apply after integrator I6's tree (HEAD b3d61e41 + S6 wave 2 + S3 waves 2 / 3 + S2 wave 5): S3.6b rebased S3.6 onto exactly that
tree and measured every gate there (log/roadmap/S3/6b_notes.md section 4). files_w4.txt lists this wave's files; nothing is staged.

## Shared-file edits

log/roadmap/S3/6_merge.md sections 1-4 (pixel family, the Widget_fillRect rule, hud family, UNCERTAINTIES rows U-1350..U-1359),
as amended by log/roadmap/S3/6b_merge.md section 2 (changes against 6_merge.md) and section 3 (interaction with S3.5's later
merge). The anchors are applied with exact-match checks by S3.6b's edit script (6b_notes.md lists it); every anchor applied on
I6's tree. UNCERTAINTIES.md also gets the rows of log/roadmap/S3/6b_uncertain.tsv (U-1380..U-1384).
Widget_fillRect: S2's body wins (S2_S3_SPLIT.md); 6_merge.md section 2 deletes the pixel family's line when S2's reports /
dialogs family is in the tree.

## CLAUDE.md

6_merge.md section 5's paragraph with the sentence 6b_merge.md section 4 replaces.

## Gate (6b_notes.md section 4; S3.6's census numbers in 6_notes.md sections 4b / 4c)

- test_gfx_hud 34 passed (7 opt-in skipped), test_gfx_pixel 17, test_gfx 25, x64 and Win32; port static 6.
- segments 205 / 205 x64 and Win32, 0 digests changed against the committed tables.
- boot: HEAD's 61 at 58 / 2 / 1, 0 of the 70 committed rows differ.
- HUD opaque gate: 368 / 368 in-game rows clean of S3.6 reasons (framegate --no-live; U-1383), no row worse than S3.6 in any region.
- employee panels: emulator oracle (SG_EMU_ORACLE=1) 240 / 240 golfer-list and 239 / 239 detail-panel call logs equal the
  original's (1 case faults in the original); controls 11-15 RED; ui_tour `people` bottom panel 1,053 -> 0 px.
- controls: S3.6 7 RED, 4 blind measured (U-1350, U-1351, U-1357); S3.6b 5 / 5 RED, cut-geometry control 3 un-blinded.

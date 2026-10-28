# S3.5b merge: S3.5 rebased onto I6 + wave 4, and the animal figure

Own files (log/roadmap/S3/5b_files.txt): port/src/sim/gfx_scene.cpp, port/include/portsim/sim/gfx_scene.h (unchanged),
port/test/test_gfx_scene.py (copy as they are; they replace S3.5's versions, which are a strict subset).

Base the step was built and measured on (log/roadmap/S3/5b_mirror.py, port/build/S3_5b_src): S3.6b's source root
port/build/S3_6b_src = I6's tree (HEAD b3d61e41 + S6 wave 2 + S3 waves 2 / 3 + S2 wave 5) + wave 4 (S3.6 + S3.6b), then S3.5's
three files, then the shared-file edits below, each applied with an exact-match check (every anchor found exactly once).

## 1. Changes against 5_merge.md

- Edits 1-11 of 5_merge.md: apply unchanged, in the order of 5_merge_edits.py (EDITS, then EDITS_AFTER_S2W5, then EDITS_HUD). On
  this tree every anchor occurs once: the hud edits (9 and 11) anchor on S3.6b's final gfx_hud.cpp / gfx_hud.h, edit 10 on I6's
  gfx_reports.cpp, edit 5 on menu.cpp with S3.4's flic hook already in it (MERGE_w2 edits 2a..2c are in I6's tree, so
  5_mirror.py's W2_MENU step is not needed). Nothing for gfx_body.h or gfx_families.inc beyond the `scene` line of edit 1: the
  `reports` / `hud` / `pixel` lines are already there.
- Note 12 (Widget_fillRect registered twice) is closed by wave 4: 6_merge.md section 2 removes the pixel family's line, and
  test_gfx's test_family_table_registers_selftest passes (25 / 25 on this tree, x64).
- No file of 5_merge.md's EDITS changed; 5b_mirror.py is the replacement of 5_mirror.py (I6 + wave 4 base).

## 2. New shared-file edit (log/roadmap/S3/5b_edits.py `EDITS`, exact text)

The animal's standing figure (U-1349 b, section 3 of 5b_notes.md): placeAnimal queues a sprite the port dropped.
1. port/src/sim/golfers.cpp: `#include "portsim/sim/gfx.h"` after `#include "portsim/sim/bridge.h"`.
2. port/src/sim/golfers_world.inc, place_animal: replace
   `      gs.g_animalCell = cell;                 // drawing state (the sprite queue itself is the renderer's)` + `      (void)n;`
   by the same first line without the parenthesis and, for an attached graphics backend only,
   `gl::call_peer(ctx, "enqueueSprite", {sx, sy, sy, 0x1e, 0x28, cell, 4, (n + 0x9c) | 0x100})` (guarded by
   `g_gfx_attached` and `gl::registered("enqueueSprite")`). Without a backend nothing changes: the corpus vectors and the
   simulation never see a sprite queue write.

Not applied (no oracle, U-1385): the spawn zoom-in of drawWorldObjects (an animal record with flags < 0) is a dropped
Surface_drawSprite; the transcription is kept in 5b_edits.py `EDITS_SPAWN_NOT_APPLIED`.

## 3. UNCERTAINTIES.md

Append the rows of log/roadmap/S3/5b_uncertain.tsv (U-1385 .. U-1387) and set U-1349 to `partial` with: `(b) resolved by S3.5b: the
figure is placeAnimal's queue entry (an animal record's standing figure), queued from golfers_world.inc; (a) the green ring /
grey lines stay S3.8's`.

## 4. CLAUDE.md paragraph (after the S3.6 / S3.6b paragraph)

Roadmap S3.5b, S3.5 rebased (2026-10-25, log/roadmap/S3/5b_notes.md, 5b_merge.md, U-1385..U-1387): S3.5's scene family on the
committed tree plus S3 waves 2-4 (5b_mirror.py), the exact queue oracle re-measured on all 585 recorded checkpoints
(`5_sprite_gate.py --queue --all`, port/corpus/scene): 584 have a port copy (cov_r_s2_screens end stops the port's boot), 47,950,391
scene px judged, 29 checkpoints with pixels beyond one level (was 32): the five cov_r_bio rows that were unjudged in the S3.5 base
now run and hold only the golfer-selection ring (S3.8's), the others are rings, path lines and isolated pixels under blends. The
standing figure the port missed (cov_k1_animals bought / menu2, cov_b_inject end) is placeAnimal's queue entry
(`enqueueSprite(sx, sy, sy, 0x1e, 0x28, cell, 4, (n + 0x9c) | 0x100)`, an animal record drawn with the golfer palette path),
dropped before and now queued for an attached backend (golfers_world.inc): 187 / 216 / 92 px beyond one level -> 0, control 13
(an entry past the golfer table draws nothing) RED; test_standing_figure_equals_the_original / _negctl_red in test_gfx_scene.py.

## 5. Gate (numbers in 5b_notes.md section 4)

```
~/bin/py -3.12 log/roadmap/S3/5b_mirror.py --w32
cd port/build/S3_5b_src && SG_PORTSIM_DLL=<x64 dll> ~/bin/py -3.12 -m pytest port/test/test_gfx_scene.py
SG_PORTSIM_DLL=<x64 dll> ~/bin/py -3.12 ../../../log/roadmap/S3/5_sprite_gate.py --queue --all --jobs 2 --keep <dir> --out <tsv>
~/bin/py -3.12 -m port.test.integrator segments --dll <dll> --jobs 4 --out <tsv>   (Win32: --backend win32 --dll32 --runner32)
~/bin/py -3.12 -m port.test.boot --all --jobs 3 --out <tsv>;  log/roadmap/S3/6b_boot_compare.py <tsv>
```

## 6. S2 test expectations that S3.5's hand-over changes (lead's request)

On a base without S3.8, S3.5's edits 8 (Image_draw) and 10 (Surface_drawSprite hand-over) and the registered `addSprite` body break
S2's tests, because each asserted an `unsupported` report for a draw the original makes and the scene family now draws (the original
jgld bodies behind them are judged by test_gfx_scene's emulator oracle). The pairs are in log/roadmap/S3/5b_edits_tests.py
(`EDITS_TESTS`, exact text, run it to apply):
- test_gfx_routing.py `test_image_draw_at_another_scale_is_unsupported` -> `..._is_drawn_by_the_scene_family`: no report and pixels
  at scale (2, 2, 1). The old scale (1, 1, 5) draws nothing by the original's arithmetic: the 4-column image becomes
  (int)(4 * 0.2) = 0 columns (the skeleton in gfx_scene.cpp), so it is not a case that can show a drawing.
- test_gfx_dialogs.py and test_gfx_reports.py `test_draw_sprite_other_alpha_is_unsupported`: the alpha-0.5 report is gone (drawAlpha's
  window bodies draw it, 30 / 30 emulator cases); the reports one no longer asserts an empty root.
- test_gfx_reports.py `test_frames_report_the_unmodelled_sprites`: `no body: addSprite` is no longer logged (the body exists, 40 / 40
  emulator cases); the log still carries the other reasons.
- test_gfx_reports.py `_control`, mode 8 (Surface::callBounds leaves the clip narrowed): on rep_financial the control no longer changes
  a judged pixel (S3.5's sprite draws set their own clip), so mode 8 must keep its effect on at least one of its six checkpoints
  instead of all six; every other mode keeps the all-checkpoints rule.
Results on the rebased tree with these four edits: test_gfx_routing 35 / 35, test_gfx_dialogs 32 / 32, test_gfx_reports 36 / 36, x64
and Win32 (logs 5b_pytest_test_gfx_*.log, 5b_pytest_w32_*.log); test_gfx_scene 118 / 118 still passes (it does not touch these tests).

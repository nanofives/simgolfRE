# S3.5b notes: S3.5 rebased, the queue oracle re-measured, the animal figure

Worker: claude2 sonnet (S3 lead cmvmns8sn3fsbo115g4dgm1nf), 2026-10-25. No game run (the recordings in port/corpus/scene exist).
Builds: port/build/S3_head2/port/build/S3_5b (x64), S3_5b_w32 (Win32), S3_5b_probe (development probe build). Source root
port/build/S3_5b_src (log/roadmap/S3/5b_mirror.py).

## 1. The rebase

Base = S3.6b's source root port/build/S3_6b_src: I6's tree (b3d61e41 + S6 wave 2 + S3 waves 2 / 3 + S2 wave 5, snapshot sha in
S3_6b_src/I6_SNAPSHOT) with wave 4 (S3.6 + S3.6b) applied. 5b_mirror.py copies it, puts S3.5's three own files over it and applies
5_merge_edits.py's EDITS, EDITS_AFTER_S2W5 and EDITS_HUD, then 5b_edits.py. Every anchor was found exactly once (no change to
5_merge_edits.py; 5b_merge.md section 1 lists what differs from 5_merge.md: menu.cpp already holds S3.4's hook, the family lines
`reports` / `hud` / `pixel` are in, note 12's Widget_fillRect duplicate is gone). x64 and Win32 both build.

## 2. Gates on the rebased tree, with the figure edit (final build)

- test_gfx_scene.py: x64 118 passed (5b_pytest_x64_final.log, 741 s), Win32 118 passed (5b_pytest_w32.log, 1,573 s). S3.5's 114
  plus 3 figure checkpoints and 1 control (section 3). Before the figure edit: x64 114 passed (5b_pytest_x64.log).
- Queue oracle (`5_sprite_gate.py --queue --all`, 585 recorded checkpoints, 66 scenarios):
  | run | with a port copy | scene px judged | beyond one level px | rows with beyond-one-level px |
  |---|---|---|---|---|
  | S3.5 round 2 (old base) | 582 | 47,754,472 | 2,818 | 32 |
  | rebased, no figure edit (5b_queue.tsv) | 584 | 47,950,271 | 2,833 | 32 |
  | final (5b_queue_final.tsv, images 5b_img/queue) | 584 | 47,950,391 | 2,338 | 29 |
  The 585th row, cov_r_s2_screens `end`, has no port copy (U-1386). Rows that changed between the rebased and the final run: exactly
  cov_k1_animals bought (187 -> 0 beyond one level), menu2 (216 -> 0) and cov_b_inject end (92 -> 0).
- The four cov_r_bio rows S3.5 could not judge (its base lacked S1's bio port) run now: five rows, bio1_done / bio2_done / editor /
  exit / panel, 48..50 px beyond one level each; cov_r_bio panel viewed: the golfer-selection ring (green circle) the port does not
  draw (S3.8, U-1349 a); 37..40 of the 48..50 px are clearly green (5b_queue_kind.tsv), the rest its edge. cov_r_bio loaded / end
  have 0. cov_b_inject2 debug_overlay went from 34 to 0 beyond one level between S3.5's base and this tree (not examined further).
- The other 24 rows are the S3.5 set: the ring and grey lines (cov_a_golfer, cov_a_build terrain_*, cov_a_keys3, cov_a_play end /
  n_yes: viewed, a grey curved line), single pixels under blends (cov_b_career_types, cov_k1_label, cov_k2_build built: viewed, the
  end of the white path line). U-1387 states what was viewed and what was classified by colour only.
- Segments: 205 / 205 PASS x64; Win32 204 PASS in the full run and course_season/08 FAIL at record 51 of 612 once, PASS 612 / 612
  twice on re-run with the same digest (5b_segs_cs08_1.tsv / _2.tsv; 5b_segs_win32.log holds the failed line); with the re-run row
  taken, 0 digest or status differences from the committed tables (log/roadmap/merges/S2S3-5/segs_*.tsv) on both and x64 = Win32
  (5b_segs_compare.log). The cause of the one failure is not determined (it did not repeat).
- Boot (`port.test.boot --all --jobs 3`, 5b_boot_x64.tsv): HEAD's 61 at 58 PASS / 2 KNOWN / 1 NO_FRAMES, 0 of the 70 committed
  rows differ (5b_boot_compare.log).
- Other files, x64, one per process: port test_static 6 / 6, test_golfers 60 / 60 (place_animal's vectors: the edit queues a sprite
  only for an attached backend), test_gfx 25 / 25, test_tick 250 / 250 (logs 5b_pytest_*.log).

## 3. The standing figure (U-1349 b)

What it is, from the recordings and the port's own queue log (5b_probe.py, SG_GFX_SCENE_LOG with a per-entry bbox of the root
pixels it changed; the logs were 86 MB and are deleted, seg_1032.txt keeps the queue of cov_k1_animals bought):
- cov_k1_animals bought: the original draws a standing person (grey hair, white shirt, dark trousers) at 413..431 x 331..357 on the
  path by the clubhouse; the port draws nothing there. The port's 157 queue entries at that call: the clubhouse (a8 65, bbox
  336..446 x 270..362) and a tree cover the box, nothing else reaches it. The neighbouring frames (queue calls 1031 and 1033) differ
  in 2,544 / 2,652 px against the original, call 1032 in 187: the call count is aligned and the figure is stable, not a moving
  object that a one-frame shift hides. The same figure is missing at menu2 and, in another outfit (orange shirt), at cov_b_inject end.
- It is not a golfer record (g_golfers 0..9 of the port's frame equal the original's segment start image: records 4..9 sit at tile
  (32, 24) with screen -1), not a walker (g_walkers: one active record, Gary, kind -6; the walker selection word +0x10 of the
  engine words at f_w_4c2e00 is -1 in the image and in the port, so drawFlyingBall's ghost addSprite does not run), not the
  0x13c sheets (their cells are 1 x 1 in the original's start image too: cells 43849.. = (1, 1) at +0x10 / +0x14).
- It is an ANIMAL record: re/analysis/sim/004017d0_placeAnimal.md (placeAnimal, 0x004017d0): on screen with a wait below 0x80 the
  function queues enqueueSprite(sx, sy, sy, 0x1e, 0x28, cell, 4, (n + 0x9c) | 0x100) at 0x004018dc, cell = cellAt(sprite, frame,
  (rotation - heading - 2) & 7) with sprite = type + sprite_base. The cov_k1_animals scenario's own text (re/tools/scenario.py:
  "spawnObject ... when a home site is bought, tickAnimals runs placeAnimal on every live record") and the image (g_animalRecords
  record 0 live: type 7, tile (35, 19)) agree. The port's place_animal (golfers_world.inc) kept the cell write and dropped the
  enqueue ("the sprite queue itself is the renderer's"); (n + 0x9c) | 0x100 is a golfer-palette entry past the golfer table, drawn
  by drawQueuedSprite's golfer arm (composeGolferSprite on record 0x9c + n, whose bytes are the memory after the golfer table).
- Edit (5b_merge.md section 2): place_animal queues that entry through the registered enqueueSprite when a graphics backend is
  attached and enqueueSprite is registered (nothing changes otherwise: test_golfers 60 / 60, segments unchanged).
- Result: cov_k1_animals bought / menu2 and cov_b_inject end have 0 pixels beyond one level in the figure's box and 0 differing
  pixels at all in it (the probe image against the original's: no pixel beyond 8 / 255 in any channel anywhere, 119 pixels differ at all, none in the box).
- Evidence in the suite (test_gfx_scene.py): test_standing_figure_equals_the_original x3 (the boxes 405..440 x 320..365 for both
  cov_k1_animals checkpoints, 435..470 x 300..345 for cov_b_inject end: 0 px beyond one level of the original's root at the
  queue's return); control 13 (`g_negctl_gfx_scene` = 13: an entry with the golfer flag and a palette index of 0x19c or more draws
  nothing, as the port did before) reads RED: more than 100 px in the first box (test_standing_figure_negctl_red; the pre-edit
  queue gate row, 187 px, is the same state measured on the full gate).
- Blind: control 13 judges the draw path, not the enqueue call; the enqueue call is judged by the same three checkpoints (without
  it the box is missing 187 / 216 / 92 px, 5b_queue.tsv). A wrong a8 (n + 0x9c without the 0x100 flag) or a wrong cell is not
  separately controlled, and n (the placed-object scan of place_animal) was not recorded at these three frames: if it is 0 there,
  the `n` term is judged only by the corpus vectors of place_animal, which do not see the queue entry.

## 4. Not done / leads

- The spawn zoom-in (U-1385): transcribed, not applied, no oracle.
- cov_r_s2_screens end has no copy (U-1386).
- The 29 rows (U-1387): S3.8's ring and line draws, then the gate again.
- test_gfx_scene's two new frame tests boot the scenario once each (about 3 minutes each under load); the figure tests share no
  boot with the other frame tests.

## 5. Reproduce

See 5b_merge.md section 5. Probe: `cd port/build/S3_5b_src && SG_PORTSIM_DLL=<probe dll> ~/bin/py -3.12 ../../../log/roadmap/S3/5b_probe.py
<scenario> <queue call> <out prefix> [--walkers]`; classification: `~/bin/py -3.12 log/roadmap/S3/5b_queue_kind.py`.

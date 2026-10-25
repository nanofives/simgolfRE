# S6.8 notes: D52b3, generateRandomCourse's evalPlacementArea searches (fixed profile, G3)

Status: DONE (probes, port switch, tests, wave-1 gates, emulator oracle 115 passed).

## Probes (log/roadmap/S6/8_probes/, harness plib.py copied from 6_probes)
evalPlacementArea 0x0040db90 is called four times in generateRandomCourse 0x00470a60 (list_calls.py):
- 0x0047160b: the clubhouse search (D52b2, wave 1).
- 0x00471764: loop A, 0x0047172e..0x0047176f, `je 0x0047172e` at 0x0047176f. x = 5 + rand(0x28), y = 5 + rand(0x28),
  evalPlacementArea(x, y, 2, 4). Reached only for location 4 (jump table 0x00472244[4] = 0x0047169f, then `cmp eax, 4` at
  0x00471720; location 3's slot is 0x00471f89, so the `cmp eax, 3` at 0x00471725 never sees 3 here: jt.py).
- 0x004718dc: loop B, 0x004718a4..0x004718e7, `je 0x004718a4` at 0x004718e7. x = rand(0x28) + 3, y = rand(0x28) + 3,
  evalPlacementArea(x, y, 5, 0xf). Reached for locations 5, 6, 8, 12, 13, 15 (table slots = 0x004718a4). Its (x, y) are not
  used after the loop (0x004718e9 reads g_difficulty and goes on to the object pass): the loop is a gate.
- 0x0047195e: inside the object pass (`p3` loop, bounded by 8 - difficulty); a reject skips that object. Not a search.

d52b3_placement.py (sandbox_basic/00 image, course record 9, course type 3, difficulty 1, RNG word 0x12345678, location id
byte 0x571ff4 + 9 * 0x2e overridden). At the loop entry the hook ORs 0x8080 into every flag word of the 50 x 50 grid
(0x53caf0): evalPlacementArea returns -1 for any interior cell with those bits (`test word [esi*2+0x53caf0], 0x8080` at
0x0040dc7f), so no draw is accepted. Results, budget 300,000,000 instructions (d52b3_placement.log):

| loop | location | control (grid as generated) | window (hang grid, 8 x 8 block rows/cols 20..27 left clear) | hang grid |
|---|---|---|---|---|
| A | 4 | completed, 1 iteration | completed, 10 iterations | budget exhausted: 1,236,965 iterations, 1,236,965 evalPlacementArea calls |
| B | 5, 6, 8, 12, 13, 15 | completed, 1 iteration each | completed, 145 iterations | budget exhausted: 1,126,725 iterations, eip 0x0040dbe4 (in evalPlacementArea) |

Loop A's hang stopped at eip 0x004a6037 (__ftol, called by Random::range); the in-loop flag in the log does not include
__ftol, the iteration count shows the loop. Both loops hang; none could not be made to hang.

## Port (port/src/sim/terrain_gen.cpp)
New switch `generateRandomCourse.placementBound` (model_defects#52, default 1). A second switch, not searchBound, so
wave 1's switch keeps exactly the D52b1 / D52b2 meaning and its profile row and tests are untouched; D52b3 gets its own
profile row. Same probe as wave 1: after 65,536 consecutive misses the port evaluates every cell the draws can reach;
only an empty candidate set fires. At 1 that throws NoReturn (unchanged); at 0:
- loop A gives up and the type-4 object is not stamped (the original has no `not placed` arm: port choice, U-1598);
- loop B gives up and the whole feature is skipped (the object pass does not run; port choice, U-1599).
No change on any state where the original completes (the draw sequence is untouched until the probe proves no exit).

## Probe on the port's own test images (d52b3_port_images.py, logs d52b3_port_images.log / _plain.log)
Image = test_terrain_gen._fixed_image(n 3, setting 0, course type 0, difficulty 1, relief 1) with the location byte set; BLOCKED
= every flag word 0x8080 except the 4 x 4 block at the clubhouse corner the plain location-4 run picks (the clubhouse site is
4 x 4: port_explore.log, with a 4 x 4 or 5 x 5 clear block the clubhouse is placed and the loop hangs; with 6 x 6, location 4
finds a 2 x 2 site beside it). ORIGINAL in Unicorn on these exact bytes:
- BLOCKED, location 4: budget 300 M exhausted, 921,011 loop A iterations; locations 5..15: 933,493 loop B iterations each.
  Port: error -10 (NoReturn) at 1, completes at 0. Location 3 on the same BLOCKED image: the original and the port at both
  settings complete with equal eax and data image (so the -10 is the placement loop, not another search).
- PLAIN, locations 3, 4, 5 (300 M) and 6, 8, 12, 13, 15 (4 G; they needed more than 300 M after the loop): the original
  completes, and eax plus the whole data image equal the port's at placementBound 1 AND 0.

## Port details
- `miss_placement` = wave 1's `miss_bounded` with the new switch; loops A and B call it instead of `miss`.
- negctl modes 4 (loop A gives up but still stamps at the last draw) and 5 (loop B gives up but runs the object pass).
- Wave 1's `miss_bounded`, `miss`, searchBound and its kDefects row are unchanged.

## Gate (own build port/build/S6_8 from a private mirror port/build/S6_8_root = git HEAD of port/ + re/model/include with
## this terrain_gen.cpp: a sibling's golfers_pro.inc fails /WX in the shared tree, C4456 at line 76)
Reproduce: copy terrain_gen.cpp into port/build/S6_8_root/port/src/sim/, cmake --build port/build/S6_8 --config Release
--target portsim_c, then with SG_PORTSIM_DLL=port/build/S6_8/Release/portsim_c.dll:
- port/test/test_defects_gen_w2.py: 30 passed (8_test_w2.log).
- port/test/test_defects_gen.py: 13 passed; port/test/test_terrain_gen.py: 14 passed, 101 skipped (8_gate_*.log).
- port/test/test_static.py: test_no_original_addresses_in_port_code passes; test_every_module_file_is_listed fails on
  siblings' unlisted modules (audio_*, video, textlab, fttext*), not on this step (8_static.log).
- SG_EMU_ORACLE=1 test_terrain_gen.py: 115 passed in 373 s (8_gate_terrain_gen_oracle.log): the original under Unicorn equals the port at switch 1 on every random image.
- Segment / boot gates not run: default behaviour (switch 1) changes only where the old code threw NoReturn after the same
  probe, so no completing run can change; the integrator runs them on merge.

## Controls
- Hang arm: 7 locations, -10 at 1 / completes at 0 (a body that applied the fixed arm at 1 reads RED there).
- Completing controls: 8 locations equal at 1 and 0 and pinned to digests equal to the original (PLAIN_DIGESTS).
- Wrong bodies RED: negctl 4 on location 4, negctl 5 on locations 5 and 15 (against FIXED_DIGESTS), wave 1's negctl 1 on the
  plain controls of locations 4 and 5. 5/5 RED.
- Blind: a probe that gives up without checking the candidate set (after 65,536 misses while a cell is accepted) cannot be
  seen: a reachable accepted cell among at most 1,764 is drawn within 65,536 draws with probability above 1 - e^-37. Judged
  by reading `miss_placement` (the candidate lambda is the full draw range of each loop).
- FIXED_DIGESTS pin the port choice (U-1598, U-1599), not the original.

## Open
- U-1598 / U-1599: the fixed arms are port choices (skip the object / the feature).
- Merge rows: 8_merge.md.

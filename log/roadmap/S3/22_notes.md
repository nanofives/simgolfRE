# S3.22 notes (wave 8, W8.3 classification + W8.6 body removal)

Worker: claude2 (Sonnet), 2026-10-30. Base f2dff003. U-1323..U-1329 not used (no new open item that is not a W8.4 task below).

## Task B (W8.6): showCharacterEditor.bio removed
- Body, its entry in kBodies and its header comment line removed from port/src/sim/gfx_chareditor.cpp. A search of port/ found no other
  reference (chareditor.cpp calls only .setup / .background / .teardown; no test names or counts it).
- Edits: 22_make_edits.py -> 22_edits.py (3 anchored hunks), 22_assemble.py (`--dst S3_22` builds `w7_assemble.py --plain --commit f2dff003` then applies;
  `--root <tree> --only-22 [--dry]` for an existing tree; a tree that already holds the edit refuses: "already applied").
- Gate (port/build/S3_22, heavy lock held): x64 and Win32 build 0 errors (22_build_x64.log, 22_build_w32.log); `22_count_bodies.py`: gfx.check 0,
  137 bodies, no `showCharacterEditor.bio`; test_gfx + test_gfx_s3_19 x64 40 passed / 6 skipped (opt-in oracle); test_gfx Win32 25 passed;
  segments x64 205 / 205 PASS; boot x64 66 PASS / 2 KNOWN / 2 NO_FRAMES (as the baseline). Logs 22_pytest_gfx_*.log, 22_segs_x64.log, 22_boot_x64.log.
  Reproduce: `bash log/roadmap/S3/22_gates.sh` (from the lock holder). Jobs 1 throughout.

## Task A (W8.3): the 50 host-GL FAIL rows
Tool `22_rows.py` (one framegate boot per scenario with host_gl=True, so terrain is exact; captures the port frame by wrapping
framegate.region_numbers, the reference from port/corpus refs; per row: diff components with boxes, q_before / q_after = differing px where the
original's final frame equals / differs from its root at renderSpriteQueue's return in port/corpus/scene) -> `22_rows.tsv`; images
`22_img/<scenario>.<checkpoint>.png` (port | original | differing px, 50 files); `22_make_causes.py` -> `22_causes.tsv`. The px and beyond columns
equal census_auto.tsv for all 50 rows (e.g. holes 2829, y_inj3 847, hire 1397).
Rows with a scene root: all 50 (q columns filled). q_after > 0 means a layer drawn after the sprite queue (HUD, text, overlay lines, modal dialogs).

### Groups (rows, cause, proposed fix, expected rows to PASS, cost, game run)

1. **Overlay line / ring edge, one level: 31 rows** (championship tour_0180; cov_a_build x4; cov_a_champ x4; cov_a_golfer x11; cov_a_keys x3; cov_a_keys3 panel;
   cov_a_play shot_keys; cov_b_decor; cov_r_bio x2; cov_r_tutorial p18; ui_tour / ui_tour_v practice). 1..52 px, boxes 1x1 to 20x20, max difference 1-2 levels
   (beyond one level in 7 of them: cov_a_champ x3 with 3 px, cov_a_golfer panel_3 / mod_11 with 2, cov_a_keys k_render_dot 3 / k_render_slash 11, cov_b_decor 9, ui_tour 2,
   p18 1). Images show the differing px along thin lines and rings (hole pin lines, shot arc, green selection ring, flight rings). Same class as S3.8's
   U-1370 residue ("footprint-edge pixels under 0.13 coverage"). Fix: refine overlay.rasterLine coverage at line ends / diagonal edges, judged by
   glreplay on synthetic drawLine sequences (no game run; S3.8's tool). Expected: up to 24 rows PASS (the 0-beyond ones still need exact one-level
   equality under the census's definition, so the realistic count is the rows whose every px the model fixes; unknown until tried). Cost 4-6 h, uncertain. No game run.
   Not a missing body: nothing to add, only coverage numerics.
2. **Terrain edge at the map corner: 8 rows** (cov_r_s2_screens course, radial_hover 65 px; rep_comments / histograph / financial / report1 / shortcuts 20 px;
   rep_roster 11 px). One diagonal box 781..799 x 451..466. course / radial_hover are q_before 65: the original's root at the queue return already has its
   pixels, so the difference is in the terrain pass, not 2D; the report rows are the same edge seen through the dim band (7 q_before + 13 q_after).
   The scenario has no S3.2 terrain recording (log/p5/terrain has none for cov_r_s2_screens), so whether the port culls a triangle or draws the rim shifted is not
   measured: needs the original's exit.bin for this scenario or glreplay of the pass (one game run to record). Fix by terrain_render's draw list at the map
   boundary. Expected 8 rows. Cost 3-5 h. Game run: yes (a terrain recording of cov_r_s2_screens, S3.2's recorder).
3. **Animated scene sprites differ: 2 rows** (cov_b_holes18 holes 2,829 px, cov_k3_camera2 y_inj4 414 px). Components: fountain sprays (416 / 381 / 359 px, 411 px at y_inj4),
   water ripples 820 px at the left shore, 26-px 5x12 flag poles. 2,815 / 414 px q_before: layers up to and including the queue. The port picks a different animation
   frame than the original (or places the cell differently). Fix: compare the queue entries of the port with the original's sprite queue at these frames
   (`5_sprite_gate.py --queue` data per entry does not exist; the oracle holds only the final root), then the frame-index source of those entries (fountain /
   ripple / flag animation words). Expected 2 rows, plus the 6 older one-off sprite rows S3.15 noted. Cost 4-6 h. Game run: yes, unless the queue entries can be derived from the code (a queue entry log from the original).
4. **Scene sprite missing, translucent ellipse: 1 row** (golfer_stories season_end 685 px, box 60,70..102,90, max 4 levels, 653 q_before). A lighter ellipse over the
   water under the logo that the port never draws. In the queue root, so a sprite (drawAlpha window). Fix: identify the queue entry (cell and alpha) from the
   golfer_stories run; no body exists to name. Expected 1 row. Cost 2-3 h. Game run: yes (queue log).
5. **Overlay line missing: 1 row** (cov_k3_camera y_inj3 847 px, box 398,275..431,496, 724 q_after). The original draws a thin antialiased red connector from the `Benches` label
   diamond to the amenities panel item; the port draws the diamond and no connector. Candidates are the drawMapLine call of drawAmenitiesPanel's hover tip or
   its endpoint selection (S3.7 / S3.8 notes); which call is not established by these images. Fix: record the original's Terrain::drawLine calls for this checkpoint (gl_rec)
   and compare with the port's overlay.logWrite log. Expected 1 row. Cost 2-3 h. Game run: yes (a GL stream recording of cov_k3_camera), or a static read
   of the tip code.
6. **Body missing, addSprite FLIC cells: 1 row** (cov_r_s2_screens hire 1,397 px: four boxes 473 / 361 / 314 / 249 px; 1,376 q_after). Known since S2.8. Fix: the dialog's
   addSprite calls of the employee cells (sheet cells, flic module decodes them already, S3.4), drawn after the queue inside showHireEmployeeDialog. Expected 1 row.
   Cost 3-4 h. Game run: only if the cell and position are not derivable from the dialog code and the pinned images (the sprite calls were not recorded).
7. **Other, 4x4 marker after the queue: 6 rows** (cov_b_inject zoom_box / forced_events / end, cov_b_inject2 zoom_box / forced_events / end). 15 px in one 4x4 box, max 31
   levels (port black, original pale pink), all q_after, box position varies (142,89), (568,355), (576,365), (272,61). Terrain not involved (these were the S3.15
   "terrain after the recorded exit" rows only under GlRaster). Body not identified: a 2D draw after the queue of a 4x4 dot beside a label. Fix: find the call by a
   call-log run of the original at these checkpoints (C3FT_CALL_LOG on drawing functions) or by diffing the port's draw calls (gfx.log) against the exe's draw
   sites near a label. Expected 6 rows. Cost 3-4 h. Game run: yes, unless the call is found statically (a call log).

Order for W8.4 by rows per hour: group 7 (6 rows), group 2 (8 rows, one recording), group 6 / 4 / 5 (1 row each), group 3 (2 rows), group 1 last (31 rows, uncertain). Rows
that cannot reach PASS without group 1: all 31 of it.

## Pitfalls hit
- framegate in the f2dff003 tree has no snap_sink (the shared tree has it); 22_rows.py wraps region_numbers instead (15_split_all's method).
- The cause column for group 1 is assigned by pattern plus 6 inspected images (cov_a_golfer panel_0 and tee_300, cov_a_keys k_render_dot, cov_a_build terrain_03,
  cov_a_champ champ_render, cov_b_decor terrain, cov_r_bio panel, cov_a_play shot_keys); the other rows of the group were not opened one by one.
- Heavy runs used: build x64 + Win32, test_gfx x64 / Win32, segments x64, boot x64, two 22_rows runs (host GL). Lock released.

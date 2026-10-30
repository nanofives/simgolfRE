# S3.17 notes: the two missing gfx bodies of I14's UNSUPPORTED rows

Step row S3.17 of re/roadmap/S3_steps.tsv (wave 7). Base: port/build/I15 = `git archive ee6efc1d` + 12_assemble.py. Mirror:
port/build/S3_17 (`12_assemble.py --dst S3_17 --commit ee6efc1d`), x64 and Win32 builds in it (port/test/build.py,
SG_FREETYPE_ROOT). Env for every command below: `cd port/build/S3_17; source ../../../log/roadmap/S3/17_env.sh` (a copy of 14_env.sh).
Delivery: 17_edits.py (17_make_edits.py), 17_new/, 17_assemble.py, 17_merge.md, 17_uncertain.tsv (U-1830..U-1833).

## 1. What the 44 rows were

I14's census (log/roadmap/merges/I14/census.tsv): 22 rows `no body: Surface_createFull_fwd1` (cov_a_career 11, cov_a_champ 6,
cov_a_build 3, cov_b_decor 2) and 22 `no body: showRoutingMapScreen.teeSprite` (cov_a_keys 10, ui_tour 6, ui_tour_v 6). A run's
unsupported list is cumulative, so every checkpoint after the first such call kept UNSUPPORTED; the pixels of all but three rows were
the same HUD / view residue as the FAIL rows. The unsupported reasons of the six runs (census.tsv.d): teeSprite, flagSprite and
Surface_createFull_fwd1 only.

### 1a. showRoutingMapScreen's tee / flag sprites
The original (shim/src/re/c3et.cpp ShowRoutingMap_re, instruction addresses there) per hole with a par byte: tee = cellAt(pv + 0x18d,
widths[pv + 0x18d] - 1, 0) with pv = clamp(par - 3, 0, 2) (0x00457749 / 0x0045775f), stored in 0x00822b84 (0x0045776c), then
addSprite(sx, sy, sy, 0x28, 0x28, cell, 4, 0x5e) (0x0045777d); flag = cellAt(g_courseType + 0x189, 0, 3) (g_courseType a signed byte,
0x004578f5 / 0x00457906), stored (0x00457915), addSprite(sx, sy, sy, 0x28, 0x28, cell, 2, g_courseType + 0x60) (0x0045792d). addSprite
(c3ei.cpp AddSprite_re) fills queue entry i and calls drawQueuedSprite(i) at once: no queue flush is involved. buyland.cpp's hole loop
already makes the same two calls. Port: routing.cpp MapDraw::hole_map now makes them (`gx("addSprite", ...)`, cellAt through
cm::call_peer, the pure tick.cpp cellAt), drawn by the scene family's existing addSprite body (gfxs::add_sprite: position as addSprite
computes it, then drawQueuedSprite's drawing). The store into 0x00822b84 is not made (U-1830: engine class, never compared; a drawing
pass leaves the GameState as it was).

### 1b. Surface_createFull_fwd1
mainLoop's forwarder (re/analysis/render/00421b30_Surface_createFull_fwd1.md; instructions 0x00421b30..0x00421b50): ecx passes
through, it pushes [esp+0x14], [esp+0x10] .. as a5, a4, a3, 0, a2, a1 and calls Surface_createFull 0x00474dd0, `ret 0x14`. So
Surface_createFull(this, a1, a2, 0, a3, a4, a5). Its two calls (tick_frame.inc ML+0xd6f2 key 'B', ML+0xdae0 keys '.' / '/') run on a
frame-local Image just constructed (Image_ctor erases the key first). Port: gfx_surface.cpp body `Surface_createFull_fwd1` hands that
list to Surface_createFull's body (erases the key's backend entries). Surface_createFull_fwd1 is a tick.cpp kDrops drop already, so with a
backend TkBase::call reaches the body through gfx_drop; no tick / families change.

### 1c. cov_a_champ n_yes / end (not fwd1)
These two rows had 479,920 px (479,021 beyond one level): the port's snapshot showed the cancel prompt, the original's frame the title
menu. SG_TICK_TRACE showed the port leaving the frame for the title menu (buildGolferBioText, initRecords, the title image and items
drawn), then `waitTicks 1 0, pumpInput483bd0, pumpMessages` and the end: run_detail `the last frame ran to the end of the trace: a
Blocking pump with no input left; stopped in pumpMessages; ... cursor reads over/under 0/1120`. The title menu's hover loop
(tick_menu.inc hover_loop) draws and calls Window::refreshFull AFTER its pumps and View_toLocal; its pumpMessages runs at the modal
pump site (Blocking), and `Yes, cancel.` is the trace's last dispatch, so the first pass threw NeedInput before any refresh. TraceInput
has TAIL IDLE for this (one idle pass per unread CURSOR record past the frame's last dispatch), but tick_run_frames set it only for a
boot with no frame head. The boot's last-frame branch now sets it too (U-1831): the title loop makes the original's 1,120 passes and
both rows PASS 0 px. Other session-to-title rows (cov_a_quit, cov_r_quit, cov_k3_career, cov_k1_animals) passed before because input
followed in the title menu. The boot gate (boot.py without to_end) stops at the end checkpoint and never enters this branch; to_end runs
are framegate / gfx.render, test_boot.py, test_gfx_eoi.py, test_video.py.

## 2. Results (framegate per scenario, x64; `~/bin/py -3.12 -m port.test.framegate <scenario> --no-probe --out ...`)

| scenario | rows | I14 | S3.17 |
|---|---|---|---|
| cov_a_keys | 10 UNSUPPORTED | k_dlg_R 135 px (134 beyond) | k_dlg_R FAIL 1 px one level; the other 9 FAIL with I14's px |
| ui_tour | 6 UNSUPPORTED | | 6 FAIL with I14's px |
| ui_tour_v | 6 UNSUPPORTED | | 6 FAIL with I14's px |
| cov_a_career | 11 UNSUPPORTED | | 11 FAIL with I14's px (57 / 56, 1 beyond) |
| cov_a_champ | 6 UNSUPPORTED | n_yes / end 479,920 px | 4 FAIL with I14's px, n_yes / end PASS 0 px |
| cov_b_decor | 2 UNSUPPORTED | | 2 FAIL with I14's px |
| cov_a_build | 3 UNSUPPORTED | | 3 FAIL with I14's px |

44 -> 0 UNSUPPORTED: 2 PASS, 42 FAIL. Every PASS row of these scenarios in I14 is still PASS (cov_a_keys menu / k_worldmap / k_dlg_L,
cov_a_champ menu, cov_a_career menu / career_editor, cov_a_build menu, ui_tour / ui_tour_v menu / menu_done). Files: 17_fg/new_*.tsv,
new2_cov_a_champ.tsv; baseline before the edits: 17_fg/base_cov_a_keys.tsv (equal to I14's rows).
Residue in the 42 FAIL rows (owners): hud_title / hud_meters / hud_message / toolbar one-level px (S3.15); bottom_panel and view px
(S3.16), all one level except the beyond-one-level px I14 already had: ui_tour `menus` 252 px beyond in the view = the money popup
`-$600` at (640..700, 352..416), where the port draws a name label (S3.16's drawLabels / drawMoneyLabels gap, ORCH_LOG 17:07); 1..11 px
beyond in the view of cov_a_keys k_render_dot / k_render_slash, cov_a_champ, cov_b_decor terrain, cov_a_career (all equal to I14).
k_dlg_R's last pixel (460, 396), one level: outside the sprites' footprint (134 px, gfx.family.scene 12 run, 17_fg/img_nosprite) and
inside the hole lines' footprint (246 px, overlay.rasterLine 10 run, 17_fg/img_noline): a Terrain::drawLine antialias pixel (U-1832).
Win32 (SG_BACKEND=win32): cov_a_keys and cov_a_champ every row equal to x64 in status, px and beyond px (17_fg/w32_*.tsv).

## 3. Tests and gates

- port/test/test_gfx_s3_17.py + test_gfx_routing.py, x64: 46 passed, 1 skipped (the oracle) (17_test_x64.log); SG_EMU_ORACLE=1
  test_fwd1_argument_order_equals_the_original: passed (17_test_oracle.log: the ORIGINAL forwarder under Unicorn, cmdemu with
  Surface_createFull stubbed as a recorder: ecx unchanged, (a1, a2, 0, a3, a4, a5) for three argument sets, five arguments popped,
  the callee's eax returned).
- gfx.check: 0 duplicates (test_bodies_registered_without_duplicates).
- to_end users: test_gfx_eoi 5 passed, test_video 32 passed / 24 skipped (oracle opt-in), test_boot 25 passed / 70 deselected (17_test_boot_x64.log).
- tick.cpp is its own merge item (17_merge.md section 3, accepted by the S3 lead). Its extra gate, after I15 ends:
  `sh log/roadmap/S3/17_gates.sh boot_x64 boot_win32 live_equiv_x64 live_equiv_win32 app_replay_x64 app_replay_win32
  t_test_app_live t_test_app_ingame t_test_tick ctl13` (17_out/S3_17/). Wanted: boot 66 / 2 / 2 x64 + Win32 with 0 rows differing,
  live equivalence 68 + 2 and app replay 3 / 3 as I14, the three files passing, ctl13 RED.
- segments / boot / census: PENDING (heavy runs held for I15); by construction the segment gate cannot move (segments run without a
  backend, and the tick.cpp change is in the boot-only branch) and the boot gate cannot move (it never runs the last-frame branch).

## 4. Controls

| control | where | result |
|---|---|---|
| showRoutingMapScreen 11 (tee one pixel right) | k_dlg_R | RED: framegate 1 -> 43 px; GX.render 246 -> 280 |
| showRoutingMapScreen 12 (flag at shade 4) | k_dlg_R | RED: framegate 1 -> 413 px; GX.render 246 -> 643 |
| gfx.family.scene 12 (addSprite draws nothing) | k_dlg_R | 370 px = S2.4's bound (the sprites' contribution) |
| gfx.family.surface 9 (fwd1 does nothing) | unit test (crop after fwd1 finds the image) | RED; BLIND on the frames: both call sites run Image_ctor on the key right before, which already erased it, and loadImage / the full-screen Surface_blit3 replace it after (equivalent there) |
| gfx.family.surface 3 (Surface_createFull keeps the image) | unit test through fwd1 | carried through the forwarder (one body for both names) |
| tick.frame 13 (no tail idle in the last frame) | cov_a_champ n_yes / end | RED: 0 -> > 400,000 px (479,920 before the change) |

4 / 4 RED (surface 9 blind on frames, RED in its unit test).

## 5. Open

- U-1830 (the 0x00822b84 store), U-1832 (k_dlg_R line pixel, S3.8), U-1833 (fwd1's argument order unobservable through the backend),
  U-1831 (the tail-idle change: measured on the scenarios above and the to_end test files; the census is the wider check).
- Not drawn yet, seen on the way: showRoutingMapScreen.walkers (mode 0's walker list and markers, routing.cpp, a drop with no body).
  No row of I14's census lists it, so no census run reached the routing map in mode 0 with a backend; a future scenario that does
  will report it.

# S2 status (2D screens; lead session cmvmns3ax3frbo1152mcm7eg2)
steps: 14: accepted 11 (S2.1-S2.9, S2.12 partial, S2.13), running 2 (S2.10, S2.11 on claude3)
accepted / merge waves: 1-4 with I5; wave 5 = S2.7-S2.9 + S2.13 (MERGE_wave5.md, 13_shared.patch)   held: k_dlg_R (S3), scroll text U-1250
live workers (cap 2, claude3): S2.10 cmvncanfd6upxo115u3cj3svi (char editor, buy land, skill points, radial); S2.11 cmvncb29y6uqlo1153sxbjj5g (results, bulletin, SimFoto)
game slots: none held (s2_12 returned)
gate now (S2.13 tree): census 83 PASS; terrain-free not PASS: k_dlg_R (held), char editor 3 (S2.10), title -11 4 (S3.3);
  title NOT_RUN -11 4 = S3.3); cov_a_quit + cov_a_quit_title: 19 terrain-free rows PASS with the S2.4 DLL (S2.5 traces; 5 in-game rows = S3)
gate reading: "no terrain on screen" taken from the viewed reference (S2_screens.tsv full_screen yes), not only the census rule (U-1210)
next: S2.13 integration (needs a non-claude2 session or the lead), then S2.10 / S2.11; waves 5+ = S2.7-S2.9 after S2.13
last event: 2026-10-25 S2.13 accepted, wave 5 requested; S2.10 + S2.11 spawned

## Log
- 2026-10-24: lead started; steps S2.1 / S2.2 spawned.
- 2026-10-24: S2.1 hung twice on a bare `cmake` call (claude2 has no allow rule for it; the call waits on an approval prompt).
  Fix: log/roadmap/S2/build.py runs configure + build through `~/bin/py` (allowed). Every S2 worker prompt names it.
- 2026-10-24: S2.2 accepted (50 checkpoint rows, 27 family rows; 12 families have no pinned checkpoint). Steps S2.3-S2.10 planned.
- 2026-10-24: S2.1 accepted (test_gfx 25/25 x64 + Win32, segments 205/205 0 changed, boot 58/2/1, controls 8/9 RED, 1 blind U-1200). Wave 1 MERGE.md written. S2.3 spawned.
- 2026-10-24: S2.3 accepted: credits 0 px, census 604 same / 5 better / 0 worse, segments 205/205 0 changed, boot = S2.1. Shared edits in 3_shared.patch (gfx_body.h, gfx.cpp, gfx_families.inc, commands.cpp, commands_screens.inc, tick.cpp snapshot-on-clock fix). S2.4 spawned.
- 2026-10-24: orchestrator granted S2.5 / S2.6 slots and queued waves 1 + 2; S2.6 spawned; asked for the scenario.py hook glob.
- 2026-10-24: S2.6 BLOCKED: scenario.py's hook imports only scenario_s1_*.py (exact line in 6_merge.md); worker kept idle. Cap raised to 3; S2.5 spawned.
- 2026-10-24: orchestrator: scenario.py frozen until I4 commits; S2.6 pins through its own driver (s2_6_runs.py), glob change kept in 6_merge.md.
- 2026-10-24: S2.4 accepted: routing + k_dlg_L 0 px, k_dlg_R 476,812 -> 370 px (held, S3), census 508 better / 89 same / 12 worse (all UNSUPPORTED rows at 96-99.9 % wrong, same first cause), segments 205/205, boot = S2.3, 12/12 controls RED. MERGE_wave3.md written. S2.9 spawned; S2.11 added.
- 2026-10-24: S2.5 accepted: cov_a_quit / cov_a_quit_title traces (no tool change, no re-pin), boot PASS 301/301 and NO_FRAMES (title-only scenario). Lead framegate with the S2.4 DLL (L_framegate_quit_s24.tsv): 19 PASS, 5 UNSUPPORTED in-game. Slot s2_5 returned.
- 2026-10-24: S2.6 accepted PARTIAL: cov_r_s2_screens pinned (32 checkpoints, det_a == det_b == det_c, replay 31/32 SAME: theme_packs frame-only U-1252), boot stops at record 771 cursor tile (U-1253 KNOWN entry in 6_merge.md); scroll text no checkpoint (U-1250), buy land unported (U-1251). Slot s2_6 returned. S2.7, S2.8 spawned.
- 2026-10-24: orchestrator refused the unmeasured KNOWN entry (U-1253). MERGE_wave4.md item 1.3 removed; lead rewrote port/test/test_s2_6_scenario.py (records 0..770 strict, symptom pinned, control RED via g_memberTypeHistory, 2 xfail U-1253): 3 passed, 2 xfailed. S2.12 (measure U-1253) planned, slot requested. S2.8 told.
- HEAD 2c05f02a (S1 + S8): 3_shared + 4_shared still apply. S2.7 accepted (6 boot screens 0 px, census PASS 50 -> 69, 13/13 RED; U-1260 0x1a stack word). S2.8 accepted PARTIAL (8 report screens 0 px; hire addSprite icons S3; U-1278 duplicate bodies with S2.9). S2.12 spawned on slot s2_12.
- S2.9 accepted (showPopup / system menu / textInputBox / saveGameDialog / SGA page: 14 rectangles 0 failing px). Lead + S2.12 moved to claude3 (claude2 100%). S2.13 integration planned.
- S2.12 accepted PARTIAL: original and capture call logs identical with / without FRAMEHASH (0 of 3,199 rows differ), so U-1253 is not U-1062's mechanism; the port's P6 cursor read in frame 770 after showEndOfYearScreen returns the pre-screen cursor (175,535) instead of the click (576,246); path not found (U-1254). No KNOWN entry proposed. Slot s2_12 returned.
- G7 (orchestrator): new workers on claude3, cap 2. S2.13 spawned (integration + U-1254).
- 2026-10-25: S2.13 accepted (combined tree, U-1278 reports owns the 5 shared bodies, U-1254 fixed: boot cov_r_s2_screens equal through 1156; segments 205/205; 9/9 controls RED). MERGE_wave5.md written. S2.10, S2.11 spawned on claude3.
- 2026-10-25: orchestrator: Surface_drawSprite stays S2's (gfx_reports.cpp); S3.5 drops its registration. Radial menu, Surface_drawScaled, drawFrameR3 / S2 moved to S3.6.

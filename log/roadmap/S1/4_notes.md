# S1.4 notes (U-1066 part 1: tutorial pages 1..29; U-1051: input during a world load)

Worker: claude2 session under S1, 2026-10-24. Instance `instances/s1_4` (winmm.dll 381ed2b2 = the pinned references', copied from s1_3), `SIMGOLF_ALLOW_MULTI=1`,
`C3FT_AUDIO=none`, never booted in original\. Python `~/bin/py -3.12`.

## Result
(a) cov_r_tutorial: pinned (3 runs identical, 34 checkpoints), boot gate PASS 1195 / 1195, controls showTutorialText 1..4 RED, a test reads the
original's checkpoint images to prove every page was reached. (b) cov_r_flic_input: pinned (4 checkpoints), boot PASS 75 / 75 with the Load pump
(patch NOT applied, mirror build, `4_merge.md`), RED without it (record 1, g_cachedCameraTileX) and with `tick.frame` 8.

## (a) cov_r_tutorial (re/tools/scenario_s1_4.py)
- Entry: no ztut.sve fixture. mood5.sve is loaded from the title menu (load row 2), then bit 15 of g_flags 0x0059e7b8 is set by a harness
  read-modify-write (`_cov_b_set_bits`, the value read in the record run and kept in the schedule) and g_tutorialPage 0x00567a18 by a harness write
  (`c.write32`, a DELIVER kind 4 the port's TraceInput already replays, as cov_b_location's date write).
- Why a write: a byte scan of golf_clean.exe .text for the dword 0x00567a18 finds nine operands: 0x0041c11b (mainLoop's reset to 0, edi), 0x004604f1 (page
  read), 0x00460b91 / 0x00460b9b, 0x00460c18 / 0x00460c22, 0x00460cda / 0x00460ce4 (read, +1), 0x00460d5d (reset to 0). No store of a nonzero value; the
  page is not in re/model/sve_fields.tsv. Register-relative or block writes are not excluded (U-1182).
- Script: keys are WM_CHAR `z` (mainLoop's key loop calls showTutorialText at 0x0041c147 when bit 15 is set and the key is not Esc). Sequence: page 1 written, 11
  keys (pages 1..10 advance; page 11 saves "Tutorial Save.sve" (saveGame 0x0040b4a0), clears bit 15, page := 0); then for pages 12..20 and 30 the flag and the
  page are written again, one key each (the switch table's return entries and the `ja` at 0x0046050f: no text, no advance); page 21 written, 9 keys (21..28
  advance, 29 saves and resets); a key with the flag clear (page 0); flag set, Esc: loadGame of the file the tutorial saved (0x0041c0f9). Checkpoints after each.
- Evidence of coverage (`test_tutorial_pages_reached_in_the_original`, reads log/p2/p24/cov_r_tutorial/c_a/*.fh.bin): after the key of page n the counter is n + 1 for
  n in 1..10 and 21..28, pages 11 and 29 end at page 0 with g_flags 0x41000000, pages 12..20 and 30 stay at their value.
- Gate: `~/bin/py -3.12 -m port.test.boot cov_r_tutorial` -> `PASS 1195/1195` (log `4_boot_tutorial.log`); x64. Controls (`test_tutorial_negctl_reads_red`,
  showTutorialText 1 page not advanced, 2 no startMessage, 3 camera move dropped, 4 camera cache reset skipped): 4 / 4 RED. Before this scenario controls 1..3 read GREEN
  on all 60 scenarios (every corpus call had page 0).
- Dropped tail: a version with five more virtual seconds after the Esc-load diverges at record 1244 of 1247 in g_golfers, g_shotTargetX / Y, g_rng
  (`4_boot_tutorial_long_FAIL.log`, U-1180). The shipped scenario ends at esc_load (the load itself, with the saved Tutorial Save.sve, is equal).
- Pipeline commands (each from the repo root, env `SIMGOLF_ALLOW_MULTI=1 C3FT_AUDIO=none`):
  `~/bin/py -3.12 re/tools/scenario_refs.py run --scenario cov_r_tutorial --label det_a --instance s1_4 --record`
  `~/bin/py -3.12 re/tools/scenario_refs.py regen cov_r_tutorial --instances s1_4 --jobs 1` (b / c replay, writes the manifest entry)
  `~/bin/py -3.12 re/tools/segment_corpus.py --instance s1_4 capture --scenario cov_r_tutorial --label c_a` (framehash.bin of the original)
  `~/bin/py -3.12 re/tools/segment_corpus.py --instance s1_4 trace --scenario cov_r_tutorial` (port/corpus/traces; delete an older trace first, it refuses a differing one)
  `~/bin/py -3.12 -m port.test.boot cov_r_tutorial`. Logs `4_regen_tutorial.log`, `4_capture_cov_r_tutorial.log`, `4_trace_cov_r_tutorial.log`.

## (b) cov_r_flic_input and U-1051
- Scenario: title -> Sandbox -> location screen -> a `z` WM_CHAR posted at +600 ms (the click's own actions end at +500) -> Monterey -> hud. The first draft posted eight
  keys; the first vanished in the location screen's loop, the second was the one that mattered.
- Measured (trace, `trace_tool.py dump`): the DELIVER of the key falls in a waitTicks (clock site 0x0045bfe3) 22 clock calls before the wait ends; the MSG and
  rootWin_onKey records come after the wait's last timeGetTime read and before the first jgld palette QPC of a 763-call run; FRAME 0 follows, its TAKE value 122.
  waitTicks' pump (0x00483c30 from 0x0045bfc6) dispatches nothing here.
- Measured (call log `C3FT_CALL_LOG=465750:2,483bd0:0,483c30:0,43d740:0,43dbe0:0`, run `calls2`): after the last 0x00483c30 call (virtual T 5369) loadWorldAssets
  (0x0043dbe0) starts at T 5370; 120 loadFlic calls (0x0043d740) each call pumpInput483bd0 from 0x0043d799 at that one T (the load makes no clock read);
  rootWin_onKey (0x00465750, return address 0x0047c67e, args 122, 0) runs inside the 15th pump (14 earlier pumps dispatched nothing, U-1181).
- Why harness input cannot land inside a load: the harness posts from the clock stub's poll; the load calls no timeGetTime, so only a message posted before
  the load stays queued for it.
- Port before the patch: loadFlic's +0x54 pump took nothing (menu.cpp comment, U-1052 wording) and TraceInput gives a modal batch only to a Blocking pump of a modal
  site, so the key never arrived: boot FAIL at record 1 on g_cachedCameraTileX (the original's first frame zooms).
- Patch (4_merge.md section 1, `4_load_pump.diff`): `PumpKind::Load`, `cm::pump_load`, `TBatch::load`. Rule: a modal-site batch whose next clock read after its handlers is the palette QPC.
  Not a general mid-load model: one recording (U-1183).
- Gate (mirror DLL): flic 75 / 75 PASS; boot --all 62 PASS + 2 KNOWN + 1 NO_FRAMES + 1 FAIL (cov_r_bio, S1.5's, same record 579 on the unpatched build); segments 205 / 205
  PASS, 0 digests differ from log/roadmap/S0/1_segs_x64.tsv. Control tick.frame 8 RED.

## Files and tests
- `port/test/test_s1_4_scenarios.py`: 9 tests (boot tutorial, 4 controls, page-reach, pinned manifest, 2 flic tests behind `SG_S14_LOAD_PUMP=1`).
  Run: `SG_PORTSIM_DLL=port/build/S1_4/Release/portsim_c.dll ~/bin/py -3.12 -m pytest port/test/test_s1_4_scenarios.py -q -p no:cacheprovider` (7 passed, 2 skipped); with
  `SG_PORTSIM_DLL=port/build/S1_4_m/Release/portsim_c.dll SG_S14_LOAD_PUMP=1` 9 passed (`4_tests_base.log`, `4_tests_mirror.log`).
- Builds: `port/build/S1_4` (the working tree, x64, helper scratch script), `port/build/S1_4_m` from the mirror `port/build/S1_4_root/port`. My first two `port.test.boot` calls used
  lib.load()'s default and may have rebuilt the integrator's `port/build/x64` (its DLL is dated 09:04 today); after that every run used SG_PORTSIM_DLL.
- Manifest `re/analysis/data/scenario_refs.json` gained `cov_r_tutorial` and `cov_r_flic_input` (write_manifest merges the old file; S1.3's entry changed in between because
  S1.3 re-recorded its own scenario, not through my runs).

## Open
U-1180 (5 s tail diverges), U-1181 (why the 15th pump), U-1182 (no static writer of a nonzero page: indirect writes not excluded), U-1183 (one recording behind the Load rule), all in `4_uncertain.tsv`.
A click variant and a title-menu load (loadInterfaceArt at boot) with a queued key were not recorded.

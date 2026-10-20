# Phase 3.8 merges (parent)

## 38e (2026-10-20) merged
- git apply log/p38/38e/commands_h.patch log/p38/38e/tick_cpp.patch; SG_MODULE(holestats) appended to modules.inc.
- Pending at the round's end: UNCERTAINTIES.md rows from 38e_uncertain.tsv (U-1000 hook c3eo.cpp rating v/20 vs v/10, re-check its C4; U-1001),
  close U-0949, commands_split.tsv row 00453330 gap -> command.
- Child closed. Result claimed: 146/190 PASS (championship/02, cov_a_keys2/02 new), others unchanged.

## 38c (2026-10-20) merged
- SG_MODULE(reports) appended; test_tick.py OPEN_MISSING: removed showCourseReport / showEndOfYearScreen / showHoleStatsScreen (38e),
  added showRoutingMapScreen (ui_tour/00 twins now stop there at 97/247); commands_split.tsv rows 0044cff0, 0044fb30, 00453330 -> command
  (log/p38/apply_split_rows.py log/p38/38c_split_rows.tsv). 38e's U-0949 split row done here.
- Pending at round end: UNCERTAINTIES.md U-0980, U-0981; CLAUDE.md paragraph (text in 38c_merge.md section 5).
- Child closed. Claimed: cov_a_career/02, /04, cov_a_keys/03, cov_b_career/01, /02 PASS.
- NEW GAP not in the 3.8 table: showRoutingMapScreen (F5), stops ui_tour/00 and ui_tour_v/00.

## 38a (2026-10-20) merged
- SG_MODULE(gditext); git apply log/p38/38a/segments_py.patch (SegmentRunner attaches the GDI provider by default, SG_TEXT_METRICS=none
  restores the old stops); test_tick.py: Text_put reason changed, saveCourse / saveGameDialog / promptAndSaveFile added (sent to 38b),
  OPEN_DIVERGENT = the four title-menu exits (cov_b_career_types/00-02, cov_k1_animals/00: FrameExit::Menu, outside sg_run_frames).
- Pending at round end: UNCERTAINTIES.md U-0960, U-0961; U-0944 and U-0947 -> resolved; CLAUDE.md paragraph (38a_merge.md section 5).
- Child closed. Claimed: COMMAND 109 -> 117 PASS with the provider (on its own build, before 38c/38e).
- Tool issue: purge_frida_temp needs psutil, missing in ~/bin/py's Python.
- Memory: 38d's full-suite pytest PID 34832 held 10.6 GB (commit exhausted, 38a/38f MemoryErrors); parent killed it, told 38d.

## 38b (2026-10-20) merged
- modules.inc: shotstyle, loaders, saveload inserted BEFORE tick (saveload's saveGame supersedes tick's placeholder; registry_find takes the
  first match). test_tick.py OPEN_MISSING: removed shotStyleButtonCode, loadGameBrowserScreen, courseActionMenu, saveCourse, saveGameDialog,
  promptAndSaveFile. Shared files 38b edited in place (additive): segments.py (segment_files overlay), portsim_c.cpp (sg_files_put overlay),
  cmdemu.py (stub_py, import_handlers), gamefiles.h/.cpp + core_test.cpp (list / write / overlay).
- Deferred clean-up (38b_merge.md section 2): tick's own saveGame FuncDef / g_negctl_save_game / Leaves::saveGame are dead once saveload is
  listed; not removed yet.
- Pending at round end: UNCERTAINTIES.md rows from 38b_uncertain.tsv; CLAUDE.md paragraph.
- Child closed. Claimed on the working tree + its modules: COMMAND 136/155 PASS; left: drawGolferInfoPanel 9 (incl. cov_a_keys2/03 after
  saveCourse), showBulletinBoard 2, showRoutingMapScreen 2, showCharacterIntro 2, title-menu exits 4.

## 38d (2026-10-20) merged
- SG_MODULE(dialogs) after commands; tick.cpp HeadlessTerrain kArenaSize 1 MB -> 16 MB (cov_b_holes18/00 PASS 2130/2130 in 38d's mirror);
  test_events.py test_run_bulletin_board_with_sound body replaced (38d_merge.md item 6); test_tick.py OPEN_MISSING: removed
  drawGolferInfoPanel / showBulletinBoard / showCharacterIntro, added showCharacterEditor (-> new stream 38h).
- Deferred clean-up: tick.cpp fallbacks()["showBulletinBoard"] + fb_show_bulletin_board are dead now.
- Pending at round end: UNCERTAINTIES.md U-0990..U-0996; resolve U-0865, U-0931, U-0950; U-0905 note; CLAUDE.md paragraph.
- Child closed. Claimed (own build + gditext): COMMAND 127/155; the scroll-text layout is reached by no segment (oracle only).

## Merged-tree check (2026-10-20, parent; port/build/x64 copied to port/build/p38_run)
- Tree: HEAD + 38a, 38b, 38c, 38d, 38e merges (38g's routing.cpp present but unregistered).
- TIME-FORWARD 35/35 PASS (merged_segs_tf.tsv); COMMAND 148/155 PASS (merged_segs_all.tsv), 7 left: showRoutingMapScreen 2 (38g),
  showCharacterEditor 1 (38h), title-menu exits 4 (38f ledger). 0 divergences.

## 38g step 1 (2026-10-20) merged
- SG_MODULE(routing); OPEN_MISSING showRoutingMapScreen -> showShortcutsScreen; commands_split row 00456be0 -> command.
- Merged tree: ui_tour/00, ui_tour_v/00 FAIL 178/247 in showShortcutsScreen (log/p38/merged_segs_uitour00.tsv). 38g asked to continue.
- Pending at round end: U-1020..U-1022, CLAUDE.md paragraph.

## 38f (2026-10-20) merged
- SG_MODULE(integrator); git apply log/p38/38f_tick.patch (batch_override hook). Shared files 38f edited directly: scenario_ab.py
  (course_season_6y), segment_corpus.py, scenario_refs.json, segments_manifest.tsv + segments.json (+10 rows, 200 accepted),
  test_static.py (190 -> 200), trace_format.py (q0_64) + trace_schema.md / port/schemas/trace.md, new re/port/divergences.tsv.
- Gates (38f_notes.md section 1): (c) PASS incl. course_season_6y 50,923 frames / 6.22 game years, x64 = Win32; (d) NOT met: 131/145
  championship dispatches have no semantic kind, translated panel clicks lose mainLoop H2's own press effects (U-1011); (e) ledger 74
  rows, 1 without cause (chooseShotTarget.strictFlagSticky, U-1012); (f) 8,192 frames in 23 s x64 / 29 s Win32 vs 713 s, but a
  no-input year does not exist in the original (U-1010: input-gated waits); backends 200/200 identical.
- Title menu: recommended as its own next stream (38f_notes.md section 11).
- Optional: SG_PORTSIM_DLL32 / SG_RUNNER32 overrides in lib32.py.
- Pending at round end: U-1010..U-1014 (+ U-0650 note), CLAUDE.md paragraph.
- Child closed.

## 38g step 2 (2026-10-20) merged
- routing.cpp now also holds showShortcutsScreen and showMembershipRoster (tests test_shortcuts.py, test_membership.py + goldens).
  commands_split: 00454c50 replaced, 0044e770 appended (was not a straddler row). OPEN_MISSING: showShortcutsScreen removed.
- Claimed: ui_tour/00, ui_tour_v/00 PASS 247/247 (log/p38/38g_cmdsegs3.tsv). Pending: U-1023, U-1024; CLAUDE.md line. Child closed.

## 38h (2026-10-20) merged
- SG_MODULE(chareditor); OPEN_MISSING showCharacterEditor removed; commands_split row 004385d0 replaced (38h_split_rows.tsv).
- Claimed: cov_a_keys3/01 PASS 118/118; 161/165 COMMAND (only the 4 title-menu exits fail). Bio entry (control 20) not ported (U-1032).
- Pending: U-1030..U-1035; U-0996 resolved (opened by key 0x3f, not clickGolferPanel); CLAUDE.md paragraph. Child closed.

## Parent test fixes (2026-10-20)
- golden_commands.json: clickGolferPanel re-pinned (SG_EMU_ORACLE=update: oracle GREEN on every golden case; 11 cases changed because
  showCharacterEditor is now registered); showSystemMenu: test_commands.py WIDGET_GAP_PEERS emptied, promptAndSaveFile moved to
  WIDGET_RECORD (recorded on both sides, now registered by saveload), re-pinned after the oracle (widget oracle 5/5 GREEN).
  test_commands alone: 83 passed, 44 skipped.
- test_loaders roster cases (scrollTextWindow now real via dialogs): handed to 38k.
- Suite on the working tree (one file per process): everything passes except test_static / test_menu (38i/38j in-flight modules)
  and test_loaders (38k). Static 600.

## 38j (2026-10-20) merged
- git apply log/p38/38j/tick_cpp.patch tick_h.patch (title menu + session dispatch in the frame step, refreshTerrain out of kDrops,
  retile exit before the next record); SG_MODULE(menu) before tick; test_tick.py OPEN_DIVERGENT = {} and drawRecords note 57 frames.
- Claimed on its build: 200/200 segments PASS; chains 335/335 PASS, no BREAK; test_menu 67 with the oracle.
- Pending at round end: mainLoop_structure.md correction (38j_merge.md item 7, U-1053: selections 2/3 and flags 0x4000000/0x1000000
  swapped), U-1050..U-1053, integrator.py cleanup (item 6: SEGMENT_MENU_EXITS {}, MISSING_CAUSES rows), CLAUDE.md paragraph.
- Told 38i to rebase on it. Child closed.

## 38i (2026-10-20) merged
- git apply log/p38/38i/commands_h.patch commands_cpp.patch; SG_MODULE(semantic). semantic.py edited in place by 38i (additive).
- Claimed (tree after 38h/38j): 200/200 segments PASS without and with; gate (d) championship/00..02 PASS semantic-only on x64 and Win32,
  digests equal to the raw run; all 165 COMMAND segments PASS semantic-only (10,420 dispatches, 0 translation gaps).
- Pending at round end: U-1040..U-1042, U-0654 note, CLAUDE.md paragraph (38i_merge.md section 6). Child closed.

## 38k (2026-10-20) done in place (no shared file)
- loaders.cpp: missing-file message through cm::call_peer (recordable); private peer() removed. test_loaders.py record seam; golden_38b.json
  celeb_both_missing / pro_both_missing re-pinned after the oracle (64/64 GREEN). Siblings 161 passed with the oracle. Child closed.

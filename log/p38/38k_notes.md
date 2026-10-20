# 38k: loaders roster oracle seam (2026-10-20)

Problem: stream 38d's module `dialogs` registered scrollTextWindow, so loadGolferRoster's missing-file message ran the real
modal on the port side. The test's original side stubs scrollTextWindow (0x0046DE70) and the port used to stop there with
MissingResource, so test_roster_oracle_equals_original[celeb_both_missing / pro_both_missing] and the loadGolferRoster golden
digests read RED (port wrote f_w_4e3da4+0x8 = 0x07, original snapshot 0x08).

Fix:
- port/src/sim/loaders.cpp: the not-found path calls `cm::call_peer(ctx, "scrollTextWindow", {text, 0x64, 0x32})`
  (commands_internal.h). Default behaviour is unchanged (registered function runs, MissingResource when none is registered);
  with `commands.recordPeer` on for that name the call is logged and the body continues, as the original's stub does. The
  private `peer()` helper was removed (no other user).
- port/test/test_loaders.py: port_run takes `record=` / `calls_out=` (same commands.recordPeer seam as test_saveload.py);
  run_roster_port records scrollTextWindow; the original's stub is registered as cdecl with 3 arguments (`ret_pop` returned 0
  at that address, so the first version of the stub saw no arguments); roster_compare compares the call list (name hash,
  three arguments, count) of both arms, then eax and the whole data image after the run (both arms' scrollTextWindow returns 0).
  The image the original's stub saw at its first call is not compared on its own any more: the port has no stop point there;
  the final image covers it except bytes the later steps overwrite (the text buffer). digest() takes the call records.
- port/test/golden_38b.json: loadGolferRoster re-pinned through SG_EMU_ORACLE=update (oracle GREEN on every case first); only
  celeb_both_missing and pro_both_missing changed (the other 27 digests are byte-identical to the previous pin).

Evidence (DLL port/build/38k/Release, x64, SG_PORTSIM_DLL):
- SG_EMU_ORACLE=1 test_loaders.py: 64 passed (roster oracle all cases, listing oracle, golden, 11 negative controls RED).
- SG_EMU_ORACLE=1 test_saveload.py test_save_dialogs.py test_browser.py test_course_menu.py: 161 passed.
- default (no oracle) test_loaders.py + test_static.py: 20 passed, 50 skipped (oracle cases).
Logs: log/p38/38k_loaders_run{1,2,3}.log, 38k_siblings_oracle.log, 38k_default_static.log; previous pin kept in 38k_golden_38b.pre.json.

Open: none. No merge into shared files (no 38k_merge.md). No new U-rows.

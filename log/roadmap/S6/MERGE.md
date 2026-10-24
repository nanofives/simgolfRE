# S6 merge (defect decisions)

Nothing here is staged or committed. The files in files.txt are new or edited by S6; the orchestrator commits them.

## Shared-file edits (apply in this order)

1. UNCERTAINTIES.md: append the 28 rows of `log/roadmap/S6/uncertain_rows.md` (U-1550..U-1564, U-1580..U-1590, U-1595..U-1597) to the
   table, at its end. Regenerate with `~/bin/py -3.12 log/roadmap/S6/uncertain_rows.py` if a worker file changes.
2. re/port/divergences.tsv: no edit. The ledger regenerates itself (`integrator ledger`); the six new switches add six `defect-switch`
   rows with status `no evidence` until the integrator's `defects` run is repeated (see Gate, last line).
3. re/PORT_PLAN.md status table, row 7: replace `not started; every reproduced defect has a switch (default = the original)` by
   `decisions listed (re/port/defect_decisions.tsv, 74 rows: 10 fix, 26 ask Mariano, 38 keep); profile fixed = 6 switches over the
   hang and fault defects (re/port/defect_profiles.tsv); profile original = every switch 1`.
4. CLAUDE.md: the paragraph below, after the "Phase 3.8 integrator" / "Phase 4" entries (the end of the port history), as one entry.
5. C ABI (optional, not needed by the gate): `log/roadmap/S6/3_merge.md` has the text for `sg_profile_set` / `sg_profile_get`
   (portsim_c.h / portsim_c.cpp, both held by another session). The Python side (port/test/profiles.py) applies profiles through the
   existing `sg_defect_*` calls; a host that wants a profile natively (S7) needs the optional ABI.

No change to port/src/sim/modules.inc, tick.cpp, commands.cpp or port/CMakeLists.txt. Each fix is a DefectDef in its module's
`kDefects` (golfers.cpp, events.cpp, terrain_gen.cpp), default 1 = the original's behaviour.

## CLAUDE.md paragraph

Phase 7 defect decisions, section S6 of the roadmap orchestrator (2026-10-24, notes log/roadmap/S6/, U-1550..U-1597): `re/port/defect_decisions.tsv`
holds every defect of model_defects.md (#1-#52, some split by arm) and every non-PASS row of divergences.tsv (grouped by cause) with its class
(hang / fault / overrun-silent / wrong-value / dead-state / port-gap / harmless), six effect columns (save / balance / sim / frame / hang / fault),
a recommendation and the switch: 74 rows, 10 `fix`, 26 `ask`, 38 `keep` (generator `log/roadmap/S6/merge_decisions.py --write`; schema
log/roadmap/S6/SCHEMA.md). The classification came in two passes: S6.1 / S6.2 from the model notes, then S6.6 re-read the cited instructions and ran
the ORIGINAL in Unicorn on constructed states (log/roadmap/S6/6_probes/, one probe per defect, each hang with a completing control at the same
budget). Corrections it made: spawnWalker with 64 live walkers completes (writes the record over g_editQueueX, returns 0x40) instead of hanging;
stampCourseTiles' scan only reads and the original already refuses a full table; showMembershipRoster copies a type name, not the 0x230-byte slot;
D22's register test is equivalent; rebuildHeightfield's relax pass and buildGolferBioText's picks cannot loop on the shipped tables; the movie id
list of serviceMovie has no writer; and one new hang, loadGolfPro with g_difficulty 5 or 6 never ends even with every pro record filled (D42c, `ask`).
Profiles (port/test/profiles.py, tests test_profiles.py 19): `original` = every sg_defect switch 1 and is the oracle (equal to no profile); `fixed`
= the members of re/port/defect_profiles.tsv, and only decision D5's class: a hang or fault whose fix changes no save, balance value, simulation state
or frame on a run the original completes. Six switches, each default 1, each judged by the probe state that hangs or faults in the original (the
port returns cleanly at 0) and by a completing control whose result is identical at 1 and 0, plus a wrong body that reads RED:
placeHabitat.clusterLoopBound (slot 127 with 0xff after the table is treated as taken), spawnObject.cellSearchBound (return before the animal record
is claimed), loadGolfPro.divideAndRejectGuard (a zero divisor or an empty roster ends the draw at pick 0: U-1590, no original run exists there),
placeAnimal.zeroFrameCount, replaceInText.tailLimit (a tail of 0x200 or more is refused: the original overwrites its return address from there;
U-1597 truncation instead), generateRandomCourse.searchBound (the type-0x11 search skips the feature, the clubhouse search takes the window centre:
U-1596, a port choice). Tests port/test/test_defects_golfers.py (25) and test_defects_gen.py (13). The 26 `ask` rows (every fix there changes a
save, balance, sim state or a frame) are in log/roadmap/S6/ASK_LIST.md for Mariano: D1 D5 D6 D10 D11 D12 D17-D21 D23 D24 D26 D32 D37-D39 D42c D44
D47b D48a D48b D49a D52a D52b3. Findings in the original: loadGolfPro hangs for difficulty 5 and 6; replaceInText derails the return address from a
tail of 0x200 bytes (0x204 returns to the tail's own bytes); spawnObject's cell search draws about 250,000 times before the budget ends when all 16
cells are taken; the five existing switch rows keep their evidence, chooseShotTarget.strictFlagSticky still has none (U-1012).

## Gate (expected numbers)

Run each file in its own process, with `SG_PORTSIM_DLL` pointing at a build that holds the S6 files (the lead's mirror: port/build/S6_mirror_root/b/Release):
- `~/bin/py -3.12 -m pytest port/test/test_profiles.py -q` -> 19 passed
- `~/bin/py -3.12 -m pytest port/test/test_defects_golfers.py -q` -> 25 passed (about 6 minutes)
- `~/bin/py -3.12 -m pytest port/test/test_defects_gen.py -q` -> 13 passed
- `~/bin/py -3.12 -m port.test.profiles validate` -> `fixed` lists 6 switches, no error
- the `original` profile leaves every gate unchanged: test_golfers 60 passed, test_events 50 passed, test_terrain_gen 14 passed (101 opt-in skipped),
  test_tick default segments, test_static (see log/roadmap/S6/lead_gate_original.log); the full segment gate (200+ accepted segments, digests unchanged)
  and the boot gate belong to the integrator, run once after the merge: `~/bin/py -3.12 -m port.test.integrator segments` and `... defects`
  (the defects run with all 11 switches should list the six new ones as `evidence` after S6's tests are named in integrator.py's evidence map, a shared file not edited here).

# S6.4 notes: `fixed`-profile switches of the golfers module

Switches (DefectDef in port/src/sim/golfers.cpp kDefects; flags g_defect_*; default 1 = original, 0 = clean return):
- placeHabitat.clusterLoopBound (D8, D40a, D40b), golfers_world.inc: with 0 the cluster loop needs `slot + 1 < 128`. Equal to the
  original on every completing run because the loop ends once slot + 1 is claimed, which holds for every slot but 127 (D40a);
  D40b's write before the table only happens inside the hang, so with 0 a `t == -1` also breaks before the write. Execution then
  continues to the habitat name message and sound like the original does when the byte after the table is not 0xff.
- spawnObject.cellSearchBound (D41), golfers.cpp: after finding the free animal record and BEFORE claiming it, count the free
  cells of the 4 x 4 corner of the bank's map; none -> return. No write, no RNG draw. A run that finds a cell is unchanged.
  (The original claims the record first, then hangs; with 0 the record stays free.)
- loadGolfPro.divideAndRejectGuard (D42a, D42b), golfers_pro.inc: `gave_up` ends the draw loop with pick 0 for (a) no filled
  pro record 1..99, (b) first divisor (difficulty * 5 + 0x14) * 2 == 0, (c) second divisor 4 - difficulty == 0. Pick 0 is the arm the
  function already takes for a repeated pick; taken[] is not marked. D42c (difficulty 5 / 6) is NOT touched (`ask`).
  Semantics choice: a zero divisor has no value, so the draw yields no pro; the alternative (treat the quotient as 0) can leave the
  loop unbounded for difficulty 4 (the upper bound stays below v), so it was not taken.
- placeAnimal.zeroFrameCount (D43), golfers_world.inc: return before the mod when the frame count is 0 (probe d43: no write before it).

Negative-control modes added (only active when the switch is 0): spawnObject 3 (gives up with one free cell), placeHabitat 2
(skips the loop for slot 126), placeAnimal 3 (returns for frame count 1), loadGolfPro 2 (gives up when 4 - difficulty is 1).

Gate (build port/build/S6_4, SG_PORTSIM_DLL; logs 4_*.log): test_golfers 60 passed; test_vectors -k golfers functions 8 passed,
2 skipped (placeAnimal has no vector file); test_segments sandbox_basic / course_season / championship 29 passed (digests unchanged);
test_static 6 passed; test_defects_golfers 25 passed (319 s: the habitat tests scan seeds, each call hashes the whole image).
Controls: 4 of 4 RED (each against its completing control), 0 blind. ON states still raise (NoReturn / DivideError), OFF return.
Reproduce: SG_PORTSIM_DLL=<dll> ~/bin/py -3.12 -m pytest port/test/test_defects_golfers.py
Open: the loadGolfPro OFF result builds the golfer from pro record 0 (name of record 0 read through loadThemeFile); no original
run exists to compare, recorded as U-1590 in 4_uncertain.tsv. Rows for defect_profiles.tsv: 4_merge.md.

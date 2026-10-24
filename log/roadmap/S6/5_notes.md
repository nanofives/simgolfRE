# S6.5 notes: fixed-profile switches D46, D52b1, D52b2

Facts: log/roadmap/S6/6_decisions.tsv (D46, D52b1, D52b2), probes log/roadmap/S6/6_probes/d46_replace.py, d52b_search.py, d52b_clubhouse.py.

## Switches (default 1 = today's port = the original; 0 = fixed)
- `replaceInText.tailLimit` (model_defects#46, port/src/sim/events.cpp, kDefects). Original (probe log): tail strlen <= 0x1ff completes, 0x200 puts the NUL over the return address, 0x204 faults. Fixed: a tail of 0x200 characters or more refuses the replacement and leaves g_textBuffer untouched; every shorter tail is byte-identical in both settings. The limit applies only through `replaceInText` (`ev::replace_with(..., limit_tail)` defaults to false, so substituteStoryNames and the other internal callers are unchanged).
- `generateRandomCourse.searchBound` (model_defects#52, port/src/sim/terrain_gen.cpp, kDefects), one switch for both rows:
  - D52b1 (type-0x11 search, layout kind 1, window x 10..39, y 0..29): at 1, NoReturn once 65,536 misses and no cell in the window is type 0x11 (as before). At 0 the search gives up, the `do` loop of that feature ends (remaining features of the kind are skipped, no flag written), generation continues.
  - D52b2 (clubhouse search, window 15..31): at 1, NoReturn as before. At 0 it takes the window centre (cx = cy = 0x17) and goes on to stamp the clubhouse there. The function has no "nothing found" arm; the centre is the minimal choice that keeps every later write defined.
  - The other searches (evalPlacementArea ones, D52b3, and the `qualifies` searches) still use `miss()` and NoReturn at both settings (D52b3 and D52a are `ask`).
  - The bound only fires after the existing probe proves the candidate set empty, so no state the original completes on changes (draw sequence untouched).

## Evidence (own build port/build/S6_5, SG_PORTSIM_DLL)
- port/test/test_defects_gen.py: 13 passed. D46: tails 0x10 and 0x1ff equal with both settings; 0x200, 0x204, 0x300 replaced at 1, untouched at 0; control replaceInText negctl 1 under switch 0 differs from the completing result (RED). D52b2: flags 0x8000 everywhere gives error -10 at 1, completes at 0. D52b1: loc 1, setting 0, ctype 0, difficulty 3, relief 0 gives -10 at 1 and completes at 0; the same image with loc 3 completes at 1 (so the -10 is the loc-1 search; log/roadmap/S6/5_probe_b1.log lists 40+ such images). Three completing controls (two loc-1, one loc-3) give identical (eax, image) at both settings. generateRandomCourse negctl 1 under switch 0 differs from the good run.
- Gate with switches at 1: test_events 50 passed (423 s), test_terrain_gen 14 passed / 101 skipped (opt-in oracle), test_core 54 passed (defect table test), test_vectors -k replaceInText: 2 passed (generateRandomCourse has no vector file, as before). Logs: log/roadmap/S6/5_gate_*.log.
- Not run: SG_EMU_ORACLE=1 oracle for terrain_gen (code path unchanged at switch 1: the new branches fire only when the old probe threw NoReturn); segment and boot gates (no change to default behaviour; integrator to run).

## Blind / open
- D52b1's -10 images are identified by location (loc 1 vs loc 3 control), not by an instruction-level counter. U-1595: whether any image reaching D52b1 also hits another NoReturn search after the fix (the fixed run completed on all tested images, but only one is in the test).
- D52b2 fallback (window centre) is a port choice with no original behaviour to compare. U-1596.
- D46 refuses (does not truncate): a decision for the lead if truncation is preferred. U-1597.

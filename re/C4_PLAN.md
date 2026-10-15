# C4 plan: the C3 rows that are not yet C4 (written 2026-10-15)

## Where it stands

C4 has two kinds of evidence (re/CONFIDENCE.md, scripts/re_classify.py target C4), each on top of C3:

- **match**: a 100% VC6 match of the function (re/tools/match.py, `log/diff/<addr>_[<stem>_]<name>.match.csv`). After
  the match CSVs were regenerated over all 262 re/match/*.cpp (log/c3/c4_match_regen.py), 1,889 rows were promoted this
  way (2026-10-15).
- **scenario**: a canonical-scenario run with the hooks ON and OFF reading the same game state at every checkpoint
  (re/frida/scenario_ab.py, c3ft), `<addr>_[<stem>_]<name>_<scenario>.scenario.csv`, every one GREEN with its install
  witness. 150 rows were promoted this way (2026-10-15, the user's decision). Caveats from c3ft's own controls: the
  verdicts come from one run with all hooks ON, and an equivalent mutant can read GREEN.

C4 = 2,040 after both. 876 C3 rows remain (log/c3/c4_pool.py -> log/c3/c4_pool.tsv): 874 with no evidence of either kind
(sound.dll 449, golf_clean.exe 406, jgld.dll 19) and 2 exe rows with a match below 100%. flagSet 0x0049eef0 and flagIsSet
0x0049ef80 were demoted to C2: no SG_HOOK at their addresses ever existed (log/c3/hook_integrity.py now checks that every
C3 / C4 row has a linked hook).

| module | <= 64 B | <= 256 B | <= 1 KB | > 1 KB | main subsystems |
|---|---|---|---|---|---|
| sound.dll | 86 | 227 | 121 | 15 | audio 393 |
| golf_clean.exe | 227 | 56 | 59 | 64 | ui 218, boot 102 |
| jgld.dll | 5 | 6 | 2 | 6 | crt, render |

## Evidence decisions (the user, 2026-10-15)

- **A naked inline-asm transcription is NOT match evidence.** A `__declspec(naked)` copy of a function's assembly matches by
  construction, so it proves only that the bytes were copied. log/c3/c4_match_promote.py skips any address whose MATCH line
  sits on a naked function. Five rows that had reached C4 that way were demoted to C3: sound strcpy 0x4a010, __ftol 0x42550,
  strstr 0x42690 (m36c_crt_asm.cpp) and exe fillSpanDown 0x493000 / fillSpanUp 0x493080 (golf_hand_03.cpp). They need
  scenario evidence.
- **Library-object identity IS match evidence.** A CRT routine instruction-identical (match.py's normalisation, 100%) to its
  member of the VC6 library the binary links (tools/vc6/vc98/lib/libcmtd.lib for the debug DLLs) is the vendor's own compiled
  object, stronger than a recompile of guessed source. m36e measured it for jgld __ftol, strlen, strcpy, strcat and _setjmp3
  (log/c3/m36e_lib_match.py, CSVs log/c3/m36e_crtlib/, copied into log/diff); all five are C4.
- **An inline `__asm` block inside a compiled C body IS match evidence.** The C prologue, locals, frame and epilogue are
  compiled and matched; the asm covers instructions VC6 never emits from C (register-only loops at /Od in jgld, `fninit;
  fldcw` in the sound mixers), so the original source evidently held inline asm there. 85 rows were already C4 this way
  (78 jgld blitters, 6 exe, 1 sound); m36a's 21 sound mixer / stream bodies were promoted on this rule (round 36).
- **Scenario evidence does not cover dynInit_* stubs.** s36c's control A (a stub that skips its constructor) read GREEN in
  both spawn scenarios; c4_match_promote.py skips dynInit_* in scenario mode. They go to C4 by match only.

## Two routes

**Match** is the main lever. sound.dll is a release build (`/O2`, like the 245 sound functions already matched; an earlier
version of this plan said debug, corrected by m36b), so its rate will sit nearer the exe's than Terrain.dll's (a debug build
where all 170 functions matched). The bulk path is
re/tools/decomp.py -> re/tools/ghidra2src.py -> re/tools/g2s_collect.py, then match_permute.py on 80-99% near misses
(CLAUDE.md, Matching decompilation). The exe is `/O2` (release): small functions match in bulk, larger ones need hand work.

**Scenario** covers what matching cannot reach cheaply: exe UI and boot code. The two canonical scenarios
(sandbox_basic, course_season) never open most screens, and reach is counted only after attach, so static initializers
(boot, run from __cinit before the harness looks) never read as reached. sound.dll's audio is not compared, so the scenario
route does not help it.

## Round 36 (C4 round 1)

| batch | route | rows | work |
|---|---|---|---|
| m36a, m36b, m36c | match | 150 / 150 / 149 | sound.dll, split by address |
| m36d | match | 283 + 2 | exe rows <= 256 B; plus flagSet / flagIsSet re-implemented and A/B'd (C2 -> C3) |
| m36e | match | 19 + 2 | jgld.dll rows; the two exe partial matches |
| s36a | scenario | exe pool | scenario_ab.py over the canonical scenarios not run yet: championship, golfer_events, golfer_stories, stories_fixture, mood5_fixture |
| s36b | scenario | exe pool, ui first | a UI-tour scenario that opens every menu screen, report and dialog, recorded once and replayed on c3ft's virtual clock |
| s36c | scenario | exe pool, boot first | spawn-time reach (INT3 armed before the process resumes) so static initializers and boot code count, behind an opt-in flag |

The verifier then regenerates the match CSVs, merges the scenario CSVs, re-runs log/c3/hook_integrity.py and promotes
through the gate (log/c3/c4_match_promote.py, `--kind match|scenario`).

Expected: sound by match about 350-420 of 449, exe by match about 150-200, scenarios about 100-200 more exe UI and boot
rows. Left after round 36: an estimated 150-250 rows (sound's x87 and thread bodies, large exe screens no scenario reaches),
which need audio observation in the scenario harness or a dedicated scenario each.

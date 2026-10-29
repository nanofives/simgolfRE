# S4.1 merge lines (for the integrator)

No shared file is required for the tool to work. One suggested addition:

## CLAUDE.md, at the end of the port sections (after "Phase 5 step 2, the location screen")

```
Step S4.1, sound.dll replay (2026-10-24, re/analysis/data/sound_replay.md, notes log/roadmap/S4/1_notes.md, U-1400..U-1409):
the 980 sound.dll path-1 keys replay without the game. re/tools/fixvm/fixvm.js (Node) runs the fixture JS and the real
stand-in helpers over a mock Frida API (frida.js, mem.js, syslibs.js); port/test/sound_emu.py runs the ORIGINAL sound.dll in
Unicorn, relocated (5,769 HIGHLOW entries), DllMain first, traps for callbacks / fake DLL exports / Interceptor redirects
served by the VM over a synchronous page-syncing link; KeyRun repeats diff_hook.js per key (regions, arm built after the
fixture, per-vector resolve, both arms). re/tools/sound_census.py -> re/roadmap/S4_census.tsv, three layouts (a field equal in
A, B and C is position-independent): 181 EXACT, 785 equal on every position-independent field, 5 DIFF (__ftol_snd: Unicorn
sets no x87 exception flag and stores 0 for fistp of an empty st(0); 42 rows per key match with the flag byte searched),
2 distinct-count differences (U-1403), 5 partial (RaiseException SEH, timeGetTime), 2 fixtures that wait for the live engine.
Facts: the live TLS index of sound.dll's CRT was 27; the CSVs print doubles shortest (2026-10-07) or as 17 truncated digits
(2026-10-14); KeyRun(second_arm=...) is the S4.2 port hook. Tests: tests/test_fixvm.py (16, static marker, ~35 s).
```

# S2.12 U-1253: cursor tile at record 771 of cov_r_s2_screens

## Result
U-1253 is NOT U-1062's mechanism. The original's screenToTile gives the same result with and without C3FT_FRAMEHASH=sim, and the
capture's value at record 771 is the click position's tile, which the port reaches one record later. The difference is a port
pacing difference (which cursor value the frame's P6 read sees after the end-of-year screen), not uninitialised stack.

## Measurements (instance s2_12, winmm.dll 381ed2b2, C3FT_AUDIO=none, SIMGOLF_ALLOW_MULTI=1)
Command (both arms, schedule log/c3/alt/refs/cov_r_s2_screens/schedule.json):
`C3FT_CALL_LOG="430020:2:r,40acd0:2:430280:r" [C3FT_FRAMEHASH=sim] ~/bin/py -3.12 re/tools/s2_6_runs.py refs run --scenario cov_r_s2_screens --label s212_fh|s212_nofh --instance s2_12`
(logs 12_run_fh.log, 12_run_nofh.log; both OK, polls 2582, 32 checkpoints.)
Call logs: log/c3/alt/refs/cov_r_s2_screens/s212_fh/calls.json and s212_nofh/calls.json (3,199 rows each, 0 dropped).
- The two logs are identical row for row (0 differing rows of 3,199): args and return values of screenToTile 0x430020 and of
  distance 0x40acd0 at return 0x430280 do not depend on the tracer.
- Around record 771 (virtual T 171601): screenToTile(576, 246) at virtual 171599 (ret 0x543d08), distance(48, 12) -> 49 and
  distance(-16, 12) -> 20, again at 171602, 171701, ... Small distances, no overflow, no NaN: the first estimate is on the grid,
  so U-1062's uninitialised locals are not reached here.
- The 171599 call is the 786th screenToTile call of the run (index 785); the previous one (163522) has the pre-screen cursor
  (175, 535). The end-of-year screen ran in between (no calls logged 163522..171599).
- Capture values by sgh64 brute force (12_cursor.py, 12_cursor.log): record 770 (24, 17); record 771 (38, 16); record 772 (38, 16).
  Port (HEAD-mirror DLL port/build/S2_6): record 770 (24, 17); record 771 (24, 17) FAIL; record 772 (38, 16).
  So the capture has already written the click's tile before the head of 771; the port writes it in frame 771.
- Trace (12_trace_dump.txt, `trace_tool.py dump`): the click is at pos 202027, CURSOR reads (576, 246) from pos 201807 on, and
  three CURSOR reads (576, 246) at pos 206172 right before FRAME 771 (pos 206175).
- Port callee trace (SG_TICK_TRACE, 314 MB, deleted after reading): in frame 770 showEndOfYearScreen runs and then P6's
  pumpInput483bd0 / View_toLocal / screenToTile run with (175, 535) (the cursor before the screen); the first (576, 246) call is the
  next frame's. The original made that same P6 call with (576, 246).

## Cause (from the logs)
Frame 770's P6 cursor read (View_toLocal after pumpInput483bd0, ML+0xa434) returns the pre-screen cursor in the port and the click
position in the original. Which port path hands P6 the stale value (cursor-record pacing around showEndOfYearScreen's exit in
tick.cpp / reports.cpp) is not established: U-1254.

## Proposal
No KNOWN entry with U-1062's text: that text would be wrong. If the orchestrator wants the gate to pass past 771 before the fix,
the entry is `"cov_r_s2_screens": [(771, 772, _CURSOR, "U-1254")]` in port/test/boot.py (cursor tile only, one record; with it the
run is KNOWN through record 1156 per 6_notes.md). The proper fix is in the port (the cursor value P6 reads after a modal screen
that consumed CURSOR records): files outside this step's ownership (log/roadmap/S2/12_*). Not done.

## Files / housekeeping
12_files.txt lists them. purge_frida_temp called. No game PID left running by this step. Build: none (HEAD-mirror DLL reused; a
test run through port/build/x64 started a rebuild that failed on siblings' half-written audio_exe.cpp / gfx_hud.cpp, no change to the tree).

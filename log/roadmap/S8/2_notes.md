# S8.2 notes: the original's intro and closing movies measured (DONE 2026-10-24, worker on instance s8_2)

Write-up: re/analysis/video/movie_runs.md (tables, call lists, differences, tool status). Raw numbers: log/roadmap/S8/2_results.json
(regenerate: `~/bin/py -3.12 re/tools/s8_movie_runs.py analyze report`). Merge text: log/roadmap/S8/2_merge.md. U-rows: 2_uncertain.tsv (U-1710..U-1716).

## Files
re/tools/scenario_s8_2.py (cov_r_s8_intro, cov_r_s8_full), re/tools/s8_movie_runs.py (driver + `analyze`), re/analysis/video/movie_runs.md,
port/test/test_s8_2_scenarios.py (14 tests, static, ~3 s), log/roadmap/S8/2_*, log/roadmap/S8/runs/ (run logs, schedule, statediff JSON).
Local only (git-ignored): run folders log/c3/alt/refs/cov_r_s8_full|cov_r_s8_intro/<label>/ (about 640 MB), traces port/corpus/traces/s8_2/.

## Gate numbers
- Intro ON recorded from process start to the title menu and, in cov_r_s8_full, through the quit with the closing movie, three times each: all identical
  (clock digest 138,628 calls / 78,500 for the intro scenario; per-checkpoint tgt/qpc counts, .bin state hash, frame hash, compared-class object hashes; exit
  files and exit code). Skip baseline likewise (83,683 / 23,554 calls). Three recorded traces byte-identical (299,543 B), replay with exit support byte-identical.
- Intro: 825 of 825 frames, 54,940 virtual ms by QPC (0 by timeGetTime), clip end. Closing: 589 of 589 frames, 39,207 ms, clip end.
- Clock calls added by the intro: 54,945 main-thread QPC calls, all `binkw32.dll+b6e7` (plus the process's single `b6d3` call, which the closing movie makes in the skip run).
- State difference ON against skip: one run-stable GameState dword, g_ui83ab9c 0x0083ab9c (0 -> 1), at the checkpoints before the closing movie; g_seedTime, g_rng, the
  playAudioFile deadline word 0x005aa774 and every sim object are equal (the lead's expectation that the seed moves did not hold: nothing in a movie reads timeGetTime).
- The recorder (Interceptor on binkw32's exports) and the object-image option do not move the timeline (plain_on vs i1, skipA vs j1: same clock digest and state hash).

## Controls (analysis side; the game side has no ported body)
Tests: a movie that ends at FrameNum 3 of 5 reads "early"; a flipped stable dword (g_rng) is attributed; identical conditions attribute nothing; a results file with
another frame count, a false identity flag or an extra attributed dword fails check_results. 14/14 pass; blind: none recorded.

## Reproduce
See the command list in movie_runs.md section 1. Order: `--record` run of the ON scenario (writes the schedule), two replays, the skip run with `--schedule
log/roadmap/S8/runs/sched_full_skip.json --record` and two replays (C3FT_OBJHASH=image on for the .full.bin images), `analyze identity|statediff|clockdiff|movies|report`,
trace record x3 and replay with S8_REPLAY_EXIT=1 (C3FT_OBJHASH unset).

## Session facts worth keeping
- binkw32's pacing is QPC-based (BinkWait: one QPC call per call, BinkDoFrame 3 per frame, BinkNextFrame 1, BinkOpen 5-6): the virtual QPC clock advances the movie, the timeGetTime clock does not.
- Heap addresses (about 47,150 dwords) differ from process to process in both conditions: any GameState comparison across processes must use values stable inside each condition.
- trace_tool replay of an exiting scenario needs S8_REPLAY_EXIT=1 (U-1714). A stale instance lock (dead pid) appeared once (U-1716).
- Boots on instances\s8_2 only; `original\` listing hash unchanged (2_original_listing.md5); purge_frida_temp run after the runs (0 dirs left); no game process left.

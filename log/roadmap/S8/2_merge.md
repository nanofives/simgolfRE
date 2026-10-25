# S8.2 merge notes (for the integrator)

1. re/tools/scenario.py, end-of-file hook (the glob covers only scenario_s1_*.py). Replace the glob line so scenario_s8_2.py is registered too:

```python
for _p in sorted(_glob.glob(str(pathlib.Path(__file__).with_name("scenario_s[18]_*.py")))):
    _importlib.import_module(pathlib.Path(_p).stem)
```

(The driver re/tools/s8_movie_runs.py imports scenario_s8_2 itself, so nothing breaks if this edit is not made. Without it, `scenario_refs.py run --scenario cov_r_s8_full` called directly, not through the driver, does not know the scenario.)

2. re/tools/trace_tool.py, `install` (U-1714): for mode == "replay" wrap the replay script so a scenario whose lockstep function has `expect_exit` sets
`c.run.exit_expected = True` first and keeps answering polls (`for _ in range(300): yield`) after the recorded ones. s8_movie_runs.py does this from outside under
`S8_REPLAY_EXIT=1`; with it the replay of an intro-ON, exiting scenario is byte-identical to the record.

3. UNCERTAINTIES.md: append the rows of log/roadmap/S8/2_uncertain.tsv (U-1710..U-1716).

4. CLAUDE.md port section, one paragraph (suggested text):

```
S8.2 movies (2026-10-24, re/analysis/video/movie_runs.md): the intro (SMSG_introfinal.bik, 825 frames) and closing movie (SMSG_closingfinal.bik, 589 frames) both play to the
clip end; binkw32 paces itself with QueryPerformanceCounter (54,945 main-thread QPC calls for the intro, from binkw32+b6e7) and never reads timeGetTime, so the movies add no
virtual T and no scenario poll runs inside them. Intro ON against skipped: the only GameState word that differs among run-stable values is g_ui83ab9c (0x0083ab9c = 1, written by
BinkPlayer::play, cleared by the root-window input handlers); g_seedTime, g_rng and all sim objects are equal. Three runs per condition identical (clock digest, per-checkpoint state
and frame hashes). Scenarios cov_r_s8_intro / cov_r_s8_full, driver re/tools/s8_movie_runs.py (`--skip-intro 0|1 --bink-log ref|trace ...`, `analyze identity|statediff|clockdiff|movies|report`).
```

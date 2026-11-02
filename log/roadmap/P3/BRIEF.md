# P3: the long session's bimodal memory growth (orchestrator brief, 2026-11-01; decision G21 part b)

## The observation
port/test/test_app_present.py test_long_session_memory_bounded runs simgolf_port.exe `--live-trace course_season_6y` (51,092 frame
records, `--mem-every 1000`) and measures the private bytes gained after frame 2,000 (`_growth`). On one build it gives one of two
values, not a spread:
- I21's build (d075dc56's tree): 3,293,184 / 3,317,760 / 4,280,320 bytes;
- I22's build (d075dc56 + S3 wave 9): 4,284,416 bytes three times;
- earlier: I19 3.34 MB / 4.30 MB, I21's suite run 4.33 MB; at S7.8 (2026-10-25) 28,672 bytes over 49,000 frames.
The limit was 4 MB and is 8 MB since G21 (2026-11-01). The control without bounding grows about 139 MB. Facts of each run:
log/roadmap/merges/I21/facts, log/roadmap/merges/I22/facts, log/roadmap/merges/I22/mem_ab/facts_*; per-run memory.tsv / session.json
copies go to log/roadmap/S7/8_long/<variant>/ (overwritten by each run).

## The question
What makes a run gain about 0.97 MB more than another on the same build and input, and why did the growth go from 28 KB (S7.8) to
3.3 MB? Candidates to measure, not to assume: the raster worker threads' per-thread buffers (S3.10, min(4, hw / 2) workers, started
when?), the terrain pass log / LiveTerrain records (S3.10b bounded them), the host-GL / GlRaster scratch, the audio thread (S7.10: the
app's DirectSound shadow context; the test runs with the default `--audio on`), heap fragmentation of the trace spill. Use the
memory.tsv samples (every 1,000 frames) of a 3.3 MB run and a 4.28 MB run side by side: where does the extra step appear?
Repeat runs with `--audio off`, with SG_RASTER_THREADS=1, and with `--terrain headless` (one variable at a time) to see which removes
the mode.

## Rules
- Work in your own mirror (git archive of the current HEAD + junctions, as log/roadmap/merges/I22/make_mirror.py does); never edit the
  shared tree. Each app run is a heavy run: one `log/roadmap/S3/w8_lock.py` hold per run, step P3 (take / release around a shell
  command; `run` cannot start bash scripts). ~/bin/py -3.12 for Python, port/test/build.py for builds (never bare cmake).
- Measure; report numbers per run in log/roadmap/P3/notes.md. A fix (if the cause is a leak or an avoidable allocation) goes with its
  gates (the memory test 5 runs, segments x64 / Win32, boot x64, test_app_present, test_app_live) as edits.py; if the extra mode is
  the OS / allocator and not ours, say so with the evidence and propose nothing.
- Report DONE or BLOCKED to the orchestrator session cmvqfdn450fcno1159lw1u367 with mcp__happy__send_to_session, then stop.

# S3.3 merge text (U-1120)

Shared file edited (in the mirror only, port/build/S3_head/port/src/sim/tick.cpp): `port/src/sim/tick.cpp`, function `run_frames` (the sg_run_frames / sg_run_boot driver).
Patch: log/roadmap/S3/3_tick.patch (unified diff against the mirror's HEAD copy; apply by hand if the shared file has moved: two hunks).

1. Boot's last frame, the block `if (boot && static_cast<size_t>(k) == frames.size())` ("a boot run's last frame"): wrap
   `tk::tick_frame_io` with `catch (const NeedInput &)` that calls `gfx_end_of_input(ctx)` and records `out_of_input`; the detail reads
   "the last frame ran to the end of the trace: a Blocking pump with no input left". `FrameExit ex` starts as `FrameExit::Wait`.
2. End-checkpoint block (`catch (const NeedInput &e)` that builds `stop`): add `gfx_end_of_input(ctx);` so snapshots still pending get the back buffer.

No other shared file changes (gfx.cpp, tick_hook.h, framegate.py, portsim_c.cpp untouched). framegate.py's `3_rerun_noend.py` relabel of S0.3 is no longer needed.
Tests to add to the suite list: port/test/test_gfx_eoi.py (mine, 5 tests, ~95 s).

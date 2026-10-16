# c3fy registry fragment (2026-10-16): mainLoop 0x0040f5c0.
# mainLoop is called ONCE per process, so it has no path-1 A/B key (diff_hook calls a function many times with vectors).
# Its oracle is the whole-game scenario with the hook ON vs OFF (re/frida/scenario_ab.py over sandbox_basic,
# course_season, ui_tour, championship), compared at fixed virtual-time checkpoints. See log/c3/c3fy_notes.txt.
# c3fy2 (U-0008 resolved): nine scenarios GREEN through log/c3/c3fy2_run.py; evidence in log/c3/c3fy2_notes.txt.
# No HOOKS entries here.
HOOKS = {}

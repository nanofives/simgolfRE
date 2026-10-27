# S7.3 merge instructions (for the integrator)

New files (untracked until staged; nothing else of the shared tree was touched):

```
port/app/CMakeLists.txt        (owned by S7.3 since S7.2's acceptance; rewritten: adds simgolf_port and the standalone shell mode)
port/app/main.cpp
port/app/window.h  port/app/window.cpp
port/app/present.h port/app/present.cpp
port/app/pacing.h  port/app/pacing.cpp
port/app/saves.h   port/app/saves.cpp
port/app/tools/app_drive.py
port/test/test_app_shell.py
```

S7.2's files (port/app/input_map.*, port/app/tools/input_check.cpp, port/test/test_app_input.py) are unchanged; its test still
passes with the new CMakeLists (16 passed, log/roadmap/S7/3_test_app_input.log).

## 1. port/CMakeLists.txt

Only S7.2's line (log/roadmap/S7/2_merge.md section 1): `add_subdirectory(app)` at the end. With it, `cmake -S port` builds
simgolf_port too (checked in the mirror: port/build/S7_3_head/port/build/S7_3, log/roadmap/S7/3_build_merged.log). Without it,
port/test/test_app_shell.py still works: it configures `cmake -S port/app -DPORTAPP_SHELL=ON`, which adds port/ itself (a guard
variable makes port/'s own `add_subdirectory(app)` return at once in that case).

## 2. UNCERTAINTIES.md

Append the rows of `log/roadmap/S7/3_uncertain.tsv` (U-1620..U-1626, same columns) after S7.2's U-1610..U-1619.

## 3. CLAUDE.md

One paragraph under the roadmap entries; text in `log/roadmap/S7/3_notes.md` section 1 (the integrator condenses it).

## 4. Nothing else

No change to modules.inc, tick.cpp, commands.cpp, the registry, test_static or any port/src / port/include file: the app links
the portsim library as it is and calls registered functions (gditext.attach, gfx.attach / snapshotAt / dump / log / detach) and
tick_run_frames (the C++ side of sg_run_boot).

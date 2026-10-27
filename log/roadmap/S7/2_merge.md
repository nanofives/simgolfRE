# S7.2 merge instructions (for the integrator)

New files (all untracked until the integrator stages them; nothing else of the shared tree was touched):

```
port/app/CMakeLists.txt        (ownership passes to S7.3 after acceptance)
port/app/input_map.h
port/app/input_map.cpp
port/app/tools/input_check.cpp
port/test/test_app_input.py
```

## 1. port/CMakeLists.txt

Append at the end of the file (after the Phase 3.R runner block):

```cmake
# Roadmap S7: the Windows host (input translation library and its trace checker; S7.3 adds the app executable).
add_subdirectory(app)
```

The step was built and judged in the HEAD export `port/build/S7_2_head` with exactly this line appended
(`log/roadmap/S7/2_mirror.py` applies it to the mirror only). `port/app/CMakeLists.txt` also configures on its own
(`cmake -S port/app -B <dir>`), which is how `port/test/test_app_input.py` builds the checker: that test needs no line in
port/CMakeLists.txt to pass, and it builds into `port/build/S7_2_app` (gitignored: inside port/build).

## 2. UNCERTAINTIES.md

Append the rows of `log/roadmap/S7/2_uncertain.tsv` (U-1610..U-1619, same columns as the table: ID | addr | type | description |
path to resolution | status) after U-1121 (or after the last U-11xx / U-15xx row the integrator holds).

## 3. CLAUDE.md

One paragraph under the roadmap entries (S7.2 numbers: 67 corpus traces, 26,861 MSG records, 23,432 HANDLER records, 42,441 owned
reads and 5,614 cursor answers, 0 mismatches; 9 controls RED, 1 blind) is in `log/roadmap/S7/2_notes.md` section 1; the integrator
condenses it.

## 4. Nothing else

No change to modules.inc, tick.cpp, commands.cpp, registry or test_static (port/app is outside the scanned `src/` and
`include/portsim`; the sources carry no image-range literal anyway). The library does not link portsim: S7.3 copies a Step's
fields into `portsim::Command` (RootHandler: handler, a0, a1, async_ctrl, async_shift, t).

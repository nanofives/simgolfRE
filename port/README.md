# portsim

The portable reimplementation of Sid Meier's SimGolf's simulation (re/PORT_PLAN.md; phases 2-3 in re/PHASE23_PLAN.md).
It plays from the user's own game files and loads or patches none of the original binaries.

## Rules

- **No original code or data.** Everything here is written from specifications: the data model
  (re/model/include/simgolf_model.h), the analysis notes and the recorded behaviour of the original. The VC6-exact sources in
  re/match/ and the hook reimplementations in shim/src/re/ are reading references, never copied.
- **No original addresses in port code.** State is reached through named GameState members only. The address map
  `include/gamestate_addresses.inc` exists for the tooling that turns dumps of the original into GameState images; port
  code never includes it.
- **Bit-exact, defects included** (decision 4): original defects are reproduced behind named compatibility switches,
  default on (re/analysis/data/model_defects.md).
- **Corpora stay local** (decision 5): `corpus/` (memory dumps, vectors, traces) and `build/` are gitignored; generators
  and manifests are committed.

## Layout

```
port/
├── CMakeLists.txt            # C++17; library portsim, test portsim_layout_test (ctest)
├── include/
│   ├── gamestate.h           # GENERATED (re/tools/model_state_gen.py): struct portsim::GameState = the original data section
│   ├── gamestate_meta.inc    # GENERATED: X-macro table of objects (class, pointer words, hidden-state code) and gaps
│   └── gamestate_addresses.inc  # GENERATED, tooling only: original addresses
├── src/portsim.cpp           # the library (phase 2.2: a size export only)
├── test/gamestate_layout_test.cpp
├── corpus/                   # local only
└── build/                    # local only
```

`gamestate.h` includes the model header from `../re/model/include` (CMake sets the path). GameState is 0x381174 bytes:
allocate it on the heap.

## Build

```
cmake -S port -B port/build/x64 -A x64        # or -A Win32: the layout is the same (pointers are uint32_t, pack(1))
cmake --build port/build/x64 --config Release
ctest --test-dir port/build/x64 -C Release
```

With Visual Studio Build Tools 2022 the CMake binary is under
`Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/`.

## Regenerating the GameState

```
~/bin/py -3.12 re/tools/model_merge.py merge       # re/model/addresses.tsv
~/bin/py -3.12 re/tools/model_state_gen.py         # include/gamestate.h, gamestate_meta.inc, gamestate_addresses.inc
```

Design, classes and how the oracle compares states: re/analysis/data/gamestate_gen.md.

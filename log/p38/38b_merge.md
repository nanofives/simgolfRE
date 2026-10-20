# 38b merge notes (lines for the parent; nothing of the shared files was edited except where marked "edited")

## 1. port/src/sim/modules.inc

Add three lines BEFORE `SG_MODULE(tick)` (registry_find takes the first match, and `saveload` supersedes tick's saveGame
placeholder; `refreshTerrain` and `loadGame` / `loadCourse` / `listMatchingFiles` / `loadGolferRoster` / `shotStyleButtonCode`
are new names):

```
SG_MODULE(shotstyle)
SG_MODULE(loaders)
SG_MODULE(saveload)
```

## 2. port/src/sim/tick.cpp (optional clean-up, no behaviour change needed)

`saveload`'s saveGame has the same negative-control numbering as tick's (mode 1 = appendDate skipped), so
test_tick.py::test_save_game_refreshes_the_year reads the new function. Once `saveload` is listed, tick's own `saveGame`
FuncDef (`{"saveGame", ...a_save_game, &g_negctl_save_game}`), `g_negctl_save_game` and `Leaves::saveGame` / `save_descriptor`
in tick_leaves.inc are dead and can go (tick's own `cc("saveGame", ...)` calls resolve through the registry). Leave the
kDrops entry `refreshTerrain` as it is: mainLoop's two call sites keep dropping it; `saveload` registers a real
`refreshTerrain` (rebuildTerrainGrid + the repaint flag byte) that only loadGame calls.

## 3. port/test/test_tick.py

Line 31 lists `"shotStyleButtonCode": "not registered (ui, 46 bytes)"` among the known missing names: remove it (the function
is registered now). `loadGameBrowserScreen` is registered too (module saveload).

## 4. Edited shared files (additive)

- `port/test/segments.py`: imports `port.test.segment_files` and, after `st.files_root(INSTALL)`, adds
  `segment_files.files_for(seg.scenario)` through `st.files_put` (three lines). cov_a_flags needs the three fixture saves its
  recording's install held (ztut / zdemo / zedit = mood5.sve with one flag bit, derived by `segment_files.py`).
- `port/src/portsim_c.cpp`: `sg_files_put` with a root attached puts into the directory's in-memory overlay (it used to
  write the unused memory files).
- `port/test/cmdemu.py`: `Emu.stub_py(addr, nargs, fn(uc, args, ecx) -> eax)` and `Emu.import_handlers` (python bodies that
  read and write emulated memory), 11 lines, default behaviour unchanged.
- `port/include/portsim/gamefiles.h`, `port/src/gamefiles.cpp`, `port/test/core_test.cpp`: GameFiles gained `list`, `write`,
  `writable`, `remove` and wildcard helpers; `DirGameFiles` keeps writes and deletes in an in-memory OVERLAY (the install is
  never touched) unless write-through is on (`set_write_through`, or `SG_FILES_WRITE_THROUGH=1` at construction).

## 5. Build

My own files (shotstyle, loaders, saveload, saveload_fields.inc, the headers) are picked up by the CMake glob. Nothing in
CMakeLists.txt changes.

## 6. Segments

With my DLL (log/p38/38b_segs_all.tsv): see log/p38/38b_notes.md.

## 7. Round 2 (saveCourse, saveGameDialog, promptAndSaveFile; gditext is merged)

`saveload` now also registers `saveCourse`, `saveGameDialog` and `promptAndSaveFile` (same module line as before; nothing new to
add to modules.inc). Round-2 builds use the working tree: `log/p38/b_mirror_wt.py --build` (the whole port/ of the tree + my three
module lines before `tick`). New test file `port/test/test_save_dialogs.py`; `emu38b.py` gained an stdio `fwrite` stand-in and
a `scrollTextWindow` entry in SAVE_PEERS (recorded). tick.cpp's kDrops / fallbacks need no change.

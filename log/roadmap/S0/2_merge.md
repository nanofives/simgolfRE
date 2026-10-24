# S0.2 merge requests (for the integrator; none is needed for framegate.py to run at HEAD)

framegate.py reads the optional lines of items 1 and 2 when gfx.log has them and falls back to its trace rules otherwise.

## 1. port/src/sim/gfx.cpp, `a_log`: the unsupported reasons in first-occurrence order (U-1110, U-1113)

`Gfx::notes` already keeps the first 64 reasons in order. In `a_log`, after the `unsupported` lines, add:

```cpp
  for (size_t k = 0; k < g.notes.size(); ++k) s += "first\t" + std::to_string(k) + "\t" + g.notes[k] + "\n";
```

and raise the `notes.size() < 64` cap in `Gfx::unsup` to cover every distinct reason (or drop the cap: one string per distinct
reason). With it framegate skips its probe run (about 40 s per scenario) and unsup_order reads `gfx` (exact).

## 2. port/src/sim/gfx.cpp + port/src/sim/terrain_dll.cpp: a per-snapshot terrain flag (U-1110, U-1111)

- gfx.h: `void gfx_note_terrain(Ctx &ctx);` (no-op without an attached backend).
- terrain_dll.cpp `update_terrain_render`: call `gfx_note_terrain(ctx)` on every path that calls render / local_render /
  render_strip / blit_screen (every exit but the empty-list `return 0`, which still ran blit_screen: call it there too).
- gfx.cpp: `bool terrain_in_root = false;` in `Gfx`; `gfx_note_terrain` sets it; every body that writes the whole root surface
  (showTitleScreen, Surface_fillRegion / Image_draw / Surface_blit3 whose rectangle covers 0x320 x 0x258) clears it;
  `b_refresh_full` copies it into `terrain_in_back`; `gfx_progress` pushes `terrain_in_back` per snapshot; `a_log` writes
  `snapshot_terrain\t<k>\t<0|1>`.

## 3. port/src/sim/gfx.cpp `kNotDrawing`: drops tick.cpp's kDrops describes as not drawing

framegate.py's NOT_DRAWING (checked against the kDrops reasons by test_framegate.py): playSound, Snd::status, capsLockOff,
emptyFunc_4a0890, saveCourseJpg, Widget_readPixel, enterModal483d30 (waitTicks and startSoundSlot are already there). With them in
kNotDrawing, gfx.log's unsupported count is drawing only and the per-snapshot counts match framegate's first_unsupported_drawing.

## 4. port/test/gfx.py `render`: one frame further (U-1114)

```python
        max_records = frames + 2      # was frames + 1: a checkpoint in a frame's H1 wait is snapshotted in the next frame
```

plus `to_end=no_frames or max_records > <number of FRAME records>` in the boot.run call (framegate.run_scenario does both).

## 5. port/test/runner32 + lib32.py: the sg_ts_live ops

The Win32 runner has no op for sg_ts_live_attach / detach / log / object, so framegate attaches LiveTerrain on x64 only
(column liveterrain = n/a on Win32). At HEAD no status or pixel depends on it (2_backend_x64.json = 2_backend_win32.json but
for liveterrain / backend).

## 6. port/test/README.md: one paragraph

Under the phase 5 section: "port/test/framegate.py (roadmap S0.2): every checkpoint of a pinned scenario with gfx and
LiveTerrain attached, one TSV row per checkpoint with status, screen kind, context, terrain rule, px per region (one-level /
beyond) and the unsupported calls before the snapshot; `--all --jobs 2 --out <tsv> --resume` is the census. Tests:
test_framegate.py."

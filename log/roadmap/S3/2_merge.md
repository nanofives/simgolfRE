# S3.2 merge: terrain into the frame

Owned files (log/roadmap/S3/2_files.txt) are in the working tree as the step left them. The shared-file edits below are the exact
(old, new) pairs of log/roadmap/S3/2_merge_edits.py, which log/roadmap/S3/2_mirror.py applies to its source copy with an
exact-match check (each anchor once; the file's own line ends). Base: S2 wave 1 (gfx.cpp / gfx_body.h / gfx_families.inc as S2.1 left
them) and S3 wave 1 (portsim_c.cpp routing of log/roadmap/S3/MERGE.md section 1). If the S2 integrator changes gfx.cpp / gfx_body.h
before this merge, the anchors of 2 .. 6 are short single lines and should still be found once; re-run
`~/bin/py -3.12 log/roadmap/S3/2_mirror.py --no-build` to check (it exits with `anchor found N times` otherwise).

## 1. Shared-file edits, in apply order

### 1.1 port/src/sim/gfx_families.inc: append the family line

```
SG_GFX_FAMILY(terrain)
```
after `SG_GFX_FAMILY(selftest)`. Requires S2 MERGE 1.1 (the family-aware test_every_module_file_is_listed); gfx_terrain.cpp holds
`kGfxFamily_terrain` and `g_negctl_gfx_terrain`.

### 1.2 port/src/sim/gfx_body.h: the per-snapshot terrain flag in class Gfx (S0 recommendation 2)

After the line `  std::vector<std::string> notes;       // every distinct unsupported reason, in first-occurrence order (gfx.log `first`)`
insert:

```cpp
  // terrain on the surfaces (roadmap S3.2, S0 recommendation 2): set on the root surface by a terrain pass or the restore of
  // the terrain picture (gfx_terrain.cpp), cleared by a 2D write that covers the whole root surface, copied to the back buffer
  // at Window::refreshFull; each snapshot keeps the back buffer's (gfx.log `snapshot_terrain`)
  int root_terrain = 0, back_terrain = 0;
  std::vector<int> snap_terrain;
  // a 2D write of the rectangle (x, y, w, h) onto the root surface: clears root_terrain when it covers the whole surface
  void note_root_write(int32_t x, int32_t y, int32_t w, int32_t h) {
    if (x <= 0 && y <= 0 && static_cast<int64_t>(x) + w >= root.w && static_cast<int64_t>(y) + h >= root.h) root_terrain = 0;
  }
```

### 1.3 port/src/sim/gfx.cpp (four places)

a. b_refresh_full: after `  g.back.px = g.root.px;` add `  g.back_terrain = g.root_terrain;`
b. b_surface_blit3: after `  copy8(it->second, g.root, x, y, w, h, x2, y2);` add `  g.note_root_write(x2, y2, w, h);`
c. b_fill_region: after `  copy8(it->second, g.root, x, y, static_cast<int32_t>(a[4]), static_cast<int32_t>(a[5]), x, y);` add
   `  g.note_root_write(x, y, static_cast<int32_t>(a[4]), static_cast<int32_t>(a[5]));`
d. gfx_progress: after `    g->snap_unsupported.push_back(g->unsupported.size());` add `    g->snap_terrain.push_back(g->back_terrain);`
e. a_log: after the `snapshot ... unsupported before it` loop add

```cpp
  for (size_t k = 0; k < g.snap_terrain.size(); ++k)
    s += "snapshot_terrain\t" + std::to_string(k) + "\t" + std::to_string(g.snap_terrain[k]) + "\n";
```

### 1.4 port/test/framegate.py (LiveTerrain on both backends, S0 recommendation 5; the terrain flag as a column)

a. COLUMNS: append `"terrain_drawn"` after `"reference"`.
b. `    use_live = live and backend == "x64" and lib.has("gfx.attach")` -> `    use_live = live and lib.has("gfx.attach")`
c. `    if use_live:\n        _bind_live(lib.dll)` -> `    from port.test import terrainlive as TLm\n    if use_live and getattr(lib, "dll", None) is not None:\n        _bind_live(lib.dll)`
d. `lib.dll.sg_ts_live_attach(st.ctx, va, None, 0) != 0` -> `TLm.live_attach(lib, st, va) != 0`
e. the six-line sg_ts_live_log block and the `attached=f"0x{lib.dll.sg_ts_live_object(st.ctx):x}"` line ->
   `names = [ln.split()[3] for ln in TLm.live_log(lib, st).splitlines() if len(ln.split()) > 3]` and
   `attached=f"0x{TLm.live_object(lib, st):x}"` (exact text: 2_merge_edits.py item 7)
f. `            lib.dll.sg_ts_live_detach(st.ctx)` -> `            TLm.live_detach(lib, st)`
g. after `row["terrain_rule"] += f"; gfx snapshot_terrain=..."` add `            row["terrain_drawn"] = glog["snap_terrain"][i]`.
The module docstring's sentence "On the Win32 backend ... the runner has no sg_ts_live op, so LiveTerrain is not attached there" is
then out of date: replace it by "On the Win32 backend the runner's ops 28..32 carry the sg_ts_live calls (roadmap S3.2)".

### 1.5 The census rule (re/roadmap/frame_census.tsv, S3.9's census, log/roadmap/S0/3_rank.py)

The S0 rule (`terrain not drawn` = the view region of the FINAL frame fails G2 over the region's area) cannot see the terrain once it
is drawn: the view region also holds sprites, overlay lines and HUD text that S3.5 / S3.7 / S3.8 own, so every in-game row keeps
failing it. The terrain verdict of this step is per checkpoint the port's root surface at the return of the last updateTerrainRender
before the checkpoint against the original's (recorded with re/tools/terrain_calls.py --pass-frames / --frames-lite), by G2 over the
pixels the original's frame drew. log/roadmap/S3/2_census.py `terrain` measures it, `rank` re-ranks a framegate census with it
(log/roadmap/S3/2_census_ranked.tsv). Proposed for S3.9: rank with 2_census.py (or move its rule into 3_rank.py's successor).

### 1.6 UNCERTAINTIES.md: append the rows of log/roadmap/S3/2_uncertain.tsv (U-1310 ..).

### 1.7 Nothing for modules.inc (terrain_live, terrain_state, terrain_dll are listed modules already; gfx_terrain is a family),
CMakeLists.txt (src/sim/*.cpp glob), tick.cpp, commands.cpp, portsim_c.cpp / .h (no new export: the harness uses sg_call
terrainLive.* and the existing sg_ts_live_* calls; the Win32 runner gains ops 28..32 in runner32.cpp, an owned file).

## 2. Gate (expected numbers on a build holding S2 wave 1 + S3 wave 1 + this step; the S3.2 mirror's numbers in 2_notes.md)

```
cd <root> && SG_PORTSIM_DLL=<x64 dll> ~/bin/py -3.12 -m pytest port/test/test_terrain_live.py -q        # course view + controls
SG_PORTSIM_DLL32=<Win32 dll> SG_PORTSIM_RUNNER32=<runner> (same command)                                  # x64 = Win32 test
~/bin/py -3.12 -m port.test.integrator segments --backend x64 --jobs 6 --out <tsv>   # 205 / 205, 0 differ from S6-1
~/bin/py -3.12 -m port.test.boot --all --jobs 3 --out <tsv>                          # HEAD's 61: 58 / 2 / 1, 0 differ from S0
```

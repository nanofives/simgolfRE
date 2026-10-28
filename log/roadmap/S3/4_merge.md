# S3.4 merge: FLIC sheet-cell decoder (module `flic`)

Worker S3.4 (session cmvmrnwb33stzo115k44eqook), 2026-10-24. Notes: log/roadmap/S3/4_notes.md. Own files: log/roadmap/S3/4_files.txt.

## 1. port/src/sim/modules.inc (shared): one line

Append at the end of the file (after `SG_MODULE(terrain_live)`, or after whatever the integrator's tree ends with):

```
SG_MODULE(flic)
```

Then touch port/src/sim/registry.cpp (MSBuild does not rebuild it when only modules.inc changes). Without this line the tree's
port/test/test_static.py::test_every_module_file_is_listed fails (port/src/sim/flic.cpp is a module file), and test_flic.py skips.
port/CMakeLists.txt needs nothing (it globs src/sim/*.cpp).

## 2. port/src/sim/menu.cpp (not S3's file): the hook-up in load_flic

Effect only with a gfx backend attached AND S2 wave 1's gfx_body.h in the tree (flic.cpp tests `__has_include("gfx_body.h")`;
without it sheet_to_gfx returns -1 and does nothing). It writes nothing into the GameState, so sim hashes, segment digests and the
boot gate are unchanged by construction (measured in the mirror without the hook-up: section 4 of the notes).

(a) Include, beside `#include "portsim/sim/gfx.h"`:

```cpp
#include "portsim/sim/flic.h"
```

(b) In `load_flic`, the overflow test's else branch (today):

```cpp
  } else {
    gs.g_sheetFirstCell[id] = base;
    const int32_t cells = frames > 0 && cols > 0 ? (f.delay == -1 ? 1 : mul32(frames, cols)) : 0;
    gs.g_cellCursor = add32(base, cells);
  }
```

becomes

```cpp
  } else {
    gs.g_sheetFirstCell[id] = base;
    const int32_t cells = frames > 0 && cols > 0 ? (f.delay == -1 ? 1 : mul32(frames, cols)) : 0;
    gs.g_cellCursor = add32(base, cells);
    // the cells' pixels for an attached graphics backend (S3.4, portsim/sim/flic.h): loadFlic's frame-by-frame cut of the
    // flic into the sheet's cells g_cellPool[base ..], judged against the original's sheet memory (port/test/test_flic.py)
    if (g_gfx_attached.load(std::memory_order_relaxed) != 0)
      flic::sheet_to_gfx(ctx, path, f.frames, f.delay, f.flag,
                         state_va(ctx, &GameState::g_cellPool) + 0x2cu * static_cast<uint32_t>(base));
  }
```

`path` is load_flic's existing `const uint32_t path = ctx.mem.data_base() + f.path;` (the loadFlic path argument's address).
`f.delay` is loadFlic's fourth argument; the decoder calls it `size` (it selects the crop rectangle, loadFlic's four arms; -1 = one
cell). `f.frames` is passed as given: the decoder applies loadFlic's own rule (size -1 = one row of one cell).

Cost: one decode per load while a backend is attached (about 500 loads at boot plus 114..120 per course). The 1,698 recorded
loads decode in about 2 minutes x64 including the test's comparison (port/test/test_flic.py --all).

## 3. UNCERTAINTIES.md (shared): append log/roadmap/S3/4_uncertain.tsv (U-1330..U-1335)

U-1330 settles U-1701 (S8's row): set U-1701's status to `resolved (S3.4, U-1330)`. Its address column: the six sites are in
loadWorldAssets (0x0043dbe0 .. 0x00442180), not loadInterfaceArt; loadInterfaceArt reaches them through its call of
loadWorldAssets.

## 4. Decision for the lead / S6 (U-1331): tropical courses

The original dies at loadWorldAssets' first `Tree Cerc` load on every tropical course (course type 2) of the shipped install;
the port's load_flic (flic_cols -> 0 columns) runs on. Options: (a) `original` profile = a Fault at that load (SG_E_FAULT, as the
port reproduces other faults), a `fixed` switch that reads `Tree_Cerc` / `Tree_Drac` for the exe's `Tree Cerc` / `Tree Drac`
(the alias run cov_b_s34_hawaii/flic shows what the original then does: 114 loads, every one decoded equal by the port);
(b) leave the port running on with 0 columns (today's behaviour, a divergence with no original run to compare). The fix changes
sim state (the sheet's columns decide later cell allocations), so it is an `ask` row under S6's D5 rule, not a `fix`.

## 5. Gate results in the mirror (port/build/S3_head, c6d7d78b + flic module, no hook-up)

- test_flic.py: 10 passed with the gfx-plumbing build (S3_4_gfx), 9 passed + 1 skipped (no gfx_body.h) with S3_4.
- loadWorldAssets: 443 / 443 table entries of menu_flics.inc recorded and decoded equal (Parkland, Links, Desert, Tropical with the
  alias); all 1,698 recorded distinct loads (boot + worlds, 148,926 cells) equal on x64 and on Win32.
- Segments x64 205 / 205 PASS, Win32 205 / 205 PASS, 0 differ from log/roadmap/merges/S6-1/segs_*.tsv. Boot 58 PASS / 2 KNOWN /
  1 NO_FRAMES of HEAD's 61, 0 rows differ from S0. Port test_static 6 passed; repo static 595 passed / 5 skipped / 2 failed, the
  same two mirror-only shim tests as S3.1's mirror run.

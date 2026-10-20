# 38j merge lines (the title menu and session dispatch in sg_run_frames)

Everything below was built and run in my mirror (`~/bin/py -3.12 log/p38/38j/mirror.py --mine`: the merged working tree,
modules.inc's modules only, plus items 1-3), last at 2026-10-20 09:20 (port/build/38j_run/x64_m10, Win32_m10w).

## 1. port/src/sim/modules.inc
Add one line; I built with it just before `SG_MODULE(tick)` (no name it registers is registered by another module, so the
position does not change a lookup). Until it is there, port/test/test_static.py test_every_module_file_is_listed fails
(port/src/sim/menu.cpp exists but is not listed); the other 5 static tests pass on the working tree with my files:

```
SG_MODULE(menu)
```

## 2. port/src/sim/tick.cpp
`git apply log/p38/38j/tick_cpp.patch` (made against the working tree of 09:20; `git apply --check` passed). The same edits
as anchored text replacements: `~/bin/py -3.12 log/p38/38j/tick_patch.py --apply` (each anchor must occur once; an edit that
is already there is skipped). Hunks:

a. kDrops (your table): delete the row `{"refreshTerrain", "Terrain.dll push (renderer, U-0800)"},`. Module saveload (38b)
   registers refreshTerrain; besides the Terrain.dll call it sets g_terrain820f2a[0], which makes the next
   updateTerrainRender take its full-repaint path, and that path does not set bit 1 of g_tileWallMask[0] (a sim object). The
   preamble calls it in its course block (ML+0xf4e: g_date 0 or g_ui_542df8 set), i.e. on the first frames of every new
   session; with the drop three of the four title-menu segments diverged in g_tileWallMask at the second frame head after
   the menu (build m2; the fourth, cov_b_career_types/02, stopped earlier on a divide error that the then missing sprite
   sheets caused). The other 196 segments' digests are unchanged with the row removed.
b. struct Frame: `#include "tick_menu.inc"` after `#include "tick_frame.inc"` (inside the same warning pragmas).
c. Frame::run, the last two lines: `kMenu` -> `menu_session(F)` (tick_menu.inc: Setup's menu slots, Menu, Session, then
   rebuildTileCoverage and FrameExit::Frame, or FrameExit::Quit); `kRetile` -> rebuildTileCoverage in this step and
   FrameExit::Frame (the original runs ML+0x664 before the frame head where its frame record is taken; before, the record was
   hashed first and the next step rebuilt; no segment digest changes).
d. tick_run_frames: `FrameExit::Quit` stops the run like the old menu stop (detail `mainLoop returned (the title menu's quit)`).
e. parse_trace / the end checkpoint: TraceFrame gets the FRAME snapshot's g_flags, the clock runs keep their first value,
   TraceEnd gets `after_wait` (the final POLL comes after the last frame's H1 wait had ended: the first wait read at
   kSiteFrameMs + 0x20 / + 0x4f that is >= frame head + 87, + 125 with g_flags bit 17); the end-check frame sets
   `in.stop_at_wait = !end.after_wait`. No accepted segment has such an end checkpoint (log/p38/38j/after_wait_census.py:
   0 of 200); the chain checkpoint cov_k1_animals@menu2 (taken in the title menu) does.

COORDINATION with 38i: my hunks are the kDrops row, the include, Frame::run's tail, the three struct fields, the end block of
parse_trace (`if (end.has_poll) {` ...) and the `in.stop_at_wait` line. None touches H1 / H2 (tick_frame.inc), TraceInput's
batching or commands.h. If 38i's patch changes `parse_trace`'s `struct Run` or `TraceEnd`, apply mine with tick_patch.py
(anchored, it reports an anchor it cannot find).

## 3. port/include/portsim/sim/tick.h
`git apply log/p38/38j/tick_h.patch`: `FrameExit::Quit = 5` and the comment above the enum.

## 4. New files (mine)
port/src/sim/menu.cpp, port/src/sim/menu_flics.inc (generated: log/p38/38j/gen_flics.py), port/src/sim/tick_menu.inc
(generated: log/p38/38j/conv_menu.py), port/include/portsim/sim/menu.h, port/test/test_menu.py, port/test/golden_menu.json.
`conv_menu.py --check` and `gen_flics.py --check` say whether a file still equals its generator's output.

## 5. port/test/test_tick.py
The four segments pass now. Replace the OPEN_DIVERGENT assignment (lines 30..32) with:

```python
# no COMMAND segment diverges for a cause outside this module (the four title-menu exits run through tick_menu.inc since
# phase 3.8 stream 38j: port/test/test_menu.py test_segments_through_the_title_menu)
OPEN_DIVERGENT: dict = {}
```

and in NEGCTL_BLIND's drawRecords text `cov_k1_animals/00's 18 frames` -> `cov_k1_animals/00's 57 frames` (still blind there:
measured with the m10 build, the four title-menu segments PASS with drawRecords mode 1).

## 6. port/test/integrator.py (38f's; dead code after item 2, safe to leave)
- The BREAK logic (`MENU_EXIT`, `menu_break`) can no longer trigger: sg_run_frames no longer stops at the title menu. Its unit
  test test_integrator.py `menu_break` still passes (it feeds a synthetic row).
- `SEGMENT_MENU_EXITS` and `SEGMENT_MENU_EXIT_FRAME` explain rows that no longer exist: set both to `{}` (or delete them and
  the two ledger branches that read them).
- MISSING_CAUSES, add the names the menu path can now stop on (U-1050):
```python
    "showThemePackSelect": ("the title menu's Select A Theme path is not ported", "U-1050", "38j"),
    "__open": ("the title menu's quit (top10.sve, credits) is not ported", "U-1050", "38j"),
    "loadInterfaceArt(full != 0): the boot art (mainLoop's Setup)": ("mainLoop's Setup is boot code, not run", "U-1050", "38j"),
```
- After merging, `integrator ledger --run --dll <dll>` should lose the 4 segment rows, the 8 BREAK rows, the 6 chain FAIL rows
  of 38a's segments and the menu2 row.

## 7. re/analysis/sim/mainLoop_structure.md (U-1053, resolved: the selections 2 and 3 were swapped)
In "TITLE MENU", replace the sentences from "Selection 3 = Play a Championship" to "(SEH state F+0x124c=2)." and "Selection
2 = Sandbox Mode ..." with:

> The hotspots 0..5 are the labels Continue Saved Game, Start New Game, Select A Theme, Play a Championship, Sandbox Mode and
> the quit spot (label pointers F+0x1f4 + 4 i, 0x0040f779..0x0040f7b6; hotspot table F+0xf4, 0x0040f705..0x0040f771); the
> table 0x00421b04 maps them to the selections 1, 0, 4, 2, 3, -2. Selection 3 = Sandbox Mode: `[0x571fd4]=0x186a0`,
> `[0x59e7b8] |= 0x1000000`, `[0x5a6364]=0x11`, `[0x543cfc]=0x3fff`, `[0x822c88]=1` (0x0042140c..0x00421439). Selections
> 0/-1/2 build the difficulty prompt and run `selectDifficultyScreen` 0x0043a400 (0x004214fb); -1 from it returns to the
> menu head. Selection 2 = Play a Championship then sets `[0x5a34e0]=0xff` and `[0x59e7b8] |= 0x4000000` (0x00421541..
> 0x00421557). Selection 1 (Continue Saved Game) sets `[0x822c88]=6` (0x004215f7). Selection 4 (Select A Theme) runs
> `showThemePackSelect` and a preview Image at F+0xe4c and loops to 0x00420e35; mode 5 shows a second preview at F+0xb94.

In "SESSION DISPATCH" replace "if the sandbox flag `[0x0059e7b8] & 0x4000000` is set it calls `startNewCourse`" with "if the
championship flag `[0x0059e7b8] & 0x4000000` is set it calls `startNewCourse` (the course list loadGameBrowserScreen(1, 1),
the pro list pickAProScreen, loadGolfPro)", and in "Globals" `0x4000000 sandbox, 0x1000000 championship` with
`0x4000000 championship, 0x1000000 sandbox`.

## 8. UNCERTAINTIES.md
Rows U-1050 .. U-1053 from log/p38/38j_uncertain.tsv (U-1053 is resolved: it records the doc correction of item 7).

## 9. CLAUDE.md (suggested paragraph, port section after the 3.8 entries)
See log/p38/38j_notes.md, last section.

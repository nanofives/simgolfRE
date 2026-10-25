# S1.3 notes (U-1066 parts 2 and 3: theme-screen Cancel, build-palette hotspots 21 / 22)

Worker: claude2 sonnet, instance `instances\s1_3` (a copy of original\ with the PINNED shim `winmm.dll` 381ed2b2, taken from
instances\p4q: original\winmm.dll is a newer build and would make references incomparable), SIMGOLF_ALLOW_MULTI=1,
C3FT_AUDIO=none. Nothing was booted in original\; no commit, no stage.

## Result

| scenario | pinned (det_a == det_b == det_c) | capture c_a vs det_b | trace | boot gate |
|---|---|---|---|---|
| cov_r_theme_cancel (6 checkpoints: menu cancelled loaded cancelled2 career end) | SAME | SAME | 526,803 B, validate OK | PASS 123 / 123 |
| cov_r_palette_hot (6: sandbox hover_undo undo_help hover_wrench amenities end) | SAME | SAME | 734,098 B, validate OK | PASS 565 / 565 |

Controls (`log/roadmap/S1/3_controls.log`, boot run, port/build/S1_3):
- showThemePackSelect 2 (Cancel keeps the theme) on cov_r_theme_cancel: RED at record 0 (g_theme and the theme-loaded tables
  g_golferTypes, g_storyDefs, g_golferPhrases, g_golferTypeState, g_golfPros, g_namedStoryIndex). It read GREEN on cov_b_title,
  ui_tour, ui_tour_v before.
- showThemePackSelect 1 on cov_r_theme_cancel: GREEN by construction (the final theme is Standard either way); RED on
  cov_b_title as before. Not a blind spot of the pair.
- loadInterfaceArt 3 (both palette cuts skipped) on cov_r_palette_hot: GREEN in the boot run, so the step's gate "control 3 now
  reads RED" is NOT met by hash. Reason (U-1170, measured): SpriteRefs 84 / 88 (0x00571b28, 0x00571bd8) are touched by two
  functions only, loadInterfaceArt (the cuts, +0x4fb3 / +0x4fd4) and showInfoCursorPanel (0x004336d8 / 0x004336fd: Surface_drawScaled
  with the SpriteRef as `this`, when the hit-test code in ebp is 0x15 / 0x16): re/model/inventory_addresses.tsv rows 0x00571b28 and
  0x00571bd8. hitTestSlot432f90 walks slots 0..15 (terminator 0xff at 0x004c79b0) and reads SpriteRefs 0, 4 .. 60 (+0x1c). The
  hotspots 0x15 / 0x16 are returned from its fixed approxDistance tests (0x00432fa2..0x00432fd7), never from a SpriteRef height.
  g_sprites570cb8 is a `pointer` object and never compared.
- Instead judged as STATE: `port/test/test_s1_3_scenarios.py::test_palette_cuts_equal_the_original_after_the_hotspots`
  (5 checkpoints): all 96 cut fields (+0x18 / +0x1c / +0x28) of the boot run at the record of the checkpoint's frame head equal the
  original's `<checkpoint>.fh.bin` (SpriteRef 84 = 0x22 x 0x28, 88 = 0x22 x 0x22, kind 0 in the image), and control 3 differs in
  exactly SpriteRefs 84 and 88. A rendered comparison of showInfoCursorPanel's overlays belongs to phase 5.
- loadInterfaceArt 2 (first interface flic not loaded) on cov_r_palette_hot: RED at record 1 (unk_005a9cc8), as a sanity probe that
  the scenario's run is sensitive to the module.

## What the scenarios do (re/tools/scenario_s1_3.py)

cov_r_theme_cancel: Select A Theme (630,425) -> row 0 (100,109) -> Cancel (749,556 = hot button 1 at (0x2ED,0x22C)) -> again row,
Load (615,555 = hot 0 at (0x267,0x22B)) -> a third visit: row, Cancel -> Start New Game -> Impossible -> Northeast -> HUD -> 6 virtual s.
The record run reads g_theme (0x00567328 = SG_OBJECT g_theme, offset 0x0a6328 from 0x004c1000) before, after the row press and after
each Cancel and stops unless the row changed it and Cancel restored the first value (obs in the schedule). Two findings that shaped it
(U-1171): listThemes sets g_theme to "Standard" on entry, so Cancel only shows after a row press in the same visit; and a sandbox
ignores g_theme (controls 1 and 2 stayed GREEN on the first two versions, which ended in a sandbox, then in a sandbox plus a Cancel
from a Standard theme), a career reads it. The title menu has no FRAME record, so the scenario has to play to be a boot-gate scenario.

cov_r_palette_hot: sandbox Monterey, golf palette (45,470) open: hover Undo (261,578 = hit-test 0x15 at (0x105,0x242)), click it
(clickSlotPanel 0x00433040 draws the undo help window and waits in pumpAndRedraw(0)), Esc; hover and click the wrench tab (237,525 =
0x16 at (0xED,0x20D): [0x0055e924] = 1, the amenities page), a click at the Undo spot on that page (clickAmenityPanel), wrench again,
palette closed, a hole built with the page-1 tools, Undo with a hole, 10 virtual s. Frames of undo_help and amenities were read:
the help text window and the amenities page (bench, bridge, landmark, flower, tree buttons) are on screen. The wrench tab (235,527) is already
clicked in re/tools/scenario.py (nine call sites); no `261,578` click exists there, so the Undo hotspot is new.

## Reproduce

```
~/bin/py -3.12 re/tools/instances.py create s1_3 ; copy instances\p4q\winmm.dll over instances\s1_3\winmm.dll  (pinned build 381ed2b2)
set SIMGOLF_ALLOW_MULTI=1 C3FT_AUDIO=none
~/bin/py -3.12 re/tools/scenario_refs.py regen <scenario> --rerecord --jobs 1 --instances s1_3
~/bin/py -3.12 re/tools/segment_corpus.py --instance s1_3 capture --scenario <scenario> --label c_a
~/bin/py -3.12 re/tools/segment_corpus.py --instance s1_3 trace --scenario <scenario>
cmake -S port -B port/build/S1_3 -A x64 ; cmake --build port/build/S1_3 --config Release --target portsim_c   (VS Build Tools cmake)
SG_PORTSIM_DLL=port/build/S1_3/Release/portsim_c.dll ~/bin/py -3.12 -m port.test.boot cov_r_theme_cancel cov_r_palette_hot
SG_PORTSIM_DLL=... ~/bin/py -3.12 -m pytest port/test/test_s1_3_scenarios.py     (8 passed, 21 s)
```
After editing a scenario: delete log/p2/p24/<scenario> and port/corpus/traces/<scenario>.trace(.json) first (capture / trace refuse a
stale schedule copy and a differing trace); `regen --rerecord` replaces the schedule.

## Gates

No port source changed, so the older gates have no new input: segment gate and the boot gate of the 60 older scenarios were not
re-run (the boot gate of the two new ones is above; tests/test_scenario_refs.py and tests/test_scenarios.py static: 4 passed).
re/analysis/data/scenario_refs.json gained the two entries (149 inserted lines, older entries untouched, `updated` stamp changed).

## Open

- U-1170 (cuts judged as state, not by hash), U-1171 (resolved for Cancel; the Load kind 2 arm and arrow buttons 2 / 3 not pressed),
  U-1172 (no segment corpus for the two scenarios). Rows in log/roadmap/S1/3_uncertain.tsv.
- Merge text for the integrator: log/roadmap/S1/3_merge.md.

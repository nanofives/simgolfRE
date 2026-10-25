# S2.7 merge notes (the boot screens: loading, Select Difficulty, theme packs, Pick A Pro)

Base: the S2.4 tree (port/build/S2_4_head = HEAD c6d7d78b + waves 1-3: S2.1's files + 1_test_static.patch, S2.3's files +
3_shared.patch, S2.4's files + 4_shared.patch). Everything was built and judged in the mirror `port/build/S2_7_head` (a copy of
S2_4_head's tree, junctions re-created per log/roadmap/S0/export.md; S2.6's re/tools/scenario_s2_6.py, s2_6_runs.py and
port/test/test_s2_6_scenario.py copied in; boot.py as S2.4 left it: the orchestrator refused the record-771 KNOWN entry, and
none of S2.7's checkpoints is past record 771). Build: x64 `port/build/S2_7` through log/roadmap/S2/build.py.

## 1. New files (copy from the mirror, `port/build/S2_7_head/<path>` -> `<path>`)

- port/src/sim/gfx_boot_screens.cpp     family `boot_screens` (13 bodies, header lists them and the 13 control modes)
- port/test/test_gfx_boot_screens.py    32 tests

Copy them together with item 2 (gfx_families.inc names the family; a gfx_<f>.cpp missing from it fails test_static).

## 2. Shared-file edits

Two variants of one patch (LF), `git apply <patch>` from the repo root, AFTER 3_shared.patch and 4_shared.patch:

- log/roadmap/S2/7_shared.patch       against the S2.4 tree (checked with `git apply --check` on S2_4_head's four files)
- log/roadmap/S2/7_shared_wt.patch    against TODAY's working tree + 3_shared + 4_shared (checked the same way on a scratch copy):
  the working tree's menu.cpp has a sibling's uncommitted `cm::pump_load(ctx);   // +0x54 pumpInput483bd0` after the
  loading-screen line of load_flic (and its comment change); in this variant S2.7's loading-screen block keeps it right after
  the block (the original's order: showLoadingScreen, refreshFull, waitTicks(10, 0), then pumpInput483bd0 at +0x54).
  Use 7_shared.patch if that line is not in the tree when wave 4+ is applied, 7_shared_wt.patch if it is.

Every hunk is inside one of these functions (S2.8 / S2.9 patch other functions of the same files):

| file | function | change |
|---|---|---|
| port/src/sim/menu.cpp | load_flic | the loading-screen `if` becomes a block: with a backend attached, Window::refreshFull(root, 0) (+0x3c..+0x43) and waitTicks(10, 0) over the source's clock reads at its three sites (+0x48..+0x4c); controls 11 / 12 read through the registry switch of gfx.family.boot_screens |
| port/src/sim/menu.cpp | pick_a_pro_screen | the drawing (setup / teardown bodies, each redraw: background, disabled buttons, title, rows with the selection fill, the card, refreshFull; hover images; tips; the loading message); control 13 through the registry switch |
| port/src/sim/menu.cpp | show_theme_pack_select | setup / redraw / hover / tip / teardown body calls |
| port/src/sim/commands_screens.inc | select_difficulty_screen | setup, per-pass redraw before the pump, the dwell counter and the back button's tip, refreshFull after the choice, teardown |
| port/src/sim/commands_screens.inc | show_loading_screen | keeps the two Random::range results and hands them to showLoadingScreen.draw |
| port/src/sim/tick_boot.inc | BootSetup | Setup's title art (ML+0x24e Surface_blit3, the seven items, five labels) with a backend attached; control 10 |
| port/src/sim/gfx_families.inc | (list) | `SG_GFX_FAMILY(boot_screens)` appended (after routing; S2.8 / S2.9 append their own lines: keep all) |

Nothing else: modules.inc, CMakeLists.txt, gfx.cpp, gfx.h, gfx_body.h, tick.cpp, commands.cpp, test_static.py unchanged.

Without a backend attached every new line is skipped (one relaxed atomic load), so the segment and boot gates cannot move; both
were re-run anyway (7_notes.md).

Body names are screen-scoped (`showThemePackSelect.*`, `pickAProScreen.textSurface` ...). Two pieces duplicate a shared helper
on purpose, so S2.7 touches no shared drawing file: draw_message in gfx_boot_screens.cpp is gfxb::draw_message_buffer plus
drawTextSurface's shadow (gfx.cpp's version reports that shadow as unsupported), and pickAProScreen.textSurface is
drawTextSurface. If another step adds generic `drawTextSurface` / `Surface_drawScaled` / drawMessageBuffer-with-shadow bodies,
these can call them; gfx.check counts a name registered twice, none is now.

Frame-object keys: gfx_local_key(0x70000 .. 0x73fff) (loading 0x70000, difficulty 0x71000, theme packs 0x72000, pick a pro
0x73000). S2.4 uses 0x4000 / 0x4400, the core 0x0 .. 0x1000.

## 3. UNCERTAINTIES.md

Append the 4 rows of log/roadmap/S2/7_uncertain.tsv (U-1260..U-1263). U-1252 (S2.6) is settled by this step: set its status to
`resolved` with "the 2,170 px are showThemePackSelect's column-5 cell, drawn at an uninitialised stack word (U-1260); the trace
recorder changes that word; port with no cell = t_rec = t_rep1, with w = 0x1a = det_b (test_gfx_boot_screens.py)".

## 4. Decision for the lead (U-1260)

The port's default for showThemePackSelect's unwritten column-5 word is w = 0x1a, the value the four pinned runs without the
trace recorder show: theme_packs then equals det_b (the census reference). `none` would equal the trace runs instead. Neither
is the original's own value without the harness (not measured).

## 5. CLAUDE.md paragraph (after the S2.4 paragraph)

```
Roadmap S2.7, the boot screens (2026-10-24, notes log/roadmap/S2/7_notes.md, U-1260..U-1263): cov_r_s2_screens loading_t200,
loading_t300, theme_packs, difficulty, champ_courses and pick_a_pro equal the original's back buffer in every pixel. Family
`boot_screens` (gfx_boot_screens.cpp): showLoadingScreen.draw (picture r and hint r2 from the screen's two Random::range draws,
drawMessageBuffer with drawTextSurface's shadow, the bar), selectDifficultyScreen / showThemePackSelect setup, redraw, hover, tip,
teardown, pickAProScreen setup / textSurface / teardown; the screens' port functions issue them, Setup now draws the title art
under the boot's loading screens, and load_flic issues the refreshFull and consumes waitTicks(10, 0)'s clock reads that follow
each boot loading screen (a checkpoint inside that wait shows it). U-1252 settled: showThemePackSelect draws a sixth column cell at
an uninitialised stack word w (U-1260); the pinned runs have w = 0x1a, the trace recorder's runs a w that hides it; the backend
uses 0x1a (SG_GFX_THEMEPACK_COL5 overrides). Controls 13 modes RED (11 on the frames, 2 in unit tests).
```

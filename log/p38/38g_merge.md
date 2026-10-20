# 38g merge lines (for the parent)

## 1. port/src/sim/modules.inc (append one line)

```
SG_MODULE(routing)
```

Without it `port/test/test_static.py::test_every_module_file_is_listed` fails (routing.cpp is not listed) and
`showRoutingMapScreen` is not in the registry (tick then stops on it). My own builds use `log/p38/38g_mirror.py` (the working tree's
tracked port/ + holestats / reports / routing, modules.inc = working tree + this line) into `port/build/m38g`, `log/p38/38g_build.sh`.
The module registers one function and one defect switch (`showRoutingMapScreen.exitRemapReadsFrame`, ref U-1020, ON = original).

## 2. port/test/test_tick.py OPEN_MISSING

Delete the entry (ported now):

```
    "showRoutingMapScreen": "3.7b gap (F5 map; reached by ui_tour/00 twins after showCourseReport, 38c)",
```

ui_tour/00 and ui_tour_v/00 now run showRoutingMapScreen and stop later: 121 of 247 frames in `switchToCourse`, missing name
`courseActionMenu` (38b's item after 38a; it is not in OPEN_MISSING yet, add `"courseActionMenu": "not registered (ui_tour/00 twins, 38b)"`
unless 38b registers it first). No hash divergence in the 121 frames.

## 3. re/port/commands_split.tsv (replace the gap row; columns addr name class command_effect dropped evidence gap)

```
00456be0	showRoutingMapScreen	command	hole / waypoint records and tile hole numbers moved by a right click, g_ui_4d20dc mode set by a button click, walker kind word rewrite of the listed rows, golfers' hole bytes through the inverse permutation, g_holesPlusOne at exit, spreadAllShade in mode 1, appendCourseTitle / appendWalkerName / appendDate / appendNumber calls	map, hole and walker drawing, display slot calls, waits	emulator oracle 400 + edges, golden 120	-
```

## 4. UNCERTAINTIES.md

Append the three rows of `log/p38/38g_uncertain.tsv` (U-1020 .. U-1024).

## 5. CLAUDE.md (one paragraph, after the 38c line)

```
Phase 3.8 38g routing (2026-10-20, port/src/sim/routing.cpp, tests port/test/test_routing.py + golden_routing.json, notes log/p38/38g_notes.md, U-1020..U-1022):
showRoutingMapScreen (F5 routing map) as a command effect over the 3.7b pump model: hole reorder by right click (hole, waypoint and tile hole-number
writes), mode buttons, walker list rows, exit remap through the inverse permutation; 400 oracle cases + 112 hit-box edge cases GREEN, 9 negative controls
RED; defect switch exitRemapReadsFrame; ui_tour/00 twins now pass it and stop in courseActionMenu (121 / 247).
```

## 6. Continuation (2026-10-20, parent's follow-up): showShortcutsScreen and showMembershipRoster

Both live in module `routing` (port/src/sim/routing.cpp, no new module line), tests port/test/test_shortcuts.py (+ golden_shortcuts.json) and
port/test/test_membership.py (+ golden_membership.json). Section 2 additions:

- port/test/test_tick.py OPEN_MISSING: nothing to add for these two; if the parent's tree still lists `showShortcutsScreen` /
  `showMembershipRoster` / `courseActionMenu` there, delete them (all three are registered now).
- re/port/commands_split.tsv rows (columns addr name class command_effect dropped evidence gap):

```
0044e770	showShortcutsScreen	command	g_ui_569498 cleared on the way out, the close-button press / key that ends the loop (pump model)	two backgrounds, 63 captions, close button states, display slot call, waits	emulator oracle 300, golden 120	-
00454c50	showMembershipRoster	command	bit 7 of byte +2 of all 76 golfer-type records cleared at every redraw (exclusion bit lost, U-1023), g_ui_569498 cleared, zone / press loop (pump model)	roster drawing, row selection by name, scroll offset, icons, display slot call, waits	emulator oracle 300, golden 120	-
```
(the first row replaces whatever row 0044e770 has in the file; the second replaces the `gap` row at line 41.)

- UNCERTAINTIES.md: U-1023, U-1024 (log/p38/38g_uncertain.tsv, appended to the file).
- Segments: ui_tour/00 and ui_tour_v/00 PASS 247/247 on the working tree + routing (module lines already merged) built by
  log/p38/38g_build.sh (log/p38/38g_cmdsegs3.tsv). Each step's stop: routing 121 -> courseActionMenu was already registered by 38b so the parent's
  tree went 178 -> showShortcutsScreen; with it 202 -> showMembershipRoster; with it PASS. CLAUDE.md paragraph: extend the 38g line with
  "showShortcutsScreen, showMembershipRoster; ui_tour/00 twins PASS 247/247".

# Stream 38i: the command-API extension for gate (d) (2026-10-20)

Task (parent, from log/p38/38f_notes.md sections 5 and 11, U-1011): a game driven entirely by semantic commands, with no
RootHandler command; gate (d) = championship/00..02 PASS that way, then the count over the COMMAND segments; the raw path
unchanged. Brief log/p38/BRIEF.md; U-rows log/p38/38i_uncertain.tsv (U-1040..U-1042); merge lines log/p38/38i_merge.md;
files log/p38/38i_files.txt; run outputs and scripts log/p38/38i/.

## 1. Result

Final numbers on the working tree of 2026-10-20 09:56 (38h's chareditor and 38j's title menu merged; 38i rebased on it:
port/build/38i_run/x64_20261020_095718, base x64_base_20261020_095645, Win32_20261020_100941). Section 1b keeps the
first round's numbers (tree of 08:37, before those merges).

| check | result |
|---|---|
| gate (d) championship/00..02, semantic commands only | PASS / PASS / PASS (636, 577, 394 frames), in the probe run (click targets unstated) and the final run (targets stated and checked); 145 dispatches, 0 gaps, 88 semantic commands, every recorded batch replaced (applied = batches) |
| same on the Win32 backend | PASS x3, per-frame digests equal to x64 (log/p38/38i/gate_champ_win32.tsv) |
| all accepted COMMAND segments, semantic only | 165 / 165 PASS, every one IDENTICAL to the raw run of the same build (status, frame count, per-frame digest over every T and 203 sim hashes and the end check; log/p38/38i/gate_all2.tsv vs raw_after2.tsv). The 155 COMMAND segments of the plan: 155 / 155 PASS; course_season_6y's 10: PASS. Every recorded batch replaced in every run, 0 clicks without a route |
| translation | 10,420 recorded dispatches (15,852 handler calls) in the 165 segments: every one translated, 0 gaps. 6,122 semantic commands; 4,301 releases need none |
| raw path unchanged | `integrator segments` over all 200 accepted segments on the build without 38i's files and edits (port/build/38i_base) and with them: 200 rows, 0 differ (`integrator compare`), 200 PASS each (raw_before2.tsv / raw_after2.tsv) |
| negative controls (semantic module, championship/00..02) | 1 left click lowered as a right click: FAIL at records 200 / 151 and a refused click; 2 a character key without its WM_CHAR: FAIL at records 9 / 10 (the first Escape) and a refused click; 3 a true target refused: FAIL (refused at the first click) |
| named key actions for WM_CHAR-only deliveries (`--named-chars`) | the 25 segments that hold them: 0 digests differ from the raw run |
| route cross-check | see section 5 |
| tests | port/test/test_semantic_input.py 11 (3 static, 8 port) + port/test/test_integrator.py 15: 26 passed on the rebased build (SG_PORTSIM_DLL); port/test/test_static.py passes except test_every_module_file_is_listed, which waits for `SG_MODULE(semantic)` |

## 1b. First round (tree of 08:37, before 38h / 38j were merged)

All 165 COMMAND segments semantic-only were identical to their raw runs: 160 PASS; the 5 others stopped where raw stopped
(cov_a_keys3/01 at showCharacterEditor, cov_b_career_types/00..02 and cov_k1_animals/00 leaving for the title menu), i.e. 150
of the plan's 155. Raw before / after on that tree: 200 rows, 0 differ (195 PASS / 5 FAIL each; raw_before.tsv /
raw_after.tsv, gate_all.tsv).

## 2. The design: input actions lowered to the game's own handlers

The 3.7b semantic kinds (0x100..0x106) run a click handler's effect directly, so they skip mainLoop H2's press code (U-1011).
The new kinds (0x200.., portsim/sim/commands.h after the merge; portsim/sim/semantic.h; module `semantic`,
port/src/sim/semantic.cpp) state what the player DOES and are LOWERED to the root-handler calls that input makes
(commands.cpp's root_handler, the same code RootHandler commands run) at the dispatch point they are applied at. Everything
after that is the game's own code, unchanged: H1's takeKey / takeCode and its two key switches, H2's press code and routing
(g_cachedCameraTileX = -1, screenToTile of the click, the panel handlers, the world action, the return to the frame head),
and the modal screens' hit tests. So a semantic command has exactly the effect of its input, H2 included, by construction.

| kind | arguments | lowering |
|---|---|---|
| Key 0x200 | arg[0] KeyAction (75 named keys, semantic.h); KeyAction::Char: arg[1] character, arg[2] virtual key (0: a character with no key-down, arg[3] the code jgld passes) | character key: onChar(vk), onKey(ch, vk); non-character key: onKey(0, vk); Char with vk 0: onKey(ch, code) |
| PointerDown 0x201 | x, y, button 1 / 2 | onPoint(x, y) / onPoint2(x, y) |
| Click 0x202 | x, y, button, modifiers (bit 0 Ctrl, bit 1 Shift), target, two target arguments | the target checked (below), then onPress1(x, y) + onUnpoint, or onPress2 + onUnpoint2; the press handlers' GetAsyncKeyState samples are 0x8000 for a held modifier (they keep bits 8..15) |
| TileClick 0x203 | tile x, tile y, button, modifiers | PointerDown + Click at the tile's screen centre (tileToScreen), refused when that point does not select the tile (screenToTile) or the tile is off screen |

KEY ACTIONS. The H1 char switch has 56 cases (the byte table read by sw_h1_key, chars 9..0x7a), the code switch 16
(0x21, 0x24..0x28, 0x70..0x79): every one has a named row (test_key_table_names_every_h1_case reads both tables out of
tick.cpp), plus PageDown, End (no H1 action) and Backspace (text boxes). Where the case depends on the panel mode, the name
gives both (ShotStyle0OrList7 = 's': shot style 0 in panel mode 3, panelListSelect(7) otherwise). Bindings: a character key
is onChar(vk) then onKey(ch, vk) once or twice (the second onKey is identical or, for '+' / 'n' / 't' / 'p', another
character of the same key, and the last one counts), a non-character key is onKey(0, vk) alone. The pairs are the recorded
ones for Escape, Tab, Enter, Backspace, the letters b d e f h l n p s t x z, '+', '-' and all 18 non-character keys (PageUp, PageDown, End, Home, the arrows, F1..F10); the
other rows take the US-layout key of the character. A character key's virtual key reaches H1 only through the code switch,
whose cases no such key hits, so H1's effect depends on the character alone. What some effects mean in the game is not
determined (U-1041).

WHAT A LOWERING LEAVES OUT, with the evidence (re/model/inventory_addresses.tsv):
- the key RELEASE (WM_KEYUP -> rootWin_onChar0): its only write is g_char83821c (0x0083821c), which the exe only writes
  (refs 2: onChar, onChar0). A Key action is a tap; the 4,301 release batches translate to no command.
- g_key838208 (0x00838208): written only (onKey, refs 1).
- the deadline clock of onPoint / onUnpoint / onKey (they clear g_ui83ab9c, read only by BinkPlayer::play, when the deadline
  word g_audio5aa774 is 0 or the clock passed it): the word is 0 in all 200 segment images (log/p38/38i/deadline.py) and
  only playAudioFile writes it; a lowering with the word set and no clock in arg[7] throws MissingResource.

CLICK TARGETS (semantic.h ClickTarget). A Click states what it is for; the lowering evaluates H2's first-level routing (the
press-code entry 0x0041e36c as tick_frame.inc transcribes it) on the state at the dispatch point and refuses the command
(CommandRefused, SG_E_NO_RESOURCE) when the stated target does not hold:
Any (no check: modal screens), World (+ the tile screenToTile gives the point), Panel (+ the panel kind 0x106 / 0x104 /
0x102 / 0x100 by panel mode 0 / 1 / 2 / 3-4, + its hit test's control), Hud (x < 0xc8, y > 0x1b8: showSystemMenu), Swing
(panel mode 3, left button, no tool word, no demolish flag, y < 0x1e0: the shot click that copies the aim), Place (a pending
placement f_w_4c2e00 + 0x10), GolferAction (g_msgState53df54 > 0 and g_infoPanelTab != -1), TerrainEdit (g_flags bit 1: H2
maps the click to '=', '-' or '/'). screenToTile and tileToScreen write the tile screen cache, so the route runs them on a
copy of the whole GameState (restored after; test_click_route_and_refusal checks the image is unchanged). The hit tests
are pure. The route is evaluated before H2 runs (U-1042).

The new kinds build against the unpatched commands.h (semantic.cpp switches on the numbers, semantic.h's kKind*), so the
file can sit in port/src/sim before the merge without breaking a build (checked: port/build/38i_noedits).

## 3. The translator and the gate (port/test/semantic.py: `xlate`, `gate`)

    ~/bin/py -3.12 -m port.test.semantic xlate --segment championship/00 ... [--named-chars] [--out <tsv>]
    ~/bin/py -3.12 -m port.test.semantic gate --build 38i --segment ... | --all [--jobs 2] [--named-chars]
                                              [--negctl semantic.apply=<mode>] [--backend win32] [--out <tsv>]

Per recorded batch (port/test/trace_commands.py): onChar(vk) + onKey(ch, vk).. -> Key(the action bound to (vk, last ch)),
else Key(Char); onKey(0, vk) -> Key(the non-character key); onKey(ch, code) alone -> Key(Char, ch, vk 0, code) (exact) or,
with --named-chars, the action of ch; onChar0 -> nothing; onPoint / onPoint2 -> PointerDown; onPress + onUnpoint -> Click;
anything else a named gap (none occurred). In a modal dispatch a printable character is written Key(Char) (text typed into a
screen; the same lowering as its named action). The run: integrator.setBatchOverrides (38f) with EVERY recorded batch's
replacement, so no RootHandler command reaches the port from the trace (applied = batches in every run). Click targets: a
PROBE run with every target Any and the semantic module's click log on gives the route of each frame-body click; the FINAL
run states them and the port checks them. Modal-dispatch clicks keep Any.

What the 10,420 dispatches became (log/p38/38i/xlate_stats.py over gate_all_xlate.tsv): 3,942 Key(Escape); 4,301 releases
(no command); 875 PointerDown; 777 frame-body clicks: World 348, Panel SlotPanelSelect 185, Hud 103, Panel GolferPanelAction
49, Swing 46, Panel BuildingSelect 27, TerrainEdit 15, Panel EmployeeViewSelect 2, Place 1, GolferAction 1; 98 modal-screen
clicks (target Any); 70 Key(Char) (59 WM_CHAR-only deliveries, 11 characters typed into screens); the named keys ShotStyle*
(228), RotatePlus 27, ZoomOut / ZoomIn 15 each, CashCheat 13, the ten F-keys 2 each, arrows, Tab, Enter, Backspace and the
rest (log/p38/38i/gate_all_xlate.tsv has every row).

championship/00..02 (log/p38/38i/gate_champ_xlate.tsv): 50 Key(Escape), 3 Key(ZoomOut), 3 Key(ZoomIn), 57 releases, 16
PointerDown, 16 Clicks: Panel GolferPanelAction control 4 x 5 and control -1 x 2 (the shot-style oval at (415, 508); the same
seven clicks 38f matched to clickGolferPanel calls), Swing x 5, World x 3, Place 0 x 1.

## 4. Findings

- WM_CHAR WITH NO KEY-DOWN (U-0654, partly answered): the cov_* drivers post some characters as WM_CHAR alone; jgld then
  calls rootWin_onKey(ch, code) with the code of the MOST RECENT key-down, whether it reached onChar(vk) (a character key) or
  onKey(0, vk) (a non-character key): 156 of 156 such deliveries in the scenario traces (log/p38/38i/charonly_code.py; codes
  27 after Escape, 72 after 'h', 38 after Up, 112 after F1). That code is jgld state, not GameState, so the exact form
  carries it (arg[3]) and is the translator's default. In H1 the code reaches the code switch (0x0041de11): 27 and 72 fall
  into its default arm like a character key's own code, so there the named form (onChar(vk) + onKey(ch, vk)) has the same
  effect, and it gave the raw digest in all 25 segments that hold such deliveries (they carry only 27 and 72). It is NOT
  equivalent in general: a delivery with code 38 (Up) or 112 (F1), as cov_a_keys and cov_a_keys2 have outside their accepted
  segments, also scrolls the camera or opens the course report through the code switch, which the named form does not.
- The cursor is fully determined by the harness's cursor moves: 344,524 of 344,524 cursor reads equal the last DELIVER cursor
  move before them (1,667 moves; log/p38/38i/cursor_moves.py). The aim of a shot is that cursor; an exact PointerMove kind is
  possible but needs a cursor hook in tick.cpp (U-1040).
- H2 routing measured on the corpus: a click is recognised on button release, and the mouse-down's only effects are the held
  point g_rootInput + 0x34 / + 0x38 (read only by P6's distance overlay, which draws and formats into g_textBuf) and the
  deadline tail; the gate keeps PointerDown as its own command anyway, so no merge of down and up was needed.

## 5. Route cross-check (log/p38/38i/route_check.py, log/p38/38i/route_check.log)

Every Panel and Hud route against the handler the RAW run called (SG_TICK_TRACE of every callee call, one child process per
segment): a Panel route of kind 0x106 / 0x104 / 0x102 / 0x100 must be a call of clickSlotPanel / pickBuilding /
clickEmployeeView / clickGolferPanel with the click's point, a Hud route a call of showSystemMenu; multisets per segment.
RESULT: 66 segments hold such routes, 66 SAME, 0 differ; 366 routed clicks: clickSlotPanel 185, showSystemMenu 103,
clickGolferPanel 49, pickBuilding 27, clickEmployeeView 2, each a call the raw run's H2 made with that point (and no such
call without a route). The World / Swing / Place / GolferAction / TerrainEdit routes are not cross-checked this way (U-1042).

## 5b. With stream 38j's title-menu section (combined check, 2026-10-20 09:21)

`log/p38/38i/mirror.py --with38j`: the working tree of 09:21 (chareditor merged by then) + 38j's in-progress files (menu.cpp,
tick_menu.inc, menu_flics.inc, menu.h; SG_MODULE(menu) before tick; log/p38/38j/tick_patch.py applied) + 38i's files and
edits: it builds (port/build/38i_38j), the four patches (38j tick_cpp / tick_h, 38i commands_h / commands_cpp) pass
`git apply --check` together (they touch disjoint files). Raw: 200 / 200 accepted segments PASS (log/p38/38i/raw_38j_all.tsv).
Semantic only: 165 / 165 COMMAND segments PASS, every digest equal to the raw run (gate_38j_all.tsv), the four title-menu
segments included (their menu dispatches are modal batches: target-Any clicks and keys). 38j was still editing then; the
parent merged it at about 09:50 and section 1's final numbers are on that merged tree (the same 165 / 165).

## 6. Builds and runs

`~/bin/py -3.12 log/p38/38i/mirror.py [--arch x64 Win32] [--base | --no-edits]`: a snapshot of the working tree with only the
modules modules.inc lists (port/build/38i_root), plus semantic.h / semantic.cpp, `SG_MODULE(semantic)` and the two shared-file
edits (log/p38/38i/edits.py, the same text as the .patch files); x64 port/build/38i, Win32 port/build/38i_w32, base
port/build/38i_base, compile check port/build/38i_noedits; DLLs copied to port/build/38i_run/<arch>_<stamp>/ (latest.json:
keys x64, Win32, x64_base, x64_noedits, x64_38j; `--build 38i` reads x64 / Win32; `--with38j` adds 38j's files and tick edits).
Raw before/after: raw_before2.tsv / raw_after2.tsv (final tree; first round raw_before.tsv / raw_after.tsv), DLL paths in
raw_dlls2.txt. Semantic gate: gate_all2.tsv (final; + _xlate.tsv), gate_all.tsv (first round), gate_champ*.tsv,
gate_named_chars.tsv, gate_negctl{1,2,3}.tsv, gate_38j_all.tsv; comparison with raw: log/p38/38i/cmp_gate_raw.py (first round)
and the inline comparisons in the logs. One cl.exe of the base rebuild at 09:33 sat in a kernel wait for 21 minutes with
1 s of CPU; I killed only that build's processes (my mirror.py, its cmake, MSBuild, Tracker, cl) and the rebuild then took
23 s. One pytest at a time, 2 workers for the gate (commit charge stayed above
11 GB free; two large python processes started 09:11 were not this stream's and were left alone).

## 7. Open items

- U-1040 PointerMove (Aim) from semantic commands instead of the trace's CURSOR records (needs a tick.cpp cursor hook).
- U-1041 game meaning of several key effects (names are by effect).
- U-1042 World / Swing / Place / GolferAction / TerrainEdit routes checked only through the runs, not against H2's branch.
- Done after 38j's merge: the four title-menu segments pass semantic-only (section 1). After any later merge that changes the
  frame step, re-run `~/bin/py -3.12 -m port.test.semantic gate --dll <x64 dll> --all --jobs 2` and compare with
  `integrator segments` of the same DLL.
- onPress1b / onPress2b (handlers 10 / 11) have no input kind: they never occur in the corpus (0 of 15,852 handler calls).

# Phase 3.8 stream 38h: showCharacterEditor

Module `chareditor`: port/src/sim/chareditor.cpp, port/include/portsim/sim/chareditor.h, tests port/test/test_chareditor.py,
golden digests port/test/golden_chareditor.json. Not merged: needs `SG_MODULE(chareditor)` in modules.inc
(log/p38/38h_merge.md). Built in a private tree: log/p38/38h/mirror.py (the WORKING TREE's port/, tracked and untracked,
modules.inc = the working tree's + chareditor) and log/p38/38h/build.sh (port/build/p38h, copied to port/build/p38h_run).
Tests run with `SG_PORTSIM_DLL=$PWD/port/build/p38h_run/portsim_c.dll`.

## 1. Specification (port code cites no address)

From the hook sources, read as reference: showCharacterEditor 0x004385d0 (shim/src/re/c3fx.cpp, log/c3/c3fx_notes.txt),
pickCharEditor 0x00438390 (c3em.cpp), nearestCharControl 0x00438260 (c3v.cpp), charEditorZone 0x004382f0 (c3u.cpp).
c3fx.cpp addresses golfer records from 0x005794c0 (g_golfers + 8), so its golfer offsets are 8 below the model's
(+0x10 kind = model +0x18, +0x11 prefs = +0x19, +0xae type = +0xb6, +0xb0 traits = +0xb8, +0xb2 class = +0xba, +0xb3 name
index = +0xbb, +0xa6 = +0xae, skills +0xe8 = +0xf0). Checked against the disassembly where the port depends on it
(log/p38/38h/disasm38h.py):
- release-wait pumps: `call 0x45c030` at 0x0043a235 (return 0x0043a23a) and 0x0043a24f (return 0x0043a254), loops
  `jne` 0x0043a241 / 0x0043a25b; the pass pump at 0x004397b7, button test 0x004397ce.
- pickCharEditor: x in its `push ecx` slot ([esp+0x10] at 0x004383b6), y in its ARGUMENT slot ([esp+0x18] at 0x004383b2,
  `row` already in ebx at 0x00438392), so y starts at row; button 2 0x004384f1, y < 0x102 0x004384fa, button != 1
  0x00438504 (returns 0 at 0x00438578), the scroll steps 0x00438513..0x00438559 (clamp to [0x0059b76c + 4 * row] - 10,
  then to 0).
- the tip test 0x00439776..0x0043977d (`je` when g_textBuffer[0] == 0).
Pool strings by offset in f_strpool_4c84e8 (tips 284 .. 160, bio tip 148, load / save texts 0 / 140 / 132 / 116 / 88 /
72 / 80, prompts 60 / 32 / 48, status texts 500 .. 436, type card 340 / 328 / 312 / 304).

## 2. What was ported

| function | port | oracle cases | jcc both sides | negative controls |
|---|---|---|---|---|
| showCharacterEditor | the whole modal: entry (backup to type record 83, preview golfer 0x99, Random::range(3)), every pass (cursor read, control, figure set, cellAt / addSprite / composeGolferSprite, all texts into g_textBuffer / g_panelNumberText, 20 comment lines with buildGolferComment and g_commentTone, tip counters), the pass pump, every control 0..19, the no-control presses (name / title textInputBox, demographics, comment lines, skill bars), the release wait, the exit's pro looks | 300 | 95 / 109 | 9 RED |
| pickCharEditor | the picker loop | 150 | 17 / 17 | 2 RED |
| nearestCharControl | spot table, dx / 3 for the first 8 | 400 | 3 / 3 | 1 RED |
| charEditorZone | preset centres + 0x3c, the two arrows | 400 | 3 / 3 | 1 RED |

Not ported: the bio entry (control 20 for a non-member), U-1032: MissingResource{"showCharacterEditor: the bio EditBox"}
before any of its writes. Drawing, sound, the screen Image, display slots and drawMessageBuffer (its NULs are put back)
are dropped; the return value is 0 (U-1035).

Missing jcc sides of the editor (log/p38/38h/cover_showCharacterEditor.log), each argued: 0x004387de taken, 0x00439f95
taken (the name area keeps the quotient in 0..3 / 0..4), 0x00439f50 taken (nearestCharControl returns -1..20),
0x0043a294 taken (zero-extended byte compared with -1), 0x0043977d taken (every tip string is non-empty), 0x00439932
taken (listMatchingFiles returns a count >= 0), 0x00438a2b fall-through (preview kind is zeroed at entry; only a real
loadThemeFile on golfer 0x99 sets it, U-1033), 0x00439b1c / 0x00439b36 fall-through (the hair counter starts at 0,
U-1030), 0x00439f02 / 0x00439f0a / 0x00439f0e (the bio loop, U-1032), 0x0043a241 / 0x0043a25b taken (a press inside
the release wait, U-1034).

## 3. Pump model (the one design point)

The editor's pass reads the cursor once and pumps once (Blocking: NeedInput when no input is left). The two release-wait
loops after a press pump differently (pump_release): no input left = nothing (the original's wait just ends), and after
one refusal no further request until the next cursor read, because tick.cpp TraceInput delivers a Blocking request that
it refused twice at the same cursor count ("stuck"), which would hand the next pass's batch to the second release loop.
The test source (chareditor.setScript) refuses the release pumps the way the oracle does (cmdemu with their return
addresses added to DRAIN_CALLERS for the run). In cov_a_keys3/01 the run's cursor reads matched the trace exactly
(over / under 0 / 0, 0 dispatches at another cursor count).

## 4. Oracle (port/test/test_chareditor.py, SG_EMU_ORACLE=1)

cmdemu.py unchanged. Differences from 38d's: View_toLocal and View::fromParent run for real in the original (the port calls
the registered View::fromParent): g_graphicsDevice points at a fake display block (vtable slot +0x2c = the cursor
sequence, +0x9c and +0x38 stubs), the root window's offset / parent fields are 0. composeGolferSprite runs for real on
both sides (tick's addSprite calls it too, so it cannot be scripted on one path only): the compositor [0x0081ca14] is a
fake object whose slot 0x14 is a stub and the part pointers are 0, so the original runs its look choice and write-back
only. Scripted on both sides with compared call sequences: listMatchingFiles (count 0..3 from the salt),
promptAndSaveFile (0 / 1), loadThemeFile, buildGolferComment (U-1033). EditBox::create is a stub that stops the run (the
bio). Keys for the nested modals (textInputBox, showPopup, scrollTextWindow) are an idle WM_KEYDOWN batch (code 0) and the
WM_CHAR batch: two consecutive key batches let flushRedraw's pending flag clear the second key, so the first script
version lost every other key and 125 / 200 cases ended waiting for input (a test-input artifact; the real game's empty
pumps clear the flag). Final: 300 / 150 / 400 / 400 cases equal (log/p38/38h/t_oracle4.log: 23 passed with the oracle);
default suite 19 passed, 4 skipped (log/p38/38h/t_default.log). Equivalent / blind controls found and replaced: the
name-area hover divisor (drawing only: moved to the press test), the tip threshold and the hair step (golden cases now hover
9..12 passes on Exit before pressing it and press control 10 more), the arrow radius (edge points added).

## 5. Segments (log/p3/p37a_cmdsegs.py with the 38h DLL)

- cov_a_keys3/01: was FAIL 1/118 (missing showCharacterEditor, log/p38/38h/segs_base.tsv); now PASS 118/118
  (log/p38/38h/segs1.tsv), 7 dispatches delivered, 0 left, cursor reads over / under 0 / 0. The editor is opened by H1's
  key 0x3f (TAKE 0x0041c0d7 value 63, mainLoop ML+0xe387) and closed by one click on Exit.
- All COMMAND segments (log/p38/38h/segs_all.tsv, 165 rows in the current manifest): 161 PASS, 4 FAIL = the title-menu
  exits already in test_tick.py OPEN_DIVERGENT (cov_b_career_types/00..02, cov_k1_animals/00). No other segment reaches
  the editor.

## 6. Open

- Merge (log/p38/38h_merge.md): modules.inc line, test_tick.py OPEN_MISSING entry removed, commands_split row, U-rows,
  U-0996 resolved, CLAUDE.md paragraph.
- U-rows: log/p38/38h_uncertain.tsv (U-1030..U-1035). The bio EditBox (U-1032) is the one unported path.
- Not run: the Win32 backend (no x87 in this module), the rest of port/test (memory rule: only this stream's file).

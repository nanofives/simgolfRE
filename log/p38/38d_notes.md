# Phase 3.8 stream 38d: skillPointDialog, showCharacterIntro, drawGolferInfoPanel, the scroll text box, showBulletinBoard

Module `dialogs`: port/src/sim/dialogs.cpp, port/include/portsim/sim/dialogs.h, tests port/test/test_dialogs.py, golden
digests port/test/golden_dialogs.json. Not merged: the module needs `SG_MODULE(dialogs)` in modules.inc
(log/p38/38d_merge.md). Built in a private tree: log/p38/38d/mirror.py (port/ at HEAD + this stream's files, modules.inc
= HEAD's + dialogs) and log/p38/38d/build.sh (port/build/p38d, copied to port/build/p38d_run). Tests run with
`SG_PORTSIM_DLL=$PWD/port/build/p38d_run/portsim_c.dll`.

## 1. Specification and addresses (port code cites none)

All from the C3/C4 hook sources, re-read against the disassembly where the port depends on a detail
(log/p38/38d/disasm38d.py):
- skillPointDialog 0x0045f0f0 (shim/src/re/c3em.cpp): entry copy of g_playerSkills (32 bytes) 0x0045f116..0x0045f124;
  enterModal for points > 0 0x0045f13d; golfer -1 writes record 0x99 +0xb6 = 0 and +0xb8 = movsx g_golferTypes+0x21
  (0x0045f1f7..0x0045f213) before drawThoughtBubble(who, .., 1) 0x0045f21f; hover reset 0x0045f261; View_toLocal only
  for points > 0 (0x0045f26f jle, call 0x0045f280; lx = [esp+0x24], ly = [esp+0x18]); row value from the golfer
  (+0xb6 != 0: +0xf0 + i) or g_playerSkills (0x0045f2d5..0x0045f2f6); hover tests signed (jl / jle / jg,
  0x0045f34a..0x0045f3aa; the second `lx < colX` 0x0045f394 repeats the first); text into g_textBuffer and itoa into
  0x00824134 (g_infoPanelNumBuf) 0x0045f3db..0x0045f48e and 0x0045f4ee..0x0045f57b; points == -1 returns at
  0x0045f5b0; waitTicks(5, 0) 0x0045f5c9 (waitTicks 0x0045bf80 with a zero second argument never calls pumpMessages:
  0x0045bfc8 je), pumpMessages 0x0045f5d1, flushRedraw only when the button word is 0 (0x0045f5dd jne); OK test abs
  (cdq / xor / sub) 0x0045f60b..0x0045f63e; `points <= used` jle 0x0045f648 returns movsx word [0x004c2c9c]
  (0x0045f844); popup text 0x004d3bf4 + 0x004d3bb4, showPopup(0x190, 0xc8, 1, 1, 0) 0x0045f6ff; points == 0 test
  0x0045f74c; idiv by 10 0x0045f771; + arrow 0x0045f77a..0x0045f7b3 (or word); - arrow signed compares against the entry
  copy 0x0045f7c5..0x0045f7cd, bit clear at 0 0x0045f7eb..0x0045f7fa; refusal sound 0x0045f822.
- showCharacterIntro 0x004065c0 (c3fa.cpp): 22 records at 0x004c15a0 (= g_bulletinEvents + 0x28), stride 0x30;
  f_w_4c2e00[2] = -1 0x00406641; drawWindow(0xc8, 0x172, 0x80007fff, -2); skillPointDialog(0x004d6098, 0, n, -1, 0xc8)
  0x0040665f; result word 0x00406667.
- drawGolferInfoPanel 0x0045c560 (c3fr.cpp, the jcc addresses are in its BR() calls): the only simulation writes of its
  own are the record's mood byte (+0x5d + hole = low byte of +0xa4 while hole < 0x13, 0x0045cf61 / 0x0045cf67),
  g_infoPanelTab 0x00824130 (= -1 0x0045d22d, then the hovered button) and g_infoPanelHoverFrames 0x00823768; the rest are
  its callees' (skillPointDialog with points -1, composeGolferSprite through drawThoughtBubble 0x0045c632 / 0x0045d71f,
  buildGolferName, golferAge, appendOpinion, thoughtBalance, loadStoryStrings, storyText, substituteStoryNames,
  buildGolferComment) and the text in g_textBuffer / g_infoPanelNumBuf. drawThoughtBubble 0x0045c200 (c3ei.cpp) calls
  composeGolferSprite once (mode != 1, 0x0045c264) or twice (mode 1, 0x0045c264 and 0x0045c32c); its other callees
  typeBit7Clear / thoughtFlag only read.
- drawScrollText 0x0046e260 (c3eh.cpp), runScrollText 0x0046dea0 / scrollTextWindow 0x0046de70 (c3el.cpp),
  resetScrollText 0x00465560 (c3dw.cpp), showBulletinBoard 0x0046e810 (c3em.cpp), pumpInput483bd0 0x00483bd0 (writes
  [0x0083ad4c] = 0x3f, loops showScreen 0x00483bb0, ends with [0x0083ad4c] = 0, 0x00483bd0..0x00483bf6).

## 2. What was ported

| function | port | oracle cases | jcc both sides | negative controls |
|---|---|---|---|---|
| skillPointDialog | the whole modal: cursor read per pass, Blocking pump, OK / popup, arrows, refusal sound (backend) | 150 | 37 / 39 | 4 RED |
| showCharacterIntro | text, points, f_w_4c2e00[2], drawWindow effect, dialog, mask word | 150 | 2 / 2 | 2 RED |
| drawGolferInfoPanel | every write and peer call above; text reproduced | 400 | 70 / 73 | 5 RED |
| drawScrollText | layout pass, clamp, edges, entry count, pass 3's NUL / restore writes and right-column widths | 300 | 33 / 33 | 2 RED |
| runScrollText | the menu loop (Blocking pump, cursor or click point, keys, hotkeys), exit resets | 200 | 33 / 34 | 1 RED (+2 shared) |
| scrollTextWindow | position + runScrollText | 150 | (runScrollText's) | 1 RED (shared) |
| showBulletinBoard | photo / camera prefix, events in date order (appendCourseTitle, appendDate, g_year), Blocking wait in pumpInput483bd0, award text layout, record 156, skillPointDialog | 200 | 31 / 32 | 2 RED |

Unreached sides, each argued from the instructions: skillPointDialog 0x0045f398 taken (the repeated `lx < colX` inside
the block the first one guards), 0x0045f812 fall-through (used > points with points > 0: used only grows while below
points); drawGolferInfoPanel 0x0045d01d fall-through (the arm the hook source marks as never drawing), 0x0045da72 taken
(tab above 5 after the -1 test, ids are 0..4), 0x0045db27 taken (k starts at 4 or 9); runScrollText 0x0046e160 taken
(a letter key with the selection at 31: reachable, needs a 33+-line text and the cursor on line 31 in the same pass; not
generated); showBulletinBoard 0x0046f03f taken (a dispatch with no press and no key, U-0994).

Oracle method (port/test/test_dialogs.py, SG_EMU_ORACLE=1): cmdemu.py unchanged; the cursor reads are answered from a
SEQUENCE (CursorSeq: cmdemu's ("cursor", pt) stub reads pt[0] then pt[1], and [1] advances), the port gets the same
sequence through the test-only InputSource `dialogs.setScript` (QueueInput's batch rule, one point per read); the
number of cursor reads is compared too. Peers whose own oracle is elsewhere are scripted on both sides
(`dialogs.scriptPeer`, U-0990). showBulletinBoard's showScreen call is redirected (in the emulated image only) to
cmdemu's dispatch point, so its wait dispatches like the port's Blocking pump. Text widths: commands.setTextWidth on the
port, cmdemu's ("width", base, mask) on the original. Golden digests (default suite) pinned after the oracle was GREEN on
the very cases (SG_EMU_ORACLE=update). Every negative control RED against the digests and, with the oracle, against the
original. Equivalent mutants found and replaced: showCharacterIntro "result not stored" (skillPointDialog returns the
mask word it keeps up to date), runScrollText "Down past the last entry" (blind on the scripts).

## 3. Segments (log/p3/p37a_cmdsegs.py with the 38d DLL; all 155: log/p38/38d/segs_all.tsv)

117 PASS (109 before), 0 regressions. New PASS: cov_a_golfer/00..04, cov_a_keys2/04, cov_a_keys3/00 (drawGolferInfoPanel),
cov_b_holes18/01 (showBulletinBoard with the award dialog). Still stopped, with the new cause:
- cov_b_holes18/00: was record 137 (showBulletinBoard); now record 1140, tick's HeadlessTerrain arena (U-0995). With a
  16 MB arena (mirror-only experiment) 2130/2130 PASS.
- ui_tour/02, ui_tour_v/02: past showCharacterIntro's skill rows (20 / 22 dispatches delivered, was 12 / 14); without a
  provider they stop on Text_put in the dialog's showPopup (the player pressed OK with points left).
- cov_a_keys3/01: past the panel, stops on showCharacterEditor (U-0996).

With stream 38a's GdiTextMetrics (module gditext from the working tree linked into the 38d mirror, attached around
sg_run_frames by log/p38/38d/segrun_gdi.py, the effect of 38a's segments.py patch): ui_tour/02 PASS 296/296 and
ui_tour_v/02 PASS 302/302 (log/p38/38d/segs_gdi.tsv); cov_b_holes18/00 PASS 2130/2130 with the provider and the 16 MB
arena (segs_gdiexp.tsv); cov_a_keys3/01 still showCharacterEditor. All 155 with dialogs + gditext
(log/p38/38d/segs_all_gdi.tsv): 127 PASS, 28 stopped: in this stream's area only cov_a_keys3/01 (showCharacterEditor)
and cov_b_holes18/00 (tick's arena); the others on showCourseReport / showEndOfYearScreen (38c), shotStyleButtonCode /
loadGameBrowserScreen / courseActionMenu / saveCourse / saveGameDialog / promptAndSaveFile (38b, 38a), showHoleStatsScreen
(38e), and the four title-menu exits 38a documented.

Port suite (log/p38/38d/port_test2.log, committed test files + test_dialogs, DLL = HEAD + dialogs + gditext): all pass
except (a) test_events::test_run_bulletin_board_with_sound (merge item 6 has the fix, verified), (b) test_tick's slow
COMMAND gate: the working-tree OPEN_MISSING table lacks the names other streams port, and showCharacterEditor /
`headless Terrain arena` for this stream's two (add or resolve), (c) test_win32::test_runner_basics compares this x64 DLL
with the parent's Win32 build, which lacks the two modules (harness artifact). Oracle (SG_EMU_ORACLE=1): test_dialogs 34 /
34 on the final source (log/p38/38d/t11.log). No COMMAND segment reaches the scroll text layout
(the bulletin segments pass without any provider, so their award branch never runs): its evidence is the oracle only.

## 4. Open

- Merge (log/p38/38d_merge.md): modules.inc line; optional tick fallback cleanup; the arena size.
- After 38a's GdiTextMetrics is merged: re-run ui_tour/02, ui_tour_v/02 (popup inside the dialog), cov_b_holes18/00
  (award text layout), and check U-0993 with them.
- U-rows: log/p38/38d_uncertain.tsv (U-0990..U-0996).

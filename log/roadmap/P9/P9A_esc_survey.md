# P9A item 2: Esc on menus and full-screen screens, survey

Tree: `port/build/P9A` (base 2fd3553c). Original: `original\golf_clean.exe` (VAs). Every key-test address below was
disassembled with capstone (`keys.py` / `tab.py` in the session scratchpad: per function every call to takeKey
0x0045ae50 (hooks name setDirtyFlagB, reads gDirtyFlagB 0x00822b9c), takeCode 0x0045ae30 (setDirtyFlagA, reads gDirtyFlagA
0x00822b98), flushRedraw 0x0045ae70, pumpAndRedraw 0x0045c0c0, pumpMessages 0x0045c030, pumpLight 0x0045c150, every
`cmp ..., 0x1b` and every read of g_rootInput 0x00822d58 (+0x10 = [4] = kButton)). File:line refer to `port/src/sim/` of
the P9A tree.

## How a key reaches a loop (what the classification rests on)

- WM_KEYDOWN -> rootWin_onChar (commands.cpp:194): gDirtyFlagA = VK (0x1b for Esc). WM_CHAR -> rootWin_onKey
  (commands.cpp:186): gDirtyFlagB = the char (0x1b), gDirtyFlagA = the code.
- pumpMessages 0x0045c030 never touches either word (0x0045c030..0x0045c0b0: PeekMessage loop, the button-word reset at
  0x0045c094, doMouse 0x00483cd0). The port's pump_messages (commands.cpp:309) matches.
- flushRedraw 0x0045ae70 (port commands.cpp:338): returns 1 while a key word is set (and sets g_redrawPending), clears both
  words on the call after that. So a loop that calls flushRedraw ends (or reacts) on ANY key; a loop that never calls
  flushRedraw / takeKey / takeCode leaves the words set when it returns.
- pumpAndRedraw 0x0045c0c0 (port commands.cpp:353): a pending key ends the wait, like a click.

## Table

Classes: (a) Esc already leaves the screen the way its back / cancel / OK control does (often because any key does);
(b) Esc does something else; (c) Esc ignored and the screen has a back / cancel / done control: CANDIDATE; (d) no
back / cancel control or no input loop.

| # | Screen | Port function, input loop | Back / cancel / done control and its click | ORIGINAL on Esc (addresses) | Class |
|---|---|---|---|---|---|
| 1 | Title menu | Menu, tick_menu.inc:96 (hover_loop) .. :177 | Exit icon = hotspot 5 (P5A rect test tick_menu.inc:123); click -> remap jx 7 -> ebx = -2 (tick_menu.inc:206, ML+0x11db1) -> Session's QUIT path (top10.sve, promptAndSaveFile, closing movie, credits, tick_menu.inc:302) | No key read in the loop 0x00421025..0x0042127a (only g_rootInput[4] at 0x0042126a, ML+0x11caa); the loop is entered after pumpLight ML+0x11a37 | (c) CANDIDATE, but the control is "quit the game", not "back": caller's decision |
| 2 | selectDifficultyScreen | commands_screens.inc:574..600 | Back = hotspot 4 (approxDistance to (0x2ff, 0x22d) < 0x19); click -> sel = -1 (commands_screens.inc:601, 0x0043a86b `cmp ebp, 4` -> 0x0043a870 `or ebp, -1`) -> title menu again (tick_menu.inc:246..247) | Any key: flushRedraw 0x0043a856, no button 0x0043a863 -> 0x0043a870 sel = -1, the same exit as Back | (a) |
| 3 | showThemePackSelect | menu.cpp:843..890 | Cancel = hot 1 (to (0x2ed, 0x22c) < 0x18): exit_kind 1 -> g_theme = "Standard" (menu.cpp:895) | Any key with no button: flushRedraw 0x004730fe, button 0 -> 0x00473114 `mov ebp, 1` = Cancel | (a) |
| 4 | courseActionMenu (location / world map) | saveload.cpp:877..1127, pump :1065 | Back = hot 3 (to (0x300, 0x22d) < 0x19): result = -1, break (saveload.cpp:1094) = 0x00470634 `cmp eax, 3; je 0x0047089e`; exit flushInput 0x0047089e, button word 0 (0x004708aa); Session then returns to the title menu (tick_menu.inc:341, ML+0x5ef) | Ignored: the only input reads after pumpMessages 0x00470463 are the button word 0x00470468 (`je 0x0046f733` = next pass); no flushRedraw / takeKey / takeCode in the function (its nested "not enough cash" pumpAndRedraw 0x0047070e ends on any key) | (c) CANDIDATE |
| 5 | pickAProScreen | menu.cpp:680..718 | Back = hot 1 (to (0x301, 0x22c) < 0x18): action 1 -> flushInput, return 0, theme kept as set (menu.cpp:737) | Esc tested: flushRedraw 0x0043b221, no button, takeKey 0x0043b237, `cmp eax, 0x1b` 0x0043b23c -> 0x0043b241 `mov ebx, 1` = Back | (a) |
| 6 | Championship course list = loadGameBrowserScreen(1, 1) (from startNewCourse, menu.cpp:552) | saveload.cpp:678..738 | Cancel = hot 1: action 1, break, return 0 | Esc tested: takeKey 0x0043c5c6, `cmp eax, 0x1b` 0x0043c5cb -> 0x0043c5d4 `mov ebp, 1` = Cancel; another key -> 0x0043c7ab (a press at the last press position) | (a) |
| 7 | loadGameBrowserScreen (load browser: title Continue (1, 0), in-game 'L' (0, 0) tick_frame.inc:7568, courseActionMenu's Load) | saveload.cpp:678..738 | Cancel = hot 1 | Same code as row 6 (0x0043c5cb) | (a) |
| 8 | showCharacterEditor | chareditor.cpp:696..1108, pump :916 | Exit = control 19 (chareditor.cpp:1017; 0x00439d8f, done = 1 at 0x00439d9d, then the release wait 0x0043a235). Cancel = control 18 restores the record and STAYS OPEN (chareditor.cpp:1014; 0x00439d74 `rep movsd`, `jmp 0x0043a235`) | Ignored: after pumpMessages 0x004397b7 only the button word 0x004397c5 (`je 0x004386f4` = next pass) and the control switch 0x004397e3 (table 0x0043a39c) | (c) CANDIDATE (the leaving control is Exit) |
| 8a | pickCharEditor (the body / outfit picker, control 12) | chareditor.cpp:279..299 | No drawn control: a right button (0x004384f1 `cmp eax, 2; je 0x0043857d`) or a press above y 0x102 (0x004384fa, `jl 0x0043857d`) returns -1 = no change (chareditor.cpp:286..287) | Ignored: only the button word 0x004384e4 after pumpMessages 0x004384df | (c) CANDIDATE (cancel by press, not by a control) |
| 8b | Character editor's bio EditBox (control 20, bio_entry) | chareditor.cpp:564..602 | Ends on a press or Enter (gDirtyFlagB == 0x0d; original 0x00439f04 `cmp [0x822b9c], esi`) | Esc goes to the EditBox as a key (bio_command) | (d) not a screen with a cancel control |
| 9 | customizeCharacter -> showCharacterIntro -> skillPointDialog | terrain_setup.cpp:159; dialogs.cpp:345 | No loop of their own (customizeCharacter 0x0040f190 calls showCharacterIntro 0x004065c0 at 0x0040f1d5); the screen is the skill dialog, row 21 | see row 21 | see row 21 |
| 10 | showPreferencesMenu (System > Preferences) | commands_menus.inc:32 -> showPopup with flag4 = 0 (original 0x004325f3..0x00432600 pushes eax, eax = 0, 0) | Close button (within 0x14 of (x + w/2 - 0x14, y + h - 0x14)): returns the toggled mask (commands_popup.inc:197..200; 0x0046dcdf -> 0x0046dd7a, flag4 0 -> 0x0046dd86 `mov eax, [esp + 0x14]`); the caller stores it in g_preferences | Ignored: with flag4 = 0 the popup never calls flushRedraw / takeCode (0x0046db31 `test eax, eax; je 0x0046dbc7` skips the key block) | (c) CANDIDATE |
| 10a | Tournament par check list (buildTournamentPrep, showPopup flag4 = 0) | terrain_course.cpp:508 (original 0x0046d49e..0x0046d4ae: push 0, 0, 0xfffff) | Close button: returns the mask (which par changes to keep) into g_tournamentPrep | Ignored, as row 10 | (c) CANDIDATE |
| 11 | showSystemMenu and its popups (System, Information, the yes / no prompts) | commands_menus.inc:54; showPopup flag4 = 1 | Close button returns the recorded option | Esc tested in showPopup when flag4 != 0: flushRedraw 0x0046db39, takeCode 0x0046db42, `add eax, -0xd; cmp eax, 0x1b` 0x0046db4a, byte table 0x0046dda8 entry 14 = 1 -> jump table 0x0046dd94[1] = 0x0046dcfe: flushInput, return -1. System popup -1 -> default -> -1 (nothing); Information -1 -> return -1 | (a) for every flag4 = 1 popup |
| 12 | showPopup in general | commands_popup.inc:94..205 | close button (as rows 10 / 11) | flag4 != 0: Esc -1 (row 11). flag4 = 0: ignored (row 10). flag4 = 0 call sites in the port: commands_menus.inc:32 and terrain_course.cpp:508 only; every other site passes 1, or ebx = 1 (tick_frame.inc:7273, ML+0xcf8e, before the key switch; the land popup ML+0xed65 is in the phase whose comment at ML+0xeb44 reads showCourseReport(1) (push ebx)) | see 10 / 11 |
| 13 | textInputBox | commands_popup.inc:224..309, key read :269..274 | No cancel control (a click does not end it: the take after the loop gets ch 0) | Esc tested: takeKey 0x0045b51c, `cmp ebx, 0x1b` 0x0045b528 -> 0x0045b635: g_textBuffer[0] = 0 (0x0045b638), return | (a) Esc = cancel (empty text) |
| 13a | saveGameDialog ('S') / promptAndSaveFile | saveload.cpp:349 / :407 (no loop: textInputBox + showPopup) | - | Esc in its textInputBox empties the name -> the refused-name branch: startMessage of the invalid-name text (dirs + 0xC, saveload.cpp:374..377); Esc in the overwrite prompt -> showPopup -1 -> nonzero -> return without saving (saveload.cpp:388..389) | (b) Esc cancels the save but shows the invalid-name message |
| 13b | Other textInputBox users | courseActionMenu save (saveload.cpp:1081: empty -> continue), System rename (commands_menus.inc:101: empty -> refusal sound), editor name / title (chareditor.cpp:1036 / :1043: empty -> unchanged), editor comment line (chareditor.cpp:1080..1086: the box text is copied back unconditionally at :1086) | - | Esc empties the text; for the editor's comment line the port's transcription then writes the empty text into that comment line (chareditor.cpp:1086) | (a) except the comment line: (b) Esc erases the line |
| 14 | showCreditsScreen (QUIT path) | commands_screens.inc:448..480 | none (scroll) | Any key ends: flushRedraw 0x0044bd12 (commands_screens.inc:478) | (a) any key ends |
| 15 | showHireEmployeeDialog | commands_screens.inc:501..555 | OK button (0x229..0x251, 0x1a6..0x1cf): a click on it is a click on no row: pumpLight, sel -1 -> returns 2 (0x0045981a sets -1, 0x00459837 `lea eax, [eax + ecx + 1]` = 0 * 3 + 1 + 1); hireEmployee (tick_leaves.inc:238..240) takes k = 2, the `k % 3 == 2` row | Any key with no button: flushRedraw 0x004597e7, button 0 at 0x004597f4 -> 0x00459800 `or eax, -1`: returns -1 without pumpLight; hireEmployee returns at k < 0 (no hire) | (b) Esc closes WITHOUT hiring; the OK click does not (it returns 2) |
| 16 | F-key report screens: showBestScoresScreen, showHistographScreen, showFinancialReport, showPlayerCommentsReport | commands_screens.inc:73 / :176 / :244 / :330 -> click_or_key_loop :9 | OK button: any button ends (click_or_key_loop tests the button word, not the OK rectangle) | Any key ends: flushRedraw 0x00455eaf / 0x00456a0e / 0x0044fab3 / 0x00454c26, then pumpLight | (a) any key = the click |
| 17 | showMembershipRoster | routing.cpp:958..974 | Close = zone 0 (0x2dc..0x308, 0x224..0x250): a left press leaves | Any key ends the pass (flushRedraw 0x00455953), b = 0 and not `left` -> leave (routing.cpp:967); pumpLight 0x00455a03 | (a) |
| 18 | showShortcutsScreen | routing.cpp:722..742 | Close box (0x2c6..0x2f2, 0x16a..0x196), left press | Any key ends: flushRedraw 0x0044f61a (routing.cpp:740) | (a) |
| 19 | showRoutingMapScreen | routing.cpp:532..595 | Quit button = state -2 (routing.cpp:555) | Any key with no button: flushRedraw 0x00458306; clicked = false, then the hover state 9 / 10 scroll step runs, then `quit` (routing.cpp:572 / :590); the scroll step is routing.cpp:559; the same s.leave() as Quit | (a) (a key over an arrow also scrolls once first) |
| 20 | showCourseReport / showEndOfYearScreen | reports.cpp:677 / :866 -> report_loop :90 | OK / close button: any button (course report) or a left press (end of year) | Any key ends: flushRedraw 0x004531f2 / 0x0044e4ef | (a) |
| 21 | skillPointDialog (also from showCharacterIntro and showBulletinBoard) | dialogs.cpp:225..341, exit tests :298..311 | OK (within 0x14 of (xbase + 0x15e, 0x14e)) = done: every point used -> return the mask word (0x0045f644 `jle 0x0045f844`); points left -> the "not all points" showPopup, answer 0 -> return the mask (0x0045f64e..) | A key is consumed and only redraws: flushRedraw 0x0045f5df, then flushInput 0x0045f5f8, button 0 at 0x0045f5fd -> 0x0045f74c (the arrow / refusal tests, which need a button) | (c) CANDIDATE (the control is OK / done) |
| 22 | showBulletinBoard | dialogs.cpp:1099..1103 | none drawn: any click ends | Any key ends: flushRedraw 0x0046f038 (after doAllTasks 0x0046f02b) | (a) |
| 23 | showTournamentResults (scorecard layout) | tick_panels.inc:432..436 | Close button: any button ends | Any key ends: flushRedraw 0x0045adde (showTournamentResults+0xd4e) | (a) |
| 24 | showHoleStatsScreen | holestats.cpp:356..366 | OK: any button | Any key ends: flushRedraw 0x00454622 | (a) |
| 25 | drawScoreSummary (Top 10) | commands_menus.inc:219 | none: pumpAndRedraw | pumpAndRedraw 0x00473a20: any key ends | (a) |
| 26 | runScrollText / scrollTextWindow (file list of the editor's Load, messages) | dialogs.cpp:903..975 | none drawn; Enter / a click picks | Esc tested: takeCode 0x0046e10f, `lea ecx, [eax - 0xd]; cmp ecx, 0x1b` 0x0046e117, table 0x0046e244 entry 14 = 1 -> 0x0046e154 (sel -1, done) | (a) Esc = -1 |
| 27 | showBuyLandScreen | buyland.cpp:340..400, pump :354 | Done = hit -2 (0x296..0x2d6, 0x215..0x255): return 0 (buyland.cpp:376; 0x004592e7 `cmp eax, -2; je 0x004593e9`); a right click also returns 0 (0x004592da `cmp eax, 2; je 0x004593e9`) | Ignored: after pumpMessages 0x0045927a and waitTicks only the button word 0x00459288 (`jmp 0x004591d5` = next pass); no flushRedraw / takeKey in the function | (c) CANDIDATE |
| 28 | showLoadingScreen | commands_screens.inc:608 | none | no input | (d) |
| 29 | showGolferPairSelect (g_ui_569498 == 7, tick_frame.inc:8269) | not ported (no registered body in the tree) | - | flushRedraw 0x00459f1a: a key with no button reaches 0x00459f38, button 0 -> 0x00459fa8 (the tail a click on no row reaches); not analysed further | not in scope (not ported) |
| 30 | showMainRadialMenu | tick.cpp:623 (fallback) | - | a per-frame panel, no modal loop | (d) |

## Candidates: where the arm goes and what it must write

General form of each arm (`play` switch, default 1 = original): test the char word the original's own Esc tests read
(pickAProScreen and the browser read takeKey = gDirtyFlagB; showPopup and runScrollText read takeCode = gDirtyFlagA), and
only for 0x1b, so every other key keeps the original's behaviour in these loops (they never read the words):

```
if (SG_DEFECT(g_defect_<screen>_escIgnored) == 0 && gs.gDirtyFlagB == 0x1b) { take_key(ctx); take_code(ctx); <click effect>; }
```

Do not call flush_redraw there: it would change what the original leaves in the words for non-Esc keys. Point to check
when implementing: a key word left set from before the screen opened would fire the arm on its first pass; the title menu
(pumpLight ML+0x11a37) and courseActionMenu (called after the menu's pumpLight ML+0x11db6) start with the words cleared by
pumpLight's flushRedraw loop; for the other candidates the entry state of the words was not determined here.

1. courseActionMenu (row 4). Arm: saveload.cpp:1066, right after `cm::pump_messages(ctx, PumpKind::Blocking,
   "courseActionMenu")` (:1065) and before `if (btn == 0) continue;` (:1067). Effect = the Back click (saveload.cpp:1094..1097):
   `result = -1; break;` (the common exit then calls flushInput and clears the button word, saveload.cpp:1129..1130, =
   0x0047089e / 0x004708aa). Reproduces 0x00470634 `cmp eax, 3; je 0x0047089e`. Session then goes back to the title menu
   (ML+0x5ef).

2. showBuyLandScreen (row 27). Arm: buyland.cpp:355, right after `cm::pump_messages(ctx, PumpKind::Blocking,
   "showBuyLandScreen")` (:354), inside the inner loop. Effect = the Done click: `return 0;` (buyland.cpp:376; the right click's `return 0` is :373). Reproduces
   0x004592e7 `cmp eax, -2; je 0x004593e9`. The caller (tick_frame.inc:8343..8345, ML+0xed71) then skips g_landPurchaseCount++ and
   runs syncTiles as after Done.

3. showCharacterEditor (row 8). Arm: chareditor.cpp:917, right after `pump_pass(ctx, "showCharacterEditor")` (:916) and
   before `if (btn == 0) continue;` (:918). Effect = the Exit click: set `ctl = 19; btn = 1;` and fall into the press switch, so
   case 19 (chareditor.cpp:1017..1020: done = 1, the preview preferences into golfer +0x19 and custom bits 0..2 of R1 + 0x2c)
   and the release wait (:1104..1107) run as for the click. Reproduces 0x00439d8f..0x00439dba (done at 0x00439d9d), then
   0x0043a235. Alternative the caller may prefer (two controls, not one click): case 18 (Cancel, restore the entry copy,
   0x00439d74) followed by case 19.

4. pickCharEditor (row 8a). Arm: chareditor.cpp:284, right after `pump_pass(ctx, "pickCharEditor")` (:283) and before
   `if (button == 0) continue;` (:285). Effect = the right-click / press-above-0x102 path: `return -1;` (chareditor.cpp:286..287),
   = 0x0043857d. The caller's case 12 then breaks with no change (chareditor.cpp:970..971).

5. showPopup with flag4 = 0 (rows 10, 10a: Preferences, the tournament par check list). Arm: commands_popup.inc:146, right
   after `pump_messages(ctx, PumpKind::Blocking, "showPopup")` (:145), guarded by `flag4 == 0` (the flag4 != 0 branch keeps the
   original's own Esc). Effect = the close-button click: `return mask;` (commands_popup.inc:199..200 with flag4 0: result = mask).
   Reproduces 0x0046dcdf `jl 0x0046dd7a` -> 0x0046dd86 `mov eax, [esp + 0x14]` (mask) -> ret 0x0046dd91. showPreferencesMenu
   then stores it in g_preferences (commands_menus.inc:33), buildTournamentPrep in g_tournamentPrep (terrain_course.cpp:509).

6. skillPointDialog (row 21; also reached from showCharacterIntro and showBulletinBoard's award). Arm: dialogs.cpp:300, after
   the test at :299 (a key there already passed flushRedraw, which leaves the words set for this pass), as
   `if (button == 0 && take_key(ctx) == 0x1b)`. Effect = the OK click (dialogs.cpp:305..310): `if (points <= used) return mask
   word; else the "points left" showPopup, answer 0 -> return the mask word, else continue`. Reproduces 0x0045f644 `cmp esi,
   [esp + 0x10]; jle 0x0045f844` and the popup path from 0x0045f64e. Note: in that popup (flag4 = 1) a second Esc returns -1,
   which is nonzero, so the dialog stays open, as for the click (dialogs.cpp:309..310).

7. Title menu (row 1), only if the caller wants Esc = quit. Arm: tick_menu.inc:176 (tick_menu.inc was being edited by P9A item 3 during this survey: re-find the line by its text), before `if (G32((GS(g_rootInput) + 16)) ==
   0) goto hover_loop;`. Effect = a click on the exit icon: `ebx = 5;` and continue at the click branch (tick_menu.inc:179 ..),
   whose remap turns 5 (jx 7) into -2 (ML+0x11db1); then pumpLight ML+0x11db6 and Session's QUIT path. The exit control quits the
   game (top10.sve, Default.pro, the closing movie, the credits); it is not a "back".

## Not candidates, with the reason

- Esc already acts as the screen's back / cancel / OK (class a): rows 2, 3, 5, 6, 7, 11, 13, 14, 16..20, 22..26.
- Esc does something other than the control's click (class b), left as the original does:
  - showHireEmployeeDialog (row 15): Esc closes without hiring (-1, 0x00459800), while the OK click returns 2 (0x00459837) and
    hireEmployee then treats it as row k = 2. Esc is the only no-hire way out; an "Esc as the OK click" arm would make Esc hire.
  - saveGameDialog (row 13a): Esc cancels and shows the invalid-name message.
  - the character editor's comment-line box (row 13b): Esc empties the comment line.
- No control (class d): rows 8b, 28, 30. Not ported: row 29.

# S11.1 merge notes: the 5 C3 rows (nothing here is applied; every edit is to a shared file or a tracker)

Order: section 1 (strcpy copy step), section 2 (PolyFill MATCH lines), section 3 (U-1351), then the four promote commands of
section 4, then the hold line of section 5. Every promote command was dry-run with `re_classify.py check` (read-only) on the tree
of 2026-11-01: log/roadmap/S11/1_check_before.log (before) and 1_check_after.log (with the section 1 and 3 edits applied in a
scratch copy: see 1_notes.md section 6).

Note: `log/diff/*.match.csv` and `log/*` are gitignored (.gitignore lines 22 / 25): the match CSVs live on disk only, so the
copy step of section 1 and the match.py run of section 2 must have happened on the machine that runs the promotes; the S11 files
need `git add -f` as earlier sections' logs had.

## 1. sound.dll strcpy 0x1004a010: library-object identity CSV into log/diff

```
cp log/roadmap/S11/1_strcpy_lib.csv log/diff/0004a010_sound__strcpy.match.csv
```

The CSV is S11.1's own comparison (log/roadmap/S11/1_strcpy_lib.py; reproduce with `~/bin/py -3.12 log/roadmap/S11/1_strcpy_lib.py`):
VC6 SP5 LIBCMT.LIB member strcat.obj, symbol `_strcpy` at +0x0 of its .text, 0 relocations in the section from `_strcpy` to its end
(0xf0 bytes), all 15 16-byte rows equal to sound.dll RVA 0x4a010..0x4a0ff (VERDICT GREEN 15 100.0%); strcpy's own 7 bytes
`57 8b 7c 24 08 eb 6a` (our extent, re/match/boundaries.tsv unchanged) equal, and its `jmp 0x1004a081` (0x1004a015) lands inside the
compared span, which holds the copy loop strcpy shares with strcat 0x1004a020; no branch of the span leaves it. It equals S10.7's
log/roadmap/S10/7_lib/sound/sp5_cmt/0004a010_sound__strcpy.match.csv byte for byte in its 15 rows. Why it was dropped at I16 and why
that no longer applies: S10 MERGE.md section 7 dropped it because 7_boundaries.tsv proposed a 240-byte extent (which broke the naked
m36c line) and because byte identity waited on D4. No extent change is proposed here (the CSV states the span it compares), and D4
(byte identity counts as library-object identity) was accepted at I16. With 0 relocations in the span, U-1881's masking concern does
not arise for this row (nothing is masked).

## 2. re/match/smsg_golf_jackal_PolyFill.cpp: two MATCH lines

Replace the two lines

```
// ALSO 100%, already in golf_hand_03.cpp: golf_clean.exe 0x00493000 ?scan_out_line_poly_16@@YAXHH@Z
// ALSO 100%, already in golf_hand_03.cpp: golf_clean.exe 0x00493080 ?scan_out_line_poly_8@@YAXHH@Z
```

with

```
// MATCH: golf_clean.exe 0x00493000 ?scan_out_line_poly_16@@YAXHH@Z   // scan_out_line_poly_16 = fillSpanDown (compiled C, inline __asm body)
// MATCH: golf_clean.exe 0x00493080 ?scan_out_line_poly_8@@YAXHH@Z   // scan_out_line_poly_8 = fillSpanUp (compiled C, inline __asm body)
```

then `~/bin/py -3.12 re/tools/match.py re/match/smsg_golf_jackal_PolyFill.cpp` (expected: setup_edge_poly 100%, 0x00493000 100.0%
41/41, 0x00493080 100.0% 40/40; it rewrites the two CSVs below, identical to the ones S11.1 wrote) and
`~/bin/py -3.12 -m pytest tests/test_match.py -q -k PolyFill`. golf_hand_03.cpp's naked lines stay (log/c3/c4_match_promote.py skips
them). The evidence CSVs are already in log/diff (written by `match.py log/roadmap/S11/1_polyfill_match.cpp`, which compiles the same
unmodified SMSG PolyFill.cpp with the same header; both GREEN, 100%):

- log/diff/00493000__scan_out_line_poly_16__YAXHH_Z.match.csv
- log/diff/00493080__scan_out_line_poly_8__YAXHH_Z.match.csv

Rule applied (re/C4_PLAN.md, round 36 evidence decisions): SMSG's scan_out_line_poly_16 / _8 are ordinary compiled C functions
(`void f(int xLeft, int xRight) { __asm { ... } }`, not `__declspec(naked)`), so the frame is VC6's: `push ebp; mov ebp, esp; push ebx;
push edi` and `pop edi; pop ebx; pop ebp; ret` around the asm block are compiled and match (8 of 41 / 8 of 40 instructions). The
original holds TWO saves of edi (the compiler's push at 0x00493004 / 0x00493084 and the asm block's own push right after it), which a
C body without inline asm never produces; that is the inline-asm-in-a-C-body case the user ruled IS match evidence. Difference from
the 85 rows already C4 that way: here the asm block is the whole body (U-2000 asks the lead to confirm; if the lead rules an all-asm
body out, drop section 2 and section 4's two fillSpan lines, delete the two CSVs above, and add the fillSpan hold lines of section 5b).

## 3. UNCERTAINTIES.md U-1351 (blocks gameMain 0x0045baf0)

U-1351's addr cell is 0x0045baf0 (gameMain mallocs and fills g_halfBrightLut 0x00822c74), but its subject is the shade table that
callers of Sprite::drawG's three exe wrappers pass, not gameMain's code, which the 100% match (320/320, SMSG GameMain.cpp, no inline
asm) fixes completely. Its resolution column asks for `the call sites' table recorded per drop`; S11.1 recorded it from the binary
(log/roadmap/S11/1_shade_sites.py -> 1_shade_sites.log): of the 18 `call` sites of Surface_blit2 0x004740f0, Surface_drawScaled2
0x00474030 and drawScaledSprite421b90 0x00421b90, 16 push `[0x00822c74]` (g_halfBrightLut) as the table argument a4 (mainLoop
0x004167e1, 0x00416d81, 0x004170b3, 0x004170ef, 0x00417111, 0x00417166, 0x004171a7, 0x004171e5, 0x00418ee9, 0x004194ca, 0x004195be,
0x00419683; drawGolferInfoPanel 0x0045ce0b; drawQueuedSprite 0x0046306b), the two in blitPanel 0x0040ca10 push `[0x00824148]`
(g_color16Lut, loads 0x0040ca52 / 0x0040caa1), and the two inner calls (0x00421bb4, 0x004740ae) pass their own a4 on. Every site
chooses a constant table; the port's drop sites pass g_halfBrightLut at exactly the 14 + 1 + 1 non-blitPanel sites (tick_frame.inc
ML+0x724f, +0x77ef, +0x7b09..+0x7c3d, +0x9947, +0x9f28, +0xa01c, +0xa0e1; dialogs.cpp drawGolferInfoPanel; gfx_scene.cpp
drawQueuedSprite), and blitPanel is gfx_prim_fill.cpp's own body over g_color16Lut.

Edit (recommended, A): in U-1351's row set the status cell `open` -> `resolved` and append to its description:
` Resolved S11.1 (2026-11-01): the original's 18 call sites each pass a constant table: 16 [g_halfBrightLut], blitPanel's two
[g_color16Lut] (log/roadmap/S11/1_shade_sites.log); the port's drop sites match them site for site. The blind control (block 1 always)
remains a limit of the opaque-pixel frame gate, not a question about the original.`

Alternative (B), if the lead keeps the blind-control part open: set U-1351's addr cell to empty (the row is about the drawG
callers, not gameMain) and leave it `open`. Either edit frees gameMain; tests/test_classify.py::test_real_tracker_state accepts an
empty addr cell (CLAUDE.md, I16).

## 4. Promote commands (after sections 1-3)

```
~/bin/py -3.12 scripts/re_classify.py promote 0x0045baf0 --to C4 --evidence log/diff/0045baf0__WinMain_16.match.csv
~/bin/py -3.12 scripts/re_classify.py promote 0x00493000 --to C4 --evidence log/diff/00493000__scan_out_line_poly_16__YAXHH_Z.match.csv
~/bin/py -3.12 scripts/re_classify.py promote 0x00493080 --to C4 --evidence log/diff/00493080__scan_out_line_poly_8__YAXHH_Z.match.csv
~/bin/py -3.12 scripts/re_classify.py promote 0x0004a010 --to C4 --module sound.dll --evidence log/diff/0004a010_sound__strcpy.match.csv
```

- gameMain: SMSG GameMain.cpp `_WinMain@16`, cl 8168, /O2, 100.0% (320/320), re-scored 2026-11-01 (1_gamemain_noev.log); the CSV
  in log/diff (I16) is GREEN 320. Its only refused gate line was U-1351.
- After the four: C4 2,928 -> 2,932 (exe 4 -> 1 C3 left, sound 1 -> 0). Then `~/bin/py -3.12 -m pytest tests/test_changelog_integrity.py -q`
  and `~/bin/py -3.12 log/c3/hook_integrity.py`.

## 5. Hold line (re/c3_hold.tsv; module addr reason)

drawAmenitiesPanel stays C3. A C3 row in c3_hold.tsv changes no count (re_classify counts held rows among C2 only; c3_verify only
skips it):

```
golf_clean.exe	00434cf0	drawAmenitiesPanel held at C3 (no C4 evidence, S11.1 2026-11-01): SMSG Toolbar.cpp RenderBuildingToolbar reads 99.9% (681/682) under cl 8168, one reload pair after the second tooltip call 0x00432620 in the other order (original mov esi, [esp+0x28] before mov edi, [esp+0x24]); 100% only under SP5's front end, which the exe was not built with (U-1910); 145 permuter compiles (S10.12) and 10 hand variants (log/roadmap/S11/1_tb_variants.log) leave it; UNREACHED in all 7 canonical scenarios (log/c3/s36a_results.tsv, s36b_results.tsv). Route: scenario_ab.py ON/OFF over a pinned scenario that shows the amenities palette (cov_a_build, cov_k3_build, cov_k3_camera: log/roadmap/S3/6_notes.md) (U-2001)
```

### 5b. Only if the lead rules section 2 out (U-2000)

```
golf_clean.exe	00493000	fillSpanDown held at C3: only matches are golf_hand_03.cpp's naked transcription and SMSG PolyFill.cpp's all-asm body inside a compiled frame (100%, 41/41), which the lead ruled out (U-2000); no scenario CSV
golf_clean.exe	00493080	fillSpanUp held at C3: only matches are golf_hand_03.cpp's naked transcription and SMSG PolyFill.cpp's all-asm body inside a compiled frame (100%, 40/40), which the lead ruled out (U-2000); no scenario CSV
```

## 6. Lead decision on U-2000 (S11 lead, 2026-11-01)

The lead does not extend the round-36 inline-asm rule to an all-asm body: that is an evidence-rule change, Mariano's to make.
Until he rules, sections 2 and 4's two fillSpan lines are NOT applied and section 5b's two hold lines ARE applied. The two GREEN
CSVs were moved out of log/diff (a blanket match pass would otherwise promote them) to
log/roadmap/S11/1_ev/00493000__scan_out_line_poly_16__YAXHH_Z.match.csv and .../00493080__scan_out_line_poly_8__YAXHH_Z.match.csv.
If Mariano accepts an all-asm body inside a compiled C function: copy both back into log/diff, apply section 2, run the two
fillSpan promote lines of section 4, and remove the two 5b hold lines.

# S11.1: the 5 C3 rows to C4 by named evidence, or held (2026-11-01)

Result: 4 promote commands with named GREEN own-module evidence that is not a naked transcription (gameMain, fillSpanDown,
fillSpanUp, sound strcpy), 1 hold (drawAmenitiesPanel). Everything to apply is in 1_merge.md; no tracker, hooks.csv, CHANGELOG,
c3_hold.tsv, UNCERTAINTIES.md or shim file was edited. Files written outside log/roadmap/S11: the two 100% match CSVs in log/diff
(section 2).

## 1. Per row

| row | evidence | how | result |
|---|---|---|---|
| gameMain 0x0045baf0 | SMSG GameMain.cpp `_WinMain@16`, cl 8168 /O2, 100.0% 320/320 (log/diff/0045baf0__WinMain_16.match.csv, I16) | the only refused gate line was U-1351 | promote after U-1351 is resolved (merge section 3) |
| fillSpanDown 0x00493000 | SMSG PolyFill.cpp scan_out_line_poly_16, compiled C + inline __asm body, 100.0% 41/41 | new CSV via match.py | promote (merge 2, 4); lead to confirm the all-asm limit case (U-2000) |
| fillSpanUp 0x00493080 | SMSG PolyFill.cpp scan_out_line_poly_8, 100.0% 40/40 | same | same |
| sound strcpy 0x1004a010 | SP5 LIBCMT.LIB strcat.obj `_strcpy`, byte identity over 0xf0 bytes, 0 relocations | 1_strcpy_lib.py | promote after the copy step (merge 1, 4) |
| drawAmenitiesPanel 0x00434cf0 | none: 99.9% under 8168, 100% only under SP5 (U-1910), UNREACHED in the canonical scenarios | 10 hand variants | hold (merge 5, U-2001) |

## 2. fillSpanDown / fillSpanUp
- SMSG's PolyFill.cpp (third_party/smsg/SMSG-decomp/golf/jackal.lib/src/PolyFill.cpp) holds `void scan_out_line_poly_16(int, int)` /
  `_8` as ordinary functions whose body is one `__asm {}` block, not `__declspec(naked)`. The S10 importer recorded them only as
  `// ALSO 100%, already in golf_hand_03.cpp` (the naked transcription), so no non-naked CSV existed.
- log/roadmap/S11/1_polyfill_match.cpp = smsg_golf_jackal_PolyFill.cpp's header (FLAGS, INCLUDEs, COMPILE of the unmodified file)
  + the two MATCH lines. `match.py <it> --no-evidence --show` (1_polyfill_noev.log): every instruction equal. `match.py <it>`
  (1_polyfill_ev.log) wrote log/diff/00493000__scan_out_line_poly_16__YAXHH_Z.match.csv (GREEN 41) and
  log/diff/00493080__scan_out_line_poly_8__YAXHH_Z.match.csv (GREEN 40). Both 100%: no RED CSV was written.
- Compiled part: prologue `push ebp; mov ebp, esp; push ebx; push edi` (VC6 saves the callee-saved registers the asm uses and
  builds an ebp frame for the asm's parameter references) and epilogue `pop edi; pop ebx; pop ebp; ret`; the original's double
  save of edi (compiler, then the asm block's `push edi`) is the trace of inline asm in a C body. U-2000 for the lead.

## 3. sound strcpy
- S10.7 had the same comparison (7_lib/.../0004a010_sound__strcpy.match.csv); I16 dropped it only for the 240-byte boundaries
  proposal (which would break the naked line) and the then-open D4. 1_strcpy_lib.py is an independent re-measurement: own
  archive / COFF reader, `_strcpy` at +0x0 of strcat.obj's .text, the span to the section end 0xf0 bytes, 0 relocations, 15 / 15
  rows equal, the 7-byte entry equal, the entry's `jmp 0x1004a081` inside the span, no branch leaving it (1_strcpy_lib.log). No
  extent change. Output CSV 1_strcpy_lib.csv equals S10.7's rows.

## 4. gameMain and U-1351
- re_classify's C3/C4 gate refuses a row with an open semantic / structural uncertainty whose addr cell is the row's address
  (scripts/re_classify.py open_blocking_uncertainties). U-1351 (S3.6) sits at 0x0045baf0 because gameMain fills g_halfBrightLut,
  but it asks which shade table the drawG wrappers' callers pass. 1_shade_sites.py (capstone over golf_clean.exe .text) lists the
  18 call sites: 16 load [0x00822c74], blitPanel's two load [0x00824148], two pass their own a4. The port's sites agree (grep of
  port/src/sim: tick_frame.inc, dialogs.cpp, gfx_scene.cpp; gfx_prim_fill.cpp for blitPanel). So the resolution column's second
  option is met; merge section 3 gives the edit (A: resolved; B: empty addr cell).
- GameMain.cpp has no `__asm` / naked (grep count 0); re-scored 100.0% 320/320 (1_gamemain_noev.log).

## 5. drawAmenitiesPanel
- Diff (S10.12's show log): after `call 0x00432620` (the landmark branch's second RenderToolbarTooltip(x, y - 2)) the original
  reloads `mov esi, [esp+0x28]` then `mov edi, [esp+0x24]`; the compile the reverse.
- 1_tb_variants.py (each variant edits a copy of Toolbar.cpp inside RenderBuildingToolbar only, scored with --no-evidence, 1_tb/
  removed afterwards; rerun recreates it): base, tt2_plus, case_m2_swap (99.6), tt1_tt2_vars, decl_order, decl_split,
  id_local_landmark, switch_default (95.0), freelm_ne0, hover_y_first, y_copy: none reaches 100, all others 99.9 with the same pair.
- Scenario route: UNREACHED in all 7 canonical scenarios (log/c3/s36a_results.tsv, s36b_results.tsv); the port's frame work shows
  the palette in cov_a_build / cov_k3_build / cov_k3_camera (log/roadmap/S3/6_notes.md), so scenario_ab.py over one of them is the
  next step (needs a game slot).

## 6. Reproduce
```
~/bin/py -3.12 scripts/re_classify.py check 0x0045baf0 --to C4        # before: refused by U-1351 only (1_check_before.log)
~/bin/py -3.12 re/tools/match.py log/roadmap/S11/1_polyfill_match.cpp  # 100% x2, writes the two CSVs
~/bin/py -3.12 log/roadmap/S11/1_strcpy_lib.py                         # GREEN 15/15, writes 1_strcpy_lib.csv
~/bin/py -3.12 log/roadmap/S11/1_shade_sites.py                        # 18 sites (1_shade_sites.log)
~/bin/py -3.12 log/roadmap/S11/1_tb_variants.py                        # 11 variants (1_tb_variants.log)
~/bin/py -3.12 log/roadmap/S11/1_check_after.py <scratch dir>          # all four pass with U-1351 resolved (1_check_after.log)
```
1_check_after.py runs re_classify's check (write=False) with Project.uncert pointed at a scratch copy of UNCERTAINTIES.md where
U-1351 reads resolved; strcpy is checked with the lib CSV as --evidence (the gate's own CSV scan reads log/diff, where the copy
step puts it next to the naked CSV, both GREEN).

## 7. Controls
- strcpy: the comparison is GREEN only when every byte, the entry, the relocation count (0) and the branch targets agree; a flipped
  span byte reads RED (verified by construction of the check: `same == n` gate). Not run as a separate mutant.
- Matches: match.py's own instruction comparison; the variant runs show it moves (case_m2_swap 99.6, switch_default 95.0).
- U-1351 sites: the script tags each site by the operand it loads; blitPanel's two read g_color16Lut, so the scan distinguishes the
  tables (not all sites read the same constant).

## 8. Open
- U-2000 (lead: all-asm body under the inline-asm rule), U-2001 (drawAmenitiesPanel: scenario A/B or an 8168 spelling).

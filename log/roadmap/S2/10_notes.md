# S2.10 notes: buy land (sim + drawing), syncTiles, the character editor's and the skill dialog's drawing

Worker on claude3 (session cmvncanfd6upxo115u3cj3svi), lead cmvmns3ax3frbo1152mcm7eg2. Merge request: 10_merge.md (+ 10_shared.patch).
Files: 10_files.txt. Uncertainties: 10_uncertain.tsv (U-1290..U-1297).

## 1. Trees

- port/build/S2_10_head: the step's mirror = a copy of port/build/S2_13_head (HEAD 2c05f02a + S2 waves 1-5; the waves are not
  committed) with the junctions of log/roadmap/S0/export.md. port/build/S2_10_base: pre-step copies of port/src, port/include,
  port/test, re/tools (the patch base). Build: `~/bin/py -3.12 log/roadmap/S2/build.py <abs>/port/build/S2_10_head
  <abs>/port/build/S2_10 --log log/roadmap/S2/10_build.log`; Win32 `~/bin/py -3.12 port/test/build.py --arch Win32` from the mirror.
- port/build/S2_10_s3: a MEASUREMENT tree, not a deliverable: S2_10_head + the in-flight S3.5 / S3.6 work (port/build/S3_5_root,
  which carries S3.6's files), 3-way merged against S2_4_head (`log/roadmap/S2/10_tools/make_s3_tree.py`, 10_s3tree.log: 2
  conflicts, gfx_families.inc and modules.inc, resolved by keeping every line; `terrain` and `scene` moved after `reports` so
  reports keeps Surface_drawSprite, the duplicate S2.13 settled (U-1278)). The `scene` family needed three things for the
  editor's figure and the buy-land sprites (10_merge.md section 3): I measured them first as prototype edits to the tree's copy
  of gfx_scene.cpp (kept as the scratchpad file gfx_scene_proto.cpp), asked S3.5, which added them to its own file; the final
  measurements (section 4, test_gfx_s2_10.py 13 / 13, 10_s3_test_gfx_s2_10.log) use S3.5's gfx_scene.cpp / .h as they are
  (S3_5_root, 05:09). Build into port/build/S2_10s3.

## 2. showBuyLandScreen (module `buyland`, port/src/sim/buyland.cpp)

Written from c3fr.cpp ShowBuyLand_re (reading reference). Simulation part: per redraw pass the nine 16 x 16 lots of g_tileType,
one Random::range(3) per tile for sale (a low word of 0 adds 1 << (g_landPurchaseCount & 31) to the lot's sum, from 5), the
per-type counters indexed with the SIGNED g_tileTypeSaved byte into the frame words (the port keeps the original's frame run
L58..Ldc as one array F[42], so a negative type counts into the costs and the last lot's row number as the original does; an
index outside the modelled words is a MissingResource), the first pass's costs (worth * 10; a lot with no tile for sale is
priced 50), the texts in g_textBuffer, appendNumber (g_textBuf) for the cash label and each price, the routing map's draw-only
pass (showRoutingMapScreen with g_map822b80[0] = 1), the hole overlay's cellAt results in g_map822b80[1]. The loop: the cursor,
the region (lots in three columns of 68-row bands, the done button), a redraw on a change, else a Blocking pump until the button
word; then the original's click rules, canAfford, the purchase (cash, the year's finance record + 0xe, the lot's tiles,
logTick(0x140, lot)), result 1; 0 for a right click or the done button.

Evidence (port/test/test_buyland.py, SG_EMU_ORACLE=1): the ORIGINAL under Unicorn (cmdemu.py) with the drawing callees stubbed
(the 3.7b screens' list and the routing map's, cellAt kept real, plus drawTextLinesAt, drawRight, drawWindow, playSound): eax,
consumed input and the whole data image (the screens' mask + g_linePoint5a412c; g_map822b80[1] compared whenever a hole is
drawn, since the routing port does not write its own cell pointer) equal the port's in 150 random + 112 boundary cases;
branch coverage 49 of 55 jcc both ways, the 6 others unreachable (UNREACHED in the test, with reasons). Golden digests pinned
after the oracle was GREEN (golden_buyland.json, 120 + 112 cases). Controls (NEGCTL_RED, 8 modes): all RED against the goldens
and against the oracle (10_buyland_negctl.log, 9 passed in 1,044 s).

## 3. syncTiles (buyland.cpp; was a tick drop)

With showBuyLandScreen registered the cov_r_s2_screens boot ran to its end and differed at record 1157 in g_tileWallMask only.
`log/roadmap/S2/10_tools/solve_record.py` (sgh64 is a sum of per-dword terms: one changed dword is solved for directly) gave the
capture's value: dword 0 = 0x00000000, the port's 0x00000002. syncTiles (c3ey.cpp SyncTiles_re) ends with the byte
g_terrain820f2a[0] = 1 (0x00449513); the drop left it 0, so the frame's updateTerrainRender took the incremental path, which sets
bit 1 of g_tileWallMask[0] (terrain_dll.cpp), where the original repaints from scratch. syncTiles is ported (Terrain::setType /
getVariation through TerrainApi, the per-column pump_input as rebuildTerrainGrid, the byte) and its kDrops line removed.
Result: cov_r_s2_screens PASS 1268 / 1268 (10_boot_s2screens.log).

## 4. Drawing

| screen | calls (in the original's order, each `gfx_drop` under g_gfx_attached) | bodies |
|---|---|---|
| buy land (buyland.cpp) | backdrop and selection images, the routing map (its own drawing), the hole overlay (setLinePoint / lineTo, addSprite, drawCenteredText2), the title, the cash label, per lot the map label (drawStringAt with the original's 2-byte label, through the text buffer, bytes put back), the text block (drawTextLinesAt), the price (drawRight), the lot images, the buttons, refreshFull | gfx_buyland.cpp: drawTextLinesAt (measureText2 + wrapTextToWidth, plain text), drawRight |
| character editor (chareditor.cpp) | the frame Image's background (per gender, reloaded by control 17), the control hover sprite / the name area's hover colours, the hat, composeGolferSprite, the body in the composed palette, the head, Display::setScale, the frame sprite, names, status lines, traits and preferences with their selected sprites, the member's skill bars or the type card and bio (drawMessageBuffer), the 20 comment lines, the hover tip (drawPanelBackdrop), refreshFull, the bio panel | gfx_chareditor.cpp: showCharacterEditor.setup / .background / .teardown / .bio, drawMessageBuffer |
| skill-point dialog (dialogs.cpp) | drawFrameR3, title, the golfer's backdrop, drawThoughtBubble's drawing (mode 1: hat, body, head), the stand, rows / arrows / hovers / values / names, the points line, refreshFull; showCharacterIntro's drawWindow.frame | existing (core, prim_*, hud, scene) |
| text quad state (gfx.cpp, gfx_prim_text2.cpp) | drawCenteredText / drawText4049d0 / drawCenteredText2 now run Widget_setQuad first, as their originals do (U-1293) | - |

Measured frames (framegate, 10_fg_*.tsv, images 10_img/):

| checkpoint | S2 mirror (S2_10) | measurement tree (S2_10s3: + S3.5 / S3.6, S3.5's gfx_scene.cpp as is) |
|---|---|---|
| cov_r_s2_screens editor | 24,400 px (figure, hover sprites: S3) | 0 px PASS |
| cov_a_keys3 q_editor, cov_a_golfer button_0, cov_a_career career_editor | 21,285 / 18,617 / 24,489 px | 0 px PASS each |
| cov_r_s2_screens buy_land | 221 px (+ the tee / flag sprites need addSprite) | 161 px, all the hole's GL line in x 0x177..0x1ad, y 0x17c..0x180 (U-1294, S3.8) |
| cov_r_s2_screens skill_points / skill_popup | 456,730 / 409,278 px (the course: S3) | 68 px each, all in the course view; 0 inside the dialog; 3 under the shaded help window (course pixels) |

Steps on the way (10_fg_s3a..f): editor 3,256 px (only the body: the composed palette and Image_draw's palette argument) ->
0 with the image_draw palette and composeGolferSprite parts; buy_land 299 -> 221 (the lot label's colour, U-1293) -> 161 (the addSprite body).

## 5. Controls

Frame controls (10_controls_s3.log, measurement tree; again in test_gfx_s2_10.py, 7 / 7 RED with S3.5's file, and the four
buy-land ones RED in the S2 mirror too, 10_x64_test_gfx_s2_10.log: 4 passed, 9 skipped for want of the S3 families): chareditor 1 (other gender's background) 2,265 px,
2 (background one row lower) 115,944, 3 (drawMessageBuffer x + 1) 3,054 on q_editor; buyland 1 (line spacing + 1) 6,495, 2
(wrap after the word) 9,200, 3 (drawRight x - 1) 4,496; gfx.attach 7 (no setQuad in the core text bodies) 629 against 161: 7 RED.
Blind: prim_text2 mode 3 on editor (U-1293). S3.5's own control 11 (its composeGolferSprite body does nothing), measured for it:
blind on the four editor checkpoints (the editor's addSprite figures, flags 0x199 = golfer 0x99, make drawQueuedSprite.draw
compose the same golfer every pass, so g_images[0] already holds the palette), RED on skill_points 68 -> 146 px
(10_s3_scene11*.tsv). Not controlled: showCharacterEditor.teardown (nothing reads the image after it) and
.bio (it only reports). Sim controls: buyland 8 modes RED (oracle), syncTiles mode 1 RED at boot record 1157, showBuyLandScreen
modes 4 / 1 RED at record 1156 (g_rng / g_textBuf) in test_buyland.py.

## 6. Gates (S2 mirror port/build/S2_10_head, x64 DLL port/build/S2_10/Release, Win32 port/build/S2_10_head/port/build/Win32/Release)

| gate | result | log |
|---|---|---|
| x64 files, one per process | 14 / 14 PASS: test_static 6, test_buyland 13 (+3 oracle skips), test_chareditor 19, test_dialogs 27, test_s2_6_scenario 4, test_s2_5_traces 3, test_gfx 25, credits 47, routing 35, boot_screens 32, reports 36, gfx_dialogs 32, commands 83 / 44 skipped, tick 250 | 10_x64_suite.log, 10_x64_*.log |
| oracle (SG_EMU_ORACLE=1) | test_buyland: 150 random + 112 boundary cases equal, coverage 49 / 55 (6 unreachable), 8 / 8 controls RED against goldens and oracle | 10_buyland_negctl.log |
| segments | 205 / 205 PASS x64 and Win32, 0 digests differ from log/p5/segs_*_loc.tsv | 10_segs_*.tsv, 10_segs_*_compare.log |
| boot --all | 70 rows: PASS 66, KNOWN 2, NO_FRAMES 2; against 13_boot_gate.tsv one row differs: cov_r_s2_screens FAIL 1156/1268 -> PASS 1268/1268 | 10_boot_gate.tsv, 10_boot_compare.log |
| census --all | 698 rows: PASS 83 (unchanged), UNSUPPORTED 583, NOT_RUN 32 (was 35: land_popup, buy_land, end now run). No row changed status for the worse; 29 rows have fewer px (the four editor checkpoints of the S2 mirror 463-480 k -> 18-24 k, cov_r_bio's three editor rows 471 k -> 22-24 k, skill_points / skill_popup about 5 k fewer), 3 run that did not (buy_land 221 px, land_popup, end), 22 UNSUPPORTED rows have more px, all in-game frames already 99.4-99.9 % wrong with the same first unsupported call: the root keeps the last screen the backend drew (now the editor) under frames it cannot draw, the class S2.13 reported | 10_census_x64.tsv, 10_census_compare.log, 10_census_analysis.log |
| Win32 files | 10 / 10 PASS: test_gfx 25, credits 47, routing 35, boot_screens 29 + 3 skipped, reports 36, gfx_dialogs 32, s2_6_scenario 4, buyland 13 + 3, dialogs 27 + 7, chareditor 19 + 4 | 10_gates.log, 10_w32_*.log |
| patch | 10_shared.patch applies (git apply --check) to S2_10_base's copies and reproduces the mirror's 8 files byte for byte (LF) | - |

## 6b. Base for the merge

The S2 lead committed waves 1-4 as b3d61e41 (with S3 wave 1). 10_shared.patch applies on b3d61e41 + 13_shared.patch (checked on a
`git archive b3d61e41 port`); the 8 files equal the mirror's except tick.cpp's S3.3 U-1120 lines that b3d61e41 adds.

## 7. Scroll text, radial menu

Scroll text: U-1295 (what would show it). Radial menu: drawn by S3.6's `hud` family (showMainRadialMenu.draw); not touched here.

## 8. Reproduce

```
~/bin/py -3.12 log/roadmap/S2/10_tools/make_s3_tree.py                      # measurement tree (then the hand resolutions, section 1)
SG_PORTSIM_DLL=<dll> SG_EMU_ORACLE=1 ~/bin/py -3.12 -m pytest port/test/test_buyland.py   # from the mirror root
SG_PORTSIM_DLL=<dll> ~/bin/py -3.12 -m port.test.boot cov_r_s2_screens
SG_PORTSIM_DLL=<s3 dll> ~/bin/py -3.12 -m port.test.framegate cov_r_s2_screens --checkpoints editor,buy_land,skill_points --no-probe --images <dir> --out <tsv>
SG_PORTSIM_DLL=<s3 dll> ~/bin/py -3.12 log/roadmap/S2/10_tools/controls.py <out tsv>     # from port/build/S2_10_s3
sh log/roadmap/S2/10_tools/gates.sh
```

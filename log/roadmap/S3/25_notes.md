# S3.25 notes (wave 8, W8.4b: the original's recordings for groups 2-7 of 22_causes.tsv)

Worker: claude2 (Opus), 2026-10-30. Game slot: instance `instances\s3_w8` (created from original\ by re/tools/instances.py, pinned
winmm.dll 381ed2b2 copied from instances\s3_5, simgolf_shim.ini identical to s3_5's). Every run: hooks off (scenario_refs.py run, OFF
arm), C3FT_AUDIO default (none), one boot per heavy-lock hold (log/roadmap/S3/25_heavy.py). No port code changed.
U-1834..U-1839 reserved (rows in 25_uncertain.tsv if used).

## Tools

- `25_rec.js` (agent, appended to the pinned scenario's agent like S3.5's scene_rec.js) + `25_rec.py <preset> <scenario>` (driver):
  per frame of the original (a frame = from resetSpriteQueue 0x004627d0, called once by mainLoop at 0x004105cf, to the next one; the
  checkpoint waits in H1 after the present at 0x0041e29b) it logs every main-thread entry of the preset's exe functions as
  [seq, hook id, return address, ecx, 8 stack dwords, strings of pointer args, eax], copies the root DIB at renderSpriteQueue's
  return (0x00463100), and with boxes reads the 16-bit root pixels of each box at every hooked return after the queue (a change is
  logged with the row of the returning call: inner calls return first, so the first event after a write names the innermost hooked
  writer). Preset `queue` also copies the sprite-queue arrays (0x0082415c..0x0083820b), g_sheetFirstCell 0x005a9370, g_sheetCols
  0x0053f3e8 and g_images 0x0081ca10 at renderSpriteQueue's entry. Output port/corpus/w8rec/<scenario>/<preset>/ (index.json with the
  hook table and the det_b verdict; <cp>.rows.json.gz; <cp>.rootq.z = zlib RGB555 root at the queue's return, memory row 0 = top;
  <cp>.q.bin).
- `25_decode.py rows|boxes|queue` reads them (queue: the draw order g_spriteQueueHead -> g_spriteQueueNext, each entry with its sheet
  id / column / row from the cell pointer: cellAt 0x0043d6f0..0x0043d731 returns 0x005aaa30 + 0x2c * (first[id] + row * cols[id] + col),
  and the sheet's .flc path from port/src/sim/menu_flics.inc).
- `25_heavy.py "<what>" -- <cmd>`: W8_HEAVY_RULE.md as a wrapper (wait file, mkdir lock, owner.txt, commit >= 16 GB checked before and
  after the lock is taken, waits behind older wait files, 2 min after its own release).
- Group 2 uses S3.2's recorder unchanged: re/tools/terrain_calls.py --scenario cov_r_s2_screens --pass-frames (output
  log/p5/terrain/s325_cov_r_s2_screens/, S3.2's layout, which port/test/terrainlive.py reads).

## Group 7: the 4x4 marker after the queue (cov_b_inject zoom_box / forced_events / end)

Recording: `25_rec.py inject cov_b_inject` (113 hooked exe drawing functions + Terrain::drawLine, rows after the queue only, boxes
(142,89)-(145,92) and (568,355)-(571,358)); run log/c3/alt/refs/cov_b_inject/s325_inject SAME as det_b (5 checkpoints).
The box events (`25_decode.py boxes cov_b_inject <cp>`), identical in the three checkpoints except the position:

    80 Surface_blit3      ra=0x00418b4d ecx=0x00519cd4 args=[0x519cd4, 0xc8, 0x96, 0x190, 0x12c, 0x0, 0x0, 0x320]
    81 Surface_drawScaled ra=0x00418bbd ecx=0x00567b08 args=[0x519cd4, 0x8e, 0x59, 0x1, 0x1, 0x1, 0x0]   (zoom_box, end: x 142, y 89)
                                                       args=[0x519cd4, 0x238, 0x163, 0x1, 0x1, 0x1, 0x0] (forced_events: 568, 355)

The code (mainLoop P5, 0x00418b05..0x00418bc5): when unk_005a9cbc != 0 (the zoom box flag), Surface_blit3(this = root 0x00519cd4,
0x00519cd4, 0xc8, 0x96, 0x190, 0x12c, 0, 0, 0x320, 0x258) at 0x00418b48 draws the magnified view; then for each i < g_nameLabelCount
0x0058bcb0, x = g_ballScreenXs[i] 0x005a47e8, y = g_ballScreenYs[i] 0x005a4868, esi = 2x - 0x190, edi = 2y - 0x12c (0x00418b66 /
0x00418b6d); inside 0..0x31f x 0..0x257 and when Widget_readPixel(root, esi, edi) 0x00478df0 returns 0x7fff (0x00418b98), it calls
Surface_drawScaled(this = g_sprite567b08 0x00567b08, [f_stateptr_4c1570] = 0x00519cd4, esi - 2, edi - 1, 1, 1, 1, 0) at 0x00418bb8.
So the marker is the ball's dot drawn over the zoom box. The box pixels after that call, equal to the det_b reference frame in all 16
px at all three checkpoints (RGB555, rows top to bottom):

    779c 7fff 7fff 5294 | 7fff 7fff 7fff 779c | 7fff 779c 779c 5294 | 4210 5294 4210 2108

Before the call (after Surface_blit3) the box held the magnified scene; at zoom_box: 2206 2206 2627 2627 | 0000 0000 7fff 7fff |
0000 0000 7fff 7fff | 0000 0000 0000 0000 (the 7fff at box (2, 1) = (144, 90) is the readPixel test point). The port already makes
this call (port/src/sim/tick_frame.inc, ML+0x95df..ML+0x95f8, with the cut gfx_hud.cpp `loadInterfaceArt+0x3782`: image 0xc4, (0, 0),
4 x 4) and its frame differs from these 16 px in 15, drawn black (22_causes.tsv; which px is equal was not read here): W8.4c's question is
the content the backend holds for g_sprite567b08 or its draw path, not a missing call.
cov_b_inject2 (`25_rec.py inject cov_b_inject2`, run log/c3/alt/refs/cov_b_inject2/s325_inject SAME as det_b, 5 checkpoints) has the
same call at all three of its boxes: Surface_drawScaled from 0x00418bbd at (142, 89) zoom_box, (576, 365) forced_events, (272, 61)
end, each followed by these 16 px; all six det_b boxes equal them (25_group7.log). At inject2 zoom_box, box2 (272, 61) is also written
after the marker pass by Surface_blit2 (ra 0x0040caca), Surface_stretchBlit (ra 0x0040d4ad) and Surface_drawScaled (ra 0x0045c2c6),
the panel drawn there at that checkpoint; the marker is drawn at (272, 61) only at `end`.
The post-queue rows of each frame are all in the recording (zoom_box 1,165 rows), e.g. the floating text `Birdie. This is a really good
course.` at drawCenteredText ra 0x00416dd4 and the panel calls of P6 after the marker.

## Group 2: the terrain edge at the map corner (cov_r_s2_screens, box 781..799 x 451..466)

Recording: `re/tools/terrain_calls.py --scenario cov_r_s2_screens --instance s3_w8 --label s325_cov_r_s2_screens --pass-frames`
(S3.2's recorder unchanged): log/p5/terrain/s325_cov_r_s2_screens/ (calls.txt 22,857 Terrain calls, snaps/<cp> state copies for all 32
checkpoints, frames/<cp>/{entry,exit}.z and the pass copies of the last frame with a pass, course.pass / end.pass / best_scores.strip);
run log/c3/alt/refs/cov_r_s2_screens/s325_cov_r_s2_screens SAME as det_b (32 checkpoints). This is the scenario's missing S3.2 terrain
recording: W8.4c can run S3.2's G2 / terrain tools on it with this label.
What it shows (log/roadmap/S3/25_group2.py prints it):
- at `course` (terrain frame 279) and `radial_hover` (306) the frame makes no pass (unmoved camera: the back surface is copied whole), and
  the root at updateTerrainRender's return equals the det_b frame AND the scene corpus root at renderSpriteQueue's return in all 304 px
  of the box: the box is terrain only, nothing later draws over it. At the six report checkpoints (rep_comments 318 .. rep_roster 438)
  the exit box equals `course`'s and the det_b frame differs from it in 201 px (254 at rep_roster): the report's dim band over it.
- the passes before `course`: a full `render` at Terrain call seq 19931 (calls.txt line 19932: render(Tile 0x0398b4f4, angle 0.0)),
  then localRenders at seq 20156..20235 (80), 20269 and 20384 (= frame 155's only pass). Frame 155's entry, pass0_pre, pass0_post and
  exit all equal `course`'s exit inside the box (outside it pass0_pre differs in 530 px, entry in 118,648), so the corner was final
  before frame 155; the 80 localRenders between the render and frame 155 were not copied one by one (--pass-frames keeps the last frame
  with a pass). The next render (seq 22650, Tile 0x0398b064) comes after `course`.
- the corner (course exit, RGB555; 25_group2.log has the 16 x 19 table): per row the last px that is not one of {0x0000, 0x0020,
  0x0420, 0x0421} is at x - (451, 452), 782 (453), 783, 785, 786, 787, 788, 790, 791, 792, 793, 795, 796, 797, 798 (466): the map edge
  moves 16 px right over rows 453..466. Right of it the original has no plain black but a checker of 0x0020 / 0x0421 / 0x0420 with
  0x0000 (e.g. row 454: 29e8 2a09 25e7 0421 0000 0421 0000 0421 0000 0421 0000 ...), the non-zero dark px running from just right of
  the edge to x 785 (451) .. 790 (454) .. 799 (460..465); 0x0000 beyond it, and row 466 has none.

## Group 5: the missing connector line (cov_k3_camera y_inj3, box 398,275..431,496)

Recording: `25_rec.py camera cov_k3_camera` (drawMapLine, drawMinimapSegment, setLinePoint, lineTo, drawCircle, drawAmenitiesPanel,
showInfoCursorPanel, drawCenteredText2, drawTooltip, Terrain::drawLine body RVA 0x48a0, and worldToScreen / tileToScreen only when they
return into mainLoop, with their two out values); run log/c3/alt/refs/cov_k3_camera/s325_camera SAME as det_b (21 checkpoints).
Table of every y_inj checkpoint: 25_group5.log. In each frame mainLoop P7 draws the cursor tile's diamond as four drawMinimapSegment
0x00406220 calls (ra 0x00419e6f, 0x00419ec4, 0x00419f1c, 0x00419f71), each -> drawMapLine 0x004493d0 (ra 0x00406248) ->
Terrain::drawLine (ra 0x004493fe) with colour 0x80007d08, width 1, 7, between the screen points of five worldToScreen 0x0042fb90 calls
(ra 0x00419e17, 0x00419e45, 0x00419e9d, 0x00419ef2, 0x00419f47), then a sixth (ra 0x00419fa7) for the centre, where the label is drawn
(drawCenteredText2 ra 0x0041a5b5, text `Benches`). At y_inj3:

    worldToScreen(0xc400, 0xd400) -> (368, 295) ret 1      worldToScreen(0xc7ff, 0xd400) -> (399, 276) ret 1
    worldToScreen(0xc7ff, 0xd800) -> (431, 496) ret 1      worldToScreen(0xc400, 0xd7ff) -> (399, 315) ret 1
    worldToScreen(0xc400, 0xd400) -> (368, 295) ret 1      worldToScreen(0xc600, 0xd600) -> (400, 295) ret 1 (label centre)
    Terrain::drawLine (368,295)-(399,276) | (431,496)-(399,276) | (431,496)-(399,315) | (368,295)-(399,315)

So the connector is two diamond edges ending at (431, 496), the screen point the ORIGINAL's worldToScreen returns (eax 1, both outs
written) for the corner (0xc7ff, 0xd800). The other corners of that tile, and every corner at the other seven y_inj checkpoints
(cursor tiles at world (0xc400, 0x400 / 0xc00) and (0xb800, 0xc400)), give a regular diamond 63 px wide and 33..38 px tall (y_inj0:
(336,255) (367,241) (399,261) (367,275); y_inj1 / 2: x 400..463, y 281..314; y_inj4..7: x 432..495, y 256..294). This contradicts U-1376's account (frame slots [F+0x8c] / [F+0x94] left unwritten because
worldToScreen returned 0): in the original every one of these calls returns 1 and writes its outs. W8.4c's comparison: the port's
worldToScreen for (0xc7ff, 0xd800) at that state against (431, 496). Why the original's value jumps 181 px below the
(0xc400, 0xd7ff) corner is not read here (U-1834).

## Group 3: animated scene sprites (cov_b_holes18 holes; cov_k3_camera2 y_inj4 below)

Recording: `25_rec.py queue cov_b_holes18` (every enqueueSprite / addSprite call with caller and arguments, drawQueuedSprite and its
drawing calls, buildSceneSprites / drawRecords / drawWorldObjects; the queue arrays at renderSpriteQueue's entry; the root at its
return; the box (5,131)-(100,185) watched from resetSpriteQueue in the frames up to 2.5 virtual s before `holes`, checked only at the
drawing hooks); run log/c3/alt/refs/cov_b_holes18/s325_queue SAME as det_b (6 checkpoints). The checkpoint frame is terrain frame
3266 (= renderSpriteQueue call 3266). Cells decoded with g_sheetFirstCell / g_sheetCols from the same dump (25_cells.py).
- The left-shore px of the box (22_causes.tsv's 820 px "water ripples") change at exactly three calls in the frame, all before the queue
  is drawn: mainLoop's per-tile ground pass (P1) calls addSprite 0x004628d0 at ra 0x0041182f, which draws at once (drawQueuedSprite ra
  0x00462a21 -> Sprite_draw ra 0x00463040, root 0x00519cd4, palette g_images[0xb2] = 0x00820740 since flags 0xb2):

      addSprite(x 16, y 140, key 140, 60, 60, cell, shade 4, flags 0xb2) cell 45606 = sheet 0x232 col 1 row 1 (Water\UW*RockC / UWTropCoralC)  -> Sprite_draw at (-44, 80)
      addSprite(x 48, y 160, key 160, 60, 60, cell, shade 4, flags 0xb2) cell 45498 = sheet 0x230 col 1 row 0 (Water\UW*RockA / UWTropCoralA)  -> Sprite_draw at (-12, 100)
      addSprite(x 80, y 140, key 140, 60, 60, cell, shade 4, flags 0xb2) cell 45426 = sheet 0x22f col 1 row 1 (Water\AS*RockB / ASTropSandB)    -> Sprite_draw at (20, 80)

  (25_group3_holes_cells.log; g_sheetCols 12, 12, 24.) These are the underwater rock / coral cells of water tiles, and the ORIGINAL
  picks column 1 for all three at this frame; W8.4c compares the port's addSprite cells at the same call site.
- The fountain sprays: five enqueueSprite calls at ra 0x00412bae (P2) queue sheet 0x1a0 (Scenic\*waterspray) column 2 row 0, flags
  0x1d, shade 4, 40 x 40, at (240, 100), (304, 60), (432, -20), (688, 20), (784, 0) (anchor x, y; seq 129..138). Their boxes were not
  watched; the queue order and every entry are in holes.q.bin (`25_decode.py queue cov_b_holes18 holes`).
- A flag pole: addSprite at ra 0x00410b02 of the SpriteRef 0x00587fdc (not a sheet cell; 5 x 40, flags 0xffffffff), drawn by
  Surface_blit2 at (107, 120) with no palette object.
- cov_k3_camera2 y_inj4 (`25_rec.py queue cov_k3_camera2`, run log/c3/alt/refs/cov_k3_camera2/s325_queue SAME as det_b, 25
  checkpoints; box (65,413)-(97,451) watched as above; terrain frame 153): one call changes the box, the queued spray
  enqueueSprite(ra 0x00412bae, anchor 80, 440, 40 x 40, shade 4, flags 0x1d) of sheet 0x1a0 column 9 row 0 (cell 46361), drawn by
  Sprite_draw (ra 0x00463040) at (40, 400) with palette g_images[0x1d] = 0x0081d408.
- The spray's column against g_date 0x00834170 as dumped at renderSpriteQueue's entry, with g_sheetCols[0x1a0] = 13 (first cell 46352):
  holes 3265 % 13 = 2 (column 2), y_inj4 152 % 13 = 9 (column 9), golfer_stories season_end 2082 % 13 = 2 (its spray entry 5: column 2).
  The three recorded frames agree with column = g_date mod 13; the instructions at P2's call site (the enqueueSprite at 0x00412ba9)
  are the place to confirm it (not read here).

## Group 4: the translucent ellipse (golfer_stories season_end, box 60,70..102,90)

Recordings: `25_rec.py queue golfer_stories` (queue dump, no box; run s325_queue SAME as det_b, 6 checkpoints) and
`25_rec.py queue golfer_stories --suffix _box` (the box watched from resetSpriteQueue in the frames up to 2.5 virtual s before
season_end; port/corpus/w8rec/golfer_stories/queue_box/, run s325_queue_box SAME as det_b, 6 checkpoints). Checkpoint frame 2083. The
root at renderSpriteQueue's return equals S3.5's scene corpus copy (0 px) for season_end and season_0120.
- It is not a queue entry. The box changes at one call before the queue: Sprite_draw 0x004741b0 at ra 0x00415a44 (mainLoop P4,
  0x004159f7..0x00415a3f), this = g_sprite5423e8 0x005423e8, args (root 0x00519cd4, x 0x3c = 60, y 0x46 = 70, [g_color16Lut
  0x00824148] = 0x03a20048, 0): 707 of the box's 903 px change. The code: byte [g_ui_569498 0x00569498] == 1, golfer index
  [0x004c2e08] != -1, that golfer's record byte at 0x005794d9 + 0x100 g (golfer field +0x21) != 0, then x = [0x005794c0 + 0x100 g] -
  0x14, y = [0x005794c4 + 0x100 g] - 0xa (so that golfer is at (80, 80)). The port transcribes the same call (port/src/sim/tick_frame.inc
  P4_selectedSprite, ML+0x6437..ML+0x6484) and the backend holds the cut (port/src/sim/gfx_hud.cpp `loadInterfaceArt+0x3a51`: image
  0x225 at (0xd7, 0x2b), 0x2b x 0x15 = 43 x 21 px, the box's size). So W8.4c's question is why the port's frame has no ellipse: the
  condition's values in the port at that frame, or the call reaching the backend (whether it is handed over and drawn with
  g_color16Lut's blend); not a missing body.
- After the queue the box changes once more (103 px), detected at the return of Surface_blit2 seq 519 (ra 0x004740b3, inside
  Surface_drawScaled2 seq 518, this 0x005205bc, called by drawScaledSprite421b90 at ra 0x00421bb9 with (0x289, 0xd)); the previous check
  was the return of Surface_drawScaled2 seq 516 (this 0x00520590, ra 0x00418f0c, (0x30, 5)). The queue preset hooks few functions, so
  this change is bounded to that interval, not attributed to one call (22_causes.tsv's 32 q_after px of this row).

## Summary table and how to reproduce

`25_recordings.tsv` (from `~/bin/py -3.12 log/roadmap/S3/25_table.py`, which re-runs scenario_refs' comparison for every run): 19 rows,
every recording SAME as det_b at all its checkpoints. Runs (instance s3_w8, from the repo root, each under the heavy lock):

    SIMGOLF_ALLOW_MULTI=1 ~/bin/py -3.12 log/roadmap/S3/25_heavy.py "<what>" -- ~/bin/py -3.12 log/roadmap/S3/25_rec.py inject cov_b_inject --instance s3_w8
      (likewise: inject cov_b_inject2 | camera cov_k3_camera | queue golfer_stories [--suffix _box] | queue cov_b_holes18 | queue cov_k3_camera2)
    SIMGOLF_ALLOW_MULTI=1 ~/bin/py -3.12 re/tools/terrain_calls.py --scenario cov_r_s2_screens --instance s3_w8 --label s325_cov_r_s2_screens --pass-frames
    ~/bin/py -3.12 re/tools/scenario_refs.py check --scenario <s> --run s325_<preset>[_box]     # SAME vs det_b
    ~/bin/py -3.12 log/roadmap/S3/25_decode.py boxes|rows|queue ...; 25_cells.py; 25_group2.py   # the readings above

Real time per boot: cov_b_inject 3 min, cov_r_s2_screens 2 min, cov_k3_camera 3 min, golfer_stories 6-10 min, cov_b_holes18 18 min,
cov_k3_camera2 3 min, cov_b_inject2 3 min (the queue preset logs every enqueueSprite of every frame, so holes18 is slow).

## Process

- Every recording's hooks are Frida Interceptor attachments on the main thread (no clock call): with up to 114 hooked exe functions
  (inject) none changed the timeline; each run's checkpoints are SAME (state and frame) as det_b. drawFlyingBall 0x004038f0 and
  screenToTile 0x00430020 (uninitialised stack reads, CLAUDE.md batch b5 / boot notes) were left unhooked on purpose; worldToScreen
  was hooked only in the camera preset, which was SAME too.
- A first cov_b_holes18 attempt (box watch on every drawing call of every frame) was stopped by its own PIDs after 20 min (no output;
  its lock released by hand); the box watch was then limited to the frames before the focus checkpoints (`watchT`).
- Heavy lock: 25_heavy.py holds one boot per hold, checks the commit before taking the lock (the first version took and dropped it
  while the commit was below 16 GB), queues behind older wait files only (the lead's order, ORCH_LOG 23:03). Its first launch failed
  because ~/bin/py is a shell script (CreateProcess); the empty lock dir it had made (no owner.txt) was removed at once. Once (01:15)
  S3.21 took the lock while this step's wait file was the oldest.
- original\'s saves are byte-identical before and after (25_original_saves_before.txt / _after.txt). instances\s3_w8 is kept.
- Not done: group 6 (hire, addSprite FLIC cells) was not in this step's list; no recording for it.

## Open

- U-1834 (25_uncertain.tsv): why the original's worldToScreen(0xc7ff, 0xd800) gives y 496 (group 5).
- Group 3: the five spray boxes and the flag poles' boxes were not watched (only the shore box at holes and the spray box at y_inj4); their
  calls and cells are in the logs. The spray column = g_date mod 13 holds on three frames; the code at 0x00412ba9 is unread.
- Group 4: the post-queue 103 px change is bounded to two hooked returns, not attributed to one call.

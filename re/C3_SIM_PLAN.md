# Plan: dedicated batches for the big simulation functions (2026-10-11)

Scope: the open (C2, not dead/trivial) golf_clean.exe functions of 1 KB and more in the simulation, golfer and terrain
subsystems. Most of their callees are already at C3; what blocks them is a handful of small shared leaves still at C2,
two call cycles, and one file-reading chain. Order is bottom-up: a function goes only after every C2 callee it has is
promoted (the main-build A/B then runs it over C3 callees, and the callees-off run over the originals).

All are integer code (x87 instruction counts: simulateBallFlight 4 of 1,191, simulateShot 4 of 2,650, the rest 0), one
address range each.

## Phase 0: shared leaves (one batch, Sonnet)
These nine block every phase below.

| addr | name | bytes | blocks |
|---|---|---|---|
| 0x4481b0 | playSound | 80 | advanceStory, editTerrainHeight, emitGolferThought, updateGolfers, narrateShot, announceVipGolfer |
| 0x40c500 | playSoundAt | 487 | advanceStory, updateWalkers, placeAnimal, emitGolferThought, updateGolfers |
| 0x45c0c0 | pumpAndRedraw | 142 | simulateBallFlight, simulateShot, pathStep, updateGolfers, narrateShot |
| 0x4668f0 | storyText | 254 | advanceStory |
| 0x4070b0 | inRange | 448 | trialShot, updateGolfers |
| 0x42f340 | rebuildTileCoverage | 362 | applyTerrainEdit, editTerrainHeight |
| 0x409620 | scanShotArea | 803 | updateGolfers |
| 0x409a90 | scanNearbyGolfers | 345 | updateGolfers |
| 0x45bf80 | waitTicks | 127 | (mainLoop) |

Methods already proven: recording callbacks in the sound function table (c3dm, c3dt), message pumps through the import
stand-in (c3dw), live globals under __freeze.

## Phase 1: the golfer text chain (one batch, Opus)
loadStoryStrings 0x466b70 (1,426 B) -> buildGolferComment 0x469b00 (11,309 B) -> emitGolferThought 0x467a00 (7,763 B,
calls itself) -> announceVipGolfer 0x45de80 (4,426 B), advanceStory 0x466370 (1,376 B).
Blocker so far (round 9): loadStoryStrings reads the story file with the CRT's fopen/fgets. In this exe the CRT is
static, so the file goes through KERNEL32 CreateFileA/ReadFile imports: stand those in with the import stand-in and serve
a fixture file whose lines the fixture controls. Every function after it then runs on a known string table. The
message buffers (0x51a068, 0x58a528) and the RNG seed 0x822d9c go in the state (round 9's event-builder rule).

## Phase 2: shot physics (one batch, Opus)
simulateBallFlight 0x422fb0 (4,455 B), trialShot 0x4226a0 (2,320 B) and simulateShot 0x424120 (9,490 B) call each other
(simulateBallFlight -> trialShot -> simulateShot -> simulateBallFlight), so they go in one batch and are verified
together, each also with the other two hooks off. They draw through the display (category `display`): the recording
display and synthetic Surfaces of rounds 24-28. Needs phase 0 (pumpAndRedraw) and phase 1 (emitGolferThought).
narrateShot 0x407e00 (6,107 B) follows in the same batch if time allows: its saveBmp 0x475580 call writes a file, so
CreateFileA/WriteFile are stood in and the written bytes compared.

## Phase 3: movement (one batch, Sonnet)
pathStep 0x42e7e0 (1,665 B), updateWalkers 0x402a40 (3,729 B), placeAnimal 0x4017d0 (1,061 B). Tables: g_walkers
0x585850 (stride 0x4c), g_tileFlags 0x53caf0, g_typeAttr 0x578372. Needs phases 0 and 1.

## Phase 4: updateGolfers (a single-function batch, Opus)
updateGolfers 0x4289e0, 20,851 B, 5,560 instructions, 45 callees (33 at C3). The per-tick golfer state machine: one
registry key per golfer state, fixtures from the golfer records 0x5794c0 (0x100 each) and course records, the branch
table pinned with mutation builds as matchUpdate's was in round 9 (5.7 KB, 16 keys, 23 mutants). Needs phases 0-3 and
skillPointDialog 0x45f0f0 (a dialog: it is stood in by a recording stub for the key, or the dialog arm is held).

## Phase 5: terrain editing (one batch, Sonnet)
rebuildTileCoverage (in phase 0), editTerrainHeight 0x409cf0 (1,087 B), applyTerrainEdit 0x470a60 (6,056 B, `live`: it
edits the tile grid the render thread reads, so the first thread is frozen and the grid saved/restored). Its Terrain.dll
calls go to hooks already at C3.

## Not in this plan
- mainLoop 0x40f5c0 (8,035 B, 25 ranges, 55 callees incl. the asset loaders and the frame pump): the game loop itself.
  It is not a function a path-1 A/B can call meaningfully; its evidence is the C4 scenario runs.
- The asset loaders (loadInterfaceArt 24 KB, loadWorldAssets 17.7 KB, loadInfoScreenArt, loadGameBrowserScreen): a
  separate track (file I/O over the shipped archives).
- The UI screens (showCourseReport 14 KB, showRoutingMapScreen, courseActionMenu...): the drawing batches' track.

## Order and cost
Phase 0 first (blocks everything), then 1 and 3 in parallel, then 2, then 4 and 5. Five batches, about 30 functions,
~120 KB of code. Phases 1, 2 and 4 are Opus by the size and the cycles; 0, 3 and 5 Sonnet.

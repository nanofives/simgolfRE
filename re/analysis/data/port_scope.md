# Port scope manifest (phase 2.0, 2026-10-18)

Every one of the 2,934 golf_clean.exe functions (re/functions_ghidra.tsv) is assigned to one port module, with its mainLoop phases,
a rule id and the GameState objects it touches. Files: `re/port/scope.tsv`, `re/port/scope_straddlers.tsv`, `re/port/simcore_order.tsv`;
rules in `re/port/scope_rules.md`; generator `re/tools/port_scope.py` (deterministic: two runs give identical files; inputs cached in
log/p2/p20/inputs.json, 45 s to rebuild); test `tests/test_port_scope.py` (static, 5 tests); notes `log/p2/p20_notes.txt`.

## Method
Static only. The call graph, tags, boot walk and mainLoop phase reach come from `model_inventory.Inventory` (a call site inside a
phase range of mainLoop seeds a walk down direct calls). Data accesses come from `model_scan` (30,102 references, the 1,812 functions
with entries in 0x401000..0x4a4ecf) mapped to the 989 objects of port/include/gamestate_meta.inc (classes sim / hidden / ui / pointer /
render / audio / engine / const; offsets there are relative to 0x004c1000). Evidence: hooks.csv level, hook source and the number of
data rows in the function's path-1 A/B CSVs (`vec=`), `scenario` when a scenario CSV is recorded; defects: rows of model_defects.md
that cite an address inside the function or its name. Functions at or above 0x004a4ed0 have no scanned references and are CRT or libjpeg.

## Counts per module (2,934)
| module | functions |
|---|---|
| crt-dropped | 1,263 (1,045 by range, 218 by tag) |
| ui | 709 |
| backend-gfx | 249 |
| boot-only | 195 |
| sim-core | 147 (incl. mainLoop) |
| engine-glue | 128 |
| backend-audio | 108 |
| formats-loaders | 93 (77 libjpeg, 16 save serializer) |
| sim-leaf-util | 22 |
| net | 17 |
| unknown | 3 (KeyTable::find, matchDirective, loggerSetPath: U-0662) |

## Per mainLoop phase (functions reached; sim-core / sim-leaf-util of them)
SETUP 531 (5/4), SESSION 621 (51/9), QUIT 511 (1/3), MENU 483 (6/5), PRE 577 (42/10), P1 225 (9/5), P2 246 (13/7), P3 532 (37/15),
P4 402 (27/12), P5 685 (70/18), P6 588 (42/13), P7 258 (18/8), H1 795 (108/19), H2 576 (58/12), no phase (indirect) 1,861 (2/0).
A function is counted under every phase that reaches it, so the rows overlap; the sums of the common helpers (CRT, draw, text) inflate
the phase columns, the sim columns are the useful ones. H1 is the biggest sim consumer (command handlers and the per-frame golfer
and hole bookkeeping), P5 next (the date step, golfers, walkers, events).

## Straddlers (126, re/port/scope_straddlers.tsv)
By chosen module: ui 62, sim-core 43, backend-gfx 14, formats-loaders 5, backend-audio 1, boot-only 1. 43 sim-core functions touch
heap objects too, mostly the root window / input objects (g_rootWindow, g_rootInput: 50 and 27 straddlers), the font and sprite
refs, the panel-list mode and the message-label queues. The commonest heap touch is g_rootWindow (50), then f_stateptr_4c1570 (47, U-0661).
Typical sim-core straddlers: spawnObject, placeAnimal, updateWalkers (sprite-sheet column tables g_sheetCols), canAfford,
raiseCornerLevelsAround / lowerCornerLevelsAround (surface table g_surfaces5a48e8), narrateShot, stepCourseEditQueue,
clearMatching, stampCourseTiles. The ui-module straddlers are the click / handler functions with command effects
(clickGolferPanel, handleEmployeeAction, pickBuilding, clickAmenityPanel, showBulletinBoard, showTournamentResults ...) and the
`show*Screen` modal screens that read and write sim state while drawing.

## The 20 largest sim-core functions (bytes)
mainLoop 73,813 (C2); updateGolfers 20,851; buildGolferComment 11,309; prepareShot 9,490; emitGolferThought 7,763; narrateShot 6,107;
generateRandomCourse 6,056; matchUpdate 5,723; chooseShotTarget 4,455; pairGolfersWithStory 4,426; updateWalkers 3,729; patronEvent 3,264;
announceBuilding 2,614; loadGolferRoster 2,509; trialShot 2,320; holeMagazineEvent 2,303; composeGolferSprite 1,963; skillPointDialog 1,908;
buildScenarioIntro 1,883 (retagged: tag frontend, no heap object); loadGolfPro 1,729. All except mainLoop are C4 (hook evidence); the
sim set is 153 C4, 6 C3, 10 C2.

## Sim-core order (re/port/simcore_order.tsv)
169 functions (147 sim-core + 22 sim-leaf-util), 11 levels (0..10: level = 1 + the highest level among callees inside the set, cycles
broken in DFS order), mainLoop last. Target was about 250: only 169 functions with data references are simulation, the others are
drawing, UI plumbing or CRT. 28 rows cite a model_defects.md entry. Suggested phase 3 child (hint, name / tag / graph rules):

| child | functions |
|---|---|
| 3.0 core leaves | 6 |
| 3.1 world and course editing | 90 |
| 3.2 economy and membership | 15 |
| 3.3 golfers | 13 |
| 3.4 shots | 13 |
| 3.5 events, news, stories | 21 |
| 3.6 walkers, animals, ambient | 8 |
| 3.7 tick and commands | 1 (mainLoop) |
| unassigned | 2 (listMatchingFiles 0x0043d2a0, inRange 0x004070b0; inRange is a 3.0 leaf) |

## What phase 3 (re/PHASE23_PLAN.md section 4) should change
1. 3.1 gets 53% of the list because "record / object / build / hole" are broad name hints: split it into 3.1a tile grids, cellAt,
   coverage and edits and 3.1b records, placed objects and habitats (the model has separate streams B and C for them).
2. 3.7 is nearly empty by name but the command effects live in the 62 ui-module straddlers (click and handler functions): 3.7's brief
   must include "split each handler into a pure command effect plus its drawing" and take them from scope_straddlers.tsv, not from
   simcore_order.tsv.
3. 3.2, 3.3 and 3.6 are small (15, 13, 8) next to 3.1 and 3.5; fold 3.6 into 3.3 or into 3.1b to balance the children.
4. Some sim-core functions read files (loadGolferRoster, loadGolfPro, the story file readers in 3.5): the loaders listed under 3.5 must
   read the user's install; keep them behind the file interface the plan already names.
5. mainLoop (R0-MAINLOOP) is one monolithic C2 body with an SEH frame and 15 uninitialised frame slots: it is the 3.7/3.8 integrator's
   work, not a leaf-first item, and has no path-1 A/B (re/c3_hold.tsv NO_PATH1); its oracle is the scenario / segment corpus.
6. The 2 `unassigned` and 3 `unknown` functions need a human look; U-0660 asks how much command logic still hides in `ui` and
   `backend-gfx` rows.

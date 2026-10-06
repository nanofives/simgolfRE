# exe_0 systems (golf_clean.exe, 0x00401000-0x0042f1c0)

210 functions. 191 named in `re/names/exe_0.tsv`, 11 globals in `re/names/exe_0_globals.tsv`,
19 left unnamed (see `log/naming/exe_0_notes.txt`). Every (string in the binary) citation is a 100%
byte match (C4). Names without a match citation are derived from string references, the call graph,
and the decompilation (NO-GUESSING: facts only).

This range is the heart of the game simulation and HUD. It contains the top-level loop
(`mainLoop` 0x0040f5c0), the golfer/walker AI, the course/terrain editing tools, the economy,
save/load, text building, and the 2D HUD drawing helpers. Most functions are reached through
`mainLoop`, which has 169 direct callees.

## 1. World objects and record banks (sim / render)

Placed objects on the course live in two parallel systems.

- **g_placedObjects** (0x0058bcb8): a flat table of 256 entries x 16 bytes. Fields confirmed from
  `objectAt` 0x0040df80: `short type @+0` (-1 = empty), `short x @+2`, `short y @+4`, `int variant @+8`.
  `objectAt(x,y)` scans it and returns the index whose footprint (size from `g_recordDefs`/0x004c26c0)
  covers a tile. `holeQuadrants` 0x00407000 scans the same table for type-4 objects within
  `tileDistance` and builds a 4-direction bitmask in `g_holeDirMask` 0x00541318.
- **Record banks** (g_recordBanks 0x004e6d20): multi-tile (string in the binary) objects (animals, scenery) with a
  6x6 footprint. `g_recordDefs` 0x004c11e0 (0x27 bytes/entry) holds the footprint bitmask and size.
  `placeRecord` 0x00401040 tests the per-bank occupancy grid `g_recordOccupancy` 0x004e6d70 for
  collision, writes id/x/y into the bank, and returns 1/0. `drawRecords` 0x004012d0 renders a bank:
  it maps each occupied cell to screen with world->screen 0x0042fb90, blits a sprite from bank
  0x004e9a80 selected by `g_cameraRot` 0x005685f4, and advances the animation counter at bank+0x60.
  `initRecords` 0x00401750 clears the banks at startup. Animals: `placeAnimal` 0x004017d0 and
  `tickAnimals` 0x00401c00.

## 2. Ambient walkers (sim / golfer)

- **g_walkers** (0x00585850): a table with 0x4c-byte stride (pos x @+0, y @+4) plus parallel per-entry
  state byte arrays based at 0x00585862 (flags, timers, direction @0x00585866, step counters).
- `spawnWalker` 0x00402970 seeds a walker at a random edge. `updateWalkers` 0x00402a40 (3729 bytes) is
  the per-tick update: it reads `g_date` and `g_flags`, steps each walker across the tile grid using
  the direction table 0x004c2878/0x004c2898, resolves collisions with the golfer table 0x00579560 and
  terrain (`tileBlocked`, `typeAtPos`), plays positional sounds (`playSoundAt`), and raises popups.
- `placeHabitat` 0x00405e30 places an animal habitat and seeds its spawn slots (`addHabitatSlot`).

## 3. Golfer simulation and events (golfer / sim)

Driven from `mainLoop` through `updateGolfers` 0x004289e0 (20851 bytes), the largest function here.

- `updateGolfers` handles golfer mood, leaving/resigning (strings at 0x004c750c and neighbours:
  tired/thirsty/hungry/insulted/club-thrown), match participation, and story progression. It calls
  `simulateShot`, `patronEvent`, `matchUpdate`, `resetGolfer`, `rateTile`, and `startMessage`.
- `simulateShot` 0x00424120 (9490 bytes) simulates a shot: `shotPower` 0x00422430 (via `decaySum`
  /`decaySum2`), `wobble` 0x00405920 for aim, terrain tracing via `heightBlend`/`rateTile`, and
  `thoughtFlag`. `scanShotArea` 0x00409620 classifies the lie/outcome and calls `narrateShot`
  0x00407e00, which builds the shot/skill text and the SimFoto snapshot (strings 0x004c4e58 (string in the binary),
  0x004c4fe8 (string in the binary), skill-change strings).
- `patronEvent` 0x004266b0 (3264 bytes) fires VIP events: celebrity vacation homes, landmark
  donations, commissioner/CEO expansion-and-investment approvals (strings 0x004c6d24, 0x004c6f88,
  0x004c7058), using `landmarkName` and `appendOpinion`.
- `matchUpdate` 0x00427380 (5723 bytes) handles tournament/match scoring, new course records
  (0x004c7228) and greens-fee collection.
- `membershipReport` 0x00406670 and `updateMembership` 0x00421bc0 manage club membership
  (string 0x004c6cac (string in the binary)).

## 4. Terrain and course editing (terrain / course)

- Tile data: `g_tile_type` (type grid, stride 0x32), `g_tile_byte`, `g_tileFlags` 0x0053caf0, and the
  per-type attribute table `g_typeAttr` 0x00578372 (stride 0x30, kind byte at 0x00578376).
- Height/geometry: `tileBlocked` 0x0040bf60 (29 callers), `typeAtPos`, `cornerHeights` 0x0040bfe0,
  `heightBlend` 0x0040c170, `sampleHeight` 0x0042dba0, `slopeX`/`slopeY`/`slopeMix`.
- Editing: `stampCourseTiles` 0x0040e000 paints tee/green/fairway/path shapes into `g_tile_type`
  (values 1/4/0x11/0x15/0x16) then rebuilds via 0x0042f7a0. `editTerrainHeight` 0x00409cf0 is the
  raise/lower tool (reads the camera tile 0x004c2ba0/0x004c2ba4). `demolishObject` 0x0040a4e0 removes
  placed objects after a confirm prompt. `floodMark` 0x0042f120 and `propagateType11` 0x0042f1c0 are
  the recursive flood fills that mark connected tile regions (path connectivity, the 0x40 flag).
- Ratings/costs: `rateTile` 0x00422530, `rateLot` 0x0042ef40, `clearCost` 0x0042ee80.

## 5. Economy (economy)

`canAfford` 0x00406c30 reads `g_cash` 0x00571fd4 and compares against a cost, posting an
insufficient-funds message. `hireEmployee` 0x0040aa80 gates hiring on course size (Daily-Fee course,
6+ holes) and cash. Lot/home-site valuation is `rateLot` 0x0042ef40 with `clearCost`.

## 6. Save / load (save)

- `saveGame` 0x0040b4a0 writes a `.sve` under `saved games\` using `serializeCourse` 0x0040afa0,
  which dumps tile/object state field by field through `saveFieldIo` 0x0040af70.
- `loadGame` 0x0040b9b0 and `loadCourse` 0x0040b840 read a file, `deserializeCourse` 0x0040bbf0
  restores the state, then 0x0042f7a0 rebuilds derived data.
- `saveGameDialog` 0x00405b10 is the name-entry dialog (`sanitizeFileName` rejects illegal chars).
- `loadTopScores` 0x0040ad60 loads the high-score table from `top10.sve`.

## 7. HUD / text (ui / render)

- Text is assembled into the shared buffer `g_textBuf` 0x0058a528 by the `append*` family
  (`appendNumber`, `appendCents`, `appendDate`, `appendHoleName`, `appendCourseTitle`,
  `appendClubName`, `appendUpgradeText`), all using __itoa.
- Drawing helpers: `drawText*`/`drawCenteredText*` push text into widget quads (`Widget::setQuad`
  0x00476310); `drawCircle`, `lineTo`/`setLinePoint` draw primitives; `blitPanel`, `drawWindowFrame`,
  `drawFrameR3`/`drawFrameS2`, `drawPanelS2`, and the dispatcher `drawWindow` 0x0040d320 render
  windows; `startMessage`/`tickMessage` run popup message boxes; `pointsPopup`, `drawLabels`,
  `drawMoneyLabels` draw the on-course overlays.

## 8. Frontend / main loop (frontend / sim)

- `mainLoop` 0x0040f5c0 (59614 bytes) is the top-level menu + per-frame game loop. Its menu strings
  (0x004c6c88 (string in the binary), 0x004c6c98 (string in the binary), 0x004c6c54 (string in the binary),
  0x004c6c64 (string in the binary)) and its single caller 0x0045baf0 identify it.
- `showLoadingScreen` 0x00406250 shows a random loading image and golf quote.
- `customizeCharacter` 0x0040f190 (+ `showCharacterIntro` 0x004065c0) runs character customization and
  writes the `.pro` profile.

## 9. Compiler-generated C++ static init (crt)

77 functions (subsystem `crt`) are compiler-generated: static initializers that construct global
objects (`staticInit_*`, from golf_raw_01.cpp), vector-constructor-iterator array initializers
(`initGlobalArray_*`, golf_arrays.cpp, calling ??_L 0x004a55d2), `_atexit` registration thunks
(`atexitRegister_*`, golf_raw_07.cpp calling 0x004a56d2), and destructors/unwind cleanup helpers
(`*_dtor`, `deletingDtor_*`, `unwindFree_*`). They carry no game logic; they are listed for
completeness because each is a 100% match.

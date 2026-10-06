# exe_1 systems (golf_clean.exe 0x0042f2c0 - 0x0045c460)

211 functions. The range is dominated by the in-game and front-end **UI**, the **Terrain.dll glue**
layer, asset loaders, and a tail of small **util/crt** helpers. 188 functions are named in
`re/names/exe_1.tsv`; the rest are listed in `log/naming/exe_1_notes.txt`. Every claim below is from the
decompilation/match data in `re/tools/xref.py` output; names in `[...]` are addresses.

## 1. Terrain heightfield and world->screen projection (subsystem: terrain / render)

Entry points: `rebuildHeightfield` 0x42f7a0 (called from course load 0x40b840 and the main loop).
It walks the tile grid calling `relaxEdges42f530` 0x42f530, `relaxTile42f630` 0x42f630 and
`raiseFromNeighbours` 0x42f6e0, each of which reads the four corner heights through
`cornerRange` 0x42f4b0 and skips blocked tiles via `tileBlocked` 0x40bf60.
`heightAt42fa30` 0x42fa30 interpolates a height inside a tile.

Projection: `worldToScreen` 0x42fb90 (1157 bytes) is the core world-tile -> screen-pixel transform; it
reads the camera tile pair **g_4c2ba0 / g_4c2ba4** and is wrapped by `tileToScreen` 0x42f940.
Many drawers call it (drawCircle, drawLabels, drawMoneyLabels). `markTileKind11` 0x42f2c0 marks a tile
kind into tables 0x5722e8 / 0x572cac.

## 2. Terrain.dll glue (subsystem: terrain)

The exe owns a `Terrain` handle in **g_820ed0** and pushes all tile data into the OpenGL terrain in
`Terrain.dll`. The exported accessors `Terrain::tileAt/getElevation/getWall/getType`
(0x4490d0..0x449150) are dead code in the exe (0 callers); the live copies are in Terrain.dll (see
project CLAUDE.md).

Read side (exe tile array): `tileType` 0x4492d0, `tileByte` 0x4492f0, `wallHeight` 0x449330,
`tileFlag20` 0x4493b0, `tileQuery449310` 0x449310.
Write side (into Terrain.dll, via imports at 0x4a4f28+): `syncTiles` 0x449470, `rebuildTerrainGrid`
0x449540 (setType/layPath/setWall/calcNormals/elevateCorner), `initTerrain` 0x449790
(initTerrain/initSystem), `refreshTerrain` 0x449520, `applyCourseType` 0x449400
(loadNewCourseType/passCollarInfo), `freeTerrain` 0x449860 (~Terrain/closeSystem).
Per-tile appliers driven by `rebuildTileData` 0x44a5b0: `applyTileVariation` 0x449f00,
`applyTileElevation` 0x449fa0, `applyTileWalls` 0x44a380, `togglePath` 0x44a410.
Render driver `updateTerrainRender` 0x4498a0 calls render/localRender/setZoomLevel/calcAllNormals;
`renderTerrainStrip` 0x44a6e0 calls stripRender. `allocTileBuffer` 0x449210 allocates the exe-side
tile buffer g_820f18. `drawMapLine` 0x4493d0 wraps the jgld `drawLine` export.

## 3. In-game UI panels and menus (subsystem: ui)

Each build/people panel follows a draw + hit-test + click triple:

- Build-slot panel: `clickSlotPanel` 0x433040 / `hitTestSlot432f90` 0x432f90.
- Terrain build toolbar: `drawTerrainToolbar` 0x433e50, `hitBuildTool433c60` 0x433c60,
  `clickBuildTool433d30` 0x433d30.
- Building panel: `drawBuildingPanel` 0x434350, `hitBuildingTool` 0x4340a0, `pickBuilding` 0x434140.
- Amenities/landmarks panel: `drawAmenitiesPanel` 0x434cf0, `pickAmenity434980` 0x434980,
  `clickAmenityPanel` 0x434ac0.
- Employee view: `drawEmployeeViewPanel` 0x435760, `hitEmployeeSlot435570` 0x435570,
  `clickEmployeeView` 0x435680; detail: `drawEmployeeDetailPanel` 0x436e50, `hitEmployee436b00`
  0x436b00, `handleEmployeeAction` 0x436c00.
- Golfer action panel: `drawGolferActionPanel` 0x4362f0, `hitGolferPanel435f00` 0x435f00,
  `clickGolferPanel` 0x436060.
- Info Cursor / shot-lie panel: `showInfoCursorPanel` 0x433190 (loads interface\\*.txt theme text via
  `lookupThemeText` 0x45b660).

Shared helpers: `drawPanelBackdrop` 0x432620, `nearestMenuSpot` 0x4326a0, `textInputBox` 0x45b2c0,
`drawValueWidget` 0x45b0d0, `renderTextWidget45af30` 0x45af30. Menus: `showSystemMenu` 0x432720,
`showMainRadialMenu` 0x432ba0, `showPreferencesMenu` 0x432560.

The confirm/undo flow strings (0x4c7e7c (string in the binary), 0x4c7ec4 "To UNDO
a previous action...") are shared by every build-panel click handler.

### Full-screen info screens (subsystem: ui, except economy ones)
`showCourseReport` 0x44fb30, `showHoleStatsScreen` 0x453330, `showPlayerCommentsReport` 0x4546b0,
`showMembershipRoster` 0x454c50, `showBestScoresScreen` 0x455a30, `showHistographScreen` 0x455ed0,
`showRoutingMapScreen` 0x456be0 (routing/aura/home-value overlay maps; cell->screen via
`mapToScreen456b70` 0x456b70 and `overlayCharAt` 0x456bb0), `showTournamentResults` 0x45a090,
`showShortcutsScreen` 0x44e770. Their PCX art is bulk-loaded by `loadInfoScreenArt` 0x44bde0.

## 4. Economy screens (subsystem: economy)
`showFinancialReport` 0x44f6b0, `showEndOfYearScreen` 0x44cff0 (board warnings, happy-ending events),
`showBuyLandScreen` 0x4587a0 (tracts for sale, canAfford 0x406c30), `showHireEmployeeDialog` 0x459400.

## 5. Front-end / boot (subsystem: frontend / boot)
`gameMain` 0x45baf0 is the program main (entry 0x4a682f): it registers the TrueType fonts (Klepto/Times
New Roman/Comic Sans/Arial, strings 0x4d2748+), creates the "Sid Meier's SimGolf" window, loads
`jgld.dll`, seeds the RNG (seedFromTime 0x467110), loads sounds, then runs the title and main loop and
tears everything down (freeTerrain, resetAllPanels, shutdownSoundSystem).
Title/menus: `showTitleScreen` 0x43cd70 (plays SMSG_introfinal.bik), `selectDifficultyScreen` 0x43a400,
`pickAProScreen` 0x43a8c0, `loadGameBrowserScreen` 0x43b610.

## 6. Character / golfer editing and stories (subsystem: golfer)
`showCharacterEditor` 0x4385d0 (7528 bytes) edits a golfer's gender/skin/hair/clothes/body and
saves/loads .chr/.pro files; hit-tested by `nearestCharControl` 0x438260, `charEditorZone` 0x4382f0,
`pickCharEditor` 0x438390. `showGolferPairSelect` 0x459850 picks the next pair.
`buildGolferBioText` 0x4315e0 composes bio text (favorite colour, likes). Story/thought helpers:
`substituteStoryNames` 0x45c460 (PARTNER/MYNAME tokens), `drawThoughtBubble` 0x45c200,
`thoughtBalance` 0x45c420 (reads g_579540), `golferScore` 0x453260, `appendRatingLabel` 0x4532a0.

## 7. Asset loaders (subsystem: render / course)
`loadWorldAssets` 0x43dbe0 (17732 bytes) loads every per-theme sprite/flic (trees, flowers, water,
bridges, buildings for Links/Parkland/Desert/Tropical). `loadInterfaceArt` 0x442180 (24059 bytes) loads
bldg.pcx plus the interface panels and then the world assets. `loadFlic` 0x43d740 loads a .flc into the
flic table managed by the block allocator (`allocBlock` 0x43d5d0, `freeBlock` 0x43d520,
`freeTable` 0x43d670). `loadThemeFile` 0x437fa0 loads a theme palette/asset from Themes\\.

## 8. Audio (subsystem: audio)
`loadSoundTable` 0x448220 (3724 bytes) loads the entire sound-filename table. `playSound` 0x4481b0,
`startSoundSlot` 0x448200, `playAudioFile` 0x43cce0, `shutdownSoundSystem` 0x4490b0.

## 9. Save / course info (subsystem: save)
`writeCourseInfo` 0x431fa0 writes the [Course] .srf text file; `saveThumbAndFull` 0x431ee0 and
`saveCourseJpg` 0x431d20 write Thumb.jpg / Full.jpg via CreateJPG; `flushCourseInfo` 0x432170 commits
them when g_587da0 is set. `promptAndSaveFile` 0x437910 validates a name and writes a course/character
file with an overwrite prompt.

## 10. Timing / message pump / RNG / text buffers (subsystem: util / input / render)
Message pump: `pumpMessages` 0x45c030, `pumpLight` 0x45c150, `pumpAndRedraw` 0x45c0c0,
`waitTicks` 0x45bf80. Redraw: `setDirtyFlagA/B` 0x45ae30/0x45ae50 (g_822b98/g_822b9c),
`flushRedraw` 0x45ae70, `waitRedrawReady` 0x45aed0, `drainAndRedraw` 0x45af00.
RNG: `Random::next` 0x45c1a0 (g_4ba800 LCG state), `Random::range` 0x45c1e0 (used by ~45 callers).
Text buffers: `appendString` 0x45b9f0 and `clearBuffers` 0x45b880 on g_56fcb0; `appendToBuffer45b8b0`
0x45b8b0 on g_59d81c; `replaceInText` 0x45b7c0. Grid: `cellAt` 0x43d6f0.

## 11. Compiler static-init / atexit machinery (subsystem: crt)
~53 small functions are MSVC-generated: `staticCtor_*` construct a file-scope global object,
`registerExit_*` register its destructor via `_atexit`, `arrayStaticInit_*` init global arrays of
objects (matched to re/match/golf_raw_0*.cpp and golf_arrays.cpp). They are listed individually in the
TSV with subsystem `crt`.

## Globals (see re/names/exe_1_globals.tsv)
g_820ed0 Terrain handle; g_4c2ba0/g_4c2ba4 camera tile; g_56fcb0 / g_59d81c text buffers;
g_4ba800 RNG state; g_822b98/g_822b9c redraw dirty flags; g_587da0 course-info dirty flag;
g_820f18 tile buffer; g_579540 thought table.

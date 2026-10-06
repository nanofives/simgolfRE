# exe_2 systems (golf_clean.exe 0x0045c560 - 0x0047ae80)

Range exe_2 holds four cooperating systems: the golfer text/story generator, the
isometric sprite draw queue, a 2D graphics surface library, and a rich-text / widget UI
toolkit. 161 of 211 functions are named in `re/names/exe_2.tsv`; globals in
`re/names/exe_2_globals.tsv`; functions left unnamed in `log/naming/exe_2_notes.txt`.

Addresses are VAs (image base 0x00400000, no relocations). Many functions in this range
are 100% matched (`re/match/golf_*.cpp`), which is the primary evidence for their bodies;
their matcher-chosen class names are hints only and the names below were set from each
function's decompiled behaviour, strings, and caller/callee chain.

## 1. Golfer text / story generation (golfer)

Produces every piece of golfer-facing prose: names, thoughts, hole comments, VIP notices
and the multi-chapter golfer stories.

- Golfer records live in two arrays:
  - `g_golferRecords` 0x0057956e, stride 0x80, read by `formatGolferTitle` 0x00462020.
  - `g_golferRoster` 0x0058587a, stride 0x4c, read by `pickGolferName` 0x00467600
    (copies a default first name from the name table 0x004c148c into the shared text
    buffer `g_textBuffer` 0x0051a068).
- Name assembly: `buildGolferName` 0x004676e0 (full name + roman-numeral suffix, 18
  callers), `appendEndearment` 0x00467560 (gendered suffixes at 0x004e1b5c+).
- Thoughts and comments: `emitGolferThought` 0x00467a00 (situational one-liners: first
  club use, new-facility appreciation, shot setup, slow play, VIP excitement; recurses
  and dispatches to many subsystems); `buildGolferComment` 0x00469b00 expands PARTNER /
  MYNAME / DATA() tokens (via `replaceInText` 0x0045b7c0) and appends opinion phrases via
  `appendOpinion` 0x00469a20 (phrases at 0x004e28e4+). `thoughtFlag` 0x004675d0 returns a
  thought-category bit.
- Stories: `advanceStory` 0x00466370 steps a golfer pair's chapter and fires its text /
  landmark / sound (see CLAUDE.md scenario `golfer_stories`); `storyText` 0x004668f0
  builds a chapter line; `computeStoryReply` 0x004669f0 recomputes a partner's chapter
  from mood/counter/latest-thought (note `re/analysis/golfers/004669f0_story_reply.md`);
  `loadStoryStrings` 0x00466b70 reads per-theme story text files.
- Roster / theme loading: `loadThemes` 0x004658b0 and `loadGolferRoster` 0x004659a0 read
  Themes\Standard\{progolfers.dta, celebrities.dta, *.glf, *.txt}; `loadGolfPro`
  0x0046c970 loads a hired pro from a .pro file. `listThemes` 0x004724c0 enumerates themes
  for the frontend.
- Panels / notices: `drawGolferInfoPanel` 0x0045c560 (the big per-golfer popup: marital
  status, Thirst/Hunger/Energy/Attitude, View Story / Next Chapter / Comments / Snapshot /
  Move-Eject / Customize buttons); `announceVipGolfer` 0x0045de80; `skillPointDialog`
  0x0045f0f0; `announceLandmark` 0x004722c0.
- Type helpers: `typeBit7Clear` 0x0046c940 (15 callers) tests bit 7 of a golfer type byte.

## 2. Isometric sprite draw queue (render)

A per-frame, depth-sorted display list of world sprites (golfers, objects).

- Storage: parallel arrays indexed by `g_spriteQueueCount` 0x00838200 (cap 0x3fe): x
  `g_spriteX` 0x00830164, y `g_spriteY` 0x00831164, flags `g_spriteFlags` 0x0082815c,
  sort key `g_spriteSortKey` 0x0082415c (plus 0x0082915c/0x0082a15c/0x0082b160/0x0082c164).
  A z-sorted singly-linked list threads them: head `g_spriteQueueHead` 0x0082c160, links
  `g_spriteQueueNext` 0x008361f4. Scaling uses `g_zoomScale` 0x004c2844 and tier from
  `g_zoomLevel` 0x00822c8c.
- Flow each frame (all from the main update 0x0040f5c0):
  `resetSpriteQueue` 0x004627d0 -> `buildSceneSprites` 0x00463180 (projects world cells
  via `cellAt` 0x0043d6f0 and `tileToScreen`, calls `addSprite` 0x004628d0 ->
  `enqueueSprite` 0x00462a30) -> `renderSpriteQueue` 0x00463100 walks the sorted list
  calling `drawQueuedSprite` 0x00462be0 (blit + optional label).

## 3. 2D graphics surface library (render)

A Surface/Image/device-context class family wrapping a backing device object
(`this+4`, called through its vtable). 100% matched in `re/match/golf_hand_0*.cpp`.

- Lifecycle: `Surface_ctor` 0x00473ab0, `Surface_create` 0x00474550 /
  `Surface_createFull` 0x00474dd0 / `Surface_init` 0x00473ae0, `Surface_open`
  0x00473b50; `Image_ctor` 0x00474ae0 / `Image_dtor` 0x00474c40;
  `initGlobalImage` 0x00473a70 constructs the global image object 0x00839348.
- Blits / draws: `Surface_blit`/`blit2`/`blit3` (0x00473bf0, 0x004740f0, 0x00475d00),
  `Surface_stretchBlit` 0x00473f60 (device vtable +0x54), `Surface_drawSprite`
  0x00474440 (vtable +0x94, origin at this+0x20/+0x24), `Image_draw` 0x00473e60
  (40+ callers), `Surface_drawScaled*` (0x00473cb0, 0x00474030, 0x00474260),
  `Sprite_draw`/`Sprite_drawScaled` (0x004741b0, 0x00474260), `drawComposite`
  0x004791b0 (left/mid/right via blitLeft/Mid/Right 0x00475e10/0x00475fb0/0x00476140).
- Primitives: `Surface_box` 0x00475b20, `Surface_fill` 0x00475da0 / `Surface_fillRegion`
  0x00475c90, `Surface_paint` 0x00474e70, `Surface_clip` 0x00479a80, `Surface_pixelPtr`
  0x004796a0, `drawPixelRun` 0x00479950, `drawLineH/V` 0x00478df0/0x00478ea0,
  `blendRed/Green` 0x00479830/0x004798c0. `DC_call84/90/a0/a4` (0x00475b60..0x00475c20)
  forward to fixed device-context vtable slots.
- Image files: `loadBmp` 0x00474ee0, `flipBmpRows` 0x00475410, `loadBmpSurface`
  0x00475210, `saveBmp` 0x00475580, `loadImage` 0x00475840 (22 callers),
  `loadImageData` 0x00478cd0 and `openImageAsset` 0x00478f50 (.pcx/.pcf/.pcg, signature
  (string in the binary)).
- Colour: `build16BitColorTable` 0x00461830 fills the 16-bit LUT `g_color16Lut`
  0x00824148, picking 565 vs 555 from the device cap on `g_graphicsDevice` 0x0083ad50
  (vtable +0xb4); `brightenComponent` 0x00461810.
- Draw context: `setDrawTarget` 0x004762d0 sets the target surface and clip rect on a
  draw-context object.
- Memory: `heapAlloc` 0x00474860 (block heap, prints (string in the binary) on oversize),
  `Buf_alloc` 0x00474820.

## 4. Rich-text and widget UI toolkit (ui)

Text layout with an inline markup language and simple widgets/windows.

- Markup tokens `$DROPLINK`, `$DROPDOWN`, `$LINK<` (strings 0x004e423c+). Parsing:
  `skipToken` 0x00476d80, `scanToken` 0x00476dd0, `wrapText` 0x00476e20,
  `measureMarkupRun` 0x00476ef0, `drawMarkupRun` 0x00477280. Core renderer
  `drawRichText` 0x004775b0 (8 alignment-wrapper callers) with buffer from
  `allocTextBuffer` 0x004767a0.
- Alignment wrappers: `drawText` 0x00477c30, `drawStringLeft` 0x00477fc0,
  `drawString` 0x00478140, `drawCentered`/2/3 (0x00477cd0, 0x00477da0, 0x00477eb0),
  `drawRight`/2 (0x00478080, 0x004781f0), `drawBox` 0x00477e60, and the
  measure+draw block helpers 0x004782d0/0x00478430/0x00478530/0x00478610/0x004787a0.
  `Text_put` 0x00477250 / `Text_flush` 0x00478970 accumulate into a text object.
- Widgets/windows: `Widget_setQuad` 0x00476310, `Widget_value` 0x00477580 (23 callers),
  `Widget_get` 0x00477560, `Widget_draw`/2 0x00476650/0x004766a0, `Widget_reset`
  0x00478a70, `List_reset` 0x00478a20, `addDropdownSpot` 0x00478a90 ("Maximum drop down
  spots exceeded!"), `Win_fillRect` 0x00478af0, `setFocusItem` 0x0047ae80 (focus pointer
  `g_focusedItem` 0x0083ab98, toggled via item vtable +0xf0).
- View tree / auto-layout: `View_ctor` 0x00479c40 (vtables 0x4ba8b4/0x4baa10),
  `View_attach` 0x0047abe0, `View_dtor` 0x0047a3c0, recursive `layoutNode` 0x00479f30,
  `layoutViewTree` 0x0047a4c0 (aborts on (string in the binary) / MAX_PARENTS).
- Dialogs/screens: `showPopup` 0x0046d6e0 (modal notice + input pump, 11 callers),
  `drawScoreSummary` 0x00473470 (Total Score / Fun / Skill / Cash / Top-10 trophies),
  `showBulletinBoard` 0x0046e810 (accomplishments + snapshots), `courseActionMenu`
  0x0046f550 (save-game, land purchase, scenic placement), the scroll-text window
  (`scrollTextWindow` 0x0046de70 -> `runScrollText` 0x0046dea0 -> `drawScrollText`
  0x0046e260), and `drawMatchScoreboard` 0x00461110.

## 5. Shared math / terrain helpers (util / terrain)

`clamp` 0x00467130 (44 callers), `sign` 0x00467150, `approxDistance` 0x00467170
(19 callers, octagonal metric), `direction8` 0x004671a0 (8-way octant), `angleFixed`
0x004672d0 (16.16 atan2), `foldRange` 0x00467270, `absDiff` 0x004672b0, `seedFromTime`
0x00467110, `genNoise` 0x004673e0. Terrain: `spreadShade` 0x004615f0 /
`spreadAllShade` 0x004616f0 write the shade maps `g_tileShadeA` 0x00822da0 /
`g_tileShadeB` 0x0082376c; `bilinearSample` 0x004674c0 reads the grid `g_sampleGrid`
0x00838c1c; `clearTile` 0x00470a10; `applyTerrainEdit` 0x00470a60.

## 6. Game-state / economy entry points

`startNewCourse` 0x0046ddd0 resets a fresh game (cash 1000, date 0x2c00, flags
0x4000000) and loads the starting pro. `seedCourseTable` 0x0046f2b0 seeds the 12-entry
table `g_courseTable` 0x00571fd8 (stride 0x2e). `tierPrice` 0x0046f1d0, `showCourseRanking`
0x0045f870 (Top-10), `addScore` 0x004732d0, `announceHoleType` 0x00460df0 (fired by the
hole rater 0x0042dea0 via `postEvent` 0x0046e7b0), `buildTournamentPrep` 0x0046d200.
Frontend narration: `buildScenarioIntro` 0x0045fd80, `showTutorialText` 0x004604f0.

## Open questions
- `g_golferRecords` (0x80) vs `g_golferRoster` (0x4c): the two strides are distinct
  record layouts; the exact field maps are not worked out here (see U-0003/U-0004).
- `courseActionMenu` 0x0046f550 multiplexes save/purchase/scenic paths; the dispatch key
  was not traced.
- `g_courseTable` 0x00571fd8 is seeded with course-like names but its consumer in this
  range is only the seeder; confirm whether it feeds `showCourseRanking`.

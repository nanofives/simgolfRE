# exe_6 systems — golf_clean.exe (round-2 leftovers)

Round 2 over the 115 functions exe_0..exe_5 left unnamed. 105 are named in
`re/names/exe_6.tsv`; 10 remain unnamed (see `log/naming/exe_6_notes.txt`). All
addresses are VAs (image base 0x00400000). Facts are from the Ghidra decompilation
(`log/show.py` / `re/tools/decomp.py`) and the 100%-match sources; offsets and globals
are cited inline.

## 1. Widget 2D drawing primitives (render)

A toolkit object (the Widget/Window class in exe_2/exe_3 naming) holds its drawing surface at
`this+4` and forwards 2D primitives through that surface's vtable. The primitives named
here are leaf helpers of the panel/tooltip/scrollbar/list draw code:

- `Widget_fillRectR` 0x00475b00 — fill a rect given as int[4] (surface vtbl+0x34).
- `Widget_clearArea` 0x00478b30 / `Widget_fillArea` 0x00478b50 — fill via vtbl+0x44.
- `Widget_fillRect` 0x00478b80 — filled rect (x1,y1,x2,y2,colour) via vtbl+0x64, fill flag.
- `Widget_drawHLine` 0x00478bb0 / `Widget_drawVLine` 0x00478be0 — single lines via vtbl+0x64.
- `Widget_drawHLineDepth` 0x00478c10 / `Widget_drawVLineDepth` 0x00478c70 — depth-dispatched
  (surface descriptor at vtbl+0xe4: 8 -> Image::hline/vline, 0x10 -> drawColorRunA/B).
- `Widget_drawBevelBox` 0x00479560 (+ `...R` 0x004795d0 from a rect) — two-colour bevel
  outline, built from the HLine/VLine helpers (top/left = param_5, bottom/right = param_6).
- `Widget_drawBox` 0x00479610 (+ `...R` 0x00479670 from a rect) — single-colour outline
  built from the depth-dispatched line helpers.
- `drawLineOnSurface` 0x004049a0 and `drawRectScaled` 0x0046e710 — the scroll-text path's
  rectangle drawer (scales coords through scaleX, draws four edges via Widget_fillRect on
  surface PTR 0x004c1570).
- `Widget_applyPalette` 0x004789f0 — forward palette object field +4 to surface vtbl+0xec.
- `Surface_copyFrom` 0x004744c0 — deep-copy a Surface (recreate DIB via g_graphicsDevice
  vtbl+0x80, blit source). `Image_releaseAll` 0x00474cb0 — release an Image and its 0x28-entry
  sub-surface array at +0x1b4. `createCache` 0x0049c830 — operator new + Cache482fd0::ctor.

The `Widget_setQuad70/74/78` setters (0x00476340/0x00476370/0x004763a0) each write four ints
at `this+{0x70,0x80,0x90,0xa0}+col` (col = 0/4/8, stride 0x10). They are the per-state style
columns of a text/label sub-widget: Button::setColorHover uses +0x70, setColorPressed +0x74;
TextView uses all three. The exact meaning of the four rows was not established.

## 2. Global graphics objects: Surface/Image ctor/dtor pool (render)

The `Surface` class (vtable 0x004ba2d8; ctor `Surface_ctor` 0x00473ab0 zeroes 9 fields,
`Surface_free` 0x00473ae0 releases DIB/palette/cache via g_graphicsDevice vtbl+0x94) backs a
set of file-scope global objects constructed by `staticCtor_*` thunks at startup and torn down
together by `resetAllPanels` 0x0044cce0 (which calls Surface_free directly on the globals at
0x00821020, 0x00821040, 0x008210c8 .. and several arrays at 0x00820f40/0x00821468 stride 0x2c).

The ctor/dtor bodies for these globals are named `gSurfaceCtor_*` / `gSurfaceDtor_*`
(0x0044ae00..0x0044b8d0, plus 0x00474a90). Array variants use the ??_L/??_M vector ctor/dtor
iterators (0x004a55d2 / 0x004a5713). Two global `Image` objects are built by
`gImageCtor_838f98` 0x0046de40, `gImageCtor_83a500` 0x004799f0, `gImageCtor_83a7b8` 0x00479a40
(via Image_ctor 0x00474ae0). The matching `atexitThunk_*` functions register the destructors.
The concrete role of each global (which panel/sprite sheet) was not pinned down.

Deleting destructors in this area: `Image_deleteDtor` 0x00474c20, `View_deleteDtor` 0x00479f10,
`Cursor_deleteDtor` 0x0045c000, `editBoxTimer_dtor`/`_deleteDtor` 0x0043cb50/0x0043cbd0 (a
composite owning an EditBox + Timer), `screenObj_deleteDtor` 0x0049bf70 (a Screen subclass,
vtable 0x004ba278, multiple-inheritance base at this+0x5c).

## 3. Text layout and markup (ui)

- `measureTextWidth` 0x00483930 — pixel width of a substring via the surface font vtbl+0x10.
- `wrapTextToWidth` 0x00483980 — greedy word wrap (splits on spaces with memchr, measures each
  word, breaks when the width budget goes negative). Reads the running width at 0x00839aa8.
- `scanMarkupText` 0x00476d40 — scan to the next markup delimiter ({ } [ ] $).
- `findMarkupTokenIndex` 0x004a4ad0 — look a token id up in the table at this+0x580 (stride 6).
- TextView run state: `TextView_beginMeasure` 0x004785e0, `TextView_endSegment` 0x00478700,
  `TextView_beginDrawRun` 0x00478750 (shared globals 0x00839a9c/aa0/aa4/aa8 = pen x, colour,
  accumulated width). `getFieldValueByKind` 0x004a1370 returns a widget's value by its kind at
  +0x1f4. `TextView_layoutRun` 0x00493670 is a TextView::layout overload. `comboListQuery`
  0x00493ed0 queries the combo list object at this+0x23b4 (vtbl+0xd8). `drawMenuList` 0x004763d0
  iterates a list (listIter 0x00402130/0x00402160) and calls a per-item callback.

## 4. Widget/view tree and input (ui / input)

- `ChildArray_grow` 0x00479b40 — grow the child-pointer array at this+0x224 by 10.
- `View_unlinkFromParent` 0x0047ada0 — remove a view from the parent child list (parent+0x138/
  0x13c, count +0x140). `View_toLocal` 0x0047ab50 — map a point into a view's local coords.
- `releaseCursorClip` 0x00479b20 — ClipCursor(NULL) when the detaching window owns the clip
  (g_cursorClipOwner 0x0083aa98). `isPrimaryMouseDown` 0x0047abc0 — GetAsyncKeyState(VK_LBUTTON)
  XOR SM_SWAPBUTTON. `EditBox_setCursorChar` 0x0047ab00 — store a special-key code and redraw.
- `Timer_fire` 0x00495cb0 — timer dispatch (callbacks at +0x10/+0x14). `redrawRegion` 0x00495770
  — invalidation callback (redrawDrawList + drawTooltip). `listSelectionChanged` 0x0049e640 —
  list selection handler (stores selected id into context 0x0083ab2c, invokes callback).
- `resetScrollText` 0x00465560 — clear scroll-text globals 0x00822d68/0x00822b9c/0x00822b98.
- `drawAmenityRow` 0x00432200 — draw a row of amenity sprites with cursor highlight.
- `showThemePackSelect` 0x004725b0 — the theme-pack selection screen (strings at 0x004e40e0+).

## 5. Buf dynamic-buffer container (util)

A small growable buffer class (vtable 0x004ba84c; fields +4 byte, +8 heap pointer, +0xc/+0x10/
+0x14 counts) used by KeyTable/ListModel/TextView/Dialog:

- `Buf_init` 0x00474780 (clear fields), `Buf_ctor` 0x004747a0, `Buf_releaseData` 0x004747e0
  (free +8, re-clear), `Buf_dtor` 0x00474810, `Buf_deleteDtor` 0x004747c0,
  `pairArray_deleteDtor` 0x0049d070. `listEraseRange` 0x0044ac00 and `readListNode` 0x004a4ea0
  and the list iterators `listIterNext`/`listIterValue` are generic list helpers. `boundIndex`
  0x00463170 returns an index or -1.

## 6. Sim / course / golfer (sim, course, golfer, ai)

Tile grid globals (all stride 0x32 tiles/row): `g_tile_type`, `g_tileFlags`, `g_placed_objects`,
plus the type-attribute table at 0x00578374 (stride 0x30) and 0x004c26c0 (stride 0x14).

- Record banks: `resetRecordBank` 0x00401000 (g_recordBanks 0x004e6d20 + g_recordOccupancy,
  0x74 bytes/bank), `freeRecordAtPos` 0x004011b0 (table 0x0056d1e0 stride 0x3c).
- Placement: `evalPlacementArea` 0x0040db90 (validate a build block and sum its terrain cost),
  `clearObjectFootprint` 0x0040e400 (remove a placed object: refund via rateLot/pointsPopup,
  free tiles, rebuildHeightfield), `rebuildTileCoverage` 0x0042f340 (recompute g_tileFlags bit
  0x40 coverage from holes via floodMark).
- Golfer stories: `g_storyPairs` 0x0059fc60 (10 x 0x388). `addGolferPair` 0x004099f0 registers a
  golfer+partner pair; `updatePairSnapshot` 0x00409950 refreshes its cached stats and event
  counter. `findNearestTargetTile` 0x0040de70 picks the nearest tile with g_tileFlags bit 0x200
  (writes g_nearTileDist/X/Y 0x00568d0c/0x0056a91c/0x0056a920). `computeRatingTrend` 0x004060a0
  is a weighted stat delta used by matchUpdate. `updateRollingStats` 0x00409bf0 decays a per-entry
  table at 0x005736bc (stride 0x24) by g_date.
- Shot simulation: `trialShot` 0x004226a0 (snapshot a golfer, run simulateShot, restore -> AI
  shot evaluation) and `simulateBallFlight` 0x00422fb0 (step the ball through the grid using the
  ball-state globals 0x00579584-0x00579598, increment stroke counter 0x005794da).
- Per-frame render: `screenToTile` 0x00430020 (inverse isometric projection with 8-neighbour
  refinement, camera g_cameraRot/gCameraTileX/Y), `drawWorldObjects` 0x00430360 (project, enqueue
  and draw world/golfer sprites, thought bubbles, ambient habitat sounds), `drawMinimapSegment`
  0x00406220. FLC playback: `flicOpenStream` 0x00404120, `flicNextFrame` 0x00404090
  (per-flic records 0x004f6620 stride 0x84, streams 0x0050fc20 stride 0x44).

## Open questions

- The four (string in the binary) of the `Widget_setQuad70/74/78` style table (this+0x70..0xa0) and which global
  Surface/Image each `gSurfaceCtor_*`/`gImageCtor_*` constructs were not resolved.
- `pushTripleEntry` 0x00409cb0 has no caller in the image; its three arrays (0x00586b50,
  0x00586fa8, 0x005a8834, counter 0x005a9cd4) have no reader identified in this set.
- `simulateBallFlight`/`drawWorldObjects` are large (0x1157 / 0x1244 bytes); only their dominant
  behaviour is documented, not every branch.

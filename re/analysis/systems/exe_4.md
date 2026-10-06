# exe_4 systems (golf_clean.exe 0x004853b0 - 0x004935b0)

This range is the engine's **UI toolkit** (a Widget-based control library), plus the supporting
2D image/surface code, a multimedia timer, the Bink video player, a text/config file reader, and
low-level string/path/polygon helpers. 211 functions, all named.

Base facts verified from the decompilation:
- `g_graphicsDevice` = global `DAT_0083ad50`, the graphics DLL object (jgld export at RVA 0x1780) obtained by `engineInit`
  (0x004855b0). Called through its vtable: `+0x20` returns the main HWND, `+0xa8` returns the screen
  width, `+0x80` creates a surface, `+0x150` forces a redraw.
- `Widget` is the control base class (ctor `0x004804a0`, create `0x004806c0`, invalidate at vtable
  `+0x120`; `setQuad` = `0x00476310`, confirmed by its exported-style name). Controls store a font/
  text-measure object at `this+0x274` and a parent/owner pointer at `this+0x130`.
- Surfaces are accessed through a vtable: `+0x14` lock bits at (x,y), `+0x18` bits pointer, `+0x24`
  unlock, `+0xcc` clip rect {top, ?, left@8, right@0xc}, `+0xd8` width, `+0xdc` height, `+0xe0`
  stride, `+0xe4` pixel format, `+0xe8` palette. Pixels are 16-bit in the draw paths here.

## Engine lifecycle (boot)
- `engineInit` 0x004855b0 creates `g_graphicsDevice` and, per the `param_3` subsystem bitmask, brings up each
  subsystem (graphics 0x8000, sound 0x2 -> 0x00483320, input, message box 0x100 -> 0x00490bf0,
  sine table 0x4000 -> 0x00491c10, cursor metrics 0x800 -> 0x004884e0, timer lock 0x1000). Writes
  `g_engineFlags` (0x0083afd0).
- `engineShutdown` 0x00485740 tears everything down in order (blitter 0x00492470, timers 0x00486ff0,
  message box 0x00490c30, sound, resources).

## 2D image / surface (render)
`Image` wraps a surface at `this+4` and owns a palette at `this+0xac`.
- `Image::loadPcx` 0x00485790 / `Image::savePcx` 0x00485aa0 are a PCX codec (0x80-byte header,
  signature byte 0x0a, 8-bit RLE, 256-colour palette after marker 0x0c).
- `Image::hline` 0x00485d40 / `Image::vline` 0x00485e80 draw clipped 16-bit lines.
- `cropSprite` 0x00492000 trims a sprite's transparent (0xff) border into a fresh surface.
- Convex-polygon scanline fill: `polyEdgeStep` 0x00492ed0 / `polyEdgeAdvance` 0x00492fa0 run the edge
  DDA over the vertex table `g_polyVerts` (0x0083d358); `fillSpanDown/Up` 0x00493000/0x00493080 write
  runs of `g_polyFillColor` (0x0083d34c); `fillConvexPoly8/16` 0x00493100/0x004932d0 and the depth
  dispatcher `fillConvexPoly` 0x00493520.
- The `Screen` class (vtable 0x004badf8) at the top of the range owns a device at field [0x10]:
  dtor 0x004853d0, close 0x004854c0.
- Global blit state: `g_blitError` 0x0083c310, `g_blitContext` 0x0083c328, `g_blitSurface` 0x0083c330
  (`shutdownBlitter` 0x00492470, `flushBlitSurface` 0x004924b0, `blitError` 0x00492460).

## Multimedia timer (util)
`Timer` (vtable 0x004baea0, 0x2c bytes): callback [0x10], context [0x1c], interval [0x20], resolution
[0x28], id [0xc]. ctor 0x00486c90, init family 0x00486cf0/0x00486d20/0x00486d60/0x00486d90, arm
`startOnce` 0x00486dc0 / `startRepeat` 0x00486e40 (timeSetEvent for < 50 ms else SetTimer on the app
window), `stop` 0x00486ec0 (timeKillEvent/KillTimer), `reset` 0x00486f10, `callback` 0x00486f90 (posts
message 0x401). Reentrancy guard `g_timerLock` 0x0083afec.

## Bink video player (video)
`BinkPlayer` (vtable 0x004baea8): Bink handle [4], buffer [8], target DIB [0xc]. ctor 0x00487000,
`setTarget` 0x00487050, `close` 0x00487060, `play` 0x00487090 (BinkOpen + BinkBufferOpen + decode/
present loop, hides cursor), `frame` 0x00487180 (BinkDoFrame + BinkCopyToBuffer + BinkBufferBlit).

## Text / config file reader (util)
`Reader` (vtable 0x004bb084): FILE* [0x158], current line [0x154], two line buffers [0x15c]/[0x160].
ctor 0x00487ea0, `open` 0x00487fb0 (forces a .txt extension, seeks to a named `[section]`, logs
(string in the binary)), `readLine` 0x00488230 (fgets + strip newline + trim). A global reader lives at
`g_sectionReader` 0x0083b000 (helpers readSectionGlobal 0x00487e70, readLineGlobal 0x00487e90).
`KeyTable` (vtable 0x004bb088): a filename + key/value map; `find` 0x00488310 matches a file and key,
`lookup` 0x00488420 scans the 16-entry global array `g_keyTables` 0x0083b170. `matchDirective`
0x0048cd80 matches a line's leading token against the directive keyword table at 0x004e4584.
String helpers: `trimLeadingSpace` 0x004924e0, `trimTrailingSpace` 0x00492570, `trimSpaces`
0x004925b0, `stripNewline` 0x004925d0, `appendNewline` 0x004925f0, `reverseFindChar` 0x004935b0,
`swapInts` 0x00493580. `resolveDataPath` 0x00491da0 probes a filename against the data search
directories with FindFirstFileA, returning a path in scratch buffer `g_pathScratch` 0x0083c004.
`MappedFile` (vtable 0x004bba78) 0x00492d80/0x00492dd0/0x00492e80 is a read-only CreateFileMapping
wrapper.

## Generic containers (util)
A growable node list (nodes 0x1c bytes; ctor 0x00487280, clear 0x00487310, dtor 0x00487390) with two
further vtable variants (0x004bafb0 at 0x00487a20, 0x004bb014 at 0x00487b40). A 16-bucket hash table:
`remove` 0x004876c0, `bucketEmpty` 0x00487770, `bucketSet` 0x004877a0. A resource wrapper with three
registered close hooks `g_resCloseHook1/2/3` (0x0083af80/88/90): closeHookA/B/C
0x00487430/0x004879f0/0x00487bd0, resourceLoad 0x00487c00, resourceLoadB 0x00487a70.

## UI controls (ui)
All derive from `Widget`. Common pattern: ctor builds the base and any embedded Timers, `create`
calls `Widget::create`, state changes invalidate via vtable `+0x120`.
- `EditBox` (vtable 0x004ba4a8): single/multi-line text input. Text buffer [0x574], capacity [0x578],
  caret/length [0x5a4], focus [0x5a8], caret Timer [0x16b]. ctor 0x00486070, create 0x00486110,
  setText 0x00486200, resizeBuffer 0x00486250, wrapToLines 0x004862b0, hit-testing 0x00486360/
  0x004863e0, insertChar 0x004866f0 (beeps when full), setCaretActive 0x00486b30.
- `Cursor` (vtable 0x004ba7f4): ctor 0x00488490, destroy 0x004884b0; metrics cache 0x004884e0.
- `Button` (vtable 0x004bb0a8): command id [0x5e8], label [0x5f4], tooltip [0x5f8], hover state
  [0x614], press/tooltip Timers [0x15f]/[0x16d]. ctor 0x00488500, create 0x004887c0, colour setters
  0x00488930/0x00488970/0x004889b0, setTooltip 0x00488a20, onMouseMove 0x00488cf0, onTimer 0x00488fd0,
  setMode 0x004890e0. Defaults in `g_defaultButtonFont` 0x0083b60c (setButtonDefaults 0x004889f0).
- `ListModel` (vtable 0x004bb21c): linked list of items (id at +4, data at +8, next at +0xc). ctor
  0x00489150, clear 0x004894b0, append 0x00489890, indexOfId 0x004898d0, selectedId 0x00489950,
  findId 0x004899d0, idAt 0x00489a30.
- `ListBox` (vtable 0x004bb3cc): composite control = embedded Widget window [0x5c] + ListModel
  [0x5d4]. ctor 0x00489cb0, setSelection 0x00489f50 (scrolls into view, fires callbacks), measure
  0x0048a120, build 0x0048a5d0, onNavKey 0x0048aa00, typeAhead 0x0048aea0, plus the multiple-
  inheritance sub-object handlers 0x0048c420-0x0048caf0 (scroll/select/click/addItem/mouse/drag/
  clearSelection/scrollBy).
- `TextView` (vtable 0x004bb3f8, 0x2114 bytes): scrollable formatted-text view with span lists
  ([0x13e0] images, [0x1410] text), alignment mode [0x1654], flags [0x1f68], a HotList, OK/Cancel
  buttons and a scrollbar. ctor 0x0048ce00, reset 0x0048d480, init 0x0048db60, addText 0x0048df20
  (decodes `^` escape codes), addImage 0x0048dfc0, setName 0x0048e010, setLineSpacing/setFontSize
  (hi-res *3/2) 0x0048e0b0/0x0048e120, setSize 0x0048e190, layout 0x0048e1c0, build 0x0048e900,
  render 0x0048fe60, select 0x00490960. Factory create 0x00491310. A composite `Dialog` (vtable
  0x004bb3f8 shared base) destructor chain lives at 0x004914d0-0x00491710.
- `MsgBox` globals: two labels `g_msgBoxLabelA/B` 0x0083b994/0x0083b998 (setLabelA/B 0x0048de00/
  0x0048de90, (string in the binary) is the default label B), two button descriptors and colour quads
  (0x00490c80/0x00490cc0/0x00490cf0/0x00490d20), initDefaults 0x00490bf0, freeLabels 0x00490c30.
- `HotList` (vtable 0x004bba74): a list of tooltip hotspot rects (0x20 bytes each) with a delay
  Timer [8] (default 1000 ms). ctor 0x00492850, alloc/grow 0x00492920/0x00492690, add 0x004929b0,
  hitTest 0x00492a90/0x00492b10, showTip 0x00492bd0, onMouseMove 0x00492cc0.

## Math / geometry helpers (util)
`initSinTable` 0x00491c10 builds the fixed-point sine table `g_sinTable` 0x0083b9f4 with fsin;
`sinScaled` 0x00491c70 / `cosScaled` 0x00491d80 interpolate it. `pointInRect` 0x00492610 tests a
point against a {left,top,right,bottom} rect.

## Open questions
- The exact class identity of the resource wrapper behind closeHookA/B/C (0x00487430 etc) and its
  three global close hooks is not pinned down; only the literal per-field behaviour is documented.
- `Screen`/`Image` are named from their shared `this` layouts (device at +0x10, surface at +4); the
  original class names are not recoverable from this range alone.

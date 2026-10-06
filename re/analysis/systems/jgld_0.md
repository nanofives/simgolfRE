# jgld.dll systems — jgld_0 (0x10002650 – 0x100165a0)

Reverse-engineering notes for the first half of `jgld.dll`, the Jackal 2D graphics library
(debug build, `C:\JackDev\Debug\jgld.pdb`). This range holds the library's support classes
(memory-mapped file I/O, 3D math, intrusive list, RNG) and the first half of the drawing layer
(`Surface` and the start of `Sprite`). The second half (`Sprite` draw/blit families, `Display`,
`DllMain`, factory) is `jgld_1`.

Evidence comes from the 100%-instruction matches in `re/match/jgld_*.cpp` (byte identity, so the
class/field layout is exact), the Ghidra decompilation (`log/show.py`), and xref data
(`re/tools/xref.py`). 212 of the 276 functions in this range are named in `re/names/jgld_0.tsv`.

## MappedFile — memory-mapped file I/O (util)

A thin RAII wrapper over `CreateFileA` + `CreateFileMappingA` + `MapViewOfFile`. vtable `0x1011d01c`.

Layout (from the ctor 0x10002650 and close 0x10002e10):
- +0x00 vtable
- +0x04 mapped view pointer (0 when closed)
- +0x08 file handle (`0xffffffff` when closed)
- +0x0c file-mapping handle (0 when closed)

Entry points: `openRead` (0x10002870), `openWrite` (0x10002a20), `create` (0x10002bd0, creates a file
of a given size). `close` (0x10002e10) unmaps and closes all three handles; `closeTruncate`
(0x10002ef0) truncates the file to a final size with `SetFilePointer`/`SetEndOfFile` before closing.
Used by `Surface::writePcx8` (0x10012140) to write a .pcx screenshot.

## 3D math — Vector3 / Quat / Matrix / Transform (util)

A small 3D math library (the Jackal engine descends from Alpha Centauri). All are value types stored
inline; `Matrix` and `Transform` additionally carry a vtable (`0x1011d024`, `0x1011d028`).

- **Vector3**: three floats at +0/+4/+8. Full operator set: add/sub (both in-place and to a result
  pointer), scale, negate, dot (0x10003c90), cross (0x10003e50), distance / distance2 (0x10003b30 /
  0x10003bf0), normalize (0x10003cf0), midpoint (0x10003f80, uses 0.5f at 0x1011d04c), rotate by a
  quaternion (0x10004100).
- **Quat**: four floats at +0/+4/+8 (x,y,z) and +0xc (w). Identity ctor (0x10003050) sets w=1.0.
  `setAxisAngle` (0x10004230), multiply / multiply-assign (0x10004410 / 0x100042f0), conjugate
  (0x10004530).
- **Matrix**: vtable + 16 floats at +4..+0x43 (row-major; translation row at +0x34/+0x38/+0x3c,
  diagonal at +4/+0x18/+0x2c/+0x40). `loadIdentity` (0x100045f0), `setRotation` from a Quat
  (0x10004750), `setTranslation` (0x100046f0), add / multiply / scale / transform-point
  (0x100048e0 / 0x10005030 / 0x10004f40 / 0x10004e40). Several ctors take a Quat and/or Vector3.
- **Transform**: vtable + Quat rotation at +4 + Vector3 translation at +0x14 + a flags word at +0x20
  (bit 2 enables translation scaling, see `scaleTranslation` 0x10006270). `getMatrix` (0x10005690)
  bakes it into a Matrix. `invert` (0x100062e0) conjugates the rotation and negates the translation.
  `rotateX/Y/Z` (0x10006420/0x10006520/0x10006620), compose (operator* 0x10005dd0).
  0x10005990 and 0x10005c70 (translate / move) are byte-identical, folded by the linker.

## Intrusive list — Tracked / LinkedList / ListNode (util)

`re/match/jgld_list.cpp`. `Tracked` (ctor 0x10006ab0) is a base that links every instance into a
global list; `Tracked::deleteAll` (0x10006bf0) frees them all (leak tracking in the debug build).
Both `SurfaceBase` (0x10009690) and `SpriteBase` (0x10014c70) derive from `Tracked`.
`LinkedList` (ctor 0x10006c80) is a node list with `add` (0x10006f40), `removeCurrent` (0x10007100),
`find` (0x100072b0), `count` (0x10006d40), `clear` (0x10006df0); nodes are `ListNode` (0x10007370).
A global `LinkedList` at `0x10128428` is torn down once (guarded by flag `0x10128444`) by
`atexitDestroyGlobalList` (0x10006a40).

## Random — LCG PRNG (util)

`re/match/jgld_random.cpp`. A seed at +0. `next` (0x10007530) is a classic LCG
(`seed = seed*0x41c64e6d + 0x3039`) returning `(seed>>16 & 0x7fff)/32768.0` in [0,1) (divisor at
0x1011d0a0). `range` (0x100075b0) scales that into [0,n). ctor (0x10007620) seeds 0.

## Surface — the 2D drawing target (render)

The core of the graphics layer. `Surface` (vtable `0x1011d0b0`) derives from `SurfaceBase`
(vtable `0x1011d220`, itself `Tracked`). It is a large object (fields past +0x810). Known fields:
- +0x24 pixel depth (8 or 16), read directly and also via the virtual at vtbl+0xe4.
- +0x44 / +0x54 embedded RECTs (bounds / clip; `getBounds` 0x1000a9d0, `getClip` 0x1000a930).
- +0x4b8/+0x4c0/+0x4cc cached GDI DCs with use-counts at +0x4c4/+0x4c8 (`acquireDC`/`releaseDC`
  0x10008640/0x100086b0 and the secondary pair 0x10008730/0x100087b0).
- +0x4d0 an auxiliary pointer (set 0x1000a5d0 / get 0x1000a620).

**Depth dispatch.** The public drawing methods are thin dispatchers that read the surface depth
(vtbl+0xe4) and branch to an 8-bit or 16-bit implementation:
- `drawLine` 0x100094e0 → `line8` 0x1000fb70 / `line16` 0x1000b140
- `drawDashedLine` 0x100095a0 → `dashedLine8` 0x10010870 / `dashedLine16` 0x1000c140
  (`dashedLine16` splits into `dashedHLine16` 0x1000cf90 and `dashedVLine16` 0x1000d4f0)
- `fillRectDispatch` 0x10009770 → `fillRectC8` 0x10011dd0 / `fillRect16` 0x1000dcf0
- `ditherRectDispatch` 0x10009c50 → `ditherRectC8` 0x10011f80 / `ditherRect16` 0x1000e580
- `blendRectDispatch` 0x10009ac0 → `blendRect16` 0x1000dfc0 (16-bit only)
- `ditherBlendRectDispatch` 0x10009e40 → `ditherBlendRect16` 0x1000e8b0 (16-bit only)
- `blitT` 0x100091a0 (transparent blit) → `blitT_8to8` 0x10013c50 / `blitT_8to16` 0x10012db0 /
  `blitT_16to16` 0x1000ed70 (branches on both source +0x24 and destination depth)
- `saveImage` 0x1000a550 → `writePcx8` 0x10012140 (8-bit RLE .pcx writer via MappedFile)

Lower-level primitives: `fill8`/`fill16` (0x1000f880/0x1000afe0), `clear8`/`clear16`
(0x10011c40/0x1000da70), `hline`/`vline` 8-bit (0x100103f0/0x10010620) and 16-bit
(0x1000ba90/0x1000bde0), `blit`/`stretchTo` (0x10008e50/0x10008f70), `blit8from8`/`blit16from16`
(0x10013800/0x1000f6a0), `blit16`/`blitKey8to16` (0x100098f0/0x10012900), `pixelAddr` (0x10008830),
`setPalette` (0x100089d0), `drawTo`/`drawToZ` (0x10009fd0/0x1000a330).

**GDI text.** Text is drawn by acquiring a GDI DC (vtbl+0x28/+0x2c bracket the operation):
`selectSystemFont` 0x10008b90, `selectObject` 0x10008ae0, `setTextColorRGB`/`setTextColor16`
0x10008c40/0x10008d00, `textOut` 0x10008da0.

**Color + RECT helpers** (`re/match/jgld_color.cpp`): `makeColor` 0x1000a660 packs r,g,b into a
16-bit pixel, `redOf`/`greenOf`/`blueOf` 0x1000e440/0x1000e4c0/0x1000e540 unpack it, `fillWords`
0x1000af30 is a word memset. `intersect`/`equal` (0x10008590/0x100085f0) and `rectFromXYWH`/
`setRect4`/`rectWidth`/`rectHeight` (0x10008360/0x10010390/0x10009120/0x10009160) are free helpers.

**Virtual-table default stubs.** Between the named Surface methods sit ~40 tiny functions (43 bytes
each) that `return 0x18`, `return 0` or `return;`, plus single-field accessors. These are the base
class's default virtual-slot implementations (0x18 is the library (string in the binary) code,
cf. `LIBERR_NOT_THIS_DEPTH` in Sprite). They are byte-matched but not individually named; see
`log/naming/jgld_0_notes.txt`.

## Sprite — start of the sprite class (render)

`Sprite` (vtable `0x1011d380`) derives from `SpriteBase` (vtable `0x1011d444`, itself `Tracked`) and
owns a `CRITICAL_SECTION` at +0x38. `create` (0x10014ff0) dispatches on depth to `create8`
(0x1001b690, in jgld_1) or `create16` (0x1001a250), `release` (0x10014eb0) frees pixel data, `remap`
(0x100150d0) remaps palette indices. The draw family begins here — `draw` 0x10015180 and variants
`drawB`..`drawG` (0x10015480–0x100165a0) — and continues through all of jgld_1.
The debug asserts reference `c:\jackdev\jglsprite.cpp`.

`allocDrawBuffer` (0x10007e40) allocates the global draw buffer and, on failure, prints a fatal
error message (strings 0x1011d1d0 / 0x1011d210) and calls `_exit`.
`loadPng` (0x100145a0) decodes a PNG via statically-linked libpng 1.0.5.

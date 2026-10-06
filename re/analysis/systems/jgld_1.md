# jgld.dll systems — range 0x100168e0 .. 0x1006afc0 (ID jgld_1)

Jackal 2D graphics library (debug build). This range is the upper half of jgld: the Sprite
blitter family, the Display/window layer, the object Factory, the Font (GDI text) layer, and the
Palette layer. 145 of the 277 functions carry a determined name; all names below are backed by a
100% instruction match in `re/match/jgld_*.cpp` unless stated otherwise. Addresses are RVAs on the
jgld.dll image; globals live in jgld's data at 0x1011dxxx (const) and 0x10128xxx (mutable).

## Sprite (render) — jglsprite.cpp, jgld_sprite.cpp, jgld_blit*.cpp, jgld_blitY*.cpp

A Sprite owns a pixel buffer (8-bit or 16-bit) and blits it onto a Surface. The bulk of this range
(89 named functions) is the blitter family:

- Construction / lifetime: `Sprite::create16` (0x1001a250), `Sprite::create8` (0x1001b690) allocate
  the pixel buffer for (w,h); `Sprite::copyFrom` (0x1001a0e0) copies another sprite; `Sprite::remap`
  / `Sprite::remap8` (0x1001bae0) re-index 8-bit pixels through a translation table;
  `SpriteBase::~SpriteBase` (0x100168e0) releases the buffer.
- Lettered draw variants `Sprite::drawH..drawP` (0x100169a0, 0x10016cb0, 0x10016fc0, 0x100172b0,
  0x100178a0, 0x10017b40, 0x10018120, 0x100185b0, 0x100193d0) — each blits the sprite to a Surface at
  (x,y); the letters distinguish clipping/transparency flavours chosen by the matcher (the exact
  per-letter selector is not determined here; the lower-range companions drawA..drawG live below
  0x100168e0, outside this ID).
- Line drawing into the sprite's own buffer: `Sprite::lineTo` (0x10018490), `Sprite::dashTo`
  (0x10018510).
- Block/pair/mask blits: `Sprite::blt16a/blt16b/blt16c` (0x10019830/0x10019900/0x100199d0),
  `Sprite::bltS/bltS2` (0x10019aa0/0x10019b90) blit into another sprite, `Sprite::blitPair` /
  `blitPair1` (0x10025900/0x10025dd0), `Sprite::blitMask` / `blitMaskScaled`
  (0x1002cec0/0x1002d390), `Sprite::drawAlpha` (0x10018920) alpha-blends via a Pal16 table.
- Large machine-generated blit families, all 100%-matched, differing only in their inner `__asm`
  loops (clip/scale/flip/mask combinations; the exact selector per variant is not determined):
  - `draw16_*` (0x1001bc40, 0x1001cde0, 0x10020430 … 0x100576a0): 16-bit blits.
  - `draw16t_*` (0x1001ab40, 0x1001b060): transparent 16-bit.
  - `draw16m_*` (0x1001df80, 0x1001e970): masked 16-bit.
  - `draw8_*` (0x1005fd20, 0x10060cf0, 0x10061db0, 0x10063f60): 8-bit.
  - `draw_*` (0x1001f2d0, 0x1002ab90, 0x1002bcf0, 0x1005dde0, 0x1005ed80, 0x10062dd0): mixed-depth.
  - `drawRle16*` (0x10058970 .. 0x1005d1f0): RLE-compressed 16-bit sprites
    (jglsprite_8_16c.cpp; these functions carry a debug report about compressed sprites).

`getField14/20/28/2c` (0x10017f70/0x10017fe0/0x10017f30/0x10018020, jgld_small1.cpp) are four
thiscall accessors that return the int at the named offset of their object; the matcher grouped them
under a placeholder class "J17" whose real identity is not established, so they are named here as
plain offset accessors rather than as methods of an invented class.

## Display and the window layer (render / ui / input) — jgld_display.cpp, jgld_app.cpp

`createDisplay` (0x10065390) is the singleton constructor: it `operator new`s 0x14c bytes, runs
`Display::Display` (0x100654b0), and stores the pointer in both **g_display** (0x1012870c) and
**g_displayLib** (0x10128420). `Lib::destroyDisplay` (0x10065420) tears it down.

Display-mode management (all render): `enumModes` (0x10065a90, EnumDisplaySettingsA into the mode
tables at 0x101286c8/0x101286f8), `findMode` (0x10065c30), `setMode` (0x10065da0), `pickMode`
(0x10065ea0), `pickModeFreq` (0x10066040), `setDesktopMode` (0x10066200), `setModeIndex` (0x100662a0),
`setResolution` (0x10066890), `restore` (0x10066970). `setModeIndex` enforces a minimum resolution:
when the enumerated width is 640 it raises a MessageBox (title string in the binary) telling the player to use
800x600 or higher.

`Display::createWindow` (0x100657e0, ui) registers the game's window class (string
0x1011d978) and creates the game window; it records the caller's Listener in **g_listener**
(0x1012872c). `Display::present` (0x10066470, render) blits the back-buffer Surface (reached through
this+0x148) to the screen DC with BitBlt rop 0xcc0020, clipped to the dirty rect.
`Display::getCursor` (0x10066780, input) reads the mouse position.

The window procedure is `windowProc` (0x100692b0, ui; not match-covered, named from its callees): it
dispatches messages to `realizePalette` (0x100699c0), `onPaint` (0x10069ac0), `onActivate`
(0x10069b90), `onKey` (0x10069c70), `onChar` (0x10069d50), `onCommand` (0x1006a0e0) and eight
mouse-event handlers (0x10069db0 .. 0x1006a080, undetermined individually). Each handler forwards the
event through the **g_listener** vtable: onPaint +4, onActivate +0x2c, onKey +0xc, onChar +0x10;
`onCommand` instead calls the **g_commandHandler** callback (0x10128728) set by
`Lib::setCommandHandler` (0x10067f10). `onActivate` realizes the palette on activation.

## Factory (render) — jgld_factory.cpp

A thin object factory: `createFont`/`createPalette`/`createSurface`/`createSprite`
(0x10067850/0x10067910/0x100679d0/0x10067a90) allocate and construct the matching object;
`destroyFont`/`destroyPalette`/`destroySurface`/`destroySprite`
(0x10067b50/0x10067bd0/0x10067c50/0x10067cd0) free them.

## Font (render) — jgld_font.cpp

GDI-backed text. A shared memory DC **g_fontDC** (0x10128724) is created by `initFonts` (0x10068f30,
CreateCompatibleDC) and deleted by `initFontTable` (0x10068fa0). `Font::create` (0x10068850) builds a
GDI font with CreateFontIndirectA from a height and a style bitmask (bit0 bold→weight 700, bit1
italic, bit2 underline) and caches its text metrics (stored at font+4: +8 line height, +0xc ascent,
+0x14 ascent, +0x18 descent). `Font::loadFile` (0x10068ae0) loads a (string in the binary) resource; `Font::textWidth`
(0x10068e60) measures a string with GetTextExtentPoint32A on g_fontDC; `Font::release` (0x10068da0)
deletes the GDI font. `Font`/`~Font` (0x10068660/0x10068760) and `FontBase`/`~FontBase`
(0x10069220/0x10069010) are the ctors/dtors.

## Palette (render) — jgld_app.cpp

A Palette holds 256 BGRA entries at +0xc, two 16-bit colour lookup tables (RGB565 at +0x40c, RGB555
at +0x60c), a dirty counter pair (+0x810 current / +0x814 applied), and a timestamp (+0x818).

- `Palette::setRGB` (0x1006a9a0): writes `count` RGB triples into the entries at +0xc, then `update`.
- `Palette::setDefault` (0x1006a600): fills the entries — first 10 and last 10 from the default
  system-colour tables **g_defaultPaletteLo** (0x1011dab0) / **g_defaultPaletteHi** (0x1011dad8), the
  236 middle entries as a greyscale ramp — then `update`.
- `Palette::update` (0x1006a3d0): seeds a Random from QueryPerformanceCounter (+0x818), spins a random
  16-bit value into +0x810, then calls `apply`.
- `Palette::apply` (0x1006a4a0): when +0x810 differs from +0x814, rebuilds the RGB565 (+0x40c) and
  RGB555 (+0x60c) lookup tables from the 256 BGRA entries and copies +0x810 to +0x814.
- `Palette::animate` (0x1006a860): if **g_logPalette** (0x10128738) exists, AnimatePalette entries
  10..0xec from this+0x34.
- `createPalette` (0x1006aa70, free function): builds a 256-entry Win32 LOGPALETTE (version 0x300)
  from the default colour tables and creates the GDI palette; `realizePalette` (0x100699c0) selects
  and realizes g_logPalette into the window DC.

## Containers (util) — jgld_list.cpp

`Array_Rec14::add` (0x100681a0) and `Array_Rec94::add` (0x10068400) append a fixed-size record to a
dynamic array laid out as {buffer +0x4, capacity +0x8, count +0xc, grow-step +0x10}; when full they
`operator new` a larger buffer, memcpy the old contents, and grow capacity by +0x10 entries. Rec14's
record is 0x14 bytes (5 dwords).

## Boot

`DllMain` (0x10065300) is the DLL entry point.

## Globals (see jgld_1_globals.tsv)

g_display (0x1012870c) / g_displayLib (0x10128420), g_listener (0x1012872c), g_commandHandler
(0x10128728), g_fontDC (0x10128724), g_logPalette (0x10128738), g_defaultPaletteLo (0x1011dab0),
g_defaultPaletteHi (0x1011dad8). The back-buffer Surface pointer is reached through the Display at
this+0x148 and the global at 0x1012873c (read by present); its writer was not traced in this range.

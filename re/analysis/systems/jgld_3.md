# jgld_3 systems — jgld.dll 0x10066810 .. 0x1006afc0

Round 2 of jgld.dll naming. The 98 functions in this range are the vtable thunks, base-class
defaults, field accessors and small helpers of four graphics-library objects that rounds 0/1 left
unnamed (they are all 100% instruction matches in `re/match/jgld_raw_03/04/08/09/10.cpp`, so the
struct layouts below are exact). Vtables were read from `original/jgld.dll` with pefile, following
each slot's incremental-link `jmp` thunk to the real body; slot names come from the overriding
implementation in the sibling (derived) class.

All addresses are RVAs (image base 0x10000000).

## Display / DisplayBase (the jgld graphics device)

The concrete `Display` class (`class Display : public DisplayBase`, confirmed by the matched
`??0Display@@` / `?createWindow@Display@@` symbols in `re/match/jgld_display.cpp`) owns the window,
the saved display mode and the GDI device context. Its vtable has 86 slots.

- `Display_vtable` = 0x1011d640 (derived; installed by the ctor path 0x10065670).
- `DisplayBase_vtable` = 0x1011d7dc (base; installed by DisplayBase::dtor 0x10066d20 / ctor 0x10065630).
  Unoverridden slots point at `0x1007f360` (the pure-virtual filler) or at the shared default stubs
  below; overridden slots in the derived vtable name the method.

Object layout (this = Display*):
- +0x04  DisplayMember m_mode (DEVMODEA m_cur; DEVMODEA m_orig), used by ChangeDisplaySettingsA.
- +0x12c m_width   (int; written by setModeIndex from DEVMODEA.dmPelsWidth) — getter 0x10067760.
- +0x130 m_height  (int; DEVMODEA.dmPelsHeight)                             — getter 0x100677a0.
- +0x134 m_bpp     (int; DEVMODEA.dmBitsPerPel)                             — getter 0x100677e0.
- +0x138 m_dc      (HDC; GetDC in createWindow).
- +0x140 m_hinst   (HINSTANCE; used by UnregisterClassA)                    — getter 0x100675a0.
- +0x144 m_hwnd    (HWND)                                                   — getter 0x10067560 (DisplayBase slot 8 getHwnd).
- +0x148 m_mode    (Mode*).

Concrete Display virtuals named here: setWindowPos 0x10066810 (SetWindowPos), showWindow 0x100675e0
(SW_SHOWNORMAL), hideWindow 0x10067640 (SW_HIDE), minimizeWindow 0x100676a0 (SW_MINIMIZE),
maximizeWindow 0x10067700 (SW_MAXIMIZE), getWidth/getHeight/getBpp/getHwnd/getInstance accessors,
setScale 0x10067d50 / getScale 0x10067da0 (the blit scale globals, below), and the four
subsystem-init wrappers at slots 46-49: initFonts 0x10067e00, shutdownFonts 0x10067e40,
createPalette 0x10067e80, freePalette 0x10067ed0.

DisplayBase defaults named here: deletingDtor 0x100674f0, dtor 0x10066d20, getHwnd 0x10066d60 (returns
0), getInstance 0x10066d90 (returns 0), setWindowPos 0x10066dc0 (no-op), setCommandHandler 0x100674c0
(no-op). The remaining ~28 base stubs (slots 16-19, 33, 38 and 50-71) return the library
not-this-depth status 0x18, or 0, or void, and are never overridden by Display, so no method name is
evidenced for them (listed in the notes file).

### Blit scale globals
`Display::setScale` 0x10067d50 writes `g_blitScaleX`/`g_blitScaleY` (0x10122dc0/dc4, signed scale
numerators) and `g_blitScaleDen` (0x10122dc8). The sprite blitters in `re/match/jgld_blit*.cpp` read
these to compute per-pixel step. `Display::getScale` 0x10067da0 reads them back.

### Window message forwarders (input)
`windowProc` 0x100692b0 translates Win32 messages into calls on the registered `g_listener`
(0x1012872c, named in round 1) through its vtable. The eight free forwarders here each handle one
message family and were mapped from windowProc's switch:
- onWmSize 0x10069db0 (WM_SIZE 0x05 -> listener slot 0, width/height)
- onWmNcHitTest 0x10069e10 (WM_NCHITTEST 0x84 -> slot 2)
- onWmMouseWheel 0x10069e70 (WM_MOUSEWHEEL 0x20a -> slot 6, HIWORD(wParam) delta)
- onWmMouseMove 0x10069ee0 (WM_MOUSEMOVE 0x200 -> slot 5)
- onWmLButtonDown 0x10069f40 (WM_LBUTTONDOWN 0x201 / DBLCLK 0x203 -> slot 7, flag 0/1)
- onWmLButtonUp 0x10069fb0 (WM_LBUTTONUP 0x202 -> slot 8)
- onWmRButtonDown 0x1006a010 (WM_RBUTTONDOWN 0x204 / DBLCLK 0x206 -> slot 9, flag 0/1)
- onWmRButtonUp 0x1006a080 (WM_RBUTTONUP 0x205 -> slot 10)

`storeCommandHandler` 0x10067f60 (called by Lib::setCommandHandler 0x10067f10) stores the WM_COMMAND
callback into `g_commandHandler` (0x10128728, round 1). `onPaletteChanged` 0x10069a70 handles
WM_PALETTECHANGED (0x311): if the changed window is not ours it calls realizePalette 0x100699c0.

## Array<T> (dynamic array; three instantiations)

A minimal `Array<T>` template: a one-slot vtable (just the deleting destructor) and the fields
`T* m_data` (+4), `int m_cap` (+8), `int m_count` (+0xc), `int m_grow` (+0x10). `clear()` frees
`m_data`, zeroes the three counters and sets m_grow to 1. Three instances live as globals,
constructed at static init:
- g_arrayE0  (0x101286e0, vtable 0x1011da58) — element type undetermined.
- g_modes    (0x101286c8, vtable 0x1011da5c) — Array<ModeInfo> (0x14-byte records = Array_Rec14 of round 1).
- g_devmodes (0x101286f8, vtable 0x1011da60) — Array<DEVMODEA> (0x94-byte records = Array_Rec94 of round 1).

Each instantiation's ctor/dtor/clear/clearRet/deletingDtor are named with the matching `Array_e0` /
`Array_Rec14` / `Array_Rec94` prefix (round 1 already named `Array_Rec14::add` 0x100681a0 and
`Array_Rec94::add` 0x10068400). `count()` 0x10066a80 (Rec14) and 0x10066b50 (Rec94) return m_count;
`reset()` 0x10066ac0 (g_modes) and 0x10066b90 (g_devmodes) zero m_count. Attribution of count/reset is
by their callers (findMode/pickMode iterate g_modes; setModeIndex indexes g_devmodes; enumModes resets
both, passing 0x101286c8 then 0x101286f8).

## Font / FontBase

`class Font : public FontBase`. Font's vtable (0x1011da6c, installed by the ctor 0x10068660) has 5
slots: deletingDtor, loadFile (0x10068ae0), create (0x10068850), release (0x10068da0), textWidth
(0x10068e60) — the last four named by round 1. The FontBase vtable (0x1011da90) holds the base
defaults named here: deletingDtor 0x10069140, loadFile 0x10069090 (returns 0x18), create 0x10069050
(returns 0x18), release 0x100690d0 (no-op), textWidth 0x10069100 (returns 0). Font's own deleting
destructor 0x100691b0 (runs Font::~Font 0x10068760) is also named.

## Palette / PaletteBase

`class Palette : public PaletteBase`. Palette holds a 256-entry colour table at this+0xc (4 bytes per
entry: R,G,B,pad), with further state at this+0x80c (ctor arg), +0x810, +0x814. Its vtable
(0x1011db10) names setDefault/animate/setRGB (round 1) plus the entries documented here:
- Palette::ctor 0x1006a150 (chains PaletteBase::ctor 0x1006a260, installs vtable, inits fields).
- Palette::reset 0x1006a380 (vtable slot 2; zeroes +0x810/+0x814; also called from the dtor).
- Palette::getRGBRange 0x1006a8e0 (slot 4; copies a run of RGB triples out of the table).
- Palette::getColor 0x1006add0 (slot 8; one entry's RGB into g_rgbScratch 0x10128734).
- Palette::toRGBQuads 0x1006ae50 (called by Surface::setPalette 0x100089d0; builds a 256-entry BGRX
  RGBQUAD array for the DIB colour table).
- Palette::deletingDtor 0x1006a1f0 (runs Palette::~Palette 0x1006a2c0).

PaletteBase (vtable 0x1011db3c, slot 0 deleting dtor, rest pure) is named by its ctor 0x1006a260 and
dtor 0x1006af00 / deletingDtor 0x1006af50. The reserved GDI handle it manages, `g_logPalette`
(0x10128738), is deleted by `deleteLogPalette` 0x1006ad60 (DeleteObject), the body Display::freePalette
calls.

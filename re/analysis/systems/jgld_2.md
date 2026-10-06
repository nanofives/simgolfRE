# jgld_2 — Surface / SurfaceBase virtual-table map, Sprite accessors, Display window helpers

Range covered: the 98 functions in `log/naming/jgld_2.txt` that rounds 1 (jgld_0, jgld_1) left
unnamed. All are 100%-instruction-matched in `re/match/jgld_*.cpp`, so the layouts cited below are
exact. Addresses are RVAs (image base 0x10000000). This round's spine is the **vtable slot map** that
round 1 asked for; once built, most of the leftover functions resolve to a named virtual slot.

## The two shared default stubs

Every Surface/SurfaceBase virtual that a concrete class does not implement points at one of two
shared bodies:

- **0x1007f360** — the abstract/not-implemented default: `push 0x19; call 0x10082870` (the library
  error reporter, code 0x19) then returns `this`. It fills the SurfaceBase/SpriteBase slots that have
  no default at all. It is not in this range (not named here).
- The per-slot **0x18 default stubs** (0x10009740–0x1000a2f0 in SurfaceBase, 0x1000abb0–0x1000ae70 in
  Surface): each `return 0x18;` and pops its own argument count. **0x18 (=24)** is the library's
  not-supported-for-this-depth status (same family as `LIBERR_NOT_THIS_DEPTH`; cf.
  Sprite::remap 0x100150d0). A few slots instead `return 0` (0x10009740) or are void (0x10009810,
  0x10009840).

The arg count each stub pops (`ret N`) equals the real method's argument count, which corroborates
each slot→method mapping below (e.g. the +0xb8 default pops 4 dwords, matching Surface::textOut's
(x,y,str,len); the +0xb4 default pops 3, matching setTextColorRGB's (r,g,b)).

## Surface (vtable 0x1011d0b0) and SurfaceBase (vtable 0x1011d220)

Surface derives from SurfaceBase (which derives from Tracked). The two vtables are parallel: slot i is
the same virtual in both. SurfaceBase supplies the default (0x18 stub or 0x1007f360); Surface
overrides the slots it implements. The map (slot / vtable offset / Surface impl / SurfaceBase default):

```
 0  +0x00  Surface::deletingDtor 0x10007900     SurfaceBase::deletingDtor 0x1000a4e0
 1  +0x04  allocDrawBuffer 0x10007e40           0x1007f360
 2  +0x08  Surface::release 0x10007cc0          0x1007f360
 3  +0x0c  Surface::pixelAddr 0x10008830        0x1007f360
 4  +0x10  Surface::acquireDC2 0x10008730       0x1007f360
 5  +0x14  Surface::call3a 0x1000a810           0x1007f360
 6  +0x18  Surface::fwdVfn18 0x1000a750          0x1007f360          [fwd -> slot 4]
 7  +0x1c  Surface::call3b 0x1000a870           0x1007f360
 8  +0x20  Surface::fwdVfn20 0x1000a7b0          0x1007f360          [fwd -> slot 4]
 9  +0x24  Surface::releaseDC2 0x100087b0       0x1007f360
10  +0x28  Surface::acquireDC 0x10008640        SurfaceBase::acquireDC_default 0x10009740 (ret 0)
11  +0x2c  Surface::releaseDC 0x100086b0        SurfaceBase::releaseDC_default 0x10009810 (void)
12  +0x30  (shared) 0x10009840                  (shared) 0x10009840  [void, both vtables; method n/d]
13  +0x34  Surface::setClip 0x100083c0          0x1007f360
14  +0x38  Surface::blit 0x10008e50             0x1007f360
15  +0x3c  Surface::blitT 0x100091a0            SurfaceBase::blitT_default 0x10009870
16  +0x40  Surface::stretchTo 0x10008f70        0x1007f360
17  +0x44  Surface::fillRectDispatch 0x10009770 0x1007f360
18  +0x48  Surface::blendRectDispatch 0x10009ac0 0x1007f360
19  +0x4c  Surface::ditherRectDispatch 0x10009c50 0x1007f360
20  +0x50  Surface::ditherBlendRectDispatch 0x10009e40 0x1007f360
21  +0x54  Surface::blit16 0x100098f0           0x1007f360
22  +0x58  Surface::fillRect 0x10009320         SurfaceBase::fillRect_default 0x100098b0
23  +0x5c  Surface::fill2 0x10009440            SurfaceBase::fill2_default 0x100099c0
24  +0x60  Surface::drawDashedLine 0x100095a0   SurfaceBase::drawDashedLine_default 0x10009a40
25  +0x64  Surface::drawLine 0x100094e0         SurfaceBase::drawLine_default 0x10009a00
26  +0x68  Surface::unimplVfn68 0x1000abb0      SurfaceBase::defaultVfn68 0x10009a80   [both 0x18]
27  +0x6c  Surface::unimplVfn6c 0x1000abf0      SurfaceBase::defaultVfn6c 0x10009b50   [both 0x18]
28  +0x70  Surface::unimplVfn70 0x1000ac30      SurfaceBase::defaultVfn70 0x10009b90   [both 0x18]
29  +0x74  loadPng 0x100145a0                   SurfaceBase::loadPng_default 0x10009bd0
30  +0x78  Surface::saveImage 0x1000a550        SurfaceBase::saveImage_default 0x10009c10
31  +0x7c  Surface::unimplVfn7c 0x1000ac70      SurfaceBase::defaultVfn7c 0x10009d00   [both 0x18]
32  +0x80  Surface::unimplVfn80 0x1000adb0      SurfaceBase::defaultVfn80 0x10009f50   [both 0x18]
33  +0x84  Surface::drawTo 0x10009fd0           SurfaceBase::drawTo_default 0x10009d40
34  +0x88  Surface::unimplVfn88 0x1000acb0      SurfaceBase::defaultVfn88 0x10009d80   [both 0x18]
35  +0x8c  Surface::drawToZ 0x1000a330          SurfaceBase::drawToZ_default 0x10009e00
36  +0x90  Surface::unimplVfn90 0x1000acf0      SurfaceBase::defaultVfn90 0x10009dc0   [both 0x18]
37  +0x94  Surface::unimplVfn94 0x1000ad30      SurfaceBase::defaultVfn94 0x10009ed0   [both 0x18]
38  +0x98  Surface::unimplVfn98 0x1000ad70      SurfaceBase::defaultVfn98 0x10009f10   [both 0x18]
39  +0x9c  Surface::unimplVfn9c 0x1000adf0      SurfaceBase::defaultVfn9c 0x10009f90   [both 0x18]
40  +0xa0  Surface::unimplVfnA0 0x1000ae70      SurfaceBase::defaultVfnA0 0x1000a1b0   [both 0x18]
41  +0xa4  Surface::unimplVfnA4 0x1000ae30      SurfaceBase::defaultVfnA4 0x1000a170   [both 0x18]
42  +0xa8  Surface::selectObject 0x10008ae0     SurfaceBase::selectObject_default 0x1000a1f0
43  +0xac  Surface::selectSystemFont 0x10008b90 SurfaceBase::selectSystemFont_default 0x1000a230
44  +0xb0  Surface::setTextColor16 0x10008d00   SurfaceBase::setTextColor16_default 0x1000a2b0
45  +0xb4  Surface::setTextColorRGB 0x10008c40  SurfaceBase::setTextColorRGB_default 0x1000a270
46  +0xb8  Surface::textOut 0x10008da0          SurfaceBase::textOut_default 0x1000a2f0
47  +0xbc  Surface::setField4d0 0x1000a5d0      0x1007f360
48  +0xc0  Surface::getField4d0 0x1000a620      0x1007f360
49  +0xc4  Surface::callBounds 0x1000a8d0       0x1007f360
50  +0xc8  Surface::getClip 0x1000a930          0x1007f360
51  +0xcc  Surface::getClipRectPtr 0x1000a990   0x1007f360
52  +0xd0  Surface::getBounds 0x1000a9d0        0x1007f360
53  +0xd4  Surface::getBoundsRectPtr 0x1000aa30 0x1007f360
54  +0xd8  Surface::width 0x1000aa70            0x1007f360
55  +0xdc  Surface::height 0x1000aab0           0x1007f360
56  +0xe0  Surface::getField40 0x1000aaf0       0x1007f360
57  +0xe4  Surface::getDepthPtr 0x1000ab30      0x1007f360
58  +0xe8  Surface::getField7c 0x1000ab70       0x1007f360
59  +0xec  Surface::setPalette 0x100089d0       0x1007f360
```

Names in this file's TSV are the previously-unnamed cells above. Names without a `::` prefix
(allocDrawBuffer, loadPng) came from rounds 0/1; `setField28`/`initDepthInfo` are class-less because
the body is shared or operates on a Surface subobject.

### Surface object layout (offsets proven by the accessors above)

- **+0x24** int depth (8 or 16); read directly by blitT (0x100091a0) as the source depth and through
  the slot-+0xe4 getter (getDepthPtr) as the destination depth. The constructor runs `initDepthInfo`
  (0x100092e0) on `this+0x24`, clearing +0x28.
- **+0x28** int; written by `setField28` (0x1000aeb0) from Surface::drawTo / drawToZ.
- **+0x40** int (getField40, slot +0xe0).
- **+0x44** RECT clip rectangle {l,t,r,b}; copied out by getClip (0x1000a930), address returned by
  getClipRectPtr (slot +0xcc). setClip (0x100083c0) intersects the argument with the bounds rect into
  this rect.
- **+0x54** RECT bounds rectangle; copied out by getBounds (0x1000a9d0), address returned by
  getBoundsRectPtr (slot +0xd4). `width` (slot +0xd8) = r-l = *(+0x5c)-*(+0x54); `height`
  (slot +0xdc) = b-t = *(+0x60)-*(+0x58).
- **+0x7c** int (getField7c, slot +0xe8).
- **+0x4ac** a GDI region/object freed with DeleteObject (seen in setClip).
- **+0x4b8/+0x4bc/+0x4c4** cached GDI DC and use count (acquireDC/releaseDC, round 1);
  **+0x4c0/+0x4c8/+0x4cc** the secondary DC (acquireDC2/releaseDC2).
- **+0x4d0** a pointer (setField4d0/getField4d0, slots +0xbc/+0xc0).
- **+0x810** int (getField810, read by setPalette).

## Sprite (vtable 0x1011d380) and SpriteBase (vtable 0x1011d444)

The full Sprite vtable was dumped to place the leftover accessors. Slots 0–13 are object
plumbing/accessors; slots 14–40 are the remap and the drawA..drawP / blt / dashTo / lineTo family
already named in rounds 0/1. New names this round:

```
 0 +0x00  Sprite::deletingDtor 0x10014c00
 4 +0x10  Sprite::setField10 0x10017eb0        (field +0x10 setter)
 5 +0x14  Sprite::getField10 0x10017ef0        (field +0x10 getter)
 6 +0x18  setField28 0x1000aeb0                (shared with Surface; sets +0x28)
 7 +0x1c  getField28 0x10017f30  [round 1]
 8 +0x20  getField14 0x10017f70  [round 1]
 9 +0x24  Sprite::emptyVfn24 0x10017fb0        (empty 1-arg virtual)
10 +0x28  getField20 0x10017fe0  [round 1]
11 +0x2c  getField2c 0x10018020  [round 1]
12 +0x30  Sprite::getField30 0x10018060
13 +0x34  Sprite::getField34 0x100180a0
24 +0x60  Sprite::drawQ 0x100175d0             (draw/blit virtual, not match-covered)
```

`SpriteBase_vtable` (0x1011d444) slot 0 is `SpriteBase::deletingDtor` (0x10016930), which runs
`SpriteBase::~SpriteBase` (0x100168e0, round 1); every other SpriteBase slot is 0x1007f360.
`Sprite::getFlag18` (0x100180e0) is not a vtable slot; it returns bit 0 of the flags int at
Sprite+0x18 and is called by every Sprite draw variant (0x10015180..0x100193d0).

## Display window helpers (jgld_display.cpp)

The Display object is the 0x14c-byte block `operator new`'d by createDisplay (0x10065390, round 1).
Fields exercised by this round's methods:

- **+0x04** a DEVMODE-sized block passed to ChangeDisplaySettingsA by `Display::shutdown` (0x100656c0),
  which then DestroyWindow(+0x144) and destroys the back-buffer Surface at +0x148 via its vtable +8.
- **+0x12c / +0x130** two ints copied out by `Display::getField12c130` (0x100666b0).
- **+0x138** int returned by `Display::getField138` (0x10065bf0); the field onKey (0x10069c70) reads
  through g_displayLib+0x138.
- **+0x144** the game window HWND: `Display::getWindowRect` (0x10066650, GetWindowRect) and
  `Display::invalidate` (0x10066710, InvalidateRect) both pass it.
- **+0x148** the back-buffer Surface pointer (also mirrored by the global g_backBufferSurface
  0x1012873c read by Display::present and by 0x1001a6b0).

Corrected 2026-10-06: 0x1011d7dc and 0x1011d640 are not member subobjects but the base and derived
vtables of the Display object itself (DisplayBase_vtable / Display_vtable, re/names/jgld_3_globals.tsv and
re/analysis/systems/jgld_3.md). Display::Display 0x100654b0 calls 0x10065630 (stores 0x1011d7dc into
[this], the base constructor) at 0x100654e5 and then stores into [this] at 0x100654ff; 0x10065670 stores
0x1011d640 into [this] and calls the shared base destructor 0x10066d20 (derived destructor).

## Open questions

- Slots 26,27,28,31,32,34,36,37,38,39,40,41 of Surface/SurfaceBase are stubbed in both vtables, so the
  virtual's method name is not determined; they are named by vtable offset (unimplVfnNN / defaultVfnNN).
- Slot +0x30 (0x10009840) is a void no-op in both vtables; its method is not determined.
- Surface fields +0x40, +0x7c, +0x4d0, +0x810 have getters/setters here but no determined meaning.

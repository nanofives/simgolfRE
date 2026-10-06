# exe_3 systems (golf_clean.exe 0x0047af60 - 0x004852e0)

211 functions. 208 named. The range is the game's **Jackal UI/windowing toolkit** (0x0047af60-0x00481870),
a **2D image/sprite/FLC decoder and display-surface layer** (0x00481870-0x004838f0), the **screen
show/hide + modal input pump** (0x004838b0-0x00483ba0), and the **sound system** wrapper over sound.dll
(0x00483e90-0x004852e0). Evidence for each function is in `re/names/exe_3.tsv`; globals in
`re/names/exe_3_globals.tsv`. 172 functions have 100% matched C++ in `re/match/*.cpp`; the rest were
read from the Ghidra decompilation (cited by caller/callee/string/import).

All field offsets below are byte offsets from the object base. Confidence of the matched functions is
C4-grade evidence (byte-identical); the unmatched ones are decompilation-only.

## 1. UI / windowing toolkit (View / Window / Node / Panel)

A retained-mode window toolkit. Every window shares one large object layout (the same `this` seen across
mouse/key/paint/layout methods). Known offsets:

- `+0x9c`  m_flags1 (uint): 0x1=mapped/shown contribution, 0x4/0x8=has bottom/right scrollbar,
  0x10/0x11/0x400=bordered, 0x20=child uses parent offset, 0x1000=force-focus, 0x8000=scrolls with parent,
  0x200000=input-disabled, 0x2000000=stays at back of z-order, 0x20000000=no caption, 0x80000=has backing window.
- `+0xa0`  m_flags2 (byte): 0x1=visible, 0x2=use secondary rect (m_1bc/m_1c0), 0x8=disabled.
- `+0xb0`  owner/parent back-pointer (used by currentFocusOwner).
- `+0xbc`  m_bc: embedded HotList hit-test object (FUN_00492b10 hitRect / FUN_00492cc0 select).
- `+0x130` m_parent / container; `+0x138`/`+0x13c` active-child bookkeeping; `+0x140` focus child present.
- `+0x15c` caption/title child (has height()/extent()); `+0x150/+0x154/+0x158` scrollbar+corner children.
- `+0x180`,`+0x184`,`+0x188` border/title thickness fields; `+0x1f0` resize-border width.
- `+0x1ac..0x1b0` local rect/origin; `+0x1bc..0x1c0` outer (screen) rect/scroll; `+0x1dc` cached clip rect.
- `+0x20..+0x70` Panel notify targets; `+0x224` child array, `+0x228` capacity, `+0x22c` count.
- `+0x238..+0x264` per-event callback function pointers (mouse m_23c..m_258, click m_248, key m_25c, command m_264).
- `+0x26c`/`+0x270` the two scrollbar child objects.

### Coordinate transforms (subsystem ui)
`View::toParent` (0x47b170) / `View::fromParent` (0x47b200) walk the parent chain adding/subtracting scroll
(+0x1bc/+0x1c0) and origin (+0x1ac/+0x1b0) under m_9c bits 0x20/0x8000. `toLocal` (0x47b290),
`toGlobal` (0x47b2d0) and the RECT variants `offsetRectToParent`/`offsetRectToLocal` (0x47b0d0/0x47b120)
build on them.

### Size / layout (ui)
`setSize` (0x47b4e0), `moveTo` (0x47b420), `center` (0x47b310), `resized` (0x47bc60),
`layoutScrollbars` (0x47d570), `calcSizeFromCorners` (0x481760), `doLayout` (0x480d70). The border maths is
a four-way family: `growRect` (0x47cc10, RECT outward), `shrinkRect` (0x47cce0, RECT inward),
`adjustSizeOuter` (0x47ca10, size outward), `innerSize` (0x47cb10, size inward); all use g_titleBarH
(0x83ff10) plus m_184/m_188 border fields.

### Z-order and draw list (ui/render)
Two global arrays. The **z-list** g_zlist (0x83a2d8, count g_zcount 0x83ab94) is the top-level window stack:
`zPush` (0x47e4c0) inserts front/back by flag 0x2000000, `zRaise` (0x47e580) promotes. The **draw list**
g_drawList (0x839ac0, count g_drawCount 0x83ab90) is the per-frame paint order, rebuilt by
`rebuildDrawList` (0x47e450) which calls `collectDrawList` (0x47e330, recursive, visible-only) then
`clipAllWindows` (0x47e2d0) / `relayoutClip` (0x47e140, recursive IntersectRect into m_1dc). Container child
arrays are maintained by `ChildArray::insert/remove/bringToFront` (0x47e5f0/0x47e680/0x47e700) and the flat
`listRemove` (0x47e520). `Node::contains` (0x47b080) tests descendant membership.

### Painting (render)
`compositeScreen` (0x47fab0) is the full repaint: it computes clip rects for every window and calls
`redrawDrawList` (0x47f8e0), which walks g_drawList invoking each window's draw vtable slot (+0x13c).
`redrawWindow` (0x47fa30) repaints from one window. `addDirtyRect` (0x47d060) accumulates the dirty region
g_dirtyRect (0x83a4d8) and blits the save buffer g_blitSaveBuf (0x83a7b8). `Window::paint` (0x4808c0),
`paintBackground` (0x480a10), `fillWindowRect` (0x480b00), `invalidate` (0x480ce0), `refreshRect`/`refreshFull`
(0x480c20/0x480c80). `drawCursor` (0x47d130) builds the pointer sprite each move.

### Input dispatch (input)
Mouse buttons each have a dispatcher sharing one body that differs only in the callback offset, hit-handler
and panel it notifies: `mouseEvent238` (0x47be40), `mouseDispatch23c..258` (0x47bf40, 0x47c010, 0x47c0e0,
0x47c290, 0x47c360, 0x47c430, 0x47c500) and `click` (0x47c1b0). Each sets g_curWidget (0x83ab2c), runs its
m_2xx callback, hit-tests m_bc, then calls hitNN or missNN and notifies its Panel. Keyboard: `key`
(0x47c5d0, Enter/Esc close, Tab tabs), `dispatchAccel` (0x47c6c0, MapVirtualKeyA-filtered accelerator routing),
`dispatchCommand` (0x47c970). Focus: `setFocus` (0x47f1b0) -> g_focusWin/g_focusWin2 (0x83ab40/0x83ab44),
`currentFocusOwner` (0x47f2f0). Hit search: `hitTestScreen` (0x47f700) tries captured/focus windows then the
root trees via `hitTestTree` (0x47f340); `hitTestBorder` (0x47ed30) returns a resize-region code.
`matchInputCode` (0x47eee0) scans the 10-byte circular keystroke buffer g_codeBuf (0x83aba4) against the
string at 0x4e42ec (a typed code/cheat sequence).

### Tooltips (ui)
`Tooltip::show` (0x480220) lays out g_tipRect (0x83a2c8) from the caret-split text g_tipText (0x83aac4) using
the tooltip font g_tipFont (0x83aac8); `drawTooltip` (0x47cdb0) renders it; `Tooltip::hide` (0x480360)
clears and repaints. `Label::setText` (0x47b8f0) manages a window's heap text at m_14c.

## 2. Image / sprite / FLC decoder + display surfaces (render / video)

### Display surface and screen manager
The screen manager object g_screenMgr (0x83ad50) owns the display. Surfaces are created through it:
`Surface::createFromId` (0x483800, vtable f29), `Surface::createSized` (0x483850), `releaseBacking`
(0x4838b0), `ctorVtbl483b90` (0x483b90). `setPrimarySurface` (0x483ac0) installs g_primarySurface (0x83ad44);
`clearPrimarySurface` (0x483b10); `screenFlip` (0x483ba0) presents. Two manager singletons g_screenMgrA/B
(0x83acb0/0x83ac30) are built by static ctors and reattached by `resetScreenState` (0x483320);
`clearScreenObjects` (0x483340) empties g_screenObjList (0x83ac88).

### Chunk decoder -> surface
An image/frame resource is a stream of typed chunks. `decodeImageChunks` (0x482570) dispatches each chunk to:
`Palette::decodeChunk` (0x4826f0, runs of RGB triples into a 256-entry palette), `Surface::clear` (0x482940,
zero fill), `Surface::blitRLE` (0x482990, per-scanline byte-run RLE), `Surface::copyRaw` (0x482a80, memcpy),
and `decodeSprite4827d0` (0x4827d0, transparency/run compressed sprite with 0xc000/0x8000 control words into a
16-bit surface). `Image::seek` (0x482490) / `Image::step` (0x482420) walk frame records. The decoder dispatch
objects `Decoder482ae0/482b20::ctor` (0x482ae0/0x482b20) and the link/target plumbing
`Link482::resolve/data` (0x482e20/0x482e10), `Link482b::send` (0x482e40) wire chunks to their targets.
Resource caches are 5-slot: `Cache482fd0::ctor` (0x482fd0), `Cache483070::reset` (0x483070),
`Cache483010/483030/483060` (ctorVtbl/attach/detach).

### Palette
`Palette::setEntries` (0x4830e0) uploads BITMAPINFO RGBQUADs to the hardware palette; `Palette::nearest`
(0x483420) finds the closest index by squared RGB distance, honoring reserved ranges. Animated palette
cycling uses the 5-entry `Table483` (`Table483::find` 0x4833f0) with `allocColorRange` (0x483190) and
`startColorCycle` (0x4832e0, Timer486::start).

### FLC animation (video)
`Flic::open` (0x481f40) memory-maps a FLC resource (MappedFile::open 0x492dd0) and builds its structures;
`Flic::indexFrames` (0x481e40) walks chunks to the FLC frame magic 0xf1fa building the row/column frame
index; `Flic::buildFrameTable` (0x481ca0), `Flic::createStream` (0x482b90), `Flic::reset` (0x481ba0),
`Flic::ctor481b50` (0x481b50) + `Flic::ctorBase482b60` (0x482b60). The Bink video frame wrapper's scalar
deleting dtor `BinkFrame::scalarDtor` (0x480410) also lives here. `allocResampleTable` (0x481870) /
`freeResampleTable` (0x481b20) allocate a double-precision work buffer (g_83ac10/g_83ac14) for a resampling
or windowing step (its consumer is outside this range).

## 3. Screen show/hide and modal input pump (ui)

`showScreen` (0x483bb0) / `hideScreen` (0x483c10) / `redrawAll` (0x483c70) flush the gfx queues and toggle the
screen object. The event pump drains the OS/message queue while setting the phase global g_inputPhase
(0x83ad4c): `pumpInput483bd0` (0x3f), `pumpInput483c90` (2), `pumpInput483cf0` (8), `pumpInput483c30`.
Modal state: `enterModal483d30` (0x483d30), `modalPush483d40` (0x483d40), `modalFocusClear483d60` (0x483d60),
`leaveModal483d80` (0x483d80), `frameTick483cd0` (0x483cd0). All route through g_screenMgr vtable slots.

## 4. Sound system (audio)

A thin game-side wrapper over sound.dll, loaded on demand. `soundVersionCheck` (0x483fc0) LoadLibrary's
`.\sound.dll` (g_soundDll 0x83afc0) and MessageBoxes on a header-version mismatch (strings 0x4bac50 /
0x4babd0). `initSound` (0x483e90) runs the check and loads the three device singletons g_sfxDevice
(0x83ad58), g_musicDevice (0x83ad80), g_voiceDevice (0x83af98), then sets g_soundReady (0x83afc4).
`shutdownSound` (0x483f10) stops every active sound and `freeSoundLib` (0x484060) FreeLibrary's the dll.
The dll exports a function-pointer table g_soundFns (0x83af6c, 11 entries, zeroed by `clearSoundPtrs`
0x484130); `createSound4840e0` (0x4840e0) is the factory, `releaseSoundObj` (0x484110),
`callSoundHook74/94` (0x484090/0x4840c0). Active sounds form a doubly-linked list
(head g_soundHead 0x83af68 / tail g_soundTail 0x83af64) with `unlinkSound` (0x4843e0) and
`Snd::unlinkFromList` (0x4844e0).

### Sound-object (Snd) layout
One voice/channel class wraps a sound.dll device object at `m_40` (+0x40). Known fields:
`+0x4` volume (0..0x7f), `+0x8` pan (-0x40..0x3f), `+0x34`/`+0x38` device params, `+0x3c` mixer slot id
(<16 = pooled), `+0x44` state flags (bit0=playing), `+0x50` name string, `+0x54` mode, `+0x5c` pitch/freq
(-0x4b0..0x4b0), `+0x58` config flags, `+0x64` length, `+0x68` start time.
Construction: `Snd::ctorBase` (0x484150, setRate 1000), `Snd::ctorDerived` (0x484820, setMode 1),
`Snd::ctor485260` (0x485260, default volume 0x7f), `VoiceRx::ctor` (0x4852e0, kind 6, name (string in the binary)).
Destruction: `Snd::dtor` (0x4841e0), `SndMusic::dtor` (0x4848a0) and their scalar-dtor wrappers.
Control: `setMode` (0x484260), `setName` (0x484b80), `setVolume` (0x485140, called by the game-wide
`playSound` 0x4481b0), `setPan` (0x4847f0), `setPitch` (0x484f40), `setField34/38` (0x4846b0/0x4846d0),
`play` (0x484940), `start` (0x484e70), `stop484550`/`stop484f00`, `busy` (0x484b30), `flags` (0x484ff0),
`status` (0x4845e0), `open` (0x484c80), `reopen` (0x484d60), `configure` (0x484a40),
`SndMusic::setFile` (0x4842f0, streaming), `SndStream::open484c20` (0x484c20). Each control forwards to a
sound.dll device vtable slot (f17 pan, f39 pitch, f40 volume, f19/f1 params, v5 stop, v7 run).

## 5. Static init / CRT (crt)
0x482e80/0x482ec0/0x482f00/0x483de0/0x483e20/0x483e60 are compiler-generated static-object constructors for
the screen-manager and sound-device singletons; 0x482e90/0x482ed0/0x482f10/0x4837c0/0x483df0/0x483e30/0x483e70
register their destructors with `_atexit`. `setNewHeap` (0x4823c0) installs the global allocator (g_839650)
and aborts via MessageBox+_exit(3) on failure. `View::scalarDtor47d810` (0x47d810) is a scalar deleting dtor.

## Open questions
- 0x0047b9a0, 0x00483930, 0x00483980 left unnamed (see `log/naming/exe_3_notes.txt`).
- The exact meaning of several sound.dll device vtable slots (f17/f39/f40 named by effect, others numbered).
- Whether `allocResampleTable` (0x481870) feeds audio or image resampling: its buffer consumer is outside
  this range, so the subsystem is recorded as `util`, not `audio`.

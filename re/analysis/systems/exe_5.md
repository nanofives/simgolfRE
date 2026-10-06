# exe_5 systems — golf_clean.exe 0x004935f0 .. 0x004abe6b

Range of 211 functions. 190 named in `re/names/exe_5.tsv`, 21 left in
`log/naming/exe_5_notes.txt`. Every named function has a 100% match source in
`re/match/*.cpp` unless the evidence column cites only callers/callees/strings.
All offsets below are from the decompilation at the cited address.

Subsystem note: this range contains the Jackal engine's network/transport layer. Its 15
`net*` functions are tagged `net` (subsystem added 2026-10-06; they were `util` before).

## 1. Dropdown/combo control (ui) — 0x004936a0 .. 0x00494020, 0x004940e0

A combo/dropdown list widget. `comboInit` (0x004936a0) sets state flags at
`this+4`, creates the child widgets and installs the item list via `comboSetItems`
(0x00493ef0) / `listSetItems` (0x00494020). `comboOpenPopup` (0x00493b90)
positions the popup window relative to its parent (`toParent` 0x0047b170,
`moveTo` 0x0047b420). `comboRefresh` (0x00493a60, and the thunk 0x00493a30)
lays out items (`comboLayoutItems` 0x00493f50), draws the box (`drawBox`
0x00477e60) and highlights the current row. Selection is read through
`comboCurrentIndex` (0x004942a0) and `listCurrentIndex` (0x004941e0).
`comboFindItem` (0x004940e0) walks the item linked list rooted at `this+0x2d98`
(node id at +4, next at +0xc). The control uses very large `this` offsets
(0x22e8, 0x2d98, 0x3468, 0x3480), i.e. it is embedded in a larger view object.

## 2. UI text markup (ui) — 0x004942f0, 0x004935f0, 0x00494cb0

`expandTextMarkup` (0x004942f0) preprocesses UI strings, finding `$`-prefixed
tokens — `$DROPLINK` (0x004e423c), `$DROPDOWN` (0x004e4248), `$LINK` (0x004e4708),
`$LINK<` (0x004e4254), `$HEX` (0x004e4714), `$NUM`/`$NUMBER` (0x004e471c/0x004e4724)
— and expanding each into an output buffer. It uses `_strstr`, `_strncmp`,
`__itoa` and `findByteInRange` (0x004935f0, scans for a byte in a value range).

`setHudTextSlot` (0x00494cb0) is a separate on-screen text registry: given a slot
index 0..9, a string and x/y, it stores into the parallel global arrays
`g_hudTextX` (0x0083e8b8), `g_hudTextY` (0x0083d3c8) and `g_hudTextBuf`
(0x0083e8e0, stride 0x100). It is called from the network dispatchers (so the
transport layer can post status lines) and from 0x0048e900 / 0x0048e1c0.

## 3. Image decode and fills (render) — 0x00495100, 0x00495420, 0x00493630, 0x00494d30, 0x00494f00

`decodeImagePaletted` (0x00495100) and `decodeImage16` (0x00495420) decode a
custom image format guarded by the signature string (string in the binary)
(0x004e472c / 0x004e4748). Both are called from `load@r3_C478cd0` (0x00478cd0),
read a 0x80-byte header, then either install a palette (`setPalette` 0x004830e0)
or fill 16-bit pixels (`fill16` 0x00493630) and paint (`paint` 0x00474e70).
`drawColorRunA`/`drawColorRunB` (0x00494d30 / 0x00494f00) draw runs of 16-bit
pixels into a surface, looking colors up through the owning widget's palette
(virtual calls at vtbl +0xcc / +0xe4).

## 4. Scrollbar widget (ui) — 0x004961d0 .. 0x004979a0, 0x004974d0

A scrollbar. `scrollbarCreate` (0x00496330) stores range min at `this+0x580`,
max at `this+0x584`, page at `this+0x578` and orientation flags at `this+0x574`,
after `controlInitBase` (0x004961d0). Two orientations are exposed via
`scrollbarCreateA`/`scrollbarCreateB` (0x00496710 / 0x00496740) and their
`_impl` forwarders (0x00496690 / 0x004966d0). Interaction: `scrollbarOnDown`
(0x00496c30) records the drag anchor in globals `g_scrollDragX`/`g_scrollDragY`
(0x0083ab30 / 0x0083ab34), `scrollbarOnDrag` (0x00496d20) reads them,
`scrollbarOnKey` (0x00496e30) and `scrollbarScrollBy` (0x004973c0) move the
position, and `scrollbarDraw` (0x00496fc0) draws the track and thumb using
`scrollbarThumbRect` (0x004974d0), which derives the thumb pixel rect from the
track rect at `this+0x5ac` and the min/max/page range. `scrollbarResetState`
(0x004979a0) / `scrollbarResetThumb` (0x004967d0) reset state. `comboDtor`
(0x00497a20) and its deleting variant (0x00497a00) tear the control down.
`orderPair` (0x00496770) swaps two ints into ascending order (drag range helper).

## 5. Network/transport layer (net) — 0x00497b00 .. 0x0049bec0

The Jackal engine network stack (string (string in the binary) 0x004e4680). Entry is `netPump`
(0x0049aa70), called from `gfxFlushA` (0x00497b00) during screen flush;
`gfxFlushB` (0x00497b20) drives `netServiceControl` (0x0049ab40). `netPump`
calls `netService` (0x00497fc0, processes transport state and incoming traffic,
strings (string in the binary) 0x004e47b0, (string in the binary) 0x004e47e8) and
`netSendMessages` (0x00499140, serializes/transmits queued output, retries,
string (string in the binary) 0x004e47fc). Outgoing messages
are queued via `netQueueMessage` (0x0049b690) / `netSendList` (0x00497b40) /
`netFlush` (0x00497cc0); the receive side is `netPoll` (0x0049b7b0) which drains a
queue with `queuePop` (0x004a4c70). `netHandleControl` (0x0049acf0) processes
control messages — (string in the binary) (0x004e4824), (string in the binary),
(string in the binary) — and edits the peer list via `netListRemove` (0x0049bec0).
`netNameOf` (0x0049aa30) returns a peer name. `netHeapAlloc` (0x0049b970)
allocates net objects and aborts on failure ((string in the binary)
0x004c1434). `netSendShutdown` (0x00497d10) clears the send buffers. Timing
counter `g_netTimer` (0x008400b0) is read by `netTimerReadA`/`netTimerReadB`
(0x00497c20 / 0x00497c70). This stack is inherited from the Jackal/SMAC
multiplayer lineage.

## 6. Movie / FMV player (video) — 0x0049c8e0 .. 0x0049cf50

A frame-FIFO movie player driven during screen updates. `serviceMovie`
(0x0049c8e0, called from showScreen/hideScreen/redrawAll) calls
`serviceMovieFrame` (0x0049c910), which runs `movieDecodeStart` (0x0049cae0,
(string in the binary)), `movieReadProcess` (0x0049cb20, (string in the binary))
and `moviePlayFrame` (0x0049ccc0, (string in the binary), (string in the binary)).
`movieReadFrame` (0x0049cb90, (string in the binary)) reads a frame and
`listPush`es (0x004a4db0) it into the FIFO; `moviePlayFrame` consumes it with
`listRemoveHead` (0x004a4ed0). `movieAdvanceFrame` (0x0049c940, "Advancing
audio.", (string in the binary)) steps audio. `movieSelectStream` (0x0049cf50)
switches the active buffer. The player logs through `logLine` (0x004a0320).

## 7. Debug logging (util) — 0x004a00f0 .. 0x004a0320

A text log to (string in the binary) (0x004e4a44). `initLogFile` (0x004a0130) opens it
into `g_logFile` (0x00840930). `logWrite` (0x004a0280) writes formatted records
((string in the binary) 0x004e4a50). `logLine` (0x004a0320) is the convenience wrapper
used by the movie player and the file reader. `loggerCtor`/`loggerSetPath`
(0x004a0180 / 0x004a01d0) build the logger and resolve its path via
`resolveAndOpenFile` (0x004a00f0, `resolvePath` 0x00491da0).
`loadGraphsyObject` (0x004a00a0, (string in the binary) 0x004e4a2c) loads a
resource object into `g_graphsyObject` (0x0084092c).

## 8. String class Str49d (util) — 0x0049d090, 0x0049d100, 0x0049d0e0

Small heap-backed string. `strAssign` (0x0049d090) copies a C string (substituting
(string in the binary) 0x004e49f4 for null), `strDup` (0x0049d100) duplicates, `strFree`
(0x0049d0e0) releases.

## 9. Resource loading and static init (render/ui/input) — 0x0049d1b0 .. 0x0049fe50

Resource init is driven by `init4855b0` (0x004855b0, outside this range).
`loadPcxSurface` (0x0049d1b0, (string in the binary)), `loadCursors` (0x0049e9d0, cursor
surfaces 0x00840830/60/90), `loadFonts` (0x0049fe50, font surfaces
0x008408c8/f8) and `loadFileWinResource` (0x0049d3b0, (string in the binary), buffer
0x00840820) populate the graphics globals; the matching `free*` functions
(0x0049d280, 0x0049ead0, 0x0049ff30, 0x0049d460, 0x004a0060) release them, driven
by the mass-cleanup `f_00485740`. Numerous `initGfxGlobal_*` thunks
(0x0049d160, 0x0049d2a0, 0x0049d300, 0x0049d360, 0x0049d490, 0x0049d4f0,
0x0049d550, 0x0049eb00, 0x0049eb60) are C++ dynamic initializers that construct
each surface global via the 0x00473ab0 constructor, each paired with a
`regAtexit_*` thunk that registers the static destructor.

## 10. Flag/bit array (ui) — 0x0049ee90 .. 0x0049f030

A small bit/flag array used by the option dialog. `flagSet` (0x0049eef0),
`flagIsSet` (0x0049ef80), `bitSet` (0x0049eff0), `bitTest` (0x0049f030), and the
two toggle wrappers (0x0049ee90, 0x0049eec0).

## 11. Option / checkbox dialog (ui) — 0x0049ed20 .. 0x0049fda0

Built by `optionDialogBuild` (0x0049ed20) from the panel dispatcher. Drawn by
`optionDialogDraw` (0x0049f050) and `optionListDraw` (0x0049f370, scrollable
rows). Input via `optionDialogOnKey` (0x0049fa90), `optionDialogOnMouse`
(0x0049fda0) and `optionDialogUpdate` (0x0049f9a0), which toggle flags for the
picked item through system 10. `radioSelect` (0x004a0350) handles radio-button
groups (`setMode` 0x004890e0).

## 12. Composite view / panel (ui) — 0x0049d5a0, 0x0049ebb0, 0x004a0600 .. 0x004a4890

A large composite view built by `viewCtor4a0740` (0x004a0740), which constructs
several sub-fields (`viewFieldCtor49d5a0`, `viewFieldCtor49ebb0`,
`viewFieldCtor4a2250`, `panelSubCtor4a3110`). `panelDispatch` (0x004a08a0)
routes a command id to `panelBuildRow` (0x0049d6d0), `optionDialogBuild`,
`panelCreate4a2400` or `panelBuild4a3880`. Text content is assembled by
`panelBuildText`/`panelBuildText4a3be0` (0x0049d770 / 0x004a3be0) using
`Text477::put` (0x00477250). Drawing: `listPanelDraw` (0x0049dab0, scrollable
list of rows with aligned columns), `panelDrawRow` (0x004a2d50),
`panelDraw4a3f10` (0x004a3f10). Interaction: `panelOnPick` (0x004a4350) and
`panelOnKey` (0x004a4680) hit-test a HotList (`HotList::hit` 0x00492a90, seven
callbacks `hotHitCb0..6` at 0x004a43f0..0x004a4570) and redraw; `listFindById`
(0x004a4890) resolves an entry id. Views are initialized by `viewDispatchOpen`
(0x004a1250) / `viewDispatchReset` (0x004a12e0) and torn down by `panelStop`
(0x004a11c0); sub-field cleanup is `viewFieldCleanup49d690`/`49ece0`.
`postMessageLine` (0x004a0600) posts a speech/message line (called from `say`
0x0048df20). `sendCommand` (0x004a09a0) appends a command to an outgoing buffer.
These functions use multiple-inheritance base-pointer arithmetic (negative `this`
offsets such as -0x604, -0x8c), i.e. the panel is one of several base subobjects.

## 13. Generic containers (util) — 0x004a4b00 .. 0x004a4ed0, 0x004a28a0, 0x004a29c0

Singly-linked list / bounded FIFO / queue used by the network and movie systems:
`listClear4a4b` (0x004a4b00), `listAppend4a4b60` (0x004a4b60), `queuePop`
(0x004a4c70), `listClear4a4d` (0x004a4d50, thunk 0x004a4d30), `listPush`
(0x004a4db0), `listRemoveHead` (0x004a4ed0). `listAddEntry` (0x004a28a0) and
`setStringField` (0x004a29c0) manage heap-copied string entries;
`bufferCopyTo`/`bufferFree` (0x004a0540 / 0x004a05d0) copy and free buffers.

## 14. CRT tail (crt) — 0x004a4fc4, 0x004a512a, 0x004a910c, 0x004a9115, 0x004abe6b

`initFpControl` (0x004a4fc4, called from __fpmath) sets the FP control globals
0x004e4b00.. . `doexit` (0x004a512a, from _exit/__exit) runs the atexit/term
lists via __initterm. `errnoPtr` (0x004a910c, getptd()+2) and `doserrnoPtr`
(0x004a9115, getptd()+3) return the per-thread errno / _doserrno addresses.
`amsgExitThunk` (0x004abe6b) forwards to the CRT fatal-message exit.

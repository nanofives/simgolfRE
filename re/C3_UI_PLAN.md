# C3 plan for the UI / drawing pool (rounds 29-31, written 2026-10-12)

## Where the pool stands

After the simulation plan, golf_clean.exe has 410 C2 rows: 400 reachable (a recorded caller or a stored-pointer
installer), 285 of them in ui / render / video / frontend. Round 28 already offered every reachable row, so what is left
was dropped for a reason. The drop notes (log/c3/c3dv_notes.txt, c3dw_notes.txt, c3dx_notes.txt) give four blockers:

1. **The image pipeline.** Image_draw 0x473e60 and Surface_drawScaled 0x473cb0 are held (re/c3_hold.tsv): their
   this+8 != 0 arm calls loadImage 0x475840 (file I/O), which no fixture reached. Image_draw is a C2 callee of 35 of
   the 285 rows, loadImage of 18, Surface_drawScaled of 11 (callee counts over hooks.csv, 2026-10-12).
2. **The widget object graph.** The ctor / reset / dtor families (ListBox::resetFields, Button::resetFields,
   viewFieldCleanup*, panelSubReset4a35e0, controlInitBase) all reach Window::ctorInit 0x480610 = layoutNode + Image_releaseAll
   of a full window; c3dx: "need c3di / c3cz object graphs".
3. **user32 side effects.** Surface_clip 0x479a80 (GetForegroundWindow, ClipCursor), drawCursor 0x47d130 (ShowCursor),
   ListBox::onMouse 0x48c6e0 (ReleaseCapture), drawTooltip 0x47cdb0 (the live display's +0xa4 even with no tooltip).
4. **Size and order.** 0.4-2.8 KB drawing bodies over live windows, several waiting on a body below them (the listPanel /
   optionDialog bodies need a 1.5-2 KB drawing body each first).

Every tool these need exists or is one shared helper away: the import stand-in (c3cy), the heap stand-in with realloc and
scheduled failures, __freeze, synthetic Surfaces on jgld's real vtable, the recording display, the CRT file-layer
stand-ins (c3dz fopen/fgets/fclose; c3dw: fgets / fwrite / fflush / fprintf / fopen / __read / __write) and calleeStand (c3ea).

## Round 29: the four blockers, one batch each

Candidates: `py -3.12 log/c3/round29_candidates.py` -> log/c3/c3e{e,f,g,h}_candidates.tsv (each row's callees still
below C3 listed). Brief: log/c3/round29_brief.md. The four batches touch disjoint rows and run in parallel.

| Batch | Model | Rows | Bytes | What it adds |
|---|---|---|---|---|
| c3ee image pipeline | Opus (pilot) | 17 | 8.3 KB | shared `re/frida/js/filestandin.js`: one call serves a fixture file's bytes to both MappedFile's KERNEL32 imports (CreateFileA / CreateFileMappingA / MapViewOfFile / UnmapViewOfFile / CloseHandle / GetFileSize) and the static CRT file layer, with the file cursor and every write in the compared state. Then bottom-up: Surface_paint, decodeImage16 / decodeImagePaletted, loadImageData, loadImage, and Image_draw / Surface_drawScaled released from hold with keys that run the loadImage arm; cropSprite, Surface_blit, loadBmp / reloadBmp / loadBmpSurface, openImageAsset, decodeSprite4827d0, Image::loadPcx / savePcx, loadFileWinResource. Fixture images are generated files (log/c3/c3ee_gen.py), every format arm the decoders branch on. |
| c3ef widget lifecycle | Opus (pilot) | 38 | 9.4 KB | shared `re/frida/js/uitree.js`: builds real widgets with the game's own constructors (C3 ones run through their hooks) on a recording display, never linked into the live z-list, and saves / restores every global the constructors touch; a snapshot of the built tree is the state region. Then the ctor / reset / dtor families: Window::create + layoutViewTree, Button, ListModel, ListBox, viewField*, panelSub*, controlInitBase, viewCtor4a0740, TextView::reset, the deleting destructors. |
| c3eg window plumbing | Sonnet | 18 | 6.7 KB | a user32 preset for the import stand-in (ShowCursor, ClipCursor, GetForegroundWindow, SetCapture / ReleaseCapture, IntersectRect forwarded) in its own fixture file; Window::invalidate, Surface_clip, Window::show, addDirtyRect, relayoutClip, redrawDrawList, drawTooltip, drawCursor, EditBox_setCursorChar, Window::layoutScrollbars, the press / release / key / accelerator handlers, ListBox::typeAhead / onMouse. |
| c3eh drawing bodies | Sonnet | 12 | 8.4 KB | no new tool: ListBox::drawContents (+ vtordisp), ListBox::measure, drawPanelContents, drawScrollText, drawMatchScoreboard, drawWindowFrame, drawCircle, buildControlStrip47d610, comboSetItems, panelBuildChildren, drawMinimapSegment, on synthetic Surfaces and a recording display under __freeze. |

85 functions, ~33 KB. A good outcome is 60-70 promoted plus the two holds released; the two helpers matter more than
the count, because rounds 30-31 are built on them.

Verifier (this session): the round-28 recipe. Each agent's instance READY -> main build with every hook on and with
other batches' hooked callees off (one batch per boot, 20-60-key chunks) -> existing C3 callers of the new hooks both
ways -> `c3_verify.py <ID> --apply`. Image_draw / Surface_drawScaled leave re/c3_hold.tsv only when a key runs the
loadImage arm (the reason they were held), and c3df's keys for both are re-run over the new loadImage hook.

## Round 30: consumers of round 29

- Image_draw's and loadImage's callers (35 + 18 rows, many of them the show* / select* screens' small helpers), with
  filestandin.js serving the assets.
- Deferred from round 29 because each calls a row it delivers: scrollbarDraw, LabelButton::drawContents, drawPanel4810d0
  (Image_draw); compositeScreen, uiPointerMove (drawTooltip / relayoutClip / redrawDrawList / drawCursor); comboRefresh +
  thunk (Window::invalidate); TextView::render (panelStop).
- The listPanel / optionDialog / combo bodies and the view dispatchers viewDispatchBuild / viewDispatchOnScroll /
  viewDispatchDraw with their vtordisp thunks (c3dx: only the default arm was reachable without real panels; uitree.js
  gives them real panels).
- Modal pilot (Opus): a `modal.js` helper that stands in the C3 pumps (waitTicks, pumpMessages, pumpAndRedraw,
  flushRedraw) with a scripted input queue (SimGolfShim_SetVirtualKey + virtual cursor), so a modal loop runs N
  iterations and leaves on a scripted Esc / click. First users: Window::runModal 0x47bab0, textInputBox 0x45b2c0,
  runScrollText 0x46dea0, showPopup 0x46d6e0, skillPointDialog 0x45f0f0.

## Round 31 and after

The show* / select* / pick* screens over modal.js + filestandin.js + uitree.js, then drawWindow 0x40d320 (Surface_drawScaled)
and narrateShot 0x407e00 (saveBmp, already at C3), dropped from the simulation plan. Out of scope as before: mainLoop
and the asset loaders over the shipped archives (loadInterfaceArt, loadWorldAssets, loadInfoScreenArt).

## Not changed by this plan

The C4 harness (DEFERRED.md D-0002) stays the higher-value track: these rounds still prove functions one at a time.
The UI rounds and the C4 work touch different files and can run at the same time.

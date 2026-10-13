# C3 end-game plan: the last 305 open C2 rows (written 2026-10-13)

## Where the pool stands

After round 32: 2,679 functions at C3, 305 C2 rows still open (not `dead`, not `trivial`, not held in
re/c3_hold.tsv): sound.dll 169, golf_clean.exe 109, jgld.dll 18, Terrain.dll 9. Every reachable row has been offered at
least once, so what is left was dropped for a reason. log/c3/endgame_classified.tsv (built from log/c3/endgame_pool.tsv by
a read-only pass over every batch's notes, re/c3_hold.tsv and the round summaries; helper log/c3/alt/endgame_classify.py)
gives each row one class and the newest measured blocker, with the notes file it comes from:

| class | exe | jgld | sound | Terrain | total | bytes |
|---|---|---|---|---|---|---|
| SIZE_ONLY (no technical blocker measured) | 62 | 2 | 53 | 0 | 117 | 130,075 |
| NO_CALLER | 9 | 0 | 11 | 9 | 29 | 12,038 |
| CRT (non-C conventions, process exit, heap internals) | 3 | 0 | 26 | 0 | 29 | 3,349 |
| OTHER | 10 | 3 | 10 | 0 | 23 | 64,820 |
| MMIO (RIFF / mmio parsing) | 0 | 0 | 20 | 0 | 20 | 16,420 |
| LIVE_STATE | 8 | 0 | 10 | 0 | 18 | 11,982 |
| X87 | 1 | 1 | 16 | 0 | 18 | 13,458 |
| NO_OBSERVABLE | 11 | 0 | 5 | 0 | 16 | 347 |
| THREAD (thread / window / timer procs) | 0 | 0 | 12 | 0 | 12 | 7,553 |
| ASM (register-only inline-asm loops) | 0 | 9 | 0 | 0 | 9 | 21,974 |
| SEH_FRAME | 5 | 2 | 0 | 0 | 7 | 25,329 |
| NEVER_TRIED | 0 | 1 | 6 | 0 | 7 | 636 |

The classification also found recorded blockers that later tools have removed:

- **The Terrain grid** (7 exe LIVE_STATE rows: applyTileVariation, rebuildTileData, initTerrain, togglePath,
  applyTileElevation, renderTerrainStrip, updateTerrainRender). c3ey's fixture Terrain object at [0x820ed0], with the six
  Terrain imports stood in at the exe's IAT, is the tool c3eu named.
- **Single frame objects** (jgld Surface::drawTo 0x9fd0, drawToZ 0xa330). One Sprite sits in the frame, and
  framemask.js's single-object path is proven (c3es).
- **The MappedFile writers** (jgld Surface::saveImage 0xa550, writePcx8 0x12140): filestandin.js covers MappedFile.
- **The net family** (9 exe rows). Their blocker was the wsock32 imports; importstandin.js covers those, and the net
  object 0x008400b8 is NULL at the menu.
- **About 25 exe SIZE_ONLY rows** whose blocker was a callee that has since reached C3 (showPopup, modal.js, drawWindow,
  Surface_drawScaled, textInputBox, loadStoryStrings, saveGame, playAudioFile, Window::create, drawRichText ...):
  narrateShot, demolishObject, GolferPanelEject, buildTournamentPrep and others.
- **crtValidatePointer** (held since round 21: its result is IsBadWritePtr's): importstandin.js can stand that import in.
- **DC_calla0 0x475c20**: `call 0x475c20` at 0x480f30 sits in code outside every function. Creating that function
  (CreateFunctions.java) gives it its caller.
- **Exports reached only through GetProcAddress**, misread as `reach none`. jgld createDisplay is export #1
  get_graphsy_object_ptr (loadGraphsyObject 0x4a00a0). sound.dll init_sound_timer, release_sound and the six
  Dll_*::create_device / delete_device are ordinal exports #3-#10 that initSound 0x483e90 fetches. They need `--callers`,
  because pointer_callers.py cannot see an ordinal import.
- **sound.dll's 12 THREAD and 10 LIVE_STATE rows** were judged on MARIANO-PC, where the timer thread and the engine ran.
  On LIVING-PC keepsound.js pins sound.dll while the game takes its no-sound path (g_soundReady 0x83afc4 = 0), so no game
  thread should enter them. One probe confirms it (count entries of each row over a 60 s boot); if it shows 0, they are
  ordinary rows here.

## Three paths

Every open row ends in exactly one of three places: **C3** (path A or B), a **C2 end state** (`dead` / `trivial`,
re/CONFIDENCE.md) or a **hold** with its measured reason in re/c3_hold.tsv (path C). The end-game is done when
`re_classify.py status --summary` reports `C2 still open: 0`.

### Path A: ordinary batches (round 33), about 150 rows

No new tool, the existing helpers and the round 28-32 briefs:

| batch | rows | what |
|---|---|---|
| A1 exe, removed blockers | ~45 | the Terrain-grid 7 on c3ey's fixture, the net 9 through importstandin.js, the ~25 whose callee reached C3, DC_calla0 after creating its caller function |
| A2 exe, size only, small | ~35 | the exe SIZE_ONLY rows under 2 KB not in A1 |
| A3 exe, size only, large | 13 | the rows over 2 KB: showCourseReport 14,054 B, buildSceneSprites 9,100, netSendMessages 6,382, GolferPanelEject 6,292, narrateShot 6,107, drawWorldObjects 4,676, netService 4,479, loadSoundTable 3,724, showBuyLandScreen 3,158, showInfoCursorPanel 0x433190 2,692 ... One per agent session where needed (c3er's method). |
| A4 sound, size only + never tried | ~59 | sound SIZE_ONLY 53 (minus the 8 small-block-heap rows, which go to path C), NEVER_TRIED 6, the GetProcAddress exports with `--callers` |
| A5 sound, THREAD + LIVE_STATE re-probe | 22 | the probe above first; rows the game never enters on LIVING-PC become ordinary rows |
| A6 jgld | ~8 | drawTo / drawToZ through framemask.js, saveImage / writePcx8 through filestandin.js, createDisplay with `--callers`, the 2 SIZE_ONLY, crtValidatePointer |

Naming step first (one names TSV, the gate needs named callees): Window::setSize, MultiSound_ctor,
MultiSound_ctorWithType, MultiSound_setSlotSource and Sound_lookupOrCreate are blocked only by unnamed callees.
Expected yield at round 31-32 rates (40-70% on offered pools): 80-110 promoted.

### Path B: two tooling pilots (round 34), up to 38 rows

Only two blocker classes are large enough and narrow enough to pay for a shared tool:

- **B1 mmio stand-in** (20 sound rows, 16.4 KB): `re/frida/js/mmiostandin.js`, built like filestandin.js. It serves
  fixture RIFF / WAVE bytes to winmm's mmioOpen / mmioRead / mmioDescend / mmioAscend / mmioClose / mmioSeek on the
  A/B thread, with the cursor, chunk stack and an operation log as compared state. Controls: null RED, one body wrong
  only in a chunk id RED, a descend into a missing chunk returning its error code. Pilot in Opus, then the 20 rows.
- **B2 x87-faithful harness** (18 rows, 13.5 KB, 16 of them sound mixers / renderers). c3ap's x87 method covers the
  single-expression helpers: each `fstp` spill becomes a float assignment, operands are widened to double, the DLL's own
  constants are used. The Voice_mix* / Voice_renderStream* leaves set their own control word 0xf7f (64-bit, toward zero)
  and keep state in registers (round 16), so a C body cannot reproduce their rounding. The pilot decides, on two leaves,
  whether a C body that sets the same control word with _controlfp and spills explicitly matches bit for bit. If it does,
  the 16 follow; if not, they go to path C with that measurement as the reason.

Expected yield: B1 15-20, B2 0-16.

Not worth a tool:
- **ASM (9 jgld blitters):** a copy of the inline-asm loops (asm2inline.py) is a transcription, not an understanding, and
  proves nothing C3 asks for.
- **SEH_FRAME exe (5 browser screens):** the array path needs real Surface array construction under the original's SEH
  frame (c3es section 5), which brings back the frame-layout dependence the mask removes. All five are 3-7.5 KB modal
  screens.
- **CRT (29):** most use non-C conventions or end the process.

These go to path C unless a path-A batch finds a narrow way in (one CRT string helper, say).

### Path C: end states and holds (round 35 close-out), about 110-150 rows

| target | rows | end |
|---|---|---|
| NO_CALLER exe / sound, not exported | ~11 | `dead` after `re/tools/endstate.py` (sound PtrList_pushFront / insert, the Voice_renderStream* with no caller, Strftime, fp2ArgResultDispatch, atexitDtor_100b49b8; exe DC_call90 / DC_calla4 / Widget_writePixel / EditBox::charPosToPixel) |
| NO_CALLER, exported | 13 | hold: the 4 exe Terrain accessor exports (tileAt, getElevation, getWall, getType, dead at runtime, CLAUDE.md) and the 9 Terrain.dll exports no import reaches (exported, so never `dead` by definition) |
| NO_OBSERVABLE | 16 | `trivial` where endstate.py passes (fix first: null_method 0x4b01e0, jpeg_mem_init 0x4b04b0, ListBox::Slot60 0x487af0 fail its eax check with `0 ret(s)`, an extent issue); a hold for the vtordisp thunks onto a bare `ret`, which are not `trivial` by definition |
| CRT | ~29 | hold: process exit (error_exit, _amsg_exit), non-C conventions (_alloca_probe, the fp dispatchers), the heap stand-in's own anchors |
| sound small-block heap | 8 | hold: the module runs on the system heap (0x100b5d20 = 1), dead at runtime (round 23) |
| ASM | 9 | hold, reason as above |
| SEH_FRAME exe | 5 | hold, reason c3es section 5 |
| OTHER | ~15 | hold: the 6 out of scope (mainLoop, gameMain, loadFlic, the three asset loaders), the 5 already held, 2 with an undefined eax on one path (setOutputMasterVolume, DirectSoundDevice::setMasterVolume), 2 where the original faults (dashedVLine16, dashedLine16), realize_virt_arrays (half its branch sides cannot run); loadPng goes to path A first (c3ds's longjmp catcher covers libpng) |
| path A / B leftovers | rest | each with the measured reason its batch records |

A hold is not a failure: re/c3_hold.tsv says what was measured and what would release it. The close-out round
regenerates log/c3/c2_survey.py and checks that every open row has C3, an end state or a hold.

## After the end-game

C2 open = 0 closes the C3 track. The higher-value track stays the C4 harness (DEFERRED.md D-0002): canonical-scenario
ON/OFF runs that turn verified functions into C4. It runs on different files and can start before path C ends.

## Order and checkpoints

1. Round 33: path A (A1-A6 plus the naming step). Re-run log/c3/c2_survey.py and the classifier afterwards.
2. Round 34: the B1 and B2 pilots, then their users.
3. Round 35: path C close-out (the endstate.py extent fix first), plus the C4 harness kickoff.

Children run on claude2, model `opus` (= claude-opus-5-5). Use `~/bin/py -3.12`, allow rules in
~/.claude-account2/settings.json, and keepsound.js on this audio-less PC (CLAUDE.md, rounds 31-32).

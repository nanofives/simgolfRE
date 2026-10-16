# mainLoop 0x0040f5c0 — control-flow structure (c3fy, 2026-10-16)

Specification of SimGolf's top-level session and frame loop, for the portable reimplementation. Every claim cites an
address in `golf_clean.exe` (image base 0x00400000, VAs). Measured by `log/c3/alt/c3fy_cfg.py` (recursive-descent walk
from the entry, every switch table followed) and read from `log/c3/alt/c3fy_norm.asm` (the disassembly with every
`[esp+X]` rewritten as a frame offset `[F+X]`, F = esp after the prologue's `push edi` at 0x0040f5e1). NO-GUESSING:
only what the code shows; unknowns are marked and filed in `UNCERTAINTIES.md`. Dependency list for the port:
`log/c3/c3fy_deps.tsv`.

## Extent
- ONE contiguous body **0x0040f5c0..0x00421615** (73,813 bytes, 21,541 instructions, 27 switch tables at
  0x00421618..0x00421b23, one `ret` at 0x0040fb04). `re/functions_ghidra.tsv` records only 8,035 bytes in two ranges
  (0040f5c0-00410896; 0041094f-004115dc): Ghidra stopped at the first switch `jmp [ecx*4 + 0x421634]` at 0x004115d5, so
  65 KB of the loop lay outside every function in the master. The whole body is one function (the only `ret` and the
  SEH frame span all of it).
- Called once, from `gameMain` 0x0045baf0 at 0x0045bf48. Convention: `__cdecl`, no stack arguments (plain `ret`).

## Frame and SEH
- Prologue 0x0040f5c0..0x0040f5e1: `mov eax, fs:[0]; push -1; push 0x004b7372; push eax; mov eax, 0x1238;
  mov fs:[0], esp; call 0x004a6070 (__alloca_probe, 0x1238 bytes); push ebx; push esi; push edi`. The SEH record is
  {state -1 at entry_esp-4, handler 0x004b7372 at -8, prev fs:[0] at -0xc}; handler 0x004b7372 runs the one unwind
  action (destroying frame objects). One further `push ebp` at 0x0040f982 (reached once, on the boot path) creates a
  phantom frame slot that lives for the whole run; the epilogue 0x0040faec..0x0040fb04 pops ebp, edi, esi, ebx,
  restores fs:[0] and `add esp, 0x1244`. So F = entry_esp - 0x1250.
- SEH scope state byte at **F+0x124c** (0 for the normal scope, 1 at the theme-pack preview in the menu, 2 at the
  championship preview): set at 0x0040f672, 0x004213af, 0x0042159f, 0x004213f5, 0x004215e5, 0x0040fadc.
- Frame objects the handler can destroy: the three title Surfaces are global, not frame; the frame holds a
  Cache482fd0 at F+0x2b0 (ctor 0x0040f663, dtor 0x0040fae7 via 0x00483010), the end-of-year report Image at F+0x36c,
  the menu preview Images at F+0xe4c and F+0xb94 (ctor 0x00474ae0, dtor 0x00474c40), and the WIN32_FIND_DATA at
  F+0x1104. Frame slot catalogue: `log/c3/alt/c3fy_frame_slots.txt`; slots read before any write (seeded by the frame
  the first time): `log/c3/alt/c3fy_uninit.txt` (F+0x48, 0x4c, 0x84, 0x90, 0xac, 0xb4, 0xbc, 0xcc, 0xe0, 0xe4, 0xe8,
  0x12c, 0x16c, 0x180, 0x184).

## Top-level state machine
```
entry 0x0040f5e2
   |
   v
[SETUP] 0x0040f5e2..0x0040f9f7  (one time)                            -- jmp 0x00420e35
   |
   v
[TITLE MENU] 0x00420e35..0x00421610  <----------------------------+   -- loops to 0x00420e35 (theme-pack preview),
   |  jmp 0x0040f9fc (carries the selection in ebx)               |      jmp 0x0040f9fc on a chosen mode
   v                                                              |
[SESSION DISPATCH] 0x0040f9fc..0x0040fc24                         |
   |  ebx == -2  -> [QUIT] 0x0040fa29..0x0040fac8 -> RET 0x0040fb04 (mainLoop returns to gameMain)
   |  load fails -> jmp 0x00420e35 ------------------------------>+
   |  otherwise  -> fall to the frame (0x0040fc24)                |
   v                                                              |
[FRAME] 0x0040fc24 (rebuildTileCoverage) / 0x0040fc29 (head) .. 0x00420e35/0x0040fc29/0x0040fc24
      per-frame update and render; the command/mouse dispatch at the end loops to the frame head,
      to 0x0040fc24 (rebuild tiles first), or back to the menu (0x00420e35) ----------------------->+
```

## SETUP 0x0040f5e2..0x0040f9f7 (reimplementation `Setup`, shim/src/re/c3fy.cpp)
One-time boot. In order: `playSound(0x80,0x64,0,0,0)` 0x0040f5ef; copy the 0x114-dword tile-name table 0x004c1a40 ->
0x00578350 (0x0040f60e); create the three title Surfaces 0x00568d10 (800x600), 0x00587ae8 (800x600), 0x005a7148
(0x8c x 0x1a4) with `Surface_createFull` (0x0040f61f/3b/57); construct Cache482fd0 at F+0x2b0 (0x0040f663); fill the
title-button geometry word tables at F+0x150 and F+0xf4 and the five menu-label string pointers at F+0x1f4..0x214
(0x0040f67d..0x0040f7b6); `setDrawTarget(Root, 0x0051a028, 0,0,0)` (0x0040f7c1); load `interface\TitleBase.pcx` into
0x00568d10 (0x0040f7de) and blit it to the root Surface (0x0040f80e); draw the 7 menu items (Image_draw from
0x0053b7e0, stride 0x58, highlighted variant +0x2c for item 6) and 5 labels (0x0040f813..0x0040f880); seed the title
ticker RNG `[0x00822c78] = Random::range(0xffff)` and set `[0x005a5a00] = [0x00822c88] = -1` (0x0040f898..0x0040f8bb);
`loadTopScores` (0x0040f8c0); if `Themes\Default.pro` exists, `loadThemeFile(Default.pro)` (0x0040f8ca..0x0040f8e4);
course type `[0x005a34e0] = 0`, theme name 0x00567328 = "Standard" (0x0040f8f0..0x0040f910); `loadInfoScreenArt`
(0x0040f912); `showLoadingScreen(0)` (0x0040f919); `Window::refreshFull(0x00519a60, 0)` (0x0040f928);
`loadInterfaceArt(1)` (0x0040f92f); reset the ticker state 0x0058dd34..0x0058dd4a to -1 and scatter 0..15 by the order
table 0x004c2d28 (0x0040f939..0x0040f977); copy the two ticker strings and the empty default (0x0040f983..0x0040f9c9);
`FindFirstFileA("saved games\\&AutoSave1.sve", F+0x1104)` (0x0040f9f1); jump to the menu.

## TITLE MENU 0x00420e35..0x00421610 (reimplementation `Menu`, part MV)
Resets the session (SetDrawTarget, `[0x00834170]=0`, `[0x0059e7b8]=0x40000000`, buildGolferBioText, initRecords, two
8-dword memsets at 0x005a5a04 / 0x00542f00), redraws the title and the 7 items, then a hover/click loop: project the
cursor into the root window 0x00519a60 (View_toLocal 0x0047ab50), pick the nearest of 6 hotspots with distance
0x0040acd0, draw the highlight, Window::refreshFull, read the mouse-down word `[0x00822d68]` and the selection; on a
click it runs the jump table **0x00421b04** (index = selection + 2, 0..7): the selection maps to 0, 1, 4, 2, 3 or -2.
Selection 4 = Select A Theme -> `showThemePackSelect` + a preview Image at F+0xe4c (SEH state F+0x124c=1), loop to
0x00420e35. Selection 3 = Play a Championship -> set the championship flags (`[0x571fd4]=0x186a0`, `[0x59e7b8] |=
0x1000000`, `[0x5a6364]=0x11`, `[0x543cfc]=0x3fff`, `[0x822c88]=1`). Selections 0/-1/2 build the difficulty prompt and
run `selectDifficultyScreen` 0x0043a400 (0x004214fb) when applicable; mode 5 shows a second preview Image at F+0xb94
(SEH state F+0x124c=2). Selection 2 = Sandbox Mode sets `[0x5a34e0]=0xff` and `[0x59e7b8] |= 0x4000000`. Selection 6
(set by the themes path) -> `showLoadingScreen(-1)` + `seedCourseTable` + `jmp 0x0040f9fc`. The menu carries the chosen
mode in ebx to the session dispatch. (Full per-step detail: part MV notes.)

## SESSION DISPATCH 0x0040f9fc..0x0040fc24 (reimplementation `Session`, part SV)
Reads the mode `[0x00822c88]`, calls `loadThemes(mode != 6)` and `startSoundSlot(0x80)`. If ebx == -2 it runs QUIT
(below) and mainLoop returns. Otherwise per-session setup: `[0x00567afc]=5`, `[0x00822d8c]=-1`; if the sandbox flag
`[0x0059e7b8] & 0x4000000` is set it calls `startNewCourse` (0x0040fb25) and, on 0, clears the flag and returns to the
menu, else `loadWorldAssets` and enters the frame; if mode == 6 it calls `loadGameBrowserScreen(1,0)` (0x0040fb49) and
returns to the menu on 0, else `loadThemes(1)`, `loadWorldAssets`, enter the frame; otherwise the normal new-course
path (`courseActionMenu` 0x0046f550 via thunk 0x00470a00; on -1 return to the menu; else `generateRandomCourse`,
`rebuildHeightfield`, set the clock/round globals) and enter the frame. (Detail: part SV notes.)

### QUIT 0x0040fa29..0x0040fac8
`startSoundSlot(0x2d)`, `waitTicks(0x3c,0)`, write `top10.sve` (the 0x618-byte block at 0x00541ce0, name from
0x004c52ac into 0x0051a068, via __open 0x004a5d48 / __write 0x004a5b58 / __close 0x004a5a78), `promptAndSaveFile`,
`playAudioFile(SMSG_closingfinal.bik, 1)`, `pumpLight`, `playSound(0x81,0x64,0,0,0)`, `showCreditsScreen`,
`startSoundSlot(0x81)`, destroy Cache483010 at F+0x2b4, epilogue, `ret`.

## FRAME preamble 0x0040fc24..0x00410604 (reimplementation `FramePreamble`, part SV)
0x0040fc24 is `rebuildTileCoverage` (run when the frame is re-entered via 0x0040fc24, i.e. the tile grid changed);
0x0040fc29 is the frame head: read `timeGetTime` through the IAT slot [0x004ba21c] into F+0x1ec; clear the two 20-dword
arrays 0x005a8c38 and 0x00543ca0; per placed-object (0x0058bcb8..0x0058ccb8 step 0x10) clamp a per-type counter into
those arrays and set the active mask 0x005a6370; under `[0x0059e7b8] & 0x400000` call `customizeCharacter`; scan the
golfer type table counting members and choosing a story pair (`pairGolfersWithStory` 0x0045de80 at 0x0040fe46); the
date / membership / story block 0x0040fe6c..0x00410399 (`updateMembership` 0x00421bc0, the magazine/news text built with
__itoa into 0x0051a068, `startMessage`, a sound); fill the 24 report-label pointers at F+0x218..0x274 and F+0x238; the
terrain refresh (`updateTerrainRender` 0x004498a0 twice around `applyCourseType`/`refreshTerrain`, `Snd::status`); the
camera-moved test that clears the per-tile caches 0x00055eb40 / 0x00567b34 when the camera position
[0x004c2ba0]/[0x004c2ba4]/[0x004c2844]/[0x005685f4] changed; `resetSpriteQueue`; and the per-frame setup
`[0x004c2840]`, F+0x40, F+0x30. (Detail: part SV notes.)

## FRAME body: the phase chain (parts P1..P7, H1, H2)
The per-frame region after the preamble is a straight dominator chain (`log/c3/alt/c3fy_spine.py`); each phase exits
only to the next, so each is a single-entry region. The reimplementation calls them in the original's order. Ranges and
roles (detail and per-callee order in each part's notes):

| part | range | role |
|---|---|---|
| P1 | 0x00410604..0x0041234e | per-tile ground pass 1: for every visible tile, the rotation-quadrant draw (switch 0x00421618 on the camera rotation [0x005685f4]) and the tile-edge markers (switch 0x00421634); fills the sprite queue with terrain cells |
| P2 | 0x0041234e..0x0041353d | per-tile ground pass 2: wall / edge / path sprites (switches 0x00421668, 0x004216a0 on tile type) and scenery labels |
| P3 | 0x0041353d..0x00415847 | the shot overlay (prepareShot, map lines, angle), snapshotTables + drawWorldObjects, the per-object pass (switches 0x004216d4/e4/f8, 0x0042170c), the per-golfer pass (names, comments, thought bubbles, minimap) |
| P4 | 0x00415847..0x004175f2 | flying ball, sprite-queue render, HUD labels, the minimap and the hole/score panels, the par/yardage readout (the one x87 site: fild/fsqrt/__ftol at 0x0041732f) |
| P5 | 0x004175f2..0x00418bc7 | the simulation tick: updateGolfers / updateWalkers / updateRollingStats, the date advance, random ambient events (switch 0x0042177c on a Random::range result), autosave and the end-of-year/report screens, the status bars, saveCourseJpg, the pixel readback |
| P6 | 0x00418bc7..0x00419d06 | the on-screen window frame and HUD panels; the display present slot [[0x0083ad50]+0x9c] (0x00418cd1), GetAsyncKeyState (0x00418d7d); dispatch to the info / building / employee / golfer panels and the cursor-tile readout |
| P7 | 0x00419d06..0x0041c0c5 (+ 0x00420d83..0x00420e0a) | the build-tool cursor preview and placement evaluation (switches 0x004217a8 on the active tool, 0x004217f4, 0x00421818); landmark names and per-tool sprite previews |
| H1 | 0x0041c0c5..0x0041ecbb | input fetch (pumpMessages, setDirtyFlag) and the keyboard-command dispatch (the 57-entry switch 0x00421838 keyed by the key code through byte table 0x0042191c): report hotkeys, camera, pause, the per-frame wait (timeGetTime via [0x004ba21c], waitTicks) |
| H2 | 0x0041ecbb..0x00420e35 (except 0x00420d83..0x00420e0a) | the mouse/tool action: GetAsyncKeyState (via esi, [0x004ba1f0]), the build/demolish/hire/pick actions (switches 0x00421aa4 on the tool, 0x00421ad4 on the placed-object type), money checks (canAfford), stampCourseTiles, rebuildTileCoverage/Heightfield; the quit-save at 0x00420e0a leaves to the menu |

The frame-dispatch result (from H1, or H2 when H1 falls through) is one of: loop to the frame head 0x0040fc29, loop via
0x0040fc24 (rebuild tiles first), or return to the title menu 0x00420e35.

## Jump tables (0x00421618..0x00421b23)
Each `jmp [reg*4 + T]` is bounded by a preceding `cmp`/`ja` (or a byte index table). Index expression and site:

| table | site | index (bounded) | role |
|---|---|---|---|
| 0x00421618 | 0x0041088f | `[0x005685f4]` camera rotation, 0..6 (>6 default) | the four quadrant step masks F+0x84/0x90/0xb4/0xcc; even rotations 0/2/4/6 distinct, odd keep previous |
| 0x00421634 | 0x004115d5 | `ecx & 0x1f`, 0..4 (>=5 default) | colour of a tile-edge marker (Widget_writePixel) |
| 0x00421648 | 0x00411c9a | 8 entries | P1 sub-switch (tile variant) |
| 0x00421668 | 0x004124c2 | `typeAttr[tile] - 0xc`, 0..6 | P2 wall/edge sprite |
| 0x00421684 | 0x00412902 | 7 entries | P2 sub-switch |
| 0x004216a0 | 0x00412a71 | byte 0x004216c0[type], 0..0x12 | P2 tile render variant |
| 0x004216d4 | 0x004135aa | edx 0..3 | P3 (0x4135b1/c5/d9/ed) |
| 0x004216e4 | 0x0041420f | ebx 0..4 | P3 (case 2 enters case 1 partway; case 4 sets only F+0x10) |
| 0x004216f8 | 0x00414388 | ecx 0..4 | P3 |
| 0x0042170c | 0x00414709 | eax 0..4 | P3 |
| 0x00421720 | 0x00416ec4 | 8 entries | P4 |
| 0x0042176c | 0x00416efb | 4 entries | P4 |
| 0x0042177c | 0x00418322 | `Random::range(...) & 0xffff`, 0..6 | P5 random ambient sprite |
| 0x00421798 | 0x00418fa5 | 4 entries | P6 |
| 0x004217a8 | 0x0041a0f9 | `[0x004c2854] - 1` active tool, 0..0x12 | P7 tool preview (case map in P7 notes) |
| 0x004217f4 | 0x0041b6db | `clamp(x,-4,4) + 4`, 0..8 | P7 |
| 0x00421818 | 0x0041ba0a | `[F+0x18] - 1`, 0..7 | P7 |
| 0x00421838 | 0x0041c5b2 | byte 0x0042191c[key-9], key 9..0x7a | H1 keyboard command dispatch (57 cases) |
| 0x00421990 | 0x0041d1a5 | 4 entries | H1 |
| 0x004219a0 | 0x0041d22f | 4 entries | H1 |
| 0x004219b0 | 0x0041de29 | 17 entries | H1 |
| 0x00421a50 | 0x0041df2a | 7 entries | H1 |
| 0x00421a6c | 0x0041e1b8 | 7 entries | H1 |
| 0x00421a88 | 0x0041e704 | 7 entries | H1 |
| 0x00421aa4 | 0x0041f4ed | byte 0x00421ac0[tool], 0..0x13 | H2 tool click action |
| 0x00421ad4 | 0x00420684 | byte 0x00421af0[typeAttr], 0..0x11 | H2 placed-object action |
| 0x00421b04 | 0x0042134a | `sel + 2`, 0..7 | menu selection -> mode {0,1,4,2,3,-2} |

## Globals
675 distinct data addresses (217 written). Full table with width and R/W per address: `log/c3/alt/c3fy_globals.tsv`;
dependency rows for the port: `log/c3/c3fy_deps.tsv`. The named ones (from re/names/*_globals.tsv) that mainLoop reads
or writes:
- `[0x0059e7b8]` g_flags: session mode bits, read/written throughout (0x4000000 sandbox, 0x1000000 championship,
  0x8000000 ..., 0x400000 ..., 0x200000 story advance enabled).
- `[0x00822c88]` session mode (-1 none, 1 championship, 2 sandbox, 5/6 ...); `[0x00822c90]` g_screenHeight,
  `[0x00822c8c]` g_screenWidth; `[0x00822c78]` title ticker seed; `[0x00822d9c]` the RNG seed (Random::range).
- `[0x00834170]` g_date (year = date/8192 + 2001, month = date/1024 % 8).
- `[0x004c2840]` tile-count, `[0x004c2844]` g_zoomScale, `[0x004c2ba0]`/`[0x004c2ba4]` camera tile x/y, `[0x005685f4]`
  g_cameraRot.
- `[0x00571fd4]` cash/100, `[0x005a34e0]` course type, `[0x00567afc]` game-state (5 = in course), `[0x00567328]`
  theme name, `[0x0051a068]` the shared text buffer.
- `[0x0083ad50]` the graphics device; `[0x004c1570]` the current draw target (`Root`).

## Findings (the original)
- When `[0x004c2848] == 0x280`, 0x0041b209 jumps into the loop at 0x00420d83 (canStep / cellAt / addSprite) that has no
  exit edge (0x00420e08 jumps back to 0x00420d96, 0x00420daf loops): the game hangs there. [UNCERTAIN U-00xx: the
  condition `[0x004c2848] == 0x280` is a build-tool drag state; whether it is reachable in play is not determined.]
- P7: 0x0041b61d divides by `rateTile(...) * 3` (halved when `[0x0058f330] == 4`), faulting on a 0 result; 0x0041ac30
  reads `byte [F+0xf0+eax]` with eax in -3..3, below the 4-byte table at F+0xf0 for a negative index; 0x0041ae28 adds
  `[0x005a34f0] % 3` to F+0x180, which can be read before written.
- (More per-phase findings in each part's notes; collected into the batch notes log/c3/c3fy_notes.txt.)

## Verification status (c3fy, 2026-10-16)
The reimplementation is `shim/src/re/c3fy.cpp` (one function per phase in the original's order; naked entry rebuilding the
SEH frame). Because mainLoop is entered once and only returns on the quit path, it has no per-call path-1 A/B; it is
verified by the whole-game scenario A/B (`re/frida/scenario_ab.py`, mainLoop ON vs OFF, every other hook off,
instance `instances/c3fy`):
- **sandbox_basic: GREEN** — state (every `.sve` global + the RNG seeds) and all frames byte-identical to the original
  at all 10 checkpoints (menu -> Sandbox -> Monterey, 40 s).
- **negctl control: RED** — one extra `Random::range` per frame (`shim/src/re/c3fy_negctl.cpp`) diverges from the first
  in-game checkpoint; so the A/B detects a wrong body and the reimplementation passes where the control fails.
- **course_season: RED** — byte-identical through the hole build, then a 2-byte divergence at the hole_open checkpoint
  (hole-1 +0x201 at 0x00575eb9, character-66 +0x163 at 0x004df79b) compounds to golfer/story/RNG divergence and a
  mid-season crash. A residual bug in the hole-open / golfer-simulation path (U-0008); the reimplementation is not yet
  byte-faithful there, so c3fy.cpp is **not** linked into `shim/re_batches.txt` (hooks default ON, so a committed live
  hook would run the buggy loop in normal play). mainLoop stays C2 (held, `re/c3_hold.tsv`): the C2->C3 gate requires a
  path-1 A/B, which is impossible for a once-entered loop.

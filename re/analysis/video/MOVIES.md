# Movies and FLIC animations of SimGolf (S8.1, static)

Tables: `re/port/movies.tsv` (2 movie rows, 118 excluded rows each with its reason), `re/port/flics_census.tsv` (14 FLIC directories).
Derivation and completeness check: `re/tools/movie_inventory.py [--check]`, test `tests/test_movie_inventory.py`.
Decision D2: no Bink decoder this round, so each movie below has a documented skip path.

## 1. The two Bink movies

| movie | file in original\Flics (and extracted\cab) | BIK header | caller, call site | path string |
|---|---|---|---|---|
| intro | `SMSG_IntroFinal.bik`, 10,110,596 B | `BIKi`, 800 x 600, 825 frames at 15/1 fps (55 s), 1 audio track | showTitleScreen 0x0043cd70, `call 0x0043cce0` at 0x0043cd97 | 0x004c8a0c `./flics/SMSG_introfinal.bik` |
| closing | `SMSG_ClosingFinal.bik`, 6,474,448 B | `BIKi`, 800 x 600, 589 frames at 15/1 fps (39 s), 1 audio track | mainLoop 0x0040f5c0, `call 0x0043cce0` at 0x0040faa4 | 0x004c6bb4 `./flics/SMSG_closingfinal.bik` |

No other module opens a movie: jgld.dll, Terrain.dll and sound.dll have no binkw32 import and no movie string. The paths
are relative to the process's working directory (the install root), written lower case; the files are upper/mixed case on
disk and the lookup (FindFirstFileA through fileExists 0x004378a0) is case-insensitive.

Path to `BinkOpen`: caller pushes (path, immediate) -> playAudioFile 0x0043cce0 (`[esp+8]` path, `[esp+0xc]` immediate)
-> fileExists -> BinkPlayer::setTarget 0x00487050 (window from display slot +0x20) -> BinkPlayer::play 0x00487090
(the only caller of play; `_BinkOpen@8` through slot 0x004ba250 at 0x004870cd, flags 0x1000000) -> BinkPlayer::close 0x00487060.

Triggers:
- intro: gameMain 0x0045baf0 calls showTitleScreen at 0x0045bf0f, once per process, before the title menu. showTitleScreen
  plays the movie first (0x0043cd97), then builds the button surfaces. `push ebx` at 0x0043cd91 with `xor ebx, ebx` at 0x0043cd8e
  gives immediate = 0.
- closing: mainLoop's QUIT block 0x0040fa29..0x0040fac8, after the `top10.sve` write and promptAndSaveFile, then pumpLight
  0x0045c150, playSound(0x81, 0x64, 0, 0, 0) and showCreditsScreen. `push 1` at 0x0040fa9d gives immediate = 1.

Cancelling (the `immediate` argument): playAudioFile stores a deadline in 0x005aa774. With immediate 0 (intro) it is
timeGetTime + 0x1b58 (7 s, 0x0043cd00); with immediate 1 (closing) it is 0 (0x0043cd0c). rootWin_onPoint 0x00465610,
rootWin_onUnpoint 0x00465650 and rootWin_onKey 0x00465750 clear the byte 0x0083ab9c (the BinkPlayer::play loop's run flag,
set to 1 at 0x00487124) when the deadline is 0 or timeGetTime is above it. So the closing movie ends on the first key or click,
the intro only after 7 s of playing.

### Skip paths (what the original itself does, and what the port reproduces without a decoder)

| situation | the original's behaviour | evidence |
|---|---|---|
| file absent (`fileExists` = 0) | playAudioFile returns at once: no deadline written, no window or repaint, no Bink call; the caller continues | `test eax, eax; je` 0x0043ccee / 0x0043ccf0 |
| `_BinkOpen` answers NULL, or the handle's error word at +0x1c is non-zero | BinkPlayer::play answers 1 before any buffer or cursor change; playAudioFile still runs the one input pump pumpInput483bd0 0x00483bd0 (0x0043cd2c), the root repaint Window::paint 0x004808c0 with 0x80007fff (0x0043cd3b), BinkPlayer::close (a NULL handle is skipped, `je` 0x0048706b) and clears the deadline (0x0043cd64) | 0x004870d8 / 0x004870de..0x004870e3; this is the path the shim's `skip_intro` takes for the intro (shim.cpp `h_BinkOpen` returns NULL for a name containing `introfinal`) and therefore the path of every recorded run |
| no target window (this+0xc = 0) | BinkPlayer::play answers 7 and does nothing | 0x00487093..0x0048709a |
| binkw32.dll missing | the process does not start (16 static imports, descriptor name at 0x004bf430); not a runtime skip | import table |

For the port: the two cheapest skips are the file-absent exit (nothing else runs) and the NULL-open exit (pump, repaint, close).
The recorded corpus uses the second one for the intro; the tick backend already drops playAudioFile for the closing movie
(`tick.cpp` kDrops, "pumps dispatch no recorded input in the corpus"). Replaying the original's own pump and repaint after a
skipped movie is part of the NULL-open exit, so a faithful skip must keep them.

## 2. Bink API surface (16 imports, not nine)

`_BinkBufferClose@4` 0x004ba224, `_BinkDoFrame@4` 228, `_BinkBufferLock@4` 22c, `_BinkCopyToBuffer@28` 230,
`_BinkBufferUnlock@4` 234, `_BinkGetRects@8` 238, `_BinkBufferBlit@12` 23c, `_BinkNextFrame@4` 240, `_BinkOpenDirectSound@4` 244,
`_BinkSetSoundSystem@8` 248, `_BinkSetIOSize@4` 24c, `_BinkOpen@8` 250, `_BinkBufferOpen@16` 254, `_BinkBufferSetOffset@12` 258,
`_BinkWait@4` 25c, `_BinkClose@4` 260. Real uses (all inside the BinkPlayer class 0x00487000..0x00487204):
play 0x00487090 (SetSoundSystem with the address of the OpenDirectSound slot when the sound argument is non-zero, SetIOSize 0x9c4000,
Open, BufferOpen, BufferSetOffset, Wait), frame 0x00487180 (DoFrame, BufferLock, CopyToBuffer, BufferUnlock, GetRects,
BufferBlit, NextFrame; returns false when the frame counters at BIK +8 and +0xc are equal), close 0x00487060 (Close, BufferClose).
The other 73 operand sites of the slots lie outside every function, in the SafeDisc code-cave padding
(`re/analysis/data/safedisc_padding.md`, opcode `ff 15`, 12-17 per slot for the five slots BufferUnlock 15, GetRects 12, BufferBlit 17, NextFrame 17,
OpenDirectSound 12; the 16 in-function sites are the three BinkPlayer methods): nothing reaches them, they are excluded rows.

## 3. FLIC files (`re/port/flics_census.tsv`)

No FLIC is a cutscene or a full-screen movie. All 1,893 `.flc` files of original\Flics are `0xAF12` FLIC files (0 bad magic) whose
largest frame is 442 x 377 (columns `max_width` / `max_height`; the screen is 800 x 600), and the only
code that opens them is loadFlic 0x0043d740 (821 call sites, all inside loadWorldAssets 0x0043dbe0 and loadInterfaceArt 0x00442180,
no other caller and no stored pointer). loadFlic builds `flics\` + name + `.flc` (strings 0x004c8a64 / 0x004c8a50), optionally the
`Shadow.flc` sibling (0x004c8a58, used when the fifth argument is non-zero and fileExists finds it; the call at 0x0043d896), opens it with flicOpenStream
0x00404120, steps frames with flicNextFrame 0x00404090 and blits each into the 0x1e0 x 0x1e0 sheet Surface 0x005aa778
(Surface_blit / Image_draw), with the guard "Too many flics!" (0x004c8a3c) when the cumulative size reaches 0xdd40 (0x0043d9ef). They are the sprite animations
of trees, buildings, golfers and so on: S3's. Directory per call-site counts are in the TSV; 6 call sites in loadInterfaceArt
(0x00441e13..0x00441e7f) name `trees\Tropic\Tree Cerc\...` and `...\Tree Drac\...` with a space where the install's directories are
`Tree_Cerc` and `Tree_Drac` (U-1701). Files no call site names (386 of 1,893, e.g. Female `femaleskss_*`) are not loaded
by this binary.

## 4. The movie table family (serviceMovie 0x0049c8e0, table 0x00840698)

What it is: an RIFF / mmio player (not Bink, not FLIC): `movieAdvanceFrame` 0x0049c940 and `movieReadFrame` 0x0049cb90 read
chunks with `mmioDescend` / `mmioRead` / `mmioAscend` (the three winmm slots 0x004ba208 / 20c / 210) from a handle at object +0xbc,
queue frames in a FIFO at +0x668 (`listPush` 0x004a4db0) and pace them against timeGetTime or a clock object at +4;
`movieSelectStream` 0x0049cf50 scans 0x10-byte stream records for the type at +0x1a4. serviceMovie reads the pointer table
0x00840698 (19 entries, model `g_movieIds`) for `[0x008406e4]` entries (`g_movieCount`) and calls serviceMovieFrame on each.
It services nothing in this build: the only references to 0x00840698..0x008406e4 in any section are the three operands inside
serviceMovie itself (0x0049c8e1, 0x0049c8ee, 0x0049c8fa), the .data tail is zero-initialised, and winmm has no `mmioOpen` import, so
no code can produce the handle the frame functions need. The count stays 0 and serviceMovie returns at 0x0049c8ea on every call
from showScreen, hideScreen, redrawAll and frameTick483cd0. The port can drop it (U-1702 records the one static gap).

## 5. Excluded strings

`$bink` (0x004d21a4) is a credits.txt line marker and `bink64.pcx` (0x004d21c0) a logo image, both used by showCreditsScreen
0x0044b9c0 (0x0044bc55, 0x0044ba7e); the `_Bink...` names and `binkw32.dll` (0x004bf2ea..0x004bf430) are the import table;
`golf_v100_clean.exe` (retail v1.00, reference only), `winmm.dll` (our shim) and `simgolf_shim.log` repeat the movie names.

# Sid Meier's Gettysburg!

Sid Meier's Gettysburg! (Firaxis, 1997). Where the evidence for its source lives, and what it gives the SimGolf (SMSG) reconstruction. Pointers only: no source, no binaries.

Engine: JACKAL.LIB `4.00`, static, with the graphics and window code still inside it (no jgl DLL yet); the sound API client compiled with the game against `sound.dll`; Sid's `lee` game project. Compiler: VC4.2 in 1997 (`_MSC_VER=1020` in `vc40.idb`; linker 4.20, the December 1997 demos linker 5.02); VC6 for the 2002 rebuild (linker 6.0).

## Builds

| artifact | version / date | md5 | gives | source |
|---|---|---|---|---|
| `Lee.exe`, demo `smgdemo.exe` | 1997-12-02, linker 5.02, `v 7.00f` | `da59c7fd76d35c04a215943224e40829` | no debug record; game 0x401000-~0x4547a0, JACKAL ~0x4547a0-0x49c180, then CRT; optimized, but two /Od runs inside JACKAL, 0x45f583-0x466430 (110 frames) and 0x499130-0x49afe4 (24) | archive.org/details/smgdemo |
| `lee.exe`, demo `smgdemo_c.exe` | 1997-12-10 | `34d1d297f3582586646289b075fbea3b` | the same two /Od runs; `TEMP.BAT` (`\games95\lee`) | firaxis.com/downloads/demo/smgdemo_c.exe (Wayback 20021204124739); SMAC 1.0z disc |
| `Lee.exe`, demo `smgdemo_u.exe` | 1997-12-18 | `d1fb5d468eeff5c552e9dadfdaea3ff4` | the same two /Od runs; `TEMP.BAT` | firaxis.com/downloads/demo/smgdemo_u.exe (Wayback 20021219052329) |
| `SMGDEMO.EXE`, LEVEL CD-ROM Romania 1/1998 | 1998-01 | Lee.exe = `smgdemo`'s | a copy | archive.org/details/LEVEL0198R |
| `vc40.idb` (3 demos) | VC4.2 | `f4e94fdff7d30fd0534c5a6525cc9a9a` | see [vc40.idb](#vc40idb) | demos |
| `Script1.rc`, `Script1.res` (3 demos) | | `5af6d51ec7b92a35b8b32634362366e9`, `eca5180e67c7e88658ebc5175928ad44` | the resource script: one icon, `generals.ico` | demos |
| `lee.exe`, retail 1.00F | 1997-09-25, linker 4.20, `v 1.00f` | `7d7e1a2ae1a007a192f91859b2247293` | release: no debug record, no /Od run (patches 1-3 the same) | disc |
| `LEE.EXE`, patch 1 | 1997-10-21, `v 2.00f` | `98e34c553a0b56c35b837f7e2b3fe969` | release | archive.org/details/README1 |
| `LEE.EXE`, patch 2 | 1997-11-06, `v 7.00f` | `b603445ad8d3c3866588f1f6f50417b9` | release; also the Civil War Collection's `SMG/lee.exe` | firaxis.com/getty/files/smgpatch2.exe (Wayback 20010417134909) |
| `Lee.exe`, patch 3 | 1998-03-13, `v 7.03f` | `d3a027d4229cefd821c27aeec70e1fc7` | release | firaxis.com/downloads/patch/smgpatch3.exe (Wayback 20021227182118) |
| `lee.exe`, Win2000/XP update | 2002-06-06, linker 6.0, `v 7.00f` | `2e634b942621456a0754016796915de1` | optimized game, /Od JACKAL 4.00 at 0x456e70-0x4966fc (1064 of 1072 frames; the JACKAL.LIB check message is pushed at 0x487995) | firaxis.com/downloads/Patch/SMG_2000_XP_Compatibility_Update.exe (Wayback 20051016075051); same file in the SMCWC update |
| `mpgetty.dll` | 1997-09-17, linker 4.20 | `ff2b44da380c0de17c165e4b99ea78f5` | /Od 0x10002870-0x10067c06 (1992 of 2006 frames); JACKAL 4.00 + sound client (`4.00`, `sound.dll`, `Voice Rx/Tx`); linked as `c:\lee\mpgetty.dll` | disc, CWC |
| `autolee.exe` | 1997-09-22, linker 4.20 | `a7e678052b1f5b8975e94e9451e8f869` | the disc autorun (`open=autolee.exe`); JACKAL 4.00; Debug PDB path but optimized (no /Od run among its 112 frames) | disc |
| `sound.dll` | 1997-09-22 / 1997-10-13 / 2002-04-18 | `5f81919893591d29e9b92e9f5a02a913` / `a718c276756c1a294f5371a7277dbdd6` / `794604575a9c4c580dec0f64153cbbc5` | 12 exports on the disc, 13 from 1997-10 (adds `get_sound_version`); no debug record | disc / demos, patches 1-3, CWC / XP update |

## Discs

| pressing | image | md5 | lee.exe | source |
|---|---|---|---|---|
| SMG-E1.00F | ISO 1997-09-24 | `e9b24ecb1c7baf07198081e82fd19d6e` | 1.00F | archive.org/details/smg-e-1.00-f_202502 |
| Redump (USA) | ISO 1997-09-24 | `941ce4cba9008e3af1baa4d2cff77330` (zip) | 1.00F | archive.org/details/20250203_20250203_2027 |
| 1998-12 pressing: "2001 EA" CD, Rerelease (En,Fr,De) | ISO 1998-12-14 | `87631dc47d7a56f223ba2a7180a09f4a` (7z), `1ae22f2f5461c6877aaffbdafb765a8c` (zip) | 1.00F, plus `patch/smgpatch3.exe`, `xtras/smgart.exe` | archive.org/details/sid-meiers-gettysburg-2001-ea-firaxis-cd, archive.org/details/20250203_20250203_2027 |
| Civil War Collection 2000, 2001 | ISO 2000-08-07, 2001-06-22, 2001-06-15 | `7d8e23a4233d8de92b3d952b4b5e4dbc`, `3629c7004105688e5fc81092a1ee0bde`, `2e76d98e64e158de3b523929932e5aac` | patch 2 | archive.org/details/Nova_SidMeiersCWC_USA, Nova_SidMeiersCivilWarCollection_USA, smcwc_202101 |

## vc40.idb

| field | value |
|---|---|
| TU | `l:\source\project.cpp` -> `l:\source\debug\project.obj` (the only one recorded) |
| PCH | `-Ycl:\source\project.h`, `-Fpl:\source\debug\lee.pch` |
| flags | `-D_MSC_VER=1020 -D_DEBUG -D_DLL -D_MT -G5 -GX -Gi -Gs4096 -Gy -MD -Ob2 -Og -Oi -Ot -Oy -W3 -Zp8 -ZB64` |
| JACKAL headers | `-Ic:\jackal` |
| include path | `c:\msdev\include`, `\\jason #2\f\msdev\include`, `e:\msdev\include`, `c:\program files\devstudio\vc\include` |

## Names

| source | count | gives |
|---|---|---|
| CodeView, PDB, map | 0 | none in any Gettysburg file; a 2026-10-11 sweep (617 PEs) found one embedded CodeView, SimGolf's demo `sound.dll` |
| NB10 pointers | 2 | `C:\mpgetty\Debug\mpgetty.pdb` (signature 0x33f31fa4, age 76), `C:\autolee\Debug\autolee.pdb` (0x3412f47c, age 67); not shipped |
| `sound.dll` exports | 12 / 13 | `create_device` / `delete_device` of `Dll_Midi_Device`, `Dll_Wave_Device` and `Dll_Wave_In_Device`, `WaveInDeviceMgr::call_back`, enum `SNDERR`, `create_sound`, `delete_sound`, `release_sound`, `init_sound_timer`, `acm_stream_callback`, and from 1997-10 `get_sound_version`; SimGolf's exports the same 12 without `acm_stream_callback` |
| `mpgetty.dll` exports | 10 | `MPOpenOffer` (ordinal 1) to `MPRegisterCallback` (10) |
| strings | | the JACKAL.LIB check (`4.00`, "Brian Reynolds", "William Robinson"); `fifo::add`; `vrx_list`, `last_time`, `current_time` (Net::check_polling) |
| demo `jackal.txt` sections | 24 | 18 of them among SimGolf's 30 |
| Mac traceback, Mach-O | 0 | the Mac name lists cover SMAC, SMAX and Civ III only |

## Code shared with SimGolf

| SimGolf layer | in Gettysburg | measured |
|---|---|---|
| jackal.lib | JACKAL.LIB 4.00: one run after the game code, starting with Heap (golf 0x474780) | 96 of 836 golf engine functions score >= 0.5 against Lee.exe (MSVCRT control: 2); 40-53 confident matches inside the run |
| | dialog keyword table | position for position, renamed (`width` -> `xs`, `title` -> `caption`); grammar `@` / `width=` / `title=` / `%NUM`, SimGolf `#` / `xs` / `caption` / `$NUM` |
| jgl | not yet split: `JackalClass`, the PCX tag `monkey sweats on a tuesday` | tag on 86 of 88 demo PCX files |
| sound client | compiled with the game: demo 0x434900-0x437000 (`sound.dll` at 0x4a6770, `Voice Rx` at 0x4a677c) | SimGolf: inside the static library, sound.cpp from 0x483e90 |
| game code (`golf.exe/src`) | Sid's framework: Geometry.cpp, InputPoll.cpp, InputWait.cpp, Screen.cpp, ListDialog.cpp, ScreenText.cpp, GameMain.cpp's wait loop, save-block transfer | XP rebuild, masked whole body (golf -> lee): ApproxDistance 0x467170 -> 0x438fd0, GetDirection8 0x4671a0 -> 0x439000, FixedPointSin 0x467270 -> 0x439210, CalculateAngle 0x4672d0 -> 0x41e370, InputWait.cpp's ProcessMouseMessages 0x45c030 -> 0x41e480, Screen::on_right_double_click 0x465840 -> 0x436b70 |
| | the credits roll: 11 strings, 3 blanks, -1; no reader | demo 0x4a6798, patches 1-3, XP; SimGolf 0x4e192c |
| | project tree | `l:\source\`, `\games95\lee`, `c:\lee\`; Antietam reused it (`C:\Doug Antieitam\lee\debug\Antietam.pdb`) |
| assets | `BLDG.PCX` | SimGolf's `bldg.pcx` is this sheet: 800x600, palette differs in entry 255 only |

## BreakAway licensees

JACKAL 4.00 games on Gettysburg's engine. Low value for SimGolf.

| artifact | date | md5 | gives | source |
|---|---|---|---|---|
| `Waterloo.exe`, demo | 2001-04-02 | `ffc85b372d7a039472d1e4a0570f41b1` | JACKAL init and the `4.00` check (0x4a0b4c) are /Od; /Od runs 0x4a14c0-0x4f57cf, 246 of 1036 KB | archive.org/download/Waterloo_exe/Waterloo.exe |
| `Waterloo.exe` 1.001, 1.002 | 2001-04-23, 2001-06-13 | `803340c05b9210e5bef2e7eb70761f8c`, `4ee05979fa56bd775d0ffa32e25f853e` | same runs | breakawaygames.com/downloads/waterloo_patch_1_001.exe, _1_002.exe (Wayback 20020212) |
| `AusterlitzDemo.exe` | 2001-11-27 | `657bcd769757b7238d4a3f04f53e0e2e` | same runs, 243 KB | PC Gamer DVD 2001-12, archive.org 2001_12_20_PCG106D0202, `/Games/austerlitz/` |
| `Austerlitz.exe` 2.01 | 2004-12-03 | `899815c08bd80ae9aff64b8496890187` | same runs, 285 KB | breakawaygames.com/downloads/austerlitz_patch_201.zip (Wayback 20060724151950) |
| `BAGLoad.dll`, `BAGSound.dll` | 2001-02 to 2003-12 | `5611135e0a6f0fccb5fb1a39497f64f2` (Waterloo BAGLoad), `8be8ee0fb8bd6456134bc750a6a6ecc9`, `3d5f55524321ceb8cf50e06be8442078` (Austerlitz demo), `ee73139a600f65878d7999d95edc6399` (2.01 BagSound) | Debug builds: debug CRT, `C:\SRC\Water32\BagLoad\Debug\BAGLoad.pdb`, `C:\SRC\Austerlitz\Bag{Load,Sound}\Debug\`; no JACKAL strings | as above |

## What it cannot give

- Symbols. No build of Lee.exe has a debug record; the two PDBs named are not shipped.
- TU names or file order. `vc40.idb` records only the PCH TU, `project.cpp`.
- Byte-identity. 1997 is VC4.2/5, the 2002 JACKAL is /Od; SimGolf is VC6 /O2. Matching is masked similarity.
- SimGolf's branch. Gettysburg is 4.00; SimGolf's JACKAL descends from Alpha Centauri's 10.10. The 2001 jgl rewrite (GDI, 16-bit, PNG, JGLSys) has no counterpart.
- Exact run boundaries. They come from contiguity, flags and string references, and can be off by a file.
- Most of golf's game code. 38 of its 700 game functions have a verified Sid-lineage ancestor, all measured against Antietam.

## Notes

- Copy protection: none found on the Gettysburg media (no SafeDisc, SecuROM or ActiveMARK files or signatures; standard PE sections).
- Full-game media were analysed from images of owned titles (a format-shift), not redistributed. No crack was applied or run.
- Downloads were read as bytes only (objdump, strings, md5sum). Nothing was run.
- /Od ranges: found by frame-prologue idioms, in runs of 15+ frames.

## Related

SimGolf (SMSG). Shared code: [README.md](README.md). Siblings: [Antietam.md](Antietam.md), [AlphaCentauri.md](AlphaCentauri.md), [Civilization3.md](Civilization3.md).

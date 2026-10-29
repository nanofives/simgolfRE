# Sid Meier's SimGolf

Sid Meier's SimGolf (Firaxis Games, with Maxis and Electronic Arts, 2002), as the SimGolf (SMSG) byte-exact
reconstruction found it: where the source information for its four images lives. Pointers only: no source, no
binaries.

## What it is

Four images, all linked by LINK 6.00 (optional-header linker version 6.0):

- `golf.exe`: the game. Release. JACKAL's non-graphics half is linked in statically, from Heap at 0x474780 (SMSG
  calls the library `jackal.lib`, its ancestor's attested name; SimGolf's own file name is not recorded). Also IJG
  JPEG 6a (`6a  7-Feb-96`) and a Bink import library. Imports binkw32.dll, Terrain.dll, KERNEL32, USER32, WINMM;
  loads `jgld.dll` and `.\sound.dll` by name. SafeDisc on every patch build held, 1.01 to 1.03.
- `jgld.dll`: JACKAL's graphics and window half, a DLL since 2000. Debug build. One export,
  `get_graphsy_object_ptr`; window class `JackalClass`; libpng 1.0.5 and zlib (`inflate 1.0.2`).
- `sound.dll`: the sound API (`C:\soundapi`), with LPC-10. Release.
- `Terrain.dll`: the 3D terrain, on OPENGL32 and GLU32. Debug build. 38 exports, all of class `Terrain`.

Compilers, from the Rich `@comp.id` stamps: Visual C++ 6.0 build 8168, VC6's original C2 back end, for golf.exe,
jgld.dll and Terrain.dll (Service Pack 2 ships no compiler; SP3's back end stamps 8447, SP4's 8799, SP5's 8966); build
8966, Service Pack 5, for sound.dll's 34 C++ objects, with LPC-10's C at 8168. jgld.dll and Terrain.dll link Service
Pack 2's LIBCMTD.LIB: its 1,153-byte `realloc.obj` code is at jgld 0x10086000 and Terrain 0x1001abd0 (RTM's and SP3's
are 490 bytes, SP4 to SP6's 1,171).

## Builds

| Image | Linked (UTC) | Size | md5 | Kind | Rich (build x objects) | Debug record |
|---|---|---|---|---|---|---|
| `golf.exe` 1.03, PE part | 2002-03-29 20:34:22 (export stamp `0x3ca4cfce`) | 954,368 | `b68e7ebc61e59a333a0467e0fe129a81` | release, full link: no debug CRT strings, no `/GZ` fills | C 8168 x152, C++ 8168 x109, MASM 6.13.7299 x25, Cvtres 5.00.1720 x1; key `0x15294f1c` | none |
| `jgld.dll` | 2001-12-19 20:23:06 | 1,273,898 | `9fbecb5baa3586947af082483ec25b55` | Debug: debug CRT (`dbgheap.c`, `i386\chkesp.c`), 772 `/GZ` stack fills; incremental | C 8168 x141, C++ 8168 x31, MASM x34, Linker600 8168 x1; key `0xde2226f2` | NB10 `C:\JackDev\Debug\jgld.pdb`, signature `0x3c20f5ab`, age 3 |
| `sound.dll` | 2001-12-18 19:51:58 | 458,752 | `93f8b776945a0c07907136a1984e66fd` | release, no debug CRT; 42 `/GZ` fills, one in each of LPC-10's 42 functions (0x1003a980-0x1004164f); incremental | C++ 8966 x34, C 8168 x35, C++ 8168 x5, SP5 LIBCMT 8047 (C x104, C++ x11), MASM x33, Linker600 8447 x1, DX8 `dsound.lib` 0x25.8755 x3 and `dxguid.lib` 0x1d.8830 x1; key `0x1512b911` | none |
| `Terrain.dll` | 2001-12-14 20:12:29 | 454,719 | `31bfda31e8214f6f00e73e5dc52851b8` | Debug: debug CRT and iostream (`dbgheap.c`, `streamb.cpp`), 169 `/GZ` fills; incremental | C 8168 x127, C++ 8168 x27, MASM x33, Linker600 8168 x1; key `0x61905ee0` | NB10 `C:\Projects\3DTerrainLowPoly\Debug\Terrain.pdb`, signature `0x3c18cb6a`, age 23 |
| `jgl.dll`, beside jgld.dll | 2001-12-19 23:25:13 | 393,216 | `c5cf40447504c9c2086d583a362cef34` | release; golf.exe 1.02 and 1.03 name only `jgld.dll` | C 8168 x105, C++ 8168 x31; key `0xc6ec886e` | none |

- No PDB was shipped. golf.exe and sound.dll have no debug directory.
- jgld.dll: `__FILE__` `c:\jackdev\jglsprite.cpp` and `C:\JackDev\jglsprite_8_16c.cpp`; `libpng version 1.0.5 - October
  15, 1999`.
- Terrain.dll: `__FILE__` `C:\Projects\3DTerrainLowPoly\Terrain.cpp`.
- sound.dll: 12 exports: `create_sound`, `delete_sound`, `release_sound`, `init_sound_timer`, `get_sound_version`,
  `create_device` / `delete_device` of `Dll_Midi_Device`, `Dll_Wave_Device` and `Dll_Wave_In_Device`,
  `WaveInDeviceMgr::call_back`.
- golf.exe: strings `jackal`, `jackal.pcx`, `fifo::add`, `Message from the Net Underworld`, `advance_frame()`,
  `monkey sweats on a tuesday` (x4, beside `.pcx`; Gettysburg!'s PCX tag), and "The sound header files used in the game
  do not match the ones used in sound.dll". No `JACKAL` string and no JACKAL.LIB version check.
- SMSG's projects (`.dsp`): golf.exe `/O2 /MT /YX`, linking `jackal.lib` (a Win32 Static Library, `/O2 /MT /YX`) and
  `jpeg.lib`; jgld.dll `/Od /ZI /GZ /MTd`, `/debug`; sound.dll `/O2 /MT`, `/incremental:yes`, with `lpc10.lib` `/Od /ZI
  /GZ`; Terrain.dll `/Od /ZI /GZ /Gm /GX /MTd`, `/debug`. VC6 without a service pack, SP5 for sound.dll, SP2's
  LIBCMTD.LIB for jgld.dll and Terrain.dll.

## golf.exe and SafeDisc

| Build | Patch, md5 | golf.exe | Linked (UTC) | md5 | SafeDisc | Readable copy |
|---|---|---|---|---|---|---|
| 1.01 | `SimGolf_Patch_v101.exe` `abf11d69c03db51741f8905d276abcfc` | 1,944,781 | 2002-01-29 16:49:12 | `bb2dc4f6d30c7ceb0b05d20cd579ac8d` | 2.51.021 | none held |
| 1.02b beta 2 | `SimGolf_USEnglish_Patch_v102b_BETA2.exe` `1d49ed286a7938b067152e4a4f3fcd6e` | 1,948,877 | 2002-02-23 00:52:44 | `12f60ff0acab71c4cb947f37741cf835` | 2.51.021 | none held |
| 1.02 | `SimGolf_Patch_v102.exe` `70c198de09d884421ca052ace8d61fa7` | 1,948,877 | 2002-03-08 02:05:58 | `9db56f7f26059d6c1a9dfc05bd10bfac` | 2.51.021 | cleaned, 954,368 B, `8fe51a6b48aa7b9de15b84063bb5d2b5` (header text `further intrigue`) |
| 1.03 | `SimGolf_Patch_v103.exe` `ded4687473c42c1817d1dd59e909f46d` | 2,286,953 | 2002-03-29 20:34:22 | `0eaabccd12f448a0d4c469ad64c15ce7` | 2.60.052 | Safedisc2Cleaner output, same length, `c6c7da79f2b88d609b357c7998f1a860`; its PE part is the reference |

Sources: 1.03 firaxis.com/downloads/patch/SimGolf_Patch_v103.exe (Wayback 20021227182734); 1.02
firaxis.com/downloads/Patch/SimGolf_Patch_v102.exe (Wayback 20051016074149); 1.01
simgolf.ea.com/dloads/SimGolf_Patch_v101.exe (Wayback 20060805205403); 1.02b
archive.org/details/sim-golf-usenglish-patch-v-102b-beta-2. Each patch holds golf.exe and text files only, so jgld.dll,
sound.dll and Terrain.dll stay the retail December 2001 files.

- Every wrapped build adds sections `stxt774` and `stxt371` and encrypts code and data: the 1.03 wrapped and cleaned
  files differ in 752,049 `.text` and 155,244 `.data` bytes. The string `jgld.dll` is readable only in the cleaned
  copies.
- The SafeDisc version is three dwords at header offset 0xff4, after `BoG_ *90.0&!!  Yy>`.
- The reference is the cleaned 1.03's first 954,368 bytes, where the last section, `.rsrc`, ends
  (sha256 `a78b196737f86600ab95693334a7015e5638cde581c6c78eb2c6227c7b9834ab`). The rest is a
  1,332,585-byte overlay: a 45-byte marker, then zeros. The cleaner wrote
  `Safedisc2Cleaner (c) bOOls eYe, waste_me & r!sc boolseye.cjb.net` at 0x2a0 and set the COFF stamp to `0x73655965`
  ("eYes"); SizeOfImage is `0x44414e` (not page-aligned); the IAT directory is zeroed.
- Cleaned 1.02 against cleaned 1.03: the same section table and Rich header; 6,655 bytes differ, 1,790 of them inside
  SetupGolferAppearance (0x462020-0x4627d0), 1.03's only code change. The rest are header fields (COFF stamp,
  SizeOfCode, SizeOfImage), the cleaner's text, the SafeDisc version, the export stamp and SafeDisc stubs in post-`ret`
  pads.

## Demo

`simgolfdemo.exe`, 41,170,929 B, `f80d456930834cec5042fc01d36f22a2`: archive.org/details/Simgolfdemo, and
firaxis.com/downloads/Demo/simgolf_demo.exe (Wayback 20060326032123). An InstallShield set; extracted, the game files
are under `Disk1/game_data/` (`Program_Executable/`, `Program_Files/`).

| File | Linked (UTC) | Size | md5 | Kind | Gives |
|---|---|---|---|---|---|
| `golf_demo.exe` | 2001-09-20 22:24:28 | 790,528 | `551d02c7527d8b7b5cf2f3279f77aae5` | release; no SafeDisc, no debug directory; Rich C++ 8966 x70 and 8168 x38, C 8168 x122 | loads `jgl.dll`; string `Sid Meier's Next Game` |
| `jgl.dll` | 2001-09-07 17:42:04 | 438,319 | `607b49c98712767cfaccd883168eb446` | release; Rich C 8966 x19, C++ 8966 x23 | NB10 `C:\jackdev\jgl\Release\jgl.pdb`, signature `0x3b924d47`, age 2 |
| `sound.dll` | 2001-09-07 01:53:29 | 2,404,920 | `ccf6c2f658ac6a2f83341d0fcc8a315c` | Debug; not incremental | embedded CodeView, below; also imports `mss32.dll`; the same 12 exports |
| `Terrain.dll` | 2001-08-22 19:49:59 | 450,623 | `b46fa72ab0ab9f5df08362f63daf3a99` | Debug; incremental | NB10 `C:\Projects\3DTerrainLowPoly\Debug\Terrain.pdb`, signature `0x3b840ce7`, age 1 |
| `jackal.txt` | | 6,017 | `fbdcc5f6f4cddd4d36c6f49169f9de6d` | | the Alpha Centauri demo's file plus one hash line; title still `ALPHA CENTAURI` |

The demo's `binkw32.dll` is the retail file (`ecbd8213e89f8afde368f8eb05ff5a9c`); it and `Mss32.dll`
(`2552e9cffd352ff5b939d88af6fc6b61`) are third-party, with no debug directory.

### The demo sound.dll's CodeView

- Debug directory: MISC (`C:\civilization3/sound.dll`), FPO, and NB11 CodeView, 1,925,272 bytes at file offset
  0x751a0. The only embedded CodeView in any SimGolf file.
- The sound API: `C:\soundapi\`, 34 `.cpp` and 23 headers (`sound.cpp`, `sound device.cpp`, `dll main.cpp`,
  `dsmgr.cpp`, `wavemgr.cpp`, `VoiceRxMgr.cpp` ...), objects in `C:\soundapi\soundO\`, PCH `sound_pch.h`. Compiler front
  end 12.00.8964, back end 12.00.8966. `-G5 -MTd -W3 -Gm -GX -ZI -Od`, `-DSND_DLL -DUSE_MMX -DEAX20 -DUSE_MPEG`;
  includes `c:\mssdk\include`, `C:\Eax2.0Sdk\Include`, `C:\Program Files\Aureal\A3D 2.0 SDK\sdk\inc`,
  `C:\Miles6\include`.
- LPC-10: 34 objects in `C:\lpc10-15\lpc55-C\lpc10\Debug\`, compiler 12.00.8168, `-Od -ZI -YX -GZ`.
- Libraries: `c:\miles6\lib\win\mss32.lib`, `LIBCMTD.lib`, `libcimtd.lib`, `libcpmtd.lib`; 93 CRT objects from
  `E:\8966\VC98\crtbld\` and 6 iostream objects from `E:\8168\VC98\crtbld\`.
- 1,743 procedure records in the 33 `soundO` modules; class records with member offsets and methods (`CWaveMgr`,
  0x1498 bytes, 78 fields); 6,821 parameter and local records, 5,070 of them in `soundO`.
- The retail release has the same shape: 34 C++ objects at 8966, LPC-10's C at 8168, 42 `/GZ` fills.

## Attested source directories

| Directory | Attested by |
|---|---|
| `C:\JackDev\` (`Debug\jgld.pdb`, `jglsprite.cpp`, `jglsprite_8_16c.cpp`) | retail jgld.dll |
| `C:\jackdev\jgl\` (`Release\jgl.pdb`) | demo jgl.dll |
| `C:\Projects\3DTerrainLowPoly\` (`Terrain.cpp`, `Debug\Terrain.pdb`) | retail and demo Terrain.dll |
| `C:\soundapi\`, `C:\soundapi\soundO\` | demo sound.dll |
| `C:\lpc10-15\lpc55-C\lpc10\` | demo sound.dll |
| `C:\civilization3\` (`sound.dll`, the link output) | demo sound.dll |
| `C:\Miles6\`, `c:\mssdk\`, `C:\Eax2.0Sdk\`, `C:\Program Files\Aureal\A3D 2.0 SDK\` | demo sound.dll |
| `C:\Code\JPEG-6A\Code\` | `JPEG.lib` (224,968 B, `04e3c9facdb92090c86e2b9c4c8e1858`): 47 members, IJG's `.C` files and `CreateJPG.c`; its `CreateJPG.obj` code is golf.exe 0x4ae020 |
| none | golf.exe (every build), retail sound.dll, retail jgl.dll |

## Debug information checked

- Embedded CodeView: the demo sound.dll only.
- NB10 pointers, PDB not shipped: retail jgld.dll and Terrain.dll; demo jgl.dll and Terrain.dll.
- No debug directory: golf.exe 1.01, 1.02b beta 2, 1.02 and 1.03 as wrapped, and the cleaned 1.02 and 1.03; retail
  sound.dll, jgl.dll and `autorun.exe` (2001-12-17, `04c12cb3f10313ebd13b9c7b24b12a1f`); demo golf_demo.exe,
  binkw32.dll and Mss32.dll.

## Incremental-link placement

| Image | Thunk table at `.text`+5 | Imports | Other |
|---|---|---|---|
| jgld.dll | 570 `jmp rel32` | separate `.idata` after `.data` | 57,636 bytes of 0xcc at 0x100a0b1c-0x100aec40, before the EH funclets; export stamp and NB10 signature 20:16:43, COFF 20:23:06 |
| sound.dll | 1,207 | separate `.idata` | |
| Terrain.dll | 169 | separate `.idata` | 33,098 bytes of 0xcc ending at 0x100410b0; NB10 signature 2001-12-13 15:38, export stamp 16:49, COFF 2001-12-14 20:12; age 23 |
| golf.exe | none | in `.rdata` | no `.reloc`; a full link |
| demo sound.dll | none | in `.rdata` | the Debug build was not incremental, the retail release was |
| demo Terrain.dll | 165 | separate `.idata` | |

The three DLLs' function order and padding record an edit-and-relink history their source does not hold.

## Among its siblings

| Link | Evidence | Page |
|---|---|---|
| JACKAL.LIB 4.00 | `4.00` and "Please rebuild JACKAL.LIB" in Gettysburg!'s XP `lee.exe` (2002-06-06, `2e634b942621456a0754016796915de1`) and Antietam!'s XP `Antietam.exe` (2002-09-16, `6a6b8ceed227142f8fa5e9821163323b`). Not SimGolf's branch. | [Gettysburg.md](Gettysburg.md), [Antietam.md](Antietam.md) |
| JACKAL.LIB 10.10, SimGolf's branch | `10.10` and the same check in Alpha Centauri patch 4 `terran.exe` (1999-06-08) and Alien Crossfire patch 2 `facedit.exe` (1999-11-05; `__FILE__` `C:\jackdev\sprite.cpp` x69, `sprite_8_16c.cpp` x36, `spot.cpp`). SimGolf's retail `jackal.txt` is the SMAC / SMAX demo file (`1f266c4e9ad7d12f7ae1b650887558ad`) with the title line changed and a `; Version:` block and one hash line added. | [AlphaCentauri.md](AlphaCentauri.md) |
| Post-split JACKAL, 2000 | Planetary Pack `autorun.exe` (2000-01-29, `edc4e8b1537302d5c924c7fb1f811fb8`) and Civil War Collection `autorun.exe` (2000-07-26, `470d3d47f8724c984cadb315c71ba678`): 10.10, loading `jgl.dll` and `get_graphsy_object_ptr`. Their Debug jgl.dll: `C:\jackdev\jgl\Debug\jgl.pdb` (2000-01-24, `322493d7c9f9d1ffadef6fbcbb8e9963`) and `C:\JACKDEV\jgl\Debug\jgl.pdb` (2000-05-18, `3dc4a599699049a650eb508298a384c8`). The `C:\jackdev\spot.cpp` object holds Spot's bodies; SMSG's file is `spot.cpp`. | [AlphaCentauri.md](AlphaCentauri.md), [Antietam.md](Antietam.md) |
| jgl.dll, Civ III | SimGolf's retail jgl.dll has Rich key `0xc6ec886e` and census C 8168 x105, C++ 8168 x31, as do Civ III's jgl.dll builds of 2002-04-08 (1.21f), 2002-07-01, 2002-08-28 and 2002-10-15. | [Civilization3.md](Civilization3.md) |
| The static layer, Civ III | Of 680 golf.exe functions in 0x474780-0x4a0000 with 8 or more instructions, whole-body identical with addresses masked: 316 in the CHS 2001-10 `Civilization3.exe`, 301 in PTW US 2002-10, 194 in PTW CHS 2002-12; an unrelated control, 28. | [Civilization3.md](Civilization3.md) |
| The sound API | SimGolf's 12 sound.dll exports are Gettysburg!'s XP sound.dll's 13 less `acm_stream_callback`. Civ III's sound.dll of 2001-09-28 has 875 SimGolf sound.dll functions identical (2002-08-15: 873; 2002-11-14 and 2003-09-26: 829; the MSS32.DLL control: 14). The demo sound.dll was linked as `C:\civilization3/sound.dll`. | [Gettysburg.md](Gettysburg.md), [Civilization3.md](Civilization3.md) |
| Mac names | Four SMAC / SMACX Mac apps' PowerPC traceback tables: 4,689 names, 1,352 in 71 JACKAL classes (Win = WidgetBase, Buffer = AssetSurface; BaseButton, Net and Heap are SMSG's names too). Civ III Mac 1.21g: 5,068 (Win, Buffer, JGLSys, JGLBuffer ...). Conquests 1.22, i386 slice: 10,740 `T` symbols, 345 of them in `JGL*` / `GS*` classes (JGLSprite, JGLBuffer, JGLSys, GSBuffer ...). | [AlphaCentauri.md](AlphaCentauri.md), [Civilization3.md](Civilization3.md) |
| Gettysburg! game code | golf.exe carries Gettysburg!'s credits roll at 0x4e192c (11 strings, 3 blanks, -1; nothing reads it). ApproxDistance golf 0x467170 = XP `lee.exe` 0x438fd0, instruction for instruction. | [Gettysburg.md](Gettysburg.md) |
| Antietam! game code | Antietam.md counts 38 of golf.exe's 700 game functions with a verified ancestor, 35 in the XP build and 3 in the demo. ApproxDistance's /Od twin is XP `Antietam.exe` 0x481414. | [Antietam.md](Antietam.md) |

## What it cannot give

- Symbols for golf.exe, or for sound.dll beyond the demo build's. The jgld.dll and Terrain.dll PDBs were not shipped.
- golf.exe's translation-unit names and order, or its static library's file name.
- Release bytes from the demo sound.dll: it is a September 2001 /Od build. It gives names, types and layouts.
- golf.exe 1.01 to 1.02b: encrypted, and no cleaned copy is held.
- The DLLs' link history.

## The reconstruction

SMSG rebuilds all four images from source with VC6 (no service pack; SP5 for sound.dll; SP2's LIBCMTD.LIB for jgld.dll
and Terrain.dll), every function byte-identical to the shipped one. Exempt: the three DLLs' incremental-link placement,
and everything SafeDisc and the cleaner did to golf.exe, if the game stays playable. No image is yet the shipped file
byte for byte; golf.exe's link order was not recovered.

## Notes

- Files were read as bytes (objdump, strings, xxd, cmp, md5sum), Rich headers included; no game file was run. The
  `realloc.obj` test compares the LIBCMTD.LIB members of RTM and SP2 to SP6, relocations masked.
- Retail files: an installed retail copy and the official patches. golf.exe is read through cleaned
  copies; nothing was decrypted here.

Related: shared code: [README.md](README.md); [Gettysburg.md](Gettysburg.md) (Gettysburg!), [Antietam.md](Antietam.md)
(Antietam!), [AlphaCentauri.md](AlphaCentauri.md) (Alpha Centauri, Alien Crossfire),
[Civilization3.md](Civilization3.md) (Civilization III).

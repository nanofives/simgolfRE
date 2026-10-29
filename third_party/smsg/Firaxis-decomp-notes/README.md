# Circa 2000 Sid Meier Decompilation Notes

Where the source information for circa-2000 Sid Meier / Firaxis games lives, as it bears on the SimGolf (SMSG)
reconstruction: Sid Meier's Gettysburg! (1997), Sid Meier's Antietam!, Sid Meier's Alpha Centauri and Alien Crossfire
(1999), Civilization III (2001), Play the World (2002) and Conquests (2003), and Sid Meier's SimGolf (2002) itself.
Pointers only: no source, no binaries.

Abbreviations: CWC, the Civil War Collection; SMAC, Alpha Centauri; SMAX, Alien Crossfire; PP, the Planetary Pack;
PTW, Play the World; C3C, Conquests.

## Shared

### JACKAL.LIB

- `4.00`, the Civil War branch: Gettysburg! and Antietam!, and BreakAway's Waterloo and Austerlitz on Gettysburg's
  engine. `10.10`: Alpha Centauri and Alien Crossfire (Jason Coleman, credited "System and Interface Programming"),
  the branch SimGolf's jackal.lib descends from. ([Gettysburg.md](Gettysburg.md),
  [Antietam.md](Antietam.md), [AlphaCentauri.md](AlphaCentauri.md))
- Version check `4.00`: Gettysburg! (with the strings "Brian Reynolds", "William Robinson"), Antietam!, Waterloo.
  "10.10" and "Please rebuild JACKAL.LIB": Alpha Centauri. Like golf.exe, Civ III's readable exes have no JACKAL.LIB
  version string, so neither 4.00 nor 10.10 is attested. ([Civilization3.md](Civilization3.md))
- Graphics inside the library: Gettysburg! (no jgl DLL yet), Antietam! (`Antietam.exe` imports no jgl.dll), and every
  readable terran.exe / terranx.exe, the 2002 XP build included. The Planetary Pack's `autorun.exe` (2000-01-29) is the
  first readable build seen that loads jgl.dll.
- Post-split: the CWC 2000 pressing's `autorun.exe` (2000-07-26), full /Od post-split JACKAL "10.10", SimGolf's line;
  Civ III's post-split JACKAL static library (`Civ3.opt`: project `jackdev.dsp`, Debug library `jackaldevd.lib`; the
  2003 `D:\Civ3\jackdev` tree, not SimGolf's `C:\JackDev`).
- jackal.lib oracles: the CWC `autorun.exe` ranks first, with Planetary Pack's 2000-01 `autorun.exe`. Rank the 10.10
  oracles (the CWC `autorun.exe`, Alien Crossfire's `facedit.exe`, the Alpha Centauri demo's `terran.exe`) above
  Antietam!.
- Dialog grammar: 4.00 `@`, `width=`, `title=`, `%NUM`; SMAC and SimGolf `#`, `xs`, `caption`, `$NUM` / `$LINK`.
  Gettysburg!'s dialog keyword table matches SimGolf's position for position, renamed (`width` -> `xs`, `title` ->
  `caption`); SimGolf's is SMAC's, its 22-keyword table adds itemindex and comment. Antietam!'s BasePop default
  tables are byte-for-byte SimGolf's.
- jackal.txt: SimGolf's is the SMAC / SMAX demo file, 30 of 30 sections identical (golf.exe uses 6); the Gettysburg!
  demo's has 24 sections, 18 of them among SimGolf's 30.
- Class `Net`, SMSG's name too: "Net::send" in Antietam! and SMAC, Mac SMAC class `Net`; no `Net::` caption in
  Civ III's readable exes.
- `jackal_init` (golf 0x4855b0): Antietam!'s `Jackal_Init` runs the same init steps in the same order (body
  0x4f2587), with no step where `jackal_init` calls its empty bit-0x04 module; Civ III anchors 0x57a020 (CHS 2001),
  0x5f0e80 (PTW US).

| game | build | golf engine functions | file |
|---|---|---|---|
| Gettysburg! | `Lee.exe` | 96 of 836 score >= 0.5 (MSVCRT control: 2) | [Gettysburg.md](Gettysburg.md) |
| Antietam! | XP `Antietam.exe` | 124 confident, 15 near-identical, of 1114 | [Antietam.md](Antietam.md) |
| | South Mountain `Antietam.exe` | 133 functions in 32 of 64 files reach 0.7 or more (`C1XX.DLL` control: 0) | [Antietam.md](Antietam.md) |
| | CWC `autorun.exe` | 30 of 30 string-bearing | [Antietam.md](Antietam.md) |
| Alpha Centauri | SMAX demo `terranx.exe` | 110 with a counterpart in the JACKAL run | [AlphaCentauri.md](AlphaCentauri.md) |
| | patch 4 `terran.exe` | 188 at similarity >= 0.7 | [AlphaCentauri.md](AlphaCentauri.md) |
| | PP `autorun.exe` | 30 of 30 string-bearing (retail 1.0 `autorun.exe`: 26) | [AlphaCentauri.md](AlphaCentauri.md) |
| Civ III | CHS exe, 2001-10-02 | 316 of 680 whole-body identical (unrelated control: 28) | [Civilization3.md](Civilization3.md) |
| | PTW US exe, 2002-10-17 | 301 of 680 | [Civilization3.md](Civilization3.md) |
| | PTW CHS exe, 2002-12-27 | 194 of 680 | [Civilization3.md](Civilization3.md) |

### jgl.dll

- Gettysburg! and Antietam! predate the split. Gettysburg!: `JackalClass`, the PCX tag `monkey sweats on a tuesday`
  (on 86 of 88 demo PCX files). Antietam! strings: `JackalClass`, `DirectDClass`.
- Debug builds: Planetary Pack `jgl.dll` (2000-01-24, PDB path `C:\jackdev\jgl\Debug\jgl.pdb`), 718 of 1,252
  jgld.dll functions with an identical masked body; CWC `jgl.dll` (2000-05-18, built with jgld's flags, NB10
  `C:\JACKDEV\jgl\Debug\jgl.pdb`), 721 of 1252 identical. Both carry `c:\jackdev\jgl\jglsprite.cpp`.
- Civ III: 21 `jgl.dll` builds, 2001-09-28 to 2004-03-17; window class `JackalClass`, one export
  `get_graphsy_object_ptr`; no debug directory. Rich key `0xc6ec886e`, with its census, is SimGolf's release `jgl.dll`
  (2001-12-19, md5 `c5cf40447504c9c2086d583a362cef34`) exactly. 1.16f / 1.17f: 99.7% of jgl's own 8-instruction
  sequences shared; SimGolf's adds the 800x600 check. The Windows exes carry the string `monkey sweats on a tuesday`;
  the Mac Conquests app links the jgl classes statically (345 symbols).
- Not matched: 28 of 694 SimGolf demo jgl.dll functions match SMAX's JACKAL at >= 0.5 (release vs release).
  Gettysburg! has no counterpart to the 2001 jgl rewrite (GDI, 16-bit, PNG, JGLSys).

### sound.dll and the sound API

- The separate sound API (sound.dll / soundx.dll, David Evans). The client half: Gettysburg! compiles it with the game
  (demo 0x434900-0x437000); Antietam!'s loads `.\sound.dll` and in the XP build is the /GZ run 0x531a20-0x539515;
  SimGolf's is inside the static library, sound.cpp from 0x483e90.
- Exports: Gettysburg!'s `sound.dll` 12 on the disc, 13 from 1997-10 (adds `get_sound_version`); SimGolf's exports the
  same 12 without `acm_stream_callback`.
- One file: `sound.dll` `5794b0d11bdf6abb7deb0f791881b5a5` (1999-12-13) is Antietam! beta patch v9.84's `sound.dll`,
  the CWC disc's `sound.dll` and Alien Crossfire patch 2's `soundx.dll`.
- Debug sound clients: CWC `cwcstart.exe` (/Od /GZ), its functions in golf's sound.cpp order; the XP `Antietam.exe`
  /GZ run, holding 52 of its 124 confident jackal.lib matches (sound.cpp / SoundDevice.cpp).
- Civ III `sound.dll` 2001-09-28 / 2002-08-15 / 2002-11-14 / 2003-09-26: 875 / 873 / 829 / 829 SimGolf `sound.dll`
  functions identical (MSS32.DLL control: 14).
- SimGolf's demo `sound.dll` is the one embedded CodeView a 2026-10-11 sweep (617 PEs) found; it was linked as
  `C:\civilization3/sound.dll`. 268 `Class::method(params)` in Civ III's Mac 1.22 apps equal its CodeView; Mac SMAC's
  sound-client names (Sound, Wave, VoiceRx, Patch, WaveControlGroup) give 71 exact class::method equal to SimGolf's.

### lpc10

- Named only in the Civ III notes: an /Od /GZ (lpc10) block at `0x1003c650-0x10043f37` in the 2001-09-28 `sound.dll`,
  a like /GZ block in the PTW and C3C builds; lpc10 is among the functions identical with SimGolf's `sound.dll`.
  ([Civilization3.md](Civilization3.md))

### Compilers

- Gettysburg! 1997: VC4.2 (`_MSC_VER=1020` in `vc40.idb`; linker 4.20, the December 1997 demos linker 5.02); the 2002
  rebuild VC6 (linker 6.0). ([Gettysburg.md](Gettysburg.md))
- Antietam! XP rebuild (2002-09-16): full /Od debug build with the debug CRT, not incrementally linked.
  ([Antietam.md](Antietam.md))
- Civ III Rich: Utc12 C++ objects, CHS 2001 exe 212 at build 8966 (VC6 SP5), PTW US 2002-10 230 at 8168, golf's build;
  the PTW CHS `jgl.dll` linker 7.0. ([Civilization3.md](Civilization3.md))
- Mac: CodeWarrior, Cfront-mangled (SMAC / SMACX 2000-2003, Civ III 1.21g PEF); Civ III Complete 1.22: Mach-O,
  the Conquests i386 slice Itanium-mangled. ([AlphaCentauri.md](AlphaCentauri.md), [Civilization3.md](Civilization3.md))
- SimGolf is VC6 /O2. The VC4.2/5 and /Od code is matched to it by masked similarity only. PowerPC and /Od builds give
  names, classes and grouping, not MSVC6 /O2 bytes; Mac builds give names and signatures, not MSVC6 bytes or Windows
  TU order. ([Gettysburg.md](Gettysburg.md), [Antietam.md](Antietam.md), [AlphaCentauri.md](AlphaCentauri.md),
  [Civilization3.md](Civilization3.md))

### Game code

- Sid's framework (Geometry.cpp, InputPoll.cpp, InputWait.cpp, Screen.cpp, ListDialog.cpp, ScreenText.cpp, GameMain.cpp's wait loop, save-block
  transfer) is in Gettysburg!. 38 of golf's 700 game functions have a verified Sid-lineage ancestor, all measured
  against Antietam! (35 in the XP build, 3 in the demo), about 1%.
- The project tree: Gettysburg!'s `lee` (`l:\source\`, `\games95\lee`, `c:\lee\`); Antietam! is built in it
  (`C:\Doug Antieitam\lee\`).
- The credits roll: Gettysburg! 11 strings, 3 blanks, -1, no reader; Antietam!'s XP build keeps the unused reader at
  0x481bf0, zero callers; SimGolf's data at 0x4e192c. Alpha Centauri has none; its game code is Brian Reynolds's, and 2
  of 268 golf game functions match, both trivial.

### Evidence by game

| evidence | game | held | file |
|---|---|---|---|
| debug and /Od builds | Gettysburg! | two /Od runs inside JACKAL in the 1997-12 demos; the 2002 XP `lee.exe`, /Od JACKAL 4.00; `mpgetty.dll`; BreakAway's Waterloo / Austerlitz /Od runs and Debug `BAGLoad.dll` / `BAGSound.dll` | [Gettysburg.md](Gettysburg.md) |
| | Antietam! | the XP `Antietam.exe` (2002-09-16, full /Od, the main 4.00 oracle); `AntietamDemo.exe`'s /Od JACKAL 4.00 region; CWC `cwcstart.exe`, `jgl.dll` and the 2000 pressing's `autorun.exe` (10.10) | [Antietam.md](Antietam.md) |
| | Alpha Centauri | demo `terran.exe` (1998-12-18), demo 1.1 and 1.1m; retail 1.0 `terran.exe` and `autorun.exe`; SMAX patch 2 `facedit.exe` on a Debug JACKAL.LIB; Planetary Pack `autorun.exe` and `jgl.dll` | [AlphaCentauri.md](AlphaCentauri.md) |
| | Civ III | the `sound.dll` /Od /GZ (lpc10) block; the C3C `jgl.dll`'s Debug libpng / zlib region | [Civilization3.md](Civilization3.md) |
| | SimGolf | retail `jgld.dll` and `Terrain.dll` (Debug, incremental); the demo's `sound.dll` (Debug) and `Terrain.dll` | [SimGolf.md](SimGolf.md) |
| CodeView | all four | none in any Gettysburg!, Antietam!, SMAC / SMAX or Firaxis Civ III Windows file; a 2026-10-11 sweep (617 PEs) found one, SimGolf's demo `sound.dll` | [Gettysburg.md](Gettysburg.md), [Antietam.md](Antietam.md), [AlphaCentauri.md](AlphaCentauri.md), [Civilization3.md](Civilization3.md) |
| | SimGolf | the demo `sound.dll`: NB11 at file offset 0x751a0, the only embedded CodeView in any SimGolf file | [SimGolf.md](SimGolf.md) |
| PDB paths | Gettysburg! | `C:\mpgetty\Debug\mpgetty.pdb`, `C:\autolee\Debug\autolee.pdb`; BreakAway `C:\SRC\Water32\BagLoad\Debug\BAGLoad.pdb`, `C:\SRC\Austerlitz\Bag{Load,Sound}\Debug\` | [Gettysburg.md](Gettysburg.md) |
| | Antietam! | `C:\Doug Antieitam\lee\debug\Antietam.pdb`, `C:\Recon Rover Rick\Debug\cwcstart.pdb`, `C:\JACKDEV\jgl\Debug\jgl.pdb` | [Antietam.md](Antietam.md) |
| | Alpha Centauri | `C:\jackdev\jgl\Debug\jgl.pdb`, `C:\smac sound developement\Release\sound.pdb`, `C:\Alpha Centauri v5\Release\terran.pdb` | [AlphaCentauri.md](AlphaCentauri.md) |
| | Civ III | `Debug/Civilization3Xd.pdb`, named in `Civ3.opt` | [Civilization3.md](Civilization3.md) |
| | SimGolf | `C:\JackDev\Debug\jgld.pdb`, `C:\Projects\3DTerrainLowPoly\Debug\Terrain.pdb`, demo `C:\jackdev\jgl\Release\jgl.pdb`; no PDB shipped | [SimGolf.md](SimGolf.md) |
| `__FILE__` names | Antietam! | CWC `autorun.exe` `C:\jackdev\spot.cpp`; CWC `jgl.dll` `c:\jackdev\jgl\jglsprite.cpp` | [Antietam.md](Antietam.md) |
| | Alpha Centauri | 5 files: `facedit.exe` `C:\jackdev\sprite.cpp`, `C:\jackdev\sprite_8_16c.cpp`, `C:\jackdev\spot.cpp`; PP `autorun.exe` `C:\jackdev\spot.cpp`; PP `jgl.dll` `c:\jackdev\jgl\jglsprite.cpp`, `C:\jackdev\jgl\jglsprite_8_16c.cpp` | [AlphaCentauri.md](AlphaCentauri.md) |
| | SimGolf | `jgld.dll` `c:\jackdev\jglsprite.cpp`, `C:\JackDev\jglsprite_8_16c.cpp`; `Terrain.dll` `C:\Projects\3DTerrainLowPoly\Terrain.cpp` | [SimGolf.md](SimGolf.md) |
| build records | Gettysburg! | `vc40.idb` (VC4.2: the PCH TU `l:\source\project.cpp`, flags, include path); `Script1.rc` | [Gettysburg.md](Gettysburg.md) |
| | Civ III | `Civ3.opt` (MSDEV workspace: project paths, link line, PDB name, jackdev folder groups, 145 `.obj` names) | [Civilization3.md](Civilization3.md) |
| Mac traceback names | Gettysburg! | 0, Mach-O too; the Mac name lists cover SMAC, SMAX and Civ III only | [Gettysburg.md](Gettysburg.md) |
| | Antietam! | 0, Mach-O too; no Mac build held | [Antietam.md](Antietam.md) |
| | Alpha Centauri | 4,689 unique from 4 SMAC / SMACX apps, 1,352 in 71 JACKAL classes; also the Mac SMAX demo, 3,906 names | [AlphaCentauri.md](AlphaCentauri.md) |
| | Civ III | Mac 1.21g PEF: 5,068; 1,009 in 54 JACKAL classes, 325 in `JGL*` / `GS*` classes | [Civilization3.md](Civilization3.md) |
| unstripped symbols | Alpha Centauri | 0 unstripped ELF / Mach-O; Loki's 5 ELF binaries stripped | [AlphaCentauri.md](AlphaCentauri.md) |
| | Civ III | Complete 1.22 Rev A, i386 slices unstripped: Conquests 10,740 `T` + 696 `t`, PTW 9,974 `T`; also the Mac editor `Civ3Edit.zip`, 2,496 `T` | [Civilization3.md](Civilization3.md) |
| readable release builds | Gettysburg! | retail 1.00F and patches 1-3 `lee.exe` (no debug record, no /Od run) | [Gettysburg.md](Gettysburg.md) |
| | Antietam! | South Mountain `Antietam.exe` (2000-08-07); beta patch v9.84 | [Antietam.md](Antietam.md) |
| | Alpha Centauri | patch 4 `terran.exe` (1999-06-08, JACKAL 10.10); SMAX demo `terranx.exe` | [AlphaCentauri.md](AlphaCentauri.md) |
| | Civ III | CHS `Common/Civilization3.exe` (2001-10-02, unprotected); PTW US `Civilization3X.exe` (2002-10-17, SecuROM); PTW CHS (2002-12-27, unprotected) | [Civilization3.md](Civilization3.md) |
| copy protection | Gettysburg!, Antietam! | none found | [Gettysburg.md](Gettysburg.md), [Antietam.md](Antietam.md) |
| | Alpha Centauri | SafeDisc on the SMAX and Planetary Pack discs and SMAX patch 2; `TERRAN.ICD` / `TERRANX.ICD` code encrypted | [AlphaCentauri.md](AlphaCentauri.md) |
| | Civ III | SafeDisc (retail, 1.16f, 1.17f), SecuROM (PTW US 2002-10), ActiveMARK (Trymedia trial) | [Civilization3.md](Civilization3.md) |
| | SimGolf | SafeDisc on every patch build of `golf.exe` held, 1.01 to 1.03 | [SimGolf.md](SimGolf.md) |

## Index

| file | covers |
|---|---|
| [Gettysburg.md](Gettysburg.md) | Sid Meier's Gettysburg! (1997): builds, discs, `vc40.idb`, names, code shared with SimGolf, the BreakAway licensees |
| [Antietam.md](Antietam.md) | Sid Meier's Antietam!: the Civil War Collection, demo, scenario and patch builds, names, code shared with SimGolf |
| [AlphaCentauri.md](AlphaCentauri.md) | Sid Meier's Alpha Centauri and Alien Crossfire (1999): Windows, Linux and Mac builds, names, the Mac class map, code shared with SimGolf |
| [Civilization3.md](Civilization3.md) | Civilization III (2001), Play the World, Conquests: Windows builds, 21 `jgl.dll` builds, Mac names, `Civ3.opt`, code shared with SimGolf |
| [SimGolf.md](SimGolf.md) | Sid Meier's SimGolf (2002): its four images, golf.exe and SafeDisc, the demo and its `sound.dll` CodeView, source directories, its place among the siblings |

All five: no crack was applied or run; no game file was run; downloads were read as bytes only; full-game media from
images of owned titles (a format-shift; SimGolf: an installed retail copy), not redistributed.

## Credits

Names the notes attest:

- "Brian Reynolds", "William Robinson": strings in Gettysburg!'s JACKAL.LIB `4.00` check.
- Jason Coleman: JACKAL.LIB `10.10`, credited "System and Interface Programming".
- David Evans: the sound API (sound.dll / soundx.dll).
- Brian Reynolds: Alpha Centauri's game code.
- Sid Meier: "Sid's `lee` game project", "Sid's framework".

Artifacts: archive.org, the Wayback Machine, macintoshrepository.org and the download sites named in each file.

# Sid Meier's Alpha Centauri and Alien Crossfire

Where the source information for Sid Meier's Alpha Centauri and Alien Crossfire lives, as it bears on the SimGolf
(SMSG) reconstruction. Pointers only: no source, no binaries.

## What it is

Sid Meier's Alpha Centauri (SMAC, Firaxis, 1999) and Alien Crossfire (SMAX, 1999). Engine: JACKAL.LIB version "10.10"
(Jason Coleman, credited "System and Interface Programming"), the branch SimGolf's jackal.lib descends from. Graphics
are inside the library in every readable terran.exe / terranx.exe, the 2002 XP build included; the Planetary Pack's
autorun.exe (2000-01-29) is the first readable build seen that loads jgl.dll. Sound: the separate sound API (sound.dll /
soundx.dll, David Evans).

## Where source information lives

### Windows

| Artifact | Build | md5 | Gives | From |
|---|---|---|---|---|
| demo `terran.exe` | 1998-12-18 | 6c3223a2105a015c1baa4820ff0bb423 | full /Od debug build, JACKAL 10.10; JACKAL run 0x5af710-0x63591f | SMACDEMO.EXE, Level 48 (1999-01) cover CD, archive.org czlevel48cd |
| demo 1.1 `terran.exe` | 1999-01-08 | c1a08fe83ac5b0502f10b2ea568b987e | full /Od debug build | firaxis.com/downloads/demo/smac_demo1_1.exe (copy: archive.org SidMeiersAlphaCentauri_1020) |
| demo 1.1m `terran.exe` | 1999-01-08 | 33ebe6938b75a1221fc890575d99855d | the same debug build line | smac_demo1_1m.exe, archive.org SidMeiersAlphaCentauriDemo |
| retail 1.0 `terran.exe` | 1999-01-14 | 1ecfa606412257b4ee2f975a09b2e4f6 | debug build; 6,702 of 7,392 functions (90.7%) have demo 1.1's masked body | disc (1.0z, Classic 2000-06, rerelease) |
| retail 1.0 `autorun.exe` | 1999-01-15 | d4150ec94f97e085b419e8c4817a3218 | debug build, pre-split JACKAL | disc |
| patch 4 `terran.exe` | 1999-06-08 | 8198535e951ab638cc6c302285ce9741 | release build, JACKAL 10.10 | firaxis.com/downloads/patch/smacp4e.exe |
| patch 4 `sound.dll` | 1999-05-03 | a862911bb500165928cfef2abaf9f020 | PDB path `C:\smac sound developement\Release\sound.pdb` | same |
| XP update 1.03f `terran.exe` | 2002-08-13 | 9061913c585074f5098baed7578890a6 (installer) | PDB path `C:\Alpha Centauri v5\Release\terran.pdb` | firaxis.com/downloads/Patch/SMAC_Win2000_XP_Compatibility_Update_v1.03f.exe |
| SMAX demo `terranx.exe` | 1999-09-28 | d3693eb02532876468aad90d4dac1a18 | release; whole JACKAL functions masked-instruction-identical with golf | archive.org SidMeiersAlienCrossfireDemo |
| SMAX patch 2 `facedit.exe` | 1999-11-05 | 8ff927f7e8dcffee92f52d5fd0c677e1 | optimized editor (0x401000-0x41549f) on a Debug JACKAL.LIB (0x4154a0 to the CRT); `__FILE__` names; 353 /Od COMDAT helpers; version check called at 0x412d33 | firaxis.com/downloads/patch/smacxpv2.exe; also on the PP disc |
| Planetary Pack `autorun.exe` | 2000-01-29 | edc4e8b1537302d5c924c7fb1f811fb8 | full /Od post-split JACKAL (loads jgl.dll, get_graphsy_object_ptr); `C:\jackdev\spot.cpp` | disc |
| Planetary Pack `jgl.dll` | 2000-01-24 | 322493d7c9f9d1ffadef6fbcbb8e9963 | Debug jgl; PDB path `C:\jackdev\jgl\Debug\jgl.pdb`; `c:\jackdev\jgl\jglsprite.cpp` | disc |

### Linux and Mac

| Artifact | Build | md5 (download) | Gives | From |
|---|---|---|---|---|
| Loki update 6.0a | 2001-06 | b170edcde8ea753f7eef3ac6f13da6a7 | xdelta deltas for 5 ELF binaries (smac, smacx, smac.dynamic, smacx.dynamic, smacpack), about 60% literal bytes incl. section headers; all stripped | updates.lokigames.com/smac/smac-6.0a-x86.run |
| Mac SMAC demo | 2000-01-27 | 6f47348587e8f5d2ab5ae60fa2471463 | PEF, PowerPC traceback names: 3,872 | macintoshrepository.org/4996 (smacdemo.sit_.bin); also MacHome CD 05/2000 |
| Mac SMAC 1.5.2 updater | 2000-10-20 | e1ee3030358fb781a80c8a4197b09dd8 | the full app; 4,127 names | macintoshrepository.org/4996 (Alpha_Centauri_1.5.2.sit-binhex.hqx) |
| Mac Carbon b3 SMAC + SMACX | 2003-09-23 | ce865d9fb5cf668342ce2bc27118d456 | 4,073 + 3,748 names | macintoshrepository.org/4996 (SMAC_Carbon_b3.sit) |
| Mac SMAX demo | 2000-05-30 | ca90d8396e9f61d8e1936e3a78a8b20c | 3,906 names | macintoshrepository.org/4495 (smacxdemo.sea_.bin) = Aspyr's macupdate.com 4631 |

## Names

| Source | Count | What |
|---|---|---|
| Mac traceback tables, 4 SMAC / SMACX apps | 4,689 unique | CodeWarrior, Cfront-mangled C++ with signatures (`init__4HeapFi`, `__ct__6BufferFv`) |
| of those, JACKAL classes | 1,352 in 71 classes | Win, Buffer, BaseButton, Heap, Strings, Net, Spot, Video ... |
| `__sinit_<FILE>_CPP` | 59 files: 45 game, 14 JACKAL | BUFFER CAVIAR CHECKBOX FILEWIN LOG MULTIDEBUG RADIOBUT RANDOM SCROLL SOUND SPRITE STRINGS TEXT WINDOW |
| Mac SMAX demo | 3,906 | 1,295 JACKAL names, all in the SMAC set; 5 new non-JACKAL free functions |
| Windows `__FILE__` | 5 files | facedit: `C:\jackdev\sprite.cpp` (69), `C:\jackdev\sprite_8_16c.cpp` (36), `C:\jackdev\spot.cpp` (1); PP autorun.exe: `C:\jackdev\spot.cpp`; PP jgl.dll: `c:\jackdev\jgl\jglsprite.cpp`, `C:\jackdev\jgl\jglsprite_8_16c.cpp` |
| Firaxis PDB paths | 3 | jgl.pdb, sound.pdb, terran.pdb above; no PDB shipped |
| strings | - | "10.10" and "Please rebuild JACKAL.LIB"; "Net::send" error caption; jackal.txt (30 sections) |
| CodeView, unstripped ELF / Mach-O | 0 | none in any SMAC / SMAX file |

Class map, Mac = SMSG name, 26 of its 35 rows: Win = WidgetBase, Buffer = AssetSurface, CheckBox = CheckBoxCtl,
Palette = FLCPalette, Flic = FlicSprite, Cursor = MouseCursor; the other 20 Mac names are SMSG's too (GraphicWin,
BaseButton, Scroll, ListBox, SpriteBox, Dialog, BasePop, Net, Time, Spot, Text, Font, Video, Strings, Filemap, Dialogs,
EditBox, Heap, Log, NetFifo).

## Code shared with SimGolf

| Measure | Count |
|---|---|
| SMAX demo: golf jackal.lib functions with a counterpart in the JACKAL run | 110; golf game code 0 of 286 |
| masked, instruction-identical (golf = SMAX demo) | CheckBoxCtl::calculate_dimensions 0x49f050 = 0x5fff30 (265); SpriteBox::SpriteBox 0x4a3110 = 0x601120 (141); Net::check_polling 0x49ab40 = 0x6232f0 (134); sin_table 0x491c70 = 0x62cbc0 (84) |
| patch 4: golf jackal.lib functions at similarity >= 0.7 | 188, 168 in the one JACKAL run; noise 2 of 262 golf game functions |
| PP autorun.exe: string-bearing golf jackal.lib functions paired | 30 of 30 (retail 1.0 autorun.exe: 26, no LoadGraphsyDll) |
| PP jgl.dll: jgld.dll functions with an identical masked body | 718 of 1,252 |
| facedit's Debug JACKAL vs jgld.dll, >= 0.5 | 43 of 481, mostly the sprite files |
| XP 1.03f 0x615b50 vs jgl's message pump | 0.986, with JACKAL's three service calls at entry |
| Mac bridge, first pass: golf jackal.lib functions named | 139 (16 by string, 123 by call graph) |
| Mac sound-client names (Sound, Wave, VoiceRx, Patch, WaveControlGroup) = SimGolf's | 71 exact class::method |
| jackal.txt | SimGolf's is the SMAC / SMAX demo file: 30 of 30 sections identical; title line changed, a `; Version:` block added; golf.exe uses 6 |
| dialog grammar | `#`, `xs`, `caption`, `$NUM` / `$LINK`; the keyword table is SMAC's; SimGolf's 22-keyword table adds itemindex and comment |

Not shared: the game code is Brian Reynolds's. No credits roll; 2 of 268 golf game functions match, both trivial.

## What it cannot give

- Windows member / TU names for JACKAL.LIB beyond the `__FILE__` and PDB strings above. No PDB was shipped.
- Loki: all 5 ELF binaries stripped, 0 names. Mac SMAX 1.2.1 DE updater: a VISE diff, not an app.
- Mac `__sinit_` names exist only for files with static initialisers; their address order is alphabetical (game files,
  then JACKAL files), not the Windows link order.
- Mac names are 2000-2003 CodeWarrior builds; Civ III's names supersede them where both exist (see [Civilization3.md](Civilization3.md)).
- jgl: only 28 of 694 SimGolf demo jgl.dll functions match SMAX's JACKAL at >= 0.5 (release vs release).
- TERRAN.ICD / TERRANX.ICD code is SafeDisc-encrypted (7-8 readable prologues against 2,217 in patch 4).
- PowerPC and /Od builds give names, classes and grouping, not MSVC6 /O2 bytes.

## Notes

- Copy protection: SafeDisc (C-Dilla) on the SMAX and Planetary Pack discs (small terran.exe / terranx.exe loaders,
  `.ICD` payloads, secdrv.sys, drvmgt.dll) and SMAX patch 2 (a terranx.exe loader and TERRANX.ICD beside patch 4's plain
  terran.exe). The SMAX and PP Terran.icd carry patch 4's PE timestamp. SMAC 1.0 (1.0z, Classic, rerelease), Packard
  Bell (terran.exe 1999-03-11) and patch 4 ship a plain terran.exe. No SecuROM or ActiveMARK.
- Full-game media were analysed from images of owned titles (a format-shift), not redistributed: SMAC 1.0z, Classic 2000-06,
  Packard Bell 1999-07, rerelease, Planetary Pack, SMAX 1999. Everything else is a free demo, patch or updater.
- No cracks: nothing was decrypted, patched or run; downloads were unpacked and read as bytes only.

Related: SimGolf (SMSG); shared code: [README.md](README.md); [Gettysburg.md](Gettysburg.md) (Gettysburg!), [Antietam.md](Antietam.md) (Antietam!),
[Civilization3.md](Civilization3.md) (Civilization III).

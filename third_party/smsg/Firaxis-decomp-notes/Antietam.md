# Sid Meier's Antietam!

Where the source information for Sid Meier's Antietam! (Firaxis) can be found, as it bears on the SimGolf reconstruction (SMSG). This repository holds no source and no binaries.

## What it is

Antietam! is built in Gettysburg!'s `lee` project tree (`C:\Doug Antieitam\lee\`). It links `JACKAL.LIB` version "4.00", the Civil War branch. SimGolf's jackal.lib descends from the "10.10" Alpha Centauri branch. In Antietam! the graphics half is still inside JACKAL.LIB, so `Antietam.exe` imports no jgl.dll. Sound goes through the sound API's client half, which loads `.\sound.dll`.

## Where source information lives

| Artifact | Version / date | md5 | What it gives | From |
| --- | --- | --- | --- | --- |
| `Antietam.exe`, Civil War Collection XP rebuild | 2002-09-16 | `6a6b8ceed227142f8fa5e9821163323b` | Full /Od debug build with the debug CRT, not incrementally linked. NB10 `C:\Doug Antieitam\lee\debug\Antietam.pdb`, age 1; the PDB was not shipped. Game code, then the JACKAL 4.00 run, then the sound client built /GZ (0x531a20-0x539515, the image's only /GZ code), then import thunks and the debug CRT. The main 4.00 oracle. | [1] |
| `AntietamDemo.exe` | 1999-11-19 | `5653cd1552d22cd54b05724eaa16c72b` | An optimized game linked with an /Od JACKAL 4.00 region. Credits-roll data at 0x50cf78. | [2] |
| `Antietam.exe`, South Mountain | 2000-08-07 | `f7efe4ba5a8c01de2e6f52237c026d77` | Release build. The same file is `SMA/Antietam.exe` on all three CWC pressings. | [3] |
| `Antietam.exe`, beta patch v9.84 | 2000-02-11 | `8fe7bec3025f9d0723a162514457391a` | Release build. The patch's `sound.dll` (`5794b0d11bdf6abb7deb0f791881b5a5`, 1999-12-13) is the same file as the CWC disc's `sound.dll` and Alien Crossfire patch 2's `soundx.dll`. | [4] |
| `CWC/cwcstart.exe` | 2000-07-31 | `6a1da090e5269f0c33e318aa507c49e4` | A debug build (/Od /GZ) of the sound client. NB10 `C:\Recon Rover Rick\Debug\cwcstart.pdb`, not shipped. Its functions are in golf's sound.cpp order. | CWC disc, all pressings |
| `jgl.dll` | 2000-05-18 | `3dc4a599699049a650eb508298a384c8` | A debug, post-split jgl, built with jgld's flags. NB10 `C:\JACKDEV\jgl\Debug\jgl.pdb`. 721 of 1252 jgld.dll functions are identical. | CWC disc, all pressings |
| `autorun.exe` | 2000-07-26 | `470d3d47f8724c984cadb315c71ba678` | Full /Od, post-split JACKAL "10.10", which is SimGolf's line, with every helper out of line. `__FILE__` `C:\jackdev\spot.cpp`. Matches 30 of 30 string-bearing golf engine functions. Ranked first of the jackal.lib oracles, with Planetary Pack's 2000-01 autorun.exe. | CWC disc, 2000 pressing only |

The 2001 CWC pressings ship a release `autorun.exe` instead (2000-08-29, `fe3a9912c27362f127db36eebd3757cf`).

| # | Download | md5 | Source |
| --- | --- | --- | --- |
| 1 | `SMCWC_2000&XP_Compatibility_Update.exe`, file `payload/Data1/Antietam.exe` | `8b944315deaba203cab1bc3975b8efc6` | `http://www.firaxis.com/downloads/patch/SMCWC_2000&XP_Compatibility_Update.exe` (Wayback 20030414211043) |
| 2 | `Antietam_Demo.exe` | `47c48ca5b245a3817e69f83b356a60e3` | https://archive.org/details/SidMeiersAntietamDemo |
| 3 | `South_Mountain.exe` | `7716895c9b250d51fdcd9541010b8501` | `http://www.firaxis.com/downloads/scenario/South_Mountain.exe` (Wayback 20021219053517) |
| 4 | `antibt1.zip` | `0554a900850d3a34f41188a146d4b983` | https://archive.org/download/video-game-patches_s/Video%20Game%20Patches%20S.Part2.7z/Sid%20Meier%27s%20Antietam%2Fpatch%20beta%201%2FUS%2Fantibt1.zip |

## Names

| Source | Count | What |
| --- | --- | --- |
| Embedded CodeView | 0 | NB10 pointers only: the XP `Antietam.exe`, `cwcstart.exe` and `jgl.dll` |
| `__FILE__` names | 0 | No game or JACKAL file names in the XP `Antietam.exe`, only the debug CRT's. The CWC `autorun.exe` has `C:\jackdev\spot.cpp`; the CWC `jgl.dll` has `c:\jackdev\jgl\jglsprite.cpp`. |
| Mac traceback tables, Mach-O | 0 | No Mac build held |
| Strings | 7 names | The names below, plus `JackalClass`, `DirectDClass`, and the JACKAL.LIB version check with "4.00" |

| Antietam! name | Where | SMSG counterpart | Status |
| --- | --- | --- | --- |
| class `Net` | "Net::send", "Net: JoinService : (0x%08x)" | `Net` | applied (10.10 attests it too) |
| `SendPkType` | 0x4f674b; demo 0x4952cd | `Net::send_packet_type` | recorded |
| `SendGroup`, `SendGroupPkType` | 0x4f409b, 0x4f560d, 0x4f5e99 | no paired body | recorded |
| `Jackal_Init` | "InitMP: Jackal_Init", then a call to 0x4f2550: version check 0x4f2710, body 0x4f2587; demo 0x4bb520 / 0x4bb557 | `jackal_init` 0x4855b0 is the body | recorded |
| `JoinService` | string only | none | recorded |
| `player_id` | the "SendPkType / player_id" caption | a field | recorded |

## Code shared with SimGolf

| SimGolf part | Measured against | Result |
| --- | --- | --- |
| jackal.lib, 1114 functions | XP `Antietam.exe` | 124 confident, 15 near-identical. 52 of the 124 are sound.cpp / SoundDevice.cpp functions in the /GZ run. |
| jackal.lib, 64 files | South Mountain `Antietam.exe` | 133 functions in 32 of the 64 files reach 0.7 or more. The control, VC6 `C1XX.DLL`, gives 0. |
| jgld.dll | XP `Antietam.exe` | 28 confident ancestors. 15 atoms agree with SMSG's file grouping and 0 contradict it. |
| golf.exe game code: 700 functions, 70.9% of golf's own bytes | XP `Antietam.exe`, `AntietamDemo.exe` | 38 verified ancestors (35 in the XP build, 3 in the demo), about 1%. In the demo, 7 of 271 golf game functions reach 0.9, all of them framework functions. |

The 35 by golf.exe file: Screen.cpp 13, Geometry.cpp 8, InputPoll.cpp 5, InputWait.cpp 3, ListDialog.cpp 3, and 1 each in GameMain.cpp, ScreenText.cpp and SpriteAssets.cpp. Examples (golf = XP): ApproxDistance 0x467170 = 0x481414, GetDirection8 0x4671a0 = 0x481462, CalculateAngle 0x4672d0 = 0x44f375, ShowListSelectionDialog 0x46dea0 = 0x4cc59d.

- `Jackal_Init` runs the same init steps in the same order as golf's `jackal_init`, and its call list pins 10 pairs. It has no step where `jackal_init` calls its empty bit-0x04 module.
- Pre-split objects hold jgl and engine code together: in the XP build, atom 0x512fd0-0x513689 holds jgl's `JGLFont::init` (jglfont.cpp) and jackal.lib's `Font`. In the demo, atom 0x47fba0-0x4808a0 holds AssetSurface and jgl's `JGLBuffer::fill8`, and atom 0x480bd0-0x482940 holds PolyFill.cpp's code and the JGLBuffer line code.
- The BasePop default tables are byte-for-byte the same as SimGolf's.
- The XP build keeps the unused credits-roll reader at 0x481bf0. It draws `g_creditLines[tick/70-1]` and has zero callers. golf's copy of the data is at 0x4e192c.
- The demo's sound.cpp: 11 functions paired; 12 of their 55 pairwise orders are inverted against golf's.

## What it cannot give

- Symbols. The PDB was not shipped, and no Antietam! build carries CodeView. Names come from strings only.
- SimGolf's branch. 4.00 is not 10.10, and the dialog grammar differs: `@`, `width=`, `title=` and `%NUM` here, against SimGolf's `#`, `xs`, `caption` and `$NUM`. For jackal.lib, rank the 10.10 oracles (the CWC `autorun.exe`, Alien Crossfire's `facedit.exe`, the Alpha Centauri demo's `terran.exe`) above Antietam!.
- The jgl split. Antietam! predates it, and its order of the jglbuffer sub-blocks is reasoned, not proven.
- Most of golf's game code. About 99% has no ancestor here.
- Bytes. The /Od debug code is matched to golf's release code by masked-instruction similarity only.

## Notes

- Copy protection: none found in any Antietam! executable held. Their sections are `.text`, `.rdata`, `.data` and `.rsrc`; the XP build has `.idata` and `.reloc` instead of `.rsrc`. They contain no SafeDisc, SecuROM or ActiveMARK string, and the CWC media hold no SafeDisc files.
- Full-game media, the three Civil War Collection pressings, were analysed from images of owned titles (a format-shift), not redistributed. Downloads were read as bytes (objdump, strings, md5sum) and never run. No crack was applied or run.
- The XP update also ships the 2002 XP `lee.exe` (`2e634b942621456a0754016796915de1`); see [Gettysburg.md](Gettysburg.md).

Related: SMSG (SimGolf); shared code: [README.md](README.md); [Gettysburg.md](Gettysburg.md) (Gettysburg!), [AlphaCentauri.md](AlphaCentauri.md) (Alpha Centauri, Alien Crossfire), [Civilization3.md](Civilization3.md) (Civilization III).

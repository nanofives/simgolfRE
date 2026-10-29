# Sid Meier's Civilization III

Where the source information for Sid Meier's Civilization III lives, as it bears on the SimGolf
reconstruction (SMSG). Pointers only: no source, no binaries.

## What it is

Civilization III (Firaxis, 2001), Play the World (PTW, 2002) and Conquests (C3C, 2003) for Windows, with Mac
ports by MacSoft (1.21g) and Aspyr (Complete 1.22).

Engine: the post-split JACKAL static library (`Civ3.opt`: project `jackdev.dsp`, Debug library
`jackaldevd.lib`), `jgl.dll` (window class `JackalClass`, one export `get_graphsy_object_ptr`) and the sound
API's `sound.dll`. Like golf.exe, the readable exes have no JACKAL.LIB version string, so neither 4.00 nor
10.10 is attested.

## Where source information lives

| Artifact | Version / date | md5 | Gives | From |
|---|---|---|---|---|
| `Common/Civilization3.exe` (CHS bilingual disc cab) | linked 2001-10-02, unprotected | `9511667859511dd04fa6f455342c8d41` | readable retail code: 316 of 680 golf engine functions identical | disc (archive.org `civ-iii`) |
| `Civilization3.exe` (EU disc root) | retail, 2001-10-02, SafeDisc 2 | `cd5725abb8aea7d65d3d42c35ea196bf` | the same link as the row above (timestamp `0x3bb9d986`, Rich key `0x1fe26303`); code encrypted | disc (archive.org `civ-3_202512`) |
| `Civilization3X.exe` (PTW US disc root) | 2002-10-17, SecuROM | `14953de37e3db4accc7c13e1ed1d87b5` | readable: 301 of 680 identical; Utc12 C++ 8168, golf's compiler build | disc (archive.org `civ3_ptw`) |
| `CIV3PTW/Civilization3X.exe` (PTW CHS) | 2002-12-27, unprotected | `fc3ff7fb75d7c451c1b59530568c9974` | 194 of 680 identical | disc (archive.org `Civ3PtWChs`) |
| `civ3v116f.exe`, `civ3v117f.exe` | 1.16f, 1.17f | `ab2d05dfc06b53421c1e43089552a6df`, `10ba1581452937f940bbf72dc0a7f4a8` | SafeDisc exe, readable `.rdata`: vftables, function spacing; `jgl.dll` | archive.org `civ3-patches` |
| `jgl.dll`, 21 builds | 2001-09-28 to 2004-03-17 | see below | Rich census; Conquests: a Debug libpng / zlib region | the discs, patches and trial below |
| `sound.dll` | 2001-09-28 | `f82a1295591a680ce4a5356d06680c5c` | 875 SimGolf `sound.dll` functions identical; /Od /GZ (lpc10) block `0x1003c650-0x10043f37` | discs, trial |
| `sound.dll` | 2002-08-15 (PTW), 2003-09-26 (C3C) | `51f918ae704c4badfbf1fcf92585bbc6`, `0d7fad671f0b8ddb45a79511ce325561` | 873, 829 identical; a like /GZ block | discs |
| `Conquests/Civ3.opt` (install cab) | C3C 2003 | `f129667a9bee4ec71b9e576088490c61` | MSDEV workspace `Civ3`: project paths, link line, PDB name, jackdev folder groups, 145 `.obj` names | disc (archive.org `civilization-iii-conquests`) |
| `Civ3GoldSetup.exe` | Trymedia trial, 2004 | `b75b67ac22fe6318f73c6dfd276bb92b` | Civ III + PTW: ActiveMARK exe; `JGL.DLL` 2003-07-17; two `SOUND.DLL` | archive.org `Civ3GoldSetup` |
| `Civ3_121g_Installer.sit` | Mac 1.21g (= PC 1.21f) | `506da6f56a8714332aac89069ba3dad0` | the full CodeWarrior PEF app: 5,068 traceback names | `ftp://ftp.infogrames.net/patches/civ3/Civ3_121g_Installer.sit` |
| `Civilization3MacRevA.dmg` | Complete 1.22 Rev A, Universal | `9eaa3bb562f3a0b8d27a1bd28108fbdc` | PTW and C3C Mach-O apps; i386 slices unstripped | Aspyr support entry 20687091 |
| `Civ3Edit.zip` | Mac editor, 2005-10-21 | `26291b7816a45ce6ae7d52fd8ef08fd0` | 2,496 `T` symbols; no jgl, no JACKAL classes | `http://www.macsoftgames.com/products/civ3goty/support/Civ3Edit.zip` |

The two Mac updaters were fetched from macintoshrepository.org (ids 67309, 28642).

### jgl.dll builds

| PE date | md5 | Rich key | Found in |
|---|---|---|---|
| 2001-09-28 | `497829ad4be5571b15c45d37ef6f299e` | `0x4281b064` | EU disc root |
| 2001-10-02 | `20a71bc730d659fce8d7187e11d48c3f` | `0xad36c261` | EU cab, CHS English |
| 2001-11-26 | `833d88b8ced0c02f4dc3ed737e384c32` | `0xad36c261` | 1.16f, 2002 reissue cab |
| 2001-12-04 | `10b6808f069f2ee7d0c20a7946ca8a60` | `0x4281b064` | 2002 reissue disc root |
| 2002-01-10 | `38213c13f190f4e7f45d478be28557ad` | `0x2938a091` | CHS PRC |
| 2002-01-16 | `d698844bdd2403e2dd7402b1cd0b58c4` | `0xad36c261` | 1.17f |
| 2002-04-08 | `4f26b03b59258272082583cd77067b1b` | `0xc6ec886e` | 1.21f |
| 2002-07-01 | `10fd7e9e6c61bb8e9c256b4620bcb45e` | `0xc6ec886e` | 1.29f; patch cabs on the PTW US and C3C discs; PTW CHS cab |
| 2002-08-08 | `41497367bb613355ecc423d626126b32` | `0xad36c261` | 1.29f JP |
| 2002-08-28 | `99ada8b7ced73217e128c5627a93cd9d` | `0xc6ec886e` | PTW US disc root |
| 2002-10-15 | `a9b196093a604b94e458e2e97a5cf077` | `0xc6ec886e` | PTW US cab |
| 2002-10-28 | `cc2802509b66538181e584a944778115` | `0x0ba9063e` | PTW 1.04f, 1.14f; C3C disc root |
| 2002-12-18 | `6af2a9aaef159993beeac0237eaa95a4` | `0xd6a6d691` | PTW 1.21f (not JP), 1.27f DE/ES/FR/IT |
| 2002-12-27 | `45d95ee978f53dde33e0b8ecdbc7f9ff` | `0x66718438` | PTW CHS (linker 7.0) |
| 2003-03-31 | `2ee4364cf1d013829cf36151b1930949` | `0xd6a6d691` | PTW 1.21f JP |
| 2003-06-06 | `f5dbcc8dcb71db405947848966c2a16b` | `0xd6a6d691` | PTW 1.27f US/UK, C3C cab |
| 2003-07-17 | `9fd549730a15269e2d6896185c8a99a9` | `0xd6a6d691` | Trymedia trial |
| 2003-09-13 | `43c05961e48963a898b49c80d1fbd424` | `0xd6a6d691` | PTW 1.27f JP |
| 2003-09-24 | `ce316c818f31ec372f452427d268f104` | `0x8c91ce6e` | C3C |
| 2003-12-18 | `c2b58c504df4d70633102693db0a4850` | `0xb391e16e` | C3C 1.15 beta |
| 2004-03-17 | `6b3734df025f302b987fb906331bae3d` | `0xb391e16e` | C3C 1.20, 1.22 |

- No build has a debug directory.
- Rich key `0xc6ec886e`, census `14.7299x22 19.8034x7 1.0x113 10.8168x105 11.8168x31 4.8168x1`, is SimGolf's
  release `jgl.dll` (2001-12-19, md5 `c5cf40447504c9c2086d583a362cef34`) exactly.
- The C3C build links a Debug libpng 1.0.12 / zlib (inflate 1.1.3): `_chkesp` calls at
  `0x1003dcef-0x10041240` and `0x100459ba-0x1004d44f`.

## Names

| Source | Count |
|---|---|
| Mac 1.21g PEF traceback tables (Cfront-mangled, with signatures) | 5,068 names; 1,009 in 54 JACKAL classes (`Win`, `Buffer`, `BaseButton`, `Heap` ...); 325 in `JGL*` / `GS*` classes |
| Mac 1.22 Conquests app, i386 slice (Itanium-mangled) | 10,740 `T` + 696 `t`; 2,241 distinct `D d B b` names; the PPC slice also 10,421 `T` |
| the same, jgl classes (linked statically) | 345 symbols: `JGLSprite` 117, `JGLBuffer` 91, `JGLSys` 61, `GSBuffer` 33, `JGLPalette` 15, `JGLFont` 9, `GSFont` 6 ... |
| Mac 1.22 PTW app, i386 slice | 9,974 `T`; the PPC slice stripped to 1,138 `T` |
| sound classes in the Mac 1.22 apps | 268 `Class::method(params)` equal SimGolf's demo `sound.dll` CodeView (`Midi`, `Wave_Device`, `Wave`, `Sound` ...) |
| `Civ3.opt` | `D:\Civ3\jackdev\jackdev.dsp`, `D:\Civ3\Peer\Peer.dsp`, `D:\Civ3\Civilization3X.dsp`; `\jackdev\jackaldevd.lib`, `\jackdev\uv_lib.lib`, `\jackdev\caviar.lib`, `..\jackdev\intel\WinCPUIDlib.lib`; `Debug/Civilization3Xd.pdb`; 145 `.\Debug\*.obj` (Civ III game code, none of jackdev's) |
| jackdev folder groups (`Civ3.opt`) | Source Files, Header Files, Additional Libraries, Sound Code and DLL, Math, Sprite, Buttons, Dialogs, Buffer, Textures, Popup, Multiplayer, 3D Rendering, Data Storage, Common, Intel |
| Windows exes | strings only: `jackal.pcx`, `FILEWIN*`, `monkey sweats on a tuesday`, golf's one sound-header message |

## Code shared with SimGolf

| Layer | Civ III build | Measure |
|---|---|---|
| JACKAL static layer (SMSG `jackal.lib`) | CHS exe, 2001-10-02 | 316 of 680 golf engine functions whole-body identical (addresses and displacements masked); an unrelated control exe: 28, none over 12 instructions |
| | PTW US exe, 2002-10-17 | 301 of 680; of 343 candidate JACKAL functions golf's `/OPT:REF` dropped, 152 are byte-proven here |
| | PTW CHS exe, 2002-12-27 | 194 of 680 |
| | 1.16f / 1.17f exe `.rdata` | 11 vftable windows of 96-118 slots (WidgetBase, GraphicWin, BasePop, BaseButton, Scroll, SysMessageHandler, Sound / Sound_Device ...), 0 mapping violations; 272 functions at golf's spacing; the same DirectPlay GUID block and Bink imports |
| Net | CHS 2001 exe only | golf's `Net::send_packet_type` (0x499140, 1285 instructions) and `SendMessageWithRetry` (0x497fc0, 869) identical |
| jgl | 1.16f / 1.17f `jgl.dll` | 99.7% of jgl's own 8-instruction sequences shared; SimGolf's adds the 800x600 check |
| sound API | `sound.dll` 2001-09-28 / 2002-08-15 / 2002-11-14 / 2003-09-26 | 875 / 873 / 829 / 829 SimGolf `sound.dll` functions identical, lpc10 among them; MSS32.DLL control: 14 |
| | SimGolf demo `sound.dll` | was linked as `C:\civilization3/sound.dll` |
| compiler | Rich | Utc12 C++ objects: CHS 2001 exe 212 at build 8966 (VC6 SP5); PTW US 2002-10 230 at 8168, golf's build |

Anchors:

| Function | golf.exe | CHS 2001 | PTW US |
|---|---|---|---|
| `jackal_init` | 0x4855b0 | 0x57a020 | 0x5f0e80 |
| `jackal_close` | 0x485740 | 0x57a1b0 | 0x5f1020 |
| `BasePop::BasePop` | 0x48ce00 | 0x565180 | 0x5e3460 |

## What it cannot give

- No PDB, CodeView or debug directory in any Firaxis Windows file (only `binkw32.dll`, DirectX setup, `secdrv.sys`
  and `setupapi.dll` carry one), so no Windows names, file names or TU order.
- The SafeDisc exes (retail discs, 1.16f, 1.17f): code is encrypted; only headers, Rich and `.rdata` were read. The
  ActiveMARK trial exe keeps its sections in one packed blob (entropy 8.0): headers and Rich only. No decryption was
  attempted.
- PTW US: one call-round name pair (`EditBox::on_key_click` -> 0xc42e50) landed in SecuROM's `.cms_t`
  (0xc37000-0xc64000) and was wrong.
- Civ III's release function order includes `/O2` TU-end inline emission, so it is not source order.
- `Civ3.opt` names jackdev's folder groups but no member files (its only header names are `ifc.h`, `imm*.h`,
  `feelitapi.h`, `basetsd.h`), and is the 2003 `D:\Civ3\jackdev` tree, not SimGolf's `C:\JackDev`.
- No `Net::` caption in the readable exes: the class name `Net` is not attested here.
- Mac builds give names and signatures, not MSVC6 bytes or Windows TU order.
- No Civ III demo exists. The Taiwan 1.29 update (RAR SFX behind UPX) was not extracted.

## Notes

- Protection: retail, 1.16f and 1.17f `Civilization3.exe` SafeDisc (`stxt774`, `stxt371`); PTW US 2002-10
  SecuROM (`.cms_t`, `.cms_d`); Trymedia trial ActiveMARK (`TMSAMVOH` overlay). The CHS bilingual disc's cab
  carries the pre-wrap linker output: same timestamp and Rich key, smaller sections, no SafeDisc `.rsrc` text.
- Full-game discs were read because the owner holds Civ III Complete; patches, the trial and the Mac updaters
  are official free downloads.
- No cracks: the EU ISO's added `crack/Civilization3.exe` (DEViANCE) was not analysed; the PTW CHS ISO's
  unwrapped 2012 `Civilization3.exe` was excluded from every count here; the trial item's `C3GE.zip` was not fetched.
- Nothing was executed. Bytes were read with objdump, strings, od and md5sum.

Related: SimGolf (SMSG); shared code: [README.md](README.md); [Gettysburg.md](Gettysburg.md), [Antietam.md](Antietam.md),
[AlphaCentauri.md](AlphaCentauri.md).

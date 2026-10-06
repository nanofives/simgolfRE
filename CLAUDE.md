# CLAUDE.md — SimGolf RE

Reverse engineering of **Sid Meier's SimGolf** (Firaxis / Maxis / EA, 2002), v1.03, built on the
Mashed RE workflow (`..\Mashed\CLAUDE.md`): Ghidra + Frida, NO-GUESSING, the C0..C4 confidence
ladder, signature-checked patches, and a test suite that boots the real game.

**Shape of the project: hook-based, not greenfield.** The stock game runs; our code lives in a shim
DLL that fixes Windows 11 compatibility, applies patches, and hosts reimplementations that are A/B
tested against the original inside the live process.

## Anchors (verify before citing any address)

All in `re/anchors.json`, enforced by `tests/test_static.py`.

```
original\golf_clean.exe  2,286,953  E74EA9F95350796432CB1CCDA3BFFE00E48789F3E410CD5FFB61F70ACEB26789   <- THE TARGET
original\golf.exe        2,286,953  200711A655DEB60DD63B2C379B25086B90CBF9A0B8C8BCC5427B224201BAC0A2   (SafeDisc 2.60.52, v1.03)
original\jgld.dll        1,273,898  07D6B89C9CAE741222DF7741C9116ACE5F64EC7B7B44124092AB7BC1C86629D4   (Jackal gfx, DEBUG build; the one loaded)
```

`golf_clean.exe` = `golf.exe` v1.03 unwrapped by Safedisc2Cleaner. Image base `0x00400000`, no
relocations, so addresses are VAs. DLL addresses are RVAs (the `module` column says which).
**Never modify `golf_clean.exe` on disk**: it is the A/B reference. Patches go through the shim.

## Engine and binaries

- **Jackal** engine (Firaxis; Alpha Centauri lineage: `jackal.txt` still has SMAC error strings).
- `jgld.dll` — 2D GDI/DIB graphics, window, fonts (debug build, `C:\JackDev\Debug\jgld.pdb`; U-0001).
- `Terrain.dll` — OpenGL 3D terrain; exports the `Terrain` class (golf imports 26 methods).
- `sound.dll` — DirectSound/winmm. `binkw32.dll` — RAD Bink 1.x video.
- `golf_clean.exe` exports 4 Terrain accessors (`tileAt`, `getElevation`, `getWall`, `getType`) that
  are **dead code** (0 runtime calls); the live ones are in `Terrain.dll`.
- Compiler: linker 6.0 (MSVC 6) for all three. `Terrain.dll` and `jgld.dll` are **debug** builds
  (stack fill `0xcccccccc`, incremental-linking `jmp` thunks: exports point at thunks, not bodies).
- `golf_v100_clean.exe` (retail v1.00, SafeDisc 2.51.021 unwrapped) is in the Ghidra master for version
  diffing only: `re/versions/PATCH_MAP.md` links v1.01-1.03 patch notes to changed functions.

## Runtime (how it runs on Win11) — details in `re/analysis/PORTABLE_RUNTIME.md`

`Play SimGolf.bat` -> `original\golf_clean.exe` + `original\winmm.dll` (our shim, a winmm proxy).
No installer, no registry (no game module imports ADVAPI32), no SafeDisc, windowed 800x600.

Shim (`shim/`, build `shim\build.bat`, MSVC x86 + vendored MinHook). Config `original\simgolf_shim.ini`,
env overrides `SIMGOLF_*`, log `original\simgolf_shim.log`:
- `windowed=1` — suppress `ChangeDisplaySettingsA`, fake 800x600 screen metrics, framed window, no `ClipCursor`.
- `bink_dib=1` — force `BINKBUFFERDIBSECTION`; `BINKBUFFERAUTO` crashes inside `BinkBufferOpen` on Win11.
- `skip_intro=0` — tests set `SIMGOLF_SKIP_INTRO=1` (menu in ~4.5 s).
- `patches/*.ini` — signature-checked byte patches (`expect` must match or the patch is refused).
- `shim/src/re/*.cpp` — reimplementations, `SG_HOOK(module, addr, name, detour, orig)`, toggle with
  `[hooks] <addr>=0` or `SIMGOLF_HOOKS_OFF=<addr>,...|all`.
- Test exports: `SimGolfShim_SetVirtualCursor`, `SimGolfShim_FindHook`, `SimGolfShim_GetInfo`,
  `SimGolfShim_CovPhase`/`CovDump` (INT3 function coverage, `SIMGOLF_COVERAGE=<entries file>`).
  `SIMGOLF_VCURSOR=x,y` makes the virtual cursor live from the first frame (the harness always sets it).
  `SimGolfShim_SetVirtualKey(vk, 0|1|2)` overlays GetKeyboardState/GetKeyState/GetAsyncKeyState: the
  in-game UI reads mouse buttons from GetKeyboardState (~13/s), which posted messages never update.
- `SIMGOLF_TIMEWARP=N` scales timeGetTime/GetTickCount/QueryPerformanceCounter (x32 + Esc every 2 s =
  ~1 game year per 40 s; event popups such as SimFoto hold game time until Esc clears them).

## Layout

```
SimGolf\
├── SM_SIMGOLF.iso, iso\, extracted\   # media and raw extraction (never edit)
├── original\                          # the portable install; golf_clean.exe + shim deployed here
├── shim\                              # winmm.dll proxy: compat fixes, patch loader, SG_HOOK registry
├── patches\                           # one .ini per byte patch
├── re\
│   ├── anchors.json, CONFIDENCE.md, functions_ghidra.tsv (2,207 exe fns), functions_ghidra_Terrain.dll.tsv (1,050)
│   ├── coverage\, versions\, replay\ (recordings, not committed), scoreboard.tsv
│   ├── analysis\<subsystem>\<addr>_<name>.md, analysis\CHANGELOG.md, analysis\PORTABLE_RUNTIME.md
│   ├── frida\  boot_trace.py, dump_image.py, diff_hook.py, hooks_registry.py, js\*.js
│   └── tools\  game.py (harness), imgcmp.py, decomp.py (headless Ghidra), winshot.py
├── tests\                             # pytest suite (see "Tests")
├── scripts\                           # re_classify.py (confidence gate), ghidra_pool.{sh,ps1}, ghidra_assert.sh
├── ghidra\scripts\                    # ExportFacts.java, ListFunctions.java, Decompile.java
├── SimGolf.gpr / SimGolf.rep          # Ghidra MASTER; simgolf_pool\ = read-only clones
├── tools\                             # unshield, SafeDiscLoader2, Safedisc2Cleaner driver (one-time setup)
└── hooks.csv, UNCERTAINTIES.md, STUBS.md, DEFERRED.md   # trackers
```

## Toolchain

- Ghidra 12.0.3 shared from `..\TD5RE\ghidra\ghidra_12.0.3_PUBLIC` (do not duplicate); MCP
  `ghidra-headless-mcp` from `..\TD5RE\ghidra\headless-mcp\` and `frida-game-hacking` — both wired in
  `.mcp.json` (same as Mashed; a new session picks them up).
- Python 3.12 (`py -3.12`): frida 17, pefile, capstone, pywinauto, pillow, numpy, pytest.
- MSVC Build Tools 2022 x86 (`vcvars32.bat`) for the shim.
- **Visual C++ 6** (cl 12.00.8168, the exact build in the game's Rich headers) in `tools/vc6/vc98`
  (extracted from the user's `Visual Studio 6.iso`, no installer; not committed). Used only by
  `re/tools/match.py` for matching decompilation.

### Matching decompilation
Plan for all four modules (scope, phases, observed rates): `re/match/PLAN.md`; counts per module and size from
`py -3.12 re/tools/match_inventory.py`. Next candidates: `re/tools/match_queue.py --module <m>` (skip list
`re/match/skip.tsv`); stuck at 80-99%: `re/tools/match_permute.py <file> <addr> [--write]` (mechanical rewrites,
hill climbing); neighbours sharing data: `re/tools/match_clusters.py --module <m> --at <addr>`. `// LANG c`
compiles a file as C. jgld.dll and sound.dll are supported (FLAGS jgld.dll: /Od /ZI /GZ). `re/match/*.cpp` hold canonical sources with `// MATCH: <module> <addr> <decorated name>` and per-module
Bulk matching from Ghidra (2026-10-05): `re/tools/ghidra2src.py --module <m> --decomp <decomp files>` turns
decompilations (dump them with `re/tools/decomp.py --program <m> <addrs...>`) into raw VC6 sources and scores them:
drops the /GZ fill and `__chkesp`, maps the `this` slot, inlines Ghidra's register temporaries, declares callees
from `log/sigs_<m>.tsv` (`ghidra/scripts/DumpSignatures.java`), thiscall callees through wrapper structs, virtual
calls `(**(code **)(*X + N))()` through synthetic vtables, then fixes VC6's conversion errors with casts and tries
variants (return this/0/local that Ghidra drops, field copies as struct assignments, match_permute.py for >= 80%).
`re/tools/g2s_collect.py --module <m> --prefix <p>` merges the 100% results into `re/match/<p>_raw_NN.cpp`
(one namespace per function). In /Od, `*(int *)((int)this + 0x14)` compiles exactly like `this->m_14`.
`ghidra/scripts/FixParams.java` committed decompiler-inferred parameters for the 2,151 golf_clean.exe functions
whose signature was still Ghidra's default (2026-10-05, master backup `ghidra/backup/*.pre_fixparams_20261005`):
before that, thiscall/fastcall callees were stored as `__cdecl (void)` and callers' decompilations dropped `this`.
VC6 /ZI writes `vc60.pdb` to the cwd; match.py and ghidra2src.py pass `/Fd<temp dir>` so parallel runs do not clash.
Hand matching of jgld.dll (debug, C++ EH on: `// FLAGS jgld.dll: /Od /ZI /GZ /GX` where a function has an EH frame)
works best with real classes (`re/match/jgld_math.cpp`, `jgld_list.cpp`, `jgld_surface.cpp`...): compiler-generated
copy constructors / `operator=` / `??_G` deleting destructors / `_$E` static initializers then match too.
Register-only loops in a /Od function are inline `__asm` in the original: `re/tools/asm2inline.py <module> <fn>
--from A --to B` prints the range as an `__asm {}` block (literal `[ebp-x]` operands, labels for jumps).
Forward `goto` in VC6 /Od jumps to a per-goto `jmp` stub; the stubs pile up in reverse order after the function's
last statement (Surface::line8, 0x1000fb70); nested if/else exits jump straight to the join instead. A `double`
compared with 1.0 is two integer compares. Ghidra's stored conventions are often wrong: ghidra2src reads each callee's
`ret N` (pops 0 with stack params = cdecl) and, in debug code, the prologue's `mov [ebp-4], ecx` (= thiscall).
Third-party libraries are matched by compiling their original release source, vendored in `re/match/vendor/`
(licenses allow it): jgld.dll links libpng 1.0.5 + zlib 1.0.2 (`jgld_png_*.cpp`, `jgld_zlib_*.cpp`, /Od /ZI /GZ,
stock config; zlib sits at 0x1009bdd0+, after the CRT), golf_clean.exe links IJG libjpeg 6a (`golf_jpeg_*.cpp`, /O2;
`vendor/jpeg-6a/jconfig.h` is ours: `INLINE __inline` and `MAX_ALLOC_CHUNK 65520L`, both proven by the binary).
Each wrapper is `// LANG c` + `#include "vendor/.../x.c"` + MATCH lines from match_autoname.py. In Git Bash pass
`MSYS_NO_PATHCONV=1` when giving `--flags "/O2"`, or MSYS rewrites it as a path and nothing matches. Identical
functions folded by the exe's linker (/OPT:ICF: empty `ret`, `return 0`) keep one MATCH with "name not determined".
Release exe by hand (`re/match/golf_classes.cpp`, `log/fix.py` re-matches an edited g2s `.best.cpp`): a function that
ends `call X; ret` instead of a tail `jmp X` is a destructor (VC6 never tail-calls the implicit member/base dtors);
`push 3; call [vtbl]` is `delete[]` of a polymorphic object; member accesses compile closer than Ghidra's temporaries
(`if (++m_x > 20)`, `m_cap -= 10; if (m_cap == 0)`); two-case if chains that test with `je` first are a `switch`;
`(cond - 1 & mask) + k` is a ternary written `i < 8 ? 0x10 : 0x20`; compare operand order follows the source.
match.py masks an original operand as an address only where the DLL's base relocations say so (the exe has none and
keeps a range test that starts at the first section, so the image base itself is a constant): jgld's `or ecx, 0x10000000` is FILE_FLAG_RANDOM_ACCESS, equal to its image base.
jgld.dll's ~40 sprite blitters onto 16-bit surfaces (4.2-4.9 KB each) share one C skeleton and differ only in their 8
inline `__asm` loops: `re/tools/blit/gen.py <addr> <name> ...` (`TPL=<template>`) fills `re/tools/blit/body*.cpp` with each function's blocks (found
as `push esi; push edi` .. `pop edi; pop esi`), giving `jgld_blit*.cpp` (69 functions; templates and generators in re/tools/blit/).
Release exe hand matching scales with parallel local agents, one output file each (`golf_hand_NN.cpp`, flags
`/O2 /GX`: /GX only adds the SEH frames the original has); their notes are in `log/probe/*_notes.txt`. Function extents Ghidra gets wrong (noreturn calls, trailing pads, cut-off tails) go in `re/match/boundaries.tsv`. What is left near 99% is mostly VC6 /O2 scheduling and register allocation that source rewrites do not move: the permuter's
score line `exact% (regs-renamed %, in place %)` shows it (regs-renamed == exact means the sequence itself differs, not
just register names); 5 near-misses at 98.7-99.4% did not move in ~1,200 compiles each (2026-10-05). In the exe (no
relocations) fixed addresses can be literal constants (`(T*)0x519a60`), but a global object called through its vtable
must be an extern symbol.
Library-header code (VC6 STL, old iostream inlines): compile an instantiation and let
`re/tools/match_autoname.py <src> --module <m> --range LO HI [--prefer regex]` assign every obj function to the
addresses it matches at 100% (it flags names that compile identically). Terrain.dll (phase 1, done 2026-10-05): all 170
game functions; the 141 functions between 0x100158e0 and 0x100378c0 are CRT fragments (re/match/skip.tsv).
Terrain flags are `/Od /ZI /GZ /GX /MTd` (/MTd only changes the iostream lock inlines); its objects include the
OpenGL SuperBible's bitmap.c (LoadDIBitmap/SaveDIBitmap, compiled as C), a TextureLoad variant and the NeHe
LoadTGA. Tile::faceNormal (0x10011d60) is called with the Tile in ecx (smoothNormals), so it is a Tile
method, not a Terrain one; 0x1000bb70/0x1000bba0 are unreferenced empty ctor/dtor bodies (std::_Lockit without _MT). Ternaries written as `x = (a < b) ? a : b;` and stdlib's `__min(a, b)` compile differently in debug.
`// FLAGS <module>: ...`. `py -3.12 re/tools/match.py re/match/<file>.cpp` compiles with VC6, compares
normalized instructions against the anchored binary, writes `log/diff/<addr>_<name>.match.csv`.
Flags found so far: `golf_clean.exe` `/O2` (or `/Ox`, indistinguishable so far); `Terrain.dll`
`/Od /ZI /GZ /GX` (debug, C++ EH on: `getInstance` has an SEH frame; /GX changes nothing else).
29 functions at 100%: the 4 Terrain accessors in both builds and ALL 22 live exported Terrain.dll methods
(incl. render 399 ins, localRender 380, drawLine 219, resize 150), plus 29 unexported callees
(Tile accessors/reset/setTypeId/layPath/calcNormals, buildArrays, reloadTextures, initGL, normalize;
`terrain_tile*.cpp`) and 9 large ones (Tile::render, drawTileObjects, isCulled, elevate/lowerCorner, the
Terrain ctor, drawTile, the texture loader "relight", 0x10038900), setTypeId's callees (face blending
0x10013670, type-6/7 enter/leave walks) and loadLighting; golf_clean.exe at /O2: 151 live functions
(golf_util/golf_small*/golf_story.cpp: RNG, clamp, distance, tile accessors, storyText, playSound,
window z-order, string table, block allocator, sine table...).
2243 functions (170 Terrain.dll + 1072 exe + 756 jgld.dll + 245 sound.dll, 2026-10-05; count with `grep -h "// MATCH:" re/match/*.cpp`). Release-build
misses are kept in `re/match/wip/` with what was tried (register roles, loop pointer anchors); the 100% test
only globs `re/match/*.cpp`. match.py compares a tail-call `jmp` to another function like a call.
match.py also compares switch jump/index tables entry by entry once the code matches (2026-10-03): it caught
`rebuild` and the texture loader with case 1 and 2 swapped (identical code, masked string addresses), and
Snd484::setMode whose default is the `m_54 = m` store. Masked string operands are not compared: check them
by hand when cases differ only by a literal. Unreferenced locals show only as frame size (loadLighting: 0x1c bytes). The ctor fixes the type table:
0x25 x {char name[0x14]; int count;} at +0x2c (names: 0 Tee, 2 Fairway, 4 Rough, ... in terrain_ctor.cpp). VC6 puts switch jump tables after the function inside the obj symbol;
match.py strips them (reloc targets into the body's tail) and still compares case-block order. Original source file:
`C:\Projects\3DTerrainLowPoly\Terrain.cpp` (assert in localRender). With /ZI, `__LINE__` is relative to a
per-function variable, so an assert must sit at the same line offset as in the original (localRender:
base+3). VC6's STL headers are 8.3-named on the CD (fctional, algrithm, stdxcept, ...); long-name copies
were added to tools/vc6/vc98/include. Style differences the matcher resolves: `continue` vs a single `&&`
condition (localRender vs render), variable-left vs expression-left comparisons, early return vs wrapping if.
The shim links VC6 output: `shim\build_vc6.bat` compiles `re/match/terrain.cpp` (/O2 /Zl) and
`/DSG_VC6_TERRAIN` makes it the tileAt detour (the original 57 bytes appear verbatim in winmm.dll). Debug builds compile almost literally, so they reveal source shape the release
optimizer erased (e.g. Terrain accessors delegate to inline `Tile` methods). When several spellings
compile identically, record that the source is NOT determined there. 100% match = C4 evidence.

### Ghidra project

Master `SimGolf.gpr` holds `golf_clean.exe` (2,207 functions after analysis), `jgld.dll`,
`Terrain.dll`, `sound.dll`. Rebuild recipe (~4 min):
```
<ghidra>\support\analyzeHeadless.bat C:\Users\maria\Desktop\Proyectos\SimGolf SimGolf ^
  -import original\golf_clean.exe original\jgld.dll original\Terrain.dll original\sound.dll
```
Use the `ghidra-pool` skill for slots. Without the MCP: `py -3.12 re\tools\decomp.py 0x<addr>`.

## Tests (`test-suite` skill)

```
py -3.12 -m pytest -m static                  # anchors, SafeDisc removal, portability, shim surface, gate unit tests
py -3.12 -m pytest -m "runtime and not slow"  # boots the game windowed; Frida; input; in-game; patches; A/B tool
py -3.12 -m pytest -m ghidra                  # master project, headless facts via a pool slot, MCP tools/list
py -3.12 -m pytest -m slow                    # 5x boot reliability, full intro through the Bink fix
```

## Workflow rules

### NO-GUESSING
Report only what the decompilation/disassembly literally shows; cite the address of every constant
and offset; raw hex + signed decimal for sign-sensitive values; unclear -> `[UNCERTAIN U-NNNN]`
filed in `UNCERTAINTIES.md`. Banned: *probably, likely, seems to, appears to, I think, presumably,
might be, maybe* (the gate rejects notes containing them). Verify claims you write: the first note
here claimed `Terrain.dll` called the exe's `tileAt`; the import table and a runtime counter said no.

### Confidence ladder (`re-classify` skill)
`re/CONFIDENCE.md` is the rubric; `scripts/re_classify.py` is the only writer of `hooks.csv` and
`re/analysis/CHANGELOG.md`. One level at a time. C3 needs a GREEN path-1 A/B with install witness and
an anti-island caller and callee. C4 needs a canonical-scenario ON/OFF run (D-0002). Path-1 is never
C4 evidence.
`hooks.csv` is keyed by (module, addr): the three DLLs share the image base 0x10000000 and 204 RVAs collide.
`re_classify.py batch <tsv...> --to C1` promotes every row of a names TSV through the same gates (DLL VAs are
converted to RVAs; one CHANGELOG line each); `status --summary` prints counts per module; `retag <tsv...>` copies a
TSV's subsystem column onto tracked rows (level unchanged). Subsystem `net` (Jackal network layer, exe 0x497b40-0x49bec0)
was added 2026-10-06. `re/functions_ghidra*.tsv` are exported from the master with
`-postScript ListFunctions.java <out.tsv> -readOnly` (namespaced names; thunks carry their target's name, no `thunk_`
prefix, so tools exclude them by size <= 5).

### Names and systems (`re/names/`, `re/analysis/systems/`)
`re/names/<id>.tsv` (`module addr name subsystem evidence purpose`) and `<id>_globals.tsv` (`module addr name type
evidence`) hold evidence-backed names. `terrain.tsv` and `libs.tsv` (libpng/zlib/libjpeg) come from matched sources
via `re/tools/names_from_match.py`; `exe_0..5`, `jgld_0..1`, `sound_0..1` were named by agents from
`log/naming_brief.md` (2026-10-06) with a system writeup each in `re/analysis/systems/<id>.md`. Before committing
new ones: `re/tools/names_sanitize.py` (quoted binary text -> placeholder; the repo is public) and
`re/tools/names_validate.py <id>...` (also checks names/addresses unique across all files of a module) and
`re/tools/names_check_evidence.py` (every `string 0x..` / `import X` citation must be referenced by the function;
it caught 2 round-1 sound.dll names whose cited string the function never touches). sound.dll round 2 (`sound_2..5`,
2026-10-06) took it from 127 to 471 named; the mixer dispatch (`count` vs 0xb) selects accumulate vs overwrite bodies
(verified: 0x10019770 and 0x10019b20 differ only by `fadd [edi]`). Withdrawn names go in `re/names/withdrawn.tsv`
(ApplyNames resets them to FUN_; `re_classify.py demote` lowers the row). `ghidra/scripts/ApplyNames.java <re/names dir>
[overwrite]` applies them to a program (USER names kept unless `overwrite`, IMPORTED always kept, `Class::method` ->
namespaces), run headless with `-process <prog> -noanalysis`.
`re/tools/xref.py <module> 0x<addr>` gives offline callers/callees/strings (capstone, thunks resolved, cached in log/).
Round 2 (2026-10-06): exe_6, jgld_2 (Surface/SurfaceBase 60-slot vtable map), jgld_3 (Display/DisplayBase 86-slot
vtables 0x1011d640/0x1011d7dc, Array/Font/Palette), audits of sound_1 (log/naming/sound_1_audit.tsv) and of 4 global
conflicts (log/naming/globals_conflicts.tsv). names_validate.py also rejects a global address named in two files.
`re/tools/c2_note.py` writes a C2 transcription per function (reads/writes with width, branch signedness, callees with
imports and resolved thunks, constants, convention from `ret N` and registers read before written; never a
`## Purpose`), linking the names row, the system writeup and any hand-written note; `--all` covers every C1 row.
C2 = this mechanical transcription (decided 2026-10-06): all 2669 named functions were promoted with
`c2_note.py --all --out log/c2_batch.tsv` + `re_classify.py batch log/c2_batch.tsv --to C2` (notes in
re/analysis/<subsystem>/<addr>_<name>.md). C3 still needs a hand-written `## Purpose`, a hooked reimplementation and
a GREEN path-1 A/B. Regenerate a note after renaming by deleting it and rerunning (hand-written notes are never touched).
C3 (started 2026-10-06, 13 golf_clean.exe functions): hand-written reimplementations (not the recompiled match, which
proves no understanding) in `shim/src/re/golf_math.cpp` (pure integer leaves) and `golf_tables.cpp` (read-only table
lookups); a `## Purpose` added to the generated note (c2_note.py then never regenerates it); vectors in
`re/frida/hooks_registry.py`; fixtures in `re/frida/js/diff_fixtures.js` seed main-menu tables so every branch runs
(`golf_tables`: tiles, wall masks/heights, golfers, cell tables). Pick live (scoreboard reach > 0), matched, small
leaves first. Stateful functions: give the registry entry `state=[(addr | "$fixture_key", offset, size), ...]`;
diff_hook then snapshots those regions per vector, runs both arms from the same snapshot, compares the return value
AND the regions afterwards (CSV columns state_original/state_reimpl/state_changed, FNV hashes), and restores them.
Test-only override `orig_restore_state` (the original's value with its write undone) must read RED on state alone
(tests/test_diff_tool.py). Regions the running game writes concurrently make a run flaky. A function whose behaviour
depends on a fixture-set global gets one registry entry per setting (clearTile / clearTile_kind0).
`shim/src/re/golf_state.cpp`: Random::next, clearTile. `golf_writers.cpp` (batch 3): queueMessage, clearMatching,
pointsPopup, logTick, resetGolfer, appendOpinion (21 at C3). Not A/B'd on purpose: zPush 0x47e4c0 (fake windows in
the live z-list the game draws from), snapshotTables 0x462800 (no arguments and its pages already equal at the menu:
an empty GREEN is not evidence).
Function bodies are not always contiguous (86 functions, e.g. mainLoop 0x40f5c0: 25 ranges up to 0x421614).
`re/functions_ghidra*.tsv` carry a `ranges` column (ListFunctions.java) and xref.py / c2_note.py disassemble every
range; before that, calls in the far ranges were missed (clearMatching looked uncalled). After the index changes, run
`c2_note.py --refresh --out log/c2_refresh.tsv` + `re_classify.py retag log/c2_refresh.tsv` (rewrites generated notes
without a hand Purpose, refreshes callers/callees in hooks.csv).
`py -3.12 re/frida/diff_hook.py <names...>` runs all in one boot; then `re_classify.py promote <addr> --to C3 --file <cpp>`.
`xref.load_pe` is fast_load: call `pe.parse_data_directories()` before reading imports.

### A/B verification (`diff-original` skill)
Add vectors to `re/frida/hooks_registry.py`, run `py -3.12 re/frida/diff_hook.py <name>`. Never write
one-off harnesses; extend the registry. For live functions prefer **record/replay**:
`py -3.12 re/frida/record.py <name>` (real calls + memory, hook OFF) then `py -3.12 re/frida/replay.py <name>`
(Unicorn, offline: live == emulated original == reimpl). `re/tools/invariants.py <name>` infers contracts
from the recording that neighbours can be checked against.

### Scoreboard, coverage, version diff, failures
- `py -3.12 scripts/scoreboard.py` — Reach x Understanding x Verification per function; priority list.
- `py -3.12 re/tools/coverage_census.py --runs 3` — INT3 census of the sandbox scenario (exe + Terrain.dll).
- `py -3.12 re/tools/version_diff.py re/versions/v100.jsonl re/versions/v103.jsonl` — function matching.
- `re/tools/sbfl.py` (Ochiai), `re/tools/cov_bisect.py` (ddmin; needs a deterministic oracle).
- Read `re/analysis/TESTING_STRATEGY.md` before trusting any FAIL: first decide harness vs game.

### Function ID, saves, time travel
- **FidDb** `re/fid/simgolf_vc6.fidb` (not committed; `py -3.12 re/tools/build_fiddb.py`, ~45 min): the 12 VC6
  static CRT/STL/iostream libs (release + debug) + `original\JPEG.lib` (IJG libjpeg compress side + `CreateJPG`).
  `py -3.12 re/tools/fid_report.py --program <module>` -> `re/fid/<module>.fid.tsv`, read-only on a pool slot,
  applies nothing. `py -3.12 re/tools/fid_apply.py [--dry-run]` applies with Ghidra's ApplyFidEntriesCommand
  (USER/IMPORTED names untouched, `$L` labels reverted) -> `re/fid/<module>.fid_applied.tsv`. Applied to the master
  2026-10-03: exe 296, Terrain.dll 423, jgld.dll 270, sound.dll 287 renames (backup `ghidra/backup/`, not committed).
  Headless `-import` of a `.lib` does nothing ("Ignoring file '.'"); `BuildFidDb.java` imports the members itself.
- **.sve** `py -3.12 re/tools/sve.py info|diff|dump|check`: 100-byte header + raw dump of 86 globals (+ flag-gated
  tail) written by FUN_0040afa0 (`re/analysis/save/0040afa0_save_serializer.md`). 20 globals named (labels `g_*` in
  the master via `ghidra/scripts/LabelGlobals.java`; args `<va>#<name>`, the .bat splits on `=`); the rest: U-0004.
- **TTD** (WinDbg 1.2603 + TTD 1.11 installed): `py -3.12 re/tools/ttd_record.py` (one UAC prompt: ttd.exe
  -attach needs admin; the game runs unelevated) -> `log/ttd/<scenario>/*.run` (~1 GB). Query with
  `py -3.12 re/tools/ttd_query.py <run> --index` once, then `dx @$cursession.TTD.Memory(lo,hi,"r"|"w"|"e")`.
  The game runs far slower under TTD; ReadFile'd data is not a recorded write.

### Scenarios (`re/tools/scenario.py`)
`sandbox_basic` (menu -> Sandbox -> Monterey, 40 s), `course_season` (+ build tee/green with the Build
Course palette, open the hole with `H`, 240 s at x32 = ~6 game years), `championship` (installs the
fixture course, Play a Championship -> Easy -> course -> Gary Golf; the harness plays Gary's shots:
straight shot at the cup found by colour (red pennant + pale pole base) or the green centroid, zooming
out with X when the green is off-screen; 5/5 rounds finish in 45-107 s), `golfer_events` (organic golfer
info panel by clicking a golfer under its name label; then an INJECTED landmark notice 0x004722c0 called
on the game thread via re/frida/js/events.js), `golfer_stories` (4-hole course at x32 until the pair-story
advance FUN_00466370 returns 1; organic path toward the landmark, which needs both partners at chapter 4;
the 1-hole course can never advance a story because of the hole >= 2 gate, U-0003).
`stories_fixture` loads `tests/fixtures/stories/stories5.sve` (5 holes, not committed; README there) via
Continue Saved Game; the camera must be moved back over the course (`view_stories_course`) because story
advances only happen for golfers inside the screen rect. `stories9_fixture`: same with `stories9.sve` (Scotland, 9 holes;
do not zoom out: a 25 min zoomed-out run never passed stage 1). The camera is the tile pair at
0x4c2ba0/0x4c2ba4 (world->screen FUN_0042fb90); arrows move it 4 tiles, `events.js` `setCamera` 1 tile.
Golfer arrivals are capped by membership (type table 0x5849e0 +2): only member types arrive, so a save with
few members rarely produces story pairs, and a golfer whose mood drops below 0 quits and bans its type for good
(+0x29 = 0xff): `re/analysis/golfers/00406670_membership.md`, U-0003. Mood losses come from the rough, the
bad-lie complaint and the hole rating (FUN_0042dea0 flags 4 'too hard' / 8 'too easy'; short par 3s get 8 and every
birdie there costs mood). `mood5_fixture` (`mood5.sve`, 5 holes with fairways) keeps 8-16 golfers on the course for 15 min at x32 (bans still grow; best story stage 2). Chapters roll back because FUN_004669f0 recomputes the partner's chapter from its mood/counter and latest thought (`re/analysis/golfers/004669f0_story_reply.md`).
Census: `py -3.12 re/tools/coverage_census.py --scenario <name>`. All 6 patch-linked functions are now
reached (5 organically). Functions a scenario hooks with Frida are excluded from INT3 arming ("watched")
and recorded from the hook instead; injected calls use NativeFunction `exceptions: 'propagate'` so the
INT3 VEH sees them.
Input gotchas: posted mouse lParam is rescaled by Windows from the DPI-aware harness to the DPI-unaware
game (send physical coords: `Game._lp`); keys need WM_CHAR too; right click drops a build tool.

### Screenshots
`Game.shot()` BitBlts from the window DC (DWM redirection surface, unscaled 800x600, works for GDI and
OpenGL and while covered). Never grab screen pixels: another window on top produced false "hangs";
`PrintWindow` returned black frames 8-18% of the time (2026-10-02).

### Process hygiene
The game is **single-instance** (a second copy exits after ~2 s). Track the PID you spawn and kill
only that one. `Game.start()` refuses to run while another SimGolf window exists. Never blanket-kill
by name.

### Patching
Patches are `patches/<name>.ini` with `expect` bytes taken from the anchored binary. Default
`enabled=0` unless it is a compat fix. Cite the analysis note in `desc`. The static test validates
every committed patch against the anchors.

## Delegation

Workspace policy (`..\CLAUDE.md`): read-only surveys go to the account2 worker via `delegate.ps1`.
Ghidra MCP, Frida, builds, game runs, tracker writes stay local.

## Happy info pane
Keep `.happy/project-info.json` current when a standing fact changes (status, counts, key dates).

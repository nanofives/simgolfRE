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
`golf_terrain.cpp` (batch 4): slopeX, slopeY, slopeMix, heightAt42fa30 (25 at C3), fixture `terrain_slopes` seeds
the per-corner heights cornerHeights 0x40bfe0 returns first. A first run was GREEN with every slope 0 because the
fixture gave all corners of a cell one height: diff_hook now writes a `distinct_results` row and warns when every
vector returns the same value with no state change; always look at the spread before promoting.
`golf_course.cpp` (batch 5): rateTile, heightBlend, evalPlacementArea (no matched source; written from the
instructions) — 28 at C3. Fixtures seed golfer records (0x5794c0, 0x100 each), course records (0x571ff4, 0x2e
each), the 0x5a4998 level table (51 per row), placed objects (0x58bcb8, 16 each) and the signed height grid at
0x838c1c that bilinearSample 0x4674c0 reads (16x16, 19-byte rows; amplitude matters: too small or too large and
heightBlend only returns its clamp bounds). One registry entry per global mode (`_mode1`, `_c0`, `_tick`, `_ct1`).
Parallel C3 (2026-10-07): agents prepare batches in their own files, never shared ones: `shim/src/re/<ID>.cpp`,
`re/frida/registry.d/<ID>.py` (`HOOKS = {...}`, merged by hooks_registry.py, duplicate names rejected),
`re/frida/js/fixtures.d/<ID>.js` (`Object.assign(globalThis.DIFF_FIXTURES, {...})`, loaded by diff_hook.py),
`log/c3/<ID>_purpose.md`; `shim\check_re.bat src\re\<ID>.cpp` compile-checks without deploying. One verifier
(the game is single-instance) builds once, runs diff_hook over all new names in one boot, checks each spread, inserts
the Purposes and promotes. Brief: log/c3_brief.md (local).
`shim/re_batches.txt` lists the batches linked into the shim; build.bat compiles the core files plus only those,
and diff_hook / hooks_registry load only their fixture and registry fragments (an unlisted fragment that fails
to import is skipped with a warning). `py -3.12 re/tools/c3_verify.py <ID>` reports each address (READY = all keys
GREEN, more than one distinct result or a state change, and a Purpose); `--keys` prints the keys for diff_hook;
`--apply` inserts the agent's Purpose with the A/B facts taken from the CSVs and promotes (refused ones are rolled
back). VC6 converts double to int with __ftol 0x4a6030: 64-bit fistp, low dword kept (NaN -> 0, not 0x80000000);
reimplementations write (int)(long long)v (c3b distance was RED until then).
First parallel round (2026-10-07): c3a 12, c3b 9, c3c 11, c3d 8 promoted (68 at C3), the three later batches
A/B'd at the same time on original\, instances\b and instances\c (6-8 s). Fixes it forced: diff_hook writes
`Class::method` keys as `Class_method` file names, records a bad entry or crash as ERROR and goes on, accepts a
number for a pointer argument (0 = NULL), and rewrites `Memory.alloc(` to `__keepAlloc(` in fixtures
(js/keepalive.js): Frida frees an allocation no JS value references, so a parent stored only at view+0x130 was
freed mid-run (c3d RED and access violations until then). `distinct_results` counts (return, final state) pairs.
Held at C2: raiseFromNeighbours 0x42f6e0 (fixture never exercises it), Snd::ctorDerived 0x484820 (no static caller).
Second round (2026-10-07): agents also built (`powershell -NoProfile -File shim/build_instance.ps1 -Id <ID>`) and
A/B'd on their own instances\<ID> until c3_verify said READY; the verifier re-ran every key on the main build and
promoted 26 (c3e 4, c3f 10, c3g 7, c3h 5; 94 at C3). `c3_verify.py --apply --skip <addrs>` holds back READY ones
whose A/B only reached guard branches (c3h Surface_blit/fillRegion/blit3: the DirectDraw path never ran). The
gate's leaf rule (>= 10 vectors without callees) and the caller rule held back 8 more.
Third round (2026-10-07): a fix-up agent (c3i) re-did the evidence of 11 held-back functions in the earlier batches'
files (9 promoted; clearBuffers held: its 10 vectors are one argument-free call repeated; Surface_blit held: the copy
path still never runs), and c3j/c3k/c3l added 16 (119 at C3; propagateType11 held: one of 7 vectors does anything).
Verify ONE batch per boot (parallel boots on instances are fine): running five batches' fixtures in one boot crashed
at appendDate and made appendToBuffer45b8b0 RED once (its input text 0x51a068 is now in its state regions).
Evidence review (2026-10-07): a GREEN with many distinct results can still miss branches. updateRollingStats (no arguments: one vector is one call over the fixture) never reached its acc <= 0 branch, freeAtTile hit 2 of 42 tiles, appendCourseTitle reached 2 of 4 suffix arms; all three now have fixtures that reach every branch (one registry entry per fixture variant: `_early`/`_late`, `_c`..`_f`) and are still GREEN. Before promoting, check which branches the fixture reaches, not only the spread.
Fourth round (2026-10-07): candidates were live C2 functions never offered before (log/c3/c3m..c3p_candidates.tsv); each agent also wrote a per-jcc branch coverage table in its purpose file (c3m measured it with Unicorn, c3p with a Python model checked against the CSVs). c3m 4, c3n 9, c3o 7, c3p 7 promoted (146 at C3). Window::dispatchCommand 0x47c970 held: GREEN, but its only static caller is itself (the real ones are virtual). With agents' instances running, the verifier needs SIMGOLF_ALLOW_MULTI=1 on the main build too. c3_verify's gate wants a `// 0x<addr>` comment starting a line for each address, so twins sharing one comment need a line each.
Fifth round (2026-10-07): live C2 candidates were exhausted, so candidates were statically called functions the scenarios never reach (reach 0; the gate needs a caller, not reach), ranked by callers already at C3. c3q 10, c3r 14, c3s 14, c3t 11 promoted (195 at C3), none held. A function whose callees are hooked in the same batch (flagToggle49eec0 -> bitTest/bitSet) was re-run with `SIMGOLF_HOOKS_OFF=<callees>` so the original arm used original callees (diff_hook.run(..., out_dir=log/c3/alt) keeps that run out of log/diff). Corrections it found: 0x4ae210 is jpeg_destroy_compress, not jpeg_abort_compress (identical wrappers; its callee 0x4afa90 is jpeg_destroy, renamed in libs.tsv and the match file, note regenerated, row re-recorded via demote C1 + promote C2; the Ghidra master keeps the old name until ApplyNames runs with overwrite), and skipToken's names row (delimiters { } [ ] $ =, not whitespace).
Sixth round (2026-10-07, agents on the claude2 account via Happy spawn_child account=claude2): c3u 8, c3v 9, c3w 8, c3x 8 promoted (228 at C3). Two batches were handed the same functions (flagIsSet/flagSet, the verifier's mistake): hooks_registry.py now loads enabled batches first and skips a duplicate in a batch that is not enabled with a warning (an enabled duplicate still fails). Re-run with callee hooks off: flagToggle49ee90 (flagIsSet/flagSet), Button::setColorHover (Widget_setQuad70, another batch's hook). c3_verify reads only `**Purpose:**` paragraphs (a `### Purpose` heading reads as no-purpose), and its READY does not check the gate's leaf rule (>= 10 vectors without callees): ListModel::selectedId and comboCurrentIndex were refused at 8/7 and passed at 11. exe_5.tsv's flag trio rows now describe the list-position-to-mask-bit lookup.
Seventh round (2026-10-07, claude2): c3y 5, c3z 6, c3aa 8, c3ab 8 (a revisit batch over earlier agents' "not attempted" notes) promoted (255 at C3). Gate fix: re_classify.py's leaf rule counted diff_hook's state_regions and distinct_results rows as vectors and read only the first CSV of an address; it now counts vector rows summed over all of an address's path-1 CSVs (one per registry key), every key GREEN and witnessed (tests/test_classify.py). Three leaves promoted under the old count with 8 real vectors (appendToBuffer45b8b0, Snd::setField38, Button::setMode) got 3 more each and stayed GREEN. Because the gate now sums every log/diff/<addr>_*.path1.csv, delete a CSV when its registry key is removed (wrapTextToWidth's five single-call keys were replaced by _acc0/_accN). The gate's source check wants a comment starting `// 0x<addr>` (not `// ---- 0x<addr>`), and a Purpose must cite an instruction address. Held then widened before promotion: emit_dht (2 -> 8 tables plus the sent_table skip key), wrapTextToWidth (5 -> 13 calls, every jcc side).
Eighth round (2026-10-08, claude2): c3ac 1 (holeMagazineEvent, 2.3 KB, five argument-free fixture variants each sweeping 18 holes), c3ad 1 (build16BitColorTable: overrides one slot of the real display object 0x83ad50, A/B at the menu only), c3ae 8 and c3af 8 (revisits) promoted (273 at C3). Functions over 2 KB mostly draw, play sound or read files; the message/event family (startMessage, postEvent, logTick, announceHoleType) writes fixed global buffers, so event builders are A/B-able with those buffers and the RNG seed 0x822d9c in state (patronEvent 0x4266b0 and announceBuilding 0x40e720 are the next candidates; expandTextMarkup 0x4942f0 too). Every function whose callees are hooked (same or earlier batch) was re-run with all those hooks off.
Ninth round (2026-10-08, claude2 on claude-opus-5): event builders, the C2 callers of the message family. c3ag 3 (patronEvent, substituteStoryNames, LandmarkAvailableNotice), c3ah 4 (addScore, clearObjectFootprint, membershipReport, announceBuilding), c3ai 3 (expandTextMarkup, buildScenarioIntro, showTutorialText), c3aj 1 (matchUpdate, 5.7 KB, 16 keys; its branch table was pinned with 23 mutation builds) promoted (284 at C3). These writers are flaky in long boots: the running menu writes the text buffer 0x51a068, the RNG seed and the ticker globals they share, so a single-vector state RED that moves between keys and passes on a re-run is that, not a defect. Verify them a few keys or one function per boot (log/c3/alt/chunks.py-style), re-run a RED once, and compare against the callees-off run; patronEvent was RED only when run after the LandmarkAvailableNotice fixtures in the same boot. Fixtures for these must read-modify-write shared flag words (0x59e7b8, 0x5a5a00) or the game dies after a few dozen calls. Dropped for file I/O: advanceStory/announceVipGolfer/emitGolferThought/buildGolferComment (loadStoryStrings 0x466b70 uses fgets), writeCourseInfo, narrateShot (saveBmp).
DLL C3 (2026-10-08): SG_HOOKs in jgld.dll / sound.dll (LoadLibrary'd after the shim starts) now install from the LoadLibraryA hook (SgHooksInstallPending, re/hooks.cpp; patches.cpp installs that hook even with no patch files, which it did not before). The tooling is keyed by (module, rva): registry entries carry `module=`, diff_hook looks DLL hooks up with SimGolfShim_FindHookM and writes `<rva>_<stem>_<key>.path1.csv` (jgld_, sound_, Terrain_), the gate reads a DLL row's CSVs by that tag and converts DLL caller VAs (0x10xxxxxx) to RVAs, c3_verify groups by module and passes --module, and SIMGOLF_HOOKS_OFF accepts `module:rva`. Pilot: jgld.dll LinkedList::find 0x72b0 (batch c3dll) is the first DLL function at C3. Keep one DLL per batch (purpose sections are keyed by address only).
Tenth round (2026-10-08, claude2): the first DLL batches. c3ak 12 + c3al 13 (jgld.dll: jgld's Random, rect helpers, libpng getters/setters, LinkedList/ListNode, Matrix::setTranslation, blueOf), c3am 11 (Terrain.dll: Tile/Terrain accessors and writers, iabs; Tile::isSolidType held because its only caller 0x10013670 is matched but unnamed), c3an 12 (sound.dll: command queues, voice/channel/sequencer record helpers, randRange) promoted (333 at C3: exe 284, jgld 26, sound 12, Terrain 11). sound.dll loads ~1 s after the game window (jgld.dll before it): diff_hook.py now polls each requested DLL's hook (rpc `installed`) for up to 60 s before running. Integer functions first: the x87 Vector3/Quat/Matrix arithmetic and sound.dll's fcomp code were left for a batch that handles the rounding.
Eleventh round (2026-10-08, claude2): c3ao 14 (jgld libpng row transforms and setters, fillWords, Palette::toRGBQuads), c3ap 13 (jgld x87 math: Vector3, Quat::setAxisAngle, Matrix::setRotation), c3aq 17 + Tile::isSolidType (Terrain.dll), c3ar 15 (sound.dll MIDI/sequencer/voice helpers, Voice_noteOff, nextRandomFloat) promoted (393 at C3: exe 284, jgld 53, Terrain 29, sound 27). x87 method (c3ap, with a negative control): the A/B thread's control word is 0x027f (53-bit, nearest); write each `fstp dword` spill as a float assignment and widen every operand with static_cast<double>, take constants and sin/cos from the DLL's own addresses; plain float expressions prove nothing (MSVC 2022 /O2 compiles some to x87 inheriting the 53-bit word, others to SSE). 0x10013670 was named Tile::blendFaceTextures (re/names/terrain_1.tsv, applied to the master) because it was the only caller of five Terrain functions the anti-island check held (idClass, flagCode, isOpenType, edgeKind are now promotable). A /Od function returning void can leave a plausible value in eax (std::_Construct): declare ret="void" and compare memory.
Twelfth round (2026-10-08, claude2): c3as 16 (Terrain.dll: idClass/flagCode/isOpenType/edgeKind, the x87 helpers degToRad/powf2/bernstein/hermitePoint/rotateAxis/normalize/faceNormal, std::list iterator helpers), c3at 16 (jgld: Quat::mul/mulAssign, Vector3::rotate, Matrix/Transform copies and setters, png_set_cHRM/gAMA), c3au 16 (sound.dll: bitPack/bitUnpack, Voice_setLevel, sequencer/channel helpers; floatCopySign held, its callers are unnamed), c3av 9 (jgld render: hline/vline, fills, dither, dashed lines, clear8, fillRectC8) promoted (450 at C3: exe 284, jgld 78, Terrain 45, sound 43). Live-hot functions: the game's render thread enters jgld Surface::fillRectC8 0x11dd0 ~30,000 times/s and parks its row skip in the module global 0x1012847c, so a state region on that global flaked and the A/B's own writes there crashed the game once. Fix (fixture c3av_surfaces_frozen): leave the global out of the state, verify the skip through multi-row pixel results, and suspend the game's render thread for that key's vectors (no allocation or lock while it is down, the global restored before ResumeThread); menu frames with the hooks on and off are pixel-identical. Keys containing `<`, `>` or `*` (std::list<Tile*>) must be passed to diff_hook from Python, not through the shell. Frida reports an x87 double return of +/-infinity as NaN.
Thirteenth round (2026-10-08, claude2): c3aw 13 (Terrain.dll: roundHalf via the DLL's own __ftol, Faces::build, eleven std::list helpers whose exact template names are 'not determined' in the match file), c3ax 16 (jgld: Vector3::scale, intersect/equal, libpng row transforms and helpers, zlib crc32/adler32/inflate_set_dictionary), c3ay 7 (jgld 16-bit Surface primitives on a synthetic Palette built on jgld's Palette vtable 0x11db10, and blit8from8), c3az 14 (sound.dll codec math, channel voice/pan, sequencer channel descriptors) promoted (500 at C3: exe 284, jgld 100, Terrain 58, sound 57). Findings: dashedHLine16 always takes colour 1 through Palette slot +0x18 and colour 2 through +0x1c whatever the format; blit8from8 writes the ARGUMENT surface from `this`; dashedVLine16 0xd4f0 reads its dash lengths through ebp after repurposing it as a pixel counter (0x1000d912), so the original faults (dropped); floatFloorToInt 0x3e400 rounds half away from zero, not floor (sound_5.tsv row corrected; held, its callers are unnamed). Callee-off runs that would switch off a function under test are split into separate boots (log/c3/alt/c3aw_runs.py).
Fourteenth round (2026-10-08, claude2, five batches, candidates from log/c3/round14_candidates.py: leads from the previous notes first, then called C2 rows no notes file mentions): c3ba 7 (Terrain: Tile::elevateCorner/lowerCorner/reset/calcNormals/setFaceTexture, initGrid, setTextureId), c3bb 12 (jgld: png_format_buffer, png_read_filter_row, png_do_unshift/expand_palette/dither, inflate_flush, Array_Rec14/94 count/reset, initDepthInfo), c3bc 6 (jgld render: blitT_8to8, blitKey8to16, blit16from16, line8/16, dashedLine8), c3bd 9 (sound codec leaves, Midi_scanTrack/seekTrackToTick), c3be 13 (exe: libjpeg writers, netNameOf, Surface/DC forwarders, hitBuildingTool) promoted (547 at C3: exe 297, jgld 119, Terrain 65, sound 66). Held: Array_Rec14::add / Array_Rec94::add (GREEN, but the operator-new growth branch cannot be A/B'd). Each batch's hooked callees were re-run off on the main build (log/c3/alt/c3*_main_off), all GREEN. Findings: blitT_8to8's descending bodies skip the leftmost (cols & 3) bytes when cols >= 4 (a defect of the original, covered); png_warning calls the handler at png_struct+0x44, so a fixture `ret` there covers png_read_filter_row's default arm without I/O; the corner switches of elevate/lowerCorner fault in the original for corners 2/4/6 (/GZ-filled slots). Unblockers found: naming 0x10013500 (matched Tile re-blend) frees Tile::blendFaceTextures; naming sound 0x10040ab0 / 0x10041020 (codecStageDispatch stages) frees floatCopySign and floatFloorToInt.
Callers named to unblock held rows (2026-10-08, re/names/terrain_2.tsv, sound_9.tsv with systems writeups): Tile::reblendNeighbourhood 0x10013500, codecMarkCorrelationJumps 0x10040ab0, codecWindowFeatures 0x10041020, codecFeatureDecision 0x1003f720 (neutral name: a per-half 0/1 flag from a weighted feature sum; what the flag means is not determined). Recipe: name row + names_validate/names_check_evidence, ApplyNames headless on the module, `re_classify.py batch <tsv> --to C1`, `c2_note.py --all --out <tsv>` + `batch --to C2`, then re-run `c3_verify.py <batch> --apply`.
Fifteenth round (2026-10-08, claude2, log/c3/round15_candidates.py): c3bf 3 (Terrain: blendFaceTextures, reblendNeighbourhood, smoothNormals), c3bg 10 (jgld: png_do_strip_filler/read_filler/gamma/expand/rgb_to_gray/read_interlace, png_combine_row, png_set_sRGB_gAMA_and_cHRM, inflate_fast, inflate_codes), c3bh 4 (jgld render: blitT_8to16, blitT_16to16, fill16, Sprite::remap8), c3bi 9 (sound: bitPermute, gaussSolveClamped, codecAmdfPitch, codecGainSmoother, the two named stages, Seq_cmdHandler10, Seq_scheduleEvent, Midi_Device::setChannelVolume), c3bj 9 (exe: libjpeg jinit_* controllers/setup, BinkPlayer::setTarget) promoted (584 at C3: exe 306, jgld 133, Terrain 68, sound 77). Findings: Surface::blitT and its three routines have no transparency test (jgld_0.tsv purposes corrected); the Sprite draw payloads are plain 8-bit bitmaps, not RLE, and the live Sprite::draw16 0x1bc40 shares seven loop globals with its twins (frozen-thread fixture needed); png_do_read_filler's 16-bit RGB bodies write below the row pointer and png_do_rgb_to_gray's 8-bit RGBA body uses the green coefficient for red (defects of the original, covered); DC_call84's siblings 0x475ba0/be0/c20 have no caller and no data reference.
Sixteenth round (2026-10-08, claude2, log/c3/round16_candidates.py; Tile::setTypeId's six matched callees named first in re/names/terrain_3.tsv: setPathFlagForType, resetFaceTextures, enter/leaveType6, enter/leaveType7): c3bk 7 (Terrain: setTypeId and those six), c3bl 2 (jgld: png_do_background, png_do_read_transformations, the latter GREEN with all 18 hooked png_do_* callees off), c3bm 6 (jgld Sprite: blitPair/blitPair1, draw16t x2, draw16m x2; every key suspends the game's first thread because the live Sprite::draw16 0x1bc40 shares the loop globals), c3bn 4 (sound: reflectionToLpc, lpcSynthesize with g_codecOrder 0x100b58a0 set to 10 by the fixture, codecPitchAnalysis, Seq_remarkEventRange), c3bo 4 (exe libjpeg: validate_script, jinit_c_master_control, prepare_for_pass, emit_eobrun) promoted (607 at C3: exe 310, jgld 141, Terrain 75, sound 81). A function reached only through a stored pointer (prepare_for_pass: `mov [edi], 0x4b5810` at 0x004b51fc in jinit_c_master_control, the only reference in the image) is promoted with `c3_verify.py <batch> --apply --callers <addr>=<caller>` (passed to re_classify promote --callers) and its Purpose cites the store; pass_startup 0x4b5ca0 and finish_pass 0x4b5cd0 are installed the same way. Findings: leaveType6 adds instead of subtracting on one corner (enter+leave leaves it 40.0 higher); draw16m's three mirror quadrants fault in the original (mask cursor stored only in one); the Voice_mix* leaves set their own x87 control word 0xf7f (64-bit, toward zero) and keep state in registers, so only an inline-asm copy would model them; jgld_1.tsv Sprite purposes corrected (blitPair blends two Sprites onto two Surfaces; the 0x10122dc0/dc4/dc8 selector is determined).
Seventeenth round (2026-10-08, claude2, log/c3/round17_candidates.py, no Terrain batch: what is left there is exported methods called from the exe or drawing): c3bp 10 (jgld Sprite draw family: draw16_1001cde0, the stipple pair 0x56450/0x576a0, draw_1001f2d0, the remap/plane/average variants 0x26240/0x27430/0x28630/0x298b0/0x2bcf0/0x2d890; 108 keys, one function per boot with the first thread frozen, log/c3/alt/c3bp_main.py), c3bq 7 (jgld: png_read_transform_info, png_set/get_IHDR, png_set_read_fn, Palette::reset/apply, Matrix::loadIdentity), c3br 4 (sound: quantizeParams, interpSubframeParams, codecSynthStage, codecTransformStage; one key per value of the mode global 0x100b58a8), c3bs 7 (exe: pass_startup, finish_pass_master, spreadShade/spreadAllShade, Surface::blitRLE, spawnObject, pickGolferName) promoted (635 at C3: exe 317, jgld 158, Terrain 75, sound 85). finish_pass_master 0x4b5cd0 was not a function in the master (reached only through the store at 0x004b5209): `ghidra/scripts/CreateFunctions.java <hex addrs>` creates such functions headless; then re-export re/functions_ghidra.tsv (ListFunctions), delete log/xref_<module>.json, and it matched IJG's finish_pass_master 100% (golf_jpeg_cmaster.cpp). Global names in re/match/jgld_blit*.cpp are not evidence (match.py masks relocated operands; 0x298b0's source says g_10128548, the binary writes 0x10128574). Dead in this build: png_set_background 0x6f230, png_set_rgb_to_gray 0x700f0 (no caller, no stored pointer). Frida's Stalker delivers no events in this process (c3br). Names corrected: ten jgld Sprite purposes (jgld_1.tsv), intPow/interpSubframeParams (sound_7.tsv), codecTransformStage (sound_5.tsv), and g_golferRoster 0x0058587a removed from exe_2_globals.tsv (it is field +0x2a of g_walkers record 0).
Cross-module callers (2026-10-08): xref.py cannot see the exe's calls into Terrain.dll, so every imported Terrain method had `callers none`. `re/tools/import_callers.py [--rva 0x<rva>] [--callers-arg 0x<rva>]` follows each Terrain import of golf_clean.exe to the export body (thunk followed) and lists the exe functions calling the exe's `jmp [slot]` import thunk (three `call [0x4ba19c]` in post-ret padding are reported, never counted). re_classify accepts a caller written `<module>:0x<addr>` and looks it up in that module; `c3_verify.py --apply` fills these automatically for Terrain rows with `callers none`. Only the exe's 26 Terrain imports benefit (jgld/sound are LoadLibrary'd); Tile methods the exe reaches inline (minHeight, maxHeight) and setSplineHeight stay blocked.
Eighteenth round (2026-10-08, claude2, log/c3/round18_candidates.py, which can now include rows earlier notes mention): c3bt 12 (the first Terrain exports via their exe callers: setType, getVariation, layPath, hasConnectedPath, updatePath, hasPath, setWall, elevate/lower/lowerEdgeCorner, calcNormals, calcAllNormals; GREEN with all 35 hooked Terrain callees off), c3bu 10 (the rest of the Sprite draw16 blend family plus 0x20430 and the 8-bit-destination 0x62dd0; log/c3/alt/sprite_main.py <batch> runs one function per boot hooks on and off), c3bv 4 (sound: codecProcessBlock, codecProcessResource, codecStageDispatch, codecFeatureDecision: the codec from codecProcessResource down is at C3 except 0x1003edc0, 0x10040740 and codecInitContextA/B), c3bw 11 (jgld: Quat/Matrix/Transform ctors and dtors, LinkedList ctor/dtor/clear/removeCurrent, inflate_trees_free; LinkedList::ctor's caller 0x100069b0 named constructTrackedList in re/names/jgld_4.tsv), c3bx 10 (exe: dump_buffer, StrList::dtor, close/release hooks, comboListQuery, four Window scroll/range forwarders) promoted (682 at C3: exe 327, jgld 179, Terrain 87, sound 89). Held: jpeg_set_quality 0x004ae740 (its _badstate key crashes on the main build only while c3aa's jpeg_add_quant_table 0x004ae600 hook is on: that C3 reimplementation misbehaves on its ERREXIT path with a returning error_exit, which c3aa's A/B never ran; log/c3/c3bx_notes.txt). Findings: LinkedList::clear leaks one node per list of 2+; the 565 Sprite blend members store without the final >>4 (only red's low bit survives); Terrain::setWall passes a byte flag inside a /GZ-filled dword; codecStageDispatch's chain writes outside its 0x2600 context, so its fixture context sits in a block with 0x800 of slack and the whole block is the state region. Frees through a caller-owned function pointer (a node's own vtable slot 0, a z_stream's zfree) are A/B-able with a counting stub; direct operator delete is not. A vtable dispatch is `[[obj]+slot]`: three c3bx drafts called `[obj+slot]` and faulted. Fixed after the round (2026-10-08): c3aa.cpp called libjpeg's error_exit as `(*F<fn**>(err,0))(cinfo)`, one dereference too many (F<T> already returns the dword at err+0), so all six ERREXIT paths jumped to the callback's first code bytes (0xfb1e0ff3 = `endbr32`); no c3aa key ever reached an ERREXIT, so its GREENs were unaffected. Fixed to `F<fn*>(err,0)(cinfo)`, a returning-error_exit key jpeg_add_quant_table_badstate added (fixture c3aa_add_quant_badstate), all 30 c3aa keys GREEN, and jpeg_set_quality promoted (683). Error paths of C3 functions are a blind spot when the fixture's handler never runs: give every ERREXIT a returning-handler key.
Nineteenth round (2026-10-09, claude2, log/c3/round19_candidates.py): c3by 21 (the rest of the Sprite `char flag` blend family: 12 on RGB555, 9 on RGB565, ten mirrored pairs; the 30-member sub-family is now covered), c3bz 8 (jgld Sprite: 0x22a10 / 0x24140 and 0x62dd0's six 8-bit siblings; 0x21720 held: no caller and no stored pointer, dead in this build; 0x5ed80 is live, ~7,300 calls/s from 0x15480, menu frames identical with its hook on and off), c3ca 10 (sound: the codec's last two stages, named in re/names/sound_9.tsv as codecSelectWindowFromJumps 0x10040740 and codecAlignAnalysisWindow 0x1003edc0, plus codecInitContextA/B and six channel/voice/DsBuffer helpers; codecStageDispatch, codecProcessResource and Voice_setLevel re-run GREEN with c3ca's hooks on and off, since they call them; the whole codec is now at C3), c3cb 13 (jgld: the zlib 1.0.2 inflate chain, inflateInit_/Init2_/Reset/End, inflate_blocks_new/reset/free, inflate_trees_bits/dynamic/fixed, inflate_codes_new/free and falloc via its store at 0x1009f065; zlib allocates only through the caller's z_stream zalloc/zfree, so a slot allocator and a recording deallocator make every Z_MEM_ERROR arm reachable), c3cc 10 (exe: releaseWithHookA/B, closeHookC via the vtable its installer NodeListC::ctor 0x487b40 sets, orderPair, scrollbarResetThumb, jpeg_set_linear_quality, out_of_memory, jpeg_mem_available, jpeg_open_backing_store, jpeg_destroy_compress) promoted (745 at C3: exe 338, jgld 221, Terrain 87, sound 99). Findings: huft_build 0x1009e7a0 reads past the values it stored for an incomplete code set with zero-length codes (its guard counts all n lengths, not the nonzero ones), so a fixture must give every length a nonzero value; Palette::update 0x6a3d0 calls QueryPerformanceCounter unconditionally and seeds its Random with it, so it can never be A/B'd; Sprite 0x5fd20's stepped half writes 0 instead of the caller's colour; codecInitContextA's names row corrected (sound_7.tsv). When a new batch hooks a function that existing C3 functions call, re-run those callers with the new hooks on (log/diff) and off (log/c3/alt) before committing.
Stored-pointer callers (2026-10-09): a survey of the C2 rows found the called pool nearly exhausted (exe 13 never offered, jgld 0, sound 94, Terrain 0) while 962 rows had `callers none`. `re/tools/pointer_callers.py <module> 0x<addr> | --callers-arg 0x<addr> | --survey` finds who installs a function reached only through a pointer: every dword in the mapped image equal to the body or to one of its incremental-link thunks (the debug DLLs' thunk table at the start of .text is read directly; Ghidra lists only part of it) is a STORE when it lies inside a function's code, else a TABLE slot whose table starts at the nearest preceding address some code holds (walking back only through dwords that point into code, so a neighbouring vtable is never crossed). It reproduces the four hand-checked cases (prepare_for_pass, finish_pass_master, falloc, closeHookC slot 2 of 0x4bb014) and finds installers for 428 of the 962: exe 88, jgld 203, sound 135, Terrain 2 (the Surface/Display vtables 0x1011d220/0x1011d640/0x1011d7dc among them). `c3_verify.py --apply` now fills callers in this order: --callers, the exe import table (Terrain), stored-pointer installers. An installer is reachability evidence, not proof the slot runs: the Purpose cites the store or slot.
Twentieth round (2026-10-09, claude2, log/c3/round20_candidates.py: pointer-reached rows with their installers, plus jgld zlib/libpng leads): c3cd 12 (jgld: Lib::setCommandHandler via Display slot 85, storeCommandHandler, the nine windowProc message forwarders onChar/onWmSize/.../onWmRButtonUp and onCommand, made A/B-able by swapping the module's live Lib pointer 0x1012872c for a recording sink while the first thread is frozen), c3ce 25 (jgld Surface vtable slots: acquire/releaseDC and DC2, pixelAddr, fillRect/fill2/drawLine/ditherRectDispatch, clip/bounds/size accessors; every earlier jgld render batch, 60 boots, re-run GREEN with them underneath, log/c3/alt/c3ce_crossrun.py), c3cf 13 (sound: Seq/Ctrl vtable slots, stream byte readers, startStream), c3cg 11 (exe: libjpeg marker writers, colour converters, request_virt_sarray/barray via a seeded alloc_small pool, Window::hitTestBorder, setFocusItem) and c3ch 7 (jgld: huft_build, inflate_blocks, inflate, png_read_data, png_crc_read/error/finish; c3cb's tree builders re-run GREEN over the new huft_build hook) promoted, plus Snd::flags 0x00484ff0 through c3f (its installer is slot 28 of 0x4bacf8; c3cg's new key Snd_flags_device ran its device branch for the first time) (814 at C3: exe 350, jgld 265, Terrain 87, sound 112). Held by judgement: the four jgld deleting destructors (operator delete is a direct call). `re/c3_hold.tsv` (module addr reason) lists functions held by a verifier's judgement that the gate cannot express; c3_verify --apply never promotes them (a c3f re-run had promoted clearBuffers, held since round 3: demoted back with the reason recorded). Findings: onChar passes its character inside a /GZ-filled dword (0xcccccc00 | ch); alloc_small 0x004aef70 is A/B-able with a seeded small_list pool header (no malloc); rgb_gray_convert / grayscale_convert loop `dec eax; js`, so num_rows = INT_MIN walks 2^31 rows; any fixture feeding huft_build an incomplete code set must make every length nonzero (the over-read hit the original arm again); names corrected for acquireDC2/releaseDC2 (pixel base, not a DC), fwdVfn18/20 (plain calls), getField7c (Palette pointer), startStream, Ctrl5b5c0_mapValue, Seq_getTrackFlagHigh.
Twenty-first round (2026-10-09, claude2; pool re-surveyed first with log/c3/survey.py, which classifies every C2 row as called / pointer with installer / no caller and judged before / never offered: the never-offered reachable rows were jgld Sprite 36, Display 45, Palette/Font 15, sound audio 91 and CRT 57, exe 12 CRT, Terrain none; log/c3/round21_candidates.py): c3ci 20 (jgld Sprite/SpriteBase vtable: field accessors, remap, drawQ and the ten draw/blt dispatchers over C3 blitters; one function per boot, log/c3/alt/c3ci_main.py), c3cj 17 (jgld Display: findMode/setMode/pickMode/pickModeFreq on a synthetic vtable whose mode slots are recording stubs, field accessors, setScale/getScale under a frozen first thread, the four Factory destroy slots), c3ck 12 (jgld: seven png chunk readers, four live Palette methods, windowProc via Display::createWindow's store), c3cl 9 (sound: Seq_findTrackById after naming its callee Track_setField8RaisingMax 0x1002e430 in re/names/sound_10.tsv, device open/close inners, getPlaybackTime, Channel_applyFlags) and c3cm 6 (sound CRT: strchr, strstr, strcpy, cvtdate, doubleExponentField, iosResetVtable) promoted (878 at C3: exe 350, jgld 314, Terrain 87, sound 127). Held: crtValidatePointer (its result is KERNEL32!IsBadWritePtr's; in re/c3_hold.tsv). getPlaybackTime was promoted although it calls timeGetTime: its vectors were shown to give identical results for nine clock values. `c3_verify.py --callees <addr>=<list|none>` overrides a callee hooks.csv recorded from a jump into a shared body (strstr's jmp into strchr, strcpy's into strcat's loop). Tool bug fixed: `re_classify.py retag` replaced callers with c2_note --refresh's xref-only view, so a refresh reset 68 recorded exe/installer callers to `none`; retag now merges callers (tests/test_classify.py), and the 68 were restored through retag with correcting CHANGELOG lines. Findings: VC6 debug _RPTF0 reports reach OutputDebugStringA when jgld's _CrtDbgMode[_CRT_WARN] (RVA 0x123378) is 2, and Frida's NativeFunction turns the resulting DBG_PRINTEXCEPTION_C into a failed rpc: fixtures set it to 0 for the key; [sprite+0x10] is the Palette pointer, not a key colour (jgld_2.tsv corrected); the Palette methods are live (~400 calls/s at the menu through the vtable) and windowProc dispatches onCommand from WM_COPYDATA, not WM_COMMAND; findMode always returns 0 and pickModeFreq passes three bytes of `this` with its flag (jgld_1.tsv corrected); Sprite::lineTo / dashTo's callees blitMask 0x2cec0 / blitMaskScaled 0x2d390 hang the game under the A/B (left at C2); drawQ's extent added to re/match/boundaries.tsv.
Undiscovered code (2026-10-09): 247 of the exe's 253 'unreachable' C2 rows (sound 109/113, jgld 54/127) were called from code Ghidra never made into functions. `re/tools/undiscovered.py <module>` takes the nearest referenced address (data pointer, direct call, or a referenced incremental-link thunk) before each call site outside every function as a missing entry, and rejects an entry whose only references are slots of a table used by an indirect `jmp [reg*4 + table]` (switch cases: the first pass created 26 such false functions, removed with `ghidra/scripts/DeleteFunctions.java`). 210 functions were created (`CreateFunctions.java`, master backup ghidra/backup/SimGolf.rep.pre_undiscovered_20261009) and named by agent N1 in re/names/undisc_exe/sound/jgld.tsv: 91 dynInit_* C++ dynamic-initializer stubs (CRT initializer tables 0x004c1004-0x004c1184, sound 0x10063104-0x10063130), 14 atexit destructor stubs, 38 this-adjustor thunks (_adj274 etc.; 0x274 is the Surface sub-object of every Window class), 18 libjpeg functions (match_autoname.py 100%; MATCH lines not yet in golf_jpeg_*.cpp) and Window base virtuals named from their vtable slots. The callers refresh then gave 292 C2 rows their first caller. What sound and jgld still show as unreachable sits behind thunks nothing references: incremental linking keeps dead code. Leads left: nine VC6 two-level switch dispatchers (e.g. 0x004a0a10) still missing from the listing, initializer slots 0x004c1114/0x004c1118, libjpeg start_pass_coef near 0x004b080c.
Cleanup (2026-10-09): the leads were closed. The 18 libjpeg MATCH lines are in golf_jpeg_*.cpp (all 100%). 87 more exe functions were created and named by agent N2 (re/names/undisc2_exe.tsv, re/analysis/systems/undisc2_exe.md): 9 two-level switch dispatchers (entries of viewCtor4a0740's class; each picks one of five this-adjustments from the flag word at [ecx-8]: 1 -0x168, 2 -0x1a0, 4 -0x4, 8 -0x8c, 0x10 -0x180), 49 vtordisp thunks and the 26 methods they reach, and the init stubs 0x474a80 / 0x4799c0 / 0x4799d0 (initializer slots 0x004c1114 / 0x004c1118). Order matters: a dispatcher created before its target methods absorbs them into its body (0x4a0d30, 0x4a1090 did), so create the targets and thunks first, then the dispatchers. 0x4799c0 is a bare 5-byte jmp and stays at C1 (c2_note skips thunks). The exe listing now holds 2,476 functions. Stored pointers with no installer (2026-10-09): of the exe's 92 such C2 rows, 88 were dynInit_* slots of the CRT initializer table, whose first slot follows a NULL at 0x4c1000 that __cinit 0x4a50db loads (0x4a50fb); pointer_callers.py now accepts a NULL-delimited table whose delimiter address code loads (tests/test_pointer_callers.py). Three were stores in code Ghidra never made a function: start_pass_main 0x4b05f0 and start_pass_coef 0x4b07b0 (created, 100% matched, names in libs.tsv). output_message 0x4aecf0 sat inside error_exit's body (noreturn exit); `ghidra/scripts/SetBodies.java <entry>:<size>...` applies boundaries.tsv-style extents and creates the missing function. TextView::create 0x491310 stays without an installer: its only reference is the global 0x4e44ec, which no instruction addresses. Exe: 2,479 functions.
Heap stand-in (2026-10-09, pilot batch c3cn): `re/frida/js/heapstandin.js` (loaded before every fixture) gives `__heapStandIn({module, alloc:[rva | {rva, args, sizeArgs, fill}], free:[rva], arenaSize})`: for one key the module's allocation entry points carve blocks from a fixture arena whose cursor is a state region (both arms get the SAME addresses) and its deallocation entry points only record the pointers (count at log+0); only the A/B thread takes these paths, every other thread is forwarded to the real allocator, and it reverts from setTimeout(0) (and reverts a leftover replacement before installing, which had made a following key ERROR). Proven with controls: null stub RED; three bodies wrong ONLY in their heap operation (missing free, oversized new, skipped guarded delete) RED on state with 0 return mismatches (shim/src/re/c3cn_negctl.cpp, built only through log/c3/c3cn_negctl_batches.txt, never listed in shim/re_batches.txt); 96,188 game allocations forwarded while installed with menu frames pixel-identical. c3cn promoted 61: the six jgld rows held for direct operator new/delete (Array_Rec14/94::add growth branch, Matrix/Transform/LinkedList/ListNode::deletingDtor; released from re/c3_hold.tsv and promoted with their own source files c3bb.cpp / c3cd.cpp), jgld allocation leaves (jalloc/jfree, png_malloc/free/zalloc/zfree, png_create/destroy_struct, zcalloc/zcfree, Array clears, Sprite::copyFrom), exe release leaves (Buf_releaseData/Buf_dtor, strFree, KeyTable::dtor, jpeg_get/free_large, list helpers, NodeList::clear after naming its callee operatorDelete_4a4ffc in re/names/exe_7.tsv), 22 sound list helpers (four instruction-identical families) and Terrain's std::list<Tile*> allocation layer (939 at C3: exe 361, jgld 336, Terrain 93, sound 149). The heap-blocked pool earlier notes listed (deleting destructors of every class, sound's *_scalarDtor family, create/alloc chains) is now ordinary C3 work.
Twenty-second round (2026-10-09, claude3 on claude-opus-5-5, log/c3/round22_candidates.py over the exe's never-offered pool; two claude2 launches hung because account2's managed policy disables bypass mode and sets allowManagedHooksOnly, so the repo-fleet exec-gate no longer forwards and every compound Bash command waits for an approval nobody sees): c3co 73 and c3cp 72 (the C++ static-initialization families: dynInit_* stubs installed by __cinit 0x4a50db through the initializer table 0x4c1000, initGlobalArray_*, atexitRegister_*, the Surface exit routines; _atexit/_onexit replaced by a recorder on the A/B thread, the ~290 KB of globals the constructors write saved and pattern-filled with the first thread frozen, the live display 0x83ad50 swapped for a recording one), c3cq 105 (the static constructors and atexit registrations the stubs reach, three eh-vector array constructors, errnoPtr/doserrnoPtr, initFpControl, initGlobalReader/destroyGlobalReader), c3cr 22 (ListBox bodies over its two virtual bases via the vbtable at this-0x58, their vtordisp thunks in 0x4bb260, Window::isFocusChild, Container::cycleSelection, EditBox::cycleSelectionIfOwned, Button::timerCallback) and c3cs 19 (libjpeg Huffman encoder, scan headers and the virtual-array I/O, measured branch coverage 196/197 jcc) promoted (1,230 at C3: exe 652, jgld 336, Terrain 93, sound 149). Main build: all 527 keys GREEN with every hook on and again with all other batches' hooked callees off (log/c3/alt/r22_main.py; the agents' instance runs, which linked only their own batch, are kept in log/c3/alt/r22_instance). These hooks run during real startup (the shim installs them in DllMain, before __cinit), the runtime suite passes (18) with them; in the slow suite test_full_intro_with_bink_dib timed out once and passed on the re-run, and test_time_warp_moves_the_calendar failed 2 of 8 runs with the hooks on (changed fraction 0.0125 against its 0.02 threshold; identical frames score 0, so the date moved) and passed 4 of 4 with the 291 new hooks off. Bisect baseline (log/c3/alt/timewarp_bisect.py, 8 runs each, crops in log/c3/alt/timewarp/): hooks on 8/8 PASS (0.104-0.160), hooks off 7/8 (one FAIL at 0.0130), so the hooks are not the cause. The test's measure is: an exact one-year advance (June 2001 -> June 2002) changes one digit and scores under 0.02, and in 4 of 16 runs a crop holds no date text at all (terrain or a black frame), so those runs pass or fail on the background. Frozen-thread fixtures: a cooperative SuspendThread NativeFunction releases Frida's JS lock, so the main thread can be suspended while holding it inside a heap-stand-in callback (cdb showed gum-js-loop in RtlEnterCriticalSection); c3cq's fixtures call Suspend/GetContext/Resume with `scheduling: 'exclusive'`, freeze only when the main thread's eip is in golf_clean.exe .text, and flush a pending resume synchronously at the start of the next key (`__c3cq.begin()`). Follow-ups: the names Cache483010::ctorVtbl, Surface_init and netTimerReadA/B were not supported by the disassembly (renamed 2026-10-09 to Cache483010::dtor, Surface_free, netBeginOp0/1); 0x483da0-0x483dcb is code outside every function reached from Timer::stop 0x486f06; jpeg_gen_optimal_table's length > 32 arm (0x004b11ac) never terminates after a returning error handler; negative-control sources shim/src/re/c3co_negctl.cpp (never in re_batches.txt).
Shared freeze helper (2026-10-09): `re/frida/js/freeze.js` (loaded by diff_hook after heapstandin.js) is c3cq's method for every fixture: `__freeze.thread()` suspends the process's first thread with exclusive-scheduled Win32 calls, keeps it only while its eip is in golf_clean.exe code (10,000 tries of 1 ms, then throws instead of running unfrozen) and returns an idempotent release(); `save/fill/setU32/atFlush` queue restores that run newest first BEFORE the resume, from setTimeout(0); diff_hook.js calls `__freeze.begin()` (a synchronous flush) before every fixture. The 14 freezing batches (c3av c3bm c3bp c3bu c3by c3bz c3cd c3ci c3cj c3ck c3co c3cp c3cq c3cr) were moved onto it and all 1,120 of their keys re-run GREEN on the main build in 60-key boots (log/c3/alt/freeze_recheck.py), the Sprite batches included, which had needed one function per boot. With the old 2,000-try budget nine c3bp draw keys ERRORed: during those keys the first thread is almost always in jgld/GDI code. New freezing fixtures call __freeze, never SuspendThread directly.
Twenty-third round (2026-10-09, claude3 on claude-opus-5-5, log/c3/round23_candidates.py: round 22's leads plus the never-offered pool): c3ct 27 (EditBox caret/drag family over wrapToLines/pixelToCharPos, ListBox::setSelection/onNavKey/onDrag with Ctrl/Shift through SimGolfShim_SetVirtualKey under a frozen thread, scrollbarOnDrag, Timer_fire, optionDialog/listPanel slot bodies and their vtordisp thunks, viewDispatchSlot27), c3cu 37 (view dispatchers, hotHit callbacks, panel thunks, the file-scope gSurfaceDtor_* over a recording display), c3cv 16 (libjpeg main/coef controllers, progressive Huffman encoder, pre_process; 237/241 jcc both sides, the other four unreachable), c3cw 14 (jgld Array/Font/Tracked/Palette destructor chains: heap stand-in plus the tracked-object list 0x10128428 swapped under a frozen thread) and c3cx 34 (sound.dll 3D position, timer-rate family with g_soundTimer swapped, MultiSound slots, CRT small-block-heap helpers) promoted (1,358 at C3: exe 732, jgld 350, Terrain 93, sound 183). Gate callees named first: re/names/exe_8.tsv (view slot-60 bodies), exe_9.tsv (optionDialogSlot28/38), sound_11.tsv (channel slots 16/28/39). Main build (log/c3/alt/r23_main.py): all 193 keys GREEN hooks on and with other batches' callees off; c3cp's 12 exit routines re-run GREEN over c3cu's gSurfaceDtor hooks both ways. One hooks-off boot lost its Frida script at ListBox::onNavKey_ctrl (the game died; a key ERROR, not a mismatch); the whole pass re-run in 20-key boots was 51/51 GREEN. Children were spawned before the Happy mute change; every child spawned after it sends no push notifications (Terminal repo, pushNotifications.ts gate on HAPPY_MUTE_PUSH / HAPPY_NOTIFY_PARENT_ON_COMPLETE). Findings: Tracked::dtor 0x6b80 never checks find's result (removeCurrent then deletes another object's node); start_pass_phuff keeps indexing with a bad table number after a returning ERREXIT1; compress_first_pass divides by h_samp_factor (0 faults at 0x004b0bc8); sound.dll runs on the system heap (0x100b5d20 = 1), so its small-block heaps are dead code; ListBox::onNavKey's body is a click handler that takes a point (name to revisit). Leads: finish_pass_phuff 0x004b3380 (stored at 0x004b1ff9) is not yet a function; an import stand-in would unblock MappedFile and Font destructor chains (c3cw notes).
Twenty-fourth round (2026-10-10, claude3 on claude-opus-5-5; candidates were C2 rows judged before, grouped by the drop reason the notes record next to their address, log/c3/round24_classify.py and round24_candidates.py): c3cy 26 (the IMPORT STAND-IN pilot `re/frida/js/importstandin.js`: `__importStandIn({module, imports: {'DLL!Name': {ret|impl|forward, record}}})` patches the module's IAT slots, read from the import descriptors because Frida's enumerateImports() misreports golf_clean.exe's slots, through a hand-encoded native stub that checks the thread id, so only the A/B thread enters JS and game threads jump straight to the real API; the call log `$implog` is compared state; one call per key; controls: null body RED, five bodies wrong only in one import call RED on exactly the vectors that reach it, ~706 M game-thread calls forwarded with menu frames identical; reimplementations call each import through the original's IAT slot; first users MappedFile open/close/dtor in both modules, the jgld Font destructor chain, cursor/mouse/time helpers and the Timer family), c3cz 48 (exe rows dropped for heap use: list/array growth, Surface ctor/free/open, the Cache483010/482f60 families, libjpeg alloc_small/large/sarray/barray, free_pool, self_destruct, jinit_* and finish_pass_phuff, created first in the master; its fixtures carry a heapstandin.js copy with scheduled allocation failures so every out-of-memory arm runs), c3da 20 and c3db 21 (exe drawing through synthetic Surfaces on jgld's real vtable and a recording display under a frozen thread: blits, sprite draws, panel/frame drawers, dotted lines, bevel boxes, convex polygon fills) and c3dc 26 (jgld: tracked-list ctor/deleteAll/teardown, DisplayBase/SurfaceBase/SpriteBase destructors, pixel format helpers, Surface::blit/blitT/blit16, the eight Sprite::draw dispatchers incl. the live drawB) promoted (1,499 at C3: exe 840, jgld 383, Terrain 93, sound 183). Gate names first: re/names/jgld_5.tsv (registerTrackedListTeardown). Main build (log/c3/alt/r24_main.py, all modules, callees from hooks.csv plus the agents' lists): 304 keys GREEN hooks on and with other batches' callees off; 23 existing C3 callers of the new hooks re-run both ways. Verifier fix: c3cy's Timer::stop returned void, but the original leaves a defined eax that Timer_fire 0x00495cb0 returns (`call 0x486ec0; pop esi; ret 4`), so Timer_fire went RED with the hook on; Timer::stop now returns the id / import result / display slot result and its keys compare it (a void reimplementation of a function a caller returns through is a defect the callee's own A/B cannot see). drawP_s223 ERRORed once on the freeze budget and passed on the re-run. Runtime 18 and slow 13/13 pass with all 141 hooks (several are hot: Cache483010::dtor ~1.2 M calls per minute). Tracker integrity: two promotions (registerExit_44b190 in round 22, MappedFile::dtor 0x492dc0 now) had written hooks.csv without a CHANGELOG line; both re-recorded by demote + promote, and tests/test_changelog_integrity.py now checks every C3/C4 row has a C2->C3 line. Names fixed after the round (2026-10-10, retagged with levels unchanged; note filenames keep the old names): Timer::startOnce/startRepeat -> Timer::startMmPeriodic/startMmOneShot (each named by its verified multimedia-timer mode; initStart* to match), drawLineH/V -> Widget_readPixel/Widget_writePixel, blendRed/blendGreen/drawPixelRun -> pixelRed/pixelGreen/pixelBlue, Image::hline/vline -> Image::dottedHLine/dottedVLine, drawColorRunA/B -> dottedHLine16/dottedVLine16, allocTileBuffer -> initList820f18, allocPairArray -> PairArray::ctor, jgld Lib::destroyDisplay -> Display::destroyAll. As reported: Timer::startOnce/startRepeat (TIME_PERIODIC vs TIME_ONESHOT swapped), drawLineH reads and drawLineV writes a pixel, blendRed/blendGreen/drawPixelRun return R/G/B, Image::hline/vline and drawColorRunA/B draw dotted lines, allocTileBuffer (a std::list static ctor), allocPairArray (allocates nothing), jgld Lib::destroyDisplay (Display vtable slot 3). Leads: a realloc entry in the heap stand-in (11 rows), 0x00486f40 window-timer proc (no row), the import-blocked rows c3cy lists.
Heap stand-in upgrade (2026-10-10): `__heapStandIn` now takes `realloc: [rva]` entries (p NULL allocates, n 0 records p as freed and returns NULL like the VC6 CRT, otherwise a new arena block with min(old, n) bytes copied and the old pointer recorded in the free log; the old size is known for blocks the stand-in carved, a fixture-made block is declared with h.register(ptr, size), an unknown block throws) and scheduled failures from c3cz's copy: arena header +4 allocation call counter, +8 fail mask (call k < 32 returns NULL when bit k is set), +0xc fail size (opts.failMask / failSize; opts.exhaust: 'null' returns NULL instead of throwing when the arena is full). Defaults keep the old behaviour: all 617 keys of the seven batches that use it re-run GREEN (log/c3/alt/heap_upgrade_recheck.log; three ERRORed in 40-key boots that lost the game and passed on a one-key re-run).
Twenty-fifth round (2026-10-10, claude3; first round under the model split: Opus for tooling pilots and jgld, Sonnet 5.5 for the rest; log/c3/round25_candidates.py over round 24's leads): c3dd 26 (Sonnet; the first realloc users: heapAlloc 0x474860 and its callers S3List_add/add2, strDup, strAssign, bufferCopyTo, Surface_get, View_attach, TextView::addImage, postMessageLine, listAddEntry, netHeapAlloc, plus Image_releaseAll, the gSurfaceCtor family, Flic::reset, Stream482dd0::dtor, VoiceRx::ctor; a control wrong only in its _realloc call is RED on state alone), c3de 15 (Sonnet; import-blocked exe rows: capsLockOff, showMessageBox, EditBox::insertChar, the HotList family, Timer::scalarDtor, startColorCycle, allocColorRange), c3df 12 (Sonnet; measureMarkupRun, wrapText, Text_put, the drawCentered2/drawRight/drawString family, seedCourseTable, enqueueSprite; Surface_drawScaled 0x473cb0 and Image_draw 0x473e60 HELD in re/c3_hold.tsv: their this+8 != 0 loadImage arm never runs), c3dg 21 (Opus; jgld MappedFile write side, palette/font/key handlers, Display::enumModes/present, the Sprite create/ctor/dtor chain with jgld's _expand stood in under the freeze flush) and c3dh 52 (Sonnet; sound.dll queues and timer clients, DirectSound/MIDI wrappers, mmio and wave-in, codec allocators, CRT helpers) promoted (1,623 at C3: exe 891, jgld 404, Terrain 93, sound 235). Main build (log/c3/alt/r25_main.py): 306 keys GREEN hooks on and with other batches' callees off; 20 existing C3 callers re-run both ways, incl. four sound.dll callers hooks.csv does not record (Seq_loadChannelDesc/mergeChannelDesc -> Sound_setName, codecInitContextA/B -> codecSetParams). Stand-in race fixed (found by c3dd): a key's revert timer could undo the stand-in the NEXT key had already installed at the same address (one false RED); heapstandin.js and importstandin.js now give every install a token and a timer only reverts its own; all 617 keys of the seven earlier batches that use either stand-in re-run GREEN with the fix (log/c3/alt/token_fix_recheck.log), and the runtime suite passes with every round-25 hook live. c3dg's lesson (log/c3_brief.md): scan a function's whole callee tree for imports before stubbing, or an unstubbed import lets the original arm touch live state. Names fixed after the round (retagged, levels unchanged): drawStringLeft -> drawStringCentered, drawString -> drawStringRightAt, strAssign -> strBufReset, netHeapAlloc -> S3List_addVoiceRx, Surface_get -> recordCopyStr20, bufferCopyTo -> recordCopyStr1c. As reported: drawStringLeft 0x477fc0 centres on x, drawString 0x478140 ends at x, strAssign takes a size, netHeapAlloc is a list append with a VoiceRx payload, Surface_get / bufferCopyTo are record copiers.
Twenty-sixth round (2026-10-10, claude3, model split; log/c3/round26_candidates.py over round 24's drop classes plus sizes): c3di 13 (Opus; window-tree UI destructors: layoutNode, Window::ctorInit/detach, View_dtor, Image_dtor, Base4805a0::dtor, the deleting destructors and _adj274 adjustors), c3dj 96 (Sonnet; sound.dll Ctrl/Device/Voice/Channel ctor and dtor families, DsBuffer COM methods, list and name helpers, Voice stop requests), c3dk 13 (Opus; jgld Display ctor chain, Sprite drawC/drawD/drawAlpha, Display::createWindow, allocDrawBuffer, Font::loadFile, four libpng readers; names first in re/names/jgld_6.tsv: DisplayBase::ctor, DisplayMember::ctor, strcpy), c3dl 43 (Sonnet; exe rows dropped earlier for heap or import use, Timer, Tooltip, sound-library loader) and c3dm 46 (Sonnet; exe destructors and list helpers, grid writers genNoise/markTileKind11/floodMark, the libjpeg compress API, mouse dispatchers, Snd/device wrappers) promoted (1,834 at C3: exe 993, jgld 417, Terrain 93, sound 331). Main build (log/c3/alt/r26_main.py, r26_rerun.py for lost boots): all 384 keys GREEN hooks on and with other batches' callees off; 21 existing C3 callers (29 keys) GREEN both ways, incl. ten sound.dll callers hooks.csv does not record. Held at C2: sound Midi_Device::ctor 0x9be0, Ch5bb90_ctor 0x2c490 and exe DC_calla0 0x475c20 (no caller, no stored pointer). Children that hit the session limit are `stopped`: a message to them lands in the transcript and is never read, so continue the work in a fresh child from the files on disk. The first c3di agent was stopped by a safety classifier while writing its fixture file; the second wrote it in small appends. Two batches hooked the same address twice (Image_dtor 0x474c40 in c3di and c3dl, clearSoundPtrs 0x484130 in c3dl and c3dm): scan every linked batch plus the new ones for duplicate SG_HOOK addresses before building. The gate sums every log/diff/<addr>_*.path1.csv, so an agent's leftover debug key CSV (00486c90_zz_dbg) refused Timer::ctor until deleted. unlinkFromActiveList 0x2b090 is called only from an unnamed 8-byte wrapper; it was promoted with its vtable-slot installers (slot 32 of 0x1005c234/0x1005c2ec/0x1005c528) through --callers. c3g's resetWidgetTable and c3de's HotList::cancelTip are void while the originals return a value, but no caller reads it (every call site checked). Findings: drawAlpha's opaque arm can never run (alpha <= 1.0 is tested before alpha >= 100.0 - g); drawC/drawD return an uninitialised 0xcccccccc; layoutNode skips resetting the second of three children; DsBuffer::unlock and isBusy run ~33 times/s on the multimedia-timer thread.
Twenty-seventh round (2026-10-11, claude3, model split; log/c3/round27_candidates.py takes every C2 row that has a recorded caller or a stored-pointer installer, by module, and for the exe by earlier drop reason; shared brief log/c3/round27_brief.md): c3dn 54 (Opus; jgld Display wrappers through the import stand-in, module-global functions under a frozen thread, GDI wrappers, Base/Palette/Font/Array constructors over a swapped tracked list, Surface/Sprite destructors, Factory::createFont/createSurface, libpng warnings), c3do 42 (Sonnet; sound mixer bodies, DsBuffer::ctor, MidiReader_dtor, Seq_rewindTracks after naming its callee ListValues_callSlot18 0x1001bc00 in re/names/sound_12.tsv), c3dp 52 (Sonnet; Terrain.dll: render, localRender, initTerrain/resetTerrain, initSystem/closeSystem, the ifstream lighting reader, LoadDIBitmap, std helpers; 856 of 858 branch sides), c3dq 44 (Sonnet; exe drawing: screen/modal stack, text-draw wrappers, wrapped text, window-tree draw list, nine-patch tilers) and c3dr 48 (Sonnet; exe static-init stubs, view vtordisp thunks, small logic rows) promoted, plus matchInputCode 0x47eee0 (c3e, round 2, held at 7 vectors; c3dr's keys run against c3e's linked hook, promoted with `c3_verify.py c3e --apply`) (2,075 at C3: exe 1,086, jgld 471, Terrain 145, sound 373). Main build (log/c3/alt/r27_main.py): all 570 keys GREEN hooks on and with other batches' callees off; 55 existing C3 callers (120 keys) GREEN both ways, plus c3dn's seven vtable callers and c3do's six c3dj destructors that call removeSoundNode, which hooks.csv does not record. Held (re/c3_hold.tsv): mixResampleG 0x15f70 (the original reads an uninitialised eof flag, loop path not covered), Channel_propagateFlag0 0x2ddb0 (only caller is itself); mixCopyF 0x17eb0 has no caller. Defect found on the main build: c3c's Widget_setQuad 0x476310 and c3x's Widget_setQuad70/74/78 (rounds 1 and 6) were void, while each original leaves its third argument in eax (0x0047631b, `ret 0x10`) and c3dq's drawCenteredText2 returns it on NULL text: all four now return it, their registry entries compare it (ret="int"), and they, drawCenteredText2 and the Button setColor* callers are GREEN with the hooks on and off (log/c3/alt/setquad_fix_recheck.py). The c3e/c3dr duplicate was found by the pre-build duplicate-SG_HOOK scan. Tools: pointer_callers.py counts a code reference only when the dword is an immediate/displacement operand of an instruction decoded from its function range's start (simulateShot held 0x004bb904 as bytes across two instructions at 0x004242ef and was reported as a vtable installer), and walks back through runs of NULL slots (up to 0x400 bytes) to the address code loads, so jgld's initializer table __xc_a 0x10122000 (walked by __cinit 0x1007f160) resolves; jgld's dynInit_* stubs were named in re/names/jgld_7.tsv. c3_verify.py now prints re_classify's error when it returns no verdict (the blank line that silently skipped Display::restore and appendNamedNode until a re-run). A registry key with a space (`ifstream::{vbase dtor}`) is split by the runner's whitespace key list: run such keys from Python. Names applied after the round (re/names, ApplyNames overwrite on all four modules, retag): deleteFontDC (was initFontTable), trace's purpose, drawTextLines/drawTextLinesAt/drawCenteredTextLines (were measureText/2/3: they draw and return the next y), drawNinePatch (drawComposite), 16 sound mixer bodies by format (mixStereoAdd ... mixRatedMono8Set) and dsEnumCallbackAppend, g_terrain 0x10106b4c (terrain_4_globals.tsv), and three Terrain methods renamed in their matched sources (still 100%): renderTileAtFiveOffsets (lowerEdge), enableBlackLight1 (light1Off), initLighting (initLists). The overwrite also applied eight Tile names from terrain_2/terrain_3 that the master had never received. Findings: png_chunk_warning's 80-byte buffer overflows by two bytes for a four-non-letter chunk name; png_destroy_read_struct's NULL-png arms cannot run (png_read_destroy reads [png+0xac] first); loadLighting passes glLightfv a fourth float read past each float[3].
Time-warp test (2026-10-11): test_time_warp_moves_the_calendar reads the game date g_date 0x834170 (appendDate 0x40d7b0 decodes year = date / 8192 + 2001, month = date / 1024 % 8; drawEmployeeDetailPanel passes it, logTick writes it) through the harness's new readU32 / Game.read_u32 and requires three game months; the old date-line pixel measure failed whenever exactly one year passed (one digit changed). 5/5 pass; at x1 the same steps move it 493 of the 3,072 required.
Twenty-eighth round (2026-10-11, claude3, model split; log/c3/round28_candidates.py, Terrain left out with 3 reachable rows; brief log/c3/round28_brief.md adds round 27's lessons): c3ds 36 (Opus; jgld libpng error entry points, chunk readers and read loops, blend/dither rect bodies and dispatchers, setClip, Palette::update, Display::setModeIndex, Font::create, strcpy, the jgld_7 dynInit stubs), c3dt 56 (Sonnet; sound wave-in and mmio wrappers, deleting-destructor wrappers, global-guarded forwarders, Sound_reapIdle, CRT lock wrappers and unlock funclets), c3dv 30 (Sonnet; exe text widgets, window refresh, list/button/combo, panel layout and draw bodies with their vtordisp thunks, drawRoutingPoly), c3dw 45 (Sonnet; exe message pumps, Terrain/window/display glue, CRT and file readers through the import stand-in) and c3dx 35 (Sonnet; exe scrollbars, list views, list/heap rows, Snd objects, buildStatusText, stampCourseTiles, snapshotTables) promoted (2,277 at C3: exe 1,196, jgld 507, Terrain 145, sound 429). Main build (log/c3/alt/r28_main.py): all 412 keys GREEN hooks on and with other batches' callees off (one List401c70_dtor ERROR, an access violation, GREEN alone 3/3 and in its chunk twice); existing C3 callers GREEN both ways at every step, plus Timer_fire over c3dw's pumpInputDisplay483da0 (Timer::stop tail-jumps into it). LONGJMP CATCHER (c3ds): libpng's error arms end in png_error's longjmp; a native stub over the first 5 bytes of the hook's trampoline and detour fills png_ptr's jmp_buf as VC6's setjmp would at the harness call, so the longjmp returns 1 to the harness (Interceptor.attach killed the game under the heap stand-in); PNG fixtures are whole streams with real CRCs (log/c3/c3ds_gen.py). Palette::update, judged never A/B-able in round 19, runs through the import stand-in. Bug class found by c3dx's caller run: panelSubSlot60 returns with `ret 4` but its detour and registry declared no argument, so its own A/B was GREEN while a C3 caller crashed; `re/tools/check_ret_args.py` (static test tests/test_ret_args.py) now checks every registry entry's abi and argument count against the original's `ret N` (tail jumps followed): 2,296 combinations, 0 mismatches. Silent promotion failures explained: re_classify wrote hooks.csv, then opening CHANGELOG.md for writing raised OSError [Errno 22] (seen on zRaise 0x47e580, caught by the changelog integrity test); both trackers are now written through `_write_retry` (tests/test_classify.py) and zRaise was demoted with the reason and re-promoted. c3_verify.py: `--keys-lines` prints one key per line (keys with spaces), and every address needs its own `// 0x<rva>` line (c3dt's shared group comments refused 14 until added). Named first (re/names): sound_13 (return0Pop4, setField6cReturn0, clearField6cReturn0: c3dt's callees), jgld_8 (png_default_error and the three registerArrayE0/Modes/DevmodesTeardown registrars), exe_10_globals (g_date). Renames after the round (overwrite, retag): Wave_In_Device::setStream / clearStream (op1/op2), Channel_recomputeGainsSetBit3 (Voice_stopAndFlag stops nothing), listCurrentItemValue, listPanelHoverAt, viewAddEntry (sendCommand), wrapMessageBuffer / drawMessageBuffer, optionDialogLayout, panelLayoutText / panelLayoutIconText, comboDrawFrame, panelDrawItem, panelDrawGrid. Findings: drawTooltip 0x47cdb0 calls the live display's +0xa4 even with no tooltip; radioSelect returns 0 on every path; renderTextWidget's append at 0x45b077 can never run; png_create_read_struct's zlib-error switch is dead (png_malloc never returns NULL); serviceWaveDevice's callee always returns 0.
C2 end states (2026-10-11, re/CONFIDENCE.md): rows that can never meet the C3 gates keep C2 with hooks.csv status
`dead` (no caller, no call/jmp/jcc encoding at an instruction boundary or outside every function, no stored pointer, not
exported) or `trivial` (<= 64 bytes of frame/stack/immediate/return instructions, constant eax), written by
`re_classify.py mark <addr...> --status dead|trivial` after `re/tools/endstate.py`'s checks; `--status transcribed
--reason` reopens. 265 marked (jgld 126+49, sound 69+7, Terrain 7+5, exe 2 trivial); open C2 958 -> 693; candidate
generators skip them (log/c3/round26_candidates.py's C2 pool). DC_calla0 0x475c20 is not dead: `call 0x475c20` at
0x480f30 sits in code outside every function (a caller round 15/26 missed). `log/c3/c2_survey.py` lists every C2 row
with reach, drop reason and size. Plan for the large simulation functions: re/C3_SIM_PLAN.md (shared leaves first:
playSound, playSoundAt, pumpAndRedraw, storyText, inRange, rebuildTileCoverage, scanShotArea, scanNearbyGolfers; then the
golfer text chain through a stood-in story file, the simulateShot/simulateBallFlight/trialShot cycle as one batch,
movement, updateGolfers alone, terrain editing; mainLoop and the asset loaders out of scope).
Simulation plan done (2026-10-11/12, claude3, one child per phase, verifier notes log/c3/sim_verifier_notes.md, runner
log/c3/alt/sim_main.py, children log/c3/sim_children.tsv): phase 0 c3dy 10 (the shared leaves plus Snd::play;
scanNearbyGolfers renamed addGolferToTable), phase 1 c3dz 6 (loadStoryStrings, computeStoryReply, advanceStory,
emitGolferThought, buildGolferComment, pairGolfersWithStory = announceVipGolfer), phase 5 c3ed 2 (stepCourseEditQueue =
editTerrainHeight, generateRandomCourse = applyTerrainEdit), phase 2 c3ea 4 (trialShot, chooseShotTarget =
simulateBallFlight, prepareShot = simulateShot, saveBmp), phase 3 c3eb 3 (pathStep, updateWalkers, placeAnimal), phase 4
c3ec 1 (updateGolfers, 12 keys, 734 mutants) promoted: 26 functions, C3 2,303 (exe 1,222). Dropped: narrateShot 0x407e00
and drawWindow 0x40d320 (drawWindow calls Surface_drawScaled, held in re/c3_hold.tsv; drawWindowFrame 0x40cdd0 and
drawThoughtBubble 0x45c200 are C2). Methods: the story file served by stand-ins for the static CRT's fopen/fgets/fclose
with the file cursor compared (c3dz; the KERNEL32 path mallocs a stream buffer and takes locks); calleeStand recording
stand-ins for every non-subject callee with their calls compared; uninitialised stack reads of the original reproduced by a
naked entry that rebuilds the original's frame, plus a thread-checked stub over __alloca_probe 0x4a6070 that seeds the slots
alike in both arms (c3ea); the three-cycle verified with each function's two partners off (log/c3/alt/sim_c3ea_cycle.py);
updateGolfers as one key per golfer state, each vector a whole call over all 152 golfer records. Every batch: all keys
GREEN with every hook on and with other batches' callees off; existing C3 callers re-run both ways (log/c3/alt/
sim_callers_of.py, sim_callers_rerun.py; matchUpdate and the round-9 event writers flake in both modes and pass on
single-key re-runs). Defects fixed in earlier C3 hooks: Snd::setVolume / Snd::setField34 (c3j) dropped the device forward
(and setVolume the scaled __ftol path), Snd::setPan (c3f) returned the clamped pan instead of the device call's eax; device
keys on fixture c3j_snd_dev. TOOL DEFECT (c3ea): shim/src/re/hooks.cpp read SIMGOLF_HOOKS_OFF into 2,048 bytes and a
longer list left every hook ON; fixed (any length, tests/test_runtime_hooks_off.py). It voided the callers-off passes of
rounds 22, 26, 27, 28 and c3dp's callees-off pass (2,244 characters); no gate depended on them. Windows Defender flags
negctl DLLs with a fixed-address flag page as Trojan:Win32/Bearfoos.B!ml (c3ec rebuilt its control with an exported
switch). Findings in the original: generateRandomCourse fills the tile-type table from an uninitialised stack slot for
course type 4 / types > 4 and has two unbounded searches; its river and smoothing loops step one row outside the grid;
placeAnimal's idiv by the frame count (0x0040189a) and updateGolfers' idiv by word +0xb2 + 6 (0x00428c38) fault at 0 / -6;
updateWalkers' list scan subtracts x in the y expression (0x00402ec1); updateGolfers plays the water sound at (ball x,
golfer y) (0x0042d7ff). updateGolfers' only call (0x00417609, mainLoop) lies outside mainLoop's recorded ranges.
Slow suite 13 passed with every sim hook.
Debt cleared (2026-10-12): names_sanitize.py had rewritten our own double-quoted prose in jgld_0.tsv and exe_10.md on
every run (now backticks; any quote of 4+ characters in re/names or systems/*.md is treated as binary text, so quote our
own wording with backticks), and it had already mangled wrapMessageBuffer's row in exe_1.tsv at commit (restored from
the disassembly, characters as hex). ApplyNames counted a name already equal as a rename when its source was ANALYSIS
(the demangled exe exports 0x4490d0..0x449150), so every overwrite run re-renamed them; it now keeps an equal name
whatever its source (overwrite on the exe: renamed=0). The two C0 sound rows were named from their vtable slots
(re/names/sound_14.tsv: Ch5c2ec_forwardSlot37 0x1002f680, enqueueWaveCmd10 0x1002fc10) and are C2.
UI / drawing plan (2026-10-12): re/C3_UI_PLAN.md. Round 29 = four batches by blocker (log/c3/round29_candidates.py,
round29_brief.md): c3ee image pipeline + shared filestandin.js (Opus; releases Image_draw / Surface_drawScaled from hold),
c3ef widget lifecycle + shared uitree.js (Opus), c3eg window plumbing with a user32 import preset, c3eh drawing bodies.
Twenty-ninth round (2026-10-12, claude3, model split; verifier runner log/c3/alt/r29_main.py, children
log/c3/round29_children.tsv): c3ee 17 (Opus; the image pipeline: loadImage, loadImageData, decodePcx8/16, the BMP loaders,
openImageAsset, cropSprite, Image::loadPcx/savePcx, loadFileWinResource, through the FILE STAND-IN `re/frida/js/filestandin.js`:
`__fileStandIn({module, kernel32, crt, files, imports})` serves fixture files to KERNEL32 CreateFile/GetFileSize/mapping
imports and to the exe's static CRT fopen/fread/fwrite/fgets/fseek layer on the A/B thread, bytes, cursors, VC6 FILE records
and an operation log all compared state; its new keys ran the load arms Image_draw, Surface_drawScaled and Surface_blit were
held for, so all three left re/c3_hold.tsv and were promoted through c3df / c3h), c3ef 36 + c3l's setButtonDefaults and
MsgBox::setButtonB (Opus; the widget lifecycle on REAL objects through `re/frida/js/uitree.js`: `__uiTree(opts)` builds
window-tree objects with the game's own constructors in one heap arena, a recording display at 0x0083ad50 that forwards
other threads' calls to the real one (the winmm timer thread calls its slot +0x20), recording Surfaces, a user32/winmm
import preset and allocation-failure scheduling; Button, EditBox, ListBox, ListModel, the composite view fields and
panels, layoutViewTree, Window::create), c3eg 18 (Sonnet; Window::show / invalidate / dispatchAccel / onKeyChar, press and
release slots, Surface_clip, relayoutClip, redrawDrawList, drawTooltip, drawCursor, ListBox::onMouse / onKey) and c3eh 12
(Sonnet; ListBox::measure / drawContents, rebuildFramePieces / drawFramePieces, drawScrollText, drawMatchScoreboard,
drawWindowFrame, drawCircle, comboSetItems, panelLayoutLabeledText) promoted: 87 functions, C3 2,388 (exe 1,307), open
C2 584. diff_hook loads filestandin.js after importstandin.js and uitree.js after freeze.js. Main build: every key GREEN
with every hook on and with other batches' callees off (c3ee 54, c3eh 14, c3ef 140, c3eg 19); existing C3 callers GREEN
both ways (the three held rows' 13 keys, c3eg's 13 callers / 36 keys); runtime suite 19 passed with all 87 hooks live.
Renames (re/names, ApplyNames overwrite renamed exactly the changed rows each time, systems writeup exe_12.md, globals
exe_12_globals.tsv): decodePcx8 / decodePcx16 (decodeImagePaletted / decodeImage16), Image_recreateSurface (Surface_paint),
decodeChunk7WordDelta, rebuildFramePieces / drawFramePieces (buildControlStrip47d610 / drawPanelContents),
panelLayoutLabeledText (panelBuildChildren), widgetTableDtor (View::ctorVtbl47d7c0), modelListDtor489c90 (ListBox::subDtor),
viewFieldDtor49d5a0 (Dialog::childDtor), EditBox::dtor 0x43cb50 / EditBox::releaseState 0x4860d0 (editBoxTimer_dtor /
EditBox::dtor), ListBox::onKey (ListBox::typeAhead). The disk filled during the round (the Frida temp leak above); every
child re-ran what failed in that window. Findings: decodePcx16 reads its colours through a table pointer its local cache's
constructor leaves NULL (any 16-bit image with pixels faults at 0x0049566c); Image::savePcx writes 102 uninitialised stack
bytes into every PCX header and can encode past its buffer; drawScrollText's line table has no bound (line 32 overwrites
its refresh switch, line 34 its line count) and it returns its second argument; drawMatchScoreboard always returns 0x300;
Window::onKeyChar toggles the debug object 0x008400c8 when `mdebug` is typed (table 0x004e4324); ListBox::onMouse divides
by [B+0x60] unchecked (0x0048c7ec); Window::dispatchAccel faults when +0x140 is set and +0x138 is 0. Lead: 23 older fixture
files swap 0x0083ad50 for a recording display without uitree.js's thread test (c3ef notes 4.1).
Display guard (2026-10-12, closes that lead): `re/frida/js/recdisplay.js` (loaded after freeze.js) gives
`__recDisplay.guard(fake[, real])`: the fixture display's vtable pointer is swapped for a native thunk table whose slots
run the fake's own slot on the thread that guarded it and jump to the real display's slot (counted) on any other thread,
without entering JS. Applied at 36 swap sites in 21 fixture files (c3ad overrides one real slot and c3cg swaps nothing:
unchanged); all 1,608 of their keys GREEN; a probe key showed 48 winmm-thread calls reaching an unguarded recorder (both
arms alike, so still GREEN) and 0 with the guard. Fixtures that set 0x0083ad50 to 0 can still fault the timer thread
(0x00486fba has no NULL test); none did. log/c3/dispguard_notes.txt.
Thirtieth round (2026-10-12, claude3, model split; log/c3/round30_candidates.py, round30_brief.md, runner
log/c3/alt/r30_main.py, which no longer switches off a batch's own hooks in the callees-off pass even when an agent's
list names an in-batch callee): c3el 7 (Opus; the MODAL pilot `re/frida/js/modal.js`, loaded after uitree.js: for one key
the C3 pumps are stood in on the A/B thread with a scripted input queue fed through the root window's own input handlers
and a virtual clock, so a modal loop runs a bounded number of iterations and leaves on a scripted Esc / click / Enter;
pump calls, events, script position and clock are compared state; past maxCalls / maxFrames the next pump faults on
purpose, the key is an ERROR and the game keeps running; contract in log/c3/c3el_notes.txt section 1; gfxFlushA,
pumpInput483c90 / 483cf0, Window::runModal, scrollTextWindow, runScrollText, textInputBox), c3ei 15 (Sonnet; image
consumers: loadPcxSurface, loadFonts, loadCursors, addSprite, drawQueuedSprite, composeGolferSprite, drawWindow,
drawThoughtBubble, the toolbar / amenity / building / record panels, drawPanel4810d0, scrollbarDraw), c3ej 38 (Sonnet;
the eight viewDispatch* dispatchers with one key per this-adjustment tag, the listPanel / optionDialog bodies and every
vtordisp thunk on real uitree.js composites, compositeScreen, uiPointerMove) and c3ek 22 + resetMsgObject (Sonnet;
TextView ctor / init / loadScript / drawContents, panelStop, LabelButton, Window::createFromRect, Button::create,
drawRichText, drawText, the combo chain) promoted: 83 functions, C3 2,470 (exe 1,389), open C2 514. Held at C2:
TextView::create (no caller; only the global 0x4e44ec references it). Main build: every key GREEN with every hook on and
with other batches' callees off (c3el 11, c3ei 15, c3ej 97, c3ek 54; two freeze-budget / own-hook ERRORs passed on
re-run); existing C3 callers GREEN both ways (c3el 19 keys, c3ei 1, c3ek 32: the drawText / drawCentered family); runtime
suite 19 passed with every round-30 hook live. Named first (re/names/exe_13.tsv, systems exe_13.md, C1 -> C2): the eleven
root-window input handlers rootWin_onPress1 .. rootWin_onChar0 (vtable 0x004ba2fc slots 29..39, outside every function
until created with CreateFunctions.java; exe index re-exported, 2,492 functions) and listPanelSlot28 0x0049e990. Renames:
formatGolferTitle -> composeGolferSprite (it composes the layered golfer sprite), resetMsgObject -> LabelButton::reset,
TextView::layout -> TextView::loadScript (a script-section interpreter). Lessons: under load a key's first allocations
can escape its arena (c3ej's fixtures now check and report an ERROR; re-run once); boots of 12-20 keys lose the game now
and then (re-run the boot). Findings: textInputBox's Backspace deletes the byte at the cursor and its width cut leaves the
length stale; flushRedraw drops a key that arrives the frame after another (scripts leave an empty frame between keys);
drawWindow ignores its third argument; drawAmenitiesPanel overwrites its own first argument's slot; drawRecords reads an
uninitialised slot for odd rotations; compositeScreen can union an uninitialised local into the dirty box; optionListDraw
never reads its third argument; ListModel::reset always returns 0. Round 31 leads: showPopup, skillPointDialog (fit
modal.js as it stands), allocTextBuffer, LabelButton::drawContents, TextView::render / build, then the 42 show* / select*
screens that call a pump.
Thirty-first round (2026-10-13, first on LIVING-PC, all six children on claude2 `claude-opus-5`: the Accenture policy that
blocked claude2 on MARIANO-PC is absent here; log/c3/round31_candidates.py takes every open C2 row calling a C3 pump except
mainLoop / gameMain / loadFlic, brief log/c3/round31_brief.md, runner log/c3/alt/r31_main.py): c3em 9 (canAfford, the
build-panel click / pick helpers, showPopup, skillPointDialog, showBulletinBoard), c3en 7 (selectDifficultyScreen,
showCreditsScreen, showFinancialReport, showHireEmployeeDialog, showBestScoresScreen, showPlayerCommentsReport,
showHistographScreen; branch coverage measured on an instrumented build, 144 of 162 conditions both sides), c3eo 4
(resetGolferState, showMembershipRoster, showHoleStatsScreen, showTournamentResults), c3ep 9 (listThemes, loadThemes,
listMatchingFiles, loadThemeFile, loadCourse, saveGame, loadGame through a directory stand-in and a low-level
__open / __read / __write stand-in in its own fixture, EditBox::create, promptAndSaveFile), c3eq 3 (playAudioFile,
BinkPlayer::play over stood-in binkw32 imports, TextView::render) and c3er 2 (drawScoreSummary, CareerRetirementFlow)
promoted: 34 functions, C3 2,504 (exe 1,423), open C2 480. Main build: all 116 keys GREEN with every hook on and with
other batches' hooked callees off; the existing C3 callers updateGolfers (over skillPointDialog) and showTutorialText (over
saveGame), 32 keys, GREEN both ways; static 360 and runtime 19 pass with every round-31 hook live. Never switch Timer::stop
0x00486ec0 off under an EditBox::create key: its original tail-jumps into pumpInputDisplay483da0 and pumps the real queue
on the A/B thread. Lessons: a modal loop that waits only on a pump outside modal.js's default visit set (showBulletinBoard's
pumpInput483bd0) needs `visits: [...]` or no frame is delivered; a stand-in's result must depend only on its arguments or on
compared memory (a JS-side per-call counter differs between the arms: 17 RED hire vectors); a UI_STATE arena larger than the
fixture's arenaSize makes diff_hook read and write past the arena and the key hangs instead of reading RED. Dropped with
leads: syncTiles / rebuildTerrainGrid (sweep the live 50x50 Terrain grid through Terrain.dll; a Terrain-shaped batch),
showGolferPairSelect, showRoutingMapScreen, showBuyLandScreen, showCourseReport (4,715 instructions, 204 jcc), allocTextBuffer
(no usable decompilation: ebp is `this`), LabelButton::drawContents, TextView::build (size only); showShortcutsScreen and the
five browser screens showThemePackSelect, pickAProScreen, courseActionMenu, loadGameBrowserScreen, showCharacterEditor keep
their Surfaces in frame-local records, so stand-in logs carry stack addresses that differ between arms: the unblocker is a
`{base, stride}` mask in calleeStand that records a frame pointer as an index (c3ep notes section 5). Name lead: fillSlots54
0x004a29a0 (log/c3/c3eq_names.tsv, no hooks.csv row). Findings: showPopup's `1 << line` and recorded-index arms are dead and it
compares an uninitialised slot when the text has no option line; clickSlotPanel's signed slot test reads the byte before
0x004c79a0; showFinancialReport writes entry 8 back into every drawn year row (0x0044f911); showHireEmployeeDialog returns 2
for a click on no row (0x00459837); showHistographScreen draws the previous event's text for kinds outside its switch windows;
resetGolferState's 18-iteration loop works on one course record (0x0046d11d) and applies its thresholds twice;
showMembershipRoster builds a tally nothing draws and strcpy's a 0x230-byte slot into a 19-byte buffer (0x00454fb6); a
championship save loses its `.chr` / `Default` suffix; fileExists 0x004378a0 never closes its FindFirstFileA handle.
Thirty-second round (2026-10-13, claude2 with model `opus` = claude-opus-5-5; Happy lists only aliases for claude2 and
silently runs a full id such as `claude-opus-5-5` as claude-sonnet-4-6. The Accenture policy (v33) reached LIVING-PC
mid-round: children run in don't-ask mode, unblocked by allow rules in ~/.claude-account2/settings.json; bare `py` stays
refused, `~/bin/py -3.12 ...` runs; writing new .ps1 files is refused. Four batches were stopped at the user's request and
continued by fresh sessions from their files. log/c3/round32_candidates.py takes round 31's drops plus the whole open C2
pool by module and drop class, brief log/c3/round32_brief.md, runner log/c3/alt/r32_main.py): c3es 1 (showShortcutsScreen,
over the FRAME MASK pilot `re/frida/js/framemask.js`, now loaded by diff_hook after modal.js: a pointer into the caller's
frame is logged as (object ordinal, offset) so recording stand-ins compare which frame object a call targets; controls: mask
on GREEN, null RED, mask off RED, three wrong-object bodies RED; the {base, stride} array path is coded but not yet proven),
c3et 5 (allocTextBuffer, showGolferPairSelect, TextView::build, LabelButton::drawContents, showRoutingMapScreen), c3eu 35 (exe
heap / import / file I/O rows: initSound, shutdownSound, the flic readers, logWrite and writeCourseInfo through fprintf /
fopen / fclose stand-ins, the rootWin input handlers, jpeg_CreateCompress ...), c3ev 65 (exe display / live / other rows:
movie readers, KeyTable::find, applyTileWalls, startNewCourse ...), c3ew 21 (sound wave-in / wave-out devices, SoundTimer,
sequencer handlers, Seq_allocTrackSlot), c3ex 31 (sound channel / device / DirectSound list / MultiSound / sequencer rows) and
c3ey 15 (syncTiles / rebuildTerrainGrid on a fixture Terrain object at [0x820ed0] with the six Terrain imports stood in at
the exe's IAT under a frozen thread, jgld Sprite::lineTo / dashTo / drawO / blitMask / blitMaskScaled, png_default_error /
read_data, the jgld_8 teardown registrars, three Terrain forwarders) promoted, plus Snd::ctorDerived 0x484820 (c3c, installer
arrayStaticInit_448160) and Window::dispatchCommand 0x47c970 (c3o, caller Window::onCommand): 175 functions, C3 2,679 (exe
1,533, jgld 517, Terrain 148, sound 481), open C2 305. Every batch has null and wrong-body controls (exported <Batch>NegCtl
switches); the controls found fixture gaps in c3ew, c3ex (keys that never froze, so never armed the switch) and c3ev (missing
vectors). Main build: all 360 keys GREEN with every hook on and with other batches' hooked callees off; 34 existing C3 callers
(80 keys) GREEN both ways, including seven hooks.csv does not record (EXTRA_CALLERS in r32_main.py); static 360 and runtime
19 pass with every hook live. Held: Channel_set3DBufferStub sound 0x37c00 (its only callee is `mov eax, 0xb`). Fixture rules:
never stub a key's own function's thunk (Interceptor.replace follows the jmp into the shim's detour); mask stack out-slots in
stand-in logs; default vtable answers in fixture objects are 0, not marker words; a fixture that reaches the walker list or
cellAt early in a boot must write the cell-table widths itself (showRoutingMapScreen divided by zero only when it ran first).
Dropped with leads: showThemePackSelect and the four browser screens (with ??_L stood in, the original's SEH unlink leaves
fs:[0] on a bogus in-stack record and the process dies; real array construction brings back the frame layout), showBuyLandScreen
and showCourseReport (not attempted), error_exit (with exit stood in it returns to a garbage address), the heavier sequencer
bodies and DirectSound list bodies, about 30 larger exe rows per batch. Leads: c3eu's trivial proposals null_method 0x4b01e0,
ListBox::Slot60 0x487af0, jpeg_mem_init 0x4b04b0 fail endstate.py's eax check with `0 ret(s)` (extent issue to look at);
add jgld fread 0x10082d80 to filestandin.js's CRT table (c3ey patches it at run time); rename allocTextBuffer -> drawTextRun
(log/c3/c3et_names.tsv). Findings: moviePlayFrame's catch-up loops wait for movieStepReturn1, which always returns 1, so the
stock game can hang there; KeyTable::find scans a count-0 table until a name matches; DirectSoundDevice::removeBuffer
decrements its count even for a buffer not on the list (0x1001336b); MultiSound::startVoice queues on g_soundEngine with no
NULL test (0x10039fa6); blitMaskScaled divides by abs(a4) before testing a4 < 0; showRoutingMapScreen's exit remap indexes a
20-entry table with golfers' hole bytes (>= 20 reads the stack); writeCourseInfo never checks fopen; shutdownSound's drain
loop never ends if a node is not unlinked.
End-game plan (2026-10-13): re/C3_ENDGAME_PLAN.md classifies the 305 open C2 rows left after round 32 by their newest
measured blocker (log/c3/endgame_classified.tsv) into path A (ordinary batches), path B (two tooling pilots: an mmio
stand-in, an x87-faithful mixer harness) and path C (`dead` / `trivial` / holds with the measured reason).
Thirty-third round = path A (2026-10-13, claude2 model `opus`, log/c3/round33_candidates.py over endgame_classified.tsv,
brief log/c3/round33_brief.md, runner log/c3/alt/r33_main.py): c3ez 11 (the seven Terrain-grid exe rows on c3ey's fixture
Terrain object, netPump / netSendList / netQueueMessage / netServiceControl on a fixture session object at [0x8400b8]),
c3fa 18 and c3fb 13 (exe size-only rows: serializeCourse, deserializeCourse, showTitleScreen, demolishObject,
OptionsMenuBuild, Flic::open, panelDispatch, hireEmployee ...), c3fc 3 (loadSoundTable, loadGolferRoster,
drawEmployeeDetailPanel), c3fd 32 (sound GetProcAddress exports delete_device / init_sound_timer / create_device,
MultiSound ctor / dtor, command rings, MIDI senders, crtValidatePointer, released from re/c3_hold.tsv through the import
stand-in and promoted via c3cm, whose source holds its hook), c3fe 8 (release_sound, soundWindowProc, soundThreadPump, the
channel render rows) and c3ff 6 (jgld Surface::drawTo / drawToZ over framemask.js, saveImage / writePcx8 / loadPng over
filestandin.js with jgld's fopen / fread / fclose added at run time, createDisplay) promoted: 91 functions, C3 2,770 (exe
1,578, jgld 523, sound 521, Terrain 148), open C2 243 (29 newly named callees entered at C2). Names first (re/names/exe_14,
sound_15, jgld_9 with systems writeups): Window::passLocalRect, Window::blitToTarget 0x480dc0 (created with
CreateFunctions.java; its `call 0x475c20` gives DC_calla0 its caller), openHookA/B/C 0x4873f0 / 0x4879b0 / 0x487b90
(created; they call sound.dll's create_device exports through slots soundVersionCheck 0x483fc0 fills with
GetProcAddress), the static CRT names c3fc / c3ff needed, nine sound callees; Sound_lookupOrCreate's sound_3.tsv row
read 0x1002d430 as strcmp, but its whole body is `mov eax, [ecx+0x204]; ret` (row reworded). GetProcAddress exports are
promoted with `c3_verify.py --callers <rva>=golf_clean.exe:<caller>` (pointer_callers.py cannot see an ordinal import).
Main build: all 169 keys GREEN with every hook on and with other batches' hooked callees off; 31 existing C3 callers (56
keys) GREEN with the hooks on, 53/56 off in 15-key boots, the three others (loadCourse_short, gfxFlushB_noobj /
_call1) GREEN alone; static 360 and runtime 19 pass. AUDIO: this PC had an output device during the round (the TV), so
keepsound.js stayed unloaded and the sound timer ran live (~32 ticks/s); c3fe's fixtures make SoundTimer::onTick return at
its first test (0x100b49a8 = 5) only when its lock is free, and c3fe warns that c3ew's fixture saves that word whatever its
value (a mid-tick save would silence the timer for the rest of that boot). Commit note: the per-batch commit script matched
CSVs and notes by RVA only, so jgld drawToZ's (RVA 0xa330, = sound Midi_Device::drainCommandQueue) went into the c3fd
commit; match the module tag in the file name too. Dropped with leads: drawFlyingBall (reads an unwritten frame slot),
showInfoCursorPanel (a frame overrun writes the caller's saved esi), showBuyLandScreen / drawWorldObjects (RNG-driven),
buildSceneSprites (9.1 KB, no blocker: best next exe candidate), narrateShot, showCourseReport, unlinkSound (its walker
0x487500 is never entered at the menu: A/B-able next round), netPoll / netSendShutdown (drafted under #if 0), the 11 sound
rows the live timer thread enters, eight DirectSound list / COM bodies, the uitree / file rows c3fa left. Findings:
loadGolfPro divides by 4 - level; customizeCharacter writes golfer index -1 when updateMembership returns -1; Flic::open
faults on every failed decoder-pool allocation (0x0048231c); loadGolferRoster writes each .pro record's skill bytes into the
next record (0x0046630b); both create_device exports report success after a failed allocation; Midi_Device::shutdown skips
every second track; Channel_startVoice's signed index writes below the channel object; writePcx8 maps width*height*2+0x80
bytes but writes pitch bytes per row plus 769 palette bytes; loadPng only accepts RGB.
Thirty-fourth round = paths A + B (2026-10-13/14, seven claude2 `opus` children, all stopped at the token limit once and
resumed in fresh sessions from their files; brief log/c3/round34_brief.md, runner log/c3/alt/r34_main.py, children
log/c3/round34_children.tsv): c3fg 8 (path B1, the MMIO STAND-IN `re/frida/js/mmiostandin.js`, loaded by diff_hook after
filestandin.js: the twelve WINMM mmio imports of sound.dll served on the A/B thread through importstandin.js, 354/354 records
equal to the real winmm in its conformance harness log/c3/alt/c3fg_conform*; first user parseRiffWave, plus MidiReader_init,
Sound_resetBuffers, beginDecodeStreamA/B, processWaveChunks, loadMultiChannel, Wave_Device::readFmtChunk), c3fh 18 (path B2,
the x87 pilot: the six Voice_mix* leaves and streamRenderBlock / Tail load control word 0x0f7f (64-bit, toward zero) and leave
it set; a C body with MSVC `long double`, which is the 53-bit double, is RED on all six mixers (first divergence at `fmul st(2)`
0x10019cf4), while a body of one-instruction X80 steps on 10-byte operands in the original's order matches every register,
the status and tag words (log/c3/alt/c3fh_ab.py fnsaves both arms); the caller-word helpers Seq_setTempo, DsBuffer::setVolume,
Channel_setRate / recomputeVoiceGains / setReverseRate / stepVolumeFade, Voice_resetDefaults, Sound5b6a8_ctor,
allocResampleTable and png_init_read_transformations A/B'd at 0x027f and 0x0f7f, since their originals' outputs change with
the word), c3fi 15 (exe rows under 2 KB: netPoll, netSendShutdown, Reader::open, ListBox::build, scrollbarCreate,
showMainRadialMenu, showLoadingScreen ...), c3fj 2 (buildSceneSprites, 16 keys / 562 vectors; netHandleControl, its dialog
declared through framemask.js), c3fk 25 (sound sequencer channel setup, track nodes, the eight DirectSoundDevice bodies),
c3fl 13 (sound rows the live timer thread runs: SoundTimer::onTick, Wave_Device::serviceTick, writeWaveFile, DsBuffer::restore
...; this PC had an output device, so the engine ran live) and c3fm 24 (the exe's file / string CRT and jgld's memcpy / strlen
/ strcat / abs / operator_new / _setjmp3 / fopen / fclose, openHookA/B/C, Window::blitToTarget, Surface::dashedLine16)
promoted, plus DC_calla0 0x475c20 (c3dl's key, its caller now recorded): 106 functions, C3 2,876 (exe 1,612, jgld 533, sound 583,
Terrain 148), open C2 170. Names first (re/names/exe_15, sound_16, jgld_10, 33 rows: FID-matched CRT callees,
NodeB000_initBase, ChannelTable_loadRange / releaseObject). Main build: all 251 keys GREEN with every hook on and with other
batches' hooked callees off; 208 existing C3 callers (888 keys) GREEN both ways (c3fm's CRT hooks put abs, __itoa, memcpy and
the file CRT under 162 of them); lost boots (announceBuilding, matchUpdate, expandTextMarkup, Sprite::create8_oom) and two
one-vector matchUpdate state REDs passed in 1-4-key boots; static 360 and runtime 19 pass. Dll_Midi_Device::create_device
needed `--callers 0x9a90=golf_clean.exe:0x004879b0` (openHookB calls export #7 through 0x0083af84). Defender quarantined
c3fi's COVERAGE build in instances\c3fi (Behavior:Win32/DefenseEvasion.A!ml: the DLL CreateFile/WriteFile'd its counters);
the verifier then ran the five missing controls on the normal and negctl builds without a detection. Those controls found a
fixture gap: showMainRadialMenu's wrong body storing the spot s instead of the image byte k (0x00432f39) read GREEN because
every row had k == s (0x004c7994[s] = s for s < 3); two rows with s = 3 and 7 now read it RED. Running c3fi_run.py with
`--batches <negctl list>` loads no c3fi fixture (`not a function` on every key): the negctl DLL goes to the instance, the
registry list stays the default. A background shell's 2-hour limit kills the shell, not the Python runner (it kept writing
and finished); stopping the "orphaned" game killed its live boot instead. Held (re/c3_hold.tsv): jgld __chkesp (its input
is the caller's ZF), Surface::dashedVLine16 (no vector reaches its run reload without a fault), showInfoCursorPanel (it
overwrites the caller's saved esi, which diff_hook does not compare). return0Pop4_487b00 stays C2: endstate.py reads its
5-byte body as size 0 (the extent fix planned for round 35). Leads (each batch's Path C): the 12 remaining MMIO rows over the
stand-in (closeMmioSound, openMmioSound, fillDecodeRing, Midi_Device::loadFile / loadFromMemory ...; the write path needs
measuring first), Voice::update and loadWaveResource with the X80 method, renderChannelStreamed (a dedicated batch), netService
/ netSendMessages (uninitialised frame arrays), drawFlyingBall, create_sound. Findings: streamRenderBlock's chain end reads the
flags of `cmp edx, 0` and always stores the loaded sample as the integer position (0x10018722); Sound5b6a8_ctor's tempo
multiplies a freshly zeroed +0x720; Wave_Device::dispatchCommand kind 4 reads o+0x44 before its NULL test; _xtoa never ends for
radix 1; jgld fclose dereferences a NULL stream after reporting it; Cache482f60::dtor's second pass can never run; showLoadingScreen
frees its frame Surface twice; netHandleControl leaves slot 15 duplicated when it removes a peer.
Thirty-fifth round = the C3 close-out (2026-10-14, eight claude2 `opus` children; pool log/c3/round35_pool.py, candidates
log/c3/round35_candidates.py, brief log/c3/round35_brief.md, runner log/c3/alt/r35_main.py, children
log/c3/round35_children.tsv): c3fn 8 (the MMIO remainder over mmiostandin.js: openMmioSound, closeMmioSound,
streamWaveSamples, loadWaveResource, fillDecodeRing, flushMmioBuffer, Midi_Device::loadFile / loadFromMemory), c3fo 10 (the
three `__ftol`, jgld fabs, __strcmpi / __strnicmp, _fseek / _rewind, both ehVectorConstructorIterator; new tool: an exported x87
probe that fnsaves the whole FPU after each arm, so st(0)-in / st(0)-out leaves and their status flags are compared), c3fp 1
(renderChannelStreamed, frame mapped by hand, X80 fade tail at both control words), c3fq 6 (Wave_Device freeBufferList /
appendBuffer, create_sound, rewindChannel, ChannelTable_loadRange / releaseObject), c3fr 4 (showBuyLandScreen, narrateShot,
GolferPanelEject (= hooks.csv drawGolferInfoPanel 0x0045c560), showCourseReport; 75 wrong bodies, all RED), c3fs 1
(drawFlyingBall, c3ea's naked entry with the unwritten slot [F+0x14] seeded alike) and c3fu 11 (rows round 33 classed
NO_OBSERVABLE / NO_CALLER that write globals, return a field or have a reference) promoted: 41 functions, C3 2,917 (exe 1,630, jgld 535, sound 604, Terrain 148). Main build: all 77 keys GREEN with
every hook on and with other batches' hooked callees off; 143 existing C3 callers (708 keys) GREEN both ways (six
announceBuilding keys and jgld_Random::range lost a boot and passed in 2-key boots); static 360 and runtime 19 pass.
PATH C CLOSE-OUT: pointer_callers.py had skipped the range of every body of 5 bytes or less as a `jmp rel32` thunk, so
endstate.py read real 1-5 byte functions as 0 bytes / 0 rets (return0Pop4, ListBox::Slot60, null_method, jpeg_mem_init); only E9
bodies are skipped now and 8 rows were marked `trivial`. Every other row the batches did not take got a hold in
re/c3_hold.tsv with its measured reason (123 held); `re_classify.py status --summary` now prints `C2 held` apart, so `C2 still
open` means no end at all: 5 after this round, all named in round 35 and never offered (EditBox::onKey 0x004867b0, both
__ArrayUnwind, sound dynInit_soundWindowObj100b49b8 / SoundWindowObj_ctor). Created in the master first (CreateFunctions.java):
EditBox::onKey, dynInit_soundWindowObj100b49b8 0x100091c0, SoundWindowObj_ctor 0x10028600 (re/names/exe_16, sound_17). The two
ehVectorCtor_finally funclets are held (they read their parent's frame). Gate pitfalls hit again: a stale CSV of a removed key
(an ERROR) refused fillDecodeRing until deleted; a shared heading with `**Purpose (x):**` labels read as no-purpose
(Midi_Device::loadFile / loadFromMemory, split into one `## 0x<rva>` section each). Held with measured reasons (path C):
writeWaveChunk / B and writeWaveFileB (a full 10-vector run crashes the game after install, one vector never does, the arms never
mismatch), loadSoundFile, jgld pow (naked x87 CRT with internal helpers), Voice::update (a full sequencer tick: its own batch),
Wave_Device::open / start (real device, thread and timer: ~18 stubs), drawWorldObjects (per-call-site argument slots),
netService / netSendMessages (SEH frame + alloca + in-frame dialog + COM + wsock32), the six x87 renderers in the table
0x1005b830 that only an unreferenced constructor installs, and four functions called only from unreferenced code. Findings:
renderChannelStreamed hangs its thread on an underrun at the read limit (0x10036782) or a segment switch with no second segment
(0x10036792); create_sound maps `.aif` to the default case (0xb); ChannelTable_loadRange's release scan restarts at index 1;
the MIDI loaders never check `MTrk`; showCourseReport's subtotal minutes are the last hole's time (0x00450b60); showBuyLandScreen
prices a lot with no tile for sale at 50; resetSpriteQueue stores -1 into one dword 1,024 times. The C4 pilot c3ft is the
next paragraph. The claude2 session that ran the round could not write the git index (don't-ask mode refuses `git add` /
`git apply --cached`), so its commits were made from a claude3 session.
C4 pilot c3ft (2026-10-14/15, notes log/c3/c3ft_notes.txt): `re/frida/scenario_ab.py` + agent `re/frida/js/scenario_det.js`
run a canonical scenario on instances\c3ft with a hook set OFF and ON and compare, at fixed checkpoints, every .sve global
(re/tools/sve.py FIELDS + both tail blocks), the RNG words 0x822d9c / 0x51b380 and the frame pixels. DETERMINISM: the main
thread's timeGetTime (exe, sound.dll, binkw32 slots) and QPC (jgld, binkw32) point at native stubs that return a virtual clock
advanced 1 ms per call (other threads get real time); a stub that finds T >= due calls JS on the main thread, which runs input
(SetVirtualCursor / SetVirtualKey / PostMessageW, all from the main thread), the exe's SetTimer / timeSetEvent family stood in
as virtual timers, and polls (the main thread blocks while Python screenshots and dumps). The RNG seed (seedFromTime 0x467110 =
timeGetTime*37) needs no write. A record run times input by golden-screen matches and saves the virtual T of each
(schedule.json); every other arm replays it. Pitfall: messages posted by the game's own thread are not DPI-rescaled, so they
take LOGICAL coordinates (game.py's physical lParam landed off-screen at 250 % and the listener's hit test 0x47f700 in
0x495aa0 dropped every click). Two OFF runs are identical in sandbox_basic (10 checkpoints) and course_season (build a hole,
600 virtual s season, 12 checkpoints), with sound live. Reach = one-shot INT3 on each function's ORIGINAL entry (in the ON arm
over MinHook's 0xE9), re-armed once per virtual second, counted per thread: Frida inline counters killed the game (relocated
prologues) and detour breakpoints merged MSVC-folded identical reimplementations; a CModule's own data is read-only here (pass
a JS-allocated table). Verdicts: GREEN (SAME + entered on the main thread), RED (diff, or the ON arm hung / died), UNREACHED,
OFFTHREAD (only other threads), UNOBSERVED (sound.dll: audio is not compared, and three sound.dll functions' main-thread reach
follows the real-time sound thread). With all 2,886 hooks ON both scenarios read SAME: sandbox 440 GREEN, course_season 583
GREEN (562 functions, 559 at C3); their CSVs `<addr>_[<stem>_]<name>_<scenario>.scenario.csv` are in log/diff (nothing
promoted). Controls: Random::range with results 0/1 swapped (shim/src/re/c3ft_negctl.cpp) RED from the menu on;
fillRectC8 writing its last row as c^1 (c3ft_negctl2.cpp) RED on 11 frame px with the state SAME; the first fillRectC8
control (skip the last row) read GREEN as an equivalent mutant (every fill is 255 over a row already 255), so a GREEN does
not prove the scenario could see a wrong body; jpeg_set_quality ON read UNREACHED. GATE FIXES (2026-10-15): re_classify's
C4 gate read the first `<addr>_*.scenario.csv` / `.match.csv` of ANY module (a jgld CSV passed a sound.dll row at the same
RVA; sound Midi_Device::scalarDeletingDtor 0x9c80 would have passed through Terrain initSystem's match CSV) and only the first
scenario (a GREEN could hide a RED); it now reads every CSV of the row's own module through read_path1 (all GREEN, scenario
ones witnessed). The C3 replay check does the same (calls summed); Project.diff_csv was removed; replay.py writes
`<rva>_<stem>_` names (the Terrain tileAt replay was renamed). scripts/scoreboard.py keys evidence by (module, addr) from the
file name and counts a kind only when all its CSVs are GREEN: 19 Terrain.dll rows had been credited with jgld / sound files.
To promote: `re_classify.py promote <addr> --to C4 [--module m]`; the C2 rows propagateType11 / clearBuffers are refused.
C4 BY MATCH (2026-10-15, approved by the user): the gate's other C4 evidence is a 100% VC6 match on top of C3. Only 316 match
CSVs existed for 2,265 MATCH lines (bulk and test runs skip evidence), so log/c3/c4_match_regen.py re-ran re/tools/match.py
over all 262 re/match/*.cpp (all rc 0, about a second each) and log/c3/c4_match_promote.py promoted every C3 row whose own
module's match CSVs are all GREEN through `re_classify.py promote --to C4 --evidence <csv>` (the gate re-checks each and writes
its CHANGELOG line): 1,889 promoted, C4 1,890 (exe 1,076, jgld 510, sound 155, Terrain 149), C3 1,028 left (1,020 without a
match, 6 below 100%). Refused: flagSet 0x0049eef0 / flagIsSet 0x0049ef80, whose hooks.csv `file` names a source without their
SG_HOOK (round 6's duplicate batch): point the column at the linked batch and re-run. The C3 rows left are the scenario route's.
Thirty-sixth round = C4 round 1 (2026-10-15, plan re/C4_PLAN.md, brief log/c3/round36_brief.md, pool log/c3/c4_pool.py,
candidates log/c3/round36_candidates.py, children log/c3/round36_children.tsv; eight claude2 `opus` children, five of them
stopped when the account ran out of tokens and reactivated by the user from the Happy app). Before the round: the 150
scenario-ready rows were promoted (user's decision), and flagSet 0x0049eef0 / flagIsSet 0x0049ef80 were found to have NO
SG_HOOK in any source, now or in git history (round 6 promoted them from path-1 CSVs), so they were demoted to C2;
log/c3/hook_integrity.py now checks that every C3 / C4 row has a linked hook (core sources by build.bat's `golf_*.cpp`
wildcard included). MATCH route: m36a 143 + 1 and m36b 117 (sound.dll), m36c 129 + 2 (sound.dll), m36d 256 + 1 (exe <= 256
bytes) and m36e 16 (jgld + the two exe partial matches) at 100%, with boundaries rows for Ctrl5b5c0_transposeTable,
viewDispatchBuild, both ehVectorConstructorIterator-family extents, Channel_parseWaveHeader_adjPlus70, atexitDtor_100b49b8,
streamRenderTail, png_default_error and Font::deletingDtor_adj8 (each checked in the disassembly). sound.dll is THREE builds:
the C++ code /O2 (/GX with an EH frame), the LPC-10 2400 bps speech codec 0x3a900-0x41a00 debug /Od C (f2c-shaped:
lpc10_encode/decode, analys_, synths_, pitsyn_, dyptrk_ ...; several re/names rows contradict it, leads in
log/c3/m36c_D1/D2_notes.txt), and the static CRT /O1; the brief's sound flags were wrong (m36b caught it). log/sigs_sound.dll.tsv
predated the names, so ghidra2src declared every renamed callee varargs cdecl: replaced with m36b's fresh dump (old kept as
.pre_r36); the bulk path otherwise gave almost nothing, every match was hand work. m36d re-implemented flagSet / flagIsSet
(shim/src/re/m36d.cpp, 12 keys, null + 7 wrong bodies RED), C2 -> C3 (promote with `--file shim/src/re/m36d.cpp`: their
hooks.csv `file` still named c3x.cpp and c3_verify gave no reason) -> C4 by their old matches. SCENARIO route: s36a ran the
six canonical scenarios not yet used (championship, golfer_events, golfer_stories, stories_fixture, stories9_fixture,
mood5_fixture), each with two OFF runs SAME and the negctl RED; s36c added `--spawn-reach` to scenario_ab.py (default proven
byte-identical) so boot code counts, and its boot control A showed a dynInit_* stub that skips its constructor reads GREEN, so
dynInit_* stubs never go to C4 by scenario; s36b built the `ui_tour` scenario (re/tools/scenario.py, ~415 virtual s: every
menu screen, the F1-F10 reports, hire / character editor, preferences, rename, save / load, a practice round), deterministic
with the negctl RED. EVIDENCE DECISIONS (the user): a naked inline-asm transcription is NOT match evidence (matches by
construction; five rows demoted: sound strcpy / __ftol / strstr, exe fillSpanDown / fillSpanUp); library-object identity IS
(m36e: jgld __ftol, strlen, strcpy, strcat, _setjmp3 instruction-identical to libcmtd.lib members); an inline __asm block in a
compiled C body IS (the C is matched and the asm covers instructions VC6 never emits from C; 85 rows already C4 that way, 21
promoted). Verifier mistakes caught and undone: a blanket scenario pass promoted 122 rows on s36c's unverified spawn CSVs
(demoted, then 35 re-promoted after its report); the match pass promoted three naked transcriptions. log/c3/c4_match_promote.py
(`--kind match|scenario`) now skips naked-asm matches and dynInit_* scenario rows; promote scenario evidence by named files
(log/c3/c4_promote_s36b.py / c4_promote_s36c.py), never by a blanket pass while batches run. Result: C4 2,765 (exe 1,536,
jgld 535, sound 545, Terrain 149), C3 153 (exe 94, sound 59), C2 open 5; static 443 + match 344 and runtime 19 pass. Left: the
exe UI families only network code builds (TextView / ListBox / optionDialog / panel, about 120 rows), story / tournament /
retirement screens, sound near misses (VC6 scheduling and register choice the permuter does not move), strchr (needs a signed
branch offset in match.py's normalize, and is a transcription anyway).
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
The game has **no single-instance guard**: a copy only failed because the `DWM8And16BitMitigation` compatibility
layer is registered for `original\golf_clean.exe`'s path; game.py passes it as `__COMPAT_LAYER` (2026-10-07).
Parallel A/B: `re/tools/instances.py create <name>` -> instances\<name>, then `SIMGOLF_ALLOW_MULTI=1
SIMGOLF_GAME_DIR=instances/<name>` for game.py / diff_hook, and `SIMGOLF_BUILD_DIR / SIMGOLF_DEPLOY_DIR /
SIMGOLF_RE_BATCHES` for build.bat. Keep copies at short paths (a Temp-path copy failed). Track the PID you spawn
and kill only that one. Without SIMGOLF_ALLOW_MULTI `Game.start()` refuses while another SimGolf window exists.
Never blanket-kill by name.
Frida temp (2026-10-12): frida-core unpacks a 22-50 MB agent dir (%TEMP%rida-<hash>) per Python process and
removes it only on a clean exit, so runners killed on a timeout leak one each: 6,461 (326 GB) filled C: during round 29.
Game.start() now calls re/tools/game.py purge_frida_temp(): it deletes only dirs created before every running python /
node / frida process started (a live process's dir has no open file between attaches, so the first version, which trusted
a failed rename, deleted its own runner's dir and the next attach failed with `unable to find DLL`). Tools that attach
Frida without Game should call it too.
No audio output (LIVING-PC, 2026-10-13, found by c3ex): with waveOutGetNumDevs() = 0 initSound 0x483e90 gets 3 from the
music device open and freeSoundLib 0x484060 FreeLibrary's sound.dll ~1.3 s after the window, so every sound.dll key
ERRORs with `unable to find module`. re/tools/game.py then loads re/frida/js/keepsound.js, which pins sound.dll
(GetModuleHandleExA PIN) when the game's LoadLibraryA returns it; the game still runs its no-sound path (g_soundReady
0x83afc4 = 0), so sound code runs only when a fixture calls it. `SIMGOLF_KEEP_SOUND=0/1` overrides the detection.
Defender: `instances\` is excluded from scanning (2026-10-12; MARIANO-PC only, not set on LIVING-PC). Negctl builds that VirtualAlloc'd a flag page at the
fixed address 0x7A110000 were quarantined there as Trojan:Win32/Bearfoos.B!ml. All negctl sources now export the switch
instead (`<Batch>NegCtl[4]`, word 1 magic 0x5147454e; fixtures call `__freeze.negOn()` in re/frida/js/freeze.js after the
freeze); never reintroduce a fixed-address page. `original\` is still scanned: never deploy a negctl build there.

### Patching
Patches are `patches/<name>.ini` with `expect` bytes taken from the anchored binary. Default
`enabled=0` unless it is a compat fix. Cite the analysis note in `desc`. The static test validates
every committed patch against the anchors.

## Delegation

Workspace policy (`..\CLAUDE.md`): read-only surveys go to the account2 worker via `delegate.ps1`.
Ghidra MCP, Frida, builds, game runs, tracker writes stay local.

## Happy info pane
Keep `.happy/project-info.json` current when a standing fact changes (status, counts, key dates).

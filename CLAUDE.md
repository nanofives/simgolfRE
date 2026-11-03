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
│   ├── anchors.json, CONFIDENCE.md, functions_ghidra.tsv (2,934 exe fns), functions_ghidra_Terrain.dll.tsv (1,050)
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
match.py also reads // INCLUDE <dir> lines (repo-relative, search order; // INCLUDE $VC places VC6's include
directory, else it comes last) and // COMPILE <path> (cl compiles that file in place of the annotated one: under /ZI
an #included file's __LINE__ is a constant, not the per-function line variable). A file's // TOOLCHAIN sp5 line runs
VC6 SP5's cl 8804 / c1xx 8964 / c2 8966 from tools/vc6sp5 (sound.dll's C++ objects: Rich entry 11/8966 x34 equals the
obj @comp.id, the back end's build); sp5c2 is RTM's front end with SP5's C2 (/B2), indistinguishable by the Rich
header; default 8168. In the exe (no relocations) an original operand whose value the obj holds as a plain literal is
compared as that literal (U-1886: 0x807fff, flag words such as 0x800000 in the image range).
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
py -3.12 -m pytest -m ""                      # FULL suite (pytest.ini addopts skips slow and ghidra by default)
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
PORT PIVOT (2026-10-15, plan re/PORT_PLAN.md): the user's target is a portable reimplementation that plays from the user's own game
files and loads or patches none of the four binaries. The C3/C4 hooks become the specification and the test oracle (re/match sources are
VC6-exact transcriptions with literal addresses: reading reference, not code to copy); C4 stops being the gate. The plan holds the module
table, seven phases (data model first) and a keep-or-fix list for original defects. Surveys: `re/tools/port_survey_globals.py`
(log/port_globals.tsv): the 1,537 game-logic functions (0x401000-0x4a4ecf) touch 5,302 data addresses in 955 clusters (gap <= 0x40), 134
named exactly, 891 clusters (500 functions) unnamed, the largest initialised-data tables (0x4c80d0-0x4cd3cc 848 addresses), the most-shared
mutable cluster 0x83aa78-0x83ac30 (82 addresses, 99 functions); immediates in the data range count, so these are upper bounds.
`re/tools/port_survey_hooks.py` (log/port_hooks.tsv): of 1,577 exe SG_HOOKs by their own detour body, pure 313, globals only 177, original
callees 568, both 507, naked 12, so 80% do not port as written (111 sources use literal addresses, 9,660 occurrences).
Batches after round 36 (2026-10-15/17, notes log/c3/<batch>_notes.txt): c3fv 5 (gameMain 0x45baf0, loadWorldAssets 0x43dbe0, loadInterfaceArt
0x442180 [24 KB, 819 call sites as 48 generated tables, log/c3/alt/c3fv_segments.py], loadInfoScreenArt 0x44bde0, drawWorldObjects 0x430360;
c3fv.js REC, an ordered TID-gated recorder with framemask's frame mapping and 64 stubs per key; gameMain also SAME at 10 checkpoints of the
sandbox_basic spawn-reach scenario), c3fw 4 (loadGameBrowserScreen 0x43b610, showThemePackSelect 0x4725b0, pickAProScreen 0x43a8c0,
courseActionMenu 0x46f550: round 32's SEH blocker ended by running ??_L 0x4a55d2 / ??_M 0x4a5713 for real as framemask stubs with
`forward: true` and the Surface array declared as a {base, stride} region), c3fx 1 (showCharacterEditor 0x4385d0: the hold reason ??_L was
wrong, the body holds one Image under SEH; the obstacle was 41 stood-in callees, solved with REC), c3fz + c3fza 5 (EditBox::onKey,
__ArrayUnwind exe 0x4a5793 / sound 0x1004288e, sound dynInit_soundWindowObj100b49b8 and SoundWindowObj_ctor; __ArrayUnwind is a plain
stdcall with its own SEH frame, run through an exported four-argument harness) promoted: no C2 row is open. Counts (`status --summary`,
2026-10-17): C4 2,765 (exe 1,536, jgld 535, sound 545, Terrain 149), C3 168 (exe 106, sound 62), C2 386 (113 held, 273 end states dead /
trivial), C1 1; exe function list 2,497.
Golfer record (g1, 2026-10-16): g_golfers starts at 0x005794b8, 156 records of 0x100 bytes (0x2700 dwords: cleared at 0x0043160b..0x00431615,
saved at 0x0040b107, whole-record `rep movsd` at 0x0045de3e / 0x004226ec); 0x005794c0 is field +0x08 of record 0 and the stride-0x80 array at
0x57956e does not exist (field +0xb6). Records 152 scratch, 153 preview, 154 synthetic, 155 trialShot's copy. re/analysis/data/golfer_record.md
(field map, flag bits, C struct) and record_tables.md (walkers/employees 0x585850 64 x 0x4c, per-class stats 0x56ae70, per-type state
0x5849e0, story definitions 0x53a454); `re/tools/record_fields.py --base --stride --count` also scans code outside every function range (444 of
3,120 references). golf_course.cpp, c3fa, c3fy and c3fx_deps.tsv use base 0x5794c0 (offsets 8 lower). Open ranges U-0009..U-0017.
mainLoop 0x0040f5c0 (c3fy family, 2026-10-16/17): one body 0x0040f5c0..0x00421615 = 73,813 B, 21,541 instructions, 27 switch tables
0x00421618..0x00421b23, one `ret` 0x0040fb04 (the Ghidra list had 8,035 B in 2 ranges); called once per process, no arguments, returns only
on QUIT. Spec re/analysis/sim/mainLoop_structure.md + log/c3/c3fy_deps.tsv; reimplementation shim/src/re/c3fy.cpp (a naked entry rebuilds the
SEH frame). The C2 -> C3 gate needs a path-1 A/B and none can exist for a once-per-process function, so it stays C2 (re/c3_hold.tsv NO_PATH1)
and c3fy.cpp is NOT in shim/re_batches.txt; the evidence is whole-game scenario A/B (mainLoop ON vs OFF, other hooks off) on state, RNG words
and frames. U-0008 resolved (c3fy2): eight transcription defects, not the original (a register reloaded between a `cmp` and its `jge` /
`setle`, 0x00413a59, 0x004156b7; `lea` between `test` and `jne`, 0x00414a88; 0x00414e91; a missing load before a switch, 0x0041e38f; strcpy
written as strcat, 0x0041cabc, 0x0041ec50). U-0007 open: `[0x004c2848] == 0x280` enters the exit-less loop 0x00420d83..0x00420e08 at
0x0041b209. Block coverage of the original: 54.0% (nine c3fy2 scenarios), 82.3% (2,103 of 2,554) with c3fya's cov_a_* and c3fyb's cov_b_*
(re/tools/scenario.py). c3fyc: five audits (re/tools/c3fy_audit_*.py) and 190 mutation modes (c3fy_mutgen.py, c3fy_mutants.tsv); the rounds
fixed more transcription sites (c3fya 3, c3fyc 7, f19530e7 14, 7e85f800 two P7 sites 0x0041a47a / 0x0041ac89). Kill round c3fyk1-3
(cov_k1_* / k2_* / k3_*) over the 45 surviving mutants: 22 killed (RED twice at the same checkpoint and hash, real hook GREEN twice), 16
equivalent (argued from the code), 1 dead (129), 6 open (113 a playSound id; 187 a tile kind no stamp writes; 272; 279 a closed handle number;
117, 123 a camera clamp read only by a money label no frame shows); verdicts log/c3/c3fyk{1,2,3}_results.tsv. TOOLING LESSONS: (1) a stored det_b
from another instance or build, or a boot overlapping a sibling's, differs at the first checkpoint in boot words (title ticker 0x00822c78, RNG
seed 0x00822d9c, seed_time 0x0051b380 = clock calls x 37): regress against a fresh OFF arm on the same instance; scenario_ab.py makes
winmm.dll.c3ft_main once and restores it after EVERY control run, so a stale backup reinstalls an old build (two c3fyk3 runs voided). (2)
Defender quarantined rebuilt mutant DLLs in instances\ (DefenseEvasion.A!ml) and Frida injection failed: rebuild and re-run, do not alter
code to dodge it. (3) A killed `--negctl` run leaves the control DLL and a stale boot.lock: compare winmm.dll with the backup, restore,
delete the lock. (4) TaskStop kills the shell, not its Python runners. (5) A single DIFF under load (cov_b_career, 384 px, state SAME) did not
reproduce: count a kill only when a second run stops at the same checkpoint with the same hash.
Batch b3 (2026-10-17): tool fixes with tests in tests/test_tool_fixes_b3.py: asm2inline.py keeps `movsx` operands and reads every range of
a multi-range function (loadInterfaceArt reaches its `ret` 0x004480f7, not 0x00447f79); match.normalize() compares branch offsets as signed
32-bit (strchr's `+0xfffffff0` against `+0x-10`; five match files re-run unchanged at 100%); c3_verify.py prints `hook source:` with the
reason when a row's hooks.csv `file` names a source without the SG_HOOK. Names (re/names/exe_17.tsv, systems exe_17.md, pending ApplyNames
list re/analysis/data/names_pending_apply.md; hooks.csv follows through `retag`): pickGolferName -> appendWalkerName, golferScore -> golferAge
(printed after `, age ` at 0x0045c7e0..0x0045c81e), updatePairSnapshot -> appendPairTrackPoint, allocTextBuffer -> drawTextRun,
ListBox::onNavKey* -> onPress* (a click handler), codecCombFilter -> codecRing5Add, new fillSlots54 0x004a29a0, four legacy hook names
(OptionsMenuBuild, CareerRetirementFlow, GolferPanelEject, LandmarkAvailableNotice) retagged to the names rows, and g_screenWidth 0x00822c8c /
g_screenHeight 0x00822c90 (gameMain 0x0045baf6 / 0x0045bb00, set-mode slot +0x58 at 0x0045bb2a..0x0045bb42; they were g_zoomLevel).
Batch b1 (2026-10-17, function boundary repair of the Ghidra master, backup ghidra/backup/SimGolf.rep.pre_b1_20261017, write-up
re/analysis/data/function_boundaries.md, per-region rows log/b1/gaps_<module>.tsv): the function lists missed code in every module. Of the
exe's bytes outside the old ranges, 66,856 were listed functions Ghidra had cut short (mainLoop alone is one body 0x0040f5c0..0x00421615,
73,813 B, with a bogus pointer at 0x41ea32 cleared), 28,837 were 437 separate functions reached only through .rdata vtables or stored
pointers (largest 0x48b3f0, 3,384 B, ListBox), 9,095 jump / byte tables and 3,961 junk `call`s in the padding (716 calls after a `ret`
into the stub 0x404a14 that jumps outside the image; origin not established, U-0018). New `ghidra/scripts/RepairBodies.java` sets
explicit bodies (so a function created early cannot absorb its targets), adds switch references and jump-table overrides and verifies
every body: 43 exe switch sites got references (38 table overrides), functions created exe 437, jgld 62, Terrain 56, sound 463 (+1 called
thunk 0x10001c62) with neutral `FUN_` names, 24 false functions removed (11 each in jgld / Terrain: pieces split off the debug CRT heap
routines; 2 in sound). Function counts: exe 2,497 -> 2,934, jgld 1,570 -> 1,621, Terrain 1,050 -> 1,095, sound 1,753 -> 2,215; no reachable
code is outside a function. The refresh recipe (c2_note --refresh + retag) changed callers / callees on 158 hooks.csv rows (mainLoop's
callees 55 -> 216) and no C3 / C4 row lost a caller; no new hooks.csv rows (rows come from names files). Re-run survey: 1,812 game-logic
functions, 5,589 data addresses, 960 clusters, 893 unnamed (584 functions touch one), golfer references outside every function 444 -> 0.
Thunks referenced only from vtable data (jgld 247, sound 550) were not created: pointer_callers.py resolves them. The b3 names (pickGolferName
-> appendWalkerName ... ) were applied to the master afterwards: ApplyNames overwrite renamed 9 exe rows and codecRing5Add, the exe and sound
function lists were re-exported. Lesson: a command that reverts a directory (`git checkout -- re/analysis`) also reverts siblings' uncommitted edits.
Batch b2 (2026-10-17, reproducible scenario baseline, re/analysis/data/scenario_baseline.md, manifest scenario_refs.json, tool
re/tools/scenario_refs.py): the boot desync of the stored references was the PC's AUDIO DEVICE, not concurrency or hooks. LIVING-PC's only
output device is the TV's HDMI endpoint; with a device sound.dll's timeGetTime at 0x10010dc3 (DsBuffer::play) runs on the main thread during
initSound just before seedFromTime 0x00467110, one extra clock call: seed_time 0x0051b380 = 0x00100001, ticker 0x00822c78 = 0xbb71, RNG seed
0x1d467741 (and location prices, which come from the RNG); without one 0x00100000 / 0xb81f. All 909 stored runs map one-to-one. scenario_det.js
now gives sound.dll and binkw32 the device-less answers by default (`C3FT_AUDIO=none`; `live` reproduces the old timeline) and scenario_ab.py
reports an A/B whose arms ran different audio paths as INCOMPARABLE. Measured: 20/20 OFF and 20/20 ON boots identical in clock-call count
(21,265) and state hash, also with two games overlapping; the c3fy flakes cov_b_inject2, cov_a_build3, cov_a_career and cov_b_career
matched their reference 10/10 OFF and 10/10 ON (cov_b_career's old script sat on the location screen, whose two pixel variants of 384 and
5,830 px occur in OFF and ON alike). 55 scenarios are pinned (det_a == det_b == det_c on winmm.dll 381ed2b2..., scenario_det.js 93e128bb...):
regenerate with `re/tools/scenario_refs.py regen`; the 35 sound-era references under log/c3/alt/c3fy2|a|b|c are stale (use
log/c3/alt/refs). cov_b_career now buys San Diego ($70,000) and cov_a_career Monterey ($50,000): the old clicks were unaffordable on the
device-less timeline. Leads: cov_b_shots / cov_b_shots2 take 5-7 shots and idle (results screen not recognised); a jgld.dll data dump for
the location-screen variants.
Batch b5 (2026-10-17, the two scenario leads b2 left; re/analysis/data/scenario_baseline.md sections 11-12, tests
tests/test_scenario_b5.py): cov_b_shots / cov_b_shots2 stopped after 7 / 5 shots because Gary's post-hole skill panel
(19.9 from tests/golden/shot_panel.png) passed the old turn test (25). The script ran a whole shot sequence, and its own
input closed the TOURNAMENT RESULTS panel that came up meanwhile. Turns themselves measure 0.0 to 18.4 (the ovals change
with lie and club), so `_cov_b_champ_rounds` also tests tests/golden/gary_skill_panel.png (8; every turn is 19.9 or
more from it). It checks for the results before every input group and before every Esc, starts a new championship
round after each results screen, runs the turn's input once per round on the post-hole panel on purpose (that reaches
H1 key cases and P7 tool previews outside a turn), and stops at 24 shots: cov_b_shots 3 rounds, cov_b_shots2 4.
Coverage of the original mainLoop against the b2 references: +86 blocks (H2 51), none lost; the union stays 2,103 /
2,554. The location-screen variants of the pre-b2 cov_b_career (`log/c3/b2_oldcareer.py`) have two causes:
(1) courseActionMenu 0x0046f550 reads its blink counter (entry esp - 0x4cc), dwell counter and previous hot button
uninitialised. The stale blink word differed in every boot, and its bit 3 decided the 384 px variant in 8 of 8 boots.
scenario_det.js STACK SEED (default) puts a stub at the thunk 0x00470a00, the only way in, that zero-fills 16 KB below
esp (`C3FT_STACK_SEED=probe` records the words natively, `0` disables). (2) The window, through the
DWM8And16BitMitigation layer, lagged the game's unchanged back buffer for 90 virtual s while the game presented the
full rect every frame. scenario_ab.py now takes checkpoint frames from jgld's back buffer (`checkpoint_frame`:
RGB555, `(v << 3) | (v >> 2)`, rows top-down although GetObjectA says biHeight +600), which equals the window capture in
every pixel when the window is current, so references stay valid. Window differences are recorded as `window_px_differ`
plus `<name>.window.png`; `C3FT_FRAME=window` restores the old capture. Result: cov_b_career_phx 20/20 OFF and 20/20
mainLoop-ON identical at all 6 checkpoints. New regression scenario `cov_b_location`. Never probe a stack slot with a
Frida Interceptor on the same thread: its callback runs below esp and overwrites the slot (the first measurement read
Frida's own words).
Batch c3fyl (2026-10-17, the six mainLoop mutants c3fyk left open; notes log/c3/c3fyl_notes.txt, tests/test_scenario_oracle_ext.py): 113, 117 and
187 killed, 272 and 279 equivalent, 123 not settled; no transcription defect, c3fy.cpp untouched. Tally of the 45 DATA / COV survivors: 25 killed,
18 equivalent, 1 dead, 1 open (123: the camera y is bounded at 0x34 by every static writer except those that copy a golfer, walker or ball
position, and a golfer's turn at 0x0042b39f is not checked against the map edge). 113 is killed only by the new OPT-IN extended oracle in
scenario_ab.py (`--call-log va:n[:ra/ra][:sK][:r][:xK]` logs main-thread calls with arguments and return value, `--extra-regions va:n` dumps more
memory per checkpoint, `extcmp` compares two finished runs; default behaviour byte-identical, the extended result is reported next to a verdict
that still comes from the normal comparison): playSound received 0x27 instead of 0x26. 187 is reached through the Scenic Bridge button, which
sets the tile variant to Random::range(8) at 0x00434c57 (the popup items only give 0, 2, 4, 6), then painting fairway on the bridge tile (it keeps
the variant bits and adds 0x4000) and a 2x2 building that covers the tile without being anchored on it; 117 needs a course with money labels (a
sandbox draws none, 0x0040c926). New scenarios cov_l_camera, cov_l_point, cov_l_bridge.
Batch b4 (2026-10-17, U-0018 narrowed to `partial`, re/analysis/data/safedisc_padding.md): the 716 `call` slots in the exe's padding are SafeDisc's
own code-cave instrumentation, written before it encrypted `.text`; Safedisc2Cleaner did not create them and nothing in the project reaches them,
so the port ignores them. original\golf.exe's `.text` is encrypted (2,682 of 754,731 bytes equal the cleaned file); from 0x00425990 on each byte is
the plaintext XOR a table of the previous four plaintext bytes (99.90% agreement), and 566 of 619 slots decrypt byte for byte to the cleaned file's
slots. golf_v100_clean.exe has 183 `call [IAT]` slots after a `ret` (caves filled with a copy of the function's last bytes, no `call rel32`, no
dispatcher); the slots are redrawn per build (49 share a function end between v1.00 and v1.03, none the same import). One slot per cave of 5-13 bytes
after a `ret`: 5-9 bytes get `call rel32`, 9-13 `call [IAT]`; the dispatcher is three pieces (0x00405099 -> 0x00404a14 -> `jmp [0x00403f9c]`, a dword
holding 0x008a2590 inside SecServ.dll). The DLLs carry none. Open: the 97 slots below 0x00425990 use a cipher not recovered, and what changed 35
IAT operands after encryption (SafeDisc at load time or the cleaner); a memory image of the wrapped exe after decryption would settle both.
Batch b6 (2026-10-17, sound.dll speech codec 0x1003a900..0x10041a00, re/names/sound_18.tsv, sound_18_globals.tsv, systems sound_18.md, U-0019): of 42
functions 23 renamed from their instructions (codecProcessBlock -> codecDecodeFrame, codecProcessResource -> codecEncodeFrame, codecStageDispatch ->
codecAnalyzeFrame [a fixed 14-stage pipeline], codecGainSmoother -> codecPitchTrackDp, codecFeatureDecision -> codecVoicingDecision, codecCombFilter ->
codecRing5Add, applyIir2 -> codecSynthPostFilter, floatFloorToInt -> floatRoundHalfAway, ...), 2 kept with corrected purposes (bitPermute, codecAmdfPitch),
17 confirmed; g_codecPermTable was one dword before the real slot table (now g_codecFrameBitSlots 0x10064480, 53 entries; g_codecCoeffSignMask 0x10064458,
g_codecErrProtect 0x100b58a8, g_codecFrameLen 0x100b58a4). Applied to the master (ApplyNames overwrite renamed 23, `ghidra/scripts/DeleteLabels.java`
removed the misplaced labels 0x1006447c and 0x10064454, backup ghidra/backup/SimGolf.rep.pre_b6names_20261017). The reference LPC-10 source was not
opened: structural matches rest on the m36c notes. Old spellings stay in registry keys, fixtures and SG_HOOK name strings (rows are looked up by address).
PHASE 1, THE DATA MODEL (2026-10-17/18, plan re/PHASE1_PLAN.md, shared brief re/model/STREAM_BRIEF.md, decisions re/model/DECISIONS.md; streams on
claude3 [A] and claude2 [B-G]): constants stay in the user's own exe (the model records DESCRIPTORS: name, element type, count, stride, file offset;
no table bytes or strings are copied), the header is clean C99 with static_assert sizes and no original addresses, the address map is separate.
Stream A (re/tools/model_scan.py, model_inventory.py, model_check.py; re/model/inventory_*.tsv, ownership.tsv, vtables.tsv; re/analysis/data/
model_inventory.md): 30,102 references to 5,949 data addresses in 1,767 clusters (the old survey's 5,589 skipped base-register displacements and
call/jmp memory operands), classifier error rate 7.5% on known types, element COUNTS weak (10 of 16); model_check verifies a proposed type against
every access of every function. Streams (each re/model/include/sections/<s>.h, re/model/<s>_addresses.tsv, re/names/model_<s>_globals.tsv,
re/analysis/data/model_<s>.md, re/model/<s>_uncertain.tsv): B simulation state (165 ranges, 122 names), C course/terrain/world (50 ranges, 35 names;
Terrain.dll boundary structs), D rendering and audio state (109 ranges, 62 names; 300 sound objects of 0x6c at 0x80d840, 190 image slots at 0x81ca10,
sprite queue arrays), E1 UI toolkit class layouts (209 rows), E2 engine class layouts and the DLL API tables (29 structs, 153 globals, 130 names,
e2_dll_api.tsv), F constants and configuration (366 clusters as 374 descriptor objects, f_strings.tsv indexes 3,114 strings without text). G2 (re/model/
sve_fields.tsv 91 rows, re/model/model_ctypes.py generated by re/tools/model_ctypes_gen.py, re/tools/model_sve_fields.py): 116 saves round-trip
byte-identical through the field table and mirror (3 fixtures, game-written checkpoint saves, autosaves), 63 of 69 unnamed save fields named by the streams
and the last six named by G1 (fields 25 g_courseReportScore, 29 g_buildingUnlockLevel, 46 g_proChallenger, 76-78 g_namePool / Offset / Length),
178 uncertainty rows merged into UNCERTAINTIES.md (U-0110..U-0514). G1 (re/tools/model_merge.py, re/model/addresses.tsv 1,119 rows with 0 overlaps and 0
duplicate symbols, re/model/include/simgolf_model.h compiled with MSVC, tests/test_model_header.py, re/analysis/data/model_integration.md,
model_defects.md, model_name_changes.md with 80 changes): model_check over all 1,812 functions gives 13 explained errors and 770 warnings, the hook
cross-check 0 unexplained; the names were applied to the Ghidra master (backup ghidra/backup/SimGolf.rep.pre_phase1names_20261018, ApplyNames overwrite:
24 renames, 800 labels). Findings that change older notes: hole n is the record at 0x575ab0 + n*0x208 (record 0 a swap slot, 19 the leaving slot);
animals start at 0x56d1b8 and g_typeAttr 0x578372 is field +0x22 of the table at 0x578350; 0x822c74 is a 64K-entry halving table and 0x822c78 a random
course id (not the title ticker); 0x5a4998 is the 51x51 terrain corner-level grid (stream C); gRngState 0x4ba800 is a read-only double constant (withdrawn);
the vtable 0x004bb3f8 class is TextView (Dialog:: names are the older prefix); sound.dll's sprite and sound objects (g_sounds 300 x 0x6c). The game's own
save routine refreshes g_year (0x5a6d3c) through appendDate, so a port must recompute the year from the date after loading (model_defects.md 33).
showBulletinBoard writes two words one past the golfer table (record 156) into the tile_build_code grid. Lessons: a hold-out check that reads every
names file stops being a hold-out once the streams add theirs (excluded model_* files); names_validate needs a re/analysis/systems/<id>.md stub per names
id; claude2 children cannot write the git index, so the parent commits their files by explicit path; ownership overlaps between parallel streams were
settled by early notes, so tell the affected streams at once.
Stream H (2026-10-18, naming the exe game-code FUN_ functions with the finished model; brief re/model/H_BRIEF.md, candidates re/model/h_candidates.tsv in four
chunks, four claude2 Sonnet children; notes re/model/h<k>_notes.md, names re/names/model_h<k>.tsv): 279 of 281 functions named (h1 70 distinct, h2 70, h3 71,
h4 68; 2 deferred: 0x0047e770 View slot 78, one h2 row), 0 bad citations. Groups: atexit destructor stubs (about 70: gSurfaceDtor_*, exitDtor_*, per-global
destructors), this-adjustor thunks and vtordisp pairs, window-class slot defaults (one constant-return default fills 363 slots), NodeList / Snd / VoiceRx
forwarders named by target slot (Class::fwdDevN / callChildN), TextView / EditBox / Button / scrollbar slot bodies, UI event-table entries, static-init
stubs, and ten course / terrain / UI functions called by mainLoop (raiseCornerLevelsAround, switchToCourse, saveCourse, panelListSelect ...). Applied to the Ghidra
master (backup ghidra/backup/SimGolf.rep.pre_streamH_20261018, 279 renames); tracker: 209 rows C0 -> C1 through `re_classify.py batch re/names/model_h*.tsv --to
C1`, 200 of them C2 through `c2_note.py --all` + `batch --to C2` (10 thunks of 5 bytes or less stay C1); exe C1 10, C2 235 after it, total C2 586. Defects
found (not fixed, addresses in the notes): 5x5 corner-level windows with no bounds test, saveCourse's unbounded filename copy and untested _open,
setPendingCourseInfo's unbounded copy, divisions by an untested value at 0x0047e8e1 / 0x0047e8f7 / 0x0048661e, unset eax returns in NodeList::fwdChild9 / 15.
Model corrections reported and listed in re/model/DECISIONS.md (applied at the start of the next phase): vtables 0x004ba2d8 / 0x004ba468 are SpriteRef (0x2c) and
FontRef (0x20) not GfxSurface / SurfaceRef; e1_vtables.tsv's SgRecList (0x004bc3b4) is not supported by its slots; no vtable rows for 0x004ba2a8;
g_ui83aa9c 0x0083aa9c is the root-window pointer, not int32[2]; the 16-record table (0x58 each) at pointer 0x0083ff6c is untyped; Snd slots 41-45 are
unimplemented constant-0xb stubs. Save-field tool: model_sve_fields.py and model_ctypes_gen.py now build from the merged re/model/addresses.tsv (1,116
mirror symbols; all 91 save fields named and resolvable).
Phase 2.1 premise check (2026-10-18, re/analysis/data/premise_check.md, tool re/frida/restart_ab.py, runs log/p2/p21/, notes
log/p2/p21_notes.txt, instance instances\p21 with the pinned references' winmm.dll 381ed2b2): can the ORIGINAL restart from a state
image at a checkpoint and reproduce the next ones? Restart instant = mainLoop's frame head (the timeGetTime call at 0x0040fc29,
return address 0x0040fc2f); the opt-in IMAGE block of scenario_det.js (cfg.image; default unchanged: a plain sandbox_basic replay is
SAME as the pinned det_b) captures .data 0x004c1000+0x381174, the stack, T, the clock value the game receives, Q, pending events,
virtual timers and input there, and writes them into a fresh process that replayed the schedule to an earlier checkpoint; the
scenario script is then advanced offline over the source's polls. Scenarios sandbox_basic, course_season and p21_mood5 (mood5.sve
with 15 checkpoints), three continuous runs each: (a) two continuous runs SAME everywhere; (c) a full .data image minus the DWORDS
that differ in two other continuous runs restarts SAME (all of .data, frame, T) in all three, raw it crashes on frame 1 (Surface_blit3
0x00475d73) and minus byte-level noise it crashed too (a heap pointer of another process shares its low bytes: half-written
pointers); the non-pointer model objects alone give every simulation object equal (g_sounds' plain fields and 2 unmodeled bytes
differ). 12 dwords in 5 objects the model types as data hold heap pointers (g_rootWindow +0x11c..+0x518, g_listPalette,
g_msgBoxLabelA/B, g_graphsyObject: premise_check_flag_gaps.tsv, U-0604). (b) a save (the game's own saveGame at the instant,
loadGame in the fresh process) diverges in all three (11 / 48 / 33 simulation objects at k+1); the causal state it lacks is
g_rng 0x00822d9c and g_lastTipDate 0x0055e5ac (save + those two: no saved object differs in 48-480 virtual s; 13 stale buffers,
UI, g_sounds and story text still do); loadGame also rebuilds g_cornerCache / g_tileWallMask / g_tileScreenY differently, edits
g_flags and redraws Terrain.dll's state. Hidden-state table (123 objects, classes H0-H8): premise_check_hidden_state.tsv.
Terrain.dll's state is outside every image: frames differ when the prefix had another course (U-0607). mainLoop's frame need not be
written at the frame head. Negative controls RED: g_date +1, g_rng +1 (two scenarios); g_cash_div100 +1 is rewritten within 60 s
(U-0605). Pitfall: a value-in-mapped-range test is not a pointer flag (string pools, grids, RNG words pass it); it is applied only
inside pointer-flagged objects. Tests: tests/test_restart_ab.py (static), tests/test_restart_ab_runtime.py (slow, 67 s).
Phase 2.2 GameState (2026-10-18, re/analysis/data/gamestate_gen.md, notes log/p2/p22_notes.txt, runs log/p2/p22/, instance
instances\p22 with the pinned references' winmm.dll 381ed2b2): re/model/addresses.tsv has an 18th column `pointer` (model_merge.py:
`type` by restart_ab's rule = phase 2.1's 195 objects / 2,863,036 B; `words:+0x..` for U-0604's 12 heap dwords in g_rootWindow,
g_listPalette, g_msgBoxLabelA / B, g_graphsyObject, evidence in POINTER_WORDS; the header and model_ctypes.py are unchanged).
`re/tools/model_state_gen.py` generates port/include/gamestate.h (struct portsim::GameState = golf_clean.exe's .data, 0x381174 B,
#pragma pack(1), 989 objects at their offsets with model types + 425 gap members, offsetof / size static_asserts, no address
literal), gamestate_meta.inc (SG_OBJECT symbol, offset, size, class, pointer, hidden code, saved; SG_POINTER_WORD; SG_GAP) and
gamestate_addresses.inc (tooling only). Classes: sim 203 objects / 518,308 B, hidden 18 / 14,594, ui 122 / 13,093, render 67 /
120,424, audio 13 / 33,028, pointer 186 / 2,821,872, engine 51 / 11,065, const 329 / 90,789, gaps 51,311 B; never compared:
pointer, audio, the 12 pointer words, gaps (2,906,259 B). `re/tools/state_image.py` turns a scenario_ab checkpoint dump, a full
.data dump (.full.bin / .fh.bin / .pre.bin) or a .sve (model_ctypes over sve_fields.tsv) into a GameState image with a known mask,
hashes objects (sgh64: dword sum with odd position multipliers + splitmix64; pointer words as 0) and diffs them (`diff`: symbol,
class, first differing byte). Opt-in tracer in scenario_ab.py: C3FT_OBJHASH=1|image (`<name>.objhash.json` per checkpoint),
C3FT_FRAMEHASH=sim (a native CModule, re/frida/js/framehash_cm.c, called from the tgt stub at mainLoop's frame head 0x0040fc2f, no
JS per frame; framehash.bin), C3FT_PERTURB=va:xor@ms (agent action x32); `re/tools/frame_trace.py cmp` finds the first divergent
frame and its objects; `re/frida/state_trace.py run|cmp|check` drives it. Proof: plain and traced sandbox_basic replays SAME as det_b
10/10; two OFF runs equal in every frame record (sandbox_basic 439, course_season 6,296) and in every non-pointer, non-audio object
at every checkpoint (10, 12); g_rng ^= 1 at T 20,099 located at record 154 (T 20,105, g_rng alone), g_date ^= 1 at T 300,071 at
record 2,985 (T 300,119, g_date alone). port/ skeleton: CMakeLists.txt (C++17, library portsim, ctest gamestate_layout; x64 and
Win32 both build and pass), README.md (rules), .gitignore (corpus/, build/). Tests: test_gamestate_gen.py (MSVC x86 / x64 compile
with every offset asserted against addresses.tsv), test_state_image.py, test_frame_trace.py. U-0610..U-0613.
Phase 2.3 time and input model (2026-10-18, re/analysis/sim/time_model.md, schema re/model/trace_schema.md = port/schemas/trace.md,
notes log/p2/p23_notes.txt, instance instances\p23 with winmm.dll 381ed2b2): `re/tools/time_input_audit.py` lists every clock /
timer / wait / input import call site of the four modules (re/analysis/sim/time_input_sites.tsv, 253). golf_clean.exe reads only
timeGetTime; the one read whose VALUE enters simulation state is seedFromTime 0x00467110 (g_seedTime = t, g_rng = t * 37, once from
gameMain). Game time is FRAME-counted: P5 does g_date += 1 per unpaused frame (0x0041762f; g_flags bit 2 = pause, key 0x50; bits 3
and 17 have no setter), 1,024 frames = 1 game month, 8,192 = 1 year. The H1 wait (0x0041e631..0x0041e6b1) holds a frame to
frame-head time + 87 ms (+125 with bit 17) in waitTicks(1, 0) steps of 10 ms; g_frameMs 0x005a8c88 is write-only; under the virtual
clock a frame is 99 calls = 99 virtual ms (99.9% of frames; 5 course_season frames skip the wait, U-0651). pumpAndRedraw(0) (16
callers, incl. updateGolfers / pathStep / prepareShot) blocks until a click or key (U-0650). Input: messages -> jgld windowProc ->
root handlers (WM_LBUTTONDOWN -> rootWin_onPoint, WM_LBUTTONUP -> onPress1 = the click + onUnpoint, WM_KEYDOWN -> onChar, WM_CHAR
-> onKey, WM_KEYUP -> onChar0); in a frame they are dispatched in H1 (never in the wait), keys one frame after posting; H1 takes
the key / code with 0x0045ae50 / 0x0045ae30 (misnamed setDirtyFlagB / A: take-and-clear readers); capsLockOff reads
GetKeyboardState every frame. TRACES: `re/tools/trace_tool.py record|replay|validate|summary|dump|diff` (reader
re/tools/trace_format.py) appends re/frida/js/trace_rec.js to scenario_det.js through a Run subclass installed after
scenario_refs.setup (no shared file edited): native wrappers over scenario_det's clock stubs log every main-thread clock call
(site module-relative, value), FRAME records at 0x0040fc2f with the input globals, harness deliveries, window messages, root
handlers, key-state / cursor reads, timer registrations (timers named by registration ordinal: their ids are heap pointers); REPLAY
feeds the recorded values and deliveries back with the virtual clock unused on the main thread. sandbox_basic / course_season /
mood5_fixture: two records byte-identical (580,656 / 7,867,550 / 371,014 B; 439 / 6,296 / 245 frames; 66,744 / 936,098 / 57,910
clock calls), checkpoints SAME as det_b, and the replay from the trace alone writes a byte-identical trace with 0 site mismatches,
every delivery applied and 10 / 12 / 4 checkpoints SAME. Traces go to port/corpus/traces/ (gitignored). Tests:
tests/test_trace_format.py (static, synthetic traces), tests/test_trace_runtime.py (slow, 40 s). U-0650..U-0654.
Phase 2.0 scope manifest (2026-10-18, re/analysis/data/port_scope.md, rules re/port/scope_rules.md, notes log/p2/p20_notes.txt): every one of the
2,934 exe functions has a port module in `re/port/scope.tsv` (columns addr, name, size, hooks_level, subsystem, port_module, mainloop_phase,
reach_reason = rule id, touches_sim_objects, touches_heap_objects, evidence, plus the object lists and a straddler flag), generated by
`re/tools/port_scope.py` from model_inventory's reach map and tags, model_scan's references and the GameState object classes (inputs cached
in log/p2/p20/inputs.json, deterministic, tests/test_port_scope.py). Counts: crt-dropped 1,263, ui 709, backend-gfx 249, boot-only 195,
sim-core 147 (mainLoop kept as one row), engine-glue 128, backend-audio 108, formats-loaders 93, sim-leaf-util 22, net 17, unknown 3.
`re/port/simcore_order.tsv` orders the 169 sim-core + leaf functions leaves-first (11 levels) with globals, hook source, evidence, defects and a
suggested phase 3 child (3.1 90, 3.5 21, 3.2 15, 3.3 13, 3.4 13, 3.6 8, 3.0 6, 3.7 1, 2 unassigned). 126 straddlers (sim state and ui / heap
objects, 62 of them in the ui module: the click / handler functions that carry command effects) are in `re/port/scope_straddlers.tsv`: phase
3.7 must take them from there. Rules that mattered: constant tables f_ptrtab_ / f_strpool_ are not heap objects, a ui-family tag inherited from
neighbours does not make a function UI when it touches no heap object (R6-SIM-RETAG), show* / draw* names go to ui even with a sim tag. U-0660..U-0662.
Phase 2.4 segment corpus (2026-10-18, re/analysis/data/segment_corpus.md, tool re/tools/segment_corpus.py, notes
log/p2/p24_notes.txt, runs log/p2/p24/<scenario>/, instance instances\p24 with winmm.dll 381ed2b2): a SEGMENT = a frame-head
.data image of a continuous run (start.data.bin = portsim::GameState, noise.bin = dword bitmap of the words that differ
between three continuous runs plus the 12 flag-gap dwords), the trace slice from that frame head to an end checkpoint
(trace.trace, a stand-alone valid schema v1 trace rebased to 0), the expected per-frame hashes of the 203 sim objects
(frames.bin, record 0 = the start frame = the image's own hashes) and the checkpoint object hashes (checkpoints.json);
corpus in port/corpus/segments/<scenario>/<k>/ + port/corpus/segments.json (local), committed table
re/port/segments_manifest.tsv. Per scenario: three capture runs c_a / c_b / c_c (restart_ab's IMAGE block, one capture
armed at a time, C3FT_FRAMEHASH=sim, C3FT_OBJHASH=1; all 177 SAME as the pinned det_b), the trace (trace_tool's recorder;
its FRAME values and indices equal the frame-hash records in every scenario), a plan (prefix = the first checkpoint taken
inside the frame loop, capture within 250 clock calls, with an empty message queue; segments of >= 20 frame heads, at most
6 per scenario, 10 for course_season), then per segment two restarts from the image (both must reproduce every frame
record's T and 203 hashes and the end checkpoint's sim hashes) and, when the slice holds input, a COUNTERFACTUAL restart
with the input dropped: COMMAND if it diverges (its first divergent object is recorded), UI-ONLY if not. All 59 pinned
scenarios: 193 segments, 190 ACCEPTED in 53 scenarios (35 TIME-FORWARD 1,534 frames, 155 COMMAND 56,260 frames, 0
UI-ONLY), 3 rejected (golfer_events 4: trace_rec.js writes its injected `call` action as DELIVER kind 0, which
trace_format rejects, U-0681; cov_a_quit 0-1: the game quits, so the recorder never sends its last chunk), no segment in
cov_a_quit_title / cov_b_location (no in-frame capture) and cov_b_locations / cov_k3_career / cov_k1_steps (< 20 frames
after the prefix); negative control g_rng + 1 RED at record 0. Findings that extend phase 2.1's rule: (1) a prefix on the
title menu never reaches a frame head (the restart stops polling the prefix script); (2) the `data` variant crashed
restarts of build-heavy segments (heap pointers equal in all three continuous runs, invalid in the restarted process,
whose prefix is shorter): restarts use `data+` (default since the first 239 restarts), sim objects are written the same;
(3) INPUT IN FLIGHT: a message posted before the start frame head and dispatched after it is in the Windows queue, not in
any image (cov_l_camera step0's VK_UP KEYDOWN: both restarts diverged at record 0); `queued_posts` (FIFO over DELIVER
post / MSG records) finds them, restarts post them again from inside the restoring clock call, start.json carries
`queued_messages` (2 segments). The first counterfactual effect of the season loops' Esc presses is g_cachedCameraTileX
0x00543d04 (U-0682). Run hygiene: Frida spawns hung (game suspended at spawn, or no child at all) about six times while
other sessions also spawned (U-0683): the tool has its own spawn lock, a spawn watchdog (150 s, kills only its own child
by parent pid: a suspended process reports no ExecutablePath) and step watchdogs; it skips the shared boot lock, which was
held for 600 s twice by an unknown owner (U-0684). Port-loading rule, frame alignment and pass criteria: segment_corpus.md
section 5. Tests: tests/test_segment_corpus.py (static, 13), tests/test_segment_corpus_runtime.py (slow, 95 s).
Phase 2.5 vector corpus (2026-10-18/19, re/analysis/data/vector_corpus.md, schema re/model/vector_schema.md, notes
log/p2/p25_notes.txt, instances p25 / p25b / p25c): the recorded inputs and outputs of the ORIGINAL for 308 selected exe
functions (re/tools/vector_select.py -> re/port/vector_selection.tsv: C3/C4 sim / golfer / economy / course / terrain / ai rows,
the util and crt rows they reach through hooks.csv callees, the other pure / globals-only util hooks, simcore_order.tsv
rows), 642 registry keys. `re/tools/vector_capture.py capture` boots with SIMGOLF_HOOKS_OFF=all, loads diff_hook.py's files
with re/frida/js/vector_capture.js in place of diff_hook.js (shared files untouched; it wraps __keepAlloc, the stand-in
entry points and Interceptor at run time), FREEZES the first thread for every key (the menu changed up to 1,753 data dwords
between two vectors otherwise), records the post-fixture data image, the fixture blocks, per vector the drift and the exact
dword / byte changes, the functions the fixture stood in (first 8 bytes of every function start against the file), and
fetches in the same boot the live pages a light emulation misses. `build` replays every vector in Unicorn over the FILE image
(re/tools/vector_emu.py: full flat GDT, fs -> fake TEB, IAT slots -> trap addresses, memory hooks bound to the image and
blocks) and keeps a vector `ok` only when the emulated original gives the live return value and its writes equal the live
changes; inputs (seed / in), outputs (out, out_ptrs) and relocations come from the emulation, relocations proven by a second
run with every block moved (`reloc_check`). Output port/corpus/vectors/<function>.vec (local, ~10 MB; raw captures 0.57 GB)
and the committed re/port/vectors_manifest.tsv / vectors_coverage.tsv. `capture --unstub` re-captures the 122 keys whose
fixtures stand in selected functions with those stand-ins reverted (+unstub keys; 3 functions reproduce only that way).
Result: 243 functions with vectors, 15,191 reproduced vectors; `re/tools/vector_validate.py emu` (from the .vec alone, zero
data, blocks moved) 15,188 pass and `live` (the shim's reimplementation in a hooks-on boot) 15,189 pass, every failure a
vector flagged reloc_check false. Excluded: 65 functions (callee stand-ins the emulation cannot reproduce, imports such as
the CRT's EnterCriticalSection, calls into sound.dll / KERNEL32 code, __ftol's st(0) input). Coverage verdicts (per jcc side
from the emulation's edges): full 113, one_sided 35, partial_branch 24, thin 71, none 65. Pitfalls: running diff_hook.py
directly with hooks off overwrote an evidence CSV (restored from git); Frida frees an unreferenced CModule's code; a Proxy
on Interceptor breaks its invariants; the live replay must freeze too (the menu writes g_textBuffer). Tests:
tests/test_vector_corpus.py (static, 30), tests/test_vector_capture_runtime.py (runtime, 22 s). U-0700..U-0704.
Phase 2.6 harness (2026-10-19, port/test/README.md, notes log/p2/p26_notes.txt): the port's test harness, judging C++ port
code against the vector and segment corpora with no game, no Frida and no original binary run. C ABI port/include/portsim_c.h
(DLL target portsim_c, x64 for the 64-bit Python; the Win32 build compiles too): sg_state_new / free / size / ptr, a 32-bit
VIRTUAL address space per context (GameState at a caller-set base, sg_map for harness blocks; port code resolves 32-bit
pointers through ctx.mem and throws portsim::Fault -> SG_E_FAULT, never casts), sg_call(ctx, name, uint32 slots) by hooks.csv
name returning eax / edx / st(0), sg_func_count / name / info (abi, ret, arity, fpcw), sg_set_fpcw (x64 accepts only
0x027f-equivalent SSE2 semantics), negative-control switches sg_negctl_set by function name (no fixed-address page), the
gamestate_meta.inc object table and sgh64 in C++ (sg_hash_class; equals re/tools/state_image.py on every class), and
sg_run_frames with its final signature returning SG_E_NOT_IMPLEMENTED until 3.7 / 3.8. Registry: port/src/sim/<module>.cpp
defines `extern const portsim::ModuleDef kModule_<module>` (FuncDef: name, abi, ret, arity, fpcw, adapter `void(Ctx&, const
uint32_t* args, Ret&)`, negctl int*), listed by one `SG_MODULE(<module>)` line in port/src/sim/modules.inc; CMake globs
src/sim/*.cpp. First client port/src/sim/util.cpp (Random::next, Random::range, clamp, distance, written from the notes and
the corpus): 4 functions, 279 / 279 vectors GREEN (15 + 32 + 160 + 72, coverage full for all four). Python: lib.py (ctypes,
builds the DLL when stale), vectors.py VectorRunner (zero GameState mapped at the original data VA, seed then in, blocks at
fresh page offsets, relocs, fpcw, call, ret / ret_loc / pointer returns as locations, every out region with out_ptrs, and
no changed byte outside the out regions; skips non-ok, reloc_check false and raw-location vectors with reasons; warns under
10 vectors, zero spread or non-full coverage), segments.py SegmentRunner (load check: frames.json symbols = the library's
203 sim objects, no noise dword in a sim object, C++ sim hashes = frames.bin record 0; then sg_run_frames, LOADED_ONLY until
it exists) with first_divergent_record / first_byte_divergence, report.py (`~/bin/py -3.12 -m port.test.report` ->
log/p2/p26_report_functions.tsv / _segments.tsv). Tests: `~/bin/py -3.12 -m pytest port/test` (markers static and port,
the new `port` marker in pytest.ini): 219 pass in about 16 s, all 190 accepted segments LOADED_ONLY. Negative controls RED:
Random::range wrong bodies (results 0/1 swapped 8 of 15 vectors fail, scaled by n + 1 4, rounded to nearest 6), a flipped out
byte and an incremented ret of a vector, a dropped out region (extra write), a flipped byte in any sim object of a segment
image (load check fails and first_byte_divergence names object and byte). Finding: listAddEntry 0x004a28a0 has one ok vector
whose argument is a `raw` location, which the schema says does not occur (U-0720). U-0720..U-0724.
Phase 3.0 core (2026-10-19, port/CORE.md, notes log/p3/p30_notes.txt): the conventions stage B builds on. port/include/portsim/
arith.h (wrapping add32 / sub32 / mul32 / neg32, x86 shifts with count mod 32, div32 / mod32 throwing DivideError = SG_E_DIVIDE
where idiv raises #DE, abs32, sx8 / zx8, ftol_low32, NoReturn = SG_E_NO_RETURN for a loop the original can never leave), ptr.h
(`Ptr<T>` over the 32-bit virtual address space: wrapping element arithmetic, `field(&Struct::member)`, faults as SG_E_FAULT;
`state_arr / state_obj / state_grid / state_va` over GameState members; the VC6 idioms rep_movs, vstrlen, vstrcat, vstrcpy,
vstrpbrk), DefectDef compatibility switches in ModuleDef (default 1 = the original; sg_defect_*; first switch
sinScaled.readPastTable = model_defects#10), execonst.h (`ExeConstants`: the user's own exe, verified by the .data size and an
sgh64 per descriptor, constants by model name; `execonst_desc.inc` generated by re/tools/execonst_gen.py from
re/model/f_descriptors.tsv holds offsets, sizes, types and hashes only; fill_state writes the initial `const` objects or the whole
initialised image; tests use $SG_EXE_PATH or original/golf_clean.exe) and gamefiles.h (`GameFiles`: bytes by logical name =
path relative to the install root, MemoryGameFiles / DirGameFiles / NullGameFiles, sg_files_*). Modules util (extended), core,
tiles, text: 31 functions ported (27 new: the six 3.0 rows appendClubName, overlayCharAt, staticInit_gRngZero, swapTableEntry,
approxDistance, direction8, plus sign, directionOf, angleFixed, sinScaled, foldRange, absDiff, cosScaled, bucketValue, decaySum,
decaySum2, tileDistance, tileType, tileByte, tileFlag20, tileBlocked, typeAtPos, wallHeight, objectAt, inRange, sanitizeFileName,
initSinTable), 3,108 / 3,108 vectors GREEN (2,829 new); staticInit_gRngZero and initSinTable have no vectors and get state
tests (initSinTable with the CRT sin equals the g_sinTable of all 190 segment start images). Every function has a negative
control (test_core.py NEGCTL_RED, all RED); four wrong bodies the vectors cannot see are recorded as NEGCTL_BLIND (U-0742).
objectAt and sanitizeFileName have one-sided branch coverage: segments must cover them. Finding: the SafeDisc golf.exe also
encrypts .data (404 of 155,648 bytes equal), so ExeConstants needs the unwrapped exe (U-0744). U-0740..U-0745.
Phase 3.R 32-bit runner (2026-10-19, port/test/README.md "x87 and the Win32 backend", port/CORE.md section 7, notes log/p3/p3r_notes.txt):
U-0721 (x64 harness, SSE2 doubles, no 32-bit Python) closed by an OUT-OF-PROCESS Win32 runner: `port/test/runner32/runner32.cpp` (CMake target
portsim_runner32, -A Win32 only, built by `port/test/build.py --arch Win32` or on demand) loads the Win32 portsim_c.dll and serves the unchanged C ABI
over stdin / stdout (`u32 length, u32 op, payload`; reply `u32 length, i32 status, payload`); `port/test/lib32.py` (`Lib32`, `State32` subclassing
lib.State) has lib.py's interface, so VectorRunner / SegmentRunner / report.py run unchanged. `SG_BACKEND=win32` (or `pytest --backend win32`) makes
`lib.load()` return it; default stays x64; `lib32` / `lib64` fixtures give both in one test; no Win32 toolchain = `RunnerUnavailable`, a skip with the
reason. The GameState is a named shared mapping copied into the DLL state around each handle op (no state bytes on the pipe); address-space edits are
queued and sent with the next op, writable blocks travel with each call, exe sections are cached once per runner. `sg_set_fpcw` really executes fldcw on
Win32 (any word); the runner applies it per call, reports the control / tag word after it (`State32.last_fpu`) and restores 0x027f + fninit. Test-only module
`x87probe` (`x87Probe::mulDiv`, `controlWord`, inline x87 asm under _M_IX86): bit-exact against an exact-rational x87 model for 0x027f 0x037f 0x0f7f 0x077f
0x0b7f 0x007f; the same vectors are GREEN on Win32 and skipped as unsupported on x64; the wrong-rounding body (negctl 1, plain C doubles) reads RED at
every non-default word and is an equivalent mutant at 0x027f. Numbers: all 31 ported functions give identical run / passed / distinct on both backends
(the dashboard TSVs are byte-identical: 29 GREEN, 190 segments LOADED_ONLY, 63 s Win32 against 67 s x64); a bare call costs 0.43 ms against 0.002 ms
(two 3.6 MB copies), a vector run 18.0 against 17.5 ms (harness setup dominates). Suites: port/test x64 387 passed, SG_BACKEND=win32 322 passed, static
600. Caveat U-0761: MSVC compiles Win32 C++ doubles as SSE2 as well, so only `__asm` x87 code (or /arch:IA32 sources) honours fldcw: 3.4 writes x87
functions as asm under _M_IX86 plus a C body for x64. U-0760..U-0763.
Phase 3.2 economy and membership (2026-10-19, port/src/sim/economy.cpp, port/include/portsim/sim/economy.h, tests port/test/test_economy.py, notes
log/p3/p32_notes.txt): of the 15 rows of simcore_order.tsv, 9 ported (computeRatingTrend, appendDate, appendNumber, appendRatingLabel,
appendToBuffer45b8b0, clearCost, rateLot, updateRollingStats, updateMembership) and all pass their vectors (160 of 160); canAfford, membershipReport, addScore,
matchUpdate and updateGolfers are straddlers (3.7) and hireEmployee has no vectors and calls modal UI (U-0840), so they are open. Coverage: appendNumber and
appendRatingLabel full, six one_sided, updateRollingStats thin (3 vectors); 14 negative controls RED, one blind spot (updateRollingStats grace) covered by a state
test, plus a state test of updateMembership's decline arm. Port notes: strings are named by pool offset or character, and the month names come through
the pointer table f_ptrtab_4c2908 (U-0841); typeBit7Clear is a private copy of 3.3's row (U-0842). U-0840..U-0844.
Phase 3.1b course (2026-10-19, port/src/sim/course.cpp, port/include/portsim/sim/course.h, tests port/test/test_course.py, notes log/p3/p31b_notes.txt):
of the 30 rows, 7 were already ported in 3.0 and the other 23 are registered in module `course`: records (freeRecordAtPos, initRecords with resetRecordBank /
placeRecord as internal helpers, clearMatching, staticInit_820f04, setPendingCourseInfo, clearBuffers), names and titles (appendString, landmarkName,
buildGolferName, buildStatusText, appendHoleName, appendCourseTitle, sceneryLabel, buildScenarioIntro), geometry (nearestHole, holeQuadrants), seedCourseTable,
files over GameFiles (fileExists, lookupThemeText with a CRT text-mode fgets model, loadThemeFile without the portrait decode / tile blits), listEraseRange,
loadThemes and switchToCourse. 1,033 / 1,033 vectors pass (15 functions; full 9, thin 4, one_sided 1, partial_branch 1); the 8 without vectors have state tests
(staticInit_820f04 also against the 190 segment start images). Other modules are called through the registry by hooks.csv name (MissingResource while absent):
sceneryLabel -> 3.1a's decorationName, loadThemes -> loadGolferRoster (3.3) / listMatchingFiles (unassigned), switchToCourse -> courseActionMenu (ui) + 3.1a's
course builders (switch_to_course_chosen is exported for 3.7); buildScenarioIntro uses a local copy of startMessage until 3.7 registers it. 22 + 2 negative
controls RED, 4 blind on the vectors (clearMatching b, listEraseRange x2, startMessage priority) of which 3 are read by state tests. MSBuild does not recompile
registry.cpp when only modules.inc changes: touch it. U-0820..U-0825.
Phase 3.3 golfers, walkers, animals (2026-10-19, port/src/sim/golfers.cpp + golfers_glue / _world / _walkers / _pro.inc,
port/include/portsim/sim/golfers.h, tests port/test/test_golfers.py, notes log/p3/p33_notes.txt): 19 of the 21 rows of 3.3 and
3.6 ported, 367 / 367 vectors GREEN in 16 functions (appendPairTrackPoint, addGolferPair, resetGolfer, golferAge, typeBit7Clear,
appendEndearment, appendWalkerName, spawnObject, spawnWalker, addHabitatSlot, tickAnimals, placeHabitat, updateWalkers 92/92,
initGolferState, addGolferToTable, loadGolfPro 2/2), plus placeAnimal (judged through tickAnimals' vectors and state tests),
resetGolferState and composeGolferSprite (no vectors: state tests; composeGolferSprite keeps only its look-index write-back into
the type record, the layers are drawing). Not ported: loadGolferRoster (needs a GameFiles directory listing for
listMatchingFiles, which has no owner; U-0864) and skillPointDialog (a modal input loop whose effect is a 3.7 command; U-0865).
Every function has a negative control (RED on vectors, on a caller's vectors, or on a state test; blind modes listed in
test_golfers.py with the state test that catches them). The VIEW / SIM BRIDGE is module `bridge` (port/src/sim/bridge.cpp,
portsim/sim/bridge.h, tests port/test/test_bridge.py; owner 3.3 by the parent's decision): worldToScreen, playSoundAt (simulation
part: playSound is the audio backend's), queueMessage and pointsPopup, registered under their hooks.csv names for 3.4 / 3.5. They
have no corpus vectors, so test_bridge.py replays their C3 path-1 runs: it rebuilds each fixture in a zero GameState, calls the
port and compares the return and diff_hook's FNV-1a hash of the compared regions (the hex of each region joined by |):
worldToScreen 103 of 112 rows exact (the 9 others sit on TTA_LEVEL_MIN tiles whose height reads terrain grids the fixture left
live, U-0867), playSoundAt 27 / 27 through the test-only entry bridge.playSoundAtScripted (the C3 run stood in worldToScreen and
playSound; its call log, including volume / pan / pitch, is rebuilt and hashed), queueMessage 16 / 16, pointsPopup 72 / 72.
playSoundAt draws g_rng only when worldToScreen puts the point on screen, so the simulation RNG depends on the camera and screen
globals (U-0860). Peers are called by hooks.csv name through the registry. Defects reproduced as faults / NoReturn:
model_defects 40..45 (placeHabitat's cluster loop never advances, spawnObject's cell search, loadGolfPro's two divisions and
pro draw, placeAnimal's idiv, resetGolferState on one record, addGolferToTable's discarded draws). A private build (a
scratchpad copy of port/ with only the compiling modules, loaded through SG_PORTSIM_DLL) kept work going while siblings'
files were mid-edit. U-0860..U-0867.
Phase 3.4 shots (2026-10-19, port/src/sim/shots.cpp, port/include/portsim/sim/shots.h, tests port/test/test_shots.py, notes
log/p3/p34_notes.txt): the 7 rows of 3.4 not already in util (decaySum, decaySum2, sign, sinScaled, foldRange, absDiff came from
3.0) are ported: shotPower 64/64, wobble 19/19, trialShot 228/228, chooseShotTarget 12/12, prepareShot 307/307, scanShotArea
289/289, narrateShot 2/2 (921 vectors), identical on the x64 and Win32 backends, with 3.1a's real terrain callees. The cycle's own x87 code is `fild; fsqrt; __ftol`, run as inline x87 on Win32 (sqrt_ftol). Its
vectors stay GREEN at 0x037f / 0x0f7f / 0x0b7f, and a body that rounds instead of chopping reads RED on both backends. Other
children's callees are called through the registry by hooks.csv name (MissingResource until registered); world_to_screen
comes from bridge.h (module bridge, 3.3). trialShot, chooseShotTarget, prepareShot and narrateShot are listed as 3.7 straddlers but are 3.4's. The first
three drop only a debug overlay and message pumps (U-0881). narrateShot keeps its state effects and drops drawing, sound, the
modal pumpAndRedraw and saveBmp; startMessage comes from the registry once 3.7 registers it, a local copy until then (U-0883). Uninitialised stack slots (prepareShot's
dirs[], chooseShotTarget's strict flag) start at 0, as in the corpus's zeroed-stack emulation (U-0880). Switches: model_defects
#18 shotPower.cacheIgnoresPutt (OFF changes exactly the 32 putt vectors), #37 chooseShotTarget.strictFlagSticky (no vector
reaches it), #38 prepareShot.hazardCellTransposed (OFF changes only real-shot vectors), #39 narrateShot.maskNotRestored (state
test). The static test reads any 6-digit hex literal in the exe's range as an address, so write flag bits such as 0x400000
as shifts. chooseShotTarget's
scoring weights and pruning are blind to its 12 vectors (each ends with one surviving cell); the segments must judge them.
MSBuild does not rebuild registry.cpp when src/sim/modules.inc changes, so touch registry.cpp after editing it.
U-0880..U-0883.
Phase 3.5 events, news and golfer stories (2026-10-19, port/src/sim/events.cpp + events_{calls,story,think,news,pair}.inc,
port/include/portsim/sim/events.h, tests port/test/test_events.py, notes log/p3/p35_notes.txt, U-0900..U-0906, model_defects 46-49): 20 of the 21
rows of re/port/simcore_order.tsv with child 3.5 (angleFixed is 3.0's, in util): appendUpgradeText, replaceInText, thoughtBalance, thoughtFlag,
appendOpinion, postEvent, runBulletinBoardWithSound, loadStoryStrings (the story file through GameFiles: "Themes/<theme or Standard>/<story file
of the slot>", read in the CRT's text mode with fgets(249)), computeStoryReply, storyText, substituteStoryNames, advanceStory,
emitGolferThought, buildGolferComment, announceLandmark, patronEvent, holeMagazineEvent, announceHoleType, announceBuilding,
pairGolfersWithStory. One module, one .cpp plus `.inc` parts (test_static wants one .cpp per module). The long bodies are mechanical
conversions of the hook sources (`log/p3/p35_hook2port.py`: comments dropped, data addresses to `GS(member) + off`, typed references
`G<T>(ctx, va)` in events_hook.h) edited by hand; a flag mask between 0x400000 and 0x8fffff reads as an address to the converter and to
test_static (advanceStory's bit 23 is written `1u << 23`). Callees of other stages go through the registry by hooks.csv names
(ev::call_peer): typeBit7Clear, buildGolferName, buildStatusText, decorationName, holeQuadrants, appendEndearment, appendHoleName,
appendNumber, landmarkName, golferAge; the phase 3.7 straddlers startMessage, appendCents and logTick have local copies of their hook
bodies when not registered (U-0903), playSoundAt is the registered bridge function (3.3; counts unchanged with it), playSound is dropped when not registered
(U-0904). Evidence: vectors 16 functions (x64 and Win32 identical; buildGolferComment 1,002/1,002 with the user's story files
attached as GameFiles: port/test/vectors.py got one line for it); the new LIVE ORACLE `port/test/liveraw.py` judges functions the Unicorn
emulation could not run against the RAW captures (live return value, live byte changes, port started from the captured data image,
blocks at their live addresses; `+unstub` keys have the selected callees real, startMessage / appendCents / playSound stay stood in, so
emitGolferThought and pairGolfersWithStory ignore the objects those would have written): loadStoryStrings 47/47, computeStoryReply
124/124, storyText 19/19, advanceStory 68/69 (one vector reads live heap the capture did not keep), announceBuilding 22/22,
holeMagazineEvent 5/5, announceLandmark 44/44, patronEvent 126/126, emitGolferThought 136/152, pairGolfersWithStory 284/301 (the failures follow
the scripted startMessage return, U-0906). Negative controls: 29 modes read RED (13 vectors, 10 live, 6 unit tests), 4 blind on purpose.
Six children building into the shared port/build/x64 lock the DLL against each other: this module was built in `port/build/p35`
(`cmake -S port -B port/build/p35 -A x64`, `SG_PORTSIM_DLL`). runBulletinBoardWithSound has no oracle (C2): only its writes around the modal
call are tested (U-0905).
Phase 3.1a terrain (2026-10-19, port/src/sim/terrain*.cpp, port/include/portsim/sim/terrain.h / terrain_api.h / terrain_gen.h,
port/test/test_terrain.py / test_terrain_gen.py, notes log/p3/p31a_notes.txt): child 3.1a's 60 rows; 8 were 3.0's (tiles / util),
mainLoop is 3.8's, 51 are ported plus applyTileVariation / applyTileWalls (unowned backend-gfx rows rebuildTileData calls), in modules
terrain (grids, heightfield, coverage, shade, noise, scanTileLine), terrain_text (decorationName, buildToolLabel), terrain_path
(pathStep), terrain_setup (buildGolferBioText, customizeCharacter), terrain_course (stampCourseTiles, clearObjectFootprint,
stepCourseEditQueue, demolishObject, buildTournamentPrep), terrain_dll (the Terrain.dll callers), terrain_gen (generateRandomCourse) and
the TEST-ONLY terrain_fake. Vectors 6,646 / 6,646 GREEN over the list (5,595 for the new functions). TERRAIN.DLL (U-0607, U-0800): no
sim-class write of the exe's Terrain.dll callers depends on a value Terrain.dll returns (their writes are engine / pointer class plus
updateTerrainRender's camera memory and g_tileWallMask[0] bit 1, from its arguments), so Terrain.dll is `portsim::TerrainApi` (20
imported methods + the engine services the same functions reach: input pump, Window::paint, blits, renderTerrainStrip, the display DC,
GetVersionExA's major version, operator new), attached per context with set_terrain_api (MissingResource without one, so 3.1b's
switchToCourse needs one too); the Terrain object stays in the 32-bit address space and its fields are read like the exe does. EMULATOR
ORACLE (opt-in `SG_EMU_ORACLE=1`, development only): test_terrain.py runs the ORIGINAL function in Unicorn (vector_emu's file image) over
random GameState images and compares the whole image and eax with the port; for the Terrain callers a recording fake (terrain_fake.cpp,
mirrored by PyFakeTerrain) serves the original at the Terrain import traps and at the engine functions' entries, and the call logs, the
Terrain object and the heap block are compared too. Every function equals the original there (generateRandomCourse 96 / 97 equal, 1 both
spin), including all no-vector / thin ones, and the 4 vector-blind negative controls read RED. Default runs check GOLDEN, SHA-256 digests
of the emulated original's results for fixed scenarios of 18 functions, from the port; every no-vector control must change them.
Builds: siblings' half-written module files broke the shared glob build, so 3.1a built a mirror of port/ (port/build/m31a, finished
modules only). Findings: rebuildHeightfield returns the new g_flags (stepCourseEditQueue returns it); propagateType11 / markTileKind11 /
applyTileWalls leave eax values their callers return (demolishObject); scanTileLine zeroes type 1's walk byte while it runs; endless
loops of the original become NoReturn (rebuildHeightfield's relax / raise, buildGolferBioText's picks, generateRandomCourse's searches,
model_defects#50..52). U-0800..U-0806.
Phase 3.7a tick (2026-10-19, port/src/sim/tick.cpp + tick_*.inc, port/include/portsim/sim/tick.h, port/src/sim/tick_hook.h, tests
port/test/test_tick.py, notes log/p3/p37a_notes.txt, U-0920..U-0929): mainLoop's frame step and sg_run_frames. All 35 TIME-FORWARD
segments PASS (every frame record's T and 203 sim hashes plus the end checkpoint). The frame body is the C3 transcription
shim/src/re/c3fy.cpp (FramePreamble .. H2) converted mechanically by log/p3/p37a_conv.py (.data literals -> GS(<object>) + offset,
calls by hooks.csv name, idiv -> DivideError, frame slots in a frame block of the address space, citations as ML+0x<off> /
<object>+0x<off> without the 0x prefix of an address) and edited only by the asserted replacements of log/p3/p37a_patch_frame.py
(clock reads from the trace, GetAsyncKeyState from the run's key state, the five switch byte tables of mainLoop's .text as case
lists, the H1 wait dropped); other hook sources go through `p37a_conv.py --hook` + log/p3/p37a_patch_scene.py (updateGolfers
465/465 vectors, matchUpdate 35/35, membershipReport 23/23, buildSceneSprites, drawFlyingBall, drawBuildingPanel,
showTournamentResults). A callee is a registered port function, one of 48 documented drops (tick.cpp kDrops; their call closures
write and address no sim object, log/p3/p37a_simwrites.py, log/p3/p37a_drop_census.tsv), or a tick fallback used only while no
module registers the name (showBulletinBoard prefix, showMainRadialMenu, drawBuildingPanel, showTournamentResults, tickMessage:
3.7b's list); anything else is MissingResource{name}, readable through the new sg_run_frames_detail. Drawing passes keep their sim
effects (cell addresses in unk_005a9cc8, the itoa buffer g_textBuf, sky actors, walker screen points, golfer look indices, tile
flags). Every TIME-FORWARD end checkpoint is taken inside the last frame's H1 wait (log/p3/p37a_polls.py), so sg_run_frames runs
that frame up to the wait. Harness: SegmentRunner maps the GameState at 0x4c1000 (unk_005a9cc8 holds a state address) and attaches
original/ as GameFiles (advanceStory's story text; without it cov_a_flags/05 diverged at record 64, found with SG_TICK_DUMP +
log/p3/p37a_emucheck.py, the original under Unicorn on the port's pre-call state). Stage B's local copies of startMessage /
appendCents / logTick (course, shots, events) now call the registered functions. Input is not applied yet (U-0925).
Phase 3.7b commands (2026-10-19, port/src/sim/commands.cpp + commands_panels / _layout / _screens.inc, commands_internal.h,
port/include/portsim/sim/commands.h, tests port/test/test_commands.py, oracle port/test/cmdemu.py, trace mapping
port/test/trace_commands.py, split table re/port/commands_split.tsv, notes log/p3/p37b_notes.txt, U-0940..U-0946): the 62 UI-module
straddlers split into command effect and drawing. INTERFACE: `Command` with the RAW kind RootHandler (one root-window handler call as
a trace HANDLER record gives it, plus the GetAsyncKeyState results and the clock value the handler reads; apply_command does exactly
the handler's writes, all 11 equal the emulated original on 400 states) and SEMANTIC kinds 0x100..0x106 (a click handler's action
with its hit test decided). tickFrame applies the frame's raw commands at H1 and attaches the rest as an `InputSource`; every pump
inside a straddler asks it for ONE dispatch batch with a PumpKind (Blocking: pumpAndRedraw(0) and the screens' click-or-key loops,
NeedInput when none is left; Drain: pumpLight / interruptible waitTicks, nothing unless the source says it arrived) and for the
cursor where a screen chooses by it (U-0940, U-0943). EVIDENCE: no corpus vector exists for any of the 62, so the oracle is the
ORIGINAL in Unicorn (opt-in SG_EMU_ORACLE=1): dropped callees stubbed with their own `ret N` convention, frameTick483cd0 runs the
original root handlers on the next scripted batch through a fresh trampoline per dispatch (Unicorn caches translations), a
pumpMessages called from pumpLight / waitTicks ([esp + 0x20] on entry) is a Drain, View_toLocal returns the scripted cursor, other
children's callees run for real (buildGolferComment recorded on both sides instead, U-0945); eax, consumed input and the whole data
image must equal the port's (only drawing scratch masked). The default suite checks golden digests pinned from the port after the
oracle was GREEN on them (port/test/golden_commands.json) and every negative control against them. CLASSIFICATION by a write audit
(log/p3/p37b/audit.py: each address a body writes and who reads it): 22 ported (14 command: the 7 panel click handlers, selectDifficultyScreen,
showHireEmployeeDialog, showFinancialReport, showPlayerCommentsReport, showBestScoresScreen, showHistographScreen, showLoadingScreen's
two RNG draws; 6 layout: tickMessage, drawAmenitiesPanel, showInfoCursorPanel, drawEmployeeView / DetailPanel, drawGolferActionPanel;
drawWindow / drawThoughtBubble as a helper), 10 pure drawing, 4 owned by 3.3 / 3.7a, 26 gaps with reasons (font-metric hit tests of
showPopup / textInputBox and their callers U-0944, file browsers, the remaining report and map screens). Findings: drawing writes
simulation state (composeGolferSprite's type-record write-back, appendDate's g_year, appendNumber / appendCents' hashed g_textBuf
U-0946, drawEmployeeDetailPanel's g_zoomScale = 4, walker animation word and its hireEmployee call on the panel's first draw);
hitTestSlot432f90's boxes depend on the palette sprites' heights. SEGMENTS: the 155 COMMAND segments hold 8,352 handler records in
5,420 dispatch batches (5,130 in the frame's H1 pump, 257 in screen loops, 29 in pumpAndRedraw, 4 in showHoleStatsScreen); no batch
lies inside a waitTicks loop (U-0941); running them is 3.7a's sg_run_frames (U-0925). Build: a private mirror at HEAD plus own files
(log/p3/p37b/mirror.py, port/build/m37b; each build copied to port/build/m37b_run so a running test never locks it): a sibling's
in-flight edit of events_calls.inc (startMessage's copy removed) otherwise broke the commands tests. test_static refuses image-range
literals in comments too: cite functions by name in port code.
3.7b round 2 (same day, notes section 7): U-0944 decided from the code: Text_put's widths are GDI's (jgld Font::textWidth
0x10068e60 = GetTextExtentPoint32A on a CreateFontIndirectA font over the game's font file), so the port takes them from a
per-context `TextMetrics` provider (commands.h; test provider commands.setTextWidth, oracle stub cmdemu ("width", base,
mask)); without one, MissingResource{"Text_put"} before any write. Ported over it and judged by the oracle: showPopup,
textInputBox, showPreferencesMenu, showSystemMenu (with nearestMenuSpot, which replaces tick's fallback), plus
panelListSelect (transcribed from its instructions: the H1 letter keys f/r/g/s/t/w/v call it) and drawScoreSummary; 20 gaps
left. log/p3/p37b/seg_reach.py maps every call site a COMMAND trace records to its function (which gaps the segments
reach). Segment runs reaching the widgets need a host provider (U-0947). Equivalent-mutant trap: drawEmployeeViewPanel's old
control (limit 0x2f3) changed nothing because no wide column falls in 0x2b8..0x2f3; check a control's arithmetic, not only
its digest.
Phase 3.7a input (2026-10-19, second session; port/src/sim/tick.cpp "trace slices", notes log/p3/p37a_notes.txt section 9,
U-0930..U-0935): sg_run_frames now feeds the trace's input. tick.cpp's TraceInput (the context's InputSource during a run)
splits the slice into DISPATCHES (port/test/trace_commands.py's batches) and gives each to the pump call that made it in the
original: the clock site before it gives the class (frame head or jgld's Palette::update QPC = a pump of the frame body, else
a modal loop) and the frame's cursor reads before it say which pump. Most of the corpus's dispatches come in P6's
pumpInput483bd0 (between the status bar's and P6's cursor reads), the rest in H1's pumpMessages; both are tick bodies now
(`pump_at` sets the site), pumpLight is 3.7b's. GetAsyncKeyState reads outside the root handlers come from the frame's
KEYSTATE records per key; the harness's memory writes (DELIVER w32) apply before the next dispatch, at H1's frame wait
(TkBase::wait_point) or at the frame's end. An end checkpoint inside a modal wait is compared where the last frame's
Blocking pump runs out of recorded input. Result: 109 of 155 COMMAND segments PASS (35 / 35 TIME-FORWARD still); 44 stop on
names outside tick (Text_put 17: no TextMetrics provider, U-0944; drawGolferInfoPanel 8, showCourseReport 5,
shotStyleButtonCode 4, loadGameBrowserScreen 3, showHoleStatsScreen 2, showEndOfYearScreen 2, the modal showBulletinBoard 2,
courseActionMenu 1) and 2 diverged (ui_tour/02, ui_tour_v/02: 3.1a's customizeCharacter skipped the unregistered
showCharacterIntro; its call site now throws MissingResource, so they stop there, U-0931). Fixes the segments forced: a headless Terrain stand-in for the heap objects the exe side reads
(HeadlessTerrain: Terrain object, list sentinel, operator-new arena, the corner step applyTileElevation's loop waits for,
U-0932); narrateShot's modal pumpAndRedraw(0) (shots.cpp) now waits for the next command when an InputSource is attached
(U-0933); new tick functions rankValue, playHabitatSound, drawRecords; drops drawTerrainToolbar, enterModal483d30,
Snd::status, Widget_readPixel. port/test/segments.py: NON_CAUSAL = {g_textBuf} (all 81 references are __itoa destinations,
log/p3/p37a_noncausal.py), reported in SegmentResult.noncausal, and run_detail. Runner log/p3/p37a_cmdsegs.py (TSV per
segment), summary p37a_cmdsum.py; development switches SG_TICK_INPUT_LOG, SG_TICK_CALL_LIMIT, SG_TICK_TRACE. The full
COMMAND gate is test_tick.py's slow test (about five minutes); 7 segments run by default.
Phase 3.8 integrator (2026-10-20, plan re/PHASE38_PLAN.md, shared brief log/p38/BRIEF.md, merge log log/p38/MERGES.md, per-stream
notes log/p38/38<x>_notes.md, U-0960..U-1053): eleven claude2 children (38a..38k), each in its own build dir with SG_PORTSIM_DLL,
shared files changed only through patches the parent applied. Result on the merged tree: all 200 accepted segments PASS (165
COMMAND, 35 TIME-FORWARD), x64 = Win32 in every row; 0 entries left in test_tick.py OPEN_MISSING except Text_put without a
provider, OPEN_DIVERGENT empty. New modules: gditext (38a: the host GDI TextMetrics provider, the game's 15 startup fonts rebuilt
with Font::create's LOGFONT over the font files read through GameFiles, equal to 11,459 / 11,459 recorded printable widths;
recorder re/tools/widths_rec.py + re/frida/js/widths_rec.js, corpus port/corpus/widths/; SegmentRunner attaches it by default,
SG_TEXT_METRICS=none disables it), shotstyle / loaders / saveload (38b: GameFiles list / write with an in-memory overlay that never
touches the install, listMatchingFiles, loadGolferRoster, the load browser, courseActionMenu, saveCourse, saveGameDialog,
promptAndSaveFile), reports (38c: showCourseReport, showEndOfYearScreen), dialogs (38d: skillPointDialog, showCharacterIntro,
drawGolferInfoPanel, drawScrollText layout, runScrollText, scrollTextWindow, showBulletinBoard), holestats (38e:
InputSource::clock = the trace's next clock read at a call site, showHoleStatsScreen over it), integrator (38f, test-only batch
overrides), routing (38g: showRoutingMapScreen, showShortcutsScreen, showMembershipRoster), chareditor (38h), semantic (38i),
menu + tick_menu.inc (38j: mainLoop's title menu and session dispatch converted from c3fy.cpp parts MV / SV, so sg_run_frames runs
through FrameExit::Menu; loadWorldAssets with the sprite-sheet cell allocation drawing passes store into a sim word,
loadInterfaceArt(0), startNewCourse, pickAProScreen; refreshTerrain is no longer a drop). Gates (38f, port/test/integrator.py
segments | chain | speed | defects | ledger | compare): (c) course_season as one 6,127-frame run and the new pinned scenario
course_season_6y (5,000 virtual s, 10 segments, 50,923 frames = 6.22 game years, re/frida/scenario_ab.py, trace_format.py meta
q0_64 for QPC past 2^32) as one run, all checkpoints PASS; chains 335 / 335 PASS with no BREAK after 38j; (d) championship/00..02
driven ONLY by semantic commands (38i: input-action kinds lowered to the root-handler calls the raw input makes at its dispatch
point, port/test/semantic.py translate | xlate | gate; all 165 COMMAND segments PASS semantic-only, 10,420 dispatches, 0 gaps);
(e) re/port/divergences.tsv (`integrator ledger`), the one row without a cause is chooseShotTarget.strictFlagSticky, which no
segment or test reaches (U-1012); (f) 8,192 frames take 23 s x64 / 29 s Win32 against the original's 713 s of frame waits, but
a year with no input does not exist in the original (input-gated waits, year-end screen, U-1010). Fixes the merge forced: tick's
HeadlessTerrain arena 1 MB -> 16 MB (cov_b_holes18/00), golden digests re-pinned only through SG_EMU_ORACLE=update after the
oracle was GREEN (clickGolferPanel, showSystemMenu, loadGolferRoster: their callees became registered), test seams that recorded
a peer the port now has (showSystemMenu's promptAndSaveFile, loadGolferRoster's scrollTextWindow via cm::call_peer). Findings:
c3eo.cpp's C4 hook picks the hole-stats rating word with v / 10 where the original uses v / 20 (U-1000; the port follows the
original, the hook needs fixing); mainLoop_structure.md had the title menu's championship / sandbox selections and flags 0x4000000
/ 0x1000000 swapped (corrected, U-1053). HYGIENE: never run `pytest port/test` as one process (it grew to 10-16 GB and exhausted
the 64 GB commit charge, killing siblings' runs with MemoryError): log/p38/suite_by_file.sh runs one file per process; lib.load()
rebuilds port/build/x64 from every .cpp in port/src/sim, so a tree holding a child's unmerged module tests that module too.
Phase 4 .sve round trip (2026-10-21, re/analysis/data/sve_roundtrip.md, port/test/test_sve_roundtrip.py, U-1060 / U-1061):
scenarios cov_r_reload (load mood5, browser-load stories5, play, save key = save_1, browser-load save_1 back, play, save_2; an
Esc before every browser / save key; the record run requires g_date to go back on the reload) and cov_r_sve (no Esc: a SimFoto
wait took the second 'L', so no reload; kept as that wait's regression), family s36a (prefix `cov_r` in scenario_refs.py). Each
run copies the saved-games folder to `<run>/saves/end/` before its end checkpoint; `~/bin/py -3.12 -m port.test.sve_refs
collect <scenario>` checks that every run agrees (9 per scenario did) and copies the files to port/corpus/saves/. The port runs
the one segment of each (979 / 1,047 frames PASS) and reads its files back with the new C ABI call `sg_files_read` (lib.py /
lib32.py `files_read`; `SegmentRunner.run(seg, after=fn)` gives the context after the run): all 10 files the original wrote
have equal body bytes and equal header text up to the NUL, x64 and Win32; the header's 0x64 bytes past the NUL are leftover
g_textBuffer text from drawing the port does not do (reported, not failed, U-1060). Controls RED: saveGame mode 4 (one body
byte; cov_r_sve's segment still PASSES, cov_r_reload's reload reads it back and FAILS), modes 1 / 3, loadGame mode 9,
pumpAndRedraw mode 3. PORT DEFECT FIXED: TraceInput handed a modal dispatch to the first pass of the cursor-less SimFoto wait
(narrateShot's pumpAndRedraw(0)); the original took it on pass 107, after the first pass's flushRedraw had cleared the pending
redraw textInputBox's Enter left, so the port cleared the key too and starved. `InputSource::pace(site)` (commands.h): the
wait reads its clock sites through the source and a Blocking dispatch waits until at most one read at the pass site before it
is unread. The 200 earlier segments keep their digests. g_redrawPending / gDirtyFlagA / gDirtyFlagB are class `render`, which
the segment gate does not compare. Diagnosing a modal-wait slip: `C3FT_CALL_LOG=<va>:<n>:r,...` on a `scenario_refs.py run`
logs the original's main-thread calls with returns (calls.json); SG_TICK_INPUT_LOG takes a FILE NAME, not 1.
Course files (2026-10-22, same write-up): cov_r_course saves stories9's course with the 'C' key (Themes\Championship\<title>.cse
via saveCourse, then flushCourseInfo writes the `[Course]` .srf), quits to the title and plays a championship on that file.
The port now WRITES the .srf (writeCourseInfo in tick_render.inc: the exe's formats from f_strpool_4c7818, text mode CR LF);
.cse body + both header texts, .srf and &QuitSave.sve equal; the Thumb / Full .jpg screenshots are phase 5 (reported). Each
cov_r_* scenario collects the folders at run_a too, and the test seeds the port with the original's files at the segment
start: a load before the segment can write an autosave the install's copy lacks. Controls RED incl. saveCourse mode 1 (the
tournament loads the port's broken file and faults). 15 tests x64 + Win32; gate 203 / 203, earlier digests unchanged.
Phase 4 boot path (2026-10-22, re/PHASE4_BOOT_PLAN.md "Result", port/test/boot.py, port/test/test_boot.py, U-1062..U-1066): the
port runs a whole scenario from process start over the user's exe image alone. C ABI `sg_run_boot` (Win32 runner op 27): the
trace records before the first FRAME are a frame 0 (parse_trace boot flag) that runs port/src/sim/tick_boot.inc (BootCrt,
BootGameMain, BootSetup, then the title menu and session); loadInterfaceArt(1) is the generated table menu_iart.inc
(`~/bin/py -3.12 re/tools/gen_iart.py [--check]` from c3fv.cpp). Gate `~/bin/py -3.12 -m port.test.boot --all --jobs 3 --out
log/p4/boot_gate.tsv` compares every record with log/p2/p24/<scenario>/c_a/framehash.bin: 57 / 60 PASS, cov_a_golfer and
cov_b_holes18 KNOWN (boot.py KNOWN table: the cursor tile only, U-1062), cov_b_location NO_FRAMES; x64 = Win32; segment
digests unchanged. Lessons: a boot run needs the scenario's own install files (scenario.py "files", read by
segment_files.files_for: cov_b_title's Default.pro); instances\p24 still holds a leftover Default.pro (= Gary Golf.pro) from
a cov_b_title run, so never take an instance's directory as the install a capture saw. The captures' frame-hash tracer runs on
the main thread's stack at the frame head, so a function that reads uninitialised stack (screenToTile) can behave differently
with the tracer on: find such cases with a `C3FT_CALL_LOG` run with and without `C3FT_FRAMEHASH=sim`. The call log's second
column is virtual ms since t0, not the trace position (cov_b_title: the trace's first FRAME t 1113321 = 0x100000 + 64745). jgld's window procedure
reads the cursor once per WM_MOUSEMOVE; parse_trace drops those reads (U-1064). Debug recipe for a boot divergence:
record-k sim hashes give the original's value of a one-dword object by brute force over sgh64 (re/tools/state_image.py), and
SG_TICK_DUMP=<callee>;<dir> brackets each call with the port's state.
Phase 4 quit (2026-10-22, re/analysis/data/sve_roundtrip.md "The quit", U-1050 part 2 resolved, U-1067): scenario cov_r_quit (pinned on instances\p4q) ends with the title menu's exit; trace_rec.js ends a trace at ExitProcess (END sub 1, the exiting thread waits for `trec_ack`), an exiting s36a-family lockstep scenario loads c3fya_run's exit support. The port's boot run goes past the last record with boot.run(to_end=True) and writes top10.sve, Default.pro and both saves as 12 runs of the original did (character files compared on their records: sve_refs.compared_bytes). New: __open / __write / __close (Ctx::lowio), showCreditsScreen, drops playAudioFile / PaletteCache::dtor, FontRef +8 = -1. A real key press reached one recording (VK 174, no DELIVER before it) and ended it early: compare a new recording's checkpoint count with the reference's.
Phase 5 step 1, the gfx backend (2026-10-22, re/PHASE5_GFX_PLAN.md, port/test/gfx.py, test_gfx.py, U-1068..U-1070): module `gfx`
(port/src/sim/gfx.cpp) is a software RGB555 renderer owned by the port, attached per context (gfx.attach) like the text provider. With
one attached, TkBase::call hands a drop it has a body for to gfx_drop (the frame keeps the drop's result; nothing is written into the
GameState, so sim hashes and segment digests are unchanged by construction); without one nothing changes. Bodies: showTitleScreen (the
boot now calls it at gameMain+0x41f as a drop), loadImage (8-bit RLE PCX via GameFiles, palette `>> 3` = Palette::apply's RGB555),
Surface_blit (cropSprite with its 0xff trims), Surface_blit3 (1:1), Image_draw (draw16: bytes >= 0xfe transparent), setDrawTarget,
drawCenteredText (plain text: x + (0 - width) / 2, baseline y + the font's +0x10, TextOutA through TextMetrics::draw_glyphs, which
GdiTextMetrics implements on a 16-bit DIB: the original draws its labels with GDI too) and Window::refreshFull (root -> back buffer).
Anything else is recorded as unsupported (gfx.log). GATE: a checkpoint frame is jgld's back buffer at the checkpoint's clock call
(summary.json status.tgt = the index of the trace's tgt call, its ordinal P); gfx.snapshotAt(P) copies the port's back buffer when
the run first consumes a CURSOR record (FrameInput::cursor_pos) or a dispatch past P, into the files overlay `$gfx/snapshot_<k>.555`;
compare with re/tools/imgcmp.py exact_diff per region. sandbox_basic `menu`: 0 of 480,000 px differ (x64 and Win32), controls
wrong blit / wrong palette word / no present RED, `> 0xfe` blind (no 0xfe byte in the title images); segments 205 / 205 and boot
58 / 2 / 1 unchanged.
Phase 5 step 2, the location screen (2026-10-22, re/PHASE5_GFX_PLAN.md section 6, U-1071 / U-1072): all five cov_b_location
checkpoints (refused, site_hover, buttons, tip, back) equal the original's back buffer in every pixel, x64 and Win32. New
bodies: loadInterfaceArt(1)'s image tail (`loadInterfaceArt.images`, called after menu.cpp's table), courseActionMenu's own
Image / Surface array under `gfx_local_key` (a range no GameState address reaches; the draw calls are back in saveload.cpp as
`gx(...)`), Surface_fillRegion, Surface_stretchBlit = jgld bltS -> blitPair1 (the mask blend from the matched asm),
drawText4049d0, drawPanelBackdrop (its Terrain::drawLine bar is OpenGL, not in the back buffer) and drawWindow's
drawFrameR3 + drawMessageBuffer for a dialog without a golfer (blitPanel's 0xf8 bytes shade through build16BitColorTable's
block 1). Port changes: TraceInput applies the harness's memory writes at a modal loop's cursor reads too (cov_b_location's
date write; control tick.frame 7), and a boot trace with no frame head now runs to its end, with idle passes per unread
CURSOR record and the pending snapshots taken when the input runs out (gfx_end_of_input); boot.run(to_end=True) no longer
returns NO_FRAMES before running. Segments 205 / 205 (digests unchanged) and boot 58 / 2 / 1 unchanged.
Roadmap S0, baseline and frame census (2026-10-24, re/roadmap/BASELINE.md, frame_census.tsv, frame_causes.tsv, notes
log/roadmap/S0/, U-1100..U-1102, U-1110..U-1115, U-1120 / U-1121). Every regression gate was measured on HEAD 4bfefc2a in a
git-archive export (port/build/S0_head, with junctions to the gitignored data), so the S1 / S4 / S6 working-tree edits were not in it.
Results: repo static 600 / 2 skipped, port suite x64 41 / 41 files, segments 205 / 205 on x64 and Win32 with 0 digests changed, boot
gate 58 PASS / 2 KNOWN / 1 NO_FRAMES on the 61 HEAD scenarios, tick 250, sve_roundtrip 15, gl_raster 139, terrain_state 500,
terrain_render 40, terrain_gl_setup 6, gfx 17, port static 6, terrain_live 144. The Win32 backend suite has no record after phase 3.R:
20 failures in 8 files are its baseline. Of these, 11 are x64-only helpers (U-1100) and 9 are browser / load / save digests and
files not yet diagnosed (U-1101). port/corpus is shared, so `boot --all` also runs uncommitted recordings: count the HEAD scenarios
only (U-1102). port/test/framegate.py runs every checkpoint of a scenario in one boot with gfx.attach and LiveTerrain attached. It
writes one row per checkpoint with: status PASS / UNSUPPORTED / FAIL / NOT_RUN; screen kind (title, location, ingame, frame); the
terrain-on-screen rule from the trace's clock sites (U-1110 / U-1111); px per region (in-game: hud_title, hud_meters, hud_message,
toolbar, bottom_panel, view); one-level and beyond-one-level px; the unsupported drawing calls before the snapshot. LiveTerrain at
an address of its own leaves every sim hash equal. The census over HEAD's 609 checkpoints (64 scenarios) gives PASS 45 (title menu,
location screen and one world map), UNSUPPORTED 525 and NOT_RUN 39. Of the 39, 34 have no trace (cov_a_quit, cov_a_quit_title,
golfer_events) and 5 got no snapshot. U-1120 covers those 5: with a gfx.snapshotAt pending, sg_run_boot returns -11 at the end of
cov_a_champ, championship and cov_k3_career. Terrain checkpoints are 522, and 520 of them rank under `terrain not drawn`. Terrain
pixels never reach the gfx back buffer at HEAD, because terrain_state's draw passes only set the view quadrant. The G2 criterion
(1.5 % one-level, 70 px beyond one level, glraster constants) is applied to the view region's area until S3 draws the terrain
(U-1121). 2D causes (the first unsupported drawing call): Widget_writePixel 316, Surface_blit3 with an unmodelled source or
destination 168, Image_draw with a SpriteRef the backend has no sprite for 39, Surface_drawScaled2 2. No non-PASS row lacks an
unsupported call.
Roadmap S2 wave 1, the gfx body families (2026-10-24, notes log/roadmap/S2/1_notes.md, 2_notes.md, steps re/roadmap/S2_steps.tsv,
screen map re/roadmap/S2_screens.tsv, U-1200, U-1210..U-1214). Drawing bodies of the gfx backend can now live in their own files:
`port/src/sim/gfx_<family>.cpp` includes port/src/sim/gfx_body.h (Image8 / Sprite8 / Surface16 / Gfx and the shared helpers, moved
out of gfx.cpp), defines a GfxBodyDef table and `kGfxFamily_<family>`, and is listed by one `SG_GFX_FAMILY(<family>)` line in
port/src/sim/gfx_families.inc, which gfx.cpp expands into explicit references (portsim is a static library, so a self-registering
object would be dropped) and one registry entry `gfx.family.<family>` carrying the family's negctl switch. New test entries
`gfx.call(name_va, a0..a10)` (one body without a trace) and `gfx.check()` (`$gfx/bodies.txt`, the count of names registered
twice); test-only family `selftest` (gfx_selftest.cpp). S0 recommendations 1, 3, 4: gfx.log lists every distinct unsupported reason
in first-occurrence order with no cap (`first	<k>	<reason>`), kNotDrawing gains playSound, Snd::status, capsLockOff,
emptyFunc_4a0890, saveCourseJpg, Widget_readPixel, enterModal483d30, and gfx.render's budget is the FRAME records up to the last
checkpoint + 2 with to_end past the last FRAME record (record_budget). Gates: test_gfx 25 / 25 x64 and Win32 (17 + 8; 6 of the 8
RED with the selftest line removed), segments 205 / 205 x64 + Win32 with 0 digests changed, boot 58 / 2 / 1 of HEAD's 61, framegate
sandbox_basic + cov_b_location + cov_r_quit equal to the census in status and px (first_unsupported now the first drawing call,
Widget_writePixel instead of Snd::status), port test_static 6 / 6 with the family-aware module listing. Controls 8 / 9 RED, gfx 3
blind on every PASS checkpoint (U-1200). Screen map (S2.2): of the non-PASS census rows, the pinned full-screen 2D checkpoints are
credits (cov_r_quit, 5), load browser (cov_a_keys k_dlg_L), routing map (k_dlg_R, cov_k1_routing routing) and the character editor
(cov_a_career career_editor, cov_a_golfer button_0, cov_a_keys3 q_editor); the census marks the last six terrain yes (U-1210). The
port's screen functions make no drawing calls (3.7b dropped them), so each screen family adds its calls. Twelve families have no
pinned checkpoint (difficulty, theme packs, pick a pro, hire, F1-F10 reports, end of year, preferences, scroll text / skill points,
shortcuts / roster, buy land, loading, radial menu). The 480 non-PASS H1WAIT rows are course view plus HUD (S3). claude2 workers have
no allow rule for a bare `cmake`, so a cmake call waits on an approval nobody sees: S2 workers build through
log/roadmap/S2/build.py (`~/bin/py`, VS Build Tools' cmake).
Roadmap S2.3, the credits screen (2026-10-24, notes log/roadmap/S2/3_notes.md, U-1220..U-1226): cov_r_quit's five credits
checkpoints (quit_00..03, credits: the scroll at five positions) equal the original's back buffer in every pixel, x64 and Win32.
Three body families: `surface` (gfx_surface.cpp: Image_ctor / dtor / releaseAll, Surface_ctor / free / createFull erase the key's
backend entries; Surface_blitTakeDirty = Surface_blit; Surface_drawRegion = jgld Sprite::draw -> draw16_1001cde0, which draws
0xfe and skips only 0xff, at the trims; Surface_blitThunk = Surface_blit3 with one shared size), `prim_text` (gfx_prim_text.cpp:
Widget_setQuad's colour words per target in the new per-family Gfx::state_of, drawText's min(strlen, limit) characters in the
main colour, no shadow) and `credits` (gfx_credits.cpp: the logo at its own size, the kept lines read by the backend, one line per
call, the logo centred by its kept width). loadImage's flag bit 0 decodes with the file's palette. The port's show_credits_screen
makes the original's drawing calls (credits_gx, local keys). TraceInput::clock advances the gfx snapshots past a served clock read
(ordinal + 1): the credits scroll reads the clock once per pass and dispatches nothing, so before it every snapshot came from the
end of the input. Gates: census 609 rows, 604 unchanged, the 5 credits rows UNSUPPORTED -> PASS (PASS 45 -> 50); segments
205 / 205 x64 + Win32, 0 digests changed; boot equal to S2.1 in all 67 rows (HEAD's 61: 58 / 2 / 1); test_gfx_credits 47 x64 and
Win32, test_gfx 25, port test_static 6, test_commands 83 / 44 skipped, test_tick 250. Controls: 4 RED on the frames (prim_text 1,
2, credits 2, tick.frame 11), every other mode RED in a unit test (equivalent on the five frames, U-1223); gfx.attach 3 stays blind
(the background has no byte 0xfe, U-1200).
Roadmap S2.4, the routing map and the load browser (2026-10-24, notes log/roadmap/S2/4_notes.md, U-1230..U-1234): cov_k1_routing
`routing` and cov_a_keys `k_dlg_L` equal the original's back buffer in every pixel, x64 and Win32; cov_a_keys `k_dlg_R` (two holes
built) differs in 370 px, all in the map's hole overlay: the tee / flag sprites (FLIC sheet cells through the sprite queue's
scaled draw, S3.4 / S3.5) and the OpenGL tee-to-green lines (Terrain::drawLine, S3.8), reported as drops without a body. Three
body families: `prim_fill` (gfx_prim_fill.cpp: fillWindowRect / Win_fillRect = jgld fillRect16 with a direct colour, blitPanel
style 0 / nine-cell over Sprite::drawG, Display::setScale as state, fillConvexPoly16 with its edge walker and span filler),
`prim_text2` (drawCenteredText2: a shadow run in run-palette colour 1 at (+0, +1), entry 1 measured as RGB (0x80, 0, 0), U-1230)
and `routing` (gfx_routing.cpp: loadInfoScreenArt's whole art manifest, now drawn at boot; the map's tile diamond). Image_draw
reports a display scale other than (1, 1, 1). routing.cpp's MapDraw and saveload.cpp's browser issue the original's drawing
calls; a text the original builds in g_textBuffer is built there, drawn and the buffer put back, so the GameState with a
backend equals the one without. Gates: census 609 rows against S2.3, PASS 48 -> 50 (raw), 12 already-unsupported rows grew by 344 .. 1,012 px from
stale root content and partly drawn character-editor frames (first unsupported call unchanged); segments 205 / 205 x64 + Win32, 0 digests changed; boot
equal to S2.3; test_gfx_routing 35 x64 and Win32, test_gfx 25, test_gfx_credits 47, port test_static 6. Controls: 12 modes RED
(10 on the frames, 2 only in unit tests), 0 blind.
Roadmap S2.5 / S2.6, scenarios for the 2D screens (2026-10-24, notes log/roadmap/S2/5_notes.md, 6_notes.md, U-1250..U-1253).
S2.5 recorded the missing traces of cov_a_quit and cov_a_quit_title (the p24-era runs ended at the exit with a 147-byte .trace.json;
trace_rec.js's ExitProcess hook now ends them; no re-pin): every one of their 24 checkpoints now runs, the port's boot gives cov_a_quit
PASS 301 / 301 and cov_a_quit_title NO_FRAMES (it never leaves the title menu and credits), and with the S2.4 backend framegate reads
19 PASS (every terrain-free row) and 5 in-game rows UNSUPPORTED. S2.6 pinned cov_r_s2_screens (re/tools/scenario_s2_6.py, driven by
re/tools/s2_6_runs.py, which imports it because scenario.py's hook then globbed only scenario_s1_*.py): 32 checkpoints inside the 2D
screens no earlier checkpoint showed (loading at T 100 / 200 / 300, theme packs, difficulty, championship course list, pick a pro,
radial hover label, reports F2 / F3 / F4 / F7 / F8 / F9, the F10 board, hole stats, best scores, the system menu alone, preferences,
hire, end of year, skill points and their popup, the land popup, buy land), det_a == det_b == det_c on instances\s2_6, the replay
from the trace SAME at 31 of 32 (theme_packs differs in the frame only, 2,170 px, U-1252). The scenario writes g_date and
g_eventFlags567a1c to reach end of year and buy land. The port's boot equals the capture up to record 1156 except the cursor tile
at record 771 (U-1253, cause not measured), then stops at the unported showBuyLandScreen (U-1251). Scroll text has no checkpoint: the
character editor's load control stops the lockstep clock inside runScrollText (U-1250).
Roadmap S2.7, the boot screens (2026-10-24, notes log/roadmap/S2/7_notes.md, U-1260..U-1263): cov_r_s2_screens loading_t200,
loading_t300, theme_packs, difficulty, champ_courses and pick_a_pro equal the original's back buffer in every pixel. Family
`boot_screens` (gfx_boot_screens.cpp): showLoadingScreen.draw (picture r and hint r2 from the screen's two Random::range draws,
drawMessageBuffer with drawTextSurface's shadow, the bar), selectDifficultyScreen / showThemePackSelect setup, redraw, hover, tip,
teardown, pickAProScreen setup / textSurface / teardown; the screens' port functions issue them, Setup now draws the title art
under the boot's loading screens, and load_flic issues the refreshFull and consumes waitTicks(10, 0)'s clock reads that follow
each boot loading screen (a checkpoint inside that wait shows it). U-1252 settled: showThemePackSelect draws a sixth column cell at
an uninitialised stack word w (U-1260); the pinned runs have w = 0x1a, the trace recorder's runs a w that hides it; the backend
uses 0x1a (SG_GFX_THEMEPACK_COL5 overrides). Controls 13 modes RED (11 on the frames, 2 in unit tests).
Roadmap S2.8, the report screens over the course (2026-10-24, notes log/roadmap/S2/8_notes.md, U-1270..U-1278): on cov_r_s2_screens
rep_comments, rep_histograph, rep_financial, rep_shortcuts, rep_roster, hole_stats, best_scores and end_of_year equal the
original's back buffer in every judged pixel, and hire differs only in its four animated employees (addSprite FLIC cells, S3,
1,397 px pinned). JUDGED PIXELS: every report dims the whole in-game frame (black.pcx through Sprite::drawAlpha's quarter band =
draw16_1002ed60's 555 arithmetic, U-1271) and lies over the course the backend does not draw yet, so test_gfx_reports.py boots
three more times with the root filled with 0x0000 / 0x7fff / 0x2a55 at the screen's first call (`reports.begin`, negctl modes
101..103) and judges only the pixels that do not change with that underlay; their counts are pinned. Family `reports`
(gfx_reports.cpp): Surface_drawSprite, Widget_draw / Widget_draw2 / drawStringAt / drawStringAt2 (font swap, left / centred /
right / centred in a width), Widget_fillRect (jgld line16 with its octant walk and clip), Surface_box / Surface::callBounds (the
clip), reports.begin; gfx_reports.h ScreenDraw gives the screens gx calls, texts built in g_textBuffer under save / restore and the
art's cut sizes (Sprite8::cut_w / cut_h = SpriteRef +0x18 / +0x1c). framegate now judges the checkpoints before a boot that stops
on a missing name (U-1270). rep_report1 and preferences are S2.9's functions (U-1276). Gates: segments 205 / 205 x64 + Win32,
0 digests changed; boot equal to S2.4 on its 67 rows; goldens of showEndOfYearScreen / showPlayerCommentsReport re-pinned through
the oracle; controls 17 modes, 14 RED on the frames, 3 RED in unit tests.
Roadmap S2.9, dialogs over the course (2026-10-24, notes log/roadmap/S2/9_notes.md, U-1280..U-1283): showPopup (every popup and
the System menu, which is a showPopup), textInputBox with saveGameDialog's panel, and showCourseReport's association page are
drawn by the backend. The course under a dialog is S3's, so each dialog is judged in its own rectangle, measured on the
reference frame (the region of the fill colour 0x4e79 grown by 5 px; the page's panel colour), on the pixels the dialogs decide:
family `dialogs`' probe (negctl modes >= 0x10000) fills the root surface when a dialog stack starts, and a pixel is judged when
three probe backgrounds (black, white, a pseudo-random pattern) and the plain run agree. All 14 rectangles of the 10 pinned
checkpoints (cov_r_quit exit_prompt / exit_save, cov_a_keys k_dlg_S / k_dlg_J / k_C, cov_a_keys2 C_prompt, cov_a_champ /
cov_a_play n_prompt, cov_a_build2 remove_prompt1, cov_k3_career prompt / prompt2) have 0 failing pixels, x64 and Win32; held:
9..32 px per popup (blended border corners), 28,579 on the page (its drawAlpha pass and transparent parts). Family `dialogs`
(gfx_dialogs.cpp, 8 bodies): the dialog art loadInterfaceArt cuts (popup border and masks, option bullets, the icon row),
drawPanelS2, drawTextSurface, (Widget_fillRect, Surface_drawSprite, Widget_draw, drawStringAt, drawStringAt2: the reports family's bodies after S2.13), showPopup's
odd-y box (the colour read from the root under it), dialogs.begin / dialogs.end. The screens' calls are in commands_popup.inc,
saveload.cpp and reports.cpp (texts built in g_textBuffer, which is put back with g_textBuf). Gates: census 609 rows against
S2.4, same 427 / better 146 / worse 36 (5 = cov_a_quit rows S2.5 made runnable, 31 = dialog pixels left in unsupported in-game
frames, first unsupported call unchanged); segments 205 / 205 x64 + Win32, 0 digests changed; boot equal to S2.4. Controls: 11
modes RED (8 on the frames, 4 in unit tests), 0 blind.
Roadmap S2.13, integration of S2.7-S2.9 (2026-10-25, notes log/roadmap/S2/13_notes.md, U-1205..U-1207): the three steps' trees
merged on HEAD 2c05f02a + waves 1-4 by 3-way merges (log/roadmap/S2/13_tools/make_mirror.py), one patch 13_shared.patch. U-1278:
the five body names both reports and dialogs registered (Surface_drawSprite, Widget_fillRect, Widget_draw, drawStringAt,
drawStringAt2) are the reports family's only (gfx.check 0 duplicates): jgld Sprite::drawAlpha's flag byte starts 1 and is cleared
only when the destination format's +4 is 1, and with it set the quarter band is draw16_1002ed60 (RGB555 masks), the member S2.8
measured on the references; the dialogs' controls moved with the bodies (reports modes 9, 10, 20, 21 and 1). U-1254 fixed: the
end-of-year and course-report modal loops make the original's per-pass View_toLocal cursor read (reports.cpp report_loop; after
the pump on the end-of-year screen, before it in the course report), so cov_r_s2_screens' boot equals the capture through record
1156 and stops only at the unported showBuyLandScreen (U-1251); control showEndOfYearScreen mode 6 brings back the record-771
cursor-tile divergence. Gates: test_gfx 25, credits 47, routing 35, boot_screens 32, reports 36, dialogs 32, port static 6,
commands 83 / 44 skipped, tick 250, reports 14 (16 with SG_EMU_ORACLE=1), s2_5 3, s2_6 4 (x64); segments 205 / 205 x64 + Win32,
0 digests changed; Win32 gfx files and s2_6 pass; boot 70 rows 65 PASS / 2 KNOWN / 2 NO_FRAMES / 1 FAIL (cov_r_s2_screens at 1156).
Roadmap S2.11, dialogs over the course 2 (2026-10-25, notes log/roadmap/S2/11_notes.md, U-1215..U-1219): the TOURNAMENT RESULTS strip
(showTournamentResults' scorecard layout), the bulletin board (showBulletinBoard's trophy shelf) and the SimFoto / message ball are drawn
by three families, gfx_results.cpp (drawStringRightAt, results.begin), gfx_simfoto.cpp (loadInterfaceArt.ballArt, drawWindow.golfer,
simfoto.begin) and gfx_bulletin.cpp (the board's art cuts of loadInterfaceArt kIa11..kIa16, the board image, events, photo pins, the
fixed sprites). Findings that shaped it: the results rows and the close button read the bands' SpriteRef +0x18 / +0x1c, which only the
backend holds (tick_panels.inc reads gfx_sprite_cut with a backend: drawing positions only, the y break never fires before the rank
break); the port's tickMessage (commands_layout.inc, registered) never handed drawWindow to the backend; every pinned SimFoto sits in
narrateShot's pumpAndRedraw(0), so its frame is narrateShot's own, which the port's shots.cpp now draws with a backend, running
drawWindow's window-height pass and putting g_textBuffer / g_textBuf / g_ui_821c50 / g_value587ea0 back (commands.drawWindowGfx). The
ball's head (Surface_drawScaled of a heads cut, S3.6; palette by composeGolferSprite, S3.5) is not drawn: the probe holds the ball's
0x8c square (U-1216). Gate (test_gfx_dialogs2.py, rectangles measured on the reference frames by log/roadmap/S2/11_tools/rects.py, held
pixels by the families' probes): all 17 rectangles of 13 checkpoints 0 failing pixels, x64 and Win32 (results strips 101,719 judged px
each; the bulletin board all 480,000); championship tournament_end has no snapshot (U-1120). Segments 205 / 205 x64 + Win32, 0 digests
changed; boot equal to S2.13. Controls: 10 RED, 0 blind.
Roadmap S3 wave 1, the terrain draw-list passes and the rasteriser's last two rules (2026-10-24, steps S3.1 / S3.1b, notes
log/roadmap/S3/1_notes.md, 1b_notes.md, U-1300..U-1304, U-1390..U-1392):
port/src/sim/terrain_render.cpp now ports localRender (25 window tiles, no culling of its own; drawTileObjects' camera-facing wall
neighbours, recursive and culled against pTile), stripRender (odd directions are two complete nested passes, 8 + 2, 2 + 4, 4 + 6,
8 + 6, each with its own push / list / flush) and pathUpdateRender (no setViewAngle: it keeps the quadrant the previous pass left;
left edge clipped to x + 1, U-1302), with drawTile's sorted insert (its `other angle` branch test never holds, so the secondary walk
always runs) and the draw list empty at every pass entry (137 snapshots); terrain_render_entry serves sg_gl_render(_to_raster)'s
entry kinds 1 strip, 2 render, 3 local, 4 pathUpdate. Recorder: re/tools/gl_record.py --scenario runs a pinned scenario through
scenario_refs.py with gl_rec.js from process start and --inject queues draw passes on the game thread (Frida runs NO hook for a
call made inside an Interceptor.attach callback, so the injection is an Interceptor.replace of render / localRender; state the
module observer's tryInstall reads must be declared before it). Recordings g2 .. g6 (instances\s3_1, cov_b_holes18 / cov_b_paths,
every run SAME as its reference; port/corpus/gl/<label> via `port.test.glcorpus export`). Evidence: 237 draw-list passes equal the
original line for line (33 localRender, 160 stripRender, 44 pathUpdateRender; zoom 0 / 1 / 2, four angles, directions 1..8);
GlRaster from the port's stream meets G2 on 209 / 209 passes outside g6 (one-level 0.08 .. 1.25 % of drawn, at most 24 beyond);
g6 (60 % type 7, border column not hidden) missed on 16 / 28 with the original's own stream too (U-1300), which S3.1b traced to two
rules of Windows' generic GL that GlRaster lacked (found with glreplay.exe, which replays g6 bit-exact): the specular term is a
256-entry table over [0.0005^(1/n), 1] read at the nearest entry, not the exact pow (gl_raster.cpp spec_lookup; 0 of 4,923 probe
quads wrong; tune 27 = 1 restores the pow, U-1390), and a pixel centre exactly on a horizontal edge belongs to the edge pointing to
+x (tune 10 = 2 restores the old rule, U-1391). Now every pass meets G2 (g6 28 / 28) and one-level pixels fell everywhere: the 209
other draw-list passes 0.46 -> 0.03 % mean, c7 + t3 0.47 -> 0.06 %; the residual 0 .. 0.21 % is not split further (U-1392);
controls 9 .. 14 RED on the streams (14 redraws a tile over itself: blind in pixels). Unjudged: passes that draw no tile leave no
snapshot (U-1301), NULL tile / pivot paths (U-1303), vacuous G2 on 4 localRender passes (U-1304). The renderer is still reached only
through sg_gl_*; LiveTerrain's draw passes into the back buffer are step S3.2. U-1120 resolved (S3.3): a boot run with to_end
ended in its last frame's Blocking pump with no input left, NeedInput (a MissingResource) surfaced as -11 and pending snapshots
were never taken; run_frames now takes them there (cov_a_champ, championship, cov_k3_career: 22 census rows run).
Roadmap S3 wave 2, FLIC sheet cells (2026-10-24, steps S3.4 / S3.4b, notes log/roadmap/S3/4_notes.md, 4b_notes.md, U-1330..U-1339): module
flic (port/src/sim/flic.cpp, portsim/sim/flic.h) decodes the user's .flc files (GameFiles) into loadFlic's sheet cells: cropSprite result,
SpriteRef geometry +0x10 .. +0x28, jgld Sprite words, pixel bytes, column and sequence counts, slot and shadow palettes; menu.cpp's
load_flic hands them to an attached gfx backend through flic::sheet_to_gfx (nothing written into the GameState). Oracle: the original's
sheet memory after every loadFlic (re/tools/flic_cells_rec.py + re/frida/js/flic_cells.js, pinned scenarios, hooks off): 443 / 443
entries of menu_flics.inc's tables equal for Parkland, Links, Desert and Tropical, 1,698 distinct loads (148,926 cells, boot loads
included) equal on x64 and Win32; controls (delta chunk skipped, palette chunk ignored, frames reversed, 0xfe drawn: thin, U-1334) RED.
FINDING IN THE STOCK GAME (U-1330, settles U-1701): the exe names `trees\Tropic\Tree Cerc` / `Tree Drac` (space; v1.00 too) while the
install, the v1.03 patch and the cab hold Tree_Cerc / Tree_Drac, so the original dies at loadWorldAssets' first tropical tree load
(flicOpenStream returns 0, then Image_draw reads 0x8 at 0x00473e67) on every tropical course of the shipped install
(log/roadmap/S3/4_rec_hawaii_stock.log, instances\s3_4); copies under the exe's names (instance only) make all 114 tropical loads
complete. Decision D5 option a (S3.4b): defect switch flic.treeAliasNames, default 1 = Fault{8, 4} at that load as the original, 0
(profile fixed) = flic::resolve_name returns the underscore names; it acts in load_flic's column path with or without a backend,
since the columns decide later cell allocations. Open: U-1332 (the palette a sheet cell is drawn with: sprite-queue step S3.5), U-1333 /
U-1334 / U-1335 (evidence limits), U-1338 (other missing main files still run on with status 1; no shipped load reaches one).
Re-merged with S3 waves 4 / 5 (roadmap S3.11): the defect row is D53 of re/port/defect_decisions.tsv (75 rows, 13 fix / 62
keep, its input log/roadmap/S6/s3_4b_decisions.tsv) and profile fixed has 9 switches. test_menu's emulated original faults like the stock game (Image_draw on loadFlic's NULL stream reads 0x8): its stock tropical cases are
pinned against that Fault at flic.treeAliasNames = 1 and `_alias` cases (the spaced copies) run the port at 0; test_terrain_live runs
S3.2's hawaii recording (made with the copies) at 0 and checks the Fault at 1 (roadmap S3.4c).
Roadmap S3 wave 3, the terrain in the frame (2026-10-24/25, step S3.2, notes log/roadmap/S3/2_notes.md, U-1310..): the
original's GL draws into the ROOT surface (initTerrain gives Terrain::initSystem the DC of the jgld surface at
[g_rootWindow + 0x278]; that DIB's memory row 0 is the picture's top row, so GlRaster's buffer and gfx.root are the same
array, no flip). updateTerrainRender clears the root through Window::paint(g_rootWindow, 0x80000000) (clear16 with colour 0),
the passes draw over it, Surface_blitThunk copies root -> the exe's back surface g_image51b068, an unmoved camera copies the
back surface whole onto the root and local-renders changed tiles, and renderTerrainStrip draws the strip and copies the shifted
old picture back. Body family `terrain` (port/src/sim/gfx_terrain.cpp: terrain.windowPaint, terrain.pass, terrain.blit) runs
LiveTerrain's passes through S3.1's terrain_render_entry into a GlRaster over gfx.root; the GL setup is the port's own
(terrain_gl_setup). gfx.log carries a per-snapshot terrain flag (snapshot_terrain) and framegate a `terrain_drawn` column;
LiveTerrain attaches on Win32 too (runner32 / lib32 ops). EVIDENCE: the original's root surface recorded at the return of every
updateTerrainRender (re/tools/terrain_calls.py --pass-frames / --frames-lite, instance s3_2, every hooked run SAME as its
reference): per census checkpoint the port's root at the last pass before the checkpoint meets G2 over the pixels the original
drew on all 527 terrain checkpoints (log/roadmap/S3/2_terrain_verdicts.tsv; S0's `terrain not drawn` 520 -> 0), four themes,
x64 = Win32 terrain pixels; controls (wrong zoom, wrong theme texture set, pass skipped, strip shift, Window::paint not clearing)
RED. Segments 205 / 205 and boot 58 / 2 / 1 unchanged (the backend writes no GameState byte). Frida spawns hung 5 times on the
instance (U-0683 pattern): kill the own PID and retry. Open: U-1310 (rasteriser fixed at 800 x 600), U-1311 (the back surface's
depth inferred from results), U-1312 (pathUpdateRender never runs organically), U-1313 (one organic strip direction framed),
U-1315 (two title rows keep the terrain flag at 1).
Roadmap S3.6, HUD panels and the in-game Surface_blit3 (2026-10-25, notes log/roadmap/S3/6_notes.md, U-1350..U-1359): two
graphics-backend families. `pixel` (gfx_pixel.cpp): Widget_writePixel (the low word, nothing outside the surface) and
Widget_fillRect (jgld line16: both ends drawn, a zero-length run draws nothing); the census rows after an in-game frame
reported Widget_writePixel only because a run's unsupported list is cumulative. `hud` (gfx_hud.cpp): Surface_blit3's 16-bit
sources, family surfaces and nearest-pixel magnification; loadInterfaceArt(1)'s whole image half (48 generated tables from
c3fv.cpp, `log/roadmap/S3/6_gen_iart.py --check`) and loadWorldAssets' build-palette cuts, checked against the original's
SpriteRef records in segment start images (>1,000 per scenario, 0 differ); Surface_blit2 / drawScaled2 (Sprite::drawG with
g_halfBrightLut), Surface_drawScaled under a display scale (jgld draw16_1001bc40: float scale, 16.16 steps, the x-mirrored
body), drawFrameR3 / S2, drawTerrainToolbar, the radial menu, showInfoCursorPanel, drawAmenitiesPanel, drawGolferActionPanel and
drawMatchScoreboard (names and numbers from the registered buildGolferName / appendNumber over a saved and restored GameState).
The golfer list and the employee detail panel (drawEmployeeViewPanel / drawEmployeeDetailPanel, S3.6b) are drawn from
c3fb.cpp / c3fc.cpp over the state commands_layout.inc hands over before its own writes (the detail panel's scroll step and
this frame's list are recomputed); names come through g_textBuffer with the buffer put back. No pinned frame holds the golfer
list or a selected employee, so their evidence is an EMULATOR CALL ORACLE (test_gfx_hud.py, SG_EMU_ORACLE=1): the original
functions under Unicorn with every drawing callee a recorder give the same calls (callee, image, position, colour, text) as
the port's `gfx_hud.recordPanels` log on 240 varied segment start images per panel; the default suite pins the port's log
digests; controls 11..15 RED. The detail panel's employee figures (addSprite) and its circle (drawCircle) go to S3.5 / S3.8. GATE MEASURE: framegate --no-live (no terrain; S3.2's terrain is judged under G2) and no scene sprites, so `log/roadmap/S3/6_hudgate.py` runs framegate twice with the test-only root fill
SG_GFX_HUD_BGFILL=0x0000 / 0x7fff after every present; a pixel equal in both was drawn opaquely and is compared with the
reference per HUD region. Hover tips are reported as `tooltip:` (S3.7 / S3.8) and the sprite queue's draws as `addSprite:`
(S3.5).
Roadmap S3.5, scene sprites (2026-10-25, log/roadmap/S3/5_notes.md, 5_merge.md, U-1340..U-1348): body family `scene`
(port/src/sim/gfx_scene.cpp / portsim/sim/gfx_scene.h, tests port/test/test_gfx_scene.py) draws the sprite queue
(drawQueuedSprite.draw / .label from tick_render.inc, renderSpriteQueue.end), Sprite_draw, Surface_drawSprite and the scaled
Image_draw path (gfx.cpp hands them over through gfxs::image_draw), plus the palette objects loadWorldAssets loads
(`loadWorldAssets.palettes`) and composeGolferSprite's palette layers. The jgld bodies behind them: Sprite vtable 0x1011d380 slots
+0x6c drawG, +0x78 blt16a (plane blit through build16BitColorTable's five blocks), +0x4c blt16c, +0x94 drawAlpha (15 bodies by alpha
window, programs generated from re/match/jgld_blit16.cpp), +0x44 drawB, all on one 16.16 scaling skeleton under the display scale
(drawQueuedSprite sets (s, s, d) and restores (zoom, zoom, 4); P6 sets (1, 1, 1)). U-1332 resolved: blt16a / blt16c store the
key's palette word in the Sprite and never restore it, so a sheet cell draws with its queue entry's g_images palette. Evidence: the
ORIGINAL jgld bodies in Unicorn on constructed sprites (40 cases, x64 = Win32), sandbox_basic's three checkpoints exact on more than
50,000 scene px each (one-level only where a blend meets the terrain, U-1347), controls 5 of 6 RED (5, palette-word persistence, is
blind on the pinned scenes). Cells drawn with no backend sprite are cells whose cut kept nothing; the original's records agree
(section 6). buildSceneSprites' alpha now reaches the backend as float bits (tick_scene.inc). The exact sprite-pixel oracle for every
checkpoint, the original's root at renderSpriteQueue's return (5_tools/scene_rec.py), needs a game run and has not been recorded.
Round 2: the exact oracle ran (5_tools/scene_rec.py on instance s3_5, hooks off: the original's root at
every renderSpriteQueue return, 66 scenarios SAME as det_b, 585 copies in port/corpus/scene/): 550 of 582 checkpoints equal in every scene
pixel up to one level, the rest overlay lines the port does not draw and one missing standing figure (U-1349). drawAlpha on the root
draws the flag-set member of each window's pair (555 masks; round 1 had the 565 members); Surface_drawSprite has one body, gfx_reports.cpp's,
which hands gfxs::surface_draw_sprite every case outside its quarter band; bodies composeGolferSprite and addSprite and Image_draw's mask
palette were added for S2.10; the hole flag and the amenities in gfx_hud.cpp draw through gfxs::add_sprite.
Roadmap S3.5b, S3.5 rebased (2026-10-25, log/roadmap/S3/5b_notes.md, 5b_merge.md, U-1385..U-1387): S3.5's scene family on the
committed tree plus S3 waves 2-4 (5b_mirror.py), the exact queue oracle re-measured on all 585 recorded checkpoints
(`5_sprite_gate.py --queue --all`, port/corpus/scene): 584 have a port copy (cov_r_s2_screens end stops the port's boot), 47,950,391
scene px judged, 29 checkpoints with pixels beyond one level (was 32): the five cov_r_bio rows that were unjudged in the S3.5 base
now run and hold only the golfer-selection ring (S3.8's), the others are rings, path lines and isolated pixels under blends. The
standing figure the port missed (cov_k1_animals bought / menu2, cov_b_inject end) is placeAnimal's queue entry
(`enqueueSprite(sx, sy, sy, 0x1e, 0x28, cell, 4, (n + 0x9c) | 0x100)`, an animal record drawn with the golfer palette path),
dropped before and now queued for an attached backend (golfers_world.inc): 187 / 216 / 92 px beyond one level -> 0, control 13
(an entry past the golfer table draws nothing) RED; test_standing_figure_equals_the_original / _negctl_red in test_gfx_scene.py.
Roadmap S3.7, the marked-up HUD text, the ticker and the hover tips (2026-10-25, notes log/roadmap/S3/7_notes.md, U-1360..U-1368):
body family `text` (port/src/sim/gfx_text.cpp). The root text object (the Image at g_rootWindow + 0x274) has markup ON from
Image_ctor (f_cfg_4e4210 = 1), so every frame text is drawRichText's: the family ports the scanners, drawTextRun (style fonts and
colours, baseline, shadow run, tab stops of 30, '_', the run palette's entry 1) and drawMarkupRun / Text_put, and the core's
draw_text_string hands it every text with a markup character, a tab or a palette colour. drawWindow is the family's whole body
(golfer portrait, picture windows, the slide-in, drawFrameR3 / S2 / drawWindowFrame, the centring) with drawMessageBuffer; the
TICKER had never reached the backend (commands.cpp draw_window_effect handed nothing over) and now does. The HUD panels' hover tips
(showInfoCursorPanel's tool tip, transcribed from the instructions, with its theme text, and slot tip; drawTerrainToolbar's,
drawGolferActionPanel's, the employee panels', drawAmenitiesPanel's tip and building row) are drawn through S3.6's gfx_hud.cpp;
their drawMapLine bars are S3.8's. Text_put became a measuring drop (gfx_take_value: with a backend the frame code gets the width
that places the thought labels' bars). EVIDENCE: an emulator oracle down to a fake jgld surface (setFont / colours / draws / end)
over the ORIGINAL text functions (391 random marked-up cases), drawWindow (300) and the six panels' tips (60 per kind), all equal;
goldens in the default suite; the opaque two-fill frame measure over every pinned scenario: no in-game checkpoint keeps a bad
pixel the family drew (the 9 rows with any are other steps' content over or under it). The image cursor exists only over an EditBox
and the widget tooltips only through Tooltip::show: neither is on a pinned frame (U-1360, settles U-1069's cursor / tooltip parts);
drawWindow's and the tips' texts never take drawTextSurface's shadow (g_font51b360 H 15, g_fontRef_519fd8 H 10), U-1072 resolved.
Controls: 10 RED on the oracles, 5 on frames.
Roadmap S3.8, overlay lines (2026-10-25, notes log/roadmap/S3/8_notes.md, U-1370..U-1377): Terrain::drawLine is the only overlay
primitive the exe reaches (Terrain::drawCircle, both splines and renderTile have no caller), and Terrain::initGL leaves
GL_LINE_SMOOTH on with GL_NICEST, so the lines are antialiased. Measured with glreplay.exe on synthetic drawLine sequences: coverage
at a pixel centre = clamp(w / 2 + 0.5 - |n|) * clamp(t + 0.5) * clamp(L - t + 0.5), the segment clipped to the viewport then its
ends snapped by U-1076's fixed-point setup, width clamped to 10, a zero-length line draws nothing, alpha * coverage blended and
dithered as the triangles (probes: 108 of 74,466 changed pixels differ; footprint-edge pixels under 0.13 coverage, U-1370). Module
`terrain_overlay` (overlay.rasterLine, overlay.logStart / logWrite) and the bodies drawMapLine, drawMinimapSegment, setLinePoint,
lineTo, drawCircle in the terrain family (the line point in the overlay's state; the GL context, flip byte and screen size from the
Terrain object at gTerrain; no context = nothing drawn); drawPanelBackdrop's bar and the routing map's hole line call them. The
port's drawLine calls equal the recorded GL streams in count, width, colour and alpha (cov_b_holes18 3,362; cov_b_paths 3,221 + 13
tooltip bars S3.7 draws); in the census the line pixels are 109,292 of 117,446 equal to the original, judged ones 0.36 % one level.
Roadmap S3.11, the S3 resubmission (2026-10-25, notes log/roadmap/S3/11_notes.md, merge log/roadmap/S3/MERGE_w5.md): S3 waves 2
(S3.4 / S3.4b / S3.4c), 4 (S3.6 / S3.6b) and 5 (S3.5 / S3.5b) and S3.7 / S3.8 assembled as ONE tree on HEAD bbc9072f by
log/roadmap/S3/11_assemble.py (every edit from the steps' own edit lists, each anchor exactly once, `--root <dir>` applies them to
another tree). One interaction no step's gate ran: S3.5's resolution of U-1332 (a sheet cell draws with its queue entry's palette
object) failed S3.4's test_flic hook-up test, which drew a cell with no mask; the test now passes the slot palette as Image_draw's mask
through the TEST-ONLY body gfx_scene.setPalette (11_edits.py). Gates on the tree: port suite x64 68 / 69 files (test_boot's known
cov_r_s2_screens stop), Win32 S3 files 25 / 26 (test_tick's baseline failure), segments 205 / 205 x64 + Win32 with 0 digests changed,
boot rows unchanged on both, gfx.check 0 duplicates, census 698 rows PASS 91 (was 87, none lost, no row with more differing px),
S3.2 terrain verdict 527 / 527 unchanged.
Roadmap S3.13, the I9 refusal fixed (2026-10-27, notes log/roadmap/S3/13_notes.md, merge log/roadmap/S3/MERGE_w5b.md): integrator I9
refused S3.11's resubmission on 78ffb06d because S3.7's text family and S2 waves 6 / 7 drew the same things in three places no gate had
run together. One body per name now. drawStringRightAt is the text family's (S3.7: drawRichText at x - drawMarkupRun, the original's
pair); the `results` twin and its control are gone. drawWindow.frame is the text family's whole drawWindow, and it calls S2.11's
held-pixel probe (gfx_simfoto.cpp's exported window_probe / head_probe) for sel >= 0 where S2.11's golfer body ran it, so
`drawWindow.golfer` is gone; simfoto controls 2..4 and results 1 are text 12..15 (simfoto 4 had read BLIND in I9, text 14 reads RED).
Two duplicate draws caused I9's dialogs2 and text-frame failures and the skill dialog's 8 px: since S3.7 draw_window_effect draws the
window, so S2.11's second draw_window_gfx in tickMessage drew the ticker twice, and S2.10's drawWindow.frame drop after
showCharacterIntro's draw_window_effect drew the intro window again at the ticker's default (0xf0, -8); showCharacterIntro now passes
its own position (0xc8, 0x172). integrator.py's switch-evidence map gained flic.treeAliasNames (test_defects_flic.py). The edits are
log/roadmap/S3/13_edits.py (36 anchored edits in 9 files, generated by 13_make_edits.py from the fixed mirror), applied after
11_assemble.py's steps by 13_assemble.py. Gates on port/build/S3_13: gfx.check 0 duplicates / 131 bodies, static 614 / 2, port suite
x64 75 / 75 files, Win32 75 files 61 PASS: the known 8 files / 20 tests, 5 x64-only files on the Win32 harness, test_fttext's 2 host-GDI metric tests (U-1393; they pass and fail with the host on x64 and in I9's mirror too); all 9 I9 clash files PASS, segments 205 / 205 x64 + Win32 + freetype with 0 digests changed, boot 70 rows 66 / 2 / 2 on
all three equal to I9, chains 342 / 342, ledger 1 row without a cause (as at I7), census 698 rows PASS 100 (59 rows with fewer differing
px than I9, none with more), S3.2 terrain 527 PASS. Process: a background shell's 2-hour limit stopped the suite loop while a pytest ran;
the pytest finished and wrote its result, but its loop must be restarted (13_suite_rest.sh FROM=<file>), and long suites run detached.
S3.14 (2026-10-27, notes log/roadmap/S3/14_notes.md, edits 14_edits.py applied by 14_assemble.py after 13_assemble.py): I12 found
test_app_ingame and test_app_live failing on this tree where d66aa755 passes. Three causes where S3's frame meets S7's app:
(1) once the course animates every in-game frame presents, so an app session's last present lies in its final frame, and
run_frames' boot last-frame branch took pending gfx snapshots only on NeedInput; it now takes them when that frame runs to its end
(U-1394, control tick.frame 12). (2) The test-slot hook runs pytest at BELOW_NORMAL and the app inherited it, which put the host's
ABOVE_NORMAL sim thread below every normal-priority thread: under 10 busy processes its frame waits ended 1-5 s late, and
d66aa755 itself failed with I12's `frame 40 never came (at 0)`; app_drive.launch now starts the app in the Normal class (U-1395,
SG_APP_PRIORITY=inherit is the control, RED on pacing under load). (3) The drivers took 1.5 s without a present as the title menu,
but S3's boot loading screens are 431-776 ms apart (flic sheets decoded for the backend; 117-174 ms at d66aa755) and a click posted
among them goes to the boot's load pump; app_replay._to_monterey and test_app_live._to_title now wait for the title menu's frame
(app_replay.title_menu_hash = the pinned sandbox_basic `menu` reference, U-1396). The app files' pacing gates measure the host:
run them one file per process at low load.
Roadmap S3 wave 6, S3.10 / S3.10b / the LiveTerrain log switch (2026-10-28 / 29, step S3.12, notes log/roadmap/S3/12_notes.md, merge
log/roadmap/S3/MERGE_w6.md, U-1388 / U-1389 / U-1399 / U-1673 / U-1398). Base e179bd24: 01d84188's committed gfx_terrain.cpp held S3.10's
thread calls while its gl_raster.h did not (7 C2039 / C2737 errors on x64 and Win32, all in that file; no other committed file used
working-copy-only code), and e179bd24 put I13's file back. S3.10: GlRaster's default pipeline runs a fast pixel loop that writes the old
loop's bits (edge functions exact as int64 in units of 2^-16, each row's covered span from the three inside tests, each pixel first with
reciprocals and kept only when every texel and 5-bit rounding decision is clear of its boundary) on pixel-loop workers that own interleaved
4-row bands (GlRaster::set_threads; gfx_terrain.cpp starts min(4, hardware threads / 2), SG_RASTER_THREADS=<n>); stream lines `#fastpath
<m>`, `#threads <n>` and the worker control `#workerctl 1`; SG_TERRAIN_PASS_LOG times each pass. S3.10b: terrain_state.cpp resolves the
Terrain object once per exported call and reads and writes it in place (VPtr), no float changed. LiveTerrain keeps a compact record per
call and formats the lines only when log() is read; log_size() / drop_log() / set_keep_log(false) / calls() let the app's TerrainLogBound
bound the log without a const_cast (U-1673). The app tests take SG_APP_FACTS_DIR (U-1398), and test_app_ingame's zoom test is strict (its
U-1640 xfail removed). Evidence against plain e179bd24 (whose port/ equals I13's mirror): all 299 recorded stream and chain passes
bit-identical to the old rasteriser with and without 4 workers, census 698 rows PASS 100 with every row's pixel count unchanged, terrain
527 / 527, segments 205 / 205 and boot 66 / 2 / 2 on x64 and Win32, live equivalence 68 + 2 and app replay 3 / 3, all with 0 rows
differing; port suite x64 82 / 82 files, Win32 the 22 known ids; controls RED: `#workerctl 1` on 270 / 299 passes, `#fastpath 4` on 47 /
268 (`#fastpath 3` blind, U-1389), terrain_state 23, LiveTerrain 6 (log off) / 7 (setType variation lost). App (LIVING-PC, Ryzen 5 3600,
MSVC 19.44 Release x64, three interleaved sessions): zoom frames 39..58 ms at 54..82 % load against 133..175 ms for plain e179bd24 at
59..96 %; idle (1..9 %) every terrain-pass frame 10..30 ms against 25..112 ms. The app's live-trace frame at cov_a_keys k_dlg_J varies in
about 1 run of 6 without workers too (U-1399).
Roadmap S3.17, the last two missing gfx bodies (2026-10-29, notes log/roadmap/S3/17_notes.md, merge 17_merge.md, U-1830..U-1833):
I14's census had 44 UNSUPPORTED rows stopping on `no body: Surface_createFull_fwd1` (22) and `no body: showRoutingMapScreen.teeSprite`
(22); a run's unsupported list is cumulative, so every row after the call kept the status. The routing map's tee and flag are now the
original's addSprite calls of the sheet cells (routing.cpp MapDraw::hole_map: tee = last column of sheet 0x18d + clamp(par - 3, 0, 2),
row 0, shade 4, flags 0x5e; flag = sheet 0x189 + g_courseType, column 0, row 3, shade 2, flags g_courseType + 0x60), drawn at once by
the scene family's addSprite body: cov_a_keys k_dlg_R 135 px (134 beyond one level) -> 1 px one level on a hole line (U-1832).
Surface_createFull_fwd1 (mainLoop's forwarder for the 'B' snapshot and the '.' / '/' routing render images) is Surface_createFull's
body with (this, a1, a2, 0, a3, a4, a5), the order checked on the original under Unicorn (U-1833). The 42 other rows now FAIL with
exactly I14's px, all HUD / bottom-panel / view residue (S3.15 / S3.16) except ui_tour `menus`' 252 px beyond one level, the money
popup `-$600` the port does not draw (S3.16's drawMoneyLabels gap). cov_a_champ n_yes / end were not fwd1: the boot's last frame never
made TraceInput's tail idle passes (only a boot with no frame head did), so the title menu after `Yes, cancel.` stopped at its first
pumpMessages before it drew; tick_run_frames' last-frame branch now sets them (control tick.frame 13), both PASS 0 px (U-1831). The
boot gate never runs that branch (it stops at the end checkpoint; only to_end runs: framegate, gfx.render, test_boot / test_gfx_eoi /
test_video). Controls RED: showRoutingMapScreen 11 / 12, surface 9 (unit test; equivalent on the frames: Image_ctor already erased
the key), tick.frame 13. Tests port/test/test_gfx_s3_17.py.
Roadmap S3.18, the bulletin board's photos (2026-10-29, notes log/roadmap/S3/18_notes.md, merge 18_merge.md, U-1840..U-1845): the
photo is not narrateShot's: showBulletinBoard's first call with a pending photo position saves the root's 0xc8 x 0xa0 rectangle at
(clamp(sx - 0x64, 0, 0x258), clamp(sy - 0x64, 0, 0x1b8)) as `snapshots\accomp<record>.bmp` (saveBmp, 24-bit, rows bottom-up, no
padding), and the board loads every listed event's file into ONE photo Image (loadBmpSurface; FitSurface only grows it, so a missing
file redraws the previous event's photo) and blits it. The shipped install (original\, every instance, the cab) has no `snapshots`
folder, so the original's fopen fails and no pinned frame shows a photo; data1.hdr's directory table lists `Snapshots` (U-1840).
gfx_bulletin.cpp's `showBulletinBoard.capture` writes the file through GameFiles only when that directory exists (dialogs.cpp hands
it the call) and `.photo` reads it back: the capture's bytes equal the ORIGINAL saveBmp's and the decode the ORIGINAL loadBmpSurface's
under Unicorn (SG_EMU_ORACLE=1, port/test/test_gfx_s3_18.py); the file goes through the GameFiles write overlay (the app's --data
directory), and a run over an install holding `snapshots` leaves every install file byte-identical. cov_b_holes18's 5 UNSUPPORTED rows now FAIL with exactly I14's px (HUD
and view residue); the board is modal inside P5, so no H1WAIT checkpoint shows it. cov_r_bio's 2 rows (the bio EditBox, a window-tree
child drawn only inside the bio loop and covered by the editor's next pass) are HELD for S3.19 (U-1844); all 7 rows were
UNSUPPORTED only through framegate's cumulative reason list (U-1845).
Roadmap S3.19, the bio EditBox (2026-10-29, notes log/roadmap/S3/19_notes.md, merge 19_merge.md, U-1846..U-1849): the character
editor's bio EditBox is a window-tree child of the root with its own 0x12c x 0xc8 window surface at (0x1e0, 0x70) (Window::create
with flags 0x2020: no edges, innerSize unchanged). Body family `editbox` (gfx_editbox.cpp) ports EditBox::drawContents (the
EditBox vtable's slot +0x58, called by Window::show and by Window::doLayout after every edit): Window::paint, the bevel passes,
the inset clip, wrapToLines, lineIndexAt, the first visible line kept around the caret, drawTextLines and the caret line;
gfx_text.cpp draws marked-up text on a window target (its surface, clip, the Image's fonts and text object; measureMarkupRun
with wrapTextToWidth and the break tail) and gfx.cpp's Window::refreshFull composes shown children over the root's copy
(Window::blitToTarget). chareditor.cpp's bio entry hands the calls over (show, every key the EditBox takes, a refresh per pass,
close). drawContents STORES the first visible line [+0x57c], a ui-class GameState word the port's bio_entry never writes; the
backend keeps the same rule in its own state; its only reader (pixelToCharPos, from Up / Down and the caret clicks) is not
ported and cov_r_bio never changes it (U-1846). The colours are palette indices: Window::paint gets Palette::nearest(white),
which the port does not store, so the backend paints direct white; text and caret are index 0, drawn black (jgld
Palette::setDefault's table; the palette at the bio is not measured, U-1847). The port's boot never runs the EditBox's static constructor, so the backend reads an unconstructed Image (vtable word 0)
as Image_ctor / EditBox::create leave it; the caret does not blink (U-1848). Evidence: the emulator CALL oracle
(port/test/test_gfx_s3_19.py, SG_EMU_ORACLE=1: the ORIGINAL drawContents under Unicorn with recording callees against the port's
record mode) equal on 300 / 300 varied EditBox states, controls 1 2 3 4 8 RED; a composed frame of a constructed bio state
(controls 5 6 7 9 RED); a cov_r_bio boot with the backend PASS with no unsupported reason left for the bio, and framegate's
cov_r_bio exit / end UNSUPPORTED -> FAIL with S3.18's px (107,138 / 130, HUD and view residue); no reference frame can hold the
box (U-1849). Finding in the original: with 32 wrapped lines wrapToLines' end entry lands on drawContents' return address (its
line table holds 32), so it returns into the text; the bio cannot reach 32 lines.
Roadmap S3.16, wave 7: 2D pixels beyond one level in the view (2026-10-29, notes log/roadmap/S3/16_notes.md, edits
16_edits.py applied by 16_assemble.py on 12_assemble's tree, U-1820..U-1825). Drawing calls the port had dropped now reach an
attached graphics backend: drawLabels / drawMoneyLabels' setDrawTarget + drawCenteredText2 (tick_leaves.inc: the floating text
and money labels, cov_l_camera `-600` 227 px), narrateShot's shadow line and flight arc (setLinePoint / lineTo, shots.cpp),
drawGolferInfoPanel's whole panel (dialogs.cpp, from c3fr.cpp GolferPanel_re in its order: about 100,000 px on 15 rows -> 0),
composeGolferSprite(0x99)'s palette before drawFlyingBall's preview-golfer addSprite (tick_ball.inc; a registered function's
drawing half must be handed over with gfx_drop), and the Window::refreshFull the panel handlers make between drawWindow and
pumpAndRedraw (commands_panels.inc help_window, canAfford; cov_r_palette_hot undo_help 46,200 -> 0; clickSlotPanel /
clickAmenityPanel also drew at the ticker's position). GATE HARNESS: a checkpoint's clock call is its POLL record's position
when the poll rode on a non-tgt call (gfx.checkpoint_ordinal, framegate): status.tgt pointed at H1's g_frameMs read, after the
frame's present (7 rows, 53-2,648 px each -> 0..2; SG_GFX_CHECKPOINT_AT=tgt restores the old mapping). The bottom_panel
residue (26 / 29 / 45 / 50 one-level px) is terrain, not 2D (U-1821).
Roadmap S3.15, the HUD top-bar residue (2026-10-29, notes log/roadmap/S3/15_notes.md, merge 15_merge.md, U-1810..U-1814): I14's
446 FAIL rows with one-level px in hud_title / hud_meters / hud_message / toolbar hold TERRAIN pixels, not 2D ones: split against the
original's root at updateTerrainRender's return (S3.2's exit.bin, 15_terrsplit.py) every differing pixel of sandbox_basic,
cov_k1_river, cov_k3_camera2 and championship already differs in the terrain, and the half-bright frames over them are exact. The
remaining GlRaster residual (U-1392: textured pixels within 0.015..0.08 8-bit units of a dither threshold) was not closed by float32
plane, fixed-point weight, texel or colour models. Instead the terrain can be drawn by the host's own OpenGL: gl::WglSink
(gl_wgl.h, opengl32 loaded at run time onto the original's 16-bit 555 DIB, glreplay.exe's method, which is bit-exact on the c7
passes), selected per LiveTerrain by terrainLive.hostGl(1) / SG_GL_BACKEND=wgl / framegate --host-gl; default GlRaster. Full census with it: PASS
100 -> 612 of 698 (476 FAIL and 36 UNSUPPORTED rows to PASS), 0 rows lose PASS, the GlRaster census equal to I15 row for row, x64 = Win32; what is
left is cov_a_golfer's undrawn golfer info panel (U-1814) and sprite / view content of S3.16's. Controls 8 (565 DIB) and 10 (root not
loaded into the DIB) RED; 9 (top-down DIB) and 11 (texture names unmapped) blind with the reason (U-1811).
Roadmap S3.20, the host-OpenGL startup self-check (2026-10-30, decision G13 final, notes log/roadmap/S3/20_notes.md, merge 20_merge.md,
U-1815..U-1819): GlRaster stays the default terrain renderer; the host's OpenGL (S3.15's gl::WglSink) is opt-in. In the app,
`--gl auto` (or SG_GL_BACKEND=auto) runs a startup self-check and uses the host GL only if it passes, else GlRaster; `--gl host` (= wgl)
takes the host GL without the check. framegate has the same `--gl` (default raster) and columns renderer / renderer_reason. The app logs
its choice (stderr `simgolf_port: terrain renderer ...`, terrain_renderer.json). The check (gl_wgl.h wgl_self_check,
LiveTerrain::choose_renderer) runs the port's own GL setup from the user's install into a WglSink the way the live path does, draws the
19 triangles of the original's recorded pass c7/z2_r1 that reach a 32 x 32 rectangle (port/src/sim/gl_wgl_ref.inc, generated by
log/roadmap/S3/20_make_ref.py) and compares SHA-256 of the rectangle with the hash of the ORIGINAL's pixels there. Host GL equals the
original in 1,024 / 1,024 px; GlRaster differs in 45 (U-1392). Fallbacks, with the reason logged: wrong pinned hash (control 13),
perturbed reference (14), 565 DIB (8), no 16-bit generic pixel format (12). Census: default (GlRaster) 111 PASS / 577 FAIL / 10 NOT_RUN
of 698, equal to S3.9's row for row; opt-in auto 638 / 50 / 10, 0 PASS lost. Why not the default (U-1818): a full host-GL pass takes
33..43 ms against GlRaster's 6..12 ms with workers; app terrain frames median 45 / max 95 ms against 19 / 40. A banded host GL
(20_band/, measurement only) gave the same pixels and median 21 / max 46 ms, but hung inside opengl32 twice (U-1819).
Roadmap S3.21, U-1392 part 1 (2026-10-30, notes log/roadmap/S3/21_notes.md, U-1305..U-1309): GlRaster's terrain residue came from
two stages of Windows' generic GL it did not model, both measured exactly with a feedback-mode probe (log/roadmap/S3/21_glfeed/
glfeed.exe: the pass replayed in GL_FEEDBACK, GLFEED_POINTS=1 one point per vertex): (1) the VERTEX RULE: float32 matrices, the MVP
updated by every modelview operation (not recomputed as P * MV), clip = m3 + x m0 + y m1 + z m2 with every operation float32 (the
translation FIRST), window = round-half-even(f32(ndc * f32(h - 2^-8)) + centre) to 2^-8: all 973,749 vertices of the 268 corpus passes
equal (the double model missed 20,709, the vertices within a float ulp of a 2^-8 tie: S3.16's x bands); (2) CLIPPING: triangles crossing
the view volume are culled on their snapped positions (zero area kept), clipped in clip space (x >= -w, x <= w, y, z; classic
Sutherland-Hodgman order; t = d_in / (d_in - d_out) and attributes in float from the inside vertex) and drawn as the fan of the
polygon. GlRaster's `#tune 28 1` / `#tune 29 1` restore the old model / no clipping, `SG_RASTER_TUNE=<i>=<v>,...` sets knobs for a
process. One-level px of the 271 stream passes 18,234 -> 2,296, of the 237 draw-list chains 10,300 -> 1,605; census sample (5 scenarios,
133 rows) differing px 24,960 -> 1,902 with no row worse and no PASS lost; full census: 21_notes.md section 7. What is left (U-1309) is
the attribute interpolation: incremental 16.16 fixed-point stepping of the 5-bit-scaled colour, truncated, rows walked from the
largest-y vertex (exact on a slow-ramp probe, 480 -> 90 on random Gouraud quads), not yet in GlRaster.
S3.21b (21b_edits.py, opt-in #tune 30): the 16.16 fixed-point colour stepping is exact on untextured triangles (ramp 705 -> 0 px,
Gouraud 481 -> 90) and changes nothing on the textured terrain, so it stays off; U-1309 is the textured (GL_MODULATE) interpolation.
Both part 1 rules are on by default (orchestrator, 2026-10-31: exact stage models, not fitted); SG_RASTER_TUNE=28=1,29=1 restores the
old renderer row for row.
Roadmap S3.24, the overlay line's footprint (2026-10-31, notes log/roadmap/S3/24_notes.md, edits 24_edits.py, U-1826..U-1829;
U-1370 resolved): Windows' generic GL draws an antialiased line per column of the major axis as at most N = floor(w * L / |major|) + 2
pixels, the centres whose minor offset o from the line centre times the tie sign (x-major sign(dx) * sign(dy), y-major sign(dx)) lies
in [-N/2, N/2); S3.8's coverage formula gives the values inside. Measured with glreplay.exe on 16-dither-phase probe sets (the cut
meets N/2 at every width and angle where S3.8's support reached past it) and fitted on exact ties. terrain_overlay.cpp's line rule 2
is the default (overlay.setRule / SG_OVERLAY_RULE: 0 = S3.8's model, 6 = no tie decision, 1 / 3 / 4 / 5 / 7 other tie signs;
control 11 = rule 0); SG_OVERLAY_DUMP=<file> dumps every drawn call with its box before / after. Results: S3.8's probe scenes 108 / 88 /
1 / 18 -> 7 / 12 / 0 / 5 px with none newly wrong; the game's own 9,534 dumped calls through the real GL 879 -> 104 px, none newly
wrong; the 31 overlay-line census rows (22_causes.tsv group 1) 29 PASS with the host-GL terrain (left: cov_a_build brush_465 3 px, the
second-vertex end-cap drop U-1826; cov_a_keys k_dlg_R 1 px value); the default GlRaster census 111 / 577 / 0 / 10 equal in status,
0 PASS lost, 0 rows with more px, 29 rows with fewer (71,871 -> 71,517); segments 205 / 205 and boot 66 / 2 / 2 unchanged; unit tests
33 on x64 and Win32. Open: the end-cap drop at the second vertex of wide lines (U-1826) and a few y-major / non-45-degree ties (U-1827).
Roadmap S3.23, the census split verdict (2026-10-31, decision G18, notes log/roadmap/S3/23_notes.md, edits 23_edits.py): framegate.py
--split runs each scenario a second time with the host-GL terrain (S3.15's WglSink, bit-exact against the original's terrain). A
differing pixel is terrain-origin when it differs under GlRaster and equals the reference under host GL, on rows with terrain on
screen; every other differing pixel is 2D. Split PASS = 2D px 0 in every region and terrain-origin px within G2 over the view area (the
census's G2 denominator, U-1121). Raw columns stay unchanged (raw = f2dff003 on 698 / 698 rows); a host without a 16-bit generic pixel
format reads UNJUDGED, never PASS. Census 70 scenarios: split 638 / 50 / 10 / 0 = the host-GL census row for row; every split FAIL is
2D only; the largest terrain-origin count is 270 px (8 beyond), inside G2. Test seam SG_FRAMEGATE_PLANT=<both|raster>:<region>:<n>
(controls: a 2D pixel planted in hud_title stays FAIL; a terrain-only difference beyond G2 fails as terrain-origin).
port/test/test_framegate_split.py 17 x64 + Win32.
Roadmap S3.26, wave 8's 2D residue (2026-10-31, notes log/roadmap/S3/26_notes.md, merge 26_merge.md, U-1834 settled, U-1950..U-1952):
four of the six cause groups S3.22 found in the host-GL census's 50 FAIL rows are fixed, each from S3.25's recordings of the original.
Zoom box marker (cov_b_inject / inject2 zoom_box, forced_events, end, 15 px each): Widget_readPixel was a not-drawing drop returning
0, so mainLoop P5's `== 0x7fff` test (0x00418b98) never let the port draw g_sprite567b08; family `pixel` now has its body (c3db.cpp
GetPixel_re) and hands the root pixel back through gfx_take_value with a backend (without one the drop still returns 0). Hire dialog
(cov_r_s2_screens hire, 1,397 px): the four employees are addSprite of cellAt(idx + 0x20e, frame mod cols, s) (0x00459777); the port
had passed cell 0. Selected-golfer sprite (golfer_stories season_end, 685 px): f_w_4c2e00[2] = -2 (sim class, the original's value
too) and P4 tests only -1, so the original reads `golfer -2` inside g_sprites5791f8 (element 4's cut size 80 x 80 places it, element
5's handle byte gates it); the port keeps SpriteRef fields in the backend only, so P4's three reads go through gfx_sprite_ref_read
(gfx_pixel.cpp) when a backend is attached (U-1950: the handle byte is answered 1). Animated sprites (cov_k3_camera2 y_inj4 414 px,
cov_b_holes18 holes 2,829 px): not a drawing body: the checkpoint's POLL rode on the frame-head clock call (its pos is a FRAME record's;
S3.25's call log has 153 resetSpriteQueue calls there), and the snapshot rule skipped that frame's own cursor reads, so the port showed
the next frame's spray columns; gfx.poll_position (framegate, gfx.checkpoint_ordinal, test_app_ingame) now takes such a checkpoint at
the head call (pos - 1); 9 of 688 checkpoints have such a POLL (U-1951), SG_GFX_CHECKPOINT_AT=poll16 restores S3.16's rule. All six rows
PASS on host GL. Not fixed: cov_k3_camera y_inj3 (847 px): worldToScreen(0xc7ff, 0xd800) is tile (49, 54), past the grids; its level
read g_tileLevel[2504] lands on byte 0 of g_surfaces5439e0, the vtable 0x004ba2d8 Surface_ctor stores (0xd8 = -40, sy + 215 instead of
+ 15), and the port's GameState holds no vtable words (U-1834 settled; kept as an explained frame divergence on cov_k3_camera y_inj3 (orchestrator, 2026-10-31)).
cov_r_s2_screens course / radial_hover / report rows (65 / 20 / 11 px) are terrain: terrainlive over S3.25's recording gives 20,204 =
20,204 Terrain calls and 32 / 32 states equal, and the original's pass draws a near-black dither over the map's right edge that the
port's draw list does not hold (U-1952). Development switch SG_GFX_SNAPSTATE=1 (gfx.cpp) keeps the GameState with every gfx snapshot
(`$gfx/state_<k>.bin`; log/roadmap/S3/26_snap.py). Controls RED: pixel 2 and 3, showHireEmployeeDialog 3, SG_GFX_CHECKPOINT_AT=poll16.
S3 wave 8 heavy-run lock (2026-10-31): log/roadmap/S3/w8_lock.py take | release | run <step> -- <cmd> | status enforces one S3
heavy run at a time (a build, a pytest file, a census batch or a game boot): `take` writes w8_wait_<step> (epoch, its pid), blocks
until that file is the oldest live wait file, the lock dir w8_heavy.lock is free and >= 16 GB commit charge is free; wait files of a dead
take pid are removed; `release` refuses another step's lock; `run` always releases; `--priority` (lead / orchestrator only) ranks first,
used for I20's quiet window (w8_i20_window.py). A hand-written lock seen by a waiter is logged to w8_breaches.log. It replaced a written
oldest-first rule that workers broke four times in one night (a helper that checked only the lock dir, an unlocked finish step).
S3.22 (W8.3) classified the 50 host-GL FAIL rows into seven cause groups (22_causes.tsv, images local) and removed the unused body
showCharacterEditor.bio (W8.6, gfx.check 137 bodies); S3.25 (W8.4b) recorded the original for groups 2-7 on instance s3_w8 (8 runs SAME
as det_b, port/corpus/w8rec/ and log/p5/terrain/s325_* local, 25_notes.md).
Merged-tree census on 2291a342: raw 123 / 565 / 0 / 10 (0 PASS lost, 576 rows fewer px, 71,871 -> 6,203), split 677 / 11 / 0 / 10; the
11 are group 5 (explained), group 2 (U-1952, 8 rows), U-1826, U-1829.
Roadmap S3.28, lowerEdgeCorner's black-light pass (2026-10-31, notes log/roadmap/S3/28_notes.md, merge 28_merge.md, U-1955..U-1957,
U-1960, U-1961): U-1952's near-black dither in cov_r_s2_screens' map corner is Terrain::lowerEdgeCorner's draw half,
renderTileAtFiveOffsets (Terrain.dll 0x10038900, re/match/terrain_edge.cpp): localRender's transform without a draw list and, when a
neighbour toward the camera is outside the grid or hidden, enableBlackLight1 (0x10003830: light 0 off, light 1 on with zero ambient /
diffuse / specular) and the tile rendered at five offsets, leaving only the global ambient term (0.04 x texel). It is terrain pass kind
5 (terrain_render.h: a0 tile, a1 pivot, a2 angle = lowerEdgeCorner's arguments 0, 2, 3); LiveTerrain::lower_edge_corner hands it to the
backend before the state call (the original draws before Tile::lowerCorner), and it takes no p<j> pass number (the original's pass
copies count kinds 1..4). GlRaster needs no GL_LIGHT1 model while its colours are 0. Recorder log/roadmap/S3/28_gl_rec.js /
28_gl_record.py: entry a3 (lowerEdgeCorner's angle), renderTileAtFiveOffsets as a direct kind-5 entry for injection, kind-5 passes that
draw nothing kept. Recording s328_s2b (cov_r_s2_screens, SAME as det_b): 12 organic passes (angle 0, as every organic lowerEdgeCorner
call of the pinned terrain recordings) and 20 direct calls on the edges of the land (x, y = 12..37) at 0 / 90 / 180 / -90, each edge
tile drawing at exactly two angles. All 32 streams equal the original's line for line; GlRaster meets G2 and the host GL equals the
original's back buffer on every pass. Census (698 rows) against the wave 8 stack: exactly the 8 cov_r_s2_screens rows of group 2 move,
split 677 -> 685 PASS (all 8 PASS), raw 123 -> 126 PASS (rep_histograph, rep_report1, rep_roster; the other five keep 1..4 px of
GlRaster terrain residue, none beyond one level), 0 PASS lost, 0 rows worse. Controls RED: kind 5 skipped (gfx.family.terrain 15: 65
px in the corner box, 20 beyond one level), renderer 15 / 16 / 18 on the streams, 17 on the streams and the host GL (blind in GlRaster
by construction).
Roadmap S3.29, U-1309 (2026-10-31, notes log/roadmap/S3/29_notes.md, U-1965..U-1969): the generic GL's textured interpolation, measured
with probes replayed through glreplay.exe (log/roadmap/S3/29_probes.py; slow ramps, alternating 0 / 255 texels that amplify u's error
255 x, random triangles with permuted vertex order, and boundary probes whose exact value sits on a 5-bit boundary for one dither cell so
the arithmetic decides it; fitted by 29_texfit.py), is in GlRaster behind `#tune 32 1` (default off), in both pixel loops: a textured
triangle whose texture has a GL_LINEAR filter is interpolated in float32 (gradients by the t1..t4 form from the lowest vertex, rows walked
from the largest y with the row start = float32(previous + float32(-dA/dy rows) + float32(dA/dx pixels)), restarted from the exact plane
where the left edge turns at the middle vertex, float32 sums along each span; colour at the 5-bit scale), and an unblended GL_MODULATE
pixel is filtered, modulated and dithered in float32 with texel = byte / 255.0f (GlRaster::TexWalk, walk_final). Exact on 9 probes
(alt_big 64,599 -> 1 is one of the 4 that are not: 1-2 float32 ulps of u or v left, U-1965); all-NEAREST textures take another path
(U-1966); the blend's float form is not determined (976 of 1,115,136 channel values, U-1967). The terrain's remaining residue is mostly
not interpolation (per-vertex lighting, coverage ownership 2e-6 .. 2.4e-3 px from an edge, blending: U-1968): c7's seven passes one-level
173 -> 154 with the rule; full census with the rule raw 123 -> 131 PASS of 698, differing px 6,203 -> 4,221, 0 PASS lost, split verdict
unchanged (677 PASS).
S3 wave 9 (2026-10-31, merge log/roadmap/S3/MERGE_w9.md, stack w9_assemble.py): on 2291a342 + wave 8 + the two I21 fix layers
(w8_fix_i21.py: the pixel family's body count and the hire RESIDUAL after S3.26; w8_fix_i21b.py: test_gl_wgl's sandbox_00 pin 125 -> 9 px
and the self-check's GlRaster control, which turns S3.21's rules off with glWgl.rasterCheck(1) since the default GlRaster now passes the
check) S3.28 and S3.29 give census raw 126 / 562 / 0 / 10 and split 685 / 3 / 0 / 10 (0 PASS lost, 0 rows worse); the 3 split FAIL rows
left are cov_k3_camera y_inj3 (U-1834, explained), cov_a_build brush_465 (U-1826) and cov_a_keys k_dlg_R (U-1829). w8_lock.py: a
`--priority` step that made the last hold yields one turn (alternation), one pytest file per hold, and `run` starts its command with
CreateProcess (wrap ~/bin/py in `bash -c`; test-slot the same: `test-slot bash -c '...'`).
S3 wave 10 (2026-11-02/03, merge log/roadmap/S3/MERGE_w10.md + log/roadmap/merges/I24.md, layers 31 / 30 / 32 applied in that order by
their assemblers, U-1970..U-1983): S3.30 adds trace DELIVER kind 7 `call` (pos, addr, argc, a0, a1), written by trace_rec.js, applied by
TraceInput in trace order and re-recorded by the live core (`LiveWriteLog::called`), so golfer_events' injected landmark call replays
(boot PASS 2522 / 2522); S3.31 is overlay line rule 9 (float32 line ends and the second-vertex holes, U-1826; brush_465 split PASS);
S3.32 makes Surface_ctor's vtable stores for the boot's 147 static Surface objects, the word read from the user's exe through
f_imm_473ab9 (U-1834 option b, default ON, control tick.frame 14; y_inj3 split PASS). Census raw 127 / 571 / 0 / 0, split 697 / 1 / 0 / 0:
the one split FAIL is cov_a_keys k_dlg_R's 1 px (held, G27 = a). tests/test_trace_format_s1.py fails on any tree without S3.30's
trace_format.py, since the shared corpus holds the kind-7 golfer_events.trace.
Section S4, audio engine model (2026-10-24, lead notes log/roadmap/S4/STATUS.md, steps re/roadmap/S4_steps.tsv, function map
re/roadmap/S4_functions.tsv: 744 sound.dll rows, 573 C3/C4 + 31 C2 ported in S4.2..S4.9, 71 CRT and 69 dead not ported).
The path-1 CSVs keep only an FNV-1a hash of each region's raw bytes, and every one of the 19,785 regions of the 980 sound.dll
keys is a fixture block holding the run's own addresses (heap blocks, list links, vtable pointers of the relocated DLL), none
recorded, so the test_bridge.py replay cannot reproduce them. Decision G1 (Mariano, 2026-10-24): the oracle is the ORIGINAL
function in Unicorn over the user's sound.dll on its replayed fixture, port == emulated original byte for byte, the emulated
original tied to the CSV on every address-independent field; default runs check golden digests. All 205 segment start
images hold g_soundReady 0x83afc4 = 0: every segment runs the exe's device-less sound path.
Step S4.1, sound.dll replay (2026-10-24, re/analysis/data/sound_replay.md, notes log/roadmap/S4/1_notes.md, U-1400..U-1409):
the 980 sound.dll path-1 keys replay without the game. re/tools/fixvm/fixvm.js (Node) runs the fixture JS and the real
stand-in helpers over a mock Frida API (frida.js, mem.js, syslibs.js); port/test/sound_emu.py runs the ORIGINAL sound.dll in
Unicorn, relocated (5,769 HIGHLOW entries), DllMain first, traps for callbacks / fake DLL exports / Interceptor redirects
served by the VM over a synchronous page-syncing link; KeyRun repeats diff_hook.js per key (regions, arm built after the
fixture, per-vector resolve, both arms). re/tools/sound_census.py -> re/roadmap/S4_census.tsv, three layouts (a field equal in
A, B and C is position-independent): 181 EXACT, 785 equal on every position-independent field, 5 DIFF (__ftol_snd: Unicorn
sets no x87 exception flag and stores 0 for fistp of an empty st(0); 42 rows per key match with the flag byte searched),
2 distinct-count differences (U-1403), 5 partial (RaiseException SEH, timeGetTime), 2 fixtures that wait for the live engine.
Controls 10 RED (fixture byte flipped, wrong relocation base, callee stub +1), 2 blind. Facts: the live TLS index of
sound.dll's CRT was 27; the CSVs print doubles shortest (2026-10-07) or as 17 truncated digits (2026-10-14);
KeyRun(second_arm=...) is the port hook. Tests: tests/test_fixvm.py (16, static marker, ~35 s).
Step S4.2, the audio core (2026-10-24, module audio_core, notes log/roadmap/S4/2_notes.md, U-1410..U-1417): sound.dll's
port runs over the context's 32-bit space with the image at a caller-chosen base. port/include/portsim/sim/soundstate.h
(GENERATED by re/tools/soundstate_gen.py from re/names/sound_*_globals.tsv, hooks.csv and the user's section table):
portsim::SoundState = .data (0x54098 B, members at their offsets, static_asserts), snd_rva constants (.rdata vtables and
tables), kSoundFunctions (rva -> hooks.csv name of all 744 rows). portsim/sim/audio.h: audio_attach (the harness maps the
image, or map_image maps the user's sound.dll from GameFiles after checking .data size and an .rdata hash), snd_obj /
snd_va / ld / st, and the DISPATCHER every sound.dll port function calls through: snd_call (rva, thunk or body),
snd_vcall (virtual slot), snd_call_va, snd_import (IAT slot). Order: a VA the host claims (a fixture's Interceptor.replace)
-> host; inside the image -> follow E9 thunks -> the registered port function of that rva; inside but unported -> host as
FALLBACK (development only, counted; gates want 0); outside -> host as EXTERNAL. Hosts: the C ABI callback sg_snd_set_host
(live), or the in-library script host (a recorded transcript from "$audio/script", requests checked slot by slot, replies and
host memory writes replayed), configured through sg_call "audio_core.setup" etc., so the Win32 runner needed no change.
audio_sink.h: AudioSink (import / com commands by DLL!name from the image's import directory, interface objects whose
vtables route back), RecordingSink. port/test/sound_replay.py: ORACLE (SG_EMU_ORACLE=1) runs the port as KeyRun's second
arm on the emulator's memory in layouts A / B / C, the emulator serving the port's host calls, and ties the emulated
original to the CSV with S4.1's census compare(); DEFAULT replays bundles (port/corpus/audio, local, 17 MB) against golden
digests (port/test/golden_audio_core.json), x64 and Win32; SG_S4_MIRROR=<dir> points it at a mirror build's x64 / w32 DLLs.
First family: the 60 S4.2 rows (lists, nodes, queues, helpers; 64 keys + 4 derived keys of the emu-only rows: PtrList_pushFront
and PtrList_insert borrow their instruction-identical twins' fixtures, stubReturnBB the (this, uint32) key setField6cReturn0's):
1,219 vectors (1,159 of the 64 keys) equal to the emulated original in all three layouts, 0 fallbacks, tiers EXACT 8 /
EQUAL_PI 55 / DISTINCT_DIFF 1 / EMU_ONLY 4; mode 1 of all 60 functions RED. re/tools/snd_hook2port.py converts hook sources
(accessor roles found by body shape, thunk / slot / IAT calls to the dispatcher, VAs in comments to RVAs, macro-made detours
expanded, adapters and FuncDef rows): its UNEDITED output for the 57 hook-sourced S4.2 functions compiles at /W4 and passes all
64 keys from the bundles (`--module audio_conv`, log/roadmap/S4/2_conv_replay.log). Segment gate 205/205 x64 and Win32,
digests equal to a HEAD build; boot gate unchanged.
S4 wave 2 (2026-10-24/25, steps S4.2b..S4.11, notes log/roadmap/S4/<k>_notes.md, U-1418..U-1499 and U-1540..U-1549): sound.dll's
engine is ported in eleven modules over the audio core: audio_mixer (the Voice_mix* leaves, renderers and streamRender at the
original's x87 word, Win32 real x87 and an x64 software 80-bit path that equals it on every vector and 60,000 probe operations),
audio_chan (channels, voices, Voice::update), audio_seq (sequencer, MIDI, tracks), audio_sound (Sound / MultiSound /
SoundGroup, create_sound, Engine_playSound), audio_ds (DirectSound as AudioSink commands, whose logs equal the import stand-in's)
and audio_dev (devices, Ctrl, command queues, SoundTimer, the GetProcAddress exports), audio_wave (wave-out / wave-in devices,
mmio RIFF readers over GameFiles, stream readers, decode rings), audio_soundfile (loadSoundFile, the Sound vtable's slot 4: no
user file uses its chunk format, so every path but the unknown-tag exit is judged on synthetic banks, U-1492), audio_codec (the
LPC-10 speech codec), audio_crt (the CRT helpers, so 0 development fallbacks remain) and audio_exe (the exe's Snd layer,
playSound / startSoundSlot / initSound / loadSoundTable on the port's engine). S4.2b consolidated the core: ONE stack mechanism
(SndFrame: a function's locals at the original's offsets below its entry esp), the virtual x87 unit as the core FPU model,
unclaimed `jmp [iat]` stubs routed as imports, SndResidue for words a caller pushes that its callee never reads, `snd.<name>`
registration (an unnamed function keeps Ghidra's FUN_<va> until it is named), and one front end, port/test/sound_replay.py
`--step S4.k`. Oracle (G1): 1,087 keys / 16,859 vectors equal to the emulated original with 0 fallbacks, and the twelve golden
files pinned. Segments run with the audio module attached and write an AudioSink request stream (205 / 205, 4,964 requests, every
digest unchanged). All 205 start images hold g_soundReady 0, so the requests are recorded at the exe's sound layer; a device-present
fixture run drives the engine end to end (181 requests, 0 fallbacks). Test-only switch `audio_exe.stubRequests` stands in for the
audio calls the UI oracles stub; sg_state_free (audio_release -> audio_exe_release) drops audio_exe's per-context state, which a
context allocated at a freed context's address used to inherit (I8: test_buyland's Win32 digests varied run to run). Unicorn rules folded into the oracle: the x87 exception flags are not set (searched), and a
signaling NaN copied through fld / fst is quieted by the CPU, not by Unicorn (U-1451, 17 + 1 vectors forgiven on the quiet bit
only). U-1421: the codec's caller control word is not measured. Three live runs with an audio device never entered the codec:
all 319 install sounds are PCM and openMmioSound / loadWaveResource are the network voice path (D6). The port keeps the codec at
0x027f. Findings in the original: renderChannelStreamed hangs on an underrun at the read limit; create_sound maps `.aif` to the
default; the MIDI loaders never check `MTrk`; loadSoundFile never leaves its chunk loop on a truncated file (U-1490); both
create_device exports report success after a failed allocation; Midi_Device::shutdown skips every second track.
Phase 7 defect decisions, section S6 of the roadmap orchestrator (2026-10-24, notes log/roadmap/S6/, U-1550..U-1597): `re/port/defect_decisions.tsv`
holds every defect of model_defects.md (#1-#52, some split by arm) and every non-PASS row of divergences.tsv (grouped by cause) with its class
(hang / fault / overrun-silent / wrong-value / dead-state / port-gap / harmless), six effect columns (save / balance / sim / frame / hang / fault),
a recommendation and the switch: 74 rows, 10 `fix`, 26 `ask`, 38 `keep` (generator `log/roadmap/S6/merge_decisions.py --write`; schema
log/roadmap/S6/SCHEMA.md). The classification came in two passes: S6.1 / S6.2 from the model notes, then S6.6 re-read the cited instructions and ran
the ORIGINAL in Unicorn on constructed states (log/roadmap/S6/6_probes/, one probe per defect, each hang with a completing control at the same
budget). Corrections it made: spawnWalker with 64 live walkers completes (writes the record over g_editQueueX, returns 0x40) instead of hanging;
stampCourseTiles' scan only reads and the original already refuses a full table; showMembershipRoster copies a type name, not the 0x230-byte slot;
D22's register test is equivalent; rebuildHeightfield's relax pass and buildGolferBioText's picks cannot loop on the shipped tables; the movie id
list of serviceMovie has no writer; and one new hang, loadGolfPro with g_difficulty 5 or 6 never ends even with every pro record filled (D42c, `ask`).
Profiles (port/test/profiles.py, tests test_profiles.py 19): `original` = every sg_defect switch 1 and is the oracle (equal to no profile); `fixed`
= the members of re/port/defect_profiles.tsv, and only decision D5's class: a hang or fault whose fix changes no save, balance value, simulation state
or frame on a run the original completes. Six switches, each default 1, each judged by the probe state that hangs or faults in the original (the
port returns cleanly at 0) and by a completing control whose result is identical at 1 and 0, plus a wrong body that reads RED:
placeHabitat.clusterLoopBound (slot 127 with 0xff after the table is treated as taken), spawnObject.cellSearchBound (return before the animal record
is claimed), loadGolfPro.divideAndRejectGuard (a zero divisor or an empty roster ends the draw at pick 0: U-1590, no original run exists there),
placeAnimal.zeroFrameCount, replaceInText.tailLimit (a tail of 0x200 or more is refused: the original overwrites its return address from there;
U-1597 truncation instead), generateRandomCourse.searchBound (the type-0x11 search skips the feature, the clubhouse search takes the window centre:
U-1596, a port choice). Tests port/test/test_defects_golfers.py (25) and test_defects_gen.py (13). The 26 `ask` rows (every fix there changes a
save, balance, sim state or a frame) are in log/roadmap/S6/ASK_LIST.md for Mariano: D1 D5 D6 D10 D11 D12 D17-D21 D23 D24 D26 D32 D37-D39 D42c D44
D47b D48a D48b D49a D52a D52b3. Findings in the original: loadGolfPro hangs for difficulty 5 and 6; replaceInText derails the return address from a
tail of 0x200 bytes (0x204 returns to the tail's own bytes); spawnObject's cell search draws about 250,000 times before the budget ends when all 16
cells are taken; the five existing switch rows keep their evidence, chooseShotTarget.strictFlagSticky still has none (U-1012).
S6 wave 2 (2026-10-24, notes log/roadmap/S6/7_notes.md and 8_notes.md, U-1591, U-1592, U-1598, U-1599): Mariano's answer G3 kept the 24 `ask` rows of groups
overruns, wrong values and course / terrain / money at `keep` and turned the two hangs into `fix`, so re/port/defect_decisions.tsv reads 74 rows,
12 fix / 62 keep, with a `decision` column (D5 fix, G3 fix, G3 keep) and profile `fixed` has 8 switches (re/port/defect_profiles.tsv). D42c:
loadGolfPro with g_difficulty 5 or 6 never ends in the original (4 - d < 0 makes the acceptance bound fall without end); switch
loadGolfPro.negativeWindowBound (own switch, tests port/test/test_defects_golfers_w2.py, 45): the port's old behaviour at those difficulties was an
endless spin like the original's, now switch 1 raises NoReturn once the falling bound is below every value the roster can give and switch 0 ends the draw
with pick 0; where the original completes the check never fires and 1 and 0 agree, the 11 of 36 synthetic states in which the original hangs in Unicorn are exactly
the ones the port gives up on, and the golfers vectors are 367/367 GREEN at 1 and 0 (U-1591 the cutoff argument, U-1592 the emulator stops at loadThemeFile's
file reads on the states where the original passes). D52b3: the feature-site searches gated by evalPlacementArea hang the original at location 4 (2 x 2 site,
0x0047172e..0x0047176f) and at locations 5, 6, 8, 12, 13, 15 (5 x 5 site, 0x004718a4..0x004718e7) when no reachable cell is accepted (300 M instructions
exhausted at about 921,000 iterations on the port's own images); switch generateRandomCourse.placementBound (wave 1's searchBound keeps D52b1 / D52b2),
tests port/test/test_defects_gen_w2.py (30; controls for all 8 locations equal the original byte for byte at 1 and 0, location 3 on the same image
completes): at 0 location 4 stamps no object and locations 5..15 skip the whole feature (U-1598, U-1599, port choices). The opt-in emulator oracle for
terrain_gen (115 tests) passes. The old S6.7 note that wave 1's loadGolfPro guard raised NoReturn at d = 5 / 6 was wrong: the port spun.
Roadmap S1, loose ends of phases 3 and 4 (2026-10-24, log/roadmap/S1, plan re/roadmap/S1_steps.tsv, five step workers on claude2 and an
integration check): U-1067 resolved: trace_format.validate rejects a first-generation input MSG (WM_KEYDOWN/UP, SYSKEY, mouse buttons) with
no earlier harness DELIVER (WM_CHAR and WM_MOUSEMOVE are derived and exempt; a trace slice is exempt up to its second FRAME, U-1151);
266 of 266 stored traces validate. U-1060 resolved: the four header read sites (loadGame, loadCourse) and the two writers are pinned by
port/test/test_sve_header_readers.py and none reads past the NUL, so the leftover g_textBuffer residue is dead data. U-1012 resolved:
port/test/test_shots_strict.py builds GameStates (42 of 700 seeds) where the original keeps chooseShotTarget's strict flag (frame word
[esp+0xb4], one store at 0x00423b0e, read at 0x00423b20) after a later landing fails the test; the port with the switch ON equals the
original in Unicorn (eax and the whole image), OFF is RED 8 / 8 (SG_EMU_ORACLE=1). U-1066: cov_r_theme_cancel (the Cancel after a row
press in the same visit; showThemePackSelect control 2 RED) and cov_r_palette_hot (Undo hotspot 0x15 and wrench tab 0x16) are pinned;
the palette SpriteRefs 84 / 88 are read only by showInfoCursorPanel's drawing, so loadInterfaceArt control 3 reads GREEN by hash and
test_s1_3_scenarios.py compares all 96 cut fields with the original's images (U-1170); cov_r_tutorial runs showTutorialText pages 1..29 and
30 (the page word 0x00567a18 and bit 15 of g_flags are harness writes: no instruction stores a nonzero page, U-1182), boot 1,195 / 1,195,
controls 1..4 RED. U-1051: cov_r_flic_input posts a key in courseActionMenu's wait; the original dispatches it from the 15th pumpInput483bd0
of loadFlic (loadWorldAssets makes no clock read); the port takes it with PumpKind::Load (U-1181, U-1183 open). U-1032: the character editor's
bio entry is ported over the EditBox family (chareditor.cpp: create, setText, resizeBuffer, insertChar, onKey without Up / Down, resetFields,
releaseState); the original routes keys to a focused EditBox through the window tree, so a key it takes leaves a MSG record with no HANDLER
record and tick.cpp parse_trace makes those RootHandler commands 12 / 13 (none exist in the 64 older traces); the bio loop makes no clock call,
so cov_r_bio posts its keys from the scenario agent's own doAction at leaveModal483d80 (DELIVER logged); oracle 7 x 300 EditBox cases and 120
editor cases (212 bio entries) equal the original, golden_chareditor.json re-pinned for cases 43, 61, 90 after the oracle was GREEN. New
scenarios register through the hook at the end of re/tools/scenario.py (re/tools/scenario_s1_*.py). Mirror run with every patch: boot gate 63
PASS / 2 KNOWN / 1 NO_FRAMES (the three rows that differ from the S0 baseline are cov_r_bio, cov_r_flic_input, cov_r_tutorial), segments 205 / 205
x64 and Win32 with every digest equal. Findings in the original: Window::key / dispatchAccel feed an EditBox before any root handler; the
bio loop never reads the clock; the page word has no nonzero store. Open: U-1170..U-1172, U-1180..U-1183, U-1190..U-1195, U-1151, U-1160..U-1162.
S8 video, skip path (2026-10-24, D2: no decoder this round; re/analysis/video/MOVIES.md, movie_runs.md, skip_path.md, re/port/movies.tsv,
flics_census.tsv): the game plays exactly two Bink movies, both through playAudioFile 0x0043cce0 -> BinkPlayer::play 0x00487090 (its only caller): the
intro SMSG_introfinal.bik from showTitleScreen 0x0043cd97 (path 0x004c8a0c) and the closing SMSG_closingfinal.bik from mainLoop 0x0040faa4 (path
0x004c6bb4); jgld.dll, Terrain.dll and sound.dll import nothing from binkw32. No FLIC is a cutscene: all 1,893 .flc files are loadFlic sprite animations
(S3); the serviceMovie family (0x0049c8e0) is an unused RIFF/mmio player (table 0x00840698 never written). Measured on the original with the intro ON
(instance s8_2, three runs per condition identical): both play to the clip end (825 and 589 frames), binkw32 paces with QueryPerformanceCounter (54,945
main-thread calls for the intro) and never reads timeGetTime, so the movies add no virtual T and no scenario poll runs inside them; against the skipped
intro the only run-stable GameState word that differs is g_ui83ab9c 0x0083ab9c (0 -> 1, written by BinkPlayer::play, cleared by the root-window input
handlers); g_seedTime, g_rng and every sim object are equal. Port: module `video` (port/src/sim/video.cpp, portsim/sim/video.h) ports playAudioFile and
BinkPlayer::play / setTarget / close behind a MovieHost; the default host answers "unavailable", i.e. the original's own BinkOpen-NULL path (deadline word
set and cleared, g_bink target set, one pumpInput483bd0, g_ui83ab9c untouched), which is what the pinned corpus recorded; the intro call is made from the
boot (tick_boot.inc), the closing call from the QUIT block (playAudioFile is no longer a drop). binkw32's QPC records never enter a clock list
(parse_trace keeps timeGetTime values and reads are site-keyed), so an intro-ON trace needs nothing else. Evidence: emulator oracle 23 / 23 cases, 12 / 12
branches both ways; 5 negative controls RED; new intro-ON boot scenario cov_r_s8_play (re/tools/s8_boot_capture.py) PASS 216 / 216 and to the exit with the
quit files equal; boot rows 61 / 61 unchanged, segments 205 / 205 x64 = Win32 with 0 digests changed. U-1700..U-1723 (input during a playing movie is
modelled not measured; the drawing around a movie and the sound argument are not ported).
Section S5, text without GDI (2026-10-24, roadmap orchestrator; steps re/roadmap/S5_steps.tsv, notes log/roadmap/S5/<k>_notes.md,
report re/analysis/data/text_freetype.md, U-1500..U-1532): a FreeType TextMetrics provider (module fttext) beside the GDI one, judged
against it. FreeType 2.14.3 core (no zlib / png / bzip2 / brotli / harfbuzz) comes from vcpkg in C:\Users\maria\vcpkg\installed
(decisions D4 and G4; license FTL, its credit line goes into the documentation of any binary that links it) as an OPTIONAL CMake
dependency: `SG_FREETYPE_ROOT` (or `-DPORTSIM_FREETYPE_ROOT`) at configure links it and copies freetype.dll beside the DLLs; without
it module fttext throws MissingResource{"FreeType"} and nothing else changes. gditext was split into MarkupTextMetrics (the
original's Text_put / drawMarkupRun / measureTextWidth chain) and GdiTextMetrics (the GDI primitives; every output row-identical to
before); FtTextMetrics shares the chain. FACES (measured with GetFontData hashes on LIVING-PC): GDI resolves lfFaceName by the
family name, else by the full name of a face alone in its family, else to Arial, so the game's `Arial Bold` 9 / 10 and `Times New
Roman Bold` 18 are drawn with arial.ttf (regular) and `Comic Sans MS Bold` italic 100 with ariali.ttf; Manual SSi (Bold) and Klepto
ITC TT are the install's manu3_.ttf / klepto__.ttf. A host without Windows fonts must supply arial.ttf, ariali.ttf and times.ttf
($SG_FONT_DIR). WIDTHS: ppem = size, interpreter v35, the five FontRef words from VDMX else OS/2 win metrics, advances = hdmx at the
ppem, else linear rounded when head flags bit 4 is clear or LTSH allows, else v35 hinted (3,136 / 3,136 probe cases); FreeType equals
GDI and the recordings on every printable row (11,459 widths, 9,079 Text_put, 10,776 drawMarkupRun, 15 / 15 metric words, x64 =
Win32); only lone 0x0a rows differ, which the original itself measures differently per process (U-0960). PIXELS: the original's labels
and GDI's are ClearType subpixel on the 16-bit DIB (lfQuality 0 follows the host's ClearType setting: on, contrast 1200, RGB here).
Module textlab + port/test/labels.py hold a label corpus (6,540 (FontRef, text) pairs from the widths recordings, 15,960 jobs, plus the
calls of the six gfx checkpoints that pass at HEAD; GDI reference 0 px against itself and against the original's frames). The
FreeType raster (fttext_raster.{cpp,h}) reproduces GDI ClearType's blend exactly (per channel in the 5-bit space through gamma
contrast / 1000 tables, 0 / 7,168) and its 6-sample box filter (exact on Arial), but not GDI's ClearType x hinting (U-1520):
8,736,593 corpus px differ (about 70 % of GDI's ink; Klepto 0.26-0.29, Arial 0.60, Manual SSi 0.61-0.81) and the six checkpoints
differ by 1,735 to 15,347 px each, all inside label boxes. The simulation reads text only through widths, so with
`SG_TEXT_METRICS=freetype` (segments.py / boot.py / integrator.py; default gdi) the segment gate stays 205 / 205 and the boot gate 58 /
2 / 1 on x64 and Win32. The port's default stays GDI; the report's last section is Mariano's decision (recommendation: keep GDI on
Windows, FreeType for hosts without GDI, judged by these numbers). Process lessons: on claude2 `cmake` is not an allowed command
(`~/bin/py` is), so children run every configure and build from a Python script; two S5.3 workers stalled on unanswered permission
prompts before that rule was in their brief.
Roadmap S7, the Windows host, wave 1 (2026-10-24/25, log/roadmap/S7, plan re/roadmap/S7_steps.tsv, U-1600..U-1664): `port/app` builds
`simgolf_port.exe` (port/CMakeLists.txt add_subdirectory(app)), which plays from the user's install (`--install <dir>`, saves under its
own `--data` dir, never in the install; no original binary loaded). LIVE CORE (module `live`, port/src/sim/live.cpp, live.h; tick.cpp
asks a LiveHost through tk::LiveIo, inert in trace runs): the frame loop takes the frame-head clock, every clock read, pump batches,
cursor reads and GetAsyncKeyState from a host on demand and records every answer as a schema v1 trace plus framehash.bin; a host
that answers from a corpus trace equals sg_run_boot in 68 of 70 boot scenarios plus 2 NO_FRAMES, x64 = Win32, 0 replay
differences; idle passes of unpaced modal loops (title menu hover) are recorded so the replay takes the same passes (U-1601);
S1's world-load pump and focus-child keys are handled (U-1604). INPUT (port/app/input_map.*): Windows messages to root-handler
commands, cursor and async-key answers, 0 mismatches over all 26,861 MSG records of the 67 corpus traces. APP (host.*, window.*,
present.*, pacing.*, saves.*): sim thread above normal priority, frame wait in real time (355 periods 87..100 ms, median 93; 125..135
with g_flags bit 17), the gfx back buffer presented at every frame wait and modal pump (title menu 0 px against the original's
sandbox_basic `menu` checkpoint at 96, 144 and 240 dpi), idle on the title menu 3.1 % of one core, every session recorded and
replayed through sg_run_boot equal in every record (422 / 422). Tests: port/test/test_live.py (32 x64, 32 Win32), test_app_live.py
13, test_app_shell.py 18, test_app_input.py 16. Open: U-1610..U-1619 (double click, wheel, Alt, modifiers, chars above 0x7f
unmeasured), U-1620..U-1626, U-1650 (bio EditBox keys from the real keyboard), U-1660, U-1662, U-1663.
Roadmap S7.4, app recordings in the boot gate (2026-10-25, notes log/roadmap/S7/4_notes.md, U-1630..U-1637; U-1611 /
U-1612 resolved):
- `port/test/app_replay.py record <hole|saveload|report>` drives the app with posted messages only. The app runs with
  `--cursor message --keys message --clock-origin 0x100000`, and the session goes into port/corpus/app/<name>/
  (local), together with posted.json and the app's new dispatch log (input_dispatch.tsv, dispatches.tsv, body_calls.tsv).
- `app_replay.py gate [--backend win32]` runs sg_run_boot over each recording against its own framehash.bin.
  hole 340, saveload 266 and report 205 records PASS on x64 and Win32 (port/test/test_app_replay.py, 14 tests).
- The recordings hold:
  - Shift / Ctrl clicks, a double click, the wheel, Alt+X and Shift+A;
  - one hole built and opened;
  - a save the `S` key wrote into the app's overlay, loaded back through the browser (g_date returns to the save's value);
  - the F1..F4 report screens (modal).
- CROSS-CHECK (log/roadmap/S7/4_xcheck.py, instance s7_4): the app's raw input was posted into the original at the same
  pump calls, by frame-head count:
  - P6 = the `call pumpInput483bd0` at 0x004199e0;
  - H1 = the `call pumpMessages` at 0x0041c0cd;
  - the cursor reads were answered from the app's CURSOR records;
  - the virtual keys were set as the messages were posted.
  Result: all 340 records equal the original's in every sim object but g_frameMs, which stores a timeGetTime value
  (U-1637). All 59 root-handler calls are identical in handler, arguments and frame.
- The cross-check found three things, fixed:
  - the translator dropped uiKeyEvent's modifier bits;
  - the translator dropped the double click's rootWin_onPress1b;
  - the host let H1 take several messages or a key. H1 takes ONE mouse-range message: pumpMessages drains WM_MOUSEMOVE,
    then display slot +0x13c (jgld 0x1006b7e0) makes one PeekMessage(0x200..0x20a). P6 loops PeekMessage(0, 0) until
    the queue is empty.
- Control: the tee click moved by 64 px in the original's input diverges at record 185, the head right after that click.
Roadmap S7.5, the in-game frame in the app (2026-10-25, notes log/roadmap/S7/5_notes.md, U-1640..U-1643): simgolf_port.exe now
attaches a LiveTerrain (`--terrain live`, the default; `headless` = tick.cpp's stand-in) after gfx.attach, as framegate does, so
the course is drawn into the gfx root surface and presented with the 2D bodies; waits.tsv carries the cumulative terrain passes /
pixels. `--live-trace <scenario>` runs the live core over a corpus trace with a LiveTracePresenter (presents where a session
makes them) and gfx.snapshotAt at `--snapshots`; `--negctl reg:<name>=<mode>` sets any port negctl switch. Gate
(port/test/test_app_ingame.py, 12 passed + 1 xfail): an in-game session driven by posted messages (Monterey, seven camera moves,
four zoom keys, a hole built) has every present (84) equal to the replay's back buffer with LiveTerrain at the same recorded
position, and its view differs from a headless replay in 358,874 px; over cov_a_keys, cov_k1_routing, sandbox_basic and
course_season every checkpoint (49, 46 in game) produced on the app's live path equals framegate's frame in every pixel and the
present that showed it, and the three in-game rows framegate passes (k_worldmap, k_dlg_L, routing) pass; 368 in-game frames hold
87..98 ms (median 93). Open: frames with two terrain passes (zoom) work 144..186 ms (U-1640); HUD panels, toolbar
and sprites are not drawn at this base (U-1641). Controls RED: terrain pass skipped in the app and on the live path, pacing off,
headless terrain. S7.6's test_app_live replays now attach LiveTerrain (5_test_app_live.patch).
Roadmap S7.8, the app's present point and a long session's memory (2026-10-25, notes log/roadmap/S7/8_notes.md, merge 8_merge.md,
U-1670..U-1673; U-1642 / U-1643 resolved): the original's back buffer reaches its window only through jgld Display::present (vtable
slot +0xa4), which the exe calls once, at the end of drawTooltip 0x0047cdb0; Window::refreshFull reaches it through refreshRect (H1
0x0041e29b in an ordinary frame), the jgld Listener's paint / mouse-move slots and the tooltip through the other callers. P6's
[[0x0083ad50]+0x9c] is Display::setScale(1, 1, 1), not a present (mainLoop_structure.md row P6 corrected). tick.cpp calls
LiveIo::present_point after the Window::refreshFull drop of a live run, and the app presents there ('r'): 0 frame-wait presents over
four traces and the in-game session. U-1643's terrain log was small after the world load (25,404 of 25,671 calls in it over
course_season_6y); the growth was the live core's in-memory records (3.3 KB per frame). LiveOptions::trace_file spills the recording
to disk every 4,096 events (byte-identical to the in-memory one) and on_record hands each frame record to the app, which writes
framehash.bin as it goes: over course_season_6y's 51,092 records private bytes stay flat after frame 2,000 and framehash.bin equals
the original's capture byte for byte. `--in-memory` restores the old way.
Roadmap S7.9, S7 waves 1-3 merged as one (2026-10-26, notes log/roadmap/S7/9_notes.md, merge 9_merge.md / 9_apply.py, U-1680..U-1685):
rebased on 78ffb06d with I7's three refusals fixed. (1) cov_a_keys no longer takes the live core's stuck delivery (U-1680); the test
now runs ui_tour / ui_tour_v (4 inflated dispatches each) and judges the call through TraceInput's count of deliveries at another cursor
count, which must equal the live run's inflated count: live.recorder control 10 (the CURSOR record left out) is RED there and blind in
every frame record and drawing call. (2) tick.cpp's HeadlessTerrain kept its 16 MB arena per freed context in a process-wide map keyed
by the Ctx address, so one Win32 runner grew up to 48 MB per equivalence scenario and died near 2 GB: ~sg_ctx now calls
release_headless_terrain (one runner stays at 4-5 MB over 8 scenarios, 52 -> 117 MB without it), and test_equivalence_all runs one
process per scenario (live.py run_parallel). (3) port/CMakeLists.txt adds the app before S5's FreeType block, which copies freetype.dll
beside simgolf_port (the x64 app had loaded a JDK's from PATH). Since S2.7 the boot's loading screens and the module-drawn modal screens
refresh outside TkBase::call, so the app presented none of them: the present point is now a refresh hook the gfx backend calls at the end
of every Window::refreshFull body (gfx_set_refresh_hook, set by the live core for its run only; U-1670 resolved); the boot's title and
first loading screen share one recording position, so the app tests judge the last present at each position (U-1685). Gates: port suite x64 75 / 75 files, Win32 the known 8 files / 20 failures, test_equivalence_all x64 + Win32,
live equivalence 68 PASS + 2 NO_FRAMES x64 = Win32, app replay 3 / 3, segments 205 / 205 and boot 66 / 2 / 2 on both backends with 0 rows
differing from 78ffb06d, repo static 614 / 2.
Roadmap S7.10, audio out (2026-10-29, notes log/roadmap/S7/10_notes.md, U-1686..U-1692): `simgolf_port --audio on|off|model|capture`
(default on since S7.11) keeps the sim's device-less sound path (GameState unchanged: --live-trace sandbox_basic trace.trace and
framehash.bin equal with and without audio) and copies every request of audio_exe (Req depth 0, the new
`audio_exe_set_observer`) to a SHADOW context on an audio thread (port/app/audio_out.*): the exe's initialised data plus the
device-holder constructors (`audio_exe_construct_holders`), gameMain's initSound(1, 0x8a) before the first non-CRT request,
shutdownSoundSystem at the end, the timer client called at its 30 ms period on that thread. Its sink (port/app/ds_sink.*)
answers through S4's DeviceSink model (`DeviceModel`, `make_device_model`, `audio_exe_device_fixture`) and drives real
dsound.dll buffers (DSound) or a software capture (Capture). Gate (a): audio_stream.device_run's sequence through the app
sink gives $device 4b6ed61a x64 and Win32 (`simgolf_audio_check device`). Gate (c): a scripted title session's capture
against the install's WAVs (port/test/test_app_audio.py): clicks at volume / 127 and hard pan, static buffers at unity gain
then faded; controls unlock-skip and volume-ignored RED, pan / frequency equivalent (the engine sends no SetPan, SetFrequency
only at 22050 Hz). Finding: S4's model answers SetWindowLongA 0, so SoundWindowObj_subclass never kept the window and every
completion message went to hwnd 0 (a slot played once); the app sink answers a previous-procedure token and delivers the
messages to soundWindowSubclassProc. The real device was not available (DirectSoundCreate DSERR_NODRIVER, TV off, U-1691).
Roadmap S7.11, audio out edge cases (2026-10-30, notes log/roadmap/S7/11_notes.md, U-1693..U-1699): `--audio` defaults to
`on`. NO DEVICE: when DirectSoundCreate fails (or `--audio-no-device` / audio_check `--no-device 1` forces it) the sink answers
the engine's device probes as a PC without an output device does (scenario_baseline.md section 1: IDirectSound::Initialize
DSERR_NODRIVER, its Release 0, midiOutOpen / waveOutOpen errors), so the shadow's initSound takes the original's own failure
path (eax 3, g_soundReady 0, after exactly the measured probe order), its thread ends and the observer drops every later
request; the sim's trace.trace and framehash.bin equal `--audio off`'s. SHUTDOWN: the window session's WM_CLOSE stops the
shadow before DestroyWindow (gameMain's order); no thread id is left (Toolhelp32) and the handle count stops growing after a
process's first DirectSoundCreate. Pause (g_flags bit 2, key 0x50 at 0x0041d020) makes no sound call and focus muting is
DirectSound's own (the engine's buffers carry no GLOBALFOCUS / STICKYFOCUS, level DSSCL_PRIORITY), so neither needs code.
Roadmap S7.12, the course-sfx alias and the app's defect profile (2026-10-30, notes log/roadmap/S7/12_notes.md, U-1607 / U-1608 /
U-1627 / U-1628 / U-1638): switch `sound.golfSfxAlias` (audio_exe.cpp golf_sfx_name, decision G15, D54): at 1 the device sink's
mmioOpenA of the exe's `sounds\Golf sfx\` names fails on the extracted install's `Golf_Sfx` as the original's does (the CD header
iso\data1.hdr names it `Golf Sfx`; unshield wrote spaces as underscores), at 0 the install's Golf_Sfx file opens. Device run:
switch 1 = golden $device, switch 0 fcbc9195 (229 opens against 211) = an install with the exe's directory name line for line, x64
= Win32. `simgolf_port --profile original|fixed` (default fixed, compiled copy of defect_profiles.tsv, `--profile-list`); every
session.json records `profile` and every switch's value, and app_replay.gate_one replays under them. The tropical study trace runs
under fixed and faults under original (va 8, the stock game's). The same unshield rename hides six music files (U-1608). S7.13 (G16): the same switch opens the six theme music tracks under sounds\music\Desert Music / Links Music / Tropical Music from the install's underscore folders (kDirAliases); the seven names wrong on the CD stay absent; switch-0 device run 07f83f3f, 235 opens (24 aliased), x64 = Win32.
Roadmap P3, the long session's bimodal memory (2026-11-02, notes log/roadmap/P3/notes.md, edits log/roadmap/P3/edits.py,
U-2050..U-2053, U-1692 resolved): test_long_session_memory_bounded's number depended on the PC's audio device and on the heap.
(1) The app takes its last memory sample after audio.finish(), which frees the shadow audio context: with no device the shadow
fails at initSound and is gone before frame 2,000, with one (LIVING-PC has VB-Audio `CABLE Input` since 2026-11-01) it is alive
at frame 2,000 and freed at the end, so the measure read -19.9 MB. (2) Through (1), three leaks of ours in audio_exe.cpp
DeviceSink, which the app's DirectSound sink answers through: Release kept a buffer's storage (16 MB-chunk arena, at most 8) and
interface object (64 KB block), a freed block served only a request of its exact rounded size, and mmioClose kept mmio's own
buffer. A sink that reports a buffer's end (the real device, Capture) lets the engine reap idle sounds and load them again, so the
shadow grew with wall time (base1: 8 of 8 chunks after 1,571 s) until MissingResource silenced it. Fixed (edits.py A / A2 / A3,
control registry `audio_exe.deviceHeap` 1): the same session with the device holds a 13.4 MB arena and 2.0 MB live; capture WAVs
are byte-identical; port/test/test_audio_device_heap.py. (3) With audio off the growth is the sim's: the present log (24 B per
present; every in-game frame presents since S3) reallocated by 1.5x among three 960,000-B buffers per present, and whether its
983,160-B block found a free hole is the 0.97 MB between the modes (no-device runs 3,407,872 / 4,300,800 / 4,403,200;
SG_RASTER_THREADS=1 and --terrain headless show both modes). LiveTracePresenter reserves the log from the trace's frame count
(no-device growth 892,928 / 962,560), and the memory test measures up to the last in-session sample. test_app_audio's dsound
shutdown test fails with a real device on any build (cycle 1 releases 6 handles, U-2050).
Function bodies are not always contiguous (86 functions before b1; mainLoop 0x40f5c0 is now one body ending at 0x421615).
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
GDI font temp dirs (P2, 2026-10-31, log/roadmap/P2/notes.md): until P2 the port's GDI text provider (gditext.cpp) copied the two
game fonts into %TEMP%\sg_gditext_<pid>_<n>, accepted an existing directory and registered them session-wide
(AddFontResourceA). A killed test process left its .ttf locked by the session, and a later process that got the same pid failed
at record 0 with `missing text font file` / sg_run_boot -11 (I21 hit it three times in one day). Now every provider creates a
new directory and registers with FR_PRIVATE (negctl gditext 8 restores the old way; port/test/test_gditext_tempdir.py). A tree
built before P2 still leaks: release a dead pid's stale registration with RemoveFontResourceA on its .fot (deletes nothing).
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

### LAN release of the port (mini-server)
`scripts/lan_release/` (2026-10-29, copied from SimCity's re/scripts/lan_release, itself TD5RE's): `publish_simgolf.bat`
(`publish.sh`) builds a clean `git archive HEAD` of port/ + re/model/include into port/build/lan_build, stages
port/build/lan_release with `stage_lan_release.py` (simgolf_port.exe, freetype.dll, the app-local VC143 runtime, game\ =
original\ minus the SafeDisc files, shim, dev logs, JPEG.lib and saved games; manifest.json of sha256 per file) and
`deploy_lan_release.sh` pushes only changed files over SSH to the td5re-files nginx: http://mariano-server.local:8088/simgolf/
(/home/mariano/td5re-release/simgolf). Player PCs: `update.bat` in an empty folder fetches `simgolf_update.ps1` and the build,
makes a "SimGolf (port)" shortcut; saves live in %LOCALAPPDATA%\SimGolfPort. The updater only updates the folder holding
it and refuses a git checkout (an early version fell back to the current directory and wrote into the repo).
Release rule (2026-10-29): the roadmap orchestrator publishes on its own after merges, when `scripts/lan_release/release_due.py`
says DUE (2 commits changing port/ or re/model/include since the commit named in the server manifest's label, or 1 once that
build is 24 h old); publish.sh appends each release to log/roadmap/lan_releases.tsv. Every deploy also deletes the server files the
new manifest no longer lists (refused above half of them unless PRUNE_FORCE=1), so the server holds one copy of the game.
Player PCs prune too (since 2026-10-29): update.bat deletes files under game\ the manifest no longer lists (`-NoPrune` keeps them;
above half of game\ it stops unless `-Prune` is given), never anything outside game\.

### SMSG-decomp snapshot (third_party/smsg)
`third_party/smsg/` (2026-10-29, copied on Mariano's instruction; PROVENANCE.md has the commits) holds github.com/SMSG-decomp: an
independent matching decompilation of the same v1.03 (claims all 5,982 functions at 100%, no rebuilt binary byte-exact), its
MSVC6-tooling (Rust compiler models, no permuter), Firaxis-decomp-notes and four PDBs on the original addresses (release/). Locally
only (gitignored): VC6 SP5 in tools/vc6sp5 (sound.dll's C++ objects were built by SP5's C2 12.00.8966, so our 8168 near-misses there
used the wrong back end), the SP2 debug CRT in tools/vc6sp2 (jgld / Terrain link it), the DirectX 8 SDK in tools/dxsdk8. Not copied:
MSVC6-decomp (decompiled Microsoft compiler; nothing here uses it). Their claims are hypotheses: a source counts when our match.py
gives 100% against our anchors (C4 match evidence), a name or field meaning when our disassembly supports it.
Section S10, SMSG import and verification (2026-10-29, steps re/roadmap/S10_steps.tsv, notes log/roadmap/S10/<k>_notes.md,
merge log/roadmap/S10/MERGE.md, U-1850..U-1897). `re/tools/smsg_pdb.py` reads their MSF 7.0 PDBs (procedure rva / size / name /
.obj / archive, publics); `re/tools/smsg_import.py file|project|inventory|calibrate` turns an SMSG source into an annotation-only
re/match wrapper (`// COMPILE` their file, `// INCLUDE` their .dsp / build.bat dirs, `// FLAGS` from the .dsp project line plus
per-source overrides, `// TOOLCHAIN sp5` for sound.dll, one MATCH line per PDB procedure of that .obj: the public at the rva,
else the obj symbol by undecorated name, overloads scored, an ICF-folded address held once), scores every line, writes 100%
lines to re/match/smsg_<pdb>_[<lib>_]<stem>.cpp and misses to re/match/wip/. Never `--evidence` while exploring: match.py
writes a CSV for every result and a RED CSV breaks the C4 gate of a row already at C4. Calibration: all 169 procedures of
their Terrain sources compile to 100% at addresses we already matched (after `// COMPILE` replaced `#include`: under /ZI an
included file's __LINE__ is a constant); every address we matched in golf / jackal / jgld / libpng / zlib / lpc10 also reads
100% from their source. New 100% addresses: golf_clean.exe 609 (golf.exe/src 249, jackal.lib 335, jpeg.lib 25), sound.dll
489 (+143 bodies our function lists lack, 100% with the PDB extent, 6_boundaries_sound.tsv), jgld.dll 6, Terrain.dll 0;
misses 4 (97-99.9%, MultiDebug::~MultiDebug 92.9%). mainLoop 0x0040f5c0 is 100% from their MainLoop.cpp. SP5: `// TOOLCHAIN
sp5` runs SP5's cl / c1xx / c2 (C2 12.00.8966 = sound.dll's Rich entry, proven by the obj @comp.id); it lifts none of our 54
sound near misses and only one of their 489 sound functions needs it (0x10010920, 99.4% with 8168): sound.dll's C++ objects
were built by SP5 but 8168 and SP5 emit the same code for nearly every function. Library-object identity (S10.7): jgld 317 /
318 and Terrain 471 / 472 library procedures are SP2 libcmtd members, the exe 234 / 262 RTM libcmt, sound 425 / 554 SP5 libcmt
(the rest are import / Terrain thunks; `__amsg_exit` is below 100% in all four, U-1880); a FidDb rebuild was not needed.
Names (S10.8 / S10.9 / S10.10 / S10.7): 1,578 new rows in re/names/smsg_*.tsv with our citations, and rename lists
(8_merge.md, 9_renames_sound.tsv, 10_renames.tsv) of which rows marked P rename port keys (decision D1). Findings: golf.pdb is
right on four of five contradictions (0x004038f0 drawWalkers, 0x0040e720 openHole, 0x0042dea0 rateCourseHoles, 0x00422530
golferShotReach); the 0x0056d1b8 records are celebrity vacation-home residents (spawned by mainLoop 0x00417a5a on type-5 lots,
string 0x004c6444), not animals; golfer fields U-0010 / U-0015 / U-0016 resolved. match.py: an exe operand the obj holds as a
plain literal is compared as a literal (U-1886).
Merged by I16 (2026-10-29, log/roadmap/merges/I16.md): C4 2,765 -> 2,928 (exe +102, sound +61; gameMain held by U-1351, sound strcpy
0x1004a010 kept at its 7-byte extent), 1,578 names C0 -> C2, the 143 sound bodies created (sound.dll list 2,358, 632 smsg sound lines),
19 false starts removed per debug DLL, 23 renames without port keys; rows marked P (port keys) and the g_animalRecords symbol rename
wait for a port step (D1); mainLoop stays C2 (D2). ApplyNames.java dies on a DuplicateNameException when an ANALYSIS label of the
same name sits at the entry (21 FID labels such as __lockexit 0x004a51cf) after saving the renames before it; I16 used a copy that
drops the label and retries (log/roadmap/merges/I16/ghidra/ApplyNamesI16.java). ApplyNames reads every re/names/*.tsv, other sessions'
untracked files included: apply from a directory holding only the files being merged. UNCERTAINTIES.md addr cells must be one hex
number or empty (re_classify parses every row).
Wave 2: the SimGolf demo's sound.dll (archive.org Simgolfdemo, InstallShield 6) is a debug build with NB11 CodeView records
(2,146 procedures, 1,493 class layouts; re/tools/cv_debug.py): 58 map to retail by code identity (LPC-10, CRT asm), the rest only
through vtable slots (45 of 47 tables), giving 13 names and 72 cited field proposals. Two exe misses reach 100% only with SP5's
front end, but the exe's Rich header lists only build 8168 objects, so they are not evidence (U-1910).
Section S11, RE upkeep wave 1 (2026-11-01, G23; steps re/roadmap/S11_steps.tsv, notes log/roadmap/S11/<k>_notes.md, merge
log/roadmap/S11/MERGE.md, U-2000..U-2023). S11.1, the five C3 rows: gameMain 0x0045baf0 reaches C4 by SMSG GameMain.cpp (100%
320/320, no inline asm) once U-1351 is resolved (the original's 18 Sprite::drawG-wrapper call sites each pass a constant table, 16
g_halfBrightLut and blitPanel's two g_color16Lut, log/roadmap/S11/1_shade_sites.py; U-1351 had held gameMain only through its addr
cell); sound strcpy 0x1004a010 by library-object identity with SP5 LIBCMT.LIB strcat.obj `_strcpy` (0xf0 bytes, 0 relocations,
15 / 15 rows, 1_strcpy_lib.py; S10.7's CSV had been dropped at I16 only for its extent proposal). Held at C3 (re/c3_hold.tsv):
drawAmenitiesPanel 0x00434cf0 (SMSG Toolbar.cpp 99.9% under 8168, one reload pair after the second tooltip call 0x00432620 in the
other order, 100% only under SP5's front end, U-1910 / U-2001; route: scenario_ab.py over cov_a_build / cov_k3_build) and
fillSpanDown 0x00493000 / fillSpanUp 0x00493080, which SMSG PolyFill.cpp matches 100% (41/41, 40/40) as compiled C functions whose
whole body is one inline __asm block: which G26 (Mariano, 2026-11-02) ruled is not C4 match evidence: they need scenario evidence
(U-2000 resolved; CSVs parked in log/roadmap/S11/1_ev/, not log/diff). S11.2: the Ghidra master already carried every re/names row (5,222 function rows, 1,091
globals rows) except the four IMPORTED `entry` names of the smsg_*_lib CRT startups (kept: the loader's symbol) and five withdrawn
labels: ApplyNames skipped withdrawn `label <name>` rows at data addresses (G1's g_animals, g_typeAttr, g_recordOccupancy,
gTextBuffer2Index) and round 17's g_golferRoster 0x0058587a had no withdrawn row; all five were removed (backup
ghidra/backup/SimGolf.rep.pre_s11names_20261101). ApplyNames now applies withdrawn label rows and compares function rows by the
mapped name (the 32 Terrain rows with `<`, `*` or spaces were re-renamed to the same name on every run). Check:
log/roadmap/S11/2_compare.py over an ExportNames.java export (log/roadmap/S11/2_ghidra/). S11.3: re/roadmap/S11_c2_triage.tsv
holds every open C2 row (1,881: exe 500, jgld 337, sound 561, Terrain 483) with endstate verdict, callers / import callers /
pointer installers, reach (called 1,015, pointer-installed 657, pointer-no-installer 14, no caller 195), port module and 100% MATCH
line (exe 295, sound 289, jgld 22, Terrain 0); generator log/roadmap/S11/3_triage.py. 220 rows passed endstate.py and were marked
(dead 188, trivial 32), open C2 1,881 -> 1,661. endstate.py passes the four PE entry points as dead (U-2020; excluded by hand). 280
of the exe's 296 pointer-installed open rows already have a 100% match but no C3 (vtable / callback bodies): the largest group left.
Roadmap S11.4, the S10 renames that waited for the port (2026-11-01, notes log/roadmap/S11/4_notes.md, merge 4_merge.md, U-2030..U-2032):
the 70 `P` rows of S10 (8_merge.md sections 2 / 2b, 10_renames.tsv, 9_renames_sound.tsv minus three already applied) are applied by
log/roadmap/S11/4_assemble.py --root <dir> [--names]: re/names + hooks.csv retag (4_names.py), 936 anchored edits in 190 files of port/,
shim/src (comments only: 84 files, code and strings identical by lexer), re/model (addresses.tsv rows, the stream rows' types, the section
headers) and two new files, then the generated headers rebuilt (simgolf_model.h, model_ctypes.py, gamestate.h / _meta.inc /
_addresses.inc, soundstate.h) and model_merge checked to reproduce addresses.tsv. g_animalRecords -> g_vipResidents (type VipResident),
g_recordBanks -> g_homeLayouts (type SgHomeLayout), spawnObject -> spawnVipResident, placeAnimal -> updateVipResident, tickAnimals ->
updateVipResidents, drawFlyingBall -> drawWalkers, announceBuilding -> openHole, holeMagazineEvent -> rateCourseHoles, rateTile ->
golferShotReach, pumpInput483bd0 -> doAllTasks, frameTick483cd0 -> doMouse, enterModal483d30 -> flushInput, drawText4049d0 ->
drawTextWithColor, the sound placeholders Ch5* / Ctrl5* / Sound5* -> their class names, and the rest of 4_renames.tsv. Keep the old
spellings: registry keys, fixtures, SG_HOOK name tokens (the shim log and scenario CSV stems), pytest ids, the defect switch ids
spawnObject.cellSearchBound / placeAnimal.zeroFrameCount (U-2032) and every corpus file. The corpus is never rewritten:
re/port/corpus_aliases.tsv + port/test/renames.py translate at the readers (vec stems, raw capture keys, the manifest and coverage
tables, a .vec document's `sym` locations in re/tools/vector_lib.read_vec, segment frames.json / checkpoints.json, the captures'
framehash.json); without them every boot row ERRORs and every segment fails its load check. Merged by I25 on cdeb2609 (log/roadmap/merges/I25.md, fast gates by
G30): builds x64 / Win32 rc 0, static 820 passed (tests/test_port_scope.py and port/test/audio_stream.py read re/port/scope.tsv's
old names through port/test/renames.py), segments 205 / 205 x64 and Win32 with 0 digests changed, boot 67 / 2 / 2 on both, app
replay 3 / 3, gfx.check 0 duplicates, the 8 golden files, test_tick 250, test_golfers 60 and the audio files. Golden digests that hash a renamed callee name (commands.recordPeer logs FNV-1a of the
name) are re-pinned through SG_EMU_ORACLE=update after the oracle is GREEN.
S11.5 (2026-11-01, log/roadmap/S11/5_notes.md, U-2020 resolved, U-2040..U-2042): re/tools/endstate.py's dead checks refuse a function
containing the module's AddressOfEntryPoint or a TLS callback (`not the PE entry point or a TLS callback`; none of the four modules has a
TLS directory) and count an export inside a function's range, not only at its entry; before, the four CRT startups passed as dead (S11.3
left them out of its marks by hand). tests/test_endstate_entry.py: the entry points refused, every hooks.csv dead / trivial row still
passing, stubbed TLS / export cases, controls RED.
Roadmap S9.1, the network survey (2026-10-29, re/analysis/net/NET_MODEL.md, tables re/port/net_functions.tsv /
net_messages.tsv / net_state.tsv, tool re/tools/net_survey.py, tests tests/test_net_survey.py, U-1750..U-1754, U-1760..U-1763):
the Jackal network layer is linked but never active in v1.03 or v1.00. g_netSession 0x008400b8 is .bss with no store in the
image; the four entries into the layer (gfxFlushA / gfxFlushB from the input pumps, uiIdleNetPoll, Window::onCommand) test it
and return; g_netInterface 0x008400b0 (IDirectPlay2A) is only ever stored 0 (0x00497d78). No DPLAYX.dll import descriptor
(golf.exe, golf_clean.exe, v1.00: only the name string 0x004bf82e, unreferenced), no LoadLibraryA of it, no CreateThread /
CreateEventA / socket import, no Receive / Open / EnumSessions call; the DirectPlay GUIDs 0x004bc3e8..0x004bc428 are
unreferenced and the connect / host / join dialogs exist only in jackal.txt. The 13 interface calls: Close, Release,
EnumGroupPlayers (callback netMarkRecordCb), 9 Send, GetPlayerName. Packet = type word, sequence dword, payload at +8;
types 0x10 roster (host only), 0x20 keep-alive every 2 s, 0x80 peer drop, 0x100 / 0x200 stream data / end; DPSYS 3 / 5 /
0x31 / 0x102 / 0x103 parsed by netPoll; no simulation state is sent (membership, liveness and a stream only; host migration
by slot order). All 205 segment images hold 0 in every net global. The firewall here is BlockInbound with notification and
has no rule for the game. S9.2 part 1 built a generic DirectPlay recorder (re/frida/js/net_rec.js, reader
re/tools/net_trace.py, 24 static tests); its Frida part never ran, since no organic run can reach DirectPlay.
Decision G14 (Mariano, 2026-10-29): S9 closes as `the original has no network play`; recordings, replay and interop gates
dropped; the dormant layer is ported as specification only (S9.3: module net, 12 bodies + 13 registered
names over a recording NetHost at the IDirectPlay2A slot level, never called from tick; port/test/test_net.py 29 passed, 13 / 13
controls RED; only netNameOf (20 rows) and netPump's address-free rows are CSV-exact path-1 replays, because the other
net path-1 CSVs hash fixture blocks that hold the run's own heap addresses, so the rest are judged by instruction-level
assertions on rebuilt session objects with the host call log checked call by call; uiIdleNetPoll, netMarkRecordCb,
netService, netSendMessages have no path-1 rows and stay host calls; netHandleControl.slot15Stays default 1; U-1770..U-1774).

### Patching
Patches are `patches/<name>.ini` with `expect` bytes taken from the anchored binary. Default
`enabled=0` unless it is a compat fix. Cite the analysis note in `desc`. The static test validates
every committed patch against the anchors.

## Delegation

Workspace policy (`..\CLAUDE.md`): read-only surveys go to the account2 worker via `delegate.ps1`.
Ghidra MCP, Frida, builds, game runs, tracker writes stay local.

## Happy info pane
Keep `.happy/project-info.json` current when a standing fact changes (status, counts, key dates).

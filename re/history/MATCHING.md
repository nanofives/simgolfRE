# Matching decompilation: reference notes

Moved verbatim from CLAUDE.md on 2026-11-04 (section "Matching decompilation"). VC6 flags, match.py options, idioms per module.

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

# Matching plan: every module with code (2026-10-04)

Goal: a VC6 source for every game-authored function in the four modules the game runs, each proven by
`re/tools/match.py` at 100% (C4 evidence). Numbers below come from `py -3.12 re/tools/match_inventory.py`;
re-run it to update this file.

## Scope

| Module | Build (Rich header, PDB) | Functions | Not game code | Library (FidDb) | Game code | Matched | Left |
|---|---|---|---|---|---|---|---|
| golf_clean.exe | release /O2; 152 C + 109 C++ objects, cl 12.00.8168; 4 MASM objects | 2,207 | 603 | 312 | 1,292 | 155 | 1,137 |
| Terrain.dll | debug /Od /ZI /GZ (`3DTerrainLowPoly\Debug\Terrain.pdb`); 127 C + 27 C++ objects, 8168; 1 MASM | 1,050 | 225 | 521 | 311 | 69 | 242 |
| jgld.dll | debug (`C:\JackDev\Debug\jgld.pdb`); 141 C + 31 C++ objects, 8168 | 1,569 | 308 | 365 | 903 | 4 | 899 |
| sound.dll | release; 104 C + 11 C++ objects from cl build **8047**, 34 C++ from **8966**, 40 from 8168 | 1,721 | 607 | 307 | 807 | 0 | 807 |

"Not game code" = incremental-linking thunks, `Unwind@`/`Catch@` C++ EH funclets (547 in the exe; they are
generated with their parent function), `FID_conflict:` library matches and 6-byte import stubs
(`match_queue.not_game`). "Library" = uniquely identified by the FidDb (VC6 CRT/STL/iostream, IJG libjpeg).
Total left: **3,085 functions**.

Left by size (functions; reached by the coverage census in brackets):

| Module | <= 64 B | 65-256 B | 257-1024 B | > 1024 B |
|---|---|---|---|---|
| golf_clean.exe | 455 (253) | 364 (136) | 204 (88) | 114 (61) |
| Terrain.dll | 145 (49) | 52 (30) | 36 (10) | 9 (4) |
| jgld.dll | 319 (36) | 332 (69) | 146 (20) | 102 (5) |
| sound.dll | 321 (21) | 301 (19) | 160 (23) | 25 (4) |

Game-code bytes by size, golf_clean.exe: 15.7 KB / 61.7 KB / 101.6 KB / 424.3 KB; the 114 functions over 1 KB hold
70% of it (FUN_0040f5c0 alone is 59.6 KB).

## Observed success rates (re/match vs re/match/wip, 2026-10-03/04)

| Build | <= 64 B | 65-256 B | 257-1024 B | > 1024 B |
|---|---|---|---|---|
| golf_clean.exe /O2 | 82 of 89 (92%) | 69 of 85 (81%) | 4 of 9 (44%) | none tried |
| Terrain.dll debug | 18 of 18 | 26 of 26 | 12 of 12 | 13 of 13 |
| jgld.dll debug | 4 of 4 | none tried | none tried | none tried |

Release misses are register allocation and block/load ordering (late callee-saved pushes, loop pointer anchors,
which value lands in esi vs edi). Fixes that worked were mechanical: comparison spelling (`> 5` vs `>= 6`,
`< 1` vs `<= 0`), if/else block order, ternary operand order, declaration order of two temporaries, a store moved
before or after an inline strcpy, `strcat` per switch case. Debug builds compile almost literally.

## Phase 0: tooling (done 2026-10-04)

| Tool | What it does |
|---|---|
| `re/tools/match_queue.py --module M --min A --max B` | next candidates: game code only, not in re/match, wip or `re/match/skip.tsv`; reached first (`--size-order` to ignore reach), one-line disassembly each |
| `re/tools/match.py` | now all four modules (image bases from the PE); `// LANG c` compiles a file as C (`/Tc`, `_name` symbols); `--only 0x...` one function |
| `re/tools/match_permute.py <file> <addr> [--write]` | hill-climbing over rewrites: comparison spelling, operand swap, if/else swap, ternary swap, adjacent statement swap, `!x` / `x == 0`. Validated by recovering `canAfford` from its 93% form in 13 compiles (~1 s). On the 31 wip partials: freeBlock 81% -> 100% (adopted), 8 others improved (best kept in wip) |
| `re/tools/match_clusters.py --at 0x...` | neighbourhood of a function: adjacent functions sharing data, callees or calls, and the shared globals. Not object boundaries (U-0006: the 38 Terrain.cpp exports fall into 26 clusters) |
| `re/tools/match_inventory.py` | the tables in this file |
| coverage census | arms jgld.dll and sound.dll too (adopted at the first phase switch after they are LoadLibrary'd); sandbox_basic: jgld 207, sound 129 functions |
| `tests/test_match_tools.py` | static: inventory, every `// MATCH:` address is a function entry, queue exclusions, permuter rewrites, C mode |

## Phases

Each phase works smallest-first inside the module, in batches of 5-10 functions per file, and ends with
`pytest -m static` green and the counts updated in CLAUDE.md, ROADMAP.md and the info pane.

| Phase | Work | Why this order | Projection at the observed rates |
|---|---|---|---|
| 1 | Terrain.dll: the 242 left | Debug build, 100% so far; finishes a whole module | DONE 2026-10-05: 101 matched, 141 were CRT fragments (skip.tsv): 170 of 170 game functions (the 2 empty methods are unreferenced; names not determined) |
| 2 | jgld.dll <= 1024 B (797) | Debug build, same compiler; the graphics layer every screen uses | 2026-10-05: 640 of 671 game functions <= 1 KB matched (libpng 1.0.5 + zlib 1.0.2 compiled from their source: 202 functions); 31 left (float blitters, CONCAT, the guarded dtor stub 0x10006a40, png_default_error) |
| 3 | golf_clean.exe <= 64 B (455) | High rate, mechanical; fills struct layouts used by larger functions | 2026-10-05: 497 of 526 matched (11 C runtime functions moved to skip.tsv) |
| 4 | golf_clean.exe 65-256 B (364) | 81% rate; permuter for the rest | 2026-10-05: 280 of 433 matched, by hand in parallel local agents (golf_hand_NN.cpp); libjpeg 6a compiled from source adds 97 across all sizes |
| 5 | golf_clean.exe 257-1024 B (204) + jgld.dll > 1024 B (102) | Needs the permuter and the layouts from phases 3-4 | 2026-10-05: jgld > 1 KB 69 of 101 (42 sprite blitters from one skeleton, re/tools/blit/gen.py); exe 257-1024 B in progress |
| 6 | golf_clean.exe > 1024 B (114, 424 KB) | The simulation core: FUN_0040f5c0 (59.6 KB), 0x442180 (24.1 KB), FUN_004289e0 (20.9 KB) | no data yet |
| 7 | sound.dll (807) | Blocked on compilers: 115 objects from cl 8047 and 34 from 8966 (only 8168 is installed); start with its 40 objects from 8168 | 8168 part only |

Projections multiply the left counts by the observed rates; they are not measurements. jgld.dll and the > 1024 B
exe functions have no attempts yet.

## Large functions (phase 6)

Split by behaviour first (the analysis notes already map parts of FUN_0040f5c0 and FUN_004289e0), write the
source in pieces, and compare the whole function only once all pieces compile. Partial progress is tracked as the
instruction-match percentage in `re/match/wip/` like today. A function that stays below 100% can still be
verified functionally with record/replay (`re/frida/record.py` / `replay.py`); that is C3 evidence, not C4.

## Risks and open questions

- **sound.dll compilers** `[UNCERTAIN U-0005]`: the Rich header lists Utc12 builds 8047 and 8966 besides 8168; which
  Visual C++ releases these are and whether they are obtainable is not established. Without them those objects
  can only reach "functionally verified".
- **Unreached functions** have no runtime check beyond the match itself; that is fine for C4 (byte-level
  identity) but they rank lower for analysis notes.
- **Game text**: string literals are masked by the matcher; dialogue and UI sentences go in as placeholders with
  their address (the repo is public), short identifiers (club names, "Hole ") as written.
- **Throughput** is not measured yet (phases 3-4: 819 functions).

## Tracking

- `py -3.12 re/tools/match_inventory.py` after every batch; this file's tables are regenerated from it.
- `re/match/wip/*.cpp` keeps every partial with what was tried.
- Workspace delegation: drafting candidate sources from decompilation is read-only and can go to the worker
  (`repo-fleet`), returning text; compiling, comparing and writing files stay local.

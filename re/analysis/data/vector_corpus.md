# Phase 2.5: the function-vector corpus (2026-10-18/19)

Step 2.5 of re/PHASE23_PLAN.md: for the sim-scope functions of golf_clean.exe, the recorded inputs and outputs of the
ORIGINAL in a form a port test harness replays without the game. Format: re/model/vector_schema.md. Running notes:
log/p2/p25_notes.txt. Every number here is from a run listed there (logs in log/p2/p25/).

## 1. Files

| file | committed | what |
|---|---|---|
| re/tools/vector_select.py | yes | selection -> re/port/vector_selection.tsv |
| re/tools/vector_capture.py | yes | `capture` (live), `build` (Unicorn), `status` |
| re/tools/vector_emu.py | yes | the Unicorn engine over the file image of golf_clean.exe |
| re/tools/vector_lib.py | yes | schema, GameState address map, .vec reader / writer / structural validator |
| re/tools/vector_validate.py | yes | `emu` and `live` replays (section 5) |
| re/tools/vector_coverage.py | yes | coverage statement -> re/port/vectors_coverage.tsv |
| re/frida/js/vector_capture.js, vector_replay.js | yes | the capture and live-replay runners |
| re/port/vector_selection.tsv, vectors_manifest.tsv, vectors_coverage.tsv | yes | selection, index, coverage |
| port/corpus/vectors/<function>.vec | no (local) | the corpus: 243 files, 9.6 MB |
| port/corpus/vectors/raw/<key>.raw.gz | no (local) | raw captures: 764 files, 0.57 GB |
| tests/test_vector_corpus.py, test_vector_capture_runtime.py | yes | 30 static tests, 1 runtime test (22 s) |

No shared file was edited for the capture: diff_hook.py, the registry, the fixtures and keepalive.js are unchanged.
vector_capture.js is loaded after the same files diff_hook.py loads (a static test keeps the two lists equal) and wraps
`__keepAlloc`, the stand-in entry points and `Interceptor` inside its own script.

## 2. Selection (re/port/vector_selection.tsv)

| tier | rule | functions | with vectors |
|---|---|---|---|
| sim | C3/C4 hooks.csv rows, subsystem sim / golfer / economy / course / terrain / ai | 146 | 112 |
| util_callee | C3/C4 util rows reachable from a sim row through hooks.csv callees | 79 | 66 |
| crt_callee | C3/C4 crt rows reachable the same way | 18 | 13 |
| util_pure | the other C3/C4 util rows whose detour is `pure` / `globals` (log/port_hooks.tsv) | 54 | 42 |
| simcore | rows of re/port/simcore_order.tsv (phase 2.0) not in a tier above | 11 | 10 |
| total | | 308 | 243 |

All 308 have registry keys from linked batches (642 keys). 159 of simcore_order.tsv's 169 rows are selected (the other 10
are not C3 / C4, mainLoop among them); 137 of the 159 have vectors.

## 3. Capture (`vector_capture.py capture`)

One boot per batch (68 batches, 3 instances in parallel, `--shard k/n`), SIMGOLF_HOOKS_OFF=all: the function's address is
the original code and so are its callees (diff_hook's original arm is the trampoline with REIMPLEMENTED callees). Per key:

1. the fixture runs; every `__keepAlloc` block is recorded with its size; stand-in use is recorded by name;
2. the first thread is frozen (`__freeze.thread()`) unless the fixture froze it: without the freeze the main menu changed
   up to 1,753 data dwords between two vectors of one key, and computeRatingTrend 0x004060a0 vector 15 returned 0 live
   where the pre-call snapshot gives -45 (20/20 after the freeze);
3. functions the fixture stood in: the first 8 bytes of every exe function start against the file (a forwarding object in
   front of `Interceptor` also records the calls; a Proxy fails with `inconsistent get`, its methods are read-only);
4. the data-section image, the blocks, and the 2 pages behind every pointer argument / block dword that is neither data,
   module nor block; FPU control word of the calling thread (0x027f for every key);
5. per vector: arguments as resolved, the live return value, the drift since the previous vector and the call's exact
   dword (data) / byte (blocks) changes (a CModule diffs the 921,693 dwords); the changes are undone after each vector
   (pointer-class objects excepted);
6. in the same boot, a light emulation of up to 24 vectors names live pages the function reached through data pointers;
   they are fetched (up to 4 rounds; data only, code outside the exe is never emulated): membershipReport reads sound.dll
   and heap pages that way.

Result: 642 / 642 keys captured, no capture error after the 4 s settle (U-0700). `capture --unstub` re-captured the 122
keys whose fixtures stand in a selected function, with those stand-ins reverted (118 captured; applyTerrainEdit /
_real, updateTerrainRender and one more hung the game and are recorded as capture errors).

## 4. Build (`vector_capture.py build`)

Each vector is replayed in Unicorn (re/tools/vector_emu.py) from the file image (the original code, no hook jumps),
the captured image + drift, the blocks at their live addresses, the captured control word. Statuses:

| status | meaning | vectors |
|---|---|---|
| ok | emulated return value = live; every emulated write holds the live post value; every live change was emulated (or is a concurrent write the function never read) | 15,191 |
| not_emulated | over 50 M instructions in a light run, or the first 12 emulated vectors of the key all failed, or a heavy key's sample (a key whose first hooked vector makes > 2 M memory accesses keeps 12 evenly spread vectors) | 3,421 |
| emu_error | an import, an unmapped read, code outside the exe | 2,528 |
| write_mismatch / ret_mismatch | the emulated original differs from the live call: stood-in callees in the live run | 1,429 / 129 |
| live_error | the live original faulted (kept, without outputs) | 470 |

Inputs and outputs come from the emulation of the `ok` vectors: `seed` (data bytes any vector of the key read, values at
the key's start), `in` (a vector's reads whose value drifted), `out` (every byte written outside the stack, final value),
`out_ptrs` (written dwords that point into a block), `relocs` (block dwords at any byte offset and read data dwords whose
value points into a block; List402_clear keeps block pointers at +17 / +22 of 64-byte records). A second emulation with
every block moved (packed from 0x30000000) proves the relocations per vector (`reloc_check`): 4,193 true, 17 false,
10,981 without blocks. Branch coverage: the block hook bound to the function's own ranges gives (jcc, successor) edges.
`x87` and `callees` are static over direct call / jmp targets (U-0702). Build time: about 40 minutes on 8 processes.

Keys: 356 ok, 106 partial, 180 excluded (of 642); +unstub: 15 ok, 38 partial, 63 excluded, 6 capture / fixture errors.
updateWalkers, placeHabitat and readSectionGlobal reproduce only through +unstub keys.

## 5. Validation (`vector_validate.py`)

| replay | what runs | functions | vectors pass | fail |
|---|---|---|---|---|
| emu | the original in Unicorn from the .vec ALONE: zero data section + seed + in, blocks at new bases (page offsets changed) with the relocations applied; return, stack pop and the exact written set compared | 243 | 15,188 | 3 |
| live | the shim's REIMPLEMENTATION (SG_HOOK detour) in a hooks-on boot, first thread frozen, seed / in / blocks written into the live process; return and every out region compared | 243 | 15,189 | 2 |

Every failure is a vector the build had flagged `reloc_check: false` (destroyGlobalReader_open 1-2, scanShotArea_real+unstub
5; U-0701). Findings made during validation, all fixed in the tools, none in the reimplementations:

- the first live run failed 174 vectors in 21 functions, all text builders writing g_textBuffer (announceHoleType,
  append*, buildGolferName, decorationName ...): the live replay froze the first thread only when the fixture did, and
  the main menu writes g_textBuffer. Freezing every key: 1,067 / 1,069 pass in those functions;
- logWrite returns a block address typed `int`: such returns are now `ret_loc` locations (20 / 20);
- `__ftol` converts st(0): its keys (an exported x87 probe in c3fo) cannot be memory vectors; the live replay read a
  different st(0) for every vector, so __ftol is excluded (X87_REGISTER_INPUT in vector_capture.py).

No mismatch between a reimplementation and the original was found: the C3 evidence of the 243 functions holds on these
15,191 vectors.

## 6. Coverage statement (re/port/vectors_coverage.tsv)

Per function: reproduced vectors and distinct (ret, out) pairs, the C3 A/B CSVs' vector rows and distinct_results, the
function's conditional jumps and the sides the reproduced vectors took, the open sides (function-relative), and the C3
purpose file citing the address (log/c3/*_purpose.md; 302 of the 308 rows have one).

| verdict | rule | functions | simcore |
|---|---|---|---|
| full | every jcc of the function's own body taken both ways (or none) | 113 | 70 |
| one_sided | every jcc reached, some one way only | 35 | 27 |
| partial_branch | a jcc no vector reached | 24 | 19 |
| thin | fewer than 10 reproduced vectors, or one distinct result | 71 | 21 |
| none | no reproduced vector | 65 | 22 |

`thin`, `partial_branch` and `none` (160 functions) need the segment corpus (phase 2.4) for the parts the vectors miss.
Simcore functions in these classes include updateGolfers, updateWalkers, pathStep, trialShot, chooseShotTarget,
prepareShot, matchUpdate, emitGolferThought (partial_branch: stood-in callees and heavy-key sampling), narrateShot,
storyText, addScore, holeMagazineEvent (thin), placeAnimal, generateRandomCourse, computeStoryReply, loadStoryStrings,
resetGolferState (none: stand-ins, file I/O, imports).

## 7. Why 65 functions have no vector

| class | functions | examples |
|---|---|---|
| callee stand-ins + imports | 32 | fileExists, listMatchingFiles, initTerrain, refreshTerrain, lookupThemeText (FindFirstFileA, Terrain.dll imports, heap) |
| callee stand-ins | 24 | placeAnimal, hireEmployee, syncTiles, rebuildTerrainGrid, the report screens (stood-in drawing, pumps, RNG) |
| code outside the exe | 7 | resourceLoad / B, orderPair, jpeg_alloc_quant_table / huff_table, jpeg_abort, dump_buffer (KERNEL32 / DLL code) |
| imports | 1 | _strtok |
| x87 register input | 1 | __ftol |

The most frequent import reached is KERNEL32!EnterCriticalSection (91 keys: the static CRT's heap lock), then HeapAlloc,
FindFirstFileA, timeKillEvent and Terrain.dll methods. 23 functions with vectors depend on the heap (a heap stand-in in
the fixture or a heap import on a failed vector); 47 have x87 code in their static closure.

## 8. What phase 2.6 (the harness) must read

1. Only `status: ok` vectors are tests; skip a vector with `reloc_check: false`.
2. Build the GameState zero-filled, write `seed`, then `in`; allocate blocks, copy, apply `relocs` (an `at` in a block or a
   `sym` dword gets the harness address of `to`).
3. Set the x87 control word to `fpcw` (0x027f everywhere) before an `x87` function.
4. Call by name with `args` (`ptr` locations resolved); `abi` and `stack_pop` give the convention (thiscall: args[0] in ecx).
5. Compare `ret` (`{"ptr": loc}` for pointers, `ret_loc` for an integer that is a block address, `float_repr` for
   NaN / infinities) and every `out` region, with `out_ptrs` dwords compared as locations. The exact written set is the
   stricter check vector_validate.py emu applies.
6. `pointer_objects_read` names inputs that hold original addresses (U-0703); `const` regions are bytes of the user's own
   exe (the port reads them from there); `stood_in` / `stood_in_reached` and `+unstub` keys tell which callees ran real.
7. The manifest's sha256 / bytes identify each .vec; `vectors_coverage.tsv` tells which functions need segments.

## 9. Known gaps

- Callee stand-ins: 56 functions are excluded because the live original ran with stand-ins the emulation does not
  reproduce; recording the stand-ins' (args, return) per call would make them replayable as mocked-callee tests.
- Heavy keys are sampled (12 vectors) and vectors over 50 M instructions are not emulated (3,421 vectors in all).
- Reads of data addresses held in plain objects (U-0703), static x87 / callee closure (U-0702), concurrent-write
  attribution with only the first thread frozen (U-0704), relocation of destroyGlobalReader_open (U-0701).
- Sizes: corpus 9.6 MB, raw captures 0.57 GB, about 0.58 GB in all.

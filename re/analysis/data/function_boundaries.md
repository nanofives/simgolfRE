# Function boundaries: gap classification and master repair (b1, 2026-10-17)

The function index (`re/functions_ghidra*.tsv`, exported from the Ghidra master) is what xref.py, c2_note.py, the
callers/callees columns of hooks.csv, record_fields.py and the port surveys read. Before this repair, 84,015 bytes of
golf_clean.exe's game code (0x401000..0x4a4ecf) that are not 0x00, 0x90 or 0xCC lay outside every function range, 57 KB
of them in mainLoop. This note records what every byte outside the index was, what was changed in the master, and the
survey numbers before and after. NO-GUESSING applies: every class below is decided by a rule over the anchored image,
and the one origin question that the image cannot answer is filed as [UNCERTAIN U-0018].

## Method (log/b1/, local)

`log/b1/gaps.py <module> [--tsv <list>]` classifies every byte of each executable section that no range of the list
covers, by a recursive descent over the image (capstone):

- **Descent.** From every listed entry: jcc both ways, direct jmp, fall-through after a call unless the next byte is
  padding or the callee does not return (exit / _exit / abort / _amsg_exit / _CxxThrowException, ExitProcess and the
  other non-returning imports, and functions proven by a fixpoint to reach no `ret`, where a flow into other code
  counts as returning). Switches `jmp [r*4 + T]` are decoded from their bound: `cmp r, N` + `ja` / `jae`, through a
  byte index table `mov r8, [s + B]`, register copies, a stack slot the debug builds reload (`cmp dword [ebp-x], N;
  ja; mov ecx, [ebp-x]`), `cmp r, r2` with `mov r2, imm`, or `and r, 2^k-1`. Only the CRT memcpy / memmove tables
  (all four modules) have no bound; they are read while their dwords point into the function (class `jumptable_h`).
  A descent stops at any other entry and at another function's range.
- **New entries** (iterated until nothing changes): a direct call target; a code address in a 4-aligned word of
  .rdata, of a pointer table in .data (a neighbouring slot holds a known entry) or of a NULL-delimited CRT table whose
  start code loads; in the DLLs only relocated words count, since a DLL pointer always carries a relocation; a code
  address pushed / moved as an imm32 by decoded code (relocated in the DLLs); the target of a referenced
  incremental-link thunk; a jmp to the first byte after padding, or out of the jumping function's slot (its entry
  up to the next entry it does not itself call). Data and operand evidence needs a boundary at the target (padding,
  a flow end, a table end, the VC++ `VC20XC00` signature or the padding residue below before it).
- **False functions.** A listed entry reached only by jcc / switch / fall-through, or only by one function's jmp
  from inside that function's own span, with no call, data reference or thunk, belongs to that function. Deleted
  one owner at a time (`log/b1/plan.py`), the analysis re-run after each pass, until none is left; an entry several
  functions reach (Terrain 0x10021e3e, a shared CRT tail) stays.
- **Bodies.** A body is the code the descent reaches, inside the function's slot. Unreachable compiled bytes (after
  a non-returning call, report sequences no branch enters) are classified but not added, as Ghidra keeps them.

Outputs: `log/b1/gaps_<module>.tsv` (one row per run of one class: lo, hi, bytes, class, owner, evidence),
`log/b1/after/gaps_<module>.tsv` (the same over the repaired index), `log/b1/new_functions_<module>.tsv` (every created
function: size, kind, evidence, reach), `log/b1/spec_<module>.tsv` (the repair applied), `log/b1/apply_*.txt`.

## Classification before the repair (bytes outside every listed range)

| class | golf_clean.exe | jgld.dll | Terrain.dll | sound.dll |
|---|---:|---:|---:|---|
| A reachable code of a listed function (`code_of`) | 66,856 | 904 | 498 | 3 |
| B separate function missing from the index (`code_new`) | 28,837 (437 entries) | 1,087 (62) | 625 (56) | 11,217 (463) |
| C switch jump tables and byte index tables | 9,095 | 2,295 | 1,926 | 2,404 |
| D other data in .text (`VC20XC00` signature) | 8 | 8 | 0 | 8 |
| E incremental-link thunk-table entries not in the index | n/a | 1,605 | 45 | 4,010 |
| F `jmp [IAT]` import thunks nothing references | 0 | 828 | 762 | 774 |
| G dead / unreachable code | 46 | 4,617 | 4,701 | 14,348 |
| H padding residue (below) | 3,961 | 0 | 0 | 0 |
| I padding (0x90 / 0xCC runs, 2-3 byte nop forms before tables) | 10,815 | 603,038 | 179,254 | 99,491 |
| total | 119,618 | 614,382 | 187,811 | 132,255 |

Reconciliation with the 84,015 bytes measured before b1: that count treats every 0x00, 0x90 and 0xCC byte outside
the ranges as padding. With only 0x90 / 0xCC runs as padding, the game range holds 98,713 such bytes; the classes
above cover all of them (the 205-byte difference to their sum is the nop forms and the 0x90 / 0xCC bytes that lie
inside instructions and tables).

Classes in detail:

- **A, mainLoop.** 65,778 of the 66,856 exe bytes are mainLoop's: Ghidra stopped at the switch `jmp [ecx*4 +
  0x421634]` at 0x004115d5, and its listed ranges (0x40f5c0-0x410896; 0x41094f-0x4115dc) also missed the case bodies
  of the switch at 0x0041088f. The descent from 0x0040f5c0 gives one contiguous body 0x0040f5c0..0x00421615, as
  `re/analysis/sim/mainLoop_structure.md` measured, followed by its 27 jump / byte tables at 0x00421618..0x00421b23.
  The other truncated exe functions: loadInterfaceArt 0x442180 (+381), showRoutingMapScreen 0x456be0 (+286),
  courseActionMenu 0x46f550 (+231), viewDispatchBuild 0x4a0ef0 (+167, the 206-byte extent of boundaries.tsv),
  buildGolferName 0x4676e0 (+9), LabelButton::drawContents 0x4a1580 (+1, one byte of an instruction straddling its
  range end 0x4a1ae8), __amsg_exit 0x4a6937 (+3). In jgld and Terrain, A is the debug CRT heap routines
  (`__heap_alloc_base`, `__heapchk` and three FUN_ siblings): Ghidra split each at its `__try/__finally`
  (`mov [ebp-4], -1; call $L_finally; jmp $L_after`, e.g. jgld 0x10085cee / 0x10085cf3), lists `$L_after` as
  a separate function and leaves code between the parts outside; plus png_default_error 0x100792b0 (+17, the /GZ
  epilogue after longjmp, boundaries.tsv's 74 bytes) and jgld FUN_10082a82 (CRT x87 asm, +389 inside its slot).
- **B.** By evidence: exe 193 from .rdata (vtable slots, EH tables), 12 from .data pointer tables, 3 from the CRT's
  NULL-delimited initializer / terminator tables (0x4a56e4 in slot 0x4c1190, 0x4a9860 in 0x4c11a8, 0x4abecb in
  0x4c11b4), 207 from push / mov imm32 (atexit destructors, callbacks, the 97 `__ehhandler$` stubs `mov eax, offset
  FuncInfo; jmp ___CxxFrameHandler` the SEH prologues push), 12 from calls (eight called only from mainLoop's far
  blocks: 0x406f20, 0x406f90, 0x407d30, 0x40b680, 0x40d760, 0x40d890, 0x421b30, 0x46f180; 0x4321a0, called from
  0x40b680; three CRT helpers) and 10 from tail jumps (`jmp body` dynamic-initializer stubs such
  as 0x449160 -> 0x449170, and the CRT `jmp 0x4ac02c` at 0x4a986e). The five largest exe gaps outside mainLoop were
  vtable methods never made functions: 0x48b3f0 (3,384 bytes, ListBox vtables 0x4bb3cc / 0x4bba3c), 0x49c0b0
  (1,738), 0x47ea30 / 0x47e770, 0x496860 (973), 0x495a50 / 0x495aa0 / 0x495bd0. sound.dll's 290 thunk-evidence
  entries are virtual methods whose incremental-link thunks sit in relocated .rdata vtable slots.
- **E.** Ghidra lists 248 of jgld's 570 thunk-table entries, 160 of Terrain's 169 and 403 of sound's 1,207. Of the
  unlisted ones, jgld 247 and sound 550 are referenced only from data (vtable slots; pointer_callers.py reads the
  thunk table directly), and exactly one is called: sound 0x10001c62 (`jmp 0x10028600`, called at 0x100091c5).
- **G.** Debug-DLL functions that nothing references, whose thunks nothing references either (incremental
  linking keeps them); jgld Sprite::drawQ's two unreachable report sequences 0x10017693..0x10017705 (c3ci notes);
  compiled epilogues after non-returning calls. Referenced dead code: Terrain 0x10019eea..0x10019fb1 before the
  repair (it held `_except_handler3`, created in pass 2) and sound 0x1005861b (an EH handler stub whose only reference,
  at 0x10009249, lies in the unreferenced code 0x10009240..0x100092b9).
- **H, padding residue (exe).** 716 call instructions sit in inter-function padding, right after a `ret` / `ret n`
  and before a function entry, and no flow reaches any of them: 358 `call rel32` into 0x00405095..0x00405099 (the
  mid-instruction bytes of the stub at 0x405010 that ends `ret` at 0x00405098) and 358 `call [IAT slot]` over 101
  imports (binkw32 _BinkBufferBlit@12 17, _BinkNextFrame@4 17, WINMM mmioAscend 15 ...). 0x00405099 runs `push ecx;
  push eax; call 0x00404a14`; 0x00404a14 is `mov eax, 0xffffeefc; pop ecx; add eax, ecx; mov eax, [eax]; jmp eax`,
  so it jumps through the dword at its return address 0x004050a0 minus 0x1104 = 0x00403f9c, which holds 0x008a2590,
  outside the image. Who wrote these bytes is [UNCERTAIN U-0018] (golf.exe's .text is encrypted; golf_v100_clean.exe,
  SafeDisc 2.51, has no such stub). The residue explains the "5 unknown bytes before an entry" pattern (`call
  0x405099` in front of 0x49c0b0, 0x486f40, 0x484920, 0x4a3390 ...) and one Ghidra artefact: a pointer `addr
  0x0080c781` that Ghidra had defined at 0x0041ea32, where mainLoop's code has `add edi, 0x80`.

## Repair of the master

Backup first: `ghidra/backup/SimGolf.rep.pre_b1_20261017` and `SimGolf.gpr.pre_b1_20261017`. New script
`ghidra/scripts/RepairBodies.java <spec> <dry|apply> <report>` (headless, `-noanalysis`): deletes, then disassembles
the instruction starts the descent found (flow restricted to the body; a conflicting code unit is cleared only when
no function owns it: one, 0x0041ea32), adds COMPUTED_JUMP references for each decoded switch, sets the bodies of
existing functions, creates new ones with explicit bodies (so no function can absorb code it jumps to, the ordering
problem CreateFunctions.java has), marks thunks, writes a jump-table override where references had to be added, and
verifies that every set / created body equals the requested ranges with no byte that is not an instruction. A dry run
on the read-only master preceded each apply.

| module | functions before -> after | bodies set | created | deleted | verify |
|---|---|---:|---:|---:|---|
| golf_clean.exe | 2,497 -> 2,934 | 8 | 437 | 0 | 445 / 445 |
| jgld.dll | 1,570 -> 1,621 | 8 | 62 | 11 | 70 / 70 |
| Terrain.dll | 1,050 -> 1,095 | 6 | 55 + 1 (pass 2: `_except_handler3` 0x10019ef4) | 11 | 62 / 62 |
| sound.dll | 1,753 -> 2,215 | 3 | 463 + 1 thunk (0x10001c62 -> SoundWindowObj_ctor) | 2 | 466 / 466 |

- **mainLoop** 0x0040f5c0: 8,035 bytes in two ranges -> one range 0x0040f5c0..0x00421615 (73,813 bytes); its export row
  now lists 219 callees. 43 exe switch sites got computed-jump references (28 had none, 15 were partial), with 38
  jump-table overrides; the mainLoop sites include 0x0041088f, 0x004115d5, 0x0041c5b2 (57 cases) and 0x0042134a.
- **Deleted false functions**: sound FUN_1001c1f5 (part of Ctrl5b5c0_transposeTable, which now has the 94 bytes of
  boundaries.tsv) and FUN_100431d1 (the int3 block of FUN_100431ce); jgld FUN_10085d00 / d82 / da1 / e9c / f95,
  FUN_100861c1 / 863d6 / 86516 / 86598 / 86692, thunk_FUN_10086692 0x1008664d, and their 11 Terrain twins
  (0x1001a120 ... 0x1001b262): `$L_after` continuations of the debug CRT heap routines. None had a hooks.csv row or
  appeared in any row's callers / callees.
- **Names**: every created function has Ghidra's default FUN_ name (no re/names row or MATCH line names any of them);
  no existing function was renamed (the before / after export diff has 0 name changes).
- **Not changed**: Ghidra bodies that include trailing padding (boundaries.tsv rows Channel_parseWaveHeader_adjPlus70,
  atexitDtor_100b49b8, Font::deletingDtor_adj8) and drawQ's unreachable middle; data-referenced thunk-table entries
  (they resolve through pointer_callers.py's direct read of the table). The four boundaries.tsv rows whose extent the
  master now has (buildGolferName, png_default_error, viewDispatchBuild, Ctrl5b5c0_transposeTable) are kept as
  overrides with a note.

After the repair, `log/b1/after/gaps_<module>.tsv` has no `code_of` and no `code_new` byte in any module: what lies
outside the index is padding, the padding residue, tables, the signature, thunks, unreferenced import thunks and
dead code.

## Reachability of the created functions (log/b1/new_functions_<module>.tsv)

Roots: the module entry point and the exports. Edges: direct calls (thunks resolved), tail jumps and flows into
another function, pushed / stored function addresses, code-pointer tables whose start (or the NULL slots before it,
up to 0x400 bytes) code loads, and FuncInfo (0x19930520) -> its unwind / catch funclets for the function that loads it.

| module | created | reachable from the roots | not reached |
|---|---:|---:|---:|
| golf_clean.exe | 437 (game code 275, CRT 162) | 433 (all 275 game functions) | 4: CRT 0x4aad0a, 0x4aad0e, 0x4adc8e, 0x4adc92, referenced only from .rdata tables no code loads |
| jgld.dll | 62 | 38 | 24 |
| Terrain.dll | 56 | 29 | 27 |
| sound.dll | 464 | 425 | 39 |

The not-reached DLL functions are virtual methods and EH stubs of code that is itself unreachable (debug builds keep
unreferenced functions). hooks.csv gets **no new row**: rows are created only from names TSVs (`re_classify.py batch`),
and no created function has a name.

## Effect on the trackers and the surveys

- `c2_note.py --refresh` + `re_classify.py retag`: 158 hooks.csv rows changed callers / callees only (no level, file,
  hook, name or evidence column changed; no C3 / C4 row lost a recorded caller). 153 rows gained callers (123 C4, 19
  C3, 11 C2), mostly calls from mainLoop's far blocks; mainLoop's callees 55 -> 216. The one dropped callee is
  mainLoop's `0x00420e35`, which is inside mainLoop's own body. Before the thunk 0x10001c62 existed, the refresh would
  also have dropped dynInit_soundWindowObj100b49b8's recorded callee 0x10028600 (xref.py resolves calls through a
  thunk only when the index lists the thunk).
- `log/c3/hook_integrity.py`: 0 C3 / C4 rows without a linked SG_HOOK. Static suite: 459 passed, 2 skipped (the
  match tests included: every re/match/*.cpp still 100%).

| survey | before | after |
|---|---:|---:|
| game-logic functions scanned (port_survey_globals.py) | 1,537 | 1,812 |
| distinct data addresses referenced | 5,302 | 5,589 |
| clusters (gap <= 0x40) | 955 | 960 |
| clusters with a named member / unnamed | 67 / 888 | 67 / 893 |
| functions touching unnamed clusters | 500 | 584 |
| golfer-record references (record_fields.py, 0x5794b8 x 0x100 x 156) | 3,120 | 3,120 |
| of them in code outside every function range | 444 | 0 |
| port_survey_hooks.py (sources only, unchanged by b1) | pure 313, globals 177, callees 570, both 517, naked 12 | same |

The two largest unnamed clusters before (0x4c80d0-0x4cd3cc, 848 addresses; 0x4cd5b8-0x4d2074, 732) merge into one
(0x4c80d0-0x4d2074, 1,602 addresses, 37 functions) once mainLoop's far code is scanned; 0x83aa78-0x83ac30 is now
touched by 116 functions (99 before).

## Re-running

```
py -3.12 log/b1/gaps.py <module> [--tsv <list>]      # classification
py -3.12 log/b1/plan.py <module> [<list>]            # false-function passes -> log/b1/plan_<module>.json
py -3.12 log/b1/spec.py <module>                     # repair spec -> log/b1/spec_<module>.tsv
<analyzeHeadless> . SimGolf -process <module> -noanalysis -scriptPath ghidra/scripts \
    -postScript RepairBodies.java <spec> dry|apply <report>
py -3.12 log/b1/final.py <module>                    # created functions, reachability
```

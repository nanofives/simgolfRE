# S11.3, triage of every open C2 row (2026-11-01)

Deliverable: `re/roadmap/S11_c2_triage.tsv` (1,881 rows + header). Offline, read-only: no tracker writes, no game, no Ghidra.

## Open-row definition
Same as `scripts/re_classify.py status --summary` ("C2 still open: 1881"): hooks.csv `confidence` C2, `status` not dead / trivial,
and not in `re/c3_hold.tsv`. The generator re-implements that filter (`open_rows()`); its count is 1,881 and equals the tool's.
(hooks.csv has 1,993 `transcribed` + 1 `impl` C2 rows; 113 are held, which leaves 1,881. The `impl` row is clearBuffers, held.)

## Reproduce
```
~/bin/py -3.12 log/roadmap/S11/3_triage.py [--refresh]     # ~10 min cold (endstate + pointer scans), seconds with log/roadmap/S11/3_cache/
~/bin/py -3.12 log/roadmap/S11/3_handcheck.py > log/roadmap/S11/3_handcheck.log
```
Outputs: `re/roadmap/S11_c2_triage.tsv`, `log/roadmap/S11/3_summary.tsv` (counts per module x column), `3_mark_commands.txt`.
Rows are sorted by (module, addr); the run is deterministic (two runs, same bytes).

## Columns
module, addr (hooks.csv: VA for the exe, RVA for DLLs), name, size (functions_ghidra*.tsv), subsystem, hooks_status,
`endstate` (dead / trivial / none: endstate.py's verdict, dead additionally requires hooks.csv callers `none`, as `mark` does),
`endstate_dead_fail` and `endstate_trivial_fail` (each failing check with detail), `callers` (hooks.csv),
`import_callers` (Terrain.dll: exe functions found by import_callers.py), `installers` (pointer_callers.py: store or table
installers), `pointer_refs`, `reach`, `port_module`, `port_source`, `match`, `match_file`, `notes`.

reach: `called` (hooks.csv callers or import callers), `pointer-installed` (no caller, an installer found),
`pointer-no-installer` (a stored dword or table slot exists, no code stores the table), `no-caller`.

## Counts (open rows)
| module | rows | dead | trivial | none | called | pointer-installed | pointer-no-installer | no-caller |
|---|---|---|---|---|---|---|---|---|
| golf_clean.exe | 500 | 2 | 21 | 477 | 201 | 296 | 0 | 3 |
| jgld.dll | 337 | 58 | 3 | 276 | 240 | 33 | 5 | 59 |
| sound.dll | 561 | 27 | 4 | 530 | 231 | 295 | 4 | 31 |
| Terrain.dll | 483 | 101 | 4 | 378 | 343 | 33 | 5 | 102 |
| total | 1,881 | 188 | 32 | 1,661 | 1,015 | 657 | 14 | 195 |

Match (100% = a `// MATCH:` line in `re/match/*.cpp`; DLL lines converted VA -> RVA): exe 100% ours 2, smsg 293, none 205;
jgld ours 20, smsg 2, none 315; sound ours 71, smsg 218, none 272; Terrain none 483 (the open Terrain rows are the CRT block,
re/match/skip.tsv). `wip` is 0 in all four: no open row has only a re/match/wip line. Port module: exe from re/port/scope.tsv
(crt-dropped 276, ui 86, backend-audio 53, engine-glue 39, boot-only 19, backend-gfx 16, sim-core 7, formats-loaders 2, net 2);
sound.dll: 19 rows are in S4_functions.tsv (all family crt -> audio_crt), 542 are not; jgld.dll / Terrain.dll: `none-stated`
(the module-level target is in `port_source`). Full tables: `3_summary.tsv`.

## Dead / trivial
188 dead + 32 trivial = 220 rows pass endstate.py and are `transcribed`, so `mark` accepts them: commands in `3_merge.md`
(not run). endstate.py passed 4 more rows as dead that are not: the PE entry points (U-2020), excluded from the
mark list and shown as `none` with a note: golf_clean.exe 0x004a682f _WinMainCRTStartup, jgld.dll 0x00082770
_DllMainCRTStartup, sound.dll 0x000430fe __DllMainCRTStartup, Terrain.dll 0x00019b80 _DllMainCRTStartup.

### Hand check (`3_handcheck.py`, log `3_handcheck.log`): 11 verdicts + the entry point, all consistent
The check scans the FILE's section bytes (not the mapped image endstate.py uses) for E8/E9/Jcc rel32 landing on the body and for the
absolute VA as a dword anywhere, and prints the capstone disassembly.
- dead, 0 hits, disassembly read: exe `_strcspn` 0x004a7470 (62 B), exe `__seh_longjmp_unwind` 0x004a9039, Terrain `__threadid`
  0x10021b70, Terrain `setlocale` 0x1002a9e0, jgld `operator_new_array` 0x1007f110, jgld `_i64toa` 0x10089a40.
- dead, sound `Randomizer::set_range` 0x1001c730: the one hit is `e9` at 0x10001c76, its own incremental-link thunk (`e9 b5 aa 01 00`);
  a second scan for the thunk's address (call, jmp, dword) found nothing.
- trivial: exe `return0Pop4_402180` (`xor eax, eax; ret 4`), exe `returnZeroPop8_488cb0` (`xor eax, eax; ret 8`), Terrain
  `_CrtDefaultAllocHook` 0x1001a200 (`mov eax, 1; ret`, referenced by a data dword at 0x10064000), sound `__fpclear`
  0x10042505 (`ret`). The exe pair has .rdata dword references (vtable slots) while hooks.csv says callers `none`: the trivial
  rule does not need unreferenced, so this is as designed (they are live but unobservable).
- negative case: 0x004a682f is the exe's `AddressOfEntryPoint` (0x4a682f), so the "dead" verdict was wrong (U-2020).

## Findings for the S11 plan
- 296 of 500 exe rows are `pointer-installed`, and 280 of those have a 100% match (the largest group; 15 `called` rows also have one).
  These are vtable / callback bodies with a C4-grade match but no C3 (the C4 gate wants C3 first).
- sound.dll: 295 of 561 are `pointer-installed` (vtable slots of sound objects), 218 have an SMSG 100% match.
- jgld.dll / Terrain.dll `no-caller` 59 + 102: CRT debris and unused library members; 59 + 101 of them pass the dead check.
- 14 `pointer-no-installer` rows (jgld 5, sound 4, Terrain 5): a stored dword or table slot exists but no code stores its table.
  Not dead by the endstate rule (an uninstalled slot counts as a reference), not reachable by evidence: candidates for a hold.

## Open items (U-2020..U-2023, `3_uncertain.tsv`)
U-2020 endstate.py ignores PE entry points. U-2021 match CSVs in log/diff cannot be attributed to a module (sound.dll CSV names
carry no module tag: 00402180_Wave_In_Device_fade.match.csv sits beside the exe's 00402180), so `match` means a MATCH line in
re/match/*.cpp only; a first version with CSV colours showed 295 false "red" exe rows and was removed. U-2022 no file states a
jgld.dll / Terrain.dll function's port module. U-2023 542 of 561 open sound.dll rows have no S4_functions.tsv row.

Files: see `3_files.txt`.

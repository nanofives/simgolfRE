# SafeDisc call slots in the inter-function padding (batch b4, 2026-10-17)

Read-only analysis of the 716 unreachable `call` instructions batch b1 found in golf_clean.exe's padding
([UNCERTAIN U-0018], `re/analysis/data/function_boundaries.md` class H). Scripts and per-site tables are in
`log/b4/` (local): `sites.py` -> `residue_sites.tsv`, `decrypt.py` + `slots_decrypt.py` + `verdict.py` ->
`slots_decrypt.tsv`, `table.py` -> `evidence.tsv` (all 719 b1 residue rows with clean bytes, wrapped cipher, recovered
wrapped plaintext and the v1.00 bytes after the same function tail), `scan2.py` -> `v100_sites.tsv`, `refs.py`,
`realsites.py`, `pattern.py`.

## Result

The slots are SafeDisc protector output, written into the code caves before the `.text` section was encrypted.
They are not game code, not written by Safedisc2Cleaner, and no code, data or tool of this project uses them.
The port ignores them.

- In the region of `.text` this analysis can decrypt (0x00425990..0x004b942b, 619 of the 716 slots), the wrapped
  `golf.exe` already holds 566 of them: every recovered byte equals the cleaned byte. None of the 310 `call rel32`
  slots conflicts. 35 `call [IAT]` slots hold `FF 15` with a different IAT operand (see "Operand rewrites").
- `golf_v100_clean.exe` (SafeDisc 2.51.21, unwrapped by the same Safedisc2Cleaner build: both
  `tools/unwrap*/Safedisc2Cleaner_version.exe` hash `fad0f3f4...`) holds the same construct: 183 `call [IAT]` slots
  after a `ret`, in caves of 7..15 bytes, the rest of each cave filled with a copy of the bytes just before the slot
  (0x00401036: `ff15 10924b00` `f3ab5fc3` after the `f3ab5fc3` that ends the function). v1.00 has no `call rel32`
  slots and no dispatcher. The imports drawn are the same family in both builds (v1.00 top: _BinkNextFrame@4 13,
  _BinkClose@4 12, mmioDescend 11, _BinkBufferUnlock@4 11, mmioAscend 10; v1.03 top: _BinkBufferBlit@12 17,
  _BinkNextFrame@4 17, _BinkBufferUnlock@4 15, mmioAscend 15, mmioDescend 14).
- The slots are regenerated per protected build: of the 63 v1.00 slots whose 12 preceding bytes occur once in
  v1.03, 49 have a v1.03 slot at the same function end and none of the 49 calls the same import; the other 14 sit
  where v1.03 has plain 0x90 padding.
- jgld.dll, Terrain.dll, sound.dll (and binkw32.dll) carry no SafeDisc sections and no slot: b1's gap tables have
  no `residue` class for the three DLLs, and the signature scan below finds only 1-2 `ret`+`call` byte pairs per DLL,
  each followed by more code (jgld 0x1006c80b `call 0x1009c0f0; add esp, 8` inside a function), none followed by
  filler.

## The construct in golf_clean.exe

Every slot starts at the byte after a `ret` / `ret n` that ends a listed function (715 exactly at the range end;
0x004b04e9 one byte later: `jround_up` ends `ret` at 0x004b04e7, then one 0x90, then the slot), and the rest of the
cave up to the next entry is 0x90 (633), 0xCC (6) or empty (77, all in 5-byte caves). The cave size decides the kind:

| cave bytes (ret .. next entry) | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| `call rel32` (5 bytes) | 77 | 71 | 86 | 87 | 37 | | | | |
| `call [IAT]` (6 bytes) | | | | | 60 | 73 | 83 | 73 | 69 |

Caves of 0..4 and 14+ bytes hold none. 25 caves of 5..13 bytes after a `ret` hold none either: 19 13-byte caves
at 0x00493ec3..0x004b65c3 (the slots stop at 0x004b5d48), `_strchr`'s 11-byte cave 0x004a6165 (filled with `lea`
alignment instructions) and 0x004011a4 (12), 0x00402969 (7), 0x0046719a (6), 0x004785d8 (8), 0x00491d7b (5).

**`call rel32`.** The displacement equals `0x00405099 - (next function entry)`: it is the displacement of a
`call 0x00405099` that ends at the next entry, written at the start of the cave instead. The target as decoded
from the slot is therefore 0x00405099 minus the padding left after the slot: 0x00405099 (77, cave 5),
0x00405098 (71), 0x00405097 (86), 0x00405096 (87), 0x00405095 (37, cave 9). 0x00405095..0x00405098 are the last
displacement bytes and the `ret` of `FUN_00405010`'s `call 0x4a5713; ret` (0x00405093..0x00405098), so only the
cave-5 slots decode to a call of the dispatcher entry. The shifted slot 0x004b04e9 follows the same rule: target
0x00405097, two 0x90 bytes before the next entry 0x004b04f0.

**`call [IAT]`.** `FF 15` with an IAT slot of 0x004ba000..0x004ba260, over 101 imports (33 of them have no `FF 15`
call inside a listed function: Terrain methods reached through the exe's `jmp [slot]` thunks, GetKeyState,
DispatchMessageA ...). Per-import counts do not follow the real call sites (_BinkBufferBlit@12: 17 slots, 1 real
call; timeGetTime: 9 slots, 26 real calls).

**Dispatcher.** Three pieces, each filling a cave of exactly its own size:

| VA | cave | bytes | meaning |
|---|---:|---|---|
| 0x00403f9c | 4 | `90 25 8a 00` | dword 0x008a2590, after `initGlobalArray_00403f80` |
| 0x00404a14 | 12 | `b8 fceeffff 59 03c1 8b00 ffe0` | `mov eax, -0x1104; pop ecx; add eax, ecx; mov eax, [eax]; jmp eax`, after `drawText4049d0` |
| 0x00405099 | 7 | `51 50 e8 74f9ffff` | `push ecx; push eax; call 0x00404a14`, after `FUN_00405010` |

0x00405099 pushes ecx and eax and calls 0x00404a14, which pops its return address 0x004050a0 and jumps through
[0x004050a0 - 0x1104] = [0x00403f9c] = 0x008a2590. In both Safedisc2Cleaner runs logged on this PC
(`tools/unwrap/version.log.txt` lines 12-14, `tools/unwrap_v100/version.log.txt` lines 12-14) SecServ.dll
(`~df394b.tmp`, named in the log's `FreeLibrary_Hook: Unloading SecServ.dll (~df394b.tmp)`) was mapped at 0x00850000,
and the v1.03 run searched it up to 0x0091F000: 0x008a2590 lies inside it (offset 0x52590). Because SecServ loaded at
the same base in both runs, this value does not tell whether the dword was in the file or written at load.

## Wrapped golf.exe: what was in the encrypted file

`original/golf.exe` and `golf_clean.exe` share the section layout of `.text` (VA 0x00401000, raw 0x1000, virtual size
0xb842b); the wrapped file adds `stxt774` / `stxt371` and imports only 9 functions. On disk 2,682 of 754,731 `.text`
bytes are equal (chance level 754,731 / 256 = 2,948): the section is encrypted, so the raw bytes of the wrapped file
compare to nothing directly.

The cipher was measured against the cleaned plaintext (`log/b4/model*.py`). Above 0x00425990 (resolution 16 bytes,
inside a function, not at a boundary) each byte is `c = p XOR h(p[-4], p[-3], p[-2], p[-1])`: a table of the
previous four plaintext bytes (0x90 runs encrypt to 0x83 runs, xor 0x13). A table learned by majority vote over
0x00427000..0x004b942b agrees on 421,441 of 421,849 bytes (99.90%) with known context. Below 0x00425990 the same
table fails (0..1% agreement per 1 KB page) and windows up to 8 bytes reach at most 73% consistency: that region uses
another keystream, which this analysis does not recover.

`decrypt.py` recovers the wrapped plaintext at a slot by chaining that table from the four cleaned bytes before the
slot; a byte whose context the table lacks is left unknown and the cleaned byte is used as context for the next one.

| slots in the decryptable region | matches cleaned bytes | conflicts | no slot byte recovered |
|---|---:|---:|---:|
| `call rel32` (310) | 296 | 0 | 14 |
| `call [IAT]` (309) | 270 | 35 (operand only) | 4 |

Control on real import calls inside listed functions in the same region: of 150 `FF 15` / `FF 25` sites with a
recovered operand byte, 147 match and 3 recover bytes that fit no IAT call (0x004321db `ff ?? 11`; 0x0047bb46
recovers 0x19 for the opcode byte; 0x004a7503): the table errs on about 2% of such sites.

**Operand rewrites.** The 35 conflicting slots recover a byte of the IAT operand that differs from the cleaned
one (11% of 309, against the 2% error floor of the control). In three the whole wrapped dword is recovered and is
another slot of the same IAT: 0x004851a6 and 0x004a2fe6 wrapped [0x004ba1e0] TranslateMessage, cleaned
[0x004ba1c8] GetSystemMetrics / [0x004ba1a8] GetKeyState; 0x00487cb5 wrapped [0x004ba06c] RtlUnwind, cleaned
[0x004ba054] IsBadCodePtr. The cleaned operands of the 35 concentrate on GetKeyState (7), GetKeyboardState (5) and
GetSystemMetrics (4). Which writer changed them after encryption (the SafeDisc runtime at load, captured by the
cleaner's memory dump, or Safedisc2Cleaner's import pass) is not decided here.

## Evidence table (sample; all 719 rows in log/b4/evidence.tsv)

Wrapped cipher = raw bytes of `original/golf.exe` at the same file offset. Recovered = wrapped plaintext from the
cipher (`??` unknown). v1.00 = `golf_v100_clean.exe` bytes after the same 12-byte function tail, when that tail
occurs once there.

| VA | kind | cleaned bytes | target | wrapped cipher | wrapped recovered | verdict | v1.00 after the same tail |
|---|---|---|---|---|---|---|---|
| 0x00401036 | iat | `ff15c0a14b00` | MapVirtualKeyA | `1b12959e980a` | region A | not decryptable | `ff1510924b00` mmioAscend + `f3ab5fc3` (0x00401036) |
| 0x00401c27 | rel32 | `e869340000` | 0x00405095 | `46104a1c35` | region A | not decryptable | tail not found |
| 0x00403f9c | dispatcher dword | `90258a00` | 0x008a2590 | `49d996ba` | region A | not decryptable | tail not found |
| 0x00404a14 | dispatcher | `b8fceeffff5903c18b00ffe0` | [ret-0x1104] | `e3541204493468408eac6b25` | region A | not decryptable | tail not found |
| 0x00405099 | dispatcher entry | `5150e874f9ffff` | 0x00404a14 | `1fe53ed9652807` | region A | not decryptable | tail not found |
| 0x00407498 | rel32 | `e8f9dbffff` | 0x00405096 | `5537184e9c` | region A | not decryptable | `ff1534924b00` _BinkBufferUnlock@4 + `5bc3` (0x00407268) |
| 0x004289db | rel32 | `e8b9c6fdff` | 0x00405099 | `b093f847e8` | `e8b9??????` | matches | `ff1508924b` mmioDescend (0x004280f9) |
| 0x0042dbf8 | rel32 | `e89974fdff` | 0x00405096 | `4a4b6bd6f8` | `e89974????` | matches | tail not unique |
| 0x0042dd44 | iat | `ff1538a24b00` | _BinkGetRects@8 | `5d0152f39149` | `ff1538a24b00` | matches | tail not unique |
| 0x0042ef37 | rel32 | `e85961fdff` | 0x00405095 | `606ee9fbdb` | `????61????` | matches | `ff1540924b00` _BinkNextFrame@4 + `f85ec3` (0x0042e637) |
| 0x0042f336 | iat | `ff1540a24b00` | _BinkNextFrame@4 | `41792ada5386` | `ff1540a24b00` | matches | tail not found |
| 0x004492e7 | rel32 | `e8a9bdfbff` | 0x00405095 | `33dc3a569b` | `??????????` | no byte recovered | tail not found |
| 0x0042f1b3 | iat | `ff15c8a14b00` | GetSystemMetrics | `79219e5a1778` | `??15f4??????` | operand differs | `ff153c914b00` loadNewCourseType + 7-byte copy (0x0042e8b3) |
| 0x004851a6 | iat | `ff15c8a14b00` | GetSystemMetrics | `f053308f520e` | `ff15e0a14b00` TranslateMessage | operand differs | tail not unique |
| 0x00487cb5 | iat | `ff1554a04b00` | IsBadCodePtr | `40c8f9413d82` | `ff156ca04b00` RtlUnwind | operand differs | tail not found |

## Signature scan across the binaries (`log/b4/scan2.py`)

`ret` / `ret n`, then `E8 rel32` or `FF 15 abs32`, then filler up to the next 16-byte boundary that is all
0x90 / 0xCC (`pad`) or a copy of the bytes just before the call (`tailcopy`); `none` = the call ends on the boundary.

| file | pad | tailcopy | none | note |
|---|---:|---:|---:|---|
| golf_clean.exe | 641 | 0 | 81 | 722 hits at 720 addresses: the 716 b1 slots and 4 pairs at 0x0043287b, 0x0045c2cb, 0x0046e46b, 0x0047581b; rel32 targets of the `pad` hits only 0x00405095..0x00405098 |
| golf_v100_clean.exe | 0 | 183 (all `FF 15`) | 20 | the 20 `none` hits checked by hand at 0x004020ca, 0x00431f0b, 0x0045bb9b are followed by code |
| jgld.dll / Terrain.dll / sound.dll / binkw32.dll | 0 | 0 | 2 / 1 / 1 / 1 | code |

## References and tools

- No instruction in any listed function range (re/functions_ghidra.tsv after b1) has 0x00403f9c, 0x00404a14,
  0x00405095..0x00405099 or any slot address as a branch target or in its bytes. The only dwords in the image equal
  to one of those addresses are five copies of 0x00454c49 (a slot address), all inside ASCII text: `PCXFILE`,
  `TILE` (0x004c52c5, 0x004c9f28, 0x004ca152, 0x004ca347, 0x004ca583).
- `re/tools/xref.py` and `re/tools/port_survey_globals.py` disassemble listed function ranges only (the survey also
  skips call / jmp operands); no slot byte lies inside a range, so the IAT operands (0x004ba000..0x004ba260, inside
  the survey's DATA range) are not counted and the b1 survey numbers (1,812 functions, 5,589 data addresses) do not
  include them.
- `re/tools/undiscovered.py` uses `rel32` sites whose target is a listed entry or one of its thunks; 0x00405095..
  0x00405099 are not entries (`FUN_00405010` ends at 0x00405099, the next entry is 0x004050a0). If a function were
  ever created at 0x00405099, its 77 cave-5 slots would become sites outside every function; each would land in
  `unresolved`, since the byte before each slot belongs to the preceding function.
- `re/tools/pointer_callers.py` matches dwords equal to code addresses; slot operands are IAT addresses or
  displacements.
- b1's `log/b1/gaps.py` classes all 719 rows (716 slots + 3 dispatcher pieces) as `residue`, and its descent reaches
  none of them.

## Open

- Whether the dispatcher pieces and the region below 0x00425990 (97 slots) were in the encrypted file: needs that
  region's keystream, or a decrypted in-memory image of the wrapped exe taken before SecServ runs its import setup.
- Who rewrote the 35 slot operands (SafeDisc runtime or the cleaner's import pass): the same in-memory image
  compared with `golf_clean.exe` decides it.

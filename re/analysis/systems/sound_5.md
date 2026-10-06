# sound.dll — systems in range 0x1003a560 .. 0x10051319 (ID sound_5)

Round-2 naming pass over the 102 functions round 1 (sound_0/sound_1) left unnamed. The range splits
cleanly into two bodies: a self-contained floating-point audio **codec** (0x1003a560 .. 0x10041020,
game code, subsystem `audio`) and the **C runtime tail** (0x10041aad .. 0x10051319, subsystem `crt`,
mostly the fragments FidDb did not identify). Facts only; unverified items are in
`log/naming/sound_5_notes.txt`. Addresses are RVAs on image base 0x10000000.

## 1. Audio codec subtree (0x1003a560 .. 0x10041020)

A string-free numeric codec reached from exactly two sound-file loaders named in round 1:
`openMmioSound` (0x10030840) and `loadWaveResource` (0x100316c0). Every function in the subtree calls
the shared SEH prolog `FUN_100431ce` (`_SEH_prolog`, out of range) and uses the CRT math helpers
`sqrt` (0x10043204), `log` (0x100432e4), `__ftol` (0x10042550) and `_floor` (0x1004339c). There are no
string references anywhere in the subtree.

### Entry points and pipeline
- `codecProcessBlock` 0x1003a980 (caller `openMmioSound`): reads the config block `g_codecConfig`
  (0x10063ef0), then runs three stages in order — `bitUnpack` 0x1003c1d0 -> `codecTransformStage`
  0x1003b5a0 -> `codecSynthStage` 0x1003b230 — writing the output sample buffer (param_3).
- `codecProcessResource` 0x1003b150 (caller `loadWaveResource`): runs `bitPack` 0x1003c180,
  FUN_1003c220, `codecStageDispatch` 0x1003c640 and FUN_1003d040.

### Bit (de)interleaver
`bitPermute` 0x1003bf30 is the shared primitive (no callees, 578 bytes). It permutes 54 values through
the index table `g_codecPermTable` (0x1006447c) using the per-position bit masks `g_codecBitMasks`
(0x10064454). `param_1 == 1` unpacks a packed input array (param_6) into bit-field outputs
(param_3/param_4/param_5); `param_1 == 0` performs the inverse pack. The two SEH wrappers select the
direction: `bitUnpack` 0x1003c1d0 calls it with mode 1, `bitPack` 0x1003c180 with mode 0.

### Transform / synthesis stages
- `codecTransformStage` 0x1003b5a0 (2435 bytes): floating-point transform over the unpacked
  coefficients, converting to integer with `__ftol`; leaf helpers FUN_1003e2c0 (shared with
  FUN_1003c220), FUN_1003e480, FUN_1003e520.
- `codecSynthStage` 0x1003b230 (879 bytes): synthesis sub-tree FUN_1003d0a0, FUN_1003d1d0 (1779 B,
  `sqrt` + FUN_10040f60), FUN_1003d8d0 (`sqrt`), FUN_1003da50 (2150 B, `log` + a one-argument
  transcendental + `__ftol`).
- `codecStageDispatch` 0x1003c640 (2551 bytes): dispatches fourteen sub-stages between 0x1003e600 and
  0x10040da0 (FUN_1003e600/e6d0/e9f0/ec60[`sqrt`]/ed00/edc0/f310/f720[`sqrt`]/10040000/10040330/
  100404d0/10040740/10040ab0/10040da0). FUN_1003edc0, FUN_1003f720 and FUN_10041020 share the
  quantization helper `floatFloorToInt` 0x1003e400 (`_floor` + `__ftol`); FUN_10040ab0 and FUN_10041020
  share leaf FUN_1003e380.

### Codec object accessors (0x1003a560 .. 0x1003a800)
A small group of thin accessors on a codec/stream object whose concrete class is not identified; they
delegate to a sub-object embedded at `this+0x70` (bodies at 0x10037b00 / 0x10035770, matched in
`re/match/sound_raw_06.cpp`). Left unnamed except `getDsCoopLevel` 0x1003a770 (returns `g_dsCoopLevel`
0x100b4a3c). Details in the notes file.

### Globals (see `re/names/sound_5_globals.tsv`)
| addr | name | what |
|---|---|---|
| 0x1006447c | g_codecPermTable | 54-entry permutation index table for bitPermute |
| 0x10064454 | g_codecBitMasks | per-position bit masks for bitPermute |
| 0x10063ef0 | g_codecConfig | config/header block read by codecProcessBlock |

## 2. C runtime tail (0x10041aad .. 0x10051319)

The functions here are the CRT/library pieces FidDb left unidentified (the FID entries that do line up
are already applied in the master). Named by code shape, imports, strings and the FID-identified
neighbours that call them.

### Allocator and heap
- `heap_alloc` 0x10042b0b — core allocator body behind malloc/`_nh_malloc`; `__lock` +
  `___sbh_alloc_block`.
- `free` 0x10042c07 — `___sbh_find_block` + `__sbh_free_block` 0x10045f5a (coalesce, memcpy) under
  `__lock`.
- `calloc` 0x100458a1 — count*size with overflow guard (< 0xffffffe1), zero-filled.
- `operator_delete` 0x1004249a — forwards to `free` (called by ~150 destructors).
- SEH `__finally` unlock funclets that call `_unlock` 0x10047771 (= `LeaveCriticalSection` over the
  lock table `g_crtLockTable` 0x10067400): `heapAllocUnlockFunclet` 0x10042b72 (index 9),
  `freeUnlockFunclet` 0x10042c71, `reallocUnlockFunclet` 0x100453dd, `callocUnlockFunclet` 0x1004593a.
  0x10047771 is `_unlock` (one `LeaveCriticalSection(g_crtLockTable[index])`); round 1 had named it
  `crtStartupIo`, corrected in `re/names/sound_1.tsv` on 2026-10-06.
- `_alloca_probe` 0x1004aa10 (FID), `__free_lc_time` 0x1004e4b3 (FID).

### errno / fatal errors
- `_errno` 0x1004588f, `__doserrno` 0x10045898 — both return the address of a per-thread field via
  `__getptd`; distinguished by their caller sets (math/str vs file I/O).
- `_amsg_exit` 0x1004319b — fatal runtime-error reporter (`__FF_MSGBANNER` + `__NMSG_WRITE`).

### Math
- `sqrt` 0x10043204 and its special-value path `sqrt_classify` 0x1004320d (string (string in the binary) at
  0x10064ef0); `log` 0x100432e4 and `log_classify` 0x100432ed (string 'log' at 0x10064f00).
- FP-intrinsic forwarders reading ST0: `CIsqrt` 0x100431f0, `CIlog` 0x100432d0, and
  `CItranscendental1` 0x100432b0 (through `__ctrandisp1`).

### String
- `strcpy` 0x1004a010 and `strchr` 0x10045680, both using the 0x7efefeff dword zero-byte test.

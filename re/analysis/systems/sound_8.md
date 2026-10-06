# sound.dll — systems in the round-3 leftover set (ID sound_8)

Round-3 pass over the 55 functions rounds 1-2 (sound_0/1, sound_5) left unnamed in
0x1003e9f0 .. 0x10051319, plus the five earlier names that were withdrawn on 2026-10-06
(`re/names/withdrawn.tsv`). The set splits into the tail of the floating-point **audio codec**
(subsystem `audio`) and the **C runtime tail** (subsystem `crt`). Addresses are RVAs on image base
0x10000000. Facts only; unidentified functions are in `log/naming/sound_8_notes.txt`.

## 1. Audio codec DSP leaves (0x1003e9f0 .. 0x10041020)

The remaining leaves of the codec subtree documented in `sound_5.md`. They are reached from
`codecStageDispatch` 0x1003c640 (the fourteen-way sub-stage dispatcher), from the synthesis sub-tree
(`lpcSynthesize` 0x1003d1d0, `codecApplyFixedCoeffs` 0x1003d040), and from each other. All are
string-free float math and use the CRT helpers `sqrt` 0x10043204 and `__ftol` 0x10042550. Naming is
by the arithmetic each performs:

| addr | name | operation |
|---|---|---|
| 0x1003e9f0 | codecAutocorrelation | nested products sample[i]*sample[i-lag] -> autocorrelation/covariance matrix |
| 0x1003ec60 | codecRmsEnergy | sqrt(sum(x^2)/N) |
| 0x1003ed00 | codecRemoveMean | subtract mean(sum/N) from each sample |
| 0x1003f310 | codecGainSmoother | one-pole gain recursion g = x/c + g*c (coeffs 0x1005ec2c/ec04/ec4c, state at +0x2538) |
| 0x10040000 | codecPitchAnalysis | drives the AMDF pitch estimator 0x100414f0 and __ftol conversion |
| 0x10040330 | codecCorrelationLags | three cross-correlation values at lags 1..3, stride 2 |
| 0x100404d0 | codecSymmetricFir | linear-phase FIR out = x0*c0 + (x+1+x-1)*c1 + ... (taps 0x1005ec6c+) |
| 0x10040da0 | codecFirstOrderFilter | out = x - a*state; state = x |
| 0x10040e30 | codecBiquadFilter | two cascaded 2nd-order sections, coeffs 0x1005ecb0..0x1005ecc0, state param_4[0..3] |
| 0x10040f60 | codecCombFilter | 5-tap circular comb accumulate at param_1+0xbde (indices +0xbd8/+0xbdc) |
| 0x100414f0 | codecAmdfPitch | sum|x[i]-x[i+lag]| per lag, records min lag (*param_7) and max lag (*param_8) |

The primitives (autocorrelation, AMDF pitch, biquad/FIR/comb synthesis filters, gain smoothing)
are those of an LPC/speech vocoder. Four stages in the range could not be named from their
arithmetic alone (0x1003edc0, 0x1003f720, 0x10040740, 0x10040ab0, 0x10041020); see the notes file.

## 2. C runtime tail (0x10041aad .. 0x10051319)

### Small-block heap (two implementations behind malloc/free/realloc/calloc/_msize)

`g_crtHeapMode` (0x100b5d20, values 1/2/3) selects the active heap; `setSbhThreshold` 0x10045e34
(`_set_sbh_threshold`) initialises it on first use. Mode 3 is the **group heap** (the already-named
`___sbh_*` functions at 0x10045f2f..0x10046aff); mode 2 is the **region heap** documented here, which
reserves 4 MB address ranges with `VirtualAlloc` and commits 64 KB pages on demand.

- Lifecycle: `sbhGroupHeapInit` 0x10045ee7 (mode-3 region array), `sbhRegionCreate` 0x10046e50
  (mode-2 4 MB region, links `g_sbhRegionList` 0x100653d8), `sbhHeapTerm` 0x10045d6d
  (`VirtualFree`/`HeapFree`/`HeapDestroy`), `getModuleLinkerVersion` 0x10045b9b (reads the main
  module PE linker version to pick the implementation).
- Region heap internals: `sbhRegionLocate` 0x100470ac (pointer -> region + block index),
  `sbhFindFreeRun` 0x10047350 (scan a 0x3e-entry page bitmap for N free slots),
  `sbhResizeBlock` 0x10047474 (grow/shrink in place), `sbhPageFree` 0x10047103 (mark free, bump
  `g_sbhFreePageCount` 0x100b5590, decommit at 0x20), `sbhRegionDecommit` 0x10046fea (`VirtualFree`
  MEM_DECOMMIT of empty pages), `sbhRegionFree` 0x10046f94 (unlink + release a region),
  `sbhRegionHeapCheck` 0x1004751d (walk the list, return -1 on inconsistency).
- `_msize` 0x10045581 returns a block size through either implementation under heap lock 9.
- Heap-lock `__finally` unlock funclets (each `_unlock(9)` 0x10047771, one per mode path, matching the
  sound_5 naming of heapAllocUnlockFunclet/freeUnlockFunclet/etc): `heapAllocUnlockFunclet2`
  0x10042bd1, `freeUnlockFunclet2` 0x10042cc9, `callocUnlockFunclet2` 0x100459c3,
  `reallocUnlockFunclet2` 0x1004552b, `msizeUnlockFunclet` 0x100455eb, `msizeUnlockFunclet2`
  0x10045666.

### C++ exception / iostream support
- `EH_prolog` 0x100437e0 — the standard MSVC `__EH_prolog` SEH frame installer (callers are the
  ostream constructors/assignment operators).
- `iosResetVtable` 0x10041aad and `ostreamResetVtable` 0x10041c26 — destructor vtable-reset helpers
  for the ios/ostream classes (vtables 0x1005ecd4 / 0x1005ece4).
- `NLG_Notify` 0x10042482 — writes the NLG destination record `g_nlgDest` (0x10064ea8) used by the
  SEH longjmp/unwind debug notifications.

### Threading primitives
`initCriticalSection`/`deleteCriticalSection`/`enterCriticalSection`/`leaveCriticalSection`
(0x10041e43/e4e/e59/e64) — one-call wrappers around the Win32 critical-section APIs.

### Floating-point support
- `cfltcvtInit` 0x10042506 — installs the float-formatting function-pointer table `g_cfltcvtTable`
  (0x100651b8) used by printf.
- `fpuSetControlWord` 0x100481f5 — FLDCW with rounding kept (& 0x300) and all exceptions masked
  (| 0x7f).
- `doubleExponentField` 0x10048268 (callers CIsqrt/CIlog) and `fpStatusExceptionType` 0x1004820c
  (callers sqrt_classify/log_classify) — IEEE-754 exponent extraction and status-to-error mapping.
- `fp2ArgResultDispatch` 0x10048180, `fpTranscendentalDispatch` 0x10048643,
  `fpIntrinsicDispatch` 0x1004864a — x87 result/exception dispatchers that call `__87except`
  0x1004aecc (callers are the `__ctrandisp`/`__cintrindisp` tables); FSCALE denormal handling with
  the constants at 0x1005f278/0x1005f288/0x1005f298.
- `__fptrap` 0x1004a158 — calls `_amsg_exit(2)` (FP support not loaded).

### String and locale/time
- `strstr` 0x10042690 — substring search; single-char needle uses the 0x7efefeff dword zero test.
- `Strftime` 0x1004fe24 — the strftime worker: expands `%<spec>` (and `%#`) through `__expandtime`
  0x1004ff1e under locale lock 0x13.
- `cvtdate` 0x10051319 — resolves a daylight-saving transition date (365-day/leap math, month tables
  at 0x1006891c/0x10068950/0x10068920/0x10068954, mod-7 weekday); caller `__isindst_lk` 0x1005116d.

# S4.11 notes: zero fallbacks in the sound.dll port (module audio_crt)

Step row: re/roadmap/S4_steps.tsv S4.11. Merge request: log/roadmap/S4/11_merge.md. Uncertainties U-1540 .. U-1544:
log/roadmap/S4/11_uncertain.tsv. Files: log/roadmap/S4/11_files.txt.

Two workers: the first (cmvn2auzr5jwfo115qltauyzk, claude2) was closed at its session cap before writing notes; it had written
port/src/sim/audio_crt.cpp (28 functions), re/names/sound_19.tsv (19 rows), the systems writeup, the key table (11_keys.py), the
fixtures (11_fixtures.js), the mirror script and the census of the state WITHOUT audio_crt (11_census_nocrt_*). This worker (claude3)
re-read all of it, reproduced its last oracle run (47 keys, 29 GREEN, 18 RED, 0 fallbacks), found that every RED was a fixture or
harness problem and not a port defect, fixed them, added 14 keys, took the oracle to 61 / 61 GREEN and ran the whole S4 census.

## 1. What the module is

`port/src/sim/audio_crt.cpp`, 28 functions registered as `snd.<name>`; rvas are sound.dll's:

| group | functions |
|---|---|
| the four unnamed callees of loadSoundFile (U-1493, names in re/names/sound_19.tsv) | GlblHeader::init 0x2e3c0, Sound::getField1b8 0x5f40, MixBuffer::deletingDtor 0x2cf50, Sound::queueMidiCommand3 0x20480 |
| element constructor / destructor and a slot the iterators and MultiSound::set3DBuffer reach | ChannelDesc_dtor 0x244a0, VoiceBase_initListSlot 0x29b80, Ch5bb90_set3DBuffer 0x2c270 |
| heap | operator_new 0x424d7, _malloc 0x42acd, __nh_malloc 0x42adf, __callnewh 0x45b80, heap_alloc 0x42b0b, free 0x42c07, operator_delete 0x4249a, msize 0x45581, crtRealloc 0x45252 |
| locks | __lock 0x47710, _unlock 0x47771, _lockexit 0x45226, _unlockexit 0x4522f |
| exit table | _atexit 0x42647, __onexit 0x425c9 |
| strings | _strrchr 0x424b0, strstr 0x42690, _sprintf 0x42577 |
| arrays | ehVectorConstructorIterator 0x42902, ehVectorDestructorIterator 0x4280e, __ArrayUnwind 0x4288e |

Every call out of a body goes through the dispatcher (snd_call / snd_vcall / snd_call_va / snd_import), so a fixture stand-in, another
port function, the host or the AudioSink serves it. The system-heap arm allocates through the HeapAlloc / HeapFree / HeapSize /
HeapReAlloc imports (`snd_import`), so production reaches the AudioSink by name (S4.10 serves them from an arena). The core finds the
28 rvas through `audio_crt_function_name` (11_merge.md section 2). No other S4 file was edited.

S4.10's pending callees (return0_479820 and the NodeList / B / C destructors) are golf_clean.exe functions (re/names/model_h2.tsv, exe_4.tsv),
not sound.dll's: nothing to do here (U-1544).

## 2. Evidence: the S4.11 oracle (G1, layouts A / B / C, SG_EMU_ORACLE=1)

`~/bin/py -3.12 log/roadmap/S4/11_replay.py oracle --step S4.11 --out ...` (log/roadmap/S4/11_oracle_final.log / .tsv):
**61 keys, 672 vectors, 61 GREEN, 0 RED, 0 fallbacks, 0 errors, 0 x87 differences**. The keys: 57 custom (port/test/sound_keys_crt.py over
port/test/sound_fixtures/s411_crt.js) and 4 registry keys that tie to path-1 CSVs (strstr tier EXACT; _unlock, sound___ArrayUnwind,
ehVectorConstructorIterator_snd_none EQUAL_PI). Per-function table: log/roadmap/S4/11_function_keys.tsv. The default replay (no
emulator) of the pinned bundles passes on x64 and Win32; `pytest port/test/test_audio_crt.py`: 96 passed, 61 skipped (the opt-in oracle),
5 min (log/roadmap/S4/11_pytest.log).

### Fixes that took the first worker's 29 / 47 to 61 / 61 (all in the harness, none in a port body)

1. **Heap addresses differed by 0x10 between the arms** (operator_new, _malloc, __nh_malloc, heap_alloc at both g_newMode values,
   crtRealloc, __lock@create, Sound::queueMidiCommand3@dev, MixBuffer): the original arm runs first and takes a block from the fixture
   VM's heap, the port's host call then takes the next. `heapSI` (11_fixtures.js) stands in `KERNEL32!HeapAlloc` / `HeapFree` (and, per key,
   `HeapSize` / `HeapReAlloc`) with `__importStandIn`: HeapAlloc returns arena block (call number & 15) * 0x40, HeapReAlloc a 0x100 block
   from a second area, HeapSize a function of the block address. The import log, the arena and a cell are state regions (`HS` in the key
   table), so the call sequence and arguments are compared as well (a body that rounds the size differently, skips the free or frees the
   wrong pointer reads RED on the log).
2. **The failure paths were unreachable** (a system heap does not fail): `heapSI({after: true})` makes HeapAlloc / HeapReAlloc fail until
   the new-handler stub has run, with a cell protocol that keeps every vector self-contained (the stub returns the cell's second word
   and, when that is non-zero, sets the ok flag; the next Heap* call consumes it). New keys operator_new / _malloc / __nh_malloc `@fyes`
   (g_newMode 1, handler agrees), `@fno` (handler refuses), `@fmode0` (g_newMode 0 with a handler: _malloc never asks it), and
   crtRealloc `@hyes` / `@hno`.
3. **__onexit / _atexit growth** (`@full`): the table's block reports 0x80 bytes (HeapSize stand-in) so the table is full, and
   HeapReAlloc returns a block of the stand-in's arena; `@fail` adds the growth that fails (-1 from _atexit, 0 from __onexit).
4. **MixBuffer::deletingDtor vectors 8 / 9 faulted the ORIGINAL** (the fixture wrote a marker word into a slot the loop later reads as a
   record pointer: `mov eax, [eax]` at 0x1002cf69). The marker is gone; the slot stays NULL (the original frees NULL and skips the zero fill).
5. **The four raising registry keys** (c3fo / c3fz t0, t2, t3) end at `unmodelled import KERNEL32!RaiseException`: EXCLUDED, U-1540.
6. **Controls that were equivalent mutants** (reported BLIND by `negctl`): heap_alloc's mode 1 rounded to 8 after rounding to 16 (now rounds
   to 8 instead of 16: differs for sizes such as 0x11); operator_delete / free / MixBuffer::deletingDtor had no HeapFree stand-in log in
   their state, so a skipped delete was invisible (HS added); _lockexit's mode 1 (lock 0xe instead of 0xd) entered another section of the
   same kind (new fixtures `s411_lockexit_enter` / `_pair` put fixture critical sections in slots 0xd and 0xe, `$cs0` / `$cs1` are state).
7. **Branches the first 47 keys never reached**: `__lock@race` (a second thread fills the slot while this thread waits for lock 0x11:
   EnterCriticalSection stand-in), `heap_alloc@sbh3 / @sbh2` and `free@sbh3 / @sbh2` (g_heapType 3 / 2 with recording stubs for the nine
   small-block helpers and __lock / _unlock; U-1541).

### Coverage of the emulated originals (oracle --coverage, log/roadmap/S4/11_coverage.tsv)

Every conditional jump of 24 of the 28 functions has both sides in the emulated original (strstr executes 66 of 69 instructions: the
unrolled tail past a NUL the keys' haystacks never reach; the other 23 execute every instruction); the four others:
- `_sprintf` 0x425ad (the `count < 0` __flsbuf arm: needs 2 GiB of output; U-1542; 26 of 32 instructions execute);
- `__lock` 0x47735 (the malloc-failure arm calls `_amsg_exit(0x11)` and ends the process; U-1543);
- `msize` and `crtRealloc`, whose heap-type 2 / 3 arms are not ported (`MissingResource`; U-1541): msize 30 of 73 instructions,
  crtRealloc 61 of 278 execute. The type is 1 on every NT >= 5 (crtHeapSelect 0x10045bf1..0x10045c06) and setSbhThreshold, the only
  other writer, has no caller, so those arms cannot run in this build.

### Controls

`~/bin/py -3.12 log/roadmap/S4/11_replay.py negctl --step S4.11 --mode 1` (log/roadmap/S4/11_negctl_m1.log / .tsv, live oracle, layout A):
**28 / 28 RED, 0 BLIND** (each body documents its mode-1 detail: a skipped delete, an off-by-one size, lock 0xe for 0xd, the first
occurrence for strrchr's last, a missing pad character, ...). The default pytest repeats the 28 against the pinned goldens
(`test_negctl_mode1_red_against_golden`).

## 3. The gate: every S4 key with 0 fallbacks in every layout

Baseline (log/roadmap/S4/11_census_nocrt_*.tsv, the same runner on the mirror WITHOUT audio_crt): 4,800 fallback calls (all layouts)
over 59 keys: S4.5 12 (1 key), S4.6 240 (6), S4.7 315 (10), S4.8 dev 36 (12), S4.9 285 (6), S4.9b 3,912 (25); S4.2, S4.3, S4.4 and S4.8
ds had none.

Final (11_census2.sh: `oracle --step S4.k --update` for every step on the S4.11 mirror, layouts A B C, bundles and goldens into
the development directories port/corpus/audio_s411 and log/roadmap/S4/11_goldens; logs log/roadmap/S4/11_final_*.log):

| step | keys | vectors | result |
|---|---|---|---|
| S4.2 | 68 | 1,219 | 68 GREEN, 0 fallbacks |
| S4.3 | 57 | 628 | 57 GREEN, 0 |
| S4.4 | 57 | 2,001 | 57 GREEN, 0 |
| S4.5 | 113 | 1,840 | 113 GREEN, 0 |
| S4.6 | 168 | 3,344 | 168 GREEN, 0 (17 sNaN-forgiven vectors, U-1451, as accepted) |
| S4.7 | 117 | 2,146 | 117 GREEN, 0 |
| S4.8 ds | 70 | 1,215 | 70 GREEN, 0 |
| S4.8 dev | 104 | 1,316 | 104 GREEN, 0 |
| S4.9 | 247 | 2,404 | 247 GREEN, 0 (1 forgiven) |
| S4.9b | 25 | 74 | 25 GREEN, 0 (was 3,912 fallback calls) |
| S4.11 | 61 | 672 | 61 GREEN, 0 |

`~/bin/py -3.12 log/roadmap/S4/11_golden_compare.py` (tree goldens against the re-pinned ones, log/roadmap/S4/11_golden_compare.tsv):
**1,026 keys, 0 differ in digest list, tier, region list, x87 exit digests or vector count; fallbacks 4,800 -> 0, no pending entry left.**
The emulated original did not change on any key; only the port's calls did.

Default replay on the re-recorded bundles and goldens (11_replay_all.sh): x64 PASS on all ten families (11_replay_all_x64.log);
Win32 PASS on all ten as well (section 4).

## 4. Other gates

- **Win32 default replay** of the re-recorded bundles against the re-pinned goldens (log/roadmap/S4/11_replay_all_w32.log,
  11_replay_s49_w32.log): S4.2 68, S4.3 57, S4.4 57, S4.5 113, S4.6 168, S4.7 117, S4.9 247, S4.9b 25, S4.8 ds 70, S4.8 dev 104: 0 failures
  (S4.11 itself: 61 / 61 in 11_replay_w32.log and the pytest). x64: the same ten families 0 failures (11_replay_all_x64.log). The
  S4.9 Win32 line was re-run on the final build after a stray runner killed the first one; the other nine ran on a build whose only
  difference is six address comments rewritten as rvas (test_static's rule).
- **Segment gate** on the S4.11 mirror (`-m port.test.integrator segments`, 11_segments_x64.tsv / 11_segments_w32.tsv): x64 205 / 205 PASS,
  Win32 205 / 205 PASS; 0 rows differ from log/roadmap/S0/1_segs_{x64,win32}.tsv in status, digest or frames, none between the backends
  (11_segments_compare.log). The audio modules are not on the segment path (g_soundReady is 0 in every start image), so this shows the
  new module and the three shared edits do not disturb the build.
- **pytest** (one file per process): port/test/test_audio_crt.py 96 passed, 61 skipped (the opt-in oracle) in 3 min
  (11_pytest.log); port/test/test_audio_names.py: 7 passed, 1 failed, and the failure is S4.10's (74 `snd.FUN_` names in audio_exe.cpp
  that no names file lists: U-1498), none from audio_crt; port/test/test_static.py: audio_crt.cpp passes the address rule (it had six
  `0x1004....` comments, now rvas), audio_exe.cpp still has three literals (0x445000, 0x44414e, 0x400000) and test_every_module_file_is_listed
  fails for every file the integrator has not listed in modules.inc yet: both are other steps' to fix.
- The brief's rule on shared files held: only the files of 11_files.txt were created or changed.

## 5. How to reproduce

```
~/bin/py -3.12 log/roadmap/S4/11_mirror.py --w32                  # port/build/S4_11_mirror (x64, w32, runner)
export SG_PORTSIM_DLL=$PWD/port/build/S4_11_mirror/x64/Release/portsim_c.dll SG_S4_MIRROR=$PWD/port/build/S4_11_mirror
SG_EMU_ORACLE=1 ~/bin/py -3.12 log/roadmap/S4/11_replay.py oracle --step S4.11 [--update] [--coverage tsv]
SG_EMU_ORACLE=1 ~/bin/py -3.12 log/roadmap/S4/11_replay.py negctl --step S4.11 --mode 1
~/bin/py -3.12 log/roadmap/S4/11_replay.py replay --step S4.11 [--w32]
~/bin/py -3.12 -m pytest port/test/test_audio_crt.py                # one file per process
sh log/roadmap/S4/11_census2.sh $SG_PORTSIM_DLL                      # the whole census with --update into the dev directories (~2 h)
~/bin/py -3.12 log/roadmap/S4/11_golden_compare.py
sh log/roadmap/S4/11_replay_all.sh $SG_PORTSIM_DLL [--w32]
```
11_mirror.py `--no-crt` builds the state before this step (the baseline census, `11_census.sh`).

## 6. Open

- U-1540 .. U-1544 (11_uncertain.tsv): the SEH throw path of the vector iterators (4 registry keys excluded); heap types 2 / 3 in msize /
  crtRealloc; the sprintf conversions its callers cannot produce; __lock's process-exit arm; S4.10's pending callees are exe functions.
- The lead applies 11_merge.md sections 1-8 (modules.inc, two lines in audio_core.cpp, two in sound_replay.py, the S4_functions.tsv
  rows, the goldens, the tracker commands, UNCERTAINTIES.md, CLAUDE.md). Until then the new files in the tree are port/test/
  sound_keys_crt.py, port/test/sound_fixtures/s411_crt.js, port/test/test_audio_crt.py and port/test/golden_audio_crt.json (the
  last three are in the row's files; the first two are new files no sibling edits).

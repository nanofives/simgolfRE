# P3: the long session's bimodal memory growth (worker P3, 2026-11-01/02)

Brief: log/roadmap/P3/BRIEF.md (decision G21 part b). Base: e9f59779 (I22's tree + G21's 8 MB limit). Machine: LIVING-PC,
shared with S3 wave 10 (S3.30 / S3.31 / S3.32) and S11 runs, plus other projects' processes; every app run, build and gate took
one log/roadmap/S3/w8_lock.py hold (steps P3 / P3b / P3f / P3g / P3h).

## 1. Answer

Three things moved test_long_session_memory_bounded's number. None of them is from wave 9.

1. **The audio shadow, and whether the PC has an audio device. This decides the sign and the size.**
   `simgolf_port --audio on` (the default) runs the shadow audio context (S7.10) on its own thread.
   - With no output device, initSound fails, so the thread ends and frees the shadow before frame 2,000 (S7.11's device-less
     path). The test then measures the sim alone. That was the I21 / I22 condition.
   - With a device, the shadow lives until `audio.finish()`. main.cpp's live-trace block calls it BEFORE the test's last memory
     sample (`live_guarded` .. `audio.finish()` .. `streams.close()` .. `host.memory.sample(records)`).
   - `_growth` = (end sample, shadow already freed) minus (frame 2,000 sample, shadow alive). So with a device the number is the
     sim's growth minus the shadow's whole footprint at frame 2,000.
   - Since 2026-11-01 LIVING-PC has a render endpoint (VB-Audio `CABLE Input`, Status OK, section 5), so DirectSound opens a
     device. base1 read -19,914,752 B; I22's own facts run at 11:19 on 2026-11-01 read -19,984,384 B, the same shape. S7.10 and
     S7.11 never ran this path (U-1691 / U-1699: DSERR_NODRIVER, TV off).

2. **Three leaks of ours in the device sink, found through 1.** With a device, the shadow's memory grows with WALL time. base1
   committed +121,020,416 B between frame 2,000 and the sample before teardown. That is seven new 16,781,312-B regions (one per
   16 MB arena chunk) at frames 5k, 12k, 18k, 25k, 31k, 38k and 45k. The cause is audio_exe.cpp `DeviceSink`, S4's device model,
   which the app's DsSink answers through:
   - (a) IDirectSoundBuffer Release answered 1 and never freed the buffer's storage (from the arena) nor its interface object
     (audio_core.cpp `audio_sink_object`: a 64 KB block, 112 B per object, never reused). HeapReAlloc never freed the old block.
   - (b) A freed block was reused only by a request of exactly its rounded size, so sounds of many sizes left their freed blocks
     unused.
   - (c) mmioClose dropped the file record but not a buffer mmio had allocated itself (MMIO_ALLOCBUF, or a 'MEM ' file opened
     with no buffer). mmioSetBuffer did not free the own buffer it replaced either. Real winmm frees both.

   A sink that reports a buffer's end (the real device through ds_sink.cpp GetStatus, or Capture) lets the engine reap idle
   sounds and load them again on the next play. So every replay costs new storage and new mmio buffers. The arena holds at most
   8 chunks (kChunks, 128 MB), and base1 ended at 8 of 8 after 1,571 s. The next chunk request throws
   MissingResource("device sink arena full"), audio_out.cpp sets `broken_`, and the shadow plays nothing for the rest of the
   session; the sim continues. Model mode never reports an end and never reaps (4 buffers over repro 1), which is why S7.11's
   course_season_6y run in `--audio model` stayed in one chunk.

3. **Without audio the growth is the sim's own and smaller, and its two modes are the present log.**
   - `--audio off` grows +4,325,376 / +4,395,008 B. The heap's allocated bytes rise only +1,396,060 / +1,398,156 B. That is the
     present log of LiveTracePresenter (`std::vector<Present>`, 24 B per present, capacity 3,597 -> 61,447). Since S3 animated
     the course, every in-game frame presents: 51,109 presents.
   - The rest is heap commit that holds no live data. Each present makes three 960,000-B buffers: gfx surface_bytes, the
     files-overlay copy in MemoryGameFiles::put, and LiveTracePresenter::present's px. The log grew by MSVC's 1.5x reallocation
     through blocks of 655,440 and 983,160 B in between them. When a block finds no committed free hole, the NT heap commits
     fresh pages and keeps them.
   - The commit steps fall inside one heap segment: +962,560 (= 960,000 page-rounded) at frame 8-9k, another at 12-13k or
     18-19k, and +962,560 / +987,136 at 27-28k, where the log moves to its 983,160-B block. The log's last block of 1,478,656 B
     (61,447 x 24 = 1,474,728, page-rounded) comes at 40-41k.
   - **The 0.97 MB between the modes is the 27-28k step, present or absent.** Under `--audio-no-device` (I21 / I22's condition)
     the runs gave 3,407,872 / 4,300,800 / 4,403,200 B. With SG_RASTER_THREADS=1 the result was 4,399,104 B and with
     `--terrain headless` 3,407,872 B, so neither the raster workers nor the live terrain decide it.
   - I21's 3,293,184 / 3,317,760 / 4,280,320 B and I22's 4,284,416 B are the same two modes.
   - S7.8's 28,672 B was measured before S3 animated the course: few presents, so neither the log nor the 960,000-B churn.

## 2. The fix (log/roadmap/P3/edits.py, applied to port/build/P3f)

- **A** (audio_exe.cpp DeviceSink, audio_core.cpp / audio_sink.h):
  - Each buffer has a reference count. The last Release frees its storage and interface object (`audio_sink_object_free`: a
    free list per interface and slot count).
  - HeapReAlloc frees the old block.
  - The answers the engine sees do not change (AddRef / Release still answer 1).
- **A2**: before opening a new chunk, the arena merges address-adjacent freed blocks within a chunk. A free run at the bump top
  goes back to the bump, then the bump is used if it has room again, else the smallest freed block that fits (its rest stays
  freed). Inside the first chunk nothing changes, so a run that never fills a chunk (the pinned `$device` stream) gives
  identical lines.
- **A3**: mmioClose frees mmio's own buffer, and mmioSetBuffer frees the own buffer it replaces or drops (unbuffered).
- Control: registry entry `audio_exe.deviceHeap` (its adapter returns 0) carries `g_neg_deviceHeap`. Mode 1 frees nothing (the
  old behaviour) in A, A2 and A3.
- **B**: port/test/test_app_present.py `_growth` measures up to the last sample taken during the session, before
  audio.finish().
- **C**: LiveTracePresenter reserves the present log once, from the trace's frame count + 4096
  (`reserve_presents`; control `--negctl presents-no-reserve`).
- Tools and tests: simgolf_audio_check `--negctl <registry name>=<mode>` (an unknown name exits 2), and
  port/test/test_audio_device_heap.py (from log/roadmap/P3/test_audio_device_heap.py).

Repros without a game, through `simgolf_audio_check script --mode capture` (log/roadmap/P3/repro_*.script):

| repro | build | buffers | arena bytes (peak) | chunks | live bytes at the end | capture WAV sha256 |
|---|---|---|---|---|---|---|
| 1: 20 x (0x2c, 60 s, 0x80, 60 s) | e9f59779 (P3 mirror) | 42 | 115,602,288 | 7 | 108,277,828 | 31b075b7ae482dd... |
| 1 | fix A (P3f round 1) | 42 | 6,901,120 | 1 | 1,099,636 | 31b075b7ae482dd... |
| 1 | fix A + A2 + A3 (round 3) | 42 | 6,901,120 | 1 | 1,099,636 | - |
| 1 | fix, `--negctl audio_exe.deviceHeap=1` | 42 | 115,602,288 | 7 | 108,277,828 | 31b075b7ae482dd... |
| 1 | e9f59779, `--mode model` | 4 | 6,901,376 | 1 | 6,583,064 | - |
| 2: slots 0x00..0x80 with 60 s gaps, 3 passes | e9f59779 | 38 | 97,113,104 | 6 | 90,241,192 | - |
| 2 | fix A + A2 (round 2) | 38 | 33,554,432 | 2 | 19,807,826 | - |
| 2 | fix A + A2 + A3 (round 3) | 38 | 33,533,744 | 2 | 1,362,788 | - |
| 2 | round 3, control | 38 | 97,113,104 | 6 | 90,241,192 | - |
| 2, one pass | round 3 | 14 | 23,976,192 | 2 | 1,362,212 | - |
| 2, one pass | round 3, control | 14 | 32,503,024 | 2 | 31,075,728 | - |

The engine's output is byte-identical with and without the fix (the repro 1 WAV of 423,360,044 B), so the fix changes memory
only.

The decisive run is the long session with the device (run_longf.sh: course_season_6y, `--audio on`, this PC's device). Its
audio.json is the shadow's heap at the end:

| build | private bytes, frame 2,000 -> last periodic sample | arena at the end | live at the end | live peak | 16 MB steps |
|---|---|---|---|---|---|
| P3f round 2 (A + A2 + B + C), fix2_dev1 | 359,952,384 -> 463,523,840 | 106,796,304 | 104,031,974 (2,411 blocks) | 106,144,130 | 6 |
| P3f round 3 (A + A2 + A3 + B + C), fix3_dev1 | 359,923,712 -> 362,532,864 (+2,609,152) | 13,431,744 | 2,043,892 (693 blocks) | 7,469,740 | 0 |

(Round 1 = A + B + C, through test_app_present's long run: +103,481,344 B under part B's rule, 16 MB steps; gates/
present_bound_memory.tsv.) In fix3_dev1 the remaining ±1,802,240-B swings are one real DirectSound music buffer, live at some
samples.

## 3. Runs (log/roadmap/P3/runs/<name>/: memory.tsv, regions.tsv, meta.txt, session.json, audio.json)

The instrumented mirror is port/build/P3: git archive e9f59779 + junctions, plus measurement lines only, gated by SG_P3_MEMX /
SG_P3_REGIONS.
- memory.tsv gains: heap allocated / committed over GetProcessHeaps, heaps, threads, MEM_PRIVATE commit, private regions, private
  commit in regions >= 512 KB, and writable non-private commit.
- Two more samples, before audio.finish() and after it.
- regions.tsv lists every committed private region >= 64 KB per sample.

The app arguments are _long_run's (course_season_6y, --mem-every 1000), run under test-slot (BelowNormal), one w8_lock hold each
(run_long.sh / run_long2.sh; run_longf.sh for the fix mirror, which has no extra columns). framehash.bin equals
log/p2/p24/course_season_6y/c_a/framehash.bin in every run.

| run | build, variable | wall s | growth (old rule) | growth before teardown | notes |
|---|---|---|---|---|---|
| base1 | e9f59779 | 1,571 | -19,914,752 | +121,020,416 | 7 arena chunks added, 8 of 8 at the end |
| audiooff1 | e9f59779, --audio off | 1,149 | +4,395,008 | +4,395,008 | heap allocated +1,398,156; commit steps 8-9k, 12-13k, 27-28k, 40-41k |
| audiooff2 | e9f59779, --audio off | 1,389 | +4,325,376 | +4,325,376 | heap allocated +1,396,060; the same four commit steps |
| nodev1 | e9f59779, --audio-no-device | 1,353 | +3,407,872 | +3,407,872 | 3 steps: 8-9k +962,560, 18-19k +966,656, 40-41k +1,478,656; audio.json init_eax 3, device_less, thread_ended |
| nodev2 | e9f59779, --audio-no-device | 756 | +4,300,800 | +4,300,800 | 4 steps: 8-9k, 18-19k, 27-28k +962,560, 40-41k |
| nodev3 | e9f59779, --audio-no-device | 777 | +4,403,200 | +4,403,200 | 4 steps: 8-9k, 12-13k, 27-28k +987,136, 40-41k |
| nodev_threads1 | e9f59779, --audio-no-device, SG_RASTER_THREADS=1 | 698 | +4,399,104 | +4,399,104 | 4 steps; 1 thread in the process |
| nodev_headless | e9f59779, --audio-no-device, --terrain headless | 716 | +3,407,872 | +3,407,872 | 3 steps (8-9k, 18-19k, 40-41k); 1 thread |
| fix_nodev1 | P3f round 1, --audio-no-device | 1,018 | +962,560 | - | 1 step (8-9k) |
| fix_nodev2 | P3f round 1, --audio-no-device | 913 | +892,928 | - | 1 step (8-9k +995,328) |
| fix2_dev1 | P3f round 2, device | 634 | -22,384,640 | +103,571,456 (last periodic) | audio.json live 104,031,974 |
| fix3_dev1 | P3f round 3, device | 523 | -22,409,216 | +2,609,152 (last periodic) | audio.json live 2,043,892 |

Parts A2 / A3 do not touch the no-device path (its shadow never allocates in the device sink), so fix_nodev1 / 2 stand for round
3 there too.

## 4. Gates of the fix (port/build/P3f; log/roadmap/P3/gates/, summary g_gates.log)

`sh log/roadmap/P3/gates.sh <step>` (a copy of I22's gates.sh, env log/roadmap/P3/env.sh). Round 1 = A + B + C; round 3 = A + A2
+ A3 + B + C (the final edits.py). LT suffixes `_r3` / `_r3_k` in the file names.

| gate | round 1 | round 3 | against I22 |
|---|---|---|---|
| test_audio_device_heap (new) | 4 passed | 5 passed (with the many-sizes case) | new |
| test_audio_exe (pinned `$device` stream) | 302 passed, 5 skipped | 302 passed, 5 skipped | I22 suite: 302 passed, 5 skipped |
| test_audio_names | 12 passed | 12 passed | 12 |
| port test_static | 6 passed | 6 passed | 6 |
| test_app_audio | 26 passed, 2 failed (dsound shutdown) | 26 passed, the same 2 failed | see U-2050 |
| segments x64 | 205 / 205, 0 digests differ | 205 / 205, 0 digests differ | equal |
| segments Win32 | 205 / 205, 0 digests differ | 205 / 205, 0 digests differ | equal |
| boot x64 | HEAD's 70 rows identical, + golfer_events FAIL | HEAD's 70 rows identical, + golfer_events FAIL | see below |
| test_app_live | 13 passed | 12 + 1 failed twice, a different wall-clock test each time (see below) | 13 |
| test_app_present | 6 passed, 1 failed: memory +103,481,344 (round 1 had no A2 / A3) | 7 passed: memory +937,984, control +142,442,496 | |
| test_long_session_memory_bounded x 4 | stopped (round 1 failed) | 4 passed: memory +2,605,056 / +2,625,536 / +2,609,152 / +925,696 (the last in a 1,844-s session), controls +141,369,344 / +141,307,904 / +143,114,240 / +141,496,320 | |

- test_app_audio: I22 passed all 28 on the device-less path. The unfixed build fails the same 2 dsound shutdown tests with this
  PC's device (log/roadmap/P3/base_shutdown.log: handles 97->177, 171->165, 165->165, 165->165), U-2050.
- The memory test's five round-3 runs, all with this PC's device and part B's rule, give two values: about 0.93 MB or about
  2.61 MB. The 1.67-1.70 MB between them is one music buffer live at the last periodic sample or not (fix3_dev1's
  +-1,802,240 swings; U-2052). All are under GROWTH_LIMIT (8 MB) and every control is over 141 MB.
- test_app_live (round 3): first run test_presents_replay failed (Window::refreshFull 511 in the replay against 510 live, every
  present's frame equal); the rerun passed it and failed test_title_idle_cpu (6.56 % against 5.0 %, a SimCity 3000 game and
  another session's shell loop on the CPU). Each of the 13 passed in one of the two runs; I10 recorded the same refreshFull
  off-by-one under load and passed on a quieter rerun. A2 / A3 change only the shadow's allocator, and round 1 passed 13.
- boot x64: golfer_events.trace was added to the shared corpus on 2026-11-01 16:55, after I22 (U-1102: count HEAD's scenarios).

## 5. Facts used

- PnP AudioEndpoint (2026-11-01 23:xx): `CABLE Input (VB-Audio Virtual Cable)` OK, `Philips FTV` OK + Unknown, `CABLE
  Output` OK, `CABLE In 16ch` OK.
- Machine load: CPU 100 % for long stretches (msedgewebview2 about 1.2 cores; a Mashed match_permute run with 6 jobs at Normal; a
  SimCity 3000 game; Python runners). simgolf_port at BelowNormal got about 0.5 core in base1, so runs took 523-1,571 s against
  I22's 507 s.
- Install sounds: 244 WAVs, 93,674,390 B, 228 distinct sizes.

## 6. Reproduce

```
~/bin/py -3.12 log/roadmap/S3/w7_assemble.py --dst P3 --commit e9f59779 --plain     # + the measurement lines (section 3)
sh log/roadmap/P3/build.sh x64
sh log/roadmap/P3/run_long2.sh <name> [ENV=VAL ...] [-- <app args>]
~/bin/py -3.12 log/roadmap/P3/summarize.py --steps <name>
~/bin/py -3.12 log/roadmap/P3/regdiff.py <name> 2000 51092 --occ 0|1|2
~/bin/py -3.12 log/roadmap/S3/w7_assemble.py --dst P3f --commit e9f59779 --plain && ~/bin/py -3.12 log/roadmap/P3/edits.py port/build/P3f
sh log/roadmap/P3/buildf.sh x64; sh log/roadmap/P3/buildf.sh Win32
sh log/roadmap/P3/run_longf.sh <name> [-- <app args>]           # keeps audio.json
port/build/P3f/port/build/x64/app/Release/simgolf_audio_check.exe script --exe original/golf_clean.exe --install original \
  --script log/roadmap/P3/repro_many1.script --mode capture --stats <out.json> [--negctl audio_exe.deviceHeap=1]
sh log/roadmap/P3/gates.sh <step>...
```

## 7. Open

U-2050 (the dsound shutdown test with a device; held, does not block the merge), U-2051 (the residual ~0.96 MB step at frame 8-9k), U-2052 (the in-session
measure's spread with a device), U-2053 (the arena's bound is what is held at once). Rows in log/roadmap/P3/uncertain.tsv.

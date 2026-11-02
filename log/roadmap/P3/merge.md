# P3 merge notes (for the integrator)

## 1. Code: log/roadmap/P3/edits.py

`~/bin/py -3.12 log/roadmap/P3/edits.py <tree root>` on a clean `git archive e9f59779` tree. Checked: the result equals the fix
mirror port/build/P3f file for file. The parts:

- A, A2, A3: the device sink's leaks.
  - port/src/sim/audio_exe.cpp (DeviceSink): the last Release frees a buffer's storage and interface object; HeapReAlloc frees
    the old block; before a new chunk, freed blocks are coalesced and fitted; mmioClose / mmioSetBuffer free mmio's own buffers.
    New registry entry `audio_exe.deviceHeap` carries negctl `g_neg_deviceHeap` (1 = nothing freed).
  - port/src/sim/audio_core.cpp + port/include/portsim/sim/audio_sink.h: `audio_sink_object_free`.
  - port/app/tools/audio_check.cpp: `--negctl <name>=<mode>`.
  - New port/test/test_audio_device_heap.py.
- B (the measure): port/test/test_app_present.py `_growth` stops at the last sample taken during the session (before
  audio.finish()).
- C (the mode): port/app/host.h `LiveTracePresenter::reserve_presents`, and port/app/main.cpp reserves the present log once from
  the trace's frame count (control `--negctl presents-no-reserve`).

edits.py touches port/src/registry.cpp (MSBuild does not rebuild it after a registry change). B without A3 fails the memory test
on a PC with an audio device (round 1: +103,481,344 B), so take B only together with A, A2 and A3.

## 2. UNCERTAINTIES.md

Append the four rows of log/roadmap/P3/uncertain.tsv (U-2050..U-2053, P3's range given by the orchestrator; U-2040..U-2049
are S11's). U-2050 (test_app_audio's dsound shutdown test with a real device, e9f59779 too) is a held finding: it does not block
the merge (orchestrator, 2026-11-02).

Status changes:
- U-1692 -> resolved:
  `[P3] measured: with a device the shadow's arena grew to 8 of 8 chunks in one course_season_6y session (base1, 1,571 s): DeviceSink
  never freed a released buffer, reused a freed block only for its exact size, and kept mmio's own buffers. Fixed by P3 edits.py A /
  A2 / A3: the same session with the device holds a 13,431,744-B arena and 2,043,892 live bytes at the end (fix3_dev1,
  log/roadmap/P3/notes.md section 2).`
- U-1697: stays open, carried by U-2053.

## 3. CLAUDE.md paragraph (after the S7.13 / S3 wave 9 paragraphs)

```
Roadmap P3, the long session's bimodal memory (2026-11-02, notes log/roadmap/P3/notes.md, edits log/roadmap/P3/edits.py,
U-2050..U-2053, U-1692 resolved): test_long_session_memory_bounded's number depended on the PC's audio device and on the heap.
(1) The app takes its last memory sample after audio.finish(), which frees the shadow audio context: with no device the shadow
fails at initSound and is gone before frame 2,000, with one (LIVING-PC has VB-Audio `CABLE Input` since 2026-11-01) it is alive
at frame 2,000 and freed at the end, so the measure read -19.9 MB. (2) Through (1), three leaks of ours in audio_exe.cpp
DeviceSink, which the app's DirectSound sink answers through: Release kept a buffer's storage (16 MB-chunk arena, at most 8) and
interface object (64 KB block), a freed block served only a request of its exact rounded size, and mmioClose kept mmio's own
buffer. A sink that reports a buffer's end (the real device, Capture) lets the engine reap idle sounds and load them again, so the
shadow grew with wall time (base1: 8 of 8 chunks after 1,571 s) until MissingResource silenced it. Fixed (edits.py A / A2 / A3,
control registry `audio_exe.deviceHeap` 1): the same session with the device holds a 13.4 MB arena and 2.0 MB live; capture WAVs
are byte-identical; port/test/test_audio_device_heap.py. (3) With audio off the growth is the sim's: the present log (24 B per
present; every in-game frame presents since S3) reallocated by 1.5x among three 960,000-B buffers per present, and whether its
983,160-B block found a free hole is the 0.97 MB between the modes (no-device runs 3,407,872 / 4,300,800 / 4,403,200;
SG_RASTER_THREADS=1 and --terrain headless show both modes). LiveTracePresenter reserves the log from the trace's frame count
(no-device growth 892,928 / 962,560), and the memory test measures up to the last in-session sample. test_app_audio's dsound
shutdown test fails with a real device on any build (cycle 1 releases 6 handles, U-2050).
```

## 4. Files

log/roadmap/P3/files.txt.

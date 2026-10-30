# S3 wave 6 merge (S3.12): S3.10 + S3.10b + the LiveTerrain log switch, on e179bd24

Notes: log/roadmap/S3/12_notes.md. Files: log/roadmap/S3/files_w6.txt. Base: e179bd24 (the orchestrator's fix of 01d84188, whose
committed gfx_terrain.cpp did not compile against its gl_raster.h; 12_notes.md section 1). Reference of every gate: plain e179bd24
(port/build/S3_12_e179, builds x64 + Win32), whose port/ equals I13's mirror (so I13's tables are its results).

## 1. What the wave brings

- S3.10 (rasteriser workers, SG_RASTER_THREADS, fast pixel loop with checked reciprocals; 10_notes.md): gl_raster.cpp / .h,
  test_gl_raster.py as S3.10 left them, plus this step's worker control `#workerctl` (GlRaster::worker_ctl); gfx_terrain.cpp gets
  S3.10's thread calls (raster_threads, set_threads, finish, SG_TERRAIN_PASS_LOG) merged onto S3.8's file, plus the comment of the two
  new terrain-family controls.
- S3.10b (Terrain-state rebuild speed, no float changed; 10b_notes.md): terrain_state.cpp, test_terrain_state.py as S3.10b left them.
- The LiveTerrain log switch (U-1673, approved by the S3 lead and S7): terrain_live.cpp / .h = 10b_live's lazy log (log_size, drop_log)
  plus S7.8's requested set_keep_log / calls() and controls 6 / 7; port/app/host.cpp TerrainLogBound over them (no const_cast), host.h's
  TerrainLogBound comment.
- The app tests' facts path made a parameter: test_app_ingame.py, test_app_live.py and test_app_present.py take SG_APP_FACTS_DIR
  (U-1398 resolved), and test_app_ingame's stale zoom xfail is dropped (approved 2026-10-29; the test becomes strict).
- No shared-file edit outside these: no module, registry, CMake, C ABI or modules.inc change.

## 2. Apply (exact edits, in order)

All 130 edits are in log/roadmap/S3/12_edits.py (list of (path, old, new), applied in list order, each old text exactly once), generated
on e179bd24's blobs. Apply to a tree at e179bd24 with

```
~/bin/py -3.12 log/roadmap/S3/12_assemble.py --root <tree>            # edits + section 5 rows + section 6 paragraph
~/bin/py -3.12 log/roadmap/S3/12_assemble.py --root <tree> --no-docs  # edits only
```

Order and count: port/src/sim/gl_raster.cpp 13, port/include/portsim/sim/gl_raster.h 8, port/test/test_gl_raster.py 1,
port/src/sim/gfx_terrain.cpp 6, port/src/sim/terrain_state.cpp 81, port/test/test_terrain_state.py 1, port/src/sim/terrain_live.cpp 5,
port/include/portsim/sim/terrain_live.h 3, port/app/host.cpp 5, port/app/host.h 2, port/test/test_app_ingame.py 2,
port/test/test_app_live.py 1, port/test/test_app_present.py 2. Each file's result equals its target (12_make_edits.py asserts it): the
step trees' files for gl_raster / test_gl_raster (S3.10, with the worker control) and terrain_state / test_terrain_state (S3.10b),
log/roadmap/S3/12_src/ for the rest.

THE SHARED TREE: its gl_raster.cpp / .h, test_gl_raster.py, gfx_terrain.cpp, terrain_state.cpp, test_terrain_state.py hold S3.10's /
S3.10b's uncommitted copies, so `--root .` there refuses the edits whose new text is already present. Apply to a clean checkout of
e179bd24 (or first put those six files back to e179bd24's content, file by file), then apply; the result equals port/build/S3_12.

## 3. To S7 (its files host.cpp, host.h, test_app_ingame.py, test_app_live.py, test_app_present.py; approved with U-1673 / 2026-10-29)

- host.cpp TerrainLogBound::frame_end: counts log_size() (no formatting), drop_log()s the boot's first-frame lines, then
  set_keep_log(false): LiveTerrain records no call afterwards; calls() = LiveTerrain::calls(); length() = log_size(). `--negctl
  terrain-log-keep` keeps every line as before.
- SG_APP_FACTS_DIR=<dir> puts 8_app_ingame_facts.json / 8_app_live_facts.json / 8_present_facts.json there (default unchanged: the
  tracked log/roadmap/S7/ files). One line each (test_app_present also imports os).
- test_app_ingame.py: the `xfail(reason="U-1640 ...")` of test_terrain_pass_frames_fit_the_frame is removed, replaced by a comment naming
  wave 6 and the measured zoom frames; the test now fails if any in-game frame works 87 ms or more.

## 4. Re-checks after merging (cheap)

```
(cd <tree> && SG_PORTSIM_DLL=<x64 dll> ~/bin/py -3.12 -m pytest port/test/test_gl_raster.py port/test/test_terrain_state.py -q)
SG_TREE=<tree> SG_PORTSIM_DLL=<x64 dll> ~/bin/py -3.12 log/roadmap/S3/12_frames.py <out.tsv> --line "#threads 4"
~/bin/py -3.12 log/roadmap/S3/12_frames.py --compare log/roadmap/S3/10_frames_old.tsv <out.tsv>      # 0 rows differ
(cd <tree> && SG_PORTSIM_DLL=<x64 dll> ~/bin/py -3.12 ../../../log/roadmap/S3/12_ctl_live.py sandbox_basic)   # modes 6 / 7 RED
```

## 5. UNCERTAINTIES.md

### replace

```
| U-1673 |  | method | [S7.8, kind `method`] host.cpp TerrainLogBound dropped LiveTerrain's log lines through a const_cast on LiveTerrain::log(). Resolved in roadmap S3 wave 6 (S3.12): LiveTerrain keeps compact records formatted only when log() is read (S3.10b), log_size() / drop_log() count and empty them, set_keep_log(false) records no call and calls() counts every call (S7.8's request, 8_merge.md section 3); TerrainLogBound uses them (no const_cast). Evidence: test_terrain_live (call logs line for line), test_app_present (log bound, keep control), log/roadmap/S3/12_ctl_live.py controls 6 / 7 RED. | - | resolved |
| U-1398 |  | process | test_app_ingame / test_app_live / test_app_present wrote their measured facts to ROOT/log/roadmap/S7/, and a mirror's log is a junction to the shared log, so every mirror run rewrote these committed files in the shared tree. Resolved in roadmap S3 wave 6 (S3.12): all three take SG_APP_FACTS_DIR=<dir> (default unchanged); with it set, S3.12's whole x64 suite on the wave 6 tree rewrote no tracked log/roadmap/S7/ file (git status clean; the facts are in log/roadmap/S3/12_out/S3_12/suite/facts_test_app_*). | - | resolved |
```

### append

```
| U-1388 |  | performance | [roadmap S3.10 / S3.10b] The zoom keys' frame rebuilds the Terrain state (14,804 calls: resetTerrain 1, setType 2,500, elevateCorner 9,801, calcNormals 2,500). S3.10b made terrain_state.cpp resolve the Terrain object once per exported call and access it in place (no float changed); the rebuild's microbenchmark minima fell from 18.8..21.0 to 11.7..12.3 ms per burst. With wave 6 merged on e179bd24 (S3.10 workers, S3.10b, the LiveTerrain log switch; roadmap S3.12, LIVING-PC Ryzen 5 3600, MSVC 19.44 Release x64) the zoom frames of three interleaved app sessions worked 39..58 ms at 54..82 % load against 133..175 ms for plain e179bd24 at 59..96 %, and on an idle PC (1..9 % load, test_app_ingame) every terrain-pass frame worked 10..30 ms against 25..112 ms. | - | resolved |
| U-1389 |  | evidence | [roadmap S3.10] GlRaster's checked reciprocal path keeps a pixel only when every rounding decision is farther from its boundary than the reciprocal and exact computations can differ (kTexEps 1e-9 on texel fractions below 2^16, kQuantEps 1e-6 on the quantiser, bounds argued in gl_raster.cpp above fast_triangle_). The control without the checks (`#fastpath 3`) changes no pixel of the 268 recorded passes (S3.10, and again in S3.12's 12_equiv.py on both bases): the checks are justified by the error bound, not by a recorded pixel. The control with float-rounded reciprocals (`#fastpath 4`) changes pixels on 47 passes. | A constructed stream whose pixels sit within 1e-15 of a quantisation or texel boundary would judge `#fastpath 3` directly | open |
| U-1399 |  | harness | [roadmap S3.12] The app's live-trace path (`simgolf_port.exe --live-trace cov_a_keys`, test_app_ingame.app_live_trace) does not give the same frame at checkpoint k_dlg_J (context MODAL:waitTicks) in every run: over 6 runs its snapshot hashed 019371043c52 five times and ce732697574a once, on the wave 6 tree with the rasteriser workers AND on a build without workers whose port/ equals plain e179bd24 (01d84188 + I13's gfx_terrain.cpp; log/roadmap/S3/12_determinism_t4.log, 12_determinism_head.log); the other 22 checkpoints of cov_a_keys were the same in all 6. The second value is 33 px away from framegate's in-process frame, so test_app_ingame.test_app_path_equals_framegate_and_its_present fails when it occurs (once in S3.12's 9 test_app_ingame runs). Not caused by wave 6; the cause (which input of the live-trace path differs between runs inside that modal wait) is not measured. | Record the live core's answers (clock reads, cursor reads, dispatch points) of a varying and a stable run up to k_dlg_J's ordinal and diff them | open |
```

## 6. CLAUDE.md paragraph (after S3.14's, before `Phase 7 defect decisions`)

```
Roadmap S3 wave 6, S3.10 / S3.10b / the LiveTerrain log switch (2026-10-28 / 29, step S3.12, notes log/roadmap/S3/12_notes.md, merge
log/roadmap/S3/MERGE_w6.md, U-1388 / U-1389 / U-1399 / U-1673 / U-1398). Base e179bd24: 01d84188's committed gfx_terrain.cpp held S3.10's
thread calls while its gl_raster.h did not (7 C2039 / C2737 errors on x64 and Win32, all in that file; no other committed file used
working-copy-only code), and e179bd24 put I13's file back. S3.10: GlRaster's default pipeline runs a fast pixel loop that writes the old
loop's bits (edge functions exact as int64 in units of 2^-16, each row's covered span from the three inside tests, each pixel first with
reciprocals and kept only when every texel and 5-bit rounding decision is clear of its boundary) on pixel-loop workers that own interleaved
4-row bands (GlRaster::set_threads; gfx_terrain.cpp starts min(4, hardware threads / 2), SG_RASTER_THREADS=<n>); stream lines `#fastpath
<m>`, `#threads <n>` and the worker control `#workerctl 1`; SG_TERRAIN_PASS_LOG times each pass. S3.10b: terrain_state.cpp resolves the
Terrain object once per exported call and reads and writes it in place (VPtr), no float changed. LiveTerrain keeps a compact record per
call and formats the lines only when log() is read; log_size() / drop_log() / set_keep_log(false) / calls() let the app's TerrainLogBound
bound the log without a const_cast (U-1673). The app tests take SG_APP_FACTS_DIR (U-1398), and test_app_ingame's zoom test is strict (its
U-1640 xfail removed). Evidence against plain e179bd24 (whose port/ equals I13's mirror): all 299 recorded stream and chain passes
bit-identical to the old rasteriser with and without 4 workers, census 698 rows PASS 100 with every row's pixel count unchanged, terrain
527 / 527, segments 205 / 205 and boot 66 / 2 / 2 on x64 and Win32, live equivalence 68 + 2 and app replay 3 / 3, all with 0 rows
differing; port suite x64 82 / 82 files, Win32 the 22 known ids; controls RED: `#workerctl 1` on 270 / 299 passes, `#fastpath 4` on 47 /
268 (`#fastpath 3` blind, U-1389), terrain_state 23, LiveTerrain 6 (log off) / 7 (setType variation lost). App (LIVING-PC, Ryzen 5 3600,
MSVC 19.44 Release x64, three interleaved sessions): zoom frames 39..58 ms at 54..82 % load against 133..175 ms for plain e179bd24 at
59..96 %; idle (1..9 %) every terrain-pass frame 10..30 ms against 25..112 ms. The app's live-trace frame at cov_a_keys k_dlg_J varies in
about 1 run of 6 without workers too (U-1399).
```

## 7. Gate block

Tree port/build/S3_12 (12_assemble.py: `git archive e179bd24` + 12_edits.py), built x64 and Win32 with port/test/build.py and
SG_FREETYPE_ROOT. LIVING-PC (Ryzen 5 3600), MSVC 19.44 Release. Reference: PLAIN e179bd24 = port/build/S3_12_e179 (builds x64 + Win32; its
port/ src / include / app / test / CMakeLists equal port/build/I13's, so log/roadmap/merges/I13/'s tables are its results). Outputs
log/roadmap/S3/12_out/S3_12/ and 12_out/pairs_e179/ (2026-10-29 01:21..08:22, log/roadmap/S3/12_run_all.sh).

```
edits                 130 / 130 apply exactly once (S3_12 and a scratch root of e179bd24's files); a second apply is refused
builds                wave 6 x64 + Win32 0 errors; plain e179bd24 x64 + Win32 0 errors; plain 01d84188 7 errors (gfx_terrain.cpp only)
recorded passes       299 / 299 bit-identical to the old rasteriser, default and `#threads 4`; 268 + chain 0 differ from the general loop
terrain census        527 / 527 PASS, every row equal to plain e179bd24's and S3.2's
segments              x64 205 / 205, Win32 205 / 205, 0 rows differ (digests included), x64 = Win32
boot                  x64 and Win32 66 PASS / 2 KNOWN / 2 NO_FRAMES, 0 rows differ
framegate census      698 rows, PASS 100 / FAIL 537 / UNSUPPORTED 51 / NOT_RUN 10 as the reference, 0 PASS lost, total_px equal on 688 / 688
live equivalence x64  68 PASS + 2 NO_FRAMES, 0 rows differ;  app replay x64 + Win32 3 / 3, 0 rows differ
repo static           614 passed, 2 skipped
port suite x64        82 / 82 files PASS; vs the reference: test_gl_raster 177 -> 211, test_terrain_state 500 -> 501, test_app_ingame
                      12 + 1 xfailed -> 13 passed (strict zoom test), test_fttext 21 (U-1393's 2 pass with today's host), 78 files equal;
                      app files: ingame 13, input 16, live 13, present 7, replay 14, shell 18
port suite Win32      I7's 59 files: 22 failed ids = I10's 22 known, 0 new; test_live 39 passed. test_gl_raster / test_terrain_state call
                      the DLL through ctypes and cannot run on the Win32 runner (error at setup, as on the reference); their Win32 code
                      is judged by test_x64_equals_win32_terrain_pixels (PASS), segments and boot on Win32
test_app_ingame       idle pairs (load 1..9 %): plain e179bd24 12 + 1 xfailed twice (terrain-pass frames 25..112 ms), wave 6 13 passed
                      twice (10..30 ms); in the x64 suite (load 63 %) wave 6 13 passed
zoom frames           3 interleaved app sessions: wave 6 39..58 ms (load 54..82 %), plain e179bd24 133..175 ms (59..96 %);
                      arrow keys 13..23 against 7..86 ms
controls RED          #workerctl 1: 270 / 299 passes (frames), 239 / 268 (equiv); #fastpath 4: 47 / 268; #fastpath 3: 0 (blind, U-1389);
                      terrain_state 23 (in test_terrain_state 501); LiveTerrain 6 (log off): 0 port calls, 7 (setType var lost): call 4
                      differs, on sandbox_basic and cov_k1_terrain; app terrain-log-keep (test_app_present)
facts files           no tracked log/roadmap/S7/ file rewritten (SG_APP_FACTS_DIR); git status log/roadmap/S7 clean
U-1673, U-1398        resolved (section 5);  U-1399 new, open (not caused by the wave)
```

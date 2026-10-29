# S4.12: S4's resubmission proof rebuilt on HEAD 01d84188 (I11's refusal)

Worker S4.12 (claude2), 2026-10-28/29. Lead cmvsixi81044xp727qsgerjz6. Row S4.12 of re/roadmap/S4_steps.tsv.
Result: DONE. Every gate of the row passes on the mirror port/build/S4r2 = 01d84188 + S4's 86 paths + `2b_apply_shared.py`, with one
substitution 01d84188 forced (U-1546: its gfx_terrain.cpp did not compile against its gl_raster.h; the mirror took port/build/I13's
copy, the file I13 gated). Commit e179bd24 made the same fix during the run, and 6e6df6e7 touched no file a gate reads, so S4r2 is
6e6df6e7 + S4 for the gates (section 3b; --prove SAME on 6e6df6e7). Every comparison against I13 (and I12 where I13 did not run a gate)
reads 0 rows differing.
BASE OF RECORD (lead, 2026-10-29): every number below is for e179bd24 + S4. e179bd24 = 01d84188 + gfx_terrain.cpp set to I13's file
(`git diff --stat 01d84188 e179bd24` lists that file and log/roadmap/ORCH_LOG.md only), and gfx_terrain.cpp's md5 with CR stripped is
e88dfcab04344287b3bc5643cd755ee0 in e179bd24, port/build/I13 and port/build/S4r2, so S4r2 = the e179bd24 archive + S4 (written in
port/build/S4r2/HEAD_COMMIT.note; HEAD_COMMIT itself still reads 01d84188, the archive it was built from). The nine shared files are
equal in 01d84188 and e179bd24, so the --prove on 01d84188 holds for e179bd24 (and it was re-run SAME on 6e6df6e7).
The draft for MERGE.md is log/roadmap/S4/12_merge_proof.md.

## 1. Setup (commands from the shared root)
1. `~/bin/py -3.12 log/roadmap/S4/2b_apply_shared.py --prove` (the lead's fixed tool: I3's portsim_c check relaxed to sg_build_info's
   tail): SAME on all 8 files at 01d84188 (resub2/prove.log): modules.inc 12 added, portsim_c.h 22, portsim_c.cpp 1 removed / 18
   added, tick.cpp 3 removed, tick_boot.inc 2 / 4, UNCERTAINTIES.md 89 (S4 88 + 1 extra = U-1545), CLAUDE.md 70 (45 + 25 = the wave-2
   paragraph), port/CORE.md 7.
2. `~/bin/py -3.12 log/roadmap/S4/resub2/mirror.py --dst S4r2` (resub2/mirror.log): git archive of 01d84188, the ten junctions
   (original, log, port/corpus, tests/golden, tests/fixtures, re/versions, tools/vc6, shim/build, re/replay, extracted), the nine
   shared files from the working tree (each unmodified against HEAD, the tool checks), the 86 paths of log/roadmap/S4/files.txt.
   - The 86 working-tree copies equal port/build/S4r's (the 78ffb06d proof) byte for byte except re/roadmap/S4_functions.tsv (the
     apply script edits S4r's copy) and re/roadmap/S4_steps.tsv (the lead's status column).
   - None of the 86 paths changed between 78ffb06d and 01d84188 (I11 checked; the S4r comparison agrees).
   - S3.12's uncommitted working-tree edits (gl_raster.h / .cpp, terrain_state.cpp, test_gl_raster.py, test_terrain_state.py) are
     not in the mirror: each mirror file equals `git archive HEAD <path>` byte for byte.
3. U-1546 (log/roadmap/S4/12_uncertain.tsv): the first x64 build of the archive failed, C2039 at gfx_terrain.cpp(127)
   (`threads` / `set_threads` / `finish` / `Stats::fast_px` are not members of GlRaster). HEAD's committed gfx_terrain.cpp carries
   S3.10's rasteriser-worker edit (+42 / -1), while HEAD's gl_raster.h / .cpp do not have that API. The working-tree file's mtime is
   16:33:02 and the commit's date 16:40:54. port/build/I13's gfx_terrain.cpp has no such edit. Of the 1,229 tracked files under port,
   shim, scripts, tests, re/tools and re/frida (corpus excluded), it is the only one where HEAD differs from port/build/I13 (CR
   stripped; script in this file's history: git show HEAD:<f> against port/build/I13/<f>), and port/build/I13 holds no source file under
   port/src, port/include, port/test or port/app that HEAD lacks. So the mirror takes `cp port/build/I13/port/src/sim/gfx_terrain.cpp`
   and is the tree I13 gated plus S4. Lead told 2026-10-28 (send_to_session). S4's paths and shared edits do not touch gfx_terrain.cpp or gl_raster.
4. `~/bin/py -3.12 log/roadmap/S4/2b_apply_shared.py port/build/S4r2` (resub2/apply_mirror.log; ends with "UNCERTAINTIES rows 71
   (U-1418 .. U-1545), status set for 13 + 3 wave-1 rows, U-1421 extended") and `~/bin/py -3.12 log/roadmap/S4/resub2/edit_equiv.py
   S4r2` (resub2/edit_equiv.log): SAME on all 8 files.
5. Builds (FreeType on, as I13): from port/build/S4r2, `SG_FREETYPE_ROOT='C:\Users\maria\vcpkg\installed' ~/bin/py -3.12
   port/test/build.py --arch x64` and `--arch Win32` (resub2/build_x64.log, build_w32.log): portsim_c.dll x64 + Win32,
   portsim_runner32.exe, simgolf_port.exe. re/tools/glexact/glexact.exe and glreplay.exe copied from port/build/I13 (untracked
   binaries test_gl_raster needs, as in the 78ffb06d proof).

## 2. Gates (all on port/build/S4r2; logs and TSVs in log/roadmap/S4/resub2/)
Runner: `sh log/roadmap/S4/resub2/gates.sh <step>...` (I13's steps + resub's s4_* steps + buyland_w32; each non-pytest runner under
test-slot; waits for 16 GB free commit; `[N/M] step rc=..` lines in g_gates.log). Chunks: gates_c1.log (segments / boot),
gates_c2.log (oracle a + b), queue_c3.sh (oracle c and the rest of G1), queue_c4.sh (baseline gates, the suites, s4_pytest_w32). All
steps rc=0. Free commit stayed 32-41 GB, no step waited.

### S4's own gate (G1), against S4's 78ffb06d proof (log/roadmap/S4/resub/; `s4_totals.py` -> s4_totals.log)
- sound_replay oracle (`oracle --update` into the mirror's s4r/ scratch dirs): 1,087 keys / 16,859 vectors, 1,087 GREEN,
  0 GREEN_PENDING, 0 fallbacks (S4.2 68 / 1,219, S4.3 57 / 628, S4.4 57 / 2,001, S4.5 113 / 1,840, S4.6 168 / 3,344 with 17
  quirk-forgiven, S4.7 117 / 2,146, S4.8 ds 70 / 1,215, S4.8 dev 104 / 1,316, S4.9 247 / 2,404 with 1 quirk-forgiven, S4.9b 25 / 74,
  S4.11 61 / 672). Per-key rows (verdict, vectors, match, fallbacks, x87diff, tier, pending): 0 differ from resub.
- audio_exe oracle: 182 keys / 1,861 vectors, 178 GREEN + 4 GREEN_PENDING (179 / 1,858 + the 3 arrayStaticInit_448160 keys), 0
  fallbacks; 0 rows differ from resub.
- Goldens (`2b_golden_compare.py --new s4r/goldens` + `golden_vs_tree.py`): all twelve EQUAL key for key to 2b_accepted and to the
  shipped copies (core 68, chan 168, ds 70, dev 104, mixer 57, codec 57, seq 113, sound 117, wave 247, soundfile 25, crt 61, exe 183).
- Replay: 0 failures in every step, x64 and Win32 (S4.2 68, S4.3 57, S4.4 57, S4.5 113, S4.6 168, S4.7 117, S4.9 247, S4.9b 25,
  S4.11 61, S4.8 ds 70, dev 104) and S4.4 Win32 soft 57.
- Controls (sound_replay negctl): 669 functions over 36 runs, 667 RED, 2 BLIND = Sound5b6a8_ctor mode 2 and DsBuffer::setVolume
  mode 3, as accepted; every row equal to resub's (mode-1 totals S4.2 61, S4.3 42, S4.4 38, S4.5 80, S4.6 101, S4.7 66, S4.8 ds 54 +
  dev 62 = 116, S4.9 100, S4.9b 10 modes, S4.11 28). S4.10's 114 controls are test_audio_exe.py's (302 passed, below).
- Sink logs: S4.8 ds 5 / 5, dev 31 / 31, S4.9 83 / 83, S4.9b 25 / 25 EQUAL; controls closeMmioHandle:2 DIFF 3, WaveInDevice_reset:2
  DIFF 6.
- AudioSink streams (`audio_stream stream`): 205 / 205 PASS, 4,964 requests, 0 other commands, x64 and Win32; the per-context
  counter equals the segment's requests in 205 / 205 rows on both; 0 rows differ from resub; x64 stream_sha = Win32's in every row
  (streams_compare.log).
- Device-present run (`audio_stream device --update`): 181 requests, 3,005 dispatched, 0 fallbacks, 219 heap allocations, 13,020
  lines, sha256 4b6ed61a9306793c298044d20fb3c91e32c2fa9a4e75b11220220c43b8f0eddb; s4_device.txt byte-equal to resub's.
- test_buyland alone on Win32 (`-m ""`): 13 passed / 3 skipped in 6 of 6 runs (buyland_w32_<n>.log).
- The 13 audio test files under SG_BACKEND=win32 (suite_win32_audio/) and the s4_pytest_w32 selections (s4_pytest_*_w32.log, setvolume
  grid): section 3.

### HEAD's baseline (re/roadmap/BASELINE.md at 01d84188), against I13 (log/roadmap/merges/I13/) unless named
- repo static (`pytest -m static`): 630 passed / 2 skipped / 46 deselected (HEAD 614 / 2; +16 are S4's: test_fixvm and the static
  audio tests; 630 / 2 was also the 78ffb06d proof), 0 failed.
- segments (`integrator segments`): x64 205 / 205, Win32 205 / 205, freetype x64 205 / 205; 0 rows differ from I13, from I12 and
  from resub; x64 = Win32 = freetype (segs_compare.log).
- boot (`boot --all`): x64, Win32, freetype x64 each 70 rows 66 PASS / 2 KNOWN / 2 NO_FRAMES; 0 rows differ from I13 (x64, Win32),
  I12 (all three) and resub (boot_compare_*.log). No boot row failed, so no U-1545 re-run was needed.
- chains (`integrator chain --all`): 342 / 342 PASS; defects 15 switches; ledger 30 lines, 1 without a cause
  (showRoutingMapScreen.exitRemapReadsFrame). I13 did not run these (BASELINE row 22), so against I12 (MERGE_w5b at d66aa755): 0 rows
  differ in all three. Against resub: chain 0, defects and ledger differ only by flic.treeAliasNames (S3.4b's switch, in HEAD since
  01d84188) (ledger_compare.log).
- gfx.check: x64 and Win32 0 duplicates, 131 bodies, 10 not-drawing, as I13.
- frame census (`framegate --all`): 698 rows, PASS 100 / FAIL 537 / UNSUPPORTED 51 / NOT_RUN 10, as I13; PASS lost 0, gained 0, no
  status transition, total_px equal in all 688 judged rows (census_compare.log).
- S3 terrain (`2_census.py terrain`): 527 PASS / 75 NOT_MEASURED, 0 verdicts differ (terrain_compare.log).
- S7 live equivalence (`live equiv --all`): x64 and Win32 68 PASS + 2 NO_FRAMES; app replay (`app_replay gate`): hole 340 / report
  205 / saveload 266 PASS on x64 and Win32; 0 rows differ from I13 (s7_compare.log).
- port suite x64, one file per process (`-m ""`, suite/, suite_compare_x64.log): 95 files = HEAD's 82 + S4's 13, 94 PASS. The one
  failure is test_fttext with 1 failed test, test_font_metrics_equal_on_win32, one of the two U-1393 host GDI metric tests (I13 had
  both failing). All six test_app_* files PASS (app at Normal priority, G10). Per file against I13 (suite + suite_app): 79 SAME,
  13 NEW (the audio files, each equal to resub's counts), 3 CHANGED: test_fttext (U-1393), test_vectors 310 passed / 2,236 skipped
  (I13 298 / 538), test_win32 185 / 1,116 (I13 179 / 267). test_win32 = I13 + S4's delta at 78ffb06d (+6 passed, +849 skipped).
  test_vectors: every skip is a registered name with no vector file, and with -rs the names are exactly I13's 538 plus S4's 1,698
  (vectors_rs_S4r2.txt, vectors_rs_I13.txt; 0 names outside that union, 0 missing). The +12 passed are S4's exe vector stems, as
  at 78ffb06d.
- port suite Win32 on I13's 66-file list (log/roadmap/merges/I13/win32_files.txt; suite_win32/, win32_compare.log): 66 files judged,
  0 missing; failed ids 20, every one in log/roadmap/merges/I10/win32_failed_ids.txt, 0 NEW. The other 2 known ids
  (test_fttext::test_font_metrics_equal_gdi_and_the_originals and ::test_font_metrics_equal_on_win32, U-1393) passed in this run;
  U-1393 records that those two pass or fail with the host's GDI state (on x64 the same night one of them failed).
- U-1398: suite_by_file.sh restores log/roadmap/S7/8_app_ingame_facts.json, 8_app_live_facts.json and 8_present_facts.json (file by
  file `git checkout --`) after every test_app_* file and gates.sh after each app_replay step; the rewritten copies are kept in
  resub2/facts_saved/. `git status log/roadmap/S7/` was clean after the x64 suite. S3.12 runs app tests in its own mirror through the
  same log junction at the same time, so a rewrite in the shared tree can also come from S3.12.

## 3. Win32 audio files and the s4_pytest_w32 selections
- The 13 audio files under SG_BACKEND=win32 (`-m ""`, suite_win32_audio/, g_suite_win32_audio.log): all PASS, every count equal to
  the 78ffb06d proof's (chan 288 / 269 skipped, codec 111 / 99, core 146 / 128, crt 96 / 61, dev 170 / 104, ds 131 / 71, exe 302 / 5,
  mixer 109 / 57, names 11, seq 200 / 193, sound 190 / 183, soundfile 41 / 25, wave 353 / 347).
- s4_pytest_w32 (`--backend win32 -k 'win32 or x87_ or replay_key or spill'` per audio module, plus the setVolume grid with
  SG_EMU_ORACLE=1): every line equal to resub's (core 1, mixer 61, chan 7, ds 1, dev 1, codec 7, seq 3, sound 3, wave 1, soundfile 1,
  crt 1 passed; grid 1 passed), 0 failed.
- test_audio_exe's 114 negctl parametrizations (test_negctl_red + test_device_negctl_red, `--collect-only`: 114) all ran and passed on
  x64 and Win32 (its 5 skips are the opt-in emulator oracle), so S4.10's 114 controls read RED.

## 3b. HEAD moved during the run (01d84188 -> e179bd24 -> 6e6df6e7)
- e179bd24 (2026-10-29): gfx_terrain.cpp = the file I13 tested. HEAD's gfx_terrain.cpp now equals port/build/S4r2's (CR stripped),
  so U-1546 is resolved and S4r2 needs no substitution on the new HEAD.
- 6e6df6e7: scripts/lan_release/* (new) and CLAUDE.md +10 lines; no S4 path, no port/ file, no other shared file. No test reads
  scripts/lan_release (the repo tests that touch scripts/ import re_classify, scoreboard and ghidra_pool by name).
- `2b_apply_shared.py --prove` re-run on 6e6df6e7: SAME on all 8 files (resub2/prove_6e6df6e7.log).
- So port/build/S4r2 equals 6e6df6e7 + S4 in every file the gates read; the two differences are CLAUDE.md's 10 documentation lines
  and the LAN release scripts. No gate was re-run for the move.

## 4. Files
log/roadmap/S4/12_files.txt. Tools in log/roadmap/S4/resub2/: mirror.py, edit_equiv.py, golden_vs_tree.py (resub's, pointed at
S4r2), env.sh, gates.sh, suite_by_file.sh, compare.py (against I13 / I12 / resub), win32_compare.py (I13's), s4_totals.py,
waitfor.py, queue_c3.sh, queue_c4.sh, win32_files.txt (= I13's). log/roadmap/S4/resub/ is untouched.
Removed after use: resub's identity_check.py copy (I8's post-merge check of the shared tree against the mirror; nothing is applied to the
shared tree before the merge, so edit_equiv.py is the pre-merge check) and gfx_check.py (gates.sh uses log/roadmap/S3/13_gfx_check.py
as I13).

## 5. Open
- U-1546 resolved by e179bd24 (12_uncertain.tsv). U-1547..U-1549 unused.
- U-1393 stays as recorded: test_fttext's two host GDI metric tests failed 1 of 2 on x64 and 0 of 2 on Win32 in this run.
- Mirror port/build/S4r2 kept (about the size of port/build/S4r) for the integrator; delete it after the merge.

## 6. Reproduce (from the shared root)
```
~/bin/py -3.12 log/roadmap/S4/2b_apply_shared.py --prove
~/bin/py -3.12 log/roadmap/S4/resub2/mirror.py --dst S4r2
~/bin/py -3.12 log/roadmap/S4/2b_apply_shared.py port/build/S4r2
~/bin/py -3.12 log/roadmap/S4/resub2/edit_equiv.py S4r2
(cd port/build/S4r2; SG_FREETYPE_ROOT='C:\Users\maria\vcpkg\installed' ~/bin/py -3.12 port/test/build.py --arch x64; ... --arch Win32)
sh log/roadmap/S4/resub2/gates.sh segs_x64 segs_win32 boot_x64 boot_win32 segs_ft_x64 boot_ft_x64
LT=_a S4STEPS="S4.2 S4.3 S4.4" S48="" sh log/roadmap/S4/resub2/gates.sh s4_oracle      # _b: S4.5 S4.6 S4.7; _c: S4.9 S4.9b S4.11 with S48 unset
sh log/roadmap/S4/resub2/gates.sh s4_exe s4_streams s4_device s4_golden s4_replay s4_negctl s4_sinklog buyland_w32
sh log/roadmap/S4/resub2/gates.sh static chain defects gfx_check census terrain live_equiv_x64 live_equiv_win32 app_replay_x64 app_replay_win32
OUT=log/roadmap/S4/resub2/suite sh log/roadmap/S4/resub2/suite_by_file.sh
SG_BACKEND=win32 OUT=log/roadmap/S4/resub2/suite_win32 FILES="$(cat log/roadmap/S4/resub2/win32_files.txt)" sh log/roadmap/S4/resub2/suite_by_file.sh
SG_BACKEND=win32 OUT=log/roadmap/S4/resub2/suite_win32_audio FILES="<port/test/test_audio_*.py>" sh log/roadmap/S4/resub2/suite_by_file.sh
sh log/roadmap/S4/resub2/gates.sh s4_pytest_w32
~/bin/py -3.12 log/roadmap/S4/resub2/compare.py segs | boot x64|win32|ft_x64 | ledger | census | s7 | terrain | streams | suite x64
~/bin/py -3.12 log/roadmap/S4/resub2/s4_totals.py; ~/bin/py -3.12 log/roadmap/S4/resub2/win32_compare.py
```

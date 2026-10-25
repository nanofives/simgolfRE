# S8.3 notes: the port's video module, the documented skip path for both movies (2026-10-24, worker on instance s8_3)

Write-up: re/analysis/video/skip_path.md (the skip path, the answers to the step's three questions, the host contract, the evidence).
Merge text: log/roadmap/S8/3_merge.md. U-rows: 3_uncertain.tsv (U-1720..U-1723). Files: 3_files.txt.

## What was done
- Module `video` (port/src/sim/video.cpp, port/include/portsim/sim/video.h): playAudioFile, BinkPlayer::play / setTarget / close behind a
  `MovieHost` (window, open, open_buffer, show_cursor, wait, frame, close_movie, close_buffer). Default host `SkipMovieHost` = every movie
  unavailable = the original's BinkOpen-NULL path. Test-only entries: video.attachRecorder / attachScripted / detach / setInput / clearInput /
  log / scriptedState. BinkPlayer::ctor not ported (vtable word + zeroes; g_bink is pointer class and the exe image already holds zeros).
- Intro call: tick_boot.inc before showTitleScreen (merge item 3); closing call: tick_menu.inc's existing call reaches the module once
  playAudioFile leaves kDrops (merge item 2).
- New intro-ON boot scenario cov_r_s8_play (re/tools/s8_boot_capture.py): S8.2's start (intro ON, checkpoint 2 s in), cov_r_quit's first session
  (Sandbox, a hole, Exit the Game -> quit save -> title), the exit icon (closing movie, credits), Esc, exit. Recorded on instances\s8_3 (winmm.dll
  381ed2b2, the pinned references' build; C3FT_AUDIO=none): schedule `rec`, captures `c1` `c2` with C3FT_FRAMEHASH=sim (identical: 216 frame
  records), trace `t1` (537,416 B, validate OK, 216 FRAME records with (index, T) equal to the capture's; binkw32 QPC 94,157 = intro 54,945 +
  closing 39,212). Installed: log/p2/p24/cov_r_s8_play/c_a/, port/corpus/traces/cov_r_s8_play.trace, port/corpus/saves/cov_r_s8_play/ (sve_refs
  collect: 4 runs agree).

## Answers (details and addresses in skip_path.md section 4)
1. binkw32's QPC records: parse_trace keeps only kind-1 (timeGetTime) values in a frame's clock list; QPC runs only move the ordinal and
   ctx_ra; all port clock reads are site-keyed. So they are skipped, never mismatched. No HANDLER / CURSOR / MSG record lies inside the movie.
   The HEAD build (movie dropped) already passes cov_r_s8_play 216 / 216; the merged build too, and its to-the-end run serves 929 clock
   queries with 0 missing (the intro's deadline read, site 0x2d0d1 = ra 0x0043cd00, value 1048577, is consumed by the module).
2. g_ui83ab9c: one reader (BinkPlayer::play 0x0048713d), setters 0x00487124 (1) and the three root handlers (0); engine class. No sim read.
   The skip path must not set it (the original's NULL path does not); vs an intro-ON recording the port holds 0 where the original holds 1 until
   the first click / key: documented, not compared. The deadline g_audio5aa774: written only by playAudioFile, read only by the handlers'
   tails (`jbe`: equal clock does not end the movie).
3. Skip path = BinkOpen NULL: written down in skip_path.md section 3 with the per-object effect table.

## Gate numbers (all in port/build/S8_3_root, variant merged, HEAD 4bfefc2a + this step's files + merge items 1-3)
- test_video.py x64: 32 passed, 24 skipped (oracle); SG_EMU_ORACLE=1: 24 passed (23 cases + coverage 12 / 12 jcc both sides); Win32 backend
  (SG_BACKEND=win32): 32 passed, oracle 24 passed (pinned digests equal on both backends). Logs 3_test_video_{x64,w32}.log, 3_oracle_*.log.
- Boot gate x64 `--all`: 67 rows, the 61 rows of log/p5/boot_gate_loc.tsv all same status / records / expected; new: cov_r_s8_play PASS 216 / 216.
  Also present in the shared corpus (siblings', not this step): cov_r_bio FAIL 579 / 687 (BASELINE's S1 row), cov_r_flic_input FAIL at record 1
  (g_cachedCameraTileX; identical FAIL with the HEAD DLL port/build/S0_head), cov_r_palette_hot / theme_cancel / tutorial PASS.
  3_boot_gate_x64.tsv, 3_boot_compare.log.
- Segment gate: x64 205 / 205, 0 digests differ vs log/p5/segs_x64_loc.tsv; Win32 205 / 205, 0 differ vs segs_win32_loc.tsv and vs x64.
- test_static (port): 6 passed. test_tick.py 250 passed (BASELINE 250). test_gfx + test_sve_roundtrip 32 passed (17 + 15, BASELINE).
- test_boot.py: 89 passed, 2 failed: test_boot_gate[cov_r_bio] and [cov_r_flic_input], the two sibling recordings above, which fail the same way
  with the HEAD DLL (checked with `port.test.boot` on port/build/S0_head's DLL). BASELINE's 88 predates those traces in the shared corpus.
  3_test_boot_x64.log, 3_test_tick.log, 3_test_gfx_sve.log.

## Controls (playAudioFile negctl modes; test_negctl_reads_red / test_boot_closing_dropped_reads_red)
1 deadline not cleared (RED on intro_skip, intro_play3), 2 skip path sets g_ui83ab9c (RED intro_skip, closing_skip), 3 skip path not taken (RED
intro_skip, closing_skip), 4 closing movie dropped (RED closing_skip, closing_play4, and the boot run: host log has only the intro), 5 deadline
one ms early (RED intro_key_early only). 5 / 5 RED. Blind: the boot gate itself to all five (no sim object), by construction, recorded.

## Reproduce
```
export SIMGOLF_ALLOW_MULTI=1 SIMGOLF_GAME_DIR=instances/s8_3          # instance made by re/tools/instances.py create s8_3, winmm.dll from instances/p24
~/bin/py -3.12 re/tools/s8_boot_capture.py run --label rec --record
~/bin/py -3.12 re/tools/s8_boot_capture.py run --label c1 --framehash ; ... --label c2 --framehash
~/bin/py -3.12 re/tools/s8_boot_capture.py trace --label t1
~/bin/py -3.12 re/tools/s8_boot_capture.py check c1 c2 --trace t1 ; ... install c1 --trace t1
~/bin/py -3.12 -m port.test.sve_refs collect cov_r_s8_play
~/bin/py -3.12 log/roadmap/S8/3_mirror.py --variant merged             # HEAD export + owned files + merge lines
cmake -S port/build/S8_3_root/port -B port/build/S8_3_root/port/build/S8_3 -A x64 ; cmake --build ... --config Release --target portsim_c
cd port/build/S8_3_root ; export SG_PORTSIM_DLL=$PWD/port/build/S8_3/Release/portsim_c.dll
SG_EMU_ORACLE=1 ~/bin/py -3.12 -m pytest port/test/test_video.py -q -p no:cacheprovider
~/bin/py -3.12 -m port.test.boot --all --jobs 3 --out log/roadmap/S8/3_boot_gate_x64.tsv
~/bin/py -3.12 -m port.test.integrator segments --backend x64 --jobs 8 --out log/roadmap/S8/3_segs_x64.tsv
```
(Win32: `~/bin/py -3.12 port/test/build.py --arch Win32` inside the mirror, then SG_BACKEND=win32.)

## Run hygiene
Boots only on instances\s8_3, one at a time (rec, c1, c2, t1; all exit code 0); original\ untouched; purge_frida_temp run (0 left); no game process left.
Note: the mirror's build of the Win32 runner lives in port/build/S8_3_root/port/build/Win32 (inside the mirror, not the shared port/build/Win32).

## Open
U-1720 sound argument (resourceQuery) passed as 0; U-1721 frameless boot branch does not report the quit (merge item 4); U-1722 input during a playing
movie modelled, not measured; U-1723 drawing around a movie not ported (phase 5).

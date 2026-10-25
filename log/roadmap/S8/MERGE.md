# S8 MERGE (video, skip path; lead 2026-10-24; D2: no decoder this round)

Steps S8.1, S8.2, S8.3 accepted. Files: log/roadmap/S8/files.txt (new files only: no tracked file was edited by a worker). Workers' own merge texts:
log/roadmap/S8/2_merge.md and 3_merge.md (exact text for items 1-6 below is in them). The gates ran in a private HEAD mirror (port/build/S8_3_root,
log/roadmap/S8/3_mirror.py --variant merged), not on the shared tree.

## Shared-file edits, in apply order

1. port/src/sim/modules.inc: append `SG_MODULE(video)`; then touch port/src/registry.cpp (MSBuild does not rebuild it when only modules.inc changes).
2. port/src/sim/tick.cpp, kDrops: delete the line `{"playAudioFile", "the closing Bink video (binkw32); its pumps dispatch no recorded input in the corpus"},`
   (a drop wins over the registry, so the module is unreachable while it stays). showTitleScreen stays a drop.
3. port/src/sim/tick_boot.inc, BootGameMain: insert directly before `    cc("showTitleScreen");` the two comment lines and
   `    cc("playAudioFile", (GS(f_strpool_4c89ac) + 96), 0);           // ./flics/SMSG_introfinal.bik` (text in 3_merge.md item 3).
4. OPTIONAL port/src/sim/tick.cpp, the frameless boot branch of tick_run_frames: keep the boot step's FrameExit and report "mainLoop returned" for a
   title-menu-only trace (U-1721; replacement text in 3_merge.md item 4; test_video.py accepts both texts).
5. re/tools/scenario.py, end-of-file hook: widen the glob to `scenario_s[18]_*.py` so scenario_s8_2.py registers (2_merge.md item 1; the S8 drivers
   import it themselves, so nothing breaks without the edit). If another section also widens this glob, merge the character class.
6. OPTIONAL re/tools/trace_tool.py, `install`: replay of a scenario whose lockstep function has `expect_exit` sets `c.run.exit_expected = True` first and
   keeps answering polls after the recorded ones (U-1714, 2_merge.md item 2; s8_movie_runs.py does this from outside under S8_REPLAY_EXIT=1).
7. UNCERTAINTIES.md: append log/roadmap/S8/1_uncertain.tsv, 2_uncertain.tsv, 3_uncertain.tsv (U-1700..U-1723).
8. re/roadmap/sections.tsv: S8 status `done`; re/PORT_PLAN.md: video row = skip path done (D2), decoder host open.
9. Local data (gitignored, already in place in the shared tree; a clean checkout needs `~/bin/py -3.12 re/tools/s8_boot_capture.py install` and a capture
   run on its own instance): port/corpus/traces/cov_r_s8_play.trace (+ .json), log/p2/p24/cov_r_s8_play/c_a/, port/corpus/saves/cov_r_s8_play/,
   port/corpus/traces/s8_2/ (S8.2's cov_r_s8_full traces), log/c3/alt/refs/cov_r_s8_*/.

## Cross-section notes
- For S3 (sprite animations): U-1701, six loadFlic sites in loadInterfaceArt (0x00441e13, 0x00441e28, 0x00441e3d, 0x00441e52, 0x00441e6a, 0x00441e7f) name
  `Tree Cerc` / `Tree Drac` with a space, the install has Tree_Cerc / Tree_Drac; the original's behaviour on the failed open is not traced.
- No FLIC is a cutscene: all 1,893 files are loadFlic sprite animations (loadWorldAssets / loadInterfaceArt), S3's. The serviceMovie family
  (0x0049c8e0, table 0x00840698, count 0x008406e4) is an unused RIFF/mmio player: nothing writes the table and winmm has no mmioOpen import.
- Decoder (later decision): implement MovieHost (port/include/portsim/sim/video.h); skip_path.md lists what a decoder host must reproduce.

## CLAUDE.md paragraph (port section, after the last phase paragraph)

```
S8 video, skip path (2026-10-24, D2: no decoder this round; re/analysis/video/MOVIES.md, movie_runs.md, skip_path.md, re/port/movies.tsv,
flics_census.tsv): the game plays exactly two Bink movies, both through playAudioFile 0x0043cce0 -> BinkPlayer::play 0x00487090 (its only caller): the
intro SMSG_introfinal.bik from showTitleScreen 0x0043cd97 (path 0x004c8a0c) and the closing SMSG_closingfinal.bik from mainLoop 0x0040faa4 (path
0x004c6bb4); jgld.dll, Terrain.dll and sound.dll import nothing from binkw32. No FLIC is a cutscene: all 1,893 .flc files are loadFlic sprite animations
(S3); the serviceMovie family (0x0049c8e0) is an unused RIFF/mmio player (table 0x00840698 never written). Measured on the original with the intro ON
(instance s8_2, three runs per condition identical): both play to the clip end (825 and 589 frames), binkw32 paces with QueryPerformanceCounter (54,945
main-thread calls for the intro) and never reads timeGetTime, so the movies add no virtual T and no scenario poll runs inside them; against the skipped
intro the only run-stable GameState word that differs is g_ui83ab9c 0x0083ab9c (0 -> 1, written by BinkPlayer::play, cleared by the root-window input
handlers); g_seedTime, g_rng and every sim object are equal. Port: module `video` (port/src/sim/video.cpp, portsim/sim/video.h) ports playAudioFile and
BinkPlayer::play / setTarget / close behind a MovieHost; the default host answers "unavailable", i.e. the original's own BinkOpen-NULL path (deadline word
set and cleared, g_bink target set, one pumpInput483bd0, g_ui83ab9c untouched), which is what the pinned corpus recorded; the intro call is made from the
boot (tick_boot.inc), the closing call from the QUIT block (playAudioFile is no longer a drop). binkw32's QPC records never enter a clock list
(parse_trace keeps timeGetTime values and reads are site-keyed), so an intro-ON trace needs nothing else. Evidence: emulator oracle 23 / 23 cases, 12 / 12
branches both ways; 5 negative controls RED; new intro-ON boot scenario cov_r_s8_play (re/tools/s8_boot_capture.py) PASS 216 / 216 and to the exit with the
quit files equal; boot rows 61 / 61 unchanged, segments 205 / 205 x64 = Win32 with 0 digests changed. U-1700..U-1723 (input during a playing movie is
modelled not measured; the drawing around a movie and the sound argument are not ported).
```

## Section gate: commands and expected numbers (in a mirror of HEAD plus the files of files.txt and items 1-5)

```
~/bin/py -3.12 -m pytest tests/test_movie_inventory.py -q                 # 7 passed (static, no game)
~/bin/py -3.12 -m pytest port/test/test_s8_2_scenarios.py -q              # 14 passed (static)
SG_EMU_ORACLE=1 ~/bin/py -3.12 -m pytest port/test/test_video.py -q       # 32 module tests + 24 oracle tests, x64 and SG_BACKEND=win32 equal digests
~/bin/py -3.12 -m port.test.boot --all --jobs 3 --out log/roadmap/S8/boot_gate.tsv   # 61 baseline rows unchanged + cov_r_s8_play PASS 216/216
log/p38/suite_by_file.sh (test_tick 250, test_gfx + test_sve_roundtrip 32)             # as re/roadmap/BASELINE.md
```
Known failures in the mirror that are not S8's (same with the HEAD build): test_boot cov_r_bio (S1's, BASELINE) and cov_r_flic_input (a sibling's new recording).

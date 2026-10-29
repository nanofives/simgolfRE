# Draft: replacement of MERGE.md's "Resubmission proof" text for HEAD 01d84188 (S4.12, for the lead to edit into MERGE.md)

Replace the paragraph that starts `Resubmission proof (2026-10-26/27, mirror port/build/S4r = HEAD 78ffb06d ...` down to the line before
`Command lines (2b_notes.md "Reproduce"):` with the text below. In the command block, replace the three `resub/` lines with the
`resub2/` lines at the end of this file. Also in MERGE.md's "Shared-file edits": the --prove re-run is on 01d84188 and on 6e6df6e7 (SAME
on all 8 files, log/roadmap/S4/resub2/prove.log, prove_6e6df6e7.log); section 3 of I3's script now anchors on sg_build_info's tail (the
lead's fix after I11). Add U-1546 (log/roadmap/S4/12_uncertain.tsv, status resolved) to the UNCERTAINTIES rows the merge appends, or
leave it in the notes: the lead's call.

---

Resubmission proof on HEAD e179bd24 (2026-10-28/29, step S4.12, notes log/roadmap/S4/12_notes.md; I11 refused the apply on 01d84188,
log/roadmap/merges/I11.md). Mirror port/build/S4r2 = the e179bd24 tree + the 86 paths + `2b_apply_shared.py`: built as git archive of
01d84188 (log/roadmap/S4/resub2/mirror.py --dst S4r2) with port/build/I13's gfx_terrain.cpp, because 01d84188's copy did not compile
against its gl_raster.h (U-1546). e179bd24 commits that same file (md5 with CR stripped e88dfcab... in e179bd24, port/build/I13 and the
mirror) and changes nothing else under port, shim, scripts, tests, re or tools, so the mirror equals e179bd24 + S4
(port/build/S4r2/HEAD_COMMIT.note). Gates: log/roadmap/S4/resub2/gates.sh (I13's steps
plus resub's s4_* steps), the suites through resub2/suite_by_file.sh. Baselines: I13 (log/roadmap/merges/I13/), and I12 for the gates
I13 did not run (chains, defects, ledger, freetype).
- --prove: SAME on all 8 files on 01d84188 (the nine shared files are equal in e179bd24) and on 6e6df6e7; edit_equiv.py on S4r2: SAME
  on all 8.
- repo static 630 / 2 (HEAD 614 / 2 + S4's 16), 0 failed.
- segments 205 / 205 on x64, Win32 and freetype x64; 0 rows differ from I13 / I12; x64 = Win32 = freetype digests.
- boot x64 / Win32 / freetype x64: 70 rows 66 / 2 KNOWN / 2 NO_FRAMES each, 0 rows differ from I13 / I12 (no low-memory re-run needed).
- chains 342 / 342, defects 15 switches, ledger 30 lines with 1 row without a cause (showRoutingMapScreen.exitRemapReadsFrame): 0 rows
  differ from I12. gfx.check 0 duplicates / 131 bodies on both backends.
- frame census 698 rows PASS 100 (FAIL 537, UNSUPPORTED 51, NOT_RUN 10): none lost, none gained, total_px equal in all 688 judged rows.
  S3 terrain 527 PASS / 75 NOT_MEASURED, 0 verdicts differ.
- S7: live equivalence 68 PASS + 2 NO_FRAMES on x64 and Win32; app replay hole 340 / report 205 / saveload 266 on both; 0 rows differ
  from I13.
- port suite x64, one file per process with `-m ""`: 95 files (HEAD's 82 + 13 audio), 94 PASS; the failure is test_fttext with 1 of the
  2 U-1393 host GDI metric tests (I13: both). All six test_app_* files PASS (app at Normal priority). 79 files equal I13's counts; the
  13 audio files equal the 78ffb06d proof's; test_vectors (310 passed / 2,236 skipped) and test_win32 (185 / 1,116) are I13's plus S4's
  own additions (test_vectors' skips are exactly I13's 538 names plus S4's 1,698, each a registered name with no vector file).
- port suite Win32 on I13's 66-file list: 20 failed ids, every one in I10's 22 known ids, 0 new; the other 2 known ids (test_fttext's
  U-1393 pair) passed in this run. The 13 audio files under SG_BACKEND=win32: all PASS, counts equal to the 78ffb06d proof's.
  test_buyland alone on Win32: 13 passed / 3 skipped in 6 of 6 runs.
- G1: sound_replay oracle 1,087 keys / 16,859 vectors GREEN, 0 pending, 0 fallbacks (17 + 1 quirk-forgiven vectors); audio_exe 182 keys /
  1,861 vectors, 178 GREEN + 4 GREEN_PENDING; every per-key row equal to the 78ffb06d proof's. Twelve goldens EQUAL to 2b_accepted and
  to the shipped copies. Replay 0 failures in every step on x64 and Win32 (+ S4.4 Win32 soft). Controls: 669 functions, 667 RED, BLIND
  only Sound5b6a8_ctor m2 and DsBuffer::setVolume m3 (as accepted), every row equal to the 78ffb06d proof's; S4.10's 114 controls
  (test_audio_exe) all RED. Sink logs 5 + 31 + 83 + 25 EQUAL, the two sink controls DIFF 3 / 6. AudioSink streams 205 / 205, 4,964
  requests, the per-context counter = the segment's requests in 205 / 205 rows, x64 = Win32, 0 rows differ from the 78ffb06d proof.
  Device-present run 181 requests, 0 fallbacks, sha256 4b6ed61a... (= I8's and the 78ffb06d proof's). Win32 audio selections and the
  setVolume grid pass, every count equal to the 78ffb06d proof's.
- HEAD moved during the run: e179bd24 replaced gfx_terrain.cpp with I13's file (= the mirror's, so U-1546 is resolved), and 6e6df6e7
  added scripts/lan_release and 10 CLAUDE.md lines, which no gate reads. --prove on 6e6df6e7 is SAME on all 8 files, so the proof
  stands for 6e6df6e7 + S4.
- U-1398: the tracked log/roadmap/S7/8_*facts.json were restored after every app test run; `git status log/roadmap/S7/` clean at the end.

Command block lines (replace the three resub/ lines):
```
~/bin/py -3.12 log/roadmap/S4/resub2/mirror.py --dst S4r2          # mirror port/build/S4r2 (86 paths); on 01d84188 also cp port/build/I13/port/src/sim/gfx_terrain.cpp (U-1546; not needed from e179bd24 on)
~/bin/py -3.12 log/roadmap/S4/2b_apply_shared.py port/build/S4r2   # then build x64 + Win32 there (SG_FREETYPE_ROOT set, port/test/build.py)
~/bin/py -3.12 log/roadmap/S4/resub2/edit_equiv.py S4r2             # every edit = S4's on b3d61e41 (+ wave-2 paragraph, U-1545)
sh log/roadmap/S4/resub2/gates.sh <step>...                         # steps: see the file; suites: resub2/suite_by_file.sh
~/bin/py -3.12 log/roadmap/S4/resub2/compare.py segs|boot <be>|ledger|census|s7|terrain|streams|suite x64
~/bin/py -3.12 log/roadmap/S4/resub2/s4_totals.py; ~/bin/py -3.12 log/roadmap/S4/resub2/win32_compare.py
```

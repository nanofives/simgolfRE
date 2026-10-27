# S7.7 notes: S7 rebased onto HEAD 2c05f02a, U-1604 closed (worker S7.7, 2026-10-24/25)

Row S7.7 of re/roadmap/S7_steps.tsv, run on claude3. Inputs were S7.1 (1_notes / 1_merge / 1_tick.patch / 1_capi.patch), S7.6
(6_notes / 6_merge / 6_capi.patch) and the live files.

Mirror port/build/S7_7_head: `~/bin/py -3.12 log/roadmap/S7/7_mirror.py [--build]`. It holds git archive 2c05f02a, the S0 junctions,
shim/build and re/replay. On creation it applies 7_tick.patch / 7_capi.patch, or S7.1's and S7.6's patches if those are missing. It
takes the live files from log/roadmap/S7/7_live/ if that exists, else from port/build/S7_6_h2. It adds S7.1's modules.inc line and
S7.2's CMake line, and copies the shared tree's port/app and app tests in (an existing mirror file is never overwritten; a
difference is printed).

Merge: 7_merge.md (replaces 1_merge.md and 6_merge.md). Files: 7_files.txt. U-rows: 7_uncertain.tsv (U-1660..U-1664).

## 1. Result (paragraph for CLAUDE.md)

Roadmap S7.7, S7 on HEAD 2c05f02a (2026-10-25, notes log/roadmap/S7/7_notes.md, merge 7_merge.md, U-1660..U-1664; U-1604 resolved):
- The live core (S7.1 / S7.6) runs over S1's two input paths.
  - A dispatch given at loadFlic's pump (PumpKind::Load) is recorded with jgld's Palette::update QPC record right after its
    HANDLER records. That record is parse_trace's `load` marker, so TraceInput hands the batch to the same Load call in the
    replay.
  - A key the window tree's focus child took (RootHandler 12 / 13) is recorded as its MSG record with no HANDLER after it. A
    following root handler first gets its own MSG record, so parse_trace resolves the pending key as the focus child's.
- 7_tick.patch is S7.1's tick.cpp / tick_hook.h / tick.h change regenerated against 2c05f02a: the changed lines are the same,
  only the offsets differ.
- 7_capi.patch merges S7.1's and S7.6's C ABI patches with two changes:
  - The live run's Win32 runner op is 40, because S3.2's uncommitted runner32.cpp uses 28..32 (U-1661).
  - sg_live_run_builtin kind 3 is a PREFIX host (live.cpp PrefixHost): a corpus trace answers until its first dispatch that
    holds a focus key, then a script answers from that pump call on.
- The script host has a `wait_load` step: the next input is given at the first Load call the core lets deliver.
- New scripts:
  - `bio_type`: prefix cov_r_bio, then 11 focus keys plus Back / Enter through the root's onKey in four batches, one of them
    ending on a focus key, then the editor's check mark.
  - `load_input`: Sandbox, then the Monterey click, 'z' and 'x' at two Load calls of the one world load.
- Both round-trip on x64 and Win32 (identical runs).
- Controls are RED: focus keys left out of the recording (bio_type, record 579, g_golferTypes); load marker left out
  (load_input, record 1, g_zoomScale / g_cachedCameraTileX).
- Gates in the mirror:
  - Equivalence: 70 rows, 68 PASS + 2 NO_FRAMES on both backends, 0 violations, 0 replay differences, x64 = Win32 in every row.
    cov_r_bio (687 records) and cov_r_flic_input (75), whose boot run failed at c6d7d78b, now PASS live, boot and replay.
  - Segments and boot gate: as section 2.
  - test_live 32 passed on x64 (`-m "not slow"`; one assertion was moved, section 3) and 32 on Win32.
  - test_app_live 13, test_app_shell 18, test_app_input 16, port test_static 6.

## 2. Gate, measured

| item | measured | where |
|---|---|---|
| patches apply | 7_tick.patch / 7_capi.patch: `git apply --check` OK on HEAD 2c05f02a's files and on the shared working tree's (2026-10-25, with S3.2's runner32.cpp and lib32.py edits in it) | 7_make_patches.log |
| equivalence x64 | 70 rows: 68 PASS, 2 NO_FRAMES (cov_a_quit_title, cov_b_location), violations 0, replay differences 0; HEAD's 61 + the six S1 / S8 rows + S7.6's three extra corpus rows. cov_l_point failed once (boot run code -11 SG_E_NO_RESOURCE while siblings loaded the PC) and PASSED alone | 7_equiv_x64.tsv, 7_equiv_x64_cov_l_point.tsv, 7_equiv_compare.log |
| equivalence Win32 | the same 70 rows, 0 rows differ from x64 (status, records, expected, boot status, both diffs, violations) | 7_equiv_w32.tsv, 7_equiv_compare.log |
| against S7.6 (c6d7d78b) | only cov_r_bio (records 579 -> 687, boot FAIL -> PASS) and cov_r_flic_input (boot FAIL -> PASS) differ: S1's merge made their boot runs pass, and the live core now gives what the boot run gives | 7_equiv_compare.log |
| segments | x64 205 / 205 PASS and Win32 205 / 205 PASS; 0 rows differ from the S1 + S8 merge record (log/roadmap/merges/S1S8-4/segs_x64.tsv / segs_win32.tsv), so 0 digests changed | 7_segs_x64.tsv, 7_segs_win32.tsv |
| boot gate x64 | 70 rows. HEAD's 61 at 58 PASS / 2 KNOWN / 1 NO_FRAMES with 0 rows differing from the merge record. The six S1 / S8 rows PASS with their measured records (cov_r_bio 687, cov_r_flic_input 75, cov_r_tutorial 1195, cov_r_theme_cancel 123, cov_r_palette_hot 565, cov_r_s8_play 216). Uncommitted corpus rows: cov_a_quit PASS, cov_a_quit_title NO_FRAMES, cov_r_s2_screens FAIL (an S2 recording, also FAIL at c6d7d78b: 6_notes.md) | 7_boot_x64.tsv, 7_boot_compare.log |
| U-1604 script sessions | bio_type PASS, 619 records, 61 dispatches, 0 violations, focus_msgs 11; load_input PASS, load_dispatches 2; both x64 = Win32 | 7_test_live_u1604.log, 7_scripts_probe.log |
| tests | test_live x64 31 passed + 1 failed, then the moved assertion re-run 1 passed (7_test_live_x64.log, _rerun.log); Win32 32 passed (7_test_live_w32.log); test_app_live 13 (7_test_app_live.log; facts copied to 7_app_live_facts.json: 356 periods 87..100 ms, median 93; 58 with bit 17, 125..136; idle CPU 2.64 %; 423 records replay; it rewrites 6_app_live_facts.json on every run), test_app_shell 18, test_app_input 16, port test_static 6 | 7_test_*.log |

## 3. What was done

### Checked: what S7.6 left in port/build/S7_6_h2
- HEAD_COMMIT is 2c05f02a.
- Its shared files equal HEAD + 1_tick + 1_capi + 6_capi byte for byte.
- live.h and live.py equal S7_6_head's. live.cpp and test_live.py carry S7.6's unfinished U-1604 edits: the Load class in the
  core's taker (`same_class(body, load)`), the palette-QPC separator after a load dispatch, the focus-key MSG records with
  `handler_msg`, the DELIVER post for first-generation key MSGs, controls 7 / 8, and two trace-host tests.
- 6_u1604_check.log's `replay 2` for cov_r_bio was not reproduced: on this mirror cov_r_bio's replay equals its live run.
- S7_6_h2 was used only as the source of those two files. Every gate was re-run here.

### Rebase
- 1_tick.patch applies to 2c05f02a with offsets, and S1's parse_trace / TraceInput need no core change beyond S7.6's edits.
- 1_capi.patch failed on the shared tree only, at runner32.cpp: S3.2's LiveTerrain ops 28..32 sit where S7.1's op 28 did, and
  both edit the `handle ops 17..27` comment.
  - Op 40 now (live.py OP_LIVE).
  - The block moved before `op == 20`.
  - The comment is untouched.

### U-1604 in the live core (S7.6's edits, kept and judged here)
- Load: a Load call is neither body nor modal-Blocking class. The taker counts earlier modal Blocking and Load calls of the
  burst for a load dispatch (TraceInput::batch: a modal Blocking pump takes any modal batch, a Load pump only a `load` one).
- After a load dispatch the recorder writes the palette-QPC clock record (separator(true)). A next modal dispatch gets a
  waitTicks-site record in front of it, so its class stays modal.
- Focus keys: a 12 / 13 command is written as DELIVER post (0x100 only) + MSG 0x100 / 0x102 with the key in wParam and no
  HANDLER. The next root-handler command first gets its own message's MSG record (handler_msg: 2 -> 0x201, 1/3/10 -> 0x202,
  5 -> 0x204, 4/6/11 -> 0x205, 7 -> 0x102 or 0x100 for a0 = 0, 8 -> 0x100, 9 -> 0x101).
  - So parse_trace resolves the pending key at that MSG record, and the HANDLER does not count as the key's own.
  - A dispatch of focus keys only is a violation: no HANDLER can open its batch (U-1660).
- Script host:
  - `wait_load`: the next step is given at the first Load call with may_deliver.
  - While a `wait_load` waits, every other pump call declines.
  - SG_LIVE_SCRIPT_LOG=<file> (development) logs every pump call the script answers.
- PrefixHost (new): it forwards to the trace host and remembers its last clock / frame-head value, cursor and async answers.
  At the first pump call where the trace host would deliver a batch holding 12 / 13 it drops that batch and switches. The
  script host's take_over sets now_ / last_head_ / cursor / keys and runs its first steps; boot passes the core's writes /
  marker pointers to both hosts.
- Why a prefix: no script reached the editor's bio box on its own.
  - The sandbox's people palette (272, 560) opens showHireEmployeeDialog (SG_TICK_TRACE).
  - The ui_tour trace host never calls showCharacterEditor in its first 1,250 records.
  - mood5.sve's palette gives clickEmployeeView.
  - cov_r_bio opens the editor with '?' on a golfer it clicked from the name label the record run found (U-1662).
- The bio keys follow cov_r_bio's records (7_bio_probe.py): printable characters as WM_CHAR alone are taken by the EditBox (13).
  An edit key's KEYDOWN is taken by it (12) and its KEYUP reaches the root's onChar0 (9). Back's WM_CHAR 8 and Enter's WM_CHAR 13
  reach the root's onKey (7, a1 = the last key-down's virtual key: H7(8, 8), H7(13, 35)).
- The check mark needs `cursor 753 213` + `idle 3` before the click, because the hit test reads the cursor before the pump. It
  needs `idle 1` after it, because the release wait's second pumpMessages is idle_ok and the script host stops the session
  there otherwise (U-1663).
  - Without `idle 1` the session stopped inside the editor at 579 records and control 8 read GREEN. That was an equivalent
    mutant: the edited bio is only hashed once the editor returns.
  - With it: 619 records, control 8 RED at record 579.

### A test the rebase moved
- test_callback_host_equals_script_host asserted `n >= 60` for `wait_frames 60` and now gets 59 records, on both hosts alike (the
  equality, which is what the test checks, holds). Since S1, the core asks the host about the world load's Load pumps. The script
  host's (and PyHost's) advance runs there, so the 60-frame wait starts during the Monterey load instead of at the next frame head.
- SG_LIVE_SCRIPT_LOG shows the step after `wait_frames` taken at a kind 3 call in frame 0, with 506 Load calls in the session.
- The threshold is now 59, with that reason in a comment.

### Tests added (port/test/test_live.py)
- test_roundtrip / test_roundtrip_backends_identical now run every SCRIPTS entry through script_data / SCRIPT_FILES.
  - bio_type: >= 600 records, >= 61 dispatches, focus_msgs == 11.
  - load_input: >= 80 records, load_dispatches == 2.
- `input_paths(trace)` in live.py counts both paths by parse_trace's rules.
- NEGCTL rows: `focus keys left out of the recording (bio_type)` mode 8, `the load dispatch's palette QPC left out (load_input)`
  mode 7.
- S7.6's trace-host tests (test_load_pump_and_focus_keys_recorded, test_u1604_controls_red) are kept.

## 4. Commands

```
~/bin/py -3.12 log/roadmap/S7/7_mirror.py --build                       # mirror + x64 build (port/build/S7_7 inside it)
cd port/build/S7_7_head; ~/bin/py -3.12 port/test/build.py --arch Win32  # Win32 DLL + runner (port/build/Win32 inside it)
export SG_PORTSIM_DLL=$PWD/port/build/S7_7/Release/portsim_c.dll
~/bin/py -3.12 -m port.test.live equiv --all --jobs 3 [--backend win32] --out ../../../log/roadmap/S7/7_equiv_<x64|w32>.tsv
~/bin/py -3.12 -m pytest port/test/test_live.py -p no:cacheprovider -m "not slow"     # SG_BACKEND=win32 too
~/bin/py -3.12 -m port.test.integrator segments --dll $SG_PORTSIM_DLL --jobs 3 [--backend win32] --out <tsv>
~/bin/py -3.12 -m port.test.integrator compare ../../../log/roadmap/merges/S1S8-4/segs_<x64|win32>.tsv <tsv>
~/bin/py -3.12 -m port.test.boot --all --jobs 3 --out ../../../log/roadmap/S7/7_boot_x64.tsv
cd ../../..; ~/bin/py -3.12 log/roadmap/merges/S1S8-4/boot_compare.py log/roadmap/S7/7_boot_x64.tsv
~/bin/py -3.12 log/roadmap/S7/7_equiv_compare.py; ~/bin/py -3.12 log/roadmap/S7/7_make_patches.py
```

## 5. Controls

| control | result |
|---|---|
| live.recorder 8, bio_type (focus keys left out of the recording) | replay differs at record 579 (g_golferTypes): RED |
| live.recorder 7, load_input (palette QPC after a load dispatch left out) | replay differs at record 1 (g_zoomScale, g_cachedCameraTileX): RED |
| live.recorder 8 / 7 on the trace host, cov_r_bio / cov_r_flic_input (S7.6's test_u1604_controls_red) | record 579 g_golferTypes / record 1 g_cachedCameraTileX: RED |
| S7.1's four and S7.6's controls in test_live.py | RED (test_live run, section 2) |

Equivalent mutant found and fixed: control 8 on a bio_type that never left the editor (above).
Blind on purpose: control 7 on bio_type, which has no load dispatch in its script part, and control 8 on load_input, which has no
focus key.

## 6. Open

- U-1660: a focus-only dispatch is not recordable. The app does not make one yet (U-1650).
- U-1662: the prefix session's script part has no original run to compare. That is S7.4's cross-check.
- U-1663: the script host's act rule in release waits.
- U-1650 can now be done: the app's focus callback from the port's focus state.
- S7.4 can start: its recordings replay through sg_run_boot, and the app writes trace.trace + framehash.bin per session as S7.6
  left it.

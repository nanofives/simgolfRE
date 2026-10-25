# S1.5 notes (U-1032: showCharacterEditor's bio entry over the EditBox family)

Worker: claude2 opus (re-pinned cov_r_bio after the lead's validate check, see the end), instance `instances\s1_5` (a copy of original\ with the PINNED shim winmm.dll 381ed2b2 copied from
instances\p4q), SIMGOLF_ALLOW_MULTI=1, C3FT_AUDIO=none. Nothing booted in original\; no commit, no stage.

## Result

| what | number |
|---|---|
| EditBox functions vs the original (Unicorn), 7 x 300 cases | 2,100 / 2,100 equal (SG_EMU_ORACLE=1) |
| the bio in showCharacterEditor vs the original with the key routing for real | 120 / 120 equal (212 bio entries, 100 bios changed, 27 at the 500-character capacity, 10 runs stopping inside a modal) |
| test_chareditor.py's oracle (now runs the bio for real) | 300 + 150 + 400 + 400 equal |
| pinned cov_r_bio (det_a == det_b == det_c, 7 checkpoints) | boot gate PASS 687 / 687 (HEAD build: FAIL at 579, MissingResource on the bio) |
| boot gate, all 65 | 62 PASS / 2 KNOWN / 1 NO_FRAMES, identical to the HEAD build except cov_r_bio |
| segment gate | 205 / 205 PASS x64 and Win32, every digest equal to the HEAD build's |
| controls | 13 (EditBox / bio golden) + 5 (cov_r_bio boot) RED, 0 blind; test_chareditor.py's 13 still RED |

U-1032 resolved (merge item 4); residual U-1190..U-1195 (`5_uncertain.tsv`).

## What the original does (measured, not assumed)

Probe runs of cov_r_bio with S15_DIAG=1 (the second script logs the routing functions; `log/c3/alt/refs/cov_r_bio/probe4`,
probe5; `5_probe*.log`):
- The bio loop 0x00439ed8..0x00439f0a (showScreen, redrawAll, leaveModal483d80, pumpMessages, Window::refreshFull) makes no
  timeGetTime call: the record run stopped with T frozen (probe2 / probe3). Hence the in-loop posting of the scenario (U-1193).
- The EditBox is the root window's focus child from EditBox::create on. WM_CHAR c: uiKeyEvent148 -> currentFocusOwner (the root)
  -> Window::onKeyChar(root) -> Window::key(root, c, [0x0083ab88]) -> Window::key(EditBox) -> EditBox::insertChar(c): 1 for
  any character outside 0..0x1f, and then no root handler runs; Enter (0x0d) and Back's WM_CHAR 8 return 0 there and reach
  rootWin_onKey. WM_KEYDOWN vk: uiKeyEvent(vk, 1) -> Window::onCommand -> dispatchAccel(root) -> (needKey keys, MapVirtualKeyA 0:
  Window::key(EditBox, 0, vk) twice, both 0) -> dispatchAccel(EditBox) -> EditBox::onKey(vk): 1 for 8, 0x23..0x28, 0x2e.
  Keys the EditBox ignores reach the root: rootWin_onChar(vk), or rootWin_onKey(0, vk) for a key with no character (the root's
  own Window::key path). Key-ups reach rootWin_onChar0. After the bio the root's ring count +0x140 is 0 (probe5 tree1).
- `Florence likes ... blue.` + "Hello Golf", Back, Left, Left, X, Home, A, Delete, Right, End, !, Enter gave
  `Alorence likes ... blue.Hello GXol!` (probe5 obs bio1), the text a caret model gives.
- In the 64 corpus traces every WM_KEYDOWN / WM_CHAR MSG record has a root HANDLER after it (11,312;
  `5/unmatched_keys2.py`): the 159 key-downs without HANDLER 8 have HANDLER 7 with a0 = 0 (arrows, Home, PgUp, F keys).

## Port (port/src/sim/chareditor.cpp)

- EditBox family, registered under the hooks.csv names (thiscall, a[0] = this): EditBox::setText, resizeBuffer, insertChar,
  onKey (Up / Down: MissingResource, U-1190), resetFields, releaseState, create. Text buffers come from a per-context arena
  mapped above the GameState (16-byte blocks, cursor reset when no block is live). Not modelled: the window-tree side (U-1191).
- The bio entry (`bio_entry`): create on g_characterEditorEditBox with the root window as parent, the blink flag, a 500-character
  buffer, setText(bio), the five style words, button word and gDirtyFlagB cleared; the loop pumps Blocking batches until a press
  or gDirtyFlagB == 0x0d; RootHandler commands 12 / 13 in a batch go to EditBox::onKey / insertChar (a refusal throws: the
  recording and the model disagree), all others to apply_command; the bio = "" + the EditBox's text; releaseState.
- Test entry `chareditor.peek(src, dst, n)` (reads the arena in tests).
- Negative controls: EditBox::insertChar 1, onKey 1 / 2, setText 1, resizeBuffer 1, resetFields 1, releaseState 1, create 1,
  showCharacterEditor 10 (exit on 0x0a), 11 (text not copied); tick.frame 8 lives in the tick.cpp patch.

## Shared files (not edited; `5_merge.md`)

tick.cpp parse_trace (commands 12 / 13 from MSG records no root handler follows; `5/patches.py`), commands.h and chareditor.h
comments, UNCERTAINTIES.md, CLAUDE.md.

## Oracle (port/test/test_chareditor_bio.py, SG_EMU_ORACLE=1)

See the module docstring. Two points that took measurement:
- cmdemu registers its own code hook on frameTick483cd0 in addition to any stub there, so a Python stub on that address runs
  twice per entry; the bio oracle replaces pumpMessages 0x0045c030 instead (its semantics around a trampoline).
- The port's commands for the original's run are derived like the trace recorder would: markers stored before each scripted
  message, the root handlers identified by the eip of their writes to gDirtyFlagA / B and g_char83821c; the derivation must
  equal the prediction the batches were cut by (a wrong prediction is reported as such, not as a state difference).
- The oracle's malloc uses the port arena's address (0x03040000) and rule, so the EditBox text pointers are equal; a run that
  stops inside the bio compares the live text too (port via chareditor.peek).

## Reproduce

```
~/bin/py -3.12 log/roadmap/S1/5/mirror.py                       # S15_BASE=1 for the HEAD build
cmake -S port/build/S1_5_root/port -B port/build/S1_5 -A x64 ; cmake --build port/build/S1_5 --config Release --target portsim_c
export SG_PORTSIM_DLL=$PWD/port/build/S1_5/Release/portsim_c.dll
SG_EMU_ORACLE=1 ~/bin/py -3.12 -m pytest port/test/test_chareditor_bio.py port/test/test_chareditor.py -q     # oracle + controls
~/bin/py -3.12 -m pytest port/test/test_chareditor_bio.py -q                     # default: goldens, controls, cov_r_bio boot
~/bin/py -3.12 -m port.test.boot --all --jobs 3 --out <tsv>
~/bin/py -3.12 -m port.test.integrator segments --dll $SG_PORTSIM_DLL --jobs 3 --out <tsv>
# the scenario (game slot): instances\s1_5 with winmm.dll 381ed2b2, SIMGOLF_ALLOW_MULTI=1 C3FT_AUDIO=none
~/bin/py -3.12 re/tools/scenario_refs.py regen cov_r_bio --rerecord --jobs 1 --instances s1_5
~/bin/py -3.12 re/tools/segment_corpus.py --instance s1_5 capture --scenario cov_r_bio --label c_a
~/bin/py -3.12 re/tools/segment_corpus.py --instance s1_5 trace --scenario cov_r_bio
# probes: S15_DIAG=1 ... scenario_refs.py run --scenario cov_r_bio --label probeN --record --instance s1_5 (never pinned)
```

## Port test files run (my DLL, one file per process; `5/suite/*.txt`)

test_chareditor 19 passed / 4 skipped (oracle opt-in), test_chareditor_bio 28 / 8, test_tick 250, test_commands 83 / 44,
test_boot 89, test_dialogs 27 / 7, test_segments 210 (no test_semantic.py exists). test_static: 5 passed, 1 failed in the shared tree (test_every_module_file_is_listed: a sibling's untracked
port/src/sim/audio_core.cpp not in modules.inc; not this step's).

## Open

U-1190 (Up / Down), U-1191 (window tree), U-1192 (uiKeyEvent callbacks), U-1193 (no trace replay, segment corpus or semantic kind for
cov_r_bio), U-1194 (press on the box, Tab, mdebug), U-1195 (dispatch order of a burst).

## golden_chareditor.json, before / after (pinned with SG_EMU_ORACLE=update after the oracle was GREEN on these cases)

Pre-S1.5 copy: log/roadmap/S1/5/golden_chareditor.pre_s15.json. Per key, sha256[:16] of the JSON list:

| key | before | after |
|---|---|---|
| showCharacterEditor (120) | ab30ac6f432200dc | bbc86d696723a51b (cases 43, 61, 90 only: the three whose script presses a non-member's bio) |
| pickCharEditor (80) | be2b5e2097de3b1d | unchanged |
| nearestCharControl (200) | c7922dab301a53c0 | unchanged |
| charEditorZone (200) | fa280e1186db9b7c | unchanged |
| bio (60), EditBox::create / insertChar / onKey / releaseState / resetFields / resizeBuffer / setText (120 each) | - | new: 43cd9cf8d2a9dc3c, 0f5ee3c193a4eaa2, 059ee440c9821e2a, 4cfbcc4301b74d73, 245474e1fa251a66, 056c0ea950cf28e7, d6be0cdb91cbb9d5, 5b4488b791ec191e |

| case | before | after |
|---|---|---|
| showCharacterEditor[43] | fae63f08eb9b3b2489f334f2:2874a71ddc82 | 44776287bb239c5a6ab29d5e:af2edbc78be6 |
| showCharacterEditor[61] | 05c99b9384c683f0cb34246e:3adb7036d040 | 2ef238565445f04ded4d35d5:52ca3e66c1a7 |
| showCharacterEditor[90] | 10ef263b82034eaeba1a01dc:fed5c9b0578a | 20d19c50d920ce5e9c2d6b25:41b6e360379c |

Before S1.5 these three were "port MissingResource, oracle stopped at EditBox::create" (equal by the old rule); now both run
the bio (log/roadmap/S1/5_golden_update_tc.log, 5_golden_update_bio.log).

## Re-record for trace_format.validate (S1.1's undelivered_input rule)

The first pinned cov_r_bio posted its bio keys from a SECOND Frida script, so its trace had 18 input MSG records with no harness
DELIVER before them. Now the cov_r_bio agent itself carries the in-loop part (scenario_s1_5.py INLOOP_JS, appended by a wrapper
of scenario_ab.Run.agent_source that a meta-path hook installs once scenario_ab is imported; other scenarios' agents unchanged),
armed by rpc s15Arm at the editor and posting through the agent's doAction, which trace_rec.js wraps: a DELIVER post precedes
every message. Probe6 (S15_DIAG=1) gave the same bios as probe5. Re-pinned: `regen cov_r_bio --rerecord` PINNED (b/c and a/b
SAME, 7 checkpoints; manifest: only cov_r_bio's entry changed), c_a SAME as det_b, trace 928,574 B `validate OK` (trace_tool
validate OK; 150 DELIVER posts, 0 undelivered). The keys' structure is still the parse rule's input: 17 WM_CHAR and 8 WM_KEYDOWN
MSG records with no root handler (the 17 typed characters; Back, Left x2, Home, Delete, Right, End x2), every other key MSG
followed by its root handler (`5/unmatched_keys2.py port/corpus/traces/cov_r_bio.trace`). Boot gate row: cov_r_bio PASS 687 /
687 (HEAD build FAIL 579 / 687); test_chareditor_bio.py 28 passed / 8 skipped (oracle opt-in), its 5 cov_r_bio controls RED.
No other gate row depends on the recording (cov_r_bio has no segments).

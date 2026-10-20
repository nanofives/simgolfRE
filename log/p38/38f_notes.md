# Stream 38f: the integrator gates (2026-10-20)

Plan: re/PHASE38_PLAN.md section 3; brief log/p38/BRIEF.md. Tools: `port/test/integrator.py` (segments, chain,
check-slices, speed, defects, ledger, compare), `port/test/semantic.py` (gate d), tests `port/test/test_integrator.py`
(15: 10 static, 4 port, 1 x87); module `integrator` (port/src/sim/integrator.cpp + portsim/sim/integrator.h, test-only);
private builds `log/p38/f_mirror.py`; the 6-year recording `log/p38/cs6y_pipeline.py`. Run outputs in `log/p38/38f/`.
U-rows: `log/p38/38f_uncertain.tsv` (U-1010 .. U-1014). Merge lines: `log/p38/38f_merge.md`. Files: `log/p38/38f_files.txt`.

Final numbers below are on the MERGED build: the working tree at 2026-10-20 02:38 (38a, 38b, 38c, 38e merged; 27 modules)
plus the integrator module and the two tick.cpp lines (log/p38/38f_tick.patch), port/build/38f_run/x64_20261020_023850 and
Win32_20261020_023938. 38d was not merged then.

## 1. Gate summary

| gate | result |
|---|---|
| (c) chained runs | course_season: one run of 6,127 frame records, all 10 checkpoints PASS. course_season_6y: one run of 50,923 frame records (6.22 game years, 6 year ends), all 10 checkpoints PASS, x64 and Win32 identical. Every chain of every scenario: 350 runs, 308 PASS, 8 BREAK (mainLoop exits to the title menu at a segment boundary), 34 FAIL, all with a cause (missing names, measured title-menu exits) |
| (d) championship by semantic commands | not reachable with the API as it is: 131 of 145 recorded dispatches have no semantic kind (keys, course clicks); the 7 translated panel clicks lose mainLoop H2's own press effects (U-1011). Runs: semantic-only diverges at the first Esc (records 9 / 10 / 26), hybrid at the frame after the first translated click (178 / 130 / 250) |
| (e) ledger | re/port/divergences.tsv, 74 rows, 73 with a cause; the one without: chooseShotTarget.strictFlagSticky, no segment or test changes with it OFF (U-1012) |
| (f) speed | the gate as written has no instance in the original (U-1010: input-gated waits). Longest no-input stretch, 6,188 frames: x64 20.4 s (303 fps), Win32 23.2 s (267 fps), digests equal. Recorded input over 8,192+ frames: 10,167 frames in 28.6 s x64 / 34.7 s Win32; 50,923 frames in 141.7 s (359 fps) / 178.9 s (285 fps). Budget for 8,192 frames: 713 s |
| backends | the whole segment gate, 200 segments: x64 = Win32 in every row (status, frames, record, objects, per-frame digest); 181 PASS / 19 FAIL on both |

## 2. Builds

`~/bin/py -3.12 log/p38/f_mirror.py [--rev HEAD | --merged] [--mine]` snapshots port/ + re/model/include into
port/build/38f_root, builds x64 (port/build/38f) and Win32 (port/build/38f_w32, with portsim_runner32.exe), copies the
outputs to port/build/38f_run/<arch>_<stamp>/ and records them in port/build/38f_run/latest.json (read by `--build 38f`).
`--merged` = the working tree with only the module .cpp files modules.inc lists (siblings' unmerged modules stay out).
`--mine` overlays the integrator module, adds its SG_MODULE line and applies the two tick.cpp lines as anchored text edits
(git apply run inside the repository's ignored build folder applied nothing and reported success). Compile parallelism 3.

Library selection in integrator.py: `--dll` (x64), `--dll32 --runner32` (Win32) or `--build 38f`. Win32 with explicit paths
patches port/test/build.py's path functions in the integrator's own process only, so port/build/Win32 is never built or used.
`run_from` sets up a run exactly as the merged SegmentRunner does (38b's file overlay, 38a's GDI TextMetrics provider
attached and detached; SG_TEXT_METRICS=none turns it off).

## 3. Gate (c): chained runs

`integrator chain`: the accepted segments of a scenario whose windows touch (segment k+1 starts at the checkpoint segment k
ends at, and its first frame head is the one after segment k's last record) form a chain. For every checkpoint in the
chain (every entry of every segment's checkpoints.json, intermediate ones included, which the segment gate never checks)
one sg_run_frames call starts from the chain's FIRST segment image and runs through a slice of the scenario's whole trace
(port/corpus/traces/<s>.trace) from that segment's start frame head to the checkpoint's POLL, cut by
re/tools/segment_corpus.py's own slice_trace: no restart in between. Every frame record is compared with the segments'
frames.bin records concatenated, the state at the checkpoint with its sim hashes. Self-check (`check-slices`,
test_chain_slices_equal_the_segment_traces): the slice from a segment's own start to its end checkpoint equals its
trace.trace byte for byte (19 / 19 for course_season, golfer_stories, championship and the three story / mood fixtures).

Two rules the chained runs forced (both found by chains, invisible to the segment gate):
1. Writes riding on a checkpoint's own poll. The original sends a poll's checkpoint dump before it applies the poll's
   actions (re/frida/js/scenario_det.js lines 850 / 854: `reply.dump` sent, then `reply.actions`), but the trace records the
   DELIVER w32 writes at the poll's clock position and the frame step applies them at its frame wait, before the comparison.
   cov_k3_camera2 checkpoint y_inj0 (inside segment 02): the poll writes gCameraTileY = 54 (0x004c2ba4); the original's
   checkpoint holds 3 (recovered from its sgh64), the port had 54. The chain runner drops the DELIVER w32 records at the final
   poll's position from its slices (`drop_final_poll_writes`, column dropped_poll_writes): they come after the checkpoint.
   No accepted segment's end checkpoint carries such writes.
2. mainLoop exits at a segment boundary. cov_b_shots / cov_b_shots2 finish each championship round on the results screen
   and start the next one from the title menu (re/tools/scenario.py `_cov_b_champ_rounds`); at the boundary after
   `results_r<n>` the two frame heads are 23.3 .. 23.7 virtual s apart (99 ms otherwise) and the port's frame takes
   FrameExit::Menu at mainLoop ML+0xf106 ((g_flags & 0x4200000) == 0x4000000) at exactly the next segment's first record.
   What runs before that frame head (title menu, championship set-up) is not frame-step code. Such a stop is a BREAK; the
   chain's later checkpoints run again from the next segment's image. Breaks: cov_b_shots at segment 3, cov_b_shots2 at 2
   and 4 (8 BREAK rows). Also a checkpoint polled inside the title menu after a measured in-segment exit (38a's four
   segments; cov_k1_animals/00's `menu2`, its poll rides on waitTicks, which the menu loop calls) is explained, not a port
   failure.

Results (x64, merged build; log/p38/38f/chain_x64.tsv):
- course_season: 6,127 frame records (frames 167 .. 6,294) from segment 0's image, all 10 checkpoints equal (22.5 s);
  golfer_stories 3 / 3 (1,915 frames); championship 3 / 3 (1,609 frames; on HEAD the third stopped at showHoleStatsScreen);
  stories_fixture 123, stories9_fixture 411, mood5_fixture 122 frames (one segment each, spanning `view` .. `season_end`).
- course_season_6y (section 4): 50,923 frame records, all 10 checkpoints (season_500 .. season_5000) PASS.
- Every scenario with accepted segments (54; 36 of them have a chain of two or more segments; 93 chains after the breaks):
  350 runs, 308 PASS, 8 BREAK, 34 FAIL. No run stops on Text_put any more. The FAILs: showRoutingMapScreen 12,
  drawGolferInfoPanel 11, the title-menu exits inside 38a's four measured segments 6, showBulletinBoard 4, and
  cov_k1_animals' `menu2` checkpoint taken in the title menu 1; each has its cause in the ledger, and no chain fails where all
  its segments pass.
- Negative control: tick.frame mode 1 (one extra draw of the game's generator per frame) fails a chain at record 1
  (test_chain_reads_red_on_a_wrong_frame).

## 4. The multi-year recording (course_season_6y)

Scenario `course_season_6y` (re/frida/scenario_ab.py): course_season with a 5,000 virtual s season and a checkpoint every
500 s. A probe through the first year end showed the date passing 8,192 with no stall (the Esc every 2 s ends the year-end
screen). Pinned with `re/tools/scenario_refs.py regen course_season_6y --instances p38f p38f2` (instances: copies of
original\ with the pinned references' winmm.dll 381ed2b2, C3FT_AUDIO=none): det_a = det_b = det_c at all 12 checkpoints,
50,216 polls; re/analysis/data/scenario_refs.json entry course_season_6y (schedule sha256 670bfb9c...).

GAME-DATE SPAN (the checkpoints' g_date; one unit per unpaused frame, 8,192 a year, re/analysis/sim/time_model.md):
sandbox 0x16 (22), hole_open 0xa7 (167), season_500 0x1493 (5,267), season_1000 0x285f (10,335, May 2002 on screen), ...,
season_5000 0xc793 (51,091: year 2007, month index 1). From the hole opening to the end: 50,924 units = 6.22 game years; from
the sandbox start 6.23. Each 500 virtual s advances the date by 5,068 .. 5,101 units (about 10.1 per virtual s); the
recording crosses the year ends at 8,192, 16,384, 24,576, 32,768, 40,960 and 49,152.

Segment corpus (`log/p38/cs6y_pipeline.py` = segment_corpus.py's steps in order over two instances, no per-step timeout):
captures c_a, c_b, c_c SAME as det_b (c_c needed four attempts: three died with MemoryError, U-1014); trace 63,523,090
bytes, validates, SAME as the reference at 12 checkpoints (port/corpus/traces/course_season_6y.trace, local); plan: 11
captures agree, prefix `sandbox`, 10 segments, the continuous runs agree on all 51,092 frame records, trace join exact;
plan first failed with struct.error (the header's u32 q0 overflows past 2^32 QPC counts; trace_format.py now keeps the
whole value in meta q0_64, 38f_merge.md section 4); 30 restarts (r1, r2, cf per segment, 42 .. 113 s each): all 10
segments ACCEPTED, class COMMAND (5,067 .. 5,101 frames each, 50,914 in all); extracted to
port/corpus/segments/course_season_6y/00..09, manifests updated (203 segments, 200 accepted).

Port results (merged build): the 10 segments each PASS (about 14 s each, x64; Win32 identical); the end-to-end chain from
segment 0's image passes every frame record and checkpoint (section 1 table, log/p38/38f/chain_{x64,win32}_cs6y.tsv).

## 5. Gate (d): championship through the command API

`port/test/semantic.py translate`: every recorded dispatch (port/test/trace_commands.py batches) of championship/00..02 is
classified from the port's raw run. A press whose click reaches one of the seven click handlers that have a semantic kind
is translated into that kind with arg[0] = the handler's own hit test evaluated on the state the handler was called with
(SG_TICK_DUMP pre-state, the hit test called through sg_call); its mouse-down belongs to the same gesture.
Merged build (log/p38/38f/semantic_translation_merged.tsv): 145 dispatches; 7 presses reach clickGolferPanel (the
shot-style oval at (415, 508): GolferPanelAction(4) x 5, GolferPanelAction(-1) x 2) plus their 7 mouse-downs; 131 have no
semantic translation: 50 Esc presses (onChar + onKey 0x1b), 57 key releases (onChar0), 3 'X' and 3 'Z' presses (zoom), 9
presses no click handler took (the aim / swing clicks on the course and shot-style clicks the panel did not take), 9
mouse-downs of those.

`semantic.py run` (sg_run_frames with the integrator module's batch overrides: the pump call that took a recorded batch
applies the replacement; log/p38/38f/semantic_runs_merged.tsv):
- semantic (no RootHandler command at all): FAIL at records 9 / 10 / 26 (g_cachedCameraTileX, unk_005a9ccc: the first Esc's
  effect, as the corpus's counterfactual says).
- hybrid (only the translated gestures replaced): FAIL at records 178 / 130 / 250, each the frame head after the first
  GolferPanelAction, in g_cachedCameraTileX (recorded -1, semantic 0x19) and g_frameMs (recorded 1, semantic 0). Cause in
  mainLoop H2's own press code around the clickGolferPanel call 0x0041e48c: g_cachedCameraTileX = -1 (0x0041e3b1),
  screenToTile of the click, the button word [g_rootInput+0x10] cleared (0x0041e49a), the return to the frame head when
  the handler returns <= 0 (0x0041e4a0). The semantic kinds carry only the handler's effect (U-1011).
- raw: PASS / PASS / PASS.
Verdict: the command API as it stands cannot drive a championship. Test: test_semantic_golfer_panel_click_misses_the_frame_effect
(the integrator's negative control 1, replacements ignored, makes the same run PASS).

## 6. Gate (e): the divergence ledger

`~/bin/py -3.12 -m port.test.integrator ledger [--run --dll <dll>]` writes re/port/divergences.tsv from three inputs
(log/p38/38f/segments_x64.tsv, chain_x64.tsv, defects_x64.tsv; `--run` regenerates them, about 20 minutes): every
accepted segment that is not PASS (the missing name with the owner stream and U-rows, a measured title-menu exit, or the
divergent record and objects), every rejected segment (manifest reason; golfer_events/04 = U-0681), every pinned scenario
with no segment (cause from its plan.json), every chain run that is not PASS (BREAK rows apart), and every compatibility
switch with its evidence. Rows without a cause are printed and make the command return 1.

Current ledger: 74 rows: 19 segments (drawGolferInfoPanel 9 and showBulletinBoard 2, showCharacterIntro 2: 38d;
showRoutingMapScreen 2: no owner; 4 measured title-menu exits), 3 rejected segments, 5 segment-less scenarios, 34 chain
rows, 8 chain breaks, 5 switches. One row without a cause: chooseShotTarget.strictFlagSticky.

Defect switches (`integrator defects`: each switch OFF over all segments, digests compared with the ON run; merged build):
sinScaled.readPastTable changes 33 of 190 (and all 10 of course_season_6y); prepareShot.hazardCellTransposed 10;
shotPower.cacheIgnoresPutt 0 segments, its vector test changes the putt vectors; narrateShot.maskNotRestored 0 segments,
state test test_defect_narrate_mask_not_restored; chooseShotTarget.strictFlagSticky: 0 of 200 segments, no test, 480
mutated inputs of its 12 vectors gave ON == OFF (U-1012).

## 7. Gate (f): speed

`integrator speed --segment <s/k> --frames N`: a trace with no input record built from the segment's own most frequent
input-free frame (frame-head call, 10 QPC calls, 3 cursor reads at the harness's cursor, one keyboard-state read, the H1 wait's
98 timeGetTime calls: 99 virtual ms between frame heads), repeated N + 1 times, one final POLL at the last frame head. (A
first version without cursor records left the cursor at (0, 0) and the camera edge-scrolled into a UI path.)

THE GATE AS WRITTEN HAS NO INSTANCE IN THE ORIGINAL (U-1010): with no input the game blocks in input-gated waits.
(1) From course_season/00's image the run stops after 1,426 frames (frame 1,593 since boot) in pumpAndRedraw(0) at mainLoop
0x0041e283, on the [0x004c2e0c] != -1 arm (U-1013); course_season's own trace shows the original waiting 440 virtual ms there
until the recorded Esc. (2) At every year end showEndOfYearScreen waits for a left click or a key (log/p38/38c_notes.md),
and every 8,192-frame window contains a year end. From course_season/03, 05, 07, 09 the no-input runs stop at date 8,192
(6,188 / 4,962 / 3,736 / 2,510 frames).

Measured: no-input, course_season/03 to the year end, 6,188 frames: x64 20.4 s (303 fps), Win32 23.2 s (267 fps), the same
digest (budget for 6,188 frames: 538 s); course_season/00, 1,426 frames: x64 5.8 s, Win32 6.7 s (HEAD build). Recorded
input: course_season_6y to season_1000, 10,167 frames: x64 28.6 s, Win32 34.7 s; to season_5000, 50,923 frames: x64 141.7 s
(359 fps), Win32 178.9 s (285 fps). 8,192 frames therefore take 23 s (x64) to 29 s (Win32) against the 713 s budget.

## 8. Backends

The whole segment gate on Win32 (`integrator segments --backend win32`, then `integrator compare`), digest = sha256 over the
return code, frame count, every returned T and 203 hashes, and the end-check objects:
- HEAD: 190 / 190 rows identical (144 PASS / 46 FAIL on both); the hooked HEAD build equals the unhooked one on all 190.
- merged build: 200 / 200 rows identical (181 PASS / 19 FAIL on both, course_season_6y included).
- chains: the 10 course_season_6y runs identical on both backends (digest per run).

## 9. Run hygiene and incidents

- Memory (U-1014): from about 01:50 the machine's commit charge sat at its limit (63.8 .. 64.2 GB of 65.4 .. 68.6 GB; the
  processes' private bytes summed to 24.7 GB; 38d's full-suite pytest, pid 34832, held 10.6 .. 10.9 GB until the parent
  killed it (log/p38/MERGES.md), and a second pytest, pid 54976 from 02:28, held 10.4 GB). MemoryError killed course_season_6y's capture c_c three times and the trace once; one build failed (Lib.exe
  1102), Git Bash failed to fork. My own parallel gate runs (2 .. 3 workers, a few hundred MB each) added to it: run nothing
  heavy beside a capture.
- Instances p38f and p38f2 (instances\, copies of original\ + winmm.dll 381ed2b2 from instances\p24); every game I started
  was a child of my runners; none is left running; purge_frida_temp found nothing to remove.
- Git Bash heredocs turned `\\n` / `\\0` escapes in Python patch scripts into real newlines / a NUL byte (twice); use the
  Edit tool for such text.

## 10. Status and next steps (for a fresh session)

Done: every task of the brief (gates c .. f, the multi-year recording, the Win32 gate), notes, merge file, U-rows, tests, and the parent's question on the title-menu section (section 11: yes, as the next stream).
For the parent: merge 38f_merge.md (modules.inc line, the tick.cpp patch, U-rows); then re-run on the merged build:
`~/bin/py -3.12 -m port.test.integrator segments --dll <x64 dll> --jobs 2` (+ `--backend win32 --dll32 <dll> --runner32 <exe>`,
then `integrator compare`), `integrator chain --all --dll <dll>`, `integrator ledger --run --dll <dll>`. When 38d lands,
drawGolferInfoPanel / showBulletinBoard / showCharacterIntro rows should leave the ledger. Open: U-1010 (the user decides how
gate f is read), U-1011 (a semantic layer for presses and keys), U-1012 (a test for strictFlagSticky), U-1014.

## 11. mainLoop's title-menu section: should sg_run_frames run it? (the parent's question, decided here)

What it is (re/analysis/sim/mainLoop_structure.md): FrameExit::Menu (ML+0xf106, ML+0x11875) jumps to mainLoop's own TITLE MENU
0x00420e35..0x00421610 (session reset: g_date = 0, the flag word, buildGolferBioText, initRecords; a hover / click loop over 6
hotspots; selectDifficultyScreen, showThemePackSelect, the championship flags) and its SESSION DISPATCH 0x0040f9fc..0x0040fc24
(loadThemes, then startNewCourse + loadWorldAssets for a sandbox, loadGameBrowserScreen for a saved game, or courseActionMenu +
generateRandomCourse + rebuildHeightfield for a new course), then back to the frame head. Both sections have C3 transcriptions
in shim/src/re/c3fy.cpp (parts MV and SV), the source 3.7a converted the frame from.

Measured (log/p38/38f_menu_gap.py -> log/p38/38f/menu_gap.json): in the 7 stretches the corpus holds (cov_b_career_types/00-02,
cov_k1_animals/00, the three cov_b_shots round starts) the original spends 11.6 .. 23.7 virtual s between the exit frame head
and the next one; every main-thread clock call there is waitTicks' (22,819 of 22,822 in cov_b_career_types/00, the other 3
mainLoop's), and 4 .. 7 clicks are dispatched (onPoint / onPress1 / onUnpoint). The frames after it are in the corpus already:
the segments' records continue past the exit (cov_b_career_types/00: 49 more frame heads), and the cov_b_shots chains hold the
next rounds.

Callees, registered in the merged build: buildGolferBioText, initRecords, View_toLocal, distance, selectDifficultyScreen,
showLoadingScreen, seedCourseTable, loadThemes, courseActionMenu, loadGameBrowserScreen, generateRandomCourse,
rebuildHeightfield, promptAndSaveFile, pumpLight; startSoundSlot and Window::refreshFull are documented drops. Not
registered: showThemePackSelect (theme path), startNewCourse and loadWorldAssets (sandbox and saved-game paths), and
playAudioFile / showCreditsScreen (QUIT only, which ends mainLoop).

Decision: YES, sg_run_frames should run the title-menu and session sections. They are mainLoop's code, between two frame heads
of the same call; what they write (the session reset, the new course) is simulation state every later frame reads; a
headless game that plays a second round or starts a new game goes through them; and the corpus already has the expected
frames on the far side. NOT in 3.8: it is new port code, not integration: convert c3fy.cpp's MV / SV parts with
log/p3/p37a_conv.py as 3.7a did for the frame, return a new FrameExit (or continue the loop) instead of stopping, give
TraceInput a menu-loop pump class (its dispatches come through the menu's own waitTicks loop), and port startNewCourse and
loadWorldAssets' simulation writes (or show there are none) for the sandbox path. The career and championship paths of the
7 measured stretches need only the menu / session conversion: their other callees are registered. Gate: the 4 segments, the 3
cov_b_shots boundaries (then the chains need no BREAK) and cov_k1_animals' `menu2` checkpoint must PASS; until then the ledger
carries them as explained rows.

## CLAUDE.md paragraph (suggested)

Phase 3.8 integrator (2026-10-20, stream 38f, log/p38/38f_notes.md): `port/test/integrator.py` runs the whole segment gate on
either backend with a per-frame digest (`segments`, `compare`: 200 / 200 rows equal x64 vs Win32), CHAINED runs (`chain`:
one sg_run_frames from a chain's first segment image through a slice of the scenario's whole trace to every checkpoint,
intermediate ones included; a mainLoop exit to the title menu at a segment boundary is a BREAK; writes made in a checkpoint's
own poll are dropped from the slice because the original dumps before it applies a poll's actions), the no-input speed trace
(`speed`), the defect-switch sweep (`defects`) and the ledger re/port/divergences.tsv (`ledger [--run]`). course_season runs
as one 6,127-frame chain; the new pinned scenario course_season_6y (re/frida/scenario_ab.py, 5,000 virtual s, 10 accepted
COMMAND segments of about 5,090 frames, date 167 -> 51,091 = 6.22 game years) runs as one 50,923-frame chain, every
checkpoint equal, x64 141.7 s, Win32 178.9 s. A no-input run of the original cannot last 8,192 frames (pumpAndRedraw(0) at
0x0041e283, the year-end screen's key wait: U-1010). `port/test/semantic.py` drives a segment with semantic commands through
the test-only `integrator` module's batch overrides: championship's keys and course clicks have no semantic kind and a
translated golfer-panel click loses mainLoop H2's press effects (U-1011). trace_format.py keeps a q0 past 2^32 in meta q0_64.
Private builds: log/p38/f_mirror.py (`--merged --mine`).

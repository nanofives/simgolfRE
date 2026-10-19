# Phase 2.4: the segment corpus (2026-10-18)

Step 2.4 of re/PHASE23_PLAN.md. A SEGMENT is whole-run test data the port must reproduce: a start state image taken at a
mainLoop frame head, the recorded clock / input trace from that frame on, and the expected per-object hashes at every
later frame head and at the checkpoints up to the segment's end. Tool: `re/tools/segment_corpus.py`. Running notes:
`log/p2/p24_notes.txt`. Corpus (local, gitignored): `port/corpus/segments/<scenario>/<k>/` and `port/corpus/segments.json`;
committed manifest: `re/port/segments_manifest.tsv`. Every number below is from a run listed in the notes or in
`log/p2/p24/<scenario>/{plan,verify}.json`.

## 1. Method

Validity rule: phase 2.1's (re/analysis/data/premise_check.md section 7). A segment starts at mainLoop's frame head (the
main-thread timeGetTime call at 0x0040fc29, return address 0x0040fc2f) of a continuous run, from an image of the whole
.data section (0x004c1000, 0x381174 bytes) with the per-process dwords masked; never from a save.

Per pinned scenario (re/analysis/data/scenario_refs.json, 59 scenarios), every hook off, C3FT_AUDIO=none, the b5 stack
seed, winmm.dll 381ed2b2 (the pinned references' build), instances p24 / p24b / p24c:

1. **Three continuous capture runs** `c_a`, `c_b`, `c_c` of the pinned schedule. After every checkpoint but the last, a
   frame-head capture is armed (restart_ab.py's IMAGE block, `cfg.image`); at most one is armed at a time (a second
   request would replace the armed watch, so a checkpoint inside a modal screen waits for the previous capture). Every
   frame head is hashed natively (C3FT_FRAMEHASH=sim: the 203 sim-class objects of port/include/gamestate_meta.inc, sgh64)
   and every checkpoint gets the object hashes of all 989 objects (C3FT_OBJHASH=1). Each run is compared with the pinned
   `det_b` (scenario_refs.compare).
2. **Trace**: the scenario's clock / input trace (re/tools/trace_tool.py's recorder, schema re/model/trace_schema.md) into
   `port/corpus/traces/<scenario>.trace`. The trace's FRAME values and indices must equal the frame-hash records' T and
   frame index, record for record (`trace_join` in plan.json).
3. **Plan**: the RESTART PREFIX is the first checkpoint taken inside the frame loop (its capture met the frame head within
   250 main-thread clock calls) with no posted message waiting in the game's queue there (section 4.2); segments start at
   the later captures. A segment ends at the first checkpoint after its
   start that leaves at least 20 frame heads in between; captures inside a segment are skipped; with more than 6 candidates
   (10 for course_season) an evenly spaced subset is kept. The three continuous runs must agree on every frame record of
   the window.
4. **Determinism proof**: two RESTARTS per segment (`r1`, `r2`). A fresh process replays the scenario up to the prefix
   checkpoint, waits for the next frame head, writes c_a's start image there (restart_ab.py `data+` variant: all of .data
   except the dwords that differ in c_b or c_c at the capture instant, the 12 model flag-gap dwords of
   premise_check_flag_gaps.tsv, and, inside pointer-flagged objects and unmodeled bytes, every dword that holds a pointer
   in either process; the first 239 restarts used `data`, see section 4.2), hands over the captured clock value, T, the QPC
   counter, pending events, virtual timers and virtual input, posts again the messages that were still queued at the
   start (section 4.2), and continues c_a's schedule to the segment's end checkpoint. ACCEPTED when, in both restarts,
   every frame record after the start (aligned in order) has the expected T and all 203 sim hashes, the record count is
   equal, and the end checkpoint's sim-class object hashes are equal. Any divergence rejects the segment with the first
   divergent record, its T and its objects.
5. **Classification** from the trace slice and one more restart: see section 3.

## 2. What a segment holds (`port/corpus/segments/<scenario>/<k>/`)

| file | content |
|---|---|
| `start.data.bin` | golf_clean.exe's .data section at the start frame head (0x381174 bytes = `portsim::GameState`, port/include/gamestate.h) |
| `noise.bin` | dword bitmap (bit i, LSB first, = the dword at offset 4i): the dwords that differed between the three continuous runs at that instant (heap pointers, handles) plus the 12 flag-gap dwords |
| `start.json` | scenario, segment, start / end checkpoint, class and reason; the start instant: `frame_value` (what the frame-head timeGetTime returned), the agent's T, QPC (Qlo / Qhi), frame-head index since boot, pending events, virtual timers, virtual cursor / keys; `queued_messages` (posted messages still in the game's queue at the start); noise statistics (`sim_bytes`: noise bytes inside sim-class objects); `start_frame_hash_check`; the trace slice and input summary |
| `trace.trace` | the trace slice: a stand-alone, valid schema v1 trace starting with the frame-head clock call of the start frame (ordinal 0, value = `frame_value` = header t0) and ending with the end checkpoint's POLL (final), plus a FRAME of that same clock call if one follows; ordinals, frame and poll indices rebased to 0, q0 = the slice's first QPC value; `meta.slice` keeps the source offsets |
| `frames.bin` / `frames.json` | expected frame records: u32 T (the frame-head clock value), u32 frame-head index since boot, u64 sgh64 x 203 (symbols in frames.json). Record 0 is the start frame: its hashes equal the sim hashes of start.data.bin (checked: `start_frame_hash_check`); record k is the k-th frame head after it |
| `checkpoints.json` | per checkpoint inside the segment (the end one included): T and `{symbol: [class, sgh64]}` for all 989 objects |
| `verify.json` | the two restarts' results (frames, end-checkpoint classes, restore statistics) and the counterfactual's |

## 3. Classes

| class | rule (measured) | corpus |
|---|---|---|
| TIME-FORWARD | the trace slice holds no input record: no DELIVER (harness cursor, virtual key, posted message, fixture write, timer), no MSG, no root-window HANDLER entry, no H1 TAKE | yes |
| COMMAND | input records present, and a COUNTERFACTUAL restart (same image, every harness action after the restore dropped, the captured pending actions too) diverges from the expected sim hashes at some frame or at the end checkpoint: the inputs changed simulation state. Its first divergent record and objects are recorded as the first effect of the command (`class_reason`) | yes |
| UI-ONLY | input records present, but the counterfactual equals the expected sim hashes at every frame and at the end checkpoint: the inputs reached only UI / rendering state | no: excluded (a headless simulation has nothing to test there beyond what a TIME-FORWARD segment tests) |

The input summary per segment (start.json `inputs`, plan.json) counts deliveries by kind, messages by number, handler
entries by name, H1 takes (which / value), frames, frame-head intervals, the time spent outside the frame loop
(`modal_ms`: intervals over 200 virtual ms, minus 99), paused frames (g_flags bit 2) and RNG changes from the FRAME
snapshots.

## 4. Results (2026-10-18/19, LIVING-PC, instances p24 / p24b / p24c / p24d / p24t)

All 59 pinned scenarios were run: 177 continuous capture runs (3 per scenario), every one SAME as its pinned `det_b`
(scenario_refs.compare); 193 planned segments, 386 restarts `r1` / `r2`, 155 counterfactual restarts; about 4.1 hours of
restart wall time. Every trace slice's FRAME values and indices equal the frame-hash records of the capture runs, record
for record, in every scenario with a trace (`trace_join`).

**190 segments ACCEPTED in 53 scenarios: 35 TIME-FORWARD (1,534 frames), 155 COMMAND (56,260 frames); 0 UI-ONLY; 3
REJECTED.** Every accepted segment: start-frame record equal to the image's sim hashes (190 / 190), 0 noise bytes inside
sim-class objects (190 / 190), a trace slice that passes trace_format.validate (190 / 190).

| class | segments | frames |
|---|---|---|
| TIME-FORWARD | 35 | 1,534 |
| COMMAND | 155 | 56,260 |
| UI-ONLY | 0 | 0 |
| rejected | 3 | 183 |

Rejected (reasons from verify.json):

- golfer_events segment 4 (season_0200 -> landmark): its slice holds a DELIVER of a kind schema v1 does not define:
  trace_rec.js wrote the scenario's injected landmark notice (scenario_det action `call` to 0x004722c0) as kind 0 with no
  fields, and trace_format rejects the whole file. That trace is kept at `log/p2/p24/golfer_events/trace_rec.trace`, out of
  port/corpus/traces; the other 5 segments of the scenario are accepted (U-0681).
- cov_a_quit segments 0 and 1: no trace. The scenario ends with the game quitting (the c3fya runner's "game exited as
  expected: code 0"), and trace_tool reports no complete trace (trace_rec.js sends its last chunk after the final poll,
  which never comes). Both restarts of both segments reproduced every frame and the end checkpoint; only the trace is
  missing.

Scenarios without an accepted segment, and why:

| scenario | reason |
|---|---|
| cov_a_quit_title, cov_b_location | no capture met mainLoop's frame head within 250 clock calls of a checkpoint: every checkpoint is on the title menu, the location screen or the quit / credits screens |
| cov_b_locations, cov_k3_career, cov_k1_steps | no segment of at least 20 frame heads after the prefix (1 or 2 in-frame captures, the next checkpoint too close or none) |
| cov_a_quit | 2 segments rejected: no trace (above) |

Per scenario (`segment_corpus.py report`; captures = frame-head captures, in frame = met within 250 clock calls):

| scenario | captures (in frame) | prefix | segments | accepted TF / CMD | excluded UI-ONLY | rejected | frames (accepted) | note |
|---|---|---|---|---|---|---|---|---|
| cov_a_build3 | 7 (6) | amen_sub0 | 5 | 1 / 4 | 0 | 0 | 1043 |  |
| cov_b_inject2 | 4 (3) | course | 2 | 0 / 2 | 0 | 0 | 750 |  |
| cov_a_career | 13 (11) | game | 6 | 1 / 5 | 0 | 0 | 4151 |  |
| cov_b_career | 5 (5) | career | 4 | 0 / 4 | 0 | 0 | 1487 |  |
| championship | 5 (3) | tournament_start | 3 | 0 / 3 | 0 | 0 | 1607 |  |
| course_season | 11 (11) | sandbox | 10 | 0 / 10 | 0 | 0 | 6118 |  |
| golfer_events | 9 (7) | sandbox | 6 | 0 / 5 | 0 | 1 | 2241 | 4: trace slice: the slice holds 1 DELIVER record(s) of a kind schema v1 does not define (unkn |
| golfer_stories | 5 (4) | sandbox | 3 | 0 / 3 | 0 | 0 | 1913 |  |
| mood5_fixture | 3 (2) | loaded | 1 | 0 / 1 | 0 | 0 | 122 |  |
| sandbox_basic | 9 (8) | sandbox_00 | 6 | 6 / 0 | 0 | 0 | 306 |  |
| stories9_fixture | 3 (2) | loaded | 1 | 0 / 1 | 0 | 0 | 411 |  |
| stories_fixture | 3 (2) | loaded | 1 | 0 / 1 | 0 | 0 | 123 |  |
| ui_tour | 8 (5) | game | 6 | 0 / 6 | 0 | 0 | 1237 |  |
| ui_tour_v | 8 (7) | game | 6 | 0 / 6 | 0 | 0 | 1244 |  |
| cov_b_build | 6 (6) | sandbox | 5 | 0 / 5 | 0 | 0 | 4033 |  |
| cov_b_career_types | 11 (11) | career_scotland | 6 | 0 / 6 | 0 | 0 | 1075 |  |
| cov_b_decor | 4 (4) | sandbox | 3 | 0 / 3 | 0 | 0 | 3645 |  |
| cov_b_holes18 | 5 (5) | zoom1 | 4 | 0 / 4 | 0 | 0 | 3715 |  |
| cov_b_inject | 4 (4) | course | 3 | 0 / 3 | 0 | 0 | 839 |  |
| cov_b_locations | 1 (1) | phoenix | 0 | 0 / 0 | 0 | 0 | 0 | no segment of >= 20 frames after the prefix |
| cov_b_paths | 3 (3) | paths | 2 | 0 / 2 | 0 | 0 | 695 |  |
| cov_b_people | 4 (4) | course | 3 | 0 / 3 | 0 | 0 | 993 |  |
| cov_b_shots | 6 (4) | tournament_start | 5 | 1 / 4 | 0 | 0 | 5670 |  |
| cov_b_title | 3 (2) | career | 1 | 0 / 1 | 0 | 0 | 305 |  |
| cov_a_build | 19 (16) | terrain_03 | 6 | 1 / 5 | 0 | 0 | 1032 |  |
| cov_a_build2 | 9 (8) | holes | 6 | 0 / 6 | 0 | 0 | 733 |  |
| cov_a_champ | 9 (8) | tournament_start | 5 | 0 / 5 | 0 | 0 | 281 |  |
| cov_a_golfer | 28 (24) | holes | 6 | 1 / 5 | 0 | 0 | 508 |  |
| cov_a_keys | 22 (18) | holes | 6 | 1 / 5 | 0 | 0 | 519 |  |
| cov_a_keys2 | 8 (6) | holes | 5 | 0 / 5 | 0 | 0 | 682 |  |
| cov_a_play | 7 (6) | hole | 4 | 1 / 3 | 0 | 0 | 266 |  |
| cov_a_quit | 6 (3) | game | 2 | 0 / 0 | 0 | 2 | 0 | 0: trace slice: no trace of this scenario: the recorder delivered no complete trace (trace_to; 1: trace slice: no trace of this scenario: the recorder delivered no complete trace (trace_to |
| cov_a_quit_title | 0 (0) | - | 0 | 0 / 0 | 0 | 0 | 0 | no capture inside the frame loop |
| cov_b_shots2 | 7 (4) | tournament_start | 6 | 1 / 5 | 0 | 0 | 6269 |  |
| cov_a_flags | 10 (8) | mood5 | 6 | 2 / 4 | 0 | 0 | 607 |  |
| cov_a_keys3 | 5 (3) | holes | 2 | 0 / 2 | 0 | 0 | 250 |  |
| cov_k1_routing | 3 (2) | trees | 1 | 0 / 1 | 0 | 0 | 55 |  |
| cov_k2_build | 7 (7) | built | 6 | 0 / 6 | 0 | 0 | 983 |  |
| cov_k2_golfers | 3 (3) | paused | 1 | 0 / 1 | 0 | 0 | 31 |  |
| cov_k2_label | 5 (5) | career | 1 | 1 / 0 | 0 | 0 | 23 |  |
| cov_k2_simfoto | 2 (2) | paused | 1 | 0 / 1 | 0 | 0 | 38 |  |
| cov_k3_build | 11 (11) | sandbox | 6 | 1 / 5 | 0 | 0 | 443 |  |
| cov_k3_camera | 20 (20) | sandbox | 4 | 0 / 4 | 0 | 0 | 201 |  |
| cov_k3_career | 1 (1) | career | 0 | 0 / 0 | 0 | 0 | 0 | no segment of >= 20 frames after the prefix |
| cov_k1_animals | 4 (3) | course | 2 | 0 / 2 | 0 | 0 | 261 |  |
| cov_k1_magazine_d2900 | 2 (2) | holes | 1 | 0 / 1 | 0 | 0 | 102 |  |
| cov_k1_paths | 3 (2) | bridge | 1 | 1 / 0 | 0 | 0 | 51 |  |
| cov_k1_river | 71 (70) | r00 | 6 | 6 / 0 | 0 | 0 | 124 |  |
| cov_k1_steps | 2 (1) | steps | 0 | 0 / 0 | 0 | 0 | 0 | no segment of >= 20 frames after the prefix |
| cov_k1_terrain | 4 (3) | raised | 2 | 1 / 1 | 0 | 0 | 69 |  |
| cov_k1_washers | 4 (3) | washers | 2 | 1 / 1 | 0 | 0 | 51 |  |
| cov_k3_camera2 | 24 (23) | sandbox | 5 | 3 / 2 | 0 | 0 | 119 |  |
| cov_k3_champ | 12 (10) | tournament_start | 2 | 1 / 1 | 0 | 0 | 138 |  |
| cov_k3_hole19 | 7 (7) | hole | 1 | 0 / 1 | 0 | 0 | 78 |  |
| cov_k1_label | 2 (2) | course | 1 | 1 / 0 | 0 | 0 | 51 |  |
| cov_l_camera | 12 (12) | edge | 2 | 1 / 1 | 0 | 0 | 55 |  |
| cov_l_point | 4 (3) | game0 | 1 | 1 / 0 | 0 | 0 | 21 |  |
| cov_l_bridge | 12 (11) | bridge | 1 | 1 / 0 | 0 | 0 | 30 |  |
| cov_b_location | 0 (0) | - | 0 | 0 / 0 | 0 | 0 | 0 | no capture inside the frame loop |

### 4.1 COMMAND, first effects

The counterfactual (the same start image with every input dropped) named the first sim object the inputs changed. Most
input segments carry the season loops' Esc presses (every 2 virtual s) or arrow / letter keys, and their first effect is
g_cachedCameraTileX 0x00543d04 one frame after the first post-restore poll (where the loop presses Esc), for example
course_season 10 / 10, the career, story and championship segments (U-0682: which key path writes it is not
determined). Cursor moves first change g_cursorTileX / Y (cov_b_holes18, golfer_stories), fixture writes (`w32`) change
g_date at record 0 (cov_b_career 1, 2). At the end checkpoints the effects include g_messageTimer, the ball screen
positions, g_cash_div100 / g_courseTable / g_date (cov_b_shots 2) and g_accuracySkillX100. No input segment left the
simulation untouched, so UI-ONLY is empty: the class remains defined for scenarios whose inputs only reach UI state.
Per-segment detail: `class_reason` in verify.json / start.json, and `log/p2/p24/cf_effects.py`.

### 4.2 Findings that extend phase 2.1's rule

1. **Prefix inside the frame loop.** A restart replays its prefix script only up to the prefix checkpoint, then waits for
   the next frame head. A prefix checkpoint on the title menu never reaches one (sandbox_basic `menu`: 200,001 clock calls,
   run FAILED). The prefix is the first checkpoint whose capture met the frame head within 250 clock calls.
2. **`data+`, not `data`.** The `data` variant (all of .data minus the dwords that differ between the three continuous
   runs) crashed every restart of cov_a_build segments 0-2 and cov_b_decor segment 0. The crashes were access violations
   in the heap free path (golf_clean.exe+0xa7a10 <- +0x76d17) and in Surface code (+0x75d7c reading 0xf7f7f837). A
   pointer-class word that holds the same heap pointer in all three continuous runs is not noise, but the restarted
   process's heap differs: its prefix is shorter than the source run. `data+` additionally keeps the restarted
   process's own value of every pointer-valued dword inside pointer-flagged objects, flag gaps and unmodeled bytes, and
   writes sim-class objects exactly as `data` does. With it, cov_b_decor segment 0 reproduced 2,685 / 2,685 frames and
   every class at the end checkpoint, and cov_a_build 6 / 6 were accepted. From then on every restart used `data+`: 304
   restarts used `data+`, 239 earlier ones `data`. Their summaries record the variant; an absent field means `data`.
3. **Input in flight.** A message the harness posted before the start frame head but that the window procedure gets
   only after it sits in the Windows message queue, which no .data image holds. Measured on cov_l_camera segment 0: the
   VK_UP WM_KEYDOWN was posted at the step0 poll (clock position 47,958) and dispatched in the H1 of the start frame
   (MSG record after the start FRAME, position 48,044). Both restarts diverged at the first frame (gCameraTileY), and 9
   sim objects differed at the end. Queue model (`queued_posts`, FIFO over DELIVER post / MSG records; a dispatched
   message removes every older entry): 2 segment starts with queued messages, 0 prefixes. Restarts now post the queued
   messages again from inside the restoring clock call, before the frame body. cov_l_camera segment 0 then reproduced
   34 / 34 frames. A prefix must have an empty queue. `start.json` carries `queued_messages`. cov_k3_camera segment 1
   (one queued WM_MOUSEMOVE) was proven before the queue model existed, without that message, and both restarts still
   matched.
4. **Negative control.** sandbox_basic segment 0 restarted with g_rng + 1 in the written image (`--corrupt g_rng`): RED at
   record 0 (the first frame after the start, g_rng only, all 51 records), end checkpoint g_rng and g_skyActors. The
   runtime test repeats it.

## 5. How the port loads and runs a segment

1. **State.** Read `start.data.bin` into a `portsim::GameState` (0x381174 bytes, layout of port/include/gamestate.h). Do
   not interpret as values the dwords of `noise.bin`, the objects whose class is `pointer` or `audio` in
   gamestate_meta.inc, the SG_POINTER_WORD dwords or the SG_GAP bytes: they hold the source process's heap pointers and
   handles. No sim-class object holds such a byte in any accepted segment (`noise.sim_bytes` = 0, 190 / 190).
2. **Load check.** sgh64 of the 203 sim objects of the loaded state (pointer words hashed as 0, re/tools/state_image.py)
   must equal `frames.bin` record 0.
3. **Clock.** The port starts AT the start frame's frame head: its frame-head clock read returns `start.frame_value`
   (= the slice header's t0 = the value of the first CLOCKRUN record). Every later clock read takes the next value of the
   slice, in call order (CLOCKRUN: ordinal, call site, value). Game time advances one date unit per unpaused frame (time
   model section 1), so the values matter for pacing reads and the RNG seed only.
4. **Input.** First `queued_messages` (they are in the queue at the start instant; the original dispatches them in the
   start frame's H1), then every DELIVER record of the slice at its clock position (`pos` = clock calls before it). The
   harness wrote MSG, HANDLER, TAKE, KEYSTATE, KBSTATE, CURSOR and MAPVK records as observations: the port's input path
   should produce the same records. The virtual cursor and key states at the start are `start.input`.
5. **Frame alignment.** `frames.bin` record k (k >= 1) is the k-th frame head after the start: the state when mainLoop's
   frame-head clock call runs (0x0040fc29; the hash is taken inside that call, before the frame body), with T = the value
   that call returns. The port compares its state at its own k-th frame head with record k: T and all 203 sim hashes.
   Records are aligned in order, not by the frame-head index since boot (that index is informative only).
6. **End.** The slice ends at the end checkpoint's POLL (final), plus the FRAME of that same clock call if one follows.
   `checkpoints.json` holds the object hashes of every class at each checkpoint inside the segment, taken inside that poll's
   clock call; the port compares the `sim` class there.
7. **Pass.** Every record equal (count, T, the 203 hashes) and the end checkpoint's sim hashes equal. A failure reports the
   first divergent record (index, T, objects whose hash differs), as frame_trace.compare_frames does. The other classes
   (hidden, ui, render, engine, const) were equal between the original's restarts and its continuous run at the end
   checkpoint in 348 of 380 accepted restarts; in the other 32 (ui_tour, ui_tour_v, cov_a_keys, cov_k1_routing) only the
   render-class g_polyFill differed. A port is not expected to reproduce those classes.

## 6. Hidden state

The start image is the whole .data section, so it carries phase 2.1's causal non-saved objects g_rng 0x00822d9c and
g_lastTipDate 0x0055e5ac and every stale buffer (H2). The segment adds what is outside .data and was measured to
matter: the clock value, T and the QPC counter at the start, the harness's pending events (a pending key release in 9
accepted segments, the next poll in all) and virtual timers (none live in any accepted segment), the virtual cursor and
key states, and the queued window messages (section 4.2). It does
not carry Terrain.dll / sound.dll / jgld.dll state or the heap (U-0607). Within the 190 segments no sim object depended
on them: every restart began from a prefix whose DLL and heap state was the prefix checkpoint's, not the start's (U-0680).

## 7. What step 2.6 (harness) and 3.8 (integrator) need

- Inputs per segment: `start.data.bin`, `noise.bin`, `start.json` (instant, `queued_messages`, `input`, `events`,
  `timers`, class), `trace.trace` (schema v1, re/model/trace_schema.md; header t0 / q0 are the slice's first clock
  values), `frames.bin` (u32 T, u32 index, u64 x 203, symbols in `frames.json` in gamestate_meta.inc order of class sim).
- Index: `port/corpus/segments.json` (all 193 rows with status, reason, files, sha256, bytes); committed copy of the
  table: `re/port/segments_manifest.tsv`.
- Start with TIME-FORWARD (35, no input at all: sandbox_basic 6, cov_k1_river 6, the end stretches of the shot and
  career scenarios), then COMMAND by first effect (camera cache, cursor tile, fixture writes, then building, shots, hires).
- Segments are short to long: 20 to 2,685 frames; course_season covers the whole 600-virtual-s season in 10
  consecutive 60-s windows (6,118 frames); cov_a_career has four 1,000-frame windows between its year_100 and year_800
  checkpoints.
- A segment whose start frame is followed by a modal stretch (modal_ms > 0 in start.json `inputs`) runs UI code
  between two frame heads (reports, hire dialog, shot results); its sim effect shows at the next frame head.

## 8. Tooling and tests

- `re/tools/segment_corpus.py`: capture / trace / plan / restart / verify / extract / manifest / report / pipeline; run
  folders `log/p2/p24/<scenario>/` (local). Pipeline: `pipeline --scenario <s> --instance <i> --cleanup`; a finished
  scenario is skipped (`pipeline_done.json`) unless `--redo`.
- Run hygiene measured in this step: the shared boot lock is skipped (SCENARIO_REFS_NO_LOCK=1; U-0684); this tool's own
  spawn lock serializes Frida spawns; a spawn watchdog kills a game still not `ready` 150 s after its spawn (matched by
  parent pid: a suspended process reports no ExecutablePath) and leaves the run when no child exists (U-0683); every step
  has a watchdog (capture / trace 2,400 s, restart 1,200 s).
- Tests: `tests/test_segment_corpus.py` (static, 13: trace slicing on synthetic traces, input summary, classification,
  frame window / comparison, segment selection, prefix rule, queue model, noise bitmap, manifests) and
  `tests/test_segment_corpus_runtime.py` (slow, about 95 s: three captures SAME as det_b, plan, two restarts of a
  sandbox_basic segment, the g_rng control RED at record 0, extract into a temporary corpus).
- Open points: U-0680 (DLL / heap state at the restart instant), U-0681 (trace_rec `call` actions), U-0682
  (g_cachedCameraTileX on keys), U-0683 (spawn hangs), U-0684 (boot lock holder).

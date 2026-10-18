# Phase 2.1 premise check: does the original restart from a state image? (2026-10-18)

Question (re/PHASE23_PLAN.md 2.1): the planned port oracle starts a segment from a state image taken at checkpoint k,
advances it with the recorded clock and inputs, and compares with checkpoint k+1. That is valid only if the ORIGINAL
game reproduces k+1 when it is restarted from an image of k. This note measures it on `golf_clean.exe` v1.03 (every hook
off, instances\p21, C3FT_AUDIO=none). NO-GUESSING: every claim below is a measured run (run folders under
`log/p2/p21/<scenario>/<label>/`, logs `log/p2/*.log`, running notes `log/p2/p21_notes.txt`).

## 1. Method

Tool: `re/frida/restart_ab.py` (commands `capture`, `restart`, `compare`, `gaps`) on top of `re/frida/scenario_ab.py`
and the opt-in IMAGE block of `re/frida/js/scenario_det.js` (`cfg.image`; absent by default).

**Restart instant = a frame boundary.** The first main-thread `timeGetTime` call, after a checkpoint poll, whose return
address is 0x0040fc2f: mainLoop's frame-head `call [0x004ba21c]` at 0x0040fc29, whose result goes to F+0x1ec at
0x0040fc2f (re/analysis/sim/mainLoop_structure.md). It recurs once per frame at the same stack depth (game esp
0x001ae9cc in every run on this PC), with nothing of the frame body live in registers. While a watch is armed every
main-thread clock call enters the agent; events still fire only at a call whose old T has reached the earliest event,
the rule of the default mode (an earlier version fired them one call early and drifted 1 ms per poll; found and fixed).

**Capture** (continuous run C): at the frame head after the listed checkpoints the agent sends golf_clean.exe's whole
.data section (0x004c1000, 0x381174 bytes), the main-thread stack from game esp - 0x40 to + 0x1400 (mainLoop's frame F =
esp + 4 .. F + 0x1250), the value the game receives from that clock call, T, Q, the pending event queue (next poll,
scheduled input), the virtual timers, the virtual cursor and key states, and the process's mapped ranges outside the
exe. Every checkpoint also dumps the whole .data section (`<name>.full.bin`), the writable pages of jgld.dll,
Terrain.dll, sound.dll and binkw32.dll (`<name>.dll.bin`) and the mapped ranges. `--save-at` also calls saveGame
0x0040b4a0 there (a separate run, since the save writes the text buffer and g_year).

**Restart** (run R, a fresh process): replays the same schedule up to checkpoint `--from` (or another scenario's, with
`--prefix-scenario`), then at the next frame head: sends its own .data and stack (`<k>.pre.bin`), receives the write set,
optionally calls loadGame 0x0040b9b0 (`--load`, name with `.sve`, a2 = 1 as mainLoop's in-frame reload at 0x0041c102),
writes the regions, hands the game the captured clock value, sets T and Q, rebuilds the event queue and the virtual
timers from the captured descriptors and applies the captured input state. The scenario script of C is then advanced
offline over C's polls before the capture instant (in replay mode a script is a function of the poll times), so R
continues with exactly C's remaining schedule; its next poll arrives at C's time.

**Write sets (variants).** Noise = every aligned dword that differs between the source run and the other continuous runs
at the capture instant (dword-granular: a heap pointer of another process often shares its low bytes, so a byte mask
splices two processes' pointers). Pointer-flagged = a model object of re/model/addresses.tsv whose element type is a
pointer / vtable or a struct with a `ptr` / `vptr` member in re/model/include/sections/*.h (195 objects, 2,863,036 bytes
of .data; 989 model objects cover 3,623,173 of the 3,674,484 bytes). Flag gaps = the dwords of non-flagged objects that
differ between continuous runs (`gaps`, re/analysis/data/premise_check_flag_gaps.tsv). Variants:

| variant | writes |
|---|---|
| none | nothing (clock, events, timers and input only) |
| sve | the .sve globals (re/tools/sve.py FIELDS and the tail blocks the image's flags select) minus noise |
| model | every non-pointer-flagged model object minus noise and flag gaps |
| data | the whole .data minus noise |
| data+ | the whole .data minus noise and, inside pointer-flagged objects, flag gaps and unmodeled bytes, every dword whose value lies in a mapped non-exe range of either process |
| full | the whole .data, raw |

`--stack` also writes mainLoop's frame minus noise words and value-in-range words; `--add` / `--drop` add or remove named
objects; `--corrupt <symbol>:<delta>` changes one object in the written image (negative control).

**Compare** (R against C at every checkpoint after the restart): T of the poll; every .data byte, minus the dword noise of
the other continuous runs and, inside pointer-flagged objects, flag gaps and unmodeled bytes only, value-in-range pointer
words of each process (plain model objects are compared strictly: string pools, grids and RNG words fall in mapped ranges
often enough that a value test would hide real differences, measured: 325 objects held such words); the checkpoint frame
(jgld back buffer); the DLL pages with the same rules. A checkpoint is SAME when all four agree.

## 2. Scenarios and controls

| scenario | continuous runs | checkpoints | virtual s | capture instants (frame head after) | restart prefix (`--from`) |
|---|---|---|---|---|---|
| sandbox_basic (pinned schedule) | c_a, c_b, c_c | 10 | 48 | sandbox_00, sandbox_20 | sandbox_00 |
| course_season (pinned schedule) | c_a, c_b, c_c | 12 | 627 | sandbox, hole_open, season_120 | hole_open; sandbox (before the hole is built) |
| p21_mood5 (this tool: mood5.sve via Continue Saved Game, Esc x4, camera (28, 15), 240 s of season, a checkpoint every 20 s) | rec (record), c_a, c_b, c_c | 15 | 265 | view, season_0080 | view; sandbox_basic's sandbox_00 (another course) |

Plus one save run per scenario (c_save: saveGame at the capture instant; `re/tools/sve.py check` OK on all three files).

- **Default mode unchanged.** A plain `scenario_refs.py run --scenario sandbox_basic` after the edits (no `cfg.image`) is
  SAME as the pinned det_b at all 10 checkpoints (`check`). The capture runs (IMAGE block active, a watch armed after the
  capture checkpoints) are also identical to det_b in state and frame: sandbox_basic 10 / 10, course_season 12 / 12.
- **(a) two continuous runs.** c_b against c_a, noise from c_c (p21_mood5: from c_c and rec): SAME at every checkpoint in
  all three scenarios (state, frame, DLL pages, T).
- **Process noise.** c_a and c_b differ in 201,591 bytes of .data at sandbox_20, in 122 objects: 116 pointer-flagged ones,
  g_rootWindow and g_graphsyObject (flag gaps, section 5) and 4 unmodeled spots (0x004e4dea, 0x00841012, 0x00842042,
  0x0084214e). A pointer of another process often shares its low bytes
  (g_rootWindow +0x11c: 0x037029d0 / 0x037129d0 / 0x037629d0 / 0x037a29d0 in four runs, and the same value in c_a and c_c
  of p21_mood5), so a byte mask from one or two runs misses parts of words: noise is taken per dword, from at least two
  other runs, and the 12 gap dwords are excused by name.
- **Same-instant restart** (`--from k --image k`, the write lands on the process's own state): SAME in all three
  scenarios (sandbox_basic 4 / 4, course_season 8 / 8, p21_mood5 8 / 8 checkpoints after).

## 3. Results

Verdicts over every checkpoint after the restart (4 in sandbox_basic, 8 in course_season, 8 in p21_mood5). "sim" = only
the objects of model streams B and C plus every .sve field (stream D is rendering / audio, E1 UI, E2 engine classes, F
constants). Bytes / objects / pixels are at the first checkpoint after the restart. Full table: `log/p2/p21_summary.json`
(regenerated by `log/p2/p21_summary.py`).

### 3.1 Restart from a state image (c)

| scenario | prefix -> image | variant | verdict | sim | first checkpoint after |
|---|---|---|---|---|---|
| sandbox_basic | sandbox_00 -> sandbox_20 | full | CRASH | - | access violation in Surface_blit3 0x00475d73 reading 0x051887a0 (the source process's heap), frame 1 |
| sandbox_basic | sandbox_00 -> sandbox_20 | data, BYTE-level noise of one or two runs (first runs) | CRASH | - | access violation in Sprite_draw 0x00474240 reading 0xc6c (drawQueuedSprite <- addSprite <- mainLoop 0x0041182f) |
| sandbox_basic | sandbox_00 -> sandbox_20 | data (dword noise of two runs) | SAME | SAME | 0 B, 0 px |
| sandbox_basic | sandbox_00 -> sandbox_20 | model (3 runs, one with the mainLoop frame) | SAME in .data, frame, T; 1 sound.dll byte in 2 of 3 runs | SAME | 0 B, 0 px |
| sandbox_basic | sandbox_00 -> sandbox_20 | data+ (2 runs, one with the mainLoop frame) | SAME in .data, frame, T; 1 sound.dll byte | SAME | 0 B, 0 px |
| course_season | hole_open -> season_120 | model | DIFF | SAME | g_sounds 50 B (3 B from season_300 on), 0 px |
| course_season | hole_open -> season_120 | data+ | SAME | SAME | 0 B, 0 px, 8 / 8 checkpoints |
| course_season | hole_open -> season_120 | data | SAME | SAME | 0 B, 0 px, 8 / 8 checkpoints |
| course_season | sandbox (no hole yet) -> season_120 | model | DIFF | SAME | g_sounds 57 B + 2 B; Terrain.dll 4,774 B; frame 49,929 px at every checkpoint |
| p21_mood5 | view -> season_0080 | model | DIFF | SAME | g_sounds 93 B, unmodeled 0x00840b24 2 B; 0 px |
| p21_mood5 | view -> season_0080 | data+ | SAME | SAME | 0 B, 0 px, 8 / 8 checkpoints |
| p21_mood5 | view -> season_0080 | data | SAME | SAME | 0 B, 0 px, 8 / 8 checkpoints |
| p21_mood5 | sandbox_basic sandbox_00 (Monterey) -> season_0080 | model | DIFF | SAME | g_sounds 94 B + 5 B; Terrain.dll 56,594 B, jgld.dll 5 B; frame 245,390 px |

- A full .data image restarts only when the per-process words keep the fresh process's values. Written raw it crashes
  on the first frame (the source's heap pointers). Minus the BYTES that differ between continuous runs it also crashed:
  a heap pointer of another process often shares its low bytes with this one's, so a byte mask writes half a pointer
  (measured: g_rootWindow +0x11c 0x037029d0 / 0x037129d0 / 0x037629d0 / 0x037a29d0 in four runs). Minus the DWORDS that
  differ in two other continuous runs, plus the 12 gap dwords (section 5), it is SAME in all three scenarios. At the
  restart instant the fresh process (sandbox_00 -> sandbox_20) then differs from the image outside that noise in 2,040
  bytes of plain objects and 4 dwords of pointer-flagged ones (g_spriteObj +0x2c / +0x20c, g_spriteSnapObj +0x2c / +0x20c:
  0x00783bbc against 0x00783c14: addresses inside golf_clean.exe's .bss, which mean the same in every process since the
  exe has no relocations).
- The model image (every non-pointer model object, minus noise and the 12 gap dwords) restarts with every simulation
  object equal for 48, 480 and 160 virtual s, frames pixel-identical when the prefix is the same course. The only
  residue is g_sounds (the plain fields of the 300 sound records 0x0080d840, pointer-flagged as a whole) and 2 unmodeled
  bytes; data+ writes those too and is SAME everywhere.
- The mainLoop frame (F) does not have to be restored at the frame head: model with and without `--stack` give the same
  result (sandbox_basic), although 7 to 127 frame words differ between the fresh process and the image there.
- Terrain.dll holds state the exe image does not: with a prefix that never built the hole (course_season from
  `sandbox`) or that loaded another course (p21_mood5 from sandbox_basic), every exe object still evolves identically,
  but Terrain.dll's writable pages and the frame differ at every checkpoint. Within these horizons no simulation object
  depended on it; the frames do.

### 3.2 Restart from a save (b)

The save is the game's own: saveGame 0x0040b4a0 called at the capture instant of a separate continuous run (c_save; its
.sve passes `sve.py check`). The fresh process calls loadGame 0x0040b9b0 (name with `.sve`, a2 = 1, a3 = 0, a4 = 0: the
in-frame reload of mainLoop 0x0041c102) at the restart instant, then takes the clock, events and input as in (c), so the
two runs are compared at the same virtual times. `sve` is the same set of globals written as an image, without loadGame.

| scenario | variant | verdict | sim | objects / sim objects at the first checkpoint after | frame px | DLL pages |
|---|---|---|---|---|---|---|
| sandbox_basic | none (clock only) | DIFF | DIFF | 29 / 11 | 2,095 | - |
| sandbox_basic | sve image | DIFF | DIFF | 25 / 8 | 917 | sound.dll 1 B |
| sandbox_basic | loadGame | DIFF | DIFF | 38 / 11 | 267,727 | Terrain.dll 23,686 B, jgld.dll 4 B |
| course_season | none | DIFF | DIFF | 89 / 56 | 9,151 | - |
| course_season | sve image | DIFF | DIFF | 79 / 47 | 9,204 | - |
| course_season | loadGame | DIFF | DIFF | 80 / 48 | 184,710 | Terrain.dll 20,297 B |
| p21_mood5 | none | DIFF | DIFF | 89 / 54 | 24,482 | jgld.dll 7 B |
| p21_mood5 | sve image | DIFF | DIFF | 66 / 33 | 14,760 | - |
| p21_mood5 | loadGame | DIFF | DIFF | 66 / 33 | 129,959 | Terrain.dll 2 B |

- A save does not restart the simulation in any of the three scenarios. The divergence starts in state the save does
  not hold (section 4) and spreads into saved objects: g_storyPairs (0x0059fc60), g_golfers, g_walkers, g_holes,
  g_skyActors, the ratings and finance rows all differ at k+1 in course_season and p21_mood5.
- loadGame itself changes state beyond what the sve image writes (objects that differ after loadGame and not after the sve
  image, at any checkpoint): sandbox_basic g_cornerCache, g_tileWallMask, g_tileScreenY (the derived terrain tables it
  rebuilds through rebuildHeightfield 0x0042f7a0 / rebuildTileCoverage 0x0042f340 / markTileKind11 0x0042f2c0 from the
  loaded fields, 0x0040bb19..0x0040bb23), g_panelListMode (0 at 0x0040bafe), g_saveIoRead (left 1), g_textBuffer,
  g_value587ea0, g_ui_5aa554, g_ui_5aa6b8; course_season g_flags (loadGame clears bits 2-3 of the loaded flags except the
  entry's bit 2 and sets 0x40000000, 0x0040bae7..0x0040bb08). Its terrain refresh (applyCourseType 0x00449400,
  refreshTerrain 0x00449520) rewrites Terrain.dll's pages and the frame: the Build Course palette is open and the
  shoreline cliffs are drawn differently after the load in sandbox_basic (log/p2/p21/sandbox_basic/r_load_00_20/
  sandbox_25.png against c_a/sandbox_25.png).

### 3.3 Leave-out runs (which non-saved objects the simulation needs)

All from the `sve` image plus named objects (`--add`), or from `model` minus named objects (`--drop`); "self" = only the
left-out object itself differs afterwards (its stale bytes stay until the game rewrites them), "none" = nothing differs.

| run | scenario | written | result (simulation objects) |
|---|---|---|---|
| r_sveplus_00_20 | sandbox_basic | sve + g_rng, g_pathQueueX/Y, g_pathGrid | SAME (g_panelState 2 B left) |
| r_model_norng_00_20 | sandbox_basic | model - g_rng | DIFF: 8 sim objects (24 objects), 917 px |
| r_model_noqx_00_20 | sandbox_basic | model - g_pathQueueX | self (660 B at every checkpoint) |
| r_model_nogrid_00_20 | sandbox_basic | model - g_pathGrid | none (SAME) |
| r_svesim_* | course_season, p21_mood5 | sve + the 32 non-saved B / C candidates | SAME; left: g_sounds, UI words, story text |
| r_lgo_A..E | course_season, p21_mood5 | the 32 minus one group | only group C is causal (course_season: 35 sim objects at season_180); A, B, D, E self or none |
| r_loo_<x> | course_season | the 32 minus one of group C | g_lastTipDate causal (35); g_thoughtArgByKind self; g_shotStartDate, g_commentTone, g_storyReplyChapter none |
| r_sverng_* | all three | sve + g_rng | sandbox_basic self only (path queues); course_season DIFF 36 sim objects; p21_mood5 self only |
| r_svemin_* | all three | sve + g_rng + g_lastTipDate | no saved object differs at any checkpoint; frames 0 px; left: 13 stale buffers (below), UI, audio, story text |
| r_loadmin_hole_120 | course_season | loadGame + g_rng + g_lastTipDate | sim objects: only g_flags (loadGame's own change) and stale buffers; frame 177,826 px, Terrain.dll 20,297 B |

## 4. Hidden state

Full list with one row per object: `re/analysis/data/premise_check_hidden_state.tsv` (123 model objects: every object that
differs at a checkpoint after a restart from the save in any scenario, plus the candidates; columns: va, size, stream,
saved, class, where it differed after loadGame, whether it still differs after save + g_rng + g_lastTipDate, leave-out
result). Generated by `log/p2/p21_hidden_table.py` from the run folders.

| class | objects | what |
|---|---|---|
| H1 causal, not in the save | 2 | **g_rng** 0x00822d9c (the Random state, Random::next 0x0045c1a3) and **g_lastTipDate** 0x0055e5ac (emitGolferThought stores g_date after a tip, 0x0046980d..0x00469843; a tip needs g_date - v > 0x1f4). Without them the simulation diverges in every scenario that has golfers |
| H0 saved, diverges as a consequence | 40 | g_golfers, g_storyPairs, g_walkers, g_holes, g_skyActors, the ratings, finance and history rows, g_date ...: equal again once H1 is restored |
| H2 not saved, rewritten before use | 20 | equal at every later checkpoint once H1 is restored: unk_005a6d40 / g_messageTimer / unk_005a9ccc (message text and timer), g_shotTargetX / Y, unk_005a9cc8 / ce4 / ce8, g_shotFlags5a7140, g_ballTrailX, g_ballScreenXs / Ys, g_golferStripSlot, g_nameLabelCount, g_textBuf, g_numberTextBuf, g_pathGrid, g_commentTone, g_storyReplyChapter, unk_0059b738 |
| H2 not saved, stale | 13 | still differ at later checkpoints but feed nothing measured: g_pathQueueX / Y (BFS queues, 660 B), the shotPower memo cache g_shotPowerCache / Arg2 / Dist / RingIndex (10 entries keyed by the two arguments, 0x00422430), g_thoughtArgByKind, g_shotStartDate, g_infoPanelNumBuf (an __itoa buffer), g_ballTrailY, tile_anim_frame, g_value587ea0, g_saveIoRead (the save direction flag) |
| H3 UI | 11 | g_panelState, g_ui_* words, g_rootInput, g_panelListMode, g_rootWindow |
| H4 rendering | 25 | the sprite queue arrays g_sprite* / g_spriteSnap* (0x00824368..0x0083620c), g_sceneCount838204, g_flyBall4e9a9c, g_shot56a78c / 790, g_linePoint5a412c: rebuilt by the frame, equal once H1 is restored except g_linePoint5a412c (3 B) |
| H5 text | 4 | g_textBuffer, g_statusText, g_storyFiles / g_story838220 (story records loaded by loadGolferRoster 0x00465b0c / loadStoryStrings on demand) |
| H6 audio | 1 | g_sounds (the plain fields of the 300 Snd records at 0x0080d840; the model flags the array as pointer-holding, so `model` does not write it) |
| H7 load path | 5 | g_cornerCache, g_tileWallMask, g_tileScreenY (derived tables loadGame rebuilds, different from the course's own), g_flags (loadGame's bit edit), f_w_4c2e00 (0x004c2e08 = -1 at 0x0040bba3) |
| H8 unmodeled | 2 | 0x00840b24 (2 bytes, p21_mood5) |

Outside golf_clean.exe's .data (not part of any image here): Terrain.dll's writable pages (the course's terrain: 4,774 to
56,594 bytes differ when the fresh process had another course or no hole; rendering differs, the simulation objects
did not within 160-480 virtual s), sound.dll's data (1 byte in some runs), jgld.dll (4-7 bytes; single bytes vary from
restart to restart, e.g. Terrain.dll RVA 0x110778 and jgld.dll RVA 0x12879c in the runtime test's restart), the heaps those
point into, and mainLoop's frame F (7 to 127 words differ at the frame head; writing them changes nothing measured).

## 5. Pointer-holding objects and model flag gaps

- 195 model objects (2,863,036 bytes) are pointer-flagged; 116 of them carried process-specific words in these runs.
- 12 dwords in 5 objects the model types as plain data hold heap pointers (values 0x0370xxxx..0x03e4xxxx in every run,
  different between runs): g_rootWindow (SgRootWindow) +0x11c, +0x120, +0x124, +0x128, +0x14c, +0x278, +0x378, +0x518;
  g_listPalette 0x00839a98; g_msgBoxLabelA 0x0083b994; g_msgBoxLabelB 0x0083b998; g_graphsyObject 0x0084092c
  (`premise_check_flag_gaps.tsv`, `restart_ab.py gaps`). The first model restart crashed on them: an access violation in
  jgld+0x8fb9 reading 0x4fc from Surface_blit3 0x00475d00 <- updateTerrainRender 0x00449abd (the blit of g_rootWindow's
  Surface at 0x00519cd4). For phase 2.2: flag these fields as pointers.
- A value test (a dword whose value lies in a mapped range outside the exe) is not a pointer flag: string pools, grids
  and RNG words pass it (325 objects, e.g. g_cornerCache 4,812 B, g_rng); the first model+ / data+ runs that dropped such
  words lost real state. It is used only inside pointer-flagged objects and unmodeled bytes.

## 6. Negative controls

| run | corrupted in the written image | result |
|---|---|---|
| sandbox_basic r_neg_date_00_20 | g_date 0xe6 -> 0xe7 (model image) | RED at all 4 checkpoints, g_date among the differing objects at each, plus 15-28 others and 2,044-3,256 px |
| course_season r_neg_rng_hole_120 | g_rng + 1 (model image) | RED at all 8 checkpoints, 39-50 simulation objects |
| p21_mood5 r_neg_rng_view_80 | g_rng + 1 (model image) | RED at all 8 checkpoints, 26 simulation objects at season_0100 |
| course_season r_neg_cash_hole_120 | g_cash_div100 + 1 (model image) | the corruption is gone by season_180: the run equals r_model_hole_120 there (only g_sounds), so this control cannot read RED at a 60 s checkpoint [U-0605] |

`tests/test_restart_ab_runtime.py` (marker `slow`) repeats the model restart and the g_date control on sandbox_basic from
scratch (three captures, two restarts).

## 7. Verdicts, the rule for a segment start, the fallback

**Premise: holds, with conditions.** On the original, a fresh process restarted at a frame boundary from a state image of
checkpoint k reproduces every later checkpoint (all of .data, frame, DLL pages, T) when the image carries all of
golf_clean.exe's .data except the per-process words; it reproduces every simulation object when the image carries the
non-pointer model objects. A save does not: it lacks two causal objects.

| scenario | (a) two continuous runs | (b) restart from the save | (c) restart from the image (`data` / `data+`) | (c) model objects only |
|---|---|---|---|---|
| sandbox_basic | SAME | DIFF (11 sim objects; loadGame also rebuilds derived terrain tables and opens the build palette) | SAME | SAME |
| course_season | SAME | DIFF (48 sim objects) | SAME | sim SAME; g_sounds differs |
| p21_mood5 (golfers, stories) | SAME | DIFF (33 sim objects) | SAME | sim SAME; g_sounds and 2 unmodeled bytes differ |

**Rule for a valid segment start** (what the measured SAME runs had in common):
1. The instant is mainLoop's frame head, the `timeGetTime` call at 0x0040fc29 (return address 0x0040fc2f), in the frame
   state (a course is running). The title menu, modal screens and the session dispatch were not measured; the fresh
   process's watch never meets the frame head there.
2. The fresh process reaches the frame state of the same session kind first (here: the same scenario's earlier
   checkpoint, or another course for the simulation-only result). For pixel-identical frames it must have the same
   course geometry in Terrain.dll (same course, holes already built): Terrain.dll's state is not in the image.
3. Written: every .data dword except those that differ between at least two other continuous runs at that instant
   and the 12 flag-gap dwords (section 5); the pointer-holding words keep the fresh process's own values. For the
   simulation alone, the non-pointer model objects suffice.
4. Handed over with the image: the value the frame-head clock call returns, the virtual clock T and the QPC counter,
   the pending events (the next poll and any scheduled input), the virtual timers (none were live in these runs) and
   the virtual cursor / key states. Every capture instant had only the next poll pending (no input in flight).
5. mainLoop's frame F need not be written at the frame head (measured in sandbox_basic, both ways SAME).

**For the oracle (phases 2.2-2.4).** Start segments from frame-head images of a recorded continuous run, written as
the `data` variant (or the model objects plus g_sounds' plain fields for simulation-only comparisons), never from a
save. If only a save is available, the simulation needs at least the save plus g_rng and g_lastTipDate; with those, no
saved or causal object diverged in 48-480 virtual s, but 13 stale buffers (H2), UI words, g_sounds and story text keep
differing, so a byte-level checkpoint comparison must exclude them or restore them too. Fallback if a segment cannot
start from an image (a screen that is not the frame, a missing capture): start at the title menu and replay the whole
recorded schedule, which the pinned references already reproduce.

## 8. Tooling (opt-in; the default is unchanged)

- `re/frida/js/scenario_det.js`: the IMAGE block runs only with `cfg.image` (the stub's extra `mov [D+0x48], esp`, the
  forced onDue of an armed watch, the site check, the input note, the `img` poll reply). The event queue entries carry a
  descriptor (`desc`) that only that block reads. Proof: the plain replay of sandbox_basic is SAME as the pinned det_b.
- `re/frida/restart_ab.py`: `capture`, `restart`, `compare`, `gaps`; outputs in `log/p2/p21/` (not committed: dumps).
- `tests/test_restart_ab.py` (static: gating, helpers, variant masks, model pointer flags) and
  `tests/test_restart_ab_runtime.py` (slow: model restart SAME, g_date control RED).
- Not done: a restart at frame boundaries other than the frame head; Terrain.dll / sound.dll / heap state in the image.


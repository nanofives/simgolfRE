# Port pivot and phases 1-5 (2026-10-15 .. 2026-10-22)

Moved verbatim from CLAUDE.md on 2026-11-04: port pivot, batches b1-b6 / c3fy*, phase 1 data model, phase 2 corpora and harness, phase 3 sim modules, phase 4 boot / saves, phase 5 gfx steps 1-2.

PORT PIVOT (2026-10-15, plan re/PORT_PLAN.md): the user's target is a portable reimplementation that plays from the user's own game
files and loads or patches none of the four binaries. The C3/C4 hooks become the specification and the test oracle (re/match sources are
VC6-exact transcriptions with literal addresses: reading reference, not code to copy); C4 stops being the gate. The plan holds the module
table, seven phases (data model first) and a keep-or-fix list for original defects. Surveys: `re/tools/port_survey_globals.py`
(log/port_globals.tsv): the 1,537 game-logic functions (0x401000-0x4a4ecf) touch 5,302 data addresses in 955 clusters (gap <= 0x40), 134
named exactly, 891 clusters (500 functions) unnamed, the largest initialised-data tables (0x4c80d0-0x4cd3cc 848 addresses), the most-shared
mutable cluster 0x83aa78-0x83ac30 (82 addresses, 99 functions); immediates in the data range count, so these are upper bounds.
`re/tools/port_survey_hooks.py` (log/port_hooks.tsv): of 1,577 exe SG_HOOKs by their own detour body, pure 313, globals only 177, original
callees 568, both 507, naked 12, so 80% do not port as written (111 sources use literal addresses, 9,660 occurrences).
Batches after round 36 (2026-10-15/17, notes log/c3/<batch>_notes.txt): c3fv 5 (gameMain 0x45baf0, loadWorldAssets 0x43dbe0, loadInterfaceArt
0x442180 [24 KB, 819 call sites as 48 generated tables, log/c3/alt/c3fv_segments.py], loadInfoScreenArt 0x44bde0, drawWorldObjects 0x430360;
c3fv.js REC, an ordered TID-gated recorder with framemask's frame mapping and 64 stubs per key; gameMain also SAME at 10 checkpoints of the
sandbox_basic spawn-reach scenario), c3fw 4 (loadGameBrowserScreen 0x43b610, showThemePackSelect 0x4725b0, pickAProScreen 0x43a8c0,
courseActionMenu 0x46f550: round 32's SEH blocker ended by running ??_L 0x4a55d2 / ??_M 0x4a5713 for real as framemask stubs with
`forward: true` and the Surface array declared as a {base, stride} region), c3fx 1 (showCharacterEditor 0x4385d0: the hold reason ??_L was
wrong, the body holds one Image under SEH; the obstacle was 41 stood-in callees, solved with REC), c3fz + c3fza 5 (EditBox::onKey,
__ArrayUnwind exe 0x4a5793 / sound 0x1004288e, sound dynInit_soundWindowObj100b49b8 and SoundWindowObj_ctor; __ArrayUnwind is a plain
stdcall with its own SEH frame, run through an exported four-argument harness) promoted: no C2 row is open. Counts (`status --summary`,
2026-10-17): C4 2,765 (exe 1,536, jgld 535, sound 545, Terrain 149), C3 168 (exe 106, sound 62), C2 386 (113 held, 273 end states dead /
trivial), C1 1; exe function list 2,497.
Golfer record (g1, 2026-10-16): g_golfers starts at 0x005794b8, 156 records of 0x100 bytes (0x2700 dwords: cleared at 0x0043160b..0x00431615,
saved at 0x0040b107, whole-record `rep movsd` at 0x0045de3e / 0x004226ec); 0x005794c0 is field +0x08 of record 0 and the stride-0x80 array at
0x57956e does not exist (field +0xb6). Records 152 scratch, 153 preview, 154 synthetic, 155 trialShot's copy. re/analysis/data/golfer_record.md
(field map, flag bits, C struct) and record_tables.md (walkers/employees 0x585850 64 x 0x4c, per-class stats 0x56ae70, per-type state
0x5849e0, story definitions 0x53a454); `re/tools/record_fields.py --base --stride --count` also scans code outside every function range (444 of
3,120 references). golf_course.cpp, c3fa, c3fy and c3fx_deps.tsv use base 0x5794c0 (offsets 8 lower). Open ranges U-0009..U-0017.
mainLoop 0x0040f5c0 (c3fy family, 2026-10-16/17): one body 0x0040f5c0..0x00421615 = 73,813 B, 21,541 instructions, 27 switch tables
0x00421618..0x00421b23, one `ret` 0x0040fb04 (the Ghidra list had 8,035 B in 2 ranges); called once per process, no arguments, returns only
on QUIT. Spec re/analysis/sim/mainLoop_structure.md + log/c3/c3fy_deps.tsv; reimplementation shim/src/re/c3fy.cpp (a naked entry rebuilds the
SEH frame). The C2 -> C3 gate needs a path-1 A/B and none can exist for a once-per-process function, so it stays C2 (re/c3_hold.tsv NO_PATH1)
and c3fy.cpp is NOT in shim/re_batches.txt; the evidence is whole-game scenario A/B (mainLoop ON vs OFF, other hooks off) on state, RNG words
and frames. U-0008 resolved (c3fy2): eight transcription defects, not the original (a register reloaded between a `cmp` and its `jge` /
`setle`, 0x00413a59, 0x004156b7; `lea` between `test` and `jne`, 0x00414a88; 0x00414e91; a missing load before a switch, 0x0041e38f; strcpy
written as strcat, 0x0041cabc, 0x0041ec50). U-0007 open: `[0x004c2848] == 0x280` enters the exit-less loop 0x00420d83..0x00420e08 at
0x0041b209. Block coverage of the original: 54.0% (nine c3fy2 scenarios), 82.3% (2,103 of 2,554) with c3fya's cov_a_* and c3fyb's cov_b_*
(re/tools/scenario.py). c3fyc: five audits (re/tools/c3fy_audit_*.py) and 190 mutation modes (c3fy_mutgen.py, c3fy_mutants.tsv); the rounds
fixed more transcription sites (c3fya 3, c3fyc 7, f19530e7 14, 7e85f800 two P7 sites 0x0041a47a / 0x0041ac89). Kill round c3fyk1-3
(cov_k1_* / k2_* / k3_*) over the 45 surviving mutants: 22 killed (RED twice at the same checkpoint and hash, real hook GREEN twice), 16
equivalent (argued from the code), 1 dead (129), 6 open (113 a playSound id; 187 a tile kind no stamp writes; 272; 279 a closed handle number;
117, 123 a camera clamp read only by a money label no frame shows); verdicts log/c3/c3fyk{1,2,3}_results.tsv. TOOLING LESSONS: (1) a stored det_b
from another instance or build, or a boot overlapping a sibling's, differs at the first checkpoint in boot words (title ticker 0x00822c78, RNG
seed 0x00822d9c, seed_time 0x0051b380 = clock calls x 37): regress against a fresh OFF arm on the same instance; scenario_ab.py makes
winmm.dll.c3ft_main once and restores it after EVERY control run, so a stale backup reinstalls an old build (two c3fyk3 runs voided). (2)
Defender quarantined rebuilt mutant DLLs in instances\ (DefenseEvasion.A!ml) and Frida injection failed: rebuild and re-run, do not alter
code to dodge it. (3) A killed `--negctl` run leaves the control DLL and a stale boot.lock: compare winmm.dll with the backup, restore,
delete the lock. (4) TaskStop kills the shell, not its Python runners. (5) A single DIFF under load (cov_b_career, 384 px, state SAME) did not
reproduce: count a kill only when a second run stops at the same checkpoint with the same hash.
Batch b3 (2026-10-17): tool fixes with tests in tests/test_tool_fixes_b3.py: asm2inline.py keeps `movsx` operands and reads every range of
a multi-range function (loadInterfaceArt reaches its `ret` 0x004480f7, not 0x00447f79); match.normalize() compares branch offsets as signed
32-bit (strchr's `+0xfffffff0` against `+0x-10`; five match files re-run unchanged at 100%); c3_verify.py prints `hook source:` with the
reason when a row's hooks.csv `file` names a source without the SG_HOOK. Names (re/names/exe_17.tsv, systems exe_17.md, pending ApplyNames
list re/analysis/data/names_pending_apply.md; hooks.csv follows through `retag`): pickGolferName -> appendWalkerName, golferScore -> golferAge
(printed after `, age ` at 0x0045c7e0..0x0045c81e), updatePairSnapshot -> appendPairTrackPoint, allocTextBuffer -> drawTextRun,
ListBox::onNavKey* -> onPress* (a click handler), codecCombFilter -> codecRing5Add, new fillSlots54 0x004a29a0, four legacy hook names
(OptionsMenuBuild, CareerRetirementFlow, GolferPanelEject, LandmarkAvailableNotice) retagged to the names rows, and g_screenWidth 0x00822c8c /
g_screenHeight 0x00822c90 (gameMain 0x0045baf6 / 0x0045bb00, set-mode slot +0x58 at 0x0045bb2a..0x0045bb42; they were g_zoomLevel).
Batch b1 (2026-10-17, function boundary repair of the Ghidra master, backup ghidra/backup/SimGolf.rep.pre_b1_20261017, write-up
re/analysis/data/function_boundaries.md, per-region rows log/b1/gaps_<module>.tsv): the function lists missed code in every module. Of the
exe's bytes outside the old ranges, 66,856 were listed functions Ghidra had cut short (mainLoop alone is one body 0x0040f5c0..0x00421615,
73,813 B, with a bogus pointer at 0x41ea32 cleared), 28,837 were 437 separate functions reached only through .rdata vtables or stored
pointers (largest 0x48b3f0, 3,384 B, ListBox), 9,095 jump / byte tables and 3,961 junk `call`s in the padding (716 calls after a `ret`
into the stub 0x404a14 that jumps outside the image; origin not established, U-0018). New `ghidra/scripts/RepairBodies.java` sets
explicit bodies (so a function created early cannot absorb its targets), adds switch references and jump-table overrides and verifies
every body: 43 exe switch sites got references (38 table overrides), functions created exe 437, jgld 62, Terrain 56, sound 463 (+1 called
thunk 0x10001c62) with neutral `FUN_` names, 24 false functions removed (11 each in jgld / Terrain: pieces split off the debug CRT heap
routines; 2 in sound). Function counts: exe 2,497 -> 2,934, jgld 1,570 -> 1,621, Terrain 1,050 -> 1,095, sound 1,753 -> 2,215; no reachable
code is outside a function. The refresh recipe (c2_note --refresh + retag) changed callers / callees on 158 hooks.csv rows (mainLoop's
callees 55 -> 216) and no C3 / C4 row lost a caller; no new hooks.csv rows (rows come from names files). Re-run survey: 1,812 game-logic
functions, 5,589 data addresses, 960 clusters, 893 unnamed (584 functions touch one), golfer references outside every function 444 -> 0.
Thunks referenced only from vtable data (jgld 247, sound 550) were not created: pointer_callers.py resolves them. The b3 names (pickGolferName
-> appendWalkerName ... ) were applied to the master afterwards: ApplyNames overwrite renamed 9 exe rows and codecRing5Add, the exe and sound
function lists were re-exported. Lesson: a command that reverts a directory (`git checkout -- re/analysis`) also reverts siblings' uncommitted edits.
Batch b2 (2026-10-17, reproducible scenario baseline, re/analysis/data/scenario_baseline.md, manifest scenario_refs.json, tool
re/tools/scenario_refs.py): the boot desync of the stored references was the PC's AUDIO DEVICE, not concurrency or hooks. LIVING-PC's only
output device is the TV's HDMI endpoint; with a device sound.dll's timeGetTime at 0x10010dc3 (DsBuffer::play) runs on the main thread during
initSound just before seedFromTime 0x00467110, one extra clock call: seed_time 0x0051b380 = 0x00100001, ticker 0x00822c78 = 0xbb71, RNG seed
0x1d467741 (and location prices, which come from the RNG); without one 0x00100000 / 0xb81f. All 909 stored runs map one-to-one. scenario_det.js
now gives sound.dll and binkw32 the device-less answers by default (`C3FT_AUDIO=none`; `live` reproduces the old timeline) and scenario_ab.py
reports an A/B whose arms ran different audio paths as INCOMPARABLE. Measured: 20/20 OFF and 20/20 ON boots identical in clock-call count
(21,265) and state hash, also with two games overlapping; the c3fy flakes cov_b_inject2, cov_a_build3, cov_a_career and cov_b_career
matched their reference 10/10 OFF and 10/10 ON (cov_b_career's old script sat on the location screen, whose two pixel variants of 384 and
5,830 px occur in OFF and ON alike). 55 scenarios are pinned (det_a == det_b == det_c on winmm.dll 381ed2b2..., scenario_det.js 93e128bb...):
regenerate with `re/tools/scenario_refs.py regen`; the 35 sound-era references under log/c3/alt/c3fy2|a|b|c are stale (use
log/c3/alt/refs). cov_b_career now buys San Diego ($70,000) and cov_a_career Monterey ($50,000): the old clicks were unaffordable on the
device-less timeline. Leads: cov_b_shots / cov_b_shots2 take 5-7 shots and idle (results screen not recognised); a jgld.dll data dump for
the location-screen variants.
Batch b5 (2026-10-17, the two scenario leads b2 left; re/analysis/data/scenario_baseline.md sections 11-12, tests
tests/test_scenario_b5.py): cov_b_shots / cov_b_shots2 stopped after 7 / 5 shots because Gary's post-hole skill panel
(19.9 from tests/golden/shot_panel.png) passed the old turn test (25). The script ran a whole shot sequence, and its own
input closed the TOURNAMENT RESULTS panel that came up meanwhile. Turns themselves measure 0.0 to 18.4 (the ovals change
with lie and club), so `_cov_b_champ_rounds` also tests tests/golden/gary_skill_panel.png (8; every turn is 19.9 or
more from it). It checks for the results before every input group and before every Esc, starts a new championship
round after each results screen, runs the turn's input once per round on the post-hole panel on purpose (that reaches
H1 key cases and P7 tool previews outside a turn), and stops at 24 shots: cov_b_shots 3 rounds, cov_b_shots2 4.
Coverage of the original mainLoop against the b2 references: +86 blocks (H2 51), none lost; the union stays 2,103 /
2,554. The location-screen variants of the pre-b2 cov_b_career (`log/c3/b2_oldcareer.py`) have two causes:
(1) courseActionMenu 0x0046f550 reads its blink counter (entry esp - 0x4cc), dwell counter and previous hot button
uninitialised. The stale blink word differed in every boot, and its bit 3 decided the 384 px variant in 8 of 8 boots.
scenario_det.js STACK SEED (default) puts a stub at the thunk 0x00470a00, the only way in, that zero-fills 16 KB below
esp (`C3FT_STACK_SEED=probe` records the words natively, `0` disables). (2) The window, through the
DWM8And16BitMitigation layer, lagged the game's unchanged back buffer for 90 virtual s while the game presented the
full rect every frame. scenario_ab.py now takes checkpoint frames from jgld's back buffer (`checkpoint_frame`:
RGB555, `(v << 3) | (v >> 2)`, rows top-down although GetObjectA says biHeight +600), which equals the window capture in
every pixel when the window is current, so references stay valid. Window differences are recorded as `window_px_differ`
plus `<name>.window.png`; `C3FT_FRAME=window` restores the old capture. Result: cov_b_career_phx 20/20 OFF and 20/20
mainLoop-ON identical at all 6 checkpoints. New regression scenario `cov_b_location`. Never probe a stack slot with a
Frida Interceptor on the same thread: its callback runs below esp and overwrites the slot (the first measurement read
Frida's own words).
Batch c3fyl (2026-10-17, the six mainLoop mutants c3fyk left open; notes log/c3/c3fyl_notes.txt, tests/test_scenario_oracle_ext.py): 113, 117 and
187 killed, 272 and 279 equivalent, 123 not settled; no transcription defect, c3fy.cpp untouched. Tally of the 45 DATA / COV survivors: 25 killed,
18 equivalent, 1 dead, 1 open (123: the camera y is bounded at 0x34 by every static writer except those that copy a golfer, walker or ball
position, and a golfer's turn at 0x0042b39f is not checked against the map edge). 113 is killed only by the new OPT-IN extended oracle in
scenario_ab.py (`--call-log va:n[:ra/ra][:sK][:r][:xK]` logs main-thread calls with arguments and return value, `--extra-regions va:n` dumps more
memory per checkpoint, `extcmp` compares two finished runs; default behaviour byte-identical, the extended result is reported next to a verdict
that still comes from the normal comparison): playSound received 0x27 instead of 0x26. 187 is reached through the Scenic Bridge button, which
sets the tile variant to Random::range(8) at 0x00434c57 (the popup items only give 0, 2, 4, 6), then painting fairway on the bridge tile (it keeps
the variant bits and adds 0x4000) and a 2x2 building that covers the tile without being anchored on it; 117 needs a course with money labels (a
sandbox draws none, 0x0040c926). New scenarios cov_l_camera, cov_l_point, cov_l_bridge.
Batch b4 (2026-10-17, U-0018 narrowed to `partial`, re/analysis/data/safedisc_padding.md): the 716 `call` slots in the exe's padding are SafeDisc's
own code-cave instrumentation, written before it encrypted `.text`; Safedisc2Cleaner did not create them and nothing in the project reaches them,
so the port ignores them. original\golf.exe's `.text` is encrypted (2,682 of 754,731 bytes equal the cleaned file); from 0x00425990 on each byte is
the plaintext XOR a table of the previous four plaintext bytes (99.90% agreement), and 566 of 619 slots decrypt byte for byte to the cleaned file's
slots. golf_v100_clean.exe has 183 `call [IAT]` slots after a `ret` (caves filled with a copy of the function's last bytes, no `call rel32`, no
dispatcher); the slots are redrawn per build (49 share a function end between v1.00 and v1.03, none the same import). One slot per cave of 5-13 bytes
after a `ret`: 5-9 bytes get `call rel32`, 9-13 `call [IAT]`; the dispatcher is three pieces (0x00405099 -> 0x00404a14 -> `jmp [0x00403f9c]`, a dword
holding 0x008a2590 inside SecServ.dll). The DLLs carry none. Open: the 97 slots below 0x00425990 use a cipher not recovered, and what changed 35
IAT operands after encryption (SafeDisc at load time or the cleaner); a memory image of the wrapped exe after decryption would settle both.
Batch b6 (2026-10-17, sound.dll speech codec 0x1003a900..0x10041a00, re/names/sound_18.tsv, sound_18_globals.tsv, systems sound_18.md, U-0019): of 42
functions 23 renamed from their instructions (codecProcessBlock -> codecDecodeFrame, codecProcessResource -> codecEncodeFrame, codecStageDispatch ->
codecAnalyzeFrame [a fixed 14-stage pipeline], codecGainSmoother -> codecPitchTrackDp, codecFeatureDecision -> codecVoicingDecision, codecCombFilter ->
codecRing5Add, applyIir2 -> codecSynthPostFilter, floatFloorToInt -> floatRoundHalfAway, ...), 2 kept with corrected purposes (bitPermute, codecAmdfPitch),
17 confirmed; g_codecPermTable was one dword before the real slot table (now g_codecFrameBitSlots 0x10064480, 53 entries; g_codecCoeffSignMask 0x10064458,
g_codecErrProtect 0x100b58a8, g_codecFrameLen 0x100b58a4). Applied to the master (ApplyNames overwrite renamed 23, `ghidra/scripts/DeleteLabels.java`
removed the misplaced labels 0x1006447c and 0x10064454, backup ghidra/backup/SimGolf.rep.pre_b6names_20261017). The reference LPC-10 source was not
opened: structural matches rest on the m36c notes. Old spellings stay in registry keys, fixtures and SG_HOOK name strings (rows are looked up by address).
PHASE 1, THE DATA MODEL (2026-10-17/18, plan re/PHASE1_PLAN.md, shared brief re/model/STREAM_BRIEF.md, decisions re/model/DECISIONS.md; streams on
claude3 [A] and claude2 [B-G]): constants stay in the user's own exe (the model records DESCRIPTORS: name, element type, count, stride, file offset;
no table bytes or strings are copied), the header is clean C99 with static_assert sizes and no original addresses, the address map is separate.
Stream A (re/tools/model_scan.py, model_inventory.py, model_check.py; re/model/inventory_*.tsv, ownership.tsv, vtables.tsv; re/analysis/data/
model_inventory.md): 30,102 references to 5,949 data addresses in 1,767 clusters (the old survey's 5,589 skipped base-register displacements and
call/jmp memory operands), classifier error rate 7.5% on known types, element COUNTS weak (10 of 16); model_check verifies a proposed type against
every access of every function. Streams (each re/model/include/sections/<s>.h, re/model/<s>_addresses.tsv, re/names/model_<s>_globals.tsv,
re/analysis/data/model_<s>.md, re/model/<s>_uncertain.tsv): B simulation state (165 ranges, 122 names), C course/terrain/world (50 ranges, 35 names;
Terrain.dll boundary structs), D rendering and audio state (109 ranges, 62 names; 300 sound objects of 0x6c at 0x80d840, 190 image slots at 0x81ca10,
sprite queue arrays), E1 UI toolkit class layouts (209 rows), E2 engine class layouts and the DLL API tables (29 structs, 153 globals, 130 names,
e2_dll_api.tsv), F constants and configuration (366 clusters as 374 descriptor objects, f_strings.tsv indexes 3,114 strings without text). G2 (re/model/
sve_fields.tsv 91 rows, re/model/model_ctypes.py generated by re/tools/model_ctypes_gen.py, re/tools/model_sve_fields.py): 116 saves round-trip
byte-identical through the field table and mirror (3 fixtures, game-written checkpoint saves, autosaves), 63 of 69 unnamed save fields named by the streams
and the last six named by G1 (fields 25 g_courseReportScore, 29 g_buildingUnlockLevel, 46 g_proChallenger, 76-78 g_namePool / Offset / Length),
178 uncertainty rows merged into UNCERTAINTIES.md (U-0110..U-0514). G1 (re/tools/model_merge.py, re/model/addresses.tsv 1,119 rows with 0 overlaps and 0
duplicate symbols, re/model/include/simgolf_model.h compiled with MSVC, tests/test_model_header.py, re/analysis/data/model_integration.md,
model_defects.md, model_name_changes.md with 80 changes): model_check over all 1,812 functions gives 13 explained errors and 770 warnings, the hook
cross-check 0 unexplained; the names were applied to the Ghidra master (backup ghidra/backup/SimGolf.rep.pre_phase1names_20261018, ApplyNames overwrite:
24 renames, 800 labels). Findings that change older notes: hole n is the record at 0x575ab0 + n*0x208 (record 0 a swap slot, 19 the leaving slot);
animals start at 0x56d1b8 and g_typeAttr 0x578372 is field +0x22 of the table at 0x578350; 0x822c74 is a 64K-entry halving table and 0x822c78 a random
course id (not the title ticker); 0x5a4998 is the 51x51 terrain corner-level grid (stream C); gRngState 0x4ba800 is a read-only double constant (withdrawn);
the vtable 0x004bb3f8 class is TextView (Dialog:: names are the older prefix); sound.dll's sprite and sound objects (g_sounds 300 x 0x6c). The game's own
save routine refreshes g_year (0x5a6d3c) through appendDate, so a port must recompute the year from the date after loading (model_defects.md 33).
showBulletinBoard writes two words one past the golfer table (record 156) into the tile_build_code grid. Lessons: a hold-out check that reads every
names file stops being a hold-out once the streams add theirs (excluded model_* files); names_validate needs a re/analysis/systems/<id>.md stub per names
id; claude2 children cannot write the git index, so the parent commits their files by explicit path; ownership overlaps between parallel streams were
settled by early notes, so tell the affected streams at once.
Stream H (2026-10-18, naming the exe game-code FUN_ functions with the finished model; brief re/model/H_BRIEF.md, candidates re/model/h_candidates.tsv in four
chunks, four claude2 Sonnet children; notes re/model/h<k>_notes.md, names re/names/model_h<k>.tsv): 279 of 281 functions named (h1 70 distinct, h2 70, h3 71,
h4 68; 2 deferred: 0x0047e770 View slot 78, one h2 row), 0 bad citations. Groups: atexit destructor stubs (about 70: gSurfaceDtor_*, exitDtor_*, per-global
destructors), this-adjustor thunks and vtordisp pairs, window-class slot defaults (one constant-return default fills 363 slots), NodeList / Snd / VoiceRx
forwarders named by target slot (Class::fwdDevN / callChildN), TextView / EditBox / Button / scrollbar slot bodies, UI event-table entries, static-init
stubs, and ten course / terrain / UI functions called by mainLoop (raiseCornerLevelsAround, switchToCourse, saveCourse, panelListSelect ...). Applied to the Ghidra
master (backup ghidra/backup/SimGolf.rep.pre_streamH_20261018, 279 renames); tracker: 209 rows C0 -> C1 through `re_classify.py batch re/names/model_h*.tsv --to
C1`, 200 of them C2 through `c2_note.py --all` + `batch --to C2` (10 thunks of 5 bytes or less stay C1); exe C1 10, C2 235 after it, total C2 586. Defects
found (not fixed, addresses in the notes): 5x5 corner-level windows with no bounds test, saveCourse's unbounded filename copy and untested _open,
setPendingCourseInfo's unbounded copy, divisions by an untested value at 0x0047e8e1 / 0x0047e8f7 / 0x0048661e, unset eax returns in NodeList::fwdChild9 / 15.
Model corrections reported and listed in re/model/DECISIONS.md (applied at the start of the next phase): vtables 0x004ba2d8 / 0x004ba468 are SpriteRef (0x2c) and
FontRef (0x20) not GfxSurface / SurfaceRef; e1_vtables.tsv's SgRecList (0x004bc3b4) is not supported by its slots; no vtable rows for 0x004ba2a8;
g_ui83aa9c 0x0083aa9c is the root-window pointer, not int32[2]; the 16-record table (0x58 each) at pointer 0x0083ff6c is untyped; Snd slots 41-45 are
unimplemented constant-0xb stubs. Save-field tool: model_sve_fields.py and model_ctypes_gen.py now build from the merged re/model/addresses.tsv (1,116
mirror symbols; all 91 save fields named and resolvable).
Phase 2.1 premise check (2026-10-18, re/analysis/data/premise_check.md, tool re/frida/restart_ab.py, runs log/p2/p21/, notes
log/p2/p21_notes.txt, instance instances\p21 with the pinned references' winmm.dll 381ed2b2): can the ORIGINAL restart from a state
image at a checkpoint and reproduce the next ones? Restart instant = mainLoop's frame head (the timeGetTime call at 0x0040fc29,
return address 0x0040fc2f); the opt-in IMAGE block of scenario_det.js (cfg.image; default unchanged: a plain sandbox_basic replay is
SAME as the pinned det_b) captures .data 0x004c1000+0x381174, the stack, T, the clock value the game receives, Q, pending events,
virtual timers and input there, and writes them into a fresh process that replayed the schedule to an earlier checkpoint; the
scenario script is then advanced offline over the source's polls. Scenarios sandbox_basic, course_season and p21_mood5 (mood5.sve
with 15 checkpoints), three continuous runs each: (a) two continuous runs SAME everywhere; (c) a full .data image minus the DWORDS
that differ in two other continuous runs restarts SAME (all of .data, frame, T) in all three, raw it crashes on frame 1 (Surface_blit3
0x00475d73) and minus byte-level noise it crashed too (a heap pointer of another process shares its low bytes: half-written
pointers); the non-pointer model objects alone give every simulation object equal (g_sounds' plain fields and 2 unmodeled bytes
differ). 12 dwords in 5 objects the model types as data hold heap pointers (g_rootWindow +0x11c..+0x518, g_listPalette,
g_msgBoxLabelA/B, g_graphsyObject: premise_check_flag_gaps.tsv, U-0604). (b) a save (the game's own saveGame at the instant,
loadGame in the fresh process) diverges in all three (11 / 48 / 33 simulation objects at k+1); the causal state it lacks is
g_rng 0x00822d9c and g_lastTipDate 0x0055e5ac (save + those two: no saved object differs in 48-480 virtual s; 13 stale buffers,
UI, g_sounds and story text still do); loadGame also rebuilds g_cornerCache / g_tileWallMask / g_tileScreenY differently, edits
g_flags and redraws Terrain.dll's state. Hidden-state table (123 objects, classes H0-H8): premise_check_hidden_state.tsv.
Terrain.dll's state is outside every image: frames differ when the prefix had another course (U-0607). mainLoop's frame need not be
written at the frame head. Negative controls RED: g_date +1, g_rng +1 (two scenarios); g_cash_div100 +1 is rewritten within 60 s
(U-0605). Pitfall: a value-in-mapped-range test is not a pointer flag (string pools, grids, RNG words pass it); it is applied only
inside pointer-flagged objects. Tests: tests/test_restart_ab.py (static), tests/test_restart_ab_runtime.py (slow, 67 s).
Phase 2.2 GameState (2026-10-18, re/analysis/data/gamestate_gen.md, notes log/p2/p22_notes.txt, runs log/p2/p22/, instance
instances\p22 with the pinned references' winmm.dll 381ed2b2): re/model/addresses.tsv has an 18th column `pointer` (model_merge.py:
`type` by restart_ab's rule = phase 2.1's 195 objects / 2,863,036 B; `words:+0x..` for U-0604's 12 heap dwords in g_rootWindow,
g_listPalette, g_msgBoxLabelA / B, g_graphsyObject, evidence in POINTER_WORDS; the header and model_ctypes.py are unchanged).
`re/tools/model_state_gen.py` generates port/include/gamestate.h (struct portsim::GameState = golf_clean.exe's .data, 0x381174 B,
#pragma pack(1), 989 objects at their offsets with model types + 425 gap members, offsetof / size static_asserts, no address
literal), gamestate_meta.inc (SG_OBJECT symbol, offset, size, class, pointer, hidden code, saved; SG_POINTER_WORD; SG_GAP) and
gamestate_addresses.inc (tooling only). Classes: sim 203 objects / 518,308 B, hidden 18 / 14,594, ui 122 / 13,093, render 67 /
120,424, audio 13 / 33,028, pointer 186 / 2,821,872, engine 51 / 11,065, const 329 / 90,789, gaps 51,311 B; never compared:
pointer, audio, the 12 pointer words, gaps (2,906,259 B). `re/tools/state_image.py` turns a scenario_ab checkpoint dump, a full
.data dump (.full.bin / .fh.bin / .pre.bin) or a .sve (model_ctypes over sve_fields.tsv) into a GameState image with a known mask,
hashes objects (sgh64: dword sum with odd position multipliers + splitmix64; pointer words as 0) and diffs them (`diff`: symbol,
class, first differing byte). Opt-in tracer in scenario_ab.py: C3FT_OBJHASH=1|image (`<name>.objhash.json` per checkpoint),
C3FT_FRAMEHASH=sim (a native CModule, re/frida/js/framehash_cm.c, called from the tgt stub at mainLoop's frame head 0x0040fc2f, no
JS per frame; framehash.bin), C3FT_PERTURB=va:xor@ms (agent action x32); `re/tools/frame_trace.py cmp` finds the first divergent
frame and its objects; `re/frida/state_trace.py run|cmp|check` drives it. Proof: plain and traced sandbox_basic replays SAME as det_b
10/10; two OFF runs equal in every frame record (sandbox_basic 439, course_season 6,296) and in every non-pointer, non-audio object
at every checkpoint (10, 12); g_rng ^= 1 at T 20,099 located at record 154 (T 20,105, g_rng alone), g_date ^= 1 at T 300,071 at
record 2,985 (T 300,119, g_date alone). port/ skeleton: CMakeLists.txt (C++17, library portsim, ctest gamestate_layout; x64 and
Win32 both build and pass), README.md (rules), .gitignore (corpus/, build/). Tests: test_gamestate_gen.py (MSVC x86 / x64 compile
with every offset asserted against addresses.tsv), test_state_image.py, test_frame_trace.py. U-0610..U-0613.
Phase 2.3 time and input model (2026-10-18, re/analysis/sim/time_model.md, schema re/model/trace_schema.md = port/schemas/trace.md,
notes log/p2/p23_notes.txt, instance instances\p23 with winmm.dll 381ed2b2): `re/tools/time_input_audit.py` lists every clock /
timer / wait / input import call site of the four modules (re/analysis/sim/time_input_sites.tsv, 253). golf_clean.exe reads only
timeGetTime; the one read whose VALUE enters simulation state is seedFromTime 0x00467110 (g_seedTime = t, g_rng = t * 37, once from
gameMain). Game time is FRAME-counted: P5 does g_date += 1 per unpaused frame (0x0041762f; g_flags bit 2 = pause, key 0x50; bits 3
and 17 have no setter), 1,024 frames = 1 game month, 8,192 = 1 year. The H1 wait (0x0041e631..0x0041e6b1) holds a frame to
frame-head time + 87 ms (+125 with bit 17) in waitTicks(1, 0) steps of 10 ms; g_frameMs 0x005a8c88 is write-only; under the virtual
clock a frame is 99 calls = 99 virtual ms (99.9% of frames; 5 course_season frames skip the wait, U-0651). pumpAndRedraw(0) (16
callers, incl. updateGolfers / pathStep / prepareShot) blocks until a click or key (U-0650). Input: messages -> jgld windowProc ->
root handlers (WM_LBUTTONDOWN -> rootWin_onPoint, WM_LBUTTONUP -> onPress1 = the click + onUnpoint, WM_KEYDOWN -> onChar, WM_CHAR
-> onKey, WM_KEYUP -> onChar0); in a frame they are dispatched in H1 (never in the wait), keys one frame after posting; H1 takes
the key / code with 0x0045ae50 / 0x0045ae30 (misnamed setDirtyFlagB / A: take-and-clear readers); capsLockOff reads
GetKeyboardState every frame. TRACES: `re/tools/trace_tool.py record|replay|validate|summary|dump|diff` (reader
re/tools/trace_format.py) appends re/frida/js/trace_rec.js to scenario_det.js through a Run subclass installed after
scenario_refs.setup (no shared file edited): native wrappers over scenario_det's clock stubs log every main-thread clock call
(site module-relative, value), FRAME records at 0x0040fc2f with the input globals, harness deliveries, window messages, root
handlers, key-state / cursor reads, timer registrations (timers named by registration ordinal: their ids are heap pointers); REPLAY
feeds the recorded values and deliveries back with the virtual clock unused on the main thread. sandbox_basic / course_season /
mood5_fixture: two records byte-identical (580,656 / 7,867,550 / 371,014 B; 439 / 6,296 / 245 frames; 66,744 / 936,098 / 57,910
clock calls), checkpoints SAME as det_b, and the replay from the trace alone writes a byte-identical trace with 0 site mismatches,
every delivery applied and 10 / 12 / 4 checkpoints SAME. Traces go to port/corpus/traces/ (gitignored). Tests:
tests/test_trace_format.py (static, synthetic traces), tests/test_trace_runtime.py (slow, 40 s). U-0650..U-0654.
Phase 2.0 scope manifest (2026-10-18, re/analysis/data/port_scope.md, rules re/port/scope_rules.md, notes log/p2/p20_notes.txt): every one of the
2,934 exe functions has a port module in `re/port/scope.tsv` (columns addr, name, size, hooks_level, subsystem, port_module, mainloop_phase,
reach_reason = rule id, touches_sim_objects, touches_heap_objects, evidence, plus the object lists and a straddler flag), generated by
`re/tools/port_scope.py` from model_inventory's reach map and tags, model_scan's references and the GameState object classes (inputs cached
in log/p2/p20/inputs.json, deterministic, tests/test_port_scope.py). Counts: crt-dropped 1,263, ui 709, backend-gfx 249, boot-only 195,
sim-core 147 (mainLoop kept as one row), engine-glue 128, backend-audio 108, formats-loaders 93, sim-leaf-util 22, net 17, unknown 3.
`re/port/simcore_order.tsv` orders the 169 sim-core + leaf functions leaves-first (11 levels) with globals, hook source, evidence, defects and a
suggested phase 3 child (3.1 90, 3.5 21, 3.2 15, 3.3 13, 3.4 13, 3.6 8, 3.0 6, 3.7 1, 2 unassigned). 126 straddlers (sim state and ui / heap
objects, 62 of them in the ui module: the click / handler functions that carry command effects) are in `re/port/scope_straddlers.tsv`: phase
3.7 must take them from there. Rules that mattered: constant tables f_ptrtab_ / f_strpool_ are not heap objects, a ui-family tag inherited from
neighbours does not make a function UI when it touches no heap object (R6-SIM-RETAG), show* / draw* names go to ui even with a sim tag. U-0660..U-0662.
Phase 2.4 segment corpus (2026-10-18, re/analysis/data/segment_corpus.md, tool re/tools/segment_corpus.py, notes
log/p2/p24_notes.txt, runs log/p2/p24/<scenario>/, instance instances\p24 with winmm.dll 381ed2b2): a SEGMENT = a frame-head
.data image of a continuous run (start.data.bin = portsim::GameState, noise.bin = dword bitmap of the words that differ
between three continuous runs plus the 12 flag-gap dwords), the trace slice from that frame head to an end checkpoint
(trace.trace, a stand-alone valid schema v1 trace rebased to 0), the expected per-frame hashes of the 203 sim objects
(frames.bin, record 0 = the start frame = the image's own hashes) and the checkpoint object hashes (checkpoints.json);
corpus in port/corpus/segments/<scenario>/<k>/ + port/corpus/segments.json (local), committed table
re/port/segments_manifest.tsv. Per scenario: three capture runs c_a / c_b / c_c (restart_ab's IMAGE block, one capture
armed at a time, C3FT_FRAMEHASH=sim, C3FT_OBJHASH=1; all 177 SAME as the pinned det_b), the trace (trace_tool's recorder;
its FRAME values and indices equal the frame-hash records in every scenario), a plan (prefix = the first checkpoint taken
inside the frame loop, capture within 250 clock calls, with an empty message queue; segments of >= 20 frame heads, at most
6 per scenario, 10 for course_season), then per segment two restarts from the image (both must reproduce every frame
record's T and 203 hashes and the end checkpoint's sim hashes) and, when the slice holds input, a COUNTERFACTUAL restart
with the input dropped: COMMAND if it diverges (its first divergent object is recorded), UI-ONLY if not. All 59 pinned
scenarios: 193 segments, 190 ACCEPTED in 53 scenarios (35 TIME-FORWARD 1,534 frames, 155 COMMAND 56,260 frames, 0
UI-ONLY), 3 rejected (golfer_events 4: trace_rec.js writes its injected `call` action as DELIVER kind 0, which
trace_format rejects, U-0681; cov_a_quit 0-1: the game quits, so the recorder never sends its last chunk), no segment in
cov_a_quit_title / cov_b_location (no in-frame capture) and cov_b_locations / cov_k3_career / cov_k1_steps (< 20 frames
after the prefix); negative control g_rng + 1 RED at record 0. Findings that extend phase 2.1's rule: (1) a prefix on the
title menu never reaches a frame head (the restart stops polling the prefix script); (2) the `data` variant crashed
restarts of build-heavy segments (heap pointers equal in all three continuous runs, invalid in the restarted process,
whose prefix is shorter): restarts use `data+` (default since the first 239 restarts), sim objects are written the same;
(3) INPUT IN FLIGHT: a message posted before the start frame head and dispatched after it is in the Windows queue, not in
any image (cov_l_camera step0's VK_UP KEYDOWN: both restarts diverged at record 0); `queued_posts` (FIFO over DELIVER
post / MSG records) finds them, restarts post them again from inside the restoring clock call, start.json carries
`queued_messages` (2 segments). The first counterfactual effect of the season loops' Esc presses is g_cachedCameraTileX
0x00543d04 (U-0682). Run hygiene: Frida spawns hung (game suspended at spawn, or no child at all) about six times while
other sessions also spawned (U-0683): the tool has its own spawn lock, a spawn watchdog (150 s, kills only its own child
by parent pid: a suspended process reports no ExecutablePath) and step watchdogs; it skips the shared boot lock, which was
held for 600 s twice by an unknown owner (U-0684). Port-loading rule, frame alignment and pass criteria: segment_corpus.md
section 5. Tests: tests/test_segment_corpus.py (static, 13), tests/test_segment_corpus_runtime.py (slow, 95 s).
Phase 2.5 vector corpus (2026-10-18/19, re/analysis/data/vector_corpus.md, schema re/model/vector_schema.md, notes
log/p2/p25_notes.txt, instances p25 / p25b / p25c): the recorded inputs and outputs of the ORIGINAL for 308 selected exe
functions (re/tools/vector_select.py -> re/port/vector_selection.tsv: C3/C4 sim / golfer / economy / course / terrain / ai rows,
the util and crt rows they reach through hooks.csv callees, the other pure / globals-only util hooks, simcore_order.tsv
rows), 642 registry keys. `re/tools/vector_capture.py capture` boots with SIMGOLF_HOOKS_OFF=all, loads diff_hook.py's files
with re/frida/js/vector_capture.js in place of diff_hook.js (shared files untouched; it wraps __keepAlloc, the stand-in
entry points and Interceptor at run time), FREEZES the first thread for every key (the menu changed up to 1,753 data dwords
between two vectors otherwise), records the post-fixture data image, the fixture blocks, per vector the drift and the exact
dword / byte changes, the functions the fixture stood in (first 8 bytes of every function start against the file), and
fetches in the same boot the live pages a light emulation misses. `build` replays every vector in Unicorn over the FILE image
(re/tools/vector_emu.py: full flat GDT, fs -> fake TEB, IAT slots -> trap addresses, memory hooks bound to the image and
blocks) and keeps a vector `ok` only when the emulated original gives the live return value and its writes equal the live
changes; inputs (seed / in), outputs (out, out_ptrs) and relocations come from the emulation, relocations proven by a second
run with every block moved (`reloc_check`). Output port/corpus/vectors/<function>.vec (local, ~10 MB; raw captures 0.57 GB)
and the committed re/port/vectors_manifest.tsv / vectors_coverage.tsv. `capture --unstub` re-captures the 122 keys whose
fixtures stand in selected functions with those stand-ins reverted (+unstub keys; 3 functions reproduce only that way).
Result: 243 functions with vectors, 15,191 reproduced vectors; `re/tools/vector_validate.py emu` (from the .vec alone, zero
data, blocks moved) 15,188 pass and `live` (the shim's reimplementation in a hooks-on boot) 15,189 pass, every failure a
vector flagged reloc_check false. Excluded: 65 functions (callee stand-ins the emulation cannot reproduce, imports such as
the CRT's EnterCriticalSection, calls into sound.dll / KERNEL32 code, __ftol's st(0) input). Coverage verdicts (per jcc side
from the emulation's edges): full 113, one_sided 35, partial_branch 24, thin 71, none 65. Pitfalls: running diff_hook.py
directly with hooks off overwrote an evidence CSV (restored from git); Frida frees an unreferenced CModule's code; a Proxy
on Interceptor breaks its invariants; the live replay must freeze too (the menu writes g_textBuffer). Tests:
tests/test_vector_corpus.py (static, 30), tests/test_vector_capture_runtime.py (runtime, 22 s). U-0700..U-0704.
Phase 2.6 harness (2026-10-19, port/test/README.md, notes log/p2/p26_notes.txt): the port's test harness, judging C++ port
code against the vector and segment corpora with no game, no Frida and no original binary run. C ABI port/include/portsim_c.h
(DLL target portsim_c, x64 for the 64-bit Python; the Win32 build compiles too): sg_state_new / free / size / ptr, a 32-bit
VIRTUAL address space per context (GameState at a caller-set base, sg_map for harness blocks; port code resolves 32-bit
pointers through ctx.mem and throws portsim::Fault -> SG_E_FAULT, never casts), sg_call(ctx, name, uint32 slots) by hooks.csv
name returning eax / edx / st(0), sg_func_count / name / info (abi, ret, arity, fpcw), sg_set_fpcw (x64 accepts only
0x027f-equivalent SSE2 semantics), negative-control switches sg_negctl_set by function name (no fixed-address page), the
gamestate_meta.inc object table and sgh64 in C++ (sg_hash_class; equals re/tools/state_image.py on every class), and
sg_run_frames with its final signature returning SG_E_NOT_IMPLEMENTED until 3.7 / 3.8. Registry: port/src/sim/<module>.cpp
defines `extern const portsim::ModuleDef kModule_<module>` (FuncDef: name, abi, ret, arity, fpcw, adapter `void(Ctx&, const
uint32_t* args, Ret&)`, negctl int*), listed by one `SG_MODULE(<module>)` line in port/src/sim/modules.inc; CMake globs
src/sim/*.cpp. First client port/src/sim/util.cpp (Random::next, Random::range, clamp, distance, written from the notes and
the corpus): 4 functions, 279 / 279 vectors GREEN (15 + 32 + 160 + 72, coverage full for all four). Python: lib.py (ctypes,
builds the DLL when stale), vectors.py VectorRunner (zero GameState mapped at the original data VA, seed then in, blocks at
fresh page offsets, relocs, fpcw, call, ret / ret_loc / pointer returns as locations, every out region with out_ptrs, and
no changed byte outside the out regions; skips non-ok, reloc_check false and raw-location vectors with reasons; warns under
10 vectors, zero spread or non-full coverage), segments.py SegmentRunner (load check: frames.json symbols = the library's
203 sim objects, no noise dword in a sim object, C++ sim hashes = frames.bin record 0; then sg_run_frames, LOADED_ONLY until
it exists) with first_divergent_record / first_byte_divergence, report.py (`~/bin/py -3.12 -m port.test.report` ->
log/p2/p26_report_functions.tsv / _segments.tsv). Tests: `~/bin/py -3.12 -m pytest port/test` (markers static and port,
the new `port` marker in pytest.ini): 219 pass in about 16 s, all 190 accepted segments LOADED_ONLY. Negative controls RED:
Random::range wrong bodies (results 0/1 swapped 8 of 15 vectors fail, scaled by n + 1 4, rounded to nearest 6), a flipped out
byte and an incremented ret of a vector, a dropped out region (extra write), a flipped byte in any sim object of a segment
image (load check fails and first_byte_divergence names object and byte). Finding: listAddEntry 0x004a28a0 has one ok vector
whose argument is a `raw` location, which the schema says does not occur (U-0720). U-0720..U-0724.
Phase 3.0 core (2026-10-19, port/CORE.md, notes log/p3/p30_notes.txt): the conventions stage B builds on. port/include/portsim/
arith.h (wrapping add32 / sub32 / mul32 / neg32, x86 shifts with count mod 32, div32 / mod32 throwing DivideError = SG_E_DIVIDE
where idiv raises #DE, abs32, sx8 / zx8, ftol_low32, NoReturn = SG_E_NO_RETURN for a loop the original can never leave), ptr.h
(`Ptr<T>` over the 32-bit virtual address space: wrapping element arithmetic, `field(&Struct::member)`, faults as SG_E_FAULT;
`state_arr / state_obj / state_grid / state_va` over GameState members; the VC6 idioms rep_movs, vstrlen, vstrcat, vstrcpy,
vstrpbrk), DefectDef compatibility switches in ModuleDef (default 1 = the original; sg_defect_*; first switch
sinScaled.readPastTable = model_defects#10), execonst.h (`ExeConstants`: the user's own exe, verified by the .data size and an
sgh64 per descriptor, constants by model name; `execonst_desc.inc` generated by re/tools/execonst_gen.py from
re/model/f_descriptors.tsv holds offsets, sizes, types and hashes only; fill_state writes the initial `const` objects or the whole
initialised image; tests use $SG_EXE_PATH or original/golf_clean.exe) and gamefiles.h (`GameFiles`: bytes by logical name =
path relative to the install root, MemoryGameFiles / DirGameFiles / NullGameFiles, sg_files_*). Modules util (extended), core,
tiles, text: 31 functions ported (27 new: the six 3.0 rows appendClubName, overlayCharAt, staticInit_gRngZero, swapTableEntry,
approxDistance, direction8, plus sign, directionOf, angleFixed, sinScaled, foldRange, absDiff, cosScaled, bucketValue, decaySum,
decaySum2, tileDistance, tileType, tileByte, tileFlag20, tileBlocked, typeAtPos, wallHeight, objectAt, inRange, sanitizeFileName,
initSinTable), 3,108 / 3,108 vectors GREEN (2,829 new); staticInit_gRngZero and initSinTable have no vectors and get state
tests (initSinTable with the CRT sin equals the g_sinTable of all 190 segment start images). Every function has a negative
control (test_core.py NEGCTL_RED, all RED); four wrong bodies the vectors cannot see are recorded as NEGCTL_BLIND (U-0742).
objectAt and sanitizeFileName have one-sided branch coverage: segments must cover them. Finding: the SafeDisc golf.exe also
encrypts .data (404 of 155,648 bytes equal), so ExeConstants needs the unwrapped exe (U-0744). U-0740..U-0745.
Phase 3.R 32-bit runner (2026-10-19, port/test/README.md "x87 and the Win32 backend", port/CORE.md section 7, notes log/p3/p3r_notes.txt):
U-0721 (x64 harness, SSE2 doubles, no 32-bit Python) closed by an OUT-OF-PROCESS Win32 runner: `port/test/runner32/runner32.cpp` (CMake target
portsim_runner32, -A Win32 only, built by `port/test/build.py --arch Win32` or on demand) loads the Win32 portsim_c.dll and serves the unchanged C ABI
over stdin / stdout (`u32 length, u32 op, payload`; reply `u32 length, i32 status, payload`); `port/test/lib32.py` (`Lib32`, `State32` subclassing
lib.State) has lib.py's interface, so VectorRunner / SegmentRunner / report.py run unchanged. `SG_BACKEND=win32` (or `pytest --backend win32`) makes
`lib.load()` return it; default stays x64; `lib32` / `lib64` fixtures give both in one test; no Win32 toolchain = `RunnerUnavailable`, a skip with the
reason. The GameState is a named shared mapping copied into the DLL state around each handle op (no state bytes on the pipe); address-space edits are
queued and sent with the next op, writable blocks travel with each call, exe sections are cached once per runner. `sg_set_fpcw` really executes fldcw on
Win32 (any word); the runner applies it per call, reports the control / tag word after it (`State32.last_fpu`) and restores 0x027f + fninit. Test-only module
`x87probe` (`x87Probe::mulDiv`, `controlWord`, inline x87 asm under _M_IX86): bit-exact against an exact-rational x87 model for 0x027f 0x037f 0x0f7f 0x077f
0x0b7f 0x007f; the same vectors are GREEN on Win32 and skipped as unsupported on x64; the wrong-rounding body (negctl 1, plain C doubles) reads RED at
every non-default word and is an equivalent mutant at 0x027f. Numbers: all 31 ported functions give identical run / passed / distinct on both backends
(the dashboard TSVs are byte-identical: 29 GREEN, 190 segments LOADED_ONLY, 63 s Win32 against 67 s x64); a bare call costs 0.43 ms against 0.002 ms
(two 3.6 MB copies), a vector run 18.0 against 17.5 ms (harness setup dominates). Suites: port/test x64 387 passed, SG_BACKEND=win32 322 passed, static
600. Caveat U-0761: MSVC compiles Win32 C++ doubles as SSE2 as well, so only `__asm` x87 code (or /arch:IA32 sources) honours fldcw: 3.4 writes x87
functions as asm under _M_IX86 plus a C body for x64. U-0760..U-0763.
Phase 3.2 economy and membership (2026-10-19, port/src/sim/economy.cpp, port/include/portsim/sim/economy.h, tests port/test/test_economy.py, notes
log/p3/p32_notes.txt): of the 15 rows of simcore_order.tsv, 9 ported (computeRatingTrend, appendDate, appendNumber, appendRatingLabel,
appendToBuffer45b8b0, clearCost, rateLot, updateRollingStats, updateMembership) and all pass their vectors (160 of 160); canAfford, membershipReport, addScore,
matchUpdate and updateGolfers are straddlers (3.7) and hireEmployee has no vectors and calls modal UI (U-0840), so they are open. Coverage: appendNumber and
appendRatingLabel full, six one_sided, updateRollingStats thin (3 vectors); 14 negative controls RED, one blind spot (updateRollingStats grace) covered by a state
test, plus a state test of updateMembership's decline arm. Port notes: strings are named by pool offset or character, and the month names come through
the pointer table f_ptrtab_4c2908 (U-0841); typeBit7Clear is a private copy of 3.3's row (U-0842). U-0840..U-0844.
Phase 3.1b course (2026-10-19, port/src/sim/course.cpp, port/include/portsim/sim/course.h, tests port/test/test_course.py, notes log/p3/p31b_notes.txt):
of the 30 rows, 7 were already ported in 3.0 and the other 23 are registered in module `course`: records (freeRecordAtPos, initRecords with resetRecordBank /
placeRecord as internal helpers, clearMatching, staticInit_820f04, setPendingCourseInfo, clearBuffers), names and titles (appendString, landmarkName,
buildGolferName, buildStatusText, appendHoleName, appendCourseTitle, sceneryLabel, buildScenarioIntro), geometry (nearestHole, holeQuadrants), seedCourseTable,
files over GameFiles (fileExists, lookupThemeText with a CRT text-mode fgets model, loadThemeFile without the portrait decode / tile blits), listEraseRange,
loadThemes and switchToCourse. 1,033 / 1,033 vectors pass (15 functions; full 9, thin 4, one_sided 1, partial_branch 1); the 8 without vectors have state tests
(staticInit_820f04 also against the 190 segment start images). Other modules are called through the registry by hooks.csv name (MissingResource while absent):
sceneryLabel -> 3.1a's decorationName, loadThemes -> loadGolferRoster (3.3) / listMatchingFiles (unassigned), switchToCourse -> courseActionMenu (ui) + 3.1a's
course builders (switch_to_course_chosen is exported for 3.7); buildScenarioIntro uses a local copy of startMessage until 3.7 registers it. 22 + 2 negative
controls RED, 4 blind on the vectors (clearMatching b, listEraseRange x2, startMessage priority) of which 3 are read by state tests. MSBuild does not recompile
registry.cpp when only modules.inc changes: touch it. U-0820..U-0825.
Phase 3.3 golfers, walkers, animals (2026-10-19, port/src/sim/golfers.cpp + golfers_glue / _world / _walkers / _pro.inc,
port/include/portsim/sim/golfers.h, tests port/test/test_golfers.py, notes log/p3/p33_notes.txt): 19 of the 21 rows of 3.3 and
3.6 ported, 367 / 367 vectors GREEN in 16 functions (appendPairTrackPoint, addGolferPair, resetGolfer, golferAge, typeBit7Clear,
appendEndearment, appendWalkerName, spawnObject, spawnWalker, addHabitatSlot, tickAnimals, placeHabitat, updateWalkers 92/92,
initGolferState, addGolferToTable, loadGolfPro 2/2), plus placeAnimal (judged through tickAnimals' vectors and state tests),
resetGolferState and composeGolferSprite (no vectors: state tests; composeGolferSprite keeps only its look-index write-back into
the type record, the layers are drawing). Not ported: loadGolferRoster (needs a GameFiles directory listing for
listMatchingFiles, which has no owner; U-0864) and skillPointDialog (a modal input loop whose effect is a 3.7 command; U-0865).
Every function has a negative control (RED on vectors, on a caller's vectors, or on a state test; blind modes listed in
test_golfers.py with the state test that catches them). The VIEW / SIM BRIDGE is module `bridge` (port/src/sim/bridge.cpp,
portsim/sim/bridge.h, tests port/test/test_bridge.py; owner 3.3 by the parent's decision): worldToScreen, playSoundAt (simulation
part: playSound is the audio backend's), queueMessage and pointsPopup, registered under their hooks.csv names for 3.4 / 3.5. They
have no corpus vectors, so test_bridge.py replays their C3 path-1 runs: it rebuilds each fixture in a zero GameState, calls the
port and compares the return and diff_hook's FNV-1a hash of the compared regions (the hex of each region joined by |):
worldToScreen 103 of 112 rows exact (the 9 others sit on TTA_LEVEL_MIN tiles whose height reads terrain grids the fixture left
live, U-0867), playSoundAt 27 / 27 through the test-only entry bridge.playSoundAtScripted (the C3 run stood in worldToScreen and
playSound; its call log, including volume / pan / pitch, is rebuilt and hashed), queueMessage 16 / 16, pointsPopup 72 / 72.
playSoundAt draws g_rng only when worldToScreen puts the point on screen, so the simulation RNG depends on the camera and screen
globals (U-0860). Peers are called by hooks.csv name through the registry. Defects reproduced as faults / NoReturn:
model_defects 40..45 (placeHabitat's cluster loop never advances, spawnObject's cell search, loadGolfPro's two divisions and
pro draw, placeAnimal's idiv, resetGolferState on one record, addGolferToTable's discarded draws). A private build (a
scratchpad copy of port/ with only the compiling modules, loaded through SG_PORTSIM_DLL) kept work going while siblings'
files were mid-edit. U-0860..U-0867.
Phase 3.4 shots (2026-10-19, port/src/sim/shots.cpp, port/include/portsim/sim/shots.h, tests port/test/test_shots.py, notes
log/p3/p34_notes.txt): the 7 rows of 3.4 not already in util (decaySum, decaySum2, sign, sinScaled, foldRange, absDiff came from
3.0) are ported: shotPower 64/64, wobble 19/19, trialShot 228/228, chooseShotTarget 12/12, prepareShot 307/307, scanShotArea
289/289, narrateShot 2/2 (921 vectors), identical on the x64 and Win32 backends, with 3.1a's real terrain callees. The cycle's own x87 code is `fild; fsqrt; __ftol`, run as inline x87 on Win32 (sqrt_ftol). Its
vectors stay GREEN at 0x037f / 0x0f7f / 0x0b7f, and a body that rounds instead of chopping reads RED on both backends. Other
children's callees are called through the registry by hooks.csv name (MissingResource until registered); world_to_screen
comes from bridge.h (module bridge, 3.3). trialShot, chooseShotTarget, prepareShot and narrateShot are listed as 3.7 straddlers but are 3.4's. The first
three drop only a debug overlay and message pumps (U-0881). narrateShot keeps its state effects and drops drawing, sound, the
modal pumpAndRedraw and saveBmp; startMessage comes from the registry once 3.7 registers it, a local copy until then (U-0883). Uninitialised stack slots (prepareShot's
dirs[], chooseShotTarget's strict flag) start at 0, as in the corpus's zeroed-stack emulation (U-0880). Switches: model_defects
#18 shotPower.cacheIgnoresPutt (OFF changes exactly the 32 putt vectors), #37 chooseShotTarget.strictFlagSticky (no vector
reaches it), #38 prepareShot.hazardCellTransposed (OFF changes only real-shot vectors), #39 narrateShot.maskNotRestored (state
test). The static test reads any 6-digit hex literal in the exe's range as an address, so write flag bits such as 0x400000
as shifts. chooseShotTarget's
scoring weights and pruning are blind to its 12 vectors (each ends with one surviving cell); the segments must judge them.
MSBuild does not rebuild registry.cpp when src/sim/modules.inc changes, so touch registry.cpp after editing it.
U-0880..U-0883.
Phase 3.5 events, news and golfer stories (2026-10-19, port/src/sim/events.cpp + events_{calls,story,think,news,pair}.inc,
port/include/portsim/sim/events.h, tests port/test/test_events.py, notes log/p3/p35_notes.txt, U-0900..U-0906, model_defects 46-49): 20 of the 21
rows of re/port/simcore_order.tsv with child 3.5 (angleFixed is 3.0's, in util): appendUpgradeText, replaceInText, thoughtBalance, thoughtFlag,
appendOpinion, postEvent, runBulletinBoardWithSound, loadStoryStrings (the story file through GameFiles: "Themes/<theme or Standard>/<story file
of the slot>", read in the CRT's text mode with fgets(249)), computeStoryReply, storyText, substituteStoryNames, advanceStory,
emitGolferThought, buildGolferComment, announceLandmark, patronEvent, holeMagazineEvent, announceHoleType, announceBuilding,
pairGolfersWithStory. One module, one .cpp plus `.inc` parts (test_static wants one .cpp per module). The long bodies are mechanical
conversions of the hook sources (`log/p3/p35_hook2port.py`: comments dropped, data addresses to `GS(member) + off`, typed references
`G<T>(ctx, va)` in events_hook.h) edited by hand; a flag mask between 0x400000 and 0x8fffff reads as an address to the converter and to
test_static (advanceStory's bit 23 is written `1u << 23`). Callees of other stages go through the registry by hooks.csv names
(ev::call_peer): typeBit7Clear, buildGolferName, buildStatusText, decorationName, holeQuadrants, appendEndearment, appendHoleName,
appendNumber, landmarkName, golferAge; the phase 3.7 straddlers startMessage, appendCents and logTick have local copies of their hook
bodies when not registered (U-0903), playSoundAt is the registered bridge function (3.3; counts unchanged with it), playSound is dropped when not registered
(U-0904). Evidence: vectors 16 functions (x64 and Win32 identical; buildGolferComment 1,002/1,002 with the user's story files
attached as GameFiles: port/test/vectors.py got one line for it); the new LIVE ORACLE `port/test/liveraw.py` judges functions the Unicorn
emulation could not run against the RAW captures (live return value, live byte changes, port started from the captured data image,
blocks at their live addresses; `+unstub` keys have the selected callees real, startMessage / appendCents / playSound stay stood in, so
emitGolferThought and pairGolfersWithStory ignore the objects those would have written): loadStoryStrings 47/47, computeStoryReply
124/124, storyText 19/19, advanceStory 68/69 (one vector reads live heap the capture did not keep), announceBuilding 22/22,
holeMagazineEvent 5/5, announceLandmark 44/44, patronEvent 126/126, emitGolferThought 136/152, pairGolfersWithStory 284/301 (the failures follow
the scripted startMessage return, U-0906). Negative controls: 29 modes read RED (13 vectors, 10 live, 6 unit tests), 4 blind on purpose.
Six children building into the shared port/build/x64 lock the DLL against each other: this module was built in `port/build/p35`
(`cmake -S port -B port/build/p35 -A x64`, `SG_PORTSIM_DLL`). runBulletinBoardWithSound has no oracle (C2): only its writes around the modal
call are tested (U-0905).
Phase 3.1a terrain (2026-10-19, port/src/sim/terrain*.cpp, port/include/portsim/sim/terrain.h / terrain_api.h / terrain_gen.h,
port/test/test_terrain.py / test_terrain_gen.py, notes log/p3/p31a_notes.txt): child 3.1a's 60 rows; 8 were 3.0's (tiles / util),
mainLoop is 3.8's, 51 are ported plus applyTileVariation / applyTileWalls (unowned backend-gfx rows rebuildTileData calls), in modules
terrain (grids, heightfield, coverage, shade, noise, scanTileLine), terrain_text (decorationName, buildToolLabel), terrain_path
(pathStep), terrain_setup (buildGolferBioText, customizeCharacter), terrain_course (stampCourseTiles, clearObjectFootprint,
stepCourseEditQueue, demolishObject, buildTournamentPrep), terrain_dll (the Terrain.dll callers), terrain_gen (generateRandomCourse) and
the TEST-ONLY terrain_fake. Vectors 6,646 / 6,646 GREEN over the list (5,595 for the new functions). TERRAIN.DLL (U-0607, U-0800): no
sim-class write of the exe's Terrain.dll callers depends on a value Terrain.dll returns (their writes are engine / pointer class plus
updateTerrainRender's camera memory and g_tileWallMask[0] bit 1, from its arguments), so Terrain.dll is `portsim::TerrainApi` (20
imported methods + the engine services the same functions reach: input pump, Window::paint, blits, renderTerrainStrip, the display DC,
GetVersionExA's major version, operator new), attached per context with set_terrain_api (MissingResource without one, so 3.1b's
switchToCourse needs one too); the Terrain object stays in the 32-bit address space and its fields are read like the exe does. EMULATOR
ORACLE (opt-in `SG_EMU_ORACLE=1`, development only): test_terrain.py runs the ORIGINAL function in Unicorn (vector_emu's file image) over
random GameState images and compares the whole image and eax with the port; for the Terrain callers a recording fake (terrain_fake.cpp,
mirrored by PyFakeTerrain) serves the original at the Terrain import traps and at the engine functions' entries, and the call logs, the
Terrain object and the heap block are compared too. Every function equals the original there (generateRandomCourse 96 / 97 equal, 1 both
spin), including all no-vector / thin ones, and the 4 vector-blind negative controls read RED. Default runs check GOLDEN, SHA-256 digests
of the emulated original's results for fixed scenarios of 18 functions, from the port; every no-vector control must change them.
Builds: siblings' half-written module files broke the shared glob build, so 3.1a built a mirror of port/ (port/build/m31a, finished
modules only). Findings: rebuildHeightfield returns the new g_flags (stepCourseEditQueue returns it); propagateType11 / markTileKind11 /
applyTileWalls leave eax values their callers return (demolishObject); scanTileLine zeroes type 1's walk byte while it runs; endless
loops of the original become NoReturn (rebuildHeightfield's relax / raise, buildGolferBioText's picks, generateRandomCourse's searches,
model_defects#50..52). U-0800..U-0806.
Phase 3.7a tick (2026-10-19, port/src/sim/tick.cpp + tick_*.inc, port/include/portsim/sim/tick.h, port/src/sim/tick_hook.h, tests
port/test/test_tick.py, notes log/p3/p37a_notes.txt, U-0920..U-0929): mainLoop's frame step and sg_run_frames. All 35 TIME-FORWARD
segments PASS (every frame record's T and 203 sim hashes plus the end checkpoint). The frame body is the C3 transcription
shim/src/re/c3fy.cpp (FramePreamble .. H2) converted mechanically by log/p3/p37a_conv.py (.data literals -> GS(<object>) + offset,
calls by hooks.csv name, idiv -> DivideError, frame slots in a frame block of the address space, citations as ML+0x<off> /
<object>+0x<off> without the 0x prefix of an address) and edited only by the asserted replacements of log/p3/p37a_patch_frame.py
(clock reads from the trace, GetAsyncKeyState from the run's key state, the five switch byte tables of mainLoop's .text as case
lists, the H1 wait dropped); other hook sources go through `p37a_conv.py --hook` + log/p3/p37a_patch_scene.py (updateGolfers
465/465 vectors, matchUpdate 35/35, membershipReport 23/23, buildSceneSprites, drawFlyingBall, drawBuildingPanel,
showTournamentResults). A callee is a registered port function, one of 48 documented drops (tick.cpp kDrops; their call closures
write and address no sim object, log/p3/p37a_simwrites.py, log/p3/p37a_drop_census.tsv), or a tick fallback used only while no
module registers the name (showBulletinBoard prefix, showMainRadialMenu, drawBuildingPanel, showTournamentResults, tickMessage:
3.7b's list); anything else is MissingResource{name}, readable through the new sg_run_frames_detail. Drawing passes keep their sim
effects (cell addresses in unk_005a9cc8, the itoa buffer g_textBuf, sky actors, walker screen points, golfer look indices, tile
flags). Every TIME-FORWARD end checkpoint is taken inside the last frame's H1 wait (log/p3/p37a_polls.py), so sg_run_frames runs
that frame up to the wait. Harness: SegmentRunner maps the GameState at 0x4c1000 (unk_005a9cc8 holds a state address) and attaches
original/ as GameFiles (advanceStory's story text; without it cov_a_flags/05 diverged at record 64, found with SG_TICK_DUMP +
log/p3/p37a_emucheck.py, the original under Unicorn on the port's pre-call state). Stage B's local copies of startMessage /
appendCents / logTick (course, shots, events) now call the registered functions. Input is not applied yet (U-0925).
Phase 3.7b commands (2026-10-19, port/src/sim/commands.cpp + commands_panels / _layout / _screens.inc, commands_internal.h,
port/include/portsim/sim/commands.h, tests port/test/test_commands.py, oracle port/test/cmdemu.py, trace mapping
port/test/trace_commands.py, split table re/port/commands_split.tsv, notes log/p3/p37b_notes.txt, U-0940..U-0946): the 62 UI-module
straddlers split into command effect and drawing. INTERFACE: `Command` with the RAW kind RootHandler (one root-window handler call as
a trace HANDLER record gives it, plus the GetAsyncKeyState results and the clock value the handler reads; apply_command does exactly
the handler's writes, all 11 equal the emulated original on 400 states) and SEMANTIC kinds 0x100..0x106 (a click handler's action
with its hit test decided). tickFrame applies the frame's raw commands at H1 and attaches the rest as an `InputSource`; every pump
inside a straddler asks it for ONE dispatch batch with a PumpKind (Blocking: pumpAndRedraw(0) and the screens' click-or-key loops,
NeedInput when none is left; Drain: pumpLight / interruptible waitTicks, nothing unless the source says it arrived) and for the
cursor where a screen chooses by it (U-0940, U-0943). EVIDENCE: no corpus vector exists for any of the 62, so the oracle is the
ORIGINAL in Unicorn (opt-in SG_EMU_ORACLE=1): dropped callees stubbed with their own `ret N` convention, frameTick483cd0 runs the
original root handlers on the next scripted batch through a fresh trampoline per dispatch (Unicorn caches translations), a
pumpMessages called from pumpLight / waitTicks ([esp + 0x20] on entry) is a Drain, View_toLocal returns the scripted cursor, other
children's callees run for real (buildGolferComment recorded on both sides instead, U-0945); eax, consumed input and the whole data
image must equal the port's (only drawing scratch masked). The default suite checks golden digests pinned from the port after the
oracle was GREEN on them (port/test/golden_commands.json) and every negative control against them. CLASSIFICATION by a write audit
(log/p3/p37b/audit.py: each address a body writes and who reads it): 22 ported (14 command: the 7 panel click handlers, selectDifficultyScreen,
showHireEmployeeDialog, showFinancialReport, showPlayerCommentsReport, showBestScoresScreen, showHistographScreen, showLoadingScreen's
two RNG draws; 6 layout: tickMessage, drawAmenitiesPanel, showInfoCursorPanel, drawEmployeeView / DetailPanel, drawGolferActionPanel;
drawWindow / drawThoughtBubble as a helper), 10 pure drawing, 4 owned by 3.3 / 3.7a, 26 gaps with reasons (font-metric hit tests of
showPopup / textInputBox and their callers U-0944, file browsers, the remaining report and map screens). Findings: drawing writes
simulation state (composeGolferSprite's type-record write-back, appendDate's g_year, appendNumber / appendCents' hashed g_textBuf
U-0946, drawEmployeeDetailPanel's g_zoomScale = 4, walker animation word and its hireEmployee call on the panel's first draw);
hitTestSlot432f90's boxes depend on the palette sprites' heights. SEGMENTS: the 155 COMMAND segments hold 8,352 handler records in
5,420 dispatch batches (5,130 in the frame's H1 pump, 257 in screen loops, 29 in pumpAndRedraw, 4 in showHoleStatsScreen); no batch
lies inside a waitTicks loop (U-0941); running them is 3.7a's sg_run_frames (U-0925). Build: a private mirror at HEAD plus own files
(log/p3/p37b/mirror.py, port/build/m37b; each build copied to port/build/m37b_run so a running test never locks it): a sibling's
in-flight edit of events_calls.inc (startMessage's copy removed) otherwise broke the commands tests. test_static refuses image-range
literals in comments too: cite functions by name in port code.
3.7b round 2 (same day, notes section 7): U-0944 decided from the code: Text_put's widths are GDI's (jgld Font::textWidth
0x10068e60 = GetTextExtentPoint32A on a CreateFontIndirectA font over the game's font file), so the port takes them from a
per-context `TextMetrics` provider (commands.h; test provider commands.setTextWidth, oracle stub cmdemu ("width", base,
mask)); without one, MissingResource{"Text_put"} before any write. Ported over it and judged by the oracle: showPopup,
textInputBox, showPreferencesMenu, showSystemMenu (with nearestMenuSpot, which replaces tick's fallback), plus
panelListSelect (transcribed from its instructions: the H1 letter keys f/r/g/s/t/w/v call it) and drawScoreSummary; 20 gaps
left. log/p3/p37b/seg_reach.py maps every call site a COMMAND trace records to its function (which gaps the segments
reach). Segment runs reaching the widgets need a host provider (U-0947). Equivalent-mutant trap: drawEmployeeViewPanel's old
control (limit 0x2f3) changed nothing because no wide column falls in 0x2b8..0x2f3; check a control's arithmetic, not only
its digest.
Phase 3.7a input (2026-10-19, second session; port/src/sim/tick.cpp "trace slices", notes log/p3/p37a_notes.txt section 9,
U-0930..U-0935): sg_run_frames now feeds the trace's input. tick.cpp's TraceInput (the context's InputSource during a run)
splits the slice into DISPATCHES (port/test/trace_commands.py's batches) and gives each to the pump call that made it in the
original: the clock site before it gives the class (frame head or jgld's Palette::update QPC = a pump of the frame body, else
a modal loop) and the frame's cursor reads before it say which pump. Most of the corpus's dispatches come in P6's
pumpInput483bd0 (between the status bar's and P6's cursor reads), the rest in H1's pumpMessages; both are tick bodies now
(`pump_at` sets the site), pumpLight is 3.7b's. GetAsyncKeyState reads outside the root handlers come from the frame's
KEYSTATE records per key; the harness's memory writes (DELIVER w32) apply before the next dispatch, at H1's frame wait
(TkBase::wait_point) or at the frame's end. An end checkpoint inside a modal wait is compared where the last frame's
Blocking pump runs out of recorded input. Result: 109 of 155 COMMAND segments PASS (35 / 35 TIME-FORWARD still); 44 stop on
names outside tick (Text_put 17: no TextMetrics provider, U-0944; drawGolferInfoPanel 8, showCourseReport 5,
shotStyleButtonCode 4, loadGameBrowserScreen 3, showHoleStatsScreen 2, showEndOfYearScreen 2, the modal showBulletinBoard 2,
courseActionMenu 1) and 2 diverged (ui_tour/02, ui_tour_v/02: 3.1a's customizeCharacter skipped the unregistered
showCharacterIntro; its call site now throws MissingResource, so they stop there, U-0931). Fixes the segments forced: a headless Terrain stand-in for the heap objects the exe side reads
(HeadlessTerrain: Terrain object, list sentinel, operator-new arena, the corner step applyTileElevation's loop waits for,
U-0932); narrateShot's modal pumpAndRedraw(0) (shots.cpp) now waits for the next command when an InputSource is attached
(U-0933); new tick functions rankValue, playHabitatSound, drawRecords; drops drawTerrainToolbar, enterModal483d30,
Snd::status, Widget_readPixel. port/test/segments.py: NON_CAUSAL = {g_textBuf} (all 81 references are __itoa destinations,
log/p3/p37a_noncausal.py), reported in SegmentResult.noncausal, and run_detail. Runner log/p3/p37a_cmdsegs.py (TSV per
segment), summary p37a_cmdsum.py; development switches SG_TICK_INPUT_LOG, SG_TICK_CALL_LIMIT, SG_TICK_TRACE. The full
COMMAND gate is test_tick.py's slow test (about five minutes); 7 segments run by default.
Phase 3.8 integrator (2026-10-20, plan re/PHASE38_PLAN.md, shared brief log/p38/BRIEF.md, merge log log/p38/MERGES.md, per-stream
notes log/p38/38<x>_notes.md, U-0960..U-1053): eleven claude2 children (38a..38k), each in its own build dir with SG_PORTSIM_DLL,
shared files changed only through patches the parent applied. Result on the merged tree: all 200 accepted segments PASS (165
COMMAND, 35 TIME-FORWARD), x64 = Win32 in every row; 0 entries left in test_tick.py OPEN_MISSING except Text_put without a
provider, OPEN_DIVERGENT empty. New modules: gditext (38a: the host GDI TextMetrics provider, the game's 15 startup fonts rebuilt
with Font::create's LOGFONT over the font files read through GameFiles, equal to 11,459 / 11,459 recorded printable widths;
recorder re/tools/widths_rec.py + re/frida/js/widths_rec.js, corpus port/corpus/widths/; SegmentRunner attaches it by default,
SG_TEXT_METRICS=none disables it), shotstyle / loaders / saveload (38b: GameFiles list / write with an in-memory overlay that never
touches the install, listMatchingFiles, loadGolferRoster, the load browser, courseActionMenu, saveCourse, saveGameDialog,
promptAndSaveFile), reports (38c: showCourseReport, showEndOfYearScreen), dialogs (38d: skillPointDialog, showCharacterIntro,
drawGolferInfoPanel, drawScrollText layout, runScrollText, scrollTextWindow, showBulletinBoard), holestats (38e:
InputSource::clock = the trace's next clock read at a call site, showHoleStatsScreen over it), integrator (38f, test-only batch
overrides), routing (38g: showRoutingMapScreen, showShortcutsScreen, showMembershipRoster), chareditor (38h), semantic (38i),
menu + tick_menu.inc (38j: mainLoop's title menu and session dispatch converted from c3fy.cpp parts MV / SV, so sg_run_frames runs
through FrameExit::Menu; loadWorldAssets with the sprite-sheet cell allocation drawing passes store into a sim word,
loadInterfaceArt(0), startNewCourse, pickAProScreen; refreshTerrain is no longer a drop). Gates (38f, port/test/integrator.py
segments | chain | speed | defects | ledger | compare): (c) course_season as one 6,127-frame run and the new pinned scenario
course_season_6y (5,000 virtual s, 10 segments, 50,923 frames = 6.22 game years, re/frida/scenario_ab.py, trace_format.py meta
q0_64 for QPC past 2^32) as one run, all checkpoints PASS; chains 335 / 335 PASS with no BREAK after 38j; (d) championship/00..02
driven ONLY by semantic commands (38i: input-action kinds lowered to the root-handler calls the raw input makes at its dispatch
point, port/test/semantic.py translate | xlate | gate; all 165 COMMAND segments PASS semantic-only, 10,420 dispatches, 0 gaps);
(e) re/port/divergences.tsv (`integrator ledger`), the one row without a cause is chooseShotTarget.strictFlagSticky, which no
segment or test reaches (U-1012); (f) 8,192 frames take 23 s x64 / 29 s Win32 against the original's 713 s of frame waits, but
a year with no input does not exist in the original (input-gated waits, year-end screen, U-1010). Fixes the merge forced: tick's
HeadlessTerrain arena 1 MB -> 16 MB (cov_b_holes18/00), golden digests re-pinned only through SG_EMU_ORACLE=update after the
oracle was GREEN (clickGolferPanel, showSystemMenu, loadGolferRoster: their callees became registered), test seams that recorded
a peer the port now has (showSystemMenu's promptAndSaveFile, loadGolferRoster's scrollTextWindow via cm::call_peer). Findings:
c3eo.cpp's C4 hook picks the hole-stats rating word with v / 10 where the original uses v / 20 (U-1000; the port follows the
original, the hook needs fixing); mainLoop_structure.md had the title menu's championship / sandbox selections and flags 0x4000000
/ 0x1000000 swapped (corrected, U-1053). HYGIENE: never run `pytest port/test` as one process (it grew to 10-16 GB and exhausted
the 64 GB commit charge, killing siblings' runs with MemoryError): log/p38/suite_by_file.sh runs one file per process; lib.load()
rebuilds port/build/x64 from every .cpp in port/src/sim, so a tree holding a child's unmerged module tests that module too.
Phase 4 .sve round trip (2026-10-21, re/analysis/data/sve_roundtrip.md, port/test/test_sve_roundtrip.py, U-1060 / U-1061):
scenarios cov_r_reload (load mood5, browser-load stories5, play, save key = save_1, browser-load save_1 back, play, save_2; an
Esc before every browser / save key; the record run requires g_date to go back on the reload) and cov_r_sve (no Esc: a SimFoto
wait took the second 'L', so no reload; kept as that wait's regression), family s36a (prefix `cov_r` in scenario_refs.py). Each
run copies the saved-games folder to `<run>/saves/end/` before its end checkpoint; `~/bin/py -3.12 -m port.test.sve_refs
collect <scenario>` checks that every run agrees (9 per scenario did) and copies the files to port/corpus/saves/. The port runs
the one segment of each (979 / 1,047 frames PASS) and reads its files back with the new C ABI call `sg_files_read` (lib.py /
lib32.py `files_read`; `SegmentRunner.run(seg, after=fn)` gives the context after the run): all 10 files the original wrote
have equal body bytes and equal header text up to the NUL, x64 and Win32; the header's 0x64 bytes past the NUL are leftover
g_textBuffer text from drawing the port does not do (reported, not failed, U-1060). Controls RED: saveGame mode 4 (one body
byte; cov_r_sve's segment still PASSES, cov_r_reload's reload reads it back and FAILS), modes 1 / 3, loadGame mode 9,
pumpAndRedraw mode 3. PORT DEFECT FIXED: TraceInput handed a modal dispatch to the first pass of the cursor-less SimFoto wait
(narrateShot's pumpAndRedraw(0)); the original took it on pass 107, after the first pass's flushRedraw had cleared the pending
redraw textInputBox's Enter left, so the port cleared the key too and starved. `InputSource::pace(site)` (commands.h): the
wait reads its clock sites through the source and a Blocking dispatch waits until at most one read at the pass site before it
is unread. The 200 earlier segments keep their digests. g_redrawPending / gDirtyFlagA / gDirtyFlagB are class `render`, which
the segment gate does not compare. Diagnosing a modal-wait slip: `C3FT_CALL_LOG=<va>:<n>:r,...` on a `scenario_refs.py run`
logs the original's main-thread calls with returns (calls.json); SG_TICK_INPUT_LOG takes a FILE NAME, not 1.
Course files (2026-10-22, same write-up): cov_r_course saves stories9's course with the 'C' key (Themes\Championship\<title>.cse
via saveCourse, then flushCourseInfo writes the `[Course]` .srf), quits to the title and plays a championship on that file.
The port now WRITES the .srf (writeCourseInfo in tick_render.inc: the exe's formats from f_strpool_4c7818, text mode CR LF);
.cse body + both header texts, .srf and &QuitSave.sve equal; the Thumb / Full .jpg screenshots are phase 5 (reported). Each
cov_r_* scenario collects the folders at run_a too, and the test seeds the port with the original's files at the segment
start: a load before the segment can write an autosave the install's copy lacks. Controls RED incl. saveCourse mode 1 (the
tournament loads the port's broken file and faults). 15 tests x64 + Win32; gate 203 / 203, earlier digests unchanged.
Phase 4 boot path (2026-10-22, re/PHASE4_BOOT_PLAN.md "Result", port/test/boot.py, port/test/test_boot.py, U-1062..U-1066): the
port runs a whole scenario from process start over the user's exe image alone. C ABI `sg_run_boot` (Win32 runner op 27): the
trace records before the first FRAME are a frame 0 (parse_trace boot flag) that runs port/src/sim/tick_boot.inc (BootCrt,
BootGameMain, BootSetup, then the title menu and session); loadInterfaceArt(1) is the generated table menu_iart.inc
(`~/bin/py -3.12 re/tools/gen_iart.py [--check]` from c3fv.cpp). Gate `~/bin/py -3.12 -m port.test.boot --all --jobs 3 --out
log/p4/boot_gate.tsv` compares every record with log/p2/p24/<scenario>/c_a/framehash.bin: 57 / 60 PASS, cov_a_golfer and
cov_b_holes18 KNOWN (boot.py KNOWN table: the cursor tile only, U-1062), cov_b_location NO_FRAMES; x64 = Win32; segment
digests unchanged. Lessons: a boot run needs the scenario's own install files (scenario.py "files", read by
segment_files.files_for: cov_b_title's Default.pro); instances\p24 still holds a leftover Default.pro (= Gary Golf.pro) from
a cov_b_title run, so never take an instance's directory as the install a capture saw. The captures' frame-hash tracer runs on
the main thread's stack at the frame head, so a function that reads uninitialised stack (screenToTile) can behave differently
with the tracer on: find such cases with a `C3FT_CALL_LOG` run with and without `C3FT_FRAMEHASH=sim`. The call log's second
column is virtual ms since t0, not the trace position (cov_b_title: the trace's first FRAME t 1113321 = 0x100000 + 64745). jgld's window procedure
reads the cursor once per WM_MOUSEMOVE; parse_trace drops those reads (U-1064). Debug recipe for a boot divergence:
record-k sim hashes give the original's value of a one-dword object by brute force over sgh64 (re/tools/state_image.py), and
SG_TICK_DUMP=<callee>;<dir> brackets each call with the port's state.
Phase 4 quit (2026-10-22, re/analysis/data/sve_roundtrip.md "The quit", U-1050 part 2 resolved, U-1067): scenario cov_r_quit (pinned on instances\p4q) ends with the title menu's exit; trace_rec.js ends a trace at ExitProcess (END sub 1, the exiting thread waits for `trec_ack`), an exiting s36a-family lockstep scenario loads c3fya_run's exit support. The port's boot run goes past the last record with boot.run(to_end=True) and writes top10.sve, Default.pro and both saves as 12 runs of the original did (character files compared on their records: sve_refs.compared_bytes). New: __open / __write / __close (Ctx::lowio), showCreditsScreen, drops playAudioFile / PaletteCache::dtor, FontRef +8 = -1. A real key press reached one recording (VK 174, no DELIVER before it) and ended it early: compare a new recording's checkpoint count with the reference's.
Phase 5 step 1, the gfx backend (2026-10-22, re/PHASE5_GFX_PLAN.md, port/test/gfx.py, test_gfx.py, U-1068..U-1070): module `gfx`
(port/src/sim/gfx.cpp) is a software RGB555 renderer owned by the port, attached per context (gfx.attach) like the text provider. With
one attached, TkBase::call hands a drop it has a body for to gfx_drop (the frame keeps the drop's result; nothing is written into the
GameState, so sim hashes and segment digests are unchanged by construction); without one nothing changes. Bodies: showTitleScreen (the
boot now calls it at gameMain+0x41f as a drop), loadImage (8-bit RLE PCX via GameFiles, palette `>> 3` = Palette::apply's RGB555),
Surface_blit (cropSprite with its 0xff trims), Surface_blit3 (1:1), Image_draw (draw16: bytes >= 0xfe transparent), setDrawTarget,
drawCenteredText (plain text: x + (0 - width) / 2, baseline y + the font's +0x10, TextOutA through TextMetrics::draw_glyphs, which
GdiTextMetrics implements on a 16-bit DIB: the original draws its labels with GDI too) and Window::refreshFull (root -> back buffer).
Anything else is recorded as unsupported (gfx.log). GATE: a checkpoint frame is jgld's back buffer at the checkpoint's clock call
(summary.json status.tgt = the index of the trace's tgt call, its ordinal P); gfx.snapshotAt(P) copies the port's back buffer when
the run first consumes a CURSOR record (FrameInput::cursor_pos) or a dispatch past P, into the files overlay `$gfx/snapshot_<k>.555`;
compare with re/tools/imgcmp.py exact_diff per region. sandbox_basic `menu`: 0 of 480,000 px differ (x64 and Win32), controls
wrong blit / wrong palette word / no present RED, `> 0xfe` blind (no 0xfe byte in the title images); segments 205 / 205 and boot
58 / 2 / 1 unchanged.
Phase 5 step 2, the location screen (2026-10-22, re/PHASE5_GFX_PLAN.md section 6, U-1071 / U-1072): all five cov_b_location
checkpoints (refused, site_hover, buttons, tip, back) equal the original's back buffer in every pixel, x64 and Win32. New
bodies: loadInterfaceArt(1)'s image tail (`loadInterfaceArt.images`, called after menu.cpp's table), courseActionMenu's own
Image / Surface array under `gfx_local_key` (a range no GameState address reaches; the draw calls are back in saveload.cpp as
`gx(...)`), Surface_fillRegion, Surface_stretchBlit = jgld bltS -> blitPair1 (the mask blend from the matched asm),
drawText4049d0, drawPanelBackdrop (its Terrain::drawLine bar is OpenGL, not in the back buffer) and drawWindow's
drawFrameR3 + drawMessageBuffer for a dialog without a golfer (blitPanel's 0xf8 bytes shade through build16BitColorTable's
block 1). Port changes: TraceInput applies the harness's memory writes at a modal loop's cursor reads too (cov_b_location's
date write; control tick.frame 7), and a boot trace with no frame head now runs to its end, with idle passes per unread
CURSOR record and the pending snapshots taken when the input runs out (gfx_end_of_input); boot.run(to_end=True) no longer
returns NO_FRAMES before running. Segments 205 / 205 (digests unchanged) and boot 58 / 2 / 1 unchanged.

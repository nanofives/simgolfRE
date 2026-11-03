# S11.4 merge notes: the S10 renames the port waited for (D1 `P` rows)

Worker S11.4 wrote nothing into the shared tree outside log/roadmap/S11/4_*: every change below is applied by the integrator, in
this order, on the tree being merged (`<root>` = the repository, or the integrator's mirror).

## 1. Order

1. Tracker part (re/names, hooks.csv, CHANGELOG) and port / shim / model part in one command:
   `~/bin/py -3.12 log/roadmap/S11/4_assemble.py --root <root> --names`
   - 4_names.py: the name column of the 70 rows of log/roadmap/S11/4_renames.tsv in re/names/*.tsv (13 files), then
     `scripts/re_classify.py --root <root> retag log/roadmap/S11/4_retag.tsv` (68 function rows; levels, callers and callees
     unchanged; one CHANGELOG line per row), then names_validate on the touched ids (expected: bad 0, cross-file duplicates 0).
   - 4_edits.py: 936 anchored edits in 190 files + 2 new files (re/port/corpus_aliases.tsv, port/test/renames.py). The whole
     list is checked first; a missing / ambiguous anchor writes nothing and prints `MISSING edit <k> <file>: <first line>`;
     a second apply is refused.
   - regenerated: re/model/include/simgolf_model.h, re/model/model_ctypes.py, port/include/gamestate.h /
     gamestate_meta.inc / gamestate_addresses.inc, port/include/portsim/sim/soundstate.h; then their --check and
     `model_merge.py merge` == re/model/addresses.tsv.
   After S3 wave 10 merges: first `4_assemble.py --root <root> --check-only`; a MISSING anchor means wave 10 changed that line:
   rerun `~/bin/py -3.12 log/roadmap/S11/4_tools/gen.py --base <the merged tree>` (it regenerates 4_edits.py from that tree with
   the same policy) and apply again. Anchors that sit in files S3 wave 10 owns: section 4.
2. Golden digests: section 3 (re-pinned through the emulator oracle, never by hand).
3. Ghidra master: ApplyNames overwrite on golf_clean.exe and sound.dll with a directory holding ONLY the 13 changed names files
   (CLAUDE.md I16: ApplyNames reads every re/names/*.tsv; use I16's ApplyNamesI16.java for the FID-label clash), backup first:
   ```
   cp -r SimGolf.rep ghidra/backup/SimGolf.rep.pre_s114names_<date>
   mkdir -p log/roadmap/S11/4_apply_names && cp re/names/{exe_0,exe_0_globals,exe_1,exe_3,exe_6,exe_9,model_c_globals,model_h2,model_h3,sound_3,sound_4,sound_6,undisc2_exe}.tsv log/roadmap/S11/4_apply_names/
   <ghidra>/support/analyzeHeadless.bat . SimGolf -process golf_clean.exe -noanalysis -scriptPath ghidra/scripts -postScript ApplyNames.java log/roadmap/S11/4_apply_names overwrite
   <ghidra>/support/analyzeHeadless.bat . SimGolf -process sound.dll -noanalysis -scriptPath ghidra/scripts -postScript ApplyNames.java log/roadmap/S11/4_apply_names overwrite
   ```
   expected: golf_clean.exe 54 function renames + 2 labels (g_vipResidents, g_homeLayouts), sound.dll 16; then re-export
   re/functions_ghidra.tsv and re/functions_ghidra_sound.dll.tsv (ListFunctions.java, `-readOnly`).
4. CLAUDE.md paragraph (section 5) and UNCERTAINTIES.md rows (log/roadmap/S11/4_uncertain.tsv).

## 2. What is renamed and what keeps its spelling

See 4_notes.md section 2. Short form: port C++ / tests / model / shim comments renamed; registry keys, fixture names, SG_HOOK name
tokens, corpus files, pytest ids and the two defect switch ids keep the old spellings; re/port/corpus_aliases.tsv translates the
corpus names where tests read the corpus.

## 3. Golden digests

How the candidates were found: commands.recordPeer (port/src/sim/commands.cpp call_peer) stores FNV-1a of the callee NAME in the
peer log the tests digest, so a pinned digest changes only where a recorded peer was renamed. The port tests that compute fnv1a
(grep `fnv1a(` in port/test: emu38b.py and 13 test files) were searched for the 70 new names as string literals: only
`flushInput` (was enterModal483d30, entry of emu38b.SAVE_PEERS and test_course_menu's own `modal` check) and `drawTextWithColor`
(was drawText4049d0, test_gfx_info_panel's call recorder) occur. The 7 emu38b users plus test_gfx_info_panel:

| file | why |
|---|---|
| test_course_menu | SAVE_PEERS (flushInput) and its own `E.fnv1a("flushInput")` modal check |
| test_saveload, test_browser, test_save_dialogs, test_loaders, test_menu, test_shotstyle | import emu38b (SAVE_PEERS holds flushInput) |
| test_gfx_info_panel | its recorder logs drawTextWithColor |

A file whose default run passes needs nothing (its scenarios record no renamed peer). Not run by S11.4.

Commands, on the merged tree after S3 wave 10 and step 1 (shell variables as log/roadmap/S11/4_suite.sh sets them: SG_PORTSIM_DLL
etc. pointing at that tree's build; one w8_lock hold per file, wrapped in test-slot):
```
# 1. default runs: the 8 candidates and the 5 representative files S11.4 did not reach (test_golfers passed: 60)
FILES="test_course_menu test_saveload test_browser test_save_dialogs test_loaders test_menu test_shotstyle test_gfx_info_panel \
       test_audio_sound test_events test_vectors test_defects_golfers" test-slot bash -c 'bash log/roadmap/S11/4_suite.sh'
# 2. for each file that failed only on a golden digest: oracle + re-pin in one run, one file per hold
~/bin/py -3.12 log/roadmap/S3/w8_lock.py run S11.4 --what "S11.4 repin <file>" -- \
  bash -c 'SG_EMU_ORACLE=update ~/bin/py -3.12 -m pytest port/test/<file>.py -m "" -q -p no:cacheprovider'
# keep the rewritten port/test/golden_*.json only if every oracle test of that run passed (test_oracle_* / *_equals_original);
# otherwise restore the golden file from git and report the oracle failure (it is a behaviour difference, not a rename)
# 3. the default run of the file again: it must pass with the new pins
```
Then the full x64 and Win32 suites (log/p38/suite_by_file.sh pattern, `bash log/roadmap/S11/4_suite.sh` with SG_BACKEND=win32 for
the second) as the integrator's usual check.

## 4. Anchors in files S3 wave 10 edits

From re/roadmap/S3_steps.tsv files_owned of S3.30 / S3.31 / S3.32 (edit numbers of 4_edits.py; each anchor is one or two lines):
- port/src/sim/tick.cpp (S3.30 edits TraceInput): edits 273..290. 278..286 sit in or next to TraceInput (comments naming
  pumpInput483bd0 / frameTick483cd0, the `dispatch(t, PumpKind::Drain, "pumpInput483bd0")` site string and the
  `locals()["pumpInput483bd0"]` table entry); 273..277 and 287..290 are kDrops / negctl / registration lines.
- port/test/segments.py (436..438) and port/test/boot.py (392, 393), which S3.30 may touch "only if the kind needs it": the
  import line, the frames.json symbol line, the checkpoints line and the framehash.json return line.
- port/src/sim/terrain_overlay.cpp/.h (S3.31), tick_boot.inc and the execonst inputs (S3.32), trace_format.py / trace_tool.py /
  trace_rec.js / the two trace schemas (S3.30): no S11.4 edit.
If `--check-only` reports a MISSING anchor there, regenerate the list from the merged tree (`gen.py --base <tree>`) instead of
editing 4_edits.py by hand: the policy is in gen.py / manual.py, and the hunks are recut from whatever the lines now hold.

## 5. CLAUDE.md paragraph (after the last roadmap paragraph)

```
Roadmap S11.4, the S10 renames that waited for the port (2026-11-01, notes log/roadmap/S11/4_notes.md, merge 4_merge.md, U-2030..U-2032):
the 70 `P` rows of S10 (8_merge.md sections 2 / 2b, 10_renames.tsv, 9_renames_sound.tsv minus three already applied) are applied by
log/roadmap/S11/4_assemble.py --root <dir> [--names]: re/names + hooks.csv retag (4_names.py), 936 anchored edits in 190 files of port/,
shim/src (comments only: 84 files, code and strings identical by lexer), re/model (addresses.tsv rows, the stream rows' types, the section
headers) and two new files, then the generated headers rebuilt (simgolf_model.h, model_ctypes.py, gamestate.h / _meta.inc /
_addresses.inc, soundstate.h) and model_merge checked to reproduce addresses.tsv. g_animalRecords -> g_vipResidents (type VipResident),
g_recordBanks -> g_homeLayouts (type SgHomeLayout), spawnObject -> spawnVipResident, placeAnimal -> updateVipResident, tickAnimals ->
updateVipResidents, drawFlyingBall -> drawWalkers, announceBuilding -> openHole, holeMagazineEvent -> rateCourseHoles, rateTile ->
golferShotReach, pumpInput483bd0 -> doAllTasks, frameTick483cd0 -> doMouse, enterModal483d30 -> flushInput, drawText4049d0 ->
drawTextWithColor, the sound placeholders Ch5* / Ctrl5* / Sound5* -> their class names, and the rest of 4_renames.tsv. Keep the old
spellings: registry keys, fixtures, SG_HOOK name tokens (the shim log and scenario CSV stems), pytest ids, the defect switch ids
spawnObject.cellSearchBound / placeAnimal.zeroFrameCount (U-2032) and every corpus file. The corpus is never rewritten:
re/port/corpus_aliases.tsv + port/test/renames.py translate at the readers (vec stems, raw capture keys, the manifest and coverage
tables, a .vec document's `sym` locations in re/tools/vector_lib.read_vec, segment frames.json / checkpoints.json, the captures'
framehash.json); without them every boot row ERRORs and every segment fails its load check. Gates on a copy of I22: builds x64 /
Win32 rc 0, port test_static 6, test_model_header 14, segments 205 / 205 x64 and Win32 with 0 differing cells (digests included),
boot equal to I22's 70 rows on both backends. Golden digests that hash a renamed callee name (commands.recordPeer logs FNV-1a of the
name) are re-pinned through SG_EMU_ORACLE=update after the oracle is GREEN.
```

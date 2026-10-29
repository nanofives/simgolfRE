# S10.6 notes: sound.dll bulk match, converter fixes, the 143 no-size procedures (2026-10-29)

Status: DONE. Gate: `~/bin/py -3.12 -m pytest tests/test_match.py tests/test_match_tools.py tests/test_smsg_import.py -q`
484 passed (log/roadmap/S10/6_match_tier.log).

Process rules followed: file writes of 60 lines or fewer, runs over 2 minutes in the background with logs under
log/roadmap/S10/, `~/bin/py -3.12` only, at most 2 compile jobs at a time, no tracker write (commands in 6_merge.md,
6_promote_cmds.txt), no edit of port/, shim/, original\, the Ghidra master, re/functions_ghidra*.tsv or
re/match/boundaries.tsv. `--evidence` only at the end, on the final canonical files (section 4).

## 1. Converter fixes (re/tools/smsg_import.py, tests/test_smsg_import.py)

1. ICF-folded addresses across files. `our_matches(smsg_before)` also counts the smsg_* canonical files whose NAME sorts
   before the file being written (smsg_dedupe.py's rule: the first smsg file by name holds the address), so a run writes the
   later file's line as `// ALSO 100%, already in <file>` itself. `smsg_dedupe.py` is not needed after a run: on the re-run
   `smsg_dedupe.py smsg_sound_` reports 0 duplicate lines.
2. Error rows. A row with status `error` (no score: `no Ghidra size`) goes to the wip file as
   `// SMSG-ERROR: <module> 0x<va> <pdb name> symbol=<decorated>  <message>`, never as `// MATCH:`. Scored misses keep their
   MATCH line. The 22 wip files with only error rows are now comment-only files in re/match/wip/ (they pass
   test_every_match_annotation_is_a_function_entry); the S10.5 copies in log/roadmap/S10/5_sound_wip/ are superseded.
3. No canonical file with only `// ALSO` lines: `write_outputs` writes a canonical file only when it has a new line and
   deletes a stale one otherwise (the guard existed; it is now covered by a test and the case "stale ALSO-only file" is tested).
Four tests added (11 in the file, all pass): ICF address held by an earlier smsg file, a later file does not hold it, a stale
ALSO-only canonical file is removed, error rows become comments. They were not run against the old code (the old
`our_matches()` took no argument and the old wip writer emitted a MATCH line for every row with a symbol, read from the diff).

Re-run of the sound project (log/roadmap/S10/6_sound_rerun.log, summary after: 6_summary_after_sound.tsv): 34 sources,
new_100 489, already_100 564, miss 0, nosym 0, error 143. The canonical MATCH set is unchanged: 489 lines in 28 files before
and after, `diff` of the sorted lines (6_baseline_match.txt, 6_after_match.txt) is empty.

## 2. The 143 `no Ghidra size` procedures (U-1875)

They are exactly the 143 addresses of S10.9's 9_deferred_not_functions.tsv (143 of 143, none other). None is held by another
re/match file, none has a hooks.csv row. `6_tools/extent_score.py` scores each with its sound.pdb size through a
process-local replacement of `match.boundary_overrides` (nothing written outside the output TSV) and checks the extent
against our disassembly (last instruction, gap to the next listed function, containment in a listed function, whether the
bytes decode to exactly that size). Result (6_extent.tsv): 143 of 143 at 100.0% with SP5 AND with 8168; 0 inside a listed
function; every gap to the next listed entry is larger than the extent; no two extents overlap.
- 140 extents equal the PDB size and end in `ret` / `ret N` or a tail `jmp` (138 `ret`, 2 `jmp`).
- 3 PDB sizes include the switch's jump and index tables after the last `ret` and scored 85.2 / 35.6 / 56.1% (first run,
  6_extent_first.tsv): 0x100250b0 CMMError::handle_error (PDB 172, table at 0x10025108), 0x100392c0
  WaveInDeviceMgr::handle_error (144, table 0x10039314), 0x1002e2f0 Midi::get_tonal_center (160, table 0x1002e354). Their code
  extent ends at the last `ret`: 0x56, 0x53, 0x62 bytes (0x100250b0 + 0x56 = 0x10025106 after `ret 4` at 0x10025103;
  0x100392c0 + 0x53 = 0x10039313 after the `ret 4` at 0x10039310; 0x1002e2f0 + 0x62 = 0x1002e352 after the `ret` at 0x1002e351).
  With those extents all three read 100%.
Lines for the orchestrator (6_merge.md section 1): 6_boundaries_sound.tsv (143 rows in boundaries.tsv format),
6_setbodies_args.txt (SetBodies.java arguments, creates the missing functions with those bodies), then re-run
`smsg_import.py project sound/sound.dll/sound.dsp` (writes the 143 as canonical lines). Staged lines: 6_staged_canonical.tsv.

## 3. lpc10.lib (LPC-10 codec, RTM, debug C)

`smsg_import.py project sound/lpc10.lib/lpc10.dsp` with the project's own flags (`/MT /Od /ZI /GZ /GR- /Zl`, no /GX, RTM 8168;
log/roadmap/S10/6_lpc10_run.log): 37 sources, 40 procedures, all mapped, 40 already 100% in our canonical files (m36c and
earlier), 0 new, 0 miss, 0 error. No misses to analyse; no smsg_sound_lpc10_* file is written (nothing new).

## 4. Evidence, inventory, promote

`match.py re/match/smsg_sound_*.cpp` for the 28 final canonical files wrote 489 CSVs to log/diff (all 100.0%; log 6_evidence.log
holds each file's last two lines; `ls log/diff` 14,102 -> 14,591). `smsg_import.py inventory sound`: 1,238 procedures, our_match
606, smsg_100 1,095, smsg_error 143, c4_candidate 56 (all hooks.csv C3, all SMSG status 100). 6_promote_cmds.txt holds the 56
commands (`re_classify.py promote 0x<rva> --module sound.dll --to C4 --evidence log/diff/<rva>_sound_<name>.match.csv`);
`re_classify.py check` on all 56 returned `C3 -> C4: all gates pass` (6_check_all.log; hooks.csv untouched).

Toolchain per new 100% (6_tc_canonical.tsv, 489 lines, 3 compilers): 488 are 100% with 8168 and SP5; only
0x10010920 CDSB::get_emulated_write_info needs `// TOOLCHAIN sp5` (8168 99.4%, SP5 100.0%), as S10.5 found; it is C2 dead,
so it has no promote line. The 143 extent rows are 100% under both. Every smsg_sound_*.cpp file carries `// TOOLCHAIN sp5`
because it is the project's toolchain; for the 488 the line changes no score.

## Reproduce

```
~/bin/py -3.12 re/tools/smsg_import.py project sound/sound.dll/sound.dsp      # canonical set unchanged
~/bin/py -3.12 re/tools/smsg_import.py project sound/lpc10.lib/lpc10.dsp
~/bin/py -3.12 log/roadmap/S10/6_tools/extent_score.py --out log/roadmap/S10/6_extent.tsv
~/bin/py -3.12 log/roadmap/S10/6_tools/stage_lines.py
~/bin/py -3.12 log/roadmap/S10/5_tools/nearmiss.py --glob 'smsg_sound_*.cpp' --out log/roadmap/S10/6_tc_canonical.tsv
~/bin/py -3.12 re/tools/smsg_import.py inventory sound; ~/bin/py -3.12 log/roadmap/S10/6_tools/promote_cmds_sound.py
```

## Open

U-1875 (143 bodies not in the function list: boundaries rows and SetBodies arguments pending), U-1876 (holder rule by file
name differs from smsg_dedupe.py's PROTECT list for the golf files), U-1877 (the 143 have no names or hooks.csv rows, so
they cannot reach C4 until named). Controls: the four new converter tests are the negative cases; the extent run is its own
control (the three table-inclusive extents read 85.2 / 35.6 / 56.1%, the wrong extent changes the score).

## 5. tests/test_match.py: skip when a file's toolchain is absent (lead's addition)

`test_every_matched_source_is_100_percent` reads the file's `// TOOLCHAIN` through `match.file_toolchain` and skips with
`// TOOLCHAIN <id> not present: no cl.exe in <dir> (tools/ is local, not committed)` when `TOOLCHAINS[id]["bin"]/cl.exe` is
missing (tools/vc6sp5 is gitignored), so the tier passes on a PC without SP5. Nothing else changed. With SP5 present the
test runs as before (test_match.py: 466 passed, log 6_match_tier_skip.log). The skip condition was checked by pointing the sp5
bin directory at a missing path: the cl.exe test is False for smsg_sound_dsb.cpp.

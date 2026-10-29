# S10.9 notes: names from sound.pdb / jgld.pdb / Terrain.pdb where we have none (2026-10-29)

Status: DONE. The merge items are in 9_merge.md and every file written is in 9_files.txt.

## Counts (re-measured; the investigation's 535 sound / 57 jgld counted differently)
| module | candidates | accepted | rejected | deferred (not a master function) |
|---|---|---|---|---|
| sound.dll | 561 | 268 | 150 | 143 |
| jgld.dll | 120 (63 game + 57 library) | 76 (22 game + 54 CRT) | 43 (41 game + 2 library) | 1 |
| Terrain.dll | 62 (all library) | 60 | 1 | 1 |
| total | 743 | 404 | 194 | 145 |

The 194 rejections break down as follows, counted from the ledgers' reason column:
- 152 stubs (113 mechanical, 39 by hand): constant, empty or argument-returning bodies, so no instruction supports the
  verb.
- 13 bodies not reviewed in depth: no import or named callee supports the verb.
- 8 forwarders whose target slot holds no body with the same name.
- 6 bodies whose few stores or calls do not support the verb (MidiCtrl::refresh, Dll_Wave_In_Device::update_sound,
  CVoiceRxMgr::unload, CVoiceTxMgr::stop, CWaveMgr::set_fade, and Dll_Wave_Device::timer_unhook, U-1892).
- 5 set_total_class_size constants with no allocation of that size for the class.
- 3 unreferenced functions.
- 2 boundary mismatches (U-1890).
- 2 jgld timer-message functions (U-1893).
- 3 library misses: _CrtCheckMemory twice (U-1891) and zlibVersion.

Ledgers, one row per candidate: 9_ledger_sound.tsv, 9_ledger_jgld.tsv (game rows), 9_libledger_jgld.tsv and
9_libledger_Terrain.tsv. In the hand ledgers the name column is the pre-classmap name.

There are also 19 rename proposals for existing placeholder ctors and dtors (9_renames_sound.tsv), plus 20 boundaries
rows (9_boundaries.tsv).

## Method (reproduce with `~/bin/py -3.12`)
1. `log/roadmap/S10/9_tools/measure.py <sound|jgld|Terrain> --out log/roadmap/S10/9_cand_<k>.tsv`. A candidate is an
   SMSG procs row whose address has no names-file row and a `FUN_` name in the master, or no master function at all.
2. `9_tools/collect.py <k>` writes 9_facts_<k>.json, facts from our binary only:
   - the capstone body, imports, strings, and callers / callees under our names;
   - vtable slots with their installers (re/tools/pointer_callers.py).
3. `9_tools/verdict.py <k>` writes 9_verdict_<k>.tsv. It uses shape.py's body kinds and vtslots.py's slot-name index.
   The rules are in re/analysis/systems/smsg_sound_1.md. sound.dll's release code fits the mechanical rules; jgld.dll's
   debug code does not, so it is all hand verdicts.
4. Hand verdicts, one per MANUAL row: 9_manual_sound.tsv, 9_manual_sound_2..4.tsv and 9_manual_jgld.tsv.
5. `9_tools/build_rows.py <k> <id> <subsystem>` writes the names file.
   - Class prefixes go through `9_tools/classmap.py`: an SMSG class takes our established prefix when our rows have a
     dominant one for it (U-1894). Without this, our `Wave_Device` / `Midi_Device` / `Sound` rows and SMSG's classes of
     the same names would have pointed at different vtables.
   - SMSG duplicate names, and names our files already use, get an address suffix.
   - Mentions of renamed new rows inside the evidence text are rewritten.
6. Library rows:
   - `9_tools/libid.py <jgld|Terrain>` runs log/c3/m36e_lib_match.py against SP2's libcmtd.lib, RTM's LIBCMTD.LIB and
     LIBCIMTD.LIB. The CSVs go to 9_lib/<k>/lib0..2/, never log/diff.
   - `9_tools/libid_extent.py` repeats a miss with SMSG's extent (a process-local boundary override), writing to
     9_lib/<k>/ext_sp2/.
   - `9_tools/build_lib_rows.py <k> <id>` writes the rows.
   - Windows paths are case-insensitive: an earlier ext run for LIBCMTD.LIB overwrote the libcmtd.lib CSVs in the same
     folder, so the ext runs now use SP2 only.
7. `9_tools/renames.py <k>` writes 9_renames_<k>.tsv.
8. Checks:
   - `9_tools/check_cites.py`: every address cited inside a row's own body must start an instruction. It caught 4 wrong
     hand-written addresses, now fixed.
   - names_sanitize, names_validate and names_check_evidence (commands in 9_merge.md).

## Findings
- SMSG's jgld JGLSys vtable 0x1011d640 is our Display. Its slots 72..84 are a message pump, and each slot is named by
  its PeekMessageA filter range. The slot pumpMessages calls (+0x13c) is do_mouse (0x200..0x20a).
- jgld.dll's do_timer / flush_timer peek 0x401, and nothing in jgld posts it (U-1893).
- sound.dll CMidiMgr's trackset slots queue MIDI commands 7 / 8 / 9 / 0xa. Those are the ids that our Seq_cmdHandler7..10
  handle.
- Of SMSG's set_total_class_size constants, two match allocation sizes in our code:
  - NoteMap 0xe4: Wave_Device::readFmtChunk 0x100054be;
  - TrackCtrl 0x34: loadSoundFile 0x10004553.
  The other set_total_class_size rows were rejected.
- Ten debug-heap CRT routines in jgld and Terrain are cut short in our function lists, with 11-byte false starts inside
  them (9_boundaries.tsv).

## Not done
- No Ghidra was used. Nothing was written to the tracker or the master. The 145 non-function addresses are deferred.
- About 30 large sound bodies were rejected as not reviewed, so a deeper pass can still name them (reasons are in the
  ledger).
- No `_globals.tsv` was written.

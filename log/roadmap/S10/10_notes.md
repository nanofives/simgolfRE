# S10.10 notes: golf.pdb names for the 204 generic exe method rows and the held forwarders (2026-10-29, claude2 worker)

Status: DONE. RE names work only. Nothing was written to port/, shim/, original\, the Ghidra master, the tracker or
re/names. Every claim rests on our own disassembly: capstone over original/golf_clean.exe and sound.dll / jgld.dll,
re/tools/xref.py and re/tools/pointer_callers.py. golf.pdb, sound.pdb and jgld.pdb names were leads only.

Process rules from the brief: every file write is 60 lines or fewer (longer files are written in appends), and no tool
call was refused.

## 1. Counts

Scope:
- The 204 rows are the 214 GENERIC method rows of 8_methods.tsv minus the 10 NodeList rows S10.8 renamed.
- They include all 16 held forwarders: 13 NodeList, 2 NodeListC and 1 NodeListB rows whose sound.dll target was
  unnamed. They also include the 2 that reach constant stubs.
- No address of the set lacks a names row, so no smsg_exe_2.tsv was written.

| rule | rows | verdict |
|---|---|---|
| ACCEPT_FWD | 17 | forwarder: every body reached through this+0x40 / this+0x14 has our names row with golf.pdb's method |
| ACCEPT_SLOT | 9 | Window slot 29..39 dispatcher; the root window's handler in the same slot has a measured message |
| ACCEPT_THUNK | 9 | vtordisp thunk of an accepted body |
| ACCEPT_PUMP | 7 | input pump whose jgld Display slot S10.9 named by its PeekMessageA range |
| ACCEPT_HAND | 5 | hand verdict on the instructions (drawTextWithColor, calcTileWallMask, propagateMinTileLevel, cornerHeightAt, createSound) |
| ACCEPT_SET | 3 | S10.9's ACCEPT_SET (vtable-pinned setter that stores its arguments, no call) |
| accepted | 50 | renames, log/roadmap/S10/10_merge.md section 2 |
| REJECT_FWD_CONFLICT | 22 | our sound.dll name of the reached slot differs from golf.pdb's verb (13 mechanical + 9 store-and-forward by hand), U-1896 |
| REJECT_THUNK | 21 | thunk whose target keeps its name |
| REJECT_KEEP_SAME | 20 | our row already states the act; golf.pdb adds only an unsupported class |
| REJECT_ICF | 17 | 2 to 30 golf.pdb names at one address (U-1850): a neutral name stays |
| REJECT_SLOT_UNMEASURED | 17 | slot 6 / 7 / 26 / 27 / 28 / 57 / 58 / 60 or event entry with no measured message, U-1895 |
| REJECT_FWD_UNNAMED | 16 | reached body without a names row: 8 sound.dll constant stubs and 7 forwarders / unreviewed bodies (S10.9 ledger reasons), 1 MultiSound slot 1 |
| REJECT_KEEP_WRAP | 8 | wrapper our row names after its callee |
| REJECT_STUB | 7 | constant or empty body, or `mov eax, ecx; ret` |
| REJECT_NOUN_CONFLICT | 7 | toolbar hit-tests: golf.pdb's panel nouns do not follow our drawers, U-1895 |
| REJECT_CHILD_AXIS | 6 | vert / horz child at +0x26c / +0x270 not established, U-1897 |
| REJECT_POLY | 4 | DC_call*: the jgld class at this+4 is not pinned, U-1897 |
| REJECT_VERB | 3 | the stores and calls do not support init_class / close_class / close |
| REJECT_TARGET_UNNAMED | 3 | display slot body unnamed (do_non_input) or rejected (do_timer / flush_timer, U-1893) |
| REJECT_FWD_STUB | 2 | stubReturnBA / stubReturnBB (S10.8) |
| REJECT_NOT_REVIEWED | 1 | drawPanel4810d0, 1,668 bytes |
| rejected | 154 | |

The 16 held forwarders: 7 accepted (NodeList::fwdChild6 / 15 / 16 / 27 / 30 / 34 and NodeListB::fwdChild24, whose
targets S10.9 named as Wave_Device / Midi_Device get_ndevices, set_rate, get_rate, is_disabled, is_eax, is_3d,
is_disabled) and 9 rejected:
- fwdChild31, 36, 37, 38, 39, 40 and 42, and NodeListC::fwdChild26, reach `mov eax, 0xb` / `xor eax, eax` stubs that
  S10.9 rejected;
- NodeListC::fwdChild25 reaches a forwarder S10.9 rejected.
Of the Snd / VoiceRx forwarders (U-1889), 10 were accepted and the rest rejected by the rules above.

Outside the 204, two contradictions turned up: showScreen 0x00483bb0 = doTask, redrawAll 0x00483c70 = doKey
(10_merge.md section 3).

## 2. Method and findings

- The exe Snd / VoiceRx object at this+0x40 is what createSound (0x004840e0) gets from sound.dll's export ordinal 1,
  create_sound 0x100093e0 (slot 0x0083af6c filled by soundVersionCheck, ordinal edi + 1 at 0x00483ffd).
  - create_sound switches on type - 1 (0x1000946d..0x10009477, table 0x100096e0, cases from
    10_tools/sndclasses.py).
  - Every exe Snd caller passes type 1 (0x00484319, 0x00484c33, 0x00484c96, 0x00484d7d, 0x00484e88). Case 0 builds
    MultiSound: ctor 0x10039790, vtable 0x1005c7e8 stored at 0x100397ca.
  - VoiceRx::start passes 6 (0x00485460). Case 5 builds the 0x1005c450 class (ctor 0x10030080, store 0x100300b9).
  - The out pointer is this+0x40 at every call (`lea edi, [esi+0x40]` 0x00484e81, 0x00484c28; `lea esi, [ebx+0x40]`
    0x00485457).
  - Forwarders installed in VoiceRx tables (0x004badf8 / 0x004bbc3c) therefore reach 0x1005c450; all others reach
    0x1005c7e8. A forwarder in both kinds of table is accepted only when both bodies agree.
- Slot rule. Table 0x004ba2fc (root window) slots 29..39 hold rootWin_onPress1 .. onPress2b
  (10_tools/rootslots.py). Their messages are measured: WM_LBUTTONUP / DOWN / UP, the right button, WM_KEYDOWN / UP
  and the double clicks (time_model.md, S7.4).
  - golf.pdb gives the same on_* name to slot k of both the Screen and the Dialogs class.
  - The composite view table is 0x004bb8d0. pointer_callers reports the slot of the vtordisp thunks against
    0x004bb904, because a stray reference shifts its table start; slot k sits at 0x004bb8d0 + 4k, for example
    0x004bb944 for slot 29.
- Pumps. jgld Display slots +0x128 / +0x134 / +0x13c / +0x148 / +0x14c are do_task / do_key / do_mouse /
  flush_keyboard / flush_mouse in smsg_jgld_1.tsv (S10.9, PeekMessageA ranges); 10_tools/jgldslots.py lists them.
- Findings in our names:
  - relaxEdges42f530 writes no height. It computes g_tileWallMask.
  - modalPush483d40 / modalFocusClear483d60 / enterModal483d30 flush the keyboard and mouse queues.
  - showScreen / redrawAll are message pumps.
  - Our MultiSound slot names conflict with sound.pdb at 19 slots (U-1896: 17 forwarder slots plus slots 34 / 35 of applyMode484e30). The 0x1005c450 class has one conflict
    (slot 35: Sound5c450::openMmio against add_buffer).

## 3. Reproduce

```
~/bin/py -3.12 log/roadmap/S10/10_tools/collect_exe.py    # -> 10_facts_exe.json (about 3 min; log 10_collect.log)
~/bin/py -3.12 log/roadmap/S10/10_tools/fwd2.py           # -> 10_fwd.tsv (46 forwarders resolved)
~/bin/py -3.12 log/roadmap/S10/10_tools/verdict_exe.py    # -> 10_verdict_exe.tsv (reads 10_manual_exe.tsv, 9_ledger_sound.tsv)
~/bin/py -3.12 log/roadmap/S10/10_tools/portcol.py        # -> 10_renames.tsv (port column)
```
Helpers: rows.py (names rows and purposes, 10_rows_purpose.txt), sndclasses.py, sndslots.py, snddis.py, sgdis.py,
rootslots.py, slotof.py, jgldslots.py. Copied from S10.9 / S10.8 without edits: shape.py and the other 9_tools files,
and fwd.py.

Gate: no new names file, so names_sanitize / names_validate / names_check_evidence have nothing to run on. The
renames are checked for uniqueness against every golf_clean.exe row (verdict_exe.py appends the address on a clash:
Snd::play, Snd::isPlaying, Snd::getTime x2).

## 4. Open items

- U-1895..U-1897 (10_uncertain.tsv).
- The 50 renames and the 2 in section 3: 40 + 2 are P rows, because their old names are port / shim / frida keys. The
  lead decides between port aliases and renaming re/names + Ghidra only.
- U-1889 is partial: the class constructors, destructors and `$E` initializers stay as S10.8 left them.
- Fixing U-1896 on the sound side would unlock up to 22 more Snd / VoiceRx forwarders through a re-run of fwd2.py and
  verdict_exe.py.

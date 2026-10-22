# Phase 4: the .sve round trip through the port (2026-10-21)

Question (re/PORT_PLAN.md "Next steps"): does the port write the same saved games as the original when both play the same
input from the same state, and does it play on from a file it wrote itself?

## Method

1. **Scenarios** (re/tools/scenario.py, family s36a: the saved games are restored before every boot).
   - `cov_r_reload`: title menu, load mood5.sve; 20 s; the in-game browser ('L') loads stories5.sve; 20 s; the save key
     ('S', Enter accepts the proposed `<course> <day> <date>` name) writes save_1 (`Ocean's Edge 10 September 2001.sve`);
     10 s; the browser loads save_1 back; 20 s; save key again (save_2, `... 21 September 2001.sve`). An Esc goes before
     every browser and save key. The record run reads g_date 0x00834170 before and after the reload and stops unless it
     went back (0x19f1 -> 0x19d8; save_1 was made at 0x193c).
   - `cov_r_sve`: the same without the Esc. As recorded, a SimFoto wait began 107 passes before the second 'L' and took it
     as its key, so the browser never opened and save_2 (`... 25 September 2001.sve`) continues the game. It stays as the
     regression for that wait (section 3).
   - Before the end checkpoint each run copies the saved-games folder into `<run>/saves/end/` with sha256s.
2. **References.** Both pinned (`scenario_refs.py regen`, det_b == det_c), segment corpus built (`segment_corpus.py
   pipeline`): one accepted COMMAND segment each, run_a -> end (cov_r_sve 979 frames, cov_r_reload 1,047). Every run of
   the original wrote the same eight files byte for byte: record, det_b, det_c, the three captures, the trace run and
   both restarts, 9 runs per scenario (`port/test/sve_refs.py collect`, copied to port/corpus/saves/<scenario>/end/).
3. **Port.** SegmentRunner runs the segment from its start image with the recorded input (the user's install as the
   game files, writes in the in-memory overlay), then reads every file back through the new C ABI call `sg_files_read`
   (x64 and the Win32 runner, op 26).

## Result (port/test/test_sve_roundtrip.py, x64 and Win32: 10 passed)

| scenario | segment | files the original wrote | body bytes equal | header text equal | header residue differs |
|---|---|---|---|---|---|
| cov_r_sve | 979 frames PASS | 5 (save_1, save_2, While_Browsing, 2 autosaves) | 5 / 5 | 5 / 5 | 4 (2 to 57 bytes) |
| cov_r_reload | 1,047 frames PASS | 5 | 5 / 5 | 5 / 5 | 5 (2 to 57 bytes) |

The three install saves the original left unchanged are unchanged in the port. In cov_r_reload the port loads its own
save_1 (written to the overlay in the same run) and every later frame and save_2 still match the original.

**Header residue (U-1060).** saveGame 0x0040b4a0 writes all 0x64 bytes of g_textBuffer as the header line. The bytes
after the header text's NUL are the buffer's leftovers: in the original, text composed for drawing (news ticker lines,
wrapMessageBuffer's newlines), which the port does not draw. The test compares the text and its NUL exactly and reports
the residue. Whether a reader of a loaded header uses the bytes past the NUL is not checked.

## Negative controls (all RED)

| control | effect |
|---|---|
| saveGame mode 4: body byte 0xf9c of the file flipped, state untouched | cov_r_sve: segment PASS, bytes RED (only the byte comparison sees it); cov_r_reload: the reload reads the flipped byte back, segment FAIL at record 579 (g_holes) |
| saveGame mode 1: no date in the header text | RED |
| saveGame mode 3: a 0x63-byte header | RED (every body byte shifts) |
| loadGame mode 9: third body field skipped on load | RED |
| pumpAndRedraw mode 3: the SimFoto wait not paced | RED (cov_r_sve stops in narrateShot, the saves are never made) |

## Port defect found and fixed: the SimFoto wait's pass (U-1061)

cov_r_sve first stopped at record 474 with `missing: pumpAndRedraw; stopped in scanShotArea < updateGolfers`.
updateGolfers -> scanShotArea 0x00409620 -> narrateShot 0x00407e00 -> pumpAndRedraw(0) at 0x0040946a, the SimFoto wait. A
main-thread call log of the original (`C3FT_CALL_LOG=45ae70:0:r,45ae50:0:r,45ae30:0:r,45c0c0:1:r`,
log/c3/alt/refs/cov_r_sve/calllog1) showed:

- textInputBox's flushRedraw 0x0045b518 returned 1 on the save's Enter, leaving g_redrawPending set;
- the wait ran 107 passes: 107 reads at 0x0045c0e5, flushRedraw at 0x0045c0f5 returning 0 106 times (the first one
  clearing the pending redraw) and 1 once, when the 'L' had been dispatched.

The port's TraceInput handed the 'L' to the first pass, whose flush_redraw cleared the pending redraw and the new key
together, so the wait asked for more input and none was left. Fix: `InputSource::pace(site)` (portsim/sim/commands.h).
pumpAndRedraw reads its two clock sites through the source (start 0x4c49d, per pass 0x4c4b6, offsets from the frame-head
return address), and TraceInput holds a Blocking dispatch while more than one of the frame's reads at the pass site
before it is unread. The dispatch now reaches pass 107. All 200 earlier segments keep their digests on x64
(log/p4/segs_x64.tsv against log/p38/38i/raw_after2.tsv) and x64 == Win32 on all 201.

g_redrawPending, gDirtyFlagA and gDirtyFlagB are class `render`, so the segment gate does not compare them. A wrong pass
shows only where a later wait depends on them, as here.

## Files

port/test/test_sve_roundtrip.py, port/test/sve_refs.py; `sg_files_read` (port/include/portsim_c.h, port/src/portsim_c.cpp,
port/test/lib.py, lib32.py, runner32/runner32.cpp); `SegmentRunner.run(seg, after=...)` (port/test/segments.py);
InputSource::pace (commands.h, tick.cpp TraceInput, commands.cpp pump_and_redraw); saveGame mode 4 and pumpAndRedraw mode
3 controls; scenarios cov_r_sve / cov_r_reload (re/tools/scenario.py, family prefix `cov_r` in re/tools/scenario_refs.py);
re/analysis/data/scenario_refs.json, re/port/segments_manifest.tsv. Local: port/corpus/saves/, log/p4/.

## Not done here

Course files (saveCourse / loadCourse round trip through a championship course) and the asset manifest of the full Setup
path (loadInterfaceArt(full != 0), U-1050) are the rest of phase 4.

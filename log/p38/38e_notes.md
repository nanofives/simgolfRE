# Stream 38e: clock-aware InputSource (U-0949) and showHoleStatsScreen (2026-10-20)

## Result

- `InputSource::clock(site, t)` (commands.h) + the trace-driven implementation in tick.cpp's TraceInput, written as a merge
  request (log/p38/38e_merge.md; patches log/p38/38e/commands_h.patch, tick_cpp.patch; patched copies under
  log/p38/38e/shared/). Default `false`, so every other source and every existing loop is unaffected.
- showHoleStatsScreen ported in module `holestats` (port/src/sim/holestats.cpp, portsim/sim/holestats.h).
- Segments, all 190 accepted, before (HEAD) and after (HEAD + this stream), on port/build/m38e:
  - before: 144 PASS (35 / 35 TIME-FORWARD, 109 / 155 COMMAND), 46 FAIL (log/p38/38e/segs_before.tsv; cov_a_champ/00 hit a
    MemoryError in the 8-job run and PASSed alone, segs_before_champ00.tsv)
  - after: 146 PASS (35 / 35, 111 / 155), 44 FAIL (segs_final.tsv). Changed: championship/02 FAIL 335/394 -> PASS 394/394,
    cov_a_keys2/02 FAIL 2/254 -> PASS 254/254. The other 188: identical status, frames and run_detail text.
- championship/02 now ends `end checkpoint at site 312244 ... ran to a Blocking pump with no input left (pumpMessages);
  stopped in pumpMessages < showTournamentResults` (the segment's end checkpoint inside showTournamentResults' modal wait,
  as the other PASSing modal ends) with `clock queries 457 served, 0 none left, 0 dispatches late`. cov_a_keys2/02 ends at
  the frame wait with `clock queries 385 served, 0 none left, 0 dispatches late`. 457 / 385 = every timeGetTime record of the
  traces at the screen's five sites (entry 2 / 2, after-draw 8 / 7, pass 441 / 371, new-page 6 / 5, banner 0 / 0): the port
  made exactly the original's page reads, in order. (The parent's 842 counts the waitTicks reads of the loop as well; the
  port drops waitTicks.)

## The clock query

- Site = the clock call's return address minus mainLoop's frame-head return address 0x0040fc2f (the trace's key; tick.cpp
  kSiteFrameMs / kSiteWaitTicks use the same). showHoleStatsScreen's: entry 0x00453368 (call 0x00453362), banner 0x00454239
  (0x00454233), after-draw 0x0045457f (`call ebp` 0x0045457d), pass 0x0045464c (0x0045464a), new-page 0x00454661
  (0x0045465f); offsets 0x43739 0x4460a 0x44950 0x44a1d 0x44a32 (holestats.h).
- TraceInput: forward scan from the record after the last one returned, reset per frame. A loop that reads a site the
  original did not read at that point takes a later record or none (`none left`, then NeedInput where the loop waits), so a
  wrong read count shows as a stop, not a silent drift (negative control 1 below).
- Dispatch alignment is unchanged (cursor counts, 3.7a's rule); the new `dispatches late` counter checks the clock side:
  0 in both segments.
- Test sources: `QueueInput::set_clock` (per-site script) and holestats' TimedInput (test entries holestats.setScript /
  consumed / cursorReads / clearScript): batches arrive by clock value (a Blocking pump delivers batch k once the last
  clock value read is >= arrive[k]); cmdemu `run(clock=, gate=)` gives the original the same script and the same rule.

## showHoleStatsScreen (0x00453330, 4,961 B, C4, hook c3eo.cpp)

- Kept: every page build's g_textBuffer text and g_ui_821c50 itoa scratch, the peers appendHoleName / appendCents /
  appendRatingLabel / buildGolferComment(kind, word & 0x3fff, hole * 11, 0x98), the root handlers of the input its pumps
  take, g_msgState53df54 = 0 (0x00454677), pumpLight (0x00454681). The integer divisions of the page values (#DE where the
  original's idiv faults: DivideError).
- Dropped (log/p3/p37a_simwrites.py: no sim write, no sim address taken in the closure): Surface_drawSprite 0x474440,
  Surface_stretchBlit 0x473f60, Widget_setQuad 0x476310, drawStringAt 0x476700, Widget_draw 0x476650, Image_draw 0x473e60,
  Window::refreshFull 0x480c80, waitTicks 0x45bf80, enterModal483d30 0x483d30. The cursor read of each View_toLocal
  (0x00454560 / 0x00454645) is made when g_graphicsDevice != 0 (as tick's View_toLocal), its value is not used (hover image).
- The pump of the pass is a Blocking pump that does not throw on an exhausted source: the loop keeps passing on the clock;
  the clock query ending is the stop.
- HOOK DEFECT FOUND (U-1000): the rating word is chosen by v / 20 (`imul 0x66666667` 0x00453ebe, `sar edx, 3` 0x00453ec0),
  the hook c3eo.cpp uses v / 10. The port follows the instructions. The c3eo row is C4; its A/B never compared the word.

## Evidence

- Emulator oracle (SG_EMU_ORACLE=1, port/test/test_holestats.py): 300 cases with buildGolferComment recorded on both sides
  (call sequences compared, peer log cap 2,048) and 120 with the real buildGolferComment (golfer records zeroed, U-1001):
  all GREEN (whole data image, error, consumed input, peer calls). Cases end by input or by the clock script running out
  (NeedInput on both sides, images compared there too).
- Branch coverage of the original's body: 43 / 76 jcc both sides; the other 33 are listed in the test (UNREACHABLE): 31
  `test eax, eax` on a fixed data address (the VC6 `p ? p : 0` idiom), `page > 4` after an unsigned modulo (0x00453606),
  `lo > lo + 5` (0x00453b33), and the hover's left-the-box arm 0x004545e0 (fixed cursor per case; drawing only).
- Golden digests: port/test/golden_holestats.json, 80 cases, pinned with SG_EMU_ORACLE=update after the oracle was GREEN.
- test_clock_reads_follow_the_script_site_by_site: a constructed case with three page changes gives four builds of five
  comments (20 buildGolferComment calls) and consumes one command; equal to the original under the oracle.
- Negative controls (g_negctl_holestats): 1 = page from clock bit 9, 2 = g_msgState53df54 not cleared, 3 = the histogram's
  last row stops at 8. All three change the golden digests and read RED against the original (log/p38/38e/negctl_run.log).
  On championship/02 / cov_a_keys2/02 (log/p38/38e/seg_negctl.log): mode 1 stops in showHoleStatsScreen (its clock queries
  run out), mode 2 stops in the next frame (the screen reopens with no clock records), mode 3 PASSes (g_textBuffer is
  hidden class, not hashed; the oracle reads it).

## Files

log/p38/38e_files.txt. Build: `sh log/p38/38e/build.sh` (mirror log/p38/38e/mirror.py: HEAD + own files + the patched
shared copies + `SG_MODULE(holestats)`, x64 into port/build/m38e, copied to port/build/m38e_run). Segment runs:
`SG_PORTSIM_DLL=$PWD/port/build/m38e_seg/portsim_c.dll ~/bin/py -3.12 log/p3/p37a_cmdsegs.py --cls all --out <tsv>`.

## Open

- U-1000 (fix c3eo.cpp's tier, re-run its A/B, decide its C4), U-1001 (3.5 buildGolferComment over random golfer records).
- Not done: a Win32 run (the screen has no x87 code), re/port/commands_split.tsv row update (merge request section 5).

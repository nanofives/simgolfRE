# P4B notes: memory-overrun defect switches (G30)

Tree port/build/P4B (HEAD 7f7860c5). Every switch is an `int`, default 1 = the original, 0 = the fix. Build x64 and Win32 rc 0
(P4B_build_x64.log, P4B_build_win32.log). Segments gate x64 under the default switches: 205 / 205 PASS, name / status / digest columns equal
to log/roadmap/merges/I25/segs_x64.tsv on every row (P4B_segs_x64.tsv, P4B_segments.log). No per-switch proof tests were written.

## Done (6 of 8 rows are switches in the tree)

| row | switch | where | fix at 0 |
|---|---|---|---|
| D11 | spawnWalker.fullTable | port/src/sim/golfers.cpp spawn_walker (exe 0x00402970); callers terrain_gen.cpp (spawnWalker(-6)) and tick_leaves.inc hireEmployee guard a -1 result | all 64 records live: nothing is initialised and the function returns -1 (callers skip their writes) |
| D12 | ballScreenList.overflow | tick_frame.inc, mainLoop stores at 0x004157c0 / 0x004157c7 (ML+0x6200..0x620e); switch defined in terrain_course.cpp | the append (both stores and the count increment) is skipped once g_nameLabelCount is 32 or more |
| D21 | cornerLevel.unclampedAndShared | terrain_course.cpp step_course_edit_queue (0x00409e9c / 0x00409eb4); tick_frame.inc key-0x58 reset (0x0041ca44..0x0041ca74, new store at ML+0xd499 + 0x32) | inc stops at 0xff, dec stops at 0 (no wrap); the reset also writes byte 50 of each row (value 4) |
| D47b | loadStoryStrings.cacheEntryLimit | events_story.inc, the strcpy at 0x004670a1..0x004670cf | copy at most 0xff characters plus the NUL into the 0x100-byte entry text field |
| D48a | buildGolferComment.slotLimit | events_think.inc (end of build_golfer_comment) | the own-name text is cut to 0x37 characters (slot 0x38) and the third text to 0x7f (slot 0x80) |
| D49a | postEvent.idBound | events.cpp post_event (0x0046e7b0) | an id of 22 or more (unsigned compare, so negative too) returns without writing |

Notes on honesty of the default state:
- D48a: the port keeps these texts in std::vector (events_think.inc), so it never reproduced the original's stack overrun. At 1 the port
  behaves as before (no overrun); the switch at 0 only adds the truncation the original's slots imply. The slot for the saved text
  (esp + 0xdc) has no measured size and is not cut. The row's address-to-slot mapping (0x0046c4cf -> 0x38, 0x0046c6bf -> 0x80) is taken from the row text, not re-read.
- D21: the brief said "clamp to the level range the code uses elsewhere"; no explicit range is documented (U-0209 is open), so the fix clamps to the byte
  range, which only removes the wrap. The shared byte x * 51 + 50 (vertex (x, 50) and (x + 1, -1)) is not separated.
- D12: the read side (0x00418b4d..0x00418bbd, terrain.cpp-independent code in tick_frame.inc) uses g_nameLabelCount as the bound, so it needs no change.

## Not done: D5 and D6
D5 (28 table reads in mainLoop 0x00411611..0x00412838 plus scanTileLine, trialShot, updateGolfers) and D6 (heightBlend, canStep,
generateRandomCourse, floodMark: floodMark also writes outside tile_flags) each touch many converted sites in several modules, and each site
needs its own "outside cell is absent" meaning (the original reads a defined adjacent-table byte). I did not guess these; no switch exists for
grid.edgeReadsNeighbourTable or grid.rowPastEnd. They need a per-site pass.

## Files
See P4B_files.txt. Profile rows: P4B_profile.tsv.

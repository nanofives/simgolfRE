# Golfer membership (join, upgrade, resign) and why arrivals stall

Status: mapped from decompilation + runtime (2026-10-04). Names below are descriptive, not original symbols.

## Data
- Golfer-type table at `0x5849e0`, stride `0x2c`, indexed by the golfer's type (short at golfer +0xb6, 1..75):
  - `+2` byte: membership level in the low 3 bits (0 = not a member). Strings near FUN_00406670: "Member",
    "Silver Member", "Gold Member", "Platinum Member" (0x4c8674..0x4c8648); which value maps to which name is
    not established.
  - `+3 + hole` bytes: per-hole flags; FUN_00466370 ORs 2 at a story's happy ending, FUN_004289e0 ORs 4 when
    the golfer leaves on the resign path (same block that writes 0xff to +0x29).
  - `+0x29` byte: -1 excludes the type from arrivals (FUN_00421bc0); FUN_00466370 writes the story id there at a
    happy ending, FUN_004289e0 writes 0xff on the resign path.

## Arrival (FUN_0040f5c0 -> FUN_00421bc0(1))
Allocation is attempted when fewer than 2 golfers wait (or every 0x80 ticks), fewer than 8 wait, and bit
0x200000 of 0x59e7b8 is clear. FUN_00421bc0 draws up to 999 random types and accepts one only when no waiting or
playing golfer has the same type or the same type % 19, `+0x29 != -1`, and (for param 1) `+2 != 0`. Otherwise it
copies "Your membership is declining." (0x4c6cac) into 0x51a068 and returns -1. So with param 1 only member types
arrive.

## Join / upgrade (FUN_00427380 at round end -> flag 0x80000000 -> FUN_00406670 from FUN_004289e0 at 0x429146)
At the end of a golfer's round (hole 0x12, or the next hole is not open), unless 0x59e7b8 & 0x200000, points =
sum over played holes of (`+3 + hole` & 3) + 1 (one condition not decoded) + 1 if mood (+0xa4) >= 2*difficulty
(0x822c88) + 6 + 1 if one of several global conditions + 0x543cc4. If the golfer byte +0x18 is 0,
`(1 << level) <= points` and `level < 5`, it sets 0x80000000 in golfer +0x10; FUN_00406670 then raises `+2` by one
("applies for" when it was < 3, "has decided to upgrade to a" otherwise) and draws random types (up to 1000) for one
with `+2 == 0`, setting it to 1 (the "invite a friend" line).

## Resign (FUN_004289e0)
On a leave path selected by a switch over reason codes (cases include 0xd, 0xe, 0xf, 0x15, 0x1a), if the type's
level (`+2 & 7`) is > 1 the text "resigns" (0x4c74e8) is built and `+2` is set to 0.

## Runtime, stories9.sve, x32 (log/probe/member_events.js)
6 min (~9 game years): 13 member types at start; 3 joins in the first 14 s (each: one 0 -> 1 friend and one
1 -> 2 upgrade, mood 1..3 at the flag); 2 of the upgraded types resigned (2 -> 0) at 32 s and 37 s; no further
membership change; 13 at the end. Arrival attempts fail > 99% (U-0003). Membership is the arrival cap on this
save; why joins stop after the first round of play is not established.

## Why arrivals and joins stop (2026-10-04)
- Quitting: in FUN_004289e0 a golfer whose mood (short +0xa4) is below 0 quits when byte +0x84 is 0 and flag
  0x20000000 (golfer +0x10) is clear; with 0x59e7b8 & 0x200000, or category (+0x18 & 0xe0) == 0x20, the mood is
  reset to 0 instead. The leave text depends on `switch (+0x85 & 0x7f)`. The quit block (0x429782..0x4297ac)
  zeroes the type's level when it is > 1, ORs 4 into the type's flag for the current hole and writes 0xff to
  `+0x29`.
- `+0x29 == 0xff` excludes the type from FUN_00421bc0 permanently: the only other writer found is FUN_00466370
  (story id at a happy ending, 0x4667a2). The two other 0xff writers are another leave path in FUN_004289e0
  (0x42b79a) and the Move/Eject golfer action in FUN_0040f5c0 (0x41e602, golfer sent home, hole = 0x13).
- Join points (hook at 0x4286c7, edi = points): level-1 members scored 0..2, so 2 (needed for level 2) was rare;
  the mood point needs mood >= 2 * difficulty + 6 (8 here, moods seen 1..4); the global-condition point needs the
  last hole's flag & 3, difficulty 0, 0x5a6d3c == 0, or (0x571fd4 < 200 and 0x56d1b0 <= 0) (seen: 1, >= 10002).
- Runtime, stories9.sve, x32: all 16 type blocks in 3 min came from the quit block (moods -1/-2, holes 3..8 and
  19); blocked types rose 0 -> 15 in 220 s while members with `+0x29 != -1` fell 12 -> 3; from ~120 s every
  allocation from 0x40fe8b failed (0 of ~9600 per 30 s) and no golfer was on the course.
- So a course where golfers' mood drops below 0 bans their types for good; arrivals, joins and story pairs all
  stop once the remaining member types are exhausted.

## What drives mood down, and a course that keeps it up (2026-10-04)
- Hole rating, FUN_0042dea0: per hole it clears bits 4 and 8 of the hole flags (0x575cb0 + hole*0x208) and, once
  the hole has more than 9 plays in its stats, sets 4 ("too hard") when `((6 - d) * n) / 3 + par * n < strokes`
  and 8 ("too easy") when `strokes < par * n - ((3 - d) * n) / 6` (d = difficulty 0x822c88, n = plays, strokes =
  their sum; par = char at 0x575ab0 + hole*0x208).
- Hole-end thought 0x13 (FUN_00427380 at 0x427708) changes no mood by itself (FUN_00467a00 returns after the text
  for 0x13). FUN_00467a00 turns it into 0x17 when the golfer's category (+0x18 & 0xe0) is not 0x20, the hole has
  more than 9 plays (0x575ad0 + hole*0x208) and either flag 4 is set and the score (byte 0x5794db + golfer*0x100 +
  hole) is more than par + 1, or flag 8 is set and the score is under par. Thought 0x17's delta is
  `-1 - (difficulty != 0)`; negative deltas then become `(delta - 1) / 2` (category 0x40 excepted), so -1.
- Other negative thoughts seen (case values in the switch at 0x467c51): 2 'in the rough' -1, 3 -2, 8 'Who designed
  this ... course' -2 (call site 0x425883 in the shot code FUN_00424120), 9 'that ball almost hit me' -3,
  0x24 (reaction to another golfer's tantrum) -3, 0x23 (from the every-100-ticks wait block in FUN_004289e0,
  0x429882, followed by an extra -1 at 0x4298c3) -2; all halved as above.
- Runtime (log/probe/bad_lie.js, x32):
  - stories9.sve (9 par-3 holes, 35-103 yd, tee and green only on most): 8 of 9 holes had flag 8 after ~70 s (the 9th is a par 2);
    in 180 s the 0x13 -> 0x17 conversions took 75 of the 184 mood points lost (then 0x24 32, 9 26).
  - Sandbox, tee+green holes of ~255 px on random terrain, two runs: 3 holes open, 2 with flag 4, banned types 4
    after 150 s with golfers still on the course; 4 holes open, 2 with flag 4, banned types 12 and the course
    empty from ~200 s.
  - `mood5.sve` (Ocean's Edge MC, 5 holes of ~90 yd, each with a fairway painted between tee and green,
    `tests/fixtures/stories/README.md`), 903 s (log/probe/mood5_long.out): moods -2..8 in the 22 s samples (one
    sample below 0), 8-16 golfers on the course in every sample to the end; banned types 0 -> 11 by 396 s, flat
    to 661 s, then 14 (683 s) and 27 (859 s). 4 of 5 holes had flag 8 (too short for par 3) from the first
    sample. Best story stage 2; the landmark notice 0x4722c0 was not called.
- So fairways remove most of the losses (rough, bad-lie complaint) and keep the course populated for 15 min at
  x32 where tee+green courses emptied in ~3 min; bans still accumulate, and the 'too easy' rating of short
  par 3s is the loss left to remove (longer holes). Story stages on mood5.sve stayed at 1..2 (best 2), so mood alone does
  not bring a pair to chapter 4 (see U-0003 for the other gates).

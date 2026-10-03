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

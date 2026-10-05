# 0x004669f0 — partner reply: why story chapters roll back (2026-10-04)

Context: U-0003. The landmark notice 0x004722c0 needs both partners of a story pair at chapter 4 inside
FUN_00466370. This note covers what stops a chapter advance from sticking after the counter gate has passed.

## Call chain inside FUN_00466370(golfer, force)
1. Gates that return 0 before any change: story id (short +0xb0) == -1; force == 0 and hole (+0x21) < 2 and
   byte +0x22 == 0; both chapters (short +0xb2) > 3; force == 0 and counter (short +0xb4) <= chapter*4 + 4.
2. Chapter += 1 (0x4663f4), stored in the per-hole chapter byte +0x4a+hole (0x579502).
3. FUN_004668f0(story, hole, golfer) (call at 0x46645a) -> FUN_004669f0(story, cap, hole, golfer) (call at
   0x46694d), cap = the golfer's new chapter (read back from +0x4a+hole when the pair flag 0x100000 is set).
   FUN_004669f0 writes the PARTNER's chapter (short +0xb2 of the golfer at +0xa2) at 0x466b15 / 0x466b34.
4. FUN_004668f0(story, hole, golfer ^ 1) (call at 0x4664be): same for the other side.
5. LAB_004666bf: if the partner's chapter is below the golfer's, the advance is undone (chapter -= 1 at
   0x4666df) and the counter loses a third (`c -= c / 3`, 0x466700); return 0. Otherwise return 1, and at
   chapter 4 on both sides the landmark notice is called (0x4667e1).

## The partner reply (FUN_004669f0, log/decomp_a4.c lines 1-60)
When the partner is on the same hole and its byte +0x37+hole (0x5794ef) is 0:
- `v = clamp(mood_p, counter_p, 99)` (FUN_00467130 at the top; mood short +0xa4, counter short +0xb4), i.e.
  max(mood, counter) up to 99, then `v = (v - 1) / 2`;
- when cap > 1: `v = min(v, cap - 1)`;
- adjustment from the partner's latest thought that is not 0x13 (thought list +0x70, args +0x88 short each,
  up to 4 skipped): arg & 0xc000 == 0x4000 -> v + 1; otherwise, when v != 0: any of the bits -> v - 1, and
  0xc000 -> another - 1. FUN_00467a00 sets those bits on the thought it records: 0x4000 when the mood delta is
  > 0 (0x4687c0), 0x8000 when the raw delta was < 0 (0x4687cf), 0xc000 when the applied delta is < 0 (0x4687da);
- FUN_00407000(partner x, partner y, 8) != 0 -> v = cap;
- partner chapter = clamp(v, 0, cap) (0x466b15), and the per-hole byte becomes chapter*8 + 1.
Otherwise the partner chapter = clamp(byte(+0x37+hole of golfer ^ 1) >> 3, 0, cap) (0x466b34).

So for cap > 1 an advance sticks only when `(max(mood_p, counter_p) - 1) / 2 >= cap - 1` AND the partner's
latest non-0x13 thought raised its mood, or when FUN_00407000 finds the landmark condition. For chapter 4:
partner max(mood, counter) >= 7 and a happy latest thought.

## FUN_00407000(x, y, mask) (log/decomp_407000.c)
Scans the 256 placed objects (0x58bcb8, stride 0x10) for type 4 with the int at +8 below 0x10; for each within
`((state * 5 + 40) * 5) / 3` tileDistance units (FUN_0040c4b0: 25 per tile, so 3.7..7.7 tiles) it sets bit
(state & 3); returns `(bits & mask) == mask`. Type 4 is "Landmark": the object table at 0x4c26c0 has a 0x14 stride
with the name at -0x10 (0x4c26b0 + t*0x14: 0 Pathway, 1 Benches, ... 4 Landmark, 5 Home Site, 12 Marina, 15
Clubhouse; FUN_0040e000 makes type 12 water-only and the clubhouse placed at start is type 15). mask 8 = a
landmark with state & 3 == 3 near the partner. The landmark notice FUN_004722c0 picks landmark index 0..11
from the story's first letter (switch on 0x53a454 + story*0x32) and places it via FUN_004074a0(index, 1); the
Ocean's Edge landmark on mood5.sve has state 16 (outside the scan).

## Runtime (mood5.sve, x32, log/probe/gates.js, gates2.js)
- 5 min: 100 calls past the story/hole gates (24 more stopped at the hole gate): 51 rolled back, 18 stopped by the counter gate, 31 advanced
  (most to chapter 1; best 2).
- 5 min, ordered trace: the formula above (if-branch) predicted the partner chapter in 44 of 44 calls.
  Partner's latest thought with bits 0xc000: 18 calls, none reached cap; 0x4000: 11 calls, all reached cap;
  no bits: 15 calls, 6 reached cap (all with cap 1, where min(v, cap - 1) does not apply).
- Partner counters were 2..8 while the advancing golfers' were 6..29.

The counter gate (+0xb4) is not what stops chapters on this course: the partner reply is.

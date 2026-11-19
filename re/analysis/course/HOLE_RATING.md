# Hole rating: every variable (P37 survey, 2026-11-19)

Reference for the P37 hole advisor (`re/roadmap/P37_HOLE_ADVISOR.md`). Two read-only surveys of golf_clean.exe v1.03 and the
port, merged. **[V]** = checked in the disassembly during the survey (capstone over the anchored binary); **[N]** = read from an
existing note or the port's A/B-verified body. The port body of the rating pass is `rate_course_holes`
(`port/src/sim/events_news.inc`, function name rateCourseHoles, the naming pass's holeMagazineEvent 0x0042dea0).

## 1. The hole record (g_holes + hole * 0x208; hole 1..18)

Offsets are from the record start (the model header `re/model/include/simgolf_model.h` HoleRecord, and the port's
rate_course_holes, which reads par at +0, the histogram's row 1 at +0x2a, takings at +0x1f4 and flags at +0x200). The C2 notes cite
absolute addresses with the loop register's own base; use the offsets here.

| Off | Writers | Meaning |
|---|---|---|
| +0x00 par | openHole 0x40e88f..0x40e8c8 [V] | from effective yards E: E <= 50 par 2, 51..249 par 3, 250..474 par 4, 475..625 par 5, > 625 par 6 |
| +0x04 yards | openHole 0x40e7d4..0x40e84f [V]; mainLoop probe 0x41391e / 0x413954 [V] | when 0: sqrt(625 (dx^2 + dy^2)) (25 per tile tee to cup), above 250 + (len - 250) / 4. Probe source value: U (open question 1) |
| E (local) | openHole [V] | yards + 25 with a dogleg (flags & 0x60) and yards > 250 (0x40e863..0x40e86f); above 300 minus 25 * [0x5a8c60] (g_objectTypeLevel[10], 0x40e87a..0x40e88c) |
| +0x08/+0x0c tee, +0x18/+0x1c cup | stepCourseEditQueue 0x40a0b2 / 0x40a032; demolishObject; mainLoop | tiles |
| +0x20 plays | updateGolfers 0x42d421 [N] | tee shots counted; minus 1/8 monthly (ML+0x8835) |
| +0x24 | prepareShot 0x425532 inc [V] (target == -1, club byte != 0xd) | a shot count; minus 1/8 monthly |
| +0x28 histogram 8 x 11 shorts | matchUpdate 0x4273cc..0x4273ef [V] | section 3; never decays; cleared only by openHole |
| +0xd8 comment counts [64], +0x16c last argument | emitGolferThought 0x4687ae / 0x4687b5 [V] | count per thought kind whose final mood delta != 0; minus 1/8 monthly |
| +0x158 mood sum ("fun") | emitGolferThought 0x4686c1 [V] | sum of golfers' mood deltas on the hole; minus 1/8 monthly |
| +0x15c..+0x166 | updateGolfers [N] | drive total, fairways, greens in regulation, putts, longest drive (+0x166 a running max) |
| +0x1ec | matchUpdate [N] | time on the hole, (date - golfer +0xc0) / 2 |
| +0x1f0 | pointsPopup [N] | money spent at the hole (subtracted) |
| +0x1f4 takings | matchUpdate 0x4276aa.. [V]; zeroed by openHole 0x40eaa5 | cumulative greens fee taken on this hole (cash / 100 units); no decay. The model header's `u1f4` |
| +0x1f8 | mainLoop ML+0x82a2 [N] | upkeep ticks: every 0x400 / (difficulty + 2) ticks an open hole costs 1 cash unit |
| +0x1fc sameness | rateCourseHoles only [V] | section 4; the model header's `magazine_score` is this counter |
| +0x200 flags | below | |

Flag bits of +0x200:
- 0x1 Best-100 (Golf Enquirer), 0x2 Top-18 (Great Golf Holes): rateCourseHoles [V].
- 0x4 too hard, 0x8 too easy: recomputed every pass, 0x42e041..0x42e0bd [V].
- 0x10 cleared each pass (0x42e334); setter not found (open question 3).
- 0x20 / 0x40 dogleg left / right: the probe 0x413b25..0x413b4f compares the tee->cup angle with the probe landing->cup angle
  (+-0x71c71c6 of a 2^32 circle, 10 degrees) [V]; openHole's strings name them.
- 0x80 the hole has a magazine name (appendHoleName tests & 0x81).
- 0x100 / 0x200 / 0x400 notable length / accuracy / imagination (section 4).
- 0x1000 / 0x2000 uphill / downhill: 0x413a0c / 0x413a77, cup height > tee height + 1 or < tee height - 1 (heightBlend) [V].

## 2. Takings and the magazine awards [V]

matchUpdate 0x42746a..0x4274bc builds the fee of a golfer finishing a hole: the golfer's mood word +0xa4 (-10..10), doubled when
[0x543cf4] == 2, + 2 for each of hole flags 1 and 2, + [0x543cd8] (an amenity fee bonus), + 2 or 5 for type ranks above 3 [N].
It goes to cash and (when g_flags bit 0x200000 is clear [N]) to the hole's takings +0x1f4. A hole's takings are the sum of its
finishing golfers' moods and can fall.

rateCourseHoles, per open hole:
- skillDelta = the hole's length + accuracy + imagination deltas (section 4).
- difficulty <= 1 (0x42e46d): skillDelta = max(skillDelta, funDelta), funDelta = 6 * fun / (2 + (difficulty != 0)), fun = the
  hole's term of g_funRating (section 5).
- Best-100: takings > 200 and skillDelta > 200 and flags & 0xd == 0 (0x42e478..0x42e49b).
- Top-18: takings > 400 and skillDelta > 300 and flags & 0xe == 0 (0x42e649..0x42e66c). Best-100 is not a prerequisite.
- Each award needs startMessage to accept; then logTick, a sound and postEvent 7 / 10.
- The message promises "Increase greens fees by $100"; the code adds 2 fee units (open question 4).

## 3. The shot histogram [V]

matchUpdate 0x4273c0..0x4273ef: index = clamp(strokes byte +0x22, 0, 9) + (golfer prefs +0x19 & 0xf) * 11 (+ hole * 0x104
shorts), only for golfers whose kind byte +0x18 == 0. The prefs bits are the golfer's attributes (info panel table 0x4c2858):
bit 0 length, bit 1 accuracy, bit 2 imagination. Column c = a prefs value 0..7: column 7 has all three (the baseline), column 6
lacks length, 5 lacks accuracy, 3 lacks imagination. Rows 1..9 = strokes; row 0 is not read by the rating.

Per column (0x42dff0..0x42e03f): col40 = 8 + sum v, col20 = 8 par + sum v * strokes (a prior of 8 plays at par).
n = all counted plays, S = their strokes.

Too hard / too easy, n >= 10 (0x42e04a), d = g_difficulty 0x822c88 (0 Easy, 1 Moderate, 2 Difficult, 3 Impossible [N]):
- too hard (bit 4): S > par n + (6 - d) n / 3, an average above par + 2.00 / 1.67 / 1.33 / 1.00 for d = 0..3
- too easy (bit 8): S < par n - (3 - d) n / 6, an average below par - 0.50 / 0.33 / 0.17 / 0.00 for d = 0..3
(integer divisions as the code makes them.)

## 4. Skill deltas, hole type, sameness [V]

delta_X = 100 col20[c] / col40[c] - 100 col20[7] / col40[7], c = 6 length, 5 accuracy, 3 imagination (0x42e0f8..0x42e26e): the
hundredths of a stroke by which golfers lacking the attribute score worse than golfers with all three. Added to 0x5a882c /
0x56949c / 0x5a636c and the running sum.
- Flag 0x100 / 0x200 / 0x400 when delta >= 50, every difficulty.
- Notice bit 1 / 2 / 4 when delta >= 25 at d = 0, >= 50 otherwise. Then the smallest delta (if under 100) loses its notice bit
  when it is under 50 (d = 0) or under 100 (d != 0) (0x42e27a..0x42e29e).
- The notice mask is the hole type: 1 FREEWAY (length), 2 PRECISE (accuracy), 3 CHALLENGE (length + accuracy), 4 CREATIVE
  (imagination), 5 HEROIC (length + imagination), 6 STRATEGIC (accuracy + imagination), 7 CLASSIC (all three). Each kind is
  announced once per game (mask 0x5685f8).
- The 0x100..0x400 bits feed the end-of-hole comment "My length skill helps..." (0x46c37f..0x46c3b0); no mood change.

Sameness counter +0x1fc (0x42e337..0x42e430), holes 2..18, from 0, +1 for each:
- same type as the previous hole, with plays >= 8 and type != 7
- same dogleg bits (0x60) as the previous hole
- comment counts 0x2d and 0x2e both 0 (0x2d never counts: switch default, byte table 0x4698f0 index 38; in practice "no
  downhill comment")
- same par as the previous hole
- tee->cup direction within 40/256 of a turn (56 degrees) of the previous hole's (angleFixed difference >> 24)
then minus 1 at difficulty < 2 (when non-zero).

prepareShot 0x4259db..0x425a49: counter <= Random(3) gives thought 0x1d "I like the variety on this course." (+1 mood); at
d != 0, on stroke 1 of hole > 1, counter > Random(3) + 3 gives thought 0x1e "Great, another dogleg left." (-2 mood).

## 5. Course totals

- g_funRating 0x59ae78 += 100 * mood sum / (+0x24 / 2 + plays + 4) per hole with plays != 0 (0x42df93..0x42dfe1) [V].
- g_skillRatingX100 0x541cd8 = sum of every delta over all holes (0x42e794) [V].
Readers (whole-image scan) [V]: addScore (top-score rank (cash/10 + skill + fun) * (level + 1) [N]); pairGolfersWithStory
0x45e77c.. (fun at d <= 1, skill at d >= 2, as the rating gating VIP visits: k = 3 rating > (land purchases + 2)(d + 2) 50,
k = 5 rating > (milestone + 1)^2 25 [N]); the monthly history rings; writeCourseInfo; the game browser; mainLoop's "Skill
Rating:" text. No fee or arrival code reads either total.

## 6. Mood changes on a hole (emitGolferThought's switch [N]; kind 0x17's conversion [V])

- 0x13 -> 0x17 "This hole is too hard!" / "too easy." when plays >= 10 and (flag 4 and score > par + 1) or (flag 8 and score
  < par) (0x467a6a..0x467a9b): -1 at Easy, -2 otherwise.
- Lie and terrain: rough / bad lie (2, 3) -1 / -2; 0xc hazard -1; 0xd water -1; 0x18 something growing -2; 0x14 ugly view -2;
  0xa / 0x2b steep slope -2.
- Safety: 9 ball almost hit me -3; 0x24 another golfer's tantrum -3.
- Waiting: 0x15 -2; 0x23 -2. Needs: 0xe thirsty -1, 0xf hungry -1, 0x1a tired -1.
- Variety: 0x1e monotony -2, 0x1d variety +1. 8 "Who designed this course?" -2.
- Positive: 1, 6, 0xb, 0x16, 0x1c, 0x20..0x22, 0x2c, 0x2e (downhill) +1; conditional 0x12, 0x19, 0x1b, 0x27, 0x33..0x36, 0x3b.
- Then a negative delta becomes (delta - 1) / 2 except category 0x40; some -1s are dropped unless the complaint is recent; mood
  is clamped to -10..10.
- Lie: landing on lie > 0 gives thought 2 or 3, mood -1; a clean landing on lie <= 0 gives thought 1, mood +1
  (`port/src/sim/gfx_text.cpp` 1263-1264).

## 7. Costs (cash / 100 units; g_cash_div100 0x00571fd4)

- Painting a tile: attr[old] +0x24 + attr[new] +0x23 (table 0x004c1a40, 23 records of 0x30; port tick_frame.inc 9932-9933).
  +0x23: tee 5, green 10, fairway 3, firm fairway 3, rough 1, deep rough 2, mound 4, sand trap 6, waste bunker 4, pot bunker 8,
  ravine 10, brush 4, rocks 4, tree / pine / palm 10, elm 25, water 50, wetlands 2, marsh 6, out of bounds 10, building 0.
  +0x24 (removing the old type): 0 for most; rocks / tree / pine / palm 5, elm 10, water 10, wetlands 50, marsh 100.
- Path tile 2 (15 on water), path removal 2; the path charge also goes to the hole's +0x1f0.
- Raise / lower terrain: no charge in the key branches (tick_frame.inc 8389-8620); the mouse brush is not checked.
- Objects (f_tab_buildItems 0x004c26b0, price word +0x12): Pathway 1, Benches 2, Flower Bed 5, Ball Washer 50, Landmark 15,
  Home Site 10, Putting Green 100, Snack Bar 150, Pro Shop 200, Swim Club 300, Driving Range 250, Cart Garage 400, Marina 1000,
  Resort Hotel 2500, Airstrip 5000, Clubhouse 200, Willow Tree 25, TV Tower 10, TV Booth 10, Scenic Bridge 100. Charge =
  evalPlacementArea + price * (g_objectTypeLevel[t] + 2) / 2: every copy of a type costs more.
- Staff wages per week (0x004c2e2c): Club Pro 3, Celebrity 7, Ranger 2, Marshall 3, Groundskeeper 2, Technician 4, Soda Vendor
  2, Refresher 5; firing 25.
- Course tiers (tierPrice 0x0046f1d0): 500 .. 10000.

## Open questions
1. [esp+0x14] in the mainLoop probe's yards write (0x413954), and what runs the probe (U-0017).
2. Which object type [0x5a8c60] (g_objectTypeLevel[10]) is.
3. The setter of flag 0x10.
4. +2 fee units per award against the message's "$100".
5. Whether prefs & 0xf exceeds 7 (bits 4..7 are g_challengeLevel << 4); if so the histogram index overruns into +0xd8.
6. Random::range(3)'s range at 0x45c1e0.
7. Readers of the flags dword not examined: 0x412102, 0x4122a9, 0x417365, 0x46ac54, 0x46b0ea, 0x46d141.

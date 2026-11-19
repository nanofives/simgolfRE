# P37: hole advisor (Mariano, 2026-11-19)

Goal (his words): "improve over the existing course, find ways to do better designs and do challenges around that, find
something to spend money on", "build a better panel to figure out what to improve on existing holes".

Every number the advisor shows comes from `re/analysis/course/HOLE_RATING.md` (the rating pass rateCourseHoles, the histogram,
takings, mood, costs). The advisor is drawing only: it reads the GameState, never writes it, so segments and goldens stay as they
are. Each step is an app option, default on, off under `SG_APP_P37=off` (port/test/conftest.py), following P36B's
--wait-reasons. English in port code, other languages through the pack (lang::tr). The game's fonts and art (CLAUDE.md rule).

## Shared core: `port/src/sim/holeadvice.{h,cpp}`
`HoleAnalysis analyze_hole(Ctx &, int hole)`, read-only, recomputing what rateCourseHoles derives for one hole:
plays n, strokes S, average, the too-hard / too-easy limits at the current difficulty and the margin to each; the three skill
deltas and the hole type they make (notice mask rules); the fun term; skillDelta as the magazine test sees it (fun substitute at
d <= 1); takings; the award state and what is missing for Best-100 / Top-18; the sameness counter's five terms against the
previous hole; the top complaints (comment counts with negative mood); money spent and taken.
Oracle: the test runs the port's rateCourseHoles (A/B-verified against the original) on random hole records and checks that
analyze_hole gives the same too-hard / too-easy bits, notable bits, notice mask, sameness counter and skill totals.

## Steps (each is built, smoke-run, screenshot, then shown to Mariano before the next)

### P37A  The hole report (on the Hole Stats screen) (built 2026-11-19)
A panel under the Hole Stats screen's art (opened by the hole keys, the hole jump), `--hole-advice on|off`:
- Awards: "Best 100: takings 152 of 200, skill 2.10 of 2.00, blocked: too easy". Same for Top 18. "Won" when the bit is set.
- Difficulty band: average strokes against par with both limits for this difficulty and how far the hole is from each.
- Skills: the three deltas, the type they make now, and the one skill to raise for the next type up (e.g. HEROIC -> CLASSIC needs
  accuracy +0.50).
- Fun: the hole's fun term, its word, and the three most frequent complaints with the fix each points to (rough -> fairway,
  waiting -> shorter hole ahead or a marshal, ugly view -> flowers / trees, too hard -> remove hazards near the landing).
- Variety: which of the five sameness terms hold against the previous hole (same par, same direction, same dogleg, same type, no
  downhill), the counter and what it costs in mood.
- Money: spent on the hole, taken, net.
Test: `port/test/test_p37_hole_advice.py` (oracle above; panel text built for a fixed record; GameState unchanged with the option
on and off; test_holestats.py still green with the option off).

### P37B  Course overview: which hole to fix first (built 2026-11-19: under the Course Report, F1)
A port-only screen (a key from the Hole Stats screen and from the course report) with one row per hole: par, average, band
status, type, fun, takings vs 200 / 400, skill vs 200 / 300, awards won, sameness. Sorted by "closest to an award". Course
totals: fun and skill ratings, the types collected so far (mask 0x5685f8), awards won.

### P37C  Problem spots on the map
With a hole selected, mark the tiles where golfers had bad thoughts (per-tile good / bad counters g_tileMoodUp / g_tileMoodDown;
their meaning checked first) and the landing areas on rough. Drawn on the course view, toggled by a key.

### P37D  Cost of a fix
Each suggestion in P37A gets its price from the tile and object tables (section 7 of HOLE_RATING.md): "rough -> fairway on the 23
landing tiles: $6,900", "flower beds along the path: $500 each, more for every copy".

### P37E  Challenges
A goals page: every hole in Best-100, every hole in Top-18, all seven hole types collected, no hole too hard or too easy at
Difficult and Impossible, no sameness, a budget cap per rebuilt hole. Progress from the same analysis, saved with the player's
port settings (not in the .sve).

## Release
Per the play-test routine (auto-memory: light testing, fast per-fix release): x64 build, test_static, the new test, test_holestats,
one smoke run with a screenshot; publish to the LAN + GitHub release only after Mariano has seen the screenshot.

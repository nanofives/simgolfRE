# P38: large maps (Mariano, 2026-11-19)

His words: "quiero que los mapas sean más grandes", "que al jugar mapas grandes la porción de compra sea proporcional igual en
todos los tamaños, si tengo mapa grande, el tamaño agregado debería ser mayor"; sizes: "Más grande" (well above 100).

Survey 2026-11-19 (read-only agent; every claim cites port file:line there, summarised here). Status: plan, nothing built.

## What the world size is today
- 50 x 50 tiles, index x * 50 + y; the 51 x 51 vertex grid g_cornerLevel; buyable rows / columns 1..48 in a 3 x 3 grid of
  16 x 16 lots (buyland.cpp).
- 17 exe-side per-tile grids in the GameState (29 bytes per tile, gamestate.h 698-1130), plus the Terrain.dll object (0x248 bytes
  per tile, terrain_state.h 96-100) and the vertex / normal arrays (terrain_live.cpp 675-676). About 2.1 MB at 50, 18.7 MB at
  150, 33.2 MB at 200: memory and the 32-bit address space are not a limit.
- The GameState is one packed struct pinned by static_asserts (the corpus images, the emulator oracle, golden digests and the
  save tables depend on its offsets): the grids cannot grow in place. Large grids go to side storage mapped into the address
  space (Memory::map), as the Terrain object already is.
- About 400-500 access sites encode the size: 64 grid-helper lines (one-line changes, kGridW = 50 in tiles.h), 246 register-level
  VA lines (212 in tick_frame.inc, some with the stride hidden in lea chains or a loop ending at another global's address), the
  rest typed member indexing. Walker post_x / post_y are int8 (overflow above 127).
- Terrain is always generated (generate_random_course, terrain_gen.cpp); no per-location map file. The generator's walk counts,
  bounds, sweep radius 0x19, desert falloff y < 16 and the 16 x 16 noise period are sized for 50.
- The .sve has no version or size field: a large map needs a port-only save (magic, version, N).
- Costs that grow with the area: the mainLoop per-tile pass (tileToScreen for every tile every frame: 2,500 -> 40,000 at 200),
  pathStep's full-grid clear per call and its 1,024-entry ring, year-end decay, shade spreading, coverage / heightfield rebuilds.
  The renderers walk the camera window and do not grow with the world.
- The routing / mini map draws 12 x 6 px diamonds at fixed offsets: 50 tiles span 588 px, 200 would span 2,388 px.

## Sizes
Equal lots need (N - 2) divisible by 3: 101 (lots of 33), 149 (49), 152 (50), 200 (66). 150 gives unequal lots.

## Phases
| phase | content | risk | test | size |
|---|---|---|---|---|
| A | WorldDim in the context (N = 50 default) and one accessor per grid; every stride and bound through it; at N = 50 the same addresses | M (lea-split strides in tick_frame / tick_golfers) | segments 205 / 205, oracle, goldens unchanged; a static test refusing new grid literals | L |
| B | Side storage for N > 50 (grids, Terrain object, vertex arrays), widened walker posts, a larger path ring; a flat sandbox boot | M | boot and build at N = 101 / 200, tools on the far corners | M |
| C | Generator for large N: scaled counts and bounds, N / 2 sweep, noise period, acres scale | M (look) | screenshots per location, determinism by seed | M |
| D | Port-only save / course format with magic, version and N; the load browser | M | round trip at 200; .sve untouched | S-M |
| E | Proportional lots (3 x 3, (N - 2) / 3 a side) and their price | S | lot sums, purchase writes, screenshot | S |
| F | Mini map scale (12 * 50 / N px per tile), camera defaults, hover projection | M | screenshots at 50 (unchanged) and 200 | M |
| G | Performance: cull the per-tile frame pass to the view, bounded pathStep clear, faster side-storage lookup | M | frame and tick timings at 200 vs 50 | M |
Everything but A applies only in the play profile with an app option (--world-size N). Challenge / Championship courses and
existing saves stay 50.

## Decided 2026-11-19
Sizes 152 first, then 101. Lot price per tile. Owned share at the start: the same proportion as at 50. Phase A started (worker
P38A, brief log/roadmap/P38/BRIEF.md). Added: P38H, Picky's land offer keeps 2 lots buyable later, not only at the moment.

## Decisions (Mariano)
1. Sizes offered (101 / 149 / 152 / 200, or 150 with unequal lots).
2. Lot price: per tile (a 200 lot about 17x today's), the same per lot, or between.
3. Owned share at the start and how acres are shown (the acres field is a byte).
4. Which locations get large maps; accept the noise repetition or add octaves.
5. Original modes stay 50 (proposed: yes).
6. Golfer walking limits (path cost cap) on big maps: keep the original's or raise them.

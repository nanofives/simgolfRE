# P39: course building and play features (Mariano, 2026-11-20)

His list: "placeable bridges that are two tiles long, use style of the existing bridges"; "pathway can go underneath all elements
and still have the texture of a pathway coming from the central house"; "highlight of the hole should follow the whole shape of
the green and not just the tile where the hole is"; "selecting where to throw the ball should be free instead of tied to an
invisible grid, and it should snap near hole". Four read-only surveys (2026-11-20) found the facts below; every port file:line is
in their reports, summarised here. All four are play-profile changes (a play switch or app option; the original unchanged at 1 /
off; segments x64 205 / 205), sized for any world N (P38 world.h).

## P39A Two-tile scenic bridge (M, L if the art needs per-rotation tuning)
Facts: a Scenic Bridge is not a placed object: tool 19 sets TF_PATH 0x20 + bit 0x100 + a 3-bit art variant on ONE water tile
(type 0x11 only, not stream 0x0a), H2_Build case 5 (tick_frame.inc ~9613). Cost 100 + water's surcharge; needs a non-water tile
in the 3 x 3 around. Drawn by P1_flag20 from sheets 0x226..0x22d (Bridges\SCENICgen03..10, one tile, two axis frames); a tile
with any path-on-water neighbour switches to the boardwalk set (0x1fe..0x205), so two scenic bridges side by side lose their art
today. Golfers walk path-flagged water at path cost; thought 7 "I love walking over this scenic bridge" fires per tile. No
multi-tile scenic art exists (SCENICgen01/02.flc in original\Flics\Bridges are never loaded).
Design: a new tool next to the bridge in the palette: two orthogonal water tiles, land at both ends along the axis (rotation from
the drag or a key through g_cameraRot); each tile gets the original's 0x120 + variant, so walking, the thought and saves work as
today; the pair recorded in a port side table (bits 3-4 of the flag word would draw wrong cells in the original); a play-only draw
branch in P1_flag20 before the neighbour test: one scenic frame across both tiles (scaled) or two clipped halves, mocked against
screenshots first. Cost about twice the one-tile bridge (new rule). Open: whether 0x100 affects shot planning (U-0200).

## P39B Paths under every element, with the clubhouse look (S for A, M for B)
Facts: a path is the TF_PATH 0x20 flag, separate from the tile type; it survives painting terrain, trees, flower beds, benches,
buildings and fairways in the data. Two drawing rules hide or change it: the Terrain port does not draw a path on tee, green,
fairway, firm fairway, sand, pot bunker, out of bounds and large buildings (terrain_state.cpp set_path_flag_for_type /
tile_lay_path / enter_type7); the paved texture 0x21 marks tiles reached by the flood from the clubhouse (TF_FLOOD_REACHED
0x40, rebuild_tile_coverage), which stops spreading at tee / green / fairway / firm fairway (walk byte <= 0), so a path past a
fairway takes texture 0x20. Placing a tee or a cup overwrites the flag word (the path is erased there).
Options: A drawing only: draw the path on every type; B = A + a texture-only flood from the clubhouse that also crosses tee /
green / fairway paths (TF_FLOOD_REACHED itself unchanged: it decides which buildings count as joined to the clubhouse, i.e.
income); C paths survive tees and cups (changes golfer speed, routes and ball bounce there: not advised).

## P39C The green highlight follows the whole green (M)
Facts: the only cup-tile-only highlight is P1_flag80's blinking marker (g_sprite5423e8, the selected-golfer marker sprite) on the
player's current hole while aiming (g_panelListMode == 3). Green tiles carry no hole number; P22B_OnHoleGreen already floods the
green from the cup (8-connected, world-size aware). Outline: for each green tile, the edges whose neighbour is not green,
projected with worldToScreen (corner heights, so it follows the slope) and drawn with drawMapLine inside GfxWideLineScope.
Two touching greens split by the nearer cup. App option --green-highlight (drawing only), cost negligible (cached per hole).

## P39D Free shot aiming with a snap to the cup (M)
Facts: the aim preview snaps to tile centres and corners (a half-tile lattice, tick_frame.inc ~7120-7174), and the shot then
flies to the clicked tile's CENTRE regardless (prepareShot mode 1 uses (target << 10) + 0x200; the corner shown is dropped). The
sim keeps ball / golfer positions at 1 / 1024 tile and prepareShot already accepts any world point (its trial path), so free aim
needs no sim data change: keep the target tile fields as point >> 10 and pass the free world point (held outside the GameState)
to the player's real shot. Snap: within ~0x300 world units (3 / 4 tile, tuned in play) of the cup centre and on the hole's own
green, the point becomes the cup centre (then the shot equals today's click on the cup tile exactly). The player never aims putts
on the hole's own green today (P22B auto putt). The P32D shot-spread preview follows the free point.

## Order and questions
Proposed: C and B-A first (small, visible), then D, then A (art risk). Questions for Mariano: which elements hide the path or
change its look in his games; paths on greens / tees drawn on the putting surface or stopping at the edge; whether a path should
survive placing a tee / cup (gameplay); which highlight he means (the blinking marker while aiming); the snap distance and whether
he wants to aim putts too; bridges over streams too, and the cost.

## Decided 2026-11-20
- Paths: B (drawn on every type and building + a texture-only flood from the clubhouse across fairway / tee / green paths;
  TF_FLOOD_REACHED unchanged). On greens and tees the path STOPS AT THE EDGE (not drawn on the putting surface or tee box), and
  resumes after.
- Highlight: the blinking marker while aiming becomes an outline of the whole green; aiming snaps to the cup.
- Bridges, extended: two-tile bridges over water AND streams AND big gaps in height; "a rework on the path to make bridges where
  the gap is too high, and we should add stairs if the height difference is too high as well". P39A becomes: the two-tile scenic
  bridge (water, stream) + a path rework: where the path tool crosses a height gap too high for a path, it builds a bridge; where
  a slope is too steep, stairs. Needs its own survey (path over slopes, the steep-slope thoughts 0xa / 0x2b, height limits, art
  for stairs: none known) before its design.
Order: P39B and P39C in parallel now, then P39D, then the P39A survey and design.

## P39A survey (2026-11-20): bridges over water, streams and height gaps, stairs
- Heights: g_cornerLevel (N+1)^2 bytes, levels 3..15 (18 in play), one level = 16 / 1024 of a tile. Neighbours share vertices, so
  real cliffs exist only at flattened types (water, pot bunker lowered; tees, buildings raised: the retaining wall art).
  The path tool never tests height; paths drape over the mesh; walking has no height term; cliffs are walkable.
- "Must I walk up this steep slope?" (0x2b, mood -2): an orthogonal step rising 3+ levels on the golfer's half tile; paths do
  not exempt it. 0x0a ("Must I walk through ...") is about the terrain type, not slope.
- Streams (type 0x0a, "ravine") are not lowered; a path over one draws as a plain band; the scenic bridge refuses them only by
  its type test (== 0x11).
- Art: every bridge flic is one tile wide (SCENICgen03..10, the per-theme boardwalk set, DESBridgeScenic loaded but unused);
  no stairs / steps / ramp art exists: stairs are composed in the 3D renderer (path-texture treads + retaining-wall risers).
- Design: (a) two-tile scenic bridge on water / stream, pair in a port side table, drawn before P1_flag20's neighbour test;
  (b1) stairs COMPUTED from heights (a path tile rising 3+ levels along its path axis), no save change, golfers skip 0x2b on
  stairs; (b2) gap bridges built by the path tool across 1-2 tiles at least 3 levels below both banks, deck at bank height
  (walkers drawn at the deck), side table entry; wider gaps: stairs down and up. Storage: a sidecar file next to the .sve (the
  original then shows a boardwalk / plain path) or a .sgw section (version 2). New module crossings.cpp (bridge.cpp is taken).
- Sizes: (a) M (L with per-rotation tuning), (b1) M, (b2) L, storage S-M.

## P39A decided (2026-11-20)
Gap bridges: the path tool ASKS FIRST (a game-style popup with the bridge and its cost), threshold 3 levels. Stairs: always
allowed, a stairs tile costs path cost x 2. The scenic-bridge thought (+mood) only on scenic WATER bridges (as the original).
Storage: a small sidecar file next to the save (the .sve stays readable by the original). Order: A1 (two-tile scenic bridge over
water / stream + crossings module + sidecar) and A2 (stairs) in parallel, then A3 (gap bridges with the popup).

# P41 area hud_panels

Employee panel, Golfer action panel, Member list, Golfer info panel (55 roster elements, 7 of them kept as art).

Run everything: `py -3.12 re/tools/uivec/areas/hud_panels/run.py [ids...]`, then `results.py` writes
`log/roadmap/P41/work/hud_panels/results.tsv`. `run.py --paint [ids]` re-measures the bubble body paints (glyphs left
out), `run.py --shadows` re-fits the bubbles' cast shadows.

| module | what |
|---|---|
| common.py | sheets, element cells in sheet coordinates, `zoom` / `dump` / `hexdump` / `covdump` viewers, comparison writer + scores |
| kit.py | shape helpers, `shift` (moves a whole scene tree), `sample` / `sample_dark` (paint measured on the art), `match_offset` (whole-pixel glyph offset from coverage correlation), `find_cell` (FFT masked match of a cell inside a larger region) |
| bubble.py, measure_bubbles.py | bubble rim / body circle measurement, per-family body paint (bevel least squares), cast-shadow fit |
| el_bubbles.py | registry, glyph layer builder, Golfer action club / crossed / trophy |
| el_employee.py, el_golfer.py, el_blue.py | the other bubble buttons and their authored glyphs |
| el_marks.py | hearts, pips, ball, bars, slider, skill bar, page arrows, mood faces |
| el_pads.py | shot-shape pads, pedestal |
| el_slabs.py | `fit_bevel`: a flat plate's lit edges as ONE measured sdf bevel paint (optional `of` = the uncut plate, flat face beyond the bevel); Golfer info slabs |
| el_plate.py, el_trays.py | match plate; the three trays composed from their parts and the placed buttons |

Toolkit files under `re/tools/uivec/` were not edited.

## Fix round (2026-11-24, Mariano's verdicts in log/roadmap/P41/judge/decisions_final.json)

Output now goes to `log/roadmap/P41/work2/hud_panels/` (common.OUT; `HUDP_WORK=work` points it back at round 1). Run the
33 fix/redo elements: `py -3.12 run.py <ids>`, then `py -3.12 results2.py` (rows in results2.py + results2_more.py).

| module | what |
|---|---|
| figs.py | `hud_panels.limb` (polyline with a width at each point, exact round cones, round joins: limbs, shafts, smiles), `hud_panels.rrect4` (rounded rectangle with its own radius per corner, exact distance: no seam under a lit surface) |
| figtool.py | `show(key, box, shape)`: the art's dark coverage beside a shape's coverage and their difference, as text |
| el_figs.py | blue tab figures with volume; the blue rim as one lit pixel (`rim_paint`), blue body shading re-measured |
| el_tan2.py | mood / photo / hire / fire / resume / thumbs-up glyphs re-read; `("paint", f)` part paints; thumbs-up rim + shadow |
| el_small2.py | club / crossed / trophy: concentric 1.4 px rims, refitted shadows, two-tone shafts, straight-walled cup |
| el_faces2.py | the ten mood faces from per-face colour maps (one 16 px cell per face in the comparison) |
| el_pads2.py | shot pads: lip, 1.5 px ring, body + crescent, one continuous shot path, ball and shadow |
| el_ped2.py | pedestal: convex lit cap (fitted bevel), cap shade, soft side shadow |
| el_flat2.py | bars (knob cut, end shading), slider, page arrows, match plate (strip + plate), story band well |
| el_trays2.py | trays: silhouette lobe (halo circles), dark lobe edge, lit halo rings, rivet, arched note box |

## Round 3 (2026-11-24, log/roadmap/P41/FIX3_BRIEF.md)

Output `log/roadmap/P41/work3/hud_panels/`: `py -3.12 run3.py [ids]` (two-version elements as `<id>--a` / `<id>--b`), then
`py -3.12 results3.py`.

| module | what |
|---|---|
| fix3.py | `side` / `sides`: art beside a scene at 12-16x with a pixel grid; `low`: a scene at 1:1 |
| patches.py | version A: a glyph's tone patches (pixels that differ from the redrawn surface, k-means tones, each 4-connected patch its pixel outline with one round of corner cutting, its own colour); `strip_glyph` drops a bubble's named glyph layers |
| el_fix3_plate.py | match plate: concave cove silhouette, strip and plate as fitted lit surfaces with rolled corners, square sunken cells, outlined bars |
| el_fix3_figs.py | staff tabs A / B (B: the man and post re-read) |
| el_fix3_trophy.py | trophy A / B (B: ring handles, tapered bowl, per-column bands, two-tone foot) |
| el_fix3_faces.py | mood faces A / B (B: round 2's feature shapes painted with edge shade, core + edge tones, glints) |

## Round 7 (2026-11-26, log/roadmap/P41/FIX7_BRIEF.md #5: one structure per button across its states)

Output `log/roadmap/P41/work7/hud_panels/`: `py -3.12 fix7.py [tabs] [ids]` (no argument: tabs + every multi-state element),
then `py -3.12 fix7_results.py` (results.tsv; moves the per-state decisions into `_notes/<id>.json`).

| module | what |
|---|---|
| fix7_look.py | the approved tab scenes as they appear on the three trays (yellow + baked blue) and as cuts, art over drawing |
| fix7_audit.py | `audit(el)`: each state's scene against idle (item / layer structure, whole-px move, residual); `MULTI` list |
| fix7_detail.py | which layer parameters differ between states (after the move) |
| fix7_artstrip.py | a state strip of the art with idle's rim circle moved onto each state |
| fix7_artmove.py | the ART's own whole-px move per state (edge-map match), independent of the scenes |
| fix7.py | tabs: one canonical scene per glyph (the approved idle cut) placed at every cut and tray position, yellow = same shapes with rim / body paint measured on the yellow art; multi-state: idle's shapes moved per layer by whole px, state paints, grounds the art does not move held (`FIXED_ITEMS`), `MOVE_AS` for a state whose approved scene missed the art's move, a reshaped layer (cast shadow) kept only when it fits the art 20 % better; notes cross-check every move against fix7_artmove |
| fix7_results.py | results.tsv |

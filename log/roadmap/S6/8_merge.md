# S6.8 merge (D52b3, G3)

## re/port/defect_profiles.tsv: append this row (profile `fixed`)
generateRandomCourse.placementBound	0	D52b3	the feature-site searches gated by evalPlacementArea (location 4: 2 x 2 site, kind 4; locations 5, 6, 8, 12, 13, 15: 5 x 5 site, kind 0xF) never end in the original when no reachable cell is accepted; fixed gives up and skips that feature (model_defects#52)

## re/port/defect_decisions.tsv / log/roadmap/S6/6_decisions.tsv, row D52b3
recommendation ask -> fix (G3); switch generateRandomCourse.placementBound (NOT searchBound); evidence 8_probes/d52b3_placement.py
(.log), d52b3_port_images.py (.log, _plain.log); status implemented; U-1598, U-1599.

## UNCERTAINTIES.md rows
From log/roadmap/S6/8_uncertain.tsv (U-1598, U-1599).

## Other
- New switch in port/src/sim/terrain_gen.cpp kDefects (no modules.inc / CMake change). Wave 1's searchBound is unchanged.
- negctl modes 4 and 5 of generateRandomCourse added (test-only wrong fixed arms); test_terrain_gen.py's NEGCTL list (1..3)
  is untouched.
- New test file port/test/test_defects_gen_w2.py (30 tests); integrator.py's switch -> test map (line ~876) could gain
  "generateRandomCourse.placementBound": "port/test/test_defects_gen_w2.py::test_blocked_site_search_hangs_at_1_and_returns_at_0".

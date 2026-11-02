# S3.28 brief (wave 9, candidate 2): lowerEdgeCorner's black-light pass (U-1952 / U-1955 / U-1956)

Read log/roadmap/S3/W9_COMMON.md first (base tree, lock, delivery, census, report). Then log/roadmap/S3/27_notes.md (whole; section 3
is your plan), 27_uncertain.tsv, 26_notes.md "Group 2", 1_notes.md (S3.1's draw-list passes and recorder), 2_notes.md (LiveTerrain in
the frame), and the matched sources re/match/terrain_edge.cpp (renderTileAtFiveOffsets 0x10038900) and terrain_small3.cpp
(enableBlackLight1 0x10003830).

Tree: port/build/S3_28 (w8_assemble.py --dst S3_28). Step id for the lock: S3.28. U ids U-1960..U-1964. Time box 6 h. Model note:
you are opus; heavy runs one at a time.

## Task

1. Recorder: give log/roadmap/S3/27_gl_rec.js / 27_gl_record.py (or your own 28_ copies) lowerEdgeCorner's fourth argument (the
   angle) in the snapshot meta (U-1956), and re-record cov_r_s2_screens' kind-5 passes on instance `instances\s3_w8` (game slot granted;
   one boot per lock hold; the run must be SAME as det_b: `re/tools/scenario_refs.py check --scenario cov_r_s2_screens --run <run>`). If
   the census shows other scenarios with lowerEdgeCorner rows that move, record what the oracle needs for at least one non-zero angle
   (U-1955 lists 50 of 91 terrain recordings with calls).
2. Port: renderTileAtFiveOffsets + enableBlackLight1 as terrain pass kind 5 (terrain_render.cpp / .h, terrain_render_entry), handed to
   the backend from LiveTerrain::lower_edge_corner BEFORE the state call (terrain_live.cpp), accepted by gfx_terrain.cpp's terrain.pass;
   the GL sink carries glLightfv(0x4001, ...) and glEnable / glDisable(0x4001) so the stream equals the original's lines; WglSink passes
   them through. Do NOT edit gl_raster.cpp / .h (S3.29 owns them this wave): if GlRaster must model GL_LIGHT1 beyond `all colours 0`
   (U-1957), report BLOCKED with the reason instead.
3. Oracle: the kind-5 passes' draw lists equal the original's line for line (test_terrain_render-style, from the exported recording);
   pixels of the three visible passes (1,497 / 177 / 26 px) through GlRaster and host GL; new test file port/test/test_terrain_s3_28.py.

## Gate

- The 8 cov_r_s2_screens split FAIL rows (course, radial_hover, rep_comments / financial / histograph / report1 / shortcuts, rep_roster)
  move to PASS under the split verdict.
- Full census (raw + split, 698 rows) against w8_out/census_split.tsv: 0 PASS lost and 0 rows with more differing px, in raw AND split;
  list EVERY other row that moves (scenario, checkpoint, before / after px), with the lowerEdgeCorner calls that explain it.
- Segments 205 / 205 and boot 66 / 2 / 2 on x64 and Win32 with 0 digests / rows differing from the wave 8 references.
- Controls RED: kind 5 skipped (gives back the 65 px on course), and one wrong-body mode (e.g. light 1 left with a non-zero colour, or
  one offset wrong). test_static, test_terrain_render, test_gfx, test_framegate_split pass.

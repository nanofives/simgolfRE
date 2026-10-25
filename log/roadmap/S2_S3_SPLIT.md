# S2 / S3 split (orchestrator, 2026-10-24)

S2 (2D screens) and S3 (in-game frame) both draw through the gfx backend, so they share files. These rules hold for both sections
until the orchestrator changes them.

## Shared between S2 and S3: nobody edits them directly
- port/src/sim/gfx.cpp, port/include/portsim/sim/gfx.h, port/test/gfx.py, port/test/framegate.py.
- New drawing bodies live in new files owned by one step (for example port/src/sim/gfx_<family>.cpp with its header and test).
  The hook-up lines a body needs in gfx.cpp / gfx.h (dispatch, kNotDrawing, registration) go in the step's merge notes, and the
  integrator applies them.
- S2's FIRST step is the gfx plumbing: whatever gfx.cpp change lets bodies live in separate files (a registration table or the like),
  plus S0's recommendations 1, 3 and 4 (log/roadmap/S0/MERGE.md section 2: a_log first-occurrence order without the cap, kNotDrawing
  additions, gfx.py render bounds). It is merged as S2 wave 1 as soon as it is accepted, so both sections build on it. S3's steps that
  add 2D bodies start after that commit; S3's terrain-raster steps do not wait for it.

## Census causes (re/roadmap/frame_causes.tsv, frame_census.tsv)
- S2 owns: the generic 2D primitives (Widget_writePixel and the Surface / Widget fill, line and blit primitives a screen uses), every
  cause on a terrain-no checkpoint, the NOT_RUN rows that are 2D screens, and the screen families of S2's row.
- S3 owns: `terrain not drawn` (520), Surface_blit3 sources and destinations of the in-game frame, Image_draw's SpriteRef sources
  (FLIC sprite cells, the sprite queue), golfers, ball, HUD panels, ticker text with markup, cursor and tooltip, overlay lines, S0's
  recommendations 2, 5 and 6 (terrain flag per snapshot, sg_ts_live on Win32, U-1120).
- A primitive both need belongs to S2; S3 asks for it in its report (`NEED <primitive> from S2`) instead of writing its own.

## Coordination
- Each lead reads the other's re/roadmap/S<n>_steps.tsv before spawning a worker. No file appears in both sections' files_owned.
- port/src/sim/chareditor.cpp is S1's (its merge is queued): S2's character-editor step starts after S1's commit.
- S3 owns port/src/sim/gl_raster.cpp, gl_raster.h, the terrain_* files and re/tools/glexact/ from the P5.3 handoff 4bfefc2a on.

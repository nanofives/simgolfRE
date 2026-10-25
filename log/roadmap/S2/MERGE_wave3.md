# S2 merge request, wave 3: routing map and load browser (step S2.4, accepted with k_dlg_R held; lead, 2026-10-24)

Apply AFTER wave 2 (MERGE_wave2.md / 3_shared.patch). Work is in the mirror port/build/S2_4_head (= S2_3_head + S2.4). Lead check:
on a scratch copy of today's working-tree files, 3_shared.patch then 4_shared.patch apply (scratch patchchk run, 2026-10-24).
Do not commit log/roadmap/S2/4_img/ or 4_img_base/ (rendered game frames; the repo is public) nor the *.tsv.d dirs.
Held: cov_a_keys k_dlg_R stays at 370 px, all in the `view` region (the map's tee / flag sprites need S3.4 FLIC cells and S3.5's
scaled sprite queue, the tee-to-green lines S3.8's overlay rasteriser; U-1231). Files: log/roadmap/S2/files_wave3.txt.

## 1. New files (copy from the mirror, `port/build/S2_4_head/<path>` -> `<path>`)

- port/src/sim/gfx_prim_fill.cpp    family `prim_fill` (4 bodies; exports gfxf::fill_rect16 / fill_convex_poly16 / blit_panel /
                                    blit_scale, used by gfx_routing.cpp and gfx.cpp)
- port/src/sim/gfx_prim_text2.cpp   family `prim_text2` (1 body)
- port/src/sim/gfx_routing.cpp      family `routing` (2 bodies)
- port/test/test_gfx_routing.py     35 tests

Copy them together with item 2: gfx.cpp references gfxf::blit_scale (defined in gfx_prim_fill.cpp) and a gfx_<f>.cpp missing
from gfx_families.inc fails test_static's module listing.

## 2. Shared-file edits: log/roadmap/S2/4_shared.patch

One patch (LF), `git apply log/roadmap/S2/4_shared.patch` from the repo root AFTER 3_shared.patch. Checked 2026-10-24 with
`git apply --check` on a copy of the working tree's port/src/sim with 3_shared.patch applied first: every hunk applies.

| file | base | change |
|---|---|---|
| port/src/sim/gfx.cpp | S2.3 | declaration of gfxf::blit_scale; Image_draw reports `Image_draw: a display scale other than (1, 1, 1)` and draws nothing while Display::setScale's state is not (1, 1, 1) (comment updated) |
| port/src/sim/gfx_families.inc | S2.3 | `SG_GFX_FAMILY(prim_fill)`, `SG_GFX_FAMILY(prim_text2)`, `SG_GFX_FAMILY(routing)` after credits |
| port/src/sim/routing.cpp | HEAD | includes (string, vector, execonst.h, gfx.h); struct MapDraw (the original's drawing calls of one redraw pass, only with a backend attached; texts built in g_textBuffer and its bytes put back); redraw(scroll, count, more, state, selected) issues them; negctl mode 10 (header comment of MapDraw) |
| port/src/sim/saveload.cpp | HEAD | the browser's drawing: forward declaration of gx, browser helpers (bsurf / bimage / browser_setup / browser_teardown ...), details_text(ctx, draw) with the drawing calls between its text steps, the redraw / hover / tip / click-off / loading-message calls in load_game_browser_screen; negctl mode 9 (header comment) |
| port/src/sim/tick_boot.inc | HEAD | after the five loadInfoScreenArt fonts: `gfx_drop(ctx, "loadInfoScreenArt.images", nullptr, 0)` when a backend is attached |

Nothing else: modules.inc, CMakeLists.txt, gfx.h, gfx_body.h, test_static.py unchanged (the glob picks the new .cpp files).

## 3. UNCERTAINTIES.md

Append the 5 rows of log/roadmap/S2/4_uncertain.tsv (U-1230..U-1234) after the S2.3 rows.

## 4. CLAUDE.md paragraph (after the S2.3 paragraph)

```
Roadmap S2.4, the routing map and the load browser (2026-10-24, notes log/roadmap/S2/4_notes.md, U-1230..U-1234): cov_k1_routing
`routing` and cov_a_keys `k_dlg_L` equal the original's back buffer in every pixel, x64 and Win32; cov_a_keys `k_dlg_R` (two holes
built) differs in 370 px, all in the map's hole overlay: the tee / flag sprites (FLIC sheet cells through the sprite queue's
scaled draw, S3.4 / S3.5) and the OpenGL tee-to-green lines (Terrain::drawLine, S3.8), reported as drops without a body. Three
body families: `prim_fill` (gfx_prim_fill.cpp: fillWindowRect / Win_fillRect = jgld fillRect16 with a direct colour, blitPanel
style 0 / nine-cell over Sprite::drawG, Display::setScale as state, fillConvexPoly16 with its edge walker and span filler),
`prim_text2` (drawCenteredText2: a shadow run in run-palette colour 1 at (+0, +1), entry 1 measured as RGB (0x80, 0, 0), U-1230)
and `routing` (gfx_routing.cpp: loadInfoScreenArt's whole art manifest, now drawn at boot; the map's tile diamond). Image_draw
reports a display scale other than (1, 1, 1). routing.cpp's MapDraw and saveload.cpp's browser issue the original's drawing
calls; a text the original builds in g_textBuffer is built there, drawn and the buffer put back, so the GameState with a
backend equals the one without. Gates: census 609 rows against S2.3, PASS 48 -> 50 (raw), 12 already-unsupported rows grew by 344 .. 1,012 px from
stale root content and partly drawn character-editor frames (first unsupported call unchanged); segments 205 / 205 x64 + Win32, 0 digests changed; boot
equal to S2.3; test_gfx_routing 35 x64 and Win32, test_gfx 25, test_gfx_credits 47, port test_static 6. Controls: 12 modes RED
(10 on the frames, 2 only in unit tests), 0 blind.
```

## 5. Gate commands after the merge (repo root, port/build/x64)

```
~/bin/py -3.12 -m pytest port/test/test_gfx_routing.py -q -p no:cacheprovider                         # 35 passed
env SG_BACKEND=win32 ~/bin/py -3.12 -m pytest port/test/test_gfx_routing.py -q -p no:cacheprovider    # 35 passed
~/bin/py -3.12 -m port.test.framegate cov_k1_routing --checkpoints routing --out <tsv>                 # PASS
~/bin/py -3.12 -m port.test.framegate cov_a_keys --checkpoints k_dlg_R,k_dlg_L --out <tsv>             # k_dlg_L PASS, k_dlg_R 370 px
~/bin/py -3.12 -m port.test.integrator segments --backend x64 --jobs 4 --out <tsv>                     # 205 / 205, 0 differ (same win32)
~/bin/py -3.12 -m port.test.boot --all --jobs 3 --out <tsv>                                            # as S2.3
```

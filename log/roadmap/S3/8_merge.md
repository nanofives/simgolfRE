# S3.8 merge: overlay lines (module `terrain_overlay`, bodies in the terrain family)

Own files (log/roadmap/S3/8_files.txt): port/src/sim/terrain_overlay.cpp, port/include/portsim/sim/terrain_overlay.h,
port/test/test_terrain_overlay.py (copy as they are).

Base the step was built and measured on (log/roadmap/S3/8_mirror.py): port/build/S3_6b_src (I6's tree = HEAD b3d61e41 + S6 wave 2 +
S3 waves 2 / 3 + S2 wave 5, plus S3 wave 4 = S3.6 / S3.6b with their edits), plus S3.5's own files and every edit of
log/roadmap/S3/5_merge_edits.py (EDITS, EDITS_HUD, EDITS_AFTER_S2W5). Apply after S3 wave 4 and S3.5 (none of the anchors below
touches their lines, so the order between S3.5 and S3.8 does not matter).

## 1. Shared-file edits, in apply order

The exact (old, new) pairs are log/roadmap/S3/8_edits.py (each anchor once; CRLF files get CRLF; 8_mirror.py applies them).

1. port/src/sim/modules.inc: `SG_MODULE(terrain_overlay)` after `SG_MODULE(flic)` (test entries overlay.rasterLine,
   overlay.logStart, overlay.logWrite; touch port/src/registry.cpp afterwards).
2. port/src/sim/gfx_terrain.cpp (S3.2's family): `#include "portsim/sim/terrain_overlay.h"` and five rows in kBodies:
   drawMapLine, drawMinimapSegment, setLinePoint, lineTo, drawCircle -> overlay::body_*. The overlay draws into the root surface the
   terrain GL context draws into, so its bodies sit in the terrain family (test_static wants a family per gfx_<f>.cpp file).
3. port/src/sim/gfx.cpp, b_panel_backdrop: before its text, the bar drawMapLine(half - w, y, half + w, y, 0x80000000, 0xa, 5)
   through gfxb::find_body("drawMapLine") (c3dw.cpp DrawPanelBackdrop_re's call). The comment above the function (U-1071, "not
   jgld's back buffer") is superseded by section 3 below.
4. port/src/sim/routing.cpp, hole_map (S2's routing map): setLinePoint at the tee before `showRoutingMapScreen.teeSprite`, and the two
   `showRoutingMapScreen.line` drops become lineTo(x, y, 0x80007fff, 2, 0xa) (c3et.cpp ShowRoutingMapScreen_re: setLinePoint at the
   tee, lineTo to the waypoint and to the green).

## 2. Interaction with S3.7 (gfx_text)

S3.7's tips call `drawMapLine` / `drawPanelBackdrop` through gfxb::find_body (gfx_text.cpp), and its edit 7 makes the Text_put drop
return the measured width, which sizes P4_ballLabelDraw's and the thought labels' drawMapLine bars. With S3.8 alone those bars are
8 px wide (Text_put's drop result 0): the two cov_a_keys rows that miss G2 (k_render_slash, end) are those bars, and the 13 calls
cov_b_paths' recorded stream has more than the port are tooltip bars. No anchor is shared.

## 3. Text for CLAUDE.md (after the S3 wave paragraphs)

See log/roadmap/S3/8_notes.md section 6 (one paragraph).

## 4. UNCERTAINTIES.md

The rows of log/roadmap/S3/8_uncertain.tsv (U-1370..).

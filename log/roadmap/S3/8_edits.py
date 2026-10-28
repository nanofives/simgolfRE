"""S3.8's shared-file edits (log/roadmap/S3/8_merge.md), as exact (path, old, new) replacements applied by 8_mirror.py to its source
copy only. Each anchor must occur exactly once (8_mirror.patch converts LF to the file's CRLF); an edit already present is skipped."""

EDITS = [
    # 1. the module (port/src/sim/terrain_overlay.cpp: test entry overlay.rasterLine)
    ("port/src/sim/modules.inc",
     "SG_MODULE(flic)\n",
     "SG_MODULE(flic)\nSG_MODULE(terrain_overlay)\n"),
    # 2. the overlay bodies join the terrain family (the root surface is the terrain GL context's drawing surface)
    ("port/src/sim/gfx_terrain.cpp",
     '#include "portsim/sim/terrain_live.h"\n',
     '#include "portsim/sim/terrain_live.h"\n#include "portsim/sim/terrain_overlay.h"\n'),
    ("port/src/sim/gfx_terrain.cpp",
     '    {"terrain.frameBegin", b_frame_begin},   {"terrain.frameEnd", b_frame_end},\n',
     '    {"terrain.frameBegin", b_frame_begin},   {"terrain.frameEnd", b_frame_end},\n'
     '    // roadmap S3.8: Terrain::drawLine and the exe drawers over it (terrain_overlay.h)\n'
     '    {"drawMapLine", overlay::body_draw_map_line},       {"drawMinimapSegment", overlay::body_draw_minimap_segment},\n'
     '    {"setLinePoint", overlay::body_set_line_point},     {"lineTo", overlay::body_line_to},\n'
     '    {"drawCircle", overlay::body_draw_circle},\n'),
    # 3. drawPanelBackdrop's bar: drawMapLine(half - w, y, half + w, y, 0x80000000, 0xa, 5) before its text
    ("port/src/sim/gfx.cpp",
     "  const uint32_t args[4] = {text, static_cast<uint32_t>(half), static_cast<uint32_t>(sub32(static_cast<int32_t>(a[1]), 5)),\n",
     "  // roadmap S3.8: the bar is Terrain::drawLine through the overlay body (terrain_overlay.h); without a GL context it draws nothing\n"
     "  if (GfxBody bar = gfxb::find_body(\"drawMapLine\")) {\n"
     "    const uint32_t la[7] = {static_cast<uint32_t>(sub32(half, w)), a[1], static_cast<uint32_t>(add32(half, w)), a[1], 0x80000000u, 0xau, 5u};\n"
     "    bar(g, ctx, la, 7);\n"
     "  }\n"
     "  const uint32_t args[4] = {text, static_cast<uint32_t>(half), static_cast<uint32_t>(sub32(static_cast<int32_t>(a[1]), 5)),\n"),
    # 4. the routing map's tee-to-waypoint-to-green line: setLinePoint at the tee, lineTo(x, y, 0x80007fff, 2, 0xa) to the waypoint
    #    and to the green (c3et.cpp ShowRoutingMapScreen_re, calls at the tee / waypoint / green)
    ("port/src/sim/routing.cpp",
     '    gx("showRoutingMapScreen.teeSprite", {u32(tx), u32(ty)});\n',
     '    gx("setLinePoint", {u32(tx), u32(ty)});   // roadmap S3.8: the hole line starts at the tee\n'
     '    gx("showRoutingMapScreen.teeSprite", {u32(tx), u32(ty)});\n'),
    ("port/src/sim/routing.cpp",
     '      gx("showRoutingMapScreen.line", {u32(sx), u32(sy)});\n',
     '      gx("lineTo", {u32(sx), u32(sy), 0x80007fffu, 2u, 0xau});\n'),
    ("port/src/sim/routing.cpp",
     '    gx("showRoutingMapScreen.line", {u32(gx_), u32(gy_)});\n',
     '    gx("lineTo", {u32(gx_), u32(gy_), 0x80007fffu, 2u, 0xau});\n'),
    # 5. S2.4's routing test: the hole line is no longer a drop without a body (edit 4); the lines are lineTo bodies
    ("port/test/test_gfx_routing.py",
     '    for drop in ("showRoutingMapScreen.teeSprite", "showRoutingMapScreen.line", "showRoutingMapScreen.flagSprite"):\n'
     '        assert f"no body: {drop}" in keys_run.log, drop\n',
     '    for drop in ("showRoutingMapScreen.teeSprite", "showRoutingMapScreen.flagSprite"):\n'
     '        assert f"no body: {drop}" in keys_run.log, drop\n'
     '    assert "handled\\tlineTo\\t" in keys_run.log   # roadmap S3.8: the hole line through setLinePoint / lineTo\n'),
]

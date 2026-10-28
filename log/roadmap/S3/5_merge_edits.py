"""S3.5's shared-file edits (log/roadmap/S3/5_merge.md), as exact (path, old, new) replacements applied by 5_mirror.py to its
source copy only. Each anchor must occur exactly once (5_mirror.patch converts LF to the file's CRLF)."""

EDITS = [
    # 1. the scene body family (port/src/sim/gfx_scene.cpp)
    ("port/src/sim/gfx_families.inc",
     "SG_GFX_FAMILY(terrain)\n",
     "SG_GFX_FAMILY(terrain)\nSG_GFX_FAMILY(scene)\n"),
    # 2. tick_render.inc: drawQueuedSprite's drawing goes to an attached backend after the simulation part (the original's
    #    composeGolferSprite write-back and label text are the simulation part; the blits are the backend's)
    ("port/src/sim/tick_render.inc",
     "  int drawQueuedSprite(int i) {\n"
     "    const int p7 = qa(GS(g_spriteShade), i);\n",
     "  // roadmap S3.5: with a graphics backend attached, the entry's blits (gfx_scene.cpp drawQueuedSprite.draw: the golfer\n"
     "  // palette layers read the look indices before the simulation part writes them back, as the original's compositor\n"
     "  // does) and, after the simulation part has built the name text, the golfer's label (drawQueuedSprite.label)\n"
     "  int drawQueuedSprite(int i) {\n"
     "    const uint32_t a[1] = {static_cast<uint32_t>(i)};\n"
     "    if (g_gfx_attached.load(std::memory_order_relaxed) != 0) gfx_drop(ctx, \"drawQueuedSprite.draw\", a, 1);\n"
     "    const int r = drawQueuedSpriteSim(i);\n"
     "    if (g_gfx_attached.load(std::memory_order_relaxed) != 0) gfx_drop(ctx, \"drawQueuedSprite.label\", a, 1);\n"
     "    return r;\n"
     "  }\n"
     "  int drawQueuedSpriteSim(int i) {\n"
     "    const int p7 = qa(GS(g_spriteShade), i);\n"),
    # 2a'. renderSpriteQueue's return to an attached backend (the sprite-pixel gate's capture point, gfx_scene.cpp)
    ("port/src/sim/tick_render.inc",
     "    for (int i = gs().g_spriteQueueHead; i != -1; i = qa(GS(g_spriteQueueNext), i)) drawQueuedSprite(i);\n"
     "    return 0;\n",
     "    for (int i = gs().g_spriteQueueHead; i != -1; i = qa(GS(g_spriteQueueNext), i)) drawQueuedSprite(i);\n"
     "    if (g_gfx_attached.load(std::memory_order_relaxed) != 0) gfx_drop(ctx, \"renderSpriteQueue.end\", nullptr, 0);\n"
     "    return 0;\n"),
    # 2b. P6's display-scale calls reach the backend (the scene draws set (n, n, d) and restore (zoom, zoom, 4); the HUD
    #     draws after P6 at (1, 1, 1) as in the original)
    ("port/src/sim/tick_frame.inc",
     "    // [[g_graphicsDevice] + 0x9c](1, 1, 1): the display's scale slot, drawing only (dropped; the object is the renderer's)\n",
     "    // [[g_graphicsDevice] + 0x9c](1, 1, 1): the display's scale slot, drawing only (the object is the renderer's; an attached\n"
     "    // graphics backend keeps the scale, roadmap S3.5)\n"
     "    if (g_gfx_attached.load(std::memory_order_relaxed) != 0) {\n"
     "      const uint32_t a[3] = {1, 1, 1};\n"
     "      gfx_drop(ctx, \"Display::setScale\", a, 3);\n"
     "    }\n"),
    # 2c. menu.cpp load_world_assets (after S3.6's edit 5): the palette objects the course type's loads fill
    ("port/src/sim/menu.cpp",
     "  if (g_gfx_attached.load(std::memory_order_relaxed) != 0) gfx_drop(ctx, \"loadWorldAssets.images\", nullptr, 0);\n",
     "  if (g_gfx_attached.load(std::memory_order_relaxed) != 0) gfx_drop(ctx, \"loadWorldAssets.images\", nullptr, 0);\n"
     "  // roadmap S3.5: the palette objects the type's loads fill (gfx_scene.cpp), which the sprite queue draws with\n"
     "  if (g_gfx_attached.load(std::memory_order_relaxed) != 0) gfx_drop(ctx, \"loadWorldAssets.palettes\", nullptr, 0);\n"),
    ("port/src/sim/tick.cpp",
     "#include \"portsim/sim/gfx.h\"\n",
     "#include \"portsim/sim/gfx.h\"\n#include \"portsim/execonst.h\"\n"),
    # 2d. tick_scene.inc (buildSceneSprites): Surface_drawSprite's alpha reaches the backend as its float bits (the drop's
    #     slots are 32-bit words; a float converted by value lost it), computed from the user's exe constants with a backend
    #     attached (c3fj.cpp: *tbyte * f_k_dbl_4ba818, and *tbyte * f_k_dbl_4ba478[1] + f_k_dbl_4ba478[0] for kind 4)
    ("port/src/sim/tick_scene.inc",
     "    auto scale = [&]() { return 0.0f; };  // *tbyte * [.rdata double]: only Surface_drawSprite's scale (dropped)\n",
     "    // *tbyte * f_k_dbl_4ba818: Surface_drawSprite's alpha (drawing only; read with a graphics backend attached, S3.5)\n"
     "    auto scale = [&]() {\n"
     "        if (g_gfx_attached.load(std::memory_order_relaxed) == 0) return 0.0f;\n"
     "        return static_cast<float>(static_cast<double>(*tbyte) * ctx.exe().get<double>(\"f_k_dbl_4ba818\"));\n"
     "    };\n"),
    ("port/src/sim/tick_scene.inc",
     "        kDrawSprite((u32)(static_cast<unsigned>(cell)), nullptr, G<int>(GS(f_stateptr_4c1570)), x - dx, y + 5 * (lift - dl), s,\n",
     "        uint32_t sbits = 0;\n"
     "        std::memcpy(&sbits, &s, 4);   // the float's bits in the argument slot, as the original pushes it\n"
     "        kDrawSprite((u32)(static_cast<unsigned>(cell)), nullptr, G<int>(GS(f_stateptr_4c1570)), x - dx, y + 5 * (lift - dl), sbits,\n"),
    ("port/src/sim/tick_scene.inc",
     "        const float s = 0.0f;  // *tbyte * [.rdata] + [.rdata]: only Surface_drawSprite's scale (dropped)\n",
     "        // *tbyte * f_k_dbl_4ba478[1] + f_k_dbl_4ba478[0]: Surface_drawSprite's alpha (drawing only, S3.5)\n"
     "        const float s = g_gfx_attached.load(std::memory_order_relaxed) == 0 ? 0.0f\n"
     "            : static_cast<float>(static_cast<double>(*tbyte) * ctx.exe().get<double>(\"f_k_dbl_4ba478\", 1) +\n"
     "                                 ctx.exe().get<double>(\"f_k_dbl_4ba478\", 0));\n"),
    # 2e. gfx.cpp b_image_draw: a display scale other than (1, 1, 1) (the queue's restore leaves (zoom, zoom, 4) until P6), or a
    #     sprite whose jgld palette word the scene family holds (sheet cells), is drawn by the scene family's drawB
    ("port/src/sim/gfx.cpp",
     "#include \"portsim/sim/gfx_hud.h\"\n",
     "#include \"portsim/sim/gfx_hud.h\"\n#include \"portsim/sim/gfx_scene.h\"\n"),
    ("port/src/sim/gfx.cpp",
     "  const int32_t *scale = gfxf::blit_scale(g);\n"
     "  if (scale[0] != 1 || scale[1] != 1 || scale[2] != 1) {\n"
     "    g.unsup(\"Image_draw: a display scale other than (1, 1, 1)\");\n"
     "    return;\n"
     "  }\n",
     "  // roadmap S3.5: another scale, or a sprite whose palette word the scene family holds, is the scene family's (drawB under\n"
     "  // the display scale, gfx_scene.cpp gfxs::image_draw)\n"
     "  if (gfxs::image_draw(g, ctx, a)) return;\n"),
    # 3. S3.6's hud family: the palette objects it fills, readable and writable by the scene family (NEED from S3.6)
    ("port/include/portsim/sim/gfx_hud.h",
     "#include <cstdint>\n",
     "#include <array>\n#include <cstdint>\n"),
    ("port/include/portsim/sim/gfx_hud.h",
     "void after_present(Gfx &g, Ctx &ctx);\n",
     "void after_present(Gfx &g, Ctx &ctx);\n"
     "// the RGB555 words of the palette object at `key` (a PaletteCache: loadImage's palette argument), or nullptr when no load\n"
     "// filled it; `create` makes an all-zero entry (roadmap S3.5: composeGolferSprite composes g_images[0]'s palette)\n"
     "std::array<uint16_t, 256> *palette_of(Gfx &g, uint32_t key, bool create);\n"
     "// gameMain's 0x10000-word half-bright table (the table g_halfBrightLut points at)\n"
     "const std::array<uint16_t, 0x10000> &half_bright_table();\n"),
    ("port/src/sim/gfx_hud.cpp",
     "}  // namespace portsim\n",
     "namespace gfxh {\n"
     "const std::array<uint16_t, 0x10000> &half_bright_table() { return half_bright_lut(); }\n"
     "}  // namespace gfxh\n"
     "\n"
     "}  // namespace portsim\n"),
    ("port/src/sim/gfx_hud.cpp",
     "Surface16 *family_surface(Gfx &g, uint32_t key) {\n"
     "  auto &m = st(g).s16;\n"
     "  auto it = m.find(key);\n"
     "  return it == m.end() ? nullptr : &it->second;\n"
     "}\n",
     "Surface16 *family_surface(Gfx &g, uint32_t key) {\n"
     "  auto &m = st(g).s16;\n"
     "  auto it = m.find(key);\n"
     "  return it == m.end() ? nullptr : &it->second;\n"
     "}\n"
     "\n"
     "std::array<uint16_t, 256> *palette_of(Gfx &g, uint32_t key, bool create) {\n"
     "  auto &m = st(g).palettes;\n"
     "  auto it = m.find(key);\n"
     "  if (it != m.end()) return &it->second;\n"
     "  return create ? &m[key] : nullptr;\n"
     "}\n"),
]

# Edits to S2 wave 5's port/src/sim/gfx_reports.cpp (the one registered Surface_drawSprite body, roadmap S2.13), applied after
# S2 wave 5 (5_mirror.py apply_s2w5 copies the accepted file from port/build/I6 first): the body keeps its quarter band at
# (1, 1, 1) with no mask (draw16_1002ed60, S2.8's measurement) and hands every other case on the root surface to the scene
# family's gfxs::surface_draw_sprite (every drawAlpha window, the display scale, a mask palette object), which the queue's
# 0x200 entries and buildSceneSprites reach.
EDITS_AFTER_S2W5 = [
    ("port/src/sim/gfx_reports.cpp",
     '#include "portsim/sim/commands.h"\n',
     '#include "portsim/sim/commands.h"\n#include "portsim/sim/gfx_scene.h"\n'),
    ("port/src/sim/gfx_reports.cpp",
     "  if (a[1] != g.root_va(ctx) || a[5] != 0) {\n"
     '    g.unsup("Surface_drawSprite: a destination other than the root surface or a mask");\n',
     "  // roadmap S3.5: on the root surface, a mask, an alpha outside the quarter band or a display scale other than (1, 1, 1) is\n"
     "  // drawn by the scene family (gfx_scene.h surface_draw_sprite: the window's flag-set member, the scale, the mask's palette)\n"
     "  if (a[1] == g.root_va(ctx)) {\n"
     "    float al;\n"
     "    std::memcpy(&al, &a[4], 4);\n"
     "    const double ad = static_cast<double>(al);\n"
     "    const int32_t *sc0 = gfxf::blit_scale(g);\n"
     "    const bool quarter = ad >= 0.25 - 0.03125 && ad <= 0.25 + 0.03125;\n"
     "    if (a[5] != 0 || (!quarter && ad >= 0.0 && ad <= 1.0) || sc0[0] != 1 || sc0[1] != 1 || sc0[2] != 1) {\n"
     "      gfxs::surface_draw_sprite(g, ctx, a);\n"
     "      return;\n"
     "    }\n"
     "  }\n"
     "  if (a[1] != g.root_va(ctx) || a[5] != 0) {\n"
     '    g.unsup("Surface_drawSprite: a destination other than the root surface or a mask");\n'),
]


# Edits to S3.6's port/src/sim/gfx_hud.cpp (apply after S3.6's merge): the sprite-queue figures its panels draw through
# addSprite, now drawn by the scene family (gfxs::add_sprite): showInfoCursorPanel's hole flag and drawAmenityRow's amenities
# (drawAmenityRow's full argument list from c3ei.cpp DrawAmenitiesPanel_re, the ids, cells and p2 / p5 steps of DrawAmenityRow_re).
EDITS_HUD = [
    ('port/src/sim/gfx_hud.cpp',
     '#include "gfx_body.h"\n',
     '#include "gfx_body.h"\n#include "portsim/sim/gfx_scene.h"\n'),
    ('port/src/sim/gfx_hud.cpp',
     'int32_t amenity_row(Gfx &g, Ctx &ctx, int32_t p1, int32_t p2, int32_t p3, int32_t p7, int32_t p8, uint32_t p10) {\n  const uint32_t root = g.root_va(ctx);\n  if (p2 == 0x1a3) p3 = mul32(p3, 3);\n  const int32_t n = p3, n47 = mul32(47, n);\n  const int32_t x0 = add32(sub32(p1, sar32(add32(n47, (n47 >> 31) & 3), 2)), 0x220);\n  const int32_t half = sar32(add32(n, (n >> 31) & 1), 1);\n  int32_t x = x0;\n  const uint32_t strip = va(ctx, GSO(g_sprites55e930, 0)), smask = va(ctx, GSO(g_sprites542e50, 0));\n  auto piece = [&](uint32_t b) {\n    call(g, ctx, "Surface_stretchBlit", {strip + 0x2cu * b, smask + 0x2cu * b, root, static_cast<uint32_t>(x), 0x1dd, 0});\n  };\n  for (int32_t i = 0; i < half; ++i) {\n    piece(i != 0 ? 1u : 0u);\n    x = add32(x, i != 0 ? 0x2f : 0x44);\n  }\n  piece(static_cast<uint32_t>(n & 1) | 2u);\n  int32_t hover = -1, A = 0;\n  bool queued = false;\n  for (int32_t i = 0; i < n; ++i, A = add32(A, 0x2f)) {\n    if (p10 != 0 && (p10 & (1u << (static_cast<uint32_t>(i) & 31u))) == 0) continue;\n    const int32_t px = add32(add32((A + (A < 0 ? 1 : 0)) / 2, x0), 0x2c);\n    const int32_t py = 0x1f4 + ((i & 1) ? 0x1b : 0);\n    if (approx_distance(sub32(p7, px), mul32(sub32(p8, py), 2)) < 0x14) hover = i;\n    uint32_t k = 3;\n    if (hover == i) k = 0;\n    if (ctx.mem.load<int32_t>(va(ctx, GSO(g_panelState, 0x10))) == i) k = 1;\n    const uint32_t b[8] = {strip + ((i & 1) ? 0x160u : 0xb0u) + 0x2cu * k, root, static_cast<uint32_t>(sub32(px, 0x16)),\n                           static_cast<uint32_t>(sub32(py, 0x14)), 1, 1, 1, 0};\n    draw_scaled(g, ctx, b, 1, 1, 1, 0, "drawAmenityRow");\n    queued = true;\n  }\n  if (queued) g.unsup("addSprite: the sprite queue (drawAmenityRow\'s amenities)");\n  return hover;\n}\n',
     'int32_t amenity_row(Gfx &g, Ctx &ctx, int32_t p1, int32_t p2, int32_t p3, int32_t p4, int32_t p5, int32_t p6, int32_t p7,\n                    int32_t p8, int32_t p9, uint32_t p10, uint32_t p11) {\n  const uint32_t root = g.root_va(ctx);\n  if (p2 == 0x1a3) p3 = mul32(p3, 3);\n  const int32_t n = p3, n47 = mul32(47, n);\n  const int32_t x0 = add32(sub32(p1, sar32(add32(n47, (n47 >> 31) & 3), 2)), 0x220);\n  const int32_t half = sar32(add32(n, (n >> 31) & 1), 1);\n  int32_t x = x0;\n  const uint32_t strip = va(ctx, GSO(g_sprites55e930, 0)), smask = va(ctx, GSO(g_sprites542e50, 0));\n  auto piece = [&](uint32_t b) {\n    call(g, ctx, "Surface_stretchBlit", {strip + 0x2cu * b, smask + 0x2cu * b, root, static_cast<uint32_t>(x), 0x1dd, 0});\n  };\n  for (int32_t i = 0; i < half; ++i) {\n    piece(i != 0 ? 1u : 0u);\n    x = add32(x, i != 0 ? 0x2f : 0x44);\n  }\n  piece(static_cast<uint32_t>(n & 1) | 2u);\n  // roadmap S3.5: each row\'s amenity is a sprite-sheet cell drawn through addSprite (c3ei.cpp DrawAmenityRow_re): p9 == -1\n  // takes 0xf0 for p6 >= -2, else 0x8c; id = B + p2 (B += p4 per row); for p3 == 1 and id != 0x168 first the cell of sheet\n  // k2 = id + 3 (id + 1 for 0x1ec / 0x1ca), then the cell of sheet id, both at column (its columns - 1), row 0 (cellAt, read\n  // only), drawn by gfxs::add_sprite(px, py, 0, p9, p9, cell, p6, i + p5 [| 0x400 for p11\'s bit i]); p2 / p5 then step as the\n  // original\'s tail does (0x1a9 -> 0x1af, 0x1a3 -> 0x1a9, 0x226: p5 - 1)\n  if (p9 == -1) p9 = p6 >= -2 ? 0xf0 : 0x8c;\n  auto cols = [&](int32_t s) {\n    return ctx.mem.load<int32_t>(state_va(ctx, &GameState::g_sheetCols) + 4u * static_cast<uint32_t>(s));\n  };\n  auto cell = [&](int32_t s) {\n    return cm::call_peer(ctx, "cellAt", {static_cast<uint32_t>(s), static_cast<uint32_t>(sub32(cols(s), 1)), 0}).eax;\n  };\n  int32_t hover = -1, A = 0, B = 0;\n  for (int32_t i = 0; i < n; ++i, A = add32(A, 0x2f), B = add32(B, p4)) {\n    if (p10 != 0 && (p10 & (1u << (static_cast<uint32_t>(i) & 31u))) == 0) continue;\n    const int32_t id = add32(B, p2);\n    const int32_t px = add32(add32((A + (A < 0 ? 1 : 0)) / 2, x0), 0x2c);\n    const int32_t py = 0x1f4 + ((i & 1) ? 0x1b : 0);\n    if (approx_distance(sub32(p7, px), mul32(sub32(p8, py), 2)) < 0x14) hover = i;\n    uint32_t k = 3;\n    if (hover == i) k = 0;\n    if (ctx.mem.load<int32_t>(va(ctx, GSO(g_panelState, 0x10))) == i) k = 1;\n    const uint32_t b[8] = {strip + ((i & 1) ? 0x160u : 0xb0u) + 0x2cu * k, root, static_cast<uint32_t>(sub32(px, 0x16)),\n                           static_cast<uint32_t>(sub32(py, 0x14)), 1, 1, 1, 0};\n    draw_scaled(g, ctx, b, 1, 1, 1, 0, "drawAmenityRow");\n    if (p3 == 1 && id != 0x168) {\n      const int32_t k2 = (id == 0x1ec || id == 0x1ca) ? add32(id, 1) : add32(id, 3);\n      const int32_t v[8] = {px, py, 0, p9, p9, static_cast<int32_t>(cell(k2)), p6, add32(i, p5)};\n      gfxs::add_sprite(g, ctx, v);\n    }\n    int32_t a8 = add32(i, p5);\n    if (p11 & (1u << (static_cast<uint32_t>(i) & 31u))) a8 |= 0x400;\n    const int32_t v[8] = {px, py, 0, p9, p9, static_cast<int32_t>(cell(id)), p6, a8};\n    gfxs::add_sprite(g, ctx, v);\n    if (p2 == 0x1a9) {\n      const int32_t hi = static_cast<int32_t>((static_cast<int64_t>(add32(n, n)) * 0x55555556LL) >> 32);\n      const int32_t q = hi + static_cast<int32_t>(static_cast<uint32_t>(hi) >> 31);\n      if (i + 1 == q) {\n        const int32_t h2 = static_cast<int32_t>((static_cast<int64_t>(n) * 0x55555555LL) >> 32);\n        const int32_t s = sar32(sub32(h2, n), 1);\n        p2 = 0x1af;\n        p5 = add32(p5, s + static_cast<int32_t>(static_cast<uint32_t>(s) >> 31));\n      }\n    } else if (p2 == 0x1a3) {\n      const int32_t hi = static_cast<int32_t>((static_cast<int64_t>(n) * 0x55555556LL) >> 32);\n      const int32_t q = hi + static_cast<int32_t>(static_cast<uint32_t>(hi) >> 31);\n      if (i + 1 == q) {\n        p2 = 0x1a9;\n        p5 = sub32(p5, q);\n      }\n    } else if (p2 == 0x226) {\n      p5 = sub32(p5, 1);\n    }\n  }\n  call(g, ctx, "Display::setScale", {1, 1, 1});   // the display slot 0x9c gets (1, 1, 1) at the end, as in the original\n  return hover;\n}\n'),
    ('port/src/sim/gfx_hud.cpp',
     '  if (M == 1) amenity_row(g, ctx, -0x54, 0x208, 5, a, b, 0);\n  else if (M == 4 && mask != 0 && amenity_row(g, ctx, 0, 0x168, 0xe, a, b, mask) != -1)',
     '  if (M == 1) amenity_row(g, ctx, -0x54, 0x208, 5, M, 0xaa, 4, a, b, 0x28, 0, 0);\n  else if (M == 4 && mask != 0 && amenity_row(g, ctx, 0, 0x168, 0xe, 1, 0x64, -4, a, b, -1, mask, 0) != -1)'),
    ('port/src/sim/gfx_hud.cpp',
     '  else if (M == 0x13) amenity_row(g, ctx, -0x10, 0x226, 8, a, b, 0);\n',
     '  else if (M == 0x13) amenity_row(g, ctx, -0x10, 0x226, 8, 1, 0xa9, 4, a, b, 0x28, 0, 0);\n'),
    ('port/src/sim/gfx_hud.cpp',
     '  else if (M == 2) amenity_row(g, ctx, 0xd, 0x1a3, 5, a, b, 0);\n',
     '  else if (M == 2) amenity_row(g, ctx, 0xd, 0x1a3, 5, 0, 0x2d, -6, a, b, 0x28, 0, 0);\n'),
    ('port/src/sim/gfx_hud.cpp',
     '  else if (M == 0x10) amenity_row(g, ctx, 0x64, 0x12f, 7, a, b, 0);\n',
     '  else if (M == 0x10) amenity_row(g, ctx, 0x64, 0x12f, 7, 1, 0xb4, 2, a, b, 0x64, 0, 0);\n'),
    ('port/src/sim/gfx_hud.cpp',
     '  if (ctx.mem.load<uint32_t>(rec + 0x18) == 0 && n < 0x13) g.unsup("addSprite: the sprite queue (showInfoCursorPanel\'s hole flag)");\n',
     '  if (ctx.mem.load<uint32_t>(rec + 0x18) == 0 && n < 0x13) {\n    // roadmap S3.5: the hole flag, a sprite-sheet cell (showInfoCursorPanel, cellAt + addSprite): sheet c + 0x189 with c the\n    // signed g_courseType, column g_date % its columns when the tool f_w_4c2848 is 1 (else 0), row 3; addSprite(point 1 x +\n    // 0x1f, point 1 y + 0x18, 0, 0x28, 0x28, cell, 4, 0x63), drawn at once by the scene family (gfxs::add_sprite)\n    const int32_t sheet = add32(static_cast<int32_t>(gs.g_courseType), 0x189);\n    const int32_t fc = ctx.mem.load<int32_t>(state_va(ctx, &GameState::g_sheetCols) + 4u * static_cast<uint32_t>(sheet));\n    const int32_t col = tool == 1 ? mod32(gs.g_date, fc) : 0;\n    const uint32_t c = cm::call_peer(ctx, "cellAt", {static_cast<uint32_t>(sheet), static_cast<uint32_t>(col), 3}).eax;\n    const int32_t v[8] = {add32(pt(1, 0), 0x1f), add32(pt(1, 1), 0x18), 0, 0x28, 0x28, static_cast<int32_t>(c), 4, 0x63};\n    gfxs::add_sprite(g, ctx, v);\n  }\n'),
]

# S3.11 integration fixes, exact text (path relative to the repo root, old, new), applied by 11_assemble.py after every step's edits.
# Each is in an S3-owned file and is named in log/roadmap/S3/11_notes.md section 2.
#
# 1-2. S3.5 x S3.4: test_flic's hook-up test (test_sheet_to_gfx_draws_the_recorded_cells) drew a sheet cell with Image_draw and no
#      mask and expected the slot palette (S3.4's assumption while U-1332 was open). S3.5 resolved U-1332: a sheet cell draws with a
#      palette OBJECT (its queue entry's g_images palette; blt16a / blt16c store the key's palette word), and gfxs::image_draw reports a
#      cell-pool sprite with no palette word and no mask as `a sprite with no palette` instead of drawing it, so the test failed in
#      every tree holding S3.5 (S3.5b's own mirror port/build/S3_5b_src included; S3.5 / S3.5b's gates did not run test_flic). Fix in
#      S3.5's favour: a TEST-ONLY body gfx_scene.setPalette(key, words) fills a palette object, and the test draws each cell with the
#      slot palette given as that object (Image_draw's mask = drawB's key for the call); without the scene family the mask stays 0.
EDITS = [
    ("port/src/sim/gfx_scene.cpp",
     '  gfxb::put_file(ctx, "$gfx/scene_spriteinfo.bin", o);\n'
     "}\n",
     '  gfxb::put_file(ctx, "$gfx/scene_spriteinfo.bin", o);\n'
     "}\n"
     "\n"
     "// TEST-ONLY gfx_scene.setPalette(key, words): the palette object `key` (gfxh::palette_of) gets the 256 RGB555 words at `words`,\n"
     "// as a loadImage with a palette argument would fill it; lets a test draw a sheet cell with a mask palette (roadmap S3.11:\n"
     "// test_flic's hook-up test, U-1332)\n"
     "void b_set_palette(Gfx &g, Ctx &ctx, const uint32_t *a, int) {\n"
     "  std::array<uint16_t, 256> &p = *gfxh::palette_of(g, a[0], true);\n"
     "  for (uint32_t i = 0; i < 256; ++i) p[i] = ctx.mem.load<uint16_t>(a[1] + 2 * i);\n"
     "}\n"),
    ("port/src/sim/gfx_scene.cpp",
     '    {"gfx_scene.spriteInfo", b_sprite_info},\n'
     "};",
     '    {"gfx_scene.spriteInfo", b_sprite_info},\n'
     '    {"gfx_scene.setPalette", b_set_palette},\n'
     "};"),
    ("port/test/test_flic.py",
     "        cell0 = st.data_base + 0xE9A30 + 0x2C * 7          # g_cellPool's offset in the GameState\n"
     "        name_va = NAME_VA + len(ld[\"path\"]) + 1\n"
     "        drawn = 0\n"
     "        for k, c in enumerate(ld[\"cells\"][:4]):\n"
     "            if c[\"ret\"] != 0:\n"
     "                continue\n"
     "            assert st.call(\"gfx.call\", [name_va, cell0 + 0x2C * k, 0, 0, 0, 0] + [0] * 6).eax == 1\n",
     "        cell0 = st.data_base + 0xE9A30 + 0x2C * 7          # g_cellPool's offset in the GameState\n"
     "        name_va = NAME_VA + len(ld[\"path\"]) + 1\n"
     "        lut = np.array([((pal[3 * i] >> 3) << 10) | ((pal[3 * i + 1] >> 3) << 5) | (pal[3 * i + 2] >> 3) for i in range(256)],\n"
     "                       np.uint16)\n"
     "        # roadmap S3.11: with the scene family in the tree a sheet cell draws with a palette OBJECT (its queue entry's, U-1332\n"
     "        # resolved by S3.5), not with the slot palette, so the cell is drawn with the slot palette given as Image_draw's mask\n"
     "        # (gfx_scene.setPalette fills the object); without the scene family the mask stays 0 and the core path draws as before\n"
     "        sp = b\"gfx_scene.setPalette\\0\"\n"
     "        ctypes.memmove(ctypes.addressof(buf) + 0x400, sp, len(sp))\n"
     "        ctypes.memmove(ctypes.addressof(buf) + 0x800, lut.tobytes(), 0x200)\n"
     "        pal_key = NAME_VA + 0xF00\n"
     "        mask = pal_key if st.call(\"gfx.call\", [NAME_VA + 0x400, pal_key, NAME_VA + 0x800] + [0] * 9).eax == 1 else 0\n"
     "        drawn = 0\n"
     "        for k, c in enumerate(ld[\"cells\"][:4]):\n"
     "            if c[\"ret\"] != 0:\n"
     "                continue\n"
     "            assert st.call(\"gfx.call\", [name_va, cell0 + 0x2C * k, 0, 0, 0, mask] + [0] * 6).eax == 1\n"),
    ("port/test/test_flic.py",
     "            region = img[top:top + kh, left:left + kw]\n"
     "            lut = np.array([((pal[3 * i] >> 3) << 10) | ((pal[3 * i + 1] >> 3) << 5) | (pal[3 * i + 2] >> 3) for i in range(256)],\n"
     "                           np.uint16)\n"
     "            mask = px < 0xFE\n"
     "            assert (region[mask] == lut[px[mask]]).all(), (k, ld[\"path\"])\n",
     "            region = img[top:top + kh, left:left + kw]\n"
     "            keep = px < 0xFE\n"
     "            assert (region[keep] == lut[px[keep]]).all(), (k, ld[\"path\"])\n"),
]

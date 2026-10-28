"""S3.5b shared-file edits (exact (path, old, new) pairs, applied by 5b_mirror.py after 5_merge_edits.py's lists and written
out in 5b_merge.md). LF files; the mirror's patch() is CRLF-aware."""

EDITS_SPAWN_NOT_APPLIED = [
    # 1. drawWorldObjects' spawn zoom-in (an animal record with flags < 0), dropped before
    ("port/src/sim/tick_world.inc",
     "          // the spawn zoom-in: Surface_drawSprite with scale k * [.rdata] + [.rdata] (drawing, dropped)\n"
     "          a.flags() += 1;\n",
     "          // the spawn zoom-in (drawWorldObjects+0x4309fe..+0x430a3c): Surface_drawSprite(cell, root, sx - 0x1e,\n"
     "          // sy + rise[k] * 4 - 0x28, alpha, g_images[0x78 + kind], 1) with k = flags + 0x14 and the alpha the float\n"
     "          // k * f_k_dbl_4ba478[1] + f_k_dbl_4ba478[0]; its bits reach the backend only (0 without one, as buildSceneSprites)\n"
     "          {\n"
     "            const int k = a.flags() + 0x14;\n"
     "            uint32_t abits = 0;\n"
     "            if (g_gfx_attached.load(std::memory_order_relaxed) != 0) {\n"
     "              const float al = static_cast<float>(static_cast<double>(k) * ctx.exe().get<double>(\"f_k_dbl_4ba478\", 1) +\n"
     "                                                  ctx.exe().get<double>(\"f_k_dbl_4ba478\", 0));\n"
     "              std::memcpy(&abits, &al, 4);\n"
     "            }\n"
     "            tc(\"Surface_drawSprite\", cell, Root(), sub32(sx.get(), 0x1e),\n"
     "               sub32(add32(sy.get(), mul32(kSpawnRise[k], 4)), 0x28), static_cast<int>(abits),\n"
     "               static_cast<int>(GS(g_images) + 0x58u * static_cast<uint32_t>(0x78 + a.kind())), 1);\n"
     "          }\n"
     "          a.flags() += 1;\n"),
]

EDITS = [
    # 2. placeAnimal's queue entry: the standing figure of an animal record (U-1349 b), dropped before
    ("port/src/sim/golfers_world.inc",
     "      gs.g_animalCell = cell;                 // drawing state (the sprite queue itself is the renderer's)\n"
     "      (void)n;\n",
     "      gs.g_animalCell = cell;                 // drawing state\n"
     "      // placeAnimal queue entry: enqueueSprite(sx, sy, sy, 0x1e, 0x28, cell, 4, (n + 0x9c) | 0x100), the animal's standing figure\n"
     "      // (a golfer-palette entry past the golfer table); queued only for an attached graphics backend, so the vectors that\n"
     "      // judge the simulation (and every run without a backend) see no sprite queue write\n"
     "      if (g_gfx_attached.load(std::memory_order_relaxed) != 0 && gl::registered(\"enqueueSprite\"))\n"
     "        gl::call_peer(ctx, \"enqueueSprite\", {u32(sx), u32(sy), u32(sy), 0x1eu, 0x28u, u32(cell), 4u, u32((n + 0x9c) | 0x100)});\n"),
    ("port/src/sim/golfers.cpp",
     "#include \"portsim/sim/bridge.h\"\n",
     "#include \"portsim/sim/bridge.h\"\n#include \"portsim/sim/gfx.h\"\n"),
]

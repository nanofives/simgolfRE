"""S3.2's shared-file edits as exact (file, old, new) replacements; log/roadmap/S3/2_merge.md shows the same text for the integrator,
2_mirror.py applies them to its source copy (each anchor must occur exactly once). Line ends: the file's own (CRLF handled)."""

EDITS = [
    # 1. the terrain body family
    ("port/src/sim/gfx_families.inc",
     "SG_GFX_FAMILY(selftest)\n",
     "SG_GFX_FAMILY(selftest)\nSG_GFX_FAMILY(terrain)\n"),
    # 2. gfx_body.h: the per-snapshot terrain flag (S0 recommendation 2)
    ("port/src/sim/gfx_body.h",
     "  std::vector<std::string> notes;       // every distinct unsupported reason, in first-occurrence order (gfx.log `first`)\n",
     "  std::vector<std::string> notes;       // every distinct unsupported reason, in first-occurrence order (gfx.log `first`)\n"
     "  // terrain on the surfaces (roadmap S3.2, S0 recommendation 2): set on the root surface by a terrain pass or the restore of\n"
     "  // the terrain picture (gfx_terrain.cpp), cleared by a 2D write that covers the whole root surface, copied to the back buffer\n"
     "  // at Window::refreshFull; each snapshot keeps the back buffer's (gfx.log `snapshot_terrain`)\n"
     "  int root_terrain = 0, back_terrain = 0;\n"
     "  std::vector<int> snap_terrain;\n"
     "  // a 2D write of the rectangle (x, y, w, h) onto the root surface: clears root_terrain when it covers the whole surface\n"
     "  void note_root_write(int32_t x, int32_t y, int32_t w, int32_t h) {\n"
     "    if (x <= 0 && y <= 0 && static_cast<int64_t>(x) + w >= root.w && static_cast<int64_t>(y) + h >= root.h) root_terrain = 0;\n"
     "  }\n"),
    # 3. gfx.cpp: refreshFull copies the flag
    ("port/src/sim/gfx.cpp",
     "  if (SG_NEGCTL(g_negctl_gfx) == 4) return;\n  g.back.px = g.root.px;\n",
     "  if (SG_NEGCTL(g_negctl_gfx) == 4) return;\n  g.back.px = g.root.px;\n  g.back_terrain = g.root_terrain;\n"),
    # 4. gfx.cpp: the full-screen 2D copies clear it (Surface_blit3, Surface_fillRegion)
    ("port/src/sim/gfx.cpp",
     "  copy8(it->second, g.root, x, y, w, h, x2, y2);\n}\n",
     "  copy8(it->second, g.root, x, y, w, h, x2, y2);\n  g.note_root_write(x2, y2, w, h);\n}\n"),
    ("port/src/sim/gfx.cpp",
     "  copy8(it->second, g.root, x, y, static_cast<int32_t>(a[4]), static_cast<int32_t>(a[5]), x, y);\n",
     "  copy8(it->second, g.root, x, y, static_cast<int32_t>(a[4]), static_cast<int32_t>(a[5]), x, y);\n"
     "  g.note_root_write(x, y, static_cast<int32_t>(a[4]), static_cast<int32_t>(a[5]));\n"),
    # 5. gfx.cpp: each snapshot keeps the back buffer's flag
    ("port/src/sim/gfx.cpp",
     "    g->snap_unsupported.push_back(g->unsupported.size());\n",
     "    g->snap_unsupported.push_back(g->unsupported.size());\n    g->snap_terrain.push_back(g->back_terrain);\n"),
    # 6. gfx.cpp: gfx.log writes it
    ("port/src/sim/gfx.cpp",
     "    s += \"snapshot\\t\" + std::to_string(k) + \"\\tunsupported before it\\t\" + std::to_string(g.snap_unsupported[k]) + \"\\n\";\n",
     "    s += \"snapshot\\t\" + std::to_string(k) + \"\\tunsupported before it\\t\" + std::to_string(g.snap_unsupported[k]) + \"\\n\";\n"
     "  for (size_t k = 0; k < g.snap_terrain.size(); ++k)\n"
     "    s += \"snapshot_terrain\\t\" + std::to_string(k) + \"\\t\" + std::to_string(g.snap_terrain[k]) + \"\\n\";\n"),
    # 7. framegate.py: LiveTerrain on both backends (terrainlive.live_*: ctypes on x64, the runner's ops 28..32 on Win32), and the
    #    gfx.log terrain flag as its own column
    ("port/test/framegate.py",
     "record with the capture's framehash.bin, as the boot gate does. On the Win32 backend (SG_BACKEND=win32) the runner has no\n"
     "sg_ts_live op, so LiveTerrain is not attached there (column liveterrain = n/a); nothing in a row's status or pixels depends on it at\n"
     "HEAD (terrain_state's draw passes set only the view quadrant: no terrain pixel reaches the back buffer).\n",
     "record with the capture's framehash.bin, as the boot gate does. On the Win32 backend (SG_BACKEND=win32) the runner's ops 28..32\n"
     "carry the sg_ts_live calls (roadmap S3.2, port/test/terrainlive.py live_*). With LiveTerrain and gfx attached the terrain passes\n"
     "are rasterised into the backend's root surface (the terrain body family, port/src/sim/gfx_terrain.cpp); gfx.log's\n"
     "snapshot_terrain gives column terrain_drawn.\n"),
    ("port/test/framegate.py",
     '           "unsup_order", "boot_status", "boot_records", "liveterrain", "backend", "variant", "reference"]\n',
     '           "unsup_order", "boot_status", "boot_records", "liveterrain", "backend", "variant", "reference", "terrain_drawn"]\n'),
    ("port/test/framegate.py",
     '    use_live = live and backend == "x64" and lib.has("gfx.attach")\n',
     '    use_live = live and lib.has("gfx.attach")\n'),
    ("port/test/framegate.py",
     '    if use_live:\n        _bind_live(lib.dll)\n',
     '    from port.test import terrainlive as TLm\n    if use_live and getattr(lib, "dll", None) is not None:\n        _bind_live(lib.dll)\n'),
    ("port/test/framegate.py",
     '            if lib.dll.sg_ts_live_attach(st.ctx, va, None, 0) != 0:\n',
     '            if TLm.live_attach(lib, st, va) != 0:\n'),
    ("port/test/framegate.py",
     '            n = C.c_size_t(0)\n'
     '            lib.dll.sg_ts_live_log(st.ctx, None, 0, C.byref(n))\n'
     '            b = C.create_string_buffer(n.value + 1)\n'
     '            lib.dll.sg_ts_live_log(st.ctx, b, n.value + 1, C.byref(n))\n'
     '            names = [ln.split()[3] for ln in b.raw[:n.value].decode().splitlines() if len(ln.split()) > 3]\n'
     '            live_info.update(attached=f"0x{lib.dll.sg_ts_live_object(st.ctx):x}", calls=len(names),\n',
     '            names = [ln.split()[3] for ln in TLm.live_log(lib, st).splitlines() if len(ln.split()) > 3]\n'
     '            live_info.update(attached=f"0x{TLm.live_object(lib, st):x}", calls=len(names),\n'),
    ("port/test/framegate.py",
     '            lib.dll.sg_ts_live_detach(st.ctx)\n',
     '            TLm.live_detach(lib, st)\n'),
    ("port/test/framegate.py",
     '        if i in glog["snap_terrain"]:\n'
     '            row["terrain_rule"] += f"; gfx snapshot_terrain={glog[\'snap_terrain\'][i]}"\n',
     '        if i in glog["snap_terrain"]:\n'
     '            row["terrain_rule"] += f"; gfx snapshot_terrain={glog[\'snap_terrain\'][i]}"\n'
     '            row["terrain_drawn"] = glog["snap_terrain"][i]\n'),
]

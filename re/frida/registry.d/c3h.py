# C3 batch c3h (2026-10-06): render-subsystem leaves, writers and table lookups, reimplemented in
# shim/src/re/c3h.cpp. Format documented at the top of re/frida/hooks_registry.py. Keys are the hooks.csv
# names (Table483::find carries `::`; diff_hook writes its CSV as Table483_find). Writers declare their
# `state` regions so the A/B snapshots, compares and restores them.

# brightenComponent: component values straddling the 0xdf saturation boundary (and out-of-range probes).
_BRIGHT = [-0x80000000, -1, 0, 1, 0x20, 0x7f, 0xa0, 0xde, 0xdf, 0xe0, 0xfe, 0xff, 0x100, 0x7fffffff]

# setGrids / setLinePoint writers: in-range tile coordinates (0..49) with varied byte payloads.
_GRIDS = [(x, y, a, b) for x in (0, 1, 7, 24, 48, 49) for y in (0, 2, 13, 25, 49)
          for a, b in ((0, 0xff), (0x3c, 0x11), (0x7f, 0x80))]
_PAIRS = [(a, b) for a in (-0x80000000, -1, 0, 1, 0x123, 0x7fffffff) for b in (-1, 0, 0x456, 0x7fffffff)]

# Table483::find keys: match each used slot, the free (-1) slot, and keys that fall through it.
_KEYS = [(k,) for k in (10, 20, 30, 40, 50, 60, -1, 0, 0x7fffffff)]

# tileToScreen tile coordinates: on-map cells that land on each cache branch, plus off-map probes.
_TILES = [(x, y) for x in (0, 1, 2, 10, 24, 25, 48, 49) for y in (0, 1, 2, 13, 26, 48, 49)]
_TILES += [(-1, 0), (0, -1), (50, 10), (10, 50), (-0x80000000, 0), (0x7fffffff, 0)]

HOOKS = {}
HOOKS.update({
    # --- pure leaf (no state, no fixture) ---
    "brightenComponent": dict(module="golf_clean.exe", addr=0x00461810, abi="default", ret="int", args=["int"],
                              vectors=[(v,) for v in _BRIGHT]),

    # --- writers (state = the regions each one mutates) ---
    "setGrids": dict(module="golf_clean.exe", addr=0x0040A130, abi="default", ret="void",
                     args=["int", "int", "int", "int"],
                     state=[(0x005830B8, 0, 2500), (0x0059C090, 0, 2500)], vectors=_GRIDS),
    "setLinePoint": dict(module="golf_clean.exe", addr=0x0040BF00, abi="default", ret="void", args=["int", "int"],
                         state=[(0x005A412C, 0, 8)], vectors=_PAIRS),

    # --- thiscall table reader: two tables so both the key-match and the free-slot/not-found branches are covered ---
    "Table483::find": dict(module="golf_clean.exe", addr=0x004833F0, abi="thiscall", ret="int",
                           args=["pointer", "int"], fixture="c3h_table",
                           vectors=[("$obj", k) for (k,) in _KEYS]),
    "Table483::find_full": dict(module="golf_clean.exe", addr=0x004833F0, abi="thiscall", ret="int",
                                args=["pointer", "int"], fixture="c3h_table_full",
                                vectors=[("$obj", k) for (k,) in _KEYS]),

    # --- reader+writer: the two short caches plus the two out-params are the state regions ---
    "tileToScreen": dict(module="golf_clean.exe", addr=0x0042F940, abi="default", ret="int",
                         args=["int", "int", "pointer", "pointer"], fixture="c3h_tiles",
                         state=[(0x0055EB40, 0, 5000), (0x0055FEC8, 0, 5000), ("$sx", 0, 4), ("$sy", 0, 4)],
                         vectors=[(x, y, "$sx", "$sy") for x, y in _TILES]),

    # --- surface methods: c3i_surface gives the surface a non-null fake DirectDraw object (one fake vtable) so the
    #     format/fill/blit path runs. $obj is a fake Surf473 whose m_4 (+4) points at fakeDD; $znull has m_4 == 0 for
    #     the "return 7" guard; $rec (0x40) is the state region the vtable callbacks record their arguments into.
    #     Surface_blit: a null source -> 0x10, a source with m_4 == 0 -> 7, and a live fake source -> the format()
    #     virtual (vtable+0xe4) runs and reports depth 16 (!= 8), so it returns 0 (the 8-bit blitTo path needs a real
    #     DirectDraw surface; the 100% body match covers it). Three distinct results. ---
    "Surface_blit": dict(module="golf_clean.exe", addr=0x00473BF0, abi="thiscall", ret="int",
                         args=["pointer", "pointer", "int", "int", "int", "int", "int", "int"], fixture="c3i_surface",
                         state=[("$rec", 0, 0x40)],
                         vectors=[("$obj", 0, 1, 2, 3, 4, 5, 6), ("$obj", 0, 9, 9, 9, 9, 9, 9),
                                  ("$obj", "$znull", 1, 2, 3, 4, 5, 6), ("$obj", "$znull", 7, 8, 9, 10, 11, 12),
                                  ("$obj", "$obj", 1, 2, 3, 4, 5, 6), ("$obj", "$obj", 10, 20, 30, 40, 50, 60),
                                  ("$obj", "$obj", -1, -2, -3, -4, -5, -6), ("$obj", "$obj", 0, 0, 0, 0, 0, 0),
                                  ("$obj", "$obj", 100, 200, 300, 400, 500, 600)]),
    # Surface_fillRegion: null source -> 0x10; this->m_4 or s->m_4 null -> 7; otherwise the fill virtual (this->m_4
    # vtable+0x40) runs with the rect {x,y,x+w,y+h} as source and dest, records into $rec and returns 0x2a. 10 vectors
    # (leaf gate needs >= 10): two 0x10, two 7, six fill with distinct rects -> distinct final states and the fill path.
    "Surface_fillRegion": dict(module="golf_clean.exe", addr=0x00475C90, abi="thiscall", ret="int",
                               args=["pointer", "pointer", "int", "int", "int", "int"], fixture="c3i_surface",
                               state=[("$rec", 0, 0x40)],
                               vectors=[("$obj", 0, 1, 2, 3, 4), ("$obj", 0, 5, 6, 7, 8),
                                        ("$znull", "$obj", 1, 2, 3, 4), ("$obj", "$znull", 1, 2, 3, 4),
                                        ("$obj", "$obj", 10, 20, 30, 40), ("$obj", "$obj", 0, 0, 5, 5),
                                        ("$obj", "$obj", -3, -4, 100, 200), ("$obj", "$obj", 7, 7, 7, 7),
                                        ("$obj", "$obj", 50, 60, 70, 80), ("$obj", "$obj", 1, 1, 2, 2)]),
    # Surface_blit3: same guards; otherwise the blit virtual (this->m_4 vtable+0x40) runs with src->m_4 and the two
    # rects {x,y,x+w,y+h} and {x2,y2,x2+w2,y2+h2}, records into $rec and returns 0x2a. 10 vectors (leaf gate).
    "Surface_blit3": dict(module="golf_clean.exe", addr=0x00475D00, abi="thiscall", ret="int",
                          args=["pointer", "pointer", "int", "int", "int", "int", "int", "int", "int", "int"],
                          fixture="c3i_surface", state=[("$rec", 0, 0x40)],
                          vectors=[("$obj", 0, 1, 2, 3, 4, 5, 6, 7, 8), ("$obj", 0, 9, 8, 7, 6, 5, 4, 3, 2),
                                   ("$znull", "$obj", 1, 2, 3, 4, 5, 6, 7, 8), ("$obj", "$znull", 1, 2, 3, 4, 5, 6, 7, 8),
                                   ("$obj", "$obj", 10, 20, 30, 40, 11, 22, 33, 44), ("$obj", "$obj", 0, 0, 5, 5, 1, 1, 2, 2),
                                   ("$obj", "$obj", -3, -4, 100, 200, -1, -2, 50, 60), ("$obj", "$obj", 7, 7, 7, 7, 8, 8, 8, 8),
                                   ("$obj", "$obj", 50, 60, 70, 80, 90, 100, 110, 120), ("$obj", "$obj", 2, 3, 4, 5, 6, 7, 8, 9)]),
})

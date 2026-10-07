# C3 batch c3n (2026-10-07): util constructors, virtual-dispatch helpers and rebuildHeightfield, reimplemented in
# shim/src/re/c3n.cpp. Format documented at the top of re/frida/hooks_registry.py. Keys are the hooks.csv names
# (NodeListB::ctor carries `::`; diff_hook writes its CSV as NodeListB_ctor). Writers declare their `state` regions.

_BUFS = [(f"$b{i}",) for i in range(12)]
_BUF_STATE = [("$obj", 0, 12 * 0x24)]

# resourceQuery: t0..t7 carry an inner object (values 100, 200, -7, -21, twice), t8..t11 a NULL inner (returns 0).
_QUERY = [(f"$t{i}",) for i in range(12)]
# resourceLoad / resourceLoadB (this, a, b) on t0..t7 only (non-NULL inner):
#   odd b            -> slot +4 returns non-zero, returned at once;
#   even b, a == 0   -> inner slot +0x10 returns 0 -> 0;
#   t3 (inner -21), a = 7, even b -> inner slot returns 0 -> 0;
#   even b, a != 0   -> inner slot returns a*3 + value, slot +8 runs, that value is returned (B clears this+0x14).
_LOAD = [("$t0", 5, 3), ("$t1", 9, 7), ("$t2", 0, 1), ("$t5", -3, -1),
         ("$t0", 0, 2), ("$t1", 0, 4), ("$t3", 7, 2),
         ("$t0", 5, 2), ("$t1", 7, 0), ("$t2", -4, 6), ("$t3", 100, 8), ("$t4", 1, -2), ("$t6", 0x7FFFFFFF, 10),
         ("$t7", 3, 12)]
_RES_STATE = [("$rec", 0, 0x40), ("$obj", 0, 12 * 0x20)]

# rebuildHeightfield writes the corner cache 0x0051b770 (20000), the level grid 0x00543018 (2500), the mask grid
# 0x005619a0 (2500, relaxEdges and the final byte-0 update), 0x004c2e04 and the flags dword 0x0059e7b8.
_HF_STATE = [(0x0051B770, 0, 20000), (0x00543018, 0, 2500), (0x005619A0, 0, 2500), (0x004C2E04, 0, 4),
             (0x0059E7B8, 0, 4)]


# scanTileLine (x0, y0, tx, ty, range, curve, strict): world start points, target tiles in the 8 directions at 1..14
# tiles, range 0/60/130/250 (clamped run of 1, 3, 6, 11 steps: lead 1, 1, 2, 3), curve 0/1/-1, strict 0/1, plus
# lines that leave the grid on the negative x and y sides (negative sub-tile offsets, tileBlocked's range test).
_DIRS = [(1, 0), (1, 1), (0, 1), (-1, 1), (-1, 0), (-1, -1), (0, -1), (1, -1)]
_STARTS = [(25 * 1024 + 300, 25 * 1024 + 700), (18 * 1024 + 900, 31 * 1024 + 50), (30 * 1024 + 512, 20 * 1024 + 1000)]
_SCAN = []
for _si, (_x0, _y0) in enumerate(_STARTS):
    _tx0, _ty0 = _x0 >> 10, _y0 >> 10
    for _di, (_ux, _uy) in enumerate(_DIRS):
        for _k, _d in enumerate((1, 3, 6, 9, 14)):
            _j = _si + _di + _k
            _SCAN.append((_x0, _y0, _tx0 + _ux * _d, _ty0 + _uy * _d, (0, 60, 130, 250)[_j % 4], (0, 1, -1)[_j % 3],
                          _j % 2))
# every direction x curve x strict at 9 tiles with the longest run
_SCAN += [(25 * 1024 + 300, 25 * 1024 + 700, 25 + ux * 9, 25 + uy * 9, 250, c, s)
          for ux, uy in _DIRS for c in (0, 1, -1, 2) for s in (0, 1)]
# leaving the grid: negative x, negative y, both
_SCAN += [(200, 20 * 1024 + 500, -4, 20, r, c, 0) for r in (60, 250) for c in (0, 1)]
_SCAN += [(20 * 1024 + 500, 200, 20, -4, r, c, 0) for r in (60, 250) for c in (0, -1)]
_SCAN += [(300, 300, -3, -3, 250, 0, 0), (300, 300, -3, -3, 130, 1, 1)]


def _scan(fixture):
    return dict(module="golf_clean.exe", addr=0x00421FA0, abi="default", ret="int",
                args=["int", "int", "int", "int", "int", "int", "int"], fixture=fixture,
                state=[(0x005783A2, 0, 1)], vectors=_SCAN)


def _hf(fixture):
    return dict(module="golf_clean.exe", addr=0x0042F7A0, abi="default", ret="void", args=[], fixture=fixture,
                state=_HF_STATE, vectors=[()])


HOOKS = {
    # --- constructors / init over 12 pre-patterned objects (leaf rule: >= 10 vectors) ---
    "Buf_init": dict(module="golf_clean.exe", addr=0x00474780, abi="thiscall", ret="void", args=["pointer"],
                     fixture="c3n_bufs", state=_BUF_STATE, vectors=_BUFS),
    "Buf_ctor": dict(module="golf_clean.exe", addr=0x004747A0, abi="thiscall", ret="pointer", args=["pointer"],
                     fixture="c3n_bufs", state=_BUF_STATE, vectors=_BUFS),
    "NodeListB::ctor": dict(module="golf_clean.exe", addr=0x00487A20, abi="thiscall", ret="pointer",
                            args=["pointer"], fixture="c3n_bufs", state=_BUF_STATE, vectors=_BUFS),
    "NodeListC::ctor": dict(module="golf_clean.exe", addr=0x00487B40, abi="thiscall", ret="pointer",
                            args=["pointer"], fixture="c3n_bufs", state=_BUF_STATE, vectors=_BUFS),

    # --- virtual dispatch through two fake vtables (c3n_res) ---
    "resourceQuery": dict(module="golf_clean.exe", addr=0x00487630, abi="thiscall", ret="int", args=["pointer"],
                          fixture="c3n_res", state=_RES_STATE, vectors=_QUERY),
    "resourceLoad": dict(module="golf_clean.exe", addr=0x00487C00, abi="thiscall", ret="int",
                         args=["pointer", "int", "int"], fixture="c3n_res", state=_RES_STATE, vectors=_LOAD),
    "resourceLoadB": dict(module="golf_clean.exe", addr=0x00487A70, abi="thiscall", ret="int",
                          args=["pointer", "int", "int"], fixture="c3n_res", state=_RES_STATE, vectors=_LOAD),

    # --- argument-free rebuild: one call per fixture variant ---
    "rebuildHeightfield": _hf("c3n_hf_c2"),
    "rebuildHeightfield_c0": _hf("c3n_hf_c0"),
    "rebuildHeightfield_tick": _hf("c3n_hf_tick"),
    "rebuildHeightfield_flat": _hf("c3n_hf_flat"),

    # --- line scan over the tile grid, two type-record settings ---
    "scanTileLine": _scan("c3n_scan_mixed"),
    "scanTileLine_neg": _scan("c3n_scan_neg"),
}

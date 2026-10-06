# C3 batch c3j of golf_clean.exe: a golfer stat-trend average, two tile/object scans that write scratch globals,
# two Snd field writers and a Snd base constructor. Reimplementations in shim/src/re/c3j.cpp, fixtures in
# re/frida/js/fixtures.d/c3j.js. Keys are the hooks.csv names (they may contain `::`; diff_hook maps the address).
# Format: top of re/frida/hooks_registry.py. thiscall functions take `this` as the first (pointer) arg.

# computeRatingTrend selects a numerator/divisor by the second argument (1/2/4); other values return 0. Four seeded
# records (idx 0..3) give four stat patterns, so the chosen branch and the result vary across the grid.
_TREND = [(idx, sel) for idx in range(4) for sel in (1, 2, 4, 0, 3)]

# setVolume / setField34 store an argument-derived value; v is masked to 7 bits by setVolume. Ten garbage-filled
# slots, one per vector, so both the stored value and the untouched slot tail vary.
_VOLS = [0, 1, 5, 0x40, 0x7f, 0x33, 0x80, 0x81, 0xAA, 0x1FF]

HOOKS = {
    # 0x004060a0 computeRatingTrend(idx, sel): pure leaf over the per-index stat tables (>= 10 vectors).
    "computeRatingTrend": dict(module="golf_clean.exe", addr=0x004060A0, abi="default", ret="int",
                               args=["int", "int"], fixture="c3j_rating", vectors=_TREND),

    # 0x00407000 holeQuadrants(a, b, mask): scans the placed-object table, stores the direction mask at 0x00541318
    # and returns coverage. State region: the mask global. Vectors place (a, b) near different objects and vary mask.
    "holeQuadrants": dict(module="golf_clean.exe", addr=0x00407000, abi="default", ret="int",
                          args=["int", "int", "int"], fixture="c3j_holes", state=[(0x00541318, 0, 4)],
                          vectors=[(10 << 10, 10 << 10, 1), (10 << 10, 20 << 10, 2), (20 << 10, 10 << 10, 4),
                                   (20 << 10, 20 << 10, 8), (30 << 10, 30 << 10, 1), (10 << 10, 10 << 10, 3),
                                   (15 << 10, 15 << 10, 0xF), (10 << 10, 15 << 10, 1), (45 << 10, 45 << 10, 0xF),
                                   (10 << 10, 10 << 10, 0), (20 << 10, 20 << 10, 0x10), (20 << 10, 20 << 10, 0xF)]),

    # 0x0040de70 findNearestTargetTile(px, py, maxDist): scans a 9x9 tile block for the nearest flagged, unblocked
    # tile and writes the min distance / x / y to three scratch globals. State region: those three globals.
    "findNearestTargetTile": dict(module="golf_clean.exe", addr=0x0040DE70, abi="default", ret="void",
                                  args=["int", "int", "int"], fixture="c3j_nearest",
                                  state=[(0x00568D0C, 0, 4), (0x0056A91C, 0, 4), (0x0056A920, 0, 4)],
                                  vectors=[(10 << 10, 10 << 10, 1000), (14 << 10, 14 << 10, 1000),
                                           (18 << 10, 18 << 10, 1000), (10 << 10, 18 << 10, 1000),
                                           (18 << 10, 10 << 10, 1000), (30 << 10, 30 << 10, 1000),
                                           (12 << 10, 12 << 10, 1000), (16 << 10, 16 << 10, 1000),
                                           (20 << 10, 20 << 10, 1000), (10 << 10, 10 << 10, 2)]),

    # 0x00485140 Snd::setVolume(v): stores v & 0x7f at this+4; the fixture holds the device (this+0x40) null and
    # this+0x3c >= 0x10, so the float scale and device forward do not run. Arena is the state region.
    "Snd::setVolume": dict(module="golf_clean.exe", addr=0x00485140, abi="thiscall", ret="void",
                           args=["pointer", "int"], fixture="c3j_snd_vol", state=[("$obj", 0, 0x80 * 10)],
                           vectors=[(f"$o{i}", v) for i, v in enumerate(_VOLS)]),

    # 0x004846b0 Snd::setField34(v): stores v at this+0x34; the fixture holds the device (this+0x40) null so the
    # forward does not run. Arena is the state region. No callees -> 10 vectors.
    "Snd::setField34": dict(module="golf_clean.exe", addr=0x004846B0, abi="thiscall", ret="void",
                            args=["pointer", "int"], fixture="c3j_snd_f34", state=[("$obj", 0, 0x80 * 10)],
                            vectors=[(f"$o{i}", v) for i, v in enumerate(_VOLS)]),

    # 0x00485260 Snd::ctor485260(): installs a vtable and fixed fields in this+0..this+0x30, returns this. Ten
    # garbage-filled slots; the arena tail past 0x34 keeps its garbage so the final state varies. No callees -> 10.
    "Snd::ctor485260": dict(module="golf_clean.exe", addr=0x00485260, abi="thiscall", ret="pointer",
                            args=["pointer"], fixture="c3j_snd_ctor", state=[("$obj", 0, 0x80 * 10)],
                            vectors=[(f"$o{i}",) for i in range(10)]),
}

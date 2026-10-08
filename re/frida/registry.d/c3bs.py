# C3 batch c3bs (round 17, 2026-10-08) of golf_clean.exe: the two IJG libjpeg 6a master methods with no call
# xref, the shading spreader pair, the RLE surface blitter, the object spawner and the roster name builder.
# Reimplementations in shim/src/re/c3bs.cpp, fixtures in re/frida/js/fixtures.d/c3bs.js, Purposes and per-jcc
# branch coverage in log/c3/c3bs_purpose.md (model: log/c3/c3bs_branches.py).
#
# Hooked callees (re-run these keys with SIMGOLF_HOOKS_OFF so the original arm uses original callees):
#   spreadShade    -> 0040bf60 tileBlocked and 00467170 approxDistance (C3, core golf_math.cpp) and
#                     00467130 clamp (C3, c3f.cpp)
#   spreadAllShade -> 004615f0 spreadShade (IN THIS BATCH) plus the three above
#   spawnObject    -> 0045c1e0 Random::range (C3, c3f.cpp); both arms advance the same restored seed 0x00822d9c
# pass_startup, finish_pass and Surface::blitRLE dispatch only through fixture callbacks, not through hooks.

# --- the two 50x50 shading grids plus the tile types tileBlocked reads -------------------------------------
_SHADE_OUT = [(0x0082376C, 0, 2500), (0x00822DA0, 0, 2500), (0x005722E8, 0, 2500)]
# spreadAllShade also reads the weight grid and the two source grids.
_SHADE_ALL = _SHADE_OUT + [(0x0053EA24, 0, 2500), (0x005A6378, 0, 2500), (0x0056C7E4, 0, 2500)]

# spreadShade stamp centres: four wholly inside the 50x50 grid, four on a corner or an edge so tileBlocked
# 0x0040bf60 rejects part of the 15x15 block by its bounds check instead of by a tile type.
_CENTRES = [(10, 10), (20, 30), (42, 42), (7, 7), (0, 0), (49, 49), (3, 46), (46, 3)]
# 1 -> amount 0 (the `jle` at 0x0046166f takes the negative grid with a zero delta); 1500 / -1500 -> ordinary
# add and subtract; 90000 / -90000 -> amount 60000 / -60000, which clamps at 0xff and at 0 near the centre.
_VALUES = [1500, -1500, 1, 90000, -90000]


def _spread_all(fixture):
    return dict(module="golf_clean.exe", addr=0x004616F0, abi="default", ret="void", args=[],
                fixture=fixture, state=_SHADE_ALL, vectors=[()])


HOOKS = {
    # pass_startup(cinfo): the clear of master->call_pass_startup and the two marker methods, in order.
    # 12 vectors; vectors 8..11 use a frame-header callback that replaces cinfo->marker, so the re-read at
    # 0x004b5cbc is evidence. Leaf by hooks.csv (both calls are indirect).
    "pass_startup": dict(
        module="golf_clean.exe", addr=0x004B5CA0, abi="default", ret="void", args=["pointer"],
        fixture="c3bs_pass",
        state=[(f"$c{i}", 0, 0x180) for i in range(12)]
              + [(f"$m{i}", 0, 0x24) for i in range(12)]
              + [(f"$r{i}", 0, 0x100) for i in range(12)],
        vectors=[(f"$c{i}",) for i in range(12)]),

    # finish_pass(cinfo): the entropy end-of-pass call and the pass_type switch. 14 vectors covering all three
    # cases and the default arm, each crossed with optimize_coding, plus the 0xffffffff increments.
    # NOTE: 0x004b5cd0 is not a function in the Ghidra master and has no hooks.csv row (see the notes file):
    # the verifier must either track it first or pass --skip 004b5cd0.
    "finish_pass": dict(
        module="golf_clean.exe", addr=0x004B5CD0, abi="default", ret="void", args=["pointer"],
        fixture="c3bs_finish",
        state=[(f"$c{i}", 0, 0x180) for i in range(14)]
              + [(f"$m{i}", 0, 0x24) for i in range(14)]
              + [(f"$r{i}", 0, 0x100) for i in range(14)],
        vectors=[(f"$c{i}",) for i in range(14)]),

    # spreadShade(row, col, value): 40 vectors, eight stamp centres by five values.
    "spreadShade": dict(
        module="golf_clean.exe", addr=0x004615F0, abi="default", ret="void", args=["int", "int", "int"],
        fixture="c3bs_shade", state=_SHADE_OUT,
        vectors=[(r, c, v) for r, c in _CENTRES for v in _VALUES]),

    # spreadAllShade(): no arguments, so one call per key and one fixture per case of the weight total.
    "spreadAllShade_empty": _spread_all("c3bs_all_empty"),      # total 0: the early return
    "spreadAllShade_below": _spread_all("c3bs_all_below"),      # total 9: the last early return
    "spreadAllShade_at10": _spread_all("c3bs_all_at10"),        # total 10: the first spread
    "spreadAllShade_pos": _spread_all("c3bs_all_pos"),          # total 1000, positive sources only
    "spreadAllShade_neg": _spread_all("c3bs_all_neg"),          # total 1000, negative sources only
    "spreadAllShade_at1750": _spread_all("c3bs_all_at1750"),    # total 0x6d6: the last uncapped total
    "spreadAllShade_clamped": _spread_all("c3bs_all_clamped"),  # total 5000: the cap at 0x6d6, both sources

    # Surface::blitRLE(chunk, unused, display): 14 vectors, each with its own header, payload, destination
    # buffer and lock/unlock recorder. Leaf (both calls are through the surface vtable).
    "Surface::blitRLE": dict(
        module="golf_clean.exe", addr=0x00482990, abi="thiscall", ret="int",
        args=["pointer", "pointer", "int", "pointer"], fixture="c3bs_rle",
        state=[(f"$b{i}", 0, 0xC0) for i in range(14)] + [(f"$r{i}", 0, 0x40) for i in range(14)],
        vectors=[(f"$t{i}", f"$k{i}", (i * 7) - 3, f"$d{i}") for i in range(14)]),

    # spawnObject(lot, kind, a, b): the object table, the RNG seed, and the two read-only tables the fixture
    # seeds (the kind records and the lot cell maps, neither of which the function writes).
    "spawnObject": dict(
        module="golf_clean.exe", addr=0x004011E0, abi="default", ret="void",
        args=["int", "int", "int", "int"], fixture="c3bs_spawn0",
        state=[(0x0056D1B8, 0, 0x1770), (0x00822D9C, 0, 4), (0x004E6D70, 0, 8 * 0x74),
               (0x0055D758, 0, 16 * 0x25)],
        vectors=[(0, 0, 1, 2), (1, 1, -1, -2), (2, 2, 300, 400), (3, 3, 0x7FFF, -0x8000), (4, 4, 10, 20),
                 (5, 5, 0x10000, 0x10001), (6, 6, -5, -6), (7, 7, 100, 200), (1, 8, 0, 0), (2, 9, 7, 7),
                 (3, 10, 0x1234, 0x5678), (0, 11, -1, 1)]),
    # records 0..4 taken, record 5 free: five turns of the search loop at 0x004011ed.
    "spawnObject_occ": dict(
        module="golf_clean.exe", addr=0x004011E0, abi="default", ret="void",
        args=["int", "int", "int", "int"], fixture="c3bs_spawn5",
        state=[(0x0056D1B8, 0, 0x1770), (0x00822D9C, 0, 4), (0x004E6D70, 0, 8 * 0x74),
               (0x0055D758, 0, 16 * 0x25)],
        vectors=[(0, 0, 1, 2), (1, 1, -1, -2), (2, 2, 300, 400), (3, 3, 0x7FFF, -0x8000), (4, 4, 10, 20),
                 (5, 5, 0x10000, 0x10001), (6, 6, -5, -6), (7, 7, 100, 200), (1, 8, 0, 0), (2, 9, 7, 7),
                 (3, 10, 0x1234, 0x5678), (0, 11, -1, 1)]),
    # every record taken: the loop runs out at 0x004011f1 and the function returns without writing anything.
    "spawnObject_full": dict(
        module="golf_clean.exe", addr=0x004011E0, abi="default", ret="void",
        args=["int", "int", "int", "int"], fixture="c3bs_spawnfull",
        state=[(0x0056D1B8, 0, 0x1770), (0x00822D9C, 0, 4), (0x004E6D70, 0, 8 * 0x74),
               (0x0055D758, 0, 16 * 0x25)],
        vectors=[(0, 0, 1, 2), (3, 3, 0x7FFF, -0x8000), (6, 6, -5, -6), (0, 11, -1, 1)]),

    # pickGolferName(index): the shared text buffer and the 16 seeded roster records. Leaf, 16 vectors.
    # The running menu also writes 0x0051a068, so this key wants a boot of its own.
    "pickGolferName": dict(
        module="golf_clean.exe", addr=0x00467600, abi="default", ret="void", args=["int"],
        fixture="c3bs_names",
        state=[(0x0051A068, 0, 768), (0x00585850, 0, 16 * 0x4C)],
        vectors=[(i,) for i in range(16)]),
}

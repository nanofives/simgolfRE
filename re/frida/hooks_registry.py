"""Single source of truth for A/B test vectors. Add an entry here; never write a one-off harness.

Each entry drives re/frida/diff_hook.py:
  addr      VA (golf_clean.exe) the SG_HOOK is registered at
  abi       Frida ABI name: "thiscall" | "stdcall" | "default" (cdecl) | "fastcall"
  ret/args  Frida NativeFunction types
  fixture   name of a JS fixture in js/diff_fixtures.js; it returns {obj, ...} pointers that vectors
            reference as "$obj" (and pointer returns are reported relative to $obj, so arms compare)
  vectors   list of argument tuples
"""

_GRID = [(x, y) for x in (-1, 0, 1, 5, 9, 10, 11, 0x7FFFFFFF, -0x80000000) for y in (-1, 0, 3, 7, 8, 9)]

HOOKS = {
    "Terrain_tileAt": dict(
        module="golf_clean.exe",
        addr=0x004490D0,
        abi="thiscall",
        ret="pointer",
        args=["pointer", "int", "int"],
        fixture="terrain_10x8",
        vectors=[("$obj", x, y) for x, y in _GRID],
        note="re/analysis/terrain/004490d0_tileAt.md",
    ),
    # Live copy in Terrain.dll (RVA of the body; module-relative). Recorded/replayed by record.py/replay.py.
    "TerrainDll_tileAt": dict(
        module="Terrain.dll",
        addr=0x00001D50,
        abi="thiscall",
        ret="pointer",
        args=["pointer", "int", "int"],
        nargs=2,  # stack args (this is in ecx)
        fixture="terrain_10x8",
        vectors=[("$obj", x, y) for x, y in _GRID],
    ),
}

_EDGE = [-0x80000000, -0x7FFFFFFF, -100000, -129, -128, -65, -64, -63, -17, -16, -9, -8, -7, -2, -1, 0, 1, 2, 7, 8, 9,
         15, 16, 17, 63, 64, 65, 127, 128, 129, 1000, 100000, 0x7FFFFFFE, 0x7FFFFFFF]
_SMALL = [-20, -11, -10, -6, -5, -1, 0, 1, 2, 4, 5, 6, 8, 9, 10, 15, 16, 17, 18, 19, 20, 0x7FFFFFFF, -0x80000000]
_PAIRS = [(a, b) for a in (-0x80000000, -1000, -3, -1, 0, 1, 2, 3, 7, 1000, 0x3FFFFFFF, 0x7FFFFFFF)
          for b in (-0x80000000, -1000, -4, -1, 0, 1, 4, 8, 999, 1001, 0x3FFFFFFF, 0x7FFFFFFF)]
_DECAY2 = [(x, y) for x in (-100000, -64, -1, 0, 1, 15, 16, 64, 1000, 100000, 0x7FFFFFFF)
           for y in (-0x80000000, -1000, -16, -1, 0, 1, 15, 16, 127, 128, 129, 1000, 100000)]
_TILES = [(x, y) for x in (-0x80000000, -1, 0, 1, 2, 9, 24, 25, 48, 49, 50, 51, 0x7FFFFFFF)
          for y in (-0x80000000, -1, 0, 1, 2, 3, 13, 26, 48, 49, 50, 0x7FFFFFFF)]
_TILES += [(x, y) for x in range(50) for y in range(50) if (x * 7 + y * 3) & 0x1F in (0x13, 0x14, 0x15)]

HOOKS.update({
    # Batch 1 of C3 (2026-10-06): __cdecl integer leaves, hand-written in shim/src/re/golf_math.cpp.
    "approxDistance": dict(module="golf_clean.exe", addr=0x00467170, abi="default", ret="int", args=["int", "int"],
                           vectors=_PAIRS),
    "bucketValue": dict(module="golf_clean.exe", addr=0x0044FAF0, abi="default", ret="int", args=["int"],
                        vectors=[(v,) for v in sorted(set(_SMALL + _EDGE))]),
    "decaySum": dict(module="golf_clean.exe", addr=0x004223C0, abi="default", ret="int", args=["int"],
                     vectors=[(v,) for v in _EDGE]),
    "decaySum2": dict(module="golf_clean.exe", addr=0x004223F0, abi="default", ret="int", args=["int", "int"],
                      vectors=_DECAY2),
    "scaleX": dict(module="golf_clean.exe", addr=0x00404970, abi="default", ret="int", args=["int"],
                   vectors=[(v,) for v in _EDGE + [160, 319, 320, 321, 640, 799, 800]]),
    "tierPrice": dict(module="golf_clean.exe", addr=0x0046F1D0, abi="default", ret="int", args=["int"],
                      vectors=[(v,) for v in range(-3, 20)] + [(-0x80000000,), (0x7FFFFFFF,), (0x10,), (0xFFFF,)]),
    "tileBlocked": dict(module="golf_clean.exe", addr=0x0040BF60, abi="default", ret="int", args=["int", "int"],
                        fixture="tile_types_pattern", vectors=_TILES),
})

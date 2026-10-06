# C3 batch c3b: util helpers of golf_clean.exe, reimplemented in shim/src/re/c3b.cpp. Format documented at the
# top of re/frida/hooks_registry.py. Keys match the hooks.csv names.

# distance 0x0040acd0: dx/dy around the 0x4000 scale threshold (|v| > 0x4000 divides by 8) and the signs.
_D = [-0x7FFFFFFF, -0x40000000, -0x10001, -0x10000, -0x4001, -0x4000, -0x3FFF, -1, 0, 1, 0x3FFF, 0x4000,
      0x4001, 0x10000, 0x10001, 0x40000000, 0x7FFFFFFF]
_DIST = [(dx, dy) for dx in _D[::2] for dy in _D[1::2]]

# tileDistance 0x0040c4b0: world points near and far from tile centres (0x400 per tile, centre + 0x200).
_TD = [(x, y, tx, ty) for x, y in ((0, 0), (0x200, 0x200), (0x800, 0x400), (0x4000, 0x4000), (-0x200, 0x600))
       for tx, ty in ((0, 0), (1, 1), (2, 0), (10, 10), (49, 49))]

# foldRange 0x00467270: a across the sign bit and bit 0x40000000 boundaries; b both signs.
_FA = [-0x80000000, -0x7FFFFFFF, -0x40000001, -0x40000000, -0x3FFFFFFF, -1, 0, 1,
       0x3FFFFFFF, 0x40000000, 0x40000001, 0x7FFFFFFF]
_FOLD = [(a, b) for a in _FA for b in (-1000, -1, 0, 1, 100, 0x4000)]

_STR8 = [(f"$s{i}",) for i in range(8)]
_GTEXT = [(0x0051A068, 0, 1024)]
_POOL = [(0x00820B70, 0, 400), (0x00820D00, 0, 400)]

HOOKS = {
    "distance": dict(module="golf_clean.exe", addr=0x0040ACD0, abi="default", ret="int", args=["int", "int"],
                     vectors=_DIST),
    "tileDistance": dict(module="golf_clean.exe", addr=0x0040C4B0, abi="default", ret="int",
                         args=["int", "int", "int", "int"], vectors=_TD),
    "foldRange": dict(module="golf_clean.exe", addr=0x00467270, abi="default", ret="int", args=["int", "int"],
                      vectors=_FOLD),
    "trimLeadingSpace": dict(module="golf_clean.exe", addr=0x004924E0, abi="default", ret="void", args=["pointer"],
                             fixture="c3b_strings", state=[("$obj", 0, 0x100)], vectors=_STR8),
    "trimTrailingSpace": dict(module="golf_clean.exe", addr=0x00492570, abi="default", ret="void", args=["pointer"],
                              fixture="c3b_strings", state=[("$obj", 0, 0x100)], vectors=_STR8),
    "appendString": dict(module="golf_clean.exe", addr=0x0045B9F0, abi="default", ret="int", args=["int"],
                         fixture="c3b_strtable", state=_GTEXT,
                         vectors=[(i,) for i in (-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 20)]),
    "replaceInText": dict(module="golf_clean.exe", addr=0x0045B7C0, abi="default", ret="void",
                          args=["pointer", "pointer"], fixture="c3b_replace", state=_GTEXT,
                          vectors=[("$k0", "$r0"), ("$k1", "$r1"), ("$k2", "$r2"), ("$k3", "$r3"),
                                   ("$k0", "$r2"), ("$k1", "$r3")]),
    "allocBlock": dict(module="golf_clean.exe", addr=0x0043D5D0, abi="default", ret="int", args=["int"],
                       fixture="c3b_blocks", state=_POOL,
                       vectors=[(n,) for n in (0, 1, 3, 5, 10, 18, 50, 51, 100, 1000)]),
    "freeBlock": dict(module="golf_clean.exe", addr=0x0043D520, abi="default", ret="void", args=["int", "int"],
                      fixture="c3b_blocks", state=_POOL,
                      vectors=[(10, 5), (25, 5), (150, 10), (205, 3), (500, 7), (0, 4),
                               (20, 80), (320, 4), (5, 2), (250, 1), (155, 0), (-5, 3)]),
}

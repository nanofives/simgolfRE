# Batch c3as: Terrain.dll. addr is the RVA (VA = 0x10000000 + RVA).
#
# Three groups: the four integer classifiers of Tile::blendFaceTextures 0x10013670, Terrain's x87 math, and five
# allocation-free std::list<Tile*> helpers. Fixtures: re/frida/js/fixtures.d/c3as.js (c3as_tiles, c3as_math,
# c3as_nodes). Nothing of the game's is handed to an arm: the tiles, the collar table, the vertex array the face
# normals are built from, the faces, the vectors and the list nodes are all our own allocations.
#
# Returns declared "double" (degToRad, powf2, Terrain::bernstein): these three leave an UNROUNDED 53-bit value in
# st(0) - degToRad has no store at all, powf2's `fst dword [ebp-4]` keeps st(0), and bernstein's last `fmul` is
# its result - and their callers consume exactly that, so the A/B compares the whole of st(0).
#
# Hooked callees (re-run with SIMGOLF_HOOKS_OFF so the original arm uses original callees):
#   Tile::edgeKind       -> Tile::getType Terrain.dll:1f60 (batch c3am), Tile::getVariation Terrain.dll:15340 (c3aq)
#   rotateAxis           -> degToRad Terrain.dll:1880 (this batch)
#   Terrain::bernstein   -> factorial Terrain.dll:58a0 (c3aq), powf2 Terrain.dll:5840 (this batch)
#   Terrain::faceNormal  -> normalize Terrain.dll:37c80 (this batch)

# the 16 mutable vectors / the whole math block, plus the separate face records
_MATH = [("$blk", 0, 0x900), ("$faces", 0, 16 * 0x40)]

_IDS = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 16, 20, 21, 24, 25, 30, 34, 40, 41, 42, 43,
        50, 52, 60, 61, 70, 71, 0x46, 0x47, 100, 0x80, -1, 0x7FFFFFFF, -0x80000000]
# every combination of the two constants each argument can add, plus values that add nothing
_FLAGS = [(a, b, c) for a in (0, 1, 2, 3) for b in (0, 1, 2, -1) for c in (0, 1, 2, 5)]

_ANGLES = [0.0, 1.0, -1.0, 90.0, 180.0, 0.1, 33.333333, -45.0, 1e-7, 3e5, 359.9999, 1e7,
           1.0000001, 16777215.0, -1e-7, 0.30000001192092896, 123.456, -0.0077]
# (base, exponent) for powf2: no NaN and no infinity (a negative base is only raised to an integral exponent)
_POWS = [(2.0, 3.0), (2.0, -3.0), (0.5, 2.0), (10.0, 0.5), (0.1, 3.0), (3e5, 2.0), (1.0000001, 100.0),
         (16777215.0, 0.5), (7.0, 0.0), (0.0, 0.0), (0.0, 2.0), (1.0, 1e7), (-2.0, 3.0), (-2.0, 2.0),
         (0.3333333333, 4.0), (123.456, -1.5), (1e-7, 2.0), (2.0, 0.1)]
# (t, i) for Terrain::bernstein: i = 0, 1, 2 are the degree-2 terms; 3, -1 and 4 leave the range
_BERN = [(t, i) for i in (0, 1, 2) for t in (0.0, 0.25, 0.5, 0.75, 1.0, 0.1, 0.3333333333, 1.0000001)]
_BERN += [(0.5, 3), (0.5, -1), (0.3, 4)]

HOOKS = {
    # ---------------------------------------------------------------- integer classifiers
    # `this` is stored and never read (0x10013dea), so 0 is passed for it.
    # ja 0x10013df7 (unsigned > 0x46) takes 0x47, 0x80, 100, -1, 0x7fffffff and -0x80000000 to the default;
    # the other ids hit each of the nine case blocks, and 9, 11, 13 are in-range ids whose table byte is the
    # default entry, so both `xor eax, eax` blocks are reached.
    "Terrain::idClass": dict(module="Terrain.dll", addr=0x00013DD0, abi="thiscall", ret="int",
                             args=["pointer", "int"], vectors=[(0, v) for v in _IDS]),
    # six jcc (0x10013f28, 0x10013f39, 0x10013f48, 0x10013f59, 0x10013f68, 0x10013f79): each argument takes the
    # == 1 arm, the == 2 arm and the arm that adds nothing (0, 3, -1, 5)
    "Terrain::flagCode": dict(module="Terrain.dll", addr=0x00013F00, abi="thiscall", ret="int",
                              args=["pointer", "int", "int", "int"],
                              vectors=[(0, a, b, c) for a, b, c in _FLAGS]),
    # tiles 0..6 carry the seven types that take a je (2, 7, 1, 0, 9, 8, 3, in the original's order), tiles
    # 7..15 types that fall through to the 0 store (including -1, 0x7fffffff and -0x80000000)
    "Tile::isOpenType": dict(module="Terrain.dll", addr=0x000155B0, abi="thiscall", ret="uint8",
                             args=["pointer"], fixture="c3as_tiles",
                             vectors=[("$t%d" % i,) for i in range(16)]),
    # edgeKind: $collarSlot is the module pointer at Terrain.dll+0x106b48 the fixture installs our table in;
    # it is listed as state so diff_hook snapshots and restores it around every vector (the function never
    # writes it). Vectors, in order: NULL argument; the 0x11 path (type differs / variation equal / variation
    # differs with rotation 0, 1 and other; the sign-extended 0x80 byte); the collar path (equal entry with
    # equal and with different types, different entry).
    "Tile::edgeKind": dict(module="Terrain.dll", addr=0x00015650, abi="thiscall", ret="int",
                           args=["pointer", "pointer"], fixture="c3as_tiles",
                           state=[("$collarSlot", 0, 4)],
                           vectors=[("$t10", 0),
                                    ("$t16", "$t24"), ("$t16", "$t11"),
                                    ("$t16", "$t17"), ("$t16", "$t16"),
                                    ("$t18", "$t19"), ("$t18", "$t22"), ("$t18", "$t27"),
                                    ("$t17", "$t23"), ("$t19", "$t19"),
                                    ("$t20", "$t21"), ("$t20", "$t19"),
                                    ("$t22", "$t22"), ("$t23", "$t18"),
                                    ("$t24", "$t24"), ("$t24", "$t25"), ("$t24", "$t26"),
                                    ("$t0", "$t0"), ("$t1", "$t3"), ("$t26", "$t25")]),

    # ---------------------------------------------------------------- x87 math
    # leaf (no call of its own): 18 angles, including values whose product with pi/180 is not exact in float
    "degToRad": dict(module="Terrain.dll", addr=0x00001880, abi="default", ret="double", args=["float"],
                     vectors=[(a,) for a in _ANGLES]),
    "powf2": dict(module="Terrain.dll", addr=0x00005840, abi="default", ret="double",
                  args=["float", "float"], vectors=_POWS),
    # normalize writes the vector in place; the state region covers all 24, and diff_hook restores the
    # snapshot between the two arms, so every vector starts from the fixture's values. $v16..$v23 are the
    # negative control of the rounding model: computing the sum of squares and the three divisions in float
    # instead of at 53 bits returns a different float for them (9 of the 24 separate the two models).
    "normalize": dict(module="Terrain.dll", addr=0x00037C80, abi="default", ret="void", args=["pointer"],
                      fixture="c3as_math", state=_MATH,
                      vectors=[("$v%d" % i,) for i in range(24)]),
    # rotateAxis: nine x87 compares against 0.0f. Vectors 1-4 take the X branch, 5-8 and 19 the Y branch,
    # 9-12 the Z branch, and 13-18 the axes that write nothing (two non-zero components, all three zero,
    # all three non-zero).
    "rotateAxis": dict(module="Terrain.dll", addr=0x00003A50, abi="default", ret="void",
                       args=["float", "float", "float", "float", "pointer"],
                       fixture="c3as_math", state=_MATH,
                       vectors=[(90.0, 1.0, 0.0, 0.0, "$v1"), (-45.0, -1.0, 0.0, 0.0, "$v2"),
                                (0.1, 3e5, 0.0, 0.0, "$v3"), (33.333333, 1e-7, 0.0, 0.0, "$v4"),
                                (90.0, 0.0, 1.0, 0.0, "$v5"), (-45.0, 0.0, -2.5, 0.0, "$v6"),
                                (1e-7, 0.0, 0.1, 0.0, "$v7"), (180.0, 0.0, 1.0, 0.0, "$v8"),
                                (90.0, 0.0, 0.0, 1.0, "$v9"), (123.456, 0.0, 0.0, -1.0, "$v10"),
                                (0.1, 0.0, 0.0, 1e7, "$v11"), (359.9999, 0.0, 0.0, 0.5, "$v12"),
                                (90.0, 1.0, 1.0, 0.0, "$v13"), (45.0, 0.0, 0.0, 0.0, "$v14"),
                                (30.0, 1.0, 1.0, 1.0, "$v15"), (10.0, 1.0, 0.0, 1.0, "$v0"),
                                (10.0, 0.0, 1.0, 1.0, "$v1"), (20.0, 0.0, 1.0, 1.0, "$v2"),
                                (17.5, 1.0, 0.0, 0.0, "$v16"), (62.5, 0.0, 1.0, 0.0, "$v17"),
                                (-123.25, 0.0, 0.0, 1.0, "$v18"), (200.125, 1.0, 0.0, 0.0, "$v19")]),
    # bernstein: `this` is stored and never read (0x1000576a), so 0 is passed for it
    "Terrain::bernstein": dict(module="Terrain.dll", addr=0x00005750, abi="thiscall", ret="double",
                               args=["pointer", "float", "int"],
                               vectors=[(0, t, i) for t, i in _BERN]),
    # hermitePoint: `this` is stored and never read (0x100050aa). The output point is one of $o0..$o9 and the
    # four control points are our own; t covers 0, 1, inside, outside and inexact-in-float values.
    "Terrain::hermitePoint": dict(module="Terrain.dll", addr=0x00005090, abi="thiscall", ret="void",
                                  args=["pointer", "pointer", "float", "pointer", "pointer", "pointer",
                                        "pointer"],
                                  fixture="c3as_math", state=_MATH,
                                  vectors=[(0, "$o0", 0.0, "$c0", "$c1", "$c2", "$c3"),
                                           (0, "$o1", 1.0, "$c0", "$c1", "$c2", "$c3"),
                                           (0, "$o2", 0.5, "$c0", "$c1", "$c2", "$c3"),
                                           (0, "$o3", 0.25, "$c4", "$c5", "$c6", "$c7"),
                                           (0, "$o4", 0.75, "$c4", "$c5", "$c6", "$c7"),
                                           (0, "$o5", 0.1, "$c8", "$c9", "$c10", "$c11"),
                                           (0, "$o6", 0.3333333333, "$c8", "$c9", "$c10", "$c11"),
                                           (0, "$o7", 1.0000001, "$c0", "$c4", "$c8", "$c11"),
                                           (0, "$o8", -0.5, "$c1", "$c5", "$c9", "$c10"),
                                           (0, "$o9", 2.0, "$c2", "$c6", "$c7", "$c3"),
                                           (0, "$o0", 1e-7, "$c5", "$c0", "$c11", "$c4"),
                                           (0, "$o1", 0.9999999, "$c3", "$c7", "$c2", "$c6"),
                                           (0, "$o2", 123.456, "$c0", "$c1", "$c2", "$c3"),
                                           (0, "$o3", 0.30000001192092896, "$c9", "$c10", "$c11", "$c8")]),
    # faceNormal: `this` is stored and never read (0x10011d7a). Each face indexes our own vertex array
    # (the fixture places it at a multiple of 12 from Terrain.dll+0xb28c8 and stores the matching indices).
    "Terrain::faceNormal": dict(module="Terrain.dll", addr=0x00011D60, abi="thiscall", ret="void",
                                args=["pointer", "pointer"], fixture="c3as_math", state=_MATH,
                                vectors=[(0, "$f%d" % i) for i in range(16)]),

    # ---------------------------------------------------------------- std::list<Tile*> helpers
    # the three _Acc accessors never dereference the node; NULL is included because nothing stops it
    "std::list<Tile*>::_Acc::_Next": dict(module="Terrain.dll", addr=0x0000B600, abi="default", ret="pointer",
                                          args=["pointer"], fixture="c3as_nodes",
                                          vectors=[("$n%d" % i,) for i in range(16)] + [(0,)]),
    "std::list<Tile*>::_Acc::_Prev": dict(module="Terrain.dll", addr=0x0000B630, abi="default", ret="pointer",
                                          args=["pointer"], fixture="c3as_nodes",
                                          vectors=[("$n%d" % i,) for i in range(16)] + [(0,)]),
    "std::list<Tile*>::_Acc::_Value": dict(module="Terrain.dll", addr=0x0000B660, abi="default", ret="pointer",
                                           args=["pointer"], fixture="c3as_nodes",
                                           vectors=[("$n%d" % i,) for i in range(16)] + [(0,)]),
    # _Mynode dereferences the iterator: 16 distinct nodes, the four repeats and one iterator holding NULL
    "std::list<Tile*>::const_iterator::_Mynode": dict(module="Terrain.dll", addr=0x0000B710, abi="thiscall",
                                                      ret="pointer", args=["pointer"], fixture="c3as_nodes",
                                                      vectors=[("$i%d" % i,) for i in range(20)] + [("$z0",)]),
    # operator==: six equal pairs (including two NULL nodes) and twelve unequal ones, so `sete al` reads both
    "std::list<Tile*>::const_iterator::operator==": dict(
        module="Terrain.dll", addr=0x0000B380, abi="thiscall", ret="int", args=["pointer", "pointer"],
        fixture="c3as_nodes",
        vectors=[("$i0", "$i16"), ("$i1", "$i17"), ("$i2", "$i18"), ("$i3", "$i19"), ("$i5", "$i5"),
                 ("$z0", "$z1"), ("$i0", "$i1"), ("$i1", "$i2"), ("$i4", "$i9"), ("$i15", "$i0"),
                 ("$i7", "$i8"), ("$i10", "$i11"), ("$i12", "$i13"), ("$i6", "$i14"), ("$i0", "$z0"),
                 ("$z0", "$i3"), ("$i19", "$i5"), ("$i16", "$i2")]),
}

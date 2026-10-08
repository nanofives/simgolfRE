# Batch c3ba: Terrain.dll, round 14. addr is the RVA (VA = 0x10000000 + RVA).
#
# The terrain-geometry group around the two module arrays: Tile::elevateCorner, Tile::lowerCorner,
# Tile::reset, initGrid, setTextureId, Tile::setFaceTexture, Tile::calcNormals.
# Fixtures: re/frida/js/fixtures.d/c3ba.js. Every Tile and Face is our own allocation. The module memory the
# vectors touch - g_vertices 0x100b28c8, g_normals 0x10070a18, the type-0x23/0x24 rows of g_textures
# 0x100687f8, and g_tileW / g_tileH / g_splineHeight 0x10063e4c - is in the state regions below, so
# diff_hook snapshots it, compares it after each arm and writes it back. It is idle at the main menu where
# the A/B runs: log/c3/alt/c3ba_probe.py counted zero calls into thirteen Terrain.dll entry points over 8 s
# and saw no byte of the three arrays change in 1843 samples (g_vertices and g_normals read all zero).
#
# Hooked callees (the verifier re-runs these keys with SIMGOLF_HOOKS_OFF so the original arm uses original
# callees):
#   Tile::lowerCorner, Tile::lowerCorner_t7  -> Tile::getType       Terrain.dll:1f60  (c3am)
#   Tile::reset                              -> PathInfo::clear     Terrain.dll:f6e0  (c3aq)
#                                            -> WallInfo::clear     Terrain.dll:f7a0  (c3aq)
#   Tile::calcNormals                        -> Terrain::faceNormal Terrain.dll:11d60 (c3as)
#                                            -> normalize           Terrain.dll:37c80 (c3as)
#                                            -> Tile::getFaces      Terrain.dll:153c0 (c3as)
#     (faceNormal and getFaces are reached through Tile::smoothNormals 0x10011ef0, which is not hooked.)
#
# g_tileW / g_tileH / g_splineHeight are read by three functions in the whole module (initGrid 0x1000c560,
# Tile::elevateCorner 0x1000c966, Tile::lowerCorner 0x1000ce76) and written by one (Terrain::setSplineHeight
# 0x1000a840), found by scanning .text for the three addresses. Each fixture writes the values it needs, so
# no key depends on what an earlier one left behind; the `_odd` keys deliberately leave a value that is not
# the live one (0.1 instead of 15.0 / 100.0) in the module for the rest of that boot, which no other code
# reads.

_TSIZE = 0x250

_CORNER_STATE = [("$blk", 0, 16 * _TSIZE), ("$vtx", 0, 0x1200)]      # 16 tiles, vertex entries 0..383
_CORNER7_STATE = [("$blk", 0, 25 * _TSIZE), ("$vtx", 0, 0x1C20)]     # 25 tiles, vertex entries 0..599
_RESET_STATE = [("$blk", 0, 8 * _TSIZE)]
_FACE_STATE = [("$blk", 0, 24 * _TSIZE)]
_TEX_STATE = [("$tex", 0, 1800)]                                     # g_textures type rows 0x23 and 0x24
_GRID_STATE = [("$vtx", 0, 0x780), ("$nrm", 0, 0x780)]               # entries 0..159 of both arrays
_NORMALS_STATE = [("$blk", 0, 10 * _TSIZE), ("$nrm", 0, 0xB40)]      # 10 tiles, normal entries 0..239

# Only the odd corners reach a switch case (jump table 0x1000cb9b / 0x1000d2d3); 2, 4, 6 and everything out
# of 1..7 reach 0x1000c952 with the twelve frame slots still holding the /GZ fill and fault in both arms.
_CORNERS = (1, 3, 5, 7)
_CORNER_VEC = [("$t%d" % k, c) for k in range(8) for c in _CORNERS]
_CORNER7_VEC = [("$u%d" % k, c) for k in range(12) for c in _CORNERS]

# Tile::reset: eight tiles pre-filled with a non-zero pattern, sixteen (x, y) pairs including the sign
# extremes (both are stored verbatim at 0x1000c2e3 and 0x1000c2ec, nothing bounds them).
_RESET_VEC = [("$r0", 0, 0), ("$r1", 1, 2), ("$r2", 50, 49), ("$r3", -1, -1),
              ("$r4", 0x7FFFFFFF, 0), ("$r5", 0, -0x80000000), ("$r6", 7, 7), ("$r7", 12, 34),
              ("$r0", 3, 0), ("$r1", 0, 3), ("$r2", -5, 5), ("$r3", 100, 200),
              ("$r4", 1, 1), ("$r5", 2, 2), ("$r6", 0x1234, 0x5678), ("$r7", -0x80000000, 0x7FFFFFFF)]

# initGrid: (w, h). (0,0), (0,3), (3,0), (-1,2) and (2,-1) make one or both loops run zero times
# (jge 0x1000c5a3 and jge 0x1000c5c1 taken on the first test); (4,4) is the largest, with 143 as its highest
# entry, which is what sizes the state regions.
_GRID_VEC = [(1, 1), (2, 1), (1, 2), (2, 2), (3, 2), (2, 3), (3, 3), (4, 4), (1, 4), (4, 1),
             (0, 0), (0, 3), (3, 0), (-1, 2), (2, -1)]

# setTextureId: (type, variation, piece, value). No conditional branch in the body; the vectors walk the two
# strides (0x384 per type at 0x10012e8b, 0x24 per variation at 0x10012e94) and the DWORD index at 0x10012ea4.
_TEX_VEC = [(0x24, 0, 0, 0x11110001), (0x24, 0, 1, 0x11110002), (0x24, 0, 8, 0x11110003),
            (0x24, 1, 0, 0x22220001), (0x24, 1, 4, 0x22220002), (0x24, 12, 3, 0x22220003),
            (0x24, 24, 0, 0x33330001), (0x24, 24, 8, 0x33330002), (0x23, 0, 0, 0x44440001),
            (0x23, 0, 8, 0x44440002), (0x23, 5, 2, 0x44440003), (0x23, 24, 8, 0x55550001),
            (0x24, 7, 7, 0x66660001), (0x24, 7, 7, 0), (0x23, 13, 5, 0xFFFFFFFF),
            (0x23, 13, 5, 0x80000000), (0x24, 3, 6, 0x0000FFFF), (0x23, 20, 1, 0x7FFFFFFF),
            (0x24, 18, 2, 0xDEADBEEF), (0x23, 9, 4, 0x00C0FFEE)]

# Tile::setFaceTexture: (tile, type, face, piece). The tiles are one per arm of the type-override switch
# (see the fixture); the type argument, the face and the piece cover the clamp and the face dispatch.
_FACE_VEC = [
    # ---- the type-override switch, one or two vectors per arm
    ("$f0", 0, 0, 0),      # own 1, sub & 0x80 set   -> type 0x1a (je 0x10012f70 not taken)
    ("$f1", 0, 1, 1),      # own 1, sub & 0x80 clear -> unchanged (je taken)
    ("$f2", 5, 2, 2),      # own 6  -> 4
    ("$f3", 5, 3, 3),      # own 21 -> 4
    ("$f4", 0, 4, 1),      # own 7, sub 5  -> 5 % 4 = 1 -> 0x1c
    ("$f5", 3, 5, 2),      # own 7, sub -6 -> remainder -2, ja 0x10012f94 -> unchanged
    ("$f6", 3, 6, 3),      # own 7, sub -8 -> remainder 0 (the dec/or/inc fix-up) -> 0x1b
    ("$f23", 3, 7, 4),     # own 7, sub 8  -> remainder 0, jns taken -> 0x1b
    ("$f18", 1, 0, 1),     # own 7, sub 6  -> 2 -> 0x1d
    ("$f19", 1, 1, 2),     # own 7, sub 7  -> 3 -> 0x1e
    ("$f7", 1, 2, 3),      # own 13 -> 0xd
    ("$f21", 1, 3, 0),     # own 14 -> 0xd
    ("$f22", 1, 4, 1),     # own 15 -> 0xd
    ("$f20", 1, 5, 2),     # own 16 -> 0xd
    ("$f8", 0, 6, 3),      # own 17, sub 1 -> 0x17
    ("$f9", 0, 7, 4),      # own 17, sub 2 -> 0x18 (jne 0x10012f1e taken, jne 0x10012f30 not)
    ("$f10", 5, 0, 0),     # own 17, sub 5 -> unchanged (both jne taken)
    ("$f11", 3, 1, 1),     # own 22, sub 0x40 -> 4
    ("$f12", 3, 2, 2),     # own 22, sub 0x47 -> 4
    ("$f13", 3, 3, 3),     # own 22, sub 0x48 -> unchanged (jge 0x10012f57 taken)
    ("$f14", 3, 4, 0),     # own 22, sub 0x3f -> unchanged (jl 0x10012f4e taken)
    ("$f15", 5, 5, 1),     # own 0  -> ja 0x10012ef3 taken (0 - 1 is 0xffffffff unsigned)
    ("$f16", 5, 6, 2),     # own 23 -> ja taken (22 > 0x15)
    ("$f17", 5, 7, 3),     # own 4  -> table byte 6: no override
    # ---- the piece clamp (cmp 4 at 0x10012fc2, cmp 7 at 0x10012fcb, cmp 2 at 0x10012fd1, jle 0x10012fdb)
    ("$f17", 4, 0, 5),     # type 4, piece 5 -> piece 0
    ("$f17", 4, 1, 3),     # type 4, piece 3 -> kept
    ("$f4", 1, 2, 6),      # own 7, piece 6 -> piece 0
    ("$f4", 1, 3, 2),      # own 7, piece 2 -> kept
    ("$f17", 2, 4, 7),     # type 2, piece 7 -> piece 0
    ("$f17", 2, 5, 4),     # type 2, piece 4 -> kept (jle taken)
    ("$f17", 3, 6, 8),     # none of the three, piece 8 -> kept
    ("$f1", 0x24, 7, 8),   # the last type row of the table, highest piece
    # ---- the face dispatch (ja 0x10012fee) and the eight slots, on one tile so the writes are comparable
    ("$f17", 0, 0, 0), ("$f17", 0, 1, 1), ("$f17", 0, 2, 2), ("$f17", 0, 3, 3),
    ("$f17", 0, 4, 4), ("$f17", 0, 5, 0), ("$f17", 0, 6, 1), ("$f17", 0, 7, 2),
    ("$f17", 0, 8, 3),     # face 8  -> ja taken, nothing written
    ("$f17", 0, -1, 3),    # face -1 -> ja taken
]

# Tile::calcNormals: one vector per tile. $c8 and $c9 have the normals-dirty byte clear, which is the early
# return (jne 0x10012d1a over the jmp at 0x10012d1c).
_NORMALS_VEC = [("$c%d" % k,) for k in range(10)]

HOOKS = {
    # ---------------------------------------------------------------- the corner pair
    "Tile::elevateCorner": dict(
        module="Terrain.dll", addr=0x0000C7B0, abi="thiscall", ret="void", args=["pointer", "int"],
        fixture="c3ba_corner", state=_CORNER_STATE, vectors=_CORNER_VEC),
    # the same tiles with g_splineHeight = 0.1f, which is not exact in binary: the `fstp dword` after
    # `fild; fmul [0x10063e54]` (0x1000c96a) and the mean's `fstp` (0x1000c9dc) both round for real
    "Tile::elevateCorner_odd": dict(
        module="Terrain.dll", addr=0x0000C7B0, abi="thiscall", ret="void", args=["pointer", "int"],
        fixture="c3ba_corner_odd", state=_CORNER_STATE, vectors=_CORNER_VEC),
    "Tile::lowerCorner": dict(
        module="Terrain.dll", addr=0x0000CCC0, abi="thiscall", ret="void", args=["pointer", "int"],
        fixture="c3ba_corner", state=_CORNER_STATE, vectors=_CORNER_VEC),
    # type 7: the sunk-centre block at 0x1000cfc5-0x1000d1cd
    "Tile::lowerCorner_t7": dict(
        module="Terrain.dll", addr=0x0000CCC0, abi="thiscall", ret="void", args=["pointer", "int"],
        fixture="c3ba_corner7", state=_CORNER7_STATE, vectors=_CORNER7_VEC),

    # ---------------------------------------------------------------- the tile itself
    "Tile::reset": dict(
        module="Terrain.dll", addr=0x0000C2C0, abi="thiscall", ret="void", args=["pointer", "int", "int"],
        fixture="c3ba_reset", state=_RESET_STATE, vectors=_RESET_VEC),
    "Tile::setFaceTexture": dict(
        module="Terrain.dll", addr=0x00012EC0, abi="thiscall", ret="void",
        args=["pointer", "int", "int", "int"], fixture="c3ba_face", state=_FACE_STATE, vectors=_FACE_VEC),
    "Tile::calcNormals": dict(
        module="Terrain.dll", addr=0x00012CF0, abi="thiscall", ret="void", args=["pointer"],
        fixture="c3ba_normals", state=_NORMALS_STATE, vectors=_NORMALS_VEC),

    # ---------------------------------------------------------------- the module arrays
    "initGrid": dict(
        module="Terrain.dll", addr=0x0000C560, abi="default", ret="void", args=["int", "int"],
        fixture="c3ba_grid", state=_GRID_STATE, vectors=_GRID_VEC),
    # g_tileW = 0.1f and g_tileH = 1/3: every fmul, fdiv and fsub below 0x1000c5dc then rounds
    "initGrid_odd": dict(
        module="Terrain.dll", addr=0x0000C560, abi="default", ret="void", args=["int", "int"],
        fixture="c3ba_grid_odd", state=_GRID_STATE, vectors=_GRID_VEC),
    "setTextureId": dict(
        module="Terrain.dll", addr=0x00012E70, abi="default", ret="void",
        args=["int", "int", "int", "uint32"], fixture="c3ba_tex", state=_TEX_STATE, vectors=_TEX_VEC),
}

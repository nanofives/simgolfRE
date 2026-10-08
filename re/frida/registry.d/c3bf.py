# Batch c3bf: Terrain.dll, round 15. addr is the RVA (VA = 0x10000000 + RVA).
#
# The tile re-blend / re-smooth group: Tile::blendFaceTextures 0x00013670, Tile::reblendNeighbourhood
# 0x00013500 (nine calls of the first one) and Tile::smoothNormals 0x00011ef0.
# Fixtures: re/frida/js/fixtures.d/c3bf.js. Every Tile and Face is our own allocation. The module memory the
# vectors touch - g_vertices 0x100b28c8, g_normals 0x10070a18, g_courseType 0x10070a0c and the g_collarInfo
# pointer 0x10106b48 - is in the state regions below, so diff_hook snapshots it, compares it after each arm and
# writes it back. g_textures 0x100687f8 is only READ (by Tile::setFaceTexture) and is left out.
# It is all idle at the main menu where the A/B runs: log/c3/alt/c3bf_probe.py counted zero calls into the three
# functions under test and into the five Terrain.dll functions that read or write g_courseType / g_collarInfo
# (Tile::setTypeId, Terrain::rebuild, Terrain::relight, Terrain::loadNewCourseType, Terrain::Terrain) over 8 s.
#
# Hooked callees (the verifier re-runs these keys with SIMGOLF_HOOKS_OFF so the original arm uses original
# callees):
#   Tile::blendFaceTextures     -> Tile::edgeKind          Terrain.dll:15650  (c3as)
#                               -> Tile::isSolidType       Terrain.dll:154a0  (c3am)
#                               -> Tile::isOpenType        Terrain.dll:155b0  (c3as)
#                               -> Terrain::flagCode       Terrain.dll:13f00  (c3as)
#                               -> Terrain::idClass        Terrain.dll:13dd0  (c3as)
#                               -> Tile::setFaceTexture    Terrain.dll:12ec0  (c3ba)
#   Tile::reblendNeighbourhood  -> Tile::blendFaceTextures Terrain.dll:13670  (THIS batch) and, through it,
#                                  all six above
#   Tile::smoothNormals         -> Tile::getFaces          Terrain.dll:153c0  (c3aq)
#                               -> Terrain::faceNormal     Terrain.dll:11d60  (c3as)
#                               -> normalize               Terrain.dll:37c80  (c3as)
#
# All three are `void`: the originals return with `ret 4` / `ret` / `ret 8` and load nothing into eax, so the
# evidence is the state regions (diff_hook's distinct_results counts (return, final state) pairs, which for a
# void function is the number of distinct final states).

_TSIZE = 0x250

# ---------------------------------------------------------------------------------------------------------
# Tile::smoothNormals: 13 tiles, each with the 24 vertex indices k*24.., so entries 0..311 of g_vertices and
# g_normals are the whole working set (312 * 12 = 0xEA0 bytes each). g_vertices is only read by
# Terrain::faceNormal and is listed so that a write by either arm would show.
# ---------------------------------------------------------------------------------------------------------
_SMOOTH_STATE = [("$blk", 0, 13 * _TSIZE), ("$nrm", 0, 0xEA0), ("$vtx", 0, 0xEA0)]

# (tile, a, b). The four arms are (a, b) in {0, 1} x {2, 3} (cmp/jne at 0x10011f9a, 0x10011fa8, 0x100122de,
# 0x1001257b, 0x10012585, 0x10012821); the five subjects cover the five fa/fb/fc combinations of each arm.
_SMOOTH_SUBJECTS = ("$s0", "$s1", "$s2", "$s3", "$s4")
_SMOOTH_VEC = [(s, a, b) for s in _SMOOTH_SUBJECTS for a in (0, 1) for b in (2, 3)]
# `a` outside {0, 1} and `b` outside {2, 3}: the three Faces* are still built (n[2] / n[3] are real pointers on
# $s4) and then nothing is written, which is the fall-through of 0x1001257b and of 0x100122de / 0x10012821
_SMOOTH_VEC += [("$s4", 2, 2), ("$s4", 3, 3), ("$s4", 2, 3), ("$s4", 0, 0), ("$s4", 0, 1), ("$s4", 1, 1)]

# ---------------------------------------------------------------------------------------------------------
# Tile::blendFaceTextures: 33 tiles; only the subject's own eight faces are written (Tile::setFaceTexture
# stores the texture at face+0x24 and the piece byte at face+0x28), so the state is the block plus the two
# module slots the body and Tile::edgeKind read.
# ---------------------------------------------------------------------------------------------------------
_BLEND_STATE = [("$blk", 0, 33 * _TSIZE), ("$courseSlot", 0, 4), ("$collarSlot", 0, 4)]

# (tile, type). -1 makes the body use the tile's own type (0x100136ca).
_BLEND_VEC = [
    # ---- the two early returns
    ("$p13", -1),          # own type 0x14            -> jmp 0x100136d6
    ("$p13", 0x14),        # the argument is 0x14     -> the same return, reached without the -1 path
    ("$p14", -1),          # own type 0, isSolidType false -> jmp 0x10013754
    ("$p14", 0),
    ("$p15", -1),          # own type 9, the other value isSolidType rejects
    ("$p15", 9),
    ("$p0", 0x14),
    # ---- the four neighbour pointers and the four open-neighbour flags, one subject per arm
    ("$p0", -1),           # no neighbour at all: every `je (n == 0)` taken
    ("$p0", 0x11),
    ("$p1", -1),           # n[0] open   -> b0 set    (je 0x10013787 not taken)
    ("$p2", -1),           # n[0] closed -> b0 clear  (je taken)
    ("$p3", -1),           # n[1] open   -> b1 set
    ("$p4", -1),           # n[1] closed -> b1 clear
    ("$p5", -1),           # n[2] open   -> b2 set
    ("$p6", -1),           # n[2] closed -> b2 clear
    ("$p7", -1),           # n[3] open   -> b3 set
    ("$p8", -1),           # n[3] closed -> b3 clear
    # ---- all four neighbours, so the four diagonal blocks run and d0..d3 are computed
    ("$p9", -1),           # every neighbour and diagonal open: b0..b7 all set
    ("$p9", 0x11),
    ("$p10", -1),          # every neighbour and diagonal closed: b0..b7 all clear
    ("$p11", -1),          # mixed diagonals: b4 clear, b5 set, b6 set, b7 clear
    ("$p16", -1),          # four water neighbours: Tile::edgeKind's water path returns 2, 0, 1 and 1
    ("$p12", -1),          # own type 4: Tile::edgeKind's collar path, results 0, 2, 1 and 1
    ("$p12", 0x11),        # the same tile with the argument 0x11, so the b flags run over its neighbours
    # ---- a type argument that is not 0x11 while g_courseType is 1 (jne 0x10013773 taken on all eight tests)
    ("$p9", 4),
    ("$p9", 2),
    ("$p9", 1),
    ("$p9", 0x24),         # the last type row of g_textures
    ("$p9", 0x12),
    ("$p10", 7),
    ("$p11", 0x15),
]

# The same tiles with g_courseType 0: the `jne 0x1001376d` side of all eight open-neighbour tests. $p17's n[0]
# and n[1] have a NULL n[2] and n[3], which only this key can use: with the guard failing those pointers reach
# Tile::edgeKind (0 for NULL) and never Tile::isOpenType.
_BLEND_CT0_VEC = [
    ("$p17", -1), ("$p17", 0x11), ("$p17", 4),
    ("$p0", -1), ("$p1", -1), ("$p5", -1), ("$p9", -1), ("$p9", 0x11), ("$p10", -1),
    ("$p11", -1), ("$p12", -1), ("$p16", -1), ("$p13", -1), ("$p14", -1), ("$p15", -1),
]

# ---------------------------------------------------------------------------------------------------------
# Tile::reblendNeighbourhood: argument-free apart from `this`, so one vector per subject tile. It writes the
# eight faces of up to nine tiles, all inside the block.
# ---------------------------------------------------------------------------------------------------------
_REBLEND_STATE = [("$blk", 0, 27 * _TSIZE), ("$courseSlot", 0, 4), ("$collarSlot", 0, 4)]
_REBLEND_VEC = [("$r%d" % k,) for k in range(11)]

_REBLEND_CT1_STATE = [("$blk", 0, 16 * _TSIZE), ("$courseSlot", 0, 4), ("$collarSlot", 0, 4)]
_REBLEND_CT1_VEC = [("$w%d" % k,) for k in range(16)]

HOOKS = {
    "Tile::blendFaceTextures": dict(
        module="Terrain.dll", addr=0x00013670, abi="thiscall", ret="void", args=["pointer", "int"],
        fixture="c3bf_blend", state=_BLEND_STATE, vectors=_BLEND_VEC),
    "Tile::blendFaceTextures_ct0": dict(
        module="Terrain.dll", addr=0x00013670, abi="thiscall", ret="void", args=["pointer", "int"],
        fixture="c3bf_blend_ct0", state=_BLEND_STATE, vectors=_BLEND_CT0_VEC),

    "Tile::reblendNeighbourhood": dict(
        module="Terrain.dll", addr=0x00013500, abi="thiscall", ret="void", args=["pointer"],
        fixture="c3bf_reblend", state=_REBLEND_STATE, vectors=_REBLEND_VEC),
    "Tile::reblendNeighbourhood_ct1": dict(
        module="Terrain.dll", addr=0x00013500, abi="thiscall", ret="void", args=["pointer"],
        fixture="c3bf_reblend_ct1", state=_REBLEND_CT1_STATE, vectors=_REBLEND_CT1_VEC),

    "Tile::smoothNormals": dict(
        module="Terrain.dll", addr=0x00011EF0, abi="thiscall", ret="void", args=["pointer", "int", "int"],
        fixture="c3bf_smooth", state=_SMOOTH_STATE, vectors=_SMOOTH_VEC),
    # the same tiles with a second set of vertex positions and face normals
    "Tile::smoothNormals_b": dict(
        module="Terrain.dll", addr=0x00011EF0, abi="thiscall", ret="void", args=["pointer", "int", "int"],
        fixture="c3bf_smooth2", state=_SMOOTH_STATE, vectors=_SMOOTH_VEC),
}

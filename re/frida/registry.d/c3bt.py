# Batch c3bt: Terrain.dll, round 18. addr is the RVA (VA = 0x10000000 + RVA).
#
# The exported Terrain methods golf_clean.exe calls (re/tools/import_callers.py); the C3 gate's caller rule is
# satisfied through the exe's import thunks, which c3_verify.py --apply fills in.
#   Terrain::setType          0x000032f0   Terrain::getVariation     0x00003390
#   Terrain::layPath          0x0000a3e0   Terrain::hasConnectedPath 0x0000a450
#   Terrain::updatePath       0x0000a4b0   Terrain::hasPath          0x0000a510
#   Terrain::setWall          0x0000a560   Terrain::elevateCorner    0x0000a5c0
#   Terrain::lowerCorner      0x0000a620   Terrain::lowerEdgeCorner  0x0000a680
#   Terrain::calcNormals      0x0000a6f0   Terrain::calcAllNormals   0x0000a740
#
# Fixtures: re/frida/js/fixtures.d/c3bt.js. Every Tile and every Terrain object is our own allocation; the live
# Terrain instance is never used. The module memory the vectors reach through the callees - g_vertices
# 0x100b28c8, g_normals 0x10070a18, g_courseType 0x10070a0c, the g_collarInfo pointer 0x10106b48,
# g_tileW / g_tileH / g_splineHeight 0x10063e4c and g_viewAngleIndex 0x10070a14 - is in the state regions
# below, plus the CRT per-thread random seed rand 0x10018ce0 uses (the Frida thread's own, see the fixture
# file). g_textures 0x100687f8 is only READ and is left out. All of it is idle at the main menu where the A/B
# runs (log/c3/alt/c3ba_probe.py, log/c3/alt/c3bf_probe.py).
#
# Hooked callees (the verifier re-runs these keys with SIMGOLF_HOOKS_OFF so the original arm uses original
# callees). On instances\c3bt only this batch and the core files are linked, so only Terrain::tileAt is hooked
# there; on the main build all of these are:
#   Terrain::tileAt          Terrain.dll:1d50   (core, shim/src/re/Terrain.cpp) - hasConnectedPath, updatePath,
#                                                calcAllNormals, and lowerEdge under lowerEdgeCorner
#   Tile::getVariation       Terrain.dll:15340  (c3aq)  - Terrain::getVariation
#   Tile::hasPath            Terrain.dll:13320  (c3aq)  - Terrain::hasPath
#   Tile::isConnected        Terrain.dll:13360  (c3aq)  - Terrain::hasConnectedPath
#   Tile::setConnected       Terrain.dll:133a0  (c3aq)  - Terrain::updatePath
#   Tile::layPath            Terrain.dll:13400  (c3aq)  - Terrain::layPath
#   Tile::setVariation       Terrain.dll:2f80   (c3aq)  - Terrain::setType
#   Tile::setRotation        Terrain.dll:15380  (c3aq)  - Terrain::setType
#   Tile::getFaces           Terrain.dll:153c0  (c3aq)  - under setTypeId and calcNormals
#   Tile::setWall            Terrain.dll:15400  (c3am)  - Terrain::setWall
#   Tile::getType            Terrain.dll:1f60   (c3am)  - under the corner pair and setTypeId
#   Tile::isSolidType        Terrain.dll:154a0  (c3am)  - under the re-blend
#   Tile::isHidden           Terrain.dll:15460  (c3am)  - under lowerEdge
#   Tile::getX / Tile::getY  Terrain.dll:5960 / 6810 (c3am) - calcAllNormals and lowerEdge
#   Terrain::faceNormal      Terrain.dll:11d60  (c3as)  - under calcNormals
#   normalize                Terrain.dll:37c80  (c3as)  - under calcNormals
#   Tile::edgeKind           Terrain.dll:15650  (c3as)  - under the re-blend
#   Tile::isOpenType         Terrain.dll:155b0  (c3as)  - under the re-blend (never entered: g_courseType 0)
#   Terrain::flagCode        Terrain.dll:13f00  (c3as)  - under the re-blend
#   Terrain::idClass         Terrain.dll:13dd0  (c3as)  - under the re-blend
#   Tile::elevateCorner      Terrain.dll:c7b0   (c3ba)  - Terrain::elevateCorner
#   Tile::lowerCorner        Terrain.dll:ccc0   (c3ba)  - Terrain::lowerCorner, Terrain::lowerEdgeCorner
#   Tile::calcNormals        Terrain.dll:12cf0  (c3ba)  - Terrain::calcNormals, Terrain::calcAllNormals
#   Tile::setFaceTexture     Terrain.dll:12ec0  (c3ba)  - under the re-blend
#   Tile::smoothNormals      Terrain.dll:11ef0  (c3bf)  - under calcNormals
#   Tile::reblendNeighbourhood / blendFaceTextures  Terrain.dll:13500 / 13670 (c3bf) - under setTypeId
#   Tile::setTypeId and its six callees  Terrain.dll:14020, d480, 15230, de60, d540, 14190, 149e0 (c3bk)
#                                                        - Terrain::setType

_TSIZE = 0x250                  # stride of a standalone tile block
_VSTRIDE = 12 * 24              # bytes of g_vertices per tile: 24 vertex indices of 3 floats

# --------------------------------------------------------------------------------------------------------
# c3bt_tiles: twelve standalone tiles for the four pure field wrappers.
# --------------------------------------------------------------------------------------------------------
_TILES_STATE = [("$blk", 0, 12 * _TSIZE)]
_TILE_SUBJ = ["$t%d" % k for k in range(12)]

# Terrain::getVariation / Terrain::hasPath read one field each: one vector per tile.
_GETVAR_VEC = [("$terr", s) for s in _TILE_SUBJ]
_HASPATH_VEC = [("$terr", s) for s in _TILE_SUBJ]

# Terrain::layPath: both sides of `je 0x1000a401` on the second argument, with a third value (7) to show that
# any non-zero one is collapsed to the literal 1 the body pushes at 0x1000a407, times both values of the
# direction argument, over all twelve tiles (whose types cross the seven Tile::layPath matches). 72 vectors.
_LAYPATH_VEC = [("$terr", s, on, d) for s in _TILE_SUBJ for on in (0, 1, 7) for d in (0, 1)]

# Terrain::setWall: the fourth argument is read as a BYTE (0x1000a57d) and the callee reads only that byte
# (0x10015423), so 0xff is "on" and 0x100 is "off" although both are non-zero dwords. Sides 0, 4 and 8 are the
# ends and the middle of the nine-entry wall arrays at Tile+0x210 and Tile+0x234. 48 vectors.
_SETWALL_VEC = [("$terr", "$t%d" % k, side, h, on)
                for k in range(4) for side in (0, 4, 8)
                for (h, on) in ((0, 0), (1, 1), (-1, 0xFF), (0x7FFFFFFF, 0x100))]

# --------------------------------------------------------------------------------------------------------
# c3bt_grid: a 5 x 4 Terrain object of our own. 0x3a4 + 20 * 0x248 = 0x3094 bytes.
# --------------------------------------------------------------------------------------------------------
_GRID_BYTES = 0x3A4 + 20 * 0x248
_GRID_STATE = [("$terr", 0, _GRID_BYTES)]
_CELLS = [(x, y) for y in range(4) for x in range(5)]

# Terrain::hasConnectedPath: one vector per cell; the connected bytes are 0, 1 and 0xff in turn, so the
# returned byte takes three values over the grid.
_HASCONN_VEC = [("$terr", x, y) for (x, y) in _CELLS]

# Terrain::updatePath: Tile::setConnected tests the whole DWORD (cmp dword [ebp+8], 0 at 0x100133c1), unlike
# Terrain::setWall's byte, so 0x100 and -1 are both "on". Every cell with both plain values, then four cells
# with the two wide ones. 48 vectors.
_UPDATEPATH_VEC = ([("$terr", x, y, on) for (x, y) in _CELLS for on in (0, 1)]
                   + [("$terr", x, y, on) for (x, y) in _CELLS[:4] for on in (0x100, -1)])

# --------------------------------------------------------------------------------------------------------
# c3bt_corner: batch c3ba's sixteen-tile corner wiring. Vertex entries 0..383 = 0x1200 bytes.
# --------------------------------------------------------------------------------------------------------
_CORNER_STATE = [("$blk", 0, 16 * _TSIZE), ("$vtx", 0, 0x1200), ("$cfg", 0, 12)]
# Only the odd corners reach a case of the callees' jump tables (0x1000cb9b / 0x1000d2d3); 2, 4, 6 and
# anything out of 1..7 reach the common tail with the /GZ fill still in the twelve frame slots and fault in
# both arms (batch c3ba's finding 1). The two 0 vectors are the wrapper's own NULL arm.
_CORNER_VEC = ([("$terr", "$t%d" % k, c) for k in range(8) for c in (1, 3, 5, 7)]
               + [("$terr", 0, 1), ("$terr", 0, 3)])

# --------------------------------------------------------------------------------------------------------
# c3bt_normals: batch c3ba's ten tiles. Normal entries 0..239 = 0xB40 bytes.
# --------------------------------------------------------------------------------------------------------
_NORMALS_STATE = [("$blk", 0, 10 * _TSIZE), ("$nrm", 0, 0xB40)]
_CALCNORMALS_VEC = [("$terr", "$c%d" % k) for k in range(10)] + [("$terr", 0)]

# --------------------------------------------------------------------------------------------------------
# c3bt_allnormals: a 5 x 5 Terrain object (0x3a4 + 25 * 0x248 = 0x3cac bytes), twelve probe tiles whose
# coordinates move the window over the grid, and normal entries 0..599 = 0x1c20 bytes.
# --------------------------------------------------------------------------------------------------------
_ALLN_BYTES = 0x3A4 + 25 * 0x248
_ALLN_STATE = [("$terr", 0, _ALLN_BYTES), ("$nrm", 0, 0x1C20), ("$blk", 0, 12 * _TSIZE)]
_ALLN_VEC = [("$terr", "$p%d" % k) for k in range(12)]
_ALLN1_STATE = [("$terr", 0, _ALLN_BYTES), ("$nrm", 0, 0x1C20), ("$blk", 0, 6 * _TSIZE)]
_ALLN1_VEC = [("$terr", "$p%d" % k) for k in range(6)]

# --------------------------------------------------------------------------------------------------------
# c3bt_edge: a 5 x 5 Terrain object for Terrain::lowerEdgeCorner. Every subject is interior, so the three
# coordinate lookups of each of Terrain::lowerEdge's four angle arms are inside the grid and every tile has
# type 4, which is not 0x14: Tile::isHidden is false everywhere and lowerEdge's draw block (Tile::render and
# OpenGL) is never entered. g_viewAngleIndex, which Terrain::setViewAngle writes from the angle, is a state
# region.
# --------------------------------------------------------------------------------------------------------
_EDGE_STATE = [("$terr", 0, _ALLN_BYTES), ("$vtx", 0, 0x1C20), ("$cfg", 0, 12), ("$viewSlot", 0, 4)]
_ANGLES = (0.0, 90.0, 180.0, 45.0)
_EDGE_VEC = ([("$terr", "$e%d" % i, c, "$pv%d" % (i % 2), _ANGLES[(i + j) % 4])
              for i in range(4) for (j, c) in enumerate((1, 3, 5, 7))]
             + [("$terr", "$e0", 1, "$pv1", a) for a in _ANGLES]
             + [("$terr", 0, 1, "$pv0", 0.0), ("$terr", 0, 7, "$pv1", 90.0)])

# --------------------------------------------------------------------------------------------------------
# c3bt_settype: the 21-tile ring block and a Terrain object holding only the variation-count table.
# Vertex entries 0..503 = 0x17a0 bytes.
# --------------------------------------------------------------------------------------------------------
_SETTYPE_STATE = [("$blk", 0, 21 * _TSIZE), ("$vtx", 0, 0x17A0), ("$terr", 0, 0x400),
                  ("$courseSlot", 0, 4), ("$collarSlot", 0, 4), ("$randSlot", 0, 4)]
# Types 0, 4, 1 and 5 have a count of 0 or -3 (the `jle 0x1000331b` side); the other eight have a positive
# count, so rand 0x10018ce0 runs and the per-thread seed changes. The twelve together reach every arm of
# Tile::setTypeId's dispatch: 7, 0x11, 6, the 0xd..0x10 group, 0 and 9, 4 and 0x14, and the default (1, 5,
# 0x24).
_SETTYPE_TYPES = (0, 1, 4, 5, 6, 7, 9, 0xD, 0x10, 0x11, 0x14, 0x24)
_SETTYPE_VEC = [("$terr", s, t, t * 7 + 1)
                for s in ("$a0", "$a1", "$b0", "$b1") for t in _SETTYPE_TYPES]

HOOKS = {
    # ---------------------------------------------------------------- pure field wrappers
    "Terrain::getVariation": dict(
        module="Terrain.dll", addr=0x00003390, abi="thiscall", ret="int", args=["pointer", "pointer"],
        fixture="c3bt_tiles", state=_TILES_STATE, vectors=_GETVAR_VEC),
    "Terrain::hasPath": dict(
        module="Terrain.dll", addr=0x0000A510, abi="thiscall", ret="uint8", args=["pointer", "pointer"],
        fixture="c3bt_tiles", state=_TILES_STATE, vectors=_HASPATH_VEC),
    "Terrain::layPath": dict(
        module="Terrain.dll", addr=0x0000A3E0, abi="thiscall", ret="void",
        args=["pointer", "pointer", "int", "int"],
        fixture="c3bt_tiles", state=_TILES_STATE, vectors=_LAYPATH_VEC),
    "Terrain::setWall": dict(
        module="Terrain.dll", addr=0x0000A560, abi="thiscall", ret="void",
        args=["pointer", "pointer", "int", "int", "int"],
        fixture="c3bt_tiles", state=_TILES_STATE, vectors=_SETWALL_VEC),

    # ---------------------------------------------------------------- tileAt plus a Tile method
    "Terrain::hasConnectedPath": dict(
        module="Terrain.dll", addr=0x0000A450, abi="thiscall", ret="uint8",
        args=["pointer", "int", "int"],
        fixture="c3bt_grid", state=_GRID_STATE, vectors=_HASCONN_VEC),
    "Terrain::updatePath": dict(
        module="Terrain.dll", addr=0x0000A4B0, abi="thiscall", ret="void",
        args=["pointer", "int", "int", "int"],
        fixture="c3bt_grid", state=_GRID_STATE, vectors=_UPDATEPATH_VEC),

    # ---------------------------------------------------------------- the guarded corner wrappers
    "Terrain::elevateCorner": dict(
        module="Terrain.dll", addr=0x0000A5C0, abi="thiscall", ret="void",
        args=["pointer", "pointer", "int"],
        fixture="c3bt_corner", state=_CORNER_STATE, vectors=_CORNER_VEC),
    # the same tiles with g_splineHeight = 0.1f, which is not exact in binary
    "Terrain::elevateCorner_odd": dict(
        module="Terrain.dll", addr=0x0000A5C0, abi="thiscall", ret="void",
        args=["pointer", "pointer", "int"],
        fixture="c3bt_corner_odd", state=_CORNER_STATE, vectors=_CORNER_VEC),
    "Terrain::lowerCorner": dict(
        module="Terrain.dll", addr=0x0000A620, abi="thiscall", ret="void",
        args=["pointer", "pointer", "int"],
        fixture="c3bt_corner", state=_CORNER_STATE, vectors=_CORNER_VEC),
    "Terrain::lowerCorner_odd": dict(
        module="Terrain.dll", addr=0x0000A620, abi="thiscall", ret="void",
        args=["pointer", "pointer", "int"],
        fixture="c3bt_corner_odd", state=_CORNER_STATE, vectors=_CORNER_VEC),

    # ---------------------------------------------------------------- normals
    "Terrain::calcNormals": dict(
        module="Terrain.dll", addr=0x0000A6F0, abi="thiscall", ret="void", args=["pointer", "pointer"],
        fixture="c3bt_normals", state=_NORMALS_STATE, vectors=_CALCNORMALS_VEC),
    "Terrain::calcAllNormals": dict(
        module="Terrain.dll", addr=0x0000A740, abi="thiscall", ret="void", args=["pointer", "pointer"],
        fixture="c3bt_allnormals", state=_ALLN_STATE, vectors=_ALLN_VEC),
    # the window half-width is 13 << [this+0x1c] (shl edx, cl at 0x1000a768): this key sets that field to 1
    "Terrain::calcAllNormals_shift1": dict(
        module="Terrain.dll", addr=0x0000A740, abi="thiscall", ret="void", args=["pointer", "pointer"],
        fixture="c3bt_allnormals_shift1", state=_ALLN1_STATE, vectors=_ALLN1_VEC),

    # ---------------------------------------------------------------- lowerEdge plus lowerCorner
    "Terrain::lowerEdgeCorner": dict(
        module="Terrain.dll", addr=0x0000A680, abi="thiscall", ret="void",
        args=["pointer", "pointer", "int", "pointer", "float"],
        fixture="c3bt_edge", state=_EDGE_STATE, vectors=_EDGE_VEC),

    # ---------------------------------------------------------------- variation, rotation, type
    "Terrain::setType": dict(
        module="Terrain.dll", addr=0x000032F0, abi="thiscall", ret="void",
        args=["pointer", "pointer", "int", "int"],
        fixture="c3bt_settype", state=_SETTYPE_STATE, vectors=_SETTYPE_VEC),
}

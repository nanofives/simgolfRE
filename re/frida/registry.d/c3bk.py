# Batch c3bk: Terrain.dll, round 16. addr is the RVA (VA = 0x10000000 + RVA).
#
# Tile::setTypeId 0x00014020 and the six callees it dispatches to:
#   Tile::setPathFlagForType 0x0000d480   Tile::resetFaceTextures 0x00015230
#   Tile::enterType6         0x0000de60   Tile::enterType7        0x0000d540
#   Tile::leaveType6         0x00014190   Tile::leaveType7        0x000149e0
#
# Fixtures: re/frida/js/fixtures.d/c3bk.js. Every Tile is our own allocation. The module memory the vectors
# touch - g_vertices 0x100b28c8, g_courseType 0x10070a0c and the g_collarInfo pointer 0x10106b48 - is in the
# state regions below, so diff_hook snapshots it, compares it after each arm and writes it back. g_textures
# 0x100687f8 is only READ (by Tile::resetFaceTextures and by Tile::setFaceTexture under the re-blend) and is
# left out. All of it is idle at the main menu where the A/B runs (batch c3bf's log/c3/alt/c3bf_probe.py).
#
# All seven are `void`: the originals return with `ret 4` / `ret` and load nothing into eax, so the evidence
# is the state regions (diff_hook's distinct_results counts (return, final state) pairs, which for a void
# function is the number of distinct final states).
#
# Hooked callees (the verifier re-runs these keys with SIMGOLF_HOOKS_OFF so the original arm uses original
# callees):
#   every key               -> Tile::getType               Terrain.dll:1f60   (c3am)
#   the four corner walks   -> Tile::getFaces              Terrain.dll:153c0  (c3aq)
#   all but the two leaves  -> Tile::reblendNeighbourhood  Terrain.dll:13500  (c3bf) and through it
#                              Tile::blendFaceTextures     Terrain.dll:13670  (c3bf),
#                              Tile::edgeKind              Terrain.dll:15650  (c3as),
#                              Tile::isSolidType           Terrain.dll:154a0  (c3am),
#                              Tile::isOpenType            Terrain.dll:155b0  (c3as),
#                              Terrain::flagCode           Terrain.dll:13f00  (c3as),
#                              Terrain::idClass            Terrain.dll:13dd0  (c3as),
#                              Tile::setFaceTexture        Terrain.dll:12ec0  (c3ba)
#   Tile::setTypeId         -> the other six functions of THIS batch

_TSIZE = 0x250
_VSTRIDE = 12 * 24          # bytes of g_vertices per tile: 24 vertex indices of 3 floats


def _state(ntiles):
    """The tile block, the g_vertices window the block's indices cover, and the two module slots."""
    return [("$blk", 0, ntiles * _TSIZE), ("$vtx", 0, ntiles * _VSTRIDE),
            ("$courseSlot", 0, 4), ("$collarSlot", 0, 4)]


# ---------------------------------------------------------------------------------------------------------
# The four corner walks. 32 tiles; e0..e14 are the subjects, one per combination of the four neighbour
# pointers and of the three tests inside each diagonal block (see the fixture file). They take no argument
# apart from `this`, so one vector is one call: three keys give each function 45 of them.
#   <name>       the fixture of the function's OWN type: the guards accept the neighbours
#   <name>_off   the fixture of the other type: every `cmp eax, 6` / `cmp eax, 7` fails at once, which is the
#                untaken side of all thirteen type tests (and, for Tile::enterType7, of the eight type tests
#                of its dirty walk)
#   <name>_b     the same tiles with a second, unrelated set of heights in g_vertices
# ---------------------------------------------------------------------------------------------------------
_WALK_STATE = _state(32)
_WALK_VEC = [("$e%d" % k,) for k in range(15)]

# ---------------------------------------------------------------------------------------------------------
# Tile::setPathFlagForType: six subjects crossing both sides of `if (hasPath)` (je 0x1000d4aa) with both
# starting values of the flag it writes, with and without a neighbourhood for the re-blend. The type argument
# sweeps the nine values the chain matches (0x14, 0x11, 0x16, 0, 2, 1, 3, 7, 9 - the je at 0x1000d4b0,
# 0x1000d4b6, 0x1000d4bc, 0x1000d4c2, 0x1000d4c8, 0x1000d4ce, 0x1000d4d4, 0x1000d4da and the jne at
# 0x1000d4e0) and four it does not. 6 x 13 = 78 vectors.
# ---------------------------------------------------------------------------------------------------------
_PATH_STATE = _state(11)
_PATH_SUBJ = ("$pa", "$pb", "$pc", "$pd", "$pe", "$pf")
_PATH_TYPES = [0x14, 0x11, 0x16, 0, 2, 1, 3, 7, 9, 5, 0xd, -1, 0x7FFFFFFF]
_PATH_VEC = [(s, t) for s in _PATH_SUBJ for t in _PATH_TYPES]

# ---------------------------------------------------------------------------------------------------------
# Tile::resetFaceTextures: no argument apart from `this`, so one vector per subject. The fourteen subjects
# cross hasPath (je 0x1001525a), type == 0 (jne 0x1001526d) and both loop bounds (jge 0x1001528a and
# 0x100152c7), including a faceCount of 0 on each side and a partial count.
# ---------------------------------------------------------------------------------------------------------
_RESET_STATE = _state(19)
_RESET_VEC = [("$q%d" % k,) for k in range(14)]

# ---------------------------------------------------------------------------------------------------------
# Tile::setTypeId: eight subjects (four in a ring of type-6 neighbours, four in a ring of type-7 ones) times
# eighteen new types, which reach every arm of the dispatch: 7, 0x11, 6, the 0xd..0x10 group, 0 and 9, 4 and
# 0x14, and ten values that fall to the default (1, 5, 8, 0xa, 0x12, 0x15 below the jump table's 0x14 bound
# and 0x24 above it). The subjects' own starting types give the leave step its three cases (7, 6 and 0x11)
# and a fourth that is none of them. 8 x 18 = 144 vectors.
# ---------------------------------------------------------------------------------------------------------
_SETTYPE_STATE = _state(25)
_SETTYPE_SUBJ = ("$a0", "$a1", "$a2", "$a3", "$b0", "$b1", "$b2", "$b3")
_SETTYPE_TYPES = [0, 1, 4, 5, 6, 7, 8, 9, 0xA, 0xD, 0xE, 0xF, 0x10, 0x11, 0x12, 0x14, 0x15, 0x24]
_SETTYPE_VEC = [(s, t) for s in _SETTYPE_SUBJ for t in _SETTYPE_TYPES]

HOOKS = {
    "Tile::setTypeId": dict(
        module="Terrain.dll", addr=0x00014020, abi="thiscall", ret="void", args=["pointer", "int"],
        fixture="c3bk_settype", state=_SETTYPE_STATE, vectors=_SETTYPE_VEC),

    "Tile::setPathFlagForType": dict(
        module="Terrain.dll", addr=0x0000D480, abi="thiscall", ret="void", args=["pointer", "int"],
        fixture="c3bk_path", state=_PATH_STATE, vectors=_PATH_VEC),

    "Tile::resetFaceTextures": dict(
        module="Terrain.dll", addr=0x00015230, abi="thiscall", ret="void", args=["pointer"],
        fixture="c3bk_reset", state=_RESET_STATE, vectors=_RESET_VEC),

    "Tile::enterType6": dict(
        module="Terrain.dll", addr=0x0000DE60, abi="thiscall", ret="void", args=["pointer"],
        fixture="c3bk_walk6", state=_WALK_STATE, vectors=_WALK_VEC),
    "Tile::enterType6_off": dict(
        module="Terrain.dll", addr=0x0000DE60, abi="thiscall", ret="void", args=["pointer"],
        fixture="c3bk_walk7", state=_WALK_STATE, vectors=_WALK_VEC),
    "Tile::enterType6_b": dict(
        module="Terrain.dll", addr=0x0000DE60, abi="thiscall", ret="void", args=["pointer"],
        fixture="c3bk_walk6b", state=_WALK_STATE, vectors=_WALK_VEC),

    "Tile::enterType7": dict(
        module="Terrain.dll", addr=0x0000D540, abi="thiscall", ret="void", args=["pointer"],
        fixture="c3bk_walk7", state=_WALK_STATE, vectors=_WALK_VEC),
    "Tile::enterType7_off": dict(
        module="Terrain.dll", addr=0x0000D540, abi="thiscall", ret="void", args=["pointer"],
        fixture="c3bk_walk6", state=_WALK_STATE, vectors=_WALK_VEC),
    "Tile::enterType7_b": dict(
        module="Terrain.dll", addr=0x0000D540, abi="thiscall", ret="void", args=["pointer"],
        fixture="c3bk_walk7b", state=_WALK_STATE, vectors=_WALK_VEC),

    "Tile::leaveType6": dict(
        module="Terrain.dll", addr=0x00014190, abi="thiscall", ret="void", args=["pointer"],
        fixture="c3bk_walk6", state=_WALK_STATE, vectors=_WALK_VEC),
    "Tile::leaveType6_off": dict(
        module="Terrain.dll", addr=0x00014190, abi="thiscall", ret="void", args=["pointer"],
        fixture="c3bk_walk7", state=_WALK_STATE, vectors=_WALK_VEC),
    "Tile::leaveType6_b": dict(
        module="Terrain.dll", addr=0x00014190, abi="thiscall", ret="void", args=["pointer"],
        fixture="c3bk_walk6b", state=_WALK_STATE, vectors=_WALK_VEC),

    "Tile::leaveType7": dict(
        module="Terrain.dll", addr=0x000149E0, abi="thiscall", ret="void", args=["pointer"],
        fixture="c3bk_walk7", state=_WALK_STATE, vectors=_WALK_VEC),
    "Tile::leaveType7_off": dict(
        module="Terrain.dll", addr=0x000149E0, abi="thiscall", ret="void", args=["pointer"],
        fixture="c3bk_walk6", state=_WALK_STATE, vectors=_WALK_VEC),
    "Tile::leaveType7_b": dict(
        module="Terrain.dll", addr=0x000149E0, abi="thiscall", ret="void", args=["pointer"],
        fixture="c3bk_walk7b", state=_WALK_STATE, vectors=_WALK_VEC),
}

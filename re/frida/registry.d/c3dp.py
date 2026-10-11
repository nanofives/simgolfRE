# Batch c3dp (Terrain.dll, round 27). addr is the RVA (VA = 0x10000000 + RVA). Bodies: shim/src/re/c3dp.cpp,
# fixtures: re/frida/js/fixtures.d/c3dp.js. Keys with `<`, `>` or `*` must be handed to diff_hook from Python.
#
# Hooked callees (the verifier re-runs these keys with SIMGOLF_HOOKS_OFF so the original arm uses original callees):
#   push_back        -> end afe0 (c3aw), insert b100 (this batch)
#   insert           -> _Buynode b500 (c3cn), _Acc::_Next/_Prev/_Value b600/b630/b660 (c3as), construct b690 (c3aw),
#                       _Mynode b710 and iterator::iterator b750 (c3aw)
#   clear            -> begin af70, end afe0 (c3aw), erase b470 (this batch)
#   ~list            -> begin, end (c3aw), erase b470 (this batch), _Freenode b5b0 (c3cn)
#   erase (range)    -> operator!= b3d0 (c3aw), iterator::operator++ b320 (c3aw), erase b800 (this batch)
#   erase (single)   -> operator++ b320 (c3aw), _Mynode b710 (c3aw), _Freenode b5b0 (c3cn), _Acc b600/b630/b660 (c3as)
#   reverse_iterator ctor -> const_iterator ctor b6e0 (this batch)
#   ios::lock / unlock -> ios::lockptr 3f80 (this batch)
#   Tile::~Tile      -> Tile::setNeighbour c520 (c3aq/c3am)
#   Tile::Tile       -> PathInfo::PathInfo f690 and WallInfo::WallInfo f750 (c3aw)

# The whole fixture block, the arena operator new carves from, and the log operator delete appends to.
_LST = [("$blk", 0, 0x3000), ("$arena", 0, 0x1010), ("$freelog", 0, 0x404)]
_IOS = [("$blk", 0, 0x800), ("$implog", 0, 0x400)]
_TILES = [("$blk", 0, 12 * 0x260)]
_TILEN = [("$blk", 0, 60 * 0x260)]
_GLLOG = [("$implog", 0, 0x400)]
_TEXLOG = [("$blk", 0, 8 * 0x400), ("$implog", 0, 0x10000), ("$tbl", 0, 0x25 * 0x384)]
_GRID = [("$blk", 0, 11 * 0x3000), ("$vtx", 0, 0x2000), ("$nrm", 0, 0x2000), ("$cfg", 0, 8)]

HOOKS = {
    # ------------------------------------------------------------------------------------------- iterator ctors
    # const_iterator::const_iterator: leaf, returns `this` and writes nothing. 16 `this` pointers, two of them
    # NULL and one odd, so the only thing that varies (and is compared) is the returned address.
    "std::list<Tile*>::const_iterator::const_iterator": dict(
        module="Terrain.dll", addr=0x0000B6E0, abi="thiscall", ret="pointer", args=["pointer"],
        fixture="c3dp_lists", state=_LST,
        vectors=[("$c%d" % i,) for i in range(14)] + [(0,), (1,)]),
    "std::reverse_bidirectional_iterator<std::list<Tile*>::iterator>::ctor": dict(
        module="Terrain.dll", addr=0x0000B280, abi="thiscall", ret="pointer", args=["pointer"],
        fixture="c3dp_lists", state=_LST,
        vectors=[("$c%d" % i,) for i in range(14)] + [(0,), (1,)]),

    # ------------------------------------------------------------------------------------------- push_back / insert
    # push_back: (this, value reference). End-of-list insertion into lists of 0..9 nodes, so the new node is
    # linked between the last node and the head each time; the node comes from the stand-in arena.
    "std::list<Tile*>::push_back": dict(
        module="Terrain.dll", addr=0x0000B090, abi="thiscall", ret="void", args=["pointer", "pointer"],
        fixture="c3dp_lists", state=_LST,
        vectors=[("$L%d" % i, "$v%d" % ((i * 3) % 16)) for i in range(8)]
                + [("$L3", "$v1"), ("$L6", "$v7"), ("$L7", "$v9"), ("$L2", "$v15")]),
    # insert: (this, return slot, position, value reference). Positions: the head (append), the first, a middle and
    # the last node of lists of 0..9 nodes.
    "std::list<Tile*>::insert": dict(
        module="Terrain.dll", addr=0x0000B100, abi="thiscall", ret="pointer",
        args=["pointer", "pointer", "pointer", "pointer"], fixture="c3dp_lists", state=_LST,
        vectors=[("$L0", "$r0", "$S0", "$v0"), ("$L1", "$r1", "$S1", "$v1"), ("$L1", "$r2", "$N1_0", "$v2"),
                 ("$L2", "$r3", "$N2_0", "$v3"), ("$L2", "$r4", "$N2_1", "$v4"), ("$L2", "$r5", "$S2", "$v5"),
                 ("$L3", "$r6", "$N3_1", "$v6"), ("$L4", "$r7", "$N4_2", "$v7"), ("$L5", "$r8", "$N5_0", "$v8"),
                 ("$L6", "$r9", "$N6_5", "$v9"), ("$L7", "$r10", "$N7_4", "$v10"), ("$L7", "$r11", "$S7", "$v11")]),

    # ------------------------------------------------------------------------------------------- erase / clear / dtor
    # erase (single position): (this, return slot, position). Every position is a real node, never the head.
    "std::list<Tile*>::erase_1000b800": dict(
        module="Terrain.dll", addr=0x0000B800, abi="thiscall", ret="pointer",
        args=["pointer", "pointer", "pointer"], fixture="c3dp_lists", state=_LST,
        vectors=[("$L1", "$r0", "$N1_0"), ("$L2", "$r1", "$N2_0"), ("$L2", "$r2", "$N2_1"), ("$L3", "$r3", "$N3_0"),
                 ("$L3", "$r4", "$N3_1"), ("$L3", "$r5", "$N3_2"), ("$L4", "$r6", "$N4_2"), ("$L5", "$r7", "$N5_4"),
                 ("$L6", "$r8", "$N6_0"), ("$L6", "$r9", "$N6_5"), ("$L7", "$r10", "$N7_8"), ("$L7", "$r11", "$N7_4")]),
    # erase (range): (this, return slot, first, last). Empty ranges (first == last, including the empty list's
    # head), one node, a prefix, a suffix up to the head, the whole list.
    "std::list<Tile*>::erase": dict(
        module="Terrain.dll", addr=0x0000B470, abi="thiscall", ret="pointer",
        args=["pointer", "pointer", "pointer", "pointer"], fixture="c3dp_lists", state=_LST,
        vectors=[("$L0", "$r0", "$S0", "$S0"), ("$L1", "$r1", "$N1_0", "$S1"), ("$L2", "$r2", "$N2_0", "$N2_1"),
                 ("$L2", "$r3", "$N2_0", "$S2"), ("$L3", "$r4", "$N3_1", "$S3"), ("$L4", "$r5", "$N4_1", "$N4_3"),
                 ("$L5", "$r6", "$N5_2", "$N5_2"), ("$L6", "$r7", "$N6_0", "$S6"), ("$L7", "$r8", "$N7_3", "$N7_8"),
                 ("$L7", "$r9", "$N7_0", "$N7_8"), ("$L7", "$r10", "$N7_2", "$N7_5"), ("$L6", "$r11", "$N6_4", "$S6")]),
    # clear: one call per list; $L0 is empty (the loop body never runs), the others free 1..9 nodes.
    "std::list<Tile*>::clear": dict(
        module="Terrain.dll", addr=0x0000B210, abi="thiscall", ret="void", args=["pointer"],
        fixture="c3dp_lists", state=_LST, vectors=[("$L%d" % i,) for i in range(8)]),
    # ~list: clear plus the head node's release and the zeroed +4 / +8.
    "std::list<Tile*>::~list": dict(
        module="Terrain.dll", addr=0x0000AED0, abi="thiscall", ret="void", args=["pointer"],
        fixture="c3dp_lists", state=_LST, vectors=[("$L%d" % i,) for i in range(8)]),

    # ------------------------------------------------------------------------------------------- iostream helpers
    # fail: leaf, [this+8] & 6; 16 state words.
    "ios::fail": dict(module="Terrain.dll", addr=0x00002510, abi="thiscall", ret="int", args=["pointer"],
                      fixture="c3dp_ios", vectors=[("$io%d" % i,) for i in range(16)]),
    # lockptr: leaf, this + 0x38; 16 objects.
    "ios::lockptr": dict(module="Terrain.dll", addr=0x00003F80, abi="thiscall", ret="pointer", args=["pointer"],
                         fixture="c3dp_ios", vectors=[("$io%d" % i,) for i in range(16)]),
    # lock / unlock: static cdecl, one pointer. Lock counts include 0, positive values, -1, -2, INT_MIN and INT_MAX,
    # so the signed `jge` is read on both sides; the import log shows which calls were made with which section.
    "ios::lock": dict(module="Terrain.dll", addr=0x000018B0, abi="default", ret="void", args=["pointer"],
                      fixture="c3dp_ios", state=_IOS, vectors=[("$io%d" % i,) for i in range(16)]),
    "ios::unlock": dict(module="Terrain.dll", addr=0x00037930, abi="default", ret="void", args=["pointer"],
                        fixture="c3dp_ios", state=_IOS, vectors=[("$io%d" % i,) for i in range(16)]),

    # ------------------------------------------------------------------------------------------- Tile / Terrain
    "Tile::Tile": dict(module="Terrain.dll", addr=0x0000C210, abi="thiscall", ret="pointer", args=["pointer"],
                       fixture="c3dp_tiles", state=_TILES, vectors=[("$t%d" % i,) for i in range(12)]),
    "Tile::~Tile": dict(module="Terrain.dll", addr=0x0000C3D0, abi="thiscall", ret="void", args=["pointer"],
                        fixture="c3dp_tile_dtor", state=_TILEN, vectors=[("$t%d" % i,) for i in range(12)]),
    # setViewAngle: (this, angle). 0.0 and -0.0 and NaN take the C3 test, 90.0 and 180.0 the two bit compares, the
    # rest (including the neighbours of 90 and 180 in the last mantissa bit) the default arm.
    "Terrain::setViewAngle": dict(
        module="Terrain.dll", addr=0x0000ADC0, abi="thiscall", ret="void", args=["pointer", "float"],
        fixture="c3dp_angle", state=[(0x10070A14, 0, 4)],
        vectors=[(0, v) for v in (0.0, -0.0, 90.0, 180.0, 270.0, 360.0, 45.0, 90.00000762939453, 89.99999237060547,
                                  180.0000152587890625, 179.99998474121094, "$nan", "$pinf", "$ninf", -90.0, 1e30,
                                  0.5, 1e-40, -180.0)]),

    # ------------------------------------------------------------------------------------------- OpenGL setup
    # initGL: (this). Front face from the BYTE at +0x28: zero side for 0 and for dwords whose low byte is 0, non-zero
    # side for the rest. The ten GL calls are the compared log. Result: the bool in al ("uint8").
    "Terrain::initGL": dict(module="Terrain.dll", addr=0x000033E0, abi="thiscall", ret="uint8", args=["pointer"],
                            fixture="c3dp_gl_init", state=_GLLOG, vectors=[("$g%d" % i,) for i in range(8)]),
    # light1Off: (this), never read; the call log is the evidence.
    "Terrain::light1Off": dict(module="Terrain.dll", addr=0x00003830, abi="thiscall", ret="void", args=["pointer"],
                               fixture="c3dp_gl_light", state=_GLLOG, vectors=[("$g%d" % i,) for i in range(4)]),
    # resize: (this, width, height). The four flip values x the exact sizes with substitutes (800x600, 1024x768,
    # 1280x1024), zero and negative sizes, odd sizes (the `neg; sar` asymmetry) and near misses of the three sizes.
    "Terrain::resize": dict(
        module="Terrain.dll", addr=0x00009ED0, abi="thiscall", ret="void", args=["pointer", "int", "int"],
        fixture="c3dp_gl_size", state=_GLLOG,
        vectors=[("$f0", 800, 600), ("$f1", 800, 600), ("$f0", 1024, 768), ("$f1", 1024, 768), ("$f0", 1280, 1024),
                 ("$f1", 1280, 1024), ("$f2", 800, 600), ("$f3", 1024, 768), ("$f0", 0, 0), ("$f1", 0, 5),
                 ("$f0", 5, 0), ("$f0", 801, 601), ("$f1", 641, 479), ("$f0", 1, 1), ("$f1", 1024, 600),
                 ("$f0", 800, 768), ("$f1", 1280, 768), ("$f0", 1280, 1025), ("$f1", -3, -5), ("$f0", -800, -600),
                 ("$f2", 0x7FFFFFFF, 3), ("$f3", 3, 0x7FFFFFFF)]),
    # reloadTextures: 8 objects, 3 with the loaded flag clear (byte +0x29 is 0) and 5 set (1, 0xff, 0x80, 2, 0x101).
    # A set flag deletes 925 rows, zeroes the table and the 37 type words.
    "Terrain::reloadTextures": dict(module="Terrain.dll", addr=0x000380A0, abi="thiscall", ret="void",
                                    args=["pointer"], fixture="c3dp_gl_tex", state=_TEXLOG,
                                    vectors=[("$T%d" % i,) for i in range(8)]),
    # buildArrays: (this). 11 grids, among them the zero-width and zero-height ones (the loops run zero times, but
    # initGrid still runs) and non-square ones. Result: the bool in al.
    "Terrain::buildArrays": dict(module="Terrain.dll", addr=0x0000A130, abi="thiscall", ret="uint8", args=["pointer"],
                                 fixture="c3dp_grid", state=_GRID, vectors=[("$G%d" % i,) for i in range(11)]),
}


# =============================================================================================================
# The Tile draw family (Tile::render and the four functions it calls). Fixture c3dp_pool_v<N>: 40 deterministic tiles
# ($t0..$t39, js/fixtures.d/c3dp.js buildPool) under view quadrant N (g_viewAngleIndex 0x10070a14), the import stand-in
# over the OpenGL slots, g_vertices / g_normals / g_textures seeded and put back after the key. State: the tile block
# (nothing may write a tile) and the 0x20000-byte GL call log.
#
# Hooked callees (callee-off runs):
#   Tile::render        -> Tile::getType 1f60 (c3am); drawPaths 108f0, drawWalls f7f0, drawSkirts ea30 (this batch);
#                          drawPaths -> drawCornerOverlays 116d0 (this batch); drawSkirts -> Tile::getType and itself
_PL = [("$blk", 0, 40 * 0x260), ("$implog", 0, 0x20000)]


def _views(name, base, **kw):
    """One registry entry per view quadrant: <name> (quadrant 0), <name>_v1, _v2, _v3."""
    out = {}
    for q in range(4):
        spec = dict(base)
        spec["fixture"] = "c3dp_pool_v%d" % q
        spec.update(kw)
        out[name if q == 0 else "%s_v%d" % (name, q)] = spec
    return out


HOOKS.update({
    # drawCornerOverlays (this, mask, type): masks 0..15 (every switch case), then masks outside the table (16, 17, 31, -1,
    # 0x100) that return at once, with the two texture types the path code passes (0x20, 0x21) and two others.
    "Tile::drawCornerOverlays": dict(
        module="Terrain.dll", addr=0x000116D0, abi="thiscall", ret="void", args=["pointer", "int", "int"],
        fixture="c3dp_pool_v0", state=_PL,
        vectors=[("$t%d" % (m + 1), m, 0x20 + (m & 1)) for m in range(16)]
                + [("$t17", m, 0x20) for m in (16, 17, 31, -1, 0x100, 0x7FFFFFFF)]
                + [("$t20", 5, 2), ("$t21", 15, 3), ("$t22", 0, 0x24), ("$t23", 9, 0)]),
    # drawPaths (this): all 40 tiles; drawPath byte clear for i % 4 == 3, the neighbour flags and the connected byte vary.
    "Tile::drawPaths": dict(
        module="Terrain.dll", addr=0x000108F0, abi="thiscall", ret="void", args=["pointer"],
        fixture="c3dp_pool_v0", state=_PL, vectors=[("$t%d" % i,) for i in range(40)]),
})
HOOKS.update(_views("Tile::drawWalls", dict(module="Terrain.dll", addr=0x0000F7F0, abi="thiscall", ret="void",
                                            args=["pointer"], state=_PL),
                    vectors=[("$t%d" % i,) for i in range(40)]))
HOOKS.update(_views("Tile::drawSkirts", dict(module="Terrain.dll", addr=0x0000EA30, abi="thiscall", ret="void",
                                             args=["pointer", "int"], state=_PL),
                    vectors=[("$t%d" % i, d) for i in range(40) for d in ((0, 3) if i % 2 else (1, 2))]))
HOOKS.update(_views("Tile::render", dict(module="Terrain.dll", addr=0x0000E6C0, abi="thiscall", ret="void",
                                         args=["pointer"], state=_PL),
                    vectors=[("$t%d" % i,) for i in range(40)]))


# =============================================================================================================
# isCulled, drawTile, drawTileObjects, stripRender, lowerEdge. Fixtures in js/fixtures.d/c3dp.js.
#
# Hooked callees (callee-off runs): all five call Tile::getX 5960 / getY 6810 / isHidden 15460 (c3am) and Terrain::tileAt 1d50
# (core); isCulled -> Tile::maxHeight 15500 (c3am); drawTile -> list push_back b090, insert b100, empty b040 (c3aw), begin af70, end afe0,
# operator!= b3d0, operator++ b320, operator* b2d0 (c3aw); drawTileObjects -> Terrain::getWall 1e80, Tile::getWall 1ed0 (c3am),
# isCulled, drawTile, itself; stripRender -> setViewAngle adc0, isCulled, drawTile, drawTileObjects, Tile::render e6c0, clear b210
# (this batch); lowerEdge -> setViewAngle, light1Off 3830, Tile::render (this batch).

# --- isCulled -----------------------------------------------------------------------------------------------------
_CULL_ALL = list(range(36))
_CULL_SOME = [0, 5, 9, 13, 17, 21, 25, 29, 33, 34, 35, 26]
_ANG = [0.0, 90.0, 180.0, 45.0]
_cull_vec = [("$ts0", "$c%d" % i, "$ctr", a) for a in _ANG for i in _CULL_ALL]
for _ts in range(1, 10):
    _cull_vec += [("$ts%d" % _ts, "$c%d" % i, "$ctr", a) for a in _ANG for i in _CULL_SOME]
_cull_vec += [("$ts0", "$c%d" % i, "$ctr", a) for a in (-0.0, "$nan", 270.0, 360.0, 90.00000762939453, 179.99998474121094,
                                                         1e30, -90.0) for i in (0, 9, 21, 29)]
HOOKS.update({
    "Terrain::isCulled": dict(module="Terrain.dll", addr=0x00006850, abi="thiscall", ret="uint8",
                              args=["pointer", "pointer", "pointer", "float"], fixture="c3dp_cull", vectors=_cull_vec),
})

# --- drawTile -----------------------------------------------------------------------------------------------------
_DT_STATE = [("$blk", 0, 0x4000), ("$pool", 0, 40 * 0x260), ("$arena", 0, 0x1010), ("$freelog", 0, 0x404)]
_DT_PROBES = [0, 1, 3, 5, 10, 16, 17, 18, 19, 22, 25, 26, 32, 34, 36, 38]
HOOKS.update({
    "Terrain::drawTile": dict(
        module="Terrain.dll", addr=0x000381A0, abi="thiscall", ret="void",
        args=["pointer", "pointer", "pointer", "float"], fixture="c3dp_dtile", state=_DT_STATE,
        vectors=[("$L%d" % li, "$p%d" % p, "$ctr", a) for li in range(10) for a in (0.0, 90.0, 180.0, 45.0) for p in _DT_PROBES]),
})

# --- drawTileObjects ----------------------------------------------------------------------------------------------
_DO_STATE = [("$grid", 0, 64 * 0x248), ("$list", 0, 0xc), ("$head", 0, 0xc), ("$arena", 0, 0x4010), ("$freelog", 0, 0x404)]
_DO_SUBJ = [0, 7, 9, 10, 18, 21, 27, 28, 36, 45, 54, 56, 63]
HOOKS.update({
    "Terrain::drawTileObjects": dict(
        module="Terrain.dll", addr=0x00007380, abi="thiscall", ret="void",
        args=["pointer", "pointer", "pointer", "float"], fixture="c3dp_dobj", state=_DO_STATE,
        vectors=[("$obj", "$tile%d" % s, "$tile%d" % c, a) for a in (0.0, 90.0, 180.0, 45.0) for s in _DO_SUBJ for c in (27, 36)]),
})

# --- stripRender / lowerEdge --------------------------------------------------------------------------------------
_SC_STATE = [("$list", 0, 0xc), ("$head", 0, 0xc), ("$arena", 0, 0x8010), ("$freelog", 0, 0x404), ("$implog", 0, 0x30000),
             ("$view", 0, 4)]
_SC_PIV = [25 + 25 * 50, 3 + 3 * 50, 46 + 46 * 50, 0, 25 + 2 * 50]
_SC_VEC = [("$obj", "$tile%d" % p, d, a) for p in _SC_PIV for d in range(0, 10) for a in (0.0, 90.0, 180.0, 45.0)]
for _z, _suffix in ((0, ""), (1, "_z1"), (2, "_z2")):
    HOOKS["Terrain::stripRender" + _suffix] = dict(
        module="Terrain.dll", addr=0x00009270, abi="thiscall", ret="void", args=["pointer", "pointer", "int", "float"],
        fixture="c3dp_scene_z%d" % _z, state=_SC_STATE, vectors=_SC_VEC)
_LE_POS = [(25, 25), (24, 25), (10, 10), (0, 0), (49, 49), (0, 25), (25, 0), (49, 25), (25, 49), (7, 7), (14, 14), (21, 3),
           (30, 30), (40, 10), (5, 44), (33, 17), (1, 1), (48, 48)]
HOOKS["Terrain::lowerEdge"] = dict(
    module="Terrain.dll", addr=0x00038900, abi="thiscall", ret="void", args=["pointer", "pointer", "pointer", "float"],
    fixture="c3dp_scene_z0", state=_SC_STATE,
    vectors=[("$obj", "$tile%d" % (x + 50 * y), "$tile%d" % (25 + 25 * 50), a) for (x, y) in _LE_POS for a in (0.0, 90.0, 180.0, 45.0)])


# =============================================================================================================
# The exported scene drawers: render, localRender, pathUpdateRender (fixture c3dp_scene_z<N>: the 50 x 50 grid, the GL and heap stand-ins,
# zoom N), setZoomLevel, drawLine, initTerrain, resetTerrain (small Terrain stand-ins and the GL stand-in). The exe callers are in
# `re/tools/import_callers.py` (0x004498a0 for the first four, 0x004493d0, 0x00449790, 0x00449520).
#
# Hooked callees (callee-off runs): render -> setViewAngle adc0, isCulled 6850, Tile::render e6c0 (this batch), tileAt 1d50 (core), getX/getY/
# isHidden (c3am); localRender -> setViewAngle, drawTile 381a0, drawTileObjects 7380, Tile::render, list clear b210; pathUpdateRender ->
# Tile::hasPath 13320 (c3aq) and the same; setZoomLevel -> resize 9ed0; initTerrain / resetTerrain -> buildArrays a130, Tile::reset c2c0 (c3ba).
_SC_PIVOTS = [25 + 25 * 50, 3 + 3 * 50, 46 + 46 * 50, 0, 25 + 2 * 50, 2010]
for _z, _suffix in ((0, ""), (1, "_z1"), (2, "_z2")):
    HOOKS["Terrain::render" + _suffix] = dict(
        module="Terrain.dll", addr=0x00005990, abi="thiscall", ret="uint8", args=["pointer", "pointer", "float"],
        fixture="c3dp_scene_z%d" % _z, state=_SC_STATE,
        vectors=[("$obj", 0, a) for a in (0.0, 45.0)]
                + [("$obj", "$tile%d" % p, a) for p in _SC_PIVOTS for a in (0.0, 90.0, 180.0, 45.0)])
    HOOKS["Terrain::localRender" + _suffix] = dict(
        module="Terrain.dll", addr=0x000089E0, abi="thiscall", ret="void", args=["pointer", "pointer", "pointer", "float"],
        fixture="c3dp_scene_z%d" % _z, state=_SC_STATE,
        vectors=[("$obj", "$tile%d" % p, "$tile%d" % q, a) for p in (1275, 51, 2448, 0, 125, 1224) for q in (1275, 51)
                 for a in (0.0, 90.0, 180.0, 45.0)])
    HOOKS["Terrain::pathUpdateRender" + _suffix] = dict(
        module="Terrain.dll", addr=0x00006410, abi="thiscall", ret="void", args=["pointer", "pointer", "float"],
        fixture="c3dp_scene_z%d" % _z, state=_SC_STATE,
        vectors=[("$obj", "$tile%d" % p, a) for p in _SC_PIVOTS for a in (0.0, 90.0, 180.0, 45.0)])

HOOKS["Terrain::drawLine"] = dict(
    module="Terrain.dll", addr=0x000048A0, abi="thiscall", ret="void",
    args=["pointer", "int", "int", "int", "int", "int", "int", "int"], fixture="c3dp_gl_line", state=_GLLOG,
    vectors=[("$f%d" % f, x1, y1, x2, y2, col, w, al)
             for f in range(8)
             for (x1, y1, x2, y2, col, w, al) in ((0, 0, 100, 50, 0x7FFF, 1, 10), (10, 20, -30, 400, 0x1234, 2, 5),
                                                  (800, 600, 0, 0, 0x0, 0, 0), (-1, -1, 1, 1, 0xFFFF, 100000, 255),
                                                  (5, 5, 5, 5, -0x8000, -1, -3), (7, 9, 11, 13, 0x5555, 3, 16777217))])
HOOKS["Terrain::setZoomLevel"] = dict(
    module="Terrain.dll", addr=0x00008FC0, abi="thiscall", ret="void", args=["pointer", "int"], fixture="c3dp_gl_zoom",
    state=[("$blk", 0, 0x800), ("$implog", 0, 0x1000)],
    vectors=[("$z%d" % z, lv) for z in range(12) for lv in (1, 2, 4, 0, 3, -1, 8)])
_GRIDGL = _GRID + [("$implog", 0, 0x400)]
HOOKS["Terrain::initTerrain"] = dict(
    module="Terrain.dll", addr=0x0000A970, abi="thiscall", ret="void", args=["pointer"], fixture="c3dp_grid_gl", state=_GRIDGL,
    vectors=[("$G%d" % i,) for i in range(11)])
HOOKS["Terrain::resetTerrain"] = dict(
    module="Terrain.dll", addr=0x0000AA10, abi="thiscall", ret="void", args=["pointer"], fixture="c3dp_grid_gl", state=_GRIDGL,
    vectors=[("$G%d" % i,) for i in range(11)])


# =============================================================================================================
# The texture file loaders. Fixtures c3dp_dib / c3dp_tga / c3dp_tex write REAL files into the temp directory (deleted by the freeze helper's
# flush after the key), run under the heap stand-in over Terrain.dll's malloc 0x100158e0, free 0x10016470, operator new 0x10018c90 and operator
# delete 0x10018be0 (arena 0x10000; an impossible request returns NULL), and stand in for the OPENGL32 texture calls and GLU32's
# gluBuild1DMipmaps / gluBuild2DMipmaps. The files are read by the module's own CRT in both arms.
#
# Hooked callees (callee-off runs): textureLoad -> LoadDIBitmap 16a0 (this batch) and swapRedBlue 1b60 (not hooked).
_FILE = [("$arena", 0, 0x10010), ("$freelog", 0, 0x404), ("$implog", 0, 0x1000)]
_FILE_DIB = [("$arena", 0, 0x10010), ("$freelog", 0, 0x404), ("$slots", 0, 0x200)]
HOOKS.update({
    # LoadDIBitmap(filename, BITMAPINFO** out): 16 files (missing, empty, 13 bytes, bad magic, header without info, valid 24 bpp with an explicit
    # size, valid 24 bpp with the computed size, 8 bpp with a palette, negative height, truncated pixels, bfOffBits 4 and 14, 1 bpp, 32 bpp,
    # zero width, 4 x 1 24 bpp), then the same files under a malloc that fails its first call (info) and its second (bits).
    "LoadDIBitmap": dict(module="Terrain.dll", addr=0x000016A0, abi="default", ret="pointer", args=["pointer", "pointer"],
                         fixture="c3dp_dib", state=_FILE_DIB, vectors=[("$p%d" % i, "$o%d" % i) for i in range(16)]),
    "LoadDIBitmap_f1": dict(module="Terrain.dll", addr=0x000016A0, abi="default", ret="pointer", args=["pointer", "pointer"],
                            fixture="c3dp_dib_f1", state=_FILE_DIB, vectors=[("$p%d" % i, "$o%d" % i) for i in (5, 6, 7, 8, 12, 13, 15, 3)]),
    "LoadDIBitmap_f2": dict(module="Terrain.dll", addr=0x000016A0, abi="default", ret="pointer", args=["pointer", "pointer"],
                            fixture="c3dp_dib_f2", state=_FILE_DIB, vectors=[("$p%d" % i, "$o%d" % i) for i in (5, 6, 7, 8, 12, 13, 15, 3)]),
    # LoadTGA(filename): 17 files (see the fixture), then three valid ones under an operator new that fails.
    "LoadTGA": dict(module="Terrain.dll", addr=0x0000BE30, abi="default", ret="uint32", args=["pointer"], fixture="c3dp_tga",
                    state=_FILE, vectors=[("$p%d" % i,) for i in range(17)]),
    "LoadTGA_f1": dict(module="Terrain.dll", addr=0x0000BE30, abi="default", ret="uint32", args=["pointer"], fixture="c3dp_tga_f1",
                       state=_FILE, vectors=[("$p%d" % i,) for i in (10, 11, 12, 14)]),
    # textureLoad(filename, alpha, minfilter, magfilter, wrap): a 2D bitmap, a 1D one (height 1), a 5 x 3 one, a missing and a bad file, under the
    # filters that take glTexImage2D (0x2601, 0x2600), the mipmap builders (0x2701, 0x2703, 0) and two wrap modes.
    "textureLoad": dict(
        module="Terrain.dll", addr=0x0000BBD0, abi="default", ret="uint32", args=["pointer", "uint8", "uint32", "uint32", "uint32"],
        fixture="c3dp_tex", state=_FILE,
        vectors=[("$p%d" % p, al, mn, 0x2601, wr) for p in (1, 2, 3) for (al, mn, wr) in
                 ((0, 0x2601, 0x2901), (1, 0x2600, 0x812F), (0, 0x2701, 0x2901), (0, 0x2703, 0x812F), (1, 0, 0x2901))]
                + [("$p0", 0, 0x2601, 0x2601, 0x2901), ("$p4", 0, 0x2701, 0x2601, 0x2901)]),
})


# =============================================================================================================
# passCollarInfo, closeSystem, ~Terrain. Hooked callees: closeSystem and ~Terrain -> reloadTextures 380a0 (this batch); ~Terrain -> ~list aed0
# (this batch) and Tile::~Tile c3d0 (this batch), Tile::setNeighbour c520 (c3aq).
_COLLAR_STATE = [("$arena", 0, 0x4010), ("$freelog", 0, 0x404), (0x10106B48, 0, 8)]
_N_COLLAR = (0, 1, 2, 3, 7, 16, 33, 100, 256, -1)
HOOKS["Terrain::passCollarInfo"] = dict(
    module="Terrain.dll", addr=0x0000A880, abi="thiscall", ret="void", args=["pointer", "pointer", "int"],
    fixture="c3dp_collar_null", state=_COLLAR_STATE, vectors=[(0, "$info", n) for n in _N_COLLAR])
HOOKS["Terrain::passCollarInfo_set"] = dict(
    module="Terrain.dll", addr=0x0000A880, abi="thiscall", ret="void", args=["pointer", "pointer", "int"],
    fixture="c3dp_collar_set", state=_COLLAR_STATE, vectors=[(0, "$info", n) for n in _N_COLLAR])
_CLOSE_STATE = [("$blk", 0, 4 * 0x400), ("$implog", 0, 0x10000), ("$tbl", 0, 0x25 * 0x384)]
for _name, _fx in (("", "ok"), ("_mc0", "mc0"), ("_del0", "del0"), ("_both0", "both0")):
    HOOKS["Terrain::closeSystem" + _name] = dict(
        module="Terrain.dll", addr=0x00009BA0, abi="thiscall", ret="void", args=["pointer"], fixture="c3dp_close_" + _fx,
        state=_CLOSE_STATE, vectors=[("$s%d" % i,) for i in range(4)])
_DTOR_STATE = [("$d0", 0, 0x165400), ("$d1", 0, 0x165400), ("$arena", 0, 0x4010), ("$freelog", 0, 0x404), ("$implog", 0, 0x10000),
               ("$tbl", 0, 0x25 * 0x384), (0x10106B48, 0, 8)]
HOOKS["Terrain::~Terrain"] = dict(
    module="Terrain.dll", addr=0x00003090, abi="thiscall", ret="void", args=["pointer"], fixture="c3dp_dtor_null",
    state=_DTOR_STATE, vectors=[("$d0",), ("$d1",)])
HOOKS["Terrain::~Terrain_set"] = dict(
    module="Terrain.dll", addr=0x00003090, abi="thiscall", ret="void", args=["pointer"], fixture="c3dp_dtor_set",
    state=_DTOR_STATE, vectors=[("$d0",), ("$d1",)])


# =============================================================================================================
# The lighting file: loadLighting, rebuild, initLists (istream::getline is reached as a callee). Fixtures c3dp_light_t<N>: temp lighting files
# (and the game's own four, by the relative names rebuild builds), the OPENGL32 light and material stand-ins (no heap stand-in: the stdio and
# filebuf blocks come from the CRT's own debug heap and are freed by the CRT in both arms), and g_courseType 0x10070a0c = N (t4: out of range, tneg: -1).
#
# Hooked callees (callee-off runs): loadLighting -> ios::fail 2510 (this batch), istream::getline 10590 (this batch), rotateAxis 3a50 (c3as),
# normalize 37c80 (c3as); rebuild -> loadLighting; initLists -> rebuild.
_LIGHT = [("$implog", 0, 0x1000), (0x10070A0C, 0, 4)]
HOOKS["Terrain::loadLighting"] = dict(
    module="Terrain.dll", addr=0x00006DD0, abi="thiscall", ret="void", args=["pointer", "pointer"], fixture="c3dp_light_t0",
    state=_LIGHT, vectors=[(0, "$p%d" % i) for i in range(7)] + [(0, "$pmissing"), (0, "$pPark"), (0, "$pDesert"), (0, "$pTrop"), (0, "$pLinks")])
for _n, _t in (("", "t0"), ("_t1", "t1"), ("_t2", "t2"), ("_t3", "t3"), ("_t4", "t4"), ("_tneg", "tneg")):
    HOOKS["Terrain::rebuild" + _n] = dict(
        module="Terrain.dll", addr=0x00003980, abi="thiscall", ret="void", args=["pointer"], fixture="c3dp_light_" + _t, state=_LIGHT,
        vectors=[(0,)])
for _n, _t in (("", "t0"), ("_t3", "t3"), ("_t4", "t4")):
    HOOKS["Terrain::initLists" + _n] = dict(
        module="Terrain.dll", addr=0x000037E0, abi="thiscall", ret="uint8", args=["pointer"], fixture="c3dp_light_" + _t, state=_LIGHT,
        vectors=[(0,)])

# istream::getline: four primed ifstreams (c3dp_getline) x lengths x delimiters. Hooked callees (callee-off runs): ios::lock 18b0, ios::unlock 37930.
_GL_STATE = [("$ifs%d" % i, 0, 0x200) for i in range(4)] + [("$fb%d" % i, 0, 0x54) for i in range(4)] + [("$dst", 0, 0x200), ("$implog", 0, 0x400)]
HOOKS["istream::getline"] = dict(
    module="Terrain.dll", addr=0x00010590, abi="thiscall", ret="pointer", args=["pointer", "pointer", "int", "int"],
    fixture="c3dp_getline", state=_GL_STATE,
    vectors=[("$ifs%d" % i, "$dst", n, d) for i in range(4) for n in (1, 2, 5, 11, 100) for d in (10, 0x78, 0, 255)])

# ifstream::{vbase dtor}: four failed ifstreams (c3dp_ifsdtor; no open descriptor). Hooked callees: none of this batch's.
HOOKS["ifstream::{vbase dtor}"] = dict(
    module="Terrain.dll", addr=0x000046A0, abi="thiscall", ret="void", args=["pointer"], fixture="c3dp_ifsdtor",
    state=[("$ifs%d" % i, 0, 0x200) for i in range(4)] + [("$fb%d" % i, 0, 0x54) for i in range(4)]
          + [("$freelog", 0, 0x404), ("$implog", 0, 0x400), (0x10110B10, 0, 0x40)],
    vectors=[("$ifs%d" % i,) for i in range(4)])

# Terrain::relight: two Terrain stand-ins (loaded flag clear / set) x the four course directories and the default. State: both objects, g_textures, the
# arena header (cursor and call counter), the GL log.
# Hooked callees (callee-off runs): reloadTextures 380a0, textureLoad bbd0, LoadTGA be30, LoadDIBitmap 16a0 (this batch); setTextureId 12e70 (c3ba).
_RELIGHT = [("$r0", 0, 0x3a8), ("$r1", 0, 0x3a8), ("$tbl", 0, 0x25 * 0x384), ("$arena", 0, 0x10), ("$implog", 0, 0x40000)]
for _n, _c in (("", "c0"), ("_c1", "c1"), ("_c2", "c2"), ("_c3", "c3"), ("_cdef", "cdef")):
    HOOKS["Terrain::relight" + _n] = dict(
        module="Terrain.dll", addr=0x000076E0, abi="thiscall", ret="void", args=["pointer"], fixture="c3dp_relight_" + _c,
        state=_RELIGHT, vectors=[("$r0",), ("$r1",)])

# Terrain::loadNewCourseType (the relight fixture at course type 0: types 1, 2, 3, 5 and -1 differ from it and reload everything, type 0 changes nothing)
# and Terrain::initSystem (c3dp_initsys_*: results of ChoosePixelFormat, SetPixelFormat, wglCreateContext and wglMakeCurrent set per key).
# Hooked callees (callee-off runs): rebuild, reloadTextures, relight (this batch); initSystem additionally initGL, initLists, resize (this batch).
HOOKS["Terrain::loadNewCourseType"] = dict(
    module="Terrain.dll", addr=0x00001AF0, abi="thiscall", ret="void", args=["pointer", "int"], fixture="c3dp_relight_c0",
    state=_RELIGHT + [(0x10070A0C, 0, 4)], vectors=[("$r%d" % d, t) for d in (0, 1) for t in (1, 2, 3, 5, -1, 0)])
_INITSYS = [("$s0", 0, 0x3a8), ("$s1", 0, 0x3a8), ("$tbl", 0, 0x25 * 0x384), ("$arena", 0, 0x10), ("$implog", 0, 0x40000),
            (0x10070A0C, 0, 4), (0x10070A10, 0, 4)]
_INITSYS_VEC = [("$s0", 800, 600, 0x1234, 1), ("$s0", 1024, 768, 0x1234, 0), ("$s0", 1280, 1024, 0, 1), ("$s0", 640, 480, 0x1234, 0),
                ("$s0", 800, 600, 0, 0), ("$s1", 800, 600, 0x1234, 1)]
for _n, _f in (("", "ok"), ("_cp0", "cp0"), ("_sp0", "sp0"), ("_cc0", "cc0"), ("_mc0", "mc0")):
    HOOKS["Terrain::initSystem" + _n] = dict(
        module="Terrain.dll", addr=0x00009C80, abi="thiscall", ret="void", args=["pointer", "int", "int", "pointer", "int"],
        fixture="c3dp_initsys_" + _f, state=_INITSYS, vectors=_INITSYS_VEC)

# Terrain::Terrain(width, height): a pattern-filled full-size Terrain object (c3dp_ctor) and sizes 50 x 50, 3 x 2, 1 x 1, 0 x 0, 7 x 50, 50 x 1. The vector
# constructor, Tile::reset, the type-name table, g_textures and relight (about 520 texture files) all run. Returns `this`.
# Hooked callees (callee-off runs): Tile::Tile c210, Tile::~Tile c3d0 (cleanup only), Tile::reset c2c0 (c3ba), std::list<Tile*>::list ae60 (c3cn), relight
# and everything below it (textureLoad, LoadTGA, LoadDIBitmap, reloadTextures: this batch; setTextureId: c3ba).
HOOKS["Terrain::Terrain"] = dict(
    module="Terrain.dll", addr=0x00002AE0, abi="thiscall", ret="pointer", args=["pointer", "int", "int"], fixture="c3dp_ctor",
    state=[("$t", 0, 0x165400), ("$tbl", 0, 0x25 * 0x384), ("$arena", 0, 0x100), ("$implog", 0, 0x40000), (0x10070A0C, 0, 4), (0x10106B48, 0, 4)],
    vectors=[("$t", w, h) for (w, h) in ((50, 50), (3, 2), (1, 1), (0, 0), (7, 50), (50, 1))])

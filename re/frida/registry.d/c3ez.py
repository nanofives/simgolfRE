"""Batch c3ez (round 33): golf_clean.exe rows whose blocker a later tool removed (the Terrain grid, the net family,
unlinkSound, Window::setSize).

Reimplementations shim/src/re/c3ez.cpp, fixtures re/frida/js/fixtures.d/c3ez.js, purposes log/c3/c3ez_purpose.md."""

HOOKS = {}

# ============================================================================ the Terrain grid
# Every key: the Terrain import log, the callee-stand log, the globals 0x00820f14..0x00820f3b (list iterator, list
# 0x00820f18, the bytes 0x00820f28..0x00820f2c, screen w / h, last rotation), 0x004d2044..0x004d2053 and the swapped
# Terrain pointer [0x00820ed0].
_TILE = 0x248


def _ter(log, *extra):
    return [("$implog", 0, log), ("$clog", 0, 0x1000), (0x00820F14, 0, 0x28), (0x004D2044, 0, 0x10),
            (0x00820ED0, 0, 4)] + list(extra)


# applyTileVariation(tile, x, y): 13 vectors, tile i of $tiles with the grid cell (x, y) written by "$t{i}"
HOOKS["applyTileVariation"] = dict(
    addr=0x00449F00, abi="default", ret="int", args=["pointer", "int", "int"], fixture="c3ez_atv",
    state=_ter(0x1000, ("$tiles", 0, 13 * _TILE)),
    vectors=[(f"$t{i}", f"$x{i}", f"$y{i}") for i in range(13)])

# togglePath(tile, x, y): 13 vectors
HOOKS["togglePath"] = dict(
    addr=0x0044A410, abi="default", ret="int", args=["pointer", "int", "int"], fixture="c3ez_tgp",
    state=_ter(0x40000, ("$tiles", 0, 13 * _TILE)),
    vectors=[(f"$t{i}", f"$x{i}", f"$y{i}") for i in range(13)])

# applyTileElevation(tile, x, y, x2, y2): 16 vectors; the corner bytes of (7, 9) move for the NULL-tile vectors
HOOKS["applyTileElevation"] = dict(
    addr=0x00449FA0, abi="default", ret="int", args=["pointer", "int", "int", "int", "int"],
    fixture="c3ez_ate", state=_ter(0x2000, ("$tiles", 0, 16 * _TILE), ("$ncorner", 0, 8)),
    vectors=[(f"$t{i}", f"$x{i}", f"$y{i}", f"$u{i}", f"$v{i}") for i in range(16)])

# rebuildTileData(x0, y0): g_zoomScale 1 / 2 / 4 per key; the tiles of rows y 45..49 (every window lies there), the
# list sentinel, the node arena; _ylow: y0 - d < 0 over columns x 48 and 49 (one region per tile pair).
_RTD = [("$tobj", 0x3A4 + 45 * 50 * _TILE, 250 * _TILE), ("$lst", 0, 0x20), ("$arena", 0, 0x10010),
        ("$freelog", 0, 0x404)]
for _z in ("z1", "z2", "z4"):
    HOOKS[f"rebuildTileData_{_z}"] = dict(
        addr=0x0044A5B0, abi="default", ret="void", args=["int", "int"], fixture=f"c3ez_rtd_{_z}",
        state=_ter(0x40000, *_RTD), vectors=[(f"$r{i}", f"$q{i}") for i in range(7)])
HOOKS["rebuildTileData_nullprev"] = dict(
    addr=0x0044A5B0, abi="default", ret="void", args=["int", "int"], fixture="c3ez_rtd_nullprev",
    state=_ter(0x40000, *_RTD), vectors=[("$r0", "$q0")])
HOOKS["rebuildTileData_ylow"] = dict(
    addr=0x0044A5B0, abi="default", ret="void", args=["int", "int"], fixture="c3ez_rtd_ylow",
    state=_ter(0x40000, *([("$tobj", 0x3A4 + (48 + 50 * y) * _TILE, 2 * _TILE) for y in range(50)] + _RTD[1:])),
    vectors=[(f"$r{i}", f"$q{i}") for i in range(2)])

# initTerrain(w, h): dwMajorVersion 5 / 4 / 6 per key; "$w{i}" writes the type records and the mode byte
for _v in ("v5", "v4", "v6"):
    HOOKS[f"initTerrain_{_v}"] = dict(
        addr=0x00449790, abi="default", ret="int", args=["int", "int"], fixture=f"c3ez_init_{_v}",
        state=_ter(0x1000, ("$surf", 0, 0x40)),
        vectors=[(f"$w{i}", h) for i, h in enumerate((600, 768, 1024, 480, 0, 7))])

# renderTerrainStrip(surf, x, y, dx, dy, rot, zoom): the screen size per key. Every (dx, dy) in -2..2 with rotations
# and zooms spread over them, plus steps outside that range, zooms other than 1 / 2 / 4 and the default case
# (D + r2 <= 0).
_STRIP = []
_k = 0
for _dx in range(-2, 3):
    for _dy in range(-2, 3):
        _STRIP.append((0x1234, 10 + _dx, 20 + _dy, _dx, _dy, (0, 1, 2, 3, 4, 5, 6, 7)[_k % 8], (1, 2, 4)[_k % 3]))
        _k += 1
_STRIP += [(0, 0, 0, 3, 0, 2, 1), (0, 49, 49, 0, 3, 6, 2), (0, 50, -1, -3, -3, 4, 4), (0, 5, 5, 1, 1, 0, 3),
           (0, 5, 5, 0, 0, 0, 0), (0, 5, 5, 9, 9, 0, -3), (0, 5, 5, -1, 0, 2, 4), (0, 5, 5, 0, 1, 6, 1),
           (0, 5, 5, 1, -1, 4, 2), (0, 5, 5, -2, 0, 6, 4),
           # case 1 with rot 4 (D 5 + 4 folds to 1), case 3 with rot 0 and rot 4 (D 7 + 4 folds to 3), case 5 with
           # rot 0, case 7 with rot 0 and rot 4 (D 3 + 4); the tile out of the grid by x < 0, y >= 50, y < 0
           (0, 10, 20, 0, 1, 4, 1), (0, 10, 20, 1, 0, 0, 2), (0, 10, 20, -1, 0, 4, 4), (0, 10, 20, 0, 1, 0, 1),
           (0, 10, 20, -1, 0, 0, 2), (0, 10, 20, 1, 0, 4, 4), (0, -1, 5, 1, 1, 2, 2), (0, 5, 50, -1, 1, 6, 1),
           (0, 5, -1, 0, -1, 0, 4)]
for _r in ("800", "1024", "1280"):
    HOOKS[f"renderTerrainStrip_{_r}"] = dict(
        addr=0x0044A6E0, abi="default", ret="int", args=["int"] * 7, fixture=f"c3ez_strip_{_r}",
        state=_ter(0x1000), vectors=_STRIP)
# sizes outside the three (800 x 500, 1024 x 700, 1280 x 1000, 640 x 480): only vectors whose strip is the default
# (D + r2 <= 0), the one case that reads none of the three width locals those sizes leave uninitialised
_BAD = [(0, 5, 5, 0, 0, 0, 0), (0, 5, 5, 3, 3, 0, -2), (0, 5, 5, 9, 0, -3, 1), (0, 49, 2, 7, 7, -1, 0)]
for _r in ("bad800", "bad1024", "bad1280", "bad"):
    HOOKS[f"renderTerrainStrip_{_r}"] = dict(
        addr=0x0044A6E0, abi="default", ret="int", args=["int"] * 7, fixture=f"c3ez_strip_{_r}",
        state=_ter(0x1000), vectors=_BAD)

# updateTerrainRender(x, y, zoom, rot): 41 vectors, one global state each ("$ux{i}" writes it)
HOOKS["updateTerrainRender"] = dict(
    addr=0x004498A0, abi="default", ret="int", args=["int"] * 4, fixture="c3ez_utr",
    state=_ter(0x1000, ("$nodes", 0, 0x100), ("$lst", 0, 0x20), ("$freelog", 0, 0x404), (0x005619A0, 0, 1)),
    vectors=[(f"$ux{i}", f"$uy{i}", f"$uz{i}", f"$ur{i}") for i in range(41)])

# ============================================================================ the net family
# Every key: the whole session-object block ($objs, 0x800 per object), the call log ($clog), the interface and guards
# (0x008400b0..0x008400c3). Heap users add the arena and free log; the import users the import log.
_NOBJ = 0x800


def _net(n, *extra):
    return [("$objs", 0, _NOBJ * n), ("$clog", 0, 0x1000), (0x008400B0, 0, 0x14)] + list(extra)


# netPump(this): 12 objects, `$o{i}` each (thiscall, no stack argument)
HOOKS["netPump"] = dict(
    addr=0x0049AA70, abi="thiscall", ret="uint32", args=["pointer"], fixture="c3ez_netpump",
    state=_net(12), vectors=[(f"$o{i}",) for i in range(12)])

# netSendList(this): 10 objects; the arena holds the one malloc'd block. eax is unused (its one caller netQueueMessage
# overwrites it at 0x0049b74d), so ret=void: the compared evidence is the whole session block and the arena.
HOOKS["netSendList"] = dict(
    addr=0x00497B40, abi="thiscall", ret="void", args=["pointer"], fixture="c3ez_netsendlist",
    state=_net(10, ("$arena", 0, 0x4010), ("$freelog", 0, 0x404)), vectors=[(f"$o{i}",) for i in range(10)])

# netQueueMessage(this, name, id, flags): 10 objects, with a name string, an id and flags each (ret 0xc)
HOOKS["netQueueMessage"] = dict(
    addr=0x0049B690, abi="thiscall", ret="uint32", args=["pointer", "pointer", "uint32", "uint32"],
    fixture="c3ez_netqueue", state=_net(10, ("$arena", 0, 0x4010), ("$freelog", 0, 0x404)),
    vectors=[(f"$o{i}", f"$n{i}", f"$i{i}", f"$f{i}") for i in range(10)])

# netServiceControl(this): 11 objects, one per drop-logic branch; the import log holds the timeGetTime calls
HOOKS["netServiceControl"] = dict(
    addr=0x0049AB40, abi="thiscall", ret="uint32", args=["pointer"], fixture="c3ez_netsvcctl",
    state=_net(11, ("$implog", 0, 0x400), (0x008400BC, 0, 8)), vectors=[(f"$g{i}",) for i in range(11)])

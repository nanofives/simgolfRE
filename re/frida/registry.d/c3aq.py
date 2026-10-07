# Batch c3aq: Terrain.dll Tile readers and writers, the Faces/PathInfo/WallInfo constructors, the spline factorial
# and the 24-bit red/blue swap of the image loaders. addr is the RVA (VA = 0x10000000 + RVA).
#
# Fixtures (re/frida/js/fixtures.d/c3aq.js) build our own buffers only: 16 Tile buffers (c3aq_tiles), 16 standalone
# PathInfo and WallInfo blocks (c3aq_blocks) and 6 pixel buffers with 12 ImageInfo records (c3aq_images). The live
# Terrain object, its tile array and the Terrain.dll globals are never used.

_T = ["$t%d" % i for i in range(16)]            # the 16 tiles
_TNZ = _T[1:]                                   # tiles 1..15: safe for negative getCorner indices
_STATE_TILES = [("$base", 0, 16 * 0x248)]       # the whole tile block, for every Tile writer
_STATE_PATH = [("$pbase", 0, 16 * 0x10)]
_STATE_WALL = [("$wbase", 0, 16 * 0x30)]
_STATE_IMG = [("$imgbase", 0, 6 * 0x100)]

HOOKS = {
    # --- readers (leaves: >= 10 vectors each) -------------------------------------------------------------------
    # dword index, no bounds check: 0..8 are the nine seeded slots, -2/-1/9/10 read the neighbouring buffer bytes
    "Tile::getCorner": dict(module="Terrain.dll", addr=0x00001E40, abi="thiscall", ret="int",
                            args=["pointer", "int"], fixture="c3aq_tiles",
                            vectors=[(t, k) for t in _T for k in range(9)]
                                    + [(t, k) for t in _TNZ[:6] for k in (-2, -1, 9, 10)]),
    # three signed maxima; the 16 tiles take both sides of each of the three jle
    "Tile::maxHeight": dict(module="Terrain.dll", addr=0x00015500, abi="thiscall", ret="int", args=["pointer"],
                            fixture="c3aq_tiles", vectors=[(t,) for t in _T]),
    # byte at +0x208: the 16 tiles hold 0, 1, 2, 3, 0x7f, 0x80 and 0xff
    "Tile::hasPath": dict(module="Terrain.dll", addr=0x00013320, abi="thiscall", ret="uint8", args=["pointer"],
                          fixture="c3aq_tiles", vectors=[(t,) for t in _T]),
    # byte at +0x209
    "Tile::isConnected": dict(module="Terrain.dll", addr=0x00013360, abi="thiscall", ret="uint8", args=["pointer"],
                              fixture="c3aq_tiles", vectors=[(t,) for t in _T]),
    # low byte of the dword at +0x28, read back signed (movsx in the caller)
    "Tile::getVariation": dict(module="Terrain.dll", addr=0x00015340, abi="thiscall", ret="int8", args=["pointer"],
                               fixture="c3aq_tiles", vectors=[(t,) for t in _T]),
    # this + 0x44: 16 distinct pointers, reported relative to the fixture's obj
    "Tile::getFaces": dict(module="Terrain.dll", addr=0x000153C0, abi="thiscall", ret="pointer", args=["pointer"],
                           fixture="c3aq_tiles", vectors=[(t,) for t in _T]),

    # --- writers (state = the whole tile block) -----------------------------------------------------------------
    # je 0x100133c1 tests the full dword: 0x100 and -0x80000000 are "on", only 0 takes the false side
    "Tile::setConnected": dict(module="Terrain.dll", addr=0x000133A0, abi="thiscall", ret="void",
                               args=["pointer", "int"], fixture="c3aq_tiles",
                               state=_STATE_TILES,
                               vectors=[(_T[i % 16], v) for i, v in enumerate(
                                   [0, 1, -1, 2, 0x100, 0xFF, 0x7FFFFFFF, -0x80000000, 0, 1, 0x10000, -2,
                                    0, 1, 0x80, 0])]),
    # whole dword into +0x28 (the field getVariation reads the low byte of)
    "Tile::setRotation": dict(module="Terrain.dll", addr=0x00015380, abi="thiscall", ret="void",
                              args=["pointer", "int"], fixture="c3aq_tiles", state=_STATE_TILES,
                              vectors=[(_T[i % 16], v) for i, v in enumerate(
                                  [0, 1, 2, 3, -1, 0x7F, 0x80, 0xFF, 0x100, 0x1234, -0x1234, 0x7FFFFFFF,
                                   -0x80000000, 0x12345678, 0xABCD, 0])]),
    # whole dword into +0x240, a different field from setRotation's
    "Tile::setVariation": dict(module="Terrain.dll", addr=0x00002F80, abi="thiscall", ret="void",
                               args=["pointer", "int"], fixture="c3aq_tiles", state=_STATE_TILES,
                               vectors=[(_T[i % 16], v) for i, v in enumerate(
                                   [0, 1, 2, 5, -1, -7, 0x40, 0xFF, 0x101, 0x3039, -0x3039, 0x7FFFFFFF,
                                    -0x80000000, 0x55555555, 0xAAAA, 9])]),
    # layPath: tiles 0..6 carry the seven types that take a je (0x16, 0, 2, 1, 3, 7, 9), tiles 7..15 other types;
    # `on` is masked to its low byte (0x100 is off), `dir` is tested as a full dword (0x100 is on)
    "Tile::layPath": dict(module="Terrain.dll", addr=0x00013400, abi="thiscall", ret="void",
                          args=["pointer", "int", "int"], fixture="c3aq_tiles", state=_STATE_TILES,
                          vectors=[(t, 1, 1) for t in _T]
                                  + [("$t0", 1, 0), ("$t7", 1, 0), ("$t3", 1, 0), ("$t9", 1, 0),
                                     ("$t0", 0, 1), ("$t7", 0, 0), ("$t5", 0, 1), ("$t12", 0, -1),
                                     ("$t3", 0x100, 1), ("$t9", 0x101, 0), ("$t2", -1, -1),
                                     ("$t11", 0xFF, 0x100), ("$t6", 0x10000, 1), ("$t15", -0x80000000, 0)]),

    # --- constructors -------------------------------------------------------------------------------------------
    # stores the dword 0 at this+0 (the Faces count, seeded 0x5a000+i) and returns this
    "Faces::Faces": dict(module="Terrain.dll", addr=0x00001FA0, abi="thiscall", ret="pointer", args=["pointer"],
                         fixture="c3aq_tiles", state=_STATE_TILES, vectors=[("$f%d" % i,) for i in range(16)]),
    # calls PathInfo::clear 0x1000f6e0 (hooked by batch c3am) and returns this; all 16 blocks are seeded non-zero
    "PathInfo::PathInfo": dict(module="Terrain.dll", addr=0x0000F690, abi="thiscall", ret="pointer",
                               args=["pointer"], fixture="c3aq_blocks", state=_STATE_PATH,
                               vectors=[("$p%d" % i,) for i in range(16)]),
    # calls WallInfo::clear 0x1000f7a0 (hooked by batch c3am) and returns this
    "WallInfo::WallInfo": dict(module="Terrain.dll", addr=0x0000F750, abi="thiscall", ret="pointer",
                               args=["pointer"], fixture="c3aq_blocks", state=_STATE_WALL,
                               vectors=[("$w%d" % i,) for i in range(16)]),

    # --- __cdecl free functions ---------------------------------------------------------------------------------
    # jne 0x100058bc separates n == 0 (returns 1); jle 0x100058e0 ends the loop, so every n <= 2 returns n.
    # Large n is left out on purpose: the loop runs n-2 times, so 0x7fffffff would not terminate in a test.
    "factorial": dict(module="Terrain.dll", addr=0x000058A0, abi="default", ret="int", args=["int"],
                      vectors=[(v,) for v in (-0x80000000, -1000, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
                                              11, 12, 13, 14, 15, 17, 20, 33, 50)]),
    # swapRedBlue: 6 pixel buffers x ImageInfo records whose width*height is 1, 2, 3, 4, 6, 9 and 16 pixels, plus
    # the zero and negative products that take the jle at 0x10001bad on the first turn and write nothing
    "swapRedBlue": dict(module="Terrain.dll", addr=0x00001B60, abi="default", ret="void",
                        args=["pointer", "pointer"], fixture="c3aq_images", state=_STATE_IMG,
                        vectors=[("$b%d" % (i % 6), "$i%d" % i) for i in range(12)]
                                + [("$b%d" % ((i + 3) % 6), "$i%d" % i) for i in range(12)]),

    # --- allocation-free std::list<Tile*> node construction ------------------------------------------------------
    # placement operator new: returns its second argument, ignores the size. Key is not the hooks.csv name
    # ("operator new") because c3_verify --keys is word-split by the shell and the name contains a space.
    "operator_new_placement": dict(module="Terrain.dll", addr=0x0000BAB0, abi="default", ret="pointer",
                                   args=["uint32", "pointer"], fixture="c3aq_nodes",
                                   vectors=[(4, "$d%d" % i) for i in range(16)]
                                           + [(0, "$d0"), (1, "$s3"), (0x248, "$s7"), (0xFFFFFFFF, "$d9"),
                                              (4, 0), (0, 0)]),
    # std::_Construct: je 0x1000ba6d splits the NULL destination (no copy) from the dword copy. ret is void: the
    # value of the new-expression is stored in the dead frame slot [ebp-8] and never moved into eax, so the
    # original leaves the last computed value there and only the written memory is comparable.
    "std::_Construct": dict(module="Terrain.dll", addr=0x0000BA40, abi="default", ret="void",
                            args=["pointer", "pointer"], fixture="c3aq_nodes", state=[("$dest", 0, 16 * 4)],
                            vectors=[("$d%d" % i, "$s%d" % ((i * 5 + 3) % 16)) for i in range(16)]
                                    + [("$d0", "$s0"), ("$d1", "$d1"), ("$d2", "$d15"),
                                       (0, "$s0"), (0, "$s4"), (0, 0)]),
}

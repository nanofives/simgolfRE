"""Batch c3ey (round 32): jgld.dll / Terrain.dll leftovers and the exe's Terrain grid sweepers.

Reimplementations shim/src/re/c3ey.cpp, fixtures re/frida/js/fixtures.d/c3ey.js, purposes log/c3/c3ey_purpose.md."""

HOOKS = {}

_J = dict(module="jgld.dll")
_T = dict(module="Terrain.dll")

# ============================================================================ golf_clean.exe: Terrain grid sweepers
# Argument-free cdecl functions: each vector passes one dummy int ("$rN"), the getter that writes row N of the exe grids
# (and syncTiles' fixture tiles) before the state snapshot; the extra cdecl argument is popped by the caller.
# State: the Terrain import log, the pump log, the byte syncTiles sets and the swapped Terrain pointer.
_TER = lambda log: [("$implog", 0, log), ("$clog", 0, 0x800), (0x00820F2A, 0, 1), (0x00820ED0, 0, 4)]
for _k, _n in (("50", 6), ("53", 6)):
    HOOKS[f"syncTiles_{_k}"] = dict(addr=0x00449470, abi="default", ret="int", args=["int"],
                                    fixture=f"c3ey_sync_{_k}", state=_TER(0x20000),
                                    vectors=[(f"$r{i}",) for i in range(_n)])
for _k, _n in (("50", 4), ("53", 4), ("37", 4), ("30", 4), ("noflags", 2)):
    HOOKS[f"rebuildTerrainGrid_{_k}"] = dict(addr=0x00449540, abi="default", ret="int", args=["int"],
                                             fixture=f"c3ey_rebuild_{_k}", state=_TER(0x60000),
                                             vectors=[(f"$r{i}",) for i in range(_n)])

# ============================================================================ jgld.dll: _atexit registrars
# Argument-free: one call per key, three fixture variants ($alog holds 0, 3 or 1 earlier entries; _fail's recorder
# returns -1).
for _name, _addr in (("registerArrayE0Teardown", 0x00065050), ("registerModesTeardown", 0x00065160),
                     ("registerDevmodesTeardown", 0x00065270)):
    for _v in ("a", "b", "fail"):
        HOOKS[f"{_name}_{_v}"] = dict(_J, addr=_addr, abi="default", ret="int", args=[], fixture=f"c3ey_atexit_{_v}",
                                      state=[("$alog", 0, 0x40)], vectors=[()])

# ============================================================================ jgld.dll: libpng defaults
# The png block (fixtures.d/c3ey.js `pngBlock`): png_structs of 0x220 bytes from +0 (each compared from +0x40: the
# jmp_buf at +0..+0x3f holds the harness's registers), data buffers from +0x2400; the error-function log, jgld's
# stderr FILE and buffer, the WriteFile log (and, for the read keys, the file stand-in's block).
def _png(n, extra=()):
    return [("$blk", i * 0x220 + 0x40, 0x1E0) for i in range(n)] + [("$blk", 0x2400, 0x1000), ("$hlog", 0, 0x260),
            ("$giob", 0, 0x20), ("$gbuf", 0, 0x1000), ("$implog", 0, 0x1000)] + list(extra)


for _v in ("a", "b"):
    HOOKS[f"png_default_error_{_v}"] = dict(
        _J, addr=0x000792B0, abi="default", ret="int", args=["pointer", "pointer"], fixture=f"c3ey_deferr_{_v}",
        state=_png(8), vectors=[(f"$P{(i * 3 + (_v == 'b')) % 8}", f"$m{i}") for i in range(12)])

# file i of the fixture holds FILES[i] = 37, 0, 300, 5, 64, 129, 1, 255 bytes; a length above it is a short read
_RV = [("$P0", "$D0", 37), ("$P0", "$D0", 38), ("$P1", "$D1", 0), ("$P1", "$D1", 1), ("$P2", "$D2", 256),
       ("$P2", "$D2", 0), ("$P3", "$D3", 4), ("$P3", "$D3", 6), ("$P4", "$D4", 64), ("$P5", "$D5", 200),
       ("$P5", "$D5", 129), ("$P6", "$D6", 1), ("$P7", "$D7", 255), ("$P7", "$D7", 256)]
for _m in ("jump", "ret", "null"):
    HOOKS[f"png_default_read_data_{_m}"] = dict(
        _J, addr=0x00078D40, abi="default", ret="int", args=["pointer", "pointer", "uint32"], fixture=f"c3ey_read_{_m}",
        state=_png(8, [("$fsblock", 0, 0x2000)]), vectors=_RV)

# ============================================================================ jgld.dll Sprite slots 34 / 35, drawO
# The fixture block $blk holds the eight sprites (+0x10 Palette word, +0x40 / +0x44 the release stub's count and
# argument) and the four destinations; $clog the stood-in callee's calls (lineTo / dashTo).
_SPR = [("$blk", 0, 0x800), ("$clog", 0, 0x800)]
_P = 0x5A1E0001
HOOKS["Sprite::lineTo"] = dict(
    _J, addr=0x00018490, abi="thiscall", ret="int",
    args=["pointer", "pointer", "pointer", "int", "int", "pointer", "pointer"], fixture="c3ey_lineTo", state=_SPR,
    vectors=[("$s0", "$d16", "$s1", 0, 0, 0x1000, 0), ("$s0", "$d16", "$s1", 5, 7, 0x1000, _P),
             ("$s1", "$d8", 0, -3, 2, 0x2000, 0), ("$s2", 0, "$s3", 100, -100, 0, _P + 1),
             ("$s3", "$d24", "$s0", 0x7FFFFFFF, -0x80000000, 0x3000, 0), ("$s4", "$d0", "$s2", 1, 1, 0x10, _P + 2),
             ("$s5", "$d16", "$s4", 9, 8, 0x20, 0), ("$s6", "$d16", "$s5", -1, -1, 0x30, _P + 3),
             ("$s7", "$d8", "$s6", 31, 15, 0x40, 0), ("$s0", "$d16", 0, 2, 3, 0, 0xFFFFFFFF),
             ("$s2", "$d16", "$s7", 4, 4, 0x50, 0), ("$s1", "$d24", "$s2", 6, 0, 0x60, _P + 4)])
_DV5 = [(0, 0), (3, 3), (3, 4), (-1, -1), (2, 1), (0x7FFFFFFF, 0x7FFFFFFF), (5, -5), (1, 1), (8, 8), (0, 1), (7, 7),
        (-0x80000000, -0x80000000)]
HOOKS["Sprite::dashTo"] = dict(
    _J, addr=0x00018510, abi="thiscall", ret="int",
    args=["pointer", "pointer", "pointer", "int", "int", "int", "int", "int", "pointer", "pointer"],
    fixture="c3ey_dashTo", state=_SPR,
    vectors=[(f"$s{i % 8}", ("$d16", "$d8", 0, "$d24")[i % 4], (f"$s{(i + 3) % 8}" if i % 3 else 0), i * 3 - 4, 7 - i,
              a5, a6, i * 11 + 1, 0x1000 * i, (_P + i) if i % 2 else 0) for i, (a5, a6) in enumerate(_DV5)])
HOOKS["Sprite::drawO"] = dict(
    _J, addr=0x000185B0, abi="thiscall", ret="int",
    args=["pointer", "pointer", "int", "int", "pointer", "pointer"], fixture="c3ey_drawO", state=_SPR,
    vectors=[("$s0", "$d8", 0, 0, 0x1000, 0), ("$s0", "$d16", 1, 2, 0x1000, _P), ("$s0", "$d24", 3, 4, 0, 0),
             ("$s1", "$d16", 5, 6, 0x2000, _P + 1), ("$s1", "$d8", 0, 0, 0, 0), ("$s2", "$d16", -1, -2, 0x3000, 0),
             ("$s2", "$d8", 7, 7, 0, _P + 2), ("$s3", "$d16", 8, 9, 0x10, 0), ("$s4", "$d16", 0, 0, 0, _P + 3),
             ("$s5", "$d16", 0, 0, 0, _P + 4), ("$s0", 0, 0, 0, 0, _P + 5), ("$s6", "$d16", 1, 1, 0, 0),
             ("$s7", "$d16", 2, 2, 0x20, _P + 6), ("$s2", "$d0", 3, 3, 0, 0), ("$s5", 0, 0, 0, 0, 0),
             ("$s4", "$d8", 4, 4, 0, 0)])

# Sprite::blitMask(dst, x, y, table, out): $bm is the whole fixture block (sprites, surfaces with their release
# counters, palettes, colour tables, payloads, pixel buffers); $gl the module loop globals 0x10128538..0x1012857f
# (written only by the two arms while the first thread is frozen); $implog the IntersectRect stand-in's log.
_BM = [("$bm", 0, 0xC000), ("$gl", 0, 0x5C), ("$rowskip", 0, 4), ("$implog", 0, 0x400)]
_BMV = [("$S0", "$D0", 0, 0, "$T", "$O0"), ("$S0", "$D0", 5, 3, "$T", "$O0"), ("$S0", "$D0", -4, -2, "$T", "$O0"),
        ("$S0", "$D0", 28, 12, "$T", "$O0"), ("$S0", "$D0", 40, 0, "$T", "$O0"), ("$S0", "$D4", 0, 0, "$T", "$O0"),
        ("$S0", "$D1", 2, 2, "$T", "$O1"), ("$S0", "$D2", 0, 0, "$T", "$O0"), ("$S0", "$D3", 0, 0, "$T", "$O0"),
        ("$S0", "$D0", 0, 0, "$T", 0), ("$S2", "$D0", 0, 0, "$T", "$O0"), ("$S0", 0, 0, 0, "$T", "$O0"),
        ("$S3", "$D0", 3, 1, "$T2", "$O2"), ("$S4", "$D0", 7, 7, "$T", "$O0"), ("$S1", "$D0", 1, 0, "$T", "$O0"),
        ("$S5", "$D4", 10, 8, "$T2", "$O1"), ("$S0", "$D0", 0, -8, "$T", "$O0"), ("$S3", "$D1", -19, 9, "$T", "$O2")]
for _k in ("s111", "nopal", "s121", "s212"):
    HOOKS[f"Sprite::blitMask_{_k}"] = dict(
        _J, addr=0x0002CEC0, abi="thiscall", ret="int",
        args=["pointer", "pointer", "int", "int", "pointer", "pointer"], fixture=f"c3ey_bm_{_k}", state=_BM,
        vectors=_BMV if _k in ("s111", "nopal") else _BMV[:4] + _BMV[7:12])

# Sprite::blitMaskScaled(dst, x, y, a4, a5, table, out): the same block; scale numerators per key (dc0, dc4):
# (1, 1), (3, 2), (-2, 3) with the palette client empty. a5 is the x87 divisor of both ratios, a4 the integer divisor
# of the 16.16 steps (never 0: idiv), a4 < 0 skips the drawing.
_BMSV = [("$S6", "$D0", 0, 0, 1, 1, "$T", "$O0"), ("$S6", "$D0", 3, 2, 2, 1, "$T", "$O0"),
         ("$S6", "$D0", -5, -3, 1, 2, "$T", "$O0"), ("$S6", "$D4", 2, 1, 3, 3, "$T2", "$O0"),
         ("$S7", "$D1", 4, 4, 1, 7, "$T", "$O1"), ("$S6", "$D2", 0, 0, 1, 1, "$T", "$O0"),
         ("$S6", "$D0", 0, 0, -1, 1, "$T", "$O0"), ("$S6", "$D0", 60, 0, 1, 1, "$T", "$O0"),
         ("$S6", "$D0", 0, 0, 1, 1, "$T", 0), ("$S8", "$D0", 0, 0, 1, 1, "$T", "$O0"),
         ("$S7", "$D0", 10, 5, 2, 3, "$T2", "$O2"), ("$S6", "$D4", -2, 9, 1, 3, "$T", "$O0"),
         ("$S7", "$D0", 1, 1, 4, 3, "$T", "$O0"), ("$S6", "$D0", 7, 0, 5, 2, "$T2", "$O2")]
for _k in ("11", "32", "neg"):
    HOOKS[f"Sprite::blitMaskScaled_{_k}"] = dict(
        _J, addr=0x0002D390, abi="thiscall", ret="int",
        args=["pointer", "pointer", "int", "int", "int", "int", "pointer", "pointer"], fixture=f"c3ey_bms_{_k}",
        state=_BM, vectors=_BMSV)

# ============================================================================ Terrain.dll forwarders
# thiscall, `this` only: "$tN" writes the stood-in callee's call count N into $clog before the snapshot
for _name, _addr, _fx in (("Terrain::normalArray", 0x00003270, "c3ey_normal"),
                          ("Terrain::vertexArray", 0x000032B0, "c3ey_vertex")):
    HOOKS[_name] = dict(_T, addr=_addr, abi="thiscall", ret="uint32", args=["pointer"], fixture=_fx,
                        state=[("$clog", 0, 0x800)], vectors=[(f"$t{i}",) for i in range(12)])
    HOOKS[_name + "_real"] = dict(_T, addr=_addr, abi="thiscall", ret="uint32", args=["pointer"],
                                  fixture="c3ey_array_real", vectors=[("$t0",), ("$t1",), ("$t2",), (0x1234,)])
_DV = [("$obj", "$p0"), ("$obj", "$p1"), ("$obj", 0), ("$obj", 0xFFFFFFFF), ("$p2", "$p3"), ("$obj", "$p4"),
       ("$obj", 0x5A5A5A5A), (0, "$p5"), ("$p1", 0x80000000), ("$obj", 1), ("$obj", 0x7FFFFFFF), ("$p3", "$obj")]
HOOKS["std::allocator<Tile*>::destroy"] = dict(_T, addr=0x0000B9F0, abi="thiscall", ret="uint32",
                                               args=["pointer", "pointer"], fixture="c3ey_destroy",
                                               state=[("$clog", 0, 0x800)], vectors=_DV)
HOOKS["std::allocator<Tile*>::destroy_real"] = dict(_T, addr=0x0000B9F0, abi="thiscall", ret="uint32",
                                                    args=["pointer", "pointer"], fixture="c3ey_destroy_real",
                                                    vectors=[("$obj", "$p0"), ("$obj", "$p1"), ("$obj", 0)])

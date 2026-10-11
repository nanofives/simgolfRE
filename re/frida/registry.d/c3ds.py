# Batch c3ds (round 28, jgld.dll): libpng's error path and chunk readers, Surface blend dispatchers, module-global
# methods. Reimplementations: shim/src/re/c3ds.cpp; fixtures: re/frida/js/fixtures.d/c3ds.js; Purposes and per-jcc
# tables: log/c3/c3ds_purpose.md; what was not taken: log/c3/c3ds_notes.txt. addr is the RVA (VA = 0x10000000 + addr).

HOOKS = {}

_J = dict(module="jgld.dll")


def _pngs(n, extra=(), hole=None):
    """State of a c3ds png world (re/frida/js/fixtures.d/c3ds.js `world`): each png_struct from +0x40 (its jmp_buf at
    +0..+0x3f is the catcher's, holding the harness's registers), the infos, reader control blocks, zbufs, the extras
    area, the message log, jgld's stderr FILE and buffer and the WriteFile log."""
    regs = [("$blk", i * 0x220 + 0x40, 0x1E0) for i in range(n)]
    if hole:      # a png_struct field holding a stack address of the arm (z_stream next_out set to a local)
        lo, size = hole
        regs = [r for i in range(n) for r in (("$blk", i * 0x220 + 0x40, lo - 0x40),
                                              ("$blk", i * 0x220 + lo + size, 0x220 - lo - size))]
    regs += [("$blk", 0x3400, n * 0xC0), ("$blk", 0x4600, n * 0x10), ("$blk", 0x4800, n * 0x40),
             ("$blk", 0xB000, 0x1000), ("$hlog", 0, 0x260),
             ("$giob", 0, 0x20), ("$gbuf", 0, 0x1000), ("$implog", 0, 0x1000)]
    return regs + list(extra)


_HEAP = [("$arena", 0, 0x1010), ("$freelog", 0, 0x404)]


def _chunk(key, addr, fixture, n, heap=False, arena=0x1000):
    st = [("$arena", 0, 0x10 + arena), ("$freelog", 0, 0x404)] if heap else []
    HOOKS[key] = dict(_J, addr=addr, abi="default", ret="void", args=["pointer", "pointer", "int"], fixture=fixture,
                      state=_pngs(n, st), vectors=[(f"$P{i}", f"$I{i}", f"$L{i}") for i in range(n)])



# ======================================================================== group E: error entry points
for _mode in ("jump", "ret", "null"):
    _st = _pngs(8)
    HOOKS[f"png_error_{_mode}"] = dict(
        _J, addr=0x00078FE0, abi="default", ret="int", args=["pointer", "pointer"], fixture=f"c3ds_err_{_mode}",
        state=_st, vectors=[(f"$P{i % 8}", f"$m{i}") for i in range(12)])
    HOOKS[f"png_chunk_error_{_mode}"] = dict(
        _J, addr=0x000790A0, abi="default", ret="int", args=["pointer", "pointer"], fixture=f"c3ds_err_{_mode}",
        state=_st, vectors=[(f"$P{(i * 3) % 8}", f"$m{(i * 5) % 12}") for i in range(12)])
HOOKS["png_check_chunk_name"] = dict(
    _J, addr=0x0007C1F0, abi="default", ret="void", args=["pointer", "pointer"], fixture="c3ds_err_jump",
    state=_pngs(8), vectors=[(f"$P{i % 8}", f"$n{i}") for i in range(46)])

# ======================================================================== group C: chunk readers
# One png_struct per vector (fixture specs in c3ds.js: GAMA, SRGB, CHRM, CHRM2, PLTE, TRNS, BKGD, HIST, TEXT);
# vectors (png, info or NULL, length). _oom keys schedule allocation failures through the heap stand-in.
_chunk("png_handle_gAMA", 0x00079CC0, "c3ds_gama", 22)
_chunk("png_handle_sRGB", 0x0007A7D0, "c3ds_srgb", 17)
_chunk("png_handle_sRGB_chrm", 0x0007A7D0, "c3ds_srgb_chrm", 11)
_chunk("png_handle_cHRM", 0x0007A0D0, "c3ds_chrm", 24)
_chunk("png_handle_cHRM_set", 0x0007A0D0, "c3ds_chrm2", 12)
_chunk("png_handle_PLTE", 0x000799E0, "c3ds_plte", 17, heap=True)
_chunk("png_handle_PLTE_oom", 0x000799E0, "c3ds_plte_oom", 4, heap=True)
_chunk("png_handle_tRNS", 0x0007AB40, "c3ds_trns", 19, heap=True)
_chunk("png_handle_tRNS_oom", 0x0007AB40, "c3ds_trns_oom", 4, heap=True)
_chunk("png_handle_bKGD", 0x0007AE70, "c3ds_bkgd", 20)
_chunk("png_handle_hIST", 0x0007B1A0, "c3ds_hist", 12, heap=True)
_chunk("png_handle_hIST_oom", 0x0007B1A0, "c3ds_hist_oom", 3, heap=True)
_chunk("png_handle_tEXt", 0x0007BAF0, "c3ds_text", 14, heap=True)
_chunk("png_handle_tEXt_oom", 0x0007BAF0, "c3ds_text_oom", 5, heap=True)
_chunk("png_handle_zTXt", 0x0007BC40, "c3ds_ztxt", 18, heap=True, arena=0x40000)
_chunk("png_handle_zTXt_oom_all", 0x0007BC40, "c3ds_ztxt_oom_all", 4, heap=True, arena=0x10000)
_chunk("png_handle_zTXt_oom_late", 0x0007BC40, "c3ds_ztxt_oom_late", 4, heap=True, arena=0x10000)

# ======================================================================== group R: chunk loops
for _key, _addr, _fx, _n in (("png_read_info", 0x0006BE20, "c3ds_read_info", 15),
                             ("png_read_end", 0x0006CD00, "c3ds_read_end", 13)):
    HOOKS[_key] = dict(_J, addr=_addr, abi="default", ret="void", args=["pointer", "pointer"], fixture=_fx,
                       state=_pngs(_n, [("$arena", 0, 0x2010), ("$freelog", 0, 0x404)]),
                       vectors=[(f"$P{i}", f"$I{i}") for i in range(_n)])
HOOKS["png_read_finish_row"] = dict(
    _J, addr=0x0007D280, abi="default", ret="void", args=["pointer"], fixture="c3ds_finish_row",
    state=_pngs(24, [("$arena", 0, 0x40010), ("$freelog", 0, 0x404)], hole=(0x80, 4)),
    vectors=[(f"$P{i}",) for i in range(24)])
for _key, _fx, _n in (("png_read_row", "c3ds_read_row", 18), ("png_read_row_skip", "c3ds_read_row_skip", 19),
                      ("png_read_row_il", "c3ds_read_row_il", 9)):
    HOOKS[_key] = dict(
        _J, addr=0x0006C3F0, abi="default", ret="void", args=["pointer", "pointer", "pointer"], fixture=_fx,
        state=_pngs(_n, [("$arena", 0, 0x40010), ("$freelog", 0, 0x404), ("$rows", 0, _n * 0x100)], hole=(0x80, 4)),
        vectors=[(f"$P{i}", f"$R{i}", f"$D{i}") for i in range(_n)])
HOOKS["png_read_image"] = dict(
    _J, addr=0x0006CC50, abi="default", ret="void", args=["pointer", "pointer"], fixture="c3ds_read_image",
    state=_pngs(6, [("$arena", 0, 0x20010), ("$freelog", 0, 0x404), ("$rows", 0, 0x600)], hole=(0x80, 4)),
    vectors=[(f"$P{i}", f"$A{i}") for i in range(6)])
# the arena's first block (arena + 0x10) is the png_struct png_create_struct carves: its jmp_buf (+0..+0x3f) holds the
# registers and return address of the arm's own setjmp call (jgld's frame or the shim's), so it is not compared
_CS = _pngs(0, [("$arena", 0, 0x10), ("$arena", 0x50, 0xFFC0), ("$freelog", 0, 0x404)])
_CV = [(f"$v{i}", e, f, w) for i in range(8) for e, f, w in (("$eptr", "$ejump", "$ewarn"), (0, 0, 0))] +       [(0, "$eptr", "$ejump", "$ewarn"), (0, 0, 0, 0)]
HOOKS["png_create_read_struct"] = dict(
    _J, addr=0x0006BAC0, abi="default", ret="pointer", args=["pointer", "pointer", "pointer", "pointer"],
    fixture="c3ds_create", state=_CS, vectors=_CV)
for _f in (0, 1, 2, 3, 5):
    HOOKS[f"png_create_read_struct_fail{_f}"] = dict(
        _J, addr=0x0006BAC0, abi="default", ret="pointer", args=["pointer", "pointer", "pointer", "pointer"],
        fixture=f"c3ds_create_f{_f}", state=_CS,
        vectors=[("$v0", "$eptr", "$ejump", "$ewarn"), ("$v1", 0, "$ejump", 0), ("$v0", 0, 0, 0)])

# ======================================================================== group S: Surface blend rectangles
# Fixtures c3ds_blend_<k>: c3ce's surface block $blk (0x10200 bytes: surfaces, palettes, pixels, rectangles), the swapped
# display's slot counter $dcount, the palette client $client. 565 / 555: the display's layout answer; client: s3 / s4
# have no palette, so the client's pal2 is used; noclient: neither, so colours without bit 31 pass through masked.
_BS = [("$blk", 0, 0x10200), ("$dcount", 0, 0x10), ("$client", 0, 0x80)]
_SURF16 = ["$s3", "$s4", "$s5", "$s11"]
_RECTS = ["$r_in", "$r_odd", "$r_1x1", 0, "$r_out", "$r_neg", "$r_5x3", "$r_7x5"]
_COLS = [0x8000F81F, 0x80000000, 0x8000FFFF, 5, 0x123, 0x80007BEF]
_ALPHAS = [-7, 0, 1, 37, 50, 100, 101, 63]
_BV = [(_SURF16[i % 4], _RECTS[i % 8], _COLS[i % 6], _ALPHAS[(i * 3) % 8]) for i in range(24)]
_BV.append(("$s9", "$r_in", 0x8000F81F, 50))      # no pixel base: lockAt16 returns NULL
_DV = _BV[:16] + [(s, "$r_in", 0x8000F81F, 50) for s in ("$s0", "$s7", "$s8", "$s9", "$s14", "$s15")]
for _k in ("565", "555", "client", "noclient"):
    for _name, _addr, _v in (("Surface::blendRect16", 0x0000DFC0, _BV), ("Surface::ditherBlendRect16", 0x0000E8B0, _BV),
                             ("Surface::blendRectDispatch", 0x00009AC0, _DV),
                             ("Surface::ditherBlendRectDispatch", 0x00009E40, _DV)):
        if _k in ("client", "noclient") and "Dispatch" in _name:
            continue
        HOOKS[f"{_name}_{_k}"] = dict(_J, addr=_addr, abi="thiscall", ret="int",
                                      args=["pointer", "pointer", "uint32", "int"], fixture=f"c3ds_blend_{_k}",
                                      state=_BS, vectors=_v)
_CLIPV = [(s, rc) for s, rc in (("$s0", "$r_in"), ("$s0", "$r_full"), ("$s1", 0), ("$s2", "$r_out"), ("$s3", "$r_odd"),
                                  ("$s3", "$r_full"), ("$s4", "$r_neg"), ("$s9", "$r_in"), ("$s11", "$r_1x1"),
                                  ("$s11", "$r_empty"), ("$s12", "$r_s12"), ("$s13", "$r_in"), ("$s7", "$r_s24"),
                                  ("$s5", "$r_inv"), ("$s8", "$r_7x5"), ("$s10", "$r_full"))]
for _k in ("ok", "fail"):
    HOOKS[f"Surface::setClip_{_k}"] = dict(_J, addr=0x000083C0, abi="thiscall", ret="int", args=["pointer", "pointer"],
                                           fixture=f"c3ds_clip_{_k}", state=[("$blk", 0, 0x10200), ("$implog", 0, 0x800)],
                                           vectors=_CLIPV)

# ======================================================================== group M: module-global methods
HOOKS["Palette::update"] = dict(_J, addr=0x0006A3D0, abi="thiscall", ret="void", args=["pointer"], fixture="c3ds_palette",
                                state=[("$blk", 0, 12 * 0x820), ("$implog", 0, 0x400)],
                                vectors=[(f"$Q{i}",) for i in range(12)])
_MV = [(f"$D{i % 4}", x) for i, x in enumerate((0, 1, 2, 3, 4, -1, -2, 0x7FFFFFFF, -0x80000000, 5, 1, 2))]
for _k in ("ok", "fail", "640"):
    HOOKS[f"Display::setModeIndex_{_k}"] = dict(
        _J, addr=0x000662A0, abi="thiscall", ret="int", args=["pointer", "int"], fixture=f"c3ds_mode_{_k}",
        state=[("$blk", 0, 0x1000), ("$modelog", 0, 0x200), ("$implog", 0, 0x800)], vectors=_MV)
_FV = [(f"$F{i % 4}", ("$f0", "$f1", "$f2", "$f3", "$f4", 0)[i % 6], (12, 0, -5, 0x7FFFFFFF, 100, 33)[(i * 5) % 6], i % 8)
       for i in range(14)]
for _k in ("ok", "fail"):
    HOOKS[f"Font::create_{_k}"] = dict(
        _J, addr=0x00068850, abi="thiscall", ret="int", args=["pointer", "pointer", "int", "uint32"],
        fixture=f"c3ds_font_{_k}", state=[("$blk", 0, 0x400), ("$fontlog", 0, 0x80), ("$implog", 0, 0x800)], vectors=_FV)
HOOKS["strcpy_jgld"] = dict(_J, addr=0x0007F760, abi="default", ret="pointer", args=["pointer", "pointer"],
                       fixture="c3ds_strcpy", state=[("$blk", 0, 0x400)],
                       vectors=[(f"$d{i}", f"$s{i}") for i in range(10)] + [(f"$d{(i + 3) % 10}", f"$s{i}") for i in range(4)])

# ======================================================================== group I: static initialisation stubs
# argument-free: one call per key, three fixture patterns per function (c3ds_init_a/b/c)
_IS = [("$gt", 0, 0x20), ("$ga", 0, 0x50), ("$alog", 0, 0x40)]
for _name, _addr in (("registerTrackedListTeardown", 0x000069F0), ("dynInit_trackedList", 0x00006970),
                     ("constructArrayE0", 0x00065010), ("dynInit_arrayE0", 0x00064FD0),
                     ("constructModes", 0x00065120), ("dynInit_modes", 0x000650E0),
                     ("constructDevmodes", 0x00065230), ("dynInit_devmodes", 0x000651F0)):
    for _v in ("a", "b", "c"):
        HOOKS[f"{_name}_{_v}"] = dict(_J, addr=_addr, abi="default", ret="int", args=[], fixture=f"c3ds_init_{_v}",
                                      state=_IS, vectors=[()])

# ======================================================================== added after the coverage run
_chunk("png_handle_pCAL", 0x0007B620, "c3ds_pcal", 19, heap=True, arena=0x2000)
_chunk("png_handle_pCAL_oom", 0x0007B620, "c3ds_pcal_oom", 4, heap=True, arena=0x2000)
HOOKS["png_read_finish_row_2"] = dict(
    _J, addr=0x0007D280, abi="default", ret="void", args=["pointer"], fixture="c3ds_finish_row2",
    state=_pngs(3, [("$arena", 0, 0x10010), ("$freelog", 0, 0x404)], hole=(0x80, 4)), vectors=[("$P0",), ("$P1",), ("$P2",)])
HOOKS["png_read_row_3"] = dict(
    _J, addr=0x0006C3F0, abi="default", ret="void", args=["pointer", "pointer", "pointer"], fixture="c3ds_read_row3",
    state=_pngs(3, [("$arena", 0, 0x40010), ("$freelog", 0, 0x404), ("$rows", 0, 3 * 0x100)], hole=(0x80, 4)),
    vectors=[(f"$P{i}", f"$R{i}", f"$D{i}") for i in range(3)])

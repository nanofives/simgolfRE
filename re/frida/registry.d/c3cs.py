# C3 batch c3cs (round 22, 2026-10-09) of golf_clean.exe: IJG libjpeg 6a compress-side routines (sequential
# Huffman encoder, scan-header marker writers, virtual-array access, jzero_far) and listGetAt.
# Reimplementations in shim/src/re/c3cs.cpp, fixtures in re/frida/js/fixtures.d/c3cs.js, Purposes and per-jcc
# branch coverage in log/c3/c3cs_purpose.md.
#
# Hooked callees (re-run these keys with SIMGOLF_HOOKS_OFF so the original arm uses original callees):
#   emit_dri / emit_sos  -> 004afbf0 emit_marker, 004afc10 emit_2bytes, 004afbb0 emit_byte (C3, c3af)
#   write_scan_header    -> 004b0120 emit_dht (C3, c3aa), 004b01f0 emit_dri and 004b0220 emit_sos (THIS batch)
#   encode_one_block     -> 004b19e0 dump_buffer (C3, c3bx), 004b1a10 emit_bits (C3, c3aa)
#   emit_restart         -> 004b1b70 flush_bits (C3, c3be), 004b19e0 dump_buffer
#   encode_mcu_huff      -> 004b15c0 encode_one_block and 004b1ae0 emit_restart (THIS batch)
#   encode_mcu_gather    -> 004b1d80 htest_one_block (THIS batch)
#   finish_pass_gather   -> 004afad0 jpeg_alloc_huff_table (C3, c3af), 004b1080 jpeg_gen_optimal_table (THIS)
#   start_pass_huff      -> 004b0f60 jpeg_make_c_derived_tbl (THIS batch)
#   expand_bottom_edge   -> 004b04f0 jcopy_sample_rows (C3, c3w)
#   access_virt_sarray   -> 004af690 do_sarray_io and 004b0540 jzero_far (THIS batch)
#   access_virt_barray   -> 004af880 do_barray_io and 004b0540 jzero_far (THIS batch)
# The pointers the fixtures install (error_exit at err+0, alloc_small at mem+0, empty_output_buffer at dest+0xc,
# the backing-store read/write at ptr+0x30 / +0x34) are the fixture's own callbacks, not hooks.

_JPEG = dict(module="golf_clean.exe", abi="default")


def _key(addr, ret, args, fixture, state, vectors):
    return dict(_JPEG, addr=addr, ret=ret, args=args, fixture=fixture, state=state, vectors=vectors)


# ---- jzero_far ---------------------------------------------------------------------------------------------------
_ZERO_VECTORS = [("$buf", 0), ("$buf", 1), ("$buf", 2), ("$buf", 3), ("$buf", 4), ("$buf", 5), ("$buf", 7),
                 ("$buf", 8), ("$b1", 1), ("$b1", 6), ("$b3", 9), ("$b3", 0x40), ("$b6", 0x81), ("$buf", 0xFF),
                 ("$buf", 0x100), ("$b1", 0xFE)]

# ---- listGetAt ---------------------------------------------------------------------------------------------------
_INT_MIN, _INT_MAX = -0x80000000, 0x7FFFFFFF
_LIST_VECTORS = ([("$o0", i) for i in (-6, -5, -4, -1, 0, 1, 2, 4, 5, 6, _INT_MIN, _INT_MAX)]
                 + [("$o1", i) for i in (-5, -2, 3, 4, -6)]
                 + [("$o2", i) for i in (-1, 0, _INT_MIN, 3)]
                 + [("$o3", i) for i in (0, -1, 1, -2)])
_LIST_STATE = [(f"$o{i}", 0x80, 0x10) for i in range(4)]

# ---- htest_one_block ---------------------------------------------------------------------------------------------
_HTEST_VECTORS = [("$k0", 0), ("$k1", 0), ("$k1", 100), ("$k2", -5), ("$k3", 0), ("$k3", -0x10000), ("$k4", 0),
                  ("$k4", 32767), ("$k5", 1), ("$k6", -1), ("$k7", 0x40000000), ("$k8", 7), ("$k9", 0),
                  ("$k10", -100), ("$k11", 12345), ("$k12", 0), ("$k13", -0x40000000)]

# ---- jpeg_make_c_derived_tbl / jpeg_gen_optimal_table ------------------------------------------------------------
_DERIVED_VECTORS = [("$c", f"$h{i}", s) for i in range(6) for s in ("$snull", "$spre")]
_DERIVED_STATE = [("$snull", 0, 4), ("$spre", 0, 4), ("$pre", 0, 0x500), ("$pool", 0, 0x610)]
_GENOPT_STATE = ([(f"$f{i}", 0, 0x404) for i in range(12)] + [(f"$h{i}", 0, 0x118) for i in range(12)]
                 + [("$err", 0, 0x84)])

# ---- marker writers ----------------------------------------------------------------------------------------------
_DEST = [("$err", 0, 0x84), ("$dest", 0, 0x20), ("$buf", 0, 0x400), ("$spare", 0, 0x40), ("$rec", 0, 0x40)]
_WSH_STATE = ([("$err", 0, 0x84), ("$dest", 0, 0x20), ("$buf", 0, 0x1000), ("$spare", 0, 0x40), ("$rec", 0, 0x40)]
              + [(f"${t}{i}_{s}", 0, 0x118) for i in range(10) for s in range(4) for t in ("d", "a")])


# ---- encode_one_block / emit_restart (working-state fixtures) ----------------------------------------------------
def _ws_state(n):
    return ([(f"$s{i}", 0, 0x24) for i in range(n)] + [(f"$b{i}", 0, 0x200) for i in range(n)]
            + [(f"$p{i}", 0, 0x40) for i in range(n)] + [(f"$d{i}", 0, 0x20) for i in range(n)]
            + [(f"$e{i}", 0, 0x84) for i in range(n)] + [(f"$r{i}", 0, 0x40) for i in range(n)])


def _eob_key(fixture, dcs):
    n = len(dcs)
    return _key(0x004B15C0, "int", ["pointer", "pointer", "int", "pointer", "pointer"], fixture, _ws_state(n),
                [(f"$s{i}", f"$q{i}", dc, f"$dt{i}", f"$at{i}") for i, dc in enumerate(dcs)])


def _rst_key(fixture, nums):
    return _key(0x004B1AE0, "int", ["pointer", "int"], fixture, _ws_state(len(nums)),
                [(f"$s{i}", num) for i, num in enumerate(nums)])


# last_dc_val per row of C3CS_EOB_ROWS (fixtures.d/c3cs.js); the other encode fixtures use 0 throughout.
_EOB_DC = [0, 0, 100, -100, 32767, 0, 0, 0xFF, -0x10000, 0, 0x40000000, -0x40000000, 0, 1]
# restart_num per row of C3CS_RST_ROWS / C3CS_RST_TIGHT_ROWS.
_RST_NUMS = [0, 1, 2, 3, 4, 5, 6, 7, 8, -1, 0x130, 3]
_RST_TIGHT_NUMS = [1, 2, 3, 4, 5, 6, 7, 0]


# ---- encode_mcu_huff / encode_mcu_gather -------------------------------------------------------------------------
def _mcu_key(addr, fixture, n):
    state = ([(f"$n{i}", 0, 0x6C) for i in range(n)] + [(f"$d{i}", 0, 0x20) for i in range(n)]
             + [(f"$b{i}", 0, 0x400) for i in range(n)] + [(f"$p{i}", 0, 0x40) for i in range(n)]
             + [(f"$e{i}", 0, 0x84) for i in range(n)] + [(f"$r{i}", 0, 0x40) for i in range(n)])
    if addr == 0x004B1CA0:
        state = [(f"$n{i}", 0, 0x6C) for i in range(n)] + [(f"$t{i}_{j}", 0, 0x404) for i in range(n)
                                                           for j in range(8)]
    return _key(addr, "int", ["pointer", "pointer"], fixture, state, [(f"$c{i}", f"$m{i}") for i in range(n)])


# ---- finish_pass_gather ------------------------------------------------------------------------------------------
# (dc slots, ac slots) already holding a table, per row of C3CS_FPG_ROWS.
_FPG_SLOTS = [([], []), ([], []), ([0, 1, 2, 3], [0, 1, 2, 3]), ([3], [1]), ([], []), ([0], [3]), ([], [0]), ([0], [])]
_FPG_STATE = ([(f"$c{i}", 0, 0x180) for i in range(8)] + [(f"$pool{i}", 0, 0x10 + 8 * 0x120) for i in range(8)]
              + [(f"$e{i}", 0, 0x84) for i in range(8)] + [(f"$f{i}_{t}", 0, 0x404) for i in range(8) for t in range(8)]
              + [(f"$hd{i}_{s}", 0, 0x118) for i, (dc, ac) in enumerate(_FPG_SLOTS) for s in dc]
              + [(f"$ha{i}_{s}", 0, 0x118) for i, (dc, ac) in enumerate(_FPG_SLOTS) for s in ac])

# ---- start_pass_huff ---------------------------------------------------------------------------------------------
# (gather, preDerived, preCount) per row of C3CS_SPH_ROWS.
_SPH_ROWS = [(0, [], []), (0, [], []), (0, [0, 1], []), (1, [], []), (1, [], [2, 3]), (1, [], [0]), (0, [], []),
             (1, [], [0, 1, 2, 3]), (1, [], []), (0, [], []), (0, [], []), (1, [], [3]), (1, [], []),
             (0, [], []), (0, [], [])]
_SPH_STATE = ([(f"$c{i}", 0, 0x180) for i in range(15)] + [(f"$n{i}", 0, 0x80) for i in range(15)]
              + [(f"$pool{i}", 0, 0x2910) for i in range(15)] + [(f"$e{i}", 0, 0x84) for i in range(15)]
              + [("$x12_6c", 0, 0x404), ("$spare13", 0, 0x118), ("$spare14", 0, 0x118)]
              + [(f"$x{i}_{o}_{t}", 0, 0x500) for i, (g, pd, pc) in enumerate(_SPH_ROWS) for t in pd for o in ("2c", "3c")]
              + [(f"$x{i}_{o}_{t}", 0, 0x404) for i, (g, pd, pc) in enumerate(_SPH_ROWS) for t in pc for o in ("4c", "5c")])

# ---- expand_bottom_edge ------------------------------------------------------------------------------------------
_EBE_VECTORS = [("$img", 16, 3, 8), ("$img", 5, 1, 8), ("$img", 16, 8, 8), ("$img", 16, 9, 8), ("$img", 0, 2, 5),
                ("$img", 7, 4, 6), ("$img", 16, 1, 2), ("$img", 3, 7, 8), ("$img", 16, 2, 1), ("$img", 12, 5, 8)]


# ---- virtual arrays ----------------------------------------------------------------------------------------------
def _virt_state(perrow, row_bytes):
    n = len(perrow)
    return ([(f"$v{i}", 0, 0x40) for i in range(n)] + [(f"$data{i}", 0, 16 * p * row_bytes) for i, p in enumerate(perrow)]
            + [(f"$r{i}", 0, 0x40) for i in range(n)] + [(f"$e{i}", 0, 0x84) for i in range(n)]
            + [(f"$mb{i}", 0, 0x40) for i in range(n)])


_SIO_PERROW = [8, 8, 8, 8, 8, 8, 5, 3, 7, 1]
_BIO_PERROW = [1, 1, 2, 1, 1, 1, 2, 1]
# (row, start_row, num_rows, writable) per vector, rows of C3CS_AV_ROWS.
_AV_VECTORS = [(0, 4, 2, 0), (0, 5, 3, 1), (0, 7, 1, 0), (1, 4, 3, 0), (1, 5, 3, 1), (2, 4, 3, 0), (2, 4, 3, 1),
               (3, 6, 2, 1), (3, 6, 2, 0), (4, 9, 3, 0), (4, 8, 2, 1), (5, 3, 3, 0), (5, 5, 3, 1), (6, 2, 2, 0),
               (7, 15, 3, 0), (7, 0, 4, 0), (7, 1, 4, 1), (8, 1, 3, 1), (8, 3, 1, 0), (9, 1, 2, 0), (9, 0, 3, 1),
               (10, 2, 3, 1), (10, 3, 2, 0), (11, 1, 2, 0), (12, 2, 6, 0), (12, 2, 6, 1)]
_AV_PERROW = [8] * 13
_AVB_PERROW = [1 + (i % 2) for i in range(13)]


def _av_key(addr, fixture, perrow, row_bytes):
    return _key(addr, "pointer", ["pointer", "pointer", "uint32", "uint32", "int"], fixture,
                _virt_state(perrow, row_bytes),
                [(f"$c{r}", f"$v{r}", s, n, w) for r, s, n, w in _AV_VECTORS])


HOOKS = {
    # jzero_far 0x004b0540: lengths 0..0x100 with every value of bytes & 3 at three misalignments, so both
    # `rep` loops (0x004b0550 dwords, 0x004b0557 bytes) run with 0 and non-zero counts. Leaf, 16 vectors.
    "jzero_far": _key(0x004B0540, "void", ["pointer", "uint32"], "c3cs_zero", [("$buf", 0, 0x110)], _ZERO_VECTORS),

    # listGetAt 0x004a48f0 (thiscall): indices above count - 1 (0x004a4903), negative ones within and beyond the
    # count (0x004a4913 / 0x004a491c), INT_MIN (|index| stays negative), positive walks forward, and an empty list
    # (head NULL, 0x004a495d). Leaf, 25 vectors.
    "listGetAt": dict(module="golf_clean.exe", addr=0x004A48F0, abi="thiscall", ret="pointer",
                      args=["pointer", "int"], fixture="c3cs_list", state=_LIST_STATE, vectors=_LIST_VECTORS),

    # htest_one_block 0x004b1d80: the fourteen sample blocks with DC predictions that make the difference zero,
    # positive, negative and up to 31 bits; runs below 16, of exactly 16, 32 and 49 zeros (0x004b1dc3), coefficients
    # of magnitude 1..32768 and blocks with and without a trailing run (0x004b1e15). Leaf, 17 vectors.
    "htest_one_block": _key(0x004B1D80, "void", ["pointer", "int", "pointer", "pointer"], "c3cs_htest",
                            [("$dc", 0, 0x404), ("$ac", 0, 0x404)],
                            [(b, dc, "$dc", "$ac") for b, dc in _HTEST_VECTORS]),

    # jpeg_make_c_derived_tbl 0x004b0f60: six JHUFF_TBLs (12 codes, 162 codes, none, 256 codes with repeated
    # symbols, gaps between lengths, two 1-bit codes), each into a NULL slot (allocated, 0x004b0f74) and into an
    # existing table (0x004b0f74 taken). 12 vectors.
    "jpeg_make_c_derived_tbl": _key(0x004B0F60, "void", ["pointer", "pointer", "pointer"], "c3cs_derived",
                                    _DERIVED_STATE, _DERIVED_VECTORS),

    # jpeg_gen_optimal_table 0x004b1080: twelve frequency sets: one symbol, a short skewed set, 256 ties, 256
    # random counts, Fibonacci sets reaching lengths 20, 30, 33 and 35 (the fold at 0x004b11d1, ERREXIT 0x26 at
    # 0x004b11b1), counts above 1e9 that are never picked (0x004b10db), and ties with the reserved symbol 256.
    # Leaf, 12 vectors.
    "jpeg_gen_optimal_table": _key(0x004B1080, "void", ["pointer", "pointer", "pointer"], "c3cs_genopt",
                                   _GENOPT_STATE, [("$c", f"$h{i}", f"$f{i}") for i in range(12)]),

    # emit_dri 0x004b01f0: eight restart intervals (emit_2bytes keeps 16 bits), roomy destination. 8 vectors.
    "emit_dri": _key(0x004B01F0, "void", ["pointer"], "c3cs_dri", _DEST, [(f"$c{i}",) for i in range(8)]),
    # emit_dri with one free byte refilled one at a time (empty_output_buffer per byte). 4 vectors.
    "emit_dri_tight": _key(0x004B01F0, "void", ["pointer"], "c3cs_dri_tight", _DEST, [(f"$c{i}",) for i in range(4)]),

    # emit_sos 0x004b0220: comps_in_scan 0..4 (0x004b025d), sequential and progressive (0x004b0282), AC scans
    # (0x004b028c), first DC scans, DC refinements with and without arith_code (0x004b0298 / 0x004b02a2), values
    # above a byte. 12 vectors.
    "emit_sos": _key(0x004B0220, "void", ["pointer"], "c3cs_sos", _DEST, [(f"$c{i}",) for i in range(12)]),

    # write_scan_header 0x004b0060: arith_code with and without DRI (0x004b006e / 0x004b0107), sequential with 0..3
    # components, progressive DC first / DC refinement / AC scans (0x004b009c / 0x004b00a6 / 0x004b00b0), tables
    # already sent, restart interval set and clear. 10 vectors.
    "write_scan_header": _key(0x004B0060, "void", ["pointer"], "c3cs_wsh", _WSH_STATE, [(f"$c{i}",) for i in range(10)]),

    # encode_one_block 0x004b15c0, roomy buffer: every block shape, DC differences of 0..31 bits both signs, bits
    # left in put_buffer on entry. 14 vectors.
    "encode_one_block": _eob_key("c3cs_eob", _EOB_DC),
    # A refilling destination with 1..4 free bytes: dump_buffer at the inlined copies' plain and stuffed-zero byte
    # sites. 8 vectors.
    "encode_one_block_tight": _eob_key("c3cs_eob_tight", [0] * 8),
    # A suspending destination after 1..17 bytes: each `je 0x004b19c4` return-FALSE exit, including the
    # end-of-block emit_bits call (0x004b19c2). 19 vectors.
    "encode_one_block_suspend": _eob_key("c3cs_eob_suspend", [0] * 19),
    # Size-0 table entries: ERREXIT 0x27 at 0x004b1612, 0x004b17b0 and 0x004b18c2 (and emit_bits' own for EOB).
    # 8 vectors.
    "encode_one_block_zero": _eob_key("c3cs_eob_zero", [0] * 8),

    # emit_restart 0x004b1ae0: put_bits 0..7, restart numbers 0..7 and outside (the byte is num - 0x30),
    # comps_in_scan 0..4 (0x004b1b4b). 12 vectors.
    "emit_restart": _rst_key("c3cs_rst", _RST_NUMS),
    # emit_restart with suspending (flush_bits, 0x004b1b12, 0x004b1b3a) and per-byte refilling destinations.
    # 8 vectors.
    "emit_restart_tight": _rst_key("c3cs_rst_tight", _RST_TIGHT_NUMS),

    # encode_mcu_huff 0x004b1480: 0..6 blocks over four components, no restart interval, a restart marker first
    # (0x004b14c2), next_restart_num wrapping (0x004b159c), a counter that only decrements (0x004b1593). 10 vectors.
    "encode_mcu_huff": _mcu_key(0x004B1480, "c3cs_mcu", 10),
    # encode_mcu_huff with a suspending destination: emit_restart fails (0x004b14d7) or a block fails
    # (0x004b1539) with nothing written back. 5 vectors.
    "encode_mcu_huff_suspend": _mcu_key(0x004B1480, "c3cs_mcu_suspend", 5),

    # encode_mcu_gather 0x004b1ca0: 0..6 blocks, no restart interval (0x004b1cb5), a reload that clears
    # last_dc_val (0x004b1cbc / 0x004b1cc8, comps_in_scan 0..4), a counter that only decrements. 10 vectors.
    "encode_mcu_gather": _mcu_key(0x004B1CA0, "c3cs_gather", 10),

    # finish_pass_gather 0x004b1e20: 0..4 components, repeated table numbers (did_dc / did_ac at 0x004b1e89 /
    # 0x004b1ec8), NULL and present table slots (0x004b1e93 / 0x004b1ed2). 8 vectors.
    "finish_pass_gather": _key(0x004B1E20, "void", ["pointer"], "c3cs_fpg", _FPG_STATE, [(f"$c{i}",) for i in range(8)]),

    # start_pass_huff 0x004b12f0: both modes (0x004b1307 / 0x004b13b8), count arrays and derived tables allocated
    # or reused (0x004b13c0 / 0x004b13e9), out-of-range table numbers (ERREXIT1 0x31 at 0x004b137e / 0x004b13ad), a
    # missing AC table while gathering (0x004b139a), no component, a negative DC and an AC number of 4 while
    # gathering (0x004b135a / 0x004b138a), and NULL DC / AC slots while not gathering (0x004b136b / 0x004b139a) with
    # an error_exit that records, then fills the slot the original re-reads (0x004b140e / 0x004b141e). 15 vectors.
    "start_pass_huff": _key(0x004B12F0, "void", ["pointer", "int"], "c3cs_sph", _SPH_STATE,
                            [(f"$c{i}", g) for i, (g, pd, pc) in enumerate(_SPH_ROWS)]),

    # expand_bottom_edge 0x004b3d00: row ranges of 0..7 rows, empty and inverted ranges (0x004b3d0c), widths 0..16.
    # 10 vectors.
    "expand_bottom_edge": _key(0x004B3D00, "void", ["pointer", "uint32", "int", "int"], "c3cs_ebe",
                               [("$rows", 0, 0x80), ("$img", 0, 0x20)], _EBE_VECTORS),

    # do_sarray_io 0x004af690: chunk sizes 1..8, limits by rows_in_mem, first_undef_row and rows_in_array
    # (0x004af6bb / 0x004af6cb / 0x004af6d6), no row at all (0x004af6dc), rows_in_mem 0 (0x004af6ac); each row read
    # (0x004af712) and written (0x004af6fc). Leaf, 20 vectors.
    "do_sarray_io": _key(0x004AF690, "void", ["pointer", "pointer", "int"], "c3cs_sio",
                         _virt_state(_SIO_PERROW, 1),
                         [(f"$c{i}", f"$v{i}", w) for i in range(len(_SIO_PERROW)) for w in (0, 1)]),
    # do_barray_io 0x004af880: the same cases over 0x80 / 0x100-byte block rows. Leaf, 16 vectors.
    "do_barray_io": _key(0x004AF880, "void", ["pointer", "pointer", "int"], "c3cs_bio",
                         _virt_state(_BIO_PERROW, 128),
                         [(f"$c{i}", f"$v{i}", w) for i in range(len(_BIO_PERROW)) for w in (0, 1)]),

    # access_virt_sarray 0x004af540: in-window reads and writes, reads and writes past first_undef_row with and
    # without pre_zero, window moves forward / backward / clamped to 0 with a dirty write-back, a move without
    # backing store (ERREXIT 0x44), the three bad-access ERREXIT 0x14 conditions and an empty zero range below the
    # window (0x004af62d). 26 vectors.
    "access_virt_sarray": _av_key(0x004AF540, "c3cs_avs", _AV_PERROW, 1),
    # access_virt_barray 0x004af730: the same cases over block rows. 26 vectors.
    "access_virt_barray": _av_key(0x004AF730, "c3cs_avb", _AVB_PERROW, 128),
}

# C3 batch c3cv (round 23, 2026-10-09) of golf_clean.exe: the rest of IJG libjpeg 6a's compress side (main
# controller jcmainct.c, coefficient controller jccoefct.c, progressive Huffman encoder jcphuff.c, preprocessing
# controller jcprepct.c). Reimplementations in shim/src/re/c3cv.cpp, fixtures in re/frida/js/fixtures.d/c3cv.js,
# Purposes and per-jcc branch coverage in log/c3/c3cv_purpose.md.
#
# Every key builds its objects in one arena and its state is that whole arena (C3CV_ARENA must equal ARENA in the
# fixture file): everything the function, its callees and the fixture callbacks write lives in it.
#
# Hooked callees (re-run these keys with SIMGOLF_HOOKS_OFF so the original arm uses original callees):
#   start_pass_coef / compress_data / compress_output -> 004b0860 start_iMCU_row (C3, c3be)
#   compress_data / compress_first_pass               -> 004b0540 jzero_far (C3, c3cs)
#   compress_first_pass                               -> 004b0d70 compress_output (THIS batch)
#   start_pass_phuff                                  -> 004b0f60 jpeg_make_c_derived_tbl (C3, c3cs)
#   emit_restart_4b2460                               -> 004b25d0 emit_eobrun (C3, c3bo), 004b2510 flush_bits_4b2510
#                                                        (C3, c3aa), 004b2420 dump_buffer_4b2420 (C3, c3be)
#   encode_mcu_DC_first / encode_mcu_DC_refine        -> 004b2460 emit_restart_4b2460 (THIS batch), 004b2420
#   encode_mcu_AC_first                               -> the same plus 004b25d0
#   encode_mcu_AC_refine                              -> the same plus 004b27d0 emit_buffered_bits (C3, c3aa)
#   finish_pass_gather_phuff                          -> 004b25d0, 004afad0 jpeg_alloc_huff_table (C3, c3af),
#                                                        004b1080 jpeg_gen_optimal_table (C3, c3cs)
#   pre_process_data                                  -> 004b3d00 expand_bottom_edge (C3, c3cs)
#   pre_process_context                               -> 004b04f0 jcopy_sample_rows (C3, c3w), 004b3d00
# The method pointers the fixtures install (error_exit, alloc_small, access_virt_barray, forward_DCT, encode_mcu,
# pre_process_data, compress_data, color_convert, downsample, empty_output_buffer) are fixture callbacks, not hooks.

C3CV_ARENA = dict(spm=0x4000, pdsm=0x4000, spc=0x4000, cd=0xA000, cfp=0x1C000, co=0x18000, jpe=0x8000, spp=0x2A000,
                  dcf=0x18000, dcr=0x12000, acf=0x18000, acr=0x18000, rst=0x10000, fpg=0x14000, ppd=0x8000,
                  ppc=0x8000)


def _key(addr, ret, args, fx, vectors):
    return dict(module="golf_clean.exe", addr=addr, abi="default", ret=ret, args=args, fixture="c3cv_" + fx,
                state=[("$arena", 0, C3CV_ARENA[fx])], vectors=vectors)


def _cinfo(n):
    return [(f"$c{i}",) for i in range(n)]


# start_pass_main: pass modes per row of SPM_ROWS.
_SPM_MODES = [0, 0, 1, 2, 3, 2, -1, 0x100, 0, 1, 0, 4]
# process_data_simple_main: in_rows_avail per row of PDSM_ROWS.
_PDSM_AVAIL = [16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16]
# start_pass_coef: pass modes per row of SPC_ROWS.
_SPC_MODES = [0, 0, 2, 2, 3, 3, 1, 4, -1, 0, 3, 2]
# pre_process_*: in_rows_avail / out_row_groups_avail per row of PPD_ROWS / PPC_ROWS.
_PPD = [(4, 1), (4, 1), (2, 1), (1, 2), (5, 2), (3, 3), (4, 3), (2, 3), (1, 2), (3, 1), (9, 3), (2, 2)]
_PPC = [(3, 2), (3, 2), (3, 2), (3, 2), (3, 1), (7, 2), (4, 2), (3, 2), (2, 1), (2, 2), (5, 3), (1, 2), (2, 1)]
# emit_restart_4b2460: restart_num per row of RST_ROWS.
_RST_NUMS = [0, 1, 2, 3, 4, 5, 6, 7, -1, 0x130, 3, 6]


def _prep(addr, fx, rows):
    return _key(addr, "void", ["pointer", "pointer", "pointer", "uint32", "pointer", "pointer", "uint32"], fx,
                [(f"$c{i}", f"$i{i}", f"$ri{i}", a, f"$o{i}", f"$ro{i}", o) for i, (a, o) in enumerate(rows)])


HOOKS = {
    # start_pass_main 0x004b05f0: raw_data_in set / clear (0x004b0602), pass mode 0 and seven others (ERREXIT 4
    # through a recording, returning error_exit, 0x004b0622). Leaf, 12 vectors.
    "start_pass_main": _key(0x004B05F0, "void", ["pointer", "int"], "spm",
                            [(f"$c{i}", m) for i, m in enumerate(_SPM_MODES)]),

    # process_data_simple_main 0x004b0640: no row left (0x004b0659), rowgroup_ctr already 8 / above 8 (0x004b0669,
    # 0x004b068f), a partial row group, compress_data failing fresh and already suspended (0x004b06db), a success
    # after a suspension (0x004b06ab), several iMCU rows (0x004b06d2). Leaf (callees through pointers), 14 vectors.
    "process_data_simple_main": _key(0x004B0640, "void", ["pointer", "pointer", "pointer", "uint32"], "pdsm",
                                     [(f"$c{i}", "$ib", f"$ir{i}", a) for i, a in enumerate(_PDSM_AVAIL)]),

    # start_pass_coef 0x004b07b0: modes 0 / 2 / 3 with whole_image[0] NULL and set (0x004b083a, 0x004b0818,
    # 0x004b07f6), modes 1 / 4 / -1 (default ERREXIT), comps_in_scan 1..3 and both start_iMCU_row arms. 12 vectors.
    "start_pass_coef": _key(0x004B07B0, "void", ["pointer", "int"], "spc",
                            [(f"$c{i}", m) for i, m in enumerate(_SPC_MODES)]),

    # compress_data 0x004b08b0: right-edge dummies (0x004b09fe), bottom dummy rows (0x004b0973 / 0x004b0984), an
    # empty MCU_height / MCU_width / scan (0x004b095e / 0x004b09a1 / 0x004b090c), a start column past the last
    # (0x004b08f8), a finished iMCU row (0x004b08e3), encode_mcu failing at calls 0, 1, 2 (0x004b0abb), two MCU rows.
    # 12 vectors.
    "compress_data": _key(0x004B08B0, "int", ["pointer", "pointer"], "cd",
                          [(f"$c{i}", f"$i{i}") for i in range(12)]),

    # compress_first_pass 0x004b0b20: last and not-last iMCU rows (0x004b0b9f / 0x004b0c7b), height remainders 0 and
    # not (0x004b0bb4), right dummies (0x004b0bd2), bottom dummy rows over MCUs (0x004b0c96 / 0x004b0cc9), no
    # component, compress_output failing. 11 vectors.
    "compress_first_pass": _key(0x004B0B20, "int", ["pointer", "pointer"], "cfp",
                                [(f"$c{i}", f"$i{i}") for i in range(13)]),

    # compress_output 0x004b0d70: interleaved and single-component scans, start column / row offsets, MCUs_per_row 0
    # (0x004b0e0c), no component (0x004b0d8f / 0x004b0e20), MCU_height 0 / MCU_width 0 (0x004b0e53 / 0x004b0e76),
    # encode_mcu failing at calls 0 and 2 (0x004b0ef5). 11 vectors.
    "compress_output": _key(0x004B0D70, "int", ["pointer", "pointer"], "co",
                            [(f"$c{i}", f"$i{i}") for i in range(11)]),

    # jinit_phuff_encoder 0x004b1f30: ten pools with different pre-fills and cursors. Leaf, 10 vectors.
    "jinit_phuff_encoder": _key(0x004B1F30, "void", ["pointer"], "jpe", _cinfo(10)),

    # start_pass_phuff 0x004b1f70: the four encode_mcu selections (0x004b1fa6 / 0x004b1faa / 0x004b1fc0), the
    # bit_buffer allocation (0x004b1fd7), both finish_pass values (0x004b1fee), DC refinement skipping components
    # (0x004b203c), counts allocated / reused (0x004b20a3), derived tables built, out-of-range and missing tables
    # (ERREXIT1 0x31, 0x004b2047..0x004b207d) in both modes. 16 vectors.
    "start_pass_phuff": _key(0x004B1F70, "void", ["pointer", "int"], "spp",
                             [(f"$c{i}", g) for i, g in enumerate([0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1])]),

    # encode_mcu_DC_first 0x004b2140: emit and gather modes (0x004b2209), Al shifts including a count taken from Al's
    # low byte, DC differences of 0..30 bits both signs, restart markers (0x004b216f / 0x004b2176) with
    # next_restart_num wrapping, per-byte refills, size-0 symbols (ERREXIT 0x27 at 0x004b2235), a suspending
    # destination (ERREXIT from dump_buffer_4b2420), no block. 14 vectors.
    "encode_mcu_DC_first": _key(0x004B2140, "int", ["pointer", "pointer"], "dcf",
                                [(f"$c{i}", f"$m{i}") for i in range(15)]),

    # encode_mcu_DC_refine 0x004b2d30: Al 0..3 and Al's low byte, 0..10 blocks, gathering (0x004b2d9d), restart
    # markers, per-byte refills and a suspending destination. 11 vectors.
    "encode_mcu_DC_refine": _key(0x004B2D30, "int", ["pointer", "pointer"], "dcr",
                                 [(f"$c{i}", f"$m{i}") for i in range(12)]),

    # encode_mcu_AC_first 0x004b28c0: runs below 16, ZRLs (0x004b29a7), coefficients vanishing under Al
    # (0x004b2984), pending EOBRUN (0x004b2995), EOBRUN reaching 0x7fff (0x004b2cdf), gathering, Ss > Se
    # (0x004b2927), size-0 ZRL / symbol (0x004b29f4 / 0x004b2b1b), per-byte refills, a restart, suspension.
    # 14 vectors.
    "encode_mcu_AC_first": _key(0x004B28C0, "int", ["pointer", "pointer"], "acf",
                                [(f"$c{i}", f"$m{i}") for i in range(19)]),

    # encode_mcu_AC_refine 0x004b2eb0: correction bits (0x004b30ea), newly non-zero coefficients of both signs,
    # ZRLs before EOB and runs past it (0x004b2fa8), BE above 0x3a9 (0x004b331f), EOBRUN reaching 0x7fff, only
    # correction bits with no trailing run (0x004b32f9), gathering, Ss > Se, size-0 ZRL / symbol, refills, a restart.
    # 14 vectors.
    "encode_mcu_AC_refine": _key(0x004B2EB0, "int", ["pointer", "pointer"], "acr",
                                 [(f"$c{i}", f"$m{i}") for i in range(23)]),

    # emit_restart_4b2460 0x004b2460: emit and gather (0x004b2473), DC and AC bands (0x004b24cd), 0..4 components,
    # pending EOB runs, refills after the 0xff and the RST byte (0x004b2492 / 0x004b24b7), restart numbers 0..7, -1
    # and 0x130, a suspending destination. 12 vectors.
    "emit_restart_4b2460": _key(0x004B2460, "void", ["pointer", "int"], "rst",
                                [(f"$n{i}", n) for i, n in enumerate(_RST_NUMS)]),

    # finish_pass_gather_phuff 0x004b33c0: DC first, DC refinement (all components skipped, 0x004b342f), AC bands,
    # repeated table numbers (0x004b3443), NULL and present slots (0x004b3454), no component, pending EOB runs.
    # 8 vectors.
    "finish_pass_gather_phuff": _key(0x004B33C0, "void", ["pointer"], "fpg", _cinfo(8)),

    # pre_process_data 0x004b3b80: input / output exhausted on entry (0x004b3b9a / 0x004b3baa), numrows limited by the
    # buffer or the input (0x004b3bc3), bottom-of-image padding of the conversion buffer and of the output
    # (0x004b3c01 / 0x004b3c09 / 0x004b3c90), several row groups, no component. 12 vectors.
    "pre_process_data": _prep(0x004B3B80, "ppd", _PPD),

    # pre_process_context 0x004b3d40: first-pass top padding (0x004b3dbd), max_v 0 (0x004b3ddf), mid-image
    # returns (0x004b3e4d), bottom padding (0x004b3e5b), wrap of this_row_group and next_buf_row (0x004b3ed7 /
    # 0x004b3ee3), no component. 12 vectors.
    "pre_process_context": _prep(0x004B3D40, "ppc", _PPC),
}

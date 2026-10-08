# C3 batch c3bj (round 15, 2026-10-08) of golf_clean.exe: eight IJG libjpeg 6a compress-side setup routines and
# one Bink player setter. Reimplementations in shim/src/re/c3bj.cpp, fixtures in re/frida/js/fixtures.d/c3bj.js,
# per-jcc branch coverage in log/c3/c3bj_purpose.md.
#
# Hooked callees (re-run these keys with SIMGOLF_HOOKS_OFF so the original arm uses original callees):
#   jinit_c_prep_controller (the _ctx key only) -> 004b3f10 create_context_buffer, IN THIS BATCH
#   initial_setup, per_scan_setup               -> 004b04c0 jdiv_round_up (c3r)
# The memory manager and the error manager are reached through the fixture's own callbacks, not through hooks.


def _cinfo_key(addr, nvec, fixture, args=("pointer",), state_pool=True, state_comps=False, state_err=True):
    """One key over `nvec` fixture cinfos: the cinfo, its alloc_small pool, its comp_info array and its error
    manager are the compared state (the ones the function writes)."""
    state = [(f"$c{i}", 0, 0x180) for i in range(nvec)]
    if state_pool:
        state += [(f"$p{i}", 0, 0x1100) for i in range(nvec)]
    if state_comps:
        state += [(f"$k{i}", 0, 16 * 0x54) for i in range(nvec)]
    if state_err:
        state += [(f"$e{i}", 0, 0x84) for i in range(nvec)]
    return dict(module="golf_clean.exe", addr=addr, abi="default", ret="void", args=list(args),
                fixture=fixture, state=state)


HOOKS = {
    # jinit_c_main_controller: raw_data_in (vectors 0, 1) returns before the need_full_buffer test, a non-zero
    # need_full_buffer (2, 3) raises ERREXIT 4, and the rest allocate 0, 1, 2, 3, 4 or 10 strip buffers. Leaf,
    # 12 vectors; the tag alloc_sarray makes the stored pointer show width_in_blocks * 8 and v_samp_factor * 8.
    "jinit_c_main_controller": dict(
        _cinfo_key(0x004B0560, 12, "c3bj_main", args=("pointer", "int")),
        vectors=[("$c0", 0), ("$c1", 1), ("$c2", 1), ("$c3", -1), ("$c4", 0), ("$c5", 0),
                 ("$c6", 0), ("$c7", 0), ("$c8", 0), ("$c9", 0), ("$c10", 0), ("$c11", 0)]),

    # jinit_c_prep_controller, no-context path (downsample->need_context_rows == 0): the colour buffers are
    # allocated here. Vectors 4 and 5 also take the ERREXIT 4, which does NOT return in this function.
    "jinit_c_prep_controller": dict(
        _cinfo_key(0x004B3A80, 11, "c3bj_prep", args=("pointer", "int")),
        vectors=[("$c0", 0), ("$c1", 0), ("$c2", 0), ("$c3", 0), ("$c4", 1), ("$c5", -1),
                 ("$c6", 0), ("$c7", 0), ("$c8", 0), ("$c9", 0), ("$c10", 0)]),

    # jinit_c_prep_controller, context path (need_context_rows != 0): prep+4 takes 0x004b3d40 and
    # create_context_buffer 0x004b3f10 runs. Re-run with SIMGOLF_HOOKS_OFF=004b3f10 (same batch).
    "jinit_c_prep_controller_ctx": dict(
        _cinfo_key(0x004B3A80, 4, "c3bj_prep_ctx", args=("pointer", "int")),
        vectors=[("$c0", 0), ("$c1", 0), ("$c2", 1), ("$c3", 0)]),

    # create_context_buffer: the pre-processing controller comes from the fixture (pre-filled 0x55), so the
    # colour_buf writes at prep+8 and the fake row-pointer block in the pool are both compared. Leaf, 12
    # vectors; 6 and 7 skip the component loop and 8 (max_v_samp_factor 0) skips the wraparound loop.
    "create_context_buffer": dict(
        module="golf_clean.exe", addr=0x004B3F10, abi="default", ret="void", args=["pointer"],
        fixture="c3bj_ctx",
        state=[(f"$p{i}", 0, 0x1100) for i in range(12)] + [(f"$pr{i}", 0, 0x40) for i in range(12)],
        vectors=[(f"$c{i}",) for i in range(12)]),

    # jpeg_set_colorspace: the six jump-table cases, the unsigned default (6, -1, 100, 0x7FFFFFFF,
    # -0x80000000), the JCS_UNKNOWN component-count bounds (0, 1, 10, 11, -3) and two bad global_states
    # (vectors 16, 17). Leaf, 18 vectors.
    "jpeg_set_colorspace": dict(
        _cinfo_key(0x004AE990, 18, "c3bj_cs", args=("pointer", "int"), state_pool=False, state_comps=True),
        vectors=[("$c0", 0), ("$c1", 1), ("$c2", 2), ("$c3", 3), ("$c4", 4), ("$c5", 5),
                 ("$c6", 6), ("$c7", -1), ("$c8", 100), ("$c9", 0), ("$c10", 0), ("$c11", 0),
                 ("$c12", 0), ("$c13", 0), ("$c14", 0x7FFFFFFF), ("$c15", -0x80000000),
                 ("$c16", 3), ("$c17", 1)]),

    # jinit_downsampler: the five method branches, the smoothing variants of two of them, the CCIR601 ERREXIT
    # (vectors 7, 13), the fractional-sampling ERREXIT (6, 16) and the closing trace message (8). Leaf, 17 vectors.
    "jinit_downsampler": dict(
        _cinfo_key(0x004B4040, 17, "c3bj_down"),
        vectors=[(f"$c{i}",) for i in range(17)]),

    # jinit_color_converter: 26 (input_components, in_color_space, num_components, jpeg_color_space)
    # combinations covering both switch tables, every converter pair and each of the three message codes
    # (7, 8, 0x19). Leaf, 26 vectors.
    "jinit_color_converter": dict(
        _cinfo_key(0x004B4A60, 26, "c3bj_cconv"),
        vectors=[(f"$c{i}",) for i in range(26)]),

    # initial_setup: the four parameter guards, the sampling-factor range check and the two component loops.
    # Callee jdiv_round_up 0x004b04c0 (C3, c3r): re-run with SIMGOLF_HOOKS_OFF=004b04c0.
    "initial_setup": dict(
        _cinfo_key(0x004B52A0, 17, "c3bj_setup", state_pool=False, state_comps=True),
        vectors=[(f"$c{i}",) for i in range(17)]),

    # per_scan_setup: the single-component path (vectors 0..3, 11, 12, 14), the interleaved path (4, 5, 6, 9,
    # 10, 13), the comps_in_scan bound (7, 8), the blocks_in_MCU overflow (9) and the restart-interval clamp
    # (3). Callee jdiv_round_up 0x004b04c0 (C3, c3r): re-run with SIMGOLF_HOOKS_OFF=004b04c0.
    "per_scan_setup": dict(
        _cinfo_key(0x004B5AD0, 17, "c3bj_scan", state_pool=False, state_comps=True),
        vectors=[(f"$c{i}",) for i in range(17)]),

    # BinkPlayer::setTarget: the single store at this+0xc. Leaf, 12 vectors over twelve 0xAA-filled objects
    # (nine pointer targets and the numbers 0, 1, -1), so the store and the untouched rest are both compared.
    "BinkPlayer::setTarget": dict(
        module="golf_clean.exe", addr=0x00487050, abi="thiscall", ret="void", args=["pointer", "pointer"],
        fixture="c3bj_bink",
        state=[(f"$o{i}", 0, 0x20) for i in range(12)],
        vectors=[("$o0", "$t0"), ("$o1", "$t1"), ("$o2", "$t2"), ("$o3", "$t3"), ("$o4", "$t4"),
                 ("$o5", "$t5"), ("$o6", "$t6"), ("$o7", "$t7"), ("$o8", "$t8"),
                 ("$o9", 0), ("$o10", 1), ("$o11", -1)]),
}

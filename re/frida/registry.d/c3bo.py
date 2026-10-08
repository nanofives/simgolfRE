# C3 batch c3bo (round 16, 2026-10-08) of golf_clean.exe: four IJG libjpeg 6a compress-side routines.
# Reimplementations in shim/src/re/c3bo.cpp, fixtures in re/frida/js/fixtures.d/c3bo.js, Purposes and per-jcc
# branch coverage in log/c3/c3bo_purpose.md (model: log/c3/c3bo_branches.py).
#
# Hooked callees (re-run these keys with SIMGOLF_HOOKS_OFF so the original arm uses original callees):
#   jinit_c_master_control -> 004b5470 validate_script (IN THIS BATCH) and 004b52a0 initial_setup (c3bj)
#   prepare_for_pass       -> 004b5ad0 per_scan_setup (c3bj), itself calling 004b04c0 jdiv_round_up (c3r)
#   emit_eobrun            -> 004b2420 dump_buffer_4b2420 (c3be) and 004b27d0 emit_buffered_bits (c3aa)
# The memory manager (cinfo+4), the error manager (cinfo+0), the destination manager (cinfo+0x14) and the nine
# prepare_for_pass method slots are the fixture's own callbacks, not hooks.

_COMPS = 16 * 0x54


def _vs_key(fixture, nvec):
    """validate_script: the cinfo (it writes progressive_mode), its error manager (the message recorder) and
    its scan_info array (which the function must not write)."""
    return dict(module="golf_clean.exe", addr=0x004B5470, abi="default", ret="void", args=["pointer"],
                fixture=fixture,
                state=[(f"$c{i}", 0, 0x180) for i in range(nvec)]
                      + [(f"$e{i}", 0, 0x84) for i in range(nvec)]
                      + [(f"$s{i}", 0, 8 * 0x24) for i in range(nvec)],
                vectors=[(f"$c{i}",) for i in range(nvec)])


def _eob_key(fixture, nvec):
    """emit_eobrun: the entropy object, the output buffer it writes through next_output_byte, the destination
    manager dump_buffer resets, the error manager and the statistics array of the selected table."""
    return dict(module="golf_clean.exe", addr=0x004B25D0, abi="default", ret="void", args=["pointer"],
                fixture=fixture,
                state=[(f"$n{i}", 0, 0x6c) for i in range(nvec)]
                      + [(f"$b{i}", 0, 0x40) for i in range(nvec)]
                      + [(f"$d{i}", 0, 0x20) for i in range(nvec)]
                      + [(f"$e{i}", 0, 0x84) for i in range(nvec)]
                      + [(f"$t{i}", 0, 0x400) for i in range(nvec)],
                vectors=[(f"$n{i}",) for i in range(nvec)])


HOOKS = {
    # validate_script, sequential scripts (first scan Ss == 0 and Se == 0x3f): the num_scans guard, the
    # comps_in_scan bound, the component-index range and order checks, the four sequential progression
    # constants, the sent-twice check and the closing missing-data loop. Leaf, 18 vectors.
    "validate_script_seq": _vs_key("c3bo_vs_seq", 18),

    # validate_script, progressive scripts: the eight range checks on Ss / Se / Ah / Al, the DC-and-AC and
    # one-component-per-AC-scan rules, the prior-DC-scan check and both arms of the successive-approximation
    # test, plus the closing progressive loop. Leaf, 20 vectors (38 over both keys).
    "validate_script_prog": _vs_key("c3bo_vs_prog", 20),

    # jinit_c_master_control(cinfo, transcode_only): the scan_info arm (validate_script decides
    # progressive_mode) and the null arm, the progressive override of optimize_coding, all four
    # (transcode_only, optimize_coding) combinations and the doubled total_passes. 13 vectors; the compared
    # state includes everything initial_setup 0x004b52a0 writes into cinfo and comp_info.
    "jinit_c_master_control": dict(
        module="golf_clean.exe", addr=0x004B51E0, abi="default", ret="void", args=["pointer", "int"],
        fixture="c3bo_master",
        state=[(f"$c{i}", 0, 0x180) for i in range(13)]
              + [(f"$e{i}", 0, 0x84) for i in range(13)]
              + [(f"$p{i}", 0, 0x1100) for i in range(13)]
              + [(f"$k{i}", 0, _COMPS) for i in range(13)],
        vectors=[("$c0", 0), ("$c1", 1), ("$c2", 1), ("$c3", 0), ("$c4", 0), ("$c5", 0), ("$c6", 1),
                 ("$c7", 1), ("$c8", 0), ("$c9", 0), ("$c10", 1), ("$c11", 0), ("$c12", 0)]),

    # prepare_for_pass(cinfo): the three pass_type arms and the default ERREXIT, the huff_opt_pass escape
    # (Ss != 0 / Ah == 0 / arith_code) and its DC-refinement fallthrough, the optimize_coding setup skip, the
    # frame-header-once rule, the coefficient controller's buffer mode, both sides of is_last_pass and the
    # progress monitor. 16 vectors; the recorder $r<i> makes the ORDER of the nine dispatched methods and
    # every mode argument part of the compared state.
    "prepare_for_pass": dict(
        module="golf_clean.exe", addr=0x004B5810, abi="default", ret="void", args=["pointer"],
        fixture="c3bo_pass",
        state=[(f"$c{i}", 0, 0x180) for i in range(16)]
              + [(f"$m{i}", 0, 0x24) for i in range(16)]
              + [(f"$r{i}", 0, 0x100) for i in range(16)]
              + [(f"$e{i}", 0, 0x84) for i in range(16)]
              + [(f"$k{i}", 0, _COMPS) for i in range(16)]
              + [(f"$g{i}", 0, 0x20) for i in range(16)],
        vectors=[(f"$c{i}",) for i in range(16)]),

    # emit_eobrun, statistics mode off and a roomy output buffer: the EOBRUN == 0 return, the nbits counting
    # loop, the ehufsi == 0 ERREXIT, both inlined emit_bits copies including their byte loops and stuffed
    # zeros, and the correction-bit flush. 16 vectors.
    "emit_eobrun": _eob_key("c3bo_eob", 16),

    # emit_eobrun, statistics mode on: only count_ptrs[ac_tbl_no][nbits << 4] moves and both emit_bits copies
    # are skipped. 8 vectors over all four table numbers.
    "emit_eobrun_gather": _eob_key("c3bo_eob_gather", 8),

    # emit_eobrun with one to three free output bytes: each of the four dump_buffer 0x004b2420 call sites is
    # reached (0x004b26a2, 0x004b26cc, 0x004b2753, 0x004b277a). 6 vectors.
    "emit_eobrun_dump": _eob_key("c3bo_eob_dump", 6),
}

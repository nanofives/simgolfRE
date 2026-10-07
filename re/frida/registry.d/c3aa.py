# C3 batch c3aa (2026-10-07): IJG libjpeg 6a compress-side helpers of golf_clean.exe (subsystem util), none
# reached by the scenarios but each statically called. Reimplemented in shim/src/re/c3aa.cpp; fixtures in
# re/frida/js/fixtures.d/c3aa.js; the side of every jcc each vector takes is in log/c3/c3aa_purpose.md.
# The bit/byte emitters (emit_dqt/emit_sof/emit_dht) call emit_byte/emit_marker/emit_2bytes (0x004afbb0/
# 0x004afbf0/0x004afc10), which are NOT in this batch, so no SIMGOLF_HOOKS_OFF is needed for the original arm.

# emit_bits(state, code, size): working_state 0x24 bytes + 512-byte output; size in 1..16 (size 0 is the ERREXIT
# branch and is not exercised). Covers put_bits<8 (no byte) / >=8 (bytes) and the 0xFF stuff.
_EMIT_BITS = [("$state", c, s) for s in (1, 2, 4, 7, 8, 9, 12, 15, 16)
              for c in (0, 1, 0xFF, 0x1234, 0xFFFF, 0xA5)]

_WS_STATE = [("$state", 0, 0x24), ("$buf", 0, 512)]      # jchuff working_state + its output buffer
_PH_STATE = [("$obj", 0, 0x24), ("$buf", 0, 512)]        # jcphuff entropy record + its output buffer

# emit_buffered_bits(entropy, bufstart, nbits): low bit of each bufstart[k], nbits times.
_BUFFERED = [("$obj", src, n) for src in ("$bits_alt", "$bits_ones")
             for n in (0, 1, 2, 7, 8, 9, 16, 24)]

# jpeg_add_quant_table(cinfo, which_tbl, basic, scale_factor, force_baseline). The state is the quant table
# written (quant_tbl_ptrs[which_tbl]); both tables are in the state so either which_tbl is captured/restored.
_ADD_QUANT = [("$obj", w, "$basic", sc, fb) for w in (0, 1)
              for sc in (1, 25, 50, 100, 200, 800, 10000) for fb in (0, 1)]
_QUANT_STATE = [("$q0", 0, 0x84), ("$q1", 0, 0x84)]

_DEST_STATE = [("$dest", 0, 0x10), ("$buf", 0, 1024)]    # emit_byte advances next_output_byte / free_in_buffer
_DQT_STATE = _DEST_STATE + [("$q0", 0, 0x84), ("$q1", 0, 0x84)]   # + sent_table flips
_DHT_STATE = _DEST_STATE + [("$dc%d" % i, 0, 0x118) for i in range(4)] + [("$ac%d" % i, 0, 0x118) for i in range(4)]
# (index, is_ac) across the four DC and four AC tables -> eight distinct emitted streams.
_DHT = [("$obj", i, 0) for i in range(4)] + [("$obj", i, 1) for i in range(4)]
_SCAN_STATE = [("$obj", 0xe0, 0x70)]                     # comps_in_scan/cur_comp_info[]/Ss/Se/Ah/Al

HOOKS = {
    "emit_bits": dict(module="golf_clean.exe", addr=0x004B1A10, abi="default", ret="int",
                      args=["pointer", "uint32", "int"], fixture="c3aa_emit_bits", state=_WS_STATE,
                      vectors=_EMIT_BITS),

    "flush_bits_4b2510": dict(module="golf_clean.exe", addr=0x004B2510, abi="default", ret="void",
                              args=["pointer"], fixture="c3aa_phuff_flush", state=_PH_STATE, vectors=[("$obj",)]),
    "flush_bits_4b2510_ff": dict(module="golf_clean.exe", addr=0x004B2510, abi="default", ret="void",
                                 args=["pointer"], fixture="c3aa_phuff_flush_ff", state=_PH_STATE, vectors=[("$obj",)]),
    "flush_bits_4b2510_g1": dict(module="golf_clean.exe", addr=0x004B2510, abi="default", ret="void",
                                 args=["pointer"], fixture="c3aa_phuff_flush_g1", state=_PH_STATE, vectors=[("$obj",)]),
    "flush_bits_4b2510_nz": dict(module="golf_clean.exe", addr=0x004B2510, abi="default", ret="void",
                                 args=["pointer"], fixture="c3aa_phuff_flush_nz", state=_PH_STATE, vectors=[("$obj",)]),
    "flush_bits_4b2510_multi": dict(module="golf_clean.exe", addr=0x004B2510, abi="default", ret="void",
                                    args=["pointer"], fixture="c3aa_phuff_flush_multi", state=_PH_STATE, vectors=[("$obj",)]),

    "emit_buffered_bits": dict(module="golf_clean.exe", addr=0x004B27D0, abi="default", ret="void",
                               args=["pointer", "pointer", "uint32"], fixture="c3aa_phuff_buffered",
                               state=_PH_STATE, vectors=_BUFFERED),
    "emit_buffered_bits_g1": dict(module="golf_clean.exe", addr=0x004B27D0, abi="default", ret="void",
                                  args=["pointer", "pointer", "uint32"], fixture="c3aa_phuff_buffered_g1",
                                  state=_PH_STATE, vectors=[("$obj", "$bits_ones", 8)]),

    "jpeg_add_quant_table": dict(module="golf_clean.exe", addr=0x004AE600, abi="default", ret="void",
                                 args=["pointer", "int", "pointer", "int", "int"], fixture="c3aa_add_quant",
                                 state=_QUANT_STATE, vectors=_ADD_QUANT),

    "emit_dqt": dict(module="golf_clean.exe", addr=0x004AFEC0, abi="default", ret="int", args=["pointer", "int"],
                     fixture="c3aa_dqt", state=_DQT_STATE, vectors=[("$obj", 0), ("$obj", 1)]),
    "emit_dqt_sent": dict(module="golf_clean.exe", addr=0x004AFEC0, abi="default", ret="int", args=["pointer", "int"],
                          fixture="c3aa_dqt_sent", state=_DQT_STATE, vectors=[("$obj", 0), ("$obj", 1)]),

    "emit_sof": dict(module="golf_clean.exe", addr=0x004AFFA0, abi="default", ret="void", args=["pointer", "int"],
                     fixture="c3aa_sof", state=_DEST_STATE, vectors=[("$obj", 0xC0), ("$obj", 0xC1), ("$obj", 0xC2)]),
    "emit_sof_nc1": dict(module="golf_clean.exe", addr=0x004AFFA0, abi="default", ret="void", args=["pointer", "int"],
                         fixture="c3aa_sof_nc1", state=_DEST_STATE, vectors=[("$obj", 0xC0)]),

    "emit_dht": dict(module="golf_clean.exe", addr=0x004B0120, abi="default", ret="void",
                     args=["pointer", "int", "int"], fixture="c3aa_dht", state=_DHT_STATE, vectors=_DHT),
    "emit_dht_sent": dict(module="golf_clean.exe", addr=0x004B0120, abi="default", ret="void",
                          args=["pointer", "int", "int"], fixture="c3aa_dht_sent", state=_DHT_STATE, vectors=_DHT),

    "select_scan_parameters": dict(module="golf_clean.exe", addr=0x004B59D0, abi="default", ret="void",
                                   args=["pointer"], fixture="c3aa_scan_nc3", state=_SCAN_STATE, vectors=[("$obj",)]),
    "select_scan_parameters_nc1": dict(module="golf_clean.exe", addr=0x004B59D0, abi="default", ret="void",
                                       args=["pointer"], fixture="c3aa_scan_nc1", state=_SCAN_STATE, vectors=[("$obj",)]),
    "select_scan_parameters_nc2": dict(module="golf_clean.exe", addr=0x004B59D0, abi="default", ret="void",
                                       args=["pointer"], fixture="c3aa_scan_nc2", state=_SCAN_STATE, vectors=[("$obj",)]),
    "select_scan_parameters_nc4": dict(module="golf_clean.exe", addr=0x004B59D0, abi="default", ret="void",
                                       args=["pointer"], fixture="c3aa_scan_nc4", state=_SCAN_STATE, vectors=[("$obj",)]),
    "select_scan_parameters_s0c1": dict(module="golf_clean.exe", addr=0x004B59D0, abi="default", ret="void",
                                        args=["pointer"], fixture="c3aa_scan_s0c1", state=_SCAN_STATE, vectors=[("$obj",)]),
    "select_scan_parameters_s0c2": dict(module="golf_clean.exe", addr=0x004B59D0, abi="default", ret="void",
                                        args=["pointer"], fixture="c3aa_scan_s0c2", state=_SCAN_STATE, vectors=[("$obj",)]),
    "select_scan_parameters_s0c3": dict(module="golf_clean.exe", addr=0x004B59D0, abi="default", ret="void",
                                        args=["pointer"], fixture="c3aa_scan_s0c3", state=_SCAN_STATE, vectors=[("$obj",)]),
    "select_scan_parameters_s0c4": dict(module="golf_clean.exe", addr=0x004B59D0, abi="default", ret="void",
                                        args=["pointer"], fixture="c3aa_scan_s0c4", state=_SCAN_STATE, vectors=[("$obj",)]),
    "select_scan_parameters_s1c1": dict(module="golf_clean.exe", addr=0x004B59D0, abi="default", ret="void",
                                        args=["pointer"], fixture="c3aa_scan_s1c1", state=_SCAN_STATE, vectors=[("$obj",)]),
    "select_scan_parameters_s1c2": dict(module="golf_clean.exe", addr=0x004B59D0, abi="default", ret="void",
                                        args=["pointer"], fixture="c3aa_scan_s1c2", state=_SCAN_STATE, vectors=[("$obj",)]),
}

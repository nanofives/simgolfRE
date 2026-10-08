# Batch c3bn: sound.dll speech-codec functions (the two leaves of codecSynthStage 0x1003b230 and the pitch
# stage of codecStageDispatch 0x1003c640) and one sequencer function (module-aware hooks; addr is the RVA,
# VA = 0x10000000 + RVA). Reimplementations in shim/src/re/c3bn.cpp, fixtures in re/frida/js/fixtures.d/c3bn.js.
# Every entry only touches buffers, records, contexts and objects the fixture allocates: no live sound object,
# device, list or DirectSound interface is involved, no import is called and nothing is allocated or freed.
# One module global is read (never written) by lpcSynthesize: g_codecOrder 0x100b58a0, which the fixture sets
# to 10, the value its only writer codecSetParams 0x1003aa50 stores.
# All four functions return 0 on every path they can reach, so the evidence is in the state regions.
# Hooked callees (the verifier must switch these off on the main build, see log/c3/c3bn_notes.txt):
#   lpcSynthesize        -> codecCombFilter    sound.dll:40f60  (c3az)
#   codecPitchAnalysis   -> codecAmdfPitch     sound.dll:414f0  (c3bi)
#   Seq_remarkEventRange -> Seq_listGetAt      sound.dll:1d780  (c3au),
#                           Seq_scheduleEvent  sound.dll:24b80  (c3bi)

# --- 0x1003d8d0 reflectionToLpc: (reflection coefficients, LPC output, order, amplitude, gain).
_REFLECT = [
    ("$k_ramp", "$a0", "$o4", "$amp1", "$g0"),
    ("$k_third", "$a1", "$o4", "$amp1", "$g1"),
    ("$k_alt", "$a2", "$o8", "$amp1", "$g2"),
    ("$k_small", "$a3", "$o8", "$ampTiny", "$g3"),
    ("$k_big", "$a4", "$o6", "$ampBig", "$g4"),
    ("$k_zero", "$a5", "$o4", "$amp1", "$g5"),
    ("$k_ramp", "$a6", "$o0", "$amp1", "$g6"),      # both loops unentered
    ("$k_ramp", "$a7", "$o1", "$amp1", "$g7"),      # the recursion loop unentered
    ("$k_ramp", "$a8", "$o2", "$amp1", "$g8"),      # the recursion runs once
    ("$k_third", "$a9", "$o3", "$ampTiny", "$g9"),
    ("$k_alt", "$a10", "$o6", "$ampBig", "$g10"),
    ("$k_tiny", "$a11", "$o8", "$amp1", "$g11"),
    ("$k_big", "$a12", "$o8", "$ampTiny", "$g12"),
    ("$k_small", "$a13", "$o6", "$amp1234", "$g13"),
]

# --- 0x1003d1d0 lpcSynthesize: (coefficients, sub-frame length, excitation mode, output, level, second level,
# feedback gain, codec context).
_LPC = [
    ("$A_a", "$len10", "$mode1", "$out0", "$lvl_1", "$lvl2_10", "$fb_05", "$ctx_a"),
    ("$A_a", "$len10", "$mode1", "$out1", "$lvl_tiny", "$lvl2_10", "$fb_05", "$ctx_b"),   # ratio clamped to 8
    ("$A_b", "$len20", "$mode7", "$out2", "$lvl_1", "$lvl2_10", "$fb_09", "$ctx_c"),
    ("$A_b", "$len30", "$mode1", "$out3", "$lvl_1", "$lvl2_10", "$fb_05", "$ctx_d"),      # past the 0x19 table
    ("$A_zero", "$len10", "$mode1", "$out4", "$lvl_big", "$lvl2_10", "$fb_05", "$ctx_e"),
    (0, "$len0", "$mode1", 0, "$lvl_1", "$lvl2_10", "$fb_05", "$ctx_f"),                  # null arrays, order 0
    ("$A_a", "$len10", "$mode0", "$out5", "$lvl_1", "$lvl2_10", "$fb_05", "$ctx_a"),
    ("$A_a", "$len10", "$mode0", "$out6", "$lvl_1", "$lvl2_big", "$fb_05", "$ctx_b"),     # amplitude clamped
    ("$A_b", "$len20", "$mode0", "$out7", "$lvl_1", "$lvl2_small", "$fb_09", "$ctx_c"),
    ("$A_b", "$len4", "$mode0", "$out8", "$lvl_1", "$lvl2_10", "$fb_05", "$ctx_d"),
    ("$A_a", "$len1", "$mode0", "$out9", "$lvl_1", "$lvl2_10", "$fb_05", "$ctx_e"),
    ("$A_zero", "$len10", "$mode0", "$out10", "$lvl_tiny", "$lvl2_10", "$fb_05", "$ctx_f"),
    ("$A_a", "$len20", "$mode1", "$out11", "$lvl_mid", "$lvl2_10", "$fb_01", "$ctx_g"),
    ("$A_b", "$len10", "$mode0", "$out12", "$lvl_mid", "$lvl2_mid", "$fb_09", "$ctx_g"),
]

# --- 0x10040000 codecPitchAnalysis: (samples, span, lag table, lag count, scores, best index, second index,
# chosen lag).
_PITCH = [
    ("$s_p60", "$sp32", "$L_a", "$L_a_n", "$sc0", "$bi0", "$si0", "$lo0"),
    ("$s_p120", "$sp32", "$L_a", "$L_a_n", "$sc1", "$bi1", "$si1", "$lo1"),
    ("$s_p118", "$sp32", "$L_a", "$L_a_n", "$sc2", "$bi2", "$si2", "$lo2"),
    ("$s_p61", "$sp32", "$L_b", "$L_b_n", "$sc3", "$bi3", "$si3", "$lo3"),
    ("$s_p41", "$sp16", "$L_c", "$L_c_n", "$sc4", "$bi4", "$si4", "$lo4"),
    ("$s_flat", "$sp32", "$L_a", "$L_a_n", "$sc5", "$bi5", "$si5", "$lo5"),
    ("$s_saw", "$sp32", "$L_e", "$L_e_n", "$sc6", "$bi6", "$si6", "$lo6"),
    ("$s_p60", "$sp64", "$L_e", "$L_e_n", "$sc7", "$bi7", "$si7", "$lo7"),
    ("$s_p120", "$sp16", "$L_d", "$L_d_n", "$sc8", "$bi8", "$si8", "$lo8"),
    ("$s_p41", "$sp32", "$L_d", "$L_d_n", "$sc9", "$bi9", "$si9", "$lo9"),
    ("$s_p61", "$sp64", "$L_a", "$L_a_n", "$sc10", "$bi10", "$si10", "$lo10"),
    ("$s_saw", "$sp4", "$L_b", "$L_b_n", "$sc11", "$bi11", "$si11", "$lo11"),
    ("$s_flat", "$sp16", "$L_c", "$L_c_n", "$sc12", "$bi12", "$si12", "$lo12"),
    ("$s_p118", "$sp64", "$L_b", "$L_b_n", "$sc13", "$bi13", "$si13", "$lo13"),
]

# --- 0x10023610 Seq_remarkEventRange: (sequencer, event descriptor).
_REMARK = [
    ("$s_mark", "$d_bad"),                 # last below first: returns 0xa
    ("$s_mark_cnt3", "$d_over"),           # last not below the track count: returns 0xa
    ("$s_mark", "$d_0_3"),
    ("$s_mark", "$d_1_1"),
    ("$s_mark", "$d_0_7"),
    ("$s_mark_short", "$d_0_5"),           # the list runs out halfway through the range
    ("$s_sched", "$d_f1_0_3"),
    ("$s_sched", "$d_f4_0_3"),
    ("$s_sched", "$d_f10_2_4"),
    ("$s_sched", "$d_f0_0_2"),
    ("$s_sched_short", "$d_f1_0_5"),
    ("$s_sched", "$d_f1_ti2"),
    ("$s_sched_nocnt", "$d_f1_0_7"),       # every record's +0x458 is null
    ("$s_sched_step7", "$d_f4_3_6"),
]

HOOKS = {
    # 0x1003d8d0: the LPC output array and the gain cell carry the whole result (the function returns 0 on
    # every path). Orders 0 and 1 leave the recursion unentered, orders 2..8 run it. Leaf apart from the CRT's
    # sqrt, 14 vectors.
    "reflectionToLpc": dict(module="sound.dll", addr=0x0003D8D0, abi="default", ret="int",
                            args=["pointer", "pointer", "pointer", "pointer", "pointer"],
                            fixture="c3bn_reflection",
                            state=[("$a%d" % i, 0, 0x80) for i in range(14)]
                                  + [("$g%d" % i, 0, 4) for i in range(14)],
                            vectors=_REFLECT),
    # 0x1003d1d0: the codec context from +0x680 to +0xc00 (the previous length and level, the excitation array,
    # the synthesis history, the two delay lines and codecCombFilter's cursors and line) and the output buffer
    # are the state. Modes 0, 1 and 7 cover both sides of the `jne` at 0x1003d320; lvl_tiny takes the ratio
    # clamp at 0x1003d29f and lvl2_big the amplitude clamp at 0x1003d3dd; len30 takes the `jg` at 0x1003d489
    # past the 26-entry table; len0 with null arrays takes both `je` at 0x1003d1f2 and 0x1003d201.
    "lpcSynthesize": dict(module="sound.dll", addr=0x0003D1D0, abi="default", ret="int",
                          args=["pointer", "pointer", "pointer", "pointer", "pointer", "pointer",
                                "pointer", "pointer"],
                          fixture="c3bn_lpc",
                          state=[("$ctx_%s" % c, 0x680, 0x580) for c in "abcdefg"]
                                + [("$out%d" % i, 0, 0x100) for i in range(13)],
                          vectors=_LPC),
    # 0x10040000: the score array with its 0x20-float lead-in (the third-call improvement writes before the
    # array) and the three index cells are the state. L_c with a period-41 signal leaves the candidate loop
    # unentered, s_flat leaves the best index at 1 so the cursor reads the lead-in, s_p118 makes the second
    # call improve and s_p61 with L_b makes both the second and the third call improve.
    "codecPitchAnalysis": dict(module="sound.dll", addr=0x00040000, abi="default", ret="int",
                               args=["pointer", "pointer", "pointer", "pointer", "pointer", "pointer",
                                     "pointer", "pointer"],
                               fixture="c3bn_pitch",
                               state=[("$scb%d" % i, 0, (0x20 + 64) * 4) for i in range(14)]
                                     + [("$bi%d" % i, 0, 4) for i in range(14)]
                                     + [("$si%d" % i, 0, 4) for i in range(14)]
                                     + [("$lo%d" % i, 0, 4) for i in range(14)],
                               vectors=_PITCH),
    # 0x10023610: the two track arenas, their counter blocks and each sequencer's list cursor at +0x204 are the
    # state. The return value separates 0xa (two ways) from 0 (the two arms).
    "Seq_remarkEventRange": dict(module="sound.dll", addr=0x00023610, abi="thiscall", ret="int",
                                 args=["pointer", "pointer"], fixture="c3bn_remark",
                                 state=[("$arena", 0, 10 * 0x480), ("$counters", 0, 10 * 0x40),
                                        ("$arena2", 0, 10 * 0x480), ("$counters2", 0, 10 * 0x40)]
                                       + [(s, 0x1F0, 0x40) for s in
                                          ("$s_mark", "$s_mark_short", "$s_mark_cnt3", "$s_sched",
                                           "$s_sched_short", "$s_sched_nocnt", "$s_sched_step7")],
                                 vectors=_REMARK),
}

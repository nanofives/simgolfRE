# Batch c3bi: sound.dll speech-codec functions (five stage leaves of codecStageDispatch 0x1003c640 plus
# codecWindowFeatures, the analysis stage of 0x1003f720) and three sequencer / MIDI-device functions
# (module-aware hooks; addr is the RVA, VA = 0x10000000 + RVA). Reimplementations in shim/src/re/c3bi.cpp,
# fixtures in re/frida/js/fixtures.d/c3bi.js.
# Every entry only touches buffers, records and objects the fixture allocates: no live sound object, device,
# list or DirectSound interface is involved, no import is called and nothing is allocated or freed.
# Four of the nine functions are leaves (bitPermute, gaussSolveClamped, codecAmdfPitch, codecGainSmoother), so
# each of those carries >= 10 vectors (the gate's leaf rule). The other five have callees:
#   codecMarkCorrelationJumps -> floatCopySign  sound.dll:3e380  (hooked by c3au)
#   codecWindowFeatures       -> floatCopySign  sound.dll:3e380  (c3au), floatFloorToInt sound.dll:3e400 (c3az)
#   Seq_cmdHandler10          -> Seq_listGetAt  sound.dll:1d780  (hooked by c3au)
#   Seq_scheduleEvent         -> Seq_listGetAt  sound.dll:1d780  (c3au), Midi_seekTrackToTick :24920 (c3bd)
#   Midi_Device::setChannelVolume -> Voice_setLevel sound.dll:383b0 (hooked by c3au)
# Most functions return a constant (0, or 2 for setChannelVolume), so the evidence is in the state regions.

# --- 0x1003bf30 bitPermute: (mode, count, slot1, slot2, values, bits, context).
# Modes 0 and 2 are the pack case, mode 1 the unpack case (the `je` at 0x1003bf6a).
_BITS = [
    (0, "$n10", "$pa0", "$pb0", "$x_ramp", "$py0", "$ctx0"),
    (0, "$n10", "$pa1", "$pb1", "$x_max", "$py1", "$ctx0"),
    (2, "$n10", "$pa2", "$pb2", "$x_mix", "$py2", "$ctx1"),
    (0, "$n4", "$pa3", "$pb3", "$x_ramp", "$py3", "$ctx1"),
    (0, "$n0", "$pa4", "$pb4", "$x_zero", "$py4", "$ctx0"),
    (0, "$n10", "$pa5", "$pb5", "$x_neg", "$py5", "$ctx2"),
    (1, "$n10", "$pa6", "$pb6", "$px6", "$b_all1", "$ctx0"),
    (1, "$n10", "$pa7", "$pb7", "$px7", "$b_all0", "$ctx0"),
    (1, "$n10", "$pa8", "$pb8", "$px8", "$b_alt", "$ctx0"),
    (1, "$n10", "$pa9", "$pb9", "$px9", "$b_pat", "$ctx0"),
    (1, "$n4", "$pa10", "$pb10", "$px10", "$b_all1", "$ctx0"),
    (1, "$n1", "$pa11", "$pb11", "$px11", "$b_alt", "$ctx0"),
    (1, "$n0", "$pa12", "$pb12", "$px12", "$b_pat", "$ctx0"),
    (1, "$n10", "$pa13", "$pb13", "$px13", "$py13", "$ctx0"),
]

# --- 0x1003e6d0 gaussSolveClamped: (order, matrix, right-hand side, solution).
_GAUSS = [
    ("$o4", "$gm_spd", "$gr_small", "$gs0"),
    ("$o4", "$gm_spd", "$gr_big", "$gs1"),
    ("$o4", "$gm_spd", "$gr_negbig", "$gs2"),
    ("$o4", "$gm_neg", "$gr_small", "$gs3"),
    ("$o3", "$gm_spd", "$gr_odd", "$gs4"),
    ("$o2", "$gm_spd", "$gr_small", "$gs5"),
    ("$o1", "$gm_spd", "$gr_big", "$gs6"),
    ("$o4", "$gm_sing0", "$gr_small", "$gs7"),
    ("$o4", "$gm_sing3", "$gr_small", "$gs8"),
    ("$o2", "$gm_sing0", "$gr_odd", "$gs9"),
    ("$o4", "$gm_neg", "$gr_odd", "$gs10"),
    ("$o4", "$gm_mix", "$gr_huge", "$gs11"),
    ("$o3", "$gm_neg", "$gr_negbig", "$gs12"),
    ("$o4", "$gm_mix", "$gr_small", "$gs13"),
]

# --- 0x100414f0 codecAmdfPitch: (samples, span, lags, lag count, window, results, smallest, largest).
_AMDF = [
    ("$ax_sine", "$asp16", "$al4", "$anc4", "$awin64", "$ares0", "$amin0", "$amax0"),
    ("$ax_alt", "$asp16", "$al4", "$anc4", "$awin64", "$ares1", "$amin1", "$amax1"),
    ("$ax_zero", "$asp16", "$al4", "$anc4", "$awin64", "$ares2", "$amin2", "$amax2"),
    ("$ax_tiny", "$asp16", "$al4", "$anc4", "$awin64", "$ares3", "$amin3", "$amax3"),
    ("$ax_huge", "$asp16", "$al4", "$anc4", "$awin64", "$ares4", "$amin4", "$amax4"),
    ("$ax_sine", "$asp8", "$al3", "$anc3", "$awin64", "$ares5", "$amin5", "$amax5"),
    ("$ax_alt", "$asp4", "$al3", "$anc3", "$awin64", "$ares6", "$amin6", "$amax6"),
    ("$ax_sine", "$asp1", "$al4", "$anc4", "$awin64", "$ares7", "$amin7", "$amax7"),
    ("$ax_sine", "$asp0", "$al4", "$anc4", "$awin64", "$ares8", "$amin8", "$amax8"),
    ("$ax_alt", "$asp16", "$al1", "$anc1", "$awin64", "$ares9", "$amin9", "$amax9"),
    ("$ax_sine", "$asp16", "$al4", "$anc0", "$awin64", "$ares10", "$amin10", "$amax10"),
    ("$ax_huge", "$asp8", "$al0", "$anc3", "$awin64", "$ares11", "$amin11", "$amax11"),
    ("$ax_tiny", "$asp4", "$al0", "$anc3", "$awin64", "$ares12", "$amin12", "$amax12"),
    ("$ax_alt", "$asp8", "$al4", "$anc4", "$awin64", "$ares13", "$amin13", "$amax13"),
]

# --- 0x10040ab0 codecMarkCorrelationJumps: (samples, jump array, count, capacity, offset, last, window, ctx).
_JUMPS = [
    ("$ms_sine", "$mjidx0", "$mjcnt0", "$mj_cap8", "$mj_off4", "$mj_last40", "$mj_win32", "$mc_warm"),
    ("$ms_zero", "$mjidx1", "$mjcnt1", "$mj_cap8", "$mj_off4", "$mj_last40", "$mj_win32", "$mc_zero"),
    ("$ms_step", "$mjidx2", "$mjcnt2", "$mj_cap8", "$mj_off4", "$mj_last40", "$mj_win32", "$mc_ring"),
    ("$ms_alt", "$mjidx3", "$mjcnt3", "$mj_cap8", "$mj_off4", "$mj_last40", "$mj_win32", "$mc_lopsided"),
    ("$ms_tiny", "$mjidx4", "$mjcnt4", "$mj_cap8", "$mj_off4", "$mj_last40", "$mj_win32", "$mc_warm"),
    ("$ms_step", "$mjidx5", "$mjcnt5", "$mj_cap0", "$mj_off4", "$mj_last40", "$mj_win32", "$mc_ring"),
    ("$ms_sine", "$mjidx6", "$mjcnt6", "$mj_cap8", "$mj_off4", "$mj_last24", "$mj_win8", "$mc_open"),
    ("$ms_step", "$mjidx7", "$mjcnt7", "$mj_cap8", "$mj_off4", "$mj_last40", "$mj_win32", "$mc_open2"),
    ("$ms_alt", "$mjidx8", "$mjcnt8", "$mj_cap8", "$mj_off0", "$mj_last40", "$mj_win32", "$mc_warm"),
    ("$ms_sine", "$mjidx9", "$mjcnt9", "$mj_cap8", "$mj_off4", "$mj_last40", "$mj_win0", "$mc_warm"),
    (0, "$mjidx10", "$mjcnt10", "$mj_cap8", "$mj_off4", "$mj_last40", "$mj_win0", "$mc_zero"),
    ("$ms_zero", 0, "$mjcnt11", "$mj_cap8", "$mj_off4", "$mj_last24", "$mj_win8", "$mc_zero"),
    ("$ms_tiny", "$mjidx12", "$mjcnt12", "$mj_cap8", "$mj_off4", "$mj_last24", "$mj_win8", "$mc_lopsided"),
    ("$ms_step", "$mjidx13", "$mjcnt13", "$mj_cap8", "$mj_off4", "$mj_last24", "$mj_win8", "$mc_open"),
]

# --- 0x10041020 codecWindowFeatures: (range, first, second, descriptor, sub-frame, dither, lag, crossings,
# level1, level2, step ratio, lag product, corr at -lag, corr at +lag).
def _wf(i, rng, first, second, sub, lag):
    return (rng, first, second, "$wd_0", sub, "$wdit%d" % i, lag,
            "$wcr%d" % i, "$wl1%d" % i, "$wl2%d" % i,
            "$wsr%d" % i, "$wlp%d" % i, "$wcm%d" % i, "$wcp%d" % i)


_FEATURES = [
    _wf(0, "$wr_32", "$wf_sine", "$wf_alt", "$wsub1", "$wlag8"),
    _wf(1, "$wr_32", "$wf_alt", "$wf_sine", "$wsub1", "$wlag8"),
    _wf(2, "$wr_32", "$wf_zero", "$wf_zero", "$wsub1", "$wlag8"),
    _wf(3, "$wr_32", "$wf_huge", "$wf_sine", "$wsub1", "$wlag8"),
    _wf(4, "$wr_32", "$wf_tiny", "$wf_tiny", "$wsub1", "$wlag8"),
    _wf(5, "$wr_32", "$wf_ramp", "$wf_alt", "$wsub2", "$wlag8"),
    _wf(6, "$wr_32", "$wf_sine", "$wf_huge", "$wsub2", "$wlag4"),
    _wf(7, "$wr_32", "$wf_alt", "$wf_ramp", "$wsub1", "$wlag4"),
    _wf(8, "$wr_8", "$wf_sine", "$wf_alt", "$wsub1", "$wlag4"),
    _wf(9, "$wr_8", "$wf_zero", "$wf_sine", "$wsub2", "$wlag4"),
    _wf(10, "$wr_1", "$wf_sine", "$wf_alt", "$wsub1", "$wlag4"),
    _wf(11, "$wr_1", "$wf_huge", "$wf_huge", "$wsub1", "$wlag4"),
    _wf(12, "$wr_32", "$wf_tiny", "$wf_zero", "$wsub1", "$wlag8"),
]

# --- 0x1003f310 codecGainSmoother: (gains, count, index, mode, chosen out, index out, context).
_GAINS = [
    ("$gg_flat", "$gn8", "$gi1", "$gmode1", "$gco0", "$gio0", "$gc_hi"),
    ("$gg_flat", "$gn8", "$gi1", "$gmode0", "$gco1", "$gio1", "$gc_hi"),
    ("$gg_flat", "$gn8", "$gi5", "$gmode2", "$gco2", "$gio2", "$gc_hi"),
    ("$gg_ramp", "$gn8", "$gi1", "$gmode0", "$gco3", "$gio3", "$gc_lo"),
    ("$gg_ramp", "$gn20", "$gi5", "$gmode1", "$gco4", "$gio4", "$gc_lo"),
    ("$gg_dip", "$gn48", "$gi5", "$gmode1", "$gco5", "$gio5", "$gc_dip"),
    ("$gg_dip", "$gn48", "$gi1", "$gmode0", "$gco6", "$gio6", "$gc_dip"),
    ("$gg_dip", "$gn48", "$gi5", "$gmode2", "$gco7", "$gio7", "$gc_mix"),
    ("$gg_odd", "$gn20", "$gi1", "$gmode1", "$gco8", "$gio8", "$gc_mix"),
    ("$gg_odd", "$gn2", "$gi1", "$gmode0", "$gco9", "$gio9", "$gc_zero"),
    ("$gg_huge", "$gn1", "$gi1", "$gmode1", "$gco10", "$gio10", "$gc_zero"),
    ("$gg_huge", "$gn8", "$gi1", "$gmode0", "$gco11", "$gio11", "$gc_hi"),
    ("$gg_flat", "$gn1", "$gi1", "$gmode2", "$gco12", "$gio12", "$gc_lo"),
    ("$gg_dip", "$gn48", "$gi5", "$gmode1", "$gco13", "$gio13", "$gc_dip2"),
]

# --- 0x10023a60 Seq_cmdHandler10: (sequencer, tag, value).
_CMD10 = [
    ("$q_gate0", 1, 0x1111), ("$q_nochain", 1, 0x2222), ("$q_nopay", 1, 0x3333),
    ("$q_short", 9, 0x4444), ("$q_bad", 5, 0x5555), ("$q_overcount", 6, 0x6666),
    ("$q_ok", 7, 0x7777), ("$q_one", 8, 0x8888),
    ("$q_nopay2", 9, 0x9991), ("$q_deep", 2, 0x9992), ("$q_deep", 3, 0x9993),
    ("$q_long", 4, 0xAAAA), ("$q_zero", 9, 0xBBBB), ("$q_full", 10, 0xCCCC),
]

# --- 0x10024b80 Seq_scheduleEvent: (sequencer, track, tick, flag byte, track index).
_SCHEDULE = [
    ("$sq_a", "$st0", 100, 0x01, 0),
    ("$sq_a", "$st1", 100, 0x04, 0),
    ("$sq_a", "$st2", 100, 0x10, 0),
    ("$sq_a", "$st3", 100, 0x00, 0),
    ("$sq_a", "$st4", 100, 0x21, 0),
    ("$sq_a", "$st5", 37, 0x05, 0),
    ("$sq_a", "$st6", 37, 0x30, 0),
    ("$sq_a", "$st7", 0, 0x01, 0),
    ("$sq_a", "$st8", 100, 0x01, 0),
    ("$sq_a", "$st9", 100, 0x21, 0),
    ("$sq_a", "$st10", 100, 0x01, 1),
    ("$sq_a", "$st11", 100, 0x21, 2),
    ("$sq_a", "$st12", 100, 0x01, 9),
    ("$sq_b", "$st13", 100, 0x04, -3),
]

# --- 0x100213d0 Midi_Device::setChannelVolume: (device, controller event, channel, channel state).
_VOLUME = [
    ("$vdev_a", "$ve7_100", 0, "$vst_on"),
    ("$vdev_a", "$ve7_1", 0, "$vst_on"),
    ("$vdev_a", "$ve7_255", 0, "$vst_on"),
    ("$vdev_b", "$ve7_100", 3, "$vst_ch3"),
    ("$vdev_b", "$ve7_255", 15, "$vst_ch15"),
    ("$vdev_c", "$ve7_100", 0, "$vst_on"),
    ("$vdev_a", "$ve7_100", 0, "$vst_masked"),
    ("$vdev_a", "$ve7_100", 0, "$vst_other"),
    ("$vdev_a", "$ve7_100", 0, "$vst_empty"),
    ("$vdev_a", "$ve7_100", 0, "$vst_vt"),
    ("$vdev_a", "$ve10_64", 0, "$vst_on"),
    ("$vdev_a", "$ve10_64", 15, "$vst_ch15"),
    ("$vdev_a", "$ve1_90", 0, "$vst_on"),
    ("$vdev_a", "$ve127_5", 3, "$vst_ch3"),
]

HOOKS = {
    # 0x1003bf30: the pack case fills the bit array and flips the context toggle; the unpack case fills the two
    # slots and the value array. Both sides of the `je` at 0x1003bf6a and of the sign-extension test at
    # 0x1003c0fd are taken. Leaf, 14 vectors.
    "bitPermute": dict(module="sound.dll", addr=0x0003BF30, abi="default", ret="int",
                       args=["int", "pointer", "pointer", "pointer", "pointer", "pointer", "pointer"],
                       fixture="c3bi_bits",
                       state=[("$pa%d" % i, 0, 4) for i in range(14)]
                             + [("$pb%d" % i, 0, 4) for i in range(14)]
                             + [("$px%d" % i, 0, 0x40) for i in range(14)]
                             + [("$py%d" % i, 0, 0x100) for i in range(14)]
                             + [("$x_ramp", 0, 0x40), ("$x_max", 0, 0x40), ("$x_mix", 0, 0x40),
                                ("$x_zero", 0, 0x40), ("$x_neg", 0, 0x40),
                                ("$b_all1", 0, 0x100), ("$b_all0", 0, 0x100), ("$b_alt", 0, 0x100),
                                ("$b_pat", 0, 0x100),
                                ("$ctx0", 0x2540, 4), ("$ctx1", 0x2540, 4), ("$ctx2", 0x2540, 4)],
                       vectors=_BITS),
    # 0x1003e6d0: the solution buffers carry the whole result (the function returns 0 on every path). The
    # singular matrices take the pivot test at 0x1003e887 to its zero-fill side, gr_big and gr_negbig take both
    # clamps. Leaf, 14 vectors.
    "gaussSolveClamped": dict(module="sound.dll", addr=0x0003E6D0, abi="default", ret="int",
                              args=["pointer", "pointer", "pointer", "pointer"], fixture="c3bi_gauss",
                              state=[("$gs%d" % i, 0, 0x80) for i in range(14)],
                              vectors=_GAUSS),
    # 0x100414f0: the result buffers and the two index cells carry the whole result. Spans 16, 8, 4, 1 and 0
    # give the inner loop four trip counts and leave it unentered once; lag count 0 leaves the outer loop
    # unentered. Leaf, 14 vectors.
    "codecAmdfPitch": dict(module="sound.dll", addr=0x000414F0, abi="default", ret="int",
                           args=["pointer", "pointer", "pointer", "pointer", "pointer", "pointer",
                                 "pointer", "pointer"],
                           fixture="c3bi_amdf",
                           state=[("$ares%d" % i, 0, 0x200) for i in range(14)]
                                 + [("$amin%d" % i, 0, 4) for i in range(14)]
                                 + [("$amax%d" % i, 0, 4) for i in range(14)],
                           vectors=_AMDF),
    # 0x10040ab0: the context window +0x21c4..+0x2224, the jump index arrays and the counts are the state.
    # One vector passes a NULL sample pointer and one a NULL index array (the two `je` at 0x10040b38 and
    # 0x10040b47), both with a window of 0 or an index that is never appended.
    "codecMarkCorrelationJumps": dict(module="sound.dll", addr=0x00040AB0, abi="default", ret="int",
                                      args=["pointer", "pointer", "pointer", "pointer", "pointer",
                                            "pointer", "pointer", "pointer"],
                                      fixture="c3bi_jumps",
                                      state=[(c, 0x21C4, 0x60) for c in
                                             ("$mc_zero", "$mc_warm", "$mc_lopsided", "$mc_ring",
                                              "$mc_open", "$mc_open2")]
                                            + [("$mjidx%d" % i, 0, 0x80) for i in range(14)]
                                            + [("$mjcnt%d" % i, 0, 4) for i in range(14)],
                                      vectors=_JUMPS),
    # 0x10041020: the seven outputs per vector plus the dither cell the loop negates are the state. The
    # one-sample range wr_1 leaves the scan unentered, so every `< 1.0f` divisor clamp fires; wf_huge pushes
    # both levels past 0x7fff.
    "codecWindowFeatures": dict(module="sound.dll", addr=0x00041020, abi="default", ret="int",
                                args=["pointer", "pointer", "pointer", "pointer", "pointer", "pointer",
                                      "pointer", "pointer", "pointer", "pointer", "pointer", "pointer",
                                      "pointer", "pointer"],
                                fixture="c3bi_features",
                                state=[(k % i, 0, 4) for i in range(13)
                                       for k in ("$wdit%d", "$wcr%d", "$wl1%d", "$wl2%d",
                                                 "$wsr%d", "$wlp%d", "$wcm%d", "$wcp%d")],
                                vectors=_FEATURES),
    # 0x1003f310: the context window +0x2268..+0x2540 (costs, both back-pointer rows, the row number and the
    # level) and the two index outputs are the state. Modes 0, 1 and 2 cover the two `cmp` at 0x1003f36b and
    # 0x1003f3b6; gc_dip puts the chosen index above 20 so the backoff loop at 0x1003f650 fires. Leaf,
    # 14 vectors.
    "codecGainSmoother": dict(module="sound.dll", addr=0x0003F310, abi="default", ret="int",
                              args=["pointer", "pointer", "pointer", "pointer", "pointer", "pointer",
                                    "pointer"],
                              fixture="c3bi_gains",
                              state=[(c, 0x2268, 0x2D8) for c in
                                     ("$gc_hi", "$gc_lo", "$gc_dip", "$gc_dip2", "$gc_mix", "$gc_zero")]
                                    + [("$gco%d" % i, 0, 4) for i in range(14)]
                                    + [("$gio%d" % i, 0, 4) for i in range(14)],
                              vectors=_GAINS),
    # 0x10023a60: the sequencers' cursor at +0x220 and the shared 0x480-byte track arena are the state. The
    # return value separates 0x24 (four ways), 0xa (two ways) and 0 (the walk).
    "Seq_cmdHandler10": dict(module="sound.dll", addr=0x00023A60, abi="thiscall", ret="int",
                             args=["pointer", "uint32", "uint32"], fixture="c3bi_cmd10",
                             state=[("$arena", 0, 10 * 0x480)]
                                   + [(q, 0x218, 0x10) for q in
                                      ("$q_gate0", "$q_nochain", "$q_nopay", "$q_short", "$q_bad",
                                       "$q_overcount", "$q_ok", "$q_one", "$q_deep", "$q_long", "$q_nopay2",
                                       "$q_zero", "$q_full")],
                             vectors=_CMD10),
    # 0x10024b80: the function leaves no defined value in eax (the epilogue at 0x10024c0f does not write it),
    # so it is declared void and compared on memory alone: the track records and the sequencers' controller
    # block at +0x254, which Midi_seekTrackToTick writes on the bit-5 vectors.
    "Seq_scheduleEvent": dict(module="sound.dll", addr=0x00024B80, abi="thiscall", ret="void",
                              args=["pointer", "pointer", "uint32", "uint32", "int"],
                              fixture="c3bi_schedule",
                              state=[("$st%d" % i, 0, 0x60) for i in range(14)]
                                    + [("$sq_a", 0x254, 0x47C), ("$sq_b", 0x254, 0x47C)],
                              vectors=_SCHEDULE),
    # 0x100213d0: the devices' per-channel dwords, the group objects (their voice records are what
    # Voice_setLevel writes) and the 16-byte log the vtable slot +0x40 stub fills are the state. Controllers 7,
    # 0xa, 1 and 127 cover the three arms of the controller test; vst_masked and vst_other take the two early
    # returns, vst_vt the vtable arm and vst_empty the null-group arm.
    "Midi_Device::setChannelVolume": dict(module="sound.dll", addr=0x000213D0, abi="thiscall", ret="int",
                                          args=["pointer", "pointer", "uint32", "pointer"],
                                          fixture="c3bi_volume",
                                          state=[("$vdev_a", 0x254, 0x4E0), ("$vdev_b", 0x254, 0x4E0),
                                                 ("$vdev_c", 0x254, 0x4E0),
                                                 ("$vg_voices", 0x2A8, 0x1100),
                                                 ("$vg_all", 0x2A8, 0x1100),
                                                 ("$vg_none", 0x2A8, 0x1100),
                                                 ("$vg_vt", 0x2A8, 0x1100),
                                                 ("$log", 0, 0x10)],
                                          vectors=_VOLUME),
}

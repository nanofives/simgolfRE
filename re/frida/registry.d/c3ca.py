# Batch c3ca: sound.dll - the speech codec's last two unnamed analysis stages, the two codec context
# initialisers and five small voice / track / channel / DirectSound-buffer setters (module-aware hooks;
# addr is the RVA, VA = 0x10000000 + RVA). Reimplementations in shim/src/re/c3ca.cpp, fixtures in
# re/frida/js/fixtures.d/c3ca.js.
# Every entry only touches records, cells, arrays and codec contexts the fixture allocates: no live sound
# object, device, list, DirectSound interface or MIDI stream is involved, and nothing is allocated or freed.
# The two vtable slots the sequencer and DirectSound forwarders call belong to synthetic objects built by
# the fixture; both arms call the same NativeCallback, which records what it was handed.
#
# NAMES: 0x0003edc0 and 0x00040740 had no hooks.csv row when this batch was prepared. The proposed rows are
# in log/c3/c3ca_names.tsv (codecAlignAnalysisWindow and codecSelectWindowFromJumps); the keys below use
# those names, so the verifier must apply the rows before running c3_verify.
#
# ret="void" wherever the original leaves a value in eax that is not a return value (codecInitContextA/B,
# Voice_setPanGains, Channel_setPitchRate, Seq_applyChannelLevel, DsBuffer::setFrequency,
# DsBuffer::setVolumeRaw); for those the evidence is entirely in the state regions.
#
# HOOKED CALLEES (the verifier must re-run these keys on the main build with the hook off; on
# instances\c3ca only c3ca.cpp is linked, so the runs recorded there already used the true originals):
#   codecAlignAnalysisWindow -> floatFloorToInt 0x0003e400 (C3, c3az)
#   codecInitContextA / codecInitContextB -> codecSetParams 0x0003aa50 (C2, not hooked by any batch)
#   the other six have no callee at all.
#
# MODULE GLOBALS: codecInitContextA and codecInitContextB reach codecSetParams 0x1003aa50, the only writer
# in the DLL of g_codecOrder 0x100b58a0, the frame length 0x100b58a4 and g_codecMode 0x100b58a8, which it
# sets to the constants 10, 0xb4 and 1 (0x1003aa68, 0x1003aa72, 0x1003aa7c) - the values the live
# configuration already holds. Both arms write them, so they are not state regions. No other function of
# this batch reads or writes a module global.

# --- 0x1003edc0 codecAlignAnalysisWindow: nine pointers, all into one 0x200 block per vector.
_W_KEYS = ["allset", "f1zero", "f2zero", "f3zero", "f4zero", "any_mode0", "any_mode1", "any_mode2",
           "any_mode3", "none", "m2_sub", "m2_nosub", "m1_add", "m3_noadd", "slide_down", "slide_up",
           "step1", "step34", "sub0", "posthird", "possmall", "neghalf", "negsmall", "n2", "n4",
           "shortframe"]

_WINDOW = [("$w_step_%s" % k, "$w_flags_%s" % k, "$w_mode_%s" % k, "$w_pn_%s" % k, "$w_prev_%s" % k,
            "$w_cur_%s" % k, "$w_out_%s" % k, "$w_flen_%s" % k, "$w_wlen_%s" % k) for k in _W_KEYS]

# The whole block: the two bounds records the body writes, the step / mode / index / length cells and the
# flag and previous records it only reads, plus 0xb0 bytes of slack that nothing should touch.
_WINDOW_STATE = [("$w_blk_%s" % k, 0, 0x200) for k in _W_KEYS]

# --- 0x10040740 codecSelectWindowFromJumps: eleven pointers, all into one 0x200 block per vector.
_J_KEYS = ["none_empty", "none_high", "none_low", "none_eqstart", "m1_single", "m1_clamped", "m2_back",
           "m2_lo", "m3_pair", "m1_far", "m1_close", "m3_third", "m1_three", "m3_scan", "m3_jscan",
           "n2", "n4", "tinyframe", "past_run", "biggap"]

_JUMPS = [("$j_jumps_%s" % k, "$j_count_%s" % k, "$j_unused3_%s" % k, "$j_mode_%s" % k,
           "$j_bounds_%s" % k, "$j_pn_%s" % k, "$j_flen_%s" % k, "$j_gap_%s" % k, "$j_wlen_%s" % k,
           "$j_minstart_%s" % k, "$j_unused11_%s" % k) for k in _J_KEYS]

_JUMPS_STATE = [("$j_blk_%s" % k, 0, 0x200) for k in _J_KEYS]

# --- 0x1003aae0 / 0x1003aef0: one context per vector, each inside a block with 0x200 bytes of slack on
# each side; the whole block is the state, so the A/B also proves that nothing is written outside the
# context.
_CTXA = [("$a_ctx%d" % i,) for i in range(14)]
_CTXA_STATE = [("$a_blk%d" % i, 0, 0x2944) for i in range(14)]
_CTXB = [("$b_ctx%d" % i,) for i in range(14)]
_CTXB_STATE = [("$b_blk%d" % i, 0, 0x1000) for i in range(14)]

# --- 0x10037f30 Voice_setPanGains: (voice record, pan). Seven records x the pan values below.
_VOICE_OBJ = ["$v_a", "$v_b", "$v_c", "$v_d", "$v_e", "$v_f", "$v_g"]
_VOICE = ([("$v_a", p) for p in (-0x80000000, -0x41, -0x40, -0x20, -1, 0, 1, 0x20, 0x3F, 0x40, 0x7FFFFFFF)]
          + [("$v_b", p) for p in (-0x40, -1, 0, 1, 0x3F)]
          + [("$v_c", p) for p in (-0x40, 0, 0x3F)]
          + [("$v_d", p) for p in (-0x3F, 0, 7, 0x3F)]
          + [("$v_e", p) for p in (-0x3F, 0, 0x3F)]
          + [("$v_f", p) for p in (-0x40, -3, 0, 3, 0x3F)]
          + [("$v_g", p) for p in (-0x40, 0, 0x3F)])
_VOICE_STATE = [(o, 0, 0x100) for o in _VOICE_OBJ]

# --- 0x1001e530 Track_clearFlag8: a leaf with no stack argument, so one vector per record (12 of them,
# which is the gate's leaf minimum of 10 with room to spare).
_TRACK = [("$t%d" % i,) for i in range(12)]
_TRACK_STATE = [("$t%d" % i, 0, 0x60) for i in range(12)]

# --- 0x10035750 Channel_setPitchRate: a leaf, (channel record, value); 16 vectors.
_RATES = [0, 1, -1, 0x10000, -0x80000000, 0x7FFFFFFF, 0x1234, -5000]
_PITCH = [("$c%d" % (i % 4), _RATES[i % len(_RATES)]) for i in range(16)]
_PITCH_STATE = [("$c%d" % i, 0, 0x300) for i in range(4)]

# --- 0x1002b260 Seq_applyChannelLevel: (sequencer record, level). Five records whose +0x40 is null and
# five whose +0x40 is a synthetic inner object, so both sides of the null test run.
_SEQ = ([("$s_null%d" % i, v) for i, v in enumerate((0, 1, 0x7F, 0x80, -1))]
        + [("$s_dev%d" % i, v) for i, v in enumerate((0, 1, 0x7F, 0x80, -1))]
        + [("$s_dev0", 0x1FF), ("$s_dev1", -0x80000000), ("$s_null2", 0x7FFFFFFF),
           ("$s_dev3", 0x40), ("$s_null4", 0x40)])
_SEQ_STATE = ([("$s_null%d" % i, 0, 0x80) for i in range(5)]
              + [("$s_dev%d" % i, 0, 0x80) for i in range(5)]
              + [("$s_inner%d" % i, 0, 0x100) for i in range(5)])

# --- 0x1000f160 DsBuffer::setFrequency and 0x1000f190 DsBuffer::setVolumeRaw: (buffer record, value).
# Four records with a null interface and four with a synthetic one.
_DS_VALUES = (0, 1, 22050, 44100, -1, -10000, 0x7FFFFFFF, -0x80000000)
_FREQ = ([("$f_null%d" % (i % 4), v) for i, v in enumerate(_DS_VALUES)]
         + [("$f_dev%d" % (i % 4), v) for i, v in enumerate(_DS_VALUES)])
_FREQ_STATE = ([("$f_null%d" % i, 0, 0x100) for i in range(4)]
               + [("$f_dev%d" % i, 0, 0x100) for i in range(4)]
               + [("$f_iface%d" % i, 0, 0x40) for i in range(4)])
_VOL = ([("$g_null%d" % (i % 4), v) for i, v in enumerate(_DS_VALUES)]
        + [("$g_dev%d" % (i % 4), v) for i, v in enumerate(_DS_VALUES)])
_VOL_STATE = ([("$g_null%d" % i, 0, 0x100) for i in range(4)]
              + [("$g_dev%d" % i, 0, 0x100) for i in range(4)]
              + [("$g_iface%d" % i, 0, 0x40) for i in range(4)])

HOOKS = {
    # 0x1003edc0: 26 vectors. Four take the chain of flag tests apart one flag at a time, four drive the
    # output arms (copy + mode 2, copy + other mode, align, and the two ways into the first arm), four the
    # mode corrections, two the sliding loops, four the rounding of floatFloorToInt at +/-0.5 and +/-0.4,
    # two the frame indices 2 and 4 and two the extreme steps 1 and 34.
    "codecAlignAnalysisWindow": dict(module="sound.dll", addr=0x0003EDC0, abi="default", ret="int",
                                     args=["pointer"] * 9, fixture="c3ca_window",
                                     state=_WINDOW_STATE, vectors=_WINDOW),
    # 0x10040740: 20 vectors, at least one per outcome code (0, 1, 2, 3) and one per side of every
    # conditional jump. `past_run` is the one that pins the bound of the last loop to the end of the run:
    # the entry in the slot after it would give mode 3 under any wider bound and the original gives mode 1.
    "codecSelectWindowFromJumps": dict(module="sound.dll", addr=0x00040740, abi="default", ret="int",
                                       args=["pointer"] * 11, fixture="c3ca_jumps",
                                       state=_JUMPS_STATE, vectors=_JUMPS),
    # 0x1003aae0: fourteen fully seeded 0x2544-byte encoder contexts. The body overwrites all of it except
    # +0x20b4..+0x20db, +0x20ec..+0x20fb, +0x2104..+0x2113, +0x21cc and +0x221c, which the fixture seeds
    # differently per vector; that is what makes the fourteen final states distinct.
    "codecInitContextA": dict(module="sound.dll", addr=0x0003AAE0, abi="default", ret="void",
                              args=["pointer"], fixture="c3ca_ctxa",
                              state=_CTXA_STATE, vectors=_CTXA),
    # 0x1003aef0: the same for the fourteen 0xc00-byte decoder contexts; the regions it never writes are
    # +0x64c, +0x650, +0x658..+0x683 and +0xbea..+0xbeb.
    "codecInitContextB": dict(module="sound.dll", addr=0x0003AEF0, abi="default", ret="void",
                              args=["pointer"], fixture="c3ca_ctxb",
                              state=_CTXB_STATE, vectors=_CTXB),
    # 0x10037f30: 34 vectors over seven voice records. The pans cover both clamps and their boundaries and
    # zero; the records make each int output cross 0x10000 and each double output cross 1.0 on both sides.
    "Voice_setPanGains": dict(module="sound.dll", addr=0x00037F30, abi="thiscall", ret="void",
                              args=["pointer", "int"], fixture="c3ca_voice",
                              state=_VOICE_STATE, vectors=_VOICE),
    # 0x1001e530: a leaf with no stack argument; twelve flag words, with and without bit 3 and with high
    # bytes the `and al, 0xf7` must carry through.
    "Track_clearFlag8": dict(module="sound.dll", addr=0x0001E530, abi="thiscall", ret="int",
                             args=["pointer"], fixture="c3ca_track",
                             state=_TRACK_STATE, vectors=_TRACK),
    # 0x10035750: a leaf; sixteen (record, value) pairs over four records.
    "Channel_setPitchRate": dict(module="sound.dll", addr=0x00035750, abi="thiscall", ret="void",
                                 args=["pointer", "int"], fixture="c3ca_channel",
                                 state=_PITCH_STATE, vectors=_PITCH),
    # 0x1002b260: fifteen vectors, eight through the null arm and seven through the synthetic inner object
    # whose vtable slot 0x40 records the value it was called with.
    "Seq_applyChannelLevel": dict(module="sound.dll", addr=0x0002B260, abi="thiscall", ret="void",
                                  args=["pointer", "int"], fixture="c3ca_seq",
                                  state=_SEQ_STATE, vectors=_SEQ),
    # 0x1000f160: sixteen vectors, eight with a null interface (the body then does nothing at all) and
    # eight through the synthetic interface whose vtable slot 0x44 records the value.
    "DsBuffer::setFrequency": dict(module="sound.dll", addr=0x0000F160, abi="thiscall", ret="void",
                                   args=["pointer", "int"], fixture="c3ca_dsfreq",
                                   state=_FREQ_STATE, vectors=_FREQ),
    # 0x1000f190: the same shape, but the value is always cached at +0xc4 before the null test and the
    # vtable slot is 0x3c.
    "DsBuffer::setVolumeRaw": dict(module="sound.dll", addr=0x0000F190, abi="thiscall", ret="void",
                                   args=["pointer", "int"], fixture="c3ca_dsvol",
                                   state=_VOL_STATE, vectors=_VOL),
}

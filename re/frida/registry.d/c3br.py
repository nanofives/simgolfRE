# Batch c3br: sound.dll speech-codec parameter stages (module-aware hooks; addr is the RVA,
# VA = 0x10000000 + RVA). Reimplementations in shim/src/re/c3br.cpp, fixtures in re/frida/js/fixtures.d/c3br.js.
# Every entry only touches cells, arrays and codec contexts the fixture allocates: no live sound object, device,
# list or DirectSound buffer is involved, no import is called and nothing is allocated or freed.
# Three module globals are READ (never written) by these functions: g_codecOrder 0x100b58a0, the frame length
# 0x100b58a4 and the mode flag 0x100b58a8. Each has exactly one writing instruction in the whole DLL, all three
# inside codecSetParams 0x1003aa50 (0x1003aa68 stores 10, 0x1003aa72 stores 0xb4, 0x1003aa7c stores 1); the
# fixtures store those same values. quantizeParams is the one function whose behaviour depends on the mode flag,
# so it gets one entry per setting (0 = the value before codecSetParams has run, 1 = after).
# Every function returns 0 on every path it can reach, so the evidence is in the state regions.

# --- 0x1003c220 quantizeParams: (pair, index cell, value cell, coefficients, out cell, exponent cell, out array).
_QUANT = [
    ("$p_3_4", "$ix0", "$v500", "$c_rampA", "$oc0", "$ec0", "$q0"),
    ("$p_0_0", "$ix0", "$v1023", "$c_rampB", "$oc1", "$ec1", "$q1"),
    ("$p_0_5", "$ix1", "$v5000", "$c_rampC", "$oc2", "$ec2", "$q2"),
    ("$p_7_0", "$ix2", "$vm17", "$c_rampA", "$oc3", "$ec3", "$q3"),
    ("$p_1_1", "$ix3", "$v1", "$c_small", "$oc4", "$ec4", "$q4"),
    ("$p_0_0", "$ix4", "$v0", "$c_big", "$oc5", "$ec5", "$q5"),
    ("$p_2_9", "$ix5", "$v1024", "$c_neg", "$oc6", "$ec6", "$q6"),
    ("$p_0_3", "$ix6", "$v936", "$c_alt", "$oc7", "$ec7", "$q7"),
    ("$p_5_5", "$ix7", "$v33", "$c_third", "$oc8", "$ec8", "$q8"),
    ("$p_0_0", "$ix8", "$vtiny", "$c_tiny", "$oc9", "$ec9", "$q9"),
    ("$p_6_2", "$ix9", "$vhuge", "$c_rampB", "$oc10", "$ec10", "$q10"),
    ("$p_0_7", "$ix10", "$v250", "$c_bigNeg", "$oc11", "$ec11", "$q11"),
    ("$p_4_0", "$ix11", "$v64", "$c_rampC", "$oc12", "$ec12", "$q12"),
    ("$p_0_0", "$ix12", "$vm1000", "$c_zero", "$oc13", "$ec13", "$q13"),
]

_QUANT_STATE = ([("$q%d" % i, 0, 16 * 4) for i in range(14)]
                + [("$oc%d" % i, 0, 4) for i in range(14)]
                + [("$ec%d" % i, 0, 4) for i in range(14)])

# --- 0x1003b5a0 codecTransformStage: (class cell, level cell, coefficient array, flag block, gain cell, float
# cell, float coefficient array, stage record). Like quantizeParams the body splits on the mode flag
# 0x100b58a8 (`jne 0x1003b6ee` at 0x1003b655), so there is one entry per setting of it.
_TRANSFORM = [("$tcls%d" % i, "$tlvl%d" % i, "$tco%d" % i, "$tfp%d" % i, "$tgn%d" % i, "$tlf%d" % i,
               "$tcf%d" % i, "$trec%d" % i) for i in range(14)]

_TRANSFORM_STATE = ([("$trec%d" % i, 0, 0xA8) for i in range(14)]
                    + [("$tlvl%d" % i, 0, 4) for i in range(14)]
                    + [("$tco%d" % i, 0, 16 * 4) for i in range(14)]
                    + [("$tfp%d" % i, 0, 8) for i in range(14)]
                    + [("$tgn%d" % i, 0, 4) for i in range(14)]
                    + [("$tlf%d" % i, 0, 4) for i in range(14)]
                    + [("$tcf%d" % i, 0, 16 * 4) for i in range(14)])

# --- 0x1003da50 interpSubframeParams: (order cell, flag block, sub-frame length cell, gain cell, coefficients,
# frame length cell, mode array, length array, gain array, coefficient blocks, entry count cell, float out cell,
# codec context). Vector 4 is the one that passes 0 for the five pointers the prologue tests
# (0x1003da72, 0x1003da81, 0x1003dab5, 0x1003dac4, 0x1003dad3); it is also the only one with an order of 0 and a
# frame length of 0, which is what keeps every loop that would dereference them empty.
_INTERP = [tuple(["$ord10", "$fb_%s" % f, "$isub%d" % i, "$igin%d" % i, "$ikin%d" % i, "$fl180",
                  "$imode%d" % i, "$ilen%d" % i, "$igain%d" % i, "$ikout%d" % i,
                  "$icount%d" % i, "$iratio%d" % i, "$ictx%d" % i])
           for i, f in enumerate(["0_0", "1_3", "2_1", "XX", "0_0", "0_0", "4_4",
                                  "0_5", "0_0", "1_0", "7_0", "4_4", "0_0", "6_2"])]
_INTERP[3] = ("$ord0", "$fb_5_5", "$isub3", "$igin3", 0, "$fl0", 0, 0, 0, 0,
              "$icount3", "$iratio3", "$ictx3")

_INTERP_STATE = ([("$ictx%d" % i, 0x64C, 0x3C) for i in range(14)]
                 + [("$ikin%d" % i, 0, 16 * 4) for i in range(14) if i != 3]
                 + [("$isub%d" % i, 0, 4) for i in range(14)]
                 + [("$igin%d" % i, 0, 4) for i in range(14)]
                 + [("$icount%d" % i, 0, 4) for i in range(14)]
                 + [("$iratio%d" % i, 0, 4) for i in range(14)]
                 + [("$imode%d" % i, 0, 64 * 4) for i in range(14) if i != 3]
                 + [("$ilen%d" % i, 0, 64 * 4) for i in range(14) if i != 3]
                 + [("$igain%d" % i, 0, 64 * 4) for i in range(14) if i != 3]
                 + [("$ikout%d" % i, 0, 512 * 4) for i in range(14) if i != 3])

# --- 0x1003b230 codecSynthStage: (flag block, sub-frame length cell, gain cell, coefficients, output array,
# out cell, codec context). Vector 12 is the one whose context makes interpSubframeParams produce no entry at
# all (its carry of -170 leaves 10 samples to cover against a 100-sample ramp), which is the only way the
# output array is never touched, so it is also the vector that passes 0 for it (`je 0x1003b27b` at 0x1003b270).
_SYNTH = [tuple(["$sfb_%s" % f, "$ssub%d" % i, "$sgin%d" % i, "$skin%d" % i, "$sout%d" % i,
                 "$scount%d" % i, "$sctx%d" % i])
          for i, f in enumerate(["0_0", "1_3", "2_1", "0_5", "0_0", "0_0", "4_4",
                                 "0_5", "0_0", "1_0", "7_0", "4_4", "0_0", "0_0"])]
_SYNTH[11] = ("$sfb_4_4", "$ssub11", "$sgin11", "$skin11", 0, "$scount11", "$sctx11")

_SYNTH_STATE = ([("$sctx%d" % i, 0xA8, 0xB60) for i in range(14)]
                + [("$ssub%d" % i, 0, 4) for i in range(14)]
                + [("$skin%d" % i, 0, 16 * 4) for i in range(14)]
                + [("$scount%d" % i, 0, 4) for i in range(14)]
                + [("$sout%d" % i, 0, 192 * 4) for i in range(14) if i != 11])

HOOKS = {
    # 0x1003c220: the out array, the out cell and the exponent cell carry the whole result (the function returns
    # 0 on every path). The mode flag 0x100b58a8 decides three branches (0x1003c2d0, 0x1003c55d and, through the
    # out cell, 0x1003c569 and 0x1003c571), so there is one entry per setting of it.
    "quantizeParams_mode0": dict(module="sound.dll", addr=0x0003C220, abi="default", ret="int",
                                 args=["pointer", "pointer", "pointer", "pointer", "pointer", "pointer",
                                       "pointer"],
                                 fixture="c3br_quant_mode0", state=_QUANT_STATE, vectors=_QUANT),
    "quantizeParams_mode1": dict(module="sound.dll", addr=0x0003C220, abi="default", ret="int",
                                 args=["pointer", "pointer", "pointer", "pointer", "pointer", "pointer",
                                       "pointer"],
                                 fixture="c3br_quant_mode1", state=_QUANT_STATE, vectors=_QUANT),
    # 0x1003da50: the six context fields the body writes (+0x64c previous flag, +0x650 previous length,
    # +0x654 previous gain, +0x658 the ten previous coefficients, +0x680 the carry, +0x684 the first-block flag),
    # the in/out cells and coefficient array and the four per-entry output arrays are the state; the function
    # returns 0 on every path. Four vectors take the first-block path (0x1003db82), the other ten the three
    # shapes of the main path, two of which (vectors 10 and 11) run the ramp loop twice.
    "interpSubframeParams": dict(module="sound.dll", addr=0x0003DA50, abi="default", ret="int",
                                 args=["pointer"] * 13, fixture="c3br_interp",
                                 state=_INTERP_STATE, vectors=_INTERP),
    # 0x1003b5a0: the stage record (its six cells, the three-wide coefficient history and the gain and level
    # histories), the level cell, both coefficient arrays, the flag block, the gain cell and the float cell are
    # the state; the function returns 0 on every path. Its two callees medianOfThree 0x1003e480 (c3az) and
    # decodeTableSymbol 0x1003e520 (c3bd) are hooked, as is intPow 0x1003e2c0 (c3az), so the evidence run is
    # the one made with the three switched off.
    "codecTransformStage_mode0": dict(module="sound.dll", addr=0x0003B5A0, abi="default", ret="int",
                                      args=["pointer"] * 8, fixture="c3br_transform_mode0",
                                      state=_TRANSFORM_STATE, vectors=_TRANSFORM),
    "codecTransformStage_mode1": dict(module="sound.dll", addr=0x0003B5A0, abi="default", ret="int",
                                      args=["pointer"] * 8, fixture="c3br_transform_mode1",
                                      state=_TRANSFORM_STATE, vectors=_TRANSFORM),
    # 0x1003b230: the codec context from +0xa8 to +0xc08 (the synthesis ring, its cursor, interpSubframeParams'
    # six fields, lpcSynthesize's arrays and delay lines, codecCombFilter's cursors and line and applyIir2's
    # five states), the clamped sub-frame length cell, the clamped coefficient array, the out cell and the
    # output array are the state; the function returns 0 on every path. Its four callees are all hooked
    # elsewhere (interpSubframeParams in this batch, reflectionToLpc and lpcSynthesize in c3bn, applyIir2 in
    # c3bd), so the evidence run is the one made with all four switched off.
    "codecSynthStage": dict(module="sound.dll", addr=0x0003B230, abi="default", ret="int",
                            args=["pointer"] * 7, fixture="c3br_synth",
                            state=_SYNTH_STATE, vectors=_SYNTH),
}

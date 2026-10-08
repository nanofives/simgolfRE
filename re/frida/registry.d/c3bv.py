# Batch c3bv: sound.dll speech-codec stage drivers (module-aware hooks; addr is the RVA,
# VA = 0x10000000 + RVA). Reimplementations in shim/src/re/c3bv.cpp, fixtures in re/frida/js/fixtures.d/c3bv.js.
# Every entry only touches buffers, codec contexts and cells the fixture allocates: no live sound object,
# device, list or DirectSound buffer is involved, no import is called and nothing is allocated or freed.
# Two module globals are READ (never written) by these functions: g_codecOrder 0x100b58a0 and the frame length
# 0x100b58a4, each with exactly one writing instruction in the whole DLL, both inside codecSetParams
# 0x1003aa50 (0x1003aa68 stores 10, 0x1003aa72 stores 0xb4); the fixtures store those same values.
# All four functions return 0 on every path they can reach, so the evidence is in the state regions.
#
# HOOKED CALLEES (every one of them is at C3 from an earlier batch, so the verifier re-runs each key with
# SIMGOLF_HOOKS_OFF; on instances\c3bv only c3bv.cpp is linked, so the runs recorded there already used the
# true originals):
#   codecProcessBlock    -> bitUnpack 3c1d0 (c3au), codecTransformStage 3b5a0 (c3br), codecSynthStage 3b230 (c3br)
#   codecProcessResource -> codecStageDispatch 3c640 (THIS batch), quantizeParams 3c220 (c3br),
#                           bitPack 3c180 (c3au)
#   codecFeatureDecision -> codecWindowFeatures 41020 (c3bi), floatFloorToInt 3e400 (c3az)
#   codecStageDispatch   -> codecMarkCorrelationJumps 40ab0 (c3bi), codecPitchAnalysis 40000 (c3bn),
#                           codecFeatureDecision 3f720 (THIS batch), codecGainSmoother 3f310 (c3bi),
#                           gaussSolveClamped 3e6d0 (c3bi)

_BLOCK_KEYS = list(range(14))
_STAGE_KEYS = list("abcdefghijklmn")

# --- 0x1003a980 codecProcessBlock: (packed words, output samples, decoder context).
_BLOCK = [("$bpk%d" % i, "$bout%d" % i, "$bctx%d" % i) for i in _BLOCK_KEYS]

# The decoder context from 0 to 0xc08 (codecTransformStage's stage record at 0..0xa8 and codecSynthStage's
# ring, cursor, interpSubframeParams' six fields, lpcSynthesize's arrays and delay lines, codecCombFilter's
# cursors and applyIir2's states at +0xa8..+0xc08) and the output array are what the function changes. The
# packed words are included to show that the unpack path only reads them.
_BLOCK_STATE = ([("$bctx%d" % i, 0, 0xC08) for i in _BLOCK_KEYS]
                + [("$bout%d" % i, 0, 512 * 4) for i in _BLOCK_KEYS]
                + [("$bpk%d" % i, 0, 53 * 4) for i in _BLOCK_KEYS])

# --- 0x1003f720 codecFeatureDecision: thirteen arguments; the twelfth is never read by the body
# (no `[ebp+0x34]` anywhere in it), so every vector passes 0 for it.
_DECIDE_KEYS = ["h1_big", "h1_divclamp", "h1_band3", "h1_band5",
                "c0", "c1", "c2", "c2b", "c3", "c4", "c5", "c5b", "c6", "c6b", "c6c", "c7", "c7b", "c8",
                "c9", "c10", "c10b", "c11", "c12", "c13", "c13b", "c14", "c15",
                "c2c", "c6d", "c8b", "c13c", "c14b", "c14c", "cdef1", "cbig", "capped", "hilevel"]
_DECIDE = [("$drange_%s" % k, "$dctx_%s" % k, "$dctx_%s" % k, "$d_desc", "$dhalf_%s" % k, "$ddiv_%s" % k,
            "$dnum_%s" % k, "$dlag_%s" % k, "$dblk_%s" % k, "$dinf_%s" % k, "$dofl_%s" % k, 0,
            "$dctx_%s" % k)
           for k in _DECIDE_KEYS]

# The whole encoder context is the state: the body writes +0x2224..+0x2267 itself and codecWindowFeatures
# writes the dither cell +0x2224 through it, and taking the whole object also proves the two sample histories
# and every other field are left alone. The output flag block is the second region.
_DECIDE_STATE = ([("$dblk_ctx_%s" % k, 0, 0x3600) for k in _DECIDE_KEYS]
                 + [("$dofl_%s" % k, 0, 16 * 4) for k in _DECIDE_KEYS])

# --- 0x1003c640 codecStageDispatch: (input samples, flag pair out, index cell out, float level cell out,
# float coefficient array out, encoder context).
_STAGE = [("$samples_s%s" % k, "$pair_s%s" % k, "$idx_s%s" % k, "$lvl_s%s" % k, "$coeff_s%s" % k,
           "$ctx_s%s" % k) for k in _STAGE_KEYS]

_STAGE_STATE = ([("$blk_s%s" % k, 0, 0x3600) for k in _STAGE_KEYS]
                + [("$sblk_s%s" % k, 0, 0x800 + 1024 * 4 + 0x800) for k in _STAGE_KEYS]
                + [("$pair_s%s" % k, 0, 8) for k in _STAGE_KEYS]
                + [("$idx_s%s" % k, 0, 4) for k in _STAGE_KEYS]
                + [("$lvl_s%s" % k, 0, 4) for k in _STAGE_KEYS]
                + [("$coeff_s%s" % k, 0, 16 * 4) for k in _STAGE_KEYS])

# --- 0x1003b150 codecProcessResource: (input samples, packed words out, encoder context).
_RESOURCE = [("$samples_r%s" % k, "$bits_r%s" % k, "$ctx_r%s" % k) for k in _STAGE_KEYS]

_RESOURCE_STATE = ([("$blk_r%s" % k, 0, 0x3600) for k in _STAGE_KEYS]
                   + [("$sblk_r%s" % k, 0, 0x800 + 1024 * 4 + 0x800) for k in _STAGE_KEYS]
                   + [("$bits_r%s" % k, 0, 64 * 4) for k in _STAGE_KEYS])

HOOKS = {
    # 0x1003a980: the decoder pipeline. Fourteen packed-word sets built from bitPermute's own interleaving
    # table, so the class cell, the level cell and the ten quantised coefficients take named values.
    "codecProcessBlock": dict(module="sound.dll", addr=0x0003A980, abi="default", ret="int",
                              args=["pointer", "pointer", "pointer"], fixture="c3bv_block",
                              state=_BLOCK_STATE, vectors=_BLOCK),
    # 0x1003f720: thirty vectors. Four run the first half (the score-history shift and the feature-1
    # division), the other twenty-six the second half, one per value of the four-bit flag code plus the two
    # that leave the switch's range.
    "codecFeatureDecision": dict(module="sound.dll", addr=0x0003F720, abi="default", ret="int",
                                 args=["pointer"] * 11 + ["int", "pointer"], fixture="c3bv_decide",
                                 state=_DECIDE_STATE, vectors=_DECIDE),
    # 0x1003c640: the analyser. Fourteen encoder contexts and sample frames, covering voiced, noisy, silent,
    # clipped and constant input and jump lists of 0 to 10 entries.
    "codecStageDispatch": dict(module="sound.dll", addr=0x0003C640, abi="default", ret="int",
                               args=["pointer"] * 6, fixture="c3bv_stage",
                               state=_STAGE_STATE, vectors=_STAGE),
    # 0x1003b150: the encoder pipeline, over the same fourteen contexts and sample frames.
    "codecProcessResource": dict(module="sound.dll", addr=0x0003B150, abi="default", ret="int",
                                 args=["pointer", "pointer", "pointer"], fixture="c3bv_resource",
                                 state=_RESOURCE_STATE, vectors=_RESOURCE),
}

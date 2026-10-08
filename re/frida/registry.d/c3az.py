# Batch c3az: sound.dll codec arithmetic leaves, channel voice sweeps and pan maths, the DirectSound buffer
# description switch and three sequencer descriptor helpers (module-aware hooks; addr is the RVA,
# VA = 0x10000000 + RVA). Reimplementations in shim/src/re/c3az.cpp, fixtures in re/frida/js/fixtures.d/c3az.js.
# Every entry only touches records the fixture allocates: no live sound object, device, list or DirectSound
# interface is involved and no import is called.
# Nine of the fifteen functions are leaves, so each of those carries >= 10 vectors (the gate's leaf rule);
# functions with no stack argument get one vector per fixture object.
# ret="void" where the original leaves a value in eax that is not a return value (Channel_setPanGains,
# Channel_resetPanState, Seq_mergeChannelDesc).

_MEDIAN = [("$i5", "$i7", "$i6"), ("$i7", "$i5", "$i6"), ("$i6", "$i7", "$i5"), ("$i3", "$i4", "$i5a"),
           ("$i5a", "$i4", "$i3"), ("$i3", "$i2", "$i5a"), ("$i5a", "$i2", "$i3"), ("$i0", "$i7", "$i3"),
           ("$i7", "$i0", "$i3"), ("$i5", "$i5b", "$i11"), ("$i11", "$i5", "$i5b"), ("$i5", "$i11", "$i5b"),
           ("$i10", "$i1", "$i2"), ("$i2", "$i1", "$i10"), ("$i3", "$i7", "$i0"), ("$i0", "$i3", "$i7")]

_POW = [("$b2", "$e0"), ("$b2", "$e1"), ("$b2", "$e2"), ("$b2", "$e3"), ("$b2", "$e10"), ("$b3", "$e5"),
        ("$bm2", "$e3"), ("$bm2", "$e4"), ("$b7", "$e2"), ("$bbig", "$e2"), ("$b1", "$em3"), ("$bm1", "$em3"),
        ("$bm1", "$em2"), ("$b2", "$em1"), ("$bm3", "$em2"), ("$b2", "$e31"), ("$b3", "$e0"),
        ("$bm1", "$emax"), ("$b7", "$e1")]

_FIRST = [("$in0", "$out0", "$n1", "$co0", "$st0"), ("$in0", "$out1", "$n2", "$co1", "$st1"),
          ("$in1", "$out2", "$n4", "$co0", "$st2"), ("$in1", "$out3", "$n8", "$co2", "$st3"),
          ("$in2", "$out4", "$n8", "$co1", "$st4"), ("$in2", "$out5", "$n4", "$co3", "$st5"),
          ("$in3", "$out6", "$n8", "$co0", "$st0"), ("$in3", "$out7", "$n2", "$co3", "$st1"),
          ("$in4", "$out0", "$n8", "$co1", "$st2"), ("$in4", "$out1", "$n1", "$co2", "$st3"),
          ("$in5", "$out2", "$n8", "$co3", "$st4"), ("$in5", "$out3", "$n0", "$co0", "$st5"),
          ("$in6", "$out4", "$nm1", "$co1", "$st0"), ("$in6", "$out5", "$n8", "$co2", "$st1")]

_MEAN = [("$mn1", "$mi0", "$mo0"), ("$mn2", "$mi0", "$mo1"), ("$mn4", "$mi1", "$mo2"),
         ("$mn8", "$mi1", "$mo3"), ("$mn8", "$mi2", "$mo4"), ("$mn4", "$mi2", "$mo5"),
         ("$mn8", "$mi3", "$mo0"), ("$mn2", "$mi3", "$mo1"), ("$mn8", "$mi4", "$mo2"),
         ("$mn1", "$mi4", "$mo3"), ("$mn0", "$mi5", "$mo4"), ("$mnm1", "$mi5", "$mo5"),
         ("$mn8", "$mi5", "$mo2"), ("$mn4", "$mi0", "$mo5")]

_RMS = [("$rn1", "$ri0", "$ra0"), ("$rn2", "$ri0", "$ra1"), ("$rn4", "$ri1", "$ra2"), ("$rn8", "$ri1", "$ra3"),
        ("$rn8", "$ri2", "$ra4"), ("$rn4", "$ri2", "$ra5"), ("$rn8", "$ri3", "$ra0"), ("$rn2", "$ri3", "$ra1"),
        ("$rn8", "$ri4", "$ra2"), ("$rn1", "$ri4", "$ra3"), ("$rnm1", "$ri0", "$ra4"),
        ("$rnm3", "$ri2", "$ra5"), ("$rn4", "$ri4", "$ra1")]

_FLOORS = [(f"$f{i}",) for i in range(16)]
_COMBS = [(f"$k{i}",) for i in range(12)]
_RELEASE = [(f"$r{i}",) for i in range(12)]
_STOPS = [(f"$s{i}",) for i in range(12)]
_PANSTATE = [(f"$p{i}",) for i in range(12)]

_PANS = [-0x80000000, -0x41, -0x40, -0x3F, -0x20, -1, 0, 1, 0x20, 0x3F, 0x40, 0x7FFFFFFF]
_PANCH = ["$g_zero", "$g_small", "$g_mid", "$g_max", "$g_neg"]

_DSBUFS = ["$d_zero", "$d_ones", "$d_mix", "$d_busy"]
_DSMODES = [0, 1, 2, 0xFFFFFFFF]

_TRACKFLAG = [("$q4", 0)] \
    + [("$q4", f"$t{i}") for i in ("0a", "0b", "1a", "1b", "3a", "3b", "4", "99")] \
    + [("$q1", f"$t{i}") for i in ("0a", "1a")] \
    + [("$q0", f"$t{i}") for i in ("0a", "3b")]

_LOAD = [("$c_dst0", 0)] + [(d, s) for d in ("$c_dst0", "$c_dst1", "$c_dst2")
                            for s in ("$c_src0", "$c_src1", "$c_src2", "$c_src3", "$c_src4")]

_MERGE = [("$m_dst0", 0)] \
    + [("$m_dst0", f"$m_src{i}") for i in range(16)] \
    + [("$m_dst1", f"$m_src{i}") for i in (0, 3, 5, 10, 15)] \
    + [("$m_dst2", "$m_named0"), ("$m_dst2", "$m_named1"), ("$m_dst2", "$m_src0")]

HOOKS = {
    # 0x1003e480: three int cells, read-only, so the spread is the return value. The 16 triples take both sides
    # of all six compares: *b above both (1, 3, 8, 12, 15), *b above *a but not *c (4, 16), *c above/below *a
    # inside that arm (1 vs 3), *b equal to *a (10), *b below *a but not below *c (5, 11), *b below both
    # (2, 6, 7, 9, 13, 14) and *c above/below *a inside that arm (6, 13 vs 2, 7, 9, 14).
    "medianOfThree": dict(module="sound.dll", addr=0x0003E480, abi="default", ret="int",
                          args=["pointer", "pointer", "pointer"], fixture="c3az_ints",
                          vectors=_MEDIAN),
    # 0x1003e2c0: two int cells, read-only. Positive exponents drive the square-and-multiply loop with odd and
    # even remainders; the non-positive ones take the exponent-0 arm, the base-1 arm, the base--1 arm (which
    # negates the exponent and runs the loop) and the "any other base" arm that returns 0. The base-0 arm is
    # an integer division by zero and is not exercised.
    "intPow": dict(module="sound.dll", addr=0x0003E2C0, abi="default", ret="int", args=["pointer", "pointer"],
                   fixture="c3az_pow", vectors=_POW),
    # 0x10040da0: input samples, output samples, count, coefficient and the one-sample state. Counts 0 and -1
    # skip the loop; 1, 2, 4 and 8 run it. The output buffers and the state cells are the state regions (the
    # return value is always 0). Samples include values whose products are not exact in float.
    "codecFirstOrderFilter": dict(module="sound.dll", addr=0x00040DA0, abi="default", ret="int",
                                  args=["pointer", "pointer", "pointer", "pointer", "pointer"],
                                  fixture="c3az_firstorder",
                                  state=[(f"$out{i}", 0, 0x48) for i in range(8)]
                                        + [(f"$st{i}", 0, 4) for i in range(6)],
                                  vectors=_FIRST),
    # 0x1003ed00: count, input samples, output samples. Counts 0 and -1 skip both loops (count 0 also divides
    # 0 by 0); 1, 2, 4 and 8 run them. Only the output buffers change, so they are the state regions.
    "codecRemoveMean": dict(module="sound.dll", addr=0x0003ED00, abi="default", ret="int",
                            args=["pointer", "pointer", "pointer"], fixture="c3az_mean",
                            state=[(f"$mo{i}", 0, 0x48) for i in range(6)],
                            vectors=_MEAN),
    # 0x1003ec60: count, samples, accumulator slot. Counts -1 and -3 skip the loop and still divide and take
    # the square root; 1, 2, 4 and 8 run it. The accumulator slots are the state regions.
    "codecRmsEnergy": dict(module="sound.dll", addr=0x0003EC60, abi="default", ret="int",
                           args=["pointer", "pointer", "pointer"], fixture="c3az_rms",
                           state=[(f"$ra{i}", 0, 4) for i in range(6)],
                           vectors=_RMS),
    # 0x1003e400: one float cell, read-only. Eight non-negative and eight negative values, each group holding
    # exact halves, values just under and just over a half, a value that is not exact in binary and a large
    # magnitude. No NaN and no infinity: VC6's __ftol returns 0 for a NaN where a modern conversion does not.
    "floatFloorToInt": dict(module="sound.dll", addr=0x0003E400, abi="default", ret="int", args=["pointer"],
                            fixture="c3az_floors", vectors=_FLOORS),
    # 0x10040f60: 12 contexts, one vector each (one pointer argument). k0..k5 keep both cursors above 1 so
    # neither wraps, k6..k9 put one or both at 1 so the `jg` takes its other side, k10 starts a cursor at 0 and
    # k11 makes the 16-bit sum wrap. Each context's cursor pair and delay line is a state region.
    "codecCombFilter": dict(module="sound.dll", addr=0x00040F60, abi="default", ret="int", args=["pointer"],
                            fixture="c3az_comb",
                            state=[(f"$k{i}", 0xBD8, 0x14) for i in range(12)],
                            vectors=_COMBS),
    # 0x10034ac0: 12 channels, one vector each (no stack argument). r0 has bit 6 clear and r1 bit 4 set (both
    # return 0x15 untouched), r2 has bit 0 clear and r3 bit 28 set (both only set bit 3 of the flag word), r4
    # has bits 12..27 all clear so the sweep touches nothing, and r5..r11 sweep voices whose two doubles are
    # zero, non-zero, negative, a denormal and a NaN in every combination. The whole channel is the state
    # region, so the flag word, every voice flag byte and every +0x48 dword are compared.
    "Channel_releaseVoices": dict(module="sound.dll", addr=0x00034AC0, abi="thiscall", ret="int",
                                  args=["pointer"], fixture="c3az_relvoices",
                                  state=[(f"$r{i}", 0, 0x1400) for i in range(12)],
                                  vectors=_RELEASE),
    # 0x10034b90: 12 channels, one vector each. s0/s1 take the two 0x15 guards, s2 takes the parking arm (bit 0
    # clear) and s3..s11 sweep voices whose double at +0x54 is below zero, zero, above zero and a NaN, with
    # flag bytes that already have bit 3 or bit 5 set and with bits 12..27 of the flag word clear (s4).
    "Channel_stopVoices": dict(module="sound.dll", addr=0x00034B90, abi="thiscall", ret="int", args=["pointer"],
                               fixture="c3az_stopvoices",
                               state=[(f"$s{i}", 0, 0x1400) for i in range(12)],
                               vectors=_STOPS),
    # 0x10035340: five channels (base level 0, 1, 0x4000, 0x10000 and a negative one) x twelve pan values that
    # cover both clamps, zero, both signs and both extremes of int. The state window 0x1d8..0x227 holds the
    # base level, the parked pair, both gains, both gain doubles and the stored pan.
    "Channel_setPanGains": dict(module="sound.dll", addr=0x00035340, abi="thiscall", ret="void",
                                args=["pointer", "int"], fixture="c3az_pangains",
                                state=[(c, 0x1D8, 0x50) for c in _PANCH],
                                vectors=[(c, p) for c in _PANCH for p in _PANS]),
    # 0x10033ba0: 12 channels, one vector each (no stack argument). The flag words cover every combination of
    # bits 0, 2, 29 and 31 plus two words with unrelated bits set, and the parked dwords at +0x208, +0x210 and
    # +0x214 and the pan at +0x224 differ per channel so Channel_setPanGains takes its zero, positive, negative
    # and clamping arms. The state window 0x58..0x227 holds the rebuilt flag pair and everything the callee
    # writes.
    "Channel_resetPanState": dict(module="sound.dll", addr=0x00033BA0, abi="thiscall", ret="void",
                                  args=["pointer"], fixture="c3az_panstate",
                                  state=[(f"$p{i}", 0x58, 0x1D0) for i in range(12)],
                                  vectors=_PANSTATE),
    # 0x10010f30: four buffers x four modes. d_busy has a non-null interface slot at +0x60 so it is refused
    # with 6; the other three differ in the dword at +0 (0, all ones, a mixed pattern) so the bit-1 merge is
    # visible both ways. Modes 0 and 1 take the two layouts, 2 and 0xffffffff the default. The state window
    # 0..0xaf covers the flag dword and the whole description block.
    "DsBuffer::configureDesc": dict(module="sound.dll", addr=0x00010F30, abi="thiscall", ret="int",
                                    args=["pointer", "uint32"], fixture="c3az_dsdesc",
                                    state=[(b, 0, 0xB0) for b in _DSBUFS],
                                    vectors=[(b, m) for b in _DSBUFS for m in _DSMODES]),
    # 0x10023e10: three sequencers whose vtable slot +0xb4 returns 4, 1 and 0 track records, x descriptors with
    # indices 0, 1, 3, 4 and 99 and with bit 1 of +0x2c set and clear. The null descriptor and every index at
    # or above the count return 0xa; the rest write bit 4 of the record's +0x38 and the record's +0x460. The
    # record arena and each sequencer's list cursor are the state regions.
    "Seq_applyTrackFlag": dict(module="sound.dll", addr=0x00023E10, abi="thiscall", ret="int",
                               args=["pointer", "pointer"], fixture="c3az_trackflag",
                               state=[("$arena", 0, 8 * 0x480)]
                                     + [(q, 0x1F8, 0x10) for q in ("$q0", "$q1", "$q4")],
                               vectors=_TRACKFLAG),
    # 0x10024280: three destinations (own +4 of 0, all ones and a mixed pattern) x five sources whose +4 has
    # bits 0 and 1 in all four combinations plus a word with the other bits set, and the null-source guard.
    # Every source's name pointer at +0x1c is NULL and every destination starts with a NULL name at +0x1c, so
    # Sound_setName neither frees nor allocates and +0x1c stays 0 in the state window 0..0x1f.
    "Seq_loadChannelDesc": dict(module="sound.dll", addr=0x00024280, abi="thiscall", ret="int",
                                args=["pointer", "pointer"], fixture="c3az_chandesc",
                                state=[(d, 0, 0x20) for d in ("$c_dst0", "$c_dst1", "$c_dst2")],
                                vectors=_LOAD),
    # 0x10024360: the null-source guard, then all 16 combinations of bits 0, 1, 3 and 4 of the source's byte at
    # +4 against a destination whose own +4 starts at 0, five of them against a destination whose +4 starts all
    # ones, and two sources that carry a name so the Sound_setName call runs. The state windows are 0..0x1b and
    # 0x20..0x3f: the name pointer at +0x1c is left out because the two arms each allocate their own copy.
    "Seq_mergeChannelDesc": dict(module="sound.dll", addr=0x00024360, abi="thiscall", ret="void",
                                 args=["pointer", "pointer"], fixture="c3az_mergedesc",
                                 state=[(d, off, n) for d in ("$m_dst0", "$m_dst1", "$m_dst2")
                                        for off, n in ((0, 0x1C), (0x20, 0x20))],
                                 vectors=_MERGE),
}

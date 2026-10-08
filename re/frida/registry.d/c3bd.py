# Batch c3bd: sound.dll speech-codec leaves (the stage leaves called by codecStageDispatch 0x1003c640 and
# codecApplyFixedCoeffs 0x1003d040) and two MIDI track walkers (module-aware hooks; addr is the RVA,
# VA = 0x10000000 + RVA). Reimplementations in shim/src/re/c3bd.cpp, fixtures in re/frida/js/fixtures.d/c3bd.js.
# Every entry only touches buffers and records the fixture allocates: no live sound object, device, list or
# DirectSound interface is involved, no import is called and nothing is allocated or freed.
# Eight of the nine functions are leaves, so each of those carries >= 10 vectors (the gate's leaf rule); the
# ninth (Midi_seekTrackToTick) calls Midi_readVarLen 0x0001e710, which no batch hooks.
# Every function returns a constant (0, or one of 0, 0x12 and 0x29 for the MIDI walkers), so the evidence is in
# the state regions: the buffers and records each function writes.

_SYMBOLS = [(f"$s{i}", f"$v{i}", f"$c{i}") for i in range(20)]

_SUBSTITUTE = [("$n0", "$rp0", "$sc0"), ("$nm1", "$rp0", "$sc1"), ("$n9", "$rp0", "$sc0"),
               ("$n9", "$rp0", "$sc1"), ("$n9", "$rp1", "$sc2"), ("$n9", "$rp1", "$sc3"),
               ("$n9", "$rp2", "$sc4"), ("$n9", "$rp2", "$sc5"), ("$n9", "$rp3", "$sc6"),
               ("$n9", "$rp0", "$sc7"), ("$n1", "$rp1", "$sc1"), ("$n2", "$rp2", "$sc3"),
               ("$n4", "$rp0", "$sc2"), ("$n8", "$rp1", "$sc5"), ("$n8", "$rp3", "$sc7")]

_IIR2 = [(0, "$in0", "$ic0"), (0, "$inm1", "$ic1"), ("$is0", "$in0", "$ic1"), ("$is0", "$in8", "$ic0"),
         ("$is0", "$in8", "$ic1"), ("$is1", "$in8", "$ic2"), ("$is2", "$in8", "$ic3"),
         ("$is3", "$in8", "$ic4"), ("$is4", "$in8", "$ic5"), ("$is5", "$in8", "$ic2"),
         ("$is1", "$in1", "$ic3"), ("$is2", "$in2", "$ic4"), ("$is3", "$in4", "$ic5"),
         ("$is4", "$in4", "$ic0"), ("$is5", "$in2", "$ic1")]

_BIQUAD = [(0, "$bf1", "$bl0", "$bt0"), ("$bs0", "$bf1", "$bl0", "$bt0"),
           ("$bs0", "$bf1", "$bl12", "$bt0"), ("$bs0", "$bf1", "$bl12", "$bt1"),
           ("$bs1", "$bf1", "$bl12", "$bt2"), ("$bs2", "$bf1", "$bl12", "$bt3"),
           ("$bs3", "$bf1", "$bl12", "$bt4"), ("$bs4", "$bf1", "$bl12", "$bt1"),
           ("$bs0", "$bf3", "$bl8", "$bt2"), ("$bs1", "$bf3", "$bl8", "$bt3"),
           ("$bs2", "$bf5", "$bl8", "$bt4"), ("$bs3", "$bf5", "$bl12", "$bt0"),
           ("$bs4", "$bf1", "$bl1", "$bt2"), ("$bs1", "$bf1", "$bl4", "$bt4")]

_CORR = [("$cl0", "$co0", "$clast24", "$cwin12", "$ck0"), ("$cl1", "$co1", "$clast24", "$cwin12", "$ck1"),
         ("$cl2", "$co2", "$clast24", "$cwin12", "$ck2"), ("$cl3", "$co3", "$clast24", "$cwin12", "$ck3"),
         ("$cl4", "$co4", "$clast24", "$cwin12", "$ck4"), ("$cl0", "$co5", "$clast24", "$cwin8", "$ck5"),
         ("$cl1", "$co0", "$clast24", "$cwin8", "$ck1"), ("$cl2", "$co1", "$clast24", "$cwin4", "$ck2"),
         ("$cl0", "$co2", "$clast24", "$cwin4", "$ck3"), ("$cl0", "$co3", "$clast24", "$cwin0", "$ck4"),
         ("$cl3", "$co4", "$clast24", "$cwin0", "$ck0"), ("$cl1", "$co5", "$clast16", "$cwin8", "$ck5"),
         ("$cl2", "$co0", "$clast16", "$cwin8", "$ck1"), ("$cl4", "$co1", "$clast16", "$cwin4", "$ck2"),
         ("$cl0", "$co2", "$clast16", "$cwin4", "$ck3")]

_FIR = [("$fi0", "$fo0", "$flast48", "$fwin16"), ("$fi1", "$fo1", "$flast48", "$fwin16"),
        ("$fi2", "$fo2", "$flast48", "$fwin16"), ("$fi3", "$fo3", "$flast48", "$fwin16"),
        ("$fi4", "$fo4", "$flast48", "$fwin16"), ("$fi0", "$fo1", "$flast48", "$fwin8"),
        ("$fi1", "$fo2", "$flast48", "$fwin8"), ("$fi2", "$fo3", "$flast48", "$fwin1"),
        ("$fi3", "$fo4", "$flast48", "$fwin1"), ("$fi0", "$fo0", "$flast48", "$fwin0"),
        ("$fi1", "$fo1", "$flast40", "$fwin8"), ("$fi2", "$fo2", "$flast40", "$fwin8"),
        ("$fi0", "$fo3", "$flast40", "$fwin1"), ("$fi4", "$fo4", "$flast40", "$fwin8")]

_AUTO = [("$ao4", "$af1", "$an12", "$ax0", "$am0", "$al0"), ("$ao4", "$af1", "$an12", "$ax1", "$am1", "$al1"),
         ("$ao4", "$af1", "$an12", "$ax2", "$am2", "$al2"), ("$ao4", "$af1", "$an12", "$ax3", "$am3", "$al3"),
         ("$ao4", "$af1", "$an12", "$ax4", "$am4", "$al4"), ("$ao4", "$af3", "$an12", "$ax0", "$am5", "$al5"),
         ("$ao2", "$af1", "$an8", "$ax0", "$am6", "$al6"), ("$ao2", "$af1", "$an8", "$ax1", "$am7", "$al7"),
         ("$ao1", "$af1", "$an8", "$ax0", "$am0", "$al0"), ("$ao1", "$af1", "$an8", "$ax2", "$am1", "$al1"),
         ("$ao4", "$af5", "$an4", "$ax0", "$am2", "$al2"), ("$ao2", "$af3", "$an4", "$ax1", "$am3", "$al3"),
         ("$ao4", "$af1", "$an12", "$ax0", "$am4", "$al4"), ("$ao2", "$af1", "$an8", "$ax4", "$am5", "$al5")]

_SCAN = [(f"$k{i}",) for i in range(14)]

_SEEK = [("$q", "$t_notes", 0), ("$q", "$t_notes2", 50), ("$q", "$t_notes3", 1000),
         ("$q", "$t_ctrl7", 1000), ("$q", "$t_ctrl7b", 60),
         ("$q", "$t_ctrlOther", 1000),
         ("$q", "$t_meta", 1000), ("$q", "$t_meta2", 15),
         ("$q", "$t_one", 1000),
         ("$q", "$t_run", 1000), ("$q", "$t_run2", 2),
         ("$q", "$t_bad", 1000),
         ("$q", "$t_nostatus", 1000),
         ("$q", "$t_long", 10), ("$q", "$t_long2", 2500),
         ("$q", "$t_endfirst", 1000)]

HOOKS = {
    # 0x1003e520: the packed symbol, the decoded value slot and a counter, all int cells. The twenty symbols
    # cover all four (table bit 4, parity) combinations, so both sides of the `je` at 0x1003e5b1, of the `je`
    # at 0x1003e5b7 and of the `jne` at 0x1003e5d9 are taken. The value slots and the counters are the state
    # regions; the return value is always 0.
    "decodeTableSymbol": dict(module="sound.dll", addr=0x0003E520, abi="default", ret="int",
                                    args=["pointer", "pointer", "pointer"], fixture="c3bd_symbols",
                                    state=[(f"$v{i}", 0, 4) for i in range(20)]
                                          + [(f"$c{i}", 0, 4) for i in range(20)],
                                    vectors=_SYMBOLS),
    # 0x1003e600: the sample count, the replacement samples and the scanned samples. Counts 0 and -1 skip the
    # scan; the eight scanned buffers break the 0.99f limit at the first element, at the last, through a
    # negative value, exactly at the limit (which does not break it) and just above it. The scanned buffers are
    # the state regions.
    "substituteIfBelowThreshold": dict(module="sound.dll", addr=0x0003E600, abi="default", ret="int",
                                             args=["pointer", "pointer", "pointer"], fixture="c3bd_substitute",
                                             state=[(f"$sc{i}", 0, 0x48) for i in range(8)],
                                             vectors=_SUBSTITUTE),
    # 0x1003d0a0: the samples (in place), the sample count and the codec context. The first two vectors pass a
    # NULL sample pointer with a count of 0 and -1, which is the only way to take the `je` at 0x1003d0bc. The
    # sample buffers and the five float state slots at context+0xbec are the state regions.
    "applyIir2": dict(module="sound.dll", addr=0x0003D0A0, abi="default", ret="int",
                            args=["pointer", "pointer", "pointer"], fixture="c3bd_iir2",
                            state=[(f"$is{i}", 0, 0x48) for i in range(6)]
                                  + [(f"$ic{i}", 0xBEC, 0x14) for i in range(6)],
                            vectors=_IIR2),
    # 0x10040e30: the samples (in place), the first and last indices and four float state slots. The first
    # vector passes a NULL sample pointer with first 1 and last 0, the only way to take the `je` at 0x10040e4c.
    # The sample buffers and the state blocks are the state regions.
    "codecBiquadFilter": dict(module="sound.dll", addr=0x00040E30, abi="default", ret="int",
                                    args=["pointer", "pointer", "pointer", "pointer"], fixture="c3bd_biquad",
                                    state=[(f"$bs{i}", 0, 0x48) for i in range(5)]
                                          + [(f"$bt{i}", 0, 0x10) for i in range(5)],
                                    vectors=_BIQUAD),
    # 0x10040330: the input samples, the output samples, the last index, the window length and two coefficient
    # slots. Windows 12, 8 and 4 give the three accumulator loops one, two and three different trip counts and
    # window 0 leaves every loop unentered; the all-zero and the 1e-7 inputs keep the first accumulator at or
    # below the 1e-10 floor at 0x1005ec40, so both sides of the `test ah, 0x41` at 0x10040415 are taken. The
    # output buffers and the coefficient pairs are the state regions.
    "codecCorrelationLags": dict(module="sound.dll", addr=0x00040330, abi="default", ret="int",
                                       args=["pointer", "pointer", "pointer", "pointer", "pointer"],
                                       fixture="c3bd_corrlags",
                                       state=[(f"$co{i}", 0, 0x80) for i in range(6)]
                                             + [(f"$ck{i}", 0, 8) for i in range(6)],
                                       vectors=_CORR),
    # 0x100404d0: the input samples, the output samples, the last index and the window length. Windows 16, 8
    # and 1 run the 31-tap loop sixteen, eight and one times and window 0 leaves it unentered. The output
    # buffers are the state regions.
    "codecSymmetricFir": dict(module="sound.dll", addr=0x000404D0, abi="default", ret="int",
                                    args=["pointer", "pointer", "pointer", "pointer"], fixture="c3bd_fir",
                                    state=[(f"$fo{i}", 0, 0x100) for i in range(5)],
                                    vectors=_FIR),
    # 0x1003e9f0: the order, the first index, the last index, the samples, the order x order matrix and the lag
    # vector. Order 4 runs all four loops, order 2 runs loop C exactly once, order 1 leaves loops C and D
    # unentered, and a first index past the last index leaves loops A and B unentered. The matrices and the lag
    # vectors are the state regions.
    "codecAutocorrelation": dict(module="sound.dll", addr=0x0003E9F0, abi="default", ret="int",
                                       args=["pointer", "pointer", "pointer", "pointer", "pointer", "pointer"],
                                       fixture="c3bd_autocorr",
                                       state=[(f"$am{i}", 0, 0x80) for i in range(8)]
                                             + [(f"$al{i}", 0, 0x40) for i in range(8)],
                                       vectors=_AUTO),
    # 0x1001ef80: fourteen reader objects, one vector each (no stack argument). The tracks cover the two-byte,
    # one-byte, meta and error cases of the table at 0x1001f06c, running status, multi-byte delta times, a meta
    # event with an empty payload and end of track as the first event. The whole object is the state region, so
    # the total written at +0x54 is compared (the three error tracks write nothing).
    "Midi_scanTrack": dict(module="sound.dll", addr=0x0001EF80, abi="thiscall", ret="int",
                                 args=["pointer"], fixture="c3bd_scantracks",
                                 state=[(f"$k{i}", 0, 0x60) for i in range(14)],
                                 vectors=_SCAN),
    # 0x10024920: the sequencer, the track record and the tick limit. The limits take the walk past the end of
    # the track (0x29 with writes), stop it on the first event (0x29 with no write), stop it later (0 with
    # writes) and run it into an unsupported status (0x12). The controller tracks drive the controller-7 store
    # at sequencer+0x254+channel*0x4c for channels 0, 3 and 15. The track records and the sequencer's
    # controller block are the state regions.
    "Midi_seekTrackToTick": dict(module="sound.dll", addr=0x00024920, abi="thiscall", ret="int",
                                       args=["pointer", "pointer", "uint32"], fixture="c3bd_seektracks",
                                       state=[("$q", 0x254, 0x47C)]
                                             + [(t, 0, 0x60) for t in
                                                ("$t_notes", "$t_notes2", "$t_notes3", "$t_ctrl7", "$t_ctrl7b",
                                                 "$t_ctrlOther", "$t_meta", "$t_meta2", "$t_one", "$t_run",
                                                 "$t_run2", "$t_bad", "$t_nostatus", "$t_long", "$t_long2",
                                                 "$t_endfirst")],
                                       vectors=_SEEK),
}

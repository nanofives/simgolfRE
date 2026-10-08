# Batch c3au: sound.dll DirectSound buffer geometry, sequencer list helpers, the bit (de)serialiser and two x87
# leaves (module-aware hooks; addr is the RVA, VA = 0x10000000 + RVA). Reimplementations in shim/src/re/c3au.cpp,
# fixtures in re/frida/js/fixtures.d/c3au.js. Every entry only touches records the fixture allocates: no live sound
# object, device, list or DirectSound interface is involved, and no import is called.
# Twelve of the seventeen functions are leaves, so each of those carries >= 10 vectors (the gate's leaf rule);
# functions with no stack argument get one vector per fixture object.
# ret="void" where the original leaves a value in eax that is not a return value (Track_resetReadState,
# Sound_setSourceType, MmioBuffer_init, Voice_setLevel, computeCaptureDuration).


def _s32(v):
    """HRESULTs are passed as Frida 'int': give them their signed value."""
    return v - 0x100000000 if v >= 0x80000000 else v


_HRESULTS = [0x80004001, 0x80004002, 0x80004005, 0x8007000E, 0x8007000F, 0x80070057, 0x80070058,
             0x8878000A, 0x88780009, 0x88780063, 0x88780064, 0x88780065, 0x88780078, 0x88780079,
             0x88780096, 0x887800A9, 0x887800AA, 0x887800AB, 0x00000000, 0x00000001, 0x7FFFFFFF,
             0x80000000, 0xFFFFFFFF]

_STREAM = [f"$c{i}" for i in range(12)]
_TRACKS = [f"$t{i}" for i in range(12)]
_CAPTURE = [f"$d{i}" for i in range(12)]
_TYPES = [0, 1, 2, 3, 4, 5, 6, 7, 8, 0xFFFFFFFF]
_SEEK = [("$l_empty", 0), ("$l_empty", 7), ("$l_busy", 0), ("$l_busy", 3), ("$l5", 0), ("$l5", 1), ("$l5", 2),
         ("$l5", 4), ("$l5", 5), ("$l5", 0xFFFFFFFF), ("$l1", 0), ("$l1", 1)]
_SEQS = ["$q_none", "$q_nopay", "$q1", "$q3", "$q5", "$q_hole"]
_SOURCES = ["$p0", "$p1", "$p2", "$p3", "$p4", "$p5", "$p7", "$p_same", "$p_hi", "$p_zero"]
_CMD11 = [("$k_guard", 10), ("$k_none", 10), ("$k_norec", 10), ("$k3", 10), ("$k3", 20), ("$k3", 30),
          ("$k3", 99), ("$k_hole", 10), ("$k_hole", 30), ("$k_hole", 99), ("$k_dup", 20), ("$k_dup", 10)]
_MMIO = [("$m0", "$blk0", "$fmt0"), ("$m1", 0, "$fmt1"), ("$m2", "$blk1", "$fmt2"), ("$m3", "$blk2", "$fmt0"),
         ("$m4", 0, "$fmt2"), ("$m5", "$blk0", "$fmt1"), ("$m0", 0, "$fmt2"), ("$m1", "$blk2", "$fmt0"),
         ("$m2", 0, "$fmt0"), ("$m3", "$blk0", "$fmt2"), ("$m4", "$blk1", "$fmt1"), ("$m5", 0, "$fmt0")]
_REGIONS = [(10, 2), (50, 2), (100, 4), (0, 3), (1, 1), (1000, 4)]
_FLOATS = [(2, 2), (2, 3), (3, 2), (3, 3), (0, 2), (0, 3), (1, 2), (1, 3), (4, 5), (5, 4), (6, 7), (8, 9),
           (10, 11), (12, 13), (13, 12), (14, 2), (2, 14), (15, 3)]
_LEVELS = [0, 1, 63, 127, 0xFFFFFFFF]

HOOKS = {
    # 0x1000f480: HRESULT -> this module's error numbers. The vectors take every arm: the four codes handled by
    # the compare chain (0x80004001, 0x8007000e, 0x80070057, 0x8878000a), the four table hits (0x88780064,
    # 0x88780078, 0x88780096, 0x887800aa), neighbours of each that fall to the default, the two codes just
    # outside the 0x88780064..0x887800aa window, and positive values (which take the first `jg`).
    "mapDsError": dict(module="sound.dll", addr=0x0000F480, abi="default", ret="int", args=["int"],
                       vectors=[(_s32(h),) for h in _HRESULTS]),
    # 0x10035770: 12 channels, one vector each (no stack argument). c0 no head, c1 null record, c5 first key
    # above the position, c4 a key above it inside the chain, c6 a key exactly equal, c7 a null record inside
    # the chain, c9 the unsigned compare, c10 four accepted records. The cursor at +0x1350 is in the state.
    "Channel_findStreamNode": dict(module="sound.dll", addr=0x00035770, abi="thiscall", ret="int",
                                   args=["pointer"], fixture="c3au_streamchan",
                                   state=[(c, 0x1340, 0x20) for c in _STREAM],
                                   vectors=[(c,) for c in _STREAM]),
    # 0x10038330: read-only, no state; the spread is the returned voice record address. Four channels (every
    # mask bit set, every other bit, no bit, duplicate tags) x five tags (first, a tag whose voice has bit 3 of
    # its flag byte set, the last, the duplicated one, an absent one).
    "Channel_findVoiceByTag": dict(module="sound.dll", addr=0x00038330, abi="thiscall", ret="pointer",
                                   args=["pointer", "uint32"], fixture="c3au_voicetags",
                                   vectors=[(v, t) for v in ("$v_all", "$v_even", "$v_none", "$v_dup")
                                            for t in (0x10, 0x12, 0x1F, 0x41, 0x99)]),
    # 0x1001e230: 12 tracks, one vector each. The state windows hold +0x4c and +0x178/+0x17c/+0x180, so the
    # surviving bit 1 and all three clamp arms are compared.
    "Track_resetReadState": dict(module="sound.dll", addr=0x0001E230, abi="thiscall", ret="void",
                                 args=["pointer"], fixture="c3au_tracks",
                                 state=[(t, off, n) for t in _TRACKS for off, n in ((0x40, 0x10), (0x170, 0x20))],
                                 vectors=[(t,) for t in _TRACKS]),
    # 0x100116a0: the three guards (count 0 -> 3, index >= count -> 0xa, cursor already set -> 6) and walks of
    # 0, 1, 2 and 4 steps through a five-node chain. The cursor at +8 is in the state and is restored between
    # vectors, so each one starts from a null cursor.
    "seekListToIndex": dict(module="sound.dll", addr=0x000116A0, abi="thiscall", ret="int",
                            args=["pointer", "uint32"], fixture="c3au_seeklists",
                            state=[(l, off, n) for l in ("$l_empty", "$l_busy", "$l5", "$l1")
                                   for off, n in ((0, 0x10), (0x140, 0x10))],
                            vectors=list(_SEEK)),
    # 0x1002ad10: two objects (flag word all clear / all set) x types 0..8 and 0xffffffff, which covers every
    # jump-table arm, the fall-through arm (type 7) and both sides of the `ja` range test.
    "Sound_setSourceType": dict(module="sound.dll", addr=0x0002AD10, abi="thiscall", ret="void",
                                args=["pointer", "uint32"], fixture="c3au_sources",
                                state=[(s, 0x40, 0x20) for s in ("$s_clear", "$s_set")],
                                vectors=[(s, t) for s in ("$s_clear", "$s_set") for t in _TYPES]),
    # 0x1001d780: six sequences x step counts 0, 1 and 3. The out slot and the cursor at +0x1a4 are in the
    # state; q_hole's null payload in the middle ends the walk before the count is reached.
    "Seq_listGetAt": dict(module="sound.dll", addr=0x0001D780, abi="thiscall", ret="int",
                          args=["pointer", "pointer", "uint32"], fixture="c3au_getat",
                          state=[("$out", 0, 8)] + [(q, 0x190, 0x20) for q in _SEQS],
                          vectors=[(q, "$out", n) for q in _SEQS for n in (0, 1, 3)]),
    # 0x10022280: one destination table and ten sources with indices 0, 1, 2, 3, 4, 5, 7, 1 again and 0xa, plus
    # the null-source guard. The state covers slots 0..0xa of the table at +0x264.
    "Seq_copyChannelParams": dict(module="sound.dll", addr=0x00022280, abi="thiscall", ret="int",
                                  args=["pointer", "pointer"], fixture="c3au_chanparams",
                                  state=[("$dst", 0x260, 0x340)],
                                  vectors=[("$dst", p) for p in _SOURCES] + [("$dst", 0)]),
    # 0x10023c70: the zero guard, a null head, a null record, hits on the first, middle and last record of a
    # three-record chain, a miss, a null record inside the chain and duplicate keys. The record arena is in the
    # state because a hit writes the matched record's +0x30.
    "Seq_cmdHandler11": dict(module="sound.dll", addr=0x00023C70, abi="thiscall", ret="int",
                             args=["pointer", "uint32", "uint32"], fixture="c3au_cmd11",
                             state=[("$arena", 0, 32 * 0x40)]
                                   + [(q, 0x210, 0x20) for q in
                                      ("$k_guard", "$k_none", "$k_norec", "$k3", "$k_hole", "$k_dup")],
                             vectors=[(q, k, 0xABCD0000 + k) for q, k in _CMD11]),
    # 0x10037e10: six objects x (a 0x48-byte block or NULL) x three format records. The state window spans
    # 0..0xbf of each object, so the copied block, the three zeroed dwords and the 0x12 bytes at +0xa0 are all
    # compared, as is the untouched 0..0x47 of the NULL vectors.
    "MmioBuffer_init": dict(module="sound.dll", addr=0x00037E10, abi="thiscall", ret="void",
                            args=["pointer", "pointer", "pointer"], fixture="c3au_mmio",
                            state=[(f"$m{i}", 0, 0xC0) for i in range(6)],
                            vectors=list(_MMIO)),
    # 0x10010eb0: four buffers (CD rate, a tiny buffer, alignment 1, a byte rate whose product wraps) x six
    # (milliseconds, region count) pairs, which covers the rejected side of the `jbe` and a zero duration.
    "DsBuffer::setRegionMs": dict(module="sound.dll", addr=0x00010EB0, abi="thiscall", ret="int",
                                  args=["pointer", "uint32", "uint32"], fixture="c3au_dsregions",
                                  state=[(b, 0x30, 0x20) for b in ("$b_cd", "$b_small", "$b_align1", "$b_big")],
                                  vectors=[(b, ms, n) for b in ("$b_cd", "$b_small", "$b_align1", "$b_big")
                                           for ms, n in _REGIONS]),
    # 0x1003e380: read-only, returns a double. 18 pairs over both zeros, +-1, +-0.1, +-1/3, +-FLT_MAX, the
    # denormals, both infinities and a quiet NaN of each sign, so both sides of both `test ah, 1` branches run.
    "floatCopySign": dict(module="sound.dll", addr=0x0003E380, abi="default", ret="double",
                          args=["pointer", "pointer"], fixture="c3au_floats",
                          vectors=[(f"$f{a}", f"$f{b}") for a, b in _FLOATS]),
    # 0x1003c180: six records with counts 1, 2, 3, 5, 9 and 9. The serialiser writes the 54-dword bit array and
    # toggles the parity dword at ctx+0x2540; the count, scalar and value slots are in the state too, to show
    # they are only read.
    "bitPack": dict(module="sound.dll", addr=0x0003C180, abi="default", ret="int",
                    args=["pointer", "pointer", "pointer", "pointer", "pointer", "pointer"], fixture="c3au_bits",
                    state=[(f"${k}{i}", off, n) for i in range(6)
                           for k, off, n in (("a", 0, 4), ("b", 0, 4), ("val", 0, 0x40), ("bits", 0, 54 * 4),
                                             ("ctx", 0x2540, 4))],
                    vectors=[(f"$cnt{i}", f"$a{i}", f"$b{i}", f"$val{i}", f"$bits{i}", f"$ctx{i}")
                             for i in range(6)]),
    # 0x1003c1d0: the deserialising mode of the same body, six records. It writes the two scalar slots and the
    # value array from the bit array, which is in the state as a read-only witness.
    "bitUnpack": dict(module="sound.dll", addr=0x0003C1D0, abi="default", ret="int",
                      args=["pointer", "pointer", "pointer", "pointer", "pointer"], fixture="c3au_bits",
                      state=[(f"${k}{i}", 0, n) for i in range(6)
                             for k, n in (("ua", 4), ("ub", 4), ("uval", 0x40), ("ubits", 54 * 4))],
                      vectors=[(f"$ucnt{i}", f"$ua{i}", f"$ub{i}", f"$uval{i}", f"$ubits{i}")
                               for i in range(6)]),
    # 0x100383b0: four voices (pan 0, +0x20, -0x20 and a value that clamps) x levels 0, 1, 63, 127 and
    # 0xffffffff. The state window 0x40..0x8f holds the pan, the shifted level at +0x50, the double at +0x58 and
    # every gain field Voice_setPanGains writes.
    "Voice_setLevel": dict(module="sound.dll", addr=0x000383B0, abi="thiscall", ret="void",
                           args=["pointer", "uint32"], fixture="c3au_voicelevels",
                           state=[(w, 0x40, 0x50) for w in ("$w_zero", "$w_pos", "$w_neg", "$w_clamp")],
                           vectors=[(w, lv) for w in ("$w_zero", "$w_pos", "$w_neg", "$w_clamp")
                                    for lv in _LEVELS]),
    # 0x10006bd0: 12 objects, one vector each. d0 and d1 take the two guards; the rest divide exactly or leave a
    # remainder whose ratio is not exact in binary, including a negative span.
    "computeCaptureDuration": dict(module="sound.dll", addr=0x00006BD0, abi="thiscall", ret="void",
                                   args=["pointer"], fixture="c3au_capture",
                                   state=[(d, 0xB0, 0x40) for d in _CAPTURE],
                                   vectors=[(d,) for d in _CAPTURE]),
    # 0x1003d040: six sample buffers (counts 0, 1, 4, 8, 16, 16) plus the NULL-pointer vector, which the wrapper
    # turns into the address 4 and whose count of 0 keeps the filter's loop from running. The state holds each
    # buffer and its four-float filter state.
    "codecApplyFixedCoeffs": dict(module="sound.dll", addr=0x0003D040, abi="default", ret="int",
                                  args=["pointer", "pointer", "pointer"], fixture="c3au_codec",
                                  state=[(f"${k}{i}", 0, n) for i in range(6)
                                         for k, n in (("buf", 0x40), ("st", 0x10))],
                                  vectors=[(0, "$cnt0", "$st0")]
                                          + [(f"$buf{i}", f"$cnt{i}", f"$st{i}") for i in range(6)]
                                          + [("$buf2", "$cnt4", "$st5"), ("$buf5", "$cnt2", "$st1")]),
}

# Batch c3ar: sound.dll MIDI sequencer / voice-table helpers (module-aware hooks; addr is the RVA, VA =
# 0x10000000 + RVA). Reimplementations in shim/src/re/c3ar.cpp, fixtures in re/frida/js/fixtures.d/c3ar.js.
# Every entry only touches records the fixture allocates: no live sound object, device, list or import is involved.
# Fourteen of the fifteen functions are leaves, so each one carries >= 10 vectors (the gate's leaf rule); functions
# with no stack argument get one vector per fixture object.
# ret="void" wherever the original leaves a leftover value in eax that is not a return value (Voice_setLoopFlag,
# Channel_broadcastVoiceBit1, Seq_setPlayRange, Midi_advanceMsgRing, Voice_noteOff).

_VARLEN = [f"$e{i}" for i in range(15)]
_SWAP16 = [0, 1, 0xFF, 0x100, 0x7F7F, 0x1234, 0xABCD, 0xFFFF, 0x12340000, 0xFFFF0000, 0x0000FF00, 0x00FF00FF,
           0xDEADBEEF, 0x80008000, 0x000000AA, 0xFFFFFFFF]
_DELTA = [0, 1, 0x10, 0x100, 0x101, 0xFFFFFFFF]
_BITS = [0, 1, 3, 0xFE]
_RANGE = [(0, 0), (1, 2), (2, 1), (5, 5), (0, 0xFFFFFFFF), (0xFFFFFFFF, 0), (0x7FFFFFFF, 0x80000000),
          (0x80000000, 0x7FFFFFFF)]
_PAYLOAD = [0, 1, 0xFFFFFFFF, 0x80000000, 0x12345678, 0xA5A5A5A5]
_PAIR = [(0, 0), (1, 2), (0xFFFFFFFF, 0), (0, 0xFFFFFFFF), (0x12345678, 0x9ABCDEF0), (0x55555555, 0xAAAAAAAA)]

HOOKS = {
    # 0x1001e710: 7-bit groups, most significant first, until a byte without bit 7; the out counter gets the number
    # of bytes consumed. 15 encodings, from one byte to the five-byte case that overflows the 32-bit accumulator.
    "Midi_readVarLen": dict(module="sound.dll", addr=0x0001E710, abi="default", ret="uint32",
                            args=["pointer", "pointer"], fixture="c3ar_varlen",
                            state=[("$cnt", 0, 8), ("$buf", 0, 0x80)],
                            vectors=[(e, "$cnt") for e in _VARLEN]),
    # 0x1001e6e0: exchanges the two low bytes of the argument; bytes 2 and 3 are not read (the 0x12340000 /
    # 0xffff0000 / 0xdeadbeef vectors show it). The only jcc is the two-iteration loop, taken once per call.
    "byteSwap16": dict(module="sound.dll", addr=0x0001E6E0, abi="default", ret="uint16", args=["uint32"],
                       vectors=[(v,) for v in _SWAP16]),
    # 0x1001e9f0: t0 (pos 0x100, limit 0x200) covers accept/equal/reject, t1 (0, 0) equal and reject, t2
    # (0xfffffff0, 0xffffffff) the wrapping add. The state window holds +0x2c, +0x34, +0x40 and +0x44.
    "Track_advancePos": dict(module="sound.dll", addr=0x0001E9F0, abi="thiscall", ret="uint32",
                             args=["pointer", "uint32"], fixture="c3ar_tracks",
                             state=[(f"$t{i}", 0x28, 0x20) for i in range(3)],
                             vectors=[(f"$t{i}", d) for i in range(3) for d in _DELTA]),
    # 0x10037760: one channel, the 16 voice indices. The state window spans every voice flag byte
    # (0x2d4 + i*0x110, i = 0..15) plus the bytes around them, so a wrong stride shows up.
    "Voice_setLoopFlag": dict(module="sound.dll", addr=0x00037760, abi="thiscall", ret="void",
                              args=["pointer", "uint32"], fixture="c3ar_voicechan",
                              state=[("$ch", 0x2D0, 0x1018)],
                              vectors=[("$ch", i) for i in range(16)]),
    # 0x100377f0: "on1"/"on2" have bit 0 of +0x58 set (the 16-voice loop runs), "off1"/"off2" have it clear (only
    # bit 7 of +0x58 is rewritten). Both state windows are snapshotted so each path's untouched side is compared.
    "Channel_broadcastVoiceBit1": dict(module="sound.dll", addr=0x000377F0, abi="thiscall", ret="void",
                                       args=["pointer", "uint32"], fixture="c3ar_bcast",
                                       state=[(f"${o}", off, n) for o in ("on1", "on2", "off1", "off2")
                                              for off, n in ((0x50, 0x18), (0x2D0, 0x1018))],
                                       vectors=[(f"${o}", b) for o in ("on1", "on2", "off1", "off2")
                                                for b in _BITS]),
    # 0x100382c0: 14 channels whose +0x58 encodes a 16-bit mask at bits 12..27 (bits 28..31 are set in every one,
    # so the `and eax, 0xffff` is exercised). Counts 0..16: mask 0xffff gives 16 and returns NULL, 0x7fff gives 15
    # and returns the last voice record. The out counter is in the state.
    "Channel_allocVoiceSlot": dict(module="sound.dll", addr=0x000382C0, abi="thiscall", ret="pointer",
                                   args=["pointer", "pointer"], fixture="c3ar_allocslot",
                                   state=[("$out", 0, 8)] + [(f"$c{i}", 0x50, 0x10) for i in range(14)],
                                   vectors=[(f"$c{i}", "$out") for i in range(14)]),
    # 0x1001ae00: two sequences with different +0x214 (so the `or 1` is visible both ways) and eight argument pairs
    # including both orders of each, equal values and the signed extremes (the compare is unsigned).
    "Seq_setPlayRange": dict(module="sound.dll", addr=0x0001AE00, abi="thiscall", ret="void",
                             args=["pointer", "uint32", "uint32"], fixture="c3ar_seqrange",
                             state=[(f"$s{i}", 0x200, 0x20) for i in range(2)],
                             vectors=[(f"$s{i}", a, b) for i in range(2) for a, b in _RANGE]),
    # 0x1001adc0: 12 rings. r0/r1/r6/r7/r11 stay below the end, r2/r4/r5/r9/r10 pass it and reload the start,
    # r3 and r8 are the 0xffffffff index whose increment wraps to 0 and therefore does not reload.
    "Midi_advanceMsgRing": dict(module="sound.dll", addr=0x0001ADC0, abi="thiscall", ret="void", args=["pointer"],
                                fixture="c3ar_msgring",
                                state=[(f"$r{i}", 0x200, 0x18) for i in range(12)],
                                vectors=[(f"$r{i}",) for i in range(12)]),
    # 0x1001ab60: 12 entries, each pre-filled with its own non-zero pattern over 0 .. 0x22f, so the zeroed range
    # (0 .. 0x217), the 0xfa at 0x218 and the guard at 0x21c are all compared.
    "Sound_initEntry": dict(module="sound.dll", addr=0x0001AB60, abi="thiscall", ret="pointer", args=["pointer"],
                            fixture="c3ar_entries", state=[(f"$e{i}", 0, 0x230) for i in range(12)],
                            vectors=[(f"$e{i}",) for i in range(12)]),
    # 0x10025010: two nodes with different pre-fills, six payload values each.
    "NodeList_initNode": dict(module="sound.dll", addr=0x00025010, abi="thiscall", ret="pointer",
                              args=["pointer", "uint32"], fixture="c3ar_nodes",
                              state=[(f"$n{i}", 0, 0x10) for i in range(2)],
                              vectors=[(f"$n{i}", v) for i in range(2) for v in _PAYLOAD]),
    # 0x10008810: two objects, six argument pairs each.
    "pairStore": dict(module="sound.dll", addr=0x00008810, abi="thiscall", ret="pointer",
                      args=["pointer", "uint32", "uint32"], fixture="c3ar_pairs",
                      state=[(f"$p{i}", 0, 0x10) for i in range(2)],
                      vectors=[(f"$p{i}", a, b) for i in range(2) for a, b in _PAIR]),
    # 0x1000fec0: read-only, no state. 12 objects: both fields zero (returns 0), each field alone non-zero and
    # both non-zero (return 1), including 0x80000000 so the test is a zero test and not a sign test.
    "DsBuffer::hasAuxInterfaces": dict(module="sound.dll", addr=0x0000FEC0, abi="thiscall", ret="uint8",
                                       args=["pointer"], fixture="c3ar_dsbuf",
                                       vectors=[(f"$b{i}",) for i in range(12)]),
    # 0x100381f0: 12 voice records. v3/v8/v9/v10 have both doubles non-zero and ordered (bit 3 of +0x9c set);
    # v0/v1/v2/v4/v5/v11 have a zero (+0.0 or -0.0) in one of them and v6/v7 a NaN, which the x87 compare treats
    # the same way (C3 set), so all ten take the tail that writes +0xe4, +0x9c and +0x94.
    "Voice_noteOff": dict(module="sound.dll", addr=0x000381F0, abi="thiscall", ret="void", args=["pointer"],
                          fixture="c3ar_voices",
                          state=[(f"$v{i}", off, n) for i in range(12)
                                 for off, n in ((0x50, 0x10), (0x90, 0x10), (0xE0, 0x18))],
                          vectors=[(f"$v{i}",) for i in range(12)]),
    # 0x100285a0: 12 LCG states; the returned float is mantissa/2^23 in [0, 1) and the state advances.
    "nextRandomFloat": dict(module="sound.dll", addr=0x000285A0, abi="thiscall", ret="float", args=["pointer"],
                            fixture="c3ar_rngf", state=[(f"$r{i}", 0, 8) for i in range(12)],
                            vectors=[(f"$r{i}",) for i in range(12)]),
    # 0x1001e1c0: the track-list walk. q0 has no head, q1/q10 a first node without a track record, q4/q6 a null
    # track record inside the chain, q2/q3/q5/q7/q8/q9 chains of 1, 2, 3 and 5 records, and q11 has a single node
    # whose track record overlaps the sequence at +0x154 so that Seq_rewindTracks' store at track+0x50 zeroes the
    # sequence's own cursor at +0x1a4 and the walk ends at the re-read (je at 0x1001e1e9).
    # Every track record has a null list head at +0xc, which keeps Seq_rewindTracks and its callee FUN_1001bc00 on
    # their null-list paths: no virtual call is made from a fixture object.
    "Seq_rewind": dict(module="sound.dll", addr=0x0001E1C0, abi="thiscall", ret="uint32", args=["pointer"],
                       fixture="c3ar_seqs",
                       state=[("$tracks", 0, 20 * 0x60)] + [(f"$q{i}", off, n) for i in range(12)
                                                            for off, n in ((0x40, 0x20), (0x140, 0x80))],
                       vectors=[(f"$q{i}",) for i in range(12)]),
}

# Batch c3an: sound.dll leaves (module-aware hooks; addr is the RVA, VA = 0x10000000 + RVA).
# Reimplementations in shim/src/re/c3an.cpp, fixtures in re/frida/js/fixtures.d/c3an.js. Every entry calls a leaf
# that only touches the records the fixture allocates: no live sound object, device, list or import is involved.
# Functions with no stack argument get one vector per fixture object (the leaf rule wants >= 10 vectors).

_SPANS = [(f"$s{i}",) for i in range(14)]
_AT_INDEX = [0, 1, 2, 3, 0x7FF, 0x800, 0xABC, 0xFFE, 0xFFF, 0x1000, 0x1001, 0x1FFF, 0x12345, 0xFFFFF001,
             0x7FFFFFFF, 0xFFFFFFFF]
_PUSH_VALUE = [0, 1, 0xDEADBEEF, 0xFFFFFFFF]
_RAND_N = [0, 1, 2, 6, 0xFFFF, 0x10000, 0x7FFFFFFF, 0xFFFFFFFF]
_SIZES = [0, 1, 0x400, 0x8000, 0xFFFFFFFF]
_MODES = [0, 1, 2, 3, 4, 5, 6, 7, 0xF8, 0xFF, 0x100, 0x102]
_SRC = [(0, 0), (1, 2), (0x12345678, 0x9ABCDEF0), (0xFFFFFFFF, 0), (0, 0xFFFFFFFF), (0x55555555, 0xAAAAAAAA)]

HOOKS = {
    # 0x1000b030: [this+0xc] - [this+8] + 1, read-only, no branch. 14 objects with different (start, end) pairs.
    "getSpanLength": dict(module="sound.dll", addr=0x0000B030, abi="thiscall", ret="int", args=["pointer"],
                          fixture="c3an_spans", vectors=_SPANS),
    # 0x100088c0: this[(index & 0xfff)] over a 0x1000-dword array, read-only, no branch.
    "CommandQueue::at": dict(module="sound.dll", addr=0x000088C0, abi="thiscall", ret="uint32",
                             args=["pointer", "uint32"], fixture="c3an_cmdq",
                             vectors=[("$q", i) for i in _AT_INDEX]),
    # 0x10008860: zeroes 0x1000 dwords + the fields at 0x4000/0x4004 and returns this. The snapshots cover the head,
    # the middle and the tail of the array, the two fields, and the guard at 0x4008 the function must not reach.
    "CommandQueue::init": dict(module="sound.dll", addr=0x00008860, abi="thiscall", ret="pointer", args=["pointer"],
                               fixture="c3an_qinit",
                               state=[(f"$q{b}", off, n) for b in range(12)
                                      for off, n in ((0, 0x40), (0x2000, 0x40), (0x3FC0, 0x60))],
                               vectors=[(f"$q{b}",) for b in range(12)]),
    # 0x1000c3d0: ring push. "mid"/"wrap" take the store path (the second wraps the index to 0), "full"/"fullwrap"
    # take the jne at 0x1000c3e4 and return 0x22 without writing.
    "WaveCmdQueue::pushValue": dict(module="sound.dll", addr=0x0000C3D0, abi="thiscall", ret="uint32",
                                    args=["pointer", "uint32"], fixture="c3an_waveq",
                                    state=[(f"${q}", off, n) for q in ("mid", "wrap", "full", "fullwrap")
                                           for off, n in ((0x1D8, 0x40), (0x41B8, 0x30))],
                                    vectors=[(f"${q}", v) for q in ("mid", "wrap", "full", "fullwrap")
                                             for v in _PUSH_VALUE]),
    # 0x10028560: LCG step on the dword at offset 0, result scaled by the low 16 bits of the argument.
    "randRange": dict(module="sound.dll", addr=0x00028560, abi="thiscall", ret="uint32", args=["pointer", "uint32"],
                      fixture="c3an_rng", state=[(f"$r{i}", 0, 0x10) for i in range(3)],
                      vectors=[(f"$r{i}", n) for i in range(3) for n in _RAND_N]),
    # 0x1001bb80: cursor store + payload read. q0..q7 have a head node, q8..q11 have none (the je at 0x1001bb88).
    "Seq_firstTrackData": dict(module="sound.dll", addr=0x0001BB80, abi="thiscall", ret="uint32", args=["pointer"],
                               fixture="c3an_seqhead", state=[(f"$q{i}", 0, 0x20) for i in range(12)],
                               vectors=[(f"$q{i}",) for i in range(12)]),
    # 0x1000f0c0: "open" has the guard dword at +0x60 clear and stores the size; "busy"/"busy2" return 0xc.
    "DsBuffer::setBufferSize": dict(module="sound.dll", addr=0x0000F0C0, abi="thiscall", ret="uint32",
                                    args=["pointer", "uint32"], fixture="c3an_dsbuf",
                                    state=[(f"${o}", 0x60, 0x28) for o in ("open", "busy", "busy2")],
                                    vectors=[(f"${o}", s) for o in ("open", "busy", "busy2") for s in _SIZES]),
    # 0x10024c50: the three ordered bit tests. Modes 1/3/5/7/0xFF/0x102 take the first arm, 2/6 the second,
    # 4 the third, 0/0xF8/0x100 fall through without writing.
    "Voice_setPlayModeBits": dict(module="sound.dll", addr=0x00024C50, abi="thiscall", ret="void",
                                  args=["pointer", "uint32"], fixture="c3an_voice",
                                  state=[(f"${o}", 0x210, 0x18) for o in ("full", "clear")],
                                  vectors=[(f"${o}", m) for o in ("full", "clear") for m in _MODES]),
    # 0x1002ceb0: zeroes 0x41 dwords from +0xc, then +4, +0 and +8 = 1; returns this. The snapshot covers the whole
    # record including the guard at 0x110.
    "VoiceSlot_init": dict(module="sound.dll", addr=0x0002CEB0, abi="thiscall", ret="pointer", args=["pointer"],
                           fixture="c3an_slots", state=[(f"$v{i}", 0, 0x120) for i in range(12)],
                           vectors=[(f"$v{i}",) for i in range(12)]),
    # 0x10037720: stores both arguments and clears bit 2 of +0x58 (set in "set", already clear in "clear").
    "Channel_setStreamSource": dict(module="sound.dll", addr=0x00037720, abi="thiscall", ret="uint32",
                                    args=["pointer", "uint32", "uint32"], fixture="c3an_chansrc",
                                    state=[(f"${o}", off, n) for o in ("set", "clear")
                                           for off, n in ((0x50, 0x14), (0x13A0, 0x18))],
                                    vectors=[(f"${o}", a, b) for o in ("set", "clear") for a, b in _SRC]),
    # 0x10038400: c0..c6 point at a target dword (the store path), c7..c11 have a null back pointer (the je at
    # 0x10038408). The target block is in the state so the zeroing of *back is compared too.
    "Channel_clearBackref": dict(module="sound.dll", addr=0x00038400, abi="thiscall", ret="void", args=["pointer"],
                                 fixture="c3an_chanback",
                                 state=[("$targets", 0, 0x40)] + [(f"$c{i}", 0x13A0, 0x10) for i in range(12)],
                                 vectors=[(f"$c{i}",) for i in range(12)]),
    # 0x1001f150: the list walk. q0 has a null head, q1 a first node without a track record, q5/q9/q11 stop at a
    # null track record in the middle, the rest run 1, 2, 3, 4, 5 and 8 iterations. The track arena is in the state
    # so every flag byte the walk did (and did not) clear is compared.
    "Seq_clearTrackFlag8All": dict(module="sound.dll", addr=0x0001F150, abi="thiscall", ret="void", args=["pointer"],
                                   fixture="c3an_tracklist",
                                   state=[("$tracks", 0, 40 * 0x40)] + [(f"$q{i}", 0x1A0, 0x10) for i in range(12)],
                                   vectors=[(f"$q{i}",) for i in range(12)]),
}

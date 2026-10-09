# Batch c3cl: sound.dll - nine audio functions that no earlier batch was offered (log/c3/c3cl_candidates.tsv):
# the sequencer track finder of the Sound / Seq vtables and its one callee, the open/close inner-device slots
# of the three device base vtables 0x1005b914 / 0x1005b9e4 / 0x1005ba5c, and slots 23 and 27 of the channel
# vtable 0x1005bb90. Module-aware hooks; addr is the RVA, VA = 0x10000000 + RVA.
# Reimplementations in shim/src/re/c3cl.cpp, fixtures in re/frida/js/fixtures.d/c3cl.js.
#
# Every entry only touches sequencers, track lists, device objects, channels and inner sub-objects the fixture
# allocates: no live sound object, device, list, DirectSound interface or MIDI stream is involved, nothing is
# allocated or freed, and no sound.dll module global is read or written. Every vtable the bodies dispatch
# through belongs to a synthetic object; both arms call the same NativeCallback, which records what it was
# handed inside a compared state region.
#
# ONE import is reached, by getPlaybackTime alone: timeGetTime through sound.dll's own slot [0x100b8654]. It
# has no side effect, and every vector of that key puts the comparison the body makes with the result far from
# any attainable clock value (anchor 0x80000000 with limit 0x7fffffff for the expired side, anchor 1 with
# limit 0xfffffff0 for the live side), so the result is decided by the fields and not by the clock.
#
# ret="void" for Track_setField8RaisingMax: the original leaves the ARGUMENT in eax (loaded at 0x1002e430 and
# never replaced) and loads no return value, so its evidence is entirely in the state regions.
#
# HOOKED CALLEES (the verifier must re-run this key on the main build with the hook off; the same run is in
# this batch's notes for instances\c3cl, where both hooks are linked too):
#   Seq_findTrackById -> Track_setField8RaisingMax 0x0002e430 (this batch)
#     SIMGOLF_HOOKS_OFF=sound.dll:2e430
#   the other eight have no callee at all (the leaf rule: each carries at least 12 vectors).
# No hooks.csv row of any module lists one of these nine addresses among its callees apart from that pair, so
# no function already at C3 runs one of these reimplementations.

# --- 0x1002e430: six track elements x six values. The values cross the unsigned compare at 0x1002e437 from
# both sides for every element, and the pair (+0xc = 0x80000000, value = 0x7fffffff) is the one a SIGNED
# compare would resolve the other way.
_TRACK_OBJS = ["$w_zero", "$w_one", "$w_small", "$w_posmax", "$w_sign", "$w_all"]
_TRACK_VALS = [0, 1, 5, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF]
_TRACK = [(o, v) for o in _TRACK_OBJS for v in _TRACK_VALS]
_TRACK_STATE = [(o, 0, 0x40) for o in _TRACK_OBJS]

# --- 0x10023080: nine list shapes x three ids x two values, plus four ids aimed at a particular element
# (the fourth of f_five, the first of f_five, the first duplicate of f_dup and the first of f_three). The two
# values are below and above the element maxima the fixture seeds, so the callee's compare is exercised from
# both sides through this body as well.
_FIND_OBJS = ["$f_gated", "$f_empty", "$f_nullfirst", "$f_one", "$f_three", "$f_five", "$f_dup",
              "$f_nullmid", "$f_miss2"]
_FIND = ([(o, i, v) for o in _FIND_OBJS for i in (33, 7, 99) for v in (0x500, 0x40000000)]
         + [("$f_five", 44, 0x500), ("$f_five", 11, 1), ("$f_dup", 33, 0x1000), ("$f_three", 11, 0)])
_FIND_STATE = [(o, 0, 0x600) for o in _FIND_OBJS]

# --- the five device-base slots. Fourteen blocks; the two with neither an inner device nor a non-zero outer
# code are used by the close keys only, because the open bodies dereference this+0x14 unconditionally once
# the outer slot +4 has returned zero (`mov ecx, [esi+0x14]` at 0x10029fa2 / 0x1002a432).
_DEV_ALL = ["$a_null0", "$a_null1", "$a_nullE", "$a_nullE2", "$a_ok0", "$a_ok1", "$a_ok2", "$a_ok3",
            "$a_fail1", "$a_fail2", "$a_fail3", "$a_fail4", "$a_early", "$a_early2"]
_DEV_OPEN_OBJS = [o for o in _DEV_ALL if o not in ("$a_null0", "$a_null1")]
_DEV_CLOSE = [(o,) for o in _DEV_ALL]
_DEV_OPEN = [(o, a, b) for o in _DEV_OPEN_OBJS for a, b in ((0x11110001, 0x22220002), (0, -1))]
_DEV_STATE = [(o, 0, 0x200) for o in _DEV_ALL]

# --- 0x1002baa0: twelve channels, one vector each (the body takes no stack argument). Four forward to the
# device slot +0x5c, two have bit 4 of +0x58 clear, two have the bit set with a zero anchor, two are past
# their limit and two are inside it.
_TIME_OBJS = ["$t_dev0", "$t_dev1", "$t_dev2", "$t_dev3", "$t_off0", "$t_off1", "$t_zero0", "$t_zero1",
              "$t_past0", "$t_past1", "$t_live0", "$t_live1"]
_TIME = [(o,) for o in _TIME_OBJS]
_TIME_STATE = [(o, 0, 0x200) for o in _TIME_OBJS]

# --- 0x1002c040: six channels x fourteen flag words. The words take every one of the seven tested bits on
# its own (1, 2, 4, 0x10, 0x40, 0x80, 0x100, 0x400), both sides of the two tests that are gated on bit 2
# (0x10 against 0x14 and 0x100 against 0x104) and two words that set everything at once.
_APPLY_OBJS = ["$c_dev0", "$c_dev1", "$c_dev2", "$c_nodev0", "$c_nodev1", "$c_nodev2"]
_APPLY_VALS = [0, 1, 2, 3, 4, 0x10, 0x14, 0x40, 0x80, 0x100, 0x104, 0x400, 0x555, 0xFFFFFFFF]
_APPLY = [(o, v) for o in _APPLY_OBJS for v in _APPLY_VALS]
_APPLY_STATE = [(o, 0, 0x200) for o in _APPLY_OBJS]

HOOKS = {
    # 0x1002e430: the only callee of Seq_findTrackById 0x10023080, a 20-byte leaf; thirty-six vectors.
    "Track_setField8RaisingMax": dict(module="sound.dll", addr=0x0002E430, abi="thiscall", ret="void",
                                      args=["pointer", "uint32"], fixture="c3cl_track",
                                      state=_TRACK_STATE, vectors=_TRACK),
    # 0x10023080: slot 40 (+0xa0) of the Sound vtable 0x1005b000 and of the Seq vtable 0x1005b6a8.
    # Fifty-eight vectors; the state region holds the cursor +0x220 and every element the callee writes.
    "Seq_findTrackById": dict(module="sound.dll", addr=0x00023080, abi="thiscall", ret="int",
                              args=["pointer", "int", "int"], fixture="c3cl_seqfind",
                              state=_FIND_STATE, vectors=_FIND),
    # 0x10029320: slot 5 (+0x14) of the 0x1005b914 table, a leaf; fourteen vectors.
    "VoiceBase_closeInner": dict(module="sound.dll", addr=0x00029320, abi="thiscall", ret="int",
                                 args=["pointer"], fixture="c3cl_dev",
                                 state=_DEV_STATE, vectors=_DEV_CLOSE),
    # 0x1002a000: slot 5 (+0x14) of the 0x1005b9e4 table, the same body at its own address; fourteen vectors.
    "MidiDeviceBase_closeInner": dict(module="sound.dll", addr=0x0002A000, abi="thiscall", ret="int",
                                      args=["pointer"], fixture="c3cl_dev",
                                      state=_DEV_STATE, vectors=_DEV_CLOSE),
    # 0x1002a470: slot 5 (+0x14) of the 0x1005ba5c table, with the extra call to this' own slot +8;
    # fourteen vectors.
    "WaveInDeviceBase_closeInner": dict(module="sound.dll", addr=0x0002A470, abi="thiscall", ret="int",
                                        args=["pointer"], fixture="c3cl_dev",
                                        state=_DEV_STATE, vectors=_DEV_CLOSE),
    # 0x10029f90: slot 4 (+0x10) of the 0x1005b9e4 table, a leaf; twenty-four vectors.
    "MidiDeviceBase_openInner": dict(module="sound.dll", addr=0x00029F90, abi="thiscall", ret="int",
                                     args=["pointer", "int", "int"], fixture="c3cl_dev",
                                     state=_DEV_STATE, vectors=_DEV_OPEN),
    # 0x1002a420: slot 4 (+0x10) of the 0x1005ba5c table, the same sequence without the clear of this+0x14;
    # twenty-four vectors.
    "WaveInDeviceBase_openInner": dict(module="sound.dll", addr=0x0002A420, abi="thiscall", ret="int",
                                       args=["pointer", "int", "int"], fixture="c3cl_dev",
                                       state=_DEV_STATE, vectors=_DEV_OPEN),
    # 0x1002baa0: slot 23 (+0x5c) of the channel vtable 0x1005bb90, a leaf; twelve vectors.
    "getPlaybackTime": dict(module="sound.dll", addr=0x0002BAA0, abi="thiscall", ret="int",
                            args=["pointer"], fixture="c3cl_chan",
                            state=_TIME_STATE, vectors=_TIME),
    # 0x1002c040: slot 27 (+0x6c) of the same table, a leaf; eighty-four vectors. The body always returns 0,
    # so the evidence is the +0x30 / +0x58 fields and the device's record of the forwarded flag word.
    "Channel_applyFlags": dict(module="sound.dll", addr=0x0002C040, abi="thiscall", ret="int",
                               args=["pointer", "uint32"], fixture="c3cl_chan",
                               state=_APPLY_STATE, vectors=_APPLY),
}

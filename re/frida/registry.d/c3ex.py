# C3 batch c3ex (round 32): sound.dll rows of the `other` / `never` classes. Module-aware hooks; addr is the RVA,
# VA = 0x10000000 + RVA. Reimplementations in shim/src/re/c3ex.cpp, fixtures in re/frida/js/fixtures.d/c3ex.js, Purposes and
# per-jcc branch tables in log/c3/c3ex_purpose.md, notes in log/c3/c3ex_notes.txt.
#
# HOOKED CALLEES (the verifier re-runs these keys with the hooks off): Channel_start's callee WaveCmdQueue::pushNode 0xc2f0
# (c3dh) is reached only through its thunk 0x100020b3, which the fixture replaces with a recording stub on the A/B thread, so
# neither arm runs it; SIMGOLF_HOOKS_OFF=sound.dll:c2f0 for the callees-off pass.

_SND = dict(module="sound.dll")
_LOG = ("$log", 0, 0x420)
_IMPLOG = ("$implog", 0, 0x400)
_FREELOG = ("$freelog", 0, 0x404)
_G49A8 = ("$g49a8", 0, 4)
_GL = [("$gdev", 0, 4), _G49A8]

_ARGS12 = [0, 1, 0x1234, 0xFFFFFFFF, 0x80000000, 0x5555, 2, 0x100, 0xDEADBEEF, 0x41, 7, 0x7FFFFFFF]

HOOKS = {}

# A. field stores and a forwarder on fixture objects
HOOKS["setField6cReturn0"] = dict(_SND, addr=0x00039450, abi="thiscall", ret="uint32", args=["pointer", "uint32"],
                                  fixture="c3ex_f6c", state=[(f"$o{i}", 0, 0x100) for i in range(12)],
                                  vectors=[(f"$o{i}", _ARGS12[(i * 5 + 3) % 12]) for i in range(12)])
HOOKS["clearField6cReturn0"] = dict(_SND, addr=0x00039470, abi="thiscall", ret="uint32", args=["pointer"],
                                    fixture="c3ex_f6c", state=[(f"$o{i}", 0, 0x100) for i in range(12)],
                                    vectors=[(f"$o{i}",) for i in range(12)])
HOOKS["Ch5c2ec_forwardSlot37"] = dict(_SND, addr=0x0002F680, abi="thiscall", ret="uint32", args=["pointer"],
                                      fixture="c3ex_fwd37",
                                      state=[(f"$o{i}", 0, 0x100) for i in range(12)]
                                      + [(f"$c{i}", 0, 0x100) for i in range(12)] + [_LOG],
                                      vectors=[(f"$o{i}",) for i in range(12)])
for _k in ("ok", "busy", "null"):
    HOOKS[f"Channel_start_{_k}"] = dict(
        _SND, addr=0x00031080, abi="thiscall", ret="uint32", args=["pointer"], fixture=f"c3ex_chstart_{_k}",
        state=_GL + [_LOG] + [(f"$ch{i}", 0, 0x100) for i in range(12)] + ([("$eng", 0, 0x200)] if _k != "null" else []),
        vectors=[(f"$ch{i}",) for i in range(12)])


# B. channel / device wrappers over create_sound, Engine_playSound and their objects' virtual slots
def _owner(n=12, size=0x200):
    return [(f"$o{i}", 0, size) for i in range(n)] + [(f"$c{i}", 0, 0x200) for i in range(n)] + [_LOG]


_A12B = [0x11, 0x22, 0, 7, 0x33, 0x44, 0, 0x55, 0, 0x66, 0x77, 0xFFFFFFFF]
HOOKS["SubChannel_openSourceType2"] = dict(_SND, addr=0x0002DEB0, abi="thiscall", ret="uint32", args=["pointer", "uint32"],
                                           fixture="c3ex_src2", state=_owner(),
                                           vectors=[(f"$o{i}", _A12B[i]) for i in range(12)])
HOOKS["openSourceType4"] = dict(_SND, addr=0x0002EB40, abi="thiscall", ret="uint32", args=["pointer"] + ["uint32"] * 4,
                                fixture="c3ex_src4", state=_owner(),
                                vectors=[(f"$o{i}", 0x100 + i, 0x200 + i, 0x300 + i, 0x400 + i) for i in range(12)])
for _n, _a in (("openSourceType6", 0x0002F060), ("openSourceType7", 0x0002F5E0)):
    HOOKS[_n] = dict(_SND, addr=_a, abi="thiscall", ret="uint32", args=["pointer"] + ["uint32"] * 3, fixture="c3ex_src3",
                     state=_owner(), vectors=[(f"$o{i}", 0x100 + i, 0x200 + i, 0x300 + i) for i in range(12)])
HOOKS["Device_createStartQueue"] = dict(_SND, addr=0x0002BEE0, abi="thiscall", ret="uint32", args=["pointer"],
                                        fixture="c3ex_startq", state=_owner(), vectors=[(f"$o{i}",) for i in range(12)])
HOOKS["Device_lazyCreateSound"] = dict(_SND, addr=0x0002BD80, abi="thiscall", ret="uint32", args=["pointer"],
                                       fixture="c3ex_lazy", state=_owner(), vectors=[(f"$o{i}",) for i in range(12)])
HOOKS["Device_prepareSoundFlags"] = dict(_SND, addr=0x0002BC60, abi="thiscall", ret="uint32", args=["pointer", "uint32"],
                                         fixture="c3ex_prep", state=_owner(),
                                         vectors=[(f"$o{i}", 0x60 + 3 * i) for i in range(12)])
HOOKS["startDevicePlayback"] = dict(_SND, addr=0x0002B7C0, abi="thiscall", ret="uint32", args=["pointer"],
                                    fixture="c3ex_devplay", state=_owner() + [_IMPLOG],
                                    vectors=[(f"$o{i}",) for i in range(12)])
_GSTOP_MEMBERS = {2: [0, 1], 3: [0, 2], 4: [0], 7: [0, 1], 9: [0, 1, 2, 3], 11: [0, 1]}
HOOKS["SoundGroup_stop"] = dict(_SND, addr=0x00039B30, abi="thiscall", ret="uint32", args=["pointer"], fixture="c3ex_gstop",
                                state=[(f"$o{i}", 0, 0x1500) for i in range(12)]
                                + [(f"$m{i}_{j}", 0, 0x1500) for i, js in _GSTOP_MEMBERS.items() for j in js] + [_LOG],
                                vectors=[(f"$o{i}",) for i in range(12)])
HOOKS["Channel_setVolume"] = dict(_SND, addr=0x00035150, abi="thiscall", ret="void", args=["pointer", "uint32"],
                                  fixture="c3ex_chvol", state=[(f"$ch{i}", 0, 0x1500) for i in range(12)] + [_LOG],
                                  vectors=[(f"$ch{i}", _ARGS12[(i * 7 + 1) % 12]) for i in range(12)])
HOOKS["Channel_set3DBufferStub"] = dict(_SND, addr=0x00037C00, abi="thiscall", ret="uint32", args=["pointer", "uint32"],
                                        fixture="c3ex_3dstub", state=[(f"$ch{i}", 0, 0x100) for i in range(12)] + [_LOG],
                                        vectors=[(f"$ch{i}", _ARGS12[(i * 5 + 2) % 12]) for i in range(12)])
HOOKS["DsBuffer::getPosition"] = dict(_SND, addr=0x00010660, abi="thiscall", ret="uint32", args=["pointer"] + ["uint32"] * 3,
                                      fixture="c3ex_dspos",
                                      state=[(f"$s{i}", 0, 0x100) for i in range(12)] + [(f"$d{i}", 0, 0x40) for i in range(12)]
                                      + [_LOG],
                                      vectors=[(f"$s{i}", 0x1000 + i, 0x2000 * (i + 1), _ARGS12[i]) for i in range(12)])
HOOKS["Device::serviceVoices"] = dict(_SND, addr=0x0000A050, abi="thiscall", ret="uint32", args=["pointer"], fixture="c3ex_svc",
                                      state=[(f"$d{i}", 0, 0x100) for i in range(12)] + [_LOG],
                                      vectors=[(f"$d{i}",) for i in range(12)])
HOOKS["Sound::ctor"] = dict(_SND, addr=0x00003F30, abi="thiscall", ret="pointer", args=["pointer"], fixture="c3ex_sctor",
                            state=[(f"$s{i}", 0, 0xB00) for i in range(12)] + [_LOG],
                            vectors=[(f"$s{i}",) for i in range(12)])
for _k in ("eng", "null"):
    HOOKS[f"enqueueWaveCmdE_{_k}"] = dict(
        _SND, addr=0x0002FB70, abi="thiscall", ret="uint32", args=["pointer"] + ["uint32"] * 3, fixture=f"c3ex_enqe_{_k}",
        state=_GL + [_LOG, ("$arena", 0, 0x410), ("$eng", 0, 0x100)] + [(f"$o{i}", 0, 0x40) for i in range(12)],
        vectors=[(f"$o{i}", 0xA100 + i, 0xA200 + i, _ARGS12[i]) for i in range(12)])
_P12 = [0, 0x1000, 0, 0x2000, 0x3000, 0, 0x4000, 0x5000, 0, 0x6000, 0x7000, 0xFFFFFFFF]
HOOKS["Track_setData"] = dict(_SND, addr=0x0001E9A0, abi="thiscall", ret="uint32", args=["pointer", "uint32"],
                              fixture="c3ex_track", state=[(f"$t{i}", 0, 0x200) for i in range(12)] + [_FREELOG, _LOG],
                              vectors=[(f"$t{i}", _P12[i]) for i in range(12)])
_HWND12 = [0x10010, 0, 0x2002, 0x3003, 0, 0x4004, 0x5005, 0x6006, 0x5A5A0000, 0x7007, 0x8008, 0xFFFFFFFF]
HOOKS["SoundWindowObj_subclass"] = dict(_SND, addr=0x000286F0, abi="thiscall", ret="uint32", args=["pointer", "uint32"],
                                        fixture="c3ex_subclass", state=[(f"$w{i}", 0, 0x40) for i in range(12)] + [_IMPLOG],
                                        vectors=[(f"$w{i}", _HWND12[i]) for i in range(12)])
# C. DirectSound buffer list (head 0x100b4a34 / tail 0x100b4a38 swapped for the key)
def _dsstate(n, extra=False):
    st = [("$ghead", 0, 4), ("$gtail", 0, 4), _G49A8, _LOG, _FREELOG, ("$nb", 0, 0x200)]
    st += [(f"$b{j}", 0, 0x200) for j in range(n)] + [(f"$d{i}", 0, 0x200) for i in range(12)]
    if extra:
        st += [(f"$x{i}", 0, 0x200) for i in range(12) if i % 3]
    return st


for _n in (0, 1, 4):
    HOOKS[f"DirectSoundDevice::releaseAllBuffers_{_n}"] = dict(
        _SND, addr=0x000133D0, abi="thiscall", ret="uint32", args=["pointer"], fixture=f"c3ex_dsrel_{_n}",
        state=_dsstate(_n), vectors=[(f"$d{i}",) for i in range(12)])
HOOKS["DirectSoundDevice::removeBuffer_4"] = dict(
    _SND, addr=0x000132E0, abi="thiscall", ret="uint32", args=["pointer", "pointer"], fixture="c3ex_dsrm_4", state=_dsstate(4),
    vectors=[("$d1", "$b0"), ("$d2", "$b2"), ("$d5", "$b3"), ("$d6", "$b1"), ("$d1", "$nb"), ("$d2", 0), ("$d0", "$b1"),
             ("$d3", "$b0"), ("$d4", "$b1"), ("$d7", "$b3"), ("$d9", "$nb"), ("$d11", "$b2")])
HOOKS["DirectSoundDevice::removeBuffer_1"] = dict(
    _SND, addr=0x000132E0, abi="thiscall", ret="uint32", args=["pointer", "pointer"], fixture="c3ex_dsrm_1", state=_dsstate(1),
    vectors=[("$d1", "$b0"), ("$d2", "$nb"), ("$d5", 0), ("$d0", "$b0"), ("$d3", "$b0"), ("$d4", "$nb"), ("$d6", "$b0"),
             ("$d7", "$nb"), ("$d8", "$b0"), ("$d9", "$b0"), ("$d10", "$nb"), ("$d11", 0)])
for _n in (0, 3):
    HOOKS[f"DirectSoundDevice::releaseAll_{_n}"] = dict(
        _SND, addr=0x00011970, abi="thiscall", ret="uint32", args=["pointer"], fixture=f"c3ex_dsall_{_n}",
        state=_dsstate(_n, True), vectors=[(f"$d{i}",) for i in range(12)])
# D. MultiSound start paths
def _msstate(eng=True):
    return _GL + [_LOG] + [(f"$m{i}", 0, 0x1500) for i in range(12)] + [(f"$out{i}", 0, 0x10) for i in range(12)] + \
        ([("$eng", 0, 0x200)] if eng else [])


HOOKS["MultiSound::startVoice"] = dict(_SND, addr=0x00039F30, abi="thiscall", ret="uint32", args=["pointer", "pointer"],
                                       fixture="c3ex_msvoice", state=_msstate(),
                                       vectors=[(f"$m{i}", f"$out{i}") for i in range(12)])
for _k in ("ok", "busy", "null"):
    HOOKS[f"MultiSound_start_{_k}"] = dict(_SND, addr=0x00039CE0, abi="thiscall", ret="uint32", args=["pointer"],
                                           fixture=f"c3ex_msstart_{_k}", state=_msstate(_k != "null"),
                                           vectors=[(f"$m{i}",) for i in range(12)])
# E. sequencer track walks
def _tracks(spec):
    """spec: {sequencer index: [table slots that hold a track]} -> state regions of the sequencers and their tracks"""
    st = []
    for i, js in spec.items():
        st.append((f"$s{i}", 0, 0x800))
        st += [(f"$t{i}_{j}", 0, 0x300) for j in js]
    return st


HOOKS["Seq_stepTrackMeta"] = dict(_SND, addr=0x00020720, abi="thiscall", ret="uint32", args=["pointer", "pointer"],
                                  fixture="c3ex_stepmeta",
                                  state=[("$s0", 0, 0x800), _LOG] + [(f"$k{j}", 0, 0x300) for j in range(12)],
                                  vectors=[("$s0", 0)] + [("$s0", f"$k{j}") for j in range(1, 12)])
HOOKS["Seq_preRollTrack"] = dict(_SND, addr=0x000205F0, abi="thiscall", ret="uint32", args=["pointer", "uint32"],
                                 fixture="c3ex_preroll", state=_tracks({0: [0, 1, 2, 4, 5, 6, 7]}) + [_LOG],
                                 vectors=[("$s0", i) for i in range(12)])
_SEL = {0: [0, 1], 1: [0, 1], 2: [0, 1], 3: [0], 4: [0], 5: [0, 1], 6: [0, 1], 7: [0, 1], 8: [0, 1], 9: [0, 1], 10: [1],
        11: list(range(8))}
HOOKS["Seq_selectTrack"] = dict(_SND, addr=0x00024500, abi="thiscall", ret="uint32", args=["pointer", "uint32", "uint32"],
                                fixture="c3ex_seltrack", state=_tracks(_SEL) + [_LOG],
                                vectors=[(f"$s{i}", 0xDA7A0000 + i, 7 if i == 11 else 1) for i in range(12)])
_LOADT = {0: [0, 1, 2], 1: [0, 1], 2: [], 3: [0], 4: [], 5: [0, 1, 3], 6: [0, 1], 7: [0, 1, 2], 8: [1], 9: [], 10: list(range(8)),
          11: [0]}
HOOKS["Seq::loadFile"] = dict(_SND, addr=0x00020290, abi="thiscall", ret="uint32", args=["pointer", "uint32"],
                              fixture="c3ex_seqload", state=_tracks(_LOADT) + [_LOG],
                              vectors=[(f"$s{i}", 0xF11E0000 + i) for i in range(12)])
HOOKS["Midi_loadMemoryAndPreRoll"] = dict(_SND, addr=0x00020360, abi="thiscall", ret="uint32", args=["pointer", "uint32", "uint32"],
                                          fixture="c3ex_seqload", state=_tracks(_LOADT) + [_LOG],
                                          vectors=[(f"$s{i}", 0x3E300000 + i, 0xA2 + i) for i in range(12)])
HOOKS["startSignalThread"] = dict(_SND, addr=0x0002A920, abi="thiscall", ret="uint32", args=["pointer"], fixture="c3ex_sigthread",
                                  state=[(f"$t{i}", 0, 0x40) for i in range(12)] + [_IMPLOG, _LOG],
                                  vectors=[(f"$t{i}",) for i in range(12)])

# C3 batch c3dj (round 26): sound.dll rows earlier batches dropped before the heap stand-in (realloc, scheduled failures), the
# import stand-in and the freeze helper existed (log/c3/c3dj_candidates.tsv). Module-aware hooks; addr is the RVA, VA =
# 0x10000000 + RVA. Reimplementations in shim/src/re/c3dj.cpp, fixtures in re/frida/js/fixtures.d/c3dj.js, Purposes and
# per-jcc branch tables in log/c3/c3dj_purpose.md, notes in log/c3/c3dj_notes.txt.
#
# HOOKED CALLEES (the verifier re-runs these keys with the hooks off):
#   inside this batch: see log/c3/c3dj_notes.txt section C

_SND_DLL = dict(module="sound.dll")

HOOKS = {}

_O12 = [(f"$o{i}", 0, 0x100) for i in range(12)]
_V12 = [(f"$o{i}",) for i in range(12)]

# ---------------------------------------------------------------------------------------------------------------------
# A. vtable-store leaves, zeroing leaves
# ---------------------------------------------------------------------------------------------------------------------
for _name, _addr in (("initCtrlVtable", 0x00005720), ("CtrlObject_dtorA", 0x0001C560), ("CtrlObject_dtorB", 0x0001C710),
                     ("CtrlObject_dtorC", 0x0001CBD0), ("CtrlObject_dtorD", 0x0001CCD0), ("pairClear", 0x00008840)):
    HOOKS[_name] = dict(_SND_DLL, addr=_addr, abi="thiscall", ret="void", args=["pointer"], fixture="c3dj_objs12",
                        state=_O12, vectors=_V12)
for _name, _addr in (("listNode3Init", 0x000087F0), ("PtrList_init", 0x0001D130), ("CtrlObject::ctor", 0x0001BC50),
                     ("CtrlObject2_ctor", 0x0001BCA0), ("Ctrl5b668_ctor", 0x0001CBF0), ("CtrlObject3_ctor", 0x0001CAF0),
                     ("ChannelDesc_ctor", 0x00024460)):
    HOOKS[_name] = dict(_SND_DLL, addr=_addr, abi="thiscall", ret="pointer", args=["pointer"], fixture="c3dj_objs12",
                        state=_O12, vectors=_V12)

HOOKS["CommandQueue::clear"] = dict(_SND_DLL, addr=0x00008890, abi="thiscall", ret="uint32", args=["pointer"],
                                    fixture="c3dj_cmdclr", state=[(f"$o{i}", 0, 0x4010) for i in range(10)],
                                    vectors=[(f"$o{i}",) for i in range(10)])

# ---------------------------------------------------------------------------------------------------------------------
# B. Ctrl5b5c0_transposeTable: six tables x the ten legal octave counts plus the error arm
# ---------------------------------------------------------------------------------------------------------------------
_TBL_N = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
_TBL_BAD = [10, 11, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF]
HOOKS["Ctrl5b5c0_transposeTable"] = dict(
    _SND_DLL, addr=0x0001C1E0, abi="thiscall", ret="uint32", args=["pointer", "uint32"], fixture="c3dj_tbl",
    state=[(f"$o{i}", 0, 0x100) for i in range(6)],
    vectors=[(f"$o{i}", n) for i in range(6) for n in _TBL_N] + [("$o1", n) for n in _TBL_BAD] + [("$o4", 10)])

# ---------------------------------------------------------------------------------------------------------------------
# C. device-base destructor bodies and the scalar-deleting wrappers (operator delete recorded by the heap stand-in)
# ---------------------------------------------------------------------------------------------------------------------
_DEV = [(f"$d{i}", 0, 0x40) for i in range(10)] + [(f"$i{k}", 0, 0x40) for k in range(1, 8)]
_FREELOG = ("$freelog", 0, 0x404)
for _name, _addr in (("DeviceBase_dtorA", 0x00028FF0), ("DeviceBase_dtorB", 0x00029F60), ("DeviceBase_dtorC", 0x0002A3B0),
                     ("Wave_In_Device::dtor", 0x0000E8F0)):
    HOOKS[_name] = dict(_SND_DLL, addr=_addr, abi="thiscall", ret="void", args=["pointer"], fixture="c3dj_dev",
                        state=_DEV, vectors=[(f"$d{i}",) for i in range(10)])

_FLAGS = [0, 1, 2, 3, 0x100, 0x101, 0xFFFFFF00, 0xFFFFFFFF, 0x80000000, 0x80000001]
for _name, _addr in (("DeviceBase_scalarDtor", 0x00028FB0), ("ctrlScalarDeletingDtor", 0x00005740),
                     ("cmdNode::scalarDeletingDtor", 0x0000CC40), ("Wave_In_Device::scalarDeletingDtor", 0x0000E8C0),
                     ("CtrlObject_scalarDtorB", 0x0001C6E0), ("CtrlObject_scalarDtorC", 0x0001CBA0),
                     ("CtrlObject_scalarDtorD", 0x0001CCA0), ("MidiDeviceBase_scalarDtor", 0x00029F30),
                     ("WaveInDeviceBase_scalarDtor", 0x0002A380)):
    HOOKS[_name] = dict(_SND_DLL, addr=_addr, abi="thiscall", ret="pointer", args=["pointer", "uint32"],
                        fixture="c3dj_devfree", state=_DEV + [_FREELOG],
                        vectors=[(f"$d{i}", f) for i in (0, 1, 3, 8, 9) for f in _FLAGS])

HOOKS["MemBlock_deleteDtor"] = dict(_SND_DLL, addr=0x00038AE0, abi="thiscall", ret="pointer", args=["pointer", "uint32"],
                                    fixture="c3dj_memblk", state=[(f"$o{i}", 0, 0x10) for i in range(8)] + [_FREELOG],
                                    vectors=[(f"$o{i}", f) for i in range(8) for f in (0, 1, 2, 3, 0x100, 0xFFFFFFFF)])

# ---------------------------------------------------------------------------------------------------------------------
# D. the channel/voice destructor family (operator delete 0x4249a and delete_sound 0x97d0 recorded by two heap stand-ins; the
#    module's active-list head / tail globals 0x100b5070 / 0x100b506c are state)
# ---------------------------------------------------------------------------------------------------------------------
_DSLOG = ("$dslog", 0, 0x404)
_CHAN = ([(f"$c{i}", 0, 0x120) for i in range(12)] + [(f"$n{i}", 0, 0x60) for i in range(4)] +
         [("$gHead", 0, 4), ("$gTail", 0, 4), _FREELOG, _DSLOG])
_CHAN_V = _CHAN + [(f"$s{i}", 0, 0x20) for i in range(12) if i in (1, 2, 4, 6, 8, 10)]

for _name, _addr, _fx in (("SubChannel_dtor", 0x0002AC00, "c3dj_chan_plain"), ("Ch5bb90_dtor", 0x0002B620, "c3dj_chan_plain"),
                          ("destroyMixerVoices", 0x0002C5E0, "c3dj_chan_plain"), ("Mixer_dtor", 0x0002CB00, "c3dj_chan_plain"),
                          ("Ch5be88_dtorA", 0x0002D560, "c3dj_chan_plain"), ("Ch5be88_dtorB", 0x0002E660, "c3dj_chan_plain"),
                          ("Ch5c178_dtor", 0x0002EA30, "c3dj_chan_virt"), ("Ch5c234_dtor", 0x0002EF40, "c3dj_chan_virt"),
                          ("Ch5c2ec_dtor", 0x0002F3F0, "c3dj_chan_plain")):
    HOOKS[_name] = dict(_SND_DLL, addr=_addr, abi="thiscall", ret="void", args=["pointer"], fixture=_fx,
                        state=_CHAN_V if _fx.endswith("virt") else _CHAN, vectors=[(f"$c{i}",) for i in range(12)])

HOOKS["unlinkFromActiveList"] = dict(_SND_DLL, addr=0x0002B090, abi="thiscall", ret="uint32", args=["pointer"],
                                     fixture="c3dj_chan_plain", state=_CHAN, vectors=[(f"$c{i}",) for i in range(12)])

_CFLAGS = [0, 1, 2, 0x101]
for _name, _addr, _fx in (("SubChannel_scalarDtor", 0x0002ABD0, "c3dj_chan_plain"), ("Ch5bb90_scalarDtor", 0x0002B5F0, "c3dj_chan_plain"),
                          ("MixerVoiceArray_scalarDtor", 0x0002C5B0, "c3dj_chan_plain"), ("Mixer_scalarDtor", 0x0002CAD0, "c3dj_chan_plain"),
                          ("Ch5be88_scalarDtorA", 0x0002D530, "c3dj_chan_plain"), ("Ch5be88_scalarDtorB", 0x0002E630, "c3dj_chan_plain"),
                          ("Ch5c178_scalarDtor", 0x0002EA00, "c3dj_chan_virt"), ("Ch5c234_scalarDtor", 0x0002EF10, "c3dj_chan_virt"),
                          ("Ch5c2ec_scalarDtor", 0x0002F3C0, "c3dj_chan_plain")):
    HOOKS[_name] = dict(_SND_DLL, addr=_addr, abi="thiscall", ret="pointer", args=["pointer", "uint32"], fixture=_fx,
                        state=_CHAN_V if _fx.endswith("virt") else _CHAN,
                        vectors=[(f"$c{i}", f) for i in (0, 1, 2, 3, 4, 5, 6, 9, 10, 11) for f in _CFLAGS])

# ---------------------------------------------------------------------------------------------------------------------
# E. DsBuffer / Ds3DBuffer thin methods over COM-shaped interfaces (fixture objects with recording stdcall stubs)
# ---------------------------------------------------------------------------------------------------------------------
def _dsstate(n):
    return ([(f"$d{i}", 0, 0x100) for i in range(n)] +
            [(f"${p}{i}", 0, 0x300) for i in range(n) for p in "abec"])


_F3 = [0x3F800000, 0xC0490FDB, 0, 0x7FC00000, 0xFFFFFFFF]
_U4 = [(0, 0, 0, 0), (0x1000, 0x20, 0x2000, 0x30), (0xFFFFFFFF, 0x80000000, 0x7FFFFFFF, 1)]
_XYZ = [(0x3F800000, 0x40000000, 0x40400000), (0, 0xBF800000, 0x80000000), (0xFFFFFFFF, 0x7FC00000, 0x12345678)]

HOOKS["DsBuffer::stop"] = dict(_SND_DLL, addr=0x0000F760, abi="thiscall", ret="uint32", args=["pointer"],
                               fixture="c3dj_ds_stop", state=_dsstate(12), vectors=[(f"$d{i}",) for i in range(12)])
HOOKS["DsBuffer::unlock"] = dict(_SND_DLL, addr=0x00010370, abi="thiscall", ret="uint32",
                                 args=["pointer", "uint32", "uint32", "uint32", "uint32"], fixture="c3dj_ds_unlock",
                                 state=_dsstate(12), vectors=[(f"$d{i}",) + u for i in range(12) for u in _U4])
HOOKS["DsBuffer::isBusy"] = dict(_SND_DLL, addr=0x00010E30, abi="thiscall", ret="uint32", args=["pointer"],
                                 fixture="c3dj_ds_busy", state=_dsstate(14), vectors=[(f"$d{i}",) for i in range(14)])
HOOKS["DsBuffer::queryNotify"] = dict(_SND_DLL, addr=0x0000FF60, abi="thiscall", ret="uint32", args=["pointer"],
                                      fixture="c3dj_ds_qnotify", state=_dsstate(12), vectors=[(f"$d{i}",) for i in range(12)])
HOOKS["DsBuffer::query3D"] = dict(_SND_DLL, addr=0x0000FEF0, abi="thiscall", ret="uint32", args=["pointer"],
                                  fixture="c3dj_ds_q3d", state=_dsstate(13), vectors=[(f"$d{i}",) for i in range(13)])
for _name, _addr in (("Ds3DBuffer::setPositionX", 0x00011070), ("Ds3DBuffer::setPositionY", 0x000110C0),
                     ("Ds3DBuffer::setPositionZ", 0x00011110)):
    HOOKS[_name] = dict(_SND_DLL, addr=_addr, abi="thiscall", ret="uint32", args=["pointer", "uint32"],
                        fixture="c3dj_ds_3d", state=_dsstate(8), vectors=[(f"$d{i}", v) for i in range(8) for v in _F3])
HOOKS["Ds3DBuffer::setPosition"] = dict(_SND_DLL, addr=0x00011000, abi="thiscall", ret="uint32",
                                        args=["pointer", "uint32", "uint32", "uint32"], fixture="c3dj_ds_3d",
                                        state=_dsstate(8), vectors=[(f"$d{i}",) + t for i in range(8) for t in _XYZ])
HOOKS["DsBuffer::setPosition"] = dict(_SND_DLL, addr=0x0000F790, abi="thiscall", ret="uint32", args=["pointer"],
                                      fixture="c3dj_ds_setpos", state=_dsstate(12), vectors=[(f"$d{i}",) for i in range(12)])
HOOKS["DsBuffer::destroy"] = dict(_SND_DLL, addr=0x0000EF60, abi="thiscall", ret="void", args=["pointer"],
                                  fixture="c3dj_ds_rel", state=_dsstate(12), vectors=[(f"$d{i}",) for i in range(12)])
HOOKS["DsBuffer::release"] = dict(_SND_DLL, addr=0x0000F8F0, abi="thiscall", ret="uint32", args=["pointer"],
                                  fixture="c3dj_ds_rel", state=_dsstate(12), vectors=[(f"$d{i}",) for i in range(12)])
HOOKS["DsBuffer::releaseIfOwned"] = dict(_SND_DLL, addr=0x00010060, abi="thiscall", ret="uint32", args=["pointer"],
                                         fixture="c3dj_ds_rel", state=_dsstate(12), vectors=[(f"$d{i}",) for i in range(12)])
HOOKS["DsBuffer::scalarDeletingDtor"] = dict(
    _SND_DLL, addr=0x00012B80, abi="thiscall", ret="pointer", args=["pointer", "uint32"], fixture="c3dj_ds_rel_free",
    state=_dsstate(12) + [_FREELOG], vectors=[(f"$d{i}", f) for i in range(12) for f in (0, 1, 0x100, 0xFFFFFFFF)])

# ---------------------------------------------------------------------------------------------------------------------
# F. small forwarders (closeDecoder, Ctrl_appendIfAccepted, DsDevice_registerBuffer, addToSoundList)
# ---------------------------------------------------------------------------------------------------------------------
_ARENA = ("$arena", 0, 0x110)

HOOKS["closeDecoder"] = dict(_SND_DLL, addr=0x00032640, abi="thiscall", ret="uint32", args=["pointer"], fixture="c3dj_closedec",
                             state=_dsstate(12) + [(f"$x{i}", 0, 0x80) for i in range(12)],
                             vectors=[(f"$x{i}",) for i in range(12)])
HOOKS["Ctrl_appendIfAccepted"] = dict(_SND_DLL, addr=0x0001B9A0, abi="thiscall", ret="uint32", args=["pointer", "uint32"],
                                      fixture="c3dj_ctrlapp", state=[(f"$o{i}", 0, 0x100) for i in range(10)] + [_ARENA],
                                      vectors=[(f"$o{i}", a) for i in range(10) for a in (0, 0x1234, 0xFFFFFFFF)])
HOOKS["DsDevice_registerBuffer"] = dict(
    _SND_DLL, addr=0x00032410, abi="thiscall", ret="uint32", args=["pointer", "pointer"], fixture="c3dj_dsreg",
    state=[(f"$o{i}", 0, 0x1400) for i in range(6)] + [(f"$b{i}", 0, 0x40) for i in range(4)] + [_ARENA],
    vectors=[(f"$o{i}", f"$b{j}") for i in range(6) for j in range(4)] + [(f"$o{i}", 0) for i in range(6)])
HOOKS["addToSoundList"] = dict(
    _SND_DLL, addr=0x0000E120, abi="thiscall", ret="uint32", args=["pointer", "pointer"], fixture="c3dj_addsl",
    state=[(f"$o{i}", 0, 0x200) for i in range(6)] + [(f"$s{i}", 0, 0x100) for i in range(6)] + [_ARENA],
    vectors=[(f"$o{i}", f"$s{j}") for i in range(6) for j in range(6)])

# ---------------------------------------------------------------------------------------------------------------------
# G. Voice_requestStop / Voice_requestStopDrain (g_midiDevice 0x100b4a1c swapped for a fixture queue under the frozen thread)
# ---------------------------------------------------------------------------------------------------------------------
_VSTATE = [(f"$v{i}", 0, 0x100) for i in range(12)] + [("$q", 0, 0x4060), _ARENA]
for _name, _addr in (("Voice_requestStop", 0x00020EA0), ("Voice_requestStopDrain", 0x00020EE0)):
    for _mode in ("q", "full", "null"):
        HOOKS[f"{_name}_{_mode}"] = dict(_SND_DLL, addr=_addr, abi="thiscall", ret="uint32", args=["pointer"],
                                         fixture=f"c3dj_vreq_{_mode}", state=_VSTATE,
                                         vectors=[(f"$v{i}",) for i in range(12)])

# ---------------------------------------------------------------------------------------------------------------------
# H. node lists cleared from the front (operator delete recorded by the heap stand-in); I. Channel_set3DPosition
# ---------------------------------------------------------------------------------------------------------------------
_LST = [(f"$l{i}", 0, 0x100) for i in range(12)] + [_FREELOG]
for _name, _addr, _fx in (("PtrList_freeNodesB", 0x00031AC0, "c3dj_lists0"), ("nodeListClear2", 0x000384B0, "c3dj_lists0"),
                          ("nodeListClear", 0x00039580, "c3dj_lists0"), ("VoiceBase_clearList", 0x00029170, "c3dj_lists8")):
    HOOKS[_name] = dict(_SND_DLL, addr=_addr, abi="thiscall", ret="void", args=["pointer"], fixture=_fx, state=_LST,
                        vectors=[(f"$l{i}",) for i in range(12)])

HOOKS["Channel_set3DPosition"] = dict(
    _SND_DLL, addr=0x00037B00, abi="thiscall", ret="uint32", args=["pointer", "uint32", "uint32", "uint32"], fixture="c3dj_ch3d",
    state=_dsstate(8) + [(f"$ch{i}", 0, 0x80) for i in range(10)],
    vectors=[(f"$ch{i}",) + t for i in range(10) for t in _XYZ])

# ---------------------------------------------------------------------------------------------------------------------
# J. object-held lists, name copies and virtual forwarders
# ---------------------------------------------------------------------------------------------------------------------
_ARENA4 = ("$arena", 0, 0x410)

HOOKS["VoiceBase_openInner"] = dict(
    _SND_DLL, addr=0x000292D0, abi="thiscall", ret="uint32", args=["pointer", "uint32", "uint32"], fixture="c3dj_vopen",
    state=[(f"$v{i}", 0, 0x40) for i in range(10)] + [(f"$s{i}", 0, 0x40) for i in range(10)],
    vectors=[(f"$v{i}", a, b) for i in range(10) for a, b in ((0, 0), (0x1111, 0x2222), (0xFFFFFFFF, 0x80000000))])

_SEQ_OBJS = [(f"$o{i}{j}", 0, 0x20) for i in range(10) for j in range(4)]
HOOKS["Seq_freeChannelObjects"] = dict(
    _SND_DLL, addr=0x0001ABD0, abi="thiscall", ret="void", args=["pointer"], fixture="c3dj_seqfree",
    state=[(f"$q{i}", 0, 0x210) for i in range(10)] + _SEQ_OBJS + [("$ord", 0, 0x40), _FREELOG],
    vectors=[(f"$q{i}",) for i in range(10)])

_ANY_CH = {1: [0], 2: [0], 3: [0, 1], 4: [0], 5: [0], 6: [0], 7: [0, 1], 8: [0, 1], 9: [0, 1, 2], 10: [0, 1, 2]}
_ANY_EL = {(4, 0): [0, 1], (5, 0): [1, 2], (6, 0): [0], (8, 1): [0], (9, 0): [0], (9, 2): [0], (10, 0): [0, 1, 2, 3]}
HOOKS["Sound_anyChannelActive"] = dict(
    _SND_DLL, addr=0x0001AD30, abi="thiscall", ret="uint8", args=["pointer"], fixture="c3dj_anyactive",
    state=[(f"$z{i}", 0, 0x210) for i in range(11)] +
          [(f"$c{i}_{j}", 0x1430, 0xD0) for i, js in _ANY_CH.items() for j in js] +
          [(f"$e{i}_{j}_{k}", 0, 0x20) for (i, j), ks in _ANY_EL.items() for k in ks],
    vectors=[(f"$z{i}",) for i in range(11)])

_FLST = [(f"$l{i}", 0, 0x100) for i in range(10)] + [_FREELOG]
for _name, _addr in (("findAndUnlinkNode", 0x000136C0), ("findAndUnlinkNodeB", 0x0000E700)):
    HOOKS[_name] = dict(_SND_DLL, addr=_addr, abi="thiscall", ret="uint32", args=["pointer", "uint32"],
                        fixture="c3dj_findlists", state=_FLST,
                        vectors=[(f"$l{i}", v) for i in range(10) for v in (11, 22, 33, 44, 0, 5, 99)])

_NAMES = [(f"$n{i}", 0, 0x220) for i in range(4)] + [_ARENA4, _FREELOG]
for _name, _addr, _null in (("Object_setName", 0x0002BB10, True), ("Sound_assignName", 0x0002DDF0, True),
                            ("Voice_setName", 0x0002D240, False), ("Sound_setAltName", 0x0001D0A0, False)):
    _vecs = [(f"$n{i}", f"$t{j}") for i in range(4) for j in range(9)]
    if _null:
        _vecs += [(f"$n{i}", 0) for i in range(4)]
    HOOKS[_name] = dict(_SND_DLL, addr=_addr, abi="thiscall", ret="uint32", args=["pointer", "pointer"],
                        fixture="c3dj_names", state=_NAMES, vectors=_vecs)
# a NULL string: Sound_setAltName leaves eax undefined on that path, so only the memory is compared
HOOKS["Sound_setAltName_null"] = dict(_SND_DLL, addr=0x0001D0A0, abi="thiscall", ret="void", args=["pointer", "pointer"],
                                      fixture="c3dj_names", state=_NAMES, vectors=[(f"$n{i}", 0) for i in range(4)])

_CHREL_Q = [1, 2, 3, 4, 6, 7]
HOOKS["Channel_releaseSound"] = dict(
    _SND_DLL, addr=0x0002DFF0, abi="thiscall", ret="uint32", args=["pointer"], fixture="c3dj_chrel",
    state=[(f"$h{i}", 0, 0x60) for i in range(8)] + [(f"$q{i}", 0, 0x20) for i in _CHREL_Q] + [_DSLOG],
    vectors=[(f"$h{i}",) for i in range(8)])

HOOKS["unlinkDsBuffer"] = dict(
    _SND_DLL, addr=0x000134E0, abi="thiscall", ret="uint32", args=["pointer", "pointer"], fixture="c3dj_unlds",
    state=[(f"$u{i}", 0, 0x100) for i in range(10)] + [("$gHead", 0, 4), ("$gTail", 0, 4)],
    vectors=[(0, f"$u{i}") for i in range(10)] + [(0, 0)])

# ---------------------------------------------------------------------------------------------------------------------
# K. VoiceBase_dtor, Midi_Device::dtor and their scalar-deleting wrappers (operator delete recorded by the heap stand-in)
# ---------------------------------------------------------------------------------------------------------------------
_VB = [(f"$w{i}", 0, 0x400) for i in range(8)] + [(f"$s{i}", 0, 0x40) for i in (1, 2, 4, 6)] + [_FREELOG]
_MD = [(f"$m{i}", 0, 0x4060) for i in range(8)] + [(f"$s{i}", 0, 0x40) for i in (1, 3, 5, 7)] + [_FREELOG]
HOOKS["VoiceBase_dtor"] = dict(_SND_DLL, addr=0x00029200, abi="thiscall", ret="void", args=["pointer"], fixture="c3dj_vbase",
                               state=_VB, vectors=[(f"$w{i}",) for i in range(8)])
HOOKS["VoiceBase_scalarDtor"] = dict(_SND_DLL, addr=0x00029140, abi="thiscall", ret="pointer", args=["pointer", "uint32"],
                                     fixture="c3dj_vbase", state=_VB,
                                     vectors=[(f"$w{i}", f) for i in range(8) for f in (0, 1, 0x101)])
HOOKS["Midi_Device::dtor"] = dict(_SND_DLL, addr=0x00009CB0, abi="thiscall", ret="void", args=["pointer"], fixture="c3dj_midi",
                                  state=_MD, vectors=[(f"$m{i}",) for i in range(8)])
HOOKS["Midi_Device::scalarDeletingDtor"] = dict(_SND_DLL, addr=0x00009C80, abi="thiscall", ret="pointer",
                                                args=["pointer", "uint32"], fixture="c3dj_midi", state=_MD,
                                                vectors=[(f"$m{i}", f) for i in range(8) for f in (0, 1, 0x101)])

# ---------------------------------------------------------------------------------------------------------------------
# L. constructors of the channel / voice / device-base objects (Sound_setSourceType 0x2ad10 and CommandQueue::init 0x8860 run as callees)
# ---------------------------------------------------------------------------------------------------------------------
for _name, _addr, _size, _fx in (("SubChannel_ctor", 0x0002B4E0, 0x120, "c3dj_objs12_120"), ("Ch5bb90_ctor", 0x0002C490, 0x120, "c3dj_objs12_120"),
                                 ("Mixer_ctor", 0x0002C9F0, 0x120, "c3dj_objs12_120"), ("Ch5be88_ctor", 0x0002D450, 0x120, "c3dj_objs12_120"),
                                 ("Ch5c178_ctor", 0x0002E930, 0x120, "c3dj_objs12_120"), ("Ch5c3a4_ctor", 0x0002F740, 0x120, "c3dj_objs12_120"),
                                 ("Ctrl5b5c0_ctor", 0x0001BD90, 0x120, "c3dj_objs12_120"), ("MidiDeviceBase_ctor", 0x00029E70, 0x120, "c3dj_objs12_120"),
                                 ("Midi_Device::ctor", 0x00009BE0, 0x4060, "c3dj_objs12_4060"), ("VoiceBase_ctor", 0x00029020, 0x1B0, "c3dj_objs12_1b0")):
    HOOKS[_name] = dict(_SND_DLL, addr=_addr, abi="thiscall", ret="pointer", args=["pointer"], fixture=_fx,
                        state=[(f"$o{i}", 0, _size) for i in range(12)], vectors=[(f"$o{i}",) for i in range(12)])

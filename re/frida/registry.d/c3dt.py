# C3 batch c3dt (round 28): reachable sound.dll C2 rows left by earlier batches. Module-aware hooks; addr is the RVA,
# VA = 0x10000000 + RVA. Reimplementations in shim/src/re/c3dt.cpp, fixtures in re/frida/js/fixtures.d/c3dt.js, Purposes and
# per-jcc branch tables in log/c3/c3dt_purpose.md, notes in log/c3/c3dt_notes.txt.
#
# Stand-ins (one call per key, inside the fixture): the heap stand-in over sound.dll's operator delete 0x4249a (frees only
# recorded), the import stand-in over sound.dll's IAT slots (`$implog` is a state region of every key that stands in for imports)
# and a thread-filtered recording stub on an incremental-link thunk (`$log`, a state region).
#
# HOOKED CALLEES (the verifier re-runs these keys with the hooks off): see log/c3/c3dt_notes.txt section C.

_SND = dict(module="sound.dll")
_IMPLOG = ("$implog", 0, 0x400)
_LOG = ("$log", 0, 0x200)
_FREELOG = ("$freelog", 0, 0x404)
_G49A8 = ("$g49a8", 0, 4)

HOOKS = {}

_HANDLES12 = 12
# ---------------------------------------------------------------------------------------------------------------------
# A. wave-in capture device
# ---------------------------------------------------------------------------------------------------------------------
HOOKS["WaveInDevice_start"] = dict(_SND, addr=0x00038B20, abi="thiscall", ret="uint32", args=["pointer"], fixture="c3dt_wistart",
                                   state=[(f"$w{i}", 0, 0x300) for i in range(12)] + [_IMPLOG],
                                   vectors=[(f"$w{i}",) for i in range(12)])
HOOKS["WaveInDevice_reset"] = dict(_SND, addr=0x00038F10, abi="thiscall", ret="uint32", args=["pointer"], fixture="c3dt_wireset",
                                   state=[(f"$w{i}", 0, 0x300) for i in range(12)] + [_IMPLOG],
                                   vectors=[(f"$w{i}",) for i in range(12)])
_WI_ROWS = 21
HOOKS["WaveInDevice_prepareHeaders"] = dict(_SND, addr=0x00038B40, abi="thiscall", ret="uint32", args=["pointer"],
                                            fixture="c3dt_wiprep", state=[(f"$w{i}", 0, 0x300) for i in range(_WI_ROWS)] + [_IMPLOG],
                                            vectors=[(f"$w{i}",) for i in range(_WI_ROWS)])
HOOKS["WaveInDevice_addBuffer"] = dict(_SND, addr=0x00038DA0, abi="thiscall", ret="uint32", args=["pointer"],
                                       fixture="c3dt_wiadd", state=[(f"$w{i}", 0, 0x300) for i in range(_WI_ROWS)] + [_IMPLOG],
                                       vectors=[(f"$w{i}",) for i in range(_WI_ROWS)])
# requeue rows: (sub-object present, header index; 0 = NULL header)
_RQ = [(0, 1), (1, 1), (0, 0), (1, 0), (0, 2), (1, 3), (1, 2), (0, 4), (1, 5), (0, 6), (1, 7), (1, 1)]
HOOKS["WaveInDevice_requeueBuffer"] = dict(
    _SND, addr=0x000393D0, abi="thiscall", ret="uint32", args=["pointer", "pointer"], fixture="c3dt_wirequeue",
    state=[(f"$w{i}", 0, 0x300) for i in range(12)] + [(f"$h{i}", 0, 0x40) for i, r in enumerate(_RQ) if r[1]] + [_LOG, _IMPLOG],
    vectors=[(f"$w{i}", f"$h{i}" if r[1] else 0) for i, r in enumerate(_RQ)])
HOOKS["WaveInDevice_open"] = dict(_SND, addr=0x00039050, abi="thiscall", ret="uint32", args=["pointer"], fixture="c3dt_wiopen",
                                  state=[(f"$w{i}", 0, 0x300) for i in range(14)] + [_IMPLOG],
                                  vectors=[(f"$w{i}",) for i in range(14)])

# ---------------------------------------------------------------------------------------------------------------------
# B. mmio handle close and forwarders
# ---------------------------------------------------------------------------------------------------------------------
HOOKS["closeMmioHandle"] = dict(_SND, addr=0x00008770, abi="thiscall", ret="uint32", args=["pointer"], fixture="c3dt_closemmio",
                                state=[(f"$m{i}", 0, 0x60) for i in range(12)] + [("$cnt", 0, 4), _IMPLOG],
                                vectors=[(f"$m{i}",) for i in range(12)])
for _k in ("null", "h0", "h1", "h2"):
    HOOKS[f"Wave_Device::closeStream_{_k}"] = dict(
        _SND, addr=0x0000B170, abi="thiscall", ret="uint32", args=[], fixture=f"c3dt_closestream_{_k}",
        state=[("$cnt", 0, 4), ("$gdev", 0, 4), _G49A8, _IMPLOG] + ([] if _k == "null" else [("$dev", 0, 0x60)]),
        vectors=[()])
_ARGS12 = [0, 1, 0x1234, 0xFFFFFFFF, 0x80000000, 0x5555, 2, 0x100, 0xDEADBEEF, 0x41, 7, 0x7FFFFFFF]
for _k in ("null", "dev"):
    HOOKS[f"openWaveStreamFile_{_k}"] = dict(
        _SND, addr=0x0000B110, abi="stdcall", ret="uint32", args=["uint32"], fixture=f"c3dt_openstream_{_k}",
        state=[("$gdev", 0, 4), _G49A8, _LOG] + ([] if _k == "null" else [("$dev", 0, 0x60)]),
        vectors=[(a,) for a in _ARGS12])
HOOKS["Seq_closeFile"] = dict(_SND, addr=0x000204B0, abi="thiscall", ret="uint32", args=["pointer"], fixture="c3dt_seqclose",
                              state=[(f"$s{i}", 0, 0x100) for i in range(12)] + [_LOG],
                              vectors=[(f"$s{i}",) for i in range(12)])

# ---------------------------------------------------------------------------------------------------------------------
# C. deleting-destructor wrappers and the sound window object
# ---------------------------------------------------------------------------------------------------------------------
_FLAGS12 = [1, 0, 3, 2, 0x101, 0x100, 0xFFFFFFFF, 0xFFFFFFFE, 5, 4, 0x80000001, 0x7F]
_DEL = [
    ("Sound::scalarDeletingDtor", 0x00003FD0, "sound"), ("Wave_Device::scalarDeletingDtor", 0x0000B2B0, "wavedev"),
    ("CtrlObject_scalarDtorA", 0x0001C530, "ctrla"), ("Sound5b6a8_scalarDtor", 0x0001F6C0, "s5b6a8"),
    ("Sound5c450_scalarDtor", 0x00030150, "s5c450"), ("StreamReader_scalarDtor", 0x00031270, "streamrd"),
    ("WaveInDevice_deleteDtor", 0x000386E0, "wavein"), ("MultiSound_deleteDtor", 0x00039760, "multi"),
]
for _name, _addr, _fx in _DEL:
    HOOKS[_name] = dict(_SND, addr=_addr, abi="thiscall", ret="uint32", args=["pointer", "uint32"], fixture=f"c3dt_del_{_fx}",
                        state=[(f"$o{i}", 0, 0x40) for i in range(12)] + [_LOG, _FREELOG],
                        vectors=[(f"$o{i}", _FLAGS12[i]) for i in range(12)] + [(f"$o{i}", _FLAGS12[(i + 5) % 12]) for i in range(12)])
HOOKS["SoundWindowObj_dtor"] = dict(_SND, addr=0x000286A0, abi="thiscall", ret="uint32", args=["pointer"], fixture="c3dt_swodtor",
                                    state=[(f"$o{i}", 0, 0x20) for i in range(12)] + [_IMPLOG],
                                    vectors=[(f"$o{i}",) for i in range(12)])
HOOKS["destroySoundWindowObj"] = dict(_SND, addr=0x00028630, abi="thiscall", ret="uint32", args=["pointer", "uint32"],
                                      fixture="c3dt_swodestroy", state=[(f"$o{i}", 0, 0x20) for i in range(12)] + [_IMPLOG, _FREELOG],
                                      vectors=[(f"$o{i}", _FLAGS12[i]) for i in range(12)] + [(f"$o{i}", _FLAGS12[(i + 5) % 12]) for i in range(12)])

# ---------------------------------------------------------------------------------------------------------------------
# D. forwarders that test a module global (the global is swapped for the key, the first thread is frozen, the timer thread is quiet)
# ---------------------------------------------------------------------------------------------------------------------
_A12 = [0, 1, 0x1234, 0xFFFFFFFF, 0x80000000, 0x5555, 2, 0x100, 0xDEADBEEF, 0x41, 7, 0x7FFFFFFF]
_GL = [("$gdev", 0, 4), _G49A8]


def _devst(k):
    return [] if k == "null" else [("$dev", 0, 0x200)]


for _k in ("null", "dev"):
    HOOKS[f"Wave_Device::caps_{_k}"] = dict(_SND, addr=0x0000E230, abi="default", ret="uint32", args=[], fixture=f"c3dt_caps_{_k}",
                                            state=_GL + [_LOG] + _devst(_k), vectors=[()])
    HOOKS[f"Wave_Device::deviceName_{_k}"] = dict(
        _SND, addr=0x0000E260, abi="stdcall", ret="uint32", args=["uint32", "pointer", "uint32"], fixture=f"c3dt_name_{_k}",
        state=_GL + [_LOG] + _devst(_k) + [(f"$b{i}", 0, 0x20) for i in range(12)],
        vectors=[(_A12[i], f"$b{i}", _A12[(i * 5 + 3) % 12]) for i in range(12)])
    HOOKS[f"Wave_Device::setCooperativeLevel_{_k}"] = dict(
        _SND, addr=0x0000B140, abi="stdcall", ret="uint32", args=["uint32"], fixture=f"c3dt_coop_{_k}",
        state=_GL + [_LOG] + _devst(_k), vectors=[(a,) for a in _A12])
    HOOKS[f"Wave_In_Device::op1_{_k}"] = dict(
        _SND, addr=0x0000ED70, abi="stdcall", ret="uint32", args=["uint32"], fixture=f"c3dt_op1_{_k}",
        state=_GL + [_LOG] + _devst(_k), vectors=[(a,) for a in _A12])
    HOOKS[f"Wave_In_Device::op2_{_k}"] = dict(_SND, addr=0x0000EDA0, abi="default", ret="uint32", args=[], fixture=f"c3dt_op2_{_k}",
                                              state=_GL + [_LOG] + _devst(_k), vectors=[()])
    HOOKS[f"WaveInStream_start_{_k}"] = dict(
        _SND, addr=0x00031450, abi="thiscall", ret="uint32", args=["pointer", "uint32", "uint32", "uint32"], fixture=f"c3dt_wstream_{_k}",
        state=_GL + [_LOG] + _devst(_k) + [(f"$t{i}", 0, 0x200) for i in range(12)],
        vectors=[(f"$t{i}", _A12[i], _A12[(i + 3) % 12], _A12[(i + 7) % 12]) for i in range(12)])
for _k in ("null", "clear", "set", "setall"):
    HOOKS[f"serviceWaveDevice_{_k}"] = dict(_SND, addr=0x0000E310, abi="stdcall", ret="uint32", args=["uint32"], fixture=f"c3dt_svc_{_k}",
                                            state=_GL + [_LOG] + ([] if _k == "null" else [("$dev", 0, 0x200)]),
                                            vectors=[(a,) for a in _A12])
HOOKS["DirectSoundDevice::setCooperativeLevel"] = dict(
    _SND, addr=0x000120A0, abi="thiscall", ret="uint32", args=["pointer", "uint32"], fixture="c3dt_dscoop",
    state=[(f"$d{i}", 0, 0x200) for i in range(12)] + [_LOG, ("$gcoop", 0, 4), _G49A8],
    vectors=[(f"$d{i}", _A12[(i * 7 + 1) % 12]) for i in range(12)])
for _k in ("null", "noh", "ok", "hdr", "hdrbad", "err"):
    HOOKS[f"Wave_In_Device::stop_{_k}"] = dict(
        _SND, addr=0x0000EAA0, abi="default", ret="uint32", args=[], fixture=f"c3dt_stop_{_k}",
        state=_GL + [_IMPLOG] + ([] if _k == "null" else [("$w", 0, 0x300)]), vectors=[()])

# ---------------------------------------------------------------------------------------------------------------------
# E. small channel / sequencer / list bodies
# ---------------------------------------------------------------------------------------------------------------------
HOOKS["Voice_stopAndFlag"] = dict(_SND, addr=0x00034DB0, abi="thiscall", ret="uint32", args=["pointer", "uint32"], fixture="c3dt_vstop",
                                  state=[(f"$v{i}", 0, 0x200) for i in range(12)] + [_LOG],
                                  vectors=[(f"$v{i}", _A12[(i * 5 + 2) % 12]) for i in range(12)])
HOOKS["SoundGroup_rewind"] = dict(_SND, addr=0x00039C10, abi="thiscall", ret="uint32", args=["pointer"], fixture="c3dt_grewind",
                                  state=[(f"$g{i}", 0, 0x200) for i in range(12)] + [_LOG], vectors=[(f"$g{i}",) for i in range(12)])
HOOKS["Channel_serviceBuffer"] = dict(
    _SND, addr=0x00035F80, abi="thiscall", ret="uint32", args=["pointer"], fixture="c3dt_chansvc",
    state=[(f"$c{i}", 0, 0x200) for i in range(12)] + [(f"$u{i}", 0, 0x20) for i in (3, 4, 5, 6, 7, 8, 9, 10)] + [_LOG, _G49A8],
    vectors=[(f"$c{i}",) for i in range(12)])
for _k, _fx in (("eng", "c3dt_enq_eng"), ("null", "c3dt_enq_null")):
    HOOKS[f"MultiSound_enqueueCmd3_{_k}"] = dict(
        _SND, addr=0x00039BD0, abi="thiscall", ret="uint32", args=["pointer"], fixture=_fx,
        state=_GL + [_LOG] + [(f"$m{i}", 0, 0x200) for i in range(12)] + ([("$eng", 0, 0x200)] if _k == "eng" else []),
        vectors=[(f"$m{i}",) for i in range(12)])
HOOKS["MultiSound_release"] = dict(_SND, addr=0x00039C40, abi="thiscall", ret="uint32", args=["pointer"], fixture="c3dt_msrel",
                                   state=_GL + [_LOG, ("$eng", 0, 0x200)] + [(f"$m{i}", 0, 0x200) for i in range(12)],
                                   vectors=[(f"$m{i}",) for i in range(12)])
HOOKS["ListValues_callSlot18"] = dict(_SND, addr=0x0001BC00, abi="thiscall", ret="uint32", args=["pointer"], fixture="c3dt_listcall",
                                      state=[(f"$l{i}", 0, 0x200) for i in range(12)] + [_LOG], vectors=[(f"$l{i}",) for i in range(12)])
HOOKS["Seq_setPlayOrderMode"] = dict(_SND, addr=0x00022750, abi="thiscall", ret="uint32", args=["pointer", "uint32"], fixture="c3dt_seqorder",
                                     state=[(f"$q{i}", 0, 0x240) for i in range(12)] + [_LOG],
                                     vectors=[(f"$q{i}", m) for i, m in enumerate([0, 1, 2, 3, 0xFFFFFFFF, 0x80000000, 2, 1, 0, 5, 0x100, 2])])

# ---------------------------------------------------------------------------------------------------------------------
# F. parser forwarders and the MIDI output switch
# ---------------------------------------------------------------------------------------------------------------------
HOOKS["Sound5c450::openMmio"] = dict(_SND, addr=0x00030710, abi="thiscall", ret="uint32", args=["pointer", "uint32"], fixture="c3dt_openmmio",
                                     state=[(f"$o{i}", 0, 0x200) for i in range(12)] + [_LOG],
                                     vectors=[(f"$o{i}", _A12[(i * 5 + 1) % 12]) for i in range(12)])
HOOKS["Channel_parseWaveHeader"] = dict(_SND, addr=0x00033990, abi="thiscall", ret="uint32", args=["pointer"], fixture="c3dt_parsehdr",
                                        state=[(f"$p{i}", 0, 0x200) for i in range(12)] + [_LOG], vectors=[(f"$p{i}",) for i in range(12)])
HOOKS["Channel_parseWaveHeader_adjPlus70"] = dict(_SND, addr=0x00039980, abi="thiscall", ret="uint32", args=["pointer"], fixture="c3dt_parseadj",
                                                  state=[(f"$p{i}", 0, 0x200) for i in range(12)] + [_LOG], vectors=[(f"$p{i}",) for i in range(12)])
HOOKS["serviceDecodeRing"] = dict(_SND, addr=0x00034FB0, abi="thiscall", ret="uint32", args=["pointer"], fixture="c3dt_decring",
                                  state=[(f"$r{i}", 0, 0x200) for i in range(12)] + [_LOG], vectors=[(f"$r{i}",) for i in range(12)])
HOOKS["Midi_Device::enableOutput"] = dict(_SND, addr=0x0000A1B0, abi="thiscall", ret="uint32", args=["pointer"], fixture="c3dt_midien",
                                          state=[(f"$d{i}", 0, 0x200) for i in range(12)] + [_LOG, ("$gmidi", 0, 4), _G49A8],
                                          vectors=[(f"$d{i}",) for i in range(12)])

# ---------------------------------------------------------------------------------------------------------------------
# G. the idle-sound reaper and the CRT lock wrappers
# ---------------------------------------------------------------------------------------------------------------------
for _i in range(14):
    HOOKS[f"Sound_reapIdle_s{_i}"] = dict(_SND, addr=0x000204E0, abi="default", ret="uint32", args=[], fixture=f"c3dt_reap_{_i}",
                                          state=[("$s", 0, 0x1000), ("$glist", 0, 0x10), _G49A8, _LOG, _FREELOG], vectors=[()])
for _name, _addr, _fx in (("initCriticalSection", 0x00041E43, "init"), ("deleteCriticalSection", 0x00041E4E, "delete"),
                          ("enterCriticalSection", 0x00041E59, "enter"), ("leaveCriticalSection", 0x00041E64, "leave")):
    HOOKS[_name] = dict(_SND, addr=_addr, abi="default", ret="uint32", args=["uint32"], fixture=f"c3dt_cs_{_fx}", state=[_IMPLOG],
                        vectors=[(a,) for a in _A12])
HOOKS["_unlock"] = dict(_SND, addr=0x00047771, abi="default", ret="uint32", args=["uint32"], fixture="c3dt_unlock", state=[_IMPLOG],
                        vectors=[(n,) for n in (0, 1, 2, 3, 5, 9, 10, 12, 13, 17, 18, 20)])
for _name, _addr in (("heapAllocUnlockFunclet", 0x00042B72), ("heapAllocUnlockFunclet2", 0x00042BD1), ("freeUnlockFunclet", 0x00042C71),
                     ("freeUnlockFunclet2", 0x00042CC9), ("reallocUnlockFunclet", 0x000453DD), ("msizeUnlockFunclet", 0x000455EB),
                     ("msizeUnlockFunclet2", 0x00045666), ("callocUnlockFunclet", 0x0004593A), ("callocUnlockFunclet2", 0x000459C3)):
    HOOKS[_name] = dict(_SND, addr=_addr, abi="default", ret="uint32", args=[], fixture="c3dt_unlock", state=[_IMPLOG], vectors=[()])

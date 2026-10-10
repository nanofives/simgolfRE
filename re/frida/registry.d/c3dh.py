# C3 batch c3dh (round 25): sound.dll rows earlier batches dropped for imports, the CRT heap or live state
# (log/c3/c3dh_candidates.tsv). Module-aware hooks; addr is the RVA, VA = 0x10000000 + RVA. Reimplementations in
# shim/src/re/c3dh.cpp, fixtures in re/frida/js/fixtures.d/c3dh.js, Purposes and per-jcc branch tables in
# log/c3/c3dh_purpose.md, notes in log/c3/c3dh_notes.txt.
#
# Stand-ins (one call per key, inside the fixture): the heap stand-in over sound.dll's operator new 0x424d7 / operator
# delete 0x4249a (arena cursor in the state, frees only recorded) and the import stand-in over sound.dll's IAT slots (the
# call log `$implog` is a state region of every key that stands in for imports).
#
# HOOKED CALLEES (the verifier re-runs these keys with the hooks off):
#   inside this batch: SoundTimer::registerClient -> SoundTimer::linkClient (sound.dll:9040)

_SND_DLL = dict(module="sound.dll")
_LOG = ("$implog", 0, 0x400)
_ARENA = ("$arena", 0, 0x110)
_FREELOG = ("$freelog", 0, 0x404)

HOOKS = {}

# ---------------------------------------------------------------------------------------------------------------------
# A. ring queues
# ---------------------------------------------------------------------------------------------------------------------
_ARGS4 = [(0, 0, 0, 0), (1, 2, 3, 4), (0xFFFFFFFF, 0x80000000, 0x7FFFFFFF, 0x12345678)]
HOOKS.update({
    "CommandQueue::push": dict(_SND_DLL, addr=0x0000A240, abi="thiscall", ret="uint32",
                               args=["pointer", "uint32", "uint32", "uint32", "uint32"], fixture="c3dh_cmdq",
                               state=[(f"$q{i}", 0, 0x4060) for i in range(9)] + [_ARENA],
                               vectors=[(f"$q{i}",) + a for i in range(9) for a in _ARGS4]),
    "WaveCmdQueue::pushNode": dict(_SND_DLL, addr=0x0000C2F0, abi="thiscall", ret="uint32",
                                   args=["pointer", "uint32", "uint32"], fixture="c3dh_wcq",
                                   state=[(f"$q{i}", 0, 0x41F0) for i in range(9)] + [_ARENA],
                                   vectors=[(f"$q{i}", a, b) for i in range(9)
                                            for a, b in ((0, 0), (1, 2), (0xFFFFFFFF, 0x12345678))]),
})

# ---------------------------------------------------------------------------------------------------------------------
# A. node lists
# ---------------------------------------------------------------------------------------------------------------------
_CL = [0, 1, 0x1234, 0xFFFFFFFF]
_LSTATE8 = [(f"$l{i}", 0, 0x100) for i in range(8)] + [_ARENA]
_LSTATE10 = [(f"$l{i}", 0, 0x100) for i in range(10)] + [_ARENA]
HOOKS.update({
    # leaf-free (operator new is a callee): rows 0..7 of c3dh_link, four clients each
    "SoundTimer::linkClient": dict(_SND_DLL, addr=0x00009040, abi="thiscall", ret="uint32", args=["pointer", "uint32"],
                                   fixture="c3dh_link", state=_LSTATE8,
                                   vectors=[(f"$l{i}", c) for i in range(8) for c in _CL]),
    "SoundTimer::linkClient_oom": dict(_SND_DLL, addr=0x00009040, abi="thiscall", ret="uint32",
                                       args=["pointer", "uint32"], fixture="c3dh_link_oom", state=_LSTATE8,
                                       vectors=[(f"$l{i}", c) for i in (0, 6) for c in _CL]),
    "NodeList_insert": dict(_SND_DLL, addr=0x00024E60, abi="thiscall", ret="uint32", args=["pointer", "uint32"],
                            fixture="c3dh_nodeins", state=_LSTATE10,
                            vectors=[(f"$l{i}", c) for i in range(10) for c in (0, 0x1234, 0xFFFFFFFF)]),
})

HOOKS["NodeList_insert_oom"] = dict(_SND_DLL, addr=0x00024E60, abi="thiscall", ret="uint32", args=["pointer", "uint32"],
                                    fixture="c3dh_nodeins_oom", state=_LSTATE10,
                                    vectors=[(f"$l{i}", c) for i in (0, 1) for c in (0, 0x1234, 0xFFFFFFFF)])

_TSTATE = [(f"$t{i}", 0, 0x100) for i in range(8)] + [(f"$cl{i}", 0, 0x20) for i in range(4)]
HOOKS.update({
    "SoundTimer::registerClient": dict(_SND_DLL, addr=0x000088E0, abi="thiscall", ret="uint32", args=["pointer", "uint32"],
                                       fixture="c3dh_reg_a", state=_TSTATE + [_ARENA, _LOG],
                                       vectors=[(f"$t{i}", c) for i in range(8) for c in (0, 0x1234, 0xFFFFFFFF)]),
    "SoundTimer::registerClient_z": dict(_SND_DLL, addr=0x000088E0, abi="thiscall", ret="uint32",
                                         args=["pointer", "uint32"], fixture="c3dh_reg_z", state=_TSTATE + [_ARENA, _LOG],
                                         vectors=[(f"$t{i}", c) for i in range(8) for c in (0, 0x1234)]),
    # (timer, client): every list shape against clients present, absent and duplicated
    "SoundTimer::unregisterClient": dict(_SND_DLL, addr=0x00008920, abi="thiscall", ret="uint32",
                                         args=["pointer", "pointer"], fixture="c3dh_unreg",
                                         state=_TSTATE + [_FREELOG, _LOG],
                                         vectors=[("$t0", "$cl0"), ("$t1", "$cl0"), ("$t1", "$cl1"), ("$t2", "$cl0"),
                                                  ("$t3", "$cl1"), ("$t4", "$cl1"), ("$t5", "$cl2"), ("$t6", "$cl1"),
                                                  ("$t7", "$cl0"), ("$t4", "$cl0"), ("$t4", "$cl2"), ("$t0", "$cl3")]),
})

# ---------------------------------------------------------------------------------------------------------------------
# B. imports, COM-shaped interfaces, timers
# ---------------------------------------------------------------------------------------------------------------------
# SoundWindowObj_unsubclass: SetWindowLongA is stood in; twelve objects (flag byte +4, window +8, saved procedure +0xc)
HOOKS["SoundWindowObj_unsubclass"] = dict(_SND_DLL, addr=0x000287C0, abi="thiscall", ret="uint32", args=["pointer"],
                                          fixture="c3dh_winobj", state=[(f"$w{i}", 0, 0x20) for i in range(12)] + [_LOG],
                                          vectors=[(f"$w{i}",) for i in range(12)])

# postSoundThreadMessage: the PostThreadMessageA schedule is the key (ok / one retry / three retries)
_PA = [(0x400, 0x1234, 0x5678), (0xFFFFFFFF, 0, 0x80000000)]
for _k, _fx in (("ok", "c3dh_postthr_ok"), ("r1", "c3dh_postthr_r1"), ("r3", "c3dh_postthr_r3")):
    HOOKS[f"postSoundThreadMessage_{_k}"] = dict(
        _SND_DLL, addr=0x0002AA90, abi="thiscall", ret="uint32", args=["pointer", "uint32", "uint32", "uint32"],
        fixture=_fx, state=[(f"$p{i}", 0, 0x40) for i in range(6)] + [_LOG],
        vectors=[(f"$p{i}",) + a for i in range(6) for a in _PA])

# DsBuffer family (COM-shaped interface at +0x60)
_DS_RESTART_COM = [0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1]
_DS_PLAY_COM = [0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1]
_DS_TIMED_COM = [0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 1, 1, 0]


def _dsstate(com):
    return ([(f"$d{i}", 0, 0x100) for i in range(len(com))] +
            [(f"$c{i}", 0, 0x200) for i, c in enumerate(com) if c])


HOOKS.update({
    "DsBuffer::restartPlayback": dict(_SND_DLL, addr=0x0000F020, abi="thiscall", ret="uint32", args=["pointer"],
                                      fixture="c3dh_ds_restart", state=_dsstate(_DS_RESTART_COM) + [_LOG],
                                      vectors=[(f"$d{i}",) for i in range(12)]),
    "DsBuffer::play": dict(_SND_DLL, addr=0x00010DB0, abi="thiscall", ret="uint32", args=["pointer", "uint32"],
                           fixture="c3dh_ds_play", state=_dsstate(_DS_PLAY_COM) + [_LOG],
                           vectors=[(f"$d{i}", f) for i in range(12) for f in (0, 1, 0x80000000)]),
    "DsBuffer::getTimedPosition": dict(_SND_DLL, addr=0x00010BE0, abi="thiscall", ret="uint32",
                                       args=["pointer", "pointer", "pointer", "uint32"], fixture="c3dh_ds_timed",
                                       state=_dsstate(_DS_TIMED_COM) + [("$outs", 0, 0x20), _LOG],
                                       vectors=[(f"$d{i}", "$o1", "$o2", a) for i in range(14)
                                                for a in (0, 1000, 0xFFFFFFFF)]),
})

_LST = [None, 0, 0x8878000A, 0x80004001, 0x8007000E, 0x80070057, 0x88780064, 0x88780078, 0x887800AA, 0x887800AB,
        0x12345678, 0xFFFFFFFF]
HOOKS["Ds3DListener::setPosition"] = dict(
    _SND_DLL, addr=0x000135A0, abi="thiscall", ret="uint32", args=["pointer", "uint32", "uint32", "uint32"],
    fixture="c3dh_listener",
    state=[(f"$l{i}", 0, 0x200) for i in range(12)] + [(f"$c{i}", 0, 0x200) for i in range(1, 12)],
    vectors=[(f"$l{i}",) + a for i in range(12) for a in ((0x3F800000, 0x40000000, 0x40400000), (0, 0xFFFFFFFF, 0x12345678))])

# Midi_Device::disableOutput: one key per value of the module byte 0x100b49bc
for _b in ("ff", "02", "00", "04"):
    HOOKS[f"Midi_Device::disableOutput_{_b}"] = dict(
        _SND_DLL, addr=0x0000A200, abi="thiscall", ret="uint32", args=["pointer"], fixture=f"c3dh_midioff_{_b}",
        state=[(f"$m{i}", 0, 0x80) for i in range(12)] + [("$g", 0, 1)],
        vectors=[(f"$m{i}",) for i in range(12)])

# Sound timer
_TSTART = [(f"$t{i}", 0, 0x100) for i in range(12)] + [("$g49a8", 0, 4), _LOG]
for _k in ("ok", "bp61", "se61"):
    HOOKS[f"SoundTimer::start_{_k}"] = dict(_SND_DLL, addr=0x00008CE0, abi="thiscall", ret="uint32", args=["pointer"],
                                            fixture=f"c3dh_tstart_{_k}", state=_TSTART,
                                            vectors=[(f"$t{i}",) for i in range(12)])
HOOKS["SoundTimer::killEvent"] = dict(_SND_DLL, addr=0x00008D70, abi="thiscall", ret="uint32", args=["pointer"],
                                      fixture="c3dh_tkill", state=_TSTART, vectors=[(f"$t{i}",) for i in range(12)])

_TCL = [(f"$k{i}", 0, 0x80) for i in range(6)] + [("$timer", 0, 0x100), _ARENA, ("$g49a8", 0, 4), _LOG]
for _k in ("ok", "busy", "null", "bp61", "se61"):
    HOOKS[f"registerTimerClient_{_k}"] = dict(_SND_DLL, addr=0x0000A170, abi="thiscall", ret="uint32", args=["pointer"],
                                              fixture=f"c3dh_regtimer_{_k}", state=_TCL,
                                              vectors=[(f"$k{i}",) for i in range(6)])
_OA = [(0, 2), (0, 3), (0, 0), (0xFFFFFFFF, 1), (5, 0xFE), (6, 0xFFFFFFFF)]
for _k in ("ok", "fail", "null"):
    HOOKS[f"Midi_Device::openOutput_{_k}"] = dict(_SND_DLL, addr=0x00009DA0, abi="thiscall", ret="uint32",
                                                  args=["pointer", "uint32", "uint32"], fixture=f"c3dh_midiopen_{_k}",
                                                  state=[(f"$k{i}", 0, 0x80) for i in range(6)] +
                                                        [("$timer", 0, 0x100), _ARENA, _LOG],
                                                  vectors=[(f"$k{i}",) + _OA[i] for i in range(6)])

# ---------------------------------------------------------------------------------------------------------------------
# C. mmio handles, wave-in devices, MIDI files, handle lists. Every key that touches the module counter of open mmio handles
# 0x100b4a08 presets it to 100 with the game's first thread frozen ($gcnt, restored at the flush) and includes it in the state.
# ---------------------------------------------------------------------------------------------------------------------
_GCNT = ("$gcnt", 0, 4)
HOOKS["openMmioFile"] = dict(_SND_DLL, addr=0x000086F0, abi="thiscall", ret="uint32", args=["pointer", "pointer"],
                             fixture="c3dh_openmmio", state=[(f"$o{i}", 0, 0x40) for i in range(12)] + [_GCNT, _LOG],
                             vectors=[(f"$o{i}", f"$n{i}") for i in range(12)])
HOOKS["Wave_Device::openMemoryMmio"] = dict(
    _SND_DLL, addr=0x00006420, abi="thiscall", ret="uint32", args=["pointer", "uint32"], fixture="c3dh_openmem",
    state=[(f"$o{i}", 0, 0x100) for i in range(8)] + [_GCNT, _LOG],
    vectors=[(f"$o{i}", s) for i in range(8) for s in (0, 100, 0x666, 0xFFFFFFFF)])
for _k in ("n3", "n0", "nbig"):
    HOOKS[f"WaveInDevice_ctor_{_k}"] = dict(_SND_DLL, addr=0x00038650, abi="thiscall", ret="uint32", args=["pointer"],
                                            fixture=f"c3dh_wavein_ctor_{_k}",
                                            state=[(f"$o{i}", 0, 0x80) for i in range(12)] + [_LOG],
                                            vectors=[(f"$o{i}",) for i in range(12)])
HOOKS["stopSoundThreadFull"] = dict(_SND_DLL, addr=0x0002A8A0, abi="thiscall", ret="void", args=["pointer"],
                                    fixture="c3dh_stopthr", state=[(f"$s{i}", 0, 0x40) for i in range(12)] + [_LOG],
                                    vectors=[(f"$s{i}",) for i in range(12)])
HOOKS["WaveInDevice_close"] = dict(_SND_DLL, addr=0x00039140, abi="thiscall", ret="uint32", args=["pointer"],
                                   fixture="c3dh_waveinclose", state=[(f"$w{i}", 0, 0x300) for i in range(21)] + [_LOG],
                                   vectors=[(f"$w{i}",) for i in range(21)])
_AC = [(0, 1, 0x40), (0, 2, 0x40), (0, 3, 0x10), (0, 1, 0), (1, 2, 0x20), (2, 3, 0x30), (3, 1, 0x40), (3, 2, 0x40),
       (4, 1, 0x20), (4, 3, 0), (5, 1, 0x40), (6, 2, 0x40), (7, 1, 0x40)]
HOOKS["WaveInDevice_allocCaptureBuffers"] = dict(
    _SND_DLL, addr=0x00038890, abi="thiscall", ret="uint32", args=["pointer", "uint32", "uint32"], fixture="c3dh_alloccap",
    state=[(f"$a{i}", 0, 0x100) for i in range(8)] + [("$arena", 0, 0x810)],
    vectors=[(f"$a{r}", c, s) for r, c, s in _AC])
for _k, _p in (("null", None), ("p1", 1), ("p7", 7), ("p999", 999), ("p1000", 1000), ("p5000", 5000)):
    HOOKS[f"Midi_resetFileState_{_k}"] = dict(_SND_DLL, addr=0x0001D4A0, abi="thiscall", ret="void", args=["pointer"],
                                              fixture=f"c3dh_midireset_{_k}",
                                              state=[(f"$r{i}", 0, 0x1A0) for i in range(6)] + [_GCNT, _LOG],
                                              vectors=[(f"$r{i}",) for i in range(6)])

_NH = 13
_HL = [(f"$h{i}", 0, 0x400) for i in range(_NH)]
_HLX = _HL + [_GCNT, _LOG, _FREELOG]
HOOKS["Midi_Device::closeFile"] = dict(_SND_DLL, addr=0x0001E3D0, abi="thiscall", ret="uint32", args=["pointer"],
                                       fixture="c3dh_midiclose",
                                       state=[(f"$h{i}", 0, 0x600) for i in range(10)] + [_GCNT, _LOG, _FREELOG],
                                       vectors=[(f"$h{i}",) for i in range(10)])
HOOKS["closeMmioNodeList"] = dict(_SND_DLL, addr=0x00006330, abi="thiscall", ret="uint32", args=["pointer"],
                                  fixture="c3dh_hl_closenode", state=_HLX, vectors=[(f"$h{i}",) for i in range(_NH)])
HOOKS["freeMmioHandleList"] = dict(_SND_DLL, addr=0x00031490, abi="thiscall", ret="uint32", args=["pointer"],
                                   fixture="c3dh_hl_freehandle", state=_HLX, vectors=[(f"$h{i}",) for i in range(_NH)])
HOOKS["WaveStream::destroy"] = dict(_SND_DLL, addr=0x00006160, abi="thiscall", ret="void", args=["pointer"],
                                    fixture="c3dh_hl_wsdestroy", state=_HLX, vectors=[(f"$h{i}",) for i in range(_NH)])
HOOKS["WaveStream::scalarDeletingDtor"] = dict(
    _SND_DLL, addr=0x00006130, abi="thiscall", ret="uint32", args=["pointer", "uint32"], fixture="c3dh_hl_wsdel",
    state=_HLX, vectors=[(f"$h{i}", f) for i in range(_NH) for f in (0, 1, 0x101)])
for _k in ("empty", "live"):
    _st = _HLX + [("$glist", 0, 0x10)] + ([("$liveblk", 0, 0x200)] if _k == "live" else [])
    HOOKS[f"Sound_stop_{_k}"] = dict(_SND_DLL, addr=0x000303F0, abi="thiscall", ret="uint32", args=["pointer"],
                                     fixture=f"c3dh_hl_sound_{_k}", state=_st, vectors=[(f"$h{i}",) for i in range(_NH)])
    HOOKS[f"Sound_freeBufferList_{_k}"] = dict(_SND_DLL, addr=0x00030570, abi="thiscall", ret="uint32",
                                               args=["pointer"], fixture=f"c3dh_hl_sound_{_k}", state=_st,
                                               vectors=[(f"$h{i}",) for i in range(_NH)])

# ---------------------------------------------------------------------------------------------------------------------
# E. swapped module globals (frozen first thread, restored at the flush), codec allocators, CRT per-thread data
# ---------------------------------------------------------------------------------------------------------------------
# Sound_findByName: stdcall, one name; the live list head 0x100b4fe8 / cursor 0x100b4ff0 are pointed at a fixture list per key
for _k in ("l0", "l1", "l3", "lnull", "lhead"):
    HOOKS[f"Sound_findByName_{_k}"] = dict(
        _SND_DLL, addr=0x00021240, abi="stdcall", ret="uint8", args=["pointer"], fixture=f"c3dh_findname_{_k}",
        state=[("$gh", 0, 4), ("$gc", 0, 4), ("$blk", 0, 0x200)], vectors=[(f"$s{i}",) for i in range(10)])

# Midi_Device::flushMessages: g_midiDevice 0x100b4a1c pointed at a fixture device (handle 0x4d1d0001, or 0)
for _k in ("h", "h0"):
    HOOKS[f"Midi_Device::flushMessages_{_k}"] = dict(
        _SND_DLL, addr=0x0001EF10, abi="thiscall", ret="void", args=["pointer"], fixture=f"c3dh_flush_{_k}",
        state=[(f"$o{i}", 0, 0x480) for i in range(10)] + [_LOG], vectors=[(f"$o{i}",) for i in range(10)])

# Voice_requestCmd12: g_midiDevice pointed at a fixture CommandQueue (empty, one slot short of full, or NULL)
_LIM = [0, 1, 5, 0x10, 0xFF, 0x100, 0xFFFFFFFF, 0x80000000]
_RV = [(f"$v{i}", a, 0x1FF if k else 0) for i, lim in enumerate(_LIM)
       for k, a in enumerate((0, lim, (lim + 1) & 0xFFFFFFFF))]
for _k in ("q", "full", "null"):
    HOOKS[f"Voice_requestCmd12_{_k}"] = dict(
        _SND_DLL, addr=0x00023EA0, abi="thiscall", ret="uint32", args=["pointer", "uint32", "uint32"],
        fixture=f"c3dh_req12_{_k}", state=[(f"$v{i}", 0, 0x100) for i in range(8)] + [("$q", 0, 0x4060), _ARENA],
        vectors=_RV)

HOOKS["Seq_cmdHandler12"] = dict(
    _SND_DLL, addr=0x00023C20, abi="thiscall", ret="uint32", args=["pointer", "uint32", "uint32"], fixture="c3dh_seqcmd12",
    state=[(f"$q{i}", 0, 0x300) for i in range(6)],
    vectors=[(f"$q{i}", a, b) for i in range(6)
             for a, b in ((0, 0), (1, 1), (2, 0), (3, 0xFF), (0xFFFFFFFF, 1), (0, 0xFFFFFFFE))])

for _i in range(10):
    HOOKS[f"codecSetParams_v{_i}"] = dict(_SND_DLL, addr=0x0003AA50, abi="default", ret="uint32", args=[],
                                          fixture=f"c3dh_codecset_v{_i}", state=[("$g", 0, 12)], vectors=[()])

for _n, _a, _sz in (("A", 0x0003AA90, 0x2610), ("B", 0x0003AEA0, 0xC50)):
    for _k, _fx in (("", f"c3dh_allocctx_{_n.lower()}"), ("_5a", f"c3dh_allocctx_{_n.lower()}5a"),
                    ("_oom", f"c3dh_allocctx_{_n.lower()}oom")):
        HOOKS[f"codecAllocContext{_n}{_k}"] = dict(_SND_DLL, addr=_a, abi="default", ret="uint32", args=[], fixture=_fx,
                                                   state=[("$arena", 0, _sz), ("$gcodec", 0, 12)], vectors=[()])

_SF = [("$o0", 0, 0, 0), ("$o1", 1, "$f0", 7), ("$o2", 0x1234, 0, 0), ("$o3", 0xFFFFFFFF, "$f1", 0xFFFFFFFF),
       ("$o4", 0, "$f0", 0), ("$o5", 2, 0, 1), ("$o0", 3, "$f1", 2), ("$o1", 5, 0, 0), ("$o2", 6, "$f0", 0),
       ("$o3", 0, 0, 0), ("$o4", 9, "$f1", 9), ("$o5", 0x80000000, "$f0", 1)]
for _k in ("ok", "oom"):
    HOOKS[f"Channel_setFormat_{_k}"] = dict(
        _SND_DLL, addr=0x00030FB0, abi="thiscall", ret="uint32", args=["pointer", "uint32", "pointer", "uint32"],
        fixture=f"c3dh_setfmt_{_k}", state=[(f"$o{i}", 0, 0x100) for i in range(6)] + [("$arena", 0, 0xC50), ("$gcodec", 0, 12)],
        vectors=_SF)

for _k in ("a", "b", "new"):
    for _n, _addr in (("_errno", 0x0004588F), ("__doserrno", 0x00045898)):
        HOOKS[f"{_n}_{_k}"] = dict(_SND_DLL, addr=_addr, abi="default", ret="uint32", args=[], fixture=f"c3dh_errno_{_k}",
                                   state=[_LOG, ("$ptd", 0, 0x100)] + ([_ARENA] if _k == "new" else []), vectors=[()])

_SP = [0x00123450, 0x00123040, 0x00123457, 0x00100000, 0x00100100, 0x004FFFF0, 0x00500000, 0x80000000, 0x80000100,
       0x803FFFF0, 0x80400000, 0xFFFFFFFF, 0, 0x00123F00, 0x00130010, 0x7FFFFF00]
HOOKS["sbhRegionLocate"] = dict(
    _SND_DLL, addr=0x000470AC, abi="default", ret="uint32", args=["uint32", "pointer", "pointer"], fixture="c3dh_sbhloc",
    state=[("$outs", 0, 0x20), ("$head", 0, 0x18), ("$regs", 0, 0x80)], vectors=[(p, "$o1", "$o2") for p in _SP])

# ---------------------------------------------------------------------------------------------------------------------
# F. WaveInDevice_dtor (calls WaveInDevice_close 0x39140, hooked in this batch: callee-off run)
# ---------------------------------------------------------------------------------------------------------------------
HOOKS["WaveInDevice_dtor"] = dict(_SND_DLL, addr=0x00038710, abi="thiscall", ret="void", args=["pointer"],
                                  fixture="c3dh_wavedtor", state=[(f"$w{i}", 0, 0x300) for i in range(13)] + [_LOG, _FREELOG],
                                  vectors=[(f"$w{i}",) for i in range(13)])

# ---------------------------------------------------------------------------------------------------------------------
# G. heap-only functions (operator new 0x424d7 / operator delete 0x4249a stood in)
# ---------------------------------------------------------------------------------------------------------------------
HOOKS["Sound_setName"] = dict(
    _SND_DLL, addr=0x0001CED0, abi="thiscall", ret="void", args=["pointer", "pointer"], fixture="c3dh_setname",
    state=[(f"$o{i}", 0, 0x40) for i in range(12)] + [("$arena", 0, 0x410), _FREELOG],
    vectors=[(f"$o{i}", f"$n{i % 10}") for i in range(12)] + [(f"$o{i}", f"$n{(i * 3 + 1) % 10}") for i in range(12)] +
            [(f"$o{i}", 0) for i in (0, 1, 2, 5)])
HOOKS["allocVoiceSlot"] = dict(
    _SND_DLL, addr=0x0002D100, abi="thiscall", ret="uint32", args=["pointer", "pointer"], fixture="c3dh_allocslot",
    state=[(f"$b{i}", 0, 0x400) for i in range(12)] + [("$arena", 0, 0x140), _FREELOG],
    vectors=[(f"$t{i}", f"$d{i}") for i in range(12)])
HOOKS["buildVoiceRxName"] = dict(
    _SND_DLL, addr=0x0002ED50, abi="thiscall", ret="uint32", args=["pointer"], fixture="c3dh_buildrx",
    state=[(f"$r{i}", 0, 0x60) for i in range(10)] + [("$arena", 0, 0x50)], vectors=[(f"$r{i}",) for i in range(10)])

# ---------------------------------------------------------------------------------------------------------------------
# H. notification posts (PostMessageA stood in; the module object 0x100b5038 is only read)
# ---------------------------------------------------------------------------------------------------------------------
HOOKS["postNotification"] = dict(
    _SND_DLL, addr=0x0000DBE0, abi="thiscall", ret="uint32", args=["pointer", "pointer", "uint32"], fixture="c3dh_postnote",
    state=[(f"$d{i}", 0, 0x1C0) for i in range(4)] + [(f"$r{i}", 0, 0x1440) for i in range(3)] + [("$arena", 0, 0x110), _LOG],
    vectors=[(f"$d{i}", r, v) for i in range(4) for r, v in (("$r0", 0x1234), ("$r1", 0), ("$r2", 0xFFFFFFFF), (0, 5))])
HOOKS["postMidiNotification"] = dict(
    _SND_DLL, addr=0x00020B60, abi="thiscall", ret="uint32", args=["pointer"], fixture="c3dh_postmidi",
    state=[(f"$m{i}", 0, 0x220) for i in range(8)] + [_LOG], vectors=[(f"$m{i}",) for i in range(8)])
HOOKS["onWaveInCallbackMessage"] = dict(
    _SND_DLL, addr=0x00038FD0, abi="stdcall", ret="void", args=["uint32", "uint32", "uint32", "uint32", "uint32"],
    fixture="c3dh_wavecb", state=[_LOG],
    vectors=[(1, m, i, p, 9) for m in (0x3C0, 0x3BF, 0x3C1, 0x123C0, 0xFFFF03C0, 0, 0xFFFFFFFF, 0x13C0) for i, p in ((0x1234, 0x5678), (0, 0xFFFFFFFF))])

# ---------------------------------------------------------------------------------------------------------------------
# I. getModuleLinkerVersion (GetModuleHandleA returns a fake image) and crtHeapSelect (GetVersionExA / GetEnvironmentVariableA /
#    GetModuleFileNameA / GetModuleHandleA stood in; strstr 0x42690 and strchr 0x45680 are hooked by c3cm: callee-off run)
# ---------------------------------------------------------------------------------------------------------------------
for _k in ("6", "5", "badmz", "nolfanew", "7"):
    HOOKS[f"getModuleLinkerVersion_{_k}"] = dict(_SND_DLL, addr=0x00045B9B, abi="default", ret="void", args=["pointer"],
                                                 fixture=f"c3dh_linkerver_{_k}", state=[("$outs", 0, 0x10), _LOG],
                                                 vectors=[("$o0",), ("$o1",)])
for _k in ("nt5", "nt4", "w9x", "verfail", "glob1", "glob2", "glob3", "globbad", "globnocomma", "exe2", "exenomatch"):
    HOOKS[f"crtHeapSelect_{_k}"] = dict(_SND_DLL, addr=0x00045BC8, abi="default", ret="uint32", args=[],
                                        fixture=f"c3dh_heapsel_{_k}", state=[_LOG], vectors=[()])

# ---------------------------------------------------------------------------------------------------------------------
# J. SoundTimer::stopKeepLock (the live critical section is only entered/left through the stubs; 0x100b49a8 preset and compared)
# ---------------------------------------------------------------------------------------------------------------------
HOOKS["SoundTimer::stopKeepLock"] = dict(
    _SND_DLL, addr=0x00008BF0, abi="thiscall", ret="void", args=["pointer"], fixture="c3dh_stopkeep",
    state=[(f"$t{i}", 0, 0x100) for i in range(12)] + [(f"$cl{i}", 0, 0x20) for i in range(4)] + [("$g49a8", 0, 4), _LOG, _FREELOG],
    vectors=[(f"$t{i}",) for i in range(12)])

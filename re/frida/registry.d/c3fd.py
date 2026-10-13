# C3 batch c3fd (round 33): sound.dll size-only / never-tried rows, the GetProcAddress exports and the MultiSound constructors.
# Module-aware hooks; addr is the RVA, VA = 0x10000000 + RVA. Reimplementations in shim/src/re/c3fd.cpp, fixtures in
# re/frida/js/fixtures.d/c3fd.js, Purposes and per-jcc branch tables in log/c3/c3fd_purpose.md, notes in log/c3/c3fd_notes.txt.
# The structures below mirror the row tables of the fixture file (same names, same order).

_SND = dict(module="sound.dll")
_LOG = ("$log", 0, 0x820)
_IMPLOG = ("$implog", 0, 0x400)
_FREELOG = ("$freelog", 0, 0x404)
_G49A8 = ("$g49a8", 0, 4)


def _arena(size):
    return ("$arena", 0, 0x10 + size)


HOOKS = {}

# ---- A. GetProcAddress exports ---------------------------------------------------------------------------------------
for _name, _addr, _tag in (("Dll_Midi_Device::delete_device", 0x00009BB0, "midi"),
                           ("Dll_Wave_Device::delete_device", 0x0000B1A0, "wave"),
                           ("Dll_Wave_In_Device::delete_device", 0x0000EA00, "wavein")):
    for _v in range(10):
        HOOKS[f"{_name}_v{_v}"] = dict(_SND, addr=_addr, abi="default", ret="uint32", args=[],
                                       fixture=f"c3fd_del_{_tag}_v{_v}",
                                       state=[("$g", 0, 4), ("$o", 0, 0x200), _LOG, _G49A8], vectors=[()])

_FLAGS = [0, 1, 2, 3, 0x80000000, 0xFFFFFFFF, 0x10, 0x11, 0xFFFFFFFE, 0x5555, 0x5554, 7]
for _k, _fx in (("", "c3fd_itimer"), ("_alt", "c3fd_itimer_alt")):
    HOOKS[f"init_sound_timer{_k}"] = dict(_SND, addr=0x000092E0, abi="default", ret="uint32", args=["uint32", "uint32"],
                                          fixture=_fx,
                                          state=[("$gfl", 0, 4), ("$gtm", 0, 4), ("$alt", 0, 0x40), _arena(0x200), _FREELOG,
                                                 _LOG, _G49A8],
                                          vectors=[(_FLAGS[(i + 5) % 12], f) for i, f in enumerate(_FLAGS)])

for _name, _addr, _tag, _size in (("Dll_Wave_Device::create_device", 0x0000B050, "wcreate", 0x41E0),
                                  ("Dll_Wave_In_Device::create_device", 0x0000E910, "wicreate", 0x4C)):
    for _m in ("busy", "ok", "alt", "oom"):
        HOOKS[f"{_name}_{_m}"] = dict(_SND, addr=_addr, abi="default", ret="uint32", args=["pointer", "uint32"],
                                      fixture=f"c3fd_{_tag}_{_m}",
                                      state=[("$g", 0, 4), ("$dev", 0, 0x40), ("$alt", 0, 0x80), _arena(_size + 0x100),
                                             _FREELOG, _LOG, _G49A8] + [(f"$s{i}", 0, 0x10) for i in range(12)],
                                      vectors=[(f"$s{i}", _FLAGS[i]) for i in range(12)])

# crtValidatePointer: c3cm's hook (shim/src/re/c3cm.cpp), new key over IsBadWritePtr stood in (re/c3_hold.tsv release)
_VP = [(0x1000, 0), (0x1000, 1), (0x1010, 0), (0x1010, 1), (0x20000, 4), (0x20010, 4), (0x7FFE0000, 0x10), (0x7FFE0010, 0x11),
       (0, 0), (0, 1), (0x12345670, 0xFFFFFFFF), (0xFFFFFFF0, 2)]
HOOKS["crtValidatePointer_stand"] = dict(_SND, addr=0x00049604, abi="default", ret="int", args=["uint32", "uint32"],
                                         fixture="c3fd_valid", state=[_IMPLOG], vectors=_VP)

# ---- B. MultiSound constructors / destructor, Seq_dtor ---------------------------------------------------------------
_MSOBJ = [(f"$o{i}", 0, 0x1500) for i in range(12)]
HOOKS["MultiSound_ctor"] = dict(_SND, addr=0x00039690, abi="thiscall", ret="uint32", args=["pointer"], fixture="c3fd_msctor",
                                state=_MSOBJ + [_LOG, _G49A8], vectors=[(f"$o{i}",) for i in range(12)])
_TYPES = [0, 1, 2, 7, 0xFFFFFFFF, 0x80000000, 3, 0x1234, 5, 6, 0x7F, 4]
HOOKS["MultiSound_ctorWithType"] = dict(_SND, addr=0x00039790, abi="thiscall", ret="uint32", args=["pointer", "uint32"],
                                        fixture="c3fd_msctor", state=_MSOBJ + [_LOG, _G49A8],
                                        vectors=[(f"$o{i}", _TYPES[i]) for i in range(12)])

_MSD_MEMBERS = [[], [], [1], [1, 0], [1, 1, 1], [0, 1, 0, 1], [], [0], [1, 1], [], [1, 1, 0, 1, 1], [1]]
HOOKS["MultiSound_dtor"] = dict(_SND, addr=0x00039870, abi="thiscall", ret="void", args=["pointer"], fixture="c3fd_msdtor",
                                state=_MSOBJ + [(f"$str{i}", 0, 0x20) for i in range(12)]
                                + [(f"$m{i}_{j}", 0, 0x200) for i, ms in enumerate(_MSD_MEMBERS) for j in range(len(ms))]
                                + [_arena(0x100), _FREELOG, _LOG, _G49A8],
                                vectors=[(f"$o{i}",) for i in range(12)])

_SQD = [0, 0, 1, 2, 3, 2, 4, 3, 1, 5, 2, 3]       # node counts per row
HOOKS["Seq_dtor"] = dict(_SND, addr=0x0001E2B0, abi="thiscall", ret="void", args=["pointer"], fixture="c3fd_seqdtor",
                         state=[(f"$o{i}", 0, 0x200) for i in range(12)]
                         + [(f"$n{i}_{j}", 0, 0x10) for i, n in enumerate(_SQD) for j in range(n)]
                         + [_arena(0x100), _FREELOG, _LOG, _G49A8],
                         vectors=[(f"$o{i}",) for i in range(12)])

# ---- C. command queue, voice notification, sequencer and MIDI device rows --------------------------------------------
_FQ = [[], [1], [1, 2, 3, 4], [4, 0, 2, 5, 9], [0, 4], [6, 1, -1, 4, 3, 0], [2, 3], [4, 4, 1], [1, 1], [3],
       [0x80000001, 4, 2], [4, 0, 0], [1, 2]]
_fq_state = []
for _i, _es in enumerate(_FQ):
    for _j, _t in enumerate(_es):
        if _t == 0:
            continue
        _fq_state.append((f"$e{_i}_{_j}", 0, 0x10))
        if (_t & 0xFFFFFFFF) == 4 and (_j & 1) == 0:
            _fq_state.append((f"$x{_i}_{_j}", 0, 0x200))
HOOKS["Wave_Device::flushCommandQueue"] = dict(_SND, addr=0x0000CC70, abi="thiscall", ret="uint32", args=["pointer"],
                                               fixture="c3fd_flush",
                                               state=[(f"$o{i}", 0, 0x4200) for i in range(13)] + _fq_state
                                               + [_arena(0x100), _FREELOG, _LOG, _G49A8],
                                               vectors=[(f"$o{i}",) for i in range(13)])

# slot specs as in the fixture: (slot, count, flag, answer, member specs)
_NV = [[], [(0, 0, 0, 0)], [(3, 1, 1, 0)], [(0x7F, 2, 0, 1, [0, 1])], [(5, 3, 0, 0, [1, 0, 1]), (6, 0, 1, 1)],
       [(1, 1, 1, 1), (0x40, 2, 0, 0, [1, 1])], [(0, 4, 0, 0, [0, 0, 1, 1])], [(2, -1, 1, 0)],
       [(9, 2, 0, 0, [0, 0]), (10, 2, 1, 1, [1, 1])], [(0x7E, 3, 1, 0, [1, 1, 0])], [(4, 1, 0, 1), (8, 1, 0, 0), (12, 1, 1, 0)],
       [(0, 5, 0, 1, [1, 0, 0, 1, 0])]]
_nv_state = []
for _i, _slots in enumerate(_NV):
    _nv_state.append((f"$v{_i}", 0, 0x204))
    for _k, _s in enumerate(_slots):
        _nv_state.append((f"$g{_i}_{_k}", 0, 0x1500))
        for _j in range(len(_s[4]) if len(_s) > 4 else 0):
            _nv_state.append((f"$m{_i}_{_k}_{_j}", 0, 0x200))
HOOKS["Voice_notifyChannelNotes"] = dict(_SND, addr=0x0001AC40, abi="thiscall", ret="uint32", args=["pointer"],
                                         fixture="c3fd_notify", state=_nv_state + [("$gdev", 0, 4), _LOG, _G49A8],
                                         vectors=[(f"$v{i}",) for i in range(12)])

_AO_LIST = [0, 1, 3, 3, 1, 2, 0, 4, 1, 2, 2, 1]
_AO_VS = [0, 1, 2, 1, 3, 1, 1, 0, 2, 1, 1, 2]
_ao_state = []
for _i in range(12):
    _ao_state.append((f"$o{_i}", 0, 0xA00))
    _ao_state += [(f"$n{_i}_{j}", 0, 0x10) for j in range(_AO_LIST[_i])]
    _ao_state += [(f"$w{_i}_{k}", 0, 0x40) for k in range(_AO_VS[_i])]
_AO_FLAG = [0, 1, 0, 0xFF, 0, 1, 0, 0x80, 0, 2, 0, 1]
for _k, _extra in (("h", [("$gdev", 0, 4)]), ("n", [("$gdev", 0, 4)]), ("nodev", [("$gdev", 0, 4)])):
    HOOKS[f"Voice_allNotesOff_{_k}"] = dict(_SND, addr=0x00020D80, abi="thiscall", ret="uint32", args=["pointer", "uint32"],
                                            fixture=f"c3fd_alloff_{_k}",
                                            state=_ao_state + _extra + [("$gmd", 0, 4), ("$md", 0, 0x40), _LOG, _G49A8],
                                            vectors=[(f"$o{i}", _AO_FLAG[i]) for i in range(12)])


def _seq_state(n_objs, size=0x40):
    s = []
    for i in range(n_objs):
        s.append((f"$o{i}", 0, 0x400))
        s += [(f"$t{i}_{k}", 0, size) for k in range(16)]
    return s


_RP_V = [(0, 7), (0, 7), (0, 4), (2, 15), (0, 15), (0, 0), (1, 2), (0, 3), (1, 1), (3, 9), (0, 5), (2, 1)]
HOOKS["Seq_replayEventRange"] = dict(_SND, addr=0x00023950, abi="thiscall", ret="uint32", args=["pointer", "uint32", "uint32"],
                                     fixture="c3fd_replay", state=_seq_state(12) + [_LOG, _G49A8],
                                     vectors=[(f"$o{i}",) + _RP_V[i] for i in range(12)]
                                     + [("$o0", 5, 3), ("$o5", 0, 1), ("$o2", 0, 5), ("$o11", 2, 2)])

for _k in ("h", "n"):
    HOOKS[f"Midi_Device::emitEvent_{_k}"] = dict(
        _SND, addr=0x000246A0, abi="thiscall", ret="uint32", args=["pointer", "uint32"], fixture=f"c3fd_emit_{_k}",
        state=_seq_state(4, 0x460) + [("$gmd", 0, 4), ("$md", 0, 0x40), _LOG, _G49A8] + ([_IMPLOG] if _k == "h" else []),
        vectors=[(f"$o{i % 4}", k) for i, k in enumerate([0, 1, 2, 3, 4, 5, 6, 7, 9, 11, 12, 15, 16, 0x24])])

_SM_CH = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 15]
for _k in ("h", "n"):
    HOOKS[f"Midi_Device::sendShortMsg_{_k}"] = dict(
        _SND, addr=0x00021A90, abi="thiscall", ret="uint32", args=["pointer", "pointer", "uint32", "pointer"],
        fixture=f"c3fd_short_{_k}",
        state=[("$dev", 0, 0x1000)] + [(f"$e{v}", 0, 0x40) for v in range(4)] + [(f"$ea{v}", 0, 0x220) for v in range(4)]
        + [(f"$t{i}", 0, 0x500) for i in range(12)] + [(f"$c{i}", 0, 0x1500) for i in range(12)]
        + [(f"$msg{i}", 0, 0x10) for i in range(12)] + [("$gmd", 0, 4), ("$md", 0, 0x40), _LOG, _G49A8]
        + ([_IMPLOG] if _k == "h" else []),
        vectors=[("$dev", f"$msg{i}", _SM_CH[i], f"$t{i}") for i in range(12)])

HOOKS["Midi_Device::playEvent"] = dict(
    _SND, addr=0x00020870, abi="thiscall", ret="uint32", args=["pointer", "pointer", "pointer"], fixture="c3fd_playev",
    state=[("$dev", 0, 0x800)] + [(f"${p}{i}", 0, n) for i in range(17) for p, n in (("b", 0x10), ("t", 0x60), ("c", 4))]
    + [_LOG, _G49A8],
    vectors=[("$dev", f"$t{i}", f"$c{i}") for i in range(17)] + [("$dev", 0, "$c0")])

_ps_state = []
for _i in range(12):
    _ps_state += [(f"$o{_i}", 0, 0x200), (f"$s{_i}", 0, 0x200), (f"$old{_i}", 0, 0x10)]
for _k, _fx in (("", "c3fd_playsnd"), ("_off", "c3fd_playsnd_off")):
    HOOKS[f"Engine_playSound{_k}"] = dict(_SND, addr=0x0002ADD0, abi="thiscall", ret="uint32", args=["pointer", "pointer"],
                                          fixture=_fx,
                                          state=_ps_state + [("$gon", 0, 4), _arena(0x400), _FREELOG, _LOG, _G49A8],
                                          vectors=[(f"$o{i}", f"$nm{i}") for i in range(12)])


# ---- D. second group -------------------------------------------------------------------------------------------------
for _name, _addr, _fx, _dsz in (("Midi_Device::scanCommandRing", 0x0000AAF0, "c3fd_scan_midi", 0x4100),
                                ("Wave_Device::scanCommandRing", 0x0000DCF0, "c3fd_scan_wave", 0x4200)):
    HOOKS[_name] = dict(_SND, addr=_addr, abi="thiscall", ret="uint32", args=["pointer", "pointer", "pointer"], fixture=_fx,
                        state=[(f"$d{i}", 0, _dsz) for i in range(12)]
                        + [("$idx", 0, 0x60), ("$ents", 0, 0x800), ("$objs", 0, 0x300), _arena(0x100), _FREELOG, _LOG, _G49A8],
                        vectors=[(f"$d{i}", f"$fa{i}", f"$la{i}") for i in range(12)])

_LIST_STATE = [("$nodes", 0, 0x400), ("$pays", 0, 0x400), _arena(0x100), _FREELOG, _LOG, _G49A8]
HOOKS["Sound::dtor"] = dict(_SND, addr=0x00004000, abi="thiscall", ret="void", args=["pointer"], fixture="c3fd_sounddtor",
                            state=[(f"$o{i}", 0, 0xA60) for i in range(12)] + _LIST_STATE,
                            vectors=[(f"$o{i}",) for i in range(12)])
for _k in ("wi", "nowi"):
    HOOKS[f"streamReaderDtor_{_k}"] = dict(_SND, addr=0x000312A0, abi="thiscall", ret="void", args=["pointer"],
                                           fixture=f"c3fd_srdtor_{_k}",
                                           state=[(f"$o{i}", 0, 0x80) for i in range(12)] + _LIST_STATE
                                           + [("$gopen", 0, 4), ("$gwi", 0, 4), ("$wi", 0, 0x40), _IMPLOG],
                                           vectors=[(f"$o{i}",) for i in range(12)])

_SS_IDX = [0, 0x28, 0xFFFFFFFF, 3, 3, 5, 0x27, 0, 9, 1, 0x10, 2]
HOOKS["MultiSound_setSlotSource"] = dict(_SND, addr=0x0002F8D0, abi="thiscall", ret="uint32", args=["pointer", "uint32"],
                                         fixture="c3fd_setslot",
                                         state=[(f"$o{i}", 0, 0x200) for i in range(12)] + [(f"$src{i}", 0, 0x100) for i in range(12)]
                                         + [_arena(0x1500), _FREELOG, _LOG, _G49A8],
                                         vectors=[(f"$o{i}", _SS_IDX[i]) for i in range(12)])

HOOKS["Voice_startPlayback"] = dict(_SND, addr=0x00020BE0, abi="thiscall", ret="uint32", args=["pointer"], fixture="c3fd_startplay",
                                    state=[(f"$o{i}", 0, 0x800) for i in range(12)]
                                    + [("$trk", 0, 0x3000), ("$ix", 0, 0xC0), ("$gmd", 0, 4), _IMPLOG, _LOG, _G49A8],
                                    vectors=[(f"$o{i}",) for i in range(12)])

_DC_V = [("$o0", 0, 0, 0), ("$o1", "$c1", 2, 0), ("$o2", "$c2", 0, 0), ("$o3", "$c3", 0, 0), ("$o4", "$c4", 0, 0),
         ("$o5", "$c5", 0, 0), ("$o6", "$c6", 0, 4), ("$o7", "$c7", 1, 1), ("$o8", "$c8", 0, -1), ("$o9", "$c9", 0, 9),
         ("$o10", "$c10", 0, 0), ("$o11", "$c11", 0, 0), ("$o3", "$c12", 0, 0), ("$o5", "$c13", 0, 0), ("$o6", "$c14", 0, 20)]
HOOKS["Seq_dispatchChannelCmd"] = dict(_SND, addr=0x00024040, abi="thiscall", ret="uint32",
                                       args=["pointer", "pointer", "uint32", "int"], fixture="c3fd_dispatch",
                                       state=[(f"$o{i}", 0, 0x800) for i in range(12)]
                                       + [("$trk", 0, 0x470 * 96), ("$cmd", 0, 0x40 * 15), _LOG, _G49A8],
                                       vectors=_DC_V)


def _dsb_state(n):
    s = [("$fmt", 0, 0x20), ("$desc", 0, 0x10), _LOG, _G49A8]
    for i in range(n):
        s += [(f"$o{i}", 0, 0x100), (f"$ds{i}", 0, 0x200), (f"$b{i}", 0, 0x200), (f"$q{i}", 0, 0x200), (f"$q2_{i}", 0, 0x200)]
    return s


HOOKS["DsBuffer::create_hr"] = dict(_SND, addr=0x0000FBB0, abi="thiscall", ret="uint32", args=["pointer", "pointer", "pointer"],
                                    fixture="c3fd_dsb_hr", state=_dsb_state(14),
                                    vectors=[(f"$o{i}", f"$ds{i}", 0 if i == 12 else "$desc") for i in range(14)])
HOOKS["DsBuffer::create_ok"] = dict(_SND, addr=0x0000FBB0, abi="thiscall", ret="uint32", args=["pointer", "pointer", "pointer"],
                                    fixture="c3fd_dsb_ok", state=_dsb_state(12),
                                    vectors=[(f"$o{i}", f"$ds{i}", "$desc") for i in range(12)])


# ---- E. third group --------------------------------------------------------------------------------------------------
_CH_STATE = ([(f"$o{i}", 0, 0x1400) for i in range(12)]
             + [("$cnodes", 0, 0x800), ("$cpays", 0, 0x1000), ("$streams", 0, 0x3C0), ("$mnodes", 0, 0x100),
                ("$gmh", 0, 4), ("$gmt", 0, 4), ("$gmc", 0, 4), ("$gmn", 0, 4), ("$gopen", 0, 4), _IMPLOG, _arena(0x100),
                _FREELOG, _LOG, _G49A8])
for _l in ("l1", "l2"):
    HOOKS[f"Channel_releaseBuffers_{_l}"] = dict(_SND, addr=0x000340B0, abi="thiscall", ret="uint32", args=["pointer"],
                                                 fixture=f"c3fd_relbufs_{_l}", state=_CH_STATE + [("$gwd", 0, 4)],
                                                 vectors=[(f"$o{i}",) for i in range(12)])
    HOOKS[f"Channel_closeAndFree_{_l}"] = dict(_SND, addr=0x00033C70, abi="thiscall", ret="void", args=["pointer"],
                                               fixture=f"c3fd_closefree_{_l}", state=_CH_STATE,
                                               vectors=[(f"$o{i}",) for i in range(12)])
HOOKS["Midi_Device::drainCommandQueue"] = dict(_SND, addr=0x0000A330, abi="thiscall", ret="uint32", args=["pointer"],
                                               fixture="c3fd_drain",
                                               state=[(f"$d{i}", 0, 0x4100) for i in range(12)]
                                               + [("$ents", 0, 0x600), ("$vox", 0, 0x200 * 72), ("$dnodes", 0, 0x600),
                                                  _arena(0x100), _FREELOG, _LOG, _G49A8],
                                               vectors=[(f"$d{i}",) for i in range(12)])
for _k in ("a", "b", "c", "d"):
    HOOKS[f"Midi_Device::shutdown_{_k}"] = dict(
        _SND, addr=0x00009E40, abi="thiscall", ret="uint32", args=["pointer"], fixture=f"c3fd_shutdown_{_k}",
        state=[(f"$d{i}", 0, 0x100) for i in range(12)]
        + [("$vox", 0, 0xA00), ("$tn", 0, 0x80), ("$cn", 0, 0x80), ("$cp", 0, 0x100), ("$tmr", 0, 0x40),
           ("$gth", 0, 4), ("$gtt", 0, 4), ("$gtc", 0, 4), ("$gtn", 0, 4), ("$gch", 0, 4), ("$gct", 0, 4), ("$gcn", 0, 4),
           ("$gtm", 0, 4), _IMPLOG, _arena(0x100), _FREELOG, _LOG, _G49A8],
        vectors=[(f"$d{i}",) for i in range(12)])

HOOKS["buildVoiceTxName"] = dict(_SND, addr=0x0002F220, abi="thiscall", ret="uint32", args=["pointer"], fixture="c3fd_txname",
                                 state=[(f"$o{i}", 0, 0x100) for i in range(12)] + [_arena(0x200), _FREELOG, _LOG, _G49A8],
                                 vectors=[(f"$o{i}",) for i in range(12)])

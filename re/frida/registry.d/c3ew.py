# C3 batch c3ew (round 32): sound.dll rows earlier batches dropped for heap use, imports, live module globals or a constant result.
# Module-aware hooks; addr is the RVA, VA = 0x10000000 + RVA. Reimplementations in shim/src/re/c3ew.cpp, fixtures in
# re/frida/js/fixtures.d/c3ew.js, Purposes and branch tables in log/c3/c3ew_purpose.md, notes in log/c3/c3ew_notes.txt.
# Stand-ins (one call per key, inside the fixture): heap stand-in (`$arena`, `$freelog`), import stand-in (`$implog`) and the
# thread-filtered recording stubs on incremental-link thunks plus recording fixture vtables (`$log`). Every swapped module global
# is a 4-byte state region.

_SND = dict(module="sound.dll")
_LOG = ("$log", 0, 0x200)
_IMPLOG = ("$implog", 0, 0x400)
_FREELOG = ("$freelog", 0, 0x404)
_G49A8 = ("$g49a8", 0, 4)

HOOKS = {}

# ---------------------------------------------------------------------------------------------------------------------
# A. constructors
# ---------------------------------------------------------------------------------------------------------------------
_OBJS = [(f"$o{i}", 0, 0x240) for i in range(12)]
HOOKS["WaveInDeviceBase_ctor"] = dict(_SND, addr=0x0002A240, abi="thiscall", ret="uint32", args=["pointer"], fixture="c3ew_objs",
                                      state=_OBJS, vectors=[(f"$o{i}",) for i in range(12)])
HOOKS["MixBuffer_ctor"] = dict(_SND, addr=0x0002CEE0, abi="thiscall", ret="uint32", args=["pointer"], fixture="c3ew_objs",
                               state=_OBJS, vectors=[(f"$o{i}",) for i in range(12)])

# ---------------------------------------------------------------------------------------------------------------------
# B. wave-in device wrappers
# ---------------------------------------------------------------------------------------------------------------------
for _k in ("null", "dev"):
    HOOKS[f"Wave_In_Device::closeCapture_{_k}"] = dict(
        _SND, addr=0x0000EC00, abi="thiscall", ret="uint32", args=["pointer"], fixture=f"c3ew_closecap_{_k}",
        state=[(f"$o{i}", 0, 0x80) for i in range(12)] + [_LOG, _G49A8, ("$gtmr", 0, 4), ("$gwi", 0, 4)]
        + ([("$dev", 0, 0x40)] if _k == "dev" else []),
        vectors=[(f"$o{i}",) for i in range(12)])
for _k in ("null", "open", "prep", "add", "ok", "last"):
    HOOKS[f"Wave_In_Device::start_{_k}"] = dict(
        _SND, addr=0x0000EA30, abi="default", ret="uint32", args=[], fixture=f"c3ew_wistart_{_k}",
        state=[_LOG, _G49A8, ("$gwi", 0, 4)] + ([] if _k == "null" else [("$dev", 0, 0x80)]), vectors=[()])
_A12 = [0, 1, 0x1234, 0xFFFFFFFF, 0x80000000, 0x5555, 2, 0x100, 0xDEADBEEF, 0x41, 7, 0x7FFFFFFF]
for _k in ("busy", "ok", "err", "oom", "oom0"):
    HOOKS[f"Wave_In_Device::openCapture_{_k}"] = dict(
        _SND, addr=0x0000EAD0, abi="thiscall", ret="uint32", args=["pointer", "uint32", "uint32"], fixture=f"c3ew_opencap_{_k}",
        state=[(f"$o{i}", 0, 0x80) for i in range(12)] + [_LOG, _G49A8, ("$gwi", 0, 4), ("$arena", 0, 0x210), _FREELOG]
        + ([("$dev", 0, 0x40)] if _k == "busy" else []),
        vectors=[(f"$o{i}", _A12[i], _A12[(i * 5 + 3) % 12]) for i in range(12)])
for _k in ("ok", "oom"):
    HOOKS[f"WaveInStream_resetCodec_{_k}"] = dict(
        _SND, addr=0x000319E0, abi="thiscall", ret="uint32", args=["pointer"], fixture=f"c3ew_rcodec_{_k}",
        state=[(f"$o{i}", 0, 0xA0) for i in range(12)] + [_LOG, _G49A8, ("$gstrm", 0, 4), ("$arena", 0, 0x2610), ("$gcodec", 0, 12)],
        vectors=[(f"$o{i}",) for i in range(12)])
for _k in ("null", "strm"):
    HOOKS[f"freeMmioHandleListGetInfo_{_k}"] = dict(
        _SND, addr=0x00031560, abi="thiscall", ret="uint32", args=["pointer"], fixture=f"c3ew_freemmio_{_k}",
        state=[(f"$b{i}", 0, 0x400) for i in range(12)] + [_LOG, _G49A8, ("$gstrm", 0, 4), ("$cnt", 0, 4), _FREELOG, _IMPLOG],
        vectors=[(f"$b{i}",) for i in range(12)])

# ---------------------------------------------------------------------------------------------------------------------
# C. Wave_Device bodies (fixture c3ew_wd: twelve 0x200-byte objects, swapped globals `$g<rva>`, fresh blocks `$blk<rva>`,
#    the device block `$dev`)
# ---------------------------------------------------------------------------------------------------------------------
_W = [(f"$w{i}", 0, 0x200) for i in range(12)]


def _wd(globs, blks=(), dev=False, free=False, extra=()):
    st = _W + [_LOG, _G49A8] + [(f"$g{r:x}", 0, 4) for r in globs] + [(f"$blk{r:x}", 0, 0x200) for r in blks]
    if dev:
        st.append(("$dev", 0, 0x200))
    if free:
        st.append(_FREELOG)
    return st + list(extra)


_T, _DEV, _STRM, _B0, _B1, _B2, _MIDI, _COOP = 0xB49EC, 0xB49F0, 0xB49F4, 0xB49F8, 0xB49FC, 0xB4A00, 0xB4A1C, 0xB4A3C
_V12 = [(f"$w{i}",) for i in range(12)]
_SO = {"all": ([_T, _B0, _B1, _B2]), "none": ([]), "mix": ([_T, _B2]), "mix2": ([_B0, _B1])}
for _k, _b in _SO.items():
    HOOKS[f"Wave_Device::startOutput_{_k}"] = dict(
        _SND, addr=0x0000BF90, abi="thiscall", ret="uint32", args=["pointer"], fixture=f"c3ew_so_{_k}",
        state=_wd([_T, _DEV, _B0, _B1, _B2], _b, dev=True), vectors=_V12)
_STO = {"all": [_T, _STRM, _B0, _B1, _B2], "none": [_T], "mix": [_T, _STRM, _B1], "mix2": [_T, _B0, _B1, _B2]}
for _k, _b in _STO.items():
    HOOKS[f"Wave_Device::stopOutput_{_k}"] = dict(
        _SND, addr=0x0000BEF0, abi="thiscall", ret="uint32", args=["pointer"], fixture=f"c3ew_sto_{_k}",
        state=_wd([_T, _DEV, _STRM, _B0, _B1, _B2], _b, dev=True), vectors=_V12)
_CO = {"all": ([_T, _MIDI, _STRM, _B0, _B1, _B2], True), "none": ([_T, _B0, _B2], False), "part": ([_T, _STRM, _B1], True)}
for _k, (_b, _d) in _CO.items():
    HOOKS[f"Wave_Device::closeOutput_{_k}"] = dict(
        _SND, addr=0x0000BE00, abi="thiscall", ret="uint32", args=["pointer"], fixture=f"c3ew_co_{_k}",
        state=_wd([_T, _MIDI, _DEV, _STRM, _B0, _B1, _B2], _b, dev=_d), vectors=_V12)
HOOKS["Wave_Device::ctor"] = dict(_SND, addr=0x0000B1D0, abi="thiscall", ret="pointer", args=["pointer"], fixture="c3ew_ctor",
                                  state=_wd([_DEV, _B0, _STRM]), vectors=_V12)
for _k in ("dev", "null"):
    HOOKS[f"Wave_Device::dtor_{_k}"] = dict(_SND, addr=0x0000B2E0, abi="thiscall", ret="uint32", args=["pointer"],
                                            fixture=f"c3ew_dtor_{_k}", state=_wd([_DEV], free=True), vectors=_V12)
_STP = {"all": ([_T, _STRM, _B0, _B1, _B2], True), "none": ([_T, _B0], False), "dev0": ([_T, _STRM, _B1, _B2], True)}
for _k, (_b, _d) in _STP.items():
    HOOKS[f"Wave_Device::stop_{_k}"] = dict(
        _SND, addr=0x0000B960, abi="thiscall", ret="uint32", args=["pointer"], fixture=f"c3ew_stop_{_k}",
        state=_wd([_T, _DEV, _STRM, _B0, _B1, _B2], _b, dev=_d, free=True, extra=[("$gmb", 0, 4)]), vectors=_V12)
_IDX = [0, 1, 5, 6, 7, 0xFFFFFFFF, 2, 3, 0x80000000, 4, 0x100, 5]
for _k in ("null", "seek", "create", "cbuf", "ok", "ok0"):
    _globs = [_T, _DEV] if _k == "null" else [_T, _DEV, _STRM, _B0, _COOP]
    _blks = [_T] if _k == "null" else ([_T, _STRM] if _k == "ok0" else [_T, _STRM, _B0])
    HOOKS[f"Wave_Device::restart_{_k}"] = dict(
        _SND, addr=0x0000C020, abi="thiscall", ret="uint32", args=["pointer", "uint32"], fixture=f"c3ew_rs_{_k}",
        state=_wd(_globs, _blks, dev=_k != "null", extra=[("$buf", 0, 0x100)]),
        vectors=[(f"$w{i}", _IDX[i]) for i in range(12)])

# ---------------------------------------------------------------------------------------------------------------------
# D. the sound timer and a timed channel start; E. enqueueWaveCmd10
# ---------------------------------------------------------------------------------------------------------------------
_TST = [(f"$t{i}", 0, 0x100) for i in range(12)] + [_G49A8, ("$cs", 0, 24), _FREELOG, _IMPLOG]
HOOKS["SoundTimer::init"] = dict(_SND, addr=0x000089F0, abi="thiscall", ret="pointer", args=["pointer"], fixture="c3ew_timer",
                                 state=_TST, vectors=[(f"$t{i}",) for i in range(12)])
# ret void: on the NULL-value exit eax is operator delete's, which free leaves undefined; the only caller, release_sound
# (0x100093ac), discards it (`push esi; call 0x1004249a ... xor eax, eax` 0x100093b1-0x100093c4).
HOOKS["SoundTimer::stop"] = dict(_SND, addr=0x00008AC0, abi="thiscall", ret="void", args=["pointer"], fixture="c3ew_timer",
                                 state=_TST, vectors=[(f"$t{i}",) for i in range(12)])
for _k in ("tmr", "null"):
    HOOKS[f"Channel_startVoiceTimed_{_k}"] = dict(
        _SND, addr=0x00039E70, abi="thiscall", ret="uint32", args=["pointer", "uint32"], fixture=f"c3ew_chtimed_{_k}",
        state=[(f"$c{i}", 0, 0x200) for i in range(12)] + [_LOG, _G49A8, ("$gtmr", 0, 4)] + ([("$tmr", 0, 0x80)] if _k == "tmr" else []),
        vectors=[(f"$c{i}", _A12[(i * 7 + 2) % 12]) for i in range(12)])
for _k in ("eng", "oom", "null"):
    HOOKS[f"enqueueWaveCmd10_{_k}"] = dict(
        _SND, addr=0x0002FC10, abi="thiscall", ret="uint32", args=["pointer"], fixture=f"c3ew_enq10_{_k}",
        state=[(f"$e{i}", 0, 0x200) for i in range(12)] + [(f"$q{i}", 0x1410, 8) for i in range(1, 12, 2)]
        + [_LOG, _G49A8, ("$geng", 0, 4), ("$arena", 0, 0x110)] + ([("$eng", 0, 0x80)] if _k != "null" else []),
        vectors=[(f"$e{i}",) for i in range(12)])

# ---------------------------------------------------------------------------------------------------------------------
# F. sequencer command handlers (fixture c3ew_seq8 / c3ew_seq9; the find ids match c3ew_seqfind)
# ---------------------------------------------------------------------------------------------------------------------
_SEQFIND = [1, 0x55, 9, 7, 8, 6, 3, 5, 0xA, 0xF, 2, 1]
HOOKS["Seq_cmdHandler8"] = dict(_SND, addr=0x00023540, abi="thiscall", ret="uint32", args=["pointer", "uint32"], fixture="c3ew_seq8",
                                state=[(f"$s{i}", 0, 0x800) for i in range(12)] + [_LOG, _G49A8],
                                vectors=[(f"$s{i}", _SEQFIND[i]) for i in range(12)])
HOOKS["Seq_cmdHandler9"] = dict(_SND, addr=0x000238B0, abi="thiscall", ret="uint32", args=["pointer", "uint32"], fixture="c3ew_seq9",
                                state=[(f"$s{i}", 0, 0x800) for i in range(12)] + [_LOG, _G49A8],
                                vectors=[(f"$s{i}", _SEQFIND[i]) for i in range(12)])
HOOKS["Seq_allocTrackSlot"] = dict(_SND, addr=0x000247C0, abi="thiscall", ret="uint32", args=["pointer"], fixture="c3ew_seqalloc",
                                   state=[(f"$q{i}", 0, 0x1000) for i in range(12)] + [_LOG, _G49A8, ("$ctr", 0, 4)],
                                   vectors=[(f"$q{i}",) for i in range(12)])

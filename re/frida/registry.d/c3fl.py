# C3 batch c3fl (round 34): sound.dll THREAD / LIVE_STATE rows c3fe left (log/c3/c3fl_notes.txt). Module-aware hooks; addr is the
# RVA, VA = 0x10000000 + RVA. Reimplementations in shim/src/re/c3fl.cpp, fixtures in re/frida/js/fixtures.d/c3fl.js, Purposes and
# branch tables in log/c3/c3fl_purpose.md. Every key freezes the first thread and holds the live multimedia timer off
# (0x100b49a8 = 5, restored to its idle value at the flush; SoundTimer::onTick's keys also clear bit 2 of the live timer's byte
# +0x3c); thunk stubs record into `$log`, the import stand-in into `$implog`, the heap stand-in's frees into `$freelog`; every
# swapped module global is a state region.

_SND = dict(module="sound.dll")
_IMPLOG = ("$implog", 0, 0x400)
_FREELOG = ("$freelog", 0, 0x404)


def _log(n):
    return ("$log", 0, n)


HOOKS = {}

# ---------------------------------------------------------------------------------------------------------------------
# A. DsBuffer::restore (a1..a6 passed to Lock unchanged)
# ---------------------------------------------------------------------------------------------------------------------
HOOKS["DsBuffer::restore"] = dict(
    _SND, addr=0x000101A0, abi="thiscall", ret="uint32", args=["pointer"] + ["uint32"] * 6, fixture="c3fl_dsr",
    state=[(f"$o{i}", 0, 0x80) for i in range(14)] + [(f"$c{i}", 0, 0x40) for i in range(14)]
    + [("$c2x", 0, 0x40), _log(0x400)],
    vectors=[(f"$o{i}", 0x100 * i, 0x40 + i, 0xA3A3A300 + i, 0xA4A4A400 + i, 0xA5A5A500 + i, 0xA6A6A600 + i)
             for i in range(14)])

# ---------------------------------------------------------------------------------------------------------------------
# B. DsBuffer::getPlayPosition (outA, outB)
# ---------------------------------------------------------------------------------------------------------------------
HOOKS["DsBuffer::getPlayPosition"] = dict(
    _SND, addr=0x000106B0, abi="thiscall", ret="uint32", args=["pointer", "pointer", "pointer"], fixture="c3fl_dpp",
    state=[(f"$o{i}", 0, 0x100) for i in range(14)] + [(f"$a{i}", 0, 8) for i in range(13)]
    + [(f"$b{i}", 0, 8) for i in range(14)] + [("$onull", 0, 0x100), ("$anull", 0, 8), ("$bnull", 0, 8), _log(0x200)],
    vectors=[(f"$o{i}", f"$a{i}", f"$b{i}") for i in range(14)] + [("$onull", "$anull", "$bnull")])

# ---------------------------------------------------------------------------------------------------------------------
# C. SoundTimer::onTick (uID, uMsg, timer, dw1, dw2): one key per value of the re-entrancy word 0x100b49a8 (0, 3) and one whose
#    EnterCriticalSection stand-in clears the flag of timer 1 (the second test at 0x10008f13)
# ---------------------------------------------------------------------------------------------------------------------
_TICK_STATE = [(f"$t{i}", 0, 0x60) for i in range(12)] + [_log(0x400), _IMPLOG, ("$dbg", 0, 0x80), ("$clock", 0, 8),
                                                          ("$gtick", 0, 4)]
_TICK_VECTORS = [(7 + i, 0, f"$t{i}", 0xD1D10000 + i, 0xD2D20000 + i) for i in range(12)]
for _k in ("w0", "w3"):
    HOOKS[f"SoundTimer::onTick_{_k}"] = dict(
        _SND, addr=0x00008ED0, abi="stdcall", ret="uint32", args=["uint32", "uint32", "pointer", "uint32", "uint32"],
        fixture=f"c3fl_tick_{_k}", state=_TICK_STATE, vectors=_TICK_VECTORS)
HOOKS["SoundTimer::onTick_flip"] = dict(
    _SND, addr=0x00008ED0, abi="stdcall", ret="uint32", args=["uint32", "uint32", "pointer", "uint32", "uint32"],
    fixture="c3fl_tick_flip", state=_TICK_STATE,
    vectors=[(1, 0, "$t1", 2, 3), (2, 0, "$t3", 4, 5), (3, 0, "$t0", 6, 7)])

# ---------------------------------------------------------------------------------------------------------------------
# D. Wave_Device::serviceTick: one key per device / writer setting
# ---------------------------------------------------------------------------------------------------------------------
_ST_STATE = [(f"$w{i}", 0, 0x200) for i in range(8)] + [(f"$b{k}", 0, 0x60) for k in range(8)] + \
    [("$wr", 0, 0x40), _log(0x400)] + [(f"${g}", 0, 4) for g in ("gdev", "gwr", "gwa", "gwb", "gwc")]
for _k in ("nodev", "empty", "wr0", "wa", "wb"):
    HOOKS[f"Wave_Device::serviceTick_{_k}"] = dict(
        _SND, addr=0x0000C230, abi="thiscall", ret="uint32", args=["pointer"], fixture=f"c3fl_st_{_k}",
        state=_ST_STATE if _k not in ("nodev", "empty") else
        [s for s in _ST_STATE if not s[0].startswith("$b")] + ([("$dev", 0, 0x200)] if _k == "empty" else []),
        vectors=[(f"$w{i}",) for i in range(8)])
    if _k not in ("nodev", "empty"):
        HOOKS[f"Wave_Device::serviceTick_{_k}"]["state"] = _ST_STATE + [("$dev", 0, 0x200)]

# ---------------------------------------------------------------------------------------------------------------------
# E. Dll_Midi_Device::create_device (out): g_midiDevice 0 (_null), 0 with a failing operator new (_fail), non-zero (_busy)
# ---------------------------------------------------------------------------------------------------------------------
for _k in ("null", "fail", "busy"):
    HOOKS[f"Dll_Midi_Device::create_device_{_k}"] = dict(
        _SND, addr=0x00009A90, abi="default", ret="uint32", args=["pointer"], fixture=f"c3fl_cd_{_k}",
        state=[(f"$p{i}", 0, 8) for i in range(12)] + [("$arena", 0, 0x4090), ("$gmidi", 0, 4), _log(0x100)],
        vectors=[(f"$p{i}",) for i in range(12)])

# ---------------------------------------------------------------------------------------------------------------------
# F. soundWindowSubclassProc: one key per value of the flag byte 0x100b49bc; every message class and node kind as a vector
# ---------------------------------------------------------------------------------------------------------------------
_H = 0x2222
_WP_VECTORS = [
    (_H, 0x0010, 5, 6), (_H, 0x03C0, 7, 8), (_H, 0x03BF, 9, 10), (_H, 0x07E9, 1, 2), (_H, 0x03C1, 3, 4),
    (_H, 0x07EA, 3, "$snd1"),
    (_H, 0x07EB, "$chA", 0), (_H, 0x07EB, "$chB", 0),
    (_H, 0x07EC, 0, "$oA"), (_H + 1, 0x07ED, 4, 5),
    (_H, 0x07EE, 0, "$n3a"), (_H, 0x07EE, 0, "$n3b"), (_H, 0x07EE, 0, "$n4a"), (_H, 0x07EE, 0, "$n4n"),
    (_H, 0x07EE, 0, "$n5"), (_H, 0x07EE, 0, "$n7"),
    (_H, 0x07EF, 0, "$n3a"), (_H, 0x07EF, 0, "$n4a"), (_H, 0x07EF, 0, "$n4n"), (_H, 0x07EF, 0, "$n5"), (_H, 0x07EF, 0, "$n7"),
    (_H, 0x07F0, 0x77, "$wiA"), (_H, 0x07F0, 0x78, "$wiB"), (_H + 2, 0x07F0, 0x79, 0),
    (_H, 0x07F1, 1, 1), (_H + 3, 0xFFFFFFFF, 2, 2), (_H, 0x07E8, 0x7e8, 0x7e8),
]
_WP_STATE = [_log(0x400), _FREELOG, _IMPLOG, ("$gflag", 0, 4), ("$gprev", 0, 4)] + \
    [(f"${n}", 0, 0x1430) for n in ("oA", "oB", "oC", "oD")] + \
    [(f"${n}", 0, 0x20) for n in ("n3a", "n3b", "n4a", "n4n", "n5", "n7")] + [("$chA", 0, 0x1400), ("$chB", 0, 0x1400)] + \
    [("$wiA", 0, 0x40), ("$wiB", 0, 0x40), ("$snd1", 0, 0x40)]
for _v in range(4):
    HOOKS[f"soundWindowSubclassProc_f{_v}"] = dict(
        _SND, addr=0x00028B70, abi="stdcall", ret="uint32", args=["pointer"] * 4, fixture=f"c3fl_wp_f{_v}",
        state=_WP_STATE, vectors=_WP_VECTORS)

# ---------------------------------------------------------------------------------------------------------------------
# G. serviceChannelPlayback: a NULL device (_nodev) and a fixture device (_dev)
# ---------------------------------------------------------------------------------------------------------------------
_SP_BUFS = ["b0", "b1", "b3", "b4", "b5", "b6", "b7", "b8", "b9", "b10", "b11", "b12", "b13",
            "l4_0", "l4_1", "l6_0", "l6_1", "l9_0"] + [f"d{i}" for i in range(14)] + [f"f{i}" for i in range(14)]
_SP_STATE = [(f"$c{i}", 0, 0x1400) for i in range(14)] + [(f"${b}", 0, 0x100) for b in _SP_BUFS] + \
    [(f"${n}", 0, 0x10) for n in ("n4_0", "n4_1", "n6_0", "n6_1", "n9_0")] + [_log(0x800), ("$gdev", 0, 4)]
for _k in ("nodev", "dev"):
    HOOKS[f"serviceChannelPlayback_{_k}"] = dict(
        _SND, addr=0x00034430, abi="thiscall", ret="uint32", args=["pointer"], fixture=f"c3fl_sp_{_k}",
        state=_SP_STATE + ([("$dev", 0, 0x200)] if _k == "dev" else []),
        vectors=[(f"$c{i}",) for i in range(14)])

# ---------------------------------------------------------------------------------------------------------------------
# H. flushSoundList, argument-free: one key per list
# ---------------------------------------------------------------------------------------------------------------------
_FL_OBJS = {
    "empty": [], "nullobj": [],
    "k1": [f"o{i}" for i in range(7)],
    "mixed": ["q0", "y", "x", "q1", "q2", "q3", "q4", "q5", "z"],
    "cut": ["x0", "x1", "x2"], "cut2": ["x0", "x1"],
    "other": ["p0", "p9", "p1", "p2", "p3", "p4"],
}
_FL_NODES = {"empty": 0, "nullobj": 1, "k1": 7, "mixed": 3, "cut": 3, "cut2": 2, "other": 2}
for _k, _objs in _FL_OBJS.items():
    HOOKS[f"flushSoundList_{_k}"] = dict(
        _SND, addr=0x00008470, abi="default", ret="uint32", args=[], fixture=f"c3fl_fl_{_k}",
        state=[("$eng", 0, 0x200), ("$geng", 0, 4), _log(0x800), _FREELOG, _IMPLOG]
        + [(f"${o}", 0, 0x1430) for o in _objs] + [(f"$n{i}", 0, 0x10) for i in range(_FL_NODES[_k])],
        vectors=[()])

# ---------------------------------------------------------------------------------------------------------------------
# I. Midi_Device::dispatchCommand: one key per g_soundEngine setting; six devices as vectors
# ---------------------------------------------------------------------------------------------------------------------
_MD_LISTS = {0: 0, 1: 3, 2: 1, 3: 0, 4: 2, 5: 2}
_MD_STATE = [(f"$d{i}", 0, 0x4060) for i in range(6)] + \
    [(f"$ch{i}{n}", 0, 0x220) for i in range(6) for n in "ABCNFX"] + \
    [(f"$ln{i}_{k}", 0, 0x10) for i, c in _MD_LISTS.items() for k in range(c)] + \
    [_log(0x1000), _FREELOG, _IMPLOG, ("$geng", 0, 4)]
for _k in ("post", "thread", "null"):
    HOOKS[f"Midi_Device::dispatchCommand_{_k}"] = dict(
        _SND, addr=0x0000A670, abi="thiscall", ret="uint32", args=["pointer"], fixture=f"c3fl_md_{_k}",
        state=_MD_STATE + ([] if _k == "null" else [("$eng", 0, 0x200)]),
        vectors=[(f"$d{i}",) for i in range(6)])

# ---------------------------------------------------------------------------------------------------------------------
# J. Midi_Device::noteOn (msg, ch, track): _hw (g_midiDevice+0x3c non-zero), _sw / _swbig (0; master volume 100 / 0x12345678)
# ---------------------------------------------------------------------------------------------------------------------
_NO_HW_CH = [3, 4, 0xF, 9, 0, 0x12, 7, 1]
_NO_SW_CH = list(range(16))
_NO_STATE = [("$d", 0, 0xA00), ("$gm", 0, 0x40), ("$arena", 0, 0x28000), _log(0x400), _IMPLOG, ("$gmidi", 0, 4), ("$geng", 0, 4)]
for _k, _chs in (("hw", _NO_HW_CH), ("sw", _NO_SW_CH), ("swbig", _NO_SW_CH)):
    HOOKS[f"Midi_Device::noteOn_{_k}"] = dict(
        _SND, addr=0x000215C0, abi="thiscall", ret="uint32", args=["pointer", "pointer", "uint32", "pointer"],
        fixture=f"c3fl_no_{_k}", state=_NO_STATE,
        vectors=[("$d", f"$m{i}", c, f"$t{i}") for i, c in enumerate(_chs)])

# ---------------------------------------------------------------------------------------------------------------------
# K. renderChannelVoice (voice, buf, count, pass, out) on a channel: one key per g_soundEngine setting
# ---------------------------------------------------------------------------------------------------------------------
# [channel, count, pass] per vector, the same table as F.c3fl_rv_vs in fixtures.d/c3fl.js
_RV = [("M", 4, 0x10), ("M", 3, 0xC), ("M", 3, 0xB), ("S", 5, 0x20), ("E", 5, 0), ("M", 6, 0xD), ("M", 6, 1), ("S", 2, 0),
       ("M", 4, 0), ("M", 4, 0), ("M", 4, 0x30), ("M", 4, 0), ("M", 4, 0), ("M", 4, 0), ("M", 4, 0), ("M", 0, 0),
       ("O", 3, 0x40), ("E", 1, 0x10), ("O", 3, 0), ("M", 1, 0)]
_RV_STATE = [(f"$ch{c}", 0, 0x400) for c in "SEMO"] + [(f"${p}{i}", 0, n) for i in range(len(_RV))
                                                         for p, n in (("v", 0x120), ("b", 0x100), ("o", 8))] + \
    [_log(0x400), ("$geng", 0, 4)]
for _k in ("eng", "eng0", "noeng"):
    HOOKS[f"renderChannelVoice_{_k}"] = dict(
        _SND, addr=0x000371D0, abi="thiscall", ret="uint32",
        args=["pointer", "pointer", "pointer", "uint32", "uint32", "pointer"], fixture=f"c3fl_rv_{_k}",
        state=_RV_STATE + ([("$eng", 0, 0x40)] if _k != "noeng" else []),
        vectors=[(f"$ch{c}", f"$v{i}", f"$b{i}", n, p, f"$o{i}") for i, (c, n, p) in enumerate(_RV)])

# ---------------------------------------------------------------------------------------------------------------------
# L. writeWaveFile (buf) on a writer: one key per engine list (fixture L); per list entry (object?, busy block?)
# ---------------------------------------------------------------------------------------------------------------------
_WF_LISTS = {
    "noeng": None, "empty": [], "nullobj": [(0, 0)], "k6": [(1, 0)] * 4, "k4": [(1, 0)] * 4,
    "k1": [(1, 0), (1, 0), (1, 0), (1, 0), (1, 1), (1, 0), (1, 1), (1, 0), (1, 1)],
    "k1b": [(1, 0), (1, 0), (1, 0), (1, 1), (1, 0)], "mix": [(1, 0)] * 4,
}
_WF_VECTORS = [("$w0", 0), ("$w0", "$b0"), ("$w1", "$b1"), ("$w2", "$b2"), ("$w0", "$b3"), ("$w1", "$b4"), ("$w2", "$b5"),
               ("$w1", "$b3"), ("$w0", "$b5")]
_WF_BASE = [(f"$w{i}", 0, 0x60) for i in range(3)] + [(f"$b{j}", 0, 0x100) for j in range(6)] + \
    [(f"$p{j}", 0, 0x40) for j in range(6)] + [("$q4", 0, 0x40), ("$q5", 0, 0x40), _log(0x800), _IMPLOG, ("$geng", 0, 4)]
for _k, _l in _WF_LISTS.items():
    _st = list(_WF_BASE)
    if _l is not None:
        _st.append(("$eng", 0, 0x200))
        for _i, (_o, _c) in enumerate(_l):
            _st.append((f"$n{_i}", 0, 0x10))
            if _o:
                _st.append((f"$o{_i}", 0, 0x300))
            if _c:
                _st.append((f"$c{_i}", 0, 0x60))
    HOOKS[f"writeWaveFile_{_k}"] = dict(
        _SND, addr=0x00007010, abi="thiscall", ret="uint32", args=["pointer", "pointer"], fixture=f"c3fl_wf_{_k}",
        state=_st, vectors=_WF_VECTORS)

# ---------------------------------------------------------------------------------------------------------------------
# M. Wave_Device::dispatchCommand: one device per vector (all carved from $arena); one key per post / thread message path
# ---------------------------------------------------------------------------------------------------------------------
_WD_STATE = [("$arena", 0, 0x80000), ("$heaparena", 0, 0x2010), _log(0x1000), _FREELOG, _IMPLOG, ("$geng", 0, 4)]
for _k in ("post", "thread"):
    HOOKS[f"Wave_Device::dispatchCommand_{_k}"] = dict(
        _SND, addr=0x0000CD80, abi="thiscall", ret="uint32", args=["pointer"], fixture=f"c3fl_wd_{_k}",
        state=_WD_STATE, vectors=[(f"$dev{i}",) for i in range(8)])

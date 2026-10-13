# C3 batch c3fe (round 33): sound.dll THREAD / LIVE_STATE rows no game thread enters (log/c3/c3fe_notes.txt section 1).
# Module-aware hooks; addr is the RVA, VA = 0x10000000 + RVA. Reimplementations in shim/src/re/c3fe.cpp, fixtures in
# re/frida/js/fixtures.d/c3fe.js, Purposes and branch tables in log/c3/c3fe_purpose.md, notes in log/c3/c3fe_notes.txt.
# Every key freezes the first thread and holds the live multimedia timer off (0x100b49a8 = 5, restored to its idle value at
# the flush); thunk stubs record into `$log`, the import stand-in into `$implog`, the heap stand-in's frees into `$freelog`;
# every swapped module global is a state region.

_SND = dict(module="sound.dll")
_LOG = ("$log", 0, 0x200)
_IMPLOG = ("$implog", 0, 0x400)
_FREELOG = ("$freelog", 0, 0x404)

HOOKS = {}

# ---------------------------------------------------------------------------------------------------------------------
# A. release_sound (export #4), argument-free: one key per stub behaviour
# ---------------------------------------------------------------------------------------------------------------------
for _k in ("null", "tmr", "clr", "swap"):
    HOOKS[f"release_sound_{_k}"] = dict(
        _SND, addr=0x00009390, abi="default", ret="uint32", args=[], fixture=f"c3fe_rel_{_k}",
        state=[_LOG, _FREELOG, ("$gtmr", 0, 4), ("$t1", 0, 0x80), ("$t2", 0, 0x80)], vectors=[()])

# ---------------------------------------------------------------------------------------------------------------------
# B. pumpWaveMessages: twelve objects, 0-3 messages each
# ---------------------------------------------------------------------------------------------------------------------
HOOKS["pumpWaveMessages"] = dict(
    _SND, addr=0x00028810, abi="thiscall", ret="uint32", args=["pointer"], fixture="c3fe_pump",
    state=[(f"$o{i}", 0, 0x20) for i in range(12)] + [_LOG, _IMPLOG], vectors=[(f"$o{i}",) for i in range(12)])

# ---------------------------------------------------------------------------------------------------------------------
# C. soundThreadPump, argument-free: one key per wait / flag / message script
# ---------------------------------------------------------------------------------------------------------------------
for _k in ("event", "flag0", "one", "two", "drop"):
    HOOKS[f"soundThreadPump_{_k}"] = dict(
        _SND, addr=0x0002A970, abi="default", ret="uint32", args=[], fixture=f"c3fe_tp_{_k}",
        state=[_LOG, _IMPLOG, ("$sig", 0, 0x2c)], vectors=[()])

# ---------------------------------------------------------------------------------------------------------------------
# D. soundWindowProc: one key per value of the flag byte 0x100b49bc; every message class and node kind as a vector
# ---------------------------------------------------------------------------------------------------------------------
_H = 0x1111
_WP_VECTORS = [
    (_H, 0x0010, 5, 6), (_H, 0x03C0, 7, 8), (_H, 0x03C1, 9, 10), (_H, 0x07E9, 1, 2),
    (_H, 0x07EA, 3, "$snd1"),
    (_H, 0x07EB, "$chA", 0), (_H, 0x07EB, "$chB", 0),
    (_H, 0x07EC, 0, "$oA"), (_H + 1, 0x07ED, 4, 5),
    (_H, 0x07EE, 0, "$n3a"), (_H, 0x07EE, 0, "$n3b"), (_H, 0x07EE, 0, "$n4a"), (_H, 0x07EE, 0, "$n4n"),
    (_H, 0x07EE, 0, "$n5"), (_H, 0x07EE, 0, "$n7"),
    (_H, 0x07EF, 0, "$n3a"), (_H, 0x07EF, 0, "$n4a"), (_H, 0x07EF, 0, "$n4n"), (_H, 0x07EF, 0, "$n5"), (_H, 0x07EF, 0, "$n7"),
    (_H, 0x07F0, 0x77, "$wiA"), (_H, 0x07F0, 0x78, "$wiB"), (_H + 2, 0x07F0, 0x79, 0),
    (_H, 0x07F1, 1, 1), (_H + 3, 0xFFFFFFFF, 2, 2), (_H, 0x07E8, 0x7e8, 0x7e8),
]
_WP_STATE = [_LOG, _FREELOG, _IMPLOG, ("$gflag", 0, 4)] + [(f"${n}", 0, 0x1430) for n in ("oA", "oB", "oC", "oD")] + \
    [(f"${n}", 0, 0x20) for n in ("n3a", "n3b", "n4a", "n4n", "n5", "n7")] + [("$chA", 0, 0x1400), ("$chB", 0, 0x1400)] + \
    [("$wiA", 0, 0x40), ("$wiB", 0, 0x40), ("$snd1", 0, 0x40)]
for _v in range(4):
    HOOKS[f"soundWindowProc_f{_v}"] = dict(
        _SND, addr=0x000288A0, abi="default", ret="uint32", args=["pointer"] * 4, fixture=f"c3fe_wp_f{_v}",
        state=_WP_STATE, vectors=_WP_VECTORS)

# ---------------------------------------------------------------------------------------------------------------------
# E. updateVoiceRate / Channel_startVoice: one key per engine (NULL or its output rate)
# ---------------------------------------------------------------------------------------------------------------------
for _k in ("null", "22050", "44100", "0", "max"):
    HOOKS[f"updateVoiceRate_{_k}"] = dict(
        _SND, addr=0x00032500, abi="thiscall", ret="uint32", args=["pointer"], fixture=f"c3fe_ur_{_k}",
        state=[(f"$c{i}", 0, 0x1400) for i in range(12)] + [(f"$v{i}", 0, 0x100) for i in range(12)]
        + [_LOG, ("$geng", 0, 4)] + ([] if _k == "null" else [("$eng", 0, 0x40)]),
        vectors=[(f"$c{i}",) for i in range(12)])
_CS_IDX = [0, 1, 5, 16, -1, -2, 17, 3, 0x7FFFFFFF, 15, 8, 2]
for _k in ("null", "22050", "44100"):
    HOOKS[f"Channel_startVoice_{_k}"] = dict(
        _SND, addr=0x000348F0, abi="thiscall", ret="uint32", args=["pointer", "int"], fixture=f"c3fe_cs_{_k}",
        state=[(f"$b{i}", 0, 0x1800) for i in range(12)] + [_LOG, ("$geng", 0, 4)] + ([] if _k == "null" else [("$eng", 0, 0x40)]),
        vectors=[(f"$s{i}", _CS_IDX[i]) for i in range(12)] + [("$s0", 4), ("$s3", 16), ("$s6", -1), ("$s9", 0x10)])

# ---------------------------------------------------------------------------------------------------------------------
# F. renderChannel: (channel, buffer, count a1, mode a2, result pointer a3)
# ---------------------------------------------------------------------------------------------------------------------
_RC = [(0, 4, 0), (1, 5, 0x18), (2, 6, 3), (3, 7, 0x19), (4, 8, 0xC), (5, 9, 0xD), (6, 10, 0xC), (7, 3, 0x18),
       (8, 2, 0xB), (9, 1, 0x10), (10, 4, 5), (11, 6, 0x14), (3, 1, 1), (6, 10, 0xB), (7, 3, 0xC), (8, 2, 0x17)]
HOOKS["renderChannel"] = dict(
    _SND, addr=0x00035990, abi="thiscall", ret="uint32", args=["pointer", "pointer", "uint32", "uint32", "pointer"],
    fixture="c3fe_rc",
    state=[(f"$c{i}", 0, 0x1400) for i in range(12)] + [(f"$out{i}", 0, 0x10) for i in range(12)]
    + [("$buf", 0, 0x400), _LOG, _IMPLOG],
    vectors=[(f"$c{c}", "$buf", a1, a2, f"$out{c}") for c, a1, a2 in _RC])

# ---------------------------------------------------------------------------------------------------------------------
# G. renderChannelSingle: (channel, buffer, count a1, mode a2, result pointer a3); one key per g_soundEngine setting
# ---------------------------------------------------------------------------------------------------------------------
_RS = [(0, 0x7D1, 0xC), (1, 4, 0xC), (2, 4, 3), (3, 6, 0xC), (4, 6, 0xD), (5, 5, 0xC), (6, 3, 2), (7, 3, 1), (8, 3, 0),
       (9, 4, 0xC), (10, 4, 0xC), (11, 4, 0x18), (12, 5, 0xC), (13, 5, 0xB), (14, 7, 0xC), (15, 7, 0xC), (16, 7, 0xC),
       (17, 7, 0xC), (18, 2, 0xC), (19, 2, 0), (4, 6, 5)]
for _k in ("post", "bit0", "null"):
    HOOKS[f"renderChannelSingle_{_k}"] = dict(
        _SND, addr=0x00036280, abi="thiscall", ret="uint32", args=["pointer", "pointer", "uint32", "uint32", "pointer"],
        fixture=f"c3fe_rs_{_k}",
        state=[(f"$c{i}", 0, 0x1400) for i in range(20)] + [(f"$x{i}", 0, 0x20) for i in range(20)]
        + [(f"$out{i}", 0, 0x10) for i in range(20)] + [("$buf", 0, 0x800), _LOG, _IMPLOG, ("$geng", 0, 4)]
        + ([] if _k == "null" else [("$eng", 0, 0x200)]),
        vectors=[(f"$c{c}", "$buf", a1, a2, f"$out{c}") for c, a1, a2 in _RS])

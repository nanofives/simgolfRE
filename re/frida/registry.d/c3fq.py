# C3 batch c3fq (round 35): sound.dll leftovers (log/c3/c3fq_candidates.tsv). Module-aware hooks; addr is the RVA, VA =
# 0x10000000 + RVA. Reimplementations in shim/src/re/c3fq.cpp, fixtures in re/frida/js/fixtures.d/c3fq.js, Purposes and per-jcc
# branch tables in log/c3/c3fq_purpose.md, notes in log/c3/c3fq_notes.txt. The structures below mirror the row tables of the
# fixture file (same names, same order).

_SND = dict(module="sound.dll")
_LOG = ("$log", 0, 0x820)
_FREELOG = ("$freelog", 0, 0x404)
_IMPLOG = ("$implog", 0, 0x400)


def _arena(size):
    return ("$arena", 0, 0x10 + size)


HOOKS = {}

# ---- A. ChannelTable_releaseObject / ChannelTable_loadRange ---------------------------------------------------------
_RO_M = [[0, 0, 0, 0], [1, 0, 0, 0], [1, 1, 0, 0], [1, 1, 1, 0], [1, 1, 1, 1], [0, 1, 1, 0], [1, 0, 1, 1], [1, 1, 0, 1],
         [1, 1, 1, 1], [1, 0, 0, 0], [1, 1, 0, 0], [1, 1, 1, 0]]
_RO_X = {8: (0, 1), 11: (2, 3)}
_ro_state = [_LOG]
for _i, _m in enumerate(_RO_M):
    _ro_state += [(f"$t{_i}", 0, 0x220), (f"$o{_i}", 0, 0x1460)]
    _ro_state += [(f"$o{_i}_{_j}", 0, 0x200) for _j in range(4) if _m[_j]]
    _ro_state += [(f"$x{_i}_{_x}", 0, 0x200) for _x in _RO_X.get(_i, ())]
HOOKS["ChannelTable_releaseObject"] = dict(_SND, addr=0x0001B0D0, abi="thiscall", ret="uint32", args=["pointer", "pointer"],
                                           fixture="c3fq_relobj", state=_ro_state,
                                           vectors=[(f"$t{i}", f"$o{i}") for i in range(12)])

# old objects per row (id, member pattern), as LR in the fixture
_LR_OLD = [{}, {}, {"A": [1, 1, 0, 0]},
           {"A": [1, 0, 0, 0], "B": [0, 0, 0, 0], "C": [1, 1, 1, 1], "D": [1, 0, 0, 0]},
           {"A": [1, 0, 0, 0]}, {"E": [1, 1, 0, 0], "F": [0, 0, 0, 0]}, {"G": [1, 0, 0, 0]}, {"H": [1, 1, 1, 1]},
           {"I": [1, 0, 0, 0], "J": [0, 0, 0, 0], "K": [1, 1, 0, 0]}, {}, {"A": [0, 0, 0, 0], "B": [1, 0, 0, 0]},
           {"L": [1, 1, 1, 0], "M": [1, 0, 0, 0]}]
_lr_state = [_LOG, _arena(0x6800), _FREELOG]
for _i, _old in enumerate(_LR_OLD):
    _lr_state += [(f"$t{_i}", 0, 0x220), (f"$r{_i}", 0, 0x40)]
    for _id, _m in _old.items():
        _lr_state.append((f"$o{_i}{_id}", 0, 0x1460))
        _lr_state += [(f"$o{_i}{_id}_{_j}", 0, 0x200) for _j in range(4) if _m[_j]]
HOOKS["ChannelTable_loadRange"] = dict(_SND, addr=0x0001AEF0, abi="thiscall", ret="uint32", args=["pointer", "pointer"],
                                       fixture="c3fq_loadrange", state=_lr_state,
                                       vectors=[(f"$t{i}", f"$r{i}") for i in range(12)])

# ---- B. rewindChannel -------------------------------------------------------------------------------------------------
HOOKS["rewindChannel"] = dict(_SND, addr=0x000339C0, abi="thiscall", ret="uint32", args=["pointer"],
                              fixture="c3fq_rewind",
                              state=[_LOG, _IMPLOG, ("$mmblock", 0, 0x1800)]
                              + [(f"$c{i}", 0, 0x1380) for i in range(12)] + [(f"$s{i}", 0, 0x40) for i in range(12)],
                              vectors=[(f"$c{i}",) for i in range(12)])

# ---- C. Wave_Device::appendBuffer / freeBufferList -------------------------------------------------------------------
_AP_Q = [0, 1, 0, 1, 1, 2, 1, 1, 0, 2, 1, 1]
_ap_state = [_IMPLOG, _arena(0x4000), _FREELOG, ("$g4a08", 0, 4), ("$g4a20", 0, 4)]
for _i, _q in enumerate(_AP_Q):
    _ap_state.append((f"$d{_i}", 0, 0x400))
    _ap_state += [(f"$ap{_i}_{_j}", 0, 0x10) for _j in range(_q)]       # prior queue nodes
    _ap_state += [(f"$pv{_i}_{_j}", 0, 0x110) for _j in range(_q)]      # prior voices (back-patched)
HOOKS["Wave_Device::appendBuffer"] = dict(_SND, addr=0x00006850, abi="thiscall", ret="uint32",
                                          args=["pointer", "pointer"], fixture="c3fq_append", state=_ap_state,
                                          vectors=[(f"$d{i}", f"$b{i}") for i in range(12)])

# freeBufferList: payloads per row (number of nodes, and which hold a voice)
_FR_PAYS = [["v"], [], [], ["v", None, "v"], ["v"], ["v"], ["v"], ["v", "v"], ["v"], [], ["v"], ["v"]]
_FR_FMT = [0, 0, 0xc, 0, 0, 0xc, 0xc, 5, 0, 0, 0xd, 1]
_fr_state = [_LOG, _IMPLOG, _arena(0x400), _FREELOG, ("$g4a08", 0, 4), ("$dst", 0, 0x800), ("$outN", 0, 4)]
for _i, _pays in enumerate(_FR_PAYS):
    _fr_state.append((f"$d{_i}", 0, 0x400))
    _fr_state += [(f"$fq{_i}_{_j}", 0, 0x10) for _j in range(len(_pays))]
    _fr_state += [(f"$fv{_i * 10 + _j}", 0, 0x220) for _j, _p in enumerate(_pays) if _p == "v"]
HOOKS["Wave_Device::freeBufferList"] = dict(_SND, addr=0x000064E0, abi="thiscall", ret="uint32",
                                            args=["pointer", "pointer", "uint32", "uint32", "pointer"],
                                            fixture="c3fq_free", state=_fr_state,
                                            vectors=[(f"$d{i}", "$dst", 0x40, _FR_FMT[i], "$outN") for i in range(12)])

# ---- D. create_sound -------------------------------------------------------------------------------------------------
# [name, type]: type 0 uses the strstr path; the 15 rows mirror CS_V in the fixture
_CS = [("menu.wav", 0), ("song.mid", 0), ("clip.aif", 0), ("bank.rmi", 0), ("noext", 0), (None, 0),
       ("x", 1), ("x", 2), ("x", 3), ("x", 4), ("x", 5), ("x", 6), ("x", 7), ("x", 8), ("x", 0x80)]
_cs_state = [_LOG, _arena(0x8000), _FREELOG, ("$g4a1c", 0, 4)] + [(f"$out{i}", 0, 4) for i in range(len(_CS))]
for _k, _fx in (("", "c3fq_create"), ("_gate0", "c3fq_create_gate0")):
    HOOKS[f"create_sound{_k}"] = dict(_SND, addr=0x000093E0, abi="default", ret="uint32",
                                      args=["pointer", "pointer", "uint32"], fixture=_fx, state=_cs_state,
                                      vectors=[(f"$out{i}", f"$nm{i}", _CS[i][1]) for i in range(len(_CS))])

# C3 batch c3fk (round 34): sound.dll, the callees round 33 named (re/names/sound_15.tsv) plus 0x1002e3f0, then the rows c3fd
# left. Module-aware hooks; addr is the RVA, VA = 0x10000000 + RVA. Reimplementations in shim/src/re/c3fk.cpp, fixtures in
# re/frida/js/fixtures.d/c3fk.js, Purposes and per-jcc branch tables in log/c3/c3fk_purpose.md, notes in log/c3/c3fk_notes.txt.
# The structures below mirror the row tables of the fixture file (same names, same order).

_SND = dict(module="sound.dll")
_LOG = ("$log", 0, 0x820)
_FREELOG = ("$freelog", 0, 0x404)
_G49A8 = ("$g49a8", 0, 4)


def _arena(size):
    return ("$arena", 0, 0x10 + size)


def _objs(n, size, p="o"):
    return [(f"${p}{i}", 0, size) for i in range(n)]


HOOKS = {}

# ---- A. the callees round 33 named, and 0x1002e3f0 -----------------------------------------------------------------
HOOKS["getField204"] = dict(_SND, addr=0x0002D430, abi="thiscall", ret="uint32", args=["pointer"], fixture="c3fk_f204",
                            state=_objs(12, 0x210), vectors=[(f"$o{i}",) for i in range(12)])
HOOKS["ChanRecord_init"] = dict(_SND, addr=0x0001CCF0, abi="thiscall", ret="pointer", args=["pointer"],
                                fixture="c3fk_blocks40", state=_objs(12, 0x40), vectors=[(f"$o{i}",) for i in range(12)])
HOOKS["NodeB000_initBase"] = dict(_SND, addr=0x0002E3F0, abi="thiscall", ret="pointer", args=["pointer"],
                                  fixture="c3fk_blocks40", state=_objs(12, 0x40), vectors=[(f"$o{i}",) for i in range(12)])
HOOKS["NodeB000_ctor"] = dict(_SND, addr=0x0000B000, abi="thiscall", ret="pointer", args=["pointer"],
                              fixture="c3fk_blocks40", state=_objs(12, 0x40), vectors=[(f"$o{i}",) for i in range(12)])
HOOKS["ChanRecord_freeBuffers"] = dict(_SND, addr=0x0001CD30, abi="thiscall", ret="void", args=["pointer"],
                                       fixture="c3fk_crfree", state=_objs(12, 0x40) + [_arena(0x100), _FREELOG, _G49A8],
                                       vectors=[(f"$o{i}",) for i in range(12)])
HOOKS["StrPtr_assign"] = dict(_SND, addr=0x0001B160, abi="thiscall", ret="uint32", args=["pointer", "pointer"],
                              fixture="c3fk_strassign", state=_objs(12, 0x10) + [_arena(0x200), _FREELOG, _G49A8],
                              vectors=[(f"$o{i}", f"$s{i}") for i in range(12)])

_CS4_OBJS = [[], [0], [0], [0], [0, 1], [0, 1], [0, 1], [0, 1, 2], [0, 1], [0], [0, 1, 2, 3], [0, 1]]
_CS4_N = [{}, {0: 0}, {0: 1}, {0: 2}, {0: 3, 1: 0}, {0: 5, 1: 2}, {0: -1, 1: 0}, {0: 2, 1: 1, 2: 3}, {0: 4, 1: 2},
          {0: 2}, {0: 1, 1: 2, 2: 1, 3: 3}, {0: 2, 1: 1}]
_cs4_state = []
for _i, _ids in enumerate(_CS4_OBJS):
    _cs4_state.append((f"$t{_i}", 0, 0x220))
    for _id in _ids:
        _cs4_state.append((f"$g{_i}_{_id}", 0, 0x1500))
        for _j in range(max(0, min(8, _CS4_N[_i].get(_id, 0) - 1))):
            _cs4_state.append((f"$m{_i}_{_id}_{_j}", 0, 0x200))
_CS4_V = [0, 1, 0x3e8, 0xFFFFFFFF, 0x80000000, 7, 0x7F, 0x10000, 2, 0x12345678, 5, 0xFFFF]
HOOKS["ChannelTable_callSlot4"] = dict(_SND, addr=0x0001AE50, abi="thiscall", ret="uint32", args=["pointer", "uint32"],
                                       fixture="c3fk_callslot4", state=_cs4_state + [_LOG, _G49A8],
                                       vectors=[(f"$t{i}", _CS4_V[i]) for i in range(12)])

_IMPLOG = ("$implog", 0, 0x400)

# ---- B. sequencer channel records, destructors, track builders --------------------------------------------------------
_SU = [(0x80, 0, 0), (0xFFFFFFFF, 0, 1), (5, 1, 1), (5, 0, 0), (0x7F, 1, 1), (3, 0, 1), (3, 1, 0x100), (1, 0, 0),
       (2, 1, 1), (0, 1, 0), (4, 0, 1), (6, 1, 1)]          # (ch, old, flag) as in the fixture's SU table
_GD = [(f"${g}", 0, 4) for g in ("gdh", "gdt", "gdc", "gdn")]


def _setup_state(listed):
    s = _objs(12, 0xA00, "q") + _objs(12, 0x40, "old") + _GD + [("$gdef", 0, 4), _arena(0x200), _FREELOG, _LOG, _G49A8]
    for k in listed:
        s.append((f"$n{k}", 0, 0x10))
        if k != 4:
            s.append((f"$p{k}", 0, 0x220))
    return s


for _k, _fx, _listed, _rows in (("", "c3fk_setup", range(6), range(12)),
                                ("_empty", "c3fk_setup_empty", (), range(12)),
                                ("_oom", "c3fk_setup_oom", range(6), (0, 1, 2, 3, 4, 5, 6, 8)),
                                ("_tail", "c3fk_setup_tail", range(4), range(12))):
    HOOKS[f"Seq_setupChannel{_k}"] = dict(_SND, addr=0x00020FE0, abi="thiscall", ret="uint32",
                                          args=["pointer", "pointer", "uint32"], fixture=_fx, state=_setup_state(_listed),
                                          vectors=[(f"$q{i}", f"$d{i}", _SU[i][2]) for i in _rows])


def _update_state(payloads):
    s = _objs(12, 0xA00, "q") + _objs(12, 0x40, "r") + _GD + [("$z", 0, 0x220), _arena(0x100), _FREELOG, _LOG, _G49A8]
    for k in payloads:
        s.append((f"$p{k}", 0, 0x220))
    for k in range(5 if payloads else 0):
        s.append((f"$n{k}", 0, 0x10))
    return s


for _k, _fx, _pl in (("", "c3fk_update", (0, 1, 2, 3, 4)), ("_hole", "c3fk_update_hole", (0, 1, 3, 4)),
                     ("_nolist", "c3fk_update_nolist", ())):
    HOOKS[f"Seq_updateChannel{_k}"] = dict(_SND, addr=0x00021FA0, abi="thiscall", ret="uint32", args=["pointer", "pointer"],
                                           fixture=_fx, state=_update_state(_pl),
                                           vectors=[(f"$q{i}", f"$d{i}") for i in range(12)])

_D6A8 = [([], [], []), ([0], [1], [1]), ([0x7E], [1, 1], []), ([3, 4, 0x40], [1, 0, 1], [1, 1]), ([], [0, 1, 1, 1], [1, 0, 1]),
         ([1, 2, 3, 4, 5, 6], [1, 1, 1, 1], [0]), ([0x7F], [], [1]), ([0, 0x7E], [1, 1, 0], [1, 1, 1]), ([10], [0], [0, 1]),
         ([0x20, 0x21], [1, 0, 0, 1], []), ([], [1], [1, 1, 1, 1]), ([0x7D], [1, 1, 1], [1, 0])]
_d6a8_state = _objs(12, 0xA00)
for _i, (_s, _a, _b) in enumerate(_D6A8):
    _d6a8_state += [(f"$c{_i}_{j}", 0, 0x40) for j in range(len(_s))]
    _d6a8_state += [(f"$na{_i}_{k}", 0, 0x10) for k in range(len(_a))]
    _d6a8_state += [(f"$nb{_i}_{k}", 0, 0x10) for k in range(len(_b))]
HOOKS["Sound5b6a8_dtor"] = dict(_SND, addr=0x0001FF50, abi="thiscall", ret="void", args=["pointer"], fixture="c3fk_d6a8",
                                state=_d6a8_state + [_arena(0x100), _FREELOG, _LOG, _G49A8],
                                vectors=[(f"$o{i}",) for i in range(12)])

_D450 = [[], [1], [1, 0, 0], [1], [1, 1], [1, 0, 1], [0, 1], [1, 1, 1], [1, 1, 1, 1], [1, 0, 1, 1, 1], [1, 0, 1, 0], [1]]   # 1 = a buffer
_D450_INLIST = 11
_d450_state = _objs(12, 0x100)
for _i, _r in enumerate(_D450):
    _d450_state += [(f"$b{_i}_{k}", 0, 0x60) for k, _h in enumerate(_r) if _h]
    _d450_state += [(f"$nd{_i}_{k}", 0, 0x10) for k in range(len(_r))]
_d450_state += [(f"$mn{k}", 0, 0x10) for k in range(_D450_INLIST + 1)]
_d450_state += [(f"${g}", 0, 4) for g in ("gmh", "gmt", "gmc", "gmn", "gopen")]
HOOKS["Sound5c450_dtor"] = dict(_SND, addr=0x00030180, abi="thiscall", ret="void", args=["pointer"], fixture="c3fk_d450",
                                state=_d450_state + [_IMPLOG, _arena(0x100), _FREELOG, _LOG, _G49A8],
                                vectors=[(f"$o{i}",) for i in range(12)])

# Seq_buildTrack9 / 14 rows as in the fixture's BT table: (A keys, B keys or None, getAt entries, bad)
_BT = [([], None, [], 0), ([7], None, [1], 0), ([10], None, [1, 2, 0xE], 0), ([10], None, [0, 0xE, 3], 0),
       ([10, 20, 30], None, [1, 1, 1, 2], 0), ([10, 20, 30], None, [0xE, 1, 0, 0xE, 2, 3], 0), ([10, 20], None, [], 0),
       ([10], None, [], 0), ([10, 20], None, [0], 0), ([10, None, 30], None, [0, 2], 0), ([10, 20, 30], [10], [3, 0xE], 0),
       ([], None, [2], 0), ([10, 20], None, [1], 1)]
_bt_state = _objs(13, 0x500, "q")
for _i, (_a, _b, _g, _bad) in enumerate(_BT):
    _bt_state += [(f"$tk{_i}_{j}", 0, 0x20) for j, k in enumerate(_a) if k is not None]
    _bt_state += [(f"$na{_i}_{k}", 0, 0x10) for k in range(len(_a))]
    _bt_state += [(f"$nb{_i}_{k}", 0, 0x10) for k in range(len(_b if _b is not None else _a))]
    _bt_state += [(f"$tg{_i}_{k}", 0, 0x460) for k, g in enumerate(_g) if g not in (0, 0xE)]
    if _bad:
        _bt_state.append((f"$stray{_i}", 0, 0x10))
_bt_state += [_arena(0x200), _FREELOG, _LOG, _G49A8]
HOOKS["Seq_buildTrack9"] = dict(_SND, addr=0x000229F0, abi="thiscall", ret="uint32", args=["pointer", "pointer"],
                                fixture="c3fk_build9", state=_bt_state,
                                vectors=[(f"$q{i}", f"$d{i}") for i in range(13)] + [("$q4", 0)])
HOOKS["Seq_buildTrack14"] = dict(_SND, addr=0x00022D00, abi="thiscall", ret="uint32", args=["pointer", "pointer"],
                                 fixture="c3fk_build14", state=_bt_state, vectors=[(f"$q{i}", f"$d{i}") for i in range(13)])

# Sound_lookupOrCreate rows as in the fixture's LK table: (record count, sub-descriptors with an object)
_LK = [(0, 0), (1, 0), (2, 0), (2, 0), (1, 0), (2, 2), (1, 1), (2, 0), (5, 0), (2, 1), (1, 0), (1, 0)]


def _lookup_state(payloads, nodes):
    s = _objs(12, 0xA00, "q") + _GD + [("$z", 0, 0x220), _arena(0x600), _FREELOG, _LOG, _G49A8]
    for i, (nr, ns) in enumerate(_LK):
        s += [(f"$r{i}_{j}", 0, 0x40) for j in range(nr)]
        s += [(f"$so{i}_{j}", 0, 0x1500) for j in range(ns)]
    s += [(f"$p{k}", 0, 0x220) for k in payloads] + [(f"$n{k}", 0, 0x10) for k in range(nodes)]
    return s


for _k, _fx, _pl, _nn in (("", "c3fk_lookup", (0, 1, 2, 3, 4), 5), ("_hole", "c3fk_lookup_hole", (0, 1, 3, 4), 5),
                          ("_nolist", "c3fk_lookup_nolist", (), 0)):
    HOOKS[f"Sound_lookupOrCreate{_k}"] = dict(_SND, addr=0x00022320, abi="thiscall", ret="uint32", args=["pointer", "pointer"],
                                              fixture=_fx, state=_lookup_state(_pl, _nn),
                                              vectors=[(f"$q{i}", f"$d{i}") for i in range(12)])

# ---- C. Seq_buildPlayOrder, Midi_Device::handleMetaEvent --------------------------------------------------------------
HOOKS["Seq_buildPlayOrder"] = dict(_SND, addr=0x00023D00, abi="thiscall", ret="uint32", args=["pointer", "pointer"],
                                   fixture="c3fk_playorder",
                                   state=_objs(12, 0x260, "q") + _objs(12, 0x40, "b") + [("$gseed", 0, 4), _IMPLOG, _G49A8],
                                   vectors=[(f"$q{i}", f"$b{i}") for i in range(12)])

# handleMetaEvent rows as in the fixture's ME table: the payload list of each row (1 = a payload, 0 = a NULL payload)
_ME_LIST = [[]] * 19 + [[1], [1, 1, 1], [1, 1], [1, 0, 1], [0], [1]]
_me_state = _objs(len(_ME_LIST), 0x800) + _objs(len(_ME_LIST), 0x460, "t") + _objs(len(_ME_LIST), 0x40, "t2_") + \
    _objs(len(_ME_LIST), 0x40, "r")
for _i, _l in enumerate(_ME_LIST):
    _me_state += [(f"$nd{_i}_{k}", 0, 0x10) for k in range(len(_l))]
    _me_state += [(f"$pl{_i}_{k}", 0, 0x40) for k, _x in enumerate(_l) if _x]
HOOKS["Midi_Device::handleMetaEvent"] = dict(_SND, addr=0x00021BF0, abi="thiscall", ret="uint32",
                                             args=["pointer", "pointer", "pointer"], fixture="c3fk_meta",
                                             state=_me_state + [_LOG, _G49A8],
                                             vectors=[(f"$o{i}", f"$e{i}", f"$t{i}") for i in range(len(_ME_LIST))])

# ---- D. DirectSoundDevice bodies ----------------------------------------------------------------------------------------
# enumerate rows as in the fixture's DE table: (flags, entry count)
_DE = [(0, 0), (0, 0), (0, 0), (0, 0), (0x20, 0), (0x60, 0), (0x40, 3), (0xFF, 5), (0x21, 1), (0, 10), (0x60, 2), (0x9F, 0x10)]
_de_state = _objs(12, 0x200, "d") + [(f"$en{i}_{k}", 0, 0x20) for i, (_f, n) in enumerate(_DE) for k in range(n)]
for _k, _fx in (("_i0", "c3fk_dsenum_i0"), ("_i2", "c3fk_dsenum_i2"), ("_i9", "c3fk_dsenum_i9")):
    HOOKS[f"DirectSoundDevice::enumerate{_k}"] = dict(_SND, addr=0x000114E0, abi="thiscall", ret="uint32",
                                                     args=["pointer", "uint32", "pointer"], fixture=_fx,
                                                     state=_de_state + [("$gidx", 0, 4), _LOG, _G49A8],
                                                     vectors=[(f"$d{i}", _DE[i][0], f"$c{i}") for i in range(12)])

# stopAll rows as in the fixture's DSS table: (list +0x168 length)
_DSS_L = [0, 0, 0, 0, 0, 2, 3, 2, 4, 1, 0, 1]
_dss_common = _objs(12, 0x280, "d") + _objs(12, 0x200, "it") + _objs(12, 0x200, "x") + \
    [(f"$nl{i}_{k}", 0, 0x10) for i, n in enumerate(_DSS_L) for k in range(n)] + \
    [("$ghead", 0, 4), ("$gtail", 0, 4), _arena(0x100), _FREELOG, _LOG, _G49A8]
for _k, _fx, _ch in (("_l0", "c3fk_dsstop_l0", []), ("_l3", "c3fk_dsstop_l3", [0, 1, 0]), ("_l2c", "c3fk_dsstop_l2c", [1, 1])):
    _st = _dss_common + [(f"$b{j}", 0, 0x200) for j in range(len(_ch))] + [(f"$ch{j}", 0, 0x200) for j, c in enumerate(_ch) if c]
    if _k == "_l3":
        _st = _st + [("$stray", 0, 0x200)]
    HOOKS[f"DirectSoundDevice::stopAll{_k}"] = dict(_SND, addr=0x00011A80, abi="thiscall", ret="uint32", args=["pointer"],
                                                   fixture=_fx, state=_st, vectors=[(f"$d{i}",) for i in range(12)])

# close rows as in the fixture's DSC table: (entries, list +0x178 length, list +0x168 length)
_DSC = [(0, 0, 0), (1, 0, 0), (3, 0, 0), (2, 0, 0), (0, 2, 1), (1, 2, 0), (4, 3, 2), (0, 0, 3), (2, 1, 4), (1, 3, 0), (5, 0, 0),
        (0, 1, 1)]
_dsc_state = _objs(12, 0x200, "d")
for _i, (_e, _a, _b) in enumerate(_DSC):
    _dsc_state += [(f"$ce{_i}_{k}", 0, 0x20) for k in range(_e)] + [(f"$na{_i}_{k}", 0, 0x10) for k in range(_a)] + \
        [(f"$nb{_i}_{k}", 0, 0x10) for k in range(_b)]
HOOKS["DirectSoundDevice::close"] = dict(_SND, addr=0x00011290, abi="thiscall", ret="uint32", args=["pointer"],
                                         fixture="c3fk_dsclose",
                                         state=_dsc_state + [_IMPLOG, _arena(0x100), _FREELOG, _LOG, _G49A8],
                                         vectors=[(f"$d{i}",) for i in range(12)])

# duplicateBuffer rows as in the fixture's DD table: (list +0x168 length)
_DD_L = [0, 0, 0, 0, 0, 2, 1, 0, 0, 0, 0, 0]
_dd_state = _objs(12, 0x200, "d") + _objs(12, 0x200, "ds") + _objs(12, 0x200, "s") + _objs(12, 0x200, "c") + \
    _objs(12, 0x10, "o") + [(f"$dl{i}_{k}", 0, 0x10) for i, n in enumerate(_DD_L) for k in range(n)] + \
    [("$ghead", 0, 4), ("$gtail", 0, 4), _arena(0x200), _FREELOG, _LOG, _G49A8]
for _k, _fx, _nb, _rows in (("", "c3fk_dsdup", 2, range(12)), ("_empty", "c3fk_dsdup_empty", 0, range(12)),
                            ("_oom", "c3fk_dsdup_oom", 2, [i for i in range(12) if i != 5])):
    HOOKS[f"DirectSoundDevice::duplicateBuffer{_k}"] = dict(
        _SND, addr=0x00012260, abi="thiscall", ret="uint32", args=["pointer", "pointer", "pointer", "pointer"], fixture=_fx,
        state=_dd_state + [(f"$b{j}", 0, 0x200) for j in range(_nb)],
        vectors=[(f"$d{i}", f"$c{i}", f"$s{i}", f"$o{i}") for i in _rows])

# freeBufferById rows as in the fixture's DF table: (node count)
_DF_N = [1, 0, 2, 2, 2, 1, 2, 3, 3, 1, 2, 3]
_df_state = _objs(12, 0x200, "d") + _objs(12, 0x200, "chb") + _objs(12, 0x200, "nb") + _objs(12, 0x10, "o") + \
    [(f"$fb{i}_{k}", 0, 0x200) for i, n in enumerate(_DF_N) for k in range(n)] + \
    [(f"$fn{i}_{k}", 0, 0x10) for i, n in enumerate(_DF_N) for k in range(n)] + \
    [("$stray", 0, 0x200), ("$ghead", 0, 4), ("$gtail", 0, 4), _arena(0x100), _FREELOG, _LOG, _G49A8]
for _k, _fx in (("", "c3fk_dsfree"), ("_nolist", "c3fk_dsfree_nolist")):
    HOOKS[f"DirectSoundDevice::freeBufferById{_k}"] = dict(_SND, addr=0x00012690, abi="thiscall", ret="uint32",
                                                          args=["pointer", "pointer", "pointer"], fixture=_fx, state=_df_state,
                                                          vectors=[(f"$d{i}", f"$ch{i}", f"$o{i}") for i in range(12)])

# createBuffer / createBufferFmt rows as in the fixture's DCR table: (flags, buffers in the +0x168 list)
_DCR = [(0, 0), (0, 0), (0, 0), (4, 0), (4, 0), (4, 2), (4, 2), (4, 1), (4, 2), (0xFF, 0), (4, 0), (7, 0)]
_dcr_state = _objs(12, 0x200, "d") + _objs(12, 0x10, "o") + \
    [(f"$cb{i}_{k}", 0, 0x200) for i, (_f, n) in enumerate(_DCR) for k in range(n)] + \
    [(f"$cn{i}_{k}", 0, 0x10) for i, (_f, n) in enumerate(_DCR) for k in range(n)] + \
    [("$ghead", 0, 4), ("$gtail", 0, 4), _arena(0x200), _FREELOG, _LOG, _G49A8]
for _k, _fx, _nb in (("", "c3fk_dscreate", 2), ("_empty", "c3fk_dscreate_empty", 0)):
    _st = _dcr_state + [(f"$b{j}", 0, 0x200) for j in range(_nb)]
    HOOKS[f"DirectSoundDevice::createBuffer{_k}"] = dict(
        _SND, addr=0x00012D10, abi="thiscall", ret="uint32", args=["pointer", "pointer", "uint32", "uint32"], fixture=_fx,
        state=_st, vectors=[(f"$d{i}", f"$o{i}", _DCR[i][0], 0x1000 * (i + 1)) for i in range(12)])
    HOOKS[f"DirectSoundDevice::createBufferFmt{_k}"] = dict(
        _SND, addr=0x00012FE0, abi="thiscall", ret="uint32", args=["pointer", "pointer", "pointer", "uint32", "uint32"],
        fixture=_fx, state=_st, vectors=[(f"$d{i}", f"$o{i}", f"$f{i}", _DCR[i][0], 0x1000 * (i + 1)) for i in range(12)])

# create rows as in the fixture's DV table: (list +0x178 length)
_DV_L = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3]
_dv_state = _objs(12, 0x200, "d") + _objs(12, 0x200, "itf") + _objs(12, 0x20, "ent") + \
    [(f"$vn{i}_{k}", 0, 0x10) for i, n in enumerate(_DV_L) for k in range(n)] + \
    [("$ghwnd", 0, 4), _IMPLOG, _arena(0x100), _FREELOG, _LOG, _G49A8]
HOOKS["DirectSoundDevice::create"] = dict(_SND, addr=0x00011C60, abi="thiscall", ret="uint32",
                                          args=["pointer", "pointer", "uint32"], fixture="c3fk_dscreate_dev",
                                          state=_dv_state, vectors=[(f"$d{i}", f"$h{i}", (i + 1) % 3) for i in range(12)])

HOOKS["Channel_ctorB"] = dict(_SND, addr=0x00031BE0, abi="thiscall", ret="pointer", args=["pointer"], fixture="c3fk_chctorb",
                              state=_objs(12, 0x1400) + [_LOG, _G49A8], vectors=[(f"$o{i}",) for i in range(12)])

# debugging only (log/c3/alt/c3fk_ab.py with --out log/c3/alt/c3fk_dbg): C3FK_ONLY=<key>:<i>,<j> keeps those vectors of one key
import os as _os
if _os.environ.get("C3FK_ONLY"):
    _key, _idx = _os.environ["C3FK_ONLY"].rsplit(":", 1)
    HOOKS[_key] = dict(HOOKS[_key], vectors=[HOOKS[_key]["vectors"][int(x)] for x in _idx.split(",")])

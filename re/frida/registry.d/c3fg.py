# C3 batch c3fg (round 34): the sound.dll MMIO rows over the mmio stand-in re/frida/js/mmiostandin.js.
# Module-aware hooks; addr is the RVA, VA = 0x10000000 + RVA. Reimplementations in shim/src/re/c3fg.cpp, fixtures in
# re/frida/js/fixtures.d/c3fg.js (files from log/c3/c3fg_gen.py), Purposes and per-jcc branch tables in log/c3/c3fg_purpose.md,
# notes in log/c3/c3fg_notes.txt.

_SND = dict(module="sound.dll")
_MMSIZE = 0x8000                       # c3fg.js MMSIZE: the mmio stand-in's block, all of it state
_MM = [("$mmblock", 0, _MMSIZE), ("$implog", 0, 0x400)]
_FREELOG = ("$freelog", 0, 0x404)
_G49A8 = ("$g49a8", 0, 4)


def _arena(size):
    return ("$arena", 0, 0x10 + size)


_LIST = [("$glist", 0, 0x10), ("$gmmio", 0, 4)]

HOOKS = {}

# ---- MidiReader_init: one key per g_soundTimer setting, twelve objects each --------------------------------------------
for _v in ("null", "p1", "p3", "p7", "p16", "p33", "p999", "p1000", "p5000"):
    HOOKS[f"MidiReader_init_{_v}"] = dict(_SND, addr=0x0001D380, abi="thiscall", ret="pointer", args=["pointer"],
                                          fixture=f"c3fg_mri_{_v}", state=[("$objs", 0, 12 * 0x200)],
                                          vectors=[(f"$o{i}",) for i in range(12)])

# ---- Sound_resetBuffers -------------------------------------------------------------------------------------------------
for _v in ("", "_single", "_empty"):
    HOOKS[f"Sound_resetBuffers{_v}"] = dict(_SND, addr=0x00032020, abi="thiscall", ret="uint32", args=["pointer"],
                                            fixture=f"c3fg_rb{_v}",
                                            state=[("$objs", 0, 12 * 0x80), ("$nodes", 0, 0x60)] + _LIST + _MM
                                            + [_arena(0x100), _FREELOG],
                                            vectors=[(f"$o{i}",) for i in range(12)])

# ---- beginDecodeStreamA / B: c3fg.js DEC_ROWS (device 0 NULL, 8 = $dev8, 7 = $dev0) ------------------------------------
_DEC_DEV = [0, 8, 8, 8, 8, 8, 8, 7, 8, 8, 8, 8]
_DEC_VEC = [(f"$o{i}", {0: 0, 8: "$dev8", 7: "$dev0"}[d]) for i, d in enumerate(_DEC_DEV)]
for _n, _a in (("beginDecodeStreamA", 0x00032130), ("beginDecodeStreamB", 0x00032290)):
    HOOKS[_n] = dict(_SND, addr=_a, abi="thiscall", ret="uint32", args=["pointer", "pointer"], fixture="c3fg_dec",
                     state=[("$objs", 0, 12 * 0x1400), ("$log", 0, 0x200), ("$devs", 0, 0x200)] + _MM,
                     vectors=_DEC_VEC)

# ---- processWaveChunks, loadMultiChannel, Wave_Device::readFmtChunk ----------------------------------------------------
HOOKS["processWaveChunks"] = dict(_SND, addr=0x00005770, abi="thiscall", ret="uint32", args=["pointer"], fixture="c3fg_pwc",
                                  state=[("$objs", 0, 12 * 0xB00), ("$lists", 0, 12 * 0x400), ("$log", 0, 0x200)],
                                  vectors=[(f"$o{i}",) for i in range(12)])
HOOKS["loadMultiChannel"] = dict(_SND, addr=0x000399A0, abi="thiscall", ret="uint32", args=["pointer", "pointer"],
                                 fixture="c3fg_lmc",
                                 state=[("$objs", 0, 12 * 0x1450), ("$log", 0, 0x200), _arena(0x5000), _FREELOG],
                                 vectors=[(f"$o{i}", "$name") for i in range(12)])
for _k, _fx in (("", "c3fg_rfc"), ("_t3", "c3fg_rfc_t3")):
    HOOKS[f"Wave_Device::readFmtChunk{_k}"] = dict(_SND, addr=0x00005440, abi="thiscall", ret="uint32", args=["pointer"],
                                                   fixture=_fx,
                                                   state=[("$objs", 0, 12 * 0xB00), ("$nodes", 0, 0x100), ("$log", 0, 0x200),
                                                          ("$seed", 0, 0x40)] + _MM + [_arena(0x800), _FREELOG],
                                                   vectors=[(f"$o{i}",) for i in range(12)])

# ---- parseRiffWave ------------------------------------------------------------------------------------------------------
_MMBIG = 0x61000                       # c3fg.js BIGMM.size
_PRW_COMMON = [("$log", 0, 0x200), ("$gwave", 0, 4), ("$gds", 0, 4), ("$nodes", 0, 0x40)] + _LIST + [_FREELOG]
for _k, _n in (("", 12), ("_slot", 12), ("_dec", 12)):
    HOOKS[f"parseRiffWave{_k}"] = dict(_SND, addr=0x000326A0, abi="thiscall", ret="uint32", args=["pointer", "pointer"],
                                       fixture=f"c3fg_prw{_k}",
                                       state=[("$objs", 0, _n * 0x1400)] + _PRW_COMMON + _MM + [_arena(0x2000)],
                                       vectors=[(f"$o{i}", f"$n{i}") for i in range(_n)])
for _k, _n in (("_big", 6), ("_bignodev", 4), ("_bigoom", 2)):
    HOOKS[f"parseRiffWave{_k}"] = dict(_SND, addr=0x000326A0, abi="thiscall", ret="uint32", args=["pointer", "pointer"],
                                       fixture=f"c3fg_prw{_k}",
                                       state=[("$objs", 0, _n * 0x1400)] + _PRW_COMMON
                                       + [("$mmblock", 0, _MMBIG), ("$implog", 0, 0x400), _arena(0x21000)],
                                       vectors=[(f"$o{i}", f"$n{i}") for i in range(_n)])

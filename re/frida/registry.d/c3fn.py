# C3 batch c3fn (round 35): the sound.dll MMIO rows c3fg left, over the mmio stand-in re/frida/js/mmiostandin.js.
# Module-aware hooks; addr is the RVA, VA = 0x10000000 + RVA. Reimplementations in shim/src/re/c3fn.cpp, fixtures in
# re/frida/js/fixtures.d/c3fn.js, Purposes and per-jcc branch tables in log/c3/c3fn_purpose.md, notes in log/c3/c3fn_notes.txt.

_SND = dict(module="sound.dll")
_MMSIZE = 0x8000                       # c3fn.js MMSIZE: the mmio stand-in's block, all of it state
_MM = [("$mmblock", 0, _MMSIZE), ("$implog", 0, 0x400)]
_FREELOG = ("$freelog", 0, 0x404)
_GMMIO = ("$gmmio", 0, 4)
_GLIST = [("$glist", 0, 0x10), ("$onodes", 0, 0x100)]


def _arena(size):
    return ("$arena", 0, 0x10 + size)


HOOKS = {}

# ---- loadWaveResource: c3fn.js LWR rows; the module tables 0x100b5084..0x100b542c are state ($gtab) ----------------------
for _k in ("", "_nobuf"):
    HOOKS[f"loadWaveResource{_k}"] = dict(_SND, addr=0x000316C0, abi="thiscall", ret="uint32", args=["pointer", "pointer"],
                                          fixture=f"c3fn_lwr{_k}",
                                          state=[("$objs", 0, 12 * 0x100), ("$hdrs", 0, 12 * 0x20), ("$rnodes", 0, 0x100),
                                                 ("$ctx", 0, 0x40), ("$log", 0, 0x400), ("$gtab", 0, 0x3A8), _GMMIO]
                                          + _MM + [_arena(0x200), _FREELOG],
                                          vectors=[(f"$o{i}", f"$h{i}") for i in range(12)])

# ---- flushMmioBuffer: c3fn.js FMB rows -----------------------------------------------------------------------------------
HOOKS["flushMmioBuffer"] = dict(_SND, addr=0x00037920, abi="thiscall", ret="uint32", args=["pointer"], fixture="c3fn_fmb",
                                state=[("$objs", 0, 12 * 0x1400), ("$decs", 0, 12 * 0x50)] + _MM + [_arena(0x100), _FREELOG],
                                vectors=[(f"$o{i}",) for i in range(12)])

# ---- closeMmioSound: c3fn.js CMS rows ------------------------------------------------------------------------------------
HOOKS["closeMmioSound"] = dict(_SND, addr=0x00035CA0, abi="thiscall", ret="uint32", args=["pointer"], fixture="c3fn_cms",
                               state=[("$objs", 0, 12 * 0x1400), ("$ss", 0, 12 * 0x100), ("$vs", 0, 12 * 0x120),
                                      ("$bufs", 0, 12 * 0x20), _GMMIO] + _GLIST + _MM + [_arena(0x100), _FREELOG],
                               vectors=[(f"$o{i}",) for i in range(12)])

# ---- openMmioSound: c3fn.js OMS rows (`_nodeoom`: every list empty, the 0xc-byte node allocation fails) ------------------
for _k in ("", "_nobuf", "_nodeoom"):
    HOOKS[f"openMmioSound{_k}"] = dict(_SND, addr=0x00030840, abi="thiscall", ret="uint32", args=["pointer", "pointer"],
                                       fixture=f"c3fn_oms{_k}",
                                       state=[("$objs", 0, 12 * 0x200), ("$rs", 0, 12 * 0x30), ("$pk", 0, 12 * 0x10),
                                              ("$ctx", 0, 0x40), ("$pv", 0, 12 * 0x110), ("$onodes", 0, 12 * 0x30),
                                              ("$log", 0, 0x400), _GMMIO, ("$geng", 0, 4)]
                                       + _MM + [_arena(0x400), _FREELOG],
                                       vectors=[(f"$o{i}", f"$r{i}") for i in range(12)])

# writeWaveChunk 0x7DF0 held for Path C: its reimplementation (shim/src/re/c3fn.cpp, #if 0) and fixture (c3fn_wwc) are
# oplog-verified per vector (orig == reimpl op logs) but the diff_hook full run crashes the game; no registry entry so the
# verifier does not A/B it.

# ---- fillDecodeRing: c3fn.js FDR rows ----------------------------------------------------------------------------------
for _k in ("a", "b"):
    HOOKS[f"fillDecodeRing_{_k}"] = dict(_SND, addr=0x00033300, abi="thiscall", ret="uint32", args=["pointer"],
                                         fixture=f"c3fn_fdr_{_k}",
                                         state=[("$objs", 0, 6 * 0x1400), ("$rings", 0, 6 * 0x100), ("$rbufs", 0, 6 * 0x200),
                                                _GMMIO] + _MM,
                                         vectors=[(f"$o{i}",) for i in range(6)])

# ---- Midi_Device::loadFile / loadFromMemory: c3fn.js MIDI_ROWS / MIDI_MEM_ROWS -----------------------------------------
for _n, _a, _fx in (("Midi_Device::loadFile", 0x0001D810, "c3fn_mlf"), ("Midi_Device::loadFromMemory", 0x0001DCE0, "c3fn_mlm"),
                    ("Midi_Device::loadFile_nulldata", 0x0001D810, "c3fn_mlf_nulldata"),
                    ("Midi_Device::loadFromMemory_nulldata", 0x0001DCE0, "c3fn_mlm_nulldata"),
                    ("Midi_Device::loadFile_nodeoom", 0x0001D810, "c3fn_mlf_nodeoom"),
                    ("Midi_Device::loadFromMemory_nodeoom", 0x0001DCE0, "c3fn_mlm_nodeoom")):
    HOOKS[_n] = dict(_SND, addr=_a, abi="thiscall", ret="uint32", args=["pointer", "pointer"], fixture=_fx,
                     state=[("$objs", 0, 12 * 0x500), ("$names", 0, 12 * 0x20), ("$rnodes", 0, 12 * 0x40),
                            ("$membufs", 0, 8 * 0x100), ("$log", 0, 0x400), _GMMIO] + _MM + [_arena(0x3000), _FREELOG],
                     vectors=[(f"$o{i}", f"$n{i}") for i in range(12)])

# ---- streamWaveSamples: c3fn.js SWS rows (15) ----------------------------------------------------------------------------
_SWS_N = 15
for _k, _n in (("", _SWS_N), ("_nodev", 4)):
    HOOKS[f"streamWaveSamples{_k}"] = dict(_SND, addr=0x00030BA0, abi="thiscall", ret="uint32",
                                           args=["pointer", "pointer", "uint32", "uint32", "pointer"],
                                           fixture=f"c3fn_sws{_k}",
                                           state=[("$objs", 0, _SWS_N * 0x100), ("$st", 0, _SWS_N * 0x400),
                                                  ("$snodes", 0, _SWS_N * 0x40), ("$outs", 0, _SWS_N * 0x140),
                                                  ("$wrs", 0, _SWS_N * 4), ("$log", 0, 0x400),
                                                  ("$gdevp", 0, 4), _GMMIO] + _GLIST + _MM + [_arena(0x100), _FREELOG],
                                           vectors=[(f"$o{i}", f"$out{i}", f"$n{i}", f"$sz{i}", f"$wr{i}") for i in range(_n)])

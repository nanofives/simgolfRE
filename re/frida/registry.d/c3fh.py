"""C3 batch c3fh (round 34): the x87-faithful pilot (shim/src/re/c3fh.cpp, re/frida/js/fixtures.d/c3fh.js).

Every vector is (out, posp, n, record, mode) with all five taken from the fixture ($n<i> / $m<i> are numbers), so the
record each row builds and the count / mode it is called with live in one place (MONO / STEREO in c3fh.js).
Each mixer has two keys over the same vectors: `<name>` compares eax (samples written) and `<name>_st0` declares a
double return, so the harness reads st(0) after each arm: the top of the register stack the body leaves behind.
"""

_SND = dict(module="sound.dll", abi="default")
_MIX_ARGS = ["pointer", "pointer", "uint32", "pointer", "int"]
_MONO_N = 23      # len(MONO) in c3fh.js
_STEREO_N = 16    # len(STEREO) in c3fh.js
_OUTSZ = 0x1000


def _state(count):
    st = []
    for i in range(count):
        st += [(f"$o{i}", 0, 0x1400), (f"$s{i}", 0, 0x40), (f"$p{i}", 0, 4), (f"$out{i}", 0, _OUTSZ)]
    return st


def _mix(addr, fixture, count, ret):
    return dict(_SND, addr=addr, ret=ret, args=_MIX_ARGS, fixture=fixture, state=_state(count),
                vectors=[(f"$out{i}", f"$p{i}", f"$n{i}", f"$o{i}", f"$m{i}") for i in range(count)])


HOOKS = {}
for _name, _addr, _fx, _cnt in (
        ("Voice_mixSetMono16", 0x00019B20, "c3fh_mono16", _MONO_N),
        ("Voice_mixAddMono16", 0x00019770, "c3fh_mono16", _MONO_N),
        ("Voice_mixSet8bit", 0x00019EC0, "c3fh_mono8", _MONO_N),
        ("Voice_mixAdd8bit", 0x0001A2C0, "c3fh_mono8", _MONO_N),
        ("Voice_mixSetStereo16", 0x0001A6C0, "c3fh_stereo16", _STEREO_N),
        ("Voice_mixAddStereo16", 0x0001A910, "c3fh_stereo16", _STEREO_N)):
    HOOKS[_name] = _mix(_addr, _fx, _cnt, "uint32")
    HOOKS[_name + "_st0"] = _mix(_addr, _fx, _cnt, "double")

# ---- B2. the stream resamplers over one fixture (STREAM in c3fh.js, 20 rows); entered at control word 0x0f7f
_STREAM_N = 21
for _name, _addr in (("streamRenderBlock", 0x000184A0), ("streamRenderTail", 0x000188D0)):
    HOOKS[_name] = _mix(_addr, "c3fh_stream", _STREAM_N, "uint32")
    HOOKS[_name + "_st0"] = _mix(_addr, "c3fh_stream", _STREAM_N, "double")

# ---- C. the caller-word helpers (section C of c3fh.cpp / c3fh.js). Each key name ends in the control word the fixture
# sets on the A/B thread before the vectors: _w27f (0x027f, 53-bit to nearest) or _wf7f (0x0f7f, a mixer's word).
_TH = dict(_SND, abi="thiscall")
_TEMPO_N, _DSVOL_N, _CHAN_N, _FADE_N = 12, 15, 10, 22      # len(TEMPO) / len(DSVOL) / len(CH_F58) / len(FADE) in c3fh.js


def _objs(n, size, p="o"):
    return [(f"${p}{i}", 0, size) for i in range(n)]


for _w in ("w27f", "wf7f"):
    HOOKS[f"Seq_setTempo_{_w}"] = dict(_TH, addr=0x00021EE0, ret="uint32", args=["pointer", "uint32"],
                                       fixture=f"c3fh_tempo_{_w}", state=_objs(_TEMPO_N, 0x800),
                                       vectors=[(f"$o{i}", f"$t{i}") for i in range(_TEMPO_N)])
    HOOKS[f"DsBuffer::setVolume_{_w}"] = dict(_TH, addr=0x0000F1C0, ret="uint32", args=["pointer", "uint32"],
                                              fixture=f"c3fh_dsvol_{_w}",
                                              state=_objs(_DSVOL_N, 0x100) + _objs(_DSVOL_N, 0x200, "m") + _objs(_DSVOL_N, 0x30, "c"),
                                              vectors=[(f"$o{i}", f"$v{i}") for i in range(_DSVOL_N)])
    HOOKS[f"Channel_stepVolumeFade_{_w}"] = dict(_TH, addr=0x00035FC0, ret="uint32", args=["pointer"],
                                                 fixture=f"c3fh_fade_{_w}",
                                                 state=_objs(_FADE_N, 0x1400) + _objs(_FADE_N, 0x100, "d") + _objs(_FADE_N, 0x30, "c"),
                                                 vectors=[(f"$o{i}",) for i in range(_FADE_N)])
    HOOKS[f"Sound5b6a8_ctor_{_w}"] = dict(_TH, addr=0x0001F4A0, ret="pointer", args=["pointer"], fixture=f"c3fh_snd5_{_w}",
                                          state=_objs(3, 0xA00), vectors=[(f"$o{i}",) for i in range(3)])
HOOKS["Sound5b6a8_ctor_N_w27f"] = dict(HOOKS["Sound5b6a8_ctor_w27f"], fixture="c3fh_snd5N_w27f")

_CHAN_STATE = _objs(_CHAN_N, 0x1500) + [("$eng", 0, 0x10), ("$tmr", 0, 0x10)]
for _v in ("chanN_w27f", "chanT0_w27f", "chanA_w27f", "chanA_wf7f", "chanB_w27f", "chanB_wf7f", "chanC_wf7f", "chanD_w27f"):
    _sfx = _v[4:]
    for _name, _addr, _arg in (("Channel_recomputeVoiceGains", 0x00035460, "r"), ("Channel_setReverseRate", 0x000355D0, "r"),
                               ("Channel_setRate", 0x000351C0, "ir")):
        HOOKS[f"{_name}_{_sfx}"] = dict(_TH, addr=_addr, ret="uint32", args=["pointer", "int" if _arg == "ir" else "uint32"],
                                        fixture=f"c3fh_{_v}", state=_CHAN_STATE,
                                        vectors=[(f"$o{i}", f"${_arg}{i}") for i in range(_CHAN_N)])

_PNG_N = 27                     # len(PNGROWS) in c3fh.js
for _w in ("w27f", "wf7f"):
    HOOKS[f"png_init_read_transformations_{_w}"] = dict(
        module="jgld.dll", addr=0x000702A0, abi="default", ret="void", args=["pointer"], fixture=f"c3fh_png_{_w}",
        state=_objs(_PNG_N, 0x220, "g") + _objs(_PNG_N, 0x300, "pal") + _objs(_PNG_N, 0x100, "tr")
        + [("$arena", 0, 0x4010), ("$freelog", 0, 0x404)],
        vectors=[(f"$g{i}",) for i in range(_PNG_N)])

_RS_LEN = [4, 6, 8, 3, 2, -4, 5, 10]
for _v, _lens in (("f555_w27f", _RS_LEN), ("f555_wf7f", _RS_LEN), ("fodd_w27f", _RS_LEN), ("fodd_wf7f", _RS_LEN),
                  ("f565_w27f", [4, 6, 3, 1]), ("fail0_w27f", [4, 6, 3]), ("fail1_w27f", [4, 6, 0x7FFFFFFF])):
    _rows, _max = (0x10000, 6) if _v.startswith("f565") else (0x8000, 10)
    HOOKS[f"allocResampleTable_{_v}"] = dict(module="golf_clean.exe", addr=0x00481870, abi="default", ret="int", args=["int"],
                                             fixture=f"c3fh_resample_{_v}",
                                             state=[("$arena", 0, 0x10 + _rows * _max * 2 + _max * 8 + 0x100),
                                                    ("$g10", 0, 4), ("$g14", 0, 4), ("$gb0", 0, 4)],
                                             vectors=[(n,) for n in _lens])

for _r in ("N", "r999", "r1000", "r1001", "r3", "r0", "r22050"):
    for _w in ("w27f", "wf7f"):
        HOOKS[f"Voice_resetDefaults_{_r}_{_w}"] = dict(_TH, addr=0x00037C40, ret="pointer", args=["pointer"],
                                                       fixture=f"c3fh_voice_{_r}_{_w}",
                                                       state=_objs(3, 0x120) + [("$eng", 0, 0x10)],
                                                       vectors=[(f"$o{i}",) for i in range(3)])

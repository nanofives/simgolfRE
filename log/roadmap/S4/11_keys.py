"""S4.11 keys over log/roadmap/S4/11_fixtures.js (fixtures F.s411_*): registry-shaped specs for the functions of
port/src/sim/audio_crt.cpp. Imported by 11_replay.py, which adds them to sound_replay's CUSTOM table.

FUNCTIONS: the step's functions (rva, hooks.csv / re/names/sound_19.tsv name); rows of re/roadmap/S4_functions.tsv with step `-`
become S4.11's, the others get a synthetic row (11_merge.md section 4 lists them for the lead's table).
"""

FUNCTIONS = [
    (0x2e3c0, "GlblHeader::init"), (0x05f40, "Sound::getField1b8"), (0x2cf50, "MixBuffer::deletingDtor"),
    (0x20480, "Sound::queueMidiCommand3"), (0x244a0, "ChannelDesc_dtor"), (0x29b80, "VoiceBase_initListSlot"),
    (0x2c270, "Ch5bb90_set3DBuffer"), (0x4249a, "operator_delete"), (0x424b0, "_strrchr"), (0x424d7, "operator_new"),
    (0x42577, "_sprintf"), (0x425c9, "__onexit"), (0x42647, "_atexit"), (0x42690, "strstr"), (0x4280e, "ehVectorDestructorIterator"),
    (0x4288e, "__ArrayUnwind"), (0x42902, "ehVectorConstructorIterator"), (0x42acd, "_malloc"), (0x42adf, "__nh_malloc"),
    (0x42b0b, "heap_alloc"), (0x42c07, "free"), (0x45226, "_lockexit"), (0x4522f, "_unlockexit"), (0x45252, "crtRealloc"),
    (0x45581, "msize"), (0x45b80, "__callnewh"), (0x47710, "__lock"), (0x47771, "_unlock"),
]

M = "sound.dll"

# registry keys (S4.9b's / S4.10's C3 batches c3fo, c3fz) whose vectors make the constructor / destructor stub RAISE: the original
# unwinds through the iterator's SEH frame and RaiseException, which the fixture VM does not dispatch and the port does not model
# (U-1540). The same functions' non-raising keys (_snd_none, ArrayUnwind, the 11_keys.py iterator keys) are the judged ones.
_SEH = ("an unmodelled import KERNEL32!RaiseException: the stub throws and the original unwinds through the SEH frame; "
        "U-1540 (log/roadmap/S4/11_uncertain.tsv)")
EXCLUDED = {k: _SEH for k in ("ehVectorConstructorIterator_snd_t0", "ehVectorConstructorIterator_snd_t3", "sound___ArrayUnwind_t0",
                              "sound___ArrayUnwind_t2")}


def K(addr, abi, ret, args, fixture, state, vectors):
    return dict(module=M, addr=addr, abi=abi, ret=ret, args=args, fixture=fixture, state=state, vectors=vectors)


HS = [("$implog", 0, 0x400), ("$arena", 0, 0xc00), ("$cell", 0, 0x10)]       # heapSI's regions (11_fixtures.js)


def blocks(prefix, n, size):
    return [(f"${prefix}{i}", 0, size) for i in range(n)]


KEYS = {}
KEYS["GlblHeader::init@"] = K(0x2e3c0, "thiscall", "pointer", ["pointer"], "s411_glbl", blocks("b", 10, 0x10),
                              [(f"$b{i}",) for i in range(10)])
KEYS["Sound::getField1b8@"] = K(0x5f40, "thiscall", "uint32", ["pointer"], "s411_f1b8", blocks("b", 10, 0x200),
                                [(f"$b{i}",) for i in range(10)])
for _m in ("dev", "full", "nodev"):
    KEYS[f"Sound::queueMidiCommand3@{_m}"] = K(0x20480, "thiscall", "uint32", ["pointer"], f"s411_midi_{_m}",
                                               [("$dev", 0, 0x4060), ("$gmidi", 0, 4)] + blocks("s", 6, 0x100) + HS,
                                               [(f"$s{i}",) for i in range(6)])
_MIX_RUNS = [0, 1, 3, 1, 1, 1, 3, 1]
_MIX_EXTRA = [False, False, True, False, True, False, True, True]
_mix_state = [(f"$o{i}", 0, 0x208) for i in range(8)]
_mix_state += [(f"$r{i}_{k}", 0, 0x10) for i in range(8) for k in range(_MIX_RUNS[i])]
_mix_state += [(f"$x{i}", 0, 0x40) for i in range(8) if _MIX_EXTRA[i]] + HS
KEYS["MixBuffer::deletingDtor@"] = K(0x2cf50, "thiscall", "pointer", ["pointer", "uint32"], "s411_mixdtor", _mix_state,
                                     [(f"$o{i}", f) for i in range(8) for f in ((i & 1), 2 + (i & 1))][:16])
_S = 12
KEYS["_strrchr@"] = K(0x424b0, "default", "pointer", ["pointer", "uint32"], "s411_strings", blocks("s", _S, 0x20),
                      [(f"$s{i}", ch) for i in range(_S) for ch in (0x5c, 0x61, 0)][:30]
                      + [("$s3", 0x15c), ("$s5", 0x2e), ("$s9", 0x64), ("$s8", 0x61)])
_P = 15
KEYS["strstr@"] = K(0x42690, "default", "pointer", ["pointer", "pointer"], "s411_strings", blocks("s", _S, 0x20) + [("$h", 0, 0x40)],
                    [(f"$s{i}", f"$p{j}") for i in (2, 3, 5, 8, 7) for j in range(_P)][:60]
                    + [("$h", "$p3"), ("$h", "$p12"), ("$h", "$p2"), ("$h", "$p1"), ("$h", "$p0")])
_FMT_ARGS = [123, 42, 7, -3 & 0xFFFFFFFF, 99, 99, 0xbeef, 0xbeef, 0xbeef, 0, 0o777, 8, 0xFFFFFFFF, 5, 0, "$str", "$str", "$str",
             "$str", 0x41, 1, -77 & 0xFFFFFFFF, 0x80000000, 0x18000, 0x18000, -1 & 0xFFFFFFFF, 1234, 0xdead, 0, 17, 9, -5 & 0xFFFFFFFF,
             0, 0x7a, 0]
KEYS["_sprintf@"] = K(0x42577, "default", "uint32", ["pointer", "pointer", "uint32"], "s411_sprintf",
                      [("$buf", 0, 0x80 * len(_FMT_ARGS))], [(f"$o{i}", f"$f{i}", v) for i, v in enumerate(_FMT_ARGS)])
_SIZES = [0, 1, 0xf, 0x10, 0x11, 0x100, 0x1001, 0xffffffe1, 0xffffffff, 0x7f]
for _k, _fx in (("0", "s411_heap0"), ("1", "s411_heap1")):
    _st = [("$gmode", 0, 8)] + blocks("b", 4, 0x40) + HS
    KEYS[f"operator_new@m{_k}"] = K(0x424d7, "default", "uint32", ["uint32"], _fx, _st, [(n,) for n in _SIZES])
    KEYS[f"_malloc@m{_k}"] = K(0x42acd, "default", "uint32", ["uint32"], _fx, _st, [(n,) for n in _SIZES])
    KEYS[f"__nh_malloc@m{_k}"] = K(0x42adf, "default", "uint32", ["uint32", "uint32"], _fx, _st,
                                   [(n, f) for n in _SIZES[:6] + [0xffffffe1] for f in (0, 1)])
    KEYS[f"heap_alloc@m{_k}"] = K(0x42b0b, "default", "uint32", ["uint32"], _fx, _st, [(n,) for n in _SIZES[:7]])
for _k, _fx in (("fyes", "s411_heapf_yes"), ("fno", "s411_heapf_no"), ("fmode0", "s411_heapf_mode0")):
    _st = [("$gmode", 0, 8)] + blocks("b", 4, 0x40) + HS
    KEYS[f"operator_new@{_k}"] = K(0x424d7, "default", "uint32", ["uint32"], _fx, _st, [(n,) for n in _SIZES])
    KEYS[f"_malloc@{_k}"] = K(0x42acd, "default", "uint32", ["uint32"], _fx, _st, [(n,) for n in _SIZES])
    KEYS[f"__nh_malloc@{_k}"] = K(0x42adf, "default", "uint32", ["uint32", "uint32"], _fx, _st,
                                  [(n, f) for n in _SIZES[:6] + [0xffffffe1] for f in (0, 1)])
KEYS["__callnewh@none"] = K(0x45b80, "default", "uint32", ["uint32"], "s411_heap0", [("$gmode", 0, 8)], [(n,) for n in range(10)])
KEYS["__callnewh@stub"] = K(0x45b80, "default", "uint32", ["uint32"], "s411_newh", [("$gmode", 0, 8)], [(n,) for n in range(10)])
KEYS["free@"] = K(0x42c07, "default", "void", ["pointer"], "s411_heap0", [("$gmode", 0, 8)] + blocks("b", 4, 0x40) + HS,
                  [(0,), ("$b0",), ("$b1",), ("$b2",), ("$b3",)])
KEYS["operator_delete@"] = K(0x4249a, "default", "void", ["pointer"], "s411_heap0", [("$gmode", 0, 8)] + blocks("b", 4, 0x40) + HS,
                             [(0,), ("$b0",), ("$b1",), ("$b2",), ("$b3",)])
_LOCKS = [0x14, 0x15, 0x16, 0x18, 0x1a, 0x1c, 0x20, 0x22, 0x24, 0x26]
KEYS["__lock@create"] = K(0x47710, "default", "void", ["uint32"], "s411_lock_create", [("$gtab", 0, 0xc0)] + HS, [(n,) for n in _LOCKS])
KEYS["__lock@race"] = K(0x47710, "default", "void", ["uint32"], "s411_lock_race",
                        [("$gtab", 0, 0xc0)] + blocks("cs", 10, 0x18) + HS, [(n,) for n in _LOCKS])
_SBH = [("$slog", 0, 0x800), ("$gtype", 0, 4), ("$glim3", 0, 4), ("$glim2", 0, 4)] + blocks("b", 4, 0x40) + HS
for _t, _sizes in ((3, (0, 1, 0x10, 0x20, 0x3f8, 0x3f9, 0x400)), (2, (0, 1, 0x10, 0x20, 0x30, 0x780, 0x781))):
    KEYS[f"heap_alloc@sbh{_t}"] = K(0x42b0b, "default", "uint32", ["uint32"], f"s411_sbh{_t}", _SBH, [(n,) for n in _sizes])
    KEYS[f"free@sbh{_t}"] = K(0x42c07, "default", "void", ["pointer"], f"s411_sbh{_t}", _SBH,
                              [(0,), ("$b0",), ("$b1",), ("$b2",), ("$b3",)])
KEYS["__lock@enter"] = K(0x47710, "default", "void", ["uint32"], "s411_lock_enter",
                         [("$gtab", 0, 0xc0)] + blocks("cs", 10, 0x18), [(n,) for n in _LOCKS])
KEYS["__lock@pair"] = K(0x47710, "default", "void", ["uint32"], "s411_lock_pair",
                        [("$gtab", 0, 0xc0)] + blocks("cs", 10, 0x18), [(n,) for n in _LOCKS])
KEYS["_unlock@pair"] = K(0x47771, "default", "void", ["uint32"], "s411_lock_pair",
                         [("$gtab", 0, 0xc0)] + blocks("cs", 10, 0x18), [(n,) for n in _LOCKS])
for _m in ("room", "full", "fail"):
    KEYS[f"__onexit@{_m}"] = K(0x425c9, "default", "uint32", ["uint32"], f"s411_onexit_{_m}", [("$gex", 0, 8), ("$tbl", 0, 0x80)] + (HS if _m != "room" else []),
                               [(v,) for v in (0x10001234, 0, 0xffffffff, 0x10009999, 7, 0x10000000, 0x5a5a5a5a, 1, 2, 3)])
    KEYS[f"_atexit@{_m}"] = K(0x42647, "default", "uint32", ["uint32"], f"s411_onexit_{_m}", [("$gex", 0, 8), ("$tbl", 0, 0x80)] + (HS if _m != "room" else []),
                              [(v,) for v in (0x10001234, 0, 0xffffffff, 0x10009999, 7, 0x10000000, 0x5a5a5a5a, 1, 2, 3)])
KEYS["_lockexit@"] = K(0x45226, "default", "void", [], "s411_lockexit_enter", [("$gtab", 0, 0xc0)] + blocks("cs", 2, 0x18), [()])
KEYS["_unlockexit@"] = K(0x4522f, "default", "void", [], "s411_lockexit_pair", [("$gtab", 0, 0xc0)] + blocks("cs", 2, 0x18), [()])
KEYS["msize@"] = K(0x45581, "default", "uint32", ["pointer"], "s411_realloc", [("$gex", 0, 8), ("$tbl", 0, 0x80), ("$b0", 0, 0x40)] + HS,
                   [("$tbl",), ("$b0",)])
KEYS["crtRealloc@"] = K(0x45252, "default", "uint32", ["pointer", "uint32"], "s411_realloc",
                        [("$gex", 0, 8), ("$tbl", 0, 0x80), ("$b0", 0, 0x40)] + HS,
                        [(0, 0x20), (0, 0), ("$b0", 0), ("$tbl", 0x10), ("$tbl", 0x90), ("$tbl", 0x200), ("$tbl", 0x400),
                         ("$tbl", 0xffffffe1), ("$b0", 0x10)])
for _k, _fx in (("yes", "s411_realloc_yes"), ("no", "s411_realloc_no")):
    KEYS[f"crtRealloc@h{_k}"] = K(0x45252, "default", "uint32", ["pointer", "uint32"], _fx,
                                  [("$gmode", 0, 8), ("$tbl", 0, 0x80), ("$b0", 0, 0x40)] + HS,
                                  [("$tbl", 0x10), ("$b0", 0x90), ("$tbl", 0x20), (0, 0x20), ("$b0", 0), ("$tbl", 0x7ff0)])
_IT = [("$log", 0, 0x100), ("$a0", 0, 0x110), ("$a1", 0, 0x4c * 4), ("$a2", 0, 0x18 * 0x10)]
KEYS["ehVectorConstructorIterator@"] = K(0x42902, "stdcall", "void", ["pointer", "uint32", "uint32", "pointer", "pointer"], "s411_iter",
                                         _IT, [("$a0", 0x11, 0x10, "$recf", "$recf"), ("$a1", 0x4c, 4, "$recf", "$recf"),
                                               ("$a0", 4, 0, "$recf", "$recf"), ("$a0", 8, 0xffffffff, "$recf", "$recf"),
                                               ("$a2", 0x18, 0x10, "$ctorReal", "$dtorReal"), ("$a1", 0, 3, "$recf", "$recf")])
KEYS["ehVectorDestructorIterator@"] = K(0x4280e, "stdcall", "void", ["pointer", "uint32", "uint32", "pointer"], "s411_iter", _IT,
                                        [("$a0", 0x11, 0x10, "$recf"), ("$a1", 0x4c, 4, "$recf"), ("$a0", 4, 0, "$recf"),
                                         ("$a0", 8, 0xffffffff, "$recf"), ("$a2", 0x18, 0x10, "$dtorReal"), ("$a1", 0, 3, "$recf")])
KEYS["__ArrayUnwind@"] = K(0x4288e, "stdcall", "void", ["pointer", "uint32", "uint32", "pointer"], "s411_iter", _IT,
                           [("$a0", 0x11, 0x10, "$recf"), ("$a1", 0x4c, 4, "$recf"), ("$a0", 4, 0, "$recf"), ("$a1", 0, 3, "$recf")])
KEYS["ChannelDesc_dtor@"] = K(0x244a0, "thiscall", "uint32", ["pointer"], "s411_records", blocks("b", 10, 0x60),
                              [(f"$b{i}",) for i in range(10)])
KEYS["VoiceBase_initListSlot@"] = K(0x29b80, "thiscall", "pointer", ["pointer"], "s411_records", blocks("b", 10, 0x60),
                                    [(f"$b{i}",) for i in range(10)])
KEYS["Ch5bb90_set3DBuffer@"] = K(0x2c270, "thiscall", "uint32", ["pointer", "uint32"], "s411_set3d",
                                 [("$o0", 0, 0x80), ("$o1", 0, 0x80), ("$o2", 0, 0x80), ("$leaf", 0, 0x80), ("$mid", 0, 0x80)],
                                 [(o, v) for o in ("$o0", "$o1", "$o2") for v in (0, 1, 0xdeadbeef, 0x80000000)])

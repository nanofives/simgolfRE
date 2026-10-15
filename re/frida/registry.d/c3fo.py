"""C3 batch c3fo (round 35): X80 users and CRT rows (shim/src/re/c3fo.cpp, re/frida/js/fixtures.d/c3fo.js).

x87 keys run both arms through the x87 probe (log/c3/c3fo_notes.txt section 1): the fixture builds the probe block $blk and
puts C3foX87Probe in front of both arms for the key; the saved x87 unit is the state region ($blk, 0x40, 0x74).
"""

_PROBE = [("$blk", 0x40, 0x74)]
_FTOL_N = 56                     # len(FTOL) in c3fo.js
_WORDS = ("w27f", "wf7f", "w37f", "wa7f", "w67f")

HOOKS = {}

# ---- A. __ftol, three modules. Vector i = the input record $r<i> (10-byte register value + `empty` byte); the key's
# suffix is the control word the probe loads before each arm (0x027f the game's, 0x0f7f a mixer's, 0x037f fninit's,
# 0x0a7f rounding up, 0x067f rounding down). ret int64 = edx:eax.
for _mod, _addr, _tag in (("golf_clean.exe", 0x004A6030, ""), ("sound.dll", 0x00042550, "snd_"),
                          ("jgld.dll", 0x0007F12C, "jgld_")):
    for _w in _WORDS:
        HOOKS[f"__ftol_{_tag}{_w}"] = dict(module=_mod, addr=_addr, abi="default", ret="int64", args=["pointer"],
                                           fixture=f"c3fo_ftol_{_w}", state=_PROBE,
                                           vectors=[(f"$r{i}",) for i in range(_FTOL_N)])

# ---- B. __strcmpi / __strnicmp (golf_clean.exe). $s<i> = STR[i] in c3fo.js. Three locale settings per function
# (fixtures c3fo_cmp_c / _loc / _lock); state: the reader count 0x0084100c and the stand-in log (_lock, _unlock, _tolower).
_CMP_PAIRS = [(0, 0), (0, 1), (1, 0), (1, 2), (2, 1), (3, 4), (4, 3), (3, 5), (5, 4), (3, 6), (6, 3), (7, 4), (8, 9),
              (9, 10), (10, 8), (11, 12), (12, 11), (13, 14), (14, 13), (15, 16), (16, 15), (17, 18), (18, 17), (19, 20),
              (20, 19), (21, 22), (23, 24), (24, 23), (25, 26), (26, 27), (27, 25), (28, 29), (29, 30), (30, 28), (19, 0),
              (0, 19), (11, 2), (2, 11)]
_NCMP = [(a, b, n) for a, b in _CMP_PAIRS[:20] for n in (1, 3)] + [
    (a, b, n) for a, b, n in ((3, 4, 0), (3, 5, 0), (3, 5, 2), (3, 5, 3), (3, 6, 2), (3, 6, 3), (6, 3, 3), (8, 9, 100),
                              (9, 10, 11), (9, 10, 12), (10, 9, 0xFFFFFFFF), (28, 29, 26), (29, 30, 26), (29, 30, 27),
                              (30, 28, 27), (25, 27, 8), (25, 27, 9), (17, 18, 1), (0, 0, 5), (19, 20, 0x7FFFFFFF))]
_CMP_STATE = [(0x0084100C, 0, 4), ("$fmlog", 0, 0x2000)]
for _sfx in ("c", "loc", "lock"):
    HOOKS[f"__strcmpi_{_sfx}"] = dict(module="golf_clean.exe", addr=0x004AD4B0, abi="default", ret="int",
                                      args=["pointer", "pointer"], fixture=f"c3fo_cmp_{_sfx}", state=_CMP_STATE,
                                      vectors=[(f"$s{a}", f"$s{b}") for a, b in _CMP_PAIRS])
    HOOKS[f"__strnicmp_{_sfx}"] = dict(module="golf_clean.exe", addr=0x004AD580, abi="default", ret="int",
                                       args=["pointer", "pointer", "uint32"], fixture=f"c3fo_cmp_{_sfx}", state=_CMP_STATE,
                                       vectors=[(f"$s{a}", f"$s{b}", n) for a, b, n in _NCMP])

# ---- C. _fseek / _rewind (golf_clean.exe), fixture c3fo_stream: $f<i> = FILE record i (SFILES in c3fo.js, 12 records).
_SFILES_N = 12
_STREAM_STATE = [("$fblk", 0, 0x20 * _SFILES_N), ("$io", 0, 0x24 * 32), (0x004E4D60, 0, 0x24), ("$fmlog", 0, 0x2000)]
HOOKS["_fseek"] = dict(module="golf_clean.exe", addr=0x004A7119, abi="default", ret="int",
                       args=["pointer", "int", "int"], fixture="c3fo_stream", state=_STREAM_STATE,
                       vectors=[(f"$f{i % _SFILES_N}", off, org) for i, (off, org) in enumerate(
                           [(0, 0), (5, 0), (-3, 1), (13, 2), (0x7FFFFFFF, 0), (-0x80000000, 1), (6, 3), (7, -1),
                            (1, 2), (2, 1), (12, 0), (100, 4), (9, 0), (11, 1)])])
for _mod, _addr, _tag in (("golf_clean.exe", 0x004A55D2, ""), ("sound.dll", 0x00042902, "snd_")):
    _EH = dict(module=_mod, addr=_addr, abi="stdcall", ret="void", args=["pointer", "uint32", "int", "pointer", "pointer"],
               state=[("$D", 0, 0x210), ("$arr", 0, 0x2000), ("$blk", 0x30, 4)])
    # (size, count, dtor given); no constructor raises
    HOOKS[f"ehVectorConstructorIterator_{_tag}none"] = dict(_EH, fixture="c3fo_ehvec", vectors=[
        ("$arr", s, n, "$ctor", "$dtor" if d else 0) for s, n, d in (
            (4, 0, 1), (4, 1, 1), (4, 5, 1), (8, 3, 0), (0x2C, 4, 1), (0, 3, 1), (4, -1, 1), (4, -0x80000000, 0),
            (12, 10, 1), (0x10C, 2, 1), (1, 7, 0), (0x40, 40, 1))])
    # the first constructor raises: nothing to destroy
    HOOKS[f"ehVectorConstructorIterator_{_tag}t0"] = dict(_EH, fixture="c3fo_ehvec_t0", vectors=[
        ("$arr", s, n, "$ctor", "$dtor") for s, n in ((4, 1), (8, 3), (0x2C, 2), (4, 0), (4, -2))])
    # the fourth constructor raises: three elements destroyed, last first
    HOOKS[f"ehVectorConstructorIterator_{_tag}t3"] = dict(_EH, fixture="c3fo_ehvec_t3", vectors=[
        ("$arr", s, n, "$ctor", "$dtor") for s, n in ((4, 3), (4, 4), (8, 8), (0x2C, 5), (0, 6), (0x10C, 4), (12, 2))])

HOOKS["_rewind"] = dict(module="golf_clean.exe", addr=0x004A70B5, abi="default", ret="void", args=["pointer"],
                        fixture="c3fo_stream", state=_STREAM_STATE,
                        vectors=[(f"$f{i}",) for i in range(_SFILES_N)])

# ---- E. jgld _fabs (double passed as two dwords, returned in st(0)); the probe forwards both and returns st(0).
_FAB_N = 28
HOOKS["fabs"] = dict(module="jgld.dll", addr=0x00082960, abi="default", ret="double", args=["uint32", "uint32"],
                     fixture="c3fo_fabs", state=_PROBE,
                     vectors=[(f"$lo{i}", f"$hi{i}") for i in range(_FAB_N)])

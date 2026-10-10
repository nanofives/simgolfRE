# C3 batch c3df (round 25, 2026-10-10) of golf_clean.exe: text measurement and aligned text drawing. Reimplementations
# in shim/src/re/c3df.cpp, fixtures in re/frida/js/fixtures.d/c3df.js, Purposes and per-jcc branch coverage in
# log/c3/c3df_purpose.md. Format documented at the top of re/frida/hooks_registry.py.
#
# Hooked callees from other batches (the verifier re-runs these keys with SIMGOLF_HOOKS_OFF so the original arm uses
# original callees): wrapTextToWidth 0x00483980 (c3z), measureTextWidth 0x00483930 (c3ab), scanMarkupText 0x00476d40
# (c3l), skipToken 0x00476d80 / scanToken 0x00476dd0 (c3s), reverseFindChar 0x004935b0 (c3r), Widget_value 0x00477580
# (c3c), Widget_get 0x00477560 (c3ae), drawMarkupRun 0x00477280 (c3o). Inside this batch: wrapText -> measureMarkupRun
# 0x00476ef0, Text_put and the five draw wrappers -> drawMarkupRun (c3o) and drawRichText (not hooked).
#
# Text keys (measureMarkupRun, wrapText) freeze the game's first thread: they write the accumulator 0x00839aa8 that the
# menu's own text layout also uses. The draw keys touch no global; their text-draw callback records into $rec.

_OBJS = ["tm", "tpart", "tplain", "tnofont", "tnofont1", "tdrop", "tlink", "tpend", "tind", "tneg", "tnomeas", "tnocb",
         "tnocbm"]
_CELLS = ["b1", "b6", "b18", "b40", "b90", "b1e5"]
_OSTATE = [(f"${o}", 0, 0x300) for o in _OBJS]
_TXT_STATE = (_OSTATE + [(f"${c}", 0, 4) for c in _CELLS]
              + [("$lines", 0, 0x80), ("$cnt", 0, 4), (0x00839AA8, 0, 4)])
_DRAW_STATE = _OSTATE + [("$rec", 0, 0x1000)]

_ALL = ["s_plain", "s_one", "s_braces", "s_brackets", "s_mixed", "s_double", "s_close", "s_closeb", "s_dollar", "s_link",
        "s_linknogt", "s_dd", "s_ddend", "s_dl", "s_ddbrace", "s_caret", "s_eq", "s_long", "s_spaces", "s_brace2", "s_open1", "s_open2", "s_dl2", "s_open3"]


def _mm(o, s, b, ln=None):
    return (f"${o}", f"${s}", f"${b}", f"${ln or 'len_' + s}")


# measureMarkupRun 0x00476ef0 (thiscall, text, int* width, len). The same list runs under two accumulator seeds
# (0x00839aa8 = 0 and 100: wrapTextToWidth returns different values for a first word wider than the budget).
_MM = []
_MM += [_mm("tm", s, b) for s in _ALL for b in ("b18", "b90", "b1e5")]
_MM += [_mm("tm", s, "b6") for s in ("s_plain", "s_braces", "s_link", "s_dd", "s_caret", "s_one", "s_long")]
_MM += [_mm("tm", s, "b1e5", "cut_" + s) for s in ("s_braces", "s_link", "s_dd", "s_dl", "s_mixed", "s_caret", "s_eq")]
_MM += [_mm("tm", s, "b18", "cut_" + s) for s in ("s_braces", "s_link", "s_dd", "s_mixed", "s_plain")]
_MM += [_mm("tm", s, "b1e5", "over_" + s) for s in ("s_plain", "s_braces", "s_eq")]
_MM += [_mm("tm", "s_plain", "b18", "len_s_empty"), _mm("tm", "s_empty", "b18")]
_MM += [("$tm", "$s_plain", 0, "$len_s_plain")]                       # width == NULL
_MM += [_mm("tpart", s, b) for s in ("s_mixed", "s_braces", "s_link", "s_dl", "s_plain", "s_brackets", "s_double")
        for b in ("b18", "b1e5")]
_MM += [_mm("tplain", s, b) for s in ("s_plain", "s_braces", "s_one", "s_long") for b in ("b1", "b18", "b1e5")]
_MM += [_mm("tdrop", s, b) for s in ("s_caret", "s_plain", "s_ddbrace", "s_dd", "s_link") for b in ("b18", "b1e5")]
_MM += [_mm("tlink", s, b) for s in ("s_eq", "s_caret", "s_plain", "s_link", "s_braces") for b in ("b18", "b1e5")]
_MM += [_mm("tpend", s, b) for s in ("s_plain", "s_braces", "s_caret", "s_dd") for b in ("b18", "b1e5")]
_MM += [_mm("tpart", s, b) for s in ("s_open1", "s_open2", "s_dl2") for b in ("b6", "b18", "b1e5")]
_MM += [_mm("tpart", "s_open3", b) for b in ("b1", "b6", "b18", "b1e5")] + [_mm("tm", "s_open3", b) for b in ("b1", "b6")]
_MM += [_mm("tneg", s, "b18") for s in ("s_mixed", "s_plain")]
_MM += [_mm("tnomeas", s, "b1") for s in ("s_plain", "s_braces")]
# base font 0: the zero-length run never reaches a live font's measurer (wrapTextToWidth returns at remain == 0)
_MM += [("$tnofont", "$s_plain", "$b18", "$len_s_empty"), ("$tnofont1", "$s_plain", "$b18", "$len_s_empty"),
        ("$tnofont1", "$s_empty", "$b18", "$len_s_empty")]

# wrapText 0x00476e20 (thiscall, text, int width, char** lines, int* count)
_WT = [("$tm", 0, 40, "$lines", "$cnt")]
_WT += [("$tm", "$s_long", w, "$lines", "$cnt") for w in (30, 40, 90, 200, 100000)]
_WT += [("$tm", "$s_long", 40, 0, "$cnt"), ("$tm", "$s_long", 40, "$lines", 0), ("$tm", "$s_long", 40, 0, 0)]
_WT += [("$tm", "$s_plain", w, "$lines", "$cnt") for w in (20, 60)]
_WT += [("$tm", f"${s}", 40, "$lines", "$cnt") for s in _ALL if s not in ("s_long", "s_plain")]
_WT += [("$tm", "$s_empty", 40, "$lines", "$cnt")]
_WT += [("$tind", "$s_long", w, "$lines", "$cnt") for w in (60, 90, 200)]
_WT += [("$tind", "$s_one", 25, "$lines", "$cnt")]
_WT += [("$tplain", "$s_long", w, "$lines", "$cnt") for w in (40, 90)]
_WT += [("$tpart", "$s_mixed", 30, "$lines", "$cnt"), ("$tpart", "$s_link", 30, "$lines", "$cnt")]
_WT += [("$tneg", "$s_long", 60, "$lines", "$cnt"), ("$tneg", "$s_long", 60, 0, 0)]
_WT += [("$tdrop", "$s_caret", 25, "$lines", "$cnt"), ("$tpend", "$s_plain", 40, "$lines", "$cnt")]

# Text_put 0x00477250 (thiscall, text)
_TP = [("$tm", 0), ("$tplain", 0)]
_TP += [("$tm", f"${s}") for s in _ALL + ["s_empty"]]
_TP += [("$tpart", f"${s}") for s in ("s_mixed", "s_link", "s_dl", "s_plain")]
_TP += [("$tplain", f"${s}") for s in ("s_plain", "s_braces", "s_empty")]
_TP += [("$tdrop", "$s_caret"), ("$tlink", "$s_eq"), ("$tpend", "$s_plain"), ("$tneg", "$s_mixed"),
        ("$tnomeas", "$s_plain"), ("$tnocbm", "$s_braces")]

# Draw wrappers. Strings without '$', '=' and '^' (those reach malloc / dropdown windows in drawRichText); objects
# with the draw callback; tnocb / tnocbm have neither callback nor +4 (0x004767a0 returns x), tnomeas / the NULL
# text / the guard lengths exercise the prologue.
_DT = ["s_plain", "s_one", "s_braces", "s_brackets", "s_mixed", "s_double", "s_close", "s_closeb", "s_brace2",
       "s_long", "s_spaces", "s_open1", "s_open2"]
_DOBJ = ["tm", "tpart", "tplain", "tneg", "tnocb", "tnocbm"]
_LENS = ["len_", "cut_", "over_"]


def _lens(s):
    return [f"${p}{s}" for p in _LENS]


def _guards5(fn_args):
    """Prologue vectors shared by the five-argument wrappers: (this, text, x, y, w, len)."""
    v = []
    v.append(("$tm", 0, 5, 6, 100, 8))                    # NULL text
    v.append(("$tnofont", "$s_plain", 5, 6, 100, 8))      # base font NULL
    v.append(("$tnomeas", "$s_plain", 5, 6, 100, 8))      # font +4 NULL
    for ln in (-1, 0, 3):
        v.append(("$tm", "$s_plain", 5, 6, 100, ln))
    v.append(("$tm", "$s_empty", 5, 6, 100, 4))
    return v


def _w5(widths=(0, 1, 2, 3, 7, 19, 20, 50, 99, 100, 101, 1000, -7)):
    v = _guards5(None)
    for o in _DOBJ:
        for s in ("s_plain", "s_braces", "s_mixed", "s_double"):
            v.append((f"${o}", f"${s}", 10, 20, 100, f"$len_{s}"))
    for s in _DT:
        for ln in _lens(s):
            v.append(("$tm", f"${s}", -3, 4, 77, ln))
    for w in widths:
        v.append(("$tplain", "$s_one", 12, -9, w, "$len_s_one"))
        v.append(("$tm", "$s_braces", -12, 9, w, "$len_s_braces"))
    v += [("$tm", "$s_plain", 0x7FFFFFF0, 3, 40, 5), ("$tm", "$s_plain", -0x7FFFFFF0, 3, 40, 5)]
    return v


def _w4():
    v = [("$tm", 0, 5, 6, 8), ("$tnofont", "$s_plain", 5, 6, 8), ("$tnomeas", "$s_plain", 5, 6, 8)]
    for ln in (-1, 0, 3):
        v.append(("$tm", "$s_plain", 5, 6, ln))
    v.append(("$tm", "$s_empty", 5, 6, 4))
    for o in _DOBJ:
        for s in ("s_plain", "s_braces", "s_mixed", "s_double"):
            v.append((f"${o}", f"${s}", 40, 20, f"$len_{s}"))
    for s in _DT:
        for ln in _lens(s):
            v.append(("$tm", f"${s}", 33, -4, ln))
    for x in (-50, -1, 0, 1, 2, 3, 100, 101, 0x7FFFFFF0):
        v.append(("$tplain", "$s_one", x, 8, "$len_s_one"))
        v.append(("$tm", "$s_braces", x, 8, "$len_s_braces"))
    return v


_RECT = ["r0", "r1", "r2", "r3", "r4"]


def _wr():
    v = [("$tm", 0, "$r0", 8), ("$tm", "$s_plain", 0, 8), ("$tnofont", "$s_plain", "$r0", 8),
         ("$tnomeas", "$s_plain", "$r0", 8), ("$tm", 0, 0, 8)]
    for ln in (-1, 0, 3):
        v.append(("$tm", "$s_plain", "$r0", ln))
    v.append(("$tm", "$s_empty", "$r0", 4))
    for o in _DOBJ:
        for s in ("s_plain", "s_braces", "s_mixed", "s_double"):
            v.append((f"${o}", f"${s}", "$r0", f"$len_{s}"))
    for r in _RECT:
        v.append(("$tm", "$s_plain", f"${r}", "$len_s_plain"))
        v.append(("$tm", "$s_braces", f"${r}", "$len_s_braces"))
        v.append(("$tneg", "$s_one", f"${r}", "$len_s_one"))
        v.append(("$tplain", "$s_one", f"${r}", "$len_s_one"))
    for s in _DT:
        for ln in _lens(s):
            v.append(("$tm", f"${s}", "$r4", ln))
    return v


_P = ["pointer", "pointer"]
_A5 = ["pointer", "pointer", "int", "int", "int", "int"]
_A4 = ["pointer", "pointer", "int", "int", "int"]
_AR = ["pointer", "pointer", "pointer", "int"]


def _key(addr, args, fixture, state, vectors, ret="int"):
    return dict(module="golf_clean.exe", addr=addr, abi="thiscall", ret=ret, args=args, fixture=fixture, state=state,
                vectors=vectors)


HOOKS = {
    "measureMarkupRun": _key(0x00476EF0, ["pointer", "pointer", "pointer", "int"], "c3df_text_acc0", _TXT_STATE, _MM,
                             ret="pointer"),
    "measureMarkupRun_accN": _key(0x00476EF0, ["pointer", "pointer", "pointer", "int"], "c3df_text_accN", _TXT_STATE, _MM,
                                  ret="pointer"),
    "wrapText": _key(0x00476E20, ["pointer", "pointer", "int", "pointer", "pointer"], "c3df_text_acc0", _TXT_STATE, _WT),
    "Text_put": _key(0x00477250, ["pointer", "pointer"], "c3df_draw", _DRAW_STATE, _TP),
    "drawCentered2": _key(0x00477DA0, _A5, "c3df_draw", _DRAW_STATE, _w5()),
    "drawRight": _key(0x00478080, _A5, "c3df_draw", _DRAW_STATE, _w5()),
    "drawStringLeft": _key(0x00477FC0, _A4, "c3df_draw", _DRAW_STATE, _w4()),
    "drawString": _key(0x00478140, _A4, "c3df_draw", _DRAW_STATE, _w4()),
    "drawRight2": _key(0x004781F0, _AR, "c3df_draw", _DRAW_STATE, _wr()),
}


# ---- seedCourseTable 0x0046f2b0 (cdecl, no arguments): one call per key; each key's fixture seeds the RNG 0x00822d9c,
# sets or clears bit 0x1000000 of 0x0059e7b8 (odd keys set it) and pre-fills the record area with 0xa5 (keys with bit 1)
# or zeros. 24 keys: the records and the advanced seed are the compared state.
_NSEEDS = 24
_SEED_STATE = [(0x00571FD8, 0, 0x320), (0x00822D9C, 0, 4)]
for _i in range(_NSEEDS):
    HOOKS[f"seedCourseTable_{_i:02d}"] = dict(module="golf_clean.exe", addr=0x0046F2B0, abi="default", ret="void",
                                              args=[], fixture=f"c3df_seed_{_i}", state=_SEED_STATE, vectors=[()])

# ---- enqueueSprite 0x00462a30 (cdecl, p1..p8). State: the ten parallel arrays (1024 entries each) and the queue
# globals. Shapes in js/fixtures.d/c3df.js: empty, one node, three nodes (depths 5, 10, 20 in link order 1, 0, 2) under
# six pivot / scale settings, a queue one below the limit, at the limit and past it.
_QARR = [0x00830164, 0x00831164, 0x0082915C, 0x0082C164, 0x0082815C, 0x0082A15C, 0x0082B160, 0x0082415C, 0x008361F4,
         0x00834174]
_Q_STATE = [(a, 0, 0x1000) for a in _QARR] + [(0x00838200, 0, 4), (0x0082C160, 0, 4), (0x00822C8C, 0, 4),
                                               (0x004C2844, 0, 4)]
_P7 = [0, 3, -3, 1, -1, 7, -9, 2]
_P8 = [0, 0x100, 5, 0x1ff]
_DIM = [(3, 4), (-5, 9), (64, -64), (0x7FFFFFFF, 3), (-0x80000000, 5), (17, 0), (0, 33), (1000, 1000)]


def _qvec(depths):
    v = []
    for i, d in enumerate(depths):
        p4, p5 = _DIM[i % len(_DIM)]
        v.append((100 + 7 * i, -50 + 11 * i, d, p4, p5, 0x40 + i, _P7[i % len(_P7)], _P8[(i // 2) % len(_P8)]))
    return v


_D3 = [1, 5, 7, 10, 15, 20, 25, -3, 0x7FFFFFFF, -0x80000000]
_Q3 = _qvec(_D3) + _qvec(_D3[::-1])
_Q3 += [(10, 20, 6, 40, 50, 1, p7, p8) for p7 in _P7 for p8 in (0, 0x100)]


def _qkey(fixture, vectors):
    return dict(module="golf_clean.exe", addr=0x00462A30, abi="default", ret="void", args=["int"] * 8,
                fixture=fixture, state=_Q_STATE, vectors=vectors)


HOOKS["enqueueSprite_empty"] = _qkey("c3df_q_empty", _qvec([5, 0, -1]))
HOOKS["enqueueSprite_one"] = _qkey("c3df_q_one", _qvec([5, 10, 15, 9]))
for _i in range(6):
    HOOKS[f"enqueueSprite_three_{_i}"] = _qkey(f"c3df_q_three_{_i}", _Q3)
HOOKS["enqueueSprite_last"] = _qkey("c3df_q_last", _qvec([-10, -4, 0, 8, 9]))
HOOKS["enqueueSprite_full"] = _qkey("c3df_q_full", _qvec([1, 2]))
HOOKS["enqueueSprite_over"] = _qkey("c3df_q_over", _qvec([1, 2]))


# ---- Image_draw 0x00473e60 (thiscall, surf, x, y, mask) and Surface_drawScaled 0x00473cb0 (thiscall, surf, x, y, nx, ny,
# den, mask): the m_8 == 0 arm only (m_8 != 0 calls loadImage, file I/O). Fixture surfaces s0..s7 (s3 has no handle); the
# display's scale triple differs per key (s1 (1,1,1), s2 (3,5,2), s3 (-7,11,3), s4 (2,3,-4)).
_DSTATE = [("$rec", 0, 0x2000), ("$surfs", 0, 8 * 0x30), ("$disp", 0, 0x40)]
_IMG = [("$s3", "$s0", 5, 6, 0), ("$s0", 0, 5, 6, 0), ("$s0", "$s3", 5, 6, 0)]          # 7 / 0x10 / 7
_IMG += [(f"$s{t}", f"$s{d}", x, y, m)
         for t in (0, 1, 2, 6, 7) for d in (4, 5) for (x, y) in ((10, 20), (-7, 300)) for m in (0, "$s7")]
_IMG += [("$s1", "$s4", 0x7FFFFFF0, -0x7FFFFFF0, "$s6"), ("$s2", "$s5", -1, 1, 0), ("$s0", "$s4", 0, 0, "$s3")]
_DRS = [("$s3", "$s0", 5, 6, 2, 2, 2, 0), ("$s0", 0, 5, 6, 2, 2, 2, 0), ("$s0", "$s3", 5, 6, 2, 2, 2, 0)]
_DRS += [(f"$s{t}", f"$s{d}", x, y, nx, ny, den, m)
         for t in (0, 1, 2, 6, 7) for d in (4, 5) for (x, y) in ((10, 20), (-7, 300)) for (nx, ny, den, m) in
         ((1, 1, 1, 0), (3, 5, 2, "$s7"), (-4, 9, 3, 0), (7, -2, -5, "$s6"), (0x10000, 0x10000, 7, 0))]
_DRS += [("$s1", "$s4", 100, 200, 3, 3, 1, 0), ("$s0", "$s4", 1, 2, 1, 2, 0x7FFFFFFF, "$s3")]
for _k in ("s1", "s2", "s3", "s4"):
    HOOKS[f"Image_draw_{_k}"] = dict(module="golf_clean.exe", addr=0x00473E60, abi="thiscall", ret="int",
                                     args=["pointer", "pointer", "int", "int", "pointer"], fixture=f"c3df_disp_{_k}",
                                     state=_DSTATE, vectors=_IMG)
for _k in ("s1", "s3"):
    HOOKS[f"Surface_drawScaled_{_k}"] = dict(module="golf_clean.exe", addr=0x00473CB0, abi="thiscall", ret="int",
                                             args=["pointer", "pointer", "int", "int", "int", "int", "int", "pointer"],
                                             fixture=f"c3df_disp_{_k}", state=_DSTATE, vectors=_DRS)

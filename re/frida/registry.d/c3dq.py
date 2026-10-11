# C3 batch c3dq (round 27, 2026-10-11) of golf_clean.exe: drawing rows earlier batches dropped. Reimplementations in
# shim/src/re/c3dq.cpp, fixtures in re/frida/js/fixtures.d/c3dq.js (every key freezes the game's first thread),
# Purposes and per-jcc branch tables in log/c3/c3dq_purpose.md.
#
# Hooked callees inside this batch (the keys are re-run with them off): enterModal483d30 -> modalPush483d40,
# modalFocusClear483d60.
# Callees that run ORIGINAL in both arms (not hooked here): serviceMovie 0x0049c8e0, gfxFlushA 0x00497b00, gfxFlushB
# 0x00497b20 (inlined by the tail-calling bodies), netServiceControl 0x0049ab40, Surface_free 0x00473ae0 (c3cz),
# Surface::releaseBacking 0x004838b0.

_REC = [("$rec", 0, 0x2000)]
_MODAL = _REC + [(0x0083AB40, 0, 8)]
_GLOB = _REC + [(0x00840710, 0, 0x28)]
_OBJS = _REC + [("$objs", 0, 0x400)]


def _f(addr, fixture, ret="int", state=_REC):
    return dict(module="golf_clean.exe", addr=addr, abi="default", ret=ret, args=[], fixture=fixture, state=state,
                vectors=[()])


def _trio(addr, ret="int", state=_REC, both=False):
    """none / b8 / b0 variants of the net globals [0x008400b8] / [0x008400b0]; `both` adds the netServiceControl arm."""
    d = {"": _f(addr, "c3dq_scr_none", ret, state), "_b8": _f(addr, "c3dq_scr_b8", ret, state),
         "_b0": _f(addr, "c3dq_scr_b0", ret, state)}
    if both:
        d["_both"] = _f(addr, "c3dq_scr_both", "void", state)
    return d


HOOKS = {}


def _add(name, d):
    for suffix, spec in d.items():
        HOOKS[name + suffix] = spec


_add("showScreen", _trio(0x00483BB0))
_add("hideScreen", _trio(0x00483C10))
_add("redrawAll", _trio(0x00483C70))
_add("frameTick483cd0", _trio(0x00483CD0))
_add("leaveModal483d80", _trio(0x00483D80))
_add("modalPush483d40", _trio(0x00483D40, both=True))
_add("modalFocusClear483d60", _trio(0x00483D60, state=_MODAL, both=True))
_add("enterModal483d30", _trio(0x00483D30, state=_MODAL, both=True))

for _i in range(10):
    HOOKS[f"screenTick47d010_{_i}"] = _f(0x0047D010, f"c3dq_tick_{_i}")
HOOKS["screenTick47d010_nodisp"] = _f(0x0047D010, "c3dq_scr_nodisp", "void")

HOOKS["freeGfxGlobal_840710_h"] = _f(0x0049D280, "c3dq_free_h", "void", _GLOB)
HOOKS["freeGfxGlobal_840710_h2"] = _f(0x0049D280, "c3dq_free_h2", "void", _GLOB)
HOOKS["freeGfxGlobal_840710_nohandle"] = _f(0x0049D280, "c3dq_free_nohandle", "void", _GLOB)

HOOKS["Surface::ctorVtbl483b90"] = dict(
    module="golf_clean.exe", addr=0x00483B90, abi="thiscall", ret="void", args=["pointer"], fixture="c3dq_objs",
    state=_OBJS, vectors=[(f"$o{k}",) for k in range(5)])


# ---- text drawing ---------------------------------------------------------------------------------------------
# Callees that run ORIGINAL in both arms: setQuad 0x00476310, drawCentered2 0x00477da0, drawText 0x00477c30, drawRichText
# 0x004775b0 (ended by the fixture's draw callback), drawStringCentered 0x00477fc0, drawStringRightAt 0x00478140,
# Widget_value 0x00477580, Widget_get 0x00477560, Text_put 0x00477250, scaleX 0x00404970. Hooked in other batches (c3df
# and earlier): drawCentered2, drawStringLeft, drawString, Text_put, drawMarkupRun, measureMarkupRun, scaleX.
_TOBJ = {"tm": ("$tm", 0, 0x300), "th0": ("$th0", 0, 0x300), "tnoh": ("$tnoh", 0, 0x300),
         "tnofont": ("$tnofont", 0, 0x300), "tplain": ("$tplain", 0, 0x300), "tneg": ("$tneg", 0, 0x300),
         "tnomeas": ("$tnomeas", 0, 0x300), "tind": ("$tind", 0, 0x300), "tind2": ("$tind2", 0, 0x300)}
_TXT_ALL = _REC + list(_TOBJ.values())

_S = ["$s_plain", "$s_one", "$s_long", "$s_empty", 0]
_XYC = [(10, 20, 0x1234), (-5, 300, -1), (100, 0, 7)]
_GV = [(s, x, y, c) for s in _S for (x, y, c) in _XYC]
_XYS = [(10, 10, 1), (30, 87, 2), (40, 88, 3), (50, 200, 4), (60, -50, 5), (1000, -12, 6)]
_SV = [(s, x, y, c) for s in _S for (x, y, c) in _XYS]


def _g(addr, fixture, tgt, vectors, ret="int"):
    return dict(module="golf_clean.exe", addr=addr, abi="default", ret=ret, args=["pointer", "int", "int", "int"],
                fixture=fixture, state=_REC + [_TOBJ[tgt]], vectors=vectors)


for _name, _addr in [("drawCenteredText", 0x00404B70), ("drawCenteredText2", 0x00404BC0),
                     ("drawText4049d0", 0x004049D0)]:
    HOOKS[_name] = _g(_addr, "c3dq_tg_h100", "tm", _GV)
    HOOKS[_name + "_nofont"] = _g(_addr, "c3dq_tg_nofont", "tnofont", _GV)
HOOKS["drawTextScaled"] = _g(0x00404A20, "c3dq_tg_h100", "tm", _SV)
HOOKS["drawTextScaled_s1000"] = _g(0x00404A20, "c3dq_tg_h100b", "tm", _SV)
HOOKS["drawTextScaled_h0"] = _g(0x00404A20, "c3dq_tg_h0", "th0", _SV)
HOOKS["drawTextScaled_noh"] = _g(0x00404A20, "c3dq_tg_noh", "tnoh", _SV)
HOOKS["drawTextSurface"] = _g(0x00404AD0, "c3dq_tg_h100", "tm", _SV)
HOOKS["drawTextSurface_h0"] = _g(0x00404AD0, "c3dq_tg_h0", "th0", _SV)
HOOKS["drawTextSurface_noh"] = _g(0x00404AD0, "c3dq_tg_noh", "tnoh", _SV)

# font-swapping wrappers: (this, font, text, x, y[, w], len)
_THIS = ["$tm", "$tnofont", "$tplain", "$tneg"]
_FONT = [0, "$f0", "$f1", "$fnomeas", "$fneg"]
_TXT = ["$s_plain", "$s_empty", 0, "$s_one"]
_WD = [(t, f, s, x, y, n) for t in _THIS for f in _FONT for s, x, y, n in
       [(_TXT[0], 10, 20, 19), (_TXT[1], 3, 4, 0), (_TXT[2], 5, 6, 3), (_TXT[3], -7, 80, 5)]]
_WD2 = [(t, f, s, x, y, w, n) for t in _THIS for f in _FONT for s, x, y, w, n in
        [(_TXT[0], 10, 20, 100, 19), (_TXT[1], 3, 4, 50, 0), (_TXT[2], 5, 6, 0, 3), (_TXT[3], -7, 80, 33, 5)]]


def _w(addr, args, vectors):
    return dict(module="golf_clean.exe", addr=addr, abi="thiscall", ret="int", args=args, fixture="c3dq_tx_obj",
                state=_TXT_ALL, vectors=vectors)


_P = "pointer"
HOOKS["Widget_draw"] = _w(0x00476650, [_P, _P, _P, "int", "int", "int"], _WD)
HOOKS["Widget_draw2"] = _w(0x004766A0, [_P, _P, _P, "int", "int", "int", "int"], _WD2)
HOOKS["drawStringAt"] = _w(0x00476700, [_P, _P, _P, "int", "int", "int"], _WD)
HOOKS["drawStringAt2"] = _w(0x00476750, [_P, _P, _P, "int", "int", "int"], _WD)

# rect-aligned draws (this, text, rect, len)
_RD = [(t, s, r, n) for t in ["$tm", "$tplain", "$tneg"] for s in ["$s_plain", "$s_one", "$s_braces"]
       for r, n in [("$r0", 19), ("$r1", 3), ("$r2", 100), ("$r4", -1), ("$r5", 0), ("$r3", 5)]]
_RD += [("$tnofont", "$s_plain", "$r0", 5), ("$tnomeas", "$s_plain", "$r0", 5), ("$tm", 0, "$r0", 5),
        ("$tm", "$s_plain", 0, 5), ("$tm", "$s_empty", "$r0", 5), ("$tm", "$s_short", "$r1", 2),
        ("$tm", "$s_long", "$r1", 1000), ("$tm", "$s_spaces", "$r2", 6)]
HOOKS["drawCentered"] = _w(0x00477CD0, [_P, _P, _P, "int"], _RD)
HOOKS["drawCentered3"] = _w(0x00477EB0, [_P, _P, _P, "int"], _RD)


# ---- window tree --------------------------------------------------------------------------------------------------
# Callees that run ORIGINAL in both arms: Window::visible 0x004801f0, relayoutClip 0x0047e140 (and its IntersectRect
# import), View::offsetRectToParent / ToLocal 0x0047b0d0 / 0x0047b120, View::fromParent 0x0047b200. In-batch:
# rebuildDrawList -> collectDrawList, clipAllWindows; collectDrawList -> collectDrawList (recursive).
_WTS = _REC + [("$wins", 0, 8 * 0x300), (0x00839AC0, 0, 0x80), (0x0083AB90, 0, 4), (0x0083AAEC, 0, 4)]
_FOCI = ["f0", "fw1", "fw6", "fw2", "fw5", "fs", "fw4"]
for _f_ in _FOCI:
    HOOKS[f"collectDrawList_{_f_}"] = dict(
        module="golf_clean.exe", addr=0x0047E330, abi="default", ret="void", args=["pointer"],
        fixture=f"c3dq_wt_a2_{_f_}", state=_WTS, vectors=[(f"$w{k}",) for k in range(8)])
for _a_, _f_ in [("a4", "f0"), ("a1", "f0"), ("a2", "f0"), ("a2", "fw1"), ("a2", "fw6"), ("a3", "fw1"), ("a2", "fs"),
                 ("a5", "fw2"), ("a5", "fw4")]:
    HOOKS[f"rebuildDrawList_{_a_}_{_f_}"] = dict(
        module="golf_clean.exe", addr=0x0047E450, abi="default", ret="void", args=[],
        fixture=f"c3dq_wt_{_a_}_{_f_}", state=_WTS, vectors=[()])
for _a_ in ["a1", "a2", "a3", "a4", "a5"]:
    HOOKS[f"clipAllWindows_{_a_}"] = dict(
        module="golf_clean.exe", addr=0x0047E2D0, abi="default", ret="void", args=[],
        fixture=f"c3dq_wt_{_a_}_f0", state=_WTS, vectors=[()])

_TL = [(f"$w{k}", f"$cx{k}", f"$cy{k}") for k in range(8)] + [("$w0", 0, "$cy0"), ("$w1", "$cx1", 0), ("$w2", 0, 0)]
for _c_ in ["n", "0", "1", "2", "5", "nodisp"]:
    HOOKS[f"View_toLocal_{_c_}"] = dict(
        module="golf_clean.exe", addr=0x0047AB50, abi="thiscall", ret="void", args=["pointer", "pointer", "pointer"],
        fixture=f"c3dq_wc_{_c_}", state=_REC + [("$cells", 0, 0x80)], vectors=_TL)


# ---- Window::paint / Window::paintBackground ------------------------------------------------------------------------
# Callees that run ORIGINAL in both arms: Surface_blitThunk 0x00475c60 / Surface_blit3 0x00475d00 (c3da), Surface_fill
# 0x00475da0 (c3t), Widget_clearArea 0x00478b30, Widget_fillArea 0x00478b50 (c3ab). Every handle slot is a recording stub.
_PW = ["$w0", "$w1", "$w2", "$w3", "$w4", "$w5", "$w6", "$w7"]
for _suffix, _fx in [("", "c3dq_pt_c"), ("_nocolour", "c3dq_pt_0")]:
    HOOKS["Window::paint" + _suffix] = dict(
        module="golf_clean.exe", addr=0x004808C0, abi="thiscall", ret="void", args=["pointer", "int"], fixture=_fx,
        state=_REC, vectors=[(w, c) for w in _PW for c in (0x41, -1)])
    HOOKS["Window::paintBackground" + _suffix] = dict(
        module="golf_clean.exe", addr=0x00480A10, abi="thiscall", ret="int", args=["pointer", "pointer", "int"],
        fixture=_fx, state=_REC,
        vectors=[(w, r, c) for w in _PW for r, c in [("$pr0", 0x41), ("$pr1", 7), ("$pr2", -1), ("$pr3", 0)]])


# ---- blitter, screen objects, primary surface ---------------------------------------------------------------------
# Callees that run ORIGINAL in both arms: flushBlitSurface (hooked here) <- shutdownBlitter; clearPrimarySurface
# 0x00483b10, Surface::createFromId 0x00483800 (c3dm) for setPrimarySurface.
_BLS = _REC + [(0x0083C328, 0, 0x10)]
for _name, _fx, _addr in [("flushBlitSurface", "bl", 0x004924B0), ("shutdownBlitter", "bl", 0x00492470)]:
    for _v in ["obj", "noctx", "n32c", "n328", "only328", "none"] + (["r0", "r1", "r2", "r3"] if _name == "flushBlitSurface" else []):
        HOOKS[f"{_name}_{_v}"] = dict(module="golf_clean.exe", addr=_addr, abi="default", ret="int", args=[],
                                       fixture=f"c3dq_bl_{_v}", state=_BLS, vectors=[()])

_CLS = _REC + [("$nodes", 0, 0x100), (0x0083AC88, 0, 0x20)]
for _v in ["empty", "n0", "n1", "n3", "n3b", "n2", "n3c", "n3d", "n1b"]:
    HOOKS[f"clearScreenObjects_{_v}"] = dict(module="golf_clean.exe", addr=0x00483340, abi="default", ret="int", args=[],
                                             fixture=f"c3dq_cl_{_v}", state=_CLS, vectors=[()])
HOOKS["clearScreenObjects_nodisp"] = dict(module="golf_clean.exe", addr=0x00483340, abi="default", ret="void", args=[],
                                          fixture="c3dq_cl_n3_nodisp", state=_CLS, vectors=[()])

_PSS = _REC + [("$surfs", 0, 0x100), (0x0083AD44, 0, 4)]
for _v in ["ok", "r0", "b8"]:
    HOOKS[f"setPrimarySurface_{_v}"] = dict(module="golf_clean.exe", addr=0x00483AC0, abi="default", ret="int",
                                            args=["pointer"], fixture=f"c3dq_ps_{_v}", state=_PSS,
                                            vectors=[(0,), ("$s0",), ("$s1",), ("$s2",), ("$s3",)])


# ---- tiled blits and the nine-patch compositor ------------------------------------------------------------------------
# Callees that run ORIGINAL in both arms: Image_draw 0x00473e60 (c3df, held there), Widget_fillRectR 0x00475b00 (the clip
# setter), Surface_blitThunk 0x00475c60, and the IntersectRect import. In-batch: drawComposite -> blitLeft, blitMid,
# blitRight; blitRight -> Image_draw (not hooked here).
_TLS = [("$rec", 0, 0x10000), ("$disp", 0, 0x40)]
_TV = [(0, 0, 10, 10, 20, 12), (3, 2, 10, 10, 20, 12), (-3, -2, 5, 5, 17, 13), (13, 7, -6, -4, 40, 30),
       (0, 0, 300, 200, 50, 50), (1, 1, 10, 10, 0, 0), (2, 2, 10, 10, -5, 7), (7, 9, 0, 0, 16, 12),
       (0, 0, 315, 235, 20, 20), (1000, 1000, 31, 17, 23, 29)]
_FLAGS = [0, 1, 0x7F, -1]
_LEFT = [("$D", t) + v + (_FLAGS[i % 4],) for t in ["$T0", "$T1", "$T5"] for i, v in enumerate(_TV)]
_LEFT += [("$D", "$T2") + _TV[0] + (1,), ("$D", "$T3") + _TV[0] + (1,), ("$D", "$T6") + _TV[1] + (0,),
          ("$D2", "$T0") + _TV[0] + (1,), ("$D", "$T4") + _TV[0] + (2,)]
_MID = [("$D", t) + v for t in ["$T0", "$T1", "$T5"] for v in _TV]
_MID += [("$D", "$T2") + _TV[0], ("$D", "$T3") + _TV[0], ("$D", "$T6") + _TV[1], ("$D2", "$T0") + _TV[0],
         ("$D", "$T4") + _TV[0]]
_RIGHT = [("$D", t) + v for t in ["$P0", "$P1", "$P4", "$P6", "$T0", "$T5"] for v in _TV]
_RIGHT += [("$D", "$T4") + _TV[0], ("$D", "$T6") + _TV[1], ("$D2", "$P0") + _TV[0], ("$D", "$P7") + _TV[2]]
_PARTS = ["$pfull", "$pnomask", "$pcorners", "$pedges", "$ptopbot", "$pmask", "$pleft", "$pright", "$pnone", "$pmix"]
_COMP = [("$D", r, p) for p in _PARTS for r in ["$R0", "$R1", "$R2", "$R4"]]
_COMP += [("$D", 0, "$pfull"), ("$D", "$R0", 0), ("$D2", "$R0", "$pfull"), ("$D", "$R3", "$pfull"), ("$D", "$R5", "$pfull")]
_P = "pointer"
for _c in ["full", "small", "none"]:
    HOOKS[f"blitLeft_{_c}"] = dict(
        module="golf_clean.exe", addr=0x00475E10, abi="thiscall", ret="int",
        args=[_P, _P, "int", "int", "int", "int", "int", "int", "int"], fixture=f"c3dq_tl_{_c}", state=_TLS,
        vectors=_LEFT)
    HOOKS[f"blitMid_{_c}"] = dict(
        module="golf_clean.exe", addr=0x00475FB0, abi="thiscall", ret="int",
        args=[_P, _P, "int", "int", "int", "int", "int", "int"], fixture=f"c3dq_tl_{_c}", state=_TLS, vectors=_MID)
# No display: Image_draw 0x00473e60 reads [0x0083ad50] without a test, so the no-display keys only offer vectors that
# return before any Image_draw (clip fully outside, or no piece to draw).
_RIGHT_ND = [(t, ) for t in []]
_COMP_ND = [("$D", r, p) for p in ["$pnone", "$pmask"] for r in ["$R0", "$R1", "$R2"]] + [("$D", 0, "$pfull"), ("$D", "$R0", 0)]
for _c in ["full", "small", "nodispnone"]:
    HOOKS[f"blitRight_{_c}"] = dict(
        module="golf_clean.exe", addr=0x00476140, abi="thiscall", ret="int",
        args=[_P, _P, "int", "int", "int", "int", "int", "int"], fixture=f"c3dq_tl_{_c}", state=_TLS, vectors=_RIGHT)
for _c in ["full", "small"]:
    HOOKS[f"drawComposite_{_c}"] = dict(
        module="golf_clean.exe", addr=0x004791B0, abi="thiscall", ret="void", args=[_P, _P, _P],
        fixture=f"c3dq_tl_{_c}", state=_TLS, vectors=_COMP)
HOOKS["drawComposite_nodisp"] = dict(
    module="golf_clean.exe", addr=0x004791B0, abi="thiscall", ret="void", args=[_P, _P, _P],
    fixture="c3dq_tl_nodisp", state=_TLS, vectors=_COMP_ND)


# ---- Window::center ---------------------------------------------------------------------------------------------------
# Callees that run ORIGINAL in both arms: View::fromParent 0x0047b200 (c3-hooked elsewhere), Window::moveTo 0x0047b420 (and
# the relayoutClip 0x0047e140 it calls).
for _c in ["big", "odd", "nodisp"]:
    HOOKS[f"Window::center_{_c}"] = dict(
        module="golf_clean.exe", addr=0x0047B310, abi="thiscall", ret="int", args=["pointer"],
        fixture=f"c3dq_wm_{_c}", state=_REC + [("$wins", 0, 8 * 0x300)], vectors=[(f"$w{k}",) for k in range(8)])


# ---- wrapped text layout -----------------------------------------------------------------------------------------------
# Callees that run ORIGINAL in both arms (hooked by c3df and earlier in the main build): measureMarkupRun 0x00476ef0, Widget_value
# 0x00477580, Text_put 0x00477250, drawText 0x00477c30 (-> drawRichText, ended by the fixture's draw callback), drawCentered2
# 0x00477da0.
_ML = _TXT_ALL + [("$lines", 0, 0x80), ("$cnt", 0, 4), (0x00839A9C, 0, 0x10), (0x00839A5C, 0, 4)]
_MT = ["$tm", "$tind", "$tplain"]
_MS = ["$s_plain", "$s_long", "$s_one", "$s_braces", "$s_spaces", "$s_empty", 0]
_MR = ["$r0", "$r5", "$r4"]
_M1 = [(t, s, _MR[(i + j) % 3], ("$lines" if (i + j) % 2 == 0 else 0), ("$cnt" if (i + j) % 2 == 0 else 0))
       for i, t in enumerate(_MT) for j, s in enumerate(_MS)]
_M1 += [("$tm", "$s_plain", "$r0", 0, "$cnt"), ("$tind", "$s_long", "$r4", "$lines", 0)]
_MW = [100, 40, 1, 0, 300]
_M2 = [(t, s, 5 + i, 7 + j, _MW[(i + j) % 5]) for i, t in enumerate(_MT) for j, s in enumerate(_MS)]
_M2 += [("$tm", "$s_long", -3, 4, 17), ("$tind", "$s_braces", 0, 0, 2)]
_M4 = [(t, s) for t in _MT for s in _MS] + [("$tnofont", "$s_empty"), ("$tm", "$s_long"), ("$tind", "$s_plain")]
_ML_KEYS = ["base", "wide", "pre"]
for _k in _ML_KEYS:
    HOOKS[f"measureText_{_k}"] = dict(
        module="golf_clean.exe", addr=0x004782D0, abi="thiscall", ret="int", args=[_P, _P, _P, _P, _P],
        fixture=f"c3dq_ml_{_k}", state=_ML, vectors=_M1)
    HOOKS[f"measureText2_{_k}"] = dict(
        module="golf_clean.exe", addr=0x00478430, abi="thiscall", ret="int", args=[_P, _P, "int", "int", "int"],
        fixture=f"c3dq_ml_{_k}", state=_ML, vectors=_M2)
    HOOKS[f"measureText3_{_k}"] = dict(
        module="golf_clean.exe", addr=0x00478530, abi="thiscall", ret="int", args=[_P, _P, "int", "int", "int"],
        fixture=f"c3dq_ml_{_k}", state=_ML, vectors=_M2)
for _k in ["base", "lim0", "wide", "pre"]:
    HOOKS[f"measureText4_{_k}"] = dict(
        module="golf_clean.exe", addr=0x00478610, abi="thiscall", ret="int", args=[_P, _P],
        fixture=f"c3dq_ml_{_k}", state=_ML, vectors=_M4)


# ---- Screen::close ---------------------------------------------------------------------------------------------------
# releaseSoundObj 0x00484110 runs ORIGINAL in both arms ([0x0083afc4] = 0: returns 1; set: calls [0x0083af70], a recorder).
for _k in ["nolib", "lib"]:
    HOOKS[f"Screen::close_{_k}"] = dict(
        module="golf_clean.exe", addr=0x004854C0, abi="thiscall", ret="int", args=["pointer"], fixture=f"c3dq_sc_{_k}",
        state=_REC + [("$scr", 0, 0x400)], vectors=[(f"$s{k}",) for k in range(5)])


# ---- drawScaledSprite421b60 ----------------------------------------------------------------------------------------------
# Surface_drawScaled 0x00473cb0 (c3df, held there) runs ORIGINAL in both arms: this+8 == 0 on every image of the tiles fixture,
# so the file-name arm (loadImage) never runs.
_DSS = [(t, d, x, y, n, den, m) for (t, d, x, y, n, den, m) in [
    ("$T0", "$D", 10, 20, 1, 1, 0), ("$T0", "$D", 10, 20, 2, 1, "$M"), ("$T0", "$D", -3, 7, 3, 2, "$P0"),
    ("$T1", "$D", 0, 0, 4, 5, 0), ("$T1", "$D", 100, 50, -2, 3, "$M"), ("$T5", "$D", 1, 2, 1, 7, "$P1"),
    ("$T5", "$D", 5, 6, 8, -3, 0), ("$T0", "$D2", 1, 1, 1, 1, 0), ("$T0", 0, 1, 1, 1, 1, 0), ("$T3", "$D", 1, 1, 2, 2, 0),
    ("$T2", "$D", 3, 3, 2, 2, "$M"), ("$T6", "$D", 9, 9, 1, 4, "$P2")]]
HOOKS["drawScaledSprite421b60"] = dict(
    module="golf_clean.exe", addr=0x00421B60, abi="thiscall", ret="int",
    args=["pointer", "pointer", "int", "int", "int", "int", "pointer"], fixture="c3dq_tl_full", state=_TLS, vectors=_DSS)


# ---- hitTestScreen ------------------------------------------------------------------------------------------------------
# Callees that run ORIGINAL in both arms: View::fromParent 0x0047b200, View::toGlobal 0x0047b2d0, hitTestTree 0x0047f340.
_HS = _REC + [("$wins", 0, 8 * 0x300), ("$pcells", 0, 0x80), (0x0083AB18, 0, 0x44), (0x00839AB8, 0, 8)]
_HV = [(f"$px{k}", f"$py{k}") for k in range(12)]
for _c in ["none", "c40", "c40bad", "c40dead", "c44", "c44bad", "g30", "g30bad", "g34", "g34bad", "f", "fe", "fenone", "fo",
           "flag0", "flag1", "empty"]:
    HOOKS[f"hitTestScreen_{_c}"] = dict(
        module="golf_clean.exe", addr=0x0047F700, abi="default", ret="pointer", args=["pointer", "pointer"],
        fixture=f"c3dq_hs_{_c}", state=_HS, vectors=_HV)

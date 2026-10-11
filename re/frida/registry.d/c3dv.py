# C3 batch c3dv (round 28, 2026-10-11) of golf_clean.exe: drawing rows earlier batches dropped. Reimplementations in
# shim/src/re/c3dv.cpp, fixtures in re/frida/js/fixtures.d/c3dv.js (every key freezes the game's first thread),
# Purposes and per-jcc branch tables in log/c3/c3dv_purpose.md.
#
# Callees that run ORIGINAL in both arms (not hooked here): drawText 0x00477c30, drawRichText 0x004775b0, setDrawTarget
# 0x004762d0 and the other C3 text wrappers (their hooks are in the main build and are switched off in the callee runs).

_REC = [("$rec", 0, 0x2000)]
_TOBJ = {"tnoh": ("$tnoh", 0, 0x300), "tm": ("$tm", 0, 0x300), "tind": ("$tind", 0, 0x300), "tplain": ("$tplain", 0, 0x300),
         "tneg": ("$tneg", 0, 0x300), "tbig": ("$tbig", 0, 0x300), "tmid": ("$tmid", 0, 0x300),
         "tmcb": ("$tmcb", 0, 0x300), "tsmall": ("$tsmall", 0, 0x300)}
_P = "pointer"

HOOKS = {}


def _f(addr, fixture, ret="int", state=_REC):
    return dict(module="golf_clean.exe", addr=addr, abi="default", ret=ret, args=[], fixture=fixture, state=state,
                vectors=[()])


# ---- drawTextBlock / TextView_beginDrawRun --------------------------------------------------------------------------
_TB_STATE = _REC + list(_TOBJ.values()) + [(0x00839A9C, 0, 0x10), (0x00839A5C, 0, 4), (0x00839658, 0, 0x404)]
_TXT = ["$s_plain", "$s_long", "$s_one", "$s_braces", "$s_spaces", "$s_empty", 0]
_THIS = ["$tm", "$tind", "$tplain", "$tneg"]
_TBV = [(t, s) for t in _THIS for s in _TXT]
for _k in ["base", "pre", "lim0", "acc", "accfull", "wide", "pre_acc", "neg"]:
    HOOKS[f"drawTextBlock_{_k}"] = dict(
        module="golf_clean.exe", addr=0x004787A0, abi="thiscall", ret="int", args=[_P, _P], fixture=f"c3dv_tb_{_k}",
        state=_TB_STATE, vectors=_TBV)
_BV = [(t, s, x, y, w) for t in _THIS for s in ["$s_plain", "$s_long", "$s_one", "$s_braces"]
       for x, y, w in [(11, 7, 60), (0, 0, 200), (-5, 30, 25)]]
_BV += [("$tm", 0, 1, 2, 60), ("$tm", "$s_empty", 1, 2, 60), ("$tm", "$s_plain", 3, 4, 0), ("$tind", "$s_spaces", 9, 9, 40)]
HOOKS["TextView_beginDrawRun"] = dict(
    module="golf_clean.exe", addr=0x00478750, abi="thiscall", ret="int", args=[_P, _P, "int", "int", "int"],
    fixture="c3dv_tb_pre", state=_TB_STATE, vectors=_BV)
HOOKS["TextView_beginDrawRun_base"] = dict(
    module="golf_clean.exe", addr=0x00478750, abi="thiscall", ret="int", args=[_P, _P, "int", "int", "int"],
    fixture="c3dv_tb_base", state=_TB_STATE, vectors=_BV)

# ---- the message buffer 0x0051a068 as input -----------------------------------------------------------------------------
_MSG_STATE = _REC + list(_TOBJ.values()) + [(0x0051A068, 0, 0x200)]
_MSGK = ["plain", "caret", "nl", "under", "nospace", "empty", "tail", "sp", "long"]
_COLS = [0, 1, 2, 4, 6, 10, 40, -1]
for _k in _MSGK:
    HOOKS[f"renderTextWidget45af30_{_k}"] = dict(
        module="golf_clean.exe", addr=0x0045AF30, abi="default", ret="int", args=["int"], fixture=f"c3dv_rt_{_k}",
        state=_MSG_STATE, vectors=[(c,) for c in _COLS])
_VW_COLOURS = [-2147450881, 0x1234, -1]
_VWV = [(c, x, y, col) for c in [0, 2, 5, 12, 40] for (x, y) in [(10, 20), (3, 0x230)] for col in _VW_COLOURS]
_VWV += [(2, 4, 0x240, -2147450881), (2, 4, 0x248, 7), (3, 4, 0x250, -2147450881), (1, 0, 0x300, 5), (40, 4, 0x250, 7),
         (40, 4, 0x249, -2147450881), (40, 4, 0x248, 9)]
for _k in _MSGK:
    HOOKS[f"drawValueWidget_{_k}"] = dict(
        module="golf_clean.exe", addr=0x0045B0D0, abi="default", ret="int", args=["int", "int", "int", "int"],
        fixture=f"c3dv_vw_{_k}", state=_MSG_STATE, vectors=_VWV)
    HOOKS[f"drawValueWidget_big_{_k}"] = dict(
        module="golf_clean.exe", addr=0x0045B0D0, abi="default", ret="int", args=["int", "int", "int", "int"],
        fixture=f"c3dv_vwb_{_k}", state=_MSG_STATE, vectors=_VWV)

# ---- floating labels -------------------------------------------------------------------------------------------------------
_LAB_STATE = _REC + list(_TOBJ.values()) + [
    (0x0056A924, 0, 0x20), (0x0056A794, 0, 0x20), (0x0056C570, 0, 0x244), (0x0051A068, 0, 0x200), (0x0051B360, 0, 0x20)]
_MON_STATE = _REC + list(_TOBJ.values()) + [
    (0x00542F00, 0, 0x20), (0x00542DD8, 0, 0x20), (0x00542FD8, 0, 0x20), (0x00542FF8, 0, 0x20), (0x0059E7B8, 0, 4),
    (0x0051A068, 0, 0x200), (0x0051B360, 0, 0x20), (0x0058A528, 0, 0x20)]
for _k in ["a", "b", "c", "nofont", "none", "neg"]:
    HOOKS[f"drawLabels_{_k}"] = _f(0x0040C7A0, f"c3dv_lab_{_k}", "int", _LAB_STATE)
for _k in ["a", "b", "c", "flag", "flag2", "nofont", "pos", "nonpos"]:
    HOOKS[f"drawMoneyLabels_{_k}"] = _f(0x0040C910, f"c3dv_mon_{_k}", "int", _MON_STATE)

# ---- window refresh family -------------------------------------------------------------------------------------------------
# Callees that run ORIGINAL in both arms: relayoutClip 0x0047e140 (and its IntersectRect import), Window::visible 0x004801f0,
# View::toParent 0x0047b170, redrawDrawList 0x0047f8e0, Window::refreshRect 0x00480c20, drawTooltip 0x0047cdb0 (its text
# pointer 0x0083aac4 is 0 in every key; its closing call of display slot +0xa4 reaches a recording display).
_RWS = [("$rec", 0, 0x4000), ("$wins", 0, 6 * 0x300), (0x00839AC0, 0, 0x80), (0x0083AB90, 0, 4), (0x0083AAE0, 0, 4),
        (0x0083AAA0, 0, 0x30)]
_RWK = ["a", "focus", "focus4", "empty"]
_MV = [(f"$w{k}", x, y) for k in range(6) for (x, y) in [(0, 0), (50, 60), (-7, 300)]] + [("$w0", 10, 20), ("$w4", 80, 90)]
_RWV = [(f"$w{k}", r) for k in (0, 1, 4, 5, 3) for r in (0, "$r0", "$r1", "$r2", "$r3")]
_RFV = [(f"$w{k}", a) for k in range(6) for a in (0, 5)]
HOOKS["Window::moveTo"] = dict(module="golf_clean.exe", addr=0x0047B420, abi="thiscall", ret="int",
                               args=[_P, "int", "int"], fixture="c3dv_rw_a", state=_RWS, vectors=_MV)
for _k in _RWK:
    HOOKS[f"redrawWindow_{_k}"] = dict(module="golf_clean.exe", addr=0x0047FA30, abi="thiscall", ret="void",
                                       args=[_P, _P], fixture=f"c3dv_rw_{_k}", state=_RWS, vectors=_RWV)
    HOOKS[f"redrawRegion_{_k}"] = dict(module="golf_clean.exe", addr=0x00495770, abi="stdcall", ret="int",
                                       args=[_P], fixture=f"c3dv_rw_{_k}", state=_RWS,
                                       vectors=[(0,), ("$r0",), ("$r1",), ("$r2",), ("$r3",)])
    HOOKS[f"Window::refreshFull_{_k}"] = dict(module="golf_clean.exe", addr=0x00480C80, abi="thiscall", ret="int",
                                              args=[_P, "int"], fixture=f"c3dv_rw_{_k}", state=_RWS, vectors=_RFV)

# ---- listSetItems / Button::endPress -----------------------------------------------------------------------------------------
# Callees that run ORIGINAL in both arms: Window::moveTo 0x0047b420 (hooked in this batch: the main build runs the callee-off
# pass), relayoutClip 0x0047e140, radioSelect 0x004a0350 (hooked by c3dx).
_LSS = [("$rec", 0, 0x2000), ("$obj", 0, 0x3600)]
for _k in ["a", "odd", "noh", "hidden"]:
    HOOKS[f"listSetItems_{_k}"] = dict(module="golf_clean.exe", addr=0x00494020, abi="thiscall", ret="int",
                                       args=[_P, _P], fixture=f"c3dv_ls_{_k}", state=_LSS,
                                       vectors=[("$obj", f"$i{n}") for n in range(5)])
_BTS = [("$rec", 0, 0x2000), ("$objs", 0, 6 * 0x700), ("$grp", 0, 0x200), (0x0083AB2C, 0, 12), (0x0083AB0C, 0, 8)]
for _k in ["a", "b", "nocb", "none"]:
    HOOKS[f"Button::endPress_{_k}"] = dict(module="golf_clean.exe", addr=0x00488EB0, abi="thiscall", ret="int",
                                           args=[_P, "int", "int"], fixture=f"c3dv_bt_{_k}", state=_BTS,
                                           vectors=[(f"$b{n}", 1, 2) for n in range(6)])

# ---- panel layout builders ---------------------------------------------------------------------------------------------------
# Callees that run ORIGINAL in both arms: Text_put 0x00477250, Widget_get 0x00477560, Widget_value 0x00477580, setQuad 0x00476310 /
# 0x00476340 / 0x00476370 and setDrawTarget 0x004762d0 (all hooked in the main build: callee-off pass).
_PBS = [("$rec", 0, 0x2000), ("$E", 0, 16 * 0x200), ("$BK", 0, 16 * 0x200), ("$IT", 0, 0x2000), ("$tm", 0, 0x300)]
for _n, _a, _fx, _cnt in [("panelBuildText", 0x0049D770, "c3dv_pb_pbt", 16), ("optionDialogDraw", 0x0049F050, "c3dv_pb_odd", 15),
                          ("panelBuildText4a3be0", 0x004A3BE0, "c3dv_pb_pb2", 13)]:
    HOOKS[_n] = dict(module="golf_clean.exe", addr=_a, abi="thiscall", ret="int", args=[_P], fixture=_fx, state=_PBS,
                     vectors=[(f"$o{k}",) for k in range(_cnt)])

# ---- drawRoutingPoly ---------------------------------------------------------------------------------------------------------
# fillConvexPoly 0x00493520 runs ORIGINAL in both arms in the instance run only when c3db.cpp is not linked; log/c3/c3dv_batches.txt
# links c3db.cpp so that its fixture c3db_a (11 synthetic jgld Surfaces and the polygon tables) is available, and the callee-off
# pass switches fillConvexPoly and its callees off. The polygon vectors are c3db's own.
_ARENA = [("$arena", 0, 0xC000)]
_POLY = _ARENA + [(0x0083D34C, 0, 0x48)]
_POLYS = [("$v_tri", 3), ("$v_quad", 4), ("$v_out", 3), ("$v_flat", 3), ("$v_pent", 5), ("$v_diam", 4),
          ("$v_thin", 4), ("$v_tri", 0), ("$v_tri", -1), ("$v_tri", 2)]
_RP = [(w, v, n, c) for w in ["$w0", "$w1", "$w2", "$w3", "$w5", "$w7"]
       for (v, n), c in zip(_POLYS[:5], [0x41, 0x17, 0x80001234, 0x1FF, -1])]
HOOKS["drawRoutingPoly"] = dict(module="golf_clean.exe", addr=0x00475DF0, abi="thiscall", ret="int",
                                args=[_P, _P, "int", "int"], fixture="c3dv_rp_a", state=_POLY, vectors=_RP)

# ---- ListBox::onScroll -------------------------------------------------------------------------------------------------------
# The sixteen ListBoxes are c3ct's (fixture c3ct_list, this = $t<i>; c3ct.cpp is linked through c3dv_batches.txt). Callees that
# run ORIGINAL in both arms: ListBox::setSelection 0x00489f50 (hooked by c3ct), Window::invalidate 0x00480ce0 (W+0xa0 = 0, so it
# returns at once). Key _key sets [0x0083ff18] = 1 (the setSelection arm).
_LB_N = 16
_LBS = ([(f"$blk{i}", 0, 0x800) for i in range(_LB_N)] + [(f"$aux{i}", 0, 0x1000) for i in range(_LB_N)]
        + [(f"$lst{i}", 0, 0x400) for i in range(_LB_N)]
        + [("$rec", 0, 0x2000), (0x0083AB2C, 0, 4), (0x0083B650, 0, 4), (0x0083B654, 0, 4), (0x0083FF18, 0, 4)])
_OSV = [(f"$t{i}", k, v) for i in range(_LB_N) for (k, v) in [(-2, 3), (-2, 0), (-1, 5), (-1, 0), (0, 7), (7, -2)]]
HOOKS["ListBox::onScroll"] = dict(module="golf_clean.exe", addr=0x0048C420, abi="thiscall", ret="int",
                                  args=[_P, "int", "int"], fixture="c3dv_os_a", state=_LBS, vectors=_OSV)
HOOKS["ListBox::onScroll_key"] = dict(module="golf_clean.exe", addr=0x0048C420, abi="thiscall", ret="int",
                                      args=[_P, "int", "int"], fixture="c3dv_os_key", state=_LBS, vectors=_OSV)

# ---- comboLayoutItems ------------------------------------------------------------------------------------------------------------
# Image_draw 0x00473e60 (c3df, held) and blitRight 0x00476140 (c3dq) run ORIGINAL in both arms in the instance (not linked there).
HOOKS["comboLayoutItems"] = dict(module="golf_clean.exe", addr=0x00493F50, abi="thiscall", ret="int", args=[_P],
                                 fixture="c3dv_cl_a", state=[("$rec", 0, 0x8000), ("$cbs", 0, 6 * 0x3600), ("$disp", 0, 0x40)],
                                 vectors=[(f"$c{k}",) for k in range(6)])


# ---- drawMenuList --------------------------------------------------------------------------------------------------------------
# Callees that run ORIGINAL in both arms: listIterNext 0x00402130 / listIterValue 0x00402160 (C3), measureText3 0x00478530 (c3dq),
# TextView_beginDrawRun 0x00478750 and drawTextBlock 0x004787a0 (this batch), Text_flush 0x00478970 (c3dx), drawText 0x00477c30.
_MN_STATE = _TB_STATE + [("$MA", 0, 0x4000)]
_MNV = [(t, f"$m{m}", x, y, w, fl) for t in ("$tm", "$tmcb", "$tsmall", "$tnoh") for m in range(9)
        for (x, y, w, fl) in [(5, 10, 100, 0), (0, 0, 60, 2), (3, 90, 80, 0)]]
_MNV += [("$tm", 0, 1, 2, 60, 0), ("$tmcb", 0, 1, 77, 60, 2)]
HOOKS["drawMenuList"] = dict(module="golf_clean.exe", addr=0x004763D0, abi="thiscall", ret="int",
                             args=[_P, _P, "int", "int", "int", "int"], fixture="c3dv_mn_a", state=_MN_STATE, vectors=_MNV)

# ---- TextView::drawPanelWithImage -------------------------------------------------------------------------------------------------
# drawPanel4810d0 0x004810d0 runs ORIGINAL (window flag +0xa0 = 0: it returns at once), Image_draw 0x00473e60 runs ORIGINAL.
HOOKS["TextView::drawPanelWithImage"] = dict(
    module="golf_clean.exe", addr=0x00491270, abi="thiscall", ret="int", args=[_P, "int"], fixture="c3dv_dp_a",
    state=[("$rec", 0, 0x4000), ("$pns", 0, 6 * 0x1500), ("$disp", 0, 0x40)],
    vectors=[(f"$p{k}", i) for k in range(6) for i in (0, -1, 2, -5, 1)])

# ---- panelSub family ------------------------------------------------------------------------------------------------------------
# Callees that run ORIGINAL in both arms: Image_draw 0x00473e60, Surface_blitThunk 0x00475c60, drawCentered 0x00477cd0, setQuad
# 0x00476310 / 0x00476340 / 0x00476370, setDrawTarget 0x004762d0, Widget_drawBevelBox 0x00479560 (-> Widget_drawHLine / VLine).
_PDST = [("$rec", 0, 0x2000), ("$PD", 0, 11 * 0x2000), ("$disp", 0, 0x40)]
_PDIDX = [0, 1, 2, 3, -1, -2, 5, -3, 9]
HOOKS["panelDraw4a3f10"] = dict(module="golf_clean.exe", addr=0x004A3F10, abi="thiscall", ret="void", args=[_P, "int"],
                                fixture="c3dv_pd_a", state=_PDST, vectors=[(f"$q{k}", i) for k in range(10) for i in _PDIDX])

# ---- panelRow --------------------------------------------------------------------------------------------------------------------
# Callees that run ORIGINAL in both arms: Surface_blitThunk 0x00475c60, drawCentered 0x00477cd0.
_PRST = [("$rec", 0, 0x2000), ("$PR", 0, 11 * 0x3000), (0x0083AB2C, 0, 4)]
HOOKS["panelDrawRow"] = dict(module="golf_clean.exe", addr=0x004A2D50, abi="thiscall", ret="int", args=[_P], fixture="c3dv_pr_a",
                             state=_PRST, vectors=[(f"$r{k}",) for k in range(11)])
HOOKS["panelDrawRow_vtordisp"] = dict(module="golf_clean.exe", addr=0x004A3090, abi="thiscall", ret="int", args=[_P],
                                      fixture="c3dv_pr_a", state=_PRST, vectors=[(f"$u{k}",) for k in range(11)])
HOOKS["panelDrawRow_vtordisp_adj578"] = dict(module="golf_clean.exe", addr=0x004A3100, abi="thiscall", ret="int", args=[_P],
                                             fixture="c3dv_pr_a", state=_PRST, vectors=[(f"$v{k}",) for k in range(11)])

# ---- panelOnPick / panelOnKey / panelSubDrawContents and their vtordisp thunks ---------------------------------------------------
# Callees that run ORIGINAL in both arms: HotList::hitTest 0x00492a90, listFindById 0x004a4890, panelDraw4a3f10 (hooked in this
# batch: callee-off pass), Surface_blitThunk 0x00475c60. The thunks are entered with T = P + 0x40 ([T-4] = 0x40).
_PK = [0x25, 0x26, 0x27, 0x28, 0x62, 0x64, 0x66, 0x68, 0x41, 0x24, 0x70, 0x29, 0x63]
_PPTS = [(5, 5), (30, 30), (55, 55), (65, 65), (200, 200), (95, 95)]
HOOKS["panelOnPick"] = dict(module="golf_clean.exe", addr=0x004A4350, abi="thiscall", ret="int", args=[_P, "int", "int"],
                            fixture="c3dv_pd_a", state=_PDST, vectors=[(f"$p{k}", x, y) for k in range(10) for (x, y) in _PPTS])
HOOKS["panelOnKey"] = dict(module="golf_clean.exe", addr=0x004A4680, abi="thiscall", ret="int", args=[_P, "int"],
                           fixture="c3dv_pd_a", state=_PDST, vectors=[(f"$p{k}", key) for k in range(10) for key in _PK])
HOOKS["panelSubDrawContents"] = dict(module="golf_clean.exe", addr=0x004A45B0, abi="thiscall", ret="int", args=[_P],
                                     fixture="c3dv_pd_a", state=_PDST, vectors=[(f"$p{k}",) for k in range(10)])
HOOKS["panelOnPick_vtordisp"] = dict(module="golf_clean.exe", addr=0x004A49E0, abi="thiscall", ret="int", args=[_P, "int", "int"],
                                     fixture="c3dv_pd_a", state=_PDST, vectors=[(f"$t{k}", x, y) for k in range(10) for (x, y) in _PPTS])
HOOKS["panelOnKey_vtordisp"] = dict(module="golf_clean.exe", addr=0x004A4A40, abi="thiscall", ret="int", args=[_P, "int"],
                                    fixture="c3dv_pd_a", state=_PDST, vectors=[(f"$t{k}", key) for k in range(10) for key in _PK])
HOOKS["panelSubDrawContents_vtordisp"] = dict(module="golf_clean.exe", addr=0x004A49C0, abi="thiscall", ret="int", args=[_P],
                                              fixture="c3dv_pd_a", state=_PDST, vectors=[(f"$t{k}",) for k in range(10)])

HOOKS["panelDraw4a3f10_nodisp"] = dict(module="golf_clean.exe", addr=0x004A3F10, abi="thiscall", ret="void", args=[_P, "int"],
                                       fixture="c3dv_pd_nodisp", state=_PDST,
                                       vectors=[(f"$q{k}", i) for k in (1, 5) for i in _PDIDX])

# panelDraw with a second list whose count is 2 but whose head is NULL: only index 0 survives the original (a walk would dereference NULL)
HOOKS["panelDraw4a3f10_nolist2"] = dict(module="golf_clean.exe", addr=0x004A3F10, abi="thiscall", ret="void", args=[_P, "int"],
                                        fixture="c3dv_pd_a", state=_PDST, vectors=[("$q10", 0)])

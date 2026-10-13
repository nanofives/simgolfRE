# C3 batch c3eo (round 31, 2026-10-13) of golf_clean.exe: the map, roster and statistics screens, and the golfer-state
# reset one of them calls. Reimplementations in shim/src/re/c3eo.cpp; fixtures in re/frida/js/fixtures.d/c3eo.js;
# Purposes and per-jcc branch tables in log/c3/c3eo_purpose.md.
#
# Every key runs under __uiTree (frozen first thread, recording display at 0x0083ad50, heap and import stand-ins,
# neutral window-tree globals); keys of functions that loop on the pumps also run under __modal, and then carry
# MODAL_STATE (re/frida/js/modal.js).

EXE = "golf_clean.exe"

# re/frida/js/uitree.js: GLOBALS (4 bytes each), the z-list array, the recorder size
UI_GLOBALS = [0x0083AB94, 0x0083AB90, 0x0083AAE0, 0x0083AAEC, 0x0083AAA0, 0x0083AABC, 0x0083AAC0, 0x0083AAC4, 0x0083AA98,
              0x0083AAF0, 0x0083AB30, 0x0083AB34, 0x0083AB40, 0x0083AB44, 0x0083AB54, 0x0083AC18, 0x00839650, 0x0083AB2C,
              0x0083AA9C, 0x0083A2C8, 0x0083A2CC, 0x0083A2D0, 0x0083A2D4, 0x0083AD50, 0x0083AD4C]
REC_SIZE = 0x1000
ARENA = 0x4000


def UI_STATE(arena=ARENA):
    return ([("$heap", 0, 0x10 + arena), ("$freelog", 0, 0x404), ("$implog", 0, 0x400), ("$rec", 0, REC_SIZE)]
            + [(g, 0, 4) for g in UI_GLOBALS] + [(0x0083A2D8, 0, 0x200)])


# re/frida/js/modal.js: __modal.STATE (the block and the input globals the models and handlers write)
MODAL_STATE = [("$modal", 0, 0x1000), (0x00822B98, 0, 8), (0x00822D58, 0, 0x40), (0x004D3834, 0, 4), (0x0083AD4C, 0, 4),
               (0x0083AFCC, 0, 4), (0x00838208, 0, 4), (0x0083821C, 0, 4), (0x0083AB9C, 0, 4)]

# c3eo fixture regions: the stood-in callees' result sequences and their call log
FIX_STATE = [("$seq", 0, 16 * 0x48), ("$clog", 0, 0x8000)]

HOOKS = {}

# ------------------------------------------------------------------------------------ resetGolferState 0x0046d0c0
# Written: the 19 course records 0x00575ab0 (0x208 each), the flag 0x005a47e0, and the two destination blocks the
# function copies over (0x004d6088 0x2df0 dwords, 0x00543d10 0x668a dwords). clearObjectFootprint and initGolferState
# are stood in, so their own writes are out and their calls are in $clog.
_RG_STATE = UI_STATE() + FIX_STATE + [(0x00575AB0, 0, 0x2698), (0x005A47E0, 0, 4),
                                      (0x004D6088, 0, 0x2DF0 * 4), (0x00543D10, 0, 0x668A * 4)]
_RG = dict(module=EXE, addr=0x0046D0C0, abi="default", ret="int", args=[], state=_RG_STATE, vectors=[()])

HOOKS["resetGolferState"] = dict(fixture="c3eo_rg_low", **_RG)
for _k in ("skip", "bonus", "nobonus", "mid", "high", "top", "sub", "neg", "edge", "objs", "fa"):
    HOOKS[f"resetGolferState_{_k}"] = dict(fixture=f"c3eo_rg_{_k}", **_RG)

# ------------------------------------------------------------------------------------ showMembershipRoster 0x00454c50
# Written: the 76 golfer-type records 0x005849e0 (the pass marker 0x80 of byte +2), the shared text buffer 0x0051a068,
# the itoa scratch 0x00821c50 and the byte 0x00569498 the exit clears. Everything it draws goes to the stood-in
# drawing calls, compared in $clog; the pumps are modal.js models, compared in $modal.
_MR_STATE = (UI_STATE() + FIX_STATE + MODAL_STATE
             + [(0x005849E0, 0, 0x2C * 76), (0x0051A068, 0, 0x100), (0x00821C50, 0, 0x20), (0x00569498, 0, 1)])
_MR = dict(module=EXE, addr=0x00454C50, abi="default", ret="void", args=[], state=_MR_STATE, vectors=[()])

HOOKS["showMembershipRoster"] = dict(fixture="c3eo_mr_few", **_MR)
for _k in ("empty", "many", "stats", "scroll", "scrolllimit", "zones", "pressnone", "key", "keyhover"):
    HOOKS[f"showMembershipRoster_{_k}"] = dict(fixture=f"c3eo_mr_{_k}", **_MR)

# ------------------------------------------------------------------------------------ showHoleStatsScreen 0x00453330
# The hole index is the vector, so each key is 18 calls over one seeded course table. Written: the shared text buffer
# 0x0051a068, the itoa scratch 0x00821c50 and the dword 0x0053df54 the exit clears; everything drawn goes to the
# stood-in calls in $clog and the pumps to $modal.
_HS_STATE = (UI_STATE() + FIX_STATE + MODAL_STATE
             + [(0x0051A068, 0, 0x100), (0x00821C50, 0, 0x20), (0x0053DF54, 0, 4)])
_HS = dict(module=EXE, addr=0x00453330, abi="default", ret="void", args=["int"], state=_HS_STATE,
           vectors=[(h,) for h in range(1, 19)])

HOOKS["showHoleStatsScreen"] = dict(fixture="c3eo_hs", **_HS)
for _k in ("p1", "p2", "p3", "p4", "click", "hover", "phase", "norounds", "nopar", "zero"):
    HOOKS[f"showHoleStatsScreen_{_k}"] = dict(fixture=f"c3eo_hs_{_k}", **_HS)

# ---------------------------------------------------------------------------------- showTournamentResults 0x0045a090
# Written: the purse [0x00567b04], the cash [0x00571fd4], the counter [0x005787cc], the per-year table 0x0058421e
# (0x14 per entry), the layout flags [0x0059e7b8] (the exit clears bit 0x200000), the byte 0x00569498 and the two
# text buffers. logTick, postEvent, playSound and resetGolferState are stood in, so their own writes are out and
# their calls are in $clog.
_TR_STATE = (UI_STATE() + FIX_STATE + MODAL_STATE
             + [(0x0051A068, 0, 0x100), (0x00821C50, 0, 0x20), (0x00569498, 0, 1), (0x00567B04, 0, 4),
                (0x00571FD4, 0, 4), (0x005787CC, 0, 4), (0x0059E7B8, 0, 4), (0x00584210, 0, 0x7D0)])
_TR = dict(module=EXE, addr=0x0045A090, abi="default", ret="void", args=[], state=_TR_STATE, vectors=[()])

HOOKS["showTournamentResults"] = dict(fixture="c3eo_tr", **_TR)
for _k in ("alt", "alt0", "alt2", "alt3", "altd", "scores", "nopurse", "prize", "win", "win10", "winpoor",
           "many", "tall", "dead", "click"):
    HOOKS[f"showTournamentResults_{_k}"] = dict(fixture=f"c3eo_tr_{_k}", **_TR)

import os

# C3 batch c3fw (2026-10-15) of golf_clean.exe: loadGameBrowserScreen 0x0043b610, showThemePackSelect 0x004725b0,
# pickAProScreen 0x0043a8c0 and courseActionMenu 0x0046f550.
# Reimplementations: shim/src/re/c3fw.cpp.
# Fixtures: re/frida/js/fixtures.d/c3fw.js. Purpose and per-jcc table: log/c3/c3fw_purpose.md. Notes: log/c3/c3fw_notes.txt.
#
# Every vector runs under __modal (pumps with a scripted input queue), __frameMask (recording stand-ins that log frame
# pointers as object tokens; ??_L / ??_M forward to the real iterators so the 15 frame Surfaces are really constructed
# and destroyed) and __uiTree (frozen first thread, recording display, import stand-in). __cdecl (int resume, int
# championship), `ret` at 0x0043cb2f: the getter of fx.rN (the "$rN" first argument) selects row N's script, directory and
# globals and returns its `resume`; the second argument is the row's championship flag.

EXE = "golf_clean.exe"

UI_GLOBALS = [0x0083AB94, 0x0083AB90, 0x0083AAE0, 0x0083AAEC, 0x0083AAA0, 0x0083AABC, 0x0083AAC0, 0x0083AAC4, 0x0083AA98,
              0x0083AAF0, 0x0083AB30, 0x0083AB34, 0x0083AB40, 0x0083AB44, 0x0083AB54, 0x0083AC18, 0x00839650, 0x0083AB2C,
              0x0083AA9C, 0x0083A2C8, 0x0083A2CC, 0x0083A2D0, 0x0083A2D4, 0x0083AD50, 0x0083AD4C]
REC_SIZE = 0x1000
ARENA = 0x4000          # = the fixture's __uiTree arenaSize


def UI_STATE(arena=ARENA):
    return ([("$heap", 0, 0x10 + arena), ("$freelog", 0, 0x404), ("$implog", 0, 0x400), ("$rec", 0, REC_SIZE)]
            + [(g, 0, 4) for g in UI_GLOBALS] + [(0x0083A2D8, 0, 0x200)])


MODAL_STATE = [("$modal", 0, 0x1000), (0x00822B98, 0, 8), (0x00822D58, 0, 0x40), (0x004D3834, 0, 4), (0x0083AD4C, 0, 4),
               (0x0083AFCC, 0, 4), (0x00838208, 0, 4), (0x0083821C, 0, 4), (0x0083AB9C, 0, 4)]
FM_STATE = [("$fmlog", 0, 0x8000), ("$pathlog", 0, 0x800)]

# Written by the body: the shared text buffer 0x0051a068 and _itoa's buffer 0x005a9f6c (both read back by the stand-ins'
# string records), the course type byte 0x005a34e0 and flags 0x0059e7b8 it saves at entry and restores on exit. Written
# by the stand-ins (the same way in both arms): the name table 0x0080b130 (48 x 100), the theme name 0x00567328 and the
# course globals of a preview.
_GLOBALS = [(0x0051A068, 0, 0x400), (0x005A9F6C, 0, 0x40), (0x005A34E0, 0, 1), (0x0059E7B8, 0, 4), (0x0080B130, 0, 4800),
            (0x00567328, 0, 0x40), (0x005685F0, 0, 4), (0x0059AAFC, 0, 4), (0x0058D36C, 0, 4), (0x00571FD4, 0, 4),
            (0x0059AE78, 0, 4), (0x005A882C, 0, 4), (0x0056949C, 0, 4), (0x005A636C, 0, 4), (0x0056A524, 0, 4),
            (0x0059BF90, 0, 4), (0x00575CB8, 0, 0x208 * 8), (0x00571FF4, 0, 0x2E * 4)]
_STATE = UI_STATE() + MODAL_STATE + FM_STATE + _GLOBALS

# the championship flag of each fixture row (re/frida/js/fixtures.d/c3fw.js ROWS[i].champ)
_CHAMP = [0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0]
_VECS = [("$r%d" % i, c) for i, c in enumerate(_CHAMP)]
_LGB = dict(module=EXE, addr=0x0043B610, abi="default", ret="int", args=["int", "int"], state=_STATE)

HOOKS = {"loadGameBrowserScreen": dict(fixture="c3fw_lgb", vectors=_VECS, **_LGB)}

# control (C3FW_CONTROLS=1, log/c3/alt/c3fw_ctl.py only): the frame mask off; a correct body must read RED
if os.environ.get("C3FW_CONTROLS") == "1":
    HOOKS["loadGameBrowserScreen_raw"] = dict(fixture="c3fw_lgb_raw", vectors=_VECS, **_LGB)

# ------------------------------------------------------------------------------------------ showThemePackSelect 0x004725b0
# __cdecl, no arguments, `ret` at 0x004732ca; the "$rN" word is an unused cdecl stack word whose getter selects the row.
# It returns the eax of its last ??_M call (0x004732ad), which both arms make with the same element destructor chain.
# Written by the body: the shared text buffer 0x0051a068 and the current theme name 0x00567328 (a theme row press,
# 0x00473165; the back exit, 0x00473197); the stand-in listThemes writes the theme table 0x0056e928 (20 x 0x32).
_TP_STATE = UI_STATE() + MODAL_STATE + [("$fmlog", 0, 0x8000)] + [(0x0051A068, 0, 0x400), (0x00567328, 0, 0x40),
                                                                   (0x0056E928, 0, 0x32 * 20)]
_TP = dict(module=EXE, addr=0x004725B0, abi="default", ret="int", args=["int"], state=_TP_STATE)
HOOKS["showThemePackSelect"] = dict(fixture="c3fw_tp", vectors=[("$r%d" % i,) for i in range(9)], **_TP)
if os.environ.get("C3FW_CONTROLS") == "1":
    HOOKS["showThemePackSelect_raw"] = dict(fixture="c3fw_tp_raw", vectors=[("$r%d" % i,) for i in range(9)], **_TP)

# ------------------------------------------------------------------------------------------------ pickAProScreen 0x0043a8c0
# __cdecl, no arguments, `ret` at 0x0043b5ed; returns 1 after a load (0x0043b538), else 0. Written by the body: 0x0051a068,
# _itoa's buffer, the theme name 0x00567328 (set to 0x004c3c5c at entry, restored on the load exits), the word 0x0057956e
# (0x0043ae43); by the stand-ins: the name table, the pro's card (0x004d6098 .. 0x004d60aa, 0x005a5a04).
_PP_STATE = UI_STATE() + MODAL_STATE + FM_STATE + [(0x0051A068, 0, 0x400), (0x005A9F6C, 0, 0x40), (0x00567328, 0, 0x40),
                                                   (0x0080B130, 0, 4800), (0x004D6098, 0, 0x20), (0x005A5A04, 0, 0x10),
                                                   (0x0057956C, 0, 4)]
_PP = dict(module=EXE, addr=0x0043A8C0, abi="default", ret="int", args=["int"], state=_PP_STATE)
HOOKS["pickAProScreen"] = dict(fixture="c3fw_pp", vectors=[("$r%d" % i,) for i in range(10)], **_PP)
if os.environ.get("C3FW_CONTROLS") == "1":
    HOOKS["pickAProScreen_raw"] = dict(fixture="c3fw_pp_raw", vectors=[("$r%d" % i,) for i in range(10)], **_PP)

# ---------------------------------------------------------------------------------------------- courseActionMenu 0x0046f550
# __cdecl, one stack argument it never reads (callers push it and pop it: 0x00407d45 / 0x00407d4b), `ret` at 0x004709ba;
# returns -1 (back / a load, 0x004708f3) or the bought course record (0x004709a0). Written: cash 0x00571fd4 and the
# course records (the stand-in rows), 0x0051a068, _itoa's buffer 0x008392ac, the owned-course mask 0x00543cfc, the
# mouse word (MODAL_STATE); read: the date 0x00834170, flags 0x0059e7b8, the cheat word 0x005a6364.
_CAM_STATE = UI_STATE() + MODAL_STATE + FM_STATE + [(0x00571FD4, 0, 0x300), (0x0051A068, 0, 0x400), (0x008392AC, 0, 0x40),
                                                    (0x00834170, 0, 4), (0x0059E7B8, 0, 4), (0x005A6364, 0, 4),
                                                    (0x00543CFC, 0, 4)]
_CAM = dict(module=EXE, addr=0x0046F550, abi="default", ret="int", args=["int"], state=_CAM_STATE)
HOOKS["courseActionMenu"] = dict(fixture="c3fw_cam", vectors=[("$r%d" % i,) for i in range(10)], **_CAM)
if os.environ.get("C3FW_CONTROLS") == "1":
    HOOKS["courseActionMenu_raw"] = dict(fixture="c3fw_cam_raw", vectors=[("$r%d" % i,) for i in range(10)], **_CAM)

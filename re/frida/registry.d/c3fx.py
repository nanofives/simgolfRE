import os

# C3 batch c3fx (2026-10-16) of golf_clean.exe: showCharacterEditor 0x004385d0, the golfer / character editor.
# Reimplementation: shim/src/re/c3fx.cpp. Fixtures: re/frida/js/fixtures.d/c3fx.js. Purpose: log/c3/c3fx_purpose.md.
# Notes: log/c3/c3fx_notes.txt.
#
# Every vector runs under __modal (pumps with a scripted input queue), XREC (41 recording stand-ins in one ordered log,
# frame pointers logged as offsets into the frame Image) and __uiTree (frozen first thread, recording display, import
# stand-in). __cdecl (character, golfer), `ret` at 0x0043a337; it returns the eax of Image_dtor 0x00474c40 (0x0043a31e),
# which both arms take from the same stand-in. The getter of fx.rN (the "$rN" first argument) selects row N's script and
# seeds the records, and returns the row's character index; the second argument is the row's golfer index.

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

# Written by the body, the real callees (buildGolferName, Random::range, _itoa) or the stand-ins, the same regions the
# fixture saves under the freeze: character records 0..3 (0x004d6088, 0x230 each), the entry copy 0x004e1618, golfers 3
# and 4 (0x005797c0) and the preview golfer 0x99 (0x00582dc0), the type records 0..3 (0x005849e0), the comment lines of
# characters 0..3 (0x00543d10), the story table entries 0..3 (0x0058dd70), the pro skills 0x005a5a04, the comment colour
# 0x0058b198, the tip state 0x005aa554 / 0x005a9f54, 0x005aa55c, the sprite cell 0x005aa6c4, the text buffer 0x0051a068,
# _itoa's buffer 0x005a9f6c, the theme name 0x00567328, the name slots 0x0080b130, the EditBox 0x005a9f78 and the RNG.
_GLOBALS = [(0x004D6088, 0, 0x230 * 4), (0x004E1618, 0, 0x230), (0x005797C0, 0, 0x200), (0x00582DC0, 0, 0x100),
            (0x005849E0, 0, 0x2C * 4), (0x00543D10, 0, 0x4E2 * 4), (0x0058DD70, 0, 0x38 * 4), (0x005A5A04, 0, 0x10),
            (0x0058B198, 0, 4), (0x005AA554, 0, 4), (0x005A9F54, 0, 4), (0x005AA55C, 0, 4), (0x005AA6C4, 0, 4),
            (0x0051A068, 0, 0x400), (0x005A9F6C, 0, 0x40), (0x00567328, 0, 0x40), (0x0080B130, 0, 100 * 16),
            (0x005A9F78, 0, 0x5A8), (0x00822D9C, 0, 8)]
_STATE = UI_STATE() + MODAL_STATE + [("$xrec", 0, 0x10000), ("$bio", 0, 0x200)] + _GLOBALS

# the golfer index of each fixture row (re/frida/js/fixtures.d/c3fx.js ROWS[i].g)
_GOLFER = [3, 3, 3, 3, 4, 4, 3, 3, 3, 3, 4, 3, 3]
_VECS = [("$r%d" % i, g) for i, g in enumerate(_GOLFER)]
_CE = dict(module=EXE, addr=0x004385D0, abi="default", ret="int", args=["int", "int"], state=_STATE)

HOOKS = {"showCharacterEditor": dict(fixture="c3fx_ce", vectors=_VECS, **_CE)}

# control (C3FX_CONTROLS=1, log/c3/alt/c3fx_ctl.py only): the frame mapping off; a correct body must read RED
if os.environ.get("C3FX_CONTROLS") == "1":
    HOOKS["showCharacterEditor_raw"] = dict(fixture="c3fx_ce_raw", vectors=_VECS, **_CE)

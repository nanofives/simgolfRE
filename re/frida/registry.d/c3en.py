# C3 batch c3en (round 31, 2026-10-13) of golf_clean.exe: the report and list screens. Reimplementations in
# shim/src/re/c3en.cpp; fixtures in re/frida/js/fixtures.d/c3en.js; Purposes and per-jcc branch tables in
# log/c3/c3en_purpose.md; notes in log/c3/c3en_notes.txt.
#
# Every key runs under __uiTree (frozen first thread, heap and import stand-ins, recording display at 0x0083ad50) and
# __modal (the C3 pumps stood in by their models on the A/B thread, a scripted input queue, a virtual clock, a bound).
# The three screens take no argument; each vector passes the row index that the fixture's getter uses to select the
# row's script and seed its tables (the value itself is pushed and ignored, as c3el does for gfxFlushA).

EXE = "golf_clean.exe"

# re/frida/js/uitree.js GLOBALS, z-list and recorder (same list as c3ef / c3el)
UI_GLOBALS = [0x0083AB94, 0x0083AB90, 0x0083AAE0, 0x0083AAEC, 0x0083AAA0, 0x0083AABC, 0x0083AAC0, 0x0083AAC4, 0x0083AA98,
              0x0083AAF0, 0x0083AB30, 0x0083AB34, 0x0083AB40, 0x0083AB44, 0x0083AB54, 0x0083AC18, 0x00839650, 0x0083AB2C,
              0x0083AA9C, 0x0083A2C8, 0x0083A2CC, 0x0083A2D0, 0x0083A2D4, 0x0083AD50, 0x0083AD4C]
REC_SIZE = 0x1000
ARENA = 0x4000

# re/frida/js/modal.js: __modal.STATE
MODAL_STATE = [("$modal", 0, 0x1000), (0x00822B98, 0, 8), (0x00822D58, 0, 0x40), (0x004D3834, 0, 4), (0x0083AD4C, 0, 4),
               (0x0083AFCC, 0, 4), (0x00838208, 0, 4), (0x0083821C, 0, 4), (0x0083AB9C, 0, 4)]


def UI_STATE(arena=ARENA):
    return ([("$heap", 0, 0x10 + arena), ("$freelog", 0, 0x404), ("$implog", 0, 0x400), ("$rec", 0, REC_SIZE)]
            + [(g, 0, 4) for g in UI_GLOBALS] + [(0x0083A2D8, 0, 0x200)])


# the fixture's own log of the stood-in drawing calls (with the text they were given)
FIX_STATE = [("$clog", 0, 0x2000), ("$seq", 0, 0x40)]

# the text buffer 0x0051a068, appendNumber's itoa scratch 0x0058a528, the screens' own itoa scratch 0x00821c50 and the
# flag every page clears on the way out
TEXT_STATE = [(0x0051A068, 0, 0x180), (0x0058A528, 0, 0x20), (0x00821C50, 0, 0x20), (0x00569498, 0, 4)]

# showFinancialReport: the year count, the 100 short rows it reads and writes, the label pointer table and the text a
# zero entry copies over the number
FIN_STATE = [(0x005A6D3C, 0, 4), (0x00584210, 0, 100 * 0x14), (0x004D2054, 0, 0x24), (0x004E9A84, 0, 0x10)]

# showBestScoresScreen: the hole count, the ten score ints, the two row metrics and appendString's offsets / pool
BEST_STATE = [(0x005685F0, 0, 4), (0x0056A524, 0, 0x28), (0x0082232C, 0, 4), (0x008222D4, 0, 4),
              (0x0059D81C + 0x14 * 2, 0, 0x18), (0x0056FCB0, 0, 0x80)]

# showHireEmployeeDialog: the label pointers, the cost bytes and the frame-count table
HIRE_STATE = [(0x004C1464, 0, 0x20), (0x004C2E2C, 0, 0x10), (0x0053FC20, 0, 0x20)]

# showPlayerCommentsReport: the sprite y base, the 18 hole records and the colour word the comment builder leaves
PC_STATE = [(0x008223DC, 0, 4), (0x00575C1C, 0, 18 * 0x208 + 0x100), (0x0058B198, 0, 4)]

# showCreditsScreen: the reader's line pointer and the three font-metric globals the line height comes from
CR_STATE = [(0x0083B164, 0, 4), (0x00821028, 0, 4), (0x0082102C, 0, 4), (0x00821030, 0, 4)]

# selectDifficultyScreen: only the text buffer (the cancel dwell copies into it) and the exit flag
SD_STATE = []

# showHistographScreen: the two totals that scale the axes, the date, the four series and the event words, the three
# event text tables, the text pointer, the screen surface pointer and appendCents' last-value global
_WK = 24
HG_STATE = [(0x00571FD4, 0, 4), (0x00541CD8, 0, 4), (0x00834170, 0, 4), (0x005409AE, 0, 2 * _WK),
            (0x0051B38A, 0, 2 * _WK), (0x0053E63E, 0, 2 * _WK), (0x00520642, 0, 2 * _WK), (0x00568602, 0, 2 * _WK),
            (0x004C26B0, 0, 0x20 * 20), (0x0058DD50, 0, 0x20 * 56), (0x0055D738, 0, 0x20 * 0x25), (0x004C2C18, 0, 4),
            (0x00519CD8, 0, 4), (0x00569628, 0, 4)]


def _h(addr, fixture, n, state, ret="void"):
    return dict(module=EXE, addr=addr, abi="default", ret=ret, args=["int"], fixture=fixture,
                state=UI_STATE() + MODAL_STATE + FIX_STATE + TEXT_STATE + list(state),
                vectors=[(f"$r{i}",) for i in range(n)])


HOOKS = {
    "showFinancialReport": _h(0x0044F6B0, "c3en_fin", 16, FIN_STATE),
    "showBestScoresScreen": _h(0x00455A30, "c3en_best", 16, BEST_STATE),
    "showHireEmployeeDialog": _h(0x00459400, "c3en_hire", 20, HIRE_STATE, ret="int"),
    "showPlayerCommentsReport": _h(0x004546B0, "c3en_pc", 16, PC_STATE),
    "showHistographScreen": _h(0x00455ED0, "c3en_hg", 17, HG_STATE),
    "selectDifficultyScreen": _h(0x0043A400, "c3en_sd", 17, SD_STATE, ret="int"),
    "showCreditsScreen": _h(0x0044B9C0, "c3en_cr", 12, CR_STATE),
}

# Bound controls (C3EN_CONTROLS=1): a script with no input at all must end as an ERROR row inside the bound, never a
# hang; they are not part of the evidence and are off by default.
import os  # noqa: E402

if os.environ.get("C3EN_CONTROLS"):
    HOOKS["showFinancialReport_noexit"] = _h(0x0044F6B0, "c3en_fin_noexit", 1, FIN_STATE)
    HOOKS["showBestScoresScreen_noexit"] = _h(0x00455A30, "c3en_best_noexit", 1, BEST_STATE)
    HOOKS["showHireEmployeeDialog_noexit"] = _h(0x00459400, "c3en_hire_noexit", 1, HIRE_STATE, ret="int")
    HOOKS["showPlayerCommentsReport_noexit"] = _h(0x004546B0, "c3en_pc_noexit", 1, PC_STATE)
    HOOKS["showHistographScreen_noexit"] = _h(0x00455ED0, "c3en_hg_noexit", 1, HG_STATE)
    # selectDifficultyScreen and showCreditsScreen have NO bound control: both install an exception frame
    # (0x0043a400..0x0043a40e, 0x0044b9c0..0x0044b9ce), and the handler of the ORIGINAL arm catches modal.js's bound
    # fault (the read of 0x00000b0d) before Frida sees it, which takes the process down instead of ending the key as an
    # ERROR row. Their fixtures c3en_sd_noexit / c3en_cr_noexit are kept for the record but are not registered.

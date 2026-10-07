# C3 batch c3ai (2026-10-08): text builders of golf_clean.exe. Reimplemented in shim/src/re/c3ai.cpp, fixtures in
# re/frida/js/fixtures.d/c3ai.js, per-jcc branch coverage in log/c3/c3ai_purpose.md.
#
# expandTextMarkup 0x004942f0 takes (input, output) and writes only the output buffer, so the 43 outputs are carved
# out of one block ("$outs") that is the state region; the combo object the $DROPDOWN path walks is a fixture
# allocation whose cursor/index fields comboFindItem writes, so its list base is a state region too.
# buildScenarioIntro 0x0045fd80 takes no arguments: one registry entry per fixture variant, each a single call whose
# writes to the shared text buffer, the itoa scratch and the message-ticker globals are the compared state.
#
# Hooked callees (any batch), which the verifier must switch off when re-running these keys on the main build:
#   expandTextMarkup  -> findByteInRange 0x004935f0 (c3w), comboFindItem 0x004940e0 (c3y),
#                        findMarkupTokenIndex 0x004a4ad0 (c3s)
#   buildScenarioIntro-> appendCourseTitle 0x0040daa0 (c3g), startMessage 0x0040cb00 (c3g)

_MARKUP_N = 43          # inputs i0..i42 / outputs o0..o42 in c3ai.js (MARKUP)
_ELISION_N = 16         # inputs i0..i15 / outputs o0..o15 in c3ai.js (elisionInputs)

# comboFindItem writes the cursor [base+0xc] and the index [base+0x14] of the list at combo+0x2e58.
_COMBO_STATE = [("$combo", 0x2E58, 0x20)]

_EXPAND = dict(module="golf_clean.exe", addr=0x004942F0, abi="default", ret="int", args=["pointer", "pointer"])

# buildScenarioIntro writes the text buffer 0x0051a068 (the longest story plus the course title is under 0x300),
# the itoa scratch 0x00824134 and the ticker length 0x005a7144; startMessage adds the busy/direction/arg/sound/delay
# globals, the two Random::range outputs at 0x0056d1a8, the RNG seed 0x00822d9c (so both arms draw the same numbers)
# and the message copy at 0x005a6d40 (unbounded strcpy of the text buffer).
_INTRO_STATE = [(0x0051A068, 0, 0x300), (0x00824134, 0, 32), (0x005A7144, 0, 4),
                (0x0053DF54, 0, 4), (0x00569498, 0, 1), (0x005A34EC, 0, 4), (0x004C2E08, 0, 4),
                (0x005694A4, 0, 4), (0x0056D1A8, 0, 8), (0x00822D9C, 0, 4), (0x005A6D40, 0, 0x280)]

_INTRO = dict(module="golf_clean.exe", addr=0x0045FD80, abi="default", ret="void", args=[], state=_INTRO_STATE,
              vectors=[()])

HOOKS = {
    "expandTextMarkup": dict(fixture="c3ai_markup",
                             state=[("$outs", 0, 0x200 * _MARKUP_N)] + _COMBO_STATE,
                             vectors=[("$i%d" % i, "$o%d" % i) for i in range(_MARKUP_N)] +
                                     [(0, "$o0"), ("$i0", 0)],   # the two NULL-argument returns (3)
                             **_EXPAND),
    "expandTextMarkup_fr": dict(fixture="c3ai_markup_fr",
                                state=[("$outs", 0, 0x200 * _ELISION_N)] + _COMBO_STATE,
                                vectors=[("$i%d" % i, "$o%d" % i) for i in range(_ELISION_N)],
                                **_EXPAND),

    "buildScenarioIntro": dict(fixture="c3ai_intro_k0", **_INTRO),
    "buildScenarioIntro_k1": dict(fixture="c3ai_intro_k1", **_INTRO),
    "buildScenarioIntro_k2": dict(fixture="c3ai_intro_k2", **_INTRO),
    "buildScenarioIntro_k3": dict(fixture="c3ai_intro_k3", **_INTRO),
    "buildScenarioIntro_kdef": dict(fixture="c3ai_intro_kdef", **_INTRO),
    "buildScenarioIntro_kneg": dict(fixture="c3ai_intro_kneg", **_INTRO),
    "buildScenarioIntro_flag": dict(fixture="c3ai_intro_flag", **_INTRO),
    "buildScenarioIntro_late": dict(fixture="c3ai_intro_late", **_INTRO),
}

# showTutorialText 0x004604f0 is argument-free too: one entry per tutorial page. Pages 11 and 29 are left out on
# purpose (their block at 0x00460cea calls saveGame 0x0040b4a0, which writes a .sve file); pages 13..20 share
# page 12's dropped-page arm, so one entry covers it. State: the page number itself, the entry globals, the view
# mode, the camera pair and everything startMessage writes.
_TUT_STATE = [(0x0051A068, 0, 0x300), (0x00567A18, 0, 4), (0x00543D04, 0, 4), (0x005685F4, 0, 4),
              (0x004C2844, 0, 4), (0x004C2BA0, 0, 8),
              (0x0053DF54, 0, 4), (0x00569498, 0, 1), (0x005A34EC, 0, 4), (0x005A7144, 0, 4), (0x004C2E08, 0, 4),
              (0x005694A4, 0, 4), (0x0056D1A8, 0, 8), (0x00822D9C, 0, 4), (0x005A6D40, 0, 0x280)]
_TUT = dict(module="golf_clean.exe", addr=0x004604F0, abi="default", ret="void", args=[], state=_TUT_STATE,
            vectors=[()])

HOOKS["showTutorialText"] = dict(fixture="c3ai_tut_p1", **_TUT)        # page 1: the first page of the switch
for _p in (0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 21, 22, 23, 24, 25, 26, 27, 28):
    HOOKS["showTutorialText_p%d" % _p] = dict(fixture="c3ai_tut_p%d" % _p, **_TUT)

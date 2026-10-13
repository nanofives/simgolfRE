import os

# C3 batch c3fb (round 33, 2026-10-13): golf_clean.exe size-only rows of 1-2 KB.
# Reimplementations: shim/src/re/c3fb.cpp. Fixtures: re/frida/js/fixtures.d/c3fb.js.
# Purposes and per-jcc tables: log/c3/c3fb_purpose.md. Notes and drops: log/c3/c3fb_notes.txt.

EXE = "golf_clean.exe"

# serializeCourse 0x0040afa0 (cdecl, one argument: the mode word). The second "int" is an unused cdecl stack word the
# caller pops: diff_hook resolves "$cN" before the state snapshot, so the getter of fx.cN applies tape scene N (fd,
# flag word, the CRT stand-ins' answer).
_SER_STATE = [
    ("$clog", 0, 0x1000),      # the stand-ins' call log: every __read / __write (fd, buf, count) in order
    ("$tape", 0, 0x70010),     # the tape: cursor, bytes moved, calls, and every byte written (write mode)
    (0x0053E638, 0, 4),        # the mode word the body stores (0x0040afab)
    (0x0059E7B8, 0, 4),        # the flag word (read into in read mode, call 0x0040b36d)
    (0x00822C78, 0, 4),        # the first field (read into in read mode, call 0x0040afb0)
    (0x00520A28, 0, 0x40),     # the head of the last tail field (call 0x0040b48a)
]

# showTitleScreen 0x0043cd70 (cdecl, no arguments): one vector is one whole call; "$cN" picks the Image destructor
# stand-in's answer, which is the function's eax.
_TITLE_STATE = [
    ("$clog", 0, 0x4000),      # every playAudioFile / Image / loadImage / Surface_blit call with its arguments
]

# buildGolferBioText 0x004315e0 (cdecl, no arguments): one vector is one whole reset; "$cN" applies scene N (RNG seed,
# the golfer-type class flags, the synthetic names, initRecords' answer). Every region the body writes is state.
_BIO_STATE = [("$clog", 0, 0x400)] + [(a, 0, n) for a, n in [
    (0x005A8C38, 0x50), (0x00543CA0, 0x50), (0x00578804, 0x9C4), (0x005794B8, 0x9C00), (0x00575AB0, 0x28A0),
    (0x00585850, 0x1300), (0x0058BCB8, 0x1000), (0x004C1578, 0x420), (0x005689E8, 0x320), (0x005419D0, 0x40),
    (0x0053CAF0, 0x1388), (0x0056988C, 0x9C4), (0x005849E0, 0xE70), (0x00567B34, 0x260), (0x005830B8, 0x9C4),
    (0x0056A524, 0x28), (0x0059AE80, 0x1C8), (0x004C2C9C, 2), (0x0053EA24, 0x9C4), (0x0056FCB0, 0x1002),
    (0x0059D81C, 0x100), (0x00568600, 0x3E8), (0x0051A068, 0x200), (0x00822D9C, 4), (0x004D6088, 0xA640),
    (0x005736B0, 0x2410), (0x0059FC60, 0x3880), (0x00572CB0, 0xA00), (0x005685F0, 4), (0x00567B04, 4), (0x00572CAC, 4),
    (0x0059B730, 4), (0x004C284C, 4), (0x0059AAF8, 4), (0x0053A450, 4), (0x005787CC, 4), (0x00561258, 4),
    (0x0056A51C, 4), (0x0059AE7C, 4), (0x004C2E28, 4), (0x005A59F8, 4)]]

def _regions(pairs):
    return [(a, 0, n) for a, n in pairs]


# customizeCharacter 0x0040f190 (cdecl, one argument: withPro); the second "int" is the unused stack word "$cN" that
# applies scene N (golfer table pattern, pro table, club bytes, updateMembership / loadThemeFile answers).
_CC_STATE = [("$clog", 0, 0x800)] + _regions([
    (0x005794B8 - 0x200, 0x9C00 + 0x200), (0x005689E8, 0x320), (0x0051A068, 0x200), (0x004E06C8, 0x40),
    (0x0055B028, 0x4E4), (0x004C2C9C, 2), (0x0059AE7C, 4), (0x005A5A24, 4), (0x004C2E14, 4), (0x0059E7B8, 4),
    (0x005A59F8, 4), (0x00822D9C, 4)])

# loadGolfPro 0x0046c970 (cdecl, no arguments): "$cN" applies scene N (open holes, level, money, reputation, flags,
# loadThemeFile answers, pro table).
_GP_STATE = [("$clog", 0, 0x1000)] + _regions([
    (0x0058F338, 0xB7C0), (0x00520A28, 0x19A28), (0x005794B8, 0x9C00), (0x00543D10, 0x19A28), (0x004D6088, 0xA640),
    (0x0051A068, 0x200), (0x00572CAC, 4), (0x0056A51C, 4), (0x004C2C9C, 2), (0x0059E7B8, 4), (0x005A47E0, 4),
    (0x00822D9C, 4)])

# buildTournamentPrep 0x0046d200 (cdecl, no arguments): "$cN" applies scene N (hole records, popup answer, flags,
# tile grid, stampCourseTiles answers).
_TP_STATE = [("$clog", 0, 0x800)] + _regions([
    (0x0051A068, 0x600), (0x00575CB8 - 0x208, 0x208 * 19), (0x005A47E0, 4), (0x0058BCB8, 0x1000), (0x00838EAC, 0x10),
    (0x00822D9C, 4)])

# demolishObject 0x0040a4e0 (cdecl, x, y); the third "int" is the unused stack word "$cK" that writes case K's cell.
# The (x, y) pairs are the cases' own (fixtures.d/c3fb.js DM_CASES, same order).
_DM_XY = [(-1, 5), (12, 50), (10, 10), (10, 11), (10, 12), (10, 13), (10, 14), (11, 14), (11, 15), (11, 16), (11, 17),
          (11, 18), (12, 1), (12, 2), (12, 3), (12, 4), (12, 5), (12, 6), (12, 7), (12, 8), (12, 9), (13, 1), (13, 2),
          (13, 3), (13, 4), (13, 5), (13, 6), (13, 7), (13, 8), (13, 9), (13, 10), (49, 49), (14, 1), (14, 2),
          (14, 3), (14, 4), (14, 5)]
_DM_STATE = [("$clog", 0, 0x800)] + _regions([
    (0x0053CAF0, 0x1388), (0x005830B8, 0x9C4), (0x00575AB0, 0x208 * 33), (0x005685F0, 4), (0x00571FD4, 4),
    (0x00584216 - 20 * 99, 20 * 199 + 2), (0x004C2E04, 4), (0x005A47E4, 4), (0x0051A068, 0x200)])

# OptionsMenuBuild 0x00432720 (cdecl, x, y); "$cK" selects case K (spot, popup answers, prompt answers, typed name).
_OM_N = 50
_OM_STATE = [("$clog", 0, 0x800)] + _regions([
    (0x0051A068, 0x200), (0x00567AFC, 4), (0x004C2848, 4), (0x0053DF54, 4), (0x004C2854, 4), (0x0059B734, 4)])

# showCourseRanking 0x0045f870 (cdecl, no arguments): "$cN" applies scene N (the nine rival records, the player's
# counts, the year word, the date).
_CR_STATE = [("$clog", 0, 0x4000)] + _regions([(0x0051A068, 0x100), (0x00824134, 0x10), (0x005423D0, 0x18)])

# drawEmployeeViewPanel 0x00435760 (cdecl, x, y); "$cK" applies scene K (golfer pattern, layout, scroll, hover state).
_EV_STATE = [("$clog", 0, 0x4000)] + _regions([
    (0x005A9CF0, 0x260), (0x005AA6C8, 4), (0x005A9F54, 4), (0x005AA554, 4), (0x0051A068, 0x80), (0x005A9F6C, 0x10)])

# drawGolferActionPanel 0x004362f0 (cdecl, x, y); "$cK" applies scene K (player golfer, lie class, selected shot,
# hover button and hover history, start-button flags, clubs).
_GA_STATE = [("$clog", 0, 0x2000)] + _regions([(0x005A9F54, 4), (0x005AA554, 4), (0x0051A068, 0x80), (0x005A9F6C, 0x10)])

# panelCreate4a2400 0x004a2400 (thiscall: this, parent, a1, a2, flags; `ret 0x10`). No spare stack word exists, so the
# a1 argument is the getter "$cK": it applies scene K (vbtable offsets, grid, old boxes, callee answers) and answers
# the scene's a1 value.
_PC_STATE = [("$clog", 0, 0x4000), ("$obj", 0, 0x1000), ("$arena", 0, 0x600 * 12), ("$old", 0, 0x40 * 10),
             ("$parent", 0, 0x100)]
_PC_VEC = [("$self", 0 if k == 0 else "$parent", "$c%d" % k, 30 + k, (0x40, 0x0, 0x100000, 0x7, 0x80000000)[k % 5])
           for k in range(11)]

# Flic::open 0x00481f40 (thiscall: this, name, mode, source, flags; `ret 0x10`): the flags argument is the getter
# "$cK" (scene K: the Flic's fields, the pool globals, the callee answers; it answers the scene's flags). The name is
# NULL for the first three scenes (fixtures.d/c3fb.js FO_SCENES), and source / mode follow the scene.
_FO = [(0, 0, 0), (0, 3, 0), (0, 0, 0), (1, 0, 0), (1, 0, 0x55), (1, 0, 0), (1, 2, 0), (1, 0, 0), (1, 1, 0), (1, 6, 0),
       (1, 0, 0), (1, 0, 0), (1, 0, 0x777), (1, 7, 0), (1, 2, 0), (1, 4, 0)]
_FO_STATE = [("$clog", 0, 0x2000), ("$fl", 0, 0x100), ("$arena", 0, 0x800), ("$arena2", 0, 0x800), ("$head", 0, 0x40),
             ("$tail", 0, 0x40), ("$cache", 0, 0x40), ("$cur", 0, 0x40), (0x0083AC88, 0, 0x28), (0x00839650, 0, 4),
             (0x0083AC24, 0, 4)]

HOOKS = {
    "Flic::open": dict(module=EXE, addr=0x00481F40, abi="thiscall", ret="int",
                       args=["pointer", "pointer", "int", "int", "uint32"], fixture="c3fb_flicopen", state=_FO_STATE,
                       vectors=[("$fl", "$nm" if nm else 0, mode, src, "$c%d" % k)
                                for k, (nm, mode, src) in enumerate(_FO)]),
    "panelCreate4a2400": dict(module=EXE, addr=0x004A2400, abi="thiscall", ret="int",
                              args=["pointer", "pointer", "int", "int", "uint32"],
                              fixture="c3fb_pcreate", state=_PC_STATE, vectors=_PC_VEC),
    "drawGolferActionPanel": dict(module=EXE, addr=0x004362F0, abi="default", ret="int", args=["int", "int", "int"],
                                  fixture="c3fb_golfaction", state=_GA_STATE,
                                  vectors=[(250 + 9 * k, 480 - 5 * k, "$c%d" % k) for k in range(22)]),
    "drawEmployeeViewPanel": dict(module=EXE, addr=0x00435760, abi="default", ret="int", args=["int", "int", "int"],
                                  fixture="c3fb_empview", state=_EV_STATE,
                                  vectors=[(300 + 7 * k, 400 - 3 * k, "$c%d" % k) for k in range(11)]),
    "showCourseRanking": dict(module=EXE, addr=0x0045F870, abi="default", ret="int", args=["int"],
                              fixture="c3fb_ranking", state=_CR_STATE,
                              vectors=[("$c%d" % i,) for i in range(8)]),
    "demolishObject": dict(module=EXE, addr=0x0040A4E0, abi="default", ret="int", args=["int", "int", "int"],
                           fixture="c3fb_demolish", state=_DM_STATE,
                           vectors=[(x, y, "$c%d" % k) for k, (x, y) in enumerate(_DM_XY)]),
    "OptionsMenuBuild": dict(module=EXE, addr=0x00432720, abi="default", ret="int", args=["int", "int", "int"],
                             fixture="c3fb_options", state=_OM_STATE,
                             vectors=[(100 + k, 200 - k, "$c%d" % k) for k in range(_OM_N)]),
    "buildTournamentPrep": dict(module=EXE, addr=0x0046D200, abi="default", ret="int", args=["int"],
                                fixture="c3fb_tourprep", state=_TP_STATE,
                                vectors=[("$c%d" % i,) for i in range(6)]),
    "customizeCharacter": dict(module=EXE, addr=0x0040F190, abi="default", ret="int", args=["int", "int"],
                               fixture="c3fb_custom", state=_CC_STATE,
                               vectors=[(w, "$c%d" % i) for i in range(6) for w in (0, 1)] + [(7, "$c3"), (-1, "$c4")]),
    "loadGolfPro": dict(module=EXE, addr=0x0046C970, abi="default", ret="int", args=["int"],
                        fixture="c3fb_golfpro", state=_GP_STATE,
                        vectors=[("$c%d" % i,) for i in range(6)]),
    "buildGolferBioText": dict(module=EXE, addr=0x004315E0, abi="default", ret="int", args=["int"],
                               fixture="c3fb_bio", state=_BIO_STATE,
                               vectors=[("$c%d" % i,) for i in range(8)]),
    "serializeCourse": dict(module=EXE, addr=0x0040AFA0, abi="default", ret="int", args=["int", "int"],
                            fixture="c3fb_serial", state=_SER_STATE,
                            vectors=[(m, "$c%d" % i) for i in range(8) for m in (0, 1)] + [(2, "$c0"), (-1, "$c3")]),
    "showTitleScreen": dict(module=EXE, addr=0x0043CD70, abi="default", ret="int", args=["int"],
                            fixture="c3fb_title", state=_TITLE_STATE,
                            vectors=[("$c%d" % i,) for i in range(10)]),
}

# C3 batch c3fa (round 33, 2026-10-13) of golf_clean.exe: size-only rows under 1 KB (log/c3/c3fa_candidates.tsv).
# Reimplementations: shim/src/re/c3fa.cpp. Fixtures: re/frida/js/fixtures.d/c3fa.js. Purposes and per-jcc tables:
# log/c3/c3fa_purpose.md. Notes, controls and drops: log/c3/c3fa_notes.txt.
#
# Every key freezes the game's first thread and stands every callee not under test in with a recording stub
# (re/frida/js/framemask.js FM.stand, A/B thread only): each call is logged with `this` and its arguments in $fmlog, which
# is compared state. Argument-free functions take one unused cdecl stack word ("int") so a vector can name a getter
# `$rN` that writes that row's globals before diff_hook snapshots the state.

EXE = "golf_clean.exe"
FM = [("$fmlog", 0, 0x4000)]


def _h(addr, fixture, vectors, state, args, ret="int", abi="default"):
    return dict(module=EXE, addr=addr, abi=abi, ret=ret, args=list(args), fixture=fixture, vectors=vectors,
                state=FM + list(state))


def _rows(n, *rest):
    return [(f"$r{i}",) + tuple(rest) for i in range(n)]


HOOKS = {}


def _argrows(n, nargs):
    """Rows built by the fixture's argRows: argument 0 is the row getter $rI, the others $aI_K."""
    return [(f"$r{i}",) + tuple(f"$a{i}_{k}" for k in range(1, nargs)) for i in range(n)]

# showCharacterIntro 0x004065c0: text 0x0051a068, [0x004c2e08], word [0x004c2c9c]; the table it counts is fixture input.
HOOKS["showCharacterIntro"] = _h(0x004065C0, "c3fa_sci", _rows(12),
                                 [(0x0051A068, 0, 0x200), (0x004C2E08, 0, 4), (0x004C2C9C, 0, 2), (0x004C15A0, 0, 0x420)],
                                 ["int"])

# showPreferencesMenu 0x00432560: text 0x0051a068, [0x005a5a00]; the six string pointers 0x004c2ca8 are fixture input.
HOOKS["showPreferencesMenu"] = _h(0x00432560, "c3fa_spm", _rows(10),
                                  [(0x0051A068, 0, 0x400), (0x005A5A00, 0, 4), (0x004C2CA8, 0, 0x18)], ["int"])

# saveThumbAndFull 0x00431ee0 (path): the ten path buffers.
HOOKS["saveThumbAndFull"] = _h(0x00431EE0, "c3fa_stf", _rows(10), [("$paths", 0, 0xA00)], ["pointer"])

# clickEmployeeView 0x00435680 (x, y)
_CEV = [(0x00561254, 0, 4), (0x0053DF54, 0, 4), (0x004C2BA0, 0, 8), (0x00567AFC, 0, 4), (0x0059CA54, 0, 4),
        (0x005AA6C8, 0, 4)]
HOOKS["clickEmployeeView"] = _h(0x00435680, "c3fa_cev", _rows(17, 0x77), _CEV, ["int", "int"])

# handleEmployeeAction 0x00436c00 (x, y)
_HEA = [(0x00561254, 0, 4), (0x00567AFC, 0, 4), (0x004C7AEC, 0, 4), (0x0059CA54, 0, 4), (0x005AA558, 0, 4),
        (0x004C2E10, 0, 4), (0x00571FD4, 0, 4), (0x004C2BA0, 0, 8), (0x0051A068, 0, 0x40), (0x0058421A, 0, 0x7D0)]
_HEA += [(0x00585850 + g * 0x4C, 0, 0x4C) for g in (2, 7, 11)]
HOOKS["handleEmployeeAction"] = _h(0x00436C00, "c3fa_hea", _rows(26, -5), _HEA, ["int", "int"])

# clickGolferPanel 0x00436060 (x, y)
_CGP = [(0x00561254, 0, 4), (0x0059E7B8, 0, 4), (0x00582CD0, 0, 2), (0x00582D6E, 0, 2), (0x0058F330, 0, 4),
        (0x00567AFC, 0, 4), (0x0059CA54, 0, 4)]
HOOKS["clickGolferPanel"] = _h(0x00436060, "c3fa_cgp", _rows(29, 0x66), _CGP, ["int", "int"])

# panelDispatch 0x004a08a0 (thiscall: kind, a, b, c, d): this+0x1f4 and the five stood-in builders.
_PD_KINDS = list(range(0, 34)) + [0x41, 0x48, 0x50, 0xFFFFFFE1, 0x7FFFFFFF, 0x80000002]
HOOKS["panelDispatch"] = _h(0x004A08A0, "c3fa_pd",
                            [("$obj", k, 0x10 + i, -i, 0x1000 * i, 7 - i) for i, k in enumerate(_PD_KINDS)],
                            [("$obj", 0, 0x800)], ["pointer", "uint32", "int", "int", "int", "int"], abi="thiscall")

# decodeImageChunks 0x00482570 (thiscall: chunk, image, dst)
_DIC = [("$obj", f"$t{t}", "$i0", d) for t in ("0", "3", "4", "5", "6", "7", "8", "a", "b", "c", "d", "e", "f", "10",
                                              "11", "f1f9", "f1fb", "ffff") for d in (0, 0x5000)]
_DIC += [("$obj", f"$f{n}", f"$i{i}", d) for n in (0, 1, 3, 5) for i in range(4) for d in (0, 0x6000)]
# engineInit 0x004855b0 (module, hwnd, flags): the globals it writes; [0x0083ad50] holds the guarded fixture display.
HOOKS["engineInit"] = _h(0x004855B0, "c3fa_ei", _argrows(23, 3),
                         [(0x0083AFD0, 0, 4), (0x0083AD50, 0, 4), (0x0083AD0C, 0, 4), (0x0083AB68, 0, 4)],
                         ["int", "int", "uint32"])

# buildLabeledControl 0x00496030 (thiscall, no arguments): the object.
HOOKS["buildLabeledControl"] = _h(0x00496030, "c3fa_blc", _rows(10), [("$obj", 0, 0x1400)], ["pointer"],
                                  ret="pointer", abi="thiscall")

# Window::createBackingSurface 0x00480b30 (thiscall: w, h, keep, a3)
HOOKS["Window::createBackingSurface"] = _h(0x00480B30, "c3fa_cbs", _argrows(12, 5), [(0x00839AB0, 0, 4), ("$obj", 0, 0x400)],
                                           ["pointer", "int", "int", "int", "int"], abi="thiscall")

# saveCourseJpg 0x00431d20 (path, surf, x, y, w, h, scale)
HOOKS["saveCourseJpg"] = _h(0x00431D20, "c3fa_scj", _argrows(23, 7),
                            [(0x0056125C, 0, 4), (0x004C3074, 0, 4), (0x00543CF0, 0, 4), (0x0056A510, 0, 4)],
                            ["pointer", "pointer", "int", "int", "int", "int", "int"])

# movieAdvanceFrame 0x0049c940 (thiscall, no arguments): the movie object, the sink's buffers, the mmio import log.
HOOKS["movieAdvanceFrame"] = _h(0x0049C940, "c3fa_maf", _argrows(14, 1),
                                [("$obj", 0, 0x960), ("$bufs", 0, 0x40), ("$bufs2", 0, 0x40), ("$implog", 0, 0x400)],
                                ["pointer"], abi="thiscall")

# deserializeCourse 0x0040bbf0 (mode): [0x0053e638]; the 57 field calls are in $fmlog.
HOOKS["deserializeCourse"] = _h(0x0040BBF0, "c3fa_dsc", _rows(10), [(0x0053E638, 0, 4)], ["int"])

# resetAllPanels 0x0044cce0 (no arguments): every releaseBacking / Surface_free `this` is in $fmlog.
HOOKS["resetAllPanels"] = _h(0x0044CCE0, "c3fa_rap", _rows(10), [], ["int"])

# hireEmployee 0x0040aa80 (no arguments): text 0x0051a068 and the three walker records the stood-in spawnWalker names.
HOOKS["hireEmployee"] = _h(0x0040AA80, "c3fa_hire", _rows(15),
                           [(0x0051A068, 0, 0x40)] + [(0x00585850 + 0x4C * g, 0, 0x4C) for g in (1, 4, 9)], ["int"])

# Flic::buildFrameTable 0x00481ca0 (thiscall: flags): the Flic object, the copy buffer, the header, GetFileSize's log.
HOOKS["Flic::buildFrameTable"] = _h(0x00481CA0, "c3fa_fbt", _argrows(12, 2),
                                    [("$obj", 0, 0x80), ("$dst", 0, 0x100), ("$hdr", 0, 0x100), ("$implog", 0, 0x400)],
                                    ["pointer", "uint32"], abi="thiscall")

# saveGameDialog 0x00405b10 (no arguments): text 0x0051a068 and __itoa's buffer 0x0058a528 (__itoa runs real).
HOOKS["saveGameDialog"] = _h(0x00405B10, "c3fa_sgd", _rows(12), [(0x0051A068, 0, 0x100), (0x0058A528, 0, 0x20)], ["int"])

HOOKS["decodeImageChunks"] = _h(0x00482570, "c3fa_dic", _DIC, [], ["pointer", "pointer", "pointer", "int"],
                                abi="thiscall")

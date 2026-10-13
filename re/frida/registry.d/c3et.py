# C3 batch c3et (round 32, 2026-10-13) of golf_clean.exe: the large screens and round 31's drops.
# Reimplementations: shim/src/re/c3et.cpp. Fixtures: re/frida/js/fixtures.d/c3et.js.
# Purposes and per-jcc tables: log/c3/c3et_purpose.md. Notes and drops: log/c3/c3et_notes.txt.

EXE = "golf_clean.exe"

# re/frida/js/uitree.js: GLOBALS (4 bytes each, same order), the z-list array, the recorder size
UI_GLOBALS = [0x0083AB94, 0x0083AB90, 0x0083AAE0, 0x0083AAEC, 0x0083AAA0, 0x0083AABC, 0x0083AAC0, 0x0083AAC4, 0x0083AA98,
              0x0083AAF0, 0x0083AB30, 0x0083AB34, 0x0083AB40, 0x0083AB44, 0x0083AB54, 0x0083AC18, 0x00839650, 0x0083AB2C,
              0x0083AA9C, 0x0083A2C8, 0x0083A2CC, 0x0083A2D0, 0x0083A2D4, 0x0083AD50, 0x0083AD4C]
REC_SIZE = 0x1000


def UI_STATE(arena):
    return ([("$heap", 0, 0x10 + arena), ("$freelog", 0, 0x404), ("$implog", 0, 0x400), ("$rec", 0, REC_SIZE)]
            + [(g, 0, 4) for g in UI_GLOBALS] + [(0x0083A2D8, 0, 0x200)])


HOOKS = {}

# ---------------------------------------------------------------------------------------------------- allocTextBuffer
# thiscall (this, text, x, y, len), `ret 0x10`. Twenty scenes (re/frida/js/fixtures.d/c3et.js TB_SCENES): the getter of
# $oN writes the scene into the fixture's text object and controls before diff_hook's snapshot. The arena (malloc /
# free through uitree's heap stand-in) must match the fixture's arenaSize 0x2000.
HOOKS["allocTextBuffer"] = dict(
    module=EXE, addr=0x004767A0, abi="thiscall", ret="int",
    args=["pointer", "pointer", "int", "int", "int"], fixture="c3et_textrun",
    state=UI_STATE(0x2000) + [("$tlog", 0, 0x2000)],
    vectors=[(f"$o{i}", f"$t{i}", f"$x{i}", f"$y{i}", f"$l{i}") for i in range(21)])

# re/frida/js/modal.js's block and the input globals its models and handlers write (__modal.STATE; the same list
# re/frida/registry.d/c3el.py defines)
MODAL_STATE = [("$modal", 0, 0x1000), (0x00822B98, 0, 8), (0x00822D58, 0, 0x40), (0x004D3834, 0, 4),
               (0x0083AD4C, 0, 4), (0x0083AFCC, 0, 4), (0x00838208, 0, 4), (0x0083821C, 0, 4), (0x0083AB9C, 0, 4)]

# ------------------------------------------------------------------------------------------------ showGolferPairSelect
# __cdecl, no arguments: one vector is one whole run; the unused cdecl stack word "$gN" is resolved before the snapshot,
# and the getter of fx.gN applies scene N (golfer and type records, the start and bound globals, the modal script and
# the View_toLocal cursor script).
HOOKS["showGolferPairSelect"] = dict(
    module=EXE, addr=0x00459850, abi="default", ret="int", args=["int"], fixture="c3et_pairsel",
    state=MODAL_STATE + [
        ("$clog", 0, 0x4000),      # the recording stand-ins' call log (every drawn string, image and the pairing call)
        (0x005794B8, 0, 0x9900),   # the 0x98 golfer records swapTableEntry swaps and the +0x10 flags, plus its scratch
        (0x0051A068, 0, 0x200),    # the shared text buffer
        (0x00821C50, 0, 0x40),     # the itoa scratch (0x00459ce5)
    ],
    vectors=[(f"$g{i}",) for i in range(12)])

# ------------------------------------------------------------------------------------------- LabelButton::drawContents
# thiscall (this), `ret`; vtable slot 22 (+0x58) of 0x004bb5b4. Fifteen scenes (fixture LB_SCENES): the getter of $bN
# writes the scene into the fixture object and returns it. Returns the eax of the last call on each path.
HOOKS["LabelButton::drawContents"] = dict(
    module=EXE, addr=0x004A1580, abi="thiscall", ret="int", args=["pointer"], fixture="c3et_labelbtn",
    state=[
        ("$clog", 0, 0x4000),      # the recording stand-ins (every drawing callee, with the rect words)
        ("$tlog", 0, 0x2000),      # the recording surface, owner and display slots
        (0x0083AB2C, 0, 4),        # the owner copy written at 0x004a15a9
    ],
    vectors=[(f"$b{i}",) for i in range(17)])

# ------------------------------------------------------------------------------------------------------ TextView::build
# thiscall (this, quiet, modalArg), `ret 8`. Seventeen scenes (fixture TV_SCENES): the getter of $vN writes the scene
# into the fixture TextView and the globals, $qN / $mN are its two arguments. The heap stand-in's arena is 0x4000.
HOOKS["TextView::build"] = dict(
    module=EXE, addr=0x0048E900, abi="thiscall", ret="int", args=["pointer", "int", "int"], fixture="c3et_tvbuild",
    state=[
        ("$tv", 0, 0x2600),        # the TextView block: every field the function writes, the two button parts
        ("$heap", 0, 0x4010),      # the heap stand-in arena: the list buttons and the caption strings
        ("$freelog", 0, 0x404),    # the frees the stand-in recorded
        ("$clog", 0, 0x4000),      # the recording stand-ins
        ("$tlog", 0, 0x2000),      # the recording vtables, surfaces, display and the callback
        (0x0083B8F0, 0, 4),        # the open-dialog count (0x0048fd32, 0x0048fdb7)
        (0x0083B990, 0, 4),        # the current dialog, saved and restored around the modal run
        (0x0083B9E4, 0, 8),        # the last dialog position (0x0048fd87 / 0x0048fd92)
        (0x0083B8EC, 0, 4),        # cleared when the count reaches 0 (0x0048fdbe)
    ],
    vectors=[(f"$v{i}", f"$q{i}", f"$m{i}") for i in range(27)])

# ------------------------------------------------------------------------------------------------ showRoutingMapScreen
# __cdecl, no arguments: one vector is one whole run (the dummy cdecl word "$rN" is resolved before the snapshot; its
# getter applies scene N: the world, the mode, the modal script and the View_toLocal cursor list). Every region the
# function or its real callees write is state; the regions the scene only seeds for reading are saved by the fixture.
HOOKS["showRoutingMapScreen"] = dict(
    module=EXE, addr=0x00456BE0, abi="default", ret="int", args=["int"], fixture="c3et_routing",
    state=MODAL_STATE + [
        ("$clog", 0, 0x10000),     # the recording stand-ins (every diamond, line, sprite, string and image)
        ("$tlog", 0, 0x2000),      # the display's slot +0x9c
        (0x00575AB0, 0, 0x2698),   # the hole records 0..18 (record 0 is the move's scratch)
        (0x0059AE80, 0, 0x1C8),    # the waypoint records 0..18
        (0x0053CAF0, 0, 0x1388),   # the tile -> hole word map
        (0x005794B8, 0, 0x9800),   # the golfers (their hole byte +0x21 is remapped at the exit)
        (0x00585850, 0, 0x1300),   # the walkers (+0x1e is rewritten 0xc -> 0xb at 0x00457aab)
        (0x0051A068, 0, 0x200),    # the shared text buffer
        (0x00821C50, 0, 0x40),     # the __itoa scratch
        (0x005A6D3C, 0, 4),        # appendDate's year word
        (0x005A412C, 0, 8),        # setLinePoint's point
        (0x00822B84, 0, 4),        # the last cell
        (0x004D20DC, 0, 4),        # the mode (the buttons set it, 0x00458330)
        (0x005685F0, 0, 4),        # the first free hole (0x004586fd / 0x0045872a)
    ],
    vectors=[(f"$r{i}",) for i in range(13)])

# ---------------------------------------------------------------------------------------------- negative controls
# The keys of the negctl build (log/c3/alt/c3et_negctl_build.ps1 negctl): vector j selects wrong detail NEG_x[j][0]
# through word 2 of the exported C3etNegCtl switch and runs scene NEG_x[j][1] (the same tables as the fixture file).
# Registered only for log/c3/alt/c3et_ctl.py, which sets C3ET_CONTROLS=1, so a normal run never sees them.
import os  # noqa: E402

if os.environ.get("C3ET_CONTROLS") == "1":
    NEG_TB = [(1, 0), (2, 5), (3, 5), (4, 2), (5, 8), (6, 0), (7, 6), (8, 11), (9, 8)]
    NEG_GP = [(10, 9), (11, 2), (12, 2), (13, 4), (14, 1)]
    NEG_LB = [(15, 3), (16, 3), (17, 6), (18, 3)]
    NEG_TV = [(19, 0), (20, 21), (21, 0), (22, 23), (23, 0), (24, 0)]
    NEG_RM = [(25, 0), (26, 2), (27, 6), (28, 3), (29, 3), (30, 4), (31, 9), (32, 3), (33, 11), (34, 4), (35, 1), (36, 8)]
    _neg = {
        "showRoutingMapScreen_neg": ("showRoutingMapScreen", [(f"$n{j}",) for j in range(len(NEG_RM))]),
        "allocTextBuffer_neg": ("allocTextBuffer",
                                [(f"$n{j}", f"$t{s}", f"$x{s}", f"$y{s}", f"$l{s}") for j, (_e, s) in enumerate(NEG_TB)]),
        "showGolferPairSelect_neg": ("showGolferPairSelect", [(f"$n{j}",) for j in range(len(NEG_GP))]),
        "LabelButton::drawContents_neg": ("LabelButton::drawContents", [(f"$n{j}",) for j in range(len(NEG_LB))]),
        "TextView::build_neg": ("TextView::build",
                                [(f"$n{j}", f"$q{s}", f"$m{s}") for j, (_e, s) in enumerate(NEG_TV)]),
    }
    for _j in range(len(HOOKS["showRoutingMapScreen"]["vectors"])):   # one key per scene (debugging)
        HOOKS[f"showRoutingMapScreen_v{_j}"] = dict(HOOKS["showRoutingMapScreen"], vectors=[(f"$r{_j}",)])
        HOOKS[f"showRoutingMapScreen_upto{_j}"] = dict(HOOKS["showRoutingMapScreen"],
                                                       vectors=[(f"$r{_i}",) for _i in range(_j + 1)])
    for _k, (_base, _vecs) in _neg.items():
        HOOKS[_k] = dict(HOOKS[_base], vectors=_vecs)
        # one key per edit as well, so an edit whose wrong body faults is an ERROR of its own key only
        for _j, _v in enumerate(_vecs):
            HOOKS[f"{_k}{_j}"] = dict(HOOKS[_base], vectors=[_v])

# C3 batch c3fr (round 35, 2026-10-14) of golf_clean.exe: the large screens round 34 left.
# Reimplementations: shim/src/re/c3fr.cpp. Fixtures: re/frida/js/fixtures.d/c3fr.js.
# Purposes and per-jcc tables: log/c3/c3fr_purpose.md. Notes and drops: log/c3/c3fr_notes.txt.
import os

EXE = "golf_clean.exe"

# re/frida/js/modal.js's block and the input globals its models and handlers write (__modal.STATE; the same list
# re/frida/registry.d/c3el.py defines)
MODAL_STATE = [("$modal", 0, 0x1000), (0x00822B98, 0, 8), (0x00822D58, 0, 0x40), (0x004D3834, 0, 4),
               (0x0083AD4C, 0, 4), (0x0083AFCC, 0, 4), (0x00838208, 0, 4), (0x0083821C, 0, 4), (0x0083AB9C, 0, 4)]

HOOKS = {}

# --------------------------------------------------------------------------------------------------- showBuyLandScreen
# __cdecl, no arguments: one vector is one whole run (the dummy cdecl word "$bN" is resolved before the snapshot; its
# getter applies scene N: the world, the modal script and the View_toLocal cursor list). Every region the function or
# its real callees write is state; the regions the scene only seeds for reading are saved by the fixture.
BL_N = 10
HOOKS["showBuyLandScreen"] = dict(
    module=EXE, addr=0x004587A0, abi="default", ret="int", args=["int"], fixture="c3fr_buyland",
    state=MODAL_STATE + [
        ("$clog", 0, 0x10000),     # the recording stand-ins (every image, line, sprite, string, the cost asked)
        (0x005722E8, 0, 2500),     # the tile map (a purchase turns the lot's 0x14 tiles into their types)
        (0x00571FD4, 0, 4),        # the money (0x0045933b)
        (0x0058421E, 0, 2000),     # the per-year words 0x0058421e + 20 y (0x00459349)
        (0x00568600, 0, 1000),     # logTick's ring (0x004593d1)
        (0x00822D9C, 0, 4),        # the RNG seed (Random::range per tile for sale, 0x00458c6e)
        (0x00822B80, 0, 8),        # the map-only flag and the last cell (0x00458827 / 0x00458842, 0x004588d5)
        (0x0051A068, 0, 0x200),    # the shared text buffer
        (0x00821C50, 0, 0x40),     # the __itoa scratch
        (0x005A412C, 0, 8),        # setLinePoint's point
        (0x0058A528, 0, 0x40),     # appendNumber's scratch
    ],
    vectors=[(f"$b{i}",) for i in range(BL_N)])

# --------------------------------------------------------------------------------------------------------- narrateShot
# __cdecl (shot, dist, yards, flag), `ret` at four exits, each returning the eax the original leaves (compared). "$sN"
# resolves first: its getter writes scene N's world (shot, golfer, snapshot, type and tile records, the tables and
# globals) and returns the shot index; $dN / $yN / $fN are the scene's other three arguments.
NS_N = 68
HOOKS["narrateShot"] = dict(
    module=EXE, addr=0x00407E00, abi="default", ret="int", args=["int", "int", "int", "int"], fixture="c3fr_narrate",
    state=[
        ("$clog", 0, 0x8000),      # the recording stand-ins (every drawn box, string, sound and the story call)
        (0x0051A068, 0, 0x200),    # the shared text buffer (the sentence, then the SimFoto date)
        (0x0058A528, 0, 0x40),     # the __itoa scratch
        (0x005794B8, 0, 0x9A00),   # golfer records 0..0x99 (the two snapshots 0x98 / 0x99, masks, skills, flags)
        (0x005849E0, 0, 0xD40),    # the hole-flag table 0x005849e3 + 0x2c t + h
        (0x005A5A00, 0, 0x24),     # the flag byte and the two skill tables
        (0x004C2C9C, 0, 4),        # the followed golfer's new-skill bits (0x004090bd)
        (0x004C2E08, 0, 4),        # the window's golfer (0x0040913d / 0x00409176)
        (0x00822D9C, 0, 4),        # the RNG seed (Random::range 0x00408a24)
        (0x005A412C, 0, 8),        # setLinePoint's point
        (0x00822D94, 0, 4),        # the key pumpAndRedraw's stand-in leaves
    ],
    vectors=[(f"$s{i}", f"$d{i}", f"$y{i}", f"$f{i}") for i in range(NS_N)])

# ---------------------------------------------------------------------------------------------------- GolferPanelEject
# __cdecl (g, mode), `ret`; its eax is dead in mainLoop (both call sites, see shim/src/re/c3fr.cpp), so ret is void and
# the comparison is the call log and memory. "$pN" applies scene N and returns g; "$mN" is the scene's mode.
GP_N = 19
HOOKS["GolferPanelEject"] = dict(
    module=EXE, addr=0x0045C560, abi="default", ret="void", args=["int", "int"], fixture="c3fr_panel",
    state=[
        ("$clog", 0, 0x8000),      # the recording stand-ins (every string drawn, button, meter band, tooltip)
        (0x0051A068, 0, 0x200),    # the shared text buffer
        (0x00824134, 0, 0x40),     # the __itoa scratch
        (0x005794B8, 0, 0xA00),    # golfer records 0..9 (+0x5d + hole gets the mood byte, 0x0045cf67)
        (0x00824130, 0, 4),        # the hovered button
        (0x00823768, 0, 4),        # its frame counter
    ],
    vectors=[(f"$p{i}", f"$m{i}") for i in range(GP_N)])

# ---------------------------------------------------------------------------------------------------- showCourseReport
# __cdecl (mode), `ret`; the eax it leaves is the last callee's (pumpLight's, or Image_draw's for mode 2) and its only
# caller mainLoop does not use it (log/c3/c3fr_purpose.md), so ret is void. "$cN" applies scene N and returns its mode.
CR_N = 22
HOOKS["showCourseReport"] = dict(
    module=EXE, addr=0x0044FB30, abi="default", ret="void", args=["int"], fixture="c3fr_report",
    state=MODAL_STATE + [
        ("$clog", 0, 0x10000),     # the recording stand-ins (every string, image, band, event) and the display slot
        (0x0051A068, 0, 0x200),    # the shared text buffer
        (0x00821C50, 0, 0x40),     # the __itoa scratch
        (0x0058A528, 0, 0x40),     # appendNumber's scratch
        (0x0059E7B8, 0, 4),        # the game flags (bits 0x400000 / 0x2000 written at 0x00452d6f / 0x00452fc1)
        (0x00561250, 0, 4),        # the course score (0x00452dc1)
        (0x00567B04, 0, 4),        # the first prize (0x00452f32)
        (0x00569498, 0, 4),        # the byte cleared at the exit (0x004531ff)
    ],
    vectors=[(f"$c{i}",) for i in range(CR_N)])

# ---------------------------------------------------------------------------------------------- negative controls
# The keys of the negctl build (log/c3/alt/c3fr_build.txt `negctl`): vector j selects wrong detail NEG_x[j][0] through
# word 2 of the exported C3frNegCtl switch and runs scene NEG_x[j][1] (the same tables as the fixture file). Registered
# only for log/c3/alt/c3fr_ctl.py, which sets C3FR_CONTROLS=1, so a normal run never sees them.
if os.environ.get("C3FR_CONTROLS") == "1":
    NEG_BL = [(1, 0), (2, 0), (3, 0), (4, 0), (5, 0), (6, 0), (7, 0), (8, 4), (9, 6), (10, 0), (11, 2), (12, 3),
              (13, 5), (14, 5), (15, 3)]
    NEG_GP = [(36, 0), (37, 3), (38, 0), (39, 0), (40, 0), (41, 9), (42, 9), (43, 1), (44, 3), (45, 1), (46, 1),
              (47, 18), (48, 4), (49, 4), (50, 0), (51, 17), (52, 0), (53, 10), (54, 6)]
    NEG_NS = [(16, 5), (17, 0), (18, 38), (19, 2), (20, 35), (21, 32), (22, 32), (23, 46), (24, 28), (25, 23),
              (26, 14), (27, 66), (28, 3), (29, 0), (30, 3), (31, 0), (32, 0), (33, 67), (34, 4), (35, 23)]
    NEG_CR = [(55, 0), (56, 0), (57, 0), (58, 0), (59, 0), (60, 0), (61, 6), (62, 10), (63, 21), (64, 21), (65, 21),
              (66, 21), (67, 10), (68, 10), (69, 10), (70, 10), (71, 0), (72, 0), (73, 10), (74, 7), (75, 0)]
    _neg = {
        "showCourseReport_neg": ("showCourseReport", [(f"$n{j}",) for j in range(len(NEG_CR))]),
        "showBuyLandScreen_neg": ("showBuyLandScreen", [(f"$n{j}",) for j in range(len(NEG_BL))]),
        "narrateShot_neg": ("narrateShot", [(f"$n{j}", f"$d{s}", f"$y{s}", f"$f{s}") for j, (_e, s) in enumerate(NEG_NS)]),
        "GolferPanelEject_neg": ("GolferPanelEject", [(f"$n{j}", f"$m{s}") for j, (_e, s) in enumerate(NEG_GP)]),
    }
    for _j in range(BL_N):   # one key per scene (debugging)
        HOOKS[f"showBuyLandScreen_v{_j}"] = dict(HOOKS["showBuyLandScreen"], vectors=[(f"$b{_j}",)])
    for _j in range(NS_N):
        HOOKS[f"narrateShot_v{_j}"] = dict(HOOKS["narrateShot"], vectors=[HOOKS["narrateShot"]["vectors"][_j]])
    for _j in range(GP_N):
        HOOKS[f"GolferPanelEject_v{_j}"] = dict(HOOKS["GolferPanelEject"], vectors=[HOOKS["GolferPanelEject"]["vectors"][_j]])
    for _j in range(CR_N):
        HOOKS[f"showCourseReport_v{_j}"] = dict(HOOKS["showCourseReport"], vectors=[HOOKS["showCourseReport"]["vectors"][_j]])
    for _k, (_base, _vecs) in _neg.items():
        if not _vecs:
            continue
        HOOKS[_k] = dict(HOOKS[_base], vectors=_vecs)
        for _j, _v in enumerate(_vecs):
            HOOKS[f"{_k}{_j}"] = dict(HOOKS[_base], vectors=[_v])

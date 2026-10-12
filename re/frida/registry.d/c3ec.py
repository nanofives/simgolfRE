# C3 batch c3ec (simulation plan phase 4, 2026-10-11): updateGolfers 0x004289e0, the per-tick golfer state machine.
# Reimplementation: shim/src/re/c3ec.cpp. Fixtures: re/frida/js/fixtures.d/c3ec.js. Purpose and per-jcc table:
# log/c3/c3ec_purpose.md.
#
# One vector is one whole call over the 152 golfer records: "$rN" applies scene N (a synthetic course, the golfer records,
# the scalar globals, the scripted Random::range values). Every key freezes the game's first thread and stands the drawing,
# sound, message, dialog, shot and per-golfer event callees in with recording stubs (call logs $clog / $clog2: counts, a
# running hash and the first entries). skillPointDialog 0x0045f0f0 (C2) is always a recording stub. One key per state family
# of the machine; updateGolfers_real also runs pathStep, updatePairSnapshot, addGolferPair and resetGolfer for real.
# updateGolfers takes no argument: the "$rN" argument is an unused cdecl stack word that only applies the scene.

EXE = "golf_clean.exe"


def _h(fixture, n, state):
    return dict(module=EXE, addr=0x004289E0, abi="default", ret="int", args=["int"], fixture=fixture, state=state,
                vectors=[(f"$r{i}",) for i in range(n)])


_STATE = [
    ("$clog", 0, 0x4000),
    ("$clog2", 0, 0x2000),
    ("$rc", 0, 0x200),           # the scripted Random::range counters
    (0x005794B8, 0, 152 * 0x100),   # golfer records
    (0x0059FC60, 0, 0x3880),        # story pairs
    (0x0051A068, 0, 0x200),         # text buffer
    (0x0058A528, 0, 0x20),          # _itoa buffer
    (0x005849E0, 0, 0x2C * 8),      # golfer type membership records
    (0x00575AB0, 0, 0x208 * 20),    # hole records (statistics words)
    (0x0056AE70, 0, 0x1A00),        # per-player statistics
    (0x00584214, 0, 0x7E0),         # income statistics
    (0x0056962C, 0, 152 * 4),       # shot start dates
    (0x0053EA24, 0, 2500),          # ball-stop counts per tile
    (0x00578804, 0, 2500),
    (0x0053CAF0, 0, 5000),          # tile flag words
    (0x00585850, 0, 0x18),          # walker 0 (the followed golfer)
    (0x0059B73C, 0, 4), (0x004C2E0C, 0, 4), (0x00569498, 0, 4), (0x005A59F8, 0, 4), (0x00571FD4, 0, 4),
    (0x004C2BA0, 0, 8), (0x0058F330, 0, 4), (0x005A34E4, 0, 4), (0x00567AFC, 0, 4), (0x00561254, 0, 4),
    (0x005787FC, 0, 8), (0x004C2E28, 0, 4), (0x0059E7B8, 0, 4), (0x00822D9C, 0, 4), (0x00568D0C, 0, 4),
    (0x0056A91C, 0, 8), (0x00541318, 0, 4),
]
_REAL = _STATE + [
    (0x005689E8, 0, 0x320),         # resetGolfer's type slots
    (0x0058D370, 0, 2500), (0x005A7404, 0, 0x800), (0x005A8028, 0, 0x800),   # pathStep's grid and queues
]

HOOKS = {}
HOOKS["updateGolfers_tick"] = _h("c3ec_tick", 60, _STATE)
HOOKS["updateGolfers_leave"] = _h("c3ec_leave", 60, _STATE)
HOOKS["updateGolfers_walk"] = _h("c3ec_walk", 80, _STATE)
HOOKS["updateGolfers_shot"] = _h("c3ec_shot", 60, _STATE)
HOOKS["updateGolfers_swing"] = _h("c3ec_swing", 60, _STATE)
HOOKS["updateGolfers_flight"] = _h("c3ec_flight", 80, _STATE)
HOOKS["updateGolfers_stop"] = _h("c3ec_stop", 80, _STATE)
HOOKS["updateGolfers_draw"] = _h("c3ec_draw", 40, _STATE)
HOOKS["updateGolfers_real"] = _h("c3ec_real", 60, _REAL)
HOOKS["updateGolfers_edge"] = _h("c3ec_edge", 216, _STATE)    # targeted scenes, 18 builders x 12 variants
HOOKS["updateGolfers_edge2"] = _h("c3ec_edge2", 224, _STATE + [(0x005756A0, 0, 0x20)])   # second round, 14 builders x 16 variants (hole "-2" at 0x005756a0)
HOOKS["updateGolfers_edge3"] = _h("c3ec_edge3", 220, _STATE)    # third round (mutation survivors)

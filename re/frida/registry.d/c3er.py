import os

# C3 batch c3er (round 31, 2026-10-13): the end-of-career screens of golf_clean.exe.
# Reimplementations: shim/src/re/c3er.cpp. Fixtures: re/frida/js/fixtures.d/c3er.js.
# Purposes and per-jcc tables: log/c3/c3er_purpose.md. Notes and drops: log/c3/c3er_notes.txt.

EXE = "golf_clean.exe"

# drawScoreSummary 0x00473470 is __cdecl with no arguments, so one vector is one whole call over the ten scoreboard
# rows. The single "int" argument is an unused cdecl stack word the caller pops: diff_hook resolves "$sN" before the
# state snapshot, so the getter of fx.sN applies scene N (re/frida/js/fixtures.d/c3er.js).
_SS_STATE = [
    ("$clog", 0, 0x4000),      # the recording stand-ins' call log (counts, FNV-1a hash, entries with the drawn text)
    (0x00541CE0, 0, 0x618),    # the ten 0x9c scoreboard rows addScore rewrites
    (0x0051A068, 0, 0x200),    # the shared text buffer (0x004736cb, 0x00473873, ...)
    (0x008392A8, 0, 0x48),     # the selected row (0x008392a8) and this function's itoa scratch (0x008392ac)
    (0x0058A528, 0, 0x20),     # appendCents' itoa scratch
    (0x00569628, 0, 4),        # the value appendCents stores (0x0042dd56)
]

# CareerRetirementFlow 0x0044cff0: also __cdecl with no arguments; one vector is one whole run of the end-of-year
# screen (the scene, the modal input script and the View_toLocal cursor script are applied by the getter of fx.cN).
# MODAL_STATE is re/frida/js/modal.js's block and the input globals its models and handlers write (__modal.STATE,
# the same list re/frida/registry.d/c3el.py defines).
_MODAL_STATE = [
    ("$modal", 0, 0x1000),
    (0x00822B98, 0, 8),
    (0x00822D58, 0, 0x40),
    (0x004D3834, 0, 4),
    (0x0083AD4C, 0, 4),
    (0x0083AFCC, 0, 4),
    (0x00838208, 0, 4),
    (0x0083821C, 0, 4),
    (0x0083AB9C, 0, 4),
]

_CR_STATE = _MODAL_STATE + [
    ("$clog", 0, 0x4000),      # the recording stand-ins' call log (every drawn string, blit, image and sound call)
    (0x0051A068, 0, 0x200),    # the shared text buffer
    (0x00821C50, 0, 0x40),     # this screen's itoa scratch (0x0044d2af)
    (0x0058A528, 0, 0x20),     # appendNumber / appendCents scratch
    (0x00569628, 0, 4),        # the value appendCents stores
    (0x00520640, 0, 0x3E8),    # the membership row the screen writes (0x0044d913)
    (0x005A6374, 0, 4),        # the board's warning counter (0x0044e064 / 0x0044e392)
    (0x00541CE0, 0, 0x618),    # the scoreboard rows addScore rewrites at the end of the loop
    (0x008392A8, 0, 0x48),     # the row addScore selects, and drawScoreSummary's itoa scratch
]

HOOKS = {
    "drawScoreSummary": dict(module=EXE, addr=0x00473470, abi="default", ret="void", args=["int"],
                             fixture="c3er_scores", state=_SS_STATE,
                             vectors=[("$s%d" % i,) for i in range(12)]),

    "CareerRetirementFlow": dict(module=EXE, addr=0x0044CFF0, abi="default", ret="void", args=["int"],
                                 fixture="c3er_career", state=_CR_STATE,
                                 vectors=[("$c%d" % i,) for i in range(14)]),
}

# The negative-control keys of the negctl build (log/c3/alt/c3er_negctl_build.ps1 negctl): one wrong detail per vector,
# picked through word 2 of the exported C3erNegCtl switch. Registered only for log/c3/alt/c3er_ctl.py, which sets
# C3ER_CONTROLS=1, so a normal run never sees them.
if os.environ.get("C3ER_CONTROLS") == "1":
    HOOKS["drawScoreSummary_neg"] = dict(module=EXE, addr=0x00473470, abi="default", ret="void", args=["int"],
                                         fixture="c3er_scores_neg", state=_SS_STATE,
                                         vectors=[("$n%d" % i,) for i in range(5)])
    HOOKS["CareerRetirementFlow_neg"] = dict(module=EXE, addr=0x0044CFF0, abi="default", ret="void", args=["int"],
                                             fixture="c3er_career_neg", state=_CR_STATE,
                                             vectors=[("$m%d" % i,) for i in range(4)])

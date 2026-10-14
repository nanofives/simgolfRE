import os

# C3 batch c3fj (round 34, 2026-10-13): golf_clean.exe size-only rows over 2 KB round 33 left, one at a time.
# Reimplementations: shim/src/re/c3fj.cpp. Fixtures: re/frida/js/fixtures.d/c3fj.js.
# Purposes and per-jcc tables: log/c3/c3fj_purpose.md. Notes and drops: log/c3/c3fj_notes.txt.

EXE = "golf_clean.exe"

# buildSceneSprites 0x00463180(kind, variant, x, y, x2, y2, obj): one key per kind (the cross product course 0..3 x
# variant x tile bit 0x4000 x alt, built by scenesFor() in re/frida/js/fixtures.d/c3fj.js; the getter of $kN applies
# scene N). Every drawing / projection callee is a recording stand-in; the body returns nothing (mainLoop never reads
# eax), so the evidence is the call log and the memory it writes.
_BSS_STATE = [
    ("$clog", 0, 0x4000),      # every stood-in call (targets, cells, queued sprites, projections, sprite draws, texts)
    (0x00838204, 0, 4),        # the last cell (written after every cellAt)
    (0x0058BCB8, 0, 0x100),    # placed-object records 0..15 (kind 10 clears the rotation byte +6)
    (0x0053CAF0 + 0xC8, 0, 0x320),   # tile words 100..499 (bit 0x4000 cleared at 0x00463308 / 0x0046344e)
    (0x0051A068, 0, 0x40),     # the text buffer (kind 5)
    (0x00834164, 0, 12),       # the __itoa buffer (kind 5)
]
_BSS_COUNTS = {"misc": 18, "k3": 32, "k4": 32, "k5": 32, "k6": 36, "k7": 32, "k8": 48, "k9": 32, "k10": 28,
               "k11": 32, "k12": 40, "k13": 40, "k14": 32, "k15": 32, "k17": 32, "k18": 32}


def _bss_vectors(n):
    return [tuple(f"${c}{i}" for c in "kvxypqo") for i in range(n)]


HOOKS = {}
for _k, _n in _BSS_COUNTS.items():
    HOOKS[f"buildSceneSprites_{_k}"] = dict(
        module=EXE, addr=0x00463180, abi="default", ret="void", args=["int"] * 7,
        fixture=f"c3fj_bss_{_k}", state=_BSS_STATE, vectors=_bss_vectors(_n))

# netHandleControl 0x0049acf0(this, id, flag): __thiscall, `ret 8`, returns 0 / 1. 32 scenes (fixture c3fj_nethandle,
# the getter of $oN applies scene N and returns the session object). The dialog layer is stood in through framemask.js
# ($fmlog); netNameOf and setHudTextSlot run for real.
_NH_STATE = [
    ("$fmlog", 0, 0x8000),     # every stood-in call, frame pointers as (object, offset) tokens
    ("$no", 0, 0x800),         # the session object (peer slots, +0x6f8, +0x780, +0xf0)
    ("$ans", 0, 0x10),         # the stand-ins' answers (TextView::build, destroyNodes)
    (0x008400AC, 0, 4), (0x00839650, 0, 4), (0x0083AFCC, 0, 4),
    (0x0083E8B8, 0, 0x28), (0x0083D3C8, 0, 0x28), (0x0083E8E0, 0, 0x200),   # HUD slots (setHudTextSlot)
]
HOOKS["netHandleControl"] = dict(
    module=EXE, addr=0x0049ACF0, abi="thiscall", ret="int", args=["pointer", "uint32", "int"],
    fixture="c3fj_nethandle", state=_NH_STATE, vectors=[(f"$o{i}", f"$i{i}", f"$f{i}") for i in range(32)])
if os.environ.get("C3FJ_ONLY"):        # debug: C3FJ_ONLY=3,7 keeps those vectors of every c3fj key (never in a real run)
    _only = [int(x) for x in os.environ["C3FJ_ONLY"].split(",")]
    HOOKS["netHandleControl"]["vectors"] = [HOOKS["netHandleControl"]["vectors"][i] for i in _only]

# Negative-control keys for the negctl build (shim/src/re/c3fj_negctl.cpp through log/c3/c3fj_negctl_batches.txt):
# key buildSceneSprites_neg<N> runs every scene of one scene list with edit N live (word 2 of the exported C3fjNegCtl
# switch). The edit -> scene list map is EDIT_KEYS of log/c3/alt/c3fj_negctl_gen.py (copied here and in the fixture).
# Registered only when C3FJ_CONTROLS=1 (log/c3/alt/c3fj_ctl.py sets it), so a normal run never sees them.
_BSS_NEG_KEYS = {1: "k14", 2: "k15", 3: "k7", 4: "k3", 5: "k4", 6: "k6", 7: "k8", 8: "k10", 9: "k17", 10: "k5",
                 11: "k13", 12: "k14", 13: "k9", 14: "k11", 15: "k18", 16: "k7", 17: "k12", 18: "misc", 19: "k6",
                 20: "k15", 21: "k7", 22: "k5"}
if os.environ.get("C3FJ_CONTROLS") == "1":
    # netHandleControl: edits 23..34 over all 32 scenes; _raw runs framemask with the mapping off (contract 5: a correct
    # body must read RED wherever it passes a frame pointer)
    for _e in range(23, 35):
        HOOKS[f"netHandleControl_neg{_e}"] = dict(HOOKS["netHandleControl"], fixture=f"c3fj_nethandle_neg{_e}")
    HOOKS["netHandleControl_raw"] = dict(HOOKS["netHandleControl"], fixture="c3fj_nethandle_raw")
    for _e, _k in _BSS_NEG_KEYS.items():
        HOOKS[f"buildSceneSprites_neg{_e}"] = dict(
            module=EXE, addr=0x00463180, abi="default", ret="void", args=["int"] * 7,
            fixture=f"c3fj_bss_neg{_e}", state=_BSS_STATE, vectors=_bss_vectors(_BSS_COUNTS[_k]))

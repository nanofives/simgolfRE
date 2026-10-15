import os

# C3 batch c3fs (round 35, 2026-10-14): golf_clean.exe large rows whose originals read frame slots they never wrote.
# Reimplementations: shim/src/re/c3fs.cpp. Fixtures: re/frida/js/fixtures.d/c3fs.js.
# Purposes and per-jcc tables: log/c3/c3fs_purpose.md. Notes and drops: log/c3/c3fs_notes.txt.

EXE = "golf_clean.exe"

# drawFlyingBall 0x004038f0(): takes no argument; the "$sN" argument is an unused cdecl stack word whose getter applies
# scene N (c3ec's trick). One key per selection mode: a golfer (0x80 + g), a walker index, none. Every drawing /
# projection callee is a recording stand-in; the body returns nothing (mainLoop never reads eax), so the evidence is the
# call log and the memory it writes.
_FB_STATE = [
    ("$clog", 0, 0x8000),        # every stood-in call (circles, projections, sprites, texts, cells)
    ("$seed", 0, 8),             # the seeded slot value and the seeded typeBit7Clear call count
    (0x00585850, 0, 64 * 0x4c),  # the walker records (screen point +8 / +0xc, animation +0x1c, state +0x1e)
    (0x004E9A94, 0, 4),          # the flying-golfer animation counter
    (0x004E9A9C, 0, 4),          # the last cell
    (0x0051A068, 0, 0x80),       # the text buffer
    (0x004E9A88, 0, 0x10),       # the __itoa buffer
    (0x00582DB8, 0, 0x100),      # golfer record 0x99 (fields +0x18 / +0x19 / +0x1d / +0xb6 / +0xb8 / +0xba)
    (0x004D60A9, 0, 8 * 0x230),  # golfer type records 0..7 (+2 nibble cleared at 0x00403935)
]
_FB_COUNTS = {"golfer": 24, "walker": 24, "none": 12}

HOOKS = {}
for _k, _n in _FB_COUNTS.items():
    HOOKS[f"drawFlyingBall_{_k}"] = dict(
        module=EXE, addr=0x004038F0, abi="default", ret="void", args=["int"],
        fixture=f"c3fs_fb_{_k}", state=_FB_STATE, vectors=[(f"$s{i}",) for i in range(_n)])

if os.environ.get("C3FS_CONTROLS") == "1":
    # the seeding stub off: the slot read at 0x00403965 holds each arm's own stack bytes (must read RED where it is read)
    HOOKS["drawFlyingBall_golfer_noseed"] = dict(HOOKS["drawFlyingBall_golfer"], fixture="c3fs_fb_golfer_noseed")
    # negctl keys (shim/src/re/c3fs_negctl.cpp through log/c3/c3fs_negctl_batches.txt): edit N live over one scene list
    # (EDIT_KEYS of log/c3/alt/c3fs_negctl_gen.py)
    _FB_NEG = {1: "golfer", 2: "golfer", 3: "golfer", 4: "golfer", 5: "golfer", 6: "none", 7: "walker", 8: "none",
               9: "walker", 10: "none", 11: "walker", 12: "none", 13: "walker", 14: "walker", 15: "walker",
               16: "walker", 17: "walker", 18: "walker", 19: "walker", 20: "walker"}
    for _e, _k in _FB_NEG.items():
        HOOKS[f"drawFlyingBall_neg{_e}"] = dict(HOOKS[f"drawFlyingBall_{_k}"], fixture=f"c3fs_fb_neg{_e}")

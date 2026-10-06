# C3 batch c3l (2026-10-06): ui/util leaves, two global writers and a slot hit-test, reimplemented in
# shim/src/re/c3l.cpp. Format documented at the top of re/frida/hooks_registry.py. Keys are the hooks.csv
# names (MsgBox::setButtonB carries `::`; diff_hook writes its CSV as MsgBox_setButtonB). The two writers
# declare the three globals each one stores as their `state` region so the A/B snapshots, compares and
# restores them.

# direction8 / angleFixed: (a, b) vectors across the sign quadrants, the 2:1 ratio boundaries and the axes.
_SIGNS = [-1000, -300, -101, -100, -99, -50, -2, -1, 0, 1, 2, 50, 99, 100, 101, 300, 1000]
_VEC = [(a, b) for a in _SIGNS for b in _SIGNS]

# sinScaled: angles sweeping the quadrant bits (30, 31) and the 22-bit interpolation fraction, with
# amplitudes straddling the 0x10000 / 0x1000000 pre-shift boundaries (and a few negatives).
_ANG = [0, 0x100000, 0x2aaaaa, 0x3fffff, 0x400000, 0x1555555, 0x3fffffff, 0x40000000, 0x55555555,
        0x7fffffff, -0x40000000, -1, 0x12345678, 0x6abcdef0]
_AMP = [1, 0x40, 0x100, 0xffff, 0x10000, 0x10001, 0x5a827, 0xfffff, 0xffffff, 0x1000000, 0x1000001,
        0x4000000, -0x100, -0x10000, -0x1000000]
_SIN = [(a, m) for a in _ANG for m in _AMP]

# hitTestSlot432f90: hotspot centres (0xed,0x20d)=(237,525) and (0x105,0x242)=(261,578), on-slot points
# (slots 0..15 lie in x 270..797, y 423..536 with the .data base table currently zero), and clear misses.
_HIT = [(237, 525), (240, 528), (235, 523), (261, 578), (263, 580), (258, 575),   # hotspots
        (300, 470), (290, 460), (331, 497),                                       # slot 0
        (340, 470), (350, 495),                                                   # slot 1
        (400, 470), (470, 470), (640, 470),                                       # slots 2, 3, 6
        (310, 510), (320, 500),                                                   # slot 7 (y above slot 0)
        (430, 510), (560, 510),                                                   # slots 9, 11
        (750, 440), (700, 530), (760, 490),                                       # slots 13, 14, 15
        (0, 0), (400, 400), (800, 599), (269, 455), (797, 480), (500, 537),       # misses
        (-100, -100), (240, 455)]

HOOKS = {}
HOOKS.update({
    # --- pure leaves (no state, no fixture) ---
    "direction8": dict(module="golf_clean.exe", addr=0x004671A0, abi="default", ret="int", args=["int", "int"],
                       vectors=_VEC),
    "angleFixed": dict(module="golf_clean.exe", addr=0x004672D0, abi="default", ret="int", args=["int", "int"],
                       vectors=_VEC),
    "sinScaled": dict(module="golf_clean.exe", addr=0x00491C70, abi="default", ret="int", args=["int", "int"],
                      vectors=_SIN),

    # --- reader of the fixed button tables (no fixture: 0x004c79b4/0x004c79a1 are const image data and
    #     0x00570cd4 reads as zero at the menu); its callee approxDistance is hooked on both arms ---
    "hitTestSlot432f90": dict(module="golf_clean.exe", addr=0x00432F90, abi="default", ret="int",
                              args=["int", "int"], vectors=_HIT),

    # --- reader + out-param writer: the remaining-count cell is the state region ---
    "scanMarkupText": dict(module="golf_clean.exe", addr=0x00476D40, abi="default", ret="pointer",
                           args=["pointer", "pointer"], fixture="c3l_markup",
                           state=[("$len0", 0, 4), ("$len1", 0, 4), ("$len3", 0, 4), ("$len8", 0, 4),
                                  ("$len20", 0, 4)],
                           vectors=[("$p0", "$len0"), ("$p0", "$len8"), ("$p2", "$len8"), ("$p3", "$len8"),
                                    ("$p5", "$len3"), ("$p6", "$len3"), ("$p12", "$len3"), ("$p12", "$len1"),
                                    ("$p0", "$len20"), ("$p6", "$len1")]),

    # --- global writers: the obj fixture's +4 field is non-zero (store branch) or zero (skip branch) ---
    "setButtonDefaults": dict(module="golf_clean.exe", addr=0x004889F0, abi="default", ret="int",
                              args=["pointer", "int", "int"], fixture="c3l_btnobj",
                              state=[(0x0083B60C, 0, 12)],
                              vectors=[("$obj", 0x111, 0x222), ("$obj", 0x333, 0x444), ("$objz", 0x555, 0x666),
                                       ("$objz", 0x777, 0x888), (0, 0x999, 0xaaa), ("$obj", 0, 0),
                                       ("$objz", -1, -2)]),
    "MsgBox::setButtonB": dict(module="golf_clean.exe", addr=0x00490CF0, abi="default", ret="int",
                               args=["pointer", "int", "int"], fixture="c3l_btnobj",
                               state=[(0x0083B9C4, 0, 12)],
                               vectors=[("$obj", 0x111, 0x222), ("$obj", 0x333, 0x444), ("$objz", 0x555, 0x666),
                                        ("$objz", 0x777, 0x888), (0, 0x999, 0xaaa), ("$obj", 0, 0),
                                        ("$objz", -1, -2)]),
})

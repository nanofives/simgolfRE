# C3 batch c3e (2026-10-06): terrain/course/input/golfer/sim readers and writers, reimplemented in
# shim/src/re/c3e.cpp. Format documented at the top of re/frida/hooks_registry.py. Keys are the hooks.csv
# names. Writers declare their `state` regions so the A/B snapshots, compares and restores them.

# (a, b) for bilinearSample: values chosen so the tile index (v>>8)&0xf and sub-tile weight (v>>3)&0x1f span
# the grid; negatives exercise the arithmetic shifts.
_BIL = [v for v in (-0x200, -0x80, 0, 0x40, 0x80, 0x140, 0x208, 0x300, 0x3c8, 0x555, 0x678, 0x800, 0xa40, 0xf80,
                    0x1000, 0x17c0)]
_BILPAIRS = [(a, b) for a in _BIL for b in _BIL]

# Tile coordinates for objectAt; interior cells plus a few edges so some vectors land inside a placed footprint.
_CELLS = [(x, y) for x in (0, 1, 2, 5, 10, 15, 24, 33, 44, 49) for y in (0, 1, 3, 8, 17, 25, 36, 44, 49)]

HOOKS = {}
HOOKS.update({
    # --- pure leaf readers ---
    "bilinearSample": dict(module="golf_clean.exe", addr=0x004674C0, abi="default", ret="int",
                           args=["int", "int"], fixture="c3e_bilinear", vectors=_BILPAIRS),
    "objectAt": dict(module="golf_clean.exe", addr=0x0040DF80, abi="default", ret="int", args=["int", "int"],
                     fixture="c3e_placed", vectors=_CELLS),
    "matchInputCode": dict(module="golf_clean.exe", addr=0x0047EEE0, abi="default", ret="int",
                           args=["pointer", "int"], fixture="c3e_inputcode",
                           vectors=[("$code", 10), ("$code", 5), ("$code", 2), ("$code", 1), ("$code", 0),
                                    ("$bad", 10), ("$bad", 3)]),

    # --- writers (state = the regions each one mutates) ---
    "resetRecordBank": dict(module="golf_clean.exe", addr=0x00401000, abi="default", ret="void", args=["int"],
                            fixture="c3e_banks", state=[(0x004E6D20, 0, 0x2C0)],
                            vectors=[(p,) for p in (0, 1, 2, 3, 4, 5)]),
    "updatePairSnapshot": dict(module="golf_clean.exe", addr=0x00409950, abi="default", ret="void", args=["int"],
                               fixture="c3e_pairs", state=[(0x0059FC60, 0, 0xBC0)],
                               vectors=[(p,) for p in (0, 1, 2, 3)]),
    "addGolferPair": dict(module="golf_clean.exe", addr=0x004099F0, abi="default", ret="void", args=["int"],
                          fixture="c3e_addpair", state=[(0x0059FC60, 0, 0x390)],
                          vectors=[(g,) for g in (0, 1, 2, 3, 4, 5, 6, 7)]),
    "updateRollingStats": dict(module="golf_clean.exe", addr=0x00409BF0, abi="default", ret="void", args=[],
                               fixture="c3e_rolling", state=[(0x005736B8, 0, 0x2410)], vectors=[()]),
})

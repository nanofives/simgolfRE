# C3 batch c3q (2026-10-07): functions with a static caller that the test scenarios never reach, reimplemented in
# shim/src/re/c3q.cpp. Format documented at the top of re/frida/hooks_registry.py; fixtures in
# re/frida/js/fixtures.d/c3q.js. Which side of every jcc each vector takes: log/c3/c3q_purpose.md, produced by the
# branch model log/c3/c3q_coverage_model.py (which imports the vectors below).

# --- postEvent 0x0046e7b0 (id, a, b): ids 0..15 (even ids have a free record, odd ids a used one) ---
_EVENT = [(i, 0x200 + i * 0x400, 0x7200 - i * 0x400) for i in range(16)]
_EVENT_STATE = [(0x004C15A0, 0, 16 * 0x30), (0x004E3DB8, 0, 8), (0x008392A4, 0, 4), (0x00839338, 0, 4)]

# --- relaxTile42f630 0x0042f630 (x, y), x and y in 0..49 (the function reads tile (x, y) without a bounds test) ---
_RELAX = [(0, 0), (0, 5), (0, 49), (49, 0), (49, 49), (1, 1), (2, 3), (3, 3), (3, 4), (4, 3), (4, 4), (5, 9), (7, 7),
          (8, 8), (10, 2), (11, 13), (12, 12), (15, 16), (19, 20), (20, 21), (22, 7), (25, 25), (30, 31), (33, 6),
          (36, 40), (40, 11), (44, 44), (47, 3), (48, 27), (6, 45)]

# --- canStep 0x00407400 (x, y, dir): interior cells (the neighbour stays on the 50x50 tables), every direction ---
_STEP_CELLS = [(1, 1), (2, 5), (3, 8), (5, 2), (7, 7), (9, 4), (10, 10), (12, 3), (14, 20), (17, 9), (20, 30),
               (23, 1), (25, 25), (31, 17), (40, 40), (48, 2)]
_STEP = [(x, y, d) for x, y in _STEP_CELLS for d in range(8)]

# --- freeRecordAtPos 0x004011b0 (x, y): records' (+6, +8) = (i % 10, (i*3) % 7), record 98 (0x7fff, -0x8000),
#     record 99 (-1, -1) ---
_FREE = [(0, 0), (1, 3), (3, 2), (9, 6), (5, 1), (9, 0), (0, 6), (10, 0), (-1, -1), (0xFFFF, -1), (0x7FFF, -0x8000),
         (0x7FFF, 0x8000), (2, 7), (-7, 3), (4, 5)]

# --- pushTripleEntry 0x00409cb0 (a, b, c) ---
_TRIPLE = [(1, 2, 3), (0, 0, 0), (-1, -2, -3), (0x7FFFFFFF, -0x80000000, 5), (10, 20, 30), (0x1234, 0x5678, 0x9ABC),
           (-100, 100, 0), (7, 7, 7), (0x400, 0x800, 0xC00), (99, -99, 1)]
_TRIPLE_STATE = [(0x005A9CD4, 0, 4), (0x00586B50, 0, 0x400), (0x00586FA8, 0, 0x400), (0x005A8834, 0, 0x400)]

# --- rankValue 0x0040e5b0 (v): q = v / 200 boundaries ---
_RANK = [-0x80000000, -100000, -2000, -401, -400, -399, -201, -200, -199, -1, 0, 1, 199, 200, 399, 400, 599, 600,
         601, 1199, 1200, 1999, 2000, 2199, 100000, 0x7FFFFFFF]

# --- appendEndearment 0x00467560 (n): every n & 3 with negative and large n ---
_ENDEAR = [0, 1, 2, 3, 4, 5, 6, 7, -1, -2, -3, -4, 0x7FFFFFFF, -0x80000000, 1001, 1002]
# --- appendRatingLabel 0x004532a0 (v): q = v / 25 boundaries ---
_RATING = [-0x80000000, -25, -1, 0, 1, 24, 25, 49, 50, 74, 75, 99, 100, 124, 125, 1000, 0x7FFFFFFF]
_TEXT_STATE = [(0x0051A068, 0, 0x40)]

# --- addHabitatSlot 0x00405970 (x, y, kind) ---
_HAB = [(0, 0, 0), (1, 2, 3), (49, 49, 255), (10, 20, 0x1FF), (-1, -2, 7), (0x200000, 0x3FFFFF, 1), (25, 7, 0x80),
        (3, 44, -1), (12, 12, 12), (30, 1, 2)]
_HAB_STATE = [(0x00572CB0, 0, 128 * 0x14), (0x00822D9C, 0, 4)]

# --- nearestHole 0x00407340 (x, y) ---
_HOLE = [(0, 0), (5, 40), (8, 6), (17, 28), (16, 28), (30, 44), (31, 45), (29, 44), (2, 3), (3, 4), (2, 2), (40, 5),
         (40, 23), (25, 25), (100, 100), (-500, -500), (20000, 100), (12, 30), (39, 6), (9, 36)]
_HOLE_FAR = [(0, 0), (25, 25), (199990, 199990), (-40000, 3)]

HOOKS = {}
HOOKS.update({
    "postEvent": dict(module="golf_clean.exe", addr=0x0046E7B0, abi="default", ret="void", args=["int", "int", "int"],
                      fixture="c3q_event_on", state=_EVENT_STATE, vectors=_EVENT),
    "postEvent_off": dict(module="golf_clean.exe", addr=0x0046E7B0, abi="default", ret="void",
                          args=["int", "int", "int"], fixture="c3q_event_off", state=_EVENT_STATE, vectors=_EVENT),

    "relaxTile42f630": dict(module="golf_clean.exe", addr=0x0042F630, abi="default", ret="int", args=["int", "int"],
                            fixture="c3q_relax", state=[(0x00543018, 0, 2500)], vectors=_RELAX),

    "canStep": dict(module="golf_clean.exe", addr=0x00407400, abi="default", ret="int", args=["int", "int", "int"],
                    fixture="c3q_step", vectors=_STEP),

    "freeRecordAtPos": dict(module="golf_clean.exe", addr=0x004011B0, abi="default", ret="void", args=["int", "int"],
                            fixture="c3q_records", state=[(0x0056D1D8, 0, 100 * 0x3C)], vectors=_FREE),

    "rankValue": dict(module="golf_clean.exe", addr=0x0040E5B0, abi="default", ret="int", args=["int"],
                      vectors=[(v,) for v in _RANK]),

    "appendEndearment": dict(module="golf_clean.exe", addr=0x00467560, abi="default", ret="void", args=["int"],
                             fixture="c3q_text_empty", state=_TEXT_STATE, vectors=[(n,) for n in _ENDEAR]),
    "appendEndearment_q3": dict(module="golf_clean.exe", addr=0x00467560, abi="default", ret="void", args=["int"],
                                fixture="c3q_text_q3", state=_TEXT_STATE, vectors=[(n,) for n in _ENDEAR]),
    "appendRatingLabel": dict(module="golf_clean.exe", addr=0x004532A0, abi="default", ret="void", args=["int"],
                              fixture="c3q_text_empty", state=_TEXT_STATE, vectors=[(v,) for v in _RATING]),
    "appendRatingLabel_q3": dict(module="golf_clean.exe", addr=0x004532A0, abi="default", ret="void", args=["int"],
                                 fixture="c3q_text_q3", state=_TEXT_STATE, vectors=[(v,) for v in _RATING]),

    "nearestHole": dict(module="golf_clean.exe", addr=0x00407340, abi="default", ret="int", args=["int", "int"],
                        fixture="c3q_holes", vectors=_HOLE),
    "nearestHole_far": dict(module="golf_clean.exe", addr=0x00407340, abi="default", ret="int", args=["int", "int"],
                            fixture="c3q_holes_far", vectors=_HOLE_FAR),
})

for _k, _f in (("_n0", "c3q_triple_n0"), ("_n7", "c3q_triple_n7"), ("_n255", "c3q_triple_n255"),
               ("_n256", "c3q_triple_n256"), ("_big", "c3q_triple_big")):
    HOOKS["pushTripleEntry" + _k] = dict(module="golf_clean.exe", addr=0x00409CB0, abi="default", ret="void",
                                         args=["int", "int", "int"], fixture=_f, state=_TRIPLE_STATE,
                                         vectors=_TRIPLE)

for _k, _f in (("", "c3q_habitat_5"), ("_0", "c3q_habitat_0"), ("_127", "c3q_habitat_127"),
               ("_full", "c3q_habitat_full")):
    HOOKS["addHabitatSlot" + _k] = dict(module="golf_clean.exe", addr=0x00405970, abi="default", ret="int",
                                        args=["int", "int", "int"], fixture=_f, state=_HAB_STATE, vectors=_HAB)

# C3 batch c3z (2026-10-07) of golf_clean.exe: ui functions with a static caller but not reached by the scenarios.
# Three panel hit-tests mapping a screen point to an index (hitEmployeeSlot435570, pickAmenity434980, hitEmployee436b00),
# the course-upgrade message builder appendUpgradeText, the hotspot-list writer HotList::add, and the greedy word-wrap
# helper wrapTextToWidth. Reimplemented in shim/src/re/c3z.cpp; fixtures in re/frida/js/fixtures.d/c3z.js; the side of
# every jcc each vector takes is in log/c3/c3z_purpose.md.
#
# Cross-batch note: none. The three hit-tests call clamp (0x00467130) and approxDistance (0x00467170), appendUpgradeText
# is a leaf, HotList::add calls grow/freeEntryTip (0x00492690/0x00492660), wrapTextToWidth calls measureTextWidth
# (0x00483930) and memchr; none of those callees are in this batch, so both A/B arms use the same deployed callee.

# Hit-test (x, y) vectors. One group per panel; each vector is annotated in the purpose file with the id it targets.
_HITSLOT = [(100, 100), (310, 497), (400, 600), (900, 650), (310, 498), (320, 700), (700, 498),
            (285, 492), (256, 510), (232, 538), (332, 592), (772, 592)]
_PICKAMEN = [(0, 0), (269, 500), (302, 563), (333, 526), (363, 563), (470, 575), (591, 575),
             (652, 575), (714, 575), (248, 567), (531, 580)]
_HITEMP = [(0, 0), (286, 492), (256, 510), (270, 558), (321, 549), (604, 549), (647, 513),
           (699, 510), (756, 513), (368, 523), (368, 566), (431, 523), (557, 523)]

# appendUpgradeText(i): leaf writer into the shared buffer at 0x0051a068; i and i+1 index the class-name table at
# 0x004c2a18 (valid to at least i = 12). i == 0 and i == 1 add the two different trailing clauses.
_APPEND = [(i,) for i in range(10)]

# HotList::add(this, a, b, x, y, w, h, s): s == 0 (NULL) keeps the entry text-free (no malloc) and the pre-sized array
# keeps count below capacity (no grow); both A/B arms then write identical bytes. Varying a/b/rect makes slot 0 differ.
_HOTADD = [("$obj", i * 3 + 1, i * 5 + 2, i * 7 + 3, i * 11 + 4, i * 13 + 5, i * 17 + 6, 0) for i in range(12)]
_HOTADD_ARGS = ["pointer", "int", "int", "int", "int", "int", "int", "pointer"]

# wrapTextToWidth(this, s, budget, remain): budget is a pointer to the pixel budget, remain the byte count (passed as a
# pointer-sized integer; 0 reaches the empty-run entry branch). Two keys, differing only in the accumulator the fixture
# seeds (0 vs 100); the (text, budget, remain) case lists mirror _ACC0 / _ACCN in js/fixtures.d/c3z.js.
_WRAP_ARGS = ["pointer", "pointer", "pointer", "pointer"]
# (text, budget, remain) -> the fixture exposes s<i>/b<i>; remain is the byte count, 0 for the empty run.
_ACC0 = [("aaaa bbbb", 400, 9), ("aaaa bbbb", 8, 9), ("aa bb cc", 30, 8), ("aaaa", 400, 4), ("aaaa", 50, 0),
         ("a bb ccc dddd", 400, 13), ("a bb ccc dddd", 20, 13), ("xx yy", 400, 5), ("xx yy", 10, 5), ("aaaa", 8, 4)]
_ACCN = [("aaaa bbbb", 8, 9), ("aaaa", 8, 4), ("aaaa bbbb", 400, 9)]
_ACC0_VEC = [("$layout", f"$s{i}", f"$b{i}", rem) for i, (_t, _b, rem) in enumerate(_ACC0)]
_ACCN_VEC = [("$layout", f"$s{i}", f"$b{i}", rem) for i, (_t, _b, rem) in enumerate(_ACCN)]
_ACC0_STATE = [(f"$b{i}", 0, 4) for i in range(len(_ACC0))] + [(0x00839AA8, 0, 4)]
_ACCN_STATE = [(f"$b{i}", 0, 4) for i in range(len(_ACCN))] + [(0x00839AA8, 0, 4)]

HOOKS = {
    "hitEmployeeSlot435570": dict(module="golf_clean.exe", addr=0x00435570, abi="default", ret="int",
                                  args=["int", "int"], vectors=_HITSLOT),
    "pickAmenity434980": dict(module="golf_clean.exe", addr=0x00434980, abi="default", ret="int",
                              args=["int", "int"], vectors=_PICKAMEN),
    "hitEmployee436b00": dict(module="golf_clean.exe", addr=0x00436b00, abi="default", ret="int",
                              args=["int", "int"], vectors=_HITEMP),
    "appendUpgradeText": dict(module="golf_clean.exe", addr=0x0040E5F0, abi="default", ret="void",
                              args=["int"], state=[(0x0051A068, 0, 0x200)], vectors=_APPEND),
    "HotList::add": dict(module="golf_clean.exe", addr=0x004929B0, abi="thiscall", ret="int", args=_HOTADD_ARGS,
                         fixture="c3z_hotlist", state=[("$arr", 0, 0x40), ("$obj", 0x50, 0xc)], vectors=_HOTADD),
    # Ten runs with the accumulator seeded to 0; one run per (text, budget, remain) in _ACC0. See the jcc table in
    # log/c3/c3z_purpose.md: this key reaches the multi-word fit, first-word-too-wide (acc==0 -> next word start),
    # mid-run break, single-word fit, single-word too narrow (acc==0 -> 0) and the empty run (remain==0) branches.
    "wrapTextToWidth_acc0": dict(module="golf_clean.exe", addr=0x00483980, abi="thiscall", ret="pointer",
                                 args=_WRAP_ARGS, fixture="c3z_wrap_acc0", state=_ACC0_STATE, vectors=_ACC0_VEC),
    # Accumulator seeded to 100: the first-word-too-wide and single-word-too-narrow tests take their acc!=0 side
    # (return the word itself), plus one fit that folds the consumed width into the non-zero accumulator.
    "wrapTextToWidth_accN": dict(module="golf_clean.exe", addr=0x00483980, abi="thiscall", ret="pointer",
                                 args=_WRAP_ARGS, fixture="c3z_wrap_accN", state=_ACCN_STATE, vectors=_ACCN_VEC),
}

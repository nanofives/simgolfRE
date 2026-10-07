# C3 batch c3ah (2026-10-08): course / economy event builders, reimplemented in shim/src/re/c3ah.cpp.
# Fixtures: re/frida/js/fixtures.d/c3ah.js. Branch coverage per key: log/c3/c3ah_purpose.md.
#
# addScore 0x004732d0 is __cdecl int with no arguments, so it gets one registry entry per fixture variant (each key
# is a single call whose scoreboard writes and return value are the compared evidence). clearObjectFootprint
# 0x0040e400 and membershipReport 0x00406670 take one argument, so each key sweeps a table of seeded records.

# --- shared regions -------------------------------------------------------------------------------------------
# The shared text buffer 0x0051a068 and the itoa scratch 0x0058a528 (every builder in this batch writes them).
_TEXT = [(0x0051A068, 0, 768), (0x0058A528, 0, 32)]
# startMessage 0x0040cb00 writers (as c3g's _MSG_STATE) with the message copy widened to the report length, plus
# the RNG seed 0x00822d9c that startMessage and Random::range advance.
# c3g's _MSG_STATE with the message copy widened to the report length. All nine have to stay in the list: every
# one of them is written by startMessage 0x0040cb00, so leaving one out makes the second A/B arm start from the
# first arm's ticker state and the first vector of every key goes RED.
_MSG = [(0x0053DF54, 0, 4), (0x00569498, 0, 1), (0x005A34EC, 0, 4), (0x005A7144, 0, 4), (0x004C2E08, 0, 4),
        (0x005694A4, 0, 4), (0x0056D1A8, 0, 8), (0x00822D9C, 0, 4), (0x005A6D40, 0, 768)]

# --- addScore 0x004732d0 --------------------------------------------------------------------------------------
# The 10 scoreboard rows at 0x00541ce0 (0x9c each), the selected-row dword 0x008392a8, and the text buffer that
# appendCourseTitle 0x0040daa0 fills.
_SCORE_STATE = [(0x00541CE0, 0, 10 * 0x9C), (0x008392A8, 0, 4)] + _TEXT

_SCORE = dict(module="golf_clean.exe", addr=0x004732D0, abi="default", ret="int", args=[], state=_SCORE_STATE,
              vectors=[()])

# --- clearObjectFootprint 0x0040e400 --------------------------------------------------------------------------
# Placed objects 0x0058bcb8 (0x10 each), the cash dword, the per-date revenue rows 0x00584212 (0x14 each), the
# global flags 0x0059e7b8, the three per-tile tables the footprint loop rewrites, the 100 position records
# freeRecordAtPos 0x004011b0 marks, pointsPopup's ring (as golf_writers' _POP_STATE) and everything
# rebuildHeightfield 0x0042f7a0 recomputes (0x00543018, 0x0051b770, the wall masks 0x005619a0, 0x004c2e04).
_POP = [(0x0059ABB0, 0, 4), (0x00542FD8, 0, 32), (0x00542FF8, 0, 32), (0x00542DD8, 0, 32), (0x00542F00, 0, 32)]
_OBJ_STATE = [(0x0058BCB8, 0, 256 * 0x10), (0x00571FD4, 0, 4), (0x00584212, 0, 100 * 0x14), (0x0059E7B8, 0, 4),
              (0x0053CAF0, 0, 2500 * 2), (0x005722E8, 0, 2500), (0x0056988C, 0, 2500),
              (0x0056D1D8, 0, 0x1778), (0x00543018, 0, 2500), (0x0051B770, 0, 20000),
              (0x005619A0, 0, 2500), (0x004C2E04, 0, 4)] + _POP

# One vector per seeded object record (see c3ah.js): 0 lot with cost, 1 lot without cost, 2 type 0xf, 3 type 0xc,
# 4 type 7, 5 type 8, 6 type 3, 7 zero footprint, 8 type 0, 9 type 6.
_OBJ_VECTORS = [(i,) for i in range(10)]
# Each call rebuilds the whole heightfield (rebuildHeightfield 0x0042f7a0 over the 50x50 grid) and leaves the tile
# tables scrambled; past roughly 25 calls in one boot the game's own menu loop dies on them, so only the kind-0 key
# sweeps all ten records and the two course-kind variants carry the two vectors that depend on the kind.
_OBJ = dict(module="golf_clean.exe", addr=0x0040E400, abi="default", ret="void", args=["int"], state=_OBJ_STATE,
            vectors=_OBJ_VECTORS)
_OBJ_KIND = dict(_OBJ, vectors=[(3,), (0,)])

# --- membershipReport 0x00406670 -------------------------------------------------------------------------------
# The golfer records (flag dword +0x08 of 0x005794c0 + g*0x100), the 76-entry membership type table 0x005849e0
# (0x2c each; the tier byte and the picked free slot are written), the report text, the ticker globals and the
# panel dwords 0x005a9ccc / 0x00569498.
# 0x00567afc is in the list so the ticker mode the menu latches while a message is up is restored with the rest;
# without it the menu keeps animating 0x00569498 / 0x005694a4 between the two arms and the run goes flaky.
_MEM_STATE = [(0x005794C0, 0, 0x20 * 0x100), (0x005849E0, 0, 76 * 0x2C), (0x005A9CCC, 0, 4),
              (0x00567AFC, 0, 4)] + _TEXT + _MSG

# One vector per seeded golfer: the fixture gives each golfer a membership slot whose tier byte makes
# (tier + 1) & 7 run over 0..5 (see c3ah.js).
_MEM_VECTORS = [(g,) for g in range(12)]
_MEM = dict(module="golf_clean.exe", addr=0x00406670, abi="default", ret="void", args=["int"], state=_MEM_STATE,
            vectors=_MEM_VECTORS)

# --- announceBuilding 0x0040e720 ------------------------------------------------------------------------------
# The hole table (widened to 0x26a8 so the hole-19 slot 0x00578150/0x00578154 the hole-1 arm reads is covered),
# the hole counter, the 10 dwords the pass clears, the end marker, the next-building index, the two course-centre
# bytes, the tee tile's flags word and marker byte, the report text, the ticker globals, logTick's word log and
# the event records postEvent 0x0046e7b0 fills.
_AB_STATE = [(0x00575AB0, 0, 0x26A8), (0x005685F0, 0, 4), (0x0056A524, 0, 40), (0x004C2848, 0, 4),
             (0x005A6364, 0, 4), (0x00585860, 0, 2), (0x0053CAF0, 0, 2500 * 2), (0x00578804, 0, 2500),
             (0x00568600, 0, 1000), (0x00834170, 0, 4),
             (0x004C15A0, 0, 0x300), (0x004E3DB8, 0, 8), (0x00839338, 0, 4), (0x008392A4, 0, 4)] + _TEXT + _MSG

_AB = dict(module="golf_clean.exe", addr=0x0040E720, abi="default", ret="void", args=[], state=_AB_STATE,
           vectors=[()])

# Key order matters for a single-boot run of the whole batch: every accepted startMessage leaves the menu's own
# ticker animating, and after roughly 50 of them in one session the menu's loop starts writing the ticker globals
# between the two A/B arms (RED) and then dies. The keys that start no message are therefore listed first.
HOOKS = {
    # addScore: one call per fixture; the fixture decides which row the course beats and which row carries its id.
    # No message and no sound.
    "addScore": dict(fixture="c3ah_score_top", **_SCORE),
    "addScore_mid": dict(fixture="c3ah_score_mid", **_SCORE),
    "addScore_last": dict(fixture="c3ah_score_last", **_SCORE),
    "addScore_dup": dict(fixture="c3ah_score_dup", **_SCORE),
    "addScore_dupi": dict(fixture="c3ah_score_dupi", **_SCORE),
    "addScore_rejid": dict(fixture="c3ah_score_rejid", **_SCORE),
    "addScore_rejend": dict(fixture="c3ah_score_rejend", **_SCORE),

    # clearObjectFootprint: course kind 0 / 1 / 2 select the replacement tile type of a type-0xc object and the
    # (course index >= 0xc) term of the default fill. No message.
    "clearObjectFootprint": dict(fixture="c3ah_obj_k0", **_OBJ),
    "clearObjectFootprint_k1": dict(fixture="c3ah_obj_k1", **_OBJ_KIND),
    "clearObjectFootprint_k2": dict(fixture="c3ah_obj_k2", **_OBJ_KIND),

    # membershipReport: ticker busy (startMessage refuses) / report flag clear (no message at all) first, then the
    # two keys that do start a message: free slots available / table full (no join sentence).
    "membershipReport_busy": dict(fixture="c3ah_mem_busy", **_MEM),
    "membershipReport_quiet": dict(fixture="c3ah_mem_quiet", **_MEM),
    "membershipReport": dict(fixture="c3ah_mem_free", **_MEM),
    "membershipReport_full": dict(fixture="c3ah_mem_full", **_MEM),

    # announceBuilding: argument-free, so one key per fixture variant; the three refusal keys and the one with the
    # clock stopped start no message, so they come first.
    "announceBuilding_rej_tee": dict(fixture="c3ah_ab_rej_tee", **_AB),
    "announceBuilding_rej_pin": dict(fixture="c3ah_ab_rej_pin", **_AB),
    "announceBuilding_rej_third": dict(fixture="c3ah_ab_rej_third", **_AB),
    "announceBuilding_skipbi": dict(fixture="c3ah_ab_skipbi", **_AB),
    "announceBuilding": dict(fixture="c3ah_ab_h7", **_AB),
    "announceBuilding_p3short": dict(fixture="c3ah_ab_p3short", **_AB),
    "announceBuilding_p2": dict(fixture="c3ah_ab_p2", **_AB),
    "announceBuilding_p4short": dict(fixture="c3ah_ab_p4short", **_AB),
    "announceBuilding_p5": dict(fixture="c3ah_ab_p5", **_AB),
    "announceBuilding_p5long": dict(fixture="c3ah_ab_p5long", **_AB),
    "announceBuilding_p3long": dict(fixture="c3ah_ab_p3long", **_AB),
    "announceBuilding_p4long": dict(fixture="c3ah_ab_p4long", **_AB),
    "announceBuilding_p5short": dict(fixture="c3ah_ab_p5short", **_AB),
    "announceBuilding_flag60short": dict(fixture="c3ah_ab_flag60short", **_AB),
    "announceBuilding_h1": dict(fixture="c3ah_ab_h1", **_AB),
    "announceBuilding_h6": dict(fixture="c3ah_ab_h6", **_AB),
    "announceBuilding_h10": dict(fixture="c3ah_ab_h10", **_AB),
    "announceBuilding_h18": dict(fixture="c3ah_ab_h18", **_AB),
    "announceBuilding_h9": dict(fixture="c3ah_ab_h9", **_AB),
    "announceBuilding_bi4": dict(fixture="c3ah_ab_bi4", **_AB),
    "announceBuilding_bi0": dict(fixture="c3ah_ab_bi0", **_AB),
    "announceBuilding_bi15": dict(fixture="c3ah_ab_bi15", **_AB),
}

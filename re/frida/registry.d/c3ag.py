# C3 batch c3ag (2026-10-08): golfer/course event builders, reimplemented in shim/src/re/c3ag.cpp.
# Fixtures: re/frida/js/fixtures.d/c3ag.js. Branch coverage per key: log/c3/c3ag_purpose.md.

# The shared scratch text buffer 0x0051a068, written by both functions and by their callees.
_TEXT = [(0x0051A068, 0, 512)]

# startMessage 0x0040cb00 writers (same set as c3ac/c3g), including the RNG seed 0x00822d9c that Random::range
# 0x0045c1e0 advances and the message copy 0x005a6d40.
_MSG = [(0x0053DF54, 0, 4), (0x00569498, 0, 1), (0x005A34EC, 0, 4), (0x005A7144, 0, 4), (0x004C2E08, 0, 4),
        (0x005694A4, 0, 4), (0x0056D1A8, 0, 8), (0x00822D9C, 0, 4), (0x005A6D40, 0, 512)]

# The two landmark bitmasks LandmarkAvailableNotice sets bit (id - 0x168) in (0x00472400 / 0x0047240b).
_MASKS = [(0x00822C70, 0, 4), (0x00543CFC, 0, 4)]

# substituteStoryNames 0x0045c460: one int argument (the golfer index); the only output is the text buffer.
# Golfers 40..57 are seeded in pairs by the fixture, so g and g ^ 1 are both defined for every vector.
_STORY_VEC = [(g,) for g in range(40, 58)]
_STORY = dict(module="golf_clean.exe", addr=0x0045C460, abi="default", ret="void", args=["int"],
              state=_TEXT, vectors=_STORY_VEC)

# LandmarkAvailableNotice 0x004722c0: (golfer, who). Golfer 10 reaches the accept test on the first tile, so the 14
# `who` rows sweep every arm of the switch at 0x0047236c; golfers 11/12/13/15 take the three reject tests and the
# tileBlocked early return.
_LM_VEC = ([(10, w) for w in range(14)] +
           [(12, 0), (12, 11), (13, 1), (13, 12), (11, 2), (11, 13), (15, 3), (15, 10)])
_LM = dict(module="golf_clean.exe", addr=0x004722C0, abi="default", ret="void", args=["int", "int"],
           state=_TEXT + _MASKS + _MSG, vectors=_LM_VEC)

# patronEvent 0x004266b0: one int argument (the golfer index). State: the text buffer (wider, the patron messages
# run to a few hundred characters), the __itoa scratch 0x0058a528, appendCents' scratch 0x00569628, every global the
# C2 note lists under Writes, the 100-entry word table 0x0058421e the CEO investment adds to, logTick's 0x00568600
# word log, the startMessage set, and the two record fields the tail writes for each golfer a vector uses.
_PATRON_GOLFERS = list(range(20, 38))
_PATRON_STATE = ([(0x0051A068, 0, 1024), (0x0058A528, 0, 32), (0x00569628, 0, 4),
                  (0x00572CAC, 0, 4), (0x0053A450, 0, 4), (0x00543CFC, 0, 4), (0x00822C70, 0, 4),
                  (0x00567A1C, 0, 4), (0x005A9CCC, 0, 4), (0x0059AAF8, 0, 4), (0x00571FD4, 0, 4),
                  # The CEO investment adds to the word 0x0058421e + slot*0x14, slot 0..99. The region stops at
                  # 0x005849dc (the last entry's end): the membership type table 0x005849e0 starts right after and
                  # the menu keeps writing it, which made a wider region read RED at random (observed 2026-10-08).
                  (0x005A59F8, 0, 4), (0x0058421E, 0, 0x7BE), (0x00568600, 0, 1000)] +
                 _MSG +
                 [(0x0057958C + g * 0x100, 0, 4) for g in _PATRON_GOLFERS] +
                 [(0x005794D9 + g * 0x100, 0, 1) for g in _PATRON_GOLFERS])
_PATRON = dict(module="golf_clean.exe", addr=0x004266B0, abi="default", ret="void", args=["int"],
               state=_PATRON_STATE, vectors=[(g,) for g in _PATRON_GOLFERS])

HOOKS = {
    "substituteStoryNames": dict(fixture="c3ag_story_both", **_STORY),
    "substituteStoryNames_rev": dict(fixture="c3ag_story_rev", **_STORY),
    "substituteStoryNames_partner": dict(fixture="c3ag_story_partner", **_STORY),
    "substituteStoryNames_none": dict(fixture="c3ag_story_none", **_STORY),
    "substituteStoryNames_dup": dict(fixture="c3ag_story_dup", **_STORY),
    "LandmarkAvailableNotice": dict(fixture="c3ag_landmark", **_LM),
    "LandmarkAvailableNotice_busy": dict(fixture="c3ag_landmark_busy", **_LM),
    "patronEvent": dict(fixture="c3ag_patron", **_PATRON),
    "patronEvent_diff2": dict(fixture="c3ag_patron_diff2", **_PATRON),
    "patronEvent_busy": dict(fixture="c3ag_patron_busy", **_PATRON),
    "patronEvent_lm0": dict(fixture="c3ag_patron_lm0", **_PATRON),
    "patronEvent_lm1": dict(fixture="c3ag_patron_lm1", **_PATRON),
    "patronEvent_lm2": dict(fixture="c3ag_patron_lm2", **_PATRON),
    "patronEvent_lm3": dict(fixture="c3ag_patron_lm3", **_PATRON),
}

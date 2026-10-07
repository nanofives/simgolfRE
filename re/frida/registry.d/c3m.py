# C3 batch c3m (2026-10-07): golfer/course text builders and the new-golfer generator, reimplemented in
# shim/src/re/c3m.cpp. Format documented at the top of re/frida/hooks_registry.py. Keys are the hooks.csv names; a
# function whose branches depend on a fixture-set global gets one entry per setting (`<name>_<variant>`). Every entry
# writes the shared text buffer 0x0051a068, which is a state region (snapshotted, compared and restored per vector).

# buildGolferName 0x004676e0: fixture golfers 40..63 (c3m_names) cover every kind form; each with and without title.
_NAME_GOLFERS = list(range(40, 64))
_NAME_VEC = [(g, w) for g in _NAME_GOLFERS for w in (0, 1)] + [(41, 7), (52, -1)]
_NAME_STATE = [(0x0051A068, 0, 128)]

# decorationName 0x00407700: (x, y, id) over the c3m_deco cells/objects/categories (see the fixture).
_DECO_VEC = [
    # types 0x16 / 0x15 by id: object kind 4 (landmarkName), kind 2, other kinds for each type
    (20, 0, 0x16), (20, 1, 0x15), (20, 2, 0x16), (20, 3, 0x15), (20, 4, 0x16), (20, 0, 0x15), (20, 1, 0x16),
    # id == -1: the type comes from the cell
    (20, 0, -1), (20, 3, -1), (21, 0, -1), (22, 1, -1), (24, 1, -1), (23, 0, -1),
    # id with bit 0x100: placed object (id & 0xff) gives the cell
    (0, 0, 0x1F0), (0, 0, 0x1F1), (0, 0, 0x1F3), (0, 0, 0x1F4), (0, 0, 0x1F5), (5, 5, 0x3F2),
    # category 4: type 4 with flag 0x1000 (variant byte % 5 = 0..4), with 0x1000|0x20, without; types 0xa, 0xc, other
    (22, 0, 4), (22, 1, 4), (22, 2, 4), (22, 3, 4), (22, 4, 4), (22, 5, 4), (23, 0, 4),
    (0, 0, 0x0A), (0, 0, 0x0C), (0, 0, 0x1A),
    # category 0xd: types 0x10, 0xd, 0xf, 0xe and one with no name
    (0, 0, 0x10), (0, 0, 0x0D), (0, 0, 0x0F), (0, 0, 0x0E), (0, 0, 0x1B),
    # category 0x11: type 0x13; type 0x11 on a bridge cell, a dolphin cell, an empty cell, a non-0x11 cell
    (24, 0, 0x13), (24, 0, 0x11), (24, 1, 0x11), (24, 2, 0x11), (24, 3, 0x11),
    # categories 7, 0xc, 0x12, 0x13, 0x15, 0x16, 5 (in range, default), 0, 0x17, -1 (outside 4..0x16)
    (0, 0, 7), (0, 0, 8), (0, 0, 9), (0, 0, 0x14), (0, 0, 0x1C), (0, 0, 0x1D), (0, 0, 0x1E), (0, 0, 0x1F),
    (0, 0, 0x20), (0, 0, 0x21),
]
_DECO_STATE = [(0x0051A068, 0, 256)]

# announceHoleType 0x00460df0: holes 0..3 (records seeded by c3m_holes) x every kind arm (1..7 and outside).
_HOLE_VEC = [(h, k) for h in range(4) for k in (0, 1, 2, 3, 4, 5, 6, 7, 8, -1)] + [(12345, 1), (-7, 2)]
# text buffer (strcpy'd: 83 + up to 11 + 35 + up to 147 + NUL bytes), the itoa buffer, and postEvent's writes
# (0x004c15a0 + kind*0x30 for kinds 0/1/4/9, 0x004e3db8/0x004e3dbc, 0x00839338, 0x008392a4).
_HOLE_STATE = [(0x0051A068, 0, 320), (0x00824134, 0, 16), (0x004C15A0, 0, 9 * 0x30 + 8), (0x004E3DB8, 0, 8),
               (0x00839338, 0, 4), (0x008392A4, 0, 4)]

# updateMembership 0x00421bc0: whole golfer table (0x98 records of 0x100 from 0x005794b8), slot counter, category
# counts, RNG seed (both arms draw the same numbers) and the text buffer.
_MEM_STATE = [(0x005794B8, 0, 0x98 * 0x100), (0x0059AE7C, 0, 4), (0x0059DEA0, 0, 0x80), (0x00822D9C, 0, 4),
              (0x0051A068, 0, 64)]
_MEM_VEC = [(0,), (1,), (2,)]


def _name(fx):
    return dict(module="golf_clean.exe", addr=0x004676E0, abi="default", ret="void", args=["int", "int"],
                fixture=fx, state=_NAME_STATE, vectors=_NAME_VEC)


def _deco(fx):
    return dict(module="golf_clean.exe", addr=0x00407700, abi="default", ret="int", args=["int", "int", "int"],
                fixture=fx, state=_DECO_STATE, vectors=_DECO_VEC)


def _hole(fx):
    return dict(module="golf_clean.exe", addr=0x00460DF0, abi="default", ret="void", args=["int", "int"],
                fixture=fx, state=_HOLE_STATE, vectors=_HOLE_VEC)


def _mem(fx):
    return dict(module="golf_clean.exe", addr=0x00421BC0, abi="default", ret="int", args=["int"],
                fixture=fx, state=_MEM_STATE, vectors=_MEM_VEC)


HOOKS = {
    # numeral-suffix global 0x0053a450 & 0x7f = 0 (no suffix), 5, 9 (above 8: the default suffix)
    "buildGolferName": _name("c3m_names_s0"),
    "buildGolferName_s5": _name("c3m_names_s5"),
    "buildGolferName_s9": _name("c3m_names_s9"),
    # course type byte 0x005a34e0 / course record byte (0x00571ff4 + course*0x2e)
    "decorationName": _deco("c3m_deco_ct0"),       # 0 / 0xd
    "decorationName_ct1": _deco("c3m_deco_ct1"),   # 1 / 0xa
    "decorationName_ct1b": _deco("c3m_deco_ct1b"),  # 1 / 0xd (type 4 without bit 0x1000 then reaches 0x00407a1b taken)
    "decorationName_ct2": _deco("c3m_deco_ct2"),   # 2 / 0
    "decorationName_ct3": _deco("c3m_deco_ct3"),   # 3 / 0xd
    "decorationName_ct4": _deco("c3m_deco_ct4"),   # 4 / 0xa
    # mode dword 0x00822c88 = 2 (events for kinds 3/5/6) / 1 (kind 7 only); _busy: postEvent's slots already taken
    "announceHoleType": _hole("c3m_holes_m2"),
    "announceHoleType_m1": _hole("c3m_holes_m1"),
    "announceHoleType_busy": _hole("c3m_holes_busy"),
    # see c3m.js: plain / all categories + settings / conflicting golfers / every type refused / counts >= 9999
    "updateMembership": _mem("c3m_mem_a"),
    "updateMembership_all": _mem("c3m_mem_all"),
    "updateMembership_conf": _mem("c3m_mem_conf"),
    "updateMembership_fail": _mem("c3m_mem_fail"),
    "updateMembership_big": dict(_mem("c3m_mem_big"), vectors=[(0,), (1,), (2,), (3,), (4,), (10,), (100,), (1000,)]),
}

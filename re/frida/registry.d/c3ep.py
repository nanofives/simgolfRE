# C3 batch c3ep (round 31, 2026-10-13) of golf_clean.exe: the file layer and the theme / save readers.
# Reimplementations: shim/src/re/c3ep.cpp. Fixtures: re/frida/js/fixtures.d/c3ep.js. Purposes and per-jcc branch
# tables: log/c3/c3ep_purpose.md.
#
# Every key freezes the game's first thread (the bodies write the shared text buffer 0x0051a068 and the tables the
# running menu also reads) and compares three stand-ins:
#   $lio    the low-level file block (__open / __read / __write / __close / __tell): every file's bytes and length, the
#           descriptors with their cursors, and the operation log. The directory stand-in's enumeration cursor lives in
#           the same block at +0xd0.
#   $implog the import stand-in's log (FindFirstFileA / FindNextFileA / FindClose, or the file stand-in's KERNEL32 set)
#   $rec    the recording stubs of the game callees a key does not run for real (c3ea's calleeStand)
# plus the globals each body writes.

EXE = "golf_clean.exe"

LIO = 0x3000
REC = 0x1000
FSB = 0x4000

TEXT = [(0x0051A068, 0, 0x600)]                     # the shared text buffer every body builds its path in
DIRTAB = [(0x0080B130, 0, 0x2710)]                  # listMatchingFiles' 100 x 0x64 table
BLOCKS = [("$lio", 0, LIO), ("$rec", 0, REC)]
IMPLOG = [("$implog", 0, 0x400)]


def _h(addr, fixture, vectors, state, ret="int", args=("pointer",), abi="default", nargs=None):
    d = dict(module=EXE, addr=addr, abi=abi, ret=ret, args=list(args), fixture=fixture, state=state, vectors=vectors)
    if nargs is not None:
        d["nargs"] = nargs
    return d


HOOKS = {}

# ---------------------------------------------------------------------------------------------------- listMatchingFiles
# 0x0043d2a0 (pattern, require, exclude). $p0 a nine-entry directory, $p1 an empty one (FindFirstFileA fails at
# 0x0043d2cc), $p2 one entry, $p3 twelve. Filters: $q0 empty, $q1 a substring three names carry, $q2 an extension,
# $q3 / $q4 exclusions, $q5 a substring no name carries.
_LMF_ST = TEXT + DIRTAB + BLOCKS + IMPLOG
_LMF = [("$p0", "$q0", "$q0"), ("$p0", "$q1", "$q0"), ("$p0", "$q2", "$q0"), ("$p0", "$q0", "$q3"),
        ("$p0", "$q0", "$q4"), ("$p0", "$q2", "$q3"), ("$p0", "$q1", "$q1"), ("$p0", "$q5", "$q0"),
        ("$p0", "$q0", "$q5"), ("$p0", "$q5", "$q5"), ("$p1", "$q0", "$q0"), ("$p1", "$q1", "$q2"),
        ("$p2", "$q0", "$q0"), ("$p2", "$q2", "$q0"), ("$p2", "$q0", "$q2"), ("$p3", "$q0", "$q0"),
        ("$p3", "$q2", "$q0"), ("$p3", "$q0", "$q2"), ("$p3", "$q1", "$q0"), ("$p3", "$q2", "$q3")]
HOOKS["listMatchingFiles"] = _h(0x0043D2A0, "c3ep_dir", _LMF, _LMF_ST, args=("pointer", "pointer", "pointer"))
HOOKS["listMatchingFiles_e"] = _h(0x0043D2A0, "c3ep_dir_empty",
                                  [("$p0", "$q0", "$q0"), ("$p3", "$q2", "$q0"), ("$p2", "$q0", "$q0"),
                                   ("$p1", "$q0", "$q0"), ("$p0", "$q1", "$q3")], _LMF_ST,
                                  args=("pointer", "pointer", "pointer"))

# ---------------------------------------------------------------------------------------------------- listThemes
# 0x004724c0, no argument: one call per key, one directory per key.
_LT_ST = TEXT + DIRTAB + BLOCKS + IMPLOG + [(0x0056E928, 0, 0x320), (0x00567328, 0, 0x40)]
for _v in ("mix", "one", "skip", "many", "empty", "chr"):
    HOOKS[f"listThemes_{_v}"] = _h(0x004724C0, f"c3ep_themes_{_v}", [()], _LT_ST, args=())

# ---------------------------------------------------------------------------------------------------- loadThemes
# 0x004658b0 (enable). loadGolferRoster 0x004659a0 and loadThemeFile 0x00437fa0 are recording stubs.
_LTH_ST = (TEXT + DIRTAB + BLOCKS + IMPLOG + [(0x00567328, 0, 0x40), (0x00584A0C, 0, 0x268)])
_LTH = [(0,), (1,), (-1,), (0x7FFFFFFF,)]
for _v in ("mix", "one", "empty", "many"):
    HOOKS[f"loadThemes_{_v}"] = _h(0x004658B0, f"c3ep_lt_{_v}", _LTH, _LTH_ST, args=("int",))

# ---------------------------------------------------------------------------------------------------- loadThemeFile
# 0x00437fa0 (name, index, slot). $n0 a complete file, $n1 one whose byte 0xa9 has bit 7 set (the other counter),
# $n2 one whose byte 0xaa is below 0x14, $n3 a negative sign byte with a large counter, $n4 a file that ends inside
# the second read; $nmiss has no file (fopen fails at 0x004380b1). Image::loadPcx 0x00485790 and Surface_blit
# 0x00473bf0 are recording stubs.
_TF_ST = (TEXT + [("$fsblock", 0, FSB), ("$rec", 0, REC)] + IMPLOG
          + [(0x00567328, 0, 0x40), (0x004D6088, 0, 0x8C0), (0x004D6088 + 0x4D * 0x230, 0, 0x230),
             (0x00543D10, 0, 0x1388), (0x00543D10 + 0x4D * 0x4E2, 0, 0x4E2), (0x005795A8, 0, 0x400),
             (0x005A5A04, 0, 0x14), (0x0059B76C, 0, 8), (0x0059E7B8, 0, 4)])
_TF = [("$n0", 0, 0), ("$n0", 1, 1), ("$n0", 2, -1), ("$n1", 0, 2), ("$n1", 1, -1), ("$n2", 2, 3),
       ("$n2", 0, 0), ("$n3", 1, 1), ("$n3", 0x4D, 2), ("$n4", 2, 0), ("$n4", 0x4D, -1), ("$nmiss", 0, 0),
       ("$nmiss", 0x4D, 1), ("$n0", 0x4D, 3), ("$n5", 1, -1), ("$n5", 0x4D, -1)]
for _v in ("tf", "tf_flag", "tf_full", "tf_mid"):
    HOOKS[f"loadThemeFile_{_v[3:] or 'a'}"] = _h(0x00437FA0, f"c3ep_{_v}", _TF, _TF_ST,
                                                 args=("pointer", "int", "int"))

# ---------------------------------------------------------------------------------------------------- saveGame
# 0x0040b4a0 (name). serializeCourse 0x0040afa0 is a recording stub; appendCourseTitle 0x0040daa0 and appendDate
# 0x0040d7b0 run for real (both only append to the text buffer). $nbad has no file, so __open fails and every later
# operation is logged against a descriptor that is not ours.
_SV_ST = (TEXT + BLOCKS + [(0x00568D08, 0, 4), (0x0058A528, 0, 0x20), (0x00834170, 0, 4)])
_SV = [("$n0",), ("$n1",), ("$n2",), ("$n3",), ("$nbad",)]
for _k, _v in (("a", "save"), ("d1", "save_d1"), ("d2", "save_d2"), ("neg", "save_neg")):
    HOOKS[f"saveGame_{_k}"] = _h(0x0040B4A0, f"c3ep_{_v}", _SV, _SV_ST, args=("pointer",))

# ---------------------------------------------------------------------------------------------------- loadCourse
# 0x0040b840 (mode). The path stands in the text buffer at entry. Every game callee is a recording stub.
_LC_ST = (TEXT + BLOCKS + [(0x00568D08, 0, 4), (0x004C2E08, 0, 4), (0x00567AFC, 0, 4), (0x004C2854, 0, 4),
                           (0x004C2848, 0, 4)])
_LC = [(0,), (1,), (2,), (3,), (4,), (-1,)]
for _v in ("lc", "lc_short", "lc_miss"):
    HOOKS[f"loadCourse_{_v[2:].lstrip('_') or 'a'}"] = _h(0x0040B840, f"c3ep_{_v}", _LC, _LC_ST, args=("int",))

# ---------------------------------------------------------------------------------------------------- loadGame
# 0x0040b9b0 (name, finish, headerOnly, championship). $nbrowse is the name the body compares with at 0x0040bb37.
_LG_ST = (TEXT + BLOCKS + [(0x00568D08, 0, 4), (0x0059E7B8, 0, 4), (0x00567AFC, 0, 4), (0x004C2854, 0, 4),
                           (0x004C2848, 0, 4), (0x004C2E08, 0, 4), (0x005A34E0, 0, 4)])
_LG = [("$n0", 0, 0, 0), ("$n0", 1, 0, 0), ("$n0", 0, 1, 0), ("$n0", 0, 0, 1), ("$n0", 1, 0, 1),
       ("$n0", 1, 1, 1), ("$n1", 1, 0, 0), ("$n1", 0, 0, 1), ("$nbrowse", 1, 0, 0), ("$nbrowse", 1, 0, 1),
       ("$nmiss", 0, 0, 0), ("$nmiss", 1, 0, 0), ("$n1", 1, 1, 0), ("$n0", -1, 0, 0),
       ("$nb1", 1, 0, 0), ("$nb2", 1, 0, 0)]
for _v in ("lg", "lg_changed", "lg_flags"):
    HOOKS[f"loadGame_{_v[2:].lstrip('_') or 'a'}"] = _h(0x0040B9B0, f"c3ep_{_v}", _LG, _LG_ST,
                                                        args=("pointer", "int", "int", "int"))

# ---------------------------------------------------------------------------------------------------- promptAndSaveFile
# 0x00437910 (index, mode, championship). fileExists 0x004378a0 and sanitizeFileName 0x00405ac0 run for real (the
# first through the directory stand-in's FindFirstFileA, the second only edits the record name); fopen / fwrite /
# fclose go through the file stand-in; every other callee is a recording stub.
_PSF_ST = (TEXT + [("$fsblock", 0, FSB), ("$rec", 0, REC)] + IMPLOG
           + [(0x00567328, 0, 0x40), (0x004D6088, 0, 0x690), (0x00543D10, 0, 0xE86), (0x005795A8, 0, 0x400),
              (0x005A5A04, 0, 0x14), (0x004C7B34, 0, 4), (0x005AA564, 0, 4)])
_PSF = [(0, -1, 0), (0, -2, 0), (0, 0, 0), (0, 1, 1), (1, -1, 0), (1, 3, 0), (2, 0, 0), (0, 3, 0),
        (1, 0, 1), (2, -2, 0), (0, -1, 1), (1, -2, 0)]
for _k, _v in (("a", "psf"), ("cancel", "psf_cancel"), ("cache", "psf_cache")):
    HOOKS[f"promptAndSaveFile_{_k}"] = _h(0x00437910, f"c3ep_{_v}", _PSF, _PSF_ST, args=("int", "int", "int"))

# ---------------------------------------------------------------------------------------------------- EditBox::create
# 0x00486110 (this, x, y, w, h, parent, unused), `ret 0x18`. Real objects from uitree.js: $e0..$e2 edit boxes built by
# EditBox::ctor 0x00486070, $p0 a parent with bit 12 of +0x9c set, $p1 one with it clear, parent 0 the early return.
UI_GLOBALS = [0x0083AB94, 0x0083AB90, 0x0083AAE0, 0x0083AAEC, 0x0083AAA0, 0x0083AABC, 0x0083AAC0, 0x0083AAC4, 0x0083AA98,
              0x0083AAF0, 0x0083AB30, 0x0083AB34, 0x0083AB40, 0x0083AB44, 0x0083AB54, 0x0083AC18, 0x00839650, 0x0083AB2C,
              0x0083AA9C, 0x0083A2C8, 0x0083A2CC, 0x0083A2D0, 0x0083A2D4, 0x0083AD50, 0x0083AD4C]
UI_ARENA = 0x30000


def UI_STATE():
    return ([("$heap", 0, 0x10 + UI_ARENA), ("$freelog", 0, 0x404), ("$implog", 0, 0x400), ("$rec", 0, 0x1000)]
            + [(g, 0, 4) for g in UI_GLOBALS] + [(0x0083A2D8, 0, 0x200)])


# the focus globals currentFocusOwner 0x0047f2f0 reads and the one it writes (0x0047f31a)
_EB_FOCUS = [(0x0083B990, 0, 4), (0x0083AB38, 0, 4), (0x0083AB60, 0, 4), (0x0083AB3C, 0, 4)]
_EB = [("$e0", 10, 20, 100, 40, "$p0", 0), ("$e1", 0, 0, 60, 20, "$p1", 0), ("$e2", -5, 7, 200, 18, "$p0", 1),
       ("$e0", 1, 1, 1, 1, 0, 0), ("$e1", 3, 4, 5, 6, 0, 7), ("$e2", 300, 200, 80, 24, "$p1", -1),
       ("$e1", 10, 20, 100, 40, "$p0", 0), ("$e0", 0, 0, 0, 0, "$p1", 0)]
for _v in ("eb", "eb_fail"):
    HOOKS[f"EditBox::create_{_v[2:].lstrip('_') or 'a'}"] = _h(
        0x00486110, f"c3ep_{_v}", _EB, UI_STATE() + _EB_FOCUS, abi="thiscall", nargs=6,
        args=("pointer", "int", "int", "int", "int", "pointer", "int"))

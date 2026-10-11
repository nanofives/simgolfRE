# C3 batch c3dz (simulation plan phase 1, 2026-10-11): the golfer text chain. Reimplementations: shim/src/re/c3dz.cpp.
# Fixtures: re/frida/js/fixtures.d/c3dz.js. Purposes and per-jcc tables: log/c3/c3dz_purpose.md.
#
# Every key freezes the game's first thread (re/frida/js/freeze.js) and stands callees in with recording stubs (call log
# $clog, compared). The story file is served through stand-ins of the static CRT's fopen / fgets / fclose from the fixture
# block $file (its cursor is compared state). "$rN" applies row N; "$bN".."$eN" are its further arguments.

EXE = "golf_clean.exe"

_BUF = (0x0051A068, 0, 0x800)          # the text buffer every builder appends to
_CACHE = (0x00838220, 0, 0x900)        # story-cache slot word, the eight 0x10c-byte entries, [0x00838a94], slack
_LOG = ("$clog", 0, 0x1000)
_FILE = ("$file", 0, 8)                # the served file's cursor and length


def _h(addr, fixture, vectors, state, ret="int", args=(), abi="default"):
    return dict(module=EXE, addr=addr, abi=abi, ret=ret, args=list(args), fixture=fixture, state=state, vectors=vectors)


def _rows(n, argc):
    names = "rbcde"[:argc]
    return [tuple(f"${k}{i}" for k in names) for i in range(n)]


HOOKS = {}

HOOKS["loadStoryStrings"] = _h(0x00466B70, "c3dz_lss", _rows(47, 4), [_LOG, _FILE, _BUF, _CACHE], args=("int",) * 4)

_RECS = (0x005796B8, 0, 0x800)         # golfer records 2..9 (0x005794b8 + 0x100 * g)
HOOKS["computeStoryReply"] = _h(0x004669F0, "c3dz_csr", _rows(62, 4), [_LOG, _RECS, (0x00838A94, 0, 4)], args=("int",) * 4)
HOOKS["computeStoryReply_chain"] = _h(0x004669F0, "c3dz_csrc", _rows(62, 4), [_LOG, _FILE, _RECS, _BUF, _CACHE], args=("int",) * 4)

HOOKS["advanceStory"] = _h(0x00466370, "c3dz_adv", _rows(70, 2),
                           [_LOG, _RECS, (0x005849E0, 0, 0x600), (0x0051A068, 0, 4), (0x0059B048, 0, 4), (0x00561258, 0, 4),
                            (0x00838A94, 0, 4)], args=("int",) * 2)

_GT = (0x005794B8, 0, 0x9900)          # golfer records 0..151 and the slot 0x98 (0x00582cb8)
HOOKS["announceVipGolfer"] = _h(0x0045DE80, "c3dz_vip", _rows(301, 2),
                                [("$clog", 0, 0x8000), _GT, _BUF, (0x005849E0, 0, 0xE00), (0x0056A51C, 0, 4), (0x00572CAC, 0, 4),
                                 (0x00824134, 0, 0x20)], args=("int",) * 2)

_EGT = [("$clog", 0, 0x2000), _RECS, (0x00582CB8, 0, 0x100), (0x00575AB0, 0, 0x208 * 20), (0x0053CAF0, 0, 5000), (0x00578804, 0, 2500),
        (0x005A6378, 0, 2500), (0x0056C7E4, 0, 2500), (0x00838DA0, 0, 0x120), _BUF, (0x00543D04, 0, 4), (0x0059C08C, 0, 4),
        (0x00571D38, 0, 4), (0x0055E5AC, 0, 4)]
HOOKS["emitGolferThought"] = _h(0x00467A00, "c3dz_egt", _rows(125, 3), _EGT, ret="void", args=("int",) * 3)
HOOKS["emitGolferThought_kinds"] = _h(0x00467A00, "c3dz_egtk", _rows(134, 3), _EGT, ret="void", args=("int",) * 3)
HOOKS["emitGolferThought_partner"] = _h(0x00467A00, "c3dz_egtp", _rows(27, 3), _EGT, ret="void", args=("int",) * 3)

_BGC = [("$clog", 0, 0x2000), _RECS, (0x005792B8, 0, 0x200), (0x00582CB8, 0, 0x200), _BUF, (0x00838D9C, 0, 0x20), (0x005409A8, 0, 4), (0x0058B198, 0, 4)]
HOOKS["buildGolferComment"] = _h(0x00469B00, "c3dz_bgc", _rows(205, 4), _BGC, ret="void", args=("int",) * 4)
HOOKS["buildGolferComment_grid"] = _h(0x00469B00, "c3dz_bgcg", _rows(402, 4), _BGC, ret="void", args=("int",) * 4)

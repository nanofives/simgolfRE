# C3 batch c3em (round 31, 2026-10-13) of golf_clean.exe: the modal helpers and popups of the build / amenity panels.
# Reimplementations in shim/src/re/c3em.cpp; fixtures in re/frida/js/fixtures.d/c3em.js; Purposes and per-jcc branch
# tables in log/c3/c3em_purpose.md; notes in log/c3/c3em_notes.txt. The modal.js contract: log/c3/c3el_notes.txt
# section 1.
#
# Every key runs under __uiTree (frozen first thread, recording display at 0x0083ad50, heap and import stand-ins,
# neutral window-tree globals) and under __modal (the C3 pumps stood in by the models on the A/B thread, the input a
# per-row script, the pump calls and every input global compared).
EXE = "golf_clean.exe"

# re/frida/js/uitree.js: GLOBALS (4 bytes each), the z-list array, the recorder size
UI_GLOBALS = [0x0083AB94, 0x0083AB90, 0x0083AAE0, 0x0083AAEC, 0x0083AAA0, 0x0083AABC, 0x0083AAC0, 0x0083AAC4, 0x0083AA98,
              0x0083AAF0, 0x0083AB30, 0x0083AB34, 0x0083AB40, 0x0083AB44, 0x0083AB54, 0x0083AC18, 0x00839650, 0x0083AB2C,
              0x0083AA9C, 0x0083A2C8, 0x0083A2CC, 0x0083A2D0, 0x0083A2D4, 0x0083AD50, 0x0083AD4C]
REC_SIZE = 0x1000
ARENA = 0x2000


def UI_STATE(arena=ARENA):
    return ([("$heap", 0, 0x10 + arena), ("$freelog", 0, 0x404), ("$implog", 0, 0x400), ("$rec", 0, REC_SIZE)]
            + [(g, 0, 4) for g in UI_GLOBALS] + [(0x0083A2D8, 0, 0x200)])


# re/frida/js/modal.js: __modal.STATE
MODAL_STATE = [("$modal", 0, 0x1000), (0x00822B98, 0, 8), (0x00822D58, 0, 0x40), (0x004D3834, 0, 4), (0x0083AD4C, 0, 4),
               (0x0083AFCC, 0, 4), (0x00838208, 0, 4), (0x0083821C, 0, 4), (0x0083AB9C, 0, 4)]

# c3em fixture regions: the stood-in callees' result sequences and their call log
FIX_STATE = [("$seq", 0, 16 * 0x48), ("$clog", 0, 0x1000)]

# the globals the batch's bodies read or write (the same list the fixture saves under the freeze)
BATCH_STATE = [(0x0051A068, 0, 0x100), (0x004C2848, 0, 4), (0x004C2854, 0, 4), (0x004C2E08, 0, 4),
               (0x00542F20, 0, 4), (0x0055E924, 0, 4), (0x0055E928, 0, 4), (0x00567AFC, 0, 4), (0x0056D1B0, 0, 4),
               (0x00571FD4, 0, 4), (0x005685F0, 0, 4), (0x0059E7B8, 0, 4), (0x005A6364, 0, 4), (0x005A8C38, 0, 0x40),
               (0x005A9F50, 0, 4), (0x005A9F60, 0, 4), (0x005A9F64, 0, 4), (0x00822D9C, 0, 4),
               (0x005AA55C, 0, 4), (0x0059B76C, 0, 0x20)]


def _h(addr, fixture, vectors, args=("int",), state=(), abi="default", ret="int"):
    return dict(module=EXE, addr=addr, abi=abi, ret=ret, args=list(args), fixture=fixture,
                state=UI_STATE() + MODAL_STATE + FIX_STATE + BATCH_STATE + list(state), vectors=vectors)


def _rows(n):
    return [(f"$r{i}",) for i in range(n)]


def _xy(n):
    return [(f"$r{i}", f"$y{i}") for i in range(n)]


HOOKS = {}

# ---------------------------------------------------------------------------------------------------- canAfford
HOOKS["canAfford"] = _h(0x00406C30, "c3em_ca", _rows(12))

# ---------------------------------------------------------------------------------------------- clickBuildTool433d30
HOOKS["clickBuildTool433d30"] = _h(0x00433D30, "c3em_cbt", _xy(10), args=("int", "int"))

# ------------------------------------------------------------------------------------------------ clickAmenityPanel
HOOKS["clickAmenityPanel"] = _h(0x00434AC0, "c3em_cap", _xy(19), args=("int", "int"))

# --------------------------------------------------------------------------------------------------- clickSlotPanel
HOOKS["clickSlotPanel"] = _h(0x00433040, "c3em_csp", _xy(12), args=("int", "int"))

# ----------------------------------------------------------------------------------------------------- pickBuilding
HOOKS["pickBuilding"] = _h(0x00434140, "c3em_pb", _xy(16), args=("int", "int"))

# ---------------------------------------------------------------------------------------------------- pickCharEditor
HOOKS["pickCharEditor"] = _h(0x00438390, "c3em_pce", _rows(15))

# --------------------------------------------------------------------------------------------------------- showPopup
HOOKS["showPopup"] = _h(0x0046D6E0, "c3em_sp",
                        [(f"$r{i}", f"$y{i}", f"$m{i}", f"$f{i}", f"$d{i}") for i in range(20)],
                        args=("int",) * 5)

# -------------------------------------------------------------------------------------------------- skillPointDialog
_SPD = [(0x005A5A04, 0, 0x20), (0x004C2C9C, 0, 4), (0x00582E6C, 0, 8), (0x00824134, 0, 0x20),
        (0x005794C0, 0, 0x300)]
HOOKS["skillPointDialog"] = _h(0x0045F0F0, "c3em_spd",
                               [(f"$r{i}", f"$k{i}", f"$p{i}", f"$g{i}", f"$b{i}") for i in range(17)],
                               args=("pointer", "int", "int", "int", "int"), state=_SPD)

# ----------------------------------------------------------------------------------------------- showBulletinBoard
# No arguments and no result: one key per scenario, each a single call whose global and call-log writes are the
# compared state (as c3ac's holeMagazineEvent).
_BB = [(0x004E3DB8, 0, 4), (0x004E3DBC, 0, 4), (0x008392A4, 0, 4), (0x00839338, 0, 4), (0x004C2BA0, 0, 8),
       (0x004C15A0, 0, 0x420), (0x00834170, 0, 4), (0x0059BF90, 0, 4), (0x004C2C9C, 0, 4), (0x0058316C, 0, 8),
       (0x00838F70, 0, 4), (0x00822C88, 0, 4), (0x004C2D9C, 0, 0x58), (0x008392AC, 0, 0x20)]
for _k in ("none", "photo", "camera", "clamp", "board", "award", "remap", "empty", "key", "slot4", "neg"):
    HOOKS["showBulletinBoard" + ("" if _k == "board" else "_" + _k)] = _h(
        0x0046E810, f"c3em_bb_{_k}", [()], args=(), state=_BB, ret="void")

# C3 batch c3ed (simulation plan phase 5, 2026-10-11): terrain editing, golf_clean.exe.
# Reimplementations: shim/src/re/c3ed.cpp. Fixtures: re/frida/js/fixtures.d/c3ed.js. Purposes and per-jcc tables:
# log/c3/c3ed_purpose.md.
#
# Keys freeze the game's first thread (re/frida/js/freeze.js), save every table they write and stand the heavy callees in with
# calleeStand (call log $clog). "$rN" arguments apply row N (globals, tables, scripted callee answers).
#
# Hooked callees: Random::range 0x0045c1e0, sign 0x00467150, clamp 0x00467130, clearTile 0x00470a10 run for real; the stood-in
# callees (stampCourseTiles, updateMembership, playSound, markTileKind11, rebuildTileCoverage, rebuildHeightfield, heightBlend,
# sampleHeight, tileBlocked, evalPlacementArea, spawnWalker) are replaced on the A/B thread in every key.

EXE = "golf_clean.exe"


def _g(*addrs):
    return [(a, 0, 4) for a in addrs]


def _h(addr, fixture, vectors, state, ret="int", args=(), abi="default"):
    return dict(module=EXE, addr=addr, abi=abi, ret=ret, args=list(args), fixture=fixture, state=state, vectors=vectors)


def _rows1(n):
    return [(f"$r{i}",) for i in range(n)]


HOOKS = {}

# ---------------------------------------------------------------------------------------------------- editTerrainHeight
_ETH_N = 90                                  # c3ed.js ethRows()
_ETH = ([("$clog", 0, 0x800), ("$plog", 0, 64), (0x0053CAF0, 0, 5000), (0x005722E8, 0, 2500), (0x00578804, 0, 2500), (0x005A4998, 0, 2700),
         (0x00575AB0, 0, 520 * 8), (0x005794B0, 0, 0x800)] +
        _g(0x004C2F9C, 0x004C2BA0, 0x004C2BA4, 0x005685FC, 0x0055E920, 0x005A9CD4, 0x0059E7B8, 0x004C2854, 0x004C2E04, 0x005A47E4,
           0x005685F0, 0x00822D9C, 0x00834170))
HOOKS["editTerrainHeight"] = _h(0x00409CF0, "c3ed_eth", _rows1(_ETH_N), _ETH, args=("int",))

# ---------------------------------------------------------------------------------------------------- applyTerrainEdit
_ATE_N = 222                                 # c3ed.js ateRows()
_ATE = ([("$clog", 0, 0x2000), (0x00571FF4, 0, 46 * 17), (0x0053CAF0, 0, 5016), (0x005722A8, 0, 2692), (0x0053BBAC, 0, 2516),
         (0x0053EA24, 0, 2516), (0x005A6378, 0, 2516), (0x0056C7E4, 0, 2516), (0x0056988C, 0, 2516), (0x00542414, 0, 2516),
         (0x005A4998, 0, 2700), (0x0058BCB8, 0, 0x200), (0x00585850, 0, 76 * 8)] +
        _g(0x0059BF90, 0x005A34E0, 0x004C2FA0, 0x00578150, 0x00578154, 0x00578158, 0x0057815C, 0x004C2BA0, 0x004C2BA4, 0x00822D9C))
HOOKS["applyTerrainEdit"] = _h(0x00470A60, "c3ed_ate", _rows1(_ATE_N), _ATE, args=("int",))
# the same function with heightBlend, sampleHeight and tileBlocked running for real (c3ed.js ATE_REAL: 46 of the rows)
HOOKS["applyTerrainEdit_real"] = _h(0x00470A60, "c3ed_ater", _rows1(46), _ATE, args=("int",))

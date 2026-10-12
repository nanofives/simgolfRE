# C3 batch c3ea (simulation plan phase 2, 2026-10-11): the shot physics cycle trialShot / simulateBallFlight / simulateShot.
# Reimplementations: shim/src/re/c3ea.cpp. Fixtures: re/frida/js/fixtures.d/c3ea.js. Purposes and per-jcc tables:
# log/c3/c3ea_purpose.md.
#
# Every key freezes the game's first thread, builds a synthetic course per row and stands the display / pump / thought
# callees in with recording stubs (call log $clog: counts, a running hash and the first entries). Keys that can reach
# simulateBallFlight also seed its three uninitialised stack slots through the __alloca_probe stand-in ($slot: the seeded
# values and the number of frames seeded). "$rN" applies row N; "$bN".."$eN" are its further arguments.
# Row counts: node log/c3/alt/c3ea_rowcount.js.

EXE = "golf_clean.exe"


def _h(addr, fixture, vectors, state, ret="int", args=(), abi="default"):
    return dict(module=EXE, addr=addr, abi=abi, ret=ret, args=list(args), fixture=fixture, state=state, vectors=vectors)


def _rows(n, argc):
    names = "rbcde"[:argc]
    return [tuple(f"${k}{i}" for k in names) for i in range(n)]


_STATE = [
    ("$clog", 0, 0x2000),
    (0x005796B8, 0, 0x800),     # golfer records 2..9
    (0x00582FB8, 0, 0x100),     # trialShot's record scratch
    (0x0059FC60, 0, 0x3880),    # pair snapshots
    (0x00822D9C, 0, 4),         # RNG seed
    (0x005A47B8, 0, 0x2C),      # shotPower cache (distances) and the flag byte 0x005a47e0
    (0x005685C8, 0, 0x28),      # shotPower cache (lies)
    (0x0053FD20, 0, 0x28),      # shotPower cache (values)
    (0x005A9CC4, 0, 0x28),      # 0x005a9cc4, shotPower's slot 0x005a9ce0, 0x005a9ce4, 0x005a9ce8
    (0x005A8828, 0, 0xC),       # chosen landing point
    (0x0056A78C, 0, 8),         # trial landing point
    (0x0059B738, 0, 4),
    (0x0059E7B8, 0, 4),
    (0x004C2E1C, 0, 4),
    (0x004C2FC8, 0, 8),         # overlay golfer, 0x004c2fcc
    (0x005A7140, 0, 4),
    (0x0051A068, 0, 0x100),     # text buffer (overlay)
    (0x0058A528, 0, 0x20),      # _itoa buffer (overlay)
    (0x00578370, 0, 0x6F0),     # type table (0x005786a2, 0x00578732, scanTileLine's 0x005783a2)
    (0x00575AC8, 0, 0x208 * 20),  # hole records (shot counter +0xc)
]
_SLOT = [("$slot", 0, 0x10)]

HOOKS = {}

HOOKS["trialShot"] = _h(0x004226A0, "c3ea_ts", _rows(114, 4), _STATE, args=("int",) * 4)

HOOKS["simulateShot_trial"] = _h(0x00424120, "c3ea_sst", _rows(88, 5), _STATE, args=("int",) * 5)
HOOKS["simulateShot_real"] = _h(0x00424120, "c3ea_ssr", _rows(152, 5), _STATE + _SLOT, args=("int",) * 5)
HOOKS["simulateShot_mode"] = _h(0x00424120, "c3ea_ssm", _rows(81, 5), _STATE + _SLOT, args=("int",) * 5)

HOOKS["simulateBallFlight"] = _h(0x00422FB0, "c3ea_sbf", _rows(48, 1), _STATE + _SLOT, args=("int",))
HOOKS["simulateBallFlight_overlay"] = _h(0x00422FB0, "c3ea_sbfo", _rows(14, 1),
                                         [("$clog", 0, 0x4000)] + _STATE[1:] + _SLOT, args=("int",))

# saveBmp 0x00475580 (thiscall holder, path, rect): synthetic surface and display, heap stand-in, CRT file stand-ins; the
# bytes written to the "file" ($file: cursor + bytes), the call log, the arena and the free log are compared.
HOOKS["saveBmp"] = _h(0x00475580, "c3ea_bmp", [(f"$r{i}", f"$p{i}", f"$q{i}") for i in range(27)],
                      [("$clog", 0, 0x2000), ("$file", 0, 0x1000), ("$arena", 0, 0x4010), ("$freelog", 0, 0x404)],
                      args=("pointer", "pointer", "pointer"), abi="thiscall")

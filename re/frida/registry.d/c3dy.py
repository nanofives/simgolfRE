# C3 batch c3dy (simulation plan phase 0, 2026-10-11): shared leaves of the large simulation functions.
# Reimplementations: shim/src/re/c3dy.cpp. Fixtures: re/frida/js/fixtures.d/c3dy.js. Purposes and per-jcc tables:
# log/c3/c3dy_purpose.md.
#
# Keys freeze the game's first thread (re/frida/js/freeze.js), stand callees in with calleeStand (call log $clog) and the
# clock in through importstandin.js ($implog); "$rN" / "$pN" arguments apply row N (globals and scripted sequences).

EXE = "golf_clean.exe"

_LOGS = [("$clog", 0, 0x400), ("$implog", 0, 0x400)]


def _g(*addrs):
    return [(a, 0, 4) for a in addrs]


def _h(addr, fixture, vectors, state, ret="int", args=(), abi="default"):
    return dict(module=EXE, addr=addr, abi=abi, ret=ret, args=list(args), fixture=fixture, state=state, vectors=vectors)


def _rows2(n):
    return [(f"$r{i}", f"$p{i}") for i in range(n)]


def _rows1(n):
    return [(f"$r{i}",) for i in range(n)]


HOOKS = {}

HOOKS["waitTicks"] = _h(0x0045BF80, "c3dy_wt", _rows2(16), _LOGS + _g(0x00822D68), args=("int", "int"))
HOOKS["pumpAndRedraw"] = _h(0x0045C0C0, "c3dy_par", _rows1(10), _LOGS + _g(0x00822D68, 0x00822D94, 0x00822D7C) + [("$obs", 0, 0x80)],
                            args=("int",))

# ---------------------------------------------------------------------------------------------------- sound
_SND = [("$snd", 0, 0x80), ("$rec", 0, 0x800), ("$cell", 0, 0x40)]
HOOKS["Snd_play"] = _h(0x00484940, "c3dy_sp", _rows1(15), _SND + _LOGS, args=("pointer",), abi="thiscall")
_PS = [(f"$r{i}", f"$p{i}", f"$q{i}", f"$s{i}", f"$t{i}") for i in range(13)]
HOOKS["playSound"] = _h(0x004481B0, "c3dy_ps", _PS, _SND + _LOGS, args=("int",) * 5)
_AT = [(f"$r{i}", f"$p{i}", f"$q{i}", f"$s{i}") for i in range(27)]
HOOKS["playSoundAt"] = _h(0x0040C500, "c3dy_at", _AT, [("$clog", 0, 0x400)] + _g(0x005A9CBC, 0x005A9CD8, 0x005A9CDC, 0x00822D9C),
                          args=("int",) * 4)

# ---------------------------------------------------------------------------------------------------- simulation leaves
_SEED = _g(0x00822D9C)
_N = 214                                   # c3dy.js INR: 96 + 12 + 10 + 80 rows
HOOKS["inRange"] = _h(0x004070B0, "c3dy_ir", [(f"$r{i}", f"$p{i}", f"$q{i}", f"$s{i}") for i in range(_N)], _SEED,
                      args=("int",) * 4)
_RB = [("$clog", 0, 0x1000), (0x0053CAF0, 0, 5000), (0x0058BCB8, 0, 0x1000), (0x005A8C38, 0, 0x4C)]
HOOKS["rebuildTileCoverage"] = _h(0x0042F340, "c3dy_rb", _rows1(25), _RB, ret="void", args=("int",))
_SN = [("$clog", 0, 0x8000), (0x005736B0, 0, 0x2400), (0x0056C7B4, 0, 4), (0x00822D9C, 0, 4)]
for _k, _fx in (("", "c3dy_sn"), ("_real", "c3dy_snr")):
    HOOKS["scanNearbyGolfers" + _k] = _h(0x00409A90, _fx, _rows1(12), _SN, args=("int",))
_SS = [("$clog", 0, 0x1000), (0x0056C7B4, 0, 4), (0x00822D9C, 0, 4)]
HOOKS["scanShotArea"] = _h(0x00409620, "c3dy_ss", _rows1(237), _SS, ret="void", args=("int",))
HOOKS["scanShotArea_real"] = _h(0x00409620, "c3dy_ssr", _rows1(12), _SS, ret="void", args=("int",))
HOOKS["storyText"] = _h(0x004668F0, "c3dy_st", [(f"$r{i}", f"$p{i}", f"$q{i}") for i in range(19)],
                        [("$clog", 0, 0x400), (0x0051A068, 0, 0x200)], args=("int",) * 3)

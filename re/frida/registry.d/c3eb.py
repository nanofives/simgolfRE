# C3 batch c3eb (simulation plan phase 3, 2026-10-11): placeAnimal, pathStep, updateWalkers.
# Reimplementations: shim/src/re/c3eb.cpp. Fixtures: re/frida/js/fixtures.d/c3eb.js. Purposes and per-jcc tables:
# log/c3/c3eb_purpose.md.
#
# Keys freeze the game's first thread (re/frida/js/freeze.js), stand the drawing / sound / random callees in with calleeStand
# (call log $clog); "$rN" arguments apply row N (globals, tile maps, tables and scripted sequences).

EXE = "golf_clean.exe"


def _h(addr, fixture, vectors, state, ret="int", args=(), abi="default"):
    return dict(module=EXE, addr=addr, abi=abi, ret=ret, args=list(args), fixture=fixture, state=state, vectors=vectors)


def _rows1(n):
    return [(f"$r{i}",) for i in range(n)]


def _rows5(n):
    return [(f"$r{i}", f"$b{i}", f"$c{i}", f"$d{i}", f"$e{i}") for i in range(n)]


HOOKS = {}

# ---------------------------------------------------------------------------------------------------- placeAnimal
_PA_N = 169                                # c3eb.js animalRows()
_PA = [("$clog", 0, 0x400), (0x0056D1D8, 0, 100 * 0x3C), (0x004E9A80, 0, 4), (0x004E9A70, 0, 0x20), (0x0051A068, 0, 0x40)]
HOOKS["placeAnimal"] = _h(0x004017D0, "c3eb_pa", _rows1(_PA_N), _PA, ret="void", args=("int",))

# ---------------------------------------------------------------------------------------------------- pathStep
_PS_N = 279                                # c3eb.js psRows()
_PS = [("$clog", 0, 0x2000), (0x0058D370, 0, 2500), (0x005A7404, 0, 0x800), (0x005A8028, 0, 0x800), (0x0059E7B8, 0, 4),
       (0x0051A068, 0, 0x40), (0x0058A528, 0, 0x20)]
HOOKS["pathStep"] = _h(0x0042E7E0, "c3eb_ps", _rows5(_PS_N), _PS, args=("int",) * 5)

# ---------------------------------------------------------------------------------------------------- updateWalkers
_UW_N = 152                                # c3eb.js uwRows()
_UW = [("$clog", 0, 0x4000), (0x00585850, 0, 0x1300), (0x005794B8, 0, 152 * 0x100), (0x005736B0, 0, 0x2400), (0x0053CAF0, 0, 5000),
       (0x00578804, 0, 2500), (0x00584214, 0, 0x7E0), (0x00571FD4, 0, 4), (0x0059E7B8, 0, 4)]
HOOKS["updateWalkers"] = _h(0x00402A40, "c3eb_uw", _rows1(_UW_N), _UW, args=("int",))
# the same scenes with the real pathStep (hooked reimplementation, or the original when SIMGOLF_HOOKS_OFF names 0x0042e7e0):
# the wave grid, the queues and the debug text are compared as well
_UWR = _UW + [(0x0058D370, 0, 2500), (0x005A7404, 0, 0x800), (0x005A8028, 0, 0x800), (0x0051A068, 0, 0x40), (0x0058A528, 0, 0x20)]
HOOKS["updateWalkers_real"] = _h(0x00402A40, "c3eb_uwr", _rows1(_UW_N), _UWR, args=("int",))

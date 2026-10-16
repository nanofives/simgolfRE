# C3 batch c3fza (round 37, 2026-10-16) of sound.dll: SoundWindowObj_ctor 0x00028600, dynInit_soundWindowObj100b49b8
# 0x000091c0 and __ArrayUnwind 0x0004288e. Reimplementations in shim/src/re/c3fza.cpp, fixtures in
# re/frida/js/fixtures.d/c3fza.js, Purposes and per-jcc branch coverage in log/c3/c3fza_purpose.md.
#
# Hooked callees (the verifier re-runs these keys with SIMGOLF_HOOKS_OFF so the original arm uses original callees):
#   dynInit_soundWindowObj100b49b8 -> SoundWindowObj_ctor 0x00028600 (this batch; through its thunk 0x10001c62)
#   __ArrayUnwind -> none (its filter 0x100428ec is called through its address in both arms); its caller
#                    ehVectorConstructorIterator 0x00042902 (c3fo) is re-run with this hook on and off.
_SND = dict(module="sound.dll")
_UW_STATE = [("$D", 0, 0x210), ("$arr", 0, 0x2000), ("$blk", 0x30, 4)]


def _uw(fixture, rows):
    return dict(_SND, addr=0x0004288E, abi="stdcall", ret="void", args=["pointer", "uint32", "int", "pointer"],
                fixture=fixture, state=_UW_STATE,
                vectors=[("$arrend", s, n, "$dtor" if d else 0) for s, n, d in rows])


HOOKS = {
    "SoundWindowObj_ctor": dict(_SND, addr=0x00028600, abi="thiscall", ret="uint32", args=["pointer"], fixture="c3fza_ctor",
                                state=[(f"$o{i}", 0, 0x20) for i in range(12)],
                                vectors=[(f"$o{i}",) for i in range(12)]),
    "dynInit_soundWindowObj100b49b8_ok": dict(_SND, addr=0x000091C0, abi="default", ret="int", args=[],
                                              fixture="c3fza_dyn_ok", state=[("$g", 0, 0x10), ("$log", 0, 0x40)],
                                              vectors=[()]),
    "dynInit_soundWindowObj100b49b8_fail": dict(_SND, addr=0x000091C0, abi="default", ret="int", args=[],
                                                fixture="c3fza_dyn_fail", state=[("$g", 0, 0x10), ("$log", 0, 0x40)],
                                                vectors=[()]),
    "sound___ArrayUnwind": _uw("c3fza_unwind_none", [
        (4, 0, 1), (4, 1, 1), (4, 5, 1), (8, 3, 1), (0x2C, 4, 1), (0, 3, 1), (4, -1, 1), (4, -5, 1), (12, 10, 1),
        (0x10C, 2, 1), (1, 7, 1), (0x40, 40, 1), (0xFFFFFFFC, 3, 1), (4, 0, 0), (4, -3, 0)]),
    "sound___ArrayUnwind_t0": _uw("c3fza_unwind_t0", [(4, 1, 1), (8, 3, 1), (0x2C, 2, 1), (4, 0, 1), (4, -2, 1), (4, 40, 1)]),
    "sound___ArrayUnwind_t2": _uw("c3fza_unwind_t2", [
        (4, 3, 1), (4, 2, 1), (4, 1, 1), (8, 8, 1), (0x2C, 5, 1), (0, 6, 1), (0x10C, 4, 1), (12, 2, 1), (4, 0, 1)]),
}

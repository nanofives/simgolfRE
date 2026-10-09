# C3 batch c3cq (round 22, 2026-10-09) of golf_clean.exe: CRT static-initialisation rows (atexit registrations,
# static constructors of global objects and arrays) and a few CRT helpers. Reimplementations in
# shim/src/re/c3cq.cpp, fixtures in re/frida/js/fixtures.d/c3cq.js, Purposes in log/c3/c3cq_purpose.md; the
# uniform entries are emitted by log/c3/c3cq_gen.py.
#
# Hooked callees (re-run these keys with SIMGOLF_HOOKS_OFF so the original arm uses original callees):
#   staticInit_004043f0 .. staticInit_00404840 (14) -> 004837f0 C4837f0::ctor (C3, c3p)
#   staticCtor_43ccb0 -> 00487000 BinkPlayer::ctor (C3, c3c)
#   staticCtor_45ba60 -> 00488490 Cursor::ctor (C3, c3c)
#   staticInit_g83ad58 -> 00487a20 NodeListB::ctor, staticInit_g83af98 -> 00487b40 NodeListC::ctor (C3, c3n)

# atexit registrations: the exit-table end/begin pointers and the slot at the table end ($slot from the fixture).
_ONEXIT_STATE = [(0x0084216C, 0, 8), ("$slot", 0, 4)]


def _onexit_entry(addr):
    return dict(module="golf_clean.exe", addr=addr, abi="default", ret="int", args=[],
                fixture="c3cq_onexit", state=_ONEXIT_STATE, vectors=[(), (), ()])


def _global_ctor_entry(addr, g, size):
    return dict(module="golf_clean.exe", addr=addr, abi="default", ret="pointer", args=[],
                fixture=f"c3cq_g{g:06x}", state=[(g, 0, size)], vectors=[(), (), ()])


def _global_array_entry(addr, base, size, nvec):
    return dict(module="golf_clean.exe", addr=addr, abi="default", ret="void", args=[],
                fixture=f"c3cq_g{base:06x}", state=[(base, 0, size)], vectors=[()] * nvec)


HOOKS = {
    "registerExit_4378f0": _onexit_entry(0x004378F0),
    "registerExit_4385b0": _onexit_entry(0x004385B0),
    "registerExit_43cc30": _onexit_entry(0x0043CC30),
    "registerExit_43cc80": _onexit_entry(0x0043CC80),
    "registerExit_43ccc0": _onexit_entry(0x0043CCC0),
    "registerExit_43d490": _onexit_entry(0x0043D490),
    "registerExit_43d4f0": _onexit_entry(0x0043D4F0),
    "registerExit_43d650": _onexit_entry(0x0043D650),
    "registerExit_448180": _onexit_entry(0x00448180),
    "registerExit_4491d0": _onexit_entry(0x004491D0),
    "registerExit_449240": _onexit_entry(0x00449240),
    "registerExit_44aca0": _onexit_entry(0x0044ACA0),
    "registerExit_44ad40": _onexit_entry(0x0044AD40),
    "registerExit_44ad90": _onexit_entry(0x0044AD90),
    "registerExit_44ade0": _onexit_entry(0x0044ADE0),
    "registerExit_44aed0": _onexit_entry(0x0044AED0),
    "registerExit_44af10": _onexit_entry(0x0044AF10),
    "registerExit_44af50": _onexit_entry(0x0044AF50),
    "registerExit_44af90": _onexit_entry(0x0044AF90),
    "registerExit_44b110": _onexit_entry(0x0044B110),
    "registerExit_44b150": _onexit_entry(0x0044B150),
    "registerExit_44b190": _onexit_entry(0x0044B190),
    "registerExit_44b3c0": _onexit_entry(0x0044B3C0),
    "registerExit_44b4d0": _onexit_entry(0x0044B4D0),
    "registerExit_44b670": _onexit_entry(0x0044B670),
    "registerExit_44b770": _onexit_entry(0x0044B770),
    "registerExit_45ba70": _onexit_entry(0x0045BA70),
    "registerExit_45bac0": _onexit_entry(0x0045BAC0),
    "atexitThunk_46de50": _onexit_entry(0x0046DE50),
    "atexitThunk_473a80": _onexit_entry(0x00473A80),
    "atexitThunk_474aa0": _onexit_entry(0x00474AA0),
    "atexitThunk_479a00": _onexit_entry(0x00479A00),
    "atexitThunk_479a50": _onexit_entry(0x00479A50),
    "atexit_482ea0": _onexit_entry(0x00482E90),
    "atexit_482ee0": _onexit_entry(0x00482ED0),
    "atexit_482f20": _onexit_entry(0x00482F10),
    "atexit_4837d0": _onexit_entry(0x004837C0),
    "atexit_483e00": _onexit_entry(0x00483DF0),
    "atexit_483e40": _onexit_entry(0x00483E30),
    "atexit_483e80": _onexit_entry(0x00483E70),
    "registerReaderAtexit": _onexit_entry(0x00487D90),
    "registerKeyTableAtexit": _onexit_entry(0x00487E30),
    "regAtexit_49c000": _onexit_entry(0x0049C000),
    "regAtexit_49d020": _onexit_entry(0x0049D020),
    "regAtexit_49d170": _onexit_entry(0x0049D170),
    "regAtexit_49d2b0": _onexit_entry(0x0049D2B0),
    "regAtexit_49d310": _onexit_entry(0x0049D310),
    "regAtexit_49d370": _onexit_entry(0x0049D370),
    "regAtexit_49d4a0": _onexit_entry(0x0049D4A0),
    "regAtexit_49d500": _onexit_entry(0x0049D500),
    "regAtexit_49d560": _onexit_entry(0x0049D560),
    "regAtexit_49eb10": _onexit_entry(0x0049EB10),
    "regAtexit_49eb70": _onexit_entry(0x0049EB70),
    "regAtexit_4a0140": _onexit_entry(0x004A0140),
    "staticInit_004043f0": _global_ctor_entry(0x004043F0, 0x00519928, 0x8),
    "staticInit_00404440": _global_ctor_entry(0x00404440, 0x00519FD8, 0x8),
    "staticInit_00404490": _global_ctor_entry(0x00404490, 0x0051B320, 0x8),
    "staticInit_004044e0": _global_ctor_entry(0x004044E0, 0x0051B360, 0x8),
    "staticInit_00404530": _global_ctor_entry(0x00404530, 0x00519948, 0x8),
    "staticInit_00404580": _global_ctor_entry(0x00404580, 0x00519A40, 0x8),
    "staticInit_004045d0": _global_ctor_entry(0x004045D0, 0x0051A028, 0x8),
    "staticInit_00404620": _global_ctor_entry(0x00404620, 0x0051B340, 0x8),
    "staticInit_00404670": _global_ctor_entry(0x00404670, 0x00519968, 0x8),
    "staticInit_004046c0": _global_ctor_entry(0x004046C0, 0x005199C8, 0x58),
    "staticInit_00404700": _global_ctor_entry(0x00404700, 0x00519A20, 0x8),
    "staticInit_00404750": _global_ctor_entry(0x00404750, 0x0051A008, 0x8),
    "staticInit_004047a0": _global_ctor_entry(0x004047A0, 0x00519988, 0x8),
    "staticInit_004047f0": _global_ctor_entry(0x004047F0, 0x005199A8, 0x8),
    "staticInit_00404840": _global_ctor_entry(0x00404840, 0x0051A048, 0x8),
    "staticCtor_4378e0": _global_ctor_entry(0x004378E0, 0x005AA560, 0x58),
    "staticCtor_43cc70": _global_ctor_entry(0x0043CC70, 0x005AA6F0, 0x58),
    "staticCtor_43ccb0": _global_ctor_entry(0x0043CCB0, 0x005AA6D0, 0x1C),
    "staticCtor_43d4e0": _global_ctor_entry(0x0043D4E0, 0x00820E90, 0x28),
    "staticCtor_4491c0": _global_ctor_entry(0x004491C0, 0x00820ED8, 0x28),
    "staticCtor_44ad80": _global_ctor_entry(0x0044AD80, 0x00821040, 0x28),
    "staticCtor_44add0": _global_ctor_entry(0x0044ADD0, 0x008210C8, 0x58),
    "staticCtor_44aec0": _global_ctor_entry(0x0044AEC0, 0x00821C60, 0x58),
    "staticCtor_44af00": _global_ctor_entry(0x0044AF00, 0x00821BF8, 0x58),
    "staticCtor_44af40": _global_ctor_entry(0x0044AF40, 0x00821070, 0x58),
    "staticCtor_44af80": _global_ctor_entry(0x0044AF80, 0x00821200, 0x108),
    "staticCtor_44b100": _global_ctor_entry(0x0044B100, 0x008223C0, 0x108),
    "staticCtor_44b140": _global_ctor_entry(0x0044B140, 0x008222B8, 0x108),
    "staticCtor_44b180": _global_ctor_entry(0x0044B180, 0x00821308, 0x8F0),
    "staticCtor_44b3b0": _global_ctor_entry(0x0044B3B0, 0x00821F48, 0x370),
    "staticCtor_44b4c0": _global_ctor_entry(0x0044B4C0, 0x008224C8, 0x6B4),
    "staticCtor_44b660": _global_ctor_entry(0x0044B660, 0x00821120, 0xDC),
    "staticCtor_44b760": _global_ctor_entry(0x0044B760, 0x00821CB8, 0x210),
    "staticCtor_45ba60": _global_ctor_entry(0x0045BA60, 0x00822CC8, 0x14),
    "staticCtor_45bab0": _global_ctor_entry(0x0045BAB0, 0x00822C98, 0x28),
    "staticInit_g83acb0": _global_ctor_entry(0x00482E80, 0x0083ACB0, 0x58),
    "staticInit_g83ac30": _global_ctor_entry(0x00482EC0, 0x0083AC30, 0x58),
    "staticInit_g83ad80": _global_ctor_entry(0x00483DE0, 0x0083AD80, 0x1E4),
    "staticInit_g83ad58": _global_ctor_entry(0x00483E20, 0x0083AD58, 0x20),
    "staticInit_g83af98": _global_ctor_entry(0x00483E60, 0x0083AF98, 0x20),
    "arrayStaticInit_43cc10": _global_array_entry(0x0043CC10, 0x0081CA10, 0x4150, 3),
    "arrayStaticInit_44ad20": _global_array_entry(0x0044AD20, 0x00820F40, 0xDC, 3),
    "initKeyTableArray": _global_array_entry(0x00487E10, 0x0083B170, 0x480, 3),

    # ---------------------------------------------------------------- hand-written CRT helpers
    # errnoPtr / doserrnoPtr: _installed (this thread's per-thread block is a fixture block: result obj+8 / obj+0xc,
    # no allocation) and _fresh (slot empty: __getptd's first-call path allocates from the arena and fills the slot).
    "errnoPtr_installed": dict(module="golf_clean.exe", addr=0x004A910C, abi="default", ret="pointer", args=[],
                               fixture="c3cq_ptd_installed", state=[("$tls", 0, 4)], vectors=[(), (), ()]),
    "errnoPtr_fresh": dict(module="golf_clean.exe", addr=0x004A910C, abi="default", ret="pointer", args=[],
                           fixture="c3cq_ptd_fresh", state=[("$arena", 0, 0x90), ("$tls", 0, 4)],
                           vectors=[(), (), ()]),
    "doserrnoPtr_installed": dict(module="golf_clean.exe", addr=0x004A9115, abi="default", ret="pointer", args=[],
                                  fixture="c3cq_ptd_installed", state=[("$tls", 0, 4)], vectors=[(), (), ()]),
    "doserrnoPtr_fresh": dict(module="golf_clean.exe", addr=0x004A9115, abi="default", ret="pointer", args=[],
                              fixture="c3cq_ptd_fresh", state=[("$arena", 0, 0x90), ("$tls", 0, 4)],
                              vectors=[(), (), ()]),

    # initFpControl: a leaf; three pre-states of the table, 4 vectors each (12).
    "initFpControl_zero": dict(module="golf_clean.exe", addr=0x004A4FC4, abi="default", ret="void", args=[],
                               fixture="c3cq_fp_zero", state=[(0x004E4B00, 0, 0x18)], vectors=[()] * 4),
    "initFpControl_a5": dict(module="golf_clean.exe", addr=0x004A4FC4, abi="default", ret="void", args=[],
                             fixture="c3cq_fp_a5", state=[(0x004E4B00, 0, 0x18)], vectors=[()] * 4),
    "initFpControl_rot": dict(module="golf_clean.exe", addr=0x004A4FC4, abi="default", ret="void", args=[],
                              fixture="c3cq_fp_rot", state=[(0x004E4B00, 0, 0x18)], vectors=[()] * 4),

    # unwindCleanup_4021a0: a leaf; twelve guard objects with different saved values.
    "unwindCleanup_4021a0": dict(module="golf_clean.exe", addr=0x004021A0, abi="thiscall", ret="void",
                                 args=["pointer"], fixture="c3cq_guard",
                                 state=[(0x00839650, 0, 4)] + [(f"$o{i}", 0, 8) for i in range(12)],
                                 vectors=[(f"$o{i}",) for i in range(12)]),

    # unwindFree_4021b0: p = NULL (0x004021b6) and six blocks, with 0x00839650 zero (free recorded) or set (no free).
    "unwindFree_4021b0_g0": dict(module="golf_clean.exe", addr=0x004021B0, abi="default", ret="void",
                                 args=["pointer"], fixture="c3cq_free_g0",
                                 state=[(0x00839650, 0, 4), ("$freelog", 0, 0x24)],
                                 vectors=[(0,)] + [(f"$p{i}",) for i in range(6)]),
    "unwindFree_4021b0_gset": dict(module="golf_clean.exe", addr=0x004021B0, abi="default", ret="void",
                                   args=["pointer"], fixture="c3cq_free_gset",
                                   state=[(0x00839650, 0, 4), ("$freelog", 0, 0x24)],
                                   vectors=[(0,)] + [(f"$p{i}",) for i in range(6)]),

    # destroyGlobalReader: Reader::closeFile on 0x0083b000 with a FILE at +0x158 (fclose recorded) or none.
    "destroyGlobalReader_open": dict(module="golf_clean.exe", addr=0x00487E60, abi="default", ret="void", args=[],
                                     fixture="c3cq_reader_open",
                                     state=[(0x0083B158, 0, 4), ("$freelog", 0, 0x10)], vectors=[(), (), ()]),
    "destroyGlobalReader_closed": dict(module="golf_clean.exe", addr=0x00487E60, abi="default", ret="void",
                                       args=[], fixture="c3cq_reader_closed",
                                       state=[(0x0083B158, 0, 4), ("$freelog", 0, 0x10)], vectors=[(), (), ()]),

    # initGlobalReader: Reader::ctor(0x200) on 0x0083b000; its two malloc(0x200) come from the arena.
    "initGlobalReader": dict(module="golf_clean.exe", addr=0x00487D80, abi="default", ret="pointer", args=[],
                             fixture="c3cq_reader_init",
                             state=[(0x0083B000, 0, 0x164), ("$arena", 0, 0x410), ("$freelog", 0, 0x10)],
                             vectors=[(), (), ()]),

    # staticInit_g83ac88: Cache482f60::ctor(1) on 0x0083ac88, which saves and clears 0x00839650.
    "staticInit_g83ac88_g0": dict(module="golf_clean.exe", addr=0x00482F00, abi="default", ret="pointer", args=[],
                                  fixture="c3cq_cache_g0", state=[(0x0083AC88, 0, 0x28), (0x00839650, 0, 4)],
                                  vectors=[(), (), ()]),
    "staticInit_g83ac88_gset": dict(module="golf_clean.exe", addr=0x00482F00, abi="default", ret="pointer",
                                    args=[], fixture="c3cq_cache_gset",
                                    state=[(0x0083AC88, 0, 0x28), (0x00839650, 0, 4)], vectors=[(), (), ()]),
}

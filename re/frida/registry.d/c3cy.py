# C3 batch c3cy (round 24, 2026-10-10): the import stand-in pilot. Functions of golf_clean.exe and jgld.dll that
# earlier batches dropped only for Win32 imports. Every key's fixture installs __importStandIn
# (re/frida/js/importstandin.js) over the imports the function calls; the call log `$implog` is a state region of
# every key, so the import calls (which, in which order, with which arguments) are compared between the arms.
# Reimplementations in shim/src/re/c3cy.cpp, fixtures in re/frida/js/fixtures.d/c3cy.js, Purposes and per-jcc
# branch coverage in log/c3/c3cy_purpose.md.
#
# Hooked callees (re-run these keys with SIMGOLF_HOOKS_OFF so the original arm uses original callees):
#   MappedFile::dtor_exe, MappedFile::open_*  -> 00492e80 MappedFile::close (this batch)
#   MappedFile::dtor_jgld                     -> jgld.dll:2e10 MappedFile::close (this batch)
#   MappedFile::deletingDtor_jgld             -> jgld.dll:2820 MappedFile::dtor -> jgld.dll:2e10 (this batch)
#   Font::release_jgld                        -> jgld.dll:14a90 jfree (C3, c3cn; not linked in the c3cy instance)

_LOG = ("$implog", 0, 0x400)

# MappedFile objects: 12 x 0x20 bytes in one block (re/frida/js/fixtures.d/c3cy.js MF_ROWS)
_MF = [("$mfblk", 0, 0x20 * 12), _LOG]
_MF_DEL = _MF + [("$freelog", 0, 0x404)]
_MFV = [(f"$mf{i}",) for i in range(12)]

# releaseCursorClip: the owner global g_cursorClipOwner 0x0083aa98 (frozen first thread) and the log
_CLIP = [(0x0083AA98, 0, 4), _LOG]

HOOKS = {
    # ------------------------------------------------------------------ golf_clean.exe MappedFile family
    "MappedFile::close_exe": dict(module="golf_clean.exe", addr=0x00492E80, abi="thiscall", ret="void",
                                  args=["pointer"], fixture="c3cy_mf_exe", state=_MF, vectors=_MFV),
    "MappedFile::dtor_exe": dict(module="golf_clean.exe", addr=0x00492DC0, abi="thiscall", ret="void",
                                 args=["pointer"], fixture="c3cy_mf_exe", state=_MF, vectors=_MFV),
    # (this, name, sequential): every exit of open, both CreateFileA flag sets, and an object with nothing to close
    "MappedFile::open": dict(module="golf_clean.exe", addr=0x00492DD0, abi="thiscall", ret="pointer",
                             args=["pointer", "pointer", "int"], fixture="c3cy_mf_open", state=_MF,
                             vectors=[("$mf0", "$name0", 0), ("$mf7", "$name1", 1), ("$mf3", "$name2", 0),
                                      ("$mf5", "$name3", 1), ("$mf11", "$name4", 1), ("$mf2", "$name5", 0),
                                      ("$mf4", "$name6", 0), ("$mf6", "$name7", 1), ("$mf9", "$name8", 0),
                                      ("$mf1", "$name9", -1), ("$mf8", "$name10", 0x100), ("$mf10", "$name11", 1)]),

    # ------------------------------------------------------------------ releaseCursorClip (thiscall, no args)
    "releaseCursorClip_w0": dict(module="golf_clean.exe", addr=0x00479B20, abi="thiscall", ret="void",
                                 args=["pointer"], fixture="c3cy_clip_w0", state=_CLIP,
                                 vectors=[("$w0",), ("$w1",), ("$w3",), (0,), ("$w5",)]),
    "releaseCursorClip_w3": dict(module="golf_clean.exe", addr=0x00479B20, abi="thiscall", ret="void",
                                 args=["pointer"], fixture="c3cy_clip_w3", state=_CLIP,
                                 vectors=[("$w3",), ("$w0",), ("$w2",), (0,)]),
    "releaseCursorClip_null": dict(module="golf_clean.exe", addr=0x00479B20, abi="thiscall", ret="void",
                                   args=["pointer"], fixture="c3cy_clip_null", state=_CLIP,
                                   vectors=[(0,), ("$w0",), ("$w4",)]),

    # ------------------------------------------------------------------ fileExists (cdecl, name)
    "fileExists": dict(module="golf_clean.exe", addr=0x004378A0, abi="mscdecl", ret="int", args=["pointer"],
                       fixture="c3cy_files", state=[_LOG], vectors=[(f"$f{i}",) for i in range(13)]),

    # ------------------------------------------------------------------ Snd::busy (thiscall, no args)
    "Snd::busy_t100k": dict(module="golf_clean.exe", addr=0x00484B30, abi="thiscall", ret="int", args=["pointer"],
                            fixture="c3cy_snd_t100k", state=[("$sblk", 0, 0x80 * 12), _LOG],
                            vectors=[(f"$s{i}",) for i in range(12)]),
    "Snd::busy_t5": dict(module="golf_clean.exe", addr=0x00484B30, abi="thiscall", ret="int", args=["pointer"],
                         fixture="c3cy_snd_t5", state=[("$sblk", 0, 0x80 * 12), _LOG],
                         vectors=[(f"$s{i}",) for i in range(12)]),

    # ------------------------------------------------------------------ jgld.dll MappedFile family
    "MappedFile::close_jgld": dict(module="jgld.dll", addr=0x00002E10, abi="thiscall", ret="void",
                                   args=["pointer"], fixture="c3cy_mf_jgld", state=_MF, vectors=_MFV),
    "MappedFile::dtor_jgld": dict(module="jgld.dll", addr=0x00002820, abi="thiscall", ret="void",
                                  args=["pointer"], fixture="c3cy_mf_jgld", state=_MF, vectors=_MFV),
    # flags bit 0 decides the operator delete (0x100026e8); seven vectors set it
    "MappedFile::deletingDtor_jgld": dict(module="jgld.dll", addr=0x000026C0, abi="thiscall", ret="pointer",
                                          args=["pointer", "uint32"], fixture="c3cy_mf_jgld_del", state=_MF_DEL,
                                          vectors=[(f"$mf{i}", f) for i, f in enumerate(
                                              [1, 0, 3, 2, 1, 0, 0xFFFFFFFF, 0xFFFFFFFE, 0x101, 0x80000000, 1, 5])]),

    # ------------------------------------------------------------------ jgld.dll Font::release
    "Font::release_jgld": dict(module="jgld.dll", addr=0x00068DA0, abi="thiscall", ret="void", args=["pointer"],
                               fixture="c3cy_font", state=[("$ftblk", 0, 0x40 * 12), _LOG, ("$freelog", 0, 0x404)],
                               vectors=[(f"$ft{i}",) for i in range(12)]),
}

# Argument-free functions: one key per stand-in setting (one vector each).
for _v in range(10):
    HOOKS[f"cacheCursorMetrics_v{_v}"] = dict(module="golf_clean.exe", addr=0x004884E0, abi="mscdecl", ret="int",
                                             args=[], fixture=f"c3cy_metrics_{_v}",
                                             state=[(0x0083B5F8, 0, 8), _LOG], vectors=[()])
    HOOKS[f"isPrimaryMouseDown_v{_v}"] = dict(module="golf_clean.exe", addr=0x0047ABC0, abi="mscdecl", ret="int",
                                             args=[], fixture=f"c3cy_mouse_{_v}", state=[_LOG], vectors=[()])
    HOOKS[f"seedFromTime_v{_v}"] = dict(module="golf_clean.exe", addr=0x00467110, abi="mscdecl", ret="uint32",
                                       args=[], fixture=f"c3cy_seed_{_v}",
                                       state=[(0x0051B380, 0, 4), (0x00822D9C, 0, 4), _LOG], vectors=[()])

# ---------------------------------------------------------------------- golf_clean.exe Timer family
# Twelve Timer objects $t0..$t11 (TIMER_ROWS in fixtures.d/c3cy.js); the recording display's +8 / +0xc counters,
# the callback counter, the input-phase global 0x0083ad4c that pumpInputDisplay483da0 0x00483da0 writes, and the log.
_TIMER = [("$tblk", 0, 0x40 * 12), ("$disp", 4, 0xC), ("$cbctr", 0, 4), (0x0083AD4C, 0, 4), _LOG]
_TV = [(f"$t{i}",) for i in range(12)]


def _t(addr, fixture, vectors, ret="void", args=("pointer",), abi="thiscall"):
    return dict(module="golf_clean.exe", addr=addr, abi=abi, ret=ret, args=list(args), fixture=fixture,
                state=_TIMER, vectors=vectors)


# (this, a1..a5) for init / initStartOnce / initStartRepeat: a1 = the +0x14 target (0 keeps both targets 0 after the
# reset, so start returns 7), a4 = the interval (10 mm, 13 mm failure, 0x31 / 0x32 boundary, 1000 SetTimer failure)
_INIT5 = [("$t0", 1, 2, 3, 10, 5), ("$t1", 0, 2, 3, 10, 5), ("$t2", 7, 0, 0, 13, 1), ("$t3", 1, 1, 1, 0x31, 0),
          ("$t4", 1, 9, 8, 0x32, 5), ("$t5", -1, -2, -3, 1000, -4), ("$t6", 0x1234, 0, 0, 0xFFFFFFFF, 5),
          ("$t7", 3, 4, 5, 0, 6), ("$t8", 0, 0, 0, 0x32, 0), ("$t9", 2, 2, 2, 49, 2), ("$t10", 5, 6, 7, 50, 8),
          ("$t11", 1, 0, 0, 0x80000000, 5)]
_INIT4 = [("$t0", 1, 2, 10, 5), ("$t1", 0, 2, 10, 5), ("$t2", 7, 0, 13, 1), ("$t3", 1, 1, 0x31, 0),
          ("$t4", 1, 9, 0x32, 5), ("$t5", -1, -2, 1000, -4), ("$t6", 0x1234, 0, 0xFFFFFFFF, 5), ("$t7", 3, 4, 0, 6),
          ("$t8", 0, 0, 0x32, 0), ("$t9", 2, 2, 49, 2), ("$t10", 5, 6, 50, 8), ("$t11", 1, 0, 0x80000000, 5)]
# Timer::callback(id, msg, user, dw1, dw2), stdcall: user = each Timer, plus NULL
_CBV = [(0x7101, 0, f"$t{i}", 0, 0) for i in range(12)] + [(0x7100, 0, 0, 0, 0)]

HOOKS.update({
    "Timer::stop": _t(0x00486EC0, "c3cy_timer", _TV, ret="int"),
    "Timer::stop_nodisp": _t(0x00486EC0, "c3cy_timer_nodisp", _TV, ret="int"),
    "Timer::reset": _t(0x00486F10, "c3cy_timer", _TV, ret="int"),
    "Timer::reset_nodisp": _t(0x00486F10, "c3cy_timer_nodisp", _TV, ret="int"),
    "Timer::dtor": _t(0x00486CE0, "c3cy_timer", _TV, ret="int"),
    "Timer::startOnce": _t(0x00486DC0, "c3cy_timer", _TV, ret="int"),
    "Timer::startRepeat": _t(0x00486E40, "c3cy_timer", _TV, ret="int"),
    "Timer::init": _t(0x00486D20, "c3cy_timer", _INIT5, args=["pointer"] + ["int"] * 5),
    "Timer::init_nodisp": _t(0x00486D20, "c3cy_timer_nodisp", _INIT5, args=["pointer"] + ["int"] * 5),
    "Timer::initA": _t(0x00486CF0, "c3cy_timer", _INIT4, args=["pointer"] + ["int"] * 4),
    "Timer::initStartOnce": _t(0x00486D60, "c3cy_timer", _INIT5, ret="int", args=["pointer"] + ["int"] * 5),
    "Timer::initStartRepeat": _t(0x00486D90, "c3cy_timer", _INIT5, ret="int", args=["pointer"] + ["int"] * 5),
    "Timer::callback": _t(0x00486F90, "c3cy_timer", _CBV, args=["uint32", "uint32", "pointer", "uint32", "uint32"],
                          abi="stdcall"),
    "Timer::callback_own3": _t(0x00486F90, "c3cy_timer_own3", _CBV,
                               args=["uint32", "uint32", "pointer", "uint32", "uint32"], abi="stdcall"),
    "Timer::callback_own1": _t(0x00486F90, "c3cy_timer_own1", _CBV,
                               args=["uint32", "uint32", "pointer", "uint32", "uint32"], abi="stdcall"),
    "Timer::callback_other": _t(0x00486F90, "c3cy_timer_other", _CBV,
                                args=["uint32", "uint32", "pointer", "uint32", "uint32"], abi="stdcall"),
})

# ---------------------------------------------------------------------- jgld.dll Font destructor chain
_FC = [("$fblk", 0, 0x800), ("$glist", 4, 0x18), _LOG, ("$freelog", 0, 0x404)]
_FLAGS = [1, 0, 3, 2, 1, 0, 0xFFFFFFFF, 0xFFFFFFFE, 0x101, 0x80000000, 1, 5]
for _shape in ("one", "five"):
    _fx = f"c3cy_fontchain_{_shape}"
    HOOKS.update({
        f"Font::~Font_{_shape}": dict(module="jgld.dll", addr=0x00068760, abi="thiscall", ret="void",
                                      args=["pointer"], fixture=_fx, state=_FC,
                                      vectors=[(f"$ft{i}",) for i in range(12)]),
        f"Font::deletingDtor_{_shape}": dict(module="jgld.dll", addr=0x000691B0, abi="thiscall", ret="pointer",
                                             args=["pointer", "uint32"], fixture=_fx, state=_FC,
                                             vectors=[(f"$ft{i}", _FLAGS[i]) for i in range(12)]),
        f"Font::deletingDtor_adj8_{_shape}": dict(module="jgld.dll", addr=0x00069260, abi="thiscall",
                                                  ret="pointer", args=["pointer", "uint32"], fixture=_fx,
                                                  state=_FC, vectors=[(f"$tt{i}", _FLAGS[i]) for i in range(12)]),
    })

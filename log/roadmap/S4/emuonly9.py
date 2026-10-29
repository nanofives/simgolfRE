"""S4.9's emu-only rows and the registry keys the fixture VM cannot run (imported by 9_replay.py): DERIVED8 = twin keys with the same
argument slots whose body is a constant (measured instruction shape), CUSTOM8 = registry-shaped specs whose fixture is defined in
9_fixtures.js, EXCLUDED8 = registry keys replaced by custom ones. (The names keep S4.8's spelling: 9_replay.py is a copy of 8_replay.py.)"""

DERIVED8 = {
    # `mov eax, 0xb; ret 4` (rva 0xe350): the (this, uint32) slots of setField6cReturn0's key, as S4.8's stubReturn2fB
    "stubReturnBA@": ("setField6cReturn0", 0xe350),
    # `mov eax, 0xb; ret` (rva 0x32660): no argument; release_sound_null's (no-argument, cdecl) key, as S4.8's stubReturn2fA
    "return0xb_32660@": ("release_sound_null", 0x32660),
}

_WAVECB_VEC = [(1, m, i, p, 9) for m in (0x3C0, 0x3BF, 0x3C1, 0x123C0, 0xFFFF03C0, 0, 0xFFFFFFFF, 0x13C0)
               for i, p in ((0x1234, 0x5678), (0, 0xFFFFFFFF))]
_OUTVOL_STATE = ([("$dev", 0, 0x200)] + [(f"$b{i}", 0, 0xd4) for i in range(4)] + [(f"$c{i}", 0, 0x40) for i in range(4)]
                 + [("$gl", 0, 0xc)])
_OUTVOL_VEC = [(v,) for v in (0, 1, 0x40, 0x7f, 0x80, 0xffffffff, 0x3f, 0x7e, 2, 0x12345)]

CUSTOM8 = {}
for _v in ("eng", "eng0", "noeng", "noeng1"):
    CUSTOM8[f"onWaveInCallbackMessage@{_v}"] = dict(
        module="sound.dll", addr=0x38fd0, abi="stdcall", ret="void", args=["uint32"] * 5, fixture=f"s49_wavecb_{_v}",
        state=[("$implog", 0, 0x400), ("$tstate", 0, 0x30)], vectors=list(_WAVECB_VEC))
CUSTOM8["setOutputMasterVolume@dev"] = dict(module="sound.dll", addr=0xe2a0, abi="stdcall", ret="uint32", args=["uint32"],
                                            fixture="s49_outvol_dev", state=_OUTVOL_STATE, vectors=list(_OUTVOL_VEC))
# the NULL device: eax is whatever the caller left (the replay's entry eax), so the key compares state only (ret void)
CUSTOM8["setOutputMasterVolume@null"] = dict(module="sound.dll", addr=0xe2a0, abi="stdcall", ret="void", args=["uint32"],
                                             fixture="s49_outvol_null", state=_OUTVOL_STATE, vectors=list(_OUTVOL_VEC))

EXCLUDED8 = {
    "onWaveInCallbackMessage": "S4.1 census FIXTURE_ERR: c3dh_wavecb waits for the live game's sound engine (flag bit 5); "
                               "replaced by onWaveInCallbackMessage@eng / @eng0 / @noeng / @noeng1",
}

# keys whose original copies doubles through the x87 (fld qword / fst qword): the emulator keeps a signaling NaN signaling, the CPU
# quiets it (S4.6's U-1451); a difference that is exactly that quiet bit is forgiven (quirk9.py)
QUIRK_KEYS = ("closeMmioSound",)

# ---- the stereo / chunk writers: keys over fixtures of c3fl (writeWaveFile's), c3fn (writeWaveChunk's) and 9_fixtures.js -------------
import pathlib as _pl

_ROOT = _pl.Path(__file__).resolve().parents[3]


def _registry_hooks(name):
    ns = {}
    exec(compile((_ROOT / "re" / "frida" / "registry.d" / f"{name}.py").read_text(encoding="utf-8"), name, "exec"), ns)
    return ns["HOOKS"]


try:
    _C3FL = _registry_hooks("c3fl")
    # writeWaveFileB (this, buf, buf2): ten vectors over the six buffers of c3fl_wf: NULL buffers, getPosition error (b0 = 7), the
    # 0x2d case (b3), a lockCaptureRegion error (b4 as the right buffer), and the normal ones
    _WFB_VEC = [("$w0", 0, "$b1"), ("$w0", "$b1", 0), ("$w0", "$b0", "$b1"), ("$w1", "$b3", "$b2"), ("$w2", "$b1", "$b4"),
                ("$w0", "$b2", "$b3"), ("$w1", "$b4", "$b5"), ("$w2", "$b5", "$b1"), ("$w0", "$b1", "$b2"), ("$w1", "$b2", "$b5")]
    for _k in ("noeng", "empty", "nullobj", "k6", "k4", "k1", "k1b", "mix"):
        CUSTOM8[f"writeWaveFileB@{_k}"] = dict(_C3FL[f"writeWaveFile_{_k}"], addr=0x75e0, args=["pointer", "pointer", "pointer"],
                                               fixture=f"s49_wfb_{_k}", vectors=list(_WFB_VEC))
except Exception as _e:        # the registry fragment is missing: the key list shrinks, the oracle reports it
    print("emuonly9: c3fl registry not loaded:", _e)

_MM = [("$mmblock", 0, 0x8000), ("$implog", 0, 0x400)]
_TAIL = [("$log", 0, 0x400)] + _MM + [("$arena", 0, 0x10 + 0x100), ("$freelog", 0, 0x404), ("$gmmio", 0, 4)]
# writeWaveChunk (this, a0, a1, a2, a3): c3fn_wwc's ten writers; the rows with a zero block align or a zero first byte count are left out
# (the original's loop `dec ecx; jne` runs 2^32 times for a zero count: U-1481)
_WWC_ROWS = (0, 1, 2, 4, 5, 6, 7, 9)
CUSTOM8["writeWaveChunk@wwc"] = dict(
    module="sound.dll", addr=0x7df0, abi="thiscall", ret="uint32", args=["pointer"] * 5, fixture="c3fn_wwc",
    state=[("$objs", 0, 10 * 0x400), ("$bufs", 0, 10 * 0x300), ("$regs", 0, 20 * 0x200), ("$srcs", 0, 10 * 0x200),
           ("$mixes", 0, 10 * 0x100)] + _TAIL,
    vectors=[(f"$o{i}", f"$a0{i}", f"$a1{i}", 0, 0) for i in _WWC_ROWS])
# writeWaveChunkB (this, a0, a1, src, A4, A5, A6): 9_fixtures.js s49_wcb, eight rows
CUSTOM8["writeWaveChunkB@wcb"] = dict(
    module="sound.dll", addr=0x8070, abi="thiscall", ret="uint32", args=["pointer"] * 7, fixture="s49_wcb",
    state=[("$objs", 0, 8 * 0x400), ("$bufsL", 0, 8 * 0x300), ("$bufsR", 0, 8 * 0x300), ("$regs", 0, 8 * 4 * 0x200),
           ("$srcs", 0, 8 * 0x400), ("$mixes", 0, 8 * 0x100)] + _TAIL,
    vectors=[(f"$o{i}", f"$l{i}", f"$r{i}", f"$s{i}", 0x11, 0x22, 0x33) for i in range(8)])

# ---- Wave_Device::open / ::start over the scenario fixtures of 9_fixtures.js (wdev) -----------------------------------------------
_WD_STATE = ([("$self", 0, 0x1c0), ("$dev", 0, 0x200), ("$stream", 0, 0x40), ("$prim", 0, 0x100), ("$s1", 0, 0x100), ("$s2", 0, 0x100),
              ("$timer", 0, 0x20), ("$gl", 0, 0x88), ("$log", 0, 0x400), ("$arena", 0, 0x10 + 0x400), ("$freelog", 0, 0x404)])
for _k in ("nodev_ok", "nodev_enumfail", "nodev_createfail", "nodev_notimer", "dev_cbfail", "dev_nofmt", "dev_fmt1fail", "dev_fmt2fail",
           "dev_fmt_nocom", "dev_prim", "dev_secbufs", "dev_nox60"):
    CUSTOM8[f"Wave_Device::open@{_k}"] = dict(module="sound.dll", addr=0xb420, abi="thiscall", ret="uint32", args=["pointer", "uint32", "uint32"],
                                              fixture=f"s49_wdev_{_k}", state=list(_WD_STATE),
                                              vectors=[("$self", 0x77, f) for f in (0, 0x80, 0x11, 0x91)])
for _k in ("noflag", "nodev", "nostream", "primexists", "createfail", "cbfail", "ok", "nofmt", "fmt1fail", "fmt2fail", "nocom", "nox60",
           "x60zero", "prim_ok", "plain"):
    CUSTOM8[f"Wave_Device::start@{_k}"] = dict(module="sound.dll", addr=0xbaa0, abi="thiscall", ret="uint32", args=["pointer"],
                                               fixture=f"s49_wdevs_{_k}", state=list(_WD_STATE), vectors=[("$self",)])

# ---- Wave_Device::dispatchCommand over 16 generated devices (9_fixtures.js s49_wdgen) -------------------------------------------------
for _k in ("post", "thread"):
    CUSTOM8[f"Wave_Device::dispatchCommand@gen_{_k}"] = dict(
        module="sound.dll", addr=0xcd80, abi="thiscall", ret="uint32", args=["pointer"], fixture=f"s49_wdgen_{_k}",
        state=[("$arena", 0, 0x500000), ("$heaparena", 0, 0x2010), ("$log", 0, 0x1000), ("$freelog", 0, 0x404), ("$implog", 0, 0x400),
               ("$geng", 0, 4)],
        vectors=[(f"$dev{i}",) for i in range(27)])

# ---- fillDecodeRing, extra rows (9_fixtures.js s49_fdr_x1 / x2: two 6-vector keys, the stand-in allows 8 buffer slots) --------------------
for _k, _n in (("x1", 6), ("x2", 8)):
    CUSTOM8[f"fillDecodeRing@{_k}"] = dict(
        module="sound.dll", addr=0x33300, abi="thiscall", ret="uint32", args=["pointer"], fixture=f"s49_fdr_{_k}",
        state=[("$objs", 0, _n * 0x1400), ("$rings", 0, _n * 0x100), ("$rbufs", 0, _n * 0x200), ("$gmmio", 0, 4), ("$mmblock", 0, 0x8000),
               ("$implog", 0, 0x400)],
        vectors=[(f"$o{i}",) for i in range(_n)])

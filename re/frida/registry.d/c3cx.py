# Batch c3cx (round 23): sound.dll never-offered audio and CRT rows (log/c3/c3cx_candidates.tsv). Module-aware hooks;
# addr is the RVA, VA = 0x10000000 + RVA. Reimplementations in shim/src/re/c3cx.cpp, fixtures in
# re/frida/js/fixtures.d/c3cx.js, purposes and branch tables in log/c3/c3cx_purpose.md.
#
# Every object, vtable, interface, node, string, region and page is fixture-owned; nothing is allocated or freed by an
# arm and no import is called. Two module globals are written by fixtures and restored at the flush:
#   g_soundTimer 0x100b49ec  (c3cx_timer_*: swapped for a fixture timer of period 10 / 4096 / 1, or null, with the
#                            game's first thread frozen);
#   0x100b5590               (c3cx_sbh_*: the old small-block heap's counter, unused by the running system heap).
#
# HOOKED CALLEES (the verifier re-runs these keys with the hooks off):
#   inside this batch (added): MultiSound_getStatus -> Channel_collectFlagBits (sound.dll:2c0d0);
#     MultiSound::setVolume -> Channel_setField4Masked7f (sound.dll:2c370);
#     MultiSound_setPitchCents -> Channel_setField5cClamped1200 (sound.dll:2bff0).
#   other batches (added): MultiSound::setVolume -> Channel_setPanGains 0x35340 (through Channel_setRate);
#     MultiSound_setPitchCents -> Channel_setPitchRate 0x35750 (c3ca).
#   inside this batch: MultiSound::set3DPositionX/Y/Z -> Channel_set3DPositionX/Y/Z (sound.dll:37b40, 37b80, 37bc0);
#     Sound::scaleTickRate -> Seq_scaleTickRate (sound.dll:1e630);
#     MultiSound::setRateGlide -> Channel_setRateGlide (sound.dll:34c80).
#   other batches (at C3): MultiSound_stop -> Channel_stopVoices 0x34b90; MultiSound_findStreamNode ->
#     Channel_findStreamNode 0x35770; MultiSound_setVoiceBit1On -> Channel_broadcastVoiceBit1 0x377f0;
#     MultiSound::applyFlags -> Channel_applyFlags 0x2c040 (c3cl); ostreamResetVtable -> iosResetVtable 0x41aad (c3cm).

_F32 = [0x00000000, 0x3F800000, 0xBF800000, 0x42C80000, 0x7F800000, 0x12345678]
_SHAPES = ["off", "offhi", "nobuf", "nocom", "ok0", "okE", "okN"]
_C3D = [(f"$c_{s}", v) for s in _SHAPES for v in _F32]
_M3D = [(f"$m_{s}", v) for s in _SHAPES for v in _F32]
_C3D_STATE = [(f"$c_{s}", 0, 0x400) for s in _SHAPES]
_M3D_STATE = [(f"$m_{s}", 0, 0x400) for s in _SHAPES]
_TRIPLES = [(0x3F800000, 0x40000000, 0x40400000), (0, 0, 0), (0xBF800000, 0x7F800000, 0x12345678),
            (0xFFFFFFFF, 1, 0x80000000)]
_M3DPOS = [(f"$m_{s}",) + t for s in _SHAPES for t in _TRIPLES]
_M3DBUF = [(f"$m_{s}", v) for s in _SHAPES for v in (0, 1, 2, 0xFFFFFFFF, 0x1234)]

# --- timer family: one key per fixture variant (period 10, 4096, 1, null g_soundTimer).
_TARGS = [0, 1, 9, 10, 11, 99, 4096, 4097, 100000, 0xFFFFFFFF]
_SEQ = [(f"$q{i}", a) for i in range(6) for a in _TARGS]
_SEQ_STATE = [(f"$q{i}", 0, 0x400) for i in range(6)]
_SND = [(f"$s{i}", a) for i in range(3) for a in _TARGS]
_SND_STATE = [(f"$s{i}", 0, 0x400) for i in range(3)]
_GPAIRS = [(-1, -1), (0x8000, 0x10000), (0x10000, 0x8000), (-1, 0), (0x4000, -1), (0, 0), (100, 0x7FFFFFFF),
           (-200, 300)]
_GDUR = [0, 5, 11, 4097, 100000]
_GLIDE = [(f"$g{i}", a, b, d) for i in range(4) for a, b in _GPAIRS for d in _GDUR]
_GLIDE_STATE = [(f"$g{i}", 0, 0x400) for i in range(4)]
_MPAIRS = [(0, 0), (-1, -1), (5, 64), (0x80, -3), (0x7F, 0), (-1, 5), (0x7FFFFFFF, 1), (-3, -1), (2, 0x80),
           (-1, 0x7FFFFFFF)]
_MGLIDE = [(f"$h{i}", a, b, d) for i in range(4) for a, b in _MPAIRS for d in (0, 11, 100000)]
_MGLIDE_STATE = [(f"$h{i}", 0, 0x400) for i in range(4)]
_REV = [(f"$r{i}", a) for i in range(4) for a in (0, 1, 10, 11, 4097, 100000, 0xFFFFFFFE)]
_REV_STATE = [(f"$r{i}", 0, 0x400) for i in range(4)]
_VARIANTS = [("", "c3cx_timer_p10"), ("_p4096", "c3cx_timer_p4096"), ("_p1", "c3cx_timer_p1"),
             ("_null", "c3cx_timer_null")]

# --- MultiSound flag slots (fixture c3cx_ms)
_E = [(f"$e{i}",) for i in range(12)]
_E_STATE = [(f"$e{i}", 0, 0x1800) for i in range(12)]
_AFLAGS = [0, 1, 2, 3, 4, 0x10, 0x20, 0x40, 0x80, 0x100, 0x200, 0x400, 0x7FF, 0xFFFFFFFF, 0x800]
_A = [(f"$a{i}", f) for i in range(4) for f in _AFLAGS]
_A_STATE = [(f"$a{i}", 0, 0x1800) for i in range(4)]
_T = [(f"$t{i}",) for i in range(7)]
_T_STATE = [(f"$t{i}", 0, 0x1800) for i in range(7)]
_FS = [(f"$f{i}",) for i in range(7)]
_FS_STATE = [(f"$f{i}", 0, 0x1800) for i in range(7)]
_V = [(f"$v{i}",) for i in range(8)]
_V_STATE = [(f"$v{i}", 0, 0x1800) for i in range(8)]

# --- Channel_allocAndInitVoice (fixture c3cx_alloc)
_X = [(f"$x{i}", f"$x{i}o") for i in range(12)]
_X_STATE = [(f"$x{i}", 0, 0x1400) for i in range(12)]

# --- getDeviceCaps / getNodeByIndex (fixture c3cx_dev)
_D = [(f"$d{i}",) for i in range(12)]
_D_STATE = [(f"$d{i}", 0, 0x200) for i in range(12)]
_N = [(f"$n{i}", idx, f"$n{i}b", cnt) for i in range(6) for idx in (0, 1, 2, 3, 4, 0xFFFFFFFF) for cnt in (0, 4, 0x20)]
_N_STATE = [(f"$n{i}", 0, 0x400) for i in range(6)]

# --- Ctrl5b19c_ctor (fixture c3cx_ctrl): twelve low bytes (upper bytes garbage) and a dword.
_K = [(f"$k{i}",) + tuple((0xABCD0000 | ((i * 37 + j * 11) & 0xFF)) if i % 2 else ((i * 37 + j * 11) & 0x7F)
                          for j in range(12)) + ((0x13579BDF * (i + 1)) & 0xFFFFFFFF,)
      for i in range(10)]
_K_STATE = [(f"$k{i}", 0, 0x100) for i in range(10)]

# --- ostreamResetVtable (fixture c3cx_ostream)
_O = [(f"$o{i}t",) for i in range(12)]
_O_STATE = [(f"$o{i}", 0, 0x100) for i in range(12)]

# --- the old small-block heap (fixtures c3cx_sbh_c0 / c3cx_sbh_c1f)
_RS = [("$R", "$p0", "$A", 3), ("$R", "$p0", "$A", 0), ("$R", "$p0", "$A", 5), ("$R", "$p0", "$A", 7),
       ("$R", "$p0", "$A", 10), ("$R", "$p0", "$A", 11), ("$R", "$p0", "$B", 5), ("$R", "$p0", "$B", 10),
       ("$R", "$p0", "$B", 11), ("$R", "$p0", "$E", 0x10), ("$R", "$p0", "$E", 0x11), ("$R", "$p3", "$J", 0x10),
       ("$R", "$p3", "$J", 0xC), ("$R", "$p3", "$K", 1), ("$R", "$p3", "$K", 0), ("$R", "$p1", "$F", 2),
       ("$R", "$p2", "$H", 0), ("$R", "$p2", "$H", 1), ("$R", "$p0", "$C", 0xFF), ("$R", "$p0", "$A", 0xF0)]
_PF = [("$R", "$p1", "$F"), ("$R", "$p1", "$G"), ("$R", "$p0", "$A"), ("$R", "$p2", "$H"), ("$R", "$p3", "$K"),
       ("$R", "$p3", "$J"), ("$R", "$p0", "$C"), ("$R", "$p0", "$E"), ("$R", "$p0", "$B"), ("$R", "$p2", "$L")]
_SBH_STATE = [("$R", 0, 0x5000), ("$cnt", 0, 4)]

_SND_DLL = dict(module="sound.dll")

HOOKS = {
    # 0x10037b40 / 0x10037b80 / 0x10037bc0: callees of MultiSound slots 55 / 56 / 57. 42 vectors each.
    "Channel_set3DPositionX": dict(_SND_DLL, addr=0x00037B40, abi="thiscall", ret="int", args=["pointer", "uint32"],
                                   fixture="c3cx_3d", state=_C3D_STATE, vectors=_C3D),
    "Channel_set3DPositionY": dict(_SND_DLL, addr=0x00037B80, abi="thiscall", ret="int", args=["pointer", "uint32"],
                                   fixture="c3cx_3d", state=_C3D_STATE, vectors=_C3D),
    "Channel_set3DPositionZ": dict(_SND_DLL, addr=0x00037BC0, abi="thiscall", ret="int", args=["pointer", "uint32"],
                                   fixture="c3cx_3d", state=_C3D_STATE, vectors=_C3D),
    # MultiSound vtable 0x1005c7e8 slots 55 / 56 / 57 / 54 / 59.
    "MultiSound::set3DPositionX": dict(_SND_DLL, addr=0x0003A5E0, abi="thiscall", ret="int",
                                       args=["pointer", "uint32"], fixture="c3cx_3d", state=_M3D_STATE, vectors=_M3D),
    "MultiSound::set3DPositionY": dict(_SND_DLL, addr=0x0003A630, abi="thiscall", ret="int",
                                       args=["pointer", "uint32"], fixture="c3cx_3d", state=_M3D_STATE, vectors=_M3D),
    "MultiSound::set3DPositionZ": dict(_SND_DLL, addr=0x0003A650, abi="thiscall", ret="int",
                                       args=["pointer", "uint32"], fixture="c3cx_3d", state=_M3D_STATE, vectors=_M3D),
    "MultiSound_set3DPosition": dict(_SND_DLL, addr=0x0003A5B0, abi="thiscall", ret="int",
                                     args=["pointer", "uint32", "uint32", "uint32"], fixture="c3cx_3d",
                                     state=_M3D_STATE, vectors=_M3DPOS),
    # Always returns 0xb (Channel_set3DBufferStub's constant); the evidence is +0x60 and the device's records.
    "MultiSound::set3DBuffer": dict(_SND_DLL, addr=0x0003A600, abi="thiscall", ret="int", args=["pointer", "uint32"],
                                    fixture="c3cx_3d", state=_M3D_STATE, vectors=_M3DBUF),

    # MultiSound flag slots (vtable 0x1005c7e8): 58, 27, 12, 47, 14.
    "MultiSound_is3DEnabled": dict(_SND_DLL, addr=0x0003A800, abi="thiscall", ret="int", args=["pointer"],
                                   fixture="c3cx_ms", state=_E_STATE, vectors=_E),
    "MultiSound::applyFlags": dict(_SND_DLL, addr=0x0003A370, abi="thiscall", ret="int", args=["pointer", "uint32"],
                                   fixture="c3cx_ms", state=_A_STATE, vectors=_A),
    "MultiSound_stop": dict(_SND_DLL, addr=0x00039C80, abi="thiscall", ret="int", args=["pointer"],
                            fixture="c3cx_ms", state=_T_STATE, vectors=_T),
    "MultiSound_findStreamNode": dict(_SND_DLL, addr=0x0003A730, abi="thiscall", ret="int", args=["pointer"],
                                      fixture="c3cx_ms", state=_FS_STATE, vectors=_FS),
    "MultiSound_setVoiceBit1On": dict(_SND_DLL, addr=0x0003A080, abi="thiscall", ret="int", args=["pointer"],
                                      fixture="c3cx_ms", state=_V_STATE, vectors=_V),

    # 0x10034770, a leaf: twelve vectors.
    "Channel_allocAndInitVoice": dict(_SND_DLL, addr=0x00034770, abi="thiscall", ret="int",
                                      args=["pointer", "pointer"], fixture="c3cx_alloc", state=_X_STATE, vectors=_X),
    # 0x10011760, a leaf (its only call is the interface slot): twelve vectors.
    "getDeviceCaps": dict(_SND_DLL, addr=0x00011760, abi="thiscall", ret="int", args=["pointer"],
                          fixture="c3cx_dev", state=_D_STATE, vectors=_D),
    "getNodeByIndex": dict(_SND_DLL, addr=0x00011790, abi="thiscall", ret="int",
                           args=["pointer", "uint32", "pointer", "uint32"], fixture="c3cx_dev",
                           state=_N_STATE, vectors=_N),
    "Ctrl5b19c_ctor": dict(_SND_DLL, addr=0x0001BEE0, abi="thiscall", ret="uint32",
                           args=["pointer"] + ["uint32"] * 13, fixture="c3cx_ctrl", state=_K_STATE, vectors=_K),
    "ostreamResetVtable": dict(_SND_DLL, addr=0x00041C26, abi="thiscall", ret="uint32", args=["pointer"],
                               fixture="c3cx_ostream", state=_O_STATE, vectors=_O),
    # Old small-block heap. sbhResizeBlock is a leaf: twenty vectors.
    "sbhResizeBlock": dict(_SND_DLL, addr=0x00047474, abi="default", ret="int",
                           args=["pointer", "pointer", "pointer", "uint32"], fixture="c3cx_sbh_c0",
                           state=_SBH_STATE, vectors=_RS),
    # ret void: nothing is loaded into eax for a return value (see the reimplementation's comment).
    "sbhPageFree": dict(_SND_DLL, addr=0x00047103, abi="default", ret="void", args=["pointer", "pointer", "pointer"],
                        fixture="c3cx_sbh_c0", state=_SBH_STATE, vectors=_PF),
    "sbhPageFree_c1f": dict(_SND_DLL, addr=0x00047103, abi="default", ret="void",
                            args=["pointer", "pointer", "pointer"], fixture="c3cx_sbh_c1f",
                            state=_SBH_STATE, vectors=_PF),
}

for _suffix, _fx in _VARIANTS:
    HOOKS["Seq_scaleTickRate" + _suffix] = dict(_SND_DLL, addr=0x0001E630, abi="thiscall", ret="int",
                                                args=["pointer", "uint32"], fixture=_fx,
                                                state=_SEQ_STATE, vectors=_SEQ)
    HOOKS["Sound::scaleTickRate" + _suffix] = dict(_SND_DLL, addr=0x00005D80, abi="thiscall", ret="int",
                                                   args=["pointer", "uint32"], fixture=_fx,
                                                   state=_SND_STATE, vectors=_SND)
    HOOKS["Channel_setRateGlide" + _suffix] = dict(_SND_DLL, addr=0x00034C80, abi="thiscall", ret="int",
                                                   args=["pointer", "int", "int", "uint32"], fixture=_fx,
                                                   state=_GLIDE_STATE, vectors=_GLIDE)
    HOOKS["MultiSound::setRateGlide" + _suffix] = dict(_SND_DLL, addr=0x0003A140, abi="thiscall", ret="int",
                                                       args=["pointer", "int", "int", "uint32"], fixture=_fx,
                                                       state=_MGLIDE_STATE, vectors=_MGLIDE)
    HOOKS["MultiSound::setReverseRate" + _suffix] = dict(_SND_DLL, addr=0x0003A6E0, abi="thiscall", ret="int",
                                                         args=["pointer", "uint32"], fixture=_fx,
                                                         state=_REV_STATE, vectors=_REV)

# --- three channel vtable slots with no hooks.csv row yet (names proposed in log/c3/c3cx_names.tsv) and the MultiSound
# overrides that call them (fixture c3cx_ms: shapes s*, c*, w*, p*).
_S = [(f"$s{i}",) for i in range(12)]
_S_STATE = [(f"$s{i}", 0, 0x1800) for i in range(12)]
_F4 = [(o, v) for o in ("$c0", "$c1") for v in (0, 1, 0x7F, 0x80, 0xFF, 0xFFFFFFFF, 0x12345678, 0x80000000, 0x55,
                                                  0x100)]
_F5C = [(o, v) for o in ("$c0", "$c1") for v in (-0x80000000, -1201, -1200, -1199, -1, 0, 1, 1199, 1200, 1201,
                                                   0x7FFFFFFF)]
_C_STATE = [("$c0", 0, 0x1800), ("$c1", 0, 0x1800)]
_W = [(f"$w{i}", v) for i in range(6) for v in (-5, 0, 1, 0x40, 0x7E, 0x7F, 0x80, 0x7FFFFFFF, -0x80000000)]
_W_STATE = [(f"$w{i}", 0, 0x1800) for i in range(6)]
_CENTS = [-0x80000000, -2500, -1201, -1200, -1199, -150, -100, -1, 0, 1, 50, 99, 100, 101, 1199, 1200, 1201, 2400,
          0x7FFFFFFF]
_P = [(f"$p{i}", c) for i in range(4) for c in _CENTS]
_P_STATE = [(f"$p{i}", 0, 0x1800) for i in range(4)]

HOOKS.update({
    # 0x1002c370: slot 16 (+0x40) of the channel vtable 0x1005bb90, a leaf; twenty vectors.
    "Channel_setField4Masked7f": dict(_SND_DLL, addr=0x0002C370, abi="thiscall", ret="int",
                                      args=["pointer", "uint32"], fixture="c3cx_ms", state=_C_STATE, vectors=_F4),
    # 0x1002c0d0: slot 28 (+0x70), a leaf (its only call is the device's own slot); twelve vectors.
    "Channel_collectFlagBits": dict(_SND_DLL, addr=0x0002C0D0, abi="thiscall", ret="int", args=["pointer"],
                                    fixture="c3cx_ms", state=_S_STATE, vectors=_S),
    # 0x1002bff0: slot 39 (+0x9c), a leaf (its only call is the device's own slot); twenty-two vectors.
    "Channel_setField5cClamped1200": dict(_SND_DLL, addr=0x0002BFF0, abi="thiscall", ret="int",
                                          args=["pointer", "uint32"], fixture="c3cx_ms", state=_C_STATE,
                                          vectors=_F5C),
    # MultiSound vtable 0x1005c7e8 slots 28, 16 and 39.
    "MultiSound_getStatus": dict(_SND_DLL, addr=0x0003A560, abi="thiscall", ret="int", args=["pointer"],
                                 fixture="c3cx_ms", state=_S_STATE, vectors=_S),
    "MultiSound::setVolume": dict(_SND_DLL, addr=0x0003A0D0, abi="thiscall", ret="int", args=["pointer", "uint32"],
                                  fixture="c3cx_ms", state=_W_STATE, vectors=_W),
    "MultiSound_setPitchCents": dict(_SND_DLL, addr=0x0003A210, abi="thiscall", ret="int",
                                     args=["pointer", "uint32"], fixture="c3cx_ms", state=_P_STATE, vectors=_P),
})

# --- Wave_Device::open's initialisers (fixtures c3cx_stream with the heap stand-in over operator new 0x100424d7, and
# c3cx_voice). Argument-free constructors: one vector per object, ten objects each filled with its own byte.
HOOKS.update({
    "initStreamBuffer": dict(_SND_DLL, addr=0x00006E00, abi="thiscall", ret="uint32", args=["pointer"],
                             fixture="c3cx_stream",
                             state=[(f"$b{i}", 0, 0x100) for i in range(10)] + [("$arena", 0, 0x20110)],
                             vectors=[(f"$b{i}",) for i in range(10)]),
    "initSoundVoice": dict(_SND_DLL, addr=0x00011180, abi="thiscall", ret="uint32", args=["pointer"],
                           fixture="c3cx_voice", state=[(f"$u{i}", 0, 0x200) for i in range(10)],
                           vectors=[(f"$u{i}",) for i in range(10)]),
})

# --- __free_lc_time (fixture c3cx_lctime: the heap stand-in records free 0x10042c07). ret void: nothing is loaded into
# eax for a return value. Seven vectors: six structs and the null pointer.
HOOKS["__free_lc_time"] = dict(_SND_DLL, addr=0x0004E4B3, abi="default", ret="void", args=["pointer"],
                               fixture="c3cx_lctime",
                               state=[(f"$l{i}", 0, 0xb0) for i in range(6)] + [("$freelog", 0, 0x404)],
                               vectors=[(f"$l{i}",) for i in range(6)] + [(0,)])

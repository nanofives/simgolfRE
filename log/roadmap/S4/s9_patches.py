"""Patches of the converter's output for the S4.9 assembler (9_assemble.py): PATCHES = (block, old, new) exact replacements and
(block, pattern, replacement, "re") DOTALL regex replacements; VOID_OFF = for each function that returns nothing, the dword (offset
from `this`) negative-control mode 1 changes after the body; GLOBAL_RE = regex rewrites applied to every block.
"""

PATCHES = []

GLOBAL_RE = [
    (r"fldc<void\*>\((\w+), (0x[0-9A-Fa-f]+|\d+)\)", r"ld<u32>(ctx, \1 + \2)"),
    (r"fldc<unsigned char>\((\w+), (0x[0-9A-Fa-f]+|\d+)\)", r"ld<uint8_t>(ctx, \1 + \2)"),
    (r"fldc<unsigned short>\((\w+), (0x[0-9A-Fa-f]+|\d+)\)", r"ld<uint16_t>(ctx, \1 + \2)"),
    (r"fldc<unsigned>\((\w+), (0x[0-9A-Fa-f]+|\d+)\)", r"ld<uint32_t>(ctx, \1 + \2)"),
    (r"fldc<int>\((\w+), (0x[0-9A-Fa-f]+|\d+)\)", r"ld<int32_t>(ctx, \1 + \2)"),
    # `ld<T>(ctx, x) = v;` (a value, not a reference) -> st
    (r"\bld<(u?int\d+_t|u32)>\(ctx, ([^;]*?)\) = ([^;]*?);", r"st<\1>(ctx, \2, \3);"),
    (r"\+\+ld<(u?int\d+_t|u32)>\(ctx, ([^;]*?)\);", r"{ const u32 va_ = \2; st<\1>(ctx, va_, ld<\1>(ctx, va_) + 1); }"),
    (r"--ld<(u?int\d+_t|u32)>\(ctx, ([^;]*?)\);", r"{ const u32 va_ = \2; st<\1>(ctx, va_, ld<\1>(ctx, va_) - 1); }"),
    (r"snd_obj\(ctx, &SoundState::g_soundThreadState\)", r"snd_arr(ctx, &SoundState::g_soundThreadState)"),
]

VOID_OFF = {
    "WaveStream::destroy": 0x58,
    "computeCaptureDuration": 0xc0,
    "Wave_In_Device::dtor": 0,
    "MidiReader_freeContent": 0xc,
    "streamConsumeByte": 0x30,
    "MidiReader_dtor": 0x3c,
    "streamReaderDtor": 0x5c,
    "MmioBuffer_init": 0,
    "onWaveInCallbackMessage": 0,
}

GLOBAL_RE += [
    (r"(_body\(Ctx &ctx), u32 unused0\)", r"\1)"),
    (r"(_body\(Ctx &ctx), u32 unused0, ", r"\1, "),
    (r"c3fd_gref\(ctx, (\w+)\) = ([^;]+);", r"st<u32>(ctx, snd_va(ctx, \1), \2);"),
]

_OFF = {"prev": "0", "next": "4", "value": "8"}
GLOBAL_RE += [
    # node members: assignments first, then reads
    (r"\bn->prev->next = n->next;", "st<u32>(ctx, n_prev(ctx, n) + 4, n_next(ctx, n));"),
    (r"\bn->next->prev = n->prev;", "st<u32>(ctx, n_next(ctx, n), n_prev(ctx, n));"),
    (r"(\b\w+)->prev = ([^;]+);", r"st<u32>(ctx, \1, \2);"),
    (r"(\b\w+)->next = ([^;]+);", r"st<u32>(ctx, \1 + 4, \2);"),
    (r"(\b\w+)->value = ([^;]+);", r"st<u32>(ctx, \1 + 8, \2);"),
    (r"ld<u32>\(ctx, ([^;()]*(?:\([^()]*\))?[^;()]*)\)->next = ([^;]+);", r"st<u32>(ctx, ld<u32>(ctx, \1) + 4, \2);"),
    (r"\((\w+)->value\)", r"n_value(ctx, \1)"),
    (r"(\b\w+)->prev\b", r"n_prev(ctx, \1)"),
    (r"(\b\w+)->next\b", r"n_next(ctx, \1)"),
    (r"(\b\w+)->value\b", r"n_value(ctx, \1)"),
]

PATCHES += [
    ("closeMmioHandle", "    u32 cnt = (snd_obj(ctx, &SoundState::g_openMmioCount).va());\n    *cnt = *cnt - 1;  // 0x8780-0x878b\n",
     "    const u32 cnt = snd_obj(ctx, &SoundState::g_openMmioCount).va();\n    st<uint32_t>(ctx, cnt, ld<uint32_t>(ctx, cnt) - 1);  // 0x8780-0x878b\n"),
    ("Dll_Wave_Device::create_device", "        *out = 0;  // 0xb0ba\n", "        st<u32>(ctx, out, 0);  // 0xb0ba\n"),
    ("Dll_Wave_Device::create_device", "    *out = d;  // 0xb09c\n", "    st<u32>(ctx, out, d);  // 0xb09c\n"),
    ("Dll_Wave_In_Device::create_device", "        *out = 0;  // 0xe9a7\n", "        st<u32>(ctx, out, 0);  // 0xe9a7\n"),
    ("Dll_Wave_In_Device::create_device", "    *out = p;  // 0xe986\n", "    st<u32>(ctx, out, p);  // 0xe986\n"),
    ("Wave_Device::deviceName", "        *a2 = 0;  // 0xe28a\n", "        st<uint8_t>(ctx, a2, 0);  // 0xe28a (mov byte ptr [ecx], 0)\n"),
    ("Wave_Device::stop",
     "        unsigned char& mb = ld<uint8_t>(ctx, snd_obj(ctx, &SoundState::g_midiOutputFlags).va());  // 0xb988\n"
     "        mb = static_cast<uint8_t>((mb & 0xfe) ^ 1);  // 0xb98d-0xb991\n",
     "        const u32 mb = snd_obj(ctx, &SoundState::g_midiOutputFlags).va();  // 0xb988\n"
     "        st<uint8_t>(ctx, mb, static_cast<uint8_t>((ld<uint8_t>(ctx, mb) & 0xfe) ^ 1));  // 0xb98d-0xb991\n"),
    ("Wave_Device::dtor", "u32 c3ew_popFront(Ctx &ctx, u32 list, int32_t frontOff, int32_t backOff, int32_t countOff, u32 node, u32 ret) {",
     "u32 c3ew_popFront(Ctx &ctx, u32 list, int32_t frontOff, int32_t backOff, int32_t countOff, u32 node, unsigned &ret) {"),
    ("Wave_Device::dtor", "    *ret = snd_delete(ctx, node);\n", "    ret = snd_delete(ctx, node).eax;\n"),
    ("Wave_Device::dtor", "node, &dr);", "node, dr);"),
    ("freeMmioHandleListGetInfo", "node, &dr));", "node, dr));"),
    ("Wave_Device::scanCommandRing", "    unsigned i = *first;\n", "    unsigned i = ld<uint32_t>(ctx, first);\n"),
    ("Wave_Device::scanCommandRing", "    if (i == *last) return 0;\n    for (;;) {\n        if (j == *last) return 0;\n",
     "    if (i == ld<uint32_t>(ctx, last)) return 0;\n    for (;;) {\n        if (j == ld<uint32_t>(ctx, last)) return 0;\n"),
    ("Wave_Device::scanCommandRing", "                if (j == *last) break;\n", "                if (j == ld<uint32_t>(ctx, last)) break;\n"),
    ("Wave_Device::scanCommandRing", "        if (i == *last) return 0;\n    }\n}", "        if (i == ld<uint32_t>(ctx, last)) return 0;\n    }\n}"),
    ("getWaveDeviceInterface", "    if (C3FU_NULL()) return 0;\n", ""),
    ("getWaveDeviceInterface", "    if (d == 0) return NEG(13, 0u, 1u);  // 0xe2d5 / 0xe2dc\n    return *d;  // 0xe2d9\n",
     "    if (d == 0) return 0;  // 0xe2d5 / 0xe2dc\n    return ld<uint32_t>(ctx, d);  // 0xe2d9\n"),
    ("streamReadByte", "        void** vtbl = ld<u32>(ctx, self);  // 0x1c7ad\n        static_cast<SlotVoid0_t>(vtbl[0x14 / 4])(self, 0);  // call at 0x1c7b1\n",
     "        snd_vcall(ctx, self, 0x14, SndAbi::Thiscall, {});  // 0x1c7ad / call at 0x1c7b1\n"),
    ("streamConsumeByte", "        void** vtbl = ld<u32>(ctx, self);  // 0x1c7ed\n        static_cast<SlotVoid0_t>(vtbl[0x14 / 4])(self, 0);  // call at 0x1c7f1\n",
     "        snd_vcall(ctx, self, 0x14, SndAbi::Thiscall, {});  // 0x1c7ed / call at 0x1c7f1\n"),
    ("WaveInDeviceBase_openInner",
     "    void** vt = ld<u32>(ctx, self);  // 0x2a428\n    int rc = static_cast<SlotInt1_t>(vt[4 / 4])(self, 0, b);  // call at 0x2a42b\n",
     "    int rc = static_cast<int>(snd_vcall(ctx, self, 4, SndAbi::Thiscall, {static_cast<uint32_t>(b)}).eax);  // 0x2a428 / call at 0x2a42b\n"),
    ("WaveInDeviceBase_openInner",
     "    void** ivt = ld<u32>(ctx, inner);  // 0x2a43b\n    int rc2 = static_cast<SlotInt2_t>(ivt[0x10 / 4])(inner, 0, a, b);  // call at 0x2a43d\n",
     "    int rc2 = static_cast<int>(snd_vcall(ctx, inner, 0x10, SndAbi::Thiscall, {static_cast<uint32_t>(a), static_cast<uint32_t>(b)}).eax);  // 0x2a43b / call at 0x2a43d\n"),
    ("WaveInDeviceBase_openInner",
     "    void** vt2 = ld<u32>(ctx, self);  // 0x2a446\n    static_cast<SlotVoid0_t>(vt2[8 / 4])(self, 0);  // call at 0x2a44a\n",
     "    snd_vcall(ctx, self, 8, SndAbi::Thiscall, {});  // 0x2a446 / call at 0x2a44a\n"),
    ("WaveInDeviceBase_closeInner",
     "        void** ivt = ld<u32>(ctx, inner);  // 0x2a47a\n        static_cast<SlotVoid0_t>(ivt[0x14 / 4])(inner, 0);  // call at 0x2a47c\n"
     "        void** vt = ld<u32>(ctx, self);  // 0x2a47f\n        static_cast<SlotVoid0_t>(vt[8 / 4])(self, 0);  // call at 0x2a483\n",
     "        snd_vcall(ctx, inner, 0x14, SndAbi::Thiscall, {});  // 0x2a47a / call at 0x2a47c\n"
     "        snd_vcall(ctx, self, 8, SndAbi::Thiscall, {});  // 0x2a47f / call at 0x2a483\n"),
]

GLOBAL_RE += [
    (r"c3fd_at\(ctx, ring, (\w)\)\[(\d)\]", r"ld<u32>(ctx, c3fd_at(ctx, ring, \1) + \2 * 4)"),
    (r"uint32_t c3fg_u\(Ctx &ctx, u32 p\)", r"uint32_t c3fg_u(Ctx &, u32 p)"),
    (r", uint32_t unused\d\)", r", uint32_t)"),
    (r", uint32_t unused\d,", r", uint32_t,"),
]

PATCHES += [
    ("Wave_Device::stop",
     "    u32 st = ld<u32>(ctx, snd_obj(ctx, &SoundState::g_waveOwner).va());  // 0xba19\n    if (st) {  // 0xba1f / 0xba21\n"
     "        snd_call(ctx, 0x00001a32, SndAbi::Thiscall, {st}).eax;  // 0xba23 / 0xba25\n        snd_delete(ctx, st);  // 0xba2a / 0xba2b\n",
     "    u32 wo = ld<u32>(ctx, snd_obj(ctx, &SoundState::g_waveOwner).va());  // 0xba19\n    if (wo) {  // 0xba1f / 0xba21\n"
     "        snd_call(ctx, 0x00001a32, SndAbi::Thiscall, {wo}).eax;  // 0xba23 / 0xba25\n        snd_delete(ctx, wo);  // 0xba2a / 0xba2b\n"),
    ("freeMmioHandleListGetInfo",
     "    u32 st = ld<u32>(ctx, snd_obj(ctx, &SoundState::g_waveInDevice).va());  // 0x31604\n    if (st) snd_call(ctx, 0x00001ba4, SndAbi::Thiscall, {st}).eax;  // 0x3160a-0x3160e\n",
     "    u32 wi = ld<u32>(ctx, snd_obj(ctx, &SoundState::g_waveInDevice).va());  // 0x31604\n    if (wi) snd_call(ctx, 0x00001ba4, SndAbi::Thiscall, {wi}).eax;  // 0x3160a-0x3160e\n"),
    ("streamReaderDtor", "    void** next = (n[1]);\n    if (next) {\n        next[0] = 0;\n        st<u32>(ctx, self + hOff, next);\n",
     "    const u32 next = ld<u32>(ctx, n + 4);\n    if (next) {\n        st<u32>(ctx, next, 0);\n        st<u32>(ctx, self + hOff, next);\n"),
    ("streamReaderDtor", "    u32 payload = n[2];\n", "    u32 payload = ld<u32>(ctx, n + 8);\n"),
    ("streamReaderDtor", "    void** n = ld<u32>(ctx, self + hOff);\n    while (n) {", "    u32 n = ld<u32>(ctx, self + hOff);\n    while (n) {"),
    ("streamReaderDtor", "    void** n = ld<u32>(ctx, self + 0x5c);  // 0x312c6\n", "    u32 n = ld<u32>(ctx, self + 0x5c);  // 0x312c6\n"),
    ("streamReaderDtor", "    typedef unsigned(__stdcall* MmioClose_t)(unsigned, unsigned);\n", ""),
    ("getWaveDeviceInterface", "getWaveDeviceInterface_body(ctx, a[0])", "getWaveDeviceInterface_body(ctx)"),
]

PATCHES += [
    ("fillDecodeRing", "c3fn_RingCopy c3fn_ringCopy(", "struct c3fn_RingCopy { unsigned n1, n2; bool second, cut; };\nc3fn_RingCopy c3fn_ringCopy("),
    ("fillDecodeRing", "int32_t c3fn_mmioSeek(Ctx &ctx, u32 h, int32_t o, int32_t w) { return snd_import(ctx, kMmioSeek, SndAbi::Stdcall, {h, o, w}).eax; }",
     "int32_t c3fn_mmioSeek(Ctx &ctx, u32 h, int32_t o, int32_t w) { return static_cast<int32_t>(snd_import(ctx, kMmioSeek, SndAbi::Stdcall, {h, static_cast<uint32_t>(o), static_cast<uint32_t>(w)}).eax); }"),
    ("closeMmioSound",
     "    uint64_t v;\n    rep_movs(ctx.mem, &v, src, 8);\n", "    uint64_t v = ld<uint64_t>(ctx, src);\n"),
    ("closeMmioSound", "    rep_movs(ctx.mem, dst, &v, 8);\n", "    st<uint64_t>(ctx, dst, v);\n"),
    ("closeMmioSound", "    double d;\n    rep_movs(ctx.mem, &d, p, 8);\n    return d == 0.0 || d != d;\n",
     "    const uint64_t bits = ld<uint64_t>(ctx, p);\n    double d;\n    std::memcpy(&d, &bits, 8);\n    return d == 0.0 || d != d;\n"),
    ("flushMmioBuffer", "uint32_t c3fn_mmioSetBuffer(Ctx &ctx, u32 h, u32 b, int32_t n, uint32_t f) { return snd_import(ctx, kMmioSetBuffer, SndAbi::Stdcall, {h, b, n, f}).eax; }",
     "uint32_t c3fn_mmioSetBuffer(Ctx &ctx, u32 h, u32 b, int32_t n, uint32_t f) { return snd_import(ctx, kMmioSetBuffer, SndAbi::Stdcall, {h, b, static_cast<uint32_t>(n), f}).eax; }"),
    ("flushMmioBuffer", "            unsigned char& b = ld<uint8_t>(ctx, self + 0x2d4 + 0x110 * k);\n            b = static_cast<uint8_t>((b & 0xfd) | 4);\n",
     "            const u32 bp = self + 0x2d4 + 0x110 * k;\n            st<uint8_t>(ctx, bp, static_cast<uint8_t>((ld<uint8_t>(ctx, bp) & 0xfd) | 4));\n"),
    ("MmioBuffer_init", "fldc<unsigned>(block, i * 4)", "ld<uint32_t>(ctx, block + i * 4)"),
    ("MmioBuffer_init", "fldc<unsigned>(fmt, i * 4)", "ld<uint32_t>(ctx, fmt + i * 4)"),
    ("MmioBuffer_init", "fldc<unsigned short>(fmt, 0x10)", "ld<uint16_t>(ctx, fmt + 0x10)"),
    ("loadMultiChannel", "    void** slot = (self + 0x1440);  // 0x399fe\n", "    u32 slot = self + 0x1440;  // 0x399fe\n"),
    ("loadMultiChannel", "i++, slot++) {", "i++, slot += 4) {"),
    ("loadMultiChannel", "        *slot = p;  // 0x39aac\n", "        st<u32>(ctx, slot, p);  // 0x39aac\n"),
]

GLOBAL_RE += [
    (r", u32 unused\d\)", r", u32)"),
    (r"uint32_t c3ew_u\(Ctx &ctx, u32 p\)", r"uint32_t c3ew_u(Ctx &, u32 p)"),
]

GLOBAL_RE += [
    (r"snd_obj\(ctx, &SoundState::(g_openSoundList)\)", r"snd_arr(ctx, &SoundState::\1)"),
]

GLOBAL_RE += [
    (r"snd_obj\(ctx, &SoundState::(g_soundThreadCrit)\)", r"snd_arr(ctx, &SoundState::\1)"),
    (r"uint32_t (c3dt_waveInErr|c3dh_waveInErr)\(Ctx &ctx, uint32_t r\)", r"uint32_t \1(Ctx &, uint32_t r)"),
]

PATCHES += [
    # import-level control (mode 2): mmioClose with the wrong flags
    ("closeMmioHandle", "    if (h) snd_import(ctx, kIatMmioClose, SndAbi::Stdcall, {h, 0}).eax;  // 0x8773 / 0x877a\n",
     "    if (h) snd_import(ctx, kIatMmioClose, SndAbi::Stdcall, {h, NEG(closeMmioHandle) == 2 ? 1u : 0u}).eax;  // 0x8773 / 0x877a (mode 2: flags 1)\n"),
    # import-level control (mode 2): waveInReset is not called
    ("WaveInDevice_reset", "    snd_import(ctx, kIatWaveInReset, SndAbi::Stdcall, {ld<uint32_t>(ctx, self + 0x44)}).eax;  // 0x38f21\n",
     "    if (NEG(WaveInDevice_reset) != 2) snd_import(ctx, kIatWaveInReset, SndAbi::Stdcall, {ld<uint32_t>(ctx, self + 0x44)}).eax;  // 0x38f21 (mode 2: skipped)\n"),
]

GLOBAL_RE += [
    (r"/\*TODO\(convert\): x87\*/\s*", ""),
]

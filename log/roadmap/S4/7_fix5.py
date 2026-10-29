import re, sys
sys.path.insert(0, "log/roadmap/S4")
from importlib import import_module
B = import_module("7_blk")
P = B.P

BLOCKS = r'''
// ---- SoundTimer::start (rva 0x8ce0): written from c3dh.cpp TimerStart_re
constexpr uint32_t kIatTimeBeginPeriod = 0xb8640;  // c3dh.cpp
constexpr uint32_t kIatTimeSetEvent = 0xb8644;  // c3dh.cpp
uint32_t SoundTimer__start_body(Ctx &ctx, u32 self) {
    if ((ld<uint8_t>(ctx, self + 0x3c) & 4) != 0) return 0xd;  // 0x8ce3 / 0x8ce7 / 0x8ce9
    st<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_timerBusy).va(), 0);  // 0x8cf0
    if (snd_import(ctx, kIatTimeBeginPeriod, SndAbi::Stdcall, {ld<uint32_t>(ctx, self + 0x18)}).eax == 0x61) return 0xb;  // 0x8cfe / 0x8d04
    unsigned id = snd_import(ctx, kIatTimeSetEvent, SndAbi::Stdcall,
                             {ld<uint32_t>(ctx, self), ld<uint32_t>(ctx, self + 0x18), snd_va(ctx, 0x0000128a), self,
                              ld<uint32_t>(ctx, self + 0x1c)}).eax;  // 0x8d21 (callback: the thunk to SoundTimer::onTick)
    st<uint32_t>(ctx, self + 0x40, id);  // 0x8d2a
    if (id == 0x61) return 0xb;  // 0x8d27 / 0x8d2f
    unsigned f = ld<uint32_t>(ctx, self + 0x3c);  // 0x8d36
    f = (f & ~0xffu) | static_cast<uint8_t>(f | 7);  // 0x8d39 (or al, 7)
    st<uint32_t>(ctx, self + 0x3c, f);  // 0x8d3b
    return 0;  // 0x8d3e
}
int g_negctl_SoundTimer__start = 0;
void a_SoundTimer__start(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(SoundTimer__start), SoundTimer__start_body(ctx, a[0]))); }

// ---- SoundWindowObj_ctor (rva 0x28600): written from c3fza.cpp C3fzaCtor_re
u32 SoundWindowObj_ctor_body(Ctx &ctx, u32 self) {
    st<u32>(ctx, self, snd_va(ctx, snd_rva::vtbl_SoundWindowObj));  // 0x28607
    st<uint32_t>(ctx, self + 8, 0);  // 0x2860d
    st<uint32_t>(ctx, self + 0xc, 0);  // 0x28610
    st<uint8_t>(ctx, self + 4, 0);  // 0x28613 (only the byte at +4)
    return self;  // 0x28600 `mov eax, ecx`
}
int g_negctl_SoundWindowObj_ctor = 0;
void a_SoundWindowObj_ctor(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(SoundWindowObj_ctor), SoundWindowObj_ctor_body(ctx, a[0]))); }

// ---- SoundWindowObj_subclass (rva 0x286f0): written from c3ex.cpp Subclass_re
uint32_t SoundWindowObj_subclass_body(Ctx &ctx, u32 self, u32 hwnd) {
    if (ld<uint32_t>(ctx, self + 8)) return 0;  // 0x286f4-0x286f9
    unsigned r = snd_import(ctx, kIatSetWindowLongA, SndAbi::Stdcall,
                            {hwnd, static_cast<uint32_t>(-4), snd_va(ctx, 0x000025ef)}).eax;  // 0x28707 (the thunk to soundWindowSubclassProc)
    st<uint32_t>(ctx, self + 0xc, r);  // 0x2870f
    if (!r) return 0x18;  // 0x28712 / 0x28715
    st<uint32_t>(ctx, self + 8, hwnd);  // 0x28721
    st<uint8_t>(ctx, self + 4, static_cast<uint8_t>(ld<uint8_t>(ctx, self + 4) | 4));  // 0x2871e-0x28726
    return 0;  // 0x2872a
}
int g_negctl_SoundWindowObj_subclass = 0;
void a_SoundWindowObj_subclass(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(SoundWindowObj_subclass), SoundWindowObj_subclass_body(ctx, a[0], a[1]))); }

// ---- MultiSound_setSlotSource (rva 0x2f8d0): written from c3fd.cpp SetSlot_re
uint32_t MultiSound_setSlotSource_body(Ctx &ctx, u32 self, u32 index) {
    if (ld<u32>(ctx, self + 0xfc)) return 0xf;  // 0x2f8fa-0x2f903
    if (index >= 0x28) return 0xa;  // 0x2f91e-0x2f924
    if (!ld<uint32_t>(ctx, self + 0x58 + 4 * index)) return 0x24;  // 0x2f93b-0x2f942
    unsigned flags = ld<u32>(ctx, self + 0xf8) ? 0x302u : 0x102u;  // 0x2f959-0x2f970
    u32 p = snd_new(ctx, 0x1450);  // 0x2f97a
    u32 ms = p ? snd_call(ctx, 0x000025ea, SndAbi::Thiscall, {p}).eax : 0;  // 0x2f98c-0x2f995 (MultiSound_ctor)
    snd_vcall(ctx, ms, 0x6c, SndAbi::Thiscall, {flags});  // 0x2f9a4 (no NULL test: a NULL block faults)
    unsigned r = snd_vcall(ctx, ms, 0x10, SndAbi::Thiscall, {ld<uint32_t>(ctx, self + 0x58 + 4 * index)}).eax;  // 0x2f9a7-0x2f9b4
    if (r) {  // 0x2f9b9 / 0x2f9bb
        if (ms) snd_vcall(ctx, ms, 0, SndAbi::Thiscall, {1});  // 0x2f9bd-0x2f9c7
        return r;
    }
    if (ld<uint8_t>(ctx, ms + 0xcc) & 4)  // 0x2f9cb / 0x2f9d2
        snd_call(ctx, 0x00001dde, SndAbi::Thiscall, {ms + 0x70});  // 0x2f9d7
    st<u32>(ctx, self + 0xfc, ms);  // 0x2f9e4
    u32 src = ld<u32>(ctx, self + 0xf8);  // 0x2f9e0
    if (src)  // 0x2f9e2 / 0x2f9e6
        snd_call(ctx, 0x0000122b, SndAbi::Thiscall, {src + 0x70, self + 0xfc, self + 0xf8});  // 0x2f9e8-0x2f9ed
    return r;  // 0x2f9f7
}
int g_negctl_MultiSound_setSlotSource = 0;
void a_MultiSound_setSlotSource(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(MultiSound_setSlotSource), MultiSound_setSlotSource_body(ctx, a[0], a[1]))); }

// ---- MultiSound_setPitchCents (rva 0x3a210): written from c3cx.cpp MsSetPitch_re
// The x87 work runs at the caller's 53-bit control word, so it is plain double arithmetic in the original's order; __ftol keeps the
// low dword of the 64-bit result (ftol_low32). The tables are .rdata (25 entries each: n reaches -1 and 25, the original reads just
// outside the table, and so does this body).
double md_d(Ctx &ctx, u32 va) { return pun<double>(ld<uint64_t>(ctx, va)); }
int32_t MultiSound_setPitchCents_body(Ctx &ctx, u32 self, uint32_t arg) {
    int c = static_cast<int32_t>(arg);
    if (c < -1200 || c > 1200) c = c % 1200;  // 0x3a223..0x3a237
    snd_call(ctx, 0x0002bff0, SndAbi::Thiscall, {self, static_cast<uint32_t>(c)});  // 0x3a23c
    const int q = c / 100;  // 0x3a24d
    int r = c % 100;
    if (r < 0) r = -r;  // 0x3a254
    const int s = q + 12;  // 0x3a266
    const u32 T1 = snd_va(ctx, 0x0005c608), T2 = snd_va(ctx, 0x0005c66c), D = snd_va(ctx, 0x0005c6d0);
    int baseI;
    double base;
    if (snd_vcall(ctx, self, 0xcc, SndAbi::Thiscall, {}).eax != 0) {  // 0x3a277, 0x3a286
        baseI = ld<int32_t>(ctx, T2 + static_cast<u32>(s) * 4);  // 0x3a288
        base = 0.0;  // 0x3a28e
    } else {
        baseI = ld<int32_t>(ctx, T1 + static_cast<u32>(s) * 4);  // 0x3a293
        base = md_d(ctx, D + static_cast<u32>(s) * 8);  // 0x3a299
    }
    const int n = (c < 0) ? s - 1 : s + 1;  // 0x3a2a3..0x3a2aa
    double t = md_d(ctx, D + static_cast<u32>(n) * 8) - md_d(ctx, D + static_cast<u32>(s) * 8);  // 0x3a2b4
    t = t * static_cast<double>(r);  // 0x3a2bb
    t = t * md_d(ctx, snd_va(ctx, 0x0005c910));  // 0x3a2d0
    const double val = t + base;  // 0x3a2d6
    const uint32_t delta = static_cast<uint32_t>(ld<int32_t>(ctx, T1 + static_cast<u32>(n) * 4)) -
                           static_cast<uint32_t>(ld<int32_t>(ctx, T1 + static_cast<u32>(s) * 4));  // 0x3a2cb
    double u = static_cast<double>(static_cast<int32_t>(delta)) * static_cast<double>(r);  // 0x3a2dd, 0x3a2e0
    u = u * static_cast<double>(pun<float>(ld<uint32_t>(ctx, snd_va(ctx, 0x0005c90c))));  // 0x3a2e3
    const uint32_t k = ftol_low32(u);  // __ftol at 0x3a2e9
    snd_call(ctx, 0x00035750, SndAbi::Thiscall, {self + 0x70, static_cast<uint32_t>(baseI) - k});  // 0x3a2ee, 0x3a2f4
    st<uint64_t>(ctx, self + 0x13f8, pun<uint64_t>(val));  // 0x3a2f9..0x3a305
    return 0;
}
int g_negctl_MultiSound_setPitchCents = 0;
void a_MultiSound_setPitchCents(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_i32(static_cast<int32_t>(neg_u32(NEG(MultiSound_setPitchCents), static_cast<uint32_t>(MultiSound_setPitchCents_body(ctx, a[0], a[1]))))); }
'''
t = P.read_text(encoding="utf-8")
i = t.index("const FuncDef kFuncs[] = {")
t = t[:i] + BLOCKS.lstrip("\n") + "\n" + t[i:]
add = '''    {"SoundTimer::start", "thiscall", "uint32", 1, 0, a_SoundTimer__start, &g_negctl_SoundTimer__start},
    {"SoundWindowObj_ctor", "thiscall", "uint32", 1, 0, a_SoundWindowObj_ctor, &g_negctl_SoundWindowObj_ctor},
    {"SoundWindowObj_subclass", "thiscall", "uint32", 2, 0, a_SoundWindowObj_subclass, &g_negctl_SoundWindowObj_subclass},
    {"MultiSound_setSlotSource", "thiscall", "uint32", 2, 0, a_MultiSound_setSlotSource, &g_negctl_MultiSound_setSlotSource},
    {"MultiSound_setPitchCents", "thiscall", "int", 2, 0, a_MultiSound_setPitchCents, &g_negctl_MultiSound_setPitchCents},
'''
j = t.index("};\n\n}  // namespace", t.index("const FuncDef kFuncs[] = {"))
t = t[:j] + add + t[j:]
P.write_text(t, encoding="utf-8")
print("ok")

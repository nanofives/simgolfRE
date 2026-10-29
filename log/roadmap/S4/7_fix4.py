import re, sys
sys.path.insert(0, "log/roadmap/S4")
from importlib import import_module
B = import_module("7_blk")
rb, sub = B.replace_body, B.sub
P = B.P

sub("uint32_t Sound_reapIdle_body(Ctx &ctx, u32 unused0) {", "uint32_t Sound_reapIdle_body(Ctx &ctx) {")

# ---- Sound_resetBuffers: the shared open-sound-list unlink (0x32074-0x320d1)
rb("Sound_resetBuffers", """uint32_t c3fg_mmioClose(Ctx &ctx, u32 h, uint32_t f) { return snd_import(ctx, kMmioClose, SndAbi::Stdcall, {h, f}).eax; }
// The unlink sequence shared by Sound_resetBuffers (0x32074-0x320d1), closeMmioSound (0x35ce5-0x35d42) and
// parseRiffWave (0x32b7a-0x32bd8): the first node of g_openSoundList whose value is `value` is taken out (prev->next
// or the head updated, next->prev updated and the cursor set to next; with no next, the cursor = 0 and the previous-value
// word = the node's prev), freed with operator delete, and g_openSoundCount lowered. Returns the node (0 when none
// matched) and, through *count, the lowered count (eax of the originals after the decrement).
u32 c3fg_unlinkOpenSound(Ctx &ctx, u32 value, u32 *count) {
    const u32 head = snd_obj(ctx, &SoundState::g_openSoundList).va();
    u32 n = ld<u32>(ctx, head);
    while (n && ld<u32>(ctx, n + 8) != value) n = ld<u32>(ctx, n + 4);
    if (!n) return 0;
    const u32 prev = ld<u32>(ctx, n), next = ld<u32>(ctx, n + 4);
    if (prev) st<u32>(ctx, prev + 4, next);
    else st<u32>(ctx, head, next);
    if (next) {
        st<u32>(ctx, next, prev);
        st<u32>(ctx, snd_obj(ctx, &SoundState::g_openSoundListCursor).va(), next);
    } else {
        st<u32>(ctx, snd_obj(ctx, &SoundState::g_openSoundListCursor).va(), 0);
        st<u32>(ctx, snd_obj(ctx, &SoundState::g_openSoundListPrevVal).va(), prev);
    }
    snd_delete(ctx, n);
    const u32 cnt = snd_obj(ctx, &SoundState::g_openSoundCount).va();
    st<uint32_t>(ctx, cnt, ld<uint32_t>(ctx, cnt) - 1u);
    *count = ld<uint32_t>(ctx, cnt);
    return n;
}
uint32_t Sound_resetBuffers_body(Ctx &ctx, u32 self) {
    if (ld<u32>(ctx, self + 8)) {  // 0x3202a
        snd_delete(ctx, ld<u32>(ctx, self + 8));  // 0x3202d
        st<u32>(ctx, self + 8, 0);  // 0x32035
    }
    for (int k = 0; k < 0x12; ++k) st<uint32_t>(ctx, self + 0xc + 4 * k, 0);  // 0x32048
    st<uint32_t>(ctx, self + 0x58, 0);  // 0x32051 / 0x3205b
    st<uint32_t>(ctx, self + 0x5c, 0);  // 0x32053
    unsigned eax = 0;
    u32 h = ld<u32>(ctx, self);  // 0x3205d
    if (h) {  // 0x32061
        c3fg_mmioClose(ctx, h, 0);  // 0x32065
        unsigned count = 0;
        if (c3fg_unlinkOpenSound(ctx, self + 0xc, &count)) eax = count;  // 0x3206b-0x320d1
        const u32 mc = snd_obj(ctx, &SoundState::g_openMmioCount).va();
        st<uint32_t>(ctx, mc, ld<uint32_t>(ctx, mc) - 1u);  // 0x320d6
    }
    st<u32>(ctx, self, 0);  // 0x320dc
    return eax;
}""")

# ---- MultiSound_dtor
rb("MultiSound_dtor", """void MultiSound_dtor_body(Ctx &ctx, u32 self) {
    st<u32>(ctx, self, snd_va(ctx, 0x5c7e8));  // 0x39890
    if (ld<int32_t>(ctx, self + 0x1438) > 1) {  // 0x398a1 / 0x398a7
        int i = 1;
        u32 m = self + 0x1440;
        do {
            if (ld<u32>(ctx, m)) {  // 0x398b2 / 0x398b4
                snd_vcall(ctx, ld<u32>(ctx, m), 0, SndAbi::Thiscall, {1});  // 0x398b8 / 0x398ba
                st<u32>(ctx, m, 0);  // 0x398bc
            }
            ++i;
            m += 4;
        } while (i < ld<int32_t>(ctx, self + 0x1438));  // 0x398c2-0x398ce
    }
    if (ld<uint8_t>(ctx, self + 0xc8) & 2)  // 0x398d1 / 0x398d8
        snd_call(ctx, 0x00001d11, SndAbi::Thiscall, {(self + 0x70)});  // 0x398dd
    if (ld<u32>(ctx, self + 0x1430)) {  // 0x398e8 / 0x398ea
        snd_delete(ctx, ld<u32>(ctx, self + 0x1430));  // 0x398ed
        st<u32>(ctx, self + 0x1430, 0);  // 0x398f5
    }
    snd_call(ctx, 0x000011b8, SndAbi::Thiscall, {self ? (self + 0x70) : 0});  // 0x398ff-0x3990f
    snd_call(ctx, 0x00002108, SndAbi::Thiscall, {self});  // 0x3991e
}""")

# ---- SoundGroup_stop
rb("SoundGroup_stop", """uint32_t SoundGroup_stop_body(Ctx &ctx, u32 self) {
    snd_vcall(ctx, self, 0x5c, SndAbi::Thiscall, {});  // 0x39b35
    int n = ld<int32_t>(ctx, self + 0x1438);  // 0x39b38
    if (n > 1) {  // 0x39b3e / 0x39b41
        int i = 1;  // 0x39b45
        u32 p = self + 0x1440;  // 0x39b4a
        do {
            if (ld<u32>(ctx, p)) {  // 0x39b50-0x39b54
                snd_call(ctx, 0x000016a4, SndAbi::Thiscall, {ld<u32>(ctx, p)});  // 0x39b56
                u32 m = ld<u32>(ctx, p);  // 0x39b5b
                if (m) snd_vcall(ctx, m, 0, SndAbi::Thiscall, {1});  // 0x39b5d-0x39b65
                st<u32>(ctx, p, 0);  // 0x39b67
            }
            n = ld<int32_t>(ctx, self + 0x1438);  // 0x39b6d
            ++i;  // 0x39b73
            p += 4;  // 0x39b74
        } while (i < n);  // 0x39b77 / 0x39b79
        st<int32_t>(ctx, self + 0x1438, 1);  // 0x39b7c
    }
    if (ld<uint8_t>(ctx, self + 0xc8) & 2) {  // 0x39b87 / 0x39b8e
        unsigned r = snd_call(ctx, 0x00001d11, SndAbi::Thiscall, {(self + 0x70)}).eax;  // 0x39b93
        st<uint32_t>(ctx, self + 0x44, static_cast<uint32_t>(ld<uint32_t>(ctx, self + 0x44) & 0xfffffffeu));  // 0x39b98-0x39b9e
        return r;
    }
    return 0x14;  // 0x39ba3
}""")

# ---- MultiSound_start / startVoice
t = P.read_text(encoding="utf-8")
t = t.replace("c3ex_vslot(self, 0xcc)(self, 0)", "snd_vcall(ctx, self, 0xcc, SndAbi::Thiscall, {}).eax")
t = t.replace("c3ex_vslot(eng, 0x6c)(eng, 0)", "snd_vcall(ctx, eng, 0x6c, SndAbi::Thiscall, {}).eax")
t = t.replace("    *out = -1;  // 0x39f38", "    st<uint32_t>(ctx, out, 0xffffffffu);  // 0x39f38")
t = t.replace("""    void** vt = ld<u32>(ctx, self);
    if (static_cast<Th0_t>(vt[0xcc / 4])(self, 0) != 0)  // 0x3a0f6, 0x3a0fe""",
              """    if (snd_vcall(ctx, self, 0xcc, SndAbi::Thiscall, {}).eax != 0)  // 0x3a0f6, 0x3a0fe""")
t = t.replace("""        void** vt = ld<u32>(ctx, self);
        static_cast<SlotA_t>(vt[0x48 / 4])(self, 0, 1);  // 0x3a3ed""",
              """        snd_vcall(ctx, self, 0x48, SndAbi::Thiscall, {1});  // 0x3a3ed""")
P.write_text(t, encoding="utf-8")
print("ok")

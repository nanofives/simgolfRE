import re, sys, pathlib
sys.path.insert(0, "log/roadmap/S4")
from importlib import import_module
B = import_module("7_blk")
rb, sub = B.replace_body, B.sub
P = B.P

# ---- regex-wide cleanups of memory lvalues
t = P.read_text(encoding="utf-8")
t = re.sub(r"c3fk_gref\(ctx, (\w+)\) = ([^;]+);", r"st<u32>(ctx, snd_va(ctx, \1), \2);", t)
t = re.sub(r"ld<uint32_t>\(ctx, self \+ 0x44\) \|= (0x[0-9a-f]+u);", r"st<uint32_t>(ctx, self + 0x44, ld<uint32_t>(ctx, self + 0x44) | \1);", t)
t = t.replace("strcmp(ld<u32>(ctx, p), nm) == 0", "str_diff(ctx, ld<u32>(ctx, p), nm) == 0")
t = t.replace("strcmp(on, alt) != 0", "str_diff(ctx, on, alt) != 0")
t = t.replace("strcmp(ld<u32>(ctx, old), on) == 0", "str_diff(ctx, ld<u32>(ctx, old), on) == 0")
t = t.replace("if (--ld<uint32_t>(ctx, old + 0x210) == 0) {",
              "st<uint32_t>(ctx, old + 0x210, ld<uint32_t>(ctx, old + 0x210) - 1u);\n        if (ld<uint32_t>(ctx, old + 0x210) == 0) {")
P.write_text(t, encoding="utf-8")

# ---- shared list pop of c3fk (Sound5b6a8_dtor / Sound5c450_dtor): head / tail are the addresses of the header words
t = P.read_text(encoding="utf-8")
a = t.index("u32 c3fk_popHead(")
b = t.index("// 0x0001ff50  Sound5b6a8_dtor")
t = t[:a] + """u32 c3fk_popHead(Ctx &ctx, u32 head, u32 tail) {
    u32 n = ld<u32>(ctx, head);
    u32 next = ld<u32>(ctx, n + 4);
    if (next) {
        st<u32>(ctx, next, 0);
        st<u32>(ctx, head, next);
    } else {
        st<u32>(ctx, tail, 0);
        st<u32>(ctx, head, 0);
    }
    return n;
}
""" + t[b:]
P.write_text(t, encoding="utf-8")
sub("""        void** slot = (self + 0x744 + 4 * k);
        if (u32 r = *slot) {""", """        const u32 slot = self + 0x744 + 4 * k;
        if (u32 r = ld<u32>(ctx, slot)) {""")
sub("            *slot = 0;  // 0x1ffbc", "            st<u32>(ctx, slot, 0);  // 0x1ffbc")

# ---- Sound5b6a8_ctor
rb("Sound5b6a8_ctor", """// the tempo seed of Seq_setTempo with tempo 0x78 on the fresh fields: x87 arithmetic under the caller's control word (port/CORE.md section 7)
uint32_t Sound5b6a8_ctor_body(Ctx &ctx, u32 self) {
    snd_call(ctx, 0x000015f5, SndAbi::Thiscall, {self});  // 0x1f4c1
    snd_call(ctx, 0x00001b1d, SndAbi::Thiscall, {self + 0x60});  // 0x1f4c8 / 0x1f4cf
    for (int off = 0x218; off <= 0x234; off += 4) st<uint32_t>(ctx, self + off, 0);  // 0x1f4d4-0x1f4fe
    snd_call(ctx, 0x00042902, SndAbi::Stdcall, {self + 0x250, 0x4c, 0x10, snd_va(ctx, 0x2667), snd_va(ctx, 0x166d)});  // 0x1f504-0x1f51e
    st<u32>(ctx, self, snd_va(ctx, snd_rva::vt_Sound5b6a8));  // 0x1f52a vtable 0x5b6a8
    st<uint32_t>(ctx, self + 0x214, 0);  // 0x1f533
    st<uint32_t>(ctx, self + 0x214, 0x200);  // 0x1f523 / 0x1f530 (`or ch, 2` on 0) / 0x1f53d
    st<uint32_t>(ctx, self + 0x710, 4);  // 0x1f543
    st<uint32_t>(ctx, self + 0x714, 4);  // 0x1f549
    st<uint32_t>(ctx, self + 0x728, 0);  // 0x1f54f
    st<uint32_t>(ctx, self + 0x720, 0);  // 0x1f555
    st<uint32_t>(ctx, self + 0x210, 0);  // 0x1f568
    st<uint32_t>(ctx, self + 0x72c, 0);  // 0x1f56e
    st<uint32_t>(ctx, self + 0x71c, 0);  // 0x1f574
    st<uint32_t>(ctx, self + 0x724, 0);  // 0x1f57a
    st<uint32_t>(ctx, self + 0x730, 0x7f);  // 0x1f580
    for (int k = 0; k < 0x80; ++k) st<uint32_t>(ctx, self + 0x744 + 4 * k, 0);  // 0x1f55b-0x1f58c rep stosd, 0x80 dwords
    snd_call(ctx, 0x000018a2, SndAbi::Thiscall, {self, 2});  // 0x1f58a-0x1f590 Sound_setSourceType
    st<uint32_t>(ctx, self + 0x238, 0); st<uint32_t>(ctx, self + 0x23c, 0); st<uint32_t>(ctx, self + 0x240, 0);  // 0x1f595-0x1f5a1
    st<uint32_t>(ctx, self + 0x248, 0); st<uint32_t>(ctx, self + 0x24c, 0);  // 0x1f5a7 / 0x1f5ad
    st<uint32_t>(ctx, self + 0x738, 0xffffffff);  // 0x1f5b3
    st<uint32_t>(ctx, self + 0x73c, 0);  // 0x1f5bd
    X80 T, p, k, q, r;
    x_ldU32(T, ld<uint32_t>(ctx, self + 0x1bc));  // 0x1f5c3-0x1f5d1
    unsigned old = ld<uint32_t>(ctx, self + 0x718);  // 0x1f5d5
    st<uint32_t>(ctx, self + 0x718, 0x78);  // 0x1f5e3
    x_ld32(k, ld<uint32_t>(ctx, snd_va(ctx, 0x0005b6a4)));
    x_mul(p, T, k);  // 0x1f5ed
    x_ld32(k, ld<uint32_t>(ctx, snd_va(ctx, 0x0005b6a0)));
    x_mul(p, p, k);  // 0x1f5f3
    x_ld32(k, ld<uint32_t>(ctx, snd_va(ctx, 0x0005b69c)));
    x_div(p, k, p);  // 0x1f5f9 fdivr dword
    x_st(ctx, self + 0x728, p);  // 0x1f5ff
    x_ldU32(q, old);  // 0x1f5db / 0x1f5df / 0x1f605 fild qword (old, 0)
    x_ld32(k, ld<uint32_t>(ctx, snd_va(ctx, 0x0005b698)));
    x_mul(q, q, k);  // 0x1f609
    x_ld(ctx, r, self + 0x720);  // 0x1f60f
    x_mul(r, r, q);  // 0x1f615 fmul st(1)
    unsigned w = static_cast<uint32_t>(x_ftol(r));  // 0x1f617; 0x1f624 drops q
    st<uint32_t>(ctx, self + 0x740, 0);  // 0x1f62a
    x_ldU32(r, w);  // 0x1f620-0x1f630
    x_st(ctx, self + 0x720, r);  // 0x1f637
    return self;
}""")
sub("void a_Sound5b6a8_ctor(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(",
    "void a_Sound5b6a8_ctor(Ctx &ctx, const uint32_t *a, Ret &r) {\n  PlainDoubles plain(NEG(Sound5b6a8_ctor) == 2);\n  r.set_u32(")
sub('{"Sound5b6a8_ctor", "thiscall", "pointer", 1, 0, a_Sound5b6a8_ctor',
    '{"Sound5b6a8_ctor", "thiscall", "pointer", 1, 0x027f, a_Sound5b6a8_ctor')

# ---- Sound_reapIdle
rb("Sound_reapIdle", """uint32_t Sound_reapIdle_body(Ctx &ctx, u32 unused0) {
    u32 const w = snd_obj(ctx, &SoundState::g_cmdTableHead).va();  // list header {head +0, tail +4, cursor +8, count +0xc}
    u32 node = ld<u32>(ctx, w);  // 0x204e0
    st<u32>(ctx, w + 8, node);  // 0x204e8
    if (!node) return 0;  // 0x204e6 / 0x204ed
    u32 sound = ld<u32>(ctx, node + 8);  // 0x204f3
    if (!sound) return node;  // 0x204f6 / 0x204f8
    for (;;) {
        bool keep = ld<int32_t>(ctx, sound + 0x210) > 0;  // 0x204fe / 0x20504 / 0x20506
        if (!keep) {
            unsigned al = snd_call(ctx, 0x00001942, SndAbi::Thiscall, {sound}).eax & 0xff;  // 0x2050a / 0x2050f
            node = ld<u32>(ctx, w + 8);  // 0x20511
            keep = al != 0;  // 0x20516
        }
        if (!keep) {
            if (node) {  // 0x20518 / 0x2051a
                u32 prev = ld<u32>(ctx, node);  // 0x2051c
                if (prev) st<u32>(ctx, prev + 4, ld<u32>(ctx, node + 4));  // 0x20520 / 0x20522 / 0x20525
                else st<u32>(ctx, w, ld<u32>(ctx, node + 4));  // 0x2052d
                u32 next = ld<u32>(ctx, node + 4);  // 0x20533
                if (next) {  // 0x20536 / 0x20538
                    st<u32>(ctx, next, ld<u32>(ctx, node));  // 0x2053a / 0x2053c
                    st<u32>(ctx, w + 8, next);  // 0x2053e
                } else {
                    st<u32>(ctx, w + 8, 0);  // 0x20548
                    st<u32>(ctx, w + 4, ld<u32>(ctx, node));  // 0x20552
                }
                snd_delete(ctx, node);  // 0x20559
                st<uint32_t>(ctx, w + 0xc, ld<uint32_t>(ctx, w + 0xc) - 1u);  // 0x2055e-0x20567
            }
            snd_call(ctx, 0x00001d66, SndAbi::Thiscall, {sound});  // 0x2056e
            snd_delete(ctx, sound);  // 0x20574
            node = ld<u32>(ctx, w + 8);  // 0x20579
            if (!node) return 0;  // 0x2057e / 0x20583
        } else {
            if (!node) return 0;  // 0x20587 / 0x20589
            node = ld<u32>(ctx, node + 4);  // 0x2058b
            st<u32>(ctx, w + 8, node);  // 0x20590
            if (!node) return 0;  // 0x20590 / 0x20595
        }
        sound = ld<u32>(ctx, node + 8);  // 0x20597
        if (!sound) return node;  // 0x2059a / 0x2059c
    }
}""")

# ---- Engine_playSound
rb("Engine_playSound", """uint32_t Engine_playSound_body(Ctx &ctx, u32 self, u32 name) {
    if (!ld<uint32_t>(ctx, snd_va(ctx, 0xb507c))) return 1;  // 0x2add1-0x2ade1
    const u32 slot = self + 0x40;  // 0x2adf2
    if (!ld<u32>(ctx, slot)) {  // 0x2adf5 / 0x2adf7
        unsigned c = snd_call(ctx, 0x0000137a, SndAbi::Cdecl, {slot, name, 1}).eax;  // 0x2adfd
        if (c) return c;  // 0x2ae05 / 0x2ae07
    } else if (snd_vcall(ctx, ld<u32>(ctx, slot), 0x60, SndAbi::Thiscall, {}).eax) {  // 0x2ae10-0x2ae17
        return 0xf;  // 0x2ae1b
    }
    unsigned r = snd_vcall(ctx, ld<u32>(ctx, slot), 0x10, SndAbi::Thiscall, {name}).eax;  // 0x2ae25-0x2ae2d
    unsigned f = ld<uint32_t>(ctx, self + 0x44);  // 0x2ae33
    if (r) {
        st<uint32_t>(ctx, self + 0x44, f & ~1u);  // 0x2ae5a / 0x2ae5c
    } else if (!(f & 1)) {  // 0x2ae38 / 0x2ae3a
        st<uint32_t>(ctx, self + 0x44, f | 1);  // 0x2ae3c-0x2ae40
        snd_vcall(ctx, self, 0x7c, SndAbi::Thiscall, {});  // 0x2ae45
        if (ld<uint32_t>(ctx, self + 0x30))  // 0x2ae48-0x2ae4d
            snd_vcall(ctx, ld<u32>(ctx, slot), 0x48, SndAbi::Thiscall, {1});  // 0x2ae4f-0x2ae55
    }
    unsigned n = static_cast<uint32_t>(vstrlen(ctx.mem, name)) + 1;  // 0x2ae60-0x2ae69
    u32 copy = snd_new(ctx, n);  // 0x2ae6c
    rep_movs(ctx.mem, copy, name, n);  // 0x2ae73-0x2ae97
    if (ld<u32>(ctx, self + 0x50)) snd_delete(ctx, ld<u32>(ctx, self + 0x50));  // 0x2ae99-0x2aea2
    st<u32>(ctx, self + 0x50, copy);  // 0x2aeb3
    return r;  // 0x2aeae
}""")

# ---- getPlaybackTime
rb("getPlaybackTime", """int32_t getPlaybackTime_body(Ctx &ctx, u32 self) {
    u32 device = ld<u32>(ctx, self + 0x40);  // 0x2baa3
    if (device) return static_cast<int32_t>(snd_vcall(ctx, device, 0x5c, SndAbi::Thiscall, {}).eax);  // jne 0x2bad8 at 0x2baa8; call at 0x2bada
    unsigned state = ld<uint32_t>(ctx, self + 0x58);  // 0x2baaa
    if (((state >> 4) & 1) == 0) return 0;  // 0x2baad-0x2bab2, then 0x2bad4
    if (ld<uint32_t>(ctx, self + 0x68) == 0) return 0;  // 0x2bab4, je 0x2badf at 0x2bab9
    unsigned now = snd_import(ctx, 0x000b8654, SndAbi::Stdcall, {}).eax;  // the timeGetTime import slot called at 0x2babb
    unsigned elapsed = now - ld<uint32_t>(ctx, self + 0x68);  // re-read at 0x2bac1, sub at 0x2bac7
    if (ld<uint32_t>(ctx, self + 0x64) <= elapsed) return 0;  // jbe 0x2badf at 0x2bacb
    return 1;  // 0x2bacd
}""")

# ---- Sound5c450_ctor
sub("    for (int off = 0x6c; off < 0x6c + 0x12 * 4; off += 4) ld<uint32_t>(ctx, self + off) = 0;  // 0x300d7",
    "    for (int off = 0x6c; off < 0x6c + 0x12 * 4; off += 4) st<uint32_t>(ctx, self + off, 0);  // 0x300d7")

# ---- Sound_resetBuffers family
sub("constexpr uint32_t kIatMmioClose = 0xb8670;  // c3fk.cpp",
    "constexpr uint32_t kIatMmioClose = 0xb8670;  // c3fk.cpp\nconstexpr uint32_t kMmioClose = kIatMmioClose;  // c3fg.cpp names the same slot")
print("ok")

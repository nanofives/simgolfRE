"""S4.7 in-place fixes of the assembled port/src/sim/audio_sound.cpp (applied once; the .cpp is the source from here on)."""
import pathlib, re, sys
P = pathlib.Path("port/src/sim/audio_sound.cpp")
t = P.read_text(encoding="utf-8")

def rep(old, new, count=1):
    global t
    assert t.count(old) >= 1, old[:80]
    if count == 1:
        assert t.count(old) == 1, ("ambiguous", old[:80], t.count(old))
    t = t.replace(old, new)

# ---- shared helpers after fcell
rep("""u32 fcell(Ctx &c, int cell) { return snd_base(c) + kSoundImageSize - 0x4e0u + 0x10u * static_cast<u32>(cell); }
""", """u32 fcell(Ctx &c, int cell) { return snd_base(c) + kSoundImageSize - 0x4e0u + 0x10u * static_cast<u32>(cell); }

// `strcmp` the original inlines (`mov al, [a]; cmp al, [b]; jne; test al, al; je ...`): 0 when the two NUL-terminated byte strings are equal.
int str_diff(Ctx &c, u32 a, u32 b) {
  for (;; ++a, ++b) {
    const unsigned x = ld<uint8_t>(c, a), y = ld<uint8_t>(c, b);
    if (x != y) return x < y ? -1 : 1;
    if (x == 0) return 0;
  }
}
""")

# ---- Sound::dtor
rep("""    void** next = (n[1]);
    if (next) {
        next[0] = 0;
        st<u32>(ctx, self + hOff, next);
    } else {
        st<u32>(ctx, self + hOff + 4, 0);
        st<u32>(ctx, self + hOff, 0);
    }
    u32 payload = n[2];
    snd_delete(ctx, n);
    --ld<int32_t>(ctx, self + cOff);
    return payload;""", """    u32 next = ld<u32>(ctx, n + 4);
    if (next) {
        st<u32>(ctx, next, 0);
        st<u32>(ctx, self + hOff, next);
    } else {
        st<u32>(ctx, self + hOff + 4, 0);
        st<u32>(ctx, self + hOff, 0);
    }
    u32 payload = ld<u32>(ctx, n + 8);
    snd_delete(ctx, n);
    st<uint32_t>(ctx, self + cOff, ld<uint32_t>(ctx, self + cOff) - 1u);
    return payload;""")
rep("""    void** n = ld<u32>(ctx, self + hOff);
    while (n) {
        if (!c3fd_popNode(ctx, self, hOff, cOff, n)) break;""", """    u32 n = ld<u32>(ctx, self + hOff);
    while (n) {
        if (!c3fd_popNode(ctx, self, hOff, cOff, n)) break;""")

# ---- SoundTimer::unregisterClient
rep("""    u32 vtbl = ld<u32>(ctx, client);  // 0x8953
    static_cast<Th0v_t>(ld<u32>(ctx, vtbl + 0xc))(client, 0);  // 0x8957""",
    """    snd_vcall(ctx, client, 0xc, SndAbi::Thiscall, {});  // 0x8953-0x8957""")

# ---- SoundTimer::stop: popFront returns the delete's eax through a cell
rep("""u32 c3ew_popFront(Ctx &ctx, u32 list, int32_t frontOff, int32_t backOff, int32_t countOff, u32 node, u32 ret) {""",
    """u32 c3ew_popFront(Ctx &ctx, u32 list, int32_t frontOff, int32_t backOff, int32_t countOff, u32 node, u32 *ret) {""")
rep("""    for (int i = 0; i < 6; i++) cs[i] = 0;  // 0x8b34-0x8b40""", """    for (int i = 0; i < 6; i++) st<uint32_t>(ctx, cs + 4 * i, 0);  // 0x8b34-0x8b40""")

# ---- SoundTimer::onTick: the message buffer is a frame cell block (0x100 bytes = 16 cells)
rep("""                char msg[0x100];
                snd_call(ctx, 0x00042577, SndAbi::Cdecl, {msg,""", """                const u32 msg = fcell(ctx, 0);  // the 0x100-byte stack buffer (cells 0..15)
                snd_call(ctx, 0x00042577, SndAbi::Cdecl, {msg,""")

# ---- create_sound: strstr is CRT code reached through the dispatcher
rep("""        Strstr_t ss = snd_va(ctx, 0x42690);  // 0x42690 strstr
        if (ss(name, c3fq_str(ctx, 0x00063dc0))) t = 1;  // 0x940d-0x941f "wav"
        else if (ss(name, c3fq_str(ctx, 0x00063dbc))) t = 2;  // 0x9426-0x9438 "mid"
        else if (ss(name, c3fq_str(ctx, 0x00063db8))) t = 3;  // 0x943f-0x9451 "aif"
        else t = ss(name, c3fq_str(ctx, 0x00063db4)) ? 2 : 0;  // 0x9458-0x946a "rmi\"""",
    """        auto ss = [&](uint32_t rva) { return snd_call(ctx, 0x00042690, SndAbi::Cdecl, {name, c3fq_str(ctx, rva)}).eax; };  // 0x42690 strstr
        if (ss(0x00063dc0)) t = 1;  // 0x940d-0x941f wav extension
        else if (ss(0x00063dbc)) t = 2;  // 0x9426-0x9438 mid extension
        else if (ss(0x00063db8)) t = 3;  // 0x943f-0x9451 aif extension
        else t = ss(0x00063db4) ? 2 : 0;  // 0x9458-0x946a rmi extension""")
P.write_text(t, encoding="utf-8")
print("ok")

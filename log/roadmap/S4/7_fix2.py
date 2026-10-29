import sys
sys.path.insert(0, "log/roadmap/S4")
from importlib import import_module
B = import_module("7_blk")
rb, sub = B.replace_body, B.sub


rb("Sound_initEntry", """u32 Sound_initEntry_body(Ctx &ctx, u32 self) {
    for (u32 p = self + 4, n = 0x80; n != 0; --n, p += 4) st<uint32_t>(ctx, p, 0);
    st<uint32_t>(ctx, self, 0);
    st<uint32_t>(ctx, self + 0x214, 0);
    st<uint32_t>(ctx, self + 0x204, 0);
    st<uint32_t>(ctx, self + 0x208, 0);
    st<uint32_t>(ctx, self + 0x20c, 0);
    st<uint32_t>(ctx, self + 0x210, 0);
    st<uint32_t>(ctx, self + 0x218, 0xfa);
    return self;
}""")

rb("Sound_setName", """void Sound_setName_body(Ctx &ctx, u32 self, u32 name) {
    const u32 buf = fcell(ctx, 0);  // the 0x100-byte stack buffer (cells 0..15)
    if (ld<u32>(ctx, self + 0x1c) != 0) snd_delete(ctx, ld<u32>(ctx, self + 0x1c));  // 0x1cee0 / 0x1cee5
    st<u32>(ctx, self + 0x1c, 0);  // 0x1cef4
    if (name == 0) return;  // 0x1cef7 / 0x1cef9 / 0x1cefb
    u32 prefix = ld<u32>(ctx, self + 0x20);  // 0x1cf0b
    if (prefix != 0) {  // 0x1cf0e
        unsigned n = 0;
        while (ld<int8_t>(ctx, prefix + n)) { st<int8_t>(ctx, buf + n, ld<int8_t>(ctx, prefix + n)); n++; }  // 0x1cf16-0x1cf34
        st<int8_t>(ctx, buf + n, 0);
        u32 slash = snd_call(ctx, 0x000424b0, SndAbi::Cdecl, {name, 0x5c}).eax;  // 0x1cf36 (strrchr 0x424b0, backslash)
        u32 src = slash != 0 ? slash + 1 : name;  // 0x1cf3e-0x1cf47
        unsigned m = 0;
        while (ld<int8_t>(ctx, src + m)) { st<int8_t>(ctx, buf + n + m, ld<int8_t>(ctx, src + m)); m++; }  // 0x1cf47-0x1cf6d
        st<int8_t>(ctx, buf + n + m, 0);
    } else {
        unsigned n = 0;
        while (ld<int8_t>(ctx, name + n)) { st<int8_t>(ctx, buf + n, ld<int8_t>(ctx, name + n)); n++; }  // 0x1cf6f-0x1cf8e
        st<int8_t>(ctx, buf + n, 0);
    }
    unsigned len = 0;
    while (ld<int8_t>(ctx, buf + len)) len++;  // 0x1cf90-0x1cf9b
    u32 p = snd_new(ctx, len + 1);  // 0x1cf9c
    st<u32>(ctx, self + 0x1c, p);  // 0x1cfaf
    for (unsigned i = 0; i <= len; i++) st<int8_t>(ctx, p + i, ld<int8_t>(ctx, buf + i));  // 0x1cfb2-0x1cfc8
}""")

rb("create_sound", """u32 c3fq_str(Ctx &ctx, uint32_t rva) { return snd_va(ctx, rva); }
int32_t create_sound_body(Ctx &ctx, u32 out, u32 name, uint32_t type) {
    unsigned t;
    if (type) t = type;  // 0x93fe / 0x9400
    else if (!name) t = 1;  // 0x9402-0x9406
    else {
        auto ss = [&](uint32_t rva) { return snd_call(ctx, 0x00042690, SndAbi::Cdecl, {name, c3fq_str(ctx, rva)}).eax; };  // 0x42690 strstr
        if (ss(0x00063dc0)) t = 1;  // 0x940d-0x941f wav extension
        else if (ss(0x00063dbc)) t = 2;  // 0x9426-0x9438 mid extension
        else if (ss(0x00063db8)) t = 3;  // 0x943f-0x9451 aif extension
        else t = ss(0x00063db4) ? 2 : 0;  // 0x9458-0x946a rmi extension
    }
    unsigned idx = t - 1;  // 0x946d
    if (idx > 7) return 0xb;  // 0x946e-0x9471 default
    switch (idx) {
    case 0: {  // 0x947e MultiSound, 0x1450
        u32 o = snd_new(ctx, 0x1450);  // 0x947e-0x9483
        if (o) snd_call(ctx, 0x000026ad, SndAbi::Thiscall, {o, ld<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_soundWindow).va())});  // 0x949b-0x94a4
        st<u32>(ctx, out, o);  // 0x94ba
        snd_call(ctx, 0x000021b7, SndAbi::Thiscall, {o, name});  // 0x94be Object_setName
        return 0;  // 0x94c3
    }
    case 1:  // 0x94d5 Sound5b6a8, 0x948
        if (!ld<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_midiDevice).va())) return 3;  // 0x94d5-0x94de
        {
            u32 o = snd_new(ctx, 0x948);  // 0x94f3-0x94f8
            if (o) snd_call(ctx, 0x00002513, SndAbi::Thiscall, {o});  // 0x9510-0x9512 Sound5b6a8_ctor
            st<u32>(ctx, out, o);  // 0x952a
            snd_call(ctx, 0x0000105f, SndAbi::Thiscall, {o, name});  // 0x952c Sound_assignName
            return 0;  // 0x9531
        }
    case 2: return 0xb;  // 0x96c9 (jump-table default slot)
    case 3: {  // 0x9543 WaveStream, 0xec
        u32 o = snd_new(ctx, 0xec);  // 0x9543-0x9548
        if (o) snd_call(ctx, 0x0000138e, SndAbi::Thiscall, {o});  // 0x9564-0x9566 WaveStream::ctor
        st<u32>(ctx, out, o);  // 0x956f
        return 0;  // 0x9571
    }
    case 4:  // 0x9583 Sound, 0xa60
        if (!ld<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_midiDevice).va())) return 3;  // 0x9583-0x958c
        {
            u32 o = snd_new(ctx, 0xa60);  // 0x95a1-0x95a6
            if (o) snd_call(ctx, 0x00001163, SndAbi::Thiscall, {o});  // 0x95c4 Sound::ctor
            st<u32>(ctx, out, o);  // 0x95cd
            return 0;  // 0x95cf
        }
    case 5: {  // 0x95e1 Sound5c450, 0xe8
        u32 o = snd_new(ctx, 0xe8);  // 0x95e1-0x95e6
        if (o) snd_call(ctx, 0x000018c5, SndAbi::Thiscall, {o});  // 0x9604 Sound5c450_ctor
        st<u32>(ctx, out, o);  // 0x960d
        return 0;  // 0x960f
    }
    case 6: {  // 0x9621 StreamReader, 0x94
        u32 o = snd_new(ctx, 0x94);  // 0x9621-0x9626
        if (o) snd_call(ctx, 0x0000140b, SndAbi::Thiscall, {o});  // 0x9640 StreamReader_ctor
        st<u32>(ctx, out, o);  // 0x9649
        return 0;  // 0x964b
    }
    case 7: {  // 0x9675 Ch5c3a4, 0x10c
        u32 o = snd_new(ctx, 0x10c);  // 0x9675-0x967a
        if (o) snd_call(ctx, 0x0000237e, SndAbi::Thiscall, {o});  // 0x9694 Ch5c3a4_ctor
        st<u32>(ctx, out, o);  // 0x969d
        return 0;  // 0x969f
    }
    }
    return 0xb;  // unreachable (idx 0..7 all handled)
}""")
print("ok")

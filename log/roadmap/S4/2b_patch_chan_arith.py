"""S4.2b: the S4.6 bodies whose x87 arithmetic was written as C doubles compute on the x87 unit at its control word (they were
judged at 0x027f only, where 53-bit doubles rounded to nearest are the unit's own arithmetic): Channel_setRateGlide,
Channel_setPanGains, Voice_setLevel (log/roadmap/S4/2b_x87scan.tsv). Voice_setLevel calls Voice_setPanGains through its thunk
0x231a as the original does (0x383df), so a stand-in claiming the thunk serves it.

    ~/bin/py -3.12 log/roadmap/S4/2b_patch_chan_arith.py [--check]
"""
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
P = ROOT / "port" / "src" / "sim" / "audio_chan.cpp"
t = P.read_text(encoding="utf-8")


def rep(old, new, n=1):
    global t
    assert t.count(old) == n, (old[:90], t.count(old))
    t = t.replace(old, new)


rep("""    const double scale = ld<double>(ctx, snd_va(ctx, 0x5c5e8));
    const double s = static_cast<double>(start) * scale;  // 0x34cab, 0x34cb1
    st<double>(ctx, self + 0x1d0, s);  // 0x34cb7
    const double e = (end != 0) ? static_cast<double>(end) * scale  // 0x34cbf, 0x34cc3
                                : ld<double>(ctx, snd_va(ctx, 0x5b498));  // 0x34ccb
    st<double>(ctx, self + 0x1e0, e);  // 0x34cd1
    u32 timer = c3cx_timer(ctx);  // 0x34cd7
    if (!timer) return 0;  // je 0x34d57 at 0x34cde
    const unsigned period = ld<uint32_t>(ctx, timer + (0) * 4);  // 0x34ce1
    if (duration > period) {  // jbe at 0x34ce9
        const unsigned n = udiv32(duration, period);  // 0x34cf5
        const double diff = e - s;  // 0x34cf7
        st<double>(ctx, self + 0x1c8, diff / static_cast<double>(n));  // 0x34d05..0x34d10
        st<uint32_t>(ctx, self + 0x1c4, (static_cast<uint32_t>(end) - static_cast<uint32_t>(start)) / n);  // 0x34cfd..0x34d16
    } else {
        const int d = static_cast<int32_t>(static_cast<uint32_t>(end) - static_cast<uint32_t>(start));  // 0x34d2f (wraps)
        st<int32_t>(ctx, self + 0x1c4, d);  // 0x34d39
        st<double>(ctx, self + 0x1c8, static_cast<double>(d));  // 0x34d3f, 0x34d4a
    }""", """    const X80 scale = x87::from_f64(ld<uint64_t>(ctx, snd_va(ctx, 0x5c5e8)));
    X80 s, e, t;
    x_ldI32(ctx, s, start);  // fild dword (0x34cab)
    x_mul(ctx, s, s, scale);  // fmul qword (0x34cb1)
    x_st(ctx, self + 0x1d0, s);  // fst qword (0x34cb7)
    if (end != 0) {
        x_ldI32(ctx, e, end);  // fild dword (0x34cbf)
        x_mul(ctx, e, e, scale);  // fmul qword (0x34cc3)
    } else {
        x_ld(ctx, e, snd_va(ctx, 0x5b498));  // fld qword 0.0 (0x34ccb)
    }
    x_st(ctx, self + 0x1e0, e);  // fst qword (0x34cd1)
    u32 timer = c3cx_timer(ctx);  // 0x34cd7
    if (!timer) return 0;  // je 0x34d57 at 0x34cde (two fstp st(0))
    const unsigned period = ld<uint32_t>(ctx, timer + (0) * 4);  // 0x34ce1
    if (duration > period) {  // jbe at 0x34ce9
        const unsigned n = udiv32(duration, period);  // 0x34cf5
        x_sub(ctx, e, e, s);  // fsub st(1) (0x34cf7)
        x_ldU32(ctx, t, n);  // fild qword (0x34d05)
        x_div(ctx, e, e, t);  // fdivp st(1) (0x34d0b)
        x_st(ctx, self + 0x1c8, e);  // fstp qword (0x34d10)
        st<uint32_t>(ctx, self + 0x1c4, (static_cast<uint32_t>(end) - static_cast<uint32_t>(start)) / n);  // 0x34cfd..0x34d16
    } else {
        const int d = static_cast<int32_t>(static_cast<uint32_t>(end) - static_cast<uint32_t>(start));  // 0x34d2f (wraps)
        st<int32_t>(ctx, self + 0x1c4, d);  // 0x34d39
        x_ldI32(ctx, t, d);  // fild dword (0x34d3f)
        x_st(ctx, self + 0x1c8, t);  // fstp qword (0x34d4a)
    }""")
rep("""    const double scale = ld<double>(ctx, snd_va(ctx, 0x5c5e8));  // 1/65536
    st<int32_t>(ctx, self + 0x1e8, gainLow);
    st<int32_t>(ctx, self + 0x1f8, gainHigh);
    st<double>(ctx, self + 0x200, static_cast<double>(gainHigh) * scale);
    st<double>(ctx, self + 0x1f0, static_cast<double>(gainLow) * scale);""",
    """    const X80 scale = x87::from_f64(ld<uint64_t>(ctx, snd_va(ctx, 0x5c5e8)));  // 1/65536
    st<int32_t>(ctx, self + 0x1e8, gainLow);
    st<int32_t>(ctx, self + 0x1f8, gainHigh);
    X80 g;
    x_ldI32(ctx, g, gainHigh);  // fild dword (0x353e6)
    x_mul(ctx, g, g, scale);  // fmul qword (0x353f7)
    x_st(ctx, self + 0x200, g);  // fstp qword (0x353fd)
    x_ldI32(ctx, g, gainLow);  // fild dword (0x35403)
    x_mul(ctx, g, g, scale);  // fmul qword (0x35407)
    x_st(ctx, self + 0x1f0, g);  // fstp qword (0x3540d)""")
rep("""    const double scale = ld<double>(ctx, snd_va(ctx, snd_rva::g_levelScale));
    st<double>(ctx, self + 0x58, static_cast<double>(level) * scale);
    st<uint32_t>(ctx, self + 0x50, (level + (level != 0 ? 1u : 0u)) << 9);
    snd_call(ctx, 0x00037f30, SndAbi::Thiscall, {self, ld<uint32_t>(ctx, self + 0x48)});  // Voice_setPanGains""",
    """    X80 v;
    x_ldU32(ctx, v, level);  // fild qword of the zero-extended level (0x383b7-0x383c3)
    x_mul(ctx, v, v, x87::from_f64(ld<uint64_t>(ctx, snd_va(ctx, snd_rva::g_levelScale))));  // fmul qword (0x383c9)
    x_st(ctx, self + 0x58, v);  // fstp qword (0x383cf)
    st<uint32_t>(ctx, self + 0x50, (level + (level != 0 ? 1u : 0u)) << 9);
    snd_call(ctx, 0x0000231a, SndAbi::Thiscall, {self, ld<uint32_t>(ctx, self + 0x48)});  // Voice_setPanGains thunk (0x383df)""")
if "--check" in sys.argv:
    print("ok (not written)")
else:
    P.write_text(t, encoding="utf-8")
    print(f"wrote {P}")

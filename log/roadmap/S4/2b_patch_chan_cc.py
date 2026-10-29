"""S4.2b: the S4.6 bodies that computed their x87 compares as C double comparisons (or plain doubles) now compare on the x87
unit, so they leave the condition codes the original's fcom / fcomp leaves (the x87 exit state the harness compares), and
compute at the unit's control word: the rate step of updateVoiceRate / Channel_startVoice (c3fe_rateStep), updateVoiceRate's
and Voice_noteOff's zero tests, Channel_releaseVoices, Channel_stopVoices, Voice_setPanGains (log/roadmap/S4/2b_x87scan.tsv).

    ~/bin/py -3.12 log/roadmap/S4/2b_patch_chan_cc.py [--check]
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


# a helper after the x87 helpers
rep("""inline void x_st(Ctx &c, u32 va, const X80 &a) { st<uint64_t>(c, va, x_st64(c, a)); }   // fst / fstp qword [va]
""", """inline void x_st(Ctx &c, u32 va, const X80 &a) { st<uint64_t>(c, va, x_st64(c, a)); }   // fst / fstp qword [va]
// `fld qword [va]; fcomp qword [K0]` with K0 the double 0.0 at rva 0x5b498: the condition codes on the unit, C3 returned
// (`test ah, 0x40`: equal, or unordered: a NaN sets C0, C2 and C3)
inline bool x_eq0(Ctx &c, u32 va) {
  X80 a, k;
  x_ld(c, a, va);
  k = x87::from_f64(ld<uint64_t>(c, snd_va(c, 0x5b498)));
  return (x_com(c, a, k) & x87::kC3) != 0;
}
""")

rep("""// `stepOff`, the rate at `r1Off` (fst) and `r2Off` (fstp). Judged at the word 0x027f (53-bit): plain doubles equal it.
void c3fe_rateStep(Ctx &ctx, u32 v, uint32_t R, uint32_t B, u32 nOff, u32 dOff, u32 stepOff, u32 r1Off, u32 r2Off) {
    double dr = static_cast<double>(static_cast<unsigned long long>(R));
    double db = static_cast<double>(static_cast<unsigned long long>(B));
    if (db > dr) {
        double q = db / dr;
        double step = static_cast<double>(ld<int32_t>(ctx, v + nOff)) / q;
        st<uint32_t>(ctx, v + stepOff, ftol_low32(step));
        double rate = ld<double>(ctx, v + dOff) / q;
        st<double>(ctx, v + r1Off, rate);
        st<double>(ctx, v + r2Off, rate);
    } else {
        st<uint32_t>(ctx, v + stepOff, ld<uint32_t>(ctx, v + nOff));
        const uint64_t rate = quiet64(ld<uint64_t>(ctx, v + dOff));
        st<uint64_t>(ctx, v + r1Off, rate);
        st<uint64_t>(ctx, v + r2Off, rate);
    }
}
// fcomp qword [0.0] then `test ah, 0x40` (C3): taken for equal AND for unordered (a NaN sets C0, C2 and C3).
bool c3fe_eqOrUnordered0(double x) { return x == 0.0 || x != x; }""",
    """// `stepOff`, the rate at `r1Off` (fst) and `r2Off` (fstp). On the x87 unit at its word since S4.2b (the fcom's condition
// codes stay for the caller).
void c3fe_rateStep(Ctx &ctx, u32 v, uint32_t R, uint32_t B, u32 nOff, u32 dOff, u32 stepOff, u32 r1Off, u32 r2Off) {
    X80 r, b, q, n, d;
    x_ldU32(ctx, r, R);  // fild qword (0x32537)
    x_ldU32(ctx, b, B);  // fild qword (0x32547)
    if (!(x_com(ctx, b, r) & (x87::kC0 | x87::kC3))) {  // fcom st(1); `test ah, 0x41; jne` (0x3254b-0x32552)
        x_div(ctx, q, b, r);  // fdivrp st(1) (0x32554)
        x_ldI32(ctx, n, ld<int32_t>(ctx, v + nOff));  // fild dword (0x32556)
        x_div(ctx, n, n, q);  // fdiv st(1) (0x32559)
        st<uint32_t>(ctx, v + stepOff, static_cast<uint32_t>(x_ftol(ctx, n)));  // __ftol (0x3255b)
        x_ld(ctx, d, v + dOff);
        x_div(ctx, d, d, q);  // fdivr qword (0x32560)
    } else {
        st<uint32_t>(ctx, v + stepOff, ld<uint32_t>(ctx, v + nOff));  // 0x3256b / 0x32575
        x_ld(ctx, d, v + dOff);  // fstp st(0) twice; fld qword (0x3256e-0x32572)
    }
    x_st(ctx, v + r1Off, d);  // fst qword (0x3257b)
    x_st(ctx, v + r2Off, d);  // fstp qword (0x32581)
}""")
rep("""    if (c3fe_eqOrUnordered0(ld<double>(ctx, w + 0x58)) || c3fe_eqOrUnordered0(ld<double>(ctx, w + 0xf0))) {  // 0x3258e-0x325af""",
    """    if (x_eq0(ctx, w + 0x58) || x_eq0(ctx, w + 0xf0)) {  // 0x3258e-0x325af (fld; fcomp [0.0]; `test ah, 0x40`)""")

# Channel_releaseVoices
rep("""            const double d1 = ld<double>(ctx, v - 0x44);
            const double d2 = ld<double>(ctx, v + 0x54);
            // C3 (equal or unordered) on either compare takes the clearing arm at 0x34b1e.
            bool clearing = (!(d1 < 0.0) && !(d1 > 0.0)) || (!(d2 < 0.0) && !(d2 > 0.0));""",
    """            // C3 (equal or unordered) on either compare takes the clearing arm at 0x34b1e (fld; fcomp [0.0] at 0x34af9 /
            // 0x34b09, `test ah, 0x40`); the condition codes stay on the unit
            const bool clearing = x_eq0(ctx, v - 0x44) || x_eq0(ctx, v + 0x54);""")
# Channel_stopVoices
rep("""            const double d = ld<double>(ctx, v + 0x54);
            if (!(d >= 0.0)) {  // C0 set: below or unordered (0x34bd4)""",
    """            X80 d, k;
            x_ld(ctx, d, v + 0x54);  // fld qword (0x34bc9)
            k = x87::from_f64(ld<uint64_t>(ctx, snd_va(ctx, 0x5b498)));
            if (x_com(ctx, d, k) & x87::kC0) {  // fcomp [0.0] (0x34bcc); `test ah, 1`: below or unordered (0x34bd4)""")
# Voice_setPanGains
rep("""    const double base = ld<double>(ctx, self + 0x58);
    const double dshare = static_cast<double>(pan) * base / static_cast<double>(divisor);
    int right = share + level;  // lea at 0x37fb5
    int left = level - share;  // sub at 0x37fb8
    double gainRight = dshare + base;  // fadd at 0x37fab
    double gainLeft = base - dshare;  // fsubr at 0x37fb2
    const double one = ld<double>(ctx, snd_va(ctx, 0x5c5f8));  // 1.0
    if (right > 0x10000) right = 0x10000;  // jle at 0x37fc0
    if (gainRight > one) gainRight = one;  // jne at 0x37fd6
    if (left > 0x10000) left = 0x10000;  // jle at 0x37fee
    if (gainLeft > one) gainLeft = one;  // jne at 0x38000
    st<int32_t>(ctx, self + 0x68, right);  // 0x3800e
    st<int32_t>(ctx, self + 0x78, left);  // 0x38015
    st<double>(ctx, self + 0x80, gainLeft);  // fstp qword at 0x38018
    st<double>(ctx, self + 0x70, gainRight);  // 0x3801e, 0x38022""",
    """    X80 base, dshare, gl, one, t;
    x_ldI32(ctx, dshare, pan);  // fild dword (0x37f95)
    base = x87::from_f64(ld<uint64_t>(ctx, self + 0x58));  // fmul qword's operand
    x_mul(ctx, dshare, dshare, base);  // fmul qword [+0x58] (0x37f9b)
    x_ldI32(ctx, t, divisor);
    x_div(ctx, dshare, dshare, t);  // fidiv dword (0x37fa2)
    int right = share + level;  // lea at 0x37fb5
    int left = level - share;  // sub at 0x37fb8
    X80 gr;
    x_add(ctx, gr, dshare, base);  // fld st(0); fadd qword [+0x58] (0x37fa9 / 0x37fab)
    uint64_t gainRight = x_st64(ctx, gr);  // fstp qword [esp+8] (0x37fae)
    x_sub(ctx, gl, base, dshare);  // fsubr qword [+0x58] (0x37fb2)
    one = x87::from_f64(ld<uint64_t>(ctx, snd_va(ctx, 0x5c5f8)));  // 1.0
    if (right > 0x10000) right = 0x10000;  // jle at 0x37fc0
    x_ld64(ctx, t, gainRight);  // fld qword [esp+8] (0x37fc7)
    if (!(x_com(ctx, t, one) & (x87::kC0 | x87::kC3)))  // fcomp [1.0]; `test ah, 0x41; jne` (0x37fcb-0x37fd6)
        gainRight = uint64_t(0x3ff00000u) << 32;  // the two dword stores of 1.0 (0x37fd8 / 0x37fe0)
    if (left > 0x10000) left = 0x10000;  // jle at 0x37fee
    if (!(x_com(ctx, gl, one) & (x87::kC0 | x87::kC3)))  // fcom [1.0]; `test ah, 0x41; jne` (0x37ff5-0x38000)
        x_ld64(ctx, gl, ld<uint64_t>(ctx, snd_va(ctx, 0x5c5f8)));  // fstp st(0); fld qword [1.0] (0x38002 / 0x38004)
    st<int32_t>(ctx, self + 0x68, right);  // 0x3800e
    st<int32_t>(ctx, self + 0x78, left);  // 0x38015
    x_st(ctx, self + 0x80, gl);  // fstp qword at 0x38018
    st<uint64_t>(ctx, self + 0x70, gainRight);  // the dwords of [esp+8] (0x3801e, 0x38022)""")
# Voice_noteOff
rep("""bool c3ar_zeroOrNan(Ctx &ctx, u32 p) {
    unsigned lo = ld<uint32_t>(ctx, p);
    unsigned hi = ld<uint32_t>(ctx, p + 4);
    if ((hi & 0x7ff00000u) == 0x7ff00000u) return ((hi & 0x000fffffu) | lo) != 0;  // NaN: unordered -> C3 set
    return ((hi & 0x7fffffffu) | lo) == 0;  // +0.0 or -0.0 -> C3 set
}
void Voice_noteOff_body(Ctx &ctx, u32 self) {
    if (!c3ar_zeroOrNan(ctx, self + 0x58) &&
        !c3ar_zeroOrNan(ctx, self + 0xf0)) {""",
    """// fld qword; fcomp [0.0]; `test ah, 0x40` (0x381f0-0x381fe, 0x38200-0x38211): C3 = zero or unordered (x_eq0)
void Voice_noteOff_body(Ctx &ctx, u32 self) {
    if (!x_eq0(ctx, self + 0x58) &&
        !x_eq0(ctx, self + 0xf0)) {""")
if "--check" in sys.argv:
    print("ok (not written)")
else:
    P.write_text(t, encoding="utf-8")
    print(f"wrote {P}")

"""S4.2b: S4.5's and S4.7's copies of S4.6's X80 (Win32 asm groups under the thread's word / x64 doubles) replaced by value helpers
over the core's x87 unit at the unit's control word (as 2b_patch_chan.py did for audio_chan): Seq_setTempo, Sound5b6a8_ctor and the
other users of the helpers now give the original's bits at any caller word on x64 and Win32 alike; __ftol through the dispatcher.

    ~/bin/py -3.12 log/roadmap/S4/2b_patch_x80_seq_sound.py [--check]
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
SIM = ROOT / "port" / "src" / "sim"
END = "inline void x_st(Ctx &c, u32 va, const X80 &a) { st<uint64_t>(c, va, x_st64(a)); }   // fst / fstp qword [va]\n"
BLOCK = """// ---------------------------------------------------------------------------------------------------------------- x87
// The x87 code runs on the audio core's virtual x87 unit (portsim/sim/audio.h "the x87 unit", since S4.2b): each helper is one
// x87 operation on 10-byte values under the unit's control word, the word the caller left, on x64 and Win32 alike. __ftol is the
// CRT's through the dispatcher (snd_ftol: the value pushed on the unit, the CRT pops it). Mode 2 of the x87 functions is the classic
// wrong port: every operation a plain double operation (53 bits, nearest, whatever the control word says).
using x87::X80;
int g_x87_plain = 0;
struct PlainDoubles {
  explicit PlainDoubles(bool on) { g_x87_plain = on ? 1 : 0; }
  ~PlainDoubles() { g_x87_plain = 0; }
};
inline double x_d(const X80 &a) { return x87::to_double(a, 0x027f); }
inline X80 x_of(double d) {
  uint64_t b;
  std::memcpy(&b, &d, 8);
  return x87::from_f64(b);
}
inline uint16_t x_cw(Ctx &c) { return x87::unit(c).cw; }
inline void x_ld64(Ctx &, X80 &r, uint64_t bits) { r = x87::fld64(bits); }                  // fld qword (an SNaN is quieted)
inline void x_ld32(Ctx &, X80 &r, uint32_t bits) { r = x87::fld32(bits); }                  // fld dword / an fop dword operand
inline void x_fild64(Ctx &, X80 &r, uint64_t q) { r = x87::fild(static_cast<int64_t>(q)); }  // fild qword
inline void x_fild32(Ctx &, X80 &r, int32_t v) { r = x87::fild(v); }                         // fild dword
inline void x_div(Ctx &c, X80 &r, const X80 &a, const X80 &b) { r = g_x87_plain ? x_of(x_d(a) / x_d(b)) : x87::div(a, b, x_cw(c)); }
inline void x_mul(Ctx &c, X80 &r, const X80 &a, const X80 &b) { r = g_x87_plain ? x_of(x_d(a) * x_d(b)) : x87::mul(a, b, x_cw(c)); }
inline void x_sub(Ctx &c, X80 &r, const X80 &a, const X80 &b) { r = g_x87_plain ? x_of(x_d(a) - x_d(b)) : x87::sub(a, b, x_cw(c)); }
inline void x_add(Ctx &c, X80 &r, const X80 &a, const X80 &b) { r = g_x87_plain ? x_of(x_d(a) + x_d(b)) : x87::add(a, b, x_cw(c)); }
inline void x_chs(Ctx &, X80 &r, const X80 &a) { r = x87::fchs(a); }
inline void x_abs(Ctx &, X80 &r, const X80 &a) { r = x87::fabs_(a); }
// fcom / fcomp / fcompp of a against b: the condition codes go to the unit, returned in status-word positions
inline uint32_t x_com(Ctx &c, const X80 &a, const X80 &b) {
  const uint16_t s = x87::fcom(a, b);
  x87::unit(c).set_cc(s);
  return s;
}
inline uint64_t x_st64(Ctx &c, const X80 &a) { return g_x87_plain ? x87::fst64(a, 0x027f) : x87::fst64(a, x_cw(c)); }
// `call __ftol` with the value in st(0): low dword of the 64-bit chop
inline int32_t x_ftol(Ctx &c, const X80 &a) {
  if (g_x87_plain) return static_cast<int32_t>(ftol_low32(x_d(a)));
  x87::unit(c).push(a);
  return static_cast<int32_t>(snd_ftol(c).eax);
}
inline void x_ldU32(Ctx &c, X80 &r, uint32_t v) { x_fild64(c, r, v); }  // `mov [t], v; mov [t+4], 0; fild qword [t]`
inline void x_ldI32(Ctx &c, X80 &r, int32_t v) { x_fild32(c, r, v); }
inline void x_ld(Ctx &c, X80 &r, u32 va) { x_ld64(c, r, ld<uint64_t>(c, va)); }        // fld qword [va]
inline void x_st(Ctx &c, u32 va, const X80 &a) { st<uint64_t>(c, va, x_st64(c, a)); }   // fst / fstp qword [va]
"""
res = {}
for m in ("seq", "sound"):
    p = SIM / f"audio_{m}.cpp"
    t = p.read_text(encoding="utf-8")
    i = t.index("// ---------------------------------------------------------------------------------------------------------------- x87\n")
    j = t.index(END) + len(END)
    head, body = t[:i] + BLOCK, t[j:]
    body, n = re.subn(r"\bx_(ldU32|ldI32|ld32|div|mul|sub|add|chs|abs|com|ftol)\(", r"x_\1(ctx, ", body)
    assert "__asm" not in body, m
    res[p] = head + body
    print(f"audio_{m}: {n} call sites")
if "--check" not in sys.argv:
    for p, t in res.items():
        p.write_text(t, encoding="utf-8")
    print("written")

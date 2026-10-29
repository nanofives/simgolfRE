"""S4.2b: migrate port/src/sim/audio_chan.cpp onto the core (one-time patch; the result is the deliverable; S4.6's
log/roadmap/S4/6_assemble.py no longer reproduces it).

  * the module's own X80 (Win32 asm groups under the thread's word / x64 doubles) replaced by helpers over the core's x87 unit:
    every operation at the unit's control word (x64 and Win32 alike), a compare sets the unit's condition codes as the
    original's fcom does, __ftol is called through the dispatcher (snd_ftol: the value pushed, the CRT pops it);
    negative-control mode 2 (plain doubles) kept on both architectures;
  * frame_cell (16-byte cells in the image's tail padding, U-1450) replaced by SndFrame at the original's offsets:
    Voice_startPlayback (`push ecx` slot, entry - 4), serviceChannelPlayback (`push ecx` slot, entry - 4), Voice::update
    (`and esp, -8` frame: cells at aligned - 0x18 / - 0x14 / - 8).

    ~/bin/py -3.12 log/roadmap/S4/2b_patch_chan.py [--check]
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
P = ROOT / "port" / "src" / "sim" / "audio_chan.cpp"
t = P.read_text(encoding="utf-8").replace("\r\n", "\n")


def rep(old, new, n=1):
    global t
    assert t.count(old) == n, (old[:80], t.count(old))
    t = t.replace(old, new)


i = t.index("// A local variable the original passes by address (a `push ecx` slot, `&b`)")
j = t.index("inline void x_st(Ctx &c, u32 va, const X80 &a) { st<uint64_t>(c, va, x_st64(a)); }   // fst / fstp qword [va]\n")
j += len("inline void x_st(Ctx &c, u32 va, const X80 &a) { st<uint64_t>(c, va, x_st64(a)); }   // fst / fstp qword [va]\n")
t = t[:i] + """// ---------------------------------------------------------------------------------------------------------------- x87
// The x87 code runs on the audio core's virtual x87 unit (portsim/sim/audio.h "the x87 unit", since S4.2b): each helper is one
// x87 operation on 10-byte values under the unit's control word, the word the caller left (the c3fh caller-word keys run at
// 0x027f and 0x0f7f), on x64 and Win32 alike. A compare sets the unit's condition codes as the original's fcom does (what
// fnstsw reads, and what the caller finds after the return); __ftol is the CRT's through the dispatcher (snd_ftol: the value
// pushed on the unit, the CRT pops it). Mode 2 of the x87 functions is the classic wrong port: every operation a plain
// double operation (53 bits, nearest, whatever the control word says).
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
inline void x_fild64(Ctx &, X80 &r, uint64_t q) { r = x87::fild(static_cast<int64_t>(q)); }  // fild qword
inline void x_fild32(Ctx &, X80 &r, int32_t v) { r = x87::fild(v); }                         // fild dword
inline void x_div(Ctx &c, X80 &r, const X80 &a, const X80 &b) { r = g_x87_plain ? x_of(x_d(a) / x_d(b)) : x87::div(a, b, x_cw(c)); }
inline void x_mul(Ctx &c, X80 &r, const X80 &a, const X80 &b) { r = g_x87_plain ? x_of(x_d(a) * x_d(b)) : x87::mul(a, b, x_cw(c)); }
inline void x_sub(Ctx &c, X80 &r, const X80 &a, const X80 &b) { r = g_x87_plain ? x_of(x_d(a) - x_d(b)) : x87::sub(a, b, x_cw(c)); }
inline void x_add(Ctx &c, X80 &r, const X80 &a, const X80 &b) { r = g_x87_plain ? x_of(x_d(a) + x_d(b)) : x87::add(a, b, x_cw(c)); }
inline void x_chs(Ctx &, X80 &r, const X80 &a) { r = x87::fchs(a); }
inline void x_abs(Ctx &, X80 &r, const X80 &a) { r = x87::fabs_(a); }
// fcom / fcomp / fcompp of a against b: the condition codes go to the unit; returned in status-word positions (C0 0x100:
// a < b, C3 0x4000: equal, C0 C2 C3: unordered), what the original tests with `test ah, 0x41` / `0x01` / `0x40`
inline uint32_t x_com(Ctx &c, const X80 &a, const X80 &b) {
  const uint16_t s = x87::fcom(a, b);
  x87::unit(c).set_cc(s);
  return s;
}
inline uint64_t x_st64(Ctx &c, const X80 &a) {                                              // fst / fstp qword
  return g_x87_plain ? x87::fst64(a, 0x027f) : x87::fst64(a, x_cw(c));
}
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
""" + t[j:]
# the call sites take the context (the helpers above need the unit)
body_start = t.index("// ---- shared helpers")
head, body = t[:body_start], t[body_start:]
body, n = re.subn(r"\bx_(ldU32|ldI32|div|mul|sub|add|chs|abs|com|ftol)\(", r"x_\1(ctx, ", body)
assert n >= 50, n
t = head + body

# the stack
rep("""uint32_t Voice_startPlayback_body(Ctx &ctx, u32 self) {
    if (!(ld<uint8_t>(ctx, self + 0xac) & 1))  // 0x20be4 / 0x20beb""",
    """uint32_t Voice_startPlayback_body(Ctx &ctx, u32 self) {
    SndFrame fr(ctx, 8);  // push ecx; push esi (0x20be0 / 0x20be1)
    if (!(ld<uint8_t>(ctx, self + 0xac) & 1))  // 0x20be4 / 0x20beb""")
rep("""    st<uint32_t>(ctx, self + 0x740, 0);  // 0x20c1b""",
    """    fr.set_esp(fr.entry() - 0x14);  // push ebx; push ebp; push edi (0x20c11-0x20c18)
    st<uint32_t>(ctx, self + 0x740, 0);  // 0x20c1b""")
rep("""        const u32 local = frame_cell(ctx, 0);  // the `push ecx` slot (0x20be0): it starts as this""",
    """        const u32 local = fr.at(4);  // the `push ecx` slot (0x20be0; `lea eax, [esp + 0x10]` at 0x20c40): it starts as this""")
i = t.index("// ---- serviceChannelPlayback (rva 0x34430)")
k = t.index("{", t.index("serviceChannelPlayback_body(Ctx &ctx", i)) + 1
t = t[:k] + "\n    SndFrame fr(ctx, 0x14);  // push ecx; push ebx; push ebp; push esi; push edi (0x34430-0x3443e)" + t[k:]
rep("""            const u32 bcell = frame_cell(ctx, 1);  // the local `b` the callees take by address""",
    """            const u32 bcell = fr.at(4);  // the local `b` the callees take by address: the `push ecx` slot ([esp + 0x10], 0x34497)""")
rep("""    const u32 cellN = frame_cell(ctx, 2);     // [esp+0x10]: the node out-parameter
    const u32 cell14 = frame_cell(ctx, 3);    // [esp+0x14]
    const u32 cell20 = frame_cell(ctx, 4);    // [esp+0x20] in the second walk""",
    """    SndFrame fr(ctx, 0);
    const u32 aligned = (fr.entry() - 4u) & ~7u;  // push ebp; mov ebp, esp; and esp, -8 (0x1f8c0-0x1f8c3)
    fr.set_esp(aligned - 0x28u);                  // sub esp, 0x1c; push ebx; push esi; push edi (0x1f8c6-0x1f8d5)
    const u32 cellN = aligned - 0x18u;            // [esp+0x10]: the node out-parameter (0x1f9c3)
    const u32 cell14 = aligned - 0x14u;           // [esp+0x14] (0x1f8dd, 0x1fa1a)
    const u32 cell20 = aligned - 0x08u;           // [esp+0x20] in the second walk (0x1fb63)""")
assert "frame_cell" not in t, "frame_cell left"
if "--check" in sys.argv:
    print("ok (not written)")
else:
    P.write_text(t, encoding="utf-8")
    print(f"wrote {P}")

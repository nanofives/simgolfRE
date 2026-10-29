// sound.dll's mixer and channel renderers (step S4.4, log/roadmap/S4/4_notes.md; header portsim/sim/audio_mixer.h): the
// integer mixers, the x87 resamplers, the MMX renderers, the mixer objects, the channel renderers. They compute on the audio
// core's virtual x87 unit (portsim/sim/audio.h "the x87 unit"; S4.4 built it here, S4.2b moved it to audio_core.cpp with
// sound.dll's __ftol and the harness entries) and keep their by-address locals on the core's stack (SndFrame).
//
// Negative-control modes (sg_negctl_set(<name>, mode)); every function reads its own switch:
//   1  a wrong store or a wrong return in the body (each function's comment says which)
//   2  (the six Voice_mix* leaves and the two stream renderers only) the c3fh long-double control: every intermediate a
//      53-bit double rounded to nearest, which is what MSVC's `long double` gives (log/c3/c3fh_notes.txt section 2)
#if defined(_MSC_VER)
#pragma warning(disable : 4702)  // unreachable code: the optimiser's view of a store after the faulting arms (wrap kind 2)
#endif
#include <algorithm>
#include <climits>
#include <cmath>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "portsim/gamefiles.h"
#include "portsim/ptr.h"
#include "portsim/registry.h"
#include "portsim/sim/audio.h"
#include "portsim/sim/audio_mixer.h"
#include "portsim/sim/soundstate.h"

// ================================================================================================ the functions
namespace portsim {
namespace {
using u32 = uint32_t;
using x87::X80;
using x87::Unit;

// <<< S4.4 bodies
// ================================================================================================ shared helpers
uint64_t ld64(Ctx &c, u32 va) { return ld<uint64_t>(c, va); }
int32_t sx16(uint16_t w) { return static_cast<int16_t>(w); }

// The arithmetic of one resampler body: the x87 unit's operation at the word the leaf loaded (0x0f7f), or, with negative
// control 2, the c3fh long-double control: every result a 53-bit double rounded to nearest (MSVC `long double` under SSE2).
struct Ar {
  uint16_t cw = 0x0f7f;
  bool dbl = false;
  static double d(X80 x) {
    const uint64_t b = x87::sw::fst64(x, 0x027f);
    double v;
    std::memcpy(&v, &b, 8);
    return v;
  }
  static X80 x(double v) {
    uint64_t b;
    std::memcpy(&b, &v, 8);
    return x87::from_f64(b);
  }
  X80 add(X80 a, X80 b) const { return dbl ? x(d(a) + d(b)) : x87::add(a, b, cw); }
  X80 sub(X80 a, X80 b) const { return dbl ? x(d(a) - d(b)) : x87::sub(a, b, cw); }
  X80 mul(X80 a, X80 b) const { return dbl ? x(d(a) * d(b)) : x87::mul(a, b, cw); }
  X80 fisub(X80 a, int32_t i) const { return sub(a, x87::fild(i)); }          // fisub dword
  X80 fadd_m64(X80 a, uint64_t m) const { return add(a, x87::from_f64(m)); }    // fadd qword
  int32_t fist(X80 a) const { return static_cast<int32_t>(x87::fist(a, 32, cw)); }
  uint64_t fst(X80 a) const { return x87::fst64(a, cw); }
};

// The ramp step before `fninit`, on the entering register stack and control word: one push, the division, fstp qword.
// form 0: fild dword n; fdivr qword vol (mode 1)   form 1: fld qword vol; fidiv dword n (other modes)
uint64_t ramp_step(Unit &u, uint64_t vol, int32_t n, int form) {
  if (form == 0) {
    u.push(x87::fild(n));
    u.st(0) = x87::div(x87::from_f64(vol), u.st(0), u.cw);
  } else {
    u.push(x87::fld64(vol));
    u.st(0) = x87::div(u.st(0), x87::fild(n), u.cw);
  }
  return x87::fst64(u.pop(), u.cw);
}

// The registers a resampler leaves, bottom first, after its `fninit` (TOP 0 there, so they land where the original's do).
void leave(Unit &u, std::initializer_list<X80> regs) {
  for (const X80 &r : regs) u.push(r);
}

// ---------------------------------------------------------------------------------------------------------------- A.
// The x87 resamplers. Voice_mixSetMono16 0x19b20 and its five twins, and streamRenderBlock 0x184a0 / streamRenderTail 0x188d0
// hold the whole loop on the register stack: after `fninit` they load the control word 0x0f7f (`mov dword [ebp-0x24], 0xf7f`
// Voice_mixSetMono16+0x188, `fninit` +0x19f, `fldcw [ebp-0x24]` +0x1a1: 64-bit significand, toward zero), push the two gains,
// the step and the position and keep them there; the only stores are the two output doubles per sample and the fractional
// position at the end. They never restore the word and return with 4 (count exit) or 5 (end exit) registers in use. Each
// arithmetic step below is ONE x87 operation in the original's operand order (log/c3/c3fh_notes.txt section 2; the c3fh hook
// shim/src/re/c3fh.cpp cites every instruction). All take __cdecl (out, posp, n, rec, mode): out two doubles per sample,
// posp a pointer to the sample cursor, n the samples to write, rec the voice record; they return the samples written.
//
// Mono (Voice_mixSetMono16 / AddMono16 / Set8bit / Add8bit): SB = bytes per sample; the Add twins add the output doubles
// (`fadd qword [edi]` / `[edi+8]` before the stores), the 8-bit pair loads (b << 8) - 0x8000 as a word and steps one byte.
template <int SB>
int32_t sample_word(Ctx &c, u32 p) {
  if constexpr (SB == 2) return sx16(ld<uint16_t>(c, p));
  return sx16(static_cast<uint16_t>((static_cast<u32>(ld<uint8_t>(c, p)) << 8) - 0x8000u));
}

template <int SB, bool ADD>
u32 mix_mono(Ctx &c, u32 out, u32 posp, u32 n, u32 obj, int32_t mode, int neg) {
  Unit &u = x87::unit(c);
  Ar ar;
  ar.dbl = neg == 2;
  uint64_t delta = 0;                          // [ebp-0x3c] = 0.0
  int wrapFlag = 0, ramp = 0;                  // [ebp-0x1c], [ebp-0x10]
  u32 wrapVal = 0, wrapPtr = 0;                // [ebp-0x14], [ebp-0x18]
  uint64_t gB, gA, posBits, stepBits;          // [ebp-0x4c], [ebp-0x54], [ebp-0x34], [ebp-0x44]
  u32 loopStart, end;                          // [ebp-0xc], [ebp-8]
  int loopOn;                                  // [ebp-4]
  const int32_t nn = static_cast<int32_t>(n);
  if (mode == 1) {
    gB = ld64(c, obj + 0x1f0);
    gA = ld64(c, obj + 0x200);
    const u32 s = ld<u32>(c, obj + 8);
    end = ld<u32>(c, s + 0x20);
    if (ld<u32>(c, s + 0x1c) > end) end = ld<u32>(c, s + 0x24);    // unsigned
    loopStart = ld<u32>(c, s + 0x18);
    posBits = ld64(c, obj + 0x1380);
    stepBits = ld64(c, obj + 0x1388);
    const u32 flags = ld<u32>(c, obj + 0x58);
    if (flags & 8) {
      ramp = 1;
      delta = ramp_step(u, ld64(c, obj + 0x1c8), nn, 0);           // +0x1c8 / n
    }
    loopOn = (~ld<uint8_t>(c, obj + 0x5c)) & 1;
    if (flags & 0x20) {
      wrapPtr = s + 0x20;
      if (ld<u32>(c, posp) > ld<u32>(c, s + 0x20)) wrapFlag = 1;   // unsigned
      else loopOn = 0;
      wrapVal = ld<u32>(c, obj + 0x1378);
    }
  } else {
    gB = ld64(c, obj + 0x80);
    gA = ld64(c, obj + 0x70);
    stepBits = ld64(c, obj + 0x108);
    posBits = ld64(c, obj + 0x100);
    const uint8_t cb = ld<uint8_t>(c, obj + 0x9c);
    if (cb & 8) {
      ramp = 1;
      delta = ramp_step(u, ld64(c, obj + 0xf0), nn, 1);            // +0xf0 / n
    }
    if ((ld<uint8_t>(c, obj + 0x98) & 1) && !(cb & 2)) {
      loopOn = 1;
      loopStart = ld<u32>(c, obj + 0xd8);
      end = ld<u32>(c, obj + 0xd4);
    } else {
      loopOn = 0;
      loopStart = ld<u32>(c, obj + 0x18);
      end = ld<u32>(c, obj + 0x20);
    }
  }
  u.fninit();
  u.fldcw(0x0f7f);
  X80 rA = x87::fld64(gA), rB = x87::fld64(gB), rStep = x87::fld64(stepBits), rPos = x87::fld64(posBits);
  X80 frac, s0, s1, t, v, oB, oA;
  u32 ecx = n;
  u32 p = ld<u32>(c, posp);
  u32 o = out;
  int32_t ip = 0;                              // [ebp-0x20]
  bool five;
top:
  rPos = ar.add(rPos, rStep);                  // fadd st(1): position += step
  ip = ar.fist(rPos);                          // fist dword (toward zero)
  p += static_cast<u32>(ip) * SB;
  frac = ar.fisub(rPos, ip);                   // the fraction
  if (static_cast<int32_t>(p) >= static_cast<int32_t>(end)) goto atEnd;   // signed `jge`
  s0 = x87::fild(sample_word<SB>(c, p));
afterS0:
  p += SB;
  if (static_cast<int32_t>(p) >= static_cast<int32_t>(end)) {
    int32_t pad = 0;                           // the word [ebp+0x16] (the high half of the record argument's slot)
    if (loopOn) pad = sample_word<SB>(c, loopStart);
    s1 = x87::fild(pad);
  } else {
    s1 = x87::fild(sample_word<SB>(c, p));
  }
  t = ar.sub(s1, s0);                          // fsub st(1)
  t = ar.mul(t, frac);                         // fmul st(2)
  v = ar.add(t, s0);                           // fadd st(1): linear interpolation
  oB = ar.mul(v, rB);                          // fmul st(5)
  oA = ar.mul(v, rA);                          // fmul st(6)
  if constexpr (ADD) {
    oB = ar.fadd_m64(oB, ld64(c, o));          // fadd qword [edi]
    oA = ar.fadd_m64(oA, ld64(c, o + 8));      // fadd qword [edi+8]
  }
  st<uint64_t>(c, o, ar.fst(oB));
  st<uint64_t>(c, o + 8, ar.fst(oA));
  if (--ecx == 0) {
    five = false;
    goto done;
  }
  o += 16;
  p = ld<u32>(c, posp);
  if (ramp) {
    const X80 rD = x87::fld64(delta);
    rB = ar.sub(rB, rD);                       // fsub st(3), st(0)
    rA = ar.sub(rA, rD);                       // fsubp st(4), st(0)
  }
  goto top;
atEnd:
  if (!loopOn) {
    five = true;
    goto done;
  }
  rPos = frac;                                 // the position register is replaced by the fraction
  ip = ar.fist(frac);
  st<u32>(c, posp, loopStart);
  p = loopStart;
  s0 = x87::fild(sample_word<SB>(c, p));
  if (wrapFlag) {
    loopOn = 0;
    st<u32>(c, wrapPtr, wrapVal);
  }
  goto afterS0;
done:
  X80 &topReg = five ? frac : rPos;
  if (ecx == 0) {
    p -= SB;
    topReg = ar.fisub(topReg, ip);
    posBits = ar.fst(topReg);                  // fst qword [ebp-0x34]
  }
  st<u32>(c, posp, p);
  if (mode == 1) {
    st<uint64_t>(c, obj + 0x1380, posBits);
    if ((ld<uint8_t>(c, obj + 0x58) & 0x20) && !loopOn) st<u32>(c, obj + 0x5c, ld<u32>(c, obj + 0x5c) | 1u);
  } else {
    st<uint64_t>(c, obj + 0x100, posBits);
  }
  u32 ret = n - ecx;
  if (neg == 1) ret += 1;                      // negative control 1: the count written off by one
  if (five) leave(u, {rA, rB, rStep, rPos, topReg});
  else leave(u, {rA, rB, rStep, topReg});
  return ret;
}

// Stereo 16-bit (Voice_mixSetStereo16 0x1a6c0 / AddStereo16 0x1a910): no resampling; each frame is two words (left at p,
// right at p+2) times the two gains, the cursor steps 4 bytes; the loop flag lives in the record argument's own slot.
template <bool ADD>
u32 mix_stereo(Ctx &c, u32 out, u32 posp, u32 n, u32 obj, int32_t mode, int neg) {
  Unit &u = x87::unit(c);
  Ar ar;
  ar.dbl = neg == 2;
  uint64_t delta = 0;
  int wrapFlag = 0, ramp = 0;
  uint64_t gB, gA;
  u32 loopStart, end;
  int loopOn;
  const int32_t nn = static_cast<int32_t>(n);
  if (mode == 1) {
    gB = ld64(c, obj + 0x1f0);
    gA = ld64(c, obj + 0x200);
    const u32 s = ld<u32>(c, obj + 8);
    const u32 e20 = ld<u32>(c, s + 0x20);
    end = (ld<u32>(c, s + 0x1c) > e20) ? ld<u32>(c, s + 0x24) : e20;
    loopStart = ld<u32>(c, s + 0x18);
    const u32 flags = ld<u32>(c, obj + 0x58);
    if (flags & 8) {
      ramp = 1;
      delta = ramp_step(u, ld64(c, obj + 0x1c8), nn, 0);
    }
    loopOn = (~ld<uint8_t>(c, obj + 0x5c)) & 1;
    if (flags & 0x20) {
      if (ld<u32>(c, posp) > e20) wrapFlag = 1;                    // against the +0x20 read above
      else loopOn = 0;
    }
  } else {
    gB = ld64(c, obj + 0x80);
    gA = ld64(c, obj + 0x70);
    const uint8_t cb = ld<uint8_t>(c, obj + 0x9c);
    if (cb & 8) {
      ramp = 1;
      delta = ramp_step(u, ld64(c, obj + 0xf0), nn, 1);
    }
    if ((ld<uint8_t>(c, obj + 0x98) & 1) && !(cb & 2)) {
      loopOn = 1;
      loopStart = ld<u32>(c, obj + 0xd8);
      end = ld<u32>(c, obj + 0xd4);
    } else {
      loopOn = 0;
      loopStart = ld<u32>(c, obj + 0x18);
      end = ld<u32>(c, obj + 0x20);
    }
  }
  u.fninit();
  u.fldcw(0x0f7f);
  X80 rA = x87::fld64(gA), rB = x87::fld64(gB);
  u32 ecx = n;
  u32 p = ld<u32>(c, posp);
  u32 o = out;
top:
  if (static_cast<int32_t>(p) >= static_cast<int32_t>(end)) goto atEnd;   // signed
  {
    const X80 sl = x87::fild(sx16(ld<uint16_t>(c, p))), sr = x87::fild(sx16(ld<uint16_t>(c, p + 2)));
    X80 oB = ar.mul(sl, rB);                   // fmul st(2)
    X80 oA = ar.mul(sr, rA);                   // fmul st(3)
    if constexpr (ADD) {
      oB = ar.fadd_m64(oB, ld64(c, o));
      oA = ar.fadd_m64(oA, ld64(c, o + 8));
    }
    st<uint64_t>(c, o, ar.fst(oB));
    st<uint64_t>(c, o + 8, ar.fst(oA));
  }
  if (--ecx == 0) goto done;
  o += 16;
  p += 4;
  if (ramp) {
    const X80 rD = x87::fld64(delta);
    rB = ar.sub(rB, rD);                       // fsub st(1), st(0)
    rA = ar.sub(rA, rD);                       // fsubp st(2), st(0)
  }
  goto top;
atEnd:
  if (!loopOn) goto done;
  st<u32>(c, posp, loopStart);
  p = loopStart;
  if (wrapFlag) loopOn = 0;
  goto top;
done:
  st<u32>(c, posp, p);
  if (mode == 1 && (ld<uint8_t>(c, obj + 0x58) & 0x20) && !loopOn) st<u32>(c, obj + 0x5c, ld<u32>(c, obj + 0x5c) | 1u);
  u32 ret = n - ecx;
  if (neg == 1) ret += 1;
  leave(u, {rA, rB});
  return ret;
}

// The stream resamplers (streamRenderBlock 0x184a0 stores, streamRenderTail 0x188d0 accumulates). Same arguments as the mono
// mixers, 16-bit samples, the step is the constant 0x3fd7384ef2a605ce (streamRenderBlock+0x8f / +0x96). Gains: Block mode 1 2.0 /
// 2.0, other modes +0x80 / +0x70; Tail mode 1 +0x1f0 / +0x200, other modes 2.0 / 2.0. Other modes add a buffer chain: bit 0 of
// the byte +0x9c makes the end of the current buffer hop to the next one at +0xb4 (counted into +0xcc at exit). Block's chain
// end reads the flags of `cmp edx, 0` (streamRenderBlock+0x259), so it always falls into `fist` +0x288, storing the SAMPLE just
// loaded as the integer part (log/c3/c3fh_notes.txt section 4); Tail jumps back instead (streamRenderTail+0x29b).
template <bool TAIL>
u32 stream_render(Ctx &c, u32 out, u32 posp, u32 n, u32 rec, int32_t mode, int neg) {
  constexpr uint64_t kStep = 0x3fd7384ef2a605ceull, kTwo = 0x4000000000000000ull;
  Unit &u = x87::unit(c);
  Ar ar;
  ar.dbl = neg == 2;
  uint64_t delta = 0;
  int chain = 0, wrapFlag = 0, ramp = 0, hops = 0;
  u32 wrapVal = 0, wrapPtr = 0, next = 0;
  uint64_t gB, gA, posBits;
  u32 loopStart = 0, end = 0;
  int loopOn;
  const int32_t nn = static_cast<int32_t>(n);
  if (mode == 1) {
    if constexpr (TAIL) {
      gB = ld64(c, rec + 0x1f0);
      gA = ld64(c, rec + 0x200);
    } else {
      gB = kTwo;
      gA = kTwo;
    }
    const u32 s = ld<u32>(c, rec + 8);
    end = ld<u32>(c, s + 0x20);
    if (ld<u32>(c, s + 0x1c) > end) end = ld<u32>(c, s + 0x24);
    loopStart = ld<u32>(c, s + 0x18);
    posBits = ld64(c, rec + 0x1380);
    const u32 flags = ld<u32>(c, rec + 0x58);
    if (flags & 8) {
      ramp = 1;
      delta = ramp_step(u, ld64(c, rec + 0x1c8), nn, 0);
    }
    loopOn = (~ld<uint8_t>(c, rec + 0x5c)) & 1;
    if (flags & 0x20) {                       // no cursor test here (the mixers have one)
      wrapVal = ld<u32>(c, rec + 0x1378);
      wrapPtr = s + 0x20;
      wrapFlag = 1;
    }
  } else {
    if constexpr (TAIL) {
      gB = kTwo;
      gA = kTwo;
    } else {
      gB = ld64(c, rec + 0x80);
      gA = ld64(c, rec + 0x70);
    }
    posBits = ld64(c, rec + 0x100);
    const uint8_t c9c = ld<uint8_t>(c, rec + 0x9c);
    loopOn = ((ld<uint8_t>(c, rec + 0x98) & 1) && !(c9c & 2)) ? 1 : 0;
    chain = c9c & 1;
    if (chain) {
      next = ld<u32>(c, rec + 0xb4);
      loopStart = ld<u32>(c, next + 0x18);
      end = ld<u32>(c, rec + 0x20);
    } else if (loopOn) {
      loopStart = ld<u32>(c, rec + 0xd8);
      end = ld<u32>(c, rec + 0xd4);
    } else {
      loopStart = ld<u32>(c, rec + 0x18);
      end = ld<u32>(c, rec + 0x20);
    }
  }
  u.fninit();
  u.fldcw(0x0f7f);
  X80 rA = x87::fld64(gA), rB = x87::fld64(gB), rStep = x87::fld64(kStep), rPos = x87::fld64(posBits);
  X80 frac, s0, s1, t, v, oB, oA;
  u32 ecx = n;
  u32 p = ld<u32>(c, posp);
  u32 o = out;
  int32_t ip = 0;                              // [ebp-0x2c]
  bool five;
top:
  rPos = ar.add(rPos, rStep);
  ip = ar.fist(rPos);
  p += static_cast<u32>(ip) * 2;
  frac = ar.fisub(rPos, ip);
  if (static_cast<int32_t>(p) >= static_cast<int32_t>(end)) goto atEnd;
  s0 = x87::fild(sx16(ld<uint16_t>(c, p)));
afterS0:
  p += 2;
  if (static_cast<int32_t>(p) >= static_cast<int32_t>(end)) goto pad;
  s1 = x87::fild(sx16(ld<uint16_t>(c, p)));
join:
  t = ar.sub(s1, s0);
  t = ar.mul(t, frac);
  v = ar.add(t, s0);
  oB = ar.mul(v, rB);
  oA = ar.mul(v, rA);
  if constexpr (TAIL) {
    oB = ar.fadd_m64(oB, ld64(c, o));
    oA = ar.fadd_m64(oA, ld64(c, o + 8));
  }
  st<uint64_t>(c, o, ar.fst(oB));
  st<uint64_t>(c, o + 8, ar.fst(oA));
  if (--ecx == 0) {
    five = false;
    goto done;
  }
  o += 16;
  p = ld<u32>(c, posp);
  if (ramp) {
    const X80 rD = x87::fld64(delta);
    rB = ar.sub(rB, rD);
    rA = ar.sub(rA, rD);
  }
  goto top;
atEnd:
  if (chain) {
    ++hops;
    rPos = frac;
    ip = ar.fist(frac);
    p = loopStart;
    st<u32>(c, posp, p);
    s0 = x87::fild(sx16(ld<uint16_t>(c, p)));
    const u32 b = next;
    end = ld<u32>(c, b + 0xd4);
    next = ld<u32>(c, b + 0xb4);
    if (next) {
      loopStart = ld<u32>(c, next + 0xd8);
      goto afterS0;
    }
    chain = 0;
    loopOn = 0;
    if constexpr (TAIL) goto afterS0;
    ip = ar.fist(s0);                          // Block: the sample just loaded as the integer part
    goto pad;
  }
  if (!loopOn) {
    five = true;
    goto done;
  }
  rPos = frac;
  ip = ar.fist(frac);
  st<u32>(c, posp, loopStart);
  p = loopStart;
  s0 = x87::fild(sx16(ld<uint16_t>(c, p)));
  if (wrapFlag) {
    loopOn = 0;
    st<u32>(c, wrapPtr, wrapVal);
  }
  goto afterS0;
pad : {
  int32_t w = 0;                               // the word [ebp+0x16]
  if (chain || loopOn) w = sx16(ld<uint16_t>(c, loopStart));
  s1 = x87::fild(w);
}
  goto join;
done:
  X80 &topReg = five ? frac : rPos;
  if (ecx == 0) {
    p -= 2;
    topReg = ar.fisub(topReg, ip);
    posBits = ar.fst(topReg);
  }
  st<u32>(c, posp, p);
  if (mode == 1) {
    st<uint64_t>(c, rec + 0x1380, posBits);
    if ((ld<uint8_t>(c, rec + 0x58) & 0x20) && !loopOn) st<u32>(c, rec + 0x5c, ld<u32>(c, rec + 0x5c) | 1u);
  } else {
    st<uint64_t>(c, rec + 0x100, posBits);
    st<int32_t>(c, rec + 0xcc, ld<int32_t>(c, rec + 0xcc) + hops);
  }
  u32 ret = n - ecx;
  if (neg == 1) ret += 1;
  if (five) leave(u, {rA, rB, rStep, rPos, topReg});
  else leave(u, {rA, rB, rStep, topReg});
  return ret;
}

// st(0) as the caller of a double-returning call sees it (the emulator stores it with fstp qword at the word in force).
void ret_st0(Ctx &c, Ret &r) {
  Unit &u = x87::unit(c);
  const uint64_t b = x87::fst64(u.st(0), u.cw);
  double d;
  std::memcpy(&d, &b, 8);
  r.set_double(d);
}

#define SG_RES(fn, body)                                                       \
  int g_negctl_##fn = 0;                                                       \
  void a_##fn(Ctx &c, const uint32_t *a, Ret &r) {                             \
    SG_X87_GUARD(c, {                                                          \
      r.set_u32(body(c, a[0], a[1], a[2], a[3], static_cast<int32_t>(a[4]),   \
                     SG_NEGCTL(g_negctl_##fn)));                               \
      ret_st0(c, r);                                                           \
    })                                                                         \
  }
SG_RES(Voice_mixSetMono16, (mix_mono<2, false>))
SG_RES(Voice_mixAddMono16, (mix_mono<2, true>))
SG_RES(Voice_mixSet8bit, (mix_mono<1, false>))
SG_RES(Voice_mixAdd8bit, (mix_mono<1, true>))
SG_RES(Voice_mixSetStereo16, (mix_stereo<false>))
SG_RES(Voice_mixAddStereo16, (mix_stereo<true>))
SG_RES(streamRenderBlock, (stream_render<false>))
SG_RES(streamRenderTail, (stream_render<true>))
#undef SG_RES

// ---------------------------------------------------------------------------------------------------------------- B.
// The integer mixers: 16-bit PCM into the 32-bit accumulation buffer (the c3do hook shim/src/re/c3do.cpp parts M, M2, M3 cite
// every instruction). Every one is __cdecl (ppOut, ppIn, count, voice, mode) and returns the frames consumed; the cells *ppOut /
// *ppIn hold the output (dword accumulators) and input positions and are advanced in place. mode 1 reads the sample record from
// voice+8 (start +0x18, length +0x1c, end +0x20, loop end +0x24) and counts in voice+0x16c / voice+0x1360; other modes read the
// record from the voice itself (start +0x18, end +0x20, loop start +0xd8, loop end +0xd4, chain voice +0xb4, flags +0x98 /
// +0x9c) and count in voice+0x88 / voice+0xcc.
struct MixS {
  int32_t loopFlag = 0, chained = 0, wraps = 0, eof = 0;
  u32 chain = 0, limit = 0, loopStart = 0;
};

// Saturating `add [edi], eax` / `jo`: a signed overflow stores 0x7fffffff (no carry) or 0x80000000 (carry).
void sat_add(Ctx &c, u32 p, int32_t v) {
  int64_t s = static_cast<int64_t>(ld<int32_t>(c, p)) + v;
  if (s > INT32_MAX) s = INT32_MAX;
  else if (s < INT32_MIN) s = INT32_MIN;
  st<int32_t>(c, p, static_cast<int32_t>(s));
}
// The same add with eax kept: the saturation constant is loaded INTO eax, so a following add that reuses eax sees it
// (mixMonoDupAdd+0xd9 / +0xe2).
void sat_add_e(Ctx &c, u32 p, int32_t &eax) {
  const int64_t s = static_cast<int64_t>(ld<int32_t>(c, p)) + eax;
  if (s > INT32_MAX) {
    eax = INT32_MAX;
    st<int32_t>(c, p, eax);
  } else if (s < INT32_MIN) {
    eax = INT32_MIN;
    st<int32_t>(c, p, eax);
  } else {
    st<int32_t>(c, p, static_cast<int32_t>(s));
  }
}
int32_t imul_lo(int32_t a, int32_t b) {     // `imul edx`: the low dword of the signed product
  return static_cast<int32_t>(static_cast<uint32_t>(static_cast<int64_t>(a) * static_cast<int64_t>(b)));
}
bool below(u32 p, u32 limit) { return static_cast<int32_t>(p) < static_cast<int32_t>(limit); }   // signed `jge` not taken

// mixStereoAdd's prologue (mixStereoAdd+0x6..+0x97)
void prologue_a(Ctx &c, u32 v, int32_t mode, MixS &s) {
  s = MixS{};
  if (mode == 1) {
    const u32 bo = ld<u32>(c, v + 8);
    u32 ecx = ld<u32>(c, bo + 0x20);
    if (ld<u32>(c, bo + 0x1c) > ecx) ecx = ld<u32>(c, bo + 0x24);   // unsigned
    s.loopStart = ld<u32>(c, bo + 0x18);
    s.limit = ecx;
    return;
  }
  const int32_t c1 = (ld<uint8_t>(c, v + 0x98) & 1) && !(ld<uint8_t>(c, v + 0x9c) & 2);
  s.loopFlag = c1;
  s.chained = ld<uint8_t>(c, v + 0x9c) & 1;
  u32 lo, hi;
  if (s.chained) {
    s.chain = ld<u32>(c, v + 0xb4);
    lo = ld<u32>(c, s.chain + 0x18);
    hi = ld<u32>(c, v + 0x20);
  } else if (c1) {
    lo = ld<u32>(c, v + 0xd8);
    hi = ld<u32>(c, v + 0xd4);
  } else {
    lo = ld<u32>(c, v + 0x18);
    hi = ld<u32>(c, v + 0x20);
  }
  s.loopStart = lo;
  s.limit = hi;
}
// mixMonoDupSet's prologue: mode 1 also sets loopFlag = (limit == record+0x24) and eof when byte voice+0x58 has bit 5
void prologue_c(Ctx &c, u32 v, int32_t mode, MixS &s) {
  s = MixS{};
  if (mode != 1) {
    prologue_a(c, v, mode, s);
    return;
  }
  const u32 bo = ld<u32>(c, v + 8);
  u32 ecx = ld<u32>(c, bo + 0x20);
  if (ld<u32>(c, bo + 0x1c) > ecx) ecx = ld<u32>(c, bo + 0x24);
  s.limit = ecx;
  s.loopStart = ld<u32>(c, bo + 0x18);
  s.loopFlag = ecx == ld<u32>(c, bo + 0x24) ? 1 : 0;
  if ((ld<uint8_t>(c, v + 0x58) & 0x20) && s.loopFlag) s.eof = 1;
}
u32 epilogue(Ctx &c, u32 v, int32_t mode, u32 count, u32 remaining, u32 advance, int32_t wraps) {
  const u32 a = mode == 1 ? 0x16c : 0x88, w = mode == 1 ? 0x1360 : 0xcc;
  st<u32>(c, v + a, ld<u32>(c, v + a) + advance);
  st<u32>(c, v + w, ld<u32>(c, v + w) + static_cast<u32>(wraps));
  return count - remaining;
}

// The wrap handler past the limit, shared shape (mixStereoAdd+0x102..+0x17d). kind: 0 mixStereoAdd (a non-chained looping
// voice falls into the chained block when the count is spent), 1 the C-group copies (eof clears loopFlag after a looping
// restart), 2 mixStereoGainAdd (falls through too; its non-chained looping arm stores to [chained + 4], which faults),
// 3 the gain / fade pair B (`break`), 4 the gain / fade pair C (no wrap count for a non-chained looping voice).
// Returns true when the loop continues.
bool wrap(Ctx &c, MixS &s, u32 &esi, u32 ppIn, u32 ecx, int kind) {
  if (s.chained == 0) {
    if (s.loopFlag == 0) return false;
    if (kind != 4) s.wraps++;
    if (kind == 2) st<u32>(c, static_cast<u32>(s.chained) + 4, 0);   // mixStereoGainAdd+0x13f: [0 + 4], faults
    esi = s.loopStart;
    st<u32>(c, ppIn, esi);
    if (kind == 1 && s.eof) s.loopFlag = 0;
    if (ecx != 0) return true;
    if (kind != 0 && kind != 2) return false;
    // no `jmp` after the `jne`: the count spent falls into the chained block, which reads the NULL chain and faults
  }
  s.wraps++;
  esi = s.loopStart;
  st<u32>(c, ppIn, esi);
  const u32 cur = s.chain;
  s.limit = ld<u32>(c, cur + 0xd4);
  s.chain = ld<u32>(c, cur + 0xb4);
  if (s.chain != 0) {
    s.loopStart = ld<u32>(c, s.chain + 0xd8);
    return ecx != 0;
  }
  s.chained = 0;
  s.loopFlag = 0;
  return ecx != 0;
}

// mixStereoAdd 0x13890: stereo 16-bit frames added into the accumulators, (short)in[k] << 16 with saturation; a do-while over
// the count (a count of 0 wraps). Negative control 1: the right channel is added unshifted.
int g_negctl_mixStereoAdd = 0;
u32 mixStereoAdd(Ctx &c, u32 ppOut, u32 ppIn, u32 count, u32 v, int32_t mode) {
  MixS s;
  prologue_a(c, v, mode, s);
  u32 ecx = count, esi = ld<u32>(c, ppIn), edi = ld<u32>(c, ppOut);
  const int neg = SG_NEGCTL(g_negctl_mixStereoAdd);
  for (;;) {
    sat_add(c, edi, sx16(ld<uint16_t>(c, esi)) * 65536);
    edi += 4;
    sat_add(c, edi, neg == 1 ? sx16(ld<uint16_t>(c, esi + 2)) : sx16(ld<uint16_t>(c, esi + 2)) * 65536);
    edi += 4;
    esi += 4;
    if (below(esi, s.limit)) {
      if (--ecx != 0) continue;
      break;
    }
    --ecx;
    if (!wrap(c, s, esi, ppIn, ecx, 0)) break;
  }
  st<u32>(c, ppIn, esi);
  st<u32>(c, ppOut, edi);
  return epilogue(c, v, mode, count, ecx, count - ecx, s.wraps);
}

// mixMonoDupSet 0x16340: mono 16-bit samples stored twice (a left / right pair), one frame per sample. Negative control 1: the
// second slot stores the sample unshifted.
int g_negctl_mixMonoDupSet = 0;
u32 mixMonoDupSet(Ctx &c, u32 ppOut, u32 ppIn, u32 count, u32 v, int32_t mode) {
  MixS s;
  prologue_c(c, v, mode, s);
  u32 ecx = count, esi = ld<u32>(c, ppIn), edi = ld<u32>(c, ppOut);
  const int neg = SG_NEGCTL(g_negctl_mixMonoDupSet);
  for (;;) {
    const int32_t eax = sx16(ld<uint16_t>(c, esi)) * 65536;
    st<int32_t>(c, edi, eax);
    st<int32_t>(c, edi + 4, neg == 1 ? eax >> 16 : eax);
    edi += 8;
    esi += 2;
    if (below(esi, s.limit)) {
      if (--ecx != 0) continue;
      break;
    }
    --ecx;
    if (!wrap(c, s, esi, ppIn, ecx, 1)) break;
  }
  st<u32>(c, ppIn, esi);
  st<u32>(c, ppOut, edi);
  return epilogue(c, v, mode, count, ecx, count - ecx, s.wraps);
}

// mixMonoDupAdd 0x14250: mixMonoDupSet with the pair added with saturation (the second add reuses eax, which holds the
// saturation constant after an overflow). Negative control 1: the second add uses the sample, not eax.
int g_negctl_mixMonoDupAdd = 0;
u32 mixMonoDupAdd(Ctx &c, u32 ppOut, u32 ppIn, u32 count, u32 v, int32_t mode) {
  MixS s;
  prologue_c(c, v, mode, s);
  u32 ecx = count, esi = ld<u32>(c, ppIn), edi = ld<u32>(c, ppOut);
  const int neg = SG_NEGCTL(g_negctl_mixMonoDupAdd);
  for (;;) {
    int32_t eax = sx16(ld<uint16_t>(c, esi)) * 65536;
    const int32_t sample = eax;
    sat_add_e(c, edi, eax);
    edi += 4;
    if (neg == 1) eax = sample;
    sat_add_e(c, edi, eax);
    edi += 4;
    esi += 2;
    if (below(esi, s.limit)) {
      if (--ecx != 0) continue;
      break;
    }
    --ecx;
    if (!wrap(c, s, esi, ppIn, ecx, 1)) break;
  }
  st<u32>(c, ppIn, esi);
  st<u32>(c, ppOut, edi);
  return epilogue(c, v, mode, count, ecx, count - ecx, s.wraps);
}

// mixStereoGainAdd 0x13b10: stereo 16-bit frames, a gain per channel (mode 1: +0x1e8 / +0x1f8, else +0x78 / +0x68), the low
// dword of sample * gain added with saturation. Negative control 1: the right channel takes the left gain.
int g_negctl_mixStereoGainAdd = 0;
u32 mixStereoGainAdd(Ctx &c, u32 ppOut, u32 ppIn, u32 count, u32 v, int32_t mode) {
  MixS s;
  const int32_t gainL = ld<int32_t>(c, v + (mode == 1 ? 0x1e8 : 0x78));
  int32_t gainR = ld<int32_t>(c, v + (mode == 1 ? 0x1f8 : 0x68));
  if (SG_NEGCTL(g_negctl_mixStereoGainAdd) == 1) gainR = gainL;
  prologue_a(c, v, mode, s);
  u32 ecx = count, esi = ld<u32>(c, ppIn), edi = ld<u32>(c, ppOut);
  for (;;) {
    int32_t eax = imul_lo(sx16(ld<uint16_t>(c, esi)), gainL);
    sat_add_e(c, edi, eax);
    edi += 4;
    eax = imul_lo(sx16(ld<uint16_t>(c, esi + 2)), gainR);
    sat_add_e(c, edi, eax);
    edi += 4;
    esi += 4;
    if (below(esi, s.limit)) {
      if (--ecx != 0) continue;
      break;
    }
    --ecx;
    if (!wrap(c, s, esi, ppIn, ecx, 2)) break;
  }
  st<u32>(c, ppIn, esi);
  st<u32>(c, ppOut, edi);
  return epilogue(c, v, mode, count, ecx, count - ecx, s.wraps);
}

// The gain / fade prologue of mixMonoGainFadeAdd 0x14510 and mixMonoGainFadeSet 0x165a0: gains as mixStereoGainAdd; outside
// mode 1, with bit 3 of the byte +0x9c, the fade step is the dword +0xec / count (unsigned `div`: a count of 0 faults), raised
// from 0 to 1 by the Add body only.
struct MixG {
  int32_t gainL = 0, gainR = 0, step = 0;
};
void prologue_g(Ctx &c, u32 v, int32_t mode, u32 count, MixS &s, MixG &g, bool floor_one) {
  g = MixG{};
  if (mode == 1) {
    g.gainL = ld<int32_t>(c, v + 0x1e8);
    g.gainR = ld<int32_t>(c, v + 0x1f8);
  } else {
    g.gainL = ld<int32_t>(c, v + 0x78);
    g.gainR = ld<int32_t>(c, v + 0x68);
    if (ld<uint8_t>(c, v + 0x9c) & 8) {
      if (count == 0) throw DivideError{};
      const u32 q = ld<u32>(c, v + 0xec) / count;
      g.step = static_cast<int32_t>(q);
      if (floor_one && q == 0) g.step = 1;
    }
  }
  prologue_a(c, v, mode, s);
}
int32_t fade(int32_t gain, int32_t step) {   // gain - step when that stays >= 0 (signed `sub` / `jge`), else 0
  const int64_t d = static_cast<int64_t>(gain) - step;
  return d >= 0 ? static_cast<int32_t>(d) : 0;
}

// mixMonoGainFadeAdd 0x14510: mono 16-bit samples added with saturation to a left and a right dword, each with its own gain,
// the gains fading by the step per frame. Negative control 1: the gains never fade.
int g_negctl_mixMonoGainFadeAdd = 0;
u32 mixMonoGainFadeAdd(Ctx &c, u32 ppOut, u32 ppIn, u32 count, u32 v, int32_t mode) {
  MixS s;
  MixG g;
  prologue_g(c, v, mode, count, s, g, true);
  u32 ecx = count, esi = ld<u32>(c, ppIn), edi = ld<u32>(c, ppOut);
  const int neg = SG_NEGCTL(g_negctl_mixMonoGainFadeAdd);
  for (;;) {
    const int32_t smp = sx16(ld<uint16_t>(c, esi));
    int32_t eax = imul_lo(smp, g.gainL);
    sat_add_e(c, edi, eax);
    edi += 4;
    eax = imul_lo(smp, g.gainR);
    sat_add_e(c, edi, eax);
    edi += 4;
    esi += 2;
    if (below(esi, s.limit)) {
      if (neg != 1) {
        g.gainR = fade(g.gainR, g.step);
        g.gainL = fade(g.gainL, g.step);
      }
      if (--ecx != 0) continue;
      break;
    }
    --ecx;
    if (!wrap(c, s, esi, ppIn, ecx, 3)) break;
  }
  st<u32>(c, ppIn, esi);
  st<u32>(c, ppOut, edi);
  return epilogue(c, v, mode, count, ecx, count - ecx, s.wraps);
}

// mixMonoGainFadeSet 0x165a0: mixMonoGainFadeAdd with plain stores and no wrap count for a non-chained looping voice.
// Negative control 1: the left slot stores the sample times the right gain.
int g_negctl_mixMonoGainFadeSet = 0;
u32 mixMonoGainFadeSet(Ctx &c, u32 ppOut, u32 ppIn, u32 count, u32 v, int32_t mode) {
  MixS s;
  MixG g;
  prologue_g(c, v, mode, count, s, g, false);
  u32 ecx = count, esi = ld<u32>(c, ppIn), edi = ld<u32>(c, ppOut);
  const int neg = SG_NEGCTL(g_negctl_mixMonoGainFadeSet);
  for (;;) {
    const int32_t smp = sx16(ld<uint16_t>(c, esi));
    st<int32_t>(c, edi, imul_lo(smp, neg == 1 ? g.gainR : g.gainL));
    st<int32_t>(c, edi + 4, imul_lo(smp, g.gainR));
    edi += 8;
    esi += 2;
    if (below(esi, s.limit)) {
      g.gainR = fade(g.gainR, g.step);
      g.gainL = fade(g.gainL, g.step);
      if (--ecx != 0) continue;
      break;
    }
    --ecx;
    if (!wrap(c, s, esi, ppIn, ecx, 4)) break;
  }
  st<u32>(c, ppIn, esi);
  st<u32>(c, ppOut, edi);
  return epilogue(c, v, mode, count, ecx, count - ecx, s.wraps);
}

// The rated mixers: eight 8/16-bit x mono/stereo x add/store members stepping through the samples by a 16.16 rate with linear
// interpolation (c3do part M3). The position `ring` is 48-bit fixed point: the dword at ring+2 = fraction word + the low integer
// word, the dword at ring+4 = the integer part. Per channel: next = the sample one frame ahead when that frame starts below the
// limit, else 0, or the loop start's first sample when loopFlag is set (the eof members also clear loopFlag there, after the
// LEFT channel); value = cur + ((next - cur) * fraction >> 16), times the channel gain; added with saturation or stored; then
// ring += rate (carry into the word at ring+6) and the input is *ppIn + (integer part << log2(frame bytes)). The wrap handler
// also zeroes the integer part and restarts at the loop start. At the end the integer part is the voice counters' advance.
struct MixR {
  int32_t gainL = 0, gainR = 0;
  u32 rate = 0, ring = 0;
};
void prologue_r(Ctx &c, u32 v, int32_t mode, bool eof_logic, MixS &s, MixR &r) {
  if (mode == 1) {
    r.gainL = ld<int32_t>(c, v + 0x1e8);
    r.gainR = ld<int32_t>(c, v + 0x1f8);
    r.rate = ld<u32>(c, v + 0x220);
    r.ring = v + 0x1358;
    prologue_a(c, v, mode, s);
    if (eof_logic) {
      const u32 bo = ld<u32>(c, v + 8);
      s.loopFlag = s.limit == ld<u32>(c, bo + 0x24) ? 1 : 0;
      if ((ld<uint8_t>(c, v + 0x58) & 0x20) && s.loopFlag) s.eof = 1;
    }
  } else {
    r.gainL = ld<int32_t>(c, v + 0x78);
    r.gainR = ld<int32_t>(c, v + 0x68);
    r.rate = ld<u32>(c, v + 0x4c);
    r.ring = v + 0xb8;
    prologue_a(c, v, mode, s);
  }
}
struct RatedCfg {
  bool bits8, stereo, add, eof, wraps_loop;
  int fade;          // 0 none, 1 / 2 the step given
  bool posword;      // the position step uses only the word at ring+4
  bool fallthru;     // a non-chained looping voice falls into the chained block when the count is spent
};
u32 rated_kernel(Ctx &c, const RatedCfg &k, u32 ppOut, u32 ppIn, u32 count, u32 v, int32_t mode, MixS &s, int32_t gainL,
                 int32_t gainR, u32 rate, int32_t step, u32 ring, int neg) {
  const int samp = k.bits8 ? 1 : 2, stride = samp * (k.stereo ? 2 : 1);
  const int shift = stride == 4 ? 2 : (stride == 2 ? 1 : 0);
  auto dec = [&](u32 p) -> int32_t {
    return k.bits8 ? (static_cast<int32_t>(ld<uint8_t>(c, p)) << 8) - 0x8000 : sx16(ld<uint16_t>(c, p));
  };
  auto put = [&](u32 &edi, int32_t value) {
    if (k.add) sat_add(c, edi, value);
    else st<int32_t>(c, edi, value);
    edi += 4;
  };
  u32 ecx = count, esi = ld<u32>(c, ppIn), edi = ld<u32>(c, ppOut);
  for (;;) {
    const u32 saved = ecx;
    const int32_t frac = ld<uint16_t>(c, ring + 2);
    for (int ch = 0; ch < (k.stereo ? 2 : 1); ch++) {
      const int32_t cur = dec(esi + ch * samp);
      const u32 nx = esi + stride;
      int32_t next;
      if (below(nx, s.limit)) {
        next = dec(nx + ch * samp);
      } else {
        next = 0;
        if (s.loopFlag) {
          next = dec(s.loopStart + ch * samp);
          if (k.eof && s.eof) s.loopFlag = 0;
        }
      }
      int32_t interp = (imul_lo(next - cur, frac) >> 16) + cur;
      if (neg == 1) interp = cur;              // negative control 1: no interpolation
      if (k.stereo) {
        put(edi, imul_lo(interp, ch == 0 ? gainL : gainR));
      } else {
        put(edi, imul_lo(interp, gainL));
        put(edi, imul_lo(interp, gainR));
      }
    }
    const u32 lo = ld<u32>(c, ring + 2), nlo = lo + rate;
    st<u32>(c, ring + 2, nlo);
    if (nlo < rate) st<uint16_t>(c, ring + 6, static_cast<uint16_t>(ld<uint16_t>(c, ring + 6) + 1));
    const u32 ip = k.posword ? (nlo >> 16) : ld<u32>(c, ring + 4);
    esi = ld<u32>(c, ppIn) + (ip << shift);
    if (below(esi, s.limit)) {
      if (k.fade) {
        gainR = fade(gainR, step);
        gainL = fade(gainL, step);
      }
      ecx = saved - 1;
      if (ecx != 0) continue;
      break;
    }
    ecx = saved - 1;
    if (s.chained == 0) {
      if (s.loopFlag == 0) break;
      if (k.wraps_loop) s.wraps++;
      st<u32>(c, ring + 4, 0);
      esi = s.loopStart;
      st<u32>(c, ppIn, esi);
      if (ecx != 0) continue;
      if (!k.fallthru) break;
    }
    s.wraps++;
    st<u32>(c, ring + 4, 0);
    esi = s.loopStart;
    st<u32>(c, ppIn, esi);
    const u32 cur = s.chain;
    s.limit = ld<u32>(c, cur + 0xd4);
    s.chain = ld<u32>(c, cur + 0xb4);
    if (s.chain != 0) {
      s.loopStart = ld<u32>(c, s.chain + 0xd8);
      if (ecx != 0) continue;
      break;
    }
    s.chained = 0;
    s.loopFlag = 0;
    if (ecx != 0) continue;
    break;
  }
  const u32 advance = ld<u32>(c, ring + 4);
  st<u32>(c, ring + 4, 0);
  st<u32>(c, ppIn, esi);
  st<u32>(c, ppOut, edi);
  return epilogue(c, v, mode, count, ecx, advance, s.wraps);
}

Ret call5(Ctx &c, u32 rva, u32 ppOut, u32 ppIn, u32 count, u32 v, u32 mode) {
  return snd_call(c, rva, SndAbi::Cdecl, {ppOut, ppIn, count, v, mode});
}

// mixRatedStereo16Add 0x13dd0: 16-bit stereo, add. Outside mode 1 a unity rate (+0x4c == 0x10000) hands the work to
// mixStereoAdd (thunk 0x1d02) when both gains are unity too, else to mixStereoGainAdd (thunk 0x262b); their eax is the result.
int g_negctl_mixRatedStereo16Add = 0;
u32 mixRatedStereo16Add(Ctx &c, u32 ppOut, u32 ppIn, u32 count, u32 v, int32_t mode) {
  MixS s;
  MixR r;
  if (mode != 1) {
    const u32 rate = ld<u32>(c, v + 0x4c), gl = ld<u32>(c, v + 0x78), gr = ld<u32>(c, v + 0x68);
    if (rate == 0x10000) {
      if (gl == rate && gr == rate) return call5(c, 0x1d02, ppOut, ppIn, count, v, mode).eax;
      return call5(c, 0x262b, ppOut, ppIn, count, v, mode).eax;
    }
  }
  prologue_r(c, v, mode, true, s, r);
  return rated_kernel(c, {false, true, true, true, true, 0, false, true}, ppOut, ppIn, count, v, mode, s, r.gainL, r.gainR,
                      r.rate, 0, r.ring, SG_NEGCTL(g_negctl_mixRatedStereo16Add));
}

// mixRatedMono16AddFade 0x14820: 16-bit mono, add, fade. Mode 1: unity gains with a unity rate call mixMonoDupAdd (thunk
// 0x14ce), a unity rate calls mixMonoGainFadeAdd (thunk 0x15e6); other modes first compute the fade step (bit 3 of +0x9c:
// +0xec / count, 1 when that is 0), unity gains and rate with no step call mixMonoDupAdd, a unity rate mixMonoGainFadeAdd.
int g_negctl_mixRatedMono16AddFade = 0;
u32 mixRatedMono16AddFade(Ctx &c, u32 ppOut, u32 ppIn, u32 count, u32 v, int32_t mode) {
  MixS s;
  MixR r;
  int32_t step = 0;
  if (mode == 1) {
    const u32 gl = ld<u32>(c, v + 0x1e8), gr = ld<u32>(c, v + 0x1f8), rate = ld<u32>(c, v + 0x220);
    if (gl == 0x10000 && gr == gl && rate == gl) return call5(c, 0x14ce, ppOut, ppIn, count, v, mode).eax;
    if (rate == 0x10000) return call5(c, 0x15e6, ppOut, ppIn, count, v, mode).eax;
  } else {
    const u32 gl = ld<u32>(c, v + 0x78), gr = ld<u32>(c, v + 0x68), rate = ld<u32>(c, v + 0x4c);
    if (ld<uint8_t>(c, v + 0x9c) & 8) {
      if (count == 0) throw DivideError{};
      const u32 q = ld<u32>(c, v + 0xec) / count;
      step = static_cast<int32_t>(q);
      if (q == 0) step = 1;
    }
    bool kernel = false;
    if (gl == 0x10000 && gr == gl) {
      if (rate == gl) {
        if (step == 0) return call5(c, 0x14ce, ppOut, ppIn, count, v, mode).eax;
      } else {
        kernel = true;
      }
    }
    if (!kernel && rate == 0x10000) return call5(c, 0x15e6, ppOut, ppIn, count, v, mode).eax;
  }
  prologue_r(c, v, mode, true, s, r);
  return rated_kernel(c, {false, false, true, true, true, 1, true, false}, ppOut, ppIn, count, v, mode, s, r.gainL, r.gainR,
                      r.rate, step, r.ring, SG_NEGCTL(g_negctl_mixRatedMono16AddFade));
}

// mixRatedStereo8Add 0x152b0: 8-bit stereo, add; no eof flag.
int g_negctl_mixRatedStereo8Add = 0;
u32 mixRatedStereo8Add(Ctx &c, u32 ppOut, u32 ppIn, u32 count, u32 v, int32_t mode) {
  MixS s;
  MixR r;
  prologue_r(c, v, mode, false, s, r);
  return rated_kernel(c, {true, true, true, false, true, 0, false, false}, ppOut, ppIn, count, v, mode, s, r.gainL, r.gainR,
                      r.rate, 0, r.ring, SG_NEGCTL(g_negctl_mixRatedStereo8Add));
}
// mixRatedMono8Add 0x156c0: 8-bit mono, add; eof flag; with the count spent a wrapping non-chained voice falls into the
// chained block (mixRatedMono8Add+0x205 has no `jmp`).
int g_negctl_mixRatedMono8Add = 0;
u32 mixRatedMono8Add(Ctx &c, u32 ppOut, u32 ppIn, u32 count, u32 v, int32_t mode) {
  MixS s;
  MixR r;
  prologue_r(c, v, mode, true, s, r);
  return rated_kernel(c, {true, false, true, true, true, 0, false, true}, ppOut, ppIn, count, v, mode, s, r.gainL, r.gainR,
                      r.rate, 0, r.ring, SG_NEGCTL(g_negctl_mixRatedMono8Add));
}
// mixRatedStereo16Set 0x15f70: 16-bit stereo, store; eof flag.
int g_negctl_mixRatedStereo16Set = 0;
u32 mixRatedStereo16Set(Ctx &c, u32 ppOut, u32 ppIn, u32 count, u32 v, int32_t mode) {
  MixS s;
  MixR r;
  prologue_r(c, v, mode, true, s, r);
  return rated_kernel(c, {false, true, false, true, true, 0, false, false}, ppOut, ppIn, count, v, mode, s, r.gainL, r.gainR,
                      r.rate, 0, r.ring, SG_NEGCTL(g_negctl_mixRatedStereo16Set));
}
// mixRatedMono16SetFade 0x16850: 16-bit mono, store, fade; a wrap of a non-chained looping voice does not count. Mode 1: unity
// gains and rate call mixMonoDupSet (thunk 0x251d), a unity rate with other gains mixMonoGainFadeSet (thunk 0x19ec) with the
// mode argument 1; other modes: unity gains and rate with bit 3 of +0x9c clear call mixMonoDupSet, a unity rate otherwise
// mixMonoGainFadeSet; the kernel's fade step is +0xec / count when that bit is set (no floor).
int g_negctl_mixRatedMono16SetFade = 0;
u32 mixRatedMono16SetFade(Ctx &c, u32 ppOut, u32 ppIn, u32 count, u32 v, int32_t mode) {
  MixS s;
  MixR r;
  int32_t step = 0;
  if (mode == 1) {
    const u32 gl = ld<u32>(c, v + 0x1e8), gr = ld<u32>(c, v + 0x1f8), rate = ld<u32>(c, v + 0x220);
    if (gl == 0x10000 && gr == gl) {
      if (rate == gl) return call5(c, 0x251d, ppOut, ppIn, count, v, mode).eax;
    } else if (rate == 0x10000) {
      return call5(c, 0x19ec, ppOut, ppIn, count, v, 1).eax;
    }
  } else {
    const u32 gl = ld<u32>(c, v + 0x78), gr = ld<u32>(c, v + 0x68), rate = ld<u32>(c, v + 0x4c);
    bool to_resample = false;
    if (gl == 0x10000 && gr == gl) {
      if (rate == gl) {
        if (!(ld<uint8_t>(c, v + 0x9c) & 8)) return call5(c, 0x251d, ppOut, ppIn, count, v, mode).eax;
        to_resample = true;
      }
    } else {
      to_resample = true;
    }
    if (to_resample && rate == 0x10000) return call5(c, 0x19ec, ppOut, ppIn, count, v, mode).eax;
    if (ld<uint8_t>(c, v + 0x9c) & 8) {
      if (count == 0) throw DivideError{};
      step = static_cast<int32_t>(ld<u32>(c, v + 0xec) / count);
    }
  }
  prologue_r(c, v, mode, true, s, r);
  return rated_kernel(c, {false, false, false, true, false, 2, false, false}, ppOut, ppIn, count, v, mode, s, r.gainL,
                      r.gainR, r.rate, step, r.ring, SG_NEGCTL(g_negctl_mixRatedMono16SetFade));
}
// mixRatedStereo8Set 0x16d90: 8-bit stereo, store; no eof flag.
int g_negctl_mixRatedStereo8Set = 0;
u32 mixRatedStereo8Set(Ctx &c, u32 ppOut, u32 ppIn, u32 count, u32 v, int32_t mode) {
  MixS s;
  MixR r;
  prologue_r(c, v, mode, false, s, r);
  return rated_kernel(c, {true, true, false, false, true, 0, false, false}, ppOut, ppIn, count, v, mode, s, r.gainL, r.gainR,
                      r.rate, 0, r.ring, SG_NEGCTL(g_negctl_mixRatedStereo8Set));
}
// mixRatedMono8Set 0x17160: 8-bit mono, store; eof flag; a wrap of a non-chained looping voice does not count.
int g_negctl_mixRatedMono8Set = 0;
u32 mixRatedMono8Set(Ctx &c, u32 ppOut, u32 ppIn, u32 count, u32 v, int32_t mode) {
  MixS s;
  MixR r;
  prologue_r(c, v, mode, true, s, r);
  return rated_kernel(c, {true, false, false, true, false, 0, false, false}, ppOut, ppIn, count, v, mode, s, r.gainL, r.gainR,
                      r.rate, 0, r.ring, SG_NEGCTL(g_negctl_mixRatedMono8Set));
}

#define SG_INT5(fn)                                                                                     \
  void a_##fn(Ctx &c, const uint32_t *a, Ret &r) { r.set_u32(fn(c, a[0], a[1], a[2], a[3], static_cast<int32_t>(a[4]))); }
SG_INT5(mixStereoAdd)
SG_INT5(mixStereoGainAdd)
SG_INT5(mixRatedStereo16Add)
SG_INT5(mixMonoDupAdd)
SG_INT5(mixMonoGainFadeAdd)
SG_INT5(mixRatedMono16AddFade)
SG_INT5(mixRatedStereo8Add)
SG_INT5(mixRatedMono8Add)
SG_INT5(mixRatedStereo16Set)
SG_INT5(mixMonoDupSet)
SG_INT5(mixMonoGainFadeSet)
SG_INT5(mixRatedMono16SetFade)
SG_INT5(mixRatedStereo8Set)
SG_INT5(mixRatedMono8Set)
#undef SG_INT5

// ---------------------------------------------------------------------------------------------------------------- C.
// The MMX renderers (emu-only rows: installed only by an unreferenced constructor, table rva 0x5b830): __cdecl (ppOut, ppIn,
// count, voice, mode) like the rated mixers, the same voice fields (prologue_r's), 16-bit stereo OUTPUT words. Two output
// frames per MMX pass: lane A and lane B of the frame slots (dwords, two words each: cur, next, fraction) feed
//   mm1 = psubsw(next, cur); mm1 = pmulhw(mm1, frac); mm0 = paddsw(cur, mm1); mm0 = pmulhw(mm0, gains); mm0 = psllw(mm0, 1)
// (+ paddsw with the output qword in the Add twins), stored as one qword. The fraction lane is (frac - 0x8000) in both words.
// When the count runs out after lane A the pass still runs with lane B's slots as the previous pass left them (or as the
// prologue set them), and the whole qword is stored: the frame past the count is overwritten. Three bodies, each with a
// store twin that lacks only the `paddsw mm0, qword [edi]`:
//   stereo 16 (mixRenderRatedC 0x25190 / Voice_renderStream32 0x26800): a frame is the dword at p, frames 4 bytes apart;
//   mono 8 (Voice_renderStream8c 0x25c80 / 8d 0x26d30): the byte at p as ((b << 8) - 0x8000) in both words, 1 byte apart;
//   mono 16 with fade (Voice_renderStream16b 0x256c0 / 16c 0x26260): the word at p in both words, 2 bytes apart; the gains
//     fall by the step each pass (psubusw); 16b's step is +0xec / count + 1, 16c's +0xec / count.
// Exit: MMX makes every x87 register valid with TOP 0 and `emms` empties them.
enum MmxFmt { kStereo16 = 0, kMono8 = 1, kMono16 = 2 };

uint16_t sat16(int32_t v) { return static_cast<uint16_t>(v > 32767 ? 32767 : (v < -32768 ? -32768 : v)); }
uint64_t mmx_op(uint64_t a, uint64_t b, int op) {   // op 0 psubsw a - b, 1 pmulhw, 2 paddsw, 3 psubusw
  uint64_t r = 0;
  for (int i = 0; i < 4; i++) {
    const int32_t x = static_cast<int16_t>(a >> (16 * i)), y = static_cast<int16_t>(b >> (16 * i));
    const uint32_t ux = static_cast<uint16_t>(a >> (16 * i)), uy = static_cast<uint16_t>(b >> (16 * i));
    uint16_t w;
    switch (op) {
      case 0: w = sat16(x - y); break;
      case 1: w = static_cast<uint16_t>(static_cast<uint32_t>(x * y) >> 16); break;
      case 2: w = sat16(x + y); break;
      default: w = static_cast<uint16_t>(ux > uy ? ux - uy : 0); break;
    }
    r |= static_cast<uint64_t>(w) << (16 * i);
  }
  return r;
}
uint64_t psllw1(uint64_t a) {
  uint64_t r = 0;
  for (int i = 0; i < 4; i++) r |= static_cast<uint64_t>(static_cast<uint16_t>((a >> (16 * i)) << 1)) << (16 * i);
  return r;
}
u32 dup16(uint16_t w) { return (static_cast<u32>(w) << 16) | w; }
uint64_t qw(u32 lo, u32 hi) { return (static_cast<uint64_t>(hi) << 32) | lo; }

template <int FMT, bool ADD>
u32 mmx_render(Ctx &c, u32 ppOut, u32 ppIn, u32 count, u32 v, int32_t mode, int neg) {
  constexpr u32 kStep = FMT == kStereo16 ? 4 : (FMT == kMono8 ? 1 : 2);        // bytes between frames
  constexpr int kShift = FMT == kStereo16 ? 2 : (FMT == kMono8 ? 0 : 1);       // integer part -> bytes
  constexpr u32 kBnextOff = FMT == kMono8 ? 1 : 4;                              // the lane-B next handler's offset
  int32_t loopFlag = 0, chained = 0, wraps = 0, eof = 0, step = 0;
  u32 chain = 0, limit, loopStart, rate, ring, gL, gR;
  if (mode == 1) {
    rate = ld<u32>(c, v + 0x220);
    const u32 rec = ld<u32>(c, v + 8);
    gL = ld<u32>(c, v + 0x1e8);
    gR = ld<u32>(c, v + 0x1f8);
    ring = v + 0x1358;
    limit = ld<u32>(c, rec + 0x20);
    if (ld<u32>(c, rec + 0x1c) > limit) limit = ld<u32>(c, rec + 0x24);   // unsigned `jbe`
    loopStart = ld<u32>(c, rec + 0x18);
    loopFlag = limit == ld<u32>(c, rec + 0x24) ? 1 : 0;                   // `sete`
    if ((ld<uint8_t>(c, v + 0x58) & 0x20) && loopFlag) eof = 1;
  } else {
    const uint8_t f9c = ld<uint8_t>(c, v + 0x9c);
    gL = ld<u32>(c, v + 0x78);
    gR = ld<u32>(c, v + 0x68);
    if constexpr (FMT == kMono16) {
      if (f9c & 8) {                                                       // the fade step: unsigned `div` by the count
        if (count == 0) throw DivideError{};
        step = static_cast<int32_t>(ld<u32>(c, v + 0xec) / count + (ADD ? 1u : 0u));
      }
    }
    ring = v + 0xb8;
    loopFlag = (ld<uint8_t>(c, v + 0x98) & 1) && !(f9c & 2) ? 1 : 0;
    chained = f9c & 1;
    if (chained) {
      chain = ld<u32>(c, v + 0xb4);
      loopStart = ld<u32>(c, chain + 0x18);
      limit = ld<u32>(c, v + 0x20);
    } else if (loopFlag) {                                                 // `cmp loopFlag, edx` with edx = 0
      loopStart = ld<u32>(c, v + 0xd8);
      limit = ld<u32>(c, v + 0xd4);
    } else {
      loopStart = ld<u32>(c, v + 0x18);
      limit = ld<u32>(c, v + 0x20);
    }
    rate = ld<u32>(c, v + 0x4c);
  }
  if (gR == 0x10000) gR = 0xffff;
  if (gL == 0x10000) gL = 0xffff;
  const u32 packed = ((gR & 0xfffffffeu) << 15) | ((gL >> 1) & 0xffffu);
  uint64_t mm3 = qw(packed, packed);
  const uint16_t s16 = sat16(step);                                        // packssdw of (step, step) with itself
  const uint64_t mm4 = qw(dup16(s16), dup16(s16));
  // the frame slots: lane A / lane B of cur, next and the fraction (lane B's cur starts as the prologue's last store there)
  u32 aCur = 0, bCur = packed, aNext = 0, bNext = 0, aFrac = 0, bFrac = 0;
  if constexpr (FMT == kMono16) bCur = static_cast<u32>(step);
  auto cur = [&](u32 p) -> u32 {
    if constexpr (FMT == kStereo16) return ld<u32>(c, p);
    if constexpr (FMT == kMono16) return dup16(ld<uint16_t>(c, p));
    return dup16(static_cast<uint16_t>((static_cast<u32>(ld<uint8_t>(c, p)) << 8) - 0x8000u));
  };
  auto raw = [&](u32 p) -> u32 {
    if constexpr (FMT == kMono16) return ld<u32>(c, p);
    return cur(p);
  };   // the mono-16 handlers load a dword
  auto next = [&](u32 p) -> u32 { return cur(p + kStep); };   // the frame after p (stereo 16: the dword at p + 4)
  auto clip = [&](u32 x) -> u32 { return static_cast<int32_t>(x) >= static_cast<int32_t>(limit) ? x - limit : 0; };
  auto frac_lane = [&]() { return dup16(static_cast<uint16_t>(ld<uint16_t>(c, ring + 2) - 0x8000u)); };
  auto advance = [&](u32 &esi) {
    const u32 lo = ld<u32>(c, ring + 2), nlo = lo + rate;
    st<u32>(c, ring + 2, nlo);
    if (nlo < rate) st<uint16_t>(c, ring + 6, static_cast<uint16_t>(ld<uint16_t>(c, ring + 6) + 1));   // adc
    esi = ld<u32>(c, ppIn) + (ld<u32>(c, ring + 4) << kShift);
  };
  auto ge = [&](u32 p) { return static_cast<int32_t>(p) >= static_cast<int32_t>(limit); };   // signed `jge`
  Unit &u = x87::unit(c);
  u32 ecx = count, esi = ld<u32>(c, ppIn), edi = ld<u32>(c, ppOut), adv = 0;
  for (;;) {
    // lane A
    aCur = cur(esi);
    if (ge(esi + kStep)) {
      if (loopFlag != 1) {
        aNext = 0;
      } else {
        const u32 o = clip(esi);
        st<u32>(c, ring + 4, o);
        esi = o + loopStart;
        aNext = cur(esi);
        st<u32>(c, ppIn, esi);
        if (eof) loopFlag = 0;
      }
    } else {
      aNext = next(esi);
    }
    aFrac = frac_lane();
    advance(esi);
    adv += 4;
    --ecx;
    if (static_cast<int32_t>(ecx) > 0) {
      // lane B
      if (ge(esi)) {
        if (loopFlag != 1) {
          bCur = 0;
          bNext = 0;
        } else {
          const u32 o = clip(esi);
          st<u32>(c, ring + 4, o);
          esi = o + loopStart;
          bCur = raw(esi);
          bNext = next(esi);
          st<u32>(c, ppIn, esi);
          if (eof) loopFlag = 0;
        }
      } else {
        bCur = cur(esi);
        if (ge(esi + kStep)) {
          if (loopFlag != 1) {
            bNext = 0;
          } else {
            const u32 o = clip(esi + kBnextOff);
            st<u32>(c, ring + 4, o);
            bNext = raw(o + loopStart);
            st<u32>(c, ppIn, esi);
            if (eof) loopFlag = 0;
          }
        } else {
          bNext = next(esi);
        }
      }
      bFrac = frac_lane();
      advance(esi);
      adv += 4;
      --ecx;
    }
    // the MMX pass
    u.mmx_enter();
    uint64_t mm0 = qw(aCur, bCur), mm1 = qw(aNext, bNext);
    const uint64_t mm2 = qw(aFrac, bFrac);
    mm1 = mmx_op(mm1, mm0, 0);
    mm1 = mmx_op(mm1, mm2, 1);
    mm0 = mmx_op(mm0, mm1, 2);
    mm0 = mmx_op(mm0, mm3, 1);
    if (neg != 1) mm0 = psllw1(mm0);         // negative control 1: no doubling
    if constexpr (ADD) mm0 = mmx_op(mm0, ld<uint64_t>(c, edi), 2);
    st<uint64_t>(c, edi, mm0);
    if constexpr (FMT == kMono16) mm3 = mmx_op(mm3, mm4, 3);   // the fade: psubusw
    edi += adv;
    adv = 0;
    if (static_cast<int32_t>(ecx) <= 0) break;
    if (!ge(esi)) continue;
    // the wrap handler
    if (chained == 0) {
      if (loopFlag == 0) break;
      ++wraps;
      st<u32>(c, ring + 4, esi - limit);
      esi = loopStart;
      st<u32>(c, ppIn, esi);
      if (ecx != 0) continue;
      break;
    }
    ++wraps;
    st<u32>(c, ring + 4, 0);
    esi = loopStart;
    st<u32>(c, ppIn, esi);
    const u32 curc = chain;
    limit = ld<u32>(c, curc + 0xd4);
    chain = ld<u32>(c, curc + 0xb4);
    if (chain != 0) {
      loopStart = ld<u32>(c, chain + 0xd8);
    } else {
      chained = 0;
      loopFlag = 0;
    }
    if (ecx != 0) continue;
    break;
  }
  const u32 adv_int = ld<u32>(c, ring + 4);
  st<u32>(c, ring + 4, 0);
  st<u32>(c, ppIn, esi);
  st<u32>(c, ppOut, edi);
  u.emms();
  // the counters: mode 1 of the stereo-16 pair counts the frames rendered, every other case the integer part
  const u32 a = mode == 1 ? 0x16c : 0x88, w = mode == 1 ? 0x1360 : 0xcc;
  const u32 frames = count - ecx;
  bool count_frames = false;
  if constexpr (FMT == kStereo16) count_frames = mode == 1;
  st<u32>(c, v + a, ld<u32>(c, v + a) + (count_frames ? frames : adv_int));
  st<u32>(c, v + w, ld<u32>(c, v + w) + static_cast<u32>(wraps));
  return frames;
}

#define SG_MMX(fn, FMT, ADD)                                                                               \
  int g_negctl_##fn = 0;                                                                                   \
  void a_##fn(Ctx &c, const uint32_t *a, Ret &r) {                                                         \
    SG_X87_GUARD(c, r.set_u32(mmx_render<FMT, ADD>(c, a[0], a[1], a[2], a[3], static_cast<int32_t>(a[4]), \
                                                   SG_NEGCTL(g_negctl_##fn))))                              \
  }
SG_MMX(mixRenderRatedC, kStereo16, true)
SG_MMX(Voice_renderStream32, kStereo16, false)
SG_MMX(Voice_renderStream8c, kMono8, true)
SG_MMX(Voice_renderStream8d, kMono8, false)
SG_MMX(Voice_renderStream16b, kMono16, true)
SG_MMX(Voice_renderStream16c, kMono16, false)
#undef SG_MMX

// ---------------------------------------------------------------------------------------------------------------- D.
// The mixer objects (the c3dj / c3ew hooks cite every instruction). Shared channel layout: +0x40 a sound object released by
// delete_sound (thunk 0x1a87, cdecl), +0x44 flags (bit 1: linked into the module's active list), +0x48 / +0x4c the previous /
// next object, +0x50 a buffer released by operator delete. The list ends are g_activeListHead (rva 0xb5070) and the tail
// dword before it (rva 0xb506c, in soundstate.h's gap_52064). Only the last vtable a destructor stores is observable.
constexpr u32 kVtChannelFinal = 0x5bb80;   // the vtable every channel destructor stores last (rva)
constexpr u32 kThDeleteSound = 0x1a87;      // thunk -> delete_sound

u32 active_head(Ctx &c) { return snd_obj(c, &SoundState::g_activeListHead).va(); }
u32 active_tail(Ctx &c) { return snd_arr(c, &SoundState::gap_52064).va() + 8; }

// the unlink block (e.g. SubChannel_dtor+0x56..+0x91): prev's +0x4c or the head takes this+0x4c; next's +0x48 or the tail
// takes this+0x48; then the flag dword with bit 1 cleared, and the links cleared
void unlink_active(Ctx &c, u32 o) {
  const u32 prev = ld<u32>(c, o + 0x48);
  if (prev) st<u32>(c, prev + 0x4c, ld<u32>(c, o + 0x4c));
  else st<u32>(c, active_head(c), ld<u32>(c, o + 0x4c));
  const u32 next = ld<u32>(c, o + 0x4c);
  if (next) st<u32>(c, next + 0x48, ld<u32>(c, o + 0x48));
  else st<u32>(c, active_tail(c), ld<u32>(c, o + 0x48));
  const u32 flags = ld<u32>(c, o + 0x44);
  st<u32>(c, o + 0x4c, 0);
  st<u32>(c, o + 0x48, 0);
  st<u32>(c, o + 0x44, (flags & ~0xffu) | (flags & 0xfdu));
}
void delete_sound(Ctx &c, u32 p) { snd_call(c, kThDeleteSound, SndAbi::Cdecl, {p}); }

// destroyMixerVoices 0x2c5e0: the 0x28 pointers from +0x6c each go to operator delete and are cleared; then +0x50 (deleted
// when non-NULL, cleared always), the unlink when bit 1 of +0x44 was set, the sound object +0x40 (delete_sound, cleared), the
// final vtable. Negative control 1: the last pointer slot is not cleared.
int g_negctl_destroyMixerVoices = 0;
void destroyMixerVoices(Ctx &c, u32 o) {
  for (u32 i = 0; i < 0x28; i++) {
    const u32 e = ld<u32>(c, o + 0x6c + 4 * i);
    if (e) {
      snd_delete(c, e);
      if (!(SG_NEGCTL(g_negctl_destroyMixerVoices) == 1 && i == 0x27)) st<u32>(c, o + 0x6c + 4 * i, 0);
    }
  }
  const u32 p = ld<u32>(c, o + 0x50);
  if (p) snd_delete(c, p);
  const uint8_t fl = ld<uint8_t>(c, o + 0x44);
  st<u32>(c, o + 0x50, 0);
  if (fl & 2) unlink_active(c, o);
  const u32 p2 = ld<u32>(c, o + 0x50);       // zero here: the second stage's buffer test (destroyMixerVoices+0xc1) is dead
  if (p2) {
    snd_delete(c, p2);
    st<u32>(c, o + 0x50, 0);
  }
  const u32 q = ld<u32>(c, o + 0x40);
  if (q) {
    delete_sound(c, q);
    st<u32>(c, o + 0x40, 0);
  }
  if (ld<uint8_t>(c, o + 0x44) & 2) unlink_active(c, o);   // dead after the first block
  st<u32>(c, o, snd_va(c, kVtChannelFinal));
}

// Mixer_dtor 0x2cb00: the 0x28 pointers from +0x58, then +0x50 (deleted and cleared), +0x40 (delete_sound, cleared), the unlink,
// the final vtable. Negative control 1: +0x40 is not cleared.
int g_negctl_Mixer_dtor = 0;
void Mixer_dtor(Ctx &c, u32 o) {
  for (u32 i = 0; i < 0x28; i++) {
    const u32 e = ld<u32>(c, o + 0x58 + 4 * i);
    if (e) {
      snd_delete(c, e);
      st<u32>(c, o + 0x58 + 4 * i, 0);
    }
  }
  const u32 p = ld<u32>(c, o + 0x50);
  if (p) {
    snd_delete(c, p);
    st<u32>(c, o + 0x50, 0);
  }
  const u32 q = ld<u32>(c, o + 0x40);
  if (q) {
    delete_sound(c, q);
    if (SG_NEGCTL(g_negctl_Mixer_dtor) != 1) st<u32>(c, o + 0x40, 0);
  }
  if (ld<uint8_t>(c, o + 0x44) & 2) unlink_active(c, o);
  st<u32>(c, o, snd_va(c, kVtChannelFinal));
}

// The 30-byte scalar-deleting wrappers: the destructor through its thunk, operator delete when bit 0 of the flags is set,
// `this` returned. Negative control 1: the delete ignores the flag.
u32 scalar_dtor(Ctx &c, u32 thunk, u32 self, u32 flags, int neg) {
  snd_call(c, thunk, SndAbi::Thiscall, {self});
  if ((flags & 1) || neg == 1) snd_delete(c, self);
  return self;
}
int g_negctl_MixerVoiceArray_scalarDtor = 0;   // thunk 0x1f1e -> destroyMixerVoices
int g_negctl_Mixer_scalarDtor = 0;             // thunk 0x1e92 -> Mixer_dtor

// Mixer_ctor 0x2c9f0: the channel base initialisation (SubChannel_ctor's first block: +4 = 0x7f, +8 = 0, +0xc..+0x2f, +0x30,
// +0x48, +0x4c, +0x40, +0x50, +0x44 = 0, +0x38 = 0x3e8; the virtual call on +0x40 it guards is never reached, +0x40 being 0),
// +0x54 = 0, the vtable, the 0x28 dwords from +0x58 cleared, Sound_setSourceType (thunk 0x18a2) with 8. Returns this.
// Negative control 1: +0x38 = 0x3e7.
int g_negctl_Mixer_ctor = 0;
u32 Mixer_ctor(Ctx &c, u32 o) {
  st<u32>(c, o + 4, 0x7f);
  st<u32>(c, o + 8, 0);
  for (u32 i = 0; i < 9; i++) st<u32>(c, o + 0xc + 4 * i, 0);
  st<u32>(c, o + 0x30, 0);
  st<u32>(c, o + 0x48, 0);
  st<u32>(c, o + 0x4c, 0);
  st<u32>(c, o + 0x40, 0);
  st<u32>(c, o + 0x50, 0);
  st<u32>(c, o + 0x44, 0);
  st<u32>(c, o + 0x38, SG_NEGCTL(g_negctl_Mixer_ctor) == 1 ? 0x3e7u : 0x3e8u);
  const u32 sub = ld<u32>(c, o + 0x40);
  if (sub) snd_vcall(c, sub, 4, SndAbi::Thiscall, {0x3e8});
  st<u32>(c, o + 0x54, 0);
  st<u32>(c, o, snd_va(c, snd_rva::vtbl_Mixer));
  for (u32 i = 0; i < 0x28; i++) st<u32>(c, o + 0x58 + 4 * i, 0);
  snd_call(c, 0x18a2, SndAbi::Thiscall, {o, 8});
  return o;
}

// MixBuffer_ctor 0x2cee0: the vtable, +0x208 = 2, +0x20c / +0x210 / +0x204 / +0x214 = 0, +0x218 = 0xfa, the 0x80 dwords +4..
// cleared. Returns this. Negative control 1: +0x218 = 0xfb.
int g_negctl_MixBuffer_ctor = 0;
u32 MixBuffer_ctor(Ctx &c, u32 o) {
  st<u32>(c, o, snd_va(c, snd_rva::vtbl_MixBuffer));
  st<u32>(c, o + 0x208, 0);
  st<u32>(c, o + 0x208, 2);
  st<u32>(c, o + 0x20c, 0);
  st<u32>(c, o + 0x210, 0);
  st<u32>(c, o + 0x204, 0);
  st<u32>(c, o + 0x214, 0);
  st<u32>(c, o + 0x218, SG_NEGCTL(g_negctl_MixBuffer_ctor) == 1 ? 0xfbu : 0xfau);
  for (u32 i = 0; i < 0x80; i++) st<u32>(c, o + 4 + 4 * i, 0);
  return o;
}

void a_destroyMixerVoices(Ctx &c, const uint32_t *a, Ret &) { destroyMixerVoices(c, a[0]); }
void a_Mixer_dtor(Ctx &c, const uint32_t *a, Ret &) { Mixer_dtor(c, a[0]); }
void a_MixerVoiceArray_scalarDtor(Ctx &c, const uint32_t *a, Ret &r) {
  r.set_u32(scalar_dtor(c, 0x1f1e, a[0], a[1], SG_NEGCTL(g_negctl_MixerVoiceArray_scalarDtor)));
}
void a_Mixer_scalarDtor(Ctx &c, const uint32_t *a, Ret &r) {
  r.set_u32(scalar_dtor(c, 0x1e92, a[0], a[1], SG_NEGCTL(g_negctl_Mixer_scalarDtor)));
}
void a_Mixer_ctor(Ctx &c, const uint32_t *a, Ret &r) { r.set_u32(Mixer_ctor(c, a[0])); }
void a_MixBuffer_ctor(Ctx &c, const uint32_t *a, Ret &r) { r.set_u32(MixBuffer_ctor(c, a[0])); }

// ---------------------------------------------------------------------------------------------------------------- E.
// The channel renderers. Every x87 instruction is one operation on the unit (its register stack included: a callee's
// leftovers stay below what these push). Locals the original passes to a callee by address live on the core's stack at the
// original's offsets (SndFrame, portsim/sim/audio.h).
constexpr u32 kKZero = 0x5b498;   // double 0.0 (rva)
constexpr u32 kK127 = 0x5b478;    // double 127.0 (rva)
uint64_t rdata64(Ctx &c, u32 rva) { return ld<uint64_t>(c, snd_va(c, rva)); }
void zero_bytes(Ctx &c, u32 p, u32 n) {       // rep stosd / rep stosb of 0
  for (u32 i = 0; i < n; i++) st<uint8_t>(c, p + i, 0);
}
void move_bytes(Ctx &c, u32 d, u32 s, u32 n) {   // rep movsd (n >> 2 dwords) then rep movsb (n & 3), forward
  for (u32 i = 0; i < (n >> 2); i++) st<u32>(c, d + 4 * i, ld<u32>(c, s + 4 * i));
  const u32 k = n & ~3u;
  for (u32 i = 0; i < (n & 3); i++) st<uint8_t>(c, d + k + i, ld<uint8_t>(c, s + k + i));
}

// renderChannel 0x35990: thiscall (buf, count, pass, out), `ret 0x10`. pass > 0x18 returns 0xa. With bit 3 of +0x58 (read once)
// or bit 4 of the byte +0x5c the fade value +0x1d8 moves by the signed step +0x1c4 (a negative step adds |step| capped at
// 0x10000, a step below the value subtracts it, else 0). Then: bit 0 clear and the byte +0x5c without 0x44 ->
// renderChannelSingle (thunk 0x2649) with (buf, count, pass, out); else bit 6 of +0x5c -> renderChannelStreamed (thunk 0x20c7)
// with (count, pass, out); else, with any of bits 12-27: 16 voice records from +0x2d4 - 0x9c, 0x110 apart, for each with its bit
// set timeGetTime (the IAT slot rva 0xb8654, result unused) and renderChannelVoice (thunk 0x23f6); *out != count or bit 2 of the
// record's byte clears its bit; the running maximum of *out; the first rendered record with pass >= 0xc lowers pass by 0xc for
// the rest. After the 16, *out = the maximum and bit 4 is set when bits 12-27 are all clear. With none of them, bits 2 and 6
// of +0x5c, bit 28 and pass >= 0xc zero count * 16 bytes of buf. Every path past the first test returns 0.
// Negative control 1: the maximum is the last record's *out instead.
int g_negctl_renderChannel = 0;
constexpr u32 kIatTimeGetTime = 0xb8654;
u32 renderChannel(Ctx &c, u32 s, u32 a0, u32 a1, u32 a2, u32 a3) {
  if (a2 > 0x18) return 0xa;
  const u32 f = ld<u32>(c, s + 0x58);
  if ((f & 8) || (ld<uint8_t>(c, s + 0x5c) & 0x10)) {
    const int32_t step = ld<int32_t>(c, s + 0x1c4);
    if (step < 0) {
      const u32 x = ld<u32>(c, s + 0x1d8) - static_cast<u32>(step);
      st<u32>(c, s + 0x1d8, x);
      if (x > 0x10000) st<u32>(c, s + 0x1d8, 0x10000);
    } else {
      const int32_t cur = ld<int32_t>(c, s + 0x1d8);
      if (step < cur) st<int32_t>(c, s + 0x1d8, cur - step);
      else st<u32>(c, s + 0x1d8, 0);
    }
  }
  if (!(f & 1) && !(ld<uint8_t>(c, s + 0x5c) & 0x44)) {
    snd_call(c, 0x2649, SndAbi::Thiscall, {s, a0, a1, a2, a3});
    return 0;
  }
  const u32 g5c = ld<u32>(c, s + 0x5c);
  if (g5c & 0x40) {
    snd_call(c, 0x20c7, SndAbi::Thiscall, {s, a1, a2, a3});
    return 0;
  }
  if (f & 0x0ffff000u) {
    bool lower = a2 >= 0xc;
    u32 mask = 1, maxv = 0, rec = s + 0x2d4;
    for (int k = 0x10; k; k--) {
      if (((ld<u32>(c, s + 0x58) >> 12) & mask) & 0xffff) {
        snd_import(c, kIatTimeGetTime, SndAbi::Stdcall, {});
        snd_call(c, 0x23f6, SndAbi::Thiscall, {s, rec - 0x9c, a0, a1, a2, a3});
        const u32 o = ld<u32>(c, a3);
        if (o != a1 || (ld<uint8_t>(c, rec) & 4)) st<u32>(c, s + 0x58, ld<u32>(c, s + 0x58) & (((~mask) << 12) | 0xf0000fffu));
        if (SG_NEGCTL(g_negctl_renderChannel) == 1) maxv = ld<u32>(c, a3);
        else if (ld<u32>(c, a3) > maxv) maxv = ld<u32>(c, a3);
        if (lower) {
          lower = false;
          a2 -= 0xc;
        }
      }
      mask <<= 1;
      rec += 0x110;
    }
    st<u32>(c, a3, maxv);
    const u32 f2 = ld<u32>(c, s + 0x58);
    if (!(f2 & 0x0ffff000u)) st<u32>(c, s + 0x58, f2 | 0x10);
    return 0;
  }
  if ((g5c & 4) && ((g5c >> 6) & 1) && (f & 0x10000000u) && a2 >= 0xc) zero_bytes(c, a0, a1 << 4);
  return 0;
}

// renderChannelSingle 0x36280: thiscall (buf, count, pass, out), `ret 0x10`. count > 0x7d0 returns 0x22. pass >= 0xc picks the
// overwriting mixers. The position +0x228 advances by +0x230; a non-zero limit +0x22c above it sets *out = count and (with the
// flag) zeroes count * 16 bytes, return 0. Otherwise the dword +0x1c of the object at +8 goes to a frame local and ONE mixer
// runs, __cdecl (buf, &local, count, this, 1), by the word +0x182 == 2, then the word +0x18e == 8: with the flag
// Voice_mixSetStereo16 (thunk 0x10dc) / Voice_mixSet8bit (0x2220) / Voice_mixSetMono16 (0x1f64), without it
// Voice_mixAddStereo16 (0x12ee) / Voice_mixAdd8bit (0x267b) / Voice_mixAddMono16 (0x2617); its eax goes to *out. The fade
// (bit 3 of +0x58 or bit 4 of the byte +0x5c) runs on top of the registers the mixer left (renderChannelSingle+0x169..+0x203,
// below), then Channel_setPanGains (thunk 0x1a7d) with +0x224. *out > count returns 0xa; the local goes back to (+8)+0x1c;
// *out below count zeroes the rest (with the flag), sets bit 4, return 0; *out == count sets bit 1 of +0x58 when bits 0, 5
// and 10 are clear and the distance ((+8)+0x1c - +0x1378) & +0x1374 is above (+8)+0x14 >> 2 sets bit 10, counts +0x1370 up and
// posts 0x7eb: PostMessageA (IAT rva 0xb85a0) to the window +0x17c when g_soundEngine has bit 5 of its +0x1a4, else
// postSoundThreadMessage (thunk 0x11d6) on the signal object (rva 0xb5038). Returns 0.
// Negative control 1: the fade's comparison against +0x210 reads `below` as `not below`.
int g_negctl_renderChannelSingle = 0;
constexpr u32 kIatPostMessageA = 0xb85a0;
constexpr u32 kSigObj = 0xb5038;
u32 renderChannelSingle(Ctx &c, u32 s, u32 a0, u32 a1, u32 a2, u32 a3) {
  SndFrame fr(c, 0x14);           // push ecx; push ebx; push ebp; push esi; push edi (0x36280..0x36284)
  const u32 cursor = fr.arg(1);   // the caller's count slot, reused for the sample cursor (0x36318, `lea` at 0x3632f)
  if (a1 > 0x7d0) return 0x22;
  const bool lower = a2 >= 0xc;
  const u32 pos = ld<u32>(c, s + 0x228) + ld<u32>(c, s + 0x230);
  st<u32>(c, s + 0x228, pos);
  const u32 lim = ld<u32>(c, s + 0x22c);
  if (lim != 0 && lim > pos) {
    st<u32>(c, a3, a1);
    if (lower) zero_bytes(c, a0, a1 << 4);
    return 0;
  }
  st<u32>(c, cursor, ld<u32>(c, ld<u32>(c, s + 8) + 0x1c));
  u32 rva;
  if (lower) {
    if (ld<uint16_t>(c, s + 0x182) == 2) rva = 0x10dc;
    else if (ld<uint16_t>(c, s + 0x18e) == 8) rva = 0x2220;
    else rva = 0x1f64;
  } else {
    if (ld<uint16_t>(c, s + 0x182) == 2) rva = 0x12ee;
    else if (ld<uint16_t>(c, s + 0x18e) == 8) rva = 0x267b;
    else rva = 0x2617;
  }
  st<u32>(c, a3, snd_call(c, rva, SndAbi::Cdecl, {a0, cursor, a1, s, 1}).eax);
  const u32 f = ld<u32>(c, s + 0x58);
  if ((f & 8) || (ld<uint8_t>(c, s + 0x5c) & 0x10)) {
    const u32 cc = ld<u32>(c, s + 0x5c);
    if (cc & 2) {
      st<u32>(c, s + 0x58, f | 0x10);
    } else {
      Unit &u = x87::unit(c);
      u.push(x87::fld64(ld64(c, s + 0x1e0)));                                   // fld qword +0x1e0
      u.st(0) = x87::sub(u.st(0), x87::from_f64(ld64(c, s + 0x1c8)), u.cw);     // fsub qword +0x1c8
      st<uint64_t>(c, s + 0x1e0, x87::fst64(u.st(0), u.cw));                   // fst qword +0x1e0
      if (cc & 8) {
        u.set_cc(x87::fcom(u.st(0), x87::from_f64(ld64(c, s + 0x210))));      // fcomp qword +0x210
        u.pop();
        const bool below = u.cc(x87::kC0);                                      // test ah, 1
        if (SG_NEGCTL(g_negctl_renderChannelSingle) == 1 ? below : !below) {
          st<u32>(c, s + 0x58, f & 0xfffffff7u);
          st<u32>(c, s + 0x5c, cc & 0xfffffff7u);
        }
      } else if (cc & 0x10) {
        u.push(x87::fld64(ld64(c, s + 0x1c8)));                                 // fld qword +0x1c8
        u.set_cc(x87::fcom(u.st(0), x87::from_f64(rdata64(c, kKZero))));        // fcomp qword 0.0
        u.pop();
        const uint16_t first = u.sw;                                            // fnstsw ax
        u.set_cc(x87::fcom(u.st(0), x87::from_f64(ld64(c, s + 0x1d0))));       // fcomp qword +0x1d0 (pops d)
        u.pop();
        const uint16_t second = u.sw;                                           // fnstsw ax
        if (first & (x87::kC0 | x87::kC3)) {                                    // +0x1c8 <= 0.0 or unordered
          if (!(second & x87::kC0)) st<u32>(c, s + 0x5c, cc & 0xffffffefu);
        } else {
          if (second & (x87::kC0 | x87::kC3)) st<u32>(c, s + 0x5c, cc & 0xffffffefu);
        }
      } else {
        u.set_cc(x87::fcom(u.st(0), x87::from_f64(rdata64(c, kKZero))));        // fcom qword 0.0
        if (u.cc(x87::kC3)) st<u32>(c, s + 0x58, f | 0x10);                      // equal or unordered
        u.set_cc(x87::fcom(u.st(0), x87::from_f64(ld64(c, s + 0x1c8))));       // fcom qword +0x1c8
        if (u.sw & (x87::kC0 | x87::kC3)) {
          st<uint64_t>(c, s + 0x1c8, x87::fst64(u.pop(), u.cw));                // fstp qword +0x1c8
          st<u32>(c, s + 0x5c, cc | 2);
        } else {
          u.pop();                                                              // fstp st(0)
        }
      }
      snd_call(c, 0x1a7d, SndAbi::Thiscall, {s, ld<u32>(c, s + 0x224)});
    }
  }
  if (ld<u32>(c, a3) > a1) return 0xa;
  st<u32>(c, ld<u32>(c, s + 8) + 0x1c, ld<u32>(c, cursor));
  const u32 o = ld<u32>(c, a3);
  if (o != a1) {
    if (lower) zero_bytes(c, a0 + (o << 4), (a1 - o) << 4);
    st<u32>(c, s + 0x58, ld<u32>(c, s + 0x58) | 0x10);
    return 0;
  }
  st<u32>(c, a3, a1);
  const u32 g = ld<u32>(c, s + 0x58);
  if (!(g & 2) || (g & 0x21) || (g & 0x400)) return 0;
  const u32 x = ld<u32>(c, s + 8);
  const u32 dist = (ld<u32>(c, x + 0x1c) - ld<u32>(c, s + 0x1378)) & ld<u32>(c, s + 0x1374);
  if (!(dist > (ld<u32>(c, x + 0x14) >> 2))) return 0;
  st<u32>(c, s + 0x58, g | 0x400);
  st<u32>(c, s + 0x1370, ld<u32>(c, s + 0x1370) + 1);
  const u32 eng = snd_obj(c, &SoundState::g_soundEngine).get();
  if (eng && ((ld<u32>(c, eng + 0x1a4) >> 5) & 1)) {
    snd_import(c, kIatPostMessageA, SndAbi::Stdcall, {ld<u32>(c, s + 0x17c), 0x7eb, s, 0});
    return 0;
  }
  snd_call(c, 0x11d6, SndAbi::Thiscall, {snd_va(c, kSigObj), 0x7eb, s, 0});
  return 0;
}

// renderChannelStreamed's fade (renderChannelStreamed+0x422..+0x603) under the caller's control word, on the unit: flags
// fl = +0x58, g = +0x5c; doubles V = +0x1e0, A = +0x1c8 (step), T = +0x1d0 (target), +0x210; the volume __ftol(st * 127.0)
// goes to DsBuffer::setVolume (thunk 0x18f7) on +0x54.
void stream_fade(Ctx &c, u32 ch, int neg) {
  Unit &u = x87::unit(c);
  const u32 fl = ld<u32>(c, ch + 0x58);
  if (!(fl & 8) && !(ld<uint8_t>(c, ch + 0x5c) & 0x10)) return;
  const u32 g = ld<u32>(c, ch + 0x5c);
  auto m64 = [&](u32 off) { return x87::from_f64(ld64(c, ch + off)); };
  auto fld = [&](u32 off) { u.push(x87::fld64(ld64(c, ch + off))); };
  auto fcomp = [&](X80 b) {
    u.set_cc(x87::fcom(u.st(0), b));
    u.pop();
  };
  if (g & 2) {
    const u32 q = ld<u32>(c, ch + 0x13a4);
    st<u32>(c, ch + 0x58, fl | 0x10);
    if (q) st<u32>(c, q, 0);
    return;
  }
  if (g & 8) {
    fld(0x1e0);
    fcomp(m64(0x210));
    if (!u.cc(x87::kC0)) {
      st<u32>(c, ch + 0x58, fl & ~8u);
      st<u32>(c, ch + 0x5c, g & ~8u);
    }
    return;
  }
  if (g & 0x10) {
    fld(0x1c8);                                                     // A
    u.set_cc(x87::fcom(u.st(0), x87::from_f64(rdata64(c, kKZero))));   // fcom 0.0 (A stays)
    if (!(u.sw & (x87::kC0 | x87::kC3))) {                          // A > 0.0
      u.pop();                                                      // fstp st(0)
      fld(0x1e0);
      fcomp(m64(0x1d0));
      if (u.sw & (x87::kC0 | x87::kC3)) {                           // V <= T or unordered
        st<u32>(c, ch + 0x5c, g & ~0x10u);
        return;
      }
      fld(0x1e0);
      fcomp(m64(0x1c8));
      if (!u.cc(x87::kC0)) {                                        // V >= A
        fld(0x1e0);
        u.st(0) = x87::sub(u.st(0), m64(0x1c8), u.cw);
        st<uint64_t>(c, ch + 0x1e0, x87::fst64(u.pop(), u.cw));
        fld(0x1e0);
      } else {
        st<u32>(c, ch + 0x1e0, ld<u32>(c, ch + 0x1d0));
        st<u32>(c, ch + 0x1e4, ld<u32>(c, ch + 0x1d4));
        fld(0x1e0);
      }
    } else {                                                        // A <= 0.0 or unordered: A stays in st(0)
      fld(0x1e0);
      fcomp(m64(0x1d0));
      if (!u.cc(x87::kC0)) {                                        // V >= T
        u.pop();                                                    // fstp st(0)
        st<u32>(c, ch + 0x5c, g & ~0x10u);
        return;
      }
      fld(0x1d0);
      u.st(0) = x87::sub(u.st(0), m64(0x1e0), u.cw);                // T - V
      std::swap(u.st(0), u.st(1));                                  // fxch st(1)
      u.st(0) = x87::fabs_(u.st(0));                                // fabs
      u.set_cc(x87::fcom(u.st(0), u.st(1)));                        // fcompp
      u.pop();
      u.pop();
      if (!u.cc(x87::kC0)) {                                        // |A| >= T - V
        st<u32>(c, ch + 0x1e0, ld<u32>(c, ch + 0x1d0));
        st<u32>(c, ch + 0x1e4, ld<u32>(c, ch + 0x1d4));
        fld(0x1e0);
      } else {
        fld(0x1e0);
        u.st(0) = x87::sub(u.st(0), m64(0x1c8), u.cw);
        st<uint64_t>(c, ch + 0x1e0, x87::fst64(u.pop(), u.cw));
        fld(0x1e0);
      }
    }
  } else {
    fld(0x1e0);
    fcomp(x87::from_f64(rdata64(c, kKZero)));
    if (u.cc(x87::kC3)) {                                           // V == 0 or unordered
      const u32 q = ld<u32>(c, ch + 0x13a4);
      st<u32>(c, ch + 0x58, fl | 0x10);
      if (q) st<u32>(c, q, 0);
    }
    fld(0x1e0);
    fcomp(m64(0x1c8));
    if (u.sw & (x87::kC0 | x87::kC3)) {                             // V <= A or unordered
      const u32 g2 = ld<u32>(c, ch + 0x5c);
      st<u32>(c, ch + 0x1c8, ld<u32>(c, ch + 0x1e0));
      st<u32>(c, ch + 0x5c, g2 | 2);
      st<u32>(c, ch + 0x1cc, ld<u32>(c, ch + 0x1e4));
    }
    fld(0x1e0);
    u.st(0) = x87::sub(u.st(0), m64(0x1c8), u.cw);
    st<uint64_t>(c, ch + 0x1e0, x87::fst64(u.st(0), u.cw));        // fst: st(0) keeps the unrounded value
  }
  u.st(0) = x87::mul(u.st(0), x87::from_f64(rdata64(c, kK127)), u.cw);   // fmul qword 127.0
  u32 vol = snd_call(c, kSndFtolRva, SndAbi::Cdecl, {}).eax;             // __ftol (pops st(0))
  if (neg == 1) vol += 1;                                                // negative control 1: the volume off by one
  snd_call(c, 0x18f7, SndAbi::Thiscall, {ld<u32>(c, ch + 0x54), vol});
}

// renderChannelStreamed 0x36660: thiscall (a1, a2, out), `ret 0xc`; a1 / a2 are never read. Returns 0x13 when the DsBuffer +0x54
// is NULL, DsBuffer::getPosition's result (thunk 0x233d) when it is neither 0 nor 0x2d, else 0xb. The copy loop moves the
// wanted bytes from the source ring (ch+8: +0x14 size, +0x18 start, +0x1c read, +0x20 limit, +0x24 end) into the two segments
// DsBuffer::restore (thunk 0x213f) locked, chaining into the successor's stream at +0x13a0 (its mmio stream when its byte +0xcc
// has bit 2, closeMmioSound thunk 0x1b18 at the limit; else its ring X+0x78); DsBuffer::unlock (thunk 0x1ec9); the fade; then
// the unchained end or the hand-over to the successor (addToSoundList thunk 0x23f1 on g_soundEngine, postSoundThreadMessage
// thunk 0x11d6). The c3fp hook (shim/src/re/c3fp.cpp) cites every instruction; the sub-object of the successor X is X + 0x70.
int g_negctl_renderChannelStreamed = 0;
u32 renderChannelStreamed(Ctx &c, u32 ch, u32 out) {
  const int neg = SG_NEGCTL(g_negctl_renderChannelStreamed);
  uint8_t chained = 0;
  SndFrame fr(c, 0x3c);           // sub esp, 0x2c; push ebx; push ebp; push esi; push edi (0x36660..0x3666a)
  const u32 pWant = fr.at(0x24), pOff = fr.at(8);                                  // 0x366b6..0x366c0
  const u32 pP1 = fr.at(0xc), pN1 = fr.at(0x10), pP2 = fr.at(0x14), pN2 = fr.at(0x18);  // 0x36704..0x3671d
  for (u32 p : {pWant, pOff, pN2, pP2, pN1, pP1}) st<u32>(c, p, 0);                // 0x36675..0x36689
  u32 src, lim, q, rem, dst, room, n, n3 = 0, X = 0, sub;
  u32 ds = ld<u32>(c, ch + 0x54);
  if (!ds) return 0x13;
  const u32 obj = ld<u32>(c, ch + 8);
  src = ld<u32>(c, obj + 0x1c);
  lim = src <= ld<u32>(c, obj + 0x20) ? ld<u32>(c, obj + 0x20) : ld<u32>(c, obj + 0x24);
  const u32 r = snd_call(c, 0x233d, SndAbi::Thiscall, {ds, pWant, pOff, 0}).eax;
  if (r != 0 && r != 0x2d) return r;
  ds = ld<u32>(c, ch + 0x54);
  {
    const uint16_t align = ld<uint16_t>(c, ds + 0xb4);
    q = align == 0 ? 0 : ld<u32>(c, pWant) / align;
  }
  snd_call(c, 0x213f, SndAbi::Thiscall, {ds, ld<u32>(c, pOff), ld<u32>(c, pWant), pP1, pN1, pP2, pN2});
  const u32 p2 = ld<u32>(c, pP2), n2 = ld<u32>(c, pN2);
  rem = ld<u32>(c, pWant);
  dst = ld<u32>(c, pP1);
  room = ld<u32>(c, pN1);
  auto subOf = [&](u32 x) -> u32 {
    if (x == 0) throw Fault{0, 0};   // the base-class conversion's NULL side follows a dereference of X (faults there too)
    return x + 0x70;
  };
  if (rem != 0) {
    do {
      n = rem;
      if (!(src + n <= lim)) {
        const u32 fl = ld<u32>(c, ch + 0x58);
        n = lim - src;
        bool mark;
        if (!(fl & 4) && (fl & 0x20)) mark = true;
        else mark = ld<u32>(c, ch + 0x13a0) != 0 && (fl & 0x20) != 0;
        if (mark) {
          const u32 o = ld<u32>(c, ch + 8);
          if (src <= ld<u32>(c, o + 0x20)) st<u32>(c, ch + 0x58, fl | 0x10);
          else st<u32>(c, ch + 0x5c, ld<u32>(c, ch + 0x5c) | 1u);
        }
      }
      if (n == 0) continue;
      for (;;) {
        if (room == 0 && n2 != 0) {
          room = n2;
          dst = p2;
        }
        if (n <= room) break;
        move_bytes(c, dst, src, room);
        n -= room;
        src += room;
        rem -= room;
        room = 0;
        if (n == 0) goto next;
      }
      {
        bool toSucc = false;
        if (!(n >= room)) {
          const u32 fl = ld<u32>(c, ch + 0x58);
          toSucc = (fl & 0x10) != 0 && ld<u32>(c, ch + 0x13a0) != 0 && (fl & 0x20) != 0;
        }
        if (toSucc) {
          move_bytes(c, dst, src, n);
          room -= n;
          dst += n;
          rem -= n;
          X = ld<u32>(c, ld<u32>(c, ch + 0x13a0));
          if (ld<uint8_t>(c, X + 0xcc) & 4) {
            sub = subOf(X);
            src = ld<u32>(c, ld<u32>(c, sub + 0x1390) + 0x1c);
            sub = subOf(X);
            lim = ld<u32>(c, ld<u32>(c, sub + 0x1390) + 0x20);
            if (room + src <= lim) {
              n3 = room;
            } else {
              sub = subOf(X);
              n3 = ld<u32>(c, ld<u32>(c, sub + 0x1390) + 0x20) - src;
            }
          } else {
            const u32 o2 = ld<u32>(c, X + 0x78);
            src = ld<u32>(c, o2 + 0x1c);
            lim = ld<u32>(c, o2 + 0x1c) <= ld<u32>(c, o2 + 0x20) ? ld<u32>(c, o2 + 0x20) : ld<u32>(c, o2 + 0x24);
            n3 = room;
          }
          move_bytes(c, dst, src, n3);
          room -= n3;
          rem -= n3;
          dst += n3;
          src += room;                                                // the bytes LEFT
          X = ld<u32>(c, ld<u32>(c, ch + 0x13a0));
          if (ld<uint8_t>(c, X + 0xcc) & 4) {
            if (!(src < lim)) {
              sub = subOf(X);
              snd_call(c, 0x1b18, SndAbi::Thiscall, {sub});
              X = ld<u32>(c, ld<u32>(c, ch + 0x13a0));
              sub = subOf(X);
              src = ld<u32>(c, ld<u32>(c, sub + 0x1390) + 0x1c);
              sub = subOf(X);
              chained = 1;
              lim = ld<u32>(c, ld<u32>(c, sub + 0x1390) + 0x20);
            } else {
              sub = subOf(X);
              chained = 1;
              st<u32>(c, ld<u32>(c, sub + 0x1390) + 0x1c, src);
            }
          } else {
            sub = subOf(X);
            chained = 1;
            const u32 o8 = ld<u32>(c, sub + 8);
            st<u32>(c, o8 + 0x1c, ld<u32>(c, o8 + 0x1c) + n3);
          }
          goto next;
        }
      }
      move_bytes(c, dst, src, n);
      room -= n;
      dst += n;
      rem -= n;
      src += n;
      {
        const u32 g = ld<u32>(c, ch + 0x5c);
        if (g & 1) {
          src = ld<u32>(c, ld<u32>(c, ch + 8) + 0x18);
          st<u32>(c, ch + 0x5c, g & ~1u);
        } else {
          const u32 o = ld<u32>(c, ch + 8);
          if (src == ld<u32>(c, o + 0x20) && !(ld<uint8_t>(c, ch + 0x58) & 0x10)) {
            src = ld<u32>(c, o + 0x18);
          } else if ((ld<uint8_t>(c, ch + 0x58) & 0x10) && room != 0) {
            zero_bytes(c, dst, room);                                 // dst not advanced
            rem -= room;
            room = 0;
          }
        }
      }
    next:;
    } while (rem != 0);
  }
  snd_call(c, 0x1ec9, SndAbi::Thiscall, {ld<u32>(c, ch + 0x54), ld<u32>(c, pP1), ld<u32>(c, pN1), p2, n2});
  stream_fade(c, ch, neg);
  if (chained == 0) {
    st<u32>(c, ld<u32>(c, ch + 8) + 0x1c, src);
    st<u32>(c, out, q);
    const u32 f = ld<u32>(c, ch + 0x58);
    if ((f & 2) && !(f & 0x21) && !(f & 0x400)) {
      const u32 o = ld<u32>(c, ch + 8);
      const u32 d = (ld<u32>(c, o + 0x1c) - ld<u32>(c, ch + 0x1378)) & ld<u32>(c, ch + 0x1374);
      if (!(d <= (ld<u32>(c, o + 0x14) >> 2))) {
        const u32 cnt = ld<u32>(c, ch + 0x1370);
        st<u32>(c, ch + 0x58, f | 0x400);
        st<u32>(c, ch + 0x1370, cnt + 1);
        snd_call(c, 0x11d6, SndAbi::Thiscall, {snd_va(c, kSigObj), 0x7eb, ch, 0});
      }
    }
    return 0xb;
  }
  auto succ = [&]() {
    X = ld<u32>(c, ld<u32>(c, ch + 0x13a0));
    return subOf(X);
  };
  sub = succ();
  st<u32>(c, sub + 0x1e0, ld<u32>(c, ch + 0x1e0));
  st<u32>(c, sub + 0x1e4, ld<u32>(c, ch + 0x1e4));
  sub = succ();
  st<u32>(c, sub + 0x1c8, ld<u32>(c, ch + 0x1c8));
  st<u32>(c, sub + 0x1cc, ld<u32>(c, ch + 0x1cc));
  sub = succ();
  st<u32>(c, sub + 0x1d0, ld<u32>(c, ch + 0x1d0));
  st<u32>(c, sub + 0x1d4, ld<u32>(c, ch + 0x1d4));
  for (u32 off : {0x1d8u, 0x208u, 0x1f8u, 0x1e8u, 0x218u, 0x21cu}) {
    sub = succ();
    st<u32>(c, sub + off, ld<u32>(c, ch + off));
  }
  sub = succ();
  if (!(ld<uint8_t>(c, sub + 0x5c) & 4)) {
    sub = subOf(X);
    const u32 f = ld<u32>(c, sub + 0x58);
    if ((f & 2) && !(f & 0x21) && !(f & 0x400)) {
      const u32 o2 = ld<u32>(c, sub + 8);
      const u32 d = (ld<u32>(c, o2 + 0x1c) - ld<u32>(c, sub + 0x1378)) & ld<u32>(c, sub + 0x1374);
      if (!(d <= (ld<u32>(c, o2 + 0x14) >> 2))) {
        st<u32>(c, sub + 0x58, f | 0x400);
        u32 P = ld<u32>(c, ch + 0x13a0);
        st<u32>(c, P + 0x58, ld<u32>(c, P + 0x58) | 0x400);
        P = ld<u32>(c, ch + 0x13a0);
        st<u32>(c, P + 0x1370, ld<u32>(c, P + 0x1370) + 1);
        P = ld<u32>(c, ch + 0x13a0);
        snd_call(c, 0x11d6, SndAbi::Thiscall, {snd_va(c, kSigObj), 0x7eb, ld<u32>(c, P), 0});
      }
    }
  }
  snd_call(c, 0x23f1, SndAbi::Thiscall,
           {snd_obj(c, &SoundState::g_soundEngine).get(), ld<u32>(c, ld<u32>(c, ch + 0x13a0))});
  sub = succ();
  st<u32>(c, sub + 0x54, ld<u32>(c, ch + 0x54));
  sub = succ();
  st<u32>(c, sub + 0x58, ld<u32>(c, sub + 0x58) | 0x10000000u);
  {
    u32 P = ld<u32>(c, ch + 0x13a0);
    const u32 Q = ld<u32>(c, ch + 0x13a4);
    st<u32>(c, ch + 0x54, 0);
    st<u32>(c, Q, ld<u32>(c, P));
    P = ld<u32>(c, ch + 0x13a0);
    st<u32>(c, P, 0);
  }
  {
    const u32 g = ld<u32>(c, ch + 0x5c), f = ld<u32>(c, ch + 0x58);
    st<u32>(c, ch + 0x5c, g | 0x80);
    st<u32>(c, ch + 0x58, f | 0x10);
  }
  for (u32 i = 0; i < 16; ++i) {
    const uint8_t b = ld<uint8_t>(c, ch + 0x2d4 + 0x110 * i);
    st<uint8_t>(c, ch + 0x2d4 + 0x110 * i, static_cast<uint8_t>((b & 0xfd) | 4));
  }
  st<u32>(c, ch + 0x13a0, 0);
  st<u32>(c, ch + 0x13a4, 0);
  return 0xb;
}

// renderChannelVoice 0x371d0: thiscall (voice, buf, count, pass, out), `ret 0x14`, returns 0 (the c3fl hook cites every
// instruction). pos = voice+0x1c in a frame local the mixer advances. Byte +0x9c bit 2 clear: voice+0xc0 += ch+0x230; a start
// +0xc4 above that sum stores count in *out, clears count * 16 bytes of buf when pass >= 0xc, and returns. Otherwise one mixer
// (buf, &pos, count, src, flag) by format (with pass >= 0xc: the word +0x182 = 2 Voice_mixSetStereo16 thunk 0x10dc, +0x18e = 8
// Voice_mixSet8bit 0x2220, both with (ch, 1), else Voice_mixSetMono16 0x1f64 with (voice, 0), whose result is `rendered`;
// without: Voice_mixAddStereo16 0x12ee, Voice_mixSet8bit, Voice_mixAddMono16 0x2617). The fade and the rate reset run on the
// unit (renderChannelVoice+0x1d4..+0x3a9, below), Voice_setPanGains thunk 0x231a, __ftol through the dispatcher.
// Negative control 1: the rate reset's quotient is taken the other way round (A / B).
int g_negctl_renderChannelVoice = 0;
void voice_reset(Ctx &c, u32 ch, u32 v, uint8_t flags) {
  st<u32>(c, v + 0x88, 0);
  st<u32>(c, v + 0x1c, ld<u32>(c, v + 0x18));
  st<uint8_t>(c, v + 0x9c, flags & 0xe1);
  st<u32>(c, v + 0xf0, ld<u32>(c, v + 0xf8));
  st<u32>(c, v + 0xf4, ld<u32>(c, v + 0xfc));
  st<u32>(c, v + 0x8c, 0);
  st<u32>(c, v + 0xc0, 0);
  st<uint8_t>(c, v + 0x9c, static_cast<uint8_t>((flags & 0xe1) | 4));
  Unit &u = x87::unit(c);
  u.push(x87::fld64(ld64(c, ch + 0x1e0)));                       // fld qword ch+0x1e0
  st<uint64_t>(c, v + 0x58, x87::fst64(u.pop(), u.cw));         // fstp qword v+0x58
}
// the rate reset (renderChannelVoice+0x1eb..+0x250 / +0x2fe..+0x363)
void rate_reset(Ctx &c, u32 ch, u32 v, int neg) {
  const u32 eng = snd_obj(c, &SoundState::g_soundEngine).get();
  const u32 ebx = ld<u32>(c, ch + 0x1c0);
  if (!eng) return;
  const u32 rate = snd_vcall(c, eng, 0x40, SndAbi::Thiscall, {}).eax;
  Unit &u = x87::unit(c);
  u.push(x87::fild(static_cast<int64_t>(rate)));                 // fild qword (rate, 0): A
  u.push(x87::fild(static_cast<int64_t>(ebx)));                  // fild qword (ch+0x1c0, 0): B
  u.set_cc(x87::fcom(u.st(0), u.st(1)));                         // fcom st(1)
  if (!(u.sw & (x87::kC0 | x87::kC3))) {                         // B > A
    const X80 b = u.pop();
    u.st(0) = neg == 1 ? x87::div(u.st(0), b, u.cw) : x87::div(b, u.st(0), u.cw);   // fdivrp st(1): q = B / A
    u.push(x87::fild(ld<int32_t>(c, v + 0x50)));                 // fild dword v+0x50
    u.st(0) = x87::div(u.st(0), u.st(1), u.cw);                  // fdiv st(1)
    const u32 e = snd_call(c, kSndFtolRva, SndAbi::Cdecl, {}).eax;   // __ftol
    u.st(0) = x87::div(x87::from_f64(ld64(c, v + 0x58)), u.st(0), u.cw);   // fdivr qword v+0x58
    st<u32>(c, v + 0xec, e);
  } else {
    const u32 e = ld<u32>(c, v + 0x50);
    u.pop();                                                     // fstp st(0)
    u.pop();                                                     // fstp st(0)
    u.push(x87::fld64(ld64(c, v + 0x58)));
    st<u32>(c, v + 0xec, e);
  }
  st<uint64_t>(c, v + 0xf0, x87::fst64(u.st(0), u.cw));         // fst qword v+0xf0
  st<uint64_t>(c, v + 0xf8, x87::fst64(u.pop(), u.cw));         // fstp qword v+0xf8
}
u32 renderChannelVoice(Ctx &c, u32 ch, u32 v, u32 buf, u32 count, u32 pass, u32 out) {
  const int neg = SG_NEGCTL(g_negctl_renderChannelVoice);
  SndFrame fr(c, 0);
  const u32 aligned = (fr.entry() - 4u) & ~7u;    // ebp = entry - 4; and esp, -8 (0x371d3)
  fr.set_esp(aligned - 0x28u);                    // sub esp, 0x1c; push ebx; push esi; push edi (0x371d6..0x371e0)
  const u32 cursor = aligned - 0x18u;             // [esp + 0x10] (0x371ea)
  st<u32>(c, cursor, ld<u32>(c, v + 0x1c));
  u32 rendered = 0;
  const bool big = pass >= 0xc;
  if (!(ld<uint8_t>(c, v + 0x9c) & 4)) {
    const u32 sum = ld<u32>(c, v + 0xc0) + ld<u32>(c, ch + 0x230);
    st<u32>(c, v + 0xc0, sum);
    if (ld<u32>(c, v + 0xc4) > sum) {
      st<u32>(c, out, count);
      if (big) zero_bytes(c, buf, count << 4);
      return 0;
    }
    const bool st2 = ld<uint16_t>(c, ch + 0x182) == 2, b8 = ld<uint16_t>(c, ch + 0x18e) == 8;
    if (st2 || b8) {
      const u32 rva = st2 ? (big ? 0x10dc : 0x12ee) : 0x2220;
      st<u32>(c, out, snd_call(c, rva, SndAbi::Cdecl, {buf, cursor, count, ch, 1}).eax);
    } else {
      rendered = snd_call(c, big ? 0x1f64 : 0x2617, SndAbi::Cdecl, {buf, cursor, count, v, 0}).eax;
    }
    st<u32>(c, out, rendered);
    st<u32>(c, v + 0x1c, ld<u32>(c, cursor));
  }
  const uint8_t fl = ld<uint8_t>(c, v + 0x9c);
  if (fl & 8) {
    Unit &u = x87::unit(c);
    if (fl & 0x10) {
      voice_reset(c, ch, v, fl);
      snd_call(c, 0x231a, SndAbi::Thiscall, {v, ld<u32>(c, ch + 0x224)});
      rate_reset(c, ch, v, neg);
      st<u32>(c, v + 0xe4, 0);
      st<uint8_t>(c, v + 0x94, ld<uint8_t>(c, v + 0x94) & 0xfe);
    } else {
      u.push(x87::fld64(ld64(c, v + 0x58)));                                // fld qword v+0x58
      u.st(0) = x87::sub(u.st(0), x87::from_f64(ld64(c, v + 0xf0)), u.cw);  // fsub qword v+0xf0
      st<uint64_t>(c, v + 0x58, x87::fst64(u.st(0), u.cw));                // fst qword v+0x58
      if (fl & 0x20) {
        u.set_cc(x87::fcom(u.st(0), x87::from_f64(ld64(c, v + 0x60))));   // fcomp qword v+0x60
        u.pop();
        if (!u.cc(x87::kC0)) st<uint8_t>(c, v + 0x9c, fl & 0xd7);
      } else {
        u.set_cc(x87::fcom(u.st(0), x87::from_f64(rdata64(c, kKZero))));    // fcom qword 0.0
        if (u.sw & (x87::kC0 | x87::kC3)) {                                 // x <= 0.0 or unordered
          st<u32>(c, v + 0x88, 0);
          st<u32>(c, v + 0x1c, ld<u32>(c, v + 0x18));
          st<uint8_t>(c, v + 0x9c, fl & 0xe1);
          st<u32>(c, v + 0xf0, ld<u32>(c, v + 0xf8));
          u.pop();                                                          // fstp st(0)
          st<u32>(c, v + 0x8c, 0);
          st<u32>(c, v + 0xc0, 0);
          st<u32>(c, v + 0xf4, ld<u32>(c, v + 0xfc));
          st<uint8_t>(c, v + 0x9c, static_cast<uint8_t>((fl & 0xe1) | 4));
          u.push(x87::fld64(ld64(c, ch + 0x1e0)));
          st<uint64_t>(c, v + 0x58, x87::fst64(u.pop(), u.cw));
          rate_reset(c, ch, v, neg);
          st<u32>(c, v + 0xe4, 0);
          st<uint8_t>(c, v + 0x9c, ld<uint8_t>(c, v + 0x9c) & 0xf7);
        } else {
          u.set_cc(x87::fcom(u.st(0), x87::from_f64(ld64(c, v + 0xf0))));  // fcom qword v+0xf0
          if (u.cc(x87::kC0)) {
            st<uint64_t>(c, v + 0xf0, x87::fst64(u.pop(), u.cw));          // fstp qword v+0xf0
            st<uint8_t>(c, v + 0x9c, fl | 0x10);
          } else {
            u.pop();                                                        // fstp st(0)
          }
        }
      }
      snd_call(c, 0x231a, SndAbi::Thiscall, {v, ld<u32>(c, ch + 0x224)});
    }
  }
  if (rendered == count) return 0;
  if (big && !(ld<uint8_t>(c, ch + 0x5c) & 4)) zero_bytes(c, buf + (rendered << 4), (count - rendered) << 4);
  st<u32>(c, v + 0x88, 0);
  st<u32>(c, v + 0x8c, 0);
  st<u32>(c, v + 0xc0, 0);
  const uint8_t d9c = ld<uint8_t>(c, v + 0x9c) & 0xf1;
  st<u32>(c, v + 0x1c, ld<u32>(c, v + 0x18));
  st<uint8_t>(c, v + 0x9c, d9c);
  st<u32>(c, v + 0xf0, ld<u32>(c, v + 0xf8));
  st<u32>(c, v + 0xf4, ld<u32>(c, v + 0xfc));
  st<uint8_t>(c, v + 0x9c, static_cast<uint8_t>(d9c | 4));
  return 0;
}

void a_renderChannel(Ctx &c, const uint32_t *a, Ret &r) { SG_X87_GUARD(c, r.set_u32(renderChannel(c, a[0], a[1], a[2], a[3], a[4]))) }
void a_renderChannelSingle(Ctx &c, const uint32_t *a, Ret &r) {
  SG_X87_GUARD(c, r.set_u32(renderChannelSingle(c, a[0], a[1], a[2], a[3], a[4])))
}
void a_renderChannelStreamed(Ctx &c, const uint32_t *a, Ret &r) {
  SG_X87_GUARD(c, r.set_u32(renderChannelStreamed(c, a[0], a[3])))
}
void a_renderChannelVoice(Ctx &c, const uint32_t *a, Ret &r) {
  SG_X87_GUARD(c, r.set_u32(renderChannelVoice(c, a[0], a[1], a[2], a[3], a[4], a[5])))
}

// >>> S4.4 bodies
const FuncDef kFuncs[] = {
#define F(name, abi, ret, n) {"snd." #name, abi, ret, n, 0, a_##name, &g_negctl_##name},
    // A. the x87 resamplers (__cdecl out, posp, n, rec, mode)
    F(Voice_mixAddMono16, "default", "uint32", 5)
    F(Voice_mixSetMono16, "default", "uint32", 5)
    F(Voice_mixSet8bit, "default", "uint32", 5)
    F(Voice_mixAdd8bit, "default", "uint32", 5)
    F(Voice_mixSetStereo16, "default", "uint32", 5)
    F(Voice_mixAddStereo16, "default", "uint32", 5)
    F(streamRenderBlock, "default", "uint32", 5)
    F(streamRenderTail, "default", "uint32", 5)
    // B. the integer mixers (__cdecl ppOut, ppIn, count, voice, mode)
    F(mixStereoAdd, "default", "uint32", 5)
    F(mixStereoGainAdd, "default", "uint32", 5)
    F(mixRatedStereo16Add, "default", "uint32", 5)
    F(mixMonoDupAdd, "default", "uint32", 5)
    F(mixMonoGainFadeAdd, "default", "uint32", 5)
    F(mixRatedMono16AddFade, "default", "uint32", 5)
    F(mixRatedStereo8Add, "default", "uint32", 5)
    F(mixRatedMono8Add, "default", "uint32", 5)
    F(mixRatedStereo16Set, "default", "uint32", 5)
    F(mixMonoDupSet, "default", "uint32", 5)
    F(mixMonoGainFadeSet, "default", "uint32", 5)
    F(mixRatedMono16SetFade, "default", "uint32", 5)
    F(mixRatedStereo8Set, "default", "uint32", 5)
    F(mixRatedMono8Set, "default", "uint32", 5)
    // C. the MMX renderers (__cdecl ppOut, ppIn, count, voice, mode)
    F(mixRenderRatedC, "default", "uint32", 5)
    F(Voice_renderStream32, "default", "uint32", 5)
    F(Voice_renderStream8c, "default", "uint32", 5)
    F(Voice_renderStream8d, "default", "uint32", 5)
    F(Voice_renderStream16b, "default", "uint32", 5)
    F(Voice_renderStream16c, "default", "uint32", 5)
    // D. the mixer objects (thiscall)
    F(MixerVoiceArray_scalarDtor, "thiscall", "pointer", 2)
    F(destroyMixerVoices, "thiscall", "void", 1)
    F(Mixer_ctor, "thiscall", "pointer", 1)
    F(Mixer_scalarDtor, "thiscall", "pointer", 2)
    F(Mixer_dtor, "thiscall", "void", 1)
    F(MixBuffer_ctor, "thiscall", "pointer", 1)
    // E. the channel renderers (thiscall)
    F(renderChannel, "thiscall", "uint32", 5)
    F(renderChannelSingle, "thiscall", "uint32", 5)
    F(renderChannelStreamed, "thiscall", "uint32", 4)
    F(renderChannelVoice, "thiscall", "uint32", 6)
#undef F
};

}  // namespace

extern const ModuleDef kModule_audio_mixer = {"audio_mixer", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim

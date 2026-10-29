// Module audio_codec (step S4.3, log/roadmap/S4/3_notes.md): sound.dll's LPC-10 2400 bps speech codec (rva 0x3a900..0x41a00,
// a debug /Od /GZ C build in f2c shape: every scalar argument is passed by address, every array one-based through a pointer
// moved back by one element). 42 functions, written against portsim/sim/audio.h.
//
// HOW THE BODIES WERE MADE. Each body is the C3 reimplementation of its hook source (shim/src/re/c3az, c3au, c3bd, c3bi, c3bn,
// c3br, c3bv, c3ca, c3dh .cpp; every one GREEN against the original at C3 and C4 by match), carried over by
// log/roadmap/S4/3_xlat.py and edited by hand. The hooks used host pointers into the live process; here a pointer is a
// virtual address of the context's 32-bit space, wrapped in P<T> (pointer arithmetic in elements of T) whose elements are
// R<T> (a load on read, a store on write), so the hook's expressions keep their shape and their evaluation order.
//
// FLOATING POINT (port/CORE.md section 7). The originals are x87 with every `fstp dword` spill visible; none loads a control
// word, so they run at the caller's word. All 57 judging keys ran at 0x027f (53-bit, nearest), where an x87 step that ends in
// a register equals one double operation and a step that ends in a dword slot is that double rounded once to float: the
// hooks' `static_cast<double>` operands and float assignments are that method (c3ap) and are kept verbatim. Win32 MSVC also
// compiles these doubles as SSE2, so x64 and Win32 compute the same; FuncDef.fpcw is 0x027f. What another caller word would
// change is U-1421.
//
// CALLS. Every call the original makes goes through the dispatcher (ccall: snd_call by rva, cdecl, every argument a
// virtual address or a value), so a fixture's stand-in or another step's port function is honoured. The CRT leaves the
// codec calls (__ftol, sqrt, floor, log, exp) are computed inline, as the accepted S4 steps inline __ftol (ftol_low32):
// U-1422 for log / exp.
//
// STACK LOCALS. The original hands its callees pointers into its own /GZ frame (filled with 0xcc), at fixed offsets below
// ebp, and two callees write outside the block they are given (c3bv's header). CodecFrame is the ONE helper for that: it
// reserves the original's frame (plus the saved ebp and the /GZ pushes) as an SndFrame of portsim/sim/audio.h (S4.2b: the
// entry esp a `call` leaves, from the harness or the dispatcher, else the core's frame block; out-of-block writes land in free
// stack below or in the caller's frame above, as in the original) and fills the frame with 0xcc as the prologue's `rep stosd`
// does. (Until S4.2b it was S4.8's SndLocals; audio_codec.cpp is no longer reproduced by 3_assemble.py.)
#include <cmath>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include "portsim/arith.h"
#include "portsim/ptr.h"
#include "portsim/registry.h"
#include "portsim/sim/audio.h"
#include "portsim/sim/audio_codec.h"

namespace portsim {
namespace {

using u32 = uint32_t;

#if defined(_MSC_VER)
#pragma warning(disable : 4100 4189 4505)
#endif

// ------------------------------------------------------------------------------------------------ the running call's context
thread_local Ctx *g_cx = nullptr;
struct CxScope {
  Ctx *prev;
  // the codec's float compares are C comparisons, not modelled on the x87 unit: its condition codes become unknown (U-1419)
  explicit CxScope(Ctx &c) : prev(g_cx) {
    g_cx = &c;
    x87::unit(c).cc_unmodelled();
  }
  ~CxScope() { g_cx = prev; }
  CxScope(const CxScope &) = delete;
  CxScope &operator=(const CxScope &) = delete;
};
inline Ctx &cx() { return *g_cx; }

// ------------------------------------------------------------------------------------------------ virtual pointers
template <class T> struct P;

// one element of type T at a virtual address
template <class T> struct R {
  static_assert(!std::is_const<T>::value, "R<const T>");
  u32 va;
  operator T() const { return ld<T>(cx(), va); }
  T get() const { return ld<T>(cx(), va); }
  const R &operator=(T v) const {
    st<T>(cx(), va, v);
    return *this;
  }
  const R &operator=(const R &o) const { return *this = o.get(); }
  template <class U, class = typename std::enable_if<std::is_arithmetic<U>::value>::type>
  const R &operator+=(U v) const { return *this = static_cast<T>(get() + v); }
  template <class U, class = typename std::enable_if<std::is_arithmetic<U>::value>::type>
  const R &operator-=(U v) const { return *this = static_cast<T>(get() - v); }
  template <class U, class = typename std::enable_if<std::is_arithmetic<U>::value>::type>
  const R &operator*=(U v) const { return *this = static_cast<T>(get() * v); }
  template <class U> const R &operator+=(const R<U> &v) const { return *this += v.get(); }
  template <class U> const R &operator-=(const R<U> &v) const { return *this -= v.get(); }
  template <class U> const R &operator*=(const R<U> &v) const { return *this *= v.get(); }
  const R &operator++() const { return *this = static_cast<T>(get() + 1); }
  const R &operator--() const { return *this = static_cast<T>(get() - 1); }
  T operator++(int) const { T o = get(); *this = static_cast<T>(o + 1); return o; }
  T operator--(int) const { T o = get(); *this = static_cast<T>(o - 1); return o; }
  P<T> operator&() const;
};

template <class T> struct P {
  static_assert(!std::is_const<T>::value, "P<const T>");
  u32 va = 0;
  P() = default;
  constexpr explicit P(u32 v) : va(v) {}
  P(std::nullptr_t) : va(0) {}
  static constexpr u32 sz() { return static_cast<u32>(sizeof(T)); }
  R<T> operator*() const { return R<T>{va}; }
  R<T> operator[](int32_t i) const { return R<T>{va + static_cast<u32>(i) * sz()}; }
  P operator+(int32_t n) const { return P(va + static_cast<u32>(n) * sz()); }
  P operator-(int32_t n) const { return P(va - static_cast<u32>(n) * sz()); }
  int32_t operator-(const P &o) const { return static_cast<int32_t>(va - o.va) / static_cast<int32_t>(sz()); }
  P &operator+=(int32_t n) { va += static_cast<u32>(n) * sz(); return *this; }
  P &operator-=(int32_t n) { va -= static_cast<u32>(n) * sz(); return *this; }
  P &operator++() { va += sz(); return *this; }
  P &operator--() { va -= sz(); return *this; }
  P operator++(int) { P o = *this; va += sz(); return o; }
  P operator--(int) { P o = *this; va -= sz(); return o; }
  explicit operator bool() const { return va != 0; }
  bool operator==(const P &o) const { return va == o.va; }
  bool operator!=(const P &o) const { return va != o.va; }
  bool operator==(std::nullptr_t) const { return va == 0; }
  bool operator!=(std::nullptr_t) const { return va != 0; }
  bool operator==(int z) const { return va == static_cast<u32>(z); }  // `p != 0` of the hooks
  bool operator!=(int z) const { return va != static_cast<u32>(z); }
  bool operator<(const P &o) const { return va < o.va; }
  bool operator<=(const P &o) const { return va <= o.va; }
  bool operator>(const P &o) const { return va > o.va; }
  bool operator>=(const P &o) const { return va >= o.va; }
};
template <class T> P<T> R<T>::operator&() const { return P<T>(va); }

// the hooks' reinterpret_cast between pointer types: same address, other element type
template <class T> struct Strip { using type = typename std::remove_cv<T>::type; };
template <> struct Strip<void> { using type = char; };
template <> struct Strip<const void> { using type = char; };
template <class T, class U> P<typename Strip<T>::type> pc(const P<U> &p) { return P<typename Strip<T>::type>(p.va); }
template <class T> P<typename Strip<T>::type> pc(u32 va) { return P<typename Strip<T>::type>(va); }
// on a host pointer (the hooks' bit puns of a local: *reinterpret_cast<const unsigned*>(&f)) it stays a host cast
template <class T, class U> T *pc(U *p) { return reinterpret_cast<T *>(p); }
template <class T, class U> const T *pc(const U *p) { return reinterpret_cast<const T *>(p); }

// a value or pointer as a 32-bit argument slot
inline u32 slot(u32 v) { return v; }
inline u32 slot(int32_t v) { return static_cast<u32>(v); }
template <class T> u32 slot(const P<T> &p) { return p.va; }

// the bits of a host value as another type (the hook's *reinterpret_cast<T*>(&x))
template <class T, class S> T pun(const S &s) {
  static_assert(sizeof(T) == sizeof(S), "pun");
  T t;
  std::memcpy(&t, &s, sizeof t);
  return t;
}

// ------------------------------------------------------------------------------------------------ module constants
// sound.dll's own constants, tables and globals are read where the original reads them (rva into the user's image)
inline P<char> sva(u32 rva) { return P<char>(snd_va(cx(), rva)); }
inline float rf(u32 rva) { return ld<float>(cx(), snd_va(cx(), rva)); }
inline double rd(u32 rva) { return ld<double>(cx(), snd_va(cx(), rva)); }
inline int32_t ri(u32 rva) { return ld<int32_t>(cx(), snd_va(cx(), rva)); }
inline P<int32_t> g_order() { return P<int32_t>(snd_obj(cx(), &SoundState::g_codecOrder).va()); }
inline P<int32_t> g_frameLen() { return P<int32_t>(snd_obj(cx(), &SoundState::g_codecFrameLen).va()); }
inline P<int32_t> g_errProtect() { return P<int32_t>(snd_obj(cx(), &SoundState::g_codecErrProtect).va()); }

// a raw dword copy: the originals move float slots with `mov`, which carries a NaN or a 0xcccccccc fill unchanged
template <class A, class B> void copy32(const P<A> &dst, const P<B> &src) {
  st<uint32_t>(cx(), dst.va, ld<uint32_t>(cx(), src.va));
}
template <class A, class B> void copy32(const P<A> &dst, const B *src) { st<uint32_t>(cx(), dst.va, pun<uint32_t>(*src)); }
template <class A, class B> void copy32(A *dst, const P<B> &src) {
  const uint32_t v = ld<uint32_t>(cx(), src.va);
  std::memcpy(dst, &v, 4);
}
template <class A, class B> void copy32(A *dst, const B *src) { std::memcpy(dst, src, 4); }
// c3bn's fldAddr(base, off): the byte address base + off
template <class B> P<char> fldAddr(const P<B> &base, int32_t off) { return P<char>(base.va + static_cast<u32>(off)); }

// the hooks' field accessors fld<T>(base, off) / fldc<T>(base, off): the element of type T at base + off bytes
template <class T, class B> R<typename Strip<T>::type> fld(const P<B> &base, int32_t off) {
  return R<typename Strip<T>::type>{base.va + static_cast<u32>(off)};
}
template <class T, class B> typename Strip<T>::type fldc(const P<B> &base, int32_t off) {
  return ld<typename Strip<T>::type>(cx(), base.va + static_cast<u32>(off));
}


// ------------------------------------------------------------------------------------------------ CRT leaves (inline)
// __ftol: 64-bit fistp toward zero, low dword kept
inline int32_t ftol(double v) { return static_cast<int32_t>(ftol_low32(v)); }
// sqrt (rva 0x43204): fsqrt under the caller's 53-bit word = the correctly rounded double square root
inline double crt_sqrt(double v) { return std::sqrt(v); }
// floor (rva 0x4339c): exact
inline double crt_floor(double v) { return std::floor(v); }
// log (rva 0x432e4: fldln2; fyl2x) and exp (rva 0x432b0): the double the caller stores (U-1422)
inline double crt_log(double v) { return std::log(v); }
inline double crt_exp(double v) { return std::exp(v); }
// the bits of a float
inline u32 fbits(float v) { return pun<u32>(v); }
// VC6's signed `x % 2^k`: `and x, 0x8000000f`, and when the sign survived `dec; or fill; inc`
inline int32_t srem(int32_t x, u32 mask, u32 fill) {
  int32_t v = static_cast<int32_t>(static_cast<u32>(x) & mask);
  if (v < 0) {
    --v;
    v = static_cast<int32_t>(static_cast<u32>(v) | fill);
    ++v;
  }
  return v;
}

// ------------------------------------------------------------------------------------------------ calls
// A cdecl callee by rva through the dispatcher. Its eax is the int result, its st(0) (want_st0) the float one.
template <class... A> int32_t ccall(u32 rva, const A &... a) {
  return static_cast<int32_t>(snd_call(cx(), rva, SndAbi::Cdecl, {slot(a)...}).eax);
}
// The callee leaves its result in st(0) of the x87 unit; every call site stores it with `fstp` (popped here).
template <class... A> double ccall_f(u32 rva, const A &... a) {
  const double v = snd_call(cx(), rva, SndAbi::Cdecl, {slot(a)...}, true).st0;
  x87::unit(cx()).pop();
  return v;
}

// ------------------------------------------------------------------------------------------------ the frame (ONE helper)
// The original's /GZ frame: `push ebp; mov ebp, esp; sub esp, frame; push ebx; push esi; push edi`, then `rep stosd` of
// 0xcccccccc over [ebp - frame, ebp). ebp is entry esp - 4.
class CodecFrame {
 public:
  CodecFrame(Ctx &c, u32 frame) : l_(c, frame + 4u + 12u) {
    ebp_ = l_.entry() - 4u;
    std::memset(c.mem.at(ebp_ - frame, frame, true), 0xcc, frame);
  }
  P<char> ebp() const { return P<char>(ebp_); }
  template <class T> P<T> loc(u32 off) const { return P<T>(ebp_ - off); }

 private:
  SndFrame l_;  // S4.2b: the core's stack (portsim/sim/audio.h)
  u32 ebp_ = 0;
};
template <class T> P<typename Strip<T>::type> loc(const P<char> &ebp, u32 off) { return P<typename Strip<T>::type>(ebp.va - off); }

// ------------------------------------------------------------------------------------------------ negative controls
#define NEG(n) g_negctl_##n
inline u32 neg_u32(int mode, u32 v) { return mode == 1 ? v + 1u : v; }
inline void neg_mem(Ctx &c, int mode, u32 va) {
  if (mode == 1) st<uint32_t>(c, va, ld<uint32_t>(c, va) ^ 1u);
}


// negative-control modes (0 = the correct body; mode 1 every function, mode 2 the spill controls)
int g_negctl_codecDecodeFrame = 0;
int g_negctl_codecSetParams = 0;
int g_negctl_codecAllocEncoderContext = 0;
int g_negctl_codecInitEncoderContext = 0;
int g_negctl_codecAllocDecoderContext = 0;
int g_negctl_codecInitDecoderContext = 0;
int g_negctl_codecEncodeFrame = 0;
int g_negctl_codecSynthesizeFrame = 0;
int g_negctl_codecDecodeParams = 0;
int g_negctl_bitPermute = 0;
int g_negctl_bitPack = 0;
int g_negctl_bitUnpack = 0;
int g_negctl_quantizeParams = 0;
int g_negctl_codecAnalyzeFrame = 0;
int g_negctl_codecHighPassFrame = 0;
int g_negctl_codecSynthPostFilter = 0;
int g_negctl_lpcSynthesize = 0;
int g_negctl_reflectionToLpc = 0;
int g_negctl_interpSubframeParams = 0;
int g_negctl_intPow = 0;
int g_negctl_floatCopySign = 0;
int g_negctl_floatRoundHalfAway = 0;
int g_negctl_medianOfThree = 0;
int g_negctl_codecCorrectNibble = 0;
int g_negctl_codecRevertUnstableCoeffs = 0;
int g_negctl_codecLdlSolveClamped = 0;
int g_negctl_codecCovarianceMatrix = 0;
int g_negctl_codecRmsEnergy = 0;
int g_negctl_codecRemoveMean = 0;
int g_negctl_codecAlignAnalysisWindow = 0;
int g_negctl_codecPitchTrackDp = 0;
int g_negctl_codecVoicingDecision = 0;
int g_negctl_codecRefinePitchLag = 0;
int g_negctl_codecInverseFilter2 = 0;
int g_negctl_codecLowPassFir31 = 0;
int g_negctl_codecSelectWindowFromJumps = 0;
int g_negctl_codecMarkCorrelationJumps = 0;
int g_negctl_codecPreEmphasis = 0;
int g_negctl_codecHighPassFilter = 0;
int g_negctl_codecRing5Add = 0;
int g_negctl_codecWindowFeatures = 0;
int g_negctl_codecAmdfPitch = 0;



// ---- codecDecodeFrame (rva 0x3a980): from c3bv.cpp CodecProcessBlock_re
// 0x0003a980  codecProcessBlock (VA 0x3a980)
// with three stack arguments (`ret` at 0x3aa40 pops nothing), a /Od /GZ debug body whose frame
// [ebp-0xac..ebp-1] is filled with 0xcccccccc (`rep stosd`, ecx = 0x2b, at 0x3a99c).
//   [ebp+8]    the packed parameter words, moved back by one dword at 0x3a9a7 when it is not null
//              (`je 0x3a9ad` at 0x3a9a2), so that the callee is handed the original pointer again by the
//              `add edx, 4` at 0x3a9bf: the pair is the one-based convention of the whole codec
//   [ebp+0xc]  the output samples, moved back by one dword at 0x3a9b6 (`je 0x3a9bc` at 0x3a9b1) and
//              handed on as `+ 4` at 0x3aa0f
//   [ebp+0x10] the decoder context: its first 0xa8 bytes are codecTransformStage's stage record and
//              +0xa8..+0xc08 is codecSynthStage's state (c3br, re/frida/registry.d/c3br.py)
// The body is three calls over eight locals of its own frame and nothing else (0x3a99e-0x3aa2b):
//   bitUnpack 0x3c1d0 (0x3a9d4) reads the packed words into the class cell [ebp-0x14], the level cell
//     [ebp-4] and the quantised coefficient array [ebp-0x64], under the count at the descriptor 0x63ef0
//     (its first dword is 10, the order);
//   codecTransformStage 0x3b5a0 (0x3a9fc) turns those three into the flag block [ebp-0xc], the sub-frame
//     length cell [ebp-0x10], the gain cell [ebp-0x6c] and the float coefficient array [ebp-0x3c], using the
//     context's first 0xa8 bytes as its history record;
//   codecSynthStage 0x3b230 (0x3aa23) synthesises from those four into the caller's output array and
//     writes the produced sample count to [ebp-0x68].
// The int coefficient array [ebp-0x64] and the float coefficient array [ebp-0x3c] are 0x28 bytes apart in the
// frame, which is exactly the ten dwords of one order-10 parameter set, so the two never overlap in use.
// Returns 0 (0x3aa2b).
int codecDecodeFrame_body(P<int32_t> packed, P<float> out, P<char> ctx) {
    const CodecFrame fr(cx(), 0xAC);  // the original's frame, 0xcc-filled
    const P<char> ebp = fr.ebp();

    if (packed != 0) packed -= 1;                                // je at 0x3a9a2, sub at 0x3a9a7
    if (out != 0) out -= 1;                                      // je at 0x3a9b1, sub at 0x3a9b6

    const auto bitUnpack = [](const auto &... a) { return ccall(0x0003C1D0, a...); };
    const auto transformStage = [](const auto &... a) { return ccall(0x0003B5A0, a...); };
    const auto synthStage = [](const auto &... a) { return ccall(0x0003B230, a...); };

    bitUnpack(sva(0x00063EF0),                            // push at 0x3a9cf
              loc<void>(ebp, 0x14), loc<void>(ebp, 0x04), loc<void>(ebp, 0x64),
              (packed + 1));                     // add edx, 4 at 0x3a9bf

    transformStage(loc<void>(ebp, 0x14), loc<void>(ebp, 0x04), loc<void>(ebp, 0x64),
                   loc<void>(ebp, 0x0C), loc<void>(ebp, 0x10), loc<void>(ebp, 0x6C),
                   loc<void>(ebp, 0x3C), ctx);              // call at 0x3a9fc

    synthStage(loc<void>(ebp, 0x0C), loc<void>(ebp, 0x10), loc<void>(ebp, 0x6C),
               loc<void>(ebp, 0x3C), out + 1,               // add ecx, 4 at 0x3aa0f
               loc<void>(ebp, 0x68), ctx);                  // call at 0x3aa23
    return 0;                                                    // xor eax, eax at 0x3aa2b
}
void a_codecDecodeFrame(Ctx &ctx, const uint32_t *a, Ret &r) {
  const CxScope s(ctx);
  r.set_i32(static_cast<int32_t>(neg_u32(NEG(codecDecodeFrame), static_cast<u32>(codecDecodeFrame_body(P<int32_t>(a[0]), P<float>(a[1]), P<char>(a[2]))))));
}

// ---- codecSetParams (rva 0x3aa50): from c3dh.cpp CodecSet_re
// 0x0003aa50  codecSetParams (VA 0x3aa50). cdecl, no arguments. Writes the constants 0xa, 0xb4 and 1 into the module words 0xb58a0,
// 0xb58a4 and 0xb58a8 (0x3aa68, 0x3aa72, 0x3aa7c) and returns 0 (0x3aa86).
unsigned codecSetParams_body() {
    *g_order() = 0xa;                // 0x3aa68
    *g_frameLen() = 0xb4;               // 0x3aa72
    *g_errProtect() = 1;                  // 0x3aa7c
    return 0;                                                                  // 0x3aa86
}
void a_codecSetParams(Ctx &ctx, const uint32_t *a, Ret &r) {
  const CxScope s(ctx);
  r.set_u32(neg_u32(NEG(codecSetParams), codecSetParams_body()));
}

// ---- codecAllocEncoderContext (rva 0x3aa90): from c3dh.cpp AllocCtxA_re
// 0x0003aa90  codecAllocContextA (VA 0x3aa90). cdecl, no arguments. malloc 0x42acd (the CRT's malloc: it pushes the new-handler mode and
// calls 0x42adf) is asked for 0x2544 bytes (0x3aaa8 / 0x3aaad); a non-NULL block is passed to codecInitContextA 0x3aae0
// (`push eax; call` 0x3aac1 / 0x3aac2) and the block pointer, NULL included, is returned (0x3aaca). The debug frame's esp
// check (0x3aad3 / 0x3aad5) is not part of the result.
unsigned codecAllocEncoderContext_body() {
    unsigned p = ccall(0x00042acd, 0x2544);                        // 0x3aaad
    if (p != 0) ccall(0x0003aae0, p);                             // 0x3aac2
    return p;                                                                  // 0x3aaca
}
void a_codecAllocEncoderContext(Ctx &ctx, const uint32_t *a, Ret &r) {
  const CxScope s(ctx);
  r.set_u32(neg_u32(NEG(codecAllocEncoderContext), codecAllocEncoderContext_body()));
}

// ---- codecInitEncoderContext (rva 0x3aae0): from c3ca.cpp CodecInitContextA_re
// 0x0003aae0  codecInitContextA (VA 0x3aae0)
// with one stack argument (`add esp, 0x48` + `ret` at 0x3ae8e-0x3ae9b pops nothing), a /Od /GZ
// debug body whose frame [ebp-0x48..ebp-1] is filled with 0xcccccccc (`rep stosd`, ecx = 0x12, at
// 0x3aaf6) and whose only live local is the loop counter [ebp-4] (and [ebp-8], the integer the one `fild`
// converts). eax at the `ret` holds the argument (0x3ae64), not a return value, so the entry declares
// ret="void" and the evidence is the context.
// It calls codecSetParams 0x3aa50 (0x3aaf8), which is the only writer in the whole DLL of the three
// codec globals and stores the constants 10, 0xb4 and 1 into them (0x3aa68, 0x3aa72, 0x3aa7c), and
// then writes the 0x2544-byte encoder context: the four sample histories and the five counted arrays are
// zeroed (0x3ab36, 0x3ab72, 0x3aba0, 0x3abe8, 0x3ac47, 0x3ac72, 0x3ac9d, 0x3acef,
// 0x3ad68, 0x3ae20, 0x3ae4b) and the seeded fields take the values below, each with the address of
// its store. The smoothed level +0x2228 is `(float)((*+0x2250 / *+0x2254) << 6)` read back from the two
// fields this body has just stored (`cdq` + `idiv` + `shl` + `fild` + `fstp dword` at 0x3adef-0x3ae08),
// so it is 3000 / 187 = 16 shifted by 6 = 1024.0f.
// Five regions of the context are NOT written by this body: +0x20b4..+0x20db, +0x20ec..+0x20fb,
// +0x2104..+0x2113, +0x21cc and +0x221c. The fixtures seed them differently per vector, which is what makes
// the final states of the fourteen vectors distinguishable.
void codecInitEncoderContext_body(P<char> ctx) {
    [](const auto &... a) { return ccall(0x0003AA50, a...); }();         // call at 0x3aaf8
    int i;
    fld<int>(ctx, 0x0000) = 0;                                   // 0x3ab00
    fld<int>(ctx, 0x0004) = 0;                                   // 0x3ab09
    fld<int>(ctx, 0x0008) = 0;                                   // 0x3ab13
    fld<int>(ctx, 0x000c) = 0;                                   // 0x3ab1d
    for (i = 0; i < 0x21c; ++i) {                                // jge at 0x3ab3d
        fld<int>(ctx, 0x0010 + i * 4) = 0;                       // 0x3ab45
        fld<int>(ctx, 0x0880 + i * 4) = 0;                       // 0x3ab53
    }
    for (i = 0; i < 0x2b8; ++i)                                  // jge at 0x3ab79
        fld<int>(ctx, 0x10f0 + i * 4) = 0;                       // 0x3ab81
    for (i = 0; i < 0x138; ++i)                                  // jge at 0x3aba7
        fld<int>(ctx, 0x1bd0 + i * 4) = 0;                       // 0x3abaf
    fld<int>(ctx, 0x20b0) = 0;                                   // 0x3abbf
    fld<int>(ctx, 0x20dc) = 1;                                   // 0x3abcc
    for (i = 0; i < 3; ++i)                                      // jge at 0x3abec
        fld<int>(ctx, 0x20e0 + i * 4) = 0;                       // 0x3abf4
    fld<int>(ctx, 0x20fc) = 0x133;                               // 0x3ac04
    fld<int>(ctx, 0x2100) = 0x1ce;                               // 0x3ac11
    fld<int>(ctx, 0x2114) = 0x133;                               // 0x3ac1e
    fld<int>(ctx, 0x2118) = 0x1ce;                               // 0x3ac2b
    for (i = 0; i < 8; ++i)                                      // jge at 0x3ac4b
        fld<int>(ctx, 0x211c + i * 4) = 0;                       // 0x3ac53
    for (i = 0; i < 3; ++i)                                      // jge at 0x3ac76
        fld<int>(ctx, 0x213c + i * 4) = 0;                       // 0x3ac7e
    for (i = 0; i < 0x1e; ++i)                                   // jge at 0x3aca1
        fld<int>(ctx, 0x2148 + i * 4) = 0;                       // 0x3aca9
    fld<int>(ctx, 0x21c0) = 0;                                   // 0x3acb9
    fld<int>(ctx, 0x21c4) = 0;                                   // 0x3acc6
    fld<int>(ctx, 0x21c8) = 0x3f800000;                          // 1.0f, 0x3acd3
    for (i = 0; i < 0x10; ++i)                                   // jge at 0x3acf3
        fld<int>(ctx, 0x21d0 + i * 4) = 0;                       // 0x3acfb
    fld<int>(ctx, 0x2210) = 0;                                   // 0x3ad0b
    fld<int>(ctx, 0x2214) = 1;                                   // 0x3ad18
    fld<int>(ctx, 0x2218) = 9;                                   // 0x3ad25
    fld<int>(ctx, 0x2220) = 0;                                   // 0x3ad32
    fld<int>(ctx, 0x2224) = 0x41a00000;                          // 20.0f, 0x3ad3f
    fld<int>(ctx, 0x222c) = 0;                                   // 0x3ad4c
    for (i = 0; i < 6; ++i)                                      // jge at 0x3ad6c
        fld<int>(ctx, 0x2230 + i * 4) = 0;                       // 0x3ad74
    fld<int>(ctx, 0x2248) = 0xbb8;                               // 0x3ad84
    fld<int>(ctx, 0x2250) = 0xbb8;                               // 0x3ad91
    fld<int>(ctx, 0x2254) = 0xbb;                                // 0x3ad9e
    fld<int>(ctx, 0x2258) = 0xbb;                                // 0x3adab
    fld<int>(ctx, 0x225c) = 0xbb;                                // 0x3adb8
    fld<int>(ctx, 0x224c) = 0x5d;                                // 0x3adc5
    fld<int>(ctx, 0x2260) = 0x5d;                                // 0x3add2
    fld<int>(ctx, 0x2264) = 0x5d;                                // 0x3addf
    const int ratio = shl32(div32(fld<int>(ctx, 0x2250), fld<int>(ctx, 0x2254)), 6);   // idiv + shl at 0x3adf6
    fld<float>(ctx, 0x2228) = static_cast<float>(ratio);         // fild + fstp dword at 0x3ae02
    for (i = 0; i < 0x3c; ++i)                                   // jge at 0x3ae24
        fld<int>(ctx, 0x2268 + i * 4) = 0;                       // 0x3ae2c
    for (i = 0; i < 0x78; ++i)                                   // jge at 0x3ae4f
        fld<int>(ctx, 0x2358 + i * 4) = 0;                       // 0x3ae57
    fld<int>(ctx, 0x2538) = 0;                                   // 0x3ae67
    fld<int>(ctx, 0x253c) = 0;                                   // 0x3ae74
    fld<int>(ctx, 0x2540) = 0;                                   // 0x3ae81
}
void a_codecInitEncoderContext(Ctx &ctx, const uint32_t *a, Ret &) {
  const CxScope s(ctx);
  codecInitEncoderContext_body(P<char>(a[0]));
  neg_mem(ctx, NEG(codecInitEncoderContext), a[0] + 0x0u);
}

// ---- codecAllocDecoderContext (rva 0x3aea0): from c3dh.cpp AllocCtxB_re
// 0x0003aea0  codecAllocContextB (VA 0x3aea0). The same with 0xc00 bytes (0x3aeb8 / 0x3aebd) and codecInitContextB 0x3aef0
// (0x3aed2).
unsigned codecAllocDecoderContext_body() {
    unsigned p = ccall(0x00042acd, 0xc00);                         // 0x3aebd
    if (p != 0) ccall(0x0003aef0, p);                             // 0x3aed2
    return p;                                                                  // 0x3aeda
}
void a_codecAllocDecoderContext(Ctx &ctx, const uint32_t *a, Ret &r) {
  const CxScope s(ctx);
  r.set_u32(neg_u32(NEG(codecAllocDecoderContext), codecAllocDecoderContext_body()));
}

// ---- codecInitDecoderContext (rva 0x3aef0): from c3ca.cpp CodecInitContextB_re
// 0x0003aef0  codecInitContextB (VA 0x3aef0)
// with one stack argument (`add esp, 0x44` + `ret` at 0x3b142-0x3b14f pops nothing), a /Od /GZ
// debug body whose frame [ebp-0x44..ebp-1] is filled with 0xcccccccc (`rep stosd`, ecx = 0x11, at
// 0x3af06) and whose only live local is the loop counter [ebp-4]. eax at the `ret` holds the argument
// (0x3b132), not a return value, so the entry declares ret="void".
// It calls the same codecSetParams 0x3aa50 (0x3af08) and then writes the 0xc00-byte decoder context,
// which is the object c3br mapped as codecTransformStage's stage record (0..0xa8) glued to codecSynthStage's
// state (+0xa8..+0xc00).
// Four regions are NOT written: +0x64c, +0x650, +0x658..+0x683 and +0xbea..+0xbeb. The fixtures seed them
// differently per vector, which is what makes the final states distinguishable.
void codecInitDecoderContext_body(P<char> ctx) {
    [](const auto &... a) { return ccall(0x0003AA50, a...); }();         // call at 0x3af08
    int i;
    fld<int>(ctx, 0x0000) = 0x3c;                                // 0x3af10
    fld<int>(ctx, 0x0004) = 1;                                   // 0x3af19
    fld<int>(ctx, 0x0008) = 0;                                   // 0x3af23
    fld<int>(ctx, 0x000c) = 0;                                   // 0x3af2d
    fld<int>(ctx, 0x0010) = 0x3c;                                // 0x3af37
    fld<int>(ctx, 0x0014) = 0;                                   // 0x3af41
    for (i = 0; i < 0x1e; ++i)                                   // jge at 0x3af5e
        fld<int>(ctx, 0x0018 + i * 4) = 0;                       // 0x3af66
    for (i = 0; i < 3; ++i) {                                    // jge at 0x3af86
        fld<int>(ctx, 0x0090 + i * 4) = 0;                       // 0x3af8e
        fld<int>(ctx, 0x009c + i * 4) = 0;                       // 0x3af9f
    }
    for (i = 0; i < 0x168; ++i)                                  // jge at 0x3afc5
        fld<int>(ctx, 0x00a8 + i * 4) = 0;                       // 0x3afcd
    fld<int>(ctx, 0x0648) = 0xb4;                                // 0x3afdd
    fld<int>(ctx, 0x0654) = 0x3f800000;                          // 1.0f, 0x3afea
    fld<int>(ctx, 0x0684) = 1;                                   // 0x3aff7
    fld<int>(ctx, 0x0688) = 0;                                   // 0x3b004
    for (i = 0; i < 0xa6; ++i) {                                 // jge at 0x3b027
        fld<int>(ctx, 0x068c + i * 4) = 0;                       // 0x3b02f
        fld<int>(ctx, 0x0924 + i * 4) = 0;                       // 0x3b040
    }
    fld<int>(ctx, 0x0bbc) = 0;                                   // 0x3b050
    fld<int>(ctx, 0x0bc0) = 0;                                   // 0x3b05d
    fld<int>(ctx, 0x0bc4) = 0;                                   // 0x3b06a
    fld<int>(ctx, 0x0bc8) = 0;                                   // 0x3b077
    fld<int>(ctx, 0x0bcc) = 0;                                   // 0x3b084
    fld<int>(ctx, 0x0bd0) = 0;                                   // 0x3b091
    fld<int>(ctx, 0x0bd4) = 0;                                   // 0x3b09e
    fld<int>(ctx, 0x0bd8) = 2;                                   // 0x3b0ab
    fld<int>(ctx, 0x0bdc) = 5;                                   // 0x3b0b8
    fld<short>(ctx, 0x0be0) = static_cast<short>(0xad57);        // 0x3b0c5
    fld<short>(ctx, 0x0be2) = static_cast<short>(0xdee2);        // 0x3b0d1
    fld<short>(ctx, 0x0be4) = static_cast<short>(0x78ac);        // 0x3b0dd
    fld<short>(ctx, 0x0be6) = static_cast<short>(0xd818);        // 0x3b0e9
    fld<short>(ctx, 0x0be8) = static_cast<short>(0x4236);        // 0x3b0f5
    fld<int>(ctx, 0x0bec) = 0;                                   // 0x3b101
    fld<int>(ctx, 0x0bf0) = 0;                                   // 0x3b10e
    fld<int>(ctx, 0x0bf4) = 0;                                   // 0x3b11b
    fld<int>(ctx, 0x0bf8) = 0;                                   // 0x3b128
    fld<int>(ctx, 0x0bfc) = 0;                                   // 0x3b135
}
void a_codecInitDecoderContext(Ctx &ctx, const uint32_t *a, Ret &) {
  const CxScope s(ctx);
  codecInitDecoderContext_body(P<char>(a[0]));
  neg_mem(ctx, NEG(codecInitDecoderContext), a[0] + 0x0u);
}

// ---- codecEncodeFrame (rva 0x3b150): from c3bv.cpp CodecProcessResource_re
// 0x0003b150  codecProcessResource (VA 0x3b150)
// with three stack arguments (`ret` at 0x3b224 pops nothing), a /Od /GZ debug body whose frame
// [ebp-0xa8..ebp-1] is filled with 0xcccccccc (`rep stosd`, ecx = 0x2a, at 0x3b16c).
// It is codecProcessBlock's mirror image: samples in, packed parameter words out.
//   [ebp+8]    the input samples, moved back by one dword at 0x3b177 when not null (`je 0x3b17d`) and
//              handed on as `+ 4` at 0x3b198, 0x3b1bb
//   [ebp+0xc]  the packed parameter words to write, moved back by one dword at 0x3b186 and handed on as
//              `+ 4` at 0x3b1f2
//   [ebp+0x10] the encoder context (the 0x2600-byte object codecStageDispatch, codecGainSmoother and
//              bitPermute's toggle at +0x2540 share)
// Four calls over seven locals of its own frame (0x3b18c-0x3b20f):
//   codecApplyFixedCoeffs 0x3d040 (0x3b19c) pre-filters the samples in place under the descriptor
//     0x63ef4 (180, 10);
//   codecStageDispatch 0x3c640 (0x3b1bf) runs the whole analysis and leaves the flag pair [ebp-0xc], the
//     index cell [ebp-0x10], the float level cell [ebp-0x68] and the float coefficient array [ebp-0x3c];
//   quantizeParams 0x3c220 (0x3b1e3) turns those four into the class cell [ebp-0x14], the exponent cell
//     [ebp-4] and the quantised coefficient array [ebp-0x64];
//   bitPack 0x3c180 (0x3b207) writes those three into the caller's output words under the descriptor
//     0x63ef8, with the context as bitPermute's toggle owner (0x3bf76).
// Returns 0 (0x3b20f).
int codecEncodeFrame_body(P<float> samples, P<int32_t> packed, P<char> ctx) {
    const CodecFrame fr(cx(), 0xA8);  // the original's frame, 0xcc-filled
    const P<char> ebp = fr.ebp();

    if (samples != 0) samples -= 1;                              // je at 0x3b172, sub at 0x3b177
    if (packed != 0) packed -= 1;                                // je at 0x3b181, sub at 0x3b186

    const auto applyFixedCoeffs = [](const auto &... a) { return ccall(0x0003D040, a...); };
    const auto stageDispatch = [](const auto &... a) { return ccall(0x0003C640, a...); };
    const auto quantizeParams = [](const auto &... a) { return ccall(0x0003C220, a...); };
    const auto bitPack = [](const auto &... a) { return ccall(0x0003C180, a...); };

    applyFixedCoeffs((samples + 1),            // add eax, 4 at 0x3b198
                     sva(0x00063EF4), ctx);               // call at 0x3b19c

    stageDispatch((samples + 1),               // add eax, 4 at 0x3b1bb
                  loc<void>(ebp, 0x0C), loc<void>(ebp, 0x10), loc<void>(ebp, 0x68),
                  loc<void>(ebp, 0x3C), ctx);               // call at 0x3b1bf

    quantizeParams(loc<void>(ebp, 0x0C), loc<void>(ebp, 0x10), loc<void>(ebp, 0x68),
                   loc<void>(ebp, 0x3C), loc<void>(ebp, 0x14), loc<void>(ebp, 0x04),
                   loc<void>(ebp, 0x64));                   // call at 0x3b1e3

    bitPack(sva(0x00063EF8), loc<void>(ebp, 0x14), loc<void>(ebp, 0x04),
            loc<void>(ebp, 0x64), packed + 1,               // add eax, 4 at 0x3b1f2
            ctx);                                                // call at 0x3b207
    return 0;                                                    // xor eax, eax at 0x3b20f
}
void a_codecEncodeFrame(Ctx &ctx, const uint32_t *a, Ret &r) {
  const CxScope s(ctx);
  r.set_i32(static_cast<int32_t>(neg_u32(NEG(codecEncodeFrame), static_cast<u32>(codecEncodeFrame_body(P<float>(a[0]), P<int32_t>(a[1]), P<char>(a[2]))))));
}

// ---- codecSynthesizeFrame (rva 0x3b230): from c3br.cpp CodecSynthStage_re

int codecSynthesizeFrame_body(P<int32_t> flags, P<int32_t> subLenCell, P<float> gainCell, P<float> coeffs, P<float> out,
                               P<int32_t> countCell, P<char> ctx) {
    // Frame slots the callees are handed, in the original's layout:
    //   lenSlots[i]   = [ebp - 0xe8 + i*4]   slot 0 the gain cell, slots 1.. the entry lengths
    //   levelSlots[i] = [ebp - 0x58 + i*4]   slot 0 the entry count, slots 1.. the entry levels
    //   modeSlots[i]  = [ebp - 0x9c + i*4]   slot 0 the loop counter, slots 1.. the entry modes
    //   blocks[i]     = [ebp - 0x3bc + i*4]  entry s occupies 10*s+1 .. 10*s+10
    // Each is kept as raw dwords because the same slot is read as an int by one callee and as a float by
    // another (the length array's slot 0 is the gain, 0x3b449 against 0x3b48a).
    const CodecFrame fr(cx(), 0x3E0);                            // the original's frame, 0xcc-filled (rep stosd at 0x3b24c)
    const P<uint32_t> lenSlots = fr.loc<uint32_t>(0xE8);
    const P<uint32_t> levelSlots = fr.loc<uint32_t>(0x58);
    const P<uint32_t> modeSlots = fr.loc<uint32_t>(0x9C);
    const P<float> lpcCoeffs = fr.loc<float>(0x110);
    const P<float> blocks = fr.loc<float>(0x3BC);
    const P<float> level2 = fr.loc<float>(0xA4);

    if (flags) flags -= 1;                                       // je at 0x3b252
    if (coeffs) coeffs -= 1;                                     // je at 0x3b261
    if (out) out -= 1;                                           // je at 0x3b270
    P<float> ring = pc<float>(pc<char>(ctx) + 0xA8);   // 0x3b27e
    P<int32_t> cursor = pc<int32_t>(pc<char>(ctx) + 0x648);    // 0x3b289

    int clamped = (*subLenCell > 0x9C) ? 0x9C : *subLenCell;     // jg at 0x3b29b
    if (clamped < 0x14) clamped = 0x14;                          // jl at 0x3b2c1
    *subLenCell = clamped;                                       // 0x3b2e1

    const float hi = rf(0x0005EBD4);                         // 0.99f
    const float lo = rf(0x0005EBD0);                         // -0.99f
    int order = (*g_order());                              // 0x3b2e3
    for (int i = 1; i <= order; ++i) {                           // jg at 0x3b30f
        unsigned raw = *pc<uint32_t>(&coeffs[i]);        // 0x3b31a
        float v;
        *pc<uint32_t>(&v) = raw;
        // `fcomp` + `test ah, 0x41` + `je`: the replacement is taken only when the value is strictly above.
        if (static_cast<double>(v) > static_cast<double>(hi)) raw = 0x3F7D70A4u;   // je at 0x3b32e
        *pc<uint32_t>(&v) = raw;
        // `test ah, 1` + `jne`: taken when C0 is set, which is "below or unordered".
        if (!(static_cast<double>(v) >= static_cast<double>(lo))) raw = 0xBF7D70A4u;   // jne at 0x3b35c
        *pc<uint32_t>(&coeffs[i]) = raw;          // 0x3b382
    }

    const auto interp = [](const auto &... a) { return ccall(0x0003DA50, a...); };
    interp(g_order(),
           pc<int32_t>(flags + 1),
           subLenCell, gainCell, coeffs + 1,
           g_frameLen(),
           pc<int32_t>(&modeSlots[1]),
           pc<int32_t>(&lenSlots[1]),
           pc<float>(&levelSlots[1]),
           &blocks[11],
           pc<int32_t>(&levelSlots[0]),
           level2, ctx);                                         // 0x3b38a-0x3b3d7

    const int entries = static_cast<int>(levelSlots[0]);         // 0x3b3da
    if (entries > 0) {                                           // jle at 0x3b3de
        const auto reflect = [](const auto &... a) { return ccall(0x0003D8D0, a...); };
        const auto synth = [](const auto &... a) { return ccall(0x0003D1D0, a...); };
        const auto iir = [](const auto &... a) { return ccall(0x0003D0A0, a...); };
        for (int s = 1; s <= entries; ++s) {                     // jg at 0x3b40e
            reflect(&blocks[10 * s + 1], &lpcCoeffs[0],
                    g_order(),
                    pc<float>(sva(0x00063EFC)),
                    pc<float>(&lenSlots[0]));     // 0x3b414-0x3b43d
            synth(&lpcCoeffs[0],
                  pc<int32_t>(&lenSlots[s]),
                  pc<int32_t>(&modeSlots[s]),
                  ring + *cursor,
                  pc<float>(&levelSlots[s]),
                  level2,
                  pc<float>(&lenSlots[0]),
                  ctx);                                          // 0x3b445-0x3b491
            iir(ring + *cursor, pc<int32_t>(&lenSlots[s]), ctx);   // 0x3b499-0x3b4b7
            *cursor = *cursor + static_cast<int>(lenSlots[s]);   // 0x3b4bf-0x3b4d4
        }
        const float divisor = rf(0x0005EBCC);                // 4096.0f
        for (int i = 1; i <= 0xB4; ++i)                          // jg at 0x3b500
            out[i] = static_cast<float>(static_cast<double>(ring[i - 1])
                                        / static_cast<double>(divisor));   // 0x3b50b-0x3b51e
        *countCell = 0xB4;                                       // 0x3b526
        *cursor = *cursor - 0xB4;                                // 0x3b52c-0x3b53a
        const int left = *cursor;                                // 0x3b53f
        for (int i = 1; i <= left; ++i)                          // jg at 0x3b568
            copy32(&ring[i - 1], &ring[i + 0xB3]);          // 0x3b56a-0x3b583
    }
    return 0;                                                    // 0x3b589
}
void a_codecSynthesizeFrame(Ctx &ctx, const uint32_t *a, Ret &r) {
  const CxScope s(ctx);
  r.set_i32(static_cast<int32_t>(neg_u32(NEG(codecSynthesizeFrame), static_cast<u32>(codecSynthesizeFrame_body(P<int32_t>(a[0]), P<int32_t>(a[1]), P<float>(a[2]), P<float>(a[3]), P<float>(a[4]), P<int32_t>(a[5]), P<char>(a[6]))))));
}

// ---- codecDecodeParams (rva 0x3b5a0): from c3br.cpp CodecTransformStage_re
// 0x0003b5a0  codecTransformStage (VA 0x3b5a0)
// with eight stack arguments (`ret` at 0x3bf22 pops nothing), a /Od /GZ debug body.
//   [ebp+8]    a class cell, used to index the 16-entry table at 0x64014 (0x3b644)
//   [ebp+0xc]  a level cell (in/out)
//   [ebp+0x10] the integer coefficient array, decremented at 0x3b5c4 (out)
//   [ebp+0x14] a two-element flag block, decremented at 0x3b5d3 (out)
//   [ebp+0x18] a gain cell (out)
//   [ebp+0x1c] a float cell (out)
//   [ebp+0x20] the float coefficient array, decremented at 0x3b5e2 (out)
//   [ebp+0x24] the stage's own record
// The record is bound field by field at 0x3b5eb-0x3b63c: six cells at +0, +4, +8, +0xc, +0x10 and +0x14,
// a three-wide coefficient history at +0x18 (entry i occupies the dwords [+0x18 + i*12 - 12], [-8] and [-4], so
// ten entries fill +0x18..+0x8c), a three-entry gain history at +0x90 and a three-entry level history at +0x9c.
// MODE FLAG CLEAR (`jne 0x3b6ee` at 0x3b655): the body only derives the two flags from the class cell
// (both start at 1, the first clears for a class of 1 or below at 0x3b675 and the second for a class of 0 or
// 2 at 0x3b687 and 0x3b68f), writes the table entry or, when it is 4 or below, the record's +0
// (`jg 0x3b6b5` at 0x3b6a9) into the gain cell, copies the gain cell back into +0 when both flags are 1
// (0x3b6bc, 0x3b6c5) and restores it from +0 when the two flags differ (`je 0x3b6e9` at 0x3b6dd),
// then jumps straight to the output conversion at 0x3bd77.
// MODE FLAG SET: a table entry above 4 (`jle 0x3b722` at 0x3b6f2) is written to the gain history's slot 0
// and the record's +0x10 becomes `(*+0x10 * 15 + entry + 8) / 16` (0x3b703-0x3b71e) with a working class
// of 2; otherwise the working class is the entry itself and the gain history's slot 0 takes +0x10. The level
// history's slot 0 takes the level cell (0x3b73a) and the coefficient history's oldest column takes the
// caller's coefficients (0x3b75f-0x3b771).
// The transform code is `table[class + (*+8 << 4) + *+0xc * 4 + 1]` from the 33-entry table at 0x63f10
// (0x3b777-0x3b79a); its low two bits select the gain-history rotation (0x3b79d) and the rest, divided
// by 8 and again by 64 when the record's +0x14 is below 2048 (the int at 0x63f04, `jge 0x3b7d1` at
// 0x3b7c0), is the flag word. The band index is 4, 3, 2 or 1 as +0x14 falls below 2048, 1024 or 128 (the
// ints at 0x63f10, 0x63f0c and 0x63f08, 0x3b7d8-0x3b814). The two output flags are bits 1 and 0
// of the flag word (0x3b814-0x3b82e).
// A non-zero record +4 (`je 0x3b861` at 0x3b837) clears it, writes the same gain the mode-flag-clear path
// writes and skips to the flag-word tail at 0x3bc99.
// Bit 4 of the flag word (the int 0x10 at 0x64450, `je 0x3b9e7` at 0x3b86c) runs the decoder:
// decodeTableSymbol 0x3e520 is called once for the level history (key `history[22] << 4 | level/2`,
// 0x3b885-0x3b8aa) and once for each of four coefficient entries (0x3b8e5-0x3b9c1), each key being
// a four-bit context from another entry and the entry's own five bits; a non-negative result becomes
// `parity + 2*result` folded into [-16, 15] by the `& 0x10` test at 0x3b98a, a negative one leaves the
// entry's previous column. The record's +0x14 then becomes
// `__ftol(*+0x14 * 0.96875f + symbols*0x66)` (the float at 0x5ebdc, `fiadd` at 0x3b9da).
// The level cell takes the level history's slot 1 and the coefficients the history's middle column
// (0x3b9e7-0x3ba28); the gain history rotates by the code's low two bits (0x3ba2d, 0x3ba3f) and the
// gain cell takes its slot 1 (0x3ba50).
// Bits 2 and 3 of the flag word (the ints at 0x64448 and 0x6444c) run medianOfThree 0x3e480 over the
// level history, over each of six coefficient entries and over the gain history, but only where BOTH distances
// from the middle slot reach the band's threshold (the float tables at 0x63fa0, 0x63f80 indexed
// `band + i*4 + 8`, and 0x63f90; the `test ah, 1` after each `fcomp` skips when the distance is below).
// Bit 5 (the int at 0x64454) overwrites the coefficients from entry 5 upwards with the table at 0x64418
// (0x3bca6-0x3bcdb). The record then shifts: +0xc takes the working class, +8 the second output flag, and
// the gain, level and coefficient histories move one column down (0x3bcdd-0x3bd75).
// The output conversion (0x3bd77) maps the level cell through the 64-entry table at 0x64214 indexed
// `(0x1f - level) * 2`; coefficients 1 and 2 go through the 32-entry table at 0x64314 indexed by twice their
// magnitude (a magnitude above 0xf is replaced by 0 only on the negative side, 0x3bdd3) with the sign put
// back and are scaled by `intPow(2, 0xf - table[i])` (the tables at 0x643f0 and the int 2 at 0x63f00);
// coefficients 3 upwards take the same scaling plus the offset table at 0x643c8 and are then multiplied by
// the float table at 0x64388 and offset by the int table at 0x643a8 before __ftol (0x3be9f-0x3beb6).
// The float cell takes the level as a float (0x3bec9) and each coefficient is divided by the float 16384.0f
// at 0x5ebd8 into the float array (0x3bef3-0x3bf08). Returns 0 (0x3bf0d).
int codecDecodeParams_body(P<int32_t> classCell, P<int32_t> levelCell, P<int32_t> coeffOut, P<int32_t> flagPair,
                                   P<int32_t> gainCell, P<float> levelFloat, P<float> coeffFloat, P<char> record) {
    const CodecFrame fr(cx(), 0xBC);                             // the original's frame, 0xcc-filled (rep stosd at 0x3b5bc)
    const R<int32_t> key = *fr.loc<int32_t>(0x40);               // the three cells decodeTableSymbol takes by address
    const R<int32_t> value = *fr.loc<int32_t>(0x34);
    const R<int32_t> symbols = *fr.loc<int32_t>(0x5C);
    const R<int32_t> exponent = *fr.loc<int32_t>(0x58);          // intPow's exponent cell (0x3be0c, 0x3be72)
    if (coeffOut) coeffOut -= 1;                                 // je at 0x3b5c2
    if (flagPair) flagPair -= 1;                                 // je at 0x3b5d1
    if (coeffFloat) coeffFloat -= 1;                             // je at 0x3b5e0

    P<char> rec = pc<char>(record);
    P<int32_t> f00 = pc<int32_t>(rec + 0x00);               // 0x3b5eb
    P<int32_t> f04 = pc<int32_t>(rec + 0x04);               // 0x3b5f1
    P<int32_t> f08 = pc<int32_t>(rec + 0x08);               // 0x3b5fa
    P<int32_t> f0c = pc<int32_t>(rec + 0x0C);               // 0x3b603
    P<int32_t> f10 = pc<int32_t>(rec + 0x10);               // 0x3b60c
    P<int32_t> f14 = pc<int32_t>(rec + 0x14);               // 0x3b615
    P<int32_t> hist = pc<int32_t>(rec + 0x18);              // 0x3b61e
    P<int32_t> gains = pc<int32_t>(rec + 0x90);             // 0x3b627
    P<int32_t> levels = pc<int32_t>(rec + 0x9C);            // 0x3b633

    const int entry = pc<int32_t>(sva(0x00064014))[*classCell];   // 0x3b644
    int klass = 0;                                               // [ebp-0x44]
    int flagWord = 0;                                            // [ebp-0x3c]
    bool skipToFlagTail = false;

    if ((*g_errProtect()) == 0) {                               // jne at 0x3b655
        flagPair[1] = 1;                                         // 0x3b65e
        flagPair[2] = 1;                                         // 0x3b668
        if (*classCell <= 1) flagPair[1] = 0;                    // jg at 0x3b675
        if (*classCell == 0 || *classCell == 2) flagPair[2] = 0; // je at 0x3b687, jne at 0x3b68f
        *gainCell = entry;                                       // 0x3b69b-0x3b6a1
        if (*gainCell <= 4) *gainCell = *f00;                    // jg at 0x3b6a9
        if (flagPair[1] == 1 && flagPair[2] == 1) *f00 = *gainCell;   // jne at 0x3b6bc, 0x3b6c5
        if (flagPair[1] != flagPair[2]) *gainCell = *f00;        // je at 0x3b6dd
        // 0x3b6e9 jumps straight to the output conversion
    } else {
        if (entry > 4) {                                         // jle at 0x3b6f2
            gains[0] = entry;                                    // 0x3b6fa
            klass = 2;                                           // 0x3b6fc
            *f10 = (*f10 * 15 + entry + 8) / 16;                 // 0x3b703-0x3b71e
        } else {
            klass = entry;                                       // 0x3b725
            gains[0] = *f10;                                     // 0x3b728-0x3b730
        }
        levels[0] = *levelCell;                                  // 0x3b732-0x3b73a

        int n = (*g_order());                              // 0x3b73c
        for (int i = 1; i <= n; ++i)                             // jg at 0x3b75d
            hist[i * 3 - 3] = coeffOut[i];                       // 0x3b75f-0x3b771

        const int codeIndex = klass + (*f08 << 4) + *f0c * 4 + 1;        // 0x3b777-0x3b78d
        const int code = pc<int32_t>(sva(0x00063F10))[codeIndex];   // 0x3b793
        const int rotate = code & 3;                             // 0x3b79d-0x3b7a3
        flagWord = code / 8;                                     // 0x3b7a6-0x3b7b2
        if (*f14 < ri(0x00063F04)) flagWord = flagWord / 64; // jge at 0x3b7c0
        int band = 4;                                            // 0x3b7d1
        if (*f14 < ri(0x00063F10)) band = 3;                 // jge at 0x3b7e3
        if (*f14 < ri(0x00063F0C)) band = 2;                 // jge at 0x3b7f7
        if (*f14 < ri(0x00063F08)) band = 1;                 // jge at 0x3b80b
        flagPair[1] = (flagWord / 2) & 1;                        // 0x3b814-0x3b822
        flagPair[2] = flagWord & 1;                              // 0x3b825-0x3b82e

        if (*f04 != 0) {                                         // je at 0x3b837
            *f04 = 0;                                            // 0x3b83c
            *gainCell = entry;                                   // 0x3b842-0x3b848
            if (*gainCell <= 4) *gainCell = *f00;                // jg at 0x3b850
            skipToFlagTail = true;                               // 0x3b85c
        }

        if (!skipToFlagTail) {
            const auto median = [](const auto &... a) { return ccall(0x0003E480, a...); };
            if ((flagWord & ri(0x00064450)) != 0) {          // je at 0x3b86c
                const auto decode = [](const auto &... a) { return ccall(0x0003E520, a...); };
                symbols = 0;                                     // 0x3b872
                int parity = levels[1] & 1;                      // 0x3b879-0x3b882
                key = (hist[22] << 4) + levels[1] / 2;           // 0x3b885-0x3b89b
                decode(&key, &value, &symbols);                  // 0x3b89e-0x3b8aa
                levels[1] = levels[2];                           // 0x3b8b2-0x3b8bb
                if (value >= 0) levels[1] = parity + value * 2;  // jl at 0x3b8c2, lea at 0x3b8ca
                for (int i = 1; i <= 4; ++i) {                   // jg at 0x3b8e9
                    int context;
                    if (i == 1)                                  // jne at 0x3b8f3
                        context = (hist[25] & 7) * 2 + (hist[28] & 1);     // 0x3b8f5-0x3b90a
                    else
                        context = hist[(9 - i) * 3 - 2] & 0xF;             // 0x3b90f-0x3b924
                    const int cell = hist[(5 - i) * 3 - 2] & 0x1F;         // 0x3b927-0x3b93c
                    parity = cell & 1;                           // 0x3b93f-0x3b945
                    key = (context << 4) + cell / 2;             // 0x3b948-0x3b958
                    decode(&key, &value, &symbols);              // 0x3b95b-0x3b967
                    if (value >= 0) {                            // jl at 0x3b973
                        value = parity + value * 2;              // 0x3b975-0x3b97e
                        if ((value & 0x10) == 0x10) value = value - 0x20;  // jne at 0x3b98a
                    } else {
                        value = hist[(5 - i) * 3 - 1];           // 0x3b997-0x3b9a9
                    }
                    hist[(5 - i) * 3 - 2] = value;               // 0x3b9ac-0x3b9bd
                }
                *f14 = ftol(static_cast<double>(*f14) * static_cast<double>(rf(0x0005EBDC))
                                 + static_cast<double>(symbols * 0x66));   // 0x3b9c6-0x3b9e5
            }

            *levelCell = levels[1];                              // 0x3b9e7-0x3b9f0
            n = (*g_order());                              // 0x3b9f2
            for (int i = 1; i <= n; ++i)                         // jg at 0x3ba13
                coeffOut[i] = hist[i * 3 - 2];                   // 0x3ba15-0x3ba28
            if (rotate == 1) gains[1] = gains[2];                // jne at 0x3ba31
            if (rotate == 3) gains[1] = gains[0];                // jne at 0x3ba43
            *gainCell = gains[1];                                // 0x3ba50-0x3ba59

            P<float> levelThr = pc<float>(sva(0x00063FA0));
            P<float> histThr = pc<float>(sva(0x00063F80));
            P<float> gainThr = pc<float>(sva(0x00063F90));
            if ((flagWord & ri(0x00064448)) != 0) {          // je at 0x3ba66
                int d = levels[1] - levels[0];                   // 0x3ba6c-0x3ba77
                int ad = (d < 0) ? -d : d;                       // jl at 0x3ba7e
                // `fcomp` + `test ah, 1` + `jne`: the body is skipped when the distance is below the threshold.
                if (!(static_cast<double>(ad) < static_cast<double>(levelThr[band]))) {   // jne at 0x3baa2
                    int d2 = levels[1] - levels[2];              // 0x3baa4-0x3bab0
                    int ad2 = (d2 < 0) ? -d2 : d2;               // jl at 0x3bab7
                    if (!(static_cast<double>(ad2) < static_cast<double>(levelThr[band])))   // jne at 0x3badb
                        *levelCell = median(&levels[2], &levels[1], &levels[0]);   // 0x3badd-0x3bafa
                }
                for (int i = 1; i <= 6; ++i) {                   // jg at 0x3bb12
                    int e = hist[i * 3 - 2] - hist[i * 3 - 3];   // 0x3bb18-0x3bb32
                    int ae = (e < 0) ? -e : e;                   // jl at 0x3bb39
                    const float t = histThr[band + i * 4 + 8];   // 0x3bb4e-0x3bb58
                    if (static_cast<double>(ae) < static_cast<double>(t)) continue;   // jne at 0x3bb64
                    int e2 = hist[i * 3 - 2] - hist[i * 3 - 1];  // 0x3bb6a-0x3bb84
                    int ae2 = (e2 < 0) ? -e2 : e2;               // jl at 0x3bb8b
                    if (static_cast<double>(ae2) < static_cast<double>(t)) continue;  // jne at 0x3bbb6
                    coeffOut[i] = median(&hist[i * 3 - 1], &hist[i * 3 - 2],
                                         &hist[i * 3 - 3]);      // 0x3bbb8-0x3bbf0
                }
            }
            if ((flagWord & ri(0x0006444C)) != 0) {          // je at 0x3bc03
                int d = gains[1] - gains[0];                     // 0x3bc09-0x3bc14
                int ad = (d < 0) ? -d : d;                       // jl at 0x3bc1b
                if (!(static_cast<double>(ad) < static_cast<double>(gainThr[band]))) {    // jne at 0x3bc3f
                    int d2 = gains[1] - gains[2];                // 0x3bc41-0x3bc4d
                    int ad2 = (d2 < 0) ? -d2 : d2;               // jl at 0x3bc54
                    if (!(static_cast<double>(ad2) < static_cast<double>(gainThr[band])))  // jne at 0x3bc78
                        *gainCell = median(&gains[2], &gains[1], &gains[0]);   // 0x3bc7a-0x3bc97
                }
            }
        }

        if ((flagWord & ri(0x00064454)) != 0) {              // je at 0x3bca4
            n = (*g_order());                              // 0x3bca6
            for (int i = 5; i <= n; ++i)                         // jg at 0x3bcc6
                coeffOut[i] = pc<int32_t>(sva(0x00064418))[i];   // 0x3bcc8-0x3bcd8
        }
        *f0c = klass;                                            // 0x3bcdd-0x3bce3
        *f08 = flagPair[2];                                      // 0x3bce5-0x3bcee
        gains[2] = gains[1];                                     // 0x3bcf0-0x3bcf9
        gains[1] = gains[0];                                     // 0x3bcfc-0x3bd04
        levels[2] = levels[1];                                   // 0x3bd07-0x3bd10
        levels[1] = levels[0];                                   // 0x3bd13-0x3bd1b
        n = (*g_order());                                  // 0x3bd1e
        for (int i = 1; i <= n; ++i) {                           // jg at 0x3bd3f
            hist[i * 3 - 1] = hist[i * 3 - 2];                   // 0x3bd41-0x3bd57
            hist[i * 3 - 2] = hist[i * 3 - 3];                   // 0x3bd5b-0x3bd71
        }
    }

    // 0x3bd77: the output conversion, reached from both arms
    *levelCell = pc<int32_t>(sva(0x00064214))[(0x1F - *levelCell) * 2];
                                                                 // 0x3bd77-0x3bd8d
    P<int32_t> magnitude = pc<int32_t>(sva(0x00064314));
    P<int32_t> shiftTable = pc<int32_t>(sva(0x000643F0));
    P<int32_t> powBase = pc<int32_t>(sva(0x00063F00));
    const auto intPow = [](const auto &... a) { return ccall(0x0003E2C0, a...); };
    for (int i = 1; i <= 2; ++i) {                               // jg at 0x3bda5
        int v = coeffOut[i];                                     // 0x3bdab-0x3bdb4
        int negative = 0;                                        // 0x3bdb7
        if (v < 0) {                                             // jge at 0x3bdc2
            negative = 1;                                        // 0x3bdc4
            v = -v;                                              // 0x3bdce
            if (v > 0xF) v = 0;                                  // jle at 0x3bdd7
        }
        v = magnitude[v * 2];                                    // 0x3bde0-0x3bdec
        if (negative == 1) v = -v;                               // jne at 0x3bdf3
        exponent = 0xF - shiftTable[i];                          // 0x3bdfd-0x3be0c
        coeffOut[i] = v * intPow(powBase, &exponent);            // 0x3be0f-0x3be2c
    }
    P<int32_t> offsetA = pc<int32_t>(sva(0x000643C8));
    P<float> scale = pc<float>(sva(0x00064388));
    P<int32_t> offsetB = pc<int32_t>(sva(0x000643A8));
    int n = (*g_order());                                  // 0x3be34
    for (int i = 3; i <= n; ++i) {                               // jg at 0x3be55
        int v = coeffOut[i];                                     // 0x3be57-0x3be60
        exponent = 0xF - shiftTable[i];                          // 0x3be63-0x3be72
        v = v * intPow(powBase, &exponent);                      // 0x3be75-0x3be8c
        v = v + offsetA[i];                                      // 0x3be8f-0x3be9c
        // fild dword [ebp-0x50]; fmul dword [ecx*4+0x64388]; fiadd dword [edx*4+0x643a8]; __ftol
        coeffOut[i] = ftol(static_cast<double>(v) * static_cast<double>(scale[i])
                                + static_cast<double>(offsetB[i]));   // 0x3be9f-0x3beb6
    }
    *levelFloat = static_cast<float>(static_cast<double>(*levelCell));   // 0x3bec6-0x3bece
    const float divisor = rf(0x0005EBD8);                    // 16384.0f
    n = (*g_order());                                      // 0x3bed0
    for (int i = 1; i <= n; ++i)                                 // jg at 0x3bef1
        coeffFloat[i] = static_cast<float>(static_cast<double>(coeffOut[i])
                                           / static_cast<double>(divisor));   // 0x3bef3-0x3bf08
    return 0;                                                    // 0x3bf0d
}
void a_codecDecodeParams(Ctx &ctx, const uint32_t *a, Ret &r) {
  const CxScope s(ctx);
  r.set_i32(static_cast<int32_t>(neg_u32(NEG(codecDecodeParams), static_cast<u32>(codecDecodeParams_body(P<int32_t>(a[0]), P<int32_t>(a[1]), P<int32_t>(a[2]), P<int32_t>(a[3]), P<int32_t>(a[4]), P<float>(a[5]), P<float>(a[6]), P<char>(a[7]))))));
}

// ---- bitPermute (rva 0x3bf30): from c3bi.cpp BitPermute_re
// 0x0003bf30  bitPermute (VA 0x3bf30)
// with seven stack arguments ([ebp+8] the mode, [ebp+0xc] the value count, [ebp+0x10] and [ebp+0x14]
// two extra value slots, [ebp+0x18] the value array, [ebp+0x1c] the bit array, [ebp+0x20] the codec context;
// `ret` at 0x3c171 pops nothing), a /Od /GZ debug body.
// The value array and the bit array are decremented by one dword on entry (0x3bf51, 0x3bf5a), so both are
// one-based. The whole frame is filled with 0xcccccccc by the /GZ prologue (`rep stosd` at 0x3bf4c), which
// covers the 14-slot work array the body indexes as `[ebp + k*4 - 0x40]` (k = 1..13) and, three dwords higher,
// as `[ebp + i*4 - 0x34]` (= slot i+3).
// `cmp [ebp-0x44], 1` / `je 0x3bf6e` at 0x3bf66 is the two-case switch VC6 writes with the equality test
// first: mode 1 is the UNPACK case at 0x3c052, every other mode the PACK case at 0x3bf73.
//
// PACK (0x3bf73): slot 1 takes *[ebp+0x10] (0x3bf81), slot 2 takes *[ebp+0x14] (0x3bf89), slot 3 is
// zeroed (0x3bf8e) and slot i+3 takes `values[count + 1 - i] & 0x7fff` for i = 1..*count, so the values are
// copied in REVERSE and masked to 15 bits (0x3bfb7-0x3bfd0). The 53-entry dword table at 0x6447c then
// names one slot per output bit: for i = 1..53 the bit array gets `slot[table[i]] & 1` (0x3c005) and that
// slot is divided by two (`cdq` / `sub eax, edx` / `sar eax, 1` at 0x3c016-0x3c019, a SIGNED division, so
// slots 1 and 2 may be negative). The 54th output dword, at byte offset 0xd8 of the decremented bit array, is
// bit 0 of the context's dword at +0x2540 (0x3c036) and that dword is then replaced by `1 - itself`
// (0x3c044, 0x3c049). Returns 0 (0x3c04b).
//
// UNPACK (0x3c052): slots 1..13 are zeroed (0x3c06a), then for i = 1..53 the same table is walked
// BACKWARDS (`54 - i` at 0x3c08f) and each named slot becomes `bits[54-i] + slot*2` (`lea ecx, [eax+edx*2]`
// at 0x3c0b0), i.e. the bits are shifted back in most significant first. Slot i+3 is then sign-extended out
// of its field for i = 1..*count: when it still carries the sign bit of the 10-entry dword table at 0x64454
// (`and ecx, table[i]` at 0x3c0f4) twice that mask is subtracted (0x3c109, 0x3c112). Slot 1 is written
// back to *[ebp+0x10] and slot 2 to *[ebp+0x14] (0x3c123, 0x3c12b), and the value array is refilled in
// reverse, `values[i] = slot[count + 4 - i]` (0x3c154-0x3c164). Returns 0 (0x3c169).
int bitPermute_body(int mode, P<int32_t> count, P<int32_t> slot1Out, P<int32_t> slot2Out,
                          P<int32_t> values, P<int32_t> bits, P<char> context) {
    const CodecFrame fr(cx(), 0x84);                             // the original's frame, 0xcc-filled (rep stosd at 0x3bf4c)
    const P<int32_t> work = fr.loc<int32_t>(0x40);               // slot k at [ebp + k*4 - 0x40]

    values -= 1;                                             // sub at 0x3bf51
    bits -= 1;                                               // sub at 0x3bf5a
    P<int32_t> order = pc<int32_t>(sva(0x0006447C));
    P<int32_t> signMask = pc<int32_t>(sva(0x00064454));

    if (mode == 1) {                                         // je at 0x3bf6a
        for (int k = 1; k <= 13; ++k) work[k] = 0;           // 0x3c06a
        for (int i = 1; i <= 53; ++i) {                      // jg at 0x3c08d
            const int j = 0x36 - i;                          // 0x3c08f
            const int k = order[j];                          // 0x3c097
            work[k] = bits[j] + work[k] * 2;                 // lea at 0x3c0b0
        }
        int n = *count;                                      // 0x3c0cb
        for (int i = 1; i <= n; ++i) {                       // jg at 0x3c0e8
            if ((work[i + 3] & signMask[i]) != 0)            // je at 0x3c0fd
                work[i + 3] = work[i + 3] - signMask[i] * 2; // 0x3c109, 0x3c112
        }
        *slot1Out = work[1];                                 // 0x3c123
        *slot2Out = work[2];                                 // 0x3c12b
        n = *count;                                          // 0x3c130
        for (int i = 1; i <= n; ++i)                         // jg at 0x3c14d
            values[i] = work[*count + 4 - i];                // 0x3c154-0x3c164
        return 0;                                            // 0x3c169
    }

    P<int32_t> toggle = pc<int32_t>(pc<char>(context) + 0x2540);   // 0x3bf76
    work[1] = *slot1Out;                                     // 0x3bf81
    work[2] = *slot2Out;                                     // 0x3bf89
    work[3] = 0;                                             // 0x3bf8e
    int n = *count;                                          // 0x3bf98
    for (int i = 1; i <= n; ++i)                             // jg at 0x3bfb5
        work[i + 3] = values[*count + 1 - i] & 0x7FFF;       // 0x3bfc8, 0x3bfd0
    for (int i = 1; i <= 53; ++i) {                          // jg at 0x3bfec
        const int k = order[i];                              // 0x3bff1
        bits[i] = work[k] & 1;                               // 0x3bffc, 0x3c005
        work[k] = work[k] / 2;                               // cdq/sub/sar at 0x3c016
    }
    bits[0x36] = *toggle & 1;                                // 0x3c030, 0x3c036
    *toggle = 1 - *toggle;                                   // 0x3c044, 0x3c049
    return 0;                                                // 0x3c04b
}
void a_bitPermute(Ctx &ctx, const uint32_t *a, Ret &r) {
  const CxScope s(ctx);
  r.set_i32(static_cast<int32_t>(neg_u32(NEG(bitPermute), static_cast<u32>(bitPermute_body(static_cast<int32_t>(a[0]), P<int32_t>(a[1]), P<int32_t>(a[2]), P<int32_t>(a[3]), P<int32_t>(a[4]), P<int32_t>(a[5]), P<char>(a[6]))))));
}

// ---- bitPack (rva 0x3c180): from c3au.cpp BitPack_re
// 0x0003c180  bitPack (VA 0x3c180)
// with six stack arguments ([ebp+8] .. [ebp+0x1c], `ret` at 0x3c1ca pops nothing), a /Od /GZ debug
// wrapper: it pushes its six arguments in reverse (0x3c198-0x3c1af) behind a leading 0 (`push 0` at
// 0x3c1b0) and calls bitPermute at 0x3bf30 (0x3c1b2), then returns bitPermute's value in eax, which
// is always 0 (`xor eax, eax` at 0x3c04b and 0x3c169). The leading 0 is bitPermute's mode selector: 0
// serialises, 1 deserialises (`cmp [ebp-0x44], 1` / `je 0x3bf6e` at 0x3bf66). bitPack's sixth argument
// is the one only the serialising mode uses (`[ebp+0x20] + 0x2540` at 0x3bf76).
// The reimplementation forwards to the original bitPermute: the hook under test is this wrapper's argument
// shuffle, not the 578-byte body it calls.
int bitPack_body(P<int32_t> count, P<int32_t> a, P<int32_t> b, P<int32_t> values, P<int32_t> bits, P<char> ctx) {
    const auto permute = [](const auto &... a) { return ccall(0x0003bf30, a...); };
    return permute(0, count, a, b, values, bits, ctx);
}
void a_bitPack(Ctx &ctx, const uint32_t *a, Ret &r) {
  const CxScope s(ctx);
  r.set_i32(static_cast<int32_t>(neg_u32(NEG(bitPack), static_cast<u32>(bitPack_body(P<int32_t>(a[0]), P<int32_t>(a[1]), P<int32_t>(a[2]), P<int32_t>(a[3]), P<int32_t>(a[4]), P<char>(a[5]))))));
}

// ---- bitUnpack (rva 0x3c1d0): from c3au.cpp BitUnpack_re
// 0x0003c1d0  bitUnpack (VA 0x3c1d0)
// with five stack arguments ([ebp+8] .. [ebp+0x18], `ret` at 0x3c218), the same /Od /GZ wrapper
// shape as bitPack: a trailing 0 is pushed first (0x3c1e8), then the five arguments in reverse
// (0x3c1ea-0x3c1fd), then the mode 1 (`push 1` at 0x3c1fe), and bitPermute 0x3bf30 is called
// (0x3c200). So the serialising mode's extra argument is passed as NULL, which mode 1 never reads: the
// slot [ebp-4] that holds it in mode 0 is only written at 0x3bf7b, on the other side of the mode test.
// Returns bitPermute's 0.
int bitUnpack_body(P<int32_t> count, P<int32_t> a, P<int32_t> b, P<int32_t> values, P<int32_t> bits) {
    const auto permute = [](const auto &... a) { return ccall(0x0003bf30, a...); };
    return permute(1, count, a, b, values, bits, 0);
}
void a_bitUnpack(Ctx &ctx, const uint32_t *a, Ret &r) {
  const CxScope s(ctx);
  r.set_i32(static_cast<int32_t>(neg_u32(NEG(bitUnpack), static_cast<u32>(bitUnpack_body(P<int32_t>(a[0]), P<int32_t>(a[1]), P<int32_t>(a[2]), P<int32_t>(a[3]), P<int32_t>(a[4]))))));
}

// ---- quantizeParams (rva 0x3c220): from c3br.cpp QuantizeParams_re
// 0x0003c220  quantizeParams (VA 0x3c220)
// with seven stack arguments (`ret` at 0x3c632 pops nothing), a /Od /GZ debug body whose frame
// [ebp-0x78..ebp-1] is filled with 0xcccccccc (`rep stosd` at 0x3c236); no local is read before it is
// written, so the fill never leaves the frame.
//   [ebp+8]    a pair of ints, decremented by one dword at 0x3c24d so that its elements are 1 and 2
//   [ebp+0xc]  an index cell, used to pick an entry of the table at 0x64594 (0x3c2be)
//   [ebp+0x10] a float cell
//   [ebp+0x14] a float array, decremented by one float at 0x3c244 (one-based)
//   [ebp+0x18] an int out cell
//   [ebp+0x1c] an int in/out cell
//   [ebp+0x20] the int out array, decremented by one dword at 0x3c23b (one-based)
// The int cell [ebp+0x1c] first takes __ftol of the float cell (0x3c256-0x3c260) and the out array takes
// __ftol of each of the first g_codecOrder coefficients scaled by the float 32768.0f at 0x5c524
// (0x3c28b-0x3c29f, `jg 0x3c2a4` at 0x3c283).
// The out cell [ebp+0x18] is then written three different ways (0x3c2a7-0x3c306): with both elements of
// the pair non-zero it takes the entry of the table at 0x64594 the index cell names; otherwise, with the mode
// flag 0xb58a8 set it takes 0 when the two elements are equal and 0x7f when they are not (`je 0x3c2f2` at
// 0x3c2e7), and with the mode flag clear it takes `pair[2] + pair[1]*2` (`lea` at 0x3c300).
// The int cell is then turned into an exponent by a binary search over the 64-entry descending threshold table
// at 0x647e4 (1024 down to 0 at 0x648e4): the value is clamped to 0x3ff (`jg 0x3c32b` at 0x3c31f),
// the cursor starts at 0x20 with a step of 0x10 (0x3c308, 0x3c30f) and, while the step is positive
// (`jle 0x3c381` at 0x3c33e), the cursor moves down by the step when the value is above the entry
// (0x3c34f) and up by the step when it is below it (0x3c369), the step halving each round (0x3c37a).
// One more decrement follows if the value is still above the entry (`jle 0x3c39b` at 0x3c390) and the cell
// becomes `0x1f - cursor/2` (0x3c3a3-0x3c3ad).
// Elements 1 and 2 of the out array are requantised through the 64-entry table at 0x646e8: the sign is taken
// off (`jge 0x3c3f3` at 0x3c3e2), the magnitude is divided by 512 (0x3c3f3-0x3c402) and clamped to
// 0x3f (`jg 0x3c413` at 0x3c409), the table entry replaces it and the sign is put back
// (`je 0x3c43b` at 0x3c431).
// Elements 3..g_codecOrder are requantised through three 8-entry tables indexed by `g_codecOrder + 1 - i`
// (0x3c484, 0x3c49f, 0x3c4f0, i.e. the tables at 0x64688, 0x646a8 and 0x646c8 addressed one
// based from 0x64684, 0x646a4 and 0x646c4): half the element plus the first table's entry is converted
// to a double by `fild` (0x3c49c), multiplied by the second table's float (0x3c4ab) and truncated by
// __ftol (0x3c4b2), clamped to [-0x7f, 0x7f] (`jl 0x3c4c8` at 0x3c4be, `jg 0x3c4e3` at 0x3c4d9)
// and divided by `intPow(*0x64554, third table entry)` (0x3c51e-0x3c533), the quotient being lowered by
// one when the value was negative (`jne 0x3c545` at 0x3c53a), which turns the C division into a floor.
// *0x64554 is 2 and the third table's entries are 6, 5, 4, 4, 4, 4, 3 and 3, so the divisor is a power of two
// between 8 and 64 and intPow never reaches its 1/0 trap.
// Finally, with the mode flag set and the out cell equal to 0 or 0x7f (`je 0x3c620` at 0x3c55d,
// `je 0x3c577` at 0x3c569, `jne 0x3c620` at 0x3c571), six more elements of the out array are filled
// from the 16-entry permutation table at 0x64558 indexed by `(x & 0x1e)/2` for x = elements 1, 2, 3, the int
// cell and element 4 (0x3c577-0x3c61d); element 9 takes half that entry and element 10 its lowest bit.
// Returns 0 (0x3c620).
int quantizeParams_body(P<int32_t> pair, P<int32_t> indexCell, P<float> value, P<float> coeffs,
                              P<int32_t> outCell, P<int32_t> expCell, P<int32_t> out) {
    const CodecFrame fr(cx(), 0x78);                             // the original's frame, 0xcc-filled (rep stosd at 0x3c236)
    const R<int32_t> exponent = *fr.loc<int32_t>(0x10);          // intPow's exponent cell
    out -= 1;                                                    // 0x3c23b
    coeffs -= 1;                                                 // 0x3c244
    pair -= 1;                                                   // 0x3c24d

    *expCell = ftol(static_cast<double>(*value));           // 0x3c256-0x3c260

    int order = (*g_order());                              // 0x3c262
    const float scale = rf(0x0005C524);                      // the float 32768.0f at 0x5c524
    for (int i = 1; i <= order; ++i)                             // jg at 0x3c283
        out[i] = ftol(static_cast<double>(coeffs[i])
                           * static_cast<double>(scale));        // 0x3c28b-0x3c29f

    if (pair[1] != 0 && pair[2] != 0) {                          // je at 0x3c2ab, je at 0x3c2b4
        *outCell = pc<int32_t>(sva(0x00064594))[*indexCell];   // 0x3c2be-0x3c2c5
    } else if ((*g_errProtect()) != 0) {                        // je at 0x3c2d0
        *outCell = 0;                                            // 0x3c2d5
        if (pair[1] != pair[2]) *outCell = 0x7F;                 // je at 0x3c2e7, 0x3c2ec
    } else {
        *outCell = pair[2] + pair[1] * 2;                        // lea at 0x3c300, 0x3c306
    }

    P<int32_t> thresholds = pc<int32_t>(sva(0x000647E4));
    int cursor = 0x20;                                           // 0x3c308
    int step = 0x10;                                             // 0x3c30f
    if (*expCell > 0x3FF) *expCell = 0x3FF;                      // jg at 0x3c31f, 0x3c338
    while (step > 0) {                                           // jle at 0x3c33e
        if (*expCell > thresholds[cursor]) cursor -= step;       // jle at 0x3c34f
        if (*expCell < thresholds[cursor]) cursor += step;       // jge at 0x3c369
        step = step / 2;                                         // 0x3c374-0x3c37c
    }
    if (*expCell > thresholds[cursor]) cursor -= 1;              // jle at 0x3c390
    *expCell = 0x1F - cursor / 2;                                // 0x3c39b-0x3c3ad

    P<int32_t> magnitudeTable = pc<int32_t>(sva(0x000646E8));
    for (int i = 1; i <= 2; ++i) {                               // jg at 0x3c3c5
        int v = out[i];                                          // 0x3c3d1
        int negative = 0;                                        // 0x3c3d7
        if (v < 0) { v = -v; negative = 1; }                     // jge at 0x3c3e2
        v = v / 512;                                             // 0x3c3f3-0x3c402
        if (v > 0x3F) v = 0x3F;                                  // jg at 0x3c409
        v = magnitudeTable[v];                                   // 0x3c423
        if (negative != 0) v = -v;                               // je at 0x3c431
        out[i] = v;                                              // 0x3c444
    }

    P<int32_t> biasTable = pc<int32_t>(sva(0x00064684));
    P<float> gainTable = pc<float>(sva(0x000646A4));
    P<int32_t> shiftTable = pc<int32_t>(sva(0x000646C4));
    P<int32_t> powBase = pc<int32_t>(sva(0x00064554));
    const auto intPow = [](const auto &... a) { return ccall(0x0003E2C0, a...); };
    order = (*g_order());                                  // 0x3c44c
    for (int i = 3; i <= order; ++i) {                           // jg at 0x3c46d
        int v = out[i] / 2;                                      // 0x3c479-0x3c481
        const int biased = v + biasTable[(*g_order()) + 1 - i];   // 0x3c484-0x3c499
        // fild dword [ebp-0x30] (0x3c49c); fmul dword [edx*4+0x646a4] (0x3c4ab); __ftol (0x3c4b2)
        v = ftol(static_cast<double>(biased)
                      * static_cast<double>(gainTable[(*g_order()) + 1 - i]));
        if (v < -0x7F) v = -0x7F;                                // jl at 0x3c4be, 0x3c4c8
        if (v > 0x7F) v = 0x7F;                                  // jg at 0x3c4d9, 0x3c4e3
        exponent = shiftTable[(*g_order()) + 1 - i];       // 0x3c4f0-0x3c503
        int negative = 0;                                        // 0x3c506
        if (v < 0) negative = -1;                                // jge at 0x3c511, 0x3c513
        v = div32(v, intPow(powBase, &exponent));                   // 0x3c51a-0x3c533
        if (negative == -1) v -= 1;                              // jne at 0x3c53a, 0x3c53f
        out[i] = v;                                              // 0x3c54e
    }

    if ((*g_errProtect()) != 0) {                               // je at 0x3c55d
        if (*outCell == 0 || *outCell == 0x7F) {                 // je at 0x3c569, jne at 0x3c571
            P<int32_t> perm = pc<int32_t>(sva(0x00064558));
            out[5] = perm[(out[1] & 0x1E) / 2];                  // 0x3c577-0x3c58f
            out[6] = perm[(out[2] & 0x1E) / 2];                  // 0x3c592-0x3c5aa
            out[7] = perm[(out[3] & 0x1E) / 2];                  // 0x3c5ad-0x3c5c5
            out[8] = perm[(*expCell & 0x1E) / 2];                // 0x3c5c8-0x3c5df
            out[9] = perm[(out[4] & 0x1E) / 2] / 2;              // 0x3c5e2-0x3c5ff
            out[10] = perm[(out[4] & 0x1E) / 2] & 1;             // 0x3c602-0x3c61d
        }
    }
    return 0;                                                    // 0x3c620
}
void a_quantizeParams(Ctx &ctx, const uint32_t *a, Ret &r) {
  const CxScope s(ctx);
  r.set_i32(static_cast<int32_t>(neg_u32(NEG(quantizeParams), static_cast<u32>(quantizeParams_body(P<int32_t>(a[0]), P<int32_t>(a[1]), P<float>(a[2]), P<float>(a[3]), P<int32_t>(a[4]), P<int32_t>(a[5]), P<int32_t>(a[6]))))));
}

// ---- codecAnalyzeFrame (rva 0x3c640): from c3bv.cpp CodecStageDispatch_re
// 0x0003c640  codecStageDispatch (VA 0x3c640)
// with six stack arguments (`ret` at 0x3d036 pops nothing), a /Od /GZ debug body whose frame
// [ebp-0x5e0..ebp-1] is filled with 0xcccccccc (`rep stosd`, ecx = 0x178, at 0x3c65c).
// Fourteen pointers into the encoder context are bound on entry (0x3c68b-0x3c750):
//   +0x10   (pa) a 0x21c-element float history, indexed one-based with the bias -0x2d4 (0x3c7a8), so
//           element 0xb5 is its first dword and element 0x2d0 its last
//   +0x880  (qa) a second history of the same shape (0x3c7d3)
//   +0x10f0 (sa) a history indexed with the bias -0x64 (0x3c897)
//   +0x1bd0 (ra) a history indexed with the bias -0x394 (0x3c835)
//   +0x20b0 a float offset cell          +0x20b4 (jumps) a ten-slot one-based index array
//   +0x20dc the jump count               +0x20e0 (w) three dwords     +0x20ec (u) six dwords
//   +0x2104 (v) six dwords               +0x211c (t) eight dwords     +0x213c (x) three dwords
//   +0x2148 (y) ten coefficients per frame, three frames deep (+0, +0x28, +0x50)
//   +0x21c0 codecFirstOrderFilter's state cell
// Arguments: [ebp+8] the input samples (moved back one dword at 0x3c667), [ebp+0xc] the flag pair out
// (0x3c676), [ebp+0x10] the index cell out, [ebp+0x14] the float level cell out, [ebp+0x18] the float
// coefficient array out (0x3c685), [ebp+0x1c] the encoder context.
// The body is one pass of the analyser:
//   1. the four histories are shifted down by the frame length (0x3c756-0x3c8a1);
//   2. the jump index array drops every entry at or below the frame length and the rest move down by it
//      (0x3c8a1-0x3c93c);
//   3. the per-frame records t, u, v, w, x and y all move down one frame (0x3c93c-0x3cb2e);
//   4. the new samples are scaled by the float 4096.0f at 0x5ebcc, offset by the cell at +0x20b0 and
//      appended to pa, their sum accumulating in [ebp-0x398] (0x3cb2e-0x3cbcb); the offset cell then
//      moves by +1.0f (0x5b8a8) when the sum is above the frame length and by -1.0f (0x5ebe0) when it is
//      below minus the frame length (0x3cbcb-0x3cc2b);
//   5. eleven analysis stages run in order (0x3cc2b-0x3cfa6): codecFirstOrderFilter 0x40da0,
//      codecMarkCorrelationJumps 0x40ab0, 0x40740, codecSymmetricFir 0x404d0,
//      codecCorrelationLags 0x40330, codecPitchAnalysis 0x40000, codecFeatureDecision 0x3f720 once
//      per half-window, codecGainSmoother 0x3f310, 0x3edc0, codecRemoveMean 0x3ed00,
//      codecRmsEnergy 0x3ec60, codecAutocorrelation 0x3e9f0, gaussSolveClamped 0x3e6d0 and
//      substituteIfBelowThreshold 0x3e600;
//   6. the four outputs are copied out of the context (0x3cfa9-0x3d021).
// Returns 0 (0x3d021).
int codecAnalyzeFrame_body(P<float> samplesIn, P<int32_t> pairOut, P<int32_t> indexOut, P<float> levelOut,
                                  P<float> coeffOut, P<char> ctx) {
    const CodecFrame fr(cx(), 0x5E0);  // the original's frame, 0xcc-filled
    const P<char> ebp = fr.ebp();

    P<float> samples = samplesIn;
    if (samples != 0) samples -= 1;                              // je at 0x3c662, sub at 0x3c667
    P<int32_t> pair = pairOut;
    if (pair != 0) pair -= 1;                                    // je at 0x3c671, sub at 0x3c676
    P<float> coeffs = coeffOut;
    if (coeffs != 0) coeffs -= 1;                                // je at 0x3c680, sub at 0x3c685

    P<char> c = pc<char>(ctx);
    // the four histories, each addressed element-wise with its own bias
    P<float> pa = pc<float>(c + 0x10 - 0x2D4);        // 0x3c68e, bias at 0x3c7a8
    P<float> qa = pc<float>(c + 0x880 - 0x2D4);       // 0x3c69a, bias at 0x3c7d3
    P<float> sa = pc<float>(c + 0x10F0 - 0x64);       // 0x3c6a9, bias at 0x3c897
    P<float> ra = pc<float>(c + 0x1BD0 - 0x394);      // 0x3c6b8, bias at 0x3c835
    P<float> offsetCell = pc<float>(c + 0x20B0);      // 0x3c6c6
    P<int32_t> jumps = pc<int32_t>(c + 0x20B4 - 4);           // 0x3c6d5, bias at 0x3c8eb
    P<int32_t> jumpCount = pc<int32_t>(c + 0x20DC);           // 0x3c6e4
    P<int32_t> w = pc<int32_t>(c + 0x20E0);                   // 0x3c6f2
    P<int32_t> u = pc<int32_t>(c + 0x20EC);                   // 0x3c701
    P<int32_t> v = pc<int32_t>(c + 0x2104);                   // 0x3c710
    P<int32_t> t = pc<int32_t>(c + 0x211C);                   // 0x3c71e
    P<int32_t> x = pc<int32_t>(c + 0x213C);                   // 0x3c72d
    P<float> y = pc<float>(c + 0x2148);               // 0x3c73c
    P<char> filterState = c + 0x21C0;                                // 0x3c74a

    const int frameLen = (*g_frameLen());
    const int order = (*g_order());

    // The four history shifts are integer `mov` pairs in the original, so they are dword copies here.
    int n = 0x2D0 - frameLen;                                    // 0x3c756-0x3c761
    for (int i = 0xB5; i <= n; ++i) {                            // jg 0x3c7e3 at 0x3c788
        copy32(&pa[i], &pa[frameLen + i]);                  // 0x3c78a-0x3c7af
        copy32(&qa[i], &qa[frameLen + i]);                  // 0x3c7b6-0x3c7da
    }
    n = 0x21C - frameLen;                                        // 0x3c7e3-0x3c7ee
    for (int i = 0xE5; i <= n; ++i)                              // jg 0x3c845 at 0x3c815
        copy32(&ra[i], &ra[frameLen + i]);                  // 0x3c817-0x3c83c
    n = 0x2D0 - frameLen;                                        // 0x3c845-0x3c850
    for (int i = 0x19; i <= n; ++i)                              // jg 0x3c8a1 at 0x3c877
        copy32(&sa[i], &sa[frameLen + i]);                  // 0x3c879-0x3c89b

    int kept = 1;                                                // 0x3c8a1
    n = *jumpCount - 1;                                          // 0x3c8ab-0x3c8b6
    for (int i = 1; i <= n; ++i) {                               // jg 0x3c92e at 0x3c8dd
        if (jumps[i] > frameLen) {                               // jle 0x3c92c at 0x3c8f5
            jumps[kept] = jumps[i] - frameLen;                   // 0x3c8f7-0x3c919
            kept += 1;                                           // 0x3c91d-0x3c926
        }
    }
    *jumpCount = kept;                                           // 0x3c92e-0x3c93a

    t[0] = t[2];                                                 // 0x3c93c-0x3c94b
    t[1] = t[3];                                                 // 0x3c94d-0x3c95c
    for (int j = 1; j <= 2; ++j) {                               // jg 0x3cb2e at 0x3c981
        u[2 * j - 2] = u[2 * j] - frameLen;                      // 0x3c987-0x3c9af
        u[2 * j - 1] = u[2 * j + 1] - frameLen;                  // 0x3c9b3-0x3c9db
        v[2 * j - 2] = v[2 * j] - frameLen;                      // 0x3c9df-0x3ca07
        v[2 * j - 1] = v[2 * j + 1] - frameLen;                  // 0x3ca0b-0x3ca33
        w[j - 1] = w[j];                                         // 0x3ca37-0x3ca52
        t[2 * j] = t[2 * j + 2];                                 // 0x3ca56-0x3ca77
        t[2 * j + 1] = t[2 * j + 3];                             // 0x3ca7a-0x3ca9c
        x[j - 1] = x[j];                                         // 0x3caa0-0x3cabb
        for (int m = 1; m <= order; ++m)                         // jg 0x3cb29 at 0x3caec
            // y is addressed with the bias -0x2c, so slot m of frame j is y[m + j*10 - 11]
            copy32(&y[m + j * 10 - 11], &y[m + (j + 1) * 10 - 11]);   // 0x3caee-0x3cb23
    }

    float sum = 0.0f;                                            // 0x3cb2e
    const float gain = rf(0x0005EBCC);                       // the float 4096.0f at 0x5ebcc
    for (int i = 1; i <= frameLen; ++i) {                        // jg 0x3cbcb at 0x3cb65
        // fld samples[i]; fmul 4096.0f; fsub [offsetCell]; fstp dword pa[0x2d0-frameLen+i]
        pa[0x2D0 - frameLen + i] = static_cast<float>(static_cast<double>(samples[i])
                                                      * static_cast<double>(gain)
                                                      - static_cast<double>(*offsetCell));
        // fld [ebp-0x398]; fadd pa[...]; fstp dword [ebp-0x398]   (0x3cbb6-0x3cbc3)
        sum = static_cast<float>(static_cast<double>(sum)
                                 + static_cast<double>(pa[0x2D0 - frameLen + i]));
    }
    // fild frameLen; fcomp sum; test ah,1 / je 0x3cbf4
    if (static_cast<double>(frameLen) < static_cast<double>(sum))
        *offsetCell = static_cast<float>(static_cast<double>(*offsetCell)
                                         + static_cast<double>(rf(0x0005B8A8)));   // 0x3cbde-0x3cbf2
    // fild -frameLen; fcomp sum; test ah,0x41 / jne 0x3cc2b
    if (!(static_cast<double>(-frameLen) <= static_cast<double>(sum)))
        *offsetCell = static_cast<float>(static_cast<double>(*offsetCell)
                                         + static_cast<double>(rf(0x0005EBE0)));   // 0x3cc15-0x3cc29

    const int head = 0x2D1 - frameLen;                           // 0x3cc2b-0x3cc36
    [](const auto &... a) { return ccall(0x00040DA0, a...); }(          // codecFirstOrderFilter, call at 0x3cc75
        &pa[head], &qa[head], g_frameLen(), sva(0x00064A14), filterState);

    [](const auto &... a) { return ccall(0x00040AB0, a...); }(          // codecMarkCorrelationJumps, call at 0x3ccaa
        c + 0x880, c + 0x20B4, jumpCount, sva(0x000648E8), sva(0x000648EC),
        sva(0x000648F0), g_frameLen(), ctx);

    [](const auto &... a) { return ccall(0x00040740, a...); }(         // call at 0x3ccf4
        c + 0x20B4, jumpCount, sva(0x000648E8), &w[2], u, sva(0x000648F4),
        g_frameLen(), sva(0x000648F8), sva(0x000648FC), sva(0x00064900),
        sva(0x00064904));

    [](const auto &... a) { return ccall(0x000404D0, a...); }(          // codecSymmetricFir, call at 0x3cd1f
        c + 0x10 + 0x390, c + 0x10F0 + 0x600, sva(0x00064908), g_frameLen());

    [](const auto &... a) { return ccall(0x00040330, a...); }(          // codecCorrelationLags, call at 0x3cd4b
        c + 0x10F0 + 0x330, c + 0x1BD0, sva(0x00064908), g_frameLen(),
        loc<void>(ebp, 0x394));

    [](const auto &... a) { return ccall(0x00040000, a...); }(          // codecPitchAnalysis, call at 0x3cd85
        c + 0x1BD0, sva(0x000648FC), sva(0x00064914), sva(0x0006490C),
        loc<void>(ebp, 0xF4), loc<void>(ebp, 0x3E0), loc<void>(ebp, 0x3E4),
        loc<void>(ebp, 0x3D8));

    const auto featureDecision = [](const auto &... a) { return ccall(0x0003F720, a...); };
    // The half counter IS slot 0 of the block [ebp-0xf8] whose slot 1 [ebp-0xf4] is the score array
    // codecPitchAnalysis filled, so the counter overwrites that slot; `&[ebp-0xf8]` is also what the stage is
    // handed as its half-window cell (0x3cdf2).
    for (*loc<int>(ebp, 0xF8) = 1;                          // 0x3cd8d
         *loc<int>(ebp, 0xF8) <= 2;                         // jg 0x3ce23 at 0x3cdaf
         *loc<int>(ebp, 0xF8) += 1) {                       // 0x3cd99-0x3cda2
        featureDecision(&u[4],                                   // 0x3ce12
                        c + 0x10, c + 0x10F0, sva(0x00064A04),
                        loc<void>(ebp, 0xF8),
                        // the two scores codecPitchAnalysis indexed inside the same block (0x3cddc,
                        // 0x3cdea)
                        ebp - 0xF8 + *loc<int>(ebp, 0x3E0) * 4,
                        ebp - 0xF8 + *loc<int>(ebp, 0x3E4) * 4,
                        loc<void>(ebp, 0x3D8), loc<void>(ebp, 0x394), w, t,
                        sva(0x000648F4), ctx);
    }

    [](const auto &... a) { return ccall(0x0003F310, a...); }(          // codecGainSmoother, call at 0x3ce4f
        loc<void>(ebp, 0xF4), sva(0x0006490C), loc<void>(ebp, 0x3E0), &t[7],
        indexOut, loc<void>(ebp, 0x374), ctx);

    // the chosen index selects one dword of the table at 0x64910 (0x3ce57-0x3ce64)
    *loc<int>(ebp, 0x3CC) =
        pc<int32_t>(sva(0x00064910))[*loc<int>(ebp, 0x374)];

    [](const auto &... a) { return ccall(0x0003EDC0, a...); }(          // call at 0x3cea6
        loc<void>(ebp, 0x3CC), t, &w[2], sva(0x000648F4), u, v,
        loc<void>(ebp, 0x38C), g_frameLen(), sva(0x000648FC));

    *loc<int>(ebp, 0x3AC) = v[5] + 1 - v[4];                // 0x3ceae-0x3cec3

    [](const auto &... a) { return ccall(0x0003ED00, a...); }(          // codecRemoveMean, call at 0x3ceee
        loc<void>(ebp, 0x3AC), &qa[v[4]], loc<void>(ebp, 0x368));

    *loc<int>(ebp, 0x04) = *loc<int>(ebp, 0x378) - *loc<int>(ebp, 0x37C) + 1;  // 0x3cef6-

    [](const auto &... a) { return ccall(0x0003EC60, a...); }(          // codecRmsEnergy, call at 0x3cf2d
        loc<void>(ebp, 0x04),
        ebp - 0x368 + (*loc<int>(ebp, 0x37C) - v[4]) * 4,   // 0x3cf12-0x3cf21
        &x[2]);

    [](const auto &... a) { return ccall(0x0003E9F0, a...); }(          // codecAutocorrelation, call at 0x3cf5b
        g_order(), sva(0x00064910), loc<void>(ebp, 0x3AC),
        loc<void>(ebp, 0x368), loc<void>(ebp, 0x574), loc<void>(ebp, 0x59C));

    [](const auto &... a) { return ccall(0x0003E6D0, a...); }(          // gaussSolveClamped, call at 0x3cf80
        g_order(), loc<void>(ebp, 0x574), loc<void>(ebp, 0x59C),
        pc<char>(y) + 0x50);

    [](const auto &... a) { return ccall(0x0003E600, a...); }(          // substituteIfBelowThreshold, call at 0x3cfa1
        g_order(), pc<char>(y) + 0x28, pc<char>(y) + 0x50);

    pair[1] = t[2];                                              // 0x3cfa9-0x3cfb5
    pair[2] = t[3];                                              // 0x3cfb8-0x3cfc4
    copy32(levelOut, x);                                    // 0x3cfc7-0x3cfd2
    n = (*g_order());                                      // 0x3cfd4
    for (int i = 1; i <= n; ++i)                                 // jg 0x3d021 at 0x3d001
        copy32(&coeffs[i], &y[i - 1]);                      // 0x3d003-0x3d01c
    return 0;                                                    // xor eax, eax at 0x3d021
}
void a_codecAnalyzeFrame(Ctx &ctx, const uint32_t *a, Ret &r) {
  const CxScope s(ctx);
  r.set_i32(static_cast<int32_t>(neg_u32(NEG(codecAnalyzeFrame), static_cast<u32>(codecAnalyzeFrame_body(P<float>(a[0]), P<int32_t>(a[1]), P<int32_t>(a[2]), P<float>(a[3]), P<float>(a[4]), P<char>(a[5]))))));
}

// ---- codecHighPassFrame (rva 0x3d040): from c3au.cpp CodecApplyFixed_re
// 0x0003d040  codecApplyFixedCoeffs (VA 0x3d040)
// with three stack arguments ([ebp+8], [ebp+0xc], [ebp+0x10]; `ret` at 0x3d095 pops nothing), a
// /Od /GZ debug wrapper that always returns 0 (`xor eax, eax` at 0x3d083). A non-null first argument is
// decremented by 4 (`sub eax, 4` at 0x3d061, guarded by `cmp [ebp+8], 0` / `je 0x3d067` at 0x3d058)
// and 4 is added back when it is pushed (`add eax, 4` at 0x3d077), so a non-null pointer is passed through
// unchanged and a null one is passed as 4.
// The call at 0x3d07b is the IIR filter at 0x40e30, with four arguments pushed in reverse
// (0x3d06a-0x3d07a): that pointer, the address 0x64a18 (whose first dword is 1 and is read as the
// filter's first sample index at 0x40e87), this function's second argument (the filter reads the last index
// through it at 0x40e7f) and its third (the filter's four-float state, read at 0x40e5a-0x40e74 and
// written back at 0x40f2c-0x40f4c). The filter subtracts 4 from the sample pointer itself (0x40e51)
// and indexes it from 1 (0x40ea7, 0x40f24), so the samples written are the caller's [0 .. last-1].
// The reimplementation forwards to that original filter.
int codecHighPassFrame_body(P<float> samples, P<int32_t> last, P<float> state) {
    P<char> p = samples ? pc<char>(samples) - 4 : 0;
    const auto filter = [](const auto &... a) { return ccall(0x00040e30, a...); };
    filter(p + 4, pc<int32_t>(sva(0x00064a18)), last, state);
    return 0;
}
void a_codecHighPassFrame(Ctx &ctx, const uint32_t *a, Ret &r) {
  const CxScope s(ctx);
  r.set_i32(static_cast<int32_t>(neg_u32(NEG(codecHighPassFrame), static_cast<u32>(codecHighPassFrame_body(P<float>(a[0]), P<int32_t>(a[1]), P<float>(a[2]))))));
}

// ---- codecSynthPostFilter (rva 0x3d0a0): from c3bd.cpp ApplyIir2_re
// 0x0003d0a0  applyIir2 (VA 0x3d0a0)
// with three stack arguments ([ebp+8] the samples, in place, [ebp+0xc] the sample count, [ebp+0x10]
// the codec context; `ret` at 0x3d1c2 pops nothing).
// The sample pointer is decremented by 4 bytes ONLY when it is non-null (`je 0x3d0c7` at 0x3d0bc, `sub`
// at 0x3d0c1), so a null pointer with a count below 1 skips the loop untouched. The loop runs from 1 to
// *count inclusive (`jg 0x3d1ba` at 0x3d122, SIGNED).
// Five float state slots are taken from the context: +0xbec, +0xbf0, +0xbf4, +0xbf8 and +0xbfc
// (0x3d0ca-0x3d0ff), called s0..s4 below.
// Each round saves the raw dword of the input sample (0x3d131), then computes
// `(sample - s0 * K0) + s1` with `fld [s0]` / `fmul dword [0x5ebf0]` / `fsubr dword [samples+i*4]` /
// `fadd [s1]` (0x3d137-0x3d14b) and spills it to a float slot (`fstp dword [ebp-0x1c]` at 0x3d14d).
// The output is then `((s2 * K1 + that) - s3 * K2) + s4 * K3` with the constants at 0x5ebec, 0x5ebe8
// and 0x5ebe4 (0x3d153-0x3d176) and is stored back over the sample as a float (0x3d17e).
// The state is finally shifted: s1 = s0, s0 = the saved input, s4 = s3, s3 = s2, s2 = the new sample, all as
// raw dword moves (0x3d181-0x3d1b3). Returns 0 (`xor eax, eax` at 0x3d1ba).
int codecSynthPostFilter_body(P<float> samples, P<int32_t> count, P<char> context) {
    if (samples != 0) samples -= 1;                          // je at 0x3d0bc, sub at 0x3d0c1
    P<char> ctx = pc<char>(context);
    P<float> s0 = pc<float>(ctx + 0xBEC);       // 0x3d0ca
    P<float> s1 = pc<float>(ctx + 0xBF0);       // 0x3d0d6
    P<float> s2 = pc<float>(ctx + 0xBF4);       // 0x3d0e2
    P<float> s3 = pc<float>(ctx + 0xBF8);       // 0x3d0ed
    P<float> s4 = pc<float>(ctx + 0xBFC);       // 0x3d0f9
    const float k0 = rf(0x0005EBF0);
    const float k1 = rf(0x0005EBEC);
    const float k2 = rf(0x0005EBE8);
    const float k3 = rf(0x0005EBE4);

    const int n = *count;                                    // 0x3d105
    for (int i = 1; i <= n; ++i) {
        const unsigned saved = *pc<uint32_t>(&samples[i]);   // 0x3d131
        const float spilled = static_cast<float>(
            (static_cast<double>(samples[i]) - static_cast<double>(*s0) * static_cast<double>(k0))
            + static_cast<double>(*s1));                     // fstp dword at 0x3d14d
        double out = static_cast<double>(*s2) * static_cast<double>(k1) + static_cast<double>(spilled);
        out = out - static_cast<double>(*s3) * static_cast<double>(k2);        // fsubp at 0x3d169
        out = out + static_cast<double>(*s4) * static_cast<double>(k3);        // faddp at 0x3d176
        samples[i] = static_cast<float>(out);                                  // fstp dword at 0x3d17e

        *pc<uint32_t>(s1) = *pc<uint32_t>(s0);   // 0x3d189
        *pc<uint32_t>(s0) = saved;                                    // 0x3d191
        *pc<uint32_t>(s4) = *pc<uint32_t>(s3);   // 0x3d19b
        *pc<uint32_t>(s3) = *pc<uint32_t>(s2);   // 0x3d1a5
        *pc<uint32_t>(s2) = *pc<uint32_t>(&samples[i]);  // 0x3d1b3
    }
    return 0;
}
void a_codecSynthPostFilter(Ctx &ctx, const uint32_t *a, Ret &r) {
  const CxScope s(ctx);
  r.set_i32(static_cast<int32_t>(neg_u32(NEG(codecSynthPostFilter), static_cast<u32>(codecSynthPostFilter_body(P<float>(a[0]), P<int32_t>(a[1]), P<char>(a[2]))))));
}

// ---- lpcSynthesize (rva 0x3d1d0): from c3bn.cpp LpcSynthesize_re

int lpcSynthesize_body(P<float> a, P<int32_t> subLen, P<int32_t> mode, P<float> out,
                             P<float> level, P<float> level2, P<float> feedback, P<char> ctx) {
    const CodecFrame fr(cx(), 0x350);                        // the original's frame, 0xcc-filled (rep stosd at 0x3d1ec)
    const P<float> local = fr.loc<float>(0x2E8);             // the frame array at [ebp + k*4 - 0x2e8]

    if (a) a -= 1;                                           // je at 0x3d1f2
    if (out) out -= 1;                                       // je at 0x3d201

    P<int32_t> prevLen = pc<int32_t>(fldAddr(ctx, 0x688));
    // Both arrays are addressed `[base + k*4 - 4]` (0x3d300, 0x3d374, 0x3d723 and everywhere else),
    // so element k is base[k-1]: the pointers below are the bases less one float.
    P<float> exc = pc<float>(fldAddr(ctx, 0x68C)) - 1;
    P<float> hist = pc<float>(fldAddr(ctx, 0x924)) - 1;
    P<float> d0 = pc<float>(fldAddr(ctx, 0xBBC));
    P<float> d1 = pc<float>(fldAddr(ctx, 0xBC0));
    P<float> d2 = pc<float>(fldAddr(ctx, 0xBC4));
    P<float> e0 = pc<float>(fldAddr(ctx, 0xBC8));
    P<float> e1 = pc<float>(fldAddr(ctx, 0xBCC));
    P<float> e2 = pc<float>(fldAddr(ctx, 0xBD0));
    P<float> prevLevel = pc<float>(fldAddr(ctx, 0xBD4));

    const float c_eps = rf(0x0005EC28);                  // 1e-06f
    const float c_eight = rf(0x0005EC24);                // 8.0f
    const float c_one = rf(0x0005B8A8);                  // 1.0f
    const float c_64 = rf(0x0005EBFC);                   // 64.0f
    const float c_zero = rf(0x0005EC00);                 // 0.0f
    const float c_p75 = rf(0x0005EC04);                  // 0.75f
    const float c_p125 = rf(0x0005EC08);                 // 0.125f
    const float c_m125 = rf(0x0005EBF8);                 // -0.125f
    const float c_p25 = rf(0x0005EBF4);                  // 0.25f
    const float c_2000 = rf(0x0005EC18);                 // 2000.0f
    const float c_342 = rf(0x0005EC1C);                  // 342.0f
    const float c_4 = rf(0x0005EC20);                    // 4.0f
    const double c_6928 = rd(0x0005EC10);                // the qword 6.927999973297119

    P<int32_t> table = pc<int32_t>(sva(0x00064A18));
    const auto comb = [](const auto &... a) { return ccall(0x00040F60, a...); };

    const double ratio = static_cast<double>(*prevLevel)
                       / (static_cast<double>(*level) + static_cast<double>(c_eps));   // 0x3d284-0x3d28f
    const float ratioStored = static_cast<float>(ratio);     // fst dword at 0x3d291
    const float factor = (ratio > static_cast<double>(c_eight)) ? 8.0f : ratioStored;  // je at 0x3d29f

    copy32(prevLevel, level);                           // 0x3d2c2-0x3d2ca

    int g = *g_order();                     // 0x3d2cc
    for (int i = 1; i <= g; ++i)                             // jg at 0x3d2ed
        hist[i] = static_cast<float>(static_cast<double>(factor)
                                     * static_cast<double>(hist[*prevLen + i]));       // 0x3d2fa-0x3d30a
    *prevLen = *subLen;                                      // 0x3d318

    if (*mode != 0) {                                        // jne at 0x3d320
        const float scale = static_cast<float>(
            crt_sqrt(static_cast<double>(*subLen)) / c_6928);                         // 0x3d42f-0x3d445
        int n = *subLen;
        for (int i = 1; i <= n; ++i) {                       // jg at 0x3d46b
            *pc<uint32_t>(&exc[g + i]) = 0;   // 0x3d47d
            if (i <= 0x19)                                   // jg at 0x3d489
                exc[g + i] = static_cast<float>(static_cast<double>(table[i])
                                                * static_cast<double>(scale));         // 0x3d48e-0x3d4a7
            unsigned saved = *pc<uint32_t>(&exc[g + i]);                // 0x3d4b7
            const float t1 = static_cast<float>(
                static_cast<double>(exc[g + i]) * static_cast<double>(c_p125)
                + static_cast<double>(*d0) * static_cast<double>(c_p75));              // 0x3d4cd-0x3d4e4
            const float t2 = static_cast<float>(
                static_cast<double>(*d1) * static_cast<double>(c_p125)
                + static_cast<double>(t1));                                            // 0x3d4ea-0x3d4f5
            exc[g + i] = static_cast<float>(
                static_cast<double>(*d2) * static_cast<double>(c_zero)
                + static_cast<double>(t2));                                            // 0x3d4fb-0x3d511
            copy32(d2, d1);                             // 0x3d51b-0x3d51d
            copy32(d1, d0);                             // 0x3d525-0x3d527
            *pc<uint32_t>(d0) = saved;        // 0x3d532
        }
        n = *subLen;
        for (int i = 1; i <= n; ++i) {                       // jg at 0x3d559
            const int v = comb(ctx);                         // 0x3d563
            local[g + i] = static_cast<float>(
                static_cast<double>(v) * static_cast<double>(c_one)
                / static_cast<double>(c_64));                                          // 0x3d571-0x3d58b
            unsigned saved = *pc<uint32_t>(&local[g + i]);              // 0x3d59b
            const float t1 = static_cast<float>(
                static_cast<double>(local[g + i]) * static_cast<double>(c_m125)
                + static_cast<double>(*e0) * static_cast<double>(c_p25));              // 0x3d5b0-0x3d5ca
            const float t2 = static_cast<float>(
                static_cast<double>(*e1) * static_cast<double>(c_m125)
                + static_cast<double>(t1));                                            // 0x3d5d0-0x3d5db
            local[g + i] = static_cast<float>(
                static_cast<double>(*e2) * static_cast<double>(c_zero)
                + static_cast<double>(t2));                                            // 0x3d5e1-0x3d5f5
            copy32(e2, e1);                             // 0x3d602-0x3d604
            copy32(e1, e0);                             // 0x3d60c-0x3d60e
            *pc<uint32_t>(e0) = saved;        // 0x3d619
        }
        n = *subLen;
        for (int i = 1; i <= n; ++i)                         // jg at 0x3d640
            exc[g + i] = static_cast<float>(static_cast<double>(exc[g + i])
                                            + static_cast<double>(local[g + i]));      // 0x3d656-0x3d66d
    } else {
        int n = *subLen;
        for (int i = 1; i <= n; ++i) {                       // jg at 0x3d346
            const int v = comb(ctx);                         // 0x3d34c
            exc[g + i] = static_cast<float>(v / 64);         // 0x3d354-0x3d374
        }
        const int v2 = comb(ctx);                            // 0x3d37e
        const int pos = ((v2 + 0x8000) * (*subLen - 1)) / 0x10000 + g + 1;             // 0x3d386-0x3d3a8

        const double scaled = static_cast<double>(*level2) / static_cast<double>(c_4)
                              * static_cast<double>(c_one);                            // 0x3d3b5-0x3d3bd
        const double amped = scaled * static_cast<double>(c_342);                      // 0x3d3c6
        float amp = static_cast<float>(amped);               // fst dword at 0x3d3cc
        if (amped > static_cast<double>(c_2000)) amp = 2000.0f;                        // jne at 0x3d3dd
        exc[pos] = static_cast<float>(static_cast<double>(amp)
                                      + static_cast<double>(exc[pos]));                // 0x3d3f2-0x3d405
        exc[pos + 1] = static_cast<float>(static_cast<double>(exc[pos + 1])
                                          - static_cast<double>(amp));                 // 0x3d412-0x3d424
    }

    float energy;
    *pc<uint32_t>(&energy) = 0;               // 0x3d673
    double energyWide = 0.0;                  // mode 2 only
    int n = *subLen;
    for (int i = 1; i <= n; ++i) {                           // jg at 0x3d69a
        const int k = g + i;                                 // 0x3d6a9
        float acc;
        *pc<uint32_t>(&acc) = 0;              // 0x3d6ac
        const int m = g;                                     // 0x3d6b6
        for (int j = 1; j <= m; ++j)                         // jg at 0x3d6d6
            acc = static_cast<float>(static_cast<double>(a[j]) * static_cast<double>(exc[k - j])
                                     + static_cast<double>(acc));                      // 0x3d6e7-0x3d6f4
        acc = static_cast<float>(static_cast<double>(acc) * static_cast<double>(*feedback));
                                                             // 0x3d6ff-0x3d707
        hist[k] = static_cast<float>(static_cast<double>(acc)
                                     + static_cast<double>(exc[k]));                   // 0x3d713-0x3d723
    }
    n = *subLen;
    for (int i = 1; i <= n; ++i) {                           // jg at 0x3d74c
        const int k = g + i;                                 // 0x3d75a
        float acc;
        *pc<uint32_t>(&acc) = 0;              // 0x3d75d
        const int m = g;                                     // 0x3d767
        for (int j = 1; j <= m; ++j)                         // jg at 0x3d788
            acc = static_cast<float>(static_cast<double>(a[j]) * static_cast<double>(hist[k - j])
                                     + static_cast<double>(acc));                      // 0x3d799-0x3d7a6
        hist[k] = static_cast<float>(static_cast<double>(acc)
                                     + static_cast<double>(hist[k]));                  // 0x3d7b4-0x3d7c4
        if (NEG(lpcSynthesize) == 2)                // control: no float spill of the energy sum
            energyWide = static_cast<double>(hist[k]) * static_cast<double>(hist[k]) + energyWide;
        else
            energy = static_cast<float>(static_cast<double>(hist[k]) * static_cast<double>(hist[k])
                                        + static_cast<double>(energy));                // 0x3d7d4-0x3d7df
    }
    for (int i = 1; i <= g; ++i) {                           // jg at 0x3d808
        copy32(&exc[i], &exc[*subLen + i]);             // 0x3d81b-0x3d81f
        copy32(&hist[i], &hist[*subLen + i]);           // 0x3d834-0x3d838
    }

    const float sq = static_cast<float>(static_cast<double>(*level)
                                        * static_cast<double>(*level));                // 0x3d844-0x3d848
    const double product = static_cast<double>(*subLen) * static_cast<double>(sq);     // 0x3d84e-0x3d850
    const float gain = static_cast<float>(
        crt_sqrt(product / (NEG(lpcSynthesize) == 2 ? energyWide : static_cast<double>(energy))));                             // 0x3d859-0x3d86a
    n = *subLen;
    for (int i = 1; i <= n; ++i)                             // jg at 0x3d88d
        out[i] = static_cast<float>(static_cast<double>(gain)
                                    * static_cast<double>(hist[g + i]));               // 0x3d89b-0x3d8a8
    return 0;                                                // 0x3d8ad
}
void a_lpcSynthesize(Ctx &ctx, const uint32_t *a, Ret &r) {
  const CxScope s(ctx);
  r.set_i32(static_cast<int32_t>(neg_u32(NEG(lpcSynthesize), static_cast<u32>(lpcSynthesize_body(P<float>(a[0]), P<int32_t>(a[1]), P<int32_t>(a[2]), P<float>(a[3]), P<float>(a[4]), P<float>(a[5]), P<float>(a[6]), P<char>(a[7]))))));
}

// ---- reflectionToLpc (rva 0x3d8d0): from c3bn.cpp ReflectionToLpc_re
// 0x0003d8d0  reflectionToLpc (VA 0x3d8d0)
// with five stack arguments ([ebp+8] the reflection coefficients, [ebp+0xc] the LPC output, [ebp+0x10]
// the order, [ebp+0x14] an amplitude, [ebp+0x18] the gain cell; `ret` at 0x3da4c pops nothing), a /Od /GZ
// debug body whose frame [ebp-0x80..ebp-1] is filled with 0xcccccccc (`rep stosd` at 0x3d8e9).
// Both arrays are decremented by one float on entry (0x3d8ee, 0x3d8f7), so every index is one-based.
// THE WORK ARRAY ALIASES TWO LOOP SLOTS. It is `[ebp + j*4 - 0x34]` (0x3d9e5), while the outer counter is
// [ebp-0x34] (= slot 0), the inner bound is [ebp-8] (= slot 11) and the cached order is [ebp-4] (= slot 12);
// both of those are re-read on every round of their loop (0x3d9bf, 0x3d998), so an order of 12 or more
// lets the work array overwrite them. The model below keeps all three in one array so that this is reproduced.
// The gain cell is first set to the immediate 1.0f (0x3f800000 at 0x3d900) and then multiplied by
// `1.0f - k[i]*k[i]` for i = 1..*order (`fsubr` against the float 1.0f at 0x5b8a8, `jg 0x3d94c` at
// 0x3d926); it becomes `sqrt(gain) * (double)*[ebp+0x14]` (the amplitude is widened to a qword slot at
// 0x3d951, the call at 0x3d95f, `fmul qword` at 0x3d967) and is stored back as a float (0x3d96d).
// a[1] takes k[1] as a raw dword (0x3d975-0x3d978). For i = 2..*order (`jg 0x3da37` at 0x3d99b) the
// step is the standard Levinson recursion: `tmp[j] = a[j] - k[i]*a[i-j]` for j = 1..i-1 (0x3d9d3-0x3d9e5),
// tmp is copied back over a[1..i-1] as raw dwords (0x3da1b) and a[i] takes k[i], again as a raw dword
// (0x3da2f). Returns 0 (0x3da37).
int reflectionToLpc_body(P<float> k, P<float> a, P<int32_t> order, P<float> amplitude,
                               P<float> gain) {
    const CodecFrame fr(cx(), 0x80);                         // the original's frame, 0xcc-filled (rep stosd at 0x3d8e9)
    const P<uint32_t> fu = fr.loc<uint32_t>(0x34);          // slot j is [ebp - 0x34 + j*4], as a dword
    const P<float> ff = fr.loc<float>(0x34);                 // the same slots as floats

    a -= 1;                                                  // 0x3d8ee
    k -= 1;                                                  // 0x3d8f7
    *pc<uint32_t>(gain) = 0x3F800000u;        // 0x3d900
    const float one = rf(0x0005B8A8);

    fu[12] = static_cast<unsigned>(*order);             // [ebp-4] at 0x3d90b
    for (fu[0] = 1; static_cast<int>(fu[0]) <= static_cast<int>(fu[12]);
         fu[0] = static_cast<unsigned>(static_cast<int>(fu[0]) + 1)) {      // jg at 0x3d926
        const int i = static_cast<int>(fu[0]);
        *gain = static_cast<float>(
            (static_cast<double>(one) - static_cast<double>(k[i]) * static_cast<double>(k[i]))
            * static_cast<double>(*gain));                   // 0x3d934-0x3d948
    }

    const double amp = static_cast<double>(*amplitude);      // fstp qword at 0x3d951
    *gain = static_cast<float>(crt_sqrt(static_cast<double>(*gain)) * amp);   // 0x3d95f-0x3d96d
    copy32(&a[1], &k[1]);                               // 0x3d975-0x3d978

    fu[12] = static_cast<unsigned>(*order);             // 0x3d980
    for (fu[0] = 2; static_cast<int>(fu[0]) <= static_cast<int>(fu[12]);
         fu[0] = static_cast<unsigned>(static_cast<int>(fu[0]) + 1)) {      // jg at 0x3d99b
        const int i = static_cast<int>(fu[0]);
        fu[11] = static_cast<unsigned>(i - 1);          // [ebp-8] at 0x3d9a7
        for (int j = 1; j <= static_cast<int>(fu[11]); ++j)                      // jg at 0x3d9c2
            ff[j] = static_cast<float>(static_cast<double>(a[j])
                                            - static_cast<double>(k[i]) * static_cast<double>(a[i - j]));
        fu[11] = static_cast<unsigned>(i - 1);          // 0x3d9f1
        for (int j = 1; j <= static_cast<int>(fu[11]); ++j)                      // jg at 0x3da0c
            *pc<uint32_t>(&a[j]) = fu[j];                         // 0x3da1b
        copy32(&a[i], &k[i]);                           // 0x3da2f
    }
    return 0;                                                // 0x3da37
}
void a_reflectionToLpc(Ctx &ctx, const uint32_t *a, Ret &r) {
  const CxScope s(ctx);
  r.set_i32(static_cast<int32_t>(neg_u32(NEG(reflectionToLpc), static_cast<u32>(reflectionToLpc_body(P<float>(a[0]), P<float>(a[1]), P<int32_t>(a[2]), P<float>(a[3]), P<float>(a[4]))))));
}

// ---- interpSubframeParams (rva 0x3da50): from c3br.cpp InterpSubframeParams_re

int interpSubframeParams_body(P<int32_t> order, P<int32_t> flags, P<int32_t> subLen, P<float> gain, P<float> k,
                                    P<int32_t> frameLen, P<int32_t> modeOut, P<int32_t> lenOut, P<float> gainOut,
                                    P<float> kOut, P<int32_t> countOut, P<float> ratioOut, P<char> ctx) {
    // The frame array at [ebp + n*4 - 0x50]: slot 0 is the interpolation fraction (`fstp dword [ebp-0x50]` at
    // 0x3dff4) and slots 1..*order hold the saved coefficients (0x3def2). At the order 10 that
    // codecSetParams 0x3aa50 stores, slot 11 would be [ebp-0x24], the log slot, so the two never overlap.
    const CodecFrame fr(cx(), 0xF8);                             // the original's frame, 0xcc-filled (rep stosd at 0x3da6c)
    const P<float> frame = fr.loc<float>(0x50);

    // [ebp-0xc], the block stride: written only inside the `[ebp+0x2c] != 0` test at 0x3da81, so it keeps
    // the frame fill when the coefficient blocks are null (every use of it is then inside a loop that an order
    // of 0 leaves empty).
    int stride = static_cast<int>(0xCCCCCCCCu);
    if (k) k -= 1;                                               // je at 0x3da72
    if (kOut) {                                                  // je at 0x3da81
        stride = *order;                                         // 0x3da86-0x3da88
        kOut -= stride + 1;                                      // 0x3da8b-0x3da9f
    }
    if (flags) flags -= 1;                                       // je at 0x3daa6
    if (modeOut) modeOut -= 1;                                   // je at 0x3dab5
    if (lenOut) lenOut -= 1;                                     // je at 0x3dac4
    if (gainOut) gainOut -= 1;                                   // je at 0x3dad3

    P<int32_t> cPrevFlag = pc<int32_t>(pc<char>(ctx) + 0x64C);     // 0x3dade
    P<int32_t> cPrevLen = pc<int32_t>(pc<char>(ctx) + 0x650);     // 0x3daed
    P<float> cPrevGain = pc<float>(pc<char>(ctx) + 0x654);  // 0x3daf9
    P<float> cPrevK = pc<float>(pc<char>(ctx) + 0x658);   // 0x3db04
    P<int32_t> cCarry = pc<int32_t>(pc<char>(ctx) + 0x680);       // 0x3db13
    P<int32_t> cFirst = pc<int32_t>(pc<char>(ctx) + 0x684);       // 0x3db1f

    const float one = rf(0x0005B8A8);                        // 1.0f
    const float eight = rf(0x0005EC24);                      // 8.0f
    const float half = rf(0x0005EC34);                       // 0.5f
    const float ninety = rf(0x0005EC30);                     // 90.0f
    const float two = rf(0x0005EC2C);                        // 2.0f

    // `fcomp` + `test ah, 1` + `je` takes the store when C0 is set, which is "below or unordered".
    if (!(static_cast<double>(*gain) >= static_cast<double>(one)))
        *pc<uint32_t>(gain) = 0x3F800000u;         // 0x3db2d-0x3db3f
    if (!(static_cast<double>(*cPrevGain) >= static_cast<double>(one)))
        *pc<uint32_t>(cPrevGain) = 0x3F800000u;    // 0x3db48-0x3db5a

    float overrideLen;
    *pc<uint32_t>(&overrideLen) = 0;              // 0x3db60
    *ratioOut = static_cast<float>(static_cast<double>(*gain)
                                   / (static_cast<double>(*cPrevGain)
                                      + static_cast<double>(eight)));   // 0x3db67-0x3db7a

    int samples = 0;
    int accumulated = 0;

    if (*cFirst != 0) {                                          // je at 0x3db82
        samples = 0;                                             // 0x3db88
        const int mode = flags[2];                               // 0x3db92
        if (mode == 0)                                           // jne at 0x3db9c
            *subLen = *frameLen / 4;                             // 0x3db9e-0x3dbaf
        *countOut = div32(*frameLen, *subLen);                   // idiv at 0x3dbba
        *cCarry = *frameLen - (*countOut * *subLen);             // 0x3dbc1-0x3dbd6
        const int entries = *countOut;                           // 0x3dbdb
        for (int i = 1; i <= entries; ++i) {                     // jg at 0x3dbf8
            const int m = *order;                                // 0x3dbfd
            for (int j = 1; j <= m; ++j)                         // jg at 0x3dc1a
                copy32(&kOut[i * stride + j], &k[j]);       // 0x3dc1c-0x3dc34
            modeOut[i] = mode;                                   // 0x3dc39-0x3dc42
            lenOut[i] = *subLen;                                 // 0x3dc45-0x3dc50
            copy32(&gainOut[i], gain);                      // 0x3dc53-0x3dc5e
        }
        *cFirst = 0;                                             // 0x3dc66
    } else {
        bool secondPass = false;                                 // [ebp-0x5c] at 0x3dc71
        samples = *frameLen + *cCarry;                           // 0x3dc78-0x3dc82
        float slope = static_cast<float>(static_cast<double>(*subLen - *cPrevLen)
                                         / static_cast<double>(samples));    // 0x3dc95-0x3dc9e
        *countOut = 0;                                           // 0x3dca4
        accumulated = 0;                                         // 0x3dcaa
        int startIndex = 1;                                      // 0x3dcb1
        int mode;

        if (flags[1] == *cPrevFlag && flags[2] == flags[1]) {    // jne at 0x3dcc9, 0x3dcd7
            if (flags[2] == 0) {                                 // jne at 0x3dce0
                *subLen = *frameLen / 4;                         // 0x3dce2-0x3dcf3
                *cPrevLen = *subLen;                             // 0x3dcf5-0x3dcfd
                if (static_cast<double>(*ratioOut) > static_cast<double>(eight))   // jne at 0x3dd0f
                    copy32(cPrevGain, gain);                // 0x3dd11-0x3dd19
            }
            slope = static_cast<float>(static_cast<double>(*subLen - *cPrevLen)
                                       / static_cast<double>(samples));      // 0x3dd1b-0x3dd34
            mode = flags[2];                                     // 0x3dd37-0x3dd3d
        } else if (*cPrevFlag != 1) {                            // je at 0x3dd4e
            int split;
            if (*cPrevFlag == flags[1])                          // jne at 0x3dd62
                split = samples - (*frameLen / 4);               // 0x3dd64-0x3dd77
            else
                split = samples - (*frameLen * 3 / 4);           // 0x3dd7c-0x3dd92
            lenOut[1] = split / 2;                               // 0x3dd95-0x3dda0
            lenOut[2] = split - lenOut[1];                       // 0x3dda3-0x3ddaf
            modeOut[1] = 0;                                      // 0x3ddb2
            modeOut[2] = 0;                                      // 0x3ddbc
            copy32(&gainOut[1], cPrevGain);                 // 0x3ddc6-0x3ddce
            copy32(&gainOut[2], cPrevGain);                 // 0x3ddd1-0x3ddd9
            const int m = *order;                                // 0x3dddf
            for (int i = 1; i <= m; ++i) {                       // jg at 0x3ddfc
                copy32(&kOut[i + stride], &cPrevK[i - 1]);      // 0x3ddfe-0x3de14
                copy32(&kOut[i + stride * 2], &cPrevK[i - 1]);  // 0x3de17-0x3de30
                copy32(&cPrevK[i - 1], &k[i]);                  // 0x3de33-0x3de45
            }
            *pc<uint32_t>(&slope) = 0;            // 0x3de4b
            *countOut = 2;                                       // 0x3de52
            *cPrevLen = *subLen;                                 // 0x3de5b-0x3de63
            accumulated = split;                                 // 0x3de65
            startIndex = split + 1;                              // 0x3de6b-0x3de71
            mode = 1;                                            // 0x3de77
        } else {
            if (*cPrevFlag == flags[1])                          // je at 0x3de91
                samples = (*frameLen * 3 / 4) + *cCarry;         // 0x3deab-0x3dec1
            else
                samples = (*frameLen / 4) + *cCarry;             // 0x3de93-0x3dea6
            const int m = *order;                                // 0x3dec7
            for (int i = 1; i <= m; ++i) {                       // jg at 0x3dee4
                copy32(&frame[i], &k[i]);                   // 0x3dee6-0x3def2
                copy32(&k[i], &cPrevK[i - 1]);              // 0x3def6-0x3df09
            }
            mode = 1;                                            // 0x3df0e
            *pc<uint32_t>(&slope) = 0;            // 0x3df15
            secondPass = true;                                   // 0x3df1c
        }

        for (;;) {                                               // `mov edx, 1` / `je` at 0x3df2a
            const int limit = samples;                           // 0x3df30
            for (int i = startIndex; i <= limit; ++i) {          // jg at 0x3df50
                // fild prevLen; fild i; fmul slope; faddp; fst (dead); fadd 0.5f; __ftol
                int len = ftol(static_cast<double>(*cPrevLen)
                                    + static_cast<double>(i) * static_cast<double>(slope)
                                    + static_cast<double>(half));             // 0x3df56-0x3df71
                // `test ah, 0x40` after `fcomp` with the float 0.0f at 0x5ec00: the override is used only
                // when it is strictly below or strictly above zero.
                if (overrideLen < 0.0f || overrideLen > 0.0f)                 // jne at 0x3df82
                    len = ftol(static_cast<double>(overrideLen));        // 0x3df84-0x3df8c
                if (len > i - accumulated) continue;                          // jg at 0x3df98
                *countOut += 1;                                               // 0x3df9e-0x3dfa9
                lenOut[*countOut] = len;                                      // 0x3dfab-0x3dfb6
                *subLen = len;                                                // 0x3dfb9-0x3dfbf
                modeOut[*countOut] = mode;                                    // 0x3dfc1-0x3dfcc
                accumulated += len;                                           // 0x3dfcf-0x3dfd5
                frame[0] = static_cast<float>(static_cast<double>(accumulated - len / 2)
                                              / static_cast<double>(samples));  // 0x3dfd8-0x3dff4
                const int m = *order;                                         // 0x3dffa
                for (int j = 1; j <= m; ++j) {                                // jg at 0x3e017
                    const float lo = static_cast<float>(crt_log(
                        (static_cast<double>(cPrevK[j - 1]) + static_cast<double>(one))
                        / (static_cast<double>(one) - static_cast<double>(cPrevK[j - 1]))));
                                                                              // 0x3e01d-0x3e053
                    const double hi = crt_log(
                        (static_cast<double>(k[j]) + static_cast<double>(one))
                        / (static_cast<double>(one) - static_cast<double>(k[j])));
                                                                              // 0x3e056-0x3e081
                    const double mix = (hi - static_cast<double>(lo))
                                       * static_cast<double>(frame[0])
                                       + static_cast<double>(lo);             // 0x3e084-0x3e08d
                    const double e = crt_exp(mix);                           // 0x3e090-0x3e0a1
                    const float eStored = static_cast<float>(e);              // fst dword at 0x3e0a4
                    kOut[*countOut * stride + j] = static_cast<float>(
                        (e - static_cast<double>(one))
                        / (static_cast<double>(eStored) + static_cast<double>(one)));
                                                                              // 0x3e0aa-0x3e0cf
                }
                const double gPrev = crt_log(static_cast<double>(*cPrevGain));   // 0x3e0d7-0x3e0e7
                const double fraction = static_cast<double>(frame[0]);            // 0x3e0f0-0x3e0f3
                const double gNow = crt_log(static_cast<double>(*gain));         // 0x3e0f9-0x3e109
                const double gPrev2 = crt_log(static_cast<double>(*cPrevGain));  // 0x3e112-0x3e122
                gainOut[*countOut] = static_cast<float>((gNow - gPrev2) * fraction + gPrev);
                                                                                  // 0x3e125-0x3e13f
                gainOut[*countOut] = static_cast<float>(
                    crt_exp(static_cast<double>(gainOut[*countOut])));            // 0x3e142-0x3e163
            }
            if (!secondPass) break;                              // jne at 0x3e16f, jmp at 0x3e171
            secondPass = false;                                  // 0x3e176
            startIndex = accumulated + 1;                        // 0x3e17d-0x3e183
            samples = *frameLen + *cCarry;                       // 0x3e189-0x3e193
            *pc<uint32_t>(&slope) = 0;            // 0x3e196
            mode = 0;                                            // 0x3e19d
            const int rest = (samples - startIndex) / 2;         // 0x3e1a4-0x3e1b2
            overrideLen = static_cast<float>(rest);              // fst dword at 0x3e1be
            if (static_cast<double>(rest) > static_cast<double>(ninety))   // jne at 0x3e1cc
                overrideLen = static_cast<float>(static_cast<double>(overrideLen)
                                                 / static_cast<double>(two));   // 0x3e1ce-0x3e1d7
            copy32(cPrevGain, gain);                        // 0x3e1da-0x3e1e2
            const int m = *order;                                // 0x3e1e7
            for (int i = 1; i <= m; ++i) {                       // jg at 0x3e204
                copy32(&k[i], &frame[i]);                   // 0x3e206-0x3e213
                copy32(&cPrevK[i - 1], &frame[i]);          // 0x3e216-0x3e226
            }
        }
        *cCarry = samples - accumulated;                         // 0x3e231-0x3e23a
    }

    if (*countOut != 0) {                                        // je at 0x3e242
        *cPrevFlag = flags[2];                                   // 0x3e244-0x3e250
        *cPrevLen = *subLen;                                     // 0x3e252-0x3e25a
        copy32(cPrevGain, gain);                            // 0x3e25c-0x3e264
        const int m = *order;                                    // 0x3e269
        for (int i = 1; i <= m; ++i)                             // jg at 0x3e286
            copy32(&cPrevK[i - 1], &k[i]);                  // 0x3e288-0x3e29a
    }
    return 0;                                                    // 0x3e2a0
}
void a_interpSubframeParams(Ctx &ctx, const uint32_t *a, Ret &r) {
  const CxScope s(ctx);
  r.set_i32(static_cast<int32_t>(neg_u32(NEG(interpSubframeParams), static_cast<u32>(interpSubframeParams_body(P<int32_t>(a[0]), P<int32_t>(a[1]), P<int32_t>(a[2]), P<float>(a[3]), P<float>(a[4]), P<int32_t>(a[5]), P<int32_t>(a[6]), P<int32_t>(a[7]), P<float>(a[8]), P<float>(a[9]), P<int32_t>(a[10]), P<float>(a[11]), P<char>(a[12]))))));
}

// ---- intPow (rva 0x3e2c0): from c3az.cpp IntPow_re
// 0x0003e2c0  intPow (VA 0x3e2c0)
// with two pointer arguments ([ebp+8] the base, [ebp+0xc] the exponent; `ret` at 0x3e377 pops
// nothing). The base is copied to [ebp-8] (0x3e2dd) and the exponent to [ebp-0xc] (0x3e2e5).
// A positive exponent goes straight to the square-and-multiply loop (`jg 0x3e32f` at 0x3e2ec, SIGNED).
// Otherwise four special cases are tested in order: exponent 0 (`je 0x3e2fa` at 0x3e2f2) and base 1
// (`jne 0x3e301` at 0x3e2f8) both return 1 (0x3e2fa); base -1 negates the exponent (`neg ecx` at
// 0x3e32a) and falls into the loop; base 0 computes `1 / base` with `cdq` + `idiv [ebp-8]` (0x3e312,
// 0x3e313), which is an integer division by zero; any other base returns 0 (0x3e31b).
// The loop keeps the remaining exponent at [ebp-0x10] (0x3e332) and the accumulator at [ebp-4], set to 1
// (0x3e335). Each round multiplies the accumulator by the base when the low bit is set (`and eax, 1` at
// 0x3e33f, `imul` at 0x3e349), shifts the exponent right LOGICALLY (`shr edx, 1` at 0x3e353), stops
// when it reaches 0 (`je 0x3e36a` at 0x3e35c) and otherwise squares the base (`imul` at 0x3e361).
int intPow_body(P<int32_t> basePtr, P<int32_t> expPtr) {
    int b = *basePtr;
    int e = *expPtr;
    if (e <= 0) {                                   // jg at 0x3e2ec
        if (e == 0 || b == 1) return 1;             // je at 0x3e2f2, jne at 0x3e2f8
        if (b == -1) {                              // je at 0x3e305
            e = neg32(e);                           // neg at 0x3e32a
        } else if (b == 0) {                        // jne at 0x3e30b
            return div32(1, b);                     // 0x3e313: `idiv [ebp-8]` by zero raises #DE (DivideError)
        } else {
            return 0;                               // 0x3e31b
        }
    }
    unsigned n = static_cast<unsigned>(e);
    int result = 1;
    for (;;) {
        if (n & 1) result = mul32(result, b);       // je at 0x3e344
        n >>= 1;                                    // shr at 0x3e353
        if (n == 0) break;                          // je at 0x3e35c
        b = mul32(b, b);                            // imul at 0x3e361
    }
    return result;
}
void a_intPow(Ctx &ctx, const uint32_t *a, Ret &r) {
  const CxScope s(ctx);
  r.set_i32(static_cast<int32_t>(neg_u32(NEG(intPow), static_cast<u32>(intPow_body(P<int32_t>(a[0]), P<int32_t>(a[1]))))));
}

// ---- floatCopySign (rva 0x3e380): from c3au.cpp FloatCopySign_re
// 0x0003e380  floatCopySign (VA 0x3e380)
// with two stack arguments ([ebp+8] at 0x3e398 and [ebp+0xc] at 0x3e3c4, `ret` at 0x3e3f5
// pops nothing), both pointers to a float; returns a double in st(0) (`fld qword [ebp-0x14]` at 0x3e3ec).
// A /Od /GZ debug body, so every intermediate is spilled and the rounding points are visible.
// The first float is compared with the dword at 0x5ec00, which is 0.0f in the image (bytes 00 00 00 00 at
// RVA 0x5ec00): `fld dword [eax]` at 0x3e39b, `fcomp dword` at 0x3e39d, `fnstsw ax` + `test ah, 1` +
// `jne 0x3e3b4` at 0x3e3a3-0x3e3a8. ah bit 0 is the x87 C0 flag, which fcomp sets when st(0) is below
// the operand AND when the comparison is unordered (either operand a NaN), so a negative value and a NaN both
// take the jump and are negated into the float slot [ebp-0xc] (`fchs` at 0x3e3b9, `fstp dword` at
// 0x3e3bb); otherwise the argument's four bytes are copied into that slot unchanged (0x3e3ad), which
// keeps -0.0 as -0.0 because it compares equal to 0.0f and C0 stays clear.
// That float is widened to the double slot [ebp-8] (`fld dword` / `fstp qword` at 0x3e3be, exact). The
// second float is tested the same way against 0.0f (0x3e3c7-0x3e3d4): below zero or unordered, the
// double is negated into [ebp-0x14] (`fchs` at 0x3e3e7), otherwise it is copied there as two dwords
// (0x3e3d6-0x3e3df). Every step is a sign flip or an exact widening, so no rounding mode can change the
// result.
double floatCopySign_body(P<float> mag, P<float> sign) {
    float a = *mag;
    float m = !(a >= 0.0f) ? -a : a;             // C0 set = below zero or unordered
    double r = static_cast<double>(m);
    float s = *sign;
    if (!(s >= 0.0f)) r = -r;
    return r;
}
void a_floatCopySign(Ctx &ctx, const uint32_t *a, Ret &r) {
  const CxScope s(ctx);
  double v = floatCopySign_body(P<float>(a[0]), P<float>(a[1]));
  if (NEG(floatCopySign) == 1) v += 1.0;
  uint64_t bits;
  std::memcpy(&bits, &v, 8);
  x87::unit(ctx).push(x87::fld64(bits));   // `fld qword [ebp-0x14]` (0x3e3ec): the result stays in st(0) for the caller
  r.set_double(v);
}

// ---- floatRoundHalfAway (rva 0x3e400): from c3az.cpp FloatFloorToInt_re
// 0x0003e400  floatFloorToInt (VA 0x3e400)
// with one pointer argument ([ebp+8]; `ret` at 0x3e47e pops nothing), a /Od /GZ debug body that
// rounds a float half away from zero and returns it as an int.
// The sign test is `fld dword [arg]` / `fcomp dword [0x5ec00]` / `fnstsw ax` / `test ah, 1` / `jne 0x3e448`
// (0x3e41b-0x3e428): the constant at 0x5ec00 is 0.0f in the image (bytes 00 00 00 00 at RVA 0x5ec00)
// and bit 0 of ah is C0, which the compare sets for "below" and for "unordered", so the jump is taken when the
// value is below zero or is a NaN.
// The non-negative arm adds the double 0.5 at 0x5b820 (bytes 00 00 00 00 00 00 e0 3f at RVA 0x5b820) with
// `fadd qword` (0x3e42f), pushes the sum as a double and calls the CRT's floor at 0x4339c (0x3e43b).
// The negative arm computes 0.5 - value with `fsubr qword` (0x3e44d), floors that and negates the result
// (`fchs` at 0x3e461). Either way the double is stored at [ebp-8] and converted by the CRT's __ftol at
// 0x42550 (0x3e469), which does a 64-bit `fistp` and keeps the low dword.
// The reimplementation calls the original floor at its own address, reads both constants out of the module and
// reproduces __ftol as (int)(long long).
int floatRoundHalfAway_body(P<float> p) {
    const float zero = *pc<float>(sva(0x0005ec00));    // 0.0f
    const double half = *pc<double>(sva(0x0005b820));  // 0.5
    const float x = *p;
    double d;
    if (!(x >= zero))                                   // C0 set: below or unordered (0x3e425)
        d = -crt_floor(half - static_cast<double>(x));  // fsubr at 0x3e44d, floor 0x4339c, fchs at 0x3e461
    else
        d = crt_floor(static_cast<double>(x) + half);   // fadd at 0x3e42f, floor 0x4339c
    return ftol(d);                                     // __ftol at 0x42550
}
void a_floatRoundHalfAway(Ctx &ctx, const uint32_t *a, Ret &r) {
  const CxScope s(ctx);
  r.set_i32(static_cast<int32_t>(neg_u32(NEG(floatRoundHalfAway), static_cast<u32>(floatRoundHalfAway_body(P<float>(a[0]))))));
}

// ---- medianOfThree (rva 0x3e480): from c3az.cpp MedianOfThree_re
// 0x0003e480  medianOfThree (VA 0x3e480)
// with three pointer arguments ([ebp+8], [ebp+0xc], [ebp+0x10]; `ret` at 0x3e510 pops nothing), a
// /Od /GZ debug body whose only live local is the result at [ebp-4]. The result starts as *b (0x3e49b,
// 0x3e49d) and every compare is SIGNED.
// If *b is above BOTH *a (`jle 0x3e4d6` at 0x3e4aa) and *c (`jle 0x3e4d6` at 0x3e4b6), the result
// becomes *a (0x3e4bb) and then *c when *c is above *a (`jle 0x3e4d4` at 0x3e4ca, 0x3e4cf): the
// larger of *a and *c.
// Otherwise, if *b is below BOTH *a (`jge 0x3e50a` at 0x3e4e0) and *c (`jge 0x3e50a` at 0x3e4ec),
// the result becomes *a (0x3e4f1) and then *c when *c is below *a (`jge 0x3e50a` at 0x3e500,
// 0x3e505): the smaller of *a and *c. In every other case the result stays *b (0x3e50a).
int medianOfThree_body(P<int32_t> a, P<int32_t> b, P<int32_t> c) {
    int result = *b;
    if (*b > *a && *b > *c) {               // jle at 0x3e4aa, jle at 0x3e4b6
        result = *a;
        if (*c > *a) result = *c;           // jle at 0x3e4ca
    } else if (*b < *a && *b < *c) {        // jge at 0x3e4e0, jge at 0x3e4ec
        result = *a;
        if (*c < *a) result = *c;           // jge at 0x3e500
    }
    return result;
}
void a_medianOfThree(Ctx &ctx, const uint32_t *a, Ret &r) {
  const CxScope s(ctx);
  r.set_i32(static_cast<int32_t>(neg_u32(NEG(medianOfThree), static_cast<u32>(medianOfThree_body(P<int32_t>(a[0]), P<int32_t>(a[1]), P<int32_t>(a[2]))))));
}

// ---- codecCorrectNibble (rva 0x3e520): from c3bd.cpp DecodeTableSymbol_re
// 0x0003e520  decodeTableSymbol (VA 0x3e520)
// with three pointer arguments ([ebp+8] the packed symbol, [ebp+0xc] the decoded value slot,
// [ebp+0x10] a counter; `ret` at 0x3e5f9 pops nothing), a /Od /GZ debug body.
// The symbol's LOW BYTE is folded to its parity in [ebp-0xc]: `and ecx, 0xff` (0x3e53d), then three
// rounds of `x ^= x / 2^k` with k = 4, 2, 1 written as SIGNED divisions (`cdq` + `and edx, 0xf` + `sar 4` at
// 0x3e549-0x3e54f, `and edx, 3` + `sar 2` at 0x3e55e-0x3e563, `sub eax, edx` + `sar 1` at
// 0x3e572-0x3e574), and finally `and eax, 1` (0x3e581). The value is non-negative at every step, so
// each division is the plain shift.
// The symbol's LOW SEVEN BITS index the 128-entry dword table at 0x64a80 (`and edx, 0x7f` at 0x3e58c,
// `mov eax, [edx*4 + 0x64a80]` at 0x3e58f). The entry's low nibble is written to the value slot
// (`and ecx, 0xf` at 0x3e59c, store at 0x3e5a2) and its bit 4 selects the counter arm
// (`and eax, 0x10` at 0x3e5a7, `je 0x3e5c8` at 0x3e5b1).
// With bit 4 SET the counter is incremented only when the parity is 1 (`je 0x3e5c6` at 0x3e5b7,
// increment at 0x3e5be). With bit 4 CLEAR the counter is incremented unconditionally (0x3e5cd) and,
// when the parity is 0 (`jne 0x3e5f1` at 0x3e5d9), incremented a second time (0x3e5e0) and the value
// slot is overwritten with -1 (0x3e5eb). Returns 0 (`xor eax, eax` at 0x3e5f1).
int codecCorrectNibble_body(P<int32_t> symbol, P<int32_t> value, P<int32_t> counter) {
    int parity = *symbol & 0xFF;                 // and at 0x3e53d
    parity ^= parity / 16;                       // sar 4 at 0x3e54f
    parity ^= parity / 4;                        // sar 2 at 0x3e563
    parity ^= parity / 2;                        // sar 1 at 0x3e574
    parity &= 1;                                 // and at 0x3e581

    P<uint32_t> table = pc<uint32_t>(sva(0x00064A80));
    const int entry = static_cast<int>(table[*symbol & 0x7F]);   // 0x3e58f
    *value = entry & 0xF;                        // 0x3e5a2
    if ((entry & 0x10) != 0) {                   // je at 0x3e5b1
        if (parity != 0) *counter += 1;          // je at 0x3e5b7, add at 0x3e5be
    } else {
        *counter += 1;                           // add at 0x3e5cd
        if (parity == 0) {                       // jne at 0x3e5d9
            *counter += 1;                       // add at 0x3e5e0
            *value = -1;                         // 0x3e5eb
        }
    }
    return 0;
}
void a_codecCorrectNibble(Ctx &ctx, const uint32_t *a, Ret &r) {
  const CxScope s(ctx);
  r.set_i32(static_cast<int32_t>(neg_u32(NEG(codecCorrectNibble), static_cast<u32>(codecCorrectNibble_body(P<int32_t>(a[0]), P<int32_t>(a[1]), P<int32_t>(a[2]))))));
}

// ---- codecRevertUnstableCoeffs (rva 0x3e600): from c3bd.cpp SubstituteIfBelowThreshold_re
// 0x0003e600  substituteIfBelowThreshold (VA 0x3e600)
// with three pointer arguments ([ebp+8] the sample count, [ebp+0xc] the replacement samples,
// [ebp+0x10] the samples that are scanned AND overwritten; `ret` at 0x3e6ce pops nothing).
// Both sample pointers are decremented by 4 bytes on entry (0x3e61b, 0x3e627) and both loops run from 1
// to *count inclusive (`jg 0x3e68c` at 0x3e64a, `jg 0x3e6c6` at 0x3e6b0, SIGNED), so element i of
// a loop is element i-1 of the caller's array.
// The scan takes the magnitude of each scanned sample: `fcomp dword [0x5ec00]` (0x3e65b, the float 0.0f
// in the module's .rdata) with `test ah, 1` (0x3e663) tests the C0 flag, so a sample BELOW zero is negated
// (`fchs` at 0x3e673, spilled to a float slot at 0x3e675) and any other sample is copied as a raw dword
// (0x3e668-0x3e66b). That magnitude is then compared with `fcomp dword [0x5ebd4]` (0x3e67b, the
// float 0.99f) and `test ah, 0x41` (0x3e683) tests C3|C0, so the loop CONTINUES while the magnitude is at
// or below the limit and leaves it (0x3e688) as soon as one sample is strictly above the limit.
// If no sample is above the limit the function returns 0 without writing anything (0x3e68c).
// Otherwise the count is re-read through its pointer (0x3e693) and the scanned array is overwritten
// element by element with the replacement array as raw dwords (`mov` at 0x3e6c1). Returns 0 (0x3e6c6).
int codecRevertUnstableCoeffs_body(P<int32_t> count, P<float> replacement, P<float> samples) {
    samples -= 1;                                            // sub at 0x3e61b
    replacement -= 1;                                        // sub at 0x3e627
    const float zero = rf(0x0005EC00);
    const float limit = rf(0x0005EBD4);

    int n = *count;
    bool above = false;
    for (int i = 1; i <= n; ++i) {
        const float v = samples[i];                          // raw dword copy at 0x3e652
        float magnitude;
        if (static_cast<double>(v) < static_cast<double>(zero)) magnitude = -v;   // fchs at 0x3e673
        else magnitude = v;                                                       // 0x3e668
        if (static_cast<double>(magnitude) > static_cast<double>(limit)) {        // test ah, 0x41 at 0x3e683
            above = true;
            break;
        }
    }
    if (!above) return 0;                                    // 0x3e68c

    n = *count;                                              // re-read at 0x3e693
    for (int i = 1; i <= n; ++i)
        *pc<uint32_t>(&samples[i]) = *pc<uint32_t>(&replacement[i]);
    return 0;
}
void a_codecRevertUnstableCoeffs(Ctx &ctx, const uint32_t *a, Ret &r) {
  const CxScope s(ctx);
  r.set_i32(static_cast<int32_t>(neg_u32(NEG(codecRevertUnstableCoeffs), static_cast<u32>(codecRevertUnstableCoeffs_body(P<int32_t>(a[0]), P<float>(a[1]), P<float>(a[2]))))));
}

// ---- codecLdlSolveClamped (rva 0x3e6d0): from c3bi.cpp GaussSolveClamped_re
// 0x0003e6d0  gaussSolveClamped (VA 0x3e6d0)
// with four stack arguments ([ebp+8] the order, [ebp+0xc] the packed matrix, [ebp+0x10] the right-hand
// side, [ebp+0x14] the solution; `ret` at 0x3e9ef pops nothing), a /Od /GZ debug body.
// The solution and the right-hand side are decremented by one float (0x3e6f1, 0x3e6fa) and the matrix by
// `order + 1` floats (0x3e70b-0x3e71c), so every index is one-based. The work matrix is the frame slot at
// [ebp-0x1e8], indexed `row * 10 + column` (`imul eax, eax, 0xa` at 0x3e7b6 and everywhere else), so the
// order never exceeds 10.
// For each row i = 1..*order (`jg 0x3e9b3` at 0x3e73f) the row is first copied out of the packed matrix,
// `a[i][j] = matrix[j + i * order]` for j = i..*order (0x3e766-0x3e783, the row stride is the order read
// at entry, 0x3e703). Rows 1..i-1 are then eliminated: the factor `a[k][i] * a[k][k]` is spilled to a float
// (0x3e7c9-0x3e7d7) and `a[i][j] -= factor * a[k][j]` runs over j = i..*order (0x3e811-0x3e82d).
// The pivot a[i][i] is rejected when its magnitude is at or below the float 1e-10 at 0x5ec40 (`fcomp` at
// 0x3e87c, `test ah, 1` / `je 0x3e88e` at 0x3e884, the magnitude taken with `fchs` at 0x3e86e when
// the pivot is below the float 0.0f at 0x5ec00): the solution is then zeroed from the current row to the end
// (0x3e9d8-0x3e9de) and the function returns 0 (0x3e9e7).
// Otherwise the right-hand side is copied into the solution as a raw dword (0x3e89d), the rows already
// solved are back-substituted, `out[i] -= out[k] * a[k][i]` for k = 1..i-1 (0x3e8d4-0x3e8ed), the pivot is
// inverted in place with the float 1.0f at 0x5b8a8 (`fdiv` at 0x3e903, stored at 0x3e915), the
// solution is scaled by it (0x3e92d-0x3e93d) and finally clamped: strictly above the float 0.9990000f at
// 0x5ec3c it becomes the dword 0x3f7fbe77 (`test ah, 0x41` / `je 0x3e967` at 0x3e957), and strictly
// below the float -0.9990000f at 0x5ec38 it becomes 0xbf7fbe77 (`test ah, 1` / `jne 0x3e995` at
// 0x3e985). Returns 0 (0x3e9b3).
int codecLdlSolveClamped_body(P<int32_t> order, P<float> matrix, P<float> rhs, P<float> out) {
    const CodecFrame fr(cx(), 0x208);                        // the original's frame, 0xcc-filled (rep stosd at 0x3e6ec)
    const P<float> a = fr.loc<float>(0x1E8);                 // the frame array at [ebp-0x1e8]

    out -= 1;                                                // 0x3e6f1
    rhs -= 1;                                                // 0x3e6fa
    const int stride = *order;                               // 0x3e703
    matrix -= (stride + 1);                                  // 0x3e70b-0x3e71c

    const float zero = rf(0x0005EC00);
    const float pivotFloor = rf(0x0005EC40);
    const float one = rf(0x0005B8A8);
    const float upper = rf(0x0005EC3C);
    const float lower = rf(0x0005EC38);

    const int n = *order;                                    // 0x3e722
    for (int i = 1; i <= n; ++i) {                           // jg at 0x3e73f
        int m = *order;                                      // 0x3e748
        for (int j = i; j <= m; ++j)                         // jg at 0x3e764
            a[j + i * 10] = matrix[j + i * stride];          // 0x3e783

        m = i - 1;                                           // 0x3e78f
        for (int k = 1; k <= m; ++k) {                       // jg at 0x3e7ad
            const float factor = static_cast<float>(
                static_cast<double>(a[i + k * 10]) * static_cast<double>(a[k + k * 10]));  // 0x3e7d7
            const int inner = *order;                        // 0x3e7dd
            for (int j = i; j <= inner; ++j)                 // jg at 0x3e7f9
                a[j + i * 10] = static_cast<float>(
                    static_cast<double>(a[j + i * 10])
                    - static_cast<double>(factor) * static_cast<double>(a[j + k * 10]));   // 0x3e82d
        }

        const float pivot = a[i + i * 10];                   // 0x3e846
        const float magnitude = (static_cast<double>(pivot) < static_cast<double>(zero))
                                    ? -pivot : pivot;        // fchs at 0x3e86e
        if (!(static_cast<double>(magnitude) >= static_cast<double>(pivotFloor))) {   // je at 0x3e887
            const int tail = *order;                         // 0x3e9ba
            for (int j = i; j <= tail; ++j)                  // jg at 0x3e9d6
                *pc<uint32_t>(&out[j]) = 0;   // 0x3e9de
            return 0;                                        // 0x3e9e7
        }

        *pc<uint32_t>(&out[i]) = *pc<uint32_t>(&rhs[i]);  // 0x3e89d
        m = i - 1;                                           // 0x3e8a3
        for (int k = 1; k <= m; ++k)                         // jg at 0x3e8c1
            out[i] = static_cast<float>(
                static_cast<double>(out[i])
                - static_cast<double>(out[k]) * static_cast<double>(a[i + k * 10]));   // 0x3e8ed
        a[i + i * 10] = static_cast<float>(
            static_cast<double>(one) / static_cast<double>(a[i + i * 10]));            // 0x3e915
        out[i] = static_cast<float>(
            static_cast<double>(out[i]) * static_cast<double>(a[i + i * 10]));         // 0x3e93d

        float high = out[i];                                 // 0x3e946
        if (static_cast<double>(high) > static_cast<double>(upper))                    // je at 0x3e95a
            *pc<uint32_t>(&high) = 0x3F7FBE77u;                         // 0x3e967
        float low = high;                                    // 0x3e977
        if (static_cast<double>(low) < static_cast<double>(lower))                     // jne at 0x3e988
            *pc<uint32_t>(&low) = 0xBF7FBE77u;                          // 0x3e995
        out[i] = low;                                                                  // 0x3e9ab
    }
    return 0;                                                                          // 0x3e9b3
}
void a_codecLdlSolveClamped(Ctx &ctx, const uint32_t *a, Ret &r) {
  const CxScope s(ctx);
  r.set_i32(static_cast<int32_t>(neg_u32(NEG(codecLdlSolveClamped), static_cast<u32>(codecLdlSolveClamped_body(P<int32_t>(a[0]), P<float>(a[1]), P<float>(a[2]), P<float>(a[3]))))));
}

// ---- codecCovarianceMatrix (rva 0x3e9f0): from c3bd.cpp CodecAutocorrelation_re
// 0x0003e9f0  codecAutocorrelation (VA 0x3e9f0)
// with six stack arguments ([ebp+8] the order, [ebp+0xc] the first index, [ebp+0x10] the last index,
// [ebp+0x14] the samples, [ebp+0x18] an order x order matrix, [ebp+0x1c] the lag vector; `ret` at 0x3ec58
// pops nothing).
// On entry the lag vector is decremented by 4 bytes (0x3ea0b), the order is copied to [ebp-4] (0x3ea16),
// the matrix is decremented by `(order + 1) * 4` bytes (0x3ea19-0x3ea2d) and the samples by 4 bytes
// (0x3ea33), and [ebp-0x20] takes `*first + *order` (0x3ea39-0x3ea43), called S below. Every index
// in the four loops is therefore one-based.
// Loop A (i = 1..*order, `jg 0x3eacb` at 0x3ea66) zeroes matrix[i + order] (0x3ea71) and accumulates
// `x[j-1] * x[j-i]` over j = S..*last inclusive (`jg 0x3eac9` at 0x3ea97), spilling to a float each
// round (0x3eaae-0x3eac4).
// Loop B zeroes lag[*order] (0x3ead3) and accumulates `x[j] * x[j - *order]` over the same j range
// (`jg 0x3eb2a` at 0x3eaf9, body 0x3eafb-0x3eb25).
// Loop C (i = 2..*order, `jg 0x3ebdc` at 0x3eb4a; k = 2..i, `jg 0x3ebd7` at 0x3eb6e) writes
// matrix[i + k*order] = `(matrix[i + (k-1)*order - 1] - x[*last+1-i] * x[*last+1-k]) + x[S-i] * x[S-k]`
// (0x3eb70-0x3ebd2).
// Loop D (k = 1..*order-1, `jg 0x3ec50` at 0x3ebff) writes
// lag[k] = `(matrix[k + order + 1] - x[S-1] * x[S-1-k]) + x[*last] * x[*last-k]` (0x3ec01-0x3ec4b).
// Returns 0 (`xor eax, eax` at 0x3ec50).
int codecCovarianceMatrix_body(P<int32_t> order, P<int32_t> first, P<int32_t> last, P<float> x,
                                    P<float> matrix, P<float> lag) {
    lag -= 1;                                                // sub at 0x3ea0b
    const int m = *order;                                    // 0x3ea16
    matrix -= (m + 1);                                       // 0x3ea22-0x3ea2d
    x -= 1;                                                  // sub at 0x3ea33
    const int s = *first + *order;                           // 0x3ea39-0x3ea43

    int outer = *order;                                      // 0x3ea4b
    for (int i = 1; i <= outer; ++i) {                       // jg at 0x3ea66
        matrix[i + m] = 0.0f;                                // 0x3ea71
        const int bound = *last;                             // 0x3ea7b
        for (int j = s; j <= bound; ++j)                     // jg at 0x3ea97
            matrix[i + m] = static_cast<float>(
                static_cast<double>(x[j - 1]) * static_cast<double>(x[j - i])
                + static_cast<double>(matrix[i + m]));       // fstp at 0x3eac4
    }

    lag[*order] = 0.0f;                                      // 0x3ead3
    outer = *last;                                           // 0x3eadd
    for (int j = s; j <= outer; ++j)                         // jg at 0x3eaf9
        lag[*order] = static_cast<float>(
            static_cast<double>(x[j]) * static_cast<double>(x[j - *order])
            + static_cast<double>(lag[*order]));             // fstp at 0x3eb25

    outer = *order;                                          // 0x3eb2d
    for (int i = 2; i <= outer; ++i) {                       // jg at 0x3eb4a
        for (int k = 2; k <= i; ++k) {                       // jg at 0x3eb6e
            const int src = i + (k - 1) * m - 1;             // lea at 0x3eb7d
            double t = static_cast<double>(matrix[src])
                       - static_cast<double>(x[*last + 1 - i]) * static_cast<double>(x[*last + 1 - k]);
            t = t + static_cast<double>(x[s - i]) * static_cast<double>(x[s - k]);   // faddp at 0x3ebc1
            matrix[i + k * m] = static_cast<float>(t);       // fstp at 0x3ebd2
        }
    }

    outer = *order - 1;                                      // 0x3ebe1
    for (int k = 1; k <= outer; ++k) {                       // jg at 0x3ebff
        double t = static_cast<double>(matrix[k + m + 1])
                   - static_cast<double>(x[s - 1]) * static_cast<double>(x[s - 1 - k]);
        t = t + static_cast<double>(x[*last]) * static_cast<double>(x[*last - k]);   // faddp at 0x3ec43
        lag[k] = static_cast<float>(t);                      // fstp at 0x3ec4b
    }
    return 0;
}
void a_codecCovarianceMatrix(Ctx &ctx, const uint32_t *a, Ret &r) {
  const CxScope s(ctx);
  r.set_i32(static_cast<int32_t>(neg_u32(NEG(codecCovarianceMatrix), static_cast<u32>(codecCovarianceMatrix_body(P<int32_t>(a[0]), P<int32_t>(a[1]), P<int32_t>(a[2]), P<float>(a[3]), P<float>(a[4]), P<float>(a[5]))))));
}

// ---- codecRmsEnergy (rva 0x3ec60): from c3az.cpp CodecRmsEnergy_re
// 0x0003ec60  codecRmsEnergy (VA 0x3ec60)
// with three stack arguments ([ebp+8] the sample count, [ebp+0xc] the samples, [ebp+0x10] the
// accumulator slot; `ret` at 0x3ecf9 pops nothing). The sample pointer is decremented by 4 bytes
// (0x3ec7b) and the loop runs from 1 to *count inclusive (`jg 0x3ecca` at 0x3ecaa, SIGNED), so
// element i is the caller's element i-1.
// The accumulator is zeroed through its own pointer (`mov dword [ecx], 0` at 0x3ec84) and each round adds
// one squared sample to it: `fld dword [buf+i*4]` / `fmul dword [buf+i*4]` / `fadd dword [out]` /
// `fstp dword [out]` (0x3ecb8-0x3ecc6), so the square and the sum round at 53 bits and the spill rounds
// to float. Afterwards `fild dword [count]` (0x3eccd, the count re-read and converted as a SIGNED 32-bit
// integer) and `fdivr dword [out]` (0x3ecd2) leave accumulator / count, which is pushed as a double
// (`fstp qword [esp]` at 0x3ecd7) to the CRT's sqrt at 0x43204 (0x3ecda); the result is stored back
// as a float (0x3ece5). The reimplementation calls that original sqrt at its own address.
// Returns 0 (`xor eax, eax` at 0x3ece7).
int codecRmsEnergy_body(P<int32_t> count, P<float> buf, P<float> out) {
    buf -= 1;                                       // sub at 0x3ec7b
    *out = 0.0f;                                    // 0x3ec84
    const int n = *count;
    for (int i = 1; i <= n; ++i)
        *out = static_cast<float>(static_cast<double>(buf[i]) * static_cast<double>(buf[i])
                                  + static_cast<double>(*out));
    *out = static_cast<float>(crt_sqrt(static_cast<double>(*out) / static_cast<double>(*count)));   // sqrt 0x43204
    return 0;
}
void a_codecRmsEnergy(Ctx &ctx, const uint32_t *a, Ret &r) {
  const CxScope s(ctx);
  r.set_i32(static_cast<int32_t>(neg_u32(NEG(codecRmsEnergy), static_cast<u32>(codecRmsEnergy_body(P<int32_t>(a[0]), P<float>(a[1]), P<float>(a[2]))))));
}

// ---- codecRemoveMean (rva 0x3ed00): from c3az.cpp CodecRemoveMean_re
// 0x0003ed00  codecRemoveMean (VA 0x3ed00)
// with three stack arguments ([ebp+8] the sample count, [ebp+0xc] the input samples, [ebp+0x10] the
// output samples; `ret` at 0x3edb0 pops nothing). Both sample pointers are decremented by 4 bytes
// (0x3ed1b, 0x3ed24) and both loops run from 1 to *count inclusive (`jg` at 0x3ed51 and 0x3ed8f,
// SIGNED), so element i is the caller's element i-1.
// The accumulator at [ebp-8] starts as the integer 0 written into a float slot (0x3ed2a). The first loop
// adds each sample to it and spills it back as a float (`fld dword [ebp-8]` / `fadd dword [in+i*4]` /
// `fstp dword [ebp-8]` at 0x3ed59-0x3ed5f). The mean then replaces it: `fild dword [count]`
// (0x3ed67, the count re-read through the pointer and converted as a SIGNED 32-bit integer),
// `fdivr dword [ebp-8]` (0x3ed69, so accumulator / count) and `fstp dword [ebp-8]` (0x3ed6c).
// The count is re-read for the second loop (0x3ed72), which writes in[i] - mean to out[i]
// (`fld` / `fsub dword [ebp-8]` / `fstp dword [out+i*4]` at 0x3ed97-0x3eda3). Every arithmetic step
// rounds at 53 bits and every spill rounds to float. Returns 0 (`xor eax, eax` at 0x3eda8).
int codecRemoveMean_body(P<int32_t> count, P<float> in, P<float> out) {
    out -= 1;                                       // sub at 0x3ed1b
    in -= 1;                                        // sub at 0x3ed24
    float acc = 0.0f;
    int n = *count;
    if (NEG(codecRemoveMean) == 2) {                // control: the sum is never spilled to its float slot
        double wide = 0.0;
        for (int i = 1; i <= n; ++i) wide += static_cast<double>(in[i]);
        acc = static_cast<float>(wide / static_cast<double>(*count));
    } else {
        for (int i = 1; i <= n; ++i)
            acc = static_cast<float>(static_cast<double>(acc) + static_cast<double>(in[i]));
        acc = static_cast<float>(static_cast<double>(acc) / static_cast<double>(*count));
    }
    n = *count;
    for (int i = 1; i <= n; ++i)
        out[i] = static_cast<float>(static_cast<double>(in[i]) - static_cast<double>(acc));
    return 0;
}
void a_codecRemoveMean(Ctx &ctx, const uint32_t *a, Ret &r) {
  const CxScope s(ctx);
  r.set_i32(static_cast<int32_t>(neg_u32(NEG(codecRemoveMean), static_cast<u32>(codecRemoveMean_body(P<int32_t>(a[0]), P<float>(a[1]), P<float>(a[2]))))));
}

// ---- codecAlignAnalysisWindow (rva 0x3edc0): from c3ca.cpp CodecAlignAnalysisWindow_re
// 0x0003edc0  codecAlignAnalysisWindow (VA 0x3edc0)
// with nine stack arguments, all pointers to int (`add esp, 0x80` + `ret` at 0x3f2f5-0x3f305
// pops nothing), a /Od /GZ debug body whose frame [ebp-0x80..ebp-1] is filled with 0xcccccccc (`rep stosd`,
// ecx = 0x20, at 0x3edd9). The frame is not modelled: every write of the body goes to one of its
// arguments, and its only callee (floatFloorToInt 0x3e400) reads the float cell it is handed and writes
// nothing.
//   [ebp+8]    the alignment step; at the live call site it is one dword of the table 0x64910 selected by
//              the gain index at 0x3ce57 (that table is 1, 20, 21, 22, ... 34)
//   [ebp+0xc]  the flag record, moved back one dword at 0x3edf9
//   [ebp+0x10] a mode code, compared against 0, 1, 2 and 3 (0x3ef3d, 0x3f01f, 0x3f087, 0x3f08f,
//              0x3f26c); the live caller passes the cell the other stage below writes (+0x20e8)
//   [ebp+0x14] the frame index n
//   [ebp+0x18] the previous bounds record, moved back three dwords at 0x3edf0 (live: +0x20ec)
//   [ebp+0x1c] the bounds record this body writes, moved back three dwords at 0x3ede7 (live: +0x2104)
//   [ebp+0x20] the second bounds record this body writes, moved back three dwords at 0x3edde (live: a
//              block of codecStageDispatch's own frame, read back at 0x3cef6-0x3cf02)
//   [ebp+0x24] the frame length
//   [ebp+0x28] the window length
// The body computes the limits lo = (n - 2) * frameLen + 1 (0x3ee02-0x3ee10) and hi = n * frameLen
// (0x3ee13-0x3ee1e), reduces the five flags to `allSet` (every one of them equal to 1; the chain of
// four `je`/`jne` pairs at 0x3ee3a-0x3eeef) and `anySet` (either flag of frame n equal to 1,
// 0x3eef2-0x3ef27), and then takes one of two paths (0x3ef2a, 0x3ef34, 0x3ef40):
//   * align: the window start is pulled onto the lattice `cur[2n-1] + k * step` at or above lo
//     (0x3ef46-0x3ef8a), then moved by floatFloorToInt((mid - start) / step) steps toward the midpoint
//     mid = (prev.lo + prev.hi + 1 - winLen) / 2 (0x3ef95-0x3efe9), and the pair (start, start +
//     winLen - 1) is written (0x3efeb-0x3f018). One step is then subtracted when the mode is 2 or more
//     and the end is past prev.hi (0x3f01f-0x3f080), one step is added when the mode is 1 or 3 and the
//     start is before prev.lo (0x3f084-0x3f0f0), and two `while` loops slide the pair down until the
//     end is at or below hi (0x3f0f4-0x3f149) and up until the start is at or above lo
//     (0x3f14b-0x3f1a0). `done` is set to 1 (0x3f1a2).
//   * copy: the pair is copied from the previous record unchanged and `done` is 0
//     (0x3f1ab-0x3f1e3).
// Finally the second record gets a sub-window whose length is the largest multiple of the step that fits in
// the pair just written (0x3f1ea-0x3f217): the previous record's pair when that length is 0 or no flag
// of frame n is set (0x3f226-0x3f25e), the last `length` samples of the pair when the copy path ran
// and the mode is 2 (0x3f263-0x3f2af), and otherwise the first `length` samples of it
// (0x3f2b1-0x3f2ec). Returns 0 (0x3f2f0).
// A step of 0 faults on the `idiv` at 0x3ef66 and a step below 1 does not terminate the two `while`
// loops, so the fixtures only pass values of the table 0x64910.
int codecAlignAnalysisWindow_body(P<int32_t> step, P<int32_t> flags, P<int32_t> mode, P<int32_t> pn, P<int32_t> prev, P<int32_t> cur,
                                        P<int32_t> out, P<int32_t> frameLen, P<int32_t> winLen) {
    const CodecFrame fr(cx(), 0x80);                             // the original's frame, 0xcc-filled
    const R<float> q = *fr.loc<float>(4);                        // [ebp-4], handed to floatRoundHalfAway by address
    out -= 3;                                                    // sub 0xc at 0x3edde
    cur -= 3;                                                    // sub 0xc at 0x3ede7
    prev -= 3;                                                   // sub 0xc at 0x3edf0
    flags -= 1;                                                  // sub 4 at 0x3edf9

    const int n = *pn;                                           // re-read by the original at every use
    const int loLimit = (n - 2) * *frameLen + 1;                 // 0x3ee02-0x3ee10
    const int hiLimit = n * *frameLen;                           // 0x3ee13-0x3ee1e

    int allSet = (flags[2 * n - 2] == 1);                        // sete dl at 0x3ee34
    if (allSet) allSet = (flags[2 * n - 1] == 1);                // 0x3ee3a-0x3ee66
    if (allSet) allSet = (flags[2 * n] == 1);                    // 0x3ee69-0x3ee95
    if (allSet) allSet = (flags[2 * n + 1] == 1);                // 0x3ee98-0x3eec2
    if (allSet) allSet = (flags[2 * n + 2] == 1);                // 0x3eec5-0x3eeef
    const int anySet = (flags[2 * n + 1] == 1 || flags[2 * n + 2] == 1);   // 0x3eef2-0x3ef27

    int done;                                                    // [ebp-0x24]
    if (allSet || (anySet && *mode == 0)) {                      // 0x3ef2a, 0x3ef34, 0x3ef40
        const int s = *step;
        // the smallest point of the lattice `cur[2n-1] + k * step` that is at or above loLimit
        int start = div32(loLimit + s - 1 - cur[2 * n - 1], s);      // cdq + idiv at 0x3ef65, 0x3ef66
        start = start * s;                                       // imul at 0x3ef71
        start += cur[2 * n - 1];                                 // 0x3ef86
        const int len = *winLen;                                 // 0x3ef90
        // (prev.lo + prev.hi + 1 - len) / 2, divided toward zero by `cdq` + `sub` + `sar` at
        // 0x3efb8-0x3efbb
        const int mid = (prev[2 * n + 1] + prev[2 * n + 2] + 1 - len) / 2;
        // fild dword + fidiv dword + fstp dword at 0x3efc9-0x3efd1
        q = static_cast<float>(static_cast<double>(mid - start) / static_cast<double>(s));
        start += [](const auto &... a) { return ccall(0x0003E400, a...); }(&q) * s;   // 0x3efd8-0x3efe9
        cur[2 * n + 1] = start;                                  // 0x3eff5
        cur[2 * n + 2] = cur[2 * n + 1] + len - 1;               // 0x3f00a-0x3f018
        if (*mode >= 2 && cur[2 * n + 2] > prev[2 * n + 2]) {    // jl at 0x3f022, jle at 0x3f040
            cur[2 * n + 1] -= *step;                             // 0x3f053-0x3f05f
            cur[2 * n + 2] -= *step;                             // 0x3f074-0x3f080
        }
        if ((*mode == 1 || *mode == 3)                           // je at 0x3f08a, jne at 0x3f092
            && cur[2 * n + 1] < prev[2 * n + 1]) {               // jge at 0x3f0b0
            cur[2 * n + 1] += *step;                             // 0x3f0c3-0x3f0cf
            cur[2 * n + 2] += *step;                             // 0x3f0e4-0x3f0f0
        }
        while (cur[2 * n + 2] > hiLimit) {                       // jle at 0x3f105
            cur[2 * n + 1] -= *step;                             // 0x3f118-0x3f124
            cur[2 * n + 2] -= *step;                             // 0x3f139-0x3f145
        }
        while (cur[2 * n + 1] < loLimit) {                       // jge at 0x3f15c
            cur[2 * n + 1] += *step;                             // 0x3f16f-0x3f17b
            cur[2 * n + 2] += *step;                             // 0x3f190-0x3f19c
        }
        done = 1;                                                // 0x3f1a2
    } else {
        cur[2 * n + 1] = prev[2 * n + 1];                        // 0x3f1bf-0x3f1c3
        cur[2 * n + 2] = prev[2 * n + 2];                        // 0x3f1db-0x3f1df
        done = 0;                                                // 0x3f1e3
    }

    // the largest multiple of the step that fits in the pair just written
    const int sub = div32(cur[2 * n + 2] - cur[2 * n + 1] + 1, *step) * *step;   // 0x3f1fe-0x3f217
    if (sub == 0 || anySet == 0) {                               // je at 0x3f21e, jne at 0x3f224
        out[2 * n + 1] = prev[2 * n + 1];                        // 0x3f23a-0x3f23e
        out[2 * n + 2] = prev[2 * n + 2];                        // 0x3f256-0x3f25a
    } else if (done == 0 && *mode == 2) {                        // jne at 0x3f267, jne at 0x3f26f
        out[2 * n + 1] = cur[2 * n + 2] - sub + 1;               // 0x3f27f-0x3f28f
        out[2 * n + 2] = cur[2 * n + 2];                         // 0x3f2a7-0x3f2ab
    } else {
        out[2 * n + 1] = cur[2 * n + 1];                         // 0x3f2c5-0x3f2c9
        out[2 * n + 2] = cur[2 * n + 1] + sub - 1;               // 0x3f2de-0x3f2ec
    }
    return 0;                                                    // xor eax, eax at 0x3f2f0
}
void a_codecAlignAnalysisWindow(Ctx &ctx, const uint32_t *a, Ret &r) {
  const CxScope s(ctx);
  r.set_i32(static_cast<int32_t>(neg_u32(NEG(codecAlignAnalysisWindow), static_cast<u32>(codecAlignAnalysisWindow_body(P<int32_t>(a[0]), P<int32_t>(a[1]), P<int32_t>(a[2]), P<int32_t>(a[3]), P<int32_t>(a[4]), P<int32_t>(a[5]), P<int32_t>(a[6]), P<int32_t>(a[7]), P<int32_t>(a[8]))))));
}

// ---- codecPitchTrackDp (rva 0x3f310): from c3bi.cpp CodecGainSmoother_re
// 0x0003f310  codecGainSmoother (VA 0x3f310)
// with seven stack arguments ([ebp+8] the frame's gains, [ebp+0xc] the gain count, [ebp+0x10] the
// current index, [ebp+0x14] the mode, [ebp+0x18] and [ebp+0x1c] two index outputs, [ebp+0x20] the codec
// context; `ret` at 0x3f71b pops nothing).
// Four context slots are bound on entry (0x3f32b-0x3f353): the running cost array at +0x2268 (0x3c
// floats), the two-row back-pointer table at +0x2358 (row stride 0x3c dwords), the row number at +0x2538 and
// the smoothed level at +0x253c. The gains are decremented by one float when they are not null
// (`je 0x3f365` at 0x3f35a).
// Mode 1 pushes the level towards the current gain, `level * 0.75f + gains[index] / 2.0f` with the floats at
// 0x5ec04 and 0x5ec2c (0x3f36d-0x3f38e); any other mode decays it by the float 0.984375f at
// 0x5ec4c (0x3f395). The step penalty is `level / 16.0f` with the float 16.0f at 0x5ec48 (0x3f3a7),
// except that in mode 0 a level strictly below the float 128.0f at 0x5ec44 makes the penalty the dword
// 0x41000000 (`test ah, 1` / `je 0x3f3d1` at 0x3f3c5, store at 0x3f3ca).
// The current row is `*rowSlot + 1` (0x3f3d6) and its first back-pointer is set to 1 (0x3f3e5). The
// forward pass walks k = 1..*count (`jg 0x3f49c` at 0x3f426) carrying a running cost that starts as
// cost[0] (0x3f401): each step adds the penalty (0x3f42b) and, when the result is strictly below cost[k],
// overwrites cost[k] and points back at the best index so far (`test ah, 1` / `je 0x3f46c` at 0x3f440,
// stores at 0x3f44e and 0x3f463); otherwise the running cost is reset to cost[k], the back-pointer is k
// and k becomes the best index (0x3f476-0x3f494).
// The backward pass starts at `best - 1` with the running cost cost[best-1] (0x3f49f-0x3f4ae) and repeats
// the same test downwards while the index is at least 1 (`jl 0x3f531` at 0x3f4b5), either overwriting
// cost[k] or jumping to the back-pointer it finds (0x3f4fb-0x3f523).
// cost[0] then takes `gains[1] / 2.0f + cost[0]` (0x3f534-0x3f545) and k = 2..*count take
// `gains[k] / 2.0f + cost[k-1]` (0x3f586-0x3f59f), while the largest cost is tracked in a local
// (`test ah, 0x41` / `jne 0x3f5c4` at 0x3f5b2) and the smallest both in a local and in the second index
// output (`test ah, 1` / `je 0x3f5ed` at 0x3f5d3, store at 0x3f5de). Every cost is then shifted down by
// the smallest (0x3f617-0x3f624) and so is the largest (0x3f62a).
// A backoff then runs over m = 20, 30, 40 (0x3f63a, `add eax, 0xa` at 0x3f646, `jg 0x3f683` at
// 0x3f650): when the chosen index is strictly above m (`jle 0x3f681` at 0x3f65a) and a quarter of the
// largest cost, with the float 4.0f at 0x5ec20, is strictly above cost[index - m - 1]
// (`test ah, 0x41` / `jne 0x3f681` at 0x3f676), m is remembered (0x3f67e); the chosen index is reduced
// by it (0x3f688).
// The first index output takes the chosen index (0x3f6a0) and is then followed back twice through the
// back-pointer table, the row alternating as `(row % 2) + 1` (0x3f6ba-0x3f6cd, store at 0x3f6ea).
// Finally the row slot becomes `(row + 1) % 2` (0x3f6fd-0x3f711) and the function returns 0 (0x3f713).
int codecPitchTrackDp_body(P<float> gains, P<int32_t> count, P<int32_t> index, P<int32_t> mode,
                                 P<int32_t> chosenOut, P<int32_t> indexOut, P<char> context) {
    P<char> ctx = pc<char>(context);
    P<float> cost = pc<float>(ctx + 0x2268);        // 0x3f32b
    P<int32_t> back = pc<int32_t>(ctx + 0x2358);            // 0x3f336
    P<int32_t> rowSlot = pc<int32_t>(ctx + 0x2538);         // 0x3f342
    P<float> level = pc<float>(ctx + 0x253C);       // 0x3f34e

    if (gains != 0) gains -= 1;                                  // je at 0x3f35a

    const float k075 = rf(0x0005EC04);
    const float k2 = rf(0x0005EC2C);
    const float kDecay = rf(0x0005EC4C);
    const float k16 = rf(0x0005EC48);
    const float k128 = rf(0x0005EC44);
    const float k4 = rf(0x0005EC20);

    if (*mode == 1) {                                            // jne at 0x3f36b
        *level = static_cast<float>(
            static_cast<double>(*level) * static_cast<double>(k075)
            + static_cast<double>(gains[*index]) / static_cast<double>(k2));   // 0x3f370-0x3f38e
    } else {
        *level = static_cast<float>(
            static_cast<double>(*level) * static_cast<double>(kDecay));        // 0x3f395-0x3f3a0
    }
    float penalty = static_cast<float>(
        static_cast<double>(*level) / static_cast<double>(k16));               // 0x3f3a5-0x3f3ad
    if (*mode == 0 && static_cast<double>(*level) < static_cast<double>(k128))  // jne at 0x3f3b6, je at 0x3f3c8
        *pc<uint32_t>(&penalty) = 0x41000000u;                  // 0x3f3ca

    const int row = *rowSlot + 1;                                              // 0x3f3d6
    P<int32_t> bpRow = back + (row - 1) * 0x3C;                                      // 0x3f3df
    bpRow[0] = 1;                                                              // 0x3f3e5

    int best = 1;                                                              // 0x3f3f7
    float running = cost[0];                                                   // 0x3f401
    int n = *count;                                                            // 0x3f409
    for (int k = 1; k <= n; ++k) {                                             // jg at 0x3f426
        running = static_cast<float>(
            static_cast<double>(running) + static_cast<double>(penalty));      // 0x3f42b
        if (static_cast<double>(running) < static_cast<double>(cost[k - 1])) { // je at 0x3f443
            cost[k - 1] = running;                                             // 0x3f44e
            bpRow[k - 1] = best;                                               // 0x3f463
        } else {
            running = cost[k - 1];                                             // 0x3f476
            bpRow[k - 1] = k;                                                  // 0x3f48a
            best = k;                                                          // 0x3f494
        }
    }

    int k = best - 1;                                                          // 0x3f49f
    running = cost[k];                                                         // 0x3f4ab
    while (k >= 1) {                                                           // jl at 0x3f4b5
        running = static_cast<float>(
            static_cast<double>(running) + static_cast<double>(penalty));      // 0x3f4ba
        if (static_cast<double>(running) < static_cast<double>(cost[k - 1])) { // je at 0x3f4d2
            cost[k - 1] = running;                                             // 0x3f4dd
            bpRow[k - 1] = best;                                               // 0x3f4f2
        } else {
            best = bpRow[k - 1];                                               // 0x3f509
            k = best;                                                          // 0x3f516
            running = cost[k - 1];                                             // 0x3f51f
        }
        k -= 1;                                                                // 0x3f529
    }

    cost[0] = static_cast<float>(
        static_cast<double>(gains[1]) / static_cast<double>(k2) + static_cast<double>(cost[0]));  // 0x3f534-0x3f545
    float lowest = cost[0];                                                    // 0x3f54a
    float highest = lowest;                                                    // 0x3f552
    *indexOut = 1;                                                             // 0x3f558
    n = *count;                                                                // 0x3f561
    for (int j = 2; j <= n; ++j) {                                             // jg at 0x3f57e
        cost[j - 1] = static_cast<float>(
            static_cast<double>(gains[j]) / static_cast<double>(k2)
            + static_cast<double>(cost[j - 1]));                               // 0x3f586-0x3f59f
        if (static_cast<double>(cost[j - 1]) > static_cast<double>(highest))   // jne at 0x3f5b5
            highest = cost[j - 1];                                             // 0x3f5c1
        if (static_cast<double>(cost[j - 1]) < static_cast<double>(lowest)) {  // je at 0x3f5d6
            *indexOut = j;                                                     // 0x3f5de
            lowest = cost[j - 1];                                              // 0x3f5ea
        }
    }
    n = *count;                                                                // 0x3f5f2
    for (int j = 1; j <= n; ++j)                                               // jg at 0x3f60f
        cost[j - 1] = static_cast<float>(
            static_cast<double>(cost[j - 1]) - static_cast<double>(lowest));   // 0x3f617-0x3f624
    highest = static_cast<float>(
        static_cast<double>(highest) - static_cast<double>(lowest));           // 0x3f62a-0x3f630

    int backoff = 0;                                                           // 0x3f633
    for (int m = 0x14; m <= 0x28; m += 0xA) {                                  // jg at 0x3f650
        if (*indexOut > m) {                                                   // jle at 0x3f65a
            const int at = *indexOut - m;                                      // 0x3f661
            if ((static_cast<double>(highest) / static_cast<double>(k4))
                > static_cast<double>(cost[at - 1]))                           // jne at 0x3f679
                backoff = m;                                                   // 0x3f67e
        }
    }
    *indexOut -= backoff;                                                      // 0x3f688-0x3f68e

    int walkRow = *rowSlot;                                                    // 0x3f693
    *chosenOut = *indexOut;                                                    // 0x3f6a0
    for (int step = 1; step <= 2; ++step) {                                    // jg at 0x3f6b8
        walkRow = srem(walkRow, 0x80000001u, 0xFFFFFFFEu) + 1;            // 0x3f6ba-0x3f6cd
        *chosenOut = back[(walkRow - 1) * 0x3C + *chosenOut - 1];              // 0x3f6d0-0x3f6ea
    }
    *rowSlot = srem(*rowSlot + 1, 0x80000001u, 0xFFFFFFFEu);              // 0x3f6fd-0x3f711
    return 0;                                                                  // 0x3f713
}
void a_codecPitchTrackDp(Ctx &ctx, const uint32_t *a, Ret &r) {
  const CxScope s(ctx);
  r.set_i32(static_cast<int32_t>(neg_u32(NEG(codecPitchTrackDp), static_cast<u32>(codecPitchTrackDp_body(P<float>(a[0]), P<int32_t>(a[1]), P<int32_t>(a[2]), P<int32_t>(a[3]), P<int32_t>(a[4]), P<int32_t>(a[5]), P<char>(a[6]))))));
}

// ---- codecVoicingDecision (rva 0x3f720): from c3bv.cpp CodecFeatureDecision_re

int codecVoicingDecision_body(P<char> rangeIn, P<char> firstIn, P<char> secondIn, P<char> descIn, P<char> halfCell,
                                    P<char> divisorCell, P<char> numeratorCell, P<char> lagCell, P<char> blockIn,
                                    P<char> inFlagsIn, P<char> outFlagsIn, P<char> unusedArg, P<char> ctx) {
    const CodecFrame fr(cx(), 0x11C);  // the original's frame, 0xcc-filled
    const P<char> ebp = fr.ebp();
    (void)unusedArg;                                             // [ebp+0x34] is never read

    P<char> c = pc<char>(ctx);
    P<float> A = pc<float>(c + 0x2224);       // 0x3f741
    P<float> B = pc<float>(c + 0x2228);       // 0x3f74c
    P<float> C = pc<float>(c + 0x222C);       // 0x3f75b
    P<float> D = pc<float>(c + 0x2230);       // 0x3f76a
    P<int32_t> E = pc<int32_t>(c + 0x2248);           // 0x3f775
    P<int32_t> F = pc<int32_t>(c + 0x224C);           // 0x3f781
    P<int32_t> G = pc<int32_t>(c + 0x2250);           // 0x3f78d
    P<int32_t> H = pc<int32_t>(c + 0x2254);           // 0x3f798
    P<int32_t> I = pc<int32_t>(c + 0x2258);           // 0x3f7a4
    P<int32_t> J = pc<int32_t>(c + 0x2260);           // 0x3f7b0
    P<int32_t> K = pc<int32_t>(c + 0x225C);           // 0x3f7bb
    P<int32_t> L = pc<int32_t>(c + 0x2264);           // 0x3f7c7

    P<char> range = pc<char>(rangeIn);
    if (range != 0) range -= 4;                                  // je at 0x3f7d4, sub at 0x3f7d9
    P<int32_t> desc = pc<int32_t>(descIn);
    if (desc != 0) desc -= 1;                                    // je at 0x3f7e3, sub at 0x3f7e8
    // The two rebase counts live in [ebp-8] and [ebp-0xc] and are written only on the non-null path, so a null
    // array leaves the 0xcccccccc fill there and the pointer handed to codecWindowFeatures carries it
    // (0x3f900, 0x3f906).
    P<char> first = pc<char>(firstIn);
    if (first != 0) {                                            // je at 0x3f7f2
        *loc<int>(ebp, 0x08) = desc[1];                     // 0x3f7f7-0x3f7fa
        first -= *loc<int>(ebp, 0x08) * 4;                  // 0x3f7fd-0x3f808
    }
    P<char> second = pc<char>(secondIn);
    if (second != 0) {                                           // je at 0x3f80f
        *loc<int>(ebp, 0x0C) = desc[3];                     // 0x3f814-0x3f817
        second -= *loc<int>(ebp, 0x0C) * 4;                 // 0x3f81a-0x3f825
    }
    P<char> block = pc<char>(blockIn);
    if (block != 0) block -= 4;                                  // je at 0x3f82c, sub at 0x3f831
    P<char> inFlags = pc<char>(inFlagsIn);
    if (inFlags != 0) inFlags -= 4;                              // je at 0x3f83b, sub at 0x3f840
    P<char> outFlags = pc<char>(outFlagsIn);
    if (outFlags != 0) outFlags -= 4;                            // je at 0x3f84a, sub at 0x3f84f

    const int half = *pc<int32_t>(halfCell);

    if (half == 1) {                                             // jne 0x3f8c8 at 0x3f85b
        copy32(&D[0], &D[2]);                               // 0x3f85d-0x3f866
        copy32(&D[1], &D[3]);                               // 0x3f868-0x3f871
        copy32(&D[2], &D[4]);                               // 0x3f874-0x3f87d
        copy32(&D[3], &D[5]);                               // 0x3f880-0x3f889
        // fld [divisorCell]; fcomp 1.0f at 0x5b8a8; test ah,1 / jne 0x3f8ab
        float divisor;
        if (*pc<float>(divisorCell) < rf(0x0005B8A8))
            divisor = 1.0f;                                      // the immediate 0x3f800000 at 0x3f8ab
        else
            divisor = *pc<float>(divisorCell);   // 0x3f89e-0x3f8a3
        // fld [numeratorCell]; fdiv [ebp-0xa0]; fstp dword [*C]   (0x3f8b5-0x3f8c6)
        *C = static_cast<float>(static_cast<double>(*pc<float>(numeratorCell))
                                / static_cast<double>(divisor));
    }

    // codecWindowFeatures, fourteen arguments, the last seven of them out cells of this frame
    // (0x3f8c8-0x3f91b).
    [](const auto &... a) { return ccall(0x00041020, a...); }(
        range + 4, first + *loc<int>(ebp, 0x08) * 4, second + *loc<int>(ebp, 0x0C) * 4, desc + 1,
        halfCell, A, lagCell,
        loc<void>(ebp, 0x78), loc<void>(ebp, 0x94), loc<void>(ebp, 0x90),
        loc<void>(ebp, 0x80), loc<void>(ebp, 0x8C), loc<void>(ebp, 0x20),
        loc<void>(ebp, 0x24));

    const auto floorToInt = [](const auto &... a) { return ccall(0x0003E400, a...); };

    const int hDen = (*H >= 1) ? *H : 1;                         // jl 0x3f938 at 0x3f92a
    // fild [*G]; fidiv hDen; fadd [*B]; fmul 63.0f at 0x5ec68; fdiv 64.0f at 0x5ebfc; fstp [ebp-0x18]
    *loc<float>(ebp, 0x18) = static_cast<float>(
        ((static_cast<double>(*G) / static_cast<double>(hDen)) + static_cast<double>(*B))
        * static_cast<double>(rf(0x0005EC68)) / static_cast<double>(rf(0x0005EBFC)));
    const int smoothed = floorToInt(loc<float>(ebp, 0x18));     // call at 0x3f968
    *B = static_cast<float>(static_cast<double>(smoothed));      // fild [ebp-0xa8]; fstp [*B] at 0x3f976

    const int fDen = (*F >= 1) ? *F : 1;                         // jl 0x3f999 at 0x3f98a
    // fild [*H]; fmul [*B]; fidiv fDen; fstp dword [ebp-0x9c]   (0x3f9a3-0x3f9b6)
    const float selector = static_cast<float>(
        static_cast<double>(*H) * static_cast<double>(*B) / static_cast<double>(fDen));

    P<int32_t> thresholdTable = pc<int32_t>(sva(0x00064E10));
    P<float> thresholds = pc<float>(thresholdTable);
    int band = 1;                                                // 0x3f9bc, 0x3f9cf
    const int bandMax = thresholdTable[0] - 1;                   // 0x3f9c3-0x3f9cc
    // fld selector; fcomp thresholds[band]; test ah,0x41 / jne 0x3fa02 (keep looking while <=)
    while (band <= bandMax                                       // jg 0x3fa04 at 0x3f9e7
           && !(static_cast<double>(selector) > static_cast<double>(thresholds[band])))
        band += 1;                                               // 0x3f9d8-0x3f9de

    // The feature vector is one-based at [ebp-0x78]: feature j lives at [ebp-0x78+j*4] (0x3fab2).
    P<float> feature = loc<float>(ebp, 0x78);
    copy32(&feature[1], C);                                 // 0x3fa04-0x3fa0c
    const int eDen = (*E >= 1) ? *E : 1;                         // jl 0x3fa24 at 0x3fa15
    // fild [ebp-0x94]; fidiv eDen; fstp dword [ebp-0x70]       (0x3fa2e-0x3fa3a)
    feature[2] = static_cast<float>(static_cast<double>(*loc<int>(ebp, 0x94))
                                    / static_cast<double>(eDen));
    // fild [ebp-0x78]; fstp dword [ebp-0x6c]                   (0x3fa3d-0x3fa40)
    feature[3] = static_cast<float>(static_cast<double>(*loc<int>(ebp, 0x78)));
    copy32(&feature[4], loc<void>(ebp, 0x8C));         // 0x3fa43-0x3fa49
    copy32(&feature[5], loc<void>(ebp, 0x80));         // 0x3fa4c-0x3fa4f
    copy32(&feature[6], block + 8);                         // 0x3fa52-0x3fa58
    copy32(&feature[7], loc<void>(ebp, 0x20));         // 0x3fa5b-0x3fa5e
    copy32(&feature[8], loc<void>(ebp, 0x24));         // 0x3fa61-0x3fa64

    P<float> weights = pc<float>(sva(0x00064C54));
    P<float> biases = pc<float>(sva(0x00064C7C));
    P<float> score = pc<float>(pc<char>(D) + half * 4 + 0xC);
    copy32(score, &biases[band * 10]);                      // 0x3fa67-0x3fa7c
    for (int j = 1; j <= 8; ++j)                                 // jg 0x3facb at 0x3fa96
        // fld weights[band*10+j]; fmul feature[j]; fadd [score]; fstp dword [score]  (0x3faab-0x3fac5)
        *score = static_cast<float>(static_cast<double>(weights[band * 10 + j])
                                    * static_cast<double>(feature[j])
                                    + static_cast<double>(*score));

    P<int32_t> outFlag = pc<int32_t>(outFlags + half * 4 + 0x18);
    // fld [score]; fcomp 0.0f at 0x5ec00; test ah,0x41 / jne 0x3faf6
    if (static_cast<double>(*score) > static_cast<double>(rf(0x0005EC00)))
        *outFlag = 1;                                            // 0x3faec
    else
        *outFlag = 0;                                            // 0x3fafe

    P<int32_t> of = pc<int32_t>(outFlags);
    P<int32_t> inf = pc<int32_t>(inFlags);
    if (half != 1) {                                             // jne 0x3fb1d at 0x3fb16
        // (inFlags[1] & 2) != 0 or inFlags[2] == 1, and (inFlags[3] & 1) == 0   (0x3fb1d-0x3fb56)
        int gate;
        if (((inf[1] & 2) != 0 || inf[2] == 1) && (inf[3] & 1) == 0)
            gate = 1;                                            // 0x3fb40
        else
            gate = 0;                                            // 0x3fb4c
        // the four flags as a 4-bit code (0x3fb5f-0x3fb80)
        const int code = of[3] * 8 + of[4] * 4 + of[5] * 2 + of[6];
        // switch over the jump table at 0x3ffb7 (`ja 0x3fc04` at 0x3fbab)
        switch (code) {
        case 0: case 3: case 9: case 12: case 15:
            break;                                               // the five stubs that jump to 0x3fd8b
        case 2:                                                  // 0x3fc22
            // fld D[3]; fchs; fcomp D[2]; test ah,0x41 / jne 0x3fc4c
            if (of[7] != 0 && -static_cast<double>(D[3]) <= static_cast<double>(D[2]))
                of[6] = 1;                                       // 0x3fc4f
            else
                of[5] = 0;                                       // 0x3fc43
            break;
        case 4:                                                  // 0x3fc5b
            of[4] = 0;
            break;
        case 5:                                                  // 0x3fc6a
            // fld D[2]; fchs; fcomp D[1]; test ah,0x41 / jne 0x3fc8b
            if (-static_cast<double>(D[2]) <= static_cast<double>(D[1]))
                of[5] = 1;                                       // 0x3fc8e
            else
                of[4] = 0;                                       // 0x3fc82
            break;
        case 6:                                                  // 0x3fc9a
            // je 0x3fcbe at 0x3fca1 and at 0x3fcaa; fld D[3]; fcomp D[0]; jne 0x3fcca
            if (of[1] == 1 || of[7] == 1)
                of[6] = 1;                                       // 0x3fcc1
            else if (static_cast<double>(D[3]) <= static_cast<double>(D[0]))
                of[3] = 1;                                       // 0x3fccd
            else
                of[6] = 1;                                       // 0x3fcc1
            break;
        case 7:                                                  // 0x3fcd9
            if (gate != 0) of[4] = 0;                            // je 0x3fce9 at 0x3fcdd
            break;
        case 8:                                                  // 0x3fcee
            if (gate != 0) of[4] = 1;                            // je 0x3fcfe at 0x3fcf2
            break;
        case 10:                                                 // 0x3fd03
            // fld D[1]; fchs; fcomp D[2]; test ah,0x41 / jne 0x3fd24
            if (-static_cast<double>(D[1]) <= static_cast<double>(D[2]))
                of[4] = 1;                                       // 0x3fd27
            else
                of[5] = 0;                                       // 0x3fd1b
            break;
        case 11:                                                 // 0x3fd30
            of[4] = 1;
            break;
        case 13:                                                 // 0x3fd3c
            // jne 0x3fd66 at 0x3fd43; fld D[2]; fchs; fcomp D[3]; jne 0x3fd66
            if (of[7] != 0 || -static_cast<double>(D[2]) <= static_cast<double>(D[3]))
                of[5] = 1;                                       // 0x3fd69
            else
                of[6] = 0;                                       // 0x3fd5d
            break;
        case 14:                                                 // 0x3fd72
            // je 0x3fd8b at 0x3fd76, jne 0x3fd8b at 0x3fd7f
            if (gate != 0 && of[7] == 0) of[5] = 0;              // 0x3fd84
            break;
        default:                                                 // 0x3fc04 (code 1 and every code above 15)
            if (gate != 0 && of[7] == 1) of[5] = 1;              // je 0x3fc1d at 0x3fc08,
            break;                                               // jne 0x3fc1d at 0x3fc11, 0x3fc16
        }
    }

    if (*pc<int32_t>(outFlags + (*pc<int32_t>(halfCell)) * 4 + 0x18) == 0) {
        // jne 0x3feaa at 0x3fd98
        const int level2 = *loc<int>(ebp, 0x90);            // 0x3fd9e
        const int cap2 = *I * 3;                                 // 0x3fda7-0x3fdaf
        const int lo2 = (level2 > cap2) ? cap2 : level2;         // jg 0x3fdc5 at 0x3fdb8
        const int acc2 = *K * 0x3F + lo2 * 8;                    // 0x3fdce-0x3fddf
        // fild acc2; fdiv 64.0f at 0x5ebfc; fstp dword [ebp-0x18]   (0x3fde5-0x3fdf1)
        *loc<float>(ebp, 0x18) = static_cast<float>(static_cast<double>(acc2)
                                                         / static_cast<double>(rf(0x0005EBFC)));
        *K = floorToInt(loc<float>(ebp, 0x18));             // call at 0x3fdf8, store at 0x3fe03
        *H = *K / 8;                                             // cdq / and edx,7 / sar 3 at 0x3fe0a-0x3fe16
        *I = level2;                                             // 0x3fe18-0x3fe21

        const int level1 = *loc<int>(ebp, 0x94);            // 0x3fe23
        const int cap1 = *J * 3;                                 // 0x3fe2c-0x3fe34
        const int lo1 = (level1 > cap1) ? cap1 : level1;         // jg 0x3fe4a at 0x3fe3d
        const int acc1 = *L * 0x3F + lo1 * 8;                    // 0x3fe53-0x3fe64
        *loc<float>(ebp, 0x18) = static_cast<float>(static_cast<double>(acc1)
                                                         / static_cast<double>(rf(0x0005EBFC)));
        *L = floorToInt(loc<float>(ebp, 0x18));             // call at 0x3fe7d, store at 0x3fe88
        *F = *L / 8;                                             // 0x3fe8f-0x3fe9b
        *J = level1;                                             // 0x3fe9d-0x3fea6
    } else {
        const int acc3 = *E * 0x3F + *loc<int>(ebp, 0x94);  // 0x3feaa-0x3feb8
        *loc<float>(ebp, 0x18) = static_cast<float>(static_cast<double>(acc3)
                                                         / static_cast<double>(rf(0x0005EBFC)));
        *E = floorToInt(loc<float>(ebp, 0x18));             // call at 0x3fed1, store at 0x3fedc
        const int acc4 = *G * 0x3F + *loc<int>(ebp, 0x90);  // 0x3fede-0x3feec
        *loc<float>(ebp, 0x18) = static_cast<float>(static_cast<double>(acc4)
                                                         / static_cast<double>(rf(0x0005EBFC)));
        *G = floorToInt(loc<float>(ebp, 0x18));             // call at 0x3ff05, store at 0x3ff10
    }

    const int product = *F * *E;                                 // imul at 0x3ff1a
    // fild product; call sqrt; fmul qword 64.0 at 0x5ec60; fdiv qword 3000.0 at 0x5ec58
    const double level = crt_sqrt(static_cast<double>(product)) * rd(0x0005EC60) / rd(0x0005EC58);
    *loc<float>(ebp, 0x1C) = static_cast<float>(level);     // fst dword (no pop) at 0x3ff43
    unsigned chosen;
    // fcomp 1.0f at 0x5b8a8 on the UNROUNDED value; test ah,1 / jne 0x3ff5e
    if (level < static_cast<double>(rf(0x0005B8A8)))
        chosen = 0x3F800000u;                                    // the immediate at 0x3ff5e
    else
        chosen = *loc<unsigned>(ebp, 0x1C);                 // 0x3ff53
    *loc<unsigned>(ebp, 0x18) = chosen;                     // 0x3ff6e
    unsigned clamped;
    // fld [ebp-0x18]; fcomp 20.0f at 0x5ec50; test ah,0x41 / je 0x3ff8c
    if (static_cast<double>(*loc<float>(ebp, 0x18)) > static_cast<double>(rf(0x0005EC50)))
        clamped = 0x41A00000u;                                   // the immediate at 0x3ff8c
    else
        clamped = *loc<unsigned>(ebp, 0x18);                // 0x3ff81
    *pc<uint32_t>(A) = clamped;                   // 0x3ff96-0x3ff9f
    return 0;                                                    // xor eax, eax at 0x3ffa1
}
void a_codecVoicingDecision(Ctx &ctx, const uint32_t *a, Ret &r) {
  const CxScope s(ctx);
  r.set_i32(static_cast<int32_t>(neg_u32(NEG(codecVoicingDecision), static_cast<u32>(codecVoicingDecision_body(P<char>(a[0]), P<char>(a[1]), P<char>(a[2]), P<char>(a[3]), P<char>(a[4]), P<char>(a[5]), P<char>(a[6]), P<char>(a[7]), P<char>(a[8]), P<char>(a[9]), P<char>(a[10]), P<char>(a[11]), P<char>(a[12]))))));
}

// ---- codecRefinePitchLag (rva 0x40000): from c3bn.cpp CodecPitchAnalysis_re

int codecRefinePitchLag_body(P<float> samples, P<int32_t> span, P<int32_t> lags,
                                  P<int32_t> lagCount, P<float> scores, P<int32_t> bestIdx, P<int32_t> secondIdx,
                                  P<int32_t> lagOut) {
    const CodecFrame fr(cx(), 0xA8);                         // the original's frame, 0xcc-filled (rep stosd at 0x4001c)
    const P<int32_t> cand = fr.loc<int32_t>(0x5C);           // the frame array at [ebp + n*4 - 0x5c]
    const P<float> subScores = fr.loc<float>(0x2C);          // the frame array at [ebp + n*4 - 0x2c]
    const R<int32_t> subBest = *fr.loc<int32_t>(0x2C);       // [ebp-0x2c] (= subScores slot 0) and [ebp-0x34]: the
    const R<int32_t> subLargest = *fr.loc<int32_t>(0x34);    // out cells of the inner codecAmdfPitch calls
    const R<int32_t> count = *fr.loc<int32_t>(0x30);         // [ebp-0x30], handed to it by address

    samples -= 1;                                            // 0x4001e
    scores -= 1;                                             // 0x40027
    lags -= 1;                                               // 0x40030

    const auto amdf = [](const auto &... a) { return ccall(0x000414F0, a...); };
    amdf(samples + 1, span, lags + 1, lagCount, &lags[*lagCount], scores + 1, bestIdx, secondIdx);
                                                             // 0x4003c-0x4006a
    *lagOut = lags[*bestIdx];                                // 0x40077-0x40080
    int best = ftol(static_cast<double>(scores[*bestIdx]));                       // 0x4008a-0x40092

    count = 0;                                               // 0x40095
    int cursor = *bestIdx - 2;                               // 0x400a1-0x400a4
    const int lo = *lagOut - 3;                              // 0x400ac
    const int hi = *lagOut + 3;                              // 0x400b7
    const int limit = lags[*lagCount] - 1;                   // 0x400c5-0x400c8
    const int end = (hi > limit) ? limit : hi;               // jg at 0x400d4
    const int start = (lo < 0x29) ? 0x29 : lo;               // jl at 0x400ee

    for (int i = start; i <= end; ++i) {                     // jg at 0x40116
        while (lags[cursor] < i) ++cursor;                   // jge at 0x40124
        if (lags[cursor] != i) {                             // je at 0x4013d
            ++count;                                         // 0x40142
            cand[count] = i;                                 // 0x4014e
        }
    }

    if (count > 0) {                                         // jle at 0x40158
        amdf(samples + 1, span, &cand[1], &count, &lags[*lagCount], &subScores[1],
             &subBest, &subLargest);                         // 0x4015a-0x40185
        if (static_cast<double>(best) > static_cast<double>(subScores[subBest])) {     // jne at 0x4019c
            *lagOut = cand[subBest];                         // 0x401a4-0x401a8
            best = ftol(static_cast<double>(subScores[subBest]));                 // 0x401ad-0x401b6
        }
    }

    if (*lagOut >= 0x50) {                                   // jl at 0x401bf
        const int half = *lagOut / 2;                        // 0x401ca-0x401cf
        if ((half & 1) == 0) {                               // jne at 0x401da
            count = 2;                                       // 0x401dc
            cand[1] = half - 1;                              // 0x401e9
            cand[2] = half + 1;                              // 0x401f2
        } else {
            count = 1;                                       // 0x401f7
            cand[1] = half;                                  // 0x40201
        }
        amdf(samples + 1, span, &cand[1], &count, &lags[*lagCount], &subScores[1],
             &subBest, &subLargest);                         // 0x40204-0x4022f
        if (static_cast<double>(best) > static_cast<double>(subScores[subBest])) {     // jne at 0x40246
            *lagOut = cand[subBest];                         // 0x4024e-0x40252
            best = ftol(static_cast<double>(subScores[subBest]));                 // 0x40257-0x40260
            *bestIdx -= 0x14;                                // 0x40268-0x4026e
        }
    }

    scores[*bestIdx] = static_cast<float>(best);             // 0x40270-0x4027b
    const int low = *bestIdx - 5;                            // 0x40283
    *secondIdx = (low < 1) ? 1 : low;                        // jl at 0x4028d
    const int high = *bestIdx + 5;                           // 0x402ab
    const int stop = (high > *lagCount) ? *lagCount : high;  // jg at 0x402b9
    for (int i = *secondIdx + 1; i <= stop; ++i)             // jg at 0x402ed
        if (static_cast<double>(scores[i]) > static_cast<double>(scores[*secondIdx]))  // jne at 0x40308
            *secondIdx = i;                                  // 0x4030d-0x40310
    return 0;                                                // 0x40314
}
void a_codecRefinePitchLag(Ctx &ctx, const uint32_t *a, Ret &r) {
  const CxScope s(ctx);
  r.set_i32(static_cast<int32_t>(neg_u32(NEG(codecRefinePitchLag), static_cast<u32>(codecRefinePitchLag_body(P<float>(a[0]), P<int32_t>(a[1]), P<int32_t>(a[2]), P<int32_t>(a[3]), P<float>(a[4]), P<int32_t>(a[5]), P<int32_t>(a[6]), P<int32_t>(a[7]))))));
}

// ---- codecInverseFilter2 (rva 0x40330): from c3bd.cpp CodecCorrelationLags_re
// 0x00040330  codecCorrelationLags (VA 0x40330)
// with five stack arguments ([ebp+8] the input samples, [ebp+0xc] the output samples, [ebp+0x10] the
// last index, [ebp+0x14] the window length, [ebp+0x18] two coefficient slots; `ret` at 0x404c7 pops
// nothing). All three array pointers are decremented by 4 bytes (0x4034b, 0x40354, 0x40360).
// Three accumulators live at [ebp+k*4-0x20] for k = 1, 2, 3; the slot for k = 0 is a fourth local used later.
// The outer loop runs k = 1..3 (`jg 0x403e8` at 0x40379) and zeroes its accumulator (0x4037e), takes
// the lag `4k - 4` (`lea eax, [edx*4-4]` at 0x40389) and runs an inner loop from `*last + 4k - *window`
// (0x4039b-0x403ab) to *last inclusive (`jg 0x403e6` at 0x403bf, SIGNED) in steps of TWO
// (`add edx, 2` at 0x403b3), accumulating `in[j] * in[j - lag]` as a float
// (0x403d0-0x403e0).
// The two coefficient slots are then zeroed (0x403f9, 0x40403) along with the locals at [ebp-0x20] and
// [ebp-0x24] (0x403e8, 0x403ef), and the first accumulator is compared with the float 1e-10 at
// 0x5ec40 (`fcomp` at 0x4040d, `test ah, 0x41` at 0x40415), so the solve runs only when it is
// strictly above that floor.
// The solve writes coefficient 0 = acc2 / acc1 (0x4041a-0x40423) and coefficient 1 =
// (acc3 - acc2 * coefficient0) / (acc1 - acc2 * coefficient0) (0x40426-0x40443), then leaves
// `coefficient0 - coefficient0 * coefficient1` in [ebp-0x20] (0x40446-0x40458) and a raw copy of
// coefficient 1 in [ebp-0x24] (0x4045b-0x40461).
// The output loop re-reads *last (0x40467), runs from `*last + 1 - *window` (0x4046c-0x40479) to
// *last inclusive (`jg 0x404bf` at 0x4048d) and writes
// `(in[j] - A * in[j-4]) - B * in[j-8]` (0x4048f-0x404ba). Returns 0 (0x404bf).
int codecInverseFilter2_body(P<float> in, P<float> out, P<int32_t> last, P<int32_t> window,
                                    P<float> coefficients) {
    out -= 1;                                                // sub at 0x4034b
    in -= 1;                                                 // sub at 0x40354
    coefficients -= 1;                                       // sub at 0x40360

    float acc[4];
    for (int k = 1; k <= 3; ++k) {                           // jg at 0x40379
        acc[k] = 0.0f;                                       // 0x4037e
        const int lag = k * 4 - 4;                           // lea at 0x40389
        const int bound = *last;                             // 0x40396
        for (int j = *last + k * 4 - *window; j <= bound; j += 2)      // 0x4039b, jg at 0x403bf
            acc[k] = static_cast<float>(static_cast<double>(in[j]) * static_cast<double>(in[j - lag])
                                        + static_cast<double>(acc[k]));   // fstp at 0x403e0
    }

    acc[0] = 0.0f;                                           // 0x403e8
    float b = 0.0f;                                          // 0x403ef
    coefficients[1] = 0.0f;                                  // 0x403f9
    coefficients[2] = 0.0f;                                  // 0x40403
    if (static_cast<double>(acc[1]) > static_cast<double>(rf(0x0005EC40))) {   // test ah, 0x41 at 0x40415
        coefficients[1] = static_cast<float>(static_cast<double>(acc[2])
                                             / static_cast<double>(acc[1]));       // fstp at 0x40423
        coefficients[2] = static_cast<float>(
            (static_cast<double>(acc[3]) - static_cast<double>(acc[2]) * static_cast<double>(coefficients[1]))
            / (static_cast<double>(acc[1]) - static_cast<double>(acc[2]) * static_cast<double>(coefficients[1])));
        acc[0] = static_cast<float>(static_cast<double>(coefficients[1])
                                    - static_cast<double>(coefficients[1]) * static_cast<double>(coefficients[2]));
        b = coefficients[2];                                 // raw dword at 0x40461
    }

    const int bound = *last;                                 // 0x40467
    for (int j = *last + 1 - *window; j <= bound; ++j)       // 0x4046c, jg at 0x4048d
        out[j] = static_cast<float>(
            (static_cast<double>(in[j]) - static_cast<double>(acc[0]) * static_cast<double>(in[j - 4]))
            - static_cast<double>(b) * static_cast<double>(in[j - 8]));            // fstp at 0x404ba
    return 0;
}
void a_codecInverseFilter2(Ctx &ctx, const uint32_t *a, Ret &r) {
  const CxScope s(ctx);
  r.set_i32(static_cast<int32_t>(neg_u32(NEG(codecInverseFilter2), static_cast<u32>(codecInverseFilter2_body(P<float>(a[0]), P<float>(a[1]), P<int32_t>(a[2]), P<int32_t>(a[3]), P<float>(a[4]))))));
}

// ---- codecLowPassFir31 (rva 0x404d0): from c3bd.cpp CodecSymmetricFir_re
// 0x000404d0  codecSymmetricFir (VA 0x404d0)
// with four stack arguments ([ebp+8] the input samples, [ebp+0xc] the output samples, [ebp+0x10] the
// last index, [ebp+0x14] the window length; `ret` at 0x40734 pops nothing). Both array pointers are
// decremented by 4 bytes (0x404eb, 0x404f7) and the single loop runs from `*last + 1 - *window`
// (0x40502-0x4050f) to *last inclusive (`jg 0x4072c` at 0x40523, SIGNED).
// The body is a fully unrolled 31-tap symmetric FIR. The first tap is `(in[j] + in[j-30]) * K15` with the
// constant at 0x5eca8 (0x40535-0x4053c), spilled to a float at 0x40542. Fourteen further taps add
// `(in[j-t] + in[j-(30-t)]) * K(15-t)` for t = 1..14, each with its own constant four bytes lower in .rdata
// (0x5eca4 down to 0x5ec70) and each spilled to the same float slot (0x40551-0x40702). The centre
// tap adds `in[j-15] * K0` with the constant at 0x5ec6c (0x4070b-0x40718). The accumulator is then
// copied to the output as a raw dword (`mov` at 0x40724). Returns 0 (0x4072c).
int codecLowPassFir31_body(P<float> in, P<float> out, P<int32_t> last, P<int32_t> window) {
    out -= 1;                                                // sub at 0x404eb
    in -= 1;                                                 // sub at 0x404f7
    // K[0] is the centre tap at 0x5ec6c and K[15] the outermost at 0x5eca8.
    P<float> k = pc<float>(sva(0x0005EC6C));

    const int bound = *last;                                 // 0x404fd
    for (int j = *last + 1 - *window; j <= bound; ++j) {     // 0x40505, jg at 0x40523
        float acc = static_cast<float>(
            (static_cast<double>(in[j]) + static_cast<double>(in[j - 30])) * static_cast<double>(k[15]));
        for (int t = 1; t <= 14; ++t)                        // the unrolled taps at 0x40551-0x40702
            acc = static_cast<float>(
                (static_cast<double>(in[j - t]) + static_cast<double>(in[j - (30 - t)]))
                * static_cast<double>(k[15 - t]) + static_cast<double>(acc));
        acc = static_cast<float>(static_cast<double>(in[j - 15]) * static_cast<double>(k[0])
                                 + static_cast<double>(acc));          // fstp at 0x40718
        *pc<uint32_t>(&out[j]) = fbits(acc);        // mov at 0x40724
    }
    return 0;
}
void a_codecLowPassFir31(Ctx &ctx, const uint32_t *a, Ret &r) {
  const CxScope s(ctx);
  r.set_i32(static_cast<int32_t>(neg_u32(NEG(codecLowPassFir31), static_cast<u32>(codecLowPassFir31_body(P<float>(a[0]), P<float>(a[1]), P<int32_t>(a[2]), P<int32_t>(a[3]))))));
}

// ---- codecSelectWindowFromJumps (rva 0x40740): from c3ca.cpp CodecSelectWindowFromJumps_re

int codecSelectWindowFromJumps_body(P<int32_t> jumps, P<int32_t> count, P<int32_t> /*unused3*/, P<int32_t> outMode,
                                          P<int32_t> bounds, P<int32_t> pn, P<int32_t> frameLen, P<int32_t> minGap, P<int32_t> winLen,
                                          P<int32_t> minStart, P<int32_t> /*unused11*/) {
    jumps -= 1;                                                  // sub 4 at 0x4075b
    bounds -= 3;                                                 // sub 0xc at 0x40764

    const int n = *pn;                                           // re-read by the original at every use
    int start = bounds[2 * n] + 1;                               // 0x40776-0x4077d
    const int floorLimit = (n - 2) * *frameLen + 1;              // 0x40785-0x40791
    const int lo = (start >= floorLimit) ? start : floorLimit;   // jl at 0x4079a
    const int hi = n * *frameLen;                                // 0x407b6-0x407bb

    int i = *count - 1;                                          // 0x407c1-0x407c6
    while (i >= 1 && jumps[i] > hi) --i;                         // jl at 0x407d8, jg at 0x407e6
    ++i;                                                         // 0x407ec-0x407f2

    if (i <= 1 || jumps[i - 1] < lo) {                           // jle at 0x407f9, jge at 0x40808
        start = bounds[2 * n] + 1;                               // 0x40816-0x4081d
        bounds[2 * n + 1] = (start >= *minStart) ? start : *minStart;   // jl at 0x40828
        bounds[2 * n + 2] = bounds[2 * n + 1] + *winLen - 1;     // 0x40855-0x4086c
        *outMode = 0;                                            // 0x40873
        return 0;                                                // xor eax, eax at 0x40a9a
    }

    int j = i - 1;                                               // 0x4087e-0x40884
    while (j >= 1 && jumps[j] >= lo) --j;                        // jl at 0x40896, jge at 0x408a4
    ++j;                                                         // 0x408aa-0x408b0

    const int runEnd = i - 1;                                    // [ebp-4] at 0x408b3
    int wide = 0;                                                // [ebp-0xc]
    for (int k = j + 1; k <= runEnd; ++k) {                      // jg at 0x408d6
        if (jumps[k] - jumps[j] >= *minGap) { wide = 1; break; } // jl at 0x408ef, 0x408f1
    }

    // [ebp-4] is reused from here on; the loop below is bounded by i ([ebp-0x18]), not by this value.
    const int frameBar = (n - 1) * *frameLen;                    // 0x40908-0x40911
    const int gapBar = lo + *minGap - 1;                         // 0x40917-0x40920
    if (!wide) {                                                 // jne at 0x40927
        const int bar = (frameBar >= gapBar) ? frameBar : gapBar;   // jl at 0x40933
        if (jumps[j] > bar) {                                    // jle at 0x4094f
            bounds[2 * n + 2] = jumps[j] - 1;                    // 0x4095a-0x40967
            const int back = bounds[2 * n + 2] - *winLen + 1;    // 0x4097e-0x40987
            bounds[2 * n + 1] = (lo >= back) ? lo : back;        // jl at 0x40990
            *outMode = 2;                                        // 0x409b4
            return 0;                                            // xor eax, eax at 0x40a9a
        }
    }

    bounds[2 * n + 1] = jumps[j];                                // 0x409cf-0x409d2
    for (;;) {
        ++j;                                                     // 0x409d6-0x409dc
        if (j >= i) break;                                       // cmp [ebp-0x18], jl at 0x409e5
        if (jumps[j] > bounds[2 * n + 1] + *winLen) break;       // jle at 0x40a05
        if (jumps[j] >= bounds[2 * n + 1] + *minGap) {           // jge at 0x40a25
            bounds[2 * n + 2] = jumps[j] - 1;                    // 0x40a32-0x40a3f
            *outMode = 3;                                        // 0x40a46
            return 0;                                            // xor eax, eax at 0x40a4c
        }
    }
    const int end = bounds[2 * n + 1] + *winLen - 1;             // 0x40a5e-0x40a67
    bounds[2 * n + 2] = (end <= hi) ? end : hi;                  // jg at 0x40a70
    *outMode = 1;                                                // 0x40a94
    return 0;                                                    // xor eax, eax at 0x40a9a
}
void a_codecSelectWindowFromJumps(Ctx &ctx, const uint32_t *a, Ret &r) {
  const CxScope s(ctx);
  r.set_i32(static_cast<int32_t>(neg_u32(NEG(codecSelectWindowFromJumps), static_cast<u32>(codecSelectWindowFromJumps_body(P<int32_t>(a[0]), P<int32_t>(a[1]), P<int32_t>(a[2]), P<int32_t>(a[3]), P<int32_t>(a[4]), P<int32_t>(a[5]), P<int32_t>(a[6]), P<int32_t>(a[7]), P<int32_t>(a[8]), P<int32_t>(a[9]), P<int32_t>(a[10]))))));
}

// ---- codecMarkCorrelationJumps (rva 0x40ab0): from c3bi.cpp CodecMarkCorrelationJumps_re

int codecMarkCorrelationJumps_body(P<float> samples, P<int32_t> jumps, P<int32_t> jumpCount,
                                         P<int32_t> jumpCapacity, P<int32_t> offset, P<int32_t> last,
                                         P<int32_t> window, P<char> context) {
    P<char> ctx = pc<char>(context);
    P<float> product = pc<float>(ctx + 0x21C4);     // 0x40ace
    P<float> energy = pc<float>(ctx + 0x21C8);      // 0x40ad9
    P<float> ratio = pc<float>(ctx + 0x21CC);       // 0x40ae5
    P<float> ring = pc<float>(ctx + 0x21D0);        // 0x40af1
    P<float> sum = pc<float>(ctx + 0x2210);         // 0x40afc
    P<int32_t> indexA = pc<int32_t>(ctx + 0x2214);          // 0x40b08
    P<int32_t> indexB = pc<int32_t>(ctx + 0x2218);          // 0x40b14
    P<int32_t> lastJump = pc<int32_t>(ctx + 0x221C);        // 0x40b1f
    P<int32_t> openFlag = pc<int32_t>(ctx + 0x2220);        // 0x40b2b

    if (jumps != 0) jumps -= 1;                                  // je at 0x40b38
    if (samples != 0) samples -= *offset;                        // je at 0x40b47, 0x40b54
    if (*openFlag != 0) *lastJump -= *window;                    // je at 0x40b65, 0x40b6f

    const float k63 = rf(0x0005EC68);
    const float k64 = rf(0x0005EBFC);
    const float zero = rf(0x0005EC00);
    const float moveLimit = rf(0x0005ECAC);
    P<float> one = pc<float>(sva(0x00064E3C));
    const auto copySign = [](const auto &... a) { return ccall_f(0x0003E380, a...); };

    const int end = *last;                                       // 0x40b79
    for (int j = *last - *window + 1; j <= end; ++j) {           // 0x40b88, jg at 0x40b9f
        *product = static_cast<float>(
            (static_cast<double>(samples[j]) * static_cast<double>(samples[j - 1])
             + static_cast<double>(*product) * static_cast<double>(k63))
            / static_cast<double>(k64));                         // 0x40bb1-0x40bce
        const float prev = samples[j - 1];                       // 0x40bd6
        *energy = static_cast<float>(
            (static_cast<double>(prev) * static_cast<double>(prev)
             + static_cast<double>(*energy) * static_cast<double>(k63))
            / static_cast<double>(k64));                         // 0x40bdd-0x40bf9

        if (static_cast<double>(*energy) != static_cast<double>(zero)) {          // jne at 0x40c0b
            const float magnitude = (static_cast<double>(*product) < static_cast<double>(zero))
                                        ? -*product : *product;                   // fchs at 0x40c2e
            if (static_cast<double>(magnitude) > static_cast<double>(*energy))    // jne at 0x40c40
                *ratio = static_cast<float>(copySign(one, product));             // 0x40c4b
            else
                *ratio = static_cast<float>(
                    static_cast<double>(*product) / static_cast<double>(*energy)); // 0x40c62
        }

        const float before = ring[*indexA - 1];                                    // 0x40c6c-0x40c75
        *sum = static_cast<float>(
            (static_cast<double>(*sum) - static_cast<double>(ring[*indexB - 1]))
            + static_cast<double>(*ratio));                                        // 0x40c7b-0x40c91
        ring[*indexB - 1] = *sum;                                                  // 0x40c96-0x40ca0
        ring[*indexA - 1] = *ratio;                                                // 0x40ca7-0x40cb1
        *indexA = srem(*indexA, 0x8000000Fu, 0xFFFFFFF0u) + 1;                // 0x40cb8-0x40ccc
        *indexB = srem(*indexB, 0x8000000Fu, 0xFFFFFFF0u) + 1;                // 0x40cd1-0x40ce5

        const double move = static_cast<double>(*sum) - static_cast<double>(before);  // 0x40cea
        const float moveStored = static_cast<float>(move);                            // fst dword at 0x40cef
        const float moveMagnitude = (move < static_cast<double>(zero))
                                        ? -moveStored : moveStored;                   // fchs at 0x40d0a
        if (static_cast<double>(moveMagnitude) > static_cast<double>(moveLimit)) {    // jne at 0x40d1d
            if (*openFlag == 0) {                                                     // jne at 0x40d25
                if (*jumpCount <= *jumpCapacity) {                                    // jg at 0x40d31
                    jumps[*jumpCount] = j - 9;                                        // 0x40d36, 0x40d41
                    *jumpCount += 1;                                                  // 0x40d49, 0x40d4f
                }
                *openFlag = 1;                                                        // 0x40d54
            }
            *lastJump = j;                                                            // 0x40d60
        } else if (*openFlag != 0 && (j - *lastJump) >= 0xA) {                        // je at 0x40d6a, jl at 0x40d77
            *openFlag = 0;                                                            // 0x40d7c
        }
    }
    return 0;                                                                         // 0x40d87
}
void a_codecMarkCorrelationJumps(Ctx &ctx, const uint32_t *a, Ret &r) {
  const CxScope s(ctx);
  r.set_i32(static_cast<int32_t>(neg_u32(NEG(codecMarkCorrelationJumps), static_cast<u32>(codecMarkCorrelationJumps_body(P<float>(a[0]), P<int32_t>(a[1]), P<int32_t>(a[2]), P<int32_t>(a[3]), P<int32_t>(a[4]), P<int32_t>(a[5]), P<int32_t>(a[6]), P<char>(a[7]))))));
}

// ---- codecPreEmphasis (rva 0x40da0): from c3az.cpp CodecFirstOrder_re
// 0x00040da0  codecFirstOrderFilter (VA 0x40da0)
// with five stack arguments ([ebp+8] the input samples, [ebp+0xc] the output samples, [ebp+0x10] the
// sample count, [ebp+0x14] a coefficient, [ebp+0x18] the one-sample filter state; `ret` at 0x40e26 pops
// nothing). Both sample pointers are decremented by 4 bytes on entry (0x40dbb, 0x40dc4) and the loop
// counter runs from 1 to *count inclusive (0x40dd2, `jg 0x40e1e` at 0x40dea, SIGNED), so element i of
// the loop is element i-1 of the caller's arrays.
// Each round computes `in[i] - coeff * state` with `fld dword [coeff]` / `fmul dword [state]` /
// `fsubr dword [in+i*4]` (0x40df2-0x40dfc) and spills it to a float slot (`fstp dword [ebp-8]` at
// 0x40dff). The state is then replaced by the raw dword of in[i] (0x40e0b, 0x40e0e) and the spilled
// value written to out[i] (0x40e19). The product and the subtraction both round at the thread's 53-bit
// precision control, the spill rounds to float; the accumulator is read before it is overwritten.
// Returns 0 (`xor eax, eax` at 0x40e1e).
int codecPreEmphasis_body(P<float> in, P<float> out, P<int32_t> count, P<float> coeff, P<float> state) {
    in -= 1;                                        // sub at 0x40dc4
    out -= 1;                                       // sub at 0x40dbb
    const int n = *count;
    for (int i = 1; i <= n; ++i) {
        float spilled = static_cast<float>(static_cast<double>(in[i])
                                           - static_cast<double>(*coeff) * static_cast<double>(*state));
        *pc<uint32_t>(state) = *pc<uint32_t>(&in[i]);   // dword copy
        out[i] = spilled;
    }
    return 0;
}
void a_codecPreEmphasis(Ctx &ctx, const uint32_t *a, Ret &r) {
  const CxScope s(ctx);
  r.set_i32(static_cast<int32_t>(neg_u32(NEG(codecPreEmphasis), static_cast<u32>(codecPreEmphasis_body(P<float>(a[0]), P<float>(a[1]), P<int32_t>(a[2]), P<float>(a[3]), P<float>(a[4]))))));
}

// ---- codecHighPassFilter (rva 0x40e30): from c3bd.cpp CodecBiquadFilter_re
// 0x00040e30  codecBiquadFilter (VA 0x40e30)
// with four stack arguments ([ebp+8] the samples, in place, [ebp+0xc] the first index, [ebp+0x10] the
// last index, [ebp+0x14] four float state slots; `ret` at 0x40f57 pops nothing).
// The sample pointer is decremented by 4 bytes only when it is non-null (`je 0x40e57` at 0x40e4c).
// The four state slots are copied into locals as raw dwords (0x40e5a-0x40e77) and the two loop bounds
// are read once each through their pointers (0x40e7d the last index, 0x40e85 the first); the loop is
// `jg 0x40f2c` at 0x40e9b, SIGNED.
// Each round runs two identical biquad sections. The first uses the state pair s0, s1 and the constants at
// 0x5ecc0 and 0x5ecbc: `t = (s0 * KA + sample) - s1 * KB` (0x40ead-0x40ec2, spilled to a float at
// 0x40ec4), then `sample = (t - (s0 + s0)) + s1` (`fadd st(0), st(0)` at 0x40eca, `fsubr` at 0x40ecc,
// `fadd` at 0x40ecf, spilled at 0x40ed2), then s1 = s0 and s0 = t (0x40ed5-0x40ede).
// The second uses s2, s3 and the constants at 0x5ecb8 and 0x5ecb4 in exactly the same shape
// (0x40ee1-0x40f12). The sample is finally scaled by the constant at 0x5ecb0 and written back as a
// float (0x40f15-0x40f24).
// After the loop the four locals are written back to the state slots (0x40f2c-0x40f4c) and the function
// returns 0 (`xor eax, eax` at 0x40f4f).
int codecHighPassFilter_body(P<float> samples, P<int32_t> first, P<int32_t> last, P<float> state) {
    if (samples != 0) samples -= 1;                          // je at 0x40e4c
    float s0 = state[0], s1 = state[1], s2 = state[2], s3 = state[3];   // 0x40e5a-0x40e77
    const float ka = rf(0x0005ECC0);
    const float kb = rf(0x0005ECBC);
    const float kc = rf(0x0005ECB8);
    const float kd = rf(0x0005ECB4);
    const float ke = rf(0x0005ECB0);

    const int n = *last;                                     // 0x40e7d
    for (int i = *first; i <= n; ++i) {                      // 0x40e85, jg at 0x40e9b
        float sample = samples[i];                           // raw dword at 0x40ea7
        const double tw = (static_cast<double>(s0) * static_cast<double>(ka) + static_cast<double>(sample))
                          - static_cast<double>(s1) * static_cast<double>(kb);
        float t = static_cast<float>(tw);                                // fstp at 0x40ec4
        sample = static_cast<float>(
            ((NEG(codecHighPassFilter) == 2 ? tw : static_cast<double>(t))   // control: no spill of t
             - (static_cast<double>(s0) + static_cast<double>(s0)))
            + static_cast<double>(s1));                                  // fstp at 0x40ed2
        s1 = s0;                                                         // 0x40ed8
        s0 = t;                                                          // 0x40ede
        t = static_cast<float>(
            (static_cast<double>(s2) * static_cast<double>(kc) + static_cast<double>(sample))
            - static_cast<double>(s3) * static_cast<double>(kd));        // fstp at 0x40ef8
        sample = static_cast<float>(
            (static_cast<double>(t) - (static_cast<double>(s2) + static_cast<double>(s2)))
            + static_cast<double>(s3));                                  // fstp at 0x40f06
        s3 = s2;                                                         // 0x40f0c
        s2 = t;                                                          // 0x40f12
        samples[i] = static_cast<float>(static_cast<double>(sample) * static_cast<double>(ke));  // 0x40f24
    }
    state[0] = s0; state[1] = s1; state[2] = s2; state[3] = s3;          // 0x40f32-0x40f4c
    return 0;
}
void a_codecHighPassFilter(Ctx &ctx, const uint32_t *a, Ret &r) {
  const CxScope s(ctx);
  r.set_i32(static_cast<int32_t>(neg_u32(NEG(codecHighPassFilter), static_cast<u32>(codecHighPassFilter_body(P<float>(a[0]), P<int32_t>(a[1]), P<int32_t>(a[2]), P<float>(a[3]))))));
}

// ---- codecRing5Add (rva 0x40f60): from c3az.cpp CodecCombFilter_re
// 0x00040f60  codecCombFilter (VA 0x40f60)
// with one pointer argument ([ebp+8]; `ret` at 0x41017 pops nothing). Three addresses are derived
// from it: a cursor at +0xbd8 (0x40f7b), a second cursor at +0xbdc (0x40f86) and the base of a 16-bit
// delay line at +0xbe0 (0x40f92). Both cursors are 1-based: every access is `[base + cursor*2 - 2]`.
// The entry under the second cursor is replaced by the 16-BIT sum of itself and the entry under the first
// (`mov cx, [...]` at 0x40fab, `add cx, [...]` at 0x40fb0, `mov [...], cx` at 0x40fbd), so the add
// wraps at 16 bits. That entry is then re-read and sign-extended as the return value (`movsx` at 0x40fca,
// stored at 0x40fcf and loaded into eax at 0x4100e).
// Both cursors are then decremented and wrapped: `sub edx, 1` / `cmp dword [...], 0` / `jg` / `mov dword
// [...], 5` for the second cursor (0x40fd7-0x40fea, SIGNED) and the same for the first
// (0x40ff5-0x41008), so each runs 5, 4, 3, 2, 1, 5, ... and the delay line holds five entries.
int codecRing5Add_body(P<char> ctx) {
    P<int32_t> cursorA = pc<int32_t>(pc<char>(ctx) + 0xbd8);
    P<int32_t> cursorB = pc<int32_t>(pc<char>(ctx) + 0xbdc);
    P<int16_t> line = pc<int16_t>(pc<char>(ctx) + 0xbe0);
    unsigned short sum = static_cast<unsigned short>(
        static_cast<unsigned short>(line[*cursorB - 1]) + static_cast<unsigned short>(line[*cursorA - 1]));
    line[*cursorB - 1] = static_cast<short>(sum);
    int result = line[*cursorB - 1];                // movsx at 0x40fca
    *cursorB -= 1;
    if (*cursorB <= 0) *cursorB = 5;                // jg at 0x40fe5
    *cursorA -= 1;
    if (*cursorA <= 0) *cursorA = 5;                // jg at 0x41003
    return result;
}
void a_codecRing5Add(Ctx &ctx, const uint32_t *a, Ret &r) {
  const CxScope s(ctx);
  r.set_i32(static_cast<int32_t>(neg_u32(NEG(codecRing5Add), static_cast<u32>(codecRing5Add_body(P<char>(a[0]))))));
}

// ---- codecWindowFeatures (rva 0x41020): from c3bi.cpp CodecWindowFeatures_re

int codecWindowFeatures_body(P<int32_t> range, P<float> first, P<float> second,
                                   P<int32_t> descriptor, P<int32_t> subFrame, P<float> dither,
                                   P<int32_t> lag, P<int32_t> crossings, P<int32_t> level1, P<int32_t> level2,
                                   P<float> stepRatio, P<float> lagProduct, P<float> corrMinus,
                                   P<float> corrPlus) {
    const CodecFrame fr(cx(), 0xBC);                         // the original's frame, 0xcc-filled
    const R<float> scratch = *fr.loc<float>(0x10);           // [ebp-0x10], handed to floatCopySign / floatRoundHalfAway
    range = range - 1;                                       // 0x41041
    descriptor = descriptor - 1;                             // 0x4104a
    second -= descriptor[3];                                 // [descriptor+0xc], 0x41053-0x41064
    first -= descriptor[1];                                  // [descriptor+4],   0x4106a-0x4107b

    float accSecondAbs = 0.0f;                               // [ebp-0x38] 0x4107e
    float accFirstAbs = 0.0f;                                // [ebp-0x2c] 0x41085
    float accStepAbs = 0.0f;                                 // [ebp-0x24] 0x4108c
    float accFirstSq = 0.0f;                                 // [ebp-0x4c] 0x41093
    *lagProduct = 0.0f;                                      // 0x4109d
    float accSecondSq = 0.0f;                                // [ebp-0x30] 0x410a3
    float accMinusSq = 0.0f;                                 // [ebp-0x3c] 0x410aa
    float accPlusSq = 0.0f;                                  // [ebp-0x40] 0x410b1
    float accPlusProd = 0.0f;                                // [ebp-0x48] 0x410b8
    float accMinusProd = 0.0f;                               // [ebp-0x44] 0x410bf
    *crossings = 0;                                          // 0x410c9

    const float zero = rf(0x0005EC00);
    const float one = rf(0x0005B8A8);
    const float k90 = rf(0x0005EC30);
    const float k4 = rf(0x0005EC20);
    P<float> signOne = pc<float>(sva(0x00064E40));
    const auto copySign = [](const auto &... a) { return ccall_f(0x0003E380, a...); };
    const auto floorToInt = [](const auto &... a) { return ccall(0x0003E400, a...); };

    const int window = range[2] - range[1] + 1;              // 0x410d5-0x410de
    const int start = range[1] + (((*subFrame - 1) * window) / 2) + 1;   // 0x410e1-0x410fc
    const int end = start + (window / 2) - 1;                            // 0x410ff-0x4110e

    scratch = static_cast<float>(
        static_cast<double>(first[start - 1]) - static_cast<double>(*dither));   // 0x4111a-0x41120
    float runningSign = static_cast<float>(copySign(signOne, &scratch));         // 0x4112c-0x41134

    for (int i = start; i <= end; ++i) {                     // jg at 0x41154
        float v = second[i];                                 // 0x41160
        float magnitude = (static_cast<double>(v) < static_cast<double>(zero)) ? -v : v;  // 0x41181
        accSecondAbs = static_cast<float>(
            static_cast<double>(accSecondAbs) + static_cast<double>(magnitude));          // 0x4118c

        v = first[i];                                                                     // 0x41195
        magnitude = (static_cast<double>(v) < static_cast<double>(zero)) ? -v : v;         // 0x411b6
        accFirstAbs = static_cast<float>(
            static_cast<double>(accFirstAbs) + static_cast<double>(magnitude));            // 0x411c1

        const double step = static_cast<double>(first[i]) - static_cast<double>(first[i - 1]);  // 0x411d3
        const float stepStored = static_cast<float>(step);                                 // fst dword at 0x411d7
        magnitude = (step < static_cast<double>(zero)) ? -stepStored : stepStored;         // 0x411f2
        accStepAbs = static_cast<float>(
            static_cast<double>(accStepAbs) + static_cast<double>(magnitude));             // 0x411fd

        v = first[i];                                                                      // 0x41206
        accFirstSq = static_cast<float>(
            static_cast<double>(v) * static_cast<double>(v) + static_cast<double>(accFirstSq));   // 0x41215
        *lagProduct = static_cast<float>(
            static_cast<double>(first[i]) * static_cast<double>(first[i - 1])
            + static_cast<double>(*lagProduct));                                           // 0x41224-0x41233

        v = second[i];                                                                     // 0x4123b
        accSecondSq = static_cast<float>(
            static_cast<double>(v) * static_cast<double>(v) + static_cast<double>(accSecondSq));  // 0x4124a
        v = second[i - *lag];                                                              // 0x41258
        accMinusSq = static_cast<float>(
            static_cast<double>(v) * static_cast<double>(v) + static_cast<double>(accMinusSq));   // 0x41267
        v = second[i + *lag];                                                              // 0x41275
        accPlusSq = static_cast<float>(
            static_cast<double>(v) * static_cast<double>(v) + static_cast<double>(accPlusSq));    // 0x41284
        accPlusProd = static_cast<float>(
            static_cast<double>(second[i]) * static_cast<double>(second[i + *lag])
            + static_cast<double>(accPlusProd));                                           // 0x41298-0x412a1
        accMinusProd = static_cast<float>(
            static_cast<double>(second[i]) * static_cast<double>(second[i - *lag])
            + static_cast<double>(accMinusProd));                                          // 0x412b5-0x412be

        scratch = static_cast<float>(
            static_cast<double>(first[i]) + static_cast<double>(*dither));                 // 0x412ca-0x412cf
        const float dithered = static_cast<float>(copySign(signOne, &scratch));           // 0x412db
        if (static_cast<double>(dithered) != static_cast<double>(runningSign)) {           // jne at 0x412ed
            *crossings += 1;                                                               // 0x412f4-0x412fa
            runningSign = -runningSign;                                                    // 0x412ff
        }
        *dither = -*dither;                                                                // 0x41309
    }

    const float energyDiv = (static_cast<double>(accFirstSq) < static_cast<double>(one))
                                ? one : accFirstSq;                                        // jne at 0x41323
    *lagProduct = static_cast<float>(
        static_cast<double>(*lagProduct) / static_cast<double>(energyDiv));                // 0x41339

    const double twiceLevel = static_cast<double>(accFirstAbs) + static_cast<double>(accFirstAbs);  // 0x41344
    const float twiceStored = static_cast<float>(twiceLevel);                              // fst dword at 0x41346
    const float stepDiv = (twiceLevel < static_cast<double>(one)) ? one : twiceStored;     // jne at 0x41354
    *stepRatio = static_cast<float>(
        static_cast<double>(accStepAbs) / static_cast<double>(stepDiv));                   // 0x41368

    const float minusDiv = (static_cast<double>(accMinusSq) < static_cast<double>(one))
                               ? one : accMinusSq;                                         // jne at 0x4137e
    const float ownDivA = (static_cast<double>(accSecondSq) < static_cast<double>(one))
                              ? one : accSecondSq;                                         // jne at 0x4139d
    *corrMinus = static_cast<float>(
        (static_cast<double>(accMinusProd) / static_cast<double>(minusDiv))
        * (static_cast<double>(accMinusProd) / static_cast<double>(ownDivA)));             // 0x413ae-0x413bf

    const float plusDiv = (static_cast<double>(accPlusSq) < static_cast<double>(one))
                              ? one : accPlusSq;                                           // jne at 0x413cf
    const float ownDivB = (static_cast<double>(accSecondSq) < static_cast<double>(one))
                              ? one : accSecondSq;                                         // jne at 0x413ee
    *corrPlus = static_cast<float>(
        (static_cast<double>(accPlusProd) / static_cast<double>(plusDiv))
        * (static_cast<double>(accPlusProd) / static_cast<double>(ownDivB)));              // 0x413ff-0x41410

    const int doubled = *crossings * 2;                                                    // 0x41417
    const float doubledF = static_cast<float>(doubled);                                    // fild/fstp at 0x4141c
    scratch = static_cast<float>(
        (static_cast<double>(k90) / static_cast<double>(window)) * static_cast<double>(doubledF));  // 0x41422-0x4142e
    *crossings = floorToInt(&scratch);                                                     // 0x41435, 0x41440

    scratch = static_cast<float>(
        (static_cast<double>(accSecondAbs) / static_cast<double>(k4))
        * (static_cast<double>(k90) / static_cast<double>(window)));                       // 0x41442-0x41456
    int rounded = floorToInt(&scratch);                                                    // 0x4145d
    *level1 = (rounded > 0x7FFF) ? 0x7FFF : rounded;                                       // jg at 0x4146f

    scratch = static_cast<float>(
        (static_cast<double>(accFirstAbs) / static_cast<double>(k4))
        * (static_cast<double>(k90) / static_cast<double>(window)));                       // 0x41488-0x4149c
    rounded = floorToInt(&scratch);                                                        // 0x414a3
    *level2 = (rounded > 0x7FFF) ? 0x7FFF : rounded;                                       // jg at 0x414b5
    return 0;                                                                              // 0x414ce
}
void a_codecWindowFeatures(Ctx &ctx, const uint32_t *a, Ret &r) {
  const CxScope s(ctx);
  r.set_i32(static_cast<int32_t>(neg_u32(NEG(codecWindowFeatures), static_cast<u32>(codecWindowFeatures_body(P<int32_t>(a[0]), P<float>(a[1]), P<float>(a[2]), P<int32_t>(a[3]), P<int32_t>(a[4]), P<float>(a[5]), P<int32_t>(a[6]), P<int32_t>(a[7]), P<int32_t>(a[8]), P<int32_t>(a[9]), P<float>(a[10]), P<float>(a[11]), P<float>(a[12]), P<float>(a[13]))))));
}

// ---- codecAmdfPitch (rva 0x414f0): from c3bi.cpp CodecAmdfPitch_re

int codecAmdfPitch_body(P<float> samples, P<int32_t> span, P<int32_t> lags, P<int32_t> lagCount,
                              P<int32_t> window, P<float> results, P<int32_t> smallest, P<int32_t> largest) {
    results -= 1;                                            // 0x4150b
    lags -= 1;                                               // 0x41514
    samples -= 1;                                            // 0x4151d
    *smallest = 1;                                           // 0x41526
    *largest = 1;                                            // 0x4152f

    const float zero = rf(0x0005EC00);
    const int n = *lagCount;                                 // 0x41538
    for (int i = 1; i <= n; ++i) {                           // jg at 0x41555
        const int start = ((*window - lags[i]) / 2) + 1;     // 0x41566-0x4156e
        const int end = start + *span - 1;                   // 0x4157c
        float acc = 0.0f;                                    // 0x41583
        for (int j = start; j <= end; j += 4) {              // jg at 0x415a7, add 4 at 0x4159b
            const int k = j + lags[i];                       // 0x415b2
            const double diff = static_cast<double>(samples[j]) - static_cast<double>(samples[k]);
            const float stored = static_cast<float>(diff);   // fst dword at 0x415c4
            const float magnitude = (diff < static_cast<double>(zero)) ? -stored : stored;  // 0x415df
            acc = static_cast<float>(static_cast<double>(acc) + static_cast<double>(magnitude));  // 0x415ea
        }
        results[i] = acc;                                    // 0x415f8

        if (static_cast<double>(results[i]) < static_cast<double>(results[*smallest]))   // je at 0x41614
            *smallest = i;                                   // 0x4161c
        if (static_cast<double>(results[i]) > static_cast<double>(results[*largest]))    // jne at 0x41637
            *largest = i;                                    // 0x4163f
    }
    return 0;                                                // 0x41646
}
void a_codecAmdfPitch(Ctx &ctx, const uint32_t *a, Ret &r) {
  const CxScope s(ctx);
  r.set_i32(static_cast<int32_t>(neg_u32(NEG(codecAmdfPitch), static_cast<u32>(codecAmdfPitch_body(P<float>(a[0]), P<int32_t>(a[1]), P<int32_t>(a[2]), P<int32_t>(a[3]), P<int32_t>(a[4]), P<float>(a[5]), P<int32_t>(a[6]), P<int32_t>(a[7]))))));
}

const FuncDef kFuncs[] = {
    {"snd.codecDecodeFrame", "default", "int", 3, 0x027f, a_codecDecodeFrame, &g_negctl_codecDecodeFrame},
    {"snd.codecSetParams", "default", "uint32", 0, 0, a_codecSetParams, &g_negctl_codecSetParams},
    {"snd.codecAllocEncoderContext", "default", "uint32", 0, 0, a_codecAllocEncoderContext, &g_negctl_codecAllocEncoderContext},
    {"snd.codecInitEncoderContext", "default", "void", 1, 0x027f, a_codecInitEncoderContext, &g_negctl_codecInitEncoderContext},
    {"snd.codecAllocDecoderContext", "default", "uint32", 0, 0, a_codecAllocDecoderContext, &g_negctl_codecAllocDecoderContext},
    {"snd.codecInitDecoderContext", "default", "void", 1, 0, a_codecInitDecoderContext, &g_negctl_codecInitDecoderContext},
    {"snd.codecEncodeFrame", "default", "int", 3, 0x027f, a_codecEncodeFrame, &g_negctl_codecEncodeFrame},
    {"snd.codecSynthesizeFrame", "default", "int", 7, 0x027f, a_codecSynthesizeFrame, &g_negctl_codecSynthesizeFrame},
    {"snd.codecDecodeParams", "default", "int", 8, 0x027f, a_codecDecodeParams, &g_negctl_codecDecodeParams},
    {"snd.bitPermute", "default", "int", 7, 0, a_bitPermute, &g_negctl_bitPermute},
    {"snd.bitPack", "default", "int", 6, 0, a_bitPack, &g_negctl_bitPack},
    {"snd.bitUnpack", "default", "int", 5, 0, a_bitUnpack, &g_negctl_bitUnpack},
    {"snd.quantizeParams", "default", "int", 7, 0x027f, a_quantizeParams, &g_negctl_quantizeParams},
    {"snd.codecAnalyzeFrame", "default", "int", 6, 0x027f, a_codecAnalyzeFrame, &g_negctl_codecAnalyzeFrame},
    {"snd.codecHighPassFrame", "default", "int", 3, 0x027f, a_codecHighPassFrame, &g_negctl_codecHighPassFrame},
    {"snd.codecSynthPostFilter", "default", "int", 3, 0x027f, a_codecSynthPostFilter, &g_negctl_codecSynthPostFilter},
    {"snd.lpcSynthesize", "default", "int", 8, 0x027f, a_lpcSynthesize, &g_negctl_lpcSynthesize},
    {"snd.reflectionToLpc", "default", "int", 5, 0x027f, a_reflectionToLpc, &g_negctl_reflectionToLpc},
    {"snd.interpSubframeParams", "default", "int", 13, 0x027f, a_interpSubframeParams, &g_negctl_interpSubframeParams},
    {"snd.intPow", "default", "int", 2, 0, a_intPow, &g_negctl_intPow},
    {"snd.floatCopySign", "default", "double", 2, 0x027f, a_floatCopySign, &g_negctl_floatCopySign},
    {"snd.floatRoundHalfAway", "default", "int", 1, 0x027f, a_floatRoundHalfAway, &g_negctl_floatRoundHalfAway},
    {"snd.medianOfThree", "default", "int", 3, 0, a_medianOfThree, &g_negctl_medianOfThree},
    {"snd.codecCorrectNibble", "default", "int", 3, 0, a_codecCorrectNibble, &g_negctl_codecCorrectNibble},
    {"snd.codecRevertUnstableCoeffs", "default", "int", 3, 0x027f, a_codecRevertUnstableCoeffs, &g_negctl_codecRevertUnstableCoeffs},
    {"snd.codecLdlSolveClamped", "default", "int", 4, 0x027f, a_codecLdlSolveClamped, &g_negctl_codecLdlSolveClamped},
    {"snd.codecCovarianceMatrix", "default", "int", 6, 0x027f, a_codecCovarianceMatrix, &g_negctl_codecCovarianceMatrix},
    {"snd.codecRmsEnergy", "default", "int", 3, 0x027f, a_codecRmsEnergy, &g_negctl_codecRmsEnergy},
    {"snd.codecRemoveMean", "default", "int", 3, 0x027f, a_codecRemoveMean, &g_negctl_codecRemoveMean},
    {"snd.codecAlignAnalysisWindow", "default", "int", 9, 0x027f, a_codecAlignAnalysisWindow, &g_negctl_codecAlignAnalysisWindow},
    {"snd.codecPitchTrackDp", "default", "int", 7, 0x027f, a_codecPitchTrackDp, &g_negctl_codecPitchTrackDp},
    {"snd.codecVoicingDecision", "default", "int", 13, 0x027f, a_codecVoicingDecision, &g_negctl_codecVoicingDecision},
    {"snd.codecRefinePitchLag", "default", "int", 8, 0x027f, a_codecRefinePitchLag, &g_negctl_codecRefinePitchLag},
    {"snd.codecInverseFilter2", "default", "int", 5, 0x027f, a_codecInverseFilter2, &g_negctl_codecInverseFilter2},
    {"snd.codecLowPassFir31", "default", "int", 4, 0x027f, a_codecLowPassFir31, &g_negctl_codecLowPassFir31},
    {"snd.codecSelectWindowFromJumps", "default", "int", 11, 0x027f, a_codecSelectWindowFromJumps, &g_negctl_codecSelectWindowFromJumps},
    {"snd.codecMarkCorrelationJumps", "default", "int", 8, 0x027f, a_codecMarkCorrelationJumps, &g_negctl_codecMarkCorrelationJumps},
    {"snd.codecPreEmphasis", "default", "int", 5, 0x027f, a_codecPreEmphasis, &g_negctl_codecPreEmphasis},
    {"snd.codecHighPassFilter", "default", "int", 4, 0x027f, a_codecHighPassFilter, &g_negctl_codecHighPassFilter},
    {"snd.codecRing5Add", "default", "int", 1, 0x027f, a_codecRing5Add, &g_negctl_codecRing5Add},
    {"snd.codecWindowFeatures", "default", "int", 14, 0x027f, a_codecWindowFeatures, &g_negctl_codecWindowFeatures},
    {"snd.codecAmdfPitch", "default", "int", 8, 0x027f, a_codecAmdfPitch, &g_negctl_codecAmdfPitch},
};

}  // namespace

extern const ModuleDef kModule_audio_codec = {"audio_codec", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim

// The audio core (step S4.2, log/roadmap/S4/2_notes.md): sound.dll's port runs over the context's 32-bit address space, with
// the module's .data (portsim::SoundState, soundstate.h) and its read-only sections at a base the caller chooses.
//
// WRITING A SOUND.DLL PORT FUNCTION (S4.3..S4.9)
//   * Arguments are the original's 32-bit slots (thiscall: slot 0 = this; fastcall: slots 0 / 1 = ecx / edx). Pointers are
//     virtual addresses: read and write them with ld<T> / st<T> (or Ptr<T> from ptr.h), never a host pointer.
//   * Module globals are SoundState members: snd_obj(ctx, &SoundState::g_soundEngine).get(). Constants and vtables of
//     .rdata are read where the original reads them: snd_va(ctx, snd_rva::vt_CtrlObject) is the address it stores, and
//     ld<float>(ctx, snd_va(ctx, snd_rva::g_float1)) the value it loads. No table byte is copied into port code.
//   * EVERY call the original makes to code that is not inline in it goes through the dispatcher: snd_call (a sound.dll
//     function by rva, thunk or body), snd_vcall (a virtual slot), snd_call_va (a pointer read from memory), snd_import
//     (an IAT slot). The dispatcher honours the host's claims (a fixture's stand-in), runs the registered port function of
//     an rva, and hands everything else to the host. A direct C++ call to another port function would bypass a stand-in.
//   * The callee's eax / edx / st(0) come back in a Ret; a function that returns "whatever eax the callee left" returns it.
//   * Register the function in audio_core's (or your module's) FuncDef table under its hooks.csv name: the dispatcher
//     finds it by the rva -> name table of soundstate.h.
//   * A local the original passes BY ADDRESS (a COM out-parameter, a MSG, a WAVEFORMATEX, a cursor a callee writes) lives
//     at the original's offset below the function's entry esp: `SndFrame f(ctx, n)` with n = the original's frame at the
//     call (callee-saved pushes + `sub esp` + pushed locals), f.at(off) = entry esp - off (S4.2b; "the stack" below).
//   * x87 code runs on the context's virtual x87 unit (x87::unit(ctx), "the x87 unit" below): every fld / fop / fst / fcom
//     of the original is one x87:: operation in the original's order, under the unit's control word, so the precision and
//     rounding the caller left (or the function loads itself) give the original's bits on x64 and Win32 alike. A function
//     returning float / double leaves its result pushed on the unit (and in Ret, as the caller's fstp would store it).
#ifndef PORTSIM_SIM_AUDIO_H
#define PORTSIM_SIM_AUDIO_H

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

#include "portsim/ptr.h"
#include "portsim/registry.h"
#include "portsim/sim/soundstate.h"

namespace portsim {

class AudioSink;  // portsim/sim/audio_sink.h

// ------------------------------------------------------------------------------------------------ calls outside the port
enum class SndAbi : int32_t { Cdecl = 0, Stdcall = 1, Thiscall = 2, Fastcall = 3 };
enum class SndKind : int32_t {
  External = 0,  // outside the image: a callback, a system DLL export, a COM method
  Claimed = 1,   // inside the image, claimed by the host (a fixture's Interceptor.replace stand-in)
  Fallback = 2,  // inside the image, no port function registered yet: DEVELOPMENT ONLY (the final gate wants none)
};
constexpr int kSndMaxArgs = 16;

// One call the port hands to the host. The layout is the C ABI's sg_snd_call (portsim_c.h, S4.2 merge request).
struct SndCallC {
  uint32_t va;       // target virtual address (after the thunks the dispatcher followed)
  uint32_t rva;      // va - image base when inside the image, else 0
  int32_t kind;      // SndKind
  int32_t abi;       // SndAbi
  int32_t nargs;     // argument slots used in args
  int32_t want_st0;  // the caller reads st(0) (a float / double return)
  uint32_t args[kSndMaxArgs];
  uint32_t eax, edx;  // results, written by the host
  double st0;
  int32_t has_st0;
  int32_t status;  // 0 = served; anything else stops the port call (SG_E_EXCEPTION)
};
using SndHostFn = int (*)(void *user, SndCallC *call);

// A host serves SndCallC requests. The test harness's live callback and the recorded script are hosts.
class SoundHost {
 public:
  virtual ~SoundHost() = default;
  virtual void call(Ctx &ctx, SndCallC &c) = 0;
};

// The host refused a call or a recorded script disagreed with the port's request. what() is in audio_detail().
struct SoundHostError {
  std::string what;
};

// ------------------------------------------------------------------------------------------------ attach
// Attach sound.dll at `image_base` (64 KB aligned, inside the 32-bit space). map_image = false: the caller maps the image
// itself (the replay harness maps the emulated original's pages, .data included). map_image = true: the image is mapped
// here from the context's GameFiles "sound.dll" (the user's file, relocated to image_base; .data from the file, bss zero)
// after its section table is checked against soundstate.h (.data size, .rdata hash): MissingResource otherwise.
// map_image maps once per context (a second map_image attach overlaps the first mapping: MissingResource), until the caller's
// sg_unmap_all. audio_detach drops the context's audio state; blocks the core mapped stay alive (the mappings may still
// point at them) until audio_release, which sg_state_free calls.
void audio_attach(Ctx &ctx, uint32_t image_base, bool map_image);
void audio_detach(Ctx &ctx);
void audio_release(Ctx &ctx);
bool audio_attached(const Ctx &ctx);
uint32_t snd_base(const Ctx &ctx);  // MissingResource when not attached
inline uint32_t snd_va(const Ctx &ctx, uint32_t rva) { return snd_base(ctx) + rva; }
uint32_t snd_data_va(const Ctx &ctx);  // image base + kSoundDataRva

// The host (not owned). The C ABI's callback host and the script host set it.
void audio_set_host(Ctx &ctx, SoundHost *host);
// A C callback as the host (the C ABI export sg_snd_set_host calls this); fn = nullptr removes it.
void audio_set_callback(Ctx &ctx, SndHostFn fn, void *user);
// VAs inside the image the host serves instead of the port (Interceptor.replace targets of the replay).
void audio_claim(Ctx &ctx, uint32_t va);
void audio_clear_claims(Ctx &ctx);
// The AudioSink that serves imports and COM methods when no host is attached (production; S4.10).
void audio_set_sink(Ctx &ctx, AudioSink *sink);

// Counters since the last audio_reset_counters: host calls by kind and port functions entered through the dispatcher.
struct AudioCounters {
  uint32_t external = 0, claimed = 0, fallback = 0, dispatched = 0;
  std::vector<uint32_t> fallback_rvas;  // one entry per fallback call, in order
};
const AudioCounters &audio_counters(const Ctx &ctx);
void audio_reset_counters(Ctx &ctx);
const std::string &audio_detail(const Ctx &ctx);  // why the last host call failed (empty when none did)

// ------------------------------------------------------------------------------------------------ memory
template <class T> inline T ld(const Ctx &ctx, uint32_t va) { return ctx.mem.load<T>(va); }
template <class T> inline void st(const Ctx &ctx, uint32_t va, T v) { ctx.mem.store<T>(va, v); }

namespace detail {
uint32_t snd_member_offset_bytes(const void *member_in_probe);
const SoundState &snd_probe();
}  // namespace detail
// The address of a SoundState member (a scalar, a struct, or a whole array as one object).
template <class M> Ptr<M> snd_obj(const Ctx &ctx, M SoundState::*m) {
  return Ptr<M>(ctx.mem, snd_data_va(ctx) + detail::snd_member_offset_bytes(&(detail::snd_probe().*m)));
}
// Element 0 of an array member.
template <class M, size_t N> Ptr<M> snd_arr(const Ctx &ctx, M (SoundState::*m)[N]) {
  return Ptr<M>(ctx.mem, snd_data_va(ctx) + detail::snd_member_offset_bytes(&(detail::snd_probe().*m)[0]));
}

// ------------------------------------------------------------------------------------------------ the dispatcher
// Call the code at `va` with the original's argument slots. want_st0: the caller reads st(0).
Ret snd_call_va(Ctx &ctx, uint32_t va, SndAbi abi, std::initializer_list<uint32_t> args, bool want_st0 = false);
Ret snd_call_va(Ctx &ctx, uint32_t va, SndAbi abi, const uint32_t *args, int nargs, bool want_st0 = false);
// A sound.dll function (or its incremental-link thunk) by rva.
inline Ret snd_call(Ctx &ctx, uint32_t rva, SndAbi abi, std::initializer_list<uint32_t> args, bool want_st0 = false) {
  return snd_call_va(ctx, snd_va(ctx, rva), abi, args, want_st0);
}
// A virtual slot: the code at [[obj] + slot_off], thiscall with obj first (stdcall with obj first for COM interfaces).
Ret snd_vcall(Ctx &ctx, uint32_t obj, uint32_t slot_off, SndAbi abi, std::initializer_list<uint32_t> args,
              bool want_st0 = false);
// STACK RESIDUE: the original pushes (or loads into ecx) words its callee does not read, e.g. `push 0xe; call [edx + 0x40]`
// where slot 0x40 is `mov eax, [ecx + 0x1ac]; ret` and the 0xe stays on the stack as the next call's argument. Inside a
// SndResidue scope the call passes those words like any slot (a host or stand-in sees them where the original's callee
// would) and a port callee of a SMALLER arity is entered anyway and ignores them; outside one, an arity mismatch is an error.
// The scope covers the direct call only (the callee's own calls are checked again).
class SndResidue {
 public:
  explicit SndResidue(Ctx &ctx);
  ~SndResidue();
  SndResidue(const SndResidue &) = delete;
  SndResidue &operator=(const SndResidue &) = delete;

 private:
  Ctx &ctx_;
  bool prev_;
};

// An import through its IAT slot (rva of the slot): the slot's value is called (the replay binds it); with no host and
// an unbound slot the AudioSink receives it by the import's DLL!name from the image's import directory.
Ret snd_import(Ctx &ctx, uint32_t iat_rva, SndAbi abi, std::initializer_list<uint32_t> args, bool want_st0 = false);

// The static CRT entry points the hook sources cite (c3cn Part 4: operator new `push 1; push size; call ...`, operator
// delete, malloc; c3cx getNodeByIndex: _strncpy). They are sound.dll code, reached through the dispatcher like any other.
namespace snd_crt {
constexpr uint32_t operator_new = 0x424d7;
constexpr uint32_t operator_delete = 0x4249a;
constexpr uint32_t malloc_ = 0x42acd;
constexpr uint32_t strncpy_ = 0x42710;
constexpr uint32_t ftol_ = 0x42550;  // __ftol: st(0) popped off the x87 unit, toward zero, eax:edx = the 64-bit integer
}  // namespace snd_crt
inline uint32_t snd_new(Ctx &ctx, uint32_t n) { return snd_call(ctx, snd_crt::operator_new, SndAbi::Cdecl, {n}).eax; }
inline Ret snd_delete(Ctx &ctx, uint32_t p) { return snd_call(ctx, snd_crt::operator_delete, SndAbi::Cdecl, {p}); }
// `call __ftol` with the value on the x87 unit's st(0) (registered "audio_core.crt.ftol")
inline Ret snd_ftol(Ctx &ctx) { return snd_call(ctx, snd_crt::ftol_, SndAbi::Cdecl, {}); }

// The registered port function the dispatcher runs for an rva (thunks followed), or nullptr.
const FuncDef *snd_port_function(Ctx &ctx, uint32_t rva);

// ------------------------------------------------------------------------------------------------ the stack (S4.2b)
// One virtual stack per context, in the 32-bit space, for the locals the original passes by address.
//   * The ENTRY ESP of a function is the address of its return address. A top-level call (the harness, the exe's sound
//     layer) gives it with snd_set_entry_esp (configuration entry "audio_core.esp <esp> <limit>": the esp the original's
//     arm entered with, and the lowest address a port frame may reach, above the frames of the host's own calls). With
//     none, the core's FRAME BLOCK [kSndFrameBase, kSndFrameBase + kSndFrameSize) is the stack (mapped on first use,
//     zero-filled per frame, entry esp = its top - 0x44, so 16 argument slots lie inside it): production, where no callee
//     compares addresses.
//   * A port function the dispatcher calls (snd_call / snd_vcall / snd_call_va) enters at the esp the original's `call`
//     leaves: the caller's esp minus its stack argument slots (thiscall: all but ecx, fastcall: all but ecx / edx) minus
//     the return address. The caller's esp is its entry esp minus its SndFrame (none: the entry esp).
//   * SndFrame reserves the frame for the function's lifetime; f.at(off) is the local at entry esp - off (1 <= off <= n).
//     A SndFrame built while another frame of the running function is live (a body the function calls directly, not
//     through the dispatcher) enters below it, at the live esp - 4, as a `call` would.
//     Bytes the original leaves uninitialised stay as the memory holds them; the body writes what the original writes.
// Replaces S4.4's x87::frame_push, S4.6's frame cells in the image padding and S4.8's SndLocals (U-1418).
constexpr uint32_t kSndFrameBase = 0xFEB00000u, kSndFrameSize = 0x10000u;

class SndFrame {
 public:
  SndFrame(Ctx &ctx, uint32_t bytes);
  ~SndFrame();
  SndFrame(const SndFrame &) = delete;
  SndFrame &operator=(const SndFrame &) = delete;
  uint32_t entry() const { return entry_; }
  uint32_t at(uint32_t off) const { return entry_ - off; }
  // the caller's k-th stack argument slot (entry esp + 4 + 4k): a body whose original reuses an argument slot as a local
  // (renderChannelSingle) writes and passes it there
  uint32_t arg(uint32_t k) const { return entry_ + 4u + 4u * k; }
  // the esp of a frame whose size is only known from the entry esp (`and esp, -8` frames): esp <= entry, checked like n
  void set_esp(uint32_t esp);
  bool on_stack() const { return !block_; }  // the entry esp came from the caller (else the frame block)

 private:
  Ctx &ctx_;
  uint32_t entry_ = 0, prev_esp_ = 0, prev_entry_ = 0;
  bool block_ = false, lazy_ = false;
};

// The entry esp of the next top-level port call and the lowest address its frames may use (0 = no limit); esp 0 = the
// frame block. Kept until the next audio_attach (the harness sets it per vector, after audio_core.setup).
void snd_set_entry_esp(Ctx &ctx, uint32_t esp, uint32_t limit = 0);
uint32_t snd_entry_esp(Ctx &ctx);  // of the running port function (0 = none: no frame and no caller-given esp)
uint32_t snd_esp(Ctx &ctx);        // its esp: the entry esp minus its SndFrame

// ------------------------------------------------------------------------------------------------ the x87 unit (S4.4 -> core)
// THE X87 UNIT: the thread's FPU as per-context state that persists from one sound.dll call to the next (control word,
// TOP, C0..C3, which registers are empty, their 80 bits). The Voice_mix* leaves and the stream renderers load 0x0f7f
// (64-bit significand, chop), never restore it and return with 4-5 registers pushed, and the caller-word functions
// (Channel_setRate, DsBuffer::setVolume, ...) compute at whatever word that leaves (log/c3/c3fh_notes.txt), so every x87
// instruction of a body is one operation of portsim::x87 on 10-byte values under the unit's word:
//   * Win32 (_M_IX86): each operation is the x87 instruction itself (inline asm on tbyte operands under the unit's word).
//   * x64: the software path of port/src/sim/audio_x80.inc (exact 128-bit significands, one rounding per operation to
//     24 / 53 / 64 bits by the precision control, the rounding control, the x87 NaN rules). Win32 can run it too
//     ("audio_core.x80soft 1"), so the two paths are compared instruction by instruction.
//   * fyl2x is the REFERENCE EMULATOR's (Unicorn 2.1.4 / QEMU: both operands rounded to double, log2 in double, the product
//     in double, back exactly) on both backends: the instruction itself differs in the last bits (U-1471).
//   Not modelled (an operation throws X87Unmodelled): stack overflow / underflow, results outside the extended exponent
//   range, unmasked exceptions. Exception flags are not kept (U-1407: the emulated original sets none either).
// The harness loads the unit per vector with the FPU the original found ("audio_core.x87load" from "$audio/x87") and
// compares the unit the port leaves ("audio_core.x87save" -> "$audio/x87out"). A HOST call sees the unit: when the page
// kSndFpuBox is mapped (the harness maps it), the unit's image is written there before every host call and loaded back
// after it, so a host-run callee (the emulator, a recorded script) computes on, and leaves, the port's FPU.
namespace x87 {

// One x87 data register: 64-bit significand (explicit integer bit 63) and sign | 15-bit biased exponent.
struct X80 {
  uint64_t m = 0;
  uint16_t se = 0;
};
inline bool operator==(const X80 &a, const X80 &b) { return a.m == b.m && a.se == b.se; }

// An operation the unit does not model (see above); the message says which.
struct X87Unmodelled {
  std::string what;
};

// Condition-code bits of the status word.
constexpr uint16_t kC0 = 1u << 8, kC1 = 1u << 9, kC2 = 1u << 10, kC3 = 1u << 14;

// ---- operations (each one x87 instruction's arithmetic; `cw` gives precision and rounding control) -------------------
X80 add(X80 a, X80 b, uint16_t cw);    // a + b            (fadd)
X80 sub(X80 a, X80 b, uint16_t cw);    // a - b            (fsub; fsubr = sub(b, a))
X80 mul(X80 a, X80 b, uint16_t cw);    // a * b            (fmul)
X80 div(X80 a, X80 b, uint16_t cw);    // a / b            (fdiv; fdivr = div(b, a))
X80 from_f64(uint64_t bits);           // a double operand as the arithmetic sees it (fop qword: a signaling NaN stays signaling)
X80 from_f32(uint32_t bits);           // a single operand likewise (fop dword)
X80 fld64(uint64_t bits);              // fld qword: exact; a signaling NaN is quieted
X80 fld32(uint32_t bits);              // fld dword: exact; a signaling NaN is quieted
X80 fild(int64_t v);                   // fild word / dword / qword: exact
uint64_t fst64(X80 a, uint16_t cw);    // fst(p) qword: rounded to double by the rounding control
int64_t fist(X80 a, int bits, uint16_t cw);  // fist(p) word / dword / qword by the rounding control; out of range or NaN:
                                             // the integer indefinite of that width (sign-extended into the result)
uint16_t fcom(X80 a, X80 b);           // fcom: the C3 / C2 / C0 bits (C1 clear) for st(0) = a against b
X80 fabs_(X80 a);                      // fabs
X80 fchs(X80 a);                       // fchs
X80 indefinite();                      // the default QNaN
X80 ln2();                             // fldln2 as the reference emulator loads it (0x3ffe:b17217f7d1cf79ac)
X80 fyl2x(X80 st0, X80 st1, uint16_t cw);  // fyl2x: the new st(1) = st1 * log2(st0), as the reference emulator computes it
                                           // (see above); the caller pops st(0). st0 <= 0 throws X87Unmodelled.
double to_double(X80 a, uint16_t cw);  // fst64 as a C double

// Backend selection (Win32 only; x64 always runs the software path): true = the software path.
void set_soft(bool soft);
bool soft();

// The software path itself, on every architecture (the Win32 self-test compares the two).
namespace sw {
X80 add(X80 a, X80 b, uint16_t cw);
X80 sub(X80 a, X80 b, uint16_t cw);
X80 mul(X80 a, X80 b, uint16_t cw);
X80 div(X80 a, X80 b, uint16_t cw);
uint64_t fst64(X80 a, uint16_t cw);
int64_t fist(X80 a, int bits, uint16_t cw);
uint16_t fcom(X80 a, X80 b);
}  // namespace sw

// ---- the unit ---------------------------------------------------------------------------------------------------------
class Unit {
 public:
  uint16_t cw = 0x027f;   // the control word a fresh Windows thread has
  uint16_t sw = 0;        // TOP (bits 11-13) and C0..C3; exception flags are not kept
  uint8_t used = 0;       // bit i: physical register i holds a value (tag not empty)
  // C0..C3 are not known: a body ran whose compares are not modelled on the unit (cc_unmodelled); the next modelled compare
  // (set_cc) makes them known again. The harness does not compare C0..C3 while this is set (U-1419).
  bool cc_unknown = false;
  X80 reg[8];

  int top() const { return (sw >> 11) & 7; }
  void set_top(int t) { sw = static_cast<uint16_t>((sw & ~0x3800u) | ((static_cast<unsigned>(t) & 7u) << 11)); }
  int depth() const;                      // registers in use
  X80 &st(int i);                         // st(i); throws X87Unmodelled when empty
  void push(X80 v);                       // throws X87Unmodelled when the stack is full
  X80 pop();                              // throws X87Unmodelled when st(0) is empty
  void fxch(int i);                       // fxch st(i)
  void fninit();
  void fldcw(uint16_t w) { cw = w; }
  void set_cc(uint16_t c) {
    sw = static_cast<uint16_t>((sw & ~(kC0 | kC1 | kC2 | kC3)) | c);
    cc_unknown = false;
  }
  void cc_unmodelled() { cc_unknown = true; }
  bool cc(uint16_t bit) const { return (sw & bit) != 0; }
  // MMX: an MMX instruction makes every register valid with TOP 0; emms empties them all.
  void mmx_enter();
  void emms();

  // The image the harness exchanges ("$audio/x87", the host box): "SX87", u16 cw, u16 sw, u16 full tag word, u16 flags (bit 0:
  // cc_unknown), then physical registers 0..7 as u64 significand, u16 sign|exponent.
  static constexpr size_t kImageSize = 4 + 8 + 8 * 10;
  void load_image(const uint8_t *p);
  void save_image(uint8_t *p) const;
  uint16_t tag_word() const;              // the full tag word fnstenv reports (00 valid, 01 zero, 10 special, 11 empty)
};

// The context's unit (created at the default state on first use; it survives audio_attach / audio_detach, as the thread's
// FPU outlives a DLL; audio_release drops it).
Unit &unit(Ctx &ctx);
// Why the last body stopped on an operation the unit does not model ("audio_core.report" writes it as x87_detail).
void set_detail(Ctx &ctx, const std::string &what);
const std::string &detail(Ctx &ctx);

}  // namespace x87

// The page the harness maps for the host's view of the unit (see "the x87 unit").
constexpr uint32_t kSndFpuBox = 0xFEAF0000u;

// A body's x87 stop (X87Unmodelled) is kept as the context's detail and the call stops.
#define SG_X87_GUARD(ctx, ...)                     \
  try {                                            \
    __VA_ARGS__;                                   \
  } catch (const ::portsim::x87::X87Unmodelled &e_) { \
    ::portsim::x87::set_detail(ctx, e_.what);      \
    throw;                                         \
  }

}  // namespace portsim

#endif  // PORTSIM_SIM_AUDIO_H

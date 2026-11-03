// The sound.dll sound-object family (step S4.7, log/roadmap/S4/7_notes.md; API in portsim/sim/audio.h): Sound, Sound5b6a8 (the MIDI
// sound), Sound5c450 (the file sound), MultiSound / SoundGroup, SoundWindowObj, SoundTimer, the sound registry, create_sound and
// Engine_playSound: the 66 rows of S4.7 in re/roadmap/S4_functions.tsv, written from the C3 notes in the hook sources
// (shim/src/re/c3ar.cpp, c3au.cpp, c3cl.cpp, c3cx.cpp, c3dh.cpp, c3dj.cpp, c3do.cpp, c3dt.cpp, c3ew.cpp, c3ex.cpp, c3fd.cpp, c3fg.cpp,
// c3fh.cpp, c3fk.cpp, c3fl.cpp, c3fq.cpp, c3fza.cpp), then judged by the replay (port/test/sound_replay.py through
// log/roadmap/S4/7_run.py). Addresses in comments are RVAs of the user's sound.dll.
//
// Every call to code that is not inline goes through the dispatcher (snd_call / snd_vcall / snd_import), so a fixture's stand-in,
// the host or another port function serves it.
//
// Negative controls (sg_negctl_set(<name>, mode)): mode 1 of every function is a wrong result: the value it returns is changed
// (`neg_u32`) or, for a function that returns nothing, the dword at the field the adapter names is changed after the body
// (`neg_mem`). Mode 2 of Seq::ctor_1001f4a0 is the classic wrong x87 port (plain double arithmetic, control word ignored).
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "portsim/arith.h"
#include "portsim/ptr.h"
#include "portsim/registry.h"
#include "portsim/sim/audio.h"
#include "portsim/sim/audio_sound.h"
#include "portsim/sim/soundstate.h"

namespace portsim {

namespace {
using u32 = uint32_t;

#define NEG(v) SG_NEGCTL(g_negctl_##v)
inline uint32_t neg_u32(int mode, uint32_t v) { return mode == 1 ? v ^ 1u : v; }
inline void neg_mem(Ctx &c, int mode, u32 va) {
  if (mode == 1) st<uint32_t>(c, va, ld<uint32_t>(c, va) ^ 1u);
}

// the bits of a local seen as another type: a copy, never a value conversion
template <class T, class S> T pun(const S &s) { static_assert(sizeof(T) == sizeof(S), "pun"); T t; std::memcpy(&t, &s, sizeof t); return t; }

// A local variable the original passes by address (a `push ecx` slot, `&b`, a stack buffer): the port has no stack in the
// context's address space, so the slot is a 16-byte cell in the padding after the image's last section (never read or written by
// the module; U-1460, as U-1440 of audio_seq whose cells are the 0x1c0 bytes at image end - 0x2e0 and U-1450 of audio_chan).
// The cells here are the 0x200 bytes at image end - 0x4e0; cell numbers are unique per function. ALL stack locals of this module
// are behind this one helper (step S4.2b migrates them to the core's frame mechanism by replacing it).

// `strcmp` the original inlines (`mov al, [a]; cmp al, [b]; jne; test al, al; je ...`): 0 when the two NUL-terminated byte strings are equal.
int str_diff(Ctx &c, u32 a, u32 b) {
  for (;; ++a, ++b) {
    const unsigned x = ld<uint8_t>(c, a), y = ld<uint8_t>(c, b);
    if (x != y) return x < y ? -1 : 1;
    if (x == 0) return 0;
  }
}

// ---------------------------------------------------------------------------------------------------------------- x87
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

// ---- Sound::ctor (rva 0x3f30): converted from c3ex.cpp SoundCtor_re
u32 Sound__ctor_body(Ctx &ctx, u32 self) {
    snd_call(ctx, 0x00002513, SndAbi::Thiscall, {self}).eax;  // 0x3f4f
    st<uint32_t>(ctx, self + 0xa4c, 0);  // 0x3f5a
    st<uint32_t>(ctx, self + 0xa50, 0);  // 0x3f60
    st<uint32_t>(ctx, self + 0xa54, 0);  // 0x3f66
    st<uint32_t>(ctx, self + 0xa58, 0);  // 0x3f6c
    st<u32>(ctx, self, snd_va(ctx, 0x5b000));  // 0x3f7f
    for (int k = 0; k < 0x40; ++k) st<uint32_t>(ctx, self + 0x948 + 4 * k, 0);  // 0x3f72-0x3f85
    snd_call(ctx, 0x000018a2, SndAbi::Thiscall, {self, 5}).eax;  // 0x3f90
    st<uint32_t>(ctx, self + 0xa48, 0);  // 0x3f99
    return self;  // 0x3f9f
}
int g_negctl_Sound__ctor = 0;
void a_Sound__ctor(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Sound__ctor), Sound__ctor_body(ctx, a[0]))); }

// ---- Sound::scalarDeletingDtor (rva 0x3fd0): converted from c3dt.cpp SoundDel_re
uint32_t Sound__scalarDeletingDtor_body(Ctx &ctx, u32 self, uint32_t flags) {
    snd_call(ctx, 0x00002446, SndAbi::Thiscall, {self}).eax;
if (flags & 1) snd_delete(ctx, self);
return self;
}
int g_negctl_Sound__scalarDeletingDtor = 0;
void a_Sound__scalarDeletingDtor(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Sound__scalarDeletingDtor), Sound__scalarDeletingDtor_body(ctx, a[0], a[1]))); }

// ---- Sound::dtor (rva 0x4000): converted from c3fd.cpp SoundDtor_re
// Pops the head node of the list {head at hOff, tail at hOff + 4, count at cOff}: its next n[1] becomes the head with its back
// link cleared, or with no next head and tail are cleared; the node goes to operator delete and the count is decremented. Returns
// the node's payload n[2], read before the delete.
u32 c3fd_popNode(Ctx &ctx, u32 self, int32_t hOff, int32_t cOff, u32 n) {
    u32 next = ld<u32>(ctx, n + 4);
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
    return payload;
}
// Pops nodes until the list is empty or a popped node's payload is NULL.
void c3fd_drainList(Ctx &ctx, u32 self, int32_t hOff, int32_t cOff) {
    u32 n = ld<u32>(ctx, self + hOff);
    while (n) {
        if (!c3fd_popNode(ctx, self, hOff, cOff, n)) break;
        n = ld<u32>(ctx, self + hOff);
    }
}
void Sound__dtor_body(Ctx &ctx, u32 self) {
    st<u32>(ctx, self, snd_va(ctx, 0x5b000));  // 0x401f
    c3fd_drainList(ctx, self, 0xa4c, 0xa58);  // 0x4025-0x40b5
    c3fd_drainList(ctx, self, 0xa4c, 0xa58);  // 0x40b7-0x4141
    snd_call(ctx, 0x00001893, SndAbi::Thiscall, {self}).eax;  // 0x414d
}
int g_negctl_Sound__dtor = 0;
void a_Sound__dtor(Ctx &ctx, const uint32_t *a, Ret &) { Sound__dtor_body(ctx, a[0]); neg_mem(ctx, NEG(Sound__dtor), a[0] + 0xa48); }

// ---- Sound::scaleTickRate (rva 0x5d80): converted from c3cx.cpp SoundScaleTick_re
int32_t Sound__scaleTickRate_body(Ctx &ctx, u32 self, uint32_t arg) {
    snd_call(ctx, 0x0001e630, SndAbi::Thiscall, {self + 0x60, arg}).eax;
    return 0;
}
int g_negctl_Sound__scaleTickRate = 0;
void a_Sound__scaleTickRate(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_i32(static_cast<int32_t>(neg_u32(NEG(Sound__scaleTickRate), static_cast<uint32_t>(Sound__scaleTickRate_body(ctx, a[0], a[1]))))); }

// ---- registerSound (rva 0x5e00): converted from c3do.cpp RegisterSound_re
uint32_t registerSound_body(Ctx &ctx, u32 self) {
    snd_call(ctx, 0x00005e30, SndAbi::Thiscall, {snd_obj(ctx, &SoundState::g_soundRegistry).va(), self}).eax;  // 0x5e09 (thunk 0x26c1)
    st<uint32_t>(ctx, self + 0x214, ld<uint32_t>(ctx, self + 0x214) | 2);  // 0x5e0e-0x5e16
    return 0;  // 0x5e1c
}
int g_negctl_registerSound = 0;
void a_registerSound(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(registerSound), registerSound_body(ctx, a[0]))); }

// ---- unregisterSound (rva 0x5eb0): converted from c3do.cpp UnregisterSound_re
// =====================================================================================================================
// L. Removal from the doubly linked value lists (voice list, sound list, module registry) and the registry append
// =====================================================================================================================
// A list header is {head +0, back +4, cursor +8, count +0xc} (at voice+0x1338 / sound owner +0x1c8 / module 0xb5000) and a node {prev +0, next +4, value +8}. Unlinking
// a node (voiceListRemove 0x32470-0x324b7, removeSoundNode 0xe07c-0xe0cb, unregisterSound 0x5eca-0x5f15): when there is a previous node its next
// takes the node's next, otherwise the head takes it; when there is a next node its prev takes the node's prev and the cursor takes the next node, otherwise the cursor is
// cleared and the back pointer takes the node's prev; the node goes to operator delete and the count drops by one.
void listUnlink(Ctx &ctx, u32 hdr, u32 node) {
    u32 prev = ld<u32>(ctx, node);
    if (prev) st<u32>(ctx, prev + 4, ld<u32>(ctx, node + 4));
    else st<u32>(ctx, hdr, ld<u32>(ctx, node + 4));
    u32 next = ld<u32>(ctx, node + 4);
    if (next) {
        st<u32>(ctx, next, ld<u32>(ctx, node));
        st<u32>(ctx, hdr + 8, next);
    } else {
        st<uint32_t>(ctx, hdr + 8, 0);
        st<u32>(ctx, hdr + 4, ld<u32>(ctx, node));
    }
    snd_delete(ctx, node);
    st<int32_t>(ctx, hdr + 0xc, ld<int32_t>(ctx, hdr + 0xc) - 1);
}
uint32_t unregisterSound_body(Ctx &ctx, u32 value) {
    u32 hdr = snd_obj(ctx, &SoundState::g_soundRegistry).va();
    u32 node = ld<u32>(ctx, hdr);  // 0x5eb0
    if (!node) return 0;  // 0x5eb5 / 0x5eb7 -> 0x5f1a
    while (ld<u32>(ctx, node + 8) != value) {  // 0x5eb9 / 0x5ebc
        node = ld<u32>(ctx, node + 4);  // 0x5ebe
        if (!node) return 0;  // 0x5ec1 / 0x5ec3 (eax = 0)
    }
    listUnlink(ctx, hdr, node);  // 0x5eca-0x5f15
    return 0;  // 0x5f1a
}
int g_negctl_unregisterSound = 0;
void a_unregisterSound(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(unregisterSound), unregisterSound_body(ctx, a[0]))); }

// ---- SoundTimer::registerClient (rva 0x88e0): converted from c3dh.cpp RegClient_re
constexpr uint32_t kIatEnterCriticalSection = 0xb83f4;  // c3dh.cpp
constexpr uint32_t kIatLeaveCriticalSection = 0xb84e0;  // c3dh.cpp
// The sound timer's critical section (sound.dll data 0xb4990, 0x18 bytes). The imports are stood in for the key, so the
// address is only an argument that is logged.
u32 c3dh_timerCs(Ctx &ctx) { return snd_arr(ctx, &SoundState::g_timerLock).va(); }
uint32_t SoundTimer__registerClient_body(Ctx &ctx, u32 self, uint32_t client) {
    snd_import(ctx, kIatEnterCriticalSection, SndAbi::Stdcall, {c3dh_timerCs(ctx)}).eax;  // 0x88e8
    snd_call(ctx, 0x00009040, SndAbi::Thiscall, {self + 0x4c, client}).eax;  // 0x88f6 -> 0x9040
    return snd_import(ctx, kIatLeaveCriticalSection, SndAbi::Stdcall, {c3dh_timerCs(ctx)}).eax;  // 0x8900
}
int g_negctl_SoundTimer__registerClient = 0;
void a_SoundTimer__registerClient(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(SoundTimer__registerClient), SoundTimer__registerClient_body(ctx, a[0], a[1]))); }

// ---- SoundTimer::unregisterClient (rva 0x8920): converted from c3dh.cpp UnregClient_re
uint32_t SoundTimer__unregisterClient_body(Ctx &ctx, u32 self, u32 client) {
    snd_import(ctx, kIatEnterCriticalSection, SndAbi::Stdcall, {c3dh_timerCs(ctx)}).eax;  // 0x8929
    u32 node = ld<u32>(ctx, self + 0x4c);  // 0x892f
    while (node && ld<u32>(ctx, node + 8) != client) node = ld<u32>(ctx, node + 4);  // 0x8938-0x8944
    bool found = false;
    if (node) {  // 0x896a
        u32 prev = ld<u32>(ctx, node);  // 0x896e
        if (prev) st<u32>(ctx, prev + 4, ld<u32>(ctx, node + 4));  // 0x8977
        else st<u32>(ctx, self + 0x4c, ld<u32>(ctx, node + 4));  // 0x897f
        u32 next = ld<u32>(ctx, node + 4);  // 0x8982
        if (next) {
            st<u32>(ctx, next, ld<u32>(ctx, node));  // 0x898b
            st<u32>(ctx, self + 0x54, next);  // 0x898d
        } else {
            u32 p = ld<u32>(ctx, node);  // 0x8992
            st<uint32_t>(ctx, self + 0x54, 0);  // 0x8994
            st<u32>(ctx, self + 0x50, p);  // 0x899b
        }
        found = ld<u32>(ctx, node + 8) != 0;  // 0x899f
        snd_delete(ctx, node);  // 0x89a3
        st<uint32_t>(ctx, self + 0x58, static_cast<uint32_t>(ld<uint32_t>(ctx, self + 0x58) - 1));  // 0x89ab
    }
    snd_import(ctx, kIatLeaveCriticalSection, SndAbi::Stdcall, {c3dh_timerCs(ctx)}).eax;  // 0x894d
    snd_vcall(ctx, client, 0xc, SndAbi::Thiscall, {});  // 0x8953-0x8957
    return found ? 0u : 0x14u;  // 0x895d-0x8964
}
int g_negctl_SoundTimer__unregisterClient = 0;
void a_SoundTimer__unregisterClient(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(SoundTimer__unregisterClient), SoundTimer__unregisterClient_body(ctx, a[0], a[1]))); }

// ---- SoundTimer::init (rva 0x89f0): converted from c3ew.cpp StInit_re
constexpr uint32_t kIatInitCS = 0xb83f0;  // c3ew.cpp
constexpr uint32_t kIatTimeGetDevCaps = 0xb8650;  // c3ew.cpp
u32 SoundTimer__init_body(Ctx &ctx, u32 self) {
    u32 s = self;
    st<uint32_t>(ctx, s + 0x4c, 0);  // 0x89ff
    st<uint32_t>(ctx, s + 0x50, 0);  // 0x8a02
    st<uint32_t>(ctx, s + 0x54, 0);  // 0x8a05
    st<uint32_t>(ctx, s + 0x58, 0);  // 0x8a08
    st<uint32_t>(ctx, s + 0x3c, 0);  // 0x8a0c
    snd_import(ctx, kIatTimeGetDevCaps, SndAbi::Stdcall, {s + 0x44, 8}).eax;  // 0x89f7-0x8a0e
    st<uint32_t>(ctx, s + 0x18, ld<uint32_t>(ctx, s + 0x44));  // 0x8a14 / 0x8a1b
    st<uint32_t>(ctx, s + 4, 0);  // 0x8a1e
    st<uint32_t>(ctx, s + 8, 0);  // 0x8a21
    st<uint32_t>(ctx, s + 0xc, 0);  // 0x8a24
    st<uint32_t>(ctx, s + 0x14, 0);  // 0x8a27
    st<uint32_t>(ctx, s + 0x1c, 1);  // 0x8a2a
    st<uint32_t>(ctx, s + 0x24, 0);  // 0x8a2d
    st<uint32_t>(ctx, s + 0x40, 0);  // 0x8a30
    st<uint32_t>(ctx, s + 0x20, 0);  // 0x8a33
    st<uint16_t>(ctx, s + 0x28, 1);  // 0x8a36
    st<uint16_t>(ctx, s + 0x38, 0);  // 0x8a3a
    st<uint32_t>(ctx, s, 0x1e);  // 0x8a4a
    st<uint16_t>(ctx, s + 0x2a, 2);  // 0x8a50
    st<uint32_t>(ctx, s + 0x2c, 0xac44);  // 0x8a56
    st<uint32_t>(ctx, s + 0x30, 0x2b110);  // 0x8a5d
    st<uint16_t>(ctx, s + 0x34, 4);  // 0x8a64
    st<uint16_t>(ctx, s + 0x36, 0x10);  // 0x8a6a
    st<uint32_t>(ctx, s + 0x10, 0x52b);  // 0x8a70
    u32 cs = (snd_arr(ctx, &SoundState::g_timerLock).va());
    for (int i = 0; i < 6; i++) st<uint32_t>(ctx, cs + (i) * 4, 0);  // 0x8a3e-0x8a7c
    snd_import(ctx, kIatInitCS, SndAbi::Stdcall, {cs}).eax;  // 0x8a77 / 0x8a7e
    return self;  // 0x8a84
}
int g_negctl_SoundTimer__init = 0;
void a_SoundTimer__init(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(SoundTimer__init), SoundTimer__init_body(ctx, a[0]))); }

// ---- SoundTimer::stop (rva 0x8ac0): converted from c3ew.cpp StStop_re
constexpr uint32_t kIatDeleteCS = 0xb83e4;  // c3ew.cpp
constexpr uint32_t kIatEnterCS = 0xb83f4;  // c3ew.cpp
constexpr uint32_t kIatLeaveCS = 0xb84e0;  // c3ew.cpp
constexpr uint32_t kIatTimeEndPeriod = 0xb864c;  // c3ew.cpp
constexpr uint32_t kIatTimeKillEvent = 0xb8648;  // c3ew.cpp
// The node list {+0 front, +4 back, +0xc count} of a header at `list` with nodes {+0 previous, +4 next, +8 value}: one pop from
// the front as the bodies below inline it (`mov ecx, [eax+4]; test; je; mov [ecx], 0; mov [front], ecx; jmp` / `mov [back], 0;
// mov [front], 0`, then the value read, the node deleted and the count decremented). Returns the value; *ret is operator
// delete's eax.
u32 c3ew_popFront(Ctx &ctx, u32 list, int32_t frontOff, int32_t backOff, int32_t countOff, u32 node, u32 *ret) {
    u32 next = ld<u32>(ctx, node + 4);
    if (next) {
        st<u32>(ctx, next, 0);
        st<u32>(ctx, list + frontOff, next);
    } else {
        st<u32>(ctx, list + backOff, 0);
        st<u32>(ctx, list + frontOff, 0);
    }
    u32 v = ld<u32>(ctx, node + 8);
    *ret = snd_delete(ctx, node).eax;
    st<int32_t>(ctx, list + countOff, static_cast<int32_t>(ld<int32_t>(ctx, list + countOff) - 1));
    return v;
}
uint32_t SoundTimer__stop_body(Ctx &ctx, u32 self) {
    u32 s = self;
    unsigned w = ld<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_timerBusy).va()) + 1;  // 0x8ac0 / 0x8aca
    st<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_timerBusy).va(), w);  // 0x8acd
    u32 cs = snd_arr(ctx, &SoundState::g_timerLock).va();
    unsigned f = ld<uint32_t>(ctx, s + 0x3c);  // 0x8ad3
    if ((f & 1) && (f & 2)) {  // 0x8ad6-0x8add
        st<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_timerBusy).va(), w + 1);  // 0x8adf-0x8ae7
        st<uint32_t>(ctx, s + 0x3c, static_cast<uint32_t>(ld<uint32_t>(ctx, s + 0x3c) & 0xfffffffbu));  // 0x8aed-0x8af3
        snd_import(ctx, kIatEnterCS, SndAbi::Stdcall, {cs}).eax;  // 0x8af6
        snd_import(ctx, kIatLeaveCS, SndAbi::Stdcall, {cs}).eax;  // 0x8b01
        unsigned id = ld<uint32_t>(ctx, s + 0x40);  // 0x8b07
        st<uint32_t>(ctx, s + 8, 0);  // 0x8b0a
        st<uint32_t>(ctx, s + 4, 0);  // 0x8b0e
        snd_import(ctx, kIatTimeKillEvent, SndAbi::Stdcall, {id}).eax;  // 0x8b11
        snd_import(ctx, kIatTimeEndPeriod, SndAbi::Stdcall, {ld<uint32_t>(ctx, s + 0x18)}).eax;  // 0x8b17-0x8b1b
        st<uint32_t>(ctx, s + 0x3c, static_cast<uint32_t>(ld<uint32_t>(ctx, s + 0x3c) & 0xfffffffeu));  // 0x8b21-0x8b26
    }
    snd_import(ctx, kIatDeleteCS, SndAbi::Stdcall, {cs}).eax;  // 0x8b29 / 0x8b2e
    for (int i = 0; i < 6; i++) st<uint32_t>(ctx, cs + 4 * i, 0);  // 0x8b34-0x8b40
    unsigned dr = 0;
    u32 node = ld<u32>(ctx, s + 0x4c);  // 0x8b42
    while (node) {  // 0x8b45 / 0x8b77
        u32 v = c3ew_popFront(ctx, s, 0x4c, 0x50, 0x58, node, &dr);  // 0x8b49-0x8b6f / 0x8b7b-0x8ba1
        if (!v) return dr;  // 0x8b72 / 0x8ba4
        node = ld<u32>(ctx, s + 0x4c);  // 0x8b74
    }
    return 0;  // eax = the NULL front (0x8b42 / 0x8b74)
}
int g_negctl_SoundTimer__stop = 0;
void a_SoundTimer__stop(Ctx &ctx, const uint32_t *a, Ret &) { SoundTimer__stop_body(ctx, a[0]); neg_mem(ctx, NEG(SoundTimer__stop), a[0] + 0x4c); }

// ---- SoundTimer::stopKeepLock (rva 0x8bf0): converted from c3dh.cpp StopKeep_re
void SoundTimer__stopKeepLock_body(Ctx &ctx, u32 self) {
    st<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_timerBusy).va(), ld<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_timerBusy).va()) + 1);  // 0x8bf4-0x8c02
    snd_import(ctx, kIatEnterCriticalSection, SndAbi::Stdcall, {c3dh_timerCs(ctx)}).eax;  // 0x8c0e
    snd_import(ctx, kIatLeaveCriticalSection, SndAbi::Stdcall, {c3dh_timerCs(ctx)}).eax;  // 0x8c1b
    if ((ld<uint8_t>(ctx, self + 0x3c) & 1) != 0) {  // 0x8c1d / 0x8c22 / 0x8c24
        st<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_timerBusy).va(), ld<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_timerBusy).va()) + 1);  // 0x8c26-0x8c31
        st<uint32_t>(ctx, self + 0x3c, ld<uint32_t>(ctx, self + 0x3c) & 0xfffffffbu);  // 0x8c36-0x8c3c
        snd_import(ctx, kIatEnterCriticalSection, SndAbi::Stdcall, {c3dh_timerCs(ctx)}).eax;  // 0x8c3f
        snd_import(ctx, kIatLeaveCriticalSection, SndAbi::Stdcall, {c3dh_timerCs(ctx)}).eax;  // 0x8c46
        st<uint32_t>(ctx, self + 8, 0);  // 0x8c4b
        st<uint32_t>(ctx, self + 4, 0);  // 0x8c4f
        snd_import(ctx, kIatTimeKillEvent, SndAbi::Stdcall, {ld<uint32_t>(ctx, self + 0x40)}).eax;  // 0x8c52
        snd_import(ctx, kIatTimeEndPeriod, SndAbi::Stdcall, {ld<uint32_t>(ctx, self + 0x18)}).eax;  // 0x8c5c
        unsigned f = ld<uint32_t>(ctx, self + 0x3c);  // 0x8c62
        f = (f & ~0xffu) | static_cast<uint8_t>(f & 0xfe);  // 0x8c65 (and al, 0xfe)
        st<uint32_t>(ctx, self + 0x3c, f);  // 0x8c67
    }
    for (;;) {
        u32 node = ld<u32>(ctx, self + 0x4c);  // 0x8c6a
        if (node == 0) break;  // 0x8c6d / 0x8c6f
        u32 next = ld<u32>(ctx, node + 4);  // 0x8c71
        if (next != 0) {  // 0x8c76
            st<uint32_t>(ctx, next, 0);  // 0x8c78
            st<u32>(ctx, self + 0x4c, next);  // 0x8c7a
        } else {
            st<uint32_t>(ctx, self + 0x50, 0);  // 0x8c7f
            st<uint32_t>(ctx, self + 0x4c, 0);  // 0x8c82
        }
        u32 client = ld<u32>(ctx, node + 8);  // 0x8c85
        snd_delete(ctx, node);  // 0x8c89
        st<uint32_t>(ctx, self + 0x58, ld<uint32_t>(ctx, self + 0x58) - 1);  // 0x8c8e-0x8c97
        if (client == 0) break;  // 0x8c95 / 0x8c97
        snd_vcall(ctx, client, 0xc, SndAbi::Thiscall, {});  // 0x8ca0
    }
}
int g_negctl_SoundTimer__stopKeepLock = 0;
void a_SoundTimer__stopKeepLock(Ctx &ctx, const uint32_t *a, Ret &) { SoundTimer__stopKeepLock_body(ctx, a[0]); neg_mem(ctx, NEG(SoundTimer__stopKeepLock), a[0] + 0x4c); }

// ---- SoundTimer::killEvent (rva 0x8d70): converted from c3dh.cpp KillEvent_re
uint32_t SoundTimer__killEvent_body(Ctx &ctx, u32 self) {
    if ((ld<uint8_t>(ctx, self + 0x3c) & 1) == 0) return 0x15;  // 0x8d73 / 0x8d77 / 0x8d79
    st<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_timerBusy).va(), ld<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_timerBusy).va()) + 1);  // 0x8d80 / 0x8d8b / 0x8d8c
    st<uint32_t>(ctx, self + 0x3c, ld<uint32_t>(ctx, self + 0x3c) & 0xfffffffbu);  // 0x8d92-0x8d98
    snd_import(ctx, kIatEnterCriticalSection, SndAbi::Stdcall, {c3dh_timerCs(ctx)}).eax;  // 0x8d9b
    snd_import(ctx, kIatLeaveCriticalSection, SndAbi::Stdcall, {c3dh_timerCs(ctx)}).eax;  // 0x8da6
    st<uint32_t>(ctx, self + 8, 0);  // 0x8dae
    st<uint32_t>(ctx, self + 4, 0);  // 0x8db1
    snd_import(ctx, kIatTimeKillEvent, SndAbi::Stdcall, {ld<uint32_t>(ctx, self + 0x40)}).eax;  // 0x8db8
    snd_import(ctx, kIatTimeEndPeriod, SndAbi::Stdcall, {ld<uint32_t>(ctx, self + 0x18)}).eax;  // 0x8dc2
    unsigned f = ld<uint32_t>(ctx, self + 0x3c);  // 0x8dc8
    f = (f & ~0xffu) | static_cast<uint8_t>(f & 0xfe);  // 0x8dcb (and al, 0xfe)
    st<uint32_t>(ctx, self + 0x3c, f);  // 0x8dcd
    return 0;  // 0x8dd0
}
int g_negctl_SoundTimer__killEvent = 0;
void a_SoundTimer__killEvent(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(SoundTimer__killEvent), SoundTimer__killEvent_body(ctx, a[0]))); }

// ---- SoundTimer::onTick (rva 0x8ed0): converted from c3fl.cpp OnTick_re
constexpr uint32_t kIatOutputDebugStringA = 0xb83e8;  // c3fl.cpp
constexpr uint32_t kIatTimeGetTime = 0xb8654;  // c3fl.cpp
uint32_t SoundTimer__onTick_body(Ctx &ctx, uint32_t, uint32_t, u32 timer, uint32_t, uint32_t) {
  SndFrame fr_cells(ctx, 0x100);  // S4.2b: the cells [0] of the original's locals passed by address (U-1418)
    unsigned w = ld<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_timerBusy).va());  // 0x8ed0
    if (w != 0) return w;  // 0x8edb / 0x8edf
    u32 t = timer;  // 0x8ee5
    if (t && !(ld<uint8_t>(ctx, t + 0x3c) & 4)) return 0;  // 0x8eec-0x8ef4 (eax = the word, 0)
    u32 cs = snd_arr(ctx, &SoundState::g_timerLock).va();
    st<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_timerBusy).va(), 1);  // 0x8eff
    snd_import(ctx, kIatEnterCS, SndAbi::Stdcall, {cs}).eax;  // 0x8efa / 0x8f09
    unsigned nw;
    if (t && !(ld<uint8_t>(ctx, t + 0x3c) & 4)) {  // 0x8f0f-0x8f17
        nw = ld<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_timerBusy).va()) + 1;  // 0x8f19 / 0x8f1f
    } else {
        unsigned now = snd_import(ctx, kIatTimeGetTime, SndAbi::Stdcall, {}).eax;  // 0x8f25 / 0x8f2b
        unsigned last = ld<uint32_t>(ctx, t + 4);  // 0x8f2d
        bool late = true;
        if (last == 0) {
            st<uint32_t>(ctx, t + 8, 0);  // 0x8f54
        } else {
            st<uint32_t>(ctx, t + 8, (now > last) ? now - last : last - now);  // 0x8f34-0x8f43
            if (ld<uint32_t>(ctx, t + 8) < ld<uint32_t>(ctx, t) - 7) late = false;  // 0x8f46-0x8f52
        }
        if (late) {
            if (ld<uint32_t>(ctx, t + 8) > ld<uint32_t>(ctx, t) + 0x14) {  // 0x8f5b-0x8f65
                const u32 msg = fr_cells.at(0x100);  // the 0x100-byte stack buffer (cells 0..15)
                snd_call(ctx, 0x00042577, SndAbi::Cdecl, {msg, snd_obj(ctx, &SoundState::g_fmtTimerLate).va(), ld<uint32_t>(ctx, t + 8)}).eax;  // 0x8f67-0x8f77
                snd_import(ctx, kIatOutputDebugStringA, SndAbi::Stdcall, {msg}).eax;  // 0x8f7a-0x8f7f
            }
            st<uint32_t>(ctx, t + 4, now);  // 0x8f85
            u32 node = ld<u32>(ctx, t + 0x4c);  // 0x8f88
            st<u32>(ctx, t + 0x54, node);  // 0x8f8d
            u32 c = node ? ld<u32>(ctx, node + 8) : 0;  // 0x8f90-0x8f97
            while (c) {
                snd_vcall(ctx, c, 0x44, SndAbi::Thiscall, {ld<uint32_t>(ctx, t + 8)}).eax;  // 0x8f99-0x8fa1
                snd_vcall(ctx, c, 0x38, SndAbi::Thiscall, {}).eax;  // 0x8fa4-0x8fa8
                node = ld<u32>(ctx, t + 0x54);  // 0x8fab
                if (!node) break;  // 0x8fae / 0x8fb0
                node = ld<u32>(ctx, node + 4);  // 0x8fb2
                st<u32>(ctx, t + 0x54, node);  // 0x8fb7
                if (!node) break;  // 0x8fb5 / 0x8fba
                c = ld<u32>(ctx, node + 8);  // 0x8fbc-0x8fc1
            }
        }
        nw = ld<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_timerBusy).va()) - 1;  // 0x8fc3 / 0x8fc9
    }
    st<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_timerBusy).va(), nw);  // 0x8fcf
    return snd_import(ctx, kIatLeaveCS, SndAbi::Stdcall, {cs}).eax;  // 0x8fca / 0x8fd5
}
int g_negctl_SoundTimer__onTick = 0;
void a_SoundTimer__onTick(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(SoundTimer__onTick), SoundTimer__onTick_body(ctx, a[0], a[1], a[2], a[3], a[4]))); }

// ---- SoundTimer::linkClient (rva 0x9040): converted from c3dh.cpp LinkClient_re
uint32_t SoundTimer__linkClient_body(Ctx &ctx, u32 self, uint32_t client) {
    u32 node = (snd_new(ctx, 0xc));  // 0x9045
    if (node) {  // 0x904d
        st<uint32_t>(ctx, node, 0);  // 0x9055
        st<uint32_t>(ctx, node + 4, 0);  // 0x905b
        st<uint32_t>(ctx, node + 8, client);  // 0x9062
    }
    u32 front = ld<u32>(ctx, self);  // 0x9069
    if (front) {  // 0x906b
        st<u32>(ctx, front, node);  // 0x906f
        st<u32>(ctx, node + 4, ld<u32>(ctx, self));  // 0x9071 / 0x9073
        st<u32>(ctx, self, node);  // 0x9076
    } else {
        st<u32>(ctx, self, node);  // 0x9083
        st<u32>(ctx, self + 4, node);  // 0x9085
    }
    unsigned n = ld<uint32_t>(ctx, self + 0xc) + 1;  // 0x9078 / 0x9088
    st<uint32_t>(ctx, self + 0xc, n);  // 0x907c / 0x908c
    return n;
}
int g_negctl_SoundTimer__linkClient = 0;
void a_SoundTimer__linkClient(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(SoundTimer__linkClient), SoundTimer__linkClient_body(ctx, a[0], a[1]))); }

// ---- create_sound (rva 0x93e0): converted from c3fq.cpp CreateSound_re
u32 c3fq_str(Ctx &ctx, uint32_t rva) { return snd_va(ctx, rva); }
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
            if (o) snd_call(ctx, 0x00002513, SndAbi::Thiscall, {o});  // 0x9510-0x9512 Seq::ctor_1001f4a0
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
        if (o) snd_call(ctx, 0x000018c5, SndAbi::Thiscall, {o});  // 0x9604 CVoiceRxMgr::ctor
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
        if (o) snd_call(ctx, 0x0000237e, SndAbi::Thiscall, {o});  // 0x9694 dllSoundList::ctor
        st<u32>(ctx, out, o);  // 0x969d
        return 0;  // 0x969f
    }
    }
    return 0xb;  // unreachable (idx 0..7 all handled)
}
int g_negctl_create_sound = 0;
void a_create_sound(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(create_sound), create_sound_body(ctx, a[0], a[1], a[2]))); }

// ---- Sound_initEntry (rva 0x1ab60): converted from c3ar.cpp SoundInitEntry_re
u32 Sound_initEntry_body(Ctx &ctx, u32 self) {
    for (u32 p = self + 4, n = 0x80; n != 0; --n, p += 4) st<uint32_t>(ctx, p, 0);
    st<uint32_t>(ctx, self, 0);
    st<uint32_t>(ctx, self + 0x214, 0);
    st<uint32_t>(ctx, self + 0x204, 0);
    st<uint32_t>(ctx, self + 0x208, 0);
    st<uint32_t>(ctx, self + 0x20c, 0);
    st<uint32_t>(ctx, self + 0x210, 0);
    st<uint32_t>(ctx, self + 0x218, 0xfa);
    return self;
}
int g_negctl_Sound_initEntry = 0;
void a_Sound_initEntry(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Sound_initEntry), Sound_initEntry_body(ctx, a[0]))); }

// ---- Sound_anyChannelActive (rva 0x1ad30): converted from c3dj.cpp SoundAnyActive_re
uint8_t Sound_anyChannelActive_body(Ctx &ctx, u32 self) {
    for (int ebx = 0; ebx < 0x80; ebx++) {  // 0x1ad36 / 0x1ad88 / 0x1ad89 / 0x1ad8f
        u32 obj = ld<u32>(ctx, self + 4 + 4 * ebx);  // 0x1ad38
        if (obj == 0) continue;  // 0x1ad3c / 0x1ad3e
        int n = ld<int32_t>(ctx, obj + 0x1438);  // 0x1ad40
        if (n != 0) {  // 0x1ad46 / 0x1ad48
            int esi = 0;
            int bound = n - 1;  // 0x1ad4c
            if (bound > 0) {  // 0x1ad4d / 0x1ad4f
                int edi = 0x1440;
                for (;;) {
    u32 elem = ld<u32>(ctx, obj + edi);  // 0x1ad56
                    if (elem != 0) {  // 0x1ad59 / 0x1ad5b
                        unsigned r = snd_vcall(ctx, elem, 0x5c, SndAbi::Thiscall, {}).eax;  // 0x1ad61
                        if (r != 0) return 1;  // 0x1ad64 / 0x1ad66
                    }
                    obj = ld<u32>(ctx, self + 4 + 4 * ebx);  // 0x1ad68
                    esi += 1;  // 0x1ad6c
                    edi += 4;  // 0x1ad6d
                    bound = ld<int32_t>(ctx, obj + 0x1438) - 1;  // 0x1ad70 / 0x1ad76
                    if (!(esi < bound)) break;  // 0x1ad77 / 0x1ad79
                }
            }
        }
        obj = ld<u32>(ctx, self + 4 + 4 * ebx);  // 0x1ad7b
        unsigned r2 = snd_vcall(ctx, obj, 0x5c, SndAbi::Thiscall, {}).eax;  // 0x1ad81
        if (r2 != 0) return 1;  // 0x1ad84 / 0x1ad86
    }
    return 0;  // 0x1ad94
}
int g_negctl_Sound_anyChannelActive = 0;
void a_Sound_anyChannelActive(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Sound_anyChannelActive), Sound_anyChannelActive_body(ctx, a[0]))); }

// ---- Sound_setName (rva 0x1ced0): converted from c3dh.cpp SetName_re
void Sound_setName_body(Ctx &ctx, u32 self, u32 name) {
  SndFrame fr_cells(ctx, 0x100);  // S4.2b: the cells [0] of the original's locals passed by address (U-1418)
    const u32 buf = fr_cells.at(0x100);  // the 0x100-byte stack buffer (cells 0..15)
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
}
int g_negctl_Sound_setName = 0;
void a_Sound_setName(Ctx &ctx, const uint32_t *a, Ret &) { Sound_setName_body(ctx, a[0], a[1]); neg_mem(ctx, NEG(Sound_setName), a[0] + 0x1c); }

// ---- Sound_setAltName (rva 0x1d0a0): converted from c3dj.cpp SoundSetAltName_re
// The name setters: a string argument is copied into a fresh operator-new block (strlen + 1 bytes, `repne scasb` / `not ecx` e.g.
// 0x2bb2d-0x2bb36, copied with `rep movsd` + `rep movsb`) whose pointer replaces a field after the old block was released.
uint32_t c3dj_dupInto(Ctx &ctx, u32 field, u32 name) {
    unsigned len = 0;
    while (ld<int8_t>(ctx, name + len) != 0) len++;
    len += 1;  // strlen + 1 (ecx after `not ecx`)
    u32 p = (snd_new(ctx, len));  // operator new 0x424d7
    st<u32>(ctx, field, p);
    for (unsigned i = 0; i < len; i++) st<int8_t>(ctx, p + i, ld<int8_t>(ctx, name + i));  // rep movsd + rep movsb
    return len;
}
uint32_t Sound_setAltName_body(Ctx &ctx, u32 self, u32 name) {
    u32 old = ld<u32>(ctx, self + 0x24);  // 0x1d0a4
    if (old != 0) snd_delete(ctx, old);  // 0x1d0a9 / 0x1d0ac
    if (name == 0) {  // 0x1d0b8 / 0x1d0ba
        st<uint32_t>(ctx, self + 0x24, 0);  // 0x1d0fb
        return 0;  // eax is not defined here: the NULL-name key compares memory only
    }
    return c3dj_dupInto(ctx, self + 0x24, name);  // 0x1d0c9-0x1d0f3
}
int g_negctl_Sound_setAltName = 0;
void a_Sound_setAltName(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Sound_setAltName), Sound_setAltName_body(ctx, a[0], a[1]))); }

// ---- Seq::ctor_1001f4a0 (rva 0x1f4a0): converted from c3fh.cpp Snd5b6a8Ctor_re
// the tempo seed of Seq_setTempo with tempo 0x78 on the fresh fields: x87 arithmetic under the caller's control word (port/CORE.md section 7)
uint32_t Seq_ctor_1001f4a0_body(Ctx &ctx, u32 self) {
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
    x_ldU32(ctx, T, ld<uint32_t>(ctx, self + 0x1bc));  // 0x1f5c3-0x1f5d1
    unsigned old = ld<uint32_t>(ctx, self + 0x718);  // 0x1f5d5
    st<uint32_t>(ctx, self + 0x718, 0x78);  // 0x1f5e3
    x_ld32(ctx, k, ld<uint32_t>(ctx, snd_va(ctx, 0x0005b6a4)));
    x_mul(ctx, p, T, k);  // 0x1f5ed
    x_ld32(ctx, k, ld<uint32_t>(ctx, snd_va(ctx, 0x0005b6a0)));
    x_mul(ctx, p, p, k);  // 0x1f5f3
    x_ld32(ctx, k, ld<uint32_t>(ctx, snd_va(ctx, 0x0005b69c)));
    x_div(ctx, p, k, p);  // 0x1f5f9 fdivr dword
    x_st(ctx, self + 0x728, p);  // 0x1f5ff
    x_ldU32(ctx, q, old);  // 0x1f5db / 0x1f5df / 0x1f605 fild qword (old, 0)
    x_ld32(ctx, k, ld<uint32_t>(ctx, snd_va(ctx, 0x0005b698)));
    x_mul(ctx, q, q, k);  // 0x1f609
    x_ld(ctx, r, self + 0x720);  // 0x1f60f
    x_mul(ctx, r, r, q);  // 0x1f615 fmul st(1)
    unsigned w = static_cast<uint32_t>(x_ftol(ctx, r));  // 0x1f617; 0x1f624 drops q
    st<uint32_t>(ctx, self + 0x740, 0);  // 0x1f62a
    x_ldU32(ctx, r, w);  // 0x1f620-0x1f630
    x_st(ctx, self + 0x720, r);  // 0x1f637
    return self;
}
int g_negctl_Seq_ctor_1001f4a0 = 0;
void a_Seq_ctor_1001f4a0(Ctx &ctx, const uint32_t *a, Ret &r) {
  PlainDoubles plain(NEG(Seq_ctor_1001f4a0) == 2);
  r.set_u32(neg_u32(NEG(Seq_ctor_1001f4a0), Seq_ctor_1001f4a0_body(ctx, a[0]))); }

// ---- Sound5b6a8_scalarDtor (rva 0x1f6c0): converted from c3dt.cpp S5b6a8Del_re
uint32_t Sound5b6a8_scalarDtor_body(Ctx &ctx, u32 self, uint32_t flags) {
    snd_call(ctx, 0x00001893, SndAbi::Thiscall, {self}).eax;
if (flags & 1) snd_delete(ctx, self);
return self;
}
int g_negctl_Sound5b6a8_scalarDtor = 0;
void a_Sound5b6a8_scalarDtor(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Sound5b6a8_scalarDtor), Sound5b6a8_scalarDtor_body(ctx, a[0], a[1]))); }

// ---- Seq::dtor (rva 0x1ff50): converted from c3fk.cpp S5b6a8Dtor_re
// Node {+0 previous, +4 next, +8 payload}; a list header {head, tail, cursor, count} at a fixed offset. Pop the head node:
// the successor's +0 = 0 and it becomes the head; with no successor tail and head become 0. Returns the node (not freed).
u32 c3fk_popHead(Ctx &ctx, u32 head, u32 tail) {
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
// 0x0001ff50  Seq::dtor (VA 0x1ff50). __thiscall, no stack argument, plain `ret` (0x1ffcf). vtable 0x5b6a8 at +0
// (0x1ff72). Bit 0 of +0xac (0x1ff86) runs Midi_Device::closeFile on this+0x60 through the thunk 0x1014 (0x1ff8d).
// The 0x7f channel records at +0x744 .. +0x93c (counter 0x7f 0x1ff98, `jne` 0x1ffcb): each non-NULL one gets +0x30 = 0,
// ChanRecord_freeBuffers through 0x2004, operator delete, and its slot 0 (0x1ffa5-0x1ffbc). The list {+0x218 head,
// +0x21c tail, +0x224 count} is popped (0x1ffcd-0x1fff2) node by node: the node is freed (0x1fffc), the count
// decremented (0x2000a), and a NULL payload ends the loop (0x20015), else the payload is freed too (0x20018). The eh
// vector destructor ??_M 0x4280e (__stdcall, four arguments) destroys the 0x10 elements of 0x4c bytes at +0x250 with the
// destructor thunk 0x166d (0x20066-0x2007b). The list {+0x228, +0x22c, +0x234 count} is popped the same way freeing
// only the nodes (0x20080-0x2010b), then the first list again, nodes only (0x2010d-0x20198). Seq_dtor runs on
// this+0x60 through 0x14d8 (0x201a2-0x201a9; this is never NULL here) and Midi::dtor on this through 0x1cc1
// (0x201b8). Callers discard eax (Sound5b6a8_scalarDtor 0x1f6c0, Sound::dtor 0x4000): void.
void c3fk_drainNodes(Ctx &ctx, u32 self, int32_t head, int32_t tail, int32_t count, uint8_t freePayload) {
    while (ld<u32>(ctx, self + head)) {
        u32 n = c3fk_popHead(ctx, (self + head), (self + tail));
        u32 pl = ld<u32>(ctx, n + 8);
        snd_delete(ctx, n);
        st<int32_t>(ctx, self + count, static_cast<int32_t>(ld<int32_t>(ctx, self + count) - 1));
        if (!pl) break;
        if (freePayload) snd_delete(ctx, pl);
    }
}
void Seq_dtor_body(Ctx &ctx, u32 self) {
    st<u32>(ctx, self, snd_va(ctx, snd_rva::vt_Sound5b6a8));  // 0x1ff72
    if (ld<uint8_t>(ctx, self + 0xac) & 1)  // 0x1ff78 / 0x1ff86
        snd_call(ctx, 0x00001014, SndAbi::Thiscall, {(self + 0x60)}).eax;  // 0x1ff8d
    for (int k = 0; k < 0x7f; ++k) {  // 0x1ff92-0x1ffcb
        const u32 slot = self + 0x744 + 4 * k;
        if (u32 r = ld<u32>(ctx, slot)) {
            st<uint32_t>(ctx, r + 0x30, 0);  // 0x1ffab
            snd_call(ctx, 0x00002004, SndAbi::Thiscall, {r}).eax;  // 0x1ffae
            snd_delete(ctx, r);  // 0x1ffb4
            st<u32>(ctx, slot, 0);  // 0x1ffbc
        }
    }
    c3fk_drainNodes(ctx, self, 0x218, 0x21c, 0x224, true);  // 0x1ffcd-0x20064
    snd_call(ctx, 0x0004280e, SndAbi::Stdcall, {(self + 0x250), 0x4c, 0x10, snd_va(ctx, 0x166d)});  // 0x20066-0x2007b
    c3fk_drainNodes(ctx, self, 0x228, 0x22c, 0x234, false);  // 0x20080-0x2010b
    c3fk_drainNodes(ctx, self, 0x218, 0x21c, 0x224, false);  // 0x2010d-0x20198
    snd_call(ctx, 0x000014d8, SndAbi::Thiscall, {(self + 0x60)}).eax;  // 0x201a2-0x201a9
    snd_call(ctx, 0x00001cc1, SndAbi::Thiscall, {self}).eax;  // 0x201b8
}
int g_negctl_Seq_dtor = 0;
void a_Seq_dtor(Ctx &ctx, const uint32_t *a, Ret &) { Seq_dtor_body(ctx, a[0]); neg_mem(ctx, NEG(Seq_dtor), a[0] + 0x214); }

// ---- Sound_reapIdle (rva 0x204e0): converted from c3dt.cpp Reap_re
uint32_t Sound_reapIdle_body(Ctx &ctx) {
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
}
int g_negctl_Sound_reapIdle = 0;
void a_Sound_reapIdle(Ctx &ctx, const uint32_t *, Ret &r) { r.set_u32(neg_u32(NEG(Sound_reapIdle), Sound_reapIdle_body(ctx))); }

// ---- Sound_findByName (rva 0x21240): converted from c3dh.cpp FindByName_re
uint8_t Sound_findByName_body(Ctx &ctx, u32 name) {
    u32 node = ld<u32>(ctx, snd_obj(ctx, &SoundState::g_cmdTableHead).va());  // 0x21240
    st<u32>(ctx, snd_obj(ctx, &SoundState::g_cmdTableCursor).va(), node);  // 0x2124d
    while (node != 0) {  // 0x21253
        u32 sound = ld<u32>(ctx, node + 8);  // 0x21255
        if (sound == 0) break;  // 0x2125a
        u32 a = ld<u32>(ctx, sound);  // 0x21260
        u32 b = name;  // 0x21262
        bool equal;
        for (;;) {
            unsigned char x = static_cast<uint8_t>(ld<int8_t>(ctx, a + 0));  // 0x21264
            unsigned char y = static_cast<uint8_t>(ld<int8_t>(ctx, b + 0));  // 0x21266
            if (x != y) { equal = false; break; }  // 0x2126a / 0x2126c
            if (x == 0) { equal = true; break; }  // 0x2126e / 0x21270
            x = static_cast<uint8_t>(ld<int8_t>(ctx, a + 1));  // 0x21272
            y = static_cast<uint8_t>(ld<int8_t>(ctx, b + 1));  // 0x21275
            if (x != y) { equal = false; break; }  // 0x2127a / 0x2127c
            a += 2; b += 2;  // 0x2127e / 0x21281
            if (x == 0) { equal = true; break; }  // 0x21284 / 0x21286
        }
        if (equal) return 1;  // 0x21293 / 0x212b5
        node = ld<u32>(ctx, node + 4);  // 0x21295
        st<u32>(ctx, snd_obj(ctx, &SoundState::g_cmdTableCursor).va(), node);  // 0x2129a
    }
    return 0;  // 0x212ac
}
int g_negctl_Sound_findByName = 0;
void a_Sound_findByName(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Sound_findByName), Sound_findByName_body(ctx, a[0]))); }

// ---- Sound_lookupOrCreate (rva 0x22320): converted from c3fk.cpp Lookup_re
constexpr uint32_t kDescCursor = 0xb4ff0;  // c3fk.cpp
constexpr uint32_t kDescHead = 0xb4fe8;  // c3fk.cpp
constexpr uint32_t kDescTail = 0xb4fec;  // c3fk.cpp
u32 c3fk_gref(Ctx &ctx, uint32_t rva) { return ld<u32>(ctx, snd_va(ctx, rva)); }
uint32_t Sound_lookupOrCreate_body(Ctx &ctx, u32 self, u32 desc) {
    if (!desc) return 0xa;  // 0x2233d-0x22359
    u32 cur = c3fk_gref(ctx, kDescHead);  // 0x2235c
    st<u32>(ctx, snd_va(ctx, kDescCursor), cur);  // 0x22366
    u32 obj = 0;
    u32 p = cur ? ld<u32>(ctx, cur + 8) : 0;  // 0x2236b-0x22372
    while (p) {
        u32 nm = (snd_call(ctx, 0x000025f4, SndAbi::Thiscall, {desc}).eax);  // 0x22379
        if (str_diff(ctx, ld<u32>(ctx, p), nm) == 0) { obj = p; break; }  // 0x22374-0x223ad, 0x223d1
        u32 c = c3fk_gref(ctx, kDescCursor);  // 0x223af
        if (!c) break;  // 0x223b6
        c = ld<u32>(ctx, c + 4);  // 0x223b8
        st<u32>(ctx, snd_va(ctx, kDescCursor), c);  // 0x223bd
        if (!c) break;  // 0x223c2
        p = ld<u32>(ctx, c + 8);  // 0x223c4-0x223c9
    }
    if (!obj) {
        u32 e = snd_new(ctx, 0x21c);  // 0x223df
        u32 ent = e ? (snd_call(ctx, 0x00001929, SndAbi::Thiscall, {e}).eax) : 0;  // 0x223eb-0x22402
        obj = ent;  // 0x2240e
        snd_call(ctx, 0x00001334, SndAbi::Thiscall, {ent, snd_call(ctx, 0x000025f4, SndAbi::Thiscall, {desc}).eax}).eax;  // 0x22412-0x2241a
        for (int i = 0; i < 0x80;) {  // 0x2241f / 0x22439 / 0x2243f
            u32 s = ld<u32>(ctx, desc + 4 + 4 * i);  // 0x22421
            if (s) {
                int first = ld<int32_t>(ctx, s);  // 0x22429
                snd_call(ctx, 0x00001fa0, SndAbi::Thiscall, {ent, s}).eax;  // 0x2242e
                i = first + 1;  // 0x22433
            } else {
                ++i;  // 0x22438
            }
        }
        snd_call(ctx, 0x00002455, SndAbi::Thiscall, {ent, ld<uint32_t>(ctx, desc + 0x218)}).eax;  // 0x22441-0x2244a
        if (ld<uint8_t>(ctx, desc + 0x208) & 1)  // 0x2244f / 0x22456
            snd_call(ctx, 0x00001f32, SndAbi::Thiscall, {ent, ld<uint32_t>(ctx, desc + 0x210), ld<uint32_t>(ctx, desc + 0x20c)}).eax;  // 0x22458-0x22468
        if (ld<uint8_t>(ctx, desc + 0x208) & 8) st<uint32_t>(ctx, ent + 0x214, static_cast<uint32_t>(ld<uint32_t>(ctx, ent + 0x214) | 8));  // 0x2246d-0x2247c
        u32 n = snd_new(ctx, 0xc);  // 0x22484
        if (n) {  // 0x2248c / 0x2248e
            st<uint32_t>(ctx, n, 0);
            st<uint32_t>(ctx, n + 4, 0);
            st<u32>(ctx, n + 8, ent);  // 0x22490-0x2249d
        }
        if (u32 t = c3fk_gref(ctx, kDescTail)) {  // 0x224a4-0x224ac
            st<u32>(ctx, t + 4, n);  // 0x224ae
            st<u32>(ctx, n, c3fk_gref(ctx, kDescTail));  // 0x224b1 / 0x224b7
        } else {
            st<u32>(ctx, snd_va(ctx, kDescHead), n);  // 0x224bb
        }
        st<u32>(ctx, snd_va(ctx, kDescTail), n);  // 0x224c0
        st<int32_t>(ctx, snd_va(ctx, 0xb4ff4), static_cast<int32_t>(ld<int32_t>(ctx, snd_va(ctx, 0xb4ff4)) + 1));  // 0x224c5-0x224cb
    }
    for (int k = 0; k < 0x80; ++k) {  // 0x224d4 / 0x22644
        u32 rec = ld<u32>(ctx, self + 0x744 + 4 * k);  // 0x224e6-0x224ea
        if (!rec) continue;  // 0x224f2
        u32 alt = ld<u32>(ctx, rec + 0x24);  // 0x224f8
        if (!alt) continue;  // 0x224fd
        u32 on = ld<u32>(ctx, obj);  // 0x22503 / 0x22507
        if (str_diff(ctx, on, alt) != 0) continue;  // 0x22509-0x22536
        u32 old = ld<u32>(ctx, rec + 0x30);  // 0x2253c
        if (!old) {  // 0x22543
            st<u32>(ctx, rec + 0x30, obj);  // 0x22627
            st<uint32_t>(ctx, obj + 0x210, static_cast<uint32_t>(ld<uint32_t>(ctx, obj + 0x210) + 1));  // 0x2262a
            continue;
        }
        if (str_diff(ctx, ld<u32>(ctx, old), on) == 0) continue;  // 0x22549-0x22578, 0x2261b-0x22621
        st<uint32_t>(ctx, old + 0x210, ld<uint32_t>(ctx, old + 0x210) - 1u);
        if (ld<uint32_t>(ctx, old + 0x210) == 0) {  // 0x2257e-0x2258f
            u32 n = c3fk_gref(ctx, kDescHead);  // 0x22595
            while (n && ld<u32>(ctx, n + 8) != ld<u32>(ctx, rec + 0x30)) n = ld<u32>(ctx, n + 4);  // 0x2259d-0x225ad
            if (n) {  // 0x225af / 0x225b1
                u32 prev = ld<u32>(ctx, n);  // 0x225b3
                if (prev) st<u32>(ctx, prev + 4, ld<u32>(ctx, n + 4));  // 0x225b9 / 0x225bc
                else st<u32>(ctx, snd_va(ctx, kDescHead), ld<u32>(ctx, n + 4));  // 0x225c1 / 0x225c4
                u32 next = ld<u32>(ctx, n + 4);  // 0x225ca
                if (next) {  // 0x225cd / 0x225cf
                    st<u32>(ctx, next, ld<u32>(ctx, n));  // 0x225d1 / 0x225d3
                    st<u32>(ctx, snd_va(ctx, kDescCursor), next);  // 0x225d5
                } else {
    st<u32>(ctx, snd_va(ctx, kDescCursor), 0);  // 0x225df
                    st<u32>(ctx, snd_va(ctx, kDescTail), ld<u32>(ctx, n));  // 0x225dd / 0x225e5
                }
                snd_delete(ctx, n);  // 0x225ec
                st<int32_t>(ctx, snd_va(ctx, 0xb4ff4), static_cast<int32_t>(ld<int32_t>(ctx, snd_va(ctx, 0xb4ff4)) - 1));  // 0x225f1-0x225fa
            }
            if (u32 o = ld<u32>(ctx, rec + 0x30)) {  // 0x225ff-0x22604
                snd_call(ctx, 0x00001d66, SndAbi::Thiscall, {o}).eax;  // 0x22608
                snd_delete(ctx, o);  // 0x2260e
            }
        }
        st<u32>(ctx, rec + 0x30, 0);  // 0x22616
    }
    return 0;  // 0x22651
}
int g_negctl_Sound_lookupOrCreate = 0;
void a_Sound_lookupOrCreate(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Sound_lookupOrCreate), Sound_lookupOrCreate_body(ctx, a[0], a[1]))); }

// ---- SoundWindowObj_dtor (rva 0x286a0): converted from c3dt.cpp SwoDtor_re
constexpr uint32_t kIatSetWindowLongA = 0xb85a4;  // c3dt.cpp
// 0x000286a0  SoundWindowObj_dtor (VA 0x286a0). __thiscall, `ret`. eax = the window handle at this+8 (0x286a3); the vtable pointer
// 0x5b8ac is stored at this+0 (0x286a6). With a handle (0x286ac) and bit 2 of the byte at this+4 set (0x286b0 / 0x286b4):
// SetWindowLongA [0xb85a4] (handle, -4 (GWL_WNDPROC), the dword at this+0xc) (0x286bd), the dword +0xc is cleared (0x286c6), the byte
// at +4 is read into al (0x286c3), masked with 0xfb (0x286cd), the dword +8 is cleared (0x286cf) and the byte stored back
// (0x286d6). The value left in eax is the handle when the branch is skipped (0x286d9) and, on the taken path, SetWindowLongA's result
// with its low byte replaced by the new flag byte (`mov al, [esi+4]` 0x286c3).
uint32_t c3dt_swoBody(Ctx &ctx, u32 self) {
    unsigned eax = ld<uint32_t>(ctx, self + 8);  // 0x286a3
    st<u32>(ctx, self, snd_va(ctx, snd_rva::vtbl_SoundWindowObj));  // 0x286a6
    if (eax) {  // 0x286ac
        if (ld<uint8_t>(ctx, self + 4) & 4) {  // 0x286b0 / 0x286b4
            eax = snd_import(ctx, kIatSetWindowLongA, SndAbi::Stdcall, {eax, static_cast<uint32_t>(-4), ld<uint32_t>(ctx, self + 0xc)}).eax;  // 0x286bd
            unsigned char al = ld<uint8_t>(ctx, self + 4);  // 0x286c3
            st<uint32_t>(ctx, self + 0xc, 0);  // 0x286c6
            al &= 0xfb;  // 0x286cd
            st<uint32_t>(ctx, self + 8, 0);  // 0x286cf
            st<uint8_t>(ctx, self + 4, al);  // 0x286d6
            eax = (eax & 0xffffff00u) | al;
        }
    }
    return eax;
}
uint32_t SoundWindowObj_dtor_body(Ctx &ctx, u32 self) { return c3dt_swoBody(ctx, self); }
int g_negctl_SoundWindowObj_dtor = 0;
void a_SoundWindowObj_dtor(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(SoundWindowObj_dtor), SoundWindowObj_dtor_body(ctx, a[0]))); }

// ---- SoundWindowObj_unsubclass (rva 0x287c0): converted from c3dh.cpp Unsub_re
uint32_t SoundWindowObj_unsubclass_body(Ctx &ctx, u32 self) {
    u32 hwnd = ld<u32>(ctx, self + 8);  // 0x287c3
    if (hwnd != 0 && (ld<uint8_t>(ctx, self + 4) & 4) != 0) {  // 0x287c6 / 0x287ca
        snd_import(ctx, kIatSetWindowLongA, SndAbi::Stdcall, {hwnd, static_cast<uint32_t>(-4), ld<u32>(ctx, self + 0xc)}).eax;  // 0x287d7
        unsigned char flags = ld<uint8_t>(ctx, self + 4);  // 0x287dd
        st<uint32_t>(ctx, self + 0xc, 0);  // 0x287e0
        flags = static_cast<uint8_t>(flags & 0xfb);  // 0x287e7
        st<uint32_t>(ctx, self + 8, 0);  // 0x287e9
        st<uint8_t>(ctx, self + 4, flags);  // 0x287f0
    }
    return 0;  // 0x287f3
}
int g_negctl_SoundWindowObj_unsubclass = 0;
void a_SoundWindowObj_unsubclass(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(SoundWindowObj_unsubclass), SoundWindowObj_unsubclass_body(ctx, a[0]))); }

// ---- Sound_setSourceType (rva 0x2ad10): converted from c3au.cpp SoundSetSourceType_re
void Sound_setSourceType_body(Ctx &ctx, u32 self, uint32_t type) {
    if (type - 1u <= 6u) {
        switch (type) {
            case 1: st<uint32_t>(ctx, self + 0x44, ld<uint32_t>(ctx, self + 0x44) | 0x04u); break;
            case 2: st<uint32_t>(ctx, self + 0x44, ld<uint32_t>(ctx, self + 0x44) | 0x08u); break;
            case 3: break;
            case 4: st<uint32_t>(ctx, self + 0x44, ld<uint32_t>(ctx, self + 0x44) | 0x10u); break;
            case 5: st<uint32_t>(ctx, self + 0x44, ld<uint32_t>(ctx, self + 0x44) | 0x28u); break;
            case 6: st<uint32_t>(ctx, self + 0x44, ld<uint32_t>(ctx, self + 0x44) | 0x100u); break;
            case 7: st<uint32_t>(ctx, self + 0x44, ld<uint32_t>(ctx, self + 0x44) | 0x80u); break;
        }
    }
    st<uint32_t>(ctx, self + 0x54, type);
}
int g_negctl_Sound_setSourceType = 0;
void a_Sound_setSourceType(Ctx &ctx, const uint32_t *a, Ret &) { Sound_setSourceType_body(ctx, a[0], a[1]); neg_mem(ctx, NEG(Sound_setSourceType), a[0] + 0x54); }

// ---- Engine_playSound (rva 0x2add0): converted from c3fd.cpp PlaySnd_re
uint32_t Engine_playSound_body(Ctx &ctx, u32 self, u32 name) {
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
}
int g_negctl_Engine_playSound = 0;
void a_Engine_playSound(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Engine_playSound), Engine_playSound_body(ctx, a[0], a[1]))); }

// ---- getPlaybackTime (rva 0x2baa0): converted from c3cl.cpp GetPlaybackTime_re
int32_t getPlaybackTime_body(Ctx &ctx, u32 self) {
    u32 device = ld<u32>(ctx, self + 0x40);  // 0x2baa3
    if (device) return static_cast<int32_t>(snd_vcall(ctx, device, 0x5c, SndAbi::Thiscall, {}).eax);  // jne 0x2bad8 at 0x2baa8; call at 0x2bada
    unsigned state = ld<uint32_t>(ctx, self + 0x58);  // 0x2baaa
    if (((state >> 4) & 1) == 0) return 0;  // 0x2baad-0x2bab2, then 0x2bad4
    if (ld<uint32_t>(ctx, self + 0x68) == 0) return 0;  // 0x2bab4, je 0x2badf at 0x2bab9
    unsigned now = snd_import(ctx, 0x000b8654, SndAbi::Stdcall, {}).eax;  // the timeGetTime import slot called at 0x2babb
    unsigned elapsed = now - ld<uint32_t>(ctx, self + 0x68);  // re-read at 0x2bac1, sub at 0x2bac7
    if (ld<uint32_t>(ctx, self + 0x64) <= elapsed) return 0;  // jbe 0x2badf at 0x2bacb
    return 1;  // 0x2bacd
}
int g_negctl_getPlaybackTime = 0;
void a_getPlaybackTime(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_i32(static_cast<int32_t>(neg_u32(NEG(getPlaybackTime), static_cast<uint32_t>(getPlaybackTime_body(ctx, a[0]))))); }

// ---- Sound_assignName (rva 0x2ddf0): converted from c3dj.cpp SoundAssignName_re
uint32_t Sound_assignName_body(Ctx &ctx, u32 self, u32 name) {
    if (name == 0) return 0xa;  // 0x2ddf8 / 0x2ddfa / 0x2de55
    u32 old = ld<u32>(ctx, self + 0x50);  // 0x2ddfc
    if (old != 0) snd_delete(ctx, old);  // 0x2de01 / 0x2de04
    st<uint32_t>(ctx, self + 0x50, 0);  // 0x2de14
    c3dj_dupInto(ctx, self + 0x50, name);  // 0x2de20-0x2de4c
    return 0;  // 0x2de4a
}
int g_negctl_Sound_assignName = 0;
void a_Sound_assignName(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Sound_assignName), Sound_assignName_body(ctx, a[0], a[1]))); }

// ---- CVoiceRxMgr::ctor (rva 0x30080): converted from c3do.cpp Sound5c450Ctor_re
u32 CVoiceRxMgr_ctor_body(Ctx &ctx, u32 self) {
    snd_call(ctx, 0x0002ed50, SndAbi::Thiscall, {self}).eax;  // 0x3009f -> 0x2ed50
    st<uint32_t>(ctx, self + 0x5c, 0);  // 0x300aa
    st<uint32_t>(ctx, self + 0x60, 0);  // 0x300ad
    st<uint32_t>(ctx, self + 0x64, 0);  // 0x300b0
    st<uint32_t>(ctx, self + 0x68, 0);  // 0x300b3
    st<u32>(ctx, self, snd_va(ctx, snd_rva::vt_Sound5c450));  // 0x300b9
    st<uint32_t>(ctx, self + 0xb8, 0);  // 0x300bf
    st<uint32_t>(ctx, self + 0xb4, 0);  // 0x300c5
    st<uint32_t>(ctx, self + 0x58, 0);  // 0x300cb
    for (int off = 0x6c; off < 0x6c + 0x12 * 4; off += 4) st<uint32_t>(ctx, self + off, 0);  // 0x300d7
    snd_call(ctx, 0x0002ad10, SndAbi::Thiscall, {self, 6}).eax;  // 0x300e2 -> 0x2ad10
    st<uint32_t>(ctx, self + 0xd4, 0);  // 0x300eb
    st<uint32_t>(ctx, self + 0xe4, 0);  // 0x300f1
    st<uint32_t>(ctx, self + 0xd8, 0);  // 0x300f7
    st<uint32_t>(ctx, self + 0xe0, 0);  // 0x300fd
    return self;  // 0x30103
}
int g_negctl_CVoiceRxMgr_ctor = 0;
void a_CVoiceRxMgr_ctor(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(CVoiceRxMgr_ctor), CVoiceRxMgr_ctor_body(ctx, a[0]))); }

// ---- Sound5c450_scalarDtor (rva 0x30150): converted from c3dt.cpp S5c450Del_re
uint32_t Sound5c450_scalarDtor_body(Ctx &ctx, u32 self, uint32_t flags) {
    snd_call(ctx, 0x000019c4, SndAbi::Thiscall, {self}).eax;
if (flags & 1) snd_delete(ctx, self);
return self;
}
int g_negctl_Sound5c450_scalarDtor = 0;
void a_Sound5c450_scalarDtor(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Sound5c450_scalarDtor), Sound5c450_scalarDtor_body(ctx, a[0], a[1]))); }

// ---- CVoiceRxMgr::dtor (rva 0x30180): converted from c3fk.cpp S5c450Dtor_re
constexpr uint32_t kIatMmioClose = 0xb8670;  // c3fk.cpp
constexpr uint32_t kMmioClose = kIatMmioClose;  // c3fg.cpp names the same slot
constexpr uint32_t kMmioCursor = 0xb49d0;  // c3fk.cpp
constexpr uint32_t kMmioHead = 0xb49c8;  // c3fk.cpp
constexpr uint32_t kMmioTail = 0xb49cc;  // c3fk.cpp
void CVoiceRxMgr_dtor_body(Ctx &ctx, u32 self) {
    st<u32>(ctx, self, snd_va(ctx, snd_rva::vt_Sound5c450));  // 0x301a0
    while (ld<u32>(ctx, self + 0x5c)) {  // 0x301a6-0x301b5, 0x30283-0x30288
        u32 n = c3fk_popHead(ctx, (self + 0x5c), (self + 0x60));  // 0x301bb-0x301cc
        u32 b = ld<u32>(ctx, n + 8);  // 0x301cf
        snd_delete(ctx, n);  // 0x301d3
        st<int32_t>(ctx, self + 0x68, static_cast<int32_t>(ld<int32_t>(ctx, self + 0x68) - 1));  // 0x301d8-0x301e1
        if (!b) break;  // 0x301e4 / 0x302b3
        if (unsigned h = ld<uint32_t>(ctx, b + 0x44)) {  // 0x301f0-0x301f5
            snd_import(ctx, kIatMmioClose, SndAbi::Stdcall, {h, 0}).eax;  // 0x301f7-0x301f9
            u32 m = c3fk_gref(ctx, kMmioHead);  // 0x301fb
            while (m && ld<u32>(ctx, m + 8) != b) m = ld<u32>(ctx, m + 4);  // 0x30200-0x30210
            if (m) {  // 0x30212 / 0x30214
                u32 prev = ld<u32>(ctx, m);  // 0x30216
                if (prev) st<u32>(ctx, prev + 4, ld<u32>(ctx, m + 4));  // 0x3021c / 0x3021f
                else st<u32>(ctx, snd_va(ctx, kMmioHead), ld<u32>(ctx, m + 4));  // 0x30224 / 0x30227
                u32 next = ld<u32>(ctx, m + 4);  // 0x3022d
                if (next) {  // 0x30230 / 0x30232
                    st<u32>(ctx, next, ld<u32>(ctx, m));  // 0x30234 / 0x30236
                    st<u32>(ctx, snd_va(ctx, kMmioCursor), next);  // 0x30238
                } else {
    st<u32>(ctx, snd_va(ctx, kMmioCursor), 0);  // 0x30242
                    st<u32>(ctx, snd_va(ctx, kMmioTail), ld<u32>(ctx, m));  // 0x30240 / 0x30248
                }
                snd_delete(ctx, m);  // 0x3024f
                st<int32_t>(ctx, snd_obj(ctx, &SoundState::g_openSoundCount).va(), static_cast<int32_t>(ld<int32_t>(ctx, snd_obj(ctx, &SoundState::g_openSoundCount).va()) - 1));  // 0x30254-0x3025d
            }
        }
        st<int32_t>(ctx, snd_obj(ctx, &SoundState::g_openMmioCount).va(), static_cast<int32_t>(ld<int32_t>(ctx, snd_obj(ctx, &SoundState::g_openMmioCount).va()) - 1));  // 0x30262-0x3026b
        snd_call(ctx, 0x00001b45, SndAbi::Thiscall, {b}).eax;  // 0x30273 / 0x30275
        snd_delete(ctx, b);  // 0x3027b
    }
    c3fk_drainNodes(ctx, self, 0x5c, 0x60, 0x68, false);  // 0x302b9-0x3031f
    snd_call(ctx, 0x00001cc6, SndAbi::Thiscall, {self}).eax;  // 0x3032b
}
int g_negctl_CVoiceRxMgr_dtor = 0;
void a_CVoiceRxMgr_dtor(Ctx &ctx, const uint32_t *a, Ret &) { CVoiceRxMgr_dtor_body(ctx, a[0]); neg_mem(ctx, NEG(CVoiceRxMgr_dtor), a[0] + 0x58); }

// ---- Sound_stop (rva 0x303f0): converted from c3dh.cpp SoundStop_re
// Pops the front node of a node list embedded in an object (front pointer at self+frontOff, back at +backOff, count at +countOff;
// nodes {+0 previous, +4 next, +8 element}): the next node's +0 is cleared and becomes the front, or both header pointers
// are cleared when it was the last (0x61a2-0x61ac); the node is released by operator delete 0x4249a (0x61b3) and the
// count is decremented from a read made after the call (0x61b8 / 0x61be). Returns the element.
u32 c3dh_popFront(Ctx &ctx, u32 self, uint32_t frontOff, uint32_t backOff, uint32_t countOff, u32 node) {
    u32 next = ld<u32>(ctx, node + 4);
    if (next) {
        st<uint32_t>(ctx, next, 0);
        st<u32>(ctx, self + frontOff, next);
    } else {
        st<uint32_t>(ctx, self + backOff, 0);
        st<uint32_t>(ctx, self + frontOff, 0);
    }
    u32 elem = ld<u32>(ctx, node + 8);
    snd_delete(ctx, node);
    st<uint32_t>(ctx, self + countOff, ld<uint32_t>(ctx, self + countOff) - 1);
    return elem;
}
// The three bodies below close each element's handle and release it; the shape is Sound::stop / Sound::freeBufferList /
// closeMmioNodeList. Element release: MidiReader-like destructor thunk 0x1b45 (a bare `ret` body 0x37df0), operator delete.
void c3dh_dtorThunk(Ctx &ctx, u32 elem) { snd_call(ctx, 0x00037df0, SndAbi::Thiscall, {elem}); }
// The shared element release of Sound_stop and Sound_freeBufferList. A non-zero handle at +0x44 (0x3044f / 0x30454) is closed with
// mmioClose (handle, 0) (0x30458); the module's live handle-record list (front 0xb49c8, back 0xb49cc, cursor 0xb49d0, count
// 0xb49d4) is searched from the front through the +4 links for the node whose +8 is this element (0x30463-0x3046b) and, when found, unlinked
// as in c3dh_popFront's counterpart (0x30475-0x3049d: previous.next = next or the front = next; next.previous = previous with the cursor
// = next, or cursor = 0 and back = previous), released with operator delete (0x304ae) and the list count decremented (0x304b3-0x304bc).
// Then, handle or not, the module counter 0xb4a08 is decremented (0x304c1-0x304ca) and the element is released through its destructor
// thunk 0x1b45 (0x304d4) and operator delete (0x304da).
void c3dh_releaseSoundElem(Ctx &ctx, u32 elem) {
    unsigned h = ld<uint32_t>(ctx, elem + 0x44);  // 0x3044f
    if (h) {  // 0x30454
        snd_import(ctx, kIatMmioClose, SndAbi::Stdcall, {h, 0}).eax;  // 0x30458
        u32 node = ld<u32>(ctx, snd_arr(ctx, &SoundState::g_openSoundList).va());  // 0x3045a
        while (node && ld<u32>(ctx, node + 8) != elem) node = ld<u32>(ctx, node + 4);  // 0x30463-0x3046d
        if (node) {  // 0x30473
            u32 prev = ld<u32>(ctx, node);  // 0x30475
            if (prev) st<u32>(ctx, prev + 4, ld<u32>(ctx, node + 4));  // 0x3047e
            else st<u32>(ctx, snd_arr(ctx, &SoundState::g_openSoundList).va(), ld<u32>(ctx, node + 4));  // 0x30486
            u32 next = ld<u32>(ctx, node + 4);  // 0x3048c
            if (next) {
                st<u32>(ctx, next, ld<u32>(ctx, node));  // 0x30495
                st<u32>(ctx, snd_obj(ctx, &SoundState::g_openSoundListCursor).va(), next);  // 0x30497
            } else {
                u32 p = ld<u32>(ctx, node);  // 0x3049f
                st<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_openSoundListCursor).va(), 0);  // 0x304a1
                st<u32>(ctx, snd_obj(ctx, &SoundState::g_openSoundListPrevVal).va(), p);  // 0x304a7
            }
            snd_delete(ctx, node);  // 0x304ae
            st<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_openSoundCount).va(), ld<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_openSoundCount).va()) - 1);  // 0x304b3-0x304bc
        }
    }
    st<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_openMmioCount).va(), ld<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_openMmioCount).va()) - 1);  // 0x304c1-0x304ca
    c3dh_dtorThunk(ctx, elem);  // 0x304d4
    snd_delete(ctx, elem);  // 0x304da
}
uint32_t Sound_stop_body(Ctx &ctx, u32 self) {
    if ((ld<uint8_t>(ctx, self + 0x44) & 2) != 0) {  // 0x303f3
        snd_vcall(ctx, self, 0x20, SndAbi::Thiscall, {});  // 0x303fb
        st<uint32_t>(ctx, self + 0x58, ld<uint32_t>(ctx, self + 0x58) | 8);  // 0x303fe-0x30403
        return 0;  // 0x30406
    }
    u32 node = ld<u32>(ctx, self + 0x5c);  // 0x3040a
    if (node) {  // 0x30411
        u32 elem = c3dh_popFront(ctx, self, 0x5c, 0x60, 0x68, node);  // 0x3041a-0x3043e
        while (elem) {  // 0x30443 / 0x30512
            c3dh_releaseSoundElem(ctx, elem);  // 0x30449-0x304df
            node = ld<u32>(ctx, self + 0x5c);  // 0x304e2
            if (!node) break;  // 0x304e7
            elem = c3dh_popFront(ctx, self, 0x5c, 0x60, 0x68, node);  // 0x304e9-0x3050f
        }
    }
    return 0;  // 0x3051d
}
int g_negctl_Sound_stop = 0;
void a_Sound_stop(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Sound_stop), Sound_stop_body(ctx, a[0]))); }

// ---- Sound_freeBufferList (rva 0x30570): converted from c3dh.cpp FreeBufList_re
uint32_t Sound_freeBufferList_body(Ctx &ctx, u32 self) {
    u32 node = ld<u32>(ctx, self + 0x5c);  // 0x30578
    if (node) {  // 0x3057d
        u32 elem = c3dh_popFront(ctx, self, 0x5c, 0x60, 0x68, node);  // 0x30583-0x305a9
        while (elem) {  // 0x305ac / 0x3067b
            c3dh_releaseSoundElem(ctx, elem);  // 0x305b2-0x30648
            node = ld<u32>(ctx, self + 0x5c);  // 0x3064b
            if (!node) break;  // 0x30650
            elem = c3dh_popFront(ctx, self, 0x5c, 0x60, 0x68, node);  // 0x30652-0x30678
        }
    }
    return 0;  // 0x30684
}
int g_negctl_Sound_freeBufferList = 0;
void a_Sound_freeBufferList(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Sound_freeBufferList), Sound_freeBufferList_body(ctx, a[0]))); }

// ---- Sound_resetBuffers (rva 0x32020): converted from c3fg.cpp ResetBufs_re
uint32_t c3fg_mmioClose(Ctx &ctx, u32 h, uint32_t f) { return snd_import(ctx, kMmioClose, SndAbi::Stdcall, {h, f}).eax; }
// The unlink sequence shared by Sound_resetBuffers (0x32074-0x320d1), closeMmioSound (0x35ce5-0x35d42) and
// parseRiffWave (0x32b7a-0x32bd8): the first node of g_openSoundList whose value is `value` is taken out (prev->next
// or the head updated, next->prev updated and the cursor set to next; with no next, the cursor = 0 and the previous-value
// word = the node's prev), freed with operator delete, and g_openSoundCount lowered. Returns the node (0 when none
// matched) and, through *count, the lowered count (eax of the originals after the decrement).
u32 c3fg_unlinkOpenSound(Ctx &ctx, u32 value, u32 *count) {
    const u32 head = snd_arr(ctx, &SoundState::g_openSoundList).va();
    u32 n = ld<u32>(ctx, head);
    while (n && ld<u32>(ctx, n + 8) != value) n = ld<u32>(ctx, n + 4);
    if (!n) return 0;
    const u32 prev = ld<u32>(ctx, n), next = ld<u32>(ctx, n + 4);
    if (prev) st<u32>(ctx, prev + 4, next);
    else st<u32>(ctx, head, next);
    if (next) {
        st<u32>(ctx, next, prev);
        st<u32>(ctx, snd_obj(ctx, &SoundState::g_openSoundListCursor).va(), next);
    } else {
        st<u32>(ctx, snd_obj(ctx, &SoundState::g_openSoundListCursor).va(), 0);
        st<u32>(ctx, snd_obj(ctx, &SoundState::g_openSoundListPrevVal).va(), prev);
    }
    snd_delete(ctx, n);
    const u32 cnt = snd_obj(ctx, &SoundState::g_openSoundCount).va();
    st<uint32_t>(ctx, cnt, ld<uint32_t>(ctx, cnt) - 1u);
    *count = ld<uint32_t>(ctx, cnt);
    return n;
}
uint32_t Sound_resetBuffers_body(Ctx &ctx, u32 self) {
    if (ld<u32>(ctx, self + 8)) {  // 0x3202a
        snd_delete(ctx, ld<u32>(ctx, self + 8));  // 0x3202d
        st<u32>(ctx, self + 8, 0);  // 0x32035
    }
    for (int k = 0; k < 0x12; ++k) st<uint32_t>(ctx, self + 0xc + 4 * k, 0);  // 0x32048
    st<uint32_t>(ctx, self + 0x58, 0);  // 0x32051 / 0x3205b
    st<uint32_t>(ctx, self + 0x5c, 0);  // 0x32053
    unsigned eax = 0;
    u32 h = ld<u32>(ctx, self);  // 0x3205d
    if (h) {  // 0x32061
        c3fg_mmioClose(ctx, h, 0);  // 0x32065
        unsigned count = 0;
        if (c3fg_unlinkOpenSound(ctx, self + 0xc, &count)) eax = count;  // 0x3206b-0x320d1
        const u32 mc = snd_obj(ctx, &SoundState::g_openMmioCount).va();
        st<uint32_t>(ctx, mc, ld<uint32_t>(ctx, mc) - 1u);  // 0x320d6
    }
    st<u32>(ctx, self, 0);  // 0x320dc
    return eax;
}
int g_negctl_Sound_resetBuffers = 0;
void a_Sound_resetBuffers(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Sound_resetBuffers), Sound_resetBuffers_body(ctx, a[0]))); }

// ---- MultiSound_ctor (rva 0x39690): converted from c3fd.cpp MsCtor_re
u32 MultiSound_ctor_body(Ctx &ctx, u32 self) {
    snd_call(ctx, 0x00001465, SndAbi::Thiscall, {self}).eax;  // 0x396ae
    snd_call(ctx, 0x00001005, SndAbi::Thiscall, {(self + 0x70)}).eax;  // 0x396bc
    st<u32>(ctx, self, snd_va(ctx, 0x5c7e8));  // 0x396c9
    st<uint32_t>(ctx, self + 0x1424, 0);  // 0x396d4
    st<uint32_t>(ctx, self + 0x1428, 0);  // 0x396db
    st<uint32_t>(ctx, self + 0x1420, 0);  // 0x396e4
    st<uint32_t>(ctx, self + 0x142c, 0);  // 0x396ea
    st<uint32_t>(ctx, self + 0x1438, 1);  // 0x396f0
    st<uint32_t>(ctx, self + 0x143c, 0);  // 0x396f6
    st<uint32_t>(ctx, self + 0x1440, 0);  // 0x396fc
    st<uint32_t>(ctx, self + 0x1444, 0);  // 0x396fe
    st<uint32_t>(ctx, self + 0x1448, 0);  // 0x39701
    st<uint32_t>(ctx, self + 0x1430, 0);  // 0x39706
    st<uint32_t>(ctx, self + 0x1434, 0);  // 0x3970c
    snd_call(ctx, 0x000018a2, SndAbi::Thiscall, {self, 1}).eax;  // 0x39712
    return self;  // 0x3971b
}
int g_negctl_MultiSound_ctor = 0;
void a_MultiSound_ctor(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(MultiSound_ctor), MultiSound_ctor_body(ctx, a[0]))); }

// ---- MultiSound_deleteDtor (rva 0x39760): converted from c3dt.cpp MultiDel_re
uint32_t MultiSound_deleteDtor_body(Ctx &ctx, u32 self, uint32_t flags) {
    snd_call(ctx, 0x00001d39, SndAbi::Thiscall, {self}).eax;
if (flags & 1) snd_delete(ctx, self);
return self;
}
int g_negctl_MultiSound_deleteDtor = 0;
void a_MultiSound_deleteDtor(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(MultiSound_deleteDtor), MultiSound_deleteDtor_body(ctx, a[0], a[1]))); }

// ---- MultiSound_ctorWithType (rva 0x39790): converted from c3fd.cpp MsCtorT_re
u32 MultiSound_ctorWithType_body(Ctx &ctx, u32 self, uint32_t type) {
    snd_call(ctx, 0x00001465, SndAbi::Thiscall, {self}).eax;  // 0x397ad
    snd_call(ctx, 0x00001005, SndAbi::Thiscall, {(self + 0x70)}).eax;  // 0x397bd
    st<u32>(ctx, self, snd_va(ctx, 0x5c7e8));  // 0x397ca
    st<uint32_t>(ctx, self + 0x1424, 0);  // 0x397d5
    st<uint32_t>(ctx, self + 0x1428, 0);  // 0x397dc
    st<uint32_t>(ctx, self + 0x1420, type);  // 0x397e3
    st<uint32_t>(ctx, self + 0x1438, 1);  // 0x397e9
    st<uint32_t>(ctx, self + 0x1430, 0);  // 0x397ef
    st<uint32_t>(ctx, self + 0x1434, 0);  // 0x397f9
    st<uint32_t>(ctx, self + 0x1ec, type);  // 0x39803
    st<uint32_t>(ctx, self + 0x1440, 0);  // 0x39813
    st<uint32_t>(ctx, self + 0x1444, 0);  // 0x39815
    st<uint32_t>(ctx, self + 0x1448, 0);  // 0x39818
    snd_call(ctx, 0x000018a2, SndAbi::Thiscall, {self, 1}).eax;  // 0x3981b
    return self;  // 0x39824
}
int g_negctl_MultiSound_ctorWithType = 0;
void a_MultiSound_ctorWithType(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(MultiSound_ctorWithType), MultiSound_ctorWithType_body(ctx, a[0], a[1]))); }

// ---- MultiSound_dtor (rva 0x39870): converted from c3fd.cpp MsDtor_re
void MultiSound_dtor_body(Ctx &ctx, u32 self) {
    st<u32>(ctx, self, snd_va(ctx, 0x5c7e8));  // 0x39890
    if (ld<int32_t>(ctx, self + 0x1438) > 1) {  // 0x398a1 / 0x398a7
        int i = 1;
        u32 m = self + 0x1440;
        do {
            if (ld<u32>(ctx, m)) {  // 0x398b2 / 0x398b4
                snd_vcall(ctx, ld<u32>(ctx, m), 0, SndAbi::Thiscall, {1});  // 0x398b8 / 0x398ba
                st<u32>(ctx, m, 0);  // 0x398bc
            }
            ++i;
            m += 4;
        } while (i < ld<int32_t>(ctx, self + 0x1438));  // 0x398c2-0x398ce
    }
    if (ld<uint8_t>(ctx, self + 0xc8) & 2)  // 0x398d1 / 0x398d8
        snd_call(ctx, 0x00001d11, SndAbi::Thiscall, {(self + 0x70)});  // 0x398dd
    if (ld<u32>(ctx, self + 0x1430)) {  // 0x398e8 / 0x398ea
        snd_delete(ctx, ld<u32>(ctx, self + 0x1430));  // 0x398ed
        st<u32>(ctx, self + 0x1430, 0);  // 0x398f5
    }
    snd_call(ctx, 0x000011b8, SndAbi::Thiscall, {self ? (self + 0x70) : 0});  // 0x398ff-0x3990f
    snd_call(ctx, 0x00002108, SndAbi::Thiscall, {self});  // 0x3991e
}
int g_negctl_MultiSound_dtor = 0;
void a_MultiSound_dtor(Ctx &ctx, const uint32_t *a, Ret &) { MultiSound_dtor_body(ctx, a[0]); neg_mem(ctx, NEG(MultiSound_dtor), a[0] + 0x1430); }

// ---- SoundGroup_stop (rva 0x39b30): converted from c3ex.cpp GroupStop_re
uint32_t SoundGroup_stop_body(Ctx &ctx, u32 self) {
    snd_vcall(ctx, self, 0x5c, SndAbi::Thiscall, {});  // 0x39b35
    int n = ld<int32_t>(ctx, self + 0x1438);  // 0x39b38
    if (n > 1) {  // 0x39b3e / 0x39b41
        int i = 1;  // 0x39b45
        u32 p = self + 0x1440;  // 0x39b4a
        do {
            if (ld<u32>(ctx, p)) {  // 0x39b50-0x39b54
                snd_call(ctx, 0x000016a4, SndAbi::Thiscall, {ld<u32>(ctx, p)});  // 0x39b56
                u32 m = ld<u32>(ctx, p);  // 0x39b5b
                if (m) snd_vcall(ctx, m, 0, SndAbi::Thiscall, {1});  // 0x39b5d-0x39b65
                st<u32>(ctx, p, 0);  // 0x39b67
            }
            n = ld<int32_t>(ctx, self + 0x1438);  // 0x39b6d
            ++i;  // 0x39b73
            p += 4;  // 0x39b74
        } while (i < n);  // 0x39b77 / 0x39b79
        st<int32_t>(ctx, self + 0x1438, 1);  // 0x39b7c
    }
    if (ld<uint8_t>(ctx, self + 0xc8) & 2) {  // 0x39b87 / 0x39b8e
        unsigned r = snd_call(ctx, 0x00001d11, SndAbi::Thiscall, {(self + 0x70)}).eax;  // 0x39b93
        st<uint32_t>(ctx, self + 0x44, static_cast<uint32_t>(ld<uint32_t>(ctx, self + 0x44) & 0xfffffffeu));  // 0x39b98-0x39b9e
        return r;
    }
    return 0x14;  // 0x39ba3
}
int g_negctl_SoundGroup_stop = 0;
void a_SoundGroup_stop(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(SoundGroup_stop), SoundGroup_stop_body(ctx, a[0]))); }

// ---- MultiSound_enqueueCmd3 (rva 0x39bd0): converted from c3dt.cpp MsEnq3_re
u32 c3dt_glob(Ctx &ctx, uint32_t rva) { return ld<u32>(ctx, snd_va(ctx, rva)); }
uint32_t MultiSound_enqueueCmd3_body(Ctx &ctx, u32 self) {
    u32 vt = ld<u32>(ctx, self);  // 0x39bd3
    unsigned r = snd_call_va(ctx, ld<u32>(ctx, vt + 0x60), SndAbi::Thiscall, {self}).eax;  // 0x39bd5
    if (r == 0) return 0;  // 0x39bd8 / 0x39bda
    u32 eng = c3dt_glob(ctx, 0x000b4a20);  // 0x39bde
    if (!eng) return 0x13;  // 0x39be4 / 0x39be6
    return snd_call(ctx, 0x000020b3, SndAbi::Thiscall, {eng, self, 3}).eax;  // 0x39beb
}
int g_negctl_MultiSound_enqueueCmd3 = 0;
void a_MultiSound_enqueueCmd3(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(MultiSound_enqueueCmd3), MultiSound_enqueueCmd3_body(ctx, a[0]))); }

// ---- SoundGroup_rewind (rva 0x39c10): converted from c3dt.cpp GroupRewind_re
uint32_t SoundGroup_rewind_body(Ctx &ctx, u32 self) {
    u32 vt = ld<u32>(ctx, self);  // 0x39c16
    snd_call_va(ctx, ld<u32>(ctx, vt + 0x40), SndAbi::Thiscall, {self, ld<uint32_t>(ctx, self + 4)}).eax;  // 0x39c1b
    snd_call(ctx, 0x00001de8, SndAbi::Thiscall, {(self + 0x70)}).eax;  // 0x39c21
    return 0;  // 0x39c26
}
int g_negctl_SoundGroup_rewind = 0;
void a_SoundGroup_rewind(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(SoundGroup_rewind), SoundGroup_rewind_body(ctx, a[0]))); }

// ---- MultiSound_release (rva 0x39c40): converted from c3dt.cpp MsRelease_re
uint32_t MultiSound_release_body(Ctx &ctx, u32 self) {
    unsigned r = snd_call(ctx, 0x00001bf4, SndAbi::Thiscall, {(self + 0x70)}).eax;  // 0x39c47
    if (ld<uint8_t>(ctx, self + 0xcc) & 4) {  // 0x39c4e / 0x39c54
        u32 eng = c3dt_glob(ctx, 0x000b4a20);  // 0x39c58
        snd_call(ctx, 0x000020b3, SndAbi::Thiscall, {eng, self, 6}).eax;  // 0x39c61
    }
    return r;  // 0x39c66
}
int g_negctl_MultiSound_release = 0;
void a_MultiSound_release(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(MultiSound_release), MultiSound_release_body(ctx, a[0]))); }

// ---- MultiSound_stop (rva 0x39c80): converted from c3cx.cpp MsStop_re
int32_t MultiSound_stop_body(Ctx &ctx, u32 self) {
    return snd_call(ctx, 0x00034b90, SndAbi::Thiscall, {self + 0x70}).eax;
}
int g_negctl_MultiSound_stop = 0;
void a_MultiSound_stop(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_i32(static_cast<int32_t>(neg_u32(NEG(MultiSound_stop), static_cast<uint32_t>(MultiSound_stop_body(ctx, a[0]))))); }

// ---- MultiSound_start (rva 0x39ce0): converted from c3ex.cpp MsStart_re
u32 c3ex_glob(Ctx &ctx, uint32_t rva) { return ld<u32>(ctx, snd_va(ctx, rva)); }
// =====================================================================================================================
// D. MultiSound start paths (channel at this+0x70; Channel_resetPanState thunk 0x1866, Channel_allocAndInitVoice 0x1839,
//    serviceChannelPlayback 0x1f37, Channel_setVolume 0x15aa, WaveCmdQueue::pushNode 0x20b3 on g_soundEngine)
// =====================================================================================================================
void c3ex_pushNode1(Ctx &ctx, u32 self) {
    snd_call(ctx, 0x000020b3, SndAbi::Thiscall, {c3ex_glob(ctx, 0x000b4a20), self, 1}).eax;
}
// the closing test both start paths share: slot +0xcc returning 0, or bit 4 of this+0x58, or bit 6 of this+0xcc queues command 1
uint8_t c3ex_msNeedsPush(Ctx &ctx, u32 self) {
    if (!snd_vcall(ctx, self, 0xcc, SndAbi::Thiscall, {}).eax) return true;
    if ((ld<uint32_t>(ctx, self + 0x58) >> 4) & 1) return true;
    return ((ld<uint32_t>(ctx, self + 0xcc) >> 6) & 1) != 0;
}
uint32_t MultiSound_start_body(Ctx &ctx, u32 self) {
    unsigned f = ld<uint32_t>(ctx, self + 0xc8);  // 0x39ce3
    if ((f & 0x40) && !(f & 1)) return 0xe;  // 0x39ce9-0x39cf1
    u32 eng = c3ex_glob(ctx, 0x000b4a20);  // 0x39cf8
    if (!eng) return 1;  // 0x39cfe / 0x39d00
    if (snd_vcall(ctx, eng, 0x6c, SndAbi::Thiscall, {}).eax) return 1;  // 0x39d08-0x39d0d
    if (ld<uint8_t>(ctx, self + 0x1428) & 1) return 0x28;  // 0x39d13-0x39d1c
    u32 ch = (self + 0x70);
    if (!(ld<uint8_t>(ctx, self + 0xc8) & 2)) {  // 0x39d23 / 0x39d2a
        snd_call(ctx, 0x00001866, SndAbi::Thiscall, {ch}).eax;  // 0x39d2f
        if (!(ld<uint8_t>(ctx, self + 0x58) & 2)) return 0x14;  // 0x39d34 / 0x39d38
        if (snd_vcall(ctx, self, 0x10, SndAbi::Thiscall, {ld<uint32_t>(ctx, self + 0x50)}).eax) return 8;  // 0x39d42-0x39d49
    }
    if ((ld<uint8_t>(ctx, self + 0xc8) & 1) && !snd_vcall(ctx, self, 0xcc, SndAbi::Thiscall, {}).eax)  // 0x39d57-0x39d6c
        snd_call(ctx, 0x00001f37, SndAbi::Thiscall, {ch}).eax;  // 0x39d71
    if (ld<uint8_t>(ctx, self + 0xc8) & 0x40) return 0;  // 0x39d76-0x39d7f
    unsigned v = ld<uint32_t>(ctx, self + 0xcc);  // 0x39d83
    if ((v & 4) || ((v >> 6) & 1)) {  // 0x39d89-0x39d92
        snd_call(ctx, 0x000015aa, SndAbi::Thiscall, {ch, ld<uint32_t>(ctx, self + 4)}).eax;  // 0x39d9e
        snd_call(ctx, 0x00001f37, SndAbi::Thiscall, {ch}).eax;  // 0x39da5
    }
    if ((ld<uint8_t>(ctx, self + 0xc8) & 1) && snd_vcall(ctx, self, 0xcc, SndAbi::Thiscall, {}).eax &&
        (ld<uint8_t>(ctx, self + 0x58) & 2))  // 0x39dab-0x39dc6
        c3ex_pushNode1(ctx, self);  // 0x39dd1
    if (c3ex_msNeedsPush(ctx, self)) c3ex_pushNode1(ctx, self);  // 0x39dd6-0x39e06
    return 0;  // 0x39e0b
}
int g_negctl_MultiSound_start = 0;
void a_MultiSound_start(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(MultiSound_start), MultiSound_start_body(ctx, a[0]))); }

// ---- MultiSound::startVoice (rva 0x39f30): converted from c3ex.cpp MsStartVoice_re
uint32_t MultiSound__startVoice_body(Ctx &ctx, u32 self, u32 out) {
    st<uint32_t>(ctx, out, 0xffffffffu);  // 0x39f38
    unsigned f = ld<uint32_t>(ctx, self + 0xc8);  // 0x39f3e
    if (!(f & 1)) return 0x21;  // 0x39f44 / 0x39f46
    if (!(f & 2)) {  // 0x39f52 / 0x39f54
        snd_call(ctx, 0x00001866, SndAbi::Thiscall, {(self + 0x70)}).eax;  // 0x39f59
        if (!(ld<uint8_t>(ctx, self + 0x58) & 2)) return 0x14;  // 0x39f5e / 0x39f62
        if (snd_vcall(ctx, self, 0x10, SndAbi::Thiscall, {ld<uint32_t>(ctx, self + 0x50)}).eax) return 8;  // 0x39f6c-0x39f74
    }
    unsigned r = snd_call(ctx, 0x00001839, SndAbi::Thiscall, {(self + 0x70), out}).eax;  // 0x39f8b
    if (!r) {  // 0x39f92 / 0x39f94
        if (ld<uint8_t>(ctx, self + 0xc8) & 0x40) return 0;  // 0x39f96-0x39fa0
        c3ex_pushNode1(ctx, self);  // 0x39fa6-0x39faf
    }
    if (ld<uint8_t>(ctx, self + 0xc8) & 0x40) return 0;  // 0x39fb4-0x39fbe
    if (c3ex_msNeedsPush(ctx, self)) c3ex_pushNode1(ctx, self);  // 0x39fc4-0x39ff4
    return r;  // 0x39ff9
}
int g_negctl_MultiSound__startVoice = 0;
void a_MultiSound__startVoice(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(MultiSound__startVoice), MultiSound__startVoice_body(ctx, a[0], a[1]))); }

// ---- MultiSound_setVoiceBit1On (rva 0x3a080): converted from c3cx.cpp MsVoiceBit1_re
int32_t MultiSound_setVoiceBit1On_body(Ctx &ctx, u32 self) {
    if ((ld<uint8_t>(ctx, self + 0xc8) & 0x02) == 0) return 0x14;
    snd_call(ctx, 0x000377f0, SndAbi::Thiscall, {self + 0x70, 1}).eax;
    return 0;
}
int g_negctl_MultiSound_setVoiceBit1On = 0;
void a_MultiSound_setVoiceBit1On(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_i32(static_cast<int32_t>(neg_u32(NEG(MultiSound_setVoiceBit1On), static_cast<uint32_t>(MultiSound_setVoiceBit1On_body(ctx, a[0]))))); }

// ---- MultiSound::setVolume (rva 0x3a0d0): converted from c3cx.cpp MsSetVolume_re
int32_t MultiSound__setVolume_body(Ctx &ctx, u32 self, uint32_t arg) {
    int v = static_cast<int32_t>(arg);
    if (v > 0x7f) v = 0x7f;  // 0x3a0db, 0x3a0dd
    else if (v < 0) v = 0;  // 0x3a0e6, 0x3a0e8
    snd_call(ctx, 0x0002c370, SndAbi::Thiscall, {self, static_cast<uint32_t>(v)}).eax;  // 0x3a0ed
    if (snd_vcall(ctx, self, 0xcc, SndAbi::Thiscall, {}).eax != 0)  // 0x3a0f6, 0x3a0fe
        snd_call(ctx, 0x00035150, SndAbi::Thiscall, {self + 0x70, static_cast<uint32_t>(v)}).eax;  // 0x3a104
    const unsigned rate = (v != 0) ? static_cast<uint32_t>(v + 1) << 9 : 0u;  // 0x3a109..0x3a10e
    return snd_call(ctx, 0x000351c0, SndAbi::Thiscall, {self + 0x70, rate}).eax;  // 0x3a115
}
int g_negctl_MultiSound__setVolume = 0;
void a_MultiSound__setVolume(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_i32(static_cast<int32_t>(neg_u32(NEG(MultiSound__setVolume), static_cast<uint32_t>(MultiSound__setVolume_body(ctx, a[0], a[1]))))); }

// ---- MultiSound::setRateGlide (rva 0x3a140): converted from c3cx.cpp MsSetRateGlide_re
// 0x0003a140  MultiSound::setRateGlide (VA 0x3a140)
// Slot 13 (+0x34) of the MultiSound vtable 0x5c7e8. __thiscall with three stack arguments (`ret 0xc` at
// 0x3a197). The first two are MIDI-style values converted the same way: 0 and -1 are passed unchanged (`je` at
// 0x3a146 / 0x3a14b and 0x3a169 / 0x3a16e); any other value is clamped SIGNED to 0..0x7f (`jle` at
// 0x3a150 / 0x3a173 against 0x7f, `jge` at 0x3a15b / 0x3a17e against 0) and becomes (v + 1) << 9
// (0x3a15f, 0x3a160 / 0x3a182, 0x3a183). The third is passed unchanged. They are pushed in the original
// order (0x3a18b..0x3a18d) for `call 0x211c` (0x3a191, thunk to Channel_setRateGlide 0x34c80) on the
// channel at this+0x70, whose eax is returned.
uint32_t c3cx_midiRate(Ctx &, uint32_t v) {
    int x = static_cast<int32_t>(v);
    if (x == 0 || x == -1) return v;
    if (x > 0x7f) x = 0x7f;
    else if (x < 0) x = 0;
    return static_cast<uint32_t>(x + 1) << 9;
}
int32_t MultiSound__setRateGlide_body(Ctx &ctx, u32 self, uint32_t a, uint32_t b, uint32_t duration) {
    const unsigned ra = c3cx_midiRate(ctx, a);  // 0x3a140..0x3a160
    const unsigned rb = c3cx_midiRate(ctx, b);  // 0x3a163..0x3a183
    return snd_call(ctx, 0x00034c80, SndAbi::Thiscall, {self + 0x70, ra, rb, duration}).eax;
}
int g_negctl_MultiSound__setRateGlide = 0;
void a_MultiSound__setRateGlide(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_i32(static_cast<int32_t>(neg_u32(NEG(MultiSound__setRateGlide), static_cast<uint32_t>(MultiSound__setRateGlide_body(ctx, a[0], a[1], a[2], a[3]))))); }

// ---- MultiSound::applyFlags (rva 0x3a370): converted from c3cx.cpp MsApplyFlags_re
int32_t MultiSound__applyFlags_body(Ctx &ctx, u32 self, uint32_t flags) {
    snd_call(ctx, 0x0002c040, SndAbi::Thiscall, {self, flags}).eax;  // 0x3a379
    if (flags & 0x001) snd_call(ctx, 0x00037920, SndAbi::Thiscall, {self + 0x70}).eax;  // 0x3a386
    if (flags & 0x010) st<uint32_t>(ctx, self + 0xc8, static_cast<uint32_t>(ld<uint32_t>(ctx, self + 0xc8) | 0x20000000u));  // 0x3a390
    if (flags & 0x020) st<uint32_t>(ctx, self + 0xc8, static_cast<uint32_t>(ld<uint32_t>(ctx, self + 0xc8) | 0x80000000u));  // 0x3a39f
    if (flags & 0x040) st<uint32_t>(ctx, self + 0xcc, static_cast<uint32_t>(ld<uint32_t>(ctx, self + 0xcc) | 0x20u));  // 0x3a3b4
    if (flags & 0x100) st<uint32_t>(ctx, self + 0xcc, static_cast<uint32_t>(ld<uint32_t>(ctx, self + 0xcc) | 0x40u));  // 0x3a3c7
    if (flags & 0x200) st<uint32_t>(ctx, self + 0xcc, static_cast<uint32_t>(ld<uint32_t>(ctx, self + 0xcc) | 0x80u));  // 0x3a3da
    if (flags & 0x002) {  // 0x3a3e2
        snd_vcall(ctx, self, 0x48, SndAbi::Thiscall, {1});  // 0x3a3ed
    }
    if (flags & 0x400) st<uint32_t>(ctx, self + 0xcc, static_cast<uint32_t>(ld<uint32_t>(ctx, self + 0xcc) | 0x200u));  // 0x3a3fb
    return 0;
}
int g_negctl_MultiSound__applyFlags = 0;
void a_MultiSound__applyFlags(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_i32(static_cast<int32_t>(neg_u32(NEG(MultiSound__applyFlags), static_cast<uint32_t>(MultiSound__applyFlags_body(ctx, a[0], a[1]))))); }

// ---- MultiSound_getStatus (rva 0x3a560): converted from c3cx.cpp MsGetStatus_re
int32_t MultiSound_getStatus_body(Ctx &ctx, u32 self) {
    unsigned r = static_cast<uint32_t>(snd_call(ctx, 0x0002c0d0, SndAbi::Thiscall, {self}).eax);  // 0x3a563
    if (ld<uint32_t>(ctx, self + 0x228) != 0) r |= 8;  // 0x3a573
    return static_cast<int32_t>(r);
}
int g_negctl_MultiSound_getStatus = 0;
void a_MultiSound_getStatus(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_i32(static_cast<int32_t>(neg_u32(NEG(MultiSound_getStatus), static_cast<uint32_t>(MultiSound_getStatus_body(ctx, a[0]))))); }

// ---- MultiSound_set3DPosition (rva 0x3a5b0): converted from c3cx.cpp Ms3DPos_re
int32_t MultiSound_set3DPosition_body(Ctx &ctx, u32 self, uint32_t x, uint32_t y, uint32_t z) {
    return snd_call(ctx, 0x00037b00, SndAbi::Thiscall, {self + 0x70, x, y, z}).eax;
}
int g_negctl_MultiSound_set3DPosition = 0;
void a_MultiSound_set3DPosition(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_i32(static_cast<int32_t>(neg_u32(NEG(MultiSound_set3DPosition), static_cast<uint32_t>(MultiSound_set3DPosition_body(ctx, a[0], a[1], a[2], a[3]))))); }

// ---- MultiSound::set3DPositionX (rva 0x3a5e0): converted from c3cx.cpp Ms3DX_re
int32_t MultiSound__set3DPositionX_body(Ctx &ctx, u32 self, uint32_t x) {
    return snd_call(ctx, 0x00037b40, SndAbi::Thiscall, {self + 0x70, x}).eax;
}
int g_negctl_MultiSound__set3DPositionX = 0;
void a_MultiSound__set3DPositionX(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_i32(static_cast<int32_t>(neg_u32(NEG(MultiSound__set3DPositionX), static_cast<uint32_t>(MultiSound__set3DPositionX_body(ctx, a[0], a[1]))))); }

// ---- MultiSound::set3DBuffer (rva 0x3a600): converted from c3cx.cpp Ms3DBuf_re
int32_t MultiSound__set3DBuffer_body(Ctx &ctx, u32 self, uint32_t v) {
    snd_call(ctx, 0x0002c270, SndAbi::Thiscall, {self, v}).eax;  // 0x3a609
    return snd_call(ctx, 0x00037c00, SndAbi::Thiscall, {self + 0x70, v}).eax;  // 0x3a612
}
int g_negctl_MultiSound__set3DBuffer = 0;
void a_MultiSound__set3DBuffer(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_i32(static_cast<int32_t>(neg_u32(NEG(MultiSound__set3DBuffer), static_cast<uint32_t>(MultiSound__set3DBuffer_body(ctx, a[0], a[1]))))); }

// ---- MultiSound::set3DPositionY (rva 0x3a630): converted from c3cx.cpp Ms3DY_re
int32_t MultiSound__set3DPositionY_body(Ctx &ctx, u32 self, uint32_t y) {
    return snd_call(ctx, 0x00037b80, SndAbi::Thiscall, {self + 0x70, y}).eax;
}
int g_negctl_MultiSound__set3DPositionY = 0;
void a_MultiSound__set3DPositionY(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_i32(static_cast<int32_t>(neg_u32(NEG(MultiSound__set3DPositionY), static_cast<uint32_t>(MultiSound__set3DPositionY_body(ctx, a[0], a[1]))))); }

// ---- MultiSound::set3DPositionZ (rva 0x3a650): converted from c3cx.cpp Ms3DZ_re
int32_t MultiSound__set3DPositionZ_body(Ctx &ctx, u32 self, uint32_t z) {
    return snd_call(ctx, 0x00037bc0, SndAbi::Thiscall, {self + 0x70, z}).eax;
}
int g_negctl_MultiSound__set3DPositionZ = 0;
void a_MultiSound__set3DPositionZ(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_i32(static_cast<int32_t>(neg_u32(NEG(MultiSound__set3DPositionZ), static_cast<uint32_t>(MultiSound__set3DPositionZ_body(ctx, a[0], a[1]))))); }

// ---- MultiSound::setReverseRate (rva 0x3a6e0): converted from c3cx.cpp MsSetReverseRate_re
int32_t MultiSound__setReverseRate_body(Ctx &ctx, u32 self, uint32_t rate) {
    snd_call(ctx, 0x000355d0, SndAbi::Thiscall, {self + 0x70, rate}).eax;
    return 0;
}
int g_negctl_MultiSound__setReverseRate = 0;
void a_MultiSound__setReverseRate(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_i32(static_cast<int32_t>(neg_u32(NEG(MultiSound__setReverseRate), static_cast<uint32_t>(MultiSound__setReverseRate_body(ctx, a[0], a[1]))))); }

// ---- MultiSound_findStreamNode (rva 0x3a730): converted from c3cx.cpp MsFindStream_re
int32_t MultiSound_findStreamNode_body(Ctx &ctx, u32 self) {
    return snd_call(ctx, 0x00035770, SndAbi::Thiscall, {self + 0x70}).eax;
}
int g_negctl_MultiSound_findStreamNode = 0;
void a_MultiSound_findStreamNode(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_i32(static_cast<int32_t>(neg_u32(NEG(MultiSound_findStreamNode), static_cast<uint32_t>(MultiSound_findStreamNode_body(ctx, a[0]))))); }

// ---- MultiSound_is3DEnabled (rva 0x3a800): converted from c3cx.cpp MsIs3D_re
int32_t MultiSound_is3DEnabled_body(Ctx &ctx, u32 self) {
    if ((ld<uint8_t>(ctx, self + 0xcc) & 0x20) == 0) return 0;
    if ((ld<uint8_t>(ctx, self + 0xc8) & 0x02) == 0) return 0;
    return 1;
}
int g_negctl_MultiSound_is3DEnabled = 0;
void a_MultiSound_is3DEnabled(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_i32(static_cast<int32_t>(neg_u32(NEG(MultiSound_is3DEnabled), static_cast<uint32_t>(MultiSound_is3DEnabled_body(ctx, a[0]))))); }


// ---- SoundTimer::start (rva 0x8ce0): written from c3dh.cpp TimerStart_re
constexpr uint32_t kIatTimeBeginPeriod = 0xb8640;  // c3dh.cpp
constexpr uint32_t kIatTimeSetEvent = 0xb8644;  // c3dh.cpp
uint32_t SoundTimer__start_body(Ctx &ctx, u32 self) {
    if ((ld<uint8_t>(ctx, self + 0x3c) & 4) != 0) return 0xd;  // 0x8ce3 / 0x8ce7 / 0x8ce9
    st<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_timerBusy).va(), 0);  // 0x8cf0
    if (snd_import(ctx, kIatTimeBeginPeriod, SndAbi::Stdcall, {ld<uint32_t>(ctx, self + 0x18)}).eax == 0x61) return 0xb;  // 0x8cfe / 0x8d04
    unsigned id = snd_import(ctx, kIatTimeSetEvent, SndAbi::Stdcall,
                             {ld<uint32_t>(ctx, self), ld<uint32_t>(ctx, self + 0x18), snd_va(ctx, 0x0000128a), self,
                              ld<uint32_t>(ctx, self + 0x1c)}).eax;  // 0x8d21 (callback: the thunk to SoundTimer::onTick)
    st<uint32_t>(ctx, self + 0x40, id);  // 0x8d2a
    if (id == 0x61) return 0xb;  // 0x8d27 / 0x8d2f
    unsigned f = ld<uint32_t>(ctx, self + 0x3c);  // 0x8d36
    f = (f & ~0xffu) | static_cast<uint8_t>(f | 7);  // 0x8d39 (or al, 7)
    st<uint32_t>(ctx, self + 0x3c, f);  // 0x8d3b
    return 0;  // 0x8d3e
}
int g_negctl_SoundTimer__start = 0;
void a_SoundTimer__start(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(SoundTimer__start), SoundTimer__start_body(ctx, a[0]))); }

// ---- SoundWindowObj_ctor (rva 0x28600): written from c3fza.cpp C3fzaCtor_re
u32 SoundWindowObj_ctor_body(Ctx &ctx, u32 self) {
    st<u32>(ctx, self, snd_va(ctx, snd_rva::vtbl_SoundWindowObj));  // 0x28607
    st<uint32_t>(ctx, self + 8, 0);  // 0x2860d
    st<uint32_t>(ctx, self + 0xc, 0);  // 0x28610
    st<uint8_t>(ctx, self + 4, 0);  // 0x28613 (only the byte at +4)
    return self;  // 0x28600 `mov eax, ecx`
}
int g_negctl_SoundWindowObj_ctor = 0;
void a_SoundWindowObj_ctor(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(SoundWindowObj_ctor), SoundWindowObj_ctor_body(ctx, a[0]))); }

// ---- SoundWindowObj_subclass (rva 0x286f0): written from c3ex.cpp Subclass_re
uint32_t SoundWindowObj_subclass_body(Ctx &ctx, u32 self, u32 hwnd) {
    if (ld<uint32_t>(ctx, self + 8)) return 0;  // 0x286f4-0x286f9
    unsigned r = snd_import(ctx, kIatSetWindowLongA, SndAbi::Stdcall,
                            {hwnd, static_cast<uint32_t>(-4), snd_va(ctx, 0x000025ef)}).eax;  // 0x28707 (the thunk to soundWindowSubclassProc)
    st<uint32_t>(ctx, self + 0xc, r);  // 0x2870f
    if (!r) return 0x18;  // 0x28712 / 0x28715
    st<uint32_t>(ctx, self + 8, hwnd);  // 0x28721
    st<uint8_t>(ctx, self + 4, static_cast<uint8_t>(ld<uint8_t>(ctx, self + 4) | 4));  // 0x2871e-0x28726
    return 0;  // 0x2872a
}
int g_negctl_SoundWindowObj_subclass = 0;
void a_SoundWindowObj_subclass(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(SoundWindowObj_subclass), SoundWindowObj_subclass_body(ctx, a[0], a[1]))); }

// ---- MultiSound_setSlotSource (rva 0x2f8d0): written from c3fd.cpp SetSlot_re
uint32_t MultiSound_setSlotSource_body(Ctx &ctx, u32 self, u32 index) {
    if (ld<u32>(ctx, self + 0xfc)) return 0xf;  // 0x2f8fa-0x2f903
    if (index >= 0x28) return 0xa;  // 0x2f91e-0x2f924
    if (!ld<uint32_t>(ctx, self + 0x58 + 4 * index)) return 0x24;  // 0x2f93b-0x2f942
    unsigned flags = ld<u32>(ctx, self + 0xf8) ? 0x302u : 0x102u;  // 0x2f959-0x2f970
    u32 p = snd_new(ctx, 0x1450);  // 0x2f97a
    u32 ms = p ? snd_call(ctx, 0x000025ea, SndAbi::Thiscall, {p}).eax : 0;  // 0x2f98c-0x2f995 (MultiSound_ctor)
    snd_vcall(ctx, ms, 0x6c, SndAbi::Thiscall, {flags});  // 0x2f9a4 (no NULL test: a NULL block faults)
    unsigned r = snd_vcall(ctx, ms, 0x10, SndAbi::Thiscall, {ld<uint32_t>(ctx, self + 0x58 + 4 * index)}).eax;  // 0x2f9a7-0x2f9b4
    if (r) {  // 0x2f9b9 / 0x2f9bb
        if (ms) snd_vcall(ctx, ms, 0, SndAbi::Thiscall, {1});  // 0x2f9bd-0x2f9c7
        return r;
    }
    if (ld<uint8_t>(ctx, ms + 0xcc) & 4)  // 0x2f9cb / 0x2f9d2
        snd_call(ctx, 0x00001dde, SndAbi::Thiscall, {ms + 0x70});  // 0x2f9d7
    st<u32>(ctx, self + 0xfc, ms);  // 0x2f9e4
    u32 src = ld<u32>(ctx, self + 0xf8);  // 0x2f9e0
    if (src)  // 0x2f9e2 / 0x2f9e6
        snd_call(ctx, 0x0000122b, SndAbi::Thiscall, {src + 0x70, self + 0xfc, self + 0xf8});  // 0x2f9e8-0x2f9ed
    return r;  // 0x2f9f7
}
int g_negctl_MultiSound_setSlotSource = 0;
void a_MultiSound_setSlotSource(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(MultiSound_setSlotSource), MultiSound_setSlotSource_body(ctx, a[0], a[1]))); }

// ---- MultiSound_setPitchCents (rva 0x3a210): written from c3cx.cpp MsSetPitch_re
// The x87 work runs at the caller's 53-bit control word, so it is plain double arithmetic in the original's order; __ftol keeps the
// low dword of the 64-bit result (ftol_low32). The tables are .rdata (25 entries each: n reaches -1 and 25, the original reads just
// outside the table, and so does this body).
double md_d(Ctx &ctx, u32 va) { return pun<double>(ld<uint64_t>(ctx, va)); }
int32_t MultiSound_setPitchCents_body(Ctx &ctx, u32 self, uint32_t arg) {
    int c = static_cast<int32_t>(arg);
    if (c < -1200 || c > 1200) c = c % 1200;  // 0x3a223..0x3a237
    snd_call(ctx, 0x0002bff0, SndAbi::Thiscall, {self, static_cast<uint32_t>(c)});  // 0x3a23c
    const int q = c / 100;  // 0x3a24d
    int r = c % 100;
    if (r < 0) r = -r;  // 0x3a254
    const int s = q + 12;  // 0x3a266
    const u32 T1 = snd_va(ctx, 0x0005c608), T2 = snd_va(ctx, 0x0005c66c), D = snd_va(ctx, 0x0005c6d0);
    int baseI;
    double base;
    if (snd_vcall(ctx, self, 0xcc, SndAbi::Thiscall, {}).eax != 0) {  // 0x3a277, 0x3a286
        baseI = ld<int32_t>(ctx, T2 + static_cast<u32>(s) * 4);  // 0x3a288
        base = 0.0;  // 0x3a28e
    } else {
        baseI = ld<int32_t>(ctx, T1 + static_cast<u32>(s) * 4);  // 0x3a293
        base = md_d(ctx, D + static_cast<u32>(s) * 8);  // 0x3a299
    }
    const int n = (c < 0) ? s - 1 : s + 1;  // 0x3a2a3..0x3a2aa
    double t = md_d(ctx, D + static_cast<u32>(n) * 8) - md_d(ctx, D + static_cast<u32>(s) * 8);  // 0x3a2b4
    t = t * static_cast<double>(r);  // 0x3a2bb
    t = t * md_d(ctx, snd_va(ctx, 0x0005c910));  // 0x3a2d0
    const double val = t + base;  // 0x3a2d6
    const uint32_t delta = static_cast<uint32_t>(ld<int32_t>(ctx, T1 + static_cast<u32>(n) * 4)) -
                           static_cast<uint32_t>(ld<int32_t>(ctx, T1 + static_cast<u32>(s) * 4));  // 0x3a2cb
    double u = static_cast<double>(static_cast<int32_t>(delta)) * static_cast<double>(r);  // 0x3a2dd, 0x3a2e0
    u = u * static_cast<double>(pun<float>(ld<uint32_t>(ctx, snd_va(ctx, 0x0005c90c))));  // 0x3a2e3
    const uint32_t k = ftol_low32(u);  // __ftol at 0x3a2e9
    snd_call(ctx, 0x00035750, SndAbi::Thiscall, {self + 0x70, static_cast<uint32_t>(baseI) - k});  // 0x3a2ee, 0x3a2f4
    st<uint64_t>(ctx, self + 0x13f8, pun<uint64_t>(val));  // 0x3a2f9..0x3a305
    return 0;
}
int g_negctl_MultiSound_setPitchCents = 0;
void a_MultiSound_setPitchCents(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_i32(static_cast<int32_t>(neg_u32(NEG(MultiSound_setPitchCents), static_cast<uint32_t>(MultiSound_setPitchCents_body(ctx, a[0], a[1]))))); }

const FuncDef kFuncs[] = {
    {"snd.Sound::ctor", "thiscall", "pointer", 1, 0, a_Sound__ctor, &g_negctl_Sound__ctor},
    {"snd.Sound::scalarDeletingDtor", "thiscall", "uint32", 2, 0, a_Sound__scalarDeletingDtor, &g_negctl_Sound__scalarDeletingDtor},
    {"snd.Sound::dtor", "thiscall", "void", 1, 0, a_Sound__dtor, &g_negctl_Sound__dtor},
    {"snd.Sound::scaleTickRate", "thiscall", "int", 2, 0, a_Sound__scaleTickRate, &g_negctl_Sound__scaleTickRate},
    {"snd.registerSound", "thiscall", "uint32", 1, 0, a_registerSound, &g_negctl_registerSound},
    {"snd.unregisterSound", "thiscall", "uint32", 1, 0, a_unregisterSound, &g_negctl_unregisterSound},
    {"snd.SoundTimer::registerClient", "thiscall", "uint32", 2, 0, a_SoundTimer__registerClient, &g_negctl_SoundTimer__registerClient},
    {"snd.SoundTimer::unregisterClient", "thiscall", "uint32", 2, 0, a_SoundTimer__unregisterClient, &g_negctl_SoundTimer__unregisterClient},
    {"snd.SoundTimer::init", "thiscall", "pointer", 1, 0, a_SoundTimer__init, &g_negctl_SoundTimer__init},
    {"snd.SoundTimer::stop", "thiscall", "void", 1, 0, a_SoundTimer__stop, &g_negctl_SoundTimer__stop},
    {"snd.SoundTimer::stopKeepLock", "thiscall", "void", 1, 0, a_SoundTimer__stopKeepLock, &g_negctl_SoundTimer__stopKeepLock},
    {"snd.SoundTimer::killEvent", "thiscall", "uint32", 1, 0, a_SoundTimer__killEvent, &g_negctl_SoundTimer__killEvent},
    {"snd.SoundTimer::onTick", "stdcall", "uint32", 5, 0, a_SoundTimer__onTick, &g_negctl_SoundTimer__onTick},
    {"snd.SoundTimer::linkClient", "thiscall", "uint32", 2, 0, a_SoundTimer__linkClient, &g_negctl_SoundTimer__linkClient},
    {"snd.create_sound", "default", "uint32", 3, 0, a_create_sound, &g_negctl_create_sound},
    {"snd.Sound_initEntry", "thiscall", "pointer", 1, 0, a_Sound_initEntry, &g_negctl_Sound_initEntry},
    {"snd.Sound_anyChannelActive", "thiscall", "uint8", 1, 0, a_Sound_anyChannelActive, &g_negctl_Sound_anyChannelActive},
    {"snd.Sound_setName", "thiscall", "void", 2, 0, a_Sound_setName, &g_negctl_Sound_setName},
    {"snd.Sound_setAltName", "thiscall", "uint32", 2, 0, a_Sound_setAltName, &g_negctl_Sound_setAltName},
    {"snd.Seq::ctor_1001f4a0", "thiscall", "pointer", 1, 0x027f, a_Seq_ctor_1001f4a0, &g_negctl_Seq_ctor_1001f4a0},
    {"snd.Sound5b6a8_scalarDtor", "thiscall", "uint32", 2, 0, a_Sound5b6a8_scalarDtor, &g_negctl_Sound5b6a8_scalarDtor},
    {"snd.Seq::dtor", "thiscall", "void", 1, 0, a_Seq_dtor, &g_negctl_Seq_dtor},
    {"snd.Sound_reapIdle", "default", "uint32", 0, 0, a_Sound_reapIdle, &g_negctl_Sound_reapIdle},
    {"snd.Sound_findByName", "stdcall", "uint8", 1, 0, a_Sound_findByName, &g_negctl_Sound_findByName},
    {"snd.Sound_lookupOrCreate", "thiscall", "uint32", 2, 0, a_Sound_lookupOrCreate, &g_negctl_Sound_lookupOrCreate},
    {"snd.SoundWindowObj_dtor", "thiscall", "uint32", 1, 0, a_SoundWindowObj_dtor, &g_negctl_SoundWindowObj_dtor},
    {"snd.SoundWindowObj_unsubclass", "thiscall", "uint32", 1, 0, a_SoundWindowObj_unsubclass, &g_negctl_SoundWindowObj_unsubclass},
    {"snd.Sound_setSourceType", "thiscall", "void", 2, 0, a_Sound_setSourceType, &g_negctl_Sound_setSourceType},
    {"snd.Engine_playSound", "thiscall", "uint32", 2, 0, a_Engine_playSound, &g_negctl_Engine_playSound},
    {"snd.getPlaybackTime", "thiscall", "int", 1, 0, a_getPlaybackTime, &g_negctl_getPlaybackTime},
    {"snd.Sound_assignName", "thiscall", "uint32", 2, 0, a_Sound_assignName, &g_negctl_Sound_assignName},
    {"snd.CVoiceRxMgr::ctor", "thiscall", "pointer", 1, 0, a_CVoiceRxMgr_ctor, &g_negctl_CVoiceRxMgr_ctor},
    {"snd.Sound5c450_scalarDtor", "thiscall", "uint32", 2, 0, a_Sound5c450_scalarDtor, &g_negctl_Sound5c450_scalarDtor},
    {"snd.CVoiceRxMgr::dtor", "thiscall", "void", 1, 0, a_CVoiceRxMgr_dtor, &g_negctl_CVoiceRxMgr_dtor},
    {"snd.Sound_stop", "thiscall", "uint32", 1, 0, a_Sound_stop, &g_negctl_Sound_stop},
    {"snd.Sound_freeBufferList", "thiscall", "uint32", 1, 0, a_Sound_freeBufferList, &g_negctl_Sound_freeBufferList},
    {"snd.Sound_resetBuffers", "thiscall", "uint32", 1, 0, a_Sound_resetBuffers, &g_negctl_Sound_resetBuffers},
    {"snd.MultiSound_ctor", "thiscall", "uint32", 1, 0, a_MultiSound_ctor, &g_negctl_MultiSound_ctor},
    {"snd.MultiSound_deleteDtor", "thiscall", "uint32", 2, 0, a_MultiSound_deleteDtor, &g_negctl_MultiSound_deleteDtor},
    {"snd.MultiSound_ctorWithType", "thiscall", "uint32", 2, 0, a_MultiSound_ctorWithType, &g_negctl_MultiSound_ctorWithType},
    {"snd.MultiSound_dtor", "thiscall", "void", 1, 0, a_MultiSound_dtor, &g_negctl_MultiSound_dtor},
    {"snd.SoundGroup_stop", "thiscall", "uint32", 1, 0, a_SoundGroup_stop, &g_negctl_SoundGroup_stop},
    {"snd.MultiSound_enqueueCmd3", "thiscall", "uint32", 1, 0, a_MultiSound_enqueueCmd3, &g_negctl_MultiSound_enqueueCmd3},
    {"snd.SoundGroup_rewind", "thiscall", "uint32", 1, 0, a_SoundGroup_rewind, &g_negctl_SoundGroup_rewind},
    {"snd.MultiSound_release", "thiscall", "uint32", 1, 0, a_MultiSound_release, &g_negctl_MultiSound_release},
    {"snd.MultiSound_stop", "thiscall", "int", 1, 0, a_MultiSound_stop, &g_negctl_MultiSound_stop},
    {"snd.MultiSound_start", "thiscall", "uint32", 1, 0, a_MultiSound_start, &g_negctl_MultiSound_start},
    {"snd.MultiSound::startVoice", "thiscall", "uint32", 2, 0, a_MultiSound__startVoice, &g_negctl_MultiSound__startVoice},
    {"snd.MultiSound_setVoiceBit1On", "thiscall", "int", 1, 0, a_MultiSound_setVoiceBit1On, &g_negctl_MultiSound_setVoiceBit1On},
    {"snd.MultiSound::setVolume", "thiscall", "int", 2, 0, a_MultiSound__setVolume, &g_negctl_MultiSound__setVolume},
    {"snd.MultiSound::setRateGlide", "thiscall", "int", 4, 0, a_MultiSound__setRateGlide, &g_negctl_MultiSound__setRateGlide},
    {"snd.MultiSound::applyFlags", "thiscall", "int", 2, 0, a_MultiSound__applyFlags, &g_negctl_MultiSound__applyFlags},
    {"snd.MultiSound_getStatus", "thiscall", "int", 1, 0, a_MultiSound_getStatus, &g_negctl_MultiSound_getStatus},
    {"snd.MultiSound_set3DPosition", "thiscall", "int", 4, 0, a_MultiSound_set3DPosition, &g_negctl_MultiSound_set3DPosition},
    {"snd.MultiSound::set3DPositionX", "thiscall", "int", 2, 0, a_MultiSound__set3DPositionX, &g_negctl_MultiSound__set3DPositionX},
    {"snd.MultiSound::set3DBuffer", "thiscall", "int", 2, 0, a_MultiSound__set3DBuffer, &g_negctl_MultiSound__set3DBuffer},
    {"snd.MultiSound::set3DPositionY", "thiscall", "int", 2, 0, a_MultiSound__set3DPositionY, &g_negctl_MultiSound__set3DPositionY},
    {"snd.MultiSound::set3DPositionZ", "thiscall", "int", 2, 0, a_MultiSound__set3DPositionZ, &g_negctl_MultiSound__set3DPositionZ},
    {"snd.MultiSound::setReverseRate", "thiscall", "int", 2, 0, a_MultiSound__setReverseRate, &g_negctl_MultiSound__setReverseRate},
    {"snd.MultiSound_findStreamNode", "thiscall", "int", 1, 0, a_MultiSound_findStreamNode, &g_negctl_MultiSound_findStreamNode},
    {"snd.MultiSound_is3DEnabled", "thiscall", "int", 1, 0, a_MultiSound_is3DEnabled, &g_negctl_MultiSound_is3DEnabled},
    {"snd.SoundTimer::start", "thiscall", "uint32", 1, 0, a_SoundTimer__start, &g_negctl_SoundTimer__start},
    {"snd.SoundWindowObj_ctor", "thiscall", "uint32", 1, 0, a_SoundWindowObj_ctor, &g_negctl_SoundWindowObj_ctor},
    {"snd.SoundWindowObj_subclass", "thiscall", "uint32", 2, 0, a_SoundWindowObj_subclass, &g_negctl_SoundWindowObj_subclass},
    {"snd.MultiSound_setSlotSource", "thiscall", "uint32", 2, 0, a_MultiSound_setSlotSource, &g_negctl_MultiSound_setSlotSource},
    {"snd.MultiSound_setPitchCents", "thiscall", "int", 2, 0, a_MultiSound_setPitchCents, &g_negctl_MultiSound_setPitchCents},
};

}  // namespace

extern const ModuleDef kModule_audio_sound = {"audio_sound", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim

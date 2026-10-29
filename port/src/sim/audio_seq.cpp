// The sound.dll sequencer and MIDI family (step S4.5, log/roadmap/S4/5_notes.md; API in portsim/sim/audio.h): the Seq_*, Midi_*,
// Track_*, Midi_Device and MidiDeviceBase functions of rows S4.5 in re/roadmap/S4_functions.tsv, written from the C3 notes in the
// hook sources (shim/src/re/c3an.cpp, c3ar.cpp, c3au.cpp, c3az.cpp, c3bd.cpp, c3bi.cpp, c3bn.cpp, c3ca.cpp, c3cf.cpp, c3cl.cpp,
// c3cx.cpp, c3dh.cpp, c3dj.cpp, c3do.cpp, c3dt.cpp, c3ew.cpp, c3ex.cpp, c3fd.cpp, c3fh.cpp, c3fk.cpp, c3fl.cpp, c3fn.cpp), then
// judged by the replay (port/test/sound_replay.py through log/roadmap/S4/5_run.py). Addresses in comments are RVAs of the user's
// sound.dll.
//
// Every call to code that is not inline goes through the dispatcher (snd_call / snd_vcall / snd_import), so a fixture's
// stand-in, the host or another port function serves it.
//
// Negative controls (sg_negctl_set(<name>, mode)): mode 1 of every function is a wrong result: the value it returns is
// changed (`neg_u32`) or, for a function that returns nothing, the dword at the field the adapter names is changed after the
// body (`neg_mem`). Mode 2 of Seq_setTempo is the classic wrong x87 port (plain double arithmetic, control word ignored).
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "portsim/arith.h"
#include "portsim/ptr.h"
#include "portsim/registry.h"
#include "portsim/sim/audio.h"
#include "portsim/sim/audio_seq.h"
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

// A local variable the original passes by address (a `push ecx` slot, `&b`): the port has no stack in the context's address
// space, so the slot is a 16-byte cell in the padding after the image's last section (never read or written by the module;
// U-1440, as U-1450 of audio_chan, whose cells are the top 0x100 bytes of the image). The 28 cells here are the 0x1c0 bytes
// below that band; each function that takes the address of a local has a cell of its own, because functions of this module
// call each other (preRollTrack -> playEvent -> handleMetaEvent -> stepTrackMeta) while their slots are live.

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


// ---- shared helpers
// a module global (the pointer a `mov ecx, [global]` loads)
u32 gref(Ctx &ctx, u32 rva) { return ld<u32>(ctx, snd_va(ctx, rva)); }
// `rep stosd` of zero over n dwords
void zero_dwords(Ctx &ctx, u32 va, u32 n) {
  for (u32 k = 0; k < n; ++k) st<u32>(ctx, va + 4 * k, 0);
}
// A doubly linked node list given by the addresses of its head, tail, cursor and count words, nodes {prev +0, next +4, payload +8}:
// the first node whose payload equals key (walking the next links from the head) is unlinked (the predecessor's next, or the head;
// the successor's prev with the cursor set to the successor, or with no successor the cursor cleared and the tail set to the
// predecessor), given to operator delete, and the count is decremented from a read made after the call. Nothing happens when
// no node matches. The same code is inline in Midi_Device::shutdown (0x9e95-0x9ef7, the module list), drainCommandQueue (0xa3d8,
// 0xa467-0xa4b9) and dispatchCommand (0xa720-0xa775, 0xa79b-0xa810).
void list_unlink(Ctx &ctx, u32 headA, u32 tailA, u32 curA, u32 cntA, u32 key) {
  u32 n = ld<u32>(ctx, headA);
  while (n && ld<u32>(ctx, n + 8) != key) n = ld<u32>(ctx, n + 4);
  if (!n) return;
  const u32 prev = ld<u32>(ctx, n);
  const u32 next = ld<u32>(ctx, n + 4);
  if (prev)
    st<u32>(ctx, prev + 4, next);
  else
    st<u32>(ctx, headA, next);
  if (next) {
    st<u32>(ctx, next, ld<u32>(ctx, n));
    st<u32>(ctx, curA, next);
  } else {
    st<u32>(ctx, curA, 0);
    st<u32>(ctx, tailA, ld<u32>(ctx, n));
  }
  snd_delete(ctx, n);
  st<int32_t>(ctx, cntA, sub32(ld<int32_t>(ctx, cntA), 1));
}


// ---- Midi_Device::scalarDeletingDtor (rva 0x9c80): converted from c3dj.cpp MidiDevice_scalarDtor_re
u32 Midi_Device__scalarDeletingDtor_body(Ctx &ctx, u32 self, uint32_t flags) {
    snd_call(ctx, 0x000019e2, SndAbi::Thiscall, {self});
if (flags & 1) snd_delete(ctx, self);
return self;
}
int g_negctl_Midi_Device__scalarDeletingDtor = 0;
void a_Midi_Device__scalarDeletingDtor(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Midi_Device__scalarDeletingDtor), Midi_Device__scalarDeletingDtor_body(ctx, a[0], a[1]))); }

// ---- Midi_Device::dtor (rva 0x9cb0): converted from c3dj.cpp MidiDtor_re
// =====================================================================================================================
// H. Node lists cleared from the front (operator delete per node)
// =====================================================================================================================
// PtrList_freeNodesB 0x00031ac0 (VA 0x31ac0), nodeListClear2 0x000384b0 (0x384b0) and nodeListClear 0x00039580 (0x39580) are the
// same body, and VoiceBase_clearList 0x00029170 (0x29170) is that body over a header that starts 8 bytes later. `this` is a list
// header {front, back, (cursor), count}; the nodes are {previous, next, value}. The first node (`mov eax, [esi]` 0x31ac4; the
// function returns when it is NULL, `je` 0x31ac8) is popped: with a next node that node's previous becomes 0 and the front
// moves to it (0x31aca-0x31ad7); without one back and front become 0 (0x31adb-0x31ae2). The popped node's value
// is read (0x31ae8) and the node released with operator delete 0x4249a (0x31aec), the count is decremented (0x31af1,
// 0x31af7, 0x31afa); a NULL value ends the function (`test edi, edi` / `je` 0x31af8 / 0x31afd). Otherwise the same
// pop repeats (0x31aff-0x31b38) until the list is empty (`je` 0x31b03) or a popped node's value is NULL (`jne` 0x31b38).
void c3dj_popFrontAll(Ctx &ctx, u32 hdr, int32_t fo, int32_t bo, int32_t co) {
    u32 node = ld<u32>(ctx, hdr + fo);  // 0x31ac4
    if (node == 0) return;  // 0x31ac6 / 0x31ac8
    for (;;) {
        u32 next = ld<u32>(ctx, node + 4);  // 0x31aca
        if (next != 0) {  // 0x31acd / 0x31acf
            st<uint32_t>(ctx, next, 0);  // 0x31ad1
            st<u32>(ctx, hdr + fo, next);  // 0x31ad7
        } else {
            st<uint32_t>(ctx, hdr + bo, 0);  // 0x31adb
            st<uint32_t>(ctx, hdr + fo, 0);  // 0x31ae2
        }
        u32 value = ld<u32>(ctx, node + 8);  // 0x31ae8
        snd_delete(ctx, node);  // 0x31aec
        st<uint32_t>(ctx, hdr + co, ld<uint32_t>(ctx, hdr + co) - 1);  // 0x31af1 / 0x31af7 / 0x31afa
        if (value == 0) return;  // 0x31af8 / 0x31afd
        node = ld<u32>(ctx, hdr + fo);  // 0x31aff
        if (node == 0) return;  // 0x31b01 / 0x31b03
    }
}
void Midi_Device__dtor_body(Ctx &ctx, u32 self) {
    st<u32>(ctx, self, snd_va(ctx, snd_rva::vt_MidiSeq));  // 0x9ccf
    snd_call(ctx, 0x000019a1, SndAbi::Thiscall, {self + 0x50}).eax;  // 0x9ce0
    c3dj_popFrontAll(ctx, self, 0x40, 0x44, 0x4c);  // 0x9ce5-0x9d4d
    snd_call(ctx, 0x00001b22, SndAbi::Thiscall, {self});  // 0x9d59
}
int g_negctl_Midi_Device__dtor = 0;
void a_Midi_Device__dtor(Ctx &ctx, const uint32_t *a, Ret &) { Midi_Device__dtor_body(ctx, a[0]); neg_mem(ctx, NEG(Midi_Device__dtor), a[0] + 0x0); }

// ---- Midi_Device::shutdown (rva 0x9e40): hand-fixed conversion of c3fd.cpp Shutdown_re
uint32_t Midi_Device__shutdown_body(Ctx &ctx, u32 self) {
  if (ld<u32>(ctx, self + 0x2c)) {  // 0x9e47-0x9e4c
    snd_call(ctx, 0x000026fd, SndAbi::Thiscall, {ld<u32>(ctx, self + 0x2c)});  // 0x9e4e
    snd_call(ctx, 0x00001ed3, SndAbi::Thiscall, {gref(ctx, 0xb49ec), self});  // 0x9e53-0x9e5a
  }
  const u32 curA = snd_va(ctx, 0xb5008);  // the module list's cursor (g_soundRegistryTail)
  const u32 head = gref(ctx, 0xb5000);  // 0x9e5f
  st<u32>(ctx, curA, head);  // 0x9e66
  u32 v = head ? ld<u32>(ctx, head + 8) : 0;  // 0x9e6b-0x9e72
  snd_call(ctx, 0x0000259a, SndAbi::Thiscall, {self});  // 0x9e76
  while (v) {  // 0x9e7b / 0x9f26
    st<u32>(ctx, v + 0xac, ld<u32>(ctx, v + 0xac) & 0xffffffdfu);  // 0x9e84-0x9e8f
    const u32 w = v;  // 0x9e8a
    list_unlink(ctx, snd_va(ctx, 0xb5000), snd_va(ctx, 0xb5004), curA, snd_va(ctx, 0xb500c), v);  // 0x9e95-0x9ef7
    u32 c = ld<u32>(ctx, curA);  // 0x9efc
    v = 0;
    if (c) {  // 0x9f01 / 0x9f03
      c = ld<u32>(ctx, c + 4);  // 0x9f05
      st<u32>(ctx, curA, c);  // 0x9f0a
      if (c) v = ld<u32>(ctx, c + 8);  // 0x9f0f / 0x9f11
    }
    if (w) snd_vcall(ctx, w, 0, SndAbi::Thiscall, {1});  // 0x9f18-0x9f22
  }
  if (ld<u32>(ctx, self + 0x3c))  // 0x9f2d-0x9f32
    snd_import(ctx, 0x000b863c, SndAbi::Stdcall, {ld<u32>(ctx, self + 0x3c)});  // 0x9f34 / 0x9f35
  const u32 headA = snd_va(ctx, 0xb4fe8);  // g_cmdTableHead
  u32 n = ld<u32>(ctx, headA);  // 0x9f3b
  while (n) {  // 0x9f40 / 0x9f97
    const u32 next = ld<u32>(ctx, n + 4);  // 0x9f48
    if (next) {
      st<u32>(ctx, next, 0);  // 0x9f4f
      st<u32>(ctx, headA, next);  // 0x9f51
    } else {
      st<u32>(ctx, snd_va(ctx, 0xb4fec), 0);  // 0x9f59
      st<u32>(ctx, headA, 0);  // 0x9f5f
    }
    const u32 p = ld<u32>(ctx, n + 8);  // 0x9f65
    snd_delete(ctx, n);  // 0x9f69
    st<int32_t>(ctx, snd_va(ctx, 0xb4ff4), sub32(ld<int32_t>(ctx, snd_va(ctx, 0xb4ff4)), 1));  // 0x9f6e-0x9f7a
    if (!p) break;  // 0x9f78 / 0x9f80
    snd_call(ctx, 0x00001d66, SndAbi::Thiscall, {p});  // 0x9f84
    snd_delete(ctx, p);  // 0x9f8a
    n = ld<u32>(ctx, headA);  // 0x9f8f
  }
  return 0;  // 0x9fd7
}
int g_negctl_Midi_Device__shutdown = 0;
void a_Midi_Device__shutdown(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Midi_Device__shutdown), Midi_Device__shutdown_body(ctx, a[0]))); }

// ---- Midi_Device::enableOutput (rva 0xa1b0): converted from c3dt.cpp MidiEnable_re
uint32_t Midi_Device__enableOutput_body(Ctx &ctx, u32 self) {
    if (!(ld<uint8_t>(ctx, self + 0x24) & 1)) {  // 0xa1b3 / 0xa1b7
        u32 vt = ld<u32>(ctx, self);  // 0xa1b9
        snd_call_va(ctx, ld<u32>(ctx, vt + 0x4c), SndAbi::Thiscall, {self}).eax;  // 0xa1bb
        snd_call(ctx, 0x00001997, SndAbi::Thiscall, {snd_va(ctx, 0xb49b8)}).eax;  // 0xa1c3
        st<uint8_t>(ctx, snd_obj(ctx, &SoundState::g_midiOutputFlags).va(), static_cast<uint8_t>(ld<uint8_t>(ctx, snd_obj(ctx, &SoundState::g_midiOutputFlags).va()) | 2));  // 0xa1c8-0xa1d1
        st<uint32_t>(ctx, self + 0x24, static_cast<uint32_t>(ld<uint32_t>(ctx, self + 0x24) | 1));  // 0xa1d7-0xa1dc
    }
    return 0;  // 0xa1df
}
int g_negctl_Midi_Device__enableOutput = 0;
void a_Midi_Device__enableOutput(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Midi_Device__enableOutput), Midi_Device__enableOutput_body(ctx, a[0]))); }

// ---- Midi_Device::disableOutput (rva 0xa200): hand-fixed conversion of c3dh.cpp MidiOff_re
uint32_t Midi_Device__disableOutput_body(Ctx &ctx, u32 self) {
  if ((ld<uint8_t>(ctx, self + 0x24) & 1) == 0) return 0;  // 0xa203 / 0xa207
  const u32 g = snd_obj(ctx, &SoundState::g_midiOutputFlags).va();
  uint8_t cl = ld<uint8_t>(ctx, g);  // 0xa209
  cl = static_cast<uint8_t>(cl & 0xfd);  // 0xa20f
  st<uint8_t>(ctx, g, cl);  // 0xa212
  snd_vcall(ctx, self, 0x50, SndAbi::Thiscall, {});  // 0xa21c
  unsigned f = ld<uint32_t>(ctx, self + 0x24);  // 0xa21f
  f = (f & ~0xffu) | static_cast<uint8_t>(f & 0xfe);  // 0xa222 (and al, 0xfe)
  st<uint32_t>(ctx, self + 0x24, f);  // 0xa224
  return 0;  // 0xa227
}
int g_negctl_Midi_Device__disableOutput = 0;
void a_Midi_Device__disableOutput(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Midi_Device__disableOutput), Midi_Device__disableOutput_body(ctx, a[0]))); }

// ---- Midi_Device::drainCommandQueue (rva 0xa330): converted from c3fd.cpp Drain_re
// The same unlink on a list held in an object (head at hOff, tail hOff + 4, cursor hOff + 8, count hOff + 0xc).
void c3fd_unlinkLocal(Ctx &ctx, u32 self, int32_t hOff, u32 key) {
  list_unlink(ctx, self + hOff, self + hOff + 4, self + hOff + 8, self + hOff + 0xc, key);
}
uint32_t Midi_Device__drainCommandQueue_body(Ctx &ctx, u32 self) {
  SndFrame fr_cells(ctx, 0x10);  // S4.2b: the cells [0] of the original's locals passed by address (U-1418)
    unsigned h = ld<uint32_t>(ctx, self + 0x4050);  // 0xa337
    unsigned t = ld<uint32_t>(ctx, self + 0x4054);  // 0xa33d
    if (h == t) return 0;  // 0xa343 / 0xa34d
    unsigned n = (t - h) & 0xfff;  // 0xa357 / 0xa359
    if (n) {  // 0xa35f
        const u32 cq = fr_cells.at(0x10);  // the slots of `h` and `t` the callee takes by address (0xa36d)
        st<u32>(ctx, cq, h);
        st<u32>(ctx, cq + 4, t);
        snd_call(ctx, 0x0000196f, SndAbi::Thiscall, {self, cq, cq + 4});  // 0xa36d
        h = ld<u32>(ctx, cq);
        t = ld<u32>(ctx, cq + 4);
    }
    unsigned idx = (h + 1) & 0xfff;  // 0xa372-0xa37a
    for (; n; --n) {  // 0xa37f / 0xa385 / 0xa565 / 0xa56e
        u32 e = ld<u32>(ctx, self + 0x50 + 4 * idx);  // 0xa390
        if (e) {  // 0xa394 / 0xa396
            bool freeIt = true;
            u32 o = (ld<uint32_t>(ctx, e + (1) * 4));
            switch (ld<uint32_t>(ctx, e + (0) * 4) - 1) {  // 0xa39c-0xa3a8, table 0xa588
            case 0:  // kind 1 -> 0xa511
                break;
            case 1:  // kind 2 -> 0xa3af
                snd_call(ctx, 0x00001dd9, SndAbi::Thiscall, {o, 0}).eax;
                st<uint32_t>(ctx, o + 0xac, static_cast<uint32_t>(ld<uint32_t>(ctx, o + 0xac) & 0xffffffdfu));
                snd_vcall(ctx, o, 0xb8, SndAbi::Thiscall, {}).eax;
                break;
            case 2:  // kind 3 -> 0xa3d8
                c3fd_unlinkLocal(ctx, self, 0x40, o);
                snd_call(ctx, 0x00001dd9, SndAbi::Thiscall, {o, 0}).eax;
                st<uint32_t>(ctx, o + 0xac, static_cast<uint32_t>(ld<uint32_t>(ctx, o + 0xac) & 0xffffffdfu));
                snd_call(ctx, 0x0000246e, SndAbi::Thiscall, {o}).eax;
                break;
            case 3:  // kind 4 -> 0xa453
                if (!o) { freeIt = false; break; }  // 0xa456 / 0xa458
                if (ld<uint8_t>(ctx, o + 0xac) & 0x20) {  // 0xa45e / 0xa465
                    c3fd_unlinkLocal(ctx, self, 0x40, o);  // 0xa467-0xa4b9
                    st<uint8_t>(ctx, o + 0xac, static_cast<uint8_t>(ld<uint8_t>(ctx, o + 0xac) & 0xdf));  // 0xa4bc-0xa4c4
                }
                snd_vcall(ctx, o, 0, SndAbi::Thiscall, {1}).eax;  // 0xa4ca-0xa4d0
                break;
            case 6:  // kind 7 -> 0xa4d4
                if (!o) { freeIt = false; break; }
                snd_call(ctx, 0x00001186, SndAbi::Thiscall, {o}).eax;
                break;
            case 7:  // kind 8 -> 0xa4e2
                if (!o) { freeIt = false; break; }
                snd_call(ctx, 0x00001474, SndAbi::Thiscall, {o, ld<uint32_t>(ctx, e + (2) * 4)}).eax;
                snd_call(ctx, 0x000025c2, SndAbi::Thiscall, {o, ld<uint32_t>(ctx, e + (2) * 4)}).eax;
                break;
            case 8:  // kind 9 -> 0xa501
                if (!o) { freeIt = false; break; }
                snd_call(ctx, 0x00001474, SndAbi::Thiscall, {o, ld<uint32_t>(ctx, e + (2) * 4)}).eax;
                break;
            case 9:  // kind 10 -> 0xa523
                if (o) snd_call(ctx, 0x000023e7, SndAbi::Thiscall, {o, ld<uint32_t>(ctx, e + (2) * 4), ld<uint32_t>(ctx, e + (3) * 4)}).eax;
                freeIt = false;
                break;
            case 10:  // kind 11 -> 0xa539
                if (o) snd_call(ctx, 0x000011db, SndAbi::Thiscall, {o, ld<uint32_t>(ctx, e + (2) * 4), ld<uint32_t>(ctx, e + (3) * 4)}).eax;
                freeIt = false;
                break;
            default:  // kinds 5, 6 and above 11 -> 0xa54d
                freeIt = false;
                break;
            }
            if (freeIt) {  // 0xa511-0xa519
                snd_call(ctx, 0x00001816, SndAbi::Thiscall, {e}).eax;
                snd_delete(ctx, e);
            }
            st<uint32_t>(ctx, self + 0x50 + 4 * idx, 0);  // 0xa54d-0xa551
        }
        idx = (idx + 1) & 0xfff;  // 0xa555-0xa566
    }
    st<uint32_t>(ctx, self + 0x4050, t);  // 0xa575-0xa57a
    return 0;  // 0xa581
}
int g_negctl_Midi_Device__drainCommandQueue = 0;
void a_Midi_Device__drainCommandQueue(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Midi_Device__drainCommandQueue), Midi_Device__drainCommandQueue_body(ctx, a[0]))); }

// ---- Midi_Device::dispatchCommand (rva 0xa670): converted from c3fl.cpp MidiDispatch_re
constexpr uint32_t kIatPostMessageA = 0xb85a0;  // c3fl.cpp
uint32_t c3fl_u(Ctx &, u32 p) { return p; }
void c3fl_freeCommand(Ctx &ctx, u32 c) {
    snd_call(ctx, 0x00001816, SndAbi::Thiscall, {c}).eax;  // 0xa8cc / 0xa8ce
    snd_delete(ctx, c);  // 0xa8d3 / 0xa8d4
}
// The doubly linked node list {+0 head, +4 tail, +8 cursor, +0xc count} of nodes {+0 prev, +4 next, +8 object} that flushSoundList
// (0x8559-0x85af) and Midi_Device::dispatchCommand (0xa720-0xa775, 0xa7a5-0xa810) unlink an object from:
// the first node whose object is `obj` (searched from the head); prev->next = next or head = next; next->prev = prev with the
// cursor = next, or (no next) cursor = 0 and tail = prev; the node goes to operator delete and the count drops by one.
void c3fl_unlinkObject(Ctx &ctx, u32 list, u32 obj) {
    u32 n = ld<u32>(ctx, list);
    while (n && ld<u32>(ctx, n + 8) != obj) n = ld<u32>(ctx, n + 4);
    if (!n) return;
    u32 prev = ld<u32>(ctx, n);
    if (prev) st<u32>(ctx, prev + 4, ld<u32>(ctx, n + 4));
    else st<u32>(ctx, list, ld<u32>(ctx, n + 4));
    u32 next = ld<u32>(ctx, n + 4);
    if (next) {
        st<u32>(ctx, next, ld<u32>(ctx, n));
        st<u32>(ctx, list + 8, next);
    } else {
        st<u32>(ctx, list + 8, 0);
        st<u32>(ctx, list + 4, ld<u32>(ctx, n));
    }
    snd_delete(ctx, n);
    st<int32_t>(ctx, list + 0xc, static_cast<int32_t>(ld<int32_t>(ctx, list + 0xc) - 1));
}
uint32_t Midi_Device__dispatchCommand_body(Ctx &ctx, u32 self) {
  SndFrame fr_cells(ctx, 0x10);  // S4.2b: the cells [1] of the original's locals passed by address (U-1418)
    u32 d = self;
    unsigned readL = ld<uint32_t>(ctx, d + 0x4050);  // 0xa677 / 0xa685
    unsigned writeL = ld<uint32_t>(ctx, d + 0x4054);  // 0xa67d / 0xa689
    if (readL == writeL) return readL;  // 0xa683 / 0xa68d -> 0xa954
    unsigned n = (writeL - readL) & 0xfff;  // 0xa693 / 0xa695
    if (n) {
        const u32 cq = fr_cells.at(0x10);  // the slots of readL / writeL the callee takes by address (0xa69d-0xa6a9)
        st<u32>(ctx, cq, readL);
        st<u32>(ctx, cq + 4, writeL);
        snd_call(ctx, 0x0000196f, SndAbi::Thiscall, {d, cq, cq + 4});
        readL = ld<u32>(ctx, cq);
        writeL = ld<u32>(ctx, cq + 4);
    }
    unsigned idx = (readL + 1) & 0xfff;  // 0xa6ae-0xa6b6
    for (; n; --n, idx = (idx + 1) & 0xfff) {  // 0xa6c1 / 0xa929-0xa942
        u32 c = ld<u32>(ctx, d + 0x50 + idx * 4);  // 0xa6cc
        if (!c) continue;  // 0xa6d0 / 0xa6d2
        u32 ch = ld<u32>(ctx, c + 4);
        switch (ld<uint32_t>(ctx, c)) {  // 0xa6d8-0xa6e4
        case 1:
            if (ch && !(ld<uint8_t>(ctx, ch + 0xac) & 0x20)) {  // 0xa6eb-0xa6fd
                st<uint32_t>(ctx, ch + 0xac, static_cast<uint32_t>(ld<uint32_t>(ctx, ch + 0xac) | 0x20));  // 0xa703-0xa710
                snd_call(ctx, 0x000026c1, SndAbi::Thiscall, {d + 0x40, c3fl_u(ctx, ch)}).eax;  // 0xa709-0xa716
            }
            c3fl_freeCommand(ctx, c);
            break;
        case 2:
            c3fl_unlinkObject(ctx, d + 0x40, ch);  // 0xa720-0xa775
            snd_call(ctx, 0x00001dd9, SndAbi::Thiscall, {ch, 0}).eax;  // 0xa778-0xa77b
            st<uint32_t>(ctx, ch + 0xac, static_cast<uint32_t>(ld<uint32_t>(ctx, ch + 0xac) & 0xffffffdfu));  // 0xa780-0xa78b
            snd_call(ctx, 0x0000190b, SndAbi::Thiscall, {ch}).eax;  // 0xa791
            c3fl_freeCommand(ctx, c);
            break;
        case 4:
            if (!ch) break;  // 0xa7b6 / 0xa7b8
  // fall through
        case 3: {
            c3fl_unlinkObject(ctx, d + 0x40, ch);  // 0xa79b-0xa810 / 0xa7be-0xa810
            snd_call(ctx, 0x00001dd9, SndAbi::Thiscall, {ch, 0}).eax;  // 0xa813-0xa816
            st<uint32_t>(ctx, ch + 0xac, static_cast<uint32_t>(ld<uint32_t>(ctx, ch + 0xac) & 0xffffffdfu));  // 0xa81b-0xa824
            u32 ch2 = ld<u32>(ctx, c + 4);  // 0xa82a
            if (!ch2) break;  // 0xa82d / 0xa82f
            st<uint32_t>(ctx, ch2 + 0x214, static_cast<uint32_t>(ld<uint32_t>(ctx, ch2 + 0x214) | 0x40));  // 0xa835-0xa83e
            u32 eng = ld<u32>(ctx, snd_obj(ctx, &SoundState::g_soundEngine).va());  // 0xa844
            if (eng && ((ld<uint32_t>(ctx, eng + 0x1a4) >> 5) & 1))  // 0xa84a-0xa85a
                snd_import(ctx, kIatPostMessageA, SndAbi::Stdcall, {ld<uint32_t>(ctx, ch2 + 0x210), 0x7ef, 0, c3fl_u(ctx, c)}).eax;  // 0xa85c-0xa86a
            else
                snd_call(ctx, 0x000011d6, SndAbi::Thiscall, {snd_arr(ctx, &SoundState::g_soundThreadState).va(), 0x7ef, 0, c3fl_u(ctx, c)}).eax;  // 0xa875-0xa881
            break;
        }
        case 7:
            if (!ch) break;  // 0xa88e / 0xa890
            snd_call(ctx, 0x00001186, SndAbi::Thiscall, {ch}).eax;  // 0xa896
            c3fl_freeCommand(ctx, c);
            break;
        case 8:
            if (!ch) break;  // 0xa8a0 / 0xa8a2
            snd_call(ctx, 0x00001474, SndAbi::Thiscall, {ch, ld<uint32_t>(ctx, c + 8)}).eax;  // 0xa8a4-0xa8aa
            snd_call(ctx, 0x000025c2, SndAbi::Thiscall, {ch, ld<uint32_t>(ctx, c + 8)}).eax;  // 0xa8af-0xa8b5
            c3fl_freeCommand(ctx, c);
            break;
        case 9:
            if (!ch) break;  // 0xa8bf / 0xa8c1
            snd_call(ctx, 0x00001474, SndAbi::Thiscall, {ch, ld<uint32_t>(ctx, c + 8)}).eax;  // 0xa8c3-0xa8c7
            c3fl_freeCommand(ctx, c);
            break;
        case 10:
            if (!ch) break;  // 0xa8e1 / 0xa8e3
            snd_call(ctx, 0x000023e7, SndAbi::Thiscall, {ch, ld<uint32_t>(ctx, c + 8), ld<uint32_t>(ctx, c + 0xc)}).eax;  // 0xa8e5-0xa8ed
            break;
        case 11:
            if (!ch) break;  // 0xa8f7 / 0xa8f9
            snd_call(ctx, 0x000011db, SndAbi::Thiscall, {ch, ld<uint32_t>(ctx, c + 8), ld<uint32_t>(ctx, c + 0xc)}).eax;  // 0xa8fb-0xa903
            ch = ld<u32>(ctx, c + 4);  // 0xa908
  // fall through
        case 12:
            if (!ch) break;  // 0xa90b / 0xa90d
            snd_call(ctx, 0x00001037, SndAbi::Thiscall, {ch, ld<uint32_t>(ctx, c + 8), ld<uint32_t>(ctx, c + 0xc) != 0 ? 1u : 0u}).eax;  // 0xa90f-0xa91c
            break;
        default:  // 0xa6de -> 0xa921, kinds 5 / 6 the same
            break;
        }
        st<u32>(ctx, d + 0x50 + idx * 4, 0);  // 0xa921 / 0xa925
    }
    st<uint32_t>(ctx, d + 0x4050, writeL);  // 0xa94e
    return writeL;  // 0xa949
}
int g_negctl_Midi_Device__dispatchCommand = 0;
void a_Midi_Device__dispatchCommand(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Midi_Device__dispatchCommand), Midi_Device__dispatchCommand_body(ctx, a[0]))); }

// ---- Midi_Device::scanCommandRing (rva 0xaaf0): hand-fixed conversion of c3fd.cpp MidiScan_re
// CommandQueue::at 0x88c0 through its thunk 0x2257: __thiscall (ring, index), returns ring[index & 0xfff].
u32 c3fd_at(Ctx &ctx, u32 ring, uint32_t idx) { return snd_call(ctx, 0x00002257, SndAbi::Thiscall, {ring, idx}).eax; }
// pairClear 0x8840 (thunk 0x1816) on the entry, then operator delete, when the entry is non-NULL.
void c3fd_freeEntry(Ctx &ctx, u32 e) {
  if (!e) return;
  snd_call(ctx, 0x00001816, SndAbi::Thiscall, {e});
  snd_delete(ctx, e);
}
// The shared body of the two scanCommandRing rows. ringOff is the ring's offset in the device (+0x50 Midi, +0x1d8 Wave), cmpAll
// whether entries also compare their dwords +8 and +0xc, flagOff / flagBit the byte tested on the entry's object.
// i walks from *first, j from i + 1; the walk of j for one i ends when j reaches *last or the entry at i is gone; i ends at
// *last. For two live entries with the same object (+4): equal kinds (and, for Midi, equal +8 and +0xc) delete the j entry;
// kinds (1, 2) delete the i entry when its object has the flag bit, else both; kinds (2, 1) delete the i entry when the object
// is non-NULL without the flag bit, else both; anything else keeps both.
uint32_t c3fd_scanRing(Ctx &ctx, u32 self, u32 first, u32 last, int32_t ringOff, bool cmpAll, int32_t flagOff, uint8_t flagBit) {
  const u32 ring = self + static_cast<u32>(ringOff);
  u32 i = ld<u32>(ctx, first);
  u32 j = (i + 1) & 0xfff;
  u32 nextI = j;
  if (i == ld<u32>(ctx, last)) return 0;
  for (;;) {
    if (j == ld<u32>(ctx, last)) return 0;
    if (c3fd_at(ctx, ring, i)) {
      do {
        if (j == ld<u32>(ctx, last)) break;
        if (c3fd_at(ctx, ring, j)) {
          const u32 b = c3fd_at(ctx, ring, j);
          const u32 a = c3fd_at(ctx, ring, i);
          if (ld<u32>(ctx, a + 4) == ld<u32>(ctx, b + 4)) {
            if (ld<u32>(ctx, a) == ld<u32>(ctx, b) &&
                (!cmpAll || (ld<u32>(ctx, a + 8) == ld<u32>(ctx, b + 8) && ld<u32>(ctx, a + 0xc) == ld<u32>(ctx, b + 0xc)))) {
              c3fd_freeEntry(ctx, c3fd_at(ctx, ring, j));
              st<u32>(ctx, ring + 4 * j, 0);
            } else if (ld<u32>(ctx, c3fd_at(ctx, ring, i)) == 1 && ld<u32>(ctx, c3fd_at(ctx, ring, j)) == 2) {
              const u32 o = ld<u32>(ctx, c3fd_at(ctx, ring, i) + 4);
              if (o && (ld<uint8_t>(ctx, o + static_cast<u32>(flagOff)) & flagBit)) {
                c3fd_freeEntry(ctx, c3fd_at(ctx, ring, i));
                st<u32>(ctx, ring + 4 * i, 0);
              } else {
                c3fd_freeEntry(ctx, c3fd_at(ctx, ring, i));
                st<u32>(ctx, ring + 4 * i, 0);
                c3fd_freeEntry(ctx, c3fd_at(ctx, ring, j));
                st<u32>(ctx, ring + 4 * j, 0);
              }
            } else if (ld<u32>(ctx, c3fd_at(ctx, ring, i)) == 2 && ld<u32>(ctx, c3fd_at(ctx, ring, j)) == 1) {
              const u32 o = ld<u32>(ctx, c3fd_at(ctx, ring, i) + 4);
              if (o && !(ld<uint8_t>(ctx, o + static_cast<u32>(flagOff)) & flagBit)) {
                c3fd_freeEntry(ctx, c3fd_at(ctx, ring, i));
                st<u32>(ctx, ring + 4 * i, 0);
              } else {
                c3fd_freeEntry(ctx, c3fd_at(ctx, ring, i));
                st<u32>(ctx, ring + 4 * i, 0);
                c3fd_freeEntry(ctx, c3fd_at(ctx, ring, j));
                st<u32>(ctx, ring + 4 * j, 0);
              }
            }
          }
        }
        j = (j + 1) & 0xfff;
      } while (c3fd_at(ctx, ring, i));
    }
    i = nextI;
    j = (i + 1) & 0xfff;
    nextI = j;
    if (i == ld<u32>(ctx, last)) return 0;
  }
}
uint32_t Midi_Device__scanCommandRing_body(Ctx &ctx, u32 self, u32 first, u32 last) {
  return c3fd_scanRing(ctx, self, first, last, 0x50, true, 0xac, 0x20);
}
int g_negctl_Midi_Device__scanCommandRing = 0;
void a_Midi_Device__scanCommandRing(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Midi_Device__scanCommandRing), Midi_Device__scanCommandRing_body(ctx, a[0], a[1], a[2]))); }

// ---- Seq_freeChannelObjects (rva 0x1abd0): hand-fixed conversion of c3dj.cpp SeqFreeChan_re
void Seq_freeChannelObjects_body(Ctx &ctx, u32 self) {
  int esi = 0;
  do {
    const u32 slot = self + 4 + 4 * static_cast<u32>(esi);  // 0x1abd6 / 0x1abda
    const u32 obj = ld<u32>(ctx, slot);
    if (obj != 0) {  // 0x1abde / 0x1abe0
      u32 eax = slot;
      u32 edx;
      do {
        st<u32>(ctx, eax, 0);  // 0x1abe2
        edx = ld<u32>(ctx, eax + 4);  // 0x1abe8
        eax += 4;  // 0x1abeb
        esi += 1;  // 0x1abee
      } while (edx == obj);  // 0x1abef / 0x1abf1
      esi -= 1;  // 0x1abf7
      snd_vcall(ctx, obj, 0, SndAbi::Thiscall, {1});  // 0x1abf8
    }
    esi += 1;  // 0x1abfa
  } while (esi < 0x80);  // 0x1abfb / 0x1ac01
  const u32 buf = ld<u32>(ctx, self);  // 0x1ac03
  if (buf != 0) snd_delete(ctx, buf);  // 0x1ac07 / 0x1ac0a
  st<u32>(ctx, self, 0);  // 0x1ac12
}
int g_negctl_Seq_freeChannelObjects = 0;
void a_Seq_freeChannelObjects(Ctx &ctx, const uint32_t *a, Ret &) { Seq_freeChannelObjects_body(ctx, a[0]); neg_mem(ctx, NEG(Seq_freeChannelObjects), a[0]); }

// ---- Midi_advanceMsgRing (rva 0x1adc0): converted from c3ar.cpp MidiAdvanceRing_re
void Midi_advanceMsgRing_body(Ctx &ctx, u32 self) {
    unsigned next = ld<uint32_t>(ctx, self + 0x20c) + 1u;
    unsigned end = ld<uint32_t>(ctx, self + 0x204);
    st<uint32_t>(ctx, self + 0x20c, next);
    if (next > end) st<uint32_t>(ctx, self + 0x20c, ld<uint32_t>(ctx, self + 0x208));
}
int g_negctl_Midi_advanceMsgRing = 0;
void a_Midi_advanceMsgRing(Ctx &ctx, const uint32_t *a, Ret &) { Midi_advanceMsgRing_body(ctx, a[0]); neg_mem(ctx, NEG(Midi_advanceMsgRing), a[0] + 0x20c); }

// ---- Seq_setPlayRange (rva 0x1ae00): converted from c3ar.cpp SeqSetPlayRange_re
void Seq_setPlayRange_body(Ctx &ctx, u32 self, uint32_t a, uint32_t b) {
    unsigned lo = a, hi = b;
    if (lo > hi) {
        lo = b;
        hi = a;
    }
    st<uint32_t>(ctx, self + 0x208, lo);
    st<uint32_t>(ctx, self + 0x204, hi);
    st<uint32_t>(ctx, self + 0x214, ld<uint32_t>(ctx, self + 0x214) | 1u);
    st<uint32_t>(ctx, self + 0x20c, lo);
}
int g_negctl_Seq_setPlayRange = 0;
void a_Seq_setPlayRange(Ctx &ctx, const uint32_t *a, Ret &) { Seq_setPlayRange_body(ctx, a[0], a[1], a[2]); neg_mem(ctx, NEG(Seq_setPlayRange), a[0] + 0x20c); }

// ---- Seq_firstTrackData (rva 0x1bb80): converted from c3an.cpp SeqFirstTrackData_re
uint32_t Seq_firstTrackData_body(Ctx &ctx, u32 self) {
    u32 node = ld<u32>(ctx, self + 0xc);
    st<u32>(ctx, self + 0x14, node);
    if (node == 0) return 0;
    return ld<uint32_t>(ctx, node + 8);
}
int g_negctl_Seq_firstTrackData = 0;
void a_Seq_firstTrackData(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Seq_firstTrackData), Seq_firstTrackData_body(ctx, a[0]))); }

// ---- Midi_resetFileState (rva 0x1d4a0): converted from c3dh.cpp MidiReset_re
constexpr uint32_t kIatMmioClose = 0xb8670;  // c3dh.cpp
void Midi_resetFileState_body(Ctx &ctx, u32 self) {
    unsigned h = ld<uint32_t>(ctx, self);  // 0x1d4a7
    if (h != 0) {  // 0x1d4ab
        snd_import(ctx, kIatMmioClose, SndAbi::Stdcall, {h, 0}).eax;  // 0x1d4af
        st<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_openMmioCount).va(), ld<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_openMmioCount).va()) - 1);  // 0x1d4b5
    }
    st<uint32_t>(ctx, self, 0);  // 0x1d4c5
    for (int i = 0; i < 0x12; i++) st<uint32_t>(ctx, self + 4 + 4 * i, 0);  // 0x1d4c2-0x1d4c7
    st<uint32_t>(ctx, self + 0x4c, 0);  // 0x1d4d1
    st<uint32_t>(ctx, self + 0x160, 0);  // 0x1d4d3
    st<uint32_t>(ctx, self + 0x164, 0);  // 0x1d4d9
    st<uint32_t>(ctx, self + 0x168, 0);  // 0x1d4df
    st<uint32_t>(ctx, self + 0x16c, 0);  // 0x1d4e5
    st<uint32_t>(ctx, self + 0x170, 0);  // 0x1d4eb
    st<uint32_t>(ctx, self + 0x178, 0x10000);  // 0x1d4f1
    st<uint32_t>(ctx, self + 0x17c, 0x10000);  // 0x1d4f7
    u32 timer = gref(ctx, 0xb49ec);  // 0x1d4fd (g_soundTimer)
    if (timer != 0) {  // 0x1d504
        unsigned period = ld<uint32_t>(ctx, timer);  // 0x1d506
        if (period >= 0x3e8) {  // 0x1d50e
            st<uint32_t>(ctx, self + 0x174, 0x10000);  // 0x1d529
        } else {
            unsigned q = udiv32(0x3e8, period);  // 0x1d517
            st<uint32_t>(ctx, self + 0x174, udiv32(0x10000, q));  // 0x1d51f / 0x1d521
        }
    }
    st<uint32_t>(ctx, self + 0x180, 0);  // 0x1d52f
    st<uint32_t>(ctx, self + 0x184, 0);  // 0x1d535
    st<uint32_t>(ctx, self + 0x188, 0);  // 0x1d53b
    st<uint32_t>(ctx, self + 0x18c, 0);  // 0x1d541
    st<uint32_t>(ctx, self + 0x190, 0);  // 0x1d547
    st<uint32_t>(ctx, self + 0x198, 0);  // 0x1d54d
}
int g_negctl_Midi_resetFileState = 0;
void a_Midi_resetFileState(Ctx &ctx, const uint32_t *a, Ret &) { Midi_resetFileState_body(ctx, a[0]); neg_mem(ctx, NEG(Midi_resetFileState), a[0] + 0x4c); }

// ---- Seq_listGetAt (rva 0x1d780): hand-fixed conversion of c3au.cpp SeqListGetAt_re
int32_t Seq_listGetAt_body(Ctx &ctx, u32 self, u32 out, uint32_t count) {
  u32 node = ld<u32>(ctx, self + 0x19c);
  st<u32>(ctx, self + 0x1a4, node);
  st<u32>(ctx, out, node ? ld<u32>(ctx, node + 8) : 0);
  if (count != 0) {
    for (unsigned i = 0;;) {
      u32 payload = 0;
      node = ld<u32>(ctx, self + 0x1a4);
      if (node) {
        node = ld<u32>(ctx, node + 4);
        st<u32>(ctx, self + 0x1a4, node);
        if (node) payload = ld<u32>(ctx, node + 8);
      }
      st<u32>(ctx, out, payload);
      if (!payload) break;
      if (++i >= count) break;
    }
  }
  return ld<u32>(ctx, out) != 0 ? 0 : 0x24;
}
int g_negctl_Seq_listGetAt = 0;
void a_Seq_listGetAt(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_i32(static_cast<int32_t>(neg_u32(NEG(Seq_listGetAt), static_cast<uint32_t>(Seq_listGetAt_body(ctx, a[0], a[1], a[2]))))); }

// ---- Midi_Device::loadFile (rva 0x1d810): hand-fixed conversion of c3fn.cpp MidiLoadFile_re
constexpr uint32_t kMmioStringToFourcc = 0xb865c, kMmioOpen = 0xb8660, kMmioRead = 0xb8664, kMmioGetInfo = 0xb8668, kMmioClose = 0xb8670;  // c3fn.cpp
constexpr uint32_t kStrMThd = 0x63d90, kStrMTrk = 0x63e08;  // c3fn.cpp
uint32_t c3fn_fourcc(Ctx &ctx, u32 s, uint32_t f) { return snd_import(ctx, kMmioStringToFourcc, SndAbi::Stdcall, {s, f}).eax; }
u32 c3fn_mmioOpen(Ctx &ctx, u32 n, u32 i, uint32_t f) { return snd_import(ctx, kMmioOpen, SndAbi::Stdcall, {n, i, f}).eax; }
int32_t c3fn_mmioRead(Ctx &ctx, u32 h, u32 p, int32_t n) { return static_cast<int32_t>(snd_import(ctx, kMmioRead, SndAbi::Stdcall, {h, p, static_cast<uint32_t>(n)}).eax); }
uint32_t c3fn_be32(Ctx &ctx, u32 b) {
  return (uint32_t(ld<uint8_t>(ctx, b + 0)) << 24) | (uint32_t(ld<uint8_t>(ctx, b + 1)) << 16) | (uint32_t(ld<uint8_t>(ctx, b + 2)) << 8) | ld<uint8_t>(ctx, b + 3);
}
// mmioRead of n bytes into the device field `off`, then the field = those n bytes as a big-endian number (n = 4 or 2; for 2
// the word is zero-extended into the dword: `and eax, 0xffff`)
void c3fn_readBe(Ctx &ctx, u32 self, u32 h, int32_t off, int32_t n) {
  const u32 b = self + static_cast<u32>(off);
  c3fn_mmioRead(ctx, h, b, n);
  st<uint32_t>(ctx, b, n == 4 ? c3fn_be32(ctx, b) : ((uint32_t(ld<uint8_t>(ctx, b + 0)) << 8) | ld<uint8_t>(ctx, b + 1)));
}
// the header after the tag (loadFile 0x1d900-0x1d9ed, loadFromMemory 0x1dd90-0x1de70): the chunk length
// into +0x150, +0x4c bit 0 set, then format +0x154, track count +0x158 and division +0x15c, each a big-endian word
void c3fn_readMidiHeader(Ctx &ctx, u32 self, u32 h) {
  c3fn_readBe(ctx, self, h, 0x150, 4);  // 0x1d91a / 0x1dd9d
  st<uint32_t>(ctx, self + 0x4c, static_cast<uint32_t>(ld<uint32_t>(ctx, self + 0x4c) | 1));  // 0x1d941 / 0x1ddc4
  c3fn_readBe(ctx, self, h, 0x154, 2);  // 0x1d951 / 0x1ddd4
  c3fn_readBe(ctx, self, h, 0x158, 2);  // 0x1d985 / 0x1de08
  c3fn_readBe(ctx, self, h, 0x15c, 2);  // 0x1d9be / 0x1de41
}
// a new track reader: operator new 0x464, CtrlObject2_ctor (thunk 0x1e74), the reader vtable at +0, then
// +0x34 / +0x38 = 0, 0x80 dwords from +0x58 zeroed, bit 3 of byte +0x38 cleared, +0x2c, +0x3c, +0x4c = 0, +0x48 = 0x10,
// byte +0x30 = 0, +0x50, +0x54, +0x45c, +0x460, +0x458 = 0 (loadFile 0x1da09-0x1da7a). NULL when new fails.
u32 c3fn_newTrack(Ctx &ctx) {
  const u32 t = snd_new(ctx, 0x464);
  if (!t) return 0;
  snd_call(ctx, 0x00001e74, SndAbi::Thiscall, {t});
  st<u32>(ctx, t, snd_va(ctx, snd_rva::vt_MidiReader));
  st<uint32_t>(ctx, t + 0x34, 0);
  st<uint32_t>(ctx, t + 0x38, 0);
  zero_dwords(ctx, t + 0x58, 0x80);
  st<uint8_t>(ctx, t + 0x38, static_cast<uint8_t>(ld<uint8_t>(ctx, t + 0x38) & 0xf7));
  st<uint32_t>(ctx, t + 0x2c, 0);
  st<uint32_t>(ctx, t + 0x3c, 0);
  st<uint32_t>(ctx, t + 0x4c, 0);
  st<uint32_t>(ctx, t + 0x48, 0x10);
  st<uint8_t>(ctx, t + 0x30, 0);
  st<uint32_t>(ctx, t + 0x50, 0);
  st<uint32_t>(ctx, t + 0x54, 0);
  st<uint32_t>(ctx, t + 0x45c, 0);
  st<uint32_t>(ctx, t + 0x460, 0);
  st<uint32_t>(ctx, t + 0x458, 0);
  return t;
}
// a 0xc-byte node {0, 0, t} appended to the reader list +0x19c (head) / +0x1a0 (tail), count +0x1a8 raised
// (loadFile 0x1db16-0x1db5c); a failed new leaves the node NULL (the stores through it then fault)
void c3fn_appendTrack(Ctx &ctx, u32 self, u32 t) {
  const u32 n = snd_new(ctx, 0xc);
  if (n) {
    st<u32>(ctx, n, 0);
    st<u32>(ctx, n + 4, 0);
    st<u32>(ctx, n + 8, t);
  }
  if (ld<u32>(ctx, self + 0x1a0)) {
    st<u32>(ctx, ld<u32>(ctx, self + 0x1a0) + 4, n);
    st<u32>(ctx, n, ld<u32>(ctx, self + 0x1a0));
  } else {
    st<u32>(ctx, self + 0x19c, n);
  }
  st<u32>(ctx, self + 0x1a0, n);
  st<u32>(ctx, self + 0x1a8, ld<u32>(ctx, self + 0x1a8) + 1);
}
uint32_t c3fn_mmioClose(Ctx &ctx, u32 h, uint32_t f) { return snd_import(ctx, kMmioClose, SndAbi::Stdcall, {h, f}).eax; }
uint32_t Midi_Device__loadFile_body(Ctx &ctx, u32 self, u32 namep) {
  SndFrame fr_cells(ctx, 0x10);  // S4.2b: the cells [2] of the original's locals passed by address (U-1418)
  const unsigned mthd = c3fn_fourcc(ctx, snd_va(ctx, kStrMThd), 0);  // 0x1d83c
  c3fn_fourcc(ctx, snd_va(ctx, kStrMTrk), 0);  // 0x1d848 (result unused)
  if (ld<uint8_t>(ctx, self + 0x4c) & 1) {  // 0x1d84a / 0x1d84e
    const unsigned r = snd_call(ctx, 0x00001014, SndAbi::Thiscall, {self}).eax;  // 0x1d852
    if (r) return r;  // 0x1d859
  }
  st<u32>(ctx, self, 0);  // 0x1d869
  zero_dwords(ctx, self + 4, 0x12);  // 0x1d873
  for (u32 off = 0x160; off <= 0x16c; off += 4) st<uint32_t>(ctx, self + off, 0);  // 0x1d87c-0x1d88e
  vstrcpy(ctx.mem, self + 0x50, namep);  // 0x1d894-0x1d8ad
  const u32 h = c3fn_mmioOpen(ctx, self + 0x50, 0, 0);  // 0x1d8af
  st<u32>(ctx, self, h);  // 0x1d8b7
  if (!h) {  // 0x1d8ba
    zero_dwords(ctx, self + 4, 0x12);  // 0x1d8c7
    return 8;  // 0x1d8c9
  }
  const u32 cell = fr_cells.at(0x10);  // the tag slot (+0) and the 8-byte chunk header (+4)
  c3fn_mmioRead(ctx, h, cell, 4);  // 0x1d8e1
  if (ld<u32>(ctx, cell) != mthd) {  // 0x1d8eb / 0x1d8ed
    snd_call(ctx, 0x0000194c, SndAbi::Thiscall, {self});  // 0x1d8f1
    return 0x12;  // 0x1d8f6
  }
  st<int32_t>(ctx, snd_obj(ctx, &SoundState::g_openMmioCount).va(), ld<int32_t>(ctx, snd_obj(ctx, &SoundState::g_openMmioCount).va()) + 1);  // 0x1d900-0x1d90f
  c3fn_readMidiHeader(ctx, self, ld<u32>(ctx, self));
  u32 prev = 0;  // [esp+0x10] 0x1d9de
  st<uint32_t>(ctx, self + 0x1ac, 0);  // 0x1d9e7
  for (unsigned i = 0; i < ld<uint32_t>(ctx, self + 0x158); i++) {  // 0x1d9f5 / 0x1db99
    const u32 t = c3fn_newTrack(ctx);  // 0x1da09-0x1da7a
    c3fn_mmioRead(ctx, ld<u32>(ctx, self), cell + 4, 8);  // 0x1da97
    const unsigned size = c3fn_be32(ctx, cell + 8);  // 0x1da99-0x1dab4
    const u32 buf = snd_new(ctx, size);  // 0x1dabd
    if (ld<u32>(ctx, t + 0x3c) && (ld<uint8_t>(ctx, t + 0x34) & 4)) {  // 0x1daca / 0x1dace
      snd_delete(ctx, ld<u32>(ctx, t + 0x3c));  // 0x1dad5
      st<u32>(ctx, t + 0x3c, 0);  // 0x1dadd
    }
    st<u32>(ctx, t + 0x3c, buf);  // 0x1dae2
    st<u32>(ctx, t + 0x2c, buf);  // 0x1dae5
    if (buf) snd_call(ctx, 0x0000215d, SndAbi::Thiscall, {t});  // 0x1dae8 / 0x1daec
    const u32 data = ld<u32>(ctx, t + 0x3c);  // 0x1daf4
    st<uint32_t>(ctx, t + 0x40, 0);  // 0x1daf9
    st<uint32_t>(ctx, t + 0x34, static_cast<uint32_t>(ld<uint32_t>(ctx, t + 0x34) | 4));  // 0x1daf7 / 0x1dafc
    st<uint32_t>(ctx, t + 0x44, size);  // 0x1db03
    c3fn_mmioRead(ctx, ld<u32>(ctx, self), data, static_cast<int32_t>(size));  // 0x1db10
    c3fn_appendTrack(ctx, self, t);  // 0x1db16-0x1db5c
    snd_call(ctx, 0x00001ca3, SndAbi::Thiscall, {t});  // 0x1db62
    snd_call(ctx, 0x0000215d, SndAbi::Thiscall, {t});  // 0x1db69
    if (prev && ld<uint32_t>(ctx, t + 0x54) > ld<uint32_t>(ctx, prev + 0x54))  // 0x1db74 / 0x1db7e
      st<uint32_t>(ctx, self + 0x1ac, i);  // 0x1db84
    prev = t;  // 0x1db95
  }
  c3fn_mmioClose(ctx, ld<u32>(ctx, self), 0);  // 0x1dbaa
  st<int32_t>(ctx, snd_obj(ctx, &SoundState::g_openMmioCount).va(), ld<int32_t>(ctx, snd_obj(ctx, &SoundState::g_openMmioCount).va()) - 1);  // 0x1dbb0-0x1dbb6
  st<u32>(ctx, self, 0);  // 0x1dbbb
  return 0;  // 0x1dbbe
}
int g_negctl_Midi_Device__loadFile = 0;
void a_Midi_Device__loadFile(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Midi_Device__loadFile), Midi_Device__loadFile_body(ctx, a[0], a[1]))); }

// ---- Midi_Device::loadFromMemory (rva 0x1dce0): hand-fixed conversion of c3fn.cpp MidiLoadMem_re
uint32_t c3fn_mmioGetInfo(Ctx &ctx, u32 h, u32 i, uint32_t f) { return snd_import(ctx, kMmioGetInfo, SndAbi::Stdcall, {h, i, f}).eax; }
uint32_t Midi_Device__loadFromMemory_body(Ctx &ctx, u32 self, u32 h) {
  SndFrame fr_cells(ctx, 0x10);  // S4.2b: the cells [3] of the original's locals passed by address (U-1418)
  const unsigned mthd = c3fn_fourcc(ctx, snd_va(ctx, kStrMThd), 0);  // 0x1dd0c
  c3fn_fourcc(ctx, snd_va(ctx, kStrMTrk), 0);  // 0x1dd16 (result unused)
  if (ld<uint8_t>(ctx, self + 0x4c) & 1) {  // 0x1dd18 / 0x1dd1c
    const unsigned r = snd_call(ctx, 0x00001014, SndAbi::Thiscall, {self}).eax;  // 0x1dd20
    if (r) return r;  // 0x1dd27
  }
  st<u32>(ctx, self, h);  // 0x1dd34
  zero_dwords(ctx, self + 4, 0x12);  // 0x1dd40
  for (u32 off = 0x160; off <= 0x16c; off += 4) st<uint32_t>(ctx, self + off, 0);  // 0x1dd48-0x1dd5a
  c3fn_mmioGetInfo(ctx, ld<u32>(ctx, self), self + 4, 0);  // 0x1dd60
  const u32 cell = fr_cells.at(0x10);  // the tag slot (+0) and the 8-byte chunk header (+4)
  c3fn_mmioRead(ctx, ld<u32>(ctx, self), cell, 4);  // 0x1dd77
  if (ld<u32>(ctx, cell) != mthd) {  // 0x1dd79 / 0x1dd7d
    snd_call(ctx, 0x0000194c, SndAbi::Thiscall, {self});  // 0x1dd81
    return 0x12;  // 0x1dd86
  }
  c3fn_readMidiHeader(ctx, self, ld<u32>(ctx, self));
  u32 best = 0;  // [esp+0x10] 0x1de61
  st<uint32_t>(ctx, self + 0x1ac, 0);  // 0x1de6a
  for (unsigned i = 0; i < ld<uint32_t>(ctx, self + 0x158); i++) {  // 0x1de78 / 0x1e013
    const u32 t = c3fn_newTrack(ctx);  // 0x1de8c-0x1defd
    c3fn_mmioRead(ctx, ld<u32>(ctx, self), cell + 4, 8);  // 0x1df1a
    const unsigned size = c3fn_be32(ctx, cell + 8);  // 0x1df1c-0x1df37
    const u32 buf = snd_new(ctx, size);  // 0x1df40
    st<uint32_t>(ctx, t + 0x34, static_cast<uint32_t>(ld<uint32_t>(ctx, t + 0x34) | 4));  // 0x1df45-0x1df50
    st<uint32_t>(ctx, t + 0x44, size);  // 0x1df57
    c3fn_mmioRead(ctx, ld<u32>(ctx, self), buf, static_cast<int32_t>(size));  // 0x1df64
    if (ld<u32>(ctx, t + 0x3c) && (ld<uint8_t>(ctx, t + 0x34) & 4)) {  // 0x1df6d / 0x1df71
      snd_delete(ctx, ld<u32>(ctx, t + 0x3c));  // 0x1df78
      st<u32>(ctx, t + 0x3c, 0);  // 0x1df80
    }
    st<u32>(ctx, t + 0x3c, buf);  // 0x1df85
    st<u32>(ctx, t + 0x2c, buf);  // 0x1df88
    if (buf) snd_call(ctx, 0x0000215d, SndAbi::Thiscall, {t});  // 0x1df8b / 0x1df8f
    st<uint32_t>(ctx, t + 0x40, 0);  // 0x1df96
    c3fn_appendTrack(ctx, self, t);  // 0x1df94-0x1dfdd
    snd_call(ctx, 0x00001ca3, SndAbi::Thiscall, {t});  // 0x1dfe3
    if (!best || ld<uint32_t>(ctx, t + 0x54) > ld<uint32_t>(ctx, best + 0x54)) {  // 0x1dfee / 0x1dff8
      best = t;  // 0x1dffe
      st<uint32_t>(ctx, self + 0x1ac, i);  // 0x1e002
    }
  }
  st<u32>(ctx, self, 0);  // 0x1e01f
  return 0;  // 0x1e022
}
int g_negctl_Midi_Device__loadFromMemory = 0;
void a_Midi_Device__loadFromMemory(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Midi_Device__loadFromMemory), Midi_Device__loadFromMemory_body(ctx, a[0], a[1]))); }

// ---- Track_resetReadState (rva 0x1e230): converted from c3au.cpp TrackResetRead_re
void Track_resetReadState_body(Ctx &ctx, u32 self) {
    unsigned old = ld<uint32_t>(ctx, self + 0x4c);
    st<uint32_t>(ctx, self + 0x4c, 0);  // the store at 0x1e233, overwritten below
    st<uint32_t>(ctx, self + 0x4c, ((old >> 1) & 1u) << 1);
    st<uint32_t>(ctx, self + 0x178, ld<uint32_t>(ctx, self + 0x17c));
    int v = ld<int32_t>(ctx, self + 0x180);
    if (v < -0x40) {
        st<int32_t>(ctx, self + 0x180, -0x40);
        return;
    }
    if (v > 0x3f) v = 0x3f;
    st<int32_t>(ctx, self + 0x180, v);
}
int g_negctl_Track_resetReadState = 0;
void a_Track_resetReadState(Ctx &ctx, const uint32_t *a, Ret &) { Track_resetReadState_body(ctx, a[0]); neg_mem(ctx, NEG(Track_resetReadState), a[0] + 0x4c); }

// ---- Seq_dtor (rva 0x1e2b0): hand-fixed conversion of c3fd.cpp SeqDtor_re
void Seq_dtor_body(Ctx &ctx, u32 self) {
  if (ld<uint8_t>(ctx, self + 0x4c) & 1) snd_call(ctx, 0x00001014, SndAbi::Thiscall, {self});  // 0x1e2d4-0x1e2dc
  u32 n = ld<u32>(ctx, self + 0x19c);  // 0x1e2e1
  while (n) {  // 0x1e2ef / 0x1e333
    const u32 next = ld<u32>(ctx, n + 4);  // 0x1e2f3
    if (next) {
      st<u32>(ctx, next, 0);  // 0x1e2fa
      st<u32>(ctx, self + 0x19c, next);  // 0x1e2fc
    } else {
      st<u32>(ctx, self + 0x1a0, 0);  // 0x1e304
      st<u32>(ctx, self + 0x19c, 0);  // 0x1e30a
    }
    const u32 payload = ld<u32>(ctx, n + 8);  // 0x1e310
    snd_delete(ctx, n);  // 0x1e314
    st<int32_t>(ctx, self + 0x1a8, sub32(ld<int32_t>(ctx, self + 0x1a8), 1));  // 0x1e319-0x1e325
    if (!payload) break;  // 0x1e323 / 0x1e32b
    n = ld<u32>(ctx, self + 0x19c);  // 0x1e32d
  }
}
int g_negctl_Seq_dtor = 0;
void a_Seq_dtor(Ctx &ctx, const uint32_t *a, Ret &) { Seq_dtor_body(ctx, a[0]); neg_mem(ctx, NEG(Seq_dtor), a[0] + 0x1a8); }

// ---- Midi_Device::closeFile (rva 0x1e3d0): converted from c3dh.cpp MidiClose_re
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
uint32_t Midi_Device__closeFile_body(Ctx &ctx, u32 self) {
    if ((ld<uint8_t>(ctx, self + 0x4c) & 1) != 0 && ld<uint32_t>(ctx, self) != 0) {  // 0x1e3d7 / 0x1e3dd / 0x1e3df
        snd_import(ctx, kIatMmioClose, SndAbi::Stdcall, {ld<uint32_t>(ctx, self), 0}).eax;  // 0x1e3e5
        st<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_openMmioCount).va(), ld<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_openMmioCount).va()) - 1);  // 0x1e3eb
    }
    u32 node = ld<u32>(ctx, self + 0x19c);  // 0x1e3f1
    if (node) {  // 0x1e3f9
        u32 elem = c3dh_popFront(ctx, self, 0x19c, 0x1a0, 0x1a8, node);  // 0x1e3ff-0x1e431
        while (elem) {  // 0x1e437 / 0x1e4a3
            if (ld<uint32_t>(ctx, elem + 0x3c) != 0 && (ld<uint8_t>(ctx, elem + 0x34) & 4) == 0) {  // 0x1e439 / 0x1e43e
                st<uint32_t>(ctx, elem + 0x3c, 0);  // 0x1e444
                st<uint32_t>(ctx, elem + 0x2c, 0);  // 0x1e447
                st<uint32_t>(ctx, elem + 0x40, 0);  // 0x1e44a
            }
            snd_call(ctx, 0x0001e7d0, SndAbi::Thiscall, {elem});  // 0x1e453 -> 0x1e7d0
            snd_delete(ctx, elem);  // 0x1e459
            node = ld<u32>(ctx, self + 0x19c);  // 0x1e461
            if (!node) break;  // 0x1e467 / 0x1e469
            elem = c3dh_popFront(ctx, self, 0x19c, 0x1a0, 0x1a8, node);  // 0x1e46b-0x1e49d
        }
    }
    unsigned f = ld<uint32_t>(ctx, self + 0x4c);  // 0x1e4a5
    st<uint32_t>(ctx, self, 0);  // 0x1e4a8
    f = (f & ~0xffu) | static_cast<uint8_t>(f & 0xfe);  // 0x1e4aa
    st<uint32_t>(ctx, self + 0x164, 0);  // 0x1e4ac
    st<uint32_t>(ctx, self + 0x16c, 0);  // 0x1e4b2
    st<uint32_t>(ctx, self + 0x4c, f);  // 0x1e4b8
    return 0;  // 0x1e4bd
}
int g_negctl_Midi_Device__closeFile = 0;
void a_Midi_Device__closeFile(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Midi_Device__closeFile), Midi_Device__closeFile_body(ctx, a[0]))); }

// ---- Track_clearFlag8 (rva 0x1e530): converted from c3ca.cpp TrackClearFlag8_re
int32_t Track_clearFlag8_body(Ctx &ctx, u32 self) {
    st<uint32_t>(ctx, self + 0x4c, ld<uint32_t>(ctx, self + 0x4c) & ~0x8u);  // and al, 0xf7 at 0x1e533
    return 0;  // xor eax, eax at 0x1e538
}
int g_negctl_Track_clearFlag8 = 0;
void a_Track_clearFlag8(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_i32(static_cast<int32_t>(neg_u32(NEG(Track_clearFlag8), static_cast<uint32_t>(Track_clearFlag8_body(ctx, a[0]))))); }

// ---- Seq_scaleTickRate (rva 0x1e630): converted from c3cx.cpp SeqScaleTick_re
// g_soundTimer 0xb49ec: a pointer to the sound timer, whose first dword is the period the rate functions divide by.
u32 c3cx_timer(Ctx &ctx) { return ld<u32>(ctx, snd_obj(ctx, &SoundState::g_soundTimer).va()); }
int32_t Seq_scaleTickRate_body(Ctx &ctx, u32 self, uint32_t arg) {
    u32 timer = c3cx_timer(ctx);  // 0x1e630
    if (!timer) return 0;  // je 0x1e66a at 0x1e638
    const unsigned period = ld<uint32_t>(ctx, timer + (0) * 4);  // 0x1e63a
    unsigned v = ld<uint32_t>(ctx, self + 0x17c);
    if (arg > period) {  // jbe at 0x1e642
        const unsigned q = arg / period;  // 0x1e646
        v = v / q;  // 0x1e652
    }
    st<uint32_t>(ctx, self + 0x174, v);  // 0x1e655 / 0x1e664
    return static_cast<int32_t>(v);
}
int g_negctl_Seq_scaleTickRate = 0;
void a_Seq_scaleTickRate(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_i32(static_cast<int32_t>(neg_u32(NEG(Seq_scaleTickRate), static_cast<uint32_t>(Seq_scaleTickRate_body(ctx, a[0], a[1]))))); }

// ---- Midi_readVarLen (rva 0x1e710): hand-fixed conversion of c3ar.cpp ReadVarLen_re
uint32_t Midi_readVarLen_body(Ctx &ctx, u32 p, u32 count) {
  unsigned acc = 0;
  st<u32>(ctx, count, 0);
  unsigned char cont;
  do {
    acc = (acc << 7) | static_cast<uint32_t>(ld<uint8_t>(ctx, p) & 0x7f);
    st<u32>(ctx, count, ld<u32>(ctx, count) + 1);
    cont = static_cast<uint8_t>(ld<uint8_t>(ctx, p) & 0x80);
    ++p;
  } while (cont != 0);
  return acc;
}
int g_negctl_Midi_readVarLen = 0;
void a_Midi_readVarLen(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Midi_readVarLen), Midi_readVarLen_body(ctx, a[0], a[1]))); }

// ---- Seq_rewindTracks (rva 0x1e8e0): converted from c3do.cpp SeqRewind_re
void Seq_rewindTracks_body(Ctx &ctx, u32 self) {
    unsigned edi = ld<uint32_t>(ctx, self + 0x34);  // 0x1e8e7
    unsigned char dl = ld<uint8_t>(ctx, self + 0x38);  // 0x1e8ea
    unsigned eax = ld<uint32_t>(ctx, self + 0x3c);  // 0x1e8ed
    edi &= 0xfffffffdu;  // 0x1e8f0
    dl &= 0xfb;  // 0x1e8f3
    st<uint32_t>(ctx, self + 0x2c, eax);  // 0x1e8f6
    st<uint32_t>(ctx, self + 0x40, 0);  // 0x1e8f9
    st<uint32_t>(ctx, self + 0x50, 0);  // 0x1e8fc
    st<uint32_t>(ctx, self + 0x34, edi);  // 0x1e8ff
    st<uint8_t>(ctx, self + 0x38, dl);  // 0x1e902
    st<uint32_t>(ctx, self + 0x4c, 0);  // 0x1e905
    st<uint32_t>(ctx, self + 0x48, 0x10);  // 0x1e908
    st<uint8_t>(ctx, self + 0x30, 0);  // 0x1e90f
    snd_call(ctx, 0x0001bc00, SndAbi::Thiscall, {self});  // 0x1e912 (thunk 0x22ed)
    u32 node = ld<u32>(ctx, self + 0xc);  // 0x1e917
    st<u32>(ctx, self + 0x14, node);  // 0x1e91c
    if (!node) return;  // 0x1e91f
    u32 v = ld<u32>(ctx, node + 8);  // 0x1e921
    if (!v) return;  // 0x1e926
    for (;;) {
        if (ld<int32_t>(ctx, v + 0x1c) == 5) {  // 0x1e928 / 0x1e92c
            snd_vcall(ctx, v, 0x10, SndAbi::Thiscall, {0}).eax;  // 0x1e92e-0x1e933
            unsigned char b = ld<uint8_t>(ctx, v + 0x19);  // 0x1e936
            unsigned char al = ld<uint8_t>(ctx, self + 0x38);  // 0x1e93b
            if (b == 1) al |= 1; else al &= 0xfe;  // 0x1e93e-0x1e944
            st<uint8_t>(ctx, self + 0x38, al);  // 0x1e946
        }
        node = ld<u32>(ctx, self + 0x14);  // 0x1e949
        if (!node) return;  // 0x1e94e
        node = ld<u32>(ctx, node + 4);  // 0x1e950
        st<u32>(ctx, self + 0x14, node);  // 0x1e955
        if (!node) return;  // 0x1e958
        v = ld<u32>(ctx, node + 8);  // 0x1e95a
        if (!v) return;  // 0x1e95f
    }
}
int g_negctl_Seq_rewindTracks = 0;
void a_Seq_rewindTracks(Ctx &ctx, const uint32_t *a, Ret &) { Seq_rewindTracks_body(ctx, a[0]); neg_mem(ctx, NEG(Seq_rewindTracks), a[0] + 0x4c); }

// ---- Track_setData (rva 0x1e9a0): converted from c3ex.cpp TrackSet_re
uint32_t Track_setData_body(Ctx &ctx, u32 self, uint32_t p) {
    u32 old = ld<u32>(ctx, self + 0x3c);  // 0x1e9a3
    if (old && (ld<uint8_t>(ctx, self + 0x34) & 4)) {  // 0x1e9a8-0x1e9ae
        snd_call(ctx, 0x0004249a, SndAbi::Cdecl, {old});  // 0x1e9b1
        st<u32>(ctx, self + 0x3c, 0);  // 0x1e9b9
    }
    st<uint32_t>(ctx, self + 0x3c, p);  // 0x1e9c6
    st<uint32_t>(ctx, self + 0x2c, p);  // 0x1e9c9
    unsigned r = p;  // 0x1e9c0
    if (p) r = snd_call(ctx, 0x0000215d, SndAbi::Thiscall, {self}).eax;  // 0x1e9cc-0x1e9d0
    st<uint32_t>(ctx, self + 0x40, 0);  // 0x1e9d5
    return r;
}
int g_negctl_Track_setData = 0;
void a_Track_setData(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Track_setData), Track_setData_body(ctx, a[0], a[1]))); }

// ---- Track_advancePos (rva 0x1e9f0): converted from c3ar.cpp TrackAdvance_re
uint32_t Track_advancePos_body(Ctx &ctx, u32 self, uint32_t delta) {
    unsigned pos = ld<uint32_t>(ctx, self + 0x40) + delta;
    unsigned limit = ld<uint32_t>(ctx, self + 0x44);
    if (pos > limit) return 0x22;
    if (pos == limit) st<uint32_t>(ctx, self + 0x34, ld<uint32_t>(ctx, self + 0x34) | 2u);
    st<uint32_t>(ctx, self + 0x40, pos);
    st<uint32_t>(ctx, self + 0x2c, ld<uint32_t>(ctx, self + 0x2c) + delta);
    return 0;
}
int g_negctl_Track_advancePos = 0;
void a_Track_advancePos(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Track_advancePos), Track_advancePos_body(ctx, a[0], a[1]))); }

// ---- Seq_clearTrackFlag8All (rva 0x1f150): converted from c3an.cpp SeqClearTrackFlag8All_re
void Seq_clearTrackFlag8All_body(Ctx &ctx, u32 self) {
    u32 node = ld<u32>(ctx, self + 0x19c);
    st<u32>(ctx, self + 0x1a4, node);
    if (node == 0) return;
    u32 track = ld<u32>(ctx, node + 8);
    if (track == 0) return;
    for (;;) {
        st<uint8_t>(ctx, track + 0x38, static_cast<uint8_t>(ld<uint8_t>(ctx, track + 0x38) & 0xf7));
        node = ld<u32>(ctx, self + 0x1a4);
        if (node == 0) return;
        node = ld<u32>(ctx, node + 4);
        st<u32>(ctx, self + 0x1a4, node);
        if (node == 0) return;
        track = ld<u32>(ctx, node + 8);
        if (track == 0) return;
    }
}
int g_negctl_Seq_clearTrackFlag8All = 0;
void a_Seq_clearTrackFlag8All(Ctx &ctx, const uint32_t *a, Ret &) { Seq_clearTrackFlag8All_body(ctx, a[0]); neg_mem(ctx, NEG(Seq_clearTrackFlag8All), a[0] + 0x1a4); }

// ---- Seq::loadFile (rva 0x20290): converted from c3ex.cpp SeqLoadFile_re
uint32_t c3ex_listGetAt(Ctx &ctx, u32 self, u32 out, uint32_t i) {
    return snd_call(ctx, 0x000015fa, SndAbi::Thiscall, {(self + 0x60), out, i}).eax;
}
uint32_t c3ex_trackCount(Ctx &ctx, u32 self) { return snd_vcall(ctx, self, 0xb4, SndAbi::Thiscall, {}).eax; }
// pre-roll every track: i from 0 while i < this's slot +0xb4 (called again after each pre-roll, unsigned `jb`), then bit 10 of the
// track list's +0x4c (this+0xac) is set and this+0x720 / +0x71c / +0x724 are cleared
void c3ex_preRollAll(Ctx &ctx, u32 self) {
    unsigned n = c3ex_trackCount(ctx, self);
    if (n) {
        unsigned i = 0;
        do {
            snd_call(ctx, 0x00001956, SndAbi::Thiscall, {self, i}).eax;
            n = c3ex_trackCount(ctx, self);
            ++i;
        } while (i < n);
    }
    st<uint32_t>(ctx, self + 0x60 + 0x4c, static_cast<uint32_t>(ld<uint32_t>(ctx, self + 0x60 + 0x4c) | 0x400));
    st<uint32_t>(ctx, self + 0x720, 0);
    st<uint32_t>(ctx, self + 0x71c, 0);
    st<uint32_t>(ctx, self + 0x724, 0);
}
uint32_t Seq__loadFile_body(Ctx &ctx, u32 self, uint32_t name) {
  SndFrame fr_cells(ctx, 0x10);  // S4.2b: the cells [4] of the original's locals passed by address (U-1418)
    if (ld<uint8_t>(ctx, self + 0x214) & 0x40)  // 0x20296 / 0x2029d
        snd_call(ctx, 0x00001997, SndAbi::Thiscall, {snd_va(ctx, 0xb49b8)}).eax;  // 0x2029f-0x202a4
    unsigned r = snd_call(ctx, 0x0000270c, SndAbi::Thiscall, {(self + 0x60), name}).eax;  // 0x202b3
    const u32 x = fr_cells.at(0x10);  // the out slot of Seq_listGetAt (0x202c3)
    st<u32>(ctx, x, 0);
    c3ex_listGetAt(ctx, self, x, 1);  // 0x202c3
    if (r) return r;  // 0x202ca
    c3ex_preRollAll(ctx, self);  // 0x202cc-0x2030c
    return r;
}
int g_negctl_Seq__loadFile = 0;
void a_Seq__loadFile(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Seq__loadFile), Seq__loadFile_body(ctx, a[0], a[1]))); }

// ---- Midi_loadMemoryAndPreRoll (rva 0x20360): converted from c3ex.cpp LoadMemPre_re
uint32_t Midi_loadMemoryAndPreRoll_body(Ctx &ctx, u32 self, uint32_t mem, uint32_t) {
  SndFrame fr_cells(ctx, 0x10);  // S4.2b: the cells [5] of the original's locals passed by address (U-1418)
    unsigned r = snd_call(ctx, 0x0000169f, SndAbi::Thiscall, {(self + 0x60), mem}).eax;  // 0x20373
    const u32 x = fr_cells.at(0x10);  // the out slot of Seq_listGetAt (0x20387)
    st<u32>(ctx, x, 0);
    c3ex_listGetAt(ctx, self, x, 1);  // 0x20387
    if (!r) c3ex_preRollAll(ctx, self);  // 0x2038e-0x203d2
    if (ld<uint32_t>(ctx, self + 0x224)) return r;  // 0x203d8 / 0x203de
    unsigned n = c3ex_trackCount(ctx, self);  // 0x203e6
    if (n) {  // 0x203ec / 0x203ee
        unsigned i = 0;
        do {
            st<u32>(ctx, x + 4, 0);
            if (!c3ex_listGetAt(ctx, self, x + 4, i)) {  // 0x203fa-0x20407
                const u32 t = ld<u32>(ctx, x + 4);
                st<uint8_t>(ctx, t + 0x38, static_cast<uint8_t>(ld<uint8_t>(ctx, t + 0x38) | 8));
            }
            n = c3ex_trackCount(ctx, self);  // 0x2040f
            ++i;
        } while (i < n);  // 0x20415 / 0x20417
    }
    return r;  // 0x20419
}
int g_negctl_Midi_loadMemoryAndPreRoll = 0;
void a_Midi_loadMemoryAndPreRoll(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Midi_loadMemoryAndPreRoll), Midi_loadMemoryAndPreRoll_body(ctx, a[0], a[1], a[2]))); }

// ---- Seq_closeFile (rva 0x204b0): converted from c3dt.cpp SeqCloseFile_re
uint32_t Seq_closeFile_body(Ctx &ctx, u32 self) {
    unsigned r = 0;  // 0x204b3
    if (ld<uint8_t>(ctx, self + 0xac) & 1) {  // 0x204b5 / 0x204bc
        r = snd_call(ctx, 0x00001014, SndAbi::Thiscall, {(self + 0x60)}).eax;  // 0x204c1
        st<uint32_t>(ctx, self + 0x44, static_cast<uint32_t>(ld<uint32_t>(ctx, self + 0x44) & 0xfffffffeu));  // 0x204c6
    }
    return r;  // 0x204ca
}
int g_negctl_Seq_closeFile = 0;
void a_Seq_closeFile(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Seq_closeFile), Seq_closeFile_body(ctx, a[0]))); }

// ---- Seq_preRollTrack (rva 0x205f0): converted from c3ex.cpp PreRoll_re
uint32_t c3ex_readVarLen(Ctx &ctx, u32 p, u32 v) { return snd_call(ctx, 0x0000139d, SndAbi::Cdecl, {p, v}).eax; }
void c3ex_advance(Ctx &ctx, u32 t, uint32_t n) { snd_call(ctx, 0x00001c7b, SndAbi::Thiscall, {t, n}).eax; }
uint32_t c3ex_playEvent(Ctx &ctx, u32 self, u32 t, u32 v) {
    return snd_call(ctx, 0x00002068, SndAbi::Thiscall, {self, t, v}).eax;
}
// The event walk Seq_stepTrackMeta and Seq_preRollTrack share: from the track's read pointer t+0x2c, a delta is read; while it
// reads (result 0) and the event's status nibble (byte +1 & 0xf0) is neither 0x90 nor 0xc0, the delta is consumed, the event is
// played with a count slot cleared to 0, a playEvent result of 0x29 stops with 0x29, the count the player left is consumed and the
// next delta is read. Any other end returns 0.
uint32_t c3ex_walkEvents(Ctx &ctx, u32 self, u32 t, u32 v) {
  u32 p = ld<u32>(ctx, t + 0x2c);
  if (c3ex_readVarLen(ctx, p, v)) return 0;
  for (;;) {
    const unsigned char stat = static_cast<uint8_t>(ld<uint8_t>(ctx, p + 1) & 0xf0);
    if (stat == 0x90 || stat == 0xc0) return 0;
    c3ex_advance(ctx, t, ld<u32>(ctx, v));
    st<u32>(ctx, v, 0);
    if (c3ex_playEvent(ctx, self, t, v) == 0x29) return 0x29;
    c3ex_advance(ctx, t, ld<u32>(ctx, v));
    p = ld<u32>(ctx, t + 0x2c);
    if (c3ex_readVarLen(ctx, p, v)) return 0;
  }
}
uint32_t Seq_preRollTrack_body(Ctx &ctx, u32 self, uint32_t i) {
  SndFrame fr_cells(ctx, 0x10);  // S4.2b: the cells [6] of the original's locals passed by address (U-1418)
    if (!(i < c3ex_trackCount(ctx, self))) return 0xa;  // 0x205f8-0x20606
    const u32 tc = fr_cells.at(0x10);  // the track slot (+0) and the byte-count slot (+4)
    st<u32>(ctx, tc, 0);
    if (c3ex_listGetAt(ctx, self, tc, i) == 0x24) return 0x24;  // 0x2061f-0x20627
    const u32 t = ld<u32>(ctx, tc);
    if (ld<uint8_t>(ctx, t + 0x34) & 2) return 0x29;  // 0x20636-0x2063e
    for (int k = 0; k < 0x80; ++k) st<uint32_t>(ctx, t + 0x58 + 4 * k, 0);  // 0x2064a-0x20654
    st<u32>(ctx, tc + 4, 0);
    return c3ex_walkEvents(ctx, self, t, tc + 4);  // 0x20656-0x206cf
}
int g_negctl_Seq_preRollTrack = 0;
void a_Seq_preRollTrack(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Seq_preRollTrack), Seq_preRollTrack_body(ctx, a[0], a[1]))); }

// ---- Seq_stepTrackMeta (rva 0x20720): converted from c3ex.cpp StepMeta_re
uint32_t Seq_stepTrackMeta_body(Ctx &ctx, u32 self, u32 t) {
  SndFrame fr_cells(ctx, 0x10);  // S4.2b: the cells [7] of the original's locals passed by address (U-1418)
    if (!t) return 0xa;  // 0x20727-0x2072f
    if (ld<uint8_t>(ctx, t + 0x34) & 2) return 0x29;  // 0x20738-0x20740
    const u32 v = fr_cells.at(0x10) + 4;
    st<u32>(ctx, v, 0);
    return c3ex_walkEvents(ctx, self, t, v);  // 0x20749-0x207b4
}
int g_negctl_Seq_stepTrackMeta = 0;
void a_Seq_stepTrackMeta(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Seq_stepTrackMeta), Seq_stepTrackMeta_body(ctx, a[0], a[1]))); }

// ---- Midi_Device::playEvent (rva 0x20870): hand-fixed conversion of c3fd.cpp PlayEv_re
uint32_t Midi_Device__playEvent_body(Ctx &ctx, u32 self, u32 track, u32 count) {
  if (!track) return 0xa;  // 0x20877-0x2087f
  u32 p = ld<u32>(ctx, track + 0x2c);  // 0x20888
  const uint32_t b = ld<uint8_t>(ctx, p);  // 0x2088f
  uint32_t kind;
  uint32_t channel;
  if (b & 0x80) {  // 0x20891 / 0x20894
    if ((b & 0xf0) != 0xf0) {  // 0x2089c-0x208aa
      st<uint8_t>(ctx, track + 0x30, static_cast<uint8_t>(b));  // 0x208ae
      channel = b & 0xf;  // 0x208b1
      st<uint32_t>(ctx, track + 0x48, channel);  // 0x208b6
      kind = b & 0xf0;  // 0x208b4
    } else {
      channel = track;  // 0x208c2
      kind = b;
    }
    st<u32>(ctx, count, 1);  // 0x208b9 / 0x208c6
    ++p;  // 0x208bf / 0x208cc
  } else {
    kind = ld<uint8_t>(ctx, track + 0x30) & 0xf0;  // 0x208cf / 0x208d5
    channel = ld<uint32_t>(ctx, track + 0x48);  // 0x208d2
  }
  const uint32_t idx = kind - 0x80;  // 0x208db
  if (idx > 0x7f) return 0xb;  // 0x208de / 0x208e1
  const uint32_t c = ld<uint8_t>(ctx, snd_va(ctx, 0x000209a4 + idx));  // 0x208e9
  const u32 arg = p;
  switch (c) {  // table 0x20988 (0x208ef)
    case 0:  // 0x20933
      st<u32>(ctx, count, ld<u32>(ctx, count) + snd_call(ctx, 0x000016bd, SndAbi::Thiscall, {self, arg, channel, track}).eax);
      return 0;
    case 1:  // 0x20915
      if (ld<uint8_t>(ctx, p + 1))  // 0x20918 / 0x2091a
        st<u32>(ctx, count, ld<u32>(ctx, count) + snd_call(ctx, 0x00001a14, SndAbi::Thiscall, {self, arg, channel, track}).eax);
      else
        st<u32>(ctx, count, ld<u32>(ctx, count) + snd_call(ctx, 0x000016bd, SndAbi::Thiscall, {self, arg, channel, track}).eax);
      return 0;
    case 2:  // 0x20969
      st<u32>(ctx, count, ld<u32>(ctx, count) + snd_call(ctx, 0x000022ca, SndAbi::Thiscall, {self, arg, channel, track}).eax);
      return 0;
    case 3:  // 0x2094a
      st<uint32_t>(ctx, self + 0x25c + 0x4c * channel, ld<uint8_t>(ctx, p + 0));  // 0x2094c-0x20954
      st<u32>(ctx, count, ld<u32>(ctx, count) + 1);  // 0x2095b-0x2095f
      return 0;
    case 4:  // 0x2095b
      st<u32>(ctx, count, ld<u32>(ctx, count) + 1);
      return 0;
    case 5: {  // 0x208f6
      st<u32>(ctx, count, ld<u32>(ctx, count) + snd_call(ctx, 0x0000124e, SndAbi::Thiscall, {self, arg, track}).eax);  // 0x208f8-0x20901
      if (ld<uint8_t>(ctx, p) == 0x2f) return 0x29;  // 0x20903-0x2090c
      return 0;
    }
    default:  // 0x2097c
      return 0xb;
  }
}
int g_negctl_Midi_Device__playEvent = 0;
void a_Midi_Device__playEvent(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Midi_Device__playEvent), Midi_Device__playEvent_body(ctx, a[0], a[1], a[2]))); }

// ---- Seq_setField730Clamped (rva 0x20f50): hand-fixed conversion of c3cf.cpp SeqSetField730Clamped_re
void Seq_setField730Clamped_body(Ctx &ctx, u32 self, int32_t value) {
  if (value > 0x7f) {  // jle 0x20f73 at 0x20f5b
    value = 0x7f;  // 0x20f5d
    snd_call(ctx, 0x0002b260, SndAbi::Thiscall, {self, static_cast<uint32_t>(value)});  // 0x20f63
    st<int32_t>(ctx, self + 0x730, value);  // 0x20f68
  } else {
    if (value < 0) value = 0;  // jge 0x20f79 at 0x20f75
    snd_call(ctx, 0x0002b260, SndAbi::Thiscall, {self, static_cast<uint32_t>(value)});  // 0x20f7c
    st<int32_t>(ctx, self + 0x730, value);  // 0x20f81
  }
}
int g_negctl_Seq_setField730Clamped = 0;
void a_Seq_setField730Clamped(Ctx &ctx, const uint32_t *a, Ret &) { Seq_setField730Clamped_body(ctx, a[0], static_cast<int32_t>(a[1])); neg_mem(ctx, NEG(Seq_setField730Clamped), a[0] + 0x730); }

// ---- Seq_setupChannel (rva 0x20fe0): hand-fixed conversion of c3fk.cpp SetupCh_re
constexpr uint32_t kDescCursor = 0xb4ff0;  // the channel-description list: cursor
constexpr uint32_t kDescHead = 0xb4fe8;  // head
constexpr uint32_t kDescTail = 0xb4fec;  // tail
constexpr uint32_t kDescCount = 0xb4ff4;  // count
void c3fk_freeChanRecord(Ctx &ctx, u32 r) {
  st<uint32_t>(ctx, r + 0x30, 0);  // 0x2103a / 0x21161
  snd_call(ctx, 0x00002004, SndAbi::Thiscall, {r});  // 0x2103d / 0x21164
  snd_delete(ctx, r);  // 0x21043 / 0x2116a
}
// the byte-pair string compare of the inline code (0x210b8-0x210ec, 0x21fef-0x2201f, 0x22045-0x22076): equal strings only
bool c3fk_streq(Ctx &ctx, u32 a, u32 b) {
  for (;;) {
    uint32_t x = ld<uint8_t>(ctx, a), y = ld<uint8_t>(ctx, b);
    if (x != y) return false;
    if (x == 0) return true;
    x = ld<uint8_t>(ctx, a + 1);
    y = ld<uint8_t>(ctx, b + 1);
    if (x != y) return false;
    a += 2;
    b += 2;
    if (x == 0) return true;
  }
}
uint32_t Seq_setupChannel_body(Ctx &ctx, u32 self, u32 desc, uint32_t flag) {
  const uint32_t ch = ld<uint32_t>(ctx, desc);  // 0x21002
  const uint32_t name = ld<uint32_t>(ctx, desc + 0x1c);  // 0x21004
  st<uint32_t>(ctx, self + 0x210, ld<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_soundWindow).va()));  // 0x21007 / 0x21017
  if (ch > 0x7f) return 0xa;  // 0x2100c / 0x2101d
  if (!name) return 0xa;  // 0x21025 / 0x21027
  const u32 slot = self + 0x744 + 4 * ch;
  if (const u32 old = ld<u32>(ctx, slot)) {  // 0x2102d-0x21036
    c3fk_freeChanRecord(ctx, old);
    st<u32>(ctx, slot, 0);  // 0x2104f
  }
  const u32 rec = snd_new(ctx, 0x34);  // 0x21058
  if (rec) {  // 0x21066 / 0x2106c
    snd_call(ctx, 0x000014bf, SndAbi::Thiscall, {rec});  // 0x21070
    st<uint32_t>(ctx, rec + 0x28, 0);  // 0x21075
    st<uint32_t>(ctx, rec + 0x2c, 0);  // 0x21078
    st<uint32_t>(ctx, rec + 0x30, 0);  // 0x2107b
  }
  if (ld<uint32_t>(ctx, desc + 0x24)) {  // 0x21090-0x21095
    snd_call(ctx, 0x00001604, SndAbi::Thiscall, {rec, ld<uint32_t>(ctx, desc + 0x24)});  // 0x2109e
    u32 cur = gref(ctx, kDescHead);  // 0x210a3
    st<u32>(ctx, snd_va(ctx, kDescCursor), cur);  // 0x210aa
    u32 obj = cur ? ld<u32>(ctx, cur + 8) : 0;  // 0x210af-0x210b6
    while (obj) {
      if (c3fk_streq(ctx, ld<u32>(ctx, obj), ld<uint32_t>(ctx, desc + 0x24))) {  // 0x210b8-0x210ec
        st<u32>(ctx, rec + 0x30, obj);  // 0x21108
        st<uint32_t>(ctx, obj + 0x210, ld<uint32_t>(ctx, obj + 0x210) + 1);  // 0x2110b-0x21112
        break;
      }
      cur = ld<u32>(ctx, gref(ctx, kDescCursor) + 4);  // 0x210ee / 0x210f3
      st<u32>(ctx, snd_va(ctx, kDescCursor), cur);  // 0x210f8
      if (!cur) break;  // 0x210fd
      obj = ld<u32>(ctx, cur + 8);  // 0x210ff-0x21104
    }
    snd_call(ctx, 0x00001d52, SndAbi::Thiscall, {rec, desc});  // 0x2111f
    st<u32>(ctx, slot, rec);  // 0x2112c
    return 0;  // 0x21133
  }
  snd_call(ctx, 0x000010f5, SndAbi::Thiscall, {rec, name});  // 0x2113a
  snd_call(ctx, 0x00001d52, SndAbi::Thiscall, {rec, desc});  // 0x21142
  if (flag & 0xff) {  // 0x21147 / 0x2114b
    const uint32_t r = snd_call(ctx, 0x000015d2, SndAbi::Thiscall, {rec, desc}).eax;  // 0x21150
    if (r) {  // 0x21157 / 0x21159
      if (rec) c3fk_freeChanRecord(ctx, rec);  // 0x2115b-0x2116a
      st<u32>(ctx, slot, 0);  // 0x2117c
      return r;  // 0x2117a
    }
  }
  st<u32>(ctx, slot, rec);  // 0x2118d
  return 0;  // 0x21194
}
int g_negctl_Seq_setupChannel = 0;
void a_Seq_setupChannel(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Seq_setupChannel), Seq_setupChannel_body(ctx, a[0], a[1], a[2]))); }

// ---- Midi_Device::setChannelVolume (rva 0x213d0): converted from c3bi.cpp MidiSetChannelVolume_re
int32_t Midi_Device__setChannelVolume_body(Ctx &ctx, u32 self, u32 event, uint32_t channel, u32 state) {
    const unsigned char controller = ld<uint8_t>(ctx, event + 0);  // 0x213db
    if (controller != 7) {  // je at 0x213e4
        if (controller == 0xA)  // jne at 0x213e8
            st<uint32_t>(ctx, self + 0x4C * channel + 0x260, ld<uint8_t>(ctx, event + 1));  // 0x213f3-0x213fb
        return 2;  // 0x21400
    }

    st<uint32_t>(ctx, self + 0x4C * channel + 0x254, ld<uint8_t>(ctx, event + 1));  // 0x21415-0x2141b
    if ((ld<uint8_t>(ctx, state + 0x38) & 1) != 0) return 2;  // jne at 0x2142a
    if (ld<uint32_t>(ctx, state + 0x48) != channel) return 2;  // jne at 0x2142f

    u32 cursor = state + 0x254;  // 0x21431
    for (int round = 0x80; round != 0; --round) {  // 0x2143c, jne at 0x214f6
        u32 group = ld<u32>(ctx, cursor);  // 0x21444
        if (group != 0) {  // je at 0x2144c
            if ((ld<uint32_t>(ctx, group + 0xC8) & 1) == 0) {  // je at 0x21459
                snd_vcall(ctx, group, 0x40, SndAbi::Thiscall, {ld<uint8_t>(ctx, event + 1)});  // 0x214db
            } else {
                u32 current = 0;  // 0x2145b
                unsigned bit = 1;  // 0x2145d
                u32 slot = group + 0x2A8;  // 0x21464
                for (int k = 0; k < 0x10; ++k) {  // jl at 0x214cb
                    const unsigned mask = (ld<uint32_t>(ctx, group + 0xC8) >> 0xC) & 0xFFFF;  // 0x2146a-0x21473
                    if ((bit & mask) != 0)  // je at 0x2147a
                        current = slot;  // 0x21481 (`jge` at 0x2147f is unreachable)
                    if (current != 0 && (ld<uint8_t>(ctx, current + 0x9C) & 8) == 0) {  // 0x21485, 0x2148e
                        unsigned amount = ld<uint32_t>(ctx, current + 0xE8) * ld<uint8_t>(ctx, event + 1);  // 0x21499-0x4149f
                        amount >>= 7;  // 0x214a2
                        amount *= ld<uint32_t>(ctx, self + 0x730);  // 0x214a5
                        amount >>= 7;  // 0x214ac
                        snd_call(ctx, 0x000383b0, SndAbi::Thiscall, {current, amount});  // 0x214b2
                    }
                    bit <<= 1;  // 0x214bf
                    slot += 0x110;  // 0x214c2
                }
            }
        }
        cursor -= 4;  // 0x214ea
    }
    return 2;  // 0x214ff
}
int g_negctl_Midi_Device__setChannelVolume = 0;
void a_Midi_Device__setChannelVolume(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_i32(static_cast<int32_t>(neg_u32(NEG(Midi_Device__setChannelVolume), static_cast<uint32_t>(Midi_Device__setChannelVolume_body(ctx, a[0], a[1], a[2], a[3]))))); }

// ---- Midi_Device::noteOn (rva 0x215c0): converted from c3fl.cpp NoteOn_re
constexpr uint32_t kIatMidiOutShortMsg = 0xb8638;  // c3fl.cpp
uint32_t Midi_Device__noteOn_body(Ctx &ctx, u32 self, u32 msg, uint32_t ch, u32 trk) {
  SndFrame fr_cells(ctx, 0x10);  // S4.2b: the cells [17] of the original's locals passed by address (U-1418)
    u32 d = self;
    u32 rec = d + 0x250 + ch * 0x4c;  // 0x215d2-0x215d9
    unsigned prog = ld<uint32_t>(ctx, rec + 0xc);  // 0x215e4
    unsigned vol = ld<uint32_t>(ctx, rec + 4);  // 0x215e7
    if (ld<uint8_t>(ctx, trk + 0x38) & 1) vol = 0;  // 0x215ea-0x215f9
    unsigned char apply = 1;  // 0x21614
    unsigned note = ld<uint8_t>(ctx, msg + 0);  // 0x21611
    if (snd_call(ctx, 0x00001906, SndAbi::Thiscall, {trk}).eax != 0)  // 0x2161b-0x21622
        note = snd_vcall(ctx, trk, 4, SndAbi::Thiscall, {note}).eax;  // 0x21624-0x2162c
    unsigned hout = ld<uint32_t>(ctx, ld<u32>(ctx, snd_obj(ctx, &SoundState::g_midiDevice).va()) + 0x3c);  // 0x2162e / 0x21633
    if (hout != 0) {  // 0x21636 / 0x21638
        if (ld<uint8_t>(ctx, trk + 0x38) & 1) return 2;  // 0x2163a / 0x2163e
        unsigned m = ((static_cast<uint32_t>(ld<uint8_t>(ctx, msg + 1)) << 8 | note) << 8 | ch) | 0x90;  // 0x21644-0x21650
        snd_import(ctx, kIatMidiOutShortMsg, SndAbi::Stdcall, {hout, m}).eax;  // 0x21653-0x21655
        m = ((static_cast<uint32_t>(ld<uint8_t>(ctx, msg + 1)) << 8 | note) << 8 | ch) | 0x90;  // 0x2165b-0x2166c (recomputed)
        st<uint32_t>(ctx, trk + note * 4 + 0x258, m);  // 0x2166f
        return 2;
    }
    u32 pg = ld<u32>(ctx, d + prog * 4 + 0x744);  // 0x21680-0x21688
    if (!pg) return 2;  // 0x2168f / 0x21695
    unsigned key = ld<uint8_t>(ctx, msg + 0);  // 0x2169f-0x216a1
    if (ld<uint32_t>(ctx, trk + key * 4 + 0x58) != 0) return 2;  // 0x216a4-0x216aa
    u32 bank = ld<u32>(ctx, pg + 0x30);  // 0x216b0
    if (!bank) return 2;  // 0x216b3 / 0x216b5
    if (ld<uint8_t>(ctx, bank + 0x214) & 1) key = snd_call(ctx, 0x00001546, SndAbi::Thiscall, {bank}).eax;  // 0x216bb-0x216c6
    u32 ins = ld<u32>(ctx, bank + (key & 0x7f) * 4 + 4);  // 0x216cb / 0x216ce
    if (ld<uint8_t>(ctx, bank + 0x214) & 8) apply = 0;  // 0x216d2-0x216dc
    if (!ins) return 2;  // 0x216e1 / 0x216e3
    if (snd_vcall(ctx, ins, 0x60, SndAbi::Thiscall, {}).eax == 0) return 2;  // 0x216e9-0x216f2
    if (snd_vcall(ctx, ins, 0x5c, SndAbi::Thiscall, {}).eax != 0 && !(ld<uint8_t>(ctx, ins + 0xc8) & 1)) {  // 0x216f8-0x2170a
        int cnt = ld<int32_t>(ctx, ins + 0x1438);  // 0x2170c
        if (cnt > 1) {  // 0x21712 / 0x21719 (jle)
            unsigned last = static_cast<uint32_t>(cnt) - 1, i = 0;  // 0x2171b / 0x2171e
            for (; i < last; i++) {  // 0x21720 / 0x21736-0x2173c
                u32 sub = ld<u32>(ctx, ins + i * 4 + 0x1440);  // 0x2172a
                if (snd_vcall(ctx, sub, 0x5c, SndAbi::Thiscall, {}).eax == 0) {  // 0x2172d-0x21734
                    ins = sub;  // 0x21740
                    break;
                }
            }
            if (i == static_cast<uint32_t>(cnt)) return 2;  // 0x2174f / 0x21751 (never true)
        }
    }
    if (ld<uint8_t>(ctx, pg + 4) & 8) st<u32>(ctx, trk + ld<uint8_t>(ctx, msg + 0) * 4 + 0x58, ins);  // 0x2175b-0x2176a
    st<uint32_t>(ctx, ins + 0x143c, ld<uint8_t>(ctx, msg + 0) + 1u);  // 0x2176e-0x21774
    unsigned v = 0;  // 0x2177d
    if (ld<uint8_t>(ctx, pg + 4) & 1) {  // 0x2177a-0x21782
        unsigned lo = ld<uint32_t>(ctx, pg + 0xc);  // 0x21787
        v = (snd_call(ctx, 0x0000209a, SndAbi::Thiscall, {snd_obj(ctx, &SoundState::g_playOrderSeed).va(), ld<uint32_t>(ctx, pg + 8) - lo}).eax & 0xffff) + lo;  // 0x21784-0x2179c
    }
    unsigned arg = v;
    if (!((ld<uint32_t>(ctx, pg + 4) >> 4) & 1))  // 0x2179e-0x217ab (`not; shr 4; test cl, 1`)
        arg = ld<uint32_t>(ctx, trk + 0x45c) + (ld<uint8_t>(ctx, msg + 0) - ld<uint32_t>(ctx, ins + 0x210)) * 100 + v;  // 0x217ad-0x217cf
    snd_vcall(ctx, ins, 0x9c, SndAbi::Thiscall, {arg}).eax;  // 0x217d3 / 0x217d5
    u32 voice = 0;  // 0x21609
    const u32 slotc = fr_cells.at(0x10);  // [esp+0x28], written by Channel_allocVoiceSlot
    st<u32>(ctx, slotc, 0);
    if (ld<uint8_t>(ctx, ins + 0xc8) & 1) {  // 0x217db / 0x217e2
        voice = (snd_call(ctx, 0x00001a3c, SndAbi::Thiscall, {ins + 0x70, slotc}).eax);  // 0x217e4-0x217f3
        if (!voice) return 2;  // 0x217f1 / 0x217f7
        st<uint32_t>(ctx, voice + 0xe4, ld<uint8_t>(ctx, msg + 0));  // 0x217fd-0x21802
    }
    if (ld<uint32_t>(ctx, ins + 0x228) != 0) {  // 0x21808-0x21810
        snd_vcall(ctx, ins, 0x48, SndAbi::Thiscall, {1}).eax;  // 0x21812-0x21818
        if (voice) st<uint32_t>(ctx, voice + 0x98, static_cast<uint32_t>(ld<uint32_t>(ctx, voice + 0x98) | 1));  // 0x2181f-0x2182b
    }
    if (ld<uint8_t>(ctx, ins + 0xc8) & 8) st<uint32_t>(ctx, ins + 0xc8, static_cast<uint32_t>(ld<uint32_t>(ctx, ins + 0xc8) & 0xfffffff7u));  // 0x21837-0x21848
    if (apply) {  // 0x2184e-0x21854
        unsigned velo = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(ld<uint8_t>(ctx, msg + 1))) + ld<int32_t>(ctx, trk + 0x460));  // 0x2185a-0x21869
        vol = (velo * vol) / 127u;  // 0x2186b-0x21883
        vol = (vol * ld<uint32_t>(ctx, d + 0x730)) / 127u;  // 0x21886-0x21899
        if (voice) st<uint32_t>(ctx, voice + 0xe8, velo);  // 0x2189c-0x218a4
    }
    unsigned level = vol;  // 0x218fa
    if (ld<uint8_t>(ctx, pg + 4) & 2) {  // 0x218ae / 0x218b2
        unsigned lo = ld<uint32_t>(ctx, pg + 0x14);  // 0x218b7
        unsigned r = (snd_call(ctx, 0x0000209a, SndAbi::Thiscall, {snd_obj(ctx, &SoundState::g_playOrderSeed).va(), ld<uint32_t>(ctx, pg + 0x10) - lo}).eax & 0xffff) + lo;  // 0x218b4-0x218d0
        level = vol + static_cast<uint32_t>(static_cast<int32_t>(ld<uint32_t>(ctx, d + 0x730) * r) / 127);  // 0x218d6-0x218f6
    }
    unsigned pan = ld<uint32_t>(ctx, rec + 0x10) - 0x3f;  // 0x21906 / 0x2190b, 0x21925 / 0x21928
    if (voice) {  // 0x218fe / 0x21900
        snd_call(ctx, 0x0000231a, SndAbi::Thiscall, {voice, pan}).eax;  // 0x2190e / 0x2190f
        snd_call(ctx, 0x00002040, SndAbi::Thiscall, {voice, level}).eax;  // 0x21914-0x21917
    } else {
        st<uint32_t>(ctx, ins + 0x294, pan);  // 0x2192b
        snd_vcall(ctx, ins, 0x40, SndAbi::Thiscall, {level}).eax;  // 0x21924-0x21933
    }
    if (ld<uint8_t>(ctx, ins + 0xc8) & 1) snd_call(ctx, 0x00001212, SndAbi::Thiscall, {ins + 0x70, ld<u32>(ctx, slotc)}).eax;  // 0x21936-0x21947
    if (!(ld<uint8_t>(ctx, ins + 0xc8) & 0x40)) {  // 0x2194c-0x21959
        st<uint32_t>(ctx, ins + 0xc8, static_cast<uint32_t>(ld<uint32_t>(ctx, ins + 0xc8) | 0x40));  // 0x2195b-0x21964
        snd_call(ctx, 0x00002351, SndAbi::Thiscall, {ld<u32>(ctx, snd_obj(ctx, &SoundState::g_soundEngine).va()) + 0x1c8, c3fl_u(ctx, ins)}).eax;  // 0x2196a-0x21976
    }
    return 2;  // 0x2197e
}
int g_negctl_Midi_Device__noteOn = 0;
void a_Midi_Device__noteOn(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Midi_Device__noteOn), Midi_Device__noteOn_body(ctx, a[0], a[1], a[2], a[3]))); }

// ---- Midi_Device::sendShortMsg (rva 0x21a90): converted from c3fd.cpp ShortMsg_re
constexpr uint32_t kMidiDevice = 0xb4a1c;  // c3fd.cpp
uint32_t Midi_Device__sendShortMsg_body(Ctx &ctx, u32 self, u32 msg, uint32_t channel, u32 track) {
    unsigned v = ld<uint32_t>(ctx, self + 0x25c + 0x4c * channel);  // 0x21aa3-0x21aa9
    unsigned note = ld<uint8_t>(ctx, msg + 0);  // 0x21ab8
    unsigned mapped = note;  // 0x21abc
    if (snd_call(ctx, 0x00001906, SndAbi::Thiscall, {track}).eax)  // 0x21abe-0x21ac5
        mapped = snd_vcall(ctx, track, 4, SndAbi::Thiscall, {mapped}).eax;  // 0x21ac7-0x21acf
    u32 md = gref(ctx, kMidiDevice);  // 0x21ad1
    unsigned h = ld<uint32_t>(ctx, md + 0x3c);  // 0x21ad8
    if (h) {  // 0x21adb / 0x21add
        unsigned m = ((static_cast<uint32_t>(ld<uint8_t>(ctx, msg + 1)) << 8 | mapped) << 8 | channel) | 0x80;  // 0x21ae3-0x21aef
        snd_import(ctx, kIatMidiOutShortMsg, SndAbi::Stdcall, {h, m}).eax;  // 0x21af4
        st<uint32_t>(ctx, track + 0x258 + 4 * mapped, 0);  // 0x21afa
        return 2;  // 0x21b08
    }
    u32 e = ld<u32>(ctx, self + 0x744 + 4 * v);  // 0x21b12-0x21b1a
    u32 c = ld<u32>(ctx, track + 0x58 + 4 * note);  // 0x21b21
    if (!c) return 2;  // 0x21b25 / 0x21b27
    if (!(ld<uint8_t>(ctx, e + 4) & 8)) return 2;  // 0x21b29 / 0x21b2d
    st<u32>(ctx, track + 0x58 + 4 * note, 0);  // 0x21b2f
    st<uint32_t>(ctx, c + 0x143c, 0);  // 0x21b33
    if (ld<uint8_t>(ctx, c + 0xc8) & 1) {  // 0x21b39 / 0x21b40
        u32 vc = (snd_call(ctx, 0x0000187a, SndAbi::Thiscall, {(c + 0x70), ld<uint8_t>(ctx, msg + 0)}).eax);  // 0x21b4e
        if (vc) snd_call(ctx, 0x000014a6, SndAbi::Thiscall, {vc}).eax;  // 0x21b53-0x21b59
        return 2;
    }
    u32 a = ld<u32>(ctx, e + 0x30);  // 0x21b6b
    if (a)  // 0x21b6e / 0x21b70
        snd_call(ctx, 0x0000272a, SndAbi::Thiscall, {(c + 0x70), ld<uint32_t>(ctx, a + 0x218)}).eax;  // 0x21b72-0x21b7c
    else
        snd_vcall(ctx, c, 0x28, SndAbi::Thiscall, {}).eax;  // 0x21b8e / 0x21b90
    return 2;
}
int g_negctl_Midi_Device__sendShortMsg = 0;
void a_Midi_Device__sendShortMsg(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Midi_Device__sendShortMsg), Midi_Device__sendShortMsg_body(ctx, a[0], a[1], a[2], a[3]))); }

// ---- Seq_setTempo (rva 0x21ee0): hand conversion of c3fh.cpp SetTempo_re (x87: Win32 asm / x64 double)
uint32_t Seq_setTempo_body(Ctx &ctx, u32 self, uint32_t tempo) {
  X80 T, A, p, k, q, r;
  x_ldU32(ctx, A, tempo);  // 0x21ef7 / 0x21efb: fild qword (tempo, 0)
  x_ldU32(ctx, T, ld<uint32_t>(ctx, self + 0x1bc));  // 0x21ef1 / 0x21f07: fild qword ([+0x1bc], 0)
  const int32_t old = ld<int32_t>(ctx, self + 0x718);  // 0x21f0b
  st<uint32_t>(ctx, self + 0x718, tempo);  // 0x21f19
  x_mul(ctx, p, T, A);  // 0x21f1f fmul st(1)
  x_ld32(ctx, k, ld<uint32_t>(ctx, snd_va(ctx, 0x5b6a0)));
  x_mul(ctx, p, p, k);  // 0x21f21 fmul dword [0x5b6a0]
  x_ld32(ctx, k, ld<uint32_t>(ctx, snd_va(ctx, 0x5b69c)));
  x_div(ctx, p, k, p);  // 0x21f27 fdivr dword [0x5b69c]
  x_st(ctx, self + 0x728, p);  // 0x21f2d
  x_ldI32(ctx, k, old);
  x_div(ctx, q, k, A);  // 0x21f33 fidivr dword (the old +0x718, signed)
  x_ld(ctx, r, self + 0x720);
  x_mul(ctx, r, r, q);  // 0x21f37 / 0x21f3d
  const uint32_t w = static_cast<uint32_t>(x_ftol(ctx, r));  // 0x21f3f __ftol; 0x21f44 drops q
  x_ldU32(ctx, r, w);  // 0x21f46-0x21f4e: fild qword (eax, 0)
  x_st(ctx, self + 0x720, r);  // 0x21f55
  return 0;
}
int g_negctl_Seq_setTempo = 0;
void a_Seq_setTempo(Ctx &ctx, const uint32_t *a, Ret &r) {
  PlainDoubles plain(NEG(Seq_setTempo) == 2);
  r.set_u32(neg_u32(NEG(Seq_setTempo), Seq_setTempo_body(ctx, a[0], a[1])));
}

// ---- Seq_updateChannel (rva 0x21fa0): hand-fixed conversion of c3fk.cpp UpdCh_re
uint32_t Seq_updateChannel_body(Ctx &ctx, u32 self, u32 desc) {
  if (!desc) return 0xa;  // 0x21fa7-0x21fb5
  const u32 rec = ld<u32>(ctx, self + 0x744 + 4 * ld<uint32_t>(ctx, desc));  // 0x21fb8 / 0x21fba
  if (!rec) {  // 0x21fc1 / 0x21fc7
    snd_call(ctx, 0x0000118b, SndAbi::Thiscall, {self, desc, 0});  // 0x22160-0x22163
    return 0;  // 0x2216b
  }
  snd_call(ctx, 0x00001d52, SndAbi::Thiscall, {rec, desc});  // 0x21fd0
  const u32 alt = ld<uint32_t>(ctx, desc + 0x24);  // 0x21fd5
  if (!alt) return 0;  // 0x21fd8 / 0x21fda
  snd_call(ctx, 0x00001604, SndAbi::Thiscall, {rec, ld<uint32_t>(ctx, desc + 0x24)});  // 0x21fe3
  if (const u32 cur0 = ld<u32>(ctx, rec + 0x30)) {  // 0x21fe8-0x21fed
    if (c3fk_streq(ctx, ld<u32>(ctx, cur0), ld<uint32_t>(ctx, desc + 0x24))) return 0;  // 0x21fef-0x2201f
  }
  u32 cur = gref(ctx, kDescHead);  // 0x22025
  st<u32>(ctx, snd_va(ctx, kDescCursor), cur);  // 0x2202e
  if (!cur) return 0;  // 0x22034
  u32 found = ld<u32>(ctx, cur + 8);  // 0x2203a
  if (!found) return 0;  // 0x2203f
  for (;;) {
    if (c3fk_streq(ctx, ld<u32>(ctx, found), ld<uint32_t>(ctx, desc + 0x24))) break;  // 0x22045-0x22076
    cur = ld<u32>(ctx, cur + 4);  // 0x22078
    st<u32>(ctx, snd_va(ctx, kDescCursor), cur);  // 0x2207d
    if (!cur) return 0;  // 0x22083
    found = ld<u32>(ctx, cur + 8);  // 0x22089
    if (!found) return 0;  // 0x2208e / 0x22090-0x22096
  }
  if (const u32 old = ld<u32>(ctx, rec + 0x30)) {  // 0x2209d-0x220a2
    const u32 left = ld<uint32_t>(ctx, old + 0x210) - 1;  // 0x220a8-0x220b5
    st<uint32_t>(ctx, old + 0x210, left);
    if (left == 0) {
      list_unlink(ctx, snd_va(ctx, kDescHead), snd_va(ctx, kDescTail), snd_va(ctx, kDescCursor), snd_va(ctx, kDescCount), ld<u32>(ctx, rec + 0x30));  // 0x220bb-0x22124
      if (const u32 o = ld<u32>(ctx, rec + 0x30)) {  // 0x22129-0x2212e
        snd_call(ctx, 0x00001d66, SndAbi::Thiscall, {o});  // 0x22132
        snd_delete(ctx, o);  // 0x22138
      }
    }
    st<u32>(ctx, rec + 0x30, 0);  // 0x22140
  }
  st<u32>(ctx, rec + 0x30, found);  // 0x22147
  st<uint32_t>(ctx, found + 0x210, ld<uint32_t>(ctx, found + 0x210) + 1);  // 0x2214a-0x22152
  return 0;  // 0x2215a
}
int g_negctl_Seq_updateChannel = 0;
void a_Seq_updateChannel(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Seq_updateChannel), Seq_updateChannel_body(ctx, a[0], a[1]))); }

// ---- Seq_reset (rva 0x22240): hand-fixed conversion of c3cf.cpp SeqReset_re
int32_t Seq_reset_body(Ctx &ctx, u32 self) {
  snd_call(ctx, 0x0001e1c0, SndAbi::Thiscall, {self + 0x60});  // 0x22243, 0x22246
  const uint32_t flags = ld<uint32_t>(ctx, self + 0x214);  // 0x2224b
  st<int32_t>(ctx, self + 0x740, 0);  // 0x22251
  st<uint32_t>(ctx, self + 0x214, flags & 0xffffffe7u);  // and al, 0xe7 at 0x2225b
  return 0;  // 0x22263
}
int g_negctl_Seq_reset = 0;
void a_Seq_reset(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_i32(static_cast<int32_t>(neg_u32(NEG(Seq_reset), static_cast<uint32_t>(Seq_reset_body(ctx, a[0]))))); }

// ---- Seq_copyChannelParams (rva 0x22280): converted from c3au.cpp SeqCopyChanParams_re
int32_t Seq_copyChannelParams_body(Ctx &ctx, u32 self, u32 src) {
    if (!src) return 0xa;
    unsigned index = ld<uint32_t>(ctx, src + 0x28);
    u32 dst = self + 0x264u + index * 0x4cu;
    static const unsigned kDwords[] = {4, 8, 0xc, 0x10, 0x14, 0x1c, 0x20, 0x24, 0x28, 0x2c, 0x30, 0x34};
    for (unsigned i = 0; i < sizeof(kDwords) / sizeof(kDwords[0]); ++i)
        st<uint32_t>(ctx, dst + kDwords[i], ld<uint32_t>(ctx, src + kDwords[i]));
    st<uint16_t>(ctx, dst + 0x18, ld<uint16_t>(ctx, src + 0x18));
    return 0;
}
int g_negctl_Seq_copyChannelParams = 0;
void a_Seq_copyChannelParams(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_i32(static_cast<int32_t>(neg_u32(NEG(Seq_copyChannelParams), static_cast<uint32_t>(Seq_copyChannelParams_body(ctx, a[0], a[1]))))); }

// ---- Seq_setPlayOrderMode (rva 0x22750): converted from c3dt.cpp SeqPlayOrder_re
uint32_t Seq_setPlayOrderMode_body(Ctx &ctx, u32 self, uint32_t mode) {
    if (mode > 2) return 0xa;  // 0x22754 / 0x22757
    unsigned f = ld<uint32_t>(ctx, self + 0x214);  // 0x22761
    st<uint32_t>(ctx, self + 0x214, ((mode & 3) << 7) | (f & 0xfffffe7fu));  // 0x2276a-0x22775
    snd_call(ctx, 0x00001bcc, SndAbi::Thiscall, {self, ld<uint32_t>(ctx, self + 0x238)}).eax;  // 0x22782
    return 0;  // 0x22787
}
int g_negctl_Seq_setPlayOrderMode = 0;
void a_Seq_setPlayOrderMode(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Seq_setPlayOrderMode), Seq_setPlayOrderMode_body(ctx, a[0], a[1]))); }

// ---- Seq_setField240 (rva 0x227a0): converted from c3cf.cpp SeqSetField240_re
int32_t Seq_setField240_body(Ctx &ctx, u32 self, int32_t value) {
    st<int32_t>(ctx, self + 0x240, value);  // 0x227a4
    return 0;  // 0x227aa
}
int g_negctl_Seq_setField240 = 0;
void a_Seq_setField240(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_i32(static_cast<int32_t>(neg_u32(NEG(Seq_setField240), static_cast<uint32_t>(Seq_setField240_body(ctx, a[0], a[1]))))); }

// ---- Seq_removeTrackById (rva 0x228d0): converted from c3do.cpp SeqRemoveTrackById_re
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
uint32_t Seq_removeTrackById_body(Ctx &ctx, u32 self, uint32_t id) {
    u32 node = ld<u32>(ctx, self + 0x218);  // 0x228d3
    st<u32>(ctx, self + 0x220, node);  // 0x228db
    if (!node) return 0x24;  // 0x228d9 / 0x228e1
    u32 tr = ld<u32>(ctx, node + 8);  // 0x228e7
    if (!tr) return 0x24;  // 0x228ea / 0x228ec
    while (ld<uint32_t>(ctx, tr + 4) != id) {  // 0x228f6 / 0x228f9
        node = ld<u32>(ctx, self + 0x220);  // 0x228fb
        if (!node) return 0x24;  // 0x22901 / 0x22903
        node = ld<u32>(ctx, node + 4);  // 0x22909
        st<u32>(ctx, self + 0x220, node);  // 0x2290e
        if (!node) return 0x24;  // 0x2290c / 0x22914
        tr = ld<u32>(ctx, node + 8);  // 0x2291a
        if (!tr) return 0x24;  // 0x2291d / 0x2291f
    }
    node = ld<u32>(ctx, self + 0x218);  // 0x22927
    if (!node) return 0;  // 0x2292d / 0x2292f -> 0x22997
    while (ld<u32>(ctx, node + 8) != tr) {  // 0x22931 / 0x22934
        node = ld<u32>(ctx, node + 4);  // 0x22936
        if (!node) return 0;  // 0x22939 / 0x2293b
    }
    listUnlink(ctx, self + 0x218, node);  // 0x22945-0x22991
    return 0;  // 0x22997
}
int g_negctl_Seq_removeTrackById = 0;
void a_Seq_removeTrackById(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Seq_removeTrackById), Seq_removeTrackById_body(ctx, a[0], a[1]))); }

// ---- Seq_buildTrack9 (rva 0x229f0): converted from c3fk.cpp Build9_re
// List header at a Seq: {+0 head, +4 tail, +8 cursor, +0xc count} (A at +0x218, B at +0x228). Raw append of a fresh node
// {0, 0, payload} (Seq_buildTrack9 0x22a9e-0x22b2e, Seq_buildTrack14 0x22d6a-0x22e00): operator new(0xc), the
// node filled when non-NULL; with a tail, tail+4 = node and node+0 = the tail (re-read); else head = node; tail = node; count
// re-read and incremented.
void c3fk_rawAppend(Ctx &ctx, u32 list, u32 payload) {
    u32 n = snd_new(ctx, 0xc);
    if (n) {
        st<uint32_t>(ctx, n, 0);
        st<uint32_t>(ctx, n + 4, 0);
        st<u32>(ctx, n + 8, payload);
    }
    if (u32 t = ld<u32>(ctx, list + 4)) {
        st<u32>(ctx, t + 4, n);
        st<u32>(ctx, n, ld<u32>(ctx, list + 4));
    } else {
        st<u32>(ctx, list, n);
    }
    st<u32>(ctx, list + 4, n);
    st<uint32_t>(ctx, list + 0xc, static_cast<uint32_t>(ld<uint32_t>(ctx, list + 0xc) + 1));
}
// The ordered insertion shared by Seq_buildTrack9 (0x22b36-0x22c57) and Seq_buildTrack14 (0x22e08-0x22f5b): both
// cursors start at their list's head (0x22b44 / 0x22b5e, 0x22e16 / 0x22e30); while A's payload is non-NULL and
// node+4 > payload+4 (unsigned, `jbe` 0x22b6b / 0x22e3d) A's cursor moves to its +4 (when non-NULL; the payload of a NULL
// cursor is 0) and B's cursor to its +4 when non-NULL. No stop: NodeList_pushBack(A, node) and (B, node) through the thunk
// 0x25bd. A stop: both cursors step back to their +0 when non-NULL; then A gets NodeList_pushBack when its head is NULL,
// NodeList_pushFront (thunk 0x1073) when its cursor is NULL, NodeList_pushBack when the cursor's +4 is NULL, otherwise a
// node from operator new(0xc) through NodeList_initNode (thunk 0x106e) linked between the cursor and its successor with A's
// count incremented; then NodeList_insert(B, node) through 0x1ef1. Seq_buildTrack9 steps B's cursor back after A's insertion
// (0x22c43-0x22c4c), Seq_buildTrack14 before it (0x22ea3-0x22eac): `bFirst`.
void c3fk_orderedInsert(Ctx &ctx, u32 self, u32 node, uint8_t bFirst) {
    u32 a = (self + 0x218);
    u32 b = (self + 0x228);
    st<u32>(ctx, a + 8, ld<u32>(ctx, a));
    u32 pl = ld<u32>(ctx, a) ? ld<u32>(ctx, ld<u32>(ctx, a) + 8) : 0;
    st<u32>(ctx, b + 8, ld<u32>(ctx, b));
    bool stop = false;
    while (pl) {
        if (ld<uint32_t>(ctx, node + 4) <= ld<uint32_t>(ctx, pl + 4)) { stop = true; break; }
        if (u32 c = ld<u32>(ctx, a + 8)) {
            c = ld<u32>(ctx, c + 4);
            st<u32>(ctx, a + 8, c);
            pl = c ? ld<u32>(ctx, c + 8) : 0;
        } else {
            pl = 0;
        }
        if (u32 c = ld<u32>(ctx, b + 8)) st<u32>(ctx, b + 8, ld<u32>(ctx, c + 4));
    }
    if (!stop) {
        snd_call(ctx, 0x000025bd, SndAbi::Thiscall, {a, node}).eax;
        snd_call(ctx, 0x000025bd, SndAbi::Thiscall, {b, node}).eax;
        return;
    }
    if (u32 c = ld<u32>(ctx, a + 8)) st<u32>(ctx, a + 8, ld<u32>(ctx, c));
    if (bFirst)
        if (u32 c = ld<u32>(ctx, b + 8)) st<u32>(ctx, b + 8, ld<u32>(ctx, c));
    if (!ld<u32>(ctx, a)) {
        snd_call(ctx, 0x000025bd, SndAbi::Thiscall, {a, node}).eax;
    } else if (!ld<u32>(ctx, a + 8)) {
        snd_call(ctx, 0x00001073, SndAbi::Thiscall, {a, node}).eax;
    } else if (u32 nx = ld<u32>(ctx, ld<u32>(ctx, a + 8) + 4)) {
        u32 p = snd_new(ctx, 0xc);
        u32 n = p ? (snd_call(ctx, 0x0000106e, SndAbi::Thiscall, {p, node}).eax) : 0;
        st<u32>(ctx, ld<u32>(ctx, a + 8) + 4, n);
        st<u32>(ctx, nx, n);
        st<u32>(ctx, n + 4, nx);
        st<u32>(ctx, n, ld<u32>(ctx, a + 8));
        st<uint32_t>(ctx, a + 0xc, static_cast<uint32_t>(ld<uint32_t>(ctx, a + 0xc) + 1));
    } else {
        snd_call(ctx, 0x000025bd, SndAbi::Thiscall, {a, node}).eax;
    }
    if (!bFirst)
        if (u32 c = ld<u32>(ctx, b + 8)) st<u32>(ctx, b + 8, ld<u32>(ctx, c));
    snd_call(ctx, 0x00001ef1, SndAbi::Thiscall, {b, node}).eax;
}
uint32_t Seq_buildTrack9_body(Ctx &ctx, u32 self, u32 desc) {
  SndFrame fr_cells(ctx, 0x10);  // S4.2b: the cells [12] of the original's locals passed by address (U-1418)
    if (!desc) return 0xa;  // 0x22a10-0x22a1b
    u32 p = snd_new(ctx, 0x38);  // 0x22a22
    u32 node = p ? (snd_call(ctx, 0x00001410, SndAbi::Thiscall, {p}).eax) : 0;  // 0x22a2e-0x22a3d
    rep_movs(ctx.mem, node, desc, 9 * 4);  // 0x22a3f-0x22a46
    const u32 local = fr_cells.at(0x10);  // 0x22a48
    st<u32>(ctx, local, 0);
    for (unsigned i = ld<uint32_t>(ctx, node + 8); i <= ld<uint32_t>(ctx, node + 0xc); ++i) {  // 0x22a50-0x22a60, 0x22a80-0x22a86
        snd_call(ctx, 0x000015fa, SndAbi::Thiscall, {(self + 0x60), local, i}).eax;  // 0x22a62-0x22a6d
        if (ld<u32>(ctx, local)) st<u32>(ctx, ld<u32>(ctx, local) + 0x458, node);  // 0x22a72-0x22a7a
    }
    if (ld<uint32_t>(ctx, self + 0x224) == 0) {  // 0x22a88 / 0x22a90
        snd_call(ctx, 0x000019e7, SndAbi::Thiscall, {(self + 0x60)}).eax;  // 0x22a99
        c3fk_rawAppend(ctx, (self + 0x218), node);  // 0x22a9e-0x22ae6
        c3fk_rawAppend(ctx, (self + 0x228), node);  // 0x22ae4-0x22b2e
    } else {
        c3fk_orderedInsert(ctx, self, node, false);  // 0x22b36-0x22c57
    }
    return 0;  // 0x22ba6
}
int g_negctl_Seq_buildTrack9 = 0;
void a_Seq_buildTrack9(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Seq_buildTrack9), Seq_buildTrack9_body(ctx, a[0], a[1]))); }

// ---- Seq_buildTrack14 (rva 0x22d00): converted from c3fk.cpp Build14_re
uint32_t Seq_buildTrack14_body(Ctx &ctx, u32 self, u32 desc) {
    u32 p = snd_new(ctx, 0x38);  // 0x22d1e
    u32 node = p ? (snd_call(ctx, 0x00001410, SndAbi::Thiscall, {p}).eax) : 0;  // 0x22d2c-0x22d47
    rep_movs(ctx.mem, node, desc, 0xe * 4);  // 0x22d49-0x22d5c
    if (ld<uint32_t>(ctx, self + 0x224) == 0) {  // 0x22d5e / 0x22d64
        c3fk_rawAppend(ctx, (self + 0x218), node);  // 0x22d6a-0x22db4
        c3fk_rawAppend(ctx, (self + 0x228), node);  // 0x22dab-0x22e00
    } else {
        c3fk_orderedInsert(ctx, self, node, true);  // 0x22e08-0x22f5b
    }
    return 0;  // 0x22e83
}
int g_negctl_Seq_buildTrack14 = 0;
void a_Seq_buildTrack14(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Seq_buildTrack14), Seq_buildTrack14_body(ctx, a[0], a[1]))); }

// ---- Seq_findTrackOrZero (rva 0x23000): converted from c3cf.cpp SeqFindTrackOrZero_re
u32 Seq_findTrackOrZero_body(Ctx &ctx, u32 self, int32_t id) {
    if (ld<u32>(ctx, self + 0x224) == 0) return 0;  // jne 0x2300f at 0x23008
    u32 node = ld<u32>(ctx, self + 0x218);  // 0x2300f
    st<u32>(ctx, self + 0x220, node);  // 0x23017
    if (!node) return 0;  // je 0x2304f at 0x2301d
    u32 elem = ld<u32>(ctx, node + 8);  // 0x2301f
    if (!elem) return 0;  // je 0x23051 at 0x23024
    for (;;) {
        if (ld<int32_t>(ctx, elem + 4) == id) return elem;  // je 0x23051 at 0x2302d
        node = ld<u32>(ctx, self + 0x220);  // 0x2302f
        if (!node) return 0;  // je 0x2304f at 0x23037
        node = ld<u32>(ctx, node + 4);  // 0x23039
        st<u32>(ctx, self + 0x220, node);  // 0x2303e
        if (!node) return 0;  // je 0x2304f at 0x23044
        elem = ld<u32>(ctx, node + 8);  // 0x23046
        if (!elem) return 0;  // je 0x23051 at 0x2304b
    }
}
int g_negctl_Seq_findTrackOrZero = 0;
void a_Seq_findTrackOrZero(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Seq_findTrackOrZero), Seq_findTrackOrZero_body(ctx, a[0], a[1]))); }

// ---- Seq_findTrackById (rva 0x23080): converted from c3cl.cpp SeqFindTrackById_re
int32_t Seq_findTrackById_body(Ctx &ctx, u32 self, int32_t id, int32_t value) {
    if (ld<int32_t>(ctx, self + 0x224) == 0) return 0x24;  // 0x23080-0x2308a
    u32 node = ld<u32>(ctx, self + 0x218);  // 0x23092
    st<u32>(ctx, self + 0x220, node);  // 0x2309a
    if (!node) return 0x24;  // je 0x230d6 at 0x230a0
    u32 elem = ld<u32>(ctx, node + 8);  // 0x230a2
    if (!elem) return 0x24;  // je 0x230d6 at 0x230a7
    while (ld<int32_t>(ctx, elem + 4) != id) {  // 0x230ad, je 0x230d2 at 0x230b0
        u32 cursor = ld<u32>(ctx, self + 0x220);  // 0x230b2
        if (!cursor) return 0x24;  // je 0x230d6 at 0x230ba
        u32 next = ld<u32>(ctx, cursor + 4);  // 0x230bc
        st<u32>(ctx, self + 0x220, next);  // 0x230c1
        if (!next) return 0x24;  // je 0x230d6 at 0x230c7
        elem = ld<u32>(ctx, next + 8);  // 0x230c9
        if (!elem) return 0x24;  // je 0x230d6 at 0x230ce
    }
  // 0x230d2: `test eax, eax` on the element, proved non-null above; the 0x24 arm is unreachable.
    snd_call(ctx, 0x0002e430, SndAbi::Thiscall, {elem, static_cast<uint32_t>(value)});
    return 0;  // 0x230ea
}
int g_negctl_Seq_findTrackById = 0;
void a_Seq_findTrackById(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_i32(static_cast<int32_t>(neg_u32(NEG(Seq_findTrackById), static_cast<uint32_t>(Seq_findTrackById_body(ctx, a[0], a[1], a[2]))))); }

// ---- Seq_cmdHandler7 (rva 0x231e0): converted from c3do.cpp SeqCmd7_re
uint32_t Seq_cmdHandler7_body(Ctx &ctx, u32 self) {
    if (!ld<uint32_t>(ctx, self + 0x224)) return 0x24;  // 0x231e4-0x231ec
    u32 node = ld<u32>(ctx, self + 0x218);  // 0x231f6
    st<u32>(ctx, self + 0x220, node);  // 0x231fe
    if (!node) return 0x24;  // 0x23204
    u32 tr = ld<u32>(ctx, node + 8);  // 0x23206
    if (!tr) return 0x24;  // 0x2320b
    for (;;) {
        if (ld<uint32_t>(ctx, tr + 4) == ld<uint32_t>(ctx, self + 0x738)) break;  // 0x2320d-0x23218
        node = ld<u32>(ctx, self + 0x220);  // 0x2321a
        if (!node) return 0x24;  // 0x23222
        node = ld<u32>(ctx, node + 4);  // 0x23224
        st<u32>(ctx, self + 0x220, node);  // 0x23229
        if (!node) return 0x24;  // 0x2322f
        tr = ld<u32>(ctx, node + 8);  // 0x23231
        if (!tr) return 0x24;  // 0x23236
    }
    unsigned a = ld<uint32_t>(ctx, tr + 0x1c);  // 0x23246
    if (a) snd_vcall(ctx, self, 0x84, SndAbi::Thiscall, {a}).eax;  // 0x23249-0x23252
    unsigned b = ld<uint32_t>(ctx, tr + 0xc);  // 0x23258
    unsigned c = ld<uint32_t>(ctx, tr + 8);  // 0x2325b
    return snd_vcall(ctx, self, 0x94, SndAbi::Thiscall, {c, b}).eax;  // 0x2325e-0x23264
}
int g_negctl_Seq_cmdHandler7 = 0;
void a_Seq_cmdHandler7(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Seq_cmdHandler7), Seq_cmdHandler7_body(ctx, a[0]))); }

// ---- Seq_cmdHandler8 (rva 0x23540): converted from c3ew.cpp SeqH8_re
uint32_t c3ew_u(Ctx &, u32 p) { return p; }
uint32_t Seq_cmdHandler8_body(Ctx &ctx, u32 self, uint32_t id) {
    u32 s = self;
    if (ld<uint32_t>(ctx, s + 0x224) == 0) return 0x24;  // 0x23544-0x2354f
    if (ld<uint32_t>(ctx, s + 0x738) == id) return 0;  // 0x2355c-0x23567
    u32 front = ld<u32>(ctx, s + 0x218);  // 0x2356d
    st<u32>(ctx, s + 0x220, front);  // 0x23575
    if (!front) return 0x24;  // 0x2357b / 0x235ad
    u32 val = ld<u32>(ctx, front + 8);  // 0x2357d
    if (!val) return 0x24;  // 0x23580 / 0x23582
    for (;;) {
        if (ld<uint32_t>(ctx, val + 4) == id) break;  // 0x23584-0x23587 (match)
        u32 cur = ld<u32>(ctx, s + 0x220);  // 0x23589
        if (!cur) return 0x24;  // 0x2358f / 0x23591
        cur = ld<u32>(ctx, cur + 4);  // 0x23593
        st<u32>(ctx, s + 0x220, cur);  // 0x23598
        if (!cur) return 0x24;  // 0x2359e
        val = ld<u32>(ctx, cur + 8);  // 0x235a0
        if (!val) return 0x24;  // 0x235a3 / 0x235a5
    }
  // 0x235a9: the matched value is non-NULL by the checks above
    if (ld<uint32_t>(ctx, val + 0x1c) != 0)  // 0x235b7-0x235bc
        snd_vcall(ctx, s, 0x84, SndAbi::Thiscall, {ld<uint32_t>(ctx, val + 0x1c)}).eax;  // 0x235be-0x235c3
    return snd_call(ctx, 0x00001519, SndAbi::Thiscall, {s, c3ew_u(ctx, val)}).eax;  // 0x235c9-0x235cc
}
int g_negctl_Seq_cmdHandler8 = 0;
void a_Seq_cmdHandler8(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Seq_cmdHandler8), Seq_cmdHandler8_body(ctx, a[0], a[1]))); }

// ---- Seq_remarkEventRange (rva 0x23610): hand-fixed conversion of c3bn.cpp SeqRemarkEventRange_re
// Seq_listGetAt 0x1d780 through its thunk 0x15fa: __thiscall (list, out, index), result 0 on success.
uint32_t c3fd_listGetAt(Ctx &ctx, u32 list, u32 out, uint32_t index) {
  return snd_call(ctx, 0x000015fa, SndAbi::Thiscall, {list, out, index}).eax;
}
int32_t Seq_remarkEventRange_body(Ctx &ctx, u32 self, u32 desc) {
  SndFrame fr_cells(ctx, 0x10);  // S4.2b: the cells [15] of the original's locals passed by address (U-1418)
  const uint32_t last = ld<uint32_t>(ctx, desc + 0x0c);  // 0x2361b
  const uint32_t first = ld<uint32_t>(ctx, desc + 0x08);  // 0x2361e
  if (last < first) return 0xa;  // jb at 0x23623
  if (last >= snd_vcall(ctx, self, 0xb4, SndAbi::Thiscall, {}).eax) return 0xa;  // call at 0x2362b, jae at 0x23633
  const u32 list = self + 0x60;  // 0x2363d
  const u32 found = fr_cells.at(0x10);  // the out slot of Seq_listGetAt
  st<u32>(ctx, found, 0);
  if (c3fd_listGetAt(ctx, list, found, 0) != 0)  // 0x23647, je at 0x2364e
    st<uint8_t>(ctx, ld<u32>(ctx, found) + 0x38, static_cast<uint8_t>(ld<uint8_t>(ctx, ld<u32>(ctx, found) + 0x38) | 8));  // 0x23654 (faults: found is NULL there)
  if (snd_vcall(ctx, self, 0x5c, SndAbi::Thiscall, {}).eax == 0) {  // call at 0x2365c, jne at 0x23661
    for (uint32_t i = first; i <= last; ++i) {  // jbe at 0x2368b
      st<u32>(ctx, found, 0);
      if (c3fd_listGetAt(ctx, list, found, i) == 0)  // 0x23677, jne at 0x2367e
        st<uint8_t>(ctx, ld<u32>(ctx, found) + 0x38, static_cast<uint8_t>(ld<uint8_t>(ctx, ld<u32>(ctx, found) + 0x38) | 8));  // 0x23684
    }
    return 0;  // 0x23690
  }
  for (uint32_t i = first; i <= last; ++i) {  // jbe at 0x236ed
    st<u32>(ctx, found, 0);
    if (c3fd_listGetAt(ctx, list, found, i) != 0) continue;  // 0x236a5, jne at 0x236ac
    const u32 f = ld<u32>(ctx, found);
    if (!f) continue;  // je at 0x236b4
    st<uint8_t>(ctx, f + 0x38, static_cast<uint8_t>(ld<uint8_t>(ctx, f + 0x38) | 8));  // 0x236b6-0x236bc
    snd_call(ctx, 0x00024b80, SndAbi::Thiscall, {self, f, ld<uint32_t>(ctx, self + 0x71c), ld<uint32_t>(ctx, desc + 0x00), ld<uint32_t>(ctx, desc + 0x20)});  // 0x236bf-0x236d4
    const u32 counter = ld<u32>(ctx, f + 0x458);  // 0x236dd
    if (counter)  // je at 0x236e5
      st<uint32_t>(ctx, counter + 0x34, ld<uint32_t>(ctx, counter + 0x34) + 1);  // 0x236e7
  }
  return 0;  // 0x236f2
}
int g_negctl_Seq_remarkEventRange = 0;
void a_Seq_remarkEventRange(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_i32(static_cast<int32_t>(neg_u32(NEG(Seq_remarkEventRange), static_cast<uint32_t>(Seq_remarkEventRange_body(ctx, a[0], a[1]))))); }

// ---- Seq_cmdHandler9 (rva 0x238b0): converted from c3ew.cpp SeqH9_re
uint32_t Seq_cmdHandler9_body(Ctx &ctx, u32 self, uint32_t id) {
    u32 s = self;
    if (ld<uint32_t>(ctx, s + 0x224) == 0) return 0x24;  // 0x238b0-0x238bf
    u32 node = ld<u32>(ctx, s + 0x218);  // 0x238c2
    st<u32>(ctx, s + 0x220, node);  // 0x238ca
    if (!node) return 0x24;  // 0x238d0 / 0x23906
    u32 val = ld<u32>(ctx, node + 8);  // 0x238d2
    if (!val) return 0x24;  // 0x238d7
    while (ld<uint32_t>(ctx, val + 4) != id) {  // 0x238dd-0x238e0
        node = ld<u32>(ctx, s + 0x220);  // 0x238e2
        if (!node) return 0x24;  // 0x238e8 / 0x23906
        node = ld<u32>(ctx, node + 4);  // 0x238ec
        st<u32>(ctx, s + 0x220, node);  // 0x238f1
        if (!node) return 0x24;  // 0x238f7
        val = ld<u32>(ctx, node + 8);  // 0x238f9
        if (!val) return 0x24;  // 0x238fe
    }
    if (!val) return 0x24;  // 0x23902-0x23906
    st<uint32_t>(ctx, val + 0x34, 0);  // 0x23911
  // ecx is unchanged from entry, so this = the sequencer (0x238b0 onward never writes ecx)
    return snd_call(ctx, 0x00001c3f, SndAbi::Thiscall, {s, ld<uint32_t>(ctx, val + 8), ld<uint32_t>(ctx, val + 0xc)}).eax;  // 0x2390e-0x2391d
}
int g_negctl_Seq_cmdHandler9 = 0;
void a_Seq_cmdHandler9(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Seq_cmdHandler9), Seq_cmdHandler9_body(ctx, a[0], a[1]))); }

// ---- Seq_replayEventRange (rva 0x23950): hand-fixed conversion of c3fd.cpp Replay_re
uint32_t Seq_replayEventRange_body(Ctx &ctx, u32 self, uint32_t first, uint32_t last) {
  SndFrame fr_cells(ctx, 0x10);  // S4.2b: the cells [14] of the original's locals passed by address (U-1418)
  if (last < first) return 0xa;  // 0x2395b / 0x2395f
  if (last >= snd_vcall(ctx, self, 0xb4, SndAbi::Thiscall, {}).eax) return 0xa;  // 0x23967-0x2396f
  const u32 list = self + 0x60;  // 0x2397a
  const u32 slot = fr_cells.at(0x10);  // the argument slot the original passes as the out slot
  st<u32>(ctx, slot, last);
  if (c3fd_listGetAt(ctx, list, slot, 0) != 0) {  // 0x23982 / 0x23989
    const u32 t = ld<u32>(ctx, slot);
    st<uint8_t>(ctx, t + 0x38, static_cast<uint8_t>(ld<uint8_t>(ctx, t + 0x38) | 8));  // 0x2398b / 0x2398f
  }
  if (snd_vcall(ctx, self, 0x5c, SndAbi::Thiscall, {}).eax == 0) {  // 0x23997 / 0x2399c
    for (uint32_t i = first; i <= last; ++i) {  // 0x2399e / 0x239bb / 0x239be
      if (c3fd_listGetAt(ctx, list, slot, i) == 0) {  // 0x239aa / 0x239b1
        const u32 t = ld<u32>(ctx, slot);
        st<uint8_t>(ctx, t + 0x38, static_cast<uint8_t>(ld<uint8_t>(ctx, t + 0x38) & 0xf7));  // 0x239b3 / 0x239b7
      }
    }
  } else {
    for (uint32_t i = first; i <= last; ++i) {  // 0x239c9 / 0x239ff / 0x23a02
      if (c3fd_listGetAt(ctx, list, slot, i) != 0) continue;  // 0x239d5 / 0x239dc
      const u32 t = ld<u32>(ctx, slot);
      const uint8_t b = ld<uint8_t>(ctx, t + 0x38);  // 0x239e2
      if (!(b & 8)) continue;  // 0x239e5 / 0x239e7
      st<uint8_t>(ctx, t + 0x38, static_cast<uint8_t>(b & 0xf7));  // 0x239e9 / 0x239ec
      snd_call(ctx, 0x000021ee, SndAbi::Thiscall, {self, i});  // 0x239f1
      snd_call(ctx, 0x00001ca3, SndAbi::Thiscall, {t});  // 0x239fa
    }
  }
  return 0;  // 0x239c3 / 0x23a07
}
int g_negctl_Seq_replayEventRange = 0;
void a_Seq_replayEventRange(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Seq_replayEventRange), Seq_replayEventRange_body(ctx, a[0], a[1], a[2]))); }

// ---- Seq_cmdHandler10 (rva 0x23a60): hand-fixed conversion of c3bi.cpp SeqCmdHandler10_re
int32_t Seq_cmdHandler10_body(Ctx &ctx, u32 self, uint32_t tag, uint32_t value) {
  SndFrame fr_cells(ctx, 0x10);  // S4.2b: the cells [16] of the original's locals passed by address (U-1418)
  if (ld<int32_t>(ctx, self + 0x224) == 0) return 0x24;  // jne at 0x23a6b
  u32 node = ld<u32>(ctx, self + 0x218);  // 0x23a63
  st<u32>(ctx, self + 0x220, node);  // 0x23a7e
  if (node == 0) return 0x24;  // je at 0x23a84
  u32 payload = ld<u32>(ctx, node + 8);  // 0x23a86
  if (payload == 0) return 0x24;  // je at 0x23a8b
  while (ld<uint32_t>(ctx, payload + 4) != tag) {  // je at 0x23a94
    node = ld<u32>(ctx, self + 0x220);  // 0x23a96
    if (node == 0) return 0x24;  // je at 0x23a9e
    node = ld<u32>(ctx, node + 4);  // 0x23aa0
    st<u32>(ctx, self + 0x220, node);  // 0x23aa5
    if (node == 0) return 0x24;  // je at 0x23aab
    payload = ld<u32>(ctx, node + 8);  // 0x23aad
    if (payload == 0) return 0x24;  // je at 0x23ab2
  }
  const uint32_t low = ld<uint32_t>(ctx, payload + 8);  // 0x23ac4
  const uint32_t high = ld<uint32_t>(ctx, payload + 0xc);  // 0x23ac8
  if (high < low) return 0xa;  // jb at 0x23acd
  const uint32_t count = snd_vcall(ctx, self, 0xb4, SndAbi::Thiscall, {}).eax;  // 0x23ad3
  if (high >= count) return 0xa;  // jae at 0x23adb
  const u32 record = fr_cells.at(0x10);  // the out slot of Seq_listGetAt
  st<u32>(ctx, record, 0);
  if (c3fd_listGetAt(ctx, self + 0x60, record, 0) != 0) {  // je at 0x23af1
    const u32 t = ld<u32>(ctx, record);
    st<uint8_t>(ctx, t + 0x38, static_cast<uint8_t>(ld<uint8_t>(ctx, t + 0x38) | 8));  // 0x23af7
  }
  uint32_t i = low;  // 0x23afd
  do {
    if (c3fd_listGetAt(ctx, self + 0x60, record, i) == 0)  // jne at 0x23b14
      st<uint32_t>(ctx, ld<u32>(ctx, record) + 0x45c, value);  // 0x23b1a
    ++i;  // 0x23b20
  } while (i <= high);  // jbe at 0x23b23
  return 0;  // 0x23b28
}
int g_negctl_Seq_cmdHandler10 = 0;
void a_Seq_cmdHandler10(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_i32(static_cast<int32_t>(neg_u32(NEG(Seq_cmdHandler10), static_cast<uint32_t>(Seq_cmdHandler10_body(ctx, a[0], a[1], a[2]))))); }

// ---- Seq_cmdHandler12 (rva 0x23c20): hand-fixed conversion of c3dh.cpp SeqCmd12_re
uint32_t Seq_cmdHandler12_body(Ctx &ctx, u32 self, uint32_t index, uint32_t flag) {
  SndFrame fr_cells(ctx, 0x10);  // S4.2b: the cells [13] of the original's locals passed by address (U-1418)
  const u32 slot = fr_cells.at(0x10);  // the stack slot of the first argument: it starts as the index
  st<u32>(ctx, slot, index);
  const uint32_t r = snd_call(ctx, 0x000015fa, SndAbi::Thiscall, {self + 0x60, slot, index}).eax;  // 0x23c2d -> 0x1d780
  if (r != 0) return r;  // 0x23c32 / 0x23c34
  uint8_t cl = static_cast<uint8_t>(flag & 1);  // 0x23c36 / 0x23c3f
  cl = static_cast<uint8_t>(cl << 4);  // 0x23c42
  const u32 t = ld<u32>(ctx, slot);
  uint8_t dl = ld<uint8_t>(ctx, t + 0x38);  // 0x23c45
  dl = static_cast<uint8_t>(dl & 0xef);  // 0x23c48
  st<uint8_t>(ctx, t + 0x38, static_cast<uint8_t>(cl | dl));  // 0x23c4b / 0x23c4d
  return r;
}
int g_negctl_Seq_cmdHandler12 = 0;
void a_Seq_cmdHandler12(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Seq_cmdHandler12), Seq_cmdHandler12_body(ctx, a[0], a[1], a[2]))); }

// ---- Seq_cmdHandler11 (rva 0x23c70): converted from c3au.cpp SeqCmd11_re
int32_t Seq_cmdHandler11_body(Ctx &ctx, u32 self, uint32_t key, uint32_t value) {
    if (ld<uint32_t>(ctx, self + 0x224) == 0) return 0x24;
    u32 node = ld<u32>(ctx, self + 0x218);
    st<u32>(ctx, self + 0x220, node);
    if (!node) return 0x24;
    u32 rec = ld<u32>(ctx, node + 8);
    if (!rec) return 0x24;
    while (ld<uint32_t>(ctx, rec + 4) != key) {
        node = ld<u32>(ctx, self + 0x220);
        if (!node) return 0x24;
        node = ld<u32>(ctx, node + 4);
        st<u32>(ctx, self + 0x220, node);
        if (!node) return 0x24;
        rec = ld<u32>(ctx, node + 8);
        if (!rec) return 0x24;
    }
    st<uint32_t>(ctx, rec + 0x30, value);
    return 0;
}
int g_negctl_Seq_cmdHandler11 = 0;
void a_Seq_cmdHandler11(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_i32(static_cast<int32_t>(neg_u32(NEG(Seq_cmdHandler11), static_cast<uint32_t>(Seq_cmdHandler11_body(ctx, a[0], a[1], a[2]))))); }

// ---- Seq_buildPlayOrder (rva 0x23d00): hand-fixed conversion of c3fk.cpp PlayOrder_re
constexpr uint32_t kIatTimeGetTime = 0xb8654;  // c3fk.cpp
// nextRandomFloat 0x285a0 (thunk 0x114f, ecx = the seed) leaves a float in st(0) and __ftol 0x42550 converts st(0) to an integer
// in eax (`fistp qword` under a truncating control word); the pair is called back to back with no instruction between them
// (0x23d42 / 0x23d47): the float the callee returns is truncated toward zero, low dword kept.
uint32_t c3fk_randomInt(Ctx &ctx, u32 seed) {
  snd_call(ctx, 0x0000114f, SndAbi::Thiscall, {seed}, true);  // the float stays on the x87 unit (S4.2b)
  return snd_ftol(ctx).eax;  // __ftol pops it
}
uint32_t Seq_buildPlayOrder_body(Ctx &ctx, u32 self, u32 out) {
  if ((ld<uint32_t>(ctx, self + 0x214) & 0x180) == 0x100) {  // 0x23d05-0x23d15
    uint32_t idx = ld<uint32_t>(ctx, self + 0x248);  // 0x23d83
    for (int32_t k = 0; k < ld<int32_t>(ctx, self + 0x23c); ++k) {  // 0x23d8d / 0x23da8-0x23db4
      st<uint32_t>(ctx, out + 4 * static_cast<u32>(k), idx);  // 0x23d95
      ++idx;  // 0x23d9d
      if (idx > ld<uint32_t>(ctx, self + 0x24c)) idx = ld<uint32_t>(ctx, self + 0x248);  // 0x23d97-0x23da2
    }
    st<uint32_t>(ctx, self + 0x244, 0);  // 0x23db6
    return idx;
  }
  const u32 seed = snd_obj(ctx, &SoundState::g_playOrderSeed).va();
  st<u32>(ctx, seed, snd_import(ctx, kIatTimeGetTime, SndAbi::Stdcall, {}).eax);  // 0x23d17 / 0x23d1d
  int32_t count = ld<int32_t>(ctx, self + 0x23c);  // 0x23d22
  if (count <= 0) {  // 0x23d2a / 0x23d2c
    st<uint32_t>(ctx, self + 0x244, 0);  // 0x23dc2
    return static_cast<uint32_t>(count);
  }
  for (int32_t k = 0; k < count; count = ld<int32_t>(ctx, self + 0x23c)) {  // 0x23d5e-0x23d6a
    const uint32_t lo = ld<uint32_t>(ctx, self + 0x248);  // 0x23d37
    const uint32_t r = c3fk_randomInt(ctx, seed);  // 0x23d3d-0x23d47
    const uint32_t span = ld<uint32_t>(ctx, self + 0x24c) - lo + 1;  // 0x23d4c-0x23d59
    st<uint32_t>(ctx, out + 4 * static_cast<u32>(k), (r - udiv32(r, span) * span) + lo);  // 0x23d5a-0x23d5f
    ++k;
  }
  st<uint32_t>(ctx, self + 0x244, 0);  // 0x23d6d
  return static_cast<uint32_t>(count);
}
int g_negctl_Seq_buildPlayOrder = 0;
void a_Seq_buildPlayOrder(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Seq_buildPlayOrder), Seq_buildPlayOrder_body(ctx, a[0], a[1]))); }

// ---- Seq_applyTrackFlag (rva 0x23e10): hand-fixed conversion of c3az.cpp SeqApplyTrackFlag_re
int32_t Seq_applyTrackFlag_body(Ctx &ctx, u32 self, u32 desc) {
  SndFrame fr_cells(ctx, 0x10);  // S4.2b: the cells [23] of the original's locals passed by address (U-1418)
  if (!desc) return 0xa;  // je at 0x23e1b
  const uint32_t count = snd_vcall(ctx, self, 0xb4, SndAbi::Thiscall, {}).eax;
  const uint32_t index = ld<uint32_t>(ctx, desc + 0x28);
  if (index >= count) return 0xa;  // jae at 0x23e2a
  const u32 record = fr_cells.at(0x10);  // the out slot of Seq_listGetAt
  st<u32>(ctx, record, 0);
  c3fd_listGetAt(ctx, self + 0x60, record, index);
  const uint8_t bit = static_cast<uint8_t>((ld<uint8_t>(ctx, desc + 0x2c) & 2) << 3);
  const u32 t = ld<u32>(ctx, record);
  const uint8_t kept = static_cast<uint8_t>(ld<uint8_t>(ctx, t + 0x38) & 0xef);
  st<uint8_t>(ctx, t + 0x38, static_cast<uint8_t>(bit | kept));
  st<int32_t>(ctx, t + 0x460, ld<int32_t>(ctx, desc + 0x30));
  return 0;
}
int g_negctl_Seq_applyTrackFlag = 0;
void a_Seq_applyTrackFlag(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_i32(static_cast<int32_t>(neg_u32(NEG(Seq_applyTrackFlag), static_cast<uint32_t>(Seq_applyTrackFlag_body(ctx, a[0], a[1]))))); }

// ---- Seq_dispatchChannelCmd (rva 0x24040): hand-fixed conversion of c3fd.cpp Dispatch_re
uint32_t Seq_dispatchChannelCmd_body(Ctx &ctx, u32 self, u32 cmd, uint32_t kind, int32_t value) {
  SndFrame fr_cells(ctx, 0x10);  // S4.2b: the cells [19] of the original's locals passed by address (U-1418)
  if (!cmd) return 0xa;  // 0x24048-0x2404f
  const u32 c = cmd;
  if (kind == 2) return snd_call(ctx, 0x00001550, SndAbi::Thiscall, {self, c}).eax;  // 0x2405a-0x24062
  const uint32_t t = ld<uint32_t>(ctx, cmd + 0x1c);  // 0x2406c
  if (t > 5) return 0xa;  // 0x2406f / 0x24072
  switch (t) {  // table 0x24178
    case 0:  // 0x2407f
      return snd_call(ctx, 0x00001550, SndAbi::Thiscall, {self, c}).eax;
    case 1: {  // 0x2408c
      const uint32_t n = ld<uint32_t>(ctx, cmd + 0x28);  // 0x2408f
      if (n >= snd_vcall(ctx, self, 0xb4, SndAbi::Thiscall, {}).eax) return 0xa;  // 0x24094-0x2409c
      const u32 slot = fr_cells.at(0x10);  // the cmd argument's slot
      st<u32>(ctx, slot, cmd);
      c3fd_listGetAt(ctx, self + 0x60, slot, n);  // 0x240a7
      const u32 tr = ld<u32>(ctx, slot);
      const uint8_t b = ld<uint8_t>(ctx, tr + 0x38);  // 0x240b7
      st<uint8_t>(ctx, tr + 0x38, static_cast<uint8_t>(((ld<uint8_t>(ctx, cmd + 0x2c) & 2) << 3) | (b & 0xef)));  // 0x240b0-0x240c3
      st<uint32_t>(ctx, tr + 0x460, ld<uint32_t>(ctx, cmd + 0x30));  // 0x240c6-0x240ce
      return 0;
    }
    case 2: {  // 0x240e4
      const u32 d = self + 0x264 + 0x4c * ld<uint32_t>(ctx, cmd + 0x28);  // 0x240e4-0x240f0
      for (u32 k = 4; k < 0x18; k += 4) st<uint32_t>(ctx, d + k, ld<uint32_t>(ctx, cmd + k));  // 0x240ed-0x24110
      st<uint16_t>(ctx, d + 0x18, ld<uint16_t>(ctx, cmd + 0x18));  // 0x24113 / 0x24117
      for (u32 k = 0x1c; k < 0x38; k += 4) st<uint32_t>(ctx, d + k, ld<uint32_t>(ctx, cmd + k));  // 0x2411b-0x24142
      return 0;
    }
    case 3:
    case 5: {  // 0x2414b
      if (kind) return 0xa;  // 0x2414b / 0x2414d
      if (value < 0) return 0xa;  // 0x24153 / 0x24155
      const u32 slot = fr_cells.at(0x10) + 8;  // the kind argument's slot (the kind is 0 here)
      st<u32>(ctx, slot, kind);
      const uint32_t r = c3fd_listGetAt(ctx, self + 0x60, slot, static_cast<uint32_t>(value));  // 0x24160
      if (r) return r;  // 0x24165 / 0x24167
      return snd_call(ctx, 0x00001654, SndAbi::Thiscall, {ld<u32>(ctx, slot), c}).eax;  // 0x24169-0x2416e
    }
    default:  // case 4 -> 0x24173
      return 0xa;
  }
}
int g_negctl_Seq_dispatchChannelCmd = 0;
void a_Seq_dispatchChannelCmd(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Seq_dispatchChannelCmd), Seq_dispatchChannelCmd_body(ctx, a[0], a[1], a[2], static_cast<int32_t>(a[3])))); }

// ---- Seq_getListElementAt (rva 0x241f0): hand-fixed conversion of c3cf.cpp SeqGetListElementAt_re
u32 Seq_getListElementAt_body(Ctx &ctx, u32 self, int32_t guard, int32_t index) {
  SndFrame fr_cells(ctx, 0x10);  // S4.2b: the cells [20] of the original's locals passed by address (U-1418)
  if (guard != 0) return 0;  // jne 0x24219 at 0x241f6
  if (index < 0) return 0;  // jl 0x24219 at 0x241fe
  const u32 out = fr_cells.at(0x10);  // the caller's own first argument slot (0x24201)
  st<u32>(ctx, out, static_cast<uint32_t>(guard));
  if (c3fd_listGetAt(ctx, self + 0x60, out, static_cast<uint32_t>(index)) != 0) return 0;  // jne 0x24219 at 0x24210
  return ld<u32>(ctx, out);  // 0x24212
}
int g_negctl_Seq_getListElementAt = 0;
void a_Seq_getListElementAt(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Seq_getListElementAt), Seq_getListElementAt_body(ctx, a[0], static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2])))); }

// ---- Seq_loadChannelDesc (rva 0x24280): hand-fixed conversion of c3az.cpp SeqLoadChannelDesc_re
int32_t Seq_loadChannelDesc_body(Ctx &ctx, u32 self, u32 desc) {
  if (!desc) return 0xa;  // jne at 0x24286
  st<int32_t>(ctx, self, ld<int32_t>(ctx, desc + 0));
  st<int32_t>(ctx, self + 8, ld<int32_t>(ctx, desc + 8));
  st<int32_t>(ctx, self + 0xc, ld<int32_t>(ctx, desc + 0xc));
  st<int32_t>(ctx, self + 0x10, ld<int32_t>(ctx, desc + 0x10));
  const int32_t v14 = ld<int32_t>(ctx, desc + 0x14);
  const int32_t current = ld<int32_t>(ctx, self + 4);
  st<int32_t>(ctx, self + 0x14, v14);
  const int32_t srcFlags = ld<int32_t>(ctx, desc + 4);
  const int32_t bit0 = static_cast<int32_t>(static_cast<uint32_t>(srcFlags) << 0x1f) >> 0x1f;
  const int32_t withBit0 = ((bit0 ^ current) & 1) ^ current;
  st<int32_t>(ctx, self + 4, withBit0);
  const int32_t bit1 = static_cast<int32_t>(static_cast<uint32_t>(ld<int32_t>(ctx, desc + 4)) << 0x1e) >> 0x1e;
  const int32_t withBit1 = ((bit1 ^ withBit0) & 2) ^ withBit0;
  st<int32_t>(ctx, self + 4, withBit1);
  snd_call(ctx, 0x0001ced0, SndAbi::Thiscall, {self, ld<uint32_t>(ctx, desc + 0x1c)});
  return 0xb;
}
int g_negctl_Seq_loadChannelDesc = 0;
void a_Seq_loadChannelDesc(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_i32(static_cast<int32_t>(neg_u32(NEG(Seq_loadChannelDesc), static_cast<uint32_t>(Seq_loadChannelDesc_body(ctx, a[0], a[1]))))); }

// ---- Seq_mergeChannelDesc (rva 0x24360): hand-fixed conversion of c3az.cpp SeqMergeChannelDesc_re
void Seq_mergeChannelDesc_body(Ctx &ctx, u32 self, u32 desc) {
  if (!desc) return;  // je at 0x24366
  st<int32_t>(ctx, self, ld<int32_t>(ctx, desc + 0));
  if (ld<uint8_t>(ctx, desc + 4) & 1) {  // je at 0x24372
    st<int32_t>(ctx, self + 4, static_cast<int32_t>(ld<int32_t>(ctx, self + 4) | 1));
    st<int32_t>(ctx, self + 8, ld<int32_t>(ctx, desc + 8));
    st<int32_t>(ctx, self + 0xc, ld<int32_t>(ctx, desc + 0xc));
  }
  if (ld<uint8_t>(ctx, desc + 4) & 2) {  // je at 0x24392
    st<int32_t>(ctx, self + 4, static_cast<int32_t>(ld<int32_t>(ctx, self + 4) | 2));
    st<int32_t>(ctx, self + 0x10, ld<int32_t>(ctx, desc + 0x10));
    st<int32_t>(ctx, self + 0x14, ld<int32_t>(ctx, desc + 0x14));
  }
  if (ld<uint8_t>(ctx, desc + 4) & 8) st<int32_t>(ctx, self + 4, static_cast<int32_t>(ld<int32_t>(ctx, self + 4) | 8));  // je at 0x243b0
  else st<int32_t>(ctx, self + 4, static_cast<int32_t>(ld<int32_t>(ctx, self + 4) & static_cast<int32_t>(0xfffffff7u)));
  const uint32_t complement = ~static_cast<uint32_t>(ld<int32_t>(ctx, desc + 4));
  if (((complement >> 4) & 1) != 0) st<int32_t>(ctx, self + 4, static_cast<int32_t>(ld<int32_t>(ctx, self + 4) & static_cast<int32_t>(0xffffffefu)));  // jne at 0x243ca
  else st<int32_t>(ctx, self + 4, static_cast<int32_t>(ld<int32_t>(ctx, self + 4) | 0x10));
  const u32 name = ld<u32>(ctx, desc + 0x1c);
  if (name) snd_call(ctx, 0x0001ced0, SndAbi::Thiscall, {self, name});  // je at 0x243dc
}
int g_negctl_Seq_mergeChannelDesc = 0;
void a_Seq_mergeChannelDesc(Ctx &ctx, const uint32_t *a, Ret &) { Seq_mergeChannelDesc_body(ctx, a[0], a[1]); neg_mem(ctx, NEG(Seq_mergeChannelDesc), a[0]); }

// ---- Seq_selectTrack (rva 0x24500): hand-fixed conversion of c3ex.cpp SelectTrack_re
uint32_t Seq_selectTrack_body(Ctx &ctx, u32 self, uint32_t data, uint32_t i) {
  SndFrame fr_cells(ctx, 0x10);  // S4.2b: the cells [21] of the original's locals passed by address (U-1418)
  const u32 cell = fr_cells.at(0x10);  // the slots of `cur` (+0) and `t` (+4)
  st<u32>(ctx, cell, 0);
  if (c3fd_listGetAt(ctx, self + 0x60, cell, ld<uint32_t>(ctx, self + 0x20c))) st<u32>(ctx, cell, 0);  // 0x24519-0x24522
  const u32 cur = ld<u32>(ctx, cell);
  st<u32>(ctx, cell + 4, 0);
  const uint32_t r = c3fd_listGetAt(ctx, self + 0x60, cell + 4, i);  // 0x24536
  if (r) return r;  // 0x2453d
  const u32 t = ld<u32>(ctx, cell + 4);
  snd_call(ctx, 0x00002298, SndAbi::Thiscall, {t, data});  // 0x24548
  st<uint32_t>(ctx, t + 0x34, ld<uint32_t>(ctx, t + 0x34) & 0xfffffffbu);  // 0x24551-0x24557
  if (!cur || ld<uint32_t>(ctx, cur + 0x54) < ld<uint32_t>(ctx, t + 0x54))  // 0x2455e-0x2456c
    st<uint32_t>(ctx, self + 0x20c, i);  // 0x2456e
  snd_call(ctx, 0x00001956, SndAbi::Thiscall, {self, i});  // 0x24577
  return 0;  // 0x2457c
}
int g_negctl_Seq_selectTrack = 0;
void a_Seq_selectTrack(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Seq_selectTrack), Seq_selectTrack_body(ctx, a[0], a[1], a[2]))); }

// ---- Seq_resetTrackControllers (rva 0x24630): hand-fixed conversion of c3cf.cpp SeqResetTrackControllers_re
int32_t Seq_resetTrackControllers_body(Ctx &ctx, u32 self, uint32_t index) {
  SndFrame fr_cells(ctx, 0x10);  // S4.2b: the cells [22] of the original's locals passed by address (U-1418)
  const u32 rc_cell = fr_cells.at(0x10);  // the out slot of Seq_listGetAt
  st<u32>(ctx, rc_cell, 0);
  const int32_t rc = static_cast<int32_t>(c3fd_listGetAt(ctx, self + 0x60, rc_cell, index));  // 0x2463d
  if (rc != 0) return rc;  // jne 0x24675 at 0x24644
  const u32 record = ld<u32>(ctx, rc_cell);
  st<uint8_t>(ctx, record + 0x38, static_cast<uint8_t>(ld<uint8_t>(ctx, record + 0x38) | 1));  // 0x24650
  for (int off = 0x254; off >= 0x58; off -= 4) {  // 0x2464b, 0x2466a-0x24670
    const u32 ctrl = ld<u32>(ctx, record + static_cast<u32>(off));  // 0x24658
    if (!ctrl) continue;  // je 0x2466a at 0x2465d
    snd_vcall(ctx, ctrl, 0x34, SndAbi::Thiscall, {0, 0x7f, 0x19});  // call at 0x24667
  }
  return 0;  // 0x24672
}
int g_negctl_Seq_resetTrackControllers = 0;
void a_Seq_resetTrackControllers(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_i32(static_cast<int32_t>(neg_u32(NEG(Seq_resetTrackControllers), static_cast<uint32_t>(Seq_resetTrackControllers_body(ctx, a[0], a[1]))))); }

// ---- Seq_allocTrackSlot (rva 0x247c0): hand-fixed conversion of c3ew.cpp SeqAlloc_re
uint32_t Seq_allocTrackSlot_body(Ctx &ctx, u32 self) {
  SndFrame fr_cells(ctx, 0x10);  // S4.2b: the cells [18] of the original's locals passed by address (U-1418)
  const u32 s = self;
  const int32_t count = ld<int32_t>(ctx, s + 0x23c);  // 0x247c4
  if (count == 0) return static_cast<uint32_t>(count);  // 0x247ca -> 0x24888 (eax = [+0x23c] = 0)
  const u32 ctr = snd_obj(ctx, &SoundState::g_trackRoundRobin).va();
  uint32_t c = ld<uint32_t>(ctx, ctr) + 1;  // 0x247d2-0x247d8
  if (!(static_cast<int32_t>(c) < ld<int32_t>(ctx, s + 0x23c))) c = 0;  // 0x247e3 / 0x247e5 / 0x247e7-0x247e9
  st<uint32_t>(ctx, ctr, c);
  uint32_t idx = ld<uint32_t>(ctx, ld<u32>(ctx, s + 0x238) + c * 4);  // 0x247ee / 0x247f9
  const u32 track = fr_cells.at(0x10);  // the local out slot [esp+0xc]
  st<u32>(ctx, track, 0);
  uint32_t r = snd_call(ctx, 0x000015fa, SndAbi::Thiscall, {s + 0x60, track, idx}).eax;  // 0x24804
  if (r == 0) {  // 0x24809 / 0x2480b
    if (ld<int32_t>(ctx, s + 0x23c) > 0) {  // 0x2480d / 0x2481a
      int32_t ebp = 1;  // 0x24813
      for (;;) {
        const u32 t = ld<u32>(ctx, track);  // 0x2481c
        if (!(ld<uint8_t>(ctx, t + 0x38) & 4)) {  // 0x24820 / 0x24824
          st<uint8_t>(ctx, t + 0x38, static_cast<uint8_t>(ld<uint8_t>(ctx, t + 0x38) | 4));  // 0x2486b
          break;
        }
        c = ld<uint32_t>(ctx, ctr) + 1;  // 0x24826-0x2482c
        if (!(static_cast<int32_t>(c) < ld<int32_t>(ctx, s + 0x23c))) c = 0;  // 0x24837-0x2483d
        st<uint32_t>(ctx, ctr, c);
        idx = ld<uint32_t>(ctx, ld<u32>(ctx, s + 0x238) + c * 4);  // 0x24842 / 0x2484c
        r = snd_call(ctx, 0x000015fa, SndAbi::Thiscall, {s + 0x60, track, idx}).eax;  // 0x24853
        if (r != 0) break;  // 0x24858 / 0x2485a
        const int32_t eax = ebp;  // 0x24862
        ebp++;  // 0x24864
        if (!(eax < ld<int32_t>(ctx, s + 0x23c))) break;  // 0x24865 / 0x24867
      }
    }
  }
  return snd_call(ctx, 0x0000240a, SndAbi::Thiscall, {s, ld<u32>(ctx, track), ld<uint32_t>(ctx, s + 0x71c), 0x20, 0}).eax;  // 0x2487f
}
int g_negctl_Seq_allocTrackSlot = 0;
void a_Seq_allocTrackSlot(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Seq_allocTrackSlot), Seq_allocTrackSlot_body(ctx, a[0]))); }

// ---- Seq_scheduleEvent (rva 0x24b80): hand-fixed conversion of c3bi.cpp SeqScheduleEvent_re
void Seq_scheduleEvent_body(Ctx &ctx, u32 self, u32 track, uint32_t tick, uint32_t flags, int32_t trackIndex) {
  SndFrame fr_cells(ctx, 0x10);  // S4.2b: the cells [24] of the original's locals passed by address (U-1418)
  uint32_t period = ld<uint32_t>(ctx, track + 0x54);  // 0x24b8f
  if (trackIndex > 0) {  // jle at 0x24b92
    const u32 found = fr_cells.at(0x10);  // the out slot of Seq_listGetAt
    st<u32>(ctx, found, 0);
    if (c3fd_listGetAt(ctx, self + 0x60, found, static_cast<uint32_t>(trackIndex)) == 0)  // jne at 0x24ba4
      period = ld<uint32_t>(ctx, ld<u32>(ctx, found) + 0x54);  // 0x24baa
  }
  if (period == 0) return;  // je at 0x24baf
  if ((flags & 1) != 0) {  // je at 0x24bb9
    st<uint32_t>(ctx, track + 0x50, (period - tick % period) + tick);  // 0x24bc1-0x24bc9
  } else if ((flags & 4) != 0) {  // je at 0x24bd1
    const uint32_t step = ld<uint32_t>(ctx, self + 0x1bc);  // 0x24bd3
    st<uint32_t>(ctx, track + 0x50, (udiv32(tick, step) + 1) * step);  // 0x24bdf-0x24be5
  } else if ((flags & 0x10) != 0) {  // je at 0x24bed
    st<uint32_t>(ctx, track + 0x50, tick);  // 0x24bf3
  } else {
    st<uint32_t>(ctx, track + 0x50, 0);  // 0x24bf8
  }
  if ((flags & 0x20) != 0)  // je at 0x24c03
    snd_call(ctx, 0x00024920, SndAbi::Thiscall, {self, track, 0});  // 0x24c0a
}
int g_negctl_Seq_scheduleEvent = 0;
void a_Seq_scheduleEvent(Ctx &ctx, const uint32_t *a, Ret &) { Seq_scheduleEvent_body(ctx, a[0], a[1], a[2], a[3], static_cast<int32_t>(a[4])); neg_mem(ctx, NEG(Seq_scheduleEvent), a[1] + 0x50); }

// ---- MidiDeviceBase_ctor (rva 0x29e70): converted from c3dj.cpp MidiBaseCtor_re
u32 MidiDeviceBase_ctor_body(Ctx &ctx, u32 self) {
    u32 o = self;
    st<uint32_t>(ctx, o + 4, 0);  // 0x29e79
    st<uint32_t>(ctx, o + 8, 0x7f);  // 0x29e7b
    st<uint32_t>(ctx, o + 0xc, 0);  // 0x29e82
    st<uint32_t>(ctx, o + 0x18, 0);  // 0x29e85
    st<uint32_t>(ctx, o + 0x1c, 0);  // 0x29e88
    st<uint32_t>(ctx, o + 0x10, 0);  // 0x29e8b
    st<uint32_t>(ctx, o + 0x14, 0);  // 0x29e8e
    st<u32>(ctx, o, snd_va(ctx, snd_rva::vtbl_MidiDeviceBase));  // 0x29e91
    return self;  // 0x29e97
}
int g_negctl_MidiDeviceBase_ctor = 0;
void a_MidiDeviceBase_ctor(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(MidiDeviceBase_ctor), MidiDeviceBase_ctor_body(ctx, a[0]))); }

// ---- MidiDeviceBase_scalarDtor (rva 0x29f30): converted from c3dj.cpp MidiDeviceBase_scalarDtor_re
u32 MidiDeviceBase_scalarDtor_body(Ctx &ctx, u32 self, uint32_t flags) {
    snd_call(ctx, 0x00001b22, SndAbi::Thiscall, {self});
if (flags & 1) snd_delete(ctx, self);
return self;
}
int g_negctl_MidiDeviceBase_scalarDtor = 0;
void a_MidiDeviceBase_scalarDtor(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(MidiDeviceBase_scalarDtor), MidiDeviceBase_scalarDtor_body(ctx, a[0], a[1]))); }

// ---- MidiDeviceBase_openInner (rva 0x29f90): hand-fixed conversion of c3cl.cpp MidiDeviceBaseOpenInner_re
int32_t MidiDeviceBase_openInner_body(Ctx &ctx, u32 self, int32_t a, int32_t b) {
  const int32_t rc = static_cast<int32_t>(snd_vcall(ctx, self, 4, SndAbi::Thiscall, {static_cast<uint32_t>(b)}).eax);  // call at 0x29f9b
  if (rc) return rc;  // jne 0x29fcd at 0x29fa0
  const u32 inner = ld<u32>(ctx, self + 0x14);  // 0x29fa2
  const int32_t rc2 = static_cast<int32_t>(snd_vcall(ctx, inner, 0x10, SndAbi::Thiscall, {static_cast<uint32_t>(a), static_cast<uint32_t>(b)}).eax);  // call at 0x29fad
  if (!rc2) return 0;  // je 0x29fcb at 0x29fb4
  snd_vcall(ctx, self, 8, SndAbi::Thiscall, {});  // call at 0x29fba
  st<u32>(ctx, self + 0x14, 0);  // 0x29fbf
  return rc2;  // 0x29fbd
}
int g_negctl_MidiDeviceBase_openInner = 0;
void a_MidiDeviceBase_openInner(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_i32(static_cast<int32_t>(neg_u32(NEG(MidiDeviceBase_openInner), static_cast<uint32_t>(MidiDeviceBase_openInner_body(ctx, a[0], static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2])))))); }

// ---- MidiDeviceBase_closeInner (rva 0x2a000): hand-fixed conversion of c3cl.cpp MidiDeviceBaseCloseInner_re
int32_t MidiDeviceBase_closeInner_body(Ctx &ctx, u32 self) {
  const u32 inner = ld<u32>(ctx, self + 0x14);  // 0x2a003
  if (inner) {  // je 0x2a016 at 0x2a008
    snd_vcall(ctx, inner, 0x14, SndAbi::Thiscall, {});  // call at 0x2a00c
    st<u32>(ctx, self + 0x14, 0);  // 0x2a00f
  }
  return 0;  // 0x2a016
}
int g_negctl_MidiDeviceBase_closeInner = 0;
void a_MidiDeviceBase_closeInner(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_i32(static_cast<int32_t>(neg_u32(NEG(MidiDeviceBase_closeInner), static_cast<uint32_t>(MidiDeviceBase_closeInner_body(ctx, a[0]))))); }

// ---- Seq_applyChannelLevel (rva 0x2b260): hand-fixed conversion of c3ca.cpp SeqApplyChannelLevel_re
void Seq_applyChannelLevel_body(Ctx &ctx, u32 self, int32_t level) {
  const int32_t masked = level & 0x7f;  // 0x2b264
  st<int32_t>(ctx, self + 0x04, masked & 0x7f);  // and edx, 0x7f + store at 0x2b269
  const u32 inner = ld<u32>(ctx, self + 0x40);  // 0x2b26f
  if (inner != 0) snd_vcall(ctx, inner, 0x40, SndAbi::Thiscall, {static_cast<uint32_t>(masked)});  // 0x2b276-0x2b279
}
int g_negctl_Seq_applyChannelLevel = 0;
void a_Seq_applyChannelLevel(Ctx &ctx, const uint32_t *a, Ret &) { Seq_applyChannelLevel_body(ctx, a[0], static_cast<int32_t>(a[1])); neg_mem(ctx, NEG(Seq_applyChannelLevel), a[0] + 4); }

// ---- Track_setField8RaisingMax (rva 0x2e430): converted from c3cl.cpp TrackSetField8RaisingMax_re
void Track_setField8RaisingMax_body(Ctx &ctx, u32 self, uint32_t value) {
    if (ld<uint32_t>(ctx, self + 0xc) < value) {  // cmp / jae at 0x2e437, 0x2e439
        st<uint32_t>(ctx, self + 0xc, value);  // 0x2e43b
    }
    st<uint32_t>(ctx, self + 8, value);  // 0x2e43e
}
int g_negctl_Track_setField8RaisingMax = 0;
void a_Track_setField8RaisingMax(Ctx &ctx, const uint32_t *a, Ret &) { Track_setField8RaisingMax_body(ctx, a[0], a[1]); neg_mem(ctx, NEG(Track_setField8RaisingMax), a[0] + 0x8); }

// ---- Midi_Device::openOutput (rva 0x9da0): written from c3dh.cpp OpenOut_re and the instructions
uint32_t Midi_Device__openOutput_body(Ctx &ctx, u32 self, uint32_t a1, uint32_t a2) {
  (void)a1;
  if ((a2 & 2) != 0) {  // 0x9da5
    const unsigned r = snd_import(ctx, 0x000b85f8, SndAbi::Stdcall, {self + 0x3c, 0xffffffffu, 0, 0, 0}).eax;  // 0x9db7
    if (r != 0) return 7;  // 0x9dbd / 0x9dc1
  }
  if (gref(ctx, 0xb49ec) != 0) {  // 0x9dca / 0x9dcf
    st<u32>(ctx, self + 0x2c, gref(ctx, 0xb49ec));  // 0x9dd3
    snd_call(ctx, 0x0000227a, SndAbi::Thiscall, {gref(ctx, 0xb49ec), self});  // 0x9ddd
  }
  return 0;  // 0x9de2
}
int g_negctl_Midi_Device__openOutput = 0;
void a_Midi_Device__openOutput(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Midi_Device__openOutput), Midi_Device__openOutput_body(ctx, a[0], a[1], a[2]))); }

// ---- Midi_Device::flushMessages (rva 0x1ef10): written from c3dh.cpp FlushMsg_re
void Midi_Device__flushMessages_body(Ctx &ctx, u32 self) {
  const u32 dev = gref(ctx, 0xb4a1c);  // 0x1ef10
  if (ld<u32>(ctx, dev + 0x3c) == 0) return;  // 0x1ef15 / 0x1ef18
  u32 p = self + 0x258;  // 0x1ef25
  for (int i = 0; i < 0x80; i++, p += 4) {  // 0x1ef2b / 0x1ef51 / 0x1ef52
    if (ld<uint8_t>(ctx, p) == 0) continue;  // 0x1ef30 / 0x1ef33
    st<uint8_t>(ctx, p + 2, 0);  // 0x1ef35
    const u32 d2 = gref(ctx, 0xb4a1c);  // 0x1ef39
    const u32 msg = ld<u32>(ctx, p);  // 0x1ef3f
    snd_import(ctx, 0x000b8638, SndAbi::Stdcall, {ld<u32>(ctx, d2 + 0x3c), msg});  // 0x1ef46
    st<u32>(ctx, p, 0);  // 0x1ef48
  }
}
int g_negctl_Midi_Device__flushMessages = 0;
void a_Midi_Device__flushMessages(Ctx &ctx, const uint32_t *a, Ret &) { Midi_Device__flushMessages_body(ctx, a[0]); neg_mem(ctx, NEG(Midi_Device__flushMessages), a[0] + 0x258); }

// ---- Midi_scanTrack (rva 0x1ef80): written from c3bd.cpp MidiScanTrack_re
// The inline variable-length reader of Midi_scanTrack (0x1efa2-0x1efb5 and 0x1f011-0x1f024): the value, the bytes it took.
u32 c3bd_inlineVarLen(Ctx &ctx, u32 p, u32 &bytes) {
  u32 value = 0, n = 0;
  uint8_t b;
  do {
    b = ld<uint8_t>(ctx, p);
    value = (value << 7) | (b & 0x7fu);  // shl / or at 0x1efac
    ++n;  // inc ebx at 0x1efb1
    ++p;  // inc edi at 0x1efb2
  } while ((b & 0x80u) != 0);  // jne at 0x1efb5
  bytes = n;
  return value;
}
int32_t Midi_scanTrack_body(Ctx &ctx, u32 self) {
  u32 p = ld<u32>(ctx, self + 0x3c);  // 0x1ef85
  const u32 table = snd_va(ctx, 0x1f06c);
  u32 total = 0;  // [esp+0x10], 0x1ef90
  u32 status = 0;  // eax, 0x1ef88
  bool finished = false;  // [esp+0x14], 0x1ef98
  for (;;) {
    u32 bytes = 0;
    const u32 delta = c3bd_inlineVarLen(ctx, p, bytes);
    const u32 next = ld<uint8_t>(ctx, p + bytes);  // 0x1efbb
    p += bytes;  // 0x1efbf
    total += delta;  // 0x1efc1
    if ((next & 0x80u) != 0) {  // je at 0x1efca
      const u32 high = next & 0xf0u;  // 0x1efd4
      status = (high == 0xf0u) ? next : high;  // je at 0x1efe0
      ++p;  // 0x1efe4
    }
    const u32 index = status - 0x80u;  // lea at 0x1efe5
    if (index > 0x7fu) return 0;  // ja at 0x1efeb (UNSIGNED)
    switch (ld<uint8_t>(ctx, table + index)) {
      case 2:  // 0x1effc
        if (ld<uint8_t>(ctx, p) == 0x2f) finished = true;  // 0x1f000-0x1f002
        ++p;  // 0x1f00a
        {
          u32 metaBytes = 0;
          const u32 length = c3bd_inlineVarLen(ctx, p, metaBytes);
          p += length + metaBytes;  // 0x1f026-0x1f028
        }
        break;
      case 1: p += 1; break;  // 0x1f02c
      case 0: p += 2; break;  // 0x1f02f
      default: return 0;  // 0x1f051
    }
    if (finished) break;  // je at 0x1f038
  }
  st<u32>(ctx, self + 0x54, total);  // 0x1f049
  return static_cast<int32_t>(total);
}
int g_negctl_Midi_scanTrack = 0;
void a_Midi_scanTrack(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_i32(static_cast<int32_t>(neg_u32(NEG(Midi_scanTrack), static_cast<uint32_t>(Midi_scanTrack_body(ctx, a[0]))))); }

// ---- Midi_seekTrackToTick (rva 0x24920): written from c3bd.cpp MidiSeekTrackToTick_re
int32_t Midi_seekTrackToTick_body(Ctx &ctx, u32 self, u32 rec, uint32_t limit) {
  SndFrame fr_cells(ctx, 0x10);  // S4.2b: the cells [10] of the original's locals passed by address (U-1418)
  u32 p = ld<u32>(ctx, rec + 0x3c);  // 0x2492d
  const u32 table = snd_va(ctx, 0x24a78);
  const u32 cell = fr_cells.at(0x10);  // the byte-count slots: +0 the delta's, +4 a meta length's
  u32 consumed = 0;  // edi, 0x2492b
  u32 status = 0;  // ebx, 0x24930
  u32 ticks = 0;  // [esp+0x14], 0x24936
  bool started = false;  // [esp+0x20], 0x2493e
  for (;;) {
    st<u32>(ctx, cell, 0);  // [esp+0x10], 0x2493a
    const u32 delta = snd_call(ctx, 0x0000139d, SndAbi::Cdecl, {p, cell}).eax;  // call at 0x24948
    const u32 bytes = ld<u32>(ctx, cell);
    const u32 before = ticks;  // ecx, 0x2494d
    const u32 total = delta + before;  // add at 0x24955
    if (total > limit) {  // ja at 0x2495c (UNSIGNED)
      if (!started) return 0x29;  // 0x24a19
      st<u32>(ctx, rec + 0x40, consumed);  // 0x24a4d
      st<u32>(ctx, rec + 0x2c, p);  // 0x24a50
      st<u32>(ctx, rec + 0x4c, before);  // 0x24a54
      return 0;  // 0x24a59
    }
    ticks = total;  // 0x24962
    const u32 eventStart = p;  // 0x2496a
    p += bytes;  // 0x2496c
    consumed += bytes;  // 0x2496e
    started = true;  // 0x24970
    const u32 head = ld<uint8_t>(ctx, p);  // 0x24975
    if ((head & 0x80u) != 0) {  // je at 0x24979
      const u32 high = head & 0xf0u;  // 0x24982
      status = head;  // 0x24980
      st<uint8_t>(ctx, rec + 0x30, static_cast<uint8_t>(head));  // 0x2498c
      if (high != 0xf0u) {  // je at 0x2498f
        st<u32>(ctx, rec + 0x48, head & 0xfu);  // 0x24994
        status = high;  // 0x24997
      }
      ++p;  // 0x24999
      ++consumed;  // 0x2499a
    }
    const u32 index = status - 0x80u;  // lea at 0x2499b
    if (index > 0x7fu) return 0x12;  // ja at 0x249a1 (UNSIGNED)
    switch (ld<uint8_t>(ctx, table + index)) {
      case 3:  // 0x249b6
        if (ld<uint8_t>(ctx, p) == 0x2f) {  // je at 0x249b9
          st<u32>(ctx, rec + 0x2c, eventStart);  // 0x24a2a
          st<u32>(ctx, rec + 0x4c, ticks);  // 0x24a2d
          return 0x29;  // 0x24a32
        }
        ++p;  // 0x249bf
        {
          st<u32>(ctx, cell + 4, 0);
          const u32 length = snd_call(ctx, 0x0000139d, SndAbi::Cdecl, {p, cell + 4}).eax;  // call at 0x249c2
          const u32 metaBytes = ld<u32>(ctx, cell + 4);
          p += length + metaBytes;  // 0x249d1
          consumed += length + metaBytes + 1;  // lea at 0x249d5
        }
        break;
      case 1:  // 0x249e5
        if (ld<uint8_t>(ctx, p) == 7) {  // jne at 0x249e8
          const u32 channel = ld<u32>(ctx, rec + 0x48);  // 0x249ea
          const u32 value = ld<uint8_t>(ctx, p + 1);  // 0x249ef
          st<u32>(ctx, self + 0x254 + channel * 19u * 4u, value);  // 0x249fc
        }
        p += 2;  // 0x24a03
        consumed += 2;  // 0x24a06
        break;
      case 2:  // 0x249de
        p += 1;
        consumed += 1;
        break;
      case 0:  // 0x24a03
        p += 2;
        consumed += 2;
        break;
      default: return 0x12;  // 0x24a3e
    }
  }
}
int g_negctl_Midi_seekTrackToTick = 0;
void a_Midi_seekTrackToTick(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_i32(static_cast<int32_t>(neg_u32(NEG(Midi_seekTrackToTick), static_cast<uint32_t>(Midi_seekTrackToTick_body(ctx, a[0], a[1], a[2]))))); }

// ---- Seq_rewind (rva 0x1e1c0): written from c3ar.cpp SeqRewind_re
uint32_t Seq_rewind_body(Ctx &ctx, u32 self) {
  u32 node = ld<u32>(ctx, self + 0x19c);  // 0x1e1c3
  st<u32>(ctx, self + 0x1a4, node);  // 0x1e1cb
  if (node != 0) {  // je 0x1e1d1
    u32 track = ld<u32>(ctx, node + 8);  // 0x1e1d3
    if (track != 0) {  // je 0x1e1d8
      for (;;) {
        snd_call(ctx, 0x00001ca3, SndAbi::Thiscall, {track});  // 0x1e1dc
        node = ld<u32>(ctx, self + 0x1a4);
        if (node == 0) break;  // je 0x1e1e9
        node = ld<u32>(ctx, node + 4);  // 0x1e1eb
        st<u32>(ctx, self + 0x1a4, node);  // 0x1e1f0
        if (node == 0) break;  // je 0x1e1f6
        track = ld<u32>(ctx, node + 8);  // 0x1e1f8
        if (track == 0) break;  // jne 0x1e1fd closes the loop
      }
    }
  }
  st<u32>(ctx, self + 0x4c, ld<u32>(ctx, self + 0x4c) & 0xffffff63u);  // and al, 0x63 at 0x1e202, stored at 0x1e204
  return 0;  // xor eax, eax at 0x1e207
}
int g_negctl_Seq_rewind = 0;
void a_Seq_rewind(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Seq_rewind), Seq_rewind_body(ctx, a[0]))); }

// ---- Seq_getTrackFlagHigh (rva 0x23f10): written from c3cf.cpp SeqGetTrackFlagHigh_re
int32_t Seq_getTrackFlagHigh_body(Ctx &ctx, u32 self, uint32_t index) {
  SndFrame fr_cells(ctx, 0x10);  // S4.2b: the cells [8] of the original's locals passed by address (U-1418)
  const uint32_t count = snd_vcall(ctx, self, 0xb4, SndAbi::Thiscall, {}).eax;  // 0x23f16
  if (index > count) return static_cast<int32_t>(count & 0xffffff00u);  // xor al, al at 0x23f24
  const u32 rec = fr_cells.at(0x10);  // the out slot of Seq_listGetAt (0x23f2c)
  st<u32>(ctx, rec, 0);
  const uint32_t rc = snd_call(ctx, 0x000015fa, SndAbi::Thiscall, {self + 0x60, rec, index}).eax;  // 0x23f34, eax = the code
  const uint32_t bit = (ld<uint8_t>(ctx, ld<u32>(ctx, rec) + 0x38) >> 4) & 1u;  // 0x23f3e-0x23f44
  return static_cast<int32_t>((rc & 0xffffff00u) | bit);
}
int g_negctl_Seq_getTrackFlagHigh = 0;
void a_Seq_getTrackFlagHigh(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_i32(static_cast<int32_t>(neg_u32(NEG(Seq_getTrackFlagHigh), static_cast<uint32_t>(Seq_getTrackFlagHigh_body(ctx, a[0], a[1]))))); }

// ---- Midi_Device::emitEvent (rva 0x246a0): written from c3fd.cpp Emit_re
uint32_t Midi_Device__emitEvent_body(Ctx &ctx, u32 self, uint32_t index) {
  SndFrame fr_cells(ctx, 0x10);  // S4.2b: the cells [9] of the original's locals passed by address (U-1418)
  const u32 cell = fr_cells.at(0x10);  // the argument slot the original passes as the out slot (0x246a6)
  st<u32>(ctx, cell, index);
  const uint32_t r = snd_call(ctx, 0x000015fa, SndAbi::Thiscall, {self + 0x60, cell, index}).eax;  // 0x246af
  if (r) return r;  // 0x246b4 / 0x246b6
  const u32 t = ld<u32>(ctx, cell);
  if (ld<u32>(ctx, gref(ctx, 0xb4a1c) + 0x3c)) {  // 0x246bc-0x246c6
    for (int off = 0x258; off < 0x458; off += 4) {  // 0x246d2 / 0x24704-0x2470d
      if (!ld<uint8_t>(ctx, t + static_cast<u32>(off))) continue;  // 0x246d7 / 0x246db
      st<uint8_t>(ctx, t + static_cast<u32>(off) + 2, 0);  // 0x246dd
      snd_import(ctx, 0x000b8638, SndAbi::Stdcall, {ld<u32>(ctx, gref(ctx, 0xb4a1c) + 0x3c), ld<u32>(ctx, t + static_cast<u32>(off))});  // 0x246e2-0x246f3
      st<u32>(ctx, t + static_cast<u32>(off), 0);  // 0x246f9
    }
    return 0;  // 0x24710
  }
  for (int off = 0x58; off < 0x258; off += 4) {  // 0x2471c / 0x24769-0x24772
    const u32 s = ld<u32>(ctx, t + static_cast<u32>(off));  // 0x24721
    if (!s) continue;  // 0x24724 / 0x24726
    st<u32>(ctx, t + static_cast<u32>(off), 0);  // 0x24728
    snd_vcall(ctx, s, 0x28, SndAbi::Thiscall, {});  // 0x24733
    if (ld<int32_t>(ctx, s + 0x1438) > 1) {  // 0x24736-0x2473f
      for (int j = 0; j < sub32(ld<int32_t>(ctx, s + 0x1438), 1); ++j) {  // 0x24741-0x24746, 0x24756-0x24763
        const u32 m = ld<u32>(ctx, s + 0x1440 + 4 * static_cast<u32>(j));  // 0x2474e
        snd_vcall(ctx, m, 0x28, SndAbi::Thiscall, {});  // 0x24753
      }
    }
  }
  return 0;  // 0x24775
}
int g_negctl_Midi_Device__emitEvent = 0;
void a_Midi_Device__emitEvent(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Midi_Device__emitEvent), Midi_Device__emitEvent_body(ctx, a[0], a[1]))); }

// ---- Midi_Device::handleMetaEvent (rva 0x21bf0): written from c3fk.cpp MetaEv_re
uint32_t Midi_Device__handleMetaEvent_body(Ctx &ctx, u32 self, u32 p, u32 track) {
  SndFrame fr_cells(ctx, 0x10);  // S4.2b: the cells [11] of the original's locals passed by address (U-1418)
  const u32 nc = fr_cells.at(0x10);  // the byte-count slot: the argument slot of p in the original (0x21c0a)
  const u32 type = ld<uint8_t>(ctx, p);  // 0x21c03
  const u32 len = snd_call(ctx, 0x0000139d, SndAbi::Cdecl, {p + 1, nc}).eax;  // 0x21bfd-0x21c0a
  const u32 n = ld<u32>(ctx, nc);
  const u32 data = p + n + 1;  // 0x21c0f / 0x21c22
  switch (type) {  // 0x21c17-0x21c34
    case 0x51: {
      u32 t = 0;
      for (int i = 2, sh = 0; i >= 0; --i, sh += 8) t |= static_cast<u32>(ld<uint8_t>(ctx, data + static_cast<u32>(i))) << sh;  // 0x21c3b-0x21c51
      const double q = static_cast<double>(ld<float>(ctx, snd_va(ctx, 0x5b82c))) / static_cast<double>(static_cast<int64_t>(t));  // 0x21c5f / 0x21c65
      const u32 bpm = static_cast<u32>(ftol_low32(q));  // 0x21c6b
      snd_vcall(ctx, self, 0x84, SndAbi::Thiscall, {bpm});  // 0x21c70-0x21c73
      break;
    }
    case 0x59:
      st<u32>(ctx, self + 0x73c, snd_call(ctx, 0x00001b2c, SndAbi::Cdecl, {ld<uint16_t>(ctx, p + 2)}).eax & 0xffff);  // 0x21c7e-0x21c90
      break;
    case 0x2f:
      if (!(ld<uint8_t>(ctx, track + 0x38) & 0x10)) {  // 0x21c9f / 0x21ca3
        st<u32>(ctx, track + 0x34, ld<u32>(ctx, track + 0x34) | 2);  // 0x21da3-0x21db1
        if (const u32 t2 = ld<u32>(ctx, track + 0x458)) st<u32>(ctx, t2 + 0x34, ld<u32>(ctx, t2 + 0x34) - 1);  // 0x21da6 / 0x21db4-0x21db6
        const uint8_t f = ld<uint8_t>(ctx, track + 0x38);  // 0x21db9
        if (f & 2) {  // 0x21dbc / 0x21dbe
          if (f & 4) snd_call(ctx, 0x000024cd, SndAbi::Thiscall, {self});  // 0x21dc0-0x21dc6
          snd_call(ctx, 0x00001ca3, SndAbi::Thiscall, {track});  // 0x21dcb / 0x21dcd
        }
        break;
      }
      snd_call(ctx, 0x00001ca3, SndAbi::Thiscall, {track});  // 0x21ca9 / 0x21cab
      snd_call(ctx, 0x000011cc, SndAbi::Thiscall, {self, track});  // 0x21cb0-0x21cb3
      st<u32>(ctx, track + 0x50, ld<u32>(ctx, self + 0x71c));  // 0x21cb8 / 0x21cc0
      {
        const u32 r = snd_vcall(ctx, self, 0x9c, SndAbi::Thiscall, {ld<u32>(ctx, self + 0x738)}).eax;  // 0x21cc3-0x21ccc
        if (!r || !ld<u32>(ctx, r + 0x14)) break;  // 0x21cd4-0x21cdf
        st<u32>(ctx, r + 0x24, ld<u32>(ctx, r + 0x24) + 1);  // 0x21ce5-0x21ceb
        if (ld<u32>(ctx, r + 0x24) < snd_call(ctx, 0x0000255e, SndAbi::Thiscall, {r}).eax) break;  // 0x21cee-0x21cf6
        st<u32>(ctx, r + 0x24, 0);  // 0x21d03
        st<u32>(ctx, r + 0x28, ld<u32>(ctx, r + 0x28) + 1);  // 0x21cfc-0x21d08
        if (ld<u32>(ctx, r + 0x28) < ld<u32>(ctx, r + 0x14)) break;  // 0x21cff / 0x21d0b / 0x21d0d
        st<u32>(ctx, r + 0x28, 0);  // 0x21d16
        st<u32>(ctx, self + 0x738, ld<u32>(ctx, r + 0x18));  // 0x21d13 / 0x21d19
        if (!ld<u32>(ctx, self + 0x224)) break;  // 0x21d1f-0x21d27
        u32 cur = ld<u32>(ctx, self + 0x218);  // 0x21d2d
        st<u32>(ctx, self + 0x220, cur);  // 0x21d35
        if (!cur) break;  // 0x21d3b
        u32 pl = ld<u32>(ctx, cur + 8);  // 0x21d41
        if (!pl) break;  // 0x21d46
        while (ld<u32>(ctx, pl + 4) != ld<u32>(ctx, self + 0x738)) {  // 0x21d4c-0x21d57
          cur = ld<u32>(ctx, self + 0x220);  // 0x21d59
          if (!cur) return n + len + 1;  // 0x21d61
          cur = ld<u32>(ctx, cur + 4);  // 0x21d63
          st<u32>(ctx, self + 0x220, cur);  // 0x21d68
          if (!cur) return n + len + 1;  // 0x21d6e
          pl = ld<u32>(ctx, cur + 8);  // 0x21d70
          if (!pl) return n + len + 1;  // 0x21d75
        }
        if (const u32 v = ld<u32>(ctx, pl + 0x1c)) snd_vcall(ctx, self, 0x84, SndAbi::Thiscall, {v});  // 0x21d7d-0x21d89
        snd_vcall(ctx, self, 0x94, SndAbi::Thiscall, {ld<u32>(ctx, pl + 8), ld<u32>(ctx, pl + 0xc)});  // 0x21d8f-0x21d9b
      }
      break;
    default:  // 0x58 and every other type: 0x21dd2
      break;
  }
  return n + len + 1;  // 0x21dd2-0x21ddd
}
int g_negctl_Midi_Device__handleMetaEvent = 0;
void a_Midi_Device__handleMetaEvent(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Midi_Device__handleMetaEvent), Midi_Device__handleMetaEvent_body(ctx, a[0], a[1], a[2]))); }


const FuncDef kFuncs[] = {
    {"snd.Midi_Device::scalarDeletingDtor", "thiscall", "pointer", 2, 0, a_Midi_Device__scalarDeletingDtor, &g_negctl_Midi_Device__scalarDeletingDtor},
    {"snd.Midi_Device::dtor", "thiscall", "void", 1, 0, a_Midi_Device__dtor, &g_negctl_Midi_Device__dtor},
    {"snd.Midi_Device::shutdown", "thiscall", "uint32", 1, 0, a_Midi_Device__shutdown, &g_negctl_Midi_Device__shutdown},
    {"snd.Midi_Device::enableOutput", "thiscall", "uint32", 1, 0, a_Midi_Device__enableOutput, &g_negctl_Midi_Device__enableOutput},
    {"snd.Midi_Device::disableOutput", "thiscall", "uint32", 1, 0, a_Midi_Device__disableOutput, &g_negctl_Midi_Device__disableOutput},
    {"snd.Midi_Device::drainCommandQueue", "thiscall", "uint32", 1, 0, a_Midi_Device__drainCommandQueue, &g_negctl_Midi_Device__drainCommandQueue},
    {"snd.Midi_Device::dispatchCommand", "thiscall", "uint32", 1, 0, a_Midi_Device__dispatchCommand, &g_negctl_Midi_Device__dispatchCommand},
    {"snd.Midi_Device::scanCommandRing", "thiscall", "uint32", 3, 0, a_Midi_Device__scanCommandRing, &g_negctl_Midi_Device__scanCommandRing},
    {"snd.Seq_freeChannelObjects", "thiscall", "void", 1, 0, a_Seq_freeChannelObjects, &g_negctl_Seq_freeChannelObjects},
    {"snd.Midi_advanceMsgRing", "thiscall", "void", 1, 0, a_Midi_advanceMsgRing, &g_negctl_Midi_advanceMsgRing},
    {"snd.Seq_setPlayRange", "thiscall", "void", 3, 0, a_Seq_setPlayRange, &g_negctl_Seq_setPlayRange},
    {"snd.Seq_firstTrackData", "thiscall", "uint32", 1, 0, a_Seq_firstTrackData, &g_negctl_Seq_firstTrackData},
    {"snd.Midi_resetFileState", "thiscall", "void", 1, 0, a_Midi_resetFileState, &g_negctl_Midi_resetFileState},
    {"snd.Seq_listGetAt", "thiscall", "int", 3, 0, a_Seq_listGetAt, &g_negctl_Seq_listGetAt},
    {"snd.Midi_Device::loadFile", "thiscall", "uint32", 2, 0, a_Midi_Device__loadFile, &g_negctl_Midi_Device__loadFile},
    {"snd.Midi_Device::loadFromMemory", "thiscall", "uint32", 2, 0, a_Midi_Device__loadFromMemory, &g_negctl_Midi_Device__loadFromMemory},
    {"snd.Track_resetReadState", "thiscall", "void", 1, 0, a_Track_resetReadState, &g_negctl_Track_resetReadState},
    {"snd.Seq_dtor", "thiscall", "void", 1, 0, a_Seq_dtor, &g_negctl_Seq_dtor},
    {"snd.Midi_Device::closeFile", "thiscall", "uint32", 1, 0, a_Midi_Device__closeFile, &g_negctl_Midi_Device__closeFile},
    {"snd.Track_clearFlag8", "thiscall", "int", 1, 0, a_Track_clearFlag8, &g_negctl_Track_clearFlag8},
    {"snd.Seq_scaleTickRate", "thiscall", "int", 2, 0, a_Seq_scaleTickRate, &g_negctl_Seq_scaleTickRate},
    {"snd.Midi_readVarLen", "default", "uint32", 2, 0, a_Midi_readVarLen, &g_negctl_Midi_readVarLen},
    {"snd.Seq_rewindTracks", "thiscall", "void", 1, 0, a_Seq_rewindTracks, &g_negctl_Seq_rewindTracks},
    {"snd.Track_setData", "thiscall", "uint32", 2, 0, a_Track_setData, &g_negctl_Track_setData},
    {"snd.Track_advancePos", "thiscall", "uint32", 2, 0, a_Track_advancePos, &g_negctl_Track_advancePos},
    {"snd.Seq_clearTrackFlag8All", "thiscall", "void", 1, 0, a_Seq_clearTrackFlag8All, &g_negctl_Seq_clearTrackFlag8All},
    {"snd.Seq::loadFile", "thiscall", "uint32", 2, 0, a_Seq__loadFile, &g_negctl_Seq__loadFile},
    {"snd.Midi_loadMemoryAndPreRoll", "thiscall", "uint32", 3, 0, a_Midi_loadMemoryAndPreRoll, &g_negctl_Midi_loadMemoryAndPreRoll},
    {"snd.Seq_closeFile", "thiscall", "uint32", 1, 0, a_Seq_closeFile, &g_negctl_Seq_closeFile},
    {"snd.Seq_preRollTrack", "thiscall", "uint32", 2, 0, a_Seq_preRollTrack, &g_negctl_Seq_preRollTrack},
    {"snd.Seq_stepTrackMeta", "thiscall", "uint32", 2, 0, a_Seq_stepTrackMeta, &g_negctl_Seq_stepTrackMeta},
    {"snd.Midi_Device::playEvent", "thiscall", "uint32", 3, 0, a_Midi_Device__playEvent, &g_negctl_Midi_Device__playEvent},
    {"snd.Seq_setField730Clamped", "thiscall", "void", 2, 0, a_Seq_setField730Clamped, &g_negctl_Seq_setField730Clamped},
    {"snd.Seq_setupChannel", "thiscall", "uint32", 3, 0, a_Seq_setupChannel, &g_negctl_Seq_setupChannel},
    {"snd.Midi_Device::setChannelVolume", "thiscall", "int", 4, 0, a_Midi_Device__setChannelVolume, &g_negctl_Midi_Device__setChannelVolume},
    {"snd.Midi_Device::noteOn", "thiscall", "uint32", 4, 0, a_Midi_Device__noteOn, &g_negctl_Midi_Device__noteOn},
    {"snd.Midi_Device::sendShortMsg", "thiscall", "uint32", 4, 0, a_Midi_Device__sendShortMsg, &g_negctl_Midi_Device__sendShortMsg},
    {"snd.Seq_setTempo", "thiscall", "uint32", 2, 0x027f, a_Seq_setTempo, &g_negctl_Seq_setTempo},
    {"snd.Seq_updateChannel", "thiscall", "uint32", 2, 0, a_Seq_updateChannel, &g_negctl_Seq_updateChannel},
    {"snd.Seq_reset", "thiscall", "int", 1, 0, a_Seq_reset, &g_negctl_Seq_reset},
    {"snd.Seq_copyChannelParams", "thiscall", "int", 2, 0, a_Seq_copyChannelParams, &g_negctl_Seq_copyChannelParams},
    {"snd.Seq_setPlayOrderMode", "thiscall", "uint32", 2, 0, a_Seq_setPlayOrderMode, &g_negctl_Seq_setPlayOrderMode},
    {"snd.Seq_setField240", "thiscall", "int", 2, 0, a_Seq_setField240, &g_negctl_Seq_setField240},
    {"snd.Seq_removeTrackById", "thiscall", "uint32", 2, 0, a_Seq_removeTrackById, &g_negctl_Seq_removeTrackById},
    {"snd.Seq_buildTrack9", "thiscall", "uint32", 2, 0, a_Seq_buildTrack9, &g_negctl_Seq_buildTrack9},
    {"snd.Seq_buildTrack14", "thiscall", "uint32", 2, 0, a_Seq_buildTrack14, &g_negctl_Seq_buildTrack14},
    {"snd.Seq_findTrackOrZero", "thiscall", "pointer", 2, 0, a_Seq_findTrackOrZero, &g_negctl_Seq_findTrackOrZero},
    {"snd.Seq_findTrackById", "thiscall", "int", 3, 0, a_Seq_findTrackById, &g_negctl_Seq_findTrackById},
    {"snd.Seq_cmdHandler7", "thiscall", "uint32", 1, 0, a_Seq_cmdHandler7, &g_negctl_Seq_cmdHandler7},
    {"snd.Seq_cmdHandler8", "thiscall", "uint32", 2, 0, a_Seq_cmdHandler8, &g_negctl_Seq_cmdHandler8},
    {"snd.Seq_remarkEventRange", "thiscall", "int", 2, 0, a_Seq_remarkEventRange, &g_negctl_Seq_remarkEventRange},
    {"snd.Seq_cmdHandler9", "thiscall", "uint32", 2, 0, a_Seq_cmdHandler9, &g_negctl_Seq_cmdHandler9},
    {"snd.Seq_replayEventRange", "thiscall", "uint32", 3, 0, a_Seq_replayEventRange, &g_negctl_Seq_replayEventRange},
    {"snd.Seq_cmdHandler10", "thiscall", "int", 3, 0, a_Seq_cmdHandler10, &g_negctl_Seq_cmdHandler10},
    {"snd.Seq_cmdHandler12", "thiscall", "uint32", 3, 0, a_Seq_cmdHandler12, &g_negctl_Seq_cmdHandler12},
    {"snd.Seq_cmdHandler11", "thiscall", "int", 3, 0, a_Seq_cmdHandler11, &g_negctl_Seq_cmdHandler11},
    {"snd.Seq_buildPlayOrder", "thiscall", "uint32", 2, 0, a_Seq_buildPlayOrder, &g_negctl_Seq_buildPlayOrder},
    {"snd.Seq_applyTrackFlag", "thiscall", "int", 2, 0, a_Seq_applyTrackFlag, &g_negctl_Seq_applyTrackFlag},
    {"snd.Seq_dispatchChannelCmd", "thiscall", "uint32", 4, 0, a_Seq_dispatchChannelCmd, &g_negctl_Seq_dispatchChannelCmd},
    {"snd.Seq_getListElementAt", "thiscall", "pointer", 3, 0, a_Seq_getListElementAt, &g_negctl_Seq_getListElementAt},
    {"snd.Seq_loadChannelDesc", "thiscall", "int", 2, 0, a_Seq_loadChannelDesc, &g_negctl_Seq_loadChannelDesc},
    {"snd.Seq_mergeChannelDesc", "thiscall", "void", 2, 0, a_Seq_mergeChannelDesc, &g_negctl_Seq_mergeChannelDesc},
    {"snd.Seq_selectTrack", "thiscall", "uint32", 3, 0, a_Seq_selectTrack, &g_negctl_Seq_selectTrack},
    {"snd.Seq_resetTrackControllers", "thiscall", "int", 2, 0, a_Seq_resetTrackControllers, &g_negctl_Seq_resetTrackControllers},
    {"snd.Seq_allocTrackSlot", "thiscall", "uint32", 1, 0, a_Seq_allocTrackSlot, &g_negctl_Seq_allocTrackSlot},
    {"snd.Seq_scheduleEvent", "thiscall", "void", 5, 0, a_Seq_scheduleEvent, &g_negctl_Seq_scheduleEvent},
    {"snd.MidiDeviceBase_ctor", "thiscall", "pointer", 1, 0, a_MidiDeviceBase_ctor, &g_negctl_MidiDeviceBase_ctor},
    {"snd.MidiDeviceBase_scalarDtor", "thiscall", "pointer", 2, 0, a_MidiDeviceBase_scalarDtor, &g_negctl_MidiDeviceBase_scalarDtor},
    {"snd.MidiDeviceBase_openInner", "thiscall", "int", 3, 0, a_MidiDeviceBase_openInner, &g_negctl_MidiDeviceBase_openInner},
    {"snd.MidiDeviceBase_closeInner", "thiscall", "int", 1, 0, a_MidiDeviceBase_closeInner, &g_negctl_MidiDeviceBase_closeInner},
    {"snd.Seq_applyChannelLevel", "thiscall", "void", 2, 0, a_Seq_applyChannelLevel, &g_negctl_Seq_applyChannelLevel},
    {"snd.Track_setField8RaisingMax", "thiscall", "void", 2, 0, a_Track_setField8RaisingMax, &g_negctl_Track_setField8RaisingMax},
    {"snd.Midi_Device::openOutput", "thiscall", "uint32", 3, 0, a_Midi_Device__openOutput, &g_negctl_Midi_Device__openOutput},
    {"snd.Midi_Device::flushMessages", "thiscall", "void", 1, 0, a_Midi_Device__flushMessages, &g_negctl_Midi_Device__flushMessages},
    {"snd.Midi_scanTrack", "thiscall", "int", 1, 0, a_Midi_scanTrack, &g_negctl_Midi_scanTrack},
    {"snd.Midi_seekTrackToTick", "thiscall", "int", 3, 0, a_Midi_seekTrackToTick, &g_negctl_Midi_seekTrackToTick},
    {"snd.Seq_rewind", "thiscall", "uint32", 1, 0, a_Seq_rewind, &g_negctl_Seq_rewind},
    {"snd.Seq_getTrackFlagHigh", "thiscall", "int", 2, 0, a_Seq_getTrackFlagHigh, &g_negctl_Seq_getTrackFlagHigh},
    {"snd.Midi_Device::emitEvent", "thiscall", "uint32", 2, 0, a_Midi_Device__emitEvent, &g_negctl_Midi_Device__emitEvent},
    {"snd.Midi_Device::handleMetaEvent", "thiscall", "uint32", 3, 0, a_Midi_Device__handleMetaEvent, &g_negctl_Midi_Device__handleMetaEvent},
};

}  // namespace

extern const ModuleDef kModule_audio_seq = {"audio_seq", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim

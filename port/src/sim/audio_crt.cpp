// sound.dll's static C runtime and four helpers the loaders reach (step S4.11, log/roadmap/S4/11_notes.md; API in
// portsim/sim/audio.h): the functions every S4 module used to hand to the development fallback. Written from the instructions of
// the user's sound.dll (there is no hook source for most of them); judged by the emulated original (port/test/sound_replay.py, the
// keys of test_audio_crt.py). Addresses in comments are RVAs of the user's sound.dll.
//
//   heap     operator_new 0x424d7 and _malloc 0x42acd over __nh_malloc 0x42adf (the new handler 0x45b80), heap_alloc 0x42b0b, free
//            0x42c07, operator_delete 0x4249a, __lock 0x47710 / _unlock 0x47771. The system-heap arm (heap type 1, the only one
//            this build runs: g_heapType 0xb5d20 is set by the CRT's start-up) allocates through the HeapAlloc / HeapFree
//            imports (snd_import: with a host the emulator's allocator answers, with none the AudioSink receives
//            `KERNEL32!HeapAlloc`); the small-block-heap arms (types 2 and 3) reach their callees through the dispatcher.
//   exit     _atexit 0x42647 over __onexit 0x425c9 (the table at g_onexitBegin / g_onexitEnd 0xb5d3c / 0xb5d38) and its lock pair
//            0x45226 / 0x4522f.
//   string   _strrchr 0x424b0, strstr 0x42690 (the unrolled original, over-read included), _sprintf 0x42577.
//   array    ehVectorConstructorIterator 0x42902 (normal path: a constructor that throws would unwind through the SEH
//            frame, which the port does not model; __ArrayUnwind 0x4288e is ported on its own).
//   helpers  GlblHeader::init 0x2e3c0, Sound::getField1b8 0x5f40, MixBuffer::deletingDtor 0x2cf50, Sound::queueMidiCommand3 0x20480,
//            ChannelDesc_dtor 0x244a0, VoiceBase_initListSlot 0x29b80 (the element constructor / destructor the vector iterators get)
//            (the four callees of loadSoundFile and Seq_applyTrackFlag that had no names row: re/names/sound_19.tsv).
//
// Every call to code that is not inline goes through the dispatcher (snd_call / snd_vcall / snd_call_va / snd_import), so a
// fixture's stand-in, the host, another port function or the AudioSink serves it.
//
// Registration: "snd.<name>" for every function (a name without a hooks.csv row yet is in re/names/sound_19.tsv). The core finds
// the ones its rva -> name table does not know through audio_crt_function_name() below.
//
// Negative controls (sg_negctl_set("snd.<name>", mode)): mode 1 is one wrong detail per function (documented at each body).
#include <cstdint>
#include <cstring>

#include "portsim/ptr.h"
#include "portsim/registry.h"
#include "portsim/sim/audio.h"
#include "portsim/sim/soundstate.h"

namespace portsim {

namespace {
using u32 = uint32_t;

// ---- the module's CRT globals (rvas of .data) and imports (rvas of the IAT slots)
constexpr u32 kHeapHandle = 0xb5d10, kHeapType = 0xb5d20, kSbhLimit3 = 0xb5d0c, kSbhLimit2 = 0x673fc;
constexpr u32 kNewMode = 0xb5588, kNewHandler = 0xb558c, kOnexitEnd = 0xb5d38, kOnexitBegin = 0xb5d3c;
constexpr u32 kLockTable = 0x67400;
constexpr u32 kHeapAlloc = 0xb84e8, kHeapFree = 0xb84ec, kHeapReAlloc = 0xb851c, kHeapSize = 0xb8520, kLeaveCriticalSection = 0xb84e0, kInitCriticalSection = 0xb83f0,
              kEnterCriticalSection = 0xb83f4;
constexpr u32 kMidiDevice = 0xb4a1c;          // g_midiDevice
// code the functions reach by rva
constexpr u32 kRvaMalloc = 0x42acd, kRvaNhMalloc = 0x42adf, kRvaHeapAlloc = 0x42b0b, kRvaFree = 0x42c07,
              kRvaCallnewh = 0x45b80, kRvaLock = 0x47710, kRvaUnlock = 0x47771, kRvaAmsgExit = 0x4319b,
              kRvaOnexit = 0x425c9, kRvaMsize = 0x45581, kRvaRealloc = 0x45252, kRvaLockExit = 0x45226,
              kRvaUnlockExit = 0x4522f, kRvaSbhAlloc = 0x46283, kRvaSbhFind = 0x45f2f, kRvaSbhFree = 0x45f5a,
              kRvaSbhAlloc2 = 0x47148, kRvaSbhFind2 = 0x470ac, kRvaSbhFree2 = 0x47103, kRvaOperatorDelete = 0x4249a;
constexpr u32 kThunkQueuePush = 0x15e1;       // CommandQueue::push's incremental-link thunk (the call at 0x20493)
constexpr u32 kVtMixBuffer = 0x5be84;         // vtbl_MixBuffer
constexpr u32 kThunkCtrlObjectDtorC = 0x1ab9;  // CtrlObject_dtorC's incremental-link thunk (the jmp at 0x244a3)

// negative-control switches (mode 1 = one wrong detail; each body says which)
#define CRT_NEG(v) int g_negctl_##v = 0;
CRT_NEG(operator_delete) CRT_NEG(free) CRT_NEG(heap_alloc) CRT_NEG(nh_malloc) CRT_NEG(malloc) CRT_NEG(operator_new)
CRT_NEG(callnewh) CRT_NEG(lock) CRT_NEG(unlock) CRT_NEG(lockexit) CRT_NEG(unlockexit) CRT_NEG(onexit) CRT_NEG(atexit)
CRT_NEG(msize) CRT_NEG(crtRealloc) CRT_NEG(strrchr) CRT_NEG(strstr) CRT_NEG(ArrayUnwind) CRT_NEG(ehVectorDestructorIterator) CRT_NEG(ehVectorConstructorIterator) CRT_NEG(GlblHeader_init)
CRT_NEG(Sound_getField1b8) CRT_NEG(MixBuffer_deletingDtor) CRT_NEG(Sound_queueMidiCommand3) CRT_NEG(ChannelDesc_dtor) CRT_NEG(sprintf) CRT_NEG(Ch5bb90_set3DBuffer) CRT_NEG(VoiceBase_initListSlot)
#undef CRT_NEG
#define NEG(v) SG_NEGCTL(g_negctl_##v)

inline u32 G(Ctx &c, u32 rva) { return snd_va(c, rva); }
inline u32 rd(Ctx &c, u32 rva) { return ld<u32>(c, snd_va(c, rva)); }
inline void wr(Ctx &c, u32 rva, u32 v) { st<u32>(c, snd_va(c, rva), v); }

// ================================================================================================ the locks
// __lock 0x47710 (cdecl, the lock number): the critical section of table slot n, created on first use under lock 0x11.
void a_lock(Ctx &c, const u32 *a, Ret &r) {
  const u32 slot = G(c, kLockTable) + 4 * a[0];
  if (ld<u32>(c, slot) == 0) {                                                       // 0x47717
    const u32 cs = snd_call(c, kRvaMalloc, SndAbi::Cdecl, {0x18}).eax;               // 0x4772b
    if (cs == 0) snd_call(c, kRvaAmsgExit, SndAbi::Cdecl, {0x11});                   // 0x47739
    snd_call(c, kRvaLock, SndAbi::Cdecl, {0x11});                                    // 0x47741
    if (ld<u32>(c, slot) == 0) {                                                     // 0x47746
      snd_import(c, kInitCriticalSection, SndAbi::Stdcall, {cs});                    // 0x4774d
      st<u32>(c, slot, cs);                                                          // 0x47753
    } else {
      snd_call(c, kRvaFree, SndAbi::Cdecl, {cs});                                    // 0x47757
    }
    snd_call(c, kRvaUnlock, SndAbi::Cdecl, {0x11});                                  // 0x4775f
  }
  const u32 section = NEG(lock) == 1 ? 0 : ld<u32>(c, slot);                         // 0x47766
  r.set_u32(snd_import(c, kEnterCriticalSection, SndAbi::Stdcall, {section}).eax);   // 0x47768
}

// _unlock 0x47771: LeaveCriticalSection(table slot n)
void a_unlock(Ctx &c, const u32 *a, Ret &r) {
  const u32 section = ld<u32>(c, G(c, kLockTable) + 4 * a[0]);                       // 0x47777
  r.set_u32(snd_import(c, kLeaveCriticalSection, SndAbi::Stdcall, {NEG(unlock) == 1 ? section + 4 : section}).eax);
}

// 0x45226 / 0x4522f: lock / unlock number 0xd (the exit table's lock)
void a_lockexit(Ctx &c, const u32 *, Ret &r) {
  r.set_u32(snd_call(c, kRvaLock, SndAbi::Cdecl, {NEG(lockexit) == 1 ? 0xeu : 0xdu}).eax);  // 0x45226
}
void a_unlockexit(Ctx &c, const u32 *, Ret &r) {
  r.set_u32(snd_call(c, kRvaUnlock, SndAbi::Cdecl, {NEG(unlockexit) == 1 ? 0xeu : 0xdu}).eax);  // 0x4522f
}

// ================================================================================================ the heap
// heap_alloc 0x42b0b (cdecl, the size): by the heap type [0xb5d20]: 3 and 2 the small-block heaps (0x46283 / 0x47148 under lock 9),
// anything else (and a failed small-block request) HeapAlloc(heap, 0, size rounded up to 16). eax = the block, 0 on failure.
void a_heap_alloc(Ctx &c, const u32 *a, Ret &r) {
  const u32 type = rd(c, kHeapType);
  u32 n = a[0];
  auto system = [&](u32 size) {
    const u32 handle = rd(c, kHeapHandle);
    return snd_import(c, kHeapAlloc, SndAbi::Stdcall, {handle, 0, size}).eax;        // 0x42bea .. 0x42bf2
  };
  if (type == 3) {                                                                   // 0x42b33
    if (n <= rd(c, kSbhLimit3)) {                                                    // 0x42b3b
      snd_call(c, kRvaLock, SndAbi::Cdecl, {9});                                     // 0x42b49
      const u32 p = snd_call(c, kRvaSbhAlloc, SndAbi::Cdecl, {n}).eax;               // 0x42b54
      snd_call(c, kRvaUnlock, SndAbi::Cdecl, {9});                                   // 0x42b61 (the funclet 0x42b72 is `push 9; call _unlock`)
      if (p) return r.set_u32(p);                                                    // 0x42b6b
    }
  } else if (type == 2) {                                                            // 0x42b7b
    n = n ? ((n + 0xf) & 0xfffffff0u) : 0x10;                                        // 0x42b80 .. 0x42b92
    if (n <= rd(c, kSbhLimit2)) {                                                    // 0x42b95
      snd_call(c, kRvaLock, SndAbi::Cdecl, {9});                                     // 0x42b9f
      const u32 p = snd_call(c, kRvaSbhAlloc2, SndAbi::Cdecl, {n >> 4}).eax;         // 0x42bb2
      snd_call(c, kRvaUnlock, SndAbi::Cdecl, {9});                                   // 0x42bbf (the funclet 0x42bd1)
      if (p) return r.set_u32(p);                                                    // 0x42bc7
    }
    return r.set_u32(system(n));                                                     // 0x42bcb
  }
  n = n ? n : 1;                                                                     // 0x42bda .. 0x42be9
  n = NEG(heap_alloc) == 1 ? ((n + 7) & 0xfffffff8u) : ((n + 0xf) & 0xfffffff0u);    // control: rounds to 8
  r.set_u32(system(n));
}

// __callnewh 0x45b80 (cdecl, the size): the new handler [0xb558c], when set, is called with the size; 1 when it returns non-zero
void a_callnewh(Ctx &c, const u32 *a, Ret &r) {
  const u32 handler = rd(c, kNewHandler);                                            // 0x45b80
  if (handler != 0) {
    const u32 v = snd_call_va(c, handler, SndAbi::Cdecl, {a[0]}).eax;                // 0x45b8d
    if (v != 0) return r.set_u32(NEG(callnewh) == 1 ? 0u : 1u);                      // 0x45b94
  }
  r.set_u32(0);                                                                      // 0x45b98
}

// __nh_malloc 0x42adf (cdecl, size, flag): the allocation loop; flag 0 gives up after one failure, else the new handler decides
void a_nh_malloc(Ctx &c, const u32 *a, Ret &r) {
  const u32 n = a[0], flag = a[1];
  if (n > 0xffffffe0u) return r.set_u32(0);                                          // 0x42adf .. 0x42ae4
  for (;;) {
    const u32 p = snd_call(c, kRvaHeapAlloc, SndAbi::Cdecl, {n}).eax;                // 0x42aea
    if (p != 0) return r.set_u32(p);                                                 // 0x42aef
    if (flag == 0 && NEG(nh_malloc) != 1) return r.set_u32(0);                       // 0x42af4
    if (snd_call(c, kRvaCallnewh, SndAbi::Cdecl, {n}).eax == 0) return r.set_u32(0);  // 0x42afe
  }
}

// _malloc 0x42acd (cdecl, size): __nh_malloc(size, g_newMode)
void a_malloc(Ctx &c, const u32 *a, Ret &r) {
  const u32 mode = NEG(malloc) == 1 ? 1u : rd(c, kNewMode);                          // 0x42acd
  r.set_u32(snd_call(c, kRvaNhMalloc, SndAbi::Cdecl, {a[0], mode}).eax);             // 0x42ad7
}

// operator_new 0x424d7 (cdecl, size): __nh_malloc(size, 1)
void a_operator_new(Ctx &c, const u32 *a, Ret &r) {
  const u32 n = NEG(operator_new) == 1 ? a[0] + 1 : a[0];
  r.set_u32(snd_call(c, kRvaNhMalloc, SndAbi::Cdecl, {n, 1}).eax);                   // 0x424dd
}

// free 0x42c07 (cdecl, the block): nothing for NULL; heap type 3 / 2 hand the block to their small-block heap under lock 9 (a block
// that is not theirs goes on to HeapFree); else HeapFree(heap, 0, block). eax is not defined (the SEH chain head for NULL,
// HeapFree's result on the system path), so the entry is declared void.
void a_free(Ctx &c, const u32 *a, Ret &r) {
  const u32 p = a[0];
  if (p == 0) return;                                                                // 0x42c2d
  const u32 type = rd(c, kHeapType);
  auto system = [&](u32 block) {
    const u32 handle = rd(c, kHeapHandle);
    r.set_u32(snd_import(c, kHeapFree, SndAbi::Stdcall, {handle, 0, NEG(free) == 1 ? 0u : block}).eax);  // 0x42cd5 .. 0x42cdb
  };
  if (type == 3) {                                                                   // 0x42c3a
    snd_call(c, kRvaLock, SndAbi::Cdecl, {9});                                       // 0x42c41
    const u32 region = snd_call(c, kRvaSbhFind, SndAbi::Cdecl, {p}).eax;             // 0x42c4c
    if (region) snd_call(c, kRvaSbhFree, SndAbi::Cdecl, {region, p});                // 0x42c5b
    snd_call(c, kRvaUnlock, SndAbi::Cdecl, {9});                                     // 0x42c66 (the funclet 0x42c71)
    if (region) return;                                                              // 0x42c6b / 0x42cc2
    return system(p);
  }
  if (type == 2) {                                                                   // 0x42c7d
    SndFrame f(c, 0x38);
    snd_call(c, kRvaLock, SndAbi::Cdecl, {9});                                       // 0x42c81
    const u32 hit = snd_call(c, kRvaSbhFind2, SndAbi::Cdecl, {p, f.at(0x2c), f.at(0x24)}).eax;  // 0x42c97
    if (hit) snd_call(c, kRvaSbhFree2, SndAbi::Cdecl, {ld<u32>(c, f.at(0x2c)), ld<u32>(c, f.at(0x24)), hit});  // 0x42cad
    snd_call(c, kRvaUnlock, SndAbi::Cdecl, {9});                                     // 0x42cb9 (the funclet 0x42cc9)
    if (hit) return;                                                                 // 0x42cc2
    return system(p);
  }
  system(p);                                                                         // 0x42cd2
}

// operator_delete 0x4249a (cdecl, the block): free(block); eax is free's, so the entry is void too
void a_operator_delete(Ctx &c, const u32 *a, Ret &r) {
  r = snd_call(c, kRvaFree, SndAbi::Cdecl, {NEG(operator_delete) == 1 ? 0u : a[0]});     // 0x4249e
}


// msize 0x45581 (cdecl, the block): the block's size. Heap type 3 / 2 read it from their small-block headers (0x45f2f / 0x470ac:
// not ported, see below); the system heap asks HeapSize(heap, 0, block).
void a_msize(Ctx &c, const u32 *a, Ret &r) {
  const u32 type = rd(c, kHeapType);
  if (type == 3 || type == 2)                                                        // 0x455a9 / 0x455f4
    throw MissingResource{"msize: the small-block heap arms (heap type 2 / 3) are not ported: crtHeapSelect picks the system heap"};
  const u32 handle = rd(c, kHeapHandle);
  r.set_u32(snd_import(c, kHeapSize, SndAbi::Stdcall, {handle, 0, NEG(msize) == 1 ? a[0] + 8 : a[0]}).eax);  // 0x4563f .. 0x4564a
}

// crtRealloc 0x45252 (cdecl, block, size): NULL block = _malloc(size); size 0 = free(block) and 0; the system heap arm rounds the size up
// to 16 and calls HeapReAlloc(heap, 0, block, size) (0x4a460 copies in the small-block arms, which are not ported); a failure with
// g_newMode set asks the new handler and retries.
void a_crtRealloc(Ctx &c, const u32 *a, Ret &r) {
  const u32 block = a[0];
  u32 n = a[1];
  if (block == 0) return r.set_u32(snd_call(c, kRvaMalloc, SndAbi::Cdecl, {n}).eax);   // 0x4527e
  if (n == 0) {                                                                       // 0x4528f
    snd_call(c, kRvaFree, SndAbi::Cdecl, {block});                                    // 0x45294
    return r.set_u32(0);                                                              // 0x45570
  }
  const u32 type = rd(c, kHeapType);
  if (type == 3 || type == 2)                                                         // 0x452a4 / 0x453e6
    throw MissingResource{"crtRealloc: the small-block heap arms (heap type 2 / 3) are not ported: crtHeapSelect picks the system heap"};
  for (;;) {                                                                          // 0x45536
    u32 got = 0;
    if (n <= 0xffffffe0u) {                                                           // 0x45538
      if (n == 0) n = 1;                                                              // 0x4553d
      n = (n + 0xf) & 0xfffffff0u;                                                    // 0x45544
      const u32 handle = rd(c, kHeapHandle);
      got = snd_import(c, kHeapReAlloc, SndAbi::Stdcall, {handle, 0, block, NEG(crtRealloc) == 1 ? n - 0x10 : n}).eax;  // 0x45553
    }
    if (got != 0) return r.set_u32(got);                                              // 0x45559
    if (rd(c, kNewMode) == 0) return r.set_u32(got);                                  // 0x4555d
    if (snd_call(c, kRvaCallnewh, SndAbi::Cdecl, {n}).eax == 0) return r.set_u32(0);  // 0x45566
  }
}

// ================================================================================================ exit table
// __onexit 0x425c9 (cdecl, the function): append it to the table [0xb5d3c, 0xb5d38) under lock 0xd, growing the table by 0x10 bytes
// (realloc) when it is full; eax = the function, 0 when the growth failed.
void a_onexit(Ctx &c, const u32 *a, Ret &r) {
  snd_call(c, kRvaLockExit, SndAbi::Cdecl, {});                                      // 0x425ca
  u32 begin = rd(c, kOnexitBegin);
  const u32 size = snd_call(c, kRvaMsize, SndAbi::Cdecl, {begin}).eax;               // 0x425d5
  begin = rd(c, kOnexitBegin);                                                       // 0x425da
  u32 end = rd(c, kOnexitEnd);                                                       // 0x425e1
  const u32 need = end - begin + 4;                                                  // 0x425e9 .. 0x425eb
  u32 result = 0;
  bool ok = true;
  if (size < need) {                                                                 // 0x425ee
    const u32 size2 = snd_call(c, kRvaMsize, SndAbi::Cdecl, {begin}).eax;            // 0x425f3
    const u32 grown = snd_call(c, kRvaRealloc, SndAbi::Cdecl, {begin, size2 + 0x10}).eax;  // 0x42602
    if (grown == 0) {                                                                // 0x4260a
      ok = false;
      result = 0;
    } else {
      const u32 used = end - rd(c, kOnexitBegin);                                    // 0x42612
      wr(c, kOnexitBegin, grown);                                                    // 0x4261e
      end = grown + (static_cast<u32>(static_cast<int32_t>(used) >> 2)) * 4;         // 0x42623 .. 0x42629
      wr(c, kOnexitEnd, end);                                                        // 0x42629
    }
  }
  if (ok) {
    st<u32>(c, end, NEG(onexit) == 1 ? 0u : a[0]);                                   // 0x42633
    wr(c, kOnexitEnd, rd(c, kOnexitEnd) + 4);                                        // 0x42635
    result = a[0];                                                                   // 0x4263c
  }
  snd_call(c, kRvaUnlockExit, SndAbi::Cdecl, {});                                    // 0x4263e
  r.set_u32(result);
}

// _atexit 0x42647 (cdecl, the function): 0 when __onexit took it, -1 otherwise
void a_atexit(Ctx &c, const u32 *a, Ret &r) {
  const u32 v = snd_call(c, kRvaOnexit, SndAbi::Cdecl, {a[0]}).eax;                  // 0x4264b
  u32 out = v == 0 ? 0xffffffffu : 0u;                                               // neg / sbb / neg / dec 0x42650 .. 0x42657
  if (NEG(atexit) == 1) out = ~out;
  r.set_u32(out);
}

// ================================================================================================ strings
// _strrchr 0x424b0 (cdecl, string, character): the last occurrence of the low byte of `character` in the string, the terminating
// NUL included (repne scasb up, then down with the direction flag set); 0 when there is none.
void a_strrchr(Ctx &c, const u32 *a, Ret &r) {
  const u32 s = a[0];
  const uint8_t ch = static_cast<uint8_t>(a[1]);                                     // 0x424c2
  u32 len = 0;
  while (ld<uint8_t>(c, s + len) != 0) ++len;                                        // 0x424bc
  u32 i = len + 1;                                                                   // ecx after the neg: length with the NUL
  u32 p = s + len;                                                                   // edi after the dec
  while (i != 0) {                                                                   // 0x424c6: repne scasb, backwards
    const uint8_t b = ld<uint8_t>(c, p);
    --p;
    --i;
    if (b == ch) break;
  }
  p += 1;                                                                            // 0x424c8
  const bool hit = ld<uint8_t>(c, p) == ch;                                          // 0x424c9
  if (NEG(strrchr) == 1) return r.set_u32(hit ? s : 0);                              // control: the first occurrence's start
  r.set_u32(hit ? p : 0);                                                            // 0x424d1 / 0x424cd
}

// strstr 0x42690 (cdecl, string, pattern): the unrolled original. An empty pattern gives the string, a one-character pattern is
// strchr; otherwise the first character is searched (0x426a8-0x426c2: a NUL ends the search with 0), then the second (0x426ca), then
// the rest of the pattern two characters per round (0x426d4-0x426f5). A failed round resumes one character after the match start.
void a_strstr(Ctx &c, const u32 *a, Ret &r) {
  const u32 hay = a[0], pat = a[1];
  auto B = [&](u32 va) { return ld<uint8_t>(c, va); };
  const uint8_t dl = B(pat);                                                         // 0x42697
  if (dl == 0) return r.set_u32(hay);                                                // 0x4269d -> 0x4270a
  const uint8_t dh = B(pat + 1);                                                     // 0x426a1
  if (dh == 0) {                                                                     // 0x426a4 -> 0x426f7: strchr(string, first)
    for (u32 p = hay;; ++p) {
      const uint8_t b = B(p);
      if (b == dl) return r.set_u32(p);
      if (b == 0) return r.set_u32(0);
    }
  }
  u32 edi = hay;
  for (;;) {
    u32 esi = edi;                                                                   // 0x426a8
    u32 ecx = pat;                                                                   // 0x426aa
    uint8_t al = B(edi);                                                             // 0x426ae
    esi += 1;                                                                        // 0x426b0
    for (;;) {                                                                       // 0x426b1 / 0x426bc: compare, NUL test, next
      if (al == dl) break;                                                           // 0x426b3 / 0x426be
      if (al == 0) return r.set_u32(0);                                              // 0x426b7 / 0x426c2
      al = B(esi);                                                                   // 0x426b9
      esi += 1;                                                                      // 0x426bb
    }
    for (;;) {
      al = B(esi);                                                                   // 0x426ca
      esi += 1;                                                                      // 0x426cc
      if (al != dh) {                                                                // 0x426cd -> 0x426bc
        for (;;) {                                                                   // the first-character loop again, from 0x426bc
          if (al == dl) break;
          if (al == 0) return r.set_u32(0);
          al = B(esi);
          esi += 1;
        }
        continue;                                                                    // back to 0x426ca
      }
      break;
    }
    edi = esi - 1;                                                                   // 0x426d1
    bool matched = false;
    for (;;) {                                                                       // 0x426d4
      uint8_t ah = B(ecx + 2);
      if (ah == 0) return r.set_u32(NEG(strstr) == 1 ? edi : edi - 1);               // 0x426d9 -> 0x42703
      al = B(esi);                                                                   // 0x426db
      esi += 2;                                                                      // 0x426dd
      if (al != ah) break;                                                           // 0x426e0 -> 0x426a8
      al = B(ecx + 3);                                                               // 0x426e4
      if (al == 0) return r.set_u32(edi - 1);                                        // 0x426e7 -> 0x42703
      ah = B(esi - 1);                                                               // 0x426eb
      ecx += 2;                                                                      // 0x426ee
      if (al != ah) break;                                                           // 0x426f1 -> 0x426f5 -> 0x426a8
      matched = true;
    }
    (void)matched;
  }
}

// ================================================================================================ arrays
// __ArrayUnwind 0x4288e (stdcall, ptr, size, count, destructor): the elements before ptr + 0 .. count, last first, each with the
// destructor (thiscall); the SEH frame of the original only guards the destructor calls.
void a_ArrayUnwind(Ctx &c, const u32 *a, Ret &) {
  u32 p = a[0];
  const u32 size = a[1];
  int32_t count = static_cast<int32_t>(a[2]);
  for (;;) {
    --count;                                                                         // 0x428b7
    if (count < 0) return;                                                           // 0x428ba
    p -= (NEG(ArrayUnwind) == 1 ? 0u : size);                                        // 0x428bf
    snd_call_va(c, a[3], SndAbi::Thiscall, {p});                                     // 0x428c5
  }
}

// ehVectorDestructorIterator 0x4280e (stdcall, ptr, size, count, destructor; `ret 0x10`): the destructor (thiscall) on count elements
// `size` bytes apart, last first, starting at ptr + size * count. The SEH frame only marks the loop done (a throwing destructor is not
// modelled).
void a_ehVectorDestructorIterator(Ctx &c, const u32 *a, Ret &) {
  u32 p = a[0] + a[1] * a[2];                                                        // 0x4283a / 0x4283e
  int32_t count = static_cast<int32_t>(a[2]);
  for (;;) {
    --count;                                                                         // 0x42845
    if (count < 0) return;                                                           // 0x42848
    p -= (NEG(ehVectorDestructorIterator) == 1 ? 0u : a[1]);                         // 0x4284a
    snd_call_va(c, a[3], SndAbi::Thiscall, {p});                                     // 0x42850
  }
}

// ehVectorConstructorIterator 0x42902 (stdcall, ptr, size, count, constructor, destructor): the constructor (thiscall) on count
// elements `size` bytes apart. A throwing constructor would reach __ArrayUnwind through the funclet 0x4296c; the port's constructors
// do not throw.
void a_ehVectorConstructorIterator(Ctx &c, const u32 *a, Ret &) {
  u32 p = a[0];
  const u32 size = a[1];
  const int32_t count = static_cast<int32_t>(a[2]);
  for (int32_t i = 0; i < count; ++i) {                                              // 0x42930
    snd_call_va(c, a[3], SndAbi::Thiscall, {p});                                     // 0x4293d
    p += (NEG(ehVectorConstructorIterator) == 1 ? size + 4 : size);                  // 0x42940
  }
}

// ================================================================================================ the four helpers
// GlblHeader::init 0x2e3c0 (thiscall, no stack argument): {+0 = 0x24, +4 = 0, +8 |= 1}; eax = this
void a_GlblHeader_init(Ctx &c, const u32 *a, Ret &r) {
  const u32 p = a[0];
  const u32 mode = ld<u32>(c, p + 8);                                                // 0x2e3c2
  st<u32>(c, p + 4, 0);                                                              // 0x2e3c5
  st<u32>(c, p, NEG(GlblHeader_init) == 1 ? 0x28u : 0x24u);                          // 0x2e3cf
  st<u32>(c, p + 8, mode | 1u);                                                      // 0x2e3d5
  r.set_u32(p);
}

// Sound::getField1b8 0x5f40 (thiscall): [this + 0x1b8]
void a_Sound_getField1b8(Ctx &c, const u32 *a, Ret &r) {
  r.set_u32(ld<u32>(c, a[0] + (NEG(Sound_getField1b8) == 1 ? 0x1bcu : 0x1b8u)));
}

// Sound::queueMidiCommand3 0x20480 (thiscall): with g_midiDevice set, CommandQueue::push(device, this, 3, 0, 0) (its thunk 0x15e1, four
// stack arguments); 0x13 without a device
void a_Sound_queueMidiCommand3(Ctx &c, const u32 *a, Ret &r) {
  const u32 dev = rd(c, kMidiDevice);                                                // 0x20482
  if (dev == 0) return r.set_u32(0x13);                                              // 0x20499
  r.set_u32(snd_call(c, kThunkQueuePush, SndAbi::Thiscall,
                     {dev, a[0], NEG(Sound_queueMidiCommand3) == 1 ? 2u : 3u, 0, 0}).eax);  // 0x20493
}

// MixBuffer::deletingDtor 0x2cf50 (thiscall, flags; `ret 4`): installs vtbl_MixBuffer, then walks the 0x80 slots at this + 4: for the
// first non-NULL slot of a run, it reads the run's last index k from the record the slot points to, takes the record in slot k, frees the
// block in the slot named by that record's second dword and zeroes the slots from there up to its first dword; frees the buffer at
// +0x204; frees `this` when bit 0 of flags is set. eax = this.
void a_MixBuffer_deletingDtor(Ctx &c, const u32 *a, Ret &r) {
  const u32 self = a[0];
  st<u32>(c, self, G(c, kVtMixBuffer));                                              // 0x2cf59
  u32 ebx = 0;
  do {
    u32 eax = ld<u32>(c, self + 4 + ebx * 4);                                        // 0x2cf60
    ebx += 1;                                                                        // 0x2cf64
    if (eax != 0) {                                                                  // 0x2cf65
      eax = ld<u32>(c, eax);                                                         // 0x2cf69
      ebx = eax + 1;                                                                 // 0x2cf6b
      eax = ld<u32>(c, self + 4 + eax * 4);                                          // 0x2cf6e
      const u32 edi = ld<u32>(c, eax + 4);                                           // 0x2cf72
      const u32 esi = ld<u32>(c, eax) + 1;                                           // 0x2cf75
      const u32 first = self + 4 + edi * 4;                                          // 0x2cf78
      snd_call(c, kRvaOperatorDelete, SndAbi::Cdecl, {ld<u32>(c, first)});           // 0x2cf83
      if (edi < esi) {                                                               // 0x2cf8b
        const u32 n = esi - edi;
        for (u32 k = 0; k < n; ++k) st<u32>(c, first + 4 * k, 0);                    // 0x2cf99: rep stosd
      }
    }
  } while (static_cast<int32_t>(ebx) < 0x80);                                        // 0x2cf9b
  const u32 extra = ld<u32>(c, self + 0x204);                                        // 0x2cfa3
  if (extra != 0) {
    snd_call(c, kRvaOperatorDelete, SndAbi::Cdecl, {extra});                         // 0x2cfae
    st<u32>(c, self + 0x204, 0);                                                     // 0x2cfb6
  }
  if ((a[1] & 1u) != 0 && NEG(MixBuffer_deletingDtor) != 1)                          // 0x2cfc0
    snd_call(c, kRvaOperatorDelete, SndAbi::Cdecl, {self});                          // 0x2cfc8
  r.set_u32(self);                                                                   // 0x2cfd1
}

// ChannelDesc_dtor 0x244a0 (thiscall): `add ecx, 0x14; jmp` into CtrlObject_dtorC: destroys the CtrlObject member at +0x14 and returns
// what that destructor returns
void a_ChannelDesc_dtor(Ctx &c, const u32 *a, Ret &r) {
  r = snd_call(c, kThunkCtrlObjectDtorC, SndAbi::Thiscall, {a[0] + (NEG(ChannelDesc_dtor) == 1 ? 0x10u : 0x14u)});  // 0x244a3
}

// VoiceBase_initListSlot 0x29b80 (thiscall): {+0 byte 1, +4 = 0x7f, +8 / +0xc / +0x10 / +0x14 = 0}; eax = this
void a_VoiceBase_initListSlot(Ctx &c, const u32 *a, Ret &r) {
  const u32 p = a[0];
  st<u32>(c, p + 8, 0);                                                              // 0x29b84
  st<u32>(c, p + 0xc, 0);                                                            // 0x29b87
  st<u32>(c, p + 0x10, 0);                                                           // 0x29b8a
  st<u32>(c, p + 0x14, 0);                                                           // 0x29b8d
  st<u32>(c, p + 4, NEG(VoiceBase_initListSlot) == 1 ? 0x7eu : 0x7fu);               // 0x29b90
  st<uint8_t>(c, p, 1);                                                              // 0x29b97
  r.set_u32(p);
}

// ================================================================================================ sprintf
// _sprintf 0x42577 (cdecl, buffer, format, value): __output 0x4490a over a string FILE (count 0x7fffffff, 0x42598), then the NUL
// (0x425b2); eax = the characters written. Every caller passes three slots (SoundTimer::onTick and the two stream formatters), so
// the format may consume one argument: a second one, `*` widths beyond it, floating-point (e f g E G), %n, %p, wide (%S %C, l / w on
// s / c) and I64 conversions are refused with MissingResource (the original would read further stack slots or code not ported).
// Supported, by __output's rules: flags - + space # 0, width, .precision, h / l, conversions d i u o x X c s %. Literal characters
// go through as written; a byte the CRT's ctype table (the pointer at 0x6773c, bit 0x80 of the high byte: a DBCS lead byte) marks
// takes the next byte with it (0x44aba-0x44ae0).
constexpr u32 kPctype = 0x6773c, kNullString = 0x651d0;
void a_sprintf(Ctx &c, const u32 *a, Ret &r) {
  const u32 buf = a[0];
  u32 fmt = a[1];
  int used = 0;
  auto arg = [&]() -> u32 {
    if (used++ != 0) throw MissingResource{"_sprintf: a format that reads more than the one argument its callers pass"};
    return a[2];
  };
  u32 out = 0;
  auto put = [&](uint8_t ch) { st<uint8_t>(c, buf + out++, ch); };
  auto B = [&](u32 va) { return ld<uint8_t>(c, va); };
  const u32 ctype = rd(c, kPctype);
  enum : u32 { F_LEFT = 1, F_SIGN = 2, F_SIGNSP = 4, F_ALT = 8, F_ZERO = 0x10, F_SHORT = 0x20, F_LONG = 0x40, F_SIGNED = 0x80,
               F_NEG = 0x100, F_OCT = 0x200 };
  for (;;) {
    uint8_t ch = B(fmt++);
    if (ch == 0) break;
    if (ch != '%') {
      if ((ld<uint8_t>(c, ctype + 2u * ch + 1) & 0x80u) != 0) {             // a lead byte: write it and the next one
        put(ch);
        ch = B(fmt++);
      }
      put(ch);
      continue;
    }
    u32 flags = 0;
    int32_t width = 0, prec = -1;
    ch = B(fmt++);
    for (;; ch = B(fmt++)) {                                                  // flags
      if (ch == '-') flags |= F_LEFT;
      else if (ch == '+') flags |= F_SIGN;
      else if (ch == ' ') flags |= F_SIGNSP;
      else if (ch == '#') flags |= F_ALT;
      else if (ch == '0') flags |= F_ZERO;
      else break;
    }
    if (ch == '*') {                                                          // width
      width = static_cast<int32_t>(arg());
      if (width < 0) {
        flags |= F_LEFT;
        width = -width;
      }
      ch = B(fmt++);
    } else {
      while (ch >= '0' && ch <= '9') {
        width = width * 10 + (ch - '0');
        ch = B(fmt++);
      }
    }
    if (ch == '.') {                                                          // precision
      prec = 0;
      ch = B(fmt++);
      if (ch == '*') {
        prec = static_cast<int32_t>(arg());
        if (prec < 0) prec = -1;
        ch = B(fmt++);
      } else {
        while (ch >= '0' && ch <= '9') {
          prec = prec * 10 + (ch - '0');
          ch = B(fmt++);
        }
      }
    }
    if (ch == 'h') {                                                          // size
      flags |= F_SHORT;
      ch = B(fmt++);
    } else if (ch == 'l') {
      flags |= F_LONG;
      ch = B(fmt++);
    }
    uint8_t text[0x200];
    u32 textlen = 0;
    const uint8_t *tp = text;
    uint8_t prefix[2] = {0, 0};
    u32 prefixlen = 0;
    u32 sva = 0;
    bool from_mem = false;
    if (ch == '%') {
      put('%');
      continue;
    } else if (ch == 'c') {
      if (flags & F_LONG) throw MissingResource{"_sprintf: %lc"};
      text[0] = static_cast<uint8_t>(arg());
      textlen = 1;
    } else if (ch == 's') {
      if (flags & F_LONG) throw MissingResource{"_sprintf: %ls"};
      sva = arg();
      if (sva == 0) sva = rd(c, kNullString);                                 // the narrow null-string pointer at 0x651d0
      u32 n = prec == -1 ? 0x7fffffffu : static_cast<u32>(prec);
      while (n-- && B(sva + textlen)) ++textlen;
      from_mem = true;
    } else if (ch == 'd' || ch == 'i' || ch == 'u' || ch == 'o' || ch == 'x' || ch == 'X') {
      u32 radix = 10, hexadd = 0;
      if (ch == 'd' || ch == 'i') flags |= F_SIGNED;
      if (ch == 'o') {
        radix = 8;
        if (flags & F_ALT) flags |= F_OCT;
      }
      if (ch == 'x' || ch == 'X') {
        radix = 16;
        hexadd = ch == 'x' ? 0x27u : 7u;
        if (flags & F_ALT) {
          prefix[0] = '0';
          prefix[1] = static_cast<uint8_t>('x' - 'a' + '9' + 1 + hexadd);
          prefixlen = 2;
        }
      }
      u32 v = arg();
      if (flags & F_SHORT) v = (flags & F_SIGNED) ? static_cast<u32>(static_cast<int32_t>(static_cast<int16_t>(v))) : (v & 0xffffu);
      if ((flags & F_SIGNED) && static_cast<int32_t>(v) < 0) {
        v = 0u - v;
        flags |= F_NEG;
      }
      if (prec < 0) prec = 1;
      else flags &= ~static_cast<u32>(F_ZERO);
      if (v == 0) prefixlen = 0;
      uint8_t *e = text + sizeof(text) - 1;
      uint8_t *t = e;
      while (prec-- > 0 || v != 0) {
        if (t == text) throw MissingResource{"_sprintf: precision beyond the port's buffer"};
        u32 d = v % radix + '0';
        v /= radix;
        if (d > '9') d += hexadd;
        *t-- = static_cast<uint8_t>(d);
      }
      textlen = static_cast<u32>(e - t);
      ++t;
      if ((flags & F_OCT) && (textlen == 0 || t[0] != '0')) {
        *--t = '0';
        ++textlen;
      }
      tp = t;
      if (flags & F_SIGNED) {
        if (flags & F_NEG) {
          prefix[0] = '-';
          prefixlen = 1;
        } else if (flags & F_SIGN) {
          prefix[0] = '+';
          prefixlen = 1;
        } else if (flags & F_SIGNSP) {
          prefix[0] = ' ';
          prefixlen = 1;
        }
      }
    } else {
      throw MissingResource{"_sprintf: a conversion the port does not implement"};
    }
    int32_t pad = width - static_cast<int32_t>(textlen) - static_cast<int32_t>(prefixlen);
    if (NEG(sprintf) == 1) pad -= 1;                                          // control: one pad character short
    if (!(flags & (F_LEFT | F_ZERO)))
      for (int32_t k = 0; k < pad; ++k) put(' ');
    for (u32 k = 0; k < prefixlen; ++k) put(prefix[k]);
    if ((flags & F_ZERO) && !(flags & F_LEFT))
      for (int32_t k = 0; k < pad; ++k) put('0');
    for (u32 k = 0; k < textlen; ++k) put(from_mem ? B(sva + k) : tp[k]);
    if (flags & F_LEFT)
      for (int32_t k = 0; k < pad; ++k) put(' ');
  }
  st<uint8_t>(c, buf + out, 0);                                               // 0x425b2
  r.set_u32(out);
}

// Ch5bb90_set3DBuffer 0x2c270 (thiscall, value; `ret 4`): +0x60 = value; the child +0x40 gets it through its own slot 0xec; 0x14
// without a child
void a_Ch5bb90_set3DBuffer(Ctx &c, const u32 *a, Ret &r) {
  st<u32>(c, a[0] + 0x60, a[1]);                                              // 0x2c274
  const u32 child = ld<u32>(c, a[0] + 0x40);                                  // 0x2c277
  if (child == 0 || NEG(Ch5bb90_set3DBuffer) == 1) return r.set_u32(0x14);    // 0x2c28a
  r = snd_vcall(c, child, 0xec, SndAbi::Thiscall, {a[1]});                    // 0x2c281
}

// ================================================================================================ registration
const FuncDef kFuncs[] = {
    {"snd.GlblHeader::init", "thiscall", "uint32", 1, 0, a_GlblHeader_init, &g_negctl_GlblHeader_init},
    {"snd.Sound::getField1b8", "thiscall", "uint32", 1, 0, a_Sound_getField1b8, &g_negctl_Sound_getField1b8},
    {"snd.MixBuffer::deletingDtor", "thiscall", "uint32", 2, 0, a_MixBuffer_deletingDtor, &g_negctl_MixBuffer_deletingDtor},
    {"snd.Sound::queueMidiCommand3", "thiscall", "uint32", 1, 0, a_Sound_queueMidiCommand3, &g_negctl_Sound_queueMidiCommand3},
    {"snd.ChannelDesc_dtor", "thiscall", "uint32", 1, 0, a_ChannelDesc_dtor, &g_negctl_ChannelDesc_dtor},
    {"snd.VoiceBase_initListSlot", "thiscall", "uint32", 1, 0, a_VoiceBase_initListSlot, &g_negctl_VoiceBase_initListSlot},
    {"snd._sprintf", "default", "uint32", 3, 0, a_sprintf, &g_negctl_sprintf},
    {"snd.Ch5bb90_set3DBuffer", "thiscall", "uint32", 2, 0, a_Ch5bb90_set3DBuffer, &g_negctl_Ch5bb90_set3DBuffer},
    {"snd.operator_delete", "default", "void", 1, 0, a_operator_delete, &g_negctl_operator_delete},
    {"snd._strrchr", "default", "uint32", 2, 0, a_strrchr, &g_negctl_strrchr},
    {"snd.operator_new", "default", "uint32", 1, 0, a_operator_new, &g_negctl_operator_new},
    {"snd._malloc", "default", "uint32", 1, 0, a_malloc, &g_negctl_malloc},
    {"snd.__nh_malloc", "default", "uint32", 2, 0, a_nh_malloc, &g_negctl_nh_malloc},
    {"snd.heap_alloc", "default", "uint32", 1, 0, a_heap_alloc, &g_negctl_heap_alloc},
    {"snd.free", "default", "void", 1, 0, a_free, &g_negctl_free},
    {"snd.__callnewh", "default", "uint32", 1, 0, a_callnewh, &g_negctl_callnewh},
    {"snd.__lock", "default", "uint32", 1, 0, a_lock, &g_negctl_lock},
    {"snd._unlock", "default", "uint32", 1, 0, a_unlock, &g_negctl_unlock},
    {"snd._lockexit", "default", "void", 0, 0, a_lockexit, &g_negctl_lockexit},
    {"snd._unlockexit", "default", "void", 0, 0, a_unlockexit, &g_negctl_unlockexit},
    {"snd.msize", "default", "uint32", 1, 0, a_msize, &g_negctl_msize},
    {"snd.crtRealloc", "default", "uint32", 2, 0, a_crtRealloc, &g_negctl_crtRealloc},
    {"snd.__onexit", "default", "uint32", 1, 0, a_onexit, &g_negctl_onexit},
    {"snd._atexit", "default", "uint32", 1, 0, a_atexit, &g_negctl_atexit},
    {"snd.strstr", "default", "uint32", 2, 0, a_strstr, &g_negctl_strstr},
    {"snd.__ArrayUnwind", "stdcall", "void", 4, 0, a_ArrayUnwind, &g_negctl_ArrayUnwind},
    {"snd.ehVectorDestructorIterator", "stdcall", "void", 4, 0, a_ehVectorDestructorIterator,
     &g_negctl_ehVectorDestructorIterator},
    {"snd.ehVectorConstructorIterator", "stdcall", "void", 5, 0, a_ehVectorConstructorIterator,
     &g_negctl_ehVectorConstructorIterator},
};

// the functions with a row in re/roadmap/S4_functions.tsv that the core's rva -> name table skips (a `collides` name) and the ones with
// no row yet: the core asks here (audio_core.cpp, audio_crt_name)
struct RvaName {
  u32 rva;
  const char *name;
};
const RvaName kNames[] = {
    {0x2e3c0, "snd.GlblHeader::init"}, {0x5f40, "snd.Sound::getField1b8"}, {0x2cf50, "snd.MixBuffer::deletingDtor"},
    {0x20480, "snd.Sound::queueMidiCommand3"}, {0x244a0, "snd.ChannelDesc_dtor"},
    {0x29b80, "snd.VoiceBase_initListSlot"}, {0x42577, "snd._sprintf"}, {0x2c270, "snd.Ch5bb90_set3DBuffer"}, {0x4249a, "snd.operator_delete"}, {0x424b0, "snd._strrchr"},
    {0x424d7, "snd.operator_new"}, {0x42acd, "snd._malloc"}, {0x42adf, "snd.__nh_malloc"}, {0x42b0b, "snd.heap_alloc"},
    {0x42c07, "snd.free"}, {0x45b80, "snd.__callnewh"}, {0x47710, "snd.__lock"}, {0x47771, "snd._unlock"},
    {0x45226, "snd._lockexit"}, {0x4522f, "snd._unlockexit"}, {0x425c9, "snd.__onexit"}, {0x45581, "snd.msize"}, {0x45252, "snd.crtRealloc"}, {0x42647, "snd._atexit"},
    {0x42690, "snd.strstr"}, {0x4288e, "snd.__ArrayUnwind"}, {0x42902, "snd.ehVectorConstructorIterator"}, {0x4280e, "snd.ehVectorDestructorIterator"},
};

}  // namespace

const char *audio_crt_function_name(uint32_t rva) {
  for (const RvaName &n : kNames)
    if (n.rva == rva) return n.name;
  return nullptr;
}

extern const ModuleDef kModule_audio_crt = {"audio_crt", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim

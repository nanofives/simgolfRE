// The sound.dll wave family (step S4.9, log/roadmap/S4/9_notes.md; API in portsim/sim/audio.h): Wave_Device, Wave_In_Device,
// WaveStream / StreamReader, the WaveInDevice capture object, the mmio RIFF readers, the MidiReader stream readers and the wave
// file writers: the rows S4.9 of re/roadmap/S4_functions.tsv, written from the C3 notes in the hook sources (shim/src/re/c3an.cpp,
// c3au.cpp, c3cf.cpp, c3cl.cpp, c3cx.cpp, c3dh.cpp, c3dj.cpp, c3do.cpp, c3dt.cpp, c3ew.cpp, c3ex.cpp, c3fd.cpp, c3fg.cpp, c3fl.cpp,
// c3fn.cpp, c3fq.cpp, c3fu.cpp) and, for the rows with no hook source, from the instructions of the user's sound.dll; judged by the
// replay (port/test/sound_replay.py through log/roadmap/S4/9_replay.py). Addresses in comments are RVAs of the user's sound.dll.
//
// Every call to code that is not inline goes through the dispatcher (snd_call / snd_vcall / snd_import), so a fixture's stand-in,
// the host, another port function or the AudioSink serves it. The mmio imports are the 12 WINMM mmio entry points: with a host
// the emulator's stand-in answers, with none the AudioSink receives `WINMM!mmioXxx` with the original's argument slots.
//
// Negative controls (sg_negctl_set(<name>, mode)): mode 1 of every function is a wrong result: the value it returns is changed
// (`neg_u32`) or, for a function that returns nothing, the dword at the field the adapter names is changed after the body
// (`neg_mem`).
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "portsim/arith.h"
#include "portsim/ptr.h"
#include "portsim/registry.h"
#include "portsim/sim/audio.h"
#include "portsim/sim/audio_dev.h"
#include "portsim/sim/audio_wave.h"
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

// Locals the original passes by address (an MMIOINFO built on the stack, the out slots of a callee, an MMCKINFO) live in the
// core's stack frame (portsim/sim/audio.h SndFrame, S4.2b): at the original's offsets below the function's entry esp when the
// harness supplies it (U-1470), so a stand-in that logs the pointer sees the original's value. `at(off)` is the local at entry
// esp - off.

// a dword of virtual memory the original reads as `unsigned` (the converter's hook-source reads)
inline uint32_t rd(Ctx &c, uint32_t va) { return ld<uint32_t>(c, va); }
inline void wr(Ctx &c, uint32_t va, uint32_t v) { st<uint32_t>(c, va, v); }
// a node of the doubly linked lists the module walks: {+0 prev, +4 next, +8 value}
inline uint32_t n_prev(Ctx &c, uint32_t n) { return ld<u32>(c, n); }
inline uint32_t n_next(Ctx &c, uint32_t n) { return ld<u32>(c, n + 4); }
inline uint32_t n_value(Ctx &c, uint32_t n) { return ld<u32>(c, n + 8); }
// the 12 WINMM mmio IAT slots of sound.dll (rvas of the slots; the import name table of the user's file)
constexpr uint32_t kMmioDescend = 0xb8624, kMmioAscend = 0xb8628, kMmioSetBuffer = 0xb862c, kMmioAdvance = 0xb8630,
                   kMmioSetInfo = 0xb8634, kMmioWrite = 0xb8658, kMmioStringToFourcc = 0xb865c, kMmioOpen = 0xb8660,
                   kMmioRead = 0xb8664, kMmioGetInfo = 0xb8668, kMmioSeek = 0xb866c, kMmioClose = 0xb8670;
constexpr uint32_t kSleep = 0xb840c;
// rep stosb / stosd of `n` bytes at a virtual address (the original's memset of a caller's buffer); an unmapped range faults
inline void vfill(Ctx &c, u32 va, uint8_t byte, u32 n) {
  if (n) std::memset(c.mem.at(va, n, true), byte, n);
}

// ---- the negative-control modes (read by the adapters and, for the import-level controls, by the bodies)
int g_negctl_Wave_Device__readFmtChunk = 0;
int g_negctl_processWaveChunks = 0;
int g_negctl_WaveStream__ctor = 0;
int g_negctl_WaveStream__scalarDeletingDtor = 0;
int g_negctl_WaveStream__destroy = 0;
int g_negctl_closeMmioNodeList = 0;
int g_negctl_Wave_Device__openMemoryMmio = 0;
int g_negctl_Wave_Device__freeBufferList = 0;
int g_negctl_Wave_Device__appendBuffer = 0;
int g_negctl_startStream = 0;
int g_negctl_computeCaptureDuration = 0;
int g_negctl_initStreamBuffer = 0;
int g_negctl_writeWaveFile = 0;
int g_negctl_writeWaveFileB = 0;
int g_negctl_writeWaveChunk = 0;
int g_negctl_writeWaveChunkB = 0;
int g_negctl_openMmioFile = 0;
int g_negctl_closeMmioHandle = 0;
int g_negctl_Dll_Wave_Device__create_device = 0;
int g_negctl_openWaveStreamFile = 0;
int g_negctl_Wave_Device__setCooperativeLevel = 0;
int g_negctl_Wave_Device__closeStream = 0;
int g_negctl_Dll_Wave_Device__delete_device = 0;
int g_negctl_Wave_Device__ctor = 0;
int g_negctl_Wave_Device__scalarDeletingDtor = 0;
int g_negctl_Wave_Device__dtor = 0;
int g_negctl_Wave_Device__open = 0;
int g_negctl_Wave_Device__stop = 0;
int g_negctl_Wave_Device__start = 0;
int g_negctl_Wave_Device__closeOutput = 0;
int g_negctl_Wave_Device__stopOutput = 0;
int g_negctl_Wave_Device__startOutput = 0;
int g_negctl_Wave_Device__restart = 0;
int g_negctl_Wave_Device__serviceTick = 0;
int g_negctl_WaveCmdQueue__pushNode = 0;
int g_negctl_WaveCmdQueue__pushValue = 0;
int g_negctl_Wave_Device__flushCommandQueue = 0;
int g_negctl_Wave_Device__dispatchCommand = 0;
int g_negctl_Wave_Device__scanCommandRing = 0;
int g_negctl_Wave_Device__caps = 0;
int g_negctl_Wave_Device__deviceName = 0;
int g_negctl_setOutputMasterVolume = 0;
int g_negctl_getWaveDeviceInterface = 0;
int g_negctl_serviceWaveDevice = 0;
int g_negctl_stubReturnBA = 0;
int g_negctl_Wave_In_Device__scalarDeletingDtor = 0;
int g_negctl_Wave_In_Device__dtor = 0;
int g_negctl_Dll_Wave_In_Device__create_device = 0;
int g_negctl_Dll_Wave_In_Device__delete_device = 0;
int g_negctl_Wave_In_Device__start = 0;
int g_negctl_Wave_In_Device__stop = 0;
int g_negctl_Wave_In_Device__openCapture = 0;
int g_negctl_Wave_In_Device__closeCapture = 0;
int g_negctl_Wave_In_Device__setStream = 0;
int g_negctl_Wave_In_Device__clearStream = 0;
int g_negctl_MidiReader_freeContent = 0;
int g_negctl_streamReadByte = 0;
int g_negctl_streamConsumeByte = 0;
int g_negctl_MidiReader_init = 0;
int g_negctl_MidiReader_dtor = 0;
int g_negctl_WaveInDeviceBase_ctor = 0;
int g_negctl_WaveInDeviceBase_scalarDtor = 0;
int g_negctl_WaveInDeviceBase_openInner = 0;
int g_negctl_WaveInDeviceBase_closeInner = 0;
int g_negctl_enqueueWaveCmdE = 0;
int g_negctl_enqueueWaveCmd10 = 0;
int g_negctl_Sound5c450__openMmio = 0;
int g_negctl_openMmioSound = 0;
int g_negctl_streamWaveSamples = 0;
int g_negctl_StreamReader_ctor = 0;
int g_negctl_StreamReader_scalarDtor = 0;
int g_negctl_streamReaderDtor = 0;
int g_negctl_WaveInStream_start = 0;
int g_negctl_freeMmioHandleList = 0;
int g_negctl_freeMmioHandleListGetInfo = 0;
int g_negctl_loadWaveResource = 0;
int g_negctl_WaveInStream_resetCodec = 0;
int g_negctl_beginDecodeStreamA = 0;
int g_negctl_beginDecodeStreamB = 0;
int g_negctl_closeDecoder = 0;
int g_negctl_return0xb_32660 = 0;
int g_negctl_parseRiffWave = 0;
int g_negctl_fillDecodeRing = 0;
int g_negctl_serviceDecodeRing = 0;
int g_negctl_closeMmioSound = 0;
int g_negctl_flushMmioBuffer = 0;
int g_negctl_MmioBuffer_init = 0;
int g_negctl_WaveInDevice_ctor = 0;
int g_negctl_WaveInDevice_deleteDtor = 0;
int g_negctl_WaveInDevice_dtor = 0;
int g_negctl_WaveInDevice_allocCaptureBuffers = 0;
int g_negctl_WaveInDevice_start = 0;
int g_negctl_WaveInDevice_prepareHeaders = 0;
int g_negctl_WaveInDevice_addBuffer = 0;
int g_negctl_WaveInDevice_reset = 0;
int g_negctl_onWaveInCallbackMessage = 0;
int g_negctl_WaveInDevice_open = 0;
int g_negctl_WaveInDevice_close = 0;
int g_negctl_WaveInDevice_requeueBuffer = 0;
int g_negctl_loadMultiChannel = 0;

// ---- Wave_Device::readFmtChunk (rva 0x5440): hand-written from c3fg.cpp ReadFmt_re
// A CtrlObject is built in the frame (CtrlObject::ctor, thunk 0x1ffa, 0x5461) and its vtable set to rva 0x5b178 (0x5466). Through mmioRead
// on the HMMIO +0xa48 (0x5474): a dword tag, a dword size n, then n bytes into the CtrlObject's +0x10 (0x5488-0x54ab). The frame holds
// 0x18 bytes there: the CtrlObject's +0x28 is the SEH record's link that 0x55e6 / 0x55ed reload into fs:[0], so an n above 0x18
// replaces the thread's exception chain head with file bytes at the return; the port has no exception chain and keeps the bytes it
// writes inside the frame (U-1480). When the dword at +0x1c (the read's +0xc) is 3 (0x54ad): operator new 0xe4 (0x54be), Ctrl5b5c0_ctor
// (thunk 0x25b3), then three (size, bytes) pairs read into the new object's +0x20, +8 and +0x58 (0x54fa-0x5550); its embedded record
// +0x2c gets the CtrlObject's dwords +4..+0x24 (the word at +0x18) at the same offsets (0x5552-0x5592); a 0xc-byte node {0, 0, record}
// (operator new 0x5595) is appended to the list +0xa4c (head) / +0xa50 (tail) and +0xa58 counts it (0x55b6-0x55e0). eax at the `ret`: the
// new count, or the third mmioRead's result when the type is not 3. A failed operator new faults.
// Frame (entry esp E): 0x5455 `sub esp, 0x34` below the SEH record (E-0xc): the CtrlObject at E-0x34 (`lea ecx, [esp + 0x18]` 0x545d), tag
// at E-0x3c and n at E-0x40 (the `[esp + 0x10]` / `[esp + 0xc]` the reads store to).
uint32_t Wave_Device__readFmtChunk_body(Ctx &ctx, u32 self) {
    SndFrame F(ctx, 0x4c);
    const u32 ctrl = F.at(0x34), tagA = F.at(0x3c), nA = F.at(0x40);
    snd_call(ctx, 0x00001ffa, SndAbi::Thiscall, {ctrl}).eax;  // 0x5461
    st<u32>(ctx, ctrl, snd_va(ctx, 0x5b178));  // 0x5466
    st<uint32_t>(ctx, tagA, 0);
    st<uint32_t>(ctx, nA, 0);
    const u32 mm = ld<u32>(ctx, self + 0xa48);
    snd_import(ctx, kMmioRead, SndAbi::Stdcall, {mm, tagA, 4}).eax;  // 0x5488
    snd_import(ctx, kMmioRead, SndAbi::Stdcall, {ld<u32>(ctx, self + 0xa48), nA, 4}).eax;  // 0x5498
    unsigned r = snd_import(ctx, kMmioRead, SndAbi::Stdcall, {ld<u32>(ctx, self + 0xa48), ctrl + 0x10, ld<uint32_t>(ctx, nA)}).eax;  // 0x54ab
    if (ld<uint32_t>(ctx, ctrl + 0x1c) != 3) return r;  // 0x54ad / 0x54b2
    u32 obj = snd_new(ctx, 0xe4);  // 0x54be
    if (obj) obj = snd_call(ctx, 0x000025b3, SndAbi::Thiscall, {obj}).eax;  // 0x54d5
    u32 rec = obj ? obj + 0x2c : 0;  // 0x54e9
    snd_import(ctx, kMmioRead, SndAbi::Stdcall, {ld<u32>(ctx, self + 0xa48), tagA, 4}).eax;  // 0x54fa
    snd_import(ctx, kMmioRead, SndAbi::Stdcall, {ld<u32>(ctx, self + 0xa48), obj + 0x20, ld<uint32_t>(ctx, tagA)}).eax;  // 0x550c
    snd_import(ctx, kMmioRead, SndAbi::Stdcall, {ld<u32>(ctx, self + 0xa48), tagA, 4}).eax;  // 0x551c
    snd_import(ctx, kMmioRead, SndAbi::Stdcall, {ld<u32>(ctx, self + 0xa48), obj + 8, ld<uint32_t>(ctx, tagA)}).eax;  // 0x552e
    snd_import(ctx, kMmioRead, SndAbi::Stdcall, {ld<u32>(ctx, self + 0xa48), tagA, 4}).eax;  // 0x553e
    snd_import(ctx, kMmioRead, SndAbi::Stdcall, {ld<u32>(ctx, self + 0xa48), obj + 0x58, ld<uint32_t>(ctx, tagA)}).eax;  // 0x5550
    // rec+N = the CtrlObject's +N: [esp + 0x20] is read before the `push 0xc` at 0x5556, the rest after it
    st<uint32_t>(ctx, rec + 4, ld<uint32_t>(ctx, ctrl + 4));  // 0x5552 / 0x5558
    st<uint32_t>(ctx, rec + 8, ld<uint32_t>(ctx, ctrl + 8));  // 0x555b / 0x555f (not written by the ctor)
    st<uint32_t>(ctx, rec + 0xc, ld<uint32_t>(ctx, ctrl + 0xc));  // 0x5562 / 0x5566
    st<uint32_t>(ctx, rec + 0x10, ld<uint32_t>(ctx, ctrl + 0x10));  // 0x5569 / 0x556d
    st<uint32_t>(ctx, rec + 0x14, ld<uint32_t>(ctx, ctrl + 0x14));  // 0x5570 / 0x5574
    st<uint16_t>(ctx, rec + 0x18, ld<uint16_t>(ctx, ctrl + 0x18));  // 0x5577 / 0x557c
    st<uint32_t>(ctx, rec + 0x1c, ld<uint32_t>(ctx, ctrl + 0x1c));  // 0x5580 / 0x5584
    st<uint32_t>(ctx, rec + 0x20, ld<uint32_t>(ctx, ctrl + 0x20));  // 0x5587 / 0x558b
    st<uint32_t>(ctx, rec + 0x24, ld<uint32_t>(ctx, ctrl + 0x24));  // 0x558e / 0x5592
    u32 node = snd_new(ctx, 0xc);  // 0x5595
    if (node) { st<u32>(ctx, node, 0); st<u32>(ctx, node + 4, 0); st<u32>(ctx, node + 8, rec); }  // 0x55a2-0x55af
    u32 tail = ld<u32>(ctx, self + 0xa50);  // 0x55b6
    if (tail) {  // 0x55be
        st<u32>(ctx, tail + 4, node);  // 0x55c0
        st<u32>(ctx, node, ld<u32>(ctx, self + 0xa50));  // 0x55c9
    } else {
        st<u32>(ctx, self + 0xa4c, node);  // 0x55cd
    }
    st<u32>(ctx, self + 0xa50, node);  // 0x55d3
    const uint32_t cnt = ld<uint32_t>(ctx, self + 0xa58) + 1;  // 0x55d9-0x55e0
    st<uint32_t>(ctx, self + 0xa58, cnt);
    return cnt;
}
void a_Wave_Device__readFmtChunk(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Wave_Device__readFmtChunk), Wave_Device__readFmtChunk_body(ctx, a[0]))); }

// ---- processWaveChunks (rva 0x5770): converted from c3fg.cpp ProcChunks_re
uint32_t c3fg_u(Ctx &, u32 p) { return static_cast<uint32_t>(p); }
uint32_t processWaveChunks_body(Ctx &ctx, u32 self) {
    u32 cur = ld<u32>(ctx, self + 0xa4c);  // 0x5776
    st<u32>(ctx, self + 0xa54, cur);  // 0x577e
    if (!cur) return 0;  // 0x5784
    u32 rec = n_value(ctx, cur);  // 0x578a
    if (!rec) return c3fg_u(ctx, cur);  // 0x578f
    u32 obj = self;  // 0x5795 (the slot pushed at 0x5770)
    for (;;) {
        unsigned t = ld<uint32_t>(ctx, rec + 0x20);  // 0x5799
        bool append = false;
        if (t == 0) {  // 0x579e
            obj = (snd_vcall(ctx, self, 0x118, SndAbi::Thiscall, {0, ld<uint32_t>(ctx, rec + 0x24)}).eax);  // 0x57fd
            append = true;
        } else if (t == 3) {  // 0x57a3
            u32 prevRec = 0;
            u32 c = ld<u32>(ctx, self + 0xa54);  // 0x57a5
            if (c) {
                c = n_prev(ctx, c);  // 0x57af
                st<u32>(ctx, self + 0xa54, c);  // 0x57b3
                if (c) prevRec = n_value(ctx, c);  // 0x57bb
            }
            c = ld<u32>(ctx, self + 0xa54);  // 0x57c2
            if (c) st<u32>(ctx, self + 0xa54, n_next(ctx, c));  // 0x57cc / 0x57cf
            if (ld<uint8_t>(ctx, prevRec + 0x18)) {  // 0x57d5 / 0x57da
                if (ld<uint32_t>(ctx, prevRec + 0x1c) == 3)  // 0x57dc
                    obj = prevRec ? prevRec - 0x2c : 0;  // 0x57e2-0x57ef
                if (obj) append = true;  // 0x57e9 / 0x57eb
            }
        }
        if (append) snd_call(ctx, 0x00001654, SndAbi::Thiscall, {obj, c3fg_u(ctx, rec)}).eax;  // 0x5808
        u32 c = ld<u32>(ctx, self + 0xa54);  // 0x580d
        if (!c) return 0;  // 0x5815
        c = n_next(ctx, c);  // 0x5817
        st<u32>(ctx, self + 0xa54, c);  // 0x581c
        if (!c) return 0;  // 0x5822
        rec = n_value(ctx, c);  // 0x5824
        if (!rec) return c3fg_u(ctx, c);  // 0x5829
    }
}
void a_processWaveChunks(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(processWaveChunks), processWaveChunks_body(ctx, a[0]))); }

// ---- WaveStream::ctor (rva 0x6020): converted from c3do.cpp WaveStreamCtor_re
u32 WaveStream__ctor_body(Ctx &ctx, u32 self) {
    snd_call(ctx, 0x0002e930, SndAbi::Thiscall, {self}).eax;  // 0x603f -> 0x2e930
    st<uint32_t>(ctx, self + 0x5c, 0);  // 0x604a
    st<uint32_t>(ctx, self + 0x60, 0);  // 0x604d
    st<uint32_t>(ctx, self + 0x64, 0);  // 0x6050
    st<uint32_t>(ctx, self + 0x68, 0);  // 0x6053
    st<u32>(ctx, self, snd_va(ctx, snd_rva::vt_WaveStream));  // 0x6059
    st<uint32_t>(ctx, self + 0xc8, 0);  // 0x605f
    st<uint32_t>(ctx, self + 0x58, 0);  // 0x606a
    st<uint32_t>(ctx, self + 0xb8, 0);  // 0x6071
    st<uint32_t>(ctx, self + 0xbc, 0);  // 0x6077
    st<uint32_t>(ctx, self + 0xb4, 0);  // 0x607d
    for (int off = 0x6c; off < 0x6c + 0x12 * 4; off += 4) st<uint32_t>(ctx, self + off, 0);  // 0x6083
    st<uint32_t>(ctx, self + 0xc4, 0);  // 0x608e
    st<uint32_t>(ctx, self + 0xc0, 0);  // 0x6094
    snd_call(ctx, 0x0002ad10, SndAbi::Thiscall, {self, 4}).eax;  // 0x609a -> 0x2ad10
    st<uint32_t>(ctx, self + 0xd4, 0);  // 0x60a7
    st<uint32_t>(ctx, self + 0xc0, 0);  // 0x60ad
    st<uint32_t>(ctx, self + 0xcc, 0);  // 0x60b3
    st<uint32_t>(ctx, self + 0xd0, 0);  // 0x60b9
    st<uint32_t>(ctx, self + 0xd8, 0);  // 0x60bf
    st<uint32_t>(ctx, self + 0xdc, 0);  // 0x60c5
    st<uint32_t>(ctx, self + 0xe0, 0);  // 0x60c9
    st<uint32_t>(ctx, self + 0xe4, 0);  // 0x60cc
    st<uint16_t>(ctx, self + 0xe8, 0);  // 0x60cf
    return self;  // 0x60c1
}
void a_WaveStream__ctor(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(WaveStream__ctor), WaveStream__ctor_body(ctx, a[0]))); }

// ---- WaveStream::scalarDeletingDtor (rva 0x6130): converted from c3dh.cpp WsDelDtor_re
u32 WaveStream__scalarDeletingDtor_body(Ctx &ctx, u32 self, uint32_t flags) {
    snd_call(ctx, 0x00006160, SndAbi::Thiscall, {self});  // 0x6133 -> 0x6160
    if (flags & 1) snd_delete(ctx, self);  // 0x6138 / 0x6140
    return self;  // 0x6148
}
void a_WaveStream__scalarDeletingDtor(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(WaveStream__scalarDeletingDtor), WaveStream__scalarDeletingDtor_body(ctx, a[0], a[1]))); }

// ---- WaveStream::destroy (rva 0x6160): converted from c3dh.cpp WsDestroy_re
constexpr uint32_t kIatMmioClose = 0xb8670;  // c3dh.cpp
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
void WaveStream__destroy_body(Ctx &ctx, u32 self) {
    st<u32>(ctx, self, snd_va(ctx, snd_rva::vt_WaveStream));  // 0x6180
    u32 node = ld<u32>(ctx, self + 0x5c);  // 0x6186
    if (node) {  // 0x618b / 0x6195
        u32 elem = c3dh_popFront(ctx, self, 0x5c, 0x60, 0x68, node);  // 0x619b-0x61c1
        while (elem) {  // 0x61c4 / 0x6221
            unsigned h = ld<uint32_t>(ctx, elem + 0x44);  // 0x61cc
            if (h) {  // 0x61d1
                snd_import(ctx, kIatMmioClose, SndAbi::Stdcall, {h, 0}).eax;  // 0x61d5
                st<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_openMmioCount).va(), ld<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_openMmioCount).va()) - 1);  // 0x61d7
            }
            c3dh_dtorThunk(ctx, elem);  // 0x61e3
            snd_delete(ctx, elem);  // 0x61e9
            node = ld<u32>(ctx, self + 0x5c);  // 0x61f1
            if (!node) break;  // 0x61f6
            elem = c3dh_popFront(ctx, self, 0x5c, 0x60, 0x68, node);  // 0x61f8-0x621e
        }
    }
    node = ld<u32>(ctx, self + 0x5c);  // 0x6223
    if (node) {  // 0x622c
        u32 elem = c3dh_popFront(ctx, self, 0x5c, 0x60, 0x68, node);  // 0x622e-0x6254
        while (elem) {  // 0x6257 / 0x6289
            node = ld<u32>(ctx, self + 0x5c);  // 0x6259
            if (!node) break;  // 0x625e
            elem = c3dh_popFront(ctx, self, 0x5c, 0x60, 0x68, node);  // 0x6260-0x6286
        }
    }
    snd_call(ctx, 0x0002ea30, SndAbi::Thiscall, {self});  // 0x6295 -> 0x2ea30
}
void a_WaveStream__destroy(Ctx &ctx, const uint32_t *a, Ret &) { WaveStream__destroy_body(ctx, a[0]); neg_mem(ctx, NEG(WaveStream__destroy), a[0] + 0x58); }

// ---- closeMmioNodeList (rva 0x6330): converted from c3dh.cpp CloseMmioList_re
uint32_t closeMmioNodeList_body(Ctx &ctx, u32 self) {
    snd_vcall(ctx, self, 0x20, SndAbi::Thiscall, {});  // 0x6335
    if ((ld<uint8_t>(ctx, self + 0x44) & 2) != 0) {  // 0x6338
        st<uint32_t>(ctx, self + 0x58, ld<uint32_t>(ctx, self + 0x58) | 8);  // 0x633e-0x6343
        return 0;  // 0x6348
    }
    u32 node = ld<u32>(ctx, self + 0x5c);  // 0x634a
    if (node) {  // 0x6354
        u32 elem = c3dh_popFront(ctx, self, 0x5c, 0x60, 0x68, node);  // 0x635a-0x6380
        while (elem) {  // 0x6383 / 0x63e0
            unsigned h = ld<uint32_t>(ctx, elem + 0x44);  // 0x638b
            if (h) {  // 0x6390
                snd_import(ctx, kIatMmioClose, SndAbi::Stdcall, {h, 0}).eax;  // 0x6394
                st<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_openMmioCount).va(), ld<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_openMmioCount).va()) - 1);  // 0x6396
            }
            c3dh_dtorThunk(ctx, elem);  // 0x63a2
            snd_delete(ctx, elem);  // 0x63a8
            node = ld<u32>(ctx, self + 0x5c);  // 0x63b0
            if (!node) break;  // 0x63b5
            elem = c3dh_popFront(ctx, self, 0x5c, 0x60, 0x68, node);  // 0x63b7-0x63db
        }
    }
    return 0;  // 0x63e5
}
void a_closeMmioNodeList(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(closeMmioNodeList), closeMmioNodeList_body(ctx, a[0]))); }

// ---- Wave_Device::openMemoryMmio (rva 0x6420): hand-fixed conversion of c3dh.cpp OpenMemMmio_re
// The MMIOINFO (0x48 bytes) is the function's frame (sub esp, 0x48): info at entry esp - 0x48 (0x6441 `lea edi, [esp + 8]` after
// the two pushes; 0x645e `lea ecx, [esp + 4]`).
constexpr uint32_t kIatMmioGetInfo = 0xb8668;
constexpr uint32_t kIatMmioOpenA = 0xb8660;
uint32_t Wave_Device__openMemoryMmio_body(Ctx &ctx, u32 self, uint32_t size) {
    if (ld<uint32_t>(ctx, self + 0xb0) != 0) return 0;  // 0x6426 / 0x642c / 0x6430
    SndFrame F(ctx, 0x48);
    const u32 info = F.at(0x48);
    for (int i = 0; i < 0x12; i++) st<uint32_t>(ctx, info + 4 * i, 0);  // 0x643a-0x6445
    st<uint32_t>(ctx, info + 4, 0x204d454d);  // 0x644b
    unsigned len = size;  // 0x6447
    if (len == 0) len = ld<uint32_t>(ctx, self + 0xb8);  // 0x6453 / 0x6456 / 0x6458
    st<uint32_t>(ctx, info + 0x14, len);  // 0x646a
    st<uint32_t>(ctx, info + 0x30, 0x64);  // 0x646e
    unsigned h = snd_import(ctx, kIatMmioOpenA, SndAbi::Stdcall, {0, info, 0x1000}).eax;  // 0x6476
    if (h == 0) return 0;  // 0x647c / 0x647e
    st<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_openMmioCount).va(), ld<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_openMmioCount).va()) + 1);  // 0x6487-0x6495
    u32 dst = self + 0x6c;  // 0x648d
    snd_import(ctx, kIatMmioGetInfo, SndAbi::Stdcall, {h, dst, 0}).eax;  // 0x649b
    return dst;  // 0x64a1
}
void a_Wave_Device__openMemoryMmio(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Wave_Device__openMemoryMmio), Wave_Device__openMemoryMmio_body(ctx, a[0], a[1]))); }

// ---- Wave_Device::freeBufferList (rva 0x64e0): hand-fixed conversion of c3fq.cpp Pull_re
// unlink the head node n (`mov ecx, [eax+4]`; a non-NULL next gets prev = 0 and becomes the head, else tail and head = 0),
// as at 0x651f-0x6530, 0x657a-0x658b, 0x664f-0x6660, 0x6719-0x672a
void c3fq_unlinkHead(Ctx &ctx, u32 self, u32 n) {
    if (u32 nx = ld<u32>(ctx, n + 4)) {
        st<u32>(ctx, nx, 0);
        st<u32>(ctx, self + 0x5c, nx);
    } else {
        st<u32>(ctx, self + 0x60, 0);
        st<u32>(ctx, self + 0x5c, 0);
    }
}
void c3fq_dropVoice(Ctx &ctx, u32 v) {  // 0x668f-0x669c, 0x6757-0x675d
    snd_call(ctx, 0x00001b45, SndAbi::Thiscall, {v});
    snd_delete(ctx, v);
}
void c3fq_popHead(Ctx &ctx, u32 self) {  // 0x6648-0x6670, 0x6712-0x673a
    u32 h = ld<u32>(ctx, self + 0x5c);
    if (!h) return;
    c3fq_unlinkHead(ctx, self, h);
    snd_delete(ctx, h);
    st<uint32_t>(ctx, self + 0x68, ld<uint32_t>(ctx, self + 0x68) - 1);
}
// Frame (entry esp E, 0x64e0 `sub esp, 0xc` then pushes ebx / esi / edi): the table function at E-4 (0x64fc), the pad flag at E-0xc
// (0x64f8 / 0x6502), the destination pointer `d` the table function advances at E-8 (0x6601 `mov [esp + 0x18], eax`, handed as
// `lea eax, [esp + 0x20]` 0x6610), and the position `pos` in the argument slot of `fmt` at E+0xc (0x6614 `mov [esp + 0x3c], ecx`,
// handed as `lea edx, [esp + 0x30]`); the returned count is kept in the slot of `dst` at E+4 (0x6625).
uint32_t Wave_Device__freeBufferList_body(Ctx &ctx, u32 self, u32 dst, uint32_t count, uint32_t fmt, u32 outN) {
    SndFrame F(ctx, 0xc);
    const u32 dAddr = F.at(8), posAddr = F.entry() + 0xc;
    const u32 fn = ld<u32>(ctx, snd_va(ctx, 0x0005b4a4 + 4 * fmt));  // 0x64eb
    const bool pad = fmt >= 0xc;  // 0x64f4-0x6502
    if (ld<uint8_t>(ctx, self + 0x58) & 8) {  // 0x650a / 0x650e
        if (u32 n = ld<u32>(ctx, self + 0x5c)) {  // 0x6514-0x6519
            c3fq_unlinkHead(ctx, self, n);  // 0x651f-0x6530
            u32 v = ld<u32>(ctx, n + 8);  // 0x6533
            snd_delete(ctx, n);  // 0x6537
            st<uint32_t>(ctx, self + 0x68, ld<uint32_t>(ctx, self + 0x68) - 1);  // 0x653c-0x6545
            while (v) {  // 0x6548 / 0x65a3
                if (u32 h = ld<u32>(ctx, v + 0x44)) {  // 0x654a-0x654f
                    snd_import(ctx, 0x000b8670, SndAbi::Stdcall, {h, 0}).eax;  // 0x6551-0x6553
                    st<int32_t>(ctx, snd_obj(ctx, &SoundState::g_openMmioCount).va(), ld<int32_t>(ctx, snd_obj(ctx, &SoundState::g_openMmioCount).va()) - 1);  // 0x6559
                }
                c3fq_dropVoice(ctx, v);  // 0x6563-0x656b
                n = ld<u32>(ctx, self + 0x5c);  // 0x6573
                if (!n) break;  // 0x6576 / 0x6578
                c3fq_unlinkHead(ctx, self, n);  // 0x657a-0x658b
                v = ld<u32>(ctx, n + 8);  // 0x658e
                snd_delete(ctx, n);  // 0x6592
                st<uint32_t>(ctx, self + 0x68, ld<uint32_t>(ctx, self + 0x68) - 1);  // 0x6597-0x65a0
            }
        }
        snd_vcall(ctx, self, 0x80, SndAbi::Thiscall, {}).eax;  // 0x65a5-0x65a9
    }
    u32 n = ld<u32>(ctx, self + 0x5c);  // 0x65af
    st<u32>(ctx, self + 0x64, n);  // 0x65b4
    u32 v = n ? ld<u32>(ctx, n + 8) : 0;  // 0x65b7-0x65be
    if (!v) {
        st<u32>(ctx, outN, 0);  // 0x65c0 / 0x65c4
        if (pad) vfill(ctx, dst, 0, count << 3);  // 0x65c6-0x65e7
        return 0x26;  // 0x65eb
    }
    st<u32>(ctx, dAddr, dst);  // 0x65f7 / 0x6601
    st<uint32_t>(ctx, posAddr, ld<uint32_t>(ctx, v + 0x1c));  // 0x6605 / 0x6614
    const unsigned got = snd_call_va(ctx, fn, SndAbi::Cdecl, {dAddr, posAddr, count, v, 0}).eax;  // 0x6608-0x6618
    st<u32>(ctx, outN, got);  // 0x661c / 0x6629
    unsigned k = ld<uint32_t>(ctx, v + 0xcc);  // 0x662b
    if (k) {  // 0x6631 / 0x6633
        do {
            if (u32 c = ld<u32>(ctx, self + 0x64)) st<u32>(ctx, self + 0x64, ld<u32>(ctx, c + 4));  // 0x663b-0x6645
            c3fq_popHead(ctx, self);  // 0x6648-0x6670
            snd_import(ctx, 0x000b8670, SndAbi::Stdcall, {ld<u32>(ctx, v + 0x44), 0}).eax;  // 0x6673-0x6678
            st<int32_t>(ctx, snd_obj(ctx, &SoundState::g_openMmioCount).va(), ld<int32_t>(ctx, snd_obj(ctx, &SoundState::g_openMmioCount).va()) - 1);  // 0x667e-0x6687
            if (v) c3fq_dropVoice(ctx, v);  // 0x6685-0x669c
            n = ld<u32>(ctx, self + 0x5c);  // 0x669f
            st<u32>(ctx, self + 0x64, n);  // 0x66a4
            v = n ? ld<u32>(ctx, n + 8) : 0;  // 0x66a7-0x66ae
        } while (--k);  // 0x66b0 / 0x66b1
    }
    if (v) {  // 0x66bb
        const unsigned pos = ld<uint32_t>(ctx, posAddr);
        st<uint32_t>(ctx, v + 0x1c, pos);  // 0x66c3 / 0x66c7
        if (pos >= ld<uint32_t>(ctx, v + 0x20) && !(ld<uint8_t>(ctx, v + 0x9c) & 1)) {  // 0x66ca-0x66de
            const unsigned c8 = ld<uint32_t>(ctx, self + 0xc8) + 1;  // 0x66e4 / 0x66f0
            st<uint32_t>(ctx, self + 0xcc, ld<uint32_t>(ctx, self + 0xcc) + c8);  // 0x66ea / 0x66f3 / 0x66f7
            st<uint32_t>(ctx, self + 0xc8, 0);  // 0x6703
            st<uint32_t>(ctx, self + 0x58, (ld<uint32_t>(ctx, self + 0x58) & ~0x10u) | 4);  // 0x66fd-0x670c
            snd_vcall(ctx, self, 0x20, SndAbi::Thiscall, {}).eax;  // 0x670f
            c3fq_popHead(ctx, self);  // 0x6712-0x673a
            snd_import(ctx, 0x000b8670, SndAbi::Stdcall, {ld<u32>(ctx, v + 0x44), 0}).eax;  // 0x673d-0x6742
            st<int32_t>(ctx, snd_obj(ctx, &SoundState::g_openMmioCount).va(), ld<int32_t>(ctx, snd_obj(ctx, &SoundState::g_openMmioCount).va()) - 1);  // 0x6748-0x6751
            c3fq_dropVoice(ctx, v);  // 0x6757-0x675d
        }
    }
    if (got != count && pad) vfill(ctx, ld<u32>(ctx, dAddr), 0, (count - got) << 3);  // 0x6769-0x678c
    return 0;  // 0x6791
}
void a_Wave_Device__freeBufferList(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Wave_Device__freeBufferList), Wave_Device__freeBufferList_body(ctx, a[0], a[1], a[2], a[3], a[4]))); }

// ---- Wave_Device::appendBuffer (rva 0x6850): hand-fixed conversion of c3fq.cpp AppendBuf_re
uint32_t Wave_Device__appendBuffer_body(Ctx &ctx, u32 self, u32 buf) {
    if (buf != self + 0x6c) return 0xa;  // 0x686f-0x6878
    const unsigned d = ld<uint32_t>(ctx, buf + 0x20) - ld<uint32_t>(ctx, buf + 0x18);  // 0x688f-0x6897
    if (!d) {
        snd_import(ctx, 0x000b8670, SndAbi::Stdcall, {ld<u32>(ctx, self + 0xb0), 0}).eax;  // 0x689d-0x68a7
        st<int32_t>(ctx, snd_obj(ctx, &SoundState::g_openMmioCount).va(), ld<int32_t>(ctx, snd_obj(ctx, &SoundState::g_openMmioCount).va()) - 1);  // 0x68ad-0x68b3
        st<u32>(ctx, self + 0xb0, 0);  // 0x68b8
        return 0x26;  // 0x68c0
    }
    u32 p = snd_new(ctx, 0x110);  // 0x68d8 / 0x68dd
    u32 v = p ? snd_call(ctx, 0x00001e56, SndAbi::Thiscall, {p}).eax : 0;  // 0x68f1-0x68fe
    snd_call(ctx, 0x000017a8, SndAbi::Thiscall, {v, buf, self + 0xd8}).eax;  // 0x6900-0x6912
    u32 tail = ld<u32>(ctx, self + 0x60);  // 0x6917
    st<u32>(ctx, self + 0x64, tail);  // 0x691c
    u32 prevVoice = tail ? ld<u32>(ctx, tail + 8) : 0;  // 0x691f-0x6926
    u32 node = snd_new(ctx, 0xc);  // 0x6928 / 0x692a
    if (node) { st<u32>(ctx, node, 0); st<u32>(ctx, node + 4, 0); st<u32>(ctx, node + 8, v); }  // 0x6936-0x693b
    if (ld<u32>(ctx, self + 0x60)) {  // 0x6942-0x6947
        st<u32>(ctx, ld<u32>(ctx, self + 0x60) + 4, node);  // 0x6949
        st<u32>(ctx, node, ld<u32>(ctx, self + 0x60));  // 0x694c / 0x694f
    } else {
        st<u32>(ctx, self + 0x5c, node);  // 0x6953
    }
    st<u32>(ctx, self + 0x60, node);  // 0x6959
    st<uint32_t>(ctx, self + 0x68, ld<uint32_t>(ctx, self + 0x68) + 1);  // 0x6956 / 0x6960 / 0x6961
    st<u32>(ctx, self + 0xb0, 0);  // 0x6966
    const unsigned q = udiv32(d, ld<uint16_t>(ctx, v + 0xac));  // 0x696c-0x6975
    st<uint32_t>(ctx, v + 0xd4, ld<uint32_t>(ctx, v + 0x20));  // 0x6977 / 0x697c
    st<uint32_t>(ctx, v + 0x90, q);  // 0x6987
    st<uint32_t>(ctx, v + 0xd8, ld<uint32_t>(ctx, v + 0x18));  // 0x698d / 0x6990
    if (prevVoice) {  // 0x697a / 0x6996
        st<uint32_t>(ctx, prevVoice + 0x98, static_cast<uint32_t>(ld<uint32_t>(ctx, prevVoice + 0x98) | 1));  // 0x6998-0x69a0
        st<uint8_t>(ctx, prevVoice + 0x9c, static_cast<uint8_t>(ld<uint8_t>(ctx, prevVoice + 0x9c) | 1));  // 0x69a6-0x69ae
        st<u32>(ctx, prevVoice + 0xb4, v);  // 0x69b4
        st<uint32_t>(ctx, prevVoice + 0xcc, 0);  // 0x69ba
    }
    const unsigned f = ld<uint32_t>(ctx, self + 0x58);  // 0x69c0
    if ((f & 4) && !(f & 1)) st<uint32_t>(ctx, self + 0x58, (f & ~4u) | 3);  // 0x69c4-0x69d0
    return 0;  // 0x69d9
}
void a_Wave_Device__appendBuffer(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Wave_Device__appendBuffer), Wave_Device__appendBuffer_body(ctx, a[0], a[1]))); }

// ---- startStream (rva 0x6b00): hand-fixed conversion of c3cf.cpp StartStream_re
int32_t startStream_body(Ctx &ctx, u32 self, int32_t param) {
    unsigned state = ld<uint32_t>(ctx, self + 0x58);  // 0x6b00
    if (state & 1) return 0xd;  // test al, 1 at 0x6b03
    st<int32_t>(ctx, self + 0xcc, param);  // 0x6b17
    st<uint32_t>(ctx, self + 0x58, (state & 0xfffffffbu) | 3u);  // and al, 0xfb / or al, 3; stored 0x6b1d
    st<int32_t>(ctx, self + 0xc8, 0);  // 0x6b22
    snd_vcall(ctx, self, 0x7c, SndAbi::Thiscall, {});  // call at 0x6b2c (no stack argument; edx still holds `param`)
    return 0;  // 0x6b2f
}
void a_startStream(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_i32(static_cast<int32_t>(neg_u32(NEG(startStream), static_cast<uint32_t>(startStream_body(ctx, a[0], static_cast<int32_t>(a[1])))))); }

// ---- computeCaptureDuration (rva 0x6bd0): converted from c3au.cpp ComputeCaptureDuration_re
void computeCaptureDuration_body(Ctx &ctx, u32 self) {
    unsigned total = ld<uint32_t>(ctx, self + 0xb4);
    if (total == 0) return;
    int span = ld<int32_t>(ctx, self + 0xd4);
    if (span == 0) return;
    unsigned unitSize = total / static_cast<uint32_t>(ld<uint16_t>(ctx, self + 0xe4));
    st<int32_t>(ctx, self + 0xc8, span / static_cast<int32_t>(unitSize));
    int rem = span % static_cast<int32_t>(unitSize);
    const double thousand = ld<double>(ctx, snd_va(ctx, 0x5b280));
    double v = static_cast<double>(rem) / static_cast<double>(ld<uint32_t>(ctx, self + 0xdc)) * thousand;
    st<int32_t>(ctx, self + 0xc0, static_cast<int32_t>(static_cast<long long>(v)));  // __ftol: 64-bit truncation, low dword
}
void a_computeCaptureDuration(Ctx &ctx, const uint32_t *a, Ret &) { computeCaptureDuration_body(ctx, a[0]); neg_mem(ctx, NEG(computeCaptureDuration), a[0] + 0xc0); }

// ---- initStreamBuffer (rva 0x6e00): converted from c3cx.cpp InitStreamBuffer_re
u32 initStreamBuffer_body(Ctx &ctx, u32 self) {
    u32 p = self;
    st<uint32_t>(ctx, p, 0);  // 0x6e0b
    st<uint32_t>(ctx, p + 0x04, 0);  // 0x6e0d
    st<uint32_t>(ctx, p + 0x10, 2);  // 0x6e10
    st<uint32_t>(ctx, p + 0x14, 0);  // 0x6e17
    st<uint32_t>(ctx, p + 0x0c, 0x4000);  // 0x6e1a
    st<u32>(ctx, p + 0x04, snd_call(ctx, 0x000424d7, SndAbi::Cdecl, {0x20000}).eax);  // 0x6e21, 0x6e26
    st<uint32_t>(ctx, p + 0x08, static_cast<uint32_t>(ld<uint32_t>(ctx, p + 0x08) & (~1u)));  // 0x6e29..0x6e30
    st<uint32_t>(ctx, p + 0x18, 0);  // 0x6e38
    st<uint32_t>(ctx, p + 0x30, 0);  // 0x6e3b
    st<uint32_t>(ctx, p + 0x38, 0);  // 0x6e3e
    st<uint16_t>(ctx, p + 0x1c, 1);  // 0x6e41, 0x6e53
    st<uint16_t>(ctx, p + 0x1e, 2);  // 0x6e5f
    st<uint32_t>(ctx, p + 0x20, 0x5622);  // 0x6e65
    st<uint32_t>(ctx, p + 0x24, 0x15888);  // 0x6e6c
    st<uint16_t>(ctx, p + 0x28, 4);  // 0x6e73
    st<uint16_t>(ctx, p + 0x2a, 0x10);  // 0x6e79
    st<uint16_t>(ctx, p + 0x2c, 0);  // 0x6e4f, 0x6e58
    st<uint32_t>(ctx, p + 0x34, 0);  // 0x6e5c
    return self;
}
void a_initStreamBuffer(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(initStreamBuffer), initStreamBuffer_body(ctx, a[0]))); }

// ---- writeWaveFile (rva 0x7010): hand-fixed conversion of c3fl.cpp WriteWave_re
constexpr uint32_t kIatMmioWrite = 0xb8658;
constexpr uint32_t kIatTimeGetTime = 0xb8654;
uint32_t c3fl_u(Ctx &ctx, u32 p) { (void)ctx; return p; }
// The format code the three stream calls take (0x7103-0x7142, 0x71b2-0x71f1, 0x7249-0x7288): 0, or with
// the object's word at wOff = 8 (8-bit), 8 when the writer's word +0x2a is 16 and 4 otherwise (`sub ax, 0x10; neg ax; sbb eax,
// eax; and al, 0xfc; add eax, 8`); +2 when the object's word at cOff is 1; +1 when the writer's word +0x1e is 1; +0xc on the first
// call of the pass, which clears `first`.
uint32_t c3fl_waveMode(Ctx &ctx, u32 wr, u32 o, int32_t wOff, int32_t cOff, unsigned& first) {
    unsigned m = 0;
    if (ld<uint16_t>(ctx, o + wOff) == 8) m = (ld<uint16_t>(ctx, wr + 0x2a) == 0x10) ? 8 : 4;
    if (ld<uint16_t>(ctx, o + cOff) == 1) m += 2;
    if (ld<uint16_t>(ctx, wr + 0x1e) == 1) m += 1;
    if (first) { m += 0xc; first = 0; }
    return m;
}
// Frame (entry esp E; `sub esp, 0x24` + 4 pushes = 0x34 bytes): `bytes` E-8 and `offset` E-0xc (the out slots of DsBuffer::getPosition,
// 0x7049-0x7056), `local` E-0x24 (the out slot of the stream calls, 0x7148), and the DsBuffer::restore out slots p1 E-0x10, n1 E-0x14,
// p2 E-0x20, n2 E-0x18 (0x73a8-0x73ca). frames / elapsed are registers or slots no callee addresses.
uint32_t writeWaveFile_body(Ctx &ctx, u32 self, u32 buf) {
    SndFrame F(ctx, 0x34);
    const u32 bytesA = F.at(8), offsetA = F.at(0xc), localA = F.at(0x24);
    const u32 p1A = F.at(0x10), n1A = F.at(0x14), p2A = F.at(0x20), n2A = F.at(0x18);
    u32 wr = self;
    if (!buf) return 0xa;  // 0x701d / 0x701f
    unsigned t = snd_import(ctx, kIatTimeGetTime, SndAbi::Stdcall, {}).eax;  // 0x7030
    if (ld<uint32_t>(ctx, wr + 0x18) == 0) st<uint32_t>(ctx, wr + 0x18, t);  // 0x7038-0x703f
    unsigned r = snd_call(ctx, 0x0000233d, SndAbi::Thiscall, {buf, bytesA, offsetA, t - ld<uint32_t>(ctx, wr + 0x18)}).eax;  // 0x7042-0x7056
    if (r != 0) return r;  // 0x705b / 0x705d
    const unsigned bytes = ld<uint32_t>(ctx, bytesA), offset = ld<uint32_t>(ctx, offsetA);
    unsigned short W = ld<uint16_t>(ctx, buf + 0xb4);  // 0x7063
    if (W == 0) return 0;  // 0x706a / 0x706d
    unsigned frames = udiv32(bytes, W);  // 0x706f-0x707b
    if (frames == 0) return 0;  // 0x707d / 0x7083
    unsigned elapsed = t - ld<uint32_t>(ctx, wr + 0x18);  // 0x7091-0x7096
    st<uint32_t>(ctx, wr + 0x18, t);  // 0x7098
    st<uint32_t>(ctx, wr + 0x34, 0);  // 0x709b
    unsigned first = 1;  // 0x70ae
    u32 eng = ld<u32>(ctx, snd_obj(ctx, &SoundState::g_soundEngine).va());  // 0x70a2
    if (!eng) return 3;  // 0x70a8 / 0x70b3
    u32 cur = ld<u32>(ctx, eng + 0x1c8);  // 0x70b9
    st<u32>(ctx, eng + 0x1d0, cur);  // 0x70c1
    u32 o = cur ? ld<u32>(ctx, cur + 8) : 0;  // 0x70bf-0x70d0
    if (o) {
        for (;;) {
            unsigned kind = snd_vcall(ctx, o, 0x78, SndAbi::Thiscall, {}).eax;  // 0x70d8-0x70dc
            if (kind == 6) {  // 0x70eb / 0x70ee
                if (snd_vcall(ctx, o, 0x5c, SndAbi::Thiscall, {}).eax != 0) {  // 0x70f4-0x70fd
                    unsigned m = c3fl_waveMode(ctx, wr, o, 0xca, 0xbe, first);  // 0x7103-0x7142
                    snd_call(ctx, 0x000016a9, SndAbi::Thiscall, {o, snd_va(ctx, 0x69990), frames, m, localA}).eax;  // 0x7144-0x7156
                }
            } else if (kind == 4) {  // 0x70e6 / 0x70e9
                if (snd_vcall(ctx, o, 0x5c, SndAbi::Thiscall, {}).eax != 0) {  // 0x7160-0x7169
                    unsigned f = ld<uint32_t>(ctx, o + 0x58);  // 0x716f
                    if (!(f & 0x10)) {  // 0x7172 / 0x7174
                        st<uint32_t>(ctx, o + 0xd0, offset);  // 0x717e
                        st<uint32_t>(ctx, o + 0xd4, 0u - ld<uint32_t>(ctx, buf + 0x1c));  // 0x7184-0x718b
                        st<uint32_t>(ctx, o + 0x58, f | 0x10);  // 0x7189 / 0x7191
                    } else {
    st<uint32_t>(ctx, o + 0xd4, static_cast<uint32_t>(ld<uint32_t>(ctx, o + 0xd4) + (ld<uint32_t>(ctx, buf + 0x18))));  // 0x719a-0x71a5
                    }
                    snd_call(ctx, 0x000022f2, SndAbi::Thiscall, {o}).eax;  // 0x71ab / 0x71ad
                    unsigned m = c3fl_waveMode(ctx, wr, o, 0xe6, 0xda, first);  // 0x71b2-0x71f1
                    snd_call(ctx, 0x00001c6c, SndAbi::Thiscall, {o, ld<uint32_t>(ctx, wr + 4), frames, m, localA}).eax;  // 0x71f3-0x7204
                }
            } else if (kind == 1) {  // 0x70df / 0x70e0
                if (snd_vcall(ctx, o, 0x60, SndAbi::Thiscall, {}).eax != 0) {  // 0x720e-0x7217
                    u32 c = o + 0x70;
                    bool alt = true;  // reaches 0x7316
                    if (!(ld<uint8_t>(ctx, o + 0xcc) & 4)) {  // 0x721d / 0x7224
                        if (snd_vcall(ctx, o, 0x5c, SndAbi::Thiscall, {}).eax != 0 && snd_vcall(ctx, o, 0xcc, SndAbi::Thiscall, {}).eax == 0) {  // 0x722a-0x7241
                            st<uint32_t>(ctx, o + 0x2a0, elapsed);  // 0x7243
                            unsigned m = c3fl_waveMode(ctx, wr, o, 0x1fe, 0x1f2, first);  // 0x7249-0x7288
                            snd_call(ctx, 0x0000119f, SndAbi::Thiscall, {c, snd_va(ctx, 0x69990), frames, m, localA}).eax;  // 0x728a-0x729d
                        }
                        if (!(ld<uint8_t>(ctx, o + 0xcc) & 4)) {  // 0x72a2 / 0x72a9
                            if (snd_vcall(ctx, o, 0x5c, SndAbi::Thiscall, {}).eax != 0 && snd_vcall(ctx, o, 0xcc, SndAbi::Thiscall, {}).eax != 0)  // 0x72ab-0x72c2
                                snd_call(ctx, 0x00001113, SndAbi::Thiscall, {c}).eax;  // 0x72c4 / 0x72c7
                            if (!(ld<uint8_t>(ctx, o + 0xcc) & 4)) {  // 0x72cc / 0x72d3
                                if (snd_vcall(ctx, o, 0x5c, SndAbi::Thiscall, {}).eax != 0 && snd_vcall(ctx, o, 0xcc, SndAbi::Thiscall, {}).eax != 0 &&
                                    !(ld<uint8_t>(ctx, o + 0xc8) & 1)) {  // 0x72d5-0x72f5
                                    snd_call(ctx, 0x000020c7, SndAbi::Thiscall, {c, elapsed, 0, localA}).eax;  // 0x72f7-0x7302
                                    st<uint32_t>(ctx, o + 0x2a0, elapsed);  // 0x7307
                                }
                                if (!(ld<uint8_t>(ctx, o + 0xcc) & 4)) alt = false;  // 0x730d / 0x7314
                            }
                        }
                    }
                    if (alt) {
    if (snd_vcall(ctx, o, 0xcc, SndAbi::Thiscall, {}).eax != 0) {  // 0x7316-0x7322
                            snd_call(ctx, 0x00001302, SndAbi::Thiscall, {c, elapsed, 0, localA}).eax;  // 0x7324-0x732f
                            st<uint32_t>(ctx, o + 0x2a0, elapsed);  // 0x7334
                        }
                        if (ld<uint8_t>(ctx, o + 0xcc) & 4) {  // 0x733a / 0x7341
                            u32 b = ld<u32>(ctx, o + 0xc4);  // 0x7343
                            if (b && snd_call(ctx, 0x00001b9f, SndAbi::Thiscall, {b}).eax == 0) {  // 0x7349-0x7354
                                st<uint32_t>(ctx, o + 0xc8, static_cast<uint32_t>(ld<uint32_t>(ctx, o + 0xc8) | 0x10));  // 0x7356-0x7363
                                for (unsigned i = 0; i < 0x10; i++) snd_call(ctx, 0x0000154b, SndAbi::Thiscall, {c, i}).eax;  // 0x7361-0x7372
                            }
                        }
                    }
                }
            }
            eng = ld<u32>(ctx, snd_obj(ctx, &SoundState::g_soundEngine).va());  // 0x7378
            cur = ld<u32>(ctx, eng + 0x1d0);  // 0x737e
            if (!cur) break;  // 0x7384 / 0x7386
            cur = ld<u32>(ctx, cur + 4);  // 0x7388
            st<u32>(ctx, eng + 0x1d0, cur);  // 0x738d
            if (!cur) break;  // 0x738b / 0x7393
            o = ld<u32>(ctx, cur + 8);  // 0x7395
            if (!o) break;  // 0x7398 / 0x739a
        }
        if (!first) {  // 0x73a0 / 0x73a2
            snd_call(ctx, 0x00001b13, SndAbi::Thiscall, {wr, buf, snd_va(ctx, 0x69990), bytes, offset}).eax;  // 0x7465-0x747b
            [&] { SndResidue keep(ctx); return snd_call(ctx, 0x00001f0f, SndAbi::Thiscall, {wr}).eax; }();  // 0x7480 / 0x7482 (ecx loaded, not read: flushSoundList takes no argument)
            return 0;
        }
    }
    snd_call(ctx, 0x0000213f, SndAbi::Thiscall, {buf, offset, bytes, p1A, n1A, p2A, n2A}).eax;  // 0x73a8-0x73ca
    const u32 p1 = ld<u32>(ctx, p1A), p2 = ld<u32>(ctx, p2A);
    const unsigned n1 = ld<uint32_t>(ctx, n1A), n2 = ld<uint32_t>(ctx, n2A);
    vfill(ctx, p1, 0, n1);  // 0x73cf-0x73e5
    if (p2) vfill(ctx, p2, 0, n2);  // 0x73e7-0x7401
    unsigned h = ld<uint32_t>(ctx, wr + 0x30);  // 0x7407
    if (h) {  // 0x740a / 0x740c
        snd_import(ctx, kIatMmioWrite, SndAbi::Stdcall, {h, p1, n1}).eax;  // 0x740e-0x741f
        if (p2) snd_import(ctx, kIatMmioWrite, SndAbi::Stdcall, {ld<uint32_t>(ctx, wr + 0x30), p2, n2}).eax;  // 0x7421-0x7433
    }
    snd_call(ctx, 0x00001ec9, SndAbi::Thiscall, {buf, p1, n1, p2, n2}).eax;  // 0x7439-0x744d
    [&] { SndResidue keep(ctx); return snd_call(ctx, 0x00001f0f, SndAbi::Thiscall, {wr}).eax; }();  // 0x7452 / 0x7454 (ecx loaded, not read: flushSoundList takes no argument)
    return 0;  // 0x745c
}
void a_writeWaveFile(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(writeWaveFile), writeWaveFile_body(ctx, a[0], a[1]))); }

// ---- writeWaveFileB (rva 0x75e0): hand-written from the instructions: the stereo twin of writeWaveFile
// `ret 8`: this = ecx (the wave writer), buf (E+4) the left capture DsBuffer and `buf2` (E+8) the right one; either NULL returns 0xa
// (0x75e4 / 0x75ed / 0x75f7, 0x7a8b). timeGetTime (0x7601) as in writeWaveFile; DsBuffer::getPosition (thunk 0x233d) on buf with
// (&bytes, &offset, elapsed) (0x7627): a non-zero result other than 0x2d is returned (0x7630 / 0x7a90), 0x2d sets bit 0 of the byte at
// buf2 + 4 and goes on (0x7639-0x763d). DsBuffer::lockCaptureRegion (thunk 0x24b9) on buf2 with (bytes, &lockOut) (0x7647-0x7651): a
// non-zero result is returned. frames = bytes / the block align word +0xb4 of buf (0 when that word is 0, 0x765e-0x7682; unlike
// writeWaveFile a zero count does not return). The wave writer's clock +0x18 and +0x34 as in writeWaveFile; g_soundEngine NULL
// returns 3 (0x769f / 0x7a7c). The engine's object list is walked as in writeWaveFile (kinds 6, 4 and 1: 0x76c5-0x7905) with the
// differences listed in the code; with no object taken the two buffers' regions are locked (DsBuffer::restore on buf with
// (offset, bytes, &p1, &b1, &p2, &b2) and on buf2 with (lockOut, bytes, &p1', &b1', &p2', &b2'), 0x7913-0x795c), zero-filled
// (0x7961-0x79c7), the first buffer's regions mmioWritten to +0x30 (0x79c9-0x79f5), unlocked (0x79f7-0x7a2c); with an object taken
// writeWaveChunkB (thunk 0x1cb2) does it (0x7a44-0x7a64). flushSoundList (thunk 0x1f0f) and return 0.
// Frame (entry esp E; `sub esp, 0x38` and four pushes = 0x48 bytes): bytes E-0x34, offset E-8, lockOut E-0xc, elapsed E-4, `local` E-0x30
// (the out slot of the stream calls), buf's regions p1 E-0x20, b1 E-0x24, p2 E-0x28, b2 E-0x2c and buf2's p1' E-0x10, b1' E-0x14,
// p2' E-0x18, b2' E-0x1c.
uint32_t writeWaveFileB_body(Ctx &ctx, u32 self, u32 buf, u32 buf2) {
    SndFrame F(ctx, 0x48);
    const u32 bytesA = F.at(0x34), offsetA = F.at(8), lockA = F.at(0xc), localA = F.at(0x30);
    const u32 p1A = F.at(0x20), b1A = F.at(0x24), p2A = F.at(0x28), b2A = F.at(0x2c);
    const u32 p1pA = F.at(0x10), b1pA = F.at(0x14), p2pA = F.at(0x18), b2pA = F.at(0x1c);
    u32 wr = self;
    if (!buf || !buf2) return 0xa;  // 0x75ed / 0x75f1 / 0x75f7 / 0x75fb -> 0x7a8b
    unsigned t = snd_import(ctx, kIatTimeGetTime, SndAbi::Stdcall, {}).eax;  // 0x7601
    if (ld<uint32_t>(ctx, wr + 0x18) == 0) st<uint32_t>(ctx, wr + 0x18, t);  // 0x7609-0x7610
    unsigned r = snd_call(ctx, 0x0000233d, SndAbi::Thiscall, {buf, bytesA, offsetA, t - ld<uint32_t>(ctx, wr + 0x18)}).eax;  // 0x7616-0x7627
    if (r != 0) {  // 0x762c / 0x762e
        if (r != 0x2d) return r;  // 0x7630 / 0x7633 -> 0x7a90
        st<uint8_t>(ctx, buf2 + 4, static_cast<uint8_t>(ld<uint8_t>(ctx, buf2 + 4) | 1));  // 0x7639-0x763d
    }
    r = snd_call(ctx, 0x000024b9, SndAbi::Thiscall, {buf2, ld<uint32_t>(ctx, bytesA), lockA}).eax;  // 0x7647-0x7651
    if (r != 0) return r;  // 0x7656 / 0x7658
    const unsigned bytes = ld<uint32_t>(ctx, bytesA), offset = ld<uint32_t>(ctx, offsetA);
    unsigned frames = 0;  // [esp + 0x10]
    const uint16_t ba = ld<uint16_t>(ctx, buf + 0xb4);  // 0x765e
    if (ba != 0) frames = udiv32(bytes, ba);  // 0x7665-0x7678
    unsigned elapsed = t - ld<uint32_t>(ctx, wr + 0x18);  // 0x7682-0x7687
    st<uint32_t>(ctx, wr + 0x18, t);  // 0x7689
    st<uint32_t>(ctx, wr + 0x34, 0);  // 0x768c
    unsigned first = 1;  // ebx, 0x769b
    u32 eng = ld<u32>(ctx, snd_obj(ctx, &SoundState::g_soundEngine).va());  // 0x768f
    if (!eng) return 3;  // 0x7695 / 0x76a0 -> 0x7a7c
    u32 cur = ld<u32>(ctx, eng + 0x1c8);  // 0x76a6
    st<u32>(ctx, eng + 0x1d0, cur);  // 0x76ae
    u32 o = cur ? ld<u32>(ctx, cur + 8) : 0;  // 0x76ba-0x76bf
    if (o) {
        for (;;) {
            unsigned kind = snd_vcall(ctx, o, 0x78, SndAbi::Thiscall, {}).eax;  // 0x76c5-0x76c9
            if (kind == 6) {  // 0x76d8 / 0x76db
                if (snd_vcall(ctx, o, 0x5c, SndAbi::Thiscall, {}).eax != 0) {  // 0x76e1-0x76ea
                    unsigned m = c3fl_waveMode(ctx, wr, o, 0xca, 0xbe, first);  // 0x76f0-0x7731
                    snd_call(ctx, 0x000016a9, SndAbi::Thiscall, {o, snd_va(ctx, 0x69990), frames, m, localA}).eax;  // 0x7731-0x7743
                }
            } else if (kind == 4) {  // 0x76d3 / 0x76d6
                if (snd_vcall(ctx, o, 0x5c, SndAbi::Thiscall, {}).eax != 0) {  // 0x774d-0x7756
                    unsigned f = ld<uint32_t>(ctx, o + 0x58);  // 0x775c
                    if (!(f & 0x10)) {  // 0x775f / 0x7761
                        st<uint32_t>(ctx, o + 0xd0, 0);  // 0x7767
                        st<uint32_t>(ctx, o + 0xd4, 0u - ld<uint32_t>(ctx, buf + 0x1c));  // 0x776d-0x7774
                        st<uint32_t>(ctx, o + 0x58, f | 0x10);  // 0x7772 / 0x777a
                    } else {
    st<uint32_t>(ctx, o + 0xd4, ld<uint32_t>(ctx, o + 0xd4) + ld<uint32_t>(ctx, buf + 0x18));  // 0x777f-0x778e
                    }
                    snd_call(ctx, 0x000022f2, SndAbi::Thiscall, {o}).eax;  // 0x7794
                    unsigned m = c3fl_waveMode(ctx, wr, o, 0xe6, 0xda, first);  // 0x779b-0x77dc
                    snd_call(ctx, 0x00001c6c, SndAbi::Thiscall, {o, ld<uint32_t>(ctx, wr + 4), frames, m, localA}).eax;  // 0x77dc-0x77ed
                }
            } else if (kind == 1) {  // 0x76cc / 0x76cd
                if (snd_vcall(ctx, o, 0x60, SndAbi::Thiscall, {}).eax != 0) {  // 0x77f7-0x7800
                    const u32 c = o + 0x70;
                    if (snd_vcall(ctx, o, 0xcc, SndAbi::Thiscall, {}).eax == 0 && snd_vcall(ctx, o, 0x5c, SndAbi::Thiscall, {}).eax != 0) {  // 0x7806-0x781d
                        st<uint32_t>(ctx, o + 0x2a0, elapsed);  // 0x781f / 0x7823
                        unsigned m = c3fl_waveMode(ctx, wr, o, 0x1fe, 0x1f2, first);  // 0x7829-0x786a
                        snd_call(ctx, 0x0000119f, SndAbi::Thiscall, {c, snd_va(ctx, 0x69990), frames, m, localA}).eax;  // 0x786a-0x787d
                    }
                    if (((ld<uint32_t>(ctx, o + 0xcc) >> 6) & 1) != 0)  // 0x7882-0x788d
                        snd_call(ctx, 0x00001302, SndAbi::Thiscall, {c, frames, 0, localA}).eax;  // 0x788f-0x789d
                    if (snd_vcall(ctx, o, 0xcc, SndAbi::Thiscall, {}).eax != 0) {  // 0x78a2-0x78ae
                        u32 b = ld<u32>(ctx, o + 0xc4);  // 0x78b0
                        if (b && snd_call(ctx, 0x00001b9f, SndAbi::Thiscall, {b}).eax == 0) {  // 0x78b6-0x78c1
                            st<uint32_t>(ctx, o + 0xc8, ld<uint32_t>(ctx, o + 0xc8) | 0x10);  // 0x78c3-0x78d0
                            for (unsigned i = 0; i < 0x10; i++) snd_call(ctx, 0x0000154b, SndAbi::Thiscall, {c, i}).eax;  // 0x78d3-0x78df
                        }
                    }
                }
            }
            eng = ld<u32>(ctx, snd_obj(ctx, &SoundState::g_soundEngine).va());  // 0x78e3
            cur = ld<u32>(ctx, eng + 0x1d0);  // 0x78e9
            if (!cur) break;  // 0x78ef / 0x78f1
            cur = ld<u32>(ctx, cur + 4);  // 0x78f3
            st<u32>(ctx, eng + 0x1d0, cur);  // 0x78f8
            if (!cur) break;  // 0x78f6 / 0x78fe
            o = ld<u32>(ctx, cur + 8);  // 0x7900
            if (!o) break;  // 0x7903 / 0x7905
        }
    }
    if (!first) {  // 0x790b / 0x790d -> 0x7a44
        snd_call(ctx, 0x00001cb2, SndAbi::Thiscall, {wr, buf, buf2, snd_va(ctx, 0x69990), bytes, offset, ld<uint32_t>(ctx, lockA)}).eax;  // 0x7a44-0x7a64
        [&] { SndResidue keep(ctx); return snd_call(ctx, 0x00001f0f, SndAbi::Thiscall, {wr}).eax; }();  // 0x7a69 (ecx loaded, not read: flushSoundList takes no argument)
        return 0;  // 0x7a73
    }
    snd_call(ctx, 0x0000213f, SndAbi::Thiscall, {buf, offset, bytes, p1A, b1A, p2A, b2A}).eax;  // 0x7913-0x7935
    snd_call(ctx, 0x0000213f, SndAbi::Thiscall, {buf2, ld<uint32_t>(ctx, lockA), bytes, p1pA, b1pA, p2pA, b2pA}).eax;  // 0x793a-0x795c
    vfill(ctx, ld<u32>(ctx, p1A), 0, ld<uint32_t>(ctx, b1A));  // 0x7961-0x7977
    vfill(ctx, ld<u32>(ctx, p1pA), 0, ld<uint32_t>(ctx, b1pA));  // 0x7979-0x798f
    if (ld<u32>(ctx, p2A)) vfill(ctx, ld<u32>(ctx, p2A), 0, ld<uint32_t>(ctx, b2A));  // 0x7991-0x79ab
    if (ld<u32>(ctx, p2pA)) vfill(ctx, ld<u32>(ctx, p2pA), 0, ld<uint32_t>(ctx, b2pA));  // 0x79ad-0x79c7
    if (ld<u32>(ctx, wr + 0x30)) {  // 0x79c9 / 0x79cc
        snd_import(ctx, kIatMmioWrite, SndAbi::Stdcall, {ld<u32>(ctx, wr + 0x30), ld<u32>(ctx, p1A), ld<uint32_t>(ctx, b1A)}).eax;  // 0x79d0-0x79e1
        if (ld<u32>(ctx, p2A)) snd_import(ctx, kIatMmioWrite, SndAbi::Stdcall, {ld<u32>(ctx, wr + 0x30), ld<u32>(ctx, p2A), ld<uint32_t>(ctx, b2A)}).eax;  // 0x79e3-0x79f5
    }
    snd_call(ctx, 0x00001ec9, SndAbi::Thiscall, {buf, ld<u32>(ctx, p1A), ld<uint32_t>(ctx, b1A), ld<u32>(ctx, p2A), ld<uint32_t>(ctx, b2A)});  // 0x79f7-0x7a0f
    snd_call(ctx, 0x00001ec9, SndAbi::Thiscall, {buf2, ld<u32>(ctx, p1pA), ld<uint32_t>(ctx, b1pA), ld<u32>(ctx, p2pA), ld<uint32_t>(ctx, b2pA)});  // 0x7a14-0x7a2c
    [&] { SndResidue keep(ctx); return snd_call(ctx, 0x00001f0f, SndAbi::Thiscall, {wr}).eax; }();  // 0x7a31 (ecx loaded, not read: flushSoundList takes no argument)
    return 0;  // 0x7a3b
}
void a_writeWaveFileB(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(writeWaveFileB), writeWaveFileB_body(ctx, a[0], a[1], a[2]))); }

// ---- writeWaveChunk (rva 0x7df0): hand-written from the instructions (shim/src/re/c3fn.cpp holds a compiled-out hook that agrees)
// `fld qword [src]; fistp dword [v]` under the control word `fninit` sets (0x037f: 64-bit precision, round to nearest even,
// 0x7e5e / 0x7f2a): a value outside int32 after rounding, or a NaN, stores the x87 integer indefinite 0x80000000.
int32_t c3fn_fistRN(double d) {
    const double r = std::nearbyint(d);
    if (!(r >= -2147483648.0 && r <= 2147483647.0)) return static_cast<int32_t>(0x80000000u);
    return static_cast<int32_t>(r);
}
// the clamp loop at 0x7e69-0x7e8d (and 0x7f35-0x7f59): `count` doubles at src become 16-bit words at dst, 0x7fff above 0x7fff, 0x8000
// below -0x8000, else the low word. The loop ends with `dec ecx; jne`, so a count of 0 runs 2^32 times (it faults at the first
// unmapped page: U-1481); the port keeps the do-while.
void c3fn_clampLoop(Ctx &ctx, u32 dst, u32 src, uint32_t count, uint32_t stride = 8) {
    // every call site runs `fninit` just before the loop (0x7e5e, 0x7f2a, 0x8121, 0x81aa, 0x824a, 0x82e8): on the x87 unit
    // (S4.2b) the control word 0x037f, the status and the tags it leaves are the function's exit state
    x87::unit(ctx).fninit();
    uint32_t n = count;
    do {
        const int32_t v = c3fn_fistRN(pun<double>(ld<uint64_t>(ctx, src)));  // 0x7e69 / 0x7e6b
        int16_t w;
        if (v > 0x7fff) w = 0x7fff;  // 0x7e6e / 0x7e75 / 0x7e91
        else if (v < -0x8000) w = static_cast<int16_t>(0x8000);  // 0x7e77 / 0x7e7e / 0x7e97
        else w = static_cast<int16_t>(v);  // 0x7e80 / 0x7e83
        st<int16_t>(ctx, dst, w);
        src += stride;
        dst += 2;
    } while (--n != 0);  // 0x7e8c / 0x7e8d
}
// the mix loop at 0x7ed0-0x7edd (and 0x7f98-0x7fa5): `count` words of the mix buffer are added into the region
void c3fn_mixLoop(Ctx &ctx, u32 dst, u32 mix, uint32_t count) {
    uint32_t n = count;
    do {
        st<uint16_t>(ctx, dst, static_cast<uint16_t>(ld<uint16_t>(ctx, dst) + ld<uint16_t>(ctx, mix)));  // 0x7ed0-0x7ed6
        mix += 2;
        dst += 2;
    } while (--n != 0);  // 0x7edc / 0x7edd
}
// Frame (entry esp E; `push ebp; mov ebp, esp; sub esp, 0x1c`, ebp = E-4, the arguments at [ebp + 8 ..] = E+4 ..): restore's out slots are
// p1 = the slot of the fourth argument at E+0x10 and p2 = the slot of the third at E+0xc (they hold the region pointers it returns:
// 0x7e0a / 0x7e02), b1 at ebp-8 = E-0xc and b2 at ebp-4 = E-8 (0x7e06 / 0x7dfe).
uint32_t writeWaveChunk_body(Ctx &ctx, u32 self, u32 a0, u32 a1, u32 a2, u32 a3) {
    SndFrame F(ctx, 0x2c);
    const u32 p1A = F.entry() + 0x10, p2A = F.entry() + 0xc, b1A = F.at(0xc), b2A = F.at(8);
    st<u32>(ctx, p1A, a3);  // the argument slots the callee overwrites
    st<u32>(ctx, p2A, a2);
    uint32_t mixed1 = 0;  // [ebp - 0x10], 0x7e1b
    if (snd_call(ctx, 0x0000213f, SndAbi::Thiscall, {a0, a3, a2, p1A, b1A, p2A, b2A}).eax) return 0x1a;  // 0x7e1e / 0x7e27
    const u32 p1 = ld<u32>(ctx, p1A);  // edx, 0x7e11 / 0x7e50
    uint32_t samples1 = 0;  // ebx
    const uint16_t ba = ld<uint16_t>(ctx, a0 + 0xb4);  // 0x7e35 / 0x7e3c
    if (ba != 0) samples1 = udiv32(ld<uint32_t>(ctx, b1A), ba);  // 0x7e3f-0x7e4e
    samples1 <<= 1;  // 0x7e53
    c3fn_clampLoop(ctx, p1, a1, samples1);  // 0x7e5b-0x7e9d
    const u32 out = ld<u32>(ctx, self + 0x30);  // 0x7ea3
    if (out) snd_import(ctx, kIatMmioWrite, SndAbi::Stdcall, {out, p1, samples1 + samples1}).eax;  // 0x7eaa-0x7eb0
    const u32 mix = ld<u32>(ctx, self + 0x38);  // 0x7eb6
    if (mix) {  // 0x7eb9
        mixed1 = 0;  // 0x7ebf
        if (static_cast<int32_t>(samples1) > 0) {  // 0x7ebd / 0x7ec6 (jle)
            mixed1 = samples1;  // 0x7ecd
            c3fn_mixLoop(ctx, p1, mix, samples1);  // 0x7ed0-0x7edd
        }
    }
    const u32 p2 = ld<u32>(ctx, p2A);  // esi, 0x7edf
    if (p2) {  // 0x7ee2 / 0x7ee4
        uint32_t samples2 = 0;
        const uint16_t ba2 = ld<uint16_t>(ctx, a0 + 0xb4);  // 0x7eed
        if (ba2 != 0) samples2 = udiv32(ld<uint32_t>(ctx, b2A), ba2);  // 0x7ef4-0x7f06
        samples2 <<= 1;  // 0x7f17
        const u32 src2 = a1 + samples1 * 8;  // 0x7f19
        c3fn_clampLoop(ctx, p2, src2, samples2);  // 0x7f27-0x7f69
        if (ld<u32>(ctx, self + 0x30)) snd_import(ctx, kIatMmioWrite, SndAbi::Stdcall, {ld<u32>(ctx, self + 0x30), p2, samples2 + samples2}).eax;  // 0x7f6f-0x7f7b
        const u32 mix2 = ld<u32>(ctx, self + 0x38);  // 0x7f84
        if (mix2 && static_cast<int32_t>(samples2) > 0)  // 0x7f89 / 0x7f8e / 0x7f93 (jle)
            c3fn_mixLoop(ctx, p2, mix2 + mixed1 * 2, samples2);  // 0x7f90-0x7fa5
    }
    snd_call(ctx, 0x00001ec9, SndAbi::Thiscall, {a0, ld<u32>(ctx, p1A), ld<uint32_t>(ctx, b1A), p2, ld<uint32_t>(ctx, b2A)});  // 0x7fa7-0x7fbd
    if (ld<u32>(ctx, self + 0x38)) {  // 0x7fc2
        snd_delete(ctx, ld<u32>(ctx, self + 0x38));  // 0x7fca
        st<u32>(ctx, self + 0x38, 0);  // 0x7fd2
    }
    return 0;  // 0x7fdb
}
void a_writeWaveChunk(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(writeWaveChunk), writeWaveChunk_body(ctx, a[0], a[1], a[2], a[3], a[4]))); }

// ---- writeWaveChunkB (rva 0x8070): hand-written from the instructions: the stereo twin of writeWaveChunk
// `ret 0x18`: this = ecx (+0x30 the output mmio handle), six stack arguments: a0 the left capture DsBuffer, a1 the right one, `src` the
// interleaved doubles (the left sample is src[2k], the right src[2k + 1]: stride 0x10, the right channel starts at src + 8), and three
// dwords A4 (restore's second argument for both buffers), A5 (the first argument for a0) and A6 (the first argument for a1).
// DsBuffer::restore (thunk 0x213f) on a0 with (A5, A4, &ptr1, &bytes1, &ptr2, &bytes2) where ptr2 / bytes2 are the slots of A5 / A4
// (0x8088-0x8099); non-zero returns 0x1a. On a1 with (A6, A4, &ptr1', &bytes1', &ptr2', &bytes2') (0x80b0-0x80c8); non-zero unlocks a0's
// regions and returns 0x1a (0x80d1-0x80e3). Then the four regions are clamped and written in the order a0 region 1, a1 region 1,
// a0 region 2 (source continuing after count1 pairs), a1 region 2 (0x80f6-0x8343); each loop ends with `dec ecx; jne` (a count of 0 runs
// 2^32 times, as in writeWaveChunk: U-1481); the unlocks of a0 then a1 (0x8343-0x836e); +0x38 freed if set (0x8376-0x8386); returns 0.
// Frame (entry esp E; ebp = E-4): bytes1 E-8 (ebp-4), ptr1 E-0xc (ebp-8), bytes2' E-0x10, ptr2' E-0x14, bytes1' E-0x18, ptr1' E-0x1c, and the
// argument slots ptr2 = E+0x14 (ebp+0x18, A5's) and bytes2 = E+0x10 (ebp+0x14, A4's).
uint32_t writeWaveChunkB_body(Ctx &ctx, u32 self, u32 a0, u32 a1, u32 src, uint32_t A4, uint32_t A5, uint32_t A6) {
    SndFrame F(ctx, 0x30);
    const u32 b1A = F.at(8), p1A = F.at(0xc), b2pA = F.at(0x10), p2pA = F.at(0x14), b1pA = F.at(0x18), p1pA = F.at(0x1c);
    const u32 p2A = F.entry() + 0x14, b2A = F.entry() + 0x10;
    st<uint32_t>(ctx, p2A, A5);  // the argument slots the first restore overwrites
    st<uint32_t>(ctx, b2A, A4);
    if (snd_call(ctx, 0x0000213f, SndAbi::Thiscall, {a0, A5, A4, p1A, b1A, p2A, b2A}).eax) return 0x1a;  // 0x8099 / 0x80a0
    if (snd_call(ctx, 0x0000213f, SndAbi::Thiscall, {a1, A6, A4, p1pA, b1pA, p2pA, b2pA}).eax) {  // 0x80c8 / 0x80cf
        snd_call(ctx, 0x00001ec9, SndAbi::Thiscall, {a0, ld<uint32_t>(ctx, p1A), ld<uint32_t>(ctx, b1A), ld<uint32_t>(ctx, p2A), ld<uint32_t>(ctx, b2A)});  // 0x80e3
        return 0x1a;  // 0x80e8
    }
    uint32_t cnt1 = 0;  // ebx
    const uint16_t ba0 = ld<uint16_t>(ctx, a0 + 0xb4);  // 0x80f6 / 0x80fd
    if (ba0 != 0) cnt1 = udiv32(ld<uint32_t>(ctx, b1A), ba0);  // 0x8102-0x810f
    const u32 dst1 = ld<u32>(ctx, p1A);  // edx, 0x8115
    c3fn_clampLoop(ctx, dst1, src, cnt1, 0x10);  // 0x8118-0x8162
    if (ld<u32>(ctx, self + 0x30)) snd_import(ctx, kIatMmioWrite, SndAbi::Stdcall, {ld<u32>(ctx, self + 0x30), dst1, cnt1 + cnt1}).eax;  // 0x8163-0x8173
    uint32_t cnt1p = 0;  // [ebp + 0x1c]
    const uint16_t ba1 = ld<uint16_t>(ctx, a1 + 0xb4);  // 0x8179 / 0x817c
    if (ba1 != 0) cnt1p = udiv32(ld<uint32_t>(ctx, b1pA), ba1);  // 0x8188-0x8195
    const u32 dst1p = ld<u32>(ctx, p1pA);  // edx, 0x81a1
    c3fn_clampLoop(ctx, dst1p, src + 8, cnt1p, 0x10);  // 0x81a4-0x81ee
    if (ld<u32>(ctx, self + 0x30)) snd_import(ctx, kIatMmioWrite, SndAbi::Stdcall, {ld<u32>(ctx, self + 0x30), dst1p, cnt1p + cnt1p}).eax;  // 0x81ef-0x8201
    const u32 ptr2 = ld<u32>(ctx, p2A);  // esi, 0x8207
    if (ptr2) {  // 0x820a / 0x820c
        uint32_t cnt2 = 0;
        const uint16_t ba = ld<uint16_t>(ctx, a0 + 0xb4);  // 0x8212 / 0x8215
        if (ba != 0) cnt2 = udiv32(ld<uint32_t>(ctx, b2A), ba);  // 0x821f-0x822e
        c3fn_clampLoop(ctx, ptr2, src + (cnt1 << 4), cnt2, 0x10);  // 0x8234-0x828a
        if (ld<u32>(ctx, self + 0x30)) snd_import(ctx, kIatMmioWrite, SndAbi::Stdcall, {ld<u32>(ctx, self + 0x30), ptr2, cnt2 + cnt2}).eax;  // 0x828c-0x829c
    }
    const u32 ptr2p = ld<u32>(ctx, p2pA);  // esi, 0x82a2
    if (ptr2p) {  // 0x82a5 / 0x82a7
        uint32_t cnt2p = 0;
        const uint16_t ba = ld<uint16_t>(ctx, a1 + 0xb4);  // 0x82ad / 0x82b0
        if (ba != 0) cnt2p = udiv32(ld<uint32_t>(ctx, b2pA), ba);  // 0x82ba-0x82c9
        c3fn_clampLoop(ctx, ptr2p, src + (cnt1p << 4) + 8, cnt2p, 0x10);  // 0x82cf-0x832d
        if (ld<u32>(ctx, self + 0x30)) snd_import(ctx, kIatMmioWrite, SndAbi::Stdcall, {ld<u32>(ctx, self + 0x30), ptr2p, cnt2p + cnt2p}).eax;  // 0x832d-0x833d
    }
    snd_call(ctx, 0x00001ec9, SndAbi::Thiscall, {a0, ld<uint32_t>(ctx, p1A), ld<uint32_t>(ctx, b1A), ld<uint32_t>(ctx, p2A), ld<uint32_t>(ctx, b2A)});  // 0x8343-0x8356
    snd_call(ctx, 0x00001ec9, SndAbi::Thiscall, {a1, ld<uint32_t>(ctx, p1pA), ld<uint32_t>(ctx, b1pA), ld<uint32_t>(ctx, p2pA), ld<uint32_t>(ctx, b2pA)});  // 0x835b-0x836e
    if (ld<u32>(ctx, self + 0x38)) {  // 0x8376 / 0x8379
        snd_delete(ctx, ld<u32>(ctx, self + 0x38));  // 0x837e
        st<u32>(ctx, self + 0x38, 0);  // 0x8386
    }
    return 0;  // 0x838f
}
void a_writeWaveChunkB(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(writeWaveChunkB), writeWaveChunkB_body(ctx, a[0], a[1], a[2], a[3], a[4], a[5], a[6]))); }

// ---- openMmioFile (rva 0x86f0): hand-fixed conversion of c3dh.cpp OpenMmioFile_re
// The MMIOINFO is the function's frame: info at entry esp - 0x48 (0x86fe `lea edi, [esp + 8]` after two pushes, 0x8708).
uint32_t openMmioFile_body(Ctx &ctx, u32 self, u32 name) {
    SndFrame F(ctx, 0x48);
    const u32 info = F.at(0x48);
    for (int i = 0; i < 0x12; i++) st<uint32_t>(ctx, info + 4 * i, 0);  // 0x86fc-0x8702
    st<uint32_t>(ctx, info + 4, 0x204d454d);  // 0x871b
    st<uint32_t>(ctx, info + 0x14, 0x1000);  // 0x8713
    unsigned h = snd_import(ctx, kIatMmioOpenA, SndAbi::Stdcall, {name, info, 0x11001}).eax;  // 0x8723
    st<uint32_t>(ctx, self + 0x30, h);  // 0x8729
    if (h == 0) return 8;  // 0x872d / 0x8730 / 0x8732
    st<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_openMmioCount).va(), ld<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_openMmioCount).va()) + 1);  // 0x873d-0x8743
    return 0;  // 0x8748
}
void a_openMmioFile(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(openMmioFile), openMmioFile_body(ctx, a[0], a[1]))); }

// ---- closeMmioHandle (rva 0x8770): converted from c3dt.cpp CloseMmio_re
uint32_t closeMmioHandle_body(Ctx &ctx, u32 self) {
    unsigned h = ld<uint32_t>(ctx, self + 0x30);  // 0x8770
    if (h) snd_import(ctx, kIatMmioClose, SndAbi::Stdcall, {h, NEG(closeMmioHandle) == 2 ? 1u : 0u}).eax;  // 0x8773 / 0x877a (mode 2: flags 1)
    const u32 cnt = snd_obj(ctx, &SoundState::g_openMmioCount).va();
    st<uint32_t>(ctx, cnt, ld<uint32_t>(ctx, cnt) - 1);  // 0x8780-0x878b
    return 0;  // 0x878d
}
void a_closeMmioHandle(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(closeMmioHandle), closeMmioHandle_body(ctx, a[0]))); }

// ---- Dll_Wave_Device::create_device (rva 0xb050): converted from c3fd.cpp WaveCreate_re
constexpr uint32_t kWaveDevice = 0xb4a20;  // c3fd.cpp
u32 c3fd_gref(Ctx &ctx, uint32_t rva) { return ld<u32>(ctx, snd_va(ctx, rva)); }
uint32_t Dll_Wave_Device__create_device_body(Ctx &ctx, u32 out, uint32_t) {
    if (c3fd_gref(ctx, kWaveDevice)) return 0xc;  // 0xb06b / 0xb06d / 0xb0cf
    u32 p = snd_new(ctx, 0x41e0);  // 0xb074
    if (!p) {  // 0xb080 / 0xb08a
        st<u32>(ctx, snd_va(ctx, kWaveDevice), 0);  // 0xb0b5
        st<u32>(ctx, out, 0);  // 0xb0ba
        return 0;  // 0xb0b3
    }
    u32 d = (snd_call(ctx, 0x00001f82, SndAbi::Thiscall, {p}).eax);  // 0xb08e
    st<u32>(ctx, snd_va(ctx, kWaveDevice), d);  // 0xb097
    st<u32>(ctx, out, d);  // 0xb09c
    return 0;  // 0xb09e
}
void a_Dll_Wave_Device__create_device(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Dll_Wave_Device__create_device), Dll_Wave_Device__create_device_body(ctx, a[0], a[1]))); }

// ---- openWaveStreamFile (rva 0xb110): converted from c3dt.cpp OpenStream_re
uint32_t openWaveStreamFile_body(Ctx &ctx, uint32_t name) {
    u32 dev = ld<u32>(ctx, snd_obj(ctx, &SoundState::g_waveOwner).va());  // 0xb110
    if (!dev) return 3;  // 0xb116 / 0xb118 / 0xb12c
    return snd_call(ctx, 0x00002211, SndAbi::Thiscall, {dev, name}).eax;  // 0xb11f
}
void a_openWaveStreamFile(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(openWaveStreamFile), openWaveStreamFile_body(ctx, a[0]))); }

// ---- Wave_Device::setCooperativeLevel (rva 0xb140): converted from c3dt.cpp WdCoop_re
u32 c3dt_glob(Ctx &ctx, uint32_t rva) { return ld<u32>(ctx, snd_va(ctx, rva)); }
uint32_t Wave_Device__setCooperativeLevel_body(Ctx &ctx, uint32_t arg) {
    u32 dev = c3dt_glob(ctx, 0x000b49f0);  // 0xb140
    if (!dev) return 0x13;  // 0xb146 / 0xb148
    return snd_call(ctx, 0x000010a0, SndAbi::Thiscall, {dev, arg}).eax;  // 0xb157
}
void a_Wave_Device__setCooperativeLevel(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Wave_Device__setCooperativeLevel), Wave_Device__setCooperativeLevel_body(ctx, a[0]))); }

// ---- Wave_Device::closeStream (rva 0xb170): converted from c3dt.cpp CloseStream_re
uint32_t Wave_Device__closeStream_body(Ctx &ctx) {
    u32 dev = ld<u32>(ctx, snd_obj(ctx, &SoundState::g_waveOwner).va());  // 0xb170
    if (!dev) return 3;  // 0xb176 / 0xb178 / 0xb17f
    return snd_call(ctx, 0x000024be, SndAbi::Thiscall, {dev}).eax;  // 0xb17a
}
void a_Wave_Device__closeStream(Ctx &ctx, const uint32_t *, Ret &r) { r.set_u32(neg_u32(NEG(Wave_Device__closeStream), Wave_Device__closeStream_body(ctx))); }

// ---- Dll_Wave_Device::delete_device (rva 0xb1a0): converted from c3fd.cpp DelWave_re
uint32_t Dll_Wave_Device__delete_device_body(Ctx &ctx) {
    u32 dev = c3fd_gref(ctx, kWaveDevice);  // 0xb1a0
    if (dev) snd_vcall(ctx, dev, 0, SndAbi::Thiscall, {1}).eax;  // 0xb1a8-0xb1ae
    st<u32>(ctx, snd_va(ctx, kWaveDevice), 0);  // 0xb1b0
    return 0;  // 0xb1ba
}
void a_Dll_Wave_Device__delete_device(Ctx &ctx, const uint32_t *, Ret &r) { r.set_u32(neg_u32(NEG(Dll_Wave_Device__delete_device), Dll_Wave_Device__delete_device_body(ctx))); }

// ---- Wave_Device::ctor (rva 0xb1d0): converted from c3ew.cpp WdCtor_re
u32 Wave_Device__ctor_body(Ctx &ctx, u32 self) {
    u32 s = self;
    snd_call(ctx, 0x000021df, SndAbi::Thiscall, {s}).eax;  // 0xb1ee
    st<uint32_t>(ctx, s + 0x1c8, 0);  // 0xb1f9
    st<uint32_t>(ctx, s + 0x1cc, 0);  // 0xb1ff
    st<uint32_t>(ctx, s + 0x1d0, 0);  // 0xb205
    st<uint32_t>(ctx, s + 0x1d4, 0);  // 0xb20b
    snd_call(ctx, 0x000010aa, SndAbi::Thiscall, {s + 0x1d8}).eax;  // 0xb211 / 0xb21c
    st<u32>(ctx, s, snd_va(ctx, 0x5b304));  // 0xb225
    st<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_waveDevice).va(), 0);  // 0xb231
    st<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_primaryBuffer).va(), 0);  // 0xb237
    st<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_waveOwner).va(), 0);  // 0xb23d
    st<uint32_t>(ctx, s + 0x1a8, 0);  // 0xb243
    st<uint32_t>(ctx, s + 0x1ac, 0);  // 0xb249
    st<uint32_t>(ctx, s + 0x1b0, 0);  // 0xb24f
    st<uint32_t>(ctx, s + 0x1b8, 0x7f);  // 0xb255
    st<uint32_t>(ctx, s + 0x1a4, 0);  // 0xb25f
    st<uint32_t>(ctx, s + 0x1b4, 0);  // 0xb261
    st<uint32_t>(ctx, s + 0x1c4, 0);  // 0xb267
    return self;  // 0xb26d
}
void a_Wave_Device__ctor(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Wave_Device__ctor), Wave_Device__ctor_body(ctx, a[0]))); }

// ---- Wave_Device::scalarDeletingDtor (rva 0xb2b0): converted from c3dt.cpp WaveDevDel_re
uint32_t Wave_Device__scalarDeletingDtor_body(Ctx &ctx, u32 self, uint32_t flags) {
    snd_call(ctx, 0x000017da, SndAbi::Thiscall, {self}).eax;
if (flags & 1) snd_delete(ctx, self);
return self;
}
void a_Wave_Device__scalarDeletingDtor(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Wave_Device__scalarDeletingDtor), Wave_Device__scalarDeletingDtor_body(ctx, a[0], a[1]))); }

// ---- Wave_Device::dtor (rva 0xb2e0): converted from c3ew.cpp WdDtor_re
// The node list {+0 front, +4 back, +0xc count} of a header at `list` with nodes {+0 previous, +4 next, +8 value}: one pop from
// the front as the bodies below inline it (`mov ecx, [eax+4]; test; je; mov [ecx], 0; mov [front], ecx; jmp` / `mov [back], 0;
// mov [front], 0`, then the value read, the node deleted and the count decremented). Returns the value; *ret is operator
// delete's eax.
u32 c3ew_popFront(Ctx &ctx, u32 list, int32_t frontOff, int32_t backOff, int32_t countOff, u32 node, unsigned &ret) {
    u32 next = ld<u32>(ctx, node + 4);
    if (next) {
        st<u32>(ctx, next, 0);
        st<u32>(ctx, list + frontOff, next);
    } else {
        st<u32>(ctx, list + backOff, 0);
        st<u32>(ctx, list + frontOff, 0);
    }
    u32 v = ld<u32>(ctx, node + 8);
    ret = snd_delete(ctx, node).eax;
    st<int32_t>(ctx, list + countOff, static_cast<int32_t>(ld<int32_t>(ctx, list + countOff) - 1));
    return v;
}
uint32_t Wave_Device__dtor_body(Ctx &ctx, u32 self) {
    u32 s = self;
    st<u32>(ctx, s, snd_va(ctx, 0x5b304));  // 0xb2ff
    if (ld<u32>(ctx, snd_obj(ctx, &SoundState::g_waveDevice).va())) snd_call(ctx, 0x00001a9b, SndAbi::Thiscall, {s}).eax;  // 0xb305-0xb318
    snd_call(ctx, 0x000019a1, SndAbi::Thiscall, {s + 0x1d8}).eax;  // 0xb31d / 0xb328
    unsigned dr;
    u32 node = ld<u32>(ctx, s + 0x1c8);  // 0xb32d
    while (node) {  // 0xb337 / 0xb37d
        u32 v = c3ew_popFront(ctx, s, 0x1c8, 0x1cc, 0x1d4, node, dr);  // 0xb33b-0xb36d / 0xb37f-0xb3b1
        if (!v) break;  // 0xb373 / 0xb3b7
        node = ld<u32>(ctx, s + 0x1c8);  // 0xb375
    }
    return snd_call(ctx, 0x00001dac, SndAbi::Thiscall, {s}).eax;  // 0xb3b9-0xb3c3
}
void a_Wave_Device__dtor(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Wave_Device__dtor), Wave_Device__dtor_body(ctx, a[0]))); }

// ---- Wave_Device::open (rva 0xb420): hand-written from the instructions
// `ret 8`, this = ecx, two stack arguments: `win` (the window the sound window object subclasses, E+4) and `flags` (E+8; bit 7 asks for
// the window object, the dword is kept at +0x1c4). The body sits in an SEH frame (0xb420-0xb42e: the EH state slot at E-0x14 takes the
// values 0 / 1 / -1 while an operator new result is being constructed): it only matters when a constructor throws, which the port
// does not model. Module globals (RVAs): g_dsDevice 0xb49f0, g_streamDevice 0xb49f4, the primary buffer 0xb49f8, the two secondary
// buffers 0xb49fc / 0xb4a00, g_soundTimer 0xb49ec, the flag byte 0xb49bc, the sound window object 0xb49b8 and the thread state 0xb5038.
// Steps: (1) without bit 2 of the byte 0xb49bc and with bit 7 of `flags`: SoundWindowObj_subclass (thunk 0x1177, ecx 0xb49b8) with `win`,
// and +0x1a4 |= 0x20 (0xb447-0xb463); +0x1c4 = flags (0xb469). (2) no device (0xb46f): operator new 0x1b8, initSoundVoice (thunk 0x21a3),
// stored in g_dsDevice, DirectSoundDevice::enumerate (thunk 0x2013) with (flags, 0xf) then DirectSoundDevice::create (0x180c) with (win,
// 2); a non-zero result of either closes and deletes the device, clears g_dsDevice and returns 3 (0xb4d2-0xb50c). (3) no stream device
// (0xb515): operator new 0x3c, initStreamBuffer (0x170d), +0x14 = [0xb49c0], and with bit 2 of +0x1a4 bit 1 of its +8 (0xb51c-0xb568).
// (4) no primary buffer and the device byte +0x6c without bit 4 (0xb572-0xb584): createBuffer (0x1046, &0xb49f8, 0x10088, 0x10000),
// failure releaseAll (0x104b), clears the slot and returns 0x1e; then the window's virtual slots 0x3c (0x1e) and 0x40 (0xe),
// DsBuffer::setRegionMs (0x182f), lockEntire (0x1c5d), play (0x1203, 1) and resizeDynArray (0x1311) on the stream device with twice
// bytes / block align (0xb5a6-0xb608). (5) device byte bit 3: Ds3DListener::setPosition (0x164a) with (0, 0, -0.1f) (0xb613-0xb622).
// (6) device byte bit 4 (0xb62d): two format buffers 0xb49fc / 0xb4a00 through createBufferFmt (0x11e5, the DLL's format 0x63dc8, 0x10094,
// 0x10000), their pan set through COM slot 0x4c, lockEntire and play on both, resizeDynArray with bytes / block align, +0x1a4 |= 8
// (0xb636-0xb786); failures return 0x1e after releaseAll (and removeBuffer of the first for the second's failure). (7) without bit 5 of
// +0x1a4: startSignalThread (0x16db, ecx 0xb5038) (0xb78c-0xb79a). (8) with a sound timer: +0x1a8 = it, SoundTimer::registerClient
// (0x227a) with this and SoundTimer::start (0x1dfc) (0xb79f-0xb7c0). Returns 0.
uint32_t Wave_Device__open_body(Ctx &ctx, u32 self, u32 win, u32 flags) {
    const u32 gDev = snd_va(ctx, 0xb49f0), gStream = snd_va(ctx, 0xb49f4), gPrim = snd_va(ctx, 0xb49f8), gSec1 = snd_va(ctx, 0xb49fc),
              gSec2 = snd_va(ctx, 0xb4a00);
    const unsigned char al = ld<uint8_t>(ctx, snd_va(ctx, 0xb49bc));  // 0xb42e
    if (!(al & 4) && (flags & 0x80)) {  // 0xb447 / 0xb44b
        snd_call(ctx, 0x00001177, SndAbi::Thiscall, {snd_va(ctx, 0xb49b8), win});  // 0xb456
        st<uint32_t>(ctx, self + 0x1a4, ld<uint32_t>(ctx, self + 0x1a4) | 0x20);  // 0xb45b-0xb463
    }
    st<uint32_t>(ctx, self + 0x1c4, flags);  // 0xb469
    u32 ecx = ld<u32>(ctx, gDev);  // 0xb46f
    if (ecx == 0) {  // 0xb475 / 0xb477
        u32 p = snd_new(ctx, 0x1b8);  // 0xb47d / 0xb482
        u32 obj = p ? snd_call(ctx, 0x000021a3, SndAbi::Thiscall, {p}).eax : 0;  // 0xb498-0xb4a3
        st<u32>(ctx, gDev, obj);  // 0xb4b2
        unsigned r = snd_call(ctx, 0x00002013, SndAbi::Thiscall, {obj, flags, 0xf}).eax;  // 0xb4b7
        if (r == 0) r = snd_call(ctx, 0x0000180c, SndAbi::Thiscall, {ld<u32>(ctx, gDev), win, 2}).eax;  // 0xb4c0-0xb4c9
        if (r != 0) {  // 0xb4be / 0xb4ce / 0xb4d0
            const u32 d = ld<u32>(ctx, gDev);  // 0xb4d2
            if (d) {  // 0xb4d8 / 0xb4da
                snd_call(ctx, 0x00001b81, SndAbi::Thiscall, {d});  // 0xb4de
                snd_delete(ctx, d);  // 0xb4e4
            }
            st<u32>(ctx, gDev, 0);  // 0xb4ec
            return 3;  // 0xb4f6
        }
        ecx = ld<u32>(ctx, gDev);  // 0xb50f
    }
    if (ld<u32>(ctx, gStream) == 0) {  // 0xb515 / 0xb51a / 0xb51c
        u32 p = snd_new(ctx, 0x3c);  // 0xb51e / 0xb520
        u32 obj = p ? snd_call(ctx, 0x0000170d, SndAbi::Thiscall, {p}).eax : 0;  // 0xb536-0xb541
        const u32 c0 = ld<u32>(ctx, snd_va(ctx, 0xb49c0));  // 0xb543
        st<u32>(ctx, gStream, obj);  // 0xb549
        st<uint32_t>(ctx, obj + 0x14, c0);  // 0xb54e
        if (ld<uint8_t>(ctx, self + 0x1a4) & 4) {  // 0xb551 / 0xb557
            const u32 s = ld<u32>(ctx, gStream);  // 0xb563
            st<uint32_t>(ctx, s + 8, ld<uint32_t>(ctx, s + 8) | 2);  // 0xb568
        }
        ecx = ld<u32>(ctx, gDev);  // 0xb56c
    }
    if (ld<u32>(ctx, gPrim) == 0 && !(ld<uint8_t>(ctx, ecx + 0x6c) & 0x10)) {  // 0xb572-0xb584 (bl = 0x10 from 0xb577)
        if (snd_call(ctx, 0x00001046, SndAbi::Thiscall, {ecx, gPrim, 0x10088, 0x10000}).eax != 0) {  // 0xb58a-0xb59e
            snd_call(ctx, 0x0000104b, SndAbi::Thiscall, {ld<u32>(ctx, gDev)});  // 0xb742 / 0xb748
            st<u32>(ctx, gPrim, 0);  // 0xb74d
            return 0x1e;  // 0xb757
        }
        snd_vcall(ctx, self, 0x3c, SndAbi::Thiscall, {0x1e});  // 0xb5a6-0xb5ac
        const unsigned ms = [&] { SndResidue keep(ctx); return snd_vcall(ctx, self, 0x40, SndAbi::Thiscall, {0xe}).eax; }();  // 0xb5af-0xb5b5: `push 0xe` stays (slot 0x40 is `ret`), setRegionMs's second argument
        snd_call(ctx, 0x0000182f, SndAbi::Thiscall, {ld<u32>(ctx, gPrim), ms, 0xe});  // 0xb5b8-0xb5bf
        snd_call(ctx, 0x00001c5d, SndAbi::Thiscall, {ld<u32>(ctx, gPrim)});  // 0xb5c4 / 0xb5ca
        snd_call(ctx, 0x00001203, SndAbi::Thiscall, {ld<u32>(ctx, gPrim), 1});  // 0xb5cf-0xb5d7
        const u32 pb = ld<u32>(ctx, gPrim);  // 0xb5dc
        const uint16_t bal = ld<uint16_t>(ctx, pb + 0xb4);  // 0xb5e1
        const unsigned q = bal ? udiv32(ld<uint32_t>(ctx, pb + 0x38), bal) : 0;  // 0xb5e8-0xb5fe
        snd_call(ctx, 0x00001311, SndAbi::Thiscall, {ld<u32>(ctx, gStream), q + q});  // 0xb601-0xb608
        ecx = ld<u32>(ctx, gDev);  // 0xb60d
    }
    if (ld<uint8_t>(ctx, ecx + 0x6c) & 8) {  // 0xb613 / 0xb617
        snd_call(ctx, 0x0000164a, SndAbi::Thiscall, {ecx, 0, 0, 0xbdcccccd});  // 0xb619-0xb622
        ecx = ld<u32>(ctx, gDev);  // 0xb627
    }
    if (ld<uint8_t>(ctx, ecx + 0x6c) & 0x10) {  // 0xb62d / 0xb630
        if (snd_call(ctx, 0x000011e5, SndAbi::Thiscall, {ecx, gSec1, snd_va(ctx, 0x63dc8), 0x10094, 0x10000}).eax != 0) {  // 0xb636-0xb651
            snd_call(ctx, 0x0000104b, SndAbi::Thiscall, {ld<u32>(ctx, gDev)});  // 0xb81d / 0xb823
            st<u32>(ctx, gSec1, 0);  // 0xb82e
            return 0x1e;  // 0xb838
        }
        if (snd_call(ctx, 0x000011e5, SndAbi::Thiscall, {ld<u32>(ctx, gDev), gSec2, snd_va(ctx, 0x63dc8), 0x10094, 0x10000}).eax != 0) {  // 0xb657-0xb678
            snd_call(ctx, 0x00001677, SndAbi::Thiscall, {ld<u32>(ctx, gDev), ld<u32>(ctx, gSec1)});  // 0xb7db-0xb7e8
            snd_call(ctx, 0x0000104b, SndAbi::Thiscall, {ld<u32>(ctx, gDev)});  // 0xb7ed / 0xb7f3
            st<u32>(ctx, gSec2, 0);  // 0xb7fa
            st<u32>(ctx, gSec1, 0);  // 0xb7ff
            return 0x1e;  // 0xb804
        }
        snd_vcall(ctx, self, 0x3c, SndAbi::Thiscall, {0xf});  // 0xb67e-0xb684
        unsigned ms = [&] { SndResidue keep(ctx); return snd_vcall(ctx, self, 0x40, SndAbi::Thiscall, {0xe}).eax; }();  // 0xb687-0xb68d: `push 0xe` stays (slot 0x40 is `ret`), setRegionMs's second argument
        snd_call(ctx, 0x0000182f, SndAbi::Thiscall, {ld<u32>(ctx, gSec2), ms, 0xe});  // 0xb690-0xb697
        ms = [&] { SndResidue keep(ctx); return snd_vcall(ctx, self, 0x40, SndAbi::Thiscall, {0xe}).eax; }();  // 0xb69c-0xb6a2: `push 0xe` stays (slot 0x40 is `ret`), setRegionMs's second argument
        snd_call(ctx, 0x0000182f, SndAbi::Thiscall, {ld<u32>(ctx, gSec1), ms, 0xe});  // 0xb6a5-0xb6ac
        u32 com = ld<u32>(ctx, ld<u32>(ctx, gSec1) + 0x64);  // 0xb6b1 / 0xb6b6
        if (com) snd_vcall(ctx, com, 0x4c, SndAbi::Stdcall, {0xbf000000, 0, 0xbf000000, 0});  // 0xb6bd-0xb6ce
        com = ld<u32>(ctx, ld<u32>(ctx, gSec2) + 0x64);  // 0xb6d1 / 0xb6d7
        if (com) snd_vcall(ctx, com, 0x4c, SndAbi::Stdcall, {0x3f000000, 0, 0xbf000000, 0});  // 0xb6de-0xb6ef
        snd_call(ctx, 0x00001c5d, SndAbi::Thiscall, {ld<u32>(ctx, gSec1)});  // 0xb6f2 / 0xb6f8
        snd_call(ctx, 0x00001c5d, SndAbi::Thiscall, {ld<u32>(ctx, gSec2)});  // 0xb6fd / 0xb703
        snd_call(ctx, 0x00001203, SndAbi::Thiscall, {ld<u32>(ctx, gSec1), 1});  // 0xb708-0xb710
        snd_call(ctx, 0x00001203, SndAbi::Thiscall, {ld<u32>(ctx, gSec2), 1});  // 0xb715-0xb71d
        const u32 sb = ld<u32>(ctx, gSec1);  // 0xb722
        const uint16_t bal = ld<uint16_t>(ctx, sb + 0xb4);  // 0xb727
        const unsigned q = bal ? udiv32(ld<uint32_t>(ctx, sb + 0x38), bal) : 0;  // 0xb72e-0xb742
        snd_call(ctx, 0x00001311, SndAbi::Thiscall, {ld<u32>(ctx, gStream), q});  // 0xb772-0xb779
        st<uint32_t>(ctx, self + 0x1a4, ld<uint32_t>(ctx, self + 0x1a4) | 8);  // 0xb77e-0xb786
    }
    if (!(ld<uint8_t>(ctx, self + 0x1a4) & 0x20)) snd_call(ctx, 0x000016db, SndAbi::Thiscall, {snd_va(ctx, 0xb5038)});  // 0xb78c-0xb79a
    const u32 timer = ld<u32>(ctx, snd_va(ctx, 0xb49ec));  // 0xb79f
    if (timer) {  // 0xb7a4 / 0xb7a6
        st<u32>(ctx, self + 0x1a8, timer);  // 0xb7a8
        snd_call(ctx, 0x0000227a, SndAbi::Thiscall, {ld<u32>(ctx, snd_va(ctx, 0xb49ec)), self});  // 0xb7ae-0xb7b5
        snd_call(ctx, 0x00001dfc, SndAbi::Thiscall, {ld<u32>(ctx, snd_va(ctx, 0xb49ec))});  // 0xb7ba / 0xb7c0
    }
    return 0;  // 0xb7c5
}
void a_Wave_Device__open(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Wave_Device__open), Wave_Device__open_body(ctx, a[0], a[1], a[2]))); }

// ---- Wave_Device::stop (rva 0xb960): converted from c3ew.cpp WdStop_re
uint32_t c3ew_u(Ctx &, u32 p) { return p; }
uint32_t Wave_Device__stop_body(Ctx &ctx, u32 self) {
    u32 s = self;
    u32 t = ld<u32>(ctx, s + 0x1a8);  // 0xb964
    if (t) {  // 0xb96a / 0xb96c
        snd_call(ctx, 0x000026fd, SndAbi::Thiscall, {t}).eax;  // 0xb96e
        snd_call(ctx, 0x00001ed3, SndAbi::Thiscall, {ld<u32>(ctx, snd_obj(ctx, &SoundState::g_soundTimer).va()), c3ew_u(ctx, s)}).eax;  // 0xb973-0xb97a
    }
    if (ld<uint8_t>(ctx, s + 0x1a4) & 0x20) {  // 0xb97f / 0xb986
        const u32 mb = snd_obj(ctx, &SoundState::g_midiOutputFlags).va();  // 0xb988
        st<uint8_t>(ctx, mb, static_cast<uint8_t>((ld<uint8_t>(ctx, mb) & 0xfe) ^ 1));  // 0xb98d-0xb991
    } else {
        snd_call(ctx, 0x00002757, SndAbi::Thiscall, {snd_arr(ctx, &SoundState::g_soundThreadState).va()}).eax;  // 0xb998 / 0xb99d
    }
    snd_call(ctx, 0x0000188e, SndAbi::Thiscall, {s}).eax;  // 0xb9a2 / 0xb9a4
    u32 d = ld<u32>(ctx, snd_obj(ctx, &SoundState::g_waveDevice).va());  // 0xb9a9
    if (d) {  // 0xb9af / 0xb9b1
        unsigned b = ld<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_primaryBuffer).va());  // 0xb9b3
        if (b) {  // 0xb9b8 / 0xb9ba
            snd_call(ctx, 0x00001677, SndAbi::Thiscall, {d, b}).eax;  // 0xb9bc / 0xb9bd
            d = ld<u32>(ctx, snd_obj(ctx, &SoundState::g_waveDevice).va());  // 0xb9c2
            st<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_primaryBuffer).va(), 0);  // 0xb9c8
        }
        if (ld<uint8_t>(ctx, d + 0x6c) & 8) {  // 0xb9d2 / 0xb9d6
            snd_call(ctx, 0x00001677, SndAbi::Thiscall, {d, ld<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_secondaryBuffer1).va())}).eax;  // 0xb9d8-0xb9df
            unsigned b2 = ld<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_secondaryBuffer2).va());  // 0xb9e4
            snd_call(ctx, 0x00001677, SndAbi::Thiscall, {ld<u32>(ctx, snd_obj(ctx, &SoundState::g_waveDevice).va()), b2}).eax;  // 0xb9e9-0xb9f0
            d = ld<u32>(ctx, snd_obj(ctx, &SoundState::g_waveDevice).va());  // 0xb9f5
        }
        if (d) {  // 0xb9fb-0xb9ff
            snd_call(ctx, 0x00001b81, SndAbi::Thiscall, {d}).eax;  // 0xba01
            snd_delete(ctx, d);  // 0xba06 / 0xba07
        }
        st<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_waveDevice).va(), 0);  // 0xba0f
    }
    u32 wo = ld<u32>(ctx, snd_obj(ctx, &SoundState::g_waveOwner).va());  // 0xba19
    if (wo) {  // 0xba1f / 0xba21
        snd_call(ctx, 0x00001a32, SndAbi::Thiscall, {wo}).eax;  // 0xba23 / 0xba25
        snd_delete(ctx, wo);  // 0xba2a / 0xba2b
        st<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_waveOwner).va(), 0);  // 0xba33
    }
    if (ld<uint8_t>(ctx, s + 0x1a4) & 0x20)  // 0xba3d-0xba47
        snd_call(ctx, 0x00002360, SndAbi::Thiscall, {snd_va(ctx, 0xb49b8)}).eax;  // 0xba49 / 0xba4e
    return 0;  // 0xba53
}
void a_Wave_Device__stop(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Wave_Device__stop), Wave_Device__stop_body(ctx, a[0]))); }

// ---- Wave_Device::start (rva 0xbaa0): hand-written from the instructions
// `ret`, this = ecx (esi), no argument. Without bit 1 of +0x1a4 returns 0 (0xbaa4-0xbab1). With no device (0xb49f0), no stream device
// (0xb49f4) or an existing primary buffer (0xb49f8) returns 6 (0xbab2-0xbad4, 0xbd3d). Otherwise DirectSoundDevice::enumerate (thunk
// 0x2013) with (+0x1c4, 0xf) and create (0x180c) with ([0xb4a3c], 2) (0xbada-0xbaf7): a non-zero create closes and deletes the device,
// clears it and returns 3 (0xbafe-0xbb26). No primary buffer and the device byte +0x6c without bit 4: createBuffer (0x1046), failure
// releaseAll (0x104b) / clear / 0x1e, then the window's slots 0x3c (0xf) and 0x40 (0xe) and DsBuffer::setRegionMs (0xbb28-0xbb77).
// Device bit 3: Ds3DListener::setPosition (0x164a) with the device's +0x1ac / +0x1b0 / +0x1b4 (0xbb7d-0xbb98). Device bit 4: the two
// format buffers (createBufferFmt 0x11e5, 0x63dc8, 0x10094, 0x10000), setRegionMs on both, their pan through COM slot 0x4c,
// resizeDynArray on the stream device with bytes / block align (once), +0x1a4 |= 8 (0xbba3-0xbcb4); failures: releaseAll / clear / 0x1e
// (and removeBuffer for the second). Then slot 0x50 of this, +0x1a4 &= ~2, and with the object at 0xb4a1c its slot 0x60 and, when that
// answers non-zero, slot 0x58 (0xbcba-0xbcee). Returns 0.
uint32_t Wave_Device__start_body(Ctx &ctx, u32 self) {
    const u32 gDev = snd_va(ctx, 0xb49f0), gStream = snd_va(ctx, 0xb49f4), gPrim = snd_va(ctx, 0xb49f8), gSec1 = snd_va(ctx, 0xb49fc),
              gSec2 = snd_va(ctx, 0xb4a00);
    if (!(ld<uint8_t>(ctx, self + 0x1a4) & 2)) return 0;  // 0xbaa4 / 0xbaab
    u32 ecx = ld<u32>(ctx, gDev);  // 0xbab2
    if (ecx == 0 || ld<u32>(ctx, gStream) == 0 || ld<u32>(ctx, gPrim) != 0) return 6;  // 0xbaba-0xbad4 -> 0xbd3d
    snd_call(ctx, 0x00002013, SndAbi::Thiscall, {ecx, ld<uint32_t>(ctx, self + 0x1c4), 0xf});  // 0xbada-0xbae3
    const unsigned c3c = ld<uint32_t>(ctx, snd_va(ctx, 0xb4a3c));  // 0xbae8
    if (snd_call(ctx, 0x0000180c, SndAbi::Thiscall, {ld<u32>(ctx, gDev), c3c, 2}).eax != 0) {  // 0xbaf1-0xbafe
        const u32 d = ld<u32>(ctx, gDev);  // 0xbb00
        if (d) {  // 0xbb06 / 0xbb08
            snd_call(ctx, 0x00001b81, SndAbi::Thiscall, {d});  // 0xbb0c
            snd_delete(ctx, d);  // 0xbb12
        }
        st<u32>(ctx, gDev, 0);  // 0xbb1a
        return 3;  // 0xbb21
    }
    const u32 prim = ld<u32>(ctx, gPrim);  // 0xbb28
    ecx = ld<u32>(ctx, gDev);  // 0xbb2d
    if (prim == 0 && !(ld<uint8_t>(ctx, ecx + 0x6c) & 0x10)) {  // 0xbb33-0xbb3b
        if (snd_call(ctx, 0x00001046, SndAbi::Thiscall, {ecx, gPrim, 0x10088, 0x10000}).eax != 0) {  // 0xbb3d-0xbb53
            snd_call(ctx, 0x0000104b, SndAbi::Thiscall, {ld<u32>(ctx, gDev)});  // 0xbc85 / 0xbc8b
            st<u32>(ctx, gPrim, 0);  // 0xbc90
            return 0x1e;  // 0xbc97
        }
        snd_vcall(ctx, self, 0x3c, SndAbi::Thiscall, {0xf});  // 0xbb59-0xbb5f
        const unsigned ms = [&] { SndResidue keep(ctx); return snd_vcall(ctx, self, 0x40, SndAbi::Thiscall, {0xe}).eax; }();  // 0xbb62-0xbb68: `push 0xe` stays (slot 0x40 is `ret`), setRegionMs's second argument
        snd_call(ctx, 0x0000182f, SndAbi::Thiscall, {ld<u32>(ctx, gPrim), ms, 0xe});  // 0xbb6b-0xbb72
        ecx = ld<u32>(ctx, gDev);  // 0xbb77
    }
    if (ld<uint8_t>(ctx, ecx + 0x6c) & 8) {  // 0xbb7d / 0xbb81
        snd_call(ctx, 0x0000164a, SndAbi::Thiscall, {ecx, ld<uint32_t>(ctx, ecx + 0x1ac), ld<uint32_t>(ctx, ecx + 0x1b0), ld<uint32_t>(ctx, ecx + 0x1b4)});  // 0xbb83-0xbb98
        ecx = ld<u32>(ctx, gDev);  // 0xbb9d
    }
    if (ld<uint8_t>(ctx, ecx + 0x6c) & 0x10) {  // 0xbba3 / 0xbba7
        if (snd_call(ctx, 0x000011e5, SndAbi::Thiscall, {ecx, gSec1, snd_va(ctx, 0x63dc8), 0x10094, 0x10000}).eax != 0) {  // 0xbbad-0xbbc8
            snd_call(ctx, 0x0000104b, SndAbi::Thiscall, {ld<u32>(ctx, gDev)});  // 0xbd24 / 0xbd2a
            st<u32>(ctx, gSec1, 0);  // 0xbd2f
            return 0x1e;  // 0xbd36
        }
        if (snd_call(ctx, 0x000011e5, SndAbi::Thiscall, {ld<u32>(ctx, gDev), gSec2, snd_va(ctx, 0x63dc8), 0x10094, 0x10000}).eax != 0) {  // 0xbbce-0xbbef
            snd_call(ctx, 0x00001677, SndAbi::Thiscall, {ld<u32>(ctx, gDev), ld<u32>(ctx, gSec1)});  // 0xbcf3-0xbd00
            snd_call(ctx, 0x0000104b, SndAbi::Thiscall, {ld<u32>(ctx, gDev)});  // 0xbd05 / 0xbd0b
            st<u32>(ctx, gSec2, 0);  // 0xbd10
            st<u32>(ctx, gSec1, 0);  // 0xbd16
            return 0x1e;  // 0xbd1d
        }
        snd_vcall(ctx, self, 0x3c, SndAbi::Thiscall, {0xf});  // 0xbbf5-0xbbfb
        unsigned ms = [&] { SndResidue keep(ctx); return snd_vcall(ctx, self, 0x40, SndAbi::Thiscall, {0xe}).eax; }();  // 0xbbfe-0xbc04: `push 0xe` stays (slot 0x40 is `ret`), setRegionMs's second argument
        snd_call(ctx, 0x0000182f, SndAbi::Thiscall, {ld<u32>(ctx, gSec2), ms, 0xe});  // 0xbc07-0xbc0e
        ms = [&] { SndResidue keep(ctx); return snd_vcall(ctx, self, 0x40, SndAbi::Thiscall, {0xe}).eax; }();  // 0xbc13-0xbc19: `push 0xe` stays (slot 0x40 is `ret`), setRegionMs's second argument
        snd_call(ctx, 0x0000182f, SndAbi::Thiscall, {ld<u32>(ctx, gSec1), ms, 0xe});  // 0xbc1c-0xbc23
        u32 com = ld<u32>(ctx, ld<u32>(ctx, gSec1) + 0x64);  // 0xbc28 / 0xbc2e
        if (com) snd_vcall(ctx, com, 0x4c, SndAbi::Stdcall, {0xbf000000, 0, 0xbf000000, 0});  // 0xbc35-0xbc44
        com = ld<u32>(ctx, ld<u32>(ctx, gSec2) + 0x64);  // 0xbc47 / 0xbc4c
        if (com) snd_vcall(ctx, com, 0x4c, SndAbi::Stdcall, {0x3f000000, 0, 0xbf000000, 0});  // 0xbc53-0xbc62
        const u32 sb = ld<u32>(ctx, gSec1);  // 0xbc65
        const uint16_t bal = ld<uint16_t>(ctx, sb + 0xb4);  // 0xbc6a
        const unsigned q = bal ? udiv32(ld<uint32_t>(ctx, sb + 0x38), bal) : 0;  // 0xbc71-0xbc9e
        snd_call(ctx, 0x00001311, SndAbi::Thiscall, {ld<u32>(ctx, gStream), q});  // 0xbca0-0xbca7
        st<uint32_t>(ctx, self + 0x1a4, ld<uint32_t>(ctx, self + 0x1a4) | 8);  // 0xbcac-0xbcb4
    }
    snd_vcall(ctx, self, 0x50, SndAbi::Thiscall, {});  // 0xbcba-0xbcbe
    st<uint32_t>(ctx, self + 0x1a4, ld<uint32_t>(ctx, self + 0x1a4) & 0xfffffffdu);  // 0xbcc1-0xbcca
    const u32 obj = ld<u32>(ctx, snd_va(ctx, 0xb4a1c));  // 0xbcd0
    if (obj) {  // 0xbcd6 / 0xbcd8
        if (snd_vcall(ctx, obj, 0x60, SndAbi::Thiscall, {}).eax != 0) snd_vcall(ctx, ld<u32>(ctx, snd_va(ctx, 0xb4a1c)), 0x58, SndAbi::Thiscall, {});  // 0xbcda-0xbceb
    }
    return 0;  // 0xbcef
}
void a_Wave_Device__start(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Wave_Device__start), Wave_Device__start_body(ctx, a[0]))); }

// ---- Wave_Device::closeOutput (rva 0xbe00): converted from c3ew.cpp WdCloseOut_re
constexpr uint32_t kGBuf49f8 = 0xb49f8;  // c3ew.cpp
constexpr uint32_t kGBuf49fc = 0xb49fc;  // c3ew.cpp
constexpr uint32_t kGBuf4a00 = 0xb4a00;  // c3ew.cpp
uint32_t Wave_Device__closeOutput_body(Ctx &ctx, u32 self) {
    if (ld<uint8_t>(ctx, self + 0x1a4) & 2) return 0;  // 0xbe03-0xbe0c
    u32 m = ld<u32>(ctx, snd_obj(ctx, &SoundState::g_midiDevice).va());  // 0xbe10
    if (m) snd_vcall(ctx, m, 0x5c, SndAbi::Thiscall, {}).eax;  // 0xbe19-0xbe1f
    snd_vcall(ctx, self, 0x4c, SndAbi::Thiscall, {}).eax;  // 0xbe22-0xbe26
    snd_call(ctx, 0x000026fd, SndAbi::Thiscall, {ld<u32>(ctx, snd_obj(ctx, &SoundState::g_soundTimer).va())}).eax;  // 0xbe29 / 0xbe2f
    [&] { SndResidue keep(ctx); return snd_call(ctx, 0x00001f0f, SndAbi::Thiscall, {ld<u32>(ctx, snd_obj(ctx, &SoundState::g_waveOwner).va())}).eax; }();  // 0xbe34 / 0xbe3a (ecx loaded, not read: flushSoundList takes no argument)
    st<uint32_t>(ctx, self + 0x1b0, 0);  // 0xbe3f
    st<uint32_t>(ctx, self + 0x1b4, 0);  // 0xbe45
    u32 d = ld<u32>(ctx, snd_obj(ctx, &SoundState::g_waveDevice).va());  // 0xbe4b
    if (d) {  // 0xbe51 / 0xbe53
        static const unsigned bufs[3] = {kGBuf49f8, kGBuf49fc, kGBuf4a00};
        for (int i = 0; i < 3; i++) {  // 0xbe55-0xbea0
            unsigned b = ld<uint32_t>(ctx, snd_va(ctx, bufs[i]));
            if (b) {
                snd_call(ctx, 0x00001677, SndAbi::Thiscall, {d, b}).eax;
                d = ld<u32>(ctx, snd_obj(ctx, &SoundState::g_waveDevice).va());
                st<uint32_t>(ctx, snd_va(ctx, bufs[i]), 0);
            }
        }
        snd_call(ctx, 0x000020f9, SndAbi::Thiscall, {d}).eax;  // 0xbea6
    }
    st<uint32_t>(ctx, self + 0x1a4, static_cast<uint32_t>(ld<uint32_t>(ctx, self + 0x1a4) | 2));  // 0xbeab-0xbeb4
    return 0;  // 0xbeba
}
void a_Wave_Device__closeOutput(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Wave_Device__closeOutput), Wave_Device__closeOutput_body(ctx, a[0]))); }

// ---- Wave_Device::stopOutput (rva 0xbef0): converted from c3ew.cpp WdStopOut_re
uint32_t Wave_Device__stopOutput_body(Ctx &ctx, u32 self) {
    snd_call(ctx, 0x000026fd, SndAbi::Thiscall, {ld<u32>(ctx, snd_obj(ctx, &SoundState::g_soundTimer).va())}).eax;  // 0xbef3-0xbefa
    if (ld<uint32_t>(ctx, self + 0x1a8) != 0)  // 0xbeff-0xbf09
        snd_call(ctx, 0x00001ed3, SndAbi::Thiscall, {ld<u32>(ctx, snd_obj(ctx, &SoundState::g_soundTimer).va()), c3ew_u(ctx, self)}).eax;  // 0xbf0b-0xbf12
    u32 b = ld<u32>(ctx, snd_obj(ctx, &SoundState::g_primaryBuffer).va());  // 0xbf17
    if (b) snd_call(ctx, 0x00001a2d, SndAbi::Thiscall, {b}).eax;  // 0xbf1d-0xbf21
    if (ld<uint8_t>(ctx, ld<u32>(ctx, snd_obj(ctx, &SoundState::g_waveDevice).va()) + 0x6c) & 0x10) {  // 0xbf26-0xbf2f
        u32 b1 = ld<u32>(ctx, snd_obj(ctx, &SoundState::g_secondaryBuffer1).va());  // 0xbf31
        if (b1) snd_call(ctx, 0x00001a2d, SndAbi::Thiscall, {b1}).eax;  // 0xbf37-0xbf3b
        u32 b2 = ld<u32>(ctx, snd_obj(ctx, &SoundState::g_secondaryBuffer2).va());  // 0xbf40
        if (b2) snd_call(ctx, 0x00001a2d, SndAbi::Thiscall, {b2}).eax;  // 0xbf46-0xbf4a
    }
    u32 s = ld<u32>(ctx, snd_obj(ctx, &SoundState::g_waveOwner).va());  // 0xbf4f
    if (s) st<uint32_t>(ctx, s + 0x18, 0);  // 0xbf54-0xbf58
    st<uint32_t>(ctx, self + 0x1b0, 0);  // 0xbf5b
    st<uint32_t>(ctx, self + 0x1b4, 0);  // 0xbf61
    return c3ew_u(ctx, s);  // eax from 0xbf4f
}
void a_Wave_Device__stopOutput(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Wave_Device__stopOutput), Wave_Device__stopOutput_body(ctx, a[0]))); }

// ---- Wave_Device::startOutput (rva 0xbf90): converted from c3ew.cpp WdStartOut_re
uint32_t Wave_Device__startOutput_body(Ctx &ctx, u32 self) {
    u32 b = ld<u32>(ctx, snd_obj(ctx, &SoundState::g_primaryBuffer).va());  // 0xbf93
    if (b) snd_call(ctx, 0x00001339, SndAbi::Thiscall, {b}).eax;  // 0xbf99-0xbf9d
    if (ld<uint8_t>(ctx, ld<u32>(ctx, snd_obj(ctx, &SoundState::g_waveDevice).va()) + 0x6c) & 0x10) {  // 0xbfa2-0xbfab
        u32 b1 = ld<u32>(ctx, snd_obj(ctx, &SoundState::g_secondaryBuffer1).va());  // 0xbfad
        if (b1) snd_call(ctx, 0x00001203, SndAbi::Thiscall, {b1, 1}).eax;  // 0xbfb3-0xbfb9
        u32 b2 = ld<u32>(ctx, snd_obj(ctx, &SoundState::g_secondaryBuffer2).va());  // 0xbfbe
        if (b2) snd_call(ctx, 0x00001203, SndAbi::Thiscall, {b2, 1}).eax;  // 0xbfc4-0xbfca
    }
    u32 t = ld<u32>(ctx, snd_obj(ctx, &SoundState::g_soundTimer).va());  // 0xbfcf
    if (!t) return 0;  // 0xbfd4 / 0xbfd6
    st<u32>(ctx, self + 0x1a8, t);  // 0xbfd8
    snd_call(ctx, 0x0000227a, SndAbi::Thiscall, {ld<u32>(ctx, snd_obj(ctx, &SoundState::g_soundTimer).va()), c3ew_u(ctx, self)}).eax;  // 0xbfde-0xbfe5
    return snd_call(ctx, 0x00001dfc, SndAbi::Thiscall, {ld<u32>(ctx, snd_obj(ctx, &SoundState::g_soundTimer).va())}).eax;  // 0xbfea / 0xbff0
}
void a_Wave_Device__startOutput(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Wave_Device__startOutput), Wave_Device__startOutput_body(ctx, a[0]))); }

// ---- Wave_Device::restart (rva 0xc020): converted from c3ew.cpp WdRestart_re
uint32_t Wave_Device__restart_body(Ctx &ctx, u32 self, uint32_t idx) {
    u32 s = self;
    u32 d = ld<u32>(ctx, snd_obj(ctx, &SoundState::g_waveDevice).va());  // 0xc023
    if (!d) return 3;  // 0xc02a-0xc02f
    if (!(idx < ld<uint32_t>(ctx, d + 0x148))) return 0xa;  // 0xc03c-0xc047
    if (ld<uint32_t>(ctx, s + 0x1a8) != 0) {  // 0xc050-0xc058
        snd_call(ctx, 0x00001ed3, SndAbi::Thiscall, {ld<u32>(ctx, snd_obj(ctx, &SoundState::g_soundTimer).va()), c3ew_u(ctx, s)}).eax;  // 0xc05a-0xc061
        snd_call(ctx, 0x000026fd, SndAbi::Thiscall, {ld<u32>(ctx, snd_obj(ctx, &SoundState::g_soundTimer).va())}).eax;  // 0xc066 / 0xc06c
        d = ld<u32>(ctx, snd_obj(ctx, &SoundState::g_waveDevice).va());  // 0xc071
    }
    unsigned b = ld<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_primaryBuffer).va());  // 0xc077
    if (b) {  // 0xc07c / 0xc07e
        snd_call(ctx, 0x00001677, SndAbi::Thiscall, {d, b}).eax;  // 0xc080 / 0xc081
        d = ld<u32>(ctx, snd_obj(ctx, &SoundState::g_waveDevice).va());  // 0xc086
        st<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_primaryBuffer).va(), 0);  // 0xc08c
    }
    snd_call(ctx, 0x0000245f, SndAbi::Thiscall, {d}).eax;  // 0xc096
    snd_call(ctx, 0x0000104b, SndAbi::Thiscall, {ld<u32>(ctx, snd_obj(ctx, &SoundState::g_waveDevice).va())}).eax;  // 0xc09b / 0xc0a1
    if (snd_call(ctx, 0x0000192e, SndAbi::Thiscall, {ld<u32>(ctx, snd_obj(ctx, &SoundState::g_waveDevice).va()), idx}).eax) return 6;  // 0xc0a6-0xc0b7
    unsigned r = snd_call(ctx, 0x0000180c, SndAbi::Thiscall, {ld<u32>(ctx, snd_obj(ctx, &SoundState::g_waveDevice).va()), ld<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_dsCoopLevel).va()), 0}).eax;  // 0xc0c0-0xc0ce
    if (r) return r;  // 0xc0d3 / 0xc0d5
    u32 d2 = ld<u32>(ctx, snd_obj(ctx, &SoundState::g_waveDevice).va());  // 0xc0db
    unsigned size = (ld<uint8_t>(ctx, d2 + 0x10) & 0x20) ? 0x20000u : 0x10000u;  // 0xc0e1-0xc0ec
    r = snd_call(ctx, 0x00001046, SndAbi::Thiscall, {d2, c3ew_u(ctx, snd_obj(ctx, &SoundState::g_primaryBuffer).va()), 0x10088, size}).eax;  // 0xc0f1-0xc0fc
    if (r) {  // 0xc101 / 0xc103
        snd_call(ctx, 0x0000104b, SndAbi::Thiscall, {ld<u32>(ctx, snd_obj(ctx, &SoundState::g_waveDevice).va())}).eax;  // 0xc19a / 0xc1a0
        st<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_primaryBuffer).va(), 0);  // 0xc1a5
        return 0x1e;  // 0xc1af
    }
    snd_vcall(ctx, s, 0x3c, SndAbi::Thiscall, {0xf}).eax;  // 0xc109-0xc10f
    unsigned ms = [&] { SndResidue keep(ctx); return snd_vcall(ctx, s, 0x40, SndAbi::Thiscall, {0xe}).eax; }();  // 0xc112-0xc118: `push 0xe` stays (slot 0x40 is `ret`), setRegionMs's second argument
    snd_call(ctx, 0x0000182f, SndAbi::Thiscall, {ld<u32>(ctx, snd_obj(ctx, &SoundState::g_primaryBuffer).va()), ms, 0xe}).eax;  // 0xc11b-0xc122
    snd_call(ctx, 0x00001c5d, SndAbi::Thiscall, {ld<u32>(ctx, snd_obj(ctx, &SoundState::g_primaryBuffer).va())}).eax;  // 0xc127 / 0xc12d
    snd_call(ctx, 0x00001203, SndAbi::Thiscall, {ld<u32>(ctx, snd_obj(ctx, &SoundState::g_primaryBuffer).va()), 1}).eax;  // 0xc132-0xc13a
    u32 buf = ld<u32>(ctx, snd_obj(ctx, &SoundState::g_primaryBuffer).va());  // 0xc13f
    unsigned short w = ld<uint16_t>(ctx, buf + 0xb4);  // 0xc144
    unsigned q = w ? ld<uint32_t>(ctx, buf + 0x38) / w : 0;  // 0xc14b-0xc15f
    snd_call(ctx, 0x00001311, SndAbi::Thiscall, {ld<u32>(ctx, snd_obj(ctx, &SoundState::g_waveOwner).va()), q + q}).eax;  // 0xc161-0xc16b
    st<u32>(ctx, s + 0x1a8, ld<u32>(ctx, snd_obj(ctx, &SoundState::g_soundTimer).va()));  // 0xc170 / 0xc177
    snd_call(ctx, 0x0000227a, SndAbi::Thiscall, {ld<u32>(ctx, snd_obj(ctx, &SoundState::g_soundTimer).va()), c3ew_u(ctx, s)}).eax;  // 0xc17d-0xc183
    snd_call(ctx, 0x00001dfc, SndAbi::Thiscall, {ld<u32>(ctx, snd_obj(ctx, &SoundState::g_soundTimer).va())}).eax;  // 0xc188 / 0xc18e
    return 0;  // 0xc194
}
void a_Wave_Device__restart(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Wave_Device__restart), Wave_Device__restart_body(ctx, a[0], a[1]))); }

// ---- Wave_Device::serviceTick (rva 0xc230): converted from c3fl.cpp SvcTick_re
uint32_t Wave_Device__serviceTick_body(Ctx &ctx, u32 self) {
    snd_call(ctx, 0x00001e83, SndAbi::Thiscall, {self}).eax;  // 0xc234
    u32 dev = ld<u32>(ctx, snd_obj(ctx, &SoundState::g_waveDevice).va());  // 0xc239
    unsigned eax = c3fl_u(ctx, dev);
    if (dev) {
        for (u32 n = ld<u32>(ctx, dev + 0x168); n; n = ld<u32>(ctx, n + 4)) {  // 0xc242-0xc24a / 0xc273-0xc278
            u32 b = ld<u32>(ctx, n + 8);  // 0xc24c
            eax = ld<uint32_t>(ctx, b + 0x44);  // 0xc24f
            if (eax == 0) continue;  // 0xc252 / 0xc254
            eax += ld<uint32_t>(ctx, self + 0x1b4);  // 0xc256 / 0xc25c
            st<uint32_t>(ctx, b + 0x44, eax);  // 0xc260
            if (static_cast<int32_t>(eax) < 0) continue;  // 0xc25e / 0xc263 (jl)
            st<uint32_t>(ctx, b + 0x44, 0);  // 0xc267
            eax = snd_call(ctx, 0x00001203, SndAbi::Thiscall, {b, 0}).eax;  // 0xc265 / 0xc26e
        }
    }
    st<uint32_t>(ctx, self + 0x1b0, 0);  // 0xc27a
    u32 wr = ld<u32>(ctx, snd_obj(ctx, &SoundState::g_waveOwner).va());  // 0xc284
    if (!wr) return eax;  // 0xc28c / 0xc28e -> 0xc2ba
    if (!(ld<uint8_t>(ctx, ld<u32>(ctx, snd_obj(ctx, &SoundState::g_waveDevice).va()) + 0x6c) & 0x10))  // 0xc290-0xc299
        return snd_call(ctx, 0x00001514, SndAbi::Thiscall, {wr, ld<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_primaryBuffer).va())}).eax;  // 0xc29b-0xc2a7
    return snd_call(ctx, 0x000015a5, SndAbi::Thiscall, {wr, ld<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_secondaryBuffer1).va()), ld<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_secondaryBuffer2).va())}).eax;  // 0xc2a8-0xc2ba
}
void a_Wave_Device__serviceTick(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Wave_Device__serviceTick), Wave_Device__serviceTick_body(ctx, a[0]))); }

// ---- WaveCmdQueue::pushNode (rva 0xc2f0): converted from c3dh.cpp WcqPush_re
uint32_t WaveCmdQueue__pushNode_body(Ctx &ctx, u32 self, uint32_t a1, uint32_t a2) {
    unsigned next = (ld<uint32_t>(ctx, self + 0x41dc) + 1) & 0xfff;  // 0xc30a / 0xc316 / 0xc317
    if (next == ld<uint32_t>(ctx, self + 0x41d8)) return 0x22;  // 0xc310 / 0xc31d / 0xc322
    u32 node = snd_new(ctx, 0x10);  // 0xc33b
    if (node) node = snd_call(ctx, 0x000087f0, SndAbi::Thiscall, {node}).eax;  // 0xc355 -> 0x87f0
    st<u32>(ctx, self + 0x1d8 + 4 * next, node);  // 0xc362
    st<uint32_t>(ctx, node, a2);  // 0xc369
    st<uint32_t>(ctx, node + 4, a1);  // 0xc37a
    st<uint32_t>(ctx, self + 0x41dc, next);  // 0xc37d
    return 0;  // 0xc384
}
void a_WaveCmdQueue__pushNode(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(WaveCmdQueue__pushNode), WaveCmdQueue__pushNode_body(ctx, a[0], a[1], a[2]))); }

// ---- WaveCmdQueue::pushValue (rva 0xc3d0): converted from c3an.cpp WaveCmdPush_re
uint32_t WaveCmdQueue__pushValue_body(Ctx &ctx, u32 self, uint32_t value) {
    unsigned next = (ld<uint32_t>(ctx, self + 0x41dc) + 1) & 0xfff;
    if (next == ld<uint32_t>(ctx, self + 0x41d8)) return 0x22;
    st<uint32_t>(ctx, self + 0x1d8 + next * 4, value);
    st<uint32_t>(ctx, self + 0x41dc, next);
    return 0;
}
void a_WaveCmdQueue__pushValue(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(WaveCmdQueue__pushValue), WaveCmdQueue__pushValue_body(ctx, a[0], a[1]))); }

// ---- Wave_Device::flushCommandQueue (rva 0xcc70): converted from c3fd.cpp Flush_re
uint32_t Wave_Device__flushCommandQueue_body(Ctx &ctx, u32 self) {
    unsigned head = ld<uint32_t>(ctx, self + 0x41d8);  // 0xcc76
    unsigned tail = ld<uint32_t>(ctx, self + 0x41dc);  // 0xcc7c
    if (head == tail) return 0;  // 0xcc82 / 0xcc88
    unsigned n = (tail - head) & 0xfff;  // 0xcc91-0xcc99
    unsigned idx = (head + 1) & 0xfff;  // 0xcc96 / 0xcc9e
    for (; n; --n) {  // 0xcca6 / 0xcd0b / 0xcd10
        u32 e = (snd_call(ctx, 0x00002257, SndAbi::Thiscall, {(self + 0x1d8), idx}).eax);  // 0xccb7
        if (e) {  // 0xccbe / 0xccc0
            unsigned t = ld<uint32_t>(ctx, e + (0) * 4) - 1;  // 0xccc2 / 0xccc4
            if (t <= 3) {  // 0xccc5 / 0xccc8
                if (t == 3) {  // table 0xcd28 entry 3 -> 0xccd1
                    u32 o = (ld<uint32_t>(ctx, e + (1) * 4));  // 0xccd1
                    if (o) snd_vcall(ctx, o, 0, SndAbi::Thiscall, {1}).eax;  // 0xccd4-0xccdc
                    st<uint32_t>(ctx, e + (1) * 4, 0);  // 0xccde
                }
                snd_call(ctx, 0x00001816, SndAbi::Thiscall, {e}).eax;  // 0xcce7
                snd_delete(ctx, e);  // 0xcced
            }
            st<uint32_t>(ctx, self + 0x1d8 + 4 * idx, 0);  // 0xccf5
        }
        idx = (idx + 1) & 0xfff;  // 0xcd04 / 0xcd05
    }
    st<uint32_t>(ctx, self + 0x41d8, tail);  // 0xcd18
    return 0;  // 0xcd1f
}
void a_Wave_Device__flushCommandQueue(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Wave_Device__flushCommandQueue), Wave_Device__flushCommandQueue_body(ctx, a[0]))); }

// ---- Wave_Device::dispatchCommand (rva 0xcd80): converted from c3fl.cpp WaveDispatch_re
constexpr uint32_t kIatPostMessageA = 0xb85a0;  // c3fl.cpp
// Append obj to the device list +0x1c8 ({+0 head, +4 tail, +8 cursor, +0xc count}, nodes {+0 prev, +4 next, +8 object}): operator
// new 0x424d7 (0xc bytes) gets {0, 0, obj}; a non-NULL tail links both ways, else the head is the node; tail = node, count + 1
// (0xce36-0xce82 / 0xcee4-0xcf30). A NULL block makes the node pointer 0 (0xce56) and the links write through it.
void c3fl_appendObject(Ctx &ctx, u32 list, u32 obj) {
    u32 n = (snd_new(ctx, 0xc));
    if (n) {
        st<uint32_t>(ctx, n, 0);
        st<uint32_t>(ctx, n + 4, 0);
        st<u32>(ctx, n + 8, obj);
    }
    u32 tail = ld<u32>(ctx, list + 4);
    if (tail) {
        st<u32>(ctx, tail + 4, n);
        st<u32>(ctx, n, ld<u32>(ctx, list + 4));
    } else {
        st<u32>(ctx, list, n);
    }
    st<u32>(ctx, list + 4, n);
    st<int32_t>(ctx, list + 0xc, static_cast<int32_t>(ld<int32_t>(ctx, list + 0xc) + 1));
}
// The doubly linked node list {+0 head, +4 tail, +8 cursor, +0xc count} of nodes {+0 prev, +4 next, +8 object} that flushSoundList
// (0x8559-0x85af) and Midi_Device::dispatchCommand (0xa720-0xa775, 0xa7a5-0xa810) unlink an object from:
// the first node whose object is `obj` (searched from the head); st<u32>(ctx, prev + 4, next or head = next); st<u32>(ctx, next, prev with the
// cursor = next, or (no next) cursor = 0 and tail = prev); the node goes to operator delete and the count drops by one.
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
// Stop a channel's voices: bit 4 of +0xc8 set, Voice_setLoopFlag (thunk 0x154b) on +0x70 with 0..15, bit 6 of +0xc8 cleared
// (+0xc8 re-read) (0xcfbe-0xcfe2, 0xd1aa-0xd1d7, 0xd26e-0xd291, 0xd445-0xd46c, 0xd6b5-0xd6dc).
void c3fl_stopVoices(Ctx &ctx, u32 obj) {
    st<uint32_t>(ctx, obj + 0xc8, static_cast<uint32_t>(ld<uint32_t>(ctx, obj + 0xc8) | 0x10));
    for (unsigned i = 0; i < 0x10; i++) snd_call(ctx, 0x0000154b, SndAbi::Thiscall, {obj + 0x70, i}).eax;
    st<uint32_t>(ctx, obj + 0xc8, static_cast<uint32_t>(ld<uint32_t>(ctx, obj + 0xc8) & 0xffffffbfu));
}
// Post message 0x7ee for `c` about obj: bit 0 of obj's byte +0x1428 set, then with bit 5 of the device's +0x1a4 PostMessageA
// (slot 0xb85a0) to obj's window +0x1420 with (0x7ee, 0, c), else postSoundThreadMessage (thunk 0x11d6) on 0xb5038 with
// (0x7ee, 0, c) (0xd001-0xd031 / 0xd03c, 0xd077-0xd098, 0xd2c1-0xd30d, 0xd47a-0xd4aa,
// 0xd70d-0xd756, 0xd797-0xd7d8).
void c3fl_post7ee(Ctx &ctx, u32 dev, u32 obj, u32 c) {
    st<uint8_t>(ctx, obj + 0x1428, static_cast<uint8_t>(ld<uint8_t>(ctx, obj + 0x1428) | 1));
    if ((ld<uint32_t>(ctx, dev + 0x1a4) >> 5) & 1)
        snd_import(ctx, kIatPostMessageA, SndAbi::Stdcall, {ld<uint32_t>(ctx, obj + 0x1420), 0x7ee, 0, c3fl_u(ctx, c)}).eax;
    else
        snd_call(ctx, 0x000011d6, SndAbi::Thiscall, {snd_arr(ctx, &SoundState::g_soundThreadState).va(), 0x7ee, 0, c3fl_u(ctx, c)}).eax;
}
void c3fl_freePair(Ctx &ctx, u32 c) {
    snd_call(ctx, 0x00001816, SndAbi::Thiscall, {c}).eax;  // 0xd7e6 / 0xd7e8
    snd_delete(ctx, c);  // 0xd7ed / 0xd7ee
}
uint32_t Wave_Device__dispatchCommand_body(Ctx &ctx, u32 self) {
    u32 dv = self;
    u32 list = dv + 0x1c8;
    unsigned readI = ld<uint32_t>(ctx, dv + 0x41d8);  // 0xcd9b
    unsigned writeI = ld<uint32_t>(ctx, dv + 0x41dc);  // 0xcda1
    if (readI == writeI) return readI;  // 0xcda7 / 0xcdad
    unsigned n = (writeI - readI) & 0xfff;  // 0xcdb7 / 0xcdbc
    unsigned idx = (readI + 1) & 0xfff;  // 0xcdb9 / 0xcdc2
    if (n == 0) {  // 0xcdcc / 0xcdda
        st<uint32_t>(ctx, dv + 0x41d8, writeI);  // 0xd82d
        return readI;
    }
    for (unsigned k = 0; k < n; k++) {  // 0xd816-0xd821
        u32 c = (snd_call(ctx, 0x00002257, SndAbi::Thiscall, {dv + 0x1d8, idx}).eax);  // 0xcde1-0xcde8
        if (!c) continue;  // 0xcdef / 0xcdf5
        u32 o = ld<u32>(ctx, c + 4);
        bool doFree = true;
        switch (ld<uint32_t>(ctx, c)) {  // 0xcdfb-0xce07
        case 1: {  // 0xce0e
            unsigned f44 = ld<uint32_t>(ctx, o + 0x44);  // 0xce11
            if (f44 & 4) {  // 0xce14 / 0xce16
                if (ld<uint8_t>(ctx, o + 0xc8) & 0x40) break;  // 0xce1c / 0xce23
                if (ld<uint8_t>(ctx, o + 0x1428) & 1) break;  // 0xce29 / 0xce30
                c3fl_appendObject(ctx, list, o);  // 0xce36-0xce82
                st<uint32_t>(ctx, ld<u32>(ctx, c + 4) + 0xc8, static_cast<uint32_t>(ld<uint32_t>(ctx, ld<u32>(ctx, c + 4) + 0xc8) | 0x40));  // 0xce88-0xce94
                u32 o2 = ld<u32>(ctx, c + 4);  // 0xce97
                if (!(ld<uint8_t>(ctx, o2 + 0x58) & 2)) break;  // 0xce9a / 0xce9e
                if (snd_vcall(ctx, o2, 0xcc, SndAbi::Thiscall, {}).eax == 0) break;  // 0xcea4-0xceae
                o2 = ld<u32>(ctx, c + 4);  // 0xceb4
                if (!(ld<uint8_t>(ctx, o2 + 0xc8) & 1)) break;  // 0xceb7 / 0xcebe
                snd_call(ctx, 0x00001f37, SndAbi::Thiscall, {o2 + 0x70}).eax;  // 0xcec4 / 0xcec7
            } else {
                if (!((f44 >> 8) & 1)) break;  // 0xced1 / 0xced4
                if (ld<uint8_t>(ctx, o + 0x58) & 2) break;  // 0xceda / 0xcede
                c3fl_appendObject(ctx, list, o);  // 0xcee4-0xcf30
                st<uint32_t>(ctx, ld<u32>(ctx, c + 4) + 0x58, static_cast<uint32_t>(ld<uint32_t>(ctx, ld<u32>(ctx, c + 4) + 0x58) | 2));  // 0xcf36 / 0xcf39
            }
            break;
        }
        case 2: {  // 0xcf42
            unsigned f44 = ld<uint32_t>(ctx, o + 0x44);  // 0xcf45
            if (f44 & 4) {  // 0xcf48 / 0xcf4a
                c3fl_unlinkObject(ctx, list, o);  // 0xcf50-0xcfb8
                c3fl_stopVoices(ctx, o);  // 0xcfbe-0xcfe2
                if (ld<uint8_t>(ctx, o + 0x58) & 2) {  // 0xcfe5-0xcfea
                    st<uint32_t>(ctx, c, 3);  // 0xcff0
                } else {
    if (ld<u32>(ctx, o + 0xc4)) {  // 0xd046-0xd04e
                        snd_call(ctx, 0x000011f9, SndAbi::Thiscall, {o + 0x70}).eax;  // 0xd050 / 0xd052
                        snd_call(ctx, 0x000022e8, SndAbi::Thiscall, {ld<u32>(ctx, o + 0xc4)}).eax;  // 0xd057 / 0xd05d
                    }
                    st<uint32_t>(ctx, c, 5);  // 0xd066
                }
                doFree = false;
                u32 o2 = ld<u32>(ctx, c + 4);  // 0xcff6 / 0xd06c
                if (o2) c3fl_post7ee(ctx, dv, o2, c);  // 0xcff9-0xd041 / 0xd06f-0xd098
            } else {
                if (!((f44 >> 8) & 1)) { doFree = false; break; }  // 0xd09d / 0xd0a0
                c3fl_unlinkObject(ctx, list, o);  // 0xd0a6-0xd118
                st<uint32_t>(ctx, ld<u32>(ctx, c + 4) + 0x58, static_cast<uint32_t>(ld<uint32_t>(ctx, ld<u32>(ctx, c + 4) + 0x58) & 0xfffffffcu));  // 0xd0bc-0xd0bf / 0xd11e-0xd121
            }
            break;
        }
        case 3: {  // 0xd12a
            doFree = false;
            unsigned f44 = ld<uint32_t>(ctx, o + 0x44);  // 0xd131
            if (f44 & 4) {  // 0xd134 / 0xd136
                c3fl_unlinkObject(ctx, list, o);  // 0xd13c-0xd1a4
                c3fl_stopVoices(ctx, o);  // 0xd1aa-0xd1d7
                for (int i = 1; i < ld<int32_t>(ctx, o + 0x1438); i++) {  // 0xd1d2-0xd1e2 / 0xd298-0xd2ac
                    u32 sub = ld<u32>(ctx, o + 4 * i + 0x1440);  // 0xd1f2-0xd1f6
                    if (!sub) continue;  // 0xd1f8 / 0xd1fa
                    c3fl_unlinkObject(ctx, list, sub);  // 0xd200-0xd268
                    c3fl_stopVoices(ctx, sub);  // 0xd26e-0xd291
                }
                u32 o2 = ld<u32>(ctx, c + 4);  // 0xd2b2 / 0xd2b6
                if (o2) c3fl_post7ee(ctx, dv, o2, c);  // 0xd2b9-0xd30d
            } else {
                if (!((f44 >> 8) & 1)) break;  // 0xd317 / 0xd31a
                c3fl_unlinkObject(ctx, list, o);  // 0xd320-0xd39a
                st<uint32_t>(ctx, o + 0x58, static_cast<uint32_t>(ld<uint32_t>(ctx, o + 0x58) & 0xfffffffcu));  // 0xd336-0xd33d / 0xd3a0-0xd3a7
                snd_call(ctx, 0x00001e79, SndAbi::Thiscall, {o}).eax;  // 0xd340 / 0xd3aa
            }
            break;
        }
        case 4: {  // 0xd3b4
            unsigned f44 = ld<uint32_t>(ctx, o + 0x44);  // 0xd3b7
            if (f44 & 4) {  // 0xd3ba / 0xd3bc
                doFree = false;
                if (!o) break;  // 0xd3c2 / 0xd3c4
                if (ld<uint8_t>(ctx, o + 0xc8) & 0x40) {  // 0xd3ca / 0xd3d1
                    c3fl_unlinkObject(ctx, list, o);  // 0xd3d7-0xd43f
                    c3fl_stopVoices(ctx, o);  // 0xd445-0xd46c
                }
                u32 o2 = ld<u32>(ctx, c + 4);  // 0xd46f
                if (o2) c3fl_post7ee(ctx, dv, o2, c);  // 0xd472-0xd4aa
            } else {
                if (!((f44 >> 8) & 1)) { doFree = false; break; }  // 0xd4b5 / 0xd4b8
                if (ld<uint8_t>(ctx, o + 0x58) & 2) c3fl_unlinkObject(ctx, list, o);  // 0xd4be-0xd52c
                u32 o2 = ld<u32>(ctx, c + 4);  // 0xd532
                if (o2) snd_vcall(ctx, o2, 0, SndAbi::Thiscall, {1}).eax;  // 0xd535-0xd541 / 0xd7e4
            }
            break;
        }
        case 6:  // 0xd546
            if (o && (ld<uint8_t>(ctx, o + 0xcc) & 4)) snd_call(ctx, 0x0000168b, SndAbi::Thiscall, {o + 0x70}).eax;  // 0xd549-0xd561
            break;
        case 13:  // 0xd56b
            if (o) {
                st<uint32_t>(ctx, o + 0x1430, ld<uint32_t>(ctx, c + 8));  // 0xd576 / 0xd579
                st<uint32_t>(ctx, o + 0x1434, ld<uint32_t>(ctx, c + 0xc));  // 0xd57f / 0xd582
            }
            break;
        case 14:  // 0xd58d
            if (o) {
                u32 e = ld<u32>(ctx, o + 0xf8);  // 0xd598
                if (e) {
    if (ld<uint32_t>(ctx, c + 8) == 0) snd_vcall(ctx, e, 0x34, SndAbi::Thiscall, {0, 0xffffffffu, ld<uint32_t>(ctx, c + 0xc)}).eax;  // 0xd5b3-0xd5b7
                    else snd_vcall(ctx, e, 0x34, SndAbi::Thiscall, {0xffffffffu, 0, ld<uint32_t>(ctx, c + 0xc)}).eax;  // 0xd5bf-0xd5c3
                }
            }
            break;
        case 15:  // 0xd5cb
            if (o) {
                u32 e = ld<u32>(ctx, o + 0xf8);  // 0xd5d6
                if (e) snd_vcall(ctx, e, 0x40, SndAbi::Thiscall, {ld<uint32_t>(ctx, c + 8)}).eax;  // 0xd5e4-0xd5ea
            }
            break;
        case 16:  // 0xd5f2
            if (o) {
                u32 e = ld<u32>(ctx, o + 0xf8);  // 0xd5fd
                if (e) snd_call(ctx, 0x0000272a, SndAbi::Thiscall, {e + 0x70, 0x7d0}).eax;  // 0xd60b-0xd613
            }
            break;
        case 17: {  // 0xd61d
            if (!o) break;  // 0xd620 / 0xd626
            u32 e = ld<u32>(ctx, o + 0xf8);  // 0xd62c
            if (e) {
                if (ld<uint8_t>(ctx, e + 0xc8) & 0x40) {  // 0xd63a / 0xd641
                    c3fl_unlinkObject(ctx, list, e);  // 0xd647-0xd6af
                    c3fl_stopVoices(ctx, e);  // 0xd6b5-0xd6dc
                }
                u32 nd = (snd_new(ctx, 0x10));  // 0xd6df-0xd6e9
                u32 q = nd ? (snd_call(ctx, 0x00002031, SndAbi::Thiscall, {nd}).eax) : 0;  // 0xd6f9-0xd702
                st<uint32_t>(ctx, q, 4);  // 0xd704
                st<u32>(ctx, q + 4, e);  // 0xd70a
                c3fl_post7ee(ctx, dv, e, q);  // 0xd70d-0xd756
            }
            u32 e2 = ld<u32>(ctx, o + 0xfc);  // 0xd75b / 0xd75f
            if (e2) {
                u32 nd = (snd_new(ctx, 0x10));  // 0xd769-0xd773
                u32 q = nd ? (snd_call(ctx, 0x00002031, SndAbi::Thiscall, {nd}).eax) : 0;  // 0xd783-0xd78c
                st<uint32_t>(ctx, q, 4);  // 0xd78e
                st<u32>(ctx, q + 4, e2);  // 0xd794
                c3fl_post7ee(ctx, dv, e2, q);  // 0xd797-0xd7d8
            }
            snd_vcall(ctx, o, 0, SndAbi::Thiscall, {1}).eax;  // 0xd7dd-0xd7e4
            break;
        }
        default:  // 5, 7..12, others: 0xd7f6
            doFree = false;
            break;
        }
        if (doFree) c3fl_freePair(ctx, c);  // 0xd7e6-0xd7f3
        st<u32>(ctx, dv + idx * 4 + 0x1d8, 0);  // 0xd801
        idx = (idx + 1) & 0xfff;  // 0xd7fe / 0xd80c
    }
    st<uint32_t>(ctx, dv + 0x41d8, writeI);  // 0xd827 / 0xd82d
    return n;  // eax from 0xd816
}
void a_Wave_Device__dispatchCommand(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Wave_Device__dispatchCommand), Wave_Device__dispatchCommand_body(ctx, a[0]))); }

// ---- Wave_Device::scanCommandRing (rva 0xdcf0): converted from c3fd.cpp WaveScan_re
// CommandQueue::at 0x88c0 through its thunk 0x2257: __thiscall (ring, index), returns ring[index & 0xfff].
u32 c3fd_at(Ctx &ctx, u32 ring, uint32_t idx) {
    return (snd_call(ctx, 0x00002257, SndAbi::Thiscall, {ring, idx}).eax);
}
// pairClear 0x8840 (thunk 0x1816) on the entry, then operator delete, when the entry is non-NULL.
void c3fd_freeEntry(Ctx &ctx, u32 e) {
    if (!e) return;
    snd_call(ctx, 0x00001816, SndAbi::Thiscall, {e}).eax;
    snd_delete(ctx, e);
}
// The shared body of the two scanCommandRing rows (below). ringOff is the ring's offset in the device (+0x50 Midi, +0x1d8 Wave),
// cmpAll whether entries also compare their dwords +8 and +0xc, flagOff / flagBit the byte tested on the entry's object.
// i walks from *first, j from i + 1; the walk of j for one i ends when j reaches *last or the entry at i is gone; i ends at
// *last. For two live entries with the same object (+4): equal kinds (and, for Midi, equal +8 and +0xc) delete the j entry;
// kinds (1, 2) delete the i entry when its object has the flag bit, else both; kinds (2, 1) delete the i entry when the object
// is non-NULL without the flag bit, else both; anything else keeps both.
uint32_t c3fd_scanRing(Ctx &ctx, u32 self, u32 first, u32 last, int32_t ringOff, uint8_t cmpAll, int32_t flagOff, uint8_t flagBit) {
    u32 ring = (self + ringOff);
    unsigned i = ld<uint32_t>(ctx, first);
    unsigned j = (i + 1) & 0xfff;
    unsigned nextI = j;
    if (i == ld<uint32_t>(ctx, last)) return 0;
    for (;;) {
        if (j == ld<uint32_t>(ctx, last)) return 0;
        if (c3fd_at(ctx, ring, i)) {
            do {
                if (j == ld<uint32_t>(ctx, last)) break;
                if (c3fd_at(ctx, ring, j)) {
    u32 b = c3fd_at(ctx, ring, j);
                    u32 a = c3fd_at(ctx, ring, i);
                    if (ld<uint32_t>(ctx, a + (1) * 4) == ld<uint32_t>(ctx, b + (1) * 4)) {
    if (ld<uint32_t>(ctx, a + (0) * 4) == ld<uint32_t>(ctx, b + (0) * 4) && (!cmpAll || (ld<uint32_t>(ctx, a + (2) * 4) == ld<uint32_t>(ctx, b + (2) * 4) && ld<uint32_t>(ctx, a + (3) * 4) == ld<uint32_t>(ctx, b + (3) * 4)))) {
    c3fd_freeEntry(ctx, c3fd_at(ctx, ring, j));
                            st<uint32_t>(ctx, self + ringOff + 4 * j, 0);
                        } else if (ld<u32>(ctx, c3fd_at(ctx, ring, i) + 0 * 4) == 1 && ld<u32>(ctx, c3fd_at(ctx, ring, j) + 0 * 4) == 2) {
    u32 o = (ld<u32>(ctx, c3fd_at(ctx, ring, i) + 1 * 4));
                            if (o && (ld<uint8_t>(ctx, o + flagOff) & flagBit)) {
    c3fd_freeEntry(ctx, c3fd_at(ctx, ring, i));
                                st<uint32_t>(ctx, self + ringOff + 4 * i, 0);
                            } else {
    c3fd_freeEntry(ctx, c3fd_at(ctx, ring, i));
                                st<uint32_t>(ctx, self + ringOff + 4 * i, 0);
                                c3fd_freeEntry(ctx, c3fd_at(ctx, ring, j));
                                st<uint32_t>(ctx, self + ringOff + 4 * j, 0);
                            }
                        } else if (ld<u32>(ctx, c3fd_at(ctx, ring, i) + 0 * 4) == 2 && ld<u32>(ctx, c3fd_at(ctx, ring, j) + 0 * 4) == 1) {
    u32 o = (ld<u32>(ctx, c3fd_at(ctx, ring, i) + 1 * 4));
                            if (o && !(ld<uint8_t>(ctx, o + flagOff) & flagBit)) {
    c3fd_freeEntry(ctx, c3fd_at(ctx, ring, i));
                                st<uint32_t>(ctx, self + ringOff + 4 * i, 0);
                            } else {
    c3fd_freeEntry(ctx, c3fd_at(ctx, ring, i));
                                st<uint32_t>(ctx, self + ringOff + 4 * i, 0);
                                c3fd_freeEntry(ctx, c3fd_at(ctx, ring, j));
                                st<uint32_t>(ctx, self + ringOff + 4 * j, 0);
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
        if (i == ld<uint32_t>(ctx, last)) return 0;
    }
}
uint32_t Wave_Device__scanCommandRing_body(Ctx &ctx, u32 self, u32 first, u32 last) {
    return c3fd_scanRing(ctx, self, first, last, 0x1d8, false, 0xc8, 0x40);
}
void a_Wave_Device__scanCommandRing(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Wave_Device__scanCommandRing), Wave_Device__scanCommandRing_body(ctx, a[0], a[1], a[2]))); }

// ---- Wave_Device::caps (rva 0xe230): converted from c3dt.cpp WdCaps_re
uint32_t Wave_Device__caps_body(Ctx &ctx) {
    u32 dev = c3dt_glob(ctx, 0x000b49f0);  // 0xe230
    if (!dev) return 0;  // 0xe236 / 0xe238
    return snd_call(ctx, 0x00001c26, SndAbi::Thiscall, {dev}).eax;  // 0xe23a
}
void a_Wave_Device__caps(Ctx &ctx, const uint32_t *, Ret &r) { r.set_u32(neg_u32(NEG(Wave_Device__caps), Wave_Device__caps_body(ctx))); }

// ---- Wave_Device::deviceName (rva 0xe260): converted from c3dt.cpp WdName_re
uint32_t Wave_Device__deviceName_body(Ctx &ctx, uint32_t a1, u32 a2, uint32_t a3) {
    u32 dev = c3dt_glob(ctx, 0x000b49f0);  // 0xe260
    if (!dev) {  // 0xe266 / 0xe268
        st<uint8_t>(ctx, a2, 0);  // 0xe28a (mov byte ptr [ecx], 0)
        return 3;  // 0xe285
    }
    return snd_call(ctx, 0x0000222f, SndAbi::Thiscall, {dev, a1, a2, a3}).eax;  // 0xe279
}
void a_Wave_Device__deviceName(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Wave_Device__deviceName), Wave_Device__deviceName_body(ctx, a[0], a[1], a[2]))); }

// ---- setOutputMasterVolume (rva 0xe2a0): hand-written from the instructions
// `ret 4`, one stack argument (the volume). ecx = g_dsDevice (0xb49f0, 0xe2a0); NULL returns with eax untouched (0xe2a8 -> 0xe2b4: whatever
// the caller left, so the port returns 0 and the key compares state only); otherwise DirectSoundDevice::setMasterVolume (thunk 0x2388)
// runs with ecx = the device and the volume pushed (0xe2aa-0xe2af); its result is returned.
uint32_t setOutputMasterVolume_body(Ctx &ctx, u32 vol) {
    const u32 dev = ld<u32>(ctx, snd_va(ctx, 0xb49f0));  // 0xe2a0
    if (!dev) return 0;  // 0xe2a6 / 0xe2a8
    return snd_call(ctx, 0x00002388, SndAbi::Thiscall, {dev, vol}).eax;  // 0xe2aa-0xe2af
}
void a_setOutputMasterVolume(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(setOutputMasterVolume), setOutputMasterVolume_body(ctx, a[0]))); }

// ---- getWaveDeviceInterface (rva 0xe2d0): converted from c3fu.cpp GetWaveDeviceInterface_re
uint32_t getWaveDeviceInterface_body(Ctx &ctx) {
    u32 d = ld<u32>(ctx, snd_obj(ctx, &SoundState::g_waveDevice).va());  // 0xe2d0
    if (d == 0) return 0;  // 0xe2d5 / 0xe2dc
    return ld<uint32_t>(ctx, d);  // 0xe2d9
}
void a_getWaveDeviceInterface(Ctx &ctx, const uint32_t *, Ret &r) { r.set_u32(neg_u32(NEG(getWaveDeviceInterface), getWaveDeviceInterface_body(ctx))); }

// ---- serviceWaveDevice (rva 0xe310): converted from c3dt.cpp WdService_re
uint32_t serviceWaveDevice_body(Ctx &ctx, uint32_t arg) {
    u32 dev = c3dt_glob(ctx, 0x000b49f0);  // 0xe310
    if (dev && (ld<uint8_t>(ctx, dev + 0x6c) & 0x10))  // 0xe316 / 0xe318 / 0xe31a / 0xe31e
        return snd_call(ctx, 0x00001672, SndAbi::Thiscall, {dev, arg}).eax;  // 0xe325
    return 0;  // 0xe32d
}
void a_serviceWaveDevice(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(serviceWaveDevice), serviceWaveDevice_body(ctx, a[0]))); }

// ---- stubReturnBA (rva 0xe350): hand-written from the instructions
// `mov eax, 0xb; ret 4` (0xe350 / 0xe355): a (this, one dword) slot that reports 0xb.
uint32_t stubReturnBA_body() { return 0xb; }
void a_stubReturnBA(Ctx &, const uint32_t *, Ret &r) { r.set_u32(neg_u32(NEG(stubReturnBA), stubReturnBA_body())); }

// ---- Wave_In_Device::scalarDeletingDtor (rva 0xe8c0): converted from c3dj.cpp WaveInScalarDeletingDtor_re
u32 Wave_In_Device__scalarDeletingDtor_body(Ctx &ctx, u32 self, uint32_t flags) {
    snd_call(ctx, 0x0000232e, SndAbi::Thiscall, {self});
if (flags & 1) snd_delete(ctx, self);
return self;
}
void a_Wave_In_Device__scalarDeletingDtor(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Wave_In_Device__scalarDeletingDtor), Wave_In_Device__scalarDeletingDtor_body(ctx, a[0], a[1]))); }

// ---- Wave_In_Device::dtor (rva 0xe8f0): converted from c3dj.cpp WaveInDtor_re
void Wave_In_Device__dtor_body(Ctx &ctx, u32 self) {
    st<u32>(ctx, self, snd_va(ctx, snd_rva::vt_WaveInDevice));  // 0xe8f0
    snd_call(ctx, 0x00001e06, SndAbi::Thiscall, {self});  // 0xe8f6 -> 0x2a3b0
}
void a_Wave_In_Device__dtor(Ctx &ctx, const uint32_t *a, Ret &) { Wave_In_Device__dtor_body(ctx, a[0]); neg_mem(ctx, NEG(Wave_In_Device__dtor), a[0] + 0x0); }

// ---- Dll_Wave_In_Device::create_device (rva 0xe910): converted from c3fd.cpp WaveInCreate_re
constexpr uint32_t kWaveInDevice = 0xb4a24;  // c3fd.cpp
uint32_t Dll_Wave_In_Device__create_device_body(Ctx &ctx, u32 out, uint32_t) {
    if (c3fd_gref(ctx, kWaveInDevice)) return 0xc;  // 0xe926-0xe930 / 0xe9be
    u32 p = snd_new(ctx, 0x4c);  // 0xe939
    if (!p) {  // 0xe947 / 0xe94d
        st<u32>(ctx, snd_va(ctx, kWaveInDevice), 0);  // 0xe99f
        st<u32>(ctx, out, 0);  // 0xe9a7
        return 0;  // 0xe9a5
    }
    snd_call(ctx, 0x00002761, SndAbi::Thiscall, {p}).eax;  // 0xe951
    st<u32>(ctx, p, snd_va(ctx, snd_rva::vt_WaveInDevice));  // 0xe95f
    st<uint32_t>(ctx, p + 0x30, 0);  // 0xe965
    st<uint32_t>(ctx, p + 0x34, 0);  // 0xe968
    st<uint32_t>(ctx, p + 0x38, 0);  // 0xe96b
    st<uint32_t>(ctx, p + 0x40, 0x7f);  // 0xe96e
    st<uint32_t>(ctx, p + 0x24, 0);  // 0xe975
    st<uint32_t>(ctx, p + 0x28, 0);  // 0xe977
    st<uint32_t>(ctx, p + 0x2c, 0);  // 0xe97a
    st<uint32_t>(ctx, p + 0x3c, 0);  // 0xe97d
    st<u32>(ctx, snd_va(ctx, kWaveInDevice), p);  // 0xe980
    st<u32>(ctx, out, p);  // 0xe986
    return 0;  // 0xe959
}
void a_Dll_Wave_In_Device__create_device(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Dll_Wave_In_Device__create_device), Dll_Wave_In_Device__create_device_body(ctx, a[0], a[1]))); }

// ---- Dll_Wave_In_Device::delete_device (rva 0xea00): converted from c3fd.cpp DelWaveIn_re
uint32_t Dll_Wave_In_Device__delete_device_body(Ctx &ctx) {
    u32 dev = c3fd_gref(ctx, kWaveInDevice);  // 0xea00
    if (dev) snd_vcall(ctx, dev, 0, SndAbi::Thiscall, {1}).eax;  // 0xea08-0xea0e
    st<u32>(ctx, snd_va(ctx, kWaveInDevice), 0);  // 0xea10
    return 0;  // 0xea1a
}
void a_Dll_Wave_In_Device__delete_device(Ctx &ctx, const uint32_t *, Ret &r) { r.set_u32(neg_u32(NEG(Dll_Wave_In_Device__delete_device), Dll_Wave_In_Device__delete_device_body(ctx))); }

// ---- Wave_In_Device::start (rva 0xea30): converted from c3ew.cpp WiStartDev_re
uint32_t Wave_In_Device__start_body(Ctx &ctx) {
    u32 w = ld<u32>(ctx, snd_obj(ctx, &SoundState::g_waveInCapture).va());  // 0xea30
    if (!w) return 3;  // 0xea36-0xea3a
    unsigned r = snd_call(ctx, 0x0000213a, SndAbi::Thiscall, {w}).eax;  // 0xea40
    if (r) return r;  // 0xea45 / 0xea47
    r = snd_call(ctx, 0x0000219e, SndAbi::Thiscall, {ld<u32>(ctx, snd_obj(ctx, &SoundState::g_waveInCapture).va())}).eax;  // 0xea49 / 0xea4f
    if (r) return r;  // 0xea54 / 0xea56
    r = snd_call(ctx, 0x000024b4, SndAbi::Thiscall, {ld<u32>(ctx, snd_obj(ctx, &SoundState::g_waveInCapture).va())}).eax;  // 0xea58 / 0xea5e
    if (r) return r;  // 0xea63 / 0xea65
    return snd_call(ctx, 0x00002121, SndAbi::Thiscall, {ld<u32>(ctx, snd_obj(ctx, &SoundState::g_waveInCapture).va())}).eax;  // 0xea67 / 0xea6d
}
void a_Wave_In_Device__start(Ctx &ctx, const uint32_t *, Ret &r) { r.set_u32(neg_u32(NEG(Wave_In_Device__start), Wave_In_Device__start_body(ctx))); }

// ---- Wave_In_Device::stop (rva 0xeaa0): converted from c3dt.cpp WiStop_re
uint32_t Wave_In_Device__stop_body(Ctx &ctx) {
    u32 g = c3dt_glob(ctx, 0x000b4a04);  // 0xeaa0
    if (!g) return 3;  // 0xeaa6 / 0xeaa8
    snd_call(ctx, 0x00001109, SndAbi::Thiscall, {g}).eax;  // 0xeab0
    return snd_call(ctx, 0x000024aa, SndAbi::Thiscall, {c3dt_glob(ctx, 0x000b4a04)}).eax;  // 0xeab5 / 0xeabb
}
void a_Wave_In_Device__stop(Ctx &ctx, const uint32_t *, Ret &r) { r.set_u32(neg_u32(NEG(Wave_In_Device__stop), Wave_In_Device__stop_body(ctx))); }

// ---- Wave_In_Device::openCapture (rva 0xead0): converted from c3ew.cpp WiOpenCap_re
uint32_t Wave_In_Device__openCapture_body(Ctx &ctx, u32 self, uint32_t a1, uint32_t) {
    if (ld<uint32_t>(ctx, self + 0x28) != 0) return 0;  // 0xeaea-0xeaef
    if (ld<u32>(ctx, snd_obj(ctx, &SoundState::g_waveInCapture).va()) != 0) {  // 0xeaf5-0xeafc
        st<uint32_t>(ctx, self + 0x28, 1);  // 0xeb7a
        return 0;
    }
    u32 p = snd_new(ctx, 0x70);  // 0xeafe / 0xeb00
    u32 obj = p ? (snd_call(ctx, 0x00001fa5, SndAbi::Thiscall, {p}).eax) : 0;  // 0xeb0c-0xeb21
    st<u32>(ctx, snd_obj(ctx, &SoundState::g_waveInCapture).va(), obj);  // 0xeb34
    unsigned r = snd_call(ctx, 0x00002162, SndAbi::Thiscall, {obj, 0xa, 0x168}).eax;  // 0xeb23-0xeb39
    if (r) {  // 0xeb40 / 0xeb42
        u32 w = ld<u32>(ctx, snd_obj(ctx, &SoundState::g_waveInCapture).va());  // 0xeb44
        if (w) snd_vcall(ctx, w, 0, SndAbi::Thiscall, {1}).eax;  // 0xeb4a-0xeb52
        st<u32>(ctx, snd_obj(ctx, &SoundState::g_waveInCapture).va(), 0);  // 0xeb57
        return r;  // 0xeb54
    }
    st<uint32_t>(ctx, self + 0x48, a1);  // 0xeb73 / 0xeb77
    st<uint32_t>(ctx, self + 0x28, 1);  // 0xeb7a
    return 0;  // 0xeb86
}
void a_Wave_In_Device__openCapture(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Wave_In_Device__openCapture), Wave_In_Device__openCapture_body(ctx, a[0], a[1], a[2]))); }

// ---- Wave_In_Device::closeCapture (rva 0xec00): converted from c3ew.cpp WiCloseCap_re
uint32_t Wave_In_Device__closeCapture_body(Ctx &ctx, u32 self) {
    u32 t = ld<u32>(ctx, self + 0x30);  // 0xec03
    if (t) {  // 0xec06 / 0xec08
        snd_call(ctx, 0x000026fd, SndAbi::Thiscall, {t}).eax;  // 0xec0a
        snd_call(ctx, 0x00001ed3, SndAbi::Thiscall, {ld<u32>(ctx, snd_obj(ctx, &SoundState::g_soundTimer).va()), c3ew_u(ctx, self)}).eax;  // 0xec0f-0xec16
    }
    u32 w = ld<u32>(ctx, snd_obj(ctx, &SoundState::g_waveInCapture).va());  // 0xec1b
    if (w) {  // 0xec22 / 0xec24
        snd_vcall(ctx, w, 0, SndAbi::Thiscall, {1}).eax;  // 0xec26-0xec2a
        st<u32>(ctx, snd_obj(ctx, &SoundState::g_waveInCapture).va(), 0);  // 0xec2c
    }
    return 0;  // 0xec36
}
void a_Wave_In_Device__closeCapture(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Wave_In_Device__closeCapture), Wave_In_Device__closeCapture_body(ctx, a[0]))); }

// ---- Wave_In_Device::setStream (rva 0xed70): converted from c3dt.cpp WiOp1_re
uint32_t Wave_In_Device__setStream_body(Ctx &ctx, uint32_t arg) {
    u32 g = c3dt_glob(ctx, 0x000b4a04);  // 0xed70
    if (!g) return 0x13;  // 0xed76 / 0xed78
    return snd_call(ctx, 0x0000177b, SndAbi::Thiscall, {g, arg}).eax;  // 0xed87
}
void a_Wave_In_Device__setStream(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Wave_In_Device__setStream), Wave_In_Device__setStream_body(ctx, a[0]))); }

// ---- Wave_In_Device::clearStream (rva 0xeda0): converted from c3dt.cpp WiOp2_re
uint32_t Wave_In_Device__clearStream_body(Ctx &ctx) {
    u32 g = c3dt_glob(ctx, 0x000b4a04);  // 0xeda0
    if (!g) return 0x13;  // 0xeda6 / 0xeda8
    return snd_call(ctx, 0x00001253, SndAbi::Thiscall, {g}).eax;  // 0xedb0
}
void a_Wave_In_Device__clearStream(Ctx &ctx, const uint32_t *, Ret &r) { r.set_u32(neg_u32(NEG(Wave_In_Device__clearStream), Wave_In_Device__clearStream_body(ctx))); }

// ---- MidiReader_freeContent (rva 0x1b870): converted from c3do.cpp MidiReader_freeContent_re
// The four bodies 0x0001b570 ChainObject_dtor, 0x0001c990 Ctrl5b58c_dtorEmbedded, 0x0001c080 Ctrl5b58c_dtor and 0x0001b870
// MidiReader_freeContent (VAs 0x1b570 / 0x1c990 / 0x1c080 / 0x1b870) end in the same two loops over a list whose header
// sits in the object: head +0xc, tail +0x10, count +0x18; a node is { prev +0, next +4, value +8 }. Popping the head
// (0x1b5ab-0x1b5c6): next = n_next(ctx, node) (0x1b5b2); when next != NULL, st<u32>(ctx, next, 0 and head = next (0x1b5b9 / 0x1b5bb));
// otherwise tail = head = 0 (0x1b5c0 / 0x1b5c3); value = n_value(ctx, node) (0x1b5c6); operator delete(node) (0x1b5ca); count -= 1
// (0x1b5cf-0x1b5d8).
//   Loop 1 (0x1b5ab-0x1b5e5): pops while the head is non-NULL; a popped NULL value ends the loop (0x1b5d6 / 0x1b5db), a
//     non-NULL value gets its virtual slot 0 called with ecx = value and the argument 1 (`push 1; call [eax]` 0x1b5df / 0x1b5e3).
//   Loop 2 (0x1b5e7-0x1b651, the compiler's copy for the unwind path): pops while the head is non-NULL WITHOUT calling the
//     value; a popped NULL value ends it (0x1b61a / 0x1b61f, 0x1b64c / 0x1b651).
// The count is decremented once per popped node, also when the list ends on a NULL value.
void c3do_drainList(Ctx &ctx, u32 self) {
  // loop 1
    for (;;) {
        u32 node = ld<u32>(ctx, self + 0xc);
        if (!node) break;  // 0x1b5ae / 0x1b5b0
        u32 next = ld<u32>(ctx, node + 4);  // 0x1b5b2
        if (next) {  // 0x1b5b5 / 0x1b5b7
            st<uint32_t>(ctx, next, 0);  // 0x1b5b9
            st<u32>(ctx, self + 0xc, next);  // 0x1b5bb
        } else {
            st<uint32_t>(ctx, self + 0x10, 0);  // 0x1b5c0
            st<uint32_t>(ctx, self + 0xc, 0);  // 0x1b5c3
        }
        u32 value = ld<u32>(ctx, node + 8);  // 0x1b5c6
        snd_delete(ctx, node);  // 0x1b5ca
        st<int32_t>(ctx, self + 0x18, ld<int32_t>(ctx, self + 0x18) - 1);  // 0x1b5cf-0x1b5d8
        if (!value) break;  // 0x1b5d6 / 0x1b5db
        snd_vcall(ctx, value, 0, SndAbi::Thiscall, {1}).eax;  // 0x1b5dd-0x1b5e3
    }
  // loop 2
    for (;;) {
        u32 node = ld<u32>(ctx, self + 0xc);
        if (!node) break;  // 0x1b5f2 / 0x1b5f4, 0x1b624 / 0x1b626
        u32 next = ld<u32>(ctx, node + 4);  // 0x1b5f6
        if (next) {  // 0x1b5f9 / 0x1b5fb
            st<uint32_t>(ctx, next, 0);  // 0x1b5fd
            st<u32>(ctx, self + 0xc, next);  // 0x1b5ff
        } else {
            st<uint32_t>(ctx, self + 0x10, 0);  // 0x1b604
            st<uint32_t>(ctx, self + 0xc, 0);  // 0x1b607
        }
        u32 value = ld<u32>(ctx, node + 8);  // 0x1b60a
        snd_delete(ctx, node);  // 0x1b60e
        st<int32_t>(ctx, self + 0x18, ld<int32_t>(ctx, self + 0x18) - 1);  // 0x1b613-0x1b61c
        if (!value) break;  // 0x1b61a / 0x1b61f
    }
}
void MidiReader_freeContent_body(Ctx &ctx, u32 self) {
    st<u32>(ctx, self, snd_va(ctx, snd_rva::vt_Ctrl5b58c));  // 0x1b88f
    c3do_drainList(ctx, self);
}
void a_MidiReader_freeContent(Ctx &ctx, const uint32_t *a, Ret &) { MidiReader_freeContent_body(ctx, a[0]); neg_mem(ctx, NEG(MidiReader_freeContent), a[0] + 0xc); }

// ---- streamReadByte (rva 0x1c7a0): converted from c3cf.cpp StreamReadByte_re
int32_t streamReadByte_body(Ctx &ctx, u32 self, u32) {
    unsigned pos = ld<uint32_t>(ctx, self + 0x30);  // 0x1c7a3
    const unsigned limit = ld<uint32_t>(ctx, self + 0x3c);  // 0x1c7a6
    if (pos >= limit) {  // jb 0x1c7b4 at 0x1c7ab
        snd_vcall(ctx, self, 0x14, SndAbi::Thiscall, {});  // 0x1c7ad / call at 0x1c7b1
    }
    pos = ld<uint32_t>(ctx, self + 0x30);  // 0x1c7b4
    u32 buf = ld<u32>(ctx, self + 0x2c);  // 0x1c7b7
    const unsigned char b = ld<uint8_t>(ctx, buf + pos);  // 0x1c7ba
    st<uint32_t>(ctx, self + 0x30, pos + 1);  // 0x1c7bd, 0x1c7be
    st<uint8_t>(ctx, self + 0x19, b);  // 0x1c7c3
    return b;  // 0x1c7c1, 0x1c7c6
}
void a_streamReadByte(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_i32(static_cast<int32_t>(neg_u32(NEG(streamReadByte), static_cast<uint32_t>(streamReadByte_body(ctx, a[0], a[1]))))); }

// ---- streamConsumeByte (rva 0x1c7e0): converted from c3cf.cpp StreamConsumeByte_re
void streamConsumeByte_body(Ctx &ctx, u32 self) {
    unsigned pos = ld<uint32_t>(ctx, self + 0x30);  // 0x1c7e3
    const unsigned limit = ld<uint32_t>(ctx, self + 0x3c);  // 0x1c7e6
    if (pos >= limit) {  // jb 0x1c7f4 at 0x1c7eb
        snd_vcall(ctx, self, 0x14, SndAbi::Thiscall, {});  // 0x1c7ed / call at 0x1c7f1
    }
    pos = ld<uint32_t>(ctx, self + 0x30);  // 0x1c7f4
    u32 buf = ld<u32>(ctx, self + 0x2c);  // 0x1c7f7
    const unsigned char b = ld<uint8_t>(ctx, buf + pos);  // 0x1c7fa
    st<uint8_t>(ctx, self + 0x19, b);  // 0x1c7fe
    st<uint32_t>(ctx, self + 0x30, pos + 1);  // 0x1c801
}
void a_streamConsumeByte(Ctx &ctx, const uint32_t *a, Ret &) { streamConsumeByte_body(ctx, a[0]); neg_mem(ctx, NEG(streamConsumeByte), a[0] + 0x30); }

// ---- MidiReader_init (rva 0x1d380): converted from c3fg.cpp MidiInit_re
u32 MidiReader_init_body(Ctx &ctx, u32 self) {
    for (int off = 0x19c; off <= 0x1a8; off += 4) st<uint32_t>(ctx, self + off, 0);  // 0x1d38c-0x1d39e
    st<uint32_t>(ctx, self, 0);  // 0x1d3a9
    for (int off = 4; off < 0x4c; off += 4) st<uint32_t>(ctx, self + off, 0);  // 0x1d3b1
    st<uint32_t>(ctx, self + 0x4c, 0);  // 0x1d3bd
    st<uint32_t>(ctx, self + 0x150, 0);  // 0x1d3bf
    st<uint32_t>(ctx, self + 0x154, 0);  // 0x1d3c1
    st<uint32_t>(ctx, self + 0x158, 0);  // 0x1d3c4
    st<uint32_t>(ctx, self + 0x15c, 0x78);  // 0x1d3ca
    for (int off = 0x160; off <= 0x170; off += 4) st<uint32_t>(ctx, self + off, 0);  // 0x1d3d4-0x1d3ec
    st<uint32_t>(ctx, self + 0x178, 0x10000);  // 0x1d3f2
    st<uint32_t>(ctx, self + 0x17c, 0x10000);  // 0x1d3f8
    u32 timer = ld<u32>(ctx, snd_obj(ctx, &SoundState::g_soundTimer).va());  // 0x1d3fe
    if (timer) {  // 0x1d405
        unsigned period = ld<uint32_t>(ctx, timer + (0) * 4);  // 0x1d407
        if (period >= 1000) st<uint32_t>(ctx, self + 0x174, 0x10000);  // 0x1d40f / 0x1d42a
        else st<uint32_t>(ctx, self + 0x174, 0x10000u / (1000u / period));  // 0x1d418 / 0x1d420
    }
    for (int off = 0x180; off <= 0x190; off += 4) st<uint32_t>(ctx, self + off, 0);  // 0x1d430-0x1d448
    st<uint32_t>(ctx, self + 0x198, 0);  // 0x1d44e
    return self;  // 0x1d454
}
void a_MidiReader_init(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(MidiReader_init), MidiReader_init_body(ctx, a[0]))); }

// ---- MidiReader_dtor (rva 0x1e7d0): converted from c3do.cpp MidiReader_dtor_re
void MidiReader_dtor_body(Ctx &ctx, u32 self) {
    st<u32>(ctx, self, snd_va(ctx, snd_rva::vt_MidiReader));  // 0x1e7ed
    u32 buf = ld<u32>(ctx, self + 0x3c);  // 0x1e7f3
    if (buf && (ld<uint8_t>(ctx, self + 0x34) & 4)) snd_delete(ctx, buf);  // 0x1e7fe-0x1e809
    st<uint32_t>(ctx, self + 0x3c, 0);  // 0x1e813
    snd_call(ctx, 0x0001b870, SndAbi::Thiscall, {self});  // 0x1e822 (thunk 0x18ca)
}
void a_MidiReader_dtor(Ctx &ctx, const uint32_t *a, Ret &) { MidiReader_dtor_body(ctx, a[0]); neg_mem(ctx, NEG(MidiReader_dtor), a[0] + 0x3c); }

// ---- WaveInDeviceBase_ctor (rva 0x2a240): converted from c3ew.cpp WiBaseCtor_re
u32 WaveInDeviceBase_ctor_body(Ctx &ctx, u32 self) {
    st<uint32_t>(ctx, self + 4, 0);  // 0x2a249
    st<uint32_t>(ctx, self + 8, 0x7f);  // 0x2a24b
    st<uint32_t>(ctx, self + 0xc, 0);  // 0x2a252
    st<uint32_t>(ctx, self + 0x18, 0);  // 0x2a255
    st<uint32_t>(ctx, self + 0x1c, 0);  // 0x2a258
    st<uint32_t>(ctx, self + 0x10, 0);  // 0x2a25b
    st<uint32_t>(ctx, self + 0x14, 0);  // 0x2a25e
    st<u32>(ctx, self, snd_va(ctx, snd_rva::vtbl_WaveInDeviceBase));  // 0x2a261
    return self;  // 0x2a240
}
void a_WaveInDeviceBase_ctor(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(WaveInDeviceBase_ctor), WaveInDeviceBase_ctor_body(ctx, a[0]))); }

// ---- WaveInDeviceBase_scalarDtor (rva 0x2a380): converted from c3dj.cpp WaveInDeviceBase_scalarDtor_re
u32 WaveInDeviceBase_scalarDtor_body(Ctx &ctx, u32 self, uint32_t flags) {
    snd_call(ctx, 0x00001e06, SndAbi::Thiscall, {self});
if (flags & 1) snd_delete(ctx, self);
return self;
}
void a_WaveInDeviceBase_scalarDtor(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(WaveInDeviceBase_scalarDtor), WaveInDeviceBase_scalarDtor_body(ctx, a[0], a[1]))); }

// ---- WaveInDeviceBase_openInner (rva 0x2a420): converted from c3cl.cpp WaveInDeviceBaseOpenInner_re
int32_t WaveInDeviceBase_openInner_body(Ctx &ctx, u32 self, int32_t a, int32_t b) {
    int rc = static_cast<int>(snd_vcall(ctx, self, 4, SndAbi::Thiscall, {static_cast<uint32_t>(b)}).eax);  // 0x2a428 / call at 0x2a42b
    if (rc) return rc;  // jne 0x2a456 at 0x2a430
    u32 inner = ld<u32>(ctx, self + 0x14);  // 0x2a432
    int rc2 = static_cast<int>(snd_vcall(ctx, inner, 0x10, SndAbi::Thiscall, {static_cast<uint32_t>(a), static_cast<uint32_t>(b)}).eax);  // 0x2a43b / call at 0x2a43d
    if (!rc2) return 0;  // je 0x2a454 at 0x2a444
    snd_vcall(ctx, self, 8, SndAbi::Thiscall, {});  // 0x2a446 / call at 0x2a44a
    return rc2;  // 0x2a44d
}
void a_WaveInDeviceBase_openInner(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_i32(static_cast<int32_t>(neg_u32(NEG(WaveInDeviceBase_openInner), static_cast<uint32_t>(WaveInDeviceBase_openInner_body(ctx, a[0], a[1], a[2]))))); }

// ---- WaveInDeviceBase_closeInner (rva 0x2a470): converted from c3cl.cpp WaveInDeviceBaseCloseInner_re
int32_t WaveInDeviceBase_closeInner_body(Ctx &ctx, u32 self) {
    u32 inner = ld<u32>(ctx, self + 0x14);  // 0x2a473
    if (inner) {  // je 0x2a48d at 0x2a478
        snd_vcall(ctx, inner, 0x14, SndAbi::Thiscall, {});  // 0x2a47a / call at 0x2a47c
        snd_vcall(ctx, self, 8, SndAbi::Thiscall, {});  // 0x2a47f / call at 0x2a483
        st<u32>(ctx, self + 0x14, 0);  // 0x2a486
    }
    return 0;  // 0x2a48d
}
void a_WaveInDeviceBase_closeInner(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_i32(static_cast<int32_t>(neg_u32(NEG(WaveInDeviceBase_closeInner), static_cast<uint32_t>(WaveInDeviceBase_closeInner_body(ctx, a[0]))))); }

// ---- enqueueWaveCmdE (rva 0x2fb70): converted from c3ex.cpp EnqE_re
u32 c3ex_glob(Ctx &ctx, uint32_t rva) { return ld<u32>(ctx, snd_va(ctx, rva)); }
uint32_t enqueueWaveCmdE_body(Ctx &ctx, u32 self, uint32_t a1, uint32_t, uint32_t a3) {
    if (!c3ex_glob(ctx, 0x000b4a20)) return 0;  // 0x2fb86-0x2fb90
    u32 node = (snd_call(ctx, 0x000424d7, SndAbi::Cdecl, {0x10}).eax);  // 0x2fb94
    if (node)  // 0x2fba0 / 0x2fbaa
        node = (snd_call(ctx, 0x00001299, SndAbi::Thiscall, {node, 0xe, self}).eax);  // 0x2fbb1
    st<uint32_t>(ctx, node + 8, a1);  // 0x2fbc2
    st<uint32_t>(ctx, node + 0xc, a3);  // 0x2fbc5
    return snd_call(ctx, 0x000011c7, SndAbi::Thiscall, {c3ex_glob(ctx, 0x000b4a20), node}).eax;  // 0x2fbc8-0x2fbd7
}
void a_enqueueWaveCmdE(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(enqueueWaveCmdE), enqueueWaveCmdE_body(ctx, a[0], a[1], a[2], a[3]))); }

// ---- enqueueWaveCmd10 (rva 0x2fc10): converted from c3ew.cpp EnqCmd10_re
uint32_t enqueueWaveCmd10_body(Ctx &ctx, u32 self) {
    u32 s = self;
    if (!ld<u32>(ctx, snd_obj(ctx, &SoundState::g_soundEngine).va())) return 1;  // 0x2fc26-0x2fc32
    u32 q = ld<u32>(ctx, s + 0xf8);  // 0x2fc47
    if (q) st<u32>(ctx, q + 0x1414, s + 0xf8);  // 0x2fc4d-0x2fc57
    u32 p = snd_new(ctx, 0x10);  // 0x2fc5d / 0x2fc5f
    unsigned v = p ? snd_call(ctx, 0x00001299, SndAbi::Thiscall, {p, 0x10, c3ew_u(ctx, s)}).eax : 0;  // 0x2fc6b-0x2fc83
    snd_call(ctx, 0x000011c7, SndAbi::Thiscall, {ld<u32>(ctx, snd_obj(ctx, &SoundState::g_soundEngine).va()), v}).eax;  // 0x2fc85-0x2fc94
    return 0;  // 0x2fc9d
}
void a_enqueueWaveCmd10(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(enqueueWaveCmd10), enqueueWaveCmd10_body(ctx, a[0]))); }

// ---- Sound5c450::openMmio (rva 0x30710): converted from c3dt.cpp OpenMmioFwd_re
uint32_t Sound5c450__openMmio_body(Ctx &ctx, u32 self, uint32_t arg) {
    return snd_call(ctx, 0x0000259f, SndAbi::Thiscall, {self, arg}).eax;  // 0x30715
}
void a_Sound5c450__openMmio(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(Sound5c450__openMmio), Sound5c450__openMmio_body(ctx, a[0], a[1]))); }

// ---- openMmioSound (rva 0x30840): hand-fixed conversion of c3fn.cpp OpenMmio_re
u32 c3fn_mmioOpen(Ctx &ctx, u32 n, u32 i, uint32_t f) { return snd_import(ctx, kMmioOpen, SndAbi::Stdcall, {n, i, f}).eax; }
uint32_t c3fn_mmioGetInfo(Ctx &ctx, u32 h, u32 i, uint32_t f) { return snd_import(ctx, kMmioGetInfo, SndAbi::Stdcall, {h, i, f}).eax; }
uint32_t c3fn_u(Ctx &, u32 p) { return p; }
uint32_t c3fn_mmioSetInfo(Ctx &ctx, u32 h, u32 i, uint32_t f) { return snd_import(ctx, kMmioSetInfo, SndAbi::Stdcall, {h, i, f}).eax; }
// Frame (entry esp E; the SEH record, `sub esp, 0x3f8` and three pushes: 0x410 bytes): the MMIOINFO `info` at E-0x3fc (0x30890 `lea edi,
// [esp + 0x14]`), the 54 dwords `bits` at E-0x3b4 (0x308f9 `lea edx, [esp + 0x60]`), the 180 floats `fl` at E-0x2dc (0x30938
// `lea eax, [esp + 0x138]`).
uint32_t openMmioSound_body(Ctx &ctx, u32 self, u32 rp) {
    SndFrame F(ctx, 0x410);
    const u32 info = F.at(0x3fc), bits = F.at(0x3b4), fl = F.at(0x2dc);
    u32 r = rp;
    if (!r) return 0xa;  // 0x30869 / 0x3086b
    if (!ld<u32>(ctx, self + 0xd4)) return 0x13;  // 0x3087d / 0x3087f
    for (int i = 0; i < 0x12; i++) st<uint32_t>(ctx, info + 4 * i, 0);  // 0x30899
    st<uint32_t>(ctx, info + 4, 0x204d454d);  // fccIOProc 'MEM ' 0x3089f
    st<uint32_t>(ctx, info + 0x14, 0x168);  // cchBuffer 0x308aa
    st<uint32_t>(ctx, info + 0x30, 0x64);  // adwInfo[0] 0x308b2
    u32 h = c3fn_mmioOpen(ctx, 0, info, 0x1000);  // 0x308ba
    if (!h) return 8;  // 0x308c6 / 0x308c8
    st<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_openMmioCount).va(), ld<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_openMmioCount).va()) + 1);  // 0x308d3-0x308e2
    c3fn_mmioGetInfo(ctx, h, info, 0);  // 0x308e8
    u32 p = ld<u32>(ctx, r + 0x18);  // 0x308ee
    int32_t cur = 0;
    for (int i = 0; i < 54; i++) {  // 0x308fd-0x30930
        if ((i & 7) == 0) { cur = static_cast<int8_t>(ld<uint8_t>(ctx, p)); p += 1; }  // 0x3090c / 0x3090e (movsx)
        st<uint32_t>(ctx, bits + 4 * i, (cur & (0x80 >> (i & 7))) ? 1u : 0u);  // 0x30912-0x30929
    }
    snd_call(ctx, 0x0003a980, SndAbi::Cdecl, {bits, fl, ld<uint32_t>(ctx, self + 0xd4)}).eax;  // 0x30946
    u32 out = ld<u32>(ctx, info + 0x18);  // pchBuffer, 0x3094b
    const float scale = ld<float>(ctx, snd_va(ctx, 0x5c524));  // 32768.0f
    const double lo = ld<double>(ctx, snd_va(ctx, 0x5c518)), hi = ld<double>(ctx, snd_va(ctx, 0x5c508));  // -32768.0, 32767.0
    for (int i = 0; i < 180; i++) {  // ebx = 0xb4, 0x309a8 / 0x309a9
        const float t = ld<float>(ctx, fl + 4 * i) * scale;  // 0x3095e-0x30966 (exact: scale is a power of two)
        const double d = t;
        // the two compares run on the x87 unit (S4.2b): their C0..C3 are the function's exit state when it is the last one
        const x87::X80 x = x87::fld32(pun<uint32_t>(t));
        const uint16_t c1 = x87::fcom(x, x87::from_f64(pun<uint64_t>(lo)));  // fcomp qword [lo] 0x3096a
        x87::unit(ctx).set_cc(c1);
        if (c1 & 0x0100u) {  // test ah, 1 (C0: below or unordered) 0x30972
            st<int16_t>(ctx, out + (i) * 2, static_cast<int16_t>(0x8000));  // 0x30977
            continue;
        }
        const uint16_t c2 = x87::fcom(x, x87::from_f64(pun<uint64_t>(hi)));  // fcomp qword [hi] 0x30982
        x87::unit(ctx).set_cc(c2);
        if (!(c2 & 0x4100u)) st<int16_t>(ctx, out + (i) * 2, 0x7fff);  // test ah, 0x41 0x3098a / 0x3098f
        else st<int16_t>(ctx, out + (i) * 2, static_cast<int16_t>(static_cast<int32_t>(d)));  // __ftol 0x3099a (truncation; C0..C3 kept)
    }
    st<uint32_t>(ctx, info + 0x1c, ld<uint32_t>(ctx, info + 0x18));  // pchNext 0x309ab / 0x309b3
    st<uint32_t>(ctx, info + 0x20, ld<uint32_t>(ctx, info + 0x18) + 0x168);  // pchEndRead 0x309b9 / 0x309c6
    c3fn_mmioSetInfo(ctx, h, info, 0);  // 0x309ca
    u32 v = snd_new(ctx, 0x110);  // 0x309d5
    if (v) v = snd_call(ctx, 0x00001e56, SndAbi::Thiscall, {v}).eax;  // 0x309e1-0x309f3
    snd_call(ctx, 0x000017a8, SndAbi::Thiscall, {v, info, self + 0xbc}).eax;  // 0x30a12
    u32 tail = ld<u32>(ctx, self + 0x60);  // 0x30a17
    st<u32>(ctx, self + 0x64, tail);  // 0x30a1c
    u32 prev = tail ? n_value(ctx, tail) : 0;  // 0x30a1f-0x30a26
    u32 n = snd_new(ctx, 0xc);  // 0x30a2a
    if (n) { st<u32>(ctx, n, 0); st<u32>(ctx, n + 4, 0); st<u32>(ctx, n + 8, v); }  // 0x30a36-0x30a3b
    if (ld<u32>(ctx, self + 0x60)) {  // 0x30a42 / 0x30a47
        st<u32>(ctx, ld<u32>(ctx, self + 0x60) + 4, n);  // 0x30a49
        st<u32>(ctx, n, ld<u32>(ctx, self + 0x60));  // 0x30a4f
    } else {
        st<u32>(ctx, self + 0x5c, n);  // 0x30a53
    }
    st<u32>(ctx, self + 0x60, n);  // 0x30a5d
    st<uint32_t>(ctx, self + 0x68, ld<uint32_t>(ctx, self + 0x68) + 1);  // 0x30a60-0x30a64
    const unsigned q = udiv32(ld<uint32_t>(ctx, r + 0x20) - ld<uint32_t>(ctx, r + 0x18), ld<uint16_t>(ctx, v + 0xac));  // 0x30a67-0x30a7a
    st<uint32_t>(ctx, v + 0xd4, ld<uint32_t>(ctx, v + 0x20));  // 0x30a7c / 0x30a81
    st<uint32_t>(ctx, v + 0x90, q);  // 0x30a87
    st<uint32_t>(ctx, v + 0xd8, ld<uint32_t>(ctx, v + 0x18));  // 0x30a8d / 0x30a90
    if (prev) {  // 0x30a96
        st<uint32_t>(ctx, prev + 0x98, static_cast<uint32_t>(ld<uint32_t>(ctx, prev + 0x98) | 1));  // 0x30a98-0x30aa5
        st<uint8_t>(ctx, prev + 0x9c, static_cast<uint8_t>(ld<uint8_t>(ctx, prev + 0x9c) | 1));  // 0x30aab-0x30ab3
        st<u32>(ctx, prev + 0xb4, v);  // 0x30ab9
        st<uint32_t>(ctx, prev + 0xcc, 0);  // 0x30abf
    }
    st<uint32_t>(ctx, self + 0xe0, ld<uint32_t>(ctx, self + 0xe0) + 1);  // 0x30ac5-0x30acd
    return 0;  // 0x30ad3
}
void a_openMmioSound(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(openMmioSound), openMmioSound_body(ctx, a[0], a[1]))); }

// ---- streamWaveSamples (rva 0x30ba0): hand-fixed conversion of c3fn.cpp Stream_re
// The front pop of a {head, tail, count} node list (head at h, tail at h + 4, count at h + 0xc), as streamWaveSamples does it
// on this+0x5c three times (0x30be0-0x30c11, 0x30d91-0x30dbe, 0x30e3c-0x30e64): the head's successor
// becomes the head with its prev cleared, or the list empties (tail and head 0); the node is freed with operator delete and
// the count lowered. Returns the node's value. The caller tests for an empty list first.
u32 c3fn_popFront(Ctx &ctx, u32 self) {
    u32 n = ld<u32>(ctx, self + 0x5c);
    u32 next = n_next(ctx, n);
    if (next) {
        st<u32>(ctx, next, 0);
        st<u32>(ctx, self + 0x5c, next);
    } else {
        st<u32>(ctx, self + 0x60, 0);
        st<u32>(ctx, self + 0x5c, 0);
    }
    u32 v = n_value(ctx, n);
    snd_delete(ctx, n);
    st<uint32_t>(ctx, self + 0x68, ld<uint32_t>(ctx, self + 0x68) - 1);
    return v;
}
uint32_t c3fn_mmioClose(Ctx &ctx, u32 h, uint32_t f) { return snd_import(ctx, kMmioClose, SndAbi::Stdcall, {h, f}).eax; }
// The g_openSoundList unlink these bodies share (closeMmioSound 0x35ce1-0x35d42, streamWaveSamples 0x30c29-0x30c8b;
// c3fg's Sound_resetBuffers / parseRiffWave carry the same sequence): the first node whose value is `value` is taken out
// (n_next(ctx, prev) or the head updated; n_prev(ctx, next) updated and the cursor 0xb49d0 set to next, or with no next the cursor = 0
// and the tail 0xb49cc = the node's prev), freed with operator delete, and g_openSoundCount 0xb49d4 lowered. Nothing
// happens when no node matches.
void c3fn_unlinkOpenSound(Ctx &ctx, u32 value) {
    u32 n = ld<u32>(ctx, snd_arr(ctx, &SoundState::g_openSoundList).va());
    while (n && n_value(ctx, n) != value) n = n_next(ctx, n);
    if (!n) return;
    if (n_prev(ctx, n)) st<u32>(ctx, n_prev(ctx, n) + 4, n_next(ctx, n));
    else st<u32>(ctx, snd_arr(ctx, &SoundState::g_openSoundList).va(), n_next(ctx, n));
    if (n_next(ctx, n)) {
        st<u32>(ctx, n_next(ctx, n), n_prev(ctx, n));
        st<u32>(ctx, snd_obj(ctx, &SoundState::g_openSoundListCursor).va(), n_next(ctx, n));
    } else {
        st<u32>(ctx, snd_obj(ctx, &SoundState::g_openSoundListCursor).va(), 0);
        st<u32>(ctx, snd_obj(ctx, &SoundState::g_openSoundListPrevVal).va(), n_prev(ctx, n));
    }
    snd_delete(ctx, n);
    st<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_openSoundCount).va(), ld<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_openSoundCount).va()) - 1);
}
// close and free one stream record as the three drains do (0x30dc3-0x30de4 / 0x30e67-0x30e87)
void c3fn_dropStream(Ctx &ctx, u32 s) {
    c3fn_mmioClose(ctx, ld<u32>(ctx, s + 0x44), 0);
    st<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_openMmioCount).va(), ld<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_openMmioCount).va()) - 1);
    snd_call(ctx, 0x00001b45, SndAbi::Thiscall, {s}).eax;
    snd_delete(ctx, s);
}
// `pos` lives in the argument slot of `size` at entry esp + 0xc (0x30d4d `mov [esp + 0x24], eax`, handed as `lea edx, [esp + 0x2c]`
// at 0x30d5d); the locals at E-8 / E-4 are registers' spills no callee addresses.
uint32_t streamWaveSamples_body(Ctx &ctx, u32 selfp, u32 out, uint32_t n, uint32_t size, u32 written) {
    SndFrame F(ctx, 0x18);
    const u32 posA = F.entry() + 0xc;
    u32 self = selfp;
    if (!ld<u32>(ctx, snd_obj(ctx, &SoundState::g_waveOwner).va())) return 0x13;  // 0x30ba3 / 0x30bb6
    const bool f = size >= 0xc;  // 0x30bc7-0x30bce
    if (ld<uint8_t>(ctx, self + 0x58) & 8) {  // 0x30bd6 / 0x30bda
        while (ld<u32>(ctx, self + 0x5c)) {  // 0x30be5 / 0x30cb6
            u32 s = (c3fn_popFront(ctx, self));  // 0x30beb-0x30c11 / 0x30cb8-0x30cde
            if (!s) break;  // 0x30c14 / 0x30ce1
            if (ld<u32>(ctx, s + 0x44)) {  // 0x30c1a / 0x30c1f
                c3fn_mmioClose(ctx, ld<u32>(ctx, s + 0x44), 0);  // 0x30c23
                c3fn_unlinkOpenSound(ctx, s);  // 0x30c29-0x30c8b
            }
            st<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_openMmioCount).va(), ld<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_openMmioCount).va()) - 1);  // 0x30c90-0x30c99
            snd_call(ctx, 0x00001b45, SndAbi::Thiscall, {s}).eax;  // 0x30ca3
            snd_delete(ctx, s);  // 0x30ca9
        }
        snd_vcall(ctx, self, 0x80, SndAbi::Thiscall, {}).eax;  // 0x30ceb
    }
    u32 head = ld<u32>(ctx, self + 0x5c);  // 0x30cf1
    st<u32>(ctx, self + 0x64, head);  // 0x30cf6
    u32 s = head ? n_value(ctx, head) : 0;  // 0x30cf9-0x30d00
    if (!s) {
        st<u32>(ctx, written, 0);  // 0x30d06
        if (f) vfill(ctx, out, 0, n * 8);  // 0x30d0e-0x30d29
        if (ld<uint8_t>(ctx, self + 0x58) & 0x20) st<uint32_t>(ctx, self + 0x58, static_cast<uint32_t>(ld<uint32_t>(ctx, self + 0x58) & (~1u)));  // 0x30d2e-0x30d34
        return 0x26;  // 0x30d3a
    }
    st<uint32_t>(ctx, posA, ld<uint32_t>(ctx, s + 0x1c));  // 0x30d46 / 0x30d4d
    const unsigned got = snd_call(ctx, f ? 0x00001640 : 0x00001933, SndAbi::Cdecl, {out, posA, n, s, 0}).eax;  // 0x30d66 / 0x30d6d
    st<u32>(ctx, written, got);  // 0x30d7d
    const unsigned drops = ld<uint32_t>(ctx, s + 0xcc);  // 0x30d7f
    if (drops) {  // 0x30d85 / 0x30d8b
        for (unsigned k = drops; k; k--) {  // 0x30df2 / 0x30df7
            if (!ld<u32>(ctx, self + 0x5c)) continue;  // 0x30d94 / 0x30d96
            u32 d = (c3fn_popFront(ctx, self));  // 0x30d98-0x30dbe
            if (d) c3fn_dropStream(ctx, d);  // 0x30dc1-0x30de4
        }
        head = ld<u32>(ctx, self + 0x5c);  // 0x30dfd
        st<u32>(ctx, self + 0x64, head);  // 0x30e02
        s = head ? n_value(ctx, head) : 0;  // 0x30e05 / 0x30e0b
    }
    const unsigned pos = ld<uint32_t>(ctx, posA);
    if (s) {  // 0x30e0e / 0x30e10
        st<uint32_t>(ctx, s + 0x1c, pos);  // 0x30e16
        if (pos >= ld<uint32_t>(ctx, s + 0x20) && !(ld<uint8_t>(ctx, s + 0x9c) & 1)) {  // 0x30e20 / 0x30e29
            st<uint32_t>(ctx, self + 0x58, (ld<uint32_t>(ctx, self + 0x58) & ~0x10u) | 4);  // 0x30e2b-0x30e36
            snd_vcall(ctx, self, 0x20, SndAbi::Thiscall, {}).eax;  // 0x30e39
            if (ld<u32>(ctx, self + 0x5c)) c3fn_popFront(ctx, self);  // 0x30e3c-0x30e64
            c3fn_dropStream(ctx, s);  // 0x30e67-0x30e87
        }
    } else if (ld<uint8_t>(ctx, self + 0x58) & 0x20) {  // 0x30e91-0x30e96
        st<uint32_t>(ctx, self + 0x58, static_cast<uint32_t>(ld<uint32_t>(ctx, self + 0x58) & (~1u)));  // 0x30e98 / 0x30e9a
    }
    if (got != n && f)  // 0x30ea5-0x30ead
        vfill(ctx, out + got * 8, 0, (n - got) * 16);  // 0x30eb3-0x30ec9
    return 0;  // 0x30ece
}
void a_streamWaveSamples(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(streamWaveSamples), streamWaveSamples_body(ctx, a[0], a[1], a[2], a[3], a[4]))); }

// ---- StreamReader_ctor (rva 0x31200): converted from c3do.cpp StreamReaderCtor_re
u32 StreamReader_ctor_body(Ctx &ctx, u32 self) {
    snd_call(ctx, 0x0002f220, SndAbi::Thiscall, {self}).eax;  // 0x31203 -> 0x2f220
    st<uint32_t>(ctx, self + 0x5c, 0);  // 0x3120d
    st<uint32_t>(ctx, self + 0x60, 0);  // 0x31210
    st<uint32_t>(ctx, self + 0x64, 0);  // 0x31213
    st<uint32_t>(ctx, self + 0x68, 0);  // 0x31216
    st<u32>(ctx, self, snd_va(ctx, snd_rva::vtbl_StreamReader));  // 0x31219
    st<uint32_t>(ctx, self + 0x70, 0);  // 0x3121f
    st<uint32_t>(ctx, self + 0x7c, 0);  // 0x31222
    st<uint32_t>(ctx, self + 0x74, 0);  // 0x31224
    st<uint32_t>(ctx, self + 0x90, 0);  // 0x31227
    st<uint32_t>(ctx, self + 0x84, 0);  // 0x3122d
    st<uint32_t>(ctx, self + 0x88, 0);  // 0x31233
    st<uint32_t>(ctx, self + 0x80, 0);  // 0x31239
    st<uint32_t>(ctx, self + 0x8c, 0);  // 0x3123f
    return self;  // 0x31245
}
void a_StreamReader_ctor(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(StreamReader_ctor), StreamReader_ctor_body(ctx, a[0]))); }

// ---- StreamReader_scalarDtor (rva 0x31270): converted from c3dt.cpp StreamRdDel_re
uint32_t StreamReader_scalarDtor_body(Ctx &ctx, u32 self, uint32_t flags) {
    snd_call(ctx, 0x0000108c, SndAbi::Thiscall, {self}).eax;
if (flags & 1) snd_delete(ctx, self);
return self;
}
void a_StreamReader_scalarDtor(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(StreamReader_scalarDtor), StreamReader_scalarDtor_body(ctx, a[0], a[1]))); }

// ---- streamReaderDtor (rva 0x312a0): converted from c3fd.cpp SrDtor_re
// Pops the head node of the list {head at hOff, tail at hOff + 4, count at cOff}: its next n[1] becomes the head with its back
// link cleared, or with no next head and tail are cleared; the node goes to operator delete and the count is decremented. Returns
// the node's payload n[2], read before the delete.
u32 c3fd_popNode(Ctx &ctx, u32 self, int32_t hOff, int32_t cOff, u32 n) {
    const u32 next = ld<u32>(ctx, n + 4);
    if (next) {
        st<u32>(ctx, next, 0);
        st<u32>(ctx, self + hOff, next);
    } else {
        st<u32>(ctx, self + hOff + 4, 0);
        st<u32>(ctx, self + hOff, 0);
    }
    u32 payload = ld<u32>(ctx, n + 8);
    snd_delete(ctx, n);
    { const u32 va_ = self + cOff; st<int32_t>(ctx, va_, ld<int32_t>(ctx, va_) - 1); }
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
void streamReaderDtor_body(Ctx &ctx, u32 self) {
    st<u32>(ctx, self, snd_va(ctx, snd_rva::vtbl_StreamReader));  // 0x312c0
    u32 n = ld<u32>(ctx, self + 0x5c);  // 0x312c6
    if (n) {  // 0x312cb / 0x312d5
        u32 p = c3fd_popNode(ctx, self, 0x5c, 0x68, n);  // 0x312d7-0x312fd
        while (p) {  // 0x312fb / 0x3134f
            snd_import(ctx, 0x000b8670, SndAbi::Stdcall, {ld<uint32_t>(ctx, p), 0}).eax;  // 0x31302-0x3130c
            { const u32 va_ = snd_obj(ctx, &SoundState::g_openMmioCount).va(); st<int32_t>(ctx, va_, ld<int32_t>(ctx, va_) - 1); }  // 0x3130e-0x31316
            snd_delete(ctx, p);  // 0x3131c
            n = ld<u32>(ctx, self + 0x5c);  // 0x31321
            if (!n) break;  // 0x31327 / 0x31329
            p = c3fd_popNode(ctx, self, 0x5c, 0x68, n);  // 0x3132b-0x31351
        }
    }
    u32 wi = c3fd_gref(ctx, kWaveInDevice);  // 0x31356
    if (wi) snd_call(ctx, 0x00001ba4, SndAbi::Thiscall, {wi}).eax;  // 0x3135c-0x31360
    c3fd_drainList(ctx, self, 0x5c, 0x68);  // 0x31365-0x313cb
    snd_call(ctx, 0x00002612, SndAbi::Thiscall, {self}).eax;  // 0x313d7
}
void a_streamReaderDtor(Ctx &ctx, const uint32_t *a, Ret &) { streamReaderDtor_body(ctx, a[0]); neg_mem(ctx, NEG(streamReaderDtor), a[0] + 0x5c); }

// ---- WaveInStream_start (rva 0x31450): converted from c3dt.cpp WiStreamStart_re
uint32_t WaveInStream_start_body(Ctx &ctx, u32 self, uint32_t a1, uint32_t, uint32_t) {
    st<uint32_t>(ctx, self + 0x6c, a1);  // 0x31456
    u32 g = c3dt_glob(ctx, 0x000b4a24);  // 0x31459
    if (!g) return 0x13;  // 0x3145f / 0x31461
    return snd_call(ctx, 0x000015b4, SndAbi::Thiscall, {g, self}).eax;  // 0x3146c
}
void a_WaveInStream_start(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(WaveInStream_start), WaveInStream_start_body(ctx, a[0], a[1], a[2], a[3]))); }

// ---- freeMmioHandleList (rva 0x31490): converted from c3dh.cpp FreeMmioList_re
uint32_t freeMmioHandleList_body(Ctx &ctx, u32 self) {
    u32 node = ld<u32>(ctx, self + 0x5c);  // 0x31498
    if (node) {  // 0x3149d
        u32 elem = c3dh_popFront(ctx, self, 0x5c, 0x60, 0x68, node);  // 0x3149f-0x314c5
        while (elem) {  // 0x314c8 / 0x3151c
            snd_import(ctx, kIatMmioClose, SndAbi::Stdcall, {ld<uint32_t>(ctx, elem), 0}).eax;  // 0x314d4
            st<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_openMmioCount).va(), ld<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_openMmioCount).va()) - 1);  // 0x314d6-0x314de
            snd_delete(ctx, elem);  // 0x314e4
            node = ld<u32>(ctx, self + 0x5c);  // 0x314e9
            if (!node) break;  // 0x314f1
            elem = c3dh_popFront(ctx, self, 0x5c, 0x60, 0x68, node);  // 0x314f3-0x31519
        }
    }
    st<uint32_t>(ctx, self + 0x74, 0);  // 0x3151e
    return 0;  // 0x31524
}
void a_freeMmioHandleList(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(freeMmioHandleList), freeMmioHandleList_body(ctx, a[0]))); }

// ---- freeMmioHandleListGetInfo (rva 0x31560): hand-fixed conversion of c3ew.cpp FreeMmioGI_re
// The MMIOINFO scratch is at entry esp - 0x48 (0x315a9 `lea eax, [esp + 0x10]`).
uint32_t freeMmioHandleListGetInfo_body(Ctx &ctx, u32 self) {
    SndFrame F(ctx, 0x58);
    const u32 info = F.at(0x48);
    u32 s = self;
    unsigned dr;
    u32 node = ld<u32>(ctx, s + 0x5c);  // 0x3156b
    if (node) {  // 0x3156e / 0x31570
        u32 v = (c3ew_popFront(ctx, s, 0x5c, 0x60, 0x68, node, dr));  // 0x31576-0x3159c
        while (v) {  // 0x3159f / 0x31602
            snd_import(ctx, kIatMmioGetInfo, SndAbi::Stdcall, {ld<uint32_t>(ctx, v + (0) * 4), info, 0}).eax;  // 0x315a7-0x315b0
            snd_import(ctx, kIatMmioClose, SndAbi::Stdcall, {ld<uint32_t>(ctx, v + (0) * 4), 0}).eax;  // 0x315b2-0x315b6
            st<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_openMmioCount).va(), static_cast<uint32_t>(ld<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_openMmioCount).va()) - 1));  // 0x315bc-0x315c4
            snd_delete(ctx, v);  // 0x315ca
            node = ld<u32>(ctx, s + 0x5c);  // 0x315cf
            if (!node) break;  // 0x315d5 / 0x315d7
            v = (c3ew_popFront(ctx, s, 0x5c, 0x60, 0x68, node, dr));  // 0x315d9-0x315ff
        }
    }
    u32 wi = ld<u32>(ctx, snd_obj(ctx, &SoundState::g_waveInDevice).va());  // 0x31604
    if (wi) snd_call(ctx, 0x00001ba4, SndAbi::Thiscall, {wi}).eax;  // 0x3160a-0x3160e
    st<uint32_t>(ctx, s + 0x7c, static_cast<uint32_t>(ld<uint32_t>(ctx, s + 0x7c) & 0xfffffffbu));  // 0x31613-0x31619
    return 0;  // 0x3161e
}
void a_freeMmioHandleListGetInfo(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(freeMmioHandleListGetInfo), freeMmioHandleListGetInfo_body(ctx, a[0]))); }

// ---- loadWaveResource (rva 0x316c0): hand-fixed conversion of c3fn.cpp LoadWaveRes_re
// The MMIOINFO is at entry esp - 0x48 (0x316f9 `lea edi, [esp + 0xc]` after three pushes and `sub esp, 0x48`).
uint32_t loadWaveResource_body(Ctx &ctx, u32 self, u32 hdr) {
    SndFrame F(ctx, 0x54);
    const u32 info = F.at(0x48);
    if (!hdr) return 0xa;  // 0x316ce / 0x316d2
    if (ld<uint32_t>(ctx, self + 0x68) > 0x64) return 0x10;  // 0x316de / 0x316e2
    for (int i = 0; i < 0x12; i++) st<uint32_t>(ctx, info + 4 * i, 0);  // 0x316ff
    st<uint32_t>(ctx, info + 4, 0x204d454d);  // fccIOProc 'MEM ' 0x31701
    st<uint32_t>(ctx, info + 0x14, 0x36);  // cchBuffer 0x31709
    st<uint32_t>(ctx, info + 0x30, 0);  // adwInfo[0] 0x31711
    st<uint32_t>(ctx, info + 0x18, 0);  // pchBuffer 0x31715
    u32 cell = snd_new(ctx, 4);  // 0x31719
    u32 h = c3fn_mmioOpen(ctx, 0, info, 0x11002);  // 0x3172f
    st<u32>(ctx, cell, h);  // 0x31737
    if (!h) return 6;  // 0x31739 / 0x3173d
    st<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_openMmioCount).va(), ld<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_openMmioCount).va()) + 1);  // 0x31749-0x31752
    c3fn_mmioGetInfo(ctx, ld<u32>(ctx, cell), info, 0);  // 0x31760
    u32 src = ld<u32>(ctx, hdr);  // 0x31766
    const u32 tab = snd_va(ctx, 0xb515c);
    const float k = ld<float>(ctx, snd_va(ctx, 0x5c5e0));  // 2^-15
    for (int i = 0; i < 180; i++)  // 0x3176d-0x3178c
        st<float>(ctx, tab + (i) * 4, static_cast<float>(static_cast<double>(ld<int16_t>(ctx, src + (i) * 2)) * static_cast<double>(k)));
    snd_call(ctx, 0x0003b150, SndAbi::Cdecl, {tab, snd_va(ctx, 0xb5084), ld<uint32_t>(ctx, self + 0x90)}).eax;  // 0x3179f
    u32 out = ld<u32>(ctx, info + 0x18);  // pchBuffer, 0x317a4
    const u32 bits = snd_va(ctx, 0xb5084);
    unsigned char acc = 0, mask = 0x80;  // 0x317ab / 0x317ad
    for (int i = 0; i < 54; i++) {  // 0xb5084 .. 0xb5158
        if (ld<uint32_t>(ctx, bits + (i) * 4)) acc |= mask;  // 0x317b4 / 0x317b9
        mask >>= 1;  // 0x317bb
        if (mask == 0 || i == 53) {  // 0x317bd / 0x317bf
            st<uint8_t>(ctx, out, acc);  // 0x317c7
            out += 1;
            acc = 0;
            mask = 0x80;
        }
    }
    st<uint32_t>(ctx, self + 0x80, ld<uint32_t>(ctx, self + 0x80) + 1);  // 0x317d9-0x317e6
    st<uint32_t>(ctx, info + 0x20, ld<uint32_t>(ctx, info + 0x18) + 7);  // pchEndRead 0x317ec-0x317f4
    c3fn_mmioSetInfo(ctx, ld<u32>(ctx, cell), info, 0);  // 0x317fb
    snd_call(ctx, 0x00001cb7, SndAbi::Thiscall, {self + 0x5c, cell}).eax;  // 0x31805
    return 0;  // 0x3180c
}
void a_loadWaveResource(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(loadWaveResource), loadWaveResource_body(ctx, a[0], a[1]))); }

// ---- WaveInStream_resetCodec (rva 0x319e0): converted from c3ew.cpp WsResetCodec_re
uint32_t WaveInStream_resetCodec_body(Ctx &ctx, u32 self) {
    unsigned n = ld<uint32_t>(ctx, self + 0x8c);  // 0x319e5
    st<uint32_t>(ctx, self + 0x80, 0);  // 0x319eb
    st<uint32_t>(ctx, self + 0x84, 0);  // 0x319f2
    st<uint32_t>(ctx, self + 0x8c, n + 1);  // 0x319f1 / 0x319f8
    st<uint32_t>(ctx, self + 0x88, 0);  // 0x319fe
    st<uint32_t>(ctx, self + 0x90, snd_call(ctx, 0x0003aa90, SndAbi::Cdecl, {}).eax);  // 0x31a04 / 0x31a09
    snd_call(ctx, 0x000015b4, SndAbi::Thiscall, {ld<u32>(ctx, snd_obj(ctx, &SoundState::g_waveInDevice).va()), c3ew_u(ctx, self)}).eax;  // 0x31a0f-0x31a16
    return 0;  // 0x31a1b
}
void a_WaveInStream_resetCodec(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(WaveInStream_resetCodec), WaveInStream_resetCodec_body(ctx, a[0]))); }

// ---- beginDecodeStreamA (rva 0x32130): hand-written from c3fg.cpp DecA_re / c3fg_beginDecode
// The decode-stream start shared by beginDecodeStreamA and beginDecodeStreamB from the createBufferFmt call on:
// DirectSoundDevice::createBufferFmt (thunk 0x11e5) on the device with (+0x54, the format +0x180, codec, size +0x178); non-zero -> 0x11.
// +0x58 |= 0x10000000. DsBuffer::restore (thunk 0x213f) on the buffer [+0x54] with (0, size, &ptr1, &bytes1, &ptr2, &bytes2): the out
// slots are ptr1 = the argument slot at entry esp + 4 (0x3218a `lea edx, [esp + 0x24]`), bytes1 E-0xc, ptr2 E-4, bytes2 E-8 (0x3216d /
// 0x32177 / 0x3217f); a non-zero result or bytes1 != size ends with +0x220 = 0x5622 and 0. mmioRead(+0, ptr1, bytes1) (0x321b5); a short
// read unlocks (DsBuffer::unlock, thunk 0x1ec9, with (ptr1, bytes1, ptr2, bytes2)), removes the buffer from the device
// (DirectSoundDevice::removeBuffer, thunk 0x1677) and returns 0x23 without the +0x220 store; a full read unlocks the same way, stores this
// at the buffer's +0x20 (0x3220b), then +0x220 = 0x5622 and 0.
unsigned c3fg_beginDecode(Ctx &ctx, u32 self, u32 dev, unsigned codec) {
    SndFrame F(ctx, 0x18);
    const u32 ptr1A = F.entry() + 4, bytes1A = F.at(0xc), ptr2A = F.at(4), bytes2A = F.at(8);
    if (snd_call(ctx, 0x000011e5, SndAbi::Thiscall, {dev, self + 0x54, self + 0x180, codec, ld<uint32_t>(ctx, self + 0x178)}).eax)  // A 0x3215d / B 0x322d1
        return 0x11;  // A 0x32223 / B 0x32397
    st<uint32_t>(ctx, self + 0x58, ld<uint32_t>(ctx, self + 0x58) | 0x10000000);  // A 0x32171 / B 0x322e5
    const u32 buf = ld<u32>(ctx, self + 0x54);
    if (snd_call(ctx, 0x0000213f, SndAbi::Thiscall, {buf, 0, ld<uint32_t>(ctx, self + 0x178), ptr1A, bytes1A, ptr2A, bytes2A}).eax == 0  // A 0x32195 / B 0x32309
        && ld<uint32_t>(ctx, bytes1A) == ld<uint32_t>(ctx, self + 0x178)) {  // A 0x321aa / B 0x3231e
        const unsigned bytes1 = ld<uint32_t>(ctx, bytes1A);
        const unsigned n = snd_import(ctx, kMmioRead, SndAbi::Stdcall, {ld<u32>(ctx, self), ld<uint32_t>(ctx, ptr1A), bytes1}).eax;  // A 0x321b5 / B 0x32329
        if (n != ld<uint32_t>(ctx, bytes1A)) {  // A 0x321c1 / B 0x32335
            snd_call(ctx, 0x00001ec9, SndAbi::Thiscall, {ld<u32>(ctx, self + 0x54), ld<uint32_t>(ctx, ptr1A), ld<uint32_t>(ctx, bytes1A), ld<uint32_t>(ctx, ptr2A), ld<uint32_t>(ctx, bytes2A)});  // A 0x321d5
            snd_call(ctx, 0x00001677, SndAbi::Thiscall, {dev, ld<u32>(ctx, self + 0x54)});  // A 0x321df / B 0x32353
            return 0x23;
        }
        snd_call(ctx, 0x00001ec9, SndAbi::Thiscall, {ld<u32>(ctx, self + 0x54), ld<uint32_t>(ctx, ptr1A), ld<uint32_t>(ctx, bytes1A), ld<uint32_t>(ctx, ptr2A), ld<uint32_t>(ctx, bytes2A)});  // A 0x32204
        st<u32>(ctx, ld<u32>(ctx, self + 0x54) + 0x20, self);  // A 0x3220b / B 0x3237f
    }
    st<uint32_t>(ctx, self + 0x220, 0x5622);  // A 0x3220e / B 0x32382
    return 0;
}
// A NULL device stores +0x220 = 0x5622 and returns 0 (0x3213e -> 0x3220e); otherwise c3fg_beginDecode with the codec id 0x100e0 (0x32154).
uint32_t beginDecodeStreamA_body(Ctx &ctx, u32 self, u32 dev) {
    if (!dev) {  // 0x3213e
        st<uint32_t>(ctx, self + 0x220, 0x5622);  // 0x3220e
        return 0;
    }
    return c3fg_beginDecode(ctx, self, dev, 0x100e0);  // 0x32154
}
void a_beginDecodeStreamA(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(beginDecodeStreamA), beginDecodeStreamA_body(ctx, a[0], a[1]))); }

// ---- beginDecodeStreamB (rva 0x32290): converted from c3fg.cpp DecB_re
uint32_t beginDecodeStreamB_body(Ctx &ctx, u32 self, u32 dev) {
    if (!dev) return 1;  // 0x3229e / 0x323a7
    if (!(ld<uint8_t>(ctx, dev + 0x6c) & 8)) return 0xb;  // 0x322a4 / 0x322ac
    return c3fg_beginDecode(ctx, self, dev, 0x100f4);  // 0x322c8
}
void a_beginDecodeStreamB(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(beginDecodeStreamB), beginDecodeStreamB_body(ctx, a[0], a[1]))); }

// ---- closeDecoder (rva 0x32640): converted from c3dj.cpp CloseDecoder_re
uint32_t closeDecoder_body(Ctx &ctx, u32 self) {
    u32 buf = ld<u32>(ctx, self + 0x54);  // 0x32640
    if (buf != 0) snd_call(ctx, 0x00001a2d, SndAbi::Thiscall, {buf}).eax;  // 0x32643 / 0x32645 / 0x32647
    return 0;  // 0x3264c
}
void a_closeDecoder(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(closeDecoder), closeDecoder_body(ctx, a[0]))); }

// ---- return0xb_32660 (rva 0x32660): hand-written from the instructions
// `mov eax, 0xb; ret` (0x32660 / 0x32665): no argument, returns 0xb (the value of an unsupported-format request).
uint32_t return0xb_32660_body() { return 0xb; }
void a_return0xb_32660(Ctx &, const uint32_t *, Ret &r) { r.set_u32(neg_u32(NEG(return0xb_32660), return0xb_32660_body())); }

// ---- parseRiffWave (rva 0x326a0): hand-written from c3fg.cpp Parse_re
// The unlink shared by Sound_resetBuffers, closeMmioSound and parseRiffWave (0x32b7a-0x32bd8) is c3fn_unlinkOpenSound (closeMmioSound's block).
constexpr uint32_t kStrData = 0x63eb8, kStrCue = 0x63ec0, kStrSmpl = 0x63ec8, kStrFmt = 0x63ed0, kStrWave = 0x63ed8, kStrRiff = 0x63ee0,
                   kStrMP3 = 0x63ee8, kStrMp3 = 0x63eec;
constexpr uint32_t kWaveDeviceG = 0xb4a20, kDsDeviceG = 0xb49f0;
uint32_t c3fg_fourcc(Ctx &ctx, uint32_t strRva) { return snd_import(ctx, kMmioStringToFourcc, SndAbi::Stdcall, {snd_va(ctx, strRva), 0}).eax; }
uint32_t c3fg_descend(Ctx &ctx, u32 h, u32 ck, u32 parent, uint32_t flags) { return snd_import(ctx, kMmioDescend, SndAbi::Stdcall, {h, ck, parent, flags}).eax; }
uint32_t c3fg_ascend(Ctx &ctx, u32 h, u32 ck) { return snd_import(ctx, kMmioAscend, SndAbi::Stdcall, {h, ck, 0}).eax; }
uint32_t c3fg_read(Ctx &ctx, u32 h, u32 p, uint32_t n) { return snd_import(ctx, kMmioRead, SndAbi::Stdcall, {h, p, n}).eax; }
// The close path parseRiffWave takes when a decode stream answers 0 or 0x23 (0x32b66-0x32bec, 0x32c3a-0x32cbc): mmioClose(+0, 0), the
// g_openSoundList node of this+0xc unlinked and freed, g_openMmioCount lowered, +0 = 0.
void c3fg_closeOpenSound(Ctx &ctx, u32 self) {
    snd_import(ctx, kMmioClose, SndAbi::Stdcall, {ld<u32>(ctx, self), 0}).eax;  // 0x32b6b / 0x32c3f
    c3fn_unlinkOpenSound(ctx, self + 0xc);  // 0x32b71-0x32bd8
    st<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_openMmioCount).va(), ld<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_openMmioCount).va()) - 1);  // 0x32bdd-0x32be6
    st<u32>(ctx, self, 0);  // 0x32bec / 0x32cbc
}
// Frame (entry esp E; `sub esp, 0x68` and four pushes = 0x78 bytes): five MMCKINFOs {ckid +0, cksize +4, fccType +8, dwDataOffset +0xc,
// dwFlags +0x10} of 0x14 bytes at E-0x64 .. E (0x326d9-0x32730 zero them), and the argument slot at E+4 that holds the MMIOINFO address
// first (0x32752) and later the cue count the `cue ` chunk reads into (0x329d7 / 0x329e6).
uint32_t parseRiffWave_body(Ctx &ctx, u32 self, u32 name) {
    SndFrame F(ctx, 0x78);
    const u32 riff = F.at(0x64), smpl = F.at(0x50), fmt = F.at(0x3c), cue = F.at(0x28), data = F.at(0x14);
    const u32 slotA = F.entry() + 4;
    if (snd_call(ctx, 0x00042690, SndAbi::Cdecl, {name, snd_va(ctx, kStrMp3)}).eax) return 0xb;  // 0x326b3 / 0x3300e
    if (snd_call(ctx, 0x00042690, SndAbi::Cdecl, {name, snd_va(ctx, kStrMP3)}).eax) return 0xb;  // 0x326c9
    for (int i = 0; i < 25; i++) st<uint32_t>(ctx, riff + 4 * i, 0);  // 0x326d9-0x32730
    if (ld<uint8_t>(ctx, self + 0x58) & 2) {  // 0x3272a
        unsigned r = snd_call(ctx, 0x00001d11, SndAbi::Thiscall, {self}).eax;  // 0x32738
        if (r) return r;  // 0x3273f
    }
    st<u32>(ctx, self, 0);  // 0x3274f
    vfill(ctx, self + 0xc, 0, 0x48);  // 0x32759
    vfill(ctx, self + 0x194, 0, 0x2c);  // 0x32766
    for (int off = 0x164; off <= 0x174; off += 4) st<uint32_t>(ctx, self + off, 0);  // 0x3276d-0x32785
    st<uint32_t>(ctx, self + 0x20, 0x20000);  // 0x3278b
    st<uint32_t>(ctx, self + 0x10, 0x204d454d);  // 0x32792 'MEM '
    vstrcpy(ctx.mem, self + 0x64, name);  // 0x3279a-0x327b2
    u32 h = snd_import(ctx, kMmioOpen, SndAbi::Stdcall, {self + 0x64, 0, 0}).eax;  // 0x327b4
    st<u32>(ctx, self, h);  // 0x327bc
    if (!h) {  // 0x327bf
        vfill(ctx, self + 0xc, 0, 0x48);  // 0x327cc
        return 8;  // 0x327d1
    }
    st<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_openMmioCount).va(), ld<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_openMmioCount).va()) + 1);  // 0x327dd-0x327e8
    st<uint32_t>(ctx, self + 0x58, ld<uint32_t>(ctx, self + 0x58) | 2);  // 0x327f1
    snd_call(ctx, 0x00001bb8, SndAbi::Thiscall, {snd_va(ctx, 0xb49c8), self + 0xc});  // 0x327fc
    st<uint32_t>(ctx, slotA, self + 0xc);  // the argument slot, 0x32752
    st<uint32_t>(ctx, riff, c3fg_fourcc(ctx, kStrRiff));  // 0x3280e
    st<uint32_t>(ctx, riff + 8, c3fg_fourcc(ctx, kStrWave));  // 0x3281b
    if (c3fg_descend(ctx, ld<u32>(ctx, self), riff, 0, 0x20)) {  // 0x32834
        snd_call(ctx, 0x00001aaf, SndAbi::Thiscall, {self}).eax;  // 0x3283c
        return 0x12;
    }
    st<uint32_t>(ctx, fmt, c3fg_fourcc(ctx, kStrFmt));  // 0x32857
    if (c3fg_descend(ctx, ld<u32>(ctx, self), fmt, riff, 0x10)) {  // 0x3286d
        snd_call(ctx, 0x00001aaf, SndAbi::Thiscall, {self}).eax;  // 0x32875
        return 0x12;
    }
    c3fg_read(ctx, ld<u32>(ctx, self), self + 0x180, 0x10);  // 0x3289c
    st<uint16_t>(ctx, self + 0x190, 0);  // 0x328a9
    c3fg_ascend(ctx, ld<u32>(ctx, self), fmt);  // 0x328b2
    st<uint32_t>(ctx, smpl, c3fg_fourcc(ctx, kStrSmpl));  // 0x328bf
    if (c3fg_descend(ctx, ld<u32>(ctx, self), smpl, riff, 0x10) == 0) {  // 0x328d5 / 0x328d9
        c3fg_read(ctx, ld<u32>(ctx, self), self + 0x194, 0x24);  // 0x328e8
        if (ld<uint32_t>(ctx, self + 0x1b0)) {  // 0x328f2
            u32 t = snd_new(ctx, ld<uint32_t>(ctx, self + 0x1b0) * 0x18);  // 0x328fb
            st<u32>(ctx, self + 0x1b8, t);  // 0x32909
            c3fg_read(ctx, ld<u32>(ctx, self), t, ld<uint32_t>(ctx, self + 0x1b0) * 0x18);  // 0x3291b
            const u32 lp = ld<u32>(ctx, self + 0x1b8);
            unsigned ba = ld<uint16_t>(ctx, self + 0x18c);  // 0x32925
            st<uint32_t>(ctx, self + 0x1368, (ld<uint32_t>(ctx, lp + 0xc) - ld<uint32_t>(ctx, lp + 8) + 1) * ba);  // 0x3292c-0x32936
            st<uint32_t>(ctx, self + 0x136c, ba * ld<uint32_t>(ctx, lp + 8));  // 0x3293c / 0x32940
        }
        c3fg_ascend(ctx, ld<u32>(ctx, self), smpl);  // 0x32951
    } else {
        st<uint32_t>(ctx, self + 0x1a0, 0x3c);  // 0x32959
    }
    st<uint32_t>(ctx, riff, c3fg_fourcc(ctx, kStrRiff));  // 0x3296a
    snd_import(ctx, kMmioSeek, SndAbi::Stdcall, {ld<u32>(ctx, self), 0, 0}).eax;  // 0x32978
    st<uint32_t>(ctx, riff + 8, c3fg_fourcc(ctx, kStrWave));  // 0x32985
    if (c3fg_descend(ctx, ld<u32>(ctx, self), riff, 0, 0x20)) {  // 0x32998
        snd_call(ctx, 0x00001aaf, SndAbi::Thiscall, {self}).eax;  // 0x329a0
        return 0x12;
    }
    st<uint32_t>(ctx, cue, c3fg_fourcc(ctx, kStrCue));  // 0x329bb
    if (c3fg_descend(ctx, ld<u32>(ctx, self), cue, riff, 0x10) == 0) {  // 0x329d1 / 0x329d5
        st<uint32_t>(ctx, slotA, 0);  // 0x329d7
        c3fg_read(ctx, ld<u32>(ctx, self), slotA, 4);  // 0x329e6
        unsigned i = 0;  // 0x329ec
        if (ld<uint32_t>(ctx, slotA)) {  // 0x329f6 (jbe)
            do {
                u32 rec = snd_new(ctx, 0x18);  // 0x329fa
                c3fg_read(ctx, ld<u32>(ctx, self), rec, 0x18);  // 0x32a0b
                snd_call(ctx, 0x000012bc, SndAbi::Thiscall, {self + 0x1348, rec});  // 0x32a18
            } while (++i < ld<uint32_t>(ctx, slotA));  // 0x32a25-0x32a2c (jb)
        }
        c3fg_ascend(ctx, ld<u32>(ctx, self), cue);  // 0x32a39
    }
    c3fg_ascend(ctx, ld<u32>(ctx, self), riff);  // 0x32a4b
    st<uint32_t>(ctx, riff, c3fg_fourcc(ctx, kStrRiff));  // 0x32a57
    snd_import(ctx, kMmioSeek, SndAbi::Stdcall, {ld<u32>(ctx, self), 0, 0}).eax;  // 0x32a63
    st<uint32_t>(ctx, riff + 8, c3fg_fourcc(ctx, kStrWave));  // 0x32a6f
    if (c3fg_descend(ctx, ld<u32>(ctx, self), riff, 0, 0x20)) {  // 0x32a81
        snd_call(ctx, 0x00001aaf, SndAbi::Thiscall, {self}).eax;  // 0x32a89
        return 0x12;
    }
    st<uint32_t>(ctx, data, c3fg_fourcc(ctx, kStrData));  // 0x32aa3
    if (c3fg_descend(ctx, ld<u32>(ctx, self), data, riff, 0x10)) {  // 0x32ab9
        snd_call(ctx, 0x00001aaf, SndAbi::Thiscall, {self}).eax;  // 0x32ac1
        return 0x12;
    }
    st<uint32_t>(ctx, self + 0x174, ld<uint32_t>(ctx, data + 0xc));  // 0x32adf
    unsigned cks = ld<uint32_t>(ctx, data + 4);  // 0x32ad9
    st<uint32_t>(ctx, self + 0x178, cks);  // 0x32af0
    unsigned frames = udiv32(cks, ld<uint16_t>(ctx, self + 0x18c));  // 0x32af6
    st<uint32_t>(ctx, self + 0x168, frames);  // 0x32afa
    st<uint32_t>(ctx, self + 0x139c, udiv32(mul32(frames, 1000u), ld<uint32_t>(ctx, self + 0x184)));  // 0x32b00-0x32b12
    if (!ld<u32>(ctx, self + 0x1b8)) {  // 0x32b20
        st<uint32_t>(ctx, self + 0x1368, cks);  // 0x32b22
        st<uint32_t>(ctx, self + 0x136c, 0);  // 0x32b28
    }
    if (ld<uint32_t>(ctx, self + 0x58) & 0x20000000) {  // 0x32b2e
        unsigned r = ld<u32>(ctx, snd_va(ctx, kWaveDeviceG))  // 0x32b3b
            ? snd_call(ctx, 0x00002509, SndAbi::Thiscall, {self, ld<u32>(ctx, snd_va(ctx, kDsDeviceG))}).eax  // 0x32b4c
            : ld<uint32_t>(ctx, slotA);  // 0x32b55
        if (r == 0 || r == 0x23) {  // 0x32b5b / 0x32b60
            c3fg_closeOpenSound(ctx, self);
            if (r == 0) st<uint32_t>(ctx, self + 0x58, ld<uint32_t>(ctx, self + 0x58) | 1);  // 0x32bef-0x32bf6
            return r;  // 0x32bf9
        }
    }
    if (ld<uint8_t>(ctx, self + 0x5c) & 0x20) {  // 0x32c05
        unsigned r = ld<u32>(ctx, snd_va(ctx, kWaveDeviceG))  // 0x32c0f
            ? snd_call(ctx, 0x00001b7c, SndAbi::Thiscall, {self, ld<u32>(ctx, snd_va(ctx, kDsDeviceG))}).eax  // 0x32c20
            : ld<uint32_t>(ctx, slotA);  // 0x32c29
        if (r == 0 || r == 0x23) {  // 0x32c2f / 0x32c34
            c3fg_closeOpenSound(ctx, self);
            return r;  // 0x32cbf
        }
    }
    unsigned fl = ld<uint32_t>(ctx, self + 0x58);  // 0x32ccb
    unsigned bsz = 0x20000;  // 0x32cce
    if ((fl & 1) || ld<uint32_t>(ctx, self + 0x178) < 0x20000) {  // 0x32cd5 / 0x32cdd
        bsz = ld<uint32_t>(ctx, self + 0x178);  // 0x32cdf
        st<uint32_t>(ctx, self + 0x58, fl | 1);  // 0x32ce7
    }
    if (!(ld<uint8_t>(ctx, self + 0x58) & 1)) st<uint32_t>(ctx, self + 0x1374, 0x1ffff);  // 0x32cee / 0x32cf0
    snd_import(ctx, kMmioSetBuffer, SndAbi::Stdcall, {ld<u32>(ctx, self), 0, bsz, 0}).eax;  // 0x32d03
    snd_import(ctx, kMmioGetInfo, SndAbi::Stdcall, {ld<u32>(ctx, self), self + 0xc, 0}).eax;  // 0x32d13
    snd_import(ctx, kMmioAdvance, SndAbi::Stdcall, {ld<u32>(ctx, self), self + 0xc, 0}).eax;  // 0x32d26
    unsigned ba = ld<uint16_t>(ctx, self + 0x18c);
    if ((ld<uint32_t>(ctx, self + 0x168) - ld<uint32_t>(ctx, self + 0x164)) * ba <= ld<uint32_t>(ctx, self + 0x20)) {  // 0x32d45 (ja)
        snd_import(ctx, kMmioAdvance, SndAbi::Stdcall, {ld<u32>(ctx, self), self + 0xc, 0}).eax;  // 0x32d53
        st<uint32_t>(ctx, self + 0x58, ld<uint32_t>(ctx, self + 0x58) | 1);  // 0x32d5e / 0x32d6a
        st<u32>(ctx, self + 8, self + 0xc);  // 0x32d73
        st<uint32_t>(ctx, self + 0x2c, (ld<uint32_t>(ctx, self + 0x168) - ld<uint32_t>(ctx, self + 0x164)) * ld<uint16_t>(ctx, self + 0x18c)
                                           + ld<uint32_t>(ctx, self + 0x24));  // 0x32d6d-0x32d86
        u32 rec = self + 0x2c4;  // 0x32d80
        for (int k = 0; k < 16; k++, rec += 0x110) {  // 0x32d89 / 0x32e75-0x32e7b
            u32 mi = ld<u32>(ctx, self + 8);  // 0x32d8e
            if (mi) rep_movs(ctx.mem, rec - 0x8c, mi, 0x48);  // 0x32d91-0x32da0
            st<uint32_t>(ctx, rec - 4, 0);  // 0x32daa
            st<uint32_t>(ctx, rec, 0);  // 0x32dad
            st<uint32_t>(ctx, rec + 4, 0);  // 0x32daf
            rep_movs(ctx.mem, rec + 0x14, self + 0x180, 0x10);  // 0x32db2-0x32dc9
            st<uint16_t>(ctx, rec + 0x24, ld<uint16_t>(ctx, self + 0x190));  // 0x32dcc / 0x32dd0
            st<uint32_t>(ctx, rec + 4, ld<uint32_t>(ctx, self + 0x168));  // 0x32dda
            st<uint32_t>(ctx, rec, ld<uint32_t>(ctx, self + 0x164));  // 0x32de3
            st<uint32_t>(ctx, rec - 4, 0);  // 0x32de5
            st<uint8_t>(ctx, rec + 0x10, static_cast<uint8_t>((ld<uint8_t>(ctx, rec + 0x10) & 0xfd) | 4));  // 0x32de8-0x32df1
            const u32 lp = ld<u32>(ctx, self + 0x1b8);  // 0x32df4
            unsigned b = ld<uint16_t>(ctx, self + 0x18c);
            unsigned len;
            if (lp) {  // 0x32dfc
                st<uint32_t>(ctx, rec + 0x4c, ld<uint32_t>(ctx, lp + 8) * b + ld<uint32_t>(ctx, ld<u32>(ctx, self + 8) + 0x18));  // 0x32dfe-0x32e17
                st<uint32_t>(ctx, rec + 0x48, (ld<uint32_t>(ctx, lp + 0xc) + 1) * b + ld<uint32_t>(ctx, ld<u32>(ctx, self + 8) + 0x18));  // 0x32e1a-0x32e34
                len = (ld<uint32_t>(ctx, lp + 0xc) - ld<uint32_t>(ctx, lp + 8) + 1) * b;  // 0x32e37-0x32e4f
            } else {
                const u32 m = ld<u32>(ctx, self + 8);
                st<uint32_t>(ctx, rec + 0x4c, ld<uint32_t>(ctx, m + 0x18));  // 0x32e5a
                st<uint32_t>(ctx, rec + 0x48, ld<uint32_t>(ctx, m + 0x20));  // 0x32e63
                len = ld<uint32_t>(ctx, m + 0x20) - ld<uint32_t>(ctx, m + 0x18) + 1;  // 0x32e69-0x32e71
            }
            st<uint32_t>(ctx, rec + 0x50, len);  // 0x32e72
        }
        st<uint32_t>(ctx, self + 0x228, 0);  // 0x32e82
        return 0;
    }
    u32 mi = snd_new(ctx, 0x48);  // 0x32e99
    st<u32>(ctx, self + 8, mi);  // 0x32ea7
    vfill(ctx, mi, 0, 0x48);  // 0x32eaa
    st<uint32_t>(ctx, ld<u32>(ctx, self + 8) + 0x14, bsz);  // 0x32eb0
    st<uint32_t>(ctx, ld<u32>(ctx, self + 8) + 4, 0x204d454d);  // 0x32eb6 'MEM '
    st<uint32_t>(ctx, ld<u32>(ctx, self + 8) + 0x30, 0);  // 0x32ec0
    u32 buf = snd_call(ctx, 0x00042acd, SndAbi::Cdecl, {bsz}).eax;  // 0x32ec7
    st<u32>(ctx, ld<u32>(ctx, self + 8) + 0x18, buf);  // 0x32ed2
    u32 mh = snd_import(ctx, kMmioOpen, SndAbi::Stdcall, {0, ld<u32>(ctx, self + 8), 0x1002}).eax;  // 0x32ee0
    st<u32>(ctx, self + 4, mh);  // 0x32ee8
    if (!mh) {  // 0x32eeb
        snd_call(ctx, 0x00001d11, SndAbi::Thiscall, {self}).eax;  // 0x32eef
        return 8;  // 0x32ef7
    }
    st<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_openMmioCount).va(), ld<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_openMmioCount).va()) + 1);  // 0x32f03-0x32f11
    snd_import(ctx, kMmioGetInfo, SndAbi::Stdcall, {ld<u32>(ctx, self + 4), ld<u32>(ctx, self + 8), 0}).eax;  // 0x32f1e
    snd_call(ctx, 0x00001bb8, SndAbi::Thiscall, {snd_va(ctx, 0xb49c8), ld<u32>(ctx, self + 8)});  // 0x32f29
    st<uint32_t>(ctx, self + 0x1378, ld<uint32_t>(ctx, ld<u32>(ctx, self + 8) + 0x18));  // 0x32f39 / 0x32f3c
    if (ld<u32>(ctx, self + 0x1b8)) {  // 0x32f42
        st<uint32_t>(ctx, self + 0x58, ld<uint32_t>(ctx, self + 0x58) | 4);  // 0x32f4c
        st<uint32_t>(ctx, self + 0x2d0, ld<uint32_t>(ctx, self + 0x2d0) | 1);  // 0x32f56-0x32f65
    }
    snd_call(ctx, 0x00002022, SndAbi::Thiscall, {self}).eax;  // 0x32f69
    snd_import(ctx, kMmioGetInfo, SndAbi::Stdcall, {ld<u32>(ctx, self), self + 0xc, 0}).eax;  // 0x32f78
    snd_import(ctx, kMmioAdvance, SndAbi::Stdcall, {ld<u32>(ctx, self), self + 0xc, 0}).eax;  // 0x32f81
    unsigned f5c = ld<uint32_t>(ctx, self + 0x5c);  // 0x32f87
    if ((f5c & 0x40) && !(f5c & 0x80)) {  // 0x32f8c / 0x32f90
        u32 dev = ld<u32>(ctx, snd_va(ctx, kDsDeviceG));  // 0x32f92
        if (dev) {  // 0x32f9a
            if (snd_call(ctx, 0x000011e5, SndAbi::Thiscall, {dev, self + 0x54, self + 0x180, 0x100e0, 0x10000}).eax == 0) {  // 0x32fb1 / 0x32fb8
                u32 wd = ld<u32>(ctx, snd_va(ctx, kWaveDeviceG));  // 0x32fba
                unsigned v = [&] { SndResidue keep(ctx); return snd_vcall(ctx, wd, 0x40, SndAbi::Thiscall, {8}).eax; }();  // 0x32fc4 (`ret`: the pushed 8 stays)
                snd_call(ctx, 0x0000182f, SndAbi::Thiscall, {ld<u32>(ctx, self + 0x54), v, 8});  // 0x32fc9 / 0x32fca
                st<uint32_t>(ctx, self + 0x58, ld<uint32_t>(ctx, self + 0x58) | 0x10000000);  // 0x32fd3
                st<uint32_t>(ctx, self + 0x228, 0);  // 0x32fd8
                return 0;
            }
            f5c = ld<uint32_t>(ctx, self + 0x5c);  // 0x32ff0
        }
        st<uint32_t>(ctx, self + 0x5c, f5c & ~0x40u);  // 0x32ff3 / 0x32ff5
    }
    st<uint32_t>(ctx, self + 0x228, 0);  // 0x32ff9
    return 0;
}
void a_parseRiffWave(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(parseRiffWave), parseRiffWave_body(ctx, a[0], a[1]))); }

// ---- fillDecodeRing (rva 0x33300): converted from c3fn.cpp FillRing_re
// copy `n1` bytes to `dst1` and, when `dst2` is set and the first part was not cut, `n2` bytes to `dst2`, each cut to the bytes
// the file buffer holds (status 0x2b), pchNext advanced and mmioSetInfo after each part. Returns the parts as copied.
struct c3fn_RingCopy { unsigned n1, n2; bool second, cut; };
c3fn_RingCopy c3fn_ringCopy(Ctx &ctx, u32 self, u32 dst1, uint32_t n1, u32 dst2, uint32_t n2) {
    c3fn_RingCopy r = {n1, n2, false, false};
    unsigned avail = ld<uint32_t>(ctx, self + 0x2c) - ld<uint32_t>(ctx, self + 0x28);
    if (r.n1 > avail) { r.n1 = avail; r.cut = true; }
    rep_movs(ctx.mem, dst1, ld<u32>(ctx, self + 0x28), r.n1);
    st<u32>(ctx, self + 0x28, static_cast<u32>(ld<u32>(ctx, self + 0x28) + r.n1));
    c3fn_mmioSetInfo(ctx, ld<u32>(ctx, self), (self + 0xc), 0);
    if (dst2 && !r.cut) {
        r.second = true;
        avail = ld<uint32_t>(ctx, self + 0x2c) - ld<uint32_t>(ctx, self + 0x28);
        if (r.n2 > avail) { r.n2 = avail; r.cut = true; }
        rep_movs(ctx.mem, dst2, ld<u32>(ctx, self + 0x28), r.n2);
        st<u32>(ctx, self + 0x28, static_cast<u32>(ld<u32>(ctx, self + 0x28) + r.n2));
        c3fn_mmioSetInfo(ctx, ld<u32>(ctx, self), (self + 0xc), 0);
    } else {
        r.n2 = 0;
    }
    return r;
}
uint32_t c3fn_mmioAdvance(Ctx &ctx, u32 h, u32 i, uint32_t f) { return snd_import(ctx, kMmioAdvance, SndAbi::Stdcall, {h, i, f}).eax; }
int32_t c3fn_mmioSeek(Ctx &ctx, u32 h, int32_t o, int32_t w) { return static_cast<int32_t>(snd_import(ctx, kMmioSeek, SndAbi::Stdcall, {h, static_cast<uint32_t>(o), static_cast<uint32_t>(w)}).eax); }
uint32_t fillDecodeRing_body(Ctx &ctx, u32 selfp) {
    u32 self = selfp;
    if (ld<uint8_t>(ctx, self + 0x58) & 0x20) return 0x26;  // 0x3330c / 0x33313
    u32 ring = ld<u32>(ctx, self + 8);  // 0x3331d
    u32 rd = ld<u32>(ctx, ring + 0x1c) - 4;  // 0x33320-0x33326
    u32 start = ld<u32>(ctx, ring + 0x18);  // 0x33323
    if (rd < start) rd += ld<uint32_t>(ctx, ring + 0x14);  // 0x33329-0x3332d
    u32 w = ld<u32>(ctx, self + 0x1378);  // 0x33330
    unsigned space = static_cast<uint32_t>(rd - w) & ld<uint32_t>(ctx, self + 0x1374);  // 0x3333c / 0x3333e
    if (!space) space = ld<uint32_t>(ctx, ring + 0x14);  // 0x33342 / 0x33344
    const unsigned f58 = ld<uint32_t>(ctx, self + 0x58);
    const bool loop = (f58 & 4) != 0;  // 0x33347-0x33351
    const unsigned loopEnd = ld<uint32_t>(ctx, self + 0x136c) + ld<uint32_t>(ctx, self + 0x1368);
    unsigned rem;
    if ((loop && !ld<u32>(ctx, self + 0x1b8) && !ld<uint32_t>(ctx, self + 0x13a0)) ||  // 0x33353-0x33367
        (loop && ld<u32>(ctx, self + 0x1b8) && !(f58 & 0x80)))  // 0x33383-0x33393
        rem = loopEnd - ld<uint32_t>(ctx, self + 0x170);
    else
        rem = ld<uint32_t>(ctx, self + 0x178) - ld<uint32_t>(ctx, self + 0x170);  // 0x333b9-0x333c5
    u32 end = ld<u32>(ctx, ring + 0x24);
    if (rem >= space) {  // 0x333c9 / 0x333cb
        unsigned n1 = space, n2 = 0;
        u32 seg2 = 0;
        if (static_cast<uint32_t>(c3fn_u(ctx, w) + space) > c3fn_u(ctx, end)) {  // 0x333d4-0x333d9
            n1 = static_cast<uint32_t>(end - w);  // 0x333e9
            seg2 = start;  // 0x333eb
            n2 = space - n1;  // 0x333ef
        }
        c3fn_RingCopy c = c3fn_ringCopy(ctx, self, w, n1, seg2, n2);  // 0x333f7-0x3348d
        st<u32>(ctx, self + 0x1378, c.second ? ld<u32>(ctx, ring + 0x18) + c.n2 : w + c.n1);  // 0x33493-0x334b7
        st<uint32_t>(ctx, self + 0x170, static_cast<uint32_t>(ld<uint32_t>(ctx, self + 0x170) + (c.n1 + c.n2)));  // 0x334b1 / 0x334c2
        st<uint32_t>(ctx, ring + 0x20, ld<uint32_t>(ctx, ring + 0x24));  // 0x334c8 / 0x334cd
        if (c.cut) c3fn_mmioAdvance(ctx, ld<u32>(ctx, self), (self + 0xc), 0);  // 0x3365c-0x33665
        st<uint32_t>(ctx, self + 0x58, static_cast<uint32_t>(ld<uint32_t>(ctx, self + 0x58) & (~0x400u)));  // 0x3366b-0x33673
        return 0;
    }
    unsigned n1 = rem, n2 = 0;
    u32 seg2 = 0;
    if (static_cast<uint32_t>(c3fn_u(ctx, w) + rem) > c3fn_u(ctx, end)) {  // 0x334fd-0x33505 / 0x33680-0x33688
        n1 = static_cast<uint32_t>(end - w);  // 0x33515 / 0x33696
        seg2 = start;  // 0x33519 / 0x33698
        n2 = rem - n1;  // 0x33521 / 0x3369c
    }
    c3fn_RingCopy c = c3fn_ringCopy(ctx, self, w, n1, seg2, n2);  // 0x33525-0x335bb / 0x336a2-0x33738
    u32 nw = c.second ? ld<u32>(ctx, ring + 0x18) + c.n2 : w + c.n1;  // 0x335c1-0x335e1 / 0x3373e-0x3375a
    st<u32>(ctx, self + 0x1378, nw);  // 0x335e9 / 0x33762
    st<uint32_t>(ctx, self + 0x170, static_cast<uint32_t>(ld<uint32_t>(ctx, self + 0x170) + (c.n1 + c.n2)));  // 0x335f3 / 0x3376d
    if (loop && !(f58 & 0x80) && !ld<uint32_t>(ctx, self + 0x13a0)) {  // 0x334dd-0x334f7
        if (c.cut) {  // 0x335f9 / 0x33657
            c3fn_mmioAdvance(ctx, ld<u32>(ctx, self), (self + 0xc), 0);  // 0x33665
        } else if (c.n1 + c.n2 == rem) {  // 0x335fb-0x33607
            c3fn_mmioSeek(ctx, ld<u32>(ctx, self), -ld<int32_t>(ctx, self + 0x1368), 1);  // 0x33617
            c3fn_mmioGetInfo(ctx, ld<u32>(ctx, self), (self + 0xc), 0);  // 0x33626
            c3fn_mmioAdvance(ctx, ld<u32>(ctx, self), (self + 0xc), 0);  // 0x33632
            st<uint32_t>(ctx, self + 0x170, ld<uint32_t>(ctx, self + 0x136c));  // 0x33646
        }
        st<uint32_t>(ctx, self + 0x58, static_cast<uint32_t>(ld<uint32_t>(ctx, self + 0x58) & (~0x400u)));  // 0x33641 / 0x3366f
        return 0;
    }
    if (c.cut) c3fn_mmioAdvance(ctx, ld<u32>(ctx, self), (self + 0xc), 0);  // 0x3376a-0x3377e
    st<uint32_t>(ctx, self + 0x58, static_cast<uint32_t>(ld<uint32_t>(ctx, self + 0x58) & (~0x400u)));  // 0x33784-0x3378a
    st<u32>(ctx, ld<u32>(ctx, self + 8) + 0x20, nw);  // 0x3378d / 0x33790
    if (ld<uint32_t>(ctx, self + 0x170) == ld<uint32_t>(ctx, self + 0x178))  // 0x33793-0x3379f
        st<uint32_t>(ctx, self + 0x58, static_cast<uint32_t>(ld<uint32_t>(ctx, self + 0x58) | 0x20));  // 0x337a1-0x337a6
    return 0;  // 0x337ac
}
void a_fillDecodeRing(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(fillDecodeRing), fillDecodeRing_body(ctx, a[0]))); }

// ---- serviceDecodeRing (rva 0x34fb0): converted from c3dt.cpp DecodeRing_re
uint32_t serviceDecodeRing_body(Ctx &ctx, u32 self) {
    unsigned f = ld<uint32_t>(ctx, self + 0x58);  // 0x34fb0
    if ((f & 2) != 0 && (f & 0x21) == 0)  // 0x34fb3 / 0x34fb5 / 0x34fb7 / 0x34fb9
        snd_call(ctx, 0x00002022, SndAbi::Thiscall, {self}).eax;  // 0x34fbb
    return 0;  // 0x34fc0
}
void a_serviceDecodeRing(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(serviceDecodeRing), serviceDecodeRing_body(ctx, a[0]))); }

// ---- closeMmioSound (rva 0x35ca0): converted from c3fn.cpp CloseMmio_re
void c3fn_free(Ctx &ctx, u32 p) { snd_call(ctx, 0x00042c07, SndAbi::Cdecl, {p}); }
// An fld qword / fst qword pair: the value is copied exactly; a signaling NaN (exponent all ones, a non-zero fraction with
// its top bit clear) is quieted by the load (the invalid exception is masked in every control word sound.dll uses).
void c3fn_x87Copy(Ctx &ctx, u32 dst, u32 src) {
    uint64_t v = ld<uint64_t>(ctx, src);
    if ((v & 0x7ff0000000000000ull) == 0x7ff0000000000000ull && (v & 0x000fffffffffffffull) &&
        !(v & 0x0008000000000000ull))
        v |= 0x0008000000000000ull;
    st<uint64_t>(ctx, dst, v);
}
// fld qword; fcomp qword [0.0]; fnstsw; test ah, 0x40: C3 is set for equal (+0.0 or -0.0) and for unordered (a NaN)
// On the x87 unit (S4.2b): the compare's C0..C3 stay set (the function's exit state); the zero is the qword at rva 0x5b498.
uint8_t c3fn_zeroOrNan(Ctx &ctx, u32 p) {
    const uint16_t c = x87::fcom(x87::fld64(ld<uint64_t>(ctx, p)), x87::from_f64(ld<uint64_t>(ctx, snd_va(ctx, 0x5b498))));
    x87::unit(ctx).set_cc(c);
    return (c & 0x4000u) != 0;  // test ah, 0x40 (C3)
}
uint32_t closeMmioSound_body(Ctx &ctx, u32 self) {
    u32 s = ld<u32>(ctx, self + 0x1390);  // 0x35ca7
    u32 v = ld<u32>(ctx, s + 0xb4);  // 0x35cad
    u32 buf = ld<u32>(ctx, s + 0x18);  // 0x35cb3
    c3fn_mmioClose(ctx, ld<u32>(ctx, s + 0x44), 0);  // 0x35cba
    c3fn_free(ctx, buf);  // 0x35cc1
    { const u32 va_ = snd_obj(ctx, &SoundState::g_openMmioCount).va(); st<uint32_t>(ctx, va_, ld<uint32_t>(ctx, va_) - 1); }  // 0x35cc6-0x35cd5
    c3fn_unlinkOpenSound(ctx, ld<u32>(ctx, self + 0x1390));  // 0x35cdb-0x35d42
    if (v) {  // 0x35d47 / 0x35d49
        bool copy = true;
        if (ld<uint8_t>(ctx, self + 0x58) & 8) {  // 0x35d4f-0x35d56
            u32 s2 = ld<u32>(ctx, self + 0x1390);  // 0x35d5c
            unsigned char a = ld<uint8_t>(ctx, v + 0x9c);  // 0x35d62
            st<uint8_t>(ctx, v + 0x9c, static_cast<uint8_t>((a & ~0x10) | (ld<uint8_t>(ctx, s2 + 0x9c) & 0x10)));  // 0x35d68-0x35d75
c3fn_x87Copy(ctx, (v + 0xf0), (ld<u32>(ctx, self + 0x1390) + 0xf0));  // 0x35d81 / 0x35d87
c3fn_x87Copy(ctx, (v + 0xf8), (v + 0xf0));  // 0x35d8d (fstp of the same register)
c3fn_x87Copy(ctx, (v + 0x58), (ld<u32>(ctx, self + 0x1390) + 0x58));  // 0x35d99 / 0x35d9c
c3fn_x87Copy(ctx, (v + 0x60), (v + 0x58));  // 0x35d9f
            snd_call(ctx, 0x0000231a, SndAbi::Thiscall, {v, ld<uint32_t>(ctx, self + 0x224)}).eax;  // 0x35dab
            if (!c3fn_zeroOrNan(ctx, (v + 0x58)) && !c3fn_zeroOrNan(ctx, (v + 0xf0))) {  // 0x35db0-0x35dd1
                st<uint8_t>(ctx, v + 0x9c, static_cast<uint8_t>(ld<uint8_t>(ctx, v + 0x9c) | 8));  // 0x35dd3
            } else {
                st<uint8_t>(ctx, v + 0x9c, static_cast<uint8_t>(ld<uint8_t>(ctx, v + 0x9c) | 4));  // 0x35de7 / 0x35dec
                st<uint8_t>(ctx, v + 0x94, static_cast<uint8_t>(ld<uint8_t>(ctx, v + 0x94) & 0xfe));  // 0x35dea / 0x35df2
                st<uint32_t>(ctx, v + 0xe4, 0);  // 0x35df8
            }
            copy = !(ld<uint8_t>(ctx, self + 0x58) & 8);  // 0x35e02 / 0x35e05
        }
        if (copy) {  // 0x35e0b-0x35e98
            st<uint32_t>(ctx, v + 0x58, ld<uint32_t>(ctx, self + 0x1e0));
            st<uint32_t>(ctx, v + 0x5c, ld<uint32_t>(ctx, self + 0x1e4));
            st<uint32_t>(ctx, v + 0xf0, ld<uint32_t>(ctx, self + 0x1c8));
            st<uint32_t>(ctx, v + 0xf4, ld<uint32_t>(ctx, self + 0x1cc));
            st<uint32_t>(ctx, v + 0x80, ld<uint32_t>(ctx, self + 0x1f0));
            st<uint32_t>(ctx, v + 0x84, ld<uint32_t>(ctx, self + 0x1f4));
            st<uint32_t>(ctx, v + 0x70, ld<uint32_t>(ctx, self + 0x200));
            st<uint32_t>(ctx, v + 0x74, ld<uint32_t>(ctx, self + 0x204));
            st<uint32_t>(ctx, v + 0x78, ld<uint32_t>(ctx, self + 0x1e8));
            st<uint32_t>(ctx, v + 0x68, ld<uint32_t>(ctx, self + 0x1f8));
            st<uint32_t>(ctx, v + 0x48, ld<uint32_t>(ctx, self + 0x224));
            st<uint32_t>(ctx, v + 0xf8, ld<uint32_t>(ctx, self + 0x1c8));
            st<uint32_t>(ctx, v + 0xfc, ld<uint32_t>(ctx, self + 0x1cc));
            st<uint32_t>(ctx, v + 0x50, ld<uint32_t>(ctx, self + 0x1d8));
        }
    }
    if (ld<u32>(ctx, self + 0x1390)) snd_delete(ctx, ld<u32>(ctx, self + 0x1390));  // 0x35e9b-0x35ea6
    st<u32>(ctx, self + 0x1390, v);  // 0x35eb4
    { const u32 va_ = self + 0x1398; st<uint32_t>(ctx, va_, ld<uint32_t>(ctx, va_) - 1); }  // 0x35eae-0x35ebb
    return 0;  // 0x35ec3
}
void a_closeMmioSound(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(closeMmioSound), closeMmioSound_body(ctx, a[0]))); }

// ---- flushMmioBuffer (rva 0x37920): converted from c3fn.cpp Flush_re
void c3fn_sleep(Ctx &ctx, uint32_t ms) { snd_import(ctx, kSleep, SndAbi::Stdcall, {ms}); }
uint32_t c3fn_mmioSetBuffer(Ctx &ctx, u32 h, u32 b, int32_t n, uint32_t f) { return snd_import(ctx, kMmioSetBuffer, SndAbi::Stdcall, {h, b, static_cast<uint32_t>(n), f}).eax; }
uint32_t flushMmioBuffer_body(Ctx &ctx, u32 self) {
    unsigned f = ld<uint32_t>(ctx, self + 0x58);  // 0x37925
    if (!(f & 2)) {  // 0x3792f
        st<uint32_t>(ctx, self + 0x58, f | 1);  // 0x37931 / 0x37934
        return 0;
    }
    if ((f & 0x40) && !(f & 0x10)) {  // 0x3793c / 0x37940
        st<uint32_t>(ctx, self + 0x58, f | 0x10);  // 0x3794b
        for (int k = 0; k < 16; k++) {  // 0x37954-0x37968
            const u32 bp = self + 0x2d4 + 0x110 * k;
            st<uint8_t>(ctx, bp, static_cast<uint8_t>((ld<uint8_t>(ctx, bp) & 0xfd) | 4));
        }
    }
    for (unsigned n = 0x10000;;) {  // edi 0x37928
        f = ld<uint32_t>(ctx, self + 0x58);  // 0x37970
        if (!(f & 0x40) || (f & 0x10)) break;  // 0x37975 / 0x37979
        if (--n == 0) break;  // 0x3797b / 0x3797c
        c3fn_sleep(ctx, 1);  // 0x37980
    }
    f = ld<uint32_t>(ctx, self + 0x58);  // 0x37984
    if ((f & 0x40) && !(f & 0x10)) return 6;  // 0x37989-0x37991
    u32 mi = (self + 0xc);
    c3fn_mmioGetInfo(ctx, ld<u32>(ctx, self), mi, 0);  // 0x379a9
    c3fn_mmioSetBuffer(ctx, ld<u32>(ctx, self), 0, ld<int32_t>(ctx, self + 0x178), 0);  // 0x379b7
    c3fn_mmioGetInfo(ctx, ld<u32>(ctx, self), mi, 0);  // 0x379c2
    c3fn_mmioSeek(ctx, ld<u32>(ctx, self), ld<int32_t>(ctx, self + 0x174), 0);  // 0x379cf
    c3fn_mmioGetInfo(ctx, ld<u32>(ctx, self), mi, 0);  // 0x379da
    c3fn_mmioAdvance(ctx, ld<u32>(ctx, self), mi, 0);  // 0x379e1
    u32 b = ld<u32>(ctx, self + 8);  // 0x379e7
    if (b && b != mi) {  // 0x379ec / 0x379f0
        snd_delete(ctx, b);  // 0x379f3
        st<u32>(ctx, self + 8, mi);  // 0x379fb
    }
    st<uint32_t>(ctx, self + 0x58, static_cast<uint32_t>(ld<uint32_t>(ctx, self + 0x58) | 1));  // 0x37a07 / 0x37a0e
    u32 rec = (self + 0x2c4);  // 0x37a11
    for (int k = 0; k < 16; k++, rec += 0x110) {  // ebp = 0x10, 0x37a89 / 0x37a90
        u32 m = ld<u32>(ctx, self + 8);  // 0x37a17
        if (m) rep_movs(ctx.mem, rec - 0x8c, m, 0x48);  // 0x37a1c-0x37a29
        st<uint32_t>(ctx, rec + -4, 0);  // 0x37a2f
        st<uint32_t>(ctx, rec, 0);  // 0x37a32
        st<uint32_t>(ctx, rec + 4, 0);  // 0x37a34
        rep_movs(ctx.mem, rec + 0x14, (self + 0x180), 0x10);  // 0x37a37-0x37a52
        st<uint16_t>(ctx, rec + 0x24, ld<uint16_t>(ctx, self + 0x190));  // 0x37a55 / 0x37a5b
        st<uint32_t>(ctx, rec + 4, ld<uint32_t>(ctx, self + 0x168));  // 0x37a5f / 0x37a65
        st<uint32_t>(ctx, rec, ld<uint32_t>(ctx, self + 0x164));  // 0x37a6b / 0x37a71
        st<uint32_t>(ctx, rec + -4, 0);  // 0x37a77
        st<int8_t>(ctx, rec + 0x10, static_cast<int8_t>((ld<int8_t>(ctx, rec + 0x10) & 0xfd) | 4));  // 0x37a7d-0x37a8a
    }
    return 0;  // 0x37a95
}
void a_flushMmioBuffer(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(flushMmioBuffer), flushMmioBuffer_body(ctx, a[0]))); }

// ---- MmioBuffer_init (rva 0x37e10): converted from c3au.cpp MmioBufferInit_re
void MmioBuffer_init_body(Ctx &ctx, u32 self, u32 block, u32 fmt) {
    if (block) {
        for (unsigned i = 0; i < 0x12u; ++i) st<uint32_t>(ctx, self + i * 4, ld<uint32_t>(ctx, block + i * 4));
    }
    st<uint32_t>(ctx, self + 0x88, 0);
    st<uint32_t>(ctx, self + 0x8c, 0);
    st<uint32_t>(ctx, self + 0x90, 0);
    for (unsigned i = 0; i < 4u; ++i) st<uint32_t>(ctx, self + 0xa0 + i * 4, ld<uint32_t>(ctx, fmt + i * 4));
    st<uint16_t>(ctx, self + 0xb0, ld<uint16_t>(ctx, fmt + 0x10));
}
void a_MmioBuffer_init(Ctx &ctx, const uint32_t *a, Ret &) { MmioBuffer_init_body(ctx, a[0], a[1], a[2]); neg_mem(ctx, NEG(MmioBuffer_init), a[0] + 0x0); }

// ---- WaveInDevice_ctor (rva 0x38650): converted from c3dh.cpp WaveInCtor_re
constexpr uint32_t kIatWaveInGetDevCapsA = 0xb8620;  // c3dh.cpp
constexpr uint32_t kIatWaveInGetNumDevs = 0xb861c;  // c3dh.cpp
u32 WaveInDevice_ctor_body(Ctx &ctx, u32 self) {
    st<uint32_t>(ctx, self + 0x5c, 0);  // 0x38659
    st<uint32_t>(ctx, self + 0x60, 0);  // 0x3865c
    st<uint32_t>(ctx, self + 0x64, 0);  // 0x3865f
    st<uint32_t>(ctx, self + 0x68, 0);  // 0x38662
    st<u32>(ctx, self, snd_va(ctx, snd_rva::vtbl_WaveInDevice));  // 0x38665
    st<uint32_t>(ctx, self + 4, 0);  // 0x3866b
    unsigned n = snd_import(ctx, kIatWaveInGetNumDevs, SndAbi::Stdcall, {}).eax;  // 0x3866d
    st<uint32_t>(ctx, self + 0x38, n);  // 0x3867b
    st<uint32_t>(ctx, self + 0x40, 0);  // 0x3867e
    snd_import(ctx, kIatWaveInGetDevCapsA, SndAbi::Stdcall, {0xffffffffu, self + 8, 0x30}).eax;  // 0x38681
    st<uint16_t>(ctx, self + 0x58, 0);  // 0x3868c
    st<uint16_t>(ctx, self + 0x48, 1);  // 0x38690
    st<uint16_t>(ctx, self + 0x4a, 1);  // 0x38694
    st<uint32_t>(ctx, self + 0x44, 0);  // 0x38698
    st<uint32_t>(ctx, self + 0x4c, 0x1f40);  // 0x3869b
    st<uint32_t>(ctx, self + 0x50, 0x3e80);  // 0x386a2
    st<uint16_t>(ctx, self + 0x54, 2);  // 0x386a9
    st<uint16_t>(ctx, self + 0x56, 0x10);  // 0x386af
    return self;  // 0x386b5
}
void a_WaveInDevice_ctor(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(WaveInDevice_ctor), WaveInDevice_ctor_body(ctx, a[0]))); }

// ---- WaveInDevice_deleteDtor (rva 0x386e0): converted from c3dt.cpp WaveInDel_re
uint32_t WaveInDevice_deleteDtor_body(Ctx &ctx, u32 self, uint32_t flags) {
    snd_call(ctx, 0x0000113b, SndAbi::Thiscall, {self}).eax;
if (flags & 1) snd_delete(ctx, self);
return self;
}
void a_WaveInDevice_deleteDtor(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(WaveInDevice_deleteDtor), WaveInDevice_deleteDtor_body(ctx, a[0], a[1]))); }

// ---- WaveInDevice_dtor (rva 0x38710): hand-written from c3dh.cpp WaveInDtor_re
// The vtable rva 0x5c604 is stored at +0 (0x38730). Bit 1 of the flag byte at +4 (0x38736 / 0x38747): waveInStop and waveInReset on the
// handle +0x44 (0x38753 / 0x38759) and WaveInDevice_close 0x39140 (thunk 0x24aa, 0x3875d). Then bit 0 of the byte read again (0x38762):
// waveInStop and waveInReset on the handle read again (0x3876c / 0x38772). The buffer list (front +0x5c, back +0x60, count +0x68) is
// emptied: the front node is popped (0x3877e-0x387a2) and released by operator delete (0x38794); a non-null element is destroyed through
// its deleting destructor 0x38ae0 (thunk 0x2469, argument 1, 0x387ad-0x387b1) and the next element comes from the list's pop helper
// 0x39620 (thunk 0x1dcf, ecx = the header, 0x387b8), repeated while the helper returns non-zero (0x387bf). Last, the compiler's inlined
// member-list destruction pops and deletes the remaining nodes without touching their elements, stopping at the first node with a NULL
// element (0x387c1-0x38825).
constexpr uint32_t kWaveInStopSlot = 0xb8614, kWaveInResetSlot = 0xb8618;
void WaveInDevice_dtor_body(Ctx &ctx, u32 self) {
    st<u32>(ctx, self, snd_va(ctx, 0x5c604));  // 0x38730
    unsigned char al = ld<uint8_t>(ctx, self + 4);  // 0x38736
    if ((al & 2) != 0) {  // 0x38747
        snd_import(ctx, kWaveInStopSlot, SndAbi::Stdcall, {ld<uint32_t>(ctx, self + 0x44)}).eax;  // 0x38753
        snd_import(ctx, kWaveInResetSlot, SndAbi::Stdcall, {ld<uint32_t>(ctx, self + 0x44)}).eax;  // 0x38759
        snd_call(ctx, 0x000024aa, SndAbi::Thiscall, {self}).eax;  // 0x3875d -> 0x39140
    }
    if ((ld<uint8_t>(ctx, self + 4) & 1) != 0) {  // 0x38762
        snd_import(ctx, kWaveInStopSlot, SndAbi::Stdcall, {ld<uint32_t>(ctx, self + 0x44)}).eax;  // 0x3876c
        snd_import(ctx, kWaveInResetSlot, SndAbi::Stdcall, {ld<uint32_t>(ctx, self + 0x44)}).eax;  // 0x38772
    }
    const u32 list = self + 0x5c;  // 0x38777
    u32 node = ld<u32>(ctx, list);  // 0x38774
    if (node) {  // 0x3877c
        u32 elem = c3dh_popFront(ctx, self, 0x5c, 0x60, 0x68, node);  // 0x3877e-0x387a2
        while (elem) {  // 0x387a7 / 0x387bf
            snd_call(ctx, 0x00002469, SndAbi::Thiscall, {elem, 1}).eax;  // 0x387b1 -> 0x38ae0
            elem = snd_call(ctx, 0x00001dcf, SndAbi::Thiscall, {list}).eax;  // 0x387b8 -> 0x39620
        }
    }
    node = ld<u32>(ctx, list);  // 0x387c1
    if (node) {  // 0x387cd
        u32 elem = c3dh_popFront(ctx, self, 0x5c, 0x60, 0x68, node);  // 0x387cf-0x387f3
        while (elem) {  // 0x387f6 / 0x38825
            node = ld<u32>(ctx, list);  // 0x387f8
            if (!node) break;  // 0x387fc
            elem = c3dh_popFront(ctx, self, 0x5c, 0x60, 0x68, node);  // 0x387fe-0x38822
        }
    }
}
void a_WaveInDevice_dtor(Ctx &ctx, const uint32_t *a, Ret &) { WaveInDevice_dtor_body(ctx, a[0]); neg_mem(ctx, NEG(WaveInDevice_dtor), a[0] + 0x0); }

// ---- WaveInDevice_allocCaptureBuffers (rva 0x38890): converted from c3dh.cpp AllocCap_re
uint32_t WaveInDevice_allocCaptureBuffers_body(Ctx &ctx, u32 self, uint32_t count, uint32_t size) {
    u32 front = ld<u32>(ctx, self + 0x5c);  // 0x3889b
    st<u32>(ctx, self + 0x64, front);  // 0x388a0
    if (front != 0 && ld<uint32_t>(ctx, front + 8) != 0) return 0x10;  // 0x388a3 / 0x388a5 / 0x388a8 / 0x388ad
    u32 prev = 0;  // ebp
    unsigned i = 0;  // 0x388bb
    if (count != 0) {  // 0x388bf / 0x388c1
        for (;;) {
            u32 hdr = (snd_new(ctx, 0x20));  // 0x388c9
            if (hdr) {  // 0x388d1
                st<uint32_t>(ctx, hdr, 0);  // 0x388d5
                st<uint32_t>(ctx, hdr + 4, 0);  // 0x388d7
                st<uint32_t>(ctx, hdr + 8, 0);  // 0x388da
                st<u32>(ctx, hdr + 0xc, hdr);  // 0x388dd
                st<uint32_t>(ctx, hdr + 0x10, 0);  // 0x388e0
                st<uint32_t>(ctx, hdr + 0x14, 0);  // 0x388e3
                st<uint32_t>(ctx, hdr + 0x18, 0);  // 0x388e6
                st<uint32_t>(ctx, hdr + 0x1c, 0);  // 0x388e9
            }
            if (ld<uint32_t>(ctx, hdr) != 0) return 0x10;  // 0x388f2 / 0x388f4
            if (size == 0) return 0xa;  // 0x388fa / 0x388fc
            st<u32>(ctx, hdr, snd_new(ctx, size));  // 0x388ff / 0x38904
            st<uint32_t>(ctx, hdr + 4, size);  // 0x3890f
            if (prev != 0) st<u32>(ctx, prev + 0x18, hdr);  // 0x3890d / 0x38914
            prev = hdr;  // 0x38919
            u32 node = (snd_new(ctx, 0xc));  // 0x3891b
            if (node) {  // 0x38923
                st<uint32_t>(ctx, node, 0);  // 0x38927
                st<uint32_t>(ctx, node + 4, 0);  // 0x38929
                st<u32>(ctx, node + 8, hdr);  // 0x3892c
            }
            u32 back = ld<u32>(ctx, self + 0x60);  // 0x38933
            if (back) {  // 0x38936
                st<u32>(ctx, back + 4, node);  // 0x3893a
                st<u32>(ctx, node, ld<u32>(ctx, self + 0x60));  // 0x3893d / 0x38940
            } else {
                st<u32>(ctx, self + 0x5c, node);  // 0x38944
            }
            unsigned n = ld<uint32_t>(ctx, self + 0x68) + 1;  // 0x38947 / 0x38955
            st<u32>(ctx, self + 0x60, node);  // 0x3894e
            i = i + 1;  // 0x38956
            st<uint32_t>(ctx, self + 0x68, n);  // 0x38959
            if (i >= count) break;  // 0x38957 / 0x38960
        }
    }
    u32 lastNode = ld<u32>(ctx, self + 0x60);  // 0x38974
    st<u32>(ctx, self + 0x64, lastNode);  // 0x38979
    u32 lastHdr = lastNode ? ld<u32>(ctx, lastNode + 8) : 0;  // 0x3897e / 0x38983
    u32 firstNode = ld<u32>(ctx, self + 0x5c);  // 0x38985
    st<u32>(ctx, self + 0x64, firstNode);  // 0x3898a
    u32 firstHdr = firstNode ? ld<u32>(ctx, firstNode + 8) : 0;  // 0x3898f / 0x389a2
    st<u32>(ctx, lastHdr + 0x18, firstHdr);  // 0x38994 / 0x389a4
    return 0;  // 0x38998
}
void a_WaveInDevice_allocCaptureBuffers(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(WaveInDevice_allocCaptureBuffers), WaveInDevice_allocCaptureBuffers_body(ctx, a[0], a[1], a[2]))); }

// ---- WaveInDevice_start (rva 0x38b20): converted from c3dt.cpp WiStart_re
constexpr uint32_t kIatWaveInStart = 0xb8610;  // c3dt.cpp
uint32_t WaveInDevice_start_body(Ctx &ctx, u32 self) {
    snd_import(ctx, kIatWaveInStart, SndAbi::Stdcall, {ld<uint32_t>(ctx, self + 0x44)}).eax;  // 0x38b24
    return 0;  // 0x38b2a
}
void a_WaveInDevice_start(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(WaveInDevice_start), WaveInDevice_start_body(ctx, a[0]))); }

// ---- WaveInDevice_prepareHeaders (rva 0x38b40): converted from c3dt.cpp WiPrep_re
constexpr uint32_t kIatWaveInPrepareHeader = 0xb860c;  // c3dt.cpp
// The winmm MMRESULT -> game error code mapping shared by waveInOpen's callers (0x38bac / 0x38e0c / 0x3908a ...): the code
// minus 2 above 0x1f gives 0xb; otherwise a byte table (0x38c08 / 0x38e68 / 0x390ec, identical) picks one of seven
// jump-table targets (0x38bec / 0x38e4c / 0x390d0), which return 0xa, 0xc, 1, 0x11, 0x12, 0xe, 0xb.
uint32_t c3dt_waveInErr(Ctx &, uint32_t r) {
    static const unsigned char idx[0x20] = {0, 6, 1, 0, 2, 3, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 4, 5};
    static const unsigned val[7] = {0xa, 0xc, 1, 0x11, 0x12, 0xe, 0xb};
    unsigned e = r - 2;  // add eax, -2
    if (e > 0x1f) return 0xb;  // cmp eax, 0x1f; ja
    return val[idx[e]];
}
// 0x00038b40  WaveInDevice_prepareHeaders (VA 0x38b40) and 0x00038da0  WaveInDevice_addBuffer (VA 0x38da0) share one shape and
// differ in the import: waveInPrepareHeader [0xb860c] (edi at 0x38b6c) and waveInAddBuffer [0xb8604] (0x38dcc). __thiscall,
// `ret`. The buffer list head at this+0x5c is copied to the cursor this+0x64 (0x38b44 / 0x38b49); no head, or a head whose
// header pointer (+8) is NULL, returns 0x13 (0x38b4c / 0x38b53 / 0x38b56), and so does a zero handle at +0x44 (0x38b5d /
// 0x38b62 / 0x38b65). Then, for each node, import(handle, header, 0x20) (0x38b72-0x38b79): a non-zero result is mapped
// by c3dt_waveInErr (0x38b9c-0x38be4) and returned; zero advances the cursor through the node's +4 link (0x38b7f-0x38b8e)
// until the link or the next header pointer (+8) is NULL (0x38b84 / 0x38b89 / 0x38b8e / 0x38b95) and returns 0.
uint32_t c3dt_wiLoop(Ctx &ctx, u32 self, uint32_t slotRva) {
    u32 node = ld<u32>(ctx, self + 0x5c);  // 0x38b44
    st<u32>(ctx, self + 0x64, node);  // 0x38b49
    if (!node) return 0x13;  // 0x38b47 / 0x38b4c
    u32 hdr = ld<u32>(ctx, node + 8);  // 0x38b4e
    if (!hdr) return 0x13;  // 0x38b51 / 0x38b53
    if (ld<uint32_t>(ctx, self + 0x44) == 0) return 0x13;  // 0x38b5d / 0x38b60 / 0x38b62
    for (;;) {
        unsigned r = snd_import(ctx, slotRva, SndAbi::Stdcall, {ld<uint32_t>(ctx, self + 0x44), hdr, 0x20}).eax;  // 0x38b79
        if (r != 0) return c3dt_waveInErr(ctx, r);  // 0x38b7b / 0x38b7d
        u32 cur = ld<u32>(ctx, self + 0x64);  // 0x38b7f
        if (!cur) break;  // 0x38b82 / 0x38b84
        cur = ld<u32>(ctx, cur + 4);  // 0x38b86
        st<u32>(ctx, self + 0x64, cur);  // 0x38b8b
        if (!cur) break;  // 0x38b89 / 0x38b8e
        hdr = ld<u32>(ctx, cur + 8);  // 0x38b90
        if (!hdr) break;  // 0x38b93 / 0x38b95
    }
    return 0;  // 0x38b98
}
uint32_t WaveInDevice_prepareHeaders_body(Ctx &ctx, u32 self) { return c3dt_wiLoop(ctx, self, kIatWaveInPrepareHeader); }
void a_WaveInDevice_prepareHeaders(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(WaveInDevice_prepareHeaders), WaveInDevice_prepareHeaders_body(ctx, a[0]))); }

// ---- WaveInDevice_addBuffer (rva 0x38da0): converted from c3dt.cpp WiAdd_re
constexpr uint32_t kIatWaveInAddBuffer = 0xb8604;  // c3dt.cpp
uint32_t WaveInDevice_addBuffer_body(Ctx &ctx, u32 self) { return c3dt_wiLoop(ctx, self, kIatWaveInAddBuffer); }
void a_WaveInDevice_addBuffer(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(WaveInDevice_addBuffer), WaveInDevice_addBuffer_body(ctx, a[0]))); }

// ---- WaveInDevice_reset (rva 0x38f10): converted from c3dt.cpp WiReset_re
constexpr uint32_t kIatWaveInReset = 0xb8618;  // c3dt.cpp
constexpr uint32_t kIatWaveInStop = 0xb8614;  // c3dt.cpp
uint32_t WaveInDevice_reset_body(Ctx &ctx, u32 self) {
    snd_import(ctx, kIatWaveInStop, SndAbi::Stdcall, {ld<uint32_t>(ctx, self + 0x44)}).eax;  // 0x38f17
    if (NEG(WaveInDevice_reset) != 2) snd_import(ctx, kIatWaveInReset, SndAbi::Stdcall, {ld<uint32_t>(ctx, self + 0x44)}).eax;  // 0x38f21 (mode 2: skipped)
    return 0;  // 0x38f27
}
void a_WaveInDevice_reset(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(WaveInDevice_reset), WaveInDevice_reset_body(ctx, a[0]))); }

// ---- onWaveInCallbackMessage (rva 0x38fd0): converted from c3dh.cpp WaveInCb_re
void onWaveInCallbackMessage_body(Ctx &ctx, uint32_t a1, uint32_t msg, uint32_t inst, uint32_t p1, uint32_t p2) {
    (void)a1; (void)p2;
    if ((msg & 0xffff) != 0x3c0) return;  // 0x38fd0 / 0x38fd7
    u32 eng = ld<u32>(ctx, snd_obj(ctx, &SoundState::g_soundEngine).va());  // 0x38fd9
    if (eng != 0 && ((ld<uint32_t>(ctx, eng + 0x1a4) >> 5) & 1) != 0) {  // 0x38fde / 0x38fe2 / 0x38feb
        snd_import(ctx, kIatPostMessageA, SndAbi::Stdcall, {ld<uint32_t>(ctx, snd_obj(ctx, &SoundState::g_soundWindow).va()), 0x7f0, p1, inst}).eax;  // 0x39004
        return;  // 0x3900a
    }
    snd_call(ctx, 0x0002aa90, SndAbi::Thiscall, {snd_arr(ctx, &SoundState::g_soundThreadState).va(), 0x7f0, p1, inst}).eax;  // 0x39021
}
void a_onWaveInCallbackMessage(Ctx &ctx, const uint32_t *a, Ret &) { onWaveInCallbackMessage_body(ctx, a[0], a[1], a[2], a[3], a[4]); neg_mem(ctx, NEG(onWaveInCallbackMessage), a[0] + 0x0); }

// ---- WaveInDevice_open (rva 0x39050): hand-written from c3dt.cpp WiOpen_re
// A non-zero handle at +0x44 returns 0xc (0x39053-0x3905d). Otherwise waveInOpen [0xb8600] (&this[+0x44], -1, &this[+0x48], callback
// thunk 0x179e, this, 0x30000) (0x39064-0x39076); a zero result sets bit 1 of the flag dword at +4 (0x390c1-0x390c6) and returns 0
// (0x390c9); a non-zero one goes through the mapping of the winmm result (0x39080-0x390ba, the table at 0x390ec / 0x390d0).
constexpr uint32_t kWaveInOpenSlot = 0xb8600;
uint32_t WaveInDevice_open_body(Ctx &ctx, u32 self) {
    if (ld<uint32_t>(ctx, self + 0x44) != 0) return 0xc;  // 0x39053 / 0x3905b / 0x3905d
    unsigned r = snd_import(ctx, kWaveInOpenSlot, SndAbi::Stdcall, {self + 0x44, 0xffffffffu, self + 0x48, snd_va(ctx, 0x179e), self, 0x30000}).eax;  // 0x39076
    if (r == 0) {  // 0x3907c / 0x3907e
        st<uint32_t>(ctx, self + 4, ld<uint32_t>(ctx, self + 4) | 2);  // 0x390c1 / 0x390c4 / 0x390c6
        return 0;  // 0x390c9
    }
    return c3dt_waveInErr(ctx, r);  // 0x39080
}
void a_WaveInDevice_open(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(WaveInDevice_open), WaveInDevice_open_body(ctx, a[0]))); }

// ---- WaveInDevice_close (rva 0x39140): converted from c3dh.cpp WaveInClose_re
constexpr uint32_t kIatEnterCriticalSection = 0xb83f4;  // c3dh.cpp
constexpr uint32_t kIatLeaveCriticalSection = 0xb84e0;  // c3dh.cpp
constexpr uint32_t kIatWaveInClose = 0xb85fc;  // c3dh.cpp
constexpr uint32_t kIatWaveInUnprepareHeader = 0xb8608;  // c3dh.cpp
uint32_t c3dh_waveInErr(Ctx &, uint32_t r) {
    static const unsigned char idx[0x20] = {0, 6, 1, 0, 2, 3, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 4, 5};  // 0x39248
    static const unsigned val[7] = {0xa, 0xc, 1, 0x11, 0x12, 0xe, 0xb};  // jump table 0x3922c: 0x391e2, 0x391f7, 0x391e9, 0x391fe, 0x39205, 0x391f0, 0x3920c
    unsigned e = r - 2;  // 0x391cb
    if (e > 0x1f) return 0xb;  // 0x391ce / 0x391d1 / 0x3920c
    return val[idx[e]];  // 0x391d5 / 0x391db
}
uint32_t WaveInDevice_close_body(Ctx &ctx, u32 self) {
    if (ld<uint32_t>(ctx, self + 0x44) == 0) return 0x13;  // 0x39143 / 0x39146 / 0x3914a
    u32 cs = snd_arr(ctx, &SoundState::g_soundThreadCrit).va();
    snd_import(ctx, kIatEnterCriticalSection, SndAbi::Stdcall, {cs}).eax;  // 0x39157
    unsigned f = ld<uint32_t>(ctx, self + 4);  // 0x3915d
    f = (f & ~0xffu) | static_cast<uint8_t>(f & 0xfd);  // 0x39160
    st<uint32_t>(ctx, self + 4, f);  // 0x39162
    snd_import(ctx, kIatWaveInStop, SndAbi::Stdcall, {ld<uint32_t>(ctx, self + 0x44)}).eax;  // 0x39169
    snd_import(ctx, kIatWaveInReset, SndAbi::Stdcall, {ld<uint32_t>(ctx, self + 0x44)}).eax;  // 0x39173
    u32 node = ld<u32>(ctx, self + 0x5c);  // 0x39179
    st<u32>(ctx, self + 0x64, node);  // 0x3917e
    if (node) {  // 0x3917c
        u32 hdr = ld<u32>(ctx, node + 8);  // 0x39183
        if (hdr && ld<uint32_t>(ctx, self + 0x44) != 0) {  // 0x39186 / 0x3918d
            for (;;) {
                if (snd_import(ctx, kIatWaveInUnprepareHeader, SndAbi::Stdcall, {ld<uint32_t>(ctx, self + 0x44), hdr, 0x20}).eax != 0) break;  // 0x3919e / 0x391a2
                u32 cur = ld<u32>(ctx, self + 0x64);  // 0x391a4
                if (!cur) break;  // 0x391a7
                cur = ld<u32>(ctx, cur + 4);  // 0x391ab
                st<u32>(ctx, self + 0x64, cur);  // 0x391b0
                if (!cur) break;  // 0x391b3
                hdr = ld<u32>(ctx, cur + 8);  // 0x391b5
                if (!hdr) break;  // 0x391ba
            }
        }
    }
    unsigned r = snd_import(ctx, kIatWaveInClose, SndAbi::Stdcall, {ld<uint32_t>(ctx, self + 0x44)}).eax;  // 0x391c0
    if (r != 0) return c3dh_waveInErr(ctx, r);  // 0x391c6 / 0x391c9
    snd_import(ctx, kIatLeaveCriticalSection, SndAbi::Stdcall, {cs}).eax;  // 0x39218
    st<uint32_t>(ctx, self + 0x44, 0);  // 0x3921e
    return 0;  // 0x39225
}
void a_WaveInDevice_close(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(WaveInDevice_close), WaveInDevice_close_body(ctx, a[0]))); }

// ---- WaveInDevice_requeueBuffer (rva 0x393d0): converted from c3dt.cpp WiRequeue_re
uint32_t WaveInDevice_requeueBuffer_body(Ctx &ctx, u32 self, u32 hdr) {
    if (!hdr) return 0xa;  // 0x393d8 / 0x393da
    snd_import(ctx, kIatWaveInUnprepareHeader, SndAbi::Stdcall, {ld<uint32_t>(ctx, self + 0x44), hdr, 0x20}).eax;  // 0x393ed
    st<uint32_t>(ctx, hdr + 0x10, 0);  // 0x393f3
    u32 sub = ld<u32>(ctx, self + 0x6c);  // 0x393fa
    if (sub) snd_call(ctx, 0x0000176c, SndAbi::Thiscall, {sub, hdr}).eax;  // 0x393fd / 0x39402
    snd_import(ctx, kIatWaveInPrepareHeader, SndAbi::Stdcall, {ld<uint32_t>(ctx, self + 0x44), hdr, 0x20}).eax;  // 0x3940e
    snd_import(ctx, kIatWaveInAddBuffer, SndAbi::Stdcall, {ld<uint32_t>(ctx, self + 0x44), hdr, 0x20}).eax;  // 0x3941b
    return 0;  // 0x39422
}
void a_WaveInDevice_requeueBuffer(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(WaveInDevice_requeueBuffer), WaveInDevice_requeueBuffer_body(ctx, a[0], a[1]))); }

// ---- loadMultiChannel (rva 0x399a0): converted from c3fg.cpp LoadMulti_re
uint32_t loadMultiChannel_body(Ctx &ctx, u32 self, u32 name) {
    unsigned r = snd_call(ctx, 0x00001e51, SndAbi::Thiscall, {(self + 0x70), c3fg_u(ctx, name)}).eax;  // 0x399ca
    if (r) return r;  // 0x399d3
    int n = ld<int32_t>(ctx, self + 0x1438);  // 0x399d9
    if (n <= 1 || n >= 3) return 0;  // 0x399e2 / 0x399eb
    u32 slot = self + 0x1440;  // 0x399fe
    for (int i = 1; i < ld<int32_t>(ctx, self + 0x1438); i++, slot += 4) {  // 0x39aae-0x39ab8
        u32 p = (snd_new(ctx, 0x1450));  // 0x39a09
        if (p) {  // 0x39a1d
            snd_call(ctx, 0x00001465, SndAbi::Thiscall, {p}).eax;  // 0x39a21
            snd_call(ctx, 0x00001005, SndAbi::Thiscall, {p + 0x70}).eax;  // 0x39a2e
            st<u32>(ctx, p, snd_va(ctx, 0x5c7e8));  // 0x39a3b
            st<uint32_t>(ctx, p + 0x1424, 0);  // 0x39a43
            st<uint32_t>(ctx, p + 0x1428, 0);  // 0x39a4c
            st<uint32_t>(ctx, p + 0x1420, 0);  // 0x39a55
            st<uint32_t>(ctx, p + 0x142c, 0);  // 0x39a5b
            st<uint32_t>(ctx, p + 0x1438, 1);  // 0x39a61
            st<uint32_t>(ctx, p + 0x143c, 0);  // 0x39a6b
            st<uint32_t>(ctx, p + 0x1440, 0);  // 0x39a71
            st<uint32_t>(ctx, p + 0x1444, 0);  // 0x39a73
            st<uint32_t>(ctx, p + 0x1448, 0);  // 0x39a76
            st<uint32_t>(ctx, p + 0x1430, 0);  // 0x39a7b
            st<uint32_t>(ctx, p + 0x1434, 0);  // 0x39a81
            snd_call(ctx, 0x000018a2, SndAbi::Thiscall, {p, 1}).eax;  // 0x39a87
        }
        r = snd_vcall(ctx, p, 0x10, SndAbi::Thiscall, {c3fg_u(ctx, name)}).eax;  // 0x39aa1
        if (r) return r;  // 0x39aa6
        st<u32>(ctx, slot, p);  // 0x39aac
    }
    return r;
}
void a_loadMultiChannel(Ctx &ctx, const uint32_t *a, Ret &r) { r.set_u32(neg_u32(NEG(loadMultiChannel), loadMultiChannel_body(ctx, a[0], a[1]))); }


const FuncDef kFuncs[] = {
    {"snd.Wave_Device::readFmtChunk", "thiscall", "uint32", 1, 0, a_Wave_Device__readFmtChunk, &g_negctl_Wave_Device__readFmtChunk},
    {"snd.processWaveChunks", "thiscall", "uint32", 1, 0, a_processWaveChunks, &g_negctl_processWaveChunks},
    {"snd.WaveStream::ctor", "thiscall", "pointer", 1, 0, a_WaveStream__ctor, &g_negctl_WaveStream__ctor},
    {"snd.WaveStream::scalarDeletingDtor", "thiscall", "uint32", 2, 0, a_WaveStream__scalarDeletingDtor, &g_negctl_WaveStream__scalarDeletingDtor},
    {"snd.WaveStream::destroy", "thiscall", "void", 1, 0, a_WaveStream__destroy, &g_negctl_WaveStream__destroy},
    {"snd.closeMmioNodeList", "thiscall", "uint32", 1, 0, a_closeMmioNodeList, &g_negctl_closeMmioNodeList},
    {"snd.Wave_Device::openMemoryMmio", "thiscall", "uint32", 2, 0, a_Wave_Device__openMemoryMmio, &g_negctl_Wave_Device__openMemoryMmio},
    {"snd.Wave_Device::freeBufferList", "thiscall", "uint32", 5, 0, a_Wave_Device__freeBufferList, &g_negctl_Wave_Device__freeBufferList},
    {"snd.Wave_Device::appendBuffer", "thiscall", "uint32", 2, 0, a_Wave_Device__appendBuffer, &g_negctl_Wave_Device__appendBuffer},
    {"snd.startStream", "thiscall", "int", 2, 0, a_startStream, &g_negctl_startStream},
    {"snd.computeCaptureDuration", "thiscall", "void", 1, 0, a_computeCaptureDuration, &g_negctl_computeCaptureDuration},
    {"snd.initStreamBuffer", "thiscall", "uint32", 1, 0, a_initStreamBuffer, &g_negctl_initStreamBuffer},
    {"snd.writeWaveFile", "thiscall", "uint32", 2, 0, a_writeWaveFile, &g_negctl_writeWaveFile},
    {"snd.writeWaveFileB", "thiscall", "uint32", 3, 0, a_writeWaveFileB, &g_negctl_writeWaveFileB},
    {"snd.writeWaveChunk", "thiscall", "uint32", 5, 0, a_writeWaveChunk, &g_negctl_writeWaveChunk},
    {"snd.writeWaveChunkB", "thiscall", "uint32", 7, 0, a_writeWaveChunkB, &g_negctl_writeWaveChunkB},
    {"snd.openMmioFile", "thiscall", "uint32", 2, 0, a_openMmioFile, &g_negctl_openMmioFile},
    {"snd.closeMmioHandle", "thiscall", "uint32", 1, 0, a_closeMmioHandle, &g_negctl_closeMmioHandle},
    {"snd.Dll_Wave_Device::create_device", "default", "uint32", 2, 0, a_Dll_Wave_Device__create_device, &g_negctl_Dll_Wave_Device__create_device},
    {"snd.openWaveStreamFile", "stdcall", "uint32", 1, 0, a_openWaveStreamFile, &g_negctl_openWaveStreamFile},
    {"snd.Wave_Device::setCooperativeLevel", "stdcall", "uint32", 1, 0, a_Wave_Device__setCooperativeLevel, &g_negctl_Wave_Device__setCooperativeLevel},
    {"snd.Wave_Device::closeStream", "thiscall", "uint32", 0, 0, a_Wave_Device__closeStream, &g_negctl_Wave_Device__closeStream},
    {"snd.Dll_Wave_Device::delete_device", "default", "uint32", 0, 0, a_Dll_Wave_Device__delete_device, &g_negctl_Dll_Wave_Device__delete_device},
    {"snd.Wave_Device::ctor", "thiscall", "pointer", 1, 0, a_Wave_Device__ctor, &g_negctl_Wave_Device__ctor},
    {"snd.Wave_Device::scalarDeletingDtor", "thiscall", "uint32", 2, 0, a_Wave_Device__scalarDeletingDtor, &g_negctl_Wave_Device__scalarDeletingDtor},
    {"snd.Wave_Device::dtor", "thiscall", "uint32", 1, 0, a_Wave_Device__dtor, &g_negctl_Wave_Device__dtor},
    {"snd.Wave_Device::open", "thiscall", "uint32", 3, 0, a_Wave_Device__open, &g_negctl_Wave_Device__open},
    {"snd.Wave_Device::stop", "thiscall", "uint32", 1, 0, a_Wave_Device__stop, &g_negctl_Wave_Device__stop},
    {"snd.Wave_Device::start", "thiscall", "uint32", 1, 0, a_Wave_Device__start, &g_negctl_Wave_Device__start},
    {"snd.Wave_Device::closeOutput", "thiscall", "uint32", 1, 0, a_Wave_Device__closeOutput, &g_negctl_Wave_Device__closeOutput},
    {"snd.Wave_Device::stopOutput", "thiscall", "uint32", 1, 0, a_Wave_Device__stopOutput, &g_negctl_Wave_Device__stopOutput},
    {"snd.Wave_Device::startOutput", "thiscall", "uint32", 1, 0, a_Wave_Device__startOutput, &g_negctl_Wave_Device__startOutput},
    {"snd.Wave_Device::restart", "thiscall", "uint32", 2, 0, a_Wave_Device__restart, &g_negctl_Wave_Device__restart},
    {"snd.Wave_Device::serviceTick", "thiscall", "uint32", 1, 0, a_Wave_Device__serviceTick, &g_negctl_Wave_Device__serviceTick},
    {"snd.WaveCmdQueue::pushNode", "thiscall", "uint32", 3, 0, a_WaveCmdQueue__pushNode, &g_negctl_WaveCmdQueue__pushNode},
    {"snd.WaveCmdQueue::pushValue", "thiscall", "uint32", 2, 0, a_WaveCmdQueue__pushValue, &g_negctl_WaveCmdQueue__pushValue},
    {"snd.Wave_Device::flushCommandQueue", "thiscall", "uint32", 1, 0, a_Wave_Device__flushCommandQueue, &g_negctl_Wave_Device__flushCommandQueue},
    {"snd.Wave_Device::dispatchCommand", "thiscall", "uint32", 1, 0, a_Wave_Device__dispatchCommand, &g_negctl_Wave_Device__dispatchCommand},
    {"snd.Wave_Device::scanCommandRing", "thiscall", "uint32", 3, 0, a_Wave_Device__scanCommandRing, &g_negctl_Wave_Device__scanCommandRing},
    {"snd.Wave_Device::caps", "default", "uint32", 0, 0, a_Wave_Device__caps, &g_negctl_Wave_Device__caps},
    {"snd.Wave_Device::deviceName", "stdcall", "uint32", 3, 0, a_Wave_Device__deviceName, &g_negctl_Wave_Device__deviceName},
    {"snd.setOutputMasterVolume", "stdcall", "uint32", 1, 0, a_setOutputMasterVolume, &g_negctl_setOutputMasterVolume},
    {"snd.getWaveDeviceInterface", "thiscall", "uint32", 1, 0, a_getWaveDeviceInterface, &g_negctl_getWaveDeviceInterface},
    {"snd.serviceWaveDevice", "stdcall", "uint32", 1, 0, a_serviceWaveDevice, &g_negctl_serviceWaveDevice},
    {"snd.stubReturnBA", "thiscall", "uint32", 2, 0, a_stubReturnBA, &g_negctl_stubReturnBA},
    {"snd.Wave_In_Device::scalarDeletingDtor", "thiscall", "pointer", 2, 0, a_Wave_In_Device__scalarDeletingDtor, &g_negctl_Wave_In_Device__scalarDeletingDtor},
    {"snd.Wave_In_Device::dtor", "thiscall", "void", 1, 0, a_Wave_In_Device__dtor, &g_negctl_Wave_In_Device__dtor},
    {"snd.Dll_Wave_In_Device::create_device", "default", "uint32", 2, 0, a_Dll_Wave_In_Device__create_device, &g_negctl_Dll_Wave_In_Device__create_device},
    {"snd.Dll_Wave_In_Device::delete_device", "default", "uint32", 0, 0, a_Dll_Wave_In_Device__delete_device, &g_negctl_Dll_Wave_In_Device__delete_device},
    {"snd.Wave_In_Device::start", "default", "uint32", 0, 0, a_Wave_In_Device__start, &g_negctl_Wave_In_Device__start},
    {"snd.Wave_In_Device::stop", "default", "uint32", 0, 0, a_Wave_In_Device__stop, &g_negctl_Wave_In_Device__stop},
    {"snd.Wave_In_Device::openCapture", "thiscall", "uint32", 3, 0, a_Wave_In_Device__openCapture, &g_negctl_Wave_In_Device__openCapture},
    {"snd.Wave_In_Device::closeCapture", "thiscall", "uint32", 1, 0, a_Wave_In_Device__closeCapture, &g_negctl_Wave_In_Device__closeCapture},
    {"snd.Wave_In_Device::setStream", "stdcall", "uint32", 1, 0, a_Wave_In_Device__setStream, &g_negctl_Wave_In_Device__setStream},
    {"snd.Wave_In_Device::clearStream", "default", "uint32", 0, 0, a_Wave_In_Device__clearStream, &g_negctl_Wave_In_Device__clearStream},
    {"snd.MidiReader_freeContent", "thiscall", "void", 1, 0, a_MidiReader_freeContent, &g_negctl_MidiReader_freeContent},
    {"snd.streamReadByte", "thiscall", "int", 2, 0, a_streamReadByte, &g_negctl_streamReadByte},
    {"snd.streamConsumeByte", "thiscall", "void", 1, 0, a_streamConsumeByte, &g_negctl_streamConsumeByte},
    {"snd.MidiReader_init", "thiscall", "pointer", 1, 0, a_MidiReader_init, &g_negctl_MidiReader_init},
    {"snd.MidiReader_dtor", "thiscall", "void", 1, 0, a_MidiReader_dtor, &g_negctl_MidiReader_dtor},
    {"snd.WaveInDeviceBase_ctor", "thiscall", "uint32", 1, 0, a_WaveInDeviceBase_ctor, &g_negctl_WaveInDeviceBase_ctor},
    {"snd.WaveInDeviceBase_scalarDtor", "thiscall", "pointer", 2, 0, a_WaveInDeviceBase_scalarDtor, &g_negctl_WaveInDeviceBase_scalarDtor},
    {"snd.WaveInDeviceBase_openInner", "thiscall", "int", 3, 0, a_WaveInDeviceBase_openInner, &g_negctl_WaveInDeviceBase_openInner},
    {"snd.WaveInDeviceBase_closeInner", "thiscall", "int", 1, 0, a_WaveInDeviceBase_closeInner, &g_negctl_WaveInDeviceBase_closeInner},
    {"snd.enqueueWaveCmdE", "thiscall", "uint32", 4, 0, a_enqueueWaveCmdE, &g_negctl_enqueueWaveCmdE},
    {"snd.enqueueWaveCmd10", "thiscall", "uint32", 1, 0, a_enqueueWaveCmd10, &g_negctl_enqueueWaveCmd10},
    {"snd.Sound5c450::openMmio", "thiscall", "uint32", 2, 0, a_Sound5c450__openMmio, &g_negctl_Sound5c450__openMmio},
    {"snd.openMmioSound", "thiscall", "uint32", 2, 0, a_openMmioSound, &g_negctl_openMmioSound},
    {"snd.streamWaveSamples", "thiscall", "uint32", 5, 0, a_streamWaveSamples, &g_negctl_streamWaveSamples},
    {"snd.StreamReader_ctor", "thiscall", "pointer", 1, 0, a_StreamReader_ctor, &g_negctl_StreamReader_ctor},
    {"snd.StreamReader_scalarDtor", "thiscall", "uint32", 2, 0, a_StreamReader_scalarDtor, &g_negctl_StreamReader_scalarDtor},
    {"snd.streamReaderDtor", "thiscall", "void", 1, 0, a_streamReaderDtor, &g_negctl_streamReaderDtor},
    {"snd.WaveInStream_start", "thiscall", "uint32", 4, 0, a_WaveInStream_start, &g_negctl_WaveInStream_start},
    {"snd.freeMmioHandleList", "thiscall", "uint32", 1, 0, a_freeMmioHandleList, &g_negctl_freeMmioHandleList},
    {"snd.freeMmioHandleListGetInfo", "thiscall", "uint32", 1, 0, a_freeMmioHandleListGetInfo, &g_negctl_freeMmioHandleListGetInfo},
    {"snd.loadWaveResource", "thiscall", "uint32", 2, 0, a_loadWaveResource, &g_negctl_loadWaveResource},
    {"snd.WaveInStream_resetCodec", "thiscall", "uint32", 1, 0, a_WaveInStream_resetCodec, &g_negctl_WaveInStream_resetCodec},
    {"snd.beginDecodeStreamA", "thiscall", "uint32", 2, 0, a_beginDecodeStreamA, &g_negctl_beginDecodeStreamA},
    {"snd.beginDecodeStreamB", "thiscall", "uint32", 2, 0, a_beginDecodeStreamB, &g_negctl_beginDecodeStreamB},
    {"snd.closeDecoder", "thiscall", "uint32", 1, 0, a_closeDecoder, &g_negctl_closeDecoder},
    {"snd.return0xb_32660", "default", "uint32", 0, 0, a_return0xb_32660, &g_negctl_return0xb_32660},
    {"snd.parseRiffWave", "thiscall", "uint32", 2, 0, a_parseRiffWave, &g_negctl_parseRiffWave},
    {"snd.fillDecodeRing", "thiscall", "uint32", 1, 0, a_fillDecodeRing, &g_negctl_fillDecodeRing},
    {"snd.serviceDecodeRing", "thiscall", "uint32", 1, 0, a_serviceDecodeRing, &g_negctl_serviceDecodeRing},
    {"snd.closeMmioSound", "thiscall", "uint32", 1, 0, a_closeMmioSound, &g_negctl_closeMmioSound},
    {"snd.flushMmioBuffer", "thiscall", "uint32", 1, 0, a_flushMmioBuffer, &g_negctl_flushMmioBuffer},
    {"snd.MmioBuffer_init", "thiscall", "void", 3, 0, a_MmioBuffer_init, &g_negctl_MmioBuffer_init},
    {"snd.WaveInDevice_ctor", "thiscall", "uint32", 1, 0, a_WaveInDevice_ctor, &g_negctl_WaveInDevice_ctor},
    {"snd.WaveInDevice_deleteDtor", "thiscall", "uint32", 2, 0, a_WaveInDevice_deleteDtor, &g_negctl_WaveInDevice_deleteDtor},
    {"snd.WaveInDevice_dtor", "thiscall", "void", 1, 0, a_WaveInDevice_dtor, &g_negctl_WaveInDevice_dtor},
    {"snd.WaveInDevice_allocCaptureBuffers", "thiscall", "uint32", 3, 0, a_WaveInDevice_allocCaptureBuffers, &g_negctl_WaveInDevice_allocCaptureBuffers},
    {"snd.WaveInDevice_start", "thiscall", "uint32", 1, 0, a_WaveInDevice_start, &g_negctl_WaveInDevice_start},
    {"snd.WaveInDevice_prepareHeaders", "thiscall", "uint32", 1, 0, a_WaveInDevice_prepareHeaders, &g_negctl_WaveInDevice_prepareHeaders},
    {"snd.WaveInDevice_addBuffer", "thiscall", "uint32", 1, 0, a_WaveInDevice_addBuffer, &g_negctl_WaveInDevice_addBuffer},
    {"snd.WaveInDevice_reset", "thiscall", "uint32", 1, 0, a_WaveInDevice_reset, &g_negctl_WaveInDevice_reset},
    {"snd.onWaveInCallbackMessage", "stdcall", "void", 5, 0, a_onWaveInCallbackMessage, &g_negctl_onWaveInCallbackMessage},
    {"snd.WaveInDevice_open", "thiscall", "uint32", 1, 0, a_WaveInDevice_open, &g_negctl_WaveInDevice_open},
    {"snd.WaveInDevice_close", "thiscall", "uint32", 1, 0, a_WaveInDevice_close, &g_negctl_WaveInDevice_close},
    {"snd.WaveInDevice_requeueBuffer", "thiscall", "uint32", 2, 0, a_WaveInDevice_requeueBuffer, &g_negctl_WaveInDevice_requeueBuffer},
    {"snd.loadMultiChannel", "thiscall", "uint32", 2, 0, a_loadMultiChannel, &g_negctl_loadMultiChannel},
};

}  // namespace

extern const ModuleDef kModule_audio_wave = {"audio_wave", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim

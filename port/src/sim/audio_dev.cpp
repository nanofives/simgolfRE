// Step S4.8 part 2, module audio_dev (log/roadmap/S4/8_notes.md): sound.dll's devices, Ctrl objects, command queues, sound
// timer clients, the sound thread and window, and the GetProcAddress exports. Locals passed by address live on the audio core's
// stack (SndFrame, portsim/sim/audio.h; S4.8's SndLocals until S4.2b).
//
// Every body is the hook source's reimplementation (shim/src/re/c3*.cpp, cited per function) converted by
// re/tools/snd_hook2port.py and edited; calls go through the dispatcher of portsim/sim/audio.h. Comments cite rvas.
#include <cstring>

#include "portsim/arith.h"
#include "portsim/ptr.h"
#include "portsim/registry.h"
#include "portsim/sim/audio.h"
#include "portsim/sim/audio_dev.h"
#include "portsim/sim/soundstate.h"

namespace portsim {

namespace {
using u32 = uint32_t;

// ---- negative-control switches (one per function)
#define DEV_NEG(v) int g_negctl_##v = 0;
DEV_NEG(initCtrlVtable) DEV_NEG(ctrlScalarDeletingDtor) DEV_NEG(CommandQueue_init) DEV_NEG(CommandQueue_clear)
DEV_NEG(CommandQueue_at) DEV_NEG(dynInit_soundWindowObj) DEV_NEG(atexitDtor_100b49b8) DEV_NEG(init_sound_timer)
DEV_NEG(get_sound_version) DEV_NEG(release_sound) DEV_NEG(delete_sound) DEV_NEG(Midi_create_device)
DEV_NEG(Midi_delete_device) DEV_NEG(Device_serviceVoices) DEV_NEG(stubReturn2fA) DEV_NEG(stubReturn2fB)
DEV_NEG(registerTimerClient) DEV_NEG(CommandQueue_push) DEV_NEG(postNotification) DEV_NEG(dynInit_ctrl100b4b50)
DEV_NEG(dynInit_ctrl100b4c38) DEV_NEG(dynInit_ctrl100b4d20) DEV_NEG(dynInit_ctrl100b4a68) DEV_NEG(dynInit_ctrl100b4e08)
DEV_NEG(Ctrl_appendIfAccepted) DEV_NEG(CtrlObject_ctor) DEV_NEG(CtrlObject2_ctor) DEV_NEG(Ctrl5b5c0_ctor)
DEV_NEG(Ctrl5b19c_ctor) DEV_NEG(Ctrl5b58c_dtor) DEV_NEG(Ctrl5b5c0_transposeTable) DEV_NEG(CtrlBase_resetElements)
DEV_NEG(CtrlBase_flushElements) DEV_NEG(Ctrl5b5c0_mapValue) DEV_NEG(CtrlObject_scalarDtorA) DEV_NEG(CtrlObject_dtorA)
DEV_NEG(CtrlObject_scalarDtorB) DEV_NEG(CtrlObject_dtorB) DEV_NEG(Ctrl5b58c_scalarDtor) DEV_NEG(Ctrl5b58c_dtorEmbedded)
DEV_NEG(CtrlObject3_ctor) DEV_NEG(CtrlObject_scalarDtorC) DEV_NEG(CtrlObject_dtorC) DEV_NEG(Ctrl5b668_ctor)
DEV_NEG(CtrlObject_scalarDtorD) DEV_NEG(CtrlObject_dtorD) DEV_NEG(postMidiNotification) DEV_NEG(destroySoundWindowObj)
DEV_NEG(pumpWaveMessages) DEV_NEG(soundWindowProc) DEV_NEG(soundWindowSubclassProc) DEV_NEG(DeviceBase_scalarDtor)
DEV_NEG(DeviceBase_dtorA) DEV_NEG(DeviceBase_dtorB) DEV_NEG(DeviceBase_dtorC) DEV_NEG(stopSoundThreadFull)
DEV_NEG(startSignalThread) DEV_NEG(soundThreadPump) DEV_NEG(postSoundThreadMessage) DEV_NEG(Device_prepareSoundFlags)
DEV_NEG(Device_lazyCreateSound) DEV_NEG(Device_createStartQueue)
#undef DEV_NEG
#define NEG(v) SG_NEGCTL(g_negctl_##v)

// ---- imports (IAT slot rvas of the user's sound.dll)
constexpr u32 kIatPostMessageA = 0xb85a0;            // USER32!PostMessageA
constexpr u32 kIatSetWindowLongA = 0xb85a4;          // USER32!SetWindowLongA
constexpr u32 kIatMsgWait = 0xb85a8;                 // USER32!MsgWaitForMultipleObjects
constexpr u32 kIatPostThreadMessageA = 0xb85b0;      // USER32!PostThreadMessageA
constexpr u32 kIatPeekMessageA = 0xb85b4;            // USER32!PeekMessageA
constexpr u32 kIatCallWindowProcA = 0xb85b8;         // USER32!CallWindowProcA
constexpr u32 kIatDefWindowProcA = 0xb85bc;          // USER32!DefWindowProcA
constexpr u32 kIatOutputDebugStringA = 0xb83e8;      // KERNEL32!OutputDebugStringA
constexpr u32 kIatEnterCriticalSection = 0xb83f4;    // KERNEL32!EnterCriticalSection
constexpr u32 kIatSetEvent = 0xb83fc;                // KERNEL32!SetEvent
constexpr u32 kIatSetThreadPriority = 0xb8400;       // KERNEL32!SetThreadPriority
constexpr u32 kIatResetEvent = 0xb8404;              // KERNEL32!ResetEvent
constexpr u32 kIatGetCurrentThreadId = 0xb8408;      // KERNEL32!GetCurrentThreadId
constexpr u32 kIatSleep = 0xb840c;                   // KERNEL32!Sleep
constexpr u32 kIatLeaveCriticalSection = 0xb84e0;    // KERNEL32!LeaveCriticalSection

// ---- callees (thunk or body rvas; the dispatcher follows thunks)
constexpr u32 kThCtrlDtorA = 0x1019;           // CtrlObject_dtorA
constexpr u32 kThCtrlDtorB = 0x1041;           // CtrlObject_dtorB
constexpr u32 kThCmdQueueInit = 0x10aa;        // CommandQueue::init
constexpr u32 kThSignalThreadStart = 0x1366;   // soundThreadPump (the start routine startSignalThread passes)
constexpr u32 kThCreateSound = 0x137a;         // create_sound
constexpr u32 kThInitCtrlVtable = 0x14f1;      // initCtrlVtable
constexpr u32 kThSoundGroupStop = 0x16a4;      // SoundGroup_stop
constexpr u32 kThWndProc = 0x1780;             // soundWindowProc
constexpr u32 kThPairClear = 0x1816;           // pairClear
constexpr u32 kThDeleteSound = 0x1a87;         // delete_sound
constexpr u32 kThCtrlDtorC = 0x1ab9;           // CtrlObject_dtorC
constexpr u32 kThSoundWindowObjCtor = 0x1c62;  // SoundWindowObj_ctor
constexpr u32 kThVoiceUpdate = 0x1c4e;         // Voice::update
constexpr u32 kThDispatchCommand = 0x1f4b;     // Midi_Device::dispatchCommand
constexpr u32 kThWaveInRequeue = 0x20a4;       // WaveInDevice_requeueBuffer
constexpr u32 kThSoundTimerInit = 0x22b1;      // SoundTimer::init
constexpr u32 kThPtrListPushBack = 0x22f7;     // PtrList_pushBack
constexpr u32 kThServiceDecodeRing = 0x22fc;   // serviceDecodeRing
constexpr u32 kThCtrlDtorD = 0x24af;           // CtrlObject_dtorD
constexpr u32 kThTimerStopKeepLock = 0x24e6;   // SoundTimer::stopKeepLock
constexpr u32 kThSoundGroupRewind = 0x2568;    // SoundGroup_rewind
constexpr u32 kThSubclassProc = 0x25ef;        // soundWindowSubclassProc
constexpr u32 kThMidiDeviceBaseCtor = 0x260d;  // MidiDeviceBase_ctor
constexpr u32 kThTimerNoop = 0x2671;           // a bare `ret` (rva 0x9a70) stored as the timer's callback
constexpr u32 kThEnginePlaySound = 0x26e9;     // Engine_playSound
constexpr u32 kThSoundWindowObjDtor = 0x2784;  // SoundWindowObj_dtor
constexpr u32 kThTimerStop = 0x218a;           // SoundTimer::stop
constexpr u32 kListNode3Init = 0x87f0;         // listNode3Init (called directly)
constexpr u32 kTimerRegisterClient = 0x88e0;   // SoundTimer::registerClient (called directly)
constexpr u32 kTimerStart = 0x8ce0;            // SoundTimer::start (called directly)
constexpr u32 kAtexitDtorStub = 0x91f0;        // atexitDtor_100b49b8 (pushed for _atexit)
constexpr u32 kCtrl5b19cCtor = 0x1bee0;        // Ctrl5b19c_ctor (called directly by the dynamic initializers)
constexpr u32 kTransposeTable = 0x1c1e0;       // Ctrl5b5c0_transposeTable (called directly)
constexpr u32 kCtrl5b58cDtorEmbedded = 0x1c990;  // Ctrl5b58c_dtorEmbedded (called directly)
constexpr u32 kPtrListInit = 0x1d130;          // PtrList_init (called directly)
constexpr u32 kPostSoundThreadMessage = 0x2aa90;  // postSoundThreadMessage (called directly)
constexpr u32 kCrtAtexit = 0x42647;            // the static CRT's _atexit
constexpr u32 kCrtBeginThread = 0x42984;       // the static CRT's _beginthread

// ---- unnamed .data globals by rva (no names row)
constexpr u32 kSoundWindowObj = 0xb49b8;       // the SoundWindowObj the dynamic initializer constructs
constexpr u32 kSubclassPrevProc = 0xb49c4;     // the window procedure soundWindowSubclassProc chains to
constexpr u32 kSigRunning = 0xb5060;           // soundThreadPump's running byte
constexpr u32 kSigTid = 0xb5054;               // the sound thread's id (g_soundThreadState +0x1c)
constexpr u32 kSigEvent = 0xb505c;             // its event handle (g_soundThreadState +0x24)
constexpr u32 kSigCS = 0xb503c;                // its critical section (g_soundThreadState +4)
constexpr u32 kSigObj = 0xb5038;               // its flag byte (g_soundThreadState +0)

u32 th(Ctx &c, u32 rva, std::initializer_list<uint32_t> args) { return snd_call(c, rva, SndAbi::Thiscall, args).eax; }
u32 cd(Ctx &c, u32 rva, std::initializer_list<uint32_t> args) { return snd_call(c, rva, SndAbi::Cdecl, args).eax; }
u32 vt(Ctx &c, u32 obj, u32 slot, std::initializer_list<uint32_t> args) {
  return snd_vcall(c, obj, slot, SndAbi::Thiscall, args).eax;
}
u32 imp(Ctx &c, u32 iat, std::initializer_list<uint32_t> args) { return snd_import(c, iat, SndAbi::Stdcall, args).eax; }
u32 gv(Ctx &c, u32 rva) { return ld<u32>(c, snd_va(c, rva)); }
u32 timer_va(Ctx &c) { return snd_obj(c, &SoundState::g_soundTimer).va(); }
u32 midi_va(Ctx &c) { return snd_obj(c, &SoundState::g_midiDevice).va(); }
u32 thread_state_va(Ctx &c) { return snd_arr(c, &SoundState::g_soundThreadState).va(); }

// ================================================================================================ Ctrl objects
// initCtrlVtable (rva 0x5720, c3dj): +0 = the CtrlObject vtable (rva 0x5b19c). neg 1: the vtable of CtrlObject2.
void a_initCtrlVtable(Ctx &c, const uint32_t *a, Ret &) {
  st<u32>(c, a[0], snd_va(c, NEG(initCtrlVtable) == 1 ? snd_rva::vt_CtrlObject2 : snd_rva::vt_CtrlObject));
}
// The scalar deleting destructors (c3dj / c3dt): the destructor body through its thunk, operator delete when bit 0 of the flags
// is set, `this` returned. neg 1: the delete ignores the flags.
void scalar_dtor(Ctx &c, const uint32_t *a, Ret &r, u32 dtor, int neg) {
  th(c, dtor, {a[0]});
  if ((a[1] & 1) || neg == 1) snd_delete(c, a[0]);
  r.set_u32(a[0]);
}
void a_ctrlScalarDeletingDtor(Ctx &c, const uint32_t *a, Ret &r) {  // rva 0x5740
  scalar_dtor(c, a, r, kThInitCtrlVtable, NEG(ctrlScalarDeletingDtor));
}
void a_CtrlObject_scalarDtorA(Ctx &c, const uint32_t *a, Ret &r) {  // rva 0x1c530
  scalar_dtor(c, a, r, kThCtrlDtorA, NEG(CtrlObject_scalarDtorA));
}
void a_CtrlObject_scalarDtorB(Ctx &c, const uint32_t *a, Ret &r) {  // rva 0x1c6e0
  scalar_dtor(c, a, r, kThCtrlDtorB, NEG(CtrlObject_scalarDtorB));
}
void a_CtrlObject_scalarDtorC(Ctx &c, const uint32_t *a, Ret &r) {  // rva 0x1cba0
  scalar_dtor(c, a, r, kThCtrlDtorC, NEG(CtrlObject_scalarDtorC));
}
void a_CtrlObject_scalarDtorD(Ctx &c, const uint32_t *a, Ret &r) {  // rva 0x1cca0
  scalar_dtor(c, a, r, kThCtrlDtorD, NEG(CtrlObject_scalarDtorD));
}
// CtrlObject_dtorA..D (rvas 0x1c560 / 0x1c710 / 0x1cbd0 / 0x1ccd0, c3dj): +0 = the CtrlObject vtable. neg 1: +0 not stored.
void ctrl_dtor(Ctx &c, const uint32_t *a, int neg) {
  if (neg != 1) st<u32>(c, a[0], snd_va(c, snd_rva::vt_CtrlObject));
}
void a_CtrlObject_dtorA(Ctx &c, const uint32_t *a, Ret &) { ctrl_dtor(c, a, NEG(CtrlObject_dtorA)); }
void a_CtrlObject_dtorB(Ctx &c, const uint32_t *a, Ret &) { ctrl_dtor(c, a, NEG(CtrlObject_dtorB)); }
void a_CtrlObject_dtorC(Ctx &c, const uint32_t *a, Ret &) { ctrl_dtor(c, a, NEG(CtrlObject_dtorC)); }
void a_CtrlObject_dtorD(Ctx &c, const uint32_t *a, Ret &) { ctrl_dtor(c, a, NEG(CtrlObject_dtorD)); }

// CtrlObject::ctor (rva 0x1bc50, c3dj): +0x14 +0x20 +0x24 = -1, vtable 0x5b19c, +4 = 'ctrl' (0x6c727463), word +0x18 = 0,
// +0xc = 0x18, +0x10 = 0x44444444; returns this. neg 1: +0x24 keeps its value.
void a_CtrlObject_ctor(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 s = a[0];
  st<u32>(c, s + 0x14, 0xffffffffu);
  st<u32>(c, s + 0x20, 0xffffffffu);
  if (NEG(CtrlObject_ctor) != 1) st<u32>(c, s + 0x24, 0xffffffffu);
  st<u32>(c, s, snd_va(c, snd_rva::vt_CtrlObject));
  st<u32>(c, s + 4, 0x6c727463u);
  st<uint16_t>(c, s + 0x18, 0);
  st<u32>(c, s + 0xc, 0x18);
  st<u32>(c, s + 0x10, 0x44444444u);
  r.set_u32(s);
}
// CtrlObject2_ctor (rva 0x1bca0, c3dj): the list header +0xc..+0x18 cleared, +4 = 0x18, +8 = 0x44444444, vtable 0x5b4f8
// (vt_CtrlObject2), +0x28 = 0, +0x1c = 0xc, +0x20 = 0x44444444. neg 1: +0x1c = 0x18.
void a_CtrlObject2_ctor(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 s = a[0];
  for (u32 off : {0xcu, 0x10u, 0x14u, 0x18u}) st<u32>(c, s + off, 0);
  st<u32>(c, s + 4, 0x18);
  st<u32>(c, s + 8, 0x44444444u);
  st<u32>(c, s, snd_va(c, snd_rva::vt_CtrlObject2));
  st<u32>(c, s + 0x28, 0);
  st<u32>(c, s + 0x1c, NEG(CtrlObject2_ctor) == 1 ? 0x18u : 0xcu);
  st<u32>(c, s + 0x20, 0x44444444u);
  r.set_u32(s);
}
// The CtrlObject2 base and embedded CtrlObject shared by Ctrl5b5c0_ctor (0x1bd9a-0x1bde4) and Ctrl5b19c_ctor (0x1bf10-0x1bf54).
void ctrl5b5c0_base(Ctx &c, u32 o) {
  st<u32>(c, o + 4, 0x18);
  st<u32>(c, o + 8, 0x44444444u);
  st<u32>(c, o + 0x28, 0);
  st<u32>(c, o + 0x1c, 0xc);
  st<u32>(c, o + 0x20, 0x44444444u);
  st<u32>(c, o + 0x40, 0xffffffffu);
  st<u32>(c, o + 0x4c, 0xffffffffu);
  st<u32>(c, o + 0x50, 0xffffffffu);
  st<u32>(c, o + 0x30, 0x6c727463u);
  st<uint16_t>(c, o + 0x44, 0);
  st<u32>(c, o + 0x38, 0x18);
  st<u32>(c, o + 0x3c, 0x44444444u);
}
// Ctrl5b5c0_ctor (rva 0x1bd90, c3dj): the list header +0xc..+0x18 cleared, the base, vtables 0x5b5c0 / 0x5b59c, the 0x80-byte
// identity table at +0x5c, +0x48 = 3, byte +0x44 = 1, +0xe0 = 0, +0x34 = 0xe4, +0x58 = 0x44444444, +0x54 = 0x8c; this.
// neg 1: the table's last byte is 0.
void a_Ctrl5b5c0_ctor(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 o = a[0];
  for (u32 off : {0xcu, 0x10u, 0x14u, 0x18u}) st<u32>(c, o + off, 0);
  ctrl5b5c0_base(c, o);
  st<u32>(c, o, snd_va(c, snd_rva::vt_Ctrl5b5c0));
  st<u32>(c, o + 0x2c, snd_va(c, snd_rva::vt_Ctrl5b59c));
  for (u32 i = 0; i < 0x80; ++i)
    st<uint8_t>(c, o + 0x5c + i, static_cast<uint8_t>(NEG(Ctrl5b5c0_ctor) == 1 && i == 0x7f ? 0 : i));
  st<u32>(c, o + 0x48, 3);
  st<uint8_t>(c, o + 0x44, 1);
  st<u32>(c, o + 0xe0, 0);
  st<u32>(c, o + 0x34, 0xe4);
  st<u32>(c, o + 0x58, 0x44444444u);
  st<u32>(c, o + 0x54, 0x8c);
  r.set_u32(o);
}
// Ctrl5b19c_ctor (rva 0x1bee0, c3cx CtrlCtor_re), thiscall (12 table bytes, a13): PtrList_init on +0xc, the base, the 12
// bytes at +0x5c, vtables 0x5b5c0 / 0x5b59c, Ctrl5b5c0_transposeTable(0), +0x48 = 3, +0xe0 = 0, +0x34 = 0xe4, +0xdc = a13,
// +0x58 = 0x44444444, +0x54 = 0x8c; returns this. neg 1: +0xdc not stored.
void a_Ctrl5b19c_ctor(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 p = a[0];
  th(c, kPtrListInit, {p + 0xc});
  ctrl5b5c0_base(c, p);
  for (u32 i = 0; i < 12; ++i) st<uint8_t>(c, p + 0x5c + i, static_cast<uint8_t>(a[1 + i]));
  st<u32>(c, p, snd_va(c, snd_rva::vt_Ctrl5b5c0));
  st<u32>(c, p + 0x2c, snd_va(c, snd_rva::vt_Ctrl5b59c));
  th(c, kTransposeTable, {p, 0});
  st<u32>(c, p + 0x48, 3);
  st<u32>(c, p + 0xe0, 0);
  st<u32>(c, p + 0x34, 0xe4);
  if (NEG(Ctrl5b19c_ctor) != 1) st<u32>(c, p + 0xdc, a[13]);
  st<u32>(c, p + 0x58, 0x44444444u);
  st<u32>(c, p + 0x54, 0x8c);
  r.set_u32(p);
}
// CtrlObject3_ctor (rva 0x1caf0, c3dj) and Ctrl5b668_ctor (rva 0x1cbf0, c3dj): the CtrlObject fields, their vtable, the extra
// fields cleared, +0x1c = 2 / 1, +8 = 0x38 / 0x34. neg 1: +0x1c = 0.
void a_CtrlObject3_ctor(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 s = a[0];
  st<u32>(c, s + 0x14, 0xffffffffu);
  st<u32>(c, s + 0x20, 0xffffffffu);
  st<u32>(c, s + 0x24, 0xffffffffu);
  st<u32>(c, s + 4, 0x6c727463u);
  st<uint16_t>(c, s + 0x18, 0);
  st<u32>(c, s + 0xc, 0x18);
  st<u32>(c, s + 0x10, 0x44444444u);
  st<u32>(c, s, snd_va(c, snd_rva::vt_CtrlObject3));
  for (u32 off : {0x2cu, 0x28u, 0x30u, 0x34u}) st<u32>(c, s + off, 0);
  st<u32>(c, s + 0x1c, NEG(CtrlObject3_ctor) == 1 ? 0u : 2u);
  st<u32>(c, s + 8, 0x38);
  r.set_u32(s);
}
void a_Ctrl5b668_ctor(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 s = a[0];
  st<u32>(c, s + 0x14, 0xffffffffu);
  st<u32>(c, s + 0x20, 0xffffffffu);
  st<u32>(c, s + 0x24, 0xffffffffu);
  st<u32>(c, s + 4, 0x6c727463u);
  st<uint16_t>(c, s + 0x18, 0);
  st<u32>(c, s + 0xc, 0x18);
  st<u32>(c, s + 0x10, 0x44444444u);
  st<u32>(c, s, snd_va(c, snd_rva::vt_Ctrl5b668));
  st<u32>(c, s + 0x2c, 0);
  st<u32>(c, s + 0x30, 0);
  st<u32>(c, s + 0x1c, NEG(Ctrl5b668_ctor) == 1 ? 0u : 1u);
  st<u32>(c, s + 8, 0x34);
  r.set_u32(s);
}

// The two drain loops of the Ctrl5b58c destructors (c3do c3do_drainList): list head +0xc, tail +0x10, count +0x18, nodes
// {prev, next, value}. Loop 1 pops while the head is non-NULL: a NULL value ends it, else the value's slot 0 runs with 1.
// Loop 2 (the unwind copy) pops without calling the value. neg: loop 1 does not decrement the count.
void drain_ctrl_list(Ctx &c, u32 self, int neg) {
  for (int pass = 0; pass < 2; ++pass) {
    for (;;) {
      const u32 node = ld<u32>(c, self + 0xc);
      if (!node) break;
      const u32 next = ld<u32>(c, node + 4);
      if (next) {
        st<u32>(c, next, 0);
        st<u32>(c, self + 0xc, next);
      } else {
        st<u32>(c, self + 0x10, 0);
        st<u32>(c, self + 0xc, 0);
      }
      const u32 value = ld<u32>(c, node + 8);
      snd_delete(c, node);
      if (!(pass == 0 && neg == 1)) st<int32_t>(c, self + 0x18, ld<int32_t>(c, self + 0x18) - 1);
      if (!value) break;
      if (pass == 0) vt(c, value, 0, {1});
    }
  }
}
// Ctrl5b58c_dtor (rva 0x1c080, c3do): vtable 0x5b58c, then the drain. neg 1: see drain_ctrl_list.
void a_Ctrl5b58c_dtor(Ctx &c, const uint32_t *a, Ret &) {
  st<u32>(c, a[0], snd_va(c, snd_rva::vt_Ctrl5b58c));
  drain_ctrl_list(c, a[0], NEG(Ctrl5b58c_dtor));
}
// Ctrl5b58c_dtorEmbedded (rva 0x1c990, c3do): called on the embedded sub-object (this = object + 0x2c): the sub-object's +0 =
// the CtrlObject vtable, the object's +0 = 0x5b58c, the drain on the object. neg 1: the sub-object's vtable not stored.
void a_Ctrl5b58c_dtorEmbedded(Ctx &c, const uint32_t *a, Ret &) {
  const u32 sub = a[0], self = sub - 0x2c;
  if (NEG(Ctrl5b58c_dtorEmbedded) != 1) st<u32>(c, sub, snd_va(c, snd_rva::vt_CtrlObject));
  st<u32>(c, self, snd_va(c, snd_rva::vt_Ctrl5b58c));
  drain_ctrl_list(c, self, 0);
}
// Ctrl5b58c_scalarDtor (rva 0x1c950, c3do): on the sub-object: the embedded destructor, then operator delete of sub - 0x2c with
// bit 0 of the flags; returns sub - 0x2c. neg 1: returns sub.
void a_Ctrl5b58c_scalarDtor(Ctx &c, const uint32_t *a, Ret &r) {
  th(c, kCtrl5b58cDtorEmbedded, {a[0]});
  const u32 self = a[0] - 0x2c;
  if (a[1] & 1) snd_delete(c, self);
  r.set_u32(NEG(Ctrl5b58c_scalarDtor) == 1 ? a[0] : self);
}

// Ctrl5b5c0_transposeTable (rva 0x1c1e0, c3dj Transpose_re), thiscall (n): above 9 returns 0xa; else the 0x80-byte table at
// +0x5c is rebuilt from the 12-byte octave row n (see the hook source); 0. neg 1: the & 0x7f mask is dropped.
void a_Ctrl5b5c0_transposeTable(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 n = a[1];
  if (n > 9) return r.set_u32(0xa);
  const u32 tbl = a[0] + 0x5c;
  int32_t ecx = static_cast<int32_t>(n);
  const int32_t edi = ecx * 12;
  int32_t esi = 0, edx = 0;
  do {
    if (esi == edi) {
      esi = edi + 12;
      ecx -= 1;
    }
    const uint8_t bl = static_cast<uint8_t>(static_cast<int8_t>(static_cast<uint8_t>(ecx)) * 12);
    uint8_t al = ld<uint8_t>(c, tbl + static_cast<u32>(edi + edx));
    al = static_cast<uint8_t>(al - bl);
    if (NEG(Ctrl5b5c0_transposeTable) != 1) al &= 0x7f;
    st<uint8_t>(c, tbl + static_cast<u32>(esi), al);
    if (edx == 0xb) {
      edx = 0;
      ecx -= 1;
    } else {
      edx += 1;
    }
    esi += 1;
  } while (esi < 0x80);
  r.set_u32(0);
}

// CtrlBase_resetElements / flushElements (rvas 0x1c2a0 / 0x1c2f0, c3cf): `this` is the embedded base (object + 0x2c); the
// element list at this-0x20 (cursor this-0x18) is walked calling each element's slot +0x14 / +0x18 (thiscall, no argument)
// until a NULL element; reset then stores 0x18 at this-0x28. neg 1: reset keeps this-0x28; flush calls slot +0x14.
void walk_elements(Ctx &c, u32 self, u32 slot) {
  u32 node = ld<u32>(c, self - 0x20);
  st<u32>(c, self - 0x18, node);
  while (node) {
    const u32 elem = ld<u32>(c, node + 8);
    if (!elem) break;
    vt(c, elem, slot, {});
    node = ld<u32>(c, self - 0x18);
    if (!node) break;
    node = ld<u32>(c, node + 4);
    st<u32>(c, self - 0x18, node);
  }
}
void a_CtrlBase_resetElements(Ctx &c, const uint32_t *a, Ret &) {
  walk_elements(c, a[0], 0x14);
  if (NEG(CtrlBase_resetElements) != 1) st<int32_t>(c, a[0] - 0x28, 0x18);
}
void a_CtrlBase_flushElements(Ctx &c, const uint32_t *a, Ret &) {
  walk_elements(c, a[0], NEG(CtrlBase_flushElements) == 1 ? 0x14 : 0x18);
}

// Ctrl5b5c0_mapValue (rva 0x1c370, c3cf), thiscall (value): the element list at +0xc (cursor +0x14): type 3 / 4 elements map
// the value through their slot +0x10 (thiscall, value), a type 5 element with byte +0x19 = 0 returns the value as it is; at the
// end the byte at +0x5c + value. neg 1: type 4 is skipped.
void a_Ctrl5b5c0_mapValue(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 self = a[0];
  int32_t value = static_cast<int32_t>(a[1]);
  const u32 cursor = self + 0x14;
  u32 node = ld<u32>(c, self + 0xc);
  st<u32>(c, cursor, node);
  u32 elem = node ? ld<u32>(c, node + 8) : 0;
  while (node && elem) {
    const int32_t type = ld<int32_t>(c, elem + 0x1c);
    if (type == 3 || (type == 4 && NEG(Ctrl5b5c0_mapValue) != 1)) {
      value = static_cast<int32_t>(vt(c, elem, 0x10, {static_cast<u32>(value)}));
    } else if (type == 5) {
      if (ld<uint8_t>(c, elem + 0x19) == 0) return r.set_i32(value);
    }
    node = ld<u32>(c, cursor);
    if (!node) break;
    node = ld<u32>(c, node + 4);
    st<u32>(c, cursor, node);
    if (!node) break;
    elem = ld<u32>(c, node + 8);
    if (!elem) break;
  }
  r.set_u32(ld<uint8_t>(c, self + 0x5c + static_cast<u32>(value)));
}

// Ctrl_appendIfAccepted (rva 0x1b9a0, c3dj CtrlAppend_re), thiscall (arg): slot 0 of this (thiscall, arg); a zero low byte
// returns 0x21; else PtrList_pushBack(this + 0xc, arg); 0. neg 1: the pushBack goes to this + 0x10.
void a_Ctrl_appendIfAccepted(Ctx &c, const uint32_t *a, Ret &r) {
  if ((vt(c, a[0], 0, {a[1]}) & 0xff) == 0) return r.set_u32(0x21);
  th(c, kThPtrListPushBack, {a[0] + (NEG(Ctrl_appendIfAccepted) == 1 ? 0x10u : 0xcu), a[1]});
  r.set_u32(0);
}

// The five Ctrl dynamic initializers (rvas 0x1b510 / 0x1b6b0 / 0x1b710 / 0x1b770 / 0x1b7d0, c3do): Ctrl5b19c_ctor on the
// global object with its twelve table bytes and the selector, then _atexit with the object's destructor stub; _atexit's eax.
// neg 1: the selector argument + 1.
void dyn_ctrl(Ctx &c, Ret &r, u32 obj, std::initializer_list<uint32_t> bytes, u32 sel, u32 stub, int neg) {
  uint32_t args[14];
  args[0] = snd_va(c, obj);
  u32 k = 1;
  for (u32 b : bytes) args[k++] = b;
  args[13] = neg == 1 ? sel + 1 : sel;
  snd_call_va(c, snd_va(c, kCtrl5b19cCtor), SndAbi::Thiscall, args, 14);
  r.set_u32(cd(c, kCrtAtexit, {snd_va(c, stub)}));
}
void a_dynInit_ctrl100b4b50(Ctx &c, const uint32_t *, Ret &r) {
  dyn_ctrl(c, r, 0xb4b50, {2, 2, 4, 4, 5, 7, 7, 9, 9, 0xb, 0xb, 0xc}, 0, 0x1b550, NEG(dynInit_ctrl100b4b50));
}
void a_dynInit_ctrl100b4c38(Ctx &c, const uint32_t *, Ret &r) {
  dyn_ctrl(c, r, 0xb4c38, {4, 4, 5, 5, 7, 9, 9, 0xb, 0xb, 0xc, 0xc, 0xe}, 1, 0x1b6f0, NEG(dynInit_ctrl100b4c38));
}
void a_dynInit_ctrl100b4d20(Ctx &c, const uint32_t *, Ret &r) {
  dyn_ctrl(c, r, 0xb4d20, {5, 5, 7, 7, 9, 0xb, 0xb, 0xc, 0xc, 0xe, 0xe, 0x10}, 2, 0x1b750, NEG(dynInit_ctrl100b4d20));
}
void a_dynInit_ctrl100b4a68(Ctx &c, const uint32_t *, Ret &r) {
  dyn_ctrl(c, r, 0xb4a68, {7, 7, 9, 9, 0xb, 0xc, 0xc, 0xe, 0xe, 0x10, 0x10, 0x11}, 3, 0x1b7b0, NEG(dynInit_ctrl100b4a68));
}
void a_dynInit_ctrl100b4e08(Ctx &c, const uint32_t *, Ret &r) {
  dyn_ctrl(c, r, 0xb4e08, {9, 9, 0xb, 0xb, 0xc, 0xe, 0xe, 0x10, 0x10, 0x11, 0x11, 0x13}, 4, 0x1b810, NEG(dynInit_ctrl100b4e08));
}

// ================================================================================================ CommandQueue
// CommandQueue::init (rva 0x8860, c3an) / ::clear (rva 0x8890, c3dj): the 0x1000 dwords zeroed, +0x4004 = +0x4000 = 0; init
// returns this, clear 0. neg 1: the last dword of the table is not cleared.
void cq_zero(Ctx &c, u32 s, int neg) {
  for (u32 i = 0; i < 0x1000; ++i)
    if (!(neg == 1 && i == 0xfff)) st<u32>(c, s + i * 4, 0);
  st<u32>(c, s + 0x4004, 0);
  st<u32>(c, s + 0x4000, 0);
}
void a_CommandQueue_init(Ctx &c, const uint32_t *a, Ret &r) {
  cq_zero(c, a[0], NEG(CommandQueue_init));
  r.set_u32(a[0]);
}
void a_CommandQueue_clear(Ctx &c, const uint32_t *a, Ret &r) {
  cq_zero(c, a[0], NEG(CommandQueue_clear));
  r.set_u32(0);
}
// CommandQueue::at (rva 0x88c0, c3an), thiscall (index): the dword at index & 0xfff. neg 1: index & 0x7ff.
void a_CommandQueue_at(Ctx &c, const uint32_t *a, Ret &r) {
  r.set_u32(ld<u32>(c, a[0] + (a[1] & (NEG(CommandQueue_at) == 1 ? 0x7ffu : 0xfffu)) * 4));
}
// CommandQueue::push (rva 0xa240, c3dh CmdPush_re), thiscall (a1..a4): next = (+0x4054 + 1) & 0xfff; equal to +0x4050 returns
// 0x22; else a 0x10-byte node (operator new, listNode3Init when non-NULL) {a2, a1, a3, a4} stored at +0x50 + 4 * next,
// +0x4054 = next; 0. neg 1: the first two fields are swapped.
void a_CommandQueue_push(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 self = a[0];
  const u32 next = (ld<u32>(c, self + 0x4054) + 1) & 0xfff;
  if (next == ld<u32>(c, self + 0x4050)) return r.set_u32(0x22);
  u32 node = snd_new(c, 0x10);
  if (node) node = th(c, kListNode3Init, {node});
  st<u32>(c, self + 0x50 + 4 * next, node);
  const bool sw = NEG(CommandQueue_push) == 1;
  st<u32>(c, node, sw ? a[1] : a[2]);
  st<u32>(c, node + 4, sw ? a[2] : a[1]);
  st<u32>(c, node + 8, a[3]);
  st<u32>(c, node + 0xc, a[4]);
  st<u32>(c, self + 0x4054, next);
  r.set_u32(0);
}

// ================================================================================================ timer clients, devices
// registerTimerClient (rva 0xa170, c3dh RegTimer_re): no g_soundTimer returns 0; +0x2c = the timer;
// SoundTimer::registerClient(timer, this); SoundTimer::start(g_soundTimer re-read)'s eax. neg 1: +0x2c not stored.
void a_registerTimerClient(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 t = ld<u32>(c, timer_va(c));
  if (!t) return r.set_u32(0);
  if (NEG(registerTimerClient) != 1) st<u32>(c, a[0] + 0x2c, t);
  th(c, kTimerRegisterClient, {t, a[0]});
  r.set_u32(th(c, kTimerStart, {ld<u32>(c, timer_va(c))}));
}

// init_sound_timer (rva 0x92e0, export #3; c3fd InitTimer_re), cdecl (unused, flags): g_soundTimerFlag = flags; bit 0 clear:
// operator new(0x5c), SoundTimer::init on a non-NULL block (its eax, NULL gives 0) into g_soundTimer, and +0x24 of it = the
// thunk 0x2671 (no NULL test: an allocation failure faults); 0. neg 1: the flag is not stored.
void a_init_sound_timer(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 flags = a[1];
  if (NEG(init_sound_timer) != 1) st<u32>(c, snd_obj(c, &SoundState::g_soundTimerFlag).va(), flags);
  if (!(flags & 1)) {
    const u32 p = snd_new(c, 0x5c);
    const u32 t = p ? th(c, kThSoundTimerInit, {p}) : 0;
    st<u32>(c, timer_va(c), t);
    st<u32>(c, t + 0x24, snd_va(c, kThTimerNoop));
  }
  r.set_u32(0);
}

// get_sound_version (rva 0x9370, export, emu-only row): `mov eax, 0x5000c01; ret`. neg 1: 0x5000c00.
void a_get_sound_version(Ctx &, const uint32_t *, Ret &r) {
  r.set_u32(NEG(get_sound_version) == 1 ? 0x5000c00u : 0x5000c01u);
}

// release_sound (rva 0x9390, export; c3fe RelSnd_re): with g_soundTimer, SoundTimer::stopKeepLock on it, then (re-read) a
// non-NULL timer gets SoundTimer::stop and operator delete; g_soundTimer = 0; 0. neg 1: the global is not cleared.
void a_release_sound(Ctx &c, const uint32_t *, Ret &r) {
  const u32 t = ld<u32>(c, timer_va(c));
  if (!t) return r.set_u32(0);
  th(c, kThTimerStopKeepLock, {t});
  if (u32 t2 = ld<u32>(c, timer_va(c))) {
    th(c, kThTimerStop, {t2});
    snd_delete(c, t2);
  }
  if (NEG(release_sound) != 1) st<u32>(c, timer_va(c), 0);
  r.set_u32(0);
}

// Dll_Midi_Device::create_device (rva 0x9a90, export #7; c3fl MidiCreate_re), cdecl (out): g_midiDevice set returns 0xc;
// operator new(0x4058); NULL: g_midiDevice = 0, *out = 0, 0; else MidiDeviceBase_ctor, +0x40..+0x4c = 0,
// CommandQueue::init(+0x50), vtable vt_MidiSeq, +0x24 +0x28 = 0, +0x38 = 0x7f, +0x2c = 0, +0x30 = 0xf, +0x34 +0x3c = 0,
// g_midiDevice = *out = the device; 0 (the success path reports 0 after a failed allocation too: an original defect).
// neg 1: +0x30 = 0.
void a_Midi_create_device(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 out = a[0];
  if (ld<u32>(c, midi_va(c))) return r.set_u32(0xc);
  const u32 d = snd_new(c, 0x4058);
  if (!d) {
    st<u32>(c, midi_va(c), 0);
    st<u32>(c, out, 0);
    return r.set_u32(0);
  }
  th(c, kThMidiDeviceBaseCtor, {d});
  for (u32 off : {0x40u, 0x44u, 0x48u, 0x4cu}) st<u32>(c, d + off, 0);
  th(c, kThCmdQueueInit, {d + 0x50});
  st<u32>(c, d, snd_va(c, snd_rva::vt_MidiSeq));
  st<u32>(c, d + 0x24, 0);
  st<u32>(c, d + 0x28, 0);
  st<u32>(c, d + 0x38, 0x7f);
  st<u32>(c, d + 0x2c, 0);
  st<u32>(c, d + 0x30, NEG(Midi_create_device) == 1 ? 0u : 0xfu);
  st<u32>(c, d + 0x34, 0);
  st<u32>(c, d + 0x3c, 0);
  st<u32>(c, midi_va(c), d);
  st<u32>(c, out, d);
  r.set_u32(0);
}
// Dll_Midi_Device::delete_device (rva 0x9bb0, export #8; c3fd DelMidi_re): a non-NULL g_midiDevice gets its slot 0 with 1;
// g_midiDevice = 0; 0. neg 1: the slot gets 0.
void a_Midi_delete_device(Ctx &c, const uint32_t *, Ret &r) {
  if (u32 dev = ld<u32>(c, midi_va(c))) vt(c, dev, 0, {NEG(Midi_delete_device) == 1 ? 0u : 1u});
  st<u32>(c, midi_va(c), 0);
  r.set_u32(0);
}

// Device::serviceVoices (rva 0xa050, c3ex SvcVoices_re): Midi_Device::dispatchCommand(this); the voice list from +0x40 (cursor
// +0x48): voices with bit 5 of byte +0xac get Voice::update(+0x34); +0x34 = 0; 0. neg 1: +0x34 is not cleared.
void a_Device_serviceVoices(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 self = a[0];
  th(c, kThDispatchCommand, {self});
  u32 n = ld<u32>(c, self + 0x40);
  st<u32>(c, self + 0x48, n);
  if (n) {
    u32 v = ld<u32>(c, n + 8);
    while (v) {
      if (ld<uint8_t>(c, v + 0xac) & 0x20) th(c, kThVoiceUpdate, {v, ld<u32>(c, self + 0x34)});
      n = ld<u32>(c, self + 0x48);
      if (!n) break;
      n = ld<u32>(c, n + 4);
      st<u32>(c, self + 0x48, n);
      if (!n) break;
      v = ld<u32>(c, n + 8);
    }
  }
  if (NEG(Device_serviceVoices) != 1) st<u32>(c, self + 0x34, 0);
  r.set_u32(0);
}

// stubReturn2fA (rva 0xa0c0, emu-only): `mov eax, 0x2f; ret`; stubReturn2fB (rva 0xa100): `mov eax, 0x2f; ret 4`.
// neg 1: 0x2e.
void a_stubReturn2fA(Ctx &, const uint32_t *, Ret &r) { r.set_u32(NEG(stubReturn2fA) == 1 ? 0x2e : 0x2f); }
void a_stubReturn2fB(Ctx &, const uint32_t *, Ret &r) { r.set_u32(NEG(stubReturn2fB) == 1 ? 0x2e : 0x2f); }

// postNotification (rva 0xdbe0, c3dh PostNote_re), thiscall (rec, value): NULL rec returns 0xa; a node (operator new(0x10),
// listNode3Init) {value, rec}; bit 0 of rec's byte +0x1428 set; with bit 5 of this+0x1a4 PostMessageA(rec+0x1420's window,
// 0x7ee, 0, node), else postSoundThreadMessage(g_soundThreadState, 0x7ee, 0, node); 0. neg 1: the message is 0x7ef.
void a_postNotification(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 self = a[0], rec = a[1], value = a[2];
  if (!rec) return r.set_u32(0xa);
  u32 node = snd_new(c, 0x10);
  if (node) node = th(c, kListNode3Init, {node});
  st<u32>(c, node + 4, rec);
  st<u32>(c, node, value);
  st<uint8_t>(c, rec + 0x1428, static_cast<uint8_t>(ld<uint8_t>(c, rec + 0x1428) | 1));
  const u32 msg = NEG(postNotification) == 1 ? 0x7efu : 0x7eeu;
  if ((ld<u32>(c, self + 0x1a4) >> 5) & 1) imp(c, kIatPostMessageA, {ld<u32>(c, rec + 0x1420), msg, 0, node});
  else th(c, kPostSoundThreadMessage, {thread_state_va(c), msg, 0, node});
  r.set_u32(0);
}

// postMidiNotification (rva 0x20b60, c3dh PostMidi_re): bit 8 of +0xac set returns 0; +0x214 |= 0x10; with a sound engine
// whose +0x1a4 has bit 5 PostMessageA(+0x210, 0x7ec, 0, this), else postSoundThreadMessage(g_soundThreadState, 0x7ec, 0,
// this); 0. neg 1: +0x214 |= 0x20.
void a_postMidiNotification(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 self = a[0];
  if (ld<u32>(c, self + 0xac) & 0x100) return r.set_u32(0);
  st<u32>(c, self + 0x214, ld<u32>(c, self + 0x214) | (NEG(postMidiNotification) == 1 ? 0x20u : 0x10u));
  const u32 eng = ld<u32>(c, snd_obj(c, &SoundState::g_soundEngine).va());
  if (eng && ((ld<u32>(c, eng + 0x1a4) >> 5) & 1)) {
    imp(c, kIatPostMessageA, {ld<u32>(c, self + 0x210), 0x7ec, 0, self});
    return r.set_u32(0);
  }
  th(c, kPostSoundThreadMessage, {thread_state_va(c), 0x7ec, 0, self});
  r.set_u32(0);
}

// ================================================================================================ the sound window
// dynInit_soundWindowObj100b49b8 (rva 0x91c0, c3fza): SoundWindowObj_ctor on the global at rva 0xb49b8 through its thunk, then
// _atexit(atexitDtor_100b49b8)'s eax. neg 1: the stub pushed is the global's address.
void a_dynInit_soundWindowObj(Ctx &c, const uint32_t *, Ret &r) {
  th(c, kThSoundWindowObjCtor, {snd_va(c, kSoundWindowObj)});
  r.set_u32(cd(c, kCrtAtexit, {snd_va(c, NEG(dynInit_soundWindowObj) == 1 ? kSoundWindowObj : kAtexitDtorStub)}));
}
// atexitDtor_100b49b8 (rva 0x91f0, c3fu): `mov ecx, <global 0xb49b8>; jmp <SoundWindowObj_dtor thunk>`: the destructor's eax.
// neg 1: ecx = the global + 4.
void a_atexitDtor_100b49b8(Ctx &c, const uint32_t *, Ret &r) {
  r.set_u32(th(c, kThSoundWindowObjDtor, {snd_va(c, kSoundWindowObj) + (NEG(atexitDtor_100b49b8) == 1 ? 4u : 0u)}));
}
// destroySoundWindowObj (rva 0x28630, c3dt SwoDestroy_re), thiscall (flags): the SoundWindowObj_dtor body inline (vtable
// vtbl_SoundWindowObj; a handle at +8 with bit 2 of byte +4: SetWindowLongA(handle, GWL_WNDPROC, +0xc), +0xc = +8 = 0, bit 2
// cleared), operator delete with bit 0 of the flags; this. neg 1: bit 2 stays set.
void a_destroySoundWindowObj(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 self = a[0];
  const u32 h = ld<u32>(c, self + 8);
  st<u32>(c, self, snd_va(c, snd_rva::vtbl_SoundWindowObj));
  if (h && (ld<uint8_t>(c, self + 4) & 4)) {
    imp(c, kIatSetWindowLongA, {h, 0xfffffffcu, ld<u32>(c, self + 0xc)});
    uint8_t al = ld<uint8_t>(c, self + 4);
    st<u32>(c, self + 0xc, 0);
    if (NEG(destroySoundWindowObj) != 1) al &= 0xfb;
    st<u32>(c, self + 8, 0);
    st<uint8_t>(c, self + 4, al);
  }
  if (a[1] & 1) snd_delete(c, self);
  r.set_u32(self);
}

// pumpWaveMessages (rva 0x28810, c3fe PumpWave_re): PeekMessageA(&msg, +8, 0x7e8, 0x7ef, PM_REMOVE) while non-zero, each
// message to soundWindowSubclassProc (stdcall); the window re-read for every peek; 0. The MSG is at entry-0x1c (`sub esp,
// 0x1c`, 0x28810; `lea ecx, [esp+0x14]` after three pushes and the two registers). neg 1: the filter's last message is 0x7ee.
void a_pumpWaveMessages(Ctx &c, const uint32_t *a, Ret &r) {
  SndFrame l(c, 0x1c);
  const u32 msg = l.at(0x1c);
  const u32 last = NEG(pumpWaveMessages) == 1 ? 0x7eeu : 0x7efu;
  while (imp(c, kIatPeekMessageA, {msg, ld<u32>(c, a[0] + 8), 0x7e8, last, 1})) {
    snd_call(c, kThSubclassProc, SndAbi::Stdcall,
             {ld<u32>(c, msg), ld<u32>(c, msg + 4), ld<u32>(c, msg + 8), ld<u32>(c, msg + 0xc)});
  }
  r.set_u32(0);
}

// The node messages free their node {+0 kind, +4 object} with pairClear then operator delete.
void free_node(Ctx &c, u32 node) {
  th(c, kThPairClear, {node});
  snd_delete(c, node);
}
// The message switch shared by soundWindowProc (rva 0x288a0, c3fe WndProc_re, cdecl) and soundWindowSubclassProc (rva 0x28b70,
// c3fl SubProc_re, stdcall); see those hook sources for the instruction addresses. Unhandled messages return `def` (the
// caller's DefWindowProcA / CallWindowProcA). neg (per caller): 0x7eb does not decrement +0x1370.
bool window_message(Ctx &c, u32 msg, u32 wParam, u32 lp, int neg) {
  const uint8_t flags = ld<uint8_t>(c, snd_obj(c, &SoundState::g_midiOutputFlags).va());
  switch (msg) {
    case 0x7ea:
      if (!(flags & 1)) cd(c, kThDeleteSound, {lp});
      return true;
    case 0x7eb: {
      if (flags & 1) return true;
      const u32 ch = wParam;
      u32 f = ld<u32>(c, ch + 0x58);
      if (neg != 1) st<u32>(c, ch + 0x1370, ld<u32>(c, ch + 0x1370) - 1);
      if (f & 2) {
        th(c, kThServiceDecodeRing, {ch});
        f = ld<u32>(c, ch + 0x58);
      }
      st<u32>(c, ch + 0x58, f & 0xfffffbffu);
      return true;
    }
    case 0x7ec:
      vt(c, lp, 0xb8, {});
      return true;
    case 0x7ee: {
      if (flags & 1) return true;
      const u32 node = lp;
      const u32 kind = ld<u32>(c, node);
      const u32 obj = ld<u32>(c, node + 4);
      if (kind == 3) {
        if (vt(c, obj, 0x60, {})) th(c, kThSoundGroupStop, {obj});
        const uint8_t b = ld<uint8_t>(c, obj + 0x1428);
        const uint8_t s58 = ld<uint8_t>(c, obj + 0x58);
        st<uint8_t>(c, obj + 0x1428, static_cast<uint8_t>(b & 0xfe));
        if (!(s58 & 2) && obj) vt(c, obj, 0, {1});
        free_node(c, node);
      } else if (kind == 4) {
        if (obj) vt(c, obj, 0, {1});
        free_node(c, node);
      } else if (kind == 5) {
        th(c, kThSoundGroupRewind, {obj});
        st<uint8_t>(c, obj + 0x1428, static_cast<uint8_t>(ld<uint8_t>(c, obj + 0x1428) & 0xfe));
        free_node(c, node);
      }
      return true;
    }
    case 0x7ef: {
      if (flags & 2) return true;
      const u32 node = lp;
      const u32 kind = ld<u32>(c, node);
      const u32 obj = ld<u32>(c, node + 4);
      if (kind == 3) {
        st<u32>(c, obj + 0x214, ld<u32>(c, obj + 0x214) & 0xffffffbfu);
        free_node(c, node);
        if (obj) vt(c, obj, 0, {1});
      } else if (kind == 4) {
        if (obj) vt(c, obj, 0, {1});
        free_node(c, node);
      } else if (kind == 5) {
        vt(c, obj, 0xb8, {});
        st<u32>(c, obj + 0x214, ld<u32>(c, obj + 0x214) & 0xffffffbfu);
        free_node(c, node);
      }
      return true;
    }
    case 0x7f0:
      if (!lp) return false;
      if (ld<uint8_t>(c, lp + 4) & 2) th(c, kThWaveInRequeue, {lp, wParam});
      return true;
    default:
      return false;
  }
}
void a_soundWindowProc(Ctx &c, const uint32_t *a, Ret &r) {
  if (window_message(c, a[1], a[2], a[3], NEG(soundWindowProc))) return r.set_u32(0);
  r.set_u32(imp(c, kIatDefWindowProcA, {a[0], a[1], a[2], a[3]}));
}
void a_soundWindowSubclassProc(Ctx &c, const uint32_t *a, Ret &r) {
  if (window_message(c, a[1], a[2], a[3], NEG(soundWindowSubclassProc))) return r.set_u32(0);
  r.set_u32(imp(c, kIatCallWindowProcA, {gv(c, kSubclassPrevProc), a[0], a[1], a[2], a[3]}));
}

// DeviceBase_scalarDtor (rva 0x28fb0, c3dj): sub = +0x14; +0 = vt_DeviceBaseIntermediate; a non-NULL sub's slot +0x14;
// operator delete with bit 0; this. DeviceBase_dtorA/B/C (rvas 0x28ff0 / 0x29f60 / 0x2a3b0): the same without the delete.
// neg 1: the vtable is not stored.
void devbase(Ctx &c, u32 self, int neg) {
  const u32 sub = ld<u32>(c, self + 0x14);
  if (neg != 1) st<u32>(c, self, snd_va(c, snd_rva::vt_DeviceBaseIntermediate));
  if (sub) vt(c, sub, 0x14, {});
}
void a_DeviceBase_scalarDtor(Ctx &c, const uint32_t *a, Ret &r) {
  devbase(c, a[0], NEG(DeviceBase_scalarDtor));
  if (a[1] & 1) snd_delete(c, a[0]);
  r.set_u32(a[0]);
}
void a_DeviceBase_dtorA(Ctx &c, const uint32_t *a, Ret &) { devbase(c, a[0], NEG(DeviceBase_dtorA)); }
void a_DeviceBase_dtorB(Ctx &c, const uint32_t *a, Ret &) { devbase(c, a[0], NEG(DeviceBase_dtorB)); }
void a_DeviceBase_dtorC(Ctx &c, const uint32_t *a, Ret &) { devbase(c, a[0], NEG(DeviceBase_dtorC)); }

// ================================================================================================ the sound thread
// stopSoundThreadFull (rva 0x2a8a0, c3dh StopThr_re): with bit 0 of byte +0 set: bit cleared, EnterCriticalSection(+4), bit
// cleared again, SetEvent(+0x24), LeaveCriticalSection, Sleep(0), Enter / Leave, +0x20 = +0x1c = 0. neg 1: no Sleep(0).
void a_stopSoundThreadFull(Ctx &c, const uint32_t *a, Ret &) {
  const u32 self = a[0];
  uint8_t al = ld<uint8_t>(c, self);
  if (!(al & 1)) return;
  const u32 cs = self + 4;
  st<uint8_t>(c, self, static_cast<uint8_t>(al & 0xfe));
  imp(c, kIatEnterCriticalSection, {cs});
  st<uint8_t>(c, self, static_cast<uint8_t>(ld<uint8_t>(c, self) & 0xfe));
  imp(c, kIatSetEvent, {ld<u32>(c, self + 0x24)});
  imp(c, kIatLeaveCriticalSection, {cs});
  if (NEG(stopSoundThreadFull) != 1) imp(c, kIatSleep, {0});
  imp(c, kIatEnterCriticalSection, {cs});
  imp(c, kIatLeaveCriticalSection, {cs});
  st<u32>(c, self + 0x20, 0);
  st<u32>(c, self + 0x1c, 0);
}

// startSignalThread (rva 0x2a920, c3ex SigThread_re): ResetEvent(+0x24); bit 0 of byte +0 set returns ResetEvent's eax with its
// low byte replaced by the flag byte; else bit set, _beginthread(soundThreadPump's thunk, 0, 0) into +0x20 and
// SetThreadPriority(handle, 0)'s eax. neg 1: the priority is 1.
void a_startSignalThread(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 self = a[0];
  const u32 rr = imp(c, kIatResetEvent, {ld<u32>(c, self + 0x24)});
  const uint8_t b = ld<uint8_t>(c, self);
  if (b & 1) return r.set_u32((rr & 0xffffff00u) | b);
  st<uint8_t>(c, self, static_cast<uint8_t>(b | 1));
  const u32 h = cd(c, kCrtBeginThread, {snd_va(c, kThSignalThreadStart), 0, 0});
  st<u32>(c, self + 0x20, h);
  r.set_u32(imp(c, kIatSetThreadPriority, {h, NEG(startSignalThread) == 1 ? 1u : 0u}));
}

// soundThreadPump (rva 0x2a970, c3fe ThreadPump_re), cdecl, no argument: see the hook source. The MSG is at entry-0x1c
// (`sub esp, 0x1c`, 0x2a970). neg 1: the running byte is left 1 at the exits.
void a_soundThreadPump(Ctx &c, const uint32_t *, Ret &r) {
  SndFrame l(c, 0x1c);
  const u32 msg = l.at(0x1c);
  const u32 running = snd_va(c, kSigRunning);
  st<uint8_t>(c, running, 1);
  st<u32>(c, snd_va(c, kSigTid), imp(c, kIatGetCurrentThreadId, {}));
  const u32 ev = snd_va(c, kSigEvent), cs = snd_va(c, kSigCS);
  const uint8_t done = NEG(soundThreadPump) == 1 ? 1 : 0;
  u32 w = imp(c, kIatMsgWait, {1, ev, 0, 0xffffffffu, 0x48});
  if (w == 0) {
    st<uint8_t>(c, running, done);
    return r.set_u32(w);
  }
  for (;;) {
    imp(c, kIatEnterCriticalSection, {cs});
    if (!(ld<uint8_t>(c, snd_va(c, kSigObj)) & 1)) {
      const u32 e = imp(c, kIatLeaveCriticalSection, {cs});
      st<uint8_t>(c, running, done);
      return r.set_u32(e);
    }
    while (imp(c, kIatPeekMessageA, {msg, 0, 0, 0, 1}))
      cd(c, kThWndProc, {ld<u32>(c, msg), ld<u32>(c, msg + 4), ld<u32>(c, msg + 8), ld<u32>(c, msg + 0xc)});
    imp(c, kIatLeaveCriticalSection, {cs});
    w = imp(c, kIatMsgWait, {1, ev, 0, 0xffffffffu, 0x48});
    if (w == 0) {
      st<uint8_t>(c, running, done ? 1 : static_cast<uint8_t>(w));
      return r.set_u32(w);
    }
  }
}

// postSoundThreadMessage (rva 0x2aa90, c3dh PostThr_re), thiscall (a1, a2, a3): bit 0 of byte +0 clear returns 0x15;
// PostThreadMessageA(+0x1c, a1, a2, a3) until non-zero, each failure followed by OutputDebugStringA(the string at rva 0x63e68)
// and Sleep(100) (the original retries forever); 0. neg 1: the not-running result is 0x14.
void a_postSoundThreadMessage(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 self = a[0];
  if (!(ld<uint8_t>(c, self) & 1)) return r.set_u32(NEG(postSoundThreadMessage) == 1 ? 0x14 : 0x15);
  while (!imp(c, kIatPostThreadMessageA, {ld<u32>(c, self + 0x1c), a[1], a[2], a[3]})) {
    imp(c, kIatOutputDebugStringA, {snd_va(c, 0x63e68)});
    imp(c, kIatSleep, {0x64});
  }
  r.set_u32(0);
}

// ================================================================================================ Device_* (c3ex)
// The slot calls of these three bodies are thiscall with no argument unless shown.
u32 dev_create(Ctx &c, u32 slot, u32 a) { return cd(c, kThCreateSound, {slot, a, 1}); }
// Device_prepareSoundFlags (rva 0x2bc60, c3ex PrepFlags_re), thiscall (a): see the hook source. neg 1: flag 0x40 (bit 3) is
// not passed.
void a_Device_prepareSoundFlags(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 self = a[0], arg = a[1];
  const u32 slot = self + 0x40;
  if (!ld<u32>(c, slot)) {
    const u32 e = dev_create(c, slot, arg);
    if (e) return r.set_u32(e);
  }
  u32 f = (ld<uint8_t>(c, self + 0x58) & 1) ? 1u : 0u;
  if (vt(c, self, 0x58, {})) f |= 2;
  const u32 fl = ld<u32>(c, self + 0x58);
  if ((fl & 8) && NEG(Device_prepareSoundFlags) != 1) f |= 0x40;
  if (fl & 0x10) f |= 0x80;
  if (fl & 0x40) f |= 0x400;
  if (fl & 2) f |= 4;
  vt(c, ld<u32>(c, slot), 0x6c, {f});
  if (ld<uint8_t>(c, self + 0x58) & 2) return r.set_u32(0);
  const u32 rr = th(c, kThEnginePlaySound, {self, arg});
  if (rr) {
    cd(c, kThDeleteSound, {ld<u32>(c, slot)});
    st<u32>(c, slot, 0);
    return r.set_u32(rr);
  }
  st<u32>(c, self + 0x64, vt(c, ld<u32>(c, slot), 0xc8, {}));
  vt(c, ld<u32>(c, slot), 0x40, {ld<u32>(c, self + 4)});
  vt(c, ld<u32>(c, slot), 0x9c, {ld<u32>(c, self + 0x5c)});
  vt(c, ld<u32>(c, slot), 0x44, {ld<u32>(c, self + 8)});
  r.set_u32(0);
}
// Device_lazyCreateSound (rva 0x2bd80, c3ex LazyCreate_re): see the hook source. neg 1: flag 0x100 (bit 5) is not passed.
void a_Device_lazyCreateSound(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 self = a[0];
  const u32 arg = ld<u32>(c, self + 0x50);
  if (!arg) return r.set_u32(8);
  const u32 slot = self + 0x40;
  if (!ld<u32>(c, slot)) {
    const u32 e = dev_create(c, slot, arg);
    if (e) return r.set_u32(e);
  }
  u32 f = (ld<uint8_t>(c, self + 0x58) & 1) ? 1u : 0u;
  if (vt(c, self, 0x58, {})) f |= 2;
  const u32 fl = ld<u32>(c, self + 0x58);
  if (fl & 4) f |= 0x11;
  if (fl & 8) {
    f |= 0x40;
    if (!(fl & 0x20)) f |= 1;
  }
  if (fl & 0x10) f |= 0x80;
  if ((fl & 0x20) && NEG(Device_lazyCreateSound) != 1) f |= 0x100;
  if (fl & 0x40) f |= 0x400;
  vt(c, ld<u32>(c, slot), 0x6c, {f});
  const u32 rr = th(c, kThEnginePlaySound, {self, arg});
  if (rr) {
    cd(c, kThDeleteSound, {ld<u32>(c, slot)});
    st<u32>(c, slot, 0);
    return r.set_u32(rr);
  }
  st<u32>(c, self + 0x64, vt(c, ld<u32>(c, slot), 0xc8, {}));
  r.set_u32(0);
}
// Device_createStartQueue (rva 0x2bee0, c3ex CreateStartQ_re): see the hook source. neg 1: +0x44 bit 0 is not set.
void a_Device_createStartQueue(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 self = a[0];
  const u32 arg = ld<u32>(c, self + 0x50);
  if (!arg) return r.set_u32(8);
  const u32 slot = self + 0x40;
  if (!ld<u32>(c, slot)) {
    const u32 e = dev_create(c, slot, arg);
    if (e) return r.set_u32(e);
  }
  u32 f = (ld<uint8_t>(c, self + 0x58) & 1) ? 1u : 0u;
  if (vt(c, self, 0x58, {})) f |= 2;
  vt(c, ld<u32>(c, slot), 0x6c, {f});
  const u32 rr = vt(c, ld<u32>(c, slot), 0x84, {});
  if (rr) return r.set_u32(rr);
  const u32 v = ld<u32>(c, self + 0x44);
  if (v & 1) return r.set_u32(0);
  if (NEG(Device_createStartQueue) != 1) st<u32>(c, self + 0x44, v | 1);
  vt(c, self, 0x7c, {});
  if (ld<u32>(c, self + 0x30)) vt(c, ld<u32>(c, slot), 0x48, {1});
  r.set_u32(0);
}

// delete_sound (rva 0x97d0, export, emu-only row with no hook source: transcribed from its instructions), cdecl (s), `ret`
// after an SEH frame (0x97d0-0x97de; its handler only covers operator new). A NULL s returns 0 (0x97f1). Then by the flag
// dword +0x44:
//   bit 2 (0x97f7-0x9801): with g_soundEngine, g_soundTimer and bit 2 of the timer's byte +0x3c (0x9803-0x9821):
//     WaveCmdQueue::pushNode(engine, s, 4) through the thunk 0x20b3 (0x9827-0x9829); otherwise s's slot 0 with 1 (0x993e).
//   bit 5 (0x9833): with g_midiDevice, g_soundTimer and its bit 2 (0x9837-0x984b): the dword +0x210 of the object at +0x30
//     of each non-NULL record among the 0x80 pointers from s+0x744 is decremented (0x984d-0x986e), then
//     CommandQueue::push(device, s, 4, 0, 0) through the thunk 0x15e1 (0x9870-0x987a); otherwise s's slot 0 with 1 (0x9884).
//   neither: s's slot +0x78 (0x9897); bit 3 of its result clear ends (0x989a). With g_soundEngine, g_soundTimer and its bit 2
//     (0x98a2-0x98b8): operator new(0x10) (0x98bc), pairStore(node, 0x11, s) through the thunk 0x1299 on a non-NULL node
//     (0x98d0-0x98d5; NULL gives 0), WaveCmdQueue::pushValue(engine, node or 0) through 0x11c7 (0x98de-0x98ed). Otherwise the
//     sub-object at s+0xf8 (0x98f4): non-NULL, bit 6 of its byte +0xc8 and a non-NULL engine (0x98fa-0x9909):
//     removeSoundNode(engine, sub) through 0x2243 (0x990b), sub+0xc8 = old | 0x10 (0x9911-0x991c), Voice_setLoopFlag(sub+0x70,
//     k) for k = 0..15 through 0x154b (0x991f-0x992b), bit 6 of sub+0xc8 cleared (0x992d-0x9935) and sub's slot 0 with 1
//     (0x9938-0x993c); then (and when those tests fail) s's slot 0 with 1 (0x993e-0x9945).
// Returns 0 on every path (0x994e). neg 1: the 0x80-record loop skips the last record.
constexpr u32 kThWaveCmdPushNode = 0x20b3;   // WaveCmdQueue::pushNode
constexpr u32 kThCmdQueuePush = 0x15e1;      // CommandQueue::push
constexpr u32 kThPairStore = 0x1299;         // pairStore
constexpr u32 kThWaveCmdPushValue = 0x11c7;  // WaveCmdQueue::pushValue
constexpr u32 kThRemoveSoundNode = 0x2243;   // removeSoundNode
constexpr u32 kThVoiceSetLoopFlag = 0x154b;  // Voice_setLoopFlag
bool timer_ready(Ctx &c) {
  const u32 t = ld<u32>(c, timer_va(c));
  return t && (ld<uint8_t>(c, t + 0x3c) & 4);
}
void a_delete_sound(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 s = a[0];
  r.set_u32(0);
  if (!s) return;
  const u32 fl = ld<u32>(c, s + 0x44);
  const u32 eng_va = snd_obj(c, &SoundState::g_soundEngine).va();
  if (fl & 4) {
    const u32 eng = ld<u32>(c, eng_va);
    if (eng && timer_ready(c)) {
      th(c, kThWaveCmdPushNode, {eng, s, 4});
      return;
    }
    vt(c, s, 0, {1});
    return;
  }
  if (fl & 0x20) {
    if (ld<u32>(c, midi_va(c)) && timer_ready(c)) {
      const u32 n = NEG(delete_sound) == 1 ? 0x7fu : 0x80u;
      for (u32 k = 0; k < n; ++k) {
        const u32 rec = ld<u32>(c, s + 0x744 + 4 * k);
        if (!rec) continue;
        const u32 o = ld<u32>(c, rec + 0x30);
        if (o) st<u32>(c, o + 0x210, ld<u32>(c, o + 0x210) - 1);
      }
      th(c, kThCmdQueuePush, {ld<u32>(c, midi_va(c)), s, 4, 0, 0});
      return;
    }
    vt(c, s, 0, {1});
    return;
  }
  if (!(vt(c, s, 0x78, {}) & 8)) return;
  const u32 eng = ld<u32>(c, eng_va);
  if (eng && timer_ready(c)) {
    const u32 p = snd_new(c, 0x10);
    const u32 node = p ? th(c, kThPairStore, {p, 0x11, s}) : 0;
    th(c, kThWaveCmdPushValue, {ld<u32>(c, eng_va), node});
    return;
  }
  const u32 sub = ld<u32>(c, s + 0xf8);
  if (sub && (ld<uint8_t>(c, sub + 0xc8) & 0x40) && eng) {
    th(c, kThRemoveSoundNode, {eng, sub});
    const u32 v = ld<u32>(c, sub + 0xc8) | 0x10;
    st<u32>(c, sub + 0xc8, v);
    for (u32 k = 0; k < 16; ++k) th(c, kThVoiceSetLoopFlag, {sub + 0x70, k});
    st<u32>(c, sub + 0xc8, ld<u32>(c, sub + 0xc8) & ~0x40u);
    vt(c, sub, 0, {1});
  }
  vt(c, s, 0, {1});
}

#define G(v) &g_negctl_##v
const FuncDef kFuncs[] = {
    {"snd.initCtrlVtable", "thiscall", "void", 1, 0, a_initCtrlVtable, G(initCtrlVtable)},
    {"snd.ctrlScalarDeletingDtor", "thiscall", "pointer", 2, 0, a_ctrlScalarDeletingDtor, G(ctrlScalarDeletingDtor)},
    {"snd.CommandQueue::init", "thiscall", "pointer", 1, 0, a_CommandQueue_init, G(CommandQueue_init)},
    {"snd.CommandQueue::clear", "thiscall", "uint32", 1, 0, a_CommandQueue_clear, G(CommandQueue_clear)},
    {"snd.CommandQueue::at", "thiscall", "uint32", 2, 0, a_CommandQueue_at, G(CommandQueue_at)},
    {"snd.dynInit_soundWindowObj100b49b8", "default", "int", 0, 0, a_dynInit_soundWindowObj, G(dynInit_soundWindowObj)},
    {"snd.atexitDtor_100b49b8", "thiscall", "uint32", 1, 0, a_atexitDtor_100b49b8, G(atexitDtor_100b49b8)},
    {"snd.init_sound_timer", "default", "uint32", 2, 0, a_init_sound_timer, G(init_sound_timer)},
    {"snd.get_sound_version", "default", "uint32", 0, 0, a_get_sound_version, G(get_sound_version)},
    {"snd.release_sound", "default", "uint32", 0, 0, a_release_sound, G(release_sound)},
    {"snd.delete_sound", "default", "uint32", 1, 0, a_delete_sound, G(delete_sound)},
    {"snd.Dll_Midi_Device::create_device", "default", "uint32", 1, 0, a_Midi_create_device, G(Midi_create_device)},
    {"snd.Dll_Midi_Device::delete_device", "default", "uint32", 0, 0, a_Midi_delete_device, G(Midi_delete_device)},
    {"snd.Device::serviceVoices", "thiscall", "uint32", 1, 0, a_Device_serviceVoices, G(Device_serviceVoices)},
    {"snd.stubReturn2fA", "default", "uint32", 0, 0, a_stubReturn2fA, G(stubReturn2fA)},
    {"snd.stubReturn2fB", "thiscall", "uint32", 2, 0, a_stubReturn2fB, G(stubReturn2fB)},
    {"snd.registerTimerClient", "thiscall", "uint32", 1, 0, a_registerTimerClient, G(registerTimerClient)},
    {"snd.CommandQueue::push", "thiscall", "uint32", 5, 0, a_CommandQueue_push, G(CommandQueue_push)},
    {"snd.postNotification", "thiscall", "uint32", 3, 0, a_postNotification, G(postNotification)},
    {"snd.dynInit_ctrl100b4b50", "default", "int", 0, 0, a_dynInit_ctrl100b4b50, G(dynInit_ctrl100b4b50)},
    {"snd.dynInit_ctrl100b4c38", "default", "int", 0, 0, a_dynInit_ctrl100b4c38, G(dynInit_ctrl100b4c38)},
    {"snd.dynInit_ctrl100b4d20", "default", "int", 0, 0, a_dynInit_ctrl100b4d20, G(dynInit_ctrl100b4d20)},
    {"snd.dynInit_ctrl100b4a68", "default", "int", 0, 0, a_dynInit_ctrl100b4a68, G(dynInit_ctrl100b4a68)},
    {"snd.dynInit_ctrl100b4e08", "default", "int", 0, 0, a_dynInit_ctrl100b4e08, G(dynInit_ctrl100b4e08)},
    {"snd.Ctrl_appendIfAccepted", "thiscall", "uint32", 2, 0, a_Ctrl_appendIfAccepted, G(Ctrl_appendIfAccepted)},
    {"snd.CtrlObject::ctor", "thiscall", "pointer", 1, 0, a_CtrlObject_ctor, G(CtrlObject_ctor)},
    {"snd.CtrlObject2_ctor", "thiscall", "pointer", 1, 0, a_CtrlObject2_ctor, G(CtrlObject2_ctor)},
    {"snd.Ctrl5b5c0_ctor", "thiscall", "pointer", 1, 0, a_Ctrl5b5c0_ctor, G(Ctrl5b5c0_ctor)},
    {"snd.Ctrl5b19c_ctor", "thiscall", "uint32", 14, 0, a_Ctrl5b19c_ctor, G(Ctrl5b19c_ctor)},
    {"snd.Ctrl5b58c_dtor", "thiscall", "void", 1, 0, a_Ctrl5b58c_dtor, G(Ctrl5b58c_dtor)},
    {"snd.Ctrl5b5c0_transposeTable", "thiscall", "uint32", 2, 0, a_Ctrl5b5c0_transposeTable, G(Ctrl5b5c0_transposeTable)},
    {"snd.CtrlBase_resetElements", "thiscall", "void", 1, 0, a_CtrlBase_resetElements, G(CtrlBase_resetElements)},
    {"snd.CtrlBase_flushElements", "thiscall", "void", 1, 0, a_CtrlBase_flushElements, G(CtrlBase_flushElements)},
    {"snd.Ctrl5b5c0_mapValue", "thiscall", "int", 2, 0, a_Ctrl5b5c0_mapValue, G(Ctrl5b5c0_mapValue)},
    {"snd.CtrlObject_scalarDtorA", "thiscall", "uint32", 2, 0, a_CtrlObject_scalarDtorA, G(CtrlObject_scalarDtorA)},
    {"snd.CtrlObject_dtorA", "thiscall", "void", 1, 0, a_CtrlObject_dtorA, G(CtrlObject_dtorA)},
    {"snd.CtrlObject_scalarDtorB", "thiscall", "pointer", 2, 0, a_CtrlObject_scalarDtorB, G(CtrlObject_scalarDtorB)},
    {"snd.CtrlObject_dtorB", "thiscall", "void", 1, 0, a_CtrlObject_dtorB, G(CtrlObject_dtorB)},
    {"snd.Ctrl5b58c_scalarDtor", "thiscall", "pointer", 2, 0, a_Ctrl5b58c_scalarDtor, G(Ctrl5b58c_scalarDtor)},
    {"snd.Ctrl5b58c_dtorEmbedded", "thiscall", "void", 1, 0, a_Ctrl5b58c_dtorEmbedded, G(Ctrl5b58c_dtorEmbedded)},
    {"snd.CtrlObject3_ctor", "thiscall", "pointer", 1, 0, a_CtrlObject3_ctor, G(CtrlObject3_ctor)},
    {"snd.CtrlObject_scalarDtorC", "thiscall", "pointer", 2, 0, a_CtrlObject_scalarDtorC, G(CtrlObject_scalarDtorC)},
    {"snd.CtrlObject_dtorC", "thiscall", "void", 1, 0, a_CtrlObject_dtorC, G(CtrlObject_dtorC)},
    {"snd.Ctrl5b668_ctor", "thiscall", "pointer", 1, 0, a_Ctrl5b668_ctor, G(Ctrl5b668_ctor)},
    {"snd.CtrlObject_scalarDtorD", "thiscall", "pointer", 2, 0, a_CtrlObject_scalarDtorD, G(CtrlObject_scalarDtorD)},
    {"snd.CtrlObject_dtorD", "thiscall", "void", 1, 0, a_CtrlObject_dtorD, G(CtrlObject_dtorD)},
    {"snd.postMidiNotification", "thiscall", "uint32", 1, 0, a_postMidiNotification, G(postMidiNotification)},
    {"snd.destroySoundWindowObj", "thiscall", "uint32", 2, 0, a_destroySoundWindowObj, G(destroySoundWindowObj)},
    {"snd.pumpWaveMessages", "thiscall", "uint32", 1, 0, a_pumpWaveMessages, G(pumpWaveMessages)},
    {"snd.soundWindowProc", "default", "uint32", 4, 0, a_soundWindowProc, G(soundWindowProc)},
    {"snd.soundWindowSubclassProc", "stdcall", "uint32", 4, 0, a_soundWindowSubclassProc, G(soundWindowSubclassProc)},
    {"snd.DeviceBase_scalarDtor", "thiscall", "pointer", 2, 0, a_DeviceBase_scalarDtor, G(DeviceBase_scalarDtor)},
    {"snd.DeviceBase_dtorA", "thiscall", "void", 1, 0, a_DeviceBase_dtorA, G(DeviceBase_dtorA)},
    {"snd.DeviceBase_dtorB", "thiscall", "void", 1, 0, a_DeviceBase_dtorB, G(DeviceBase_dtorB)},
    {"snd.DeviceBase_dtorC", "thiscall", "void", 1, 0, a_DeviceBase_dtorC, G(DeviceBase_dtorC)},
    {"snd.stopSoundThreadFull", "thiscall", "void", 1, 0, a_stopSoundThreadFull, G(stopSoundThreadFull)},
    {"snd.startSignalThread", "thiscall", "uint32", 1, 0, a_startSignalThread, G(startSignalThread)},
    {"snd.soundThreadPump", "default", "uint32", 0, 0, a_soundThreadPump, G(soundThreadPump)},
    {"snd.postSoundThreadMessage", "thiscall", "uint32", 4, 0, a_postSoundThreadMessage, G(postSoundThreadMessage)},
    {"snd.Device_prepareSoundFlags", "thiscall", "uint32", 2, 0, a_Device_prepareSoundFlags, G(Device_prepareSoundFlags)},
    {"snd.Device_lazyCreateSound", "thiscall", "uint32", 1, 0, a_Device_lazyCreateSound, G(Device_lazyCreateSound)},
    {"snd.Device_createStartQueue", "thiscall", "uint32", 1, 0, a_Device_createStartQueue, G(Device_createStartQueue)},
};
#undef G

}  // namespace

extern const ModuleDef kModule_audio_dev = {"audio_dev", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim

// Step S4.8 part 1, module audio_ds (log/roadmap/S4/8_notes.md): sound.dll's DirectSound boundary. DsBuffer (the wrapper of
// an IDirectSoundBuffer at +0x60, its 3D / notify interfaces at +0x64 / +0x68 / +0x6c), Ds3DBuffer / Ds3DListener positions,
// DirectSoundDevice (the IDirectSound at +0, the device list at +4, the primary DsBuffer at +0x70) and the module's DsBuffer
// list (SoundState::g_dsBufferList / g_dsBufferListTail; links prev +0xbc, next +0xc0).
//
// Every DirectSound call is a COM method through the interface's vtable (snd_vcall, stdcall with the interface first) and
// every DSOUND / ole32 / WINMM import goes through its IAT slot (snd_import): in the replay the fixture's stand-ins serve them,
// with no host the AudioSink receives them as commands (portsim/sim/audio_sink.h). Locals the original passes by address live
// at the original's stack offsets (SndFrame, portsim/sim/audio.h; S4.8's SndLocals until S4.2b). Bodies are the hook sources' reimplementations (shim/src/re/c3*.cpp, named per
// function) converted by re/tools/snd_hook2port.py and edited; comments cite rvas.
#include <cstdint>
#include <cstring>

#include "portsim/arith.h"
#include "portsim/ptr.h"
#include "portsim/registry.h"
#include "portsim/sim/audio.h"
#include "portsim/sim/audio_dev.h"
#include "portsim/sim/soundstate.h"

namespace portsim {

// ================================================================================================ the DirectSound error map
// mapDsError (rva 0xf480) and its inline copies in DsBuffer::setPrimaryFormat (0xf9f0-0xfa79) and DsBuffer::create
// (0xfc4b-0xfcd3): a result above 0x8878000a (signed) indexes a byte table by result - 0x88780064 (above 0x46 -> 0x2f) whose
// only non-default entries are index 0 -> 0x12, 0x14 -> 1, 0x32 -> 0x1e, 0x46 -> 0x13; 0x8878000a -> 0xc; above 0x8007000e
// (signed) only 0x80070057 -> 0xa; 0x8007000e -> 0x11; 0x80004001 -> 0xb; anything else 0x2f.
uint32_t snd_ds_error(uint32_t hr) {
  const int32_t c = static_cast<int32_t>(hr);
  if (c > static_cast<int32_t>(0x8878000au)) {
    const uint32_t idx = hr - 0x88780064u;
    if (idx > 0x46) return 0x2f;
    switch (idx) {
      case 0x00: return 0x12;
      case 0x14: return 1;
      case 0x32: return 0x1e;
      case 0x46: return 0x13;
      default: return 0x2f;
    }
  }
  if (hr == 0x8878000au) return 0xc;
  if (c > static_cast<int32_t>(0x8007000eu)) return hr == 0x80070057u ? 0xa : 0x2f;
  if (hr == 0x8007000eu) return 0x11;
  if (hr == 0x80004001u) return 0xb;
  return 0x2f;
}

namespace {
using u32 = uint32_t;

// ---- negative-control switches (one per function)
#define DS_NEG(v) int g_negctl_##v = 0;
DS_NEG(DsBuffer_ctor) DS_NEG(DsBuffer_destroy) DS_NEG(DsBuffer_restartPlayback) DS_NEG(DsBuffer_setBufferSize)
DS_NEG(DsBuffer_setFrequency) DS_NEG(DsBuffer_setVolumeRaw) DS_NEG(DsBuffer_setVolume) DS_NEG(mapDsError)
DS_NEG(DsBuffer_initDesc) DS_NEG(DsBuffer_stop) DS_NEG(DsBuffer_setPosition) DS_NEG(DsBuffer_lockEntire)
DS_NEG(DsBuffer_release) DS_NEG(DsBuffer_setPrimaryFormat) DS_NEG(DsBuffer_create) DS_NEG(DsBuffer_hasAuxInterfaces)
DS_NEG(DsBuffer_query3D) DS_NEG(DsBuffer_queryNotify) DS_NEG(DsBuffer_releaseIfOwned) DS_NEG(DsBuffer_restore)
DS_NEG(DsBuffer_unlock) DS_NEG(DsBuffer_getPosition) DS_NEG(DsBuffer_getPlayPosition) DS_NEG(DsBuffer_lockCaptureRegion)
DS_NEG(DsBuffer_getTimedPosition) DS_NEG(DsBuffer_play) DS_NEG(DsBuffer_isBusy) DS_NEG(DsBuffer_setRegionMs)
DS_NEG(DsBuffer_configureDesc) DS_NEG(Ds3DBuffer_setPosition) DS_NEG(Ds3DBuffer_setPositionX) DS_NEG(Ds3DBuffer_setPositionY)
DS_NEG(Ds3DBuffer_setPositionZ) DS_NEG(DsDev_close) DS_NEG(DsDev_enumerate) DS_NEG(getDeviceCaps)
DS_NEG(dsEnumCallbackAppend) DS_NEG(DsDev_releaseAll) DS_NEG(DsDev_stopAll) DS_NEG(DsDev_create) DS_NEG(return0Pop4)
DS_NEG(DsDev_setCooperativeLevel) DS_NEG(DsDev_duplicateBuffer) DS_NEG(DsDev_freeBufferById)
DS_NEG(DsBuffer_scalarDeletingDtor) DS_NEG(DsDev_createBuffer) DS_NEG(DsDev_createBufferFmt) DS_NEG(DsDev_removeBuffer)
DS_NEG(DsDev_releaseAllBuffers) DS_NEG(unlinkDsBuffer) DS_NEG(DsDev_setMasterVolume) DS_NEG(Ds3DListener_setPosition)
DS_NEG(DsDevice_registerBuffer) DS_NEG(getDsCoopLevel)
#undef DS_NEG
#define NEG(v) SG_NEGCTL(g_negctl_##v)

// ---- imports (IAT slot rvas of the user's sound.dll; the dispatcher calls the slot's value)
constexpr u32 kIatTimeGetTime = 0xb8654;           // WINMM!timeGetTime
constexpr u32 kIatCoInitialize = 0xb86b8;          // ole32!CoInitialize
constexpr u32 kIatCoCreateInstance = 0xb86bc;      // ole32!CoCreateInstance
constexpr u32 kIatCoUninitialize = 0xb86c0;        // ole32!CoUninitialize
constexpr u32 kStubDirectSoundEnumerateA = 0x3a978;  // `jmp [iat 0xb83b4]` = DSOUND!#2 (DirectSoundEnumerateA)

// ---- callees by thunk rva (incremental-link thunks: the dispatcher follows them to the body)
constexpr u32 kThReleaseAll = 0x104b;        // DirectSoundDevice::releaseAll
constexpr u32 kThGetTimedPosition = 0x115e;  // DsBuffer::getTimedPosition
constexpr u32 kThReleaseIfOwned = 0x12d5;    // DsBuffer::releaseIfOwned
constexpr u32 kThRegisterBuffer = 0x1320;    // DsDevice_registerBuffer
constexpr u32 kThRelease = 0x144c;           // DsBuffer::release
constexpr u32 kThFindUnlink = 0x15a0;        // findAndUnlinkNode
constexpr u32 kThAppendNode = 0x1681;        // appendNode
constexpr u32 kThGetPlayPosition = 0x1695;   // DsBuffer::getPlayPosition
constexpr u32 kThListAppendValue = 0x16ae;   // nodeListAppendValue
constexpr u32 kThQueryNotify = 0x1762;       // DsBuffer::queryNotify
constexpr u32 kThEnumCallback = 0x17a3;      // dsEnumCallbackAppend (passed to DirectSoundEnumerateA)
constexpr u32 kThDestroy = 0x1910;           // DsBuffer::destroy
constexpr u32 kThConfigureDesc = 0x1b4a;     // DsBuffer::configureDesc
constexpr u32 kThIsBusy = 0x1b9f;            // DsBuffer::isBusy
constexpr u32 kThCreate = 0x1bfe;            // DsBuffer::create
constexpr u32 kThCtor = 0x1c1c;              // DsBuffer::ctor
constexpr u32 kThQuery3D = 0x1c30;           // DsBuffer::query3D
constexpr u32 kThSetBufferSize = 0x1c35;     // DsBuffer::setBufferSize
constexpr u32 kThReleaseBuffers = 0x1d11;    // Channel_releaseBuffers
constexpr u32 kThSetPrimaryFormat = 0x1d75;  // DsBuffer::setPrimaryFormat
constexpr u32 kThParseRiffWave = 0x1e51;     // parseRiffWave
constexpr u32 kThUnlinkDsBuffer = 0x1ebf;    // unlinkDsBuffer
constexpr u32 kThVoiceListRemove = 0x1ec4;   // voiceListRemove
constexpr u32 kThHasAux = 0x2081;            // DsBuffer::hasAuxInterfaces
constexpr u32 kThScalarDtor = 0x2252;        // DsBuffer::scalarDeletingDtor
constexpr u32 kThDuplicate = 0x2572;         // DirectSoundDevice::duplicateBuffer
constexpr u32 kThMapDsError = 0x268a;        // mapDsError
constexpr u32 kInitDesc = 0xf590;            // DsBuffer::initDesc (called directly)
constexpr u32 kThSetVolume = 0x18f7;         // DsBuffer::setVolume
constexpr u32 kMapDsError = 0xf480;          // mapDsError (called directly by Ds3DListener::setPosition)
// unnamed .data globals (no names row): the device index enumerate selects, the enumerated-device counter
constexpr u32 kDsDeviceIndexRva = 0xb4a40;   // read by DirectSoundDevice::enumerate (0x1155c)
constexpr u32 kDsDeviceCountRva = 0xb4a44;   // incremented by dsEnumCallbackAppend (0x118fc-0x11904)

Ret com(Ctx &c, u32 iface, u32 slot, std::initializer_list<uint32_t> args) {
  return snd_vcall(c, iface, slot, SndAbi::Stdcall, args);
}
u32 th(Ctx &c, u32 rva, std::initializer_list<uint32_t> args) { return snd_call(c, rva, SndAbi::Thiscall, args).eax; }

u32 ds_head_va(Ctx &c) { return snd_obj(c, &SoundState::g_dsBufferList).va(); }
u32 ds_tail_va(Ctx &c) { return snd_obj(c, &SoundState::g_dsBufferListTail).va(); }
u32 ds_head(Ctx &c) { return ld<u32>(c, ds_head_va(c)); }
u32 ds_tail(Ctx &c) { return ld<u32>(c, ds_tail_va(c)); }

// Unlink b from the DsBuffer list: prev's next (or the head) = next, next's prev (or the tail) = prev, b's links cleared. The
// same instructions at 0x133ea-0x13421 (releaseAllBuffers), 0x1331f-0x1335a (removeBuffer), 0x11981-0x119b8 (releaseAll),
// 0x11ab6-0x11aeb (stopAll), 0x12784-0x127c2 (freeBufferById); neg: the tail is not updated.
void ds_unlink(Ctx &c, u32 b, int neg) {
  const u32 p = ld<u32>(c, b + 0xbc);
  const u32 n = ld<u32>(c, b + 0xc0);
  if (p) st<u32>(c, p + 0xc0, n); else st<u32>(c, ds_head_va(c), n);
  if (n) st<u32>(c, n + 0xbc, p); else if (neg != 1) st<u32>(c, ds_tail_va(c), p);
  st<u32>(c, b + 0xbc, 0);
  st<u32>(c, b + 0xc0, 0);
}
// Append b at the list's tail (createBuffer 0x12f8a-0x12fd4 shape; duplicateBuffer 0x1230c-0x12342).
void ds_append(Ctx &c, u32 b) {
  if (u32 t = ds_tail(c)) {
    st<u32>(c, b + 0xbc, t);
    st<u32>(c, ds_tail(c) + 0xc0, b);
    st<u32>(c, ds_tail_va(c), b);
  } else {
    st<u32>(c, ds_head_va(c), b);
    st<u32>(c, ds_tail_va(c), b);
    st<u32>(c, b + 0xbc, 0);
  }
  st<u32>(c, b + 0xc0, 0);
}
void destroy_delete(Ctx &c, u32 b) {
  th(c, kThDestroy, {b});
  snd_delete(c, b);
}

// ================================================================================================ DsBuffer
// DsBuffer::ctor (rva 0xeef0, c3do DsBufferCtor_re): bit 0 of +0 cleared, bit 0 of byte +4 set, +0x5c = +0x44 = 0,
// DsBuffer::initDesc (0xef0d), then +0xc8 +0xcc +0xd0 +0x64 +0x68 +0x4c +0x48 +0x50 +0x54 +0x58 = 0; returns this.
// neg 1: bit 0 of byte +4 is not set (an earlier mode, +0x68 not cleared, was an equivalent mutant: initDesc clears it too).
void a_DsBuffer_ctor(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 self = a[0];
  const u32 v = ld<u32>(c, self) & 0xfffffffeu;
  const uint8_t b = static_cast<uint8_t>(ld<uint8_t>(c, self + 4) | (NEG(DsBuffer_ctor) == 1 ? 0 : 1));
  st<u32>(c, self, v);
  st<uint8_t>(c, self + 4, b);
  st<u32>(c, self + 0x5c, 0);
  st<u32>(c, self + 0x44, 0);
  th(c, kInitDesc, {self});
  for (u32 off : {0xc8u, 0xccu, 0xd0u, 0x64u, 0x68u, 0x4cu, 0x48u, 0x50u, 0x54u, 0x58u})
    st<u32>(c, self + off, 0);
  r.set_u32(self);
}

// The release block shared by DsBuffer::destroy (0xef6d-0xefe2), DsBuffer::release (0xf8fd-0xf95d) and
// DsBuffer::releaseIfOwned (0xff7a-0x100ea) (c3dj c3dj_dsReleaseBlock): Stop (slot +0x48) on +0x60; with +0x64 non-NULL the
// interface at +0x6c is released first when non-NULL (slot +8, +0x6c = 0) and then +0x64 (slot +8, +0x64 = 0); +0x60 is
// released (slot +8); the five dwords at +0x94 and +8 +0xc +0x60 +0x4c +0x48 are cleared.
void release_block(Ctx &c, u32 o, int neg) {
  com(c, ld<u32>(c, o + 0x60), 0x48, {});
  if (ld<u32>(c, o + 0x64)) {
    if (u32 c6c = ld<u32>(c, o + 0x6c)) {
      com(c, c6c, 8, {});
      st<u32>(c, o + 0x6c, 0);
    }
    com(c, ld<u32>(c, o + 0x64), 8, {});
    st<u32>(c, o + 0x64, 0);
  }
  if (neg != 1) com(c, ld<u32>(c, o + 0x60), 8, {});
  for (u32 i = 0; i < 5; ++i) st<u32>(c, o + 0x94 + 4 * i, 0);
  for (u32 off : {8u, 0xcu, 0x60u, 0x4cu, 0x48u}) st<u32>(c, o + off, 0);
}

// DsBuffer::destroy (rva 0xef60, c3dj DsDestroy_re): only with +0x60 and +0x5c both non-NULL (0xef69 / 0xef6d). neg 1: the
// +0x5c test is dropped.
void a_DsBuffer_destroy(Ctx &c, const uint32_t *a, Ret &) {
  const u32 o = a[0];
  if (!ld<u32>(c, o + 0x60)) return;
  if (NEG(DsBuffer_destroy) != 1 && !ld<u32>(c, o + 0x5c)) return;
  release_block(c, o, 0);
}

// DsBuffer::restartPlayback (rva 0xf020, c3dh Restart_re): +0x4c = +0x48 = 0; Stop when +0x60 is non-NULL; with +0x60 still
// non-NULL +0x54 = timeGetTime(), Play(0, 0, 1) (slot +0x30); a non-zero result is returned; else SetFormat-like slot +0x14
// (this+0xa8, 0x12, 0) and its result. neg 1: +0x54 is not stored.
void a_DsBuffer_restartPlayback(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 self = a[0];
  u32 o = ld<u32>(c, self + 0x60);
  st<u32>(c, self + 0x4c, 0);
  st<u32>(c, self + 0x48, 0);
  if (o) com(c, o, 0x48, {});
  if (!ld<u32>(c, self + 0x60)) return r.set_u32(0);
  const u32 t = snd_import(c, kIatTimeGetTime, SndAbi::Stdcall, {}).eax;
  if (NEG(DsBuffer_restartPlayback) != 1) st<u32>(c, self + 0x54, t);
  const u32 hr = com(c, ld<u32>(c, self + 0x60), 0x30, {0, 0, 1}).eax;
  if (hr) return r.set_u32(hr);
  r.set_u32(com(c, ld<u32>(c, self + 0x60), 0x14, {self + 0xa8, 0x12, 0}).eax);
}

// DsBuffer::setBufferSize (rva 0xf0c0, c3an DsBufSetSize_re): 0xc with a buffer at +0x60, else +0x78 = size, 0.
// neg 1: the size is stored even with a buffer.
void a_DsBuffer_setBufferSize(Ctx &c, const uint32_t *a, Ret &r) {
  if (ld<u32>(c, a[0] + 0x60)) {
    if (NEG(DsBuffer_setBufferSize) == 1) st<u32>(c, a[0] + 0x78, a[1]);
    return r.set_u32(0xc);
  }
  st<u32>(c, a[0] + 0x78, a[1]);
  r.set_u32(0);
}

// DsBuffer::setFrequency (rva 0xf160, c3ca): SetFrequency (slot +0x44) with the argument when +0x60 is non-NULL; eax is the
// method's (or the NULL interface) value. neg 1: the frequency + 1 is passed.
void a_DsBuffer_setFrequency(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 o = ld<u32>(c, a[0] + 0x60);
  if (!o) return r.set_u32(0);
  r.set_u32(com(c, o, 0x44, {a[1] + (NEG(DsBuffer_setFrequency) == 1 ? 1u : 0u)}).eax);
}

// DsBuffer::setVolumeRaw (rva 0xf190, c3ca): +0xc4 = volume; SetVolume (slot +0x3c) when +0x60 is non-NULL. neg 1: +0xc4 is
// not stored.
void a_DsBuffer_setVolumeRaw(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 o = ld<u32>(c, a[0] + 0x60);
  if (NEG(DsBuffer_setVolumeRaw) != 1) st<u32>(c, a[0] + 0xc4, a[1]);
  if (!o) return r.set_u32(0);
  r.set_u32(com(c, o, 0x3c, {a[1]}).eax);
}

// DsBuffer::setVolume (rva 0xf1c0, c3fh DsSetVolume_re): v = min(volume, 0x7f) (unsigned, 0xf1ca); m = the master's +0x188
// (master at +0x5c) or 0x7f; e = __ftol(m * k * (v * k) * 127.0) with k the double at rva 0x5b488 (1/127) and 127.0 at
// 0x5b478; e != 0: e = __ftol(ln(e * k) * 10.0 * 100.0) (0x5b468, 0x5b458; fldln2 / fyl2x 0xf23b-0xf23f), else e = -10000
// (0xf254). +0xc4 = e; with +0x60 non-NULL SetVolume (slot +0x3c) with e and its result is returned, else e (eax).
// The x87 instructions on the core's unit under the caller's word (S4.2b): fild qword m (0xf1f4, the high dword 0), fmul
// qword k (0xf204), fild qword v (0xf20a), fmul k (0xf20e), fmulp st(1) (0xf214), fmul qword 127.0 (0xf216), __ftol (0xf21c,
// through the dispatcher: it pops); fild qword e (0xf231), fmul k (0xf235), fldln2 (0xf23b), fxch st(1) (0xf23d), fyl2x
// (0xf23f: the reference emulator's, portsim/sim/audio.h), fmul 10.0 (0xf241), fmul 100.0 (0xf247), __ftol (0xf24d).
// neg 1: the silent volume is -9999 instead of -10000 (0xf254). neg 2: every step at 0x027f whatever the caller's word.
// neg 3: the first product alone at 0x027f (blind on the 30 vectors: see 8_notes.md).
void a_DsBuffer_setVolume(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 self = a[0];
  u32 vol = a[1];
  if (vol > 0x7f) vol = 0x7f;
  const u32 master = ld<u32>(c, self + 0x5c);
  const u32 m = master ? ld<u32>(c, master + 0x188) : 0x7f;
  x87::Unit &u = x87::unit(c);
  const int neg = NEG(DsBuffer_setVolume);
  const uint16_t cw = neg == 2 ? 0x027fu : u.cw;
  const uint16_t cw1 = neg == 3 ? 0x027fu : cw;
  auto rd = [&](u32 rva) { return x87::from_f64(ld<uint64_t>(c, snd_va(c, rva))); };
  uint32_t e = 0;
  SG_X87_GUARD(c, {
    u.push(x87::fild(static_cast<int64_t>(m)));
    u.st(0) = x87::mul(u.st(0), rd(0x5b488), cw1);
    u.push(x87::fild(static_cast<int64_t>(vol)));
    u.st(0) = x87::mul(u.st(0), rd(0x5b488), cw);
    u.st(1) = x87::mul(u.st(1), u.st(0), cw);
    u.pop();
    u.st(0) = x87::mul(u.st(0), rd(0x5b478), cw);
  })
  e = snd_ftol(c).eax;
  if (e) {
    SG_X87_GUARD(c, {
      u.push(x87::fild(static_cast<int64_t>(e)));
      u.st(0) = x87::mul(u.st(0), rd(0x5b488), cw);
      u.push(x87::ln2());
      u.fxch(1);
      u.st(1) = x87::fyl2x(u.st(0), u.st(1), cw);
      u.pop();
      u.st(0) = x87::mul(u.st(0), rd(0x5b468), cw);
      u.st(0) = x87::mul(u.st(0), rd(0x5b458), cw);
    })
    e = snd_ftol(c).eax;
  } else {
    e = neg == 1 ? 0xffffd8f1u : 0xffffd8f0u;
  }
  const u32 buf = ld<u32>(c, self + 0x60);
  st<u32>(c, self + 0xc4, e);
  if (!buf) return r.set_u32(e);
  r.set_u32(com(c, buf, 0x3c, {e}).eax);
}

// mapDsError (rva 0xf480, c3au MapDsError_re), cdecl (hr). neg 1: 0x88780096 maps to 0x2f.
void a_mapDsError(Ctx &c, const uint32_t *a, Ret &r) {
  (void)c;
  if (NEG(mapDsError) == 1 && a[0] == 0x88780096u) return r.set_u32(0x2f);
  r.set_u32(snd_ds_error(a[0]));
}

// DsBuffer::initDesc (rva 0xf590, c3do DsBufferInitDesc_re): the DSBUFFERDESC at +0x70 (size 0x24, flags 0xe8, bytes 0x10000,
// format pointer +0x80 = this+0xa8), the WAVEFORMATEX at +0xa8 (PCM, 2 channels, 22050 Hz, 0x15888 bytes/s, align 4, 16 bits),
// region fields from rate * align / 1000 (0xf647-0xf698), every other field cleared; returns 0. neg 1: +0x34 keeps 0x295.
void a_DsBuffer_initDesc(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 self = a[0];
  st<u32>(c, self, ld<u32>(c, self) & 0xfffffffdu);
  for (u32 off = 0x70; off < 0x70 + 9 * 4; off += 4) st<u32>(c, self + off, 0);
  st<u32>(c, self + 0x70, 0x24);
  st<u32>(c, self + 0x74, 0xe8);
  st<u32>(c, self + 0x78, 0x10000);
  st<u32>(c, self + 0x7c, 0);
  st<u32>(c, self + 0x80, self + 0xa8);
  for (u32 off = 0xa8; off < 0xb8; off += 4) st<u32>(c, self + off, 0);
  st<uint16_t>(c, self + 0xb8, 0);
  st<uint16_t>(c, self + 0xa8, 1);
  st<uint16_t>(c, self + 0xaa, 2);
  st<u32>(c, self + 0xac, 0x5622);
  st<u32>(c, self + 0xb0, 0x15888);
  st<uint16_t>(c, self + 0xb4, 4);
  st<uint16_t>(c, self + 0xb6, 0x10);
  st<uint16_t>(c, self + 0xb8, 0);
  for (u32 off : {0x40u, 0x14u, 0x18u, 0x1cu, 0xcu, 0x44u}) st<u32>(c, self + off, 0);
  for (u32 off = 0x94; off < 0x94 + 5 * 4; off += 4) st<u32>(c, self + off, 0);
  u32 eax = ld<u32>(c, self + 0xac);
  u32 edi = ld<uint16_t>(c, self + 0xb4);
  const u32 ebx = ld<u32>(c, self + 0x78);
  eax = eax + eax * 2;
  st<u32>(c, self + 0x10, 0);
  st<u32>(c, self + 0x24, 0);
  st<u32>(c, self + 0x2c, 0);
  u32 edx = eax + eax * 4;
  edx <<= 1;
  edx = static_cast<u32>((static_cast<uint64_t>(edx) * 0x10624dd3u) >> 32) >> 6;  // / 1000
  edi = edi * edx;
  st<u32>(c, self + 0x30, 0);
  st<u32>(c, self + 0x60, 0);
  eax = edi + edi * 8;
  st<u32>(c, self + 0x34, 0x295);
  eax <<= 1;
  if (!(eax > ebx) && NEG(DsBuffer_initDesc) != 1) {
    st<u32>(c, self + 0x38, eax);
    eax -= edi;
    eax >>= 1;
    st<u32>(c, self + 0x40, edi);
    st<u32>(c, self + 0x34, edx);
    st<u32>(c, self + 0x3c, eax);
  }
  for (u32 off : {0x5cu, 0xc0u, 0xbcu, 0x64u, 0x68u, 0x6cu, 0x20u, 0xc4u, 0x3cu}) st<u32>(c, self + off, 0);
  r.set_u32(0);
}

// DsBuffer::stop (rva 0xf760, c3dj DsStop_re): 2 with no buffer, else Stop (slot +0x48) and 0. neg 1: returns 1.
void a_DsBuffer_stop(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 o = ld<u32>(c, a[0] + 0x60);
  if (!o) return r.set_u32(2);
  com(c, o, 0x48, {});
  r.set_u32(NEG(DsBuffer_stop) == 1 ? 1 : 0);
}

// DsBuffer::setPosition (rva 0xf790, c3dj DsSetPosition_re): with a buffer SetCurrentPosition(0) (slot +0x34) and
// GetCurrentPosition (slot +0x10) into two locals; the cursor fields +0x24..+0x30, +0xc..+0x1c, +0x48, +0x4c cleared, bit 0 of
// byte +4 set; 0. neg 1: +0x30 is not cleared.
void a_DsBuffer_setPosition(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 self = a[0];
  SndFrame l(c, 8);  // `sub esp, 8` (0xf790): play at entry-4, write at entry-8 (0xf7aa / 0xf7af), not initialised
  if (u32 o = ld<u32>(c, self + 0x60)) {
    com(c, o, 0x34, {0});
    com(c, ld<u32>(c, self + 0x60), 0x10, {l.at(4), l.at(8)});
  }
  const uint8_t b = ld<uint8_t>(c, self + 4);
  for (u32 off : {0x24u, 0x28u, 0x2cu, 0x30u, 0xcu, 0x10u, 0x14u, 0x18u, 0x1cu, 0x48u, 0x4cu})
    if (!(NEG(DsBuffer_setPosition) == 1 && off == 0x30)) st<u32>(c, self + off, 0);
  st<uint8_t>(c, self + 4, static_cast<uint8_t>(b | 1));
  r.set_u32(0);
}

// DsBuffer::lockEntire (rva 0xf810, c3do DsLockEntire_re): 0x24 with no buffer; Lock(0, +0x78, &p1, &b1, &p2, &b2, 2) (slot
// +0x2c); a failure returns 0x1a; the b1 bytes at p1 and the b2 bytes at p2 are zeroed; Unlock (slot +0x4c) with the four;
// 0. neg 1: the second region is not cleared.
void a_DsBuffer_lockEntire(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 self = a[0];
  const u32 o = ld<u32>(c, self + 0x60);
  if (!o) return r.set_u32(0x24);
  // `sub esp, 0x10` (0xf810): p1 entry-4, b1 entry-8, p2 entry-0xc, b2 entry-0x10 (0xf821-0xf838), not initialised
  SndFrame l(c, 0x10);
  const u32 hr = com(c, o, 0x2c, {0, ld<u32>(c, self + 0x78), l.at(4), l.at(8), l.at(0xc), l.at(0x10), 2}).eax;
  if (hr) return r.set_u32(0x1a);
  const u32 p1 = ld<u32>(c, l.at(4)), b1 = ld<u32>(c, l.at(8)), p2 = ld<u32>(c, l.at(0xc)), b2 = ld<u32>(c, l.at(0x10));
  for (u32 i = 0; i < b1; ++i) st<uint8_t>(c, p1 + i, 0);
  if (b2 && NEG(DsBuffer_lockEntire) != 1)
    for (u32 i = 0; i < b2; ++i) st<uint8_t>(c, p2 + i, 0);
  com(c, ld<u32>(c, self + 0x60), 0x4c, {p1, b1, p2, b2});
  r.set_u32(0);
}

// DsBuffer::release (rva 0xf8f0, c3dj DsRelease_re): the release block when +0x60 is non-NULL; 0. neg 1: +0x60 is not released.
void a_DsBuffer_release(Ctx &c, const uint32_t *a, Ret &r) {
  if (ld<u32>(c, a[0] + 0x60)) release_block(c, a[0], NEG(DsBuffer_release));
  r.set_u32(0);
}

// DsBuffer::setPrimaryFormat (rva 0xf990, c3do DsSetPrimaryFormat_re): with a buffer, bit 1 of +0 clear returns 6, else
// SetFormat (slot +0x38) and a failure maps through the error table; then (or with no buffer and bit 1 clear) the 0x12-byte
// WAVEFORMATEX is copied to +0xa8 and 0 returned; no buffer with bit 1 set returns 6. neg 1: the last word is not copied.
void a_DsBuffer_setPrimaryFormat(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 self = a[0], fmt = a[1];
  const u32 o = ld<u32>(c, self + 0x60);
  const bool bit1 = (ld<u32>(c, self) & 2) == 2;
  auto copy = [&]() {
    for (u32 i = 0; i < 16; i += 4) st<u32>(c, self + 0xa8 + i, ld<u32>(c, fmt + i));
    if (NEG(DsBuffer_setPrimaryFormat) != 1) st<uint16_t>(c, self + 0xb8, ld<uint16_t>(c, fmt + 0x10));
  };
  if (o) {
    if (!bit1) return r.set_u32(6);
    const u32 hr = com(c, o, 0x38, {fmt}).eax;
    if (hr) return r.set_u32(snd_ds_error(hr));
    copy();
    return r.set_u32(0);
  }
  if (bit1) return r.set_u32(6);
  copy();
  r.set_u32(0);
}

// DsBuffer::create (rva 0xfbb0, c3fd DsCreate_re), thiscall (ds, desc): see the hook source for the full transcription.
// neg 1: +0x50 = desc is not stored (an earlier mode, the notify failure keeping +0x6c, was not reached by the vectors).
void a_DsBuffer_create(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 self = a[0], ds = a[1], desc = a[2];
  if (!desc) return r.set_u32(0xa);
  if ((ld<u32>(c, self) & 2) == 2) {
    st<u32>(c, self + 0x80, 0);
  } else if (!ld<u32>(c, self + 0x80)) {
    st<u32>(c, self + 0x80, self + 0xa8);
  } else {
    const u32 f = ld<u32>(c, self + 0x80);
    for (u32 k = 0; k < 0x10; k += 4) st<u32>(c, self + 0xa8 + k, ld<u32>(c, f + k));
    st<uint16_t>(c, self + 0xb8, ld<uint16_t>(c, f + 0x10));
  }
  if (ld<u32>(c, self + 0x60)) return r.set_u32(0xc);
  const u32 hr = com(c, ds, 0xc, {self + 0x70, self + 0x60, 0}).eax;  // CreateSoundBuffer
  if (hr) return r.set_u32(snd_ds_error(hr));
  if (NEG(DsBuffer_create) != 1) st<u32>(c, self + 0x50, desc);
  const u32 n = ld<u32>(c, self + 0x78);
  st<u32>(c, self + 0x94, 0x14);
  if (n) st<u32>(c, self + 0x58, (~n) % n);
  com(c, ld<u32>(c, self + 0x60), 0xc, {self + 0x94});  // GetCaps
  if ((ld<u32>(c, self) & 2) == 2) {
    com(c, ld<u32>(c, self + 0x60), 0x38, {self + 0xa8});  // SetFormat
    com(c, ld<u32>(c, self + 0x60), 0x30, {0, 0, 1});      // Play looping
  }
  if (ld<uint8_t>(c, self + 0x74) & 0x10) {
    if (u32 buf = ld<u32>(c, self + 0x60)) {
      const int32_t q = static_cast<int32_t>(
          (ld<uint8_t>(c, self) & 2)
              ? com(c, buf, 0, {snd_va(c, snd_rva::IID_IDirectSound3DListener), self + 0x68}).eax
              : com(c, buf, 0, {snd_va(c, snd_rva::IID_IDirectSound3DBuffer), self + 0x64}).eax);
      if (q < 0) {
        st<u32>(c, self + 0x68, 0);
        st<u32>(c, self + 0x64, 0);
      }
    }
    u32 o = ld<u32>(c, self + 0x68);
    if (!o) o = ld<u32>(c, self + 0x64);
    if (o) {
      const int32_t q = static_cast<int32_t>(com(c, o, 0, {snd_va(c, snd_rva::IID_IDirectSoundNotify), self + 0x6c}).eax);
      if (q < 0) st<u32>(c, self + 0x6c, 0);
    }
  }
  com(c, ld<u32>(c, self + 0x60), 0x14, {self + 0xa8, 0x12, 0});
  r.set_u32(0);
}

// DsBuffer::hasAuxInterfaces (rva 0xfec0, c3ar): 1 when +0x64 or +0x68 is non-NULL. neg 1: only +0x64 is tested.
void a_DsBuffer_hasAuxInterfaces(Ctx &c, const uint32_t *a, Ret &r) {
  if (ld<u32>(c, a[0] + 0x64)) return r.set_u32(1);
  if (NEG(DsBuffer_hasAuxInterfaces) != 1 && ld<u32>(c, a[0] + 0x68)) return r.set_u32(1);
  r.set_u32(0);
}

// DsBuffer::query3D (rva 0xfef0, c3dj DsQuery3D_re): 0x14 with no buffer; QueryInterface on +0x60 for the 3D listener
// (bit 1 of byte +0, out +0x68) or 3D buffer interface (out +0x64); negative clears +0x68 and +0x64 and returns 0xb.
// neg 1: the 3D buffer's out-pointer is +0x68 too (an earlier mode, the failure keeping +0x64, read GREEN).
void a_DsBuffer_query3D(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 self = a[0];
  const u32 o = ld<u32>(c, self + 0x60);
  if (!o) return r.set_u32(0x14);
  u32 iid, out;
  if (ld<uint8_t>(c, self) & 2) {
    out = self + 0x68;
    iid = snd_va(c, snd_rva::IID_IDirectSound3DListener);
  } else {
    out = self + (NEG(DsBuffer_query3D) == 1 ? 0x68 : 0x64);
    iid = snd_va(c, snd_rva::IID_IDirectSound3DBuffer);
  }
  const int32_t hr = static_cast<int32_t>(com(c, o, 0, {iid, out}).eax);
  if (hr >= 0) return r.set_u32(0);
  st<u32>(c, self + 0x68, 0);
  st<u32>(c, self + 0x64, 0);
  r.set_u32(0xb);
}

// DsBuffer::queryNotify (rva 0xff60, c3dj DsQueryNotify_re): QueryInterface for IDirectSoundNotify (rva 0x5d52c) into +0x6c
// on +0x68, else on +0x64, else 0 with no call; negative clears +0x6c and returns 0xb. neg 1: +0x64 is tried first.
void a_DsBuffer_queryNotify(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 self = a[0];
  u32 o = ld<u32>(c, self + (NEG(DsBuffer_queryNotify) == 1 ? 0x64 : 0x68));
  if (!o) {
    o = ld<u32>(c, self + (NEG(DsBuffer_queryNotify) == 1 ? 0x68 : 0x64));
    if (!o) return r.set_u32(0);
  }
  const int32_t hr = static_cast<int32_t>(com(c, o, 0, {snd_va(c, snd_rva::IID_IDirectSoundNotify), self + 0x6c}).eax);
  if (hr >= 0) return r.set_u32(0);
  st<u32>(c, self + 0x6c, 0);
  r.set_u32(0xb);
}

// DsBuffer::releaseIfOwned (rva 0x10060, c3dj): 1 when +0x5c is NULL; the release block when +0x60 is non-NULL; 0.
// neg 1: returns 1 after the release.
void a_DsBuffer_releaseIfOwned(Ctx &c, const uint32_t *a, Ret &r) {
  if (!ld<u32>(c, a[0] + 0x5c)) return r.set_u32(1);
  if (!ld<u32>(c, a[0] + 0x60)) return r.set_u32(0);
  release_block(c, a[0], 0);
  r.set_u32(NEG(DsBuffer_releaseIfOwned) == 1 ? 1 : 0);
}

// DsBuffer::restore (rva 0x101a0, c3fl DsRestore_re), thiscall (a1..a6): Lock (slot +0x2c) with (a1..a6, 0); 0 -> 0; a
// result other than DSERR_BUFFERLOST (0x88780096, the byte tables' only entry 1) -> 0x19; lost: Restore (slot +0x50) when +0x60
// is non-NULL, Lock again, 0x1e for another BUFFERLOST else 0x19 (0 included). neg 1: the second Lock's 0 returns 0.
bool buffer_lost(u32 hr) {
  if (static_cast<int32_t>(hr) <= static_cast<int32_t>(0x8878000au)) return false;
  const u32 i = hr - 0x88780064u;
  return i <= 0x46 && i == 0x32;
}
void a_DsBuffer_restore(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 self = a[0];
  u32 hr = com(c, ld<u32>(c, self + 0x60), 0x2c, {a[1], a[2], a[3], a[4], a[5], a[6], 0}).eax;
  if (!hr) return r.set_u32(0);
  if (!buffer_lost(hr)) return r.set_u32(0x19);
  if (u32 o = ld<u32>(c, self + 0x60)) com(c, o, 0x50, {});
  hr = com(c, ld<u32>(c, self + 0x60), 0x2c, {a[1], a[2], a[3], a[4], a[5], a[6], 0}).eax;
  if (!hr) return r.set_u32(NEG(DsBuffer_restore) == 1 ? 0 : 0x19);
  r.set_u32(buffer_lost(hr) ? 0x1e : 0x19);
}

// DsBuffer::unlock (rva 0x10370, c3dj DsUnlock_re), thiscall (a1..a4): Unlock (slot +0x4c) when +0x60 is non-NULL; 0.
// neg 1: the third argument is passed as 0.
void a_DsBuffer_unlock(Ctx &c, const uint32_t *a, Ret &r) {
  if (u32 o = ld<u32>(c, a[0] + 0x60)) com(c, o, 0x4c, {a[1], a[2], NEG(DsBuffer_unlock) == 1 ? 0u : a[3], a[4]});
  r.set_u32(0);
}

// DsBuffer::getPosition (rva 0x10660, c3ex DsGetPos_re), thiscall (a1, a2, a3): 1 with no device +0x5c; bit 5 of the device's
// byte +0x10 selects getTimedPosition(a1, a2, a3), else getPlayPosition(a1, a2); its eax. neg 1: the bit test reads bit 4.
void a_DsBuffer_getPosition(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 d = ld<u32>(c, a[0] + 0x5c);
  if (!d) return r.set_u32(1);
  if (ld<uint8_t>(c, d + 0x10) & (NEG(DsBuffer_getPosition) == 1 ? 0x10 : 0x20))
    return r.set_u32(th(c, kThGetTimedPosition, {a[0], a[1], a[2], a[3]}));
  r.set_u32(th(c, kThGetPlayPosition, {a[0], a[1], a[2]}));
}

// DsBuffer::getPlayPosition (rva 0x106b0, c3fl DsPlayPos_re), thiscall (outA, outB): see the hook source. The play / write
// cursors are two zeroed locals (0x106be / 0x106c4) GetCurrentPosition (slot +0x10) writes. neg 1: 0x2d is returned as 0.
void a_DsBuffer_getPlayPosition(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 self = a[0], outA = a[1], outB = a[2];
  // `sub esp, 0xc` (0x106b0): play at entry-0xc, write at entry-8, both zeroed (0x106be / 0x106c4)
  SndFrame l(c, 0xc);
  st<u32>(c, l.at(8), 0);
  st<u32>(c, l.at(0xc), 0);
  const u32 buf = ld<u32>(c, self + 0x60);
  if (!buf) return r.set_u32(0x14);
  com(c, buf, 0x10, {l.at(0xc), l.at(8)});
  const u32 play = ld<u32>(c, l.at(0xc)), write = ld<u32>(c, l.at(8));
  const u32 last = ld<u32>(c, self + 0x10);
  const u32 S = ld<u32>(c, self + 0x78);
  const u32 d = (play < last) ? S - last + play : play - last;
  const u32 W = ld<uint16_t>(c, self + 0xb4);
  st<u32>(c, self + 0x18, W ? d / W : 0);
  if (write == ld<u32>(c, self + 0x24)) return r.set_u32(0x15);
  u32 e = ld<u32>(c, self + 0x38) + write;
  st<u32>(c, self + 0x10, play);
  u32 s = ld<u32>(c, self + 0x30);
  u32 len = 0, ret = 0;
  bool recompute;
  bool done = false;
  if (e < S) {
    recompute = !(s > write && e > s);
  } else {
    e -= S;
    if (s > write) {
      len = (e >= s) ? e - s : S - s + e;
      recompute = false;
      done = true;
    } else {
      recompute = !(e > s);
    }
  }
  if (!done) {
    if (!recompute) {
      len = e - s;
    } else {
      s = ld<u32>(c, self + 0x40) + write;
      if (s >= S) s -= S;
      len = (e >= s) ? e - s : S - s + e;
      ret = NEG(DsBuffer_getPlayPosition) == 1 ? 0 : 0x2d;
    }
  }
  st<u32>(c, self + 0x30, e);
  st<u32>(c, self + 0x24, write);
  st<u32>(c, outA, len);
  st<u32>(c, outB, s);
  const u32 g = (s < play) ? ld<u32>(c, self + 0x78) - play + s : s - play;
  const u32 W2 = ld<uint16_t>(c, self + 0xb4);
  st<u32>(c, self + 0x1c, W2 ? g / W2 : 0);
  r.set_u32(ret);
}

// DsBuffer::lockCaptureRegion (rva 0x10860, c3do DsLockCapture_re), thiscall (len, out): GetCurrentPosition (slot +0x10) into
// two zeroed locals (play first pushed last: the call's second slot is the write cursor); a failure returns 6. Bit 0 of byte
// +4 set: cleared, start = +0x40 + write (less +0x78 when not below), +0x28 = +0x24 = write; else start = +0x30. end = len +
// start (less +0x78 when not below), +0x24 = write, +0x30 = end, *out = start; 0. neg 1: +0x30 is not stored.
void a_DsBuffer_lockCaptureRegion(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 self = a[0], a1 = a[1], a2 = a[2];
  // `sub esp, 8` (0x10860): play at entry-4, write at entry-8, both zeroed (0x10875 / 0x10879)
  SndFrame l(c, 8);
  st<u32>(c, l.at(8), 0);
  st<u32>(c, l.at(4), 0);
  const u32 o = ld<u32>(c, self + 0x60);
  const u32 hr = com(c, o, 0x10, {l.at(4), l.at(8)}).eax;
  if (hr) return r.set_u32(6);
  const u32 wr = ld<u32>(c, l.at(8));
  const uint8_t al = ld<uint8_t>(c, self + 4);
  u32 start;
  if (al & 1) {
    const u32 size = ld<u32>(c, self + 0x78);
    st<uint8_t>(c, self + 4, static_cast<uint8_t>(al & 0xfe));
    start = ld<u32>(c, self + 0x40) + wr;
    st<u32>(c, self + 0x28, wr);
    st<u32>(c, self + 0x24, wr);
    if (!(start < size)) start -= size;
  } else {
    start = ld<u32>(c, self + 0x30);
  }
  const u32 size2 = ld<u32>(c, self + 0x78);
  u32 end = a1 + start;
  if (!(end < size2)) end -= size2;
  st<u32>(c, self + 0x24, wr);
  if (NEG(DsBuffer_lockCaptureRegion) != 1) st<u32>(c, self + 0x30, end);
  st<u32>(c, a2, start);
  r.set_u32(0);
}

// DsBuffer::getTimedPosition (rva 0x10be0, c3dh TimedPos_re), thiscall (out1, out2, step). neg 1: the restart's Play is
// skipped.
u32 div1000(u32 x) { return static_cast<u32>((static_cast<uint64_t>(0x10624dd3u) * x) >> 32) >> 6; }
void a_DsBuffer_getTimedPosition(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 self = a[0], out1 = a[1], out2 = a[2], a3 = a[3];
  u32 edx;
  if (ld<u32>(c, self + 0x4c) == 0) {
    const u32 now = snd_import(c, kIatTimeGetTime, SndAbi::Stdcall, {}).eax;
    const u32 elapsed = now - ld<u32>(c, self + 0x54);
    const u32 rate = ld<u32>(c, self + 0xac);
    st<u32>(c, self + 0x4c, elapsed);
    const u32 x = (elapsed + ld<u32>(c, self + 0x50) * 2) * rate;
    const u32 y = rate * a3;
    st<u32>(c, self + 0x48, div1000(x));
    edx = div1000(y);
  } else {
    const u32 t = ld<u32>(c, self + 0x48) * 1000u;
    const u32 hi = static_cast<u32>((static_cast<uint64_t>(0xbe37c63bu) * t) >> 32);
    const u32 q = hi >> 14;
    const u32 cur = a3 + ld<u32>(c, self + 0x4c);
    st<u32>(c, self + 0x4c, cur);
    const u32 lead = ld<u32>(c, self + 0x50) * 10;
    const u32 z = (lead - q + cur) * ld<u32>(c, self + 0xac);
    edx = div1000(z);
  }
  const u32 align = ld<uint16_t>(c, self + 0xb4);
  st<u32>(c, out1, align * edx);
  const u32 align2 = ld<uint16_t>(c, self + 0xb4);
  st<u32>(c, out2, (align2 * ld<u32>(c, self + 0x48)) & ld<u32>(c, self + 0x58));
  const u32 len = ld<u32>(c, self + 0x78);
  st<u32>(c, self + 0x48, ld<u32>(c, self + 0x48) + edx);
  if (ld<u32>(c, out1) <= len) return r.set_u32(0);
  const u32 o = ld<u32>(c, self + 0x60);
  st<u32>(c, self + 0x4c, 0);
  st<u32>(c, self + 0x48, 0);
  if (o) com(c, o, 0x48, {});
  if (ld<u32>(c, self + 0x60)) {
    st<u32>(c, self + 0x54, snd_import(c, kIatTimeGetTime, SndAbi::Stdcall, {}).eax);
    if (NEG(DsBuffer_getTimedPosition) != 1 && com(c, ld<u32>(c, self + 0x60), 0x30, {0, 0, 1}).eax == 0)
      com(c, ld<u32>(c, self + 0x60), 0x14, {self + 0xa8, 0x12, 0});
  }
  st<u32>(c, out1, 0);
  r.set_u32(0x27);
}

// DsBuffer::play (rva 0x10db0, c3dh Play_re), thiscall (flags): 0x14 with no buffer; +0x54 = timeGetTime(); Play(0, 0, flags)
// (slot +0x30) failing returns 0x1c; slot +0x14 (this+0xa8, 0x12, 0); 0. neg 1: the flags are passed as 0.
void a_DsBuffer_play(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 self = a[0];
  if (!ld<u32>(c, self + 0x60)) return r.set_u32(0x14);
  st<u32>(c, self + 0x54, snd_import(c, kIatTimeGetTime, SndAbi::Stdcall, {}).eax);
  if (com(c, ld<u32>(c, self + 0x60), 0x30, {0, 0, NEG(DsBuffer_play) == 1 ? 0u : a[1]}).eax) return r.set_u32(0x1c);
  com(c, ld<u32>(c, self + 0x60), 0x14, {self + 0xa8, 0x12, 0});
  r.set_u32(0);
}

// DsBuffer::isBusy (rva 0x10e30, c3dj DsIsBusy_re): 0 with no buffer; GetStatus (slot +0x24) into a zeroed local; +0x44
// non-zero returns 1, else status & 1. neg 1: +0x44 is ignored.
void a_DsBuffer_isBusy(Ctx &c, const uint32_t *a, Ret &r) {
  SndFrame l(c, 4);  // `push ecx` (0x10e30): the status local at entry-4 starts as this
  st<u32>(c, l.at(4), a[0]);
  const u32 o = ld<u32>(c, a[0] + 0x60);
  if (!o) return r.set_u32(0);
  com(c, o, 0x24, {l.at(4)});
  if (NEG(DsBuffer_isBusy) != 1 && ld<u32>(c, a[0] + 0x44)) return r.set_u32(1);
  r.set_u32(ld<u32>(c, l.at(4)) & 1);
}

// DsBuffer::setRegionMs (rva 0x10eb0, c3au), thiscall (ms, regions): frames = +0xac * ms / 1000, region = word +0xb4 * frames,
// total = region * regions; above +0x78 returns 0xa; else +0x38 total, +0x40 region, +0x3c (total - region) / 2, +0x34 frames;
// 0. neg 1: +0x3c is not stored.
void a_DsBuffer_setRegionMs(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 self = a[0];
  const u32 frames = div1000(ld<u32>(c, self + 0xac) * a[1]);
  const u32 region = static_cast<u32>(ld<uint16_t>(c, self + 0xb4)) * frames;
  const u32 total = region * a[2];
  if (total > ld<u32>(c, self + 0x78)) return r.set_u32(0xa);
  st<u32>(c, self + 0x38, total);
  st<u32>(c, self + 0x40, region);
  if (NEG(DsBuffer_setRegionMs) != 1) st<u32>(c, self + 0x3c, (total - region) >> 1);
  st<u32>(c, self + 0x34, frames);
  r.set_u32(0);
}

// DsBuffer::configureDesc (rva 0x10f30, c3az), thiscall (mode): 6 with a buffer; mode 0: format pointer = this+0xa8, bit 1 of
// +0 = 0, desc {0x24, 0xe8, 0x10000, 0}; mode 1: the desc cleared, size 0x24, bit 1 set, flags 1; else 0xa.
// neg 1: mode 1 leaves bit 1 clear.
void a_DsBuffer_configureDesc(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 self = a[0], mode = a[1];
  if (ld<u32>(c, self + 0x60)) return r.set_u32(6);
  if (mode == 0) {
    st<u32>(c, self + 0x80, self + 0xa8);
    st<u32>(c, self, ld<u32>(c, self) & 0xfffffffdu);
    st<u32>(c, self + 0x70, 0x24);
    st<u32>(c, self + 0x74, 0xe8);
    st<u32>(c, self + 0x78, 0x10000);
    st<u32>(c, self + 0x7c, 0);
    return r.set_u32(0);
  }
  if (mode == 1) {
    for (u32 i = 0; i < 9; ++i) st<u32>(c, self + 0x70 + i * 4, 0);
    st<u32>(c, self + 0x70, 0x24);
    st<u32>(c, self, (ld<u32>(c, self) & 0xfffffffdu) | (NEG(DsBuffer_configureDesc) == 1 ? 0u : 2u));
    st<u32>(c, self + 0x74, 1);
    return r.set_u32(0);
  }
  r.set_u32(0xa);
}

// ================================================================================================ Ds3DBuffer / Ds3DListener
// Ds3DBuffer::setPosition (rva 0x11000, c3dj Ds3DSetPos_re), thiscall (x, y, z floats as dwords): 0xb with no 3D interface
// +0x64; +0xc8 / +0xcc / +0xd0 = x / y / z; SetPosition (slot +0x4c) (x, y, z, 0) non-zero -> 0x1f, else 0. neg 1: z stored at
// +0xcc.
void a_Ds3DBuffer_setPosition(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 self = a[0];
  const u32 o = ld<u32>(c, self + 0x64);
  if (!o) return r.set_u32(0xb);
  st<u32>(c, self + 0xc8, a[1]);
  st<u32>(c, self + 0xcc, a[2]);
  st<u32>(c, self + (NEG(Ds3DBuffer_setPosition) == 1 ? 0xcc : 0xd0), a[3]);
  r.set_u32(com(c, o, 0x4c, {a[1], a[2], a[3], 0}).eax ? 0x1f : 0);
}
// setPositionX / Y / Z (rvas 0x11070 / 0x110c0 / 0x11110, c3dj): one coordinate stored, SetPosition with the stored other two;
// 0. neg 1: the coordinate is not stored.
void set_axis(Ctx &c, const uint32_t *a, Ret &r, u32 off, int neg) {
  const u32 self = a[0];
  const u32 o = ld<u32>(c, self + 0x64);
  if (!o) return r.set_u32(0xb);
  if (neg != 1) st<u32>(c, self + off, a[1]);
  const u32 x = off == 0xc8 ? a[1] : ld<u32>(c, self + 0xc8);
  const u32 y = off == 0xcc ? a[1] : ld<u32>(c, self + 0xcc);
  const u32 z = off == 0xd0 ? a[1] : ld<u32>(c, self + 0xd0);
  com(c, o, 0x4c, {x, y, z, 0});
  r.set_u32(0);
}
void a_Ds3DBuffer_setPositionX(Ctx &c, const uint32_t *a, Ret &r) { set_axis(c, a, r, 0xc8, NEG(Ds3DBuffer_setPositionX)); }
void a_Ds3DBuffer_setPositionY(Ctx &c, const uint32_t *a, Ret &r) { set_axis(c, a, r, 0xcc, NEG(Ds3DBuffer_setPositionY)); }
void a_Ds3DBuffer_setPositionZ(Ctx &c, const uint32_t *a, Ret &r) { set_axis(c, a, r, 0xd0, NEG(Ds3DBuffer_setPositionZ)); }

// Ds3DListener::setPosition (rva 0x135a0, c3dh ListenerPos_re), thiscall (x, y, z): with the listener +0xd8 SetPosition (slot
// +0x38) (x, y, z, 0); a non-zero result returns mapDsError(result); +0x1ac / +0x1b0 / +0x1b4 = x / y / z; 0.
// neg 1: the listener's result is ignored.
void a_Ds3DListener_setPosition(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 self = a[0];
  if (u32 o = ld<u32>(c, self + 0xd8)) {
    const u32 hr = com(c, o, 0x38, {a[1], a[2], a[3], 0}).eax;
    if (hr && NEG(Ds3DListener_setPosition) != 1)
      return r.set_u32(snd_call(c, kMapDsError, SndAbi::Cdecl, {hr}).eax);
  }
  st<u32>(c, self + 0x1ac, a[1]);
  st<u32>(c, self + 0x1b0, a[2]);
  st<u32>(c, self + 0x1b4, a[3]);
  r.set_u32(0);
}

// ================================================================================================ DirectSoundDevice
// Pop the head node of a list {head, tail, ..., count} of nodes {+0 prev, +4 next, +8 payload} (c3fk c3fk_popHead).
u32 pop_head(Ctx &c, u32 head, u32 tail) {
  const u32 n = ld<u32>(c, head);
  const u32 next = ld<u32>(c, n + 4);
  if (next) {
    st<u32>(c, next, 0);
    st<u32>(c, head, next);
  } else {
    st<u32>(c, tail, 0);
    st<u32>(c, head, 0);
  }
  return n;
}
// pop every node until a NULL payload (the node and, with freePayload, the payload to operator delete)
void drain_nodes(Ctx &c, u32 self, u32 head, u32 tail, u32 count, bool free_payload) {
  while (ld<u32>(c, self + head)) {
    const u32 n = pop_head(c, self + head, self + tail);
    const u32 pl = ld<u32>(c, n + 8);
    snd_delete(c, n);
    st<int32_t>(c, self + count, ld<int32_t>(c, self + count) - 1);
    if (!pl) break;
    if (free_payload) snd_delete(c, pl);
  }
}

// DirectSoundDevice::close (rva 0x11290, c3fk DsClose_re): frees the device entries (+4 chain along +0x10, their three strings
// at +0 +4 +8), +4 = +8 = 0, releaseAll, CoUninitialize, drains the lists {+0x178} and {+0x168} (nodes only) and returns
// DsBuffer::destroy(this+0x70)'s eax. neg 1: the second list is not drained.
void a_DsDev_close(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 self = a[0];
  for (u32 e = ld<u32>(c, self + 4); e;) {
    for (u32 off = 0; off <= 8; off += 4)
      if (u32 s = ld<u32>(c, e + off)) {
        snd_delete(c, s);
        st<u32>(c, e + off, 0);
      }
    const u32 next = ld<u32>(c, e + 0x10);
    snd_delete(c, e);
    e = next;
  }
  st<u32>(c, self + 4, 0);
  st<u32>(c, self + 8, 0);
  th(c, kThReleaseAll, {self});
  snd_import(c, kIatCoUninitialize, SndAbi::Stdcall, {});
  drain_nodes(c, self, 0x178, 0x17c, 0x184, false);
  if (NEG(DsDev_close) != 1) drain_nodes(c, self, 0x168, 0x16c, 0x174, false);
  r.set_u32(th(c, kThDestroy, {self + 0x70}));
}

// DirectSoundDevice::enumerate (rva 0x114e0, c3fk DsEnum_re), thiscall (flags, ctx). neg 1: bit 0 of +0x6c is not set.
void a_DsDev_enumerate(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 self = a[0], flags = a[1], cbctx = a[2];
  if (ld<u32>(c, self) || ld<u32>(c, self + 4) || ld<u32>(c, self + 8)) return r.set_u32(0xc);
  if (!cbctx) return r.set_u32(0xa);
  st<u32>(c, self + 0x164, cbctx);
  if (flags & 0x20) {
    st<uint8_t>(c, self + 0x6c, static_cast<uint8_t>(ld<uint8_t>(c, self + 0x6c) | 8));
    if (flags & 0x40) st<uint8_t>(c, self + 0x6c, static_cast<uint8_t>(ld<uint8_t>(c, self + 0x6c) | 0x10));
  }
  // DirectSoundEnumerateA through the image's import stub (`jmp [iat]` at rva 0x3a978, called at 0x1153a): a fixture's
  // stand-in claims the stub, so the port calls the stub's address like the original [U-1472]
  const u32 hr = snd_call(c, kStubDirectSoundEnumerateA, SndAbi::Stdcall, {snd_va(c, kThEnumCallback), self}).eax;
  if (hr) {
    snd_call(c, kThMapDsError, SndAbi::Cdecl, {hr});
    return r.set_u32(0xa);
  }
  const u32 n = ld<u32>(c, self + 0x148);
  const u32 want = ld<u32>(c, snd_va(c, kDsDeviceIndexRva));
  if (!n) return r.set_u32(3);
  if (want >= n) return r.set_u32(0xa);
  if (ld<u32>(c, self + 8)) return r.set_u32(6);
  u32 e = ld<u32>(c, self + 4);
  for (u32 k = 0; k < n; ++k) {
    st<u32>(c, self + 8, e);
    if (k == want) break;
    e = ld<u32>(c, e + 0x10);
  }
  if (NEG(DsDev_enumerate) != 1) st<uint8_t>(c, self + 0x6c, static_cast<uint8_t>(ld<uint8_t>(c, self + 0x6c) | 1));
  r.set_u32(0);
}

// getDeviceCaps (rva 0x11760, c3cx GetDeviceCaps_re): 0 with no interface at +0; GetCaps (slot +0x10) into this+0xc; returns
// +0x54 read after the call. neg 1: +0x54 is read before the call.
void a_getDeviceCaps(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 o = ld<u32>(c, a[0]);
  if (!o) return r.set_u32(0);
  const u32 before = ld<u32>(c, a[0] + 0x54);
  com(c, o, 0x10, {a[0] + 0xc});
  r.set_u32(NEG(getDeviceCaps) == 1 ? before : ld<u32>(c, a[0] + 0x54));
}

// dsEnumCallbackAppend (rva 0x11810, c3do AppendNamedNode_re), stdcall (guid, desc, module, owner): a 0x14-byte entry
// {desc copy, module copy, guid copy or NULL, owner, next} appended to owner's chain at +4 (along +0x10), owner +0x148 + 1, the
// module counter at rva 0xb4a44 + 1; returns 1. Strings and the GUID through operator new (rva 0x424d7). neg 1: the counter
// at 0xb4a44 is not incremented.
u32 dup_str(Ctx &c, u32 s) {
  const u32 len = vstrlen(c.mem, s);
  const u32 d = snd_new(c, len + 1);
  for (u32 i = 0; i <= len; ++i) st<uint8_t>(c, d + i, ld<uint8_t>(c, s + i));
  return d;
}
void a_dsEnumCallbackAppend(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 guid = a[0], desc = a[1], module = a[2], owner = a[3];
  const u32 node = snd_new(c, 0x14);
  st<int32_t>(c, owner + 0x148, ld<int32_t>(c, owner + 0x148) + 1);
  if (u32 head = ld<u32>(c, owner + 4)) {
    u32 last = head;
    while (ld<u32>(c, last + 0x10)) last = ld<u32>(c, last + 0x10);
    st<u32>(c, last + 0x10, node);
  } else {
    st<u32>(c, owner + 4, node);
  }
  st<u32>(c, node, dup_str(c, desc));
  st<u32>(c, node + 4, dup_str(c, module));
  if (guid) {
    const u32 g = snd_new(c, 0x10);
    st<u32>(c, node + 8, g);
    for (u32 i = 0; i < 0x10; i += 4) st<u32>(c, g + i, ld<u32>(c, guid + i));
  } else {
    st<u32>(c, node + 8, 0);
  }
  st<u32>(c, node + 0xc, owner);
  st<u32>(c, node + 0x10, 0);
  if (NEG(dsEnumCallbackAppend) != 1) {
    const u32 ctr = snd_va(c, kDsDeviceCountRva);
    st<int32_t>(c, ctr, ld<int32_t>(c, ctr) + 1);
  }
  r.set_u32(1);
}

// DirectSoundDevice::releaseAll (rva 0x11970, c3ex DsRelAll_re): empties the DsBuffer list (unlink, releaseIfOwned, destroy +
// delete), releaseIfOwned on the primary this+0x70 with bit 1 of +0x6c, the buffer at +0x198 released / destroyed / deleted and
// cleared, the interface at +0 released (slot +8) and cleared, +8 = 0; returns ++[+0x18c]. neg 1: +8 is not cleared.
void a_DsDev_releaseAll(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 self = a[0];
  for (u32 b = ds_head(c); b; b = ds_head(c)) {
    ds_unlink(c, b, 0);
    th(c, kThReleaseIfOwned, {b});
    destroy_delete(c, b);
  }
  if (ld<uint8_t>(c, self + 0x6c) & 2) th(c, kThReleaseIfOwned, {self + 0x70});
  if (ld<u32>(c, self + 0x198)) {
    th(c, kThReleaseIfOwned, {ld<u32>(c, self + 0x198)});
    if (u32 x = ld<u32>(c, self + 0x198)) destroy_delete(c, x);
    st<u32>(c, self + 0x198, 0);
  }
  if (u32 o = ld<u32>(c, self)) {
    com(c, o, 8, {});
    st<u32>(c, self, 0);
  }
  if (NEG(DsDev_releaseAll) != 1) st<u32>(c, self + 8, 0);
  const u32 v = ld<u32>(c, self + 0x18c) + 1;
  st<u32>(c, self + 0x18c, v);
  r.set_u32(v);
}

// DirectSoundDevice::stopAll (rva 0x11a80, c3fk DsStopAll_re). neg 1: the interface at +0 is not released.
void a_DsDev_stopAll(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 self = a[0];
  while (u32 b = ds_head(c)) {
    if (u32 ch = ld<u32>(c, b + 0x20)) {
      th(c, kThAppendNode, {self + 0x178, ch});
      th(c, kThReleaseBuffers, {ld<u32>(c, b + 0x20)});
      continue;
    }
    ds_unlink(c, b, 0);
    th(c, kThRelease, {b});
    destroy_delete(c, b);
  }
  drain_nodes(c, self, 0x168, 0x16c, 0x174, false);
  if (ld<uint8_t>(c, self + 0x6c) & 2) th(c, kThReleaseIfOwned, {self + 0x70});
  if (u32 x = ld<u32>(c, self + 0x198)) {
    th(c, kThReleaseIfOwned, {x});
    if (u32 y = ld<u32>(c, self + 0x198)) destroy_delete(c, y);
    st<u32>(c, self + 0x198, 0);
  }
  if (u32 itf = ld<u32>(c, self))
    if (NEG(DsDev_stopAll) != 1) com(c, itf, 8, {});
  st<u32>(c, self, 0);
  const u32 v = ld<u32>(c, self + 0x18c) + 1;
  st<u32>(c, self + 0x18c, v);
  r.set_u32(v);
}

// DirectSoundDevice::create (rva 0x11c60, c3fk DsDevCreate_re), thiscall (hwnd, level). The two WAVEFORMATEX (mono 22050 Hz
// at esp+0x10, stereo 44100 Hz at esp+0x24) are frame locals passed to DsBuffer::setPrimaryFormat. neg 1: +0x190 keeps its
// value.
void put_fmt(Ctx &c, u32 p, uint16_t ch, u32 rate, u32 bytes, uint16_t align) {
  st<uint16_t>(c, p, 1);
  st<uint16_t>(c, p + 2, ch);
  st<u32>(c, p + 4, rate);
  st<u32>(c, p + 8, bytes);
  st<uint16_t>(c, p + 0xc, align);
  st<uint16_t>(c, p + 0xe, 0x10);
  st<uint16_t>(c, p + 0x10, 0);
}
void a_DsDev_create(Ctx &c, const uint32_t *a, Ret &r) {
  SndFrame l(c, 0x28);
  const u32 self = a[0], hwnd = a[1];
  u32 level = a[2];
  if (!hwnd) return r.set_u32(0xa);
  if (ld<u32>(c, self)) return r.set_u32(0xc);
  if (!(ld<uint8_t>(c, self + 0x6c) & 1)) return r.set_u32(3);
  if (!level) level = 2;
  if (!ld<u32>(c, self + 8)) return r.set_u32(0x13);
  const u32 ci = snd_import(c, kIatCoInitialize, SndAbi::Stdcall, {0}).eax;
  if (ci != 0 && ci != 1) {
    st<u32>(c, self, 0);
    return r.set_u32(0x20);
  }
  st<u32>(c, self + 0x18c, ld<u32>(c, self + 0x18c) - 1);
  const u32 hr = snd_import(c, kIatCoCreateInstance, SndAbi::Stdcall,
                            {snd_va(c, snd_rva::CLSID_DirectSound), 0, 1, snd_va(c, snd_rva::IID_IDirectSound), self}).eax;
  u32 rr = 0;
  if (!hr && ld<u32>(c, self)) rr = com(c, ld<u32>(c, self), 0x28, {ld<u32>(c, ld<u32>(c, self + 8) + 8)}).eax;
  if (!rr) rr = com(c, ld<u32>(c, self), 0x18, {hwnd, level}).eax;
  if (rr) {
    snd_call(c, kThMapDsError, SndAbi::Cdecl, {rr});
    th(c, kThReleaseAll, {self});
    return r.set_u32(3);
  }
  st<u32>(c, snd_obj(c, &SoundState::g_dsCoopLevel).va(), hwnd);
  st<u32>(c, self + 0xc, 0x60);
  {
    const u32 g = com(c, ld<u32>(c, self), 0x10, {self + 0xc}).eax;
    if (g) snd_call(c, kThMapDsError, SndAbi::Cdecl, {g});
  }
  if (ld<uint8_t>(c, self + 0x10) & 0x20) {
    st<uint8_t>(c, self + 0x6c, static_cast<uint8_t>(ld<uint8_t>(c, self + 0x6c) & 0xe7));
  } else {
    const u32 v = ld<u32>(c, self + 0x30);
    st<u32>(c, self + 0x158, 0);
    st<u32>(c, self + 0x14c, v);
    st<u32>(c, self + 0x154, v);
    st<u32>(c, self + 0x150, v);
    st<u32>(c, self + 0x15c, 0);
    if (ld<u32>(c, self + 0x40) < 2 && (ld<uint8_t>(c, self + 0x6c) & 8))
      st<uint8_t>(c, self + 0x6c, static_cast<uint8_t>(ld<uint8_t>(c, self + 0x6c) & 0xe7));
  }
  // the two formats (0x11db6-0x11e11): mono at esp+0x10 = entry-0x28, stereo at esp+0x24 = entry-0x14 (`sub esp, 0x28` and
  // four pushes, 0x11c60-0x11c6a)
  put_fmt(c, l.at(0x28), 1, 0x5622, 0xac44, 2);
  put_fmt(c, l.at(0x14), 2, 0xac44, 0x2b110, 4);
  u32 e = 1;
  const u32 prim = self + 0x70;
  if (ld<uint8_t>(c, self + 0x6c) & 8) {
    th(c, kThSetPrimaryFormat, {prim, l.at(0x28)});
    e = 0x11;
  } else {
    th(c, kThSetPrimaryFormat, {prim, l.at(0x14)});
  }
  rr = th(c, kThConfigureDesc, {prim, 1});
  if (rr) {
    th(c, kThReleaseAll, {self});
    return r.set_u32(rr);
  }
  st<u32>(c, self + 0xe4, e);
  rr = th(c, kThCreate, {prim, ld<u32>(c, self), ld<u32>(c, self + 0x164)});
  if (rr) {
    th(c, kThReleaseAll, {self});
    return r.set_u32(rr);
  }
  st<uint8_t>(c, self + 0x6c, static_cast<uint8_t>(ld<uint8_t>(c, self + 0x6c) | 2));
  st<u32>(c, self + 0xcc, self);
  while (ld<u32>(c, self + 0x178)) {
    const u32 n = pop_head(c, self + 0x178, self + 0x17c);
    const u32 p = ld<u32>(c, n + 8);
    snd_delete(c, n);
    st<int32_t>(c, self + 0x184, ld<int32_t>(c, self + 0x184) - 1);
    if (!p) break;
    if (ld<u32>(c, self + 0x14c)) th(c, kThParseRiffWave, {p, p + 0x64});
  }
  if (NEG(DsDev_create) != 1) st<u32>(c, self + 0x190, level);
  r.set_u32(0);
}

// return0Pop4 (rva 0x12040, emu-only row): `xor eax, eax; ret 4` (thiscall, one stack slot). neg 1: returns 1.
void a_return0Pop4(Ctx &, const uint32_t *, Ret &r) { r.set_u32(NEG(return0Pop4) == 1 ? 1 : 0); }

// DirectSoundDevice::setCooperativeLevel (rva 0x120a0, c3dt DsCoop_re), thiscall (level): 0x13 with no interface;
// SetCooperativeLevel (slot +0x18) (level, +0x190); failure: mapDsError, releaseAll, 3; success: g_dsCoopLevel = level, 0.
// neg 1: the global is not stored.
void a_DsDev_setCooperativeLevel(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 self = a[0];
  const u32 o = ld<u32>(c, self);
  if (!o) return r.set_u32(0x13);
  const u32 hr = com(c, o, 0x18, {a[1], ld<u32>(c, self + 0x190)}).eax;
  if (hr) {
    snd_call(c, kThMapDsError, SndAbi::Cdecl, {hr});
    th(c, kThReleaseAll, {self});
    return r.set_u32(3);
  }
  if (NEG(DsDev_setCooperativeLevel) != 1) st<u32>(c, snd_obj(c, &SoundState::g_dsCoopLevel).va(), a[1]);
  r.set_u32(0);
}

// Raw append of a fresh node {0, 0, payload} to a list {head, tail, cursor, count} (c3fk c3fk_rawAppend).
void raw_append(Ctx &c, u32 list, u32 payload) {
  const u32 n = snd_new(c, 0xc);
  if (n) {
    st<u32>(c, n, 0);
    st<u32>(c, n + 4, 0);
    st<u32>(c, n + 8, payload);
  }
  if (u32 t = ld<u32>(c, list + 4)) {
    st<u32>(c, t + 4, n);
    st<u32>(c, n, ld<u32>(c, list + 4));
  } else {
    st<u32>(c, list, n);
  }
  st<u32>(c, list + 4, n);
  st<u32>(c, list + 0xc, ld<u32>(c, list + 0xc) + 1);
}

// DirectSoundDevice::duplicateBuffer (rva 0x12260, c3fk DupBuf_re), thiscall (chan, src, out). neg 1: +0x15c is not
// incremented.
void a_DsDev_duplicateBuffer(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 self = a[0], chan = a[1], src = a[2], out = a[3];
  if (!(ld<uint8_t>(c, self + 0x6c) & 2) || !ld<u32>(c, self)) return r.set_u32(3);
  if (ld<u32>(c, self + 0x15c) == ld<u32>(c, self + 0x150)) return r.set_u32(0x25);
  const u32 p = snd_new(c, 0xd4);
  u32 b = p ? th(c, kThCtor, {p}) : 0;
  st<u32>(c, out, b);
  st<u32>(c, b + 0x5c, self);
  const u32 hr = com(c, ld<u32>(c, self), 0x14, {ld<u32>(c, src + 0x60), ld<u32>(c, out) + 0x60}).eax;  // DuplicateSoundBuffer
  if (hr) {
    snd_call(c, kThMapDsError, SndAbi::Cdecl, {hr});
    if (u32 x = ld<u32>(c, out)) destroy_delete(c, x);
    st<u32>(c, out, 0);
    return r.set_u32(0x1d);
  }
  b = ld<u32>(c, out);
  ds_append(c, b);
  raw_append(c, self + 0x168, ld<u32>(c, out));
  st<u32>(c, ld<u32>(c, out) + 0x20, chan);
  th(c, kThRegisterBuffer, {chan, ld<u32>(c, out)});
  if (static_cast<uint8_t>(th(c, kThHasAux, {src}))) {
    th(c, kThQuery3D, {ld<u32>(c, out)});
    th(c, kThQueryNotify, {ld<u32>(c, out)});
  }
  if (NEG(DsDev_duplicateBuffer) != 1) st<u32>(c, self + 0x15c, ld<u32>(c, self + 0x15c) + 1);
  r.set_u32(0);
}

// DirectSoundDevice::freeBufferById (rva 0x12690, c3fk FreeById_re), thiscall (chan, out). neg 1: +0x158 is not decremented.
void a_DsDev_freeBufferById(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 self = a[0], chan = a[1], out = a[2];
  if (!chan) return r.set_u32(0xa);
  u32 n = ld<u32>(c, self + 0x168);
  u32 b = 0;
  for (; n; n = ld<u32>(c, n + 4)) {
    b = ld<u32>(c, n + 8);
    if (!th(c, kThIsBusy, {b})) break;
  }
  if (!n) return r.set_u32(0x24);
  th(c, kThVoiceListRemove, {ld<u32>(c, b + 0x20), b});
  n = ld<u32>(c, self + 0x168);
  while (n && ld<u32>(c, n + 8) != b) n = ld<u32>(c, n + 4);
  if (!n) return r.set_u32(0x24);
  const u32 prev = ld<u32>(c, n);
  if (prev) st<u32>(c, prev + 4, ld<u32>(c, n + 4)); else st<u32>(c, self + 0x168, ld<u32>(c, n + 4));
  if (u32 next = ld<u32>(c, n + 4)) {
    st<u32>(c, next, ld<u32>(c, n));
    st<u32>(c, self + 0x170, next);
  } else {
    st<u32>(c, self + 0x170, 0);
    st<u32>(c, self + 0x16c, ld<u32>(c, n));
  }
  const u32 pl = ld<u32>(c, n + 8);
  snd_delete(c, n);
  st<int32_t>(c, self + 0x174, ld<int32_t>(c, self + 0x174) - 1);
  if (!pl) return r.set_u32(0x24);
  if (ld<uint8_t>(c, self + 0x6c) & 2) {
    u32 e = ds_head(c);
    while (e && e != pl) e = ld<u32>(c, e + 0xc0);
    if (e) ds_unlink(c, e, 0);
    if (NEG(DsDev_freeBufferById) != 1) st<int32_t>(c, self + 0x158, ld<int32_t>(c, self + 0x158) - 1);
    if (!th(c, kThReleaseIfOwned, {pl})) destroy_delete(c, pl);
  }
  st<u32>(c, self + 0x15c, ld<u32>(c, self + 0x15c) - 1);
  const u32 rr = th(c, kThDuplicate, {self, chan, ld<u32>(c, chan + 0x54), out});
  if (rr) {
    st<u32>(c, out, 0);
    return r.set_u32(rr);
  }
  st<u32>(c, ld<u32>(c, out) + 0x20, chan);
  r.set_u32(0);
}

// DsBuffer::scalarDeletingDtor (rva 0x12b80, c3dj): DsBuffer::destroy; operator delete when bit 0 of the flags is set;
// returns this. neg 1: the delete ignores the flags.
void a_DsBuffer_scalarDeletingDtor(Ctx &c, const uint32_t *a, Ret &r) {
  th(c, kThDestroy, {a[0]});
  if ((a[1] & 1) || NEG(DsBuffer_scalarDeletingDtor) == 1) snd_delete(c, a[0]);
  r.set_u32(a[0]);
}

// The shared core of createBuffer (0x12d3e-0x12f34) and createBufferFmt (0x13022-0x13221) (c3fk c3fk_dsCreateCore).
u32 create_core(Ctx &c, u32 self, u32 out, u32 flags, u32 size, u32 fmt, bool with_fmt, int neg) {
  const u32 bit2 = flags & 4;
  if (bit2) {
    if (!(ld<u32>(c, self + 0x158) < ld<u32>(c, self + 0x154))) return 0x25;
    st<u32>(c, self + 0x158, ld<u32>(c, self + 0x158) + 1);
    st<u32>(c, self + 0x150, ld<u32>(c, self + 0x14c) - ld<u32>(c, self + 0x158));
    if (ld<u32>(c, self + 0x15c) > ld<u32>(c, self + 0x150)) {
      u32 n = ld<u32>(c, self + 0x168);
      u32 b = 0;
      for (; n; n = ld<u32>(c, n + 4)) {
        b = ld<u32>(c, n + 8);
        if (!th(c, kThIsBusy, {b})) break;
      }
      u32 rr = 0;
      if (n) {
        th(c, kThVoiceListRemove, {ld<u32>(c, b + 0x20), b});
        rr = th(c, kThFindUnlink, {self + 0x168, b});
      }
      if (!rr) {
        st<u32>(c, self + 0x158, ld<u32>(c, self + 0x158) - 1);
        st<u32>(c, self + 0x150, ld<u32>(c, self + 0x14c) - ld<u32>(c, self + 0x158));
        return 0x25;
      }
      if (ld<uint8_t>(c, self + 0x6c) & 2) {
        u32 e = ds_head(c);
        while (e && e != rr) e = ld<u32>(c, e + 0xc0);
        if (e) th(c, kThUnlinkDsBuffer, {self, e});
        st<int32_t>(c, self + 0x158, ld<int32_t>(c, self + 0x158) - 1);
        if (!th(c, kThReleaseIfOwned, {rr})) th(c, kThScalarDtor, {rr, 1});
      }
      st<u32>(c, self + 0x15c, ld<u32>(c, self + 0x15c) - 1);
    }
  }
  const u32 p = snd_new(c, 0xd4);
  const u32 b = p ? th(c, kThCtor, {p}) : 0;
  st<u32>(c, out, b);
  st<u32>(c, b + 0x74, flags);
  th(c, kThSetBufferSize, {ld<u32>(c, out), size});
  if (with_fmt) th(c, kThSetPrimaryFormat, {ld<u32>(c, out), fmt});
  st<u32>(c, ld<u32>(c, out) + 0x5c, self);
  const u32 rr = th(c, kThCreate, {ld<u32>(c, out), ld<u32>(c, self), ld<u32>(c, self + 0x164)});
  if (!rr) {
    if (neg != 1) ds_append(c, ld<u32>(c, out));
    return 0;
  }
  if (bit2) {
    st<u32>(c, self + 0x158, ld<u32>(c, self + 0x158) - 1);
    st<u32>(c, self + 0x150, ld<u32>(c, self + 0x14c) - ld<u32>(c, self + 0x158));
  }
  if (u32 x = ld<u32>(c, out)) destroy_delete(c, x);
  st<u32>(c, out, 0);
  return rr;
}
// DirectSoundDevice::createBuffer (rva 0x12d10), thiscall (out, flags, size): 3 without bit 1 of +0x6c. neg 1: the new buffer
// is not appended to the list.
void a_DsDev_createBuffer(Ctx &c, const uint32_t *a, Ret &r) {
  if (!(ld<uint8_t>(c, a[0] + 0x6c) & 2)) return r.set_u32(3);
  r.set_u32(create_core(c, a[0], a[1], a[2], a[3], 0, false, NEG(DsDev_createBuffer)));
}
// DirectSoundDevice::createBufferFmt (rva 0x12fe0), thiscall (out, fmt, flags, size): 3 without bit 1 of +0x6c, 0xa with no
// format. neg 1: as createBuffer's.
void a_DsDev_createBufferFmt(Ctx &c, const uint32_t *a, Ret &r) {
  if (!(ld<uint8_t>(c, a[0] + 0x6c) & 2)) return r.set_u32(3);
  if (!a[2]) return r.set_u32(0xa);
  r.set_u32(create_core(c, a[0], a[1], a[3], a[4], a[2], true, NEG(DsDev_createBufferFmt)));
}

// DirectSoundDevice::removeBuffer (rva 0x132e0, c3ex DsRemoveBuf_re), thiscall (b): 3 without bit 1 of +0x6c, 0xa for a NULL
// b; b unlinked when it is on the list; `mov eax, [b+0x98]; or eax, 4; je` (0x13365-0x1336e) never jumps, so +0x158 is
// decremented on every call (model_defects: an original defect kept); releaseIfOwned(b) 0 -> destroy + delete; 0.
// neg 1: +0x158 is decremented only when bit 2 of [b+0x98] is set (the intended test).
void a_DsDev_removeBuffer(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 self = a[0], b = a[1];
  if (!(ld<uint8_t>(c, self + 0x6c) & 2)) return r.set_u32(3);
  if (!b) return r.set_u32(0xa);
  u32 e = ds_head(c);
  while (e && e != b) e = ld<u32>(c, e + 0xc0);
  if (e) ds_unlink(c, e, 0);
  const u32 f98 = ld<u32>(c, b + 0x98);
  if (NEG(DsDev_removeBuffer) == 1 ? (f98 & 4) != 0 : (f98 | 4) != 0)
    st<int32_t>(c, self + 0x158, ld<int32_t>(c, self + 0x158) - 1);
  if (!th(c, kThReleaseIfOwned, {b})) destroy_delete(c, b);
  r.set_u32(0);
}

// DirectSoundDevice::releaseAllBuffers (rva 0x133d0, c3ex): 3 without bit 1 of +0x6c; the list emptied as in releaseAll; 0.
// neg 1: the list's tail is not updated by the unlinks.
void a_DsDev_releaseAllBuffers(Ctx &c, const uint32_t *a, Ret &r) {
  if (!(ld<uint8_t>(c, a[0] + 0x6c) & 2)) return r.set_u32(3);
  for (u32 b = ds_head(c); b; b = ds_head(c)) {
    ds_unlink(c, b, NEG(DsDev_releaseAllBuffers));
    th(c, kThReleaseIfOwned, {b});
    destroy_delete(c, b);
  }
  r.set_u32(0);
}

// unlinkDsBuffer (rva 0x134e0, c3dj UnlinkDs_re), thiscall (buf): 0xa for NULL; unlinked; 0. neg 1: the tail is not updated.
void a_unlinkDsBuffer(Ctx &c, const uint32_t *a, Ret &r) {
  if (!a[1]) return r.set_u32(0xa);
  ds_unlink(c, a[1], NEG(unlinkDsBuffer));
  r.set_u32(0);
}

// DirectSoundDevice::setMasterVolume (rva 0x13560, emu-only row, transcribed from its instructions): +0x188 = volume
// (0x1356e); every buffer of the DsBuffer list from the head (0x13561) along +0xc0 (0x1357e) gets DsBuffer::setVolume(volume)
// through the thunk 0x18f7 (0x13579); `ret 4`. eax is the last setVolume's, or the entry eax with an empty list (the replay
// enters with eax 0). neg 1: +0x188 is not stored.
void a_DsDev_setMasterVolume(Ctx &c, const uint32_t *a, Ret &r) {
  if (NEG(DsDev_setMasterVolume) != 1) st<u32>(c, a[0] + 0x188, a[1]);
  u32 eax = 0;
  for (u32 b = ds_head(c); b; b = ld<u32>(c, b + 0xc0)) eax = th(c, kThSetVolume, {b, a[1]});
  r.set_u32(eax);
}

// DsDevice_registerBuffer (rva 0x32410, c3dj DsRegister_re), thiscall (buf): NULL returns 0; buf+0x20 = this;
// nodeListAppendValue(this+0x1338, buf)'s eax. neg 1: buf+0x20 is not stored.
void a_DsDevice_registerBuffer(Ctx &c, const uint32_t *a, Ret &r) {
  if (!a[1]) return r.set_u32(0);
  if (NEG(DsDevice_registerBuffer) != 1) st<u32>(c, a[1] + 0x20, a[0]);
  r.set_u32(th(c, kThListAppendValue, {a[0] + 0x1338, a[1]}));
}

// getDsCoopLevel (rva 0x3a770, c3fu): `mov eax, [g_dsCoopLevel]; ret` (ecx never read). neg 1: the list tail global is read.
void a_getDsCoopLevel(Ctx &c, const uint32_t *, Ret &r) {
  r.set_u32(NEG(getDsCoopLevel) == 1 ? ds_tail(c) : ld<u32>(c, snd_obj(c, &SoundState::g_dsCoopLevel).va()));
}

}  // namespace

// ================================================================================================ the module table
namespace {
#define G(v) &g_negctl_##v
const FuncDef kFuncs[] = {
    {"snd.DsBuffer::ctor", "thiscall", "pointer", 1, 0, a_DsBuffer_ctor, G(DsBuffer_ctor)},
    {"snd.DsBuffer::destroy", "thiscall", "void", 1, 0, a_DsBuffer_destroy, G(DsBuffer_destroy)},
    {"snd.DsBuffer::restartPlayback", "thiscall", "uint32", 1, 0, a_DsBuffer_restartPlayback, G(DsBuffer_restartPlayback)},
    {"snd.DsBuffer::setBufferSize", "thiscall", "uint32", 2, 0, a_DsBuffer_setBufferSize, G(DsBuffer_setBufferSize)},
    {"snd.DsBuffer::setFrequency", "thiscall", "void", 2, 0, a_DsBuffer_setFrequency, G(DsBuffer_setFrequency)},
    {"snd.DsBuffer::setVolumeRaw", "thiscall", "void", 2, 0, a_DsBuffer_setVolumeRaw, G(DsBuffer_setVolumeRaw)},
    {"snd.DsBuffer::setVolume", "thiscall", "uint32", 2, 0x027f, a_DsBuffer_setVolume, G(DsBuffer_setVolume)},
    {"snd.mapDsError", "default", "int", 1, 0, a_mapDsError, G(mapDsError)},
    {"snd.DsBuffer::initDesc", "thiscall", "uint32", 1, 0, a_DsBuffer_initDesc, G(DsBuffer_initDesc)},
    {"snd.DsBuffer::stop", "thiscall", "uint32", 1, 0, a_DsBuffer_stop, G(DsBuffer_stop)},
    {"snd.DsBuffer::setPosition", "thiscall", "uint32", 1, 0, a_DsBuffer_setPosition, G(DsBuffer_setPosition)},
    {"snd.DsBuffer::lockEntire", "thiscall", "uint32", 1, 0, a_DsBuffer_lockEntire, G(DsBuffer_lockEntire)},
    {"snd.DsBuffer::release", "thiscall", "uint32", 1, 0, a_DsBuffer_release, G(DsBuffer_release)},
    {"snd.DsBuffer::setPrimaryFormat", "thiscall", "uint32", 2, 0, a_DsBuffer_setPrimaryFormat, G(DsBuffer_setPrimaryFormat)},
    {"snd.DsBuffer::create", "thiscall", "uint32", 3, 0, a_DsBuffer_create, G(DsBuffer_create)},
    {"snd.DsBuffer::hasAuxInterfaces", "thiscall", "uint8", 1, 0, a_DsBuffer_hasAuxInterfaces, G(DsBuffer_hasAuxInterfaces)},
    {"snd.DsBuffer::query3D", "thiscall", "uint32", 1, 0, a_DsBuffer_query3D, G(DsBuffer_query3D)},
    {"snd.DsBuffer::queryNotify", "thiscall", "uint32", 1, 0, a_DsBuffer_queryNotify, G(DsBuffer_queryNotify)},
    {"snd.DsBuffer::releaseIfOwned", "thiscall", "uint32", 1, 0, a_DsBuffer_releaseIfOwned, G(DsBuffer_releaseIfOwned)},
    {"snd.DsBuffer::restore", "thiscall", "uint32", 7, 0, a_DsBuffer_restore, G(DsBuffer_restore)},
    {"snd.DsBuffer::unlock", "thiscall", "uint32", 5, 0, a_DsBuffer_unlock, G(DsBuffer_unlock)},
    {"snd.DsBuffer::getPosition", "thiscall", "uint32", 4, 0, a_DsBuffer_getPosition, G(DsBuffer_getPosition)},
    {"snd.DsBuffer::getPlayPosition", "thiscall", "uint32", 3, 0, a_DsBuffer_getPlayPosition, G(DsBuffer_getPlayPosition)},
    {"snd.DsBuffer::lockCaptureRegion", "thiscall", "uint32", 3, 0, a_DsBuffer_lockCaptureRegion, G(DsBuffer_lockCaptureRegion)},
    {"snd.DsBuffer::getTimedPosition", "thiscall", "uint32", 4, 0, a_DsBuffer_getTimedPosition, G(DsBuffer_getTimedPosition)},
    {"snd.DsBuffer::play", "thiscall", "uint32", 2, 0, a_DsBuffer_play, G(DsBuffer_play)},
    {"snd.DsBuffer::isBusy", "thiscall", "uint32", 1, 0, a_DsBuffer_isBusy, G(DsBuffer_isBusy)},
    {"snd.DsBuffer::setRegionMs", "thiscall", "int", 3, 0, a_DsBuffer_setRegionMs, G(DsBuffer_setRegionMs)},
    {"snd.DsBuffer::configureDesc", "thiscall", "int", 2, 0, a_DsBuffer_configureDesc, G(DsBuffer_configureDesc)},
    {"snd.Ds3DBuffer::setPosition", "thiscall", "uint32", 4, 0, a_Ds3DBuffer_setPosition, G(Ds3DBuffer_setPosition)},
    {"snd.Ds3DBuffer::setPositionX", "thiscall", "uint32", 2, 0, a_Ds3DBuffer_setPositionX, G(Ds3DBuffer_setPositionX)},
    {"snd.Ds3DBuffer::setPositionY", "thiscall", "uint32", 2, 0, a_Ds3DBuffer_setPositionY, G(Ds3DBuffer_setPositionY)},
    {"snd.Ds3DBuffer::setPositionZ", "thiscall", "uint32", 2, 0, a_Ds3DBuffer_setPositionZ, G(Ds3DBuffer_setPositionZ)},
    {"snd.DirectSoundDevice::close", "thiscall", "uint32", 1, 0, a_DsDev_close, G(DsDev_close)},
    {"snd.DirectSoundDevice::enumerate", "thiscall", "uint32", 3, 0, a_DsDev_enumerate, G(DsDev_enumerate)},
    {"snd.getDeviceCaps", "thiscall", "int", 1, 0, a_getDeviceCaps, G(getDeviceCaps)},
    {"snd.dsEnumCallbackAppend", "stdcall", "int", 4, 0, a_dsEnumCallbackAppend, G(dsEnumCallbackAppend)},
    {"snd.DirectSoundDevice::releaseAll", "thiscall", "uint32", 1, 0, a_DsDev_releaseAll, G(DsDev_releaseAll)},
    {"snd.DirectSoundDevice::stopAll", "thiscall", "uint32", 1, 0, a_DsDev_stopAll, G(DsDev_stopAll)},
    {"snd.DirectSoundDevice::create", "thiscall", "uint32", 3, 0, a_DsDev_create, G(DsDev_create)},
    {"snd.return0Pop4", "thiscall", "uint32", 2, 0, a_return0Pop4, G(return0Pop4)},
    {"snd.DirectSoundDevice::setCooperativeLevel", "thiscall", "uint32", 2, 0, a_DsDev_setCooperativeLevel,
     G(DsDev_setCooperativeLevel)},
    {"snd.DirectSoundDevice::duplicateBuffer", "thiscall", "uint32", 4, 0, a_DsDev_duplicateBuffer, G(DsDev_duplicateBuffer)},
    {"snd.DirectSoundDevice::freeBufferById", "thiscall", "uint32", 3, 0, a_DsDev_freeBufferById, G(DsDev_freeBufferById)},
    {"snd.DsBuffer::scalarDeletingDtor", "thiscall", "pointer", 2, 0, a_DsBuffer_scalarDeletingDtor,
     G(DsBuffer_scalarDeletingDtor)},
    {"snd.DirectSoundDevice::createBuffer", "thiscall", "uint32", 4, 0, a_DsDev_createBuffer, G(DsDev_createBuffer)},
    {"snd.DirectSoundDevice::createBufferFmt", "thiscall", "uint32", 5, 0, a_DsDev_createBufferFmt, G(DsDev_createBufferFmt)},
    {"snd.DirectSoundDevice::removeBuffer", "thiscall", "uint32", 2, 0, a_DsDev_removeBuffer, G(DsDev_removeBuffer)},
    {"snd.DirectSoundDevice::releaseAllBuffers", "thiscall", "uint32", 1, 0, a_DsDev_releaseAllBuffers,
     G(DsDev_releaseAllBuffers)},
    {"snd.unlinkDsBuffer", "thiscall", "uint32", 2, 0, a_unlinkDsBuffer, G(unlinkDsBuffer)},
    {"snd.DirectSoundDevice::setMasterVolume", "thiscall", "uint32", 2, 0, a_DsDev_setMasterVolume, G(DsDev_setMasterVolume)},
    {"snd.Ds3DListener::setPosition", "thiscall", "uint32", 4, 0, a_Ds3DListener_setPosition, G(Ds3DListener_setPosition)},
    {"snd.DsDevice_registerBuffer", "thiscall", "uint32", 2, 0, a_DsDevice_registerBuffer, G(DsDevice_registerBuffer)},
    {"snd.getDsCoopLevel", "thiscall", "int", 1, 0, a_getDsCoopLevel, G(getDsCoopLevel)},
};
#undef G
}  // namespace

extern const ModuleDef kModule_audio_ds = {"audio_ds", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim

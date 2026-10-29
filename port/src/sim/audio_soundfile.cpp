// sound.dll's sound-bank loader loadSoundFile (step S4.9b, log/roadmap/S4/9b_notes.md; API in portsim/sim/audio.h): rva 0x41d0, slot 4
// (+0x10) of the Sound vtable rva 0x5b000 that Sound::ctor installs, __thiscall (this, name), `ret 4` (0x507f). Written from the
// instructions of the user's sound.dll (there is no hook source and no path-1 row); judged by the emulated original
// (log/roadmap/S4/9b_replay.py). Addresses in comments are RVAs of the user's sound.dll.
//
// The file is read chunk by chunk through the WINMM mmio imports (snd_import: with a host the emulator's stand-in answers, with none the
// AudioSink receives `WINMM!mmioXxx` with the original's argument slots; the module never reads a file itself). A 4-byte tag is read
// at the top of the loop (0x43e8) and dispatched (0x43f2-0x4606): 'ctrl' readFmtChunk, 'trak' a Ctrl5b668 on a local list, 'glbl'
// NodeB000 tracks, 'chnl' a channel record into the sequencer table, 'kmap' a MixBuffer and its voice slots, 'prgm' a channel program,
// 'MThd' (the four characters of the module string at rva 0x63d90, through mmioStringToFOURCCA 0x41f9) ends the bank: the MIDI image
// is loaded and the tracks are applied; any other tag closes the file and returns 8. mmioRead results are never tested, so a file
// that ends inside the loop keeps the last tag and the loop never ends (the original's behaviour; U-1490).
//
// Every call to code that is not inline goes through the dispatcher (snd_call by the original's thunk rva, snd_vcall, snd_import), so
// a fixture's stand-in, the host, another port function or the AudioSink serves it. The SEH frame (0x41d0-0x41de, the state byte
// stores at entry esp - 4 and the fs:[0] restore at 0x5072) is not modelled: no callee of this function unwinds through it in the
// paths the port runs (U-1483, as S4.9).
//
// Negative controls (sg_negctl_set("snd.loadSoundFile", mode)): 1 the return value ^ 1; 2..10 one wrong detail per chunk handler
// (kNeg* below), each dormant unless its mode is set.
#include <cstdint>
#include <cstring>

#include "portsim/ptr.h"
#include "portsim/registry.h"
#include "portsim/sim/audio.h"
#include "portsim/sim/audio_dev.h"
#include "portsim/sim/soundstate.h"

namespace portsim {

namespace {
using u32 = uint32_t;

int g_negctl_loadSoundFile = 0;
enum : int {
  kNegReturn = 1,       // the result ^ 1
  kNegCtrl = 2,         // 'ctrl': readFmtChunk not called
  kNegTrak = 3,         // 'trak': the new node holds NULL instead of the object (the count at E-0x40c is never read: an equivalent mutant)
  kNegGlbl = 4,         // 'glbl': Voice_setPlayModeBits not called
  kNegChnl = 5,         // 'chnl': Seq_copyChannelParams not called
  kNegKmap = 6,         // 'kmap': the trailing dword not stored at the MixBuffer's +0x218
  kNegPrgm = 7,         // 'prgm': the flag byte stays 0 for a 0x1c-byte chunk
  kNegMthd = 8,         // 'MThd': Seq_applyTrackFlag skipped in the track loop
  kNegPrefix = 9,       // path prefix: the strrchr cut of the Sound's own name not applied
  kNegUnknown = 10,     // unknown tag: g_openMmioCount not lowered
};
inline bool neg(int mode) { return SG_NEGCTL(g_negctl_loadSoundFile) == mode; }

// Locals the original passes by address live in the core's stack frame (portsim/sim/audio.h SndFrame, S4.2b): at the
// original's offsets below the entry esp when the harness supplies it, else in the core's frame block. `at(off)` is the
// local at entry - off.

// the WINMM IAT slots of sound.dll the function calls (rvas of the slots; the import name table of the user's file)
constexpr u32 kMmioStringToFourcc = 0xb865c, kMmioOpen = 0xb8660, kMmioRead = 0xb8664, kMmioGetInfo = 0xb8668,
              kMmioSeek = 0xb866c, kMmioClose = 0xb8670;
// callees by the original's incremental-link thunks (the call sites cite them; the dispatcher follows the jmp)
constexpr u32 kCtrlObject3Ctor = 0x2644, kChanRecordInit = 0x14bf, kInit2e3c0 = 0x132f, kPumpWaveMessages = 0x1997,
              kListPopFront = 0x1fbe, kChanRecordFreeBuffers = 0x2004, kCtrlObjectDtorC = 0x1ab9, kSeqCopyChannelParams = 0x2711,
              kVoiceSetPlayModeBits = 0x1127, kNodeB000Ctor = 0x1410, kSeqBuildTrack14 = 0x23ab, kCtrl5b668Ctor = 0x2180,
              kReadFmtChunk = 0x1870, kMixBufferCtor = 0x1a73, kVoiceSetName = 0x1591, kVoiceSlotInit = 0x2621,
              kGetField204 = 0x25f4, kSoundFindByName = 0x14c9, kAllocVoiceSlot = 0x24c8, kSoundReapIdle = 0x1a55,
              kSoundSetName = 0x10f5, kSoundSetAltName = 0x1604, kSeqSetupChannel = 0x118b, kSeqUpdateChannel = 0x1550,
              kMidiLoadMemoryAndPreRoll = 0x242d, kSeqApplyTrackFlag = 0x1249, kProcessWaveChunks = 0x272f,
              kSeqListGetAt = 0x15fa, kStrrchr = 0x424b0;
constexpr u32 kSoundWindowObj = 0xb49b8;     // the module object pumpWaveMessages runs on (the `mov ecx` immediate at 0x425a)
constexpr u32 kStrMThd = 0x63d90;            // "MThd" (the `push` immediate at 0x41f4)
constexpr u32 kPathString = 0x69988;         // the module string the prefix is compared with (the `mov edi` immediates at 0x4881 / 0x4b37)
// the chunk tags (dword compares 0x43fa-0x4601)
constexpr u32 kTagCtrl = 0x6c727463, kTagTrak = 0x6b617274, kTagGlbl = 0x6c626c67, kTagChnl = 0x6c6e6863, kTagPrgm = 0x6d677270,
              kTagKmap = 0x70616d6b;

// Frame (entry esp E; 0x41e5 `sub esp, 0x40c` below the 0xc-byte SEH record and four pushes: the loop runs at E - 0x428).
// Offsets from E, with the `[esp + n]` they come from at esp = E - 0x428:
constexpr u32 kMmioInfo = 0x54;       // MMIOINFO filled by mmioGetInfo ([esp + 0x3d4]), 0x48 bytes up to the SEH record
constexpr u32 kPrefix2 = 0x154;       // 'prgm' path buffer ([esp + 0x2d4]), 0x100 bytes
constexpr u32 kPrefix1 = 0x254;       // 'kmap' path buffer ([esp + 0x1d4]), 0x100 bytes
constexpr u32 kName = 0x358;          // name buffer ([esp + 0xd0]), 0x104 bytes
constexpr u32 kCtrl3 = 0x390;         // CtrlObject3 local ([esp + 0x98]): 'chnl' reads over it
constexpr u32 kChan = 0x3b8;          // ChanRecord local ([esp + 0x70]), 0x28 bytes: 'prgm' reads over it
constexpr u32 kRecD2 = 0x3bc;         // 'kmap' record dword 2 ([esp + 0x6c])
constexpr u32 kRecD1 = 0x3c0;         // 'kmap' record dword 1 ([esp + 0x68])
constexpr u32 kFccMThd = 0x3c4;       // mmioStringToFOURCCA("MThd") ([esp + 0x64])
constexpr u32 kTrail = 0x3c8;         // 'kmap' trailing dword ([esp + 0x60])
constexpr u32 kTag = 0x3cc;           // the tag read ([esp + 0x5c])
constexpr u32 kGlbl = 0x3d8;          // the 12-byte local {record size, record count, mode} ([esp + 0x50]) set by 0x2e3c0
constexpr u32 kKmapCount = 0x3dc;     // ([esp + 0x4c])
constexpr u32 kPrgmLen = 0x3e0;       // ([esp + 0x48])
constexpr u32 kGetAtOut = 0x3e4;      // Seq_listGetAt's out slot ([esp + 0x44])
constexpr u32 kPrgmFlag = 0x3e8;      // the byte flag passed to Seq_setupChannel as a dword ([esp + 0x40])
constexpr u32 kGlblLen = 0x3ec;       // ([esp + 0x3c])
constexpr u32 kRecSize = 0x3f0;       // 'kmap' record size ([esp + 0x38])
constexpr u32 kOwnName = 0x3f4;       // this + 0x948 ([esp + 0x34] after 0x4330)
constexpr u32 kIndex = 0x3f8;         // operator new's result / the 'kmap' record index ([esp + 0x30])
constexpr u32 kUsed = 0x3fc;          // bytes consumed ([esp + 0x2c])
constexpr u32 kSlot = 0x400;          // the VoiceSlot ([esp + 0x28])
constexpr u32 kMix = 0x404;           // the MixBuffer ([esp + 0x24])
constexpr u32 kSize = 0x408;          // the chunk size ([esp + 0x20])
constexpr u32 kList = 0x418;          // the local list {head +0, tail +4, +8, count +0xc} ([esp + 0x10])
constexpr u32 kFrameBytes = 0x434;    // the deepest esp of the body (E - 0x428 and three argument pushes)

struct Lsf {
  Ctx &c;
  u32 self;
  SndFrame F;
  u32 h = 0;   // the HMMIO (this + 0xa48)
  Lsf(Ctx &ctx, u32 s) : c(ctx), self(s), F(ctx, kFrameBytes) {}

  u32 L(u32 off) const { return F.at(off); }
  u32 rd(u32 va) const { return ld<u32>(c, va); }
  void wr(u32 va, u32 v) const { st<u32>(c, va, v); }
  uint8_t rb(u32 va) const { return ld<uint8_t>(c, va); }
  void wb(u32 va, uint8_t v) const { st<uint8_t>(c, va, v); }

  u32 mmread(u32 buf, u32 n) { return snd_import(c, kMmioRead, SndAbi::Stdcall, {h, buf, n}).eax; }
  u32 thiscall(u32 rva, u32 obj, std::initializer_list<u32> args = {}) {
    uint32_t a[kSndMaxArgs];
    int n = 0;
    a[n++] = obj;
    for (u32 v : args) a[n++] = v;
    return snd_call_va(c, snd_va(c, rva), SndAbi::Thiscall, a, n).eax;
  }
  u32 strrchr_(u32 s) { return snd_call(c, kStrrchr, SndAbi::Cdecl, {s, 0x5c}).eax; }
  u32 mmio_count_va() const { return snd_obj(c, &SoundState::g_openMmioCount).va(); }

  // One node off the local list's front: {prev +0, next +4, value +8} nodes, head / tail at E-0x418 / E-0x414, count E-0x40c
  // (the inline sequence at 0x428f-0x42bb and its copies). Returns the node's value.
  u32 pop_front() {
    const u32 node = rd(L(kList));
    const u32 next = rd(node + 4);
    if (next) {
      wr(next, 0);
      wr(L(kList), next);
    } else {
      wr(L(kList) + 4, 0);
      wr(L(kList), 0);
    }
    const u32 v = rd(node + 8);
    snd_delete(c, node);
    wr(L(kList) + 0xc, rd(L(kList) + 0xc) - 1);
    return v;
  }
  // 0x427f-0x42cc / 0x4348-0x4395 / 0x4c9c-0x4ceb: one inline pop, then listPopFront (thunk 0x1fbe) until it returns 0
  void drain_pop() {
    if (!rd(L(kList))) return;
    if (pop_front() == 0) return;
    while (snd_call(c, kListPopFront, SndAbi::Thiscall, {L(kList)}).eax != 0) {
    }
  }
  // 0x4d35-0x4db4 / 0x4e3e-0x4eae / 0x4fc3-0x503b: inline pops while the popped value is non-NULL and a node is left
  void drain_inline() {
    if (!rd(L(kList))) return;
    if (pop_front() == 0) return;
    while (rd(L(kList))) {
      if (pop_front() == 0) return;
    }
  }
  // the common exit: ChanRecord_freeBuffers on the ChanRecord local, CtrlObject_dtorC on the CtrlObject3 local
  u32 finish(u32 r) {
    thiscall(kChanRecordFreeBuffers, L(kChan));  // 0x42da (and 0x43a3, 0x4cf9, 0x4dc2, 0x4ebc, 0x5049)
    thiscall(kCtrlObjectDtorC, L(kCtrl3));       // 0x42f1 (and 0x43ba, 0x4d10, 0x4dd9, 0x4ed3, 0x5060)
    return r;
  }
  // a NUL-ended name read a byte at a time after its first byte (0x46ab-0x46c4 and copies); returns the bytes read after the first
  u32 read_rest_of_name() {
    u32 n = 0;
    if (!rb(L(kName))) return 0;
    u32 p = L(kName) + 1;
    for (;;) {
      mmread(p, 1);
      const uint8_t ch = rb(p);
      n++;
      p++;
      if (!ch) return n;
    }
  }
  // The path prefix of kmap / prgm names (bit 0 of +0x58; 0x4819-0x491f and 0x4acf-0x4bd5): the Sound's own name (+0x948, kept at
  // E-0x3f4) is copied to `buf` and cut after its last backslash (none: buf empty); a name with a backslash whose prefix differs
  // from the module string at rva 0x69988 (the inline compare 0x488d-0x48b6) gets buf + the name from that backslash on, else
  // buf + the whole name; the result replaces the name.
  void prefix(u32 buf) {
    const u32 own = rd(L(kOwnName));  // 0x4819
    if (own) {                        // 0x481f
      vstrcpy(c.mem, buf, own);       // 0x4821-0x4847
      const u32 r = strrchr_(buf);    // 0x4851
      if (r && !neg(kNegPrefix))      // 0x485b
        wb(r + 1, 0);                 // 0x485d
      else if (!r)
        wb(buf, 0);                   // 0x4863
    } else {
      wb(buf, 0);  // 0x4863
    }
    const u32 r2 = strrchr_(L(kName));  // 0x4875
    u32 src = L(kName);
    if (r2) {                                      // 0x487f
      if (vstrcmp_ne(buf, snd_va(c, kPathString)))  // 0x488d-0x48b8
        src = r2;                                  // 0x48c1
    }
    vstrcat(c.mem, buf, src);        // 0x48d3-0x48fd
    vstrcpy(c.mem, L(kName), buf);   // 0x48ff-0x491f
  }
  bool vstrcmp_ne(u32 a, u32 b) const {
    for (;;) {
      const uint8_t x = rb(a), y = rb(b);
      if (x != y) return true;
      if (!x) return false;
      a++;
      b++;
    }
  }

  // ---- the chunk handlers ----------------------------------------------------------------------------------------------------
  // 'chnl' (0x4428): size, size bytes over the CtrlObject3 local, Seq_copyChannelParams(this, local), mmioGetInfo into E-0x54
  void chnl() {
    mmread(L(kSize), 4);                // 0x4436
    mmread(L(kCtrl3), rd(L(kSize)));    // 0x444c
    if (!neg(kNegChnl)) thiscall(kSeqCopyChannelParams, self, {L(kCtrl3)});  // 0x4458
    snd_import(c, kMmioGetInfo, SndAbi::Stdcall, {h, L(kMmioInfo), 0});       // 0x446e
  }
  // 'glbl' (0x4479): size, n, n bytes into the 12-byte local; Voice_setPlayModeBits(this, local[2]); a NodeB000 read `count` times
  // (local[1], decremented in place: after the loop it holds 0xffffffff) and passed to Seq_buildTrack14; the node freed
  void glbl() {
    mmread(L(kSize), 4);                              // 0x4487
    mmread(L(kGlblLen), 4);                           // 0x4497
    wr(L(kSize), rd(L(kSize)) - 4);                   // 0x44a1-0x44a8
    mmread(L(kGlbl), rd(L(kGlblLen)));                // 0x44b5
    wr(L(kSize), rd(L(kSize)) - rd(L(kGlblLen)));     // 0x44b7-0x44c1
    if (!neg(kNegGlbl)) thiscall(kVoiceSetPlayModeBits, self, {rd(L(kGlbl) + 8)});  // 0x44c5-0x44cc
    u32 node = snd_new(c, 0x38);                      // 0x44d3
    wr(L(kIndex), node);                              // 0x44db
    const u32 esi = node ? thiscall(kNodeB000Ctor, node) : 0;  // 0x44e9-0x44f6
    for (;;) {
      const u32 cnt = rd(L(kGlbl) + 4);  // 0x44f8 / 0x4534
      wr(L(kGlbl) + 4, cnt - 1);         // 0x4509 / 0x453d
      if (cnt == 0) break;               // 0x450d / 0x4541
      mmread(esi, rd(L(kGlbl)));         // 0x451c
      wr(L(kSize), rd(L(kSize)) - rd(L(kGlbl)));  // 0x451e-0x452b
      thiscall(kSeqBuildTrack14, self, {esi});    // 0x452f
    }
    snd_delete(c, esi);  // 0x4544
  }
  // 'trak' (0x4551): a Ctrl5b668 (0x34 bytes) read over from +0 with the chunk's size, appended to the local list in a {0, 0, obj} node
  void trak() {
    u32 obj = snd_new(c, 0x34);                              // 0x4553
    wr(L(kIndex), obj);                                      // 0x455b
    const u32 esi = obj ? thiscall(kCtrl5b668Ctor, obj) : 0;  // 0x4569-0x4576
    mmread(L(kSize), 4);                                     // 0x458e
    mmread(esi, rd(L(kSize)));                               // 0x459d
    u32 node = snd_new(c, 0xc);                              // 0x45a1
    if (node) {                                              // 0x45ab
      wr(node, 0);                                           // 0x45ad
      wr(node + 4, 0);                                       // 0x45b3
      wr(node + 8, neg(kNegTrak) ? 0 : esi);                 // 0x45ba
    }
    const u32 tail = rd(L(kList) + 4);  // 0x45c1
    if (tail) {                         // 0x45c7
      wr(tail + 4, node);               // 0x45c9
      wr(node, rd(L(kList) + 4));       // 0x45cc-0x45d0
    } else {
      wr(L(kList), node);  // 0x45d4
    }
    wr(L(kList) + 4, node);                                                    // 0x45d8
    wr(L(kList) + 0xc, rd(L(kList) + 0xc) + 1);                               // 0x45dc-0x45e1
  }
  // 'kmap' (0x460c)
  void kmap() {
    u32 mix = snd_new(c, 0x21c);                             // 0x4611
    wr(L(kIndex), mix);                                      // 0x4619
    u32 esi = mix ? thiscall(kMixBufferCtor, mix) : 0;       // 0x4627-0x4634
    wr(L(kMix), esi);                                        // 0x4644
    mmread(L(kSize), 4);                                     // 0x4650
    mmread(esi + 0x208, 4);                                  // 0x4662
    mmread(esi + 0x210, 4);                                  // 0x4674
    mmread(esi + 0x20c, 4);                                  // 0x4686
    mmread(L(kName), 1);                                     // 0x4699
    u32 edi = 0xd + read_rest_of_name();                     // 0x46a2-0x46c4
    esi = rd(L(kMix));                                       // 0x46c6
    thiscall(kVoiceSetName, esi, {L(kName)});                // 0x46d4
    mmread(L(kKmapCount), 4);                                // 0x46e7
    if (rb(esi + 0x208) & 6) {                               // 0x46e9-0x46f8
      mmread(L(kRecSize), 4);                                // 0x4708
      edi += 4;                                              // 0x470a
    }
    wr(L(kIndex), 0);                                        // 0x4712
    u32 slot = snd_new(c, 0x110);                            // 0x471a
    wr(L(kSlot), slot);                                      // 0x4722
    esi = slot ? thiscall(kVoiceSlotInit, slot) : 0;         // 0x4730-0x473d
    wr(L(kSlot), esi);                                       // 0x4743
    if (rd(L(kKmapCount)) != 0) {                            // 0x4747 / 0x4751 (unsigned <= 0)
      for (;;) {
        if (edi >= rd(L(kSize))) break;                      // 0x4757 / 0x475b
        const u32 flags = rd(L(kMix)) + 0x208;               // 0x4761-0x4769
        wr(L(kIndex), rd(L(kIndex)) + 1);                    // 0x4765-0x476f
        if ((rb(flags) & 6) == 0) {                          // 0x4773 / 0x4776
          mmread(L(kRecD1), 4);                              // 0x4786
          mmread(L(kRecD2), 4);                              // 0x4796
          edi += 8;                                          // 0x479f
          const u32 d2 = rd(L(kRecD2));                      // 0x4798
          if (rd(esi + 4) > d2) wr(esi + 4, d2);             // 0x479c-0x47a6
          wr(esi, d2);                                       // 0x47a9
          const u32 d1 = rd(L(kRecD1));                      // 0x47ab
          if (d2 < d1) wr(esi, d1);                          // 0x47af-0x47b3
          wr(esi + 4, d1);                                   // 0x47b5
        } else {
          mmread(esi, rd(L(kRecSize)));                      // 0x47c7
          edi += rd(L(kRecSize));                            // 0x47c9
        }
        mmread(L(kName), 1);                                 // 0x47de
        edi += 1;                                            // 0x47e7
        wr(L(kUsed), edi);                                   // 0x47ea
        if (rb(L(kName))) {                                  // 0x47e8 / 0x47ee
          edi += read_rest_of_name();                        // 0x47f0-0x4809
          wr(L(kUsed), edi);                                 // 0x480b
        }
        if (rb(self + 0x58) & 1) prefix(L(kPrefix1));        // 0x480f / 0x4813
        vstrcpy(c.mem, rd(L(kSlot)) + 0xc, L(kName));        // 0x4921-0x494a
        const u32 nm = thiscall(kGetField204, rd(L(kMix)));  // 0x494c / 0x4950
        // Sound_findByName never reads ecx (`mov ecx, ebp` 0x4956 sets it; its body walks the module list): stdcall with the name
        if ((snd_call(c, kSoundFindByName, SndAbi::Stdcall, {nm}).eax & 0xff) == 0)  // 0x4958 / 0x495d
          thiscall(kAllocVoiceSlot, rd(L(kMix)), {rd(L(kSlot))});           // 0x4961-0x496a
        const u32 i = rd(L(kIndex));                         // 0x496f
        const u32 cnt = rd(L(kKmapCount));                   // 0x4973
        esi = rd(L(kSlot));                                  // 0x4977
        edi = rd(L(kUsed));                                  // 0x497b
        if (!(i < cnt)) break;                               // 0x497f / 0x4981
      }
    }
    snd_delete(c, esi);                                      // 0x4988
    if (rd(L(kSize)) - edi >= 4) {                           // 0x498d-0x4999 (unsigned)
      mmread(L(kTrail), 4);                                  // 0x49a9
      esi = rd(L(kMix));                                     // 0x49ab
      if (!neg(kNegKmap)) wr(esi + 0x218, rd(L(kTrail)));   // 0x49af / 0x49b3
    } else {
      esi = rd(L(kMix));  // 0x49bb
    }
    snd_vcall(c, self, 0xd8, SndAbi::Thiscall, {esi});       // 0x49c5
    if (esi) snd_vcall(c, esi, 0, SndAbi::Thiscall, {1});     // 0x49cd / 0x49d5
    snd_call(c, kSoundReapIdle, SndAbi::Cdecl, {});           // 0x49d7
  }
  // 'prgm' (0x49e1)
  void prgm() {
    wb(L(kPrgmFlag), 0);                                     // 0x49ef
    mmread(L(kSize), 4);                                     // 0x49f4
    if (rd(L(kSize)) == 0x1c) {                              // 0x49f6 / 0x49fb
      if (!neg(kNegPrgm)) wb(L(kPrgmFlag), 1);               // 0x4a0b
      mmread(L(kPrgmLen), 4);                                // 0x4a10
      mmread(L(kChan), 0x18);                                // 0x4a20
      const u32 n = rd(L(kPrgmLen));                         // 0x4a26
      const u32 v = rd(L(kChan) + 4);                        // 0x4a22
      wr(L(kChan) + 4, (v & ~0xffu) | ((((v & 0xffu) | 0x10u) & 0xf7u)));  // 0x4a30-0x4a35
      mmread(L(kName), n);                                   // 0x4a42
      const u32 n2 = rd(L(kPrgmLen));                        // 0x4a44
      wb(L(kName) + n2, 0);                                  // 0x4a4b
      wr(L(kUsed), n2 + 0x18);                               // 0x4a48 / 0x4a53
    } else {
      snd_import(c, kMmioGetInfo, SndAbi::Stdcall, {h, L(kMmioInfo), 0});  // 0x4a6a
      mmread(L(kChan), 0x1c);                                // 0x4a7e
      mmread(L(kName), 1);                                   // 0x4a91
      const u32 edi = 1 + read_rest_of_name();               // 0x4a9a-0x4abc
      wr(L(kUsed), edi + 0x1c);                              // 0x4abe / 0x4ac1
    }
    if (rb(self + 0x58) & 1) prefix(L(kPrefix2));            // 0x4ac5 / 0x4ac9
    thiscall(kSoundSetName, L(kChan), {L(kName)});           // 0x4be3
    if (rd(L(kUsed)) < rd(L(kSize))) {                       // 0x4be8-0x4bf2 (unsigned)
      mmread(L(kName), 1);                                   // 0x4c05
      read_rest_of_name();                                   // 0x4c07-0x4c2a
      thiscall(kSoundSetAltName, L(kChan), {L(kName)});      // 0x4c38
    }
    thiscall(kSeqSetupChannel, self, {L(kChan), rd(L(kPrgmFlag))});  // 0x4c3d-0x4c49 (the flag local read as a dword)
    thiscall(kSeqUpdateChannel, self, {L(kChan)});           // 0x4c55
    thiscall(kSoundSetName, L(kChan), {0});                  // 0x4c60
    thiscall(kSoundSetAltName, L(kChan), {0});               // 0x4c6b
    std::memset(c.mem.at(L(kChan), 0x28, true), 0, 0x28);    // 0x4c70-0x4c7b `rep stosd` of 0xa dwords
  }
  // 'MThd' (0x4c82): seek back over the tag; the MIDI image is loaded (Midi_loadMemoryAndPreRoll) and, on success, the tracks of the
  // local list are applied and released, processWaveChunks runs and each record of the +0x218 list marks its track range
  u32 mthd(u32 name) {
    if (snd_import(c, kMmioSeek, SndAbi::Stdcall, {h, 0xfffffffcu, 1}).eax == 0xffffffffu) {  // 0x4c8d / 0x4c93
      drain_pop();       // 0x4c9c-0x4ceb (the file stays open, g_openMmioCount unchanged)
      return finish(8);  // 0x4d15
    }
    if (thiscall(kMidiLoadMemoryAndPreRoll, self, {h, name}) != 0) {  // 0x4de8-0x4e00
      snd_import(c, kMmioClose, SndAbi::Stdcall, {h, 0});             // 0x4e10
      wr(mmio_count_va(), rd(mmio_count_va()) - 1);                   // 0x4e16-0x4e26
      wr(self + 0xa48, 0);                                            // 0x4e2e
      const u32 own = rd(L(kOwnName));                                // 0x4e1c
      std::memset(c.mem.at(own, 0x100, true), 0, 0x100);              // 0x4e21-0x4e3c `rep stosd` of 0x40 dwords
      drain_inline();                                                 // 0x4e3e-0x4eae
      return finish(0x12);                                            // 0x4ed8
    }
    snd_import(c, kMmioClose, SndAbi::Stdcall, {h, 0});  // 0x4eec
    wr(mmio_count_va(), rd(mmio_count_va()) - 1);        // 0x4ef2-0x4ef8
    wr(self + 0xa48, 0);                                 // 0x4efd
    while (rd(L(kList))) {                               // 0x4f03 / 0x4f09
      const u32 obj = pop_front();                       // 0x4f0b-0x4f35
      if (!obj) break;                                   // 0x4f39
      if (!neg(kNegMthd)) thiscall(kSeqApplyTrackFlag, self, {obj});  // 0x4f3e
      snd_vcall(c, obj, 0, SndAbi::Thiscall, {1});       // 0x4f43-0x4f49
    }
    thiscall(kProcessWaveChunks, self);  // 0x4f4f
    u32 cur = rd(self + 0x218);          // 0x4f54
    wr(self + 0x220, cur);               // 0x4f5c
    u32 rec = cur ? rd(cur + 8) : 0;     // 0x4f62-0x4f69
    wr(L(kGetAtOut), 0);                 // 0x4f6d
    while (rec) {                        // 0x4f71 / 0x4fc1
      u32 i = rd(rec + 8);               // 0x4f73
      if (i <= rd(rec + 0xc)) {          // 0x4f76-0x4f7b
        for (;;) {
          if (thiscall(kSeqListGetAt, self + 0x60, {L(kGetAtOut), i}) == 0)  // 0x4f7d-0x4f8f
            wr(rd(L(kGetAtOut)) + 0x458, rec);                               // 0x4f91 / 0x4f95
          i++;                                                               // 0x4f9e
          if (!(i <= rd(rec + 0xc))) break;                                  // 0x4f9b / 0x4fa1
        }
      }
      cur = rd(self + 0x220);  // 0x4fa5
      if (!cur) break;         // 0x4fad
      cur = rd(cur + 4);       // 0x4faf
      wr(self + 0x220, cur);   // 0x4fb4
      if (!cur) break;         // 0x4fba
      rec = rd(cur + 8);       // 0x4fbc
    }
    drain_inline();     // 0x4fc3-0x503b
    return finish(0);   // 0x5065
  }

  u32 run(u32 name) {
    const u32 fcc = snd_import(c, kMmioStringToFourcc, SndAbi::Stdcall, {snd_va(c, kStrMThd), 0}).eax;  // 0x41f1-0x41f9
    wr(L(kFccMThd), fcc);                    // 0x4206
    thiscall(kCtrlObject3Ctor, L(kCtrl3));   // 0x41ff / 0x420a
    thiscall(kChanRecordInit, L(kChan));     // 0x420f / 0x421a
    wr(L(kGlblLen), 0);                      // 0x422b
    thiscall(kInit2e3c0, L(kGlbl));          // 0x421f / 0x422f
    for (u32 k = 0; k < 0x10; k += 4) wr(L(kList) + k, 0);  // 0x4234-0x4240
    wr(L(kRecSize), 0);                      // 0x4254
    if (rb(self + 0x214) & 0x40)             // 0x4244 / 0x4252
      thiscall(kPumpWaveMessages, snd_va(c, kSoundWindowObj));  // 0x425a / 0x425f
    if (rb(self + 0xac) & 1) {               // 0x4264
      const u32 r = snd_vcall(c, self, 0x14, SndAbi::Thiscall, {}).eax;  // 0x4271-0x4276
      if (r) {                               // 0x427b / 0x427d
        drain_pop();                         // 0x427f-0x42cc
        return finish(r);                    // 0x42f6
      }
    }
    wr(self + 0xa48, 0);                     // 0x4309
    vstrcpy(c.mem, self + 0x948, name);      // 0x42fd-0x432e
    wr(L(kOwnName), self + 0x948);           // 0x4330
    h = snd_import(c, kMmioOpen, SndAbi::Stdcall, {self + 0x948, 0, 0}).eax;  // 0x4334
    wr(self + 0xa48, h);                     // 0x433c
    if (!h) {                                // 0x4342
      drain_pop();                           // 0x4348-0x4395
      return finish(8);                      // 0x43bf
    }
    wr(mmio_count_va(), rd(mmio_count_va()) + 1);  // 0x43c9-0x43d5
    for (;;) {
      h = rd(self + 0xa48);                  // 0x43da
      mmread(L(kTag), 4);                    // 0x43e8
      const u32 tag = rd(L(kTag));           // 0x43ea
      if (tag == rd(L(kFccMThd))) return mthd(name);  // 0x43f2 / 0x43f4
      if (tag == kTagCtrl) {                 // 0x43fa / 0x4405
        if (!neg(kNegCtrl)) thiscall(kReadFmtChunk, self);  // 0x45ec
      } else if (tag == kTagTrak) {          // 0x4410
        trak();
      } else if (tag == kTagGlbl) {          // 0x441b
        glbl();
      } else if (tag == kTagChnl) {          // 0x4422
        chnl();
      } else if (tag == kTagPrgm) {          // 0x45fb
        prgm();
      } else if (tag == kTagKmap) {          // 0x4606
        kmap();
      } else {                               // 0x4d1f: any other tag
        snd_import(c, kMmioClose, SndAbi::Stdcall, {h, 0});                  // 0x4d29
        if (!neg(kNegUnknown)) wr(mmio_count_va(), rd(mmio_count_va()) - 1);  // 0x4d2f-0x4d3c
        drain_inline();                      // 0x4d35-0x4db4 (+0xa48 keeps the closed handle)
        return finish(8);                    // 0x4dde
      }
    }
  }
};

void a_loadSoundFile(Ctx &ctx, const uint32_t *a, Ret &r) {
  Lsf f(ctx, a[0]);
  u32 v = f.run(a[1]);
  if (neg(kNegReturn)) v ^= 1u;
  r.set_u32(v);
}

const FuncDef kFuncs[] = {
    {"snd.loadSoundFile", "thiscall", "uint32", 2, 0, a_loadSoundFile, &g_negctl_loadSoundFile},
};

}  // namespace

extern const ModuleDef kModule_audio_soundfile = {"audio_soundfile", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim

// C3 batch c3cx (round 23, 2026-10-09) of sound.dll: never-offered audio and CRT rows (log/c3/c3cx_candidates.tsv).
// Addresses in SG_HOOK and in the `// 0x...` comment lines are RVAs; the instruction addresses quoted inside each
// body are VAs (VA = 0x10000000 + RVA), which is how re/tools/asm2inline.py prints sound.dll.
// Hand-written from the disassembly (py -3.12 re/tools/asm2inline.py sound.dll 0x<va> --list) and the C2
// transcriptions in re/analysis/audio and re/analysis/crt. sound.dll is LoadLibrary'd after the shim starts, so these
// hooks install from the LoadLibraryA hook (SgHooksInstallPending, re/hooks.cpp).
//
// Groups:
//   - the 3D position chain: Channel_set3DPositionX/Y/Z (channel vtable 0x1005bb90 callees) and the MultiSound
//     (vtable 0x1005c7e8) slots that forward to them on the channel embedded at MultiSound+0x70; the Ds3DBuffer
//     object and its IDirectSound3DBuffer-shaped interface are fixture-owned, with a recording slot +0x4c;
//   - the rate/timer family: Seq_scaleTickRate, Sound::scaleTickRate, Channel_setRateGlide, MultiSound::setRateGlide,
//     MultiSound::setReverseRate. They read the module global g_soundTimer 0x100b49ec (a pointer whose first dword is
//     the timer period); the fixtures swap that pointer for a fixture-owned timer (or null) with the game's first
//     thread frozen (re/frida/js/freeze.js) and restore it before the resume. No x87 control-word change is made
//     here; Channel_setRateGlide's double arithmetic is written in double, which is what the x87 code computes under
//     the A/B thread's 53-bit precision control (log/c3/c3cx_purpose.md records the measured control word);
//   - MultiSound flag/forwarding slots over channel functions already at C3 (Channel_stopVoices,
//     Channel_findStreamNode, Channel_broadcastVoiceBit1, Channel_applyFlags);
//   - Channel_allocAndInitVoice, getDeviceCaps, getNodeByIndex, Ctrl5b19c_ctor;
//   - CRT: ostreamResetVtable (virtual-base vtable store) and two routines of the old (V5) small-block heap,
//     sbhResizeBlock and sbhPageFree. The running sound.dll selects the system heap: crtHeapSelect 0x10045bc8 returns
//     1 at 0x10045c03 when GetVersionExA reports platform 2 with major version >= 5, crtHeapInit stores it at
//     0x100b5d20 (0x10045d38), and a probe at the menu read 1 there; heap_alloc 0x10042b0b / free 0x10042c07 only
//     enter a small-block heap when that global is 3 or 2. The region and page structures these two routines take are
//     therefore never live; the fixtures build their own, and the one module global sbhPageFree writes (the counter
//     0x100b5590, 0 at the menu and never moving) is saved and restored around each key.
//   - Wave_Device::open's two initialisers initStreamBuffer (which calls operator new 0x100424d7; the A/B replaces it
//     with re/frida/js/heapstandin.js for the key) and initSoundVoice;
//   - the CRT's __free_lc_time, 43 calls of free 0x10042c07 (replaced by the stand-in's recording deallocator).
// Nothing else allocates or frees; no live sound object, device, list, DirectSound interface, MIDI stream, file or
// lock is touched; no import is called.
// __thiscall is emulated with __fastcall (ecx = this, edx unused).
#include "hooks.h"
#include <stdint.h>

namespace {

template <typename T> T& fat(void* base, int off) {
    return *reinterpret_cast<T*>(reinterpret_cast<char*>(base) + off);
}
template <typename T> T fatc(const void* base, int off) {
    return *reinterpret_cast<const T*>(reinterpret_cast<const char*>(base) + off);
}

// sound.dll has base relocations: every original callee, constant table and module global is addressed through the
// loaded base, never through the 0x10000000-based VA the disassembly prints.
char* c3cx_sound(unsigned rva) {
    static HMODULE m = 0;
    if (!m) m = GetModuleHandleA("sound.dll");
    return reinterpret_cast<char*>(m) + rva;
}
template <typename F> F c3cx_fn(unsigned rva) { return reinterpret_cast<F>(c3cx_sound(rva)); }

// g_soundTimer 0x100b49ec: a pointer to the sound timer, whose first dword is the period the rate functions divide by.
const unsigned* c3cx_timer() { return *reinterpret_cast<const unsigned* const*>(c3cx_sound(0x000b49ec)); }

// Shapes of the original callees, called at their own addresses.
typedef int(__fastcall* Th1_t)(void*, void*, unsigned);                       // thiscall, one stack argument
typedef int(__fastcall* Th3_t)(void*, void*, unsigned, unsigned, unsigned);   // thiscall, three stack arguments
typedef int(__fastcall* Th0_t)(void*, void*);                                 // thiscall, nothing pushed
typedef void(__fastcall* Th1v_t)(void*, void*, unsigned);
typedef void(__cdecl* Cdecl1v_t)(int);
typedef char*(__cdecl* Strncpy_t)(char*, const char*, unsigned);
typedef unsigned(__fastcall* IosReset_t)(void*, void*);

// Interface slots the bodies dispatch through (fixture-owned objects in the A/B).
typedef long(__stdcall* ComSetPos_t)(void*, unsigned, unsigned, unsigned, unsigned);   // [vtbl+0x4c]
typedef long(__stdcall* ComGetCaps_t)(void*, void*);                                   // [vtbl+0x10]
typedef int(__fastcall* SlotA_t)(void*, void*, int);                                   // thiscall, one argument

// =====================================================================================================================
// The 3D position chain
// =====================================================================================================================
//
// The three Channel setters share one body shape: `test byte ptr [ecx + 0x5c], 0x20` / `je` (3D flag of the channel),
// then the Ds3DBuffer pointer at this+0x54 is tested (`test ecx, ecx` / `je`), and the stack argument is handed on
// to the Ds3DBuffer setter through an incremental-link thunk with that pointer in ecx; the callee's eax is returned
// unchanged. Either test failing returns 0xb (`mov eax, 0xb`). All three are __thiscall with one stack argument
// (`ret 4`). The argument is a float the bodies only move as a dword.

int c3cx_set3D(void* self, unsigned v, unsigned callee) {
    if ((fatc<unsigned char>(self, 0x5c) & 0x20) == 0) return 0xb;   // flag test
    void* buf = fatc<void*>(self, 0x54);
    if (!buf) return 0xb;                                            // buffer test
    return c3cx_fn<Th1_t>(callee)(buf, 0, v);
}

// 0x00037b40  Channel_set3DPositionX (VA 0x10037b40)
// Flag test at 0x10037b40 / je 0x10037b5a at 0x10037b44; buffer at 0x10037b46, je at 0x10037b4b; the argument is
// pushed at 0x10037b51 and `call 0x10001f05` at 0x10037b52 is a thunk to Ds3DBuffer::setPositionX 0x10011070.
// 0xb at 0x10037b5a.
typedef int(__fastcall* Ch3D_t)(void*, void*, unsigned);
Ch3D_t Ch3DX_orig;
int __fastcall Ch3DX_re(void* self, void*, unsigned x) { return c3cx_set3D(self, x, 0x00011070); }

// 0x00037b80  Channel_set3DPositionY (VA 0x10037b80)
// The same at 0x10037b80..0x10037b9f; `call 0x1000275c` at 0x10037b92 is a thunk to Ds3DBuffer::setPositionY
// 0x100110c0.
Ch3D_t Ch3DY_orig;
int __fastcall Ch3DY_re(void* self, void*, unsigned y) { return c3cx_set3D(self, y, 0x000110c0); }

// 0x00037bc0  Channel_set3DPositionZ (VA 0x10037bc0)
// The same at 0x10037bc0..0x10037bdf; `call 0x100014e2` at 0x10037bd2 is a thunk to Ds3DBuffer::setPositionZ
// 0x10011110.
Ch3D_t Ch3DZ_orig;
int __fastcall Ch3DZ_re(void* self, void*, unsigned z) { return c3cx_set3D(self, z, 0x00011110); }

// 0x0003a5e0  MultiSound::set3DPositionX (VA 0x1003a5e0)
// Slot 55 (+0xdc) of the MultiSound vtable 0x1005c7e8. `add ecx, 0x70` at 0x1003a5e4 moves to the embedded
// channel, the argument is pushed (0x1003a5e7) and `call 0x10001ed8` (0x1003a5e8, thunk to Channel_set3DPositionX
// 0x10037b40) is made; its eax is returned (`ret 4` at 0x1003a5ed, nothing loaded in between).
Ch3D_t Ms3DX_orig;
int __fastcall Ms3DX_re(void* self, void*, unsigned x) {
    return c3cx_fn<Th1_t>(0x00037b40)(reinterpret_cast<char*>(self) + 0x70, 0, x);
}

// 0x0003a630  MultiSound::set3DPositionY (VA 0x1003a630)
// Slot 56 (+0xe0): the same with `call 0x10001eb0` at 0x1003a638, a thunk to Channel_set3DPositionY 0x10037b80.
Ch3D_t Ms3DY_orig;
int __fastcall Ms3DY_re(void* self, void*, unsigned y) {
    return c3cx_fn<Th1_t>(0x00037b80)(reinterpret_cast<char*>(self) + 0x70, 0, y);
}

// 0x0003a650  MultiSound::set3DPositionZ (VA 0x1003a650)
// Slot 57 (+0xe4): the same with `call 0x10001262` at 0x1003a658, a thunk to Channel_set3DPositionZ 0x10037bc0.
Ch3D_t Ms3DZ_orig;
int __fastcall Ms3DZ_re(void* self, void*, unsigned z) {
    return c3cx_fn<Th1_t>(0x00037bc0)(reinterpret_cast<char*>(self) + 0x70, 0, z);
}

// 0x0003a5b0  MultiSound_set3DPosition (VA 0x1003a5b0)
// Slot 54 (+0xd8). __thiscall with three stack arguments (`ret 0xc` at 0x1003a5c7). They are re-pushed in the same
// order (0x1003a5b0..0x1003a5be), ecx moves to the channel at this+0x70 (0x1003a5bf) and `call 0x10001974`
// (0x1003a5c2, thunk to Channel_set3DPosition 0x10037b00) is made; its eax is returned.
typedef int(__fastcall* Ms3DPos_t)(void*, void*, unsigned, unsigned, unsigned);
Ms3DPos_t Ms3DPos_orig;
int __fastcall Ms3DPos_re(void* self, void*, unsigned x, unsigned y, unsigned z) {
    return c3cx_fn<Th3_t>(0x00037b00)(reinterpret_cast<char*>(self) + 0x70, 0, x, y, z);
}

// 0x0003a600  MultiSound::set3DBuffer (VA 0x1003a600)
// Slot 59 (+0xec). __thiscall with one stack argument (`ret 4` at 0x1003a619). The argument is read at 0x1003a602
// and pushed twice: first for `call 0x10001e7e` (0x1003a609, thunk to 0x1002c270) with ecx still `this`, then for
// `call 0x100025c7` (0x1003a612, thunk to Channel_set3DBufferStub 0x10037c00) with ecx = this+0x70 (0x1003a60f).
// The first call's eax is discarded; the second call's eax is returned.
// 0x1002c270 (no row of its own) stores the argument at this+0x60 (0x1002c274) and forwards it to the object at
// this+0x40 through its vtable slot +0xec (0x1002c281) when that pointer is non-null, else returns 0x14; it is
// called at its own address here.
Ch3D_t Ms3DBuf_orig;
int __fastcall Ms3DBuf_re(void* self, void*, unsigned v) {
    c3cx_fn<Th1_t>(0x0002c270)(self, 0, v);                                          // 0x1003a609
    return c3cx_fn<Th1_t>(0x00037c00)(reinterpret_cast<char*>(self) + 0x70, 0, v);   // 0x1003a612
}

// =====================================================================================================================
// Rate and timer: g_soundTimer 0x100b49ec
// =====================================================================================================================

// 0x0001e630  Seq_scaleTickRate (VA 0x1001e630)
// __thiscall with one stack argument (`ret 4` at 0x1001e65b, 0x1001e66b). g_soundTimer is read at 0x1001e630; a null
// pointer returns at once (`je 0x1001e66a` at 0x1001e638) with eax = 0, the value just loaded.
// Otherwise its first dword (the period) is read (0x1001e63a) and compared with the argument UNSIGNED
// (`cmp eax, esi` / `jbe` at 0x1001e640). Above the period, the quotient argument / period (`div esi` at 0x1001e646,
// edx cleared) divides the dword at this+0x17c (`div esi` at 0x1001e652, edx cleared, unsigned) and the result is
// stored at this+0x174 (0x1001e655). At or below the period, this+0x17c is copied to this+0x174 (0x1001e65e,
// 0x1001e664). In both cases eax is the stored value.
Ch3D_t SeqScaleTick_orig;
int __fastcall SeqScaleTick_re(void* self, void*, unsigned arg) {
    const unsigned* timer = c3cx_timer();                      // 0x1001e630
    if (!timer) return 0;                                      // je 0x1001e66a at 0x1001e638
    const unsigned period = timer[0];                          // 0x1001e63a
    unsigned v = fatc<unsigned>(self, 0x17c);
    if (arg > period) {                                        // jbe at 0x1001e642
        const unsigned q = arg / period;                       // 0x1001e646
        v = v / q;                                             // 0x1001e652
    }
    fat<unsigned>(self, 0x174) = v;                            // 0x1001e655 / 0x1001e664
    return static_cast<int>(v);
}

// 0x00005d80  Sound::scaleTickRate (VA 0x10005d80)
// Slot 1 (+0x4) of the Sound vtable 0x1005b000. __thiscall with one stack argument (`ret 4` at 0x10005d8f): the
// argument is pushed (0x10005d87) with ecx = this+0x60 (0x10005d84) for `call 0x1000269e` (0x10005d88), a thunk to
// Seq_scaleTickRate 0x1001e630; then `xor eax, eax` (0x10005d8d) returns 0 whatever the callee did.
Ch3D_t SoundScaleTick_orig;
int __fastcall SoundScaleTick_re(void* self, void*, unsigned arg) {
    c3cx_fn<Th1_t>(0x0001e630)(reinterpret_cast<char*>(self) + 0x60, 0, arg);
    return 0;
}

// 0x00034c80  Channel_setRateGlide (VA 0x10034c80)
// __thiscall with three stack arguments, start rate, end rate and duration (`ret 0xc` at 0x10034d29, 0x10034d54,
// 0x10034d60).
//   - The end rate is tested first: -1 is replaced by the current rate this+0x1d8 (0x10034c89..0x10034c94), then
//     the start rate the same way (0x10034c9c..0x10034ca7).
//   - start * scale (fild dword, fmul qword 0x1005c5e8, the double 1/65536) is stored at this+0x1d0 with `fst`
//     (0x10034cb7) and kept on the x87 stack.
//   - The end rate is tested against 0 (`test edi, edi` at 0x10034caf, `je 0x10034ccb` at 0x10034cbd): non-zero
//     gives end * scale (0x10034cbf, 0x10034cc3), zero loads the double at 0x1005b498 (0x10034ccb); the value is
//     stored at this+0x1e0 with `fst` (0x10034cd1).
//   - g_soundTimer is read (0x10034cd7); null pops both values and returns (`je 0x10034d57` at 0x10034cde) with eax
//     = 0, the value just loaded.
//   - The duration is compared with the timer period UNSIGNED (`cmp eax, esi` / `jbe 0x10034d2c` at 0x10034ce7):
//       above: n = duration / period (`div esi` at 0x10034cf5, edx cleared); `fsub st(1)` (d8 e1, st0 = st0 - st1)
//         at 0x10034cf7 leaves end - start in doubles; n is loaded as a 64-bit integer whose high dword is 0
//         (0x10034ced, 0x10034d01, `fild qword` at 0x10034d05); `fdivp st(1)` (de f9, st1 = st1 / st0) at 0x10034d0b
//         gives (end - start) / n, stored at this+0x1c8 (0x10034d10). The integer (end - start) is divided by n
//         UNSIGNED (`div esi` at 0x10034d09 with edx cleared at 0x10034cf9) into this+0x1c4 (0x10034d16).
//       at or below: this+0x1c4 = end - start (0x10034d2f, 0x10034d39) and this+0x1c8 = that integer as a double
//         (`fild dword` at 0x10034d3f, `fstp qword` at 0x10034d4a); the scaled values are discarded.
//     Both arms set bit 4 of this+0x5c (`or al, 0x10` at 0x10034d1f / 0x10034d43) and return eax = the new +0x5c.
// Under the A/B thread's precision control (53-bit) every x87 result above is rounded to double, which is what the
// double expressions below compute; the products by 1/65536 and their difference are exact in double.
Ms3DPos_t SetRateGlide_orig;
int __fastcall SetRateGlide_re(void* self, void*, unsigned startArg, unsigned endArg, unsigned duration) {
    int end = static_cast<int>(endArg);
    if (end == -1) end = fatc<int>(self, 0x1d8);                         // 0x10034c89..0x10034c94
    int start = static_cast<int>(startArg);
    if (start == -1) start = fatc<int>(self, 0x1d8);                     // 0x10034c9c..0x10034ca7
    const double scale = *reinterpret_cast<const double*>(c3cx_sound(0x0005c5e8));
    const double s = static_cast<double>(start) * scale;                 // 0x10034cab, 0x10034cb1
    fat<double>(self, 0x1d0) = s;                                        // 0x10034cb7
    const double e = (end != 0) ? static_cast<double>(end) * scale       // 0x10034cbf, 0x10034cc3
                                : *reinterpret_cast<const double*>(c3cx_sound(0x0005b498));   // 0x10034ccb
    fat<double>(self, 0x1e0) = e;                                        // 0x10034cd1
    const unsigned* timer = c3cx_timer();                                // 0x10034cd7
    if (!timer) return 0;                                                // je 0x10034d57 at 0x10034cde
    const unsigned period = timer[0];                                    // 0x10034ce1
    if (duration > period) {                                             // jbe at 0x10034ce9
        const unsigned n = duration / period;                            // 0x10034cf5
        const double diff = e - s;                                       // 0x10034cf7
        fat<double>(self, 0x1c8) = diff / static_cast<double>(n);        // 0x10034d05..0x10034d10
        fat<unsigned>(self, 0x1c4) = (static_cast<unsigned>(end) - static_cast<unsigned>(start)) / n;   // 0x10034cfd..0x10034d16
    } else {
        const int d = static_cast<int>(static_cast<unsigned>(end) - static_cast<unsigned>(start));   // 0x10034d2f (wraps)
        fat<int>(self, 0x1c4) = d;                                       // 0x10034d39
        fat<double>(self, 0x1c8) = static_cast<double>(d);               // 0x10034d3f, 0x10034d4a
    }
    const unsigned f = fatc<unsigned>(self, 0x5c) | 0x10u;               // 0x10034d1c / 0x10034d2c, or al, 0x10
    fat<unsigned>(self, 0x5c) = f;
    return static_cast<int>(f);
}

// 0x0003a140  MultiSound::setRateGlide (VA 0x1003a140)
// Slot 13 (+0x34) of the MultiSound vtable 0x1005c7e8. __thiscall with three stack arguments (`ret 0xc` at
// 0x1003a197). The first two are MIDI-style values converted the same way: 0 and -1 are passed unchanged (`je` at
// 0x1003a146 / 0x1003a14b and 0x1003a169 / 0x1003a16e); any other value is clamped SIGNED to 0..0x7f (`jle` at
// 0x1003a150 / 0x1003a173 against 0x7f, `jge` at 0x1003a15b / 0x1003a17e against 0) and becomes (v + 1) << 9
// (0x1003a15f, 0x1003a160 / 0x1003a182, 0x1003a183). The third is passed unchanged. They are pushed in the original
// order (0x1003a18b..0x1003a18d) for `call 0x1000211c` (0x1003a191, thunk to Channel_setRateGlide 0x10034c80) on the
// channel at this+0x70, whose eax is returned.
unsigned c3cx_midiRate(unsigned v) {
    int x = static_cast<int>(v);
    if (x == 0 || x == -1) return v;
    if (x > 0x7f) x = 0x7f;
    else if (x < 0) x = 0;
    return static_cast<unsigned>(x + 1) << 9;
}
Ms3DPos_t MsSetRateGlide_orig;
int __fastcall MsSetRateGlide_re(void* self, void*, unsigned a, unsigned b, unsigned duration) {
    const unsigned ra = c3cx_midiRate(a);                // 0x1003a140..0x1003a160
    const unsigned rb = c3cx_midiRate(b);                // 0x1003a163..0x1003a183
    return c3cx_fn<Th3_t>(0x00034c80)(reinterpret_cast<char*>(self) + 0x70, 0, ra, rb, duration);
}

// 0x0003a6e0  MultiSound::setReverseRate (VA 0x1003a6e0)
// Slot 21 (+0x54). __thiscall with one stack argument (`ret 4` at 0x1003a6ef): the argument is pushed (0x1003a6e7)
// for `call 0x1000101e` (0x1003a6e8, thunk to Channel_setReverseRate 0x100355d0) on the channel at this+0x70
// (0x1003a6e4), then `xor eax, eax` (0x1003a6ed) returns 0.
Ch3D_t MsSetReverseRate_orig;
int __fastcall MsSetReverseRate_re(void* self, void*, unsigned rate) {
    c3cx_fn<Th1_t>(0x000355d0)(reinterpret_cast<char*>(self) + 0x70, 0, rate);
    return 0;
}

// =====================================================================================================================
// MultiSound flag and forwarding slots (vtable 0x1005c7e8; the channel is embedded at this+0x70, so the channel's
// +0x58 / +0x5c flag words are this+0xc8 / +0xcc)
// =====================================================================================================================

// 0x0003a800  MultiSound_is3DEnabled (VA 0x1003a800)
// Slot 58 (+0xe8). __thiscall, nothing pushed (`ret` at 0x1003a817, 0x1003a81a), no callee. Returns 1 only when bit 5
// of the byte at this+0xcc is set (`test byte ptr [ecx + 0xcc], 0x20` / `je` at 0x1003a800, 0x1003a807) AND bit 1 of
// the byte at this+0xc8 is set (`test byte ptr [ecx + 0xc8], 2` / `je` at 0x1003a809, 0x1003a810); else 0
// (0x1003a818).
typedef int(__fastcall* Th0r_t)(void*, void*);
Th0r_t MsIs3D_orig;
int __fastcall MsIs3D_re(void* self, void*) {
    if ((fatc<unsigned char>(self, 0xcc) & 0x20) == 0) return 0;
    if ((fatc<unsigned char>(self, 0xc8) & 0x02) == 0) return 0;
    return 1;
}

// 0x00039c80  MultiSound_stop (VA 0x10039c80)
// Slot 12 (+0x30). `add ecx, 0x70` (0x10039c80) and a tail `jmp 0x100024d7` (0x10039c83), a thunk to
// Channel_stopVoices 0x10034b90: the channel's result is the result.
Th0r_t MsStop_orig;
int __fastcall MsStop_re(void* self, void*) {
    return c3cx_fn<Th0_t>(0x00034b90)(reinterpret_cast<char*>(self) + 0x70, 0);
}

// 0x0003a730  MultiSound_findStreamNode (VA 0x1003a730)
// Slot 47 (+0xbc). `add ecx, 0x70` (0x1003a730) and a tail `jmp 0x100020b8` (0x1003a733), a thunk to
// Channel_findStreamNode 0x10035770.
Th0r_t MsFindStream_orig;
int __fastcall MsFindStream_re(void* self, void*) {
    return c3cx_fn<Th0_t>(0x00035770)(reinterpret_cast<char*>(self) + 0x70, 0);
}

// 0x0003a080  MultiSound_setVoiceBit1On (VA 0x1003a080)
// Slot 14 (+0x38). __thiscall, nothing pushed (`ret` at 0x1003a08e, 0x1003a09b). When bit 1 of the byte at this+0xc8
// is clear (`test byte ptr [ecx + 0xc8], 2` / `jne` at 0x1003a080, 0x1003a087) it returns 0x14 (0x1003a089);
// otherwise it pushes 1 (0x1003a08f) and calls `0x10002117` (0x1003a094, thunk to Channel_broadcastVoiceBit1
// 0x100377f0) on the channel at this+0x70 (0x1003a091), then returns 0 (0x1003a099).
Th0r_t MsVoiceBit1_orig;
int __fastcall MsVoiceBit1_re(void* self, void*) {
    if ((fatc<unsigned char>(self, 0xc8) & 0x02) == 0) return 0x14;
    c3cx_fn<Th1_t>(0x000377f0)(reinterpret_cast<char*>(self) + 0x70, 0, 1);
    return 0;
}

// 0x0003a370  MultiSound::applyFlags (VA 0x1003a370)
// Slot 27 (+0x6c), overriding the channel's own slot 27 Channel_applyFlags 0x1002c040, which it calls first with the
// same `this` and the flag word (`push ebx` / `call 0x1000198d` at 0x1003a378, 0x1003a379; the thunk jumps to
// 0x1002c040). Then, each on its own bit of the argument and in this order:
//   bit 0      (0x1003a37e) `call 0x10001e4c` on this+0x70 (0x1003a383, 0x1003a386), a thunk to flushMmioBuffer
//              0x10037920;
//   bit 4      (0x1003a38b) this+0xc8 |= 0x20000000 (0x1003a390);
//   bit 5      (0x1003a39a) this+0xc8 |= 0x80000000 (0x1003a39f);
//   bit 6      (0x1003a3a9) this+0xcc |= 0x20 (0x1003a3ae..0x1003a3b6);
//   bit 8      (`test bh, 1` at 0x1003a3bc) this+0xcc |= 0x40 (0x1003a3c1..0x1003a3c9);
//   bit 9      (`test bh, 2` at 0x1003a3cf) this+0xcc |= 0x80 (0x1003a3d4..0x1003a3dc);
//   bit 1      (0x1003a3e2) this' own vtable slot +0x48 with 1 pushed (0x1003a3e7..0x1003a3ed);
//   bit 10     (`test bh, 4` at 0x1003a3f0) this+0xcc |= 0x200 (`or ah, 2` at 0x1003a3fb).
// Returns 0 (0x1003a405). Each `or` re-reads the field, so a slot +0x48 that changes +0xcc is seen by bit 10.
Ch3D_t MsApplyFlags_orig;
int __fastcall MsApplyFlags_re(void* self, void*, unsigned flags) {
    c3cx_fn<Th1_t>(0x0002c040)(self, 0, flags);                                   // 0x1003a379
    if (flags & 0x001) c3cx_fn<Th0_t>(0x00037920)(reinterpret_cast<char*>(self) + 0x70, 0);   // 0x1003a386
    if (flags & 0x010) fat<unsigned>(self, 0xc8) |= 0x20000000u;                  // 0x1003a390
    if (flags & 0x020) fat<unsigned>(self, 0xc8) |= 0x80000000u;                  // 0x1003a39f
    if (flags & 0x040) fat<unsigned>(self, 0xcc) |= 0x20u;                        // 0x1003a3b4
    if (flags & 0x100) fat<unsigned>(self, 0xcc) |= 0x40u;                        // 0x1003a3c7
    if (flags & 0x200) fat<unsigned>(self, 0xcc) |= 0x80u;                        // 0x1003a3da
    if (flags & 0x002) {                                                          // 0x1003a3e2
        void** vt = fatc<void**>(self, 0);
        reinterpret_cast<SlotA_t>(vt[0x48 / 4])(self, 0, 1);                      // 0x1003a3ed
    }
    if (flags & 0x400) fat<unsigned>(self, 0xcc) |= 0x200u;                       // 0x1003a3fb
    return 0;
}

// =====================================================================================================================
// Three channel vtable slots with no hooks.csv row yet (names proposed in log/c3/c3cx_names.tsv) and the MultiSound
// slots that override them and call them first: slot 16 (+0x40), slot 28 (+0x70) and slot 39 (+0x9c) of the channel
// vtable 0x1005bb90 (re/tools/pointer_callers.py: 0x1005bbd0, 0x1005bc00, 0x1005bc2c; also slots of 0x1005bcb4)
// =====================================================================================================================

// 0x0002c370  Channel_setField4Masked7f (VA 0x1002c370)
// __thiscall with one stack argument (`ret 4` at 0x1002c37a), no callee: the argument masked with 0x7f (0x1002c374)
// is stored at this+4 (0x1002c377) and left in eax.
Ch3D_t ChSetField4_orig;
int __fastcall ChSetField4_re(void* self, void*, unsigned v) {
    const unsigned m = v & 0x7fu;          // 0x1002c374
    fat<unsigned>(self, 4) = m;            // 0x1002c377
    return static_cast<int>(m);
}

// 0x0002c0d0  Channel_collectFlagBits (VA 0x1002c0d0)
// __thiscall, nothing pushed (`ret` at 0x1002c119). eax starts at 0 (0x1002c0d3); when the object at this+0x40 is
// non-null (`je` at 0x1002c0da) eax is that object's own vtable slot +0x70 result (0x1002c0de, the same slot 28).
// Then, OR-ing into the LOW BYTE (and once the second byte) of that value: bit 1 when the dword at this+0x30 is
// non-null (0x1002c0e4..0x1002c0e8), and from the low byte of this+0x58: bit 0 -> bit 0 (0x1002c0ee..0x1002c0f3),
// bit 3 -> bit 6 (0x1002c0f5..0x1002c0fa), bit 1 -> bit 2 (0x1002c0fc..0x1002c101), bit 2 -> bit 4
// (0x1002c103..0x1002c108), bit 4 -> bit 7 (0x1002c10a..0x1002c10f), bit 5 -> bit 8 (`or ah, 1` at 0x1002c116).
// No callee other than that slot.
Th0r_t ChFlagBits_orig;
int __fastcall ChFlagBits_re(void* self, void*) {
    unsigned r = 0;                                                       // 0x1002c0d3
    void* dev = fatc<void*>(self, 0x40);                                  // 0x1002c0d5
    if (dev) {
        void** vt = *reinterpret_cast<void***>(dev);
        r = static_cast<unsigned>(reinterpret_cast<Th0_t>(vt[0x70 / 4])(dev, 0));   // 0x1002c0de
    }
    if (fatc<unsigned>(self, 0x30) != 0) r |= 0x02;                       // 0x1002c0e8
    const unsigned f = fatc<unsigned char>(self, 0x58);                   // 0x1002c0ea
    if (f & 0x01) r |= 0x01;                                              // 0x1002c0f3
    if (f & 0x08) r |= 0x40;                                              // 0x1002c0fa
    if (f & 0x02) r |= 0x04;                                              // 0x1002c101
    if (f & 0x04) r |= 0x10;                                              // 0x1002c108
    if (f & 0x10) r |= 0x80;                                              // 0x1002c10f
    if (f & 0x20) r |= 0x100;                                             // 0x1002c116
    return static_cast<int>(r);
}

// 0x0002bff0  Channel_setField5cClamped1200 (VA 0x1002bff0)
// __thiscall with one stack argument (`ret 4` at 0x1002c023). The argument is clamped SIGNED to -1200..1200
// (`cmp eax, 0xfffffb50` / `jge` at 0x1002bff4, 0x1002bff9; `cmp eax, 0x4b0` / `jle` at 0x1002c002, 0x1002c007),
// stored at this+0x5c (0x1002c00e) and, when the object at this+0x40 is non-null (`je` at 0x1002c016), passed to
// that object's own vtable slot +0x9c (0x1002c01b, the same slot 39), whose result is discarded. Returns 0
// (0x1002c021).
Ch3D_t ChSetField5c_orig;
int __fastcall ChSetField5c_re(void* self, void*, unsigned arg) {
    int v = static_cast<int>(arg);
    if (v < -1200) v = -1200;                                             // 0x1002bff9, 0x1002bffb
    else if (v > 1200) v = 1200;                                          // 0x1002c007, 0x1002c009
    fat<int>(self, 0x5c) = v;                                             // 0x1002c00e
    void* dev = fatc<void*>(self, 0x40);
    if (dev) {
        void** vt = *reinterpret_cast<void***>(dev);
        reinterpret_cast<SlotA_t>(vt[0x9c / 4])(dev, 0, v);               // 0x1002c01b
    }
    return 0;
}

// 0x0003a560  MultiSound_getStatus (VA 0x1003a560)
// Slot 28 (+0x70) of the MultiSound vtable 0x1005c7e8. __thiscall, nothing pushed (`ret` at 0x1003a575).
// `call 0x1000245a` (0x1003a563, thunk to Channel_collectFlagBits 0x1002c0d0) with the same `this`; then bit 3 is
// OR-ed into the result's low byte when the dword at this+0x228 is non-null (`test ecx, ecx` / `je` at 0x1003a56f,
// 0x1003a571, `or al, 8` at 0x1003a573).
Th0r_t MsGetStatus_orig;
int __fastcall MsGetStatus_re(void* self, void*) {
    unsigned r = static_cast<unsigned>(c3cx_fn<Th0_t>(0x0002c0d0)(self, 0));   // 0x1003a563
    if (fatc<unsigned>(self, 0x228) != 0) r |= 8;                         // 0x1003a573
    return static_cast<int>(r);
}

// 0x0003a0d0  MultiSound::setVolume (VA 0x1003a0d0)
// Slot 16 (+0x40). __thiscall with one stack argument (`ret 4` at 0x1003a11c).
//   - The argument is clamped SIGNED to 0..0x7f (`jle` at 0x1003a0db against 0x7f, `jge` at 0x1003a0e6 against 0).
//   - `call 0x100026ee` (0x1003a0ed, thunk to Channel_setField4Masked7f 0x1002c370) with the same `this`.
//   - this' own vtable slot +0xcc (0x1003a0f6); when it returns non-zero (`je` at 0x1003a0fe), `call 0x100015aa`
//     (0x1003a104, thunk to Channel_setVolume 0x10035150) on the channel at this+0x70 with the clamped value.
//   - The clamped value v becomes (v + 1) << 9, or 0 when v is 0 (`je` at 0x1003a10b skips the `inc`;
//     `shl esi, 9` at 0x1003a10e), and is passed to `call 0x100024f0` (0x1003a115, thunk to Channel_setRate
//     0x100351c0) on the channel; that call's eax is the result.
Ch3D_t MsSetVolume_orig;
int __fastcall MsSetVolume_re(void* self, void*, unsigned arg) {
    int v = static_cast<int>(arg);
    if (v > 0x7f) v = 0x7f;                                               // 0x1003a0db, 0x1003a0dd
    else if (v < 0) v = 0;                                                // 0x1003a0e6, 0x1003a0e8
    c3cx_fn<Th1_t>(0x0002c370)(self, 0, static_cast<unsigned>(v));        // 0x1003a0ed
    void** vt = fatc<void**>(self, 0);
    if (reinterpret_cast<Th0_t>(vt[0xcc / 4])(self, 0) != 0)              // 0x1003a0f6, 0x1003a0fe
        c3cx_fn<Th1_t>(0x00035150)(reinterpret_cast<char*>(self) + 0x70, 0, static_cast<unsigned>(v));   // 0x1003a104
    const unsigned rate = (v != 0) ? static_cast<unsigned>(v + 1) << 9 : 0u;   // 0x1003a109..0x1003a10e
    return c3cx_fn<Th1_t>(0x000351c0)(reinterpret_cast<char*>(self) + 0x70, 0, rate);   // 0x1003a115
}

// 0x0003a210  MultiSound_setPitchCents (VA 0x1003a210)
// Slot 39 (+0x9c). __thiscall with one stack argument, a signed cent offset (`ret 4` at 0x1003a313); returns 0
// (0x1003a30d).
//   - Outside -1200..1200 (`jl` at 0x1003a223, `jle` at 0x1003a22a) the argument is replaced by its remainder
//     modulo 1200 (`idiv` by 0x4b0 at 0x1003a232, signed, the remainder keeping the dividend's sign).
//   - `call 0x10001924` (0x1003a23c, thunk to Channel_setField5cClamped1200 0x1002bff0) with that value and the same
//     `this`.
//   - q = value / 100 and r = |value % 100| (`idiv` by 0x64 at 0x1003a24d, `neg` at 0x1003a254); s = q + 12
//     (0x1003a266).
//   - this' own vtable slot +0xcc (0x1003a277): non-zero (`je` at 0x1003a286 not taken) takes the base integer from
//     the table at 0x1005c66c and the base double 0.0 (the local zeroed at 0x1003a269 / 0x1003a270, loaded at
//     0x1003a28e); zero takes the base integer from the table at 0x1005c608 and the base double from the table at
//     0x1005c6d0 (0x1003a293, 0x1003a299). Both indexed by s.
//   - n = s - 1 for a negative value, else s + 1 (`jl` at 0x1003a2a8 on the value).
//   - The double ((D[n] - D[s]) * r) * 0.01 + base (fld/fsub/fimul at 0x1003a2ad..0x1003a2bb, fmul qword 0x1005c910
//     at 0x1003a2d0, `fadd st(1)` at 0x1003a2d6) is stored in a local (0x1003a2d8) and finally copied to
//     this+0x13f8 / this+0x13fc (0x1003a2f9..0x1003a305).
//   - The integer delta T1[n] - T1[s] (both from 0x1005c608: 0x1003a2be..0x1003a2cb) times r times the float at
//     0x1005c90c (fild/fimul/fmul at 0x1003a2dd..0x1003a2e3) is converted with __ftol 0x10042550 (0x1003a2e9) and
//     subtracted from the base integer (0x1003a2ee); the result goes to `call 0x100016f9` (0x1003a2f4, thunk to
//     Channel_setPitchRate 0x10035750) on the channel at this+0x70.
// The tables are 25 entries each (indices 0..24) and n reaches -1 for value -1200 and 25 for value 1200; the
// original then reads the dword / qword just outside the table, and so does this body (same addresses).
// x87 under the A/B thread's 53-bit precision control: each step is written in double in the original order; __ftol
// truncates and keeps the low dword of the 64-bit result, written (int)(long long).
Ch3D_t MsSetPitch_orig;
int __fastcall MsSetPitch_re(void* self, void*, unsigned arg) {
    int c = static_cast<int>(arg);
    if (c < -1200 || c > 1200) c = c % 1200;                              // 0x1003a223..0x1003a237
    c3cx_fn<Th1_t>(0x0002bff0)(self, 0, static_cast<unsigned>(c));        // 0x1003a23c
    const int q = c / 100;                                                // 0x1003a24d
    int r = c % 100;
    if (r < 0) r = -r;                                                    // 0x1003a254
    const int s = q + 12;                                                 // 0x1003a266
    const char* T1 = c3cx_sound(0x0005c608);
    const char* T2 = c3cx_sound(0x0005c66c);
    const char* D = c3cx_sound(0x0005c6d0);
    void** vt = fatc<void**>(self, 0);
    int baseI;
    double base;
    if (reinterpret_cast<Th0_t>(vt[0xcc / 4])(self, 0) != 0) {            // 0x1003a277, 0x1003a286
        baseI = *reinterpret_cast<const int*>(T2 + s * 4);                // 0x1003a288
        base = 0.0;                                                       // 0x1003a28e
    } else {
        baseI = *reinterpret_cast<const int*>(T1 + s * 4);                // 0x1003a293
        base = *reinterpret_cast<const double*>(D + s * 8);               // 0x1003a299
    }
    const int n = (c < 0) ? s - 1 : s + 1;                                // 0x1003a2a3..0x1003a2aa
    double t = *reinterpret_cast<const double*>(D + n * 8) - *reinterpret_cast<const double*>(D + s * 8);   // 0x1003a2b4
    t = t * static_cast<double>(r);                                       // 0x1003a2bb
    t = t * *reinterpret_cast<const double*>(c3cx_sound(0x0005c910));     // 0x1003a2d0
    const double val = t + base;                                          // 0x1003a2d6
    const int delta = static_cast<int>(static_cast<unsigned>(*reinterpret_cast<const int*>(T1 + n * 4)) -
                                       static_cast<unsigned>(*reinterpret_cast<const int*>(T1 + s * 4)));   // 0x1003a2cb
    double u = static_cast<double>(delta) * static_cast<double>(r);       // 0x1003a2dd, 0x1003a2e0
    u = u * static_cast<double>(*reinterpret_cast<const float*>(c3cx_sound(0x0005c90c)));   // 0x1003a2e3
    const int k = static_cast<int>(static_cast<long long>(u));            // __ftol at 0x1003a2e9
    c3cx_fn<Th1_t>(0x00035750)(reinterpret_cast<char*>(self) + 0x70, 0,
                               static_cast<unsigned>(baseI) - static_cast<unsigned>(k));   // 0x1003a2ee, 0x1003a2f4
    fat<double>(self, 0x13f8) = val;                                      // 0x1003a2f9..0x1003a305
    return 0;
}

// =====================================================================================================================
// Channel voice slots
// =====================================================================================================================

// 0x00034770  Channel_allocAndInitVoice (VA 0x10034770)
// __thiscall with one stack argument, a pointer to the slot index it reports (`ret 4` at 0x100347c0, 0x10034884,
// 0x1003488f); no callee. The index is set to -1 first (0x10034777).
//   - Returns 0x21 when bit 0 of this+0x58 is clear (`test al, 1` / `je` at 0x10034780, 0x10034782) or bit 28 is set
//     (0x10034788, 0x1003478d).
//   - Scans the sixteen busy bits 12..27 of this+0x58 for the first clear one, re-reading the field each time
//     (0x1003479c..0x100347b1, the mask doubled at 0x100347ab); none clear returns 6 (0x100347b3..0x100347ba).
//   - Slot i is the 0x110-byte block at this + i*0x110 (`shl eax, 4` / `add eax, esi` / `shl eax, 4` at
//     0x100347c6..0x100347d1). In it: +0x254 = +0x250 (0x100347d9, 0x100347e6), the byte +0x2d4 &= 0xf1
//     (0x100347df), +0x328 = +0x330 (0x100347ec, 0x100347f2), +0x2c0 = +0x2c4 = +0x2f8 = 0 (0x100347fe..0x1003480a),
//     +0x32c = +0x334 (0x100347f8, 0x10034810); the doubles this+0x1f0, this+0x200, this+0x1388 and this+0x1e0 are
//     copied (fld/fstp qword) to +0x2b8, +0x2a8, +0x340 and +0x290 (0x10034816..0x10034840); +0x2fc = this+0x22c
//     (0x10034846, 0x1003484c); +0x2f8 = 0 again (0x10034852); +0x300 = this+0x230, an address (0x10034858,
//     0x1003485f); the byte +0x2d4 &= 0xfb (0x10034865); the byte +0x2cc |= 1 (0x1003486c).
//   - this+0x58 = (mask << 12) | (this+0x58 with bit 4 cleared) (`and al, 0xef` / `or edx, eax` at 0x10034876,
//     0x10034879, stored at 0x1003487d), where mask is the single bit of slot i; the index is stored (0x10034880)
//     and 0 returned (0x1003487b).
// The double copies go through the x87 stack; for every value the fixtures hold (no signalling NaN) fld/fstp of a
// qword is a bit-exact copy, as is the plain double copy here.
typedef int(__fastcall* AllocVoice_t)(void*, void*, int*);
AllocVoice_t AllocVoice_orig;
int __fastcall AllocVoice_re(void* self, void*, int* outIndex) {
    *outIndex = -1;                                                       // 0x10034777
    const unsigned f = fatc<unsigned>(self, 0x58);
    if ((f & 1u) == 0 || (f & 0x10000000u) != 0) return 0x21;            // 0x10034782, 0x1003478d -> 0x10034887
    unsigned mask = 1;
    int i = 0;
    for (; i < 16; ++i, mask <<= 1) {                                     // 0x1003479c..0x100347b1
        if (((fatc<unsigned>(self, 0x58) >> 12) & mask & 0xffffu) == 0) break;
    }
    if (i == 16) return 6;                                                // 0x100347b3..0x100347ba
    char* v = reinterpret_cast<char*>(self) + i * 0x110;
    fat<unsigned>(v, 0x254) = fatc<unsigned>(v, 0x250);                   // 0x100347e6
    fat<unsigned char>(v, 0x2d4) &= 0xf1;                                 // 0x100347df
    fat<unsigned>(v, 0x328) = fatc<unsigned>(v, 0x330);                   // 0x100347f2
    fat<unsigned>(v, 0x2c0) = 0;                                          // 0x100347fe
    fat<unsigned>(v, 0x2c4) = 0;                                          // 0x10034804
    fat<unsigned>(v, 0x2f8) = 0;                                          // 0x1003480a
    fat<unsigned>(v, 0x32c) = fatc<unsigned>(v, 0x334);                   // 0x10034810
    fat<double>(v, 0x2b8) = fatc<double>(self, 0x1f0);                    // 0x10034816, 0x1003481c
    fat<double>(v, 0x2a8) = fatc<double>(self, 0x200);                    // 0x10034822, 0x10034828
    fat<double>(v, 0x340) = fatc<double>(self, 0x1388);                   // 0x1003482e, 0x10034834
    fat<double>(v, 0x290) = fatc<double>(self, 0x1e0);                    // 0x1003483a, 0x10034840
    fat<unsigned>(v, 0x2fc) = fatc<unsigned>(self, 0x22c);                // 0x1003484c
    fat<unsigned>(v, 0x2f8) = 0;                                          // 0x10034852
    fat<char*>(v, 0x300) = reinterpret_cast<char*>(self) + 0x230;         // 0x1003485f
    fat<unsigned char>(v, 0x2d4) &= 0xfb;                                 // 0x10034865
    fat<unsigned char>(v, 0x2cc) |= 1;                                    // 0x1003486c
    fat<unsigned>(self, 0x58) = ((mask & 0xffffu) << 12) | (fatc<unsigned>(self, 0x58) & ~0x10u);   // 0x1003487d
    *outIndex = i;                                                        // 0x10034880
    return 0;
}

// =====================================================================================================================
// DirectSound device helpers (DirectSoundDevice object; the interface pointer at +0 is fixture-owned in the A/B)
// =====================================================================================================================

// 0x00011760  getDeviceCaps (VA 0x10011760)
// __thiscall, nothing pushed (`ret` at 0x10011777, 0x1001177b). The interface pointer at this+0 is tested
// (0x10011765, `je 0x10011778` at 0x10011767): null returns 0 (0x10011778). Otherwise its vtable slot +0x10 is
// called COM-style with the interface and this+0xc pushed (0x1001176b..0x10011770; the slot's result is not
// read), and the dword at this+0x54 is returned, read AFTER the call (0x10011773), so it is whatever the slot left
// in the caps block that starts at this+0xc.
Th0r_t GetDeviceCaps_orig;
int __fastcall GetDeviceCaps_re(void* self, void*) {
    void* com = fatc<void*>(self, 0);
    if (!com) return 0;                                                   // 0x10011767
    void** vt = *reinterpret_cast<void***>(com);
    reinterpret_cast<ComGetCaps_t>(vt[0x10 / 4])(com, reinterpret_cast<char*>(self) + 0xc);   // 0x10011770
    return fatc<int>(self, 0x54);                                         // 0x10011773
}

// 0x00011790  getNodeByIndex (VA 0x10011790)
// __thiscall with three stack arguments (`ret 0xc` at every exit): an index, a destination buffer and a count.
//   - The index is compared UNSIGNED with the node count at this+0x148 (`cmp edx, eax` / `jb` at 0x1001179a,
//     0x1001179c); at or above it returns 0xa (0x1001179e).
//   - The list head at this+4 is tested (0x100117a9): null returns 3 (0x100117ad).
//   - The walk follows the link at node+0x10 index times (0x100117b5..0x100117c5; `jbe` at 0x100117b9 skips it for
//     index 0), returning 3 when it meets a null node before the end (`je 0x100117cb` at 0x100117bd) or ends on one
//     (`jne` at 0x100117c9).
//   - The node's first dword (a string) is copied with `call 0x10042710` (0x100117e0, _strncpy: destination, the
//     string, the count, pushed at 0x100117dd..0x100117df; cdecl, `add esp, 0xc`) and 0 is returned (0x100117e8).
typedef int(__fastcall* NodeByIndex_t)(void*, void*, unsigned, char*, unsigned);
NodeByIndex_t NodeByIndex_orig;
int __fastcall NodeByIndex_re(void* self, void*, unsigned index, char* dst, unsigned count) {
    if (index >= fatc<unsigned>(self, 0x148)) return 0xa;                 // 0x1001179c
    void* node = fatc<void*>(self, 4);
    if (!node) return 3;                                                  // 0x100117ab
    for (unsigned i = 0; i < index; ++i) {                                // 0x100117b5..0x100117c5
        if (!node) return 3;                                              // 0x100117bd
        node = fatc<void*>(node, 0x10);                                   // 0x100117bf
    }
    if (!node) return 3;                                                  // 0x100117c9
    c3cx_fn<Strncpy_t>(0x00042710)(dst, fatc<const char*>(node, 0), count);   // 0x100117e0
    return 0;
}

// =====================================================================================================================
// Wave_Device::open's two initialisers
// =====================================================================================================================

// 0x00006e00  initStreamBuffer (VA 0x10006e00)
// __thiscall with nothing pushed (`ret` at 0x10006e83), called by Wave_Device::open 0x1000b420; returns this
// (0x10006e7f). +0 = +4 = 0 (0x10006e0b, 0x10006e0d), +0x10 = 2 (0x10006e10), +0x14 = 0, +0xc = 0x4000 (0x10006e1a);
// `call 0x100424d7` (0x10006e21, operator new, cdecl, 0x20000 pushed at 0x10006e06) and +4 = the block
// (0x10006e26); bit 0 of +8 cleared (`and al, 0xfe` at 0x10006e2c); +0x18 = +0x30 = +0x38 = 0 (0x10006e38..0x10006e3e);
// the 18 bytes at +0x1c cleared (0x10006e41..0x10006e4f) and then filled: word +0x1c = 1, word +0x1e = 2, +0x20 =
// 0x5622, +0x24 = 0x15888, word +0x28 = 4, word +0x2a = 0x10, word +0x2c = 0 (0x10006e53..0x10006e79), the field
// order and values of a WAVEFORMATEX for 2-channel 16-bit PCM at 22050 Hz; +0x34 = 0 (0x10006e5c).
typedef void*(__fastcall* Ctor0_t)(void*, void*);
typedef void*(__cdecl* OperatorNew_t)(unsigned);
Ctor0_t InitStreamBuffer_orig;
void* __fastcall InitStreamBuffer_re(void* self, void*) {
    char* p = reinterpret_cast<char*>(self);
    fat<unsigned>(p, 0x00) = 0;                                           // 0x10006e0b
    fat<unsigned>(p, 0x04) = 0;                                           // 0x10006e0d
    fat<unsigned>(p, 0x10) = 2;                                           // 0x10006e10
    fat<unsigned>(p, 0x14) = 0;                                           // 0x10006e17
    fat<unsigned>(p, 0x0c) = 0x4000;                                      // 0x10006e1a
    fat<void*>(p, 0x04) = c3cx_fn<OperatorNew_t>(0x000424d7)(0x20000);    // 0x10006e21, 0x10006e26
    fat<unsigned>(p, 0x08) &= ~1u;                                        // 0x10006e29..0x10006e30
    fat<unsigned>(p, 0x18) = 0;                                           // 0x10006e38
    fat<unsigned>(p, 0x30) = 0;                                           // 0x10006e3b
    fat<unsigned>(p, 0x38) = 0;                                           // 0x10006e3e
    fat<unsigned short>(p, 0x1c) = 1;                                     // 0x10006e41, 0x10006e53
    fat<unsigned short>(p, 0x1e) = 2;                                     // 0x10006e5f
    fat<unsigned>(p, 0x20) = 0x5622;                                      // 0x10006e65
    fat<unsigned>(p, 0x24) = 0x15888;                                     // 0x10006e6c
    fat<unsigned short>(p, 0x28) = 4;                                     // 0x10006e73
    fat<unsigned short>(p, 0x2a) = 0x10;                                  // 0x10006e79
    fat<unsigned short>(p, 0x2c) = 0;                                     // 0x10006e4f, 0x10006e58
    fat<unsigned>(p, 0x34) = 0;                                           // 0x10006e5c
    return self;
}

// 0x00011180  initSoundVoice (VA 0x10011180)
// __thiscall with nothing pushed (`ret` at 0x1001124a), called by Wave_Device::open 0x1000b420; returns this
// (0x10011246). `call 0x10001c1c` with ecx = this+0x70 (0x10011184, 0x10011187; thunk to DsBuffer::ctor 0x1000eef0,
// which writes only inside this+0x70..this+0x143), then stores 0 at +0x168..+0x184 and +0x18c (0x10011191..0x100111c1),
// +0, +4, +8 (0x100111c7..0x100111cc), the byte +0x6c (0x100111cf), +0x148, +0x158, +0x15c, +0x154, +0x14c, +0x194,
// +0x198, +0x1ac, +0x1b0, +0x1b4 and the four dwords +0x19c..+0x1a8 (0x100111d9..0x10011234); +0x188 = 0x7f
// (0x100111df) and +0x160 = 2 (0x10011201); `rep stosd` clears the 0x18 dwords +0xc..+0x6b (0x1001122b..0x1001123e);
// +0x144 = 0 (0x10011240). None of these ranges overlap, so the order is not observable.
Ctor0_t InitSoundVoice_orig;
void* __fastcall InitSoundVoice_re(void* self, void*) {
    char* p = reinterpret_cast<char*>(self);
    c3cx_fn<Ctor0_t>(0x0000eef0)(p + 0x70, 0);                            // 0x10011187
    for (int o = 0x168; o <= 0x184; o += 4) fat<unsigned>(p, o) = 0;      // 0x10011191..0x100111bb
    fat<unsigned>(p, 0x18c) = 0;                                          // 0x100111c1
    fat<unsigned>(p, 0x00) = 0;                                           // 0x100111c7
    fat<unsigned>(p, 0x04) = 0;                                           // 0x100111c9
    fat<unsigned>(p, 0x08) = 0;                                           // 0x100111cc
    fat<unsigned char>(p, 0x6c) = 0;                                      // 0x100111cf
    fat<unsigned>(p, 0x148) = 0;                                          // 0x100111d9
    fat<unsigned>(p, 0x188) = 0x7f;                                       // 0x100111df
    fat<unsigned>(p, 0x158) = 0;                                          // 0x100111e9
    fat<unsigned>(p, 0x15c) = 0;                                          // 0x100111ef
    fat<unsigned>(p, 0x154) = 0;                                          // 0x100111f5
    fat<unsigned>(p, 0x14c) = 0;                                          // 0x100111fb
    fat<unsigned>(p, 0x160) = 2;                                          // 0x10011201
    fat<unsigned>(p, 0x194) = 0;                                          // 0x1001120b
    fat<unsigned>(p, 0x198) = 0;                                          // 0x10011211
    fat<unsigned>(p, 0x1ac) = 0;                                          // 0x10011217
    fat<unsigned>(p, 0x1b0) = 0;                                          // 0x1001121d
    fat<unsigned>(p, 0x1b4) = 0;                                          // 0x10011223
    for (int o = 0x19c; o <= 0x1a8; o += 4) fat<unsigned>(p, o) = 0;      // 0x10011229..0x10011234
    for (int o = 0x0c; o < 0x6c; o += 4) fat<unsigned>(p, o) = 0;         // rep stosd at 0x1001123e
    fat<unsigned>(p, 0x144) = 0;                                          // 0x10011240
    return self;
}

// =====================================================================================================================
// Ctrl5b19c_ctor
// =====================================================================================================================

// 0x0001bee0  Ctrl5b19c_ctor (VA 0x1001bee0)
// __thiscall with thirteen stack arguments (`ret 0x34` at 0x1001c004): twelve whose low bytes are stored and a
// dword. Called by five dynInit stubs (0x1001b510, 0x1001b6b0, 0x1001b710, 0x1001b770, 0x1001b7d0). Returns this
// (0x1001bff6). The SEH frame it sets up (0x1001bee0..0x1001beee, unlinked at 0x1001bffa) only matters if a callee
// throws; neither callee does anything but plain stores.
// In order: `call 0x10001235` with ecx = this+0xc (0x1001befe, 0x1001bf01; thunk to PtrList_init 0x1001d130, which
// zeroes four dwords); +4 = 0x18 (0x1001bf10); +8 = 0x44444444 (0x1001bf13); +0x28 = 0 (0x1001bf16); +0x1c = 0xc
// (0x1001bf1d); +0x20 = 0x44444444 (0x1001bf24); +0x2c = 0x1005b19c (0x1001bf2a); +0x40 = +0x4c = +0x50 = -1
// (0x1001bf31..0x1001bf37); +0x30 = 0x6c727463 (0x1001bf3d); the word +0x44 = 0 (0x1001bf4c); +0x38 = 0x18
// (0x1001bf51); +0x3c = 0x44444444 (0x1001bf54); the bytes +0x5c..+0x67 = the low bytes of arguments 1..12
// (0x1001bf57..0x1001bfbe); +0 = 0x1005b5c0 (0x1001bfae); +0x2c = 0x1005b59c (0x1001bfb4, replacing the first);
// `call 0x1000204a` with 0 pushed and ecx = this (0x1001bfa5..0x1001bfc1; thunk to Ctrl5b5c0_transposeTable
// 0x1001c1e0, whose body runs to 0x1001c23b and fills the bytes +0x68..+0xdb from the twelve at +0x5c); +0x48 = 3
// (0x1001bfca); +0xe0 = 0 (0x1001bfd1); +0x34 = 0xe4 (0x1001bfdb); +0xdc = argument 13 (0x1001bfc6, 0x1001bfe2);
// +0x58 = 0x44444444 (0x1001bfec); +0x54 = 0x8c (0x1001bfef).
// The three vtable constants are addresses inside the image and are resolved against the module base.
typedef void*(__fastcall* CtrlCtor_t)(void*, void*, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned,
                                       unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned);
CtrlCtor_t CtrlCtor_orig;
void* __fastcall CtrlCtor_re(void* self, void*, unsigned a1, unsigned a2, unsigned a3, unsigned a4, unsigned a5,
                             unsigned a6, unsigned a7, unsigned a8, unsigned a9, unsigned a10, unsigned a11,
                             unsigned a12, unsigned a13) {
    char* p = reinterpret_cast<char*>(self);
    c3cx_fn<Th0_t>(0x0001d130)(p + 0xc, 0);                               // 0x1001bf01
    fat<unsigned>(p, 0x04) = 0x18;                                        // 0x1001bf10
    fat<unsigned>(p, 0x08) = 0x44444444u;                                 // 0x1001bf13
    fat<unsigned>(p, 0x28) = 0;                                           // 0x1001bf16
    fat<unsigned>(p, 0x1c) = 0xc;                                         // 0x1001bf1d
    fat<unsigned>(p, 0x20) = 0x44444444u;                                 // 0x1001bf24
    fat<unsigned>(p, 0x40) = 0xffffffffu;                                 // 0x1001bf31
    fat<unsigned>(p, 0x4c) = 0xffffffffu;                                 // 0x1001bf34
    fat<unsigned>(p, 0x50) = 0xffffffffu;                                 // 0x1001bf37
    fat<unsigned>(p, 0x30) = 0x6c727463u;                                 // 0x1001bf3d
    fat<unsigned short>(p, 0x44) = 0;                                     // 0x1001bf4c
    fat<unsigned>(p, 0x38) = 0x18;                                        // 0x1001bf51
    fat<unsigned>(p, 0x3c) = 0x44444444u;                                 // 0x1001bf54
    const unsigned b[12] = {a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12};
    for (int i = 0; i < 12; ++i) fat<unsigned char>(p, 0x5c + i) = static_cast<unsigned char>(b[i]);   // 0x1001bf63..
    fat<char*>(p, 0x00) = c3cx_sound(0x0005b5c0);                         // 0x1001bfae
    fat<char*>(p, 0x2c) = c3cx_sound(0x0005b59c);                         // 0x1001bfb4 (0x1001bf2a wrote 0x1005b19c)
    c3cx_fn<Th1_t>(0x0001c1e0)(p, 0, 0);                                  // 0x1001bfc1
    fat<unsigned>(p, 0x48) = 3;                                           // 0x1001bfca
    fat<unsigned>(p, 0xe0) = 0;                                           // 0x1001bfd1
    fat<unsigned>(p, 0x34) = 0xe4;                                        // 0x1001bfdb
    fat<unsigned>(p, 0xdc) = a13;                                         // 0x1001bfe2
    fat<unsigned>(p, 0x58) = 0x44444444u;                                 // 0x1001bfec
    fat<unsigned>(p, 0x54) = 0x8c;                                        // 0x1001bfef
    return self;
}

// =====================================================================================================================
// CRT
// =====================================================================================================================

// 0x00041c26  ostreamResetVtable (VA 0x10041c26)
// __thiscall, nothing pushed. The dword eight bytes below `this` is the virtual-base table pointer (0x10041c26); its
// second dword is the offset of the virtual base (0x10041c29); the store at 0x10041c2c writes 0x1005ece4 (an image
// address, resolved against the module base) to [this - 8 + offset] with 32-bit wrapping arithmetic. The body then
// tail-jumps (`jmp 0x10041aad` at 0x10041c34) to iosResetVtable 0x10041aad with ecx unchanged, which re-reads the
// same two pointers and overwrites the same dword with 0x1005ecd4; its eax (the offset) is the result.
typedef unsigned(__fastcall* OstreamReset_t)(void*, void*);
OstreamReset_t OstreamReset_orig;
unsigned __fastcall OstreamReset_re(void* self, void*) {
    char* p = reinterpret_cast<char*>(self);
    const char* vb = *reinterpret_cast<char* const*>(p - 8);                                // 0x10041c26
    const unsigned off = *reinterpret_cast<const unsigned*>(vb + 4);                        // 0x10041c29
    char* dest = reinterpret_cast<char*>(static_cast<unsigned>(reinterpret_cast<uintptr_t>(p)) + off - 8u);
    *reinterpret_cast<void**>(dest) = c3cx_sound(0x0005ece4);                               // 0x10041c2c
    return c3cx_fn<IosReset_t>(0x00041aad)(self, 0);                                        // jmp at 0x10041c34
}

// The old small-block heap's region: +0x10 is the base address of its pages (0x1004748d, 0x1004710b) and the
// 8-byte entries from +0x18 describe one page each, indexed by (page - base) >> 12 with an arithmetic shift
// (`sar` at 0x10047490 / 0x1004710e, `lea [region + index*8 + 0x18]` at 0x10047496 / 0x10047111); entry +0 is a
// free-paragraph count and +4 is written 0xf1. A page holds at +0 a pointer into its own map and at +4 a run length
// (0x100474d8, 0x100474ee, 0x100474fb, 0x10047500..0x10047507); its map is the bytes from page+8 to page+0xf8
// (0x100474b6, 0x100474e2, 0x10047504), one per paragraph, an allocation's first byte holding its length.
unsigned* c3cx_sbhEntry(char* region, char* page) {
    const int index = static_cast<int>(static_cast<unsigned>(reinterpret_cast<uintptr_t>(page)) -
                                       fatc<unsigned>(region, 0x10)) >> 12;
    return reinterpret_cast<unsigned*>(region + index * 8 + 0x18);
}

// 0x00047474  sbhResizeBlock (VA 0x10047474)
// __cdecl(region, page, map byte of the block, new length in paragraphs) (`ret` at 0x1004751c pops nothing; the
// arguments at [ebp+8..0x14]); returns 1 when the block now has the new length, else 0 (the local at [ebp-4],
// cleared at 0x10047487, set at 0x1004750e). No callee.
//   - The old length is the map byte (0x10047480). Shrinking (old above new, UNSIGNED `jbe` at 0x1004749a): the map
//     byte becomes the new length (0x100474a1), the page entry's free count grows by old - new (0x100474a3), entry+4
//     = 0xf1 (0x100474a5), result 1.
//   - Equal (`jae` at 0x100474ae): result 0, nothing written.
//   - Growing: end = map byte + new (0x100474b3); end beyond page+0xf8 returns 0 (`jb` at 0x100474be); the bytes
//     from map byte + old up to end must all be 0 (0x100474c0..0x100474d1, the walk stopping at the first non-zero
//     byte), else 0. Then the map byte becomes the new length (0x100474d6). When the page's map pointer at page+0
//     lies in [map byte, end) (`ja` at 0x100474dc, `jbe` at 0x100474e0), it moves: to end with page+4 = the number
//     of consecutive zero bytes from end (0x100474ee..0x100474fb; the count starts at 0 and is only stepped while
//     the next byte is zero) when end is below page+0xf8 (`jae` at 0x100474ea), else to page+8 with page+4 = 0
//     (0x10047500..0x10047507). Finally the entry's free count changes by old - new (0x10047509, 0x1004750c), and
//     the result is 1.
typedef int(__cdecl* SbhResize_t)(char*, char*, unsigned char*, unsigned);
SbhResize_t SbhResize_orig;
int __cdecl SbhResize_re(char* region, char* page, unsigned char* mapb, unsigned newLen) {
    const unsigned oldLen = *mapb;                                        // 0x10047480
    unsigned* entry = c3cx_sbhEntry(region, page);                        // 0x1004748b..0x10047496
    if (oldLen > newLen) {                                                // jbe at 0x1004749a
        *mapb = static_cast<unsigned char>(newLen);                       // 0x100474a1
        entry[0] += oldLen - newLen;                                      // 0x100474a3
        entry[1] = 0xf1;                                                  // 0x100474a5
        return 1;
    }
    if (oldLen == newLen) return 0;                                       // jae at 0x100474ae
    unsigned char* end = mapb + newLen;                                   // 0x100474b3
    if (reinterpret_cast<unsigned char*>(page + 0xf8) < end) return 0;    // 0x100474bc, 0x100474be
    unsigned char* q = mapb + oldLen;                                     // 0x100474c0
    while (q < end && *q == 0) ++q;                                       // 0x100474c3..0x100474cd
    if (q != end) return 0;                                               // 0x100474cf, 0x100474d1
    *mapb = static_cast<unsigned char>(newLen);                           // 0x100474d6
    unsigned char* pfree = fatc<unsigned char*>(page, 0);                 // 0x100474d8
    if (!(mapb > pfree) && !(end <= pfree)) {                             // 0x100474dc, 0x100474e0
        if (end < reinterpret_cast<unsigned char*>(page + 0xf8)) {        // 0x100474ea
            fat<unsigned char*>(page, 0) = end;                           // 0x100474ee
            unsigned run = 0;
            if (end[0] == 0) {                                            // 0x100474f0, 0x100474f2
                do { ++run; } while (end[run] == 0);                      // 0x100474f4..0x100474f9
            }
            fat<unsigned>(page, 4) = run;                                 // 0x100474fb
        } else {
            fat<unsigned>(page, 4) = 0;                                   // 0x10047500
            fat<char*>(page, 0) = page + 8;                               // 0x10047504, 0x10047507
        }
    }
    entry[0] += oldLen - newLen;                                          // 0x10047509, 0x1004750c
    return 1;
}

// 0x00047103  sbhPageFree (VA 0x10047103)
// __cdecl(region, page, map byte of the block) (`ret` at 0x10047147 pops nothing; three arguments at
// [esp+4..0xc]). The page entry's free count grows by the map byte (0x10047119, 0x1004711c), the map byte is
// cleared (0x1004711e) and entry+4 = 0xf1 (0x10047127). When the count is then exactly 0xf0 (`cmp` / `jne` at
// 0x10047121, 0x1004712e) the module counter 0x100b5590 is incremented (0x10047130) and, when it reaches exactly
// 0x20 (0x10047136, 0x1004713d), `call 0x10046fea` (sbhRegionDecommit) runs with 0x10 pushed (0x1004713f,
// 0x10047141; cdecl, `pop ecx`). Nothing is loaded into eax for a return value (it keeps the entry address computed
// at 0x10047111), so the function is treated as returning nothing; its effects are the entry, the map byte and
// the counter.
typedef void(__cdecl* SbhPageFree_t)(char*, char*, unsigned char*);
SbhPageFree_t SbhPageFree_orig;
void __cdecl SbhPageFree_re(char* region, char* page, unsigned char* mapb) {
    unsigned* entry = c3cx_sbhEntry(region, page);                        // 0x10047103..0x10047111
    entry[0] += *mapb;                                                    // 0x10047119, 0x1004711c
    *mapb = 0;                                                            // 0x1004711e
    entry[1] = 0xf1;                                                      // 0x10047127
    if (entry[0] != 0xf0) return;                                         // 0x10047121, jne at 0x1004712e
    unsigned& counter = *reinterpret_cast<unsigned*>(c3cx_sound(0x000b5590));
    ++counter;                                                            // 0x10047130
    if (counter != 0x20) return;                                          // 0x10047136, jne at 0x1004713d
    c3cx_fn<Cdecl1v_t>(0x00046fea)(0x10);                                 // 0x1004713f, 0x10047141
}

// 0x0004e4b3  __free_lc_time (VA 0x1004e4b3)
// __cdecl(struct *) (after `push esi` the argument is read from [esp+8] at 0x1004e4b4; `ret` at 0x1004e642 pops
// nothing), called by __init_time 0x1004e0d4. A null pointer returns at once (`je 0x1004e641` at 0x1004e4ba).
// Otherwise free 0x10042c07 is called (cdecl) on 43 dwords of the struct, in this order: +4, +8, +0xc, +0x10, +0x14,
// +0x18, +0, +0x20 .. +0x34, +0x1c, +0x38, then +0x3c .. +0xa8 (0x1004e4c0..0x1004e639; the arguments are popped by
// `add esp, 0x40` at 0x1004e53f and 0x1004e5c2 and `add esp, 0x2c` at 0x1004e63e). Nothing is loaded into eax for
// a return value. The A/B replaces free with the heap stand-in's recording deallocator for the key, so the order and
// identity of the 43 pointers are compared and nothing is released.
typedef void(__cdecl* FreeLcTime_t)(void*);
typedef void(__cdecl* Free_t)(void*);
FreeLcTime_t FreeLcTime_orig;
void __cdecl FreeLcTime_re(void* lc) {
    if (!lc) return;                                                      // 0x1004e4b8, 0x1004e4ba
    static const unsigned char kOrder[43] = {
        0x04, 0x08, 0x0c, 0x10, 0x14, 0x18, 0x00, 0x20, 0x24, 0x28, 0x2c, 0x30, 0x34, 0x1c, 0x38,
        0x3c, 0x40, 0x44, 0x48, 0x4c, 0x50, 0x54, 0x58, 0x5c, 0x60, 0x64, 0x68, 0x6c, 0x70, 0x74,
        0x78, 0x7c, 0x80, 0x84, 0x88, 0x8c, 0x90, 0x94, 0x98, 0x9c, 0xa0, 0xa4, 0xa8};
    for (int i = 0; i < 43; ++i)                                          // 0x1004e4c0..0x1004e639
        c3cx_fn<Free_t>(0x00042c07)(fatc<void*>(lc, kOrder[i]));
}

// ---------------------------------------------------------------------------------------------------------------------
SG_HOOK("sound.dll", 0x00037B40, sound_Channel_set3DPositionX, Ch3DX_re, Ch3DX_orig);
SG_HOOK("sound.dll", 0x00037B80, sound_Channel_set3DPositionY, Ch3DY_re, Ch3DY_orig);
SG_HOOK("sound.dll", 0x00037BC0, sound_Channel_set3DPositionZ, Ch3DZ_re, Ch3DZ_orig);
SG_HOOK("sound.dll", 0x0003A5E0, sound_MultiSound_set3DPositionX, Ms3DX_re, Ms3DX_orig);
SG_HOOK("sound.dll", 0x0003A630, sound_MultiSound_set3DPositionY, Ms3DY_re, Ms3DY_orig);
SG_HOOK("sound.dll", 0x0003A650, sound_MultiSound_set3DPositionZ, Ms3DZ_re, Ms3DZ_orig);
SG_HOOK("sound.dll", 0x0003A5B0, sound_MultiSound_set3DPosition, Ms3DPos_re, Ms3DPos_orig);
SG_HOOK("sound.dll", 0x0003A600, sound_MultiSound_set3DBuffer, Ms3DBuf_re, Ms3DBuf_orig);
SG_HOOK("sound.dll", 0x0001E630, sound_Seq_scaleTickRate, SeqScaleTick_re, SeqScaleTick_orig);
SG_HOOK("sound.dll", 0x00005D80, sound_Sound_scaleTickRate, SoundScaleTick_re, SoundScaleTick_orig);
SG_HOOK("sound.dll", 0x00034C80, sound_Channel_setRateGlide, SetRateGlide_re, SetRateGlide_orig);
SG_HOOK("sound.dll", 0x0003A140, sound_MultiSound_setRateGlide, MsSetRateGlide_re, MsSetRateGlide_orig);
SG_HOOK("sound.dll", 0x0003A6E0, sound_MultiSound_setReverseRate, MsSetReverseRate_re, MsSetReverseRate_orig);
SG_HOOK("sound.dll", 0x0003A800, sound_MultiSound_is3DEnabled, MsIs3D_re, MsIs3D_orig);
SG_HOOK("sound.dll", 0x00039C80, sound_MultiSound_stop, MsStop_re, MsStop_orig);
SG_HOOK("sound.dll", 0x0003A730, sound_MultiSound_findStreamNode, MsFindStream_re, MsFindStream_orig);
SG_HOOK("sound.dll", 0x0003A080, sound_MultiSound_setVoiceBit1On, MsVoiceBit1_re, MsVoiceBit1_orig);
SG_HOOK("sound.dll", 0x0003A370, sound_MultiSound_applyFlags, MsApplyFlags_re, MsApplyFlags_orig);
SG_HOOK("sound.dll", 0x0002C370, sound_Channel_setField4Masked7f, ChSetField4_re, ChSetField4_orig);
SG_HOOK("sound.dll", 0x0002C0D0, sound_Channel_collectFlagBits, ChFlagBits_re, ChFlagBits_orig);
SG_HOOK("sound.dll", 0x0002BFF0, sound_Channel_setField5cClamped1200, ChSetField5c_re, ChSetField5c_orig);
SG_HOOK("sound.dll", 0x0003A560, sound_MultiSound_getStatus, MsGetStatus_re, MsGetStatus_orig);
SG_HOOK("sound.dll", 0x0003A0D0, sound_MultiSound_setVolume, MsSetVolume_re, MsSetVolume_orig);
SG_HOOK("sound.dll", 0x0003A210, sound_MultiSound_setPitchCents, MsSetPitch_re, MsSetPitch_orig);
SG_HOOK("sound.dll", 0x00034770, sound_Channel_allocAndInitVoice, AllocVoice_re, AllocVoice_orig);
SG_HOOK("sound.dll", 0x00011760, sound_getDeviceCaps, GetDeviceCaps_re, GetDeviceCaps_orig);
SG_HOOK("sound.dll", 0x00011790, sound_getNodeByIndex, NodeByIndex_re, NodeByIndex_orig);
SG_HOOK("sound.dll", 0x0001BEE0, sound_Ctrl5b19c_ctor, CtrlCtor_re, CtrlCtor_orig);
SG_HOOK("sound.dll", 0x00006E00, sound_initStreamBuffer, InitStreamBuffer_re, InitStreamBuffer_orig);
SG_HOOK("sound.dll", 0x00011180, sound_initSoundVoice, InitSoundVoice_re, InitSoundVoice_orig);
SG_HOOK("sound.dll", 0x00041C26, sound_ostreamResetVtable, OstreamReset_re, OstreamReset_orig);
SG_HOOK("sound.dll", 0x00047474, sound_sbhResizeBlock, SbhResize_re, SbhResize_orig);
SG_HOOK("sound.dll", 0x00047103, sound_sbhPageFree, SbhPageFree_re, SbhPageFree_orig);
SG_HOOK("sound.dll", 0x0004E4B3, sound___free_lc_time, FreeLcTime_re, FreeLcTime_orig);

}  // namespace

// C3 batch c3ew (round 32, 2026-10-13) of sound.dll: rows earlier batches dropped for heap use, imports, live module globals
// or a constant result (log/c3/c3ew_candidates.tsv). The A/B runs under the shared stand-ins (re/frida/js/heapstandin.js over
// operator new 0x100424d7 / operator delete 0x1004249a / malloc 0x10042acd; re/frida/js/importstandin.js over the DLL's IAT
// slots), with c3dt's thread-filtered recording stubs on the incremental-link thunks a body calls, and the module globals a
// body reads swapped for the key under a frozen first thread with the multimedia timer held off (0x100b49a8 = 5, c3dh).
// Fixture-owned objects only.
// Addresses in SG_HOOK and in the `// 0x...` comment lines are RVAs; instruction addresses quoted inside each body are VAs
// (VA = 0x10000000 + RVA), as re/tools/asm2inline.py and log/c3/alt/c3ew/dump.py print sound.dll.
// Hand-written from the disassembly (log/c3/alt/c3ew/dis/<rva>.txt). sound.dll is LoadLibrary'd after the shim starts, so these
// hooks install from the LoadLibraryA hook (SgHooksInstallPending, re/hooks.cpp).
//
// RULES (c3cy / c3dt): every import is called through the SAME IAT slot the original reads (module base + slot RVA); operator
// new / delete and malloc go through the DLL's own addresses; every callee runs through the address the original calls (the
// incremental-link thunk; its target is named in the comment); every image address the original uses as a constant (vtables,
// globals) is taken from the loaded base because sound.dll is relocatable. __thiscall is emulated with __fastcall (ecx = this,
// edx unused).
//
// Groups:
//   A. constructors with constant stores
//   B. wave-in device wrappers over g_waveIn 0x100b4a04 / the wave-in stream 0x100b4a24
//   C. Wave_Device output, construction and teardown over g_waveDevice 0x100b49f0 and its buffer globals
//   D. the sound timer (its critical section 0x100b4990) and a timed channel start
//   E. a command enqueue over g_soundEngine 0x100b4a20
#include "hooks.h"
#include <stdint.h>

namespace {

template <typename T> T& fat(void* base, int off) {
    return *reinterpret_cast<T*>(reinterpret_cast<char*>(base) + off);
}

char* c3ew_sound(unsigned rva) {
    static HMODULE m = 0;
    if (!m) m = GetModuleHandleA("sound.dll");
    return reinterpret_cast<char*>(m) + rva;
}
template <typename F> F c3ew_fn(unsigned rva) { return reinterpret_cast<F>(c3ew_sound(rva)); }
template <typename F> F c3ew_imp(unsigned slotRva) { return *reinterpret_cast<F*>(c3ew_sound(slotRva)); }
template <typename T> T& c3ew_g(unsigned rva) { return *reinterpret_cast<T*>(c3ew_sound(rva)); }

// module globals (RVAs)
const unsigned kGSoundTimer = 0x000b49ec;     // g_soundTimer
const unsigned kGWaveDevice = 0x000b49f0;     // g_waveDevice
const unsigned kGWaveStreamDev = 0x000b49f4;  // g_waveStreamDev
const unsigned kGBuf49f8 = 0x000b49f8;        // primary DsBuffer pointer
const unsigned kGBuf49fc = 0x000b49fc;
const unsigned kGBuf4a00 = 0x000b4a00;
const unsigned kGWaveIn = 0x000b4a04;         // g_waveIn
const unsigned kGMmioCount = 0x000b4a08;      // mmio handle counter
const unsigned kGMidiDevice = 0x000b4a1c;     // g_midiDevice
const unsigned kGSoundEngine = 0x000b4a20;    // g_soundEngine
const unsigned kGWaveInStream = 0x000b4a24;
const unsigned kGDsCoop = 0x000b4a3c;
const unsigned kGTickWord = 0x000b49a8;       // the timer's re-entrancy word
const unsigned kGMidiByte = 0x000b49bc;
const unsigned kTimerCS = 0x000b4990;         // the timer's CRITICAL_SECTION (24 bytes)

// IAT slots (pefile over original\sound.dll)
const unsigned kIatDeleteCS = 0x000b83e4;
const unsigned kIatInitCS = 0x000b83f0;
const unsigned kIatEnterCS = 0x000b83f4;
const unsigned kIatLeaveCS = 0x000b84e0;
const unsigned kIatTimeKillEvent = 0x000b8648;
const unsigned kIatTimeEndPeriod = 0x000b864c;
const unsigned kIatTimeGetDevCaps = 0x000b8650;
const unsigned kIatMmioGetInfo = 0x000b8668;
const unsigned kIatMmioClose = 0x000b8670;

typedef unsigned(__stdcall* Imp1_t)(unsigned);
typedef unsigned(__stdcall* ImpP_t)(void*);
typedef unsigned(__stdcall* Imp2_t)(unsigned, unsigned);
typedef unsigned(__stdcall* ImpPU_t)(void*, unsigned);
typedef unsigned(__stdcall* Imp3_t)(unsigned, void*, unsigned);

typedef unsigned(__cdecl* OpDelete_t)(void*);                                  // operator delete 0x1004249a
typedef void*(__cdecl* OpNew_t)(unsigned);                                     // operator new 0x100424d7
typedef unsigned(__fastcall* Th0_t)(void*, void*);                             // thiscall, nothing pushed
typedef unsigned(__fastcall* Th1_t)(void*, void*, unsigned);                   // thiscall, one stack argument
typedef unsigned(__fastcall* Th2_t)(void*, void*, unsigned, unsigned);
typedef unsigned(__fastcall* Th3_t)(void*, void*, unsigned, unsigned, unsigned);
typedef unsigned(__fastcall* Th4_t)(void*, void*, unsigned, unsigned, unsigned, unsigned);
typedef unsigned(__cdecl* Cdecl0_t)();

unsigned c3ew_delete(void* p) { return c3ew_fn<OpDelete_t>(0x0004249a)(p); }
void* c3ew_new(unsigned n) { return c3ew_fn<OpNew_t>(0x000424d7)(n); }
// virtual slot `off` of the object's vtable (thiscall)
template <typename F> F c3ew_vslot(void* obj, int off) { return *reinterpret_cast<F*>(*reinterpret_cast<char**>(obj) + off); }
unsigned c3ew_th0(unsigned rva, void* self) { return c3ew_fn<Th0_t>(rva)(self, 0); }
unsigned c3ew_th1(unsigned rva, void* self, unsigned a) { return c3ew_fn<Th1_t>(rva)(self, 0, a); }
unsigned c3ew_th2(unsigned rva, void* self, unsigned a, unsigned b) { return c3ew_fn<Th2_t>(rva)(self, 0, a, b); }
unsigned c3ew_th3(unsigned rva, void* self, unsigned a, unsigned b, unsigned c) { return c3ew_fn<Th3_t>(rva)(self, 0, a, b, c); }
unsigned c3ew_th4(unsigned rva, void* self, unsigned a, unsigned b, unsigned c, unsigned d) { return c3ew_fn<Th4_t>(rva)(self, 0, a, b, c, d); }
unsigned c3ew_u(void* p) { return reinterpret_cast<unsigned>(p); }

// The node list {+0 front, +4 back, +0xc count} of a header at `list` with nodes {+0 previous, +4 next, +8 value}: one pop from
// the front as the bodies below inline it (`mov ecx, [eax+4]; test; je; mov [ecx], 0; mov [front], ecx; jmp` / `mov [back], 0;
// mov [front], 0`, then the value read, the node deleted and the count decremented). Returns the value; *ret is operator
// delete's eax.
void* c3ew_popFront(char* list, int frontOff, int backOff, int countOff, char* node, unsigned* ret) {
    char* next = fat<char*>(node, 4);
    if (next) {
        fat<void*>(next, 0) = 0;
        fat<char*>(list, frontOff) = next;
    } else {
        fat<void*>(list, backOff) = 0;
        fat<void*>(list, frontOff) = 0;
    }
    void* v = fat<void*>(node, 8);
    *ret = c3ew_delete(node);
    fat<int>(list, countOff) -= 1;
    return v;
}

// =====================================================================================================================
// A. constructors with constant stores
// =====================================================================================================================

// 0x0002a240  WaveInDeviceBase_ctor (VA 0x1002a240). __thiscall, `ret` (0x1002a267), returns this (`mov eax, ecx` 0x1002a240).
// +4 = 0 (0x1002a249), +8 = 0x7f (0x1002a24b), +0xc / +0x18 / +0x1c / +0x10 / +0x14 = 0 (0x1002a252-0x1002a25e), then the vtable
// 0x1005ba5c at +0 (0x1002a261).
typedef void*(__fastcall* WiBaseCtor_t)(void*, void*);
WiBaseCtor_t WiBaseCtor_orig;
void* __fastcall WiBaseCtor_re(void* self, void*) {
    fat<unsigned>(self, 4) = 0;                                               // 0x1002a249
    fat<unsigned>(self, 8) = 0x7f;                                            // 0x1002a24b
    fat<unsigned>(self, 0xc) = 0;                                             // 0x1002a252
    fat<unsigned>(self, 0x18) = 0;                                            // 0x1002a255
    fat<unsigned>(self, 0x1c) = 0;                                            // 0x1002a258
    fat<unsigned>(self, 0x10) = 0;                                            // 0x1002a25b
    fat<unsigned>(self, 0x14) = 0;                                            // 0x1002a25e
    fat<void*>(self, 0) = c3ew_sound(0x0005ba5c);                             // 0x1002a261
    return self;                                                              // 0x1002a240
}

// 0x0002cee0  MixBuffer_ctor (VA 0x1002cee0). __thiscall, `ret` (0x1002cf2e), returns this (0x1002cf2b). The vtable 0x1005be84 at +0
// (0x1002ceea); +0x208 = 0 then (0 & ~4) | 2 = 2 (0x1002cef3 / 0x1002cef9); +0x20c, +0x210, +0x204, +0x214 = 0 (0x1002cf07-0x1002cf19),
// +0x218 = 0xfa (0x1002cf1f); `rep stosd` clears the 0x80 dwords +4..+0x203 (0x1002ceff / 0x1002cf04 / 0x1002cf29).
typedef void*(__fastcall* MixBufCtor_t)(void*, void*);
MixBufCtor_t MixBufCtor_orig;
void* __fastcall MixBufCtor_re(void* self, void*) {
    fat<void*>(self, 0) = c3ew_sound(0x0005be84);                             // 0x1002ceea
    fat<unsigned>(self, 0x208) = 0;                                           // 0x1002cef3
    fat<unsigned>(self, 0x208) = (0u & 0xfffffffbu) | 2u;                     // 0x1002cee7 / 0x1002cef0 / 0x1002cef9
    fat<unsigned>(self, 0x20c) = 0;                                           // 0x1002cf07
    fat<unsigned>(self, 0x210) = 0;                                           // 0x1002cf0d
    fat<unsigned>(self, 0x204) = 0;                                           // 0x1002cf13
    fat<unsigned>(self, 0x214) = 0;                                           // 0x1002cf19
    fat<unsigned>(self, 0x218) = 0xfa;                                        // 0x1002cf1f
    for (int i = 0; i < 0x80; i++) fat<unsigned>(self, 4 + 4 * i) = 0;        // 0x1002cf29
    return self;                                                              // 0x1002cf2b
}

// =====================================================================================================================
// B. wave-in device wrappers
// =====================================================================================================================

// 0x0000ec00  Wave_In_Device::closeCapture (VA 0x1000ec00). __thiscall, `ret`, returns 0 (0x1000ec36). With a non-zero timer
// object at this+0x30 (0x1000ec03 / 0x1000ec06): `call 0x100026fd` (thunk to SoundTimer::killEvent 0x10008d70) with ecx = that
// object (0x1000ec0a), then `call 0x10001ed3` (thunk to SoundTimer::unregisterClient 0x10008920) with ecx = g_soundTimer
// 0x100b49ec and this pushed (0x1000ec0f-0x1000ec16). Then a non-zero g_waveIn 0x100b4a04 (0x1000ec1b / 0x1000ec22) receives its
// vtable slot 0 with 1 pushed (the deleting destructor, 0x1000ec26-0x1000ec2a) and the global is cleared (0x1000ec2c).
typedef unsigned(__fastcall* WiCloseCap_t)(void*, void*);
WiCloseCap_t WiCloseCap_orig;
unsigned __fastcall WiCloseCap_re(void* self, void*) {
    void* t = fat<void*>(self, 0x30);                                         // 0x1000ec03
    if (t) {                                                                  // 0x1000ec06 / 0x1000ec08
        c3ew_th0(0x000026fd, t);                                              // 0x1000ec0a
        c3ew_th1(0x00001ed3, c3ew_g<void*>(kGSoundTimer), c3ew_u(self));      // 0x1000ec0f-0x1000ec16
    }
    void* w = c3ew_g<void*>(kGWaveIn);                                        // 0x1000ec1b
    if (w) {                                                                  // 0x1000ec22 / 0x1000ec24
        c3ew_vslot<Th1_t>(w, 0)(w, 0, 1);                                     // 0x1000ec26-0x1000ec2a
        c3ew_g<void*>(kGWaveIn) = 0;                                          // 0x1000ec2c
    }
    return 0;                                                                 // 0x1000ec36
}

// 0x0000ea30  Wave_In_Device::start (VA 0x1000ea30). Plain `ret`; ecx is overwritten at once (0x1000ea30), so nothing is read from
// the caller. A NULL g_waveIn 0x100b4a04 returns 3 (0x1000ea36-0x1000ea3a). Otherwise four steps on the object the global holds,
// reloaded before each: `call 0x1000213a` (WaveInDevice_open 0x10039050, 0x1000ea40), `call 0x1000219e` (WaveInDevice_prepareHeaders
// 0x10038b40, 0x1000ea4f), `call 0x100024b4` (WaveInDevice_addBuffer 0x10038da0, 0x1000ea5e); a non-zero result of any of them is
// returned (0x1000ea45 / 0x1000ea54 / 0x1000ea63); the last step is the tail jump `jmp 0x10002121` (WaveInDevice_start 0x10038b20,
// 0x1000ea6d), whose result is returned.
typedef unsigned(__fastcall* WiStartDev_t)(void*, void*);
WiStartDev_t WiStartDev_orig;
unsigned __fastcall WiStartDev_re(void*, void*) {
    void* w = c3ew_g<void*>(kGWaveIn);                                        // 0x1000ea30
    if (!w) return 3;                                                         // 0x1000ea36-0x1000ea3a
    unsigned r = c3ew_th0(0x0000213a, w);                                     // 0x1000ea40
    if (r) return r;                                                          // 0x1000ea45 / 0x1000ea47
    r = c3ew_th0(0x0000219e, c3ew_g<void*>(kGWaveIn));                        // 0x1000ea49 / 0x1000ea4f
    if (r) return r;                                                          // 0x1000ea54 / 0x1000ea56
    r = c3ew_th0(0x000024b4, c3ew_g<void*>(kGWaveIn));                        // 0x1000ea58 / 0x1000ea5e
    if (r) return r;                                                          // 0x1000ea63 / 0x1000ea65
    return c3ew_th0(0x00002121, c3ew_g<void*>(kGWaveIn));                     // 0x1000ea67 / 0x1000ea6d
}

// 0x0000ead0  Wave_In_Device::openCapture (VA 0x1000ead0). __thiscall, two stack arguments (`ret 8` 0x1000eb70 / 0x1000eb93); only
// the first is read ([esp+0x1c] at 0x1000eb73). The C++ frame (0x1000ead0-0x1000eade) only guards the constructor call.
// A non-zero dword at this+0x28 returns 0 at once (0x1000eaea-0x1000eaef -> 0x1000eb81). A non-zero g_waveIn 0x100b4a04 sets
// this+0x28 = 1 and returns 0 (0x1000eaf5-0x1000eafc -> 0x1000eb7a). Otherwise operator new(0x70) (0x1000eb00); a non-NULL block
// goes to `call 0x10001fa5` (thunk to WaveInDevice_ctor 0x10038650, 0x1000eb1a), whose eax is the object, a NULL block gives 0
// (0x1000eb21); the object is stored in g_waveIn (0x1000eb34) and `call 0x10002162` (WaveInDevice_allocCaptureBuffers 0x10038890)
// runs on it with (0xa, 0x168) (0x1000eb23-0x1000eb39). A non-zero result: the object in g_waveIn (reloaded, 0x1000eb44) receives
// its vtable slot 0 with 1 (0x1000eb4e-0x1000eb52) when non-NULL, g_waveIn is cleared (0x1000eb57) and the result is returned
// (0x1000eb54). A zero result: this+0x48 = the first argument (0x1000eb73 / 0x1000eb77), this+0x28 = 1 (0x1000eb7a), 0 is returned.
typedef unsigned(__fastcall* WiOpenCap_t)(void*, void*, unsigned, unsigned);
WiOpenCap_t WiOpenCap_orig;
unsigned __fastcall WiOpenCap_re(void* self, void*, unsigned a1, unsigned) {
    if (fat<unsigned>(self, 0x28) != 0) return 0;                             // 0x1000eaea-0x1000eaef
    if (c3ew_g<void*>(kGWaveIn) != 0) {                                       // 0x1000eaf5-0x1000eafc
        fat<unsigned>(self, 0x28) = 1;                                        // 0x1000eb7a
        return 0;
    }
    void* p = c3ew_new(0x70);                                                 // 0x1000eafe / 0x1000eb00
    void* obj = p ? reinterpret_cast<void*>(c3ew_th0(0x00001fa5, p)) : 0;     // 0x1000eb0c-0x1000eb21
    c3ew_g<void*>(kGWaveIn) = obj;                                            // 0x1000eb34
    unsigned r = c3ew_th2(0x00002162, obj, 0xa, 0x168);                       // 0x1000eb23-0x1000eb39
    if (r) {                                                                  // 0x1000eb40 / 0x1000eb42
        void* w = c3ew_g<void*>(kGWaveIn);                                    // 0x1000eb44
        if (w) c3ew_vslot<Th1_t>(w, 0)(w, 0, 1);                              // 0x1000eb4a-0x1000eb52
        c3ew_g<void*>(kGWaveIn) = 0;                                          // 0x1000eb57
        return r;                                                             // 0x1000eb54
    }
    fat<unsigned>(self, 0x48) = a1;                                           // 0x1000eb73 / 0x1000eb77
    fat<unsigned>(self, 0x28) = 1;                                            // 0x1000eb7a
    return 0;                                                                 // 0x1000eb86
}

// 0x000319e0  WaveInStream_resetCodec (VA 0x100319e0). __thiscall, `ret`, returns 0 (0x10031a1b). +0x80 = 0, +0x84 = 0, +0x8c
// incremented, +0x88 = 0 (0x100319e5-0x100319fe); codecAllocContextA 0x1003aa90 (a direct call, cdecl, no arguments, 0x10031a04)
// and its result stored at +0x90 (0x10031a09); then `call 0x100015b4` (thunk to Wave_In_Device::setStream 0x1000ed70) with
// ecx = the module word 0x100b4a24 and this pushed (0x10031a0f-0x10031a16).
typedef unsigned(__fastcall* WsResetCodec_t)(void*, void*);
WsResetCodec_t WsResetCodec_orig;
unsigned __fastcall WsResetCodec_re(void* self, void*) {
    unsigned n = fat<unsigned>(self, 0x8c);                                   // 0x100319e5
    fat<unsigned>(self, 0x80) = 0;                                            // 0x100319eb
    fat<unsigned>(self, 0x84) = 0;                                            // 0x100319f2
    fat<unsigned>(self, 0x8c) = n + 1;                                        // 0x100319f1 / 0x100319f8
    fat<unsigned>(self, 0x88) = 0;                                            // 0x100319fe
    fat<unsigned>(self, 0x90) = c3ew_fn<Cdecl0_t>(0x0003aa90)();              // 0x10031a04 / 0x10031a09
    c3ew_th1(0x000015b4, c3ew_g<void*>(kGWaveInStream), c3ew_u(self));        // 0x10031a0f-0x10031a16
    return 0;                                                                 // 0x10031a1b
}

// 0x00031560  freeMmioHandleListGetInfo (VA 0x10031560). __thiscall, `ret`, returns 0 (0x1003161e). The list at +0x5c (front)
// / +0x60 (back) / +0x68 (count) is popped from the front (0x1003156b-0x1003159c, the pop of c3ew_popFront). The walk ends when the
// list is empty (0x10031570 / 0x100315d7) or a popped value is NULL (0x1003159f / 0x10031602). For each non-NULL value, the mmio
// handle in its FIRST dword gets mmioGetInfo [0x100b8668] (handle, a 0x48-byte stack MMIOINFO, 0) (0x100315a7-0x100315b0) and
// mmioClose [0x100b8670] (handle, 0) (0x100315b2-0x100315b6), the module counter 0x100b4a08 is decremented (0x100315bc-0x100315c4)
// and the value is deleted (0x100315ca). Then a non-zero word 0x100b4a24 receives `call 0x10001ba4` (thunk to
// Wave_In_Device::clearStream 0x1000eda0) (0x10031604-0x1003160e), and bit 2 of the dword at +0x7c is cleared (0x10031613-0x10031619).
typedef unsigned(__fastcall* FreeMmioGI_t)(void*, void*);
FreeMmioGI_t FreeMmioGI_orig;
unsigned __fastcall FreeMmioGI_re(void* self, void*) {
    char* s = static_cast<char*>(self);
    unsigned dr;
    char* node = fat<char*>(s, 0x5c);                                         // 0x1003156b
    if (node) {                                                               // 0x1003156e / 0x10031570
        unsigned* v = static_cast<unsigned*>(c3ew_popFront(s, 0x5c, 0x60, 0x68, node, &dr));   // 0x10031576-0x1003159c
        while (v) {                                                           // 0x1003159f / 0x10031602
            unsigned char info[0x48];
            c3ew_imp<Imp3_t>(kIatMmioGetInfo)(v[0], info, 0);                 // 0x100315a7-0x100315b0
            c3ew_imp<Imp2_t>(kIatMmioClose)(v[0], 0);                         // 0x100315b2-0x100315b6
            c3ew_g<unsigned>(kGMmioCount) -= 1;                               // 0x100315bc-0x100315c4
            c3ew_delete(v);                                                   // 0x100315ca
            node = fat<char*>(s, 0x5c);                                       // 0x100315cf
            if (!node) break;                                                 // 0x100315d5 / 0x100315d7
            v = static_cast<unsigned*>(c3ew_popFront(s, 0x5c, 0x60, 0x68, node, &dr));         // 0x100315d9-0x100315ff
        }
    }
    void* st = c3ew_g<void*>(kGWaveInStream);                                 // 0x10031604
    if (st) c3ew_th0(0x00001ba4, st);                                         // 0x1003160a-0x1003160e
    fat<unsigned>(s, 0x7c) &= 0xfffffffbu;                                    // 0x10031613-0x10031619
    return 0;                                                                 // 0x1003161e
}

// =====================================================================================================================
// C. Wave_Device output, construction and teardown
// =====================================================================================================================

// 0x0000bf90  Wave_Device::startOutput (VA 0x1000bf90). __thiscall, `ret`. A non-zero primary buffer 0x100b49f8 gets `call 0x10001339`
// (thunk to DsBuffer::restartPlayback 0x1000f020) (0x1000bf93-0x1000bf9d). With bit 4 of the byte at g_waveDevice+0x6c (no NULL test
// on g_waveDevice, 0x1000bfa2 / 0x1000bfa7), the buffers 0x100b49fc and 0x100b4a00, each when non-zero, get `call 0x10001203`
// (thunk to DsBuffer::play 0x10010db0) with 1 (0x1000bfad-0x1000bfca). Then a non-zero g_soundTimer 0x100b49ec (0x1000bfcf / 0x1000bfd4)
// is stored at this+0x1a8 (0x1000bfd8), `call 0x1000227a` (SoundTimer::registerClient 0x100088e0) runs on the reloaded global with this
// pushed (0x1000bfde-0x1000bfe5) and `call 0x10001dfc` (SoundTimer::start 0x10008ce0) on it (0x1000bfea / 0x1000bff0), whose eax is
// returned; with g_soundTimer NULL, eax holds that 0 (0x1000bfcf).
typedef unsigned(__fastcall* WdStartOut_t)(void*, void*);
WdStartOut_t WdStartOut_orig;
unsigned __fastcall WdStartOut_re(void* self, void*) {
    void* b = c3ew_g<void*>(kGBuf49f8);                                       // 0x1000bf93
    if (b) c3ew_th0(0x00001339, b);                                           // 0x1000bf99-0x1000bf9d
    if (fat<unsigned char>(c3ew_g<void*>(kGWaveDevice), 0x6c) & 0x10) {       // 0x1000bfa2-0x1000bfab
        void* b1 = c3ew_g<void*>(kGBuf49fc);                                  // 0x1000bfad
        if (b1) c3ew_th1(0x00001203, b1, 1);                                  // 0x1000bfb3-0x1000bfb9
        void* b2 = c3ew_g<void*>(kGBuf4a00);                                  // 0x1000bfbe
        if (b2) c3ew_th1(0x00001203, b2, 1);                                  // 0x1000bfc4-0x1000bfca
    }
    void* t = c3ew_g<void*>(kGSoundTimer);                                    // 0x1000bfcf
    if (!t) return 0;                                                         // 0x1000bfd4 / 0x1000bfd6
    fat<void*>(self, 0x1a8) = t;                                              // 0x1000bfd8
    c3ew_th1(0x0000227a, c3ew_g<void*>(kGSoundTimer), c3ew_u(self));          // 0x1000bfde-0x1000bfe5
    return c3ew_th0(0x00001dfc, c3ew_g<void*>(kGSoundTimer));                 // 0x1000bfea / 0x1000bff0
}

// 0x0000bef0  Wave_Device::stopOutput (VA 0x1000bef0). __thiscall, `ret`. `call 0x100026fd` (SoundTimer::killEvent 0x10008d70) on
// g_soundTimer 0x100b49ec with no NULL test (0x1000bef3-0x1000befa); a non-zero this+0x1a8 (0x1000beff-0x1000bf09) adds `call 0x10001ed3`
// (SoundTimer::unregisterClient 0x10008920) on the reloaded global with this pushed (0x1000bf0b-0x1000bf12). The primary buffer
// 0x100b49f8 gets `call 0x10001a2d` (thunk to DsBuffer::stop 0x1000f760) when non-zero (0x1000bf17-0x1000bf21); with bit 4 of the byte at
// g_waveDevice+0x6c (no NULL test, 0x1000bf26 / 0x1000bf2b) so do 0x100b49fc and 0x100b4a00 (0x1000bf31-0x1000bf4a). A non-zero
// g_waveStreamDev 0x100b49f4 gets +0x18 = 0 (0x1000bf4f-0x1000bf58); this+0x1b0 and +0x1b4 = 0 (0x1000bf5b / 0x1000bf61). eax at the
// `ret` is the value of 0x100b49f4 loaded at 0x1000bf4f.
typedef unsigned(__fastcall* WdStopOut_t)(void*, void*);
WdStopOut_t WdStopOut_orig;
unsigned __fastcall WdStopOut_re(void* self, void*) {
    c3ew_th0(0x000026fd, c3ew_g<void*>(kGSoundTimer));                        // 0x1000bef3-0x1000befa
    if (fat<unsigned>(self, 0x1a8) != 0)                                      // 0x1000beff-0x1000bf09
        c3ew_th1(0x00001ed3, c3ew_g<void*>(kGSoundTimer), c3ew_u(self));      // 0x1000bf0b-0x1000bf12
    void* b = c3ew_g<void*>(kGBuf49f8);                                       // 0x1000bf17
    if (b) c3ew_th0(0x00001a2d, b);                                           // 0x1000bf1d-0x1000bf21
    if (fat<unsigned char>(c3ew_g<void*>(kGWaveDevice), 0x6c) & 0x10) {       // 0x1000bf26-0x1000bf2f
        void* b1 = c3ew_g<void*>(kGBuf49fc);                                  // 0x1000bf31
        if (b1) c3ew_th0(0x00001a2d, b1);                                     // 0x1000bf37-0x1000bf3b
        void* b2 = c3ew_g<void*>(kGBuf4a00);                                  // 0x1000bf40
        if (b2) c3ew_th0(0x00001a2d, b2);                                     // 0x1000bf46-0x1000bf4a
    }
    void* s = c3ew_g<void*>(kGWaveStreamDev);                                 // 0x1000bf4f
    if (s) fat<unsigned>(s, 0x18) = 0;                                        // 0x1000bf54-0x1000bf58
    fat<unsigned>(self, 0x1b0) = 0;                                           // 0x1000bf5b
    fat<unsigned>(self, 0x1b4) = 0;                                           // 0x1000bf61
    return c3ew_u(s);                                                         // eax from 0x1000bf4f
}

// 0x0000be00  Wave_Device::closeOutput (VA 0x1000be00). __thiscall, `ret`, returns 0 (0x1000be0c / 0x1000beba). Bit 1 of the byte at
// this+0x1a4 set returns at once (0x1000be03-0x1000be0a). Otherwise: a non-zero g_midiDevice 0x100b4a1c gets its vtable slot +0x5c
// (0x1000be10-0x1000be1f); this's own slot +0x4c (0x1000be22-0x1000be26); `call 0x100026fd` (SoundTimer::killEvent) on g_soundTimer
// 0x100b49ec (0x1000be29 / 0x1000be2f); `call 0x10001f0f` (thunk to flushSoundList 0x10008470) with ecx = g_waveStreamDev 0x100b49f4
// (0x1000be34 / 0x1000be3a); this+0x1b0 and +0x1b4 = 0 (0x1000be3f / 0x1000be45). With a non-zero g_waveDevice 0x100b49f0
// (0x1000be4b-0x1000be53), each of the buffers 0x100b49f8, 0x100b49fc, 0x100b4a00 that is non-zero is pushed to `call 0x10001677`
// (thunk to DirectSoundDevice::removeBuffer 0x100132e0) with ecx = g_waveDevice (reloaded after each call) and its global cleared
// (0x1000be55-0x1000bea0), then `call 0x100020f9` (thunk to DirectSoundDevice::stopAll 0x10011a80) on g_waveDevice (0x1000bea6).
// Last, bit 1 of this+0x1a4 is set (0x1000beab-0x1000beb4).
typedef unsigned(__fastcall* WdCloseOut_t)(void*, void*);
WdCloseOut_t WdCloseOut_orig;
unsigned __fastcall WdCloseOut_re(void* self, void*) {
    if (fat<unsigned char>(self, 0x1a4) & 2) return 0;                        // 0x1000be03-0x1000be0c
    void* m = c3ew_g<void*>(kGMidiDevice);                                    // 0x1000be10
    if (m) c3ew_vslot<Th0_t>(m, 0x5c)(m, 0);                                  // 0x1000be19-0x1000be1f
    c3ew_vslot<Th0_t>(self, 0x4c)(self, 0);                                   // 0x1000be22-0x1000be26
    c3ew_th0(0x000026fd, c3ew_g<void*>(kGSoundTimer));                        // 0x1000be29 / 0x1000be2f
    c3ew_th0(0x00001f0f, c3ew_g<void*>(kGWaveStreamDev));                     // 0x1000be34 / 0x1000be3a
    fat<unsigned>(self, 0x1b0) = 0;                                           // 0x1000be3f
    fat<unsigned>(self, 0x1b4) = 0;                                           // 0x1000be45
    void* d = c3ew_g<void*>(kGWaveDevice);                                    // 0x1000be4b
    if (d) {                                                                  // 0x1000be51 / 0x1000be53
        static const unsigned bufs[3] = {kGBuf49f8, kGBuf49fc, kGBuf4a00};
        for (int i = 0; i < 3; i++) {                                         // 0x1000be55-0x1000bea0
            unsigned b = c3ew_g<unsigned>(bufs[i]);
            if (b) {
                c3ew_th1(0x00001677, d, b);
                d = c3ew_g<void*>(kGWaveDevice);
                c3ew_g<unsigned>(bufs[i]) = 0;
            }
        }
        c3ew_th0(0x000020f9, d);                                              // 0x1000bea6
    }
    fat<unsigned>(self, 0x1a4) |= 2;                                          // 0x1000beab-0x1000beb4
    return 0;                                                                 // 0x1000beba
}

// 0x0000b1d0  Wave_Device::ctor (VA 0x1000b1d0). __thiscall, `ret`, returns this (0x1000b26d). The C++ frame (0x1000b1d0-0x1000b1de)
// only guards the member constructors. `call 0x100021df` (thunk to VoiceBase_ctor 0x10029020) on this (0x1000b1ee); +0x1c8 / +0x1cc /
// +0x1d0 / +0x1d4 = 0 (0x1000b1f9-0x1000b20b); `call 0x100010aa` (thunk to CommandQueue::init 0x10008860) on this+0x1d8 (0x1000b211 /
// 0x1000b21c); the vtable 0x1005b304 at +0 (0x1000b225); the module globals g_waveDevice 0x100b49f0, 0x100b49f8 and g_waveStreamDev
// 0x100b49f4 cleared (0x1000b231-0x1000b23d); +0x1a8, +0x1ac, +0x1b0 = 0, +0x1b8 = 0x7f, +0x1a4, +0x1b4, +0x1c4 = 0 (0x1000b243-0x1000b267).
typedef void*(__fastcall* WdCtor_t)(void*, void*);
WdCtor_t WdCtor_orig;
void* __fastcall WdCtor_re(void* self, void*) {
    char* s = static_cast<char*>(self);
    c3ew_th0(0x000021df, s);                                                  // 0x1000b1ee
    fat<unsigned>(s, 0x1c8) = 0;                                              // 0x1000b1f9
    fat<unsigned>(s, 0x1cc) = 0;                                              // 0x1000b1ff
    fat<unsigned>(s, 0x1d0) = 0;                                              // 0x1000b205
    fat<unsigned>(s, 0x1d4) = 0;                                              // 0x1000b20b
    c3ew_th0(0x000010aa, s + 0x1d8);                                          // 0x1000b211 / 0x1000b21c
    fat<void*>(s, 0) = c3ew_sound(0x0005b304);                                // 0x1000b225
    c3ew_g<unsigned>(kGWaveDevice) = 0;                                       // 0x1000b231
    c3ew_g<unsigned>(kGBuf49f8) = 0;                                          // 0x1000b237
    c3ew_g<unsigned>(kGWaveStreamDev) = 0;                                    // 0x1000b23d
    fat<unsigned>(s, 0x1a8) = 0;                                              // 0x1000b243
    fat<unsigned>(s, 0x1ac) = 0;                                              // 0x1000b249
    fat<unsigned>(s, 0x1b0) = 0;                                              // 0x1000b24f
    fat<unsigned>(s, 0x1b8) = 0x7f;                                           // 0x1000b255
    fat<unsigned>(s, 0x1a4) = 0;                                              // 0x1000b25f
    fat<unsigned>(s, 0x1b4) = 0;                                              // 0x1000b261
    fat<unsigned>(s, 0x1c4) = 0;                                              // 0x1000b267
    return self;                                                              // 0x1000b26d
}

// 0x0000b2e0  Wave_Device::dtor (VA 0x1000b2e0). __thiscall, `ret`; eax at the `ret` is VoiceBase_dtor's (the last call, 0x1000b3c3).
// The vtable 0x1005b304 at +0 (0x1000b2ff); a non-zero g_waveDevice 0x100b49f0 (0x1000b305-0x1000b316) runs `call 0x10001a9b` (thunk to
// Wave_Device::stop 0x1000b960) on this (0x1000b318); `call 0x100019a1` (thunk to CommandQueue::clear 0x10008890) on this+0x1d8
// (0x1000b31d / 0x1000b328). The list at +0x1c8 (front) / +0x1cc (back) / +0x1d4 (count) is popped from the front, each node deleted,
// until it is empty (0x1000b337 / 0x1000b37d) or a popped node's value (+8) is NULL (0x1000b373 / 0x1000b3b7); the values are not freed.
// Then `call 0x10001dac` (thunk to VoiceBase_dtor 0x10029200) on this (0x1000b3b9-0x1000b3c3).
typedef unsigned(__fastcall* WdDtor_t)(void*, void*);
WdDtor_t WdDtor_orig;
unsigned __fastcall WdDtor_re(void* self, void*) {
    char* s = static_cast<char*>(self);
    fat<void*>(s, 0) = c3ew_sound(0x0005b304);                                // 0x1000b2ff
    if (c3ew_g<void*>(kGWaveDevice)) c3ew_th0(0x00001a9b, s);                 // 0x1000b305-0x1000b318
    c3ew_th0(0x000019a1, s + 0x1d8);                                          // 0x1000b31d / 0x1000b328
    unsigned dr;
    char* node = fat<char*>(s, 0x1c8);                                        // 0x1000b32d
    while (node) {                                                            // 0x1000b337 / 0x1000b37d
        void* v = c3ew_popFront(s, 0x1c8, 0x1cc, 0x1d4, node, &dr);           // 0x1000b33b-0x1000b36d / 0x1000b37f-0x1000b3b1
        if (!v) break;                                                        // 0x1000b373 / 0x1000b3b7
        node = fat<char*>(s, 0x1c8);                                          // 0x1000b375
    }
    return c3ew_th0(0x00001dac, s);                                           // 0x1000b3b9-0x1000b3c3
}

// 0x0000b960  Wave_Device::stop (VA 0x1000b960). __thiscall, `ret`, returns 0 (0x1000ba53). A non-zero timer at this+0x1a8
// (0x1000b964-0x1000b96c) gets `call 0x100026fd` (SoundTimer::killEvent, ecx = that timer, 0x1000b96e) and then `call 0x10001ed3`
// (SoundTimer::unregisterClient) on g_soundTimer 0x100b49ec with this pushed (0x1000b973-0x1000b97a). Bit 5 of the byte at this+0x1a4
// (0x1000b97f): set, the module byte 0x100b49bc becomes (byte & 0xfe) ^ 1 (0x1000b988-0x1000b991); clear, `call 0x10002757` (thunk to
// stopSoundThreadFull 0x1002a8a0) with ecx = 0x100b5038 (0x1000b998 / 0x1000b99d). `call 0x1000188e` (thunk to
// Wave_Device::flushCommandQueue 0x1000cc70) on this (0x1000b9a2 / 0x1000b9a4). With a non-zero g_waveDevice 0x100b49f0 (0x1000b9a9-
// 0x1000b9b1): a non-zero 0x100b49f8 is pushed to `call 0x10001677` (DirectSoundDevice::removeBuffer, ecx = g_waveDevice) and cleared
// (0x1000b9b3-0x1000b9c8); with bit 3 of the byte at g_waveDevice+0x6c (0x1000b9d2) 0x100b49fc and 0x100b4a00 are pushed to the same
// call with no zero test and not cleared (0x1000b9d8-0x1000b9f0); the reloaded g_waveDevice, when non-NULL, gets `call 0x10001b81`
// (thunk to DirectSoundDevice::close 0x10011290) and operator delete (0x1000b9fb-0x1000ba07); the global is cleared (0x1000ba0f). A
// non-zero g_waveStreamDev 0x100b49f4 gets `call 0x10001a32` (thunk to freeDynArray 0x10006ec0), operator delete and is cleared
// (0x1000ba19-0x1000ba33). Last, with bit 5 of this+0x1a4 (0x1000ba3d-0x1000ba47), `call 0x10002360` (thunk to
// SoundWindowObj_unsubclass 0x100287c0) with ecx = 0x100b49b8 (0x1000ba49 / 0x1000ba4e).
typedef unsigned(__fastcall* WdStop_t)(void*, void*);
WdStop_t WdStop_orig;
unsigned __fastcall WdStop_re(void* self, void*) {
    char* s = static_cast<char*>(self);
    void* t = fat<void*>(s, 0x1a8);                                           // 0x1000b964
    if (t) {                                                                  // 0x1000b96a / 0x1000b96c
        c3ew_th0(0x000026fd, t);                                              // 0x1000b96e
        c3ew_th1(0x00001ed3, c3ew_g<void*>(kGSoundTimer), c3ew_u(s));         // 0x1000b973-0x1000b97a
    }
    if (fat<unsigned char>(s, 0x1a4) & 0x20) {                                // 0x1000b97f / 0x1000b986
        unsigned char& mb = c3ew_g<unsigned char>(kGMidiByte);                // 0x1000b988
        mb = static_cast<unsigned char>((mb & 0xfe) ^ 1);                     // 0x1000b98d-0x1000b991
    } else {
        c3ew_th0(0x00002757, c3ew_sound(0x000b5038));                         // 0x1000b998 / 0x1000b99d
    }
    c3ew_th0(0x0000188e, s);                                                  // 0x1000b9a2 / 0x1000b9a4
    void* d = c3ew_g<void*>(kGWaveDevice);                                    // 0x1000b9a9
    if (d) {                                                                  // 0x1000b9af / 0x1000b9b1
        unsigned b = c3ew_g<unsigned>(kGBuf49f8);                             // 0x1000b9b3
        if (b) {                                                              // 0x1000b9b8 / 0x1000b9ba
            c3ew_th1(0x00001677, d, b);                                       // 0x1000b9bc / 0x1000b9bd
            d = c3ew_g<void*>(kGWaveDevice);                                  // 0x1000b9c2
            c3ew_g<unsigned>(kGBuf49f8) = 0;                                  // 0x1000b9c8
        }
        if (fat<unsigned char>(d, 0x6c) & 8) {                                // 0x1000b9d2 / 0x1000b9d6
            c3ew_th1(0x00001677, d, c3ew_g<unsigned>(kGBuf49fc));             // 0x1000b9d8-0x1000b9df
            unsigned b2 = c3ew_g<unsigned>(kGBuf4a00);                        // 0x1000b9e4
            c3ew_th1(0x00001677, c3ew_g<void*>(kGWaveDevice), b2);            // 0x1000b9e9-0x1000b9f0
            d = c3ew_g<void*>(kGWaveDevice);                                  // 0x1000b9f5
        }
        if (d) {                                                              // 0x1000b9fb-0x1000b9ff
            c3ew_th0(0x00001b81, d);                                          // 0x1000ba01
            c3ew_delete(d);                                                   // 0x1000ba06 / 0x1000ba07
        }
        c3ew_g<unsigned>(kGWaveDevice) = 0;                                   // 0x1000ba0f
    }
    void* st = c3ew_g<void*>(kGWaveStreamDev);                                // 0x1000ba19
    if (st) {                                                                 // 0x1000ba1f / 0x1000ba21
        c3ew_th0(0x00001a32, st);                                             // 0x1000ba23 / 0x1000ba25
        c3ew_delete(st);                                                      // 0x1000ba2a / 0x1000ba2b
        c3ew_g<unsigned>(kGWaveStreamDev) = 0;                                // 0x1000ba33
    }
    if (fat<unsigned char>(s, 0x1a4) & 0x20)                                  // 0x1000ba3d-0x1000ba47
        c3ew_th0(0x00002360, c3ew_sound(0x000b49b8));                         // 0x1000ba49 / 0x1000ba4e
    return 0;                                                                 // 0x1000ba53
}

// 0x0000c020  Wave_Device::restart (VA 0x1000c020). __thiscall, one stack argument, the device index (`ret 4`). A NULL g_waveDevice
// 0x100b49f0 returns 3 (0x1000c023-0x1000c02f); an index not below the dword at g_waveDevice+0x148 (unsigned, `jb` 0x1000c044) returns
// 0xa. With this+0x1a8 non-zero (0x1000c050-0x1000c058): `call 0x10001ed3` (SoundTimer::unregisterClient) on g_soundTimer 0x100b49ec
// with this pushed, then `call 0x100026fd` (SoundTimer::killEvent) on it (0x1000c05a-0x1000c06c). A non-zero 0x100b49f8 is pushed to
// `call 0x10001677` (DirectSoundDevice::removeBuffer) on g_waveDevice and cleared (0x1000c077-0x1000c08c). Then on g_waveDevice (reloaded
// before each): `call 0x1000245f` (DirectSoundDevice::releaseAllBuffers 0x100133d0, 0x1000c096), `call 0x1000104b`
// (DirectSoundDevice::releaseAll 0x10011970, 0x1000c0a1), `call 0x1000192e` (seekListToIndex 0x100116a0) with the index (0x1000c0ad):
// non-zero returns 6 (0x1000c0b2-0x1000c0b7); `call 0x1000180c` (DirectSoundDevice::create 0x10011c60) with (the module word 0x100b4a3c,
// 0) (0x1000c0c0-0x1000c0ce): non-zero is returned (0x1000c0d5 -> 0x1000c1b4). `call 0x10001046` (DirectSoundDevice::createBuffer
// 0x10012d10) with (&0x100b49f8, 0x10088, 0x20000 when bit 5 of the byte at g_waveDevice+0x10 is set, else 0x10000)
// (0x1000c0db-0x1000c0fc): non-zero runs releaseAll again (0x1000c19a-0x1000c1a0), clears 0x100b49f8 and returns 0x1e (0x1000c1a5-
// 0x1000c1af). On success: this's vtable slot +0x3c with 0xf (0x1000c109-0x1000c10f), slot +0x40 with 0xe (0x1000c112-0x1000c118),
// whose eax is pushed to `call 0x1000182f` (DsBuffer::setRegionMs 0x10010eb0) on 0x100b49f8 (0x1000c11b-0x1000c122); `call 0x10001c5d`
// (DsBuffer::lockEntire 0x1000f810) and `call 0x10001203` (DsBuffer::play) with 1 on it (0x1000c127-0x1000c13a). The buffer's dword +0x38
// is divided (unsigned) by its word +0xb4, 0 when the word is 0 (0x1000c13f-0x1000c15f), and twice that goes to `call 0x10001311`
// (resizeDynArray 0x10006f00) on g_waveStreamDev 0x100b49f4 (0x1000c161-0x1000c16b). Last, this+0x1a8 = g_soundTimer (0x1000c170 /
// 0x1000c177), registerClient with this and SoundTimer::start on g_soundTimer (0x1000c17d-0x1000c18e); 0 is returned (0x1000c194).
typedef unsigned(__fastcall* WdRestart_t)(void*, void*, unsigned);
WdRestart_t WdRestart_orig;
unsigned __fastcall WdRestart_re(void* self, void*, unsigned idx) {
    char* s = static_cast<char*>(self);
    void* d = c3ew_g<void*>(kGWaveDevice);                                    // 0x1000c023
    if (!d) return 3;                                                         // 0x1000c02a-0x1000c02f
    if (!(idx < fat<unsigned>(d, 0x148))) return 0xa;                         // 0x1000c03c-0x1000c047
    if (fat<unsigned>(s, 0x1a8) != 0) {                                       // 0x1000c050-0x1000c058
        c3ew_th1(0x00001ed3, c3ew_g<void*>(kGSoundTimer), c3ew_u(s));         // 0x1000c05a-0x1000c061
        c3ew_th0(0x000026fd, c3ew_g<void*>(kGSoundTimer));                    // 0x1000c066 / 0x1000c06c
        d = c3ew_g<void*>(kGWaveDevice);                                      // 0x1000c071
    }
    unsigned b = c3ew_g<unsigned>(kGBuf49f8);                                 // 0x1000c077
    if (b) {                                                                  // 0x1000c07c / 0x1000c07e
        c3ew_th1(0x00001677, d, b);                                           // 0x1000c080 / 0x1000c081
        d = c3ew_g<void*>(kGWaveDevice);                                      // 0x1000c086
        c3ew_g<unsigned>(kGBuf49f8) = 0;                                      // 0x1000c08c
    }
    c3ew_th0(0x0000245f, d);                                                  // 0x1000c096
    c3ew_th0(0x0000104b, c3ew_g<void*>(kGWaveDevice));                        // 0x1000c09b / 0x1000c0a1
    if (c3ew_th1(0x0000192e, c3ew_g<void*>(kGWaveDevice), idx)) return 6;     // 0x1000c0a6-0x1000c0b7
    unsigned r = c3ew_th2(0x0000180c, c3ew_g<void*>(kGWaveDevice), c3ew_g<unsigned>(kGDsCoop), 0);   // 0x1000c0c0-0x1000c0ce
    if (r) return r;                                                          // 0x1000c0d3 / 0x1000c0d5
    void* d2 = c3ew_g<void*>(kGWaveDevice);                                   // 0x1000c0db
    unsigned size = (fat<unsigned char>(d2, 0x10) & 0x20) ? 0x20000u : 0x10000u;   // 0x1000c0e1-0x1000c0ec
    r = c3ew_th3(0x00001046, d2, c3ew_u(c3ew_sound(kGBuf49f8)), 0x10088, size);    // 0x1000c0f1-0x1000c0fc
    if (r) {                                                                  // 0x1000c101 / 0x1000c103
        c3ew_th0(0x0000104b, c3ew_g<void*>(kGWaveDevice));                    // 0x1000c19a / 0x1000c1a0
        c3ew_g<unsigned>(kGBuf49f8) = 0;                                      // 0x1000c1a5
        return 0x1e;                                                          // 0x1000c1af
    }
    c3ew_vslot<Th1_t>(s, 0x3c)(s, 0, 0xf);                                    // 0x1000c109-0x1000c10f
    unsigned ms = c3ew_vslot<Th1_t>(s, 0x40)(s, 0, 0xe);                      // 0x1000c112-0x1000c118
    c3ew_th1(0x0000182f, c3ew_g<void*>(kGBuf49f8), ms);                       // 0x1000c11b-0x1000c122
    c3ew_th0(0x00001c5d, c3ew_g<void*>(kGBuf49f8));                           // 0x1000c127 / 0x1000c12d
    c3ew_th1(0x00001203, c3ew_g<void*>(kGBuf49f8), 1);                        // 0x1000c132-0x1000c13a
    char* buf = c3ew_g<char*>(kGBuf49f8);                                     // 0x1000c13f
    unsigned short w = fat<unsigned short>(buf, 0xb4);                        // 0x1000c144
    unsigned q = w ? fat<unsigned>(buf, 0x38) / w : 0;                        // 0x1000c14b-0x1000c15f
    c3ew_th1(0x00001311, c3ew_g<void*>(kGWaveStreamDev), q + q);              // 0x1000c161-0x1000c16b
    fat<void*>(s, 0x1a8) = c3ew_g<void*>(kGSoundTimer);                       // 0x1000c170 / 0x1000c177
    c3ew_th1(0x0000227a, c3ew_g<void*>(kGSoundTimer), c3ew_u(s));             // 0x1000c17d-0x1000c183
    c3ew_th0(0x00001dfc, c3ew_g<void*>(kGSoundTimer));                        // 0x1000c188 / 0x1000c18e
    return 0;                                                                 // 0x1000c194
}

// =====================================================================================================================
// D. the sound timer and a timed channel start
// =====================================================================================================================

// 0x000089f0  SoundTimer::init (VA 0x100089f0). __thiscall, `ret`, returns this (0x10008a84). +0x4c / +0x50 / +0x54 / +0x58 = 0 and
// +0x3c = 0 (0x100089ff-0x10008a0c); timeGetDevCaps [0x100b8650] (this+0x44, 8) (0x100089f7-0x10008a0e); the dword it filled at +0x44
// (the minimum period) is copied to +0x18 (0x10008a14 / 0x10008a1b); +4 / +8 / +0xc / +0x14 = 0, +0x1c = 1, +0x24 / +0x40 / +0x20 = 0,
// word +0x28 = 1, word +0x38 = 0 (0x10008a1e-0x10008a3a); +0 = 0x1e, word +0x2a = 2, +0x2c = 0xac44, +0x30 = 0x2b110, word +0x34 = 4,
// word +0x36 = 0x10, +0x10 = 0x52b (0x10008a4a-0x10008a70); the 24 bytes of the module's critical section 0x100b4990 are zeroed
// (`rep stosd`, 6 dwords, 0x10008a3e-0x10008a7c) and InitializeCriticalSection [0x100b83f0] runs on it (0x10008a77 / 0x10008a7e).
typedef void*(__fastcall* StInit_t)(void*, void*);
StInit_t StInit_orig;
void* __fastcall StInit_re(void* self, void*) {
    char* s = static_cast<char*>(self);
    fat<unsigned>(s, 0x4c) = 0;                                               // 0x100089ff
    fat<unsigned>(s, 0x50) = 0;                                               // 0x10008a02
    fat<unsigned>(s, 0x54) = 0;                                               // 0x10008a05
    fat<unsigned>(s, 0x58) = 0;                                               // 0x10008a08
    fat<unsigned>(s, 0x3c) = 0;                                               // 0x10008a0c
    c3ew_imp<ImpPU_t>(kIatTimeGetDevCaps)(s + 0x44, 8);                       // 0x100089f7-0x10008a0e
    fat<unsigned>(s, 0x18) = fat<unsigned>(s, 0x44);                          // 0x10008a14 / 0x10008a1b
    fat<unsigned>(s, 4) = 0;                                                  // 0x10008a1e
    fat<unsigned>(s, 8) = 0;                                                  // 0x10008a21
    fat<unsigned>(s, 0xc) = 0;                                                // 0x10008a24
    fat<unsigned>(s, 0x14) = 0;                                               // 0x10008a27
    fat<unsigned>(s, 0x1c) = 1;                                               // 0x10008a2a
    fat<unsigned>(s, 0x24) = 0;                                               // 0x10008a2d
    fat<unsigned>(s, 0x40) = 0;                                               // 0x10008a30
    fat<unsigned>(s, 0x20) = 0;                                               // 0x10008a33
    fat<unsigned short>(s, 0x28) = 1;                                         // 0x10008a36
    fat<unsigned short>(s, 0x38) = 0;                                         // 0x10008a3a
    fat<unsigned>(s, 0) = 0x1e;                                               // 0x10008a4a
    fat<unsigned short>(s, 0x2a) = 2;                                         // 0x10008a50
    fat<unsigned>(s, 0x2c) = 0xac44;                                          // 0x10008a56
    fat<unsigned>(s, 0x30) = 0x2b110;                                         // 0x10008a5d
    fat<unsigned short>(s, 0x34) = 4;                                         // 0x10008a64
    fat<unsigned short>(s, 0x36) = 0x10;                                      // 0x10008a6a
    fat<unsigned>(s, 0x10) = 0x52b;                                           // 0x10008a70
    unsigned* cs = reinterpret_cast<unsigned*>(c3ew_sound(kTimerCS));
    for (int i = 0; i < 6; i++) cs[i] = 0;                                    // 0x10008a3e-0x10008a7c
    c3ew_imp<ImpP_t>(kIatInitCS)(cs);                                         // 0x10008a77 / 0x10008a7e
    return self;                                                              // 0x10008a84
}

// 0x00008ac0  SoundTimer::stop (VA 0x10008ac0). __thiscall, `ret`. The module word 0x100b49a8 is incremented (0x10008ac0-0x10008acd).
// With bits 0 and 1 of the dword at +0x3c both set (0x10008ad3-0x10008add): the word is incremented again (0x10008adf-0x10008ae7), bit 2
// of +0x3c cleared (0x10008aed-0x10008af3), EnterCriticalSection [0x100b83f4] then LeaveCriticalSection [0x100b84e0] on 0x100b4990
// (0x10008ae1-0x10008b01), +8 = 0 and +4 = 0 (0x10008b0a / 0x10008b0e), timeKillEvent [0x100b8648] (+0x40) (0x10008b07-0x10008b11),
// timeEndPeriod [0x100b864c] (+0x18) (0x10008b17-0x10008b1b), bit 0 of +0x3c cleared (0x10008b21-0x10008b26). Then on every path
// DeleteCriticalSection [0x100b83e4] on 0x100b4990 (0x10008b29 / 0x10008b2e) and its 24 bytes zeroed (0x10008b34-0x10008b40). The client
// list at +0x4c (front) / +0x50 (back) / +0x58 (count) is popped from the front, each node deleted, until it is empty (0x10008b47 /
// 0x10008b79; eax is then the 0 just loaded) or a popped node's value is NULL (0x10008b72 / 0x10008ba4; eax is then operator delete's,
// which the CRT free leaves undefined). The only caller, release_sound 0x10009390, discards eax (0x100093ac-0x100093c4).
typedef unsigned(__fastcall* StStop_t)(void*, void*);
StStop_t StStop_orig;
unsigned __fastcall StStop_re(void* self, void*) {
    char* s = static_cast<char*>(self);
    unsigned w = c3ew_g<unsigned>(kGTickWord) + 1;                            // 0x10008ac0 / 0x10008aca
    c3ew_g<unsigned>(kGTickWord) = w;                                         // 0x10008acd
    void* cs = c3ew_sound(kTimerCS);
    unsigned f = fat<unsigned>(s, 0x3c);                                      // 0x10008ad3
    if ((f & 1) && (f & 2)) {                                                 // 0x10008ad6-0x10008add
        c3ew_g<unsigned>(kGTickWord) = w + 1;                                 // 0x10008adf-0x10008ae7
        fat<unsigned>(s, 0x3c) &= 0xfffffffbu;                                // 0x10008aed-0x10008af3
        c3ew_imp<ImpP_t>(kIatEnterCS)(cs);                                    // 0x10008af6
        c3ew_imp<ImpP_t>(kIatLeaveCS)(cs);                                    // 0x10008b01
        unsigned id = fat<unsigned>(s, 0x40);                                 // 0x10008b07
        fat<unsigned>(s, 8) = 0;                                              // 0x10008b0a
        fat<unsigned>(s, 4) = 0;                                              // 0x10008b0e
        c3ew_imp<Imp1_t>(kIatTimeKillEvent)(id);                              // 0x10008b11
        c3ew_imp<Imp1_t>(kIatTimeEndPeriod)(fat<unsigned>(s, 0x18));          // 0x10008b17-0x10008b1b
        fat<unsigned>(s, 0x3c) &= 0xfffffffeu;                                // 0x10008b21-0x10008b26
    }
    c3ew_imp<ImpP_t>(kIatDeleteCS)(cs);                                       // 0x10008b29 / 0x10008b2e
    for (int i = 0; i < 6; i++) static_cast<unsigned*>(cs)[i] = 0;            // 0x10008b34-0x10008b40
    unsigned dr = 0;
    char* node = fat<char*>(s, 0x4c);                                         // 0x10008b42
    while (node) {                                                            // 0x10008b45 / 0x10008b77
        void* v = c3ew_popFront(s, 0x4c, 0x50, 0x58, node, &dr);              // 0x10008b49-0x10008b6f / 0x10008b7b-0x10008ba1
        if (!v) return dr;                                                    // 0x10008b72 / 0x10008ba4
        node = fat<char*>(s, 0x4c);                                           // 0x10008b74
    }
    return 0;                                                                 // eax = the NULL front (0x10008b42 / 0x10008b74)
}

// 0x00039e70  Channel_startVoiceTimed (VA 0x10039e70). __thiscall, one stack argument (`ret 4`). Bit 0 of the dword at +0xc8 clear
// returns 0xb (0x10039e73-0x10039e7d). With bit 1 clear (0x10039e86): `call 0x10001866` (thunk to Channel_resetPanState 0x10033ba0) on
// this+0x70 (0x10039e8a / 0x10039e8d); bit 1 of the byte at +0x58 clear returns 0x14 (0x10039e92-0x10039eb0); otherwise this's vtable
// slot +0x10 with the dword at +0x50 (0x10039e98-0x10039ea0): non-zero returns 8 (0x10039ea3-0x10039ea7). Then a non-zero g_soundTimer
// 0x100b49ec gets `call 0x10001dfc` (SoundTimer::start) (0x10039eb9-0x10039ec3); `call 0x10001212` (thunk to Channel_startVoice
// 0x100348f0) on this+0x70 with the argument (0x10039ec8-0x10039ed3). Bit 6 of the byte at +0xc8 clear (0x10039ed8-0x10039ee5): the
// dword at (this+0x70)+0x58, i.e. +0xc8, gets bit 6 (0x10039ee7-0x10039eec) and this's slot +0x7c runs (0x10039eef-0x10039ef3). Returns 0.
typedef unsigned(__fastcall* ChStartTimed_t)(void*, void*, unsigned);
ChStartTimed_t ChStartTimed_orig;
unsigned __fastcall ChStartTimed_re(void* self, void*, unsigned arg) {
    char* s = static_cast<char*>(self);
    unsigned f = fat<unsigned>(s, 0xc8);                                      // 0x10039e73
    if (!(f & 1)) return 0xb;                                                 // 0x10039e79-0x10039e7d
    if (!(f & 2)) {                                                           // 0x10039e86 / 0x10039e88
        c3ew_th0(0x00001866, s + 0x70);                                       // 0x10039e8a / 0x10039e8d
        if (!(fat<unsigned char>(s, 0x58) & 2)) return 0x14;                  // 0x10039e92-0x10039eb0
        if (c3ew_vslot<Th1_t>(s, 0x10)(s, 0, fat<unsigned>(s, 0x50))) return 8;   // 0x10039e98-0x10039ea7
    }
    void* t = c3ew_g<void*>(kGSoundTimer);                                    // 0x10039eb9
    if (t) c3ew_th0(0x00001dfc, t);                                           // 0x10039ebf-0x10039ec3
    char* v = s + 0x70;                                                       // 0x10039ecd
    c3ew_th1(0x00001212, v, arg);                                             // 0x10039ec8-0x10039ed3
    if (!(fat<unsigned char>(s, 0xc8) & 0x40)) {                              // 0x10039ed8-0x10039ee5
        fat<unsigned>(v, 0x58) |= 0x40;                                       // 0x10039ee7-0x10039eec
        c3ew_vslot<Th0_t>(s, 0x7c)(s, 0);                                     // 0x10039eef-0x10039ef3
    }
    return 0;                                                                 // 0x10039ef7
}

// =====================================================================================================================
// E. a command enqueue over g_soundEngine
// =====================================================================================================================

// 0x0002fc10  enqueueWaveCmd10 (VA 0x1002fc10). __thiscall, `ret`. The C++ frame (0x1002fc10-0x1002fc1e) only guards the pairStore
// call. A NULL g_soundEngine 0x100b4a20 returns 1 (0x1002fc26-0x1002fc32). Otherwise a non-zero dword at +0xf8 (0x1002fc47-0x1002fc55)
// gets the address this+0xf8 at its +0x1414 (0x1002fc57); operator new(0x10) (0x1002fc5d / 0x1002fc5f); a non-NULL block goes to
// `call 0x10001299` (thunk to pairStore 0x10008810) with (0x10, this) (0x1002fc77-0x1002fc7c), whose eax is the value, a NULL block
// gives 0 (0x1002fc83); the value is pushed to `call 0x100011c7` (thunk to WaveCmdQueue::pushValue 0x1000c3d0) on the reloaded
// g_soundEngine (0x1002fc85-0x1002fc94). Returns 0 (0x1002fc9d).
typedef unsigned(__fastcall* EnqCmd10_t)(void*, void*);
EnqCmd10_t EnqCmd10_orig;
unsigned __fastcall EnqCmd10_re(void* self, void*) {
    char* s = static_cast<char*>(self);
    if (!c3ew_g<void*>(kGSoundEngine)) return 1;                              // 0x1002fc26-0x1002fc32
    char* q = fat<char*>(s, 0xf8);                                            // 0x1002fc47
    if (q) fat<void*>(q, 0x1414) = s + 0xf8;                                  // 0x1002fc4d-0x1002fc57
    void* p = c3ew_new(0x10);                                                 // 0x1002fc5d / 0x1002fc5f
    unsigned v = p ? c3ew_th2(0x00001299, p, 0x10, c3ew_u(s)) : 0;            // 0x1002fc6b-0x1002fc83
    c3ew_th1(0x000011c7, c3ew_g<void*>(kGSoundEngine), v);                    // 0x1002fc85-0x1002fc94
    return 0;                                                                 // 0x1002fc9d
}

// =====================================================================================================================
// F. sequencer command handlers (a track-list search then a thunked call)
// =====================================================================================================================

// 0x00023540  Seq_cmdHandler8 (VA 0x10023540). __thiscall, one stack argument, a track id (`ret 4`). A zero dword at this+0x224
// returns 0x24 (0x10023544-0x1002354f). The id equal to this+0x738 returns 0 (0x1002355c-0x10023567). The track list front is
// this+0x218 (0x1002356d), kept in the cursor this+0x220; each node's value is node+8; the walk follows the cursor's +4 link
// (0x10023593) and stops when the cursor or a value is NULL (returns 0x24) or when a value's +4 equals the id
// (0x10023584-0x10023587). On a match: a non-zero value+0x1c is pushed to this's vtable slot +0x84 (0x100235b7-0x100235c3);
// then `call 0x10001519` (thunk to Seq_remarkEventRange 0x10023610) with ecx = this and the value pushed (0x100235c9-0x100235cc),
// whose eax is returned.
typedef unsigned(__fastcall* SeqH8_t)(void*, void*, unsigned);
SeqH8_t SeqH8_orig;
unsigned __fastcall SeqH8_re(void* self, void*, unsigned id) {
    char* s = static_cast<char*>(self);
    if (fat<unsigned>(s, 0x224) == 0) return 0x24;                            // 0x10023544-0x1002354f
    if (fat<unsigned>(s, 0x738) == id) return 0;                             // 0x1002355c-0x10023567
    char* front = fat<char*>(s, 0x218);                                       // 0x1002356d
    fat<char*>(s, 0x220) = front;                                            // 0x10023575
    if (!front) return 0x24;                                                  // 0x1002357b / 0x100235ad
    char* val = fat<char*>(front, 8);                                         // 0x1002357d
    if (!val) return 0x24;                                                    // 0x10023580 / 0x10023582
    for (;;) {
        if (fat<unsigned>(val, 4) == id) break;                             // 0x10023584-0x10023587 (match)
        char* cur = fat<char*>(s, 0x220);                                    // 0x10023589
        if (!cur) return 0x24;                                               // 0x1002358f / 0x10023591
        cur = fat<char*>(cur, 4);                                            // 0x10023593
        fat<char*>(s, 0x220) = cur;                                          // 0x10023598
        if (!cur) return 0x24;                                               // 0x1002359e
        val = fat<char*>(cur, 8);                                            // 0x100235a0
        if (!val) return 0x24;                                               // 0x100235a3 / 0x100235a5
    }
    // 0x100235a9: the matched value is non-NULL by the checks above
    if (fat<unsigned>(val, 0x1c) != 0)                                        // 0x100235b7-0x100235bc
        c3ew_vslot<Th1_t>(s, 0x84)(s, 0, fat<unsigned>(val, 0x1c));          // 0x100235be-0x100235c3
    return c3ew_th1(0x00001519, s, c3ew_u(val));                             // 0x100235c9-0x100235cc
}

// 0x000238b0  Seq_cmdHandler9 (VA 0x100238b0). __thiscall, one stack argument, a track id (`ret 4`). A zero dword at this+0x224
// returns 0x24 (0x100238b0-0x100238bf). The track list front is this+0x218 kept in the cursor this+0x220 (0x100238c2-0x100238ca);
// each node's value is node+8; the walk follows the cursor's +4 link and stops when the cursor or a value is NULL (returns 0x24)
// or when a value's +4 equals the id (0x100238dd-0x100238e0). On a match (value non-NULL, 0x10023902): value+0x34 = 0 and
// `call 0x10001c3f` (thunk to Seq_replayEventRange 0x10023950) with (value+8, value+0xc) (0x1002390e-0x1002391d), whose eax is
// returned.
typedef unsigned(__fastcall* SeqH9_t)(void*, void*, unsigned);
SeqH9_t SeqH9_orig;
unsigned __fastcall SeqH9_re(void* self, void*, unsigned id) {
    char* s = static_cast<char*>(self);
    if (fat<unsigned>(s, 0x224) == 0) return 0x24;                            // 0x100238b0-0x100238bf
    char* node = fat<char*>(s, 0x218);                                        // 0x100238c2
    fat<char*>(s, 0x220) = node;                                             // 0x100238ca
    if (!node) return 0x24;                                                   // 0x100238d0 / 0x10023906
    char* val = fat<char*>(node, 8);                                          // 0x100238d2
    if (!val) return 0x24;                                                    // 0x100238d7
    while (fat<unsigned>(val, 4) != id) {                                     // 0x100238dd-0x100238e0
        node = fat<char*>(s, 0x220);                                         // 0x100238e2
        if (!node) return 0x24;                                              // 0x100238e8 / 0x10023906
        node = fat<char*>(node, 4);                                          // 0x100238ec
        fat<char*>(s, 0x220) = node;                                         // 0x100238f1
        if (!node) return 0x24;                                              // 0x100238f7
        val = fat<char*>(node, 8);                                           // 0x100238f9
        if (!val) return 0x24;                                               // 0x100238fe
    }
    if (!val) return 0x24;                                                    // 0x10023902-0x10023906
    fat<unsigned>(val, 0x34) = 0;                                             // 0x10023911
    // ecx is unchanged from entry, so this = the sequencer (0x100238b0 onward never writes ecx)
    return c3ew_th2(0x00001c3f, s, fat<unsigned>(val, 8), fat<unsigned>(val, 0xc));   // 0x1002390e-0x1002391d
}

// =====================================================================================================================
// G. Seq_allocTrackSlot: find a free track slot by the module round-robin counter, then schedule
// =====================================================================================================================

// 0x000247c0  Seq_allocTrackSlot (VA 0x100247c0). __thiscall, `ret`. A zero dword at this+0x23c returns it (0, 0x100247c4-
// 0x100247cc -> 0x10024888). Otherwise the module counter 0x100b5010 is incremented and wrapped to 0 when it reaches this+0x23c
// (0x100247d2-0x100247ee); the track index table this+0x238 is read at [counter] (0x100247f9) and Seq_listGetAt (thunk 0x100015fa)
// on this+0x60 writes the track to a local out slot and returns 0 when found (0x10024800-0x10024809). On a found track (`jne`
// 0x1002480b): with this+0x23c > 0 (signed, `jle` 0x1002481a) it loops ebp = 1 .. +0x23c - 1: if the out track's +0x38 bit 2 is
// clear it is set and the scan stops (0x1002481c-0x1002486b); otherwise the counter advances, Seq_listGetAt runs again, a found
// result stops the scan (`jne` 0x1002485a) and the index wraps at +0x23c (`jl` 0x10024867). Finally Seq_scheduleEvent (thunk
// 0x1000240a) is called with (this; the out track, this+0x71c, 0x20, 0) (0x1002486f-0x10024881) and its eax returned. Hooked
// callees are stubbed at their thunks (Seq_listGetAt is c3au C3, Seq_scheduleEvent is c3bi C3).
typedef unsigned(__fastcall* SeqAlloc_t)(void*, void*);
SeqAlloc_t SeqAlloc_orig;
unsigned __fastcall SeqAlloc_re(void* self, void*) {
    char* s = static_cast<char*>(self);
    int count = fat<int>(s, 0x23c);                                           // 0x100247c4
    if (count == 0) return static_cast<unsigned>(count);                      // 0x100247ca -> 0x10024888 (eax = [+0x23c] = 0)
    unsigned& ctr = c3ew_g<unsigned>(0x000b5010);
    unsigned c = ctr + 1;                                                     // 0x100247d2-0x100247d8
    if (!(static_cast<int>(c) < fat<int>(s, 0x23c))) c = 0;                   // 0x100247e3 / 0x100247e5 / 0x100247e7-0x100247e9
    ctr = c;
    unsigned idx = *reinterpret_cast<unsigned*>(fat<char*>(s, 0x238) + c * 4);   // 0x100247ee / 0x100247f9
    void* track = 0;                                                          // the local out slot [esp+0xc]
    unsigned r = c3ew_th2(0x000015fa, s + 0x60, reinterpret_cast<unsigned>(&track), idx);   // 0x10024804
    if (r == 0) {                                                             // 0x10024809 / 0x1002480b
        if (fat<int>(s, 0x23c) > 0) {                                         // 0x1002480d / 0x1002481a
            int ebp = 1;                                                      // 0x10024813
            for (;;) {
                void* t = track;                                             // 0x1002481c
                if (!(fat<unsigned char>(t, 0x38) & 4)) {                     // 0x10024820 / 0x10024824
                    fat<unsigned char>(t, 0x38) |= 4;                         // 0x1002486b
                    break;
                }
                c = ctr + 1;                                                  // 0x10024826-0x1002482c
                if (!(static_cast<int>(c) < fat<int>(s, 0x23c))) c = 0;       // 0x10024837-0x1002483d
                ctr = c;
                idx = *reinterpret_cast<unsigned*>(fat<char*>(s, 0x238) + c * 4);   // 0x10024842 / 0x1002484c
                r = c3ew_th2(0x000015fa, s + 0x60, reinterpret_cast<unsigned>(&track), idx);   // 0x10024853
                if (r != 0) break;                                           // 0x10024858 / 0x1002485a
                int eax = ebp;                                               // 0x10024862
                ebp++;                                                        // 0x10024864
                if (!(eax < fat<int>(s, 0x23c))) break;                       // 0x10024865 / 0x10024867
            }
        }
    }
    return c3ew_th4(0x0000240a, s, reinterpret_cast<unsigned>(track), fat<unsigned>(s, 0x71c), 0x20, 0);   // 0x1002487f
}

}  // namespace

SG_HOOK("sound.dll", 0x0002a240, WaveInDeviceBase_ctor, WiBaseCtor_re, WiBaseCtor_orig);
SG_HOOK("sound.dll", 0x0002cee0, MixBuffer_ctor, MixBufCtor_re, MixBufCtor_orig);
SG_HOOK("sound.dll", 0x0000ec00, Wave_In_Device_closeCapture, WiCloseCap_re, WiCloseCap_orig);
SG_HOOK("sound.dll", 0x0000ea30, Wave_In_Device_start, WiStartDev_re, WiStartDev_orig);
SG_HOOK("sound.dll", 0x0000ead0, Wave_In_Device_openCapture, WiOpenCap_re, WiOpenCap_orig);
SG_HOOK("sound.dll", 0x000319e0, WaveInStream_resetCodec, WsResetCodec_re, WsResetCodec_orig);
SG_HOOK("sound.dll", 0x00031560, freeMmioHandleListGetInfo, FreeMmioGI_re, FreeMmioGI_orig);
SG_HOOK("sound.dll", 0x0000bf90, Wave_Device_startOutput, WdStartOut_re, WdStartOut_orig);
SG_HOOK("sound.dll", 0x0000bef0, Wave_Device_stopOutput, WdStopOut_re, WdStopOut_orig);
SG_HOOK("sound.dll", 0x0000be00, Wave_Device_closeOutput, WdCloseOut_re, WdCloseOut_orig);
SG_HOOK("sound.dll", 0x0000b1d0, Wave_Device_ctor, WdCtor_re, WdCtor_orig);
SG_HOOK("sound.dll", 0x0000b2e0, Wave_Device_dtor, WdDtor_re, WdDtor_orig);
SG_HOOK("sound.dll", 0x0000b960, Wave_Device_stop, WdStop_re, WdStop_orig);
SG_HOOK("sound.dll", 0x0000c020, Wave_Device_restart, WdRestart_re, WdRestart_orig);
SG_HOOK("sound.dll", 0x000089f0, SoundTimer_init, StInit_re, StInit_orig);
SG_HOOK("sound.dll", 0x00008ac0, SoundTimer_stop, StStop_re, StStop_orig);
SG_HOOK("sound.dll", 0x00039e70, Channel_startVoiceTimed, ChStartTimed_re, ChStartTimed_orig);
SG_HOOK("sound.dll", 0x0002fc10, enqueueWaveCmd10, EnqCmd10_re, EnqCmd10_orig);
SG_HOOK("sound.dll", 0x00023540, Seq_cmdHandler8, SeqH8_re, SeqH8_orig);
SG_HOOK("sound.dll", 0x000238b0, Seq_cmdHandler9, SeqH9_re, SeqH9_orig);
SG_HOOK("sound.dll", 0x000247c0, Seq_allocTrackSlot, SeqAlloc_re, SeqAlloc_orig);

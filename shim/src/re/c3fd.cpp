// C3 batch c3fd (round 33, 2026-10-13) of sound.dll: the size-only and never-tried rows of the open C2 pool, the
// GetProcAddress exports init_sound_timer / Dll_*::create_device / delete_device, and the MultiSound constructors whose
// callee 0x10031be0 had no hooks.csv row (log/c3/c3fd_candidates.tsv, log/c3/round33_brief.md).
// Method as c3dt / c3ex (shim/src/re/c3ex.cpp): fixture-owned objects only, module globals the body tests swapped for the key
// with the first thread frozen and the multimedia timer quiet, a thread-filtered recording stub on the incremental-link thunk
// of a callee whose tree reaches the live engine, operator new / delete through heapstandin.js and winmm imports through
// importstandin.js.
// Addresses in SG_HOOK and in the `// 0x...` comment lines are RVAs; the instruction addresses quoted inside each body are
// file VAs (VA = 0x10000000 + RVA). Every fact comes from the disassembly (log/c3/alt/c3fd/dis/<rva>_<name>.txt, dumped by
// log/c3/alt/c3fd_survey.py; jump tables by log/c3/alt/c3fd_dump.py), cited by instruction address.
// sound.dll is LoadLibrary'd after the shim starts (its hooks install from the LoadLibraryA hook) and loads above its
// preferred base, so module globals, thunks and vtable addresses are taken relative to the loaded base.
// __thiscall is emulated with __fastcall (ecx = this, edx unused). Every callee runs through its original address. The C++
// EH frames the originals build (`push -1; push <handler>; mov fs:[0], esp`) only unwind a throwing callee; nothing these
// bodies call throws in the A/B, so the reimplementations build none.
#include "hooks.h"
#include <stdint.h>
#include <string.h>

namespace {

template <typename T> T& fat(void* base, int off) {
    return *reinterpret_cast<T*>(reinterpret_cast<char*>(base) + off);
}

char* c3fd_sound(unsigned rva) {
    static HMODULE m = 0;
    if (!m) m = GetModuleHandleA("sound.dll");
    return reinterpret_cast<char*>(m) + rva;
}
template <typename F> F c3fd_fn(unsigned rva) { return reinterpret_cast<F>(c3fd_sound(rva)); }
void*& c3fd_gref(unsigned rva) { return *reinterpret_cast<void**>(c3fd_sound(rva)); }

typedef unsigned(__fastcall* Th0_t)(void*, void*);
typedef unsigned(__fastcall* Th1_t)(void*, void*, unsigned);
typedef unsigned(__fastcall* Th2_t)(void*, void*, unsigned, unsigned);
typedef unsigned(__fastcall* Th3_t)(void*, void*, unsigned, unsigned, unsigned);
typedef unsigned(__fastcall* Th4_t)(void*, void*, unsigned, unsigned, unsigned, unsigned);
typedef void*(__cdecl* OpNew_t)(unsigned);
typedef void(__cdecl* OpDelete_t)(void*);
typedef unsigned(__cdecl* CreateSound_t)(void**, unsigned, unsigned);
typedef unsigned(__stdcall* Imp2_t)(unsigned, unsigned);

void* c3fd_new(unsigned n) { return c3fd_fn<OpNew_t>(0x000424d7)(n); }       // operator new 0x100424d7
void c3fd_delete(void* p) { c3fd_fn<OpDelete_t>(0x0004249a)(p); }             // operator delete 0x1004249a

// virtual slot `off` of `obj` (`mov eax, [obj]; call [eax + off]`)
template <typename F> F c3fd_vfn(void* obj, int off) {
    return *reinterpret_cast<F*>(*reinterpret_cast<char**>(obj) + off);
}
// the import whose IAT slot is at RVA `slotRva` (`call dword ptr [0x100b....]`)
template <typename F> F c3fd_imp(unsigned slotRva) { return *reinterpret_cast<F*>(c3fd_sound(slotRva)); }
const unsigned kIatMidiOutShortMsg = 0x000b8638;                              // WINMM!midiOutShortMsg [0x100b8638]

// module globals (file VAs 0x100b....)
const unsigned kMidiDevice = 0x000b4a1c;      // g_midiDevice
const unsigned kWaveDevice = 0x000b4a20;      // the Wave_Device create_device stores (g_soundEngine in earlier notes)
const unsigned kWaveInDevice = 0x000b4a24;    // the Wave_In_Device create_device stores
const unsigned kTimerFlags = 0x000b49d8;      // init_sound_timer's argument
const unsigned kSoundTimer = 0x000b49ec;      // g_soundTimer
const unsigned kSoundReady = 0x000b507c;      // the word Engine_playSound tests first

// =====================================================================================================================
// A. GetProcAddress exports (soundVersionCheck 0x00483fc0 stores GetProcAddress(h, k) for ordinals 1..11 at
//    0x0083af6c + 4 * (k - 1), `call ebx` 0x00484008 / `mov [esi], eax` 0x0048400c)
// =====================================================================================================================

// 0x00009bb0  Dll_Midi_Device::delete_device (VA 0x10009bb0), export #8 (thunk 0x10001a5f; read from slot 0x0083af88 by
// closeHookB 0x004879f0, `mov eax, [0x83af88]` 0x004879fa, `call eax` 0x00487a03). __cdecl, no argument, plain `ret`.
// ecx = g_midiDevice 0x100b4a1c (0x10009bb0); when non-NULL (`test` / `je` 0x10009bb6 / 0x10009bb8) its vtable slot 0 runs with
// 1 pushed (`push 1; call [eax]` 0x10009bbc / 0x10009bbe: the scalar deleting destructor). The global is set to 0 on both
// paths (0x10009bc0) and 0 is returned (0x10009bca).
typedef unsigned(__cdecl* DelDev_t)();
DelDev_t DelMidi_orig;
unsigned __cdecl DelMidi_re() {
    void* dev = c3fd_gref(kMidiDevice);                                       // 0x10009bb0
    if (dev) c3fd_vfn<Th1_t>(dev, 0)(dev, 0, 1);                              // 0x10009bb8-0x10009bbe
    c3fd_gref(kMidiDevice) = 0;                                               // 0x10009bc0
    return 0;                                                                 // 0x10009bca
}

// 0x0000b1a0  Dll_Wave_Device::delete_device (VA 0x1000b1a0), export #6 (thunk 0x100011c2; slot 0x0083af80, closeHookA
// 0x00487430 `mov eax, [0x83af80]` 0x0048743a, `call eax` 0x00487443). Same body on 0x100b4a20: load (0x1000b1a0), NULL test
// (0x1000b1a6 / 0x1000b1a8), `push 1; call [eax]` (0x1000b1ac / 0x1000b1ae), the global cleared (0x1000b1b0), 0 returned
// (0x1000b1ba).
DelDev_t DelWave_orig;
unsigned __cdecl DelWave_re() {
    void* dev = c3fd_gref(kWaveDevice);                                       // 0x1000b1a0
    if (dev) c3fd_vfn<Th1_t>(dev, 0)(dev, 0, 1);                              // 0x1000b1a8-0x1000b1ae
    c3fd_gref(kWaveDevice) = 0;                                               // 0x1000b1b0
    return 0;                                                                 // 0x1000b1ba
}

// 0x0000ea00  Dll_Wave_In_Device::delete_device (VA 0x1000ea00), export #10 (thunk 0x100021fd; slot 0x0083af90, closeHookC
// 0x00487bd0 `mov eax, [0x83af90]` 0x00487bda, `call eax` 0x00487be3). Same body on 0x100b4a24 (0x1000ea00-0x1000ea1a).
DelDev_t DelWaveIn_orig;
unsigned __cdecl DelWaveIn_re() {
    void* dev = c3fd_gref(kWaveInDevice);                                     // 0x1000ea00
    if (dev) c3fd_vfn<Th1_t>(dev, 0)(dev, 0, 1);                              // 0x1000ea08-0x1000ea0e
    c3fd_gref(kWaveInDevice) = 0;                                             // 0x1000ea10
    return 0;                                                                 // 0x1000ea1a
}

// 0x000092e0  init_sound_timer (VA 0x100092e0), export #3 (thunk 0x10001c67; slot 0x0083af74, read by callSoundHook74
// 0x00484090, which pushes its own two arguments, 0x004840a7 / 0x004840a8, and `call [0x83af74]` 0x004840a9). __cdecl, two
// arguments of which only the SECOND is read (`mov eax, [esp+0x14]` 0x100092ee after three pushes: return address at +0xc,
// first argument at +0x10); its slot is later reused for the new block (0x1000930c). Plain `ret`. That argument is stored at
// 0x100b49d8 on every path (0x100092fb). With bit 0 set (`test al, 1` 0x100092f9,
// `jne` 0x10009300) nothing else happens. Otherwise operator new(0x5c) (0x10009302 / 0x10009304); a non-NULL block is
// constructed by SoundTimer::init through the thunk 0x100022b1 (ecx = the block, 0x1000931c / 0x1000931e) and its eax kept,
// a NULL block gives 0 (0x10009325). That value is stored in g_soundTimer 0x100b49ec (0x10009327) and the dword at +0x24 of
// it is set to the address of the thunk 0x10002671 (a bare `ret` at 0x10009a70) with no NULL test (0x1000932c), so an
// allocation failure faults there. 0 is returned (0x10009337).
typedef unsigned(__cdecl* InitTimer_t)(unsigned, unsigned);
InitTimer_t InitTimer_orig;
unsigned __cdecl InitTimer_re(unsigned, unsigned flags) {
    *reinterpret_cast<unsigned*>(c3fd_sound(kTimerFlags)) = flags;            // 0x100092fb
    if (!(flags & 1)) {                                                       // 0x100092f9 / 0x10009300
        void* p = c3fd_new(0x5c);                                             // 0x10009304
        void* t = p ? reinterpret_cast<void*>(c3fd_fn<Th0_t>(0x000022b1)(p, 0)) : 0;   // 0x1000931e / 0x10009325
        c3fd_gref(kSoundTimer) = t;                                           // 0x10009327
        fat<void*>(t, 0x24) = c3fd_sound(0x00002671);                         // 0x1000932c
    }
    return 0;                                                                 // 0x10009337
}

// 0x0000b050  Dll_Wave_Device::create_device (VA 0x1000b050), export #5 (thunk 0x10001fcd; slot 0x0083af7c, read at 0x004873fa
// in the open routine at 0x004873f0, which pushes its argument and this+0x14 and calls the slot, 0x00487404-0x0048740a; that
// routine lies outside every function in the master). __cdecl (out, flags); only the first argument is read
// (`mov ecx, [esp+0x14]` 0x1000b093 / 0x1000b0af after four pushes). A non-NULL 0x100b4a20 returns 0xc (`jne` 0x1000b06d,
// 0x1000b0cf). Otherwise operator new(0x41e0) (0x1000b06f / 0x1000b074): a NULL block stores 0 in the global and in *out and
// returns 0 (0x1000b0af-0x1000b0ba); a block is constructed by Wave_Device::ctor through the thunk 0x10001f82 (ecx = block,
// 0x1000b08c / 0x1000b08e), and its eax is stored in the global (0x1000b097) and in *out (0x1000b09c), 0 returned (0x1000b09e).
typedef unsigned(__cdecl* CreateDev_t)(void**, unsigned);
CreateDev_t WaveCreate_orig;
unsigned __cdecl WaveCreate_re(void** out, unsigned) {
    if (c3fd_gref(kWaveDevice)) return 0xc;                                   // 0x1000b06b / 0x1000b06d / 0x1000b0cf
    void* p = c3fd_new(0x41e0);                                               // 0x1000b074
    if (!p) {                                                                 // 0x1000b080 / 0x1000b08a
        c3fd_gref(kWaveDevice) = 0;                                           // 0x1000b0b5
        *out = 0;                                                             // 0x1000b0ba
        return 0;                                                             // 0x1000b0b3
    }
    void* d = reinterpret_cast<void*>(c3fd_fn<Th0_t>(0x00001f82)(p, 0));      // 0x1000b08e
    c3fd_gref(kWaveDevice) = d;                                               // 0x1000b097
    *out = d;                                                                 // 0x1000b09c
    return 0;                                                                 // 0x1000b09e
}

// 0x0000e910  Dll_Wave_In_Device::create_device (VA 0x1000e910), export #9 (thunk 0x100020e5; slot 0x0083af8c, read at
// 0x00487b9a in the open routine at 0x00487b90, outside every function, 0x00487ba4-0x00487baa). __cdecl (out, flags); only
// out is read (`mov edx, [esp+0x1c]` 0x1000e95b / 0x1000e999 after six pushes). A non-NULL 0x100b4a24 returns 0xc (`cmp` / `jne`
// 0x1000e92e / 0x1000e930, 0x1000e9be). Otherwise operator new(0x4c) (0x1000e937 / 0x1000e939): NULL stores 0 in the global and
// *out and returns 0 (0x1000e999-0x1000e9a7). A block is constructed by WaveInDeviceBase_ctor through the thunk 0x10002761
// (0x1000e94f / 0x1000e951; its eax is not used) and then filled in place: vtable 0x1005b3d4 at +0 (0x1000e95f), +0x30, +0x34,
// +0x38 = 0 (0x1000e965-0x1000e96b), +0x40 = 0x7f (0x1000e96e), +0x24, +0x28, +0x2c = 0 (0x1000e975-0x1000e97a), +0x3c = 0
// (0x1000e97d); the block goes to the global (0x1000e980) and *out (0x1000e986) and 0 is returned (eax zeroed at 0x1000e959).
CreateDev_t WaveInCreate_orig;
unsigned __cdecl WaveInCreate_re(void** out, unsigned) {
    if (c3fd_gref(kWaveInDevice)) return 0xc;                                 // 0x1000e926-0x1000e930 / 0x1000e9be
    void* p = c3fd_new(0x4c);                                                 // 0x1000e939
    if (!p) {                                                                 // 0x1000e947 / 0x1000e94d
        c3fd_gref(kWaveInDevice) = 0;                                         // 0x1000e99f
        *out = 0;                                                             // 0x1000e9a7
        return 0;                                                             // 0x1000e9a5
    }
    c3fd_fn<Th0_t>(0x00002761)(p, 0);                                         // 0x1000e951
    fat<void*>(p, 0) = c3fd_sound(0x0005b3d4);                                // 0x1000e95f
    fat<unsigned>(p, 0x30) = 0;                                               // 0x1000e965
    fat<unsigned>(p, 0x34) = 0;                                               // 0x1000e968
    fat<unsigned>(p, 0x38) = 0;                                               // 0x1000e96b
    fat<unsigned>(p, 0x40) = 0x7f;                                            // 0x1000e96e
    fat<unsigned>(p, 0x24) = 0;                                               // 0x1000e975
    fat<unsigned>(p, 0x28) = 0;                                               // 0x1000e977
    fat<unsigned>(p, 0x2c) = 0;                                               // 0x1000e97a
    fat<unsigned>(p, 0x3c) = 0;                                               // 0x1000e97d
    c3fd_gref(kWaveInDevice) = p;                                             // 0x1000e980
    *out = p;                                                                 // 0x1000e986
    return 0;                                                                 // 0x1000e959
}

// =====================================================================================================================
// B. MultiSound constructors and destructor, Seq_dtor
// =====================================================================================================================

// 0x00039690  MultiSound_ctor (VA 0x10039690). __thiscall, no stack argument (plain `ret` 0x10039729), returns this
// (`mov eax, esi` 0x1003971b). SubChannel_ctor runs on this through the thunk 0x10001465 (0x100396ae), then the channel
// constructor 0x10031be0 on this+0x70 through the thunk 0x10001005 (0x100396b5 / 0x100396bc). Then the vtable 0x1005c7e8 is
// stored at +0 (0x100396c9) and the fields: +0x1424 = 0 (0x100396d4), +0x1428 = 0 (0x100396db), +0x1420 = 0 (0x100396e4),
// +0x142c = 0 (0x100396ea), +0x1438 = 1 (0x100396f0), +0x143c = 0 (0x100396f6), +0x1440 / +0x1444 / +0x1448 = 0
// (0x100396fc-0x10039701), +0x1430 = 0 (0x10039706), +0x1434 = 0 (0x1003970c). Sound_setSourceType(1) runs on this through the
// thunk 0x100018a2 (`push eax` with eax = 1, 0x100396d6, call 0x10039712).
typedef void*(__fastcall* MsCtor_t)(void*, void*);
MsCtor_t MsCtor_orig;
void* __fastcall MsCtor_re(void* self, void*) {
    c3fd_fn<Th0_t>(0x00001465)(self, 0);                                      // 0x100396ae
    c3fd_fn<Th0_t>(0x00001005)(&fat<char>(self, 0x70), 0);                    // 0x100396bc
    fat<void*>(self, 0) = c3fd_sound(0x0005c7e8);                             // 0x100396c9
    fat<unsigned>(self, 0x1424) = 0;                                          // 0x100396d4
    fat<unsigned>(self, 0x1428) = 0;                                          // 0x100396db
    fat<unsigned>(self, 0x1420) = 0;                                          // 0x100396e4
    fat<unsigned>(self, 0x142c) = 0;                                          // 0x100396ea
    fat<unsigned>(self, 0x1438) = 1;                                          // 0x100396f0
    fat<unsigned>(self, 0x143c) = 0;                                          // 0x100396f6
    fat<unsigned>(self, 0x1440) = 0;                                          // 0x100396fc
    fat<unsigned>(self, 0x1444) = 0;                                          // 0x100396fe
    fat<unsigned>(self, 0x1448) = 0;                                          // 0x10039701
    fat<unsigned>(self, 0x1430) = 0;                                          // 0x10039706
    fat<unsigned>(self, 0x1434) = 0;                                          // 0x1003970c
    c3fd_fn<Th1_t>(0x000018a2)(self, 0, 1);                                   // 0x10039712
    return self;                                                              // 0x1003971b
}

// 0x00039790  MultiSound_ctorWithType (VA 0x10039790). __thiscall, one stack argument (`ret 4` 0x10039831; read as
// [esp+0x1c] at 0x100397df after six pushes), returns this (0x10039824). SubChannel_ctor on this (thunk 0x10001465,
// 0x100397ad), the channel constructor 0x10031be0 on this+0x70 (thunk 0x10001005, 0x100397bd), vtable 0x1005c7e8 at +0
// (0x100397ca), +0x1424 = 0 (0x100397d5), +0x1428 = 0 (0x100397dc), +0x1420 = the argument (0x100397e3), +0x1438 = 1
// (0x100397e9), +0x1430 = 0 (0x100397ef), +0x1434 = 0 (0x100397f9), +0x1ec = the argument (0x10039803), +0x1440 / +0x1444 /
// +0x1448 = 0 (0x10039813-0x10039818), then Sound_setSourceType(1) (`push ecx` with ecx = 1 at 0x100397db, thunk 0x100018a2,
// 0x1003981b). Unlike MultiSound_ctor it does not write +0x142c or +0x143c.
typedef void*(__fastcall* MsCtorT_t)(void*, void*, unsigned);
MsCtorT_t MsCtorT_orig;
void* __fastcall MsCtorT_re(void* self, void*, unsigned type) {
    c3fd_fn<Th0_t>(0x00001465)(self, 0);                                      // 0x100397ad
    c3fd_fn<Th0_t>(0x00001005)(&fat<char>(self, 0x70), 0);                    // 0x100397bd
    fat<void*>(self, 0) = c3fd_sound(0x0005c7e8);                             // 0x100397ca
    fat<unsigned>(self, 0x1424) = 0;                                          // 0x100397d5
    fat<unsigned>(self, 0x1428) = 0;                                          // 0x100397dc
    fat<unsigned>(self, 0x1420) = type;                                       // 0x100397e3
    fat<unsigned>(self, 0x1438) = 1;                                          // 0x100397e9
    fat<unsigned>(self, 0x1430) = 0;                                          // 0x100397ef
    fat<unsigned>(self, 0x1434) = 0;                                          // 0x100397f9
    fat<unsigned>(self, 0x1ec) = type;                                        // 0x10039803
    fat<unsigned>(self, 0x1440) = 0;                                          // 0x10039813
    fat<unsigned>(self, 0x1444) = 0;                                          // 0x10039815
    fat<unsigned>(self, 0x1448) = 0;                                          // 0x10039818
    c3fd_fn<Th1_t>(0x000018a2)(self, 0, 1);                                   // 0x1003981b
    return self;                                                              // 0x10039824
}

// 0x00039870  MultiSound_dtor (VA 0x10039870). __thiscall, no stack argument (plain `ret` 0x10039933); its only caller
// MultiSound_deleteDtor 0x10039760 replaces eax with this (0x10039778), so the A/B compares memory (ret void). The vtable
// 0x1005c7e8 is stored at +0 (0x10039890). With +0x1438 > 1 (signed, `cmp eax, ebp; jle` 0x100398a1 / 0x100398a7) the members
// at +0x1440 + 4*(i-1) for i = 1 .. +0x1438 - 1 (the count is reloaded each pass, 0x100398c2-0x100398ce) are deleted through
// their vtable slot 0 with 1 pushed when non-NULL (0x100398b2-0x100398ba) and cleared (0x100398bc). With bit 1 of the byte at
// +0xc8 set (0x100398d1 / 0x100398d8) Channel_releaseBuffers runs on this+0x70 (thunk 0x10001d11, 0x100398dd). A non-NULL
// +0x1430 (0x100398e8 / 0x100398ea) is passed to operator delete (0x100398ed) and cleared (0x100398f5). Channel_closeAndFree
// runs on this+0x70, or on NULL when this is NULL (0x100398ff-0x1003990f, thunk 0x100011b8), then Ch5bb90_dtor on this (thunk
// 0x10002108, 0x1003991e).
typedef void(__fastcall* MsDtor_t)(void*, void*);
MsDtor_t MsDtor_orig;
void __fastcall MsDtor_re(void* self, void*) {
    fat<void*>(self, 0) = c3fd_sound(0x0005c7e8);                             // 0x10039890
    if (fat<int>(self, 0x1438) > 1) {                                         // 0x100398a1 / 0x100398a7
        int i = 1;
        void** m = &fat<void*>(self, 0x1440);
        do {
            if (*m) {                                                         // 0x100398b2 / 0x100398b4
                c3fd_vfn<Th1_t>(*m, 0)(*m, 0, 1);                             // 0x100398b8 / 0x100398ba
                *m = 0;                                                       // 0x100398bc
            }
            ++i;
            ++m;
        } while (i < fat<int>(self, 0x1438));                                 // 0x100398c2-0x100398ce
    }
    if (fat<unsigned char>(self, 0xc8) & 2)                                   // 0x100398d1 / 0x100398d8
        c3fd_fn<Th0_t>(0x00001d11)(&fat<char>(self, 0x70), 0);                // 0x100398dd
    if (fat<void*>(self, 0x1430)) {                                           // 0x100398e8 / 0x100398ea
        c3fd_delete(fat<void*>(self, 0x1430));                                // 0x100398ed
        fat<void*>(self, 0x1430) = 0;                                         // 0x100398f5
    }
    c3fd_fn<Th0_t>(0x000011b8)(self ? &fat<char>(self, 0x70) : 0, 0);         // 0x100398ff-0x1003990f
    c3fd_fn<Th0_t>(0x00002108)(self, 0);                                      // 0x1003991e
}

// 0x0001e2b0  Seq_dtor (VA 0x1001e2b0). __thiscall, no stack argument (plain `ret` 0x1001e382); its caller Sound5b6a8_dtor
// makes another call right after it (0x100201a9 / 0x100201b8), so eax is not passed on (ret void). With bit 0 of the byte at
// +0x4c set (0x1001e2cf / 0x1001e2d4 / 0x1001e2da) Midi_Device::closeFile runs on this (thunk 0x10001014, 0x1001e2dc). Then
// the node list at +0x19c is torn down from the head: for a node n, its next n[1] (0x1001e2f3) becomes the head with its
// back link n'[0] cleared (0x1001e2fa / 0x1001e2fc), or with no next the tail +0x1a0 and the head are cleared
// (0x1001e304 / 0x1001e30a); the node's payload n[2] is read (0x1001e310), the node passed to operator delete (0x1001e314) and
// the count +0x1a8 decremented (0x1001e319-0x1001e325). The walk stops when that payload is 0 (`cmp edi, ebx; je`
// 0x1001e323 / 0x1001e32b, 0x1001e367 / 0x1001e36f) or the head is NULL (0x1001e333 / 0x1001e335), so a node with a NULL
// payload ends the teardown with the rest of the list in place.
typedef void(__fastcall* SeqDtor_t)(void*, void*);
SeqDtor_t SeqDtor_orig;
void __fastcall SeqDtor_re(void* self, void*) {
    if (fat<unsigned char>(self, 0x4c) & 1) c3fd_fn<Th0_t>(0x00001014)(self, 0);   // 0x1001e2d4-0x1001e2dc
    void** n = fat<void**>(self, 0x19c);                                      // 0x1001e2e1
    while (n) {                                                               // 0x1001e2ef / 0x1001e333
        void** next = reinterpret_cast<void**>(n[1]);                         // 0x1001e2f3
        if (next) {
            next[0] = 0;                                                      // 0x1001e2fa
            fat<void**>(self, 0x19c) = next;                                  // 0x1001e2fc
        } else {
            fat<void*>(self, 0x1a0) = 0;                                      // 0x1001e304
            fat<void*>(self, 0x19c) = 0;                                      // 0x1001e30a
        }
        void* payload = n[2];                                                 // 0x1001e310
        c3fd_delete(n);                                                       // 0x1001e314
        --fat<int>(self, 0x1a8);                                              // 0x1001e319-0x1001e325
        if (!payload) break;                                                  // 0x1001e323 / 0x1001e32b
        n = fat<void**>(self, 0x19c);                                         // 0x1001e32d
    }
}

// =====================================================================================================================
// C. command queues, voice notification, sequencer and MIDI device rows
// =====================================================================================================================

// 0x0000cc70  Wave_Device::flushCommandQueue (VA 0x1000cc70). __thiscall, no stack argument, returns 0 (0x1000cc8a,
// 0x1000cd1f). head = +0x41d8, tail = +0x41dc (0x1000cc76 / 0x1000cc7c); equal returns 0 at once (0x1000cc82 / 0x1000cc88).
// Otherwise n = (tail - head) & 0xfff (0x1000cc91-0x1000cc99) entries are visited from index (head + 1) & 0xfff (0x1000cc96 /
// 0x1000cc9e), wrapping at 0x1000 (0x1000cd04 / 0x1000cd05); n = 0 (`jbe` 0x1000cca6) visits none. For each index, CommandQueue::at
// on this+0x1d8 (thunk 0x10002257, 0x1000ccb4-0x1000ccb7) gives the entry e; NULL skips it (0x1000ccbe / 0x1000ccc0). The entry's
// type e[0] - 1 (0x1000ccc2 / 0x1000ccc4) indexes the 4-entry table 0x1000cd28 (`cmp eax, 3; ja` 0x1000ccc5 / 0x1000ccc8):
// types 1..3 go to 0x1000cce5, type 4 to 0x1000ccd1, any other only clears the slot (0x1000ccf5). Type 4 first deletes the object
// at e[1] through its vtable slot 0 with 1 pushed when non-NULL (0x1000ccd1-0x1000ccdc) and clears e[1] (0x1000ccde); types 1..4
// then run pairClear on e (thunk 0x10001816, 0x1000cce7), pass e to operator delete (0x1000cced) and clear the slot dword at
// this + 0x1d8 + 4*index (0x1000ccf5). Finally head = the tail read at entry (0x1000cd12 / 0x1000cd18).
typedef unsigned(__fastcall* Flush_t)(void*, void*);
Flush_t Flush_orig;
unsigned __fastcall Flush_re(void* self, void*) {
    unsigned head = fat<unsigned>(self, 0x41d8);                              // 0x1000cc76
    unsigned tail = fat<unsigned>(self, 0x41dc);                              // 0x1000cc7c
    if (head == tail) return 0;                                               // 0x1000cc82 / 0x1000cc88
    unsigned n = (tail - head) & 0xfff;                                       // 0x1000cc91-0x1000cc99
    unsigned idx = (head + 1) & 0xfff;                                        // 0x1000cc96 / 0x1000cc9e
    for (; n; --n) {                                                          // 0x1000cca6 / 0x1000cd0b / 0x1000cd10
        unsigned* e = reinterpret_cast<unsigned*>(c3fd_fn<Th1_t>(0x00002257)(&fat<char>(self, 0x1d8), 0, idx));   // 0x1000ccb7
        if (e) {                                                              // 0x1000ccbe / 0x1000ccc0
            unsigned t = e[0] - 1;                                            // 0x1000ccc2 / 0x1000ccc4
            if (t <= 3) {                                                     // 0x1000ccc5 / 0x1000ccc8
                if (t == 3) {                                                 // table 0x1000cd28 entry 3 -> 0x1000ccd1
                    void* o = reinterpret_cast<void*>(e[1]);                  // 0x1000ccd1
                    if (o) c3fd_vfn<Th1_t>(o, 0)(o, 0, 1);                    // 0x1000ccd4-0x1000ccdc
                    e[1] = 0;                                                 // 0x1000ccde
                }
                c3fd_fn<Th0_t>(0x00001816)(e, 0);                             // 0x1000cce7
                c3fd_delete(e);                                               // 0x1000cced
            }
            fat<unsigned>(self, 0x1d8 + 4 * idx) = 0;                         // 0x1000ccf5
        }
        idx = (idx + 1) & 0xfff;                                              // 0x1000cd04 / 0x1000cd05
    }
    fat<unsigned>(self, 0x41d8) = tail;                                       // 0x1000cd18
    return 0;                                                                 // 0x1000cd1f
}

// 0x0001ac40  Voice_notifyChannelNotes (VA 0x1001ac40). __thiscall, no stack argument, returns 0 (0x1001acfb). For each of the
// 0x80 slots g = this[1 + i] (`[ebp + esi*4 + 4]` 0x1001ac48, `cmp esi, 0x80; jl` 0x1001acec / 0x1001acf2) that is non-NULL
// (0x1001ac4c / 0x1001ac4e): when g's count c = +0x1438 is non-zero (0x1001ac5a / 0x1001ac5c), each member m = g[0x1440 + 4j]
// for j < c - 1 (signed, the count re-read each pass: 0x1001ac60-0x1001ac63, 0x1001acaa-0x1001acb3) is first offered to
// removeSoundNode on the device 0x100b4a20 (thunk 0x10002243, ecx = the device, 0x1001ac6d-0x1001ac74); when that returns 0
// (0x1001ac79 / 0x1001ac7b) the member (re-read, 0x1001ac7d / 0x1001ac81) is rewound by SoundGroup_rewind (thunk 0x10002568)
// when bit 0 of its byte +0xc8 is set (0x1001ac84 / 0x1001ac8b), else postNotification(member, 5) runs on the device (thunk
// 0x10001b3b, 0x1001ac94-0x1001ac9d). Then g itself goes through the same steps (0x1001acb5-0x1001ace6).
typedef unsigned(__fastcall* Notify_t)(void*, void*);
Notify_t Notify_orig;
unsigned c3fd_offerNode(void* m) {
    void* dev = c3fd_gref(kWaveDevice);                                       // 0x1001ac6d / 0x1001acb9
    if (c3fd_fn<Th1_t>(0x00002243)(dev, 0, reinterpret_cast<unsigned>(m)) != 0) return 1;   // 0x1001ac74 / 0x1001acc0
    return 0;
}
void c3fd_rewindOrPost(void* m) {
    if (fat<unsigned char>(m, 0xc8) & 1)                                      // 0x1001ac84 / 0x1001accd
        c3fd_fn<Th0_t>(0x00002568)(m, 0);                                     // 0x1001ac8d / 0x1001acd6
    else
        c3fd_fn<Th2_t>(0x00001b3b)(c3fd_gref(kWaveDevice), 0, reinterpret_cast<unsigned>(m), 5);   // 0x1001ac94-0x1001ac9d
}
unsigned __fastcall Notify_re(void* self, void*) {
    for (int i = 0; i < 0x80; ++i) {                                          // 0x1001ac46 / 0x1001acec
        void* g = fat<void*>(self, 4 + 4 * i);                                // 0x1001ac48
        if (!g) continue;                                                     // 0x1001ac4e
        if (fat<int>(g, 0x1438)) {                                            // 0x1001ac5a / 0x1001ac5c
            for (int j = 0; j < fat<int>(fat<void*>(self, 4 + 4 * i), 0x1438) - 1; ++j) {   // 0x1001ac60-0x1001ac63, 0x1001acb3
                void* m = fat<void*>(fat<void*>(self, 4 + 4 * i), 0x1440 + 4 * j);   // 0x1001ac6a
                if (!c3fd_offerNode(m))                                       // 0x1001ac7b
                    c3fd_rewindOrPost(fat<void*>(fat<void*>(self, 4 + 4 * i), 0x1440 + 4 * j));   // 0x1001ac7d / 0x1001ac81
            }
        }
        void* g2 = fat<void*>(self, 4 + 4 * i);                               // 0x1001acb5
        if (!c3fd_offerNode(g2))                                              // 0x1001acc7
            c3fd_rewindOrPost(fat<void*>(self, 4 + 4 * i));                   // 0x1001acc9
    }
    return 0;                                                                 // 0x1001acfb
}

// 0x00020d80  Voice_allNotesOff (VA 0x10020d80). __thiscall, one stack argument (a flag byte, `mov al, [esp+0xc]` 0x10020dd2;
// `ret 4` 0x10020e4e). g_midiDevice 0x100b4a1c is read with no NULL test (0x10020d80 / 0x10020d89). When its +0x3c is non-zero
// (0x10020d8c / 0x10020d8e) the list at +0x1fc is walked with the cursor +0x204 (0x10020d90-0x10020d98): while the cursor node n
// is non-NULL and its payload n[2] is non-NULL (0x10020da0-0x10020da5, 0x10020dc5-0x10020dca) Midi_Device::flushMessages runs on
// the payload (thunk 0x10001bbd, 0x10020da7 / 0x10020da9), then the cursor is re-read (0x10020dae-0x10020db6) and advanced to
// n[1] (0x10020db8-0x10020dc3). Then, unless bit 1 of the byte +0x58 is set and the flag is 0 (0x10020dcc-0x10020dd8), each of
// the 0x7f voices v = +0x744 + 4k (0x10020ddb / 0x10020de1 / 0x10020e05) that is non-NULL, while the device 0x100b4a20 is
// non-NULL and v's +0x30 is non-NULL (0x10020de8-0x10020dfb), runs Voice_notifyChannelNotes on v+0x30's object (thunk
// 0x100020f4, 0x10020dfd). Finally +0xac loses bit 2 (0x10020e09-0x10020e16), +0x740 = 0 (0x10020e27), +0x214 gains bit 3
// (0x10020e1c / 0x10020e31 / 0x10020e33), vtable slot +0x48 runs on this with 0 pushed (0x10020e0f / 0x10020e3b), and +0x ac,
// re-read, gains bit 3 (0x10020e3e-0x10020e47); that value is returned.
typedef unsigned(__fastcall* AllOff_t)(void*, void*, unsigned);
AllOff_t AllOff_orig;
unsigned __fastcall AllOff_re(void* self, void*, unsigned flag) {
    void* md = c3fd_gref(kMidiDevice);                                        // 0x10020d80
    if (fat<unsigned>(md, 0x3c)) {                                            // 0x10020d89-0x10020d8e
        void** n = fat<void**>(self, 0x1fc);                                  // 0x10020d90
        fat<void**>(self, 0x204) = n;                                         // 0x10020d98
        while (n && n[2]) {                                                   // 0x10020d96 / 0x10020da3 / 0x10020dc8
            c3fd_fn<Th0_t>(0x00001bbd)(n[2], 0);                              // 0x10020da9
            n = fat<void**>(self, 0x204);                                     // 0x10020dae
            if (!n) break;                                                    // 0x10020db6
            n = reinterpret_cast<void**>(n[1]);                               // 0x10020db8
            fat<void**>(self, 0x204) = n;                                     // 0x10020dbd
        }
    }
    if (!(fat<unsigned char>(self, 0x58) & 2) || static_cast<unsigned char>(flag)) {   // 0x10020dcc-0x10020dd8
        for (int k = 0; k < 0x7f; ++k) {                                      // 0x10020de1 / 0x10020e05 / 0x10020e06
            void* v = fat<void*>(self, 0x744 + 4 * k);                        // 0x10020de6
            if (v && c3fd_gref(kWaveDevice) && fat<void*>(v, 0x30))           // 0x10020dea / 0x10020df4 / 0x10020dfb
                c3fd_fn<Th0_t>(0x000020f4)(fat<void*>(v, 0x30), 0);           // 0x10020dfd
        }
    }
    fat<unsigned>(self, 0xac) &= ~4u;                                         // 0x10020e09-0x10020e16
    fat<unsigned>(self, 0x740) = 0;                                           // 0x10020e27
    fat<unsigned>(self, 0x214) |= 8;                                          // 0x10020e1c / 0x10020e31 / 0x10020e33
    c3fd_vfn<Th1_t>(self, 0x48)(self, 0, 0);                                  // 0x10020e3b
    fat<unsigned>(self, 0xac) |= 8;                                           // 0x10020e3e-0x10020e47
    return fat<unsigned>(self, 0xac);
}

// Seq_listGetAt 0x1001d780 through its thunk 0x100015fa: __thiscall (list, out, index), result 0 on success.
unsigned c3fd_listGetAt(void* list, void** out, unsigned index) {
    return c3fd_fn<Th2_t>(0x000015fa)(list, 0, reinterpret_cast<unsigned>(out), index);
}

// 0x00023950  Seq_replayEventRange (VA 0x10023950). __thiscall (first, last), `ret 8`. last < first (unsigned, `jb`
// 0x1002395b / 0x1002395f) or last >= vtable slot +0xb4 of this (the track count, 0x10023965-0x1002396f, `jae`) returns 0xa
// (0x10023a0f). Otherwise Seq_listGetAt(this+0x60, slot, 0) runs, the out slot being last's own argument slot
// (`lea ecx, [esp+0x18]` 0x10023976 after four pushes); when it returns NON-zero (`je` 0x10023989 skips on 0) bit 3 of the byte
// +0x38 of whatever that slot then holds is set (0x1002398b / 0x1002398f). Then vtable slot +0x5c of this (0x10023993-0x10023997):
// 0 clears bit 3 of +0x38 of every track i in first..last that Seq_listGetAt finds (result 0, 0x100239a2-0x100239bb; `jbe`
// 0x100239be); non-zero visits the same range (0x100239cd-0x10023a02) and, for each found track whose +0x38 has bit 3
// (0x100239de-0x100239e7), clears the bit (0x100239e9 / 0x100239ec), runs Midi_Device::emitEvent(i) on this (thunk 0x100021ee,
// 0x100239f1) and Seq_rewindTracks on the track (thunk 0x10001ca3, 0x100239f6 / 0x100239fa). Both walks return 0.
typedef unsigned(__fastcall* Replay_t)(void*, void*, unsigned, unsigned);
Replay_t Replay_orig;
unsigned __fastcall Replay_re(void* self, void*, unsigned first, unsigned last) {
    if (last < first) return 0xa;                                             // 0x1002395b / 0x1002395f
    if (last >= c3fd_vfn<Th0_t>(self, 0xb4)(self, 0)) return 0xa;             // 0x10023967-0x1002396f
    void* list = &fat<char>(self, 0x60);                                      // 0x1002397a
    void* slot = reinterpret_cast<void*>(last);                               // the argument slot the original passes
    if (c3fd_listGetAt(list, &slot, 0) != 0)                                  // 0x10023982 / 0x10023989
        fat<unsigned char>(slot, 0x38) |= 8;                                  // 0x1002398b / 0x1002398f
    if (c3fd_vfn<Th0_t>(self, 0x5c)(self, 0) == 0) {                          // 0x10023997 / 0x1002399c
        for (unsigned i = first; i <= last; ++i) {                            // 0x1002399e / 0x100239bb / 0x100239be
            if (c3fd_listGetAt(list, &slot, i) == 0)                          // 0x100239aa / 0x100239b1
                fat<unsigned char>(slot, 0x38) &= 0xf7;                       // 0x100239b3 / 0x100239b7
        }
    } else {
        for (unsigned i = first; i <= last; ++i) {                            // 0x100239c9 / 0x100239ff / 0x10023a02
            if (c3fd_listGetAt(list, &slot, i) != 0) continue;                // 0x100239d5 / 0x100239dc
            unsigned char b = fat<unsigned char>(slot, 0x38);                 // 0x100239e2
            if (!(b & 8)) continue;                                           // 0x100239e5 / 0x100239e7
            fat<unsigned char>(slot, 0x38) = b & 0xf7;                        // 0x100239e9 / 0x100239ec
            c3fd_fn<Th1_t>(0x000021ee)(self, 0, i);                           // 0x100239f1
            c3fd_fn<Th0_t>(0x00001ca3)(slot, 0);                              // 0x100239fa
        }
    }
    return 0;                                                                 // 0x100239c3 / 0x10023a07
}

// 0x000246a0  Midi_Device::emitEvent (VA 0x100246a0). __thiscall (index), `ret 4`. Seq_listGetAt(this+0x60, slot, index) runs
// with the out slot = the index argument's own slot (`lea edx, [esp+0xc]` 0x100246a6, `add ecx, 0x60` 0x100246ac); a non-zero
// result is returned (`jne` 0x100246b6 -> 0x10024778). With the track t it found: when g_midiDevice 0x100b4a1c's +0x3c is
// non-zero (0x100246bc-0x100246c6), each dword at t+0x258 .. t+0x454 (`cmp esi, 0x458; jl` 0x10024707 / 0x1002470d) whose low
// byte is non-zero (0x100246d7 / 0x100246db) first gets its byte 2 cleared (0x100246dd), then midiOutShortMsg(+0x3c re-read,
// that dword) runs through [0x100b8638] (0x100246e2-0x100246f3) and the dword is cleared (0x100246f9); 0 is returned
// (0x10024710). Otherwise each dword s at t+0x58 .. t+0x254 (`cmp ebx, 0x258; jl` 0x10024769 / 0x10024772) that is non-NULL
// (0x10024724 / 0x10024726) is cleared (0x10024728), vtable slot +0x28 runs on s (0x10024731 / 0x10024733), and when s's
// +0x1438 is above 1 (0x10024736-0x1002473f) slot +0x28 also runs on each member s[0x1440 + 4j], j < +0x1438 - 1 (re-read each
// pass, 0x10024756-0x10024763), with no NULL test on the member; 0 is returned (0x10024775).
typedef unsigned(__fastcall* Emit_t)(void*, void*, unsigned);
Emit_t Emit_orig;
unsigned __fastcall Emit_re(void* self, void*, unsigned index) {
    void* t = reinterpret_cast<void*>(index);                                 // the argument slot the original passes
    unsigned r = c3fd_listGetAt(&fat<char>(self, 0x60), &t, index);           // 0x100246af
    if (r) return r;                                                          // 0x100246b4 / 0x100246b6
    if (fat<unsigned>(c3fd_gref(kMidiDevice), 0x3c)) {                        // 0x100246bc-0x100246c6
        Imp2_t shortMsg = c3fd_imp<Imp2_t>(kIatMidiOutShortMsg);              // 0x100246c8
        for (int off = 0x258; off < 0x458; off += 4) {                        // 0x100246d2 / 0x10024704-0x1002470d
            if (!fat<unsigned char>(t, off)) continue;                        // 0x100246d7 / 0x100246db
            fat<unsigned char>(t, off + 2) = 0;                               // 0x100246dd
            shortMsg(fat<unsigned>(c3fd_gref(kMidiDevice), 0x3c), fat<unsigned>(t, off));   // 0x100246e2-0x100246f3
            fat<unsigned>(t, off) = 0;                                        // 0x100246f9
        }
        return 0;                                                             // 0x10024710
    }
    for (int off = 0x58; off < 0x258; off += 4) {                             // 0x1002471c / 0x10024769-0x10024772
        void* s = fat<void*>(t, off);                                         // 0x10024721
        if (!s) continue;                                                     // 0x10024724 / 0x10024726
        fat<void*>(t, off) = 0;                                               // 0x10024728
        c3fd_vfn<Th0_t>(s, 0x28)(s, 0);                                       // 0x10024733
        if (fat<int>(s, 0x1438) > 1) {                                        // 0x10024736-0x1002473f
            for (int j = 0; j < fat<int>(s, 0x1438) - 1; ++j) {               // 0x10024741-0x10024746, 0x10024756-0x10024763
                void* m = fat<void*>(s, 0x1440 + 4 * j);                      // 0x1002474e
                c3fd_vfn<Th0_t>(m, 0x28)(m, 0);                               // 0x10024753
            }
        }
    }
    return 0;                                                                 // 0x10024775
}

// 0x00021a90  Midi_Device::sendShortMsg (VA 0x10021a90). __thiscall (msg, channel, track), `ret 0xc` (args read at
// [esp+0x18] / [esp+0x1c] / [esp+0x20] after five pushes, 0x10021ab0 / 0x10021a95 / 0x10021a99). v = this[0x25c + 0x4c*channel]
// (0x10021aa3-0x10021aa9) is kept in the channel's argument slot (0x10021ab4); note = msg[0] (0x10021ab8). When
// Seq_firstTrackData on the track (thunk 0x10001906, 0x10021abe) is non-zero (0x10021ac3 / 0x10021ac5) the note is remapped by
// the track's vtable slot +4 (`push ebp; call [edx+4]` 0x10021ac7-0x10021acf). With g_midiDevice 0x100b4a1c's +0x3c non-zero
// (0x10021ad1-0x10021add): midiOutShortMsg(+0x3c, ((msg[1] << 8 | note') << 8 | channel) | 0x80 on the low byte)
// (0x10021adf-0x10021af4, IAT [0x100b8638]), then the track's dword +0x258 + 4*note' is cleared (0x10021afa) and 2 returned. With
// +0x3c zero: e = this[0x744 + 4*v] (0x10021b12-0x10021b1a) and c = track[0x58 + 4*msg[0]] (the original note, 0x10021b21); c
// NULL (0x10021b25 / 0x10021b27) or bit 3 of e's byte +4 clear (0x10021b29 / 0x10021b2d) returns 2. Otherwise that track slot
// and c's +0x143c are cleared (0x10021b2f / 0x10021b33); with bit 0 of c's byte +0xc8 set (0x10021b39 / 0x10021b40)
// Channel_findVoiceByTag(msg[0]) runs on c+0x70 (thunk 0x1000187a, 0x10021b42-0x10021b4e) and a found voice gets Voice_noteOff
// (thunk 0x100014a6, 0x10021b53-0x10021b59); with it clear, a non-NULL e+0x30 (0x10021b6b-0x10021b70) passes its +0x218 to
// Channel_recomputeGainsSetBit3 on c+0x70 (thunk 0x1000272a, 0x10021b72-0x10021b7c), and a NULL one runs c's vtable slot +0x28
// (0x10021b8e / 0x10021b90). Every path past the first test returns 2.
typedef unsigned(__fastcall* ShortMsg_t)(void*, void*, unsigned char*, unsigned, void*);
ShortMsg_t ShortMsg_orig;
unsigned __fastcall ShortMsg_re(void* self, void*, unsigned char* msg, unsigned channel, void* track) {
    unsigned v = fat<unsigned>(self, 0x25c + 0x4c * channel);                 // 0x10021aa3-0x10021aa9
    unsigned note = msg[0];                                                   // 0x10021ab8
    unsigned mapped = note;                                                   // 0x10021abc
    if (c3fd_fn<Th0_t>(0x00001906)(track, 0))                                 // 0x10021abe-0x10021ac5
        mapped = c3fd_vfn<Th1_t>(track, 4)(track, 0, mapped);                 // 0x10021ac7-0x10021acf
    void* md = c3fd_gref(kMidiDevice);                                        // 0x10021ad1
    unsigned h = fat<unsigned>(md, 0x3c);                                     // 0x10021ad8
    if (h) {                                                                  // 0x10021adb / 0x10021add
        unsigned m = ((static_cast<unsigned>(msg[1]) << 8 | mapped) << 8 | channel) | 0x80;   // 0x10021ae3-0x10021aef
        c3fd_imp<Imp2_t>(kIatMidiOutShortMsg)(h, m);                          // 0x10021af4
        fat<unsigned>(track, 0x258 + 4 * mapped) = 0;                         // 0x10021afa
        return 2;                                                             // 0x10021b08
    }
    void* e = fat<void*>(self, 0x744 + 4 * v);                                // 0x10021b12-0x10021b1a
    void* c = fat<void*>(track, 0x58 + 4 * note);                             // 0x10021b21
    if (!c) return 2;                                                         // 0x10021b25 / 0x10021b27
    if (!(fat<unsigned char>(e, 4) & 8)) return 2;                            // 0x10021b29 / 0x10021b2d
    fat<void*>(track, 0x58 + 4 * note) = 0;                                   // 0x10021b2f
    fat<unsigned>(c, 0x143c) = 0;                                             // 0x10021b33
    if (fat<unsigned char>(c, 0xc8) & 1) {                                    // 0x10021b39 / 0x10021b40
        void* vc = reinterpret_cast<void*>(c3fd_fn<Th1_t>(0x0000187a)(&fat<char>(c, 0x70), 0, msg[0]));   // 0x10021b4e
        if (vc) c3fd_fn<Th0_t>(0x000014a6)(vc, 0);                            // 0x10021b53-0x10021b59
        return 2;
    }
    void* a = fat<void*>(e, 0x30);                                            // 0x10021b6b
    if (a)                                                                    // 0x10021b6e / 0x10021b70
        c3fd_fn<Th1_t>(0x0000272a)(&fat<char>(c, 0x70), 0, fat<unsigned>(a, 0x218));   // 0x10021b72-0x10021b7c
    else
        c3fd_vfn<Th0_t>(c, 0x28)(c, 0);                                       // 0x10021b8e / 0x10021b90
    return 2;
}

// 0x00020870  Midi_Device::playEvent (VA 0x10020870). __thiscall (track, count*), `ret 8`; ecx is passed on unchanged to the
// handlers. A NULL track returns 0xa (0x10020877-0x1002087f). p = track+0x2c (0x10020888) and b = *p (0x1002088f). With bit 7 of
// b set (0x10020891 / 0x10020894): for b & 0xf0 != 0xf0 (0x10020896-0x100208aa) the track's running status byte +0x30 = b
// (0x100208ae) and channel +0x48 = b & 0xf (0x100208b1 / 0x100208b6), the kind is b & 0xf0 and the channel b & 0xf; for 0xf0..0xff
// the kind is b itself (ebx keeps all of b) and the channel register is the track pointer (`mov eax, [esp+0x14]` 0x100208c2); in
// both, *count = 1 (0x100208b9 / 0x100208c6) and p advances one byte (0x100208bf / 0x100208cc). With bit 7 clear the kind is
// +0x30 & 0xf0 and the channel +0x48 (0x100208cf-0x100208d5), p and *count unchanged. kind - 0x80 above 0x7f (unsigned,
// 0x100208db-0x100208e1) returns 0xb; otherwise the byte table 0x100209a4 (0x100208e9) picks one of the seven entries of
// 0x10020988 (0x100208ef): 0x80 -> Midi_Device::sendShortMsg(p, channel, track) (thunk 0x100016bd, 0x10020933-0x10020936);
// 0x90 -> p[1] non-zero runs Midi_Device::noteOn(p, channel, track) (thunk 0x10001a14, 0x10020915-0x1002091f) and p[1] zero
// sendShortMsg (0x1002091a); 0xb0 -> Midi_Device::setChannelVolume(p, channel, track) (thunk 0x100022ca, 0x10020969-0x1002096c);
// each adds its result to *count and returns 0 (0x10020924-0x10020930, 0x1002093b-0x10020947, 0x10020971-0x10020979); 0xc0 ->
// this[0x25c + 0x4c*channel] = p[0] (0x1002094a-0x10020954), *count + 1, 0 (0x1002095b-0x10020966); 0xe0 -> *count + 1, 0
// (0x1002095b); 0xff -> Midi_Device::handleMetaEvent(p, track) (thunk 0x1000124e, 0x100208f6-0x100208f8), its result added to
// *count, then 0x29 when p[0] is 0x2f (0x10020903-0x1002090c), else 0 (0x10020973); every other kind returns 0xb (0x1002097c).
typedef unsigned(__fastcall* PlayEv_t)(void*, void*, void*, unsigned*);
PlayEv_t PlayEv_orig;
unsigned __fastcall PlayEv_re(void* self, void*, void* track, unsigned* count) {
    if (!track) return 0xa;                                                   // 0x10020877-0x1002087f
    unsigned char* p = fat<unsigned char*>(track, 0x2c);                      // 0x10020888
    unsigned b = *p;                                                          // 0x1002088f
    unsigned kind;
    unsigned channel;
    if (b & 0x80) {                                                           // 0x10020891 / 0x10020894
        if ((b & 0xf0) != 0xf0) {                                             // 0x1002089c-0x100208aa
            fat<unsigned char>(track, 0x30) = static_cast<unsigned char>(b);  // 0x100208ae
            channel = b & 0xf;                                                // 0x100208b1
            fat<unsigned>(track, 0x48) = channel;                             // 0x100208b6
            kind = b & 0xf0;                                                  // 0x100208b4
        } else {
            channel = reinterpret_cast<unsigned>(track);                      // 0x100208c2
            kind = b;
        }
        *count = 1;                                                           // 0x100208b9 / 0x100208c6
        ++p;                                                                  // 0x100208bf / 0x100208cc
    } else {
        kind = fat<unsigned char>(track, 0x30) & 0xf0;                        // 0x100208cf / 0x100208d5
        channel = fat<unsigned>(track, 0x48);                                 // 0x100208d2
    }
    unsigned idx = kind - 0x80;                                               // 0x100208db
    if (idx > 0x7f) return 0xb;                                               // 0x100208de / 0x100208e1
    unsigned c = *reinterpret_cast<unsigned char*>(c3fd_sound(0x000209a4 + idx));   // 0x100208e9
    unsigned arg = reinterpret_cast<unsigned>(p);
    switch (c) {                                                              // table 0x10020988 (0x100208ef)
    case 0:                                                                   // 0x10020933
        *count += c3fd_fn<Th3_t>(0x000016bd)(self, 0, arg, channel, reinterpret_cast<unsigned>(track));
        return 0;
    case 1:                                                                   // 0x10020915
        if (p[1])                                                             // 0x10020918 / 0x1002091a
            *count += c3fd_fn<Th3_t>(0x00001a14)(self, 0, arg, channel, reinterpret_cast<unsigned>(track));
        else
            *count += c3fd_fn<Th3_t>(0x000016bd)(self, 0, arg, channel, reinterpret_cast<unsigned>(track));
        return 0;
    case 2:                                                                   // 0x10020969
        *count += c3fd_fn<Th3_t>(0x000022ca)(self, 0, arg, channel, reinterpret_cast<unsigned>(track));
        return 0;
    case 3:                                                                   // 0x1002094a
        fat<unsigned>(self, 0x25c + 0x4c * channel) = p[0];                   // 0x1002094c-0x10020954
        *count += 1;                                                          // 0x1002095b-0x1002095f
        return 0;
    case 4:                                                                   // 0x1002095b
        *count += 1;
        return 0;
    case 5: {                                                                 // 0x100208f6
        *count += c3fd_fn<Th2_t>(0x0000124e)(self, 0, arg, reinterpret_cast<unsigned>(track));   // 0x100208f8-0x10020901
        if (*p == 0x2f) return 0x29;                                          // 0x10020903-0x1002090c
        return 0;
    }
    default:                                                                  // 0x1002097c
        return 0xb;
    }
}

// 0x0002add0  Engine_playSound (VA 0x1002add0). __thiscall (name), `ret 4`. The word 0x100b507c zero returns 1 (0x1002add1 /
// 0x1002addd / 0x1002ade1). slot = this+0x40 (0x1002adf2). An empty slot (0x1002adf5 / 0x1002adf7) calls create_sound(slot,
// name, 1) through the thunk 0x1000137a (0x1002adf9-0x1002adfd), whose non-zero result is returned (0x1002ae05 / 0x1002ae07); a
// filled slot whose vtable slot +0x60 returns non-zero (0x1002ae10-0x1002ae17) returns 0xf (0x1002ae1b). Then r = the slot
// object's vtable slot +0x10 with the name pushed (0x1002ae25-0x1002ae2a), kept as the result (0x1002ae2d). f = this+0x44
// (0x1002ae33): r non-zero clears bit 0 of it (0x1002ae5a / 0x1002ae5c); r zero with bit 0 clear (0x1002ae38 / 0x1002ae3a) sets
// it (0x1002ae3c-0x1002ae40), runs this's vtable slot +0x7c (0x1002ae43 / 0x1002ae45) and, when this+0x30 is non-zero
// (0x1002ae48-0x1002ae4d), the slot object's vtable slot +0x48 with 1 pushed (0x1002ae4f-0x1002ae55). Then the name is copied
// with its terminator into operator new(strlen + 1) (0x1002ae60-0x1002ae97, no NULL test), the old +0x50 string, when non-NULL,
// goes to operator delete (0x1002ae99-0x1002aea2) and +0x50 = the copy (0x1002aeb3); r is returned (0x1002aeae).
typedef unsigned(__fastcall* PlaySnd_t)(void*, void*, const char*);
PlaySnd_t PlaySnd_orig;
unsigned __fastcall PlaySnd_re(void* self, void*, const char* name) {
    if (!*reinterpret_cast<unsigned*>(c3fd_sound(kSoundReady))) return 1;      // 0x1002add1-0x1002ade1
    void** slot = &fat<void*>(self, 0x40);                                    // 0x1002adf2
    if (!*slot) {                                                             // 0x1002adf5 / 0x1002adf7
        unsigned c = c3fd_fn<CreateSound_t>(0x0000137a)(slot, reinterpret_cast<unsigned>(name), 1);   // 0x1002adfd
        if (c) return c;                                                      // 0x1002ae05 / 0x1002ae07
    } else if (c3fd_vfn<Th0_t>(*slot, 0x60)(*slot, 0)) {                      // 0x1002ae10-0x1002ae17
        return 0xf;                                                           // 0x1002ae1b
    }
    unsigned r = c3fd_vfn<Th1_t>(*slot, 0x10)(*slot, 0, reinterpret_cast<unsigned>(name));   // 0x1002ae25-0x1002ae2d
    unsigned f = fat<unsigned>(self, 0x44);                                   // 0x1002ae33
    if (r) {
        fat<unsigned>(self, 0x44) = f & ~1u;                                  // 0x1002ae5a / 0x1002ae5c
    } else if (!(f & 1)) {                                                    // 0x1002ae38 / 0x1002ae3a
        fat<unsigned>(self, 0x44) = f | 1;                                    // 0x1002ae3c-0x1002ae40
        c3fd_vfn<Th0_t>(self, 0x7c)(self, 0);                                 // 0x1002ae45
        if (fat<unsigned>(self, 0x30))                                        // 0x1002ae48-0x1002ae4d
            c3fd_vfn<Th1_t>(*slot, 0x48)(*slot, 0, 1);                        // 0x1002ae4f-0x1002ae55
    }
    unsigned n = static_cast<unsigned>(strlen(name)) + 1;                     // 0x1002ae60-0x1002ae69
    char* copy = static_cast<char*>(c3fd_new(n));                             // 0x1002ae6c
    memcpy(copy, name, n);                                                    // 0x1002ae73-0x1002ae97
    if (fat<void*>(self, 0x50)) c3fd_delete(fat<void*>(self, 0x50));          // 0x1002ae99-0x1002aea2
    fat<char*>(self, 0x50) = copy;                                            // 0x1002aeb3
    return r;                                                                 // 0x1002aeae
}

// 0x0002f220  buildVoiceTxName (VA 0x1002f220). __thiscall, no stack argument (plain `ret` 0x1002f314), returns this
// (0x1002f306). An inline sub-channel constructor: +4 = 0x7f (0x1002f249), +8 = 0 (0x1002f250), nine dwords +0xc .. +0x2c = 0
// (`rep stosd` 0x1002f23f-0x1002f253), +0x30 = 0 (0x1002f259), vtable 0x1005bae0 at +0 (0x1002f25e), +0x48, +0x4c, +0x40,
// +0x50 = 0 (0x1002f264-0x1002f26d), +0x44 = 0 and again 0 & ~1 (0x1002f270-0x1002f276), +0x38 = 0x3e8 (0x1002f282); the
// +0x40 object, just zeroed, is tested (0x1002f279-0x1002f289), so its vtable slot +4 call (0x1002f28b-0x1002f292) never runs.
// Then +0x54 = 0 (0x1002f295), vtable 0x1005c2ec at +0 (0x1002f29b), +0x58 = 0 (0x1002f2ab). Nine bytes at 0x10063e8c (two
// dwords and a byte, 0x1002f2ad-0x1002f2c6) are copied to a local, operator new(10) (0x1002f2a1 / 0x1002f2ca) is stored at +0x50
// (0x1002f2dd, no NULL test) and the local string with its terminator copied into it (0x1002f2d1-0x1002f2f8). Sound_setSourceType
// (7) runs on this (thunk 0x100018a2, 0x1002f2e6 / 0x1002f2fc).
typedef void*(__fastcall* TxName_t)(void*, void*);
TxName_t TxName_orig;
void* __fastcall TxName_re(void* self, void*) {
    fat<unsigned>(self, 4) = 0x7f;                                            // 0x1002f249
    fat<unsigned>(self, 8) = 0;                                               // 0x1002f250
    for (int k = 0; k < 9; ++k) fat<unsigned>(self, 0xc + 4 * k) = 0;         // 0x1002f23f-0x1002f253
    fat<unsigned>(self, 0x30) = 0;                                            // 0x1002f259
    fat<void*>(self, 0) = c3fd_sound(0x0005bae0);                             // 0x1002f25e
    fat<unsigned>(self, 0x48) = 0;                                            // 0x1002f264
    fat<unsigned>(self, 0x4c) = 0;                                            // 0x1002f267
    fat<unsigned>(self, 0x40) = 0;                                            // 0x1002f26a
    fat<unsigned>(self, 0x50) = 0;                                            // 0x1002f26d
    fat<unsigned>(self, 0x44) = 0;                                            // 0x1002f273 / 0x1002f276
    fat<unsigned>(self, 0x38) = 0x3e8;                                        // 0x1002f282
    if (fat<void*>(self, 0x40))                                               // 0x1002f279-0x1002f289
        c3fd_vfn<Th1_t>(fat<void*>(self, 0x40), 4)(fat<void*>(self, 0x40), 0, 0x3e8);   // 0x1002f28b-0x1002f292
    fat<unsigned>(self, 0x54) = 0;                                            // 0x1002f295
    fat<void*>(self, 0) = c3fd_sound(0x0005c2ec);                             // 0x1002f29b
    fat<unsigned>(self, 0x58) = 0;                                            // 0x1002f2ab
    char local[9];
    memcpy(local, c3fd_sound(0x00063e8c), 9);                                 // 0x1002f2ad-0x1002f2c6
    char* p = static_cast<char*>(c3fd_new(10));                               // 0x1002f2ca
    fat<char*>(self, 0x50) = p;                                               // 0x1002f2dd
    memcpy(p, local, strlen(local) + 1);                                      // 0x1002f2d1-0x1002f2f8
    c3fd_fn<Th1_t>(0x000018a2)(self, 0, 7);                                   // 0x1002f2fc
    return self;                                                              // 0x1002f306
}

// =====================================================================================================================
// D. second group: command-ring scans, list teardowns, sequencer dispatch, DirectSound buffer creation
// =====================================================================================================================

// CommandQueue::at 0x100088c0 through its thunk 0x10002257: __thiscall (ring, index), returns ring[index & 0xfff].
unsigned* c3fd_at(void* ring, unsigned idx) {
    return reinterpret_cast<unsigned*>(c3fd_fn<Th1_t>(0x00002257)(ring, 0, idx));
}
// pairClear 0x10008840 (thunk 0x10001816) on the entry, then operator delete, when the entry is non-NULL.
void c3fd_freeEntry(unsigned* e) {
    if (!e) return;
    c3fd_fn<Th0_t>(0x00001816)(e, 0);
    c3fd_delete(e);
}

// The shared body of the two scanCommandRing rows (below). ringOff is the ring's offset in the device (+0x50 Midi, +0x1d8 Wave),
// cmpAll whether entries also compare their dwords +8 and +0xc, flagOff / flagBit the byte tested on the entry's object.
// i walks from *first, j from i + 1; the walk of j for one i ends when j reaches *last or the entry at i is gone; i ends at
// *last. For two live entries with the same object (+4): equal kinds (and, for Midi, equal +8 and +0xc) delete the j entry;
// kinds (1, 2) delete the i entry when its object has the flag bit, else both; kinds (2, 1) delete the i entry when the object
// is non-NULL without the flag bit, else both; anything else keeps both.
unsigned c3fd_scanRing(void* self, unsigned* first, unsigned* last, int ringOff, bool cmpAll, int flagOff, unsigned char flagBit) {
    char* ring = &fat<char>(self, ringOff);
    unsigned i = *first;
    unsigned j = (i + 1) & 0xfff;
    unsigned nextI = j;
    if (i == *last) return 0;
    for (;;) {
        if (j == *last) return 0;
        if (c3fd_at(ring, i)) {
            do {
                if (j == *last) break;
                if (c3fd_at(ring, j)) {
                    unsigned* b = c3fd_at(ring, j);
                    unsigned* a = c3fd_at(ring, i);
                    if (a[1] == b[1]) {
                        if (a[0] == b[0] && (!cmpAll || (a[2] == b[2] && a[3] == b[3]))) {
                            c3fd_freeEntry(c3fd_at(ring, j));
                            fat<unsigned>(self, ringOff + 4 * j) = 0;
                        } else if (c3fd_at(ring, i)[0] == 1 && c3fd_at(ring, j)[0] == 2) {
                            void* o = reinterpret_cast<void*>(c3fd_at(ring, i)[1]);
                            if (o && (fat<unsigned char>(o, flagOff) & flagBit)) {
                                c3fd_freeEntry(c3fd_at(ring, i));
                                fat<unsigned>(self, ringOff + 4 * i) = 0;
                            } else {
                                c3fd_freeEntry(c3fd_at(ring, i));
                                fat<unsigned>(self, ringOff + 4 * i) = 0;
                                c3fd_freeEntry(c3fd_at(ring, j));
                                fat<unsigned>(self, ringOff + 4 * j) = 0;
                            }
                        } else if (c3fd_at(ring, i)[0] == 2 && c3fd_at(ring, j)[0] == 1) {
                            void* o = reinterpret_cast<void*>(c3fd_at(ring, i)[1]);
                            if (o && !(fat<unsigned char>(o, flagOff) & flagBit)) {
                                c3fd_freeEntry(c3fd_at(ring, i));
                                fat<unsigned>(self, ringOff + 4 * i) = 0;
                            } else {
                                c3fd_freeEntry(c3fd_at(ring, i));
                                fat<unsigned>(self, ringOff + 4 * i) = 0;
                                c3fd_freeEntry(c3fd_at(ring, j));
                                fat<unsigned>(self, ringOff + 4 * j) = 0;
                            }
                        }
                    }
                }
                j = (j + 1) & 0xfff;
            } while (c3fd_at(ring, i));
        }
        i = nextI;
        j = (i + 1) & 0xfff;
        nextI = j;
        if (i == *last) return 0;
    }
}

// 0x0000aaf0  Midi_Device::scanCommandRing (VA 0x1000aaf0). __thiscall (first*, last*), `ret 8` (0x1000adb3), returns 0
// (0x1000adad). The ring is the 0x1000 dwords at this+0x50 (`lea esi, [ebx+0x50]` 0x1000ab26), read through CommandQueue::at
// (thunk 0x10002257) and cleared by direct stores `mov [ebx + k*4 + 0x50], 0`. i = *first (0x1000aafb), j = (i + 1) & 0xfff
// (0x1000ab03 / 0x1000ab06), kept as the next i in the first argument's slot (0x1000ab12); i == *last returns at once
// (0x1000ab16 / 0x1000ab18), and so does j == *last at the head of each i pass (0x1000ab1e / 0x1000ab20). With a live entry at i
// (0x1000ab2c / 0x1000ab33) the inner walk runs while j != *last (0x1000ab39-0x1000ab3f) and the i entry stays live (re-tested
// after each step, 0x1000ad74-0x1000ad85); a NULL j entry is skipped (0x1000ab4f). Two entries with equal object dwords +4
// (0x1000ab67-0x1000ab6f) and equal +0, +8 and +0xc (0x1000ab87-0x1000abc5) lose the j entry (pairClear 0x10001816 + operator
// delete when non-NULL, 0x1000abca-0x1000abe2, then the slot is cleared, 0x1000abe9). Otherwise kinds (i, j) = (1, 2)
// (0x1000abfe / 0x1000ac0f) lose the i entry alone when the object has bit 5 of its byte +0xac (0x1000ac20-0x1000ac52), else both
// entries (0x1000ac5f-0x1000ad66); kinds (2, 1) (0x1000acb6 / 0x1000acc7) lose the i entry alone when the object is non-NULL
// without that bit (0x1000acd8-0x1000ad0e), else both (0x1000ad18-0x1000ad66); other kinds keep both (0x1000ad70). Then
// i = the saved next, j = i + 1 (0x1000ad8b-0x1000ada0) until i reaches *last (0x1000ad9e / 0x1000ada4).
typedef unsigned(__fastcall* Scan_t)(void*, void*, unsigned*, unsigned*);
Scan_t MidiScan_orig;
unsigned __fastcall MidiScan_re(void* self, void*, unsigned* first, unsigned* last) {
    return c3fd_scanRing(self, first, last, 0x50, true, 0xac, 0x20);
}

// 0x0000dcf0  Wave_Device::scanCommandRing (VA 0x1000dcf0). The same walk over the ring at this+0x1d8 (0x1000dd26), `ret 8`
// (0x1000df90), returns 0 (0x1000df8a), with two differences: duplicates compare only the object dword +4 (0x1000dd6a-0x1000dd72)
// and the kind +0 (0x1000dd8a-0x1000dd90), and the flag tested on the object is bit 6 of its byte +0xc8 (0x1000ddf5 / 0x1000deb3).
Scan_t WaveScan_orig;
unsigned __fastcall WaveScan_re(void* self, void*, unsigned* first, unsigned* last) {
    return c3fd_scanRing(self, first, last, 0x1d8, false, 0xc8, 0x40);
}

// Pops the head node of the list {head at hOff, tail at hOff + 4, count at cOff}: its next n[1] becomes the head with its back
// link cleared, or with no next head and tail are cleared; the node goes to operator delete and the count is decremented. Returns
// the node's payload n[2], read before the delete.
void* c3fd_popNode(void* self, int hOff, int cOff, void** n) {
    void** next = reinterpret_cast<void**>(n[1]);
    if (next) {
        next[0] = 0;
        fat<void**>(self, hOff) = next;
    } else {
        fat<void*>(self, hOff + 4) = 0;
        fat<void*>(self, hOff) = 0;
    }
    void* payload = n[2];
    c3fd_delete(n);
    --fat<int>(self, cOff);
    return payload;
}
// Pops nodes until the list is empty or a popped node's payload is NULL.
void c3fd_drainList(void* self, int hOff, int cOff) {
    void** n = fat<void**>(self, hOff);
    while (n) {
        if (!c3fd_popNode(self, hOff, cOff, n)) break;
        n = fat<void**>(self, hOff);
    }
}

// 0x00004000  Sound::dtor (VA 0x10004000). __thiscall, plain `ret` (0x10004163); its only caller Sound::scalarDeletingDtor
// 0x10003fd0 returns this (0x10003fe8), so the A/B compares memory (ret void). The vtable 0x1005b000 is stored (0x1000401f). The
// node list head +0xa4c / tail +0xa50 / count +0xa58 is drained twice with the same loop (0x10004025-0x100040b5 and
// 0x100040b7-0x10004141): pop the head (its next becomes the head with the back link cleared, 0x10004039-0x10004042, or head and
// tail are cleared, 0x1000404a / 0x10004050), read the payload +8 (0x10004056), operator delete the node (0x1000405a), decrement
// the count (0x1000405f-0x1000406b), and stop on a NULL payload (0x10004069 / 0x10004071) or an empty list (0x10004079 /
// 0x1000407b); the second pass therefore goes on past the first NULL payload. Then Sound5b6a8_dtor runs on this (thunk
// 0x10001893, 0x1000414d).
typedef void(__fastcall* SoundDtor_t)(void*, void*);
SoundDtor_t SoundDtor_orig;
void __fastcall SoundDtor_re(void* self, void*) {
    fat<void*>(self, 0) = c3fd_sound(0x0005b000);                             // 0x1000401f
    c3fd_drainList(self, 0xa4c, 0xa58);                                       // 0x10004025-0x100040b5
    c3fd_drainList(self, 0xa4c, 0xa58);                                       // 0x100040b7-0x10004141
    c3fd_fn<Th0_t>(0x00001893)(self, 0);                                      // 0x1000414d
}

// 0x000312a0  streamReaderDtor (VA 0x100312a0). __thiscall, plain `ret` (0x100313ee); its only caller StreamReader_scalarDtor
// 0x10031270 returns this (0x10031288), so ret void. The vtable 0x1005c528 is stored (0x100312c0). The list head +0x5c / tail +0x60
// / count +0x68 is popped as in Sound::dtor (0x100312c6-0x100312fd); while the popped payload p is non-NULL (0x100312fb /
// 0x10031300, 0x1003134f / 0x10031354) mmioClose(p[0], 0) runs through [0x100b8670] (0x10031302-0x1003130c), the open-file count
// 0x100b4a08 is decremented (0x1003130e-0x10031316), p goes to operator delete (0x1003131c) and the next head is popped
// (0x10031321-0x10031351), the walk ending on an empty list (0x10031327 / 0x10031329). Then, with the wave-in device 0x100b4a24
// non-NULL (0x10031356-0x1003135e), Wave_In_Device::clearStream runs on it (thunk 0x10001ba4, 0x10031360). The list is then drained
// once more without closing anything (0x10031365-0x100313cb, stopping on a NULL payload) and Ch5c2ec_dtor runs on this (thunk
// 0x10002612, 0x100313d7).
typedef void(__fastcall* SrDtor_t)(void*, void*);
SrDtor_t SrDtor_orig;
void __fastcall SrDtor_re(void* self, void*) {
    typedef unsigned(__stdcall* MmioClose_t)(unsigned, unsigned);
    fat<void*>(self, 0) = c3fd_sound(0x0005c528);                             // 0x100312c0
    void** n = fat<void**>(self, 0x5c);                                       // 0x100312c6
    if (n) {                                                                  // 0x100312cb / 0x100312d5
        void* p = c3fd_popNode(self, 0x5c, 0x68, n);                          // 0x100312d7-0x100312fd
        while (p) {                                                           // 0x100312fb / 0x1003134f
            c3fd_imp<MmioClose_t>(0x000b8670)(*reinterpret_cast<unsigned*>(p), 0);   // 0x10031302-0x1003130c
            --*reinterpret_cast<int*>(c3fd_sound(0x000b4a08));                // 0x1003130e-0x10031316
            c3fd_delete(p);                                                   // 0x1003131c
            n = fat<void**>(self, 0x5c);                                      // 0x10031321
            if (!n) break;                                                    // 0x10031327 / 0x10031329
            p = c3fd_popNode(self, 0x5c, 0x68, n);                            // 0x1003132b-0x10031351
        }
    }
    void* wi = c3fd_gref(kWaveInDevice);                                      // 0x10031356
    if (wi) c3fd_fn<Th0_t>(0x00001ba4)(wi, 0);                                // 0x1003135c-0x10031360
    c3fd_drainList(self, 0x5c, 0x68);                                         // 0x10031365-0x100313cb
    c3fd_fn<Th0_t>(0x00002612)(self, 0);                                      // 0x100313d7
}

// 0x0002f8d0  MultiSound_setSlotSource (VA 0x1002f8d0). __thiscall (index), `ret 4`. A non-NULL +0xfc returns 0xf (0x1002f8ee-0x1002f903);
// index >= 0x28 (unsigned, `cmp eax, 0x28; jb` 0x1002f91e / 0x1002f921) returns 0xa; a NULL slot +0x58 + 4*index (0x1002f93b /
// 0x1002f93f) returns 0x24. The flag word is 0x302 with +0xf8 non-NULL, else 0x102 (0x1002f959-0x1002f970). operator new(0x1450)
// (0x1002f975 / 0x1002f97a); a block is constructed by MultiSound_ctor through the thunk 0x100025ea (0x1002f98e / 0x1002f990), a
// NULL block leaves the object NULL (esi = 0, 0x1002f8ec) and the next call faults (no test). The object's vtable slot +0x6c runs
// with the flag word (0x1002f997-0x1002f9a4), then slot +0x10 with the slot's value (0x1002f9a7-0x1002f9b4); its result r is the
// return value (0x1002f9b7, 0x1002f9f7). r non-zero deletes the object through its slot 0 with 1 pushed (0x1002f9bd-0x1002f9c7;
// the object is never NULL there). r zero: with bit 2 of the object's byte +0xcc (0x1002f9cb / 0x1002f9d2) the constant routine
// 0x10032660 (`mov eax, 0xb; ret`) runs on object+0x70 (thunk 0x10001dde, 0x1002f9d7); the object is stored at +0xfc
// (0x1002f9e4) and, with +0xf8 non-NULL (0x1002f9e0-0x1002f9e6), Channel_setStreamSource(&+0xfc, &+0xf8) runs on +0xf8's object
// +0x70 (thunk 0x1000122b, 0x1002f9e8-0x1002f9ed).
typedef unsigned(__fastcall* SetSlot_t)(void*, void*, unsigned);
SetSlot_t SetSlot_orig;
unsigned __fastcall SetSlot_re(void* self, void*, unsigned index) {
    if (fat<void*>(self, 0xfc)) return 0xf;                                   // 0x1002f8fa-0x1002f903
    if (index >= 0x28) return 0xa;                                            // 0x1002f91e-0x1002f924
    if (!fat<unsigned>(self, 0x58 + 4 * index)) return 0x24;                  // 0x1002f93b-0x1002f942
    unsigned flags = fat<void*>(self, 0xf8) ? 0x302u : 0x102u;                // 0x1002f959-0x1002f970
    void* p = c3fd_new(0x1450);                                               // 0x1002f97a
    void* ms = p ? reinterpret_cast<void*>(c3fd_fn<Th0_t>(0x000025ea)(p, 0)) : 0;   // 0x1002f98c-0x1002f995
    c3fd_vfn<Th1_t>(ms, 0x6c)(ms, 0, flags);                                  // 0x1002f9a4
    unsigned r = c3fd_vfn<Th1_t>(ms, 0x10)(ms, 0, fat<unsigned>(self, 0x58 + 4 * index));   // 0x1002f9a7-0x1002f9b4
    if (r) {                                                                  // 0x1002f9b9 / 0x1002f9bb
        if (ms) c3fd_vfn<Th1_t>(ms, 0)(ms, 0, 1);                             // 0x1002f9bd-0x1002f9c7
        return r;
    }
    if (fat<unsigned char>(ms, 0xcc) & 4)                                     // 0x1002f9cb / 0x1002f9d2
        c3fd_fn<Th0_t>(0x00001dde)(&fat<char>(ms, 0x70), 0);                  // 0x1002f9d7
    fat<void*>(self, 0xfc) = ms;                                              // 0x1002f9e4
    void* src = fat<void*>(self, 0xf8);                                       // 0x1002f9e0
    if (src)                                                                  // 0x1002f9e2 / 0x1002f9e6
        c3fd_fn<Th2_t>(0x0000122b)(&fat<char>(src, 0x70), 0, reinterpret_cast<unsigned>(&fat<char>(self, 0xfc)),
                                   reinterpret_cast<unsigned>(&fat<char>(self, 0xf8)));   // 0x1002f9e8-0x1002f9ed
    return r;                                                                 // 0x1002f9f7
}

// 0x00020be0  Voice_startPlayback (VA 0x10020be0). __thiscall, plain `ret` (0x10020cf0 / 0x10020cf8). Without bit 0 of the byte
// +0xac (0x10020be4 / 0x10020beb) Track_resetReadState runs on this+0x60 (thunk 0x1000239c, 0x10020bf0). Vtable slot +0x5c of this
// non-zero (0x10020bf5-0x10020bfe) or bits 2 / 5 of +0xac (0x10020c04 / 0x10020c0b) return 0xe (0x10020cf1). Otherwise +0x740 = 0
// (0x10020c1b), Track_clearFlag8 runs on this+0x60 (thunk 0x100010be, 0x10020c21); with +0x23c non-zero (0x10020c26 / 0x10020c2c),
// for i < +0x240 (signed, re-read each pass, 0x10020c2e-0x10020c38, 0x10020c5c-0x10020c65) Seq_listGetAt(this+0x60, local,
// +0x238's dword i) runs (0x10020c3a-0x10020c4b) and a non-NULL local gets bit 2 of its byte +0x38 (0x10020c50-0x10020c58); the
// local is the slot of the entry `push ecx`, which holds this until a lookup writes it. Then slot +0x48 with 1 pushed
// (0x10020c67-0x10020c6d), +0x214 |= 4 (0x10020c70-0x10020c7d), Seq_preRollTrack(i) on this (thunk 0x10001956) for i below slot
// +0xb4's result (unsigned, called again each pass, 0x10020c85-0x10020ca4), +0xac |= 0x400 (`or dh, 4` on this+0x60+0x4c,
// 0x10020ca6-0x10020cac), +0x720 / +0x71c / +0x724 = 0 (0x10020caf-0x10020cbb), +0x734 = timeGetTime() (IAT [0x100b8654],
// 0x10020cc1 / 0x10020cc9), and CommandQueue::push(this, 1, 0, 0) on g_midiDevice 0x100b4a1c (thunk 0x100015e1, 0x10020cc7-0x10020cd8);
// a zero result clears bit 3 of +0xac (0x10020cdd-0x10020ce6). 0 is returned (0x10020cec).
typedef unsigned(__fastcall* StartPlay_t)(void*, void*);
StartPlay_t StartPlay_orig;
unsigned __fastcall StartPlay_re(void* self, void*) {
    typedef unsigned(__stdcall* TimeGetTime_t)();
    if (!(fat<unsigned char>(self, 0xac) & 1))                                // 0x10020be4 / 0x10020beb
        c3fd_fn<Th0_t>(0x0000239c)(&fat<char>(self, 0x60), 0);                // 0x10020bf0
    if (c3fd_vfn<Th0_t>(self, 0x5c)(self, 0)) return 0xe;                     // 0x10020bf9-0x10020bfe
    if (fat<unsigned char>(self, 0xac) & 0x24) return 0xe;                    // 0x10020c04 / 0x10020c0b
    fat<unsigned>(self, 0x740) = 0;                                           // 0x10020c1b
    c3fd_fn<Th0_t>(0x000010be)(&fat<char>(self, 0x60), 0);                    // 0x10020c21
    if (fat<unsigned>(self, 0x23c)) {                                         // 0x10020c26 / 0x10020c2c
        void* local = self;                                                   // the `push ecx` slot (0x10020be0)
        for (int i = 0; i < fat<int>(self, 0x240); ++i) {                     // 0x10020c2e-0x10020c38, 0x10020c5c-0x10020c65
            c3fd_listGetAt(&fat<char>(self, 0x60), &local, fat<unsigned*>(self, 0x238)[i]);   // 0x10020c3a-0x10020c4b
            if (local) fat<unsigned char>(local, 0x38) |= 4;                  // 0x10020c50-0x10020c58
        }
    }
    c3fd_vfn<Th1_t>(self, 0x48)(self, 0, 1);                                  // 0x10020c6d
    fat<unsigned>(self, 0x214) |= 4;                                          // 0x10020c70-0x10020c7d
    for (unsigned i = 0; i < c3fd_vfn<Th0_t>(self, 0xb4)(self, 0); ++i)       // 0x10020c85-0x10020ca4
        c3fd_fn<Th1_t>(0x00001956)(self, 0, i);                               // 0x10020c92
    fat<unsigned>(self, 0xac) |= 0x400;                                       // 0x10020ca6-0x10020cac
    fat<unsigned>(self, 0x720) = 0;                                           // 0x10020caf
    fat<unsigned>(self, 0x71c) = 0;                                           // 0x10020cb5
    fat<unsigned>(self, 0x724) = 0;                                           // 0x10020cbb
    fat<unsigned>(self, 0x734) = c3fd_imp<TimeGetTime_t>(0x000b8654)();       // 0x10020cc1 / 0x10020cc9
    unsigned r = c3fd_fn<Th4_t>(0x000015e1)(c3fd_gref(kMidiDevice), 0, reinterpret_cast<unsigned>(self), 1, 0, 0);   // 0x10020cc7-0x10020cd8
    if (!r) fat<unsigned>(self, 0xac) &= ~8u;                                 // 0x10020cdd-0x10020ce6
    return 0;                                                                 // 0x10020cec
}

// 0x00024040  Seq_dispatchChannelCmd (VA 0x10024040). __thiscall (cmd, kind, value), `ret 0xc`. eax is preset to 0xa (0x1002404a):
// a NULL cmd returns it (0x10024048 / 0x1002404f). kind 2 (0x1002405a / 0x1002405d) returns Seq_updateChannel(cmd) on this (thunk
// 0x10001550, 0x10024062). Otherwise cmd+0x1c above 5 (0x1002406c-0x10024072) returns 0xa, and the table 0x10024178 picks: 0 ->
// Seq_updateChannel(cmd) (0x1002407f); 1 -> n = cmd+0x28 (0x1002408f): n >= slot +0xb4's result (0x10024094-0x1002409c) returns 0xa,
// else Seq_listGetAt(this+0x60, slot, n) with the cmd argument's slot as the out slot (0x1002409e-0x100240a7, result unused); the
// track t it holds gets byte +0x38 = (old & 0xef) | ((cmd byte +0x2c & 2) << 3) (0x100240ac-0x100240c3) and t+0x460 = cmd+0x30
// (0x100240c6-0x100240ce), 0 returned; 2 -> the record at this + 0x264 + 0x4c*(cmd+0x28) gets cmd's dwords +4 .. +0x14, word +0x18
// and dwords +0x1c .. +0x34 (0x100240e4-0x10024142), 0 returned; 3 and 5 -> kind non-zero (0x1002414b / 0x1002414d) or value < 0
// (signed, 0x10024153 / 0x10024155) return 0xa, else Seq_listGetAt(this+0x60, slot, value) with the kind argument's slot as the out
// slot (0x10024157-0x10024160): a non-zero result is returned (0x10024165 / 0x10024167), zero returns Ctrl_appendIfAccepted(cmd) on
// the track found (thunk 0x10001654, 0x10024169-0x1002416e); 4 -> 0xa (0x10024173).
typedef unsigned(__fastcall* Dispatch_t)(void*, void*, unsigned char*, unsigned, int);
Dispatch_t Dispatch_orig;
unsigned __fastcall Dispatch_re(void* self, void*, unsigned char* cmd, unsigned kind, int value) {
    if (!cmd) return 0xa;                                                     // 0x10024048-0x1002404f
    unsigned c = reinterpret_cast<unsigned>(cmd);
    if (kind == 2) return c3fd_fn<Th1_t>(0x00001550)(self, 0, c);             // 0x1002405a-0x10024062
    unsigned t = *reinterpret_cast<unsigned*>(cmd + 0x1c);                    // 0x1002406c
    if (t > 5) return 0xa;                                                    // 0x1002406f / 0x10024072
    switch (t) {                                                              // table 0x10024178
    case 0:                                                                   // 0x1002407f
        return c3fd_fn<Th1_t>(0x00001550)(self, 0, c);
    case 1: {                                                                 // 0x1002408c
        unsigned n = *reinterpret_cast<unsigned*>(cmd + 0x28);                // 0x1002408f
        if (n >= c3fd_vfn<Th0_t>(self, 0xb4)(self, 0)) return 0xa;            // 0x10024094-0x1002409c
        void* slot = cmd;                                                     // the cmd argument's slot
        c3fd_listGetAt(&fat<char>(self, 0x60), &slot, n);                     // 0x100240a7
        unsigned char b = fat<unsigned char>(slot, 0x38);                     // 0x100240b7
        fat<unsigned char>(slot, 0x38) = static_cast<unsigned char>(((cmd[0x2c] & 2) << 3) | (b & 0xef));   // 0x100240b0-0x100240c3
        fat<unsigned>(slot, 0x460) = *reinterpret_cast<unsigned*>(cmd + 0x30);   // 0x100240c6-0x100240ce
        return 0;
    }
    case 2: {                                                                 // 0x100240e4
        char* d = &fat<char>(self, 0x264 + 0x4c * *reinterpret_cast<unsigned*>(cmd + 0x28));   // 0x100240e4-0x100240f0
        for (int k = 4; k < 0x18; k += 4) fat<unsigned>(d, k) = *reinterpret_cast<unsigned*>(cmd + k);   // 0x100240ed-0x10024110
        fat<unsigned short>(d, 0x18) = *reinterpret_cast<unsigned short*>(cmd + 0x18);   // 0x10024113 / 0x10024117
        for (int k = 0x1c; k < 0x38; k += 4) fat<unsigned>(d, k) = *reinterpret_cast<unsigned*>(cmd + k);   // 0x1002411b-0x10024142
        return 0;
    }
    case 3:
    case 5: {                                                                 // 0x1002414b
        if (kind) return 0xa;                                                 // 0x1002414b / 0x1002414d
        if (value < 0) return 0xa;                                            // 0x10024153 / 0x10024155
        void* slot = reinterpret_cast<void*>(kind);                           // the kind argument's slot
        unsigned r = c3fd_listGetAt(&fat<char>(self, 0x60), &slot, static_cast<unsigned>(value));   // 0x10024160
        if (r) return r;                                                      // 0x10024165 / 0x10024167
        return c3fd_fn<Th1_t>(0x00001654)(slot, 0, c);                        // 0x10024169-0x1002416e
    }
    default:                                                                  // case 4 -> 0x10024173
        return 0xa;
    }
}

// A COM method: __stdcall with the interface pointer pushed first (`push eax; call [ecx + off]` with ecx = the vtable).
template <typename F> F c3fd_com(void* itf, int off) { return *reinterpret_cast<F*>(*reinterpret_cast<char**>(itf) + off); }
typedef long(__stdcall* Com1_t)(void*, void*);
typedef long(__stdcall* Com2_t)(void*, void*, void*);
typedef long(__stdcall* Com3_t)(void*, void*, void*, void*);

// 0x0000fbb0  DsBuffer::create (VA 0x1000fbb0). __thiscall (ds, desc), `ret 8` (args at [esp+0x10] / [esp+0x14] after three pushes).
// A NULL desc returns 0xa (0x1000fbb3-0x1000fbbf). The format pointer +0x80: with bit 1 of the dword +0 (`and eax, 2; cmp al, 2`
// 0x1000fbc8-0x1000fbcf) it is cleared (0x1000fbd1); else a NULL one is pointed at this+0xa8 (0x1000fbdd-0x1000fbed), and a non-NULL
// one has its 0x12 bytes (four dwords and a word) copied to this+0xa8 (0x1000fbf5-0x1000fc19). A non-NULL buffer +0x60 returns 0xc
// (0x1000fc1d-0x1000fc29). ds's method +0xc (CreateSoundBuffer) runs with (this+0x70, &+0x60, 0) (0x1000fc32-0x1000fc40). A failure
// code is mapped (0x1000fc43-0x1000fcd9, signed compares): 0x8878000a -> 0xc, 0x8007000e -> 0x11, 0x80004001 -> 0xb, 0x80070057 ->
// 0xa, and for codes above 0x8878000a the byte table 0x1000fdd4 over code - 0x88780064 (0..0x46, `ja` 0x1000fc94) picks from
// 0x1000fdc0: 0x88780064 -> 0x12, 0x88780078 -> 1, 0x88780096 -> 0x1e, 0x887800aa -> 0x13; every other code -> 0x2f. On success
// +0x50 = desc (0x1000fcdc), +0x94 = 0x14 (0x1000fcea), and a non-zero +0x78 = n sets +0x58 = (~n) % n (unsigned `div`,
// 0x1000fcf2-0x1000fcfa). The buffer's method +0xc (GetCaps) runs with this+0x94 (0x1000fcfd-0x1000fd03; its result is kept in the
// desc argument's slot and not read again). With bit 1 of +0 (0x1000fd0a-0x1000fd11) method +0x38 (SetFormat) runs with this+0xa8
// and method +0x30 (Play) with (0, 0, 1) (0x1000fd13-0x1000fd2d). With bit 4 of the byte +0x74 (0x1000fd30 / 0x1000fd34): a
// non-NULL buffer (0x1000fd36-0x1000fd3a) is queried through method 0 for the IID at 0x1005d58c into +0x68 when bit 1 of +0 is
// set, else for 0x1005d57c into +0x64 (0x1000fd3c-0x1000fd5a); a negative result clears +0x68 and +0x64 (0x1000fd5c-0x1000fd67).
// Then +0x68 when non-NULL, else +0x64 when non-NULL (0x1000fd6e-0x1000fd8a), is queried for the IID at 0x1005d52c into +0x6c
// (0x1000fd75-0x1000fd98), a negative result clearing +0x6c (0x1000fd9a-0x1000fd9e). Finally the buffer's method +0x14
// (GetFormat) runs with (this+0xa8, 0x12, 0) (0x1000fda4-0x1000fdb4) and 0 is returned (0x1000fdb9).
typedef unsigned(__fastcall* DsCreate_t)(void*, void*, void*, void*);
DsCreate_t DsCreate_orig;
unsigned c3fd_mapHr(long hr) {
    if (hr > static_cast<long>(0x8878000aL)) {                                // 0x1000fc4b / 0x1000fc50
        unsigned idx = static_cast<unsigned>(hr) + 0x7787ff9cu;               // 0x1000fc8c
        if (idx > 0x46) return 0x2f;                                          // 0x1000fc91 / 0x1000fc94
        unsigned c = *reinterpret_cast<unsigned char*>(c3fd_sound(0x0000fdd4 + idx));   // 0x1000fc98
        static const unsigned kMap[5] = {0x12, 1, 0x1e, 0x13, 0x2f};          // targets of 0x1000fdc0 (0x1000fcb0 / fca5 / fcc6 / fcbb / fcd1)
        return kMap[c];
    }
    if (hr == static_cast<long>(0x8878000aL)) return 0xc;                     // 0x1000fc52
    if (hr > static_cast<long>(0x8007000eL))                                  // 0x1000fc54 / 0x1000fc59
        return hr == static_cast<long>(0x80070057L) ? 0xa : 0x2f;             // 0x1000fc7a-0x1000fc83 / 0x1000fcd1
    if (hr == static_cast<long>(0x8007000eL)) return 0x11;                    // 0x1000fc5b / 0x1000fc71
    if (hr == static_cast<long>(0x80004001L)) return 0xb;                     // 0x1000fc5d-0x1000fc66
    return 0x2f;                                                              // 0x1000fcd3
}
unsigned __fastcall DsCreate_re(void* self, void*, void* ds, void* desc) {
    if (!desc) return 0xa;                                                    // 0x1000fbb7-0x1000fbbf
    if ((fat<unsigned>(self, 0) & 2) == 2) {                                  // 0x1000fbc8-0x1000fbcf
        fat<void*>(self, 0x80) = 0;                                           // 0x1000fbd1
    } else if (!fat<void*>(self, 0x80)) {                                     // 0x1000fbdd-0x1000fbe5
        fat<void*>(self, 0x80) = &fat<char>(self, 0xa8);                      // 0x1000fbe7 / 0x1000fbed
    } else {
        char* f = fat<char*>(self, 0x80);
        for (int k = 0; k < 0x10; k += 4) fat<unsigned>(self, 0xa8 + k) = fat<unsigned>(f, k);   // 0x1000fbf5-0x1000fc12
        fat<unsigned short>(self, 0xb8) = fat<unsigned short>(f, 0x10);       // 0x1000fc15 / 0x1000fc19
    }
    if (fat<void*>(self, 0x60)) return 0xc;                                   // 0x1000fc1d-0x1000fc29
    long hr = c3fd_com<Com3_t>(ds, 0xc)(ds, &fat<char>(self, 0x70), &fat<char>(self, 0x60), 0);   // 0x1000fc32-0x1000fc40
    if (hr) return c3fd_mapHr(hr);                                            // 0x1000fc43 / 0x1000fc45
    fat<void*>(self, 0x50) = desc;                                            // 0x1000fcdc
    unsigned n = fat<unsigned>(self, 0x78);                                   // 0x1000fcdf
    fat<unsigned>(self, 0x94) = 0x14;                                         // 0x1000fcea
    if (n) fat<unsigned>(self, 0x58) = ~n % n;                                // 0x1000fce8-0x1000fcfa
    void* buf = fat<void*>(self, 0x60);                                       // 0x1000fcfd
    c3fd_com<Com1_t>(buf, 0xc)(buf, &fat<char>(self, 0x94));                  // 0x1000fcff-0x1000fd03
    if ((fat<unsigned>(self, 0) & 2) == 2) {                                  // 0x1000fd0a-0x1000fd11
        buf = fat<void*>(self, 0x60);                                         // 0x1000fd13
        c3fd_com<Com1_t>(buf, 0x38)(buf, &fat<char>(self, 0xa8));             // 0x1000fd15-0x1000fd1f
        buf = fat<void*>(self, 0x60);                                         // 0x1000fd22
        c3fd_com<Com3_t>(buf, 0x30)(buf, 0, 0, reinterpret_cast<void*>(1));   // 0x1000fd24-0x1000fd2d
    }
    if (fat<unsigned char>(self, 0x74) & 0x10) {                              // 0x1000fd30 / 0x1000fd34
        buf = fat<void*>(self, 0x60);                                         // 0x1000fd36
        if (buf) {                                                            // 0x1000fd38 / 0x1000fd3a
            long q = (fat<unsigned char>(self, 0) & 2)                        // 0x1000fd3c-0x1000fd43
                ? c3fd_com<Com2_t>(buf, 0)(buf, c3fd_sound(0x0005d58c), &fat<char>(self, 0x68))   // 0x1000fd45-0x1000fd5a
                : c3fd_com<Com2_t>(buf, 0)(buf, c3fd_sound(0x0005d57c), &fat<char>(self, 0x64));  // 0x1000fd50-0x1000fd5a
            if (q < 0) {                                                      // 0x1000fd5c / 0x1000fd5e
                fat<void*>(self, 0x68) = 0;                                   // 0x1000fd60
                fat<void*>(self, 0x64) = 0;                                   // 0x1000fd67
            }
        }
        void* o = fat<void*>(self, 0x68);                                     // 0x1000fd6e
        if (!o) o = fat<void*>(self, 0x64);                                   // 0x1000fd71-0x1000fd85
        if (o) {                                                              // 0x1000fd88 / 0x1000fd8a
            long q = c3fd_com<Com2_t>(o, 0)(o, c3fd_sound(0x0005d52c), &fat<char>(self, 0x6c));   // 0x1000fd75-0x1000fd98
            if (q < 0) fat<void*>(self, 0x6c) = 0;                            // 0x1000fd9a-0x1000fd9e
        }
    }
    buf = fat<void*>(self, 0x60);                                             // 0x1000fda4
    c3fd_com<Com3_t>(buf, 0x14)(buf, &fat<char>(self, 0xa8), reinterpret_cast<void*>(0x12), 0);   // 0x1000fda6-0x1000fdb4
    return 0;                                                                 // 0x1000fdb9
}

// =====================================================================================================================
// E. third group: channel buffer release / teardown, the MIDI command drain and the MIDI device shutdown
// =====================================================================================================================

// A module-global node list {prev, next, payload} given by the RVAs of its head, tail, cursor and count words. Finds the first
// node whose payload equals key (walking next links from the head), unlinks it (the predecessor's next or the head; the
// successor's prev, with the cursor set to the successor, or with no successor the cursor cleared and the tail set to the
// predecessor), passes it to operator delete and decrements the count. Nothing happens when no node matches.
void c3fd_unlinkGlobal(unsigned headRva, unsigned tailRva, unsigned curRva, unsigned cntRva, void* key) {
    void**& head = *reinterpret_cast<void***>(c3fd_sound(headRva));
    void** n = head;
    while (n && n[2] != key) n = reinterpret_cast<void**>(n[1]);
    if (!n) return;
    void** prev = reinterpret_cast<void**>(n[0]);
    void** next = reinterpret_cast<void**>(n[1]);
    if (prev) prev[1] = next; else head = next;
    if (next) {
        next[0] = n[0];
        *reinterpret_cast<void**>(c3fd_sound(curRva)) = next;
    } else {
        *reinterpret_cast<void**>(c3fd_sound(curRva)) = 0;
        *reinterpret_cast<void**>(c3fd_sound(tailRva)) = n[0];
    }
    c3fd_delete(n);
    --*reinterpret_cast<int*>(c3fd_sound(cntRva));
}
// The same unlink on a list held in an object (head at hOff, tail hOff + 4, cursor hOff + 8, count hOff + 0xc).
void c3fd_unlinkLocal(void* self, int hOff, void* key) {
    void** n = fat<void**>(self, hOff);
    while (n && n[2] != key) n = reinterpret_cast<void**>(n[1]);
    if (!n) return;
    void** prev = reinterpret_cast<void**>(n[0]);
    void** next = reinterpret_cast<void**>(n[1]);
    if (prev) prev[1] = next; else fat<void**>(self, hOff) = next;
    if (next) {
        next[0] = n[0];
        fat<void**>(self, hOff + 8) = next;
    } else {
        fat<void*>(self, hOff + 8) = 0;
        fat<void*>(self, hOff + 4) = n[0];
    }
    c3fd_delete(n);
    --fat<int>(self, hOff + 0xc);
}
typedef unsigned(__stdcall* MmioClose_t)(unsigned, unsigned);
typedef void(__cdecl* Free_t)(void*);
void c3fd_free(void* p) { c3fd_fn<Free_t>(0x00042c07)(p); }                  // free 0x10042c07
void c3fd_mmioClose(unsigned h) { c3fd_imp<MmioClose_t>(0x000b8670)(h, 0); }  // mmioClose through [0x100b8670]
// the open-mmio list of the module: head 0x100b49c8, tail 0x100b49cc, cursor 0x100b49d0, count 0x100b49d4; open count 0x100b4a08
void c3fd_dropMmio(void* key) {
    c3fd_unlinkGlobal(0x000b49c8, 0x000b49cc, 0x000b49d0, 0x000b49d4, key);
    --*reinterpret_cast<int*>(c3fd_sound(0x000b4a08));
}

// 0x000340b0  Channel_releaseBuffers (VA 0x100340b0). __thiscall, plain `ret` (0x10034147 / 0x1003435f), returns 0 or 0xb. Without
// bit 1 of the byte +0x58 (0x100340b8 / 0x100340bc) it only resets the fields below. With it, bit 2 of the byte +0x5c returns 0xb
// (0x100340c2-0x100340cb). A DirectSound buffer at +0x54 (0x100340d2 / 0x100340d5): the list head +0x1338 / tail +0x133c / count
// +0x1344 is popped (as in Sound::dtor, 0x100340dd-0x10034119) and each non-NULL payload is passed to
// DirectSoundDevice::removeBuffer on g_waveDevice 0x100b49f0 (read once, 0x100340d7; thunk 0x10001677, 0x10034121-0x10034124), the
// walk ending on a NULL payload or an empty list; then removeBuffer(+0x54) (0x1003412b-0x10034131), +0x54 = 0, +0x58 loses bit 1,
// and 0 is returned (0x10034136-0x10034144). Otherwise (an mmio stream): mmioClose(+0, 0) through [0x100b8670] (0x10034148-0x1003414c),
// the node whose payload is this+0xc is unlinked from the module's open-mmio list 0x100b49c8 (0x10034152-0x100341b7), the open
// count 0x100b4a08 is decremented (0x100341bc-0x100341c8) and the 0x12 dwords at this+0xc are zeroed (0x100341ce-0x100341d5). Without
// bit 0 of +0x58 (0x100341d7 / 0x100341db): the second stream +8 is closed (mmioClose(+4, 0) 0x100341e1-0x100341ec, its buffer
// pointer +8's +0x18 read first, 0x100341e9), unlinked from the same list again (key this+0xc, 0x100341f2-0x10034254), the open count
// decremented (0x10034259-0x10034260), that buffer freed (free 0x10042c07, 0x10034265), +8 deleted and cleared (0x1003426a-0x10034276).
// A non-NULL +0x1b8 is deleted and cleared (0x10034279-0x1003428c). The list head +0x1348 / tail +0x134c / count +0x1354 is popped
// and each non-NULL payload deleted, ending on a NULL payload or an empty list (0x10034292-0x10034329). Channel_resetPanState runs
// on this (thunk 0x10001866, 0x1003432d). Every path but the 0xb and +0x54 returns ends at 0x10034332: +0, +4, +0x168, +0x174,
// +0x170 = 0 (0x1003433f-0x10034350), the 0x440 dwords from +0x238 zeroed (0x10034332-0x10034356), +8 = 0 (0x10034358), 0 returned
// (eax zeroed at 0x1003433d).
typedef unsigned(__fastcall* RelBufs_t)(void*, void*);
RelBufs_t RelBufs_orig;
unsigned __fastcall RelBufs_re(void* self, void*) {
    if (fat<unsigned char>(self, 0x58) & 2) {                                 // 0x100340b8 / 0x100340bc
        if (fat<unsigned char>(self, 0x5c) & 4) return 0xb;                   // 0x100340c2-0x100340cb
        if (fat<void*>(self, 0x54)) {                                         // 0x100340d2 / 0x100340d5
            void* dev = c3fd_gref(0x000b49f0);                                // 0x100340d7
            void** n = fat<void**>(self, 0x1338);                             // 0x100340dd
            while (n) {                                                       // 0x100340e3 / 0x100340e5
                void* p = c3fd_popNode(self, 0x1338, 0x1344, n);              // 0x100340e7-0x10034119
                if (!p) break;                                                // 0x1003411f
                c3fd_fn<Th1_t>(0x00001677)(dev, 0, reinterpret_cast<unsigned>(p));   // 0x10034121-0x10034124
                n = fat<void**>(self, 0x1338);
            }
            c3fd_fn<Th1_t>(0x00001677)(dev, 0, fat<unsigned>(self, 0x54));    // 0x1003412b-0x10034131
            fat<unsigned>(self, 0x54) = 0;                                    // 0x10034139
            fat<unsigned>(self, 0x58) &= ~2u;                                 // 0x10034136-0x1003413f
            return 0;                                                         // 0x10034144
        }
        c3fd_mmioClose(fat<unsigned>(self, 0));                               // 0x10034148-0x1003414c
        c3fd_dropMmio(&fat<char>(self, 0xc));                                 // 0x10034152-0x100341c8
        for (int k = 0; k < 0x12; ++k) fat<unsigned>(self, 0xc + 4 * k) = 0;  // 0x100341ce-0x100341d5
        if (!(fat<unsigned char>(self, 0x58) & 1)) {                          // 0x100341d7 / 0x100341db
            void* buf = fat<void*>(fat<void*>(self, 8), 0x18);                // 0x100341e1 / 0x100341e9
            c3fd_mmioClose(fat<unsigned>(self, 4));                           // 0x100341e4-0x100341ec
            c3fd_dropMmio(&fat<char>(self, 0xc));                             // 0x100341f2-0x10034260
            c3fd_free(buf);                                                   // 0x10034265
            c3fd_delete(fat<void*>(self, 8));                                 // 0x1003426a-0x1003426e
            fat<void*>(self, 8) = 0;                                          // 0x10034276
        }
        if (fat<void*>(self, 0x1b8)) {                                        // 0x10034279-0x10034281
            c3fd_delete(fat<void*>(self, 0x1b8));                             // 0x10034284
            fat<void*>(self, 0x1b8) = 0;                                      // 0x1003428c
        }
        void** n = fat<void**>(self, 0x1348);                                 // 0x10034292
        while (n) {                                                           // 0x10034298 / 0x100342eb
            void* p = c3fd_popNode(self, 0x1348, 0x1354, n);                  // 0x100342a0-0x100342d2
            if (!p) break;                                                    // 0x100342da
            c3fd_delete(p);                                                   // 0x100342dc / 0x100342dd
            n = fat<void**>(self, 0x1348);                                    // 0x100342e2
        }
        c3fd_fn<Th0_t>(0x00001866)(self, 0);                                  // 0x1003432d
    }
    fat<unsigned>(self, 0) = 0;                                               // 0x1003433f
    fat<unsigned>(self, 4) = 0;                                               // 0x10034341
    fat<unsigned>(self, 0x168) = 0;                                           // 0x10034344
    fat<unsigned>(self, 0x174) = 0;                                           // 0x1003434a
    fat<unsigned>(self, 0x170) = 0;                                           // 0x10034350
    for (int k = 0; k < 0x440; ++k) fat<unsigned>(self, 0x238 + 4 * k) = 0;   // 0x10034332-0x10034356
    fat<unsigned>(self, 8) = 0;                                               // 0x10034358
    return 0;                                                                 // 0x1003433d
}

// 0x00033c70  Channel_closeAndFree (VA 0x10033c70). __thiscall, plain `ret` (0x10033ec1); its caller MultiSound_dtor 0x10039870 makes
// another call right after it (0x1003991e), so ret void. A non-zero +0x13a4 is cleared (0x10033c8f-0x10033ca4). With bit 1 of the
// byte +0x58 (`test al, cl` with al = 2, 0x10033cb4 / 0x10033cb6) and a non-zero +0 (0x10033cbc-0x10033cc0): mmioClose(+0, 0)
// (0x10033cc6-0x10033cc8), this+0xc's node unlinked from the open-mmio list 0x100b49c8 (0x10033cca-0x10033d2f), the open count
// 0x100b4a08 decremented (0x10033d34-0x10033d3b) and bit 1 of +0x58 cleared (0x10033d41-0x10033d46). A second stream +8 that is
// non-NULL and not this+0xc (0x10033d49-0x10033d55) is closed with mmioClose(its +0x44, 0) (0x10033d57-0x10033d5f), deleted
// (0x10033d61-0x10033d65), its buffer +0x18 (read before the close, 0x10033d5a) freed (free 0x10042c07, 0x10033d6b) and +8 cleared
// (0x10033d73). The lists +0x1348 (tail +0x134c, count +0x1354) and +0x1338 (+0x133c, +0x1344) are drained in that order, each
// stopping on a NULL payload or an empty list, payloads not freed (0x10033d76-0x10033e02, 0x10033e04-0x10033e8e). The eh vector
// destructor 0x1004280e destroys the 0x10 slots of 0x110 bytes at +0x238 with the element destructor thunk 0x10001b45
// (0x10033e90-0x10033eab).
typedef void(__fastcall* CloseFree_t)(void*, void*);
CloseFree_t CloseFree_orig;
void __fastcall CloseFree_re(void* self, void*) {
    typedef void(__stdcall* VecDtor_t)(void*, unsigned, int, void*);
    if (fat<unsigned>(self, 0x13a4)) fat<unsigned>(self, 0x13a4) = 0;        // 0x10033c8f-0x10033ca4
    if ((fat<unsigned char>(self, 0x58) & 2) && fat<unsigned>(self, 0)) {     // 0x10033cb4-0x10033cc0
        c3fd_mmioClose(fat<unsigned>(self, 0));                               // 0x10033cc6-0x10033cc8
        c3fd_dropMmio(&fat<char>(self, 0xc));                                 // 0x10033cca-0x10033d3b
        fat<unsigned>(self, 0x58) &= ~2u;                                     // 0x10033d41-0x10033d46
    }
    void* s = fat<void*>(self, 8);                                            // 0x10033d49
    if (s && s != &fat<char>(self, 0xc)) {                                    // 0x10033d4c-0x10033d55
        void* buf = fat<void*>(s, 0x18);                                      // 0x10033d5a
        c3fd_mmioClose(fat<unsigned>(s, 0x44));                               // 0x10033d57-0x10033d5f
        c3fd_delete(fat<void*>(self, 8));                                     // 0x10033d61-0x10033d65
        c3fd_free(buf);                                                       // 0x10033d6b
        fat<void*>(self, 8) = 0;                                              // 0x10033d73
    }
    c3fd_drainList(self, 0x1348, 0x1354);                                     // 0x10033d76-0x10033e02
    c3fd_drainList(self, 0x1338, 0x1344);                                     // 0x10033e04-0x10033e8e
    c3fd_fn<VecDtor_t>(0x0004280e)(&fat<char>(self, 0x238), 0x110, 0x10, c3fd_sound(0x00001b45));   // 0x10033e90-0x10033eab
}

// 0x0000a330  Midi_Device::drainCommandQueue (VA 0x1000a330). __thiscall, plain `ret`, returns 0 (0x1000a350 / 0x1000a581). h = +0x4050
// and t = +0x4054 are copied to locals (0x1000a337-0x1000a349); equal returns at once (0x1000a34d). n = (t - h) & 0xfff (0x1000a357 /
// 0x1000a359); a non-zero n first runs Midi_Device::scanCommandRing(&h, &t) on this (thunk 0x1000196f, 0x1000a361-0x1000a36d) and
// re-reads h (0x1000a372). Then n entries are visited from (h + 1) & 0xfff (0x1000a376-0x1000a381, `jbe` 0x1000a385; wrap
// 0x1000a555-0x1000a56e): a NULL ring slot this+0x50 + 4*i is skipped (0x1000a390-0x1000a396). An entry e's kind e[0] - 1 above 10
// (0x1000a39c-0x1000a3a2) only clears the slot (0x1000a54d); the table 0x1000a588 maps 1 -> free e; 2 -> Voice_allNotesOff(0) on
// e[1] (thunk 0x10001dd9), bit 5 of its +0xac cleared, its vtable slot +0xb8 (0x1000a3af-0x1000a3cd), free e; 3 -> e[1]'s node
// unlinked from the device list +0x40 (head; tail +0x44, cursor +0x48, count +0x4c; 0x1000a3d8-0x1000a42d), Voice_allNotesOff(0),
// bit 5 of +0xac cleared, Seq_closeFile on it (thunk 0x1000246e) (0x1000a430-0x1000a449), free e; 4 -> a NULL e[1] clears the slot;
// with bit 5 of its +0xac its node is unlinked from +0x40 and the bit cleared (0x1000a45e-0x1000a4c4); its vtable slot 0 runs with 1
// (0x1000a4ca-0x1000a4d0), free e; 5, 6 -> the slot is cleared; 7 -> Seq_cmdHandler7 on e[1] (thunk 0x10001186) when non-NULL, free e;
// 8 -> Seq_cmdHandler9(e[2]) and Seq_cmdHandler8(e[2]) on e[1] (thunks 0x10001474 / 0x100025c2) when non-NULL, free e; 9 ->
// Seq_cmdHandler9(e[2]) on e[1], free e; 10 / 11 -> Seq_cmdHandler10 / 11(e[2], e[3]) on e[1] (thunks 0x100023e7 / 0x100011db) when
// non-NULL, the entry NOT freed. "free e" is pairClear (thunk 0x10001816) + operator delete (0x1000a511-0x1000a519); every visited
// non-NULL slot is cleared (0x1000a54d-0x1000a551), a NULL e[1] also. Finally +0x4050 = the tail local (0x1000a575-0x1000a57a).
typedef unsigned(__fastcall* Drain_t)(void*, void*);
Drain_t Drain_orig;
unsigned __fastcall Drain_re(void* self, void*) {
    unsigned h = fat<unsigned>(self, 0x4050);                                 // 0x1000a337
    unsigned t = fat<unsigned>(self, 0x4054);                                 // 0x1000a33d
    if (h == t) return 0;                                                     // 0x1000a343 / 0x1000a34d
    unsigned n = (t - h) & 0xfff;                                             // 0x1000a357 / 0x1000a359
    if (n) {                                                                  // 0x1000a35f
        c3fd_fn<Th2_t>(0x0000196f)(self, 0, reinterpret_cast<unsigned>(&h), reinterpret_cast<unsigned>(&t));   // 0x1000a36d
    }
    unsigned idx = (h + 1) & 0xfff;                                           // 0x1000a372-0x1000a37a
    for (; n; --n) {                                                          // 0x1000a37f / 0x1000a385 / 0x1000a565 / 0x1000a56e
        unsigned* e = fat<unsigned*>(self, 0x50 + 4 * idx);                   // 0x1000a390
        if (e) {                                                              // 0x1000a394 / 0x1000a396
            bool freeIt = true;
            void* o = reinterpret_cast<void*>(e[1]);
            switch (e[0] - 1) {                                               // 0x1000a39c-0x1000a3a8, table 0x1000a588
            case 0:                                                           // kind 1 -> 0x1000a511
                break;
            case 1:                                                           // kind 2 -> 0x1000a3af
                c3fd_fn<Th1_t>(0x00001dd9)(o, 0, 0);
                fat<unsigned>(o, 0xac) &= 0xffffffdfu;
                c3fd_vfn<Th0_t>(o, 0xb8)(o, 0);
                break;
            case 2:                                                           // kind 3 -> 0x1000a3d8
                c3fd_unlinkLocal(self, 0x40, o);
                c3fd_fn<Th1_t>(0x00001dd9)(o, 0, 0);
                fat<unsigned>(o, 0xac) &= 0xffffffdfu;
                c3fd_fn<Th0_t>(0x0000246e)(o, 0);
                break;
            case 3:                                                           // kind 4 -> 0x1000a453
                if (!o) { freeIt = false; break; }                            // 0x1000a456 / 0x1000a458
                if (fat<unsigned char>(o, 0xac) & 0x20) {                     // 0x1000a45e / 0x1000a465
                    c3fd_unlinkLocal(self, 0x40, o);                          // 0x1000a467-0x1000a4b9
                    fat<unsigned char>(o, 0xac) &= 0xdf;                      // 0x1000a4bc-0x1000a4c4
                }
                c3fd_vfn<Th1_t>(o, 0)(o, 0, 1);                               // 0x1000a4ca-0x1000a4d0
                break;
            case 6:                                                           // kind 7 -> 0x1000a4d4
                if (!o) { freeIt = false; break; }
                c3fd_fn<Th0_t>(0x00001186)(o, 0);
                break;
            case 7:                                                           // kind 8 -> 0x1000a4e2
                if (!o) { freeIt = false; break; }
                c3fd_fn<Th1_t>(0x00001474)(o, 0, e[2]);
                c3fd_fn<Th1_t>(0x000025c2)(o, 0, e[2]);
                break;
            case 8:                                                           // kind 9 -> 0x1000a501
                if (!o) { freeIt = false; break; }
                c3fd_fn<Th1_t>(0x00001474)(o, 0, e[2]);
                break;
            case 9:                                                           // kind 10 -> 0x1000a523
                if (o) c3fd_fn<Th2_t>(0x000023e7)(o, 0, e[2], e[3]);
                freeIt = false;
                break;
            case 10:                                                          // kind 11 -> 0x1000a539
                if (o) c3fd_fn<Th2_t>(0x000011db)(o, 0, e[2], e[3]);
                freeIt = false;
                break;
            default:                                                          // kinds 5, 6 and above 11 -> 0x1000a54d
                freeIt = false;
                break;
            }
            if (freeIt) {                                                     // 0x1000a511-0x1000a519
                c3fd_fn<Th0_t>(0x00001816)(e, 0);
                c3fd_delete(e);
            }
            fat<unsigned>(self, 0x50 + 4 * idx) = 0;                          // 0x1000a54d-0x1000a551
        }
        idx = (idx + 1) & 0xfff;                                              // 0x1000a555-0x1000a566
    }
    fat<unsigned>(self, 0x4050) = t;                                          // 0x1000a575-0x1000a57a
    return 0;                                                                 // 0x1000a581
}

// 0x00009e40  Midi_Device::shutdown (VA 0x10009e40). __thiscall, plain `ret`, returns 0 (0x10009fd7). With a timer at +0x2c
// (0x10009e47-0x10009e4c) SoundTimer::killEvent runs on it (thunk 0x100026fd) and SoundTimer::unregisterClient(this) on g_soundTimer
// 0x100b49ec (thunk 0x10001ed3) (0x10009e4e-0x10009e5a). The cursor 0x100b5008 of the track list (head 0x100b5000, tail 0x100b5004,
// count 0x100b500c) is set to the head and v = its payload (0x10009e5f-0x10009e72); Midi_Device::drainCommandQueue runs on this
// (thunk 0x1000259a, 0x10009e76). While v is non-NULL (0x10009e7b, 0x10009f24): bit 5 of v's +0xac is cleared (0x10009e84-0x10009e8f),
// the node whose payload is v is unlinked from the track list (0x10009e95-0x10009ef7: the successor, when there is one, becomes the
// cursor), then the cursor is advanced once more to its next node and v = that node's payload or 0 (0x10009efc-0x10009f16), and the
// old v is deleted through its vtable slot 0 with 1 (0x10009f18-0x10009f22). So after an unlink with a successor, that successor is
// stepped over. With a MIDI-out handle +0x3c (0x10009f2d-0x10009f32) midiOutClose(handle) runs through [0x100b863c] (0x10009f35).
// Finally the channel list (head 0x100b4fe8, tail 0x100b4fec, count 0x100b4ff4) is popped (0x10009f3b-0x10009f7a) and while the
// payload is non-NULL Seq_freeChannelObjects runs on it (thunk 0x10001d66), it is deleted (0x10009f82-0x10009f8a) and the next head
// popped (0x10009f8f-0x10009fd3).
typedef unsigned(__fastcall* Shutdown_t)(void*, void*);
Shutdown_t Shutdown_orig;
unsigned __fastcall Shutdown_re(void* self, void*) {
    typedef unsigned(__stdcall* MidiOutClose_t)(unsigned);
    if (fat<void*>(self, 0x2c)) {                                             // 0x10009e47-0x10009e4c
        c3fd_fn<Th0_t>(0x000026fd)(fat<void*>(self, 0x2c), 0);                // 0x10009e4e
        c3fd_fn<Th1_t>(0x00001ed3)(c3fd_gref(kSoundTimer), 0, reinterpret_cast<unsigned>(self));   // 0x10009e53-0x10009e5a
    }
    void**& cursor = *reinterpret_cast<void***>(c3fd_sound(0x000b5008));
    void** head = *reinterpret_cast<void***>(c3fd_sound(0x000b5000));       // 0x10009e5f
    cursor = head;                                                            // 0x10009e66
    void* v = head ? head[2] : 0;                                             // 0x10009e6b-0x10009e72
    c3fd_fn<Th0_t>(0x0000259a)(self, 0);                                      // 0x10009e76
    while (v) {                                                               // 0x10009e7b / 0x10009f26
        fat<unsigned>(v, 0xac) &= 0xffffffdfu;                                // 0x10009e84-0x10009e8f
        void* w = v;                                                          // 0x10009e8a
        c3fd_unlinkGlobal(0x000b5000, 0x000b5004, 0x000b5008, 0x000b500c, v);   // 0x10009e95-0x10009ef7
        void** c = cursor;                                                    // 0x10009efc
        v = 0;
        if (c) {                                                              // 0x10009f01 / 0x10009f03
            c = reinterpret_cast<void**>(c[1]);                               // 0x10009f05
            cursor = c;                                                       // 0x10009f0a
            if (c) v = c[2];                                                  // 0x10009f0f / 0x10009f11
        }
        if (w) c3fd_vfn<Th1_t>(w, 0)(w, 0, 1);                                // 0x10009f18-0x10009f22
    }
    if (fat<unsigned>(self, 0x3c))                                            // 0x10009f2d-0x10009f32
        c3fd_imp<MidiOutClose_t>(0x000b863c)(fat<unsigned>(self, 0x3c));      // 0x10009f34 / 0x10009f35
    void**& chead = *reinterpret_cast<void***>(c3fd_sound(0x000b4fe8));
    void** n = chead;                                                         // 0x10009f3b
    while (n) {                                                               // 0x10009f40 / 0x10009f97
        void** next = reinterpret_cast<void**>(n[1]);                         // 0x10009f48
        if (next) {
            next[0] = 0;                                                      // 0x10009f4f
            chead = next;                                                     // 0x10009f51
        } else {
            *reinterpret_cast<void**>(c3fd_sound(0x000b4fec)) = 0;            // 0x10009f59
            chead = 0;                                                        // 0x10009f5f
        }
        void* p = n[2];                                                       // 0x10009f65
        c3fd_delete(n);                                                       // 0x10009f69
        --*reinterpret_cast<int*>(c3fd_sound(0x000b4ff4));                    // 0x10009f6e-0x10009f7a
        if (!p) break;                                                        // 0x10009f78 / 0x10009f80
        c3fd_fn<Th0_t>(0x00001d66)(p, 0);                                     // 0x10009f84
        c3fd_delete(p);                                                       // 0x10009f8a
        n = chead;                                                            // 0x10009f8f
    }
    return 0;                                                                 // 0x10009fd7
}

}  // namespace

SG_HOOK("sound.dll", 0x00009bb0, Dll_Midi_Device_delete_device, DelMidi_re, DelMidi_orig);
SG_HOOK("sound.dll", 0x0000b1a0, Dll_Wave_Device_delete_device, DelWave_re, DelWave_orig);
SG_HOOK("sound.dll", 0x0000ea00, Dll_Wave_In_Device_delete_device, DelWaveIn_re, DelWaveIn_orig);
SG_HOOK("sound.dll", 0x000092e0, init_sound_timer, InitTimer_re, InitTimer_orig);
SG_HOOK("sound.dll", 0x0000b050, Dll_Wave_Device_create_device, WaveCreate_re, WaveCreate_orig);
SG_HOOK("sound.dll", 0x0000e910, Dll_Wave_In_Device_create_device, WaveInCreate_re, WaveInCreate_orig);
SG_HOOK("sound.dll", 0x00039690, MultiSound_ctor, MsCtor_re, MsCtor_orig);
SG_HOOK("sound.dll", 0x00039790, MultiSound_ctorWithType, MsCtorT_re, MsCtorT_orig);
SG_HOOK("sound.dll", 0x00039870, MultiSound_dtor, MsDtor_re, MsDtor_orig);
SG_HOOK("sound.dll", 0x0001e2b0, Seq_dtor, SeqDtor_re, SeqDtor_orig);
SG_HOOK("sound.dll", 0x0000cc70, Wave_Device_flushCommandQueue, Flush_re, Flush_orig);
SG_HOOK("sound.dll", 0x0001ac40, Voice_notifyChannelNotes, Notify_re, Notify_orig);
SG_HOOK("sound.dll", 0x00020d80, Voice_allNotesOff, AllOff_re, AllOff_orig);
SG_HOOK("sound.dll", 0x00023950, Seq_replayEventRange, Replay_re, Replay_orig);
SG_HOOK("sound.dll", 0x000246a0, Midi_Device_emitEvent, Emit_re, Emit_orig);
SG_HOOK("sound.dll", 0x00021a90, Midi_Device_sendShortMsg, ShortMsg_re, ShortMsg_orig);
SG_HOOK("sound.dll", 0x00020870, Midi_Device_playEvent, PlayEv_re, PlayEv_orig);
SG_HOOK("sound.dll", 0x0002add0, Engine_playSound, PlaySnd_re, PlaySnd_orig);
SG_HOOK("sound.dll", 0x0002f220, buildVoiceTxName, TxName_re, TxName_orig);
SG_HOOK("sound.dll", 0x0000aaf0, Midi_Device_scanCommandRing, MidiScan_re, MidiScan_orig);
SG_HOOK("sound.dll", 0x0000dcf0, Wave_Device_scanCommandRing, WaveScan_re, WaveScan_orig);
SG_HOOK("sound.dll", 0x00004000, Sound_dtor, SoundDtor_re, SoundDtor_orig);
SG_HOOK("sound.dll", 0x000312a0, streamReaderDtor, SrDtor_re, SrDtor_orig);
SG_HOOK("sound.dll", 0x0002f8d0, MultiSound_setSlotSource, SetSlot_re, SetSlot_orig);
SG_HOOK("sound.dll", 0x00020be0, Voice_startPlayback, StartPlay_re, StartPlay_orig);
SG_HOOK("sound.dll", 0x00024040, Seq_dispatchChannelCmd, Dispatch_re, Dispatch_orig);
SG_HOOK("sound.dll", 0x0000fbb0, DsBuffer_create, DsCreate_re, DsCreate_orig);
SG_HOOK("sound.dll", 0x000340b0, Channel_releaseBuffers, RelBufs_re, RelBufs_orig);
SG_HOOK("sound.dll", 0x00033c70, Channel_closeAndFree, CloseFree_re, CloseFree_orig);
SG_HOOK("sound.dll", 0x0000a330, Midi_Device_drainCommandQueue, Drain_re, Drain_orig);
SG_HOOK("sound.dll", 0x00009e40, Midi_Device_shutdown, Shutdown_re, Shutdown_orig);

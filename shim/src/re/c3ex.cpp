// C3 batch c3ex (round 32, 2026-10-13) of sound.dll: rows in the `other` / `never` classes of the open C2 pool
// (log/c3/c3ex_candidates.tsv). Method as c3dt (shim/src/re/c3dt.cpp): fixture-owned objects only, module globals the body
// tests swapped for the key with the first thread frozen and the multimedia timer quiet, and a thread-filtered recording stub
// on the incremental-link thunk of any callee whose tree reaches the live engine.
// Addresses in SG_HOOK and in the `// 0x...` comment lines are RVAs; the instruction addresses quoted inside each body are
// VAs (VA = 0x10000000 + RVA). Facts are taken from the C2 transcriptions re/analysis/audio/<rva>_sound_<name>.md and the
// matched sources (re/match/sound_raw_05.cpp, sound_raw_06.cpp, sound_raw_07.cpp), each cited by instruction address.
// sound.dll is LoadLibrary'd after the shim starts, so these hooks install from the LoadLibraryA hook.
// __thiscall is emulated with __fastcall (ecx = this, edx unused). Every callee runs through its original address.
#include "hooks.h"
#include <stdint.h>

namespace {

template <typename T> T& fat(void* base, int off) {
    return *reinterpret_cast<T*>(reinterpret_cast<char*>(base) + off);
}

// sound.dll has base relocations: callees and module globals are addressed through the loaded base.
char* c3ex_sound(unsigned rva) {
    static HMODULE m = 0;
    if (!m) m = GetModuleHandleA("sound.dll");
    return reinterpret_cast<char*>(m) + rva;
}
template <typename F> F c3ex_fn(unsigned rva) { return reinterpret_cast<F>(c3ex_sound(rva)); }
void* c3ex_glob(unsigned rva) { return *reinterpret_cast<void**>(c3ex_sound(rva)); }

typedef unsigned(__fastcall* Th0_t)(void*, void*);                         // thiscall, nothing pushed
typedef unsigned(__fastcall* Th1_t)(void*, void*, unsigned);               // thiscall, one stack argument
typedef unsigned(__fastcall* Th2_t)(void*, void*, unsigned, unsigned);     // thiscall, two stack arguments
typedef unsigned(__fastcall* Th3_t)(void*, void*, unsigned, unsigned, unsigned);
typedef unsigned(__fastcall* Th4_t)(void*, void*, unsigned, unsigned, unsigned, unsigned);
typedef unsigned(__cdecl* CreateSound_t)(void**, unsigned, unsigned);      // create_sound 0x100093e0 (thunk 0x1000137a)
typedef unsigned(__cdecl* DeleteSound_t)(void*);                           // delete_sound 0x100097d0 (thunk 0x10001a87)
typedef void*(__cdecl* OpNew_t)(unsigned);                                  // operator new 0x100424d7
typedef void(__cdecl* OpDelete_t)(void*);                                   // operator delete 0x1004249a
typedef unsigned(__cdecl* BeginThread_t)(void*, unsigned, void*);          // _beginthread 0x10042984
typedef unsigned(__stdcall* Imp0_t)();
typedef unsigned(__stdcall* Imp1_t)(unsigned);
typedef unsigned(__stdcall* Imp2_t)(unsigned, unsigned);
typedef unsigned(__stdcall* Imp3_t)(unsigned, int, unsigned);

// The import whose IAT slot is at RVA `slotRva` (`call dword ptr [0x100b....]` in the original).
template <typename F> F c3ex_imp(unsigned slotRva) { return *reinterpret_cast<F*>(c3ex_sound(slotRva)); }
const unsigned kIatSetThreadPriority = 0x000b8400;
const unsigned kIatResetEvent = 0x000b8404;
const unsigned kIatSetWindowLongA = 0x000b85a4;
const unsigned kIatTimeGetTime = 0x000b8654;

// virtual slot `off` of the object `obj` (`mov eax, [obj]; call/jmp [eax + off]`), typed by its pushed arguments
template <typename F> F c3ex_vfn(void* obj, int off) {
    return *reinterpret_cast<F*>(*reinterpret_cast<char**>(obj) + off);
}
Th0_t c3ex_vslot(void* obj, int off) { return c3ex_vfn<Th0_t>(obj, off); }

unsigned c3ex_create(void** out, unsigned a, unsigned type) { return c3ex_fn<CreateSound_t>(0x0000137a)(out, a, type); }
void c3ex_deleteSound(void* s) { c3ex_fn<DeleteSound_t>(0x00001a87)(s); }
unsigned c3ex_playSound(void* self, unsigned a) { return c3ex_fn<Th1_t>(0x000026e9)(self, 0, a); }   // Engine_playSound 0x1002add0

// =====================================================================================================================
// A. field stores and a forwarder on fixture objects
// =====================================================================================================================

// 0x00039450  setField6cReturn0 (VA 0x10039450). __thiscall, `ret 4` (0x10039459), one stack argument. The argument
// (`mov eax, [esp+4]` 0x10039450) is stored at this+0x6c (0x10039454) and 0 is returned (`xor eax, eax` 0x10039457).
typedef unsigned(__fastcall* SetF6c_t)(void*, void*, unsigned);
SetF6c_t SetF6c_orig;
unsigned __fastcall SetF6c_re(void* self, void*, unsigned v) {
    fat<unsigned>(self, 0x6c) = v;                                            // 0x10039454
    return 0;                                                                 // 0x10039457
}

// 0x00039470  clearField6cReturn0 (VA 0x10039470). __thiscall, plain `ret` (0x10039479). The dword at this+0x6c is set to 0
// (0x10039470) and 0 is returned (`xor eax, eax` 0x10039477).
typedef unsigned(__fastcall* ClrF6c_t)(void*, void*);
ClrF6c_t ClrF6c_orig;
unsigned __fastcall ClrF6c_re(void* self, void*) {
    fat<unsigned>(self, 0x6c) = 0;                                            // 0x10039470
    return 0;                                                                 // 0x10039477
}

// 0x0002f680  Ch5c2ec_forwardSlot37 (VA 0x1002f680). __thiscall, plain `ret` on the NULL arm (0x1002f691), so nothing is
// pushed by its callers. ecx = the object at this+0x40 (0x1002f680); NULL returns 0 (`test` / `je` 0x1002f683 / 0x1002f685,
// `xor eax, eax` 0x1002f68f). Otherwise `jmp [eax+0x94]` (0x1002f687 / 0x1002f689): slot 37 of that object's vtable runs with
// ecx = the object and its result is the result.
typedef unsigned(__fastcall* Fwd37_t)(void*, void*);
Fwd37_t Fwd37_orig;
unsigned __fastcall Fwd37_re(void* self, void*) {
    void* child = fat<void*>(self, 0x40);                                     // 0x1002f680
    if (!child) return 0;                                                     // 0x1002f685 / 0x1002f68f
    return c3ex_vslot(child, 0x94)(child, 0);                                 // 0x1002f689
}

// 0x00031080  Channel_start (VA 0x10031080). __thiscall, plain `ret` (0x1003108f, 0x100310c0, 0x100310c7). Bit 0 of the byte at
// this+0x58 set (0x10031083 / 0x10031087) returns 0xd (0x10031089). g_soundEngine 0x100b4a20 is loaded (0x10031090); NULL returns 1
// (`je` 0x10031098, 0x100310c1). Otherwise the engine's vtable slot +0x6c runs with ecx = the engine (0x1003109a / 0x1003109c);
// a non-zero result returns 1 (`jne` 0x100310a1). On zero the dword at this+0x58 is rewritten as (old & 0xffffffdb) | 1
// (0x100310a3-0x100310af: bits 2 and 5 cleared, bit 0 set), g_soundEngine is loaded again (0x100310b2), 1 and this are pushed and
// `call 0x100020b3` (thunk to WaveCmdQueue::pushNode 0x1000c2f0) runs with ecx = the engine (0x100310b8); its result is dropped and
// 0 is returned.
typedef unsigned(__fastcall* ChStart_t)(void*, void*);
ChStart_t ChStart_orig;
unsigned __fastcall ChStart_re(void* self, void*) {
    if (fat<unsigned char>(self, 0x58) & 1) return 0xd;                       // 0x10031083 / 0x10031087 / 0x10031089
    void* eng = c3ex_glob(0x000b4a20);                                        // 0x10031090
    if (!eng) return 1;                                                       // 0x10031098
    if (c3ex_vslot(eng, 0x6c)(eng, 0) != 0) return 1;                         // 0x1003109c / 0x100310a1
    fat<unsigned>(self, 0x58) = (fat<unsigned>(self, 0x58) & 0xffffffdbu) | 1;   // 0x100310a3-0x100310af
    c3ex_fn<Th2_t>(0x000020b3)(c3ex_glob(0x000b4a20), 0, reinterpret_cast<unsigned>(self), 1);   // 0x100310b2-0x100310b8
    return 0;
}

// =====================================================================================================================
// B. channel / device wrappers over create_sound, Engine_playSound and their objects' virtual slots
// =====================================================================================================================
// create_sound (cdecl: the slot this+0x40 that receives the new object, an argument, a type number) is always called through
// the thunk 0x1000137a, delete_sound (cdecl, the object) through 0x10001a87, Engine_playSound (thiscall, one argument) through
// 0x100026e9.

// 0x0002deb0  SubChannel_openSourceType2 (VA 0x1002deb0). __thiscall, `ret 4`, one argument a (ebp, 0x1002deb4). With this+0x40
// NULL (0x1002deb9 / 0x1002dec1) the result r is create_sound(this+0x40, a, 2) (0x1002dec3-0x1002decc); otherwise r is the
// argument itself (`mov eax, [esp+0x10]` 0x1002ded1, the same stack slot as 0x1002deb4 after the three pushes). A non-zero r is
// returned (0x1002ded5 / 0x1002ded7). Otherwise Engine_playSound(a) runs with ecx = this (0x1002ded9-0x1002dedc); a non-zero
// result is returned (0x1002dee1 / 0x1002dee3). Then, with bit 1 of the byte at this+0x58 (0x1002dee5 / 0x1002dee9), bit 1 of
// the byte at +0x58 of the object in this+0x40 is set (0x1002deeb-0x1002def3) and a non-zero dword this+0x5c is copied to that
// object's +0x5c (0x1002def6-0x1002deff). 0 is returned (0x1002df02).
typedef unsigned(__fastcall* OpenSrc2_t)(void*, void*, unsigned);
OpenSrc2_t OpenSrc2_orig;
unsigned __fastcall OpenSrc2_re(void* self, void*, unsigned a) {
    void** slot = &fat<void*>(self, 0x40);
    unsigned r = *slot ? a : c3ex_create(slot, a, 2);                         // 0x1002debf-0x1002ded1
    if (r) return r;                                                          // 0x1002ded7
    r = c3ex_playSound(self, a);                                              // 0x1002dedc
    if (r) return r;                                                          // 0x1002dee3
    if (fat<unsigned char>(self, 0x58) & 2) {                                 // 0x1002dee5 / 0x1002dee9
        fat<unsigned char>(*slot, 0x58) |= 2;                                 // 0x1002deeb-0x1002def3
        void* v = fat<void*>(self, 0x5c);                                     // 0x1002def6
        if (v) fat<void*>(*slot, 0x5c) = v;                                   // 0x1002defb-0x1002deff
    }
    return 0;                                                                 // 0x1002df02
}

// 0x0002eb40  openSourceType4 (VA 0x1002eb40). __thiscall, `ret 0x10`, four arguments a1..a4. eax = 0 (0x1002eb4a). With
// this+0x40 NULL (0x1002eb4c / 0x1002eb4e) create_sound(this+0x40, 0, 4) runs (0x1002eb50-0x1002eb59); a non-zero result is
// returned (0x1002eb5c / 0x1002eb5e). The object in this+0x40 NULL then returns eax (0, 0x1002eb60-0x1002eb64). Otherwise its
// vtable slot +0x84 runs with ecx = the object and a1..a4 (pushed a4 first, 0x1002eb66-0x1002eb7d); a non-zero result r
// (0x1002eb85 / 0x1002eb87) passes the object to delete_sound and clears this+0x40 (0x1002eb89-0x1002eb94). Then this's own
// vtable slot +0x7c runs with ecx = this (0x1002eb9a-0x1002eb9e) and r is returned (0x1002eba1).
typedef unsigned(__fastcall* OpenSrc4_t)(void*, void*, unsigned, unsigned, unsigned, unsigned);
OpenSrc4_t OpenSrc4_orig;
unsigned __fastcall OpenSrc4_re(void* self, void*, unsigned a1, unsigned a2, unsigned a3, unsigned a4) {
    void** slot = &fat<void*>(self, 0x40);
    if (!*slot) {                                                             // 0x1002eb4c / 0x1002eb4e
        unsigned c = c3ex_create(slot, 0, 4);                                 // 0x1002eb54
        if (c) return c;                                                      // 0x1002eb5e
    }
    if (!*slot) return 0;                                                     // 0x1002eb60-0x1002eb64
    unsigned r = c3ex_vfn<Th4_t>(*slot, 0x84)(*slot, 0, a1, a2, a3, a4);      // 0x1002eb7d
    if (r) {                                                                  // 0x1002eb87
        c3ex_deleteSound(*slot);                                              // 0x1002eb8c
        *slot = 0;                                                            // 0x1002eb94
    }
    c3ex_vslot(self, 0x7c)(self, 0);                                          // 0x1002eb9e
    return r;                                                                 // 0x1002eba1
}

// 0x0002f060  openSourceType6 (VA 0x1002f060). __thiscall, `ret 0xc`, three arguments. The shape of openSourceType4 with type 6
// for create_sound (0x1002f070-0x1002f079) and three arguments passed to the object's slot +0x84 (pushed a3 first,
// 0x1002f086-0x1002f098); a non-zero result deletes and clears the object (0x1002f0a0-0x1002f0af); slot +0x7c of this runs
// (0x1002f0b5-0x1002f0b9) and the result is returned (0x1002f0bc).
typedef unsigned(__fastcall* OpenSrc6_t)(void*, void*, unsigned, unsigned, unsigned);
OpenSrc6_t OpenSrc6_orig;
unsigned __fastcall OpenSrc6_re(void* self, void*, unsigned a1, unsigned a2, unsigned a3) {
    void** slot = &fat<void*>(self, 0x40);
    if (!*slot) {                                                             // 0x1002f06c / 0x1002f06e
        unsigned c = c3ex_create(slot, 0, 6);                                 // 0x1002f074
        if (c) return c;                                                      // 0x1002f07e
    }
    if (!*slot) return 0;                                                     // 0x1002f080-0x1002f084
    unsigned r = c3ex_vfn<Th3_t>(*slot, 0x84)(*slot, 0, a1, a2, a3);          // 0x1002f098
    if (r) {                                                                  // 0x1002f0a2
        c3ex_deleteSound(*slot);                                              // 0x1002f0a7
        *slot = 0;                                                            // 0x1002f0af
    }
    c3ex_vslot(self, 0x7c)(self, 0);                                          // 0x1002f0b9
    return r;                                                                 // 0x1002f0bc
}

// 0x0002f5e0  openSourceType7 (VA 0x1002f5e0). __thiscall, `ret 0xc`, three arguments. this's vtable slot +0x14 runs first
// (0x1002f5e7 / 0x1002f5e9); ebx = 0 (0x1002f5e5). With this+0x40 NULL create_sound(this+0x40, 0, 7) is the result
// (0x1002f5f6-0x1002f5ff), returned when non-zero (0x1002f604 / 0x1002f606). The object in this+0x40 NULL returns ebx
// (0x1002f608-0x1002f60c). Otherwise its slot +0x84 runs with a1..a3 (0x1002f60e-0x1002f61f) and gives the result r; r non-zero
// and not 0x10 (0x1002f627-0x1002f62e) deletes and clears the object (0x1002f630-0x1002f63b). this+0x40 still non-zero
// (0x1002f641 / 0x1002f644) runs this's slot +0x7c (0x1002f646-0x1002f64a). r is returned (0x1002f64e).
typedef unsigned(__fastcall* OpenSrc7_t)(void*, void*, unsigned, unsigned, unsigned);
OpenSrc7_t OpenSrc7_orig;
unsigned __fastcall OpenSrc7_re(void* self, void*, unsigned a1, unsigned a2, unsigned a3) {
    c3ex_vslot(self, 0x14)(self, 0);                                          // 0x1002f5e9
    void** slot = &fat<void*>(self, 0x40);
    unsigned r = 0;                                                           // 0x1002f5e5
    if (!*slot) {                                                             // 0x1002f5f2 / 0x1002f5f4
        r = c3ex_create(slot, 0, 7);                                          // 0x1002f5fa
        if (r) return r;                                                      // 0x1002f606
    }
    if (!*slot) return r;                                                     // 0x1002f608-0x1002f60c
    r = c3ex_vfn<Th3_t>(*slot, 0x84)(*slot, 0, a1, a2, a3);                   // 0x1002f61f
    if (r && r != 0x10) {                                                     // 0x1002f629 / 0x1002f62e
        c3ex_deleteSound(*slot);                                              // 0x1002f633
        *slot = 0;                                                            // 0x1002f63b
    }
    if (*slot) c3ex_vslot(self, 0x7c)(self, 0);                               // 0x1002f641-0x1002f64a
    return r;                                                                 // 0x1002f64e
}

// 0x0002bee0  Device_createStartQueue (VA 0x1002bee0). __thiscall, plain `ret`. a = this+0x50 (0x1002bee7); zero returns 8
// (0x1002beec, 0x1002bf64). With this+0x40 NULL create_sound(this+0x40, a, 1) runs; a non-zero result is returned
// (0x1002bef8-0x1002bf06). The flag f is 1 when bit 0 of the byte at this+0x58 is set (0x1002bf08-0x1002bf0e), with bit 1 added when
// this's slot +0x58 returns non-zero (0x1002bf13-0x1002bf1e). The object in this+0x40 gets f through its slot +0x6c
// (0x1002bf21-0x1002bf26), then its slot +0x84 runs (0x1002bf29-0x1002bf2d); a non-zero result is returned (0x1002bf35 /
// 0x1002bf37). Bit 0 of this+0x44 already set returns 0 (0x1002bf39-0x1002bf3e). Otherwise the bit is set (0x1002bf42-0x1002bf46),
// this's slot +0x7c runs (0x1002bf49) and, with this+0x30 non-zero (0x1002bf4c-0x1002bf51), the object's slot +0x48 gets 1
// (0x1002bf53-0x1002bf5b). 0 is returned (0x1002bf5f).
typedef unsigned(__fastcall* CreateStartQ_t)(void*, void*);
CreateStartQ_t CreateStartQ_orig;
unsigned __fastcall CreateStartQ_re(void* self, void*) {
    unsigned a = fat<unsigned>(self, 0x50);                                   // 0x1002bee7
    if (!a) return 8;                                                         // 0x1002beec / 0x1002bf64
    void** slot = &fat<void*>(self, 0x40);
    if (!*slot) {                                                             // 0x1002bef4 / 0x1002bef6
        unsigned c = c3ex_create(slot, a, 1);                                 // 0x1002befc
        if (c) return c;                                                      // 0x1002bf06
    }
    unsigned f = (fat<unsigned char>(self, 0x58) & 1) ? 1u : 0u;              // 0x1002bf08-0x1002bf0e
    if (c3ex_vslot(self, 0x58)(self, 0)) f |= 2;                              // 0x1002bf17-0x1002bf1e
    c3ex_vfn<Th1_t>(*slot, 0x6c)(*slot, 0, f);                                // 0x1002bf26
    unsigned r = c3ex_vslot(*slot, 0x84)(*slot, 0);                           // 0x1002bf2d
    if (r) return r;                                                          // 0x1002bf37
    unsigned v = fat<unsigned>(self, 0x44);                                   // 0x1002bf39
    if (v & 1) return 0;                                                      // 0x1002bf3c / 0x1002bf3e
    fat<unsigned>(self, 0x44) = v | 1;                                        // 0x1002bf42-0x1002bf46
    c3ex_vslot(self, 0x7c)(self, 0);                                          // 0x1002bf49
    if (fat<unsigned>(self, 0x30)) c3ex_vfn<Th1_t>(*slot, 0x48)(*slot, 0, 1);  // 0x1002bf4c-0x1002bf5b
    return 0;                                                                 // 0x1002bf5f
}

// Flag word the two lazy creators pass to the new object's slot +0x6c: f = 1 for bit 0 of the byte at this+0x58, | 2 when this's
// slot +0x58 returns non-zero, then bits of the dword at this+0x58 read after that call.
// 0x0002bd80  Device_lazyCreateSound (VA 0x1002bd80). __thiscall, plain `ret`. a = this+0x50 (0x1002bd85); zero returns 8
// (0x1002bd8d, 0x1002be42). With this+0x40 NULL create_sound(this+0x40, a, 1) runs; a non-zero result is returned
// (0x1002bd9d-0x1002bdab). f as above (0x1002bdb1-0x1002bdc7), then from the dword at this+0x58 (0x1002bdca): bit 2 adds 0x11
// (0x1002bdcd-0x1002bdd1); bit 3 adds 0x40 and, without bit 5, 1 (0x1002bdd4-0x1002bddf); bit 4 adds 0x80 (0x1002bde2-0x1002bde6);
// bit 5 adds 0x100 (0x1002bdec-0x1002bdf0); bit 6 adds 0x400 (0x1002bdf6-0x1002bdfa). The object gets f through slot +0x6c
// (0x1002be00-0x1002be05). Engine_playSound(a) runs with ecx = this (0x1002be08-0x1002be0b); a non-zero result r deletes the
// object, clears this+0x40 and is returned (0x1002be12-0x1002be27). Otherwise the object's slot +0xc8 result is stored at
// this+0x64 (0x1002be2e-0x1002be38) and 0 is returned (0x1002be3b).
typedef unsigned(__fastcall* LazyCreate_t)(void*, void*);
LazyCreate_t LazyCreate_orig;
unsigned __fastcall LazyCreate_re(void* self, void*) {
    unsigned a = fat<unsigned>(self, 0x50);                                   // 0x1002bd85
    if (!a) return 8;                                                         // 0x1002bd8d / 0x1002be42
    void** slot = &fat<void*>(self, 0x40);
    if (!*slot) {                                                             // 0x1002bd99 / 0x1002bd9b
        unsigned c = c3ex_create(slot, a, 1);                                 // 0x1002bda1
        if (c) return c;                                                      // 0x1002bdab
    }
    unsigned f = (fat<unsigned char>(self, 0x58) & 1) ? 1u : 0u;              // 0x1002bdb1-0x1002bdb7
    if (c3ex_vslot(self, 0x58)(self, 0)) f |= 2;                              // 0x1002bdc0-0x1002bdc7
    unsigned fl = fat<unsigned>(self, 0x58);                                  // 0x1002bdca
    if (fl & 4) f |= 0x11;                                                    // 0x1002bdcd-0x1002bdd1
    if (fl & 8) {                                                             // 0x1002bdd4 / 0x1002bdd6
        f |= 0x40;                                                            // 0x1002bdd8
        if (!(fl & 0x20)) f |= 1;                                             // 0x1002bddb-0x1002bddf
    }
    if (fl & 0x10) f |= 0x80;                                                 // 0x1002bde2-0x1002bde6
    if (fl & 0x20) f |= 0x100;                                                // 0x1002bdec-0x1002bdf0
    if (fl & 0x40) f |= 0x400;                                                // 0x1002bdf6-0x1002bdfa
    c3ex_vfn<Th1_t>(*slot, 0x6c)(*slot, 0, f);                                // 0x1002be05
    unsigned r = c3ex_playSound(self, a);                                     // 0x1002be0b
    if (r) {                                                                  // 0x1002be12 / 0x1002be14
        c3ex_deleteSound(*slot);                                              // 0x1002be19
        *slot = 0;                                                            // 0x1002be21
        return r;                                                             // 0x1002be27
    }
    fat<unsigned>(self, 0x64) = c3ex_vslot(*slot, 0xc8)(*slot, 0);            // 0x1002be32 / 0x1002be38
    return 0;                                                                 // 0x1002be3b
}

// 0x0002bc60  Device_prepareSoundFlags (VA 0x1002bc60). __thiscall, `ret 4`, one argument a (ebp, 0x1002bc65). With this+0x40
// NULL create_sound(this+0x40, a, 1) runs; a non-zero result is returned (0x1002bc76-0x1002bc84). f as in the lazy creator
// (0x1002bc8a-0x1002bca0), then from the dword at this+0x58 (0x1002bca3): bit 3 adds 0x40 (0x1002bca6-0x1002bcaa), bit 4 adds 0x80
// (`or bl, 0x80` 0x1002bcb1), bit 6 adds 0x400 (`or bh, 4` 0x1002bcb8), bit 1 adds 4 (0x1002bcbb-0x1002bcbf). The object gets f
// through slot +0x6c (0x1002bcc2-0x1002bcc7). Bit 1 of the byte at this+0x58, read again, set returns 0 (0x1002bcca / 0x1002bcce,
// 0x1002bd29). Otherwise Engine_playSound(a) (0x1002bcd0-0x1002bcd3); a non-zero result r deletes the object, clears this+0x40
// and is returned (0x1002bcda-0x1002bcef). Otherwise the object's slot +0xc8 result goes to this+0x64 (0x1002bcf8-0x1002bd07), and
// its slots +0x40, +0x9c and +0x44 get this+4, this+0x5c and this+8 (0x1002bd02-0x1002bd26). 0 is returned (0x1002bd29).
typedef unsigned(__fastcall* PrepFlags_t)(void*, void*, unsigned);
PrepFlags_t PrepFlags_orig;
unsigned __fastcall PrepFlags_re(void* self, void*, unsigned a) {
    void** slot = &fat<void*>(self, 0x40);
    if (!*slot) {                                                             // 0x1002bc72 / 0x1002bc74
        unsigned c = c3ex_create(slot, a, 1);                                 // 0x1002bc7a
        if (c) return c;                                                      // 0x1002bc84
    }
    unsigned f = (fat<unsigned char>(self, 0x58) & 1) ? 1u : 0u;              // 0x1002bc8a-0x1002bc90
    if (c3ex_vslot(self, 0x58)(self, 0)) f |= 2;                              // 0x1002bc99-0x1002bca0
    unsigned fl = fat<unsigned>(self, 0x58);                                  // 0x1002bca3
    if (fl & 8) f |= 0x40;                                                    // 0x1002bca6-0x1002bcaa
    if (fl & 0x10) f |= 0x80;                                                 // 0x1002bcad-0x1002bcb1
    if (fl & 0x40) f |= 0x400;                                                // 0x1002bcb4-0x1002bcb8
    if (fl & 2) f |= 4;                                                       // 0x1002bcbb-0x1002bcbf
    c3ex_vfn<Th1_t>(*slot, 0x6c)(*slot, 0, f);                                // 0x1002bcc7
    if (fat<unsigned char>(self, 0x58) & 2) return 0;                         // 0x1002bcca / 0x1002bcce
    unsigned r = c3ex_playSound(self, a);                                     // 0x1002bcd3
    if (r) {                                                                  // 0x1002bcda / 0x1002bcdc
        c3ex_deleteSound(*slot);                                              // 0x1002bce1
        *slot = 0;                                                            // 0x1002bce9
        return r;                                                             // 0x1002bcef
    }
    fat<unsigned>(self, 0x64) = c3ex_vslot(*slot, 0xc8)(*slot, 0);            // 0x1002bcfc / 0x1002bd07
    c3ex_vfn<Th1_t>(*slot, 0x40)(*slot, 0, fat<unsigned>(self, 4));           // 0x1002bd0d
    c3ex_vfn<Th1_t>(*slot, 0x9c)(*slot, 0, fat<unsigned>(self, 0x5c));        // 0x1002bd18
    c3ex_vfn<Th1_t>(*slot, 0x44)(*slot, 0, fat<unsigned>(self, 8));           // 0x1002bd26
    return 0;                                                                 // 0x1002bd29
}

// 0x0002b7c0  startDevicePlayback (VA 0x1002b7c0). __thiscall, plain `ret`. r = 0 (0x1002b7c4). The object in this+0x40 non-NULL
// (0x1002b7c6-0x1002b7cb): r = its slot +0x1c (0x1002b7cd-0x1002b7d2). NULL with bit 4 of the byte at this+0x58 (0x1002b7d6 /
// 0x1002b7da): `call 0x100016f4` (thunk to Device_lazyCreateSound 0x1002bd80) with ecx = this (0x1002b7dc-0x1002b7de), its
// result dropped; then the object in this+0x40, when non-NULL, gives r through slot +0x1c (0x1002b7e3-0x1002b7ef). NULL without the
// bit: r = 0x14 (0x1002b7f3). Then, with bit 4 of the byte at this+0x58 read again (0x1002b7f8 / 0x1002b7fc), timeGetTime is
// stored at this+0x68 (0x1002b7fe-0x1002b804), this's slot +0x80 runs (0x1002b807-0x1002b80b) and this+0x40 is cleared
// (0x1002b811). r is returned (0x1002b818).
typedef unsigned(__fastcall* StartDevPlay_t)(void*, void*);
StartDevPlay_t StartDevPlay_orig;
unsigned __fastcall StartDevPlay_re(void* self, void*) {
    unsigned r = 0;                                                           // 0x1002b7c4
    void* c = fat<void*>(self, 0x40);                                         // 0x1002b7c6
    if (c) {
        r = c3ex_vslot(c, 0x1c)(c, 0);                                        // 0x1002b7cf
    } else if (fat<unsigned char>(self, 0x58) & 0x10) {                       // 0x1002b7d6 / 0x1002b7da
        c3ex_fn<Th0_t>(0x000016f4)(self, 0);                                  // 0x1002b7de
        c = fat<void*>(self, 0x40);                                           // 0x1002b7e3
        if (c) r = c3ex_vslot(c, 0x1c)(c, 0);                                 // 0x1002b7e6-0x1002b7ef
    } else {
        r = 0x14;                                                             // 0x1002b7f3
    }
    if (fat<unsigned char>(self, 0x58) & 0x10) {                              // 0x1002b7f8 / 0x1002b7fc
        fat<unsigned>(self, 0x68) = c3ex_imp<Imp0_t>(kIatTimeGetTime)();      // 0x1002b7fe / 0x1002b804
        c3ex_vslot(self, 0x80)(self, 0);                                      // 0x1002b80b
        fat<void*>(self, 0x40) = 0;                                           // 0x1002b811
    }
    return r;                                                                 // 0x1002b818
}

// 0x00039b30  SoundGroup_stop (VA 0x10039b30). __thiscall, plain `ret`. this's slot +0x5c runs first (0x10039b33 / 0x10039b35).
// With the count n at this+0x1438 greater than 1 (signed, `cmp eax, 1; jle` 0x10039b3e / 0x10039b41) the members i = 1 .. n-1 at
// this+0x1440 + 4 (i-1) are walked (ebx = i, esi = the slot, 0x10039b45-0x10039b79; n is reloaded every pass, 0x10039b6d, and the
// loop runs while i < n, signed `jl` 0x10039b79): a non-NULL member is stopped through `call 0x100016a4` (thunk to this same
// function) with ecx = the member (0x10039b56), then the slot is reloaded and a non-NULL member gets its slot 0 with 1 (the
// deleting destructor, 0x10039b5b-0x10039b65), and the slot is cleared (0x10039b67). After the walk the count is set to 1
// (0x10039b7c). Bit 1 of the byte at this+0xc8 (0x10039b87 / 0x10039b8e): `call 0x10001d11` (thunk to Channel_releaseBuffers
// 0x100340b0) with ecx = this+0x70 (0x10039b90 / 0x10039b93), bit 0 of this+0x44 cleared (0x10039b98-0x10039b9e), and that call's
// result is returned. Without the bit 0x14 is returned (0x10039ba3).
typedef unsigned(__fastcall* GroupStop_t)(void*, void*);
GroupStop_t GroupStop_orig;
unsigned __fastcall GroupStop_re(void* self, void*) {
    c3ex_vslot(self, 0x5c)(self, 0);                                          // 0x10039b35
    int n = fat<int>(self, 0x1438);                                           // 0x10039b38
    if (n > 1) {                                                              // 0x10039b3e / 0x10039b41
        int i = 1;                                                            // 0x10039b45
        void** p = &fat<void*>(self, 0x1440);                                 // 0x10039b4a
        do {
            if (*p) {                                                         // 0x10039b50-0x10039b54
                c3ex_fn<Th0_t>(0x000016a4)(*p, 0);                            // 0x10039b56
                void* m = *p;                                                 // 0x10039b5b
                if (m) c3ex_vfn<Th1_t>(m, 0)(m, 0, 1);                        // 0x10039b5d-0x10039b65
                *p = 0;                                                       // 0x10039b67
            }
            n = fat<int>(self, 0x1438);                                       // 0x10039b6d
            ++i;                                                              // 0x10039b73
            ++p;                                                              // 0x10039b74
        } while (i < n);                                                      // 0x10039b77 / 0x10039b79
        fat<int>(self, 0x1438) = 1;                                           // 0x10039b7c
    }
    if (fat<unsigned char>(self, 0xc8) & 2) {                                 // 0x10039b87 / 0x10039b8e
        unsigned r = c3ex_fn<Th0_t>(0x00001d11)(&fat<char>(self, 0x70), 0);   // 0x10039b93
        fat<unsigned>(self, 0x44) &= 0xfffffffeu;                             // 0x10039b98-0x10039b9e
        return r;
    }
    return 0x14;                                                              // 0x10039ba3
}

// 0x00035150  Channel_setVolume (VA 0x10035150). __thiscall, `ret 4`, one argument v. Bit 28 of this+0x58 clear (0x10035153 /
// 0x1003515a) or the buffer this+0x54 NULL (0x1003515c-0x10035161) returns at once. Otherwise `call 0x100018f7` (thunk to
// DsBuffer::setVolume 0x1000f1c0) runs with ecx = that buffer and v (0x10035164-0x1003516a), then for the 16 slots at
// this+0x318 + 0x110 k (0x1003516f-0x1003518d) every non-NULL dword gets the same call (0x1003517a-0x10035181). eax is left as
// the last call made (or whatever it held on the early return): both callers drop it (MultiSound::setVolume 0x1003a104 is followed
// by `test esi, esi` 0x1003a109, MultiSound_start 0x10039d9e by `mov ecx, edi` 0x10039da3), so the reimplementation returns nothing
// and the A/B compares the calls and memory.
typedef void(__fastcall* ChSetVol_t)(void*, void*, unsigned);
ChSetVol_t ChSetVol_orig;
void __fastcall ChSetVol_re(void* self, void*, unsigned v) {
    if (!(fat<unsigned>(self, 0x58) & 0x10000000u)) return;                   // 0x10035153 / 0x1003515a
    void* b = fat<void*>(self, 0x54);                                         // 0x1003515c
    if (!b) return;                                                           // 0x10035161
    c3ex_fn<Th1_t>(0x000018f7)(b, 0, v);                                      // 0x1003516a
    for (int k = 0; k < 16; ++k) {                                            // 0x10035175 / 0x1003518c / 0x1003518d
        void* s = fat<void*>(self, 0x318 + 0x110 * k);                        // 0x1003517a
        if (s) c3ex_fn<Th1_t>(0x000018f7)(s, 0, v);                           // 0x1003517c-0x10035181
    }
}

// 0x00037c00  Channel_set3DBufferStub (VA 0x10037c00). __thiscall, `ret 4`, one argument. Bit 5 of the byte at this+0x5c
// (0x10037c00 / 0x10037c04) and a non-NULL dword this+0x54 (0x10037c06-0x10037c0b): the argument is pushed and `call 0x10002522`
// (thunk to stubReturnBC 0x10011160) runs with ecx = that object (0x10037c0d-0x10037c12); its result is returned. Otherwise 0xb
// (0x10037c1a).
typedef unsigned(__fastcall* Set3DStub_t)(void*, void*, unsigned);
Set3DStub_t Set3DStub_orig;
unsigned __fastcall Set3DStub_re(void* self, void*, unsigned a) {
    if (fat<unsigned char>(self, 0x5c) & 0x20) {                              // 0x10037c00 / 0x10037c04
        void* b = fat<void*>(self, 0x54);                                     // 0x10037c06
        if (b) return c3ex_fn<Th1_t>(0x00002522)(b, 0, a);                    // 0x10037c09-0x10037c12
    }
    return 0xb;                                                               // 0x10037c1a
}

// 0x00010660  DsBuffer::getPosition (VA 0x10010660). __thiscall, `ret 0xc`, three arguments a1..a3. The descriptor this+0x5c NULL
// returns 1 (0x10010660-0x10010665, 0x10010696). Bit 5 of the byte at its +0x10 (0x10010667 / 0x1001066b): `call 0x1000115e`
// (thunk to DsBuffer::getTimedPosition 0x10010be0) with ecx = this and a1, a2, a3 (pushed a3 first, 0x1001066d-0x1001067c).
// Otherwise `call 0x10001695` (thunk to DsBuffer::getPlayPosition 0x100106b0) with ecx = this and a1, a2 (0x10010684-0x1001068e).
// The callee's result is returned.
typedef unsigned(__fastcall* DsGetPos_t)(void*, void*, unsigned, unsigned, unsigned);
DsGetPos_t DsGetPos_orig;
unsigned __fastcall DsGetPos_re(void* self, void*, unsigned a1, unsigned a2, unsigned a3) {
    void* d = fat<void*>(self, 0x5c);                                         // 0x10010660
    if (!d) return 1;                                                         // 0x10010665 / 0x10010696
    if (fat<unsigned char>(d, 0x10) & 0x20)                                   // 0x10010667 / 0x1001066b
        return c3ex_fn<Th3_t>(0x0000115e)(self, 0, a1, a2, a3);               // 0x1001067c
    return c3ex_fn<Th2_t>(0x00001695)(self, 0, a1, a2);                       // 0x1001068e
}

// 0x0000a050  Device::serviceVoices (VA 0x1000a050). __thiscall, plain `ret`. `call 0x10001f4b` (thunk to
// Midi_Device::dispatchCommand 0x1000a670) with ecx = this (0x1000a054). The list head this+0x40 is copied to the cursor this+0x48
// (0x1000a059-0x1000a060). Walk: a node's voice (+8) NULL, or a NULL cursor, ends it (0x1000a063-0x1000a06a, 0x1000a080-0x1000a096);
// a voice with bit 5 of the byte at +0xac (0x1000a06c / 0x1000a073) gets `call 0x10001c4e` (thunk to Voice::update 0x1001f8c0) with
// ecx = the voice and the dword this+0x34 (0x1000a075-0x1000a07b); the cursor is reloaded after that call (0x1000a080) and advanced
// to its +4 (0x1000a087-0x1000a08c). this+0x34 is cleared at the end (0x1000a098). Every exit is a `je` on the value just loaded
// being 0, so eax is 0 at the `ret`.
typedef unsigned(__fastcall* SvcVoices_t)(void*, void*);
SvcVoices_t SvcVoices_orig;
unsigned __fastcall SvcVoices_re(void* self, void*) {
    c3ex_fn<Th0_t>(0x00001f4b)(self, 0);                                      // 0x1000a054
    char* n = fat<char*>(self, 0x40);                                         // 0x1000a059
    fat<char*>(self, 0x48) = n;                                               // 0x1000a060
    if (n) {                                                                  // 0x1000a063
        char* v = fat<char*>(n, 8);                                           // 0x1000a065
        while (v) {                                                           // 0x1000a06a / 0x1000a096
            if (fat<unsigned char>(v, 0xac) & 0x20)                           // 0x1000a06c / 0x1000a073
                c3ex_fn<Th1_t>(0x00001c4e)(v, 0, fat<unsigned>(self, 0x34));  // 0x1000a075-0x1000a07b
            n = fat<char*>(self, 0x48);                                       // 0x1000a080
            if (!n) break;                                                    // 0x1000a085
            n = fat<char*>(n, 4);                                             // 0x1000a087
            fat<char*>(self, 0x48) = n;                                       // 0x1000a08c
            if (!n) break;                                                    // 0x1000a08f
            v = fat<char*>(n, 8);                                             // 0x1000a091
        }
    }
    fat<unsigned>(self, 0x34) = 0;                                            // 0x1000a098
    return 0;
}

// 0x00003f30  Sound::ctor (VA 0x10003f30). __thiscall, plain `ret`, wrapped in a C++ EH frame (0x10003f30-0x10003f3e, state byte
// 0x10003f8b) that no path of the body unwinds. `call 0x10002513` (thunk to Sound5b6a8_ctor 0x1001f4a0) with ecx = this
// (0x10003f4f); then the dwords this+0xa4c, +0xa50, +0xa54, +0xa58 are cleared (0x10003f5a-0x10003f66), the vtable 0x1005b000 is
// stored at this+0 (0x10003f7f), 0x40 dwords from this+0x948 are zeroed (`rep stosd` 0x10003f72-0x10003f85), `call 0x100018a2`
// (thunk to Sound_setSourceType 0x1002ad10) runs with ecx = this and 5 (0x10003f87-0x10003f90), this+0xa48 is cleared
// (0x10003f99) and this is returned (0x10003f9f).
typedef void*(__fastcall* SoundCtor_t)(void*, void*);
SoundCtor_t SoundCtor_orig;
void* __fastcall SoundCtor_re(void* self, void*) {
    c3ex_fn<Th0_t>(0x00002513)(self, 0);                                      // 0x10003f4f
    fat<unsigned>(self, 0xa4c) = 0;                                           // 0x10003f5a
    fat<unsigned>(self, 0xa50) = 0;                                           // 0x10003f60
    fat<unsigned>(self, 0xa54) = 0;                                           // 0x10003f66
    fat<unsigned>(self, 0xa58) = 0;                                           // 0x10003f6c
    fat<void*>(self, 0) = c3ex_sound(0x0005b000);                             // 0x10003f7f
    for (int k = 0; k < 0x40; ++k) fat<unsigned>(self, 0x948 + 4 * k) = 0;    // 0x10003f72-0x10003f85
    c3ex_fn<Th1_t>(0x000018a2)(self, 0, 5);                                   // 0x10003f90
    fat<unsigned>(self, 0xa48) = 0;                                           // 0x10003f99
    return self;                                                              // 0x10003f9f
}

// 0x0002fb70  enqueueWaveCmdE (VA 0x1002fb70). __thiscall, `ret 0xc`, three arguments a1..a3 (a2 is never read), wrapped in a
// C++ EH frame (0x1002fb70-0x1002fb7e) that no path unwinds. g_soundEngine 0x100b4a20 NULL returns 0 (its value, 0x1002fb86 /
// 0x1002fb8c / 0x1002fb90). Otherwise operator new 0x100424d7 allocates 0x10 bytes (0x1002fb92-0x1002fb94); a non-NULL block gets
// `call 0x10001299` (thunk to pairStore 0x10008810, which stores 0xe and this at +0 / +4 and returns the block) (0x1002fbac-0x1002fbb1);
// then a1 is stored at +8 and a3 at +0xc of the block (0x1002fbba-0x1002fbc5; a NULL block is written through, a fault of the
// original no vector takes), and `call 0x100011c7` (thunk to WaveCmdQueue::pushValue 0x1000c3d0) runs with ecx = g_soundEngine
// reloaded (0x1002fbc8) and the block (0x1002fbce-0x1002fbd7); its result is returned.
typedef unsigned(__fastcall* EnqE_t)(void*, void*, unsigned, unsigned, unsigned);
EnqE_t EnqE_orig;
unsigned __fastcall EnqE_re(void* self, void*, unsigned a1, unsigned, unsigned a3) {
    if (!c3ex_glob(0x000b4a20)) return 0;                                     // 0x1002fb86-0x1002fb90
    char* node = static_cast<char*>(c3ex_fn<OpNew_t>(0x000424d7)(0x10));      // 0x1002fb94
    if (node)                                                                 // 0x1002fba0 / 0x1002fbaa
        node = reinterpret_cast<char*>(c3ex_fn<Th2_t>(0x00001299)(node, 0, 0xe, reinterpret_cast<unsigned>(self)));   // 0x1002fbb1
    fat<unsigned>(node, 8) = a1;                                              // 0x1002fbc2
    fat<unsigned>(node, 0xc) = a3;                                            // 0x1002fbc5
    return c3ex_fn<Th1_t>(0x000011c7)(c3ex_glob(0x000b4a20), 0, reinterpret_cast<unsigned>(node));   // 0x1002fbc8-0x1002fbd7
}

// 0x0001e9a0  Track_setData (VA 0x1001e9a0). __thiscall, `ret 4`, one argument p. The old data this+0x3c non-NULL (0x1001e9a3 /
// 0x1001e9a8) with bit 2 of the byte at this+0x34 (0x1001e9aa / 0x1001e9ae) is freed by operator delete 0x1004249a (0x1001e9b0 /
// 0x1001e9b1) and this+0x3c cleared (0x1001e9b9). p is stored at this+0x3c and this+0x2c (0x1001e9c6 / 0x1001e9c9); a non-NULL p
// gets `call 0x1000215d` (thunk to Midi_scanTrack 0x1001ef80) with ecx = this (0x1001e9cc-0x1001e9d0), whose result is the result;
// a NULL p leaves eax = p = 0. this+0x40 is cleared (0x1001e9d5).
typedef unsigned(__fastcall* TrackSet_t)(void*, void*, unsigned);
TrackSet_t TrackSet_orig;
unsigned __fastcall TrackSet_re(void* self, void*, unsigned p) {
    void* old = fat<void*>(self, 0x3c);                                       // 0x1001e9a3
    if (old && (fat<unsigned char>(self, 0x34) & 4)) {                        // 0x1001e9a8-0x1001e9ae
        c3ex_fn<OpDelete_t>(0x0004249a)(old);                                 // 0x1001e9b1
        fat<void*>(self, 0x3c) = 0;                                           // 0x1001e9b9
    }
    fat<unsigned>(self, 0x3c) = p;                                            // 0x1001e9c6
    fat<unsigned>(self, 0x2c) = p;                                            // 0x1001e9c9
    unsigned r = p;                                                           // 0x1001e9c0
    if (p) r = c3ex_fn<Th0_t>(0x0000215d)(self, 0);                           // 0x1001e9cc-0x1001e9d0
    fat<unsigned>(self, 0x40) = 0;                                            // 0x1001e9d5
    return r;
}

// 0x000286f0  SoundWindowObj_subclass (VA 0x100286f0). __thiscall, `ret 4`, one argument hwnd. A non-zero this+8 (already
// subclassed) returns 0 (0x100286f4-0x100286f9, 0x10028729). Otherwise SetWindowLongA(hwnd, GWL_WNDPROC = -4, 0x100025ef)
// (0x100286fb-0x10028707; 0x100025ef is the thunk to soundWindowSubclassProc 0x10028b70) and its result is stored at this+0xc
// (0x1002870f). A zero result returns 0x18 (0x10028712-0x1002871b); otherwise hwnd is stored at this+8 (0x10028721), bit 2 of the
// byte at this+4 is set (0x1002871e-0x10028726) and 0 is returned (0x1002872a).
typedef unsigned(__fastcall* Subclass_t)(void*, void*, unsigned);
Subclass_t Subclass_orig;
unsigned __fastcall Subclass_re(void* self, void*, unsigned hwnd) {
    if (fat<unsigned>(self, 8)) return 0;                                     // 0x100286f4-0x100286f9
    unsigned r = c3ex_imp<Imp3_t>(kIatSetWindowLongA)(hwnd, -4,
                                                       reinterpret_cast<unsigned>(c3ex_sound(0x000025ef)));   // 0x10028707
    fat<unsigned>(self, 0xc) = r;                                             // 0x1002870f
    if (!r) return 0x18;                                                      // 0x10028712 / 0x10028715
    fat<unsigned>(self, 8) = hwnd;                                            // 0x10028721
    fat<unsigned char>(self, 4) |= 4;                                         // 0x1002871e-0x10028726
    return 0;                                                                 // 0x1002872a
}

// 0x0002a920  startSignalThread (VA 0x1002a920). __thiscall, plain `ret`. ResetEvent(this+0x24) runs first (0x1002a923-0x1002a927).
// Bit 0 of the byte at this+0 set (0x1002a92d-0x1002a931) returns at once with eax = ResetEvent's result whose low byte was
// replaced by that flag byte (`mov al, [esi]` 0x1002a92d). Otherwise the byte gets bit 0 (0x1002a935 / 0x1002a93e),
// _beginthread(0x10001366, 0, 0) runs (pushed 0, 0, 0x10001366: 0x1002a933-0x1002a940; 0x10001366 is the thunk to
// soundThreadPump 0x1002a970), its result is stored at this+0x20 (0x1002a948) and SetThreadPriority(that handle, 0)'s result is
// returned (0x1002a94b-0x1002a94e).
typedef unsigned(__fastcall* SigThread_t)(void*, void*);
SigThread_t SigThread_orig;
unsigned __fastcall SigThread_re(void* self, void*) {
    unsigned r = c3ex_imp<Imp1_t>(kIatResetEvent)(fat<unsigned>(self, 0x24));     // 0x1002a923-0x1002a927
    unsigned char b = fat<unsigned char>(self, 0);                            // 0x1002a92d
    if (b & 1) return (r & 0xffffff00u) | b;                                  // 0x1002a92f / 0x1002a931
    fat<unsigned char>(self, 0) = static_cast<unsigned char>(b | 1);          // 0x1002a935 / 0x1002a93e
    unsigned h = c3ex_fn<BeginThread_t>(0x00042984)(c3ex_sound(0x00001366), 0, 0);   // 0x1002a940
    fat<unsigned>(self, 0x20) = h;                                            // 0x1002a948
    return c3ex_imp<Imp2_t>(kIatSetThreadPriority)(h, 0);                     // 0x1002a94e
}

// =====================================================================================================================
// C. DirectSound buffer list (head 0x100b4a34, tail 0x100b4a38; links prev +0xbc, next +0xc0)
// =====================================================================================================================
// DsBuffer::releaseIfOwned 0x10010060 through the thunk 0x100012d5, DsBuffer::destroy 0x1000ef60 through 0x10001910 (both
// thiscall, nothing pushed), operator delete 0x1004249a.
char*& c3ex_dsHead() { return *reinterpret_cast<char**>(c3ex_sound(0x000b4a34)); }
char*& c3ex_dsTail() { return *reinterpret_cast<char**>(c3ex_sound(0x000b4a38)); }
unsigned c3ex_releaseIfOwned(void* b) { return c3ex_fn<Th0_t>(0x000012d5)(b, 0); }
void c3ex_destroyDelete(void* b) {
    c3ex_fn<Th0_t>(0x00001910)(b, 0);
    c3ex_fn<OpDelete_t>(0x0004249a)(b);
}
// unlink b from the list: its prev's next (or the head) takes b's next, its next's prev (or the tail) takes b's prev, b's links
// are cleared (the same eight instructions at 0x100133ea-0x10013421, 0x1001331f-0x1001335a and 0x10011981-0x100119b8)
void c3ex_dsUnlink(char* b) {
    char* p = fat<char*>(b, 0xbc);
    char* n = fat<char*>(b, 0xc0);
    if (p) fat<char*>(p, 0xc0) = n; else c3ex_dsHead() = n;
    if (n) fat<char*>(n, 0xbc) = p; else c3ex_dsTail() = p;
    fat<char*>(b, 0xbc) = 0;
    fat<char*>(b, 0xc0) = 0;
}

// 0x000133d0  DirectSoundDevice::releaseAllBuffers (VA 0x100133d0). __thiscall, plain `ret`. Bit 1 of the byte at this+0x6c clear
// returns 3 (0x100133d0-0x100133d6). Otherwise, while the list head 0x100b4a34 is non-NULL (0x100133dd-0x100133e8, reloaded at
// 0x10013440-0x10013448), the head buffer is unlinked (0x100133ea-0x10013421), DsBuffer::releaseIfOwned runs on it (0x10013427) and,
// the pointer being non-NULL (0x1001342c / 0x1001342e), DsBuffer::destroy (0x10013432) and operator delete (0x10013438) follow.
// 0 is returned (0x1001344b).
typedef unsigned(__fastcall* DsRelAllBufs_t)(void*, void*);
DsRelAllBufs_t DsRelAllBufs_orig;
unsigned __fastcall DsRelAllBufs_re(void* self, void*) {
    if (!(fat<unsigned char>(self, 0x6c) & 2)) return 3;                      // 0x100133d0-0x100133d6
    for (char* b = c3ex_dsHead(); b; b = c3ex_dsHead()) {                     // 0x100133dd-0x100133e8, 0x10013440-0x10013448
        c3ex_dsUnlink(b);                                                     // 0x100133ea-0x10013421
        c3ex_releaseIfOwned(b);                                               // 0x10013427
        if (b) c3ex_destroyDelete(b);                                         // 0x1001342c-0x10013438
    }
    return 0;                                                                 // 0x1001344b
}

// 0x000132e0  DirectSoundDevice::removeBuffer (VA 0x100132e0). __thiscall, `ret 4`, one argument b. Bit 1 of the byte at this+0x6c
// clear returns 3 (0x100132e0-0x100132e8); a NULL b returns 0xa (0x100132f5-0x100132f9). The list from 0x100b4a34 is searched
// for b (0x10013302-0x10013317); when found (0x1001331b) b is unlinked (0x1001331f-0x1001335a). Then `mov eax, [b+0x98]; or eax, 4;
// je` (0x10013365-0x1001336e) never jumps, so this+0x158 is decremented on every call (0x10013370). DsBuffer::releaseIfOwned(b)
// returning 0 (0x10013378-0x1001337f) is followed by DsBuffer::destroy and operator delete of b (0x10013381-0x10013389). 0 is
// returned (0x10013391).
typedef unsigned(__fastcall* DsRemoveBuf_t)(void*, void*, char*);
DsRemoveBuf_t DsRemoveBuf_orig;
unsigned __fastcall DsRemoveBuf_re(void* self, void*, char* b) {
    if (!(fat<unsigned char>(self, 0x6c) & 2)) return 3;                      // 0x100132e4-0x100132e8
    if (!b) return 0xa;                                                       // 0x100132f5-0x100132f9
    char* e = c3ex_dsHead();                                                  // 0x10013302
    while (e && e != b) e = fat<char*>(e, 0xc0);                              // 0x10013307-0x10013317
    if (e) c3ex_dsUnlink(e);                                                  // 0x1001331b-0x1001335a
    if (fat<unsigned>(b, 0x98) | 4) --fat<int>(self, 0x158);                  // 0x10013365-0x10013370
    if (!c3ex_releaseIfOwned(b)) c3ex_destroyDelete(b);                       // 0x10013378-0x10013389
    return 0;                                                                 // 0x10013391
}

// 0x00011970  DirectSoundDevice::releaseAll (VA 0x10011970). __thiscall, plain `ret`. The buffer list is emptied as in
// releaseAllBuffers (0x10011972-0x100119df, no flag test first). Bit 1 of the byte at this+0x6c (0x100119e1 / 0x100119e5) runs
// DsBuffer::releaseIfOwned on the embedded buffer this+0x70 (0x100119e7 / 0x100119ea). A buffer at this+0x198 (0x100119ef-0x100119f7)
// gets DsBuffer::releaseIfOwned (0x100119f9); the field is reloaded and, still non-NULL (0x100119fe-0x10011a06), destroyed and
// deleted (0x10011a08-0x10011a10); the field is cleared (0x10011a18). The interface at this+0 non-NULL (0x10011a1e-0x10011a22) is
// released through its vtable slot +8 (stdcall, the interface pushed, 0x10011a24-0x10011a27) and cleared (0x10011a2a). this+8 is
// cleared (0x10011a32), this+0x18c is incremented and its new value returned (0x10011a2c-0x10011a36).
typedef unsigned(__fastcall* DsRelAll_t)(void*, void*);
DsRelAll_t DsRelAll_orig;
unsigned __fastcall DsRelAll_re(void* self, void*) {
    for (char* b = c3ex_dsHead(); b; b = c3ex_dsHead()) {                     // 0x10011972-0x1001197f, 0x100119d7-0x100119df
        c3ex_dsUnlink(b);                                                     // 0x10011981-0x100119b8
        c3ex_releaseIfOwned(b);                                               // 0x100119be
        if (b) c3ex_destroyDelete(b);                                         // 0x100119c3-0x100119cf
    }
    if (fat<unsigned char>(self, 0x6c) & 2) c3ex_releaseIfOwned(&fat<char>(self, 0x70));   // 0x100119e1-0x100119ea
    if (fat<void*>(self, 0x198)) {                                            // 0x100119ef-0x100119f7
        c3ex_releaseIfOwned(fat<void*>(self, 0x198));                         // 0x100119f9
        void* c = fat<void*>(self, 0x198);                                    // 0x100119fe
        if (c) c3ex_destroyDelete(c);                                         // 0x10011a04-0x10011a10
        fat<void*>(self, 0x198) = 0;                                          // 0x10011a18
    }
    void* iface = fat<void*>(self, 0);                                        // 0x10011a1e
    if (iface) {                                                              // 0x10011a20 / 0x10011a22
        (*reinterpret_cast<Imp1_t*>(*reinterpret_cast<char**>(iface) + 8))(reinterpret_cast<unsigned>(iface));   // 0x10011a27
        fat<void*>(self, 0) = 0;                                              // 0x10011a2a
    }
    fat<unsigned>(self, 8) = 0;                                               // 0x10011a32
    return ++fat<unsigned>(self, 0x18c);                                      // 0x10011a2c-0x10011a36
}

// =====================================================================================================================
// D. MultiSound start paths (channel at this+0x70; Channel_resetPanState thunk 0x10001866, Channel_allocAndInitVoice 0x10001839,
//    serviceChannelPlayback 0x10001f37, Channel_setVolume 0x100015aa, WaveCmdQueue::pushNode 0x100020b3 on g_soundEngine)
// =====================================================================================================================
void c3ex_pushNode1(void* self) {
    c3ex_fn<Th2_t>(0x000020b3)(c3ex_glob(0x000b4a20), 0, reinterpret_cast<unsigned>(self), 1);
}
// the closing test both start paths share: slot +0xcc returning 0, or bit 4 of this+0x58, or bit 6 of this+0xcc queues command 1
bool c3ex_msNeedsPush(void* self) {
    if (!c3ex_vslot(self, 0xcc)(self, 0)) return true;
    if ((fat<unsigned>(self, 0x58) >> 4) & 1) return true;
    return ((fat<unsigned>(self, 0xcc) >> 6) & 1) != 0;
}

// 0x00039f30  MultiSound::startVoice (VA 0x10039f30). __thiscall, `ret 4`, one argument out (a dword pointer). *out = -1
// (0x10039f38). Bit 0 of the dword this+0xc8 clear returns 0x21 (0x10039f3e-0x10039f49). Without bit 1 (0x10039f52 / 0x10039f54)
// the channel's pan state is reset (0x10039f56 / 0x10039f59), then bit 1 of the byte at this+0x58 clear returns 0x14 (0x10039f5e /
// 0x10039f62, 0x10039f7d), and this's slot +0x10 with this+0x50 returning non-zero returns 8 (0x10039f64-0x10039f74).
// Channel_allocAndInitVoice(out) on the channel gives r (0x10039f87-0x10039f90). r = 0: bit 6 of the byte at this+0xc8 returns 0
// (0x10039f96-0x10039fa0), otherwise command 1 is queued on g_soundEngine (read without a NULL test, 0x10039fa6-0x10039faf). Then bit 6
// returns 0 (0x10039fb4-0x10039fbe); otherwise slot +0xcc returning 0, bit 4 of this+0x58 or bit 6 of this+0xcc queues command 1
// (0x10039fc4-0x10039ff4). r is returned (0x10039ff9).
typedef unsigned(__fastcall* MsStartVoice_t)(void*, void*, int*);
MsStartVoice_t MsStartVoice_orig;
unsigned __fastcall MsStartVoice_re(void* self, void*, int* out) {
    *out = -1;                                                                // 0x10039f38
    unsigned f = fat<unsigned>(self, 0xc8);                                   // 0x10039f3e
    if (!(f & 1)) return 0x21;                                                // 0x10039f44 / 0x10039f46
    if (!(f & 2)) {                                                           // 0x10039f52 / 0x10039f54
        c3ex_fn<Th0_t>(0x00001866)(&fat<char>(self, 0x70), 0);                // 0x10039f59
        if (!(fat<unsigned char>(self, 0x58) & 2)) return 0x14;               // 0x10039f5e / 0x10039f62
        if (c3ex_vfn<Th1_t>(self, 0x10)(self, 0, fat<unsigned>(self, 0x50))) return 8;   // 0x10039f6c-0x10039f74
    }
    unsigned r = c3ex_fn<Th1_t>(0x00001839)(&fat<char>(self, 0x70), 0, reinterpret_cast<unsigned>(out));   // 0x10039f8b
    if (!r) {                                                                 // 0x10039f92 / 0x10039f94
        if (fat<unsigned char>(self, 0xc8) & 0x40) return 0;                  // 0x10039f96-0x10039fa0
        c3ex_pushNode1(self);                                                 // 0x10039fa6-0x10039faf
    }
    if (fat<unsigned char>(self, 0xc8) & 0x40) return 0;                      // 0x10039fb4-0x10039fbe
    if (c3ex_msNeedsPush(self)) c3ex_pushNode1(self);                         // 0x10039fc4-0x10039ff4
    return r;                                                                 // 0x10039ff9
}

// 0x00039ce0  MultiSound_start (VA 0x10039ce0). __thiscall, plain `ret`. Bit 6 of this+0xc8 without bit 0 returns 0xe
// (0x10039ce3-0x10039cf1). g_soundEngine NULL (0x10039cf8-0x10039d00) or its slot +0x6c non-zero (0x10039d06-0x10039d0d) returns 1
// (0x10039e0f). Bit 0 of the byte at this+0x1428 returns 0x28 (0x10039d13-0x10039d1c). Without bit 1 of this+0xc8 (0x10039d23 /
// 0x10039d2a) the pan state is reset (0x10039d2f), then bit 1 of the byte at this+0x58 clear returns 0x14 (0x10039d34 / 0x10039d38,
// 0x10039d50) and slot +0x10 with this+0x50 non-zero returns 8 (0x10039d3a-0x10039d49). With bit 0 of this+0xc8 and slot +0xcc
// returning 0, serviceChannelPlayback runs on the channel (0x10039d57-0x10039d71). Bit 6 of this+0xc8 returns 0 (0x10039d76-0x10039d7f).
// Bit 2 or bit 6 of this+0xcc (0x10039d83-0x10039d92) sets the channel volume to this+4 and services the channel again
// (0x10039d94-0x10039da5). With bit 0 of this+0xc8, slot +0xcc non-zero and bit 1 of the byte at this+0x58, command 1 is queued
// (0x10039dab-0x10039dd1); then the shared closing test queues it again (0x10039dd6-0x10039e06). 0 is returned (0x10039e0b).
typedef unsigned(__fastcall* MsStart_t)(void*, void*);
MsStart_t MsStart_orig;
unsigned __fastcall MsStart_re(void* self, void*) {
    unsigned f = fat<unsigned>(self, 0xc8);                                   // 0x10039ce3
    if ((f & 0x40) && !(f & 1)) return 0xe;                                   // 0x10039ce9-0x10039cf1
    void* eng = c3ex_glob(0x000b4a20);                                        // 0x10039cf8
    if (!eng) return 1;                                                       // 0x10039cfe / 0x10039d00
    if (c3ex_vslot(eng, 0x6c)(eng, 0)) return 1;                              // 0x10039d08-0x10039d0d
    if (fat<unsigned char>(self, 0x1428) & 1) return 0x28;                    // 0x10039d13-0x10039d1c
    char* ch = &fat<char>(self, 0x70);
    if (!(fat<unsigned char>(self, 0xc8) & 2)) {                              // 0x10039d23 / 0x10039d2a
        c3ex_fn<Th0_t>(0x00001866)(ch, 0);                                    // 0x10039d2f
        if (!(fat<unsigned char>(self, 0x58) & 2)) return 0x14;               // 0x10039d34 / 0x10039d38
        if (c3ex_vfn<Th1_t>(self, 0x10)(self, 0, fat<unsigned>(self, 0x50))) return 8;   // 0x10039d42-0x10039d49
    }
    if ((fat<unsigned char>(self, 0xc8) & 1) && !c3ex_vslot(self, 0xcc)(self, 0))   // 0x10039d57-0x10039d6c
        c3ex_fn<Th0_t>(0x00001f37)(ch, 0);                                    // 0x10039d71
    if (fat<unsigned char>(self, 0xc8) & 0x40) return 0;                      // 0x10039d76-0x10039d7f
    unsigned v = fat<unsigned>(self, 0xcc);                                   // 0x10039d83
    if ((v & 4) || ((v >> 6) & 1)) {                                          // 0x10039d89-0x10039d92
        c3ex_fn<Th1_t>(0x000015aa)(ch, 0, fat<unsigned>(self, 4));            // 0x10039d9e
        c3ex_fn<Th0_t>(0x00001f37)(ch, 0);                                    // 0x10039da5
    }
    if ((fat<unsigned char>(self, 0xc8) & 1) && c3ex_vslot(self, 0xcc)(self, 0) &&
        (fat<unsigned char>(self, 0x58) & 2))                                 // 0x10039dab-0x10039dc6
        c3ex_pushNode1(self);                                                 // 0x10039dd1
    if (c3ex_msNeedsPush(self)) c3ex_pushNode1(self);                         // 0x10039dd6-0x10039e06
    return 0;                                                                 // 0x10039e0b
}

// =====================================================================================================================
// E. sequencer track walks (the track list at this+0x60; Seq_listGetAt thunk 0x100015fa, Midi_readVarLen 0x1000139d (cdecl),
//    Track_advancePos 0x10001c7b, Midi_Device::playEvent 0x10002068, Seq_preRollTrack 0x10001956, Track_setData 0x10002298)
// =====================================================================================================================
typedef unsigned(__cdecl* ReadVarLen_t)(void*, unsigned*);
unsigned c3ex_listGetAt(void* self, void** out, unsigned i) {
    return c3ex_fn<Th2_t>(0x000015fa)(&fat<char>(self, 0x60), 0, reinterpret_cast<unsigned>(out), i);
}
unsigned c3ex_readVarLen(void* p, unsigned* v) { return c3ex_fn<ReadVarLen_t>(0x0000139d)(p, v); }
void c3ex_advance(void* t, unsigned n) { c3ex_fn<Th1_t>(0x00001c7b)(t, 0, n); }
unsigned c3ex_playEvent(void* self, void* t, unsigned* v) {
    return c3ex_fn<Th2_t>(0x00002068)(self, 0, reinterpret_cast<unsigned>(t), reinterpret_cast<unsigned>(v));
}
unsigned c3ex_trackCount(void* self) { return c3ex_vslot(self, 0xb4)(self, 0); }

// The event walk Seq_stepTrackMeta and Seq_preRollTrack share: from the track's read pointer t+0x2c, a delta is read; while it
// reads (result 0) and the event's status nibble (byte +1 & 0xf0) is neither 0x90 nor 0xc0, the delta is consumed, the event is
// played with a count slot cleared to 0, a playEvent result of 0x29 stops with 0x29, the count the player left is consumed and the
// next delta is read. Any other end returns 0.
unsigned c3ex_walkEvents(void* self, void* t, unsigned* v) {
    unsigned char* p = fat<unsigned char*>(t, 0x2c);
    if (c3ex_readVarLen(p, v)) return 0;
    for (;;) {
        unsigned char st = static_cast<unsigned char>(p[1] & 0xf0);
        if (st == 0x90 || st == 0xc0) return 0;
        c3ex_advance(t, *v);
        *v = 0;
        if (c3ex_playEvent(self, t, v) == 0x29) return 0x29;
        c3ex_advance(t, *v);
        p = fat<unsigned char*>(t, 0x2c);
        if (c3ex_readVarLen(p, v)) return 0;
    }
}

// 0x00020720  Seq_stepTrackMeta (VA 0x10020720). __thiscall, `ret 4`, one argument t (a track). NULL returns 0xa (0x10020727-0x1002072f);
// bit 1 of the byte at t+0x34 returns 0x29 (0x10020738-0x10020740). Then the event walk: Midi_readVarLen(t+0x2c, &v) (0x10020749-0x10020752;
// v is the argument's own stack slot); a non-zero result returns 0 (0x1002075a / 0x1002075c). While the status byte at +1 masked with 0xf0
// is neither 0x90 nor 0xc0 (0x1002075e-0x10020769): Track_advancePos(v) (0x10020772), v = 0 and Midi_Device::playEvent(t, &v) with
// ecx = this (0x10020777-0x10020787), whose 0x29 returns 0x29 (0x1002078c / 0x1002078f); Track_advancePos(v) again (0x10020798) and the next
// delta from t+0x2c (0x1002079d-0x100207a6); a zero result loops (0x100207b0). 0 is returned (0x100207b4).
typedef unsigned(__fastcall* StepMeta_t)(void*, void*, void*);
StepMeta_t StepMeta_orig;
unsigned __fastcall StepMeta_re(void* self, void*, void* t) {
    if (!t) return 0xa;                                                       // 0x10020727-0x1002072f
    if (fat<unsigned char>(t, 0x34) & 2) return 0x29;                         // 0x10020738-0x10020740
    unsigned v = 0;
    return c3ex_walkEvents(self, t, &v);                                      // 0x10020749-0x100207b4
}

// 0x000205f0  Seq_preRollTrack (VA 0x100205f0). __thiscall, `ret 4`, one argument i (a track index). this's slot +0xb4 (the track count)
// at or below i (unsigned, `jb` 0x10020606) returns 0xa. Seq_listGetAt(&t, i) on this+0x60 (0x10020616-0x1002061f) returning 0x24 returns
// 0x24 (0x10020624 / 0x10020627); any other result goes on with the track slot as it stands. Bit 1 of the byte at t+0x34 returns 0x29
// (0x10020632-0x1002063e). The 0x80 dwords from t+0x58 are zeroed (`rep stosd` 0x1002064a-0x10020654) and the event walk of
// Seq_stepTrackMeta runs on t (0x10020656-0x100206cb). 0 is returned (0x100206cf).
typedef unsigned(__fastcall* PreRoll_t)(void*, void*, unsigned);
PreRoll_t PreRoll_orig;
unsigned __fastcall PreRoll_re(void* self, void*, unsigned i) {
    if (!(i < c3ex_trackCount(self))) return 0xa;                             // 0x100205f8-0x10020606
    void* t = 0;
    if (c3ex_listGetAt(self, &t, i) == 0x24) return 0x24;                     // 0x1002061f-0x10020627
    if (fat<unsigned char>(t, 0x34) & 2) return 0x29;                         // 0x10020636-0x1002063e
    for (int k = 0; k < 0x80; ++k) fat<unsigned>(t, 0x58 + 4 * k) = 0;        // 0x1002064a-0x10020654
    unsigned v = 0;
    return c3ex_walkEvents(self, t, &v);                                      // 0x10020656-0x100206cf
}

// 0x00024500  Seq_selectTrack (VA 0x10024500). __thiscall, `ret 8`, arguments data and i. The current track (index this+0x20c) is
// looked up into cur (0x10024508-0x10024519); a non-zero result sets cur = NULL (0x1002451e-0x10024522). Track i is looked up into t
// (0x1002452a-0x10024536); a non-zero result is returned (0x1002453b / 0x1002453d). Track_setData(data) runs on t (0x1002453f-0x10024548)
// and bit 2 of t+0x34 is cleared (0x1002454d-0x10024557). this+0x20c becomes i when cur is NULL or cur+0x54 is below t+0x54 (unsigned,
// `jae` 0x1002456c; 0x1002455a-0x1002456e). Seq_preRollTrack(i) (0x10024574-0x10024577) runs and 0 is returned (0x1002457c).
typedef unsigned(__fastcall* SelectTrack_t)(void*, void*, unsigned, unsigned);
SelectTrack_t SelectTrack_orig;
unsigned __fastcall SelectTrack_re(void* self, void*, unsigned data, unsigned i) {
    void* cur = 0;
    if (c3ex_listGetAt(self, &cur, fat<unsigned>(self, 0x20c))) cur = 0;      // 0x10024519-0x10024522
    void* t = 0;
    unsigned r = c3ex_listGetAt(self, &t, i);                                 // 0x10024536
    if (r) return r;                                                          // 0x1002453d
    c3ex_fn<Th1_t>(0x00002298)(t, 0, data);                                   // 0x10024548
    fat<unsigned>(t, 0x34) &= 0xfffffffbu;                                    // 0x10024551-0x10024557
    if (!cur || fat<unsigned>(cur, 0x54) < fat<unsigned>(t, 0x54))            // 0x1002455e-0x1002456c
        fat<unsigned>(self, 0x20c) = i;                                       // 0x1002456e
    c3ex_fn<Th1_t>(0x00001956)(self, 0, i);                                   // 0x10024577
    return 0;                                                                 // 0x1002457c
}

// pre-roll every track: i from 0 while i < this's slot +0xb4 (called again after each pre-roll, unsigned `jb`), then bit 10 of the
// track list's +0x4c (this+0xac) is set and this+0x720 / +0x71c / +0x724 are cleared
void c3ex_preRollAll(void* self) {
    unsigned n = c3ex_trackCount(self);
    if (n) {
        unsigned i = 0;
        do {
            c3ex_fn<Th1_t>(0x00001956)(self, 0, i);
            n = c3ex_trackCount(self);
            ++i;
        } while (i < n);
    }
    fat<unsigned>(self, 0x60 + 0x4c) |= 0x400;
    fat<unsigned>(self, 0x720) = 0;
    fat<unsigned>(self, 0x71c) = 0;
    fat<unsigned>(self, 0x724) = 0;
}

// 0x00020290  Seq::loadFile (VA 0x10020290). __thiscall, `ret 4`, one argument name. Bit 6 of the byte at this+0x214 (0x10020296 /
// 0x1002029d) first runs `call 0x10001997` (thunk to pumpWaveMessages 0x10028810) with ecx = the module object 0x100b49b8 (0x1002029f-
// 0x100202a4). r = Midi_Device::loadFile(name) on the track list this+0x60 (thunk 0x1000270c, 0x100202a9-0x100202b3); Seq_listGetAt(&x, 1) is
// called and its result dropped (0x100202b8-0x100202c3). A non-zero r is returned (0x100202c8 / 0x100202ca). Otherwise every track is
// pre-rolled (0x100202cc-0x100202f2), bit 10 of this+0xac is set (`or ah, 4` 0x100202f4-0x100202fb), this+0x720 / +0x71c / +0x724 are
// cleared (0x100202fe-0x1002030c) and r (0) is returned.
typedef unsigned(__fastcall* SeqLoadFile_t)(void*, void*, unsigned);
SeqLoadFile_t SeqLoadFile_orig;
unsigned __fastcall SeqLoadFile_re(void* self, void*, unsigned name) {
    if (fat<unsigned char>(self, 0x214) & 0x40)                               // 0x10020296 / 0x1002029d
        c3ex_fn<Th0_t>(0x00001997)(c3ex_sound(0x000b49b8), 0);                // 0x1002029f-0x100202a4
    unsigned r = c3ex_fn<Th1_t>(0x0000270c)(&fat<char>(self, 0x60), 0, name); // 0x100202b3
    void* x = 0;
    c3ex_listGetAt(self, &x, 1);                                              // 0x100202c3
    if (r) return r;                                                          // 0x100202ca
    c3ex_preRollAll(self);                                                    // 0x100202cc-0x1002030c
    return r;
}

// 0x00020360  Midi_loadMemoryAndPreRoll (VA 0x10020360). __thiscall, `ret 8`, arguments mem and a2 (a2 is never read). r =
// Midi_Device::loadFromMemory(mem) on this+0x60 (thunk 0x1000169f, 0x10020363-0x10020373), kept in mem's stack slot (0x10020383) and
// returned on every path (0x10020419 / 0x10020427). Seq_listGetAt(&x, 1) runs, result dropped (0x10020378-0x10020387). r = 0: every track is
// pre-rolled and the fields reset as in Seq::loadFile (0x10020392-0x100203d2). Then with this+0x224 zero (0x100203d8 / 0x100203de) each
// track i < slot +0xb4 (called again each pass) that Seq_listGetAt finds (result 0, 0x100203ff / 0x10020401) gets bit 3 of its +0x38 byte
// (0x10020403-0x10020407).
typedef unsigned(__fastcall* LoadMemPre_t)(void*, void*, unsigned, unsigned);
LoadMemPre_t LoadMemPre_orig;
unsigned __fastcall LoadMemPre_re(void* self, void*, unsigned mem, unsigned) {
    unsigned r = c3ex_fn<Th1_t>(0x0000169f)(&fat<char>(self, 0x60), 0, mem);  // 0x10020373
    void* x = 0;
    c3ex_listGetAt(self, &x, 1);                                              // 0x10020387
    if (!r) c3ex_preRollAll(self);                                            // 0x1002038e-0x100203d2
    if (fat<unsigned>(self, 0x224)) return r;                                 // 0x100203d8 / 0x100203de
    unsigned n = c3ex_trackCount(self);                                       // 0x100203e6
    if (n) {                                                                  // 0x100203ec / 0x100203ee
        unsigned i = 0;
        do {
            void* t = 0;
            if (!c3ex_listGetAt(self, &t, i)) fat<unsigned char>(t, 0x38) |= 8;   // 0x100203fa-0x10020407
            n = c3ex_trackCount(self);                                        // 0x1002040f
            ++i;
        } while (i < n);                                                      // 0x10020415 / 0x10020417
    }
    return r;                                                                 // 0x10020419
}

}  // namespace

SG_HOOK("sound.dll", 0x00039450, setField6cReturn0, SetF6c_re, SetF6c_orig);
SG_HOOK("sound.dll", 0x00039470, clearField6cReturn0, ClrF6c_re, ClrF6c_orig);
SG_HOOK("sound.dll", 0x0002f680, Ch5c2ec_forwardSlot37, Fwd37_re, Fwd37_orig);
SG_HOOK("sound.dll", 0x00031080, Channel_start, ChStart_re, ChStart_orig);
SG_HOOK("sound.dll", 0x0002deb0, SubChannel_openSourceType2, OpenSrc2_re, OpenSrc2_orig);
SG_HOOK("sound.dll", 0x0002eb40, openSourceType4, OpenSrc4_re, OpenSrc4_orig);
SG_HOOK("sound.dll", 0x0002f060, openSourceType6, OpenSrc6_re, OpenSrc6_orig);
SG_HOOK("sound.dll", 0x0002f5e0, openSourceType7, OpenSrc7_re, OpenSrc7_orig);
SG_HOOK("sound.dll", 0x0002bee0, Device_createStartQueue, CreateStartQ_re, CreateStartQ_orig);
SG_HOOK("sound.dll", 0x0002bd80, Device_lazyCreateSound, LazyCreate_re, LazyCreate_orig);
SG_HOOK("sound.dll", 0x0002bc60, Device_prepareSoundFlags, PrepFlags_re, PrepFlags_orig);
SG_HOOK("sound.dll", 0x0002b7c0, startDevicePlayback, StartDevPlay_re, StartDevPlay_orig);
SG_HOOK("sound.dll", 0x00039b30, SoundGroup_stop, GroupStop_re, GroupStop_orig);
SG_HOOK("sound.dll", 0x00035150, Channel_setVolume, ChSetVol_re, ChSetVol_orig);
SG_HOOK("sound.dll", 0x00037c00, Channel_set3DBufferStub, Set3DStub_re, Set3DStub_orig);
SG_HOOK("sound.dll", 0x00010660, DsBuffer_getPosition, DsGetPos_re, DsGetPos_orig);
SG_HOOK("sound.dll", 0x0000a050, Device_serviceVoices, SvcVoices_re, SvcVoices_orig);
SG_HOOK("sound.dll", 0x00003f30, Sound_ctor, SoundCtor_re, SoundCtor_orig);
SG_HOOK("sound.dll", 0x0002fb70, enqueueWaveCmdE, EnqE_re, EnqE_orig);
SG_HOOK("sound.dll", 0x0001e9a0, Track_setData, TrackSet_re, TrackSet_orig);
SG_HOOK("sound.dll", 0x000286f0, SoundWindowObj_subclass, Subclass_re, Subclass_orig);
SG_HOOK("sound.dll", 0x0002a920, startSignalThread, SigThread_re, SigThread_orig);
SG_HOOK("sound.dll", 0x000133d0, DirectSoundDevice_releaseAllBuffers, DsRelAllBufs_re, DsRelAllBufs_orig);
SG_HOOK("sound.dll", 0x000132e0, DirectSoundDevice_removeBuffer, DsRemoveBuf_re, DsRemoveBuf_orig);
SG_HOOK("sound.dll", 0x00011970, DirectSoundDevice_releaseAll, DsRelAll_re, DsRelAll_orig);
SG_HOOK("sound.dll", 0x00039f30, MultiSound_startVoice, MsStartVoice_re, MsStartVoice_orig);
SG_HOOK("sound.dll", 0x00039ce0, MultiSound_start, MsStart_re, MsStart_orig);
SG_HOOK("sound.dll", 0x00020720, Seq_stepTrackMeta, StepMeta_re, StepMeta_orig);
SG_HOOK("sound.dll", 0x000205f0, Seq_preRollTrack, PreRoll_re, PreRoll_orig);
SG_HOOK("sound.dll", 0x00024500, Seq_selectTrack, SelectTrack_re, SelectTrack_orig);
SG_HOOK("sound.dll", 0x00020290, Seq_loadFile, SeqLoadFile_re, SeqLoadFile_orig);
SG_HOOK("sound.dll", 0x00020360, Midi_loadMemoryAndPreRoll, LoadMemPre_re, LoadMemPre_orig);

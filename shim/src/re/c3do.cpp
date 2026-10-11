// C3 batch c3do (round 27, 2026-10-11) of sound.dll: the reachable C2 rows round 26's c3dj listed without a blocker
// (log/c3/c3dj_nottaken.tsv) plus the mixer family. Every function runs on fixture-owned objects and buffers; allocation and release go
// through the DLL's own operator new / operator delete at their original addresses (heap stand-in for the keys that need it).
// Addresses in SG_HOOK and in the `// 0x...` comment lines are RVAs; the instruction addresses quoted inside each body are
// VAs (VA = 0x10000000 + RVA), which is how re/tools/asm2inline.py prints sound.dll.
// Hand-written from the disassembly (py -3.12 re/tools/asm2inline.py sound.dll 0x<va> --list; incremental-link thunks resolved with
// log/c3/alt/c3do/thunk.py). sound.dll is LoadLibrary'd after the shim starts, so these hooks install from the LoadLibraryA hook
// (SgHooksInstallPending, re/hooks.cpp).
//
// RULES (log/c3/c3cy_notes.txt, c3dh, c3dj): __thiscall is emulated with __fastcall (ecx = this, edx unused). The DLL has base
// relocations, so every vtable address, callee and global is addressed through the loaded base, never through the 0x10000000-based
// VA the disassembly prints. Callees run through their original addresses (an incremental-link thunk's target is named in the comment).
//
// Groups:
//   A. list-owning destructor bodies (ChainObject_dtor, Ctrl5b58c_dtor, Ctrl5b58c_dtorEmbedded, MidiReader_freeContent) and wrappers
//   B. constructors (Sound5c450_ctor, StreamReader_ctor, WaveStream::ctor, DsBuffer::ctor) and Channel_propagateFlag0
#include "hooks.h"
#include <stdint.h>

namespace {

template <typename T> T& fat(void* base, int off) {
    return *reinterpret_cast<T*>(reinterpret_cast<char*>(base) + off);
}

char* c3do_sound(unsigned rva) {
    static HMODULE m = 0;
    if (!m) m = GetModuleHandleA("sound.dll");
    return reinterpret_cast<char*>(m) + rva;
}
template <typename F> F c3do_fn(unsigned rva) { return reinterpret_cast<F>(c3do_sound(rva)); }

typedef void*(__fastcall* Th0p_t)(void*, void*);                    // thiscall, nothing pushed, returns a pointer
typedef unsigned(__fastcall* Th0u_t)(void*, void*);                 // thiscall, nothing pushed, returns an int
typedef void(__fastcall* Th0v_t)(void*, void*);                     // thiscall, no result used
typedef unsigned(__fastcall* Th1_t)(void*, void*, unsigned);        // thiscall, one stack argument
typedef void(__cdecl* OpDelete_t)(void*);                           // operator delete 0x1004249a
void c3do_delete(void* p) { c3do_fn<OpDelete_t>(0x0004249a)(p); }

// A virtual slot of an object: [[obj] + off].
static inline void* c3do_slot(void* obj, unsigned off) { return fat<void*>(fat<void*>(obj, 0), off); }

// =====================================================================================================================
// A. List-owning destructor bodies
// =====================================================================================================================

// The four bodies 0x0001b570 ChainObject_dtor, 0x0001c990 Ctrl5b58c_dtorEmbedded, 0x0001c080 Ctrl5b58c_dtor and 0x0001b870
// MidiReader_freeContent (VAs 0x1001b570 / 0x1001c990 / 0x1001c080 / 0x1001b870) end in the same two loops over a list whose header
// sits in the object: head +0xc, tail +0x10, count +0x18; a node is { prev +0, next +4, value +8 }. Popping the head
// (0x1001b5ab-0x1001b5c6): next = node->next (0x1001b5b2); when next != NULL, next->prev = 0 and head = next (0x1001b5b9 / 0x1001b5bb);
// otherwise tail = head = 0 (0x1001b5c0 / 0x1001b5c3); value = node->value (0x1001b5c6); operator delete(node) (0x1001b5ca); count -= 1
// (0x1001b5cf-0x1001b5d8).
//   Loop 1 (0x1001b5ab-0x1001b5e5): pops while the head is non-NULL; a popped NULL value ends the loop (0x1001b5d6 / 0x1001b5db), a
//     non-NULL value gets its virtual slot 0 called with ecx = value and the argument 1 (`push 1; call [eax]` 0x1001b5df / 0x1001b5e3).
//   Loop 2 (0x1001b5e7-0x1001b651, the compiler's copy for the unwind path): pops while the head is non-NULL WITHOUT calling the
//     value; a popped NULL value ends it (0x1001b61a / 0x1001b61f, 0x1001b64c / 0x1001b651).
// The count is decremented once per popped node, also when the list ends on a NULL value.
static void c3do_drainList(void* self) {
    // loop 1
    for (;;) {
        char* node = fat<char*>(self, 0xc);
        if (!node) break;                                                        // 0x1001b5ae / 0x1001b5b0
        char* next = fat<char*>(node, 4);                                        // 0x1001b5b2
        if (next) {                                                              // 0x1001b5b5 / 0x1001b5b7
            fat<unsigned>(next, 0) = 0;                                          // 0x1001b5b9
            fat<void*>(self, 0xc) = next;                                        // 0x1001b5bb
        } else {
            fat<unsigned>(self, 0x10) = 0;                                       // 0x1001b5c0
            fat<unsigned>(self, 0xc) = 0;                                        // 0x1001b5c3
        }
        char* value = fat<char*>(node, 8);                                       // 0x1001b5c6
        c3do_delete(node);                                                       // 0x1001b5ca
        fat<int>(self, 0x18) = fat<int>(self, 0x18) - 1;                         // 0x1001b5cf-0x1001b5d8
        if (!value) break;                                                       // 0x1001b5d6 / 0x1001b5db
        reinterpret_cast<Th1_t>(c3do_slot(value, 0))(value, 0, 1);               // 0x1001b5dd-0x1001b5e3
    }
    // loop 2
    for (;;) {
        char* node = fat<char*>(self, 0xc);
        if (!node) break;                                                        // 0x1001b5f2 / 0x1001b5f4, 0x1001b624 / 0x1001b626
        char* next = fat<char*>(node, 4);                                        // 0x1001b5f6
        if (next) {                                                              // 0x1001b5f9 / 0x1001b5fb
            fat<unsigned>(next, 0) = 0;                                          // 0x1001b5fd
            fat<void*>(self, 0xc) = next;                                        // 0x1001b5ff
        } else {
            fat<unsigned>(self, 0x10) = 0;                                       // 0x1001b604
            fat<unsigned>(self, 0xc) = 0;                                        // 0x1001b607
        }
        char* value = fat<char*>(node, 8);                                       // 0x1001b60a
        c3do_delete(node);                                                       // 0x1001b60e
        fat<int>(self, 0x18) = fat<int>(self, 0x18) - 1;                         // 0x1001b613-0x1001b61c
        if (!value) break;                                                       // 0x1001b61a / 0x1001b61f
    }
}

// 0x0001b570  ChainObject_dtor and 0x0001c990 Ctrl5b58c_dtorEmbedded (VAs 0x1001b570 / 0x1001c990, instruction-identical apart from
// the unwind-table pointer). ecx is the sub-object at +0x2c of the object (`lea esi, [ecx - 0x2c]` 0x1001b591). A NULL object
// (ecx == 0x2c) leaves the base-vtable store out: eax = (esi != 0 ? ecx : 0) (`neg eax; sbb eax, eax; and eax, ecx` 0x1001b58a-0x1001b58f)
// and `mov [eax], 0x1005b19c` (0x1001b599) writes the Ctrl vtable at the sub-object (a store at address 0 when esi is 0 would fault;
// the fixture never passes ecx == 0x2c). Then the object's own vtable 0x1005b58c at +0 (0x1001b59f) and the two loops above.
#define C3DO_CHAIN_DTOR(NAME)                                                                            \
    Th0v_t NAME##_orig;                                                                                  \
    void __fastcall NAME##_re(void* sub, void*) {                                                        \
        char* self = static_cast<char*>(sub) - 0x2c;                                                     \
        fat<void*>(sub, 0) = c3do_sound(0x0005b19c);                                                     \
        fat<void*>(self, 0) = c3do_sound(0x0005b58c);                                                    \
        c3do_drainList(self);                                                                            \
    }
C3DO_CHAIN_DTOR(ChainObject_dtor)
C3DO_CHAIN_DTOR(Ctrl5b58c_dtorEmbedded)

// 0x0001c080  Ctrl5b58c_dtor (VA 0x1001c080): this = the object itself (`mov esi, ecx` 0x1001c098); vtable 0x1005b58c at +0 (0x1001c09f);
// then the two loops.
Th0v_t Ctrl5b58c_dtor_orig;
void __fastcall Ctrl5b58c_dtor_re(void* self, void*) {
    fat<void*>(self, 0) = c3do_sound(0x0005b58c);                                // 0x1001c09f
    c3do_drainList(self);
}

// 0x0001b870  MidiReader_freeContent (VA 0x1001b870): the same body as Ctrl5b58c_dtor (vtable store at 0x1001b88f, loops 0x1001b89b-0x1001b943).
Th0v_t MidiReader_freeContent_orig;
void __fastcall MidiReader_freeContent_re(void* self, void*) {
    fat<void*>(self, 0) = c3do_sound(0x0005b58c);                                // 0x1001b88f
    c3do_drainList(self);
}

// 0x0001bea0  ChainObject_scalarDtor and 0x0001c950 Ctrl5b58c_scalarDtor (VAs 0x1001bea0 / 0x1001c950). ecx is the sub-object at +0x2c;
// the destructor body runs on it (thunk 0x1000156e -> 0x1001b570, resp. 0x10002595 -> 0x1001c990, call at 0x1001bea3 / 0x1001c953);
// bit 0 of the stack argument (`test byte ptr [esp + 8], 1` 0x1001bea8) then calls operator delete on the object start (ecx - 0x2c,
// 0x1001bead / 0x1001beb3). Returns the object start (`lea eax, [esi - 0x2c]` 0x1001bebb), also when the bit is clear (0x1001bead).
#define C3DO_CHAIN_SCALAR(NAME, BODY_RVA)                                                                \
    Th1_t NAME##_orig;                                                                                   \
    unsigned __fastcall NAME##_re(void* sub, void*, unsigned flags) {                                    \
        c3do_fn<Th0v_t>(BODY_RVA)(sub, 0);                                                               \
        char* self = static_cast<char*>(sub) - 0x2c;                                                     \
        if (flags & 1) c3do_delete(self);                                                                \
        return reinterpret_cast<unsigned>(self);                                                         \
    }
C3DO_CHAIN_SCALAR(ChainObject_scalarDtor, 0x0001b570)
C3DO_CHAIN_SCALAR(Ctrl5b58c_scalarDtor, 0x0001c990)

// 0x0001e7d0  MidiReader_dtor (VA 0x1001e7d0). vtable 0x1005b68c at +0 (0x1001e7ed); eax = [this + 0x3c] (0x1001e7f3); when it is
// non-NULL AND bit 2 (value 4) of the byte at +0x34 is set (`test byte ptr [esi + 0x34], 4` 0x1001e802), operator delete(eax)
// (0x1001e809); then +0x3c = 0 (0x1001e813) and MidiReader_freeContent on this (thunk 0x100018ca -> 0x1001b870, call 0x1001e822).
Th0v_t MidiReader_dtor_orig;
void __fastcall MidiReader_dtor_re(void* self, void*) {
    fat<void*>(self, 0) = c3do_sound(0x0005b68c);                                // 0x1001e7ed
    void* buf = fat<void*>(self, 0x3c);                                          // 0x1001e7f3
    if (buf && (fat<unsigned char>(self, 0x34) & 4)) c3do_delete(buf);          // 0x1001e7fe-0x1001e809
    fat<unsigned>(self, 0x3c) = 0;                                               // 0x1001e813
    c3do_fn<Th0v_t>(0x0001b870)(self, 0);                                        // 0x1001e822 (thunk 0x100018ca)
}

// =====================================================================================================================
// B. Constructors and the flag propagation
// =====================================================================================================================

// 0x0002ddb0  Channel_propagateFlag0 (VA 0x1002ddb0). One stack argument v (`ret 4`). Bit 0 of the byte at +0x58 takes bit 0 of v:
// bl = ((al ^ dl) & 1) ^ al (0x1002ddb8-0x1002ddbf) stored as a byte (0x1002ddc1). The object at +0x40 (0x1002ddc4), when non-NULL,
// gets the same call (thunk 0x10002153 -> 0x1002ddb0, push v, call 0x1002ddcd); the function returns 0 (0x1002ddd2).
Th1_t ChanPropFlag_orig;
unsigned __fastcall ChanPropFlag_re(void* self, void*, unsigned v) {
    unsigned char al = fat<unsigned char>(self, 0x58);                           // 0x1002ddb0
    fat<unsigned char>(self, 0x58) = static_cast<unsigned char>(((al ^ v) & 1) ^ al);   // 0x1002ddb8-0x1002ddc1
    void* sub = fat<void*>(self, 0x40);                                          // 0x1002ddc4
    if (sub) c3do_fn<Th1_t>(0x0002ddb0)(sub, 0, v);                              // 0x1002ddc7-0x1002ddcd
    return 0;                                                                    // 0x1002ddd2
}

// 0x00030080  Sound5c450_ctor (VA 0x10030080). Base constructor buildVoiceRxName 0x1002ed50 (thunk 0x10001230, call 0x1003009f) on
// this; then +0x5c, +0x60, +0x64, +0x68 = 0 (0x100300aa-0x100300b3); the vtable 0x1005c450 at +0 (0x100300b9); +0xb8 = 0 and +0xb4 = 0
// (0x100300bf / 0x100300c5); +0x58 = 0 (0x100300b6 / 0x100300cb); 0x12 zero dwords from +0x6c (rep stosd 0x100300cd-0x100300d7);
// Sound_setSourceType 0x1002ad10 (thunk 0x100018a2) with the argument 6 (0x100300d9-0x100300e2); +0xd4, +0xe4, +0xd8, +0xe0 = 0
// (0x100300eb-0x100300fd). Returns this (0x10030103).
Th0p_t Sound5c450Ctor_orig;
void* __fastcall Sound5c450Ctor_re(void* self, void*) {
    c3do_fn<Th0p_t>(0x0002ed50)(self, 0);                                        // 0x1003009f -> 0x1002ed50
    fat<unsigned>(self, 0x5c) = 0;                                               // 0x100300aa
    fat<unsigned>(self, 0x60) = 0;                                               // 0x100300ad
    fat<unsigned>(self, 0x64) = 0;                                               // 0x100300b0
    fat<unsigned>(self, 0x68) = 0;                                               // 0x100300b3
    fat<void*>(self, 0) = c3do_sound(0x0005c450);                                // 0x100300b9
    fat<unsigned>(self, 0xb8) = 0;                                               // 0x100300bf
    fat<unsigned>(self, 0xb4) = 0;                                               // 0x100300c5
    fat<unsigned>(self, 0x58) = 0;                                               // 0x100300cb
    for (int off = 0x6c; off < 0x6c + 0x12 * 4; off += 4) fat<unsigned>(self, off) = 0;   // 0x100300d7
    c3do_fn<Th1_t>(0x0002ad10)(self, 0, 6);                                      // 0x100300e2 -> 0x1002ad10
    fat<unsigned>(self, 0xd4) = 0;                                               // 0x100300eb
    fat<unsigned>(self, 0xe4) = 0;                                               // 0x100300f1
    fat<unsigned>(self, 0xd8) = 0;                                               // 0x100300f7
    fat<unsigned>(self, 0xe0) = 0;                                               // 0x100300fd
    return self;                                                                 // 0x10030103
}

// 0x00031200  StreamReader_ctor (VA 0x10031200). Base constructor buildVoiceTxName 0x1002f220 (thunk 0x10002036, call 0x10031203) on
// this; then +0x5c, +0x60, +0x64, +0x68 = 0 (0x1003120d-0x10031216); the vtable 0x1005c528 at +0 (0x10031219); +0x70, +0x7c, +0x74,
// +0x90, +0x84, +0x88, +0x80, +0x8c = 0 (0x1003121f-0x1003123f). Returns this (0x10031245).
Th0p_t StreamReaderCtor_orig;
void* __fastcall StreamReaderCtor_re(void* self, void*) {
    c3do_fn<Th0p_t>(0x0002f220)(self, 0);                                        // 0x10031203 -> 0x1002f220
    fat<unsigned>(self, 0x5c) = 0;                                               // 0x1003120d
    fat<unsigned>(self, 0x60) = 0;                                               // 0x10031210
    fat<unsigned>(self, 0x64) = 0;                                               // 0x10031213
    fat<unsigned>(self, 0x68) = 0;                                               // 0x10031216
    fat<void*>(self, 0) = c3do_sound(0x0005c528);                                // 0x10031219
    fat<unsigned>(self, 0x70) = 0;                                               // 0x1003121f
    fat<unsigned>(self, 0x7c) = 0;                                               // 0x10031222
    fat<unsigned>(self, 0x74) = 0;                                               // 0x10031224
    fat<unsigned>(self, 0x90) = 0;                                               // 0x10031227
    fat<unsigned>(self, 0x84) = 0;                                               // 0x1003122d
    fat<unsigned>(self, 0x88) = 0;                                               // 0x10031233
    fat<unsigned>(self, 0x80) = 0;                                               // 0x10031239
    fat<unsigned>(self, 0x8c) = 0;                                               // 0x1003123f
    return self;                                                                 // 0x10031245
}

// 0x00006020  WaveStream::ctor (VA 0x10006020). Base constructor Ch5c178_ctor 0x1002e930 (thunk 0x1000125d, call 0x1000603f) on this;
// +0x5c, +0x60, +0x64, +0x68 = 0 (0x1000604a-0x10006053); the vtable 0x1005b1c0 at +0 (0x10006059); +0xc8 = 0 (0x1000605f); +0x58 = 0
// (0x10006056 / 0x1000606a); 0x12 zero dwords from +0x6c (rep stosd 0x1000606e-0x10006083); +0xb8, +0xbc, +0xb4 = 0 (0x10006071 /
// 0x10006077 / 0x1000607d); +0xc4 and +0xc0 = 0 (0x1000608e / 0x10006094); Sound_setSourceType 0x1002ad10 (thunk 0x100018a2) with the
// argument 4 (0x10006085-0x1000609a); +0xd4 = 0, +0xc0 = 0, +0xcc = 0, +0xd0 = 0 (0x100060a7-0x100060b9); +0xd8, +0xdc, +0xe0, +0xe4 = 0
// (0x100060bf-0x100060cc) and the word at +0xe8 = 0 (0x100060cf). Returns this (0x100060c1).
Th0p_t WaveStreamCtor_orig;
void* __fastcall WaveStreamCtor_re(void* self, void*) {
    c3do_fn<Th0p_t>(0x0002e930)(self, 0);                                        // 0x1000603f -> 0x1002e930
    fat<unsigned>(self, 0x5c) = 0;                                               // 0x1000604a
    fat<unsigned>(self, 0x60) = 0;                                               // 0x1000604d
    fat<unsigned>(self, 0x64) = 0;                                               // 0x10006050
    fat<unsigned>(self, 0x68) = 0;                                               // 0x10006053
    fat<void*>(self, 0) = c3do_sound(0x0005b1c0);                                // 0x10006059
    fat<unsigned>(self, 0xc8) = 0;                                               // 0x1000605f
    fat<unsigned>(self, 0x58) = 0;                                               // 0x1000606a
    fat<unsigned>(self, 0xb8) = 0;                                               // 0x10006071
    fat<unsigned>(self, 0xbc) = 0;                                               // 0x10006077
    fat<unsigned>(self, 0xb4) = 0;                                               // 0x1000607d
    for (int off = 0x6c; off < 0x6c + 0x12 * 4; off += 4) fat<unsigned>(self, off) = 0;   // 0x10006083
    fat<unsigned>(self, 0xc4) = 0;                                               // 0x1000608e
    fat<unsigned>(self, 0xc0) = 0;                                               // 0x10006094
    c3do_fn<Th1_t>(0x0002ad10)(self, 0, 4);                                      // 0x1000609a -> 0x1002ad10
    fat<unsigned>(self, 0xd4) = 0;                                               // 0x100060a7
    fat<unsigned>(self, 0xc0) = 0;                                               // 0x100060ad
    fat<unsigned>(self, 0xcc) = 0;                                               // 0x100060b3
    fat<unsigned>(self, 0xd0) = 0;                                               // 0x100060b9
    fat<unsigned>(self, 0xd8) = 0;                                               // 0x100060bf
    fat<unsigned>(self, 0xdc) = 0;                                               // 0x100060c5
    fat<unsigned>(self, 0xe0) = 0;                                               // 0x100060c9
    fat<unsigned>(self, 0xe4) = 0;                                               // 0x100060cc
    fat<unsigned short>(self, 0xe8) = 0;                                         // 0x100060cf
    return self;                                                                 // 0x100060c1
}

// 0x0000eef0  DsBuffer::ctor (VA 0x1000eef0). The dword at +0 loses bit 0 (`and ecx, 0xfffffffe` 0x1000eefb, stored 0x1000ef00) and the
// byte at +4 gains bit 0 (`or al, 1` 0x1000eefe, stored 0x1000ef04); +0x5c = 0 and +0x44 = 0 (0x1000ef07 / 0x1000ef0a); DsBuffer::initDesc
// 0x1000f590 (thunk 0x10001505, call 0x1000ef0d) on this; +0xc8, +0xcc, +0xd0, +0x64, +0x68, +0x4c, +0x48, +0x50, +0x54, +0x58 = 0
// (0x1000ef12-0x1000ef36). Returns this (0x1000ef39).
Th0p_t DsBufferCtor_orig;
void* __fastcall DsBufferCtor_re(void* self, void*) {
    unsigned v = fat<unsigned>(self, 0) & 0xfffffffeu;                           // 0x1000eef6 / 0x1000eefb
    unsigned char b = static_cast<unsigned char>(fat<unsigned char>(self, 4) | 1);   // 0x1000eef8 / 0x1000eefe
    fat<unsigned>(self, 0) = v;                                                  // 0x1000ef00
    fat<unsigned char>(self, 4) = b;                                             // 0x1000ef04
    fat<unsigned>(self, 0x5c) = 0;                                               // 0x1000ef07
    fat<unsigned>(self, 0x44) = 0;                                               // 0x1000ef0a
    c3do_fn<Th0p_t>(0x0000f590)(self, 0);                                        // 0x1000ef0d -> 0x1000f590
    fat<unsigned>(self, 0xc8) = 0;                                               // 0x1000ef12
    fat<unsigned>(self, 0xcc) = 0;                                               // 0x1000ef18
    fat<unsigned>(self, 0xd0) = 0;                                               // 0x1000ef1e
    fat<unsigned>(self, 0x64) = 0;                                               // 0x1000ef24
    fat<unsigned>(self, 0x68) = 0;                                               // 0x1000ef27
    fat<unsigned>(self, 0x4c) = 0;                                               // 0x1000ef2a
    fat<unsigned>(self, 0x48) = 0;                                               // 0x1000ef2d
    fat<unsigned>(self, 0x50) = 0;                                               // 0x1000ef30
    fat<unsigned>(self, 0x54) = 0;                                               // 0x1000ef33
    fat<unsigned>(self, 0x58) = 0;                                               // 0x1000ef36
    return self;                                                                 // 0x1000ef39
}

// 0x0000f590  DsBuffer::initDesc (VA 0x1000f590). Every value after the first store is an immediate or derived from one, so only the dword
// at +0 (bit 1 cleared: `and ebx, 0xfffffffd` 0x1000f5a1, stored 0x1000f5a6) depends on the object. Nine zero dwords from +0x70 (rep stosd
// 0x1000f5a8), then +0x70 = 0x24 (0x1000f5b0), +0x74 = 0xe8, +0x78 = 0x10000, +0x7c = 0, +0x80 = &this[0xa8] (0x1000f5ba-0x1000f5cd);
// the block at +0xa8 (four zero dwords and a zero word at +0xb8: 0x1000f5d3-0x1000f5de) gets the word 1 at +0xa8 (0x1000f5e2); the
// five dwords at +0x94 are zeroed (0x1000f637-0x1000f644); the words/dwords +0xaa = 2, +0xac = 0x5622, +0xb0 = 0x15888, +0xb4 = 4,
// +0xb6 = 0x10, +0xb8 = 0 (0x1000f5ef-0x1000f61e); +0x40, +0x14, +0x18, +0x1c, +0xc, +0x44 = 0 (0x1000f625-0x1000f634). Then
// edx = (0x5622 * 3 * 5 * 2) * 0x10624dd3 >> 32 >> 6 (0x1000f647-0x1000f66f, i.e. 661), edi = word[+0xb4] * edx (0x1000f672), eax =
// edi * 18 (0x1000f67b / 0x1000f685); +0x34 = 0x295 (0x1000f67e); when eax <= dword[+0x78] (`ja` 0x1000f689 not taken): +0x38 = eax,
// +0x40 = edi, +0x34 = edx, +0x3c = (eax - edi) >> 1 (0x1000f68b-0x1000f698). The remaining zero stores (0x1000f69b-0x1000f6bc, among them
// +0x3c = 0) follow; the return value is 0 (0x1000f6c1).
Th0u_t DsBufferInitDesc_orig;
unsigned __fastcall DsBufferInitDesc_re(void* self, void*) {
    fat<unsigned>(self, 0) = fat<unsigned>(self, 0) & 0xfffffffdu;               // 0x1000f59c-0x1000f5a6
    for (int off = 0x70; off < 0x70 + 9 * 4; off += 4) fat<unsigned>(self, off) = 0;   // 0x1000f5a8
    fat<unsigned>(self, 0x70) = 0x24;                                            // 0x1000f5b0
    fat<unsigned>(self, 0x74) = 0xe8;                                            // 0x1000f5ba
    fat<unsigned>(self, 0x78) = 0x10000;                                         // 0x1000f5c3
    fat<unsigned>(self, 0x7c) = 0;                                               // 0x1000f5ca
    fat<void*>(self, 0x80) = static_cast<char*>(self) + 0xa8;                    // 0x1000f5cd
    for (int off = 0xa8; off < 0xb8; off += 4) fat<unsigned>(self, off) = 0;     // 0x1000f5d3-0x1000f5db
    fat<unsigned short>(self, 0xb8) = 0;                                         // 0x1000f5de
    fat<unsigned short>(self, 0xa8) = 1;                                         // 0x1000f5e2
    fat<unsigned short>(self, 0xaa) = 2;                                         // 0x1000f5ef
    fat<unsigned>(self, 0xac) = 0x5622;                                          // 0x1000f5f8
    fat<unsigned>(self, 0xb0) = 0x15888;                                         // 0x1000f602
    fat<unsigned short>(self, 0xb4) = 4;                                         // 0x1000f60c
    fat<unsigned short>(self, 0xb6) = 0x10;                                      // 0x1000f615
    fat<unsigned short>(self, 0xb8) = 0;                                         // 0x1000f61e
    fat<unsigned>(self, 0x40) = 0;                                               // 0x1000f625
    fat<unsigned>(self, 0x14) = 0;                                               // 0x1000f628
    fat<unsigned>(self, 0x18) = 0;                                               // 0x1000f62b
    fat<unsigned>(self, 0x1c) = 0;                                               // 0x1000f62e
    fat<unsigned>(self, 0xc) = 0;                                                // 0x1000f631
    fat<unsigned>(self, 0x44) = 0;                                               // 0x1000f634
    for (int off = 0x94; off < 0x94 + 5 * 4; off += 4) fat<unsigned>(self, off) = 0;   // 0x1000f637-0x1000f644
    unsigned eax = fat<unsigned>(self, 0xac);                                    // 0x1000f647
    unsigned edi = fat<unsigned short>(self, 0xb4);                              // 0x1000f64d
    unsigned ebx = fat<unsigned>(self, 0x78);                                    // 0x1000f654
    eax = eax + eax * 2;                                                         // 0x1000f657
    fat<unsigned>(self, 0x10) = 0;                                               // 0x1000f65a
    fat<unsigned>(self, 0x24) = 0;                                               // 0x1000f65d
    fat<unsigned>(self, 0x2c) = 0;                                               // 0x1000f660
    unsigned edx = eax + eax * 4;                                                // 0x1000f663
    edx <<= 1;                                                                   // 0x1000f66b
    edx = static_cast<unsigned>((static_cast<unsigned long long>(edx) * 0x10624dd3u) >> 32);   // 0x1000f66d
    edx >>= 6;                                                                   // 0x1000f66f
    edi = edi * edx;                                                             // 0x1000f672
    fat<unsigned>(self, 0x30) = 0;                                               // 0x1000f675
    fat<unsigned>(self, 0x60) = 0;                                               // 0x1000f678
    eax = edi + edi * 8;                                                         // 0x1000f67b
    fat<unsigned>(self, 0x34) = 0x295;                                           // 0x1000f67e
    eax <<= 1;                                                                   // 0x1000f685
    if (!(eax > ebx)) {                                                          // 0x1000f687 / 0x1000f689
        fat<unsigned>(self, 0x38) = eax;                                         // 0x1000f68b
        eax -= edi;                                                              // 0x1000f68e
        eax >>= 1;                                                               // 0x1000f690
        fat<unsigned>(self, 0x40) = edi;                                         // 0x1000f692
        fat<unsigned>(self, 0x34) = edx;                                         // 0x1000f695
        fat<unsigned>(self, 0x3c) = eax;                                         // 0x1000f698
    }
    fat<unsigned>(self, 0x5c) = 0;                                               // 0x1000f69b
    fat<unsigned>(self, 0xc0) = 0;                                               // 0x1000f69e
    fat<unsigned>(self, 0xbc) = 0;                                               // 0x1000f6a4
    fat<unsigned>(self, 0x64) = 0;                                               // 0x1000f6aa
    fat<unsigned>(self, 0x68) = 0;                                               // 0x1000f6ad
    fat<unsigned>(self, 0x6c) = 0;                                               // 0x1000f6b0
    fat<unsigned>(self, 0x20) = 0;                                               // 0x1000f6b3
    fat<unsigned>(self, 0xc4) = 0;                                               // 0x1000f6b6
    fat<unsigned>(self, 0x3c) = 0;                                               // 0x1000f6bc
    return 0;                                                                    // 0x1000f6c1
}

// 0x000231e0  Seq_cmdHandler7 (VA 0x100231e0). No stack argument (plain `ret` 0x1002326c). A zero dword at +0x224 returns 0x24
// (0x100231e4-0x100231f5). Otherwise the cursor +0x220 takes the list head +0x218 (0x100231f6 / 0x100231fe) and walks the list {prev +0,
// next +4, value +8}: a NULL head or a NULL value ends the walk with 0x24 (0x10023204 / 0x1002320b -> 0x1002323e); a value whose dword at
// +4 equals the dword at this+0x738 is the track (0x10023210 / 0x10023216 -> 0x1002323a); any other value advances the cursor (0x1002321a-
// 0x10023238: a NULL cursor, NULL next node or NULL next value end the walk with 0x24). The track's dword at +0x1c, when non-zero
// (0x10023246 / 0x10023249), is passed to the sequencer's own vtable slot +0x84 (`call [edx + 0x84]` 0x10023252); then slot +0x94 gets the track's
// dwords at +8 and +0xc (pushed +0xc then +8, so the first argument is +8: 0x10023258-0x10023264) and its result is the function's.
// `test esi, esi` at 0x1002323a can never see zero (the walk only continues with a non-NULL value), so the `jne` is always taken.
typedef unsigned(__fastcall* Slot1_t)(void*, void*, unsigned);
typedef unsigned(__fastcall* Slot2_t)(void*, void*, unsigned, unsigned);
Th0u_t SeqCmd7_orig;
unsigned __fastcall SeqCmd7_re(void* self, void*) {
    if (!fat<unsigned>(self, 0x224)) return 0x24;                                // 0x100231e4-0x100231ec
    char* node = fat<char*>(self, 0x218);                                        // 0x100231f6
    fat<char*>(self, 0x220) = node;                                              // 0x100231fe
    if (!node) return 0x24;                                                      // 0x10023204
    char* tr = fat<char*>(node, 8);                                              // 0x10023206
    if (!tr) return 0x24;                                                        // 0x1002320b
    for (;;) {
        if (fat<unsigned>(tr, 4) == fat<unsigned>(self, 0x738)) break;           // 0x1002320d-0x10023218
        node = fat<char*>(self, 0x220);                                          // 0x1002321a
        if (!node) return 0x24;                                                  // 0x10023222
        node = fat<char*>(node, 4);                                              // 0x10023224
        fat<char*>(self, 0x220) = node;                                          // 0x10023229
        if (!node) return 0x24;                                                  // 0x1002322f
        tr = fat<char*>(node, 8);                                                // 0x10023231
        if (!tr) return 0x24;                                                    // 0x10023236
    }
    unsigned a = fat<unsigned>(tr, 0x1c);                                        // 0x10023246
    if (a) reinterpret_cast<Slot1_t>(c3do_slot(self, 0x84))(self, 0, a);         // 0x10023249-0x10023252
    unsigned b = fat<unsigned>(tr, 0xc);                                         // 0x10023258
    unsigned c = fat<unsigned>(tr, 8);                                           // 0x1002325b
    return reinterpret_cast<Slot2_t>(c3do_slot(self, 0x94))(self, 0, c, b);      // 0x1002325e-0x10023264
}

// 0x0001e8e0  Seq_rewindTracks (VA 0x1001e8e0). eax = dword +0x3c (0x1001e8ed); +0x34 loses bit 1 (`and edi, 0xfffffffd` 0x1001e8f0, stored
// 0x1001e8ff); the byte at +0x38 loses bit 2 (`and dl, 0xfb` 0x1001e8f3, stored 0x1001e902); +0x2c = eax (0x1001e8f6); +0x40, +0x50, +0x4c = 0
// (0x1001e8f9 / 0x1001e8fc / 0x1001e905); +0x48 = 0x10 (0x1001e908); the byte at +0x30 = 0 (0x1001e90f). FUN_1001bc00 (thunk 0x100022ed,
// call 0x1001e912) then calls the slot +0x18 of every list value. The cursor +0x14 takes the head +0xc (0x1001e917 / 0x1001e91c); a NULL head
// or NULL value ends the function (0x1001e91f / 0x1001e926). A value whose dword at +0x1c is 5 (`cmp dword ptr [edi + 0x1c], 5` 0x1001e928)
// gets its vtable slot +0x10 called with 0 (`push ebx; call [edx + 0x10]` 0x1001e930-0x1001e933); then the byte at +0x38 gets bit 0 set when
// the value's byte at +0x19 (read AFTER the call) is 1 and cleared otherwise (0x1001e936-0x1001e946). The cursor advances through next
// (0x1001e949-0x1001e955) until it, the next node or its value is NULL (0x1001e94e, 0x1001e958, 0x1001e95f).
Th0v_t SeqRewind_orig;
void __fastcall SeqRewind_re(void* self, void*) {
    unsigned edi = fat<unsigned>(self, 0x34);                                    // 0x1001e8e7
    unsigned char dl = fat<unsigned char>(self, 0x38);                           // 0x1001e8ea
    unsigned eax = fat<unsigned>(self, 0x3c);                                    // 0x1001e8ed
    edi &= 0xfffffffdu;                                                          // 0x1001e8f0
    dl &= 0xfb;                                                                  // 0x1001e8f3
    fat<unsigned>(self, 0x2c) = eax;                                             // 0x1001e8f6
    fat<unsigned>(self, 0x40) = 0;                                               // 0x1001e8f9
    fat<unsigned>(self, 0x50) = 0;                                               // 0x1001e8fc
    fat<unsigned>(self, 0x34) = edi;                                             // 0x1001e8ff
    fat<unsigned char>(self, 0x38) = dl;                                         // 0x1001e902
    fat<unsigned>(self, 0x4c) = 0;                                               // 0x1001e905
    fat<unsigned>(self, 0x48) = 0x10;                                            // 0x1001e908
    fat<unsigned char>(self, 0x30) = 0;                                          // 0x1001e90f
    c3do_fn<Th0v_t>(0x0001bc00)(self, 0);                                        // 0x1001e912 (thunk 0x100022ed)
    char* node = fat<char*>(self, 0xc);                                          // 0x1001e917
    fat<char*>(self, 0x14) = node;                                               // 0x1001e91c
    if (!node) return;                                                           // 0x1001e91f
    char* v = fat<char*>(node, 8);                                               // 0x1001e921
    if (!v) return;                                                              // 0x1001e926
    for (;;) {
        if (fat<int>(v, 0x1c) == 5) {                                            // 0x1001e928 / 0x1001e92c
            reinterpret_cast<Slot1_t>(c3do_slot(v, 0x10))(v, 0, 0);              // 0x1001e92e-0x1001e933
            unsigned char b = fat<unsigned char>(v, 0x19);                       // 0x1001e936
            unsigned char al = fat<unsigned char>(self, 0x38);                   // 0x1001e93b
            if (b == 1) al |= 1; else al &= 0xfe;                                // 0x1001e93e-0x1001e944
            fat<unsigned char>(self, 0x38) = al;                                 // 0x1001e946
        }
        node = fat<char*>(self, 0x14);                                           // 0x1001e949
        if (!node) return;                                                       // 0x1001e94e
        node = fat<char*>(node, 4);                                              // 0x1001e950
        fat<char*>(self, 0x14) = node;                                           // 0x1001e955
        if (!node) return;                                                       // 0x1001e958
        v = fat<char*>(node, 8);                                                 // 0x1001e95a
        if (!v) return;                                                          // 0x1001e95f
    }
}

// 0x00006f00  resizeDynArray (VA 0x10006f00). One stack argument n (`ret 4`). n equal to the count at +0xc returns 0 (0x10006f08-0x10006f11).
// edi = bit 0 of the dword at +8 sign-extended to 0 or -1 (`shl edi, 0x1f; sar edi, 0x1f` 0x10006f1a / 0x10006f1d); `cmp edi, 1` can never
// be equal, so the bit-clearing store at 0x10006f25-0x10006f27 is dead. A non-NULL data pointer at +4 is released with operator delete
// (0x10006f2a-0x10006f32); +4 = 0 (0x10006f41); operator new (RVA 0x424d7, call 0x10006f49) of n * 8 bytes becomes +4 (0x10006f4e); +0xc = n
// (0x10006f5a); the n pairs of zero dwords are written (loop 0x10006f5f-0x10006f70, skipped for n == 0 by `jbe` 0x10006f5d); the dword at +8 gets
// bit 0 of edi (0x10006f72-0x10006f81), which equals its old bit 0, so +8 keeps its value; returns 0 (0x10006f7f).
typedef void*(__cdecl* OpNew_t)(unsigned);
Th1_t ResizeDynArray_orig;
unsigned __fastcall ResizeDynArray_re(void* self, void*, unsigned n) {
    if (n == fat<unsigned>(self, 0xc)) return 0;                                 // 0x10006f08 / 0x10006f0b
    unsigned eax = fat<unsigned>(self, 8);                                       // 0x10006f14
    int edi = static_cast<int>(eax << 31) >> 31;                                 // 0x10006f18-0x10006f1d
    if (edi == 1) fat<unsigned>(self, 8) = eax & 0xfffffffeu;                    // 0x10006f23 (dead)
    void* old = fat<void*>(self, 4);                                             // 0x10006f2a
    if (old) c3do_delete(old);                                                   // 0x10006f2d-0x10006f32
    fat<unsigned>(self, 4) = 0;                                                  // 0x10006f41
    char* data = static_cast<char*>(c3do_fn<OpNew_t>(0x000424d7)(n * 8));        // 0x10006f49
    fat<void*>(self, 4) = data;                                                  // 0x10006f4e
    fat<unsigned>(self, 0xc) = n;                                                // 0x10006f5a
    if (n > 0) {                                                                 // 0x10006f58 / 0x10006f5d
        unsigned i = 0;
        do {
            char* d = fat<char*>(self, 4);                                       // 0x10006f5f
            i++;                                                                 // 0x10006f62
            fat<unsigned>(d, i * 8 - 8) = 0;                                     // 0x10006f63
            fat<unsigned>(d, i * 8 - 4) = 0;                                     // 0x10006f67
        } while (i < fat<unsigned>(self, 0xc));                                  // 0x10006f6b-0x10006f70
    }
    unsigned cur = fat<unsigned>(self, 8);                                       // 0x10006f72
    fat<unsigned>(self, 8) = (((cur ^ static_cast<unsigned>(edi)) & 1) ^ cur);   // 0x10006f75-0x10006f81
    return 0;                                                                    // 0x10006f7f
}

// 0x00011810  appendNamedNode (VA 0x10011810): a DirectSoundEnumerate callback (installer 0x100114e0). `ret 0x10`: four stack arguments (guid
// pointer, description string, module string, owner). A 0x14-byte node comes from operator new (0x10011814-0x10011816); the owner's counter
// at +0x148 is incremented (0x10011824-0x10011830); the node becomes the owner's head at +4 when it is NULL (0x10011838), otherwise the walk
// along the next pointer at +0x10 (0x1001183d-0x1001184d) ends at the last node, which gets the new node as next. The node then holds a copy of the
// description (strlen + 1 bytes from operator new, copied with rep movsd/movsb, stored at +0: 0x10011850-0x1001187f), a copy of the module string
// (stored at +4: 0x1001188a-0x100118c2), a 0x10-byte copy of the guid at +8 when the guid pointer is non-NULL (0x100118ca-0x100118eb) or NULL at +8
// (0x100118f3), the owner at +0xc (0x100118f6) and NULL at +0x10 (0x100118f9). The module global 0x100b4a44 is incremented (0x100118fc-0x10011904)
// and the function returns 1 (0x1001190a).
typedef int(__stdcall* AppendNamedNode_t)(const void*, const char*, const char*, void*);
AppendNamedNode_t AppendNamedNode_orig;
static char* c3do_dupStr(const char* s) {
    unsigned len = 0;
    while (s[len]) len++;                                                        // 0x10011857-0x1001185d (repne scasb)
    char* d = static_cast<char*>(c3do_fn<OpNew_t>(0x000424d7)(len + 1));         // 0x10011860
    for (unsigned i = 0; i <= len; i++) d[i] = s[i];                             // 0x1001187f-0x10011888
    return d;
}
int __stdcall AppendNamedNode_re(const void* guid, const char* desc, const char* module, void* owner) {
    char* node = static_cast<char*>(c3do_fn<OpNew_t>(0x000424d7)(0x14));        // 0x10011816
    fat<int>(owner, 0x148) = fat<int>(owner, 0x148) + 1;                         // 0x10011824-0x10011830
    char* head = fat<char*>(owner, 4);                                           // 0x1001182a
    if (!head) {                                                                 // 0x1001182e / 0x10011836
        fat<char*>(owner, 4) = node;                                             // 0x10011838
    } else {
        char* last = head;                                                       // 0x1001183d
        while (fat<char*>(last, 0x10)) last = fat<char*>(last, 0x10);            // 0x10011840-0x1001184b
        fat<char*>(last, 0x10) = node;                                           // 0x1001184d
    }
    fat<char*>(node, 0) = c3do_dupStr(desc);                                     // 0x10011850-0x1001186e
    fat<char*>(node, 4) = c3do_dupStr(module);                                   // 0x1001188a-0x100118a9
    if (guid) {                                                                  // 0x100118ca / 0x100118cc
        char* g = static_cast<char*>(c3do_fn<OpNew_t>(0x000424d7)(0x10));        // 0x100118ce-0x100118d0
        fat<char*>(node, 8) = g;                                                 // 0x100118d5
        for (int i = 0; i < 0x10; i += 4) fat<unsigned>(g, i) = fat<unsigned>(const_cast<void*>(guid), i);   // 0x100118d8-0x100118ee
    } else {
        fat<unsigned>(node, 8) = 0;                                              // 0x100118f3
    }
    fat<void*>(node, 0xc) = owner;                                               // 0x100118f6
    fat<unsigned>(node, 0x10) = 0;                                               // 0x100118f9
    char* ctr = c3do_sound(0x000b4a44);
    fat<int>(ctr, 0) = fat<int>(ctr, 0) + 1;                                     // 0x100118fc-0x10011904
    return 1;                                                                    // 0x1001190a
}

// 0x0001b510 / 0x0001b6b0 / 0x0001b710 / 0x0001b770 / 0x0001b7d0  dynInit_ctrl100b4b50 / ..4c38 / ..4d20 / ..4a68 / ..4e08 (VAs the same plus
// 0x10000000): static-initializer stubs. Each pushes 13 constants (the first is the last argument pushed: 0x1001b51c etc.), loads the object
// address (0x100b4b50 / 0x100b4c38 / 0x100b4d20 / 0x100b4a68 / 0x100b4e08, `mov ecx, imm` 0x1001b52a / 0x1001b6ca / 0x1001b72a / 0x1001b78a /
// 0x1001b7ea) and calls Ctrl5b19c_ctor 0x1001bee0 (thunk 0x1000123f, `ret 0x34`: call 0x1001b52f etc.), then registers the destructor stub (pushed
// 0x1001b534 / 0x1001b6d4 / 0x1001b734 / 0x1001b794 / 0x1001b7f4: 0x1001b550 / 0x1001b6f0 / 0x1001b750 / 0x1001b7b0 / 0x1001b810) with _atexit
// 0x10042647 (call 0x1001b539 etc., cdecl, `pop ecx`) whose result (0 or -1) is the stub's. The five objects are 0xe8 bytes apart in .data and
// no instruction of the DLL addresses them besides these stubs and the destructor stubs (the log/c3/alt/c3dh_globrefs.py scan).
typedef void*(__fastcall* Ctrl13_t)(void*, void*, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned,
                                    unsigned, unsigned, unsigned);
typedef int(__cdecl* Atexit_t)(void*);
#define C3DO_DYNINIT(NAME, GRVA, DRVA, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13)                         \
    Th0u_t NAME##_orig;                                                                                                  \
    unsigned __fastcall NAME##_re(void*, void*) {                                                                        \
        c3do_fn<Ctrl13_t>(0x0001bee0)(c3do_sound(GRVA), 0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13);     \
        return static_cast<unsigned>(c3do_fn<Atexit_t>(0x00042647)(c3do_sound(DRVA)));                                 \
    }
C3DO_DYNINIT(dynInit_ctrl100b4b50, 0x000b4b50, 0x0001b550, 2, 2, 4, 4, 5, 7, 7, 9, 9, 0xb, 0xb, 0xc, 0)
C3DO_DYNINIT(dynInit_ctrl100b4c38, 0x000b4c38, 0x0001b6f0, 4, 4, 5, 5, 7, 9, 9, 0xb, 0xb, 0xc, 0xc, 0xe, 1)
C3DO_DYNINIT(dynInit_ctrl100b4d20, 0x000b4d20, 0x0001b750, 5, 5, 7, 7, 9, 0xb, 0xb, 0xc, 0xc, 0xe, 0xe, 0x10, 2)
C3DO_DYNINIT(dynInit_ctrl100b4a68, 0x000b4a68, 0x0001b7b0, 7, 7, 9, 9, 0xb, 0xc, 0xc, 0xe, 0xe, 0x10, 0x10, 0x11, 3)
C3DO_DYNINIT(dynInit_ctrl100b4e08, 0x000b4e08, 0x0001b810, 9, 9, 0xb, 0xb, 0xc, 0xe, 0xe, 0x10, 0x10, 0x11, 0x11, 0x13, 4)





// >>> PART mix_part1

// =====================================================================================================================
// M. Software mixer: 16-bit PCM into the 32-bit accumulation buffer (mixRenderA, mixCopyF, mixCopyInnerC and the rated variants)
// =====================================================================================================================
// Shared frame of the mixer functions (every one is `__cdecl(int** ppOut, short** ppIn, unsigned count, char* voice, int mode)` and returns the
// number of frames it consumed). The two cells *ppOut / *ppIn hold the current output (dword accumulators) and input (16-bit samples)
// positions and are advanced in place. `mode == 1` selects the stream layout: the sample record is the object at voice+8 (start +0x18,
// length +0x1c, end +0x20, loop end +0x24) and the counters are voice+0x16c (frames) / voice+0x1360 (wraps); any other mode reads the sample
// record from the voice itself (start +0x18, end +0x20, loop start +0xd8, loop end +0xd4, chain voice +0xb4, flags +0x98 / +0x9c) and counts
// in voice+0x88 / voice+0xcc.
struct MixS {
    int loopFlag;         // [ebp-0xc]
    int chained;          // [ebp-0x14]
    int wraps;            // [ebp-0x10]
    int eof;              // [ebp-0x1c] (only the C-group copy functions set it)
    char* chain;          // [ebp-0x18]
    char* limit;          // [ebp-4]
    char* loopStart;      // [ebp-8]
};

// Saturating `add [edi], eax` / `jo` / `jb` pair: a signed overflow stores 0x7fffffff (no carry) or 0x80000000 (carry).
static inline void mixSatAdd(int* p, int v) {
    long long s = static_cast<long long>(*p) + v;
    if (s > 0x7fffffffLL) s = 0x7fffffffLL;
    else if (s < -0x80000000LL) s = -0x80000000LL;
    *p = static_cast<int>(s);
}

// Prologue of mixRenderA (VA 0x10013896-0x10013927; mixCopyF has the same code 0x4620 higher: 0x10017eb6-0x10017f47). Locals zeroed
// (0x100138a8-0x100138b1); `mode == 1` (0x100138a2 / 0x100138b4): the sample record is voice+8 (0x100138b6); the end is its +0x20, replaced by its +0x24 when
// its +0x1c is above it (unsigned, 0x100138bf / 0x100138c1 / 0x100138c3), the loop start its +0x18 (0x100138c6). Otherwise the voice is the sample
// record: loopFlag = (byte +0x98 & 1) and not (byte +0x9c & 2) (0x100138d1-0x100138e2), chained = byte +0x9c & 1 (0x100138e8-0x100138f3); a chained voice
// takes its chain voice +0xb4 (0x100138f8-0x100138fe), its start +0x18 as the loop start and the voice's own end +0x20 as the limit
// (0x10013901 / 0x1001391e); otherwise a looping voice uses loop start +0xd8 and loop end +0xd4 (0x1001390a-0x10013919) and the others
// start +0x18 / end +0x20 (0x1001391b / 0x1001391e).
static void mixPrologueA(char* v, int mode, MixS& s) {
    s.loopFlag = 0; s.chained = 0; s.wraps = 0; s.eof = 0; s.chain = 0;
    if (mode == 1) {                                                             // 0x100138a2 / 0x100138b4
        char* bo = fat<char*>(v, 8);                                             // 0x100138b6
        unsigned ecx = fat<unsigned>(bo, 0x20);                                  // 0x100138b9
        unsigned edx = fat<unsigned>(bo, 0x1c);                                  // 0x100138bc
        if (edx > ecx) ecx = fat<unsigned>(bo, 0x24);                            // 0x100138bf / 0x100138c1 / 0x100138c3
        s.loopStart = fat<char*>(bo, 0x18);                                      // 0x100138c6 / 0x100138cc
        s.limit = reinterpret_cast<char*>(ecx);                                  // 0x100138c9
        return;
    }
    int c1 = (fat<unsigned char>(v, 0x98) & 1) && !(fat<unsigned char>(v, 0x9c) & 2);   // 0x100138d1 / 0x100138d7 / 0x100138d9 / 0x100138e0
    s.loopFlag = c1;                                                             // 0x100138ee
    s.chained = fat<unsigned char>(v, 0x9c) & 1;                                 // 0x100138e8 / 0x100138f1 / 0x100138f3
    char* ecx; char* edx;
    if (s.chained) {                                                             // 0x100138f6
        s.chain = fat<char*>(v, 0xb4);                                           // 0x100138f8 / 0x100138fe
        ecx = fat<char*>(s.chain, 0x18);                                         // 0x10013901
        edx = fat<char*>(v, 0x20);                                               // 0x1001391e
    } else if (c1) {                                                             // 0x10013906 / 0x10013908
        ecx = fat<char*>(v, 0xd8);                                               // 0x1001390a
        edx = fat<char*>(v, 0xd4);                                               // 0x10013910
    } else {
        ecx = fat<char*>(v, 0x18);                                               // 0x1001391b
        edx = fat<char*>(v, 0x20);                                               // 0x1001391e
    }
    s.loopStart = ecx;                                                           // 0x10013921 / 0x10013916
    s.limit = edx;                                                               // 0x10013924
}

// Epilogue (0x10013a13-0x10013a83): the frames consumed are count - remaining (0x10013a25 / 0x10013a2e); `mode == 1` (0x10013a30) adds them to
// voice+0x16c and the wrap count to voice+0x1360 (0x10013a38-0x10013a51), any other mode to voice+0x88 / voice+0xcc (0x10013a5e-0x10013a77);
// the frame count is the result.
static unsigned mixEpilogue(char* v, int mode, unsigned count, unsigned remaining, int wraps) {
    unsigned processed = count - remaining;
    if (mode == 1) {
        fat<unsigned>(v, 0x16c) = fat<unsigned>(v, 0x16c) + processed;
        fat<unsigned>(v, 0x1360) = fat<unsigned>(v, 0x1360) + static_cast<unsigned>(wraps);
    } else {
        fat<unsigned>(v, 0x88) = fat<unsigned>(v, 0x88) + processed;
        fat<unsigned>(v, 0xcc) = fat<unsigned>(v, 0xcc) + static_cast<unsigned>(wraps);
    }
    return processed;
}

typedef unsigned(__cdecl* Mix5_t)(int**, short**, unsigned, char*, int);

// 0x00013890  mixRenderA (VA 0x10013890): stereo 16-bit frames added into the accumulators. Each frame adds (short)in[0] << 16 and (short)in[1] << 16
// to two consecutive dwords with saturation (0x10013939-0x10013960 / 0x1001395d-0x10013982), esi moves on by 4 bytes (0x10013982) and
// the frame ends the buffer when esi >= limit (signed, 0x10013985 / 0x10013988). The loop is a do-while: the count is decremented after the
// frame (0x1001398a); a count of 0 therefore wraps. Past the limit (0x10013992: count -= 1): a voice that is not chained and does not loop
// is done (0x10013993-0x1001399d); a looping one restarts at the loop start (wraps += 1, *ppIn = loop start: 0x1001399f-0x100139af) and continues while the count
// is non-zero (0x100139b1); a chained one also restarts at the loop start (0x100139b6-0x100139c6), then takes the chain voice's end +0xd4
// as the limit (0x100139cb-0x100139d1), moves to the next chain voice +0xb4 (0x100139d4-0x100139da) and, when there is one, its loop start +0xd8 (0x100139e2-0x100139e8),
// when there is none clears chained and loopFlag (0x100139f6 / 0x100139fd). Result: frames consumed.
Mix5_t MixRenderA_orig;
unsigned __cdecl MixRenderA_re(int** ppOut, short** ppIn, unsigned count, char* v, int mode) {
    MixS s;
    mixPrologueA(v, mode, s);                                                    // 0x10013896-0x10013927
    unsigned ecx = count;                                                        // 0x1001392c
    short* esi = *ppIn;                                                          // 0x10013932
    int* edi = *ppOut;                                                           // 0x10013937
    for (;;) {
        mixSatAdd(edi, static_cast<int>(esi[0]) << 16); edi++;                  // 0x10013939-0x1001395a
        mixSatAdd(edi, static_cast<int>(esi[1]) << 16); edi++;                  // 0x1001395d-0x1001397f
        esi += 2;                                                                // 0x10013982
        if (static_cast<int>(reinterpret_cast<intptr_t>(esi)) < static_cast<int>(reinterpret_cast<intptr_t>(s.limit))) {   // 0x10013985 / 0x10013988
            ecx--;                                                               // 0x1001398a
            if (ecx != 0) continue;                                              // 0x1001398b
            break;                                                               // 0x1001398d
        }
        ecx--;                                                                   // 0x10013992
        if (s.chained == 0) {                                                    // 0x10013993 / 0x10013997
            if (s.loopFlag == 0) break;                                          // 0x10013999 / 0x1001399d
            s.wraps++;                                                           // 0x1001399f
            esi = reinterpret_cast<short*>(s.loopStart);                         // 0x100139ac
            *ppIn = esi;                                                         // 0x100139af
            if (ecx != 0) continue;                                              // 0x100139b1 / 0x100139b4
            // no `jmp` follows 0x100139b4: with the count spent the code falls into the chained block (0x100139b6), which reads
            // [chain + 0xd4] of the NULL chain of a non-chained voice and faults
        }
        s.wraps++;                                                               // 0x100139b6
        esi = reinterpret_cast<short*>(s.loopStart);                             // 0x100139c3
        *ppIn = esi;                                                             // 0x100139c6
        char* cur = s.chain;                                                     // 0x100139c8
        s.limit = fat<char*>(cur, 0xd4);                                         // 0x100139cb / 0x100139d1
        s.chain = fat<char*>(cur, 0xb4);                                         // 0x100139d4 / 0x100139da
        if (s.chain != 0) {                                                      // 0x100139dd / 0x100139e0
            s.loopStart = fat<char*>(s.chain, 0xd8);                             // 0x100139e2 / 0x100139e8
            if (ecx != 0) continue;                                              // 0x100139eb / 0x100139ee
            break;                                                               // 0x100139f4
        }
        s.chained = 0;                                                           // 0x100139f6
        s.loopFlag = 0;                                                          // 0x100139fd
        if (ecx != 0) continue;                                                  // 0x10013a0a / 0x10013a0d
        break;                                                                   // 0x10013a0d fallthrough
    }
    *ppIn = esi;                                                                 // 0x10013a19
    *ppOut = edi;                                                                // 0x10013a1e
    return mixEpilogue(v, mode, count, ecx, s.wraps);                            // 0x10013a25-0x10013a83
}

// 0x00017eb0  mixCopyF (VA 0x10017eb0): 16-bit mono samples stored (not added) twice into two consecutive dwords (a left/right pair), every
// input sample serving two output frames. The prologue and epilogue are mixRenderA's (0x10017eb6-0x10017f47, 0x10018014-0x10018084). edx is the phase
// inside a sample: 0 before the loop (0x10017f59); a frame stores (short)in[0] << 16 into two dwords (0x10017f5e-0x10017f6b), edx += 1
// (0x10017f6e) and, below 2 (signed, 0x10017f6f / 0x10017f72), the next frame reuses the sample after count -= 1 (0x10017f74); at 2 the phase restarts
// (0x10017f7c), esi moves on by one sample (0x10017f81) and the limit test follows (0x10017f84 / 0x10017f87). The wrap handler (0x10017f91-0x10018011) is
// mixRenderA's with edx doubling as scratch: a chained wrap leaves edx = the next chain voice (0x10017fd5) or its dword +0xd8 (0x10018005), which is the
// phase the loop re-enters with (0x10017f5e).
Mix5_t MixCopyF_orig;
unsigned __cdecl MixCopyF_re(int** ppOut, short** ppIn, unsigned count, char* v, int mode) {
    MixS s;
    mixPrologueA(v, mode, s);                                                    // 0x10017eb6-0x10017f47
    unsigned ecx = count;                                                        // 0x10017f4c
    short* esi = *ppIn;                                                          // 0x10017f52
    int* edi = *ppOut;                                                           // 0x10017f57
    int edx = 0;                                                                 // 0x10017f59
    for (;;) {
        int eax = static_cast<int>(esi[0]) << 16;                                // 0x10017f5e / 0x10017f61
        edi[0] = eax;                                                            // 0x10017f64
        edi[1] = eax;                                                            // 0x10017f69
        edi += 2;                                                                // 0x10017f66 / 0x10017f6b
        edx++;                                                                   // 0x10017f6e
        if (edx < 2) {                                                           // 0x10017f6f / 0x10017f72
            ecx--;                                                               // 0x10017f74
            if (ecx != 0) continue;                                              // 0x10017f75
            break;                                                               // 0x10017f77
        }
        edx = 0;                                                                 // 0x10017f7c
        esi += 1;                                                                // 0x10017f81
        if (static_cast<int>(reinterpret_cast<intptr_t>(esi)) < static_cast<int>(reinterpret_cast<intptr_t>(s.limit))) {   // 0x10017f84 / 0x10017f87
            ecx--;                                                               // 0x10017f89
            if (ecx != 0) continue;                                              // 0x10017f8a
            break;                                                               // 0x10017f8c
        }
        ecx--;                                                                   // 0x10017f91
        if (s.chained == 0) {                                                    // 0x10017f92 / 0x10017f96
            if (s.loopFlag == 0) break;                                          // 0x10017f98 / 0x10017f9c
            s.wraps++;                                                           // 0x10017f9e
            esi = reinterpret_cast<short*>(s.loopStart);                         // 0x10017fab
            *ppIn = esi;                                                         // 0x10017fae
            if (ecx != 0) continue;                                              // 0x10017fb0 / 0x10017fb3
            break;                                                               // 0x10017fb5
        }
        s.wraps++;                                                               // 0x10017fb7
        esi = reinterpret_cast<short*>(s.loopStart);                             // 0x10017fc4
        *ppIn = esi;                                                             // 0x10017fc7
        char* cur = s.chain;                                                     // 0x10017fc9
        s.limit = fat<char*>(cur, 0xd4);                                         // 0x10017fcc / 0x10017fd2
        s.chain = fat<char*>(cur, 0xb4);                                         // 0x10017fd5 / 0x10017fdb
        edx = static_cast<int>(reinterpret_cast<intptr_t>(s.chain));             // 0x10017fd5 (edx holds the next chain voice)
        if (s.chain != 0) {                                                      // 0x10017fde / 0x10017fe1
            s.loopStart = fat<char*>(s.chain, 0xd8);                             // 0x10017fe3 / 0x10017fe9
            if (ecx != 0) continue;                                              // 0x10017fec / 0x10017fef
            break;                                                               // 0x10017ff5
        }
        s.chained = 0;                                                           // 0x10017ff7
        s.loopFlag = 0;                                                          // 0x10017ffe
        edx = fat<int>(cur, 0xd8);                                               // 0x10018005
        if (ecx != 0) continue;                                                  // 0x1001800b / 0x1001800e
        break;                                                                   // 0x1001800e fallthrough
    }
    *ppIn = esi;                                                                 // 0x1001801a
    *ppOut = edi;                                                                // 0x1001801f
    return mixEpilogue(v, mode, count, ecx, s.wraps);                            // 0x10018026-0x10018084
}

// 0x00016340  mixCopyInnerC (VA 0x10016340): mono 16-bit samples stored twice (a left/right pair) per frame, one frame per sample. The mode-1 prologue
// (0x10016363-0x1001639b) also computes eof = (limit == the sample record's +0x24) (`cmp ecx, [eax + 0x24]` 0x10016381 / `sete bl` 0x10016387) into loopFlag
// and a second flag, set (0x10016398) when the voice's byte +0x58 has bit 5 (0x20: `test cl, 0x20` 0x1001638c) and eof is set (0x10016394). Other modes: mixRenderA's
// prologue (0x1001639d-0x100163f3). A frame stores (short)in[0] << 16 twice (0x10016408-0x10016415), esi += 2 (0x10016418), limit test (0x1001641b / 0x1001641e),
// count -= 1 (0x10016420 / 0x10016425). The wrap handler is mixRenderA's except that after a looping restart (0x10016432-0x1001643b) loopFlag is cleared when the
// second flag is set (0x1001643d / 0x10016441 / 0x10016443). Result: frames consumed.
Mix5_t MixCopyInnerC_orig;
// Prologue of mixCopyInnerC (VAs 0x10016351-0x100163f3; mixCopyInnerB has the same code 0x20f0 lower: 0x10014261-0x10014303, mode-1 flag code 0x10014273-0x100142ab).
static void mixPrologueC(char* v, int mode, MixS& s) {
    s.loopFlag = 0; s.chained = 0; s.wraps = 0; s.eof = 0; s.chain = 0;         // 0x10016351-0x1001635e
    if (mode == 1) {                                                             // 0x10016353 / 0x10016361
        char* bo = fat<char*>(v, 8);                                             // 0x10016366
        unsigned ecx = fat<unsigned>(bo, 0x20);                                  // 0x10016369
        unsigned ebx = fat<unsigned>(bo, 0x1c);                                  // 0x1001636c
        if (ebx > ecx) ecx = fat<unsigned>(bo, 0x24);                            // 0x1001636f / 0x10016371 / 0x10016373
        s.limit = reinterpret_cast<char*>(ecx);                                  // 0x10016379
        s.loopStart = fat<char*>(bo, 0x18);                                      // 0x10016376 / 0x1001637c
        s.loopFlag = (ecx == fat<unsigned>(bo, 0x24)) ? 1 : 0;                   // 0x10016381 / 0x10016387 / 0x1001638f
        unsigned char f58 = fat<unsigned char>(v, 0x58);                         // 0x10016384
        if ((f58 & 0x20) && s.loopFlag != 0) s.eof = 1;                          // 0x1001638c / 0x10016392 / 0x10016394 / 0x10016396 / 0x10016398
    } else {
        mixPrologueA(v, mode, s);                                                // 0x1001639d-0x100163f3
    }
}
unsigned __cdecl MixCopyInnerC_re(int** ppOut, short** ppIn, unsigned count, char* v, int mode) {
    MixS s;
    mixPrologueC(v, mode, s);                                                    // 0x10016351-0x100163f3
    unsigned ecx = count;                                                        // 0x100163fb
    short* esi = *ppIn;                                                          // 0x10016401
    int* edi = *ppOut;                                                           // 0x10016406
    for (;;) {
        int eax = static_cast<int>(esi[0]) << 16;                                // 0x10016408 / 0x1001640b
        edi[0] = eax;                                                            // 0x1001640e
        edi[1] = eax;                                                            // 0x10016413
        edi += 2;                                                                // 0x10016410 / 0x10016415
        esi += 1;                                                                // 0x10016418
        if (static_cast<int>(reinterpret_cast<intptr_t>(esi)) < static_cast<int>(reinterpret_cast<intptr_t>(s.limit))) {   // 0x1001641b / 0x1001641e
            ecx--;                                                               // 0x10016420
            if (ecx != 0) continue;                                              // 0x10016421
            break;                                                               // 0x10016423
        }
        ecx--;                                                                   // 0x10016425
        if (s.chained == 0) {                                                    // 0x10016426 / 0x1001642a
            if (s.loopFlag == 0) break;                                          // 0x1001642c / 0x10016430
            s.wraps++;                                                           // 0x10016432
            esi = reinterpret_cast<short*>(s.loopStart);                         // 0x10016438
            *ppIn = esi;                                                         // 0x1001643b
            if (s.eof != 0) s.loopFlag = 0;                                      // 0x1001643d / 0x10016441 / 0x10016443
            if (ecx != 0) continue;                                              // 0x1001644a / 0x1001644d
            break;                                                               // 0x1001644f
        }
        s.wraps++;                                                               // 0x10016451
        esi = reinterpret_cast<short*>(s.loopStart);                             // 0x10016457
        *ppIn = esi;                                                             // 0x1001645a
        char* cur = s.chain;                                                     // 0x1001645c
        s.limit = fat<char*>(cur, 0xd4);                                         // 0x1001645f / 0x10016465
        s.chain = fat<char*>(cur, 0xb4);                                         // 0x10016468 / 0x1001646e
        if (s.chain != 0) {                                                      // 0x10016471 / 0x10016474
            s.loopStart = fat<char*>(s.chain, 0xd8);                             // 0x10016476 / 0x1001647c
            if (ecx != 0) continue;                                              // 0x1001647f / 0x10016482
            break;                                                               // 0x10016484
        }
        s.chained = 0;                                                           // 0x10016486
        s.loopFlag = 0;                                                          // 0x1001648d
        if (ecx != 0) continue;                                                  // 0x1001649a / 0x1001649d
        break;                                                                   // 0x1001649d fallthrough
    }
    *ppIn = esi;                                                                 // 0x100164ac
    *ppOut = edi;                                                                // 0x100164b1
    return mixEpilogue(v, mode, count, ecx, s.wraps);                            // 0x100164b8-0x10016516
}
// <<< PART mix_part1

// >>> PART mix_part2

// ---------------------------------------------------------------------------------------------------------------------
// M2. Mono copy with accumulation, and the gain / fade variants (mixResampleInnerA / B / C)
// ---------------------------------------------------------------------------------------------------------------------

// `add [edi], eax` / `jo` / `jb` with eax kept: the saturation constants are loaded INTO eax (0x10014329 / 0x10014332), so a following add that
// reuses eax sees the constant, not the value that overflowed.
static inline void mixSatAddE(int* p, int& eax) {
    long long s = static_cast<long long>(*p) + eax;
    if (s > 0x7fffffffLL) { eax = 0x7fffffff; *p = eax; }
    else if (s < -0x80000000LL) { eax = static_cast<int>(0x80000000u); *p = eax; }
    else *p = static_cast<int>(s);
}
// `imul edx` (one operand): the low dword of the signed 64-bit product
static inline int mixMul(int a, int b) { return static_cast<int>(static_cast<long long>(a) * static_cast<long long>(b)); }
static inline bool mixBelow(char* p, char* limit) {
    return static_cast<int>(reinterpret_cast<intptr_t>(p)) < static_cast<int>(reinterpret_cast<intptr_t>(limit));   // signed `jge` not taken
}

// 0x00014250  mixCopyInnerB (VA 0x10014250): mixCopyInnerC with additions. The prologue (0x10014261-0x10014303) and epilogue (0x10014404-0x10014474)
// are mixCopyInnerC's. A frame adds (short)in[0] << 16 into two consecutive dwords (0x10014318-0x1001435a): the first add (0x1001431e) saturates by
// loading 0x7fffffff / 0x80000000 into eax (0x10014329 / 0x10014332), and the second add (0x1001433c) uses that eax. esi += 2 (0x1001435c), limit test
// (0x1001435f / 0x10014362). The wrap handler (0x1001436c-0x10014401) is mixCopyInnerC's.
Mix5_t MixCopyInnerB_orig;
unsigned __cdecl MixCopyInnerB_re(int** ppOut, short** ppIn, unsigned count, char* v, int mode) {
    MixS s;
    mixPrologueC(v, mode, s);                                                    // 0x10014261-0x10014303
    unsigned ecx = count;                                                        // 0x1001430b
    short* esi = *ppIn;                                                          // 0x10014311
    int* edi = *ppOut;                                                           // 0x10014316
    for (;;) {
        int eax = static_cast<int>(esi[0]) << 16;                                // 0x10014318 / 0x1001431b
        mixSatAddE(edi, eax); edi++;                                             // 0x1001431e-0x10014339
        mixSatAddE(edi, eax); edi++;                                             // 0x1001433c-0x10014357
        esi += 1;                                                                // 0x1001435c
        if (mixBelow(reinterpret_cast<char*>(esi), s.limit)) {                   // 0x1001435f / 0x10014362
            ecx--;                                                               // 0x10014364
            if (ecx != 0) continue;                                              // 0x10014365
            break;                                                               // 0x10014367
        }
        ecx--;                                                                   // 0x1001436c
        if (s.chained == 0) {                                                    // 0x1001436d / 0x10014371
            if (s.loopFlag == 0) break;                                          // 0x10014373 / 0x10014377
            s.wraps++;                                                           // 0x1001437d
            esi = reinterpret_cast<short*>(s.loopStart);                         // 0x1001438a
            *ppIn = esi;                                                         // 0x1001438d
            if (s.eof != 0) s.loopFlag = 0;                                      // 0x1001438f / 0x10014393 / 0x10014395
            if (ecx != 0) continue;                                              // 0x1001439c / 0x1001439f
            break;                                                               // 0x100143a5
        }
        s.wraps++;                                                               // 0x100143a7
        esi = reinterpret_cast<short*>(s.loopStart);                             // 0x100143b4
        *ppIn = esi;                                                             // 0x100143b7
        char* cur = s.chain;                                                     // 0x100143b9
        s.limit = fat<char*>(cur, 0xd4);                                         // 0x100143bc / 0x100143c2
        s.chain = fat<char*>(cur, 0xb4);                                         // 0x100143c5 / 0x100143cb
        if (s.chain != 0) {                                                      // 0x100143ce / 0x100143d1
            s.loopStart = fat<char*>(s.chain, 0xd8);                             // 0x100143d3 / 0x100143d9
            if (ecx != 0) continue;                                              // 0x100143dc / 0x100143df
            break;                                                               // 0x100143e5
        }
        s.chained = 0;                                                           // 0x100143e7
        s.loopFlag = 0;                                                          // 0x100143ee
        if (ecx != 0) continue;                                                  // 0x100143fb / 0x100143fe
        break;                                                                   // 0x100143fe fallthrough
    }
    *ppIn = esi;                                                                 // 0x1001440a
    *ppOut = edi;                                                                // 0x1001440f
    return mixEpilogue(v, mode, count, ecx, s.wraps);                            // 0x10014416-0x10014474
}

// 0x00013b10  mixResampleInnerA (VA 0x10013b10): stereo 16-bit frames with a gain per channel, added with saturation. The gains are the dwords +0x1e8
// (left) and +0x1f8 (right) of the voice in mode 1 (0x10013b36-0x10013b48) and +0x78 / +0x68 otherwise (0x10013b63-0x10013b6c); the sample record,
// the flags and the chain are mixRenderA's (0x10013b4b-0x10013b61, 0x10013b6f-0x10013bcd). A frame adds the low dword of (short)in[0] * gainL and of
// (short)in[1] * gainR to two consecutive dwords (0x10013be2-0x10013c2c, `imul edx` 0x10013be8 / 0x10013c0f), esi += 4 (0x10013c2f), limit test
// (0x10013c32 / 0x10013c35). The wrap handler is mixRenderA's except that a NON-chained looping voice stores 0 at [ebx + 4] (0x10013c4f), where ebx is the
// chained flag (0 on that path): the original faults there, so no vector takes that arm.
Mix5_t MixResampleInnerA_orig;
unsigned __cdecl MixResampleInnerA_re(int** ppOut, short** ppIn, unsigned count, char* v, int mode) {
    MixS s;
    int gainL, gainR;
    if (mode == 1) {                                                             // 0x10013b22 / 0x10013b34
        gainL = fat<int>(v, 0x1e8);                                              // 0x10013b36 / 0x10013b45
        gainR = fat<int>(v, 0x1f8);                                              // 0x10013b3c / 0x10013b48
    } else {
        gainL = fat<int>(v, 0x78);                                               // 0x10013b63 / 0x10013b66
        gainR = fat<int>(v, 0x68);                                               // 0x10013b69 / 0x10013b6c
    }
    mixPrologueA(v, mode, s);                                                    // 0x10013b28-0x10013bcd
    unsigned ecx = count;                                                        // 0x10013bd5
    short* esi = *ppIn;                                                          // 0x10013bdb
    int* edi = *ppOut;                                                           // 0x10013be0
    for (;;) {
        int eax = mixMul(static_cast<int>(esi[0]), gainL);                       // 0x10013be2 / 0x10013be5 / 0x10013be8
        mixSatAddE(edi, eax); edi++;                                             // 0x10013bea-0x10013c05
        eax = mixMul(static_cast<int>(esi[1]), gainR);                           // 0x10013c08 / 0x10013c0c / 0x10013c0f
        mixSatAddE(edi, eax); edi++;                                             // 0x10013c11-0x10013c2c
        esi += 2;                                                                // 0x10013c2f
        if (mixBelow(reinterpret_cast<char*>(esi), s.limit)) {                   // 0x10013c32 / 0x10013c35
            ecx--;                                                               // 0x10013c37
            if (ecx != 0) continue;                                              // 0x10013c38
            break;                                                               // 0x10013c3a
        }
        ecx--;                                                                   // 0x10013c3f
        if (s.chained == 0) {                                                    // 0x10013c40 / 0x10013c44
            if (s.loopFlag == 0) break;                                          // 0x10013c46 / 0x10013c4a
            s.wraps++;                                                           // 0x10013c4c
            *reinterpret_cast<volatile unsigned*>(static_cast<uintptr_t>(s.chained) + 4) = 0;   // 0x10013c4f (faults in the original: ebx = 0)
            esi = reinterpret_cast<short*>(s.loopStart);                         // 0x10013c59
            *ppIn = esi;                                                         // 0x10013c5c
            if (ecx != 0) continue;                                              // 0x10013c5e / 0x10013c61
            // falls into the chained block (0x10013c67), as in mixRenderA
        }
        s.wraps++;                                                               // 0x10013c67
        esi = reinterpret_cast<short*>(s.loopStart);                             // 0x10013c74
        *ppIn = esi;                                                             // 0x10013c77
        char* cur = s.chain;                                                     // 0x10013c79
        s.limit = fat<char*>(cur, 0xd4);                                         // 0x10013c7c / 0x10013c82
        s.chain = fat<char*>(cur, 0xb4);                                         // 0x10013c85 / 0x10013c8b
        if (s.chain != 0) {                                                      // 0x10013c8e / 0x10013c91
            s.loopStart = fat<char*>(s.chain, 0xd8);                             // 0x10013c93 / 0x10013c99
            if (ecx != 0) continue;                                              // 0x10013c9c / 0x10013c9f
            break;                                                               // 0x10013ca5
        }
        s.chained = 0;                                                           // 0x10013ca7
        s.loopFlag = 0;                                                          // 0x10013cae
        if (ecx != 0) continue;                                                  // 0x10013cbb / 0x10013cbe
        break;                                                                   // 0x10013cbe fallthrough
    }
    *ppIn = esi;                                                                 // 0x10013cca
    *ppOut = edi;                                                                // 0x10013ccf
    return mixEpilogue(v, mode, count, ecx, s.wraps);                            // 0x10013cd6-0x10013d34
}

// Gain / fade prologue of mixResampleInnerB (VAs 0x10014521-0x100145eb) and mixResampleInnerC (0x100165ac-0x1001667b; the fade-step floor differs, see below).
// Mode 1: gainL = +0x1e8 and gainR = +0x1f8 of the voice (0x10014539-0x1001454b), step 0, the sample record as mixRenderA. Other modes: gainL = +0x78,
// gainR = +0x68 (0x10014572-0x10014581); when bit 3 (8) of byte +0x9c is set (`test bl, 8` 0x1001457b) the fade step is dword +0xec / count (unsigned
// `div dword ptr [ebp + 0x10]` 0x1001458b: a count of 0 faults), and InnerB raises a zero step to 1 (0x1001458e / 0x10014593 / 0x10014595) while InnerC keeps
// only a step of 1 (`cmp eax, edi` 0x1001661e, a zero step stays 0). Flags and chain: mixRenderA's.
struct MixG { int gainL, gainR, step; };
static void mixPrologueG(char* v, int mode, unsigned count, MixS& s, MixG& g, bool floorAtOne) {
    g.step = 0;
    if (mode == 1) {
        g.gainL = fat<int>(v, 0x1e8);
        g.gainR = fat<int>(v, 0x1f8);
    } else {
        g.gainL = fat<int>(v, 0x78);
        g.gainR = fat<int>(v, 0x68);
        if (fat<unsigned char>(v, 0x9c) & 8) {
            unsigned q = fat<unsigned>(v, 0xec) / count;
            g.step = static_cast<int>(q);
            if (floorAtOne ? (q == 0) : false) g.step = 1;
        }
    }
    mixPrologueA(v, mode, s);
}
// the fade step of one channel: gain - step when that stays >= 0 (signed `sub` / `jge`), else 0
static inline int mixFade(int gain, int step) {
    long long d = static_cast<long long>(gain) - step;
    return (d >= 0) ? static_cast<int>(d) : 0;
}

// 0x00014510  mixResampleInnerB (VA 0x10014510): mono 16-bit samples added with saturation to a left and a right dword, each with its own gain, the gains
// fading by the step per frame. A frame (0x10014600-0x1001462d): eax = (short)in[0]; the low dword of eax * gainL (`imul edx` 0x10014607) is added to
// the dword at edi (0x10014609-0x10014624), the SAME sample times gainR (0x1001462d) to the next (0x1001462f-0x1001464a); esi += 2 (0x1001464f). Past the
// limit (0x10014652 / 0x10014655) the wrap handler (0x1001467f-0x10014703) is mixRenderA's with no ring. Otherwise gainR and gainL each drop by the step,
// to 0 when that would go below 0 (0x10014657-0x10014674), and the loop continues while the count is non-zero (0x10014677 / 0x10014678). The prologue
// (0x10014521-0x100145eb) and epilogue (0x10014706-0x10014776) are described above.
Mix5_t MixResampleInnerB_orig;
unsigned __cdecl MixResampleInnerB_re(int** ppOut, short** ppIn, unsigned count, char* v, int mode) {
    MixS s; MixG g;
    mixPrologueG(v, mode, count, s, g, true);                                    // 0x10014521-0x100145eb
    unsigned ecx = count;                                                        // 0x100145f3
    short* esi = *ppIn;                                                          // 0x100145f9
    int* edi = *ppOut;                                                           // 0x100145fe
    for (;;) {
        int smp = static_cast<int>(esi[0]);                                      // 0x10014600 / 0x10014603
        int eax = mixMul(smp, g.gainL);                                          // 0x10014604 / 0x10014607
        mixSatAddE(edi, eax); edi++;                                             // 0x10014609-0x10014624
        eax = mixMul(smp, g.gainR);                                              // 0x10014629 / 0x1001462a / 0x1001462d
        mixSatAddE(edi, eax); edi++;                                             // 0x1001462f-0x1001464a
        esi += 1;                                                                // 0x1001464f
        if (mixBelow(reinterpret_cast<char*>(esi), s.limit)) {                   // 0x10014652 / 0x10014655
            g.gainR = mixFade(g.gainR, g.step);                                  // 0x10014657-0x10014664
            g.gainL = mixFade(g.gainL, g.step);                                  // 0x10014667-0x10014674
            ecx--;                                                               // 0x10014677
            if (ecx != 0) continue;                                              // 0x10014678
            break;                                                               // 0x1001467a
        }
        ecx--;                                                                   // 0x1001467f
        if (s.chained == 0) {                                                    // 0x10014680 / 0x10014684
            if (s.loopFlag == 0) break;                                          // 0x10014686 / 0x1001468a
            s.wraps++;                                                           // 0x1001468c
            esi = reinterpret_cast<short*>(s.loopStart);                         // 0x10014699
            *ppIn = esi;                                                         // 0x1001469c
            if (ecx != 0) continue;                                              // 0x1001469e / 0x100146a1
            break;                                                               // 0x100146a7
        }
        s.wraps++;                                                               // 0x100146a9
        esi = reinterpret_cast<short*>(s.loopStart);                             // 0x100146b6
        *ppIn = esi;                                                             // 0x100146b9
        char* cur = s.chain;                                                     // 0x100146bb
        s.limit = fat<char*>(cur, 0xd4);                                         // 0x100146be / 0x100146c4
        s.chain = fat<char*>(cur, 0xb4);                                         // 0x100146c7 / 0x100146cd
        if (s.chain != 0) {                                                      // 0x100146d0 / 0x100146d3
            s.loopStart = fat<char*>(s.chain, 0xd8);                             // 0x100146d5 / 0x100146db
            if (ecx != 0) continue;                                              // 0x100146de / 0x100146e1
            break;                                                               // 0x100146e7
        }
        s.chained = 0;                                                           // 0x100146e9
        s.loopFlag = 0;                                                          // 0x100146f0
        if (ecx != 0) continue;                                                  // 0x100146fd / 0x10014700
        break;                                                                   // 0x10014700 fallthrough
    }
    *ppIn = esi;                                                                 // 0x1001470c
    *ppOut = edi;                                                                // 0x10014711
    return mixEpilogue(v, mode, count, ecx, s.wraps);                            // 0x10014718-0x10014776
}

// 0x000165a0  mixResampleInnerC (VA 0x100165a0): mixResampleInnerB with plain stores (no accumulation, no saturation: 0x10016699 / 0x100166a4) and without the
// wrap counter for a NON-chained looping voice (0x100166dd-0x100166f0 has no `inc [ebp - 0x10]`); a chained wrap counts (0x100166f2). The prologue
// (0x100165ac-0x1001667b) differs from InnerB's only in the fade-step floor.
Mix5_t MixResampleInnerC_orig;
unsigned __cdecl MixResampleInnerC_re(int** ppOut, short** ppIn, unsigned count, char* v, int mode) {
    MixS s; MixG g;
    mixPrologueG(v, mode, count, s, g, false);                                   // 0x100165ac-0x1001667b
    unsigned ecx = count;                                                        // 0x10016683
    short* esi = *ppIn;                                                          // 0x10016689
    int* edi = *ppOut;                                                           // 0x1001668e
    for (;;) {
        int smp = static_cast<int>(esi[0]);                                      // 0x10016690 / 0x10016693
        edi[0] = mixMul(smp, g.gainL);                                           // 0x10016694 / 0x10016697 / 0x10016699
        edi++;                                                                   // 0x1001669b
        edi[0] = mixMul(smp, g.gainR);                                           // 0x1001669e / 0x1001669f / 0x100166a2 / 0x100166a4
        edi++;                                                                   // 0x100166a6
        esi += 1;                                                                // 0x100166a9
        if (mixBelow(reinterpret_cast<char*>(esi), s.limit)) {                   // 0x100166ac / 0x100166af
            g.gainR = mixFade(g.gainR, g.step);                                  // 0x100166b1-0x100166be
            g.gainL = mixFade(g.gainL, g.step);                                  // 0x100166c1-0x100166ce
            ecx--;                                                               // 0x100166d1
            if (ecx != 0) continue;                                              // 0x100166d2
            break;                                                               // 0x100166d4
        }
        ecx--;                                                                   // 0x100166d6
        if (s.chained == 0) {                                                    // 0x100166d7 / 0x100166db
            if (s.loopFlag == 0) break;                                          // 0x100166dd / 0x100166e1
            esi = reinterpret_cast<short*>(s.loopStart);                         // 0x100166e6
            *ppIn = esi;                                                         // 0x100166e9
            if (ecx != 0) continue;                                              // 0x100166eb / 0x100166ee
            break;                                                               // 0x100166f0
        }
        s.wraps++;                                                               // 0x100166f2
        esi = reinterpret_cast<short*>(s.loopStart);                             // 0x100166f8
        *ppIn = esi;                                                             // 0x100166fb
        char* cur = s.chain;                                                     // 0x100166fd
        s.limit = fat<char*>(cur, 0xd4);                                         // 0x10016700 / 0x10016706
        s.chain = fat<char*>(cur, 0xb4);                                         // 0x10016709 / 0x1001670f
        if (s.chain != 0) {                                                      // 0x10016712 / 0x10016715
            s.loopStart = fat<char*>(s.chain, 0xd8);                             // 0x10016717 / 0x1001671d
            if (ecx != 0) continue;                                              // 0x10016720 / 0x10016723
            break;                                                               // 0x10016729
        }
        s.chained = 0;                                                           // 0x1001672b
        s.loopFlag = 0;                                                          // 0x10016732
        if (ecx != 0) continue;                                                  // 0x1001673f / 0x10016742
        break;                                                                   // 0x10016742 fallthrough
    }
    *ppIn = esi;                                                                 // 0x1001674e
    *ppOut = edi;                                                                // 0x10016753
    return mixEpilogue(v, mode, count, ecx, s.wraps);                            // 0x1001675a-0x100167b8
}
// <<< PART mix_part2




// >>> PART ds_part

// =====================================================================================================================
// N. DsBuffer methods over a COM-shaped interface (the fixture answers the interface calls with recording stdcall stubs)
// =====================================================================================================================
typedef unsigned(__stdcall* ComCall2_t)(void*, void*);

// Map of the DirectSound / COM result codes the DsBuffer methods return to (0x1000fa38-0x1000fa7f; the table of DsBuffer::create is a copy at 0x1000fc8c-0x1000fcd1):
//  > 0x8878000a (signed): index = code - 0x88780064, above 0x46 -> 0x2f; the byte table at 0x1000faf4 (jump table 0x1000fae0) sends the index 0 to 0x12 (0x1000fa5b),
//  0x14 (0x88780078) to 1 (0x1000fa51), 0x32 (0x88780096) to 0x1e (0x1000fa6f), 0x46 (0x887800aa) to 0x13 (0x1000fa65) and every other index to 0x2f (0x1000fa79);
//  0x8878000a -> 0xc (0x1000fa2e); > 0x8007000e (signed): 0x80070057 -> 0xa (0x1000fa24), else 0x2f; 0x8007000e -> 0x11 (0x1000fa13); 0x80004001 -> 0xb (0x1000fa09);
//  anything else -> 0x2f (0x1000fa79).
static unsigned dsErrorCode(unsigned code) {
    int c = static_cast<int>(code);
    if (c > static_cast<int>(0x8878000au)) {                                     // 0x1000f9f0 / 0x1000f9f5
        unsigned idx = code + 0x7787ff9cu;                                       // 0x1000fa38
        if (idx > 0x46) return 0x2f;                                             // 0x1000fa3d / 0x1000fa40
        switch (idx) {                                                           // 0x1000fa44-0x1000fa4a
            case 0x00: return 0x12;                                              // 0x1000fa5b
            case 0x14: return 1;                                                 // 0x1000fa51
            case 0x32: return 0x1e;                                              // 0x1000fa6f
            case 0x46: return 0x13;                                              // 0x1000fa65
            default: return 0x2f;                                                // 0x1000fa79
        }
    }
    if (code == 0x8878000au) return 0xc;                                         // 0x1000f9f7 / 0x1000fa2e
    if (c > static_cast<int>(0x8007000eu)) {                                     // 0x1000f9f9 / 0x1000f9fe
        if (code == 0x80070057u) return 0xa;                                     // 0x1000fa1d / 0x1000fa22 / 0x1000fa24
        return 0x2f;                                                             // 0x1000fa79
    }
    if (code == 0x8007000eu) return 0x11;                                        // 0x1000fa00 / 0x1000fa13
    if (code == 0x80004001u) return 0xb;                                         // 0x1000fa02 / 0x1000fa07 / 0x1000fa09
    return 0x2f;                                                                 // 0x1000fa79
}

// 0x0000f990  DsBuffer::setPrimaryFormat (VA 0x1000f990). One stack argument, a WAVEFORMATEX pointer (`ret 4`). The interface pointer at +0x60 and bit 1 of the dword at +0
// decide (0x1000f994-0x1000f9d7): interface + bit clear -> 6 (0x1000f9a3 / 0x1000f9a6); no interface + bit set -> 6 (0x1000f9bb / 0x1000f9be); no interface + bit clear -> the
// format is copied into this+0xa8 (four dwords and a word: 0x1000faae-0x1000fad4) and the result is 0; interface + bit set -> the interface's slot +0x38 (SetFormat,
// `call [edx + 0x38]` 0x1000f9e5) is called with the format; a zero result copies the format (0x1000fa83-0x1000faa7) and returns 0, a non-zero one is mapped by dsErrorCode.
Th1_t DsSetPrimaryFormat_orig;
unsigned __fastcall DsSetPrimaryFormat_re(void* self, void*, unsigned fmt) {
    void* iface = fat<void*>(self, 0x60);                                        // 0x1000f994
    bool bit1 = (fat<unsigned>(self, 0) & 2) == 2;                               // 0x1000f99b-0x1000f9a0
    auto copyFmt = [&](const void* f) {
        for (int i = 0; i < 16; i += 4) fat<unsigned>(self, 0xa8 + i) = fat<unsigned>(const_cast<void*>(f), i);   // 0x1000fa83-0x1000fa9c
        fat<unsigned short>(self, 0xa8 + 0x10) = fat<unsigned short>(const_cast<void*>(f), 0x10);                 // 0x1000fa9f-0x1000faa3
    };
    if (iface) {
        if (!bit1) return 6;                                                     // 0x1000f9a3 / 0x1000f9a6
        unsigned hr = reinterpret_cast<ComCall2_t>(c3do_slot(iface, 0x38))(iface, reinterpret_cast<void*>(fmt));   // 0x1000f9dd-0x1000f9e5
        if (hr != 0) return dsErrorCode(hr);                                     // 0x1000f9e8 / 0x1000f9ea
        copyFmt(reinterpret_cast<void*>(fmt));                                   // 0x1000fa83
        return 0;                                                                // 0x1000faa8
    }
    if (bit1) return 6;                                                          // 0x1000f9bb / 0x1000f9be
    copyFmt(reinterpret_cast<void*>(fmt));                                       // 0x1000faae
    return 0;                                                                    // 0x1000face
}

// 0x0000f810  DsBuffer::lockEntire (VA 0x1000f810). No interface at +0x60 returns 0x24 (0x1000f816-0x1000f81b, 0x1000f8a9). The interface's Lock (slot +0x2c, `call [ecx + 0x2c]` 0x1000f840)
// is called for offset 0, the byte count of the dword at +0x78 and the flag 2, with four local out slots for the two regions. A non-zero result returns 0x1a (0x1000f845 /
// 0x1000f89f). Otherwise the first region is zeroed (rep stosd of bytes1 / 4 dwords, then rep stosb of the remaining bytes1 & 3: 0x1000f847-0x1000f85c), the second region too when
// its byte count is non-zero (0x1000f85e-0x1000f87a), and the interface's Unlock (slot +0x4c, 0x1000f894) gets (ptr1, bytes1, ptr2, bytes2); its result is ignored and the
// function returns 0 (0x1000f898-0x1000f89e).
Th0u_t DsLockEntire_orig;
typedef unsigned(__stdcall* ComLock_t)(void*, unsigned, unsigned, void**, unsigned*, void**, unsigned*, unsigned);
typedef unsigned(__stdcall* ComUnlock_t)(void*, void*, unsigned, void*, unsigned);
unsigned __fastcall DsLockEntire_re(void* self, void*) {
    void* iface = fat<void*>(self, 0x60);                                        // 0x1000f816
    if (!iface) return 0x24;                                                     // 0x1000f819 / 0x1000f81b / 0x1000f8a9
    void* p1 = 0; void* p2 = 0; unsigned b1 = 0, b2 = 0;                         // four local slots (0x1000f821-0x1000f838)
    unsigned hr = reinterpret_cast<ComLock_t>(c3do_slot(iface, 0x2c))(iface, 0, fat<unsigned>(self, 0x78), &p1, &b1, &p2, &b2, 2);   // 0x1000f83c-0x1000f840
    if (hr != 0) return 0x1a;                                                    // 0x1000f843 / 0x1000f845 / 0x1000f89f
    for (unsigned i = 0; i < b1; i++) static_cast<unsigned char*>(p1)[i] = 0;    // 0x1000f850-0x1000f85c
    if (b2) for (unsigned i = 0; i < b2; i++) static_cast<unsigned char*>(p2)[i] = 0;   // 0x1000f862-0x1000f878
    reinterpret_cast<ComUnlock_t>(c3do_slot(fat<void*>(self, 0x60), 0x4c))(fat<void*>(self, 0x60), p1, b1, p2, b2);   // 0x1000f87e-0x1000f894
    return 0;                                                                    // 0x1000f898
}

// 0x00010860  DsBuffer::lockCaptureRegion (VA 0x10010860). Two stack arguments (a1 = a byte count, a2 = a result cell; `ret 8`). The interface's slot +0x10
// (GetCurrentPosition, `call [ecx + 0x10]` 0x10010881) gets two local cursors (both zeroed first: 0x10010875 / 0x10010879), the first of which (the write cursor) is used;
// a non-zero result returns 6 (0x10010884 / 0x1001088f). Bit 0 of the byte at +4 set: it is cleared (0x1001089e-0x100108a5), the start is dword +0x40 + the cursor wrapped at
// the size in +0x78 (0x100108a8-0x100108b9) and +0x28 / +0x24 take the cursor (0x100108ad / 0x100108b2); clear: the start is dword +0x30 (0x100108bb). The end a1 + start is
// wrapped at +0x78 as well (0x100108c2-0x100108cb), +0x24 takes the cursor again (0x100108cd), +0x30 takes the end (0x100108d4), *a2 takes the start (0x100108d8) and
// the result is 0 (0x100108da).
typedef unsigned(__stdcall* ComGetPos_t)(void*, unsigned*, unsigned*);
typedef unsigned(__fastcall* Th2_t)(void*, void*, unsigned, unsigned);
Th2_t DsLockCapture_orig;
unsigned __fastcall DsLockCapture_re(void* self, void*, unsigned a1, unsigned a2) {
    unsigned wr = 0, pl = 0;                                                     // 0x10010875 / 0x10010879
    void* iface = fat<void*>(self, 0x60);                                        // 0x1001086b
    unsigned hr = reinterpret_cast<ComGetPos_t>(c3do_slot(iface, 0x10))(iface, &pl, &wr);   // 0x10010881 (first pushed out slot = write cursor)
    if (hr != 0) return 6;                                                       // 0x10010884 / 0x10010886 / 0x10010889
    unsigned char al = fat<unsigned char>(self, 4);                              // 0x10010895
    unsigned start;
    if (al & 1) {                                                                // 0x1001089c / 0x1001089e
        unsigned size = fat<unsigned>(self, 0x78);                               // 0x100108a0
        fat<unsigned char>(self, 4) = static_cast<unsigned char>(al & 0xfe);     // 0x100108a3 / 0x100108a5
        start = fat<unsigned>(self, 0x40) + wr;                                  // 0x100108a8 / 0x100108ab
        fat<unsigned>(self, 0x28) = wr;                                          // 0x100108ad
        fat<unsigned>(self, 0x24) = wr;                                          // 0x100108b2
        if (!(start < size)) start -= size;                                      // 0x100108b0 / 0x100108b5 / 0x100108b7
    } else {
        start = fat<unsigned>(self, 0x30);                                       // 0x100108bb
    }
    unsigned size2 = fat<unsigned>(self, 0x78);                                  // 0x100108c2
    unsigned end = a1 + start;                                                   // 0x100108be / 0x100108c5
    if (!(end < size2)) end -= size2;                                            // 0x100108c7 / 0x100108c9 / 0x100108cb
    fat<unsigned>(self, 0x24) = wr;                                              // 0x100108cd
    fat<unsigned>(self, 0x30) = end;                                             // 0x100108d4
    fat<unsigned>(reinterpret_cast<void*>(a2), 0) = start;                       // 0x100108d8
    return 0;                                                                    // 0x100108da
}
// <<< PART ds_part

// >>> PART list_part

// =====================================================================================================================
// L. Removal from the doubly linked value lists (voice list, sound list, module registry) and the registry append
// =====================================================================================================================
// A list header is {head +0, back +4, cursor +8, count +0xc} (at voice+0x1338 / sound owner +0x1c8 / module 0x100b5000) and a node {prev +0, next +4, value +8}. Unlinking
// a node (voiceListRemove 0x10032470-0x100324b7, removeSoundNode 0x1000e07c-0x1000e0cb, unregisterSound 0x10005eca-0x10005f15): when there is a previous node its next
// takes the node's next, otherwise the head takes it; when there is a next node its prev takes the node's prev and the cursor takes the next node, otherwise the cursor is
// cleared and the back pointer takes the node's prev; the node goes to operator delete and the count drops by one.
static void listUnlink(char* hdr, char* node) {
    char* prev = fat<char*>(node, 0);
    if (prev) fat<char*>(prev, 4) = fat<char*>(node, 4);
    else fat<char*>(hdr, 0) = fat<char*>(node, 4);
    char* next = fat<char*>(node, 4);
    if (next) {
        fat<char*>(next, 0) = fat<char*>(node, 0);
        fat<char*>(hdr, 8) = next;
    } else {
        fat<unsigned>(hdr, 8) = 0;
        fat<char*>(hdr, 4) = fat<char*>(node, 0);
    }
    c3do_delete(node);
    fat<int>(hdr, 0xc) = fat<int>(hdr, 0xc) - 1;
}

// 0x00032440  voiceListRemove (VA 0x10032440). One stack argument (`ret 4`), the value to remove; the list header sits at this+0x1338. A NULL argument ends at once
// (0x10032449 / 0x1003244c, eax untouched); otherwise the dword at arg+0x20 is cleared first (0x1003244e), also when the value is not in the list; an empty list
// (0x10032457 / 0x10032459) or a value not found (the walk along next: 0x1003245b-0x10032465 -> 0x10032467) ends there; the found node is unlinked as above (0x1003246c-0x100324b7,
// header offsets +0x1338 head, +0x133c back, +0x1340 cursor, +0x1344 count).
Th1_t VoiceListRemove_orig;
void __fastcall VoiceListRemove_re(void* self, void*, unsigned arg) {
    if (!arg) return;                                                            // 0x10032449 / 0x1003244c
    fat<unsigned>(reinterpret_cast<void*>(arg), 0x20) = 0;                       // 0x1003244e
    char* node = fat<char*>(self, 0x1338);                                       // 0x10032451
    if (!node) return;                                                           // 0x10032457 / 0x10032459
    while (fat<unsigned>(node, 8) != arg) {                                      // 0x1003245b / 0x1003245e
        node = fat<char*>(node, 4);                                              // 0x10032460
        if (!node) return;                                                       // 0x10032463 / 0x10032465
    }
    listUnlink(static_cast<char*>(self) + 0x1338, node);                         // 0x1003246c-0x100324b7
}

// 0x0000e050  removeSoundNode (VA 0x1000e050). One stack argument (`ret 4`), the value to remove; the list header is this+0x1c8. An empty list (0x1000e054 / 0x1000e05c) or a
// value not found (walk 0x1000e062-0x1000e06c) returns 0x24 (0x1000e06f). The found node is unlinked (0x1000e07c-0x1000e0cb, header +0x1c8 / +0x1cc / +0x1d0 / +0x1d4)
// and its value (always the argument) is kept in edi (0x1000e0b9): a NULL value returns 0x24 (0x1000e0cc / 0x1000e0d5), anything else clears bit 6 (0x40) of the dword at
// value+0xc8 (0x1000e0d7-0x1000e0e0) and returns 0 (0x1000e0e6).
Th1_t RemoveSoundNode_orig;
unsigned __fastcall RemoveSoundNode_re(void* self, void*, unsigned arg) {
    char* node = fat<char*>(self, 0x1c8);                                        // 0x1000e054
    if (!node) return 0x24;                                                      // 0x1000e05a / 0x1000e05c
    while (fat<unsigned>(node, 8) != arg) {                                      // 0x1000e062 / 0x1000e065
        node = fat<char*>(node, 4);                                              // 0x1000e067
        if (!node) return 0x24;                                                  // 0x1000e06a / 0x1000e06c
    }
    unsigned value = fat<unsigned>(node, 8);                                     // 0x1000e0b9
    listUnlink(static_cast<char*>(self) + 0x1c8, node);                          // 0x1000e07c-0x1000e0c8
    if (value == 0) return 0x24;                                                 // 0x1000e0cc / 0x1000e0d5
    fat<unsigned>(reinterpret_cast<void*>(arg), 0xc8) = fat<unsigned>(reinterpret_cast<void*>(arg), 0xc8) & 0xffffffbfu;   // 0x1000e0d7-0x1000e0e0
    return 0;                                                                    // 0x1000e0e6
}

// 0x00005eb0  unregisterSound (VA 0x10005eb0). No stack argument: ecx is the value to remove from the module list at 0x100b5000 (head) / 0x100b5004 (back) / 0x100b5008 (cursor) /
// 0x100b500c (count) (0x10005eb0-0x10005f15). An empty list (0x10005eb5 / 0x10005eb7) or a value not found (0x10005eb9-0x10005ec3) returns at once; the result is 0 on every path
// (the empty-list jump reaches `xor eax, eax` 0x10005f1a, the walk ends with eax = 0).
Th0u_t UnregisterSound_orig;
unsigned __fastcall UnregisterSound_re(void* value, void*) {
    char* hdr = c3do_sound(0x000b5000);
    char* node = fat<char*>(hdr, 0);                                             // 0x10005eb0
    if (!node) return 0;                                                         // 0x10005eb5 / 0x10005eb7 -> 0x10005f1a
    while (fat<void*>(node, 8) != value) {                                       // 0x10005eb9 / 0x10005ebc
        node = fat<char*>(node, 4);                                              // 0x10005ebe
        if (!node) return 0;                                                     // 0x10005ec1 / 0x10005ec3 (eax = 0)
    }
    listUnlink(hdr, node);                                                       // 0x10005eca-0x10005f15
    return 0;                                                                    // 0x10005f1a
}

// 0x00005e00  registerSound (VA 0x10005e00). appendNodeB 0x10005e30 (thunk 0x100026c1, call 0x10005e09) appends this to the module list at 0x100b5000 (ecx = the list header, the
// sound pushed: 0x10005e03-0x10005e09); then bit 1 (value 2) of the dword at this+0x214 is set (0x10005e0e-0x10005e16); the result is 0 (0x10005e1c).
typedef unsigned(__fastcall* Th1p_t)(void*, void*, void*);
Th0u_t RegisterSound_orig;
unsigned __fastcall RegisterSound_re(void* self, void*) {
    c3do_fn<Th1p_t>(0x00005e30)(c3do_sound(0x000b5000), 0, self);                // 0x10005e09 (thunk 0x100026c1)
    fat<unsigned>(self, 0x214) = fat<unsigned>(self, 0x214) | 2;                 // 0x10005e0e-0x10005e16
    return 0;                                                                    // 0x10005e1c
}

// 0x000228d0  Seq_removeTrackById (VA 0x100228d0). One stack argument, a track id (`ret 4`). The sequencer's track list has its head at +0x218, back +0x21c, cursor +0x220, count +0x224.
// The cursor takes the head (0x100228d3 / 0x100228db); an empty list or a NULL value ends with 0x24 (0x100228e1 / 0x100228ec -> 0x1002299d). The walk compares the dword at track+4 with the id
// (0x100228f6 / 0x100228f9) and advances the cursor along next, ending with 0x24 on a NULL cursor, NULL next node or NULL value (0x10022903, 0x1002290c-0x10022914, 0x1002291d-0x1002291f). The found
// track is looked up again by value from the head (0x10022927-0x1002293b; the walk cannot fail, its two exits would return 0), unlinked as in listUnlink (0x10022945-0x10022981, header +0x218)
// and its node released with operator delete (0x10022982); the count drops (0x10022987-0x10022991). The result is 0 (0x10022997).
Th1_t SeqRemoveTrackById_orig;
unsigned __fastcall SeqRemoveTrackById_re(void* self, void*, unsigned id) {
    char* node = fat<char*>(self, 0x218);                                        // 0x100228d3
    fat<char*>(self, 0x220) = node;                                              // 0x100228db
    if (!node) return 0x24;                                                      // 0x100228d9 / 0x100228e1
    char* tr = fat<char*>(node, 8);                                              // 0x100228e7
    if (!tr) return 0x24;                                                        // 0x100228ea / 0x100228ec
    while (fat<unsigned>(tr, 4) != id) {                                         // 0x100228f6 / 0x100228f9
        node = fat<char*>(self, 0x220);                                          // 0x100228fb
        if (!node) return 0x24;                                                  // 0x10022901 / 0x10022903
        node = fat<char*>(node, 4);                                              // 0x10022909
        fat<char*>(self, 0x220) = node;                                          // 0x1002290e
        if (!node) return 0x24;                                                  // 0x1002290c / 0x10022914
        tr = fat<char*>(node, 8);                                                // 0x1002291a
        if (!tr) return 0x24;                                                    // 0x1002291d / 0x1002291f
    }
    node = fat<char*>(self, 0x218);                                              // 0x10022927
    if (!node) return 0;                                                         // 0x1002292d / 0x1002292f -> 0x10022997
    while (fat<char*>(node, 8) != tr) {                                          // 0x10022931 / 0x10022934
        node = fat<char*>(node, 4);                                              // 0x10022936
        if (!node) return 0;                                                     // 0x10022939 / 0x1002293b
    }
    listUnlink(static_cast<char*>(self) + 0x218, node);                          // 0x10022945-0x10022991
    return 0;                                                                    // 0x10022997
}
// <<< PART list_part

// >>> PART mix_rated

// ---------------------------------------------------------------------------------------------------------------------
// M3. The rated mixers: the eight 8/16-bit x mono/stereo x add/store variants that step through the samples by a 16.16 rate with linear interpolation
// ---------------------------------------------------------------------------------------------------------------------
// One family, eight members (VA of the function): mixRenderOuterA2 0x10013dd0 (16-bit stereo, add), mixRenderRatedB 0x10014820 (16-bit mono, add),
// mixResampleD 0x100152b0 (8-bit stereo, add), mixResampleE 0x100156c0 (8-bit mono, add), mixResampleG 0x10015f70 (16-bit stereo, store),
// mixRenderOuterC 0x10016850 (16-bit mono, store), mixResampleH 0x10016d90 (8-bit stereo, store), mixResampleI 0x10017160 (8-bit mono, store).
// Common frame (every member, same stack slots; the position `ring` is a 48-bit fixed-point value: dword at ring+2 = fraction word + the low integer
// word, dword at ring+4 = the integer part):
//   - samples: 16-bit members read signed shorts, 8-bit members read bytes as (b << 8) - 0x8000 (`movzx; shl eax, 8; sub eax, 0x8000`);
//     a stereo frame is two samples, a mono frame one sample that feeds BOTH output slots (left gain first, then right gain);
//   - per channel: next = the sample one frame ahead when that frame starts below the limit (signed compare), otherwise 0, or the first sample of
//     the loop start when loopFlag is set (the members with an eof flag also clear loopFlag there: after the LEFT channel, so a stereo frame's right
//     channel then sees 0);
//   - value = cur + ((next - cur) * fraction word >> 16) (arithmetic shift of the low dword of `imul`), times the channel gain (low dword);
//   - the output dword is added with saturation (add members) or stored (store members);
//   - then ring += rate (add dword at ring+2, carry into the word at ring+6) and the input is esi = *ppIn + (integer part << log2(frame bytes));
//     past the limit the wrap handler of mixRenderA runs, but it also zeroes the integer part (dword ring+4) and restarts at the loop start;
//   - at the end the integer part is saved (it is the number of input steps that the voice counters take), cleared, and *ppIn / *ppOut are stored.
// Differences between members are template parameters (the A/B shows nothing else differing): BITS8, STEREO, ADD, EOF (mode 1 builds the eof flag from the
// sample record, set when limit == record+0x24 and bit 5 of byte voice+0x58), WRAPS_LOOP (a wrap of a NON-chained looping voice counts in the wrap total),
// FADE (0: none; 1: fade step = max(1, +0xec / count); 2: step = +0xec / count), POSWORD (the position step uses only the word at ring+4), FALLTHRU
// (the wrap handler of a non-chained looping voice falls into the chained block when the count is spent: mixRenderOuterA2 only).
// Epilogue of the rated members (e.g. 0x100140e0-0x10014147): the integer part of the position (the input steps taken since the last wrap) goes to
// voice+0x16c (mode 1) or voice+0x88, the wrap total to voice+0x1360 or voice+0xcc; the result is the number of output frames, count - remaining.
static unsigned mixEpilogueR(char* v, int mode, unsigned count, unsigned remaining, unsigned advance, int wraps) {
    if (mode == 1) {
        fat<unsigned>(v, 0x16c) = fat<unsigned>(v, 0x16c) + advance;
        fat<unsigned>(v, 0x1360) = fat<unsigned>(v, 0x1360) + static_cast<unsigned>(wraps);
    } else {
        fat<unsigned>(v, 0x88) = fat<unsigned>(v, 0x88) + advance;
        fat<unsigned>(v, 0xcc) = fat<unsigned>(v, 0xcc) + static_cast<unsigned>(wraps);
    }
    return count - remaining;
}
template <bool BITS8, bool STEREO, bool ADD, bool EOF, bool WRAPS_LOOP, int FADE, bool POSWORD, bool FALLTHRU>
static unsigned mixRatedKernel(int** ppOut, short** ppIn, unsigned count, char* v, int mode, MixS& s, int gainL, int gainR, unsigned rate, int step, char* ring) {
    const int SAMP = BITS8 ? 1 : 2;
    const int STRIDE = SAMP * (STEREO ? 2 : 1);
    const int SHIFT = (STRIDE == 4) ? 2 : (STRIDE == 2 ? 1 : 0);
    auto dec = [](const char* p) -> int { return BITS8 ? (static_cast<int>(static_cast<unsigned char>(p[0])) << 8) - 0x8000 : static_cast<int>(*reinterpret_cast<const short*>(p)); };
    auto put = [](int*& edi, int value) {
        if (ADD) mixSatAdd(edi, value); else *edi = value;
        edi++;
    };
    unsigned ecx = count;
    char* esi = reinterpret_cast<char*>(*ppIn);
    int* edi = *ppOut;
    for (;;) {
        unsigned saved = ecx;
        unsigned frac = *reinterpret_cast<unsigned short*>(ring + 2);
        for (int ch = 0; ch < (STEREO ? 2 : 1); ch++) {
            int cur = dec(esi + ch * SAMP);
            char* nx = esi + STRIDE;
            int next;
            if (static_cast<int>(reinterpret_cast<intptr_t>(nx)) < static_cast<int>(reinterpret_cast<intptr_t>(s.limit))) {
                next = dec(nx + ch * SAMP);
            } else {
                next = 0;
                if (s.loopFlag) {
                    next = dec(s.loopStart + ch * SAMP);
                    if (EOF && s.eof) s.loopFlag = 0;
                }
            }
            int interp = (mixMul(next - cur, static_cast<int>(frac)) >> 16) + cur;
            if (STEREO) {
                put(edi, mixMul(interp, ch == 0 ? gainL : gainR));
            } else {
                put(edi, mixMul(interp, gainL));
                put(edi, mixMul(interp, gainR));
            }
        }
        // ring += rate: the dword at ring+2, the carry into the word at ring+6
        unsigned lo = fat<unsigned>(ring, 2);
        unsigned nlo = lo + rate;
        fat<unsigned>(ring, 2) = nlo;
        if (nlo < rate) fat<unsigned short>(ring, 6) = static_cast<unsigned short>(fat<unsigned short>(ring, 6) + 1);
        unsigned ip = POSWORD ? (nlo >> 16) : fat<unsigned>(ring, 4);
        esi = reinterpret_cast<char*>(*ppIn) + (ip << SHIFT);
        if (static_cast<int>(reinterpret_cast<intptr_t>(esi)) < static_cast<int>(reinterpret_cast<intptr_t>(s.limit))) {
            if (FADE) {
                gainR = mixFade(gainR, step);
                gainL = mixFade(gainL, step);
            }
            ecx = saved - 1;
            if (ecx != 0) continue;
            break;
        }
        ecx = saved - 1;
        if (s.chained == 0) {
            if (s.loopFlag == 0) break;
            if (WRAPS_LOOP) s.wraps++;
            fat<unsigned>(ring, 4) = 0;
            esi = s.loopStart;
            *ppIn = reinterpret_cast<short*>(esi);
            if (ecx != 0) continue;
            if (!FALLTHRU) break;                    // mixRenderOuterA2 has no `jmp` after its `jne` (0x10014057): the count spent falls into the chained block
        }
        s.wraps++;
        fat<unsigned>(ring, 4) = 0;
        esi = s.loopStart;
        *ppIn = reinterpret_cast<short*>(esi);
        char* cur = s.chain;
        s.limit = fat<char*>(cur, 0xd4);
        s.chain = fat<char*>(cur, 0xb4);
        if (s.chain != 0) {
            s.loopStart = fat<char*>(s.chain, 0xd8);
            if (ecx != 0) continue;
            break;
        }
        s.chained = 0;
        s.loopFlag = 0;
        if (ecx != 0) continue;
        break;
    }
    unsigned advance = fat<unsigned>(ring, 4);
    fat<unsigned>(ring, 4) = 0;
    *ppIn = reinterpret_cast<short*>(esi);
    *ppOut = edi;
    return mixEpilogueR(v, mode, count, ecx, advance, s.wraps);
}

// Prologue shared by the eight rated members (stack slots as in mixRenderA). Mode 1 (e.g. 0x10013df2-0x10013e5c): gainL = +0x1e8, gainR = +0x1f8, rate = +0x220,
// ring = voice+0x1358, the sample record from voice+8 as in mixRenderA; the members with an eof flag also set loopFlag = (limit == record+0x24) and, when
// byte voice+0x58 has bit 5, eof = loopFlag; the others leave both at 0. Other modes (e.g. 0x10013e61-0x10013f26): rate = +0x4c, gainL = +0x78, gainR = +0x68,
// ring = voice+0xb8 and mixRenderA's flag / chain code.
struct MixR { int gainL, gainR; unsigned rate; char* ring; };
static void mixPrologueR(char* v, int mode, bool eofLogic, MixS& s, MixR& r) {
    if (mode == 1) {
        r.gainL = fat<int>(v, 0x1e8);
        r.gainR = fat<int>(v, 0x1f8);
        r.rate = fat<unsigned>(v, 0x220);
        r.ring = v + 0x1358;
        mixPrologueA(v, mode, s);
        if (eofLogic) {
            char* bo = fat<char*>(v, 8);
            s.loopFlag = (reinterpret_cast<unsigned>(s.limit) == fat<unsigned>(bo, 0x24)) ? 1 : 0;
            if ((fat<unsigned char>(v, 0x58) & 0x20) && s.loopFlag != 0) s.eof = 1;
        }
    } else {
        r.gainL = fat<int>(v, 0x78);
        r.gainR = fat<int>(v, 0x68);
        r.rate = fat<unsigned>(v, 0x4c);
        r.ring = v + 0xb8;
        mixPrologueA(v, mode, s);
    }
}

// 0x00013dd0  mixRenderOuterA2 (VA 0x10013dd0): 16-bit stereo, add. Prologue 0x10013dd9-0x10013f26. Outside mode 1 a unity rate (+0x4c == 0x10000, 0x10013e6d /
// 0x10013e7c) hands the work to mixRenderA 0x10013890 (thunk 0x10001d02, call 0x10013e94) when both gains are unity too (0x10013e7e / 0x10013e82), and to
// mixResampleInnerA 0x10013b10 (thunk 0x1000262b, call 0x10013eb1) otherwise; their result is the result. Kernel: frame loop 0x10013f3b-0x10014024
// (interpolation 0x10013f4a-0x10013f60, right channel 0x10013fac-0x10013fbe, ring update 0x10013fff-0x1001400d, limit test 0x10014017), wrap handler
// 0x10014029-0x100140c1, epilogue 0x100140e0-0x10014147.
Mix5_t MixRenderOuterA2_orig;
unsigned __cdecl MixRenderOuterA2_re(int** ppOut, short** ppIn, unsigned count, char* v, int mode) {
    MixS s; MixR r;
    s.loopFlag = 0; s.chained = 0; s.wraps = 0; s.eof = 0; s.chain = 0;
    if (mode != 1) {                                                             // 0x10013dde / 0x10013df0
        unsigned rate = fat<unsigned>(v, 0x4c);                                  // 0x10013e64
        int gl = fat<int>(v, 0x78), gr = fat<int>(v, 0x68);                      // 0x10013e67 / 0x10013e6a
        if (rate == 0x10000) {                                                   // 0x10013e6d / 0x10013e7c
            if (static_cast<unsigned>(gl) == rate && static_cast<unsigned>(gr) == rate)        // 0x10013e7e / 0x10013e80 / 0x10013e82 / 0x10013e84
                return c3do_fn<Mix5_t>(0x00013890)(ppOut, ppIn, count, v, mode);    // 0x10013e94 (thunk 0x10001d02)
            return c3do_fn<Mix5_t>(0x00013b10)(ppOut, ppIn, count, v, mode);        // 0x10013eb1 (thunk 0x1000262b)
        }
    }
    mixPrologueR(v, mode, true, s, r);
    return mixRatedKernel<false, true, true, true, true, 0, false, true>(ppOut, ppIn, count, v, mode, s, r.gainL, r.gainR, r.rate, 0, r.ring);
}

// 0x00014820  mixRenderRatedB (VA 0x10014820): 16-bit mono, add, fade. In mode 1 (prologue 0x1001484f-0x10014905) unity gains with a unity rate call
// mixCopyInnerB 0x10014250 (thunk 0x100014ce, call 0x1001488b), unity gains alone are unchanged and a unity rate calls mixResampleInnerB 0x10014510
// (thunk 0x100015e6, call 0x100148b0); other modes (0x1001490a-0x100149f8) first compute the fade step: with bit 3 (8) of byte +0x9c, +0xec / count, 1
// when that is 0 (0x10014924-0x1001493b); unity gains and rate with no step call mixCopyInnerB (0x10014969), a unity rate call mixResampleInnerB (0x10014991).
// Kernel: frame loop 0x10014a10-0x10014a90 (position step from the word at ring+4 only: 0x10014a4a-0x10014a63), wrap handler 0x10014af5-0x10014b8f
// (a wrap of a non-chained looping voice counts: 0x10014b07), epilogue 0x10014baa-0x10014c15.
Mix5_t MixRenderRatedB_orig;
unsigned __cdecl MixRenderRatedB_re(int** ppOut, short** ppIn, unsigned count, char* v, int mode) {
    MixS s; MixR r;
    s.loopFlag = 0; s.chained = 0; s.wraps = 0; s.eof = 0; s.chain = 0;
    int step = 0;
    if (mode == 1) {                                                             // 0x10014835 / 0x10014849
        unsigned gl = fat<unsigned>(v, 0x1e8), gr = fat<unsigned>(v, 0x1f8), rate = fat<unsigned>(v, 0x220);   // 0x10014852-0x10014864
        if (gl == 0x10000 && gr == gl) {                                         // 0x10014864 / 0x10014873 / 0x10014875 / 0x10014877
            if (rate == gl) return c3do_fn<Mix5_t>(0x00014250)(ppOut, ppIn, count, v, mode);   // 0x10014879 / 0x1001487b / 0x1001488b
        }
        if (rate == 0x10000) return c3do_fn<Mix5_t>(0x00014510)(ppOut, ppIn, count, v, mode);  // 0x1001489a / 0x100148a0 / 0x100148b0
    } else {
        unsigned gl = fat<unsigned>(v, 0x78), gr = fat<unsigned>(v, 0x68), rate = fat<unsigned>(v, 0x4c);   // 0x10014913 / 0x10014916 / 0x1001493e
        if (fat<unsigned char>(v, 0x9c) & 8) {                                   // 0x1001491c / 0x10014922
            unsigned q = fat<unsigned>(v, 0xec) / count;                         // 0x10014924-0x1001492c
            step = static_cast<int>(q);                                          // 0x10014934
            if (q == 0) step = 1;                                                // 0x1001492f / 0x10014934 / 0x10014936
        }
        if (gl == 0x10000 && gr == gl) {                                         // 0x10014941 / 0x1001494a / 0x1001494c / 0x1001494e
            if (rate == gl) {                                                    // 0x10014950 / 0x10014952
                if (step == 0) return c3do_fn<Mix5_t>(0x00014250)(ppOut, ppIn, count, v, mode);   // 0x10014954 / 0x10014956 / 0x10014969
            } else {
                goto kernel0;                                                    // 0x10014952
            }
        }
        if (rate == 0x10000) return c3do_fn<Mix5_t>(0x00014510)(ppOut, ppIn, count, v, mode);  // 0x10014978 / 0x1001497e / 0x10014991
    }
kernel0:
    mixPrologueR(v, mode, true, s, r);
    return mixRatedKernel<false, false, true, true, true, 1, true, false>(ppOut, ppIn, count, v, mode, s, r.gainL, r.gainR, r.rate, step, r.ring);
}

// 0x000152b0  mixResampleD (VA 0x100152b0): 8-bit stereo, add; no eof flag. Prologue 0x100152b6-0x10015391 (mode 1: gains 0x1e8 / 0x1f8, rate 0x220, ring
// 0x1358; other modes 0x78 / 0x68 / 0x4c / 0xb8). Frame loop 0x100153a6-0x1001549f, wrap handler 0x100154ab-0x10015545, epilogue 0x10015564-0x100155cc.
Mix5_t MixResampleD_orig;
unsigned __cdecl MixResampleD_re(int** ppOut, short** ppIn, unsigned count, char* v, int mode) {
    MixS s; MixR r;
    s.loopFlag = 0; s.chained = 0; s.wraps = 0; s.eof = 0; s.chain = 0;
    mixPrologueR(v, mode, false, s, r);
    return mixRatedKernel<true, true, true, false, true, 0, false, false>(ppOut, ppIn, count, v, mode, s, r.gainL, r.gainR, r.rate, 0, r.ring);
}

// 0x000156c0  mixResampleE (VA 0x100156c0): 8-bit mono, add; eof flag (byte [ebp - 1], set 0x10015741). Prologue 0x100156c6-0x100157ba, frame loop 0x100157cf-
// 0x1001589a, wrap handler 0x10015899-0x10015940 (no `jmp` after the `jne` at 0x100158c5: with the count spent a wrapping non-chained voice falls into the chained
// block at 0x100158cb and faults on its NULL chain), epilogue to 0x10015a9b.
Mix5_t MixResampleE_orig;
unsigned __cdecl MixResampleE_re(int** ppOut, short** ppIn, unsigned count, char* v, int mode) {
    MixS s; MixR r;
    s.loopFlag = 0; s.chained = 0; s.wraps = 0; s.eof = 0; s.chain = 0;
    mixPrologueR(v, mode, true, s, r);
    return mixRatedKernel<true, false, true, true, true, 0, false, true>(ppOut, ppIn, count, v, mode, s, r.gainL, r.gainR, r.rate, 0, r.ring);
}

// 0x00015f70  mixResampleG (VA 0x10015f70): 16-bit stereo, store; eof flag. Prologue 0x10015f7c-0x10016067, frame loop 0x1001607c-0x1001612d, wrap handler
// 0x10016138-0x100161d2, epilogue 0x100161ec-0x10016258.
Mix5_t MixResampleG_orig;
unsigned __cdecl MixResampleG_re(int** ppOut, short** ppIn, unsigned count, char* v, int mode) {
    MixS s; MixR r;
    s.loopFlag = 0; s.chained = 0; s.wraps = 0; s.eof = 0; s.chain = 0;
    mixPrologueR(v, mode, true, s, r);
    return mixRatedKernel<false, true, false, true, true, 0, false, false>(ppOut, ppIn, count, v, mode, s, r.gainL, r.gainR, r.rate, 0, r.ring);
}

// 0x00016850  mixRenderOuterC (VA 0x10016850): 16-bit mono, store, fade; a wrap of a non-chained looping voice does not count. Mode 1 (prologue 0x10016879-
// 0x10016936): unity gains and unity rate call mixCopyInnerC 0x10016340 (thunk 0x1000251d, call 0x100168b5); unity rate with other gains calls mixResampleInnerC
// 0x100165a0 (thunk 0x100019ec, call 0x100168db with the mode argument 1). Other modes (0x1001693b-0x10016a23): unity gains and rate with bit 3 of byte +0x9c
// clear call mixCopyInnerC (0x10016976); a unity rate otherwise calls mixResampleInnerC (0x1001699b); only the remaining cases reach the kernel, whose fade step
// is +0xec / count when that bit is set (0x100169b5-0x100169c8: no floor). Frame loop 0x10016a3b-0x10016ad5, wrap handler 0x10016ada-0x10016b71, epilogue
// 0x10016b8b-0x10016bf7.
Mix5_t MixRenderOuterC_orig;
unsigned __cdecl MixRenderOuterC_re(int** ppOut, short** ppIn, unsigned count, char* v, int mode) {
    MixS s; MixR r;
    s.loopFlag = 0; s.chained = 0; s.wraps = 0; s.eof = 0; s.chain = 0;
    int step = 0;
    if (mode == 1) {                                                             // 0x1001685e / 0x10016873
        unsigned gl = fat<unsigned>(v, 0x1e8), gr = fat<unsigned>(v, 0x1f8), rate = fat<unsigned>(v, 0x220);   // 0x1001687c-0x1001688e
        if (gl == 0x10000 && gr == gl) {                                         // 0x1001688e / 0x1001689d / 0x1001689f / 0x100168a1
            if (rate == gl) return c3do_fn<Mix5_t>(0x00016340)(ppOut, ppIn, count, v, mode);   // 0x100168a3 / 0x100168a5 / 0x100168b5
        } else if (rate == 0x10000) {                                            // 0x100168c4 / 0x100168ca
            return c3do_fn<Mix5_t>(0x000165a0)(ppOut, ppIn, count, v, 1);       // 0x100168db
        }
    } else {
        unsigned gl = fat<unsigned>(v, 0x78), gr = fat<unsigned>(v, 0x68), rate = fat<unsigned>(v, 0x4c);   // 0x1001693e-0x10016944
        bool toResample = false;
        if (gl == 0x10000 && gr == gl) {                                         // 0x10016947 / 0x10016955 / 0x10016957 / 0x10016959
            if (rate == gl) {                                                    // 0x1001695b / 0x1001695d
                if (!(fat<unsigned char>(v, 0x9c) & 8)) return c3do_fn<Mix5_t>(0x00016340)(ppOut, ppIn, count, v, mode);   // 0x1001695f / 0x10016966 / 0x10016976
                toResample = true;                                               // 0x10016966 -> 0x10016985
            }
        } else {
            toResample = true;                                                   // 0x10016955 / 0x10016959 -> 0x10016985
        }
        if (toResample && rate == 0x10000) return c3do_fn<Mix5_t>(0x000165a0)(ppOut, ppIn, count, v, mode);   // 0x10016985 / 0x1001698b / 0x1001699b
        if (fat<unsigned char>(v, 0x9c) & 8) step = static_cast<int>(fat<unsigned>(v, 0xec) / count);   // 0x100169aa-0x100169c8
    }
    mixPrologueR(v, mode, true, s, r);
    return mixRatedKernel<false, false, false, true, false, 2, false, false>(ppOut, ppIn, count, v, mode, s, r.gainL, r.gainR, r.rate, step, r.ring);
}

// 0x00016d90  mixResampleH (VA 0x10016d90): 8-bit stereo, store; no eof flag. Prologue 0x10016d9c-0x10016e79, frame loop 0x10016e89-0x10016f4a, epilogue to 0x10017073.
Mix5_t MixResampleH_orig;
unsigned __cdecl MixResampleH_re(int** ppOut, short** ppIn, unsigned count, char* v, int mode) {
    MixS s; MixR r;
    s.loopFlag = 0; s.chained = 0; s.wraps = 0; s.eof = 0; s.chain = 0;
    mixPrologueR(v, mode, false, s, r);
    return mixRatedKernel<true, true, false, false, true, 0, false, false>(ppOut, ppIn, count, v, mode, s, r.gainL, r.gainR, r.rate, 0, r.ring);
}

// 0x00017160  mixResampleI (VA 0x10017160): 8-bit mono, store; eof flag; a wrap of a non-chained looping voice does not count (0x10017326). Prologue 0x10017169-0x10017280,
// frame loop 0x10017280-0x1001730f, wrap handler 0x10017314-0x100173ab.
Mix5_t MixResampleI_orig;
unsigned __cdecl MixResampleI_re(int** ppOut, short** ppIn, unsigned count, char* v, int mode) {
    MixS s; MixR r;
    s.loopFlag = 0; s.chained = 0; s.wraps = 0; s.eof = 0; s.chain = 0;
    mixPrologueR(v, mode, true, s, r);
    return mixRatedKernel<true, false, false, true, false, 0, false, false>(ppOut, ppIn, count, v, mode, s, r.gainL, r.gainR, r.rate, 0, r.ring);
}
// <<< PART mix_rated

}  // namespace

// ---------------------------------------------------------------------------------------------------------------------
// Hook registrations
// ---------------------------------------------------------------------------------------------------------------------
// 0x0001b570  ChainObject_dtor
SG_HOOK("sound.dll", 0x0001b570, ChainObject_dtor, ChainObject_dtor_re, ChainObject_dtor_orig);
// 0x0001c990  Ctrl5b58c_dtorEmbedded
SG_HOOK("sound.dll", 0x0001c990, Ctrl5b58c_dtorEmbedded, Ctrl5b58c_dtorEmbedded_re, Ctrl5b58c_dtorEmbedded_orig);
// 0x0001c080  Ctrl5b58c_dtor
SG_HOOK("sound.dll", 0x0001c080, Ctrl5b58c_dtor, Ctrl5b58c_dtor_re, Ctrl5b58c_dtor_orig);
// 0x0001b870  MidiReader_freeContent
SG_HOOK("sound.dll", 0x0001b870, MidiReader_freeContent, MidiReader_freeContent_re, MidiReader_freeContent_orig);
// 0x0001bea0  ChainObject_scalarDtor
SG_HOOK("sound.dll", 0x0001bea0, ChainObject_scalarDtor, ChainObject_scalarDtor_re, ChainObject_scalarDtor_orig);
// 0x0001c950  Ctrl5b58c_scalarDtor
SG_HOOK("sound.dll", 0x0001c950, Ctrl5b58c_scalarDtor, Ctrl5b58c_scalarDtor_re, Ctrl5b58c_scalarDtor_orig);
// 0x0001e7d0  MidiReader_dtor
SG_HOOK("sound.dll", 0x0001e7d0, MidiReader_dtor, MidiReader_dtor_re, MidiReader_dtor_orig);
// 0x0002ddb0  Channel_propagateFlag0
SG_HOOK("sound.dll", 0x0002ddb0, Channel_propagateFlag0, ChanPropFlag_re, ChanPropFlag_orig);
// 0x00030080  Sound5c450_ctor
SG_HOOK("sound.dll", 0x00030080, Sound5c450_ctor, Sound5c450Ctor_re, Sound5c450Ctor_orig);
// 0x00031200  StreamReader_ctor
SG_HOOK("sound.dll", 0x00031200, StreamReader_ctor, StreamReaderCtor_re, StreamReaderCtor_orig);
// 0x00006020  WaveStream::ctor
SG_HOOK("sound.dll", 0x00006020, WaveStream_ctor, WaveStreamCtor_re, WaveStreamCtor_orig);
// 0x0000eef0  DsBuffer::ctor
SG_HOOK("sound.dll", 0x0000eef0, DsBuffer_ctor, DsBufferCtor_re, DsBufferCtor_orig);
// 0x0000f590  DsBuffer::initDesc
SG_HOOK("sound.dll", 0x0000f590, DsBuffer_initDesc, DsBufferInitDesc_re, DsBufferInitDesc_orig);
// 0x000231e0  Seq_cmdHandler7
SG_HOOK("sound.dll", 0x000231e0, Seq_cmdHandler7, SeqCmd7_re, SeqCmd7_orig);
// 0x0001e8e0  Seq_rewindTracks
SG_HOOK("sound.dll", 0x0001e8e0, Seq_rewindTracks, SeqRewind_re, SeqRewind_orig);
// 0x00006f00  resizeDynArray
SG_HOOK("sound.dll", 0x00006f00, resizeDynArray, ResizeDynArray_re, ResizeDynArray_orig);
// 0x00011810  appendNamedNode
SG_HOOK("sound.dll", 0x00011810, appendNamedNode, AppendNamedNode_re, AppendNamedNode_orig);
// 0x0001b510  dynInit_ctrl100b4b50
SG_HOOK("sound.dll", 0x0001b510, dynInit_ctrl100b4b50, dynInit_ctrl100b4b50_re, dynInit_ctrl100b4b50_orig);
// 0x0001b6b0  dynInit_ctrl100b4c38
SG_HOOK("sound.dll", 0x0001b6b0, dynInit_ctrl100b4c38, dynInit_ctrl100b4c38_re, dynInit_ctrl100b4c38_orig);
// 0x0001b710  dynInit_ctrl100b4d20
SG_HOOK("sound.dll", 0x0001b710, dynInit_ctrl100b4d20, dynInit_ctrl100b4d20_re, dynInit_ctrl100b4d20_orig);
// 0x0001b770  dynInit_ctrl100b4a68
SG_HOOK("sound.dll", 0x0001b770, dynInit_ctrl100b4a68, dynInit_ctrl100b4a68_re, dynInit_ctrl100b4a68_orig);
// 0x0001b7d0  dynInit_ctrl100b4e08
SG_HOOK("sound.dll", 0x0001b7d0, dynInit_ctrl100b4e08, dynInit_ctrl100b4e08_re, dynInit_ctrl100b4e08_orig);
// >>> HOOKS mix_part1
// 0x00013890  mixRenderA
SG_HOOK("sound.dll", 0x00013890, mixRenderA, MixRenderA_re, MixRenderA_orig);
// 0x00017eb0  mixCopyF
SG_HOOK("sound.dll", 0x00017eb0, mixCopyF, MixCopyF_re, MixCopyF_orig);
// 0x00016340  mixCopyInnerC
SG_HOOK("sound.dll", 0x00016340, mixCopyInnerC, MixCopyInnerC_re, MixCopyInnerC_orig);
// <<< HOOKS mix_part1
// >>> HOOKS mix_part2
// 0x00014250  mixCopyInnerB
SG_HOOK("sound.dll", 0x00014250, mixCopyInnerB, MixCopyInnerB_re, MixCopyInnerB_orig);
// 0x00013b10  mixResampleInnerA
SG_HOOK("sound.dll", 0x00013b10, mixResampleInnerA, MixResampleInnerA_re, MixResampleInnerA_orig);
// 0x00014510  mixResampleInnerB
SG_HOOK("sound.dll", 0x00014510, mixResampleInnerB, MixResampleInnerB_re, MixResampleInnerB_orig);
// 0x000165a0  mixResampleInnerC
SG_HOOK("sound.dll", 0x000165a0, mixResampleInnerC, MixResampleInnerC_re, MixResampleInnerC_orig);
// <<< HOOKS mix_part2
// >>> HOOKS ds_part
// 0x0000f990  DsBuffer::setPrimaryFormat
SG_HOOK("sound.dll", 0x0000f990, DsBuffer_setPrimaryFormat, DsSetPrimaryFormat_re, DsSetPrimaryFormat_orig);
// 0x0000f810  DsBuffer::lockEntire
SG_HOOK("sound.dll", 0x0000f810, DsBuffer_lockEntire, DsLockEntire_re, DsLockEntire_orig);
// 0x00010860  DsBuffer::lockCaptureRegion
SG_HOOK("sound.dll", 0x00010860, DsBuffer_lockCaptureRegion, DsLockCapture_re, DsLockCapture_orig);
// <<< HOOKS ds_part
// >>> HOOKS list_part
// 0x00032440  voiceListRemove
SG_HOOK("sound.dll", 0x00032440, voiceListRemove, VoiceListRemove_re, VoiceListRemove_orig);
// 0x0000e050  removeSoundNode
SG_HOOK("sound.dll", 0x0000e050, removeSoundNode, RemoveSoundNode_re, RemoveSoundNode_orig);
// 0x00005eb0  unregisterSound
SG_HOOK("sound.dll", 0x00005eb0, unregisterSound, UnregisterSound_re, UnregisterSound_orig);
// 0x00005e00  registerSound
SG_HOOK("sound.dll", 0x00005e00, registerSound, RegisterSound_re, RegisterSound_orig);
// 0x000228d0  Seq_removeTrackById
SG_HOOK("sound.dll", 0x000228d0, Seq_removeTrackById, SeqRemoveTrackById_re, SeqRemoveTrackById_orig);
// <<< HOOKS list_part
// >>> HOOKS mix_rated
// 0x00013dd0  mixRenderOuterA2
SG_HOOK("sound.dll", 0x00013dd0, mixRenderOuterA2, MixRenderOuterA2_re, MixRenderOuterA2_orig);
// 0x00014820  mixRenderRatedB
SG_HOOK("sound.dll", 0x00014820, mixRenderRatedB, MixRenderRatedB_re, MixRenderRatedB_orig);
// 0x000152b0  mixResampleD
SG_HOOK("sound.dll", 0x000152b0, mixResampleD, MixResampleD_re, MixResampleD_orig);
// 0x000156c0  mixResampleE
SG_HOOK("sound.dll", 0x000156c0, mixResampleE, MixResampleE_re, MixResampleE_orig);
// 0x00015f70  mixResampleG
SG_HOOK("sound.dll", 0x00015f70, mixResampleG, MixResampleG_re, MixResampleG_orig);
// 0x00016850  mixRenderOuterC
SG_HOOK("sound.dll", 0x00016850, mixRenderOuterC, MixRenderOuterC_re, MixRenderOuterC_orig);
// 0x00016d90  mixResampleH
SG_HOOK("sound.dll", 0x00016d90, mixResampleH, MixResampleH_re, MixResampleH_orig);
// 0x00017160  mixResampleI
SG_HOOK("sound.dll", 0x00017160, mixResampleI, MixResampleI_re, MixResampleI_orig);
// <<< HOOKS mix_rated

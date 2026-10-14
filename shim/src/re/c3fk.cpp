// C3 batch c3fk (round 34, 2026-10-13) of sound.dll: the nine sound callees round 33 named (re/names/sound_15.tsv), the base
// initialiser 0x1002e3f0 they reach (name proposed in log/c3/c3fk_names.tsv), and the sound rows c3fd left
// (log/c3/c3fk_candidates.tsv, log/c3/round34_brief.md, log/c3/c3fd_notes.txt section F).
// Method as c3fd (shim/src/re/c3fd.cpp): fixture-owned objects only, module globals the body reads swapped for the key with
// the first thread frozen and the multimedia timer quiet, thread-filtered recording stubs on the incremental-link thunks of
// callees whose tree reaches the live engine, operator new / delete through heapstandin.js.
// Addresses in SG_HOOK and in the `// 0x...` comment lines are RVAs; the instruction addresses quoted inside each body are file
// VAs (VA = 0x10000000 + RVA). Every fact comes from the disassembly (log/c3/alt/c3fk/dis/<rva>_<name>.txt, dumped by
// log/c3/alt/c3fk_survey.py and log/c3/alt/c3fk_dis.py), cited by instruction address.
// sound.dll is LoadLibrary'd after the shim starts and loads above its preferred base, so thunks and module globals are taken
// relative to the loaded base. __thiscall is emulated with __fastcall (ecx = this, edx unused). Every callee runs through its
// original address. The C++ EH frames the originals build (`push -1; push <handler>; mov fs:[0], esp`) only unwind a throwing
// callee; nothing these bodies call throws in the A/B, so the reimplementations build none.
#include "hooks.h"
#include <stdint.h>
#include <string.h>

namespace {

template <typename T> T& fat(void* base, int off) {
    return *reinterpret_cast<T*>(reinterpret_cast<char*>(base) + off);
}

char* c3fk_sound(unsigned rva) {
    static HMODULE m = 0;
    if (!m) m = GetModuleHandleA("sound.dll");
    return reinterpret_cast<char*>(m) + rva;
}
template <typename F> F c3fk_fn(unsigned rva) { return reinterpret_cast<F>(c3fk_sound(rva)); }
void*& c3fk_gref(unsigned rva) { return *reinterpret_cast<void**>(c3fk_sound(rva)); }

typedef unsigned(__fastcall* Th0_t)(void*, void*);
typedef unsigned(__fastcall* Th1_t)(void*, void*, unsigned);
typedef unsigned(__fastcall* Th2_t)(void*, void*, unsigned, unsigned);
typedef void*(__cdecl* OpNew_t)(unsigned);
typedef void(__cdecl* OpDelete_t)(void*);
typedef void(__stdcall* VecCtor_t)(void*, unsigned, unsigned, void*, void*);

void* c3fk_new(unsigned n) { return c3fk_fn<OpNew_t>(0x000424d7)(n); }       // operator new 0x100424d7
void c3fk_delete(void* p) { c3fk_fn<OpDelete_t>(0x0004249a)(p); }             // operator delete 0x1004249a

// virtual slot `off` of `obj` (`mov eax, [obj]; call [eax + off]`)
template <typename F> F c3fk_vfn(void* obj, int off) {
    return *reinterpret_cast<F*>(*reinterpret_cast<char**>(obj) + off);
}

// =====================================================================================================================
// A. The callees round 33 named (re/names/sound_15.tsv) and the base initialiser 0x1002e3f0
// =====================================================================================================================

// 0x0002d430  getField204 (VA 0x1002d430). __thiscall, no stack argument: the whole body is `mov eax, [ecx+0x204]`
// (0x1002d430) and `ret` (0x1002d436).
typedef unsigned(__fastcall* GetF204_t)(void*, void*);
GetF204_t GetF204_orig;
unsigned __fastcall GetF204_re(void* self, void*) {
    return fat<unsigned>(self, 0x204);                                         // 0x1002d430
}

// 0x0001ccf0  ChanRecord_init (VA 0x1001ccf0). __thiscall, no stack argument, plain `ret` (0x1001cd19); eax = this
// (`mov eax, ecx` 0x1001ccf0). Zeroes +4 (0x1001ccf9), +0 (0x1001ccfb), +8, +0xc, +0x10, +0x14 (0x1001ccfd-0x1001cd06), +0x1c,
// +0x20, +0x24 (0x1001cd09-0x1001cd0f) and stores 0xfa at +0x18 (0x1001cd12); nothing past +0x27 is written.
typedef void*(__fastcall* Ctor_t)(void*, void*);
Ctor_t CrInit_orig;
void* __fastcall CrInit_re(void* self, void*) {
    fat<unsigned>(self, 0x04) = 0;                                             // 0x1001ccf9
    fat<unsigned>(self, 0x00) = 0;                                             // 0x1001ccfb
    fat<unsigned>(self, 0x08) = 0;                                             // 0x1001ccfd
    fat<unsigned>(self, 0x0c) = 0;                                             // 0x1001cd00
    fat<unsigned>(self, 0x10) = 0;                                             // 0x1001cd03
    fat<unsigned>(self, 0x14) = 0;                                             // 0x1001cd06
    fat<unsigned>(self, 0x1c) = 0;                                             // 0x1001cd09
    fat<unsigned>(self, 0x20) = 0;                                             // 0x1001cd0c
    fat<unsigned>(self, 0x24) = 0;                                             // 0x1001cd0f
    fat<unsigned>(self, 0x18) = 0xfa;                                          // 0x1001cd12
    return self;                                                               // 0x1001ccf0
}

// 0x0001cd30  ChanRecord_freeBuffers (VA 0x1001cd30). __thiscall, no stack argument, plain `ret` (0x1001cd54). A non-NULL +0x1c
// goes to operator delete 0x1004249a (`test` / `je` 0x1001cd36 / 0x1001cd38, call 0x1001cd3b), then a non-NULL +0x24 too
// (0x1001cd46 / 0x1001cd48, call 0x1001cd4b). Neither field is cleared. Every caller discards eax (Seq_setupChannel 0x10021042,
// 0x10021169; Sound5b6a8_dtor 0x1001ffb3): void.
typedef void(__fastcall* Void0_t)(void*, void*);
Void0_t CrFree_orig;
void __fastcall CrFree_re(void* self, void*) {
    if (void* a = fat<void*>(self, 0x1c)) c3fk_delete(a);                      // 0x1001cd33-0x1001cd3b
    if (void* b = fat<void*>(self, 0x24)) c3fk_delete(b);                      // 0x1001cd43-0x1001cd4b
}

// 0x0001b160  StrPtr_assign (VA 0x1001b160). __thiscall (s), `ret 4`. A NULL s returns 0xa (`test ebx, ebx` / `jne`
// 0x1001b168 / 0x1001b16a, 0x1001b16d). A non-NULL old *this goes to operator delete (0x1001b176-0x1001b17d). Then
// operator new(strlen(s) + 1) (`repne scasb` length 0x1001b186-0x1001b191, call 0x1001b192, no NULL test) is stored at *this
// (0x1001b1a3) and s with its terminator is copied there (`rep movsd` / `rep movsb` 0x1001b1a5-0x1001b1bd); 0 is returned
// (`xor eax, eax` 0x1001b1b8).
typedef unsigned(__fastcall* StrAssign_t)(void*, void*, const char*);
StrAssign_t StrAssign_orig;
unsigned __fastcall StrAssign_re(void* self, void*, const char* s) {
    if (!s) return 0xa;                                                        // 0x1001b168-0x1001b173
    if (void* old = fat<void*>(self, 0)) c3fk_delete(old);                     // 0x1001b176-0x1001b17d
    unsigned n = static_cast<unsigned>(strlen(s)) + 1;                         // 0x1001b186-0x1001b18f
    char* p = static_cast<char*>(c3fk_new(n));                                 // 0x1001b192
    fat<char*>(self, 0) = p;                                                   // 0x1001b1a3
    memcpy(p, s, n);                                                           // 0x1001b1a5-0x1001b1bd
    return 0;                                                                  // 0x1001b1b8
}

// 0x0001ae50  ChannelTable_callSlot4 (VA 0x1001ae50). __thiscall (v), `ret 4` (0x1001aecd); v = [esp+0x14] after the
// `sub esp, 8` and two pushes (0x1001ae55). eax, the last object handled, starts at 0 (0x1001ae5f). For the 0x80 slots
// this+4 .. this+0x200 (ebx 0x1001ae61, counter 0x80 0x1001ae64, `add ebx, 4` / `dec` / `jne` 0x1001aeb2-0x1001aeba): a slot
// object that is NULL or equal to eax is skipped (0x1001ae6e-0x1001ae74). Otherwise its vtable slot +4 runs with v
// (0x1001ae76-0x1001ae79); the slot is read again (0x1001ae7c) and n = that object's +0x1438 minus 1 (0x1001ae80 / 0x1001ae86):
// when n > 0 (signed, `jle` 0x1001ae89) each member +0x1440 + 4j of the object, j < n, gets its vtable slot +4 with v
// (0x1001ae90-0x1001ae98), the slot and n re-read after every call (0x1001ae9b-0x1001aeaa, `jl` signed). eax = the slot's object
// read once more (0x1001aeac). Finally +0x218 = v (0x1001aec2); eax (the last object handled, or 0) is what returns.
Th1_t CallSlot4_orig;
unsigned __fastcall CallSlot4_re(void* self, void*, unsigned v) {
    void* last = 0;                                                            // 0x1001ae5f
    for (int k = 0; k < 0x80; ++k) {                                           // 0x1001ae61 / 0x1001ae64 / 0x1001aeba
        void** slot = &fat<void*>(self, 4 + 4 * k);
        void* o = *slot;                                                       // 0x1001ae6c
        if (!o || o == last) continue;                                         // 0x1001ae6e-0x1001ae74
        c3fk_vfn<Th1_t>(o, 4)(o, 0, v);                                        // 0x1001ae76-0x1001ae79
        o = *slot;                                                             // 0x1001ae7c
        for (int j = 0; j < fat<int>(o, 0x1438) - 1; ++j) {                    // 0x1001ae80-0x1001ae89, 0x1001aea1-0x1001aeaa
            void* m = fat<void*>(o, 0x1440 + 4 * j);                           // 0x1001ae90
            c3fk_vfn<Th1_t>(m, 4)(m, 0, v);                                    // 0x1001ae93-0x1001ae98
            o = *slot;                                                         // 0x1001ae9b
        }
        last = *slot;                                                          // 0x1001aeac
    }
    fat<unsigned>(self, 0x218) = v;                                            // 0x1001aec2
    return reinterpret_cast<unsigned>(last);
}

// 0x0002e3f0  NodeB000_initBase (VA 0x1002e3f0; no hooks.csv row yet, name proposed in log/c3/c3fk_names.tsv). __thiscall, no
// stack argument, plain `ret` (0x1002e412); eax = this (`mov eax, ecx` 0x1002e3f0). Zeroes +4, +8, +0xc, +0x10
// (0x1002e3f4-0x1002e3fd), +0 (0x1002e400), +0x14, +0x18, +0x1c (0x1002e402-0x1002e408) and stores 0xffffffff at +0x20
// (0x1002e40b). Only caller: NodeB000_ctor 0x1000b000 through the thunk 0x100012a3 (0x1000b003).
Ctor_t NodeBase_orig;
void* __fastcall NodeBase_re(void* self, void*) {
    fat<unsigned>(self, 0x04) = 0;                                             // 0x1002e3f4
    fat<unsigned>(self, 0x08) = 0;                                             // 0x1002e3f7
    fat<unsigned>(self, 0x0c) = 0;                                             // 0x1002e3fa
    fat<unsigned>(self, 0x10) = 0;                                             // 0x1002e3fd
    fat<unsigned>(self, 0x00) = 0;                                             // 0x1002e400
    fat<unsigned>(self, 0x14) = 0;                                             // 0x1002e402
    fat<unsigned>(self, 0x18) = 0;                                             // 0x1002e405
    fat<unsigned>(self, 0x1c) = 0;                                             // 0x1002e408
    fat<unsigned>(self, 0x20) = 0xffffffffu;                                   // 0x1002e40b
    return self;                                                               // 0x1002e3f0
}

// 0x0000b000  NodeB000_ctor (VA 0x1000b000). __thiscall, no stack argument, plain `ret` (0x1000b01c). Runs NodeB000_initBase
// on this through the thunk 0x100012a3 (0x1000b003; its eax is not used), zeroes +0x24, +0x28, +0x2c, +0x30, +0x34
// (0x1000b00a-0x1000b016) and returns this (`mov eax, esi` 0x1000b019).
Ctor_t NodeB_orig;
void* __fastcall NodeB_re(void* self, void*) {
    c3fk_fn<Th0_t>(0x000012a3)(self, 0);                                       // 0x1000b003
    fat<unsigned>(self, 0x24) = 0;                                             // 0x1000b00a
    fat<unsigned>(self, 0x28) = 0;                                             // 0x1000b00d
    fat<unsigned>(self, 0x2c) = 0;                                             // 0x1000b010
    fat<unsigned>(self, 0x30) = 0;                                             // 0x1000b013
    fat<unsigned>(self, 0x34) = 0;                                             // 0x1000b016
    return self;                                                               // 0x1000b019
}

// 0x00031be0  Channel_ctorB (VA 0x10031be0). __thiscall, no stack argument, plain `ret` (0x10031dcb); eax = this
// (0x10031dbb). First the eh vector constructor ??_L 0x10042902 (__stdcall, five arguments, `ret 0x14`) builds the 0x10
// slots of 0x110 bytes at this+0x238 with the element constructor thunk 0x10001e56 and destructor thunk 0x10001b45 (pushes
// 0x10031c04-0x10031c1b, call 0x10031c20). Then, with ebx = 0, edi = 0x10000, ebp = 0x3ff00000 (0x10031c25, 0x10031c71,
// 0x10031c78), the stores below in the original's order, Channel_recomputeVoiceGains(0x3e8) through the thunk 0x10001efb
// (`push 0x3e8` 0x10031c7d, ecx = this 0x10031cba, call 0x10031d0e), the stores after it, and this+0x230 written into
// +0x300 + 0x110k for k < 0x10 (0x10031d7e-0x10031d91).
Ctor_t ChCtorB_orig;
void* __fastcall ChCtorB_re(void* self, void*) {
    c3fk_fn<VecCtor_t>(0x00042902)(&fat<char>(self, 0x238), 0x110, 0x10,      // 0x10031c04-0x10031c20
                                   c3fk_sound(0x00001e56), c3fk_sound(0x00001b45));
    for (int off = 0x1338; off <= 0x1354; off += 4) fat<unsigned>(self, off) = 0;   // 0x10031c2b-0x10031c55
    fat<unsigned>(self, 0) = 0;                                                // 0x10031c65
    for (int off = 0xc; off < 0x54; off += 4) fat<unsigned>(self, off) = 0;    // `rep stosd` 0x12 dwords 0x10031c5b-0x10031c67
    fat<unsigned>(self, 0x54) = 0;                                             // 0x10031c6e
    fat<unsigned>(self, 0x58) = 0;                                             // 0x10031c76
    fat<unsigned>(self, 0x5c) = 0;                                             // 0x10031c87
    fat<unsigned>(self, 0x164) = 0;                                            // 0x10031c8a
    fat<unsigned>(self, 0x168) = 0;                                            // 0x10031c90
    fat<unsigned>(self, 0x16c) = 0;                                            // 0x10031c96
    fat<unsigned>(self, 0x170) = 0;                                            // 0x10031c9c
    fat<unsigned>(self, 0x1e0) = 0;                                            // 0x10031ca2
    fat<unsigned>(self, 0x210) = 0;                                            // 0x10031ca8
    fat<unsigned>(self, 0x1f0) = 0;                                            // 0x10031cae
    fat<unsigned>(self, 0x200) = 0;                                            // 0x10031cb4
    fat<unsigned>(self, 0x174) = 0;                                            // 0x10031cbc
    fat<unsigned>(self, 0x17c) = 0;                                            // 0x10031cc2
    fat<unsigned>(self, 0x1d8) = 0x10000;                                      // 0x10031cc8
    fat<unsigned>(self, 0x1e4) = 0x3ff00000;                                   // 0x10031cce
    fat<unsigned>(self, 0x208) = 0x10000;                                      // 0x10031cd4
    fat<unsigned>(self, 0x214) = 0x3ff00000;                                   // 0x10031cda
    fat<unsigned>(self, 0x1e8) = 0x10000;                                      // 0x10031ce0
    fat<unsigned>(self, 0x1f4) = 0x3ff00000;                                   // 0x10031ce6
    fat<unsigned>(self, 0x21c) = 0x10000;                                      // 0x10031cec
    fat<unsigned>(self, 0x1f8) = 0x10000;                                      // 0x10031cf2
    fat<unsigned>(self, 0x204) = 0x3ff00000;                                   // 0x10031cf8
    fat<unsigned>(self, 0x218) = 0x10000;                                      // 0x10031cfe
    fat<unsigned>(self, 0x1c0) = 0x3e8;                                        // 0x10031d04
    c3fk_fn<Th1_t>(0x00001efb)(self, 0, 0x3e8);                                // 0x10031c7d / 0x10031d0e
    fat<unsigned>(self, 0x220) = 0x10000;                                      // 0x10031d19
    fat<unsigned>(self, 0x224) = 0;                                            // 0x10031d29
    fat<unsigned>(self, 0x228) = 0;                                            // 0x10031d2f
    fat<unsigned>(self, 0x8) = 0;                                              // 0x10031d35
    fat<unsigned>(self, 0x22c) = 0;                                            // 0x10031d38
    fat<unsigned>(self, 0x230) = 0;                                            // 0x10031d3e
    fat<unsigned>(self, 0x1360) = 0;                                           // 0x10031d40
    fat<unsigned>(self, 0x1368) = 0;                                           // 0x10031d46
    fat<unsigned>(self, 0x136c) = 0;                                           // 0x10031d4c
    for (int off = 0x64; off < 0x164; off += 4) fat<unsigned>(self, off) = 0;  // `rep stosd` 0x40 dwords 0x10031d1f-0x10031d52
    fat<unsigned>(self, 0x1380) = 0;                                           // 0x10031d54
    fat<unsigned>(self, 0x1370) = 0;                                           // 0x10031d5a
    fat<unsigned>(self, 0x1384) = 0;                                           // 0x10031d60
    fat<unsigned>(self, 0x1388) = 0;                                           // 0x10031d66
    fat<unsigned>(self, 0x1358) = 0;                                           // 0x10031d6c
    fat<unsigned>(self, 0x138c) = 0x3ff00000;                                  // 0x10031d72
    fat<unsigned>(self, 0x135c) = 0;                                           // 0x10031d78
    for (int k = 0; k < 0x10; ++k)                                             // 0x10031d7e-0x10031d91
        fat<void*>(self, 0x300 + 0x110 * k) = &fat<char>(self, 0x230);
    fat<unsigned>(self, 0x1398) = 0;                                           // 0x10031d97
    fat<unsigned>(self, 0x1390) = 0;                                           // 0x10031d9d
    fat<unsigned>(self, 0x1394) = 0;                                           // 0x10031da3
    fat<unsigned>(self, 0x139c) = 0;                                           // 0x10031da9
    fat<unsigned>(self, 0x13a0) = 0;                                           // 0x10031daf
    fat<unsigned>(self, 0x13a4) = 0;                                           // 0x10031db5
    return self;                                                               // 0x10031dbb
}

// =====================================================================================================================
// B. The callers they unblock: sequencer channel records, two destructors, the two track builders
// =====================================================================================================================

// module globals (file VAs 0x100b....)
const unsigned kChanDefault = 0x000b49c0;     // copied into Seq +0x210 by Seq_setupChannel (0x10021007 / 0x10021017)
const unsigned kDescHead = 0x000b4fe8;        // list of shared channel objects: head
const unsigned kDescTail = 0x000b4fec;        //   tail
const unsigned kDescCursor = 0x000b4ff0;      //   cursor
const unsigned kDescCount = 0x000b4ff4;       //   count
const unsigned kMmioHead = 0x000b49c8;        // open-mmio node list: head
const unsigned kMmioTail = 0x000b49cc;        //   tail
const unsigned kMmioCursor = 0x000b49d0;      //   cursor
const unsigned kMmioCount = 0x000b49d4;       //   count
const unsigned kOpenCount = 0x000b4a08;       // open stream count

typedef unsigned(__stdcall* MmioClose_t)(unsigned, unsigned);
const unsigned kIatMmioClose = 0x000b8670;    // WINMM!mmioClose [0x100b8670]
template <typename F> F c3fk_imp(unsigned slotRva) { return *reinterpret_cast<F*>(c3fk_sound(slotRva)); }

// Node {+0 previous, +4 next, +8 payload}; a list header {head, tail, cursor, count} at a fixed offset. Pop the head node:
// the successor's +0 = 0 and it becomes the head; with no successor tail and head become 0. Returns the node (not freed).
void* c3fk_popHead(void** head, void** tail) {
    void* n = *head;
    void* next = fat<void*>(n, 4);
    if (next) {
        fat<void*>(next, 0) = 0;
        *head = next;
    } else {
        *tail = 0;
        *head = 0;
    }
    return n;
}

// 0x00020fe0  Seq_setupChannel (VA 0x10020fe0). __thiscall (desc, flag), `ret 8`. ch = desc[0] (0x10021002), name = desc[+0x1c]
// (0x10021004); the global 0x100b49c0 is copied to this+0x210 on every path (0x10021007 / 0x10021017). ch > 0x7f (unsigned
// `ja` 0x1002101d) or a NULL name (0x10021027) returns 0xa (0x10021198). The old record in the slot this+0x744 + 4ch, when
// non-NULL (0x10021034), gets +0x30 = 0 (0x1002103a), ChanRecord_freeBuffers through the thunk 0x10002004 (0x1002103d),
// operator delete (0x10021043) and the slot 0 (0x1002104f). A new record = operator new(0x34) (0x10021058): non-NULL, it runs
// ChanRecord_init through the thunk 0x100014bf (0x10021070) and gets +0x28, +0x2c, +0x30 = 0 (0x10021075-0x1002107b); NULL
// leaves 0 (0x10021082). With desc[+0x24] non-NULL (0x10021093 / 0x10021095): Sound_setAltName(rec, desc[+0x24]) through the
// thunk 0x10001604 (0x1002109e); the cursor 0x100b4ff0 = the head 0x100b4fe8 (0x100210aa) and, while the cursor and its payload
// [cursor+8] are non-NULL (0x100210af-0x100210b6, 0x100210f6-0x10021104), the payload's string *payload is compared with
// desc[+0x24] (inline strcmp 0x100210bf-0x100210e7): equal sets rec+0x30 = payload and payload+0x210 += 1 (0x10021108-0x10021112)
// and ends the walk; otherwise the cursor moves to [cursor+4] (0x100210ee-0x100210f8). Then Seq_mergeChannelDesc(rec, desc)
// through the thunk 0x10001d52 (0x1002111f), slot = rec (0x1002112c), 0 returned (0x10021133). With desc[+0x24] NULL:
// Sound_setName(rec, name) through 0x100010f5 (0x1002113a), Seq_mergeChannelDesc(rec, desc) (0x10021142); when the low byte of
// flag is non-zero (`cmp byte` 0x10021147) r = Seq_loadChannelDesc(rec, desc) through 0x100015d2 (0x10021150), and a non-zero r
// (0x10021159) frees rec as above when it is non-NULL (0x1002115b-0x1002116a), clears the slot (0x1002117c) and returns r
// (0x1002117a). Otherwise slot = rec (0x1002118d) and 0 is returned (0x10021194).
typedef unsigned(__fastcall* SetupCh_t)(void*, void*, unsigned*, unsigned);
SetupCh_t SetupCh_orig;
void c3fk_freeChanRecord(void* r) {
    fat<unsigned>(r, 0x30) = 0;                                                // 0x1002103a / 0x10021161
    c3fk_fn<Th0_t>(0x00002004)(r, 0);                                          // 0x1002103d / 0x10021164
    c3fk_delete(r);                                                            // 0x10021043 / 0x1002116a
}
unsigned __fastcall SetupCh_re(void* self, void*, unsigned* desc, unsigned flag) {
    unsigned ch = desc[0];                                                     // 0x10021002
    unsigned name = desc[0x1c / 4];                                            // 0x10021004
    fat<unsigned>(self, 0x210) = *reinterpret_cast<unsigned*>(c3fk_sound(kChanDefault));   // 0x10021007 / 0x10021017
    if (ch > 0x7f) return 0xa;                                                 // 0x1002100c / 0x1002101d
    if (!name) return 0xa;                                                     // 0x10021025 / 0x10021027
    void** slot = &fat<void*>(self, 0x744 + 4 * ch);
    if (void* old = *slot) {                                                   // 0x1002102d-0x10021036
        c3fk_freeChanRecord(old);
        *slot = 0;                                                             // 0x1002104f
    }
    void* rec = c3fk_new(0x34);                                                // 0x10021058
    if (rec) {                                                                 // 0x10021066 / 0x1002106c
        c3fk_fn<Th0_t>(0x000014bf)(rec, 0);                                    // 0x10021070
        fat<unsigned>(rec, 0x28) = 0;                                          // 0x10021075
        fat<unsigned>(rec, 0x2c) = 0;                                          // 0x10021078
        fat<unsigned>(rec, 0x30) = 0;                                          // 0x1002107b
    }
    if (desc[0x24 / 4]) {                                                      // 0x10021090-0x10021095
        c3fk_fn<Th1_t>(0x00001604)(rec, 0, desc[0x24 / 4]);                    // 0x1002109e
        void* cur = c3fk_gref(kDescHead);                                      // 0x100210a3
        c3fk_gref(kDescCursor) = cur;                                          // 0x100210aa
        void* obj = cur ? fat<void*>(cur, 8) : 0;                              // 0x100210af-0x100210b6
        while (obj) {
            if (strcmp(fat<const char*>(obj, 0), reinterpret_cast<const char*>(desc[0x24 / 4])) == 0) {   // 0x100210b8-0x100210ec
                fat<void*>(rec, 0x30) = obj;                                   // 0x10021108
                fat<unsigned>(obj, 0x210) += 1;                                // 0x1002110b-0x10021112
                break;
            }
            cur = fat<void*>(c3fk_gref(kDescCursor), 4);                       // 0x100210ee / 0x100210f3
            c3fk_gref(kDescCursor) = cur;                                      // 0x100210f8
            if (!cur) break;                                                   // 0x100210fd
            obj = fat<void*>(cur, 8);                                          // 0x100210ff-0x10021104
        }
        c3fk_fn<Th1_t>(0x00001d52)(rec, 0, reinterpret_cast<unsigned>(desc));  // 0x1002111f
        *slot = rec;                                                           // 0x1002112c
        return 0;                                                              // 0x10021133
    }
    c3fk_fn<Th1_t>(0x000010f5)(rec, 0, name);                                  // 0x1002113a
    c3fk_fn<Th1_t>(0x00001d52)(rec, 0, reinterpret_cast<unsigned>(desc));      // 0x10021142
    if (flag & 0xff) {                                                         // 0x10021147 / 0x1002114b
        unsigned r = c3fk_fn<Th1_t>(0x000015d2)(rec, 0, reinterpret_cast<unsigned>(desc));   // 0x10021150
        if (r) {                                                               // 0x10021157 / 0x10021159
            if (rec) c3fk_freeChanRecord(rec);                                 // 0x1002115b-0x1002116a
            *slot = 0;                                                         // 0x1002117c
            return r;                                                          // 0x1002117a
        }
    }
    *slot = rec;                                                               // 0x1002118d
    return 0;                                                                  // 0x10021194
}

// 0x00021fa0  Seq_updateChannel (VA 0x10021fa0). __thiscall (desc), `ret 4`. A NULL desc returns 0xa (0x10021fa7 / 0x10021faa,
// 0x10021faf). rec = the slot this+0x744 + 4 * desc[0] (0x10021fb8 / 0x10021fba, no range test). A NULL rec (0x10021fc7) runs
// Seq_setupChannel(desc, 0) on this through the thunk 0x1000118b (0x10022160-0x10022163) and returns 0 (0x1002216b). Otherwise
// Seq_mergeChannelDesc(rec, desc) through 0x10001d52 (0x10021fd0); desc[+0x24] NULL returns 0 (0x10021fda); else
// Sound_setAltName(rec, desc[+0x24]) through 0x10001604 (0x10021fe3). When rec+0x30 is non-NULL (0x10021feb) and its string
// equals desc[+0x24] (inline strcmp 0x10021fef-0x1002201d) 0 is returned (0x1002201f). Then the walk of the list 0x100b4fe8
// with the cursor 0x100b4ff0 (0x10022025-0x1002208e) looks for a payload whose string equals desc[+0x24]; none returns 0
// (0x10022034, 0x1002203f, 0x10022083, 0x10022090). Found (0x10022099): with rec+0x30 non-NULL (0x100220a0), its +0x210 is
// decremented (0x100220a8-0x100220af); reaching 0 (`jne` 0x100220b5), its node is looked up in the list (0x100220bb-0x100220d3)
// and, when found, unlinked (0x100220d9-0x1002210f: the predecessor's +4 or the head, the successor's +0 and the cursor, or
// cursor 0 and the tail = the predecessor), freed (0x10022116) and the count 0x100b4ff4 decremented (0x10022123); then the old
// object, when non-NULL (0x1002212e), goes to Seq_freeChannelObjects through 0x10001d66 (0x10022132) and operator delete
// (0x10022138), and rec+0x30 = 0 (0x10022140). Finally rec+0x30 = the found payload (0x10022147), its +0x210 += 1
// (0x1002214a-0x10022152), 0 returned (0x1002215a).
typedef unsigned(__fastcall* UpdCh_t)(void*, void*, unsigned*);
UpdCh_t UpdCh_orig;
unsigned __fastcall UpdCh_re(void* self, void*, unsigned* desc) {
    if (!desc) return 0xa;                                                     // 0x10021fa7-0x10021fb5
    void* rec = fat<void*>(self, 0x744 + 4 * desc[0]);                        // 0x10021fb8 / 0x10021fba
    if (!rec) {                                                                // 0x10021fc1 / 0x10021fc7
        c3fk_fn<Th2_t>(0x0000118b)(self, 0, reinterpret_cast<unsigned>(desc), 0);   // 0x10022160-0x10022163
        return 0;                                                              // 0x1002216b
    }
    c3fk_fn<Th1_t>(0x00001d52)(rec, 0, reinterpret_cast<unsigned>(desc));      // 0x10021fd0
    const char* alt = reinterpret_cast<const char*>(desc[0x24 / 4]);           // 0x10021fd5
    if (!alt) return 0;                                                        // 0x10021fd8 / 0x10021fda
    c3fk_fn<Th1_t>(0x00001604)(rec, 0, desc[0x24 / 4]);                        // 0x10021fe3
    if (void* cur0 = fat<void*>(rec, 0x30)) {                                  // 0x10021fe8-0x10021fed
        if (strcmp(fat<const char*>(cur0, 0), reinterpret_cast<const char*>(desc[0x24 / 4])) == 0) return 0;   // 0x10021fef-0x1002201f
    }
    void* cur = c3fk_gref(kDescHead);                                          // 0x10022025
    c3fk_gref(kDescCursor) = cur;                                              // 0x1002202e
    if (!cur) return 0;                                                        // 0x10022034
    void* found = fat<void*>(cur, 8);                                          // 0x1002203a
    if (!found) return 0;                                                      // 0x1002203f
    for (;;) {
        if (strcmp(fat<const char*>(found, 0), reinterpret_cast<const char*>(desc[0x24 / 4])) == 0) break;   // 0x10022045-0x10022076
        cur = fat<void*>(cur, 4);                                              // 0x10022078
        c3fk_gref(kDescCursor) = cur;                                          // 0x1002207d
        if (!cur) return 0;                                                    // 0x10022083
        found = fat<void*>(cur, 8);                                            // 0x10022089
        if (!found) return 0;                                                  // 0x1002208e / 0x10022090-0x10022096
    }
    if (void* old = fat<void*>(rec, 0x30)) {                                   // 0x1002209d-0x100220a2
        if (--fat<unsigned>(old, 0x210) == 0) {                                // 0x100220a8-0x100220b5
            void* n = c3fk_gref(kDescHead);                                    // 0x100220bb
            while (n && fat<void*>(n, 8) != fat<void*>(rec, 0x30)) n = fat<void*>(n, 4);   // 0x100220c3-0x100220d3
            if (n) {                                                           // 0x100220d5 / 0x100220d7
                void* prev = fat<void*>(n, 0);                                 // 0x100220d9
                if (prev) fat<void*>(prev, 4) = fat<void*>(n, 4);              // 0x100220df / 0x100220e2
                else c3fk_gref(kDescHead) = fat<void*>(n, 4);                  // 0x100220e7 / 0x100220ea
                void* next = fat<void*>(n, 4);                                 // 0x100220f0
                if (next) {                                                    // 0x100220f3 / 0x100220f5
                    fat<void*>(next, 0) = fat<void*>(n, 0);                    // 0x100220f7 / 0x100220f9
                    c3fk_gref(kDescCursor) = next;                             // 0x100220fb
                } else {
                    c3fk_gref(kDescCursor) = 0;                                // 0x10022105
                    c3fk_gref(kDescTail) = fat<void*>(n, 0);                   // 0x10022103 / 0x1002210f
                }
                c3fk_delete(n);                                                // 0x10022116
                *reinterpret_cast<int*>(c3fk_sound(kDescCount)) -= 1;          // 0x1002211b-0x10022124
            }
            if (void* o = fat<void*>(rec, 0x30)) {                             // 0x10022129-0x1002212e
                c3fk_fn<Th0_t>(0x00001d66)(o, 0);                              // 0x10022132
                c3fk_delete(o);                                                // 0x10022138
            }
        }
        fat<void*>(rec, 0x30) = 0;                                             // 0x10022140
    }
    fat<void*>(rec, 0x30) = found;                                             // 0x10022147
    fat<unsigned>(found, 0x210) += 1;                                          // 0x1002214a-0x10022152
    return 0;                                                                  // 0x1002215a
}

// 0x0001ff50  Sound5b6a8_dtor (VA 0x1001ff50). __thiscall, no stack argument, plain `ret` (0x1001ffcf). vtable 0x1005b6a8 at +0
// (0x1001ff72). Bit 0 of +0xac (0x1001ff86) runs Midi_Device::closeFile on this+0x60 through the thunk 0x10001014 (0x1001ff8d).
// The 0x7f channel records at +0x744 .. +0x93c (counter 0x7f 0x1001ff98, `jne` 0x1001ffcb): each non-NULL one gets +0x30 = 0,
// ChanRecord_freeBuffers through 0x10002004, operator delete, and its slot 0 (0x1001ffa5-0x1001ffbc). The list {+0x218 head,
// +0x21c tail, +0x224 count} is popped (0x1001ffcd-0x1001fff2) node by node: the node is freed (0x1001fffc), the count
// decremented (0x1002000a), and a NULL payload ends the loop (0x10020015), else the payload is freed too (0x10020018). The eh
// vector destructor ??_M 0x1004280e (__stdcall, four arguments) destroys the 0x10 elements of 0x4c bytes at +0x250 with the
// destructor thunk 0x1000166d (0x10020066-0x1002007b). The list {+0x228, +0x22c, +0x234 count} is popped the same way freeing
// only the nodes (0x10020080-0x1002010b), then the first list again, nodes only (0x1002010d-0x10020198). Seq_dtor runs on
// this+0x60 through 0x100014d8 (0x100201a2-0x100201a9; this is never NULL here) and Ch5be88_dtorA on this through 0x10001cc1
// (0x100201b8). Callers discard eax (Sound5b6a8_scalarDtor 0x1001f6c0, Sound::dtor 0x10004000): void.
void c3fk_drainNodes(void* self, int head, int tail, int count, bool freePayload) {
    while (fat<void*>(self, head)) {
        void* n = c3fk_popHead(&fat<void*>(self, head), &fat<void*>(self, tail));
        void* pl = fat<void*>(n, 8);
        c3fk_delete(n);
        fat<int>(self, count) -= 1;
        if (!pl) break;
        if (freePayload) c3fk_delete(pl);
    }
}
typedef void(__stdcall* VecDtor_t)(void*, unsigned, unsigned, void*);
Void0_t S5b6a8Dtor_orig;
void __fastcall S5b6a8Dtor_re(void* self, void*) {
    fat<void*>(self, 0) = c3fk_sound(0x0005b6a8);                              // 0x1001ff72
    if (fat<unsigned char>(self, 0xac) & 1)                                   // 0x1001ff78 / 0x1001ff86
        c3fk_fn<Th0_t>(0x00001014)(&fat<char>(self, 0x60), 0);                 // 0x1001ff8d
    for (int k = 0; k < 0x7f; ++k) {                                           // 0x1001ff92-0x1001ffcb
        void** slot = &fat<void*>(self, 0x744 + 4 * k);
        if (void* r = *slot) {
            fat<unsigned>(r, 0x30) = 0;                                        // 0x1001ffab
            c3fk_fn<Th0_t>(0x00002004)(r, 0);                                  // 0x1001ffae
            c3fk_delete(r);                                                    // 0x1001ffb4
            *slot = 0;                                                         // 0x1001ffbc
        }
    }
    c3fk_drainNodes(self, 0x218, 0x21c, 0x224, true);                          // 0x1001ffcd-0x10020064
    c3fk_fn<VecDtor_t>(0x0004280e)(&fat<char>(self, 0x250), 0x4c, 0x10, c3fk_sound(0x0000166d));   // 0x10020066-0x1002007b
    c3fk_drainNodes(self, 0x228, 0x22c, 0x234, false);                         // 0x10020080-0x1002010b
    c3fk_drainNodes(self, 0x218, 0x21c, 0x224, false);                         // 0x1002010d-0x10020198
    c3fk_fn<Th0_t>(0x000014d8)(&fat<char>(self, 0x60), 0);                     // 0x100201a2-0x100201a9
    c3fk_fn<Th0_t>(0x00001cc1)(self, 0);                                       // 0x100201b8
}

// 0x00030180  Sound5c450_dtor (VA 0x10030180). __thiscall, no stack argument, plain `ret` (0x10030342). vtable 0x1005c450 at +0
// (0x100301a0). The buffer list {+0x5c head, +0x60 tail, +0x68 count} is popped (0x100301a6-0x100301e4, 0x10030283-0x100302b3):
// node freed, count decremented, and a NULL payload ends the loop. For each payload b: when b+0x44 is non-NULL (0x100301f0 /
// 0x100301f5) mmioClose(b+0x44, 0) runs through the IAT slot [0x100b8670] (0x100301f7-0x100301f9) and the open-mmio list
// 0x100b49c8 is searched for the node whose +8 is b (0x100301fb-0x10030210); a found node is unlinked (0x10030216-0x10030248:
// predecessor's +4 or the head, successor's +0 and the cursor 0x100b49d0, or cursor 0 and the tail 0x100b49cc = predecessor),
// freed (0x1003024f) and the count 0x100b49d4 decremented (0x10030254-0x1003025d). Then the open count 0x100b4a08 is
// decremented (0x10030262-0x1003026b), VoiceSlot_dtorEmpty runs on b through the thunk 0x10001b45 (0x10030275) and b is freed
// (0x1003027b). After the loop (0x100302b9-0x1003031f) whatever the NULL payload left is popped again, freeing nodes only.
// Ch5c234_dtor runs on this through 0x10001cc6 (0x1003032b). The only caller, Sound5c450_scalarDtor 0x10030150, discards eax.
Void0_t S5c450Dtor_orig;
void __fastcall S5c450Dtor_re(void* self, void*) {
    fat<void*>(self, 0) = c3fk_sound(0x0005c450);                              // 0x100301a0
    while (fat<void*>(self, 0x5c)) {                                           // 0x100301a6-0x100301b5, 0x10030283-0x10030288
        void* n = c3fk_popHead(&fat<void*>(self, 0x5c), &fat<void*>(self, 0x60));   // 0x100301bb-0x100301cc
        void* b = fat<void*>(n, 8);                                            // 0x100301cf
        c3fk_delete(n);                                                        // 0x100301d3
        fat<int>(self, 0x68) -= 1;                                             // 0x100301d8-0x100301e1
        if (!b) break;                                                         // 0x100301e4 / 0x100302b3
        if (unsigned h = fat<unsigned>(b, 0x44)) {                             // 0x100301f0-0x100301f5
            c3fk_imp<MmioClose_t>(kIatMmioClose)(h, 0);                        // 0x100301f7-0x100301f9
            void* m = c3fk_gref(kMmioHead);                                    // 0x100301fb
            while (m && fat<void*>(m, 8) != b) m = fat<void*>(m, 4);           // 0x10030200-0x10030210
            if (m) {                                                           // 0x10030212 / 0x10030214
                void* prev = fat<void*>(m, 0);                                 // 0x10030216
                if (prev) fat<void*>(prev, 4) = fat<void*>(m, 4);              // 0x1003021c / 0x1003021f
                else c3fk_gref(kMmioHead) = fat<void*>(m, 4);                  // 0x10030224 / 0x10030227
                void* next = fat<void*>(m, 4);                                 // 0x1003022d
                if (next) {                                                    // 0x10030230 / 0x10030232
                    fat<void*>(next, 0) = fat<void*>(m, 0);                    // 0x10030234 / 0x10030236
                    c3fk_gref(kMmioCursor) = next;                             // 0x10030238
                } else {
                    c3fk_gref(kMmioCursor) = 0;                                // 0x10030242
                    c3fk_gref(kMmioTail) = fat<void*>(m, 0);                   // 0x10030240 / 0x10030248
                }
                c3fk_delete(m);                                                // 0x1003024f
                *reinterpret_cast<int*>(c3fk_sound(kMmioCount)) -= 1;          // 0x10030254-0x1003025d
            }
        }
        *reinterpret_cast<int*>(c3fk_sound(kOpenCount)) -= 1;                  // 0x10030262-0x1003026b
        c3fk_fn<Th0_t>(0x00001b45)(b, 0);                                      // 0x10030273 / 0x10030275
        c3fk_delete(b);                                                        // 0x1003027b
    }
    c3fk_drainNodes(self, 0x5c, 0x60, 0x68, false);                            // 0x100302b9-0x1003031f
    c3fk_fn<Th0_t>(0x00001cc6)(self, 0);                                       // 0x1003032b
}

// List header at a Seq: {+0 head, +4 tail, +8 cursor, +0xc count} (A at +0x218, B at +0x228). Raw append of a fresh node
// {0, 0, payload} (Seq_buildTrack9 0x10022a9e-0x10022b2e, Seq_buildTrack14 0x10022d6a-0x10022e00): operator new(0xc), the
// node filled when non-NULL; with a tail, tail+4 = node and node+0 = the tail (re-read); else head = node; tail = node; count
// re-read and incremented.
void c3fk_rawAppend(void* list, void* payload) {
    void* n = c3fk_new(0xc);
    if (n) {
        fat<unsigned>(n, 0) = 0;
        fat<unsigned>(n, 4) = 0;
        fat<void*>(n, 8) = payload;
    }
    if (void* t = fat<void*>(list, 4)) {
        fat<void*>(t, 4) = n;
        fat<void*>(n, 0) = fat<void*>(list, 4);
    } else {
        fat<void*>(list, 0) = n;
    }
    fat<void*>(list, 4) = n;
    fat<unsigned>(list, 0xc) += 1;
}

// The ordered insertion shared by Seq_buildTrack9 (0x10022b36-0x10022c57) and Seq_buildTrack14 (0x10022e08-0x10022f5b): both
// cursors start at their list's head (0x10022b44 / 0x10022b5e, 0x10022e16 / 0x10022e30); while A's payload is non-NULL and
// node+4 > payload+4 (unsigned, `jbe` 0x10022b6b / 0x10022e3d) A's cursor moves to its +4 (when non-NULL; the payload of a NULL
// cursor is 0) and B's cursor to its +4 when non-NULL. No stop: NodeList_pushBack(A, node) and (B, node) through the thunk
// 0x100025bd. A stop: both cursors step back to their +0 when non-NULL; then A gets NodeList_pushBack when its head is NULL,
// NodeList_pushFront (thunk 0x10001073) when its cursor is NULL, NodeList_pushBack when the cursor's +4 is NULL, otherwise a
// node from operator new(0xc) through NodeList_initNode (thunk 0x1000106e) linked between the cursor and its successor with A's
// count incremented; then NodeList_insert(B, node) through 0x10001ef1. Seq_buildTrack9 steps B's cursor back after A's insertion
// (0x10022c43-0x10022c4c), Seq_buildTrack14 before it (0x10022ea3-0x10022eac): `bFirst`.
void c3fk_orderedInsert(void* self, void* node, bool bFirst) {
    char* a = &fat<char>(self, 0x218);
    char* b = &fat<char>(self, 0x228);
    fat<void*>(a, 8) = fat<void*>(a, 0);
    void* pl = fat<void*>(a, 0) ? fat<void*>(fat<void*>(a, 0), 8) : 0;
    fat<void*>(b, 8) = fat<void*>(b, 0);
    bool stop = false;
    while (pl) {
        if (fat<unsigned>(node, 4) <= fat<unsigned>(pl, 4)) { stop = true; break; }
        if (void* c = fat<void*>(a, 8)) {
            c = fat<void*>(c, 4);
            fat<void*>(a, 8) = c;
            pl = c ? fat<void*>(c, 8) : 0;
        } else {
            pl = 0;
        }
        if (void* c = fat<void*>(b, 8)) fat<void*>(b, 8) = fat<void*>(c, 4);
    }
    if (!stop) {
        c3fk_fn<Th1_t>(0x000025bd)(a, 0, reinterpret_cast<unsigned>(node));
        c3fk_fn<Th1_t>(0x000025bd)(b, 0, reinterpret_cast<unsigned>(node));
        return;
    }
    if (void* c = fat<void*>(a, 8)) fat<void*>(a, 8) = fat<void*>(c, 0);
    if (bFirst)
        if (void* c = fat<void*>(b, 8)) fat<void*>(b, 8) = fat<void*>(c, 0);
    if (!fat<void*>(a, 0)) {
        c3fk_fn<Th1_t>(0x000025bd)(a, 0, reinterpret_cast<unsigned>(node));
    } else if (!fat<void*>(a, 8)) {
        c3fk_fn<Th1_t>(0x00001073)(a, 0, reinterpret_cast<unsigned>(node));
    } else if (void* nx = fat<void*>(fat<void*>(a, 8), 4)) {
        void* p = c3fk_new(0xc);
        void* n = p ? reinterpret_cast<void*>(c3fk_fn<Th1_t>(0x0000106e)(p, 0, reinterpret_cast<unsigned>(node))) : 0;
        fat<void*>(fat<void*>(a, 8), 4) = n;
        fat<void*>(nx, 0) = n;
        fat<void*>(n, 4) = nx;
        fat<void*>(n, 0) = fat<void*>(a, 8);
        fat<unsigned>(a, 0xc) += 1;
    } else {
        c3fk_fn<Th1_t>(0x000025bd)(a, 0, reinterpret_cast<unsigned>(node));
    }
    if (!bFirst)
        if (void* c = fat<void*>(b, 8)) fat<void*>(b, 8) = fat<void*>(c, 0);
    c3fk_fn<Th1_t>(0x00001ef1)(b, 0, reinterpret_cast<unsigned>(node));
}

// 0x000229f0  Seq_buildTrack9 (VA 0x100229f0). __thiscall (desc), `ret 4`. A NULL desc returns 0xa (0x10022a10-0x10022a16).
// node = operator new(0x38) (0x10022a22) constructed by NodeB000_ctor through the thunk 0x10001410 (0x10022a38; NULL leaves 0),
// and nine dwords of desc copied over it (`rep movsd` ecx = 9, 0x10022a3f-0x10022a46, no NULL test). The caller's argument slot
// becomes a local set to 0 (0x10022a48); for i from node+8 to node+0xc inclusive (unsigned, `ja` 0x10022a60 / `jbe`
// 0x10022a86, the bound re-read each time) Seq_listGetAt(this+0x60, &local, i) runs through 0x100015fa (0x10022a62-0x10022a6d)
// and a non-NULL local gets +0x458 = node (0x10022a72-0x10022a7a); the local is not reset between steps. When this+0x224 is 0
// (0x10022a88 / 0x10022a90): Seq_clearTrackFlag8All(this+0x60) through 0x100019e7 (0x10022a99) and a raw append of node to
// list A then list B (0x10022a9e-0x10022b2e). Otherwise the ordered insertion (c3fk_orderedInsert). 0 is returned (0x10022ba6).
typedef unsigned(__fastcall* Build_t)(void*, void*, unsigned*);
Build_t Build9_orig;
unsigned __fastcall Build9_re(void* self, void*, unsigned* desc) {
    if (!desc) return 0xa;                                                     // 0x10022a10-0x10022a1b
    void* p = c3fk_new(0x38);                                                  // 0x10022a22
    void* node = p ? reinterpret_cast<void*>(c3fk_fn<Th0_t>(0x00001410)(p, 0)) : 0;   // 0x10022a2e-0x10022a3d
    memcpy(node, desc, 9 * 4);                                                 // 0x10022a3f-0x10022a46
    void* local = 0;                                                           // 0x10022a48
    for (unsigned i = fat<unsigned>(node, 8); i <= fat<unsigned>(node, 0xc); ++i) {   // 0x10022a50-0x10022a60, 0x10022a80-0x10022a86
        c3fk_fn<Th2_t>(0x000015fa)(&fat<char>(self, 0x60), 0, reinterpret_cast<unsigned>(&local), i);   // 0x10022a62-0x10022a6d
        if (local) fat<void*>(local, 0x458) = node;                            // 0x10022a72-0x10022a7a
    }
    if (fat<unsigned>(self, 0x224) == 0) {                                     // 0x10022a88 / 0x10022a90
        c3fk_fn<Th0_t>(0x000019e7)(&fat<char>(self, 0x60), 0);                 // 0x10022a99
        c3fk_rawAppend(&fat<char>(self, 0x218), node);                         // 0x10022a9e-0x10022ae6
        c3fk_rawAppend(&fat<char>(self, 0x228), node);                         // 0x10022ae4-0x10022b2e
    } else {
        c3fk_orderedInsert(self, node, false);                                 // 0x10022b36-0x10022c57
    }
    return 0;                                                                  // 0x10022ba6
}

// 0x00022d00  Seq_buildTrack14 (VA 0x10022d00). __thiscall (desc), `ret 4`; desc is not tested. node = operator new(0x38)
// (0x10022d1e) constructed by NodeB000_ctor through 0x10001410 (0x10022d36; NULL leaves 0), and 0xe dwords of desc copied over
// it (`rep movsd` ecx = 0xe, 0x10022d49-0x10022d5c, no NULL test). When this+0x224 is 0 (0x10022d5e / 0x10022d64): a raw append
// of node to list A then list B (0x10022d6a-0x10022e00), with no Seq_clearTrackFlag8All call. Otherwise the ordered insertion
// (0x10022e08-0x10022f5b). 0 is returned (0x10022e83).
Build_t Build14_orig;
unsigned __fastcall Build14_re(void* self, void*, unsigned* desc) {
    void* p = c3fk_new(0x38);                                                  // 0x10022d1e
    void* node = p ? reinterpret_cast<void*>(c3fk_fn<Th0_t>(0x00001410)(p, 0)) : 0;   // 0x10022d2c-0x10022d47
    memcpy(node, desc, 0xe * 4);                                               // 0x10022d49-0x10022d5c
    if (fat<unsigned>(self, 0x224) == 0) {                                     // 0x10022d5e / 0x10022d64
        c3fk_rawAppend(&fat<char>(self, 0x218), node);                         // 0x10022d6a-0x10022db4
        c3fk_rawAppend(&fat<char>(self, 0x228), node);                         // 0x10022dab-0x10022e00
    } else {
        c3fk_orderedInsert(self, node, true);                                  // 0x10022e08-0x10022f5b
    }
    return 0;                                                                  // 0x10022e83
}

// 0x00022320  Sound_lookupOrCreate (VA 0x10022320). __thiscall (desc), `ret 4`. A NULL desc returns 0xa (0x1002233d-0x10022345).
// The cursor 0x100b4ff0 = the head 0x100b4fe8 (0x10022366); while the cursor and its payload are non-NULL (0x1002236b /
// 0x10022372, 0x100223af-0x100223c9) the payload's string *payload is compared with getField204(desc) (thunk 0x100025f4,
// 0x10022379; inline strcmp 0x10022380-0x100223a8). An equal one becomes obj (0x100223d1). No match creates one
// (0x100223da-0x100224cb): operator new(0x21c) initialised by Sound_initEntry through 0x10001929 (NULL leaves 0, 0x10022402),
// StrPtr_assign(entry, getField204(desc)) through 0x10001334 (0x10022412-0x1002241a); for i from 0 while i < 0x80 (signed `jl`
// 0x1002243f), a non-NULL desc[+4 + 4i] sub-descriptor s has its first dword read (0x10022429), goes to ChannelTable_loadRange
// through 0x10001fa0 (0x1002242e) and i becomes that dword + 1 (0x10022433), a NULL one advances i by 1 (0x10022438);
// ChannelTable_callSlot4(entry, desc[+0x218]) through 0x10002455 (0x1002244a); bit 0 of desc[+0x208] calls
// Seq_setPlayRange(entry, desc[+0x210], desc[+0x20c]) through 0x10001f32 (0x10022456-0x10022468); bit 3 of the byte re-read
// sets bit 3 of entry+0x214 (0x1002246d-0x1002247c); a node {0, 0, entry} from operator new(0xc) is appended at the tail
// 0x100b4fec (0x10022482-0x100224c0) and the count 0x100b4ff4 incremented (0x100224c5-0x100224cb). Then each of the 0x80 channel
// records this+0x744 + 4k (0x100224d0-0x10022644) that is non-NULL with a non-NULL alt name +0x24 (0x100224ec-0x100224fd) and
// whose alt name equals *obj (0x10022503-0x10022536): with +0x30 NULL, +0x30 = obj and obj+0x210 += 1 (0x10022623-0x1002262a);
// with +0x30 naming the same string nothing changes (0x10022578 -> 0x1002261b); otherwise the old object's +0x210 is
// decremented (0x1002257e-0x10022585), reaching 0 its node is unlinked from the list and freed with the count decremented
// (0x10022595-0x100225fa) and the old object goes to Seq_freeChannelObjects (0x10001d66) and operator delete
// (0x100225ff-0x1002260e); in both cases +0x30 = 0 (0x10022616), so the record is not pointed at obj. 0 is returned (0x10022651).
typedef unsigned(__fastcall* Lookup_t)(void*, void*, char*);
Lookup_t Lookup_orig;
unsigned __fastcall Lookup_re(void* self, void*, char* desc) {
    if (!desc) return 0xa;                                                     // 0x1002233d-0x10022359
    void* cur = c3fk_gref(kDescHead);                                          // 0x1002235c
    c3fk_gref(kDescCursor) = cur;                                              // 0x10022366
    void* obj = 0;
    void* p = cur ? fat<void*>(cur, 8) : 0;                                    // 0x1002236b-0x10022372
    while (p) {
        const char* nm = reinterpret_cast<const char*>(c3fk_fn<Th0_t>(0x000025f4)(desc, 0));   // 0x10022379
        if (strcmp(fat<const char*>(p, 0), nm) == 0) { obj = p; break; }       // 0x10022374-0x100223ad, 0x100223d1
        void* c = c3fk_gref(kDescCursor);                                      // 0x100223af
        if (!c) break;                                                         // 0x100223b6
        c = fat<void*>(c, 4);                                                  // 0x100223b8
        c3fk_gref(kDescCursor) = c;                                            // 0x100223bd
        if (!c) break;                                                         // 0x100223c2
        p = fat<void*>(c, 8);                                                  // 0x100223c4-0x100223c9
    }
    if (!obj) {
        void* e = c3fk_new(0x21c);                                             // 0x100223df
        void* ent = e ? reinterpret_cast<void*>(c3fk_fn<Th0_t>(0x00001929)(e, 0)) : 0;   // 0x100223eb-0x10022402
        obj = ent;                                                             // 0x1002240e
        c3fk_fn<Th1_t>(0x00001334)(ent, 0, c3fk_fn<Th0_t>(0x000025f4)(desc, 0));   // 0x10022412-0x1002241a
        for (int i = 0; i < 0x80;) {                                           // 0x1002241f / 0x10022439 / 0x1002243f
            void* s = fat<void*>(desc, 4 + 4 * i);                             // 0x10022421
            if (s) {
                int first = fat<int>(s, 0);                                    // 0x10022429
                c3fk_fn<Th1_t>(0x00001fa0)(ent, 0, reinterpret_cast<unsigned>(s));   // 0x1002242e
                i = first + 1;                                                 // 0x10022433
            } else {
                ++i;                                                           // 0x10022438
            }
        }
        c3fk_fn<Th1_t>(0x00002455)(ent, 0, fat<unsigned>(desc, 0x218));        // 0x10022441-0x1002244a
        if (fat<unsigned char>(desc, 0x208) & 1)                               // 0x1002244f / 0x10022456
            c3fk_fn<Th2_t>(0x00001f32)(ent, 0, fat<unsigned>(desc, 0x210), fat<unsigned>(desc, 0x20c));   // 0x10022458-0x10022468
        if (fat<unsigned char>(desc, 0x208) & 8) fat<unsigned>(ent, 0x214) |= 8;   // 0x1002246d-0x1002247c
        void* n = c3fk_new(0xc);                                               // 0x10022484
        if (n) {                                                               // 0x1002248c / 0x1002248e
            fat<unsigned>(n, 0) = 0;
            fat<unsigned>(n, 4) = 0;
            fat<void*>(n, 8) = ent;                                            // 0x10022490-0x1002249d
        }
        if (void* t = c3fk_gref(kDescTail)) {                                  // 0x100224a4-0x100224ac
            fat<void*>(t, 4) = n;                                              // 0x100224ae
            fat<void*>(n, 0) = c3fk_gref(kDescTail);                           // 0x100224b1 / 0x100224b7
        } else {
            c3fk_gref(kDescHead) = n;                                          // 0x100224bb
        }
        c3fk_gref(kDescTail) = n;                                              // 0x100224c0
        *reinterpret_cast<int*>(c3fk_sound(kDescCount)) += 1;                  // 0x100224c5-0x100224cb
    }
    for (int k = 0; k < 0x80; ++k) {                                           // 0x100224d4 / 0x10022644
        void* rec = fat<void*>(self, 0x744 + 4 * k);                          // 0x100224e6-0x100224ea
        if (!rec) continue;                                                    // 0x100224f2
        const char* alt = fat<const char*>(rec, 0x24);                         // 0x100224f8
        if (!alt) continue;                                                    // 0x100224fd
        const char* on = fat<const char*>(obj, 0);                             // 0x10022503 / 0x10022507
        if (strcmp(on, alt) != 0) continue;                                    // 0x10022509-0x10022536
        void* old = fat<void*>(rec, 0x30);                                     // 0x1002253c
        if (!old) {                                                            // 0x10022543
            fat<void*>(rec, 0x30) = obj;                                       // 0x10022627
            fat<unsigned>(obj, 0x210) += 1;                                    // 0x1002262a
            continue;
        }
        if (strcmp(fat<const char*>(old, 0), on) == 0) continue;               // 0x10022549-0x10022578, 0x1002261b-0x10022621
        if (--fat<unsigned>(old, 0x210) == 0) {                                // 0x1002257e-0x1002258f
            void* n = c3fk_gref(kDescHead);                                    // 0x10022595
            while (n && fat<void*>(n, 8) != fat<void*>(rec, 0x30)) n = fat<void*>(n, 4);   // 0x1002259d-0x100225ad
            if (n) {                                                           // 0x100225af / 0x100225b1
                void* prev = fat<void*>(n, 0);                                 // 0x100225b3
                if (prev) fat<void*>(prev, 4) = fat<void*>(n, 4);              // 0x100225b9 / 0x100225bc
                else c3fk_gref(kDescHead) = fat<void*>(n, 4);                  // 0x100225c1 / 0x100225c4
                void* next = fat<void*>(n, 4);                                 // 0x100225ca
                if (next) {                                                    // 0x100225cd / 0x100225cf
                    fat<void*>(next, 0) = fat<void*>(n, 0);                    // 0x100225d1 / 0x100225d3
                    c3fk_gref(kDescCursor) = next;                             // 0x100225d5
                } else {
                    c3fk_gref(kDescCursor) = 0;                                // 0x100225df
                    c3fk_gref(kDescTail) = fat<void*>(n, 0);                   // 0x100225dd / 0x100225e5
                }
                c3fk_delete(n);                                                // 0x100225ec
                *reinterpret_cast<int*>(c3fk_sound(kDescCount)) -= 1;          // 0x100225f1-0x100225fa
            }
            if (void* o = fat<void*>(rec, 0x30)) {                             // 0x100225ff-0x10022604
                c3fk_fn<Th0_t>(0x00001d66)(o, 0);                              // 0x10022608
                c3fk_delete(o);                                                // 0x1002260e
            }
        }
        fat<void*>(rec, 0x30) = 0;                                             // 0x10022616
    }
    return 0;                                                                  // 0x10022651
}

// =====================================================================================================================
// C. Rows c3fd left for x87 that compute nothing in floating point themselves, or only one division
// =====================================================================================================================

// nextRandomFloat 0x100285a0 (thunk 0x1000114f, ecx = the seed) leaves a float in st(0) and __ftol 0x10042550 converts
// st(0) to an integer in eax (`fistp qword` under a truncating control word, 0x10042569); the pair is called back to back
// with no instruction between them (0x10023d42 / 0x10023d47), so it is reproduced as the same two calls.
unsigned c3fk_randomInt(void* seed) {
    void* nrf = c3fk_sound(0x0000114f);
    void* ftol = c3fk_sound(0x00042550);
    unsigned r;
    __asm {
        mov ecx, seed
        call nrf
        call ftol
        mov r, eax
    }
    return r;
}

// __ftol 0x10042550 of a double: loaded with `fld` and converted by the original routine.
unsigned c3fk_ftol(double v) {
    void* ftol = c3fk_sound(0x00042550);
    unsigned r;
    __asm {
        fld v
        call ftol
        mov r, eax
    }
    return r;
}

typedef unsigned(__stdcall* TimeGetTime_t)();
const unsigned kIatTimeGetTime = 0x000b8654;  // WINMM!timeGetTime [0x100b8654]
const unsigned kPlaySeed = 0x000b4fe0;        // the play-order random seed

// 0x00023d00  Seq_buildPlayOrder (VA 0x10023d00). __thiscall (out), `ret 4`. Fills out[0 .. count-1] with track indices,
// count = this+0x23c read again after every entry (signed `jl` 0x10023d6a / 0x10023db4), within lo = this+0x248 .. hi =
// this+0x24c. When (this+0x214 & 0x180) == 0x100 (0x10023d05-0x10023d15) the entries run lo, lo + 1, ... and wrap to lo
// after hi (unsigned `jbe` 0x10023da0; the bound re-read each step 0x10023d97); eax ends as the next index (lo with
// count <= 0, 0x10023d83). Otherwise the seed 0x100b4fe0 = timeGetTime() (0x10023d17 / 0x10023d1d) and, for count > 0
// (`jle` 0x10023d2c), each entry is __ftol(nextRandomFloat(&seed)) mod (hi - lo + 1) + lo (unsigned `div` 0x10023d5a, lo and
// hi re-read each step 0x10023d37 / 0x10023d4c); eax ends as the count re-read last (0x10023d62, or the count <= 0 read at
// 0x10023d22). Both paths store 0 at +0x244 (0x10023d6d, 0x10023db6, 0x10023dc2). Its only caller Seq_setPlayOrderMode
// 0x10022750 discards eax (`xor eax, eax` 0x10022787); it is compared anyway.
typedef unsigned(__fastcall* PlayOrder_t)(void*, void*, unsigned*);
PlayOrder_t PlayOrder_orig;
unsigned __fastcall PlayOrder_re(void* self, void*, unsigned* out) {
    if ((fat<unsigned>(self, 0x214) & 0x180) == 0x100) {                       // 0x10023d05-0x10023d15
        unsigned idx = fat<unsigned>(self, 0x248);                             // 0x10023d83
        for (int k = 0; k < fat<int>(self, 0x23c); ++k) {                      // 0x10023d8d / 0x10023da8-0x10023db4
            out[k] = idx;                                                      // 0x10023d95
            ++idx;                                                             // 0x10023d9d
            if (idx > fat<unsigned>(self, 0x24c)) idx = fat<unsigned>(self, 0x248);   // 0x10023d97-0x10023da2
        }
        fat<unsigned>(self, 0x244) = 0;                                        // 0x10023db6
        return idx;
    }
    unsigned* seed = reinterpret_cast<unsigned*>(c3fk_sound(kPlaySeed));
    *seed = c3fk_imp<TimeGetTime_t>(kIatTimeGetTime)();                        // 0x10023d17 / 0x10023d1d
    int count = fat<int>(self, 0x23c);                                         // 0x10023d22
    if (count <= 0) {                                                          // 0x10023d2a / 0x10023d2c
        fat<unsigned>(self, 0x244) = 0;                                        // 0x10023dc2
        return static_cast<unsigned>(count);
    }
    for (int k = 0; k < count; count = fat<int>(self, 0x23c)) {                // 0x10023d5e-0x10023d6a
        unsigned lo = fat<unsigned>(self, 0x248);                              // 0x10023d37
        unsigned r = c3fk_randomInt(seed);                                     // 0x10023d3d-0x10023d47
        unsigned span = fat<unsigned>(self, 0x24c) - lo + 1;                   // 0x10023d4c-0x10023d59
        out[k] = r % span + lo;                                                // 0x10023d5a-0x10023d5f
        ++k;
    }
    fat<unsigned>(self, 0x244) = 0;                                            // 0x10023d6d
    return static_cast<unsigned>(count);
}

// 0x00021bf0  Midi_Device::handleMetaEvent (VA 0x10021bf0). __thiscall (p, track), `ret 8`; p points at the meta type byte.
// len = Midi_readVarLen(p + 1, &n) (cdecl, thunk 0x1000139d, 0x10021c0a; n = the bytes of the length, written over p's
// argument slot) and data = p + 1 + n (0x10021c22). The type selects through the index table 0x10021dfc (type - 0x2f, `ja`
// 0x10021c26 for anything above 0x59) and the jump table 0x10021de8: 0x2f end of track (0x10021c9b), 0x51 tempo
// (0x10021c3b), 0x59 (0x10021c7e), every other type including 0x58 nothing (0x10021dd2). The result is n + len + 1
// (`lea eax, [eax + ecx + 1]` 0x10021ddd).
//  - 0x51: t = data[0] << 16 | data[1] << 8 | data[2] (0x10021c3b-0x10021c51) is loaded as a 64-bit integer (`fild qword`
//    0x10021c5f), 60000000.0f (the float at 0x1005b82c) is divided by it (`fdivr` 0x10021c65), __ftol converts (0x10021c6b)
//    and vtable slot +0x84 runs with that value (0x10021c73). The division is written in double: the A/B thread runs with
//    the x87 control word 0x027f (53-bit precision, nearest), where `fdivr` rounds to the same double as an IEEE division;
//    __ftol is the original routine.
//  - 0x59: this+0x73c = byteSwap16(the word at p + 2) & 0xffff (thunk 0x10001b2c, 0x10021c7e-0x10021c90).
//  - 0x2f: without bit 4 of track+0x38 (0x10021c9f / 0x10021ca3): track+0x34 |= 2 (0x10021da3-0x10021db1), the dword +0x34
//    of a non-NULL track+0x458 is decremented (0x10021db4 / 0x10021db6) and, when bit 1 of track+0x38 (re-read 0x10021db9) is
//    set, Seq_allocTrackSlot(this) through 0x100024cd if bit 2 is set too (0x10021dc0-0x10021dc6) and Seq_rewindTracks(track)
//    through 0x10001ca3 (0x10021dcd). With bit 4: Seq_rewindTracks(track) (0x10021cab), Seq_stepTrackMeta(this, track) through
//    0x100011cc (0x10021cb3), track+0x50 = this+0x71c (0x10021cc0), r = slot +0x9c(this+0x738) (0x10021ccc); for a non-NULL
//    r with r+0x14 non-zero (0x10021cd6 / 0x10021cdf): r+0x24 += 1 (0x10021ceb) and, when it reaches getSpanLength(r) (thunk
//    0x1000255e, unsigned `jb` 0x10021cf6), r+0x24 = 0 and r+0x28 += 1 (0x10021cfc-0x10021d08); when that reaches r+0x14
//    (`jb` 0x10021d0d): r+0x28 = 0, this+0x738 = r+0x18 (0x10021d13-0x10021d19) and, with this+0x224 non-zero (0x10021d27),
//    the list this+0x218 is walked with the cursor this+0x220 (0x10021d2d-0x10021d77) for a payload whose +4 equals
//    this+0x738 (re-read each step 0x10021d4f); a NULL head, cursor or payload ends it. The found payload's non-zero +0x1c goes
//    to slot +0x84 (0x10021d7d-0x10021d89) and slot +0x94 runs with (+8, +0xc) (0x10021d8f-0x10021d9b).
typedef unsigned(__cdecl* ReadVarLen_t)(const unsigned char*, unsigned*);
typedef unsigned(__cdecl* ByteSwap16_t)(unsigned);
typedef unsigned(__fastcall* Th1p_t)(void*, void*, void*);
typedef unsigned(__fastcall* Th2u_t)(void*, void*, unsigned, unsigned);
typedef unsigned(__fastcall* MetaEv_t)(void*, void*, unsigned char*, void*);
MetaEv_t MetaEv_orig;
unsigned __fastcall MetaEv_re(void* self, void*, unsigned char* p, void* track) {
    unsigned type = p[0];                                                      // 0x10021c03
    unsigned n;
    unsigned len = c3fk_fn<ReadVarLen_t>(0x0000139d)(p + 1, &n);               // 0x10021bfd-0x10021c0a
    unsigned char* data = p + n + 1;                                           // 0x10021c0f / 0x10021c22
    switch (type) {                                                            // 0x10021c17-0x10021c34
    case 0x51: {
        unsigned t = 0;
        for (int i = 2, sh = 0; i >= 0; --i, sh += 8) t |= static_cast<unsigned>(data[i]) << sh;   // 0x10021c3b-0x10021c51
        double q = static_cast<double>(*reinterpret_cast<float*>(c3fk_sound(0x0005b82c))) /
                   static_cast<double>(static_cast<long long>(t));            // 0x10021c5f / 0x10021c65
        unsigned bpm = c3fk_ftol(q);                                           // 0x10021c6b
        c3fk_vfn<Th1_t>(self, 0x84)(self, 0, bpm);                             // 0x10021c70-0x10021c73
        break;
    }
    case 0x59:
        fat<unsigned>(self, 0x73c) = c3fk_fn<ByteSwap16_t>(0x00001b2c)(*reinterpret_cast<unsigned short*>(p + 2)) & 0xffff;   // 0x10021c7e-0x10021c90
        break;
    case 0x2f:
        if (!(fat<unsigned char>(track, 0x38) & 0x10)) {                       // 0x10021c9f / 0x10021ca3
            fat<unsigned>(track, 0x34) |= 2;                                   // 0x10021da3-0x10021db1
            if (void* t2 = fat<void*>(track, 0x458)) fat<unsigned>(t2, 0x34) -= 1;   // 0x10021da6 / 0x10021db4-0x10021db6
            unsigned char f = fat<unsigned char>(track, 0x38);                 // 0x10021db9
            if (f & 2) {                                                       // 0x10021dbc / 0x10021dbe
                if (f & 4) c3fk_fn<Th0_t>(0x000024cd)(self, 0);                // 0x10021dc0-0x10021dc6
                c3fk_fn<Th0_t>(0x00001ca3)(track, 0);                          // 0x10021dcb / 0x10021dcd
            }
            break;
        }
        c3fk_fn<Th0_t>(0x00001ca3)(track, 0);                                  // 0x10021ca9 / 0x10021cab
        c3fk_fn<Th1p_t>(0x000011cc)(self, 0, track);                           // 0x10021cb0-0x10021cb3
        fat<unsigned>(track, 0x50) = fat<unsigned>(self, 0x71c);               // 0x10021cb8 / 0x10021cc0
        {
            void* r = reinterpret_cast<void*>(
                c3fk_vfn<Th1_t>(self, 0x9c)(self, 0, fat<unsigned>(self, 0x738)));   // 0x10021cc3-0x10021ccc
            if (!r || !fat<unsigned>(r, 0x14)) break;                          // 0x10021cd4-0x10021cdf
            fat<unsigned>(r, 0x24) += 1;                                       // 0x10021ce5-0x10021ceb
            if (fat<unsigned>(r, 0x24) < c3fk_fn<Th0_t>(0x0000255e)(r, 0)) break;   // 0x10021cee-0x10021cf6
            fat<unsigned>(r, 0x24) = 0;                                        // 0x10021d03
            fat<unsigned>(r, 0x28) += 1;                                       // 0x10021cfc-0x10021d08
            if (fat<unsigned>(r, 0x28) < fat<unsigned>(r, 0x14)) break;        // 0x10021cff / 0x10021d0b / 0x10021d0d
            fat<unsigned>(r, 0x28) = 0;                                        // 0x10021d16
            fat<unsigned>(self, 0x738) = fat<unsigned>(r, 0x18);               // 0x10021d13 / 0x10021d19
            if (!fat<unsigned>(self, 0x224)) break;                            // 0x10021d1f-0x10021d27
            void* cur = fat<void*>(self, 0x218);                               // 0x10021d2d
            fat<void*>(self, 0x220) = cur;                                     // 0x10021d35
            if (!cur) break;                                                   // 0x10021d3b
            void* pl = fat<void*>(cur, 8);                                     // 0x10021d41
            if (!pl) break;                                                    // 0x10021d46
            while (fat<unsigned>(pl, 4) != fat<unsigned>(self, 0x738)) {       // 0x10021d4c-0x10021d57
                cur = fat<void*>(self, 0x220);                                 // 0x10021d59
                if (!cur) return n + len + 1;                                  // 0x10021d61
                cur = fat<void*>(cur, 4);                                      // 0x10021d63
                fat<void*>(self, 0x220) = cur;                                 // 0x10021d68
                if (!cur) return n + len + 1;                                  // 0x10021d6e
                pl = fat<void*>(cur, 8);                                       // 0x10021d70
                if (!pl) return n + len + 1;                                   // 0x10021d75
            }
            if (unsigned v = fat<unsigned>(pl, 0x1c)) c3fk_vfn<Th1_t>(self, 0x84)(self, 0, v);   // 0x10021d7d-0x10021d89
            c3fk_vfn<Th2u_t>(self, 0x94)(self, 0, fat<unsigned>(pl, 8), fat<unsigned>(pl, 0xc));   // 0x10021d8f-0x10021d9b
        }
        break;
    default:                                                                   // 0x58 and every other type: 0x10021dd2
        break;
    }
    return n + len + 1;                                                        // 0x10021dd2-0x10021ddd
}

// =====================================================================================================================
// D. DirectSoundDevice bodies over the module's DsBuffer list
// =====================================================================================================================

const unsigned kDsHead = 0x000b4a34;          // DsBuffer list: head (prev +0xbc, next +0xc0 in each buffer)
const unsigned kDsTail = 0x000b4a38;          //   tail
const unsigned kDsDeviceIndex = 0x000b4a40;   // the device index enumerate selects

// 0x000114e0  DirectSoundDevice::enumerate (VA 0x100114e0). __thiscall (flags, ctx), `ret 8`. A device already set up
// (+0, +4 or +8 non-zero, 0x100114e3-0x100114fc) returns 0xc (0x100115b7); a NULL ctx returns 0xa (0x10011506-0x1001150a).
// +0x164 = ctx (0x10011517); flag bit 0x20 sets bit 3 of the byte +0x6c and, with bit 0x40 too, bit 4 (0x1001151d-0x10011531).
// DirectSoundEnumerateA(0x100017a3, this) runs through the import thunk 0x1003a978 (0x10011534-0x1001153a); a non-zero result
// goes to mapDsError through 0x1000268a (0x10011544, result unused) and 0xa is returned (0x1001154c). Then n = +0x148
// (0x10011555): n = 0 returns 3 (0x10011567); the index i = [0x100b4a40] at or above n (unsigned `jb` 0x10011572) returns 0xa;
// a non-zero +8 returns 6 (0x10011583 / 0x10011586). Otherwise +8 walks from the list head +4 along +0x10 (0x1001158f-0x100115a5)
// to entry i (stopping at entry n - 1 when i is not reached first), bit 0 of +0x6c is set (0x100115ac / 0x100115b0) and 0 is
// returned.
typedef unsigned(__stdcall* DsEnum_t)(void*, void*);
typedef unsigned(__cdecl* Cdecl1u_t)(unsigned);
typedef unsigned(__fastcall* DsDevEnum_t)(void*, void*, unsigned, void*);
DsDevEnum_t DsEnum_orig;
unsigned __fastcall DsEnum_re(void* self, void*, unsigned flags, void* ctx) {
    if (fat<unsigned>(self, 0) || fat<unsigned>(self, 4) || fat<unsigned>(self, 8)) return 0xc;   // 0x100114e3-0x100115b7
    if (!ctx) return 0xa;                                                      // 0x10011502-0x1001150a
    fat<void*>(self, 0x164) = ctx;                                             // 0x10011517
    if (flags & 0x20) {                                                        // 0x10011513 / 0x1001151d
        fat<unsigned char>(self, 0x6c) |= 8;                                   // 0x10011522-0x1001152a
        if (flags & 0x40) fat<unsigned char>(self, 0x6c) |= 0x10;              // 0x10011527 / 0x1001152f
    }
    unsigned hr = c3fk_fn<DsEnum_t>(0x0003a978)(c3fk_sound(0x000017a3), self); // 0x10011534-0x1001153a
    if (hr) {                                                                  // 0x1001153f / 0x10011541
        c3fk_fn<Cdecl1u_t>(0x0000268a)(hr);                                    // 0x10011543 / 0x10011544
        return 0xa;                                                            // 0x1001154c
    }
    unsigned n = fat<unsigned>(self, 0x148);                                   // 0x10011555
    unsigned want = *reinterpret_cast<unsigned*>(c3fk_sound(kDsDeviceIndex)); // 0x1001155c
    if (!n) return 3;                                                          // 0x10011562-0x10011567
    if (want >= n) return 0xa;                                                 // 0x10011570-0x10011575
    if (fat<unsigned>(self, 8)) return 6;                                      // 0x1001157e-0x10011586
    void* e = fat<void*>(self, 4);                                             // 0x1001158f
    for (unsigned k = 0; k < n; ++k) {                                         // 0x10011592-0x100115a5
        fat<void*>(self, 8) = e;                                               // 0x1001159a
        if (k == want) break;                                                  // 0x10011598 / 0x1001159d
        e = fat<void*>(e, 0x10);                                               // 0x1001159f
    }
    fat<unsigned char>(self, 0x6c) |= 1;                                       // 0x100115a7-0x100115b0
    return 0;                                                                  // 0x100115aa
}

// Unlink a buffer from the DsBuffer list (prev +0xbc, next +0xc0; head 0x100b4a34, tail 0x100b4a38) and clear its links, as
// DirectSoundDevice::stopAll does at 0x10011ab6-0x10011aeb.
void c3fk_dsUnlink(void* b) {
    void* prev = fat<void*>(b, 0xbc);
    void* next = fat<void*>(b, 0xc0);
    if (prev) fat<void*>(prev, 0xc0) = next;
    else c3fk_gref(kDsHead) = next;
    if (next) fat<void*>(next, 0xbc) = prev;
    else c3fk_gref(kDsTail) = prev;
    fat<void*>(b, 0xbc) = 0;
    fat<void*>(b, 0xc0) = 0;
}

typedef unsigned(__stdcall* ComRelease_t)(void*);

// 0x00011a80  DirectSoundDevice::stopAll (VA 0x10011a80). __thiscall, no stack argument, plain `ret` (0x10011bfd). While the
// DsBuffer list head 0x100b4a34 is non-NULL (re-read 0x10011b0c-0x10011b14): a buffer with a channel at +0x20 (0x10011a95 /
// 0x10011a9a) has the channel appended to this+0x178 with appendNode through 0x10001681 (0x10011a9c-0x10011aa3) and passed to
// Channel_releaseBuffers through 0x10001d11 (0x10011aab), which takes the buffer out of the list; a buffer without one is
// unlinked here (0x10011ab6-0x10011aeb), released by DsBuffer::release through 0x1000144c (0x10011af3), destroyed by
// DsBuffer::destroy through 0x10001910 (0x10011afe) and freed (0x10011b04). The list {+0x168 head, +0x16c tail, +0x174 count}
// is popped freeing only its nodes until a NULL payload (0x10011b1a-0x10011ba0). Bit 1 of +0x6c (0x10011ba2) runs
// DsBuffer::releaseIfOwned on this+0x70 through 0x100012d5 (0x10011bab); a non-NULL +0x198 gets releaseIfOwned (0x10011bba) and,
// when still non-NULL after it (re-read 0x10011bbf), destroy and operator delete (0x10011bcb-0x10011bd1), and +0x198 = 0
// (0x10011bd9). A non-NULL interface at +0 gets its vtable slot +8 (Release, 0x10011be5-0x10011be8). +0 = 0 (0x10011bf1) and
// +0x18c += 1 (0x10011beb-0x10011bf4); eax is the new +0x18c.
Th0_t DsStopAll_orig;
unsigned __fastcall DsStopAll_re(void* self, void*) {
    while (void* b = c3fk_gref(kDsHead)) {                                     // 0x10011a82-0x10011a8f, 0x10011b0c-0x10011b14
        if (void* ch = fat<void*>(b, 0x20)) {                                  // 0x10011a95-0x10011a9a
            c3fk_fn<Th1p_t>(0x00001681)(&fat<char>(self, 0x178), 0, ch);       // 0x10011a9c-0x10011aa3
            c3fk_fn<Th0_t>(0x00001d11)(fat<void*>(b, 0x20), 0);                // 0x10011aa8 / 0x10011aab
            continue;
        }
        c3fk_dsUnlink(b);                                                      // 0x10011ab6-0x10011aeb
        c3fk_fn<Th0_t>(0x0000144c)(b, 0);                                      // 0x10011af1 / 0x10011af3
        c3fk_fn<Th0_t>(0x00001910)(b, 0);                                      // 0x10011afc / 0x10011afe
        c3fk_delete(b);                                                        // 0x10011b04
    }
    c3fk_drainNodes(self, 0x168, 0x16c, 0x174, false);                         // 0x10011b1a-0x10011ba0
    if (fat<unsigned char>(self, 0x6c) & 2)                                   // 0x10011ba2 / 0x10011ba6
        c3fk_fn<Th0_t>(0x000012d5)(&fat<char>(self, 0x70), 0);                 // 0x10011ba8 / 0x10011bab
    if (void* x = fat<void*>(self, 0x198)) {                                   // 0x10011bb0-0x10011bb8
        c3fk_fn<Th0_t>(0x000012d5)(x, 0);                                      // 0x10011bba
        if (void* y = fat<void*>(self, 0x198)) {                               // 0x10011bbf-0x10011bc7
            c3fk_fn<Th0_t>(0x00001910)(y, 0);                                  // 0x10011bc9 / 0x10011bcb
            c3fk_delete(y);                                                    // 0x10011bd1
        }
        fat<void*>(self, 0x198) = 0;                                           // 0x10011bd9
    }
    if (void* itf = fat<void*>(self, 0))                                       // 0x10011bdf-0x10011be3
        c3fk_vfn<ComRelease_t>(itf, 8)(itf);                                   // 0x10011be5-0x10011be8
    fat<void*>(self, 0) = 0;                                                   // 0x10011bf1
    return ++fat<unsigned>(self, 0x18c);                                       // 0x10011beb-0x10011bf4
}

// 0x00011290  DirectSoundDevice::close (VA 0x10011290). __thiscall, no stack argument, plain `ret` (0x10011456). The
// enumerated-device list at +4 (next pointer +0x10) is freed node by node (0x100112af-0x10011307): its three non-NULL strings
// +0, +4, +8 are freed and cleared (0x100112c0-0x100112f4), then the node (0x100112fd). +4 = +8 = 0 (0x1001130b / 0x1001130e),
// DirectSoundDevice::releaseAll through 0x1000104b (0x10011311) and CoUninitialize through its IAT slot [0x100b86c0]
// (0x10011316). The lists {+0x178, +0x17c, +0x184} and {+0x168, +0x16c, +0x174} are popped freeing only their nodes until a NULL
// payload (0x1001131c-0x100113a7, 0x100113a9-0x10011433), and DsBuffer::destroy runs on this+0x70 through 0x10001910
// (0x10011440); its eax is what returns.
typedef unsigned(__stdcall* CoUninit_t)();
Th0_t DsClose_orig;
unsigned __fastcall DsClose_re(void* self, void*) {
    for (void* e = fat<void*>(self, 4); e;) {                                  // 0x100112af-0x100112be, 0x10011305
        for (int off = 0; off <= 8; off += 4) {                                // 0x100112c0-0x100112f4
            if (void* s = fat<void*>(e, off)) {
                c3fk_delete(s);
                fat<void*>(e, off) = 0;
            }
        }
        void* next = fat<void*>(e, 0x10);                                      // 0x100112f9
        c3fk_delete(e);                                                        // 0x100112fd
        e = next;
    }
    fat<void*>(self, 4) = 0;                                                   // 0x1001130b
    fat<void*>(self, 8) = 0;                                                   // 0x1001130e
    c3fk_fn<Th0_t>(0x0000104b)(self, 0);                                       // 0x10011309 / 0x10011311
    c3fk_imp<CoUninit_t>(0x000b86c0)();                                        // 0x10011316
    c3fk_drainNodes(self, 0x178, 0x17c, 0x184, false);                         // 0x1001131c-0x100113a7
    c3fk_drainNodes(self, 0x168, 0x16c, 0x174, false);                         // 0x100113a9-0x10011433
    return c3fk_fn<Th0_t>(0x00001910)(&fat<char>(self, 0x70), 0);              // 0x10011435-0x10011440
}

// 0x00012260  DirectSoundDevice::duplicateBuffer (VA 0x10012260). __thiscall (chan, src, out), `ret 0xc`. Without bit 1 of
// +0x6c or without the IDirectSound interface +0 (0x1001227a-0x1001228a) it returns 3 (0x10012423); with +0x15c equal to the
// limit +0x150 (0x10012290-0x1001229e) 0x25. A new 0xd4-byte DsBuffer from operator new (0x100122bd) is constructed by
// DsBuffer::ctor through 0x10001c1c (0x100122d3; NULL leaves 0) and stored in *out (0x100122e9, no NULL test); its +0x5c =
// this (0x100122eb). The interface's slot +0x14 (DuplicateSoundBuffer) runs with (src+0x60, &new+0x60) (0x100122ee-0x10012301).
// A non-zero result goes to mapDsError through 0x1000268a (0x100123e5), a non-NULL *out is destroyed through 0x10001910 and freed
// (0x100123f3-0x100123fb), *out = 0 and 0x1d is returned (0x10012403-0x10012407). Otherwise the buffer is appended to the
// DsBuffer list (0x1001230c-0x10012342: head and tail when the tail is NULL, else after the tail; its +0xc0 = 0), a node
// {0, 0, buffer} from operator new(0xc) (0x1001234c-0x1001235d, NULL leaves 0) is appended to the list {+0x168, +0x16c,
// +0x174 count} (0x10012364-0x10012393), buffer+0x20 = chan (0x1001239b), DsDevice_registerBuffer(buffer) runs on chan through
// 0x10001320 (0x100123a1) and, when DsBuffer::hasAuxInterfaces(src) through 0x10002081 returns a non-zero byte (0x100123aa /
// 0x100123b1), DsBuffer::query3D and DsBuffer::queryNotify run on the buffer (0x100123b5, 0x100123bc). +0x15c += 1 (0x100123c9)
// and 0 is returned (0x100123d0).
typedef unsigned(__stdcall* DsDup_t)(void*, void*, void*);
typedef unsigned(__fastcall* DupBuf_t)(void*, void*, void*, void*, void**);
DupBuf_t DupBuf_orig;
unsigned __fastcall DupBuf_re(void* self, void*, void* chan, void* src, void** out) {
    if (!(fat<unsigned char>(self, 0x6c) & 2) || !fat<void*>(self, 0)) return 3;   // 0x1001227a-0x1001228a, 0x1001241e
    if (fat<unsigned>(self, 0x15c) == fat<unsigned>(self, 0x150)) return 0x25;  // 0x10012290-0x100122a1
    void* p = c3fk_new(0xd4);                                                  // 0x100122bd
    void* b = p ? reinterpret_cast<void*>(c3fk_fn<Th0_t>(0x00001c1c)(p, 0)) : 0;   // 0x100122c9-0x100122da
    *out = b;                                                                  // 0x100122e9
    fat<void*>(b, 0x5c) = self;                                                // 0x100122eb
    void* ds = fat<void*>(self, 0);                                            // 0x100122f0
    unsigned hr = c3fk_vfn<DsDup_t>(ds, 0x14)(ds, fat<void*>(src, 0x60), &fat<char>(*out, 0x60));   // 0x100122ee-0x10012301
    if (hr) {                                                                  // 0x10012304 / 0x10012306
        c3fk_fn<Cdecl1u_t>(0x0000268a)(hr);                                    // 0x100123e4 / 0x100123e5
        if (void* x = *out) {                                                  // 0x100123ea-0x100123f1
            c3fk_fn<Th0_t>(0x00001910)(x, 0);                                  // 0x100123f5
            c3fk_delete(x);                                                    // 0x100123fb
        }
        *out = 0;                                                              // 0x10012403
        return 0x1d;                                                           // 0x10012407
    }
    b = *out;                                                                  // 0x10012312
    if (void* t = c3fk_gref(kDsTail)) {                                        // 0x1001230c-0x10012316
        fat<void*>(b, 0xbc) = t;                                               // 0x1001232a
        fat<void*>(c3fk_gref(kDsTail), 0xc0) = b;                              // 0x10012330 / 0x10012336
        c3fk_gref(kDsTail) = b;                                                // 0x1001233c
    } else {
        c3fk_gref(kDsHead) = b;                                                // 0x10012318
        c3fk_gref(kDsTail) = b;                                                // 0x1001231d
        fat<void*>(b, 0xbc) = 0;                                               // 0x10012322
    }
    fat<void*>(b, 0xc0) = 0;                                                   // 0x10012342
    c3fk_rawAppend(&fat<char>(self, 0x168), *out);                             // 0x10012348-0x10012393
    fat<void*>(*out, 0x20) = chan;                                             // 0x10012382 / 0x1001239b
    c3fk_fn<Th1p_t>(0x00001320)(chan, 0, *out);                                // 0x1001239e-0x100123a1
    if (static_cast<unsigned char>(c3fk_fn<Th0_t>(0x00002081)(src, 0))) {      // 0x100123a6-0x100123b1
        c3fk_fn<Th0_t>(0x00001c30)(*out, 0);                                   // 0x100123b3 / 0x100123b5
        c3fk_fn<Th0_t>(0x00001762)(*out, 0);                                   // 0x100123ba / 0x100123bc
    }
    fat<unsigned>(self, 0x15c) += 1;                                           // 0x100123c1-0x100123c9
    return 0;                                                                  // 0x100123d0
}

// 0x00012690  DirectSoundDevice::freeBufferById (VA 0x10012690). __thiscall (chan, out), `ret 8`. A NULL chan returns 0xa
// (0x10012697-0x1001269f). The list {+0x168 head, +0x16c tail, +0x170 cursor, +0x174 count} is walked for the first buffer
// DsBuffer::isBusy (thunk 0x10001b9f) reports idle (0x100126b2-0x100126c5); none returns 0x24 (0x100126c9). The buffer is taken
// off its channel by voiceListRemove(buffer) on buffer+0x20 through 0x10001ec4 (0x100126d6-0x100126da), its node is looked up
// again from the head (0x100126df-0x100126f3, not found: 0x24), unlinked (predecessor's +4 or the head; successor's +0 and the
// cursor, or cursor 0 and the tail = predecessor, 0x10012704-0x1001273a) and freed with the count decremented
// (0x10012744-0x10012755); a NULL payload returns 0x24 (0x1001275b). With bit 1 of +0x6c (0x10012761): the buffer is unlinked
// from the DsBuffer list when found there (0x1001276b-0x100127c2), +0x158 is decremented (`or edx, 4; je` 0x100127d2 / 0x100127d5
// never jumps, so always, 0x100127d7) and DsBuffer::releaseIfOwned through 0x100012d5 (0x100127df) returning 0 has the buffer
// destroyed and freed (0x100127e8-0x100127f0). Then +0x15c -= 1 (0x100127f8-0x10012805) and r = duplicateBuffer(chan,
// chan+0x54, out) on this through 0x10002572 (0x1001280f-0x10012815): non-zero clears *out and returns r (0x1001281e), zero sets
// (*out)+0x20 = chan and returns 0 (0x1001282e).
typedef unsigned(__fastcall* FreeById_t)(void*, void*, void*, void**);
typedef unsigned(__fastcall* Dup3_t)(void*, void*, void*, void*, void**);
FreeById_t FreeById_orig;
unsigned __fastcall FreeById_re(void* self, void*, void* chan, void** out) {
    if (!chan) return 0xa;                                                     // 0x10012697-0x100126a5
    void* n = fat<void*>(self, 0x168);                                         // 0x100126a8
    void* b = 0;
    for (; n; n = fat<void*>(n, 4)) {                                          // 0x100126ae-0x100126c5
        b = fat<void*>(n, 8);                                                  // 0x100126b2
        if (!c3fk_fn<Th0_t>(0x00001b9f)(b, 0)) break;                          // 0x100126b5-0x100126be
    }
    if (!n) return 0x24;                                                       // 0x100126c7 / 0x100126d2
    c3fk_fn<Th1p_t>(0x00001ec4)(fat<void*>(b, 0x20), 0, b);                    // 0x100126d6-0x100126da
    n = fat<void*>(self, 0x168);                                               // 0x100126df
    while (n && fat<void*>(n, 8) != b) n = fat<void*>(n, 4);                   // 0x100126e5-0x100126f3
    if (!n) return 0x24;                                                       // 0x100126e7 / 0x100126f5 / 0x10012700
    void* prev = fat<void*>(n, 0);                                             // 0x10012704
    if (prev) fat<void*>(prev, 4) = fat<void*>(n, 4);                          // 0x1001270a / 0x1001270d
    else fat<void*>(self, 0x168) = fat<void*>(n, 4);                           // 0x10012712 / 0x10012715
    if (void* next = fat<void*>(n, 4)) {                                       // 0x1001271b-0x10012720
        fat<void*>(next, 0) = fat<void*>(n, 0);                                // 0x10012722 / 0x10012724
        fat<void*>(self, 0x170) = next;                                        // 0x10012726
    } else {
        fat<void*>(self, 0x170) = 0;                                           // 0x10012730
        fat<void*>(self, 0x16c) = fat<void*>(n, 0);                            // 0x1001272e / 0x1001273a
    }
    void* pl = fat<void*>(n, 8);                                               // 0x10012740
    c3fk_delete(n);                                                            // 0x10012744
    fat<int>(self, 0x174) -= 1;                                                // 0x10012749-0x10012755
    if (!pl) return 0x24;                                                      // 0x10012753 / 0x1001275b
    if (fat<unsigned char>(self, 0x6c) & 2) {                                  // 0x10012761 / 0x10012765
        void* e = c3fk_gref(kDsHead);                                          // 0x1001276b
        while (e && e != pl) e = fat<void*>(e, 0xc0);                          // 0x10012770-0x10012780
        if (e) c3fk_dsUnlink(e);                                               // 0x10012784-0x100127c2
        fat<int>(self, 0x158) -= 1;                                            // 0x100127cc-0x100127d7
        if (!c3fk_fn<Th0_t>(0x000012d5)(pl, 0)) {                              // 0x100127dd-0x100127e6
            c3fk_fn<Th0_t>(0x00001910)(pl, 0);                                 // 0x100127e8 / 0x100127ea
            c3fk_delete(pl);                                                   // 0x100127f0
        }
    }
    fat<unsigned>(self, 0x15c) -= 1;                                           // 0x100127f8-0x10012805
    unsigned r = c3fk_fn<Dup3_t>(0x00002572)(self, 0, chan, fat<void*>(chan, 0x54), out);   // 0x1001280b-0x10012815
    if (r) {                                                                   // 0x1001281a / 0x1001281c
        *out = 0;                                                              // 0x1001281e
        return r;
    }
    fat<void*>(*out, 0x20) = chan;                                             // 0x1001282a-0x1001282e
    return 0;                                                                  // eax = 0 from the call (0x1001281a)
}

// The body DirectSoundDevice::createBuffer (0x10012d10) and createBufferFmt (0x10012fe0) share, after their own entry tests.
// `a` lists each instruction pair as (createBuffer / createBufferFmt). With flag bit 2 (`and eax, 4`, 0x10012d42 / 0x10013023):
// a +0x158 at or above +0x154 (unsigned `jb` 0x10012d5d / 0x10013041) returns 0x25; else +0x158 += 1 and +0x150 = +0x14c - +0x158
// (0x10012d69-0x10012d7e / 0x1001304d-0x10013062); when +0x15c is then above +0x150 (unsigned `jbe` 0x10012d86 / 0x1001306a) the
// first buffer of the list +0x168 that DsBuffer::isBusy (thunk 0x10001b9f) reports idle is evicted: voiceListRemove on its +0x20
// (thunk 0x10001ec4), findAndUnlinkNode(buffer) on this+0x168 (thunk 0x100015a0); no idle buffer, or a NULL result, undoes the
// increment (+0x158 -= 1, +0x150 = +0x14c - +0x158) and returns 0x25 (0x10012db1-0x10012dd3 / 0x10013095-0x100130b7). The evicted
// buffer, with bit 1 of +0x6c, leaves the DsBuffer list through unlinkDsBuffer (thunk 0x10001ebf) when found there, +0x158 is
// decremented (`or eax, 4; je` 0x10012e26 / 0x10013108 never jumps), releaseIfOwned (0x100012d5) returning 0 has it deleted by
// DsBuffer::scalarDeletingDtor(1) (thunk 0x10002252), and +0x15c -= 1 (0x10012e45 / 0x10013127). Then a 0xd4-byte block from
// operator new (0x10012e54 / 0x1001313b) built by DsBuffer::ctor (0x10001c1c; NULL leaves 0) goes to *out with +0x74 = flags (no
// NULL test), DsBuffer::setBufferSize(size) (0x10001c35), [createBufferFmt: DsBuffer::setPrimaryFormat(fmt), 0x10001d75],
// +0x5c = this, r = DsBuffer::create(this+0 interface, this+0x164) (0x10001bfe). r = 0 appends the buffer at the tail of the
// DsBuffer list (both links cleared when the list was empty) and returns 0; otherwise flag bit 2 undoes the +0x158 increment,
// a non-NULL *out is destroyed (0x10001910) and freed, *out = 0 and r is returned.
typedef unsigned(__fastcall* Th2p_t)(void*, void*, void*, void*);
unsigned c3fk_dsCreateCore(void* self, void** out, unsigned flags, unsigned size, void* fmt, bool withFmt) {
    unsigned bit2 = flags & 4;
    if (bit2) {
        if (!(fat<unsigned>(self, 0x158) < fat<unsigned>(self, 0x154))) return 0x25;
        fat<unsigned>(self, 0x158) += 1;
        fat<unsigned>(self, 0x150) = fat<unsigned>(self, 0x14c) - fat<unsigned>(self, 0x158);
        if (fat<unsigned>(self, 0x15c) > fat<unsigned>(self, 0x150)) {
            void* n = fat<void*>(self, 0x168);
            void* b = 0;
            for (; n; n = fat<void*>(n, 4)) {
                b = fat<void*>(n, 8);
                if (!c3fk_fn<Th0_t>(0x00001b9f)(b, 0)) break;
            }
            void* r = 0;
            if (n) {
                c3fk_fn<Th1p_t>(0x00001ec4)(fat<void*>(b, 0x20), 0, b);
                r = reinterpret_cast<void*>(c3fk_fn<Th1p_t>(0x000015a0)(&fat<char>(self, 0x168), 0, b));
            }
            if (!r) {
                fat<unsigned>(self, 0x158) -= 1;
                fat<unsigned>(self, 0x150) = fat<unsigned>(self, 0x14c) - fat<unsigned>(self, 0x158);
                return 0x25;
            }
            if (fat<unsigned char>(self, 0x6c) & 2) {
                void* e = c3fk_gref(kDsHead);
                while (e && e != r) e = fat<void*>(e, 0xc0);
                if (e) c3fk_fn<Th1p_t>(0x00001ebf)(self, 0, e);
                fat<int>(self, 0x158) -= 1;
                if (!c3fk_fn<Th0_t>(0x000012d5)(r, 0)) c3fk_fn<Th1_t>(0x00002252)(r, 0, 1);
            }
            fat<unsigned>(self, 0x15c) -= 1;
        }
    }
    void* p = c3fk_new(0xd4);
    void* b = p ? reinterpret_cast<void*>(c3fk_fn<Th0_t>(0x00001c1c)(p, 0)) : 0;
    *out = b;
    fat<unsigned>(b, 0x74) = flags;
    c3fk_fn<Th1_t>(0x00001c35)(*out, 0, size);
    if (withFmt) c3fk_fn<Th1p_t>(0x00001d75)(*out, 0, fmt);
    fat<void*>(*out, 0x5c) = self;
    unsigned r = c3fk_fn<Th2p_t>(0x00001bfe)(*out, 0, fat<void*>(self, 0), fat<void*>(self, 0x164));
    if (!r) {
        void* nb = *out;
        if (void* t = c3fk_gref(kDsTail)) {
            fat<void*>(nb, 0xbc) = t;
            fat<void*>(c3fk_gref(kDsTail), 0xc0) = nb;
            c3fk_gref(kDsTail) = nb;
            fat<void*>(nb, 0xc0) = 0;
        } else {
            c3fk_gref(kDsHead) = nb;
            c3fk_gref(kDsTail) = nb;
            fat<void*>(nb, 0xbc) = 0;
            fat<void*>(nb, 0xc0) = 0;
        }
        return 0;
    }
    if (bit2) {
        fat<unsigned>(self, 0x158) -= 1;
        fat<unsigned>(self, 0x150) = fat<unsigned>(self, 0x14c) - fat<unsigned>(self, 0x158);
    }
    if (void* x = *out) {
        c3fk_fn<Th0_t>(0x00001910)(x, 0);
        c3fk_delete(x);
    }
    *out = 0;
    return r;
}

// 0x00011c60  DirectSoundDevice::create (VA 0x10011c60). __thiscall (hwnd, level), `ret 8`. A NULL hwnd returns 0xa
// (0x10011c6d-0x10011c76), an interface already at +0 0xc (0x10011c82-0x10011c89), bit 0 of +0x6c clear (no device enumerated,
// 0x10011c95 / 0x10011c99) 3, a NULL selected entry +8 0x13 (0x10011cb0-0x10011cb8); a level of 0 becomes 2 (0x10011ca8-0x10011cac,
// written over its argument slot). CoInitialize(NULL) through [0x100b86b8] (0x10011cc5) returning neither 0 nor 1 clears +0 and
// returns 0x20 (0x10011ccb-0x10011cd9). +0x18c -= 1 (0x10011cf3 / 0x10011cfa) and CoCreateInstance(0x1005d67c, NULL, 1,
// 0x1005d5dc, this) through [0x100b86bc] (0x10011ceb-0x10011d00) writes the interface into +0. When it succeeds with a non-NULL
// interface (0x10011d08 / 0x10011d0e) its slot +0x28 (Initialize) runs with the entry's +8 (0x10011d10-0x10011d1a); a failure there
// or of slot +0x18 (SetCooperativeLevel, with hwnd and the level, 0x10011d21-0x10011d2c) goes to mapDsError (thunk 0x1000268a),
// DirectSoundDevice::releaseAll (thunk 0x1000104b) and returns 3 (0x10011d33-0x10011d46). Then the global 0x100b4a3c = hwnd
// (0x10011d52); +0xc = 0x60 and slot +0x10 (GetCaps) fills the caps at this+0xc (0x10011d5a-0x10011d67; a failure only goes to
// mapDsError, 0x10011d6f). Without bit 5 of +0x10 (0x10011d77 / 0x10011d7b): +0x158 = +0x15c = 0 and +0x14c = +0x154 = +0x150 =
// +0x30 (0x10011d7d-0x10011d9d), and bits 3 and 4 of +0x6c are cleared when +0x40 is below 2 (unsigned `jae` 0x10011da3) and bit
// 3 is set (0x10011da8 / 0x10011daa); with bit 5 they are cleared (0x10011dae-0x10011db3). Two PCM formats are built on the stack
// (0x10011db6-0x10011e11): mono 22050 Hz 16-bit {1, 1, 0x5622, 0xac44, 2, 0x10, 0} at esp+0x10 and stereo 44100 Hz 16-bit {1, 2,
// 0xac44, 0x2b110, 4, 0x10, 0} at esp+0x24. Bit 3 of +0x6c (0x10011ddc / 0x10011e19) passes the mono one to
// DsBuffer::setPrimaryFormat on the primary buffer this+0x70 (thunk 0x10001d75, 0x10011e22) with e = 0x11, otherwise the stereo
// one (0x10011e35) with e = 1. r = DsBuffer::configureDesc(1) (thunk 0x10001b4a, 0x10011e3e) and then r = DsBuffer::create(+0,
// +0x164) (0x10011e6e), after +0xe4 = e (0x10011e5c): a non-zero r runs releaseAll and is returned (0x10011e49-0x10011e52,
// 0x10011e7b-0x10011e82). Success sets bit 1 of +0x6c and +0xcc = this (0x10011e8e-0x10011e9a), pops the list {+0x178, +0x17c,
// +0x184} node by node until a NULL payload (0x10011e9d-0x10011f3c) running parseRiffWave(payload+0x64) on each payload through
// 0x10001e51 when +0x14c is non-zero (0x10011ee7-0x10011ef3), stores the level in +0x190 (0x10011f43) and returns 0.
struct C3fkWaveFmt { unsigned short tag, ch; unsigned rate, bytes; unsigned short align, bits, cb; };
typedef unsigned(__stdcall* CoInit_t)(void*);
typedef unsigned(__stdcall* CoCreate_t)(const void*, void*, unsigned, const void*, void*);
typedef unsigned(__stdcall* Com1_t)(void*, void*);
typedef unsigned(__stdcall* Com2_t)(void*, void*, unsigned);
typedef unsigned(__fastcall* DsDevCreate_t)(void*, void*, void*, unsigned);
DsDevCreate_t DsDevCreate_orig;
unsigned __fastcall DsDevCreate_re(void* self, void*, void* hwnd, unsigned level) {
    if (!hwnd) return 0xa;                                                     // 0x10011c6d-0x10011c76
    if (fat<void*>(self, 0)) return 0xc;                                       // 0x10011c82-0x10011c89
    if (!(fat<unsigned char>(self, 0x6c) & 1)) return 3;                       // 0x10011c95 / 0x10011c99 -> 0x10011d43
    if (!level) level = 2;                                                     // 0x10011c9f-0x10011cac
    if (!fat<void*>(self, 8)) return 0x13;                                     // 0x10011cb0-0x10011cb8
    unsigned ci = c3fk_imp<CoInit_t>(0x000b86b8)(0);                           // 0x10011cc4 / 0x10011cc5
    if (ci != 0 && ci != 1) {                                                  // 0x10011ccb-0x10011cd2
        fat<void*>(self, 0) = 0;                                               // 0x10011cd4
        return 0x20;                                                           // 0x10011cd9
    }
    fat<unsigned>(self, 0x18c) -= 1;                                           // 0x10011ce5 / 0x10011cf3 / 0x10011cfa
    unsigned hr = c3fk_imp<CoCreate_t>(0x000b86bc)(c3fk_sound(0x0005d67c), 0, 1, c3fk_sound(0x0005d5dc), self);   // 0x10011ceb-0x10011d00
    unsigned r = 0;
    if (!hr && fat<void*>(self, 0)) {                                          // 0x10011d06-0x10011d0e
        void* ds = fat<void*>(self, 0);
        r = c3fk_vfn<Com1_t>(ds, 0x28)(ds, fat<void*>(fat<void*>(self, 8), 8));   // 0x10011d10-0x10011d1a
    }
    if (!r) {
        void* ds = fat<void*>(self, 0);                                        // 0x10011d21
        r = c3fk_vfn<Com2_t>(ds, 0x18)(ds, hwnd, level);                       // 0x10011d23-0x10011d2c
    }
    if (r) {                                                                   // 0x10011d1d / 0x10011d2f
        c3fk_fn<Cdecl1u_t>(0x0000268a)(r);                                     // 0x10011d33 / 0x10011d34
        c3fk_fn<Th0_t>(0x0000104b)(self, 0);                                   // 0x10011d3c / 0x10011d3e
        return 3;                                                              // 0x10011d46
    }
    c3fk_gref(0x000b4a3c) = hwnd;                                              // 0x10011d52
    fat<unsigned>(self, 0xc) = 0x60;                                           // 0x10011d5f
    {
        void* ds = fat<void*>(self, 0);                                        // 0x10011d58
        unsigned g = c3fk_vfn<Com1_t>(ds, 0x10)(ds, &fat<char>(self, 0xc));    // 0x10011d5a-0x10011d67
        if (g) c3fk_fn<Cdecl1u_t>(0x0000268a)(g);                              // 0x10011d6a-0x10011d6f
    }
    if (fat<unsigned char>(self, 0x10) & 0x20) {                               // 0x10011d77 / 0x10011d7b
        fat<unsigned char>(self, 0x6c) &= 0xe7;                                // 0x10011dae-0x10011db3
    } else {
        unsigned v = fat<unsigned>(self, 0x30);                                // 0x10011d7d
        fat<unsigned>(self, 0x158) = 0;                                        // 0x10011d80
        fat<unsigned>(self, 0x14c) = v;                                        // 0x10011d86
        fat<unsigned>(self, 0x154) = v;                                        // 0x10011d8c
        fat<unsigned>(self, 0x150) = v;                                        // 0x10011d92
        fat<unsigned>(self, 0x15c) = 0;                                        // 0x10011d9d
        if (fat<unsigned>(self, 0x40) < 2 && (fat<unsigned char>(self, 0x6c) & 8))   // 0x10011d98-0x10011daa
            fat<unsigned char>(self, 0x6c) &= 0xe7;                            // 0x10011db1 / 0x10011db3
    }
    C3fkWaveFmt mono = {1, 1, 0x5622, 0xac44, 2, 0x10, 0};                     // 0x10011dfb-0x10011e11 (esp+0x10)
    C3fkWaveFmt stereo = {1, 2, 0xac44, 0x2b110, 4, 0x10, 0};                  // 0x10011dc0-0x10011df4 (esp+0x24)
    unsigned e = 1;                                                            // 0x10011dbb
    void* prim = &fat<char>(self, 0x70);                                       // 0x10011e16
    if (fat<unsigned char>(self, 0x6c) & 8) {                                  // 0x10011dca / 0x10011ddc / 0x10011e19
        c3fk_fn<Th1p_t>(0x00001d75)(prim, 0, &mono);                           // 0x10011e1b-0x10011e22
        e = 0x11;                                                              // 0x10011e27
    } else {
        c3fk_fn<Th1p_t>(0x00001d75)(prim, 0, &stereo);                         // 0x10011e2e-0x10011e35
    }
    r = c3fk_fn<Th1_t>(0x00001b4a)(prim, 0, 1);                                // 0x10011e3a-0x10011e3e
    if (r) {                                                                   // 0x10011e45 / 0x10011e47
        c3fk_fn<Th0_t>(0x0000104b)(self, 0);                                   // 0x10011e4b
        return r;                                                              // 0x10011e52
    }
    fat<unsigned>(self, 0xe4) = e;                                             // 0x10011e5c
    r = c3fk_fn<Th2p_t>(0x00001bfe)(prim, 0, fat<void*>(self, 0), fat<void*>(self, 0x164));   // 0x10011e62-0x10011e6e
    if (r) {                                                                   // 0x10011e77 / 0x10011e79
        c3fk_fn<Th0_t>(0x0000104b)(self, 0);                                   // 0x10011e7d
        return r;                                                              // 0x10011e82
    }
    fat<unsigned char>(self, 0x6c) |= 2;                                       // 0x10011e8e-0x10011e9a
    fat<void*>(self, 0xcc) = self;                                             // 0x10011e91
    while (fat<void*>(self, 0x178)) {                                          // 0x10011e9d-0x10011ea5, 0x10011ef8-0x10011f00
        void* n = c3fk_popHead(&fat<void*>(self, 0x178), &fat<void*>(self, 0x17c));   // 0x10011eab-0x10011ec2
        void* p = fat<void*>(n, 8);                                            // 0x10011ec8
        c3fk_delete(n);                                                        // 0x10011ecc
        fat<int>(self, 0x184) -= 1;                                            // 0x10011ed1-0x10011edd
        if (!p) break;                                                         // 0x10011edb / 0x10011ee5, 0x10011f32 / 0x10011f3c
        if (fat<unsigned>(self, 0x14c)) c3fk_fn<Th1p_t>(0x00001e51)(p, 0, &fat<char>(p, 0x64));   // 0x10011ee7-0x10011ef3
    }
    fat<unsigned>(self, 0x190) = level;                                        // 0x10011f3e / 0x10011f43
    return 0;                                                                  // 0x10011f4b
}

// 0x00012d10  DirectSoundDevice::createBuffer (VA 0x10012d10). __thiscall (out, flags, size), `ret 0xc`. Without bit 1 of +0x6c
// (0x10012d2e / 0x10012d32) it returns 3 (0x10012d34); the rest is c3fk_dsCreateCore (0x10012d3e-0x10012f34, the first address of
// each pair above).
typedef unsigned(__fastcall* DsCreateBuf_t)(void*, void*, void**, unsigned, unsigned);
DsCreateBuf_t DsCreateBuf_orig;
unsigned __fastcall DsCreateBuf_re(void* self, void*, void** out, unsigned flags, unsigned size) {
    if (!(fat<unsigned char>(self, 0x6c) & 2)) return 3;                       // 0x10012d2e-0x10012d39
    return c3fk_dsCreateCore(self, out, flags, size, 0, false);                // 0x10012d3e-0x10012f34
}

// 0x00012fe0  DirectSoundDevice::createBufferFmt (VA 0x10012fe0). __thiscall (out, fmt, flags, size), `ret 0x10`. Without bit 1
// of +0x6c it returns 3 (0x10012ffe-0x10013009), with a NULL fmt 0xa (0x10013014-0x10013018); the rest is c3fk_dsCreateCore with
// DsBuffer::setPrimaryFormat(fmt) after setBufferSize (0x1001317b-0x10013182) (0x10013022-0x10013221, the second address of each
// pair above).
typedef unsigned(__fastcall* DsCreateFmt_t)(void*, void*, void**, void*, unsigned, unsigned);
DsCreateFmt_t DsCreateFmt_orig;
unsigned __fastcall DsCreateFmt_re(void* self, void*, void** out, void* fmt, unsigned flags, unsigned size) {
    if (!(fat<unsigned char>(self, 0x6c) & 2)) return 3;                       // 0x10012ffe-0x10013009
    if (!fmt) return 0xa;                                                      // 0x1001300e-0x10013018
    return c3fk_dsCreateCore(self, out, flags, size, fmt, true);               // 0x10013022-0x10013221
}

}  // namespace

SG_HOOK("sound.dll", 0x00022320, Sound_lookupOrCreate, Lookup_re, Lookup_orig);
SG_HOOK("sound.dll", 0x00020fe0, Seq_setupChannel, SetupCh_re, SetupCh_orig);
SG_HOOK("sound.dll", 0x00021fa0, Seq_updateChannel, UpdCh_re, UpdCh_orig);
SG_HOOK("sound.dll", 0x0001ff50, Sound5b6a8_dtor, S5b6a8Dtor_re, S5b6a8Dtor_orig);
SG_HOOK("sound.dll", 0x00030180, Sound5c450_dtor, S5c450Dtor_re, S5c450Dtor_orig);
SG_HOOK("sound.dll", 0x000229f0, Seq_buildTrack9, Build9_re, Build9_orig);
SG_HOOK("sound.dll", 0x00022d00, Seq_buildTrack14, Build14_re, Build14_orig);
SG_HOOK("sound.dll", 0x0002d430, getField204, GetF204_re, GetF204_orig);
SG_HOOK("sound.dll", 0x0001ccf0, ChanRecord_init, CrInit_re, CrInit_orig);
SG_HOOK("sound.dll", 0x0001cd30, ChanRecord_freeBuffers, CrFree_re, CrFree_orig);
SG_HOOK("sound.dll", 0x0001b160, StrPtr_assign, StrAssign_re, StrAssign_orig);
SG_HOOK("sound.dll", 0x0001ae50, ChannelTable_callSlot4, CallSlot4_re, CallSlot4_orig);
SG_HOOK("sound.dll", 0x0002e3f0, NodeB000_initBase, NodeBase_re, NodeBase_orig);
SG_HOOK("sound.dll", 0x0000b000, NodeB000_ctor, NodeB_re, NodeB_orig);
SG_HOOK("sound.dll", 0x00031be0, Channel_ctorB, ChCtorB_re, ChCtorB_orig);
SG_HOOK("sound.dll", 0x00023d00, Seq_buildPlayOrder, PlayOrder_re, PlayOrder_orig);
SG_HOOK("sound.dll", 0x00021bf0, Midi_Device_handleMetaEvent, MetaEv_re, MetaEv_orig);
SG_HOOK("sound.dll", 0x000114e0, DirectSoundDevice_enumerate, DsEnum_re, DsEnum_orig);
SG_HOOK("sound.dll", 0x00011a80, DirectSoundDevice_stopAll, DsStopAll_re, DsStopAll_orig);
SG_HOOK("sound.dll", 0x00011290, DirectSoundDevice_close, DsClose_re, DsClose_orig);
SG_HOOK("sound.dll", 0x00012260, DirectSoundDevice_duplicateBuffer, DupBuf_re, DupBuf_orig);
SG_HOOK("sound.dll", 0x00012690, DirectSoundDevice_freeBufferById, FreeById_re, FreeById_orig);
SG_HOOK("sound.dll", 0x00012d10, DirectSoundDevice_createBuffer, DsCreateBuf_re, DsCreateBuf_orig);
SG_HOOK("sound.dll", 0x00011c60, DirectSoundDevice_create, DsDevCreate_re, DsDevCreate_orig);
SG_HOOK("sound.dll", 0x00012fe0, DirectSoundDevice_createBufferFmt, DsCreateFmt_re, DsCreateFmt_orig);

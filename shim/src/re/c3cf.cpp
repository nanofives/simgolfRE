// C3 batch c3cf (2026-10-09) of sound.dll: thirteen methods that are reached only through a vtable slot or a
// stored pointer (log/c3/c3cf_candidates.tsv) - the stream-controller list walkers of the 0x1005b59c and
// 0x1005b5f0 tables, the byte reader of the 0x1005b5f0 table, the Wave_Device stream starter of the
// 0x1005b1c0 table, and eight sequencer slots of the Sound vtable 0x1005b000.
// Addresses in SG_HOOK and in the `// 0x...` comment lines are RVAs; the instruction addresses quoted inside
// each body are VAs (VA = 0x10000000 + RVA), which is how re/tools/asm2inline.py prints sound.dll.
// Hand-written from the disassembly (py -3.12 re/tools/asm2inline.py sound.dll 0x<va> --list) and the C2
// transcriptions in re/analysis/audio. sound.dll is LoadLibrary'd after the shim starts, so these hooks
// install from the LoadLibraryA hook (SgHooksInstallPending, re/hooks.cpp).
// No DirectSound, winmm, Win32, COM or x87 operation is made and nothing is allocated or freed: every
// controller, stream, sequencer, list node, track record and sub-object the A/B passes in is allocated by the
// fixture, and every vtable the bodies dispatch through belongs to one of those synthetic objects. No module
// global of sound.dll is read or written by any of these thirteen bodies.
// Three bodies forward to an original callee at its own address (Seq_setField730Clamped ->
// Seq_applyChannelLevel 0x1002b260, Seq_reset -> Seq_rewind 0x1001e1c0, Seq_getTrackFlagHigh /
// Seq_getListElementAt / Seq_resetTrackControllers -> Seq_listGetAt 0x1001d780); sound.dll is relocatable, so
// those addresses are resolved against the loaded module base at the first call.
// __thiscall is emulated with __fastcall (ecx = this, edx unused).
#include "hooks.h"

namespace {

// Signed offsets: three of these bodies are entered with ecx pointing INTO the object (an embedded base
// sub-object), so they index their own fields with negative displacements.
template <typename T> T& fat(void* base, int off) {
    return *reinterpret_cast<T*>(reinterpret_cast<char*>(base) + off);
}
template <typename T> T fatc(const void* base, int off) {
    return *reinterpret_cast<const T*>(reinterpret_cast<const char*>(base) + off);
}

// sound.dll has base relocations, so an original callee is addressed through the loaded base.
char* c3cf_sound(unsigned rva) {
    static HMODULE m = 0;
    if (!m) m = GetModuleHandleA("sound.dll");
    return reinterpret_cast<char*>(m) + rva;
}

// Shapes of the vtable slots these bodies dispatch through.
typedef void(__fastcall* SlotVoid0_t)(void*, void*);                    // ecx = object, nothing pushed
typedef unsigned(__fastcall* SlotCount0_t)(void*, void*);               // ecx = object, nothing pushed, result
typedef int(__fastcall* SlotMap1_t)(void*, void*, int);                 // ecx = object, one pushed argument
typedef void(__fastcall* SlotReset3_t)(void*, void*, int, int, int);    // ecx = object, three pushed arguments

// Original callees, called at their own addresses.
typedef int(__fastcall* SeqListGetAt_t)(void*, void*, void**, unsigned);     // 0x1001d780
typedef void(__fastcall* SeqApplyChannelLevel_t)(void*, void*, int);         // 0x1002b260
typedef int(__fastcall* SeqRewind_t)(void*, void*);                          // 0x1001e1c0

// ---------------------------------------------------------------------------------------------------------
// Wave_Device stream object (vtable 0x1005b1c0, slot 34)
// ---------------------------------------------------------------------------------------------------------

// 0x00006b00  startStream (VA 0x10006b00)
// __thiscall with one stack argument (`ret 4` at 0x10006b0c and 0x10006b31), the value the body parks at
// +0xcc. The dword at +0x58 is the stream's state word: when its bit 0 is already set the body returns 0xd
// and writes nothing (`test al, 1` / `je 0x10006b0f` at 0x10006b03, 0x10006b05, `mov eax, 0xd` at
// 0x10006b07).
// Otherwise the argument is stored at +0xcc (0x10006b17), the state word is stored back with bit 2 cleared
// and bits 0 and 1 set (`and al, 0xfb` / `or al, 3` at 0x10006b13, 0x10006b15, stored at 0x10006b1d - both
// operate on al, so the upper 24 bits of +0x58 are kept), +0xc8 is cleared (0x10006b22) and the object's own
// vtable slot +0x7c is called with the object still in ecx and nothing pushed (`mov eax, [ecx]` at
// 0x10006b20, `call dword [eax+0x7c]` at 0x10006b2c). Returns 0 (0x10006b2f).
typedef int(__fastcall* StartStream_t)(void*, void*, int);
StartStream_t StartStream_orig;
int __fastcall StartStream_re(void* self, void*, int param) {
    unsigned state = fatc<unsigned>(self, 0x58);                 // 0x10006b00
    if (state & 1) return 0xd;                                   // test al, 1 at 0x10006b03
    fat<int>(self, 0xcc) = param;                                // 0x10006b17
    fat<unsigned>(self, 0x58) = (state & 0xfffffffbu) | 3u;      // and al, 0xfb / or al, 3; stored 0x10006b1d
    void** vtbl = *reinterpret_cast<void***>(self);              // 0x10006b20
    fat<int>(self, 0xc8) = 0;                                    // 0x10006b22
    reinterpret_cast<SlotVoid0_t>(vtbl[0x7c / 4])(self, 0);      // call at 0x10006b2c
    return 0;                                                    // 0x10006b2f
}

// ---------------------------------------------------------------------------------------------------------
// controller base: the element list at this-0x20 (vtable 0x1005b59c, slots 5 and 6)
// ---------------------------------------------------------------------------------------------------------
//
// Both bodies are entered with ecx pointing at an EMBEDDED BASE sub-object, so they address their own fields
// with negative displacements: the list head is this-0x20, the walk cursor this-0x18 and (for the first) a
// result cell this-0x28. A list node is {+4 next, +8 element}, the same node layout Seq_listGetAt 0x1001d780
// walks.

// 0x0001c2a0  CtrlBase_resetElements (VA 0x1001c2a0)
// __thiscall with no stack argument (`ret` at 0x1001c2d9).
// The cursor this-0x18 is seeded with the head this-0x20 (0x1001c2a3, 0x1001c2a8) and the list is walked:
// for every node whose element at +8 is non-null (`test ecx, ecx` / `je 0x1001c2d1` at 0x1001c2b0) the
// element's own vtable slot +0x14 is called with the element in ecx and nothing pushed (`mov eax, [ecx]` /
// `call dword [eax+0x14]` at 0x1001c2b4, 0x1001c2b6). The cursor is then re-read (0x1001c2b9), advanced to
// the node's +4 (0x1001c2c0) and stored back whether or not it is null (0x1001c2c5), and the walk stops at a
// null cursor or at a node with a null element (`je 0x1001c2d1` at 0x1001c2be, 0x1001c2c8, `jne 0x1001c2b4`
// at 0x1001c2cf).
// Whatever path is taken, 0x18 is written to this-0x28 last (0x1001c2d1). No value is loaded into eax before
// the return, so the body has no return value: eax holds the last node pointer on the fall-through paths and
// 0 when the head or a next pointer was null.
typedef void(__fastcall* CtrlBaseResetElements_t)(void*, void*);
CtrlBaseResetElements_t CtrlBaseResetElements_orig;
void __fastcall CtrlBaseResetElements_re(void* self, void*) {
    void* node = fatc<void*>(self, -0x20);                       // 0x1001c2a3
    fat<void*>(self, -0x18) = node;                              // 0x1001c2a8
    while (node) {                                               // je 0x1001c2d1 at 0x1001c2ab
        void* elem = fatc<void*>(node, 8);                       // 0x1001c2ad
        if (!elem) break;                                        // je 0x1001c2d1 at 0x1001c2b2
        void** vtbl = *reinterpret_cast<void***>(elem);          // 0x1001c2b4
        reinterpret_cast<SlotVoid0_t>(vtbl[0x14 / 4])(elem, 0);  // call at 0x1001c2b6
        node = fatc<void*>(self, -0x18);                         // 0x1001c2b9
        if (!node) break;                                        // je 0x1001c2d1 at 0x1001c2be
        node = fatc<void*>(node, 4);                             // 0x1001c2c0
        fat<void*>(self, -0x18) = node;                          // 0x1001c2c5
    }
    fat<int>(self, -0x28) = 0x18;                                // 0x1001c2d1
}

// 0x0001c2f0  CtrlBase_flushElements (VA 0x1001c2f0)
// __thiscall with no stack argument (`ret` at 0x1001c322). The same walk as above over the same list
// (head this-0x20 at 0x1001c2f3, cursor this-0x18 at 0x1001c2f8, 0x1001c309, 0x1001c315), calling vtable
// slot +0x18 of each element instead (`call dword [eax+0x18]` at 0x1001c306), and WITHOUT the final store:
// the only cell this body writes is the cursor. No value is loaded into eax before the return.
typedef void(__fastcall* CtrlBaseFlushElements_t)(void*, void*);
CtrlBaseFlushElements_t CtrlBaseFlushElements_orig;
void __fastcall CtrlBaseFlushElements_re(void* self, void*) {
    void* node = fatc<void*>(self, -0x20);                       // 0x1001c2f3
    fat<void*>(self, -0x18) = node;                              // 0x1001c2f8
    while (node) {                                               // je 0x1001c321 at 0x1001c2fb
        void* elem = fatc<void*>(node, 8);                       // 0x1001c2fd
        if (!elem) break;                                        // je 0x1001c321 at 0x1001c302
        void** vtbl = *reinterpret_cast<void***>(elem);          // 0x1001c304
        reinterpret_cast<SlotVoid0_t>(vtbl[0x18 / 4])(elem, 0);  // call at 0x1001c306
        node = fatc<void*>(self, -0x18);                         // 0x1001c309
        if (!node) break;                                        // je 0x1001c321 at 0x1001c30e
        node = fatc<void*>(node, 4);                             // 0x1001c310
        fat<void*>(self, -0x18) = node;                          // 0x1001c315
    }
}

// 0x0001c370  Ctrl5b5c0_mapValue (VA 0x1001c370)
// __thiscall with one stack argument (`ret 4` at 0x1001c3cc and 0x1001c3dd), a value the body carries
// through the list. Here the list sits at this+0xc (`lea esi, [edi+0xc]` at 0x1001c374), so its head is
// this+0xc and its cursor this+0x14; the `test esi, esi` / `je 0x1001c3cf` at 0x1001c377 can never be taken
// (esi is this+0xc).
// The cursor is seeded with the head (0x1001c37b, 0x1001c37f) and the argument is loaded into the running
// value (0x1001c38b). For each element the dword at +0x1c selects the arm (`sub edx, 3` / `je` / `dec edx` /
// `je` / `dec edx` / `jne` at 0x1001c392-0x1001c39b):
//   * 3 or 4: the element's own vtable slot +0x10 is called with the element in ecx and the running value
//     pushed, and its RESULT becomes the new running value (`mov edx, [ecx]` / `push eax` /
//     `call dword [edx+0x10]` at 0x1001c3a6-0x1001c3a9; the value is read back out of eax at 0x1001c3c4 and
//     0x1001c3d5);
//   * 5: when the element's byte at +0x19 is zero the body returns the running value AS IS, with no table
//     lookup (`test dl, dl` / `je 0x1001c3db` at 0x1001c3a0, 0x1001c3a2, and 0x1001c3db loads nothing into
//     eax); when it is non-zero the element is skipped (`jmp 0x1001c3ac` at 0x1001c3a4);
//   * anything else: the element is skipped (`jne 0x1001c3ac` at 0x1001c39b).
// The walk then advances exactly as the two bodies above (0x1001c3ac-0x1001c3c2).
// Every other exit returns the zero-extended BYTE at this + 0x5c + value (`xor ecx, ecx` / `mov cl, byte
// [eax+edi+0x5c]` / `mov eax, ecx` at 0x1001c3c4-0x1001c3c9 and 0x1001c3d3-0x1001c3d9), with `value` the
// running value - the argument itself when the list is empty or its first element is null (0x1001c3cf).
typedef int(__fastcall* Ctrl5b5c0MapValue_t)(void*, void*, int);
Ctrl5b5c0MapValue_t Ctrl5b5c0MapValue_orig;
int __fastcall Ctrl5b5c0MapValue_re(void* self, void*, int value) {
    void** cursor = &fat<void*>(self, 0x14);                     // this+0xc +8, written at 0x1001c37f
    void* node = fatc<void*>(self, 0xc);                         // 0x1001c37b
    *cursor = node;
    void* elem = node ? fatc<void*>(node, 8) : 0;                // 0x1001c384
    while (node && elem) {                                       // je 0x1001c3cf at 0x1001c382, 0x1001c389
        const int type = fatc<int>(elem, 0x1c);                  // 0x1001c38f
        if (type == 3 || type == 4) {                            // je 0x1001c3a6 at 0x1001c395, 0x1001c398
            void** vtbl = *reinterpret_cast<void***>(elem);      // 0x1001c3a6
            value = reinterpret_cast<SlotMap1_t>(vtbl[0x10 / 4])(elem, 0, value);   // call at 0x1001c3a9
        } else if (type == 5) {                                  // jne 0x1001c3ac at 0x1001c39b
            if (fatc<unsigned char>(elem, 0x19) == 0) return value;   // je 0x1001c3db at 0x1001c3a2
        }
        node = *cursor;                                          // 0x1001c3ac
        if (!node) break;                                        // je 0x1001c3d3 at 0x1001c3b1
        node = fatc<void*>(node, 4);                             // 0x1001c3b3
        *cursor = node;                                          // 0x1001c3b8
        if (!node) break;                                        // je 0x1001c3d3 at 0x1001c3bb
        elem = fatc<void*>(node, 8);                             // 0x1001c3bd
        if (!elem) break;                                        // jne 0x1001c38f at 0x1001c3c2
    }
    return fatc<unsigned char>(self, 0x5c + value);              // 0x1001c3c4 / 0x1001c3d5
}

// ---------------------------------------------------------------------------------------------------------
// byte stream of the 0x1005b5f0 table (slots 4 and 6)
// ---------------------------------------------------------------------------------------------------------
//
// Both bodies read one byte out of the buffer at +0x2c at the position in +0x30, against the limit in +0x3c,
// and cache the byte at +0x19. The refill is the object's own vtable slot +0x14, called with the object in
// ecx and nothing pushed; neither body looks at what it returns.

// 0x0001c7a0  streamReadByte (VA 0x1001c7a0)
// __thiscall with one stack argument that the body never reads (`ret 4` at 0x1001c7c9; there is no
// `[esp+8]` in the 44 bytes of the function).
// When the position at +0x30 is NOT below the limit at +0x3c (`cmp eax, ecx` / `jb 0x1001c7b4` at
// 0x1001c7a9, 0x1001c7ab - UNSIGNED) the refill slot +0x14 is called (0x1001c7ad-0x1001c7b1). The position
// is then re-read (0x1001c7b4), the byte at buffer[position] is fetched (0x1001c7b7, 0x1001c7ba), the
// position is incremented and stored (0x1001c7bd, 0x1001c7be), the byte is cached at +0x19 (0x1001c7c3) and
// returned zero-extended (`xor eax, eax` / `mov al, dl` at 0x1001c7c1, 0x1001c7c6).
typedef int(__fastcall* StreamReadByte_t)(void*, void*, int);
StreamReadByte_t StreamReadByte_orig;
int __fastcall StreamReadByte_re(void* self, void*, int /* unread argument */) {
    unsigned pos = fatc<unsigned>(self, 0x30);                   // 0x1001c7a3
    const unsigned limit = fatc<unsigned>(self, 0x3c);           // 0x1001c7a6
    if (pos >= limit) {                                          // jb 0x1001c7b4 at 0x1001c7ab
        void** vtbl = *reinterpret_cast<void***>(self);          // 0x1001c7ad
        reinterpret_cast<SlotVoid0_t>(vtbl[0x14 / 4])(self, 0);  // call at 0x1001c7b1
    }
    pos = fatc<unsigned>(self, 0x30);                            // 0x1001c7b4
    const unsigned char* buf = fatc<const unsigned char*>(self, 0x2c);   // 0x1001c7b7
    const unsigned char b = buf[pos];                            // 0x1001c7ba
    fat<unsigned>(self, 0x30) = pos + 1;                         // 0x1001c7bd, 0x1001c7be
    fat<unsigned char>(self, 0x19) = b;                          // 0x1001c7c3
    return b;                                                    // 0x1001c7c1, 0x1001c7c6
}

// 0x0001c7e0  streamConsumeByte (VA 0x1001c7e0)
// __thiscall with no stack argument (`ret` at 0x1001c805). The same body as streamReadByte with the two
// final stores in the other order (the cache at +0x19 at 0x1001c7fe, the position at +0x30 at 0x1001c801)
// and no return value: eax is left holding the incremented position.
typedef void(__fastcall* StreamConsumeByte_t)(void*, void*);
StreamConsumeByte_t StreamConsumeByte_orig;
void __fastcall StreamConsumeByte_re(void* self, void*) {
    unsigned pos = fatc<unsigned>(self, 0x30);                   // 0x1001c7e3
    const unsigned limit = fatc<unsigned>(self, 0x3c);           // 0x1001c7e6
    if (pos >= limit) {                                          // jb 0x1001c7f4 at 0x1001c7eb
        void** vtbl = *reinterpret_cast<void***>(self);          // 0x1001c7ed
        reinterpret_cast<SlotVoid0_t>(vtbl[0x14 / 4])(self, 0);  // call at 0x1001c7f1
    }
    pos = fatc<unsigned>(self, 0x30);                            // 0x1001c7f4
    const unsigned char* buf = fatc<const unsigned char*>(self, 0x2c);   // 0x1001c7f7
    const unsigned char b = buf[pos];                            // 0x1001c7fa
    fat<unsigned char>(self, 0x19) = b;                          // 0x1001c7fe
    fat<unsigned>(self, 0x30) = pos + 1;                         // 0x1001c801
}

// ---------------------------------------------------------------------------------------------------------
// sequencer slots of the Sound vtable 0x1005b000
// ---------------------------------------------------------------------------------------------------------

// 0x00020f50  Seq_setField730Clamped (VA 0x10020f50)
// __thiscall with one stack argument (`ret 4` at 0x10020f70 and 0x10020f89). The argument is clamped to
// 0..0x7f with SIGNED compares (`cmp esi, 0x7f` / `jle 0x10020f73` at 0x10020f56, 0x10020f5b with
// `mov esi, 0x7f` at 0x10020f5d; `test esi, esi` / `jge 0x10020f79` at 0x10020f73, 0x10020f75 with
// `xor esi, esi` at 0x10020f77), handed to Seq_applyChannelLevel (VA 0x1002b260, through the
// incremental-link thunk at 0x100025d6, called at 0x10020f63 and 0x10020f7c with the sequencer still in ecx)
// and then stored at this+0x730 (0x10020f68, 0x10020f81).
// The two arms are separate code paths that differ only in the clamp; the store and the call are identical.
// No value is loaded into eax after the call, so the body has no return value of its own.
// The reimplementation calls the original Seq_applyChannelLevel at its own address.
typedef void(__fastcall* SeqSetField730Clamped_t)(void*, void*, int);
SeqSetField730Clamped_t SeqSetField730Clamped_orig;
void __fastcall SeqSetField730Clamped_re(void* self, void*, int value) {
    SeqApplyChannelLevel_t apply =
        reinterpret_cast<SeqApplyChannelLevel_t>(c3cf_sound(0x0002b260));
    if (value > 0x7f) {                                          // jle 0x10020f73 at 0x10020f5b
        value = 0x7f;                                            // 0x10020f5d
        apply(self, 0, value);                                   // 0x10020f63
        fat<int>(self, 0x730) = value;                           // 0x10020f68
    } else {
        if (value < 0) value = 0;                                // jge 0x10020f79 at 0x10020f75
        apply(self, 0, value);                                   // 0x10020f7c
        fat<int>(self, 0x730) = value;                           // 0x10020f81
    }
}

// 0x00022240  Seq_reset (VA 0x10022240)
// __thiscall with no stack argument (`ret` at 0x10022266) and no conditional jump of its own.
// Seq_rewind (VA 0x1001e1c0, through the incremental-link thunk at 0x10022246) is called on the reader
// sub-object at this+0x60 (`lea ecx, [esi+0x60]` at 0x10022243); that callee seeds the reader's walk cursor
// (this+0x204) from its list head (this+0x1fc), calls Seq_rewindTracks 0x1001e8e0 on every non-null element
// and clears bits 2, 4, 5 and 7 of the reader's byte at this+0xac (`and al, 0x63` at 0x1001e202).
// Then the dword at this+0x214 is read (0x1002224b), this+0x740 is cleared (0x10022251) and bits 3 and 4 of
// this+0x214 are cleared and stored back (`and al, 0xe7` at 0x1002225b, stored at 0x1002225d - al only, so
// the upper 24 bits are kept). Returns 0 (0x10022263).
// The reimplementation calls the original Seq_rewind at its own address.
typedef int(__fastcall* SeqReset_t)(void*, void*);
SeqReset_t SeqReset_orig;
int __fastcall SeqReset_re(void* self, void*) {
    SeqRewind_t rewind = reinterpret_cast<SeqRewind_t>(c3cf_sound(0x0001e1c0));
    rewind(reinterpret_cast<char*>(self) + 0x60, 0);             // 0x10022243, 0x10022246
    unsigned flags = fatc<unsigned>(self, 0x214);                // 0x1002224b
    fat<int>(self, 0x740) = 0;                                   // 0x10022251
    fat<unsigned>(self, 0x214) = flags & 0xffffffe7u;            // and al, 0xe7 at 0x1002225b
    return 0;                                                    // 0x10022263
}

// 0x000227a0  Seq_setField240 (VA 0x100227a0)
// __thiscall with one stack argument (`ret 4` at 0x100227ac), no callee and no conditional jump: the
// argument is stored at this+0x240 (0x100227a0, 0x100227a4) and 0 is returned (0x100227aa).
typedef int(__fastcall* SeqSetField240_t)(void*, void*, int);
SeqSetField240_t SeqSetField240_orig;
int __fastcall SeqSetField240_re(void* self, void*, int value) {
    fat<int>(self, 0x240) = value;                               // 0x100227a4
    return 0;                                                    // 0x100227aa
}

// 0x00023000  Seq_findTrackOrZero (VA 0x10023000)
// __thiscall with one stack argument (`ret 4` at 0x1002300c and 0x10023051), the id to find. No callee.
// A zero dword at this+0x224 returns 0 without touching anything (`test eax, eax` / `jne 0x1002300f` at
// 0x10023006, 0x10023008, `xor eax, eax` at 0x1002300a).
// Otherwise the walk cursor at this+0x220 is seeded with the list head at this+0x218 (0x1002300f,
// 0x10023017) and the list is walked with the same node layout as above ({+4 next, +8 element}): the element
// whose dword at +4 equals the argument is returned (`cmp dword [eax+4], edx` / `je 0x10023051` at
// 0x1002302a, 0x1002302d), the cursor is advanced and stored back at every step (0x1002302f-0x1002303e) and
// every null - head, cursor, next or element - ends the walk with 0 (`je` at 0x1002301d, 0x10023024,
// 0x10023037, 0x10023044, 0x1002304b, and `xor eax, eax` at 0x1002304f; the three `je 0x10023051` jumps
// arrive with eax already 0).
typedef void*(__fastcall* SeqFindTrackOrZero_t)(void*, void*, int);
SeqFindTrackOrZero_t SeqFindTrackOrZero_orig;
void* __fastcall SeqFindTrackOrZero_re(void* self, void*, int id) {
    if (fatc<void*>(self, 0x224) == 0) return 0;                 // jne 0x1002300f at 0x10023008
    void* node = fatc<void*>(self, 0x218);                       // 0x1002300f
    fat<void*>(self, 0x220) = node;                              // 0x10023017
    if (!node) return 0;                                         // je 0x1002304f at 0x1002301d
    void* elem = fatc<void*>(node, 8);                           // 0x1002301f
    if (!elem) return 0;                                         // je 0x10023051 at 0x10023024
    for (;;) {
        if (fatc<int>(elem, 4) == id) return elem;               // je 0x10023051 at 0x1002302d
        node = fatc<void*>(self, 0x220);                         // 0x1002302f
        if (!node) return 0;                                     // je 0x1002304f at 0x10023037
        node = fatc<void*>(node, 4);                             // 0x10023039
        fat<void*>(self, 0x220) = node;                          // 0x1002303e
        if (!node) return 0;                                     // je 0x1002304f at 0x10023044
        elem = fatc<void*>(node, 8);                             // 0x10023046
        if (!elem) return 0;                                     // je 0x10023051 at 0x1002304b
    }
}

// 0x00023f10  Seq_getTrackFlagHigh (VA 0x10023f10)
// __thiscall with one stack argument (`ret 4` at 0x10023f28 and 0x10023f47), a track index.
// The sequencer's own track count comes from its vtable slot +0xb4, called with the sequencer in ecx and
// nothing pushed (`mov eax, [esi]` / `call dword [eax+0xb4]` at 0x10023f14, 0x10023f16). The index is
// rejected when it is ABOVE that count (`cmp ecx, eax` / `jbe 0x10023f2b` at 0x10023f20, 0x10023f22 -
// UNSIGNED, and `jbe` accepts index == count, unlike the `jae` reject of Seq_applyTrackFlag 0x10023e10).
// The rejected path executes `xor al, al` only (0x10023f24), so the returned dword is the COUNT with its low
// byte zeroed, not 0.
// Otherwise Seq_listGetAt (VA 0x1001d780, through the incremental-link thunk at 0x10023f34) is called on the
// list at this+0x60 (`lea ecx, [esi+0x60]` at 0x10023f31) with the index as the step count and a local slot
// as the out pointer (`lea ecx, [esp+8]` at 0x10023f2c); the record it wrote is read back at 0x10023f39 and
// bit 4 of its byte at +0x38 is extracted (`mov al, [edx+0x38]` / `shr al, 4` / `and al, 1` at 0x10023f3e,
// 0x10023f41, 0x10023f44). Here too only al is written, but by then the call has replaced eax with
// Seq_listGetAt's own return code, so the returned dword on this path is that code with its low byte
// replaced by the bit - not the count. (Measured: with the count 0x105 the accepted indices return 1 and 0,
// while the rejected one returns 0x100; log/diff/00023f10_sound_Seq_getTrackFlagHigh.path1.csv.)
// Seq_listGetAt writes 0 through the out pointer when the index is past the end of the list, and the body
// then dereferences 0x38: the original faults. The fixtures therefore only pass indices that resolve.
// The reimplementation calls the original Seq_listGetAt at its own address.
typedef int(__fastcall* SeqGetTrackFlagHigh_t)(void*, void*, unsigned);
SeqGetTrackFlagHigh_t SeqGetTrackFlagHigh_orig;
int __fastcall SeqGetTrackFlagHigh_re(void* self, void*, unsigned index) {
    void** vtbl = *reinterpret_cast<void***>(self);              // 0x10023f14
    const unsigned count = reinterpret_cast<SlotCount0_t>(vtbl[0xb4 / 4])(self, 0);   // 0x10023f16
    if (index > count) return static_cast<int>(count & 0xffffff00u);   // xor al, al at 0x10023f24
    void* record = 0;
    SeqListGetAt_t getAt = reinterpret_cast<SeqListGetAt_t>(c3cf_sound(0x0001d780));
    const unsigned rc = static_cast<unsigned>(
        getAt(reinterpret_cast<char*>(self) + 0x60, 0, &record, index));   // 0x10023f34, eax = the code
    const unsigned bit = (fatc<unsigned char>(record, 0x38) >> 4) & 1u;    // 0x10023f3e-0x10023f44
    return static_cast<int>((rc & 0xffffff00u) | bit);
}

// 0x000241f0  Seq_getListElementAt (VA 0x100241f0)
// __thiscall with two stack arguments (`ret 8` at 0x10024216 and 0x1002421b). The first is a guard that must
// be 0 (`test eax, eax` / `jne 0x10024219` at 0x100241f4, 0x100241f6) and the second is the index, rejected
// when it is NEGATIVE (`test eax, eax` / `jl 0x10024219` at 0x100241fc, 0x100241fe - SIGNED).
// Seq_listGetAt (VA 0x1001d780, through the incremental-link thunk at 0x10024209) is then called on the list
// at this+0x60 (`add ecx, 0x60` at 0x10024206) with the index as the step count; the out pointer it is given
// is the CALLER'S OWN first argument slot (`lea eax, [esp+8]` at 0x10024201, which after the `push eax` of
// 0x10024200 is the stack slot the guard was read from), and that slot is read back as the result at
// 0x10024212. A non-zero return from Seq_listGetAt returns 0 instead (0x1002420e, 0x10024210, 0x10024219).
// The write into the caller's argument slot is not observable from a call made through the A/B harness,
// which pushes its own copy of the arguments; the reimplementation uses a local out slot.
// The reimplementation calls the original Seq_listGetAt at its own address.
typedef void*(__fastcall* SeqGetListElementAt_t)(void*, void*, int, int);
SeqGetListElementAt_t SeqGetListElementAt_orig;
void* __fastcall SeqGetListElementAt_re(void* self, void*, int guard, int index) {
    if (guard != 0) return 0;                                    // jne 0x10024219 at 0x100241f6
    if (index < 0) return 0;                                     // jl 0x10024219 at 0x100241fe
    void* out = 0;
    SeqListGetAt_t getAt = reinterpret_cast<SeqListGetAt_t>(c3cf_sound(0x0001d780));
    if (getAt(reinterpret_cast<char*>(self) + 0x60, 0, &out, static_cast<unsigned>(index)) != 0)
        return 0;                                                // jne 0x10024219 at 0x10024210
    return out;                                                  // 0x10024212
}

// 0x00024630  Seq_resetTrackControllers (VA 0x10024630)
// __thiscall with one stack argument (`ret 4` at 0x10024675), a track index.
// Seq_listGetAt (VA 0x1001d780, through the incremental-link thunk at 0x1002463d) is called on the list at
// this+0x60 (`add ecx, 0x60` at 0x1002463a) with the index as the step count and the CALLER'S OWN argument
// slot as the out pointer (`lea edx, [esp+4]` at 0x10024634); a non-zero return leaves the body at once
// (`test eax, eax` / `jne 0x10024675` at 0x10024642, 0x10024644) with that return value, which is the 0x24
// Seq_listGetAt reports for an index past the end.
// Otherwise bit 0 of the record's byte at +0x38 is set (`or byte [eax+0x38], 1` at 0x10024650) and the
// record's pointer array is walked DOWNWARDS from +0x254 to +0x58 in steps of four (`mov esi, 0x254` at
// 0x1002464b, `mov ecx, [esi+eax]` at 0x10024658, `sub esi, 4` / `cmp esi, 0x58` / `jge 0x10024654` at
// 0x1002466a-0x10024670: 128 slots). Every non-null slot (`test ecx, ecx` / `je 0x1002466a` at 0x1002465b,
// 0x1002465d) has its own vtable slot +0x34 called with the sub-object in ecx and the three constants 0,
// 0x7f and 0x19 pushed in that order (`push 0x19` / `push 0x7f` / `push 0` / `call dword [edx+0x34]` at
// 0x10024661-0x10024667). Returns 0 (0x10024672).
// The reimplementation calls the original Seq_listGetAt at its own address.
typedef int(__fastcall* SeqResetTrackControllers_t)(void*, void*, unsigned);
SeqResetTrackControllers_t SeqResetTrackControllers_orig;
int __fastcall SeqResetTrackControllers_re(void* self, void*, unsigned index) {
    void* record = 0;
    SeqListGetAt_t getAt = reinterpret_cast<SeqListGetAt_t>(c3cf_sound(0x0001d780));
    const int rc = getAt(reinterpret_cast<char*>(self) + 0x60, 0, &record, index);   // 0x1002463d
    if (rc != 0) return rc;                                      // jne 0x10024675 at 0x10024644
    fat<unsigned char>(record, 0x38) =
        static_cast<unsigned char>(fatc<unsigned char>(record, 0x38) | 1);   // 0x10024650
    for (int off = 0x254; off >= 0x58; off -= 4) {               // 0x1002464b, 0x1002466a-0x10024670
        void* ctrl = fatc<void*>(record, off);                   // 0x10024658
        if (!ctrl) continue;                                     // je 0x1002466a at 0x1002465d
        void** vtbl = *reinterpret_cast<void***>(ctrl);          // 0x1002465f
        reinterpret_cast<SlotReset3_t>(vtbl[0x34 / 4])(ctrl, 0, 0, 0x7f, 0x19);   // call at 0x10024667
    }
    return 0;                                                    // 0x10024672
}

SG_HOOK("sound.dll", 0x00006b00, sound_startStream, StartStream_re, StartStream_orig);
SG_HOOK("sound.dll", 0x0001c2a0, sound_CtrlBase_resetElements, CtrlBaseResetElements_re,
        CtrlBaseResetElements_orig);
SG_HOOK("sound.dll", 0x0001c2f0, sound_CtrlBase_flushElements, CtrlBaseFlushElements_re,
        CtrlBaseFlushElements_orig);
SG_HOOK("sound.dll", 0x0001c370, sound_Ctrl5b5c0_mapValue, Ctrl5b5c0MapValue_re, Ctrl5b5c0MapValue_orig);
SG_HOOK("sound.dll", 0x0001c7a0, sound_streamReadByte, StreamReadByte_re, StreamReadByte_orig);
SG_HOOK("sound.dll", 0x0001c7e0, sound_streamConsumeByte, StreamConsumeByte_re, StreamConsumeByte_orig);
SG_HOOK("sound.dll", 0x00020f50, sound_Seq_setField730Clamped, SeqSetField730Clamped_re,
        SeqSetField730Clamped_orig);
SG_HOOK("sound.dll", 0x00022240, sound_Seq_reset, SeqReset_re, SeqReset_orig);
SG_HOOK("sound.dll", 0x000227a0, sound_Seq_setField240, SeqSetField240_re, SeqSetField240_orig);
SG_HOOK("sound.dll", 0x00023000, sound_Seq_findTrackOrZero, SeqFindTrackOrZero_re, SeqFindTrackOrZero_orig);
SG_HOOK("sound.dll", 0x00023f10, sound_Seq_getTrackFlagHigh, SeqGetTrackFlagHigh_re,
        SeqGetTrackFlagHigh_orig);
SG_HOOK("sound.dll", 0x000241f0, sound_Seq_getListElementAt, SeqGetListElementAt_re,
        SeqGetListElementAt_orig);
SG_HOOK("sound.dll", 0x00024630, sound_Seq_resetTrackControllers, SeqResetTrackControllers_re,
        SeqResetTrackControllers_orig);

}  // namespace

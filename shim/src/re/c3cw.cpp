// Batch c3cw (round 23, jgld.dll): destructor chains that earlier batches left at C2 for the heap or for the
// live tracked-object list, now A/B-able.
//
//   group A - the Array<T> and FontBase destructors. Their only obstacle was the direct `operator delete`
//             (c3ck, c3cn notes); re/frida/js/heapstandin.js now serves operator delete 0x1007e6d0 for the
//             A/B thread, so the frees are recorded in compared state.
//     Array_e0::dtor            0x000669e0     Array_e0::deletingDtor     0x00066bd0
//     Array_Rec14::dtor         0x00066a30     Array_Rec14::deletingDtor  0x00066c40
//     Array_Rec94::dtor         0x00066b00     Array_Rec94::deletingDtor  0x00066cb0
//     FontBase::~FontBase       0x00069010     FontBase::deletingDtor     0x00069140
//
//   group B - the Tracked destructor and the two classes above it. Tracked::dtor runs LinkedList::find and
//             LinkedList::removeCurrent on the module's tracked-object list 0x10128428 (a fixed global),
//             which c3cd/c3bw would not hand to an A/B. The fixture swaps that global for a list of its own
//             with the game's first thread frozen (re/frida/js/freeze.js); log/c3/alt/c3cw_probe.py measured
//             that only that thread touches the list (Tracked::ctor/dtor, add/find/removeCurrent).
//     Tracked::dtor             0x00006b80     Tracked::deletingDtor      0x00006b10
//     PaletteBase::dtor         0x0006af00     PaletteBase::deletingDtor  0x0006af50
//     Palette::~Palette         0x0006a2c0     Palette::deletingDtor      0x0006a1f0
//
// SG_HOOK addresses are RVAs; every instruction cited in a comment is a VA (0x10000000 + RVA), the form the
// debug build's disassembly prints. jgld.dll is LoadLibrary'd after the shim starts, so these hooks install
// from the LoadLibraryA hook (SgHooksInstallPending, re/hooks.cpp). jgld.dll is relocated at run time, so every
// module address (vtables, the list global, callees) is resolved against the running base. __thiscall is
// emulated with __fastcall: ecx = this, edx unused. Callees are called through their original addresses (the
// hook trampoline when another batch hooks them).
#include "hooks.h"

namespace {

char* JgldBase() {
    static char* base = 0;
    if (!base) base = reinterpret_cast<char*>(GetModuleHandleA("jgld.dll"));
    return base;
}
void* JgldVA(unsigned va) { return JgldBase() + (va - 0x10000000); }

void SetVptr(void* self, unsigned vtableVA) { *static_cast<void**>(self) = JgldVA(vtableVA); }

typedef void(__cdecl* OperatorDelete_t)(void*);       // 0x1007e6d0  operator delete
typedef void(__fastcall* Method_t)(void*, void*);     // __thiscall, no stack argument
typedef int(__fastcall* Find_t)(void*, void*, void*); // __thiscall LinkedList::find(data)

void CallMethod(unsigned va, void* self) { reinterpret_cast<Method_t>(JgldVA(va))(self, 0); }

// The scalar deleting destructor shape every `deletingDtor` of this batch has (the Array one at
// 0x10066bd0 shown; the others differ only in the destructor called):
//   dtor(this)                                   call 0x10066bf0 (through an incremental-link thunk)
//   if (flags & 1) operator delete(this)         and eax,1 0x10066bf8 / test / je 0x10066bfd, call 0x10066c03
//   return this                                  mov eax,[ebp-4] 0x10066c0b, ret 4 0x10066c1e
// Only bit 0 of the flags is read; the other 31 bits are discarded.
void* DeletingDtor(void* self, unsigned flags, unsigned dtorVA) {
    CallMethod(dtorVA, self);
    if (flags & 1)
        reinterpret_cast<OperatorDelete_t>(JgldVA(0x1007e6d0))(self);
    return self;
}

typedef void*(__fastcall* DeletingDtor_t)(void*, void*, unsigned);
typedef void(__fastcall* Dtor_t)(void*, void*);

// ===================================================================================== group A

// 0x000669e0  Array_e0::dtor
// Writes the Array<e0> vtable 0x1011da58 at [this] (0x10066a00), then calls Array_e0::clear 0x10068000
// (`call 0x10066a09` through the thunk 0x100019f1), which hands the buffer at +4 to operator delete when it
// is not null and writes buffer 0, capacity 0, count 0 and growth step 1. Returns nothing (`ret` at
// 0x10066a1e, eax not set).
Dtor_t ArrayE0Dtor_orig;
void __fastcall ArrayE0Dtor_re(void* self, void*) {
    SetVptr(self, 0x1011da58);
    CallMethod(0x10068000, self);
}

// 0x00066bd0  Array_e0::deletingDtor
// Array_e0::dtor 0x100669e0 (thunk 0x10001771, call at 0x10066bf0), then the guarded operator delete at
// 0x10066c03, then `this` (0x10066c0b).
DeletingDtor_t ArrayE0DeletingDtor_orig;
void* __fastcall ArrayE0DeletingDtor_re(void* self, void*, unsigned flags) {
    return DeletingDtor(self, flags, 0x100669e0);
}

// 0x00066a30  Array_Rec14::dtor
// The same body one instance on: vtable 0x1011da5c at [this] (0x10066a50), then Array_Rec14::clear
// 0x10068100 (thunk 0x1000194c, call at 0x10066a59).
Dtor_t ArrayRec14Dtor_orig;
void __fastcall ArrayRec14Dtor_re(void* self, void*) {
    SetVptr(self, 0x1011da5c);
    CallMethod(0x10068100, self);
}

// 0x00066c40  Array_Rec14::deletingDtor
// Array_Rec14::dtor 0x10066a30 (thunk 0x100016bd, call at 0x10066c60), guarded delete at 0x10066c73.
DeletingDtor_t ArrayRec14DeletingDtor_orig;
void* __fastcall ArrayRec14DeletingDtor_re(void* self, void*, unsigned flags) {
    return DeletingDtor(self, flags, 0x10066a30);
}

// 0x00066b00  Array_Rec94::dtor
// Vtable 0x1011da60 at [this] (0x10066b20), then Array_Rec94::clear 0x10068360 (thunk 0x100019ba, call at
// 0x10066b29).
Dtor_t ArrayRec94Dtor_orig;
void __fastcall ArrayRec94Dtor_re(void* self, void*) {
    SetVptr(self, 0x1011da60);
    CallMethod(0x10068360, self);
}

// 0x00066cb0  Array_Rec94::deletingDtor
// Array_Rec94::dtor 0x10066b00 (thunk 0x1000150f, call at 0x10066cd0), guarded delete at 0x10066ce3.
DeletingDtor_t ArrayRec94DeletingDtor_orig;
void* __fastcall ArrayRec94DeletingDtor_re(void* self, void*, unsigned flags) {
    return DeletingDtor(self, flags, 0x10066b00);
}

// 0x00069010  FontBase::~FontBase
// One store: the FontBase vtable 0x1011da90 at [this] (0x10069030). No call (the body has no __chkesp
// either: `mov esp, ebp` follows the store at 0x10069039), no return value, no other field touched.
Dtor_t FontBaseDtor_orig;
void __fastcall FontBaseDtor_re(void* self, void*) {
    SetVptr(self, 0x1011da90);
}

// 0x00069140  FontBase::deletingDtor
// FontBase::~FontBase 0x10069010 (thunk 0x1000108c, call at 0x10069160), guarded delete at 0x10069173.
DeletingDtor_t FontBaseDeletingDtor_orig;
void* __fastcall FontBaseDeletingDtor_re(void* self, void*, unsigned flags) {
    return DeletingDtor(self, flags, 0x10069010);
}

// ===================================================================================== group B

// 0x00006b80  Tracked::dtor
// Writes the Tracked vtable 0x1011d058 at [this] (0x10006ba0), then, on the module's tracked-object list
// 0x10128428 (`mov ecx, 0x10128428` at 0x10006baa and 0x10006bb4):
//   LinkedList::find(this)        0x100072b0 (thunk 0x10001906, call at 0x10006baf): makes the node whose
//                                 data is `this` the list's current node, if there is one;
//   LinkedList::removeCurrent()   0x10007100 (thunk 0x1000123a, call at 0x10006bb9): unlinks and deletes
//                                 the current node.
// find's result is not tested: when `this` is not in the list, removeCurrent still removes whatever node is
// current (or nothing for an empty list). Returns nothing.
Dtor_t TrackedDtor_orig;
void __fastcall TrackedDtor_re(void* self, void*) {
    SetVptr(self, 0x1011d058);
    void* list = JgldVA(0x10128428);
    reinterpret_cast<Find_t>(JgldVA(0x100072b0))(list, 0, self);
    CallMethod(0x10007100, list);
}

// 0x00006b10  Tracked::deletingDtor
// Tracked::dtor 0x10006b80 (thunk 0x1000195b, call at 0x10006b30), guarded delete at 0x10006b43, `this`.
DeletingDtor_t TrackedDeletingDtor_orig;
void* __fastcall TrackedDeletingDtor_re(void* self, void*, unsigned flags) {
    return DeletingDtor(self, flags, 0x10006b80);
}

// 0x0006af00  PaletteBase::dtor
// Writes the PaletteBase vtable 0x1011db3c at [this] (0x1006af20), then Tracked::dtor 0x10006b80 on the same
// object (thunk 0x1000195b, call at 0x1006af29): the Tracked part sits at offset 0. Returns nothing.
Dtor_t PaletteBaseDtor_orig;
void __fastcall PaletteBaseDtor_re(void* self, void*) {
    SetVptr(self, 0x1011db3c);
    CallMethod(0x10006b80, self);
}

// 0x0006af50  PaletteBase::deletingDtor
// PaletteBase::dtor 0x1006af00 (thunk 0x100019b0, call at 0x1006af70), guarded delete at 0x1006af83.
DeletingDtor_t PaletteBaseDeletingDtor_orig;
void* __fastcall PaletteBaseDeletingDtor_re(void* self, void*, unsigned flags) {
    return DeletingDtor(self, flags, 0x1006af00);
}

// 0x0006a2c0  Palette::~Palette
// Writes the Palette vtable 0x1011db10 at [this] (0x1006a2f5). If the pointer at +0x80c is not null
// (cmp 0x1006a305, je 0x1006a30c), the dword at +4 of the object it points to is cleared (0x1006a317). Then
// Palette::reset 0x1006a380 (thunk 0x10001451, call at 0x1006a321: +0x810 = 0, +0x814 = 0) and
// PaletteBase::dtor 0x1006af00 (thunk 0x100019b0, call at 0x1006a330). The original also builds a C++ EH
// frame (fs:[0] at 0x1006a2ca-0x1006a2d1, state 0 at 0x1006a2fb, -1 at 0x1006a326) that unwinds only when a
// callee throws; the reimplementation builds none, so a throw inside a callee is outside what it reproduces.
// Returns nothing.
Dtor_t PaletteDtor_orig;
void __fastcall PaletteDtor_re(void* self, void*) {
    SetVptr(self, 0x1011db10);
    char* p = static_cast<char*>(self);
    char* linked = *reinterpret_cast<char**>(p + 0x80c);
    if (linked != 0)
        *reinterpret_cast<unsigned*>(linked + 4) = 0;
    CallMethod(0x1006a380, self);
    CallMethod(0x1006af00, self);
}

// 0x0006a1f0  Palette::deletingDtor
// Palette::~Palette 0x1006a2c0 (thunk 0x1000179e, call at 0x1006a210), guarded delete at 0x1006a223, `this`.
DeletingDtor_t PaletteDeletingDtor_orig;
void* __fastcall PaletteDeletingDtor_re(void* self, void*, unsigned flags) {
    return DeletingDtor(self, flags, 0x1006a2c0);
}

}  // namespace

// 0x000669e0  Array_e0::dtor
SG_HOOK("jgld.dll", 0x000669E0, Array_e0_dtor, ArrayE0Dtor_re, ArrayE0Dtor_orig);
// 0x00066bd0  Array_e0::deletingDtor
SG_HOOK("jgld.dll", 0x00066BD0, Array_e0_deletingDtor, ArrayE0DeletingDtor_re, ArrayE0DeletingDtor_orig);
// 0x00066a30  Array_Rec14::dtor
SG_HOOK("jgld.dll", 0x00066A30, Array_Rec14_dtor, ArrayRec14Dtor_re, ArrayRec14Dtor_orig);
// 0x00066c40  Array_Rec14::deletingDtor
SG_HOOK("jgld.dll", 0x00066C40, Array_Rec14_deletingDtor, ArrayRec14DeletingDtor_re, ArrayRec14DeletingDtor_orig);
// 0x00066b00  Array_Rec94::dtor
SG_HOOK("jgld.dll", 0x00066B00, Array_Rec94_dtor, ArrayRec94Dtor_re, ArrayRec94Dtor_orig);
// 0x00066cb0  Array_Rec94::deletingDtor
SG_HOOK("jgld.dll", 0x00066CB0, Array_Rec94_deletingDtor, ArrayRec94DeletingDtor_re, ArrayRec94DeletingDtor_orig);
// 0x00069010  FontBase::~FontBase
SG_HOOK("jgld.dll", 0x00069010, FontBase_dtor, FontBaseDtor_re, FontBaseDtor_orig);
// 0x00069140  FontBase::deletingDtor
SG_HOOK("jgld.dll", 0x00069140, FontBase_deletingDtor, FontBaseDeletingDtor_re, FontBaseDeletingDtor_orig);
// 0x00006b80  Tracked::dtor
SG_HOOK("jgld.dll", 0x00006B80, Tracked_dtor, TrackedDtor_re, TrackedDtor_orig);
// 0x00006b10  Tracked::deletingDtor
SG_HOOK("jgld.dll", 0x00006B10, Tracked_deletingDtor, TrackedDeletingDtor_re, TrackedDeletingDtor_orig);
// 0x0006af00  PaletteBase::dtor
SG_HOOK("jgld.dll", 0x0006AF00, PaletteBase_dtor, PaletteBaseDtor_re, PaletteBaseDtor_orig);
// 0x0006af50  PaletteBase::deletingDtor
SG_HOOK("jgld.dll", 0x0006AF50, PaletteBase_deletingDtor, PaletteBaseDeletingDtor_re, PaletteBaseDeletingDtor_orig);
// 0x0006a2c0  Palette::~Palette
SG_HOOK("jgld.dll", 0x0006A2C0, Palette_dtor, PaletteDtor_re, PaletteDtor_orig);
// 0x0006a1f0  Palette::deletingDtor
SG_HOOK("jgld.dll", 0x0006A1F0, Palette_deletingDtor, PaletteDeletingDtor_re, PaletteDeletingDtor_orig);

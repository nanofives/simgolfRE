// NEGATIVE CONTROL for batch c3cw. Never list this file in shim/re_batches.txt: every body here is
// deliberately wrong in ONE detail. It is built only through log/c3/c3cw_negctl_batches.txt (which lists this
// file and not c3cw.cpp) and driven by log/c3/alt/c3cw_negctl.py, whose CSVs go to log/c3/alt.
//
//   0x00006b80 Tracked::dtor          skips removeCurrent when find returns 0 (the "sensible" version; the
//                                     original removes the current node anyway)
//   0x0006a2c0 Palette::~Palette      leaves out the clear of [[this+0x80c]+4]
//   0x00066bd0 Array_e0::deletingDtor tests bit 1 of the flags instead of bit 0
//   0x00066a30 Array_Rec14::dtor      writes the Array<e0> vtable 0x1011da58 instead of 0x1011da5c
//   0x00069010 FontBase::~FontBase    writes the Font vtable 0x1011da6c instead of 0x1011da90
// Each must read RED, and every defect but the flag test changes state only, not the return value.
#include "hooks.h"

namespace {

char* JgldBase() {
    static char* base = 0;
    if (!base) base = reinterpret_cast<char*>(GetModuleHandleA("jgld.dll"));
    return base;
}
void* JgldVA(unsigned va) { return JgldBase() + (va - 0x10000000); }
void SetVptr(void* self, unsigned vtableVA) { *static_cast<void**>(self) = JgldVA(vtableVA); }

typedef void(__cdecl* OperatorDelete_t)(void*);
typedef void(__fastcall* Method_t)(void*, void*);
typedef int(__fastcall* Find_t)(void*, void*, void*);
void CallMethod(unsigned va, void* self) { reinterpret_cast<Method_t>(JgldVA(va))(self, 0); }
typedef void*(__fastcall* DeletingDtor_t)(void*, void*, unsigned);
typedef void(__fastcall* Dtor_t)(void*, void*);

// 0x00006b80  Tracked::dtor (WRONG: conditional removeCurrent)
Dtor_t TrackedDtor_orig;
void __fastcall TrackedDtor_re(void* self, void*) {
    SetVptr(self, 0x1011d058);
    void* list = JgldVA(0x10128428);
    if (reinterpret_cast<Find_t>(JgldVA(0x100072b0))(list, 0, self))
        CallMethod(0x10007100, list);
}

// 0x0006a2c0  Palette::~Palette (WRONG: no +0x80c clear)
Dtor_t PaletteDtor_orig;
void __fastcall PaletteDtor_re(void* self, void*) {
    SetVptr(self, 0x1011db10);
    CallMethod(0x1006a380, self);
    CallMethod(0x1006af00, self);
}

// 0x00066bd0  Array_e0::deletingDtor (WRONG: bit 1)
DeletingDtor_t ArrayE0DeletingDtor_orig;
void* __fastcall ArrayE0DeletingDtor_re(void* self, void*, unsigned flags) {
    CallMethod(0x100669e0, self);
    if (flags & 2)
        reinterpret_cast<OperatorDelete_t>(JgldVA(0x1007e6d0))(self);
    return self;
}

// 0x00066a30  Array_Rec14::dtor (WRONG: Array<e0> vtable)
Dtor_t ArrayRec14Dtor_orig;
void __fastcall ArrayRec14Dtor_re(void* self, void*) {
    SetVptr(self, 0x1011da58);
    CallMethod(0x10068100, self);
}

// 0x00069010  FontBase::~FontBase (WRONG: Font vtable)
Dtor_t FontBaseDtor_orig;
void __fastcall FontBaseDtor_re(void* self, void*) { SetVptr(self, 0x1011da6c); }

}  // namespace

// 0x00006b80  Tracked::dtor
SG_HOOK("jgld.dll", 0x00006B80, Tracked_dtor, TrackedDtor_re, TrackedDtor_orig);
// 0x0006a2c0  Palette::~Palette
SG_HOOK("jgld.dll", 0x0006A2C0, Palette_dtor, PaletteDtor_re, PaletteDtor_orig);
// 0x00066bd0  Array_e0::deletingDtor
SG_HOOK("jgld.dll", 0x00066BD0, Array_e0_deletingDtor, ArrayE0DeletingDtor_re, ArrayE0DeletingDtor_orig);
// 0x00066a30  Array_Rec14::dtor
SG_HOOK("jgld.dll", 0x00066A30, Array_Rec14_dtor, ArrayRec14Dtor_re, ArrayRec14Dtor_orig);
// 0x00069010  FontBase::~FontBase
SG_HOOK("jgld.dll", 0x00069010, FontBase_dtor, FontBaseDtor_re, FontBaseDtor_orig);

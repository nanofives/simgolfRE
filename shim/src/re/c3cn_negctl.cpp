// NEGATIVE CONTROL for batch c3cn's heap stand-in. NEVER list this file in shim/re_batches.txt: every
// body here is DELIBERATELY WRONG, and only in the heap operation, so that a diff_hook run proves the
// stand-in's arena and free log are really compared state and not decoration.
//
// It is built instead of c3bb.cpp and c3cd.cpp (which hold the correct bodies for these three
// addresses) through log/c3/c3cn_negctl_batches.txt; the run is driven by log/c3/alt/c3cn_negctl.py
// and its CSVs go to log/c3/alt, never to log/diff.
//
//   0x000681a0  Array_Rec14::add   correct except that the old buffer is NOT handed to operator delete
//                                  -> the free log must differ from the original's
//   0x00068400  Array_Rec94::add   correct except that operator new is asked for ONE ELEMENT TOO MANY
//                                  -> the arena cursor and the block layout must differ
//   0x00003600  Matrix::deletingDtor  correct except that the guarded operator delete is skipped
//                                  -> the free log must stay empty where the original records `this`
//
// Addresses are RVAs; cited instructions are VAs (0x10000000 + RVA).
#include "hooks.h"

namespace {

char* JgldBase() {
    static char* base = 0;
    if (!base) base = reinterpret_cast<char*>(GetModuleHandleA("jgld.dll"));
    return base;
}
void* JgldVA(unsigned va) { return JgldBase() + (va - 0x10000000); }

typedef void*(__cdecl* Memcpy_t)(void*, const void*, unsigned);
typedef void*(__cdecl* New_t)(unsigned);
typedef void(__cdecl* Delete_t)(void*);
typedef void(__fastcall* Dtor_t)(void*, void*);

Memcpy_t JgldMemcpy() { return reinterpret_cast<Memcpy_t>(JgldVA(0x1007f3a0)); }
New_t JgldNew() { return reinterpret_cast<New_t>(JgldVA(0x1007f0f0)); }
Delete_t JgldDelete() { return reinterpret_cast<Delete_t>(JgldVA(0x1007e6d0)); }

struct ArrayHdr {
    void* vtbl;
    void* m_data;
    int m_cap;
    int m_count;
    int m_grow;
};
struct Rec14 { int v[5]; };
struct Rec94 { int v[0x25]; };

// 0x000681a0  Array_Rec14::add  (WRONG: the operator delete at 0x10068287 is left out)
typedef int(__fastcall* Add14_t)(void*, void*, Rec14);
Add14_t Add14_orig;
int __fastcall Add14_re(void* self, void*, Rec14 v) {
    ArrayHdr* a = static_cast<ArrayHdr*>(self);
    if (a->m_count < a->m_cap) {
        static_cast<Rec14*>(a->m_data)[a->m_count] = v;
        a->m_count++;
    } else {
        Rec14* n = static_cast<Rec14*>(JgldNew()((a->m_cap + a->m_grow) * sizeof(Rec14)));
        a->m_cap += a->m_grow;
        if (a->m_data) JgldMemcpy()(n, a->m_data, a->m_count * sizeof(Rec14));
        n[a->m_count] = v;
        a->m_count++;
        a->m_data = n;                     // the old buffer is never released
    }
    return a->m_count - 1;
}

// 0x00068400  Array_Rec94::add  (WRONG: one element too many at the operator new of 0x1006840a)
typedef int(__fastcall* Add94_t)(void*, void*, Rec94);
Add94_t Add94_orig;
int __fastcall Add94_re(void* self, void*, Rec94 v) {
    ArrayHdr* a = static_cast<ArrayHdr*>(self);
    if (a->m_count < a->m_cap) {
        static_cast<Rec94*>(a->m_data)[a->m_count] = v;
        a->m_count++;
    } else {
        Rec94* n = static_cast<Rec94*>(JgldNew()((a->m_cap + a->m_grow + 1) * sizeof(Rec94)));
        a->m_cap += a->m_grow;
        if (a->m_data) JgldMemcpy()(n, a->m_data, a->m_count * sizeof(Rec94));
        n[a->m_count] = v;
        a->m_count++;
        JgldDelete()(a->m_data);
        a->m_data = n;
    }
    return a->m_count - 1;
}

// 0x00003600  Matrix::deletingDtor  (WRONG: the guarded operator delete at 0x10003633 is skipped)
typedef void*(__fastcall* DeletingDtor_t)(void*, void*, unsigned);
DeletingDtor_t MatrixDeletingDtor_orig;
void* __fastcall MatrixDeletingDtor_re(void* self, void*, unsigned) {
    reinterpret_cast<Dtor_t>(JgldVA(0x100032f0))(self, 0);
    return self;
}

}  // namespace

// 0x000681a0  Array_Rec14::add
SG_HOOK("jgld.dll", 0x000681A0, Array_Rec14_add, Add14_re, Add14_orig);
// 0x00068400  Array_Rec94::add
SG_HOOK("jgld.dll", 0x00068400, Array_Rec94_add, Add94_re, Add94_orig);
// 0x00003600  Matrix::deletingDtor
SG_HOOK("jgld.dll", 0x00003600, Matrix_deletingDtor, MatrixDeletingDtor_re, MatrixDeletingDtor_orig);

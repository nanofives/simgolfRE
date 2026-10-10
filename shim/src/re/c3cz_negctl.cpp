// NEGATIVE CONTROL for batch c3cz (never list this file in shim/re_batches.txt). Built only through
// log/c3/c3cz_negctl_batches.txt (which drops c3cz.cpp) by log/c3/alt/c3cz_negctl.py. Each body below is the c3cz
// reimplementation with ONE deliberate defect in its heap operation, which leaves the return value unchanged:
//   0x004a29c0 setStringField  the old string is not handed to free
//   0x00479bb0 Vec_release     the new block is requested 4 bytes larger
//   0x004a4db0 listPush        the failed-payload path does not call listRemoveHead (no node is released)
//   0x00473ae0 Surface_free    the cache is destroyed but not handed to operator delete
// Under the c3cz heap stand-in each must read RED on state alone.
#include "hooks.h"

namespace {

template <class T>
T& At(void* p, unsigned off) { return *reinterpret_cast<T*>(static_cast<char*>(p) + off); }
template <class T>
T& G(unsigned va) { return *reinterpret_cast<T*>(va); }
typedef void*(__cdecl* Malloc_t)(unsigned);
typedef void(__cdecl* Free_t)(void*);
const Malloc_t kMalloc = reinterpret_cast<Malloc_t>(0x004a504f);
const Free_t kFree = reinterpret_cast<Free_t>(0x004a5007);
typedef void(__fastcall* This0_t)(void*, void*);
typedef void(__fastcall* This1_t)(void*, void*, int);

unsigned SizeWithNul(const char* s) { unsigned n = 0; while (s[n] != 0) n++; return n + 1; }
void AppendCopy(char* dst, const char* src) {
    while (*dst != 0) dst++;
    unsigned n = SizeWithNul(src);
    for (unsigned i = 0; i < n; i++) dst[i] = src[i];
}

// 0x004a29c0  setStringField (wrong: no free of the old string)
typedef int(__fastcall* SetStringField_t)(void*, void*, const char*, int);
SetStringField_t NegSetStringField_orig;
int __fastcall NegSetStringField_re(void* self, void*, const char* text, int index) {
    char*& field = At<char*>(self, 4 * index + 0x2c);
    if (text == 0) {
        if (field != 0) field[0] = 0;
        return 0;
    }
    char* p = static_cast<char*>(kMalloc(SizeWithNul(text)));
    field = p;
    if (p == 0) return 4;
    p[0] = 0;
    AppendCopy(field, text);
    return 0;
}

// 0x00479bb0  Vec_release (wrong: 4 bytes too many)
This0_t NegVecRelease_orig;
void __fastcall NegVecRelease_re(void* self, void*) {
    int cap = At<int>(self, 0x228);
    if (cap != 0 && cap < 10) return;
    cap -= 10;
    At<int>(self, 0x228) = cap;
    if (cap == 0) {
        void* old = At<void*>(self, 0x224);
        if (old != 0) kFree(old);
        At<void*>(self, 0x224) = 0;
        return;
    }
    void** fresh = static_cast<void**>(kMalloc((static_cast<unsigned>(cap) << 2) + 4));
    for (int i = 0; i < At<int>(self, 0x22c); i++) fresh[i] = At<void**>(self, 0x224)[i];
    void* old = At<void*>(self, 0x224);
    if (old != 0) kFree(old);
    At<void**>(self, 0x224) = fresh;
}

// 0x004a4db0  listPush (wrong: no listRemoveHead on the failed payload)
typedef int(__fastcall* ListPush_t)(void*, void*, void**, unsigned, int, int);
ListPush_t NegListPush_orig;
int __fastcall NegListPush_re(void* self, void*, void** out, unsigned size, int a, int b) {
    if (out == 0) return 3;
    *out = 0;
    if (At<int>(self, 0xc) == At<int>(self, 0x10)) return 0x12;
    char* node;
    if (At<void*>(self, 4) == 0) {
        node = static_cast<char*>(kMalloc(0x14));
        At<char*>(self, 4) = node;
        if (node == 0) return 4;
    } else {
        At<char*>(At<char*>(self, 8), 0x10) = static_cast<char*>(kMalloc(0x14));
        node = At<char*>(At<char*>(self, 8), 0x10);
        if (node == 0) return 4;
    }
    At<char*>(self, 8) = node;
    At<void*>(node, 0x10) = 0;
    At<int>(At<char*>(self, 8), 4) = b;
    At<int>(At<char*>(self, 8), 0) = a;
    if (size != 0) {
        At<void*>(At<char*>(self, 8), 8) = kMalloc(size);
        char* tail = At<char*>(self, 8);
        if (At<void*>(tail, 8) == 0) return 4;
        At<unsigned>(tail, 0xc) = size;
    }
    At<int>(self, 0xc) += 1;
    *out = At<void*>(At<char*>(self, 8), 8);
    return 0;
}

// 0x00473ae0  Surface_free (wrong: the cache is not deleted)
This0_t NegSurfaceFree_orig;
void __fastcall NegSurfaceFree_re(void* self, void*) {
    void* disp = G<void*>(0x0083ad50);
    if (disp == 0) return;
    int handle = At<int>(self, 4);
    if (handle != 0) (*reinterpret_cast<This1_t*>(At<char*>(disp, 0) + 0x94))(disp, 0, handle);
    if (At<void*>(self, 8) != 0) { kFree(At<void*>(self, 8)); At<void*>(self, 8) = 0; }
    void* cache = At<void*>(self, 0xc);
    if (cache != 0) {
        reinterpret_cast<This0_t>(0x00483010)(cache, 0);
        At<void*>(self, 0xc) = 0;
    }
    for (unsigned off = 0x10; off <= 0x24; off += 4) At<unsigned>(self, off) = 0;
}

}  // namespace

// 0x004a29c0  setStringField (negative control)
SG_HOOK("golf_clean.exe", 0x004a29c0, negctl_setStringField, NegSetStringField_re, NegSetStringField_orig);
// 0x00479bb0  Vec_release (negative control)
SG_HOOK("golf_clean.exe", 0x00479bb0, negctl_Vec_release, NegVecRelease_re, NegVecRelease_orig);
// 0x004a4db0  listPush (negative control)
SG_HOOK("golf_clean.exe", 0x004a4db0, negctl_listPush, NegListPush_re, NegListPush_orig);
// 0x00473ae0  Surface_free (negative control)
SG_HOOK("golf_clean.exe", 0x00473ae0, negctl_Surface_free, NegSurfaceFree_re, NegSurfaceFree_orig);

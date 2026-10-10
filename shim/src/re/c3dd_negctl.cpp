// NEGATIVE CONTROL for batch c3dd. NEVER list this file in shim/re_batches.txt: heapAlloc 0x00474860 below is DELIBERATELY
// WRONG in exactly one detail of its _realloc 0x004a6980 call (chosen by the environment variable SG_NEG), so that a
// diff_hook run proves the realloc entry of the heap stand-in (re/frida/js/heapstandin.js) makes a wrong realloc read RED
// on state alone. The return value of every vector is unchanged by all three variants.
//   SG_NEG=1  _realloc asks for capacity + 0x410 instead of + 0x400            -> RED on the arena cursor
//   SG_NEG=2  no _realloc call: the bookkeeping grows, the block stays          -> RED on the arena / free log / block
//   SG_NEG=3  the old block is kept after a successful _realloc (stale pointer) -> RED on the block pointer in the heap
// Built INSTEAD of c3dd.cpp through log/c3/c3dd_negctl_batches.txt (driven by log/c3/alt/c3dd_negctl.py, CSVs in
// log/c3/alt/c3dd_negctl, never log/diff).
#include "hooks.h"
#include <stdlib.h>

namespace {
template <class T>
T& At(void* p, unsigned off) { return *reinterpret_cast<T*>(static_cast<char*>(p) + off); }
typedef void*(__cdecl* Realloc_t)(void*, unsigned);
const Realloc_t kRealloc = reinterpret_cast<Realloc_t>(0x004a6980);
int Variant() {
    static int v = -1;
    if (v < 0) {
        const char* e = getenv("SG_NEG");
        v = e ? atoi(e) : 0;
    }
    return v;
}
}  // namespace

typedef char*(__fastcall* NegHeapAlloc_t)(void*, void*, int);
NegHeapAlloc_t NegHeapAlloc_orig;
char* __fastcall NegHeapAlloc_re(void* self, void*, int n) {
    while (At<int>(self, 0x14) < n) {
        if (At<unsigned char>(self, 4) & 1)
            return 0;
        void* p;
        int v = Variant();
        if (v == 2) {
            p = At<void*>(self, 8);
        } else {
            p = kRealloc(At<void*>(self, 8), static_cast<unsigned>(At<int>(self, 0x10) + (v == 1 ? 0x410 : 0x400)));
            if (p == 0)
                return 0;
            if (v == 3)
                p = At<void*>(self, 8);
        }
        At<void*>(self, 8) = p;
        At<int>(self, 0x10) += 0x400;
        At<int>(self, 0x14) += 0x400;
    }
    char* r = At<char*>(self, 0xc);
    At<char*>(self, 0xc) = r + n;
    At<int>(self, 0x14) -= n;
    return r;
}

// 0x00474860  heapAlloc
SG_HOOK("golf_clean.exe", 0x00474860, heapAlloc, NegHeapAlloc_re, NegHeapAlloc_orig);

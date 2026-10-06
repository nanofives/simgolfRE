// View / Node / HotList geometry and hit-test readers of golf_clean.exe (ui, render). Hand-written from the C2
// transcriptions re/analysis/ui/<addr>_<name>.md and the disassembly (re/tools/asm2inline.py); each body cites the
// address every field offset and constant comes from. __thiscall is emulated with __fastcall (ecx = this, edx
// unused). Recursive helpers and __thiscall callees are reached through their original addresses, so a hooked
// helper runs its own reimplementation during the A/B (as shim/src/re/golf_course.cpp does).
#include "hooks.h"

namespace {

template <typename T> T& field(void* obj, unsigned off) {
    return *reinterpret_cast<T*>(reinterpret_cast<char*>(obj) + off);
}

// __thiscall callees, emulated as __fastcall (ecx = this, edx unused).
typedef void(__fastcall* ViewXY_t)(void* self, void* edx, int* x, int* y);
typedef void(__fastcall* RectFn_t)(void* self, void* edx, int* r);
typedef int(__fastcall* Contains_t)(void* self, void* edx, void* n);
// __cdecl callees.
typedef int(__cdecl* InRect_t)(int x, int y, const int* rect);
typedef int(__cdecl* ApproxDist_t)(int dx, int dy);

const ViewXY_t kToParent   = reinterpret_cast<ViewXY_t>(0x0047b170);
const ViewXY_t kFromParent = reinterpret_cast<ViewXY_t>(0x0047b200);
const ViewXY_t kToLocal    = reinterpret_cast<ViewXY_t>(0x0047b290);
const Contains_t kContains = reinterpret_cast<Contains_t>(0x0047b080);
const InRect_t kInRect     = reinterpret_cast<InRect_t>(0x00492610);
const ApproxDist_t kApproxDist = reinterpret_cast<ApproxDist_t>(0x00467170);

// 0x0047b170  View::toParent(int* x, int* y)
// Adds the view's scroll (+0x1bc/+0x1c0, read at 0x0047b179/0x0047b18d) and origin (+0x1ac/+0x1b0, read at
// 0x0047b17f/0x0047b193) to *x/*y. When the flags byte at +0x9c has bit 0x20 (test al,0x20 at 0x0047b1ab) and the
// parent pointer at +0x130 is non-null (0x0047b1af), recurses into the parent (call at 0x0047b1bb); then, when the
// flags dword at +0x9c has bit 0x8000 (test ah,0x80 at 0x0047b1c6), subtracts the parent's origin +0x1ac/+0x1b0
// (0x0047b1d3/0x0047b1e5) from *x/*y. Returns via ret 8 (two stack args, thiscall).
ViewXY_t ToParent_orig;
void __fastcall ToParent_re(void* self, void*, int* x, int* y) {
    *x += field<int>(self, 0x1bc) + field<int>(self, 0x1ac);
    *y += field<int>(self, 0x1c0) + field<int>(self, 0x1b0);
    if ((field<unsigned char>(self, 0x9c) & 0x20) && field<void*>(self, 0x130)) {
        void* parent = field<void*>(self, 0x130);
        kToParent(parent, 0, x, y);
        if (field<unsigned int>(self, 0x9c) & 0x8000) {
            *x -= field<int>(parent, 0x1ac);
            *y -= field<int>(parent, 0x1b0);
        }
    }
}

// 0x0047b200  View::fromParent(int* x, int* y)
// Mirror of toParent with the signs flipped: subtracts scroll+origin (0x0047b209..0x0047b22c), recurses into the
// parent on flags bit 0x20 with a non-null +0x130, and on flags bit 0x8000 adds the parent origin back. Same field
// offsets as toParent. Returns via ret 8.
ViewXY_t FromParent_orig;
void __fastcall FromParent_re(void* self, void*, int* x, int* y) {
    *x -= field<int>(self, 0x1bc) + field<int>(self, 0x1ac);
    *y -= field<int>(self, 0x1c0) + field<int>(self, 0x1b0);
    if ((field<unsigned char>(self, 0x9c) & 0x20) && field<void*>(self, 0x130)) {
        void* parent = field<void*>(self, 0x130);
        kFromParent(parent, 0, x, y);
        if (field<unsigned int>(self, 0x9c) & 0x8000) {
            *x += field<int>(parent, 0x1ac);
            *y += field<int>(parent, 0x1b0);
        }
    }
}

// 0x0047b0d0  View::offsetRectToParent(RECT* r)
// Returns without touching r when r is null (test at 0x0047b0da). Otherwise computes the parent offset of the
// point (0, 0) through toParent 0x0047b170 (call at 0x0047b0ee), then adds that dx to r->left (+0) and r->right
// (+8) and dy to r->top (+4) and r->bottom (+0xc) (0x0047b0f7..0x0047b116). Returns via ret 4.
RectFn_t OffsetRectToParent_orig;
void __fastcall OffsetRectToParent_re(void* self, void*, int* r) {
    if (!r) return;
    int dx = 0, dy = 0;
    kToParent(self, 0, &dx, &dy);
    r[0] += dx; r[2] += dx;   // left +0, right +8
    r[1] += dy; r[3] += dy;   // top +4, bottom +0xc
}

// 0x0047b120  View::offsetRectToLocal(RECT* r)
// As offsetRectToParent but the offset comes from toLocal 0x0047b290 (call at 0x0047b13e) instead of toParent.
// Same null check (0x0047b12a) and RECT field adds (0x0047b147..0x0047b166). Returns via ret 4.
RectFn_t OffsetRectToLocal_orig;
void __fastcall OffsetRectToLocal_re(void* self, void*, int* r) {
    if (!r) return;
    int dx = 0, dy = 0;
    kToLocal(self, 0, &dx, &dy);
    r[0] += dx; r[2] += dx;
    r[1] += dy; r[3] += dy;
}

// 0x0047b080  Node::contains(Node* n)
// Returns 0 when n is null (test at 0x0047b087). Reads the child count at +0x22c (0x0047b08d) and the child-pointer
// array at +0x224 (0x0047b099); for each child i in [0, count): returns 1 if child i equals n (cmp at 0x0047b0a2)
// or if child i contains n (recursion, call at 0x0047b0a7). Returns 0 when no child matches. Returns via ret 4.
Contains_t Contains_orig;
int __fastcall Contains_re(void* self, void*, void* n) {
    if (!n) return 0;
    int count = field<int>(self, 0x22c);
    void** children = field<void**>(self, 0x224);
    for (int i = 0; i < count; i++) {
        if (children[i] == n) return 1;
        if (kContains(children[i], 0, n)) return 1;
    }
    return 0;
}

// 0x004326a0  nearestMenuSpot(x, y)
// Scans the spot table at 0x004c7930 (one entry per 4 bytes: short sx at +0, short sy at +2, sign-extended at
// 0x004326c3/0x004326bf) until an entry's first short equals -1 (the sentinel, cmp at 0x004326f9). best starts -1
// (0x004326ae), bestD starts 0x1e = 30 (0x004326b8). For entry i the score is approxDistance(x - sx, y - sy)
// (x is the first argument, [esp+0x18] after five pushes; y the second, in ebp: corrected by the verifier's A/B)
// (0x00467170, call at 0x004326d2) multiplied by (i + 6) (0x004326d7/0x004326dd) then divided by 8 with signed
// truncation (cdq/and 7/add/sar 3 at 0x004326e0..0x004326e6). The smallest score wins (jge at 0x004326eb). A
// winning index of 6 is returned as -1 (cmp eax,6 at 0x00432706). Returns via ret (no stack args; cdecl).
typedef int(__cdecl* NearestSpot_t)(int, int);
NearestSpot_t NearestMenuSpot_orig;
int __cdecl NearestMenuSpot_re(int x, int y) {
    const short* spots = reinterpret_cast<const short*>(0x004c7930);
    int best = -1, bestD = 30, i = 0;
    do {
        int sx = spots[i * 2];
        int sy = spots[i * 2 + 1];
        int d = kApproxDist(x - sx, y - sy) * (i + 6) / 8;
        if (d < bestD) { bestD = d; best = i; }
        i++;
    } while (spots[i * 2] != -1);
    return best == 6 ? -1 : best;
}

// 0x00492a90  HotList::hitTest(x, y, int* aOut, int* bOut)
// Scans the entry array from the highest index down (count at +0x58, 0x00492a95; entries base at +0x50, 0x00492aa5;
// stride 0x20, shl 5 at 0x00492aa2). For each entry calls inRect(x, y, entry+4) (0x00492610, call at 0x00492ab3).
// On the first hit: when aOut is non-null writes the entry's dword at +0x18 to *aOut (0x00492ad9..0x00492ae5);
// when bOut is non-null writes the entry's dword at +0x14 to *bOut (0x00492aef..0x00492afb); returns the index.
// Returns -1 when no entry contains the point (0x00492aca). Returns via ret 0x10 (four stack args, thiscall).
typedef int(__fastcall* HitTest_t)(void* self, void* edx, int x, int y, int* aOut, int* bOut);
HitTest_t HitTest_orig;
int __fastcall HitTest_re(void* self, void*, int x, int y, int* aOut, int* bOut) {
    int count = field<int>(self, 0x58);
    char* entries = field<char*>(self, 0x50);
    for (int i = count - 1; i >= 0; i--) {
        char* e = entries + i * 0x20;
        if (kInRect(x, y, reinterpret_cast<const int*>(e + 4))) {
            if (aOut) *aOut = *reinterpret_cast<int*>(e + 0x18);
            if (bOut) *bOut = *reinterpret_cast<int*>(e + 0x14);
            return i;
        }
    }
    return -1;
}

// 0x00492b10  HotList::hitTestRect(x, y, int* aOut, int* bOut, Rect4* outRect)
// Identical scan and writes to hitTest 0x00492a90, and on a hit also copies the entry's 16-byte rect (entry+4) to
// *outRect when outRect is non-null (four dword copies at 0x00492b85..0x00492ba4). Returns the index or -1.
// Returns via ret 0x14 (five stack args, thiscall).
typedef int(__fastcall* HitTestRect_t)(void* self, void* edx, int x, int y, int* aOut, int* bOut, int* outRect);
HitTestRect_t HitTestRect_orig;
int __fastcall HitTestRect_re(void* self, void*, int x, int y, int* aOut, int* bOut, int* outRect) {
    int count = field<int>(self, 0x58);
    char* entries = field<char*>(self, 0x50);
    for (int i = count - 1; i >= 0; i--) {
        char* e = entries + i * 0x20;
        if (kInRect(x, y, reinterpret_cast<const int*>(e + 4))) {
            if (aOut) *aOut = *reinterpret_cast<int*>(e + 0x18);
            if (bOut) *bOut = *reinterpret_cast<int*>(e + 0x14);
            if (outRect) {
                const int* src = reinterpret_cast<const int*>(e + 4);
                outRect[0] = src[0]; outRect[1] = src[1]; outRect[2] = src[2]; outRect[3] = src[3];
            }
            return i;
        }
    }
    return -1;
}

}  // namespace

SG_HOOK("golf_clean.exe", 0x0047b170, View_toParent, ToParent_re, ToParent_orig);
SG_HOOK("golf_clean.exe", 0x0047b200, View_fromParent, FromParent_re, FromParent_orig);
SG_HOOK("golf_clean.exe", 0x0047b0d0, View_offsetRectToParent, OffsetRectToParent_re, OffsetRectToParent_orig);
SG_HOOK("golf_clean.exe", 0x0047b120, View_offsetRectToLocal, OffsetRectToLocal_re, OffsetRectToLocal_orig);
SG_HOOK("golf_clean.exe", 0x0047b080, Node_contains, Contains_re, Contains_orig);
SG_HOOK("golf_clean.exe", 0x004326a0, nearestMenuSpot, NearestMenuSpot_re, NearestMenuSpot_orig);
SG_HOOK("golf_clean.exe", 0x00492a90, HotList_hitTest, HitTest_re, HitTest_orig);
SG_HOOK("golf_clean.exe", 0x00492b10, HotList_hitTestRect, HitTestRect_re, HitTestRect_orig);

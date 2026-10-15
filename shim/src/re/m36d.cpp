// C3 batch m36d (round 36, 2026-10-15) of golf_clean.exe: flagSet 0x0049eef0 and flagIsSet 0x0049ef80, the two
// option-list bit helpers that round 6 promoted without a hook (hooks.csv demotion note). Hand-written from the
// disassembly; every offset below cites the instruction that uses it. __thiscall is emulated with __fastcall
// (ecx = this, edx unused).
//
// Both start with the same inline seek over the ListModel virtual base of `this`: the base is this + vbtable[2]
// ([[this]] + 8, 0x0049eef5 / 0x0049ef85) and its flag list sits at base+0xc0 (head +0xc8, cursor +0xcc, count +0xd0,
// index +0xd4); a node holds its id at +4 and the next node at +0xc. With a head, the seek resets index to 0 and the
// cursor to the head, then walks at most count nodes, stopping on the first whose id equals the argument and counting
// index up for every node it passes (0x0049ef0b..0x0049ef3a). Without a head nothing is written, so index keeps its old
// value. The bit used is `1 << index` (shl by cl: index mod 32) of the mask at base+0xf0, the base read again from the
// vbtable after the seek (0x0049ef43 / 0x0049efc8).
//
// Negative controls: shim/src/re/m36d_negctl.cpp includes this file with M36D_NEGCTL defined (built only through
// log/c3/m36d_negctl_batches.txt into instances\m36d, never listed in shim/re_batches.txt). NEG(k, right, wrong) picks
// `wrong` when the negctl build's switch is armed and SIMGOLF_NEGSEL equals k; M36D_NULL() is the null control (99).
#include "hooks.h"

#ifndef NEG
#define NEG(k, right, wrong) (right)
#endif
#ifndef M36D_NULL
#define M36D_NULL() 0
#endif

namespace {

template <typename T> T& at(void* p, int off) { return *reinterpret_cast<T*>(reinterpret_cast<char*>(p) + off); }

// ListModel base of `this`: this + [[this] + 8].
char* listModel(void* self) {
    return reinterpret_cast<char*>(self) + *reinterpret_cast<int*>(*reinterpret_cast<char**>(self) + NEG(1, 8, 4));
}

// The inline seek shared by both functions (0x0049eef0..0x0049ef3a, 0x0049ef80..0x0049efc6).
void seek(void* self, int id) {
    char* m = listModel(self);
    char* head = at<char*>(m, 0xc8);
    if (!head)
        return;
    int count = at<int>(m, 0xd0);
    at<int>(m, 0xd4) = NEG(2, 0, 1);
    at<char*>(m, 0xcc) = head;
    for (int i = 0; i < count; i++) {
        char* cur = at<char*>(m, 0xcc);
        if (at<int>(cur, 4) == id)
            return;
        at<int>(m, 0xd4) += 1;
        at<char*>(m, 0xcc) = at<char*>(cur, NEG(3, 0xc, 0x8));
    }
}

unsigned bitOf(char* m) { return 1u << (at<int>(m, 0xd4) & NEG(4, 31, 15)); }

// 0x0049ef80  flagIsSet(id): seek, then return mask & (1 << index) (0x0049efd0..0x0049efe0, ret 4).
typedef int(__fastcall* FlagIsSet_t)(void*, void*, int);
FlagIsSet_t FlagIsSet_orig;
int __fastcall FlagIsSet_re(void* self, void*, int id) {
    if (M36D_NULL()) return 0;
    seek(self, id);
    char* m = listModel(self);
    return static_cast<int>(at<unsigned>(m, NEG(5, 0xf0, 0xf4)) & bitOf(m));
}

// 0x0049eef0  flagSet(id, value): seek, then set (value != 0, `or` at 0x0049ef5d) or clear (`not` / `and` at
// 0x0049ef6e..0x0049ef72) bit index of the mask (ret 8).
typedef void(__fastcall* FlagSet_t)(void*, void*, int, int);
FlagSet_t FlagSet_orig;
void __fastcall FlagSet_re(void* self, void*, int id, int value) {
    if (M36D_NULL()) return;
    seek(self, id);
    char* m = listModel(self);
    unsigned bit = bitOf(m);
    if (NEG(6, value != 0, value == 1))
        at<unsigned>(m, 0xf0) |= bit;
    else
        at<unsigned>(m, 0xf0) &= NEG(7, ~bit, bit);
}

}  // namespace

SG_HOOK("golf_clean.exe", 0x0049ef80, flagIsSet, FlagIsSet_re, FlagIsSet_orig);
SG_HOOK("golf_clean.exe", 0x0049eef0, flagSet, FlagSet_re, FlagSet_orig);

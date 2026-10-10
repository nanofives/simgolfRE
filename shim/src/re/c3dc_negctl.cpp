// NEGATIVE CONTROL for batch c3dc. NEVER list this file in shim/re_batches.txt: every body below is wrong on
// purpose, in ONE detail each, to prove the c3dc A/B keys see that detail. Built only through
// log/c3/c3dc_negctl_batches.txt (log/c3/alt/c3dc_negctl.py wrong). The correct bodies are in c3dc.cpp.
//   0x00006f40 LinkedList::add     never sends the "ordered list" debug message (state: the $dbg recorder)
//   0x00014eb0 Sprite::release     forgets the 0x101284cc update of the span form (state: $gcount)
//   0x00018120 Sprite::drawN       does not restore the Palette word +0x10 (state: $spr)
//   0x0007e220 png_set_text        grows to num_text + count + 7 instead of + 8 (state: $arena, $blk)
#include "hooks.h"

namespace {

char* JgldBase() {
    static char* base = 0;
    if (!base) base = reinterpret_cast<char*>(GetModuleHandleA("jgld.dll"));
    return base;
}
void* JgldVA(unsigned va) { return JgldBase() + (va - 0x10000000); }
template <class T> T& G(unsigned va) { return *static_cast<T*>(JgldVA(va)); }
template <class T> T& F(void* obj, unsigned off) { return *reinterpret_cast<T*>(static_cast<char*>(obj) + off); }

typedef void*(__cdecl* OperatorNew_t)(unsigned);
typedef void*(__fastcall* NodeCtor_t)(void*, void*, void*, int);
typedef void(__fastcall* Add_t)(void*, void*, void*, int);
Add_t ListAdd_orig;
void __fastcall ListAdd_re(void* list, void*, void* data, int flag) {
    F<int>(list, 0x18) = 0;                                       // DEFECT: no debug message
    void* block = reinterpret_cast<OperatorNew_t>(JgldVA(0x1007f0f0))(0x14);
    void* node = 0;
    if (block)
        node = reinterpret_cast<NodeCtor_t>(JgldVA(0x10007370))(block, 0, data, static_cast<char>(flag));
    void* head = F<void*>(list, 0x10);
    if (head) {
        F<void*>(node, 4) = F<void*>(head, 4);
        F<void*>(node, 8) = head;
        F<void*>(head, 4) = node;
        F<void*>(F<void*>(node, 4), 8) = node;
    } else {
        F<void*>(list, 0x10) = node;
        F<void*>(node, 4) = node;
        F<void*>(node, 8) = node;
    }
    F<int>(list, 0xc) += 1;
    F<void*>(list, 0x14) = node;
    F<void*>(list, 4) = data;
    F<char>(list, 8) = F<char>(node, 0x10);
}

typedef void(__cdecl* Free_t)(void*);
typedef void(__fastcall* Dtor_t)(void*, void*);
Dtor_t SpriteRelease_orig;
void __fastcall SpriteRelease_re(void* self, void*) {
    Free_t jfree = reinterpret_cast<Free_t>(JgldVA(0x10014a90));
    if (F<void*>(self, 0x14) != 0) {
        const unsigned w = F<unsigned>(self, 0x30), h = F<unsigned>(self, 0x34);
        G<unsigned>(0x101284c8) -= w * h;                         // DEFECT: 0x101284cc never lowered
        jfree(F<void*>(self, 0x14));
        F<void*>(self, 0x14) = 0;
    }
    if (F<void*>(self, 0x1c) != 0) {
        jfree(F<void*>(self, 0x1c));
        F<void*>(self, 0x1c) = 0;
    }
    F<int>(self, 0x2c) = 0;
    F<int>(self, 0x30) = 0;
    F<int>(self, 0x34) = 0;
    F<int>(self, 0x18) = 0;
    F<int>(self, 0x24) = 0;
}

typedef int(__fastcall* Draw5_t)(void*, void*, void*, int, int, int, void*);
Draw5_t DrawN_orig;
int __fastcall DrawN_re(void* self, void*, void* dst, int x, int y, int z, void* pal) {
    // DEFECT: forwards to the original body after replacing the Palette word, then leaves the argument there
    // instead of the saved value (the original restores it at 0x100183a6)
    void* const saved = F<void*>(self, 0x10);
    const int r = DrawN_orig(self, 0, dst, x, y, z, pal);
    if (pal)
        F<void*>(self, 0x10) = pal;
    (void)saved;
    return r;
}

typedef void*(__cdecl* PngMalloc_t)(void*, unsigned);
typedef void(__cdecl* PngFree_t)(void*, void*);
typedef void*(__cdecl* Memcpy_t)(void*, const void*, unsigned);
typedef unsigned(__cdecl* Strlen_t)(const char*);
typedef void(__cdecl* PngSetText_t)(void*, void*, void*, int);
PngSetText_t PngSetText_orig;
void __cdecl PngSetText_re(void* png, void* info, void* textPtr, int numText) {
    if (png == 0 || info == 0 || numText == 0)
        return;
    int& num = F<int>(info, 0x30);
    int& max = F<int>(info, 0x34);
    char*& arr = F<char*>(info, 0x38);
    PngMalloc_t pngMalloc = reinterpret_cast<PngMalloc_t>(JgldVA(0x10078e60));
    if (num + numText > max) {
        if (arr != 0) {
            const int oldMax = max;
            char* old = arr;
            max = num + numText + 7;                              // DEFECT: + 7
            arr = static_cast<char*>(pngMalloc(png, static_cast<unsigned>(max) << 4));
            reinterpret_cast<Memcpy_t>(JgldVA(0x1007f3a0))(arr, old, static_cast<unsigned>(oldMax) << 4);
            reinterpret_cast<PngFree_t>(JgldVA(0x10078ed0))(png, old);
        } else {
            max = numText + 7;                                    // DEFECT: + 7
            num = 0;
            arr = static_cast<char*>(pngMalloc(png, static_cast<unsigned>(max) << 4));
        }
    }
    char* src = static_cast<char*>(textPtr);
    for (int i = 0; i < numText; i++) {
        char* slot = arr + (num << 4);
        char* entry = src + (i << 4);
        if (F<char*>(entry, 8) == 0)
            F<char*>(entry, 8) = static_cast<char*>(JgldVA(0x1011df00));
        if (*F<char*>(entry, 8) == 0) {
            F<int>(slot, 0xc) = 0;
            F<int>(slot, 0) = -1;
        } else {
            F<unsigned>(slot, 0xc) = reinterpret_cast<Strlen_t>(JgldVA(0x1007f6e0))(F<char*>(entry, 8));
            F<int>(slot, 0) = F<int>(entry, 0);
        }
        F<char*>(slot, 8) = F<char*>(entry, 8);
        F<char*>(slot, 4) = F<char*>(entry, 4);
        num += 1;
    }
}

}  // namespace

// 0x00006f40  LinkedList::add (negative control)
SG_HOOK("jgld.dll", 0x00006F40, LinkedList_add, ListAdd_re, ListAdd_orig);
// 0x00014eb0  Sprite::release (negative control)
SG_HOOK("jgld.dll", 0x00014EB0, Sprite_release, SpriteRelease_re, SpriteRelease_orig);
// 0x00018120  Sprite::drawN (negative control)
SG_HOOK("jgld.dll", 0x00018120, Sprite_drawN, DrawN_re, DrawN_orig);
// 0x0007e220  png_set_text (negative control)
SG_HOOK("jgld.dll", 0x0007E220, png_set_text, PngSetText_re, PngSetText_orig);

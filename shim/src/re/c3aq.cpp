// Batch c3aq: reimplementations inside Terrain.dll (addresses in SG_HOOK and in the `// 0x...` comments are RVAs;
// the instruction addresses cited in the comments are VAs, VA = 0x10000000 + RVA).
//
// Terrain.dll is a static import of golf_clean.exe, so it is mapped before the shim installs its hooks.
// It is a /Od /ZI /GZ debug build: every body is literal, so each reimplementation below follows the original's
// order of loads, compares and stores, and each fact carries the VA of the instruction it comes from.
//
// Tile is 0x248 bytes (stride in Terrain::resetTerrain, re/match/terrain_methods2.cpp). Only the offsets the
// functions here touch are used, and each one is cited at its instruction:
//   +0x00..+0x20 nine dwords indexed by Tile::getCorner (+0x04, +0x0c, +0x14, +0x1c are the four read by
//                Tile::maxHeight), +0x24 type, +0x28 rotation, +0x44 Faces block (first dword = count),
//   +0x208 hasPath, +0x209 connected, +0x20a path-collar flag, +0x210 WallInfo block, +0x240 variation.
#include "hooks.h"

namespace {

// Terrain.dll has base relocations, so a callee is reached through the mapped module, never a fixed VA.
void* TerrainDllRva(unsigned long rva) {
    static unsigned char* base = 0;
    if (!base) base = (unsigned char*)GetModuleHandleA("Terrain.dll");
    return base + rva;
}

// ---------------------------------------------------------------------------------------------------------------
// Tile readers (leaves)
// ---------------------------------------------------------------------------------------------------------------

// 0x00001e40  Tile::getCorner (VA 0x10001e40)
// this comes from the frame slot [ebp-4] (stored at 0x10001e5a, reloaded at 0x10001e60); the stack argument is
// loaded at 0x10001e5d and used as a DWORD index: mov eax, [ecx + eax*4] at 0x10001e63. No bounds check and no
// branch, so index 0..8 reads the nine dwords at +0x00..+0x20 and any other index reads outside the object.
typedef int(__fastcall* TileGetCorner_t)(void*, void*, int);
TileGetCorner_t TileGetCorner_orig;
int __fastcall TileGetCorner_re(void* self, void*, int i) {
    return *(int*)((unsigned char*)self + i * 4);
}

// 0x00015500  Tile::maxHeight (VA 0x10015500)
// Largest of the four dwords at +0x04 (0x10015523), +0x1c (0x10015526), +0x0c (0x1001554b) and +0x14 (0x1001554e),
// computed as three signed two-way maxima through three frame slots:
//   a = [ebp-8]  : +0x04 when +0x04 > +0x1c, else +0x1c           (jle 0x10015529)
//   b = [ebp-0xc]: +0x0c when +0x0c > +0x14, else +0x14           (jle 0x10015551)
//   result       : a when a > b, else b                           (jle 0x10015573)
// Every compare is signed (jle), so negative heights order normally.
typedef int(__fastcall* TileMaxHeight_t)(void*, void*);
TileMaxHeight_t TileMaxHeight_orig;
int __fastcall TileMaxHeight_re(void* self, void*) {
    unsigned char* t = (unsigned char*)self;
    int c0 = *(int*)(t + 0x04);
    int c1 = *(int*)(t + 0x0c);
    int c2 = *(int*)(t + 0x14);
    int c3 = *(int*)(t + 0x1c);
    int a = c0 > c3 ? c0 : c3;
    int b = c1 > c2 ? c1 : c2;
    return a > b ? a : b;
}

// 0x00013320  Tile::hasPath (VA 0x10013320)
// Loads this (0x1001333d) and returns the BYTE at +0x208 in al (mov al, [eax+0x208] at 0x10013340). The upper
// three bytes of eax are left holding this, so only al carries the result. No branch.
typedef unsigned char(__fastcall* TileHasPath_t)(void*, void*);
TileHasPath_t TileHasPath_orig;
unsigned char __fastcall TileHasPath_re(void* self, void*) {
    return *((unsigned char*)self + 0x208);
}

// 0x00013360  Tile::isConnected (VA 0x10013360)
// Same shape as hasPath one field further on: the BYTE at +0x209 (mov al, [eax+0x209] at 0x10013380). No branch.
typedef unsigned char(__fastcall* TileIsConnected_t)(void*, void*);
TileIsConnected_t TileIsConnected_orig;
unsigned char __fastcall TileIsConnected_re(void* self, void*) {
    return *((unsigned char*)self + 0x209);
}

// 0x00015340  Tile::getVariation (VA 0x10015340)
// Returns the low BYTE of the dword field at +0x28 (mov al, [eax+0x28] at 0x10015360); the caller Tile::edgeKind
// sign-extends it (movsx eax, al at 0x100156ae), so the byte is signed. No branch.
typedef char(__fastcall* TileGetVariation_t)(void*, void*);
TileGetVariation_t TileGetVariation_orig;
char __fastcall TileGetVariation_re(void* self, void*) {
    return (char)*((unsigned char*)self + 0x28);
}

// 0x000153c0  Tile::getFaces (VA 0x100153c0)
// Returns this + 0x44 (add eax, 0x44 at 0x100153e0): the address of the Tile's embedded Faces block. No branch.
typedef void*(__fastcall* TileGetFaces_t)(void*, void*);
TileGetFaces_t TileGetFaces_orig;
void* __fastcall TileGetFaces_re(void* self, void*) {
    return (unsigned char*)self + 0x44;
}

// ---------------------------------------------------------------------------------------------------------------
// Tile writers
// ---------------------------------------------------------------------------------------------------------------

// 0x000133a0  Tile::setConnected (VA 0x100133a0)
// One stack argument (ret 4 at 0x100133df), compared against 0 as a full DWORD (cmp [ebp+8], 0 / je 0x100133c1).
// Non-zero stores the byte 1 at +0x209 (0x100133c6), zero stores the byte 0 (0x100133d2). Nothing else is written.
typedef void(__fastcall* TileSetConnected_t)(void*, void*, int);
TileSetConnected_t TileSetConnected_orig;
void __fastcall TileSetConnected_re(void* self, void*, int on) {
    unsigned char* t = (unsigned char*)self;
    if (on)
        t[0x209] = 1;
    else
        t[0x209] = 0;
}

// 0x00015380  Tile::setRotation (VA 0x10015380)
// One stack argument (ret 4 at 0x100153ac) stored whole as a DWORD at +0x28 (mov [eax+0x28], ecx at 0x100153a3),
// the field Tile::getVariation 0x10015340 reads the low byte of. No branch.
typedef void(__fastcall* TileSetRotation_t)(void*, void*, int);
TileSetRotation_t TileSetRotation_orig;
void __fastcall TileSetRotation_re(void* self, void*, int r) {
    *(int*)((unsigned char*)self + 0x28) = r;
}

// 0x00002f80  Tile::setVariation (VA 0x10002f80)
// One stack argument (ret 4 at 0x10002faf) stored whole as a DWORD at +0x240 (mov [eax+0x240], ecx at
// 0x10002fa3). A different field from the one setRotation writes. No branch.
typedef void(__fastcall* TileSetVariation_t)(void*, void*, int);
TileSetVariation_t TileSetVariation_orig;
void __fastcall TileSetVariation_re(void* self, void*, int v) {
    *(int*)((unsigned char*)self + 0x240) = v;
}

// 0x00013400  Tile::layPath (VA 0x10013400)
// Two stack arguments (ret 8 at 0x100134be). The first is masked to its low byte before the test
// (and eax, 0xff at 0x10013420; test eax, eax / je 0x10013427), so 0x100 counts as "off".
//   off  (0x1001349a): stores 0 into the three bytes +0x208, +0x209, +0x20a (0x1001349d, 0x100134a7, 0x100134b1).
//   on   (0x10013429): stores 1 into +0x208; when the second argument is non-zero as a full dword
//        (cmp [ebp+0xc], 0 / je 0x10013437) it also stores 1 into +0x209 (0x1001343c);
//        then it reads the type at +0x24 and compares it with 0x16, 0, 2, 1, 3, 7, 9 in that order
//        (je 0x1001344a, 0x10013453, 0x1001345c, 0x10013465, 0x1001346e, 0x10013477, jne 0x10013480).
//        A match stores 0 into +0x20a (0x10013485), anything else stores 1 (0x10013491).
// +0x208 and +0x209 keep whatever they held when the "on" path does not set them, so an "on" call never clears
// +0x209.
typedef void(__fastcall* TileLayPath_t)(void*, void*, int, int);
TileLayPath_t TileLayPath_orig;
void __fastcall TileLayPath_re(void* self, void*, int on, int dir) {
    unsigned char* t = (unsigned char*)self;
    if (on & 0xff) {
        t[0x208] = 1;
        if (dir)
            t[0x209] = 1;
        int type = *(int*)(t + 0x24);
        if (type == 0x16 || type == 0 || type == 2 || type == 1 || type == 3 || type == 7 || type == 9)
            t[0x20a] = 0;
        else
            t[0x20a] = 1;
    } else {
        t[0x208] = 0;
        t[0x209] = 0;
        t[0x20a] = 0;
    }
}

// ---------------------------------------------------------------------------------------------------------------
// Constructors
// ---------------------------------------------------------------------------------------------------------------

// 0x00001fa0  Faces::Faces (VA 0x10001fa0)
// Stores the dword 0 at this+0 (mov [eax], 0 at 0x10001fc0), the Faces count, and returns this
// (mov eax, [ebp-4] at 0x10001fc6). No other field is touched and there is no branch.
typedef void*(__fastcall* FacesCtor_t)(void*, void*);
FacesCtor_t FacesCtor_orig;
void* __fastcall FacesCtor_re(void* self, void*) {
    *(int*)self = 0;
    return self;
}

// 0x0000f690  PathInfo::PathInfo (VA 0x1000f690)
// Calls PathInfo::clear with this in ecx (mov ecx, [ebp-4] at 0x1000f6ad; call 0x1000f6b0 of the
// incremental-link thunk 0x1000105f, which is jmp 0x1000f6e0) and returns this (mov eax, [ebp-4] at 0x1000f6b5).
// It has the /GZ frame check (call 0x100180a0 at 0x1000f6c0) and no branch of its own.
typedef void*(__fastcall* PathInfoCtor_t)(void*, void*);
PathInfoCtor_t PathInfoCtor_orig;
void* __fastcall PathInfoCtor_re(void* self, void*) {
    ((void(__fastcall*)(void*, void*))TerrainDllRva(0x0000f6e0))(self, 0);
    return self;
}

// 0x0000f750  WallInfo::WallInfo (VA 0x1000f750)
// Same shape: calls WallInfo::clear with this in ecx (mov ecx, [ebp-4] at 0x1000f76d; call 0x1000f770 of the
// thunk 0x100011c7, which is jmp 0x1000f7a0) and returns this (0x1000f775). /GZ frame check at 0x1000f780.
typedef void*(__fastcall* WallInfoCtor_t)(void*, void*);
WallInfoCtor_t WallInfoCtor_orig;
void* __fastcall WallInfoCtor_re(void* self, void*) {
    ((void(__fastcall*)(void*, void*))TerrainDllRva(0x0000f7a0))(self, 0);
    return self;
}

// ---------------------------------------------------------------------------------------------------------------
// Free functions (__cdecl)
// ---------------------------------------------------------------------------------------------------------------

// 0x000058a0  factorial (VA 0x100058a0)
// __cdecl, one stack argument (ret with no immediate at 0x100058fc).
// n == 0 returns 1 immediately (cmp [ebp+8], 0 / jne 0x100058bc; mov eax, 1 at 0x100058be).
// Otherwise the result [ebp-4] and the counter [ebp-8] both start at n (0x100058c8, 0x100058ce) and the loop
// multiplies the result by counter-1 while the counter is greater than 2 (cmp [ebp-8], 2 / jle 0x100058e0;
// sub eax, 1 at 0x100058e5; imul ecx, eax at 0x100058eb), decrementing the counter at the top of each turn
// (sub edx, 1 at 0x100058d6). The compare is signed, so every n <= 2 (including every negative n) returns n
// unchanged, and the multiplications wrap on 32-bit overflow.
typedef int(__cdecl* Factorial_t)(int);
Factorial_t Factorial_orig;
int __cdecl Factorial_re(int n) {
    int r, i;
    if (n == 0)
        return 1;
    r = n;
    for (i = n; i > 2; i--)
        r = (int)((unsigned int)r * (unsigned int)(i - 1));
    return r;
}

// 0x00001b60  swapRedBlue (VA 0x10001b60)
// __cdecl, two stack arguments: the pixel bytes [ebp+8] and an ImageInfo [ebp+0xc] whose dword at +4 is the width
// and dword at +8 the height (mov edx, [eax+4] at 0x10001b85; imul edx, [ecx+8] at 0x10001b88). The pixel counter
// [ebp-8] is width*height (signed imul, so it wraps) and the cursor [ebp-4] starts at the pixel pointer
// (0x10001b92). While the counter is greater than 0 (cmp [ebp-8], 0 / jle 0x10001bad) the first and third byte of
// the current pixel are exchanged through the temporary [ebp-0xc] (load 0x10001bb2, store of q[2] into q[0] at
// 0x10001bc0, store of the temporary into q[2] at 0x10001bc8); then the counter drops by 1 (0x10001b9a) and the
// cursor advances by 3 (add edx, 3 at 0x10001ba3). The middle byte is never touched, and a counter that is zero
// or negative leaves the buffer untouched. The dword at ImageInfo+0 is not read.
typedef void(__cdecl* SwapRedBlue_t)(unsigned char*, int*);
SwapRedBlue_t SwapRedBlue_orig;
void __cdecl SwapRedBlue_re(unsigned char* pixels, int* info) {
    int n = (int)((unsigned int)info[1] * (unsigned int)info[2]);
    unsigned char* q = pixels;
    for (; n > 0; n--, q += 3) {
        unsigned char t = q[0];
        q[0] = q[2];
        q[2] = t;
    }
}

// ---------------------------------------------------------------------------------------------------------------
// std::list<Tile*> node construction (allocation free: the placement form of operator new)
// ---------------------------------------------------------------------------------------------------------------

// 0x0000bab0  operator new (placement form, void* operator new(size_t, void*)) (VA 0x1000bab0)
// __cdecl, two stack arguments, no call of any kind in the body: it returns the second argument unchanged
// (mov eax, [ebp+0xc] at 0x1000bac8) and never looks at the size in [ebp+8]. No branch, no heap.
typedef void*(__cdecl* OperatorNewPlacement_t)(unsigned int, void*);
OperatorNewPlacement_t OperatorNewPlacement_orig;
void* __cdecl OperatorNewPlacement_re(unsigned int, void* p) {
    return p;
}

// 0x0000ba40  std::_Construct (VA 0x1000ba40)
// __cdecl, two stack arguments, no return value. It pushes the first argument and the constant 4 (0x1000ba5b,
// 0x1000ba5c) and calls the placement operator new 0x1000bab0 through the incremental-link thunk 0x10001069
// (call at 0x1000ba5e, the 8 bytes of arguments removed by the caller at 0x1000ba63), so the pointer it stores in
// [ebp-4] is the first argument. When that pointer is not NULL (cmp [ebp-4], 0 / je 0x1000ba6d) it copies one
// dword from the object the second argument points at into it (load [edx] at 0x1000ba75, store [ecx] at
// 0x1000ba77); a NULL pointer skips the copy and never dereferences the second argument.
// The value of the new-expression goes into the frame slot [ebp-8] (0x1000ba7c in the non-NULL path, 0 at
// 0x1000ba81) and is then discarded: no instruction loads it into eax before the epilogue, so eax on return holds
// the last value computed (the copied dword, or the NULL pointer), which is why the reimplementation returns void
// and the A/B compares the written memory only. /GZ frame check at 0x1000ba90.
typedef void(__cdecl* StdConstruct_t)(void*, void*);
StdConstruct_t StdConstruct_orig;
void __cdecl StdConstruct_re(void* dest, void* src) {
    void* p = ((OperatorNewPlacement_t)TerrainDllRva(0x0000bab0))(4, dest);
    if (p != 0)
        *(int*)p = *(int*)src;
}

}  // namespace

SG_HOOK("Terrain.dll", 0x00001e40, Tile_getCorner, TileGetCorner_re, TileGetCorner_orig);
SG_HOOK("Terrain.dll", 0x00015500, Tile_maxHeight, TileMaxHeight_re, TileMaxHeight_orig);
SG_HOOK("Terrain.dll", 0x00013320, Tile_hasPath, TileHasPath_re, TileHasPath_orig);
SG_HOOK("Terrain.dll", 0x00013360, Tile_isConnected, TileIsConnected_re, TileIsConnected_orig);
SG_HOOK("Terrain.dll", 0x00015340, Tile_getVariation, TileGetVariation_re, TileGetVariation_orig);
SG_HOOK("Terrain.dll", 0x000153c0, Tile_getFaces, TileGetFaces_re, TileGetFaces_orig);
SG_HOOK("Terrain.dll", 0x000133a0, Tile_setConnected, TileSetConnected_re, TileSetConnected_orig);
SG_HOOK("Terrain.dll", 0x00015380, Tile_setRotation, TileSetRotation_re, TileSetRotation_orig);
SG_HOOK("Terrain.dll", 0x00002f80, Tile_setVariation, TileSetVariation_re, TileSetVariation_orig);
SG_HOOK("Terrain.dll", 0x00013400, Tile_layPath, TileLayPath_re, TileLayPath_orig);
SG_HOOK("Terrain.dll", 0x00001fa0, Faces_ctor, FacesCtor_re, FacesCtor_orig);
SG_HOOK("Terrain.dll", 0x0000f690, PathInfo_ctor, PathInfoCtor_re, PathInfoCtor_orig);
SG_HOOK("Terrain.dll", 0x0000f750, WallInfo_ctor, WallInfoCtor_re, WallInfoCtor_orig);
SG_HOOK("Terrain.dll", 0x000058a0, factorial, Factorial_re, Factorial_orig);
SG_HOOK("Terrain.dll", 0x00001b60, swapRedBlue, SwapRedBlue_re, SwapRedBlue_orig);
SG_HOOK("Terrain.dll", 0x0000bab0, operator_new_placement, OperatorNewPlacement_re, OperatorNewPlacement_orig);
SG_HOOK("Terrain.dll", 0x0000ba40, std_Construct, StdConstruct_re, StdConstruct_orig);

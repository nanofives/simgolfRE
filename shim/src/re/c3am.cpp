// Batch c3am: reimplementations inside Terrain.dll (addresses are RVAs; the module's VAs are 0x10000000 + RVA).
// Terrain.dll is a static import of golf_clean.exe, so it is mapped before the shim installs its hooks and these
// hooks go in at SgHooksInstall time (re/hooks.cpp).
//
// Terrain.dll is a /Od /ZI /GZ debug build: every body below is the literal instruction sequence of the original,
// with the VA of each load/store in the comment. The Tile object is 0x248 bytes (stride in Terrain::resetTerrain,
// re/match/terrain_methods2.cpp); the offsets used here come only from the instructions cited per function:
//   +0x24 type, +0x2c x, +0x30 y, +0x34 four neighbour pointers, +0x208 PathInfo (7 bytes),
//   +0x210 nine wall heights (dwords), +0x234 nine wall flags (bytes).
#include "hooks.h"

namespace {

// Terrain.dll has base relocations, so a callee is reached through the mapped module, never a fixed VA.
void* TerrainDllRva(unsigned long rva) {
    static unsigned char* base = 0;
    if (!base) base = (unsigned char*)GetModuleHandleA("Terrain.dll");
    return base + rva;
}

// 0x00001f60  Tile::getType (VA 0x10001f60)
// Loads this from the frame slot ([ebp-4], 0x10001f7d) and returns the dword at +0x24 (0x10001f80). No branch.
typedef int(__fastcall* TileGetType_t)(void*, void*);
TileGetType_t TileGetType_orig;
int __fastcall TileGetType_re(void* self, void*) {
    return *(int*)((unsigned char*)self + 0x24);
}

// 0x00005960  Tile::getX (VA 0x10005960)
// Returns the dword at this+0x2c (0x10005980). No branch.
typedef int(__fastcall* TileGetX_t)(void*, void*);
TileGetX_t TileGetX_orig;
int __fastcall TileGetX_re(void* self, void*) {
    return *(int*)((unsigned char*)self + 0x2c);
}

// 0x00006810  Tile::getY (VA 0x10006810)
// Returns the dword at this+0x30 (0x10006830). No branch.
typedef int(__fastcall* TileGetY_t)(void*, void*);
TileGetY_t TileGetY_orig;
int __fastcall TileGetY_re(void* self, void*) {
    return *(int*)((unsigned char*)self + 0x30);
}

// 0x00015460  Tile::isHidden (VA 0x10015460)
// Compares the dword at this+0x24 with 0x14 (cmp at 0x10015482) and returns the flag through sete cl / mov al, cl
// (0x10015486, 0x10015489): 1 when the type is 0x14, 0 otherwise. Branchless.
typedef unsigned char(__fastcall* TileIsHidden_t)(void*, void*);
TileIsHidden_t TileIsHidden_orig;
unsigned char __fastcall TileIsHidden_re(void* self, void*) {
    return (unsigned char)(*(int*)((unsigned char*)self + 0x24) == 0x14);
}

// 0x000154a0  Tile::isSolidType (VA 0x100154a0)
// this is stored in the frame slot (0x100154ba) and never read. The argument is compared with 9 (je at
// 0x100154c1) and then with 0 (je at 0x100154c7); both of those jump to the store of 0 (0x100154d2), every other
// value stores 1 (0x100154c9). The result byte is returned from the dword local (mov al, [ebp-8], 0x100154d9).
typedef unsigned char(__fastcall* TileIsSolidType_t)(void*, void*, int);
TileIsSolidType_t TileIsSolidType_orig;
unsigned char __fastcall TileIsSolidType_re(void*, void*, int type) {
    int r;
    if (type != 9 && type != 0)
        r = 1;
    else
        r = 0;
    return (unsigned char)r;
}

// 0x00001ed0  Tile::getWall (VA 0x10001ed0)
// Adds the argument to this as a BYTE offset (add eax, [ebp+8] at 0x10001ef0) and returns the byte at +0x234
// (mov al, [eax+0x234] at 0x10001ef3). No bounds check and no branch.
typedef unsigned char(__fastcall* TileGetWall_t)(void*, void*, int);
TileGetWall_t TileGetWall_orig;
unsigned char __fastcall TileGetWall_re(void* self, void*, int side) {
    return *((unsigned char*)self + side + 0x234);
}

// 0x00001e80  Terrain::getWall (VA 0x10001e80)
// this is stored in the frame slot (0x10001e9a) and never read. The second argument is pushed (0x10001e9d), the
// first is loaded into ecx (0x10001ea1) and Tile::getWall is called through the incremental-link thunk at
// 0x100012da (call at 0x10001ea4); its return byte is the return value. Only callee: Tile::getWall 0x10001ed0.
typedef unsigned char(__fastcall* TerrainGetWall_t)(void*, void*, void*, int);
TerrainGetWall_t TerrainGetWall_orig;
unsigned char __fastcall TerrainGetWall_re(void*, void*, void* tile, int side) {
    return ((TileGetWall_t)TerrainDllRva(0x00001ed0))(tile, 0, side);
}

// 0x00015400  Tile::setWall (VA 0x10015400)
// Three stack arguments (ret 0xc at 0x10015442). The third is read as a BYTE from [ebp+0x10] (0x10015423) and
// stored at this + arg1 + 0x234, with arg1 again used as a byte offset (add eax, [ebp+8] at 0x10015420, store at
// 0x10015426). The second argument is then stored as a dword at this + arg1*4 + 0x210 (0x10015435). No branch.
typedef void(__fastcall* TileSetWall_t)(void*, void*, int, int, int);
TileSetWall_t TileSetWall_orig;
void __fastcall TileSetWall_re(void* self, void*, int side, int height, int on) {
    unsigned char* t = (unsigned char*)self;
    *(t + side + 0x234) = (unsigned char)on;
    *(int*)(t + side * 4 + 0x210) = height;
}

// 0x0000c520  Tile::setNeighbour (VA 0x1000c520)
// Stores the second argument as a dword at this + arg1*4 + 0x34 (mov [ecx+eax*4+0x34], edx at 0x1000c546).
// No branch, no null check.
typedef void(__fastcall* TileSetNeighbour_t)(void*, void*, int, void*);
TileSetNeighbour_t TileSetNeighbour_orig;
void __fastcall TileSetNeighbour_re(void* self, void*, int i, void* n) {
    *(void**)((unsigned char*)self + i * 4 + 0x34) = n;
}

// 0x0000f6e0  PathInfo::clear (VA 0x1000f6e0)
// Zeroes seven bytes of the PathInfo block (Tile+0x208) in the order +1, +0, +2, +3, +4, +5, +6
// (0x1000f700, 0x1000f707, 0x1000f70d, 0x1000f714, 0x1000f71b, 0x1000f722, 0x1000f729). No branch, no return value.
typedef void(__fastcall* PathInfoClear_t)(void*, void*);
PathInfoClear_t PathInfoClear_orig;
void __fastcall PathInfoClear_re(void* self, void*) {
    unsigned char* p = (unsigned char*)self;
    p[1] = 0;
    p[0] = 0;
    p[2] = 0;
    p[3] = 0;
    p[4] = 0;
    p[5] = 0;
    p[6] = 0;
}

// 0x0000f7a0  WallInfo::clear (VA 0x1000f7a0)
// Zeroes FOUR of the nine wall flag bytes of the WallInfo block (Tile+0x210): +0x24, +0x26, +0x2a, +0x28, in that
// order (0x1000f7c0, 0x1000f7c7, 0x1000f7ce, 0x1000f7d5). The nine wall heights at +0x00..+0x23 and the flags at
// +0x25, +0x27, +0x29, +0x2b, +0x2c are left untouched. No branch, no return value.
typedef void(__fastcall* WallInfoClear_t)(void*, void*);
WallInfoClear_t WallInfoClear_orig;
void __fastcall WallInfoClear_re(void* self, void*) {
    unsigned char* w = (unsigned char*)self;
    w[0x24] = 0;
    w[0x26] = 0;
    w[0x2a] = 0;
    w[0x28] = 0;
}

// 0x000158b0  iabs (VA 0x100158b0)
// __cdecl, one stack argument, no /GZ frame fill (the first function of the CRT block). jl at 0x100158b8 sends
// negative values to neg ecx (0x100158c5); zero and positive values are copied unchanged (0x100158ba). The result
// goes through the local at [ebp-4] and is returned in eax (0x100158ca). neg of 0x80000000 is 0x80000000, so that
// input is returned unchanged.
typedef int(__cdecl* Iabs_t)(int);
Iabs_t Iabs_orig;
int __cdecl Iabs_re(int v) {
    int r;
    if (v < 0)
        r = (int)(0u - (unsigned int)v);
    else
        r = v;
    return r;
}

}  // namespace

SG_HOOK("Terrain.dll", 0x00001f60, Tile_getType, TileGetType_re, TileGetType_orig);
SG_HOOK("Terrain.dll", 0x00005960, Tile_getX, TileGetX_re, TileGetX_orig);
SG_HOOK("Terrain.dll", 0x00006810, Tile_getY, TileGetY_re, TileGetY_orig);
SG_HOOK("Terrain.dll", 0x00015460, Tile_isHidden, TileIsHidden_re, TileIsHidden_orig);
SG_HOOK("Terrain.dll", 0x000154a0, Tile_isSolidType, TileIsSolidType_re, TileIsSolidType_orig);
SG_HOOK("Terrain.dll", 0x00001ed0, Tile_getWall, TileGetWall_re, TileGetWall_orig);
SG_HOOK("Terrain.dll", 0x00001e80, Terrain_getWall, TerrainGetWall_re, TerrainGetWall_orig);
SG_HOOK("Terrain.dll", 0x00015400, Tile_setWall, TileSetWall_re, TileSetWall_orig);
SG_HOOK("Terrain.dll", 0x0000c520, Tile_setNeighbour, TileSetNeighbour_re, TileSetNeighbour_orig);
SG_HOOK("Terrain.dll", 0x0000f6e0, PathInfo_clear, PathInfoClear_re, PathInfoClear_orig);
SG_HOOK("Terrain.dll", 0x0000f7a0, WallInfo_clear, WallInfoClear_re, WallInfoClear_orig);
SG_HOOK("Terrain.dll", 0x000158b0, iabs, Iabs_re, Iabs_orig);

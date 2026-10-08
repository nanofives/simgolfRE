// Batch c3ba: Terrain.dll, round 14. The addresses in SG_HOOK and in the `// 0x...` comment lines are RVAs;
// every instruction address cited in a comment is a VA (VA = 0x10000000 + RVA), the form the debug build's
// disassembly prints. Terrain.dll is a static import of golf_clean.exe, so it is mapped before the shim
// installs its hooks, and it loads at its preferred base (measured: 0x10000000, log/c3/alt/c3ba_probe.py).
// It is a /Od /ZI /GZ build: every body is literal, so each reimplementation below follows the original's
// order of loads, compares, calls and stores, and each fact carries the VA it comes from.
//
// __thiscall is emulated with __fastcall (ecx = this, edx unused).
//
// Seven functions, the terrain-geometry group around the vertex and normal arrays:
//   Tile::elevateCorner, Tile::lowerCorner  - raise/lower one tile corner and the vertices around it
//   Tile::reset                             - put a tile back to its initial state
//   initGrid                                - lay out the whole vertex/normal grid
//   Tile::setFaceTexture, setTextureId      - the texture table
//   Tile::calcNormals                       - re-sum the vertex normal of a tile
//
// THE TWO MODULE ARRAYS
// --------------------
// g_vertices 0x100b28c8 and g_normals 0x10070a18 are arrays of three floats (stride 0xc). Five of the seven
// bodies below write them, so they are not our own memory. They are safe to A/B at the main menu, where
// diff_hook runs: log/c3/alt/c3ba_probe.py attached to thirteen Terrain.dll entry points for 8 s at the menu
// and counted zero calls into any of them, and sampled the first 0x600 bytes of g_vertices, g_normals and
// g_textures 1843 times without seeing one byte change. The registry puts the windows the vectors touch in
// the state regions, so diff_hook snapshots and restores them around every vector.
//
// x87: the originals are x87 code, this file is compiled by MSVC 2022 x86 (SSE2). The control word on the
// A/B thread is 0x027f (PC = 53-bit, RC = nearest; measured by the same probe), so each x87 operation rounds
// like a `double` operation. /Od makes every rounding point visible as an `fstp dword` (float) store: each one
// below is an assignment to a float, and inside one expression every operand is widened with a (double) cast
// so C++ never rounds in between. Float constants are read from Terrain.dll's own data at the addresses the
// originals use.
#include "hooks.h"

namespace {

// Terrain.dll could be rebased, so its data and callees are reached through the mapped module.
unsigned char* TerrainBase() {
    static unsigned char* base = 0;
    if (!base) base = (unsigned char*)GetModuleHandleA("Terrain.dll");
    return base;
}
void* TerrainRva(unsigned long rva) { return TerrainBase() + rva; }

// A float constant (or a float module global) of Terrain.dll, by VA.
float TFloat(unsigned va) { return *(const float*)(TerrainBase() + (va - 0x10000000u)); }

// g_vertices 0x100b28c8 and g_normals 0x10070a18, component `c` of entry `idx`. The originals compute the
// byte offset with `imul <idx>, 0xc` and address `[<reg> + <array base>]`, i.e. the product is taken modulo
// 2^32 and added to the base; the unsigned arithmetic here is the same computation.
float* Vertex(int idx, unsigned c) {
    return (float*)(TerrainBase() + 0x000b28c8u + (unsigned)idx * 0xcu + c * 4u);
}
float* Normal(int idx, unsigned c) {
    return (float*)(TerrainBase() + 0x00070a18u + (unsigned)idx * 0xcu + c * 4u);
}

// Tile fields used below, all from the instructions cited in each function.
//   +0x00  int   corner[8]       the per-corner height in steps (only the odd indices are used)
//   +0x24  int   type
//   +0x28  int   sub             (rotation / sub-type / flag word, depending on type)
//   +0x2c  int   x, +0x30 int y
//   +0x34  Tile* n[4]            four neighbours
//   +0x48  Face  face[8]         stride 0x38: int v[3] at +0, float normal[3] at +0x2c
//   +0x208 PathInfo (7 bytes), +0x210 WallInfo (0x30 bytes)
//   +0x240 int   variation
//   +0x244 byte  normalsDirty
int*   TileCorner(void* t, int i)       { return (int*)((unsigned char*)t + (unsigned)i * 4u); }
int    TileType(void* t)                { return *(const int*)((unsigned char*)t + 0x24); }
int    TileSub(void* t)                 { return *(const int*)((unsigned char*)t + 0x28); }
void** TileNeighbour(void* t, int i)    { return (void**)((unsigned char*)t + 0x34 + i * 4); }
// face f, vertex slot v: Tile + 0x48 + f * 0x38 + v * 4
int    FaceVertex(void* t, int f, int v) {
    return *(const int*)((unsigned char*)t + 0x48 + (unsigned)f * 0x38u + (unsigned)v * 4u);
}
// face f, normal component c: Tile + 0x74 + f * 0x38 + c * 4
float  FaceNormalC(void* t, int f, unsigned c) {
    return *(const float*)((unsigned char*)t + 0x74 + (unsigned)f * 0x38u + c * 4u);
}
unsigned char* TileDirty(void* t)       { return (unsigned char*)t + 0x244; }

// ===================================================================================================
// Tile::elevateCorner / Tile::lowerCorner
// ===================================================================================================
// The two bodies are the same shape. `corner` picks six (face, slot) pairs out of a switch; the corner's own
// height counter goes up (down) by one step of g_splineHeight 0x10063e54; the corner vertex takes the new
// height, two edge midpoints take the mean of the new height with the far end of their edge, and the tile
// centre takes the larger (smaller) of its old height and the mean with one more vertex.
//
// THE SWITCH SELECTS SIX (face, slot) PAIRS, AND HAS NO SAFE DEFAULT
// The argument is copied to [ebp-0x50] and decremented (elevate 0x1000c7d3-0x1000c7df), bounded UNSIGNED
// against 6 (cmp 0x1000c7e2, ja 0x1000c7e6) and dispatched through the DWORD table at 0x1000cb9b
// (jmp 0x1000c7ef). The table holds, for indices 0..6:
//     0x1000c7f6 (corner 1), 0x1000c952, 0x1000c8a8 (corner 3), 0x1000c952,
//     0x1000c8fe (corner 5), 0x1000c952, 0x1000c84f (corner 7)
// so only the odd corners 1, 3, 5, 7 set the twelve frame slots [ebp-8] .. [ebp-0x34]. Every other argument
// reaches 0x1000c952 with those slots still holding the /GZ fill 0xcccccccc (written by the `rep stosd` at
// 0x1000c7cd), and the first use, `imul eax, eax, 0x38` at 0x1000c970 followed by the load at 0x1000c97d,
// then reads at this + 0xcccccca0 + 0x48. That faults in the original as much as in a reimplementation, so
// the vectors only pass 1, 3, 5 and 7, and the reimplementation reproduces the four defined cases.
struct CornerPairs { int f5, v5, f0, v0, f1, v1, f2, v2, f3, v3, f4, v4; };

// elevate 0x1000c7f6 (corner 1), 0x1000c84f (7), 0x1000c8a8 (3), 0x1000c8fe (5); lower 0x1000cd06, 0x1000cd5f,
// 0x1000cdb8, 0x1000ce0e - the twelve stores of each block, in the frame-slot order
//   [ebp-0x30]=f5 [ebp-0x34]=v5 [ebp-8]=f0 [ebp-0xc]=v0 [ebp-0x10]=f1 [ebp-0x14]=v1
//   [ebp-0x18]=f2 [ebp-0x1c]=v2 [ebp-0x20]=f3 [ebp-0x24]=v3 [ebp-0x28]=f4 [ebp-0x2c]=v4
bool CornerPairsFor(int corner, CornerPairs* p) {
    switch (corner) {
        case 1: { CornerPairs q = {3, 2, 4, 1, 3, 1, 1, 2, 7, 1, 0, 0}; *p = q; return true; }
        case 7: { CornerPairs q = {0, 0, 7, 1, 1, 2, 0, 1, 3, 2, 4, 1}; *p = q; return true; }
        case 3: { CornerPairs q = {7, 1, 0, 0, 5, 1, 3, 1, 4, 1, 3, 2}; *p = q; return true; }
        case 5: { CornerPairs q = {4, 1, 3, 2, 0, 1, 5, 1, 0, 0, 7, 1}; *p = q; return true; }
        default: return false;                       // the /GZ-fill path described above
    }
}

// The eight normals-dirty stores both functions end with (elevate 0x1000caa8-0x1000cb91,
// lower 0x1000d1d3-0x1000d2bc). Two of them index a neighbour of a neighbour after testing only THIS tile's
// pointer: `if (n[0] && n[2])` dereferences n[0]->n[2] (elevate 0x1000cada-0x1000cae3), not this->n[2].
void MarkNeighbourNormals(void* self) {
    *TileDirty(self) = 1;                                             // 0x1000caab
    void* n0 = *TileNeighbour(self, 0);
    void* n1 = *TileNeighbour(self, 1);
    void* n2 = *TileNeighbour(self, 2);
    void* n3 = *TileNeighbour(self, 3);
    if (n0) *TileDirty(n0) = 1;                                       // je 0x1000cab9
    if (n0 && n2) *TileDirty(*TileNeighbour(n0, 2)) = 1;              // je 0x1000cacf, je 0x1000cad8
    if (n0 && n3) *TileDirty(*TileNeighbour(n0, 3)) = 1;              // je 0x1000caf1, je 0x1000cafa
    if (n2) *TileDirty(n2) = 1;                                       // je 0x1000cb13
    if (n3) *TileDirty(n3) = 1;                                       // je 0x1000cb29
    if (n1) *TileDirty(n1) = 1;                                       // je 0x1000cb3f
    if (n1 && n2) *TileDirty(*TileNeighbour(n1, 2)) = 1;              // je 0x1000cb55, je 0x1000cb5e
    if (n1 && n3) *TileDirty(*TileNeighbour(n1, 3)) = 1;              // je 0x1000cb77, je 0x1000cb80
}

// 0x0000c7b0  Tile::elevateCorner (VA 0x1000c7b0)
// __thiscall, one stack DWORD argument (`ret 4` at 0x1000cb98), no callee at all.
// After the switch:
//   newH  = (float)((corner[arg] + 1) * g_splineHeight)   fild [ebp-0x54] 0x1000c961, fmul [0x10063e54]
//           0x1000c964, fstp dword [ebp-0x38] 0x1000c96a; the counter is read at 0x1000c958 and incremented
//           by one at 0x1000c95b BEFORE the multiply, and re-read and stored back later (0x1000ca74).
//   a, b, c = the heights of the vertices (f0,v0) 0x1000c96d, (f3,v3) 0x1000c98c and (f4,v4) 0x1000c9ab,
//           each read as g_vertices[face.v[slot]][1] (the `+ 0x100b28cc` base is component 1).
//   h     = newH, copied as a 32-bit pattern [ebp-0x38] -> [ebp-0x48] (0x1000c9ca).
//   m     = (float)((h + a) * 0.5f)                       fld 0x1000c9d0, fadd 0x1000c9d3,
//           fmul [0x1005f3e0] 0x1000c9d6, fstp [ebp-0x4c] 0x1000c9dc
// Then, in this order:
//   1. the centre vertex (face 0 slot 2, read through Tile+0x50 at 0x1000c9e2) is compared with m
//      (fcomp 0x1000c9eb, fnstsw, test ah, 0x41, jne 0x1000c9f6). ah & 0x41 is C0 | C3, so the fall-through
//      (m kept) is taken only when m is strictly greater AND the compare is ordered: that is `m > centre`,
//      and an unordered compare keeps the centre. The chosen 32-bit pattern is stored back unconditionally
//      (0x1000ca12-0x1000ca1e), so the centre is rewritten with its own value when it wins.
//   2. g_vertices[(f1,v1)][1] = (float)((h + b) * 0.5f)   0x1000ca24-0x1000ca46
//   3. g_vertices[(f2,v2)][1] = (float)((h + c) * 0.5f)   0x1000ca4c-0x1000ca6e
//   4. corner[arg] = corner[arg] + 1                      0x1000ca74-0x1000ca86
//   5. g_vertices[(f5,v5)][1] = newH (a 32-bit move, 0x1000ca9f-0x1000caa2)
//   6. the eight normals-dirty stores
typedef void(__fastcall* ElevateCorner_t)(void*, void*, int);
ElevateCorner_t ElevateCorner_orig;
void __fastcall ElevateCorner_re(void* self, void*, int corner) {
    CornerPairs p;
    if (!CornerPairsFor(corner, &p))
        return;                                      // see the note above: the original faults here
    const float spline = TFloat(0x10063e54);         // g_splineHeight
    const float half = TFloat(0x1005f3e0);           // 0.5f
    const int steps = *TileCorner(self, corner) + 1;
    const float newH = (float)((double)steps * (double)spline);

    const float a = *Vertex(FaceVertex(self, p.f0, p.v0), 1);
    const float b = *Vertex(FaceVertex(self, p.f3, p.v3), 1);
    const float c = *Vertex(FaceVertex(self, p.f4, p.v4), 1);
    const float h = newH;
    const float m = (float)(((double)h + (double)a) * (double)half);

    float* centre = Vertex(*(const int*)((unsigned char*)self + 0x50), 1);
    *centre = (m > *centre) ? m : *centre;
    *Vertex(FaceVertex(self, p.f1, p.v1), 1) = (float)(((double)h + (double)b) * (double)half);
    *Vertex(FaceVertex(self, p.f2, p.v2), 1) = (float)(((double)h + (double)c) * (double)half);
    *TileCorner(self, corner) = *TileCorner(self, corner) + 1;
    *Vertex(FaceVertex(self, p.f5, p.v5), 1) = newH;
    MarkNeighbourNormals(self);
}

// 0x0000ccc0  Tile::lowerCorner (VA 0x1000ccc0)
// __thiscall, one stack DWORD argument (`ret 4`), one callee: Tile::getType 0x10001f60, reached through the
// thunk 0x1000119a. The same switch (table at 0x1000d2d3, bound check 0x1000ccf2 / ja 0x1000ccf6) and the
// same six pairs; the differences are:
//   newH = (float)((corner[arg] - 1) * g_splineHeight)    sub 0x1000ce6b, fild 0x1000ce71, fmul 0x1000ce74
//   the centre takes the SMALLER value: fcomp 0x1000cefb, test ah, 1, je 0x1000cf06. ah & 1 is C0 alone, so
//   m wins when m < centre OR the compare is unordered, which is `!(m >= centre)`.
//   corner[arg] = corner[arg] - 1                         0x1000cf8d
// and, after the corner vertex is written and before the normals-dirty stores, a block that only runs for
// type 7 (cmp [this+0x24], 7 at 0x1000cfbb, jne 0x1000cfbf): every vertex in it is set to newH - 13.0f
// (fsub [0x1005f518], e.g. 0x1000cfc8), and each store is guarded by a neighbour pointer and by that
// neighbour's Tile::getType being 7:
//   centre (Tile+0x50)                                                        always      0x1000cfd7
//   face1.v2 (Tile+0x88)   n[0] != 0 (je 0x1000cfe4) and getType(n[0]) == 7 (jne 0x1000cff8)   0x1000d013
//     face0.v0 (Tile+0x48)   ... and n[2] != 0, getType(n[2]) == 7, getType(n[0]->n[2]) == 7   0x1000d057
//     face3.v2 (Tile+0xf8)   ... and n[3] != 0, getType(n[3]) == 7, getType(n[0]->n[3]) == 7   0x1000d09e
//   face5.v1 (Tile+0x164)  n[1] != 0 (je 0x1000d0ab) and getType(n[1]) == 7 (jne 0x1000d0bf)   0x1000d0da
//     face5.v0 (Tile+0x160)  ... and n[2] != 0, getType(n[2]) == 7, getType(n[1]->n[2]) == 7   0x1000d121
//     face7.v1 (Tile+0x1d4)  ... and n[3] != 0, getType(n[3]) == 7, getType(n[1]->n[3]) == 7   0x1000d168
//   face0.v1 (Tile+0x4c)   n[2] != 0 (je 0x1000d175) and getType(n[2]) == 7 (jne 0x1000d185)   0x1000d199
//   face3.v1 (Tile+0xf4)   n[3] != 0 (je 0x1000d1a6) and getType(n[3]) == 7 (jne 0x1000d1b6)   0x1000d1cd
// getType is called again for each test (no result is cached: 0x1000d028 and 0x1000d0ef both ask n[2]).
typedef int(__fastcall* TileGetType_t)(void*, void*);
typedef void(__fastcall* LowerCorner_t)(void*, void*, int);
LowerCorner_t LowerCorner_orig;
void __fastcall LowerCorner_re(void* self, void*, int corner) {
    CornerPairs p;
    if (!CornerPairsFor(corner, &p))
        return;                                      // same /GZ-fill default as elevateCorner
    const TileGetType_t getType = (TileGetType_t)TerrainRva(0x00001f60);
    const float spline = TFloat(0x10063e54);         // g_splineHeight
    const float half = TFloat(0x1005f3e0);           // 0.5f
    const float drop = TFloat(0x1005f518);           // 13.0f
    const int steps = *TileCorner(self, corner) - 1;
    const float newH = (float)((double)steps * (double)spline);

    const float a = *Vertex(FaceVertex(self, p.f0, p.v0), 1);
    const float b = *Vertex(FaceVertex(self, p.f3, p.v3), 1);
    const float c = *Vertex(FaceVertex(self, p.f4, p.v4), 1);
    const float h = newH;
    const float m = (float)(((double)h + (double)a) * (double)half);

    float* centre = Vertex(*(const int*)((unsigned char*)self + 0x50), 1);
    *centre = !(m >= *centre) ? m : *centre;
    *Vertex(FaceVertex(self, p.f1, p.v1), 1) = (float)(((double)h + (double)b) * (double)half);
    *Vertex(FaceVertex(self, p.f2, p.v2), 1) = (float)(((double)h + (double)c) * (double)half);
    *TileCorner(self, corner) = *TileCorner(self, corner) - 1;
    *Vertex(FaceVertex(self, p.f5, p.v5), 1) = newH;

    if (TileType(self) == 7) {
        const float sunk = (float)((double)newH - (double)drop);
        void* n0 = *TileNeighbour(self, 0);
        void* n1 = *TileNeighbour(self, 1);
        void* n2 = *TileNeighbour(self, 2);
        void* n3 = *TileNeighbour(self, 3);
        *Vertex(*(const int*)((unsigned char*)self + 0x50), 1) = sunk;
        if (n0 && getType(n0, 0) == 7) {
            *Vertex(FaceVertex(self, 1, 2), 1) = sunk;
            if (n2 && getType(n2, 0) == 7 && getType(*TileNeighbour(n0, 2), 0) == 7)
                *Vertex(FaceVertex(self, 0, 0), 1) = sunk;
            if (n3 && getType(n3, 0) == 7 && getType(*TileNeighbour(n0, 3), 0) == 7)
                *Vertex(FaceVertex(self, 3, 2), 1) = sunk;
        }
        if (n1 && getType(n1, 0) == 7) {
            *Vertex(FaceVertex(self, 5, 1), 1) = sunk;
            if (n2 && getType(n2, 0) == 7 && getType(*TileNeighbour(n1, 2), 0) == 7)
                *Vertex(FaceVertex(self, 5, 0), 1) = sunk;
            if (n3 && getType(n3, 0) == 7 && getType(*TileNeighbour(n1, 3), 0) == 7)
                *Vertex(FaceVertex(self, 7, 1), 1) = sunk;
        }
        if (n2 && getType(n2, 0) == 7)
            *Vertex(FaceVertex(self, 0, 1), 1) = sunk;
        if (n3 && getType(n3, 0) == 7)
            *Vertex(FaceVertex(self, 3, 1), 1) = sunk;
    }
    MarkNeighbourNormals(self);
}

// ===================================================================================================
// 0x0000c2c0  Tile::reset (VA 0x1000c2c0)
// ===================================================================================================
// __thiscall, two stack DWORD arguments (`ret 8` at 0x1000c390). Stores, in this order:
//   [this+0x2c] = x (0x1000c2e3), [this+0x30] = y (0x1000c2ec)
//   a local byte [ebp-8] = 0 (0x1000c2ef) that is never read again
//   [this+0x240] = 0 (0x1000c2f6), [this+4] = 0 (0x1000c303), [this+0x1c] = 0 (0x1000c30d),
//   [this+0xc] = 0 (0x1000c317), [this+0x14] = 0 (0x1000c321): the variation and the four odd corner
//   counters corner[1], corner[7], corner[3], corner[5] (the order the source wrote them in)
//   the four neighbour pointers, through a counted loop (cmp 4 / jge 0x1000c33e, store 0x1000c346)
//   [this+0x24] = 4 (0x1000c353), [this+0x28] = 0 (0x1000c35d)
//   PathInfo::clear on this+0x208 (call 0x1000c36d, thunk 0x1000105f -> 0x1000f6e0)
//   WallInfo::clear on this+0x210 (call 0x1000c37b, thunk 0x100011c7 -> 0x1000f7a0)
// Nothing is loaded into eax after the last call, and there is no conditional branch other than the loop.
typedef void(__fastcall* VoidThis_t)(void*, void*);
typedef void(__fastcall* TileReset_t)(void*, void*, int, int);
TileReset_t TileReset_orig;
void __fastcall TileReset_re(void* self, void*, int x, int y) {
    unsigned char* t = (unsigned char*)self;
    *(int*)(t + 0x2c) = x;
    *(int*)(t + 0x30) = y;
    *(int*)(t + 0x240) = 0;
    *(int*)(t + 0x04) = 0;
    *(int*)(t + 0x1c) = 0;
    *(int*)(t + 0x0c) = 0;
    *(int*)(t + 0x14) = 0;
    for (int i = 0; i < 4; i++)
        *(void**)(t + 0x34 + i * 4) = 0;
    *(int*)(t + 0x24) = 4;
    *(int*)(t + 0x28) = 0;
    ((VoidThis_t)TerrainRva(0x0000f6e0))(t + 0x208, 0);
    ((VoidThis_t)TerrainRva(0x0000f7a0))(t + 0x210, 0);
}

// ===================================================================================================
// 0x0000c560  initGrid (VA 0x1000c560)
// ===================================================================================================
// __cdecl, two stack DWORD arguments (plain `ret` at 0x1000c730), no callee. It lays out a w x h grid of
// tiles, three by three vertices each, into g_vertices and g_normals.
//   [ebp-4] = w * h (0x1000c57b) is computed and never read again.
//   row = w * 3 (0x1000c585).
// Four counted loops: y (cmp [ebp+0xc] / jge 0x1000c5a3), x (cmp [ebp+8] / jge 0x1000c5c1),
// i (cmp 3 / jge 0x1000c62c), j (cmp 3 / jge 0x1000c648). A w or h that is zero or negative makes the
// matching loop body run zero times and the function write nothing.
//   base = x * 3 + (y * row) * 3            0x1000c5ca-0x1000c5d9 (the two imul in that order)
//   ox   = (float)(x * g_tileW - (w * g_tileW) / 2.0f)
//          fild x 0x1000c5dc, fmul [0x10063e4c] 0x1000c5df, fild w 0x1000c5e5, fmul 0x1000c5e8,
//          fdiv [0x1005f028] (2.0f) 0x1000c5ee, fsubp 0x1000c5f4, fstp dword [ebp-0x18] 0x1000c5f6
//   oz   = (float)(y * g_tileH - (h * g_tileH) / 2.0f)     the same with 0x10063e50, fstp [ebp-0x1c]
// and per (i, j), with k = base + i + j * row (0x1000c662-0x1000c671):
//   g_vertices[k][0] = (float)(i * (g_tileW / 2.0f) + ox)  fild 0x1000c64e, fld [0x10063e4c] 0x1000c651,
//                      fdiv 0x1000c657, fmulp 0x1000c65d, fadd [ebp-0x18] 0x1000c65f, fstp 0x1000c674
//   g_vertices[k][2] = (float)(j * (g_tileH / 2.0f) + oz)  0x1000c67a-0x1000c6a0 (base 0x100b28d0)
//   g_vertices[k][1] = 0            an integer store of 0        0x1000c6b8
//   g_normals[k][0]  = 0            integer store                0x1000c6d4
//   g_normals[k][1]  = 1.0f         integer store of 0x3f800000  0x1000c6f0
//   g_normals[k][2]  = 0            integer store                0x1000c70c
// The index k is recomputed from scratch before each of the six stores; all six use the same value.
typedef void(__cdecl* InitGrid_t)(int, int);
InitGrid_t InitGrid_orig;
void __cdecl InitGrid_re(int w, int h) {
    const float tileW = TFloat(0x10063e4c);          // g_tileW
    const float tileH = TFloat(0x10063e50);          // g_tileH
    const float two = TFloat(0x1005f028);            // 2.0f
    const int row = w * 3;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            const int base = x * 3 + (y * row) * 3;
            const float ox = (float)((double)x * (double)tileW
                                     - ((double)w * (double)tileW) / (double)two);
            const float oz = (float)((double)y * (double)tileH
                                     - ((double)h * (double)tileH) / (double)two);
            for (int i = 0; i < 3; i++) {
                for (int j = 0; j < 3; j++) {
                    const int k = base + i + j * row;
                    *Vertex(k, 0) = (float)((double)i * ((double)tileW / (double)two) + (double)ox);
                    *Vertex(k, 2) = (float)((double)j * ((double)tileH / (double)two) + (double)oz);
                    *(int*)Vertex(k, 1) = 0;
                    *(int*)Normal(k, 0) = 0;
                    *(int*)Normal(k, 1) = 0x3f800000;
                    *(int*)Normal(k, 2) = 0;
                }
            }
        }
    }
}

// ===================================================================================================
// 0x00012e70  setTextureId (VA 0x10012e70)
// ===================================================================================================
// __cdecl, four stack DWORD arguments (plain `ret` at 0x10012ead), no callee and no conditional branch:
// one store into the texture table g_textures 0x100687f8, whose strides are 0x384 per type (imul 0x10012e8b)
// and 0x24 per variation (imul 0x10012e94); the piece indexes DWORDs (0x10012ea4). Nothing is bounds-checked
// and nothing is returned.
typedef void(__cdecl* SetTextureId_t)(int, int, int, unsigned int);
SetTextureId_t SetTextureId_orig;
void __cdecl SetTextureId_re(int type, int variation, int piece, unsigned int value) {
    unsigned char* row = TerrainBase() + 0x000687f8u + (unsigned)type * 0x384u
                         + (unsigned)variation * 0x24u;
    *(unsigned int*)(row + (unsigned)piece * 4u) = value;
}

// ===================================================================================================
// 0x00012ec0  Tile::setFaceTexture (VA 0x10012ec0)
// ===================================================================================================
// __thiscall, three stack DWORD arguments (`ret 0xc` at 0x100131d3), no callee. It copies one entry of
// g_textures into one of the tile's eight faces. The `type` ARGUMENT is first overridden according to the
// tile's own type.
//
// Override switch: [ebp-8] = this->type - 1 (0x10012ee0-0x10012eec), bounded unsigned against 0x15
// (cmp 0x10012eef, ja 0x10012ef3 to 0x10012fc2 = no override), then the byte table of 22 entries at
// 0x100131f2 selects one of seven blocks through the DWORD table at 0x100131d6 (0x10012efe, 0x10012f04).
// The table bytes, for this->type = 1 .. 22, are
//     0 6 6 6 6 1 2 6 6 6 6 6 3 3 3 3 4 6 6 6 1 5
// and the seven blocks are
//     [0] 0x10012f62  type 1:     sub & 0x80 (and 0x10012f68, je 0x10012f70) -> type = 0x1a
//     [1] 0x10012f3e  type 6, 21: type = 4
//     [2] 0x10012f7b  type 7:     type = 0x1b + (sub % 4)
//     [3] 0x10012f0b  type 13..16: type = 0xd
//     [4] 0x10012f17  type 17:    sub == 1 -> 0x17 (jne 0x10012f1e), else sub == 2 -> 0x18
//                                 (jne 0x10012f30), else unchanged
//     [5] 0x10012f47  type 22:    0x40 <= sub < 0x48 (jl 0x10012f4e, jge 0x10012f57) -> type = 4
//     [6] 0x10012fc2  every other type: no override
// The `sub % 4` of block 2 is the compiler's signed remainder: `and eax, 0x80000003` (0x10012f81) keeps the
// sign bit with the low two bits, and for a negative result `dec; or 0xfffffffc; inc` (0x10012f88-0x10012f8c)
// turns it into -3..-1. The result is then bounded UNSIGNED against 3 (cmp 0x10012f90, ja 0x10012f94), so a
// negative `sub` whose remainder is not zero skips the switch and leaves the type unchanged; a negative
// multiple of 4 gives 0 and takes case 0. The four cases (table 0x10013208) load 0x1b, 0x1c, 0x1d, 0x1e.
//
// Piece clamp (0x10012fc2-0x10012fe2): if (type == 4 || this->type == 7 || type == 2) and piece > 4
// (je 0x10012fc6, je 0x10012fcf, jne 0x10012fd5, jle 0x10012fdb) then piece = 0.
//
// Face switch: the face argument is bounded unsigned against 7 (cmp 0x10012fea, ja 0x10012fee to the
// epilogue) and dispatched through the DWORD table at 0x10013218, which maps the argument to a face slot
//     0->2, 1->3, 2->7, 3->6, 4->5, 5->4, 6->0, 7->1
// (the eight blocks write Tile+0xdc, +0x114, +0x1f4, +0x1bc, +0x184, +0x14c, +0x6c, +0xa4, which are
// face[k] + 0x24 for those k). Each block stores g_textures[type][this->variation][piece] into the face's
// +0x24 and the LOW BYTE of piece into its +0x28 (e.g. 0x1001301d-0x1001302f).
const int kFaceSlot[8] = {2, 3, 7, 6, 5, 4, 0, 1};   // table 0x10013218

typedef void(__fastcall* SetFaceTexture_t)(void*, void*, int, int, int);
SetFaceTexture_t SetFaceTexture_orig;
void __fastcall SetFaceTexture_re(void* self, void*, int type, int face, int piece) {
    const int own = TileType(self);
    const int sub = TileSub(self);
    switch (own) {
        case 1:
            if (sub & 0x80) type = 0x1a;
            break;
        case 6: case 21:
            type = 4;
            break;
        case 7: {
            unsigned int r = (unsigned int)sub & 0x80000003u;
            if ((int)r < 0) r = ((r - 1u) | 0xfffffffcu) + 1u;
            if (r <= 3u) {
                static const int kType7[4] = {0x1b, 0x1c, 0x1d, 0x1e};   // table 0x10013208
                type = kType7[r];
            }
            break;
        }
        case 13: case 14: case 15: case 16:
            type = 0xd;
            break;
        case 17:
            if (sub == 1) type = 0x17;
            else if (sub == 2) type = 0x18;
            break;
        case 22:
            if (sub >= 0x40 && sub < 0x48) type = 4;
            break;
        default:
            break;                                   // includes own <= 0 and own > 22 (the `ja`)
    }
    if ((type == 4 || own == 7 || type == 2) && piece > 4)
        piece = 0;
    if ((unsigned int)face > 7u)
        return;
    unsigned char* f = (unsigned char*)self + 0x48 + (unsigned)kFaceSlot[face] * 0x38u;
    const unsigned char* row = TerrainBase() + 0x000687f8u + (unsigned)type * 0x384u
                               + (unsigned)*(const int*)((unsigned char*)self + 0x240) * 0x24u;
    *(unsigned int*)(f + 0x24) = *(const unsigned int*)(row + (unsigned)piece * 4u);
    *(f + 0x28) = (unsigned char)piece;
}

// ===================================================================================================
// 0x00012cf0  Tile::calcNormals (VA 0x10012cf0)
// ===================================================================================================
// __thiscall, no argument (plain `ret` at 0x10012e18). It returns at once unless the normals-dirty byte
// [this+0x244] is set (movzx 0x10012d12, test 0x10012d18, jne 0x10012d1a over the jmp to the epilogue at
// 0x10012d1c). Otherwise it calls Tile::smoothNormals 0x10011ef0 (thunk 0x1000131b) four times with the
// argument pairs (0,2) (1,2) (0,3) (1,3) - pushed second argument first, 0x10012d21-0x10012d4c - then sums
// the eight face normals, component by component (loop cmp 2 / jg 0x10012d67):
//   g_normals[this->face[0].v[2]][c] = (float)(face[0].n[c] + face[1].n[c] + ... + face[7].n[c])
// The eight loads are fld [this + 0x74 + c*4] then seven fadd of +0xac, +0xe4, +0x11c, +0x154, +0x18c,
// +0x1c4, +0x1fc (0x10012d75-0x10012dc7), so the eight terms are added left to right at 53 bits and the sum
// is rounded once by the `fstp dword` at 0x10012dda. The vertex index is re-read from [this+0x50] inside the
// loop (0x10012dd1). Finally normalize 0x10037c80 is called on that normal (0x10012df6, thunk 0x100011a9)
// and the dirty byte is cleared (0x10012e01).
typedef void(__fastcall* SmoothNormals_t)(void*, void*, int, int);
typedef void(__cdecl* Normalize_t)(float*);
typedef void(__fastcall* CalcNormals_t)(void*, void*);
CalcNormals_t CalcNormals_orig;
void __fastcall CalcNormals_re(void* self, void*) {
    if (*TileDirty(self) == 0)
        return;
    const SmoothNormals_t smooth = (SmoothNormals_t)TerrainRva(0x00011ef0);
    smooth(self, 0, 0, 2);
    smooth(self, 0, 1, 2);
    smooth(self, 0, 0, 3);
    smooth(self, 0, 1, 3);
    for (int c = 0; c <= 2; c++) {
        const double sum = (double)FaceNormalC(self, 0, c) + (double)FaceNormalC(self, 1, c)
                         + (double)FaceNormalC(self, 2, c) + (double)FaceNormalC(self, 3, c)
                         + (double)FaceNormalC(self, 4, c) + (double)FaceNormalC(self, 5, c)
                         + (double)FaceNormalC(self, 6, c) + (double)FaceNormalC(self, 7, c);
        *Normal(*(const int*)((unsigned char*)self + 0x50), c) = (float)sum;
    }
    ((Normalize_t)TerrainRva(0x00037c80))(Normal(*(const int*)((unsigned char*)self + 0x50), 0));
    *TileDirty(self) = 0;
}

}  // namespace

SG_HOOK("Terrain.dll", 0x0000c7b0, Tile_elevateCorner, ElevateCorner_re, ElevateCorner_orig);
SG_HOOK("Terrain.dll", 0x0000ccc0, Tile_lowerCorner, LowerCorner_re, LowerCorner_orig);
SG_HOOK("Terrain.dll", 0x0000c2c0, Tile_reset, TileReset_re, TileReset_orig);
SG_HOOK("Terrain.dll", 0x0000c560, initGrid, InitGrid_re, InitGrid_orig);
SG_HOOK("Terrain.dll", 0x00012e70, setTextureId, SetTextureId_re, SetTextureId_orig);
SG_HOOK("Terrain.dll", 0x00012ec0, Tile_setFaceTexture, SetFaceTexture_re, SetFaceTexture_orig);
SG_HOOK("Terrain.dll", 0x00012cf0, Tile_calcNormals, CalcNormals_re, CalcNormals_orig);

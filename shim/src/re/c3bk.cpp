// Batch c3bk: Terrain.dll, round 16. The addresses in SG_HOOK and in the `// 0x...` comment lines are RVAs;
// every instruction address cited in a comment is a VA (VA = 0x10000000 + RVA), the form the debug build's
// disassembly prints. Terrain.dll is a static import of golf_clean.exe, so it is mapped before the shim
// installs its hooks, and it loads at its preferred base 0x10000000 (measured by batch c3bf,
// log/c3/alt/c3bf_probe.py). It is a /Od /ZI /GZ /GX build: every body is literal, so each reimplementation
// below follows the original's order of loads, compares, calls and stores, and each fact carries the VA it
// comes from.
//
// __thiscall is emulated with __fastcall (ecx = this, edx unused).
//
// Seven functions: Tile::setTypeId and the six callees it dispatches to.
//   Tile::setTypeId            0x00014020  leave the old type, store the new one, dispatch on it
//   Tile::setPathFlagForType   0x0000d480  set or clear +0x20a from the type, then re-blend
//   Tile::resetFaceTextures    0x00015230  give all faces one texture row, then re-blend
//   Tile::enterType6           0x0000de60  raise the type-6 corner vertices by 20.0, dirty, re-blend
//   Tile::enterType7           0x0000d540  lower the type-7 corner vertices by 13.0, dirty, re-blend
//   Tile::leaveType6           0x00014190  undo the type-6 raise (one vertex excepted), dirty
//   Tile::leaveType7           0x000149e0  undo the type-7 lower, dirty
//
// THE MODULE DATA THESE BODIES TOUCH
// ----------------------------------
//   g_vertices    0x100b28c8  three floats per entry, stride 0xc   (the four enter/leave walks write [1])
//   g_textures    0x100687f8  0x384 per type, 0x24 per variation   (READ, by Tile::resetFaceTextures and by
//                                                                   Tile::setFaceTexture under the re-blend)
//   g_courseType  0x10070a0c  dword                                (read by Tile::blendFaceTextures)
//   g_collarInfo  0x10106b48  pointer to a dword table             (read by Tile::edgeKind)
// None of that is our own memory, so every window the vectors touch is a state region of the registry entry:
// diff_hook snapshots it, compares it after each arm and writes it back. It is idle at the main menu, where
// diff_hook runs: batch c3bf's probe counted zero calls into Tile::setTypeId and into the other Terrain.dll
// functions that read or write those words over 8 s.
//
// x87: the originals are x87 code, this file is compiled by MSVC 2022 x86 (SSE2). The control word on the A/B
// thread is 0x027f (PC = 53-bit, RC = nearest; measured by c3bf's probe), so each x87 operation rounds like a
// `double` operation. /Od makes every rounding point visible as an `fstp dword` (float) store: the only shape
// in this file is `fld dword [v]; fadd/fsub dword [k]; fstp dword [v]`, written below as one float assignment
// whose two operands are widened with (double) so C++ never rounds in between. The method is batch c3ap's.
// The two constants are read from the DLL's own data (0x1005f51c = 20.0f, 0x1005f518 = 13.0f), not spelled
// out as literals.
#include "hooks.h"

namespace {

// Terrain.dll has base relocations, so its data and callees are reached through the mapped module.
unsigned char* TerrainBase() {
    static unsigned char* base = 0;
    if (!base) base = (unsigned char*)GetModuleHandleA("Terrain.dll");
    return base;
}
void* TerrainRva(unsigned long rva) { return TerrainBase() + rva; }

// Tile fields used below, all from the instructions cited in each function.
//   +0x24  int   type
//   +0x28  int   sub          (the row index Tile::resetFaceTextures uses for type 0)
//   +0x34  Tile* n[4]         (+0x34 n0, +0x38 n1, +0x3c n2, +0x40 n3)
//   +0x44  int   faceCount, and the address Tile::getFaces returns (a Faces*)
//   +0x48  Face  face[8]      stride 0x38: int v[3] at +0, int texture at +0x24
//   +0x208 byte  hasPath
//   +0x20a byte  pathTypeFlag
//   +0x240 int   variation
//   +0x244 byte  normalsDirty
int    TileType(void* t)             { return *(const int*)((unsigned char*)t + 0x24); }
int    TileSub(void* t)              { return *(const int*)((unsigned char*)t + 0x28); }
void*  TileN(void* t, int i)         { return *(void**)((unsigned char*)t + 0x34 + (unsigned)i * 4u); }
int    TileFaceCount(void* t)        { return *(const int*)((unsigned char*)t + 0x44); }
int    TileVar(void* t)              { return *(const int*)((unsigned char*)t + 0x240); }
unsigned char* TileHasPath(void* t)  { return (unsigned char*)t + 0x208; }
unsigned char* TilePathFlag(void* t) { return (unsigned char*)t + 0x20a; }
unsigned char* TileDirty(void* t)    { return (unsigned char*)t + 0x244; }

// Vertex slot `v` of face `f` of a TILE: Tile + 0x48 + f*0x38 + v*4 (the originals address it as a single
// displacement, e.g. [this+0x50] for face 0 slot 2 at 0x1000de86 and [this+0x1d4] for face 7 slot 1 at
// 0x1000e282).
int TileVIdx(void* t, int f, int v) {
    return *(const int*)((unsigned char*)t + 0x48 + (unsigned)f * 0x38u + (unsigned)v * 4u);
}
// The same slot reached through a Faces* (what Tile::getFaces 0x100153c0 returns, = Tile + 0x44): the record
// starts with a dword count, so face[f] sits at Faces + 4 + f*0x38 (e.g. [eax+0x120] at 0x1000defd is face 5
// slot 1, [eax+4] at 0x1000e09e is face 0 slot 0).
int FacesVIdx(void* fs, int f, int v) {
    return *(const int*)((unsigned char*)fs + 4 + (unsigned)f * 0x38u + (unsigned)v * 4u);
}
// Texture dword of face `f` of a tile: Tile + 0x6c + f*0x38 (store at 0x100152a4 / 0x100152f1).
unsigned int* TileFaceTexture(void* t, int f) {
    return (unsigned int*)((unsigned char*)t + 0x6c + (unsigned)f * 0x38u);
}

// g_vertices 0x100b28c8, component 1 of entry `idx`: the originals compute `imul <idx>, 0xc` and address
// [<reg> + 0x100b28cc], i.e. the product is taken modulo 2^32 and added to base+4 with no bounds check; the
// unsigned arithmetic here is the same computation.
float* VertY(int idx) {
    return (float*)(TerrainBase() + 0x000b28ccu + (unsigned)idx * 0xcu);
}

// The two height deltas, read from the DLL's own float constants.
float K20() { return *(const float*)(TerrainBase() + 0x0005f51c); }   // 0x1005f51c
float K13() { return *(const float*)(TerrainBase() + 0x0005f518); }   // 0x1005f518

// One `fld dword [v]; fadd/fsub dword [k]; fstp dword [v]` rounding point.
float AddK(float v, float k) { return (float)((double)v + (double)k); }
float SubK(float v, float k) { return (float)((double)v - (double)k); }

// The callees, reached through their own addresses so the original arm and this arm run the same code.
typedef int(__fastcall* GetType_t)(void*, void*);                 // Tile::getType        0x10001f60
typedef void*(__fastcall* GetFaces_t)(void*, void*);              // Tile::getFaces       0x100153c0
typedef void(__fastcall* Reblend_t)(void*, void*);                // reblendNeighbourhood 0x10013500
typedef void(__fastcall* LeaveType_t)(void*, void*);              // 0x10014190 / 0x100149e0
typedef void(__fastcall* EnterType_t)(void*, void*);              // 0x1000de60 / 0x1000d540
typedef void(__fastcall* ResetFaces_t)(void*, void*);             // 0x10015230
typedef void(__fastcall* SetPathFlag_t)(void*, void*, int);       // 0x1000d480

int GetType(void* t) { return ((GetType_t)TerrainRva(0x00001f60))(t, 0); }
void* GetFaces(void* t) { return ((GetFaces_t)TerrainRva(0x000153c0))(t, 0); }
void Reblend(void* t) { ((Reblend_t)TerrainRva(0x00013500))(t, 0); }

// ===================================================================================================
// 0x0000d480  Tile::setPathFlagForType (VA 0x1000d480)
// ===================================================================================================
// __thiscall, one stack DWORD argument (`ret 4` at 0x1000d510), no return value. When the tile carries a path
// (byte [this+0x208] read at 0x1000d4a2, test/je 0x1000d4aa), it decides a single flag at [this+0x20a] from
// the type it is given: nine equality tests in this order - 0x14 (0x1000d4ac), 0x11 (0x1000d4b2), 0x16
// (0x1000d4b8), 0 (0x1000d4be), 2 (0x1000d4c4), 1 (0x1000d4ca), 3 (0x1000d4d0), 7 (0x1000d4d6) and 9
// (0x1000d4dc) - and any match stores 0 (0x1000d4e5) while anything else stores 1 (0x1000d4f1). A tile with no
// path keeps whatever the flag held. It ends by re-blending the neighbourhood (call 0x1000d4fb of the thunk
// 0x1000103c = Tile::reblendNeighbourhood 0x10013500), which it does whether or not the flag was touched.
typedef void(__fastcall* SetPathFlagForType_t)(void*, void*, int);
SetPathFlagForType_t SetPathFlagForType_orig;
void __fastcall SetPathFlagForType_re(void* self, void*, int type) {
    if (*TileHasPath(self)) {                          // je 0x1000d4aa
        if (type == 0x14 || type == 0x11 || type == 0x16 || type == 0 || type == 2 || type == 1
            || type == 3 || type == 7 || type == 9)    // the nine je, last one jne 0x1000d4e0
            *TilePathFlag(self) = 0;                   // 0x1000d4e5
        else
            *TilePathFlag(self) = 1;                   // 0x1000d4f1
    }
    Reblend(self);                                     // 0x1000d4fb
}

// ===================================================================================================
// 0x00015230  Tile::resetFaceTextures (VA 0x10015230)
// ===================================================================================================
// __thiscall, no argument and no return value (plain `ret` at 0x1001530f). A tile that carries a path
// (byte [this+0x208] at 0x10015252, je 0x1001525a) has its [this+0x20a] flag cleared (0x1001525f). Then every
// face of the tile is given ONE texture, with two different rows of g_textures 0x100687f8 depending on the
// tile's own type (cmp [this+0x24], 0 at 0x10015269, jne 0x1001526d):
//   type 0:  g_textures[0][this->sub][0]   - the dword at 0x100687f8 + [this+0x28] * 0x24 (load 0x1001529e)
//   otherwise: g_textures[type][variation][3] - the dword at 0x100687f8 + type * 0x384 + [this+0x240] * 0x24
//            + 0xc (imul 0x100152cf, imul 0x100152de, load 0x100152ea)
// Both loops run `for (i = 0; i < this->faceCount; i++)` over [this+0x44] (jge 0x1001528a and 0x100152c7) and
// store at [this + i*0x38 + 0x6c] (0x100152a4, 0x100152f1), so a faceCount of 0 or less writes no face. It
// ends by re-blending the neighbourhood (call 0x100152fa of the thunk 0x1000103c = 0x10013500), which
// immediately overwrites those faces for every type Tile::isSolidType accepts.
typedef void(__fastcall* ResetFaceTextures_t)(void*, void*);
ResetFaceTextures_t ResetFaceTextures_orig;
void __fastcall ResetFaceTextures_re(void* self, void*) {
    if (*TileHasPath(self))                            // je 0x1001525a
        *TilePathFlag(self) = 0;                       // 0x1001525f

    const unsigned char* const textures = TerrainBase() + 0x000687f8;
    if (TileType(self) == 0) {                         // jne 0x1001526d
        for (int i = 0; i < TileFaceCount(self); i++) {                    // jge 0x1001528a
            const unsigned int tex =
                *(const unsigned int*)(textures + (unsigned)TileSub(self) * 0x24u);   // 0x1001529e
            *TileFaceTexture(self, i) = tex;                                // 0x100152a4
        }
    } else {
        for (int j = 0; j < TileFaceCount(self); j++) {                    // jge 0x100152c7
            const unsigned int tex =
                *(const unsigned int*)(textures + (unsigned)TileType(self) * 0x384u
                                       + (unsigned)TileVar(self) * 0x24u + 0xcu);     // 0x100152ea
            *TileFaceTexture(self, j) = tex;                                // 0x100152f1
        }
    }
    Reblend(self);                                     // 0x100152fa
}

// ===================================================================================================
// The four corner walks of types 6 and 7
// ===================================================================================================
// 0x1000de60, 0x1000d540, 0x10014190 and 0x100149e0 are the same walk over the vertices a tile shares with the
// neighbours and diagonals of ONE type (6 or 7), with a different delta and sign each. The walk, with the
// jcc of each test given per function below, is:
//
//   this->face[0].v[2]                      always
//   if (n0 && type(n0) == TT) {
//       this->face[1].v[2];  n0.faces[5].v[1]
//       if (n2 && type(n2) == TT && type(n0->n2) == TT) {
//           this->face[0].v[0];  n0.faces[4].v[1];  n2.faces[3].v[2];  (n0->n2).faces[7].v[1]
//       }
//       if (n3 && type(n3) == TT && type(n0->n3) == TT) {
//           this->face[3].v[2];  n0.faces[7].v[1];  n3.faces[0].v[0];  (n0->n3).faces[4].v[1]
//       }
//   }
//   if (n1 && type(n1) == TT) {
//       this->face[5].v[1];  n1.faces[1].v[2]
//       if (n2 && type(n2) == TT && type(n1->n2) == TT) {
//           this->face[5].v[0];  n1.faces[0].v[0];  n2.faces[7].v[1];  (n1->n2).faces[3].v[2]
//       }
//       if (n3 && type(n3) == TT && type(n1->n3) == TT) {
//           this->face[7].v[1];  n1.faces[3].v[2];  n3.faces[4].v[1];  (n1->n3).faces[0].v[0]
//       }
//   }
//   if (n2 && type(n2) == TT) { this->face[0].v[1];  n2.faces[3].v[1]; }
//   if (n3 && type(n3) == TT) { this->face[3].v[1];  n3.faces[0].v[1]; }
//
// then the dirty walk over this, n0, n0->n2, n0->n3, n2, n3, n1, n1->n2, n1->n3.
//
// Two shapes of NULL hazard, both of the original and both reproduced here:
//   * the inner tests read THIS tile's n2 / n3 and then dereference the OTHER tile's (e.g. 0x1000df22 reads
//     [this+0x3c] and 0x1000df46 reads [n0+0x3c]), so a neighbour with a NULL n2 or n3 faults inside
//     Tile::getType;
//   * the dirty walk does the same (0x1000e431 reads [this+0x3c], 0x1000e43d reads [n0+0x3c]).
// The neighbours the vectors use therefore always carry an n2 and an n3.
//
// Tile::getType is the thunk 0x1000119a = 0x10001f60 and Tile::getFaces the thunk 0x100010aa = 0x100153c0 in
// all four bodies.

// ---------------------------------------------------------------------------------------------------
// 0x0000de60  Tile::enterType6 (VA 0x1000de60)
// ---------------------------------------------------------------------------------------------------
// __thiscall, no argument and no return value (plain `ret` at 0x1000e50a). Every vertex of the walk gets
// `+ 20.0f` (fadd dword [0x1005f51c], 25 of them, the first at 0x1000de92). The guards compare against type 6
// (cmp eax, 6 at 0x1000debf and its twelve twins). The dirty walk is UNCONDITIONAL on the type: each of the
// nine tiles is marked whenever its pointers exist (0x1000e408 for this, then je 0x1000e416, 0x1000e42c /
// 0x1000e435, 0x1000e44e / 0x1000e457, 0x1000e470, 0x1000e486, 0x1000e49c, 0x1000e4b2 / 0x1000e4bb,
// 0x1000e4d4 / 0x1000e4dd). It ends with the re-blend (call 0x1000e4f2 of the thunk 0x1000103c).
typedef void(__fastcall* EnterType6_t)(void*, void*);
EnterType6_t EnterType6_orig;
void __fastcall EnterType6_re(void* self, void*) {
    const float k = K20();
    float* p;
    p = VertY(TileVIdx(self, 0, 2)); *p = AddK(*p, k);                    // 0x1000de8c-0x1000dea1

    void* const n0 = TileN(self, 0);
    void* const n1 = TileN(self, 1);
    void* const n2 = TileN(self, 2);
    void* const n3 = TileN(self, 3);

    if (n0 && GetType(n0) == 6) {                                         // je 0x1000deae, jne 0x1000dec2
        p = VertY(TileVIdx(self, 1, 2)); *p = AddK(*p, k);                // 0x1000ded4
        p = VertY(FacesVIdx(GetFaces(n0), 5, 1)); *p = AddK(*p, k);       // 0x1000defd-0x1000df1d
        if (n2 && GetType(n2) == 6 && GetType(TileN(n0, 2)) == 6) {       // je 0x1000df26, jne 0x1000df3a,
                                                                          // jne 0x1000df51
            p = VertY(TileVIdx(self, 0, 0)); *p = AddK(*p, k);            // 0x1000df60
            p = VertY(FacesVIdx(GetFaces(n0), 4, 1)); *p = AddK(*p, k);   // 0x1000df86
            p = VertY(FacesVIdx(GetFaces(n2), 3, 2)); *p = AddK(*p, k);   // 0x1000dfb3
            p = VertY(FacesVIdx(GetFaces(TileN(n0, 2)), 7, 1)); *p = AddK(*p, k);   // 0x1000dfe2
        }
        if (n3 && GetType(n3) == 6 && GetType(TileN(n0, 3)) == 6) {       // je 0x1000e00b, jne 0x1000e01f,
                                                                          // jne 0x1000e036
            p = VertY(TileVIdx(self, 3, 2)); *p = AddK(*p, k);            // 0x1000e048
            p = VertY(FacesVIdx(GetFaces(n0), 7, 1)); *p = AddK(*p, k);   // 0x1000e071
            p = VertY(FacesVIdx(GetFaces(n3), 0, 0)); *p = AddK(*p, k);   // 0x1000e09e
            p = VertY(FacesVIdx(GetFaces(TileN(n0, 3)), 4, 1)); *p = AddK(*p, k);   // 0x1000e0ca
        }
    }
    if (n1 && GetType(n1) == 6) {                                         // je 0x1000e0f3, jne 0x1000e107
        p = VertY(TileVIdx(self, 5, 1)); *p = AddK(*p, k);                // 0x1000e119
        p = VertY(FacesVIdx(GetFaces(n1), 1, 2)); *p = AddK(*p, k);       // 0x1000e142
        if (n2 && GetType(n2) == 6 && GetType(TileN(n1, 2)) == 6) {       // je 0x1000e167, jne 0x1000e17b,
                                                                          // jne 0x1000e192
            p = VertY(TileVIdx(self, 5, 0)); *p = AddK(*p, k);            // 0x1000e1a4
            p = VertY(FacesVIdx(GetFaces(n1), 0, 0)); *p = AddK(*p, k);   // 0x1000e1cd
            p = VertY(FacesVIdx(GetFaces(n2), 7, 1)); *p = AddK(*p, k);   // 0x1000e1f6
            p = VertY(FacesVIdx(GetFaces(TileN(n1, 2)), 3, 2)); *p = AddK(*p, k);   // 0x1000e226
        }
        if (n3 && GetType(n3) == 6 && GetType(TileN(n1, 3)) == 6) {       // je 0x1000e24e, jne 0x1000e262,
                                                                          // jne 0x1000e279
            p = VertY(TileVIdx(self, 7, 1)); *p = AddK(*p, k);            // 0x1000e28b
            p = VertY(FacesVIdx(GetFaces(n1), 3, 2)); *p = AddK(*p, k);   // 0x1000e2b4
            p = VertY(FacesVIdx(GetFaces(n3), 4, 1)); *p = AddK(*p, k);   // 0x1000e2e0
            p = VertY(FacesVIdx(GetFaces(TileN(n1, 3)), 0, 0)); *p = AddK(*p, k);   // 0x1000e310
        }
    }
    if (n2 && GetType(n2) == 6) {                                         // je 0x1000e335, jne 0x1000e345
        p = VertY(TileVIdx(self, 0, 1)); *p = AddK(*p, k);                // 0x1000e350
        p = VertY(FacesVIdx(GetFaces(n2), 3, 1)); *p = AddK(*p, k);       // 0x1000e376
    }
    if (n3 && GetType(n3) == 6) {                                         // je 0x1000e39f, jne 0x1000e3af
        p = VertY(TileVIdx(self, 3, 1)); *p = AddK(*p, k);                // 0x1000e3bd
        p = VertY(FacesVIdx(GetFaces(n3), 0, 1)); *p = AddK(*p, k);       // 0x1000e3e6
    }

    *TileDirty(self) = 1;                                                 // 0x1000e408
    if (n0) *TileDirty(n0) = 1;                                           // je 0x1000e416
    if (n0 && n2) *TileDirty(TileN(n0, 2)) = 1;                           // je 0x1000e42c, 0x1000e435
    if (n0 && n3) *TileDirty(TileN(n0, 3)) = 1;                           // je 0x1000e44e, 0x1000e457
    if (n2) *TileDirty(n2) = 1;                                           // je 0x1000e470
    if (n3) *TileDirty(n3) = 1;                                           // je 0x1000e486
    if (n1) *TileDirty(n1) = 1;                                           // je 0x1000e49c
    if (n1 && n2) *TileDirty(TileN(n1, 2)) = 1;                           // je 0x1000e4b2, 0x1000e4bb
    if (n1 && n3) *TileDirty(TileN(n1, 3)) = 1;                           // je 0x1000e4d4, 0x1000e4dd
    Reblend(self);                                                        // 0x1000e4f2
}

// ---------------------------------------------------------------------------------------------------
// 0x0000d540  Tile::enterType7 (VA 0x1000d540)
// ---------------------------------------------------------------------------------------------------
// __thiscall, no argument and no return value (plain `ret` at 0x1000dc81). It differs from the type-6 entry in
// four ways: it first clears [this+0x20a] (0x1000d566); every vertex gets `- 13.0f` (fsub dword [0x1005f518],
// 25 of them, the first at 0x1000d57c); the guards compare against 7 (cmp eax, 7 at 0x1000d5a9 and its twins);
// and the dirty walk also tests each target's type against 7 (jne 0x1000db11, 0x1000db43, 0x1000db78,
// 0x1000dba1, 0x1000dbc7, 0x1000dbed, 0x1000dc1f, 0x1000dc54), so a neighbour that is not type 7 is NOT
// marked - which the type-6 entry does not do. It ends with the re-blend (call 0x1000dc69).
typedef void(__fastcall* EnterType7_t)(void*, void*);
EnterType7_t EnterType7_orig;
void __fastcall EnterType7_re(void* self, void*) {
    const float k = K13();
    float* p;
    *TilePathFlag(self) = 0;                                              // 0x1000d566
    p = VertY(TileVIdx(self, 0, 2)); *p = SubK(*p, k);                    // 0x1000d576-0x1000d58b

    void* const n0 = TileN(self, 0);
    void* const n1 = TileN(self, 1);
    void* const n2 = TileN(self, 2);
    void* const n3 = TileN(self, 3);

    if (n0 && GetType(n0) == 7) {                                         // je 0x1000d598, jne 0x1000d5ac
        p = VertY(TileVIdx(self, 1, 2)); *p = SubK(*p, k);
        p = VertY(FacesVIdx(GetFaces(n0), 5, 1)); *p = SubK(*p, k);
        if (n2 && GetType(n2) == 7 && GetType(TileN(n0, 2)) == 7) {       // je 0x1000d60f, jne 0x1000d623,
                                                                          // jne 0x1000d63a
            p = VertY(TileVIdx(self, 0, 0)); *p = SubK(*p, k);
            p = VertY(FacesVIdx(GetFaces(n0), 4, 1)); *p = SubK(*p, k);
            p = VertY(FacesVIdx(GetFaces(n2), 3, 2)); *p = SubK(*p, k);
            p = VertY(FacesVIdx(GetFaces(TileN(n0, 2)), 7, 1)); *p = SubK(*p, k);
        }
        if (n3 && GetType(n3) == 7 && GetType(TileN(n0, 3)) == 7) {       // je 0x1000d6f3, jne 0x1000d707,
                                                                          // jne 0x1000d71e
            p = VertY(TileVIdx(self, 3, 2)); *p = SubK(*p, k);
            p = VertY(FacesVIdx(GetFaces(n0), 7, 1)); *p = SubK(*p, k);
            p = VertY(FacesVIdx(GetFaces(n3), 0, 0)); *p = SubK(*p, k);
            p = VertY(FacesVIdx(GetFaces(TileN(n0, 3)), 4, 1)); *p = SubK(*p, k);
        }
    }
    if (n1 && GetType(n1) == 7) {                                         // je 0x1000d7da, jne 0x1000d7ee
        p = VertY(TileVIdx(self, 5, 1)); *p = SubK(*p, k);
        p = VertY(FacesVIdx(GetFaces(n1), 1, 2)); *p = SubK(*p, k);
        if (n2 && GetType(n2) == 7 && GetType(TileN(n1, 2)) == 7) {       // je 0x1000d84f, jne 0x1000d863,
                                                                          // jne 0x1000d87a
            p = VertY(TileVIdx(self, 5, 0)); *p = SubK(*p, k);
            p = VertY(FacesVIdx(GetFaces(n1), 0, 0)); *p = SubK(*p, k);
            p = VertY(FacesVIdx(GetFaces(n2), 7, 1)); *p = SubK(*p, k);
            p = VertY(FacesVIdx(GetFaces(TileN(n1, 2)), 3, 2)); *p = SubK(*p, k);
        }
        if (n3 && GetType(n3) == 7 && GetType(TileN(n1, 3)) == 7) {       // je 0x1000d938, jne 0x1000d94c,
                                                                          // jne 0x1000d963
            p = VertY(TileVIdx(self, 7, 1)); *p = SubK(*p, k);
            p = VertY(FacesVIdx(GetFaces(n1), 3, 2)); *p = SubK(*p, k);
            p = VertY(FacesVIdx(GetFaces(n3), 4, 1)); *p = SubK(*p, k);
            p = VertY(FacesVIdx(GetFaces(TileN(n1, 3)), 0, 0)); *p = SubK(*p, k);
        }
    }
    if (n2 && GetType(n2) == 7) {                                         // je 0x1000da21, jne 0x1000da31
        p = VertY(TileVIdx(self, 0, 1)); *p = SubK(*p, k);
        p = VertY(FacesVIdx(GetFaces(n2), 3, 1)); *p = SubK(*p, k);
    }
    if (n3 && GetType(n3) == 7) {                                         // je 0x1000da8b, jne 0x1000da9b
        p = VertY(TileVIdx(self, 3, 1)); *p = SubK(*p, k);
        p = VertY(FacesVIdx(GetFaces(n3), 0, 1)); *p = SubK(*p, k);
    }

    *TileDirty(self) = 1;                                                 // 0x1000daf3
    if (n0 && GetType(n0) == 7)
        *TileDirty(n0) = 1;                                               // je 0x1000db01, jne 0x1000db11
    if (n0 && n2 && GetType(TileN(n0, 2)) == 7)
        *TileDirty(TileN(n0, 2)) = 1;                                     // je 0x1000db27, 0x1000db30,
                                                                          // jne 0x1000db43
    if (n0 && n3 && GetType(TileN(n0, 3)) == 7)
        *TileDirty(TileN(n0, 3)) = 1;                                     // je 0x1000db5c, 0x1000db65,
                                                                          // jne 0x1000db78
    if (n2 && GetType(n2) == 7)
        *TileDirty(n2) = 1;                                               // je 0x1000db91, jne 0x1000dba1
    if (n3 && GetType(n3) == 7)
        *TileDirty(n3) = 1;                                               // je 0x1000dbb7, jne 0x1000dbc7
    if (n1 && GetType(n1) == 7)
        *TileDirty(n1) = 1;                                               // je 0x1000dbdd, jne 0x1000dbed
    if (n1 && n2 && GetType(TileN(n1, 2)) == 7)
        *TileDirty(TileN(n1, 2)) = 1;                                     // je 0x1000dc03, 0x1000dc0c,
                                                                          // jne 0x1000dc1f
    if (n1 && n3 && GetType(TileN(n1, 3)) == 7)
        *TileDirty(TileN(n1, 3)) = 1;                                     // je 0x1000dc38, 0x1000dc41,
                                                                          // jne 0x1000dc54
    Reblend(self);                                                        // 0x1000dc69
}

// ---------------------------------------------------------------------------------------------------
// 0x00014190  Tile::leaveType6 (VA 0x10014190)
// ---------------------------------------------------------------------------------------------------
// __thiscall, no argument and no return value (plain `ret` at 0x10014832). It is the type-6 walk run backwards
// when the tile stops being type 6: 24 of its 25 vertices get `- 20.0f` (fsub dword [0x1005f51c], the first at
// 0x100141c2) and ONE gets `+ 20.0f` - this->face[0].v[0], the first vertex of the n0/n2 corner
// (fadd dword [0x10014296], index loaded from [this+0x48] at 0x1001428a). The guards compare against 6
// (cmp eax, 6 at 0x100141ef and its twelve twins). The dirty walk is unconditional on the type, like the
// type-6 entry (0x10014738, then je 0x10014746, 0x1001475c / 0x10014765, 0x1001477e / 0x10014787, 0x100147a0,
// 0x100147b6, 0x100147cc, 0x100147e2 / 0x100147eb, 0x10014804 / 0x1001480d). Unlike the two entry functions it
// does NOT re-blend: there is no call of 0x1000103c in the body, because the caller Tile::setTypeId runs the
// entry function of the new type right after and that one re-blends.
typedef void(__fastcall* LeaveType6_t)(void*, void*);
LeaveType6_t LeaveType6_orig;
void __fastcall LeaveType6_re(void* self, void*) {
    const float k = K20();
    float* p;
    p = VertY(TileVIdx(self, 0, 2)); *p = SubK(*p, k);                    // 0x100141b8-0x100141cd

    void* const n0 = TileN(self, 0);
    void* const n1 = TileN(self, 1);
    void* const n2 = TileN(self, 2);
    void* const n3 = TileN(self, 3);

    if (n0 && GetType(n0) == 6) {                                         // je 0x100141de, jne 0x100141f2
        p = VertY(TileVIdx(self, 1, 2)); *p = SubK(*p, k);
        p = VertY(FacesVIdx(GetFaces(n0), 5, 1)); *p = SubK(*p, k);
        if (n2 && GetType(n2) == 6 && GetType(TileN(n0, 2)) == 6) {       // je 0x10014256, jne 0x1001426a,
                                                                          // jne 0x10014281
            p = VertY(TileVIdx(self, 0, 0)); *p = AddK(*p, k);            // the one ADD: fadd 0x10014296
            p = VertY(FacesVIdx(GetFaces(n0), 4, 1)); *p = SubK(*p, k);
            p = VertY(FacesVIdx(GetFaces(n2), 3, 2)); *p = SubK(*p, k);
            p = VertY(FacesVIdx(GetFaces(TileN(n0, 2)), 7, 1)); *p = SubK(*p, k);
        }
        if (n3 && GetType(n3) == 6 && GetType(TileN(n0, 3)) == 6) {       // je 0x1001433b, jne 0x1001434f,
                                                                          // jne 0x10014366
            p = VertY(TileVIdx(self, 3, 2)); *p = SubK(*p, k);
            p = VertY(FacesVIdx(GetFaces(n0), 7, 1)); *p = SubK(*p, k);
            p = VertY(FacesVIdx(GetFaces(n3), 0, 0)); *p = SubK(*p, k);
            p = VertY(FacesVIdx(GetFaces(TileN(n0, 3)), 4, 1)); *p = SubK(*p, k);
        }
    }
    if (n1 && GetType(n1) == 6) {                                         // je 0x10014423, jne 0x10014437
        p = VertY(TileVIdx(self, 5, 1)); *p = SubK(*p, k);
        p = VertY(FacesVIdx(GetFaces(n1), 1, 2)); *p = SubK(*p, k);
        if (n2 && GetType(n2) == 6 && GetType(TileN(n1, 2)) == 6) {       // je 0x10014497, jne 0x100144ab,
                                                                          // jne 0x100144c2
            p = VertY(TileVIdx(self, 5, 0)); *p = SubK(*p, k);
            p = VertY(FacesVIdx(GetFaces(n1), 0, 0)); *p = SubK(*p, k);
            p = VertY(FacesVIdx(GetFaces(n2), 7, 1)); *p = SubK(*p, k);
            p = VertY(FacesVIdx(GetFaces(TileN(n1, 2)), 3, 2)); *p = SubK(*p, k);
        }
        if (n3 && GetType(n3) == 6 && GetType(TileN(n1, 3)) == 6) {       // je 0x1001457e, jne 0x10014592,
                                                                          // jne 0x100145a9
            p = VertY(TileVIdx(self, 7, 1)); *p = SubK(*p, k);
            p = VertY(FacesVIdx(GetFaces(n1), 3, 2)); *p = SubK(*p, k);
            p = VertY(FacesVIdx(GetFaces(n3), 4, 1)); *p = SubK(*p, k);
            p = VertY(FacesVIdx(GetFaces(TileN(n1, 3)), 0, 0)); *p = SubK(*p, k);
        }
    }
    if (n2 && GetType(n2) == 6) {                                         // je 0x10014665, jne 0x10014675
        p = VertY(TileVIdx(self, 0, 1)); *p = SubK(*p, k);
        p = VertY(FacesVIdx(GetFaces(n2), 3, 1)); *p = SubK(*p, k);
    }
    if (n3 && GetType(n3) == 6) {                                         // je 0x100146cf, jne 0x100146df
        p = VertY(TileVIdx(self, 3, 1)); *p = SubK(*p, k);
        p = VertY(FacesVIdx(GetFaces(n3), 0, 1)); *p = SubK(*p, k);
    }

    *TileDirty(self) = 1;                                                 // 0x10014738
    if (n0) *TileDirty(n0) = 1;                                           // je 0x10014746
    if (n0 && n2) *TileDirty(TileN(n0, 2)) = 1;                           // je 0x1001475c, 0x10014765
    if (n0 && n3) *TileDirty(TileN(n0, 3)) = 1;                           // je 0x1001477e, 0x10014787
    if (n2) *TileDirty(n2) = 1;                                           // je 0x100147a0
    if (n3) *TileDirty(n3) = 1;                                           // je 0x100147b6
    if (n1) *TileDirty(n1) = 1;                                           // je 0x100147cc
    if (n1 && n2) *TileDirty(TileN(n1, 2)) = 1;                           // je 0x100147e2, 0x100147eb
    if (n1 && n3) *TileDirty(TileN(n1, 3)) = 1;                           // je 0x10014804, 0x1001480d
}

// ---------------------------------------------------------------------------------------------------
// 0x000149e0  Tile::leaveType7 (VA 0x100149e0)
// ---------------------------------------------------------------------------------------------------
// __thiscall, no argument and no return value (plain `ret` at 0x10015082). The type-7 walk run backwards: all
// 25 vertices get `+ 13.0f` (fadd dword [0x1005f518], the first at 0x10014a12; there is no fsub in the body
// and no exception like the one type-6 leave has). The guards compare against 7 (cmp eax, 7 at 0x10014a3f and
// its twelve twins). The dirty walk is unconditional on the type, unlike the type-7 ENTRY (0x10014f88, then je
// 0x10014f96, 0x10014fac / 0x10014fb5, 0x10014fce / 0x10014fd7, 0x10014ff0, 0x10015006, 0x1001501c,
// 0x10015032 / 0x1001503b, 0x10015054 / 0x1001505d). It does not re-blend either.
typedef void(__fastcall* LeaveType7_t)(void*, void*);
LeaveType7_t LeaveType7_orig;
void __fastcall LeaveType7_re(void* self, void*) {
    const float k = K13();
    float* p;
    p = VertY(TileVIdx(self, 0, 2)); *p = AddK(*p, k);                    // 0x10014a08-0x10014a1d

    void* const n0 = TileN(self, 0);
    void* const n1 = TileN(self, 1);
    void* const n2 = TileN(self, 2);
    void* const n3 = TileN(self, 3);

    if (n0 && GetType(n0) == 7) {                                         // je 0x10014a2e, jne 0x10014a42
        p = VertY(TileVIdx(self, 1, 2)); *p = AddK(*p, k);
        p = VertY(FacesVIdx(GetFaces(n0), 5, 1)); *p = AddK(*p, k);
        if (n2 && GetType(n2) == 7 && GetType(TileN(n0, 2)) == 7) {       // je 0x10014aa6, jne 0x10014aba,
                                                                          // jne 0x10014ad1
            p = VertY(TileVIdx(self, 0, 0)); *p = AddK(*p, k);
            p = VertY(FacesVIdx(GetFaces(n0), 4, 1)); *p = AddK(*p, k);
            p = VertY(FacesVIdx(GetFaces(n2), 3, 2)); *p = AddK(*p, k);
            p = VertY(FacesVIdx(GetFaces(TileN(n0, 2)), 7, 1)); *p = AddK(*p, k);
        }
        if (n3 && GetType(n3) == 7 && GetType(TileN(n0, 3)) == 7) {       // je 0x10014b8b, jne 0x10014b9f,
                                                                          // jne 0x10014bb6
            p = VertY(TileVIdx(self, 3, 2)); *p = AddK(*p, k);
            p = VertY(FacesVIdx(GetFaces(n0), 7, 1)); *p = AddK(*p, k);
            p = VertY(FacesVIdx(GetFaces(n3), 0, 0)); *p = AddK(*p, k);
            p = VertY(FacesVIdx(GetFaces(TileN(n0, 3)), 4, 1)); *p = AddK(*p, k);
        }
    }
    if (n1 && GetType(n1) == 7) {                                         // je 0x10014c73, jne 0x10014c87
        p = VertY(TileVIdx(self, 5, 1)); *p = AddK(*p, k);
        p = VertY(FacesVIdx(GetFaces(n1), 1, 2)); *p = AddK(*p, k);
        if (n2 && GetType(n2) == 7 && GetType(TileN(n1, 2)) == 7) {       // je 0x10014ce7, jne 0x10014cfb,
                                                                          // jne 0x10014d12
            p = VertY(TileVIdx(self, 5, 0)); *p = AddK(*p, k);
            p = VertY(FacesVIdx(GetFaces(n1), 0, 0)); *p = AddK(*p, k);
            p = VertY(FacesVIdx(GetFaces(n2), 7, 1)); *p = AddK(*p, k);
            p = VertY(FacesVIdx(GetFaces(TileN(n1, 2)), 3, 2)); *p = AddK(*p, k);
        }
        if (n3 && GetType(n3) == 7 && GetType(TileN(n1, 3)) == 7) {       // je 0x10014dce, jne 0x10014de2,
                                                                          // jne 0x10014df9
            p = VertY(TileVIdx(self, 7, 1)); *p = AddK(*p, k);
            p = VertY(FacesVIdx(GetFaces(n1), 3, 2)); *p = AddK(*p, k);
            p = VertY(FacesVIdx(GetFaces(n3), 4, 1)); *p = AddK(*p, k);
            p = VertY(FacesVIdx(GetFaces(TileN(n1, 3)), 0, 0)); *p = AddK(*p, k);
        }
    }
    if (n2 && GetType(n2) == 7) {                                         // je 0x10014eb5, jne 0x10014ec5
        p = VertY(TileVIdx(self, 0, 1)); *p = AddK(*p, k);
        p = VertY(FacesVIdx(GetFaces(n2), 3, 1)); *p = AddK(*p, k);
    }
    if (n3 && GetType(n3) == 7) {                                         // je 0x10014f1f, jne 0x10014f2f
        p = VertY(TileVIdx(self, 3, 1)); *p = AddK(*p, k);
        p = VertY(FacesVIdx(GetFaces(n3), 0, 1)); *p = AddK(*p, k);
    }

    *TileDirty(self) = 1;                                                 // 0x10014f88
    if (n0) *TileDirty(n0) = 1;                                           // je 0x10014f96
    if (n0 && n2) *TileDirty(TileN(n0, 2)) = 1;                           // je 0x10014fac, 0x10014fb5
    if (n0 && n3) *TileDirty(TileN(n0, 3)) = 1;                           // je 0x10014fce, 0x10014fd7
    if (n2) *TileDirty(n2) = 1;                                           // je 0x10014ff0
    if (n3) *TileDirty(n3) = 1;                                           // je 0x10015006
    if (n1) *TileDirty(n1) = 1;                                           // je 0x1001501c
    if (n1 && n2) *TileDirty(TileN(n1, 2)) = 1;                           // je 0x10015032, 0x1001503b
    if (n1 && n3) *TileDirty(TileN(n1, 3)) = 1;                           // je 0x10015054, 0x1001505d
}

// ===================================================================================================
// 0x00014020  Tile::setTypeId (VA 0x10014020)
// ===================================================================================================
// __thiscall, one stack DWORD argument (`ret 4` at 0x10014112), no return value. It is the one entry point
// that changes a tile's terrain type, and it runs three steps in order.
// 1. A tile that carries a path (byte [this+0x208] at 0x10014042, test/je 0x1001404a) gets [this+0x20a] set
//    to 1 (0x1001404f), before anything else and whatever the new type is.
// 2. The OLD type at [this+0x24] is left: 7 calls Tile::leaveType7 (cmp 0x10014059, jne 0x1001405d, call
//    0x10014062 of the thunk 0x10001168 = 0x100149e0) and then, as a SEPARATE if, 6 calls Tile::leaveType6
//    (cmp 0x1001406a, jne 0x1001406e, call 0x10014073 of the thunk 0x1000114f = 0x10014190) while its else
//    branch tests 0x11 (cmp 0x1001407d, jne 0x10014081) and sets [this+0x244], the normals-dirty flag
//    (0x10014086). So an old type of 7 reaches both the leave call and the 0x11 test; 6 and 0x11 are
//    exclusive of each other.
// 3. The new type is stored (0x10014093) and dispatched through a jump table (`ja 0x100140a0` for anything
//    above 0x14, index bytes at 0x10014131, table at 0x10014115):
//      7            -> Tile::enterType7         call 0x100140b7 (thunk 0x10001203 = 0x1000d540)
//      0x11         -> Tile::setPathFlagForType(type)   push 0x100140c1, call 0x100140c5 (thunk 0x10001258
//                                                       = 0x1000d480)
//      6            -> Tile::enterType6         call 0x100140cf (thunk 0x10001230 = 0x1000de60)
//      0xd..0x10    -> Tile::setPathFlagForType(0xd)    push 0xd 0x100140d6, call 0x100140db
//      0 and 9      -> Tile::resetFaceTextures  call 0x100140e5 (thunk 0x100012e9 = 0x10015230)
//      4 and 0x14   -> Tile::reblendNeighbourhood       call 0x100140ef (thunk 0x1000103c = 0x10013500)
//      anything else-> Tile::setPathFlagForType(type)   push 0x100140f9, call 0x100140fd
// Every arm therefore ends in a re-blend of the neighbourhood, five of them through a callee.
typedef void(__fastcall* SetTypeId_t)(void*, void*, int);
SetTypeId_t SetTypeId_orig;
void __fastcall SetTypeId_re(void* self, void*, int type) {
    const LeaveType_t leaveType7 = (LeaveType_t)TerrainRva(0x000149e0);
    const LeaveType_t leaveType6 = (LeaveType_t)TerrainRva(0x00014190);
    const EnterType_t enterType7 = (EnterType_t)TerrainRva(0x0000d540);
    const EnterType_t enterType6 = (EnterType_t)TerrainRva(0x0000de60);
    const ResetFaces_t resetFaceTextures = (ResetFaces_t)TerrainRva(0x00015230);
    const SetPathFlag_t setPathFlagForType = (SetPathFlag_t)TerrainRva(0x0000d480);

    if (*TileHasPath(self))                            // je 0x1001404a
        *TilePathFlag(self) = 1;                       // 0x1001404f

    if (TileType(self) == 7)                           // jne 0x1001405d
        leaveType7(self, 0);                           // 0x10014062
    if (TileType(self) == 6)                           // jne 0x1001406e
        leaveType6(self, 0);                           // 0x10014073
    else if (TileType(self) == 0x11)                   // jne 0x10014081
        *TileDirty(self) = 1;                          // 0x10014086

    *(int*)((unsigned char*)self + 0x24) = type;       // 0x10014093

    switch (type) {                                    // ja 0x100140a0, jmp [ecx*4 + 0x10014115]
    case 7:
        enterType7(self, 0);                           // 0x100140b7
        break;
    case 0x11:
        setPathFlagForType(self, 0, type);             // 0x100140c5
        break;
    case 6:
        enterType6(self, 0);                           // 0x100140cf
        break;
    case 0xd:
    case 0xe:
    case 0xf:
    case 0x10:
        setPathFlagForType(self, 0, 0xd);              // 0x100140db
        break;
    case 0:
    case 9:
        resetFaceTextures(self, 0);                    // 0x100140e5
        break;
    case 4:
    case 0x14:
        Reblend(self);                                 // 0x100140ef
        break;
    default:
        setPathFlagForType(self, 0, type);             // 0x100140fd
        break;
    }
}

}  // namespace

SG_HOOK("Terrain.dll", 0x00014020, Tile_setTypeId, SetTypeId_re, SetTypeId_orig);
SG_HOOK("Terrain.dll", 0x0000d480, Tile_setPathFlagForType, SetPathFlagForType_re, SetPathFlagForType_orig);
SG_HOOK("Terrain.dll", 0x00015230, Tile_resetFaceTextures, ResetFaceTextures_re, ResetFaceTextures_orig);
SG_HOOK("Terrain.dll", 0x0000de60, Tile_enterType6, EnterType6_re, EnterType6_orig);
SG_HOOK("Terrain.dll", 0x0000d540, Tile_enterType7, EnterType7_re, EnterType7_orig);
SG_HOOK("Terrain.dll", 0x00014190, Tile_leaveType6, LeaveType6_re, LeaveType6_orig);
SG_HOOK("Terrain.dll", 0x000149e0, Tile_leaveType7, LeaveType7_re, LeaveType7_orig);

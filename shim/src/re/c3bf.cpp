// Batch c3bf: Terrain.dll, round 15. The addresses in SG_HOOK and in the `// 0x...` comment lines are RVAs;
// every instruction address cited in a comment is a VA (VA = 0x10000000 + RVA), the form the debug build's
// disassembly prints. Terrain.dll is a static import of golf_clean.exe, so it is mapped before the shim
// installs its hooks, and it loads at its preferred base (measured: 0x10000000, log/c3/alt/c3bf_probe.py).
// It is a /Od /ZI /GZ /GX build: every body is literal, so each reimplementation below follows the original's
// order of loads, compares, calls and stores, and each fact carries the VA it comes from.
//
// __thiscall is emulated with __fastcall (ecx = this, edx unused).
//
// Three functions, the tile re-blend / re-smooth group:
//   Tile::blendFaceTextures    0x00013670  rewrite the eight face textures of one tile from its neighbourhood
//   Tile::reblendNeighbourhood 0x00013500  do that for a tile and the eight tiles around it
//   Tile::smoothNormals        0x00011ef0  re-sum the vertex normals around one corner of a tile
//
// THE MODULE DATA THESE BODIES TOUCH
// ----------------------------------
//   g_vertices    0x100b28c8  three floats per entry, stride 0xc   (read by Terrain::faceNormal)
//   g_normals     0x10070a18  three floats per entry, stride 0xc   (written by Tile::smoothNormals)
//   g_textures    0x100687f8  0x384 per type, 0x24 per variation   (read by Tile::setFaceTexture)
//   g_courseType  0x10070a0c  dword                                (read by Tile::blendFaceTextures, 8 times)
//   g_collarInfo  0x10106b48  pointer to a dword table             (read by Tile::edgeKind)
// None of that is our own memory, so every window the vectors touch is a state region of the registry entry:
// diff_hook snapshots it, compares it after each arm and writes it back. It is idle at the main menu, where
// diff_hook runs: log/c3/alt/c3bf_probe.py counted zero calls into the three functions under test and into the
// five Terrain.dll functions that read or write g_courseType / g_collarInfo, over 8 s.
//
// x87: the originals are x87 code, this file is compiled by MSVC 2022 x86 (SSE2). The control word on the A/B
// thread is 0x027f (PC = 53-bit, RC = nearest; measured by the same probe), so each x87 operation rounds like a
// `double` operation. /Od makes every rounding point visible as an `fstp dword` (float) store: each one below is
// an assignment to a float, and inside one expression every operand is widened with a (double) cast so C++ never
// rounds in between. The method is batch c3ap's (log/c3/c3ap_purpose.md).
#include "hooks.h"

namespace {

// Terrain.dll has base relocations, so its data and callees are reached through the mapped module.
unsigned char* TerrainBase() {
    static unsigned char* base = 0;
    if (!base) base = (unsigned char*)GetModuleHandleA("Terrain.dll");
    return base;
}
void* TerrainRva(unsigned long rva) { return TerrainBase() + rva; }

// g_normals 0x10070a18, component `c` of entry `idx`. The original computes the byte offset with
// `imul <idx>, 0xc` and addresses `[<reg> + 0x10070a18]`, i.e. the product is taken modulo 2^32 and added to
// the base with no bounds check; the unsigned arithmetic here is the same computation.
float* Normal(int idx, unsigned c) {
    return (float*)(TerrainBase() + 0x00070a18u + (unsigned)idx * 0xcu + c * 4u);
}

// Tile fields used below, all from the instructions cited in each function.
//   +0x24  int   type
//   +0x28  int   sub        (rotation / variation word; Tile::getVariation reads its low byte)
//   +0x34  Tile* n[4]
//   +0x44  int   faceCount, and the address Tile::getFaces returns (a Faces*)
//   +0x48  Face  face[8]    stride 0x38: int v[3] at +0, int texture at +0x24, byte piece at +0x28,
//                           float normal[3] at +0x2c
//   +0x240 int   variation
//   +0x244 byte  normalsDirty
int    TileType(void* t)             { return *(const int*)((unsigned char*)t + 0x24); }
void** TileNeighbour(void* t, int i) { return (void**)((unsigned char*)t + 0x34 + (unsigned)i * 4u); }

// Face `f` of a TILE: vertex slot `v` at Tile + 0x48 + f*0x38 + v*4, normal component `c` at
// Tile + 0x74 + f*0x38 + c*4.
int    TileFaceVertex(void* t, int f, int v) {
    return *(const int*)((unsigned char*)t + 0x48 + (unsigned)f * 0x38u + (unsigned)v * 4u);
}
float* TileFaceNormal(void* t, int f, unsigned c) {
    return (float*)((unsigned char*)t + 0x74 + (unsigned)f * 0x38u + c * 4u);
}
// The same two fields of a face reached through a Faces* (what Tile::getFaces returns): the Faces record starts
// with a dword count, so face[f] sits at Faces + 4 + f*0x38 and its normal at Faces + 0x30 + f*0x38.
unsigned char* FacesFace(void* fs, int f)  { return (unsigned char*)fs + 4 + (unsigned)f * 0x38u; }
float* FacesNormal(void* fs, int f, unsigned c) {
    return (float*)((unsigned char*)fs + 0x30 + (unsigned)f * 0x38u + c * 4u);
}

// ===================================================================================================
// 0x00013670  Tile::blendFaceTextures (VA 0x10013670)
// ===================================================================================================
// __thiscall, one stack DWORD argument (`ret 4` at 0x10013c50), no return value. It rewrites all eight faces
// of THIS tile: each face gets the texture that belongs to the pair of edges meeting at it, so the four
// neighbours and the four diagonals decide the eight textures.
//
// Frame slots (the /GZ fill 0xcccccccc is written over the whole frame by the `rep stosd` at 0x1001368d, then
// each slot is initialised): [ebp-4] this, [ebp-8] code, [ebp-0xc] value, [ebp-0x10] b0, [ebp-0x14] b1,
// [ebp-0x18] b2, [ebp-0x1c] b3, [ebp-0x20] b4, [ebp-0x24] b5, [ebp-0x28] b6, [ebp-0x2c] b7, [ebp-0x30] t0,
// [ebp-0x34] t1, [ebp-0x38] t2, [ebp-0x3c] t3, [ebp-0x40] d0, [ebp-0x44] d1, [ebp-0x48] d2, [ebp-0x4c] d3.
//
// Prologue and the two early returns:
//   the argument -1 is replaced by this->type (cmp 0x100136c1, jne 0x100136c5, load 0x100136ca,
//   store 0x100136cd);
//   type 0x14 returns at once (cmp 0x100136d0, jne 0x100136d4, jmp 0x100136d6 to the epilogue);
//   the four edge kinds are taken first, in the order n[0], n[2], n[3], n[1] (calls at 0x100136e5,
//   0x100136f7, 0x10013709, 0x1001371b of the thunk 0x1000111d = Tile::edgeKind 0x10015650, which returns 0
//   for a NULL argument), each on THIS tile;
//   the four diagonal kinds are set to 0 (0x10013723, 0x1001372a, 0x10013731, 0x10013738);
//   Tile::isSolidType(type) (call 0x10013746 of the thunk 0x10001285 = 0x100154a0) is read as a BYTE
//   (and eax, 0xff at 0x1001374b) and a zero returns (jne 0x10013752, jmp 0x10013754) - that is type 0 and
//   type 9.
//
// The eight b flags: each is set only when g_courseType 0x10070a0c is 1 AND the type in force is 0x11 AND the
// neighbour's Tile::isOpenType (thunk 0x10001311 = 0x100155b0, read as a byte) is true. b0 is n[0]
// (0x10013789), b1 is n[1] (0x1001384d), b2 is n[2] (0x1001390d), b3 is n[3] (0x1001393d), and the four
// diagonals b4 = n[0]->n[2] (0x100137bc), b5 = n[0]->n[3] (0x10013804), b6 = n[1]->n[2] (0x10013880),
// b7 = n[1]->n[3] (0x100138c8). b4..b7 are WRITTEN AND NEVER READ: the C2 transcription's Reads list has no
// [ebp-0x20], [ebp-0x24], [ebp-0x28] or [ebp-0x2c], and the only instructions naming them are those four
// stores and the four zero stores of the prologue.
//
// The inner tests take THIS tile's n[2] / n[3] and then dereference the OTHER tile's n[2] / n[3]: the guard at
// 0x10013790 reads [ecx+0x3c] of this, and the call at 0x100137ae is on [eax+0x3c] of n[0]
// (0x100137a8-0x100137ab). So a neighbour with a NULL n[2] or n[3] faults inside Tile::isOpenType when the
// course-type and type guard holds; when that guard fails the same pointer only reaches Tile::edgeKind, which
// returns 0 for NULL.
//
// The diagonal kinds are taken inside those same blocks, unconditionally on the guard:
//   d0 = edgeKind(n[0]->n[2])   0x100137cd, stored 0x100137d2
//   d1 = edgeKind(n[0]->n[3])   0x10013815, stored 0x1001381a
//   d2 = edgeKind(n[1]->n[2])   0x10013891, stored 0x10013896
//   d3 = edgeKind(n[1]->n[3])   0x100138d9, stored 0x100138de
//
// Then eight identical blocks, one per face, each one:
//   code  = Terrain::flagCode(x, y, d)   call of the thunk 0x10001028 = 0x10013f00, arguments pushed
//                                        right to left (0x10013941-0x1001394c for the first)
//   value = Terrain::idClass(code)       thunk 0x10001262 = 0x10013dd0
//   if (b) type = 0x19;                  (0x10013974 for the first)
//   Tile::setFaceTexture(type, face, value)   thunk 0x1000116d = 0x10012ec0; `value` is its PIECE argument
//   if (b) type = 0x11;                  (0x1001399a for the first)
// The eight (x, y, d, face, flag) tuples, read off the pushes:
//   (t0, t1, d0, 7, b0) 0x10013950   (t0, t2, d1, 0, b0) 0x100139b0   (t2, t0, d1, 1, b3) 0x10013a10
//   (t2, t3, d3, 2, b3) 0x10013a6e   (t3, t2, d3, 3, b1) 0x10013ace   (t3, t1, d2, 4, b1) 0x10013b2e
//   (t1, t0, d0, 6, b2) 0x10013b8c   (t1, t3, d2, 5, b2) 0x10013bec
// The last `if (b2) type = 0x11;` (0x10013c36) is a dead store: `type` is never read again.
typedef int(__fastcall* EdgeKind_t)(void*, void*, void*);
typedef unsigned char(__fastcall* IsSolidType_t)(void*, void*, int);
typedef unsigned char(__fastcall* IsOpenType_t)(void*, void*);
typedef int(__fastcall* FlagCode_t)(void*, void*, int, int, int);
typedef int(__fastcall* IdClass_t)(void*, void*, int);
typedef void(__fastcall* SetFaceTexture_t)(void*, void*, int, int, int);

typedef void(__fastcall* BlendFaceTextures_t)(void*, void*, int);
BlendFaceTextures_t BlendFaceTextures_orig;
void __fastcall BlendFaceTextures_re(void* self, void*, int type) {
    const EdgeKind_t edgeKind = (EdgeKind_t)TerrainRva(0x00015650);
    const IsSolidType_t isSolidType = (IsSolidType_t)TerrainRva(0x000154a0);
    const IsOpenType_t isOpenType = (IsOpenType_t)TerrainRva(0x000155b0);
    const FlagCode_t flagCode = (FlagCode_t)TerrainRva(0x00013f00);
    const IdClass_t idClass = (IdClass_t)TerrainRva(0x00013dd0);
    const SetFaceTexture_t setFaceTexture = (SetFaceTexture_t)TerrainRva(0x00012ec0);
    const int* const courseType = (const int*)(TerrainBase() + 0x00070a0c);

    int b0 = 0, b1 = 0, b2 = 0, b3 = 0;              // [ebp-0x10], -0x14, -0x18, -0x1c
    int b4 = 0, b5 = 0, b6 = 0, b7 = 0;              // [ebp-0x20], -0x24, -0x28, -0x2c: written, never read
    if (type == -1)
        type = TileType(self);                       // 0x100136ca
    if (type == 0x14)
        return;                                      // jmp 0x100136d6

    void* const n0 = *TileNeighbour(self, 0);
    void* const n1 = *TileNeighbour(self, 1);
    void* const n2 = *TileNeighbour(self, 2);
    void* const n3 = *TileNeighbour(self, 3);
    const int t0 = edgeKind(self, 0, n0);            // 0x100136e5
    const int t1 = edgeKind(self, 0, n2);            // 0x100136f7
    const int t2 = edgeKind(self, 0, n3);            // 0x10013709
    const int t3 = edgeKind(self, 0, n1);            // 0x1001371b
    int d0 = 0, d1 = 0, d2 = 0, d3 = 0;              // 0x10013723-0x10013738

    if (!isSolidType(self, 0, type))                 // and eax, 0xff at 0x1001374b
        return;                                      // jmp 0x10013754

    if (n0) {                                        // je 0x10013760
        if (*courseType == 1 && type == 0x11 && isOpenType(n0, 0))
            b0 = 1;                                  // 0x10013789
        if (n2) {                                    // je 0x10013794
            if (*courseType == 1 && type == 0x11 && isOpenType(*TileNeighbour(n0, 2), 0))
                b4 = 1;                              // 0x100137bc
            d0 = edgeKind(self, 0, *TileNeighbour(n0, 2));   // 0x100137cd
        }
        if (n3) {                                    // je 0x100137dc
            if (*courseType == 1 && type == 0x11 && isOpenType(*TileNeighbour(n0, 3), 0))
                b5 = 1;                              // 0x10013804
            d1 = edgeKind(self, 0, *TileNeighbour(n0, 3));   // 0x10013815
        }
    }
    if (n1) {                                        // je 0x10013824
        if (*courseType == 1 && type == 0x11 && isOpenType(n1, 0))
            b1 = 1;                                  // 0x1001384d
        if (n2) {                                    // je 0x10013858
            if (*courseType == 1 && type == 0x11 && isOpenType(*TileNeighbour(n1, 2), 0))
                b6 = 1;                              // 0x10013880
            d2 = edgeKind(self, 0, *TileNeighbour(n1, 2));   // 0x10013891
        }
        if (n3) {                                    // je 0x100138a0
            if (*courseType == 1 && type == 0x11 && isOpenType(*TileNeighbour(n1, 3), 0))
                b7 = 1;                              // 0x100138c8
            d3 = edgeKind(self, 0, *TileNeighbour(n1, 3));   // 0x100138d9
        }
    }
    if (n2 && *courseType == 1 && type == 0x11 && isOpenType(n2, 0))
        b2 = 1;                                      // je 0x100138e8, 0x1001390d
    if (n3 && *courseType == 1 && type == 0x11 && isOpenType(n3, 0))
        b3 = 1;                                      // je 0x10013918, 0x1001393d

    int code, value;
    code = flagCode(self, 0, t0, t1, d0);            // 0x10013950
    value = idClass(self, 0, code);                  // 0x1001395f
    if (b0) type = 0x19;
    setFaceTexture(self, 0, type, 7, value);         // 0x10013988
    if (b0) type = 0x11;

    code = flagCode(self, 0, t0, t2, d1);            // 0x100139b0
    value = idClass(self, 0, code);
    if (b0) type = 0x19;
    setFaceTexture(self, 0, type, 0, value);         // 0x100139e8
    if (b0) type = 0x11;

    code = flagCode(self, 0, t2, t0, d1);            // 0x10013a10
    value = idClass(self, 0, code);
    if (b3) type = 0x19;
    setFaceTexture(self, 0, type, 1, value);         // 0x10013a47
    if (b3) type = 0x11;

    code = flagCode(self, 0, t2, t3, d3);            // 0x10013a6e
    value = idClass(self, 0, code);
    if (b3) type = 0x19;
    setFaceTexture(self, 0, type, 2, value);         // 0x10013aa6
    if (b3) type = 0x11;

    code = flagCode(self, 0, t3, t2, d3);            // 0x10013ace
    value = idClass(self, 0, code);
    if (b1) type = 0x19;
    setFaceTexture(self, 0, type, 3, value);         // 0x10013b06
    if (b1) type = 0x11;

    code = flagCode(self, 0, t3, t1, d2);            // 0x10013b2e
    value = idClass(self, 0, code);
    if (b1) type = 0x19;
    setFaceTexture(self, 0, type, 4, value);         // 0x10013b65
    if (b1) type = 0x11;

    code = flagCode(self, 0, t1, t0, d0);            // 0x10013b8c
    value = idClass(self, 0, code);
    if (b2) type = 0x19;
    setFaceTexture(self, 0, type, 6, value);         // 0x10013bc4
    if (b2) type = 0x11;

    code = flagCode(self, 0, t1, t3, d2);            // 0x10013bec
    value = idClass(self, 0, code);
    if (b2) type = 0x19;
    setFaceTexture(self, 0, type, 5, value);         // 0x10013c24
    if (b2) type = 0x11;                             // dead store kept by /Od (0x10013c36)
    (void)b4; (void)b5; (void)b6; (void)b7;          // the four flags the original also never reads
}

// ===================================================================================================
// 0x00013500  Tile::reblendNeighbourhood (VA 0x10013500)
// ===================================================================================================
// __thiscall, no argument and no return value (plain `ret` at 0x1001361c). Nine calls of
// Tile::blendFaceTextures 0x10013670 through the thunk 0x10001096: this tile with ITS OWN type (load
// 0x10013520, call 0x10013527) and then, each guarded by a pointer test, the eight tiles around it with the
// argument -1, which makes each of them use its own type:
//   n[0]                      je 0x10013533, call 0x1001353d
//   n[0]->n[2]                je 0x10013549 (this->n[0]), je 0x10013552 (THIS tile's n[2]), call 0x1001355f
//   n[2]                      je 0x1001356b, call 0x10013575
//   n[1]->n[2]                je 0x10013581 (this->n[1]), je 0x1001358a (this->n[2]), call 0x10013597
//   n[1]                      je 0x100135a3, call 0x100135ad
//   n[1]->n[3]                je 0x100135b9, je 0x100135c2 (this->n[3]), call 0x100135cf
//   n[3]                      je 0x100135db, call 0x100135e5
//   n[0]->n[3]                je 0x100135f1, je 0x100135fa (this->n[3]), call 0x10013607
// The four diagonal calls test THIS tile's n[2] / n[3] and then dereference the other tile's n[2] / n[3]
// (0x1001355c, 0x10013594, 0x100135cc, 0x10013604), so a neighbour with a NULL n[2] or n[3] hands NULL to
// Tile::blendFaceTextures, whose first dereference `[eax+0x24]` at 0x100136ca faults - in the original as much
// as here. `this` is re-read from [ebp-4] before every test; nothing is cached.
typedef void(__fastcall* ReblendNeighbourhood_t)(void*, void*);
ReblendNeighbourhood_t ReblendNeighbourhood_orig;
void __fastcall ReblendNeighbourhood_re(void* self, void*) {
    const BlendFaceTextures_t blend = (BlendFaceTextures_t)TerrainRva(0x00013670);
    blend(self, 0, TileType(self));                                        // 0x10013527
    if (*TileNeighbour(self, 0))
        blend(*TileNeighbour(self, 0), 0, -1);                             // 0x1001353d
    if (*TileNeighbour(self, 0) && *TileNeighbour(self, 2))
        blend(*TileNeighbour(*TileNeighbour(self, 0), 2), 0, -1);          // 0x1001355f
    if (*TileNeighbour(self, 2))
        blend(*TileNeighbour(self, 2), 0, -1);                             // 0x10013575
    if (*TileNeighbour(self, 1) && *TileNeighbour(self, 2))
        blend(*TileNeighbour(*TileNeighbour(self, 1), 2), 0, -1);          // 0x10013597
    if (*TileNeighbour(self, 1))
        blend(*TileNeighbour(self, 1), 0, -1);                             // 0x100135ad
    if (*TileNeighbour(self, 1) && *TileNeighbour(self, 3))
        blend(*TileNeighbour(*TileNeighbour(self, 1), 3), 0, -1);          // 0x100135cf
    if (*TileNeighbour(self, 3))
        blend(*TileNeighbour(self, 3), 0, -1);                             // 0x100135e5
    if (*TileNeighbour(self, 0) && *TileNeighbour(self, 3))
        blend(*TileNeighbour(*TileNeighbour(self, 0), 3), 0, -1);          // 0x10013607
}

// ===================================================================================================
// 0x00011ef0  Tile::smoothNormals (VA 0x10011ef0)
// ===================================================================================================
// __thiscall, two stack DWORD arguments and no return value (`ret 8` at 0x10012a18). `a` selects one of the
// neighbours n[0] / n[1] and `b` one of n[2] / n[3], so the pair names one corner of the tile; the body
// recomputes the face normals that meet at that corner on this tile, on the two neighbours and on the diagonal
// one, then re-sums the shared vertices' entries of g_normals 0x10070a18 and normalizes them.
//
// Three Faces* locals, [ebp-8] fa, [ebp-0xc] fb, [ebp-0x10] fc, all zero first (0x10011f0d-0x10011f1b):
//   fa = n[a]->getFaces()                           cmp 0x10011f28, je 0x10011f2d, call 0x10011f39
//   fb = n[b]->getFaces()                           cmp 0x10011f47, je 0x10011f4c, call 0x10011f58
//   fc = n[b]->n[a]->getFaces()                     only when n[a] != 0 (cmp 0x10011f66, je 0x10011f6b) and
//                                                   n[b]->n[a] != 0 (cmp 0x10011f7a, je 0x10011f7f),
//                                                   call 0x10011f92
// getFaces is the thunk 0x100010aa = 0x100153c0 and returns the Tile's own +0x44, so a Faces* is a Tile + 0x44
// and its face[f] sits at Faces + 4 + f*0x38. faceNormal is the thunk 0x100012c6 = 0x10011d60 and is called
// with THIS tile in ecx for every face, its own and the neighbours' (`mov ecx, [ebp-4]` before each call);
// normalize is the thunk 0x100011a9 = 0x10037c80, __cdecl.
//
// `a` is tested against 0 (cmp 0x10011f9a, jne 0x10011f9e) and then against 1 (cmp 0x10011f77,
// jne 0x1001257b), so any other value writes nothing; inside each, `b` is tested against 2 and then against 3
// by two SEPARATE ifs (0x10011fa8 and 0x100122e2 for a == 0, 0x10012585 and 0x10012821 for a == 1), so a `b`
// that is neither writes nothing either. The four arms differ only in which faces they refresh and which
// vertex entries they sum; each sum is one `fld`, one `fadd` and one `fstp dword`, so the pair of face normals
// is added at 53 bits and rounded to a float once, and each accumulation is `fld P; fadd Q;
// fadd [g_normals]; fstp [g_normals]`, i.e. (P + Q) + the value already there, rounded once.
typedef void*(__fastcall* GetFaces_t)(void*, void*);
typedef void(__fastcall* FaceNormal_t)(void*, void*, void*);
typedef void(__cdecl* Normalize_t)(float*);

typedef void(__fastcall* SmoothNormals_t)(void*, void*, int, int);
SmoothNormals_t SmoothNormals_orig;
void __fastcall SmoothNormals_re(void* self, void*, int a, int b) {
    unsigned char* const t = (unsigned char*)self;
    const GetFaces_t getFaces = (GetFaces_t)TerrainRva(0x000153c0);
    const FaceNormal_t faceNormal = (FaceNormal_t)TerrainRva(0x00011d60);
    const Normalize_t normalize = (Normalize_t)TerrainRva(0x00037c80);

    void* fa = 0;
    void* fb = 0;
    void* fc = 0;
    if (*TileNeighbour(t, a))
        fa = getFaces(*TileNeighbour(t, a), 0);
    if (*TileNeighbour(t, b)) {
        fb = getFaces(*TileNeighbour(t, b), 0);
        if (*TileNeighbour(t, a) && *TileNeighbour(*TileNeighbour(t, b), a))
            fc = getFaces(*TileNeighbour(*TileNeighbour(t, b), a), 0);
    }

    if (a == 0) {
        if (b == 2) {
            // 0x10011fae-0x10011fed: this tile's faces 0, 1, 2, 4 (Tile + 0x48, 0x80, 0xb8, 0x128)
            faceNormal(self, 0, t + 0x48);
            faceNormal(self, 0, t + 0x80);
            faceNormal(self, 0, t + 0xb8);
            faceNormal(self, 0, t + 0x128);
            if (fa) {                                // je 0x10011ff6
                faceNormal(self, 0, FacesFace(fa, 4));
                faceNormal(self, 0, FacesFace(fa, 5));
                faceNormal(self, 0, FacesFace(fa, 6));
            }
            if (fb) {                                // je 0x10012031
                faceNormal(self, 0, FacesFace(fb, 2));
                faceNormal(self, 0, FacesFace(fb, 3));
                faceNormal(self, 0, FacesFace(fb, 7));
            }
            if (fc) {                                // je 0x10012069
                faceNormal(self, 0, FacesFace(fc, 6));
                faceNormal(self, 0, FacesFace(fc, 7));
            }
            for (int i = 0; i < 3; i++) {            // jge 0x100120a5
                // 0x100120b7-0x100120d4, 0x100120e7-0x100120fe, 0x10012111-0x10012128
                float* const p12 = Normal(TileFaceVertex(t, 1, 2), i);     // index Tile+0x88
                float* const p01 = Normal(TileFaceVertex(t, 0, 1), i);     // index Tile+0x4c
                float* const p00 = Normal(TileFaceVertex(t, 0, 0), i);     // index Tile+0x48
                *p12 = (float)((double)*TileFaceNormal(t, 1, i) + (double)*TileFaceNormal(t, 2, i));
                *p01 = (float)((double)*TileFaceNormal(t, 0, i) + (double)*TileFaceNormal(t, 4, i));
                *p00 = (float)((double)*TileFaceNormal(t, 0, i) + (double)*TileFaceNormal(t, 1, i));
                if (fa) {                            // je 0x10012133
                    *p00 = (float)(((double)*FacesNormal(fa, 4, i) + (double)*FacesNormal(fa, 5, i))
                                   + (double)*p00);
                    *p12 = (float)(((double)*FacesNormal(fa, 5, i) + (double)*FacesNormal(fa, 6, i))
                                   + (double)*p12);
                }
                if (fb) {                            // je 0x100121c3
                    *p00 = (float)(((double)*FacesNormal(fb, 2, i) + (double)*FacesNormal(fb, 3, i))
                                   + (double)*p00);
                    *p01 = (float)(((double)*FacesNormal(fb, 3, i) + (double)*FacesNormal(fb, 7, i))
                                   + (double)*p01);
                }
                if (fc) {                            // je 0x1001224d
                    *p00 = (float)(((double)*FacesNormal(fc, 6, i) + (double)*FacesNormal(fc, 7, i))
                                   + (double)*p00);
                }
            }
            // 0x10012294-0x100122db
            normalize(Normal(TileFaceVertex(t, 0, 0), 0));
            normalize(Normal(TileFaceVertex(t, 0, 1), 0));
            normalize(Normal(TileFaceVertex(t, 1, 2), 0));
        }
        if (b == 3) {
            // 0x100122e8-0x10012318: this tile's faces 2, 3, 7 (Tile + 0xb8, 0xf0, 0x1d0)
            faceNormal(self, 0, t + 0xb8);
            faceNormal(self, 0, t + 0xf0);
            faceNormal(self, 0, t + 0x1d0);
            if (fa) {                                // je 0x10012321
                faceNormal(self, 0, FacesFace(fa, 6));
                faceNormal(self, 0, FacesFace(fa, 7));
            }
            if (fb) {                                // je 0x1001234b
                faceNormal(self, 0, FacesFace(fb, 0));
                faceNormal(self, 0, FacesFace(fb, 4));
            }
            if (fc) {                                // je 0x10012372
                faceNormal(self, 0, FacesFace(fc, 4));
                faceNormal(self, 0, FacesFace(fc, 5));
            }
            for (int i = 0; i < 3; i++) {            // jge 0x100123ad
                // 0x100123bf-0x100123dc, 0x100123ef-0x1001240c
                float* const p31 = Normal(TileFaceVertex(t, 3, 1), i);     // index Tile+0xf4
                float* const p32 = Normal(TileFaceVertex(t, 3, 2), i);     // index Tile+0xf8
                *p31 = (float)((double)*TileFaceNormal(t, 7, i) + (double)*TileFaceNormal(t, 3, i));
                *p32 = (float)((double)*TileFaceNormal(t, 2, i) + (double)*TileFaceNormal(t, 3, i));
                if (fa) {                            // je 0x10012417
                    *p32 = (float)(((double)*FacesNormal(fa, 6, i) + (double)*FacesNormal(fa, 7, i))
                                   + (double)*p32);
                }
                if (fb) {                            // je 0x10012463
                    // fb's face 1 is summed although only its faces 0 and 4 were refreshed above
                    *p32 = (float)(((double)*FacesNormal(fb, 0, i) + (double)*FacesNormal(fb, 1, i))
                                   + (double)*p32);
                    *p31 = (float)(((double)*FacesNormal(fb, 0, i) + (double)*FacesNormal(fb, 4, i))
                                   + (double)*p31);
                }
                if (fc) {                            // je 0x100124f0
                    *p32 = (float)(((double)*FacesNormal(fc, 4, i) + (double)*FacesNormal(fc, 5, i))
                                   + (double)*p32);
                }
            }
            // 0x1001253d-0x1001256f
            normalize(Normal(TileFaceVertex(t, 3, 1), 0));
            normalize(Normal(TileFaceVertex(t, 3, 2), 0));
        }
    } else if (a == 1) {
        if (b == 2) {
            // 0x1001258b-0x100125bb: this tile's faces 4, 5, 6 (Tile + 0x128, 0x160, 0x198)
            faceNormal(self, 0, t + 0x128);
            faceNormal(self, 0, t + 0x160);
            faceNormal(self, 0, t + 0x198);
            if (fa) {                                // je 0x100125c4
                faceNormal(self, 0, FacesFace(fa, 0));
                faceNormal(self, 0, FacesFace(fa, 1));
                faceNormal(self, 0, FacesFace(fa, 2));
            }
            if (fb) {                                // je 0x100125f7
                faceNormal(self, 0, FacesFace(fb, 6));
                faceNormal(self, 0, FacesFace(fb, 7));
            }
            if (fc) {                                // je 0x10012620
                faceNormal(self, 0, FacesFace(fc, 2));
                faceNormal(self, 0, FacesFace(fc, 3));
            }
            for (int i = 0; i < 3; i++) {            // jge 0x10012658
                // 0x1001266a-0x10012687, 0x1001269a-0x100126b7
                float* const p41 = Normal(TileFaceVertex(t, 4, 1), i);     // index Tile+0x12c
                float* const p51 = Normal(TileFaceVertex(t, 5, 1), i);     // index Tile+0x164
                *p41 = (float)((double)*TileFaceNormal(t, 5, i) + (double)*TileFaceNormal(t, 4, i));
                *p51 = (float)((double)*TileFaceNormal(t, 5, i) + (double)*TileFaceNormal(t, 6, i));
                if (fa) {                            // je 0x100126c2
                    *p41 = (float)(((double)*FacesNormal(fa, 0, i) + (double)*FacesNormal(fa, 1, i))
                                   + (double)*p41);
                    *p51 = (float)(((double)*FacesNormal(fa, 1, i) + (double)*FacesNormal(fa, 2, i))
                                   + (double)*p51);
                }
                if (fb) {                            // je 0x1001274f
                    *p41 = (float)(((double)*FacesNormal(fb, 6, i) + (double)*FacesNormal(fb, 7, i))
                                   + (double)*p41);
                }
                if (fc) {                            // je 0x1001279b
                    *p41 = (float)(((double)*FacesNormal(fc, 2, i) + (double)*FacesNormal(fc, 3, i))
                                   + (double)*p41);
                }
            }
            // 0x100127e8-0x1001281a
            normalize(Normal(TileFaceVertex(t, 4, 1), 0));
            normalize(Normal(TileFaceVertex(t, 5, 1), 0));
        }
        if (b == 3) {
            // 0x10012827-0x10012845: this tile's faces 6, 7 (Tile + 0x198, 0x1d0)
            faceNormal(self, 0, t + 0x198);
            faceNormal(self, 0, t + 0x1d0);
            if (fa) {                                // je 0x1001284e
                faceNormal(self, 0, FacesFace(fa, 2));
                faceNormal(self, 0, FacesFace(fa, 3));
            }
            if (fb) {                                // je 0x10012874
                faceNormal(self, 0, FacesFace(fb, 4));
                faceNormal(self, 0, FacesFace(fb, 5));
            }
            if (fc) {                                // je 0x1001289e
                faceNormal(self, 0, FacesFace(fc, 0));
                faceNormal(self, 0, FacesFace(fc, 1));
            }
            for (int i = 0; i < 3; i++) {            // jge 0x100128d4
                // 0x100128e6-0x10012903
                float* const p71 = Normal(TileFaceVertex(t, 7, 1), i);     // index Tile+0x1d4
                *p71 = (float)((double)*TileFaceNormal(t, 7, i) + (double)*TileFaceNormal(t, 6, i));
                if (fa) {                            // je 0x1001290e
                    *p71 = (float)(((double)*FacesNormal(fa, 2, i) + (double)*FacesNormal(fa, 3, i))
                                   + (double)*p71);
                }
                if (fb) {                            // je 0x1001295a
                    *p71 = (float)(((double)*FacesNormal(fb, 4, i) + (double)*FacesNormal(fb, 5, i))
                                   + (double)*p71);
                }
                if (fc) {                            // je 0x100129a6
                    *p71 = (float)(((double)*FacesNormal(fc, 0, i) + (double)*FacesNormal(fc, 1, i))
                                   + (double)*p71);
                }
            }
            normalize(Normal(TileFaceVertex(t, 7, 1), 0));     // 0x100129ed-0x10012a05
        }
    }
}

}  // namespace

SG_HOOK("Terrain.dll", 0x00013670, Tile_blendFaceTextures, BlendFaceTextures_re, BlendFaceTextures_orig);
SG_HOOK("Terrain.dll", 0x00013500, Tile_reblendNeighbourhood, ReblendNeighbourhood_re, ReblendNeighbourhood_orig);
SG_HOOK("Terrain.dll", 0x00011ef0, Tile_smoothNormals, SmoothNormals_re, SmoothNormals_orig);

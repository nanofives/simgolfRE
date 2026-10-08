// Batch c3bt: Terrain.dll, round 18 - the exported Terrain methods golf_clean.exe calls. The addresses in
// SG_HOOK and in the `// 0x...` comment lines are RVAs; every instruction address cited in a comment is a VA
// (VA = 0x10000000 + RVA), the form the debug build's disassembly prints. Terrain.dll is a static import of
// golf_clean.exe, so it is mapped before the shim installs its hooks, and it loads at its preferred base
// 0x10000000 (measured by batch c3bf, log/c3/alt/c3bf_probe.py).
//
// Terrain.dll is a /Od /ZI /GZ /GX build: every body is literal, so each reimplementation below follows the
// original's order of loads, compares, calls and stores, and each fact carries the VA it comes from.
// __thiscall is emulated with __fastcall (ecx = this, edx unused).
//
// Twelve functions, all of them one- or two-call wrappers around Tile methods except the last three:
//   Terrain::setType           0x000032f0  pick a variation, set the rotation, set the type id
//   Terrain::getVariation      0x00003390  Tile::getVariation, sign-extended
//   Terrain::layPath           0x0000a3e0  Tile::layPath with a literal true / false
//   Terrain::hasConnectedPath  0x0000a450  Tile::isConnected of tileAt(x, y)
//   Terrain::updatePath        0x0000a4b0  Tile::setConnected of tileAt(x, y)
//   Terrain::hasPath           0x0000a510  Tile::hasPath
//   Terrain::setWall           0x0000a560  Tile::setWall
//   Terrain::elevateCorner     0x0000a5c0  Tile::elevateCorner, guarded by a NULL test
//   Terrain::lowerCorner       0x0000a620  Tile::lowerCorner, guarded by a NULL test
//   Terrain::lowerEdgeCorner   0x0000a680  Terrain::lowerEdge then Tile::lowerCorner
//   Terrain::calcNormals       0x0000a6f0  Tile::calcNormals, guarded by a NULL test
//   Terrain::calcAllNormals    0x0000a740  Tile::calcNormals over a square window of tileAt results
//
// THE OBJECT LAYOUTS USED BELOW (every offset from an instruction cited in the function that uses it)
//   Terrain  +0x14 width, +0x18 height, +0x1c the shift count of calcAllNormals' window (0x1000a765),
//            +0x40 a table of 0x18-byte records whose first dword is a variation count (0x10003316),
//            +0x3a4 the tile array, stride 0x248 (Terrain::tileAt, 0x10001da2 / 0x10001dab)
//   Tile     +0x24 type, +0x28 the byte Tile::getVariation returns, +0x2c x, +0x30 y, +0x34 four neighbours,
//            +0x208 hasPath, +0x209 connected, +0x20a the path-type flag, +0x210 nine wall heights,
//            +0x234 nine wall flags, +0x240 variation, +0x244 normals-dirty
//
// THE MODULE DATA THESE BODIES TOUCH (all of it through the callees, none of it written by this file)
//   g_vertices        0x100b28c8  written by Tile::elevateCorner / Tile::lowerCorner / the type walks
//   g_normals         0x10070a18  written by Tile::calcNormals
//   g_textures        0x100687f8  read by Tile::resetFaceTextures and Tile::setFaceTexture
//   g_courseType      0x10070a0c  read by Tile::blendFaceTextures under Tile::setTypeId
//   g_collarInfo      0x10106b48  read by Tile::edgeKind under Tile::setTypeId
//   g_viewAngleIndex  0x10070a14  written by Terrain::setViewAngle under Terrain::lowerEdge
//   the CRT per-thread random seed at _getptd() + 0x14, read and written by rand 0x10018ce0 under
//   Terrain::setType (0x10018cef, 0x10018d01)
// Every window the vectors touch is a state region of the registry entry, so diff_hook snapshots it, compares
// it after each arm and writes it back. All of it is idle at the main menu where diff_hook runs: batch c3ba's
// probe counted zero calls into thirteen Terrain.dll entry points over 8 s and no change in the arrays, and
// batch c3bf's probe the same for g_courseType and g_collarInfo.
//
// Every callee is reached through its own address in the mapped module, so both arms of the A/B run the same
// callee code. Most of those callees are themselves hooked by earlier batches; the verifier re-runs these keys
// with SIMGOLF_HOOKS_OFF so the original arm uses original callees (the list is in re/frida/registry.d/c3bt.py).
#include "hooks.h"

namespace {

// Terrain.dll has base relocations, so its data and callees are reached through the mapped module.
unsigned char* TerrainBase() {
    static unsigned char* base = 0;
    if (!base) base = (unsigned char*)GetModuleHandleA("Terrain.dll");
    return base;
}
void* TerrainRva(unsigned long rva) { return TerrainBase() + rva; }

// ---- the callees, with the thunk each original goes through -------------------------------------------
typedef char(__fastcall* TileGetVariation_t)(void*, void*);              // 0x10015340, thunk 0x100010e6
typedef void(__fastcall* TileSetVariation_t)(void*, void*, int);         // 0x10002f80, thunk 0x100010b9
typedef void(__fastcall* TileSetRotation_t)(void*, void*, int);          // 0x10015380, thunk 0x10001050
typedef void(__fastcall* TileSetTypeId_t)(void*, void*, int);            // 0x10014020, thunk 0x10001235
typedef void(__fastcall* TileLayPath_t)(void*, void*, int, int);         // 0x10013400, thunk 0x1000128a
typedef unsigned char(__fastcall* TileHasPath_t)(void*, void*);          // 0x10013320, thunk 0x100010eb
typedef unsigned char(__fastcall* TileIsConnected_t)(void*, void*);      // 0x10013360, thunk 0x1000132a
typedef void(__fastcall* TileSetConnected_t)(void*, void*, int);         // 0x100133a0, thunk 0x10001131
typedef void(__fastcall* TileSetWall_t)(void*, void*, int, int, int);    // 0x10015400, thunk 0x100011d1
typedef void(__fastcall* TileCorner_t)(void*, void*, int);               // 0x1000c7b0 / 0x1000ccc0
typedef void(__fastcall* TileCalcNormals_t)(void*, void*);               // 0x10012cf0, thunk 0x100010f5
typedef int(__fastcall* TileGetXY_t)(void*, void*);                      // 0x10005960 / 0x10006810
typedef void*(__fastcall* TerrainTileAt_t)(void*, void*, int, int);      // 0x10001d50, thunk 0x1000108c
typedef void(__fastcall* TerrainLowerEdge_t)(void*, void*, void*, void*, unsigned int);  // 0x10038900,
                                                                         // thunk 0x10001118
typedef int(__cdecl* Rand_t)(void);                                      // 0x10018ce0

char TileGetVariation(void* t)   { return ((TileGetVariation_t)TerrainRva(0x00015340))(t, 0); }
void TileSetVariation(void* t, int v) { ((TileSetVariation_t)TerrainRva(0x00002f80))(t, 0, v); }
void TileSetRotation(void* t, int r)  { ((TileSetRotation_t)TerrainRva(0x00015380))(t, 0, r); }
void TileSetTypeId(void* t, int ty)   { ((TileSetTypeId_t)TerrainRva(0x00014020))(t, 0, ty); }
void TileLayPath(void* t, int on, int dir) { ((TileLayPath_t)TerrainRva(0x00013400))(t, 0, on, dir); }
unsigned char TileHasPath(void* t)    { return ((TileHasPath_t)TerrainRva(0x00013320))(t, 0); }
unsigned char TileIsConnected(void* t){ return ((TileIsConnected_t)TerrainRva(0x00013360))(t, 0); }
void TileSetConnected(void* t, int on){ ((TileSetConnected_t)TerrainRva(0x000133a0))(t, 0, on); }
void TileSetWall(void* t, int side, int h, int on) {
    ((TileSetWall_t)TerrainRva(0x00015400))(t, 0, side, h, on);
}
void TileElevateCorner(void* t, int c){ ((TileCorner_t)TerrainRva(0x0000c7b0))(t, 0, c); }
void TileLowerCorner(void* t, int c)  { ((TileCorner_t)TerrainRva(0x0000ccc0))(t, 0, c); }
void TileCalcNormals(void* t)         { ((TileCalcNormals_t)TerrainRva(0x00012cf0))(t, 0); }
int  TileGetX(void* t)                { return ((TileGetXY_t)TerrainRva(0x00005960))(t, 0); }
int  TileGetY(void* t)                { return ((TileGetXY_t)TerrainRva(0x00006810))(t, 0); }
void* TerrainTileAt(void* self, int x, int y) {
    return ((TerrainTileAt_t)TerrainRva(0x00001d50))(self, 0, x, y);
}
void TerrainLowerEdge(void* self, void* t, void* pivot, unsigned int angleBits) {
    ((TerrainLowerEdge_t)TerrainRva(0x00038900))(self, 0, t, pivot, angleBits);
}
int Rand() { return ((Rand_t)TerrainRva(0x00018ce0))(); }

// ===================================================================================================
// 0x00003390  Terrain::getVariation (VA 0x10003390)
// ===================================================================================================
// __thiscall, one stack DWORD argument (`ret 4` at 0x100033c8). `this` is stored in the frame slot
// (0x100033aa) and never read again: the body loads the Tile argument into ecx (0x100033ad), calls
// Tile::getVariation through the thunk 0x100010e6 (call 0x100033b0) and SIGN-EXTENDS the byte the callee
// leaves in al (movsx eax, al at 0x100033b5), so a stored byte of 0x80..0xff comes back as -128..-1. No
// branch.
typedef int(__fastcall* GetVariation_t)(void*, void*, void*);
GetVariation_t GetVariation_orig;
int __fastcall GetVariation_re(void*, void*, void* t) {
    return TileGetVariation(t);                        // call 0x100033b0, movsx 0x100033b5
}

// ===================================================================================================
// 0x0000a510  Terrain::hasPath (VA 0x1000a510)
// ===================================================================================================
// __thiscall, one stack DWORD argument (`ret 4` at 0x1000a545). `this` is stored at [ebp-4] (0x1000a52a) and
// never read: the Tile argument goes into ecx (0x1000a52d) and Tile::hasPath is called through the thunk
// 0x100010eb (call 0x1000a530). Nothing touches eax afterwards, so the callee's al - the byte at Tile+0x208 -
// is the return value. No branch.
typedef unsigned char(__fastcall* HasPath_t)(void*, void*, void*);
HasPath_t HasPath_orig;
unsigned char __fastcall HasPath_re(void*, void*, void* t) {
    return TileHasPath(t);                             // call 0x1000a530
}

// ===================================================================================================
// 0x0000a3e0  Terrain::layPath (VA 0x1000a3e0)
// ===================================================================================================
// __thiscall, three stack DWORD arguments (`ret 0xc` at 0x1000a431). `this` is stored at [ebp-4] (0x1000a3fa)
// and never read. The second argument is tested against 0 (cmp 0x1000a3fd, je 0x1000a401) and BOTH arms call
// the same Tile::layPath through the thunk 0x1000128a with the third argument and a LITERAL flag: 1 on the
// non-zero side (push 1 at 0x1000a407, call 0x1000a40c), 0 on the zero side (push 0 at 0x1000a417, call
// 0x1000a41c). So any non-zero value of the second argument is collapsed to 1 before the callee sees it.
typedef void(__fastcall* LayPath_t)(void*, void*, void*, int, int);
LayPath_t LayPath_orig;
void __fastcall LayPath_re(void*, void*, void* t, int on, int dir) {
    if (on)                                            // je 0x1000a401
        TileLayPath(t, 1, dir);                        // push 1 0x1000a407, call 0x1000a40c
    else
        TileLayPath(t, 0, dir);                        // push 0 0x1000a417, call 0x1000a41c
}

// ===================================================================================================
// 0x0000a560  Terrain::setWall (VA 0x1000a560)
// ===================================================================================================
// __thiscall, four stack DWORD arguments (`ret 0x10` at 0x1000a5a1). `this` is stored at [ebp-4] (0x1000a57a)
// and never read. The fourth argument is loaded as a BYTE into al (mov al, byte [ebp+0x14] at 0x1000a57d) and
// pushed as a dword (0x1000a580) whose upper three bytes are whatever eax held - the /GZ fill constant
// 0xcccccccc of 0x1000a577, which the rep stosd at 0x1000a577 leaves in eax - and Tile::setWall reads only the
// low byte of that slot (mov cl, byte [ebp+0x10] at 0x10015423), so only the low byte is observable. The other
// three arguments are pushed unchanged (0x1000a584, 0x1000a588) and the Tile goes into ecx (0x1000a589) for
// the call through the thunk 0x100011d1 (0x1000a58c). No branch.
typedef void(__fastcall* SetWall_t)(void*, void*, void*, int, int, int);
SetWall_t SetWall_orig;
void __fastcall SetWall_re(void*, void*, void* t, int side, int height, int on) {
    TileSetWall(t, side, height, (unsigned char)on);   // mov al 0x1000a57d, call 0x1000a58c
}

// ===================================================================================================
// 0x0000a5c0  Terrain::elevateCorner (VA 0x1000a5c0)
// ===================================================================================================
// __thiscall, two stack DWORD arguments (`ret 8` at 0x1000a5ff). `this` is stored at [ebp-4] (0x1000a5da) and
// never read. The Tile argument is tested against 0 (cmp 0x1000a5dd, je 0x1000a5ef) and, when it is not NULL,
// the corner argument is pushed (0x1000a5e6) and Tile::elevateCorner is called through the thunk 0x100010fa
// (0x1000a5ea). A NULL Tile does nothing at all.
typedef void(__fastcall* ElevateCorner_t)(void*, void*, void*, int);
ElevateCorner_t ElevateCorner_orig;
void __fastcall ElevateCorner_re(void*, void*, void* t, int corner) {
    if (t)                                             // je 0x1000a5e1
        TileElevateCorner(t, corner);                  // call 0x1000a5ea
}

// ===================================================================================================
// 0x0000a620  Terrain::lowerCorner (VA 0x1000a620)
// ===================================================================================================
// The same shape as Terrain::elevateCorner, with Tile::lowerCorner through the thunk 0x100010dc: `ret 8` at
// 0x1000a65f, `this` stored at 0x1000a63a and never read, cmp 0x1000a63d / je 0x1000a641, push of the corner
// at 0x1000a646 and call at 0x1000a64a.
typedef void(__fastcall* LowerCorner_t)(void*, void*, void*, int);
LowerCorner_t LowerCorner_orig;
void __fastcall LowerCorner_re(void*, void*, void* t, int corner) {
    if (t)                                             // je 0x1000a641
        TileLowerCorner(t, corner);                    // call 0x1000a64a
}

// ===================================================================================================
// 0x0000a6f0  Terrain::calcNormals (VA 0x1000a6f0)
// ===================================================================================================
// __thiscall, one stack DWORD argument (`ret 4` at 0x1000a72b). `this` is stored at [ebp-4] (0x1000a70a) and
// never read. The Tile argument is tested against 0 (cmp 0x1000a70d, je 0x1000a711) and, when it is not NULL,
// goes into ecx (0x1000a713) for the call of Tile::calcNormals through the thunk 0x100010f5 (0x1000a716). No
// argument is pushed.
typedef void(__fastcall* CalcNormals_t)(void*, void*, void*);
CalcNormals_t CalcNormals_orig;
void __fastcall CalcNormals_re(void*, void*, void* t) {
    if (t)                                             // je 0x1000a711
        TileCalcNormals(t);                            // call 0x1000a716
}

// ===================================================================================================
// 0x0000a450  Terrain::hasConnectedPath (VA 0x1000a450)
// ===================================================================================================
// __thiscall, two stack DWORD arguments (`ret 8` at 0x1000a494). The two arguments are pushed in the order
// second (0x1000a470) then first (0x1000a474), i.e. tileAt(first, second), with `this` in ecx from the frame
// slot (0x1000a475) for the call through the thunk 0x1000108c (0x1000a478). The returned pointer goes straight
// into ecx (0x1000a47d) and Tile::isConnected is called through the thunk 0x1000132a (0x1000a47f): there is NO
// NULL test, so a coordinate outside the grid - which Terrain::tileAt answers with 0 (xor eax, eax at
// 0x10001d8f) - makes the original read through a null pointer at 0x10013380. The callee's al, the byte at
// Tile+0x209, is the return value.
typedef unsigned char(__fastcall* HasConnectedPath_t)(void*, void*, int, int);
HasConnectedPath_t HasConnectedPath_orig;
unsigned char __fastcall HasConnectedPath_re(void* self, void*, int x, int y) {
    return TileIsConnected(TerrainTileAt(self, x, y)); // call 0x1000a478, call 0x1000a47f
}

// ===================================================================================================
// 0x0000a4b0  Terrain::updatePath (VA 0x1000a4b0)
// ===================================================================================================
// __thiscall, three stack DWORD arguments (`ret 0xc` at 0x1000a4f8). The THIRD argument is pushed first
// (0x1000a4d0) and stays on the stack across the tileAt call, which pops only its own two (second at
// 0x1000a4d4, first at 0x1000a4d8, `ret 8`), so it is the argument of the second call. tileAt runs with `this`
// in ecx through the thunk 0x1000108c (0x1000a4dc), its result goes into ecx (0x1000a4e1) and
// Tile::setConnected is called through the thunk 0x10001131 (0x1000a4e3). There is no NULL test here either.
typedef void(__fastcall* UpdatePath_t)(void*, void*, int, int, int);
UpdatePath_t UpdatePath_orig;
void __fastcall UpdatePath_re(void* self, void*, int x, int y, int on) {
    TileSetConnected(TerrainTileAt(self, x, y), on);   // call 0x1000a4dc, call 0x1000a4e3
}

// ===================================================================================================
// 0x0000a680  Terrain::lowerEdgeCorner (VA 0x1000a680)
// ===================================================================================================
// __thiscall, four stack DWORD arguments (`ret 0x10` at 0x1000a6d3). The first argument is tested against 0
// (cmp 0x1000a69d, je 0x1000a6c3) and a NULL one does nothing. Otherwise two calls run in order:
//   Terrain::lowerEdge(first, third, fourth) with `this` in ecx - the fourth argument is pushed first
//     (0x1000a6a6), then the third (0x1000a6aa), then the first (0x1000a6ae), ecx from the frame slot
//     (0x1000a6af), call of the thunk 0x10001118 at 0x1000a6b2. The fourth argument is only moved as a dword
//     here, never loaded into the FPU, so this body copies its bits.
//   Tile::lowerCorner(second) on the first argument - push 0x1000a6ba, ecx 0x1000a6bb, call of the thunk
//     0x100010dc at 0x1000a6be.
typedef void(__fastcall* LowerEdgeCorner_t)(void*, void*, void*, int, void*, unsigned int);
LowerEdgeCorner_t LowerEdgeCorner_orig;
void __fastcall LowerEdgeCorner_re(void* self, void*, void* t, int corner, void* pivot, unsigned int angleBits) {
    if (t) {                                           // je 0x1000a6a1
        TerrainLowerEdge(self, t, pivot, angleBits);   // call 0x1000a6b2
        TileLowerCorner(t, corner);                    // call 0x1000a6be
    }
}

// ===================================================================================================
// 0x0000a740  Terrain::calcAllNormals (VA 0x1000a740)
// ===================================================================================================
// __thiscall, one stack DWORD argument (`ret 4` at 0x1000a806). It runs Tile::calcNormals over a square window
// of the grid centred on the argument tile's coordinates.
//
// The half-width is 13 shifted left by the dword at this+0x1c (mov edx, 0xd at 0x1000a760, mov ecx, [eax+0x1c]
// at 0x1000a765, shl edx, cl at 0x1000a768), and the same value is copied into four frame slots
// ([ebp-0x18], [ebp-0x14], [ebp-0x10], [ebp-0xc] at 0x1000a76a..0x1000a77c), which the two loops then use
// separately. `shl r32, cl` takes the count modulo 32, which `& 31` reproduces.
//
// The outer loop runs y from Tile::getY() - [ebp-0xc] (call 0x1000a782, sub 0x1000a787) while
// y < Tile::getY() + [ebp-0x10] (call 0x1000a79b, add 0x1000a7a0, cmp/jge 0x1000a7a3), re-reading the tile's y
// on EVERY test. The inner loop runs x from Tile::getX() + [ebp-0x18] (call 0x1000a7ab, add 0x1000a7b0)
// downwards while x > Tile::getX() - [ebp-0x14] (call 0x1000a7c4, sub 0x1000a7c9, cmp/jle 0x1000a7cc), again
// re-reading the tile's x on every test. With the four slots equal, that is 26 << n values of each coordinate.
//
// Inside, tileAt(x, y) is called through the thunk 0x1000108c (pushes 0x1000a7d4 for y and 0x1000a7d8 for x,
// call 0x1000a7dc), its result is kept at [ebp-8] (0x1000a7e1) and, when it is not NULL (cmp 0x1000a7e4,
// je 0x1000a7e8), Tile::calcNormals is called on it through the thunk 0x100010f5 (0x1000a7ed). The argument
// tile itself is only read for its coordinates; it is never passed to a callee.
typedef void(__fastcall* CalcAllNormals_t)(void*, void*, void*);
CalcAllNormals_t CalcAllNormals_orig;
void __fastcall CalcAllNormals_re(void* self, void*, void* t) {
    const int step = 13 << (*(const int*)((unsigned char*)self + 0x1c) & 31);   // 0x1000a760-0x1000a768
    const int a = step, b = step, c = step, d = step;                          // 0x1000a76a-0x1000a77c
    for (int y = TileGetY(t) - a; y < TileGetY(t) + b; y++) {                  // 0x1000a782, 0x1000a79b
        for (int x = TileGetX(t) + d; x > TileGetX(t) - c; x--) {              // 0x1000a7ab, 0x1000a7c4
            void* const n = TerrainTileAt(self, x, y);                         // call 0x1000a7dc
            if (n)                                                             // je 0x1000a7e8
                TileCalcNormals(n);                                            // call 0x1000a7ed
        }
    }
}

// ===================================================================================================
// 0x000032f0  Terrain::setType (VA 0x100032f0)
// ===================================================================================================
// __thiscall, three stack DWORD arguments (`ret 0xc` at 0x1000336d). It gives a tile a variation, a rotation
// and a type, in that order.
//
// The second argument indexes a table of 0x18-byte records inside the Terrain object: imul eax, eax, 0x18 at
// 0x10003310 and cmp dword [ecx + eax + 0x40], 0 at 0x10003316, so the record's first dword is a count. The
// test is SIGNED (jle 0x1000331b): a count of zero or less takes the second arm.
//   count > 0:  rand() is called (0x1000331d), the index is recomputed (imul 0x10003325) and the remainder of
//               the SIGNED division by the count is taken (cdq 0x1000332b, idiv 0x1000332c) and pushed
//               (0x10003330) for Tile::setVariation through the thunk 0x100010b9 (call 0x10003334). rand is
//               the module's own CRT routine 0x10018ce0, which reads and rewrites the per-thread seed at
//               _getptd() + 0x14 (0x10018cef, 0x10018d01), so it is both an input and an output of this body.
//   count <= 0: the literal 0 is pushed (0x1000333b) and the same Tile::setVariation is called (0x10003340).
// Then the third argument is pushed (0x10003348) for Tile::setRotation through the thunk 0x10001050
// (0x1000334c), and the second argument again (0x10003354) for Tile::setTypeId through the thunk 0x10001235
// (0x10003358). Nothing bounds the second argument against the table's length.
typedef void(__fastcall* SetType_t)(void*, void*, void*, int, int);
SetType_t SetType_orig;
void __fastcall SetType_re(void* self, void*, void* t, int type, int rotation) {
    // the record's first dword, at Terrain + 0x40 + type * 0x18 (0x10003310, 0x10003316)
    const int* const count = (const int*)((unsigned char*)self + 0x40 + (unsigned)type * 0x18u);
    if (*count > 0)                                    // jle 0x1000331b
        TileSetVariation(t, Rand() % *count);          // call 0x1000331d, idiv 0x1000332c, call 0x10003334
    else
        TileSetVariation(t, 0);                        // push 0 0x1000333b, call 0x10003340
    TileSetRotation(t, rotation);                      // call 0x1000334c
    TileSetTypeId(t, type);                            // call 0x10003358
}

}  // namespace

// 0x000032f0  Terrain::setType
SG_HOOK("Terrain.dll", 0x000032f0, Terrain_setType, SetType_re, SetType_orig);
// 0x00003390  Terrain::getVariation
SG_HOOK("Terrain.dll", 0x00003390, Terrain_getVariation, GetVariation_re, GetVariation_orig);
// 0x0000a3e0  Terrain::layPath
SG_HOOK("Terrain.dll", 0x0000a3e0, Terrain_layPath, LayPath_re, LayPath_orig);
// 0x0000a450  Terrain::hasConnectedPath
SG_HOOK("Terrain.dll", 0x0000a450, Terrain_hasConnectedPath, HasConnectedPath_re, HasConnectedPath_orig);
// 0x0000a4b0  Terrain::updatePath
SG_HOOK("Terrain.dll", 0x0000a4b0, Terrain_updatePath, UpdatePath_re, UpdatePath_orig);
// 0x0000a510  Terrain::hasPath
SG_HOOK("Terrain.dll", 0x0000a510, Terrain_hasPath, HasPath_re, HasPath_orig);
// 0x0000a560  Terrain::setWall
SG_HOOK("Terrain.dll", 0x0000a560, Terrain_setWall, SetWall_re, SetWall_orig);
// 0x0000a5c0  Terrain::elevateCorner
SG_HOOK("Terrain.dll", 0x0000a5c0, Terrain_elevateCorner, ElevateCorner_re, ElevateCorner_orig);
// 0x0000a620  Terrain::lowerCorner
SG_HOOK("Terrain.dll", 0x0000a620, Terrain_lowerCorner, LowerCorner_re, LowerCorner_orig);
// 0x0000a680  Terrain::lowerEdgeCorner
SG_HOOK("Terrain.dll", 0x0000a680, Terrain_lowerEdgeCorner, LowerEdgeCorner_re, LowerEdgeCorner_orig);
// 0x0000a6f0  Terrain::calcNormals
SG_HOOK("Terrain.dll", 0x0000a6f0, Terrain_calcNormals, CalcNormals_re, CalcNormals_orig);
// 0x0000a740  Terrain::calcAllNormals
SG_HOOK("Terrain.dll", 0x0000a740, Terrain_calcAllNormals, CalcAllNormals_re, CalcAllNormals_orig);

// Terrain accessors exported by golf_clean.exe (Terrain.dll calls back into them by name).
// Analysis: re/analysis/terrain/004490d0_tileAt.md
#include "hooks.h"

namespace {

// __thiscall is emulated with __fastcall: ecx = this, edx = unused.
typedef void*(__fastcall* TileAt_t)(void* self, void* edx, int x, int y);
TileAt_t TileAt_orig;

// 0x004490d0  ?tileAt@Terrain@@QAEPAVTile@@HH@Z  (export ordinal 4)
// Reads this+0x14 (width) and this+0x18 (height); tiles are 0x248 bytes each, row-major, starting at this+0x3a4.
// Out-of-range x or y (negative, or >= the bound) returns NULL.
void* __fastcall TileAt_re(void* self, void*, int x, int y) {
    const char* t = static_cast<const char*>(self);
    const int width = *reinterpret_cast<const int*>(t + 0x14);
    const int height = *reinterpret_cast<const int*>(t + 0x18);
    if (x < width && x > -1 && y < height && y > -1)
        return const_cast<char*>(t) + (width * y + x) * 0x248 + 0x3a4;
    return nullptr;
}

}  // namespace

#ifdef SG_VC6_TERRAIN
// Detour = the ORIGINAL compiler's output for the matched source re/match/terrain.cpp (VC6 cl 12.00.8168,
// /O2 /Zl, built by shimuild_vc6.bat). That source matches 0x004490d0 at 100%, so the hook runs the
// same instructions the game shipped with, relocated into this DLL.
class Tile;
class Terrain {
public:
    Tile* tileAt(int x, int y);  // defined in buildc6	errain.obj
};
static void* Vc6TileAt() {
    Tile* (Terrain::*p)(int, int) = &Terrain::tileAt;
    return *reinterpret_cast<void**>(&p);  // single-inheritance member pointer: just the code address
}
#define TILEAT_DETOUR Vc6TileAt()
#else
#define TILEAT_DETOUR TileAt_re  // hand-written fallback when VC6 is not available
#endif

SG_HOOK("golf_clean.exe", 0x004490d0, Terrain_tileAt, TILEAT_DETOUR, TileAt_orig);

namespace {
TileAt_t TileAtDll_orig;
}  // namespace

// 0x00001d50  (Terrain.dll RVA) body of ?tileAt@Terrain@@QAEPAVTile@@HH@Z (the export at RVA 0x108c is an
// incremental-linking `jmp 0x10001d50`). Debug build, same semantics as 0x004490d0 in golf_clean.exe:
// bounds checks on this+0x14 / this+0x18 (0x10001d73 / 0x10001d84), stride 0x248 (0x10001da2), base
// this+0x3a4 (0x10001dab), ret 8 (0x10001db8). This is the LIVE copy (22046 calls in 20 s of sandbox).
SG_HOOK("Terrain.dll", 0x00001d50, TerrainDll_tileAt, TILEAT_DETOUR, TileAtDll_orig);

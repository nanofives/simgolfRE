// C3 batch c3h (2026-10-06) of golf_clean.exe: render-subsystem leaves, writers and table lookups.
// Hand-written from the disassembly (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr> --list) and the
// 100%-matched sources in re/match/*.cpp; each body cites the address of every global, offset and callee it uses.
// __thiscall is emulated with __fastcall (ecx = this, edx unused). Callees are invoked through their original
// addresses, so a hooked callee still runs its own reimplementation and the path-1 A/B drives both arms.
#include "hooks.h"

namespace {

template <typename T> T at(unsigned addr) { return *reinterpret_cast<const T*>(addr); }

typedef int(__cdecl* Int1_t)(int);

// 0x00461810  brightenComponent(v): a colour component brightened by 0x20, saturating at 0xff. v + 0x20 while
// v < 0xdf (0x00461814 cmp 0xdf / jge), else 0xff (0x0046181f).
Int1_t Brighten_orig;
int __cdecl Brighten_re(int v) {
    if (v < 0xdf) return v + 0x20;
    return 0xff;
}

// 0x0040a130  setGrids(x, y, a, b): stores the low byte of a at 0x005830b8 + x*50 + y and the low byte of b at
// 0x0059c090 + x*50 + y (cell = x*50 + y, built as x*25*2 + y at 0x0040a13c..0x0040a142; stores 0x0040a149 /
// 0x0040a14f). Both tables are 50x50 byte grids.
typedef void(__cdecl* SetGrids_t)(int, int, int, int);
SetGrids_t SetGrids_orig;
void __cdecl SetGrids_re(int x, int y, int a, int b) {
    const unsigned cell = x * 50 + y;
    *reinterpret_cast<unsigned char*>(0x005830b8 + cell) = static_cast<unsigned char>(a);
    *reinterpret_cast<unsigned char*>(0x0059c090 + cell) = static_cast<unsigned char>(b);
}

// 0x0040bf00  setLinePoint(a, b): stores a at 0x005a412c and b at 0x005a4130 (the current line-drawing point,
// 0x0040bf08 / 0x0040bf0d).
typedef void(__cdecl* Set2_t)(int, int);
Set2_t SetLinePoint_orig;
void __cdecl SetLinePoint_re(int a, int b) {
    *reinterpret_cast<int*>(0x005a412c) = a;
    *reinterpret_cast<int*>(0x005a4130) = b;
}

// 0x004833f0  Table483::find(this, key): scans the 5 entries (stride 0x10) whose key field is at this+8
// (0x004833f7 lea edx, [ecx+8]; 0x004833fa mov ecx, [edx]). Returns the index of the first entry whose key
// equals `key` or whose key is -1 (0x004833fc / 0x00483400), or 5 when neither is found (0x00483409 cmp 5).
typedef int(__fastcall* TableFind_t)(void*, void*, int);
TableFind_t TableFind_orig;
int __fastcall TableFind_re(void* ecx, void*, int key) {
    const unsigned base = reinterpret_cast<unsigned>(ecx) + 8;
    for (int i = 0; i < 5; i++) {
        const int k = at<int>(base + i * 0x10);
        if (k == key || k == -1) return i;
    }
    return 5;
}

// 0x0042fb90  worldToScreen(wx, wy, sx, sy, flag): world->screen transform (reads camera 0x004c2ba0/0x004c2ba4,
// zoom 0x004c2844, viewport 0x00822c8c/0x00822c90). Called below through its original address.
typedef int(__cdecl* WorldToScreen_t)(int, int, int*, int*, int);
const WorldToScreen_t kWorldToScreen = reinterpret_cast<WorldToScreen_t>(0x0042fb90);

// 0x0042f940  tileToScreen(tx, ty, sx, sy): screen position of the centre of tile (tx, ty), cached per tile.
// Returns 0 unless 0 <= tx,ty < 50 (0x0042f94b..0x0042f966). cell = tx*50 + ty. The cache is the short grids
// g_tileSX at 0x0055eb40 and g_tileSY at 0x0055fec8 (indexed by cell*2, 0x0042f978 / 0x0042f99c). A cached X
// that is nonzero and not -99 returns it: *sx = X, *sy = Y, return 1 (0x0042f984). A zero cell is uncached:
// worldToScreen(tx<<10 | 0x200, ty<<10 | 0x200, sx, sy, 0) is called (0x0042f9cc); when *sx in [-0x40,
// g_viewW+0x40) and *sy in [-0x2a, g_viewH+0x2a) (g_viewW = 0x00822c8c, g_viewH = 0x00822c90) the position is
// cached and 1 returned (0x0042fa00 / 0x0042fa0a), else the X cell is set to -99 (0xff9d, 0x0042fa19) and 0
// returned. A cached -99 returns 0 without recomputing.
typedef int(__cdecl* TileToScreen_t)(int, int, int*, int*);
TileToScreen_t TileToScreen_orig;
int __cdecl TileToScreen_re(int tx, int ty, int* sx, int* sy) {
    if (tx < 0 || tx >= 50 || ty < 0 || ty >= 50) return 0;
    const unsigned cell = tx * 50 + ty;
    const short c = at<short>(0x0055eb40 + cell * 2);
    if (c != 0) {
        if (c == -99) return 0;
        *sx = c;
        *sy = at<short>(0x0055fec8 + cell * 2);
        return 1;
    }
    kWorldToScreen((tx << 10) + 0x200, (ty << 10) + 0x200, sx, sy, 0);
    if (*sx >= -0x40 && *sx < at<int>(0x00822c8c) + 0x40 && *sy >= -0x2a && *sy < at<int>(0x00822c90) + 0x2a) {
        *reinterpret_cast<short*>(0x0055eb40 + cell * 2) = static_cast<short>(*sx);
        *reinterpret_cast<short*>(0x0055fec8 + cell * 2) = static_cast<short>(*sy);
        return 1;
    }
    *reinterpret_cast<short*>(0x0055eb40 + cell * 2) = static_cast<short>(-99);
    return 0;
}

// The DirectDraw surface held at Surf473+4 (this->m_4). Its vtable slot +0x40 is a fill/blt entry and +0xe4 is
// format() (returns a struct whose first dword is the bit depth). Only called on the non-null paths below.
struct DdVt;  // opaque: methods are reached by offset through the object's vtable pointer
typedef int(__fastcall* BlitTo_t)(void*, void*, void*, int, int, int, int, int, int);
const BlitTo_t kBlitTo = reinterpret_cast<BlitTo_t>(0x00492000);  // Surf473::blitTo

struct Rect4 { int left, top, right, bottom; };

// 0x00473bf0  Surface_blit(this, s, a2, a3, a4, a5, a6, a7): returns 0x10 when s is null (0x00473bf8); 7 when s
// has no DirectDraw surface at s+4 (0x00473c06 / 0x00473c0b); 0 when that surface's format (vtable +0xe4) reports
// a bit depth other than 8 (0x00473c17..0x00473c1f); otherwise Surf473::blitTo (0x00492000) does the 8-bit copy.
typedef int(__fastcall* SurfBlit_t)(void*, void*, void*, int, int, int, int, int, int);
SurfBlit_t SurfaceBlit_orig;
int __fastcall SurfaceBlit_re(void* ecx, void*, void* s, int a2, int a3, int a4, int a5, int a6, int a7) {
    if (!s) return 0x10;
    void* m4 = *reinterpret_cast<void**>(reinterpret_cast<unsigned>(s) + 4);  // s->m_4
    if (!m4) return 7;
    const unsigned vt = *reinterpret_cast<unsigned*>(m4);
    typedef int*(__fastcall* Format_t)(void*, void*);
    int* fmt = reinterpret_cast<Format_t>(at<unsigned>(vt + 0xe4))(m4, 0);
    if (*fmt != 8) return 0;
    return kBlitTo(ecx, 0, s, a2, a3, a4, a5, a6, a7);
}

// 0x00475c90  Surface_fillRegion(this, s, x, y, w, h): returns 0x10 when s is null (0x00475c97); 7 when this has
// no surface at this+4 or s has none at s+4 (0x00475ca8 / 0x00475caf -> 0x00475cf3); otherwise builds the rect
// {x, y, x+w, y+h} (0x00475cc3..0x00475cdb) and calls fill (this->m_4 vtable +0x40) with that rect twice
// (0x00475ce8).
typedef int(__fastcall* SurfFill_t)(void*, void*, void*, int, int, int, int);
SurfFill_t SurfaceFillRegion_orig;
int __fastcall SurfaceFillRegion_re(void* ecx, void*, void* s, int x, int y, int w, int h) {
    if (!s) return 0x10;
    void* tm4 = *reinterpret_cast<void**>(reinterpret_cast<unsigned>(ecx) + 4);  // this->m_4
    if (!tm4) return 7;
    void* sm4 = *reinterpret_cast<void**>(reinterpret_cast<unsigned>(s) + 4);    // s->m_4
    if (!sm4) return 7;
    Rect4 r = { x, y, x + w, y + h };
    const unsigned vt = *reinterpret_cast<unsigned*>(tm4);
    typedef int(__fastcall* Fill_t)(void*, void*, void*, Rect4*, Rect4*);
    return reinterpret_cast<Fill_t>(at<unsigned>(vt + 0x40))(tm4, 0, sm4, &r, &r);
}

// 0x00475d00  Surface_blit3(this, src, x, y, w, h, x2, y2, w2, h2): returns 0x10 when src is null (0x00475d07);
// 7 when this has no surface at this+4 or src has none at src+4 (0x00475d18 / 0x00475d1f -> 0x00475d87);
// otherwise builds rect1 {x, y, x+w, y+h} and rect2 {x2, y2, x2+w2, y2+h2} (0x00475d33..0x00475d6f) and calls
// blit (this->m_4 vtable +0x40) with src->m_4 and the two rects (0x00475d7c).
typedef int(__fastcall* SurfBlit3_t)(void*, void*, void*, int, int, int, int, int, int, int, int);
SurfBlit3_t SurfaceBlit3_orig;
int __fastcall SurfaceBlit3_re(void* ecx, void*, void* src, int x, int y, int w, int h,
                               int x2, int y2, int w2, int h2) {
    if (!src) return 0x10;
    void* tm4 = *reinterpret_cast<void**>(reinterpret_cast<unsigned>(ecx) + 4);   // this->m_4
    if (!tm4) return 7;
    void* sm4 = *reinterpret_cast<void**>(reinterpret_cast<unsigned>(src) + 4);   // src->m_4
    if (!sm4) return 7;
    Rect4 r1 = { x, y, x + w, y + h };
    Rect4 r2 = { x2, y2, x2 + w2, y2 + h2 };
    const unsigned vt = *reinterpret_cast<unsigned*>(tm4);
    typedef int(__fastcall* Blit_t)(void*, void*, void*, Rect4*, Rect4*);
    return reinterpret_cast<Blit_t>(at<unsigned>(vt + 0x40))(tm4, 0, sm4, &r1, &r2);
}

}  // namespace

SG_HOOK("golf_clean.exe", 0x00461810, brightenComponent, Brighten_re, Brighten_orig);
SG_HOOK("golf_clean.exe", 0x0040A130, setGrids, SetGrids_re, SetGrids_orig);
SG_HOOK("golf_clean.exe", 0x0040BF00, setLinePoint, SetLinePoint_re, SetLinePoint_orig);
SG_HOOK("golf_clean.exe", 0x004833F0, tableFind, TableFind_re, TableFind_orig);
SG_HOOK("golf_clean.exe", 0x0042F940, tileToScreen, TileToScreen_re, TileToScreen_orig);
SG_HOOK("golf_clean.exe", 0x00473BF0, surfaceBlit, SurfaceBlit_re, SurfaceBlit_orig);
SG_HOOK("golf_clean.exe", 0x00475C90, surfaceFillRegion, SurfaceFillRegion_re, SurfaceFillRegion_orig);
SG_HOOK("golf_clean.exe", 0x00475D00, surfaceBlit3, SurfaceBlit3_re, SurfaceBlit3_orig);

// NEGATIVE CONTROL for batch c3da. NEVER list this file in shim/re_batches.txt: every body here is DELIBERATELY WRONG
// in one detail, so that a diff_hook run proves the recording stubs, the rect dereference, the display scale cells and
// the swapped static tables are compared state and not decoration.
//
// Built INSTEAD of c3da.cpp through log/c3/c3da_negctl_batches.txt (driven by log/c3/alt/c3da_negctl.py, CSVs in
// log/c3/alt/c3da_negctl, never log/diff):
//   0x004740f0  Surface_blit2         x offset scaled by ny instead of nx    -> RED where nx != ny (scales s2..s4)
//   0x00474260  Sprite_drawScaled     m_20 not put back after the draw       -> RED on state ($surfs)
//   0x00475c60  Surface_blitThunk     source and destination corners swapped -> RED on the recorded rect CONTENTS
//   0x0040d0b0  drawFrameS2           style byte 0x005a34e0 ignored (style 0) -> RED on styles 2 and 3 only
//   0x00474550  Surface_create        fields set when slot +4 returns NON-zero -> RED on state
//   0x0040ca10  blitPanel             nine-patch index 3 * cx + cy (the first draft's error) -> RED on multi-tile rects
#include "hooks.h"

namespace {
static inline char* B(void* p) { return reinterpret_cast<char*>(p); }
static inline int& I(void* base, int off) { return *reinterpret_cast<int*>(B(base) + off); }
static inline void* Pp(void* base, int off) { return *reinterpret_cast<void**>(B(base) + off); }
template <typename T> static inline T vslot(void* obj, int off) {
    return *reinterpret_cast<T*>(*reinterpret_cast<char**>(obj) + off);
}
static inline int mul(int a, int b) { return static_cast<int>(static_cast<unsigned>(a) * static_cast<unsigned>(b)); }
static inline int add(int a, int b) { return static_cast<int>(static_cast<unsigned>(a) + static_cast<unsigned>(b)); }
void** const kDisplay = reinterpret_cast<void**>(0x0083ad50);
void** const kTarget = reinterpret_cast<void**>(0x004c1570);
typedef int(__fastcall* V1_t)(void*, void*, int);
typedef int(__fastcall* V2_t)(void*, void*, int, int);
typedef int(__fastcall* V3_t)(void*, void*, int, int, int);
typedef int(__fastcall* V5_t)(void*, void*, int, int, int, int, int);
typedef int(__fastcall* V6_t)(void*, void*, int, int, int, int, int, int);
typedef int(__fastcall* GetScale_t)(void*, void*, int*, int*, int*);
typedef int(__fastcall* M5_t)(void*, void*, int, int, int, int, int);
typedef int(__fastcall* M7_t)(void*, void*, int, int, int, int, int, int, int);
typedef int(__fastcall* M9_t)(void*, void*, int, int, int, int, int, int, int, int, int);
typedef void(__fastcall* M0v_t)(void*, void*);
typedef int(__cdecl* C5_t)(int, int, int, int, int);
const M5_t kStretchBlit = reinterpret_cast<M5_t>(0x00473f60);
const M5_t kBlit2 = reinterpret_cast<M5_t>(0x004740f0);
const M9_t kBlit3 = reinterpret_cast<M9_t>(0x00475d00);
const M0v_t kSurfaceFree = reinterpret_cast<M0v_t>(0x00473ae0);
const M5_t kFillWindowRect = reinterpret_cast<M5_t>(0x00480b00);
static inline int handleOrZero(void* s) { return s ? I(s, 4) : 0; }
static inline void getScale(int* nx, int* ny, int* den) {
    void* d = *kDisplay;
    vslot<GetScale_t>(d, 0xa0)(d, 0, nx, ny, den);
}
static inline void setScale(int nx, int ny, int den) {
    void* d = *kDisplay;
    vslot<V3_t>(d, 0x9c)(d, 0, nx, ny, den);
}
}  // namespace

M5_t NegBlit2_orig;
int __fastcall NegBlit2_re(void* self, void*, int dst, int x, int y, int mode, int mask) {
    if (I(self, 8) != 0) return 0x18;
    if (dst == 0) return 0x10;
    if (I(self, 4) == 0) return 7;
    if (I(reinterpret_cast<void*>(dst), 4) == 0) return 7;
    int nx, ny, den;
    getScale(&nx, &ny, &den);
    int dx = mul(I(self, 0x20), ny) / den;  // WRONG: ny
    int dy = mul(I(self, 0x24), ny) / den;
    void* h = Pp(self, 4);
    return vslot<V5_t>(h, 0x6c)(h, 0, I(reinterpret_cast<void*>(dst), 4), add(dx, x), add(dy, y), mode,
                                handleOrZero(reinterpret_cast<void*>(mask)));
}

M9_t NegSpriteDrawScaled_orig;
int __fastcall NegSpriteDrawScaled_re(void* self, void*, int dst, int x, int y, int mode, int sx, int sy, int den,
                                      int mask, int extra) {
    if (dst == 0) return 0x10;
    if (I(self, 4) == 0) return 7;
    if (I(reinterpret_cast<void*>(dst), 4) == 0) return 7;
    int nx, ny, oden;
    getScale(&nx, &ny, &oden);
    setScale(sx, sy, den);
    int hy = I(self, 0x24);
    int nhx = mul(I(self, 0x20), sx) / den;
    I(self, 0x20) = nhx;
    int nhy = mul(hy, sy) / den;
    I(self, 0x24) = nhy;
    void* h = Pp(self, 4);
    int r = vslot<V6_t>(h, 0x4c)(h, 0, I(reinterpret_cast<void*>(dst), 4), add(nhx, x), add(nhy, y), mode,
                                 handleOrZero(reinterpret_cast<void*>(mask)), extra);
    I(self, 0x24) = hy;  // WRONG: m_20 is left scaled
    setScale(nx, ny, oden);
    return r;
}

M7_t NegBlitThunk_orig;
int __fastcall NegBlitThunk_re(void* self, void*, int dst, int dx, int dy, int sx, int sy, int w, int h) {
    return kBlit3(self, 0, dst, sx, sy, w, h, dx, dy, w, h);  // WRONG: corners swapped
}

typedef int(__cdecl* C5x_t)(int, int, int, int, int);
static const int kFS2Surf[8] = {0x00562394, 0x00562418, 0x00562368, 0x005623c0, 0x00562470, 0x00562444, 0x0056249c,
                                0x005623ec};
static const int kFS2Mask[8] = {0x0059d94c, 0x0059d9d0, 0x0059d920, 0x0059d978, 0x0059da28, 0x0059d9fc, 0x0059da54,
                                0x0059d9a4};
static inline void round16(int& pos, int& size) {
    int low = size & 0xf;
    if (low != 0) {
        int r = 0xf - low;
        size += r;
        pos -= r / 2;
    }
}
static void sb(int s, int m, int x, int y) {
    kStretchBlit(reinterpret_cast<void*>(s), 0, m, reinterpret_cast<int>(*kTarget), x, y, 0);
}
int __cdecl NegDrawFrameS2_re(int x, int y, int w, int h, int fill) {
    void* d = *kDisplay;
    if (d != 0) vslot<V3_t>(d, 0x9c)(d, 0, 1, 1, 1);
    round16(x, w);
    round16(y, h);
    if (fill != 0) kFillWindowRect(reinterpret_cast<void*>(0x00519a60), 0, x + 2, y + 2, w - 4, h - 4,
                                   static_cast<int>(0x80007fdc));
    const int* s = kFS2Surf;  // WRONG: style 0 always
    const int* m = kFS2Mask;
    sb(s[0], m[0], x, y);
    if (w - 0x10 > 0x10) {
        unsigned n = static_cast<unsigned>(w - 0x11) >> 4;
        int xi = x + 0x10;
        do { sb(s[1], m[1], xi, y); sb(s[2], m[2], xi, y + h - 0x10); xi += 0x10; } while (--n != 0);
    }
    if (h - 0x10 > 0x10) {
        unsigned n = static_cast<unsigned>(h - 0x11) >> 4;
        int yi = y + 0x10;
        do { sb(s[3], m[3], x, yi); sb(s[4], m[4], x + w - 0x10, yi); yi += 0x10; } while (--n != 0);
    }
    sb(s[5], m[5], x + w - 0x10, y);
    sb(s[6], m[6], x + w - 0x10, y + h - 0x10);
    sb(s[7], m[7], x, y + h - 0x10);
    return 0;
}
C5x_t NegDrawFrameS2_orig;

V2_t NegSurfaceCreate_orig;
int __fastcall NegSurfaceCreate_re(void* self, void*, int w, int hgt) {
    kSurfaceFree(self, 0);
    void* d = *kDisplay;
    void* h = reinterpret_cast<void*>(vslot<V2_t>(d, 0x80)(d, 0, reinterpret_cast<int>(B(self) + 4), 1));
    I(self, 4) = reinterpret_cast<int>(h);
    vslot<V1_t>(h, 0x18)(h, 0, 0xff);
    void* h2 = Pp(self, 4);
    int r = vslot<V6_t>(h2, 4)(h2, 0, 0, w, hgt, 8, 1, *reinterpret_cast<int*>(0x0083acb4));
    if (r != 0) {  // WRONG: inverted
        I(self, 0x10) = w; I(self, 0x14) = hgt; I(self, 0x18) = w; I(self, 0x1c) = hgt;
        I(self, 0x20) = 0; I(self, 0x24) = 0;
    }
    return r;
}

int __cdecl NegBlitPanel_re(int x, int y, int w, int h, int style) {
    int xEnd = add(x, w);
    if (x >= xEnd) return xEnd;
    int yEnd = add(y, h);
    for (int xi = x; xi < xEnd; xi += 16) {
        for (int yi = y; yi < yEnd; yi += 16) {
            void* surf;
            if (style == 0) {
                surf = reinterpret_cast<void*>(0x005a5554);
            } else {
                int cx = (xi == x) ? 0 : 1;
                if (add(xi, 16) >= xEnd) cx = 2;
                int cy = (yi == y) ? 0 : 1;
                if (add(yi, 16) >= yEnd) cy = 2;
                surf = reinterpret_cast<void*>(0x00541580 + 0x2c * (3 * cx + cy));  // WRONG index
            }
            kBlit2(surf, 0, reinterpret_cast<int>(*kTarget), xi, yi, *reinterpret_cast<int*>(0x00824148), 0);
        }
    }
    return xEnd;
}
C5x_t NegBlitPanel_orig;

SG_HOOK("golf_clean.exe", 0x004740f0, Surface_blit2, NegBlit2_re, NegBlit2_orig);
SG_HOOK("golf_clean.exe", 0x00474260, Sprite_drawScaled, NegSpriteDrawScaled_re, NegSpriteDrawScaled_orig);
SG_HOOK("golf_clean.exe", 0x00475c60, Surface_blitThunk, NegBlitThunk_re, NegBlitThunk_orig);
SG_HOOK("golf_clean.exe", 0x0040d0b0, drawFrameS2, NegDrawFrameS2_re, NegDrawFrameS2_orig);
SG_HOOK("golf_clean.exe", 0x00474550, Surface_create, NegSurfaceCreate_re, NegSurfaceCreate_orig);
SG_HOOK("golf_clean.exe", 0x0040ca10, blitPanel, NegBlitPanel_re, NegBlitPanel_orig);

// C3 batch c3da (round 24, 2026-10-10) of golf_clean.exe: the Surface wrapper layer under 0x004781f0 that earlier
// batches dropped because it draws (log/c3/c3da_candidates.tsv).
//
// The game's Surface record is 0x2c bytes (the static arrays use stride 0x2c, e.g. initGlobalArray_00404e00 builds
// 0x20 of them at 0x00562368). Fields these functions use: +4 a handle object they call through its own vtable;
// +8 a word Surface_blit2 / Surface_drawSprite test (non-zero -> return 0x18) and Surface_free hands to _free;
// +0xc a pointer Surface_free destroys and Surface_blitTakeDirty fills from its argument's +0xac; +0x10/+0x14 and
// +0x18/+0x1c where Surface_create / Surface_init2 store (w, h); +0x20 / +0x24 offsets added to the x / y position by
// the draw wrappers; +0x28 the second argument Surface_copyFrom passes to the display's slot +0x80.
// The display is g_graphicsDevice 0x0083ad50; its vtable read in the running game (2026-10-10) is jgld.dll+0x11d640,
// jgld's Display vtable 0x1011d640, whose slot +0x9c is Display::setScale 0x10067d50 and +0xa0 Display::getScale
// 0x10067da0 (re/names/jgld_3.tsv: they store / read back the three ints g_blitScaleX/Y/Den). Surface_blit2 reads the
// three outs as (x factor, y factor, divisor): m_20 * out1 / out3 and m_24 * out2 / out3 (0x0047414c..0x00474166).
// *0x004c1570 read in the running game at the main menu is 0x00519cd4 = 0x00519a60 + 0x274, the Surface sub-object of
// the window object fillWindowRect is called on.
//
// Parameter names below (dst, mask, extra, mode, fill, colour, style) are LABELS for argument positions, chosen from
// how the callers in this file fill them; what jgld's handle slots do with them is not determined here.
//
//   leaves:      Surface_stretchBlit 0x00473f60, Surface_blitMasked 0x00473fc0, Surface_blit2 0x004740f0,
//                Sprite_draw 0x004741b0, Sprite_drawScaled 0x00474260, Surface_drawSprite 0x00474440
//   wrappers:    Surface_drawScaled2 0x00474030 (-> Surface_blit2), Surface_draw 0x00474380 (-> Surface_drawSprite),
//                drawScaledSprite421b90 0x00421b90 (-> Surface_drawScaled2), Surface_blitThunk 0x00475c60
//                (-> Surface_blit3 0x00475d00, C3 c3h), Surface_blitTakeDirty 0x00473c60 (-> Surface_blit 0x00473bf0)
//   lifetime:    Surface_copyFrom 0x004744c0, Surface_create 0x00474550, Surface_init2 0x004745c0
//                (-> Surface_free 0x00473ae0, blitErrorPtr 0x00492450)
//   tilers:      blitPanel 0x0040ca10 (-> Surface_blit2), drawPanelS2 0x0040cc00, drawFrameR3 0x0040cef0,
//                drawFrameS2 0x0040d0b0 (-> Surface_stretchBlit, fillWindowRect 0x00480b00, blitPanel)
//   lines:       drawLineOnSurface 0x004049a0 (-> Widget_fillRect 0x00478b80, C3 c3ae), drawRectScaled 0x0046e710
//                (-> scaleX 0x00404970, C3 golf_math; drawLineOnSurface)
//
// Every body is hand-written from the disassembly (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr> --list)
// and cites the instructions it reproduces. __thiscall is emulated with __fastcall (ecx = this, edx unused). Callees
// and virtual slots are called through their original addresses / the object's own vtable. Products and sums are
// computed in unsigned arithmetic and cast back (imul / add wrap; idiv truncates toward zero like C's /).
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

void** const kDisplay = reinterpret_cast<void**>(0x0083ad50);      // the display object pointer
void** const kTarget = reinterpret_cast<void**>(0x004c1570);       // a Surface* (0x00519cd4 at the menu)
int* const kBlitMode = reinterpret_cast<int*>(0x00824148);         // g_color16Lut, blitPanel's 4th blit2 arg
signed char* const kFrameStyle = reinterpret_cast<signed char*>(0x005a34e0);  // drawFrameS2's style index
char* const kMainWindow = reinterpret_cast<char*>(0x00519a60);     // fillWindowRect's window (this)

// Virtual slot types (ret = 4 * stack arguments).
typedef int(__fastcall* V0_t)(void* ecx, void* edx);
typedef int(__fastcall* V1_t)(void* ecx, void* edx, int a);
typedef int(__fastcall* V2_t)(void* ecx, void* edx, int a, int b);
typedef int(__fastcall* V3_t)(void* ecx, void* edx, int a, int b, int c);
typedef int(__fastcall* V5_t)(void* ecx, void* edx, int a, int b, int c, int d, int e);
typedef int(__fastcall* V6_t)(void* ecx, void* edx, int a, int b, int c, int d, int e, int f);
typedef int(__fastcall* GetScale_t)(void* ecx, void* edx, int* nx, int* ny, int* den);

// Original callees.
typedef int(__fastcall* M5_t)(void* ecx, void* edx, int a, int b, int c, int d, int e);
typedef int(__fastcall* M6_t)(void* ecx, void* edx, int a, int b, int c, int d, int e, int f);
typedef int(__fastcall* M7_t)(void* ecx, void* edx, int a, int b, int c, int d, int e, int f, int g);
typedef int(__fastcall* M8_t)(void* ecx, void* edx, int a, int b, int c, int d, int e, int f, int g, int h);
typedef int(__fastcall* M9_t)(void* ecx, void* edx, int a, int b, int c, int d, int e, int f, int g, int h, int i);
typedef void(__fastcall* M0v_t)(void* ecx, void* edx);
typedef int(__cdecl* C0_t)();
typedef int(__cdecl* C1_t)(int a);
typedef int(__cdecl* C5_t)(int a, int b, int c, int d, int e);
typedef int(__cdecl* C6_t)(int a, int b, int c, int d, int e, int f);

const M5_t kStretchBlit = reinterpret_cast<M5_t>(0x00473f60);     // Surface_stretchBlit, ret 0x14
const M5_t kBlit2 = reinterpret_cast<M5_t>(0x004740f0);           // Surface_blit2, ret 0x14
const M6_t kDrawSprite = reinterpret_cast<M6_t>(0x00474440);      // Surface_drawSprite, ret 0x18
const M8_t kDrawScaled2 = reinterpret_cast<M8_t>(0x00474030);     // Surface_drawScaled2, ret 0x20
const M9_t kBlit3 = reinterpret_cast<M9_t>(0x00475d00);           // Surface_blit3, ret 0x24
const M7_t kSurfaceBlit = reinterpret_cast<M7_t>(0x00473bf0);     // Surface_blit, ret 0x1c
const M0v_t kSurfaceFree = reinterpret_cast<M0v_t>(0x00473ae0);   // Surface_free, ret
const C0_t kBlitErrorPtr = reinterpret_cast<C0_t>(0x00492450);    // blitErrorPtr: returns 0x0083c310
const M5_t kFillWindowRect = reinterpret_cast<M5_t>(0x00480b00);  // fillWindowRect, ret 0x14
const C5_t kBlitPanel = reinterpret_cast<C5_t>(0x0040ca10);       // blitPanel, cdecl
const M5_t kWidgetFillRect = reinterpret_cast<M5_t>(0x00478b80);  // Widget_fillRect, ret 0x14
const C6_t kDrawLineOnSurface = reinterpret_cast<C6_t>(0x004049a0);  // drawLineOnSurface, cdecl
const C1_t kScaleX = reinterpret_cast<C1_t>(0x00404970);          // scaleX, cdecl

// m->m_4 of a Surface argument that may be null: `test eax, eax; je; mov eax, [eax+4]` else 0.
static inline int handleOrZero(void* s) { return s ? I(s, 4) : 0; }

// display->getScale(&nx, &ny, &den) through slot +0xa0 and display->setScale(nx, ny, den) through slot +0x9c.
static inline void getScale(int* nx, int* ny, int* den) {
    void* d = *kDisplay;
    vslot<GetScale_t>(d, 0xa0)(d, 0, nx, ny, den);
}
static inline void setScale(int nx, int ny, int den) {
    void* d = *kDisplay;
    vslot<V3_t>(d, 0x9c)(d, 0, nx, ny, den);
}

}  // namespace

// ================================================================================================================
// Leaves
// ================================================================================================================

// 0x00473f60  Surface_stretchBlit(this, mask, dst, x, y, extra): ret 0x14.
// a2 (dst) null or a1 (mask) null -> 0x10 (0x00473f67 / 0x00473f6f, read in that order); this->m_4, dst->m_4 or
// mask->m_4 null -> 7 (0x00473f76 / 0x00473f7d / 0x00473f84); otherwise this->m_4's slot +0x54 (0x00473fa5) with
// (mask->m_4, dst->m_4, x, y, extra ? extra->m_4 : 0) (0x00473f86..0x00473fa4); its eax is returned.
M5_t SurfaceStretchBlit_orig;
int __fastcall SurfaceStretchBlit_re(void* self, void*, int mask, int dst, int x, int y, int extra) {
    if (dst == 0) return 0x10;
    if (mask == 0) return 0x10;
    void* h = Pp(self, 4);
    if (h == 0) return 7;
    int dh = I(reinterpret_cast<void*>(dst), 4);
    if (dh == 0) return 7;
    int mh = I(reinterpret_cast<void*>(mask), 4);
    if (mh == 0) return 7;
    int eh = handleOrZero(reinterpret_cast<void*>(extra));
    return vslot<V5_t>(h, 0x54)(h, 0, mh, dh, x, y, eh);
}

// 0x00473fc0  Surface_blitMasked(this, a1, a2, a3, a4, a5, a6): ret 0x18.
// a2 null or a1 null -> 0x10 (0x00473fc8 / 0x00473fd0); this->m_4, a2->m_4, a1->m_4 or a3->m_4 null -> 7
// (0x00473fd7 / 0x00473fde / 0x00473fe5 / 0x00473ff0; a3 itself is dereferenced without a test, 0x00473feb);
// otherwise this->m_4's slot +0x50 (0x00474012) with (a1->m_4, a2->m_4, a3->m_4, a4, a5, a6 ? a6->m_4 : 0).
V6_t SurfaceBlitMasked_orig;
int __fastcall SurfaceBlitMasked_re(void* self, void*, int a1, int a2, int a3, int a4, int a5, int a6) {
    if (a2 == 0) return 0x10;
    if (a1 == 0) return 0x10;
    void* h = Pp(self, 4);
    if (h == 0) return 7;
    int h2 = I(reinterpret_cast<void*>(a2), 4);
    if (h2 == 0) return 7;
    int h1 = I(reinterpret_cast<void*>(a1), 4);
    if (h1 == 0) return 7;
    int h3 = I(reinterpret_cast<void*>(a3), 4);
    if (h3 == 0) return 7;
    int h6 = handleOrZero(reinterpret_cast<void*>(a6));
    return vslot<V6_t>(h, 0x50)(h, 0, h1, h2, h3, a4, a5, h6);
}

// Shared body of Surface_blit2 and Sprite_draw after their guards: read the display scale (slot +0xa0, the first
// out-pointer is the caller's own a1 stack slot, 0x00474141 / 0x004741ed), offset the position by the scaled +0x20 / +0x24 fields
// (m_20 * nx / den + x, m_24 * ny / den + y; 0x0047414c..0x00474190 / 0x004741f8..0x0047423c) and call the handle's
// slot with (dst->m_4, x', y', mode, mask ? mask->m_4 : 0).
static int scaledHandleCall(void* self, void* dst, int x, int y, int mode, int mask, int slot) {
    int nx, ny, den;
    getScale(&nx, &ny, &den);
    int dx = mul(I(self, 0x20), nx) / den;
    int dy = mul(I(self, 0x24), ny) / den;
    int mh = handleOrZero(reinterpret_cast<void*>(mask));
    void* h = Pp(self, 4);
    return vslot<V5_t>(h, slot)(h, 0, I(dst, 4), add(dx, x), add(dy, y), mode, mh);
}

// 0x004740f0  Surface_blit2(this, dst, x, y, mode, mask): ret 0x14.
// this->m_8 != 0 -> 0x18 (0x004740fc); dst null -> 0x10 (0x00474111); this->m_4 or dst->m_4 null -> 7 (0x00474125 /
// 0x0047412c); otherwise the scaled call through this->m_4's slot +0x6c (0x00474194).
M5_t SurfaceBlit2_orig;
int __fastcall SurfaceBlit2_re(void* self, void*, int dst, int x, int y, int mode, int mask) {
    if (I(self, 8) != 0) return 0x18;
    if (dst == 0) return 0x10;
    if (I(self, 4) == 0) return 7;
    if (I(reinterpret_cast<void*>(dst), 4) == 0) return 7;
    return scaledHandleCall(self, reinterpret_cast<void*>(dst), x, y, mode, mask, 0x6c);
}

// 0x004741b0  Sprite_draw(this, dst, x, y, mode, mask): ret 0x14. Surface_blit2 without the m_8 test: dst null ->
// 0x10 (0x004741bd); this->m_4 or dst->m_4 null -> 7 (0x004741d1 / 0x004741d8); otherwise the scaled call through
// this->m_4's slot +0x78 (0x00474240).
M5_t SpriteDraw_orig;
int __fastcall SpriteDraw_re(void* self, void*, int dst, int x, int y, int mode, int mask) {
    if (dst == 0) return 0x10;
    if (I(self, 4) == 0) return 7;
    if (I(reinterpret_cast<void*>(dst), 4) == 0) return 7;
    return scaledHandleCall(self, reinterpret_cast<void*>(dst), x, y, mode, mask, 0x78);
}

// 0x00474260  Sprite_drawScaled(this, dst, x, y, mode, sx, sy, den, mask, extra): ret 0x24.
// dst null -> 0x10 (0x0047426c); this->m_4 or dst->m_4 null -> 7 (0x0047427f / 0x0047428a). Otherwise: save the
// display scale (slot +0xa0, 0x004742aa), set (sx, sy, den) (slot +0x9c, 0x004742c7), scale m_20 / m_24 by the NEW
// scale and store it in m_20 / m_24 (m_20 * sx / den, m_24 * sy / den; 0x004742cd..0x004742f4), call this->m_4's slot
// +0x4c (0x00474338) with (dst->m_4, x + m_20', y + m_24', mode, mask ? mask->m_4 : 0, extra), put m_20 / m_24 back
// (0x00474346 / 0x00474349), restore the saved scale (slot +0x9c, 0x0047435a) and return the slot +0x4c result.
M9_t SpriteDrawScaled_orig;
int __fastcall SpriteDrawScaled_re(void* self, void*, int dst, int x, int y, int mode, int sx, int sy, int den,
                                   int mask, int extra) {
    if (dst == 0) return 0x10;
    if (I(self, 4) == 0) return 7;
    if (I(reinterpret_cast<void*>(dst), 4) == 0) return 7;
    int nx, ny, oden;
    getScale(&nx, &ny, &oden);
    setScale(sx, sy, den);
    int hx = I(self, 0x20), hy = I(self, 0x24);
    int nhx = mul(hx, sx) / den;
    I(self, 0x20) = nhx;
    int nhy = mul(hy, sy) / den;
    I(self, 0x24) = nhy;
    int mh = handleOrZero(reinterpret_cast<void*>(mask));
    void* h = Pp(self, 4);
    int r = vslot<V6_t>(h, 0x4c)(h, 0, I(reinterpret_cast<void*>(dst), 4), add(nhx, x), add(nhy, y), mode, mh, extra);
    I(self, 0x20) = hx;
    I(self, 0x24) = hy;
    setScale(nx, ny, oden);
    return r;
}

// 0x00474440  Surface_drawSprite(this, dst, x, y, mode, mask, extra): ret 0x18. No display scale.
// this->m_8 != 0 -> 0x18 (0x00474446); dst null -> 0x10 (0x00474457); this->m_4 or dst->m_4 null -> 7 (0x00474467 /
// 0x0047446e); otherwise this->m_4's slot +0x94 (0x004744a5) with (dst->m_4, m_20 + x, m_24 + y, mode,
// mask ? mask->m_4 : 0, extra) (0x00474470..0x004744a2).
M6_t SurfaceDrawSprite_orig;
int __fastcall SurfaceDrawSprite_re(void* self, void*, int dst, int x, int y, int mode, int mask, int extra) {
    if (I(self, 8) != 0) return 0x18;
    if (dst == 0) return 0x10;
    void* h = Pp(self, 4);
    if (h == 0) return 7;
    int dh = I(reinterpret_cast<void*>(dst), 4);
    if (dh == 0) return 7;
    int mh = handleOrZero(reinterpret_cast<void*>(mask));
    return vslot<V6_t>(h, 0x94)(h, 0, dh, add(I(self, 0x20), x), add(I(self, 0x24), y), mode, mh, extra);
}

// ================================================================================================================
// Scale wrappers
// ================================================================================================================

// 0x00474030  Surface_drawScaled2(this, dst, x, y, mode, sx, sy, den, mask): ret 0x20. No guards of its own.
// Saves the display scale (0x00474050), sets (sx, sy, den) (0x0047406d), stores m_20 * sx / den and m_24 * sy / den
// in m_20 / m_24 (0x00474073..0x00474098), calls Surface_blit2(dst, x, y, mode, mask) on this (0x004740ae), puts
// m_20 / m_24 back (0x004740be / 0x004740c1), restores the scale (0x004740d2) and returns Surface_blit2's eax.
M8_t SurfaceDrawScaled2_orig;
int __fastcall SurfaceDrawScaled2_re(void* self, void*, int dst, int x, int y, int mode, int sx, int sy, int den,
                                     int mask) {
    int nx, ny, oden;
    getScale(&nx, &ny, &oden);
    setScale(sx, sy, den);
    int hx = I(self, 0x20), hy = I(self, 0x24);
    I(self, 0x20) = mul(hx, sx) / den;
    I(self, 0x24) = mul(hy, sy) / den;
    int r = kBlit2(self, 0, dst, x, y, mode, mask);
    I(self, 0x20) = hx;
    I(self, 0x24) = hy;
    setScale(nx, ny, oden);
    return r;
}

// 0x00474380  Surface_draw(this, dst, x, y, mode, sx, sy, den, mask, extra): ret 0x24. Surface_drawScaled2's frame
// around Surface_drawSprite: m_20 / m_24 are read first (0x00474398 / 0x0047439b), the scale saved (0x004743a6) and
// set (0x004743c3), m_20 = sx * m_20 / den, m_24 = m_24 * sy / den (0x004743c9..0x004743ea), Surface_drawSprite(dst,
// x, y, mode, mask, extra) (0x00474405), m_20 / m_24 put back (0x0047440e / 0x00474416), the scale restored
// (0x00474429); returns Surface_drawSprite's eax.
M9_t SurfaceDraw_orig;
int __fastcall SurfaceDraw_re(void* self, void*, int dst, int x, int y, int mode, int sx, int sy, int den, int mask,
                              int extra) {
    int hx = I(self, 0x20), hy = I(self, 0x24);
    int nx, ny, oden;
    getScale(&nx, &ny, &oden);
    setScale(sx, sy, den);
    I(self, 0x20) = mul(sx, I(self, 0x20)) / den;
    I(self, 0x24) = mul(I(self, 0x24), sy) / den;
    int r = kDrawSprite(self, 0, dst, x, y, mode, mask, extra);
    I(self, 0x20) = hx;
    I(self, 0x24) = hy;
    setScale(nx, ny, oden);
    return r;
}

// 0x00421b90  drawScaledSprite421b90(dst, x, y, mode, scale, den, mask): ret 0x1c, ecx passed through untouched to
// Surface_drawScaled2 (0x00421bb4), so it is a method of the Surface in ecx. The pushes 0x00421b98..0x00421bb3 give
// Surface_drawScaled2(dst, x, y, mode, scale, scale, den, mask): one scale for both axes.
M7_t DrawScaledSprite421b90_orig;
int __fastcall DrawScaledSprite421b90_re(void* self, void*, int dst, int x, int y, int mode, int scale, int den,
                                         int mask) {
    return kDrawScaled2(self, 0, dst, x, y, mode, scale, scale, den, mask);
}

// 0x00475c60  Surface_blitThunk(dst, dx, dy, sx, sy, w, h): ret 0x1c, ecx passed through to Surface_blit3 0x00475d00
// (0x00475c86). The pushes 0x00475c6d..0x00475c85 give Surface_blit3(dst, dx, dy, w, h, sx, sy, w, h), i.e. the
// destination rect (dx, dy, dx + w, dy + h) and the source rect (sx, sy, sx + w, sy + h) (Surface_blit3 builds both,
// 0x00475d26..0x00475d6f).
M7_t SurfaceBlitThunk_orig;
int __fastcall SurfaceBlitThunk_re(void* self, void*, int dst, int dx, int dy, int sx, int sy, int w, int h) {
    return kBlit3(self, 0, dst, dx, dy, w, h, sx, sy, w, h);
}

// 0x00473c60  Surface_blitTakeDirty(this, src, a2..a7): ret 0x1c. Surface_blit(src, a2..a7) on this with the same
// arguments in the same order (0x00473c74..0x00473c89), then this->m_c = src->m_ac (0x00473c8e / 0x00473c94) and
// src->m_ac = 0 (0x00473c97); returns Surface_blit's eax (no instruction after the call writes eax).
M7_t SurfaceBlitTakeDirty_orig;
int __fastcall SurfaceBlitTakeDirty_re(void* self, void*, int src, int a2, int a3, int a4, int a5, int a6, int a7) {
    int r = kSurfaceBlit(self, 0, src, a2, a3, a4, a5, a6, a7);
    I(self, 0xc) = I(reinterpret_cast<void*>(src), 0xac);
    I(reinterpret_cast<void*>(src), 0xac) = 0;
    return r;
}

// ================================================================================================================
// Lifetime: copy / create
// ================================================================================================================

// 0x004744c0  Surface_copyFrom(this, src): ret 4, returns this.
// src->m_4 null -> return this untouched (0x004744cd). Otherwise Surface_free(this) (0x004744d8); copy src's +8 and
// +0x10..+0x28 into this (0x004744dd..0x0047450c); a new handle from the display's slot +0x80 with (&this->m_4 or 0
// when this is null, src->m_28) (0x0047450a..0x00474523) stored in this->m_4 (0x00474529); the new handle's slot
// +0x18 with 0xff (0x00474535) and then its slot +0xc with src->m_4 (0x00474541).
V1_t SurfaceCopyFrom_orig;
int __fastcall SurfaceCopyFrom_re(void* self, void*, int src) {
    void* s = reinterpret_cast<void*>(src);
    if (I(s, 4) == 0) return reinterpret_cast<int>(self);
    kSurfaceFree(self, 0);
    I(self, 8) = I(s, 8);
    for (int off = 0x10; off <= 0x24; off += 4) I(self, off) = I(s, off);
    int p28 = I(s, 0x28);
    I(self, 0x28) = p28;
    void* d = *kDisplay;
    int slot = self ? reinterpret_cast<int>(B(self) + 4) : 0;
    void* h = reinterpret_cast<void*>(vslot<V2_t>(d, 0x80)(d, 0, slot, p28));
    I(self, 4) = reinterpret_cast<int>(h);
    vslot<V1_t>(h, 0x18)(h, 0, 0xff);
    void* h2 = Pp(self, 4);
    vslot<V1_t>(h2, 0xc)(h2, 0, I(s, 4));
    return reinterpret_cast<int>(self);
}

// 0x00474550  Surface_create(this, w, h): ret 8. Surface_free(this) (0x00474555); a new handle from the display's
// slot +0x80 with (&this->m_4 or 0, 1) (0x0047455a..0x00474570) into this->m_4 (0x00474576); its slot +0x18 with 0xff
// (0x00474582); its slot +4 with (0, w, h, 8, 1, *(int*)0x0083acb4) (0x00474585..0x004745a1). When that returns 0
// (0x004745a6): m_10 = m_18 = w, m_14 = m_1c = h, m_20 = m_24 = 0 (0x004745a8..0x004745b7). Returns slot +4's eax.
V2_t SurfaceCreate_orig;
int __fastcall SurfaceCreate_re(void* self, void*, int w, int hgt) {
    kSurfaceFree(self, 0);
    void* d = *kDisplay;
    int slot = self ? reinterpret_cast<int>(B(self) + 4) : 0;
    void* h = reinterpret_cast<void*>(vslot<V2_t>(d, 0x80)(d, 0, slot, 1));
    I(self, 4) = reinterpret_cast<int>(h);
    vslot<V1_t>(h, 0x18)(h, 0, 0xff);
    int pal = *reinterpret_cast<int*>(0x0083acb4);
    void* h2 = Pp(self, 4);
    int r = vslot<V6_t>(h2, 4)(h2, 0, 0, w, hgt, 8, 1, pal);
    if (r == 0) {
        I(self, 0x10) = w;
        I(self, 0x14) = hgt;
        I(self, 0x18) = w;
        I(self, 0x1c) = hgt;
        I(self, 0x20) = 0;
        I(self, 0x24) = 0;
    }
    return r;
}

// 0x004745c0  Surface_init2(this, w, h): ret 8, always returns 0 (0x0047463f). Surface_create's sequence with a
// different slot +4 argument list: (0, w, h, *blitErrorPtr(), 1, *(int*)0x0083acb4) (0x004745f5..0x0047461c;
// blitErrorPtr 0x00492450 returns 0x0083c310 and the dword there is passed), and the fields are set when slot +4
// returns NON-zero (`je` 0x00474621).
V2_t SurfaceInit2_orig;
int __fastcall SurfaceInit2_re(void* self, void*, int w, int hgt) {
    kSurfaceFree(self, 0);
    void* d = *kDisplay;
    int slot = self ? reinterpret_cast<int>(B(self) + 4) : 0;
    void* h = reinterpret_cast<void*>(vslot<V2_t>(d, 0x80)(d, 0, slot, 1));
    I(self, 4) = reinterpret_cast<int>(h);
    vslot<V1_t>(h, 0x18)(h, 0, 0xff);
    int pal = *reinterpret_cast<int*>(0x0083acb4);
    int err = *reinterpret_cast<int*>(kBlitErrorPtr());
    void* h2 = Pp(self, 4);
    if (vslot<V6_t>(h2, 4)(h2, 0, 0, w, hgt, err, 1, pal) != 0) {
        I(self, 0x10) = w;
        I(self, 0x14) = hgt;
        I(self, 0x18) = w;
        I(self, 0x1c) = hgt;
        I(self, 0x20) = 0;
        I(self, 0x24) = 0;
    }
    return 0;
}

// ================================================================================================================
// Tilers
// ================================================================================================================

// 0x0040ca10  blitPanel(x, y, w, h, style): cdecl, no result. Tiles the rect (x, y, w, h) with 16x16 blits onto the
// target Surface *0x004c1570: columns xi = x, x+16, .. while xi < x + w (0x0040ca1b / 0x0040ca25, step 0x0040cae1),
// rows yi = y, .. while yi < y + h (0x0040ca35 / 0x0040ca44, step 0x0040cad2). Each tile is Surface_blit2(target, xi,
// yi, *0x00824148, 0) on one Surface: style 0 (0x0040ca50) always 0x005a5554 (0x0040ca63); otherwise one of the nine
// at 0x00541580 + 0x2c * (cx + 3 * cy) (`lea ecx, [ecx + eax*2]` 0x0040caa9 then `add eax, ecx` 0x0040cab4
// give cx + 3 * cy; scaled by 0x2c at 0x0040cab8..0x0040cabe) with cx = 0 in the first column, 2 when xi + 16 >=
// x + w, else 1 (0x0040ca6f..0x0040ca81), and cy the same for rows (0x0040ca86..0x0040ca9c); the last-tile test wins
// over the first-tile one. eax at both exits holds x + w (0x0040ca1b, reloaded at 0x0040cad7), which is returned.
int __cdecl BlitPanel_re(int x, int y, int w, int h, int style) {
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
                surf = reinterpret_cast<void*>(0x00541580 + 0x2c * (cx + 3 * cy));
            }
            kBlit2(surf, 0, reinterpret_cast<int>(*kTarget), xi, yi, *kBlitMode, 0);
        }
    }
    return xEnd;
}
C5_t BlitPanel_orig;

// The 16-pixel rounding drawPanelS2 / drawFrameR3 / drawFrameS2 share (0x0040cc1f..0x0040cc7c and twins): a size not
// a multiple of 16 grows by r = 15 - (size & 15) and its position moves back by r / 2 (`cdq; sub eax, edx; sar eax, 1;
// neg eax`, r is 1..15 so the division is plain).
static inline void round16(int& pos, int& size) {
    int low = size & 0xf;
    if (low != 0) {
        int r = 0xf - low;
        size += r;
        pos -= r / 2;
    }
}

// The eight-piece border the three frame drawers share. Surfaces s[0..7] / masks m[0..7] are, in call order:
// 0 top-left, 1 top edge, 2 bottom edge, 3 left edge, 4 right edge, 5 top-right, 6 bottom-right, 7 bottom-left.
// Edge loops run (len - 16 - 1) >> 4 times (unsigned shr) when len - 16 > 16, from x + 16 / y + 16 in steps of 16.
static void frameBorder(int x, int y, int w, int h, const int* s, const int* m) {
    void* t = *kTarget;
    int tg = reinterpret_cast<int>(t);
    kStretchBlit(reinterpret_cast<void*>(s[0]), 0, m[0], tg, x, y, 0);
    if (w - 0x10 > 0x10) {
        unsigned n = static_cast<unsigned>(w - 0x10 - 1) >> 4;
        int xi = x + 0x10;
        do {
            kStretchBlit(reinterpret_cast<void*>(s[1]), 0, m[1], reinterpret_cast<int>(*kTarget), xi, y, 0);
            kStretchBlit(reinterpret_cast<void*>(s[2]), 0, m[2], reinterpret_cast<int>(*kTarget), xi, y + h - 0x10, 0);
            xi += 0x10;
        } while (--n != 0);
    }
    if (h - 0x10 > 0x10) {
        unsigned n = static_cast<unsigned>(h - 0x10 - 1) >> 4;
        int yi = y + 0x10;
        do {
            kStretchBlit(reinterpret_cast<void*>(s[3]), 0, m[3], reinterpret_cast<int>(*kTarget), x, yi, 0);
            kStretchBlit(reinterpret_cast<void*>(s[4]), 0, m[4], reinterpret_cast<int>(*kTarget), x + w - 0x10, yi, 0);
            yi += 0x10;
        } while (--n != 0);
    }
    int xr = x + w - 0x10, yb = y + h - 0x10;
    kStretchBlit(reinterpret_cast<void*>(s[5]), 0, m[5], reinterpret_cast<int>(*kTarget), xr, y, 0);
    kStretchBlit(reinterpret_cast<void*>(s[6]), 0, m[6], reinterpret_cast<int>(*kTarget), xr, yb, 0);
    kStretchBlit(reinterpret_cast<void*>(s[7]), 0, m[7], reinterpret_cast<int>(*kTarget), x, yb, 0);
}

static inline void resetScale111() {
    void* d = *kDisplay;
    if (d != 0) vslot<V3_t>(d, 0x9c)(d, 0, 1, 1, 1);
}

// 0x0040cc00  drawPanelS2(x, y, w, h): cdecl. Display scale (1, 1, 1) when 0x0083ad50 is set (0x0040cc06 ..
// 0x0040cc12); 16-rounding of w / x and h / y (0x0040cc22..0x0040cc7c); fillWindowRect on the window 0x00519a60 with
// (x + 4, y + 4, w - 8, h - 8, 0x80004e79) (0x0040cc80..0x0040cc9a); then the eight border pieces from the Surfaces
// 0x00561810.. with masks 0x0058d1e0.. (0x0040cc9f..0x0040cdbd).
static const int kS2Surf[8] = {0x00561810, 0x0056183c, 0x00561944, 0x00561894, 0x005618ec, 0x00561868, 0x00561970,
                               0x00561918};
static const int kS2Mask[8] = {0x0058d1e0, 0x0058d20c, 0x0058d314, 0x0058d264, 0x0058d2bc, 0x0058d238, 0x0058d340,
                               0x0058d2e8};
int __cdecl DrawPanelS2_re(int x, int y, int w, int h) {
    resetScale111();
    round16(x, w);
    round16(y, h);
    kFillWindowRect(kMainWindow, 0, x + 4, y + 4, w - 8, h - 8, static_cast<int>(0x80004e79));
    frameBorder(x, y, w, h, kS2Surf, kS2Mask);
    return 0;
}
typedef int(__cdecl* C4_t)(int a, int b, int c, int d);
C4_t DrawPanelS2_orig;

// 0x0040cef0  drawFrameR3(x, y, w, h, fill): cdecl. Scale (1, 1, 1) (0x0040cef6..0x0040cf02); 16-rounding; when
// fill != 0 (0x0040cf76) blitPanel(x, y, w, h, 1) (0x0040cf78..0x0040cf7e); then the border from the Surfaces
// 0x005a53c8.. with masks 0x0058b680.. (0x0040cf86..0x0040d0a6).
static const int kR3Surf[8] = {0x005a53c8, 0x005a53f4, 0x005a54fc, 0x005a544c, 0x005a54a4, 0x005a5420, 0x005a5528,
                               0x005a54d0};
static const int kR3Mask[8] = {0x0058b680, 0x0058b6ac, 0x0058b7b4, 0x0058b704, 0x0058b75c, 0x0058b6d8, 0x0058b7e0,
                               0x0058b788};
int __cdecl DrawFrameR3_re(int x, int y, int w, int h, int fill) {
    resetScale111();
    round16(x, w);
    round16(y, h);
    if (fill != 0) kBlitPanel(x, y, w, h, 1);
    frameBorder(x, y, w, h, kR3Surf, kR3Mask);
    return 0;
}
C5_t DrawFrameR3_orig;

// 0x0040d0b0  drawFrameS2(x, y, w, h, fill): cdecl. Scale (1, 1, 1) (0x0040d0b8..0x0040d0c2); 16-rounding; when
// fill != 0 (0x0040d136) fillWindowRect(0x00519a60; x + 2, y + 2, w - 4, h - 4, 0x80007fdc) (0x0040d138..0x0040d152);
// then the border with the style s = (signed char)*0x005a34e0, which the original re-reads before every piece
// (0x0040d157 and twins; none of the calls writes it, so it is read once here):
// Surfaces 0x00562394 + 0x160 * s.., masks 0x0059d94c + 0x160 * s.. (`lea eax, [eax + eax*4]; lea eax, [eax + edx*2];
// shl eax, 5` = s * 0x160).
static const int kFS2Surf[8] = {0x00562394, 0x00562418, 0x00562368, 0x005623c0, 0x00562470, 0x00562444, 0x0056249c,
                                0x005623ec};
static const int kFS2Mask[8] = {0x0059d94c, 0x0059d9d0, 0x0059d920, 0x0059d978, 0x0059da28, 0x0059d9fc, 0x0059da54,
                                0x0059d9a4};
int __cdecl DrawFrameS2_re(int x, int y, int w, int h, int fill) {
    resetScale111();
    round16(x, w);
    round16(y, h);
    if (fill != 0) kFillWindowRect(kMainWindow, 0, x + 2, y + 2, w - 4, h - 4, static_cast<int>(0x80007fdc));
    int off = *kFrameStyle * 0x160;
    int s[8], m[8];
    for (int i = 0; i < 8; i++) {
        s[i] = kFS2Surf[i] + off;
        m[i] = kFS2Mask[i] + off;
    }
    frameBorder(x, y, w, h, s, m);
    return 0;
}
C5_t DrawFrameS2_orig;

// ================================================================================================================
// Lines
// ================================================================================================================

// 0x004049a0  drawLineOnSurface(widget, x0, y0, x1, y1, colour): cdecl. Widget_fillRect(x0, y0, x1, y1, colour & 0xff)
// on widget (0x004049ac `and eax, 0xff`, 0x004049be `mov ecx, [esp+0x18]` = the first argument, call 0x004049c2);
// returns its eax.
int __cdecl DrawLineOnSurface_re(int widget, int x0, int y0, int x1, int y1, int colour) {
    return kWidgetFillRect(reinterpret_cast<void*>(widget), 0, x0, y0, x1, y1, colour & 0xff);
}
C6_t DrawLineOnSurface_orig;

// 0x0046e710  drawRectScaled(x, y, w, h, colour): cdecl. sx = scaleX(x), sy = scaleX(y), sw = scaleX(w), sh =
// scaleX(h) (0x0046e719 / 0x0046e725 / 0x0046e731 / 0x0046e73d, in that order); x2 = sx + sw (0x0046e74c), y2 = sy + sh
// (0x0046e765); four drawLineOnSurface calls on the target *0x004c1570, re-read before each: (sx, sy, x2, sy)
// (0x0046e756), (sx, y2, x2, y2) (0x0046e76d), (x2, sy, x2, y2) (0x0046e784), (sx, sy, sx, y2) (0x0046e799).
int __cdecl DrawRectScaled_re(int x, int y, int w, int h, int colour) {
    int sx = kScaleX(x);
    int sy = kScaleX(y);
    int sw = kScaleX(w);
    int sh = kScaleX(h);
    int x2 = add(sw, sx);
    kDrawLineOnSurface(reinterpret_cast<int>(*kTarget), sx, sy, x2, sy, colour);
    int y2 = add(sh, sy);
    kDrawLineOnSurface(reinterpret_cast<int>(*kTarget), sx, y2, x2, y2, colour);
    kDrawLineOnSurface(reinterpret_cast<int>(*kTarget), x2, sy, x2, y2, colour);
    return kDrawLineOnSurface(reinterpret_cast<int>(*kTarget), sx, sy, sx, y2, colour);
}
C5_t DrawRectScaled_orig;

SG_HOOK("golf_clean.exe", 0x00473f60, Surface_stretchBlit, SurfaceStretchBlit_re, SurfaceStretchBlit_orig);
SG_HOOK("golf_clean.exe", 0x00473fc0, Surface_blitMasked, SurfaceBlitMasked_re, SurfaceBlitMasked_orig);
SG_HOOK("golf_clean.exe", 0x004740f0, Surface_blit2, SurfaceBlit2_re, SurfaceBlit2_orig);
SG_HOOK("golf_clean.exe", 0x004741b0, Sprite_draw, SpriteDraw_re, SpriteDraw_orig);
SG_HOOK("golf_clean.exe", 0x00474260, Sprite_drawScaled, SpriteDrawScaled_re, SpriteDrawScaled_orig);
SG_HOOK("golf_clean.exe", 0x00474440, Surface_drawSprite, SurfaceDrawSprite_re, SurfaceDrawSprite_orig);
SG_HOOK("golf_clean.exe", 0x00474030, Surface_drawScaled2, SurfaceDrawScaled2_re, SurfaceDrawScaled2_orig);
SG_HOOK("golf_clean.exe", 0x00474380, Surface_draw, SurfaceDraw_re, SurfaceDraw_orig);
SG_HOOK("golf_clean.exe", 0x00421b90, drawScaledSprite421b90, DrawScaledSprite421b90_re, DrawScaledSprite421b90_orig);
SG_HOOK("golf_clean.exe", 0x00475c60, Surface_blitThunk, SurfaceBlitThunk_re, SurfaceBlitThunk_orig);
SG_HOOK("golf_clean.exe", 0x00473c60, Surface_blitTakeDirty, SurfaceBlitTakeDirty_re, SurfaceBlitTakeDirty_orig);
SG_HOOK("golf_clean.exe", 0x004744c0, Surface_copyFrom, SurfaceCopyFrom_re, SurfaceCopyFrom_orig);
SG_HOOK("golf_clean.exe", 0x00474550, Surface_create, SurfaceCreate_re, SurfaceCreate_orig);
SG_HOOK("golf_clean.exe", 0x004745c0, Surface_init2, SurfaceInit2_re, SurfaceInit2_orig);
SG_HOOK("golf_clean.exe", 0x0040ca10, blitPanel, BlitPanel_re, BlitPanel_orig);
SG_HOOK("golf_clean.exe", 0x0040cc00, drawPanelS2, DrawPanelS2_re, DrawPanelS2_orig);
SG_HOOK("golf_clean.exe", 0x0040cef0, drawFrameR3, DrawFrameR3_re, DrawFrameR3_orig);
SG_HOOK("golf_clean.exe", 0x0040d0b0, drawFrameS2, DrawFrameS2_re, DrawFrameS2_orig);
SG_HOOK("golf_clean.exe", 0x004049a0, drawLineOnSurface, DrawLineOnSurface_re, DrawLineOnSurface_orig);
SG_HOOK("golf_clean.exe", 0x0046e710, drawRectScaled, DrawRectScaled_re, DrawRectScaled_orig);

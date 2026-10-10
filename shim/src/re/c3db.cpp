// C3 batch c3db (round 24, 2026-10-10) of golf_clean.exe: drawing helpers from 0x004781f0 up that earlier batches
// dropped because they draw or reach the display object at 0x0083ad50.
//
//   dotted lines into a jgld Surface: Image::hline 0x00485d40, Image::vline 0x00485e80 (8-bit),
//     drawColorRunA 0x00494d30, drawColorRunB 0x00494f00 (16-bit), and their depth dispatchers
//     Widget_drawHLineDepth 0x00478c10 / Widget_drawVLineDepth 0x00478c70
//   boxes: Widget_drawBox 0x00479610, Widget_drawBoxR 0x00479670, Widget_drawBevelBox 0x00479560,
//     Widget_drawBevelBoxR 0x004795d0
//   fills: Win_fillRect 0x00478af0, fillWindowRect 0x00480b00
//   pixel reads: drawLineH 0x00478df0 (reads one pixel), blendRed 0x00479830, blendGreen 0x004798c0,
//     drawPixelRun 0x00479950 (the red, green and blue component of one pixel)
//   convex polygons: fillConvexPoly 0x00493520, fillConvexPoly8 0x00493100, fillConvexPoly16 0x004932d0
//   the window array: listRemove 0x0047e520
//   TextView::setLineSpacing 0x0048e0b0
//
// Every body is hand-written from the disassembly (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr> --list)
// and cites the address of each instruction it reproduces. __thiscall is emulated with __fastcall (ecx = this, edx
// unused). Callees run through their original addresses:
//   jgld.dll Surface slots (vtable RVA 0x11d0b0, re/names/jgld_2.tsv): +0x10 acquireDC2, +0x14 call3a, +0x18
//     fwdVfn18, +0x1c call3b, +0x20 fwdVfn20, +0x24 releaseDC2, +0x44 fillRectDispatch, +0x64 drawLine, +0xcc
//     getClipRectPtr (RECT {left, top, right, bottom}), +0xd8 width, +0xdc height, +0xe0 getField40 (row pitch in
//     pixels), +0xe4 getDepthPtr ([0] bits per pixel, [1] 16-bit format code)
//   jgld.dll Palette slots +0x18 / +0x1c (the two 256-entry 16-bit colour tables, c3ce)
//   the display object [0x0083ad50]: slot +0xb4 (16-bit pixel format, 1 = RGB565, c3ad) and +0xa8
//   golf_clean.exe: Widget_drawHLine 0x00478bb0 / Widget_drawVLine 0x00478be0 (C3, c3t), Widget_fillArea 0x00478b50
//     (C3), Surface_pixelPtr 0x004796a0 (C3, c3p), polyEdgeStep 0x00492ed0 (C3, c3x), polyEdgeAdvance 0x00492fa0,
//     fillSpanDown 0x00493000, fillSpanUp 0x00493080 (C3, c3t), and this batch's own functions.
#include "hooks.h"

namespace {

static inline char* B(void* p) { return reinterpret_cast<char*>(p); }
static inline int& I(void* base, int off) { return *reinterpret_cast<int*>(B(base) + off); }
static inline void* Pp(void* base, int off) { return *reinterpret_cast<void**>(B(base) + off); }
template <typename T> static inline T& G(unsigned addr) { return *reinterpret_cast<T*>(addr); }
// `mov eax, [obj]; call [eax+off]`: slot off of the object's own vtable.
template <typename T> static inline T vslot(void* obj, int off) {
    return *reinterpret_cast<T*>(*reinterpret_cast<char**>(obj) + off);
}

typedef void*(__fastcall* V0p_t)(void* ecx, void* edx);
typedef int(__fastcall* V0i_t)(void* ecx, void* edx);
typedef void*(__fastcall* V2p_t)(void* ecx, void* edx, int a, int b);
typedef int(__fastcall* V1i_t)(void* ecx, void* edx, int a);
typedef int(__fastcall* V2i_t)(void* ecx, void* edx, int a, int b);

// Surface slots (thiscall on the jgld Surface)
static inline int* clipOf(void* s) { return reinterpret_cast<int*>(vslot<V0p_t>(s, 0xcc)(s, 0)); }
static inline int* depthOf(void* s) { return reinterpret_cast<int*>(vslot<V0p_t>(s, 0xe4)(s, 0)); }
static inline int pitchOf(void* s) { return vslot<V0i_t>(s, 0xe0)(s, 0); }
static inline void releaseBits(void* s) { vslot<V1i_t>(s, 0x24)(s, 0, 1); }        // `push 1; call [vt+0x24]`
// A depth pointer read as the original does: a null Surface gives a null pointer (`xor eax, eax`), which the
// caller then dereferences exactly as the original would.
static inline int* depthOrNull(void* s) {
    if (s) return depthOf(s);
    return 0;
}

// The 16-bit colour lookup shared by drawColorRunA/B and fillConvexPoly16: unless bit 31 of the colour is set, and when
// the object at [0x0083ad0c] exists, the 16-bit format code ([depth descriptor + 4]) picks a table from the jgld
// Palette at [that object + 4]: format 0 -> the Palette's slot +0x18, format 1 -> slot +0x1c, any other format makes
// the caller stop (returns false). A null Palette gives a null table (`xor eax, eax`), which is then read as the
// original reads it. The entry is the word at table + 2 * (colour & 0xff).
static bool lookupColor16(void* surface, unsigned& colour) {
    if (colour & 0x80000000u) return true;
    void* holder = G<void*>(0x0083ad0c);
    if (holder == 0) return true;
    const int fmt = depthOrNull(surface)[1];
    if (fmt != 0 && fmt != 1) return false;
    void* pal = Pp(holder, 4);
    const unsigned short* tab = 0;
    int slot = 0x1c;
    if (fmt == 0) slot = 0x18;
    if (pal) tab = reinterpret_cast<const unsigned short*>(vslot<V0p_t>(pal, slot)(pal, 0));
    colour = tab[colour & 0xff];
    return true;
}

// ---- dotted lines ---------------------------------------------------------------------------------------------
// The four line bodies share one shape. The Surface is this+4; a null Surface returns at once (0x00485d4d,
// 0x00485e8e, 0x00494d3d, 0x00494f0e). The clip RECT comes from slot +0xcc. All compares are signed. The pixel
// count is (last - first + 1) >> 1 (unsigned shr), so every second pixel is written, from the first one; the store
// loop runs `do { ... } while (--n)` (`loop` in the 8-bit pair, `dec ecx; jne` in the 16-bit pair), so a span that
// clips down to one pixel (n = 0) wraps the counter in the original as well. Afterwards slot +0x24 is called with 1.

// 0x00485d40  Image::hline(this, x0, x1, y, colour): 8-bit horizontal dotted line.
//   y < clip.top -> return (0x00485d63 jl); y >= clip.bottom (0x00485d7f jge); x0 == x1 (0x00485d8d je);
//   x0 > x1 -> swap (0x00485d93 jle, xor swap 0x00485d95..0x00485d9e); x0 >= clip.right (0x00485db7 jge);
//   x1 < clip.left (0x00485dd2 jl); x0 < clip.left -> x0 = left (0x00485ded / 0x00485e04); x1 >= clip.right ->
//   x1 = right - 1 (0x00485e1d / 0x00485e32..0x00485e36). p = slot +0x14 (x0, y) (0x00485e47); null -> return
//   (0x00485e4f). Stores the colour's low byte (0x00485e59 `mov ah, [ebp+0x14]`) every 2 bytes (0x00485e61..0x00485e66).
typedef void(__fastcall* Line4_t)(void* self, void* edx, int a, int b, int c, int d);
Line4_t HLine8_orig;
void __fastcall HLine8_re(void* self, void*, int x0, int x1, int y, int colour) {
    void* s = Pp(self, 4);
    if (s == 0) return;
    const int* clip = clipOf(s);
    if (y < clip[1] || y >= clip[3] || x0 == x1) return;
    if (x0 > x1) { const int t = x0; x0 = x1; x1 = t; }
    if (x0 >= clip[2] || x1 < clip[0]) return;
    if (x0 < clip[0]) x0 = clip[0];
    if (x1 >= clip[2]) x1 = clip[2] - 1;
    unsigned char* p = reinterpret_cast<unsigned char*>(vslot<V2p_t>(s, 0x14)(s, 0, x0, y));
    if (p == 0) return;
    unsigned n = static_cast<unsigned>(x1 - x0 + 1) >> 1;
    do { *p = static_cast<unsigned char>(colour); p += 2; } while (--n != 0);
    releaseBits(s);
}

// 0x00485e80  Image::vline(this, x, y0, y1, colour): 8-bit vertical dotted line.
//   x < clip.left (0x00485ea3 jl); x >= clip.right (0x00485ebf jge); y0 == y1 (0x00485ecd je); swap (0x00485ed3 jle);
//   y0 >= clip.bottom (0x00485ef7 jge); y1 < clip.top (0x00485f13 jl); y0 < clip.top -> top (0x00485f2f /
//   0x00485f44); y1 >= clip.bottom -> bottom - 1 (0x00485f60 / 0x00485f75..0x00485f79). p = slot +0x14 (x, y0)
//   (0x00485f8a), null -> return (0x00485f92). Step = pitch (slot +0xe0, 0x00485f9d) * 2 bytes (0x00485fc2 shl).
Line4_t VLine8_orig;
void __fastcall VLine8_re(void* self, void*, int x, int y0, int y1, int colour) {
    void* s = Pp(self, 4);
    if (s == 0) return;
    const int* clip = clipOf(s);
    if (x < clip[0] || x >= clip[2] || y0 == y1) return;
    if (y0 > y1) { const int t = y0; y0 = y1; y1 = t; }
    if (y0 >= clip[3] || y1 < clip[1]) return;
    if (y0 < clip[1]) y0 = clip[1];
    if (y1 >= clip[3]) y1 = clip[3] - 1;
    unsigned char* p = reinterpret_cast<unsigned char*>(vslot<V2p_t>(s, 0x14)(s, 0, x, y0));
    if (p == 0) return;
    const unsigned step = static_cast<unsigned>(pitchOf(s)) << 1;
    unsigned n = static_cast<unsigned>(y1 - y0 + 1) >> 1;
    do { *p = static_cast<unsigned char>(colour); p += step; } while (--n != 0);
    releaseBits(s);
}

// 0x00494d30  drawColorRunA(this, x0, x1, y, colour): 16-bit horizontal dotted line.
//   y < clip.top (0x00494d53 jl); y >= clip.bottom (0x00494d6f jge); x0 == x1 (0x00494d7d je); then the colour lookup
//   (bit 31 test 0x00494d86, [0x0083ad0c] 0x00494d8e, format switch 0x00494dae..0x00494db4 whose default returns at
//   0x00494db4 jne, Palette slots +0x1c 0x00494dc3 / +0x18 0x00494dd1, word read 0x00494de0), colour &= 0xffff
//   (0x00494dec); swap (0x00494df7 jle); x0 >= clip.right (0x00494e20 jge); x1 < clip.left (0x00494e40 jl); clamp
//   x0 (0x00494e60 / 0x00494e77) and x1 (0x00494e95 / 0x00494eae). p = slot +0x1c (x0, y) (0x00494ec2), null ->
//   return (0x00494eca). Stores the word every 4 bytes (0x00494edd..0x00494ee4).
Line4_t RunA_orig;
void __fastcall RunA_re(void* self, void*, int x0, int x1, int y, int colour) {
    void* s = Pp(self, 4);
    if (s == 0) return;
    const int* clip = clipOf(s);
    if (y < clip[1] || y >= clip[3] || x0 == x1) return;
    unsigned c = static_cast<unsigned>(colour);
    if (!lookupColor16(s, c)) return;
    c &= 0xffff;
    if (x0 > x1) { const int t = x0; x0 = x1; x1 = t; }
    if (x0 >= clip[2] || x1 < clip[0]) return;
    if (x0 < clip[0]) x0 = clip[0];
    if (x1 >= clip[2]) x1 = clip[2] - 1;
    unsigned short* p = reinterpret_cast<unsigned short*>(vslot<V2p_t>(s, 0x1c)(s, 0, x0, y));
    if (p == 0) return;
    unsigned n = static_cast<unsigned>(x1 - x0 + 1) >> 1;
    do { *p = static_cast<unsigned short>(c); p += 2; } while (--n != 0);
    releaseBits(s);
}

// 0x00494f00  drawColorRunB(this, x, y0, y1, colour): 16-bit vertical dotted line.
//   x < clip.left (0x00494f23 jl); x >= clip.right (0x00494f3f jge); y0 == y1 (0x00494f4d je); colour lookup as in
//   drawColorRunA (0x00494f56..0x00494fb0, default format returns at 0x00494f84), & 0xffff (0x00494fbc); swap
//   (0x00494fc7 jle); y0 >= clip.bottom (0x00494ff0 jge); y1 < clip.top (0x00495011 jl); clamp y0 (0x00495032 /
//   0x00495047) and y1 (0x00495068 / 0x0049507d..0x00495080). p = slot +0x1c (x, y0) (0x00495095), null -> return
//   (0x0049509d). Step = pitch (slot +0xe0, 0x004950a8) * 4 bytes (0x004950ce shl ebx, 2).
Line4_t RunB_orig;
void __fastcall RunB_re(void* self, void*, int x, int y0, int y1, int colour) {
    void* s = Pp(self, 4);
    if (s == 0) return;
    const int* clip = clipOf(s);
    if (x < clip[0] || x >= clip[2] || y0 == y1) return;
    unsigned c = static_cast<unsigned>(colour);
    if (!lookupColor16(s, c)) return;
    c &= 0xffff;
    if (y0 > y1) { const int t = y0; y0 = y1; y1 = t; }
    if (y0 >= clip[3] || y1 < clip[1]) return;
    if (y0 < clip[1]) y0 = clip[1];
    if (y1 >= clip[3]) y1 = clip[3] - 1;
    unsigned char* p = reinterpret_cast<unsigned char*>(vslot<V2p_t>(s, 0x1c)(s, 0, x, y0));
    if (p == 0) return;
    const unsigned step = static_cast<unsigned>(pitchOf(s)) << 2;
    unsigned n = static_cast<unsigned>(y1 - y0 + 1) >> 1;
    do { *reinterpret_cast<unsigned short*>(p) = static_cast<unsigned short>(c); p += step; } while (--n != 0);
    releaseBits(s);
}

// 0x00478c10  Widget_drawHLineDepth(this, a, b, c, d): the bit depth is [slot +0xe4 of this+4] (0x00478c1c /
// 0x00478c26; a null Surface reads address 0 as the original does). Depth 8 -> Image::hline 0x00485d40 (0x00478c67),
// 16 -> drawColorRunA 0x00494d30 (0x00478c48), any other depth does nothing (0x00478c30 jne). Arguments unchanged.
typedef void(__fastcall* Line4c_t)(void* self, void* edx, int a, int b, int c, int d);
const Line4c_t kHLine8 = reinterpret_cast<Line4c_t>(0x00485d40);
const Line4c_t kRunA = reinterpret_cast<Line4c_t>(0x00494d30);
const Line4c_t kVLine8 = reinterpret_cast<Line4c_t>(0x00485e80);
const Line4c_t kRunB = reinterpret_cast<Line4c_t>(0x00494f00);
Line4_t HLineDepth_orig;
void __fastcall HLineDepth_re(void* self, void*, int a, int b, int c, int d) {
    const int depth = *depthOrNull(Pp(self, 4));
    if (depth == 8) kHLine8(self, 0, a, b, c, d);
    else if (depth == 16) kRunA(self, 0, a, b, c, d);
}

// 0x00478c70  Widget_drawVLineDepth(this, a, b, c, d): as above with Image::vline 0x00485e80 for depth 8
// (0x00478cc7) and drawColorRunB 0x00494f00 for depth 16 (0x00478ca8); other depths nothing (0x00478c90 jne).
Line4_t VLineDepth_orig;
void __fastcall VLineDepth_re(void* self, void*, int a, int b, int c, int d) {
    const int depth = *depthOrNull(Pp(self, 4));
    if (depth == 8) kVLine8(self, 0, a, b, c, d);
    else if (depth == 16) kRunB(self, 0, a, b, c, d);
}

// ---- boxes ---------------------------------------------------------------------------------------------------
const Line4c_t kHLineDepth = reinterpret_cast<Line4c_t>(0x00478c10);
const Line4c_t kVLineDepth = reinterpret_cast<Line4c_t>(0x00478c70);
const Line4c_t kWidgetHLine = reinterpret_cast<Line4c_t>(0x00478bb0);   // (x0, x1, y, colour), C3 c3t
const Line4c_t kWidgetVLine = reinterpret_cast<Line4c_t>(0x00478be0);   // (x, y0, y1, colour), C3 c3t

// 0x00479610  Widget_drawBox(this, l, t, r, b, colour): four dotted edges through the depth dispatchers:
//   hline (l+1 .. r, y = t) (0x00479626 inc, 0x0047962b), hline (l .. r-1, y = b) (0x00479639 lea, 0x00479641),
//   vline (x = l, t .. b) (0x00479654), vline (x = r, t+1 .. b) (0x0047965e inc, 0x00479664). Returns nothing.
typedef void(__fastcall* Box5_t)(void* self, void* edx, int l, int t, int r, int b, int c);
Box5_t Box_orig;
void __fastcall Box_re(void* self, void*, int l, int t, int r, int b, int c) {
    kHLineDepth(self, 0, l + 1, r, t, c);
    kHLineDepth(self, 0, l, r - 1, b, c);
    kVLineDepth(self, 0, l, t, b, c);
    kVLineDepth(self, 0, r, t + 1, b, c);
}

// 0x00479670  Widget_drawBoxR(this, rect, colour): a null rect returns 0x10 (0x00479676 jne / 0x00479678); else
// Widget_drawBox (left, top, right - 1, bottom - 1, colour) (0x00479685..0x00479696) and returns 0 (0x0047969b).
typedef int(__fastcall* BoxR_t)(void* self, void* edx, const int* rect, int c);
const Box5_t kBox = reinterpret_cast<Box5_t>(0x00479610);
BoxR_t BoxR_orig;
int __fastcall BoxR_re(void* self, void*, const int* rect, int c) {
    if (rect == 0) return 0x10;
    kBox(self, 0, rect[0], rect[1], rect[2] - 1, rect[3] - 1, c);
    return 0;
}

// 0x00479560  Widget_drawBevelBox(this, l, t, r, b, light, dark): solid edges through Widget_drawHLine 0x00478bb0
// and Widget_drawVLine 0x00478be0 (both draw with the Surface's drawLine slot +0x64): top (l+1 .. r, y = t, light)
// (0x00479578 lea, 0x0047957f), bottom (l .. r-1, y = b, dark) (0x0047958d lea, 0x00479595), left (x = l, t .. b-1,
// light) (0x004795a2 dec, 0x004795a9), right (x = r, t+1 .. b, dark) (0x004795b7 inc, 0x004795bd). Returns nothing.
typedef void(__fastcall* Bevel_t)(void* self, void* edx, int l, int t, int r, int b, int c1, int c2);
Bevel_t Bevel_orig;
void __fastcall Bevel_re(void* self, void*, int l, int t, int r, int b, int light, int dark) {
    kWidgetHLine(self, 0, l + 1, r, t, light);
    kWidgetHLine(self, 0, l, r - 1, b, dark);
    kWidgetVLine(self, 0, l, t, b - 1, light);
    kWidgetVLine(self, 0, r, t + 1, b, dark);
}

// 0x004795d0  Widget_drawBevelBoxR(this, rect, light, dark): a null rect returns 3 (0x004795d6 jne / 0x004795d8);
// else Widget_drawBevelBox (left, top, right - 1, bottom - 1, light, dark) (0x004795e0..0x004795fb), returns 0
// (0x00479600).
typedef int(__fastcall* BevelR_t)(void* self, void* edx, const int* rect, int c1, int c2);
const Bevel_t kBevel = reinterpret_cast<Bevel_t>(0x00479560);
BevelR_t BevelR_orig;
int __fastcall BevelR_re(void* self, void*, const int* rect, int light, int dark) {
    if (rect == 0) return 3;
    kBevel(self, 0, rect[0], rect[1], rect[2] - 1, rect[3] - 1, light, dark);
    return 0;
}

// ---- fills ---------------------------------------------------------------------------------------------------
// 0x00478af0  Win_fillRect(this, x, y, w, h, colour): builds the RECT {x, y, x + w, y + h} on the stack
// (0x00478b00..0x00478b14) and returns Widget_fillArea 0x00478b50 (this, &rect, colour) (0x00478b22).
typedef int(__fastcall* FillArea_t)(void* self, void* edx, const int* rect, int c);
const FillArea_t kFillArea = reinterpret_cast<FillArea_t>(0x00478b50);
typedef int(__fastcall* Fill5_t)(void* self, void* edx, int x, int y, int w, int h, int c);
Fill5_t WinFill_orig;
int __fastcall WinFill_re(void* self, void*, int x, int y, int w, int h, int c) {
    const int rect[4] = {x, y, static_cast<int>(static_cast<unsigned>(x) + static_cast<unsigned>(w)),
                         static_cast<int>(static_cast<unsigned>(y) + static_cast<unsigned>(h))};
    return kFillArea(self, 0, rect, c);
}

// 0x00480b00  fillWindowRect(this, x, y, w, h, colour): Win_fillRect 0x00478af0 on this + 0x274 (0x00480b19, the
// Surface sub-object of every Window class) with the same five arguments (0x00480b08..0x00480b18); returns its result.
const Fill5_t kWinFill = reinterpret_cast<Fill5_t>(0x00478af0);
Fill5_t WindowFill_orig;
int __fastcall WindowFill_re(void* self, void*, int x, int y, int w, int h, int c) {
    return kWinFill(B(self) + 0x274, 0, x, y, w, h, c);
}

// ---- pixel reads ---------------------------------------------------------------------------------------------
typedef void*(__fastcall* PixelPtr_t)(void* self, void* edx, int x, int y);
const PixelPtr_t kPixelPtr = reinterpret_cast<PixelPtr_t>(0x004796a0);

// 0x00478df0  drawLineH(this, x, y): returns the pixel at (x, y). 0 when this+4 is null (0x00478dfa), x < 0
// (0x00478e06 jl), y < 0 (0x00478e12 jl), x >= width (slot +0xd8, 0x00478e1e jge) or y >= height (slot +0xdc,
// 0x00478e35 jge). p = Surface_pixelPtr 0x004796a0 (x, y) (0x00478e3b); slot +0x24 is called with 1 when this+4 is
// non-null (0x00478e4d), BEFORE the pixel is read; a null p returns 0 (0x00478e52). Depth 8 -> the byte
// (0x00478e88), 16 -> the word (0x00478e7d), any other depth -> the dword (0x00478e73).
typedef unsigned(__fastcall* Get2_t)(void* self, void* edx, int x, int y);
Get2_t GetPixel_orig;
unsigned __fastcall GetPixel_re(void* self, void*, int x, int y) {
    void* s = Pp(self, 4);
    if (s == 0 || x < 0 || y < 0) return 0;
    if (x >= vslot<V0i_t>(s, 0xd8)(s, 0)) return 0;
    s = Pp(self, 4);
    int height = 0;
    if (s) height = vslot<V0i_t>(s, 0xdc)(s, 0);
    if (y >= height) return 0;
    void* p = kPixelPtr(self, 0, x, y);
    s = Pp(self, 4);
    if (s) releaseBits(s);
    if (p == 0) return 0;
    const int depth = *depthOrNull(Pp(self, 4));
    if (depth == 8) return *reinterpret_cast<unsigned char*>(p);
    if (depth == 16) return *reinterpret_cast<unsigned short*>(p);
    return *reinterpret_cast<unsigned*>(p);
}

// The three component readers (this, x, y, def) share one shape: p = Surface_pixelPtr (x, y); a null p returns the
// low byte of def at once (no release). Otherwise the depth decides: 16 -> from the pixel word, 24 or 32 -> one byte
// of the pixel, any other depth -> the low byte of def. Slot +0x24 is then called with 1 when this+4 is non-null, and
// the byte is returned in al (the upper bytes of eax are not set by these bodies; the registry compares uint8).
// For depth 16 the red and green readers call slot +0xb4 of the display object [0x0083ad50] (no null check), whose
// result 1 means RGB565.
static unsigned char finishComponent(void* self, unsigned char v) {
    void* s = Pp(self, 4);
    if (s) releaseBits(s);
    return v;
}
static int displayFormat() {
    void* disp = G<void*>(0x0083ad50);
    return vslot<V0i_t>(disp, 0xb4)(disp, 0);
}
typedef unsigned char(__fastcall* Comp_t)(void* self, void* edx, int x, int y, int def);

// 0x00479830  blendRed(this, x, y, def): p null -> def (0x00479849 / 0x0047984b). Depth 16 (0x0047986d je): the
// word (0x00479886) is read before the display call (0x0047988b); format 1 -> (word >> 8) & 0xf8 (0x00479896 /
// 0x00479899), else (word >> 7) & 0xf8 (0x0047989e / 0x004798a1). Depth 24 or 32 (0x00479872 / 0x00479877) -> byte
// p[2] (0x00479879). Otherwise def (0x004798a6). Release at 0x004798b5.
Comp_t Red_orig;
unsigned char __fastcall Red_re(void* self, void*, int x, int y, int def) {
    const unsigned char* p = reinterpret_cast<const unsigned char*>(kPixelPtr(self, 0, x, y));
    if (p == 0) return static_cast<unsigned char>(def);
    const int depth = *depthOrNull(Pp(self, 4));
    unsigned char v;
    if (depth == 16) {
        const unsigned w = *reinterpret_cast<const unsigned short*>(p);
        if (displayFormat() == 1) v = static_cast<unsigned char>((w >> 8) & 0xf8);
        else v = static_cast<unsigned char>((w >> 7) & 0xf8);
    } else if (depth == 24 || depth == 32) {
        v = p[2];
    } else {
        v = static_cast<unsigned char>(def);
    }
    return finishComponent(self, v);
}

// 0x004798c0  blendGreen(this, x, y, def): as blendRed with format 1 -> (word >> 3) & 0xfc (0x00479926 / 0x00479929),
// else (word >> 2) & 0xf8 (0x0047992e / 0x00479931); depth 24 or 32 -> byte p[1] (0x00479909). Release at 0x00479945.
Comp_t Green_orig;
unsigned char __fastcall Green_re(void* self, void*, int x, int y, int def) {
    const unsigned char* p = reinterpret_cast<const unsigned char*>(kPixelPtr(self, 0, x, y));
    if (p == 0) return static_cast<unsigned char>(def);
    const int depth = *depthOrNull(Pp(self, 4));
    unsigned char v;
    if (depth == 16) {
        const unsigned w = *reinterpret_cast<const unsigned short*>(p);
        if (displayFormat() == 1) v = static_cast<unsigned char>((w >> 3) & 0xfc);
        else v = static_cast<unsigned char>((w >> 2) & 0xf8);
    } else if (depth == 24 || depth == 32) {
        v = p[1];
    } else {
        v = static_cast<unsigned char>(def);
    }
    return finishComponent(self, v);
}

// 0x00479950  drawPixelRun(this, x, y, def): as blendRed without a display call: depth 16 (0x0047998d je) -> the
// pixel's first byte shifted left by 3, kept to 8 bits (0x0047999d `mov bl, [esi]; shl bl, 3`); depth 24 or 32 ->
// byte p[0] (0x00479999); otherwise def (0x004799a4). Release at 0x004799b3.
Comp_t Blue_orig;
unsigned char __fastcall Blue_re(void* self, void*, int x, int y, int def) {
    const unsigned char* p = reinterpret_cast<const unsigned char*>(kPixelPtr(self, 0, x, y));
    if (p == 0) return static_cast<unsigned char>(def);
    const int depth = *depthOrNull(Pp(self, 4));
    unsigned char v;
    if (depth == 16) v = static_cast<unsigned char>(p[0] << 3);
    else if (depth == 24 || depth == 32) v = p[0];
    else v = static_cast<unsigned char>(def);
    return finishComponent(self, v);
}

// ---- convex polygons -----------------------------------------------------------------------------------------
// Scan-conversion state in globals (names from c3x / c3t): 0x0083d34c fill colour, 0x0083d350 index of the lowest
// vertex (largest y), 0x0083d358 vertex table ({x, y} dwords), 0x0083d35c clip left, 0x0083d360 clip top,
// 0x0083d378 vertex count, 0x0083d37c 16-bit base, 0x0083d380 8-bit base, 0x0083d384 current row, 0x0083d388 row
// stride in bytes, 0x0083d38c clip right - 1, 0x0083d390 clip bottom - 1.
// Edge records are 0x24 bytes on the stack ([esp+0x10] and [esp+0x34]); +0 is the walk direction (-1 / +1) the
// caller sets, +0xc the edge's x; polyEdgeStep 0x00492ed0 fills the rest.
typedef int(__cdecl* EdgeStep_t)(void* edge, int idx);
typedef int(__cdecl* EdgeAdv_t)(void* edge);
typedef void(__cdecl* Span_t)(int x0, int x1);
const EdgeStep_t kEdgeStep = reinterpret_cast<EdgeStep_t>(0x00492ed0);
const EdgeAdv_t kEdgeAdvance = reinterpret_cast<EdgeAdv_t>(0x00492fa0);
const Span_t kSpanDown = reinterpret_cast<Span_t>(0x00493000);   // 16-bit spans
const Span_t kSpanUp = reinterpret_cast<Span_t>(0x00493080);     // 8-bit spans

// The part both depths share, from the clip RECT on (0x00493179..0x004932a6 / 0x004933ba..0x004934e7). `vertsArg`
// is the second argument's stack slot: the scan for the topmost vertex stores the index of each new minimum into it
// (0x004931de / 0x0049341f), so when no y is below 0x7fff the walk starts from the pointer value itself.
static void scanPolygon(void* holder, const int* verts, int count, int vertsArg, Span_t span) {
    void* s = Pp(holder, 4);
    if (s == 0) return;                                                   // 0x00493173 / 0x004933b4
    const int* clip = clipOf(s);
    if (clip == 0) return;                                                // 0x00493183 / 0x004933c4
    G<int>(0x0083d38c) = clip[2] - 1;
    G<int>(0x0083d35c) = clip[0];
    G<int>(0x0083d390) = clip[3] - 1;
    G<int>(0x0083d360) = clip[1];
    if (count <= 0) return;                                               // 0x004931bc / 0x004933fd jle
    int minY = 0x7fff, maxY = -0x7ffd;                                    // 0x00493192 / 0x0049318c (0xffff8003)
    int start = vertsArg;
    for (int i = 0; i < count; i++) {
        const int y = verts[i * 2 + 1];
        if (y < minY) { minY = y; start = i; }                            // 0x004931d6 jge
        if (y > maxY) { maxY = y; G<int>(0x0083d350) = i; }               // 0x004931e4 jle
    }
    if (minY >= maxY) return;                                             // 0x004931f8 jge
    G<int>(0x0083d384) = minY;
    int e1[9] = {0}, e2[9] = {0};
    e1[0] = -1;                                                           // 0x0049320e
    if (kEdgeStep(e1, start) == 0) return;
    e2[0] = 1;                                                            // 0x0049322c
    if (kEdgeStep(e2, start) == 0) return;
    if (G<int>(0x0083d384) >= G<int>(0x0083d390)) return;                 // 0x0049324d jge
    for (;;) {
        if (G<int>(0x0083d384) >= G<int>(0x0083d360)) {                   // 0x00493255 jl
            const int a = e1[3], b = e2[3];                               // [esp+0x1c] / [esp+0x40]: edge +0xc
            if (a < b) span(a, b); else span(b, a);                       // 0x0049325f jge
        }
        if (kEdgeAdvance(e1) == 0) return;
        if (kEdgeAdvance(e2) == 0) return;
        const int row = G<int>(0x0083d384) + 1;
        G<int>(0x0083d384) = row;
        if (row >= G<int>(0x0083d390)) return;                            // 0x004932a6 jl
    }
}

// 0x00493100  fillConvexPoly8(holder, verts, count, colour): returns 0x10 when verts (0x0049310b) or holder
// (0x00493117) is null. Else the 8-bit base is slot +0x18 of holder+4 (0x0049312a; 0 when holder+4 is null), stored
// to 0x0083d380 (0x00493131); a null base skips to the release. Otherwise the vertex table, count and colour (whole
// dword) go to 0x0083d358 / 0x0083d378 / 0x0083d34c (0x00493144..0x00493150), the stride (slot +0xe0, 0 for a null
// Surface) to 0x0083d388 (0x00493169), then the shared scan with fillSpanUp 0x00493080. Finally slot +0x24 (1) of
// holder+4 when non-null (0x004932b9) and returns 0 (0x004932bd).
typedef int(__cdecl* Poly_t)(void* holder, const int* verts, int count, int colour);
Poly_t Poly8_orig;
int __cdecl Poly8_re(void* holder, const int* verts, int count, int colour) {
    if (verts == 0 || holder == 0) return 0x10;
    void* s = Pp(holder, 4);
    void* base = 0;
    if (s) base = vslot<V0p_t>(s, 0x18)(s, 0);
    G<void*>(0x0083d380) = base;
    if (base != 0) {
        G<const int*>(0x0083d358) = verts;
        G<int>(0x0083d378) = count;
        G<int>(0x0083d34c) = colour;
        s = Pp(holder, 4);
        int pitch = 0;
        if (s) pitch = pitchOf(s);
        G<int>(0x0083d388) = pitch;
        scanPolygon(holder, verts, count, static_cast<int>(reinterpret_cast<unsigned>(verts)), kSpanUp);
    }
    s = Pp(holder, 4);
    if (s) releaseBits(s);
    return 0;
}

// 0x004932d0  fillConvexPoly16(holder, verts, count, colour): returns 0x10 when verts (0x004932dd) or holder
// (0x004932e9) is null. The 16-bit base is slot +0x20 of holder+4 (0x004932fc), stored to 0x0083d37c (0x00493301);
// null skips to the release. The colour goes through the shared 16-bit lookup (0x00493310..0x00493373), except that
// an unknown format returns 1 at once WITHOUT the release (0x0049333e je / 0x00493343). Then 0x0083d358 = verts
// (0x00493379), 0x0083d378 = count (0x00493389), 0x0083d34c = colour & 0xffff (0x00493383 / 0x0049338f), 0x0083d388
// = stride * 2 (0x004933a8 add eax, eax), the shared scan with fillSpanDown 0x00493000, release (0x004934f8),
// returns 0.
Poly_t Poly16_orig;
int __cdecl Poly16_re(void* holder, const int* verts, int count, int colour) {
    if (verts == 0 || holder == 0) return 0x10;
    void* s = Pp(holder, 4);
    void* base = 0;
    if (s) base = vslot<V0p_t>(s, 0x20)(s, 0);
    G<void*>(0x0083d37c) = base;
    if (base != 0) {
        unsigned c = static_cast<unsigned>(colour);
        if (!lookupColor16(Pp(holder, 4), c)) return 1;
        G<const int*>(0x0083d358) = verts;
        G<int>(0x0083d378) = count;
        G<unsigned>(0x0083d34c) = c & 0xffff;
        s = Pp(holder, 4);
        int pitch = 0;
        if (s) pitch = pitchOf(s);
        G<int>(0x0083d388) = static_cast<int>(static_cast<unsigned>(pitch) * 2u);
        scanPolygon(holder, verts, count, static_cast<int>(reinterpret_cast<unsigned>(verts)), kSpanDown);
    }
    s = Pp(holder, 4);
    if (s) releaseBits(s);
    return 0;
}

// 0x00493520  fillConvexPoly(holder, verts, count, colour): the depth of holder+4 (slot +0xe4, 0x0049352e; a null
// Surface reads address 0) picks fillConvexPoly8 0x00493100 for 8 (0x0049353d je, 0x00493575) or fillConvexPoly16
// 0x004932d0 for 16 (0x00493542 je, 0x0049355b), returning its result; any other depth returns 0x17 (0x00493544).
const Poly_t kPoly8 = reinterpret_cast<Poly_t>(0x00493100);
const Poly_t kPoly16 = reinterpret_cast<Poly_t>(0x004932d0);
Poly_t Poly_orig;
int __cdecl Poly_re(void* holder, const int* verts, int count, int colour) {
    const int depth = *depthOrNull(Pp(holder, 4));
    if (depth == 8) return kPoly8(holder, verts, count, colour);
    if (depth == 16) return kPoly16(holder, verts, count, colour);
    return 0x17;
}

// ---- window array --------------------------------------------------------------------------------------------
// 0x0047e520  listRemove(obj): removes obj from the pointer array at 0x0083a2d8 whose count is 0x0083ab94. Returns
// when obj is null (0x0047e527) or the count is <= 0 (0x0047e532 jle). Scans from index 0 (0x0047e539..0x0047e543);
// not found -> returns (0x0047e545). Found at i: count - 1 is stored (0x0047e54c), obj's slot +8 is called with no
// stack arguments (0x0047e553), then entries i+1 .. count (the new count, re-read at 0x0047e556) move down one place
// (0x0047e569..0x0047e572) when i < count (0x0047e55e jge).
typedef void(__cdecl* Remove_t)(void* obj);
Remove_t ListRemove_orig;
void __cdecl ListRemove_re(void* obj) {
    if (obj == 0) return;
    const int n = G<int>(0x0083ab94);
    if (n <= 0) return;
    void** arr = reinterpret_cast<void**>(0x0083a2d8);
    int i = 0;
    while (arr[i] != obj) {
        if (++i >= n) return;
    }
    G<int>(0x0083ab94) = n - 1;
    vslot<V0i_t>(obj, 8)(obj, 0);
    const int c = G<int>(0x0083ab94);
    for (int k = i; k < c; k++) arr[k] = arr[k + 1];
}

// ---- TextView ------------------------------------------------------------------------------------------------
// 0x0048e0b0  TextView::setLineSpacing(this, v): stores into the dword at this + 0x1498 + [[this+0x1464] + 8]
// (0x0048e0f6 / 0x0048e10e). The value is v, except when [this+0x590] is 0 (0x0048e0bb), bit 0x400 of
// [this+0x1f68] is clear (0x0048e0c3 `test ah, 4`), the display [0x0083ad50] is non-null (0x0048e0d0) and its slot
// +0xa8 returns >= 0x400 (0x0048e0da cmp, signed jl): then it is (v * 3) / 2, the product wrapping to 32 bits
// (0x0048e0eb lea) and the halving rounding toward zero (cdq / sub / sar at 0x0048e0ee..0x0048e0f4).
typedef void(__fastcall* Set1_t)(void* self, void* edx, int v);
Set1_t LineSpacing_orig;
void __fastcall LineSpacing_re(void* self, void*, int v) {
    int val = v;
    if (I(self, 0x590) == 0 && (I(self, 0x1f68) & 0x400) == 0) {
        void* disp = G<void*>(0x0083ad50);
        if (disp && vslot<V0i_t>(disp, 0xa8)(disp, 0) >= 0x400)
            val = static_cast<int>(static_cast<unsigned>(v) * 3u) / 2;
    }
    const int idx = I(Pp(self, 0x1464), 8);
    I(self, 0x1498 + idx) = val;
}

}  // namespace

// 0x00485d40  Image::hline
SG_HOOK("golf_clean.exe", 0x00485d40, Image__hline, HLine8_re, HLine8_orig);
// 0x00485e80  Image::vline
SG_HOOK("golf_clean.exe", 0x00485e80, Image__vline, VLine8_re, VLine8_orig);
// 0x00494d30  drawColorRunA
SG_HOOK("golf_clean.exe", 0x00494d30, drawColorRunA, RunA_re, RunA_orig);
// 0x00494f00  drawColorRunB
SG_HOOK("golf_clean.exe", 0x00494f00, drawColorRunB, RunB_re, RunB_orig);
// 0x00478c10  Widget_drawHLineDepth
SG_HOOK("golf_clean.exe", 0x00478c10, Widget_drawHLineDepth, HLineDepth_re, HLineDepth_orig);
// 0x00478c70  Widget_drawVLineDepth
SG_HOOK("golf_clean.exe", 0x00478c70, Widget_drawVLineDepth, VLineDepth_re, VLineDepth_orig);
// 0x00479610  Widget_drawBox
SG_HOOK("golf_clean.exe", 0x00479610, Widget_drawBox, Box_re, Box_orig);
// 0x00479670  Widget_drawBoxR
SG_HOOK("golf_clean.exe", 0x00479670, Widget_drawBoxR, BoxR_re, BoxR_orig);
// 0x00479560  Widget_drawBevelBox
SG_HOOK("golf_clean.exe", 0x00479560, Widget_drawBevelBox, Bevel_re, Bevel_orig);
// 0x004795d0  Widget_drawBevelBoxR
SG_HOOK("golf_clean.exe", 0x004795d0, Widget_drawBevelBoxR, BevelR_re, BevelR_orig);
// 0x00478af0  Win_fillRect
SG_HOOK("golf_clean.exe", 0x00478af0, Win_fillRect, WinFill_re, WinFill_orig);
// 0x00480b00  fillWindowRect
SG_HOOK("golf_clean.exe", 0x00480b00, fillWindowRect, WindowFill_re, WindowFill_orig);
// 0x00478df0  drawLineH
SG_HOOK("golf_clean.exe", 0x00478df0, drawLineH, GetPixel_re, GetPixel_orig);
// 0x00479830  blendRed
SG_HOOK("golf_clean.exe", 0x00479830, blendRed, Red_re, Red_orig);
// 0x004798c0  blendGreen
SG_HOOK("golf_clean.exe", 0x004798c0, blendGreen, Green_re, Green_orig);
// 0x00479950  drawPixelRun
SG_HOOK("golf_clean.exe", 0x00479950, drawPixelRun, Blue_re, Blue_orig);
// 0x00493100  fillConvexPoly8
SG_HOOK("golf_clean.exe", 0x00493100, fillConvexPoly8, Poly8_re, Poly8_orig);
// 0x004932d0  fillConvexPoly16
SG_HOOK("golf_clean.exe", 0x004932d0, fillConvexPoly16, Poly16_re, Poly16_orig);
// 0x00493520  fillConvexPoly
SG_HOOK("golf_clean.exe", 0x00493520, fillConvexPoly, Poly_re, Poly_orig);
// 0x0047e520  listRemove
SG_HOOK("golf_clean.exe", 0x0047e520, listRemove, ListRemove_re, ListRemove_orig);
// 0x0048e0b0  TextView::setLineSpacing
SG_HOOK("golf_clean.exe", 0x0048e0b0, TextView__setLineSpacing, LineSpacing_re, LineSpacing_orig);

// C3 batch c3t (2026-10-07) of golf_clean.exe: render functions the test scenarios never reach but that have static
// callers: the polygon span fillers and edge stepper, a 16-bit run filler, an isometric coordinate map, and five thin
// wrappers that forward to a drawing object's virtual methods. Hand-written from the disassembly
// (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr> --list) and the C2 transcriptions; each body cites the
// address of every global, offset and callee it uses. __thiscall is emulated with __fastcall (ecx = this, edx unused),
// and so are the virtual methods reached through an object's vtable (thiscall, callee pops its stack arguments).
#include "hooks.h"

namespace {

template <typename T> T at(unsigned addr) { return *reinterpret_cast<const T*>(addr); }
template <typename T> T field(const void* p, unsigned off) {
    return *reinterpret_cast<const T*>(reinterpret_cast<const char*>(p) + off);
}
template <typename T> void setField(void* p, unsigned off, T v) {
    *reinterpret_cast<T*>(reinterpret_cast<char*>(p) + off) = v;
}
inline void* vslot(void* obj, unsigned off) {
    return at<void*>(*reinterpret_cast<const unsigned*>(obj) + off);
}
// 32-bit wrapping arithmetic (imul / lea keep the low dword).
inline int mul(int a, int b) { return static_cast<int>(static_cast<unsigned>(a) * static_cast<unsigned>(b)); }
inline int add(int a, int b) { return static_cast<int>(static_cast<unsigned>(a) + static_cast<unsigned>(b)); }
inline int sub(int a, int b) { return static_cast<int>(static_cast<unsigned>(a) - static_cast<unsigned>(b)); }

// Polygon fill context shared by fillSpanDown / fillSpanUp (globals cited where they are read).
const unsigned kFillColor = 0x0083d34c;  // g_polyFillColor: word (16-bit spans), low byte (8-bit spans)
const unsigned kClipMinX = 0x0083d35c;
const unsigned kBase16 = 0x0083d37c;
const unsigned kBase8 = 0x0083d380;
const unsigned kRowY = 0x0083d384;
const unsigned kPitch = 0x0083d388;
const unsigned kClipMaxX = 0x0083d38c;

// Clips the span [x0, x1) of fillSpanDown / fillSpanUp against [min, max) and returns its length, or 0 when nothing
// is drawn. Shared logic of 0x00493000..0x0049304a and 0x00493080..0x004930ca (same instruction sequence):
//   x1 <= min -> nothing (jle at 0x0049300f / 0x0049308f, signed);
//   x0 >= max -> nothing (jge at 0x0049301a / 0x0049309a, signed);
//   x1 <= x0  -> nothing (`sub ebx, ecx; jle` at 0x0049301c / 0x0049309c: signed compare including overflow);
//   x1 >= max -> x1 = max (jl at 0x00493029 / 0x004930a9); x0 < min -> x0 = min (jge at 0x0049303d / 0x004930bd);
//   x1 - x0 <= 0 after clamping -> nothing (`sub ecx, edi; jle` at 0x00493048 / 0x004930c8; reachable only when
//   min >= max).
int clipSpan(int& x0, int x1) {
    const int lo = at<int>(kClipMinX), hi = at<int>(kClipMaxX);
    if (x1 <= lo) return 0;
    if (x0 >= hi) return 0;
    if (x1 <= x0) return 0;
    if (x1 >= hi) x1 = hi;
    if (x0 < lo) x0 = lo;
    if (x1 <= x0) return 0;
    return sub(x1, x0);
}

// 0x00493000  fillSpanDown(x0, x1): fills the 16-bit pixels x0 .. x1-1 of the current polygon row with the word at
// 0x0083d34c, after clipSpan. The row address is the low dword of [0x0083d384] * [0x0083d388] (imul at 0x00493051)
// plus 2 * x0 (two `add eax, [ebp+8]` at 0x00493057 / 0x0049305a) plus the 16-bit base [0x0083d37c] (0x0049305d);
// the store loop is 0x0049306b..0x00493072. Returns nothing.
typedef void(__cdecl* FillSpan_t)(int, int);
FillSpan_t FillSpanDown_orig;
void __cdecl FillSpanDown_re(int x0, int x1) {
    const int n = clipSpan(x0, x1);
    if (n <= 0) return;
    const int addr = add(add(mul(at<int>(kRowY), at<int>(kPitch)), add(x0, x0)), at<int>(kBase16));
    unsigned short* p = reinterpret_cast<unsigned short*>(addr);
    const unsigned short c = at<unsigned short>(kFillColor);
    for (int i = 0; i < n; i++) p[i] = c;
}

// 0x00493080  fillSpanUp(x0, x1): the 8-bit twin of fillSpanDown: same clipping (clipSpan), row address = low dword of
// [0x0083d384] * [0x0083d388] (0x004930d1) + x0 (0x004930d7) + the 8-bit base [0x0083d380] (0x004930da), filled with
// the byte at 0x0083d34c (0x004930e2) by the loop 0x004930e8..0x004930ec. Returns nothing.
FillSpan_t FillSpanUp_orig;
void __cdecl FillSpanUp_re(int x0, int x1) {
    const int n = clipSpan(x0, x1);
    if (n <= 0) return;
    const int addr = add(add(mul(at<int>(kRowY), at<int>(kPitch)), x0), at<int>(kBase8));
    unsigned char* p = reinterpret_cast<unsigned char*>(addr);
    const unsigned char c = at<unsigned char>(kFillColor);
    for (int i = 0; i < n; i++) p[i] = c;
}

// 0x00493630  fill16(dst, color, count): fills count 16-bit cells at dst with the low word of color. When count is
// odd (test al, 1 / je at 0x00493648) one word is stored first (0x00493650) and dst advances by 2 (0x00493653); then
// count / 2 (cdq / sub / sar: rounds toward zero, 0x0049365c..0x0049365f) dwords of (color << 16 | color) are stored
// with rep stosd when that quotient is > 0 (jle at 0x00493663). A negative odd count still stores the single word.
typedef void(__cdecl* Fill16_t)(unsigned short*, unsigned, int);
Fill16_t Fill16_orig;
void __cdecl Fill16_re(unsigned short* dst, unsigned color, int count) {
    const unsigned c = color & 0xffff;
    if (count & 1) *dst++ = static_cast<unsigned short>(c);
    const int pairs = count / 2;
    unsigned* d = reinterpret_cast<unsigned*>(dst);
    for (int i = 0; i < pairs; i++) d[i] = (c << 16) | c;
}

// 0x00456b70  mapToScreen456b70(a, b, outX, outY): *outX = 6 * (a + b) + 0x6a (lea chain 0x00456b7d..0x00456b88,
// stored at 0x00456b8c) and then *outY = 3 * (b - a) + 0x1b8 - d, where d = 1 when the dword at 0x00822b80 is non-zero
// and -1 when it is zero (neg / sbb / and 2 / dec at 0x00456b93..0x00456b9f), stored at 0x00456ba8. All arithmetic
// wraps at 32 bits. Returns nothing.
typedef void(__cdecl* MapToScreen_t)(int, int, int*, int*);
MapToScreen_t MapToScreen_orig;
void __cdecl MapToScreen_re(int a, int b, int* outX, int* outY) {
    *outX = add(mul(add(a, b), 6), 0x6a);
    const int d = at<int>(0x00822b80) != 0 ? 1 : -1;
    *outY = add(sub(mul(sub(b, a), 3), d), 0x1b8);
}

// 0x00492fa0  polyEdgeAdvance(edge): steps a polygon edge record one row. Fields (all dwords): +4 rows left, +8 the
// edge's end vertex index, +0xc x, +0x10 x step, +0x14 x sign step, +0x18 error term, +0x1c error step, +0x20 error
// reset. It decrements +4 (0x00492fa7..0x00492fa8); when that reaches 0 (jne at 0x00492fab) it calls polyEdgeStep
// 0x00492ed0(edge, [edge+8]) to start the next edge and returns 1 when that returned non-zero, else 0 (neg / sbb / neg
// at 0x00492fba..0x00492fbe). Otherwise x += x step (0x00492fc4..0x00492fcd) and error += error step
// (0x00492fca..0x00492fd9); when the new error is > 0 (signed jle at 0x00492fde) x += sign step (0x00492fe0..0x00492fea)
// and error -= error reset (0x00492fe5..0x00492fed). Returns 1 (0x00492ff0).
typedef int(__cdecl* PolyEdgeAdvance_t)(void*);
typedef int(__cdecl* PolyEdgeStep_t)(void*, int);
const PolyEdgeStep_t kPolyEdgeStep = reinterpret_cast<PolyEdgeStep_t>(0x00492ed0);
PolyEdgeAdvance_t PolyEdgeAdvance_orig;
int __cdecl PolyEdgeAdvance_re(void* e) {
    const int rows = sub(field<int>(e, 4), 1);
    setField<int>(e, 4, rows);
    if (rows == 0) return kPolyEdgeStep(e, field<int>(e, 8)) != 0 ? 1 : 0;
    const int x = add(field<int>(e, 0xc), field<int>(e, 0x10));
    setField<int>(e, 0xc, x);
    const int err = add(field<int>(e, 0x18), field<int>(e, 0x1c));
    setField<int>(e, 0x18, err);
    if (err > 0) {
        setField<int>(e, 0xc, add(field<int>(e, 0x14), x));
        setField<int>(e, 0x18, sub(err, field<int>(e, 0x20)));
    }
    return 1;
}

// 0x00478bb0  Widget_drawHLine(this, a1, a2, a3, a4): when the drawing object at this+4 is non-null (je at
// 0x00478bb5) calls its virtual at vtable +0x64 (0x00478bd1) with the six stack arguments (a1, a3, a2, a3, a4, 1)
// (pushes 0x00478bc2..0x00478bd0): a line from (a1, a3) to (a2, a3). Returns nothing (ret 0x10).
typedef void(__fastcall* DrawLine6_t)(void*, void*, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned);
typedef void(__fastcall* WidgetLine_t)(void*, void*, unsigned, unsigned, unsigned, unsigned);
WidgetLine_t WidgetHLine_orig;
void __fastcall WidgetHLine_re(void* self, void*, unsigned a1, unsigned a2, unsigned a3, unsigned a4) {
    void* s = field<void*>(self, 4);
    if (s == 0) return;
    reinterpret_cast<DrawLine6_t>(vslot(s, 0x64))(s, 0, a1, a3, a2, a3, a4, 1);
}

// 0x00478be0  Widget_drawVLine(this, a1, a2, a3, a4): the vertical twin of Widget_drawHLine: when this+4 is non-null
// (je at 0x00478be5) calls its virtual at +0x64 (0x00478c01) with (a1, a2, a1, a3, a4, 1) (pushes
// 0x00478bf2..0x00478c00): a line from (a1, a2) to (a1, a3). Returns nothing (ret 0x10).
WidgetLine_t WidgetVLine_orig;
void __fastcall WidgetVLine_re(void* self, void*, unsigned a1, unsigned a2, unsigned a3, unsigned a4) {
    void* s = field<void*>(self, 4);
    if (s == 0) return;
    reinterpret_cast<DrawLine6_t>(vslot(s, 0x64))(s, 0, a1, a2, a1, a3, a4, 1);
}

// 0x004838f0  Stream4838f0::puts(this, text): returns 0 when the stream object at this+4 is null (je at 0x004838f6) or
// text is null (je at 0x004838fe); otherwise returns that object's virtual at vtable +0x10 (0x00483914) called with
// (text, strlen(text)) (repne scasb length at 0x00483904..0x0048390f).
typedef int(__fastcall* Write2_t)(void*, void*, const char*, unsigned);
typedef int(__fastcall* Puts_t)(void*, void*, const char*);
Puts_t Puts_orig;
int __fastcall Puts_re(void* self, void*, const char* text) {
    void* d = field<void*>(self, 4);
    if (d == 0 || text == 0) return 0;
    unsigned len = 0;
    while (text[len] != 0) len++;
    return reinterpret_cast<Write2_t>(vslot(d, 0x10))(d, 0, text, len);
}

// Virtual methods of the surface at obj+4 used by Surface::clear / Surface::copyRaw: +0x20 returns the pixel buffer,
// +0x34 and +0x30 return the two dimensions whose product is the byte count, +0x24 takes one argument (1).
typedef unsigned(__fastcall* Virt0_t)(void*, void*);
typedef unsigned(__fastcall* Virt1_t)(void*, void*, unsigned);
unsigned vcall0(void* obj, unsigned off) { return reinterpret_cast<Virt0_t>(vslot(obj, off))(obj, 0); }

// 0x00482940  Surface::clear(a1, a2, obj) (__stdcall, ret 0xc; ecx and a1/a2 are not read): with s = obj+4 (no null
// test), takes the buffer from s's virtual +0x20 (0x0048294d), then n = virtual +0x34 * virtual +0x30 (0x00482959,
// 0x00482962, imul at 0x00482965), zeroes n bytes of the buffer (rep stosd of n >> 2 dwords then rep stosb of n & 3
// bytes, 0x00482970..0x0048297a; n taken unsigned), calls virtual +0x24 with 1 on obj+4 re-read (0x0048297c..0x00482981)
// and returns 0 (0x00482987).
typedef int(__stdcall* SurfClear_t)(int, int, void*);
SurfClear_t SurfClear_orig;
int __stdcall SurfClear_re(int, int, void* obj) {
    void* s = field<void*>(obj, 4);
    unsigned char* buf = reinterpret_cast<unsigned char*>(vcall0(s, 0x20));
    s = field<void*>(obj, 4);
    const unsigned a = vcall0(s, 0x34);
    const unsigned b = vcall0(s, 0x30);
    const unsigned n = a * b;
    for (unsigned i = 0; i < n; i++) buf[i] = 0;
    s = field<void*>(obj, 4);
    reinterpret_cast<Virt1_t>(vslot(s, 0x24))(s, 0, 1);
    return 0;
}

// 0x00482a80  Surface::copyRaw(src, a2, obj) (__stdcall, ret 0xc; ecx and a2 are not read): same sequence as
// Surface::clear (virtual +0x20 buffer at 0x00482a8d, +0x34 * +0x30 byte count at 0x00482a99 / 0x00482aa2 / 0x00482aa5,
// virtual +0x24 with 1 at 0x00482ac6, returns 0 at 0x00482acc) but copies n bytes from src + 6 (lea esi, [edx+6] at
// 0x00482ab2) into the buffer (rep movsd / rep movsb, 0x00482ab5..0x00482abf).
typedef int(__stdcall* SurfCopy_t)(const unsigned char*, int, void*);
SurfCopy_t SurfCopy_orig;
int __stdcall SurfCopy_re(const unsigned char* src, int, void* obj) {
    void* s = field<void*>(obj, 4);
    unsigned char* buf = reinterpret_cast<unsigned char*>(vcall0(s, 0x20));
    s = field<void*>(obj, 4);
    const unsigned a = vcall0(s, 0x34);
    const unsigned b = vcall0(s, 0x30);
    const unsigned n = a * b;
    const unsigned char* from = src + 6;
    for (unsigned i = 0; i < n; i++) buf[i] = from[i];
    s = field<void*>(obj, 4);
    reinterpret_cast<Virt1_t>(vslot(s, 0x24))(s, 0, 1);
    return 0;
}

// 0x00475da0  Surface_fill(this, a1, a2, a3, a4, a5): returns 7 when the drawing object at this+4 is null (jne at
// 0x00475da8, 0x00475daa); otherwise builds the 16-byte local rectangle {a1, a2, a3, a2} (stores 0x00475dbd..0x00475dd2;
// a4 is not read) and returns the object's virtual at vtable +0x5c (0x00475ddd) called with (&rect, a5).
typedef int(__fastcall* Fill2_t)(void*, void*, const int*, int);
typedef int(__fastcall* SurfFill_t)(void*, void*, int, int, int, int, int);
SurfFill_t SurfFill_orig;
int __fastcall SurfFill_re(void* self, void*, int a1, int a2, int a3, int, int a5) {
    void* s = field<void*>(self, 4);
    if (s == 0) return 7;
    const int rect[4] = {a1, a2, a3, a2};
    return reinterpret_cast<Fill2_t>(vslot(s, 0x5c))(s, 0, rect, a5);
}

}  // namespace

SG_HOOK("golf_clean.exe", 0x00493000, fillSpanDown, FillSpanDown_re, FillSpanDown_orig);
SG_HOOK("golf_clean.exe", 0x00493080, fillSpanUp, FillSpanUp_re, FillSpanUp_orig);
SG_HOOK("golf_clean.exe", 0x00493630, fill16, Fill16_re, Fill16_orig);
SG_HOOK("golf_clean.exe", 0x00456b70, mapToScreen456b70, MapToScreen_re, MapToScreen_orig);
SG_HOOK("golf_clean.exe", 0x00492fa0, polyEdgeAdvance, PolyEdgeAdvance_re, PolyEdgeAdvance_orig);
SG_HOOK("golf_clean.exe", 0x00478bb0, Widget_drawHLine, WidgetHLine_re, WidgetHLine_orig);
SG_HOOK("golf_clean.exe", 0x00478be0, Widget_drawVLine, WidgetVLine_re, WidgetVLine_orig);
SG_HOOK("golf_clean.exe", 0x004838f0, Stream4838f0_puts, Puts_re, Puts_orig);
SG_HOOK("golf_clean.exe", 0x00482940, Surface_clear, SurfClear_re, SurfClear_orig);
SG_HOOK("golf_clean.exe", 0x00482a80, Surface_copyRaw, SurfCopy_re, SurfCopy_orig);
SG_HOOK("golf_clean.exe", 0x00475da0, Surface_fill, SurfFill_re, SurfFill_orig);

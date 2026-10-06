// C3 batch c3l (2026-10-06): ui/util leaves, two global writers and a slot hit-test, hand-written from the
// C2 transcriptions re/analysis/<subsystem>/<addr>_<name>.md and the disassembly (re/tools/asm2inline.py).
// Each body states the facts it relies on with their addresses. __thiscall is emulated with __fastcall
// (ecx = this) where needed; these functions are all __cdecl. Callees are reached through their original
// addresses (a hooked callee runs its reimplementation on both arms, so the A/B stays consistent).
#include "hooks.h"

namespace {

typedef int(__cdecl* Int2_t)(int, int);
typedef int(__cdecl* Int1v_t)(void*, int, int);

// approxDistance (0x00467170, reimplemented in golf_math.cpp) is the only callee here.
const Int2_t kApproxDistance = reinterpret_cast<Int2_t>(0x00467170);

// 0x004671a0  direction8(a, b): classifies the vector (a = first arg at [esp+4], b = second at [esp+8])
// into one of 8 octants. Four sign blocks (0x4671b2 a>0 b>0, 0x4671e3 a>0 b<=0, 0x467211 a<=0 b>0,
// 0x46723e a<=0 b<=0) each compute A=|a|, B=|b| and pick an octant: a default (edi), an alternate when
// |a| > 2|b| (0x4671ca), and a straight-axis value when |b| > 2|a| (0x4671d5). The block constants are
// {def,alt,straight} = {3,2,4} / {1,2,0} / {5,6,4} / {7,6,0} (0x4671b9..0x467258).
Int2_t Direction8_orig;
int __cdecl Direction8_re(int a, int b) {
    const int A = a < 0 ? -a : a;
    const int B = b < 0 ? -b : b;
    int def, alt, straight;
    if (a > 0) {
        if (b > 0) { def = 3; alt = 2; straight = 4; }
        else       { def = 1; alt = 2; straight = 0; }
    } else {
        if (b > 0) { def = 5; alt = 6; straight = 4; }
        else       { def = 7; alt = 6; straight = 0; }
    }
    int r = def;
    if (A > 2 * B) r = alt;        // lea edx,[ecx+ecx]; cmp eax,edx; jle (0x4671c8)
    if (B > 2 * A) return straight; // add eax,eax; cmp ecx,eax; jle 0x467268 (else return straight)
    return r;
}

// 0x004672d0  angleFixed(a, b): 16.16 fixed-point atan2-style angle of the vector (x = a at [esp+4],
// ebx = -b where b is at [esp+8], 0x4672da). Axis cases: x == 0 returns 0x80000000 when -b <= 0 else 0
// (0x4672e0); -b == 0 returns 0xc0000000 when x <= 0 else 0x40000000 (0x4672f9). Otherwise it divides the
// smaller magnitude (<< 14) by the larger (0x46733c), with edi = 1 when |x| > |-b|; evaluates a cubic-ish
// approximation d = |0x1333 - t|, p = 0x2800 - ((11*d) << 8 >> 14), c = (p*t) >> 14 (0x467340..0x467362);
// then adds a per-quadrant base 0x0000/0x4000/0x8000/0xc000 (0x467372..0x4673d1) and returns the result
// shifted left 16 (shl eax,0x10).
Int2_t AngleFixed_orig;
int __cdecl AngleFixed_re(int a, int b) {
    const int x = a;
    const int nb = -b;
    if (x == 0) return static_cast<int>((nb <= 0) ? 0x80000000u : 0u);
    if (nb == 0) return static_cast<int>((x <= 0) ? 0xc0000000u : 0x40000000u);
    const int ax = x < 0 ? -x : x;
    const int anb = nb < 0 ? -nb : nb;
    int num, den, edi;
    if (ax > anb) { num = anb << 14; den = ax;  edi = 1; }
    else          { num = ax << 14;  den = anb; edi = 0; }
    const int t = num / den;
    int d = 0x1333 - t; if (d < 0) d = -d;
    int e = (11 * d) << 8; e >>= 14;
    const int p = 0x2800 - e;
    const int c = (p * t) >> 14;
    int r;
    if (x > 0) {
        if (nb > 0) r = (edi == 0) ? c : (0x4000 - c);
        else        r = (edi != 0) ? (c + 0x4000) : (0x8000 - c);
    } else {
        if (nb > 0) r = (edi != 0) ? (c + 0xc000) : (-c);
        else        r = (edi != 0) ? (0xc000 - c) : (c + 0x8000);
    }
    return static_cast<int>(static_cast<unsigned>(r) << 16);
}

// 0x00491c70  sinScaled(angle, amp): fixed-point sine. Negates amp when angle's sign bit is set (0x491c78);
// index = (angle & 0x3fffffff) >> 22 (0x491c85..0x491c93) selects a pair of adjacent table entries at
// 0x0083b9f4 and 0x0083b9f8 (= 0x0083b9f4[index+1]); base = ((tabB - tabA) * (angle & 0x3fffff)) >> 22
// (0x491cb0/0x491ccd), then +tabA, or -tabA+0xffff when bit 30 is set (0x491c8b/0x491cbe). amp is pre-shifted
// to keep the final multiply in 32 bits, with signed thresholds: amp < 0xffff multiplies then >> 16
// (cmp 0xffff/jge 0x491c9c, final 0x491cc9); 0xffff..0xfffffe uses amp >> 8 then >> 8 (cmp 0xffffff/jge
// 0x491ceb, 0x491ced/0x491d1b); >= 0xffffff uses amp >> 16 with no final shift (0x491d37).
Int2_t SinScaled_orig;
int __cdecl SinScaled_re(int angle, int amp) {
    int m = (angle & 0x80000000u) ? -amp : amp;
    const int index = (angle & 0x3fffffff) >> 22;
    const int frac = angle & 0x3fffff;
    const int tabA = *reinterpret_cast<const int*>(0x0083b9f4 + index * 4);
    const int tabB = *reinterpret_cast<const int*>(0x0083b9f8 + index * 4);
    const int base = ((tabB - tabA) * frac) >> 22;
    int s;
    if (m >= 0xffff) {
        if (m >= 0xffffff) { m >>= 16; s = 0; }
        else               { m >>= 8;  s = 8; }
    } else {
        s = 16;
    }
    const int v = (angle & 0x40000000) ? (base - tabA + 0xffff) : (base + tabA);
    const int r = v * m;
    return s ? (r >> s) : r;
}

// 0x00476d40  scanMarkupText(s, lenp): scans s for the first markup delimiter ({ } [ ] $, bytes 0x7b 0x7d
// 0x5b 0x5d 0x24, 0x476d52..0x476d66), advancing while the remaining count *lenp is non-zero (0x476d6c).
// On a delimiter or when the count reaches 0 it writes the remaining count back to *lenp (0x476d6f) and
// returns the scan pointer; a starting count of 0 returns s unchanged without writing (0x476d4e).
typedef char*(__cdecl* Scan_t)(char*, int*);
Scan_t ScanMarkupText_orig;
char* __cdecl ScanMarkupText_re(char* s, int* lenp) {
    int rem = *lenp;
    char* p = s;
    if (rem != 0) {
        for (;;) {
            const unsigned char c = static_cast<unsigned char>(*p);
            if (c == 0x7b || c == 0x7d || c == 0x5b || c == 0x5d || c == 0x24) break;
            ++p;
            if (--rem == 0) break;
        }
        *lenp = rem;
        s = p;
    }
    return s;
}

// 0x004889f0  setButtonDefaults(obj, font, colour): stores the default button font/colour globals. Returns 3
// when obj is NULL (0x4889f6); otherwise stores obj into 0x0083b60c only when *(obj+4) is non-zero
// (0x488a03), then stores font into 0x0083b610 and colour into 0x0083b614 (0x488a12/0x488a17), and returns 0.
typedef int(__cdecl* SetBtn_t)(void*, int, int);
SetBtn_t SetButtonDefaults_orig;
int __cdecl SetButtonDefaults_re(void* obj, int font, int colour) {
    if (obj == 0) return 3;
    if (*reinterpret_cast<const int*>(reinterpret_cast<const char*>(obj) + 4) != 0)
        *reinterpret_cast<void**>(0x0083b60c) = obj;
    *reinterpret_cast<int*>(0x0083b610) = font;
    *reinterpret_cast<int*>(0x0083b614) = colour;
    return 0;
}

// 0x00490cf0  MsgBox::setButtonB(obj, cb, ctx): stores the message-box button-B object/callbacks globals.
// Same shape as setButtonDefaults: returns 3 on a NULL obj (0x490cf6); stores obj into 0x0083b9c4 only when
// *(obj+4) is non-zero (0x490d03); then cb into 0x0083b9c8 and ctx into 0x0083b9cc (0x490d12/0x490d17); 0.
SetBtn_t MsgBoxSetButtonB_orig;
int __cdecl MsgBoxSetButtonB_re(void* obj, int cb, int ctx) {
    if (obj == 0) return 3;
    if (*reinterpret_cast<const int*>(reinterpret_cast<const char*>(obj) + 4) != 0)
        *reinterpret_cast<void**>(0x0083b9c4) = obj;
    *reinterpret_cast<int*>(0x0083b9c8) = cb;
    *reinterpret_cast<int*>(0x0083b9cc) = ctx;
    return 0;
}

// 0x00432f90  hitTestSlot432f90(x, y): returns the toolbar slot/button index under screen point (x, y), or a
// hotspot code, or -1. Two fixed hotspots first (ebx default -1, 0x432fa2): approxDistance(x-0xed, y-0x20d)
// < 0x14 sets ebx = 0x16 (0x432fb7), approxDistance(x-0x105, y-0x242) < 0xf sets ebx = 0x15 (0x432fd7).
// Then it walks slots i = 0.. : the x range is [xmin, xmin+0x3e) with xmin = (short)[0x004c79b4 + i*4]
// (0x432feb, 0x432ff8); top = (int)[0x00570cd4 + i*0xb0] - 0xa + (short)[0x004c79b6 + i*4]
// (0x432ff1/0x432ff2/0x433002); the y range is [top-0x2c, top) (0x433004..0x433011). The first slot whose
// range contains (x, y) returns i (0x433029). The walk stops when (unsigned char)[0x004c79a1 + i] == 0xff
// (0x433023), returning ebx.
Int2_t HitTestSlot432f90_orig;
int __cdecl HitTestSlot432f90_re(int x, int y) {
    int ebx = -1;
    if (kApproxDistance(x - 0xed, y - 0x20d) < 0x14) ebx = 0x16;
    if (kApproxDistance(x - 0x105, y - 0x242) < 0xf) ebx = 0x15;
    const short* const A = reinterpret_cast<const short*>(0x004c79b4);
    const unsigned char* const term = reinterpret_cast<const unsigned char*>(0x004c79a1);
    for (int i = 0;; ++i) {
        const int xmin = A[i * 2];
        if (x >= xmin && x < xmin + 0x3e) {
            const int base = *reinterpret_cast<const int*>(0x00570cd4 + i * 0xb0) - 0xa;
            const int top = base + A[i * 2 + 1];
            if (y >= top - 0x2c && y < top) return i;
        }
        if (term[i] == 0xff) return ebx;
    }
}

}  // namespace

SG_HOOK("golf_clean.exe", 0x004671a0, direction8, Direction8_re, Direction8_orig);
SG_HOOK("golf_clean.exe", 0x004672d0, angleFixed, AngleFixed_re, AngleFixed_orig);
SG_HOOK("golf_clean.exe", 0x00491c70, sinScaled, SinScaled_re, SinScaled_orig);
SG_HOOK("golf_clean.exe", 0x00476d40, scanMarkupText, ScanMarkupText_re, ScanMarkupText_orig);
SG_HOOK("golf_clean.exe", 0x004889f0, setButtonDefaults, SetButtonDefaults_re, SetButtonDefaults_orig);
SG_HOOK("golf_clean.exe", 0x00490cf0, MsgBoxSetButtonB, MsgBoxSetButtonB_re, MsgBoxSetButtonB_orig);
SG_HOOK("golf_clean.exe", 0x00432f90, hitTestSlot432f90, HitTestSlot432f90_re, HitTestSlot432f90_orig);

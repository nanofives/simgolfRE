// C3 batch c3fp (round 35, 2026-10-14) of sound.dll: renderChannelStreamed 0x10036660 alone (log/c3/c3fp_candidates.tsv,
// log/c3/round35_brief.md "c3fp"). c3fl's probe (log/c3/c3fl_notes.txt section 1) saw no game thread enter it in 60 s at the
// menu; the fixtures still freeze the first thread and hold the multimedia timer off (c3fl's method) because they swap
// g_soundEngine 0x100b4a20 and stub thunks the timer thread runs.
// Addresses in SG_HOOK and in the `// 0x...` comment line are RVAs; instruction addresses inside the body are VAs
// (VA = 0x10000000 + RVA, as `asm2inline.py sound.dll 0x10036660 --list` prints them). Hand-written from that listing; the
// esp frame is mapped by hand in log/c3/c3fp_notes.txt section 1 (S = esp after the prologue; locals S+0x13 .. S+0x38).
//
// RULES (c3fl): every callee through the incremental-link thunk the original calls (the target is named in the comment);
// image addresses from the loaded base (sound.dll is relocated); __thiscall emulated with __fastcall (ecx = this, edx unused).
// The x87 fade tail (0x10036ac0-0x10036c5a) runs under the CALLER's control word (the function loads none): every x87
// operation is one instruction on X80 operands (c3fh's method, shim/src/re/c3fh.cpp), so a key at 0x027f and a key at 0x0f7f
// (what the Voice_mix* leaves leave on the timer thread) both compute what the original computes.
// J(va, c): c is the condition under which the jcc at va JUMPS; the coverage build (c3fp_cov.cpp) counts both outcomes.
// NEG(k): wrong-body variant k of the negative-control build (c3fp_negctl.cpp), dormant until a fixture arms the switch.
#include "hooks.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef C3FP_COV
extern "C" __declspec(dllexport) volatile unsigned C3fpCovTable[3 * 256] = {0};
static bool c3fpCov(unsigned va, bool c) {
    for (int i = 0; i < 256; ++i) {
        if (C3fpCovTable[3 * i] == va || C3fpCovTable[3 * i] == 0) {
            C3fpCovTable[3 * i] = va;
            C3fpCovTable[3 * i + (c ? 1 : 2)] += 1;
            break;
        }
    }
    return c;
}
#define J(va, c) c3fpCov((va), (c) ? true : false)
#else
#define J(va, c) ((c) ? true : false)
#endif

#ifdef C3FP_NEGCTL
extern "C" __declspec(dllexport) volatile int C3fpNegCtl[4] = {0, 0x5147454e, 0, 0};   // word 1 = magic, word 2 = variant
static int c3fpNegVar() {
    static int v = -1;
    if (v < 0) {
        char b[16] = {0};
        DWORD n = GetEnvironmentVariableA("C3FP_NEGVAR", b, sizeof b);
        v = (n && n < sizeof b) ? atoi(b) : 1;
        C3fpNegCtl[2] = v;
    }
    return v;
}
#define NEG(k) (C3fpNegCtl[0] != 0 && c3fpNegVar() == (k))
#else
#define NEG(k) false
#endif

namespace {

template <typename T> T& fat(void* base, int off) {
    return *reinterpret_cast<T*>(reinterpret_cast<char*>(base) + off);
}
template <typename T> T& fatu(unsigned base, int off) { return *reinterpret_cast<T*>(base + off); }

char* c3fp_sound(unsigned rva) {
    static HMODULE m = 0;
    if (!m) m = GetModuleHandleA("sound.dll");
    return reinterpret_cast<char*>(m) + rva;
}
template <typename F> F c3fp_fn(unsigned rva) { return reinterpret_cast<F>(c3fp_sound(rva)); }

// RVAs
const unsigned kThGetPosition = 0x0000233d;   // -> 0x10010660 DsBuffer::getPosition (thiscall, ret 0xc)
const unsigned kThRestore = 0x0000213f;       // -> 0x100101a0 DsBuffer::restore (thiscall, ret 0x18)
const unsigned kThCloseMmio = 0x00001b18;     // -> 0x10035ca0 closeMmioSound (thiscall, ret 0)
const unsigned kThUnlock = 0x00001ec9;        // -> 0x10010370 DsBuffer::unlock (thiscall, ret 0x10)
const unsigned kThSetVolume = 0x000018f7;     // -> 0x1000f1c0 DsBuffer::setVolume (thiscall, ret 4)
const unsigned kThPost = 0x000011d6;          // -> 0x1002aa90 postSoundThreadMessage (thiscall, ret 0xc)
const unsigned kThAddToList = 0x000023f1;     // -> 0x1000e120 addToSoundList (thiscall, ret 4)
const unsigned kFtol = 0x00042550;            // __ftol (st(0) in, edx:eax out), called directly at 0x10036c5a
const unsigned kSigObj = 0x000b5038;          // the object postSoundThreadMessage is called on (`mov ecx, 0x100b5038`)
const unsigned kGSoundEngine = 0x000b4a20;    // g_soundEngine
const unsigned kK127 = 0x0005b478;            // double 127.0 (bytes 00 00 00 00 00 c0 5f 40)
const unsigned kKZero = 0x0005b498;           // double 0.0

typedef unsigned(__fastcall* GetPos_t)(void*, void*, unsigned*, unsigned*, unsigned);
typedef unsigned(__fastcall* Restore_t)(void*, void*, unsigned, unsigned, unsigned*, unsigned*, unsigned*, unsigned*);
typedef unsigned(__fastcall* Close_t)(void*, void*);
typedef unsigned(__fastcall* Unlock_t)(void*, void*, unsigned, unsigned, unsigned, unsigned);
typedef unsigned(__fastcall* SetVol_t)(void*, void*, unsigned);
typedef unsigned(__fastcall* Post_t)(void*, void*, unsigned, unsigned, unsigned);
typedef unsigned(__fastcall* AddList_t)(void*, void*, unsigned);

// ---------------------------------------------------------------------------------------------------------------------
// X80: one x87 data register (10 bytes), loaded and stored with fld / fstp tbyte (exact); each helper is one x87 operation
// executed under the thread's current control word, so it rounds where and how the original rounds (copied from c3fh.cpp).
// ---------------------------------------------------------------------------------------------------------------------
struct X80 {
    unsigned char b[10];
};
__declspec(noinline) void x80Ld64(X80* r, const void* m) {      // fld qword (a signaling NaN is quieted, as in the original)
    __asm {
        mov eax, m
        mov edx, r
        fld qword ptr [eax]
        fstp tbyte ptr [edx]
    }
}
__declspec(noinline) void x80SubM64(X80* r, const X80* a, const void* m) {   // fsub qword: a - m
    __asm {
        mov eax, a
        mov ecx, m
        mov edx, r
        fld tbyte ptr [eax]
        fsub qword ptr [ecx]
        fstp tbyte ptr [edx]
    }
}
__declspec(noinline) void x80MulM64(X80* r, const X80* a, const void* m) {   // fmul qword: a * m
    __asm {
        mov eax, a
        mov ecx, m
        mov edx, r
        fld tbyte ptr [eax]
        fmul qword ptr [ecx]
        fstp tbyte ptr [edx]
    }
}
__declspec(noinline) void x80Abs(X80* r, const X80* a) {         // fabs
    __asm {
        mov eax, a
        mov edx, r
        fld tbyte ptr [eax]
        fabs
        fstp tbyte ptr [edx]
    }
}
__declspec(noinline) void x80St64(void* m, const X80* a) {      // fst(p) qword: rounds to double under the current control word
    __asm {
        mov eax, a
        mov edx, m
        fld tbyte ptr [eax]
        fstp qword ptr [edx]
    }
}
// fcom / fcomp / fcompp of a against b: the status word fnstsw leaves (C0 0x100: a < b, C3 0x4000: equal, all of C0 C2 C3:
// unordered); the original tests it with `test ah, 0x41` / `0x01` / `0x40`.
__declspec(noinline) unsigned x80Com(const X80* a, const X80* b) {
    unsigned short sw;
    __asm {
        mov eax, a
        mov ecx, b
        fld tbyte ptr [ecx]
        fld tbyte ptr [eax]
        fcompp
        fnstsw sw
    }
    return sw;
}
// __ftol through its address: st(0) in, edx:eax out, st(0) popped.
__declspec(noinline) int64_t x80FtolAt(const X80* a, void* fn) {
    unsigned lo, hi;
    __asm {
        mov eax, a
        fld tbyte ptr [eax]
        call fn
        mov lo, eax
        mov hi, edx
    }
    return static_cast<int64_t>((static_cast<uint64_t>(hi) << 32) | lo);
}
unsigned comM(const X80* a, const void* m) {   // fcom(p) qword: the m64 operand loads exactly
    X80 b;
    x80Ld64(&b, m);
    return x80Com(a, &b);
}

// rep movsd (n >> 2 dwords) then rep movsb (n & 3), forward (DF = 0), as at 0x100367b0-0x100367c0 and the three twins
void c3fp_movs(unsigned d, unsigned s, unsigned n) {
    volatile unsigned* dd = reinterpret_cast<volatile unsigned*>(d);
    const volatile unsigned* ss = reinterpret_cast<const volatile unsigned*>(s);
    for (unsigned i = n >> 2; i; --i) *dd++ = *ss++;
    volatile unsigned char* db = reinterpret_cast<volatile unsigned char*>(const_cast<unsigned*>(dd));
    const volatile unsigned char* sb = reinterpret_cast<const volatile unsigned char*>(ss);
    for (unsigned i = n & 3; i; --i) *db++ = *sb++;
}
// rep stosd / rep stosb of 0 (0x10036a38-0x10036a4c)
void c3fp_stos0(unsigned d, unsigned n) {
    volatile unsigned* dd = reinterpret_cast<volatile unsigned*>(d);
    for (unsigned i = n >> 2; i; --i) *dd++ = 0;
    volatile unsigned char* db = reinterpret_cast<volatile unsigned char*>(const_cast<unsigned*>(dd));
    for (unsigned i = n & 3; i; --i) *db++ = 0;
}

// The channel's successor: X = *(ch+0x13a0) and its channel part sub = X ? X + 0x70 : 0 (the original's NULL test of a
// base-class conversion; every one follows a dereference of X, so its NULL side faults in the original as here).
char* c3fp_subOf(char* X, bool isNull) { return isNull ? 0 : X + 0x70; }

// =====================================================================================================================
// renderChannelStreamed
// =====================================================================================================================

// 0x00036660  renderChannelStreamed (VA 0x10036660). __thiscall (a1, a2, out), `ret 0xc`; a1 and a2 are never read.
// this = ch. Returns 0x13 when ch+0x54 (the DsBuffer) is NULL (0x1003668d), DsBuffer::getPosition's result when it is
// neither 0 nor 0x2d (0x100366cc / 0x100366d1, 0x10036f68), else 0xb (0x10036eee / 0x10036f63).
// Source ring obj = ch+8 {+0x14 size, +0x18 start, +0x1c read, +0x20 limit, +0x24 end}: the read cursor src starts at
// obj+0x1c and the read limit lim is obj+0x20, or obj+0x24 when the cursor is past obj+0x20 (0x100366a1-0x100366b4).
// getPosition (thunk 0x1000233d) fills want and off; q = want / word ds+0xb4 (0 when the word is 0, 0x100366da-0x100366fc);
// DsBuffer::restore (thunk 0x1000213f) with (off, want) gives the two locked segments (p1, n1), (p2, n2).
// The copy loop (0x1003673d-0x10036a60) moves want bytes from the source into the segments, `rep movsd; rep movsb` each
// time. Per pass n = rem, clamped to lim - src when src + n > lim, in which case, with flag 0x20 of ch+0x58 and (flag 4
// clear or a successor at ch+0x13a0), flag 0x10 of ch+0x58 is set when src <= obj+0x20 and bit 0 of ch+0x5c otherwise
// (0x1003674c-0x1003677d). An empty segment switches to the second one (0x1003678a-0x1003679a); a step larger than the
// segment copies the segment's bytes without advancing the destination (0x100367a2-0x100367da). A step shorter than the
// segment with flags 0x10 and 0x20 and a successor copies it and continues from the successor's stream: its mmio stream
// ([sub+0x1390] read / limit) when the successor's byte X+0xcc has bit 2, else its ring X+0x78 (0x10036805-0x100369be);
// the read cursor then becomes the stream's start plus the segment bytes LEFT (0x100368ed-0x10036902), the stream cursor
// is stored back (or closeMmioSound, thunk 0x10001b18, when it reached the limit, and the next stream is read), or the ring's
// read cursor is advanced by the bytes copied, and the pass is marked `chained`. Otherwise the step is copied and the cursor
// wraps to obj+0x18 when bit 0 of ch+0x5c was set (clearing it) or when it reached obj+0x20 without flag 0x10; with flag
// 0x10 the rest of the segment is zero-filled (0x100369c3-0x10036a56).
// Then DsBuffer::unlock (thunk 0x10001ec9) with the two segments, the volume fade (c3fp_fade) and either the unchained end
// (cursor back into obj+0x1c, *out = q, the 0x7eb notice when the cursor ran more than obj+0x14 / 4 ahead of ch+0x1378)
// or the hand-over of the DsBuffer and the fade state to the successor (0x10036c74-0x10036ee5).
typedef unsigned(__fastcall* Stream_t)(void*, void*, unsigned, unsigned, unsigned*);
Stream_t Stream_orig;

// The fade (0x10036a82-0x10036c63); flags fl = ch+0x58, g = ch+0x5c, doubles V = ch+0x1e0, A = ch+0x1c8 (step),
// T = ch+0x1d0 (target), ch+0x210.
void c3fp_fade(char* ch) {
    unsigned fl = fat<unsigned>(ch, 0x58);                                    // 0x10036a82
    if (!J(0x10036a88, (fl & 8) != 0) && J(0x10036a8e, (fat<unsigned char>(ch, 0x5c) & 0x10) == 0)) return;   // -> 0x10036c68
    unsigned g = fat<unsigned>(ch, 0x5c);                                     // 0x10036a94
    if (!J(0x10036a9a, (g & 2) == 0)) {
        unsigned* q = fat<unsigned*>(ch, 0x13a4);                             // 0x10036a9c
        fat<unsigned>(ch, 0x58) = fl | 0x10;                                  // 0x10036aa2 / 0x10036aa7
        if (!J(0x10036aaa, q == 0)) *q = 0;                                   // 0x10036ab0
        return;
    }
    X80 st, a, v, t;
    if (!J(0x10036abe, (g & 8) == 0)) {
        x80Ld64(&v, ch + 0x1e0);                                              // 0x10036ac0
        if (!J(0x10036ad1, (comM(&v, ch + 0x210) & 0x100) != 0)) {            // fcomp 0x10036ac6; jne when C0
            fat<unsigned>(ch, 0x58) = fl & ~8u;                               // 0x10036ad7 / 0x10036add
            fat<unsigned>(ch, 0x5c) = g & ~8u;                                // 0x10036ada / 0x10036ae0
        }
        return;
    }
    if (!J(0x10036aeb, (g & 0x10) == 0)) {
        x80Ld64(&a, ch + 0x1c8);                                              // 0x10036af1
        if (!J(0x10036b02, (comM(&a, c3fp_sound(kKZero)) & 0x4100) != 0)) {   // fcom 0.0 0x10036af7: A > 0.0
            x80Ld64(&v, ch + 0x1e0);                                          // fstp st(0) 0x10036b04; fld 0x10036b06
            if (J(0x10036b17, (comM(&v, ch + 0x1d0) & 0x4100) != 0)) {        // fcomp T 0x10036b0c: V <= T or unordered
                fat<unsigned>(ch, 0x5c) = g & ~0x10u;                         // 0x10036bdb / 0x10036bde
                return;
            }
            x80Ld64(&v, ch + 0x1e0);                                          // 0x10036b1d
            if (J(0x10036b2e, (comM(&v, ch + 0x1c8) & 0x100) == 0)) {         // fcomp A 0x10036b23: V >= A
                x80Ld64(&v, ch + 0x1e0);                                      // 0x10036b53
                x80SubM64(&st, &v, ch + 0x1c8);                               // 0x10036b59
                x80St64(ch + 0x1e0, &st);                                     // fstp 0x10036b5f
                x80Ld64(&st, ch + 0x1e0);                                     // 0x10036b65
            } else {
                fat<unsigned>(ch, 0x1e0) = fat<unsigned>(ch, 0x1d0);          // 0x10036b30 / 0x10036b3c
                fat<unsigned>(ch, 0x1e4) = fat<unsigned>(ch, 0x1d4);          // 0x10036b36 / 0x10036b42
                x80Ld64(&st, ch + 0x1e0);                                     // 0x10036b48
            }
        } else {
            // A <= 0.0 or unordered; A stays in st(0) (0x10036b70)
            x80Ld64(&v, ch + 0x1e0);                                          // 0x10036b70
            if (J(0x10036b81, (comM(&v, ch + 0x1d0) & 0x100) == 0)) {         // fcomp T 0x10036b76: V >= T
                fat<unsigned>(ch, 0x5c) = g & ~0x10u;                         // fstp st(0) 0x10036bd9; 0x10036bdb
                return;
            }
            X80 d, aa;
            x80Ld64(&t, ch + 0x1d0);                                          // 0x10036b83
            x80SubM64(&d, &t, ch + 0x1e0);                                    // 0x10036b89: T - V
            x80Abs(&aa, &a);                                                  // fxch 0x10036b8f; fabs 0x10036b91
            if (J(0x10036b9a, (x80Com(&aa, &d) & 0x100) == 0)) {              // fcompp 0x10036b93: |A| >= T - V
                fat<unsigned>(ch, 0x1e0) = fat<unsigned>(ch, 0x1d0);          // 0x10036bb9 / 0x10036bc5
                fat<unsigned>(ch, 0x1e4) = fat<unsigned>(ch, 0x1d4);          // 0x10036bbf / 0x10036bcb
                x80Ld64(&st, ch + 0x1e0);                                     // 0x10036bd1
            } else {
                x80Ld64(&v, ch + 0x1e0);                                      // 0x10036b9c
                x80SubM64(&st, &v, ch + 0x1c8);                               // 0x10036ba2
                x80St64(ch + 0x1e0, &st);                                     // fstp 0x10036ba8
                x80Ld64(&st, ch + 0x1e0);                                     // 0x10036bae
            }
        }
    } else {
        x80Ld64(&v, ch + 0x1e0);                                              // 0x10036be6
        if (!J(0x10036bf7, (comM(&v, c3fp_sound(kKZero)) & 0x4000) == 0)) {   // fcomp 0.0 0x10036bec: V == 0 or unordered
            unsigned* q = fat<unsigned*>(ch, 0x13a4);                         // 0x10036bf9
            fat<unsigned>(ch, 0x58) = fl | 0x10;                              // 0x10036bff / 0x10036c04
            if (!J(0x10036c07, q == 0)) *q = 0;                               // 0x10036c09
        }
        x80Ld64(&v, ch + 0x1e0);                                              // 0x10036c0f
        if (!J(0x10036c20, (comM(&v, ch + 0x1c8) & 0x4100) == 0)) {           // fcomp A 0x10036c15: V <= A or unordered
            unsigned g2 = fat<unsigned>(ch, 0x5c);                            // 0x10036c22
            fat<unsigned>(ch, 0x1c8) = fat<unsigned>(ch, 0x1e0);              // 0x10036c25 / 0x10036c2d
            fat<unsigned>(ch, 0x5c) = g2 | (NEG(3) ? 1u : 2u);                // 0x10036c2b / 0x10036c33
            fat<unsigned>(ch, 0x1cc) = fat<unsigned>(ch, 0x1e4);              // 0x10036c36 / 0x10036c3c
        }
        x80Ld64(&v, ch + 0x1e0);                                              // 0x10036c42
        if (NEG(6)) {                                                         // wrong body 6: a 53-bit double subtraction
            double dv = fat<double>(ch, 0x1e0) - fat<double>(ch, 0x1c8);
            fat<double>(ch, 0x1e0) = dv;
            x80Ld64(&st, ch + 0x1e0);
        } else {
            x80SubM64(&st, &v, ch + 0x1c8);                                   // 0x10036c48
            x80St64(ch + 0x1e0, &st);                                         // fst 0x10036c4e: st(0) keeps the unrounded value
        }
    }
    x80MulM64(&st, &st, c3fp_sound(kK127));                                   // 0x10036c54
    unsigned vol = static_cast<unsigned>(static_cast<uint64_t>(x80FtolAt(&st, c3fp_sound(kFtol))));   // 0x10036c5a
    c3fp_fn<SetVol_t>(kThSetVolume)(fat<char*>(ch, 0x54), 0, vol);            // 0x10036c5f-0x10036c63
}

unsigned __fastcall Stream_re(void* self, void*, unsigned a1, unsigned a2, unsigned* out) {
    (void)a1;
    (void)a2;
    char* ch = static_cast<char*>(self);
    unsigned char chained = 0;                                                // S+0x13, 0x1003666e
    unsigned want = 0, off = 0, n1 = 0, n2 = 0, p1 = 0, p2 = 0;               // 0x10036675-0x10036689
    unsigned src, lim, q, rem, dst, room, n, n3;
    char* X;
    char* sub;
    char* ds = fat<char*>(ch, 0x54);                                          // 0x1003666b
    if (!J(0x1003668d, ds != 0)) return 0x13;                                 // 0x1003668f-0x1003669b
    char* obj = fat<char*>(ch, 8);                                            // 0x1003669e
    src = fat<unsigned>(obj, 0x1c);                                           // 0x100366a1 / 0x100366a9
    if (J(0x100366ad, src <= fat<unsigned>(obj, 0x20))) lim = fat<unsigned>(obj, 0x20);   // 0x100366b4
    else lim = fat<unsigned>(obj, 0x24);                                      // 0x100366af
    unsigned r = c3fp_fn<GetPos_t>(kThGetPosition)(ds, 0, &want, &off, 0);    // 0x100366b6-0x100366c5 (ecx = ds)
    if (!J(0x100366cc, r == 0) && J(0x100366d1, r != 0x2d)) return r;         // 0x10036f68
    ds = fat<char*>(ch, 0x54);                                                // 0x100366d7
    {
        unsigned short align = fat<unsigned short>(ds, 0xb4);                 // 0x100366da
        if (J(0x100366e8, align == 0)) q = 0;                                 // 0x100366fc
        else q = want / align;                                                // 0x100366ea-0x100366f6
    }
    c3fp_fn<Restore_t>(kThRestore)(ds, 0, off, want, &p1, &n1, &p2, &n2);     // 0x10036704-0x1003671e
    rem = want;                                                               // 0x10036723
    dst = p1;                                                                 // 0x10036727 / 0x1003672f
    room = n1;                                                                // 0x1003672b
    if (!J(0x10036735, rem == 0)) {
        do {
            n = rem;                                                          // eax = S+0x18 (0x10036a5a)
            if (!J(0x1003674a, src + n <= lim)) {                             // 0x10036741-0x1003674a
                unsigned fl = fat<unsigned>(ch, 0x58);                        // 0x1003674c
                n = lim - src;                                                // 0x1003674f / 0x10036751
                bool mark;
                if (!J(0x10036756, (fl & 4) != 0) && J(0x1003675b, (fl & 0x20) != 0)) mark = true;
                else mark = !J(0x10036765, fat<unsigned>(ch, 0x13a0) == 0) && !J(0x1003676a, (fl & 0x20) == 0);
                if (mark) {
                    char* o = fat<char*>(ch, 8);                              // 0x1003676c
                    if (J(0x10036772, src <= fat<unsigned>(o, 0x20))) fat<unsigned>(ch, 0x58) = fl | 0x10;   // 0x1003677a
                    else fat<unsigned>(ch, 0x5c) |= 1;                        // 0x10036774
                }
            }
            if (J(0x10036782, n == 0)) continue;                              // -> 0x10036a5a (rem unchanged)
            for (;;) {
                if (!J(0x1003678a, room != 0) && !J(0x10036792, n2 == 0)) {
                    room = n2;                                                // 0x10036798
                    dst = p2;                                                 // 0x10036794 / 0x1003679a
                }
                if (J(0x100367a0, n <= room)) break;                          // -> 0x100367dc
                c3fp_movs(dst, src, room);                                    // 0x100367a2-0x100367c0
                n -= room;                                                    // 0x100367ac
                src += room;                                                  // 0x100367be / 0x100367c6
                rem -= room;                                                  // 0x100367ca / 0x100367d0
                room = 0;                                                     // 0x100367cc
                if (J(0x100367d4, n == 0)) goto next;
            }
            {
                bool toSucc = false;
                if (!J(0x100367dc, n >= room)) {
                    unsigned fl = fat<unsigned>(ch, 0x58);                    // 0x100367e2
                    toSucc = !J(0x100367e8, (fl & 0x10) == 0) && !J(0x100367f6, fat<unsigned>(ch, 0x13a0) == 0) &&
                             !J(0x100367ff, (fl & 0x20) == 0);
                }
                if (toSucc) {
                    c3fp_movs(dst, src, n);                                   // 0x10036805-0x10036823
                    room -= n;                                                // 0x1003680f
                    dst += n;                                                 // 0x1003681a-0x10036829
                    rem -= n;                                                 // 0x10036825 / 0x10036835
                    X = *fat<char**>(ch, 0x13a0);                             // 0x1003682f / 0x10036839
                    if (!J(0x10036842, (fat<unsigned char>(X, 0xcc) & 4) == 0)) {
                        sub = c3fp_subOf(X, J(0x10036846, X == 0));
                        src = fat<unsigned>(fat<char*>(sub, 0x1390), 0x1c);   // 0x1003684f / 0x10036857 / 0x1003685a
                        sub = c3fp_subOf(X, J(0x1003685e, X == 0));
                        lim = fat<unsigned>(fat<char*>(sub, 0x1390), 0x20);   // 0x10036867 / 0x1003686d / 0x10036875
                        if (J(0x10036879, room + src <= lim)) {
                            n3 = room;                                        // 0x100368c1
                        } else {
                            sub = c3fp_subOf(X, J(0x1003687d, X == 0));
                            n3 = fat<unsigned>(fat<char*>(sub, 0x1390), 0x20) - src;   // 0x10036882-0x1003689a
                        }
                    } else {
                        char* o2 = fat<char*>(X, 0x78);                       // 0x1003689e
                        src = fat<unsigned>(o2, 0x1c);                        // 0x100368a3 / 0x100368ae
                        if (J(0x100368b2, fat<unsigned>(o2, 0x1c) <= fat<unsigned>(o2, 0x20)))
                            lim = fat<unsigned>(o2, 0x20);                    // 0x100368bd
                        else
                            lim = fat<unsigned>(o2, 0x24);                    // 0x100368b4 / 0x100368b7
                        n3 = room;                                            // 0x100368c1
                    }
                    c3fp_movs(dst, src, n3);                                  // 0x100368c3-0x100368d7
                    room -= n3;                                               // 0x100368cb
                    rem -= n3;                                                // 0x100368d9-0x100368e9
                    dst += n3;                                                // 0x100368e3 / 0x100368f3 / 0x100368f7
                    src += NEG(2) ? n3 : room;                                // 0x100368ed / 0x100368f5 / 0x10036902: the bytes left
                    X = *fat<char**>(ch, 0x13a0);                             // 0x100368dd / 0x100368f1
                    if (!J(0x10036906, (fat<unsigned char>(X, 0xcc) & 4) == 0)) {
                        if (!J(0x10036910, src < lim)) {
                            sub = c3fp_subOf(X, J(0x10036914, X == 0));
                            c3fp_fn<Close_t>(kThCloseMmio)(sub, 0);           // 0x1003691d
                            X = *fat<char**>(ch, 0x13a0);                     // 0x10036922 / 0x10036928
                            sub = c3fp_subOf(X, J(0x1003692c, X == 0));
                            src = fat<unsigned>(fat<char*>(sub, 0x1390), 0x1c);   // 0x10036935-0x10036940
                            sub = c3fp_subOf(X, J(0x10036944, X == 0));
                            chained = 1;                                      // 0x10036949 / 0x10036962
                            lim = fat<unsigned>(fat<char*>(sub, 0x1390), 0x20);   // 0x1003694e-0x10036957 / 0x10036967-0x10036970
                        } else {
                            sub = c3fp_subOf(X, J(0x1003697b, X == 0));
                            chained = 1;                                      // 0x10036980 / 0x10036995
                            fat<unsigned>(fat<char*>(sub, 0x1390), 0x1c) = src;   // 0x10036985 / 0x1003698b
                        }
                    } else {
                        sub = c3fp_subOf(X, J(0x100369aa, X == 0));
                        chained = 1;                                          // 0x100369b6
                        fat<unsigned>(fat<char*>(sub, 8), 0x1c) += n3;        // 0x100369b3 / 0x100369bb
                    }
                    goto next;
                }
            }
            c3fp_movs(dst, src, n);                                           // 0x100369c3-0x100369e1
            room -= n;                                                        // 0x100369cd
            dst += n;                                                         // 0x100369d8 / 0x100369df / 0x100369fc
            rem -= n;                                                         // 0x100369e3-0x100369f2
            if (!NEG(1)) src += n;                                            // 0x100369e7 / 0x100369ed / 0x100369f8
            {
                unsigned g = fat<unsigned>(ch, 0x5c);                         // 0x100369ef
                if (!J(0x10036a00, (g & 1) == 0)) {
                    src = fat<unsigned>(fat<char*>(ch, 8), 0x18);             // 0x10036a02 / 0x10036a07 / 0x10036a0d
                    fat<unsigned>(ch, 0x5c) = g & ~1u;                        // 0x10036a05 / 0x10036a0a
                } else {
                    char* o = fat<char*>(ch, 8);                              // 0x10036a13
                    if (!J(0x10036a1d, src != fat<unsigned>(o, 0x20)) &&
                        !J(0x10036a23, (fat<unsigned char>(ch, 0x58) & 0x10) != 0)) {
                        src = fat<unsigned>(o, 0x18);                         // 0x10036a25 / 0x10036a28
                    } else if (!J(0x10036a32, (fat<unsigned char>(ch, 0x58) & 0x10) == 0) && !J(0x10036a36, room == 0)) {
                        c3fp_stos0(dst, room);                                // 0x10036a38-0x10036a4c (dst not advanced)
                        rem -= room;                                          // 0x10036a4e-0x10036a56
                        room = 0;                                             // 0x10036a54
                    }
                }
            }
        next:;
        } while (J(0x10036a60, rem != 0));
    }
    c3fp_fn<Unlock_t>(kThUnlock)(fat<char*>(ch, 0x54), 0, p1, n1, p2, n2);   // 0x10036a66-0x10036a7d
    c3fp_fade(ch);                                                            // 0x10036a82-0x10036c63
    if (J(0x10036c6e, chained == 0)) {
        fat<unsigned>(fat<char*>(ch, 8), 0x1c) = src;                         // 0x10036efa / 0x10036efd / 0x10036f05
        *out = q;                                                             // 0x10036f01 / 0x10036f08 / 0x10036f0c
        unsigned f = fat<unsigned>(ch, 0x58);                                 // 0x10036f0e
        if (!J(0x10036f13, (f & 2) == 0) && !J(0x10036f17, (f & 0x21) != 0) && !J(0x10036f1c, (f & 0x400) != 0)) {
            char* o = fat<char*>(ch, 8);                                      // 0x10036f1e
            unsigned d = (fat<unsigned>(o, 0x1c) - fat<unsigned>(ch, 0x1378)) & fat<unsigned>(ch, 0x1374);   // 0x10036f21-0x10036f35
            if (!J(0x10036f3c, d <= (fat<unsigned>(o, 0x14) >> (NEG(4) ? 1 : 2)))) {   // 0x10036f30-0x10036f3c
                unsigned c = fat<unsigned>(ch, 0x1370);                       // 0x10036f3e
                fat<unsigned>(ch, 0x58) = f | 0x400;                          // 0x10036f44 / 0x10036f55
                fat<unsigned>(ch, 0x1370) = c + 1;                            // 0x10036f49 / 0x10036f58
                c3fp_fn<Post_t>(kThPost)(c3fp_sound(kSigObj), 0, 0x7eb, reinterpret_cast<unsigned>(ch), 0);   // 0x10036f47-0x10036f5e
            }
        }
        return 0xb;                                                           // 0x10036f63
    }
    // the successor takes over (0x10036c74-0x10036ee5)
    sub = c3fp_subOf(X = *fat<char**>(ch, 0x13a0), J(0x10036c80, *fat<char**>(ch, 0x13a0) == 0));
    fat<unsigned>(sub, 0x1e0) = fat<unsigned>(ch, 0x1e0);                     // 0x10036c89 / 0x10036c8f
    fat<unsigned>(sub, 0x1e4) = fat<unsigned>(ch, 0x1e4);                     // 0x10036c95 / 0x10036c9b
    X = *fat<char**>(ch, 0x13a0);                                             // 0x10036ca1 / 0x10036ca7
    sub = c3fp_subOf(X, J(0x10036cab, X == 0));
    fat<unsigned>(sub, 0x1c8) = fat<unsigned>(ch, 0x1c8);                     // 0x10036cb4 / 0x10036cba
    fat<unsigned>(sub, 0x1cc) = fat<unsigned>(ch, 0x1cc);                     // 0x10036cc0 / 0x10036cc6
    X = *fat<char**>(ch, 0x13a0);                                             // 0x10036ccc / 0x10036cd2
    sub = c3fp_subOf(X, J(0x10036cd6, X == 0));
    fat<unsigned>(sub, 0x1d0) = fat<unsigned>(ch, 0x1d0);                     // 0x10036cdf / 0x10036ce5
    fat<unsigned>(sub, 0x1d4) = fat<unsigned>(ch, 0x1d4);                     // 0x10036ceb / 0x10036cf1
    {
        static const int kOffs[6] = {0x1d8, 0x208, 0x1f8, 0x1e8, 0x218, 0x21c};
        static const unsigned kVas[6] = {0x10036d01, 0x10036d20, 0x10036d3f, 0x10036d5e, 0x10036d7d, 0x10036d9c};
        for (int i = 0; i < 6; ++i) {                                         // 0x10036cf7-0x10036dab
            X = *fat<char**>(ch, 0x13a0);
            sub = c3fp_subOf(X, J(kVas[i], X == 0));
            fat<unsigned>(sub, kOffs[i]) = fat<unsigned>(ch, kOffs[i]);
        }
    }
    X = *fat<char**>(ch, 0x13a0);                                             // 0x10036db1 / 0x10036db7
    sub = c3fp_subOf(X, J(0x10036dbb, X == 0));
    if (!J(0x10036dc8, (fat<unsigned char>(sub, 0x5c) & 4) != 0)) {           // 0x10036dc4
        sub = c3fp_subOf(X, J(0x10036dcc, X == 0));
        unsigned f = fat<unsigned>(sub, 0x58);                                // 0x10036dd5
        if (!J(0x10036dda, (f & 2) == 0) && !J(0x10036dde, (f & 0x21) != 0) && !J(0x10036de3, (f & 0x400) != 0)) {
            char* o2 = fat<char*>(sub, 8);                                    // 0x10036de5
            unsigned d = (fat<unsigned>(o2, 0x1c) - fat<unsigned>(sub, 0x1378)) & fat<unsigned>(sub, 0x1374);   // 0x10036de8-0x10036dfc
            if (!J(0x10036e03, d <= (fat<unsigned>(o2, 0x14) >> 2))) {        // 0x10036df1 / 0x10036dfe / 0x10036e01
                fat<unsigned>(sub, 0x58) = f | 0x400;                         // 0x10036e05 / 0x10036e09
                char* P = fat<char*>(ch, 0x13a0);                             // 0x10036e0c
                fat<unsigned>(P, 0x58) |= 0x400;                              // 0x10036e12-0x10036e18
                P = fat<char*>(ch, 0x13a0);                                   // 0x10036e1b
                fat<unsigned>(P, 0x1370) += 1;                                // 0x10036e21-0x10036e28
                P = fat<char*>(ch, 0x13a0);                                   // 0x10036e2e
                c3fp_fn<Post_t>(kThPost)(c3fp_sound(kSigObj), 0, 0x7eb, fat<unsigned>(P, 0), 0);   // 0x10036e08 / 0x10036e34-0x10036e41
            }
        }
    }
    c3fp_fn<AddList_t>(kThAddToList)(*reinterpret_cast<void**>(c3fp_sound(kGSoundEngine)), 0,
                                     fat<unsigned>(fat<char*>(ch, 0x13a0), 0));   // 0x10036e46-0x10036e55
    X = *fat<char**>(ch, 0x13a0);                                             // 0x10036e5a / 0x10036e60
    sub = c3fp_subOf(X, J(0x10036e64, X == 0));
    fat<unsigned>(sub, 0x54) = fat<unsigned>(ch, 0x54);                       // 0x10036e6d / 0x10036e70
    X = *fat<char**>(ch, 0x13a0);                                             // 0x10036e73 / 0x10036e79
    sub = c3fp_subOf(X, J(0x10036e7d, X == 0));
    fat<unsigned>(sub, 0x58) |= 0x10000000;                                   // 0x10036e86-0x10036e8f
    {
        char* P = fat<char*>(ch, 0x13a0);                                     // 0x10036e92
        unsigned* Q = fat<unsigned*>(ch, 0x13a4);                             // 0x10036e98
        fat<unsigned>(ch, 0x54) = 0;                                          // 0x10036e9e
        *Q = fat<unsigned>(P, 0);                                             // 0x10036ea1 / 0x10036ea3
        P = fat<char*>(ch, 0x13a0);                                           // 0x10036ea5
        fat<unsigned>(P, 0) = 0;                                              // 0x10036eab
    }
    {
        unsigned g = fat<unsigned>(ch, 0x5c);                                 // 0x10036ead
        unsigned f = fat<unsigned>(ch, 0x58);                                 // 0x10036eb0
        fat<unsigned>(ch, 0x5c) = g | 0x80;                                   // 0x10036eb3 / 0x10036eb6
        fat<unsigned>(ch, 0x58) = f | 0x10;                                   // 0x10036eb9-0x10036ec0
    }
    for (int i = 0; i < 16; ++i) {                                            // 0x10036ec3-0x10036edd: 16 records of 0x110 bytes
        unsigned char b = fat<unsigned char>(ch, 0x2d4 + 0x110 * i);
        fat<unsigned char>(ch, 0x2d4 + 0x110 * i) = static_cast<unsigned char>((b & (NEG(5) ? 0xff : 0xfd)) | 4);
    }
    fat<unsigned>(ch, 0x13a0) = 0;                                            // 0x10036edf
    fat<unsigned>(ch, 0x13a4) = 0;                                            // 0x10036ee5
    return 0xb;                                                               // 0x10036eee
}

}  // namespace

SG_HOOK("sound.dll", 0x00036660, renderChannelStreamed, Stream_re, Stream_orig);

// C3 batch c3fn (round 35, 2026-10-14) of sound.dll: the MMIO rows c3fg left (log/c3/c3fn_candidates.tsv,
// log/c3/round35_brief.md), A/B'd over the mmio stand-in re/frida/js/mmiostandin.js.
// Method as c3fg: fixture-owned objects only, module globals the body reads or writes swapped for the key with the first
// thread frozen, operator new / delete / malloc / free through heapstandin.js, winmm imports through the mmio stand-in, other
// imports through importstandin.js. Addresses in SG_HOOK and in the `// 0x...` comment lines are RVAs; the instruction
// addresses quoted inside each body are file VAs (VA = 0x10000000 + RVA), from log/c3/alt/c3fg_asm/<rva>_<name>.txt.
// sound.dll is the release build (/O2) and loads above its preferred base: module globals, thunks and IAT slots are taken
// relative to the loaded base. __thiscall is emulated with __fastcall (ecx = this, edx unused). Every callee runs through
// the address the original calls (its incremental-link thunk when the original calls the thunk), every import through the
// original's IAT slot, so the stand-ins see the reimplementation's calls exactly as the original's.
//
// Floating point. Only three bodies here touch the x87 unit, and in each one no instruction rounds (measured constants,
// log/c3/c3fn_notes.txt section 2), so plain C is an exact model whatever the caller's control word:
//   loadWaveResource: fild word-sized ints, fmul by 2^-15 (0x1005c5e0 = 0x38000000), fstp dword: x * 2^-15 for |x| <= 2^15
//     is a float exactly.
//   openMmioSound: fld dword, fmul by 2^15 (0x1005c524 = 32768.0f), fst dword: exact unless it overflows the float range,
//     where nearest gives +-inf and toward-zero gives +-FLT_MAX: both land in the same clamp arm.
//   closeMmioSound: fld qword / fst qword / fstp qword copies (exact; a signaling NaN comes out quiet, c3fn_x87Copy) and an
//     fcomp with 0.0 (C3 set for equal or unordered).
#include "hooks.h"
#include <stdint.h>
#include <string.h>

namespace {

template <typename T> T& fat(void* base, int off) {
    return *reinterpret_cast<T*>(reinterpret_cast<char*>(base) + off);
}

char* c3fn_sound(unsigned rva) {
    static HMODULE m = 0;
    if (!m) m = GetModuleHandleA("sound.dll");
    return reinterpret_cast<char*>(m) + rva;
}
template <typename F> F c3fn_fn(unsigned rva) { return reinterpret_cast<F>(c3fn_sound(rva)); }
template <typename T> T& c3fn_g(unsigned rva) { return *reinterpret_cast<T*>(c3fn_sound(rva)); }
// the import whose IAT slot is at RVA `slotRva` (`call dword ptr [0x100b....]`)
template <typename F> F c3fn_imp(unsigned slotRva) { return *reinterpret_cast<F*>(c3fn_sound(slotRva)); }
unsigned c3fn_u(const void* p) { return static_cast<unsigned>(reinterpret_cast<uintptr_t>(p)); }

typedef void*(__cdecl* OpNew_t)(unsigned);
typedef void(__cdecl* OpDelete_t)(void*);
void* c3fn_new(unsigned n) { return c3fn_fn<OpNew_t>(0x000424d7)(n); }        // operator new 0x100424d7
void c3fn_delete(void* p) { c3fn_fn<OpDelete_t>(0x0004249a)(p); }             // operator delete 0x1004249a
void c3fn_free(void* p) { c3fn_fn<OpDelete_t>(0x00042c07)(p); }               // free 0x10042c07

typedef unsigned(__fastcall* Th0_t)(void*, void*);
typedef unsigned(__fastcall* Th1_t)(void*, void*, unsigned);
typedef unsigned(__fastcall* Th2_t)(void*, void*, unsigned, unsigned);
typedef unsigned(__fastcall* Th4_t)(void*, void*, unsigned, unsigned, unsigned, unsigned);
typedef unsigned(__fastcall* Th6_t)(void*, void*, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned);
typedef unsigned(__cdecl* Cd3_t)(unsigned, unsigned, unsigned);
typedef unsigned(__cdecl* Cd5_t)(unsigned, unsigned, unsigned, unsigned, unsigned);
// virtual slot `off` of `obj` (`mov eax, [obj]; call [eax + off]`)
template <typename F> F c3fn_vfn(void* obj, int off) {
    return *reinterpret_cast<F*>(*reinterpret_cast<char**>(obj) + off);
}

// WINMM / KERNEL32 IAT slots of sound.dll (log/c3/alt/c3fn_peek.py iat)
const unsigned kMmioDescend = 0x000b8624, kMmioAscend = 0x000b8628, kMmioSetBuffer = 0x000b862c,
               kMmioAdvance = 0x000b8630, kMmioSetInfo = 0x000b8634, kMmioWrite = 0x000b8658,
               kMmioStringToFourcc = 0x000b865c, kMmioOpen = 0x000b8660, kMmioRead = 0x000b8664,
               kMmioGetInfo = 0x000b8668, kMmioSeek = 0x000b866c, kMmioClose = 0x000b8670, kSleep = 0x000b840c;
typedef unsigned(__stdcall* MmioClose_t)(void*, unsigned);
typedef long(__stdcall* MmioRead_t)(void*, void*, long);
typedef long(__stdcall* MmioSeek_t)(void*, long, int);
typedef void*(__stdcall* MmioOpen_t)(char*, void*, unsigned);
typedef unsigned(__stdcall* MmioInfo_t)(void*, void*, unsigned);          // GetInfo / SetInfo / Advance
typedef unsigned(__stdcall* MmioSetBuffer_t)(void*, void*, long, unsigned);
typedef unsigned(__stdcall* MmioFourcc_t)(const char*, unsigned);
typedef long(__stdcall* MmioWrite_t)(void*, const void*, long);
typedef void(__stdcall* Sleep_t)(unsigned);
unsigned c3fn_mmioClose(void* h, unsigned f) { return c3fn_imp<MmioClose_t>(kMmioClose)(h, f); }
long c3fn_mmioRead(void* h, void* p, long n) { return c3fn_imp<MmioRead_t>(kMmioRead)(h, p, n); }
long c3fn_mmioSeek(void* h, long o, int w) { return c3fn_imp<MmioSeek_t>(kMmioSeek)(h, o, w); }
void* c3fn_mmioOpen(char* n, void* i, unsigned f) { return c3fn_imp<MmioOpen_t>(kMmioOpen)(n, i, f); }
unsigned c3fn_mmioGetInfo(void* h, void* i, unsigned f) { return c3fn_imp<MmioInfo_t>(kMmioGetInfo)(h, i, f); }
unsigned c3fn_mmioSetInfo(void* h, void* i, unsigned f) { return c3fn_imp<MmioInfo_t>(kMmioSetInfo)(h, i, f); }
unsigned c3fn_mmioAdvance(void* h, void* i, unsigned f) { return c3fn_imp<MmioInfo_t>(kMmioAdvance)(h, i, f); }
unsigned c3fn_mmioSetBuffer(void* h, void* b, long n, unsigned f) { return c3fn_imp<MmioSetBuffer_t>(kMmioSetBuffer)(h, b, n, f); }
unsigned c3fn_fourcc(const char* s, unsigned f) { return c3fn_imp<MmioFourcc_t>(kMmioStringToFourcc)(s, f); }
long c3fn_mmioWrite(void* h, const void* p, long n) { return c3fn_imp<MmioWrite_t>(kMmioWrite)(h, p, n); }
void c3fn_sleep(unsigned ms) { c3fn_imp<Sleep_t>(kSleep)(ms); }

// module globals (file VAs 0x100b....)
const unsigned kOpenSoundList = 0x000b49c8;    // g_openSoundList: head +0 (0x100b49c8), tail +4, cursor +8, count +0xc
const unsigned kOpenSoundTail = 0x000b49cc;
const unsigned kOpenSoundCur = 0x000b49d0;
const unsigned kOpenSoundCount = 0x000b49d4;
const unsigned kOpenMmioCount = 0x000b4a08;    // g_openMmioCount

struct ListNode { ListNode* prev; ListNode* next; void* value; };

// The g_openSoundList unlink these bodies share (closeMmioSound 0x10035ce1-0x10035d42, streamWaveSamples 0x10030c29-0x10030c8b;
// c3fg's Sound_resetBuffers / parseRiffWave carry the same sequence): the first node whose value is `value` is taken out
// (prev->next or the head updated; next->prev updated and the cursor 0x100b49d0 set to next, or with no next the cursor = 0
// and the tail 0x100b49cc = the node's prev), freed with operator delete, and g_openSoundCount 0x100b49d4 lowered. Nothing
// happens when no node matches.
void c3fn_unlinkOpenSound(void* value) {
    ListNode* n = c3fn_g<ListNode*>(kOpenSoundList);
    while (n && n->value != value) n = n->next;
    if (!n) return;
    if (n->prev) n->prev->next = n->next;
    else c3fn_g<ListNode*>(kOpenSoundList) = n->next;
    if (n->next) {
        n->next->prev = n->prev;
        c3fn_g<ListNode*>(kOpenSoundCur) = n->next;
    } else {
        c3fn_g<ListNode*>(kOpenSoundCur) = 0;
        c3fn_g<ListNode*>(kOpenSoundTail) = n->prev;
    }
    c3fn_delete(n);
    --c3fn_g<unsigned>(kOpenSoundCount);
}

// The front pop of a {head, tail, count} node list (head at h, tail at h + 4, count at h + 0xc), as streamWaveSamples does it
// on this+0x5c three times (0x10030be0-0x10030c11, 0x10030d91-0x10030dbe, 0x10030e3c-0x10030e64): the head's successor
// becomes the head with its prev cleared, or the list empties (tail and head 0); the node is freed with operator delete and
// the count lowered. Returns the node's value. The caller tests for an empty list first.
void* c3fn_popFront(char* self) {
    ListNode* n = fat<ListNode*>(self, 0x5c);
    ListNode* next = n->next;
    if (next) {
        next->prev = 0;
        fat<ListNode*>(self, 0x5c) = next;
    } else {
        fat<ListNode*>(self, 0x60) = 0;
        fat<ListNode*>(self, 0x5c) = 0;
    }
    void* v = n->value;
    c3fn_delete(n);
    --fat<unsigned>(self, 0x68);
    return v;
}

// An fld qword / fst qword pair: the value is copied exactly; a signaling NaN (exponent all ones, a non-zero fraction with
// its top bit clear) is quieted by the load (the invalid exception is masked in every control word sound.dll uses).
void c3fn_x87Copy(void* dst, const void* src) {
    uint64_t v;
    memcpy(&v, src, 8);
    if ((v & 0x7ff0000000000000ull) == 0x7ff0000000000000ull && (v & 0x000fffffffffffffull) &&
        !(v & 0x0008000000000000ull))
        v |= 0x0008000000000000ull;
    memcpy(dst, &v, 8);
}
// fld qword; fcomp qword [0.0]; fnstsw; test ah, 0x40: C3 is set for equal (+0.0 or -0.0) and for unordered (a NaN)
bool c3fn_zeroOrNan(const void* p) {
    double d;
    memcpy(&d, p, 8);
    return d == 0.0 || d != d;
}

// 0x000316c0  loadWaveResource (VA 0x100316c0). __thiscall, one stack argument: a WAVEHDR (its lpData at +0; caller
// WaveInDevice_requeueBuffer 0x100393d0 passes the header it just unprepared, 0x10039402), `ret 4`. A NULL header returns 0xa
// (0x100316ce / 0x100316d2); a count +0x68 above 0x64 (unsigned, `jbe` 0x100316e2) returns 0x10. A zeroed MMIOINFO on the
// stack gets fccIOProc 'MEM ' (0x10031701) and cchBuffer 0x36 (0x10031709) and is opened with mmioOpenA(NULL, &info,
// 0x11002 = MMIO_ALLOCBUF | MMIO_CREATE | MMIO_READWRITE) (0x1003172f); the handle goes into a 4-byte cell from operator
// new (0x10031719, stored at 0x10031737 before the test, so a failed new faults there). A NULL handle returns 6 (0x1003173d)
// and leaks the cell. Otherwise g_openMmioCount 0x100b4a08 is raised (0x10031752) and mmioGetInfo(h, &info, 0) runs
// (0x10031760). The 180 words of lpData are converted to floats into the module table 0x100b515c..0x100b542c (fild /
// fmul [0x1005c5e0] = 2^-15 / fstp dword, 0x1003176d-0x1003178c: exact), codecProcessResource 0x1003b150 encodes them
// (cdecl: table, the 54-dword module array 0x100b5084, the encoder context +0x90; 0x1003179f), and the 54 dwords are
// packed as bits, most significant first, one bit per dword (non-zero = 1), into the MMIOINFO's buffer: a byte is stored
// after every 8th dword and after the last (0x100317b4-0x100317d7), 7 bytes. +0x80 is raised (0x100317e6), pchEndRead =
// pchBuffer + 7 (0x100317f4), mmioSetInfo(h, &info, 0) (0x100317fb), and the cell is appended to the list +0x5c by
// ResourceList_pushBack (thunk 0x10001cb7, 0x10031805). Returns 0 (0x1003180c).
typedef unsigned(__fastcall* LoadWaveRes_t)(void*, void*, void*);
LoadWaveRes_t LoadWaveRes_orig;
unsigned __fastcall LoadWaveRes_re(void* self, void*, void* hdr) {
    if (!hdr) return 0xa;                                                           // 0x100316ce / 0x100316d2
    if (fat<unsigned>(self, 0x68) > 0x64) return 0x10;                              // 0x100316de / 0x100316e2
    unsigned info[0x12];
    memset(info, 0, sizeof info);                                                   // 0x100316ff
    info[1] = 0x204d454d;                                                           // fccIOProc 'MEM ' 0x10031701
    info[5] = 0x36;                                                                 // cchBuffer 0x10031709
    info[0xc] = 0;                                                                  // adwInfo[0] 0x10031711
    info[6] = 0;                                                                    // pchBuffer 0x10031715
    void** cell = static_cast<void**>(c3fn_new(4));                                 // 0x10031719
    void* h = c3fn_mmioOpen(0, info, 0x11002);                                      // 0x1003172f
    *cell = h;                                                                      // 0x10031737
    if (!h) return 6;                                                               // 0x10031739 / 0x1003173d
    ++c3fn_g<unsigned>(kOpenMmioCount);                                             // 0x10031749-0x10031752
    c3fn_mmioGetInfo(*cell, info, 0);                                               // 0x10031760
    const short* src = *static_cast<short* const*>(hdr);                            // 0x10031766
    float* tab = &c3fn_g<float>(0x000b515c);
    const float k = c3fn_g<float>(0x0005c5e0);                                      // 2^-15
    for (int i = 0; i < 180; i++)                                                   // 0x1003176d-0x1003178c
        tab[i] = static_cast<float>(static_cast<double>(src[i]) * static_cast<double>(k));
    c3fn_fn<Cd3_t>(0x0003b150)(c3fn_u(tab), c3fn_u(&c3fn_g<unsigned>(0x000b5084)), fat<unsigned>(self, 0x90));   // 0x1003179f
    unsigned char* out = reinterpret_cast<unsigned char*>(info[6]);                 // [esp+0x30] = pchBuffer, 0x100317a4
    const unsigned* bits = &c3fn_g<unsigned>(0x000b5084);
    unsigned char acc = 0, mask = 0x80;                                             // 0x100317ab / 0x100317ad
    for (int i = 0; i < 54; i++) {                                                  // 0x100b5084 .. 0x100b5158
        if (bits[i]) acc |= mask;                                                   // 0x100317b4 / 0x100317b9
        mask >>= 1;                                                                 // 0x100317bb
        if (mask == 0 || i == 53) {                                                 // 0x100317bd / 0x100317bf
            *out++ = acc;                                                           // 0x100317c7
            acc = 0;
            mask = 0x80;
        }
    }
    ++fat<unsigned>(self, 0x80);                                                    // 0x100317d9-0x100317e6
    info[8] = info[6] + 7;                                                          // pchEndRead 0x100317ec-0x100317f4
    c3fn_mmioSetInfo(*cell, info, 0);                                               // 0x100317fb
    c3fn_fn<Th1_t>(0x00001cb7)(&fat<char>(self, 0x5c), 0, c3fn_u(cell));            // 0x10031805
    return 0;                                                                       // 0x1003180c
}

// 0x00037920  flushMmioBuffer (VA 0x10037920). __thiscall, no stack argument (plain `ret`). With +0x58 bit 1 clear, sets bit 0
// and returns 0 (0x1003792f-0x10037938). With bit 6 set and bit 4 clear, sets bit 4 (0x1003794b) and in the 16 voice records
// (stride 0x110) clears bit 1 and sets bit 2 of the byte +0x2d4 (0x10037954-0x10037968). Then up to 0xffff times, while bit 6
// is set and bit 4 clear, Sleep(1) (KERNEL32 [0x100b840c], 0x10037970-0x10037982), and returns 6 if that still holds
// (0x10037984-0x10037991). On one thread the wait never runs: the store at 0x1003794b has set bit 4 whenever bit 6 was set
// with bit 4 clear. Then on the HMMIO +0 with the MMIOINFO +0xc: mmioGetInfo (0x100379a9), mmioSetBuffer(h, NULL, +0x178, 0)
// (0x100379b7), mmioGetInfo (0x100379c2), mmioSeek(h, +0x174, SEEK_SET) (0x100379cf), mmioGetInfo (0x100379da),
// mmioAdvance(h, +0xc, 0) (0x100379e1). A decode buffer +8 that is set and is not this+0xc is freed with operator delete and
// replaced by this+0xc (0x100379e7-0x100379fb). +0x58 |= 1 (0x10037a0e). Each of the 16 voice records (+0x2c4, stride 0x110)
// gets a copy of the MMIOINFO [+8] when it is set (rep movsd 0x12 dwords at rec - 0x8c, 0x10037a17-0x10037a29), rec - 4 =
// rec = rec + 4 = 0, the 0x12-byte format +0x180 at rec + 0x14 (0x10037a37-0x10037a5b), rec + 4 = +0x168, rec = +0x164,
// rec - 4 = 0 (0x10037a65-0x10037a77) and bit 1 cleared / bit 2 set in its byte rec + 0x10 (0x10037a7d-0x10037a8a). Returns 0.
typedef unsigned(__fastcall* Flush_t)(void*, void*);
Flush_t Flush_orig;
unsigned __fastcall Flush_re(void* self, void*) {
    unsigned f = fat<unsigned>(self, 0x58);                                         // 0x10037925
    if (!(f & 2)) {                                                                 // 0x1003792f
        fat<unsigned>(self, 0x58) = f | 1;                                          // 0x10037931 / 0x10037934
        return 0;
    }
    if ((f & 0x40) && !(f & 0x10)) {                                                // 0x1003793c / 0x10037940
        fat<unsigned>(self, 0x58) = f | 0x10;                                       // 0x1003794b
        for (int k = 0; k < 16; k++) {                                              // 0x10037954-0x10037968
            unsigned char& b = fat<unsigned char>(self, 0x2d4 + 0x110 * k);
            b = static_cast<unsigned char>((b & 0xfd) | 4);
        }
    }
    for (unsigned n = 0x10000;;) {                                                  // edi 0x10037928
        f = fat<unsigned>(self, 0x58);                                              // 0x10037970
        if (!(f & 0x40) || (f & 0x10)) break;                                       // 0x10037975 / 0x10037979
        if (--n == 0) break;                                                        // 0x1003797b / 0x1003797c
        c3fn_sleep(1);                                                              // 0x10037980
    }
    f = fat<unsigned>(self, 0x58);                                                  // 0x10037984
    if ((f & 0x40) && !(f & 0x10)) return 6;                                        // 0x10037989-0x10037991
    void* mi = &fat<char>(self, 0xc);
    c3fn_mmioGetInfo(fat<void*>(self, 0), mi, 0);                                   // 0x100379a9
    c3fn_mmioSetBuffer(fat<void*>(self, 0), 0, fat<long>(self, 0x178), 0);          // 0x100379b7
    c3fn_mmioGetInfo(fat<void*>(self, 0), mi, 0);                                   // 0x100379c2
    c3fn_mmioSeek(fat<void*>(self, 0), fat<long>(self, 0x174), 0);                  // 0x100379cf
    c3fn_mmioGetInfo(fat<void*>(self, 0), mi, 0);                                   // 0x100379da
    c3fn_mmioAdvance(fat<void*>(self, 0), mi, 0);                                   // 0x100379e1
    void* b = fat<void*>(self, 8);                                                  // 0x100379e7
    if (b && b != mi) {                                                             // 0x100379ec / 0x100379f0
        c3fn_delete(b);                                                             // 0x100379f3
        fat<void*>(self, 8) = mi;                                                   // 0x100379fb
    }
    fat<unsigned>(self, 0x58) |= 1;                                                 // 0x10037a07 / 0x10037a0e
    char* rec = &fat<char>(self, 0x2c4);                                            // 0x10037a11
    for (int k = 0; k < 16; k++, rec += 0x110) {                                    // ebp = 0x10, 0x10037a89 / 0x10037a90
        const void* m = fat<void*>(self, 8);                                        // 0x10037a17
        if (m) memcpy(rec - 0x8c, m, 0x48);                                         // 0x10037a1c-0x10037a29
        fat<unsigned>(rec, -4) = 0;                                                 // 0x10037a2f
        fat<unsigned>(rec, 0) = 0;                                                  // 0x10037a32
        fat<unsigned>(rec, 4) = 0;                                                  // 0x10037a34
        memcpy(rec + 0x14, &fat<char>(self, 0x180), 0x10);                          // 0x10037a37-0x10037a52
        fat<unsigned short>(rec, 0x24) = fat<unsigned short>(self, 0x190);          // 0x10037a55 / 0x10037a5b
        fat<unsigned>(rec, 4) = fat<unsigned>(self, 0x168);                         // 0x10037a5f / 0x10037a65
        fat<unsigned>(rec, 0) = fat<unsigned>(self, 0x164);                         // 0x10037a6b / 0x10037a71
        fat<unsigned>(rec, -4) = 0;                                                 // 0x10037a77
        rec[0x10] = static_cast<char>((rec[0x10] & 0xfd) | 4);                      // 0x10037a7d-0x10037a8a
    }
    return 0;                                                                       // 0x10037a95
}

// 0x00035ca0  closeMmioSound (VA 0x10035ca0). __thiscall, no stack argument (plain `ret` 0x10035ec6), returns 0 (0x10035ec3).
// The pending stream record S = +0x1390 (read without a NULL test at 0x10035ca7): its voice V = S+0xb4 (0x10035cad), its HMMIO
// S+0x44 closed with mmioClose(h, 0) (0x10035cba), its buffer S+0x18 freed with free (0x10035cc1), g_openMmioCount lowered
// (0x10035cd5), and the g_openSoundList node whose value is S (+0x1390 re-read at 0x10035cdb) unlinked and freed
// (c3fn_unlinkOpenSound). With V set (0x10035d49): when +0x58 bit 3 is set (0x10035d56) V's byte +0x9c takes S's bit 4
// (0x10035d62-0x10035d75), the doubles S+0xf0 and S+0x58 are copied to V+0xf0 / V+0xf8 and V+0x58 / V+0x60
// (0x10035d81-0x10035d9f), Voice_setPanGains (thunk 0x1000231a) runs on V with +0x224 (0x10035dab), and V's byte +0x9c gets
// bit 3 when neither V+0x58 nor V+0xf0 is 0.0 or a NaN (fcomp [0x1005b498] = 0.0 with `test ah, 0x40`, 0x10035db3-0x10035dd3),
// else bit 2 with V's byte +0x94 bit 0 cleared and V+0xe4 = 0 (0x10035ddb-0x10035df8); with bit 3 clear (also re-tested at
// 0x10035e05, after the call) fourteen dwords of this are copied into V (0x10035e0b-0x10035e98). Then S (+0x1390 re-read,
// 0x10035e9b) is freed with operator delete when set, +0x1390 = V (0x10035eb4) and +0x1398 is lowered (0x10035ebb).
typedef unsigned(__fastcall* CloseMmio_t)(void*, void*);
CloseMmio_t CloseMmio_orig;
unsigned __fastcall CloseMmio_re(void* self, void*) {
    char* s = fat<char*>(self, 0x1390);                                             // 0x10035ca7
    char* v = fat<char*>(s, 0xb4);                                                  // 0x10035cad
    void* buf = fat<void*>(s, 0x18);                                                // 0x10035cb3
    c3fn_mmioClose(fat<void*>(s, 0x44), 0);                                         // 0x10035cba
    c3fn_free(buf);                                                                 // 0x10035cc1
    --c3fn_g<unsigned>(kOpenMmioCount);                                             // 0x10035cc6-0x10035cd5
    c3fn_unlinkOpenSound(fat<void*>(self, 0x1390));                                 // 0x10035cdb-0x10035d42
    if (v) {                                                                        // 0x10035d47 / 0x10035d49
        bool copy = true;
        if (fat<unsigned char>(self, 0x58) & 8) {                                   // 0x10035d4f-0x10035d56
            char* s2 = fat<char*>(self, 0x1390);                                    // 0x10035d5c
            unsigned char a = fat<unsigned char>(v, 0x9c);                          // 0x10035d62
            fat<unsigned char>(v, 0x9c) = static_cast<unsigned char>((a & ~0x10) | (fat<unsigned char>(s2, 0x9c) & 0x10));   // 0x10035d68-0x10035d75
            c3fn_x87Copy(&fat<char>(v, 0xf0), &fat<char>(fat<char*>(self, 0x1390), 0xf0));   // 0x10035d81 / 0x10035d87
            c3fn_x87Copy(&fat<char>(v, 0xf8), &fat<char>(v, 0xf0));                 // 0x10035d8d (fstp of the same register)
            c3fn_x87Copy(&fat<char>(v, 0x58), &fat<char>(fat<char*>(self, 0x1390), 0x58));   // 0x10035d99 / 0x10035d9c
            c3fn_x87Copy(&fat<char>(v, 0x60), &fat<char>(v, 0x58));                 // 0x10035d9f
            c3fn_fn<Th1_t>(0x0000231a)(v, 0, fat<unsigned>(self, 0x224));           // 0x10035dab
            if (!c3fn_zeroOrNan(&fat<char>(v, 0x58)) && !c3fn_zeroOrNan(&fat<char>(v, 0xf0))) {   // 0x10035db0-0x10035dd1
                fat<unsigned char>(v, 0x9c) |= 8;                                   // 0x10035dd3
            } else {
                fat<unsigned char>(v, 0x9c) |= 4;                                   // 0x10035de7 / 0x10035dec
                fat<unsigned char>(v, 0x94) &= 0xfe;                                // 0x10035dea / 0x10035df2
                fat<unsigned>(v, 0xe4) = 0;                                         // 0x10035df8
            }
            copy = !(fat<unsigned char>(self, 0x58) & 8);                           // 0x10035e02 / 0x10035e05
        }
        if (copy) {                                                                 // 0x10035e0b-0x10035e98
            fat<unsigned>(v, 0x58) = fat<unsigned>(self, 0x1e0);
            fat<unsigned>(v, 0x5c) = fat<unsigned>(self, 0x1e4);
            fat<unsigned>(v, 0xf0) = fat<unsigned>(self, 0x1c8);
            fat<unsigned>(v, 0xf4) = fat<unsigned>(self, 0x1cc);
            fat<unsigned>(v, 0x80) = fat<unsigned>(self, 0x1f0);
            fat<unsigned>(v, 0x84) = fat<unsigned>(self, 0x1f4);
            fat<unsigned>(v, 0x70) = fat<unsigned>(self, 0x200);
            fat<unsigned>(v, 0x74) = fat<unsigned>(self, 0x204);
            fat<unsigned>(v, 0x78) = fat<unsigned>(self, 0x1e8);
            fat<unsigned>(v, 0x68) = fat<unsigned>(self, 0x1f8);
            fat<unsigned>(v, 0x48) = fat<unsigned>(self, 0x224);
            fat<unsigned>(v, 0xf8) = fat<unsigned>(self, 0x1c8);
            fat<unsigned>(v, 0xfc) = fat<unsigned>(self, 0x1cc);
            fat<unsigned>(v, 0x50) = fat<unsigned>(self, 0x1d8);
        }
    }
    if (fat<void*>(self, 0x1390)) c3fn_delete(fat<void*>(self, 0x1390));           // 0x10035e9b-0x10035ea6
    fat<char*>(self, 0x1390) = v;                                                   // 0x10035eb4
    --fat<unsigned>(self, 0x1398);                                                  // 0x10035eae-0x10035ebb
    return 0;                                                                       // 0x10035ec3
}

// 0x00030840  openMmioSound (VA 0x10030840). __thiscall, one stack argument R (`ret 4`; [esp+0x410] at 0x1003085d): a record
// whose +0x18 points at packed bits and whose +0x20 - +0x18 is a byte count. Returns 0xa for a NULL R (0x10030869), 0x13 when
// the decoder context +0xd4 is NULL (0x1003087d). A zeroed MMIOINFO on the stack gets fccIOProc 'MEM ' (0x1003089f),
// cchBuffer 0x168 (0x100308aa), adwInfo[0] 0x64 (0x100308b2) and is opened with mmioOpenA(NULL, &info, MMIO_CREATE 0x1000)
// (0x100308ba); NULL returns 8 (0x100308c8). g_openMmioCount is raised (0x100308e2), mmioGetInfo(h, &info, 0) (0x100308e8).
// The first 54 bits at R+0x18, most significant first, become 54 dwords 0 / 1 on the stack (0x100308fd-0x10030930; a new
// byte is loaded at every multiple of 8), codecProcessBlock 0x1003a980 decodes them into 180 floats (cdecl: bits, floats,
// +0xd4; 0x10030946), and each float times 32768.0f ([0x1005c524], fst dword 0x10030966) is stored as a word into the
// MMIOINFO's buffer: 0x8000 when below -32768.0 ([0x1005c518]) or unordered (`test ah, 1` 0x10030972), 0x7fff when above
// 32767.0 ([0x1005c508], `test ah, 0x41` 0x1003098a), else __ftol 0x10042550's truncation (0x1003099a). pchNext = pchBuffer,
// pchEndRead = pchBuffer + 0x168 (0x100309b3 / 0x100309c6), mmioSetInfo(h, &info, 0) (0x100309ca). A 0x110-byte voice from
// operator new (0x100309d5) is initialised by Voice_resetDefaults (thunk 0x10001e56, 0x100309ee) and MmioBuffer_init (thunk
// 0x100017a8: &info, the format +0xbc; 0x10030a12). The old tail node +0x60 is kept in +0x64 (0x10030a1c) and its voice P
// remembered (0x10030a21); a 0xc-byte node {0, 0, voice} (operator new 0x10030a2a) is appended to the list +0x5c / +0x60 and
// +0x68 raised (0x10030a42-0x10030a64). The voice gets +0x90 = (R+0x20 - R+0x18) / its word +0xac (unsigned, 0x10030a7a),
// +0xd4 = its +0x20, +0xd8 = its +0x18 (0x10030a81-0x10030a90). P, when set (0x10030a96), gets +0x98 |= 1, byte +0x9c |= 1,
// +0xb4 = the new voice, +0xcc = 0 (0x10030a98-0x10030abf). +0xe0 is raised (0x10030acd); returns 0.
typedef unsigned(__fastcall* OpenMmio_t)(void*, void*, void*);
OpenMmio_t OpenMmio_orig;
unsigned __fastcall OpenMmio_re(void* self, void*, void* rp) {
    char* r = static_cast<char*>(rp);
    if (!r) return 0xa;                                                             // 0x10030869 / 0x1003086b
    if (!fat<void*>(self, 0xd4)) return 0x13;                                       // 0x1003087d / 0x1003087f
    unsigned info[0x12];
    memset(info, 0, sizeof info);                                                   // 0x10030899
    info[1] = 0x204d454d;                                                           // fccIOProc 'MEM ' 0x1003089f
    info[5] = 0x168;                                                                // cchBuffer 0x100308aa
    info[0xc] = 0x64;                                                               // adwInfo[0] 0x100308b2
    void* h = c3fn_mmioOpen(0, info, 0x1000);                                       // 0x100308ba
    if (!h) return 8;                                                               // 0x100308c6 / 0x100308c8
    ++c3fn_g<unsigned>(kOpenMmioCount);                                             // 0x100308d3-0x100308e2
    c3fn_mmioGetInfo(h, info, 0);                                                   // 0x100308e8
    int bits[54];
    const signed char* p = fat<const signed char*>(r, 0x18);                        // 0x100308ee
    int cur = 0;
    for (int i = 0; i < 54; i++) {                                                  // 0x100308fd-0x10030930
        if ((i & 7) == 0) cur = *p++;                                               // 0x1003090c / 0x1003090e
        bits[i] = (cur & (0x80 >> (i & 7))) ? 1 : 0;                                // 0x10030912-0x10030929
    }
    float fl[180];
    c3fn_fn<Cd3_t>(0x0003a980)(c3fn_u(bits), c3fn_u(fl), fat<unsigned>(self, 0xd4));   // 0x10030946
    short* out = reinterpret_cast<short*>(info[6]);                                 // [esp+0x3c] = pchBuffer, 0x1003094b
    const float scale = c3fn_g<float>(0x0005c524);                                  // 32768.0f
    const double lo = c3fn_g<double>(0x0005c518), hi = c3fn_g<double>(0x0005c508);  // -32768.0, 32767.0
    for (int i = 0; i < 180; i++) {                                                 // ebx = 0xb4, 0x100309a8 / 0x100309a9
        const float t = fl[i] * scale;                                              // 0x1003095e-0x10030966 (exact)
        const double d = t;
        if (d < lo || d != d) out[i] = static_cast<short>(0x8000);                  // 0x1003096a-0x10030977
        else if (d > hi) out[i] = 0x7fff;                                           // 0x10030982-0x1003098f
        else out[i] = static_cast<short>(static_cast<int>(d));                      // __ftol 0x1003099a (truncation)
    }
    info[7] = info[6];                                                              // pchNext 0x100309ab / 0x100309b3
    info[8] = info[6] + 0x168;                                                      // pchEndRead 0x100309b9 / 0x100309c6
    c3fn_mmioSetInfo(h, info, 0);                                                   // 0x100309ca
    char* v = static_cast<char*>(c3fn_new(0x110));                                  // 0x100309d5
    if (v) v = reinterpret_cast<char*>(c3fn_fn<Th0_t>(0x00001e56)(v, 0));          // 0x100309e1-0x100309f3
    c3fn_fn<Th2_t>(0x000017a8)(v, 0, c3fn_u(info), c3fn_u(&fat<char>(self, 0xbc)));   // 0x10030a12
    ListNode* tail = fat<ListNode*>(self, 0x60);                                    // 0x10030a17
    fat<ListNode*>(self, 0x64) = tail;                                              // 0x10030a1c
    char* prev = tail ? static_cast<char*>(tail->value) : 0;                        // 0x10030a1f-0x10030a26
    ListNode* n = static_cast<ListNode*>(c3fn_new(0xc));                            // 0x10030a2a
    if (n) { n->prev = 0; n->next = 0; n->value = v; }                              // 0x10030a36-0x10030a3b
    if (fat<ListNode*>(self, 0x60)) {                                               // 0x10030a42 / 0x10030a47
        fat<ListNode*>(self, 0x60)->next = n;                                       // 0x10030a49
        n->prev = fat<ListNode*>(self, 0x60);                                       // 0x10030a4f
    } else {
        fat<ListNode*>(self, 0x5c) = n;                                             // 0x10030a53
    }
    fat<ListNode*>(self, 0x60) = n;                                                 // 0x10030a5d
    ++fat<unsigned>(self, 0x68);                                                    // 0x10030a60-0x10030a64
    const unsigned q = (fat<unsigned>(r, 0x20) - fat<unsigned>(r, 0x18)) / fat<unsigned short>(v, 0xac);   // 0x10030a67-0x10030a7a
    fat<unsigned>(v, 0xd4) = fat<unsigned>(v, 0x20);                                // 0x10030a7c / 0x10030a81
    fat<unsigned>(v, 0x90) = q;                                                     // 0x10030a87
    fat<unsigned>(v, 0xd8) = fat<unsigned>(v, 0x18);                                // 0x10030a8d / 0x10030a90
    if (prev) {                                                                     // 0x10030a96
        fat<unsigned>(prev, 0x98) |= 1;                                             // 0x10030a98-0x10030aa5
        fat<unsigned char>(prev, 0x9c) |= 1;                                        // 0x10030aab-0x10030ab3
        fat<char*>(prev, 0xb4) = v;                                                 // 0x10030ab9
        fat<unsigned>(prev, 0xcc) = 0;                                              // 0x10030abf
    }
    ++fat<unsigned>(self, 0xe0);                                                    // 0x10030ac5-0x10030acd
    return 0;                                                                       // 0x10030ad3
}

// 0x00030ba0  streamWaveSamples (VA 0x10030ba0). __thiscall, four stack arguments (`ret 0x10`): out (a buffer of n stereo
// double pairs, 8 bytes per sample counted by n), n, a byte size, *written. A NULL device global [0x100b49f4] returns 0x13
// (0x10030bb6). A size of 0xc or more (unsigned, 0x10030bcc) selects the block renderer and the clearing (flag F). With +0x58
// bit 3 (0x10030bda) the stream list +0x5c (head) / +0x60 (tail) / +0x68 (count) is drained (0x10030be0-0x10030ce1): each
// front node is freed; its stream S, while non-NULL, has its HMMIO S+0x44 closed when set (0x10030c23) and its
// g_openSoundList node unlinked, g_openMmioCount lowered (0x10030c99, also when S+0x44 is 0), VoiceSlot_dtorEmpty (thunk
// 0x10001b45) and operator delete on S; then the virtual +0x80 of this runs (0x10030ceb). The head (kept in +0x64,
// 0x10030cf6) and its stream S give the work: none (0x10030cf9 / 0x10030d00) stores *written = 0, clears n * 8 bytes of out
// when F (0x10030d18-0x10030d29), clears +0x58 bit 0 when bit 5 is set (0x10030d2e-0x10030d34) and returns 0x26. Otherwise
// streamRenderBlock (F, thunk 0x10001640) or streamRenderTail (thunk 0x10001933) renders (cdecl: out, &pos, n, S, 0, with pos
// = S+0x1c in the size argument's slot, 0x10030d46-0x10030d6d), *written = its result (0x10030d7d). S+0xcc front nodes are then
// popped (0x10030d8d-0x10030df7): each stream closes its HMMIO (no NULL test, 0x10030dc9), lowers g_openMmioCount,
// VoiceSlot_dtorEmpty and operator delete (no g_openSoundList unlink). After a pop the stream S is the new head's (+0x64
// updated, 0x10030e02; none -> 0x10030e91). With S: S+0x1c = pos (0x10030e16); when pos >= S+0x20 (unsigned) and S's byte +0x9c
// bit 0 is clear (0x10030e20 / 0x10030e29): +0x58 bit 4 cleared and bit 2 set (0x10030e2b-0x10030e36), the virtual +0x20 of
// this (0x10030e39), the front node popped (its value not used, 0x10030e3c-0x10030e64), S's HMMIO closed, g_openMmioCount
// lowered, VoiceSlot_dtorEmpty and operator delete on S (0x10030e67-0x10030e87). Without S, +0x58 bit 0 is cleared when bit
// 5 is set (0x10030e91-0x10030e9a). Last, when the result differs from n and F (0x10030ea7 / 0x10030ead), (n - result) * 16
// bytes are cleared from out + result * 8 (0x10030eb3-0x10030ec9: twice the tail the first clear covers). Returns 0.
typedef unsigned(__fastcall* Stream_t)(void*, void*, void*, unsigned, unsigned, unsigned*);
Stream_t Stream_orig;
// close and free one stream record as the three drains do (0x10030dc3-0x10030de4 / 0x10030e67-0x10030e87)
void c3fn_dropStream(char* s) {
    c3fn_mmioClose(fat<void*>(s, 0x44), 0);
    --c3fn_g<unsigned>(kOpenMmioCount);
    c3fn_fn<Th0_t>(0x00001b45)(s, 0);
    c3fn_delete(s);
}
unsigned __fastcall Stream_re(void* selfp, void*, void* out, unsigned n, unsigned size, unsigned* written) {
    char* self = static_cast<char*>(selfp);
    if (!c3fn_g<void*>(0x000b49f4)) return 0x13;                                    // 0x10030ba3 / 0x10030bb6
    const bool f = size >= 0xc;                                                     // 0x10030bc7-0x10030bce
    if (fat<unsigned char>(self, 0x58) & 8) {                                       // 0x10030bd6 / 0x10030bda
        while (fat<ListNode*>(self, 0x5c)) {                                        // 0x10030be5 / 0x10030cb6
            char* s = static_cast<char*>(c3fn_popFront(self));                      // 0x10030beb-0x10030c11 / 0x10030cb8-0x10030cde
            if (!s) break;                                                          // 0x10030c14 / 0x10030ce1
            if (fat<void*>(s, 0x44)) {                                              // 0x10030c1a / 0x10030c1f
                c3fn_mmioClose(fat<void*>(s, 0x44), 0);                             // 0x10030c23
                c3fn_unlinkOpenSound(s);                                            // 0x10030c29-0x10030c8b
            }
            --c3fn_g<unsigned>(kOpenMmioCount);                                     // 0x10030c90-0x10030c99
            c3fn_fn<Th0_t>(0x00001b45)(s, 0);                                       // 0x10030ca3
            c3fn_delete(s);                                                         // 0x10030ca9
        }
        c3fn_vfn<Th0_t>(self, 0x80)(self, 0);                                       // 0x10030ceb
    }
    ListNode* head = fat<ListNode*>(self, 0x5c);                                    // 0x10030cf1
    fat<ListNode*>(self, 0x64) = head;                                              // 0x10030cf6
    char* s = head ? static_cast<char*>(head->value) : 0;                           // 0x10030cf9-0x10030d00
    if (!s) {
        *written = 0;                                                               // 0x10030d06
        if (f) memset(out, 0, n * 8);                                               // 0x10030d0e-0x10030d29
        if (fat<unsigned char>(self, 0x58) & 0x20) fat<unsigned>(self, 0x58) &= ~1u;   // 0x10030d2e-0x10030d34
        return 0x26;                                                                // 0x10030d3a
    }
    unsigned pos = fat<unsigned>(s, 0x1c);                                          // 0x10030d46 / 0x10030d4d
    const unsigned got = c3fn_fn<Cd5_t>(f ? 0x00001640 : 0x00001933)(c3fn_u(out), c3fn_u(&pos), n, c3fn_u(s), 0);   // 0x10030d66 / 0x10030d6d
    *written = got;                                                                 // 0x10030d7d
    const unsigned drops = fat<unsigned>(s, 0xcc);                                  // 0x10030d7f
    if (drops) {                                                                    // 0x10030d85 / 0x10030d8b
        for (unsigned k = drops; k; k--) {                                          // 0x10030df2 / 0x10030df7
            if (!fat<ListNode*>(self, 0x5c)) continue;                              // 0x10030d94 / 0x10030d96
            char* d = static_cast<char*>(c3fn_popFront(self));                      // 0x10030d98-0x10030dbe
            if (d) c3fn_dropStream(d);                                              // 0x10030dc1-0x10030de4
        }
        head = fat<ListNode*>(self, 0x5c);                                          // 0x10030dfd
        fat<ListNode*>(self, 0x64) = head;                                          // 0x10030e02
        s = head ? static_cast<char*>(head->value) : 0;                             // 0x10030e05 / 0x10030e0b
    }
    if (s) {                                                                        // 0x10030e0e / 0x10030e10
        fat<unsigned>(s, 0x1c) = pos;                                               // 0x10030e16
        if (pos >= fat<unsigned>(s, 0x20) && !(fat<unsigned char>(s, 0x9c) & 1)) {  // 0x10030e20 / 0x10030e29
            fat<unsigned>(self, 0x58) = (fat<unsigned>(self, 0x58) & ~0x10u) | 4;   // 0x10030e2b-0x10030e36
            c3fn_vfn<Th0_t>(self, 0x20)(self, 0);                                   // 0x10030e39
            if (fat<ListNode*>(self, 0x5c)) c3fn_popFront(self);                    // 0x10030e3c-0x10030e64
            c3fn_dropStream(s);                                                     // 0x10030e67-0x10030e87
        }
    } else if (fat<unsigned char>(self, 0x58) & 0x20) {                             // 0x10030e91-0x10030e96
        fat<unsigned>(self, 0x58) &= ~1u;                                           // 0x10030e98 / 0x10030e9a
    }
    if (got != n && f)                                                              // 0x10030ea5-0x10030ead
        memset(static_cast<char*>(out) + got * 8, 0, (n - got) * 16);               // 0x10030eb3-0x10030ec9
    return 0;                                                                       // 0x10030ece
}

// ---- the two Standard MIDI File loaders ---------------------------------------------------------------------------------
// Both read the header chunk into the device (big-endian fields swapped byte by byte through a stack copy) and build one
// 0x464-byte track reader per track. Shared pieces below; the per-function differences are in each body.
const unsigned kStrMThd = 0x00063d90, kStrMTrk = 0x00063e08;
uint32_t c3fn_be32(const unsigned char* b) { return (uint32_t(b[0]) << 24) | (uint32_t(b[1]) << 16) | (uint32_t(b[2]) << 8) | b[3]; }
// mmioRead of n bytes into the device field `off`, then the field = those n bytes as a big-endian number (n = 4 or 2; for 2
// the word is zero-extended into the dword: `and eax, 0xffff`)
void c3fn_readBe(char* self, void* h, int off, int n) {
    c3fn_mmioRead(h, &fat<char>(self, off), n);
    const unsigned char* b = reinterpret_cast<unsigned char*>(&fat<char>(self, off));
    fat<uint32_t>(self, off) = n == 4 ? c3fn_be32(b) : ((uint32_t(b[0]) << 8) | b[1]);
}
// the header after the tag (loadFile 0x1001d900-0x1001d9ed, loadFromMemory 0x1001dd90-0x1001de70): the chunk length
// into +0x150, +0x4c bit 0 set, then format +0x154, track count +0x158 and division +0x15c, each a big-endian word
void c3fn_readMidiHeader(char* self, void* h) {
    c3fn_readBe(self, h, 0x150, 4);                                                 // 0x1001d91a / 0x1001dd9d
    fat<unsigned>(self, 0x4c) |= 1;                                                 // 0x1001d941 / 0x1001ddc4
    c3fn_readBe(self, h, 0x154, 2);                                                 // 0x1001d951 / 0x1001ddd4
    c3fn_readBe(self, h, 0x158, 2);                                                 // 0x1001d985 / 0x1001de08
    c3fn_readBe(self, h, 0x15c, 2);                                                 // 0x1001d9be / 0x1001de41
}
// a new track reader: operator new 0x464, CtrlObject2_ctor (thunk 0x10001e74), the reader vtable 0x1005b68c at +0, then
// +0x34 / +0x38 = 0, 0x80 dwords from +0x58 zeroed, bit 3 of byte +0x38 cleared, +0x2c, +0x3c, +0x4c = 0, +0x48 = 0x10,
// byte +0x30 = 0, +0x50, +0x54, +0x45c, +0x460, +0x458 = 0 (loadFile 0x1001da09-0x1001da7a). NULL when new fails.
char* c3fn_newTrack() {
    char* t = static_cast<char*>(c3fn_new(0x464));
    if (!t) return 0;
    c3fn_fn<Th0_t>(0x00001e74)(t, 0);
    fat<void*>(t, 0) = c3fn_sound(0x0005b68c);
    fat<unsigned>(t, 0x34) = 0;
    fat<unsigned>(t, 0x38) = 0;
    memset(&fat<char>(t, 0x58), 0, 0x200);
    fat<unsigned char>(t, 0x38) &= 0xf7;
    fat<unsigned>(t, 0x2c) = 0;
    fat<unsigned>(t, 0x3c) = 0;
    fat<unsigned>(t, 0x4c) = 0;
    fat<unsigned>(t, 0x48) = 0x10;
    fat<unsigned char>(t, 0x30) = 0;
    fat<unsigned>(t, 0x50) = 0;
    fat<unsigned>(t, 0x54) = 0;
    fat<unsigned>(t, 0x45c) = 0;
    fat<unsigned>(t, 0x460) = 0;
    fat<unsigned>(t, 0x458) = 0;
    return t;
}
// a 0xc-byte node {0, 0, t} appended to the reader list +0x19c (head) / +0x1a0 (tail), count +0x1a8 raised
// (loadFile 0x1001db16-0x1001db5c); a failed new leaves the node NULL
void c3fn_appendTrack(char* self, char* t) {
    ListNode* n = static_cast<ListNode*>(c3fn_new(0xc));
    if (n) { n->prev = 0; n->next = 0; n->value = t; }
    if (fat<ListNode*>(self, 0x1a0)) {
        fat<ListNode*>(self, 0x1a0)->next = n;
        n->prev = fat<ListNode*>(self, 0x1a0);
    } else {
        fat<ListNode*>(self, 0x19c) = n;
    }
    fat<ListNode*>(self, 0x1a0) = n;
    ++fat<unsigned>(self, 0x1a8);
}

// 0x0001d810  Midi_Device::loadFile (VA 0x1001d810). __thiscall, one stack argument: the file name (`ret 4`). The codes of
// `MThd` / `MTrk` come from mmioStringToFOURCCA (0x1001d83c / 0x1001d848; the second is never used). With +0x4c bit 0 set,
// Midi_Device::closeFile (thunk 0x10001014) runs first and a non-zero result is returned (0x1001d859). Then +0 = 0, the
// MMIOINFO +4 is zeroed (0x1001d873), +0x160..+0x16c = 0, the name is copied to +0x50 (0x1001d894-0x1001d8ad) and opened
// with mmioOpenA(+0x50, NULL, 0) into +0 (0x1001d8af); NULL zeroes the MMIOINFO again and returns 8 (0x1001d8c9). A 4-byte
// tag that is not `MThd` (0x1001d8eb) calls Midi_resetFileState (thunk 0x1000194c) and returns 0x12. Otherwise
// g_openMmioCount is raised (0x1001d90f), the header read (c3fn_readMidiHeader), +0x1ac = 0, and for each of the +0x158
// tracks (re-read every round, `jb` 0x1001db9f): a reader (c3fn_newTrack), its 8-byte chunk header read to the stack
// (0x1001da97), the big-endian size swapped (0x1001daa7-0x1001dab4), that many bytes from operator new (0x1001dabd) as the
// reader's data +0x3c and cursor +0x2c, Midi_scanTrack (thunk 0x1000215d) on the reader when the block is not NULL
// (0x1001dae8 / 0x1001daec) BEFORE the data is read, +0x34 |= 4, +0x40 = 0, +0x44 = size (0x1001daf1-0x1001db03), the data read
// (0x1001db10), the reader appended (c3fn_appendTrack), Seq_rewindTracks (thunk 0x10001ca3) and Midi_scanTrack again on it
// (0x1001db62 / 0x1001db69); when there is a previous reader and this one's +0x54 is above the previous one's (unsigned,
// 0x1001db7e), +0x1ac = the track index. Last mmioClose(+0, 0) (0x1001dbaa), g_openMmioCount lowered, +0 = 0, return 0.
// The test of +0x3c with +0x34 bit 2 before the store (0x1001dac4-0x1001dadd) sees the fresh reader's +0x3c = 0 every time.
typedef unsigned(__fastcall* MidiLoad_t)(void*, void*, void*);
MidiLoad_t MidiLoadFile_orig;
unsigned __fastcall MidiLoadFile_re(void* selfp, void*, void* namep) {
    char* self = static_cast<char*>(selfp);
    const unsigned mthd = c3fn_fourcc(c3fn_sound(kStrMThd), 0);                     // 0x1001d83c
    c3fn_fourcc(c3fn_sound(kStrMTrk), 0);                                           // 0x1001d848 (result unused)
    if (fat<unsigned char>(self, 0x4c) & 1) {                                       // 0x1001d84a / 0x1001d84e
        unsigned r = c3fn_fn<Th0_t>(0x00001014)(self, 0);                           // 0x1001d852
        if (r) return r;                                                            // 0x1001d859
    }
    fat<void*>(self, 0) = 0;                                                        // 0x1001d869
    memset(&fat<char>(self, 4), 0, 0x48);                                           // 0x1001d873
    for (int off = 0x160; off <= 0x16c; off += 4) fat<unsigned>(self, off) = 0;    // 0x1001d87c-0x1001d88e
    strcpy(&fat<char>(self, 0x50), static_cast<const char*>(namep));                // 0x1001d894-0x1001d8ad
    void* h = c3fn_mmioOpen(&fat<char>(self, 0x50), 0, 0);                          // 0x1001d8af
    fat<void*>(self, 0) = h;                                                        // 0x1001d8b7
    if (!h) {                                                                       // 0x1001d8ba
        memset(&fat<char>(self, 4), 0, 0x48);                                       // 0x1001d8c7
        return 8;                                                                   // 0x1001d8c9
    }
    unsigned tag;
    c3fn_mmioRead(h, &tag, 4);                                                      // 0x1001d8e1
    if (tag != mthd) {                                                              // 0x1001d8eb / 0x1001d8ed
        c3fn_fn<Th0_t>(0x0000194c)(self, 0);                                        // 0x1001d8f1
        return 0x12;                                                                // 0x1001d8f6
    }
    ++c3fn_g<unsigned>(kOpenMmioCount);                                             // 0x1001d900-0x1001d90f
    c3fn_readMidiHeader(self, fat<void*>(self, 0));
    char* prev = 0;                                                                 // [esp+0x10] 0x1001d9de
    fat<unsigned>(self, 0x1ac) = 0;                                                 // 0x1001d9e7
    for (unsigned i = 0; i < fat<unsigned>(self, 0x158); i++) {                     // 0x1001d9f5 / 0x1001db99
        char* t = c3fn_newTrack();                                                  // 0x1001da09-0x1001da7a
        unsigned char hdr[8];
        c3fn_mmioRead(fat<void*>(self, 0), hdr, 8);                                 // 0x1001da97
        const unsigned size = c3fn_be32(hdr + 4);                                   // 0x1001da99-0x1001dab4
        void* buf = c3fn_new(size);                                                 // 0x1001dabd
        if (fat<void*>(t, 0x3c) && (fat<unsigned char>(t, 0x34) & 4)) {            // 0x1001daca / 0x1001dace
            c3fn_delete(fat<void*>(t, 0x3c));                                       // 0x1001dad5
            fat<void*>(t, 0x3c) = 0;                                                // 0x1001dadd
        }
        fat<void*>(t, 0x3c) = buf;                                                  // 0x1001dae2
        fat<void*>(t, 0x2c) = buf;                                                  // 0x1001dae5
        if (buf) c3fn_fn<Th0_t>(0x0000215d)(t, 0);                                  // 0x1001dae8 / 0x1001daec
        void* data = fat<void*>(t, 0x3c);                                           // 0x1001daf4
        fat<unsigned>(t, 0x40) = 0;                                                 // 0x1001daf9
        fat<unsigned>(t, 0x34) |= 4;                                                // 0x1001daf7 / 0x1001dafc
        fat<unsigned>(t, 0x44) = size;                                              // 0x1001db03
        c3fn_mmioRead(fat<void*>(self, 0), data, static_cast<long>(size));          // 0x1001db10
        c3fn_appendTrack(self, t);                                                  // 0x1001db16-0x1001db5c
        c3fn_fn<Th0_t>(0x00001ca3)(t, 0);                                           // 0x1001db62
        c3fn_fn<Th0_t>(0x0000215d)(t, 0);                                           // 0x1001db69
        if (prev && fat<unsigned>(t, 0x54) > fat<unsigned>(prev, 0x54))            // 0x1001db74 / 0x1001db7e
            fat<unsigned>(self, 0x1ac) = i;                                         // 0x1001db84
        prev = t;                                                                   // 0x1001db95
    }
    c3fn_mmioClose(fat<void*>(self, 0), 0);                                         // 0x1001dbaa
    --c3fn_g<unsigned>(kOpenMmioCount);                                             // 0x1001dbb0-0x1001dbb6
    fat<void*>(self, 0) = 0;                                                        // 0x1001dbbb
    return 0;                                                                       // 0x1001dbbe
}

// 0x0001dce0  Midi_Device::loadFromMemory (VA 0x1001dce0). __thiscall, one stack argument: an HMMIO the caller opened (a
// memory file; Midi_loadMemoryAndPreRoll 0x10020360 calls it), `ret 4`. As loadFile, with these differences: the handle is
// stored at +0 (0x1001dd34) and the MMIOINFO +4 zeroed and then filled by mmioGetInfo(h, +4, 0) (0x1001dd60), no open and
// no g_openMmioCount change; per track the data block is read (0x1001df64) BEFORE it becomes +0x3c / +0x2c and before the
// one Midi_scanTrack (0x1001df8f, when the block is not NULL), +0x40 = 0 is stored after it (0x1001df96), and there is no
// second scan; the reader with the largest +0x54 is tracked: the first reader, and any later one whose +0x54 is above the
// tracked one's (unsigned, 0x1001dff8), becomes the tracked reader and its index goes to +0x1ac (0x1001dffa-0x1001e002); at
// the end +0 = 0 without a close (0x1001e01f). Returns 0, or closeFile's non-zero result, or 0x12 for a wrong tag.
MidiLoad_t MidiLoadMem_orig;
unsigned __fastcall MidiLoadMem_re(void* selfp, void*, void* h) {
    char* self = static_cast<char*>(selfp);
    const unsigned mthd = c3fn_fourcc(c3fn_sound(kStrMThd), 0);                     // 0x1001dd0c
    c3fn_fourcc(c3fn_sound(kStrMTrk), 0);                                           // 0x1001dd16 (result unused)
    if (fat<unsigned char>(self, 0x4c) & 1) {                                       // 0x1001dd18 / 0x1001dd1c
        unsigned r = c3fn_fn<Th0_t>(0x00001014)(self, 0);                           // 0x1001dd20
        if (r) return r;                                                            // 0x1001dd27
    }
    fat<void*>(self, 0) = h;                                                        // 0x1001dd34
    memset(&fat<char>(self, 4), 0, 0x48);                                           // 0x1001dd40
    for (int off = 0x160; off <= 0x16c; off += 4) fat<unsigned>(self, off) = 0;    // 0x1001dd48-0x1001dd5a
    c3fn_mmioGetInfo(fat<void*>(self, 0), &fat<char>(self, 4), 0);                  // 0x1001dd60
    unsigned tag;
    c3fn_mmioRead(fat<void*>(self, 0), &tag, 4);                                    // 0x1001dd77
    if (tag != mthd) {                                                              // 0x1001dd79 / 0x1001dd7d
        c3fn_fn<Th0_t>(0x0000194c)(self, 0);                                        // 0x1001dd81
        return 0x12;                                                                // 0x1001dd86
    }
    c3fn_readMidiHeader(self, fat<void*>(self, 0));
    char* best = 0;                                                                 // [esp+0x10] 0x1001de61
    fat<unsigned>(self, 0x1ac) = 0;                                                 // 0x1001de6a
    for (unsigned i = 0; i < fat<unsigned>(self, 0x158); i++) {                     // 0x1001de78 / 0x1001e013
        char* t = c3fn_newTrack();                                                  // 0x1001de8c-0x1001defd
        unsigned char hdr[8];
        c3fn_mmioRead(fat<void*>(self, 0), hdr, 8);                                 // 0x1001df1a
        const unsigned size = c3fn_be32(hdr + 4);                                   // 0x1001df1c-0x1001df37
        void* buf = c3fn_new(size);                                                 // 0x1001df40
        fat<unsigned>(t, 0x34) |= 4;                                                // 0x1001df45-0x1001df50
        fat<unsigned>(t, 0x44) = size;                                              // 0x1001df57
        c3fn_mmioRead(fat<void*>(self, 0), buf, static_cast<long>(size));           // 0x1001df64
        if (fat<void*>(t, 0x3c) && (fat<unsigned char>(t, 0x34) & 4)) {            // 0x1001df6d / 0x1001df71
            c3fn_delete(fat<void*>(t, 0x3c));                                       // 0x1001df78
            fat<void*>(t, 0x3c) = 0;                                                // 0x1001df80
        }
        fat<void*>(t, 0x3c) = buf;                                                  // 0x1001df85
        fat<void*>(t, 0x2c) = buf;                                                  // 0x1001df88
        if (buf) c3fn_fn<Th0_t>(0x0000215d)(t, 0);                                  // 0x1001df8b / 0x1001df8f
        fat<unsigned>(t, 0x40) = 0;                                                 // 0x1001df96
        c3fn_appendTrack(self, t);                                                  // 0x1001df94-0x1001dfdd
        c3fn_fn<Th0_t>(0x00001ca3)(t, 0);                                           // 0x1001dfe3
        if (!best || fat<unsigned>(t, 0x54) > fat<unsigned>(best, 0x54)) {         // 0x1001dfee / 0x1001dff8
            best = t;                                                               // 0x1001dffe
            fat<unsigned>(self, 0x1ac) = i;                                         // 0x1001e002
        }
    }
    fat<void*>(self, 0) = 0;                                                        // 0x1001e01f
    return 0;                                                                       // 0x1001e022
}

// 0x00033300  fillDecodeRing (VA 0x10033300). __thiscall, no stack argument (plain `ret`). Copies bytes of the sound's
// buffered mmio file (the MMIOINFO +0xc: pchNext +0x28, pchEndRead +0x2c; HMMIO +0) into the decode ring R = [+8] (an
// MMIOINFO: pchBuffer +0x18, pchNext +0x1c = the reader, pchEndRead +0x20, pchEndWrite +0x24 = the ring's end, cchBuffer
// +0x14) at the write cursor W = +0x1378, with +0x170 the file position and +0x178 its end. Returns 0x26 at once when +0x58
// bit 5 (end reached) is set (0x1003330e). The free space is (R.pchNext - 4, plus cchBuffer when that falls below pchBuffer,
// unsigned `jae` 0x1003332b) - W, masked with +0x1374 (0x1003333e), or cchBuffer when that is 0 (0x10033344). The bytes left
// are: with +0x58 bit 2 (looping), no loop table +0x1b8 and +0x13a0 clear, or with bit 2, a loop table and +0x58 bit 7 clear,
// loop end (+0x136c + +0x1368) - position (0x10033367 / 0x10033393); otherwise +0x178 - position (0x100333b9).
// Free space <= bytes left (`jb` 0x100333cb not taken): the free space is filled. It is split at the ring's end into a first
// part at W and a second part at pchBuffer when W + space passes pchEndWrite (unsigned `ja` 0x100333d9); each part is copied
// from pchNext, cut to what the file buffer still holds (pchEndRead - pchNext, `jbe` 0x10033405 / 0x1003345c, the cut marks
// the status 0x2b), pchNext advanced and mmioSetInfo(h, +0xc, 0) called (0x10033434 / 0x1003348d); the second part only when
// the first was not cut (0x10033444-0x10033448). W becomes pchBuffer + second part, or W + first part (0x100334a7-0x100334b7),
// the position grows by the bytes copied (0x100334c2), R.pchEndRead = R.pchEndWrite (0x100334cd), and a cut is followed by
// mmioAdvance(h, +0xc, 0) (0x10033665) to refill the file buffer.
// Free space > bytes left (0x100334db): the last bytes are copied the same way (split at the ring's end, cut at the file
// buffer). With bit 2 set, bit 7 clear and +0x13a0 clear (0x100334df-0x100334f7) the loop arm: after the copy, when nothing
// was cut and all the bytes left were copied (0x10033607), the file seeks back by +0x1368 (SEEK_CUR, 0x10033617),
// mmioGetInfo / mmioAdvance refill it (0x10033626 / 0x10033632) and the position becomes the loop start +0x136c (0x10033646).
// Otherwise (0x10033680) R.pchEndRead = the new W (0x10033790) and +0x58 bit 5 is set when the position reached +0x178
// (0x1003379f-0x100337a6). Every exit after the first clears +0x58 bit 10 (0x1003366f / 0x10033641 / 0x10033787) and returns 0;
// a cut is followed by mmioAdvance on every arm (0x10033665 / 0x1003377e).
struct c3fn_RingCopy { unsigned n1, n2; bool cut, second; };
// copy `n1` bytes to `dst1` and, when `dst2` is set and the first part was not cut, `n2` bytes to `dst2`, each cut to the bytes
// the file buffer holds (status 0x2b), pchNext advanced and mmioSetInfo after each part. Returns the parts as copied.
c3fn_RingCopy c3fn_ringCopy(char* self, char* dst1, unsigned n1, char* dst2, unsigned n2) {
    c3fn_RingCopy r = {n1, n2, false, false};
    unsigned avail = fat<unsigned>(self, 0x2c) - fat<unsigned>(self, 0x28);
    if (r.n1 > avail) { r.n1 = avail; r.cut = true; }
    memcpy(dst1, fat<char*>(self, 0x28), r.n1);
    fat<char*>(self, 0x28) += r.n1;
    c3fn_mmioSetInfo(fat<void*>(self, 0), &fat<char>(self, 0xc), 0);
    if (dst2 && !r.cut) {
        r.second = true;
        avail = fat<unsigned>(self, 0x2c) - fat<unsigned>(self, 0x28);
        if (r.n2 > avail) { r.n2 = avail; r.cut = true; }
        memcpy(dst2, fat<char*>(self, 0x28), r.n2);
        fat<char*>(self, 0x28) += r.n2;
        c3fn_mmioSetInfo(fat<void*>(self, 0), &fat<char>(self, 0xc), 0);
    } else {
        r.n2 = 0;
    }
    return r;
}
typedef unsigned(__fastcall* FillRing_t)(void*, void*);
FillRing_t FillRing_orig;
unsigned __fastcall FillRing_re(void* selfp, void*) {
    char* self = static_cast<char*>(selfp);
    if (fat<unsigned char>(self, 0x58) & 0x20) return 0x26;                         // 0x1003330c / 0x10033313
    char* ring = fat<char*>(self, 8);                                               // 0x1003331d
    char* rd = fat<char*>(ring, 0x1c) - 4;                                          // 0x10033320-0x10033326
    char* start = fat<char*>(ring, 0x18);                                           // 0x10033323
    if (rd < start) rd += fat<unsigned>(ring, 0x14);                                // 0x10033329-0x1003332d
    char* w = fat<char*>(self, 0x1378);                                             // 0x10033330
    unsigned space = static_cast<unsigned>(rd - w) & fat<unsigned>(self, 0x1374);   // 0x1003333c / 0x1003333e
    if (!space) space = fat<unsigned>(ring, 0x14);                                  // 0x10033342 / 0x10033344
    const unsigned f58 = fat<unsigned>(self, 0x58);
    const bool loop = (f58 & 4) != 0;                                               // 0x10033347-0x10033351
    const unsigned loopEnd = fat<unsigned>(self, 0x136c) + fat<unsigned>(self, 0x1368);
    unsigned rem;
    if ((loop && !fat<void*>(self, 0x1b8) && !fat<unsigned>(self, 0x13a0)) ||     // 0x10033353-0x10033367
        (loop && fat<void*>(self, 0x1b8) && !(f58 & 0x80)))                         // 0x10033383-0x10033393
        rem = loopEnd - fat<unsigned>(self, 0x170);
    else
        rem = fat<unsigned>(self, 0x178) - fat<unsigned>(self, 0x170);              // 0x100333b9-0x100333c5
    char* end = fat<char*>(ring, 0x24);
    if (rem >= space) {                                                             // 0x100333c9 / 0x100333cb
        unsigned n1 = space, n2 = 0;
        char* seg2 = 0;
        if (static_cast<unsigned>(c3fn_u(w) + space) > c3fn_u(end)) {               // 0x100333d4-0x100333d9
            n1 = static_cast<unsigned>(end - w);                                    // 0x100333e9
            seg2 = start;                                                           // 0x100333eb
            n2 = space - n1;                                                        // 0x100333ef
        }
        c3fn_RingCopy c = c3fn_ringCopy(self, w, n1, seg2, n2);                     // 0x100333f7-0x1003348d
        fat<char*>(self, 0x1378) = c.second ? fat<char*>(ring, 0x18) + c.n2 : w + c.n1;   // 0x10033493-0x100334b7
        fat<unsigned>(self, 0x170) += c.n1 + c.n2;                                  // 0x100334b1 / 0x100334c2
        fat<unsigned>(ring, 0x20) = fat<unsigned>(ring, 0x24);                      // 0x100334c8 / 0x100334cd
        if (c.cut) c3fn_mmioAdvance(fat<void*>(self, 0), &fat<char>(self, 0xc), 0);   // 0x1003365c-0x10033665
        fat<unsigned>(self, 0x58) &= ~0x400u;                                       // 0x1003366b-0x10033673
        return 0;
    }
    unsigned n1 = rem, n2 = 0;
    char* seg2 = 0;
    if (static_cast<unsigned>(c3fn_u(w) + rem) > c3fn_u(end)) {                     // 0x100334fd-0x10033505 / 0x10033680-0x10033688
        n1 = static_cast<unsigned>(end - w);                                        // 0x10033515 / 0x10033696
        seg2 = start;                                                               // 0x10033519 / 0x10033698
        n2 = rem - n1;                                                              // 0x10033521 / 0x1003369c
    }
    c3fn_RingCopy c = c3fn_ringCopy(self, w, n1, seg2, n2);                         // 0x10033525-0x100335bb / 0x100336a2-0x10033738
    char* nw = c.second ? fat<char*>(ring, 0x18) + c.n2 : w + c.n1;  // 0x100335c1-0x100335e1 / 0x1003373e-0x1003375a
    fat<char*>(self, 0x1378) = nw;                                                  // 0x100335e9 / 0x10033762
    fat<unsigned>(self, 0x170) += c.n1 + c.n2;                                      // 0x100335f3 / 0x1003376d
    if (loop && !(f58 & 0x80) && !fat<unsigned>(self, 0x13a0)) {                    // 0x100334dd-0x100334f7
        if (c.cut) {                                                                // 0x100335f9 / 0x10033657
            c3fn_mmioAdvance(fat<void*>(self, 0), &fat<char>(self, 0xc), 0);        // 0x10033665
        } else if (c.n1 + c.n2 == rem) {                                            // 0x100335fb-0x10033607
            c3fn_mmioSeek(fat<void*>(self, 0), -fat<long>(self, 0x1368), 1);        // 0x10033617
            c3fn_mmioGetInfo(fat<void*>(self, 0), &fat<char>(self, 0xc), 0);        // 0x10033626
            c3fn_mmioAdvance(fat<void*>(self, 0), &fat<char>(self, 0xc), 0);        // 0x10033632
            fat<unsigned>(self, 0x170) = fat<unsigned>(self, 0x136c);               // 0x10033646
        }
        fat<unsigned>(self, 0x58) &= ~0x400u;                                       // 0x10033641 / 0x1003366f
        return 0;
    }
    if (c.cut) c3fn_mmioAdvance(fat<void*>(self, 0), &fat<char>(self, 0xc), 0);     // 0x1003376a-0x1003377e
    fat<unsigned>(self, 0x58) &= ~0x400u;                                           // 0x10033784-0x1003378a
    fat<char*>(fat<char*>(self, 8), 0x20) = nw;                                     // 0x1003378d / 0x10033790
    if (fat<unsigned>(self, 0x170) == fat<unsigned>(self, 0x178))                   // 0x10033793-0x1003379f
        fat<unsigned>(self, 0x58) |= 0x20;                                          // 0x100337a1-0x100337a6
    return 0;                                                                       // 0x100337ac
}

#if 0  // writeWaveChunk: reimpl written and oplog-verified per vector (orig == reimpl op logs, log/c3/alt/c3fn_oplog),
       // but the diff_hook full 10-vector run crashes the game after the hooks install (not a RED). Held for Path C /
       // next round. Kept compiled-out so the main build carries no unverified hook.
// `fld qword d; fistp dword` under the control word `fninit` sets (0x037f: 64-bit, round to nearest even); an out-of-range
// value stores the x87 integer indefinite 0x80000000. The original resets the unit with `fninit` once before the loop
// (0x10007e5e); a per-sample fninit gives the same int (the stack is empty after each fistp).
__declspec(noinline) int c3fn_fistRN(const double* d) {
    int v;
    __asm {
        mov eax, d
        fninit
        fld qword ptr [eax]
        fistp dword ptr [v]
    }
    return v;
}
// convert `count` doubles at src to clamped 16-bit words at dst: 0x7fff above 0x7fff, 0x8000 below -0x8000, else the low word
// (writeWaveChunk 0x10007e69-0x10007e8d, writeWaveChunkB's two bodies)
void c3fn_clampWords(short* dst, const double* src, unsigned count) {
    for (unsigned i = 0; i < count; i++) {
        int v = c3fn_fistRN(&src[i]);                                               // 0x10007e69 fld/fistp
        if (v > 0x7fff) dst[i] = 0x7fff;                                            // 0x10007e6e / 0x10007e75
        else if (v < -0x8000) dst[i] = static_cast<short>(0x8000);                 // 0x10007e77 / 0x10007e7e
        else dst[i] = static_cast<short>(v);                                       // 0x10007e80
    }
}

// 0x00007df0  writeWaveChunk (VA 0x10007df0). __cdecl-framed __thiscall, four stack arguments (`ret 0x10`): a0 the capture
// DsBuffer, a1 the source doubles, a2 and a3 two region pointers restore overwrites. this = ecx (the wave writer, +0x30 the
// output mmio handle, +0x38 an optional mix buffer). Returns 0x1a when DsBuffer::restore (thunk 0x1000213f) fails
// (0x10007e23), else 0. restore locks two capture regions, writing a3 = ptr1, [b1] and a2 = ptr2, [b2] (0x10007e1e). For
// each region with bytes, the sample count is 2 * (bytes / the DsBuffer's block align +0xb4) (0x10007e3f-0x10007e55 /
// 0x10007ef4-0x10007f17); that many doubles from a1 (the second region continuing after the first, 0x10007f19) are clamped
// to words into the region (c3fn_clampWords); when +0x30 is set they are mmioWritten to it (0x10007eb0 / 0x10007f7b); when
// +0x38 is set its words are added into the region (0x10007ed0 / 0x10007f98, the second continuing at +0x38 + samples1 * 2).
// Then DsBuffer::unlock (thunk 0x10001ec9) with (ptr1, b1, ptr2, b2) (0x10007fbd), +0x38 freed if set (0x10007fca).
typedef unsigned(__fastcall* WriteChunk_t)(void*, void*, void*, const double*, void*, void*);
WriteChunk_t WriteChunk_orig;
unsigned __fastcall WriteChunk_re(void* selfp, void*, void* a0, const double* a1, void* a2, void* a3) {
    char* self = static_cast<char*>(selfp);
    void* p1 = a3; void* p2 = a2; unsigned b1 = 0, b2 = 0;                          // restore's out-params (a3 / &b1 / a2 / &b2)
    if (c3fn_fn<Th6_t>(0x0000213f)(a0, 0, c3fn_u(a3), c3fn_u(a2), c3fn_u(&p1), c3fn_u(&b1), c3fn_u(&p2), c3fn_u(&b2)))   // 0x10007e1e
        return 0x1a;                                                                // 0x10007e27
    const unsigned ba = fat<unsigned short>(a0, 0xb4);                              // 0x10007e35
    const unsigned samples1 = ba ? 2 * (b1 / ba) : 0;                              // 0x10007e3f-0x10007e55
    c3fn_clampWords(static_cast<short*>(p1), a1, samples1);                         // 0x10007e69
    if (fat<void*>(self, 0x30))                                                     // 0x10007ea3
        c3fn_mmioWrite(fat<void*>(self, 0x30), p1, static_cast<long>(2 * samples1));   // 0x10007eb0
    char* mix = fat<char*>(self, 0x38);                                            // 0x10007eb6
    unsigned mixed1 = 0;
    if (mix && samples1) {                                                          // 0x10007eb9 / 0x10007ec6
        const short* m = reinterpret_cast<const short*>(mix);                       // 0x10007ed0
        short* d = static_cast<short*>(p1);
        for (unsigned i = 0; i < samples1; i++) d[i] = static_cast<short>(d[i] + m[i]);
        mixed1 = samples1;                                                          // 0x10007ecd (L10)
    }
    if (p2) {                                                                       // 0x10007ee2
        const unsigned samples2 = ba ? 2 * (b2 / ba) : 0;                          // 0x10007ef4-0x10007f17
        const double* src2 = a1 + samples1;                                         // 0x10007f19
        c3fn_clampWords(static_cast<short*>(p2), src2, samples2);                   // 0x10007f35
        if (fat<void*>(self, 0x30))                                                 // 0x10007f6f
            c3fn_mmioWrite(fat<void*>(self, 0x30), p2, static_cast<long>(2 * samples2));   // 0x10007f7b
        if (fat<char*>(self, 0x38) && samples2) {                                   // 0x10007f84 / 0x10007f93
            const short* m = reinterpret_cast<const short*>(fat<char*>(self, 0x38) + mixed1 * 2);   // 0x10007f90
            short* d = static_cast<short*>(p2);
            for (unsigned i = 0; i < samples2; i++) d[i] = static_cast<short>(d[i] + m[i]);
        }
    }
    c3fn_fn<Th4_t>(0x00001ec9)(a0, 0, c3fn_u(p1), b1, c3fn_u(p2), b2);             // 0x10007fbd DsBuffer::unlock
    if (fat<void*>(self, 0x38)) {                                                   // 0x10007fc2
        c3fn_delete(fat<void*>(self, 0x38));                                        // 0x10007fca
        fat<void*>(self, 0x38) = 0;                                                 // 0x10007fd2
    }
    return 0;                                                                       // 0x10007fdb
}

#endif  // writeWaveChunk

}  // namespace

SG_HOOK("sound.dll", 0x00033300, fillDecodeRing, FillRing_re, FillRing_orig);
SG_HOOK("sound.dll", 0x0001d810, Midi_Device_loadFile, MidiLoadFile_re, MidiLoadFile_orig);
SG_HOOK("sound.dll", 0x0001dce0, Midi_Device_loadFromMemory, MidiLoadMem_re, MidiLoadMem_orig);
SG_HOOK("sound.dll", 0x000316c0, loadWaveResource, LoadWaveRes_re, LoadWaveRes_orig);
SG_HOOK("sound.dll", 0x00037920, flushMmioBuffer, Flush_re, Flush_orig);
SG_HOOK("sound.dll", 0x00035ca0, closeMmioSound, CloseMmio_re, CloseMmio_orig);
SG_HOOK("sound.dll", 0x00030840, openMmioSound, OpenMmio_re, OpenMmio_orig);
SG_HOOK("sound.dll", 0x00030ba0, streamWaveSamples, Stream_re, Stream_orig);

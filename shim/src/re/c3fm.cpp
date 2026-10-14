// Batch c3fm (round 34): the golf_clean.exe and jgld.dll callees round 33 named, plus jgld's dashed lines.
//
//   group X - golf_clean.exe static CRT: _fopen 0x004a614d, _fclose 0x004a609f, _fgets
//             0x004a65ee, _strtok 0x004a678b, _strstr 0x004a64f0, _strchr 0x004a6170, __itoa 0x004ad425, _xtoa 0x004ad452
//   group O - the sound-library open routines openHookA / B / C 0x004873f0 / 0x004879b0 / 0x00487b90
//   group W - Window::passLocalRect 0x0047b9a0, Window::blitToTarget 0x00480dc0, DC_call90 0x00475ba0, DC_calla4 0x00475be0
//   group J - jgld.dll debug CRT: fopen 0x7faa0, fclose 0x7f850, operator_new 0x7f0f0, strlen 0x7f6e0, strcat 0x7f770,
//             memcpy 0x7f3a0, abs 0x7f370, _setjmp3 0x7fab8
//   group D - jgld.dll Surface::dashedLine16 0xc140 (dashedVLine16 0xd4f0 is not taken: log/c3/c3fm_notes.txt, Path C)
//
// Purposes and per-jcc tables: log/c3/c3fm_purpose.md; registry: re/frida/registry.d/c3fm.py; fixtures:
// re/frida/js/fixtures.d/c3fm.js; names proposed for the unnamed callees: log/c3/c3fm_names.tsv; what was not taken and
// why: log/c3/c3fm_notes.txt.
//
// Hand-written from the disassembly (log/c3/alt/c3fm_asm/*.txt, re/tools/asm2inline.py --list). golf_clean.exe has no
// relocations, so its addresses are literal. jgld.dll is relocated at run time: SG_HOOK addresses are RVAs, every jgld
// instruction cited in a comment is a VA (0x10000000 + RVA), and every jgld address a body uses is resolved against the
// running base. __thiscall is emulated with __fastcall (ecx = this, edx unused). Callees are called at the addresses
// the originals call, so a callee hooked by another batch runs that batch's body.
#include "hooks.h"
#include <intrin.h>
#include <stdint.h>

namespace {

typedef unsigned char byte;
template <class T> T& F(void* obj, unsigned off) { return *reinterpret_cast<T*>(static_cast<char*>(obj) + off); }
template <class T> T& G(unsigned addr) { return *reinterpret_cast<T*>(static_cast<uintptr_t>(addr)); }
template <class Fn> Fn At(unsigned addr) { return reinterpret_cast<Fn>(static_cast<uintptr_t>(addr)); }
// Slot `off` of the vtable of `obj` (the original's `mov eax, [obj]; call [eax + off]`).
template <class Fn> Fn Virt(void* obj, unsigned off) { return F<Fn>(F<void*>(obj, 0), off); }

// =====================================================================================================================
// group X - golf_clean.exe static CRT
// =====================================================================================================================

// The FILE layout these bodies touch (VC6 stdio.h): +0x00 _ptr, +0x04 _cnt, +0x0c _flag.
typedef void FILE_;
typedef FILE_*(__cdecl* Fsopen_t)(const char*, const char*, int);
typedef void(__cdecl* LockFile_t)(FILE_*);
typedef int(__cdecl* FcloseLk_t)(FILE_*);
typedef int(__cdecl* Filbuf_t)(FILE_*);
typedef byte*(__cdecl* Getptd_t)();
typedef void(__cdecl* Xtoa_t)(unsigned, char*, unsigned, int);

const unsigned kFsopen = 0x004a611c, kLockFile = 0x004a9874, kUnlockFile = 0x004a98c6, kFcloseLk = 0x004a60d0;
const unsigned kFilbuf = 0x004aa9f5, kGetptd = 0x004a8e53, kXtoa = 0x004ad452;

// 0x004a614d  _fopen(name, mode): `push 0x40` (_SH_DENYNO) 0x004a614d, the two arguments pushed again from
// [esp + 0xc] (0x004a614f / 0x004a6153), `call 0x4a611c` (__fsopen) 0x004a6157, `add esp, 0xc` and `ret` with
// __fsopen's eax (0x004a615c / 0x004a615f). __cdecl.
typedef FILE_*(__cdecl* Fopen_t)(const char*, const char*);
Fopen_t Fopen_orig;
FILE_* __cdecl Fopen_re(const char* name, const char* mode) {
    return At<Fsopen_t>(kFsopen)(name, mode, 0x40);
}

// 0x004a609f  _fclose(stream): edi = -1 (`or edi, 0xffffffff` 0x004a60a5). A string stream (bit 0x40 of the low flag
// byte, `test byte ptr [esi + 0xc], 0x40` 0x004a60a8) only has its flags cleared (`and dword ptr [esi + 0xc], 0`
// 0x004a60ae) and returns -1. Any other stream is locked (__lock_file 0x004a9874, 0x004a60b5), closed by __fclose_lk
// 0x004a60d0 (0x004a60bb, its result kept in edi 0x004a60c1) and unlocked (__unlock_file 0x004a98c6, 0x004a60c3);
// the result is __fclose_lk's (0x004a60cb). __cdecl, `ret` 0x004a60cf.
typedef int(__cdecl* Fclose_t)(FILE_*);
Fclose_t Fclose_orig;
int __cdecl Fclose_re(FILE_* f) {
    int r = -1;                                             // 0x004a60a5
    if (F<byte>(f, 0xc) & 0x40) {                           // 0x004a60a8 / 0x004a60ac
        F<int>(f, 0xc) = 0;                                 // 0x004a60ae
    } else {
        At<LockFile_t>(kLockFile)(f);                       // 0x004a60b5
        r = At<FcloseLk_t>(kFcloseLk)(f);                   // 0x004a60bb
        At<LockFile_t>(kUnlockFile)(f);                     // 0x004a60c3
    }
    return r;                                               // 0x004a60cb
}

// 0x004a65ee  _fgets(buf, n, stream): `cmp dword ptr [ebp + 0xc], 0` / `jg` 0x004a65f1 / 0x004a65fc: n <= 0 (signed)
// returns NULL without locking (0x004a65fe). Otherwise ebx = edi = buf, the stream is locked (0x004a6607) and the loop
// at 0x004a660d runs: `dec dword ptr [ebp + 0xc]` reaching 0 (`je` 0x004a6610) ends it with the terminator stored at
// edi (0x004a663f). Each character: `dec dword ptr [esi + 4]` (_cnt) going negative (`js` 0x004a6615) calls __filbuf
// 0x004a6622, else the byte at _ptr is read zero-extended and _ptr advanced (0x004a6617..0x004a661d). -1 (`cmp eax, -1`
// 0x004a6628, a full-width compare: a 0xff byte is 255, not -1) ends the loop: with nothing stored yet (`cmp edi,
// [ebp + 8]` 0x004a6636) the result becomes NULL and no terminator is written (0x004a663b), else the terminator is
// written (0x004a663f). Any other value is stored as its low byte (0x004a662d) and a stored '\n' (`cmp al, 0xa`
// 0x004a6630) ends the loop through the terminator. The stream is unlocked (0x004a6643) and ebx returned (0x004a6649).
typedef char*(__cdecl* Fgets_t)(char*, int, FILE_*);
Fgets_t Fgets_orig;
char* __cdecl Fgets_re(char* buf, int n, FILE_* f) {
    if (n <= 0) return 0;                                   // 0x004a65f1 / 0x004a65fc / 0x004a65fe
    char* result = buf;
    char* p = buf;
    At<LockFile_t>(kLockFile)(f);                           // 0x004a6607
    for (;;) {
        if (--n == 0) break;                                // 0x004a660d / 0x004a6610
        int c;
        if (--F<int>(f, 4) < 0) {                           // 0x004a6612 / 0x004a6615
            c = At<Filbuf_t>(kFilbuf)(f);                   // 0x004a6622
        } else {
            byte* q = F<byte*>(f, 0);                       // 0x004a6617
            c = *q;                                         // 0x004a6619 (movzx)
            F<byte*>(f, 0) = q + 1;                         // 0x004a661c / 0x004a661d
        }
        if (c == -1) {                                      // 0x004a6628 / 0x004a662b
            if (p == buf) {                                 // 0x004a6636 / 0x004a6639
                result = 0;                                 // 0x004a663b, no terminator (jmp 0x004a6642)
                At<LockFile_t>(kUnlockFile)(f);             // 0x004a6643
                return result;
            }
            break;
        }
        *p++ = static_cast<char>(c);                        // 0x004a662d / 0x004a662f
        if (static_cast<byte>(c) == 0x0a) break;            // 0x004a6630 / 0x004a6632
    }
    *p = 0;                                                 // 0x004a663f
    At<LockFile_t>(kUnlockFile)(f);                         // 0x004a6643
    return result;                                          // 0x004a6649
}

// 0x004a678b  _strtok(s, delim): the position lives in the calling thread's data block (__getptd 0x004a8e53 at
// 0x004a6797, field +0x18 read at 0x004a67d1 and written at 0x004a681f). A 32-byte bit map in the frame is cleared
// (`rep stosd` of 8 dwords 0x004a67a9) and every byte of delim INCLUDING its terminator sets bit (c & 7) of byte
// (c >> 3) (0x004a67ac..0x004a67c5: the loop stores before it tests for 0), so the terminator always counts as a
// delimiter. s NULL (`jne` 0x004a67cc) continues from the saved position. Leading delimiters are skipped while the byte
// is in the map and is not 0 (0x004a67e9 / 0x004a67ed); ebx = the token start (0x004a67f4). The token runs to the
// first byte in the map (0x004a680f / 0x004a6811; the terminator stops it through the map, a 0 byte first leaves
// through `je` 0x004a67fa without a write); a delimiter is overwritten with 0 and skipped (0x004a6816 / 0x004a6819).
// The position is stored (0x004a681f) and the token returned unless it is empty: `sub eax, edx; neg eax; sbb eax, eax;
// and eax, ebx` (0x004a6822..0x004a682a) gives ebx when start != position, else NULL.
typedef char*(__cdecl* Strtok_t)(char*, const char*);
Strtok_t Strtok_orig;
char* __cdecl Strtok_re(char* s, const char* delim) {
    byte* ptd = At<Getptd_t>(kGetptd)();                    // 0x004a6797
    byte map[32] = {0};                                     // 0x004a67a2..0x004a67a9
    const byte* d = reinterpret_cast<const byte*>(delim);
    byte c;
    do {                                                    // 0x004a67ac..0x004a67c5
        c = *d++;
        map[c >> 3] |= static_cast<byte>(1u << (c & 7));
    } while (c != 0);
    byte* p = reinterpret_cast<byte*>(s);
    if (p == 0) p = F<byte*>(ptd, 0x18);                    // 0x004a67ca / 0x004a67cc / 0x004a67d1
    while ((map[*p >> 3] & (1u << (*p & 7))) && *p != 0)    // 0x004a67d4..0x004a67f2
        ++p;
    byte* start = p;                                        // 0x004a67f4
    for (;;) {
        byte ch = *p;                                       // 0x004a67f6
        if (ch == 0) break;                                 // 0x004a67f8 / 0x004a67fa
        if (map[ch >> 3] & (1u << (ch & 7))) {              // 0x004a680f / 0x004a6811
            *p++ = 0;                                       // 0x004a6816 / 0x004a6819
            break;
        }
        ++p;                                                // 0x004a6813
    }
    F<byte*>(ptd, 0x18) = p;                                // 0x004a681f
    return start != p ? reinterpret_cast<char*>(start) : 0; // 0x004a6822..0x004a682a
}

// ---- the byte-then-dword character scan of _strchr 0x004a6170 (also the tail of _strstr). Entry 0x004a6170 keeps the
// low byte of the second argument (`xor eax, eax; mov al, [esp + 8]`); _strstr enters at 0x004a6176 with al = the
// needle's first byte. bl = c (0x004a6177). While the pointer is not dword aligned (`test edx, 3` 0x004a6180 /
// 0x004a6193) a byte loop compares with c first (`je` 0x004a618d -> 0x004a6160, which returns edx - 1) and then with 0
// (`je` 0x004a6191 -> NULL), so c == 0 finds the terminator. The aligned loop (0x004a61a6) reads one dword, advances by
// four (0x004a61c1) and tests c candidates first (`(~(x ^ cccc) ^ (0x7efefeff + (x ^ cccc))) & 0x81010100`, `jne`
// 0x004a61ca), then zero candidates (`je` 0x004a61d1 continues; `and eax, 0x1010100` / `jne` 0x004a61d8 returns NULL;
// the top byte only looking like a zero, `and esi, 0x80000000` / `jne` 0x004a61e0, continues; else NULL). A c candidate
// re-reads the dword (0x004a61e8) and walks its four bytes, c before 0 each time (0x004a61eb..0x004a620c), returning
// edx - 4 / - 3 / - 2 / - 1, or resumes the loop on a false positive (`jmp` 0x004a620e).
unsigned ScanMask(unsigned x) { return (~x) ^ (0x7efefeffu + x); }
char* StrchrScan(const char* str, unsigned char cb) {
    const byte* p = reinterpret_cast<const byte*>(str);
    while (reinterpret_cast<uintptr_t>(p) & 3) {            // 0x004a6180 / 0x004a6193
        byte ch = *p++;                                     // 0x004a6188 / 0x004a618a
        if (ch == cb) return reinterpret_cast<char*>(const_cast<byte*>(p - 1));   // 0x004a618d -> 0x004a6160
        if (ch == 0) return 0;                              // 0x004a6191
    }
    const unsigned c4 = cb * 0x01010101u;                   // ebx after 0x004a61a4
    for (;;) {
        const unsigned x = *reinterpret_cast<const unsigned*>(p);    // 0x004a61a6
        const unsigned esi = 0x7efefeffu + x;                        // 0x004a61b3
        const unsigned cmask = ScanMask(x ^ c4);                     // 0x004a61b1, 0x004a61b5, 0x004a61bd
        const unsigned zmask = ScanMask(x);                          // 0x004a61ba, 0x004a61bf
        p += 4;                                                      // 0x004a61c1
        if (cmask & 0x81010100u) {                                   // 0x004a61c4 / 0x004a61ca
            unsigned v = *reinterpret_cast<const unsigned*>(p - 4);  // 0x004a61e8
            if (static_cast<byte>(v) == cb) return reinterpret_cast<char*>(const_cast<byte*>(p - 4));       // 0x004a61ed
            if (static_cast<byte>(v) == 0) return 0;                                                       // 0x004a61f1
            if (static_cast<byte>(v >> 8) == cb) return reinterpret_cast<char*>(const_cast<byte*>(p - 3));  // 0x004a61f5
            if (static_cast<byte>(v >> 8) == 0) return 0;                                                  // 0x004a61f9
            v >>= 16;                                                                                      // 0x004a61fb
            if (static_cast<byte>(v) == cb) return reinterpret_cast<char*>(const_cast<byte*>(p - 2));       // 0x004a6200
            if (static_cast<byte>(v) == 0) return 0;                                                       // 0x004a6204
            if (static_cast<byte>(v >> 8) == cb) return reinterpret_cast<char*>(const_cast<byte*>(p - 1));  // 0x004a6208
            if (static_cast<byte>(v >> 8) == 0) return 0;                                                  // 0x004a620c
            continue;                                                                                      // 0x004a620e
        }
        if ((zmask & 0x81010100u) == 0) continue;           // 0x004a61cc / 0x004a61d1
        if (zmask & 0x01010100u) return 0;                  // 0x004a61d3 / 0x004a61d8
        if (esi & 0x80000000u) continue;                    // 0x004a61da / 0x004a61e0
        return 0;                                           // 0x004a61e2
    }
}

// 0x004a6170  _strchr(s, c): __cdecl, nothing popped (`ret` 0x004a61e7 and five more exits); only the low byte of c
// is used (`mov al, [esp + 8]`). The scan is StrchrScan above.
typedef char*(__cdecl* Strchr_t)(const char*, int);
Strchr_t Strchr_orig;
char* __cdecl Strchr_re(const char* s, int c) { return StrchrScan(s, static_cast<unsigned char>(c)); }

// 0x004a64f0  _strstr(s1, s2): __cdecl (both arguments read from the stack, `ret` 0x004a6529 / 0x004a6569 /
// 0x004a656f). dl = s2[0] (0x004a64f7): an empty s2 returns s1 (`je` 0x004a64ff -> 0x004a656a). dh = s2[1]
// (0x004a6501): a one-character s2 leaves through `mov al, dl; jmp 0x4a6176` (0x004a655c / 0x004a655e), the body of
// _strchr with the entry frame still in place, i.e. strchr(s1, s2[0]). Otherwise each attempt (0x004a6508) scans for
// dl (0x004a6511..0x004a6522; a 0 byte returns NULL at 0x004a6527); a dl match checks the next byte against dh
// (`jne` 0x004a652f resumes the scan with that byte), and a dh match sets edi = the byte after the dl match
// (0x004a6531) and compares s2[2..] two bytes per turn (`jne` 0x004a6542 / 0x004a6555 restart the attempt from edi;
// a 0 in s2 at `je` 0x004a6539 or 0x004a6549 ends the search) returning edi - 1, the dl position (0x004a6563).
typedef char*(__cdecl* Strstr_t)(const char*, const char*);
Strstr_t Strstr_orig;
char* __cdecl Strstr_re(const char* str1, const char* str2) {
    const byte* needle = reinterpret_cast<const byte*>(str2);
    const byte* edi = reinterpret_cast<const byte*>(str1);
    const byte dl = needle[0];                              // 0x004a64f7
    if (dl == 0) return const_cast<char*>(str1);            // 0x004a64fd / 0x004a64ff -> 0x004a656a
    const byte dh = needle[1];                              // 0x004a6501
    if (dh == 0) return StrchrScan(str1, dl);               // 0x004a6504 / 0x004a6506 -> 0x004a655e
    const byte* esi;
    const byte* ecx;
    byte al, ah;
L_ATTEMPT:                                                  // 0x004a6508
    esi = edi;
    ecx = needle;                                           // 0x004a650a
    al = *edi;                                              // 0x004a650e
    ++esi;                                                  // 0x004a6510
    if (al == dl) goto L_MATCH1;                            // 0x004a6511 / 0x004a6513
    if (al == 0) return 0;                                  // 0x004a6515 / 0x004a6517 -> 0x004a6527
L_SCAN:                                                     // 0x004a6519
    al = *esi;
    ++esi;
L_CMP:                                                      // 0x004a651c
    if (al == dl) goto L_MATCH1;                            // 0x004a651e
    if (al != 0) goto L_SCAN;                               // 0x004a6520 / 0x004a6522
    return 0;                                               // 0x004a6527
L_MATCH1:                                                   // 0x004a652a
    al = *esi;
    ++esi;                                                  // 0x004a652c
    if (al != dh) goto L_CMP;                               // 0x004a652d / 0x004a652f
    edi = esi - 1;                                          // 0x004a6531
L_TAIL:                                                     // 0x004a6534
    ah = ecx[2];
    if (ah == 0) return reinterpret_cast<char*>(const_cast<byte*>(edi - 1));   // 0x004a6537 / 0x004a6539 -> 0x004a6563
    al = *esi;                                              // 0x004a653b
    esi += 2;                                               // 0x004a653d
    if (al != ah) goto L_ATTEMPT;                           // 0x004a6540 / 0x004a6542
    al = ecx[3];                                            // 0x004a6544
    if (al == 0) return reinterpret_cast<char*>(const_cast<byte*>(edi - 1));   // 0x004a6547 / 0x004a6549
    ah = esi[-1];                                           // 0x004a654b
    ecx += 2;                                               // 0x004a654e
    if (al == ah) goto L_TAIL;                              // 0x004a6551 / 0x004a6553
    goto L_ATTEMPT;                                         // 0x004a6555
}

// 0x004ad452  _xtoa(value, buf, radix, is_neg): __cdecl, `ret` 0x004ad4ad. is_neg nonzero (`je` 0x004ad45f not
// taken) stores '-' (0x004ad464), advances the cursor (0x004ad467) and negates the value (0x004ad468). edi keeps the
// first digit's address (0x004ad46f). The digit loop (0x004ad471) divides the UNSIGNED value by the unsigned radix
// twice (`div` 0x004ad475 for the remainder, 0x004ad47e for the quotient); a remainder above 9 (`cmp ebx, 9` / `jbe`
// 0x004ad481 / 0x004ad486, a full 32-bit unsigned compare) becomes bl + 0x57 (0x004ad488), else bl + 0x30
// (0x004ad48d), both 8-bit adds; the digit is stored (0x004ad490) and the loop repeats while the quotient is nonzero
// (`ja` 0x004ad495). The terminator is stored (0x004ad497) and the digits are reversed in place, at least one swap
// (0x004ad49a..0x004ad4a7). eax at the return is the byte the last swap read at the right-hand cursor (`mov al,
// [ecx]` 0x004ad49d), zero-extended: the last quotient (0) fills the rest of eax.
typedef unsigned(__cdecl* XtoaRet_t)(unsigned, char*, unsigned, int);
XtoaRet_t Xtoa_orig;
unsigned __cdecl Xtoa_re(unsigned value, char* buf, unsigned radix, int is_neg) {
    char* p = buf;
    if (is_neg) {                                           // 0x004ad455 / 0x004ad45f
        *p++ = '-';                                         // 0x004ad464 / 0x004ad467
        value = 0u - value;                                 // 0x004ad468
    }
    char* first = p;                                        // 0x004ad46f
    do {
        unsigned rem = value % radix;                       // 0x004ad475
        value = value / radix;                              // 0x004ad47e
        byte bl = static_cast<byte>(rem);
        bl = static_cast<byte>(rem > 9 ? bl + 0x57 : bl + 0x30);   // 0x004ad481..0x004ad48d
        *p++ = static_cast<char>(bl);                       // 0x004ad490 / 0x004ad492
    } while (value != 0);                                   // 0x004ad493 / 0x004ad495
    *p = 0;                                                 // 0x004ad497
    char* right = p - 1;                                    // 0x004ad49a
    char* left = first;
    byte al;
    do {                                                    // 0x004ad49b..0x004ad4a7
        byte dl = static_cast<byte>(*left);
        al = static_cast<byte>(*right);
        *right = static_cast<char>(dl);
        *left = static_cast<char>(al);
        --right;
        ++left;
    } while (left < right);
    return al;
}

// 0x004ad425  __itoa(value, buf, radix): radix 10 (`cmp dword ptr [ebp + 0x10], 0xa` / `jne` 0x004ad428 / 0x004ad42c)
// with a negative value (signed `jge` 0x004ad432) calls _xtoa (value, buf, 10, 1) (0x004ad434 / 0x004ad436); every
// other case calls _xtoa (value, buf, radix, 0) (0x004ad43a / 0x004ad43c), at 0x004ad445. Returns buf (0x004ad44a).
typedef char*(__cdecl* Itoa_t)(int, char*, int);
Itoa_t Itoa_orig;
char* __cdecl Itoa_re(int value, char* buf, int radix) {
    if (radix == 10 && value < 0)
        At<Xtoa_t>(kXtoa)(static_cast<unsigned>(value), buf, 10, 1);
    else
        At<Xtoa_t>(kXtoa)(static_cast<unsigned>(value), buf, static_cast<unsigned>(radix), 0);
    return buf;
}

// =====================================================================================================================
// group O - the sound-library open routines (slot 1 of the device-node tables 0x004baf00 / 0x004bafb0 / 0x004bb014)
// =====================================================================================================================

// Each: thiscall with one stack argument (`ret 4` on its three exits). eax = this + 0x14 (`lea` first instruction);
// a non-NULL device pointer there returns 0xc; a NULL library entry point (the global soundVersionCheck 0x00483fc0
// fills with a GetProcAddress result) returns 0x14; else the entry point is called __cdecl with (this + 0x14, the
// argument) (`push edx; push eax; call ecx; add esp, 8`) and its eax is returned.
typedef int(__cdecl* CreateDevice_t)(void**, int);
int OpenThrough(void* self, int arg, unsigned slotGlobal) {
    void** dev = &F<void*>(self, 0x14);
    if (*dev != 0) return 0xc;
    CreateDevice_t create = G<CreateDevice_t>(slotGlobal);
    if (create == 0) return 0x14;
    return create(dev, arg);
}

// 0x004873f0  openHookA: device pointer test 0x004873f3 / `jne` 0x004873f8 -> 0x0048741a (0xc); entry point
// [0x0083af7c] (Dll_Wave_Device::create_device, export #5) 0x004873fa, NULL `je` 0x00487402 -> 0x00487412 (0x14);
// call 0x00487404..0x0048740c.
typedef int(__fastcall* OpenHook_t)(void*, void*, int);
OpenHook_t OpenHookA_orig;
int __fastcall OpenHookA_re(void* self, void*, int arg) { return OpenThrough(self, arg, 0x0083af7c); }

// 0x004879b0  openHookB: the same body on [0x0083af84] (Dll_Midi_Device::create_device, export #7) 0x004879ba; tests
// at 0x004879b8 and 0x004879c2.
OpenHook_t OpenHookB_orig;
int __fastcall OpenHookB_re(void* self, void*, int arg) { return OpenThrough(self, arg, 0x0083af84); }

// 0x00487b90  openHookC: the same body on [0x0083af8c] (Dll_Wave_In_Device::create_device, export #9) 0x00487b9a;
// tests at 0x00487b98 and 0x00487ba2.
OpenHook_t OpenHookC_orig;
int __fastcall OpenHookC_re(void* self, void*, int arg) { return OpenThrough(self, arg, 0x0083af8c); }

// =====================================================================================================================
// group W - window blits
// =====================================================================================================================

// 0x0047b9a0  Window::passLocalRect(this): thiscall, no stack argument (`ret` 0x0047b9e9). eax = this + 0x1ac
// (0x0047b9a4); the rect {l, t, r, b} there is moved to the origin in four registers (`neg edx; add esi, edx; add ebx,
// edx` 0x0047b9b4..0x0047b9bb for l and r, `neg eax; add edi, eax; add ebp, eax` 0x0047b9c6..0x0047b9ca for t and b),
// stored to a 16-byte frame rect {0, 0, r - l, b - t} (0x0047b9cd..0x0047b9d9) and its address passed with ecx still
// `this` to 0x00487b00 (0x0047b9dd), whose eax is returned (0x00487b00 is `xor eax, eax; ret 4`).
typedef int(__fastcall* RectSink_t)(void*, void*, int*);
typedef int(__fastcall* PassLocalRect_t)(void*, void*);
PassLocalRect_t PassLocalRect_orig;
int __fastcall PassLocalRect_re(void* self, void*) {
    const int* src = &F<int>(self, 0x1ac);
    int r[4];
    r[0] = src[0] - src[0];                                 // 0x0047b9b9 (0)
    r[1] = src[1] - src[1];                                 // 0x0047b9c8 (0)
    r[2] = src[2] - src[0];                                 // 0x0047b9bb
    r[3] = src[3] - src[1];                                 // 0x0047b9ca
    return At<RectSink_t>(0x00487b00)(self, 0, r);          // 0x0047b9dd
}

// 0x00475ba0  DC_call90(this, target, a, b, c) and 0x00475be0  DC_calla4: thiscall, four stack arguments (`ret 0x10`
// on every exit). The surface this + 4 NULL (`test ecx, ecx` / `jne`) returns 7; target NULL returns 0x10; else slot
// +0x90 (DC_call90) or +0xa4 (DC_calla4) of the surface is called with ([target + 4], a, b, c) and its eax returned.
typedef int(__fastcall* Slot4_t)(void*, void*, void*, int, int, int);
typedef int(__fastcall* DcCall4_t)(void*, void*, void*, int, int, int);
int DcCall(void* self, void* target, int a, int b, int c, unsigned slot) {
    void* surf = F<void*>(self, 4);                         // 0x00475ba0 / 0x00475be0
    if (surf == 0) return 7;
    if (target == 0) return 0x10;
    void* tsurf = F<void*>(target, 4);                      // 0x00475bc4 / 0x00475c04
    return Virt<Slot4_t>(surf, slot)(surf, 0, tsurf, a, b, c);
}
// 0x00475ba0  DC_call90: tests 0x00475ba3 / 0x00475ba5 (7 at 0x00475ba7) and 0x00475bb3 / 0x00475bb5 (0x10 at
// 0x00475bb7); call [edx + 0x90] 0x00475bd5.
DcCall4_t DcCall90_orig;
int __fastcall DcCall90_re(void* self, void*, void* target, int a, int b, int c) {
    return DcCall(self, target, a, b, c, 0x90);
}
// 0x00475be0  DC_calla4: tests 0x00475be3 / 0x00475be5 (7 at 0x00475be7) and 0x00475bf3 / 0x00475bf5 (0x10 at
// 0x00475bf7); call [edx + 0xa4] 0x00475c15.
DcCall4_t DcCalla4_orig;
int __fastcall DcCalla4_re(void* self, void*, void* target, int a, int b, int c) {
    return DcCall(self, target, a, b, c, 0xa4);
}

// 0x00480dc0  Window::blitToTarget(this, unused): thiscall, `ret 4`; the stack argument is never read. Slot 78 (+0x138)
// of the Window vtables (re/names/exe_14.tsv).
//  - The display [0x0083ad50], when set (`je` 0x00480dd0), gets slot +0x9c (1, 1, 1) (0x00480dd4..0x00480dda).
//  - The window rect this + 0x1dc is copied to the frame (0x00480de6..0x00480e09). With a redirect target
//    [0x0083aadc] (`je` 0x00480e0d not taken) the copy is offset by -[0x0083aae4] on left / right and -[0x0083aae8] on
//    top / bottom (0x00480e0f..0x00480e46) and Widget_fillRectR 0x00475b00 runs on that target; otherwise on the
//    screen widget 0x0083a7b8 (0x00480e51); the frame rect's address is the argument (0x00480e57).
//  - A frame point {0, 0} (0x00480e68 / 0x00480e70) goes through View::toParent 0x0047b170 (this, &x, &y)
//    (0x00480e78). [0x0083aadc] is read again (0x00480e7d): set, the point is offset by -[0x0083aae4] / -[0x0083aae8]
//    and the target is [0x0083aadc] (0x00480e87..0x00480ea3); else the target is 0x0083a7b8 (0x00480eb1).
//  - Bit 0x1000000 of this + 0x9c (`test` 0x00480eb6, `je` 0x00480ec0) selects the DC path on the Surface part
//    this + 0x274: this + 0xa4 nonzero (`jne` 0x00480eca) -> DC_call90 (target, x, y, [0xa4]) 0x00480efe; else this +
//    0xac nonzero (`jne` 0x00480ede or 0x00480f0d) -> DC_calla0 (target, x, y, [0xac]) 0x00480f30; else this + 0xa8
//    nonzero (`jne` 0x00480ed4, then `je` 0x00480ef2 and 0x00480f0d) -> DC_calla4 (target, x, y, [0xa8]) 0x00480f1f;
//    all three 0 -> DC_call84 (target, x, y) 0x00480ee9. Bit clear: Surface_blitThunk (target, 0, 0, x, y, [0x1b4] -
//    [0x1ac], [0x1b8] - [0x1b0]) 0x00480f62.
//  - Finally [target + 4], when set (`je` 0x00480f6f), gets slot +0xc4 (0x00480f73), whose eax is then the result;
//    otherwise the result is the blit call's eax.
// The point and the rect sit in one frame block in the original's order ({x, y} at esp + 0xc, the rect at esp + 0x14)
// so the frame pointers handed to the callees are in the same relative places.
typedef int(__fastcall* Disp9c_t)(void*, void*, int, int, int);
typedef int(__fastcall* FillRectR_t)(void*, void*, int*);
typedef int(__fastcall* ToParent_t)(void*, void*, int*, int*);
typedef int(__fastcall* DcCall3_t)(void*, void*, void*, int, int);
typedef int(__fastcall* BlitThunk_t)(void*, void*, void*, int, int, int, int, int, int);
typedef int(__fastcall* Slot0_t)(void*, void*);
typedef int(__fastcall* BlitToTarget_t)(void*, void*, int);
BlitToTarget_t BlitToTarget_orig;
int __fastcall BlitToTarget_re(void* self, void*, int) {
    struct { int x, y; int r[4]; } fr;                      // esp + 0xc .. esp + 0x20 in the original
    void* disp = G<void*>(0x0083ad50);                      // 0x00480dc8
    if (disp) Virt<Disp9c_t>(disp, 0x9c)(disp, 0, 1, 1, 1);  // 0x00480dce..0x00480dda
    const int* wr = &F<int>(self, 0x1dc);                   // 0x00480de0
    fr.r[0] = wr[0]; fr.r[1] = wr[1]; fr.r[2] = wr[2]; fr.r[3] = wr[3];   // 0x00480de6..0x00480e09
    void* redirect = G<void*>(0x0083aadc);                  // 0x00480e01
    void* fillOn;
    if (redirect) {                                         // 0x00480e07 / 0x00480e0d
        const int dx = G<int>(0x0083aae4), dy = G<int>(0x0083aae8);
        fr.r[0] -= dx; fr.r[2] -= dx;                       // 0x00480e1e..0x00480e30
        fr.r[1] -= dy; fr.r[3] -= dy;                       // 0x00480e38..0x00480e46
        fillOn = redirect;
    } else {
        fillOn = reinterpret_cast<void*>(0x0083a7b8);       // 0x00480e51
    }
    At<FillRectR_t>(0x00475b00)(fillOn, 0, fr.r);           // 0x00480e57
    fr.x = 0; fr.y = 0;                                     // 0x00480e68 / 0x00480e70
    At<ToParent_t>(0x0047b170)(self, 0, &fr.x, &fr.y);      // 0x00480e78
    void* target = G<void*>(0x0083aadc);                    // 0x00480e7d
    int x = fr.x, y = fr.y;
    if (target) {                                           // 0x00480e83 / 0x00480e85
        x -= G<int>(0x0083aae4);                            // 0x00480e97
        y -= G<int>(0x0083aae8);                            // 0x00480ea1
        fr.x = x; fr.y = y;                                 // 0x00480e9d / 0x00480ea3
    } else {
        target = reinterpret_cast<void*>(0x0083a7b8);       // 0x00480eb1
    }
    void* surf = static_cast<char*>(self) + 0x274;
    int result;
    if (F<unsigned>(self, 0x9c) & 0x1000000u) {             // 0x00480eb6 / 0x00480ec0
        const int a4 = F<int>(self, 0xa4), a8 = F<int>(self, 0xa8), ac = F<int>(self, 0xac);
        if (a4 != 0)                                        // 0x00480eca
            result = At<DcCall4_t>(0x00475ba0)(surf, 0, target, x, y, a4);       // 0x00480efe
        else if (ac != 0)                                   // 0x00480ede / 0x00480f0d
            result = At<DcCall4_t>(0x00475c20)(surf, 0, target, x, y, ac);       // 0x00480f30
        else if (a8 != 0)                                   // 0x00480ed4
            result = At<DcCall4_t>(0x00475be0)(surf, 0, target, x, y, a8);       // 0x00480f1f
        else
            result = At<DcCall3_t>(0x00475b60)(surf, 0, target, x, y);           // 0x00480ee9
    } else {
        result = At<BlitThunk_t>(0x00475c60)(surf, 0, target, 0, 0, x, y,
                                             F<int>(self, 0x1b4) - F<int>(self, 0x1ac),
                                             F<int>(self, 0x1b8) - F<int>(self, 0x1b0));      // 0x00480f37..0x00480f62
    }
    void* ts = F<void*>(target, 4);                         // 0x00480f67
    if (ts) result = Virt<Slot0_t>(ts, 0xc4)(ts, 0);        // 0x00480f6d..0x00480f73
    return result;
}

// =====================================================================================================================
// group J - jgld.dll debug CRT
// =====================================================================================================================

char* JgldBase() {
    static char* base = 0;
    if (!base) base = reinterpret_cast<char*>(GetModuleHandleA("jgld.dll"));
    return base;
}
template <class Fn> Fn JAt(unsigned va) { return reinterpret_cast<Fn>(JgldBase() + (va - 0x10000000)); }
const char* JStr(unsigned va) { return JgldBase() + (va - 0x10000000); }

// 0x0007faa0  fopen(name, mode) (VA 0x1007faa0): /Od frame; pushes 0x40 (0x1007faa3), mode (0x1007faa8) and name
// (0x1007faac) and calls __fsopen 0x1007f9a0 directly (0x1007faad); returns its eax. __cdecl (`ret` 0x1007fab6).
typedef void*(__cdecl* JFsopen_t)(const char*, const char*, int);
typedef void*(__cdecl* JFopen_t)(const char*, const char*);
JFopen_t JFopen_orig;
void* __cdecl JFopen_re(const char* name, const char* mode) {
    return JAt<JFsopen_t>(0x1007f9a0)(name, mode, 0x40);
}

// 0x0007f850  fclose(stream) (VA 0x1007f850): result slot [ebp - 4] = -1 (0x1007f857). A NULL stream (`jne`
// 0x1007f862 not taken) is reported through _CrtDbgReport (2, file string 0x1011ec94, line 0x3a, NULL, expression
// string 0x1011eca0) (0x1007f864..0x1007f874) with `int3` when it returns 1 (0x1007f87f / 0x1007f881); the do-while
// guard `xor eax, eax; test eax, eax; jne` (0x1007f882..0x1007f886) never loops, and the flag read at 0x1007f88b then
// dereferences the NULL stream. A stream with bit 0x40 of its flag dword (`and edx, 0x40` 0x1007f88e, `je`
// 0x1007f893) has the flag set to 0 (0x1007f898) and returns -1; any other is locked (__lock_file 0x10083230,
// 0x1007f8a5), closed by __fclose_lk 0x1007f8e0 (0x1007f8b1, its result stored to [ebp - 4] 0x1007f8b9) and unlocked
// (__unlock_file 0x100832a0, 0x1007f8c0). Returns [ebp - 4] (0x1007f8c8). __cdecl (`ret` 0x1007f8d1).
typedef int(__cdecl* JStream_t)(void*);
typedef int(__cdecl* CrtDbgReport_t)(int, const char*, int, const char*, const char*, ...);
JStream_t JFclose_orig;
int __cdecl JFclose_re(void* f) {
    int r = -1;                                                         // 0x1007f857
    if (f == 0) {                                                       // 0x1007f85e / 0x1007f862
        if (JAt<CrtDbgReport_t>(0x10081f90)(2, JStr(0x1011ec94), 0x3a, 0, JStr(0x1011eca0)) == 1)   // 0x1007f874
            __debugbreak();                                             // 0x1007f881
    }
    if (F<unsigned>(f, 0xc) & 0x40u) {                                  // 0x1007f88b..0x1007f893
        F<unsigned>(f, 0xc) = 0;                                        // 0x1007f898
    } else {
        JAt<JStream_t>(0x10083230)(f);                                  // 0x1007f8a5
        r = JAt<JStream_t>(0x1007f8e0)(f);                              // 0x1007f8b1 / 0x1007f8b9
        JAt<JStream_t>(0x100832a0)(f);                                  // 0x1007f8c0
    }
    return r;                                                           // 0x1007f8c8
}

// 0x0007f0f0  operator_new(size) (VA 0x1007f0f0): calls __nh_malloc 0x1007fba0 with (size, 1) (`push 1` 0x1007f0f4,
// size 0x1007f0f9, call 0x1007f0fa) and returns its eax through [ebp - 4] (0x1007f102 / 0x1007f105). __cdecl.
typedef void*(__cdecl* NhMalloc_t)(unsigned, int);
typedef void*(__cdecl* JNew_t)(unsigned);
JNew_t JNew_orig;
void* __cdecl JNew_re(unsigned size) { return JAt<NhMalloc_t>(0x1007fba0)(size, 1); }

// ---- the zero-byte dword scan shared by strlen 0x1007f6e0 and strcat 0x1007f770 (first half): byte steps while the
// pointer is not dword aligned (`test ecx, 3`), each byte tested for 0; then dwords: `(0x7efefeff + x) ^ ~x` masked
// with 0x81010100 nonzero (`test eax, 0x81010100`) re-reads the dword and tests bytes 0, 1, 2, 3 for 0 in that order;
// none zero (only bit 31 was set by a 0x80 top byte) continues the dword loop. Returns the address of the terminator.
const byte* FindNul(const byte* p) {
    while (reinterpret_cast<uintptr_t>(p) & 3) {
        if (*p == 0) return p;
        ++p;
    }
    for (;;) {
        const unsigned x = *reinterpret_cast<const unsigned*>(p);
        p += 4;
        if (((0x7efefeffu + x) ^ ~x) & 0x81010100u) {
            if ((x & 0xffu) == 0) return p - 4;
            if ((x & 0xff00u) == 0) return p - 3;
            if ((x & 0xff0000u) == 0) return p - 2;
            if ((x & 0xff000000u) == 0) return p - 1;
        }
    }
}

// 0x0007f6e0  strlen(s) (VA 0x1007f6e0): FindNul with the byte steps at 0x1007f6e4..0x1007f6f9 and the dword loop at
// 0x1007f700..0x1007f731 (masks 0x1007f702 / 0x1007f711; byte tests 0x1007f71b, 0x1007f71f, 0x1007f723, 0x1007f72a);
// returns the terminator's address minus s (0x1007f733..0x1007f75a). __cdecl, the argument re-read at each exit.
typedef unsigned(__cdecl* JStrlen_t)(const char*);
JStrlen_t JStrlen_orig;
unsigned __cdecl JStrlen_re(const char* s) {
    const byte* b = reinterpret_cast<const byte*>(s);
    return static_cast<unsigned>(FindNul(b) - b);
}

// 0x0007f770  strcat(dst, src) (VA 0x1007f770): edi = the terminator of dst (FindNul: 0x1007f775..0x1007f7ce, edi set
// at 0x1007f7bf / 0x1007f7c4 / 0x1007f7c9 / 0x1007f7ce). The copy then steps src byte by byte while src is not dword
// aligned (`test ecx, 3` 0x1007f7d5 / 0x1007f7e7), stopping after the terminator (`je` 0x1007f7e2 -> 0x1007f848, which
// stores it); then copies whole dwords until one may hold a zero (masks 0x1007f7f6..0x1007f809, store 0x1007f7f1):
// byte 0 zero stores 1 byte (0x1007f848), byte 1 a word (0x1007f83f), byte 2 a word and a 0 byte (0x1007f832 /
// 0x1007f839), byte 3 the dword (0x1007f82a); a false candidate stores the dword and continues (`jmp` 0x1007f828).
// So exactly the bytes of src up to and including its terminator are written at dst's terminator. Returns dst.
typedef char*(__cdecl* JStrcat_t)(char*, const char*);
JStrcat_t JStrcat_orig;
char* __cdecl JStrcat_re(char* dst, const char* src) {
    byte* d = const_cast<byte*>(FindNul(reinterpret_cast<const byte*>(dst)));
    const byte* s = reinterpret_cast<const byte*>(src);
    while (reinterpret_cast<uintptr_t>(s) & 3) {            // 0x1007f7d5 / 0x1007f7e7
        const byte c = *s++;                                // 0x1007f7dd / 0x1007f7df
        *d++ = c;                                           // 0x1007f7e4 (or 0x1007f848 for the terminator)
        if (c == 0) return dst;                             // 0x1007f7e0 / 0x1007f7e2
    }
    for (;;) {
        const unsigned x = *reinterpret_cast<const unsigned*>(s);   // 0x1007f7fb / 0x1007f804
        s += 4;                                                     // 0x1007f806
        if (((0x7efefeffu + x) ^ ~x) & 0x81010100u) {               // 0x1007f809 / 0x1007f80e
            if ((x & 0xffu) == 0) { d[0] = 0; return dst; }                                     // 0x1007f848
            if ((x & 0xff00u) == 0) { d[0] = static_cast<byte>(x); d[1] = 0; return dst; }      // 0x1007f83f
            if ((x & 0xff0000u) == 0) {                                                         // 0x1007f832
                d[0] = static_cast<byte>(x); d[1] = static_cast<byte>(x >> 8); d[2] = 0; return dst;
            }
            if ((x & 0xff000000u) == 0) { *reinterpret_cast<unsigned*>(d) = x; return dst; }    // 0x1007f82a
        }
        *reinterpret_cast<unsigned*>(d) = x;                        // 0x1007f7f1
        d += 4;                                                     // 0x1007f7f3
    }
}

// 0x0007f3a0  memcpy(dst, src, n) (VA 0x1007f3a0): the VC6 CRT copy that also serves overlapping blocks. A forward copy
// runs when dst <= src (`jbe` 0x1007f3b6) or dst >= src + n (`jb` 0x1007f3ba not taken): alignment bytes for dst
// (table 0x1007f400), `rep movsd` 0x1007f3d3 (or the dword table 0x1007f47c below 8 dwords), tail bytes (table
// 0x1007f4e8). Otherwise (dst inside [src, src + n)) the copy runs downward from the last byte (0x1007f538: esi / edi =
// the last dwords; `std; rep movsd; cld` 0x1007f553..0x1007f556; tables 0x1007f588 / 0x1007f630 / 0x1007f680). In both
// directions every destination byte receives the source byte at the same offset as it was before the call. Returns
// dst ([ebp + 8], e.g. 0x1007f504).
typedef void*(__cdecl* JMemcpy_t)(void*, const void*, unsigned);
JMemcpy_t JMemcpy_orig;
void* __cdecl JMemcpy_re(void* dst, const void* src, unsigned n) {
    byte* d = static_cast<byte*>(dst);
    const byte* s = static_cast<const byte*>(src);
    if (d <= s || d >= s + n) {                             // 0x1007f3b4 / 0x1007f3b8
        __movsb(d, s, n);                                   // the forward path, 0x1007f3c0..0x1007f4fe
    } else {
        for (unsigned i = n; i != 0; --i)                   // the downward path, 0x1007f538..0x1007f6d5
            d[i - 1] = s[i - 1];
    }
    return dst;
}

// 0x0007f370  abs(v) (VA 0x1007f370): `cmp dword ptr [ebp + 8], 0` / `jl` 0x1007f374 / 0x1007f378: negative ->
// `neg` (0x1007f385), else the value (0x1007f37a); returned through [ebp - 4] (0x1007f38a). __cdecl.
typedef int(__cdecl* JAbs_t)(int);
JAbs_t JAbs_orig;
int __cdecl JAbs_re(int v) { return v < 0 ? static_cast<int>(0u - static_cast<unsigned>(v)) : v; }

// 0x0007fab8  _setjmp3(jmp_buf, count, ...) (VA 0x1007fab8): no frame; edx = the buffer (0x1007fab8). Stores the
// CALLER's ebp, ebx, edi, esi at +0 .. +0xc (0x1007fabc..0x1007fac4), esp at entry at +0x10 (0x1007fac7), the return
// address [esp] at +0x14 (0x1007faca / 0x1007facd), the cookie 0x56433230 at +0x20 (0x1007fad0), 0 at +0x24
// (0x1007fad7) and the SEH registration head fs:[0] at +0x18 (0x1007fade / 0x1007fae4). A head of -1 (`jne`
// 0x1007faea not taken) stores -1 at +0x1c and returns. Otherwise ecx = count (0x1007faf5): count 0 (`je` 0x1007fafb)
// stores [head + 0xc] (the registration's try level) at +0x1c (0x1007fb07 / 0x1007fb0a). count >= 1 stores the third
// argument at +0x24 (0x1007fafd / 0x1007fb01) and, keeping it in eax, for count == 1 (`dec ecx; jne` 0x1007fb04 /
// 0x1007fb05 not taken) falls into the same store, now reading [third argument + 0xc]. count >= 2 stores the fourth
// argument at +0x1c (0x1007fb0f / 0x1007fb13), and count >= 3 (`dec ecx; je` 0x1007fb16 / 0x1007fb17 not taken) copies
// min(count - 2, 6) dwords from the fifth argument on (`lea esi, [esp + 0x1c]` after two pushes 0x1007fb1b, `cmp ecx,
// 6; jbe` 0x1007fb22 / 0x1007fb25, `rep movsd` 0x1007fb2c) to +0x28. Returns 0 (`sub eax, eax` 0x1007fb30).
// The entry registers are captured by a naked prologue (they are the data); the logic is C.
struct Sj3Entry { unsigned ebp, ebx, edi, esi, esp; };
unsigned __cdecl C3fmSetjmp3Body(const Sj3Entry* e) {
    const unsigned* st = reinterpret_cast<const unsigned*>(static_cast<uintptr_t>(e->esp));   // entry stack
    unsigned* jb = reinterpret_cast<unsigned*>(static_cast<uintptr_t>(st[1]));                // 0x1007fab8
    jb[0] = e->ebp; jb[1] = e->ebx; jb[2] = e->edi; jb[3] = e->esi;      // 0x1007fabc..0x1007fac4
    jb[4] = e->esp;                                                      // 0x1007fac7
    jb[5] = st[0];                                                       // 0x1007faca / 0x1007facd
    jb[8] = 0x56433230u;                                                 // 0x1007fad0
    jb[9] = 0;                                                           // 0x1007fad7
    const unsigned head = __readfsdword(0);                              // 0x1007fade
    jb[6] = head;                                                        // 0x1007fae4
    if (head == 0xffffffffu) {                                           // 0x1007fae7 / 0x1007faea
        jb[7] = 0xffffffffu;                                             // 0x1007faec
        return 0;
    }
    unsigned count = st[2];                                              // 0x1007faf5
    unsigned eax = head;
    if (count != 0) {                                                    // 0x1007faf9 / 0x1007fafb
        eax = st[3];                                                     // 0x1007fafd
        jb[9] = eax;                                                     // 0x1007fb01
        if (--count != 0) {                                              // 0x1007fb04 / 0x1007fb05
            jb[7] = st[4];                                               // 0x1007fb0f / 0x1007fb13
            if (--count != 0) {                                          // 0x1007fb16 / 0x1007fb17
                const unsigned k = count > 6 ? 6 : count;                // 0x1007fb22..0x1007fb27
                for (unsigned i = 0; i < k; ++i) jb[10 + i] = st[5 + i]; // 0x1007fb1b..0x1007fb2c
            }
            return 0;                                                    // 0x1007fb30
        }
    }
    jb[7] = *reinterpret_cast<const unsigned*>(static_cast<uintptr_t>(eax + 0xc));   // 0x1007fb07 / 0x1007fb0a
    return 0;
}
typedef int(__cdecl* JSetjmp3_t)(void*, int);
JSetjmp3_t JSetjmp3_orig;
__declspec(naked) int __cdecl JSetjmp3_re(void*, int) {
    __asm {
        push esp                        ; the value esp had at entry (push esp stores it before decrementing)
        push esi
        push edi
        push ebx
        push ebp
        mov eax, esp
        push eax
        call C3fmSetjmp3Body
        add esp, 4
        pop ebp
        pop ebx
        pop edi
        pop esi
        add esp, 4
        ret
    }
}

// =====================================================================================================================
// group D - jgld.dll Surface::dashedLine16
// =====================================================================================================================

// 0x0000c140  Surface::dashedLine16(x1, y1, x2, y2, c1, c2, len1, len2, phase) (VA 0x1000c140): dashedLine8 0x10010870
// (shim/src/re/c3bc.cpp) on a 16-bit surface. thiscall, `ret 0x24` (0x1000cca9), returns nothing a caller reads (its
// only caller Surface::drawDashedLine 0x100095a0 returns 0 at 0x10009641).
//  - x1 == x2 (`jne` 0x1000c163 not taken) calls dashedVLine16 through the thunk 0x100012b2 with (x1, y1, y2, c1, c2,
//    len1, len2, phase) (0x1000c188); y1 == y2 (`jne` 0x1000c198 not taken) calls dashedHLine16 through 0x100016b3 with
//    (x1, x2, y1, c1, c2, len1, len2, phase) (0x1000c1bd). Both then return without unlocking (0x1000cc99).
//  - Palette ([ebp - 0x34]): this + 0x7c when nonzero (`je` 0x1000c1d5), else [[0x1012873c] + 4] when the global is
//    nonzero (`je` 0x1000c1e9), else none. With a palette (`je` 0x1000c1fa not taken) the format word is [slot +0xe4
//    result + 4] (0x1000c20a / 0x1000c217): 0 (`je` 0x1000c221) and 1 (`je` 0x1000c227) run the same code: a colour
//    whose bit 31 is clear (`jne` 0x1000c239 / 0x1000c26b, 0x1000c29f / 0x1000c2d1) is replaced by the 16-bit entry
//    (c & 0xff) of the table palette slot +0x18 returns (c1, 0x1000c245) or slot +0x1c returns (c2, 0x1000c277); any
//    other format returns at once (0x1000c229 -> 0x1000c2fb -> 0x1000cc99).
//  - Both colours are then masked with 0xffff (0x1000c303 / 0x1000c30e), so the `cmp ..., -1` tests at every write
//    site can never be equal: every pixel of the line is written.
//  - x2 < x1 (`jge` 0x1000c31d not taken) swaps the endpoints (xchg 0x1000c322 / 0x1000c32b), len1 / len2 and c1 / c2
//    (three xors each) and sets phase = len1 + len2 - phase (0x1000c367..0x1000c370).
//  - lockBits slot +0x10 returning 0 (`jne` 0x1000c389 not taken) returns without unlocking; getClipRect slot +0xc8
//    fills the frame RECT at ebp - 0x14 (0x1000c39e).
//  - The phase becomes dashedLine8's signed cursor: signed `idiv` by len1 + len2 (0x1000c3b5), >= len1 (`jl` 0x1000c3c0
//    not taken) -> len2 - (phase - len1), else -(len1 - phase); len1 negated (0x1000c3ec).
//  - Octants: y1 < y2 (`jge` 0x1000c407 not taken) with guard 0x1000c40d..0x1000c42b, dx >= dy `jl` 0x1000c438; y1 >=
//    y2 with guard 0x1000c831..0x1000c84f, dy = abs(dy) through the CRT abs 0x1007f370 (0x1000c85a), dx >= dy `jl`
//    0x1000c86b. The pixel address is slot +0x20 + x * 2 + y * pitch * 2 with the pitch (in pixels) from slot +0xe0
//    (0x1000c4c2 / 0x1000c4dc and the same pair in each octant); every diagonal step calls slot +0xe0 again, and so do
//    the straight steps of the two steep octants (0x1000c774, 0x1000cba7).
//  - Every other exit leaves through the stubs 0x1000cc7d..0x1000cc97 to unlock slot +0x24 with 1 (0x1000cc71).
typedef int(__fastcall* Slot0i_t)(void*, void*);
typedef void*(__fastcall* Slot0p_t)(void*, void*);
typedef int(__fastcall* Slot1i_t)(void*, void*, int);
typedef int(__fastcall* SlotRect_t)(void*, void*, int*);
typedef void(__fastcall* Dashed8_t)(void*, void*, int, int, int, int, int, int, int, int);
typedef void(__fastcall* DashedLine16_t)(void*, void*, int, int, int, int, int, int, int, int, int);
DashedLine16_t DashedLine16_orig;

int SPitch(void* s) { return Virt<Slot0i_t>(s, 0xe0)(s, 0); }

void __fastcall DashedLine16_re(void* self, void*, int x1, int y1, int x2, int y2, int c1, int c2, int len1,
                                int len2, int phase) {
    if (x1 == x2) {                                                                     // 0x1000c163
        JAt<Dashed8_t>(0x100012b2)(self, 0, x1, y1, y2, c1, c2, len1, len2, phase);     // 0x1000c188
        return;
    }
    if (y1 == y2) {                                                                     // 0x1000c198
        JAt<Dashed8_t>(0x100016b3)(self, 0, x1, x2, y1, c1, c2, len1, len2, phase);     // 0x1000c1bd
        return;
    }
    void* pal = 0;                                                                      // 0x1000c1c7
    if (F<void*>(self, 0x7c) != 0) {                                                    // 0x1000c1d5
        pal = F<void*>(self, 0x7c);
    } else {
        void* g = *JAt<void**>(0x1012873c);                                             // 0x1000c1e2 / 0x1000c1e9
        if (g != 0) pal = F<void*>(g, 4);                                               // 0x1000c1f0
    }
    if (pal != 0) {                                                                     // 0x1000c1fa
        const int fmt = F<int>(Virt<Slot0p_t>(self, 0xe4)(self, 0), 4);                 // 0x1000c20a / 0x1000c217
        if (fmt != 0 && fmt != 1) return;                                               // 0x1000c221 / 0x1000c227
        if ((static_cast<unsigned>(c1) & 0x80000000u) == 0) {                           // 0x1000c239 / 0x1000c29f
            const unsigned short* t = static_cast<const unsigned short*>(Virt<Slot0p_t>(pal, 0x18)(pal, 0));
            c1 = t[c1 & 0xff];                                                          // 0x1000c25a
        }
        if ((static_cast<unsigned>(c2) & 0x80000000u) == 0) {                           // 0x1000c26b / 0x1000c2d1
            const unsigned short* t = static_cast<const unsigned short*>(Virt<Slot0p_t>(pal, 0x1c)(pal, 0));
            c2 = t[c2 & 0xff];                                                          // 0x1000c28c
        }
    }
    c1 &= 0xffff;                                                                       // 0x1000c303
    c2 &= 0xffff;                                                                       // 0x1000c30e
    if (x2 < x1) {                                                                      // 0x1000c31d
        int t = x1; x1 = x2; x2 = t;                                                    // 0x1000c322
        t = y1; y1 = y2; y2 = t;                                                        // 0x1000c32b
        len1 ^= len2; len2 ^= len1; len1 ^= len2;                                       // 0x1000c331..0x1000c349
        c1 ^= c2; c2 ^= c1; c1 ^= c2;                                                   // 0x1000c34c..0x1000c364
        phase = len1 + len2 - phase;                                                    // 0x1000c367..0x1000c370
    }
    if (Virt<Slot0i_t>(self, 0x10)(self, 0) == 0) return;                               // 0x1000c37d / 0x1000c389
    int rc[4];                                                                          // left, top, right, bottom
    Virt<SlotRect_t>(self, 0xc8)(self, 0, rc);                                          // 0x1000c39e
    const int left = rc[0], top = rc[1], right = rc[2], bottom = rc[3];
    phase = phase % (len1 + len2);                                                      // 0x1000c3b5
    if (phase >= len1) phase = len2 - (phase - len1);                                   // 0x1000c3c0..0x1000c3d1
    else phase = -(len1 - phase);                                                       // 0x1000c3d6..0x1000c3e4
    len1 = -len1;                                                                       // 0x1000c3ec
    const int dx = x2 - x1;                                                             // 0x1000c3f5
    int dy = y2 - y1;                                                                   // 0x1000c3fe
    char* p;
    int d, incE, incNE;
    // the eleven-instruction block at every write site (0x1000c4f5, 0x1000c5d0, 0x1000c6e3, 0x1000c7d5, 0x1000c928,
    // 0x1000ca06, 0x1000cb19, 0x1000cc0c): a negative cursor writes c1 and counts up, reloading len2 at 0; a
    // non-negative one writes c2 and counts down, reloading the negated len1 at 0
#define C3FM_PLOT                                                                \
    if (phase < 0) {                                                             \
        if (c1 != -1) *reinterpret_cast<unsigned short*>(p) = static_cast<unsigned short>(c1); \
        if (++phase == 0) phase = len2;                                          \
    } else {                                                                     \
        if (c2 != -1) *reinterpret_cast<unsigned short*>(p) = static_cast<unsigned short>(c2); \
        if (--phase == 0) phase = len1;                                          \
    }
#define C3FM_ADDR                                                                \
    {                                                                            \
        char* base = static_cast<char*>(Virt<Slot0p_t>(self, 0x20)(self, 0)) + x1 * 2;         \
        p = base + static_cast<int>(static_cast<unsigned>(y1) * static_cast<unsigned>(SPitch(self))) * 2; \
    }
    if (y1 < y2) {                                                                      // 0x1000c407
        if (x1 > right || y1 > bottom || x2 < left || y2 < top) goto L_UNLOCK;          // 0x1000c413..0x1000c42b
        if (dx >= dy) {                                                                 // 0x1000c438
            d = dy * 2 - dx; incE = dy * 2; incNE = (dy - dx) * 2;                      // 0x1000c43e..0x1000c459
            while (x1 < left || y1 < top) {                                             // 0x1000c462 / 0x1000c46a
                if (d <= 0) { d += incE; x1++; }                                        // 0x1000c470
                else { d += incNE; x1++; y1++; }
            }
            if (x1 >= right || y1 >= bottom) goto L_UNLOCK;                             // 0x1000c4a9 / 0x1000c4b1
            C3FM_ADDR                                                                   // 0x1000c4c2 / 0x1000c4dc
            C3FM_PLOT                                                                   // 0x1000c4f5
            if (x2 >= right) x2 = right - 1;                                            // 0x1000c54d
            while (x1 < x2) {                                                           // 0x1000c55e
                if (d <= 0) {                                                           // 0x1000c568
                    d += incE; x1++; p += 2;
                } else {
                    if (++y1 >= bottom) goto L_UNLOCK;                                  // 0x1000c596
                    d += incNE; x1++;
                    p = p + SPitch(self) * 2 + 2;                                       // 0x1000c5b9 / 0x1000c5c9
                }
                C3FM_PLOT                                                               // 0x1000c5d0
            }
        } else {
            d = dx * 2 - dy; incE = dx * 2; incNE = (dx - dy) * 2;                      // 0x1000c62c..0x1000c647
            while (x1 < left || y1 < top) {                                             // 0x1000c650 / 0x1000c658
                if (d <= 0) { d += incE; y1++; }                                        // 0x1000c65e
                else { d += incNE; y1++; x1++; }
            }
            if (x1 >= right || y1 >= bottom) goto L_UNLOCK;                             // 0x1000c697 / 0x1000c69f
            C3FM_ADDR                                                                   // 0x1000c6b0 / 0x1000c6ca
            C3FM_PLOT                                                                   // 0x1000c6e3
            if (y2 >= bottom) y2 = bottom - 1;                                          // 0x1000c73b
            while (y1 < y2) {                                                           // 0x1000c74c
                if (d <= 0) {                                                           // 0x1000c756
                    d += incE; y1++;
                    p = p + SPitch(self) * 2;                                           // 0x1000c774 / 0x1000c784
                } else {
                    if (++x1 >= right) goto L_UNLOCK;                                   // 0x1000c79b
                    d += incNE; y1++;
                    p = p + SPitch(self) * 2 + 2;                                       // 0x1000c7be / 0x1000c7ce
                }
                C3FM_PLOT                                                               // 0x1000c7d5
            }
        }
    } else {
        if (x1 > right || y1 < top || x2 < left || y2 > bottom) goto L_UNLOCK;          // 0x1000c837..0x1000c84f
        dy = JAt<JAbs_t>(0x1007f370)(dy);                                               // 0x1000c85a
        if (dx >= dy) {                                                                 // 0x1000c86b
            d = dy * 2 - dx; incE = dy * 2; incNE = (dy - dx) * 2;                      // 0x1000c871..0x1000c88c
            while (x1 < left || y1 >= bottom) {                                         // 0x1000c895 / 0x1000c89d
                if (d <= 0) { d += incE; x1++; }                                        // 0x1000c8a3
                else { d += incNE; x1++; y1--; }
            }
            if (x1 >= right || y1 < top) goto L_UNLOCK;                                 // 0x1000c8dc / 0x1000c8e4
            C3FM_ADDR                                                                   // 0x1000c8f5 / 0x1000c90f
            C3FM_PLOT                                                                   // 0x1000c928
            if (x2 >= right) x2 = right - 1;                                            // 0x1000c980
            while (x1 < x2) {                                                           // 0x1000c991
                if (d <= 0) {                                                           // 0x1000c99b
                    d += incE; x1++; p += 2;
                } else {
                    if (--y1 < top) goto L_UNLOCK;                                      // 0x1000c9c9
                    d += incNE; x1++;
                    p = p - SPitch(self) * 2 + 2;                                       // 0x1000c9ec..0x1000ca00
                }
                C3FM_PLOT                                                               // 0x1000ca06
            }
        } else {
            d = dx * 2 - dy; incE = dx * 2; incNE = (dx - dy) * 2;                      // 0x1000ca62..0x1000ca7d
            while (x1 < left || y1 >= bottom) {                                         // 0x1000ca86 / 0x1000ca8e
                if (d <= 0) { d += incE; y1--; }                                        // 0x1000ca94
                else { d += incNE; y1--; x1++; }
            }
            if (x1 >= right || y1 < top) goto L_UNLOCK;                                 // 0x1000cacd / 0x1000cad5
            C3FM_ADDR                                                                   // 0x1000cae6 / 0x1000cb00
            C3FM_PLOT                                                                   // 0x1000cb19
            if (y2 < top) y2 = top;                                                     // 0x1000cb71
            while (y1 > y2) {                                                           // 0x1000cb7f
                if (d <= 0) {                                                           // 0x1000cb89
                    d += incE; y1--;
                    p = p - SPitch(self) * 2;                                           // 0x1000cba7..0x1000cbb9
                } else {
                    if (++x1 >= right) goto L_UNLOCK;                                   // 0x1000cbcf
                    d += incNE; y1--;
                    p = p - SPitch(self) * 2 + 2;                                       // 0x1000cbf2..0x1000cc06
                }
                C3FM_PLOT                                                               // 0x1000cc0c
            }
        }
    }
#undef C3FM_PLOT
#undef C3FM_ADDR
L_UNLOCK:
    Virt<Slot1i_t>(self, 0x24)(self, 0, 1);                                             // 0x1000cc71
}

}  // namespace

SG_HOOK("golf_clean.exe", 0x004a614d, exe_fopen, Fopen_re, Fopen_orig);
SG_HOOK("golf_clean.exe", 0x004a609f, exe_fclose, Fclose_re, Fclose_orig);
SG_HOOK("golf_clean.exe", 0x004a65ee, exe_fgets, Fgets_re, Fgets_orig);
SG_HOOK("golf_clean.exe", 0x004a678b, exe_strtok, Strtok_re, Strtok_orig);
SG_HOOK("golf_clean.exe", 0x004a6170, exe_strchr, Strchr_re, Strchr_orig);
SG_HOOK("golf_clean.exe", 0x004a64f0, exe_strstr, Strstr_re, Strstr_orig);
SG_HOOK("golf_clean.exe", 0x004ad452, exe_xtoa, Xtoa_re, Xtoa_orig);
SG_HOOK("golf_clean.exe", 0x004ad425, exe_itoa, Itoa_re, Itoa_orig);
SG_HOOK("golf_clean.exe", 0x004873f0, openHookA, OpenHookA_re, OpenHookA_orig);
SG_HOOK("golf_clean.exe", 0x004879b0, openHookB, OpenHookB_re, OpenHookB_orig);
SG_HOOK("golf_clean.exe", 0x00487b90, openHookC, OpenHookC_re, OpenHookC_orig);
SG_HOOK("golf_clean.exe", 0x0047b9a0, Window_passLocalRect, PassLocalRect_re, PassLocalRect_orig);
SG_HOOK("golf_clean.exe", 0x00475ba0, DC_call90, DcCall90_re, DcCall90_orig);
SG_HOOK("golf_clean.exe", 0x00475be0, DC_calla4, DcCalla4_re, DcCalla4_orig);
SG_HOOK("golf_clean.exe", 0x00480dc0, Window_blitToTarget, BlitToTarget_re, BlitToTarget_orig);
SG_HOOK("jgld.dll", 0x0007faa0, jgld_fopen, JFopen_re, JFopen_orig);
SG_HOOK("jgld.dll", 0x0007f850, jgld_fclose, JFclose_re, JFclose_orig);
SG_HOOK("jgld.dll", 0x0007f0f0, jgld_operator_new, JNew_re, JNew_orig);
SG_HOOK("jgld.dll", 0x0007f6e0, jgld_strlen, JStrlen_re, JStrlen_orig);
SG_HOOK("jgld.dll", 0x0007f770, jgld_strcat, JStrcat_re, JStrcat_orig);
SG_HOOK("jgld.dll", 0x0007f3a0, jgld_memcpy, JMemcpy_re, JMemcpy_orig);
SG_HOOK("jgld.dll", 0x0007f370, jgld_abs, JAbs_re, JAbs_orig);
SG_HOOK("jgld.dll", 0x0007fab8, jgld_setjmp3, JSetjmp3_re, JSetjmp3_orig);
SG_HOOK("jgld.dll", 0x0000c140, Surface_dashedLine16, DashedLine16_re, DashedLine16_orig);

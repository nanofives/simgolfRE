// C3 batch c3cm (2026-10-09) of sound.dll: functions of the statically linked MSVC CRT.
// Addresses in SG_HOOK and in the `// 0x...` comment lines are RVAs; the instruction addresses quoted inside each
// body are VAs (VA = 0x10000000 + RVA), which is how re/tools/asm2inline.py prints sound.dll.
// Hand-written from the disassembly (py -3.12 re/tools/asm2inline.py sound.dll 0x<va> --list) and the C2
// transcriptions in re/analysis/crt. sound.dll is LoadLibrary'd after the shim starts, so these hooks install
// from the LoadLibraryA hook (SgHooksInstallPending, re/hooks.cpp).
//
// Scope of the batch: string and memory scans, one double-bit-field test, one virtual-base vtable store and one
// integer date computation. Nothing here reaches the CRT heap (__sbh_*, malloc/free/realloc/_msize), a critical
// section or _lock/_unlock, the per-thread data block (_getptd, so no errno/_doserrno), a file, the environment
// block or the FPU control word; the five functions that read or write only their arguments allocate nothing.
// cvtdate 0x10051319 writes six module globals and crtValidatePointer 0x10049604 calls one KERNEL32 import
// (IsBadWritePtr, through the same IAT slot the original calls); both are described in their bodies below.
// No DirectSound, winmm, COM or x87 arithmetic is involved.
//
// sound.dll has base relocations, so every global, table and constant address is resolved against the loaded
// module base (c3cm_sound), never written as the 0x10000000-based VA the disassembly prints.
#include "hooks.h"
#include <stdint.h>

namespace {

char* c3cm_sound(unsigned rva) {
    static HMODULE m = 0;
    if (!m) m = GetModuleHandleA("sound.dll");
    return reinterpret_cast<char*>(m) + rva;
}

// Address of the start of the dword a pointer lies in is never needed; only the low two bits are tested, exactly
// as `test edx, 3` / `test ecx, 3` do in the originals.
unsigned c3cm_low2(const void* p) { return static_cast<unsigned>(reinterpret_cast<uintptr_t>(p)) & 3u; }

// ---------------------------------------------------------------------------------------------------------
// The dword scan shared by strchr 0x10045680 and strcpy 0x1004a081: with ebx = c replicated into four bytes,
// `edi = 0x7efefeff + (x ^ ebx)` and `ecx = ~(x ^ ebx) ^ edi` (0x100456c1..0x100456cd), and the four candidate
// bits are `ecx & 0x81010100` (0x100456d4). A byte equal to c makes the corresponding bit set; the top byte also
// sets bit 31 when its value xored with c is 0x80, which is why both originals re-read the dword and test the
// four bytes one by one before giving an answer.
unsigned c3cm_mask(unsigned x) { return (~x) ^ (0x7efefeffu + x); }

// 0x00045680  strchr (VA 0x10045680)
// `xor eax, eax` / `mov al, [esp+8]` at 0x10045680 keep only the low byte of the second stack argument; after
// `push ebx` the string pointer is read from [esp+8] at 0x1004568c, so the convention is __cdecl(str, c) with
// nothing popped (`ret` at 0x100456f7 and five more exits). ebx becomes c (0x10045687) and, after the alignment
// loop, c replicated into all four bytes (shl/or at 0x10045689, 0x100456ab, 0x100456b0, 0x100456b4).
// While the pointer is not dword aligned (`test edx, 3` at 0x10045690 and 0x100456a3) the scan is a byte loop at
// 0x10045698: the character is compared with c first (je 0x1004569d, which returns edx-1 at 0x10045670) and only
// then with zero (je 0x100456a1 -> 0x100456f4 returns 0), so c == 0 finds the terminator.
// The aligned loop at 0x100456b6 reads one dword, advances the pointer by four (0x100456d1) and tests the c
// candidates first (jne 0x100456da -> 0x100456f8), then the zero candidates (je 0x100456e1 continues the loop;
// `and eax, 0x1010100` / jne at 0x100456e3 returns 0 for a zero in bytes 0..2; `and esi, 0x80000000` / jne at
// 0x100456ea continues the loop when the top byte only looked like zero; otherwise 0x100456f2 returns 0).
// The candidate block at 0x100456f8 re-reads the dword from memory and walks its four bytes in order, comparing
// each with c and then with zero (0x100456fb..0x1004571c), returning edx-4, edx-3, edx-2 or edx-1; when none of
// the eight tests fires the candidate was a false positive and the loop resumes (jmp at 0x1004571e).
char* c3cm_strchr(const char* str, int c) {
    const unsigned char cb = static_cast<unsigned char>(c);
    const unsigned char* p = reinterpret_cast<const unsigned char*>(str);
    while (c3cm_low2(p) != 0) {                        // 0x10045690, 0x100456a3
        unsigned char ch = *p;                         // 0x10045698
        ++p;
        if (ch == cb) return reinterpret_cast<char*>(const_cast<unsigned char*>(p - 1));  // 0x1004569d
        if (ch == 0) return 0;                         // 0x100456a1
    }
    const unsigned c4 = cb * 0x01010101u;              // ebx after 0x100456b4
    for (;;) {
        const unsigned x = *reinterpret_cast<const unsigned*>(p);   // 0x100456b6
        const unsigned esi = 0x7efefeffu + x;                       // 0x100456c3
        const unsigned cmask = c3cm_mask(x ^ c4);                   // 0x100456c1, 0x100456c5, 0x100456cd
        const unsigned zmask = c3cm_mask(x);                        // 0x100456ca, 0x100456cf
        p += 4;                                                     // 0x100456d1
        if (cmask & 0x81010100u) {                                  // 0x100456d4, 0x100456da
            unsigned v = *reinterpret_cast<const unsigned*>(p - 4); // 0x100456f8
            if (static_cast<unsigned char>(v) == cb)
                return reinterpret_cast<char*>(const_cast<unsigned char*>(p - 4));        // 0x100456fd
            if (static_cast<unsigned char>(v) == 0) return 0;                             // 0x10045701
            if (static_cast<unsigned char>(v >> 8) == cb)
                return reinterpret_cast<char*>(const_cast<unsigned char*>(p - 3));        // 0x10045705
            if (static_cast<unsigned char>(v >> 8) == 0) return 0;                        // 0x10045709
            v >>= 16;                                                                     // 0x1004570b
            if (static_cast<unsigned char>(v) == cb)
                return reinterpret_cast<char*>(const_cast<unsigned char*>(p - 2));        // 0x10045710
            if (static_cast<unsigned char>(v) == 0) return 0;                             // 0x10045714
            if (static_cast<unsigned char>(v >> 8) == cb)
                return reinterpret_cast<char*>(const_cast<unsigned char*>(p - 1));        // 0x10045718
            if (static_cast<unsigned char>(v >> 8) == 0) return 0;                        // 0x1004571c
            continue;                                                                     // 0x1004571e
        }
        if ((zmask & 0x81010100u) == 0) continue;      // 0x100456dc, 0x100456e1
        if (zmask & 0x01010100u) return 0;             // 0x100456e3, 0x100456e8
        if (esi & 0x80000000u) continue;               // 0x100456ea, 0x100456f0
        return 0;                                      // 0x100456f2
    }
}
typedef char*(__cdecl* Strchr_t)(const char*, int);
Strchr_t Strchr_orig;
char* __cdecl Strchr_re(const char* str, int c) { return c3cm_strchr(str, c); }

// 0x00042690  strstr (VA 0x10042690)
// Both arguments are read from the stack ([esp+8] = the second at 0x10042690, [esp+0x10] = the first at
// 0x10042699 after three pushes) and nothing is popped (`ret` at 0x100426c9, 0x10042709, 0x1004270f): __cdecl.
// dl is the first character of the second string (0x10042697) and dh the second (0x100426a1). An empty second
// string returns the first unchanged (je at 0x1004269f -> 0x1004270a). A one-character second string leaves
// through 0x100426fe, a `jmp` into the body of strchr at 0x10045686 with eax = dl and the entry stack frame
// still in place, which is strchr(str1, str2[0]) entered six bytes past its first instruction (the two
// instructions skipped are the ones that load the character from [esp+8]); the reimplementation runs the same
// scan through c3cm_strchr.
// The search itself: edi is the position the next attempt starts from. Each attempt (0x100426a8) reloads the
// second string into ecx, reads *edi and scans forward for dl (je at 0x100426b3, the byte loop at 0x100426b9
// with je at 0x100426be and jne at 0x100426c2; a zero byte returns 0 at 0x100426c4). With dl matched at
// position p, 0x100426ca reads the next character: if it is not dh (jne at 0x100426cf) the scan resumes at
// 0x100426bc, comparing that same character with dl. If it is, edi becomes p+1 (0x100426d1) and the tail loop at
// 0x100426d4 compares two characters per turn: ecx[2] against *esi (0x100426e0, jne at 0x100426e2 restarts the
// outer loop from edi) and ecx[3] against esi[-1] (0x100426f1, je at 0x100426f3 advances ecx by two and esi by
// two, 0x100426ee and 0x100426dd). A zero in the second string at either position (je at 0x100426d9 and
// 0x100426e9) ends the search and returns edi-1, that is the position p where dl matched.
typedef char*(__cdecl* Strstr_t)(const char*, const char*);
Strstr_t Strstr_orig;
char* __cdecl Strstr_re(const char* str1, const char* str2) {
    const unsigned char* needle = reinterpret_cast<const unsigned char*>(str2);
    const unsigned char* edi = reinterpret_cast<const unsigned char*>(str1);
    const unsigned char dl = needle[0];                            // 0x10042697
    if (dl == 0) return reinterpret_cast<char*>(const_cast<unsigned char*>(edi));   // 0x1004269f, 0x1004270a
    const unsigned char dh = needle[1];                            // 0x100426a1
    if (dh == 0) return c3cm_strchr(str1, dl);                     // 0x100426a6, 0x100426f7, 0x100426fe

    const unsigned char* esi;
    const unsigned char* ecx;
    unsigned char al, ah;
L_OUTER:                                                           // 0x100426a8
    esi = edi;
    ecx = needle;                                                  // 0x100426aa
    al = *edi;                                                     // 0x100426ae
    ++esi;                                                         // 0x100426b0
    if (al == dl) goto L_MATCH1;                                   // 0x100426b3
    if (al == 0) return 0;                                         // 0x100426b7, 0x100426c4
L_SCAN:                                                            // 0x100426b9
    al = *esi;
    ++esi;
L_CMP:                                                             // 0x100426bc
    if (al == dl) goto L_MATCH1;                                   // 0x100426be
    if (al != 0) goto L_SCAN;                                      // 0x100426c2
    return 0;                                                      // 0x100426c4
L_MATCH1:                                                          // 0x100426ca
    al = *esi;
    ++esi;                                                         // 0x100426cc
    if (al != dh) goto L_CMP;                                      // 0x100426cf
    edi = esi - 1;                                                 // 0x100426d1
L_TAIL:                                                            // 0x100426d4
    ah = ecx[2];
    if (ah == 0) return reinterpret_cast<char*>(const_cast<unsigned char*>(edi - 1));   // 0x100426d9, 0x10042703
    al = *esi;                                                     // 0x100426db
    esi += 2;                                                      // 0x100426dd
    if (al != ah) goto L_OUTER;                                    // 0x100426e2
    al = ecx[3];                                                   // 0x100426e4
    if (al == 0) return reinterpret_cast<char*>(const_cast<unsigned char*>(edi - 1));   // 0x100426e9
    ah = *(esi - 1);                                               // 0x100426eb
    ecx += 2;                                                      // 0x100426ee
    if (al == ah) goto L_TAIL;                                     // 0x100426f3
    goto L_OUTER;                                                  // 0x100426f5
}

// 0x0004a010  strcpy (VA 0x1004a010)
// The entry is three instructions: `push edi`, `mov edi, [esp+8]` (the first argument, the destination) at
// 0x1004a011 and `jmp 0x1004a081` at 0x1004a015 into a copy loop the module shares with _strcat 0x1004a020.
// The loop reads the source from [esp+0xc] at 0x1004a081 (the second argument) and every exit returns the
// destination by re-reading [esp+8] (0x1004a0dc, 0x1004a0e5, 0x1004a0f2, 0x1004a0fa) and popping only edi
// (`ret` at 0x1004a0e1 and three more): __cdecl(dst, src).
// While the SOURCE is not dword aligned (`test ecx, 3` at 0x1004a085 and 0x1004a097) the copy is a byte loop at
// 0x1004a08d; a zero byte leaves through 0x1004a092 to 0x1004a0f8, which stores that zero and returns.
// The aligned loop at 0x1004a0a6 reads the source dword twice (0x1004a0ab for the test and 0x1004a0b4 for the
// value), advances the source by four (0x1004a0b6) and tests `eax & 0x81010100` (0x1004a0b9): with no candidate
// (je at 0x1004a0be) the whole dword is written at 0x1004a0a1 and the destination advances by four. With a
// candidate the four bytes are tested in order: byte 0 zero stores one byte (0x1004a0c2 -> 0x1004a0f8), byte 1
// zero stores the low word (0x1004a0c6 -> 0x1004a0ef), byte 2 zero stores the low word and a zero byte after it
// (0x1004a0ce -> 0x1004a0e2), byte 3 zero stores the whole dword (0x1004a0d6 -> 0x1004a0da), and a candidate
// with no zero byte at all is a false positive that stores the dword and continues (jmp at 0x1004a0d8).
typedef char*(__cdecl* Strcpy_t)(char*, const char*);
Strcpy_t Strcpy_orig;
char* __cdecl Strcpy_re(char* dst, const char* src) {
    unsigned char* d = reinterpret_cast<unsigned char*>(dst);              // 0x1004a011
    const unsigned char* s = reinterpret_cast<const unsigned char*>(src);  // 0x1004a081
    if (c3cm_low2(s) != 0) {                                               // 0x1004a085, 0x1004a08b
        for (;;) {
            unsigned char ch = *s;                                         // 0x1004a08d
            ++s;
            if (ch == 0) { *d = 0; return dst; }                           // 0x1004a092, 0x1004a0f8
            *d = ch;                                                       // 0x1004a094
            ++d;
            if (c3cm_low2(s) == 0) break;                                  // 0x1004a097, 0x1004a09d
        }
    }
    for (;;) {                                                             // 0x1004a0a6
        const unsigned x = *reinterpret_cast<const unsigned*>(s);          // 0x1004a0ab
        const unsigned m = c3cm_mask(x);                                   // 0x1004a0ad, 0x1004a0af, 0x1004a0b2
        const unsigned v = *reinterpret_cast<const unsigned*>(s);          // 0x1004a0b4
        s += 4;                                                            // 0x1004a0b6
        if ((m & 0x81010100u) == 0) {                                      // 0x1004a0b9, 0x1004a0be
            *reinterpret_cast<unsigned*>(d) = v;                           // 0x1004a0a1
            d += 4;
            continue;
        }
        if (static_cast<unsigned char>(v) == 0) {                          // 0x1004a0c0, 0x1004a0c2
            *d = static_cast<unsigned char>(v);                            // 0x1004a0f8
            return dst;
        }
        if (static_cast<unsigned char>(v >> 8) == 0) {                     // 0x1004a0c4, 0x1004a0c6
            *reinterpret_cast<unsigned short*>(d) = static_cast<unsigned short>(v);   // 0x1004a0ef
            return dst;
        }
        if ((v & 0x00ff0000u) == 0) {                                      // 0x1004a0c8, 0x1004a0ce
            *reinterpret_cast<unsigned short*>(d) = static_cast<unsigned short>(v);   // 0x1004a0e2
            d[2] = 0;                                                      // 0x1004a0e9
            return dst;
        }
        if ((v & 0xff000000u) == 0) {                                      // 0x1004a0d0, 0x1004a0d6
            *reinterpret_cast<unsigned*>(d) = v;                           // 0x1004a0da
            return dst;
        }
        *reinterpret_cast<unsigned*>(d) = v;                               // 0x1004a0d8 -> 0x1004a0a1
        d += 4;
    }
}

// 0x00048268  doubleExponentField (VA 0x10048268)
// Reads one dword, [esp+8] (0x10048268 and again at 0x10048279), masks it with 0x7ff00000 (0x1004826c) and
// compares the result with 0x7ff00000 (0x10048271). When the two differ the masked value is returned
// (`ret` at 0x10048278); when they are equal the unmasked dword is returned instead (je at 0x10048276,
// `ret` at 0x1004827d). Nothing is popped: __cdecl. Its two callers CIsqrt 0x100431f0 and CIlog 0x100432d0
// pass the two halves of a double, so [esp+4] is the low half (never read here) and [esp+8] the high half,
// whose bits 20..30 are the exponent field and whose remaining bits are the sign and the top of the mantissa.
typedef unsigned(__cdecl* DoubleExpField_t)(unsigned, unsigned);
DoubleExpField_t DoubleExpField_orig;
unsigned __cdecl DoubleExpField_re(unsigned /*lo*/, unsigned hi) {
    unsigned e = hi & 0x7ff00000u;                 // 0x1004826c
    if (e == 0x7ff00000u) return hi;               // 0x10048271, 0x10048276, 0x10048279
    return e;                                      // 0x10048278
}

// 0x00041aad  iosResetVtable (VA 0x10041aad)
// `ecx` is read at 0x10041aad before any write and `ret` at 0x10041abb pops nothing: __thiscall with no stack
// argument, emulated here with __fastcall (ecx = this, edx unused). The dword eight bytes below the object is a
// pointer (0x10041aad); its second dword is an offset (0x10041ab0); the store at 0x10041ab3 writes the constant
// 0x1005ecd4 -- an address inside the image, so it is resolved against the module base -- to
// [this - 8 + offset]. The address arithmetic is 32-bit and wraps, as `lea`/`mov [eax+ecx-8]` does.
// eax is left holding the offset loaded at 0x10041ab0, which is what the caller ostreamResetVtable 0x10041c26
// sees as the return value.
typedef unsigned(__fastcall* IosResetVtable_t)(void*, void*);
IosResetVtable_t IosResetVtable_orig;
unsigned __fastcall IosResetVtable_re(void* self, void*) {
    char* p = reinterpret_cast<char*>(self);
    char* vb = *reinterpret_cast<char**>(p - 8);                        // 0x10041aad
    unsigned off = *reinterpret_cast<unsigned*>(vb + 4);                // 0x10041ab0
    char* dest = reinterpret_cast<char*>(static_cast<unsigned>(reinterpret_cast<uintptr_t>(p)) + off - 8u);
    *reinterpret_cast<void**>(dest) = c3cm_sound(0x0005ecd4);           // 0x10041ab3
    return off;
}

// 0x00049604  crtValidatePointer (VA 0x10049604)
// Pushes its two stack arguments in reverse order (0x10049608 pushes [esp+0xc], the second argument, then
// 0x1004960c pushes what is by then the first) and calls the import at [0x100b84bc] (0x10049610), which is
// KERNEL32!IsBadWritePtr; the argument order of the call is therefore (pointer, length). esi is preloaded with 1
// (0x10049605) and cleared when the import returns a non-zero value (test/je at 0x10049616, 0x10049618,
// xor at 0x1004961a), so the function returns 1 for a writable block and 0 otherwise (`ret` at 0x1004961f pops
// nothing: __cdecl). The reimplementation calls the same import through the same IAT slot, resolved against the
// module base; nothing else is read or written.
typedef int(__cdecl* CrtValidatePointer_t)(void*, unsigned);
CrtValidatePointer_t CrtValidatePointer_orig;
int __cdecl CrtValidatePointer_re(void* lp, unsigned ucb) {
    typedef BOOL(WINAPI* IsBadWritePtr_t)(LPVOID, UINT_PTR);
    IsBadWritePtr_t fn = *reinterpret_cast<IsBadWritePtr_t*>(c3cm_sound(0x000b84bc));   // 0x10049610
    int esi = 1;                                                                        // 0x10049605
    if (fn(lp, ucb) != 0) esi = 0;                                                      // 0x10049618, 0x1004961a
    return esi;
}

// 0x00051319  cvtdate (VA 0x10051319)
// Eleven dword arguments on an ebp frame, nothing popped (`ret` at 0x10051458): __cdecl. Read in frame order,
// they are [ebp+8] at 0x100513cf, [ebp+0xc] at 0x1005131c, [ebp+0x10] (kept in ebx) at 0x10051321, [ebp+0x14] at
// 0x1005132b and 0x100513b4, [ebp+0x18] at 0x10051370 and 0x10051391, [ebp+0x1c] at 0x10051374, [ebp+0x20] at
// 0x100513cc, [ebp+0x24] at 0x100513d5 and 0x10051400, [ebp+0x28] at 0x100513e1, [ebp+0x2c] at 0x100513ed and
// [ebp+0x30] at 0x100513f6.
// Two dword tables of thirteen entries are indexed by [ebp+0x14]: the one at 0x1006891c (read at 0x1005133c and
// 0x100513bc) when the low two bits of [ebp+0x10] are zero, the one at 0x10068950 (0x10051347, 0x100513c5)
// otherwise; the entry one slot further on is read at 0x1005139d and 0x100513a5 (0x10068920 and 0x10068954 are
// those same tables plus four). The dword at 0x10068874 is read once, at 0x10051412.
// Branch at 0x10051325 ([ebp+0xc] == 1): the day number is the table entry plus one (0x1005134f) moved to the
// [ebp+0x1c]-th weekday of the [ebp+0x18]-th week. The weekday comes from a signed division by 7 of
// [ebp+0x10]*365 + entry + 1 + (([ebp+0x10]-1) >> 2) - 25563 (0x10051352, 0x10051358, 0x1005135c, 0x10051365,
// 0x1005136e), keeping only the remainder; that remainder is compared with [ebp+0x1c] (jg at 0x10051377) and the
// two arms differ by exactly seven (0x10051381 and 0x1005138f). When [ebp+0x18] is 5 (jne at 0x10051395) and the
// result lies past the next table entry (jle at 0x100513ad) seven is subtracted again (0x100513af).
// Otherwise ([ebp+0xc] != 1, 0x100513b4) the day number is the table entry plus [ebp+0x20] (0x100513cc).
// Branch at 0x100513d3 ([ebp+8] == 1): the day number goes to [0x10068904], [ebp+0x10] to [0x10068900] and the
// time of day in milliseconds, ((([ebp+0x24]*60)+[ebp+0x28])*60+[ebp+0x2c])*1000+[ebp+0x30], to [0x10068908]
// (0x100513d8, 0x100513e4, 0x100513f9). Otherwise the same three go to [0x10068914], [0x10068910] and
// [0x10068918], the dword at 0x10068874 is added to the seconds before the multiplication by 1000 (0x10051412)
// and the millisecond count is normalised into one day: negative adds 86400000 and decrements the day number
// (jns at 0x10051429, 0x1005142b, 0x10051430), 86400000 or more subtracts it and increments the day number
// (jl at 0x1005143f, 0x10051441, 0x10051443); either way the corrected day number is stored again at 0x10051449,
// and [ebp+0x10] is stored at 0x1005144f on all three paths.
// Every arithmetic step is a 32-bit wrapping integer operation and the division is `cdq`/`idiv` (truncation
// toward zero, the remainder taking the sign of the dividend), which is what C++ `%` does.
typedef void(__cdecl* Cvtdate_t)(int, int, int, int, int, int, int, int, int, int, int);
Cvtdate_t Cvtdate_orig;
void __cdecl Cvtdate_re(int trantype, int datetype, int yr, int month, int week, int dayofweek, int date,
                        int hour, int min, int sec, int msec) {
    const int* tableA = reinterpret_cast<const int*>(c3cm_sound(0x0006891c));
    const int* tableB = reinterpret_cast<const int*>(c3cm_sound(0x00068950));
    int* g = reinterpret_cast<int*>(c3cm_sound(0x00068900));          // [0]=0x10068900 .. [6]=0x10068918
    const int bias = *reinterpret_cast<const int*>(c3cm_sound(0x00068874));

    int day;
    if (datetype == 1) {                                              // 0x1005131c, 0x10051325
        const int yrLow = yr & 3;                                     // 0x1005132e, 0x10051331
        const int* table = (yrLow != 0) ? tableB : tableA;            // 0x10051337
        day = table[month] + 1;                                       // 0x1005133c / 0x10051347, 0x1005134f
        const unsigned num = static_cast<unsigned>(yr) * 365u         // 0x10051352
                           + static_cast<unsigned>(day)
                           + static_cast<unsigned>((yr - 1) >> 2)     // 0x10051358, 0x1005135c
                           - 25563u;                                  // 0x10051365
        const int rem = static_cast<int>(num) % 7;                    // 0x1005136e
        const unsigned shift = static_cast<unsigned>(week) * 7u       // 0x10051379 / 0x10051387
                             - static_cast<unsigned>(rem)
                             + static_cast<unsigned>(dayofweek);
        if (rem > dayofweek)                                          // 0x10051374, 0x10051377
            day = static_cast<int>(static_cast<unsigned>(day) + shift);          // 0x1005138f
        else
            day = static_cast<int>(static_cast<unsigned>(day) + shift - 7u);     // 0x10051381
        if (week == 5) {                                              // 0x10051391, 0x10051395
            const int* t2 = (yrLow != 0) ? tableB : tableA;           // 0x1005139b
            if (day > t2[month + 1]) day -= 7;                        // 0x1005139d / 0x100513a5, 0x100513ad
        }
    } else {                                                          // 0x100513b4
        const int* table = ((yr & 3) != 0) ? tableB : tableA;         // 0x100513b7, 0x100513ba
        day = table[month] + date;                                    // 0x100513bc / 0x100513c5, 0x100513cc
    }

    if (trantype == 1) {                                              // 0x100513cf, 0x100513d3
        g[1] = day;                                                   // 0x100513d8
        g[0] = yr;                                                    // 0x100513e4
        unsigned ms = static_cast<unsigned>(hour) * 60u;              // 0x100513de
        ms = (ms + static_cast<unsigned>(min)) * 60u;                 // 0x100513e1, 0x100513ea
        ms = (ms + static_cast<unsigned>(sec)) * 1000u;               // 0x100513ed, 0x100513f0
        ms += static_cast<unsigned>(msec);                            // 0x100513f6
        g[2] = static_cast<int>(ms);                                  // 0x100513f9
    } else {                                                          // 0x10051400
        g[5] = day;                                                   // 0x10051403
        unsigned ms = static_cast<unsigned>(hour) * 60u;              // 0x10051409
        ms = (ms + static_cast<unsigned>(min)) * 60u;                 // 0x1005140c, 0x1005140f
        ms += static_cast<unsigned>(bias);                            // 0x10051412
        ms = (ms + static_cast<unsigned>(sec)) * 1000u;               // 0x10051418, 0x1005141b
        ms += static_cast<unsigned>(msec);                            // 0x10051421
        g[6] = static_cast<int>(ms);                                  // 0x10051424
        if (static_cast<int>(ms) < 0) {                               // 0x10051429
            ms += 0x5265c00u;                                         // 0x1005142b
            --day;                                                    // 0x10051430
            g[6] = static_cast<int>(ms);                              // 0x10051431
            g[5] = day;                                               // 0x10051449
        } else if (static_cast<int>(ms) >= 0x5265c00) {               // 0x10051438, 0x1005143d, 0x1005143f
            ms -= 0x5265c00u;                                         // 0x10051441
            ++day;                                                    // 0x10051443
            g[6] = static_cast<int>(ms);                              // 0x10051444
            g[5] = day;                                               // 0x10051449
        }
        g[4] = yr;                                                    // 0x1005144f
    }
}

}  // namespace

SG_HOOK("sound.dll", 0x00042690, sound_strstr, Strstr_re, Strstr_orig);
SG_HOOK("sound.dll", 0x00045680, sound_strchr, Strchr_re, Strchr_orig);
SG_HOOK("sound.dll", 0x0004a010, sound_strcpy, Strcpy_re, Strcpy_orig);
SG_HOOK("sound.dll", 0x00048268, sound_doubleExponentField, DoubleExpField_re, DoubleExpField_orig);
SG_HOOK("sound.dll", 0x00041aad, sound_iosResetVtable, IosResetVtable_re, IosResetVtable_orig);
SG_HOOK("sound.dll", 0x00049604, sound_crtValidatePointer, CrtValidatePointer_re, CrtValidatePointer_orig);
SG_HOOK("sound.dll", 0x00051319, sound_cvtdate, Cvtdate_re, Cvtdate_orig);

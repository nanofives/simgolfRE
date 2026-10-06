// C3 batch c3b: util helpers of golf_clean.exe (distance math, in-place string trims, text/string-table
// appends, the block-pool allocator). Hand-written from the C2 transcriptions re/analysis/util/<addr>_*.md and
// the 100% matched sources (re/match/golf_util.cpp, golf_small4/6/9/12/25.cpp, golf_freeblock.cpp); each body
// cites the addresses its facts come from. Callees are reached through their original addresses.
// __thiscall is not used here: every function is __cdecl.
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "hooks.h"

namespace {

template <typename T> T& at(unsigned addr) { return *reinterpret_cast<T*>(addr); }

typedef int(__cdecl* Int1_t)(int);
typedef int(__cdecl* Int2_t)(int, int);
typedef int(__cdecl* Int4_t)(int, int, int, int);
typedef void(__cdecl* VStr_t)(char*);
typedef void(__cdecl* V2Str_t)(const char*, const char*);
typedef void(__cdecl* V2i_t)(int, int);

// Callees reached through their original addresses (a hooked callee runs its own reimplementation).
const Int2_t kDistance = reinterpret_cast<Int2_t>(0x0040acd0);   // distance
const Int2_t kSinScaled = reinterpret_cast<Int2_t>(0x00491c70);  // sinScaled (pure, no callees)

// 0x0040acd0  distance(dx, dy): length of (dx, dy) in fixed-point units. A component whose magnitude exceeds
// 0x4000 is divided by 8 first and the scale multiplied by 8 (0x0040acd0, threshold 0x4000), so dx*dx + dy*dy
// never overflows; the result is sqrt(dx*dx + dy*dy) * scale truncated to int (the __ftol callee 0x004a6030).
// VC6's __ftol truncates through a 64-bit fistp and keeps the low 32 bits, so NaN (sqrt of a negative sum after
// dx*dx overflows, e.g. dx = INT_MAX) and values >= 2^31 give the low dword of the 64-bit result (0 for NaN), not
// the 0x80000000 of a 32-bit conversion: ftol() below reproduces that (path-1 A/B, 2026-10-07).
inline int ftol(double v) { return static_cast<int>(static_cast<long long>(v)); }
Int2_t Distance_orig;
int __cdecl Distance_re(int dx, int dy) {
    int scale = 1;
    if (abs(dx) > 0x4000) { scale = 8; dx /= 8; }
    if (abs(dy) > 0x4000) { dy /= 8; scale *= 8; }
    return ftol(sqrt(static_cast<double>(dx * dx + dy * dy)) * scale);
}

// 0x0040c4b0  tileDistance(x, y, tx, ty): distance from world point (x, y) to the centre of tile (tx, ty)
// (0x400 units per tile, +0x200 to reach the centre), scaled by 25 / 1024 (0x0040c4b0). Calls distance 0x0040acd0.
Int4_t TileDistance_orig;
int __cdecl TileDistance_re(int x, int y, int tx, int ty) {
    return kDistance(x - tx * 0x400 - 0x200, y - ty * 0x400 - 0x200) * 25 / 1024;
}

// 0x00467270  foldRange(a, b): reflects a into [0, 0x40000000) and returns sinScaled(a, b) (0x00491c70). When
// a's sign bit is set, b is negated and the sign bit cleared (je at 0x0046727d); when bit 0x40000000 is then
// set, a is replaced by 0x7fffffff - a (je at 0x0046728b).
Int2_t FoldRange_orig;
int __cdecl FoldRange_re(int a, int b) {
    if (a & 0x80000000) { b = -b; a &= 0x7fffffff; }
    if (a & 0x40000000) a = 0x7fffffff - a;
    return kSinScaled(a, b);
}

// 0x004924e0  trimLeadingSpace(s): removes leading isspace bytes in place. Advances past the run of leading
// isspace characters (0x004924e0, isspace callee 0x004a6598), copies the remainder through a 0x200-byte stack
// buffer and back over s.
VStr_t TrimLeadingSpace_orig;
void __cdecl TrimLeadingSpace_re(char* s) {
    char buf[0x200];
    char* p = s;
    if (isspace(static_cast<unsigned char>(*p))) {
        do { p++; } while (isspace(static_cast<unsigned char>(*p)));
    }
    strcpy(buf, p);
    strcpy(s, buf);
}

// 0x00492570  trimTrailingSpace(s): removes trailing isspace bytes in place. With a non-empty string it walks
// from the last byte toward the front writing NUL over each isspace byte and stops at the first non-space or at s
// (0x00492570, strlen and isspace callee 0x004a6598).
VStr_t TrimTrailingSpace_orig;
void __cdecl TrimTrailingSpace_re(char* s) {
    int n = static_cast<int>(strlen(s));
    if (!n) return;
    for (char* p = s + n - 1; p >= s; p--) {
        if (!isspace(static_cast<unsigned char>(*p))) return;
        *p = 0;
        if (p == s) return;
    }
}

// 0x0045b9f0  appendString(id): appends string `id` of the string table to the shared text buffer; returns 1 when
// the entry exists, else 0. id == -1 returns 0 (je at 0x0045b9f7); the signed word offset at 0x0059d81c + id*2
// (0x0045b9f9) == -1 returns 0 (je at 0x0045ba05); otherwise strcat of 0x0056fcb0 + offset onto the text buffer
// 0x0051a068 (0x0045ba0c, 0x0045ba21) and returns 1.
Int1_t AppendString_orig;
int __cdecl AppendString_re(int id) {
    if (id != -1) {
        const short off = at<short>(0x0059d81c + id * 2);
        if (off != -1) {
            strcat(reinterpret_cast<char*>(0x0051a068), reinterpret_cast<const char*>(0x0056fcb0 + off));
            return 1;
        }
    }
    return 0;
}

// 0x0045b7c0  replaceInText(key, repl): replaces the first occurrence of `key` in the text buffer 0x0051a068 with
// `repl` (0x0045b7c0, strstr callee 0x004a64f0). When key is found, its tail (from the match on) is saved in a
// 512-byte stack buffer, the match is truncated, `repl` is appended, then the saved tail past key's length is
// appended. When key is absent nothing changes.
V2Str_t ReplaceInText_orig;
void __cdecl ReplaceInText_re(const char* key, const char* repl) {
    char tail[512];
    char* text = reinterpret_cast<char*>(0x0051a068);
    char* p = strstr(text, key);
    if (p) {
        strcpy(tail, p);
        *p = 0;
        strcat(text, repl);
        strcat(text, tail + strlen(key));
    }
}

// The block pool is 100 (start, size) free regions: starts at 0x00820b70, sizes at 0x00820d00 (int each); a
// start < 0 marks an empty slot.
int& blockStart(int i) { return at<int>(0x00820b70 + i * 4); }
int& blockSize(int i) { return at<int>(0x00820d00 + i * 4); }

// 0x0043d5d0  allocBlock(n): first-fit allocation of n units. Scans the 100 regions for the first with start >= 0
// and size >= n (0x0043d5d0); carves n off its front (start += n, size -= n) when size > n, else consumes it whole
// (size = 0, start = -1); returns the region's old start, or -1 when none fits.
Int1_t AllocBlock_orig;
int __cdecl AllocBlock_re(int n) {
    for (int i = 0; i < 100; i++) {
        if (blockStart(i) >= 0 && blockSize(i) >= n) {
            int r = blockStart(i);
            if (blockSize(i) > n) {
                blockStart(i) = r + n;
                blockSize(i) -= n;
            } else {
                blockSize(i) = 0;
                blockStart(i) = -1;
            }
            return r;
        }
    }
    return -1;
}

// 0x0043d520  freeBlock(base, n): returns n units at `base` to the pool. If a region ends exactly at base
// (start >= 0 && base == start + size), it is extended by n and any region that now begins at its new end is
// merged in and emptied (start = -1, size = 0) (0x0043d520). Otherwise the (base, n) region is placed in the first
// empty slot (start < 0).
V2i_t FreeBlock_orig;
void __cdecl FreeBlock_re(int base, int n) {
    for (int i = 0; i < 100; i++) {
        if (blockStart(i) >= 0 && base == blockStart(i) + blockSize(i)) {
            blockSize(i) += n;
            for (int j = 0; j < 100; j++) {
                if (blockStart(j) == blockStart(i) + blockSize(i)) {
                    blockSize(i) += blockSize(j);
                    blockStart(j) = -1;
                    blockSize(j) = 0;
                }
            }
            return;
        }
    }
    for (int i = 0; i < 100; i++) {
        if (blockStart(i) < 0) {
            blockStart(i) = base;
            blockSize(i) = n;
            return;
        }
    }
}

}  // namespace

SG_HOOK("golf_clean.exe", 0x0040acd0, distance, Distance_re, Distance_orig);
SG_HOOK("golf_clean.exe", 0x0040c4b0, tileDistance, TileDistance_re, TileDistance_orig);
SG_HOOK("golf_clean.exe", 0x00467270, foldRange, FoldRange_re, FoldRange_orig);
SG_HOOK("golf_clean.exe", 0x004924e0, trimLeadingSpace, TrimLeadingSpace_re, TrimLeadingSpace_orig);
SG_HOOK("golf_clean.exe", 0x00492570, trimTrailingSpace, TrimTrailingSpace_re, TrimTrailingSpace_orig);
SG_HOOK("golf_clean.exe", 0x0045b9f0, appendString, AppendString_re, AppendString_orig);
SG_HOOK("golf_clean.exe", 0x0045b7c0, replaceInText, ReplaceInText_re, ReplaceInText_orig);
SG_HOOK("golf_clean.exe", 0x0043d5d0, allocBlock, AllocBlock_re, AllocBlock_orig);
SG_HOOK("golf_clean.exe", 0x0043d520, freeBlock, FreeBlock_re, FreeBlock_orig);

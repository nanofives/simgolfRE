// C3 batch c3w (2026-10-07): util/libjpeg leaves with static callers that the test scenarios never reach.
// Hand-written from the disassembly / decompilation (re/tools/decomp.py, asm2inline.py); each body cites the
// addresses its facts come from. All __cdecl leaves. Registered at the bottom with SG_HOOK.
#include <string.h>

#include "hooks.h"

namespace {

typedef int(__cdecl* Int1_t)(int);
typedef int(__cdecl* Int2_t)(int, int);

// 0x004ae700  jpeg_quality_scaling(q): IJG libjpeg. q < 1 (0x004ae705) -> 5000; q < 0x32 (50, 0x004ae712) ->
// signed 5000 / q; q > 100 (the q < 0x65 test at 0x004ae70c failing) clamps q to 100 (0x004ae71c); the tail
// returns (100 - q) * 2 (0x004ae724..0x004ae72e).
Int1_t JpegQualityScaling_orig;
int __cdecl JpegQualityScaling_re(int q) {
    if (q < 1) return 5000;
    if (q < 50) return 5000 / q;
    if (q > 100) q = 100;
    return (100 - q) * 2;
}

// 0x0042e7a0  directionOf(dx, dy): abs via cdq/xor/sub of dx (ecx) and dy (esi). When |dx| > |dy| (jle at
// 0x0042e7bc falls through) returns dx <= 0 ? 6 : 2 (setle/and 0xfc/add 6 at 0x0042e7c1..0x0042e7ca); otherwise
// (|dx| <= |dy|, the vertical branch at 0x0042e7ce) returns dy <= 0 ? 0 : 4 (and 4 at 0x0042e7d8).
Int2_t DirectionOf_orig;
int __cdecl DirectionOf_re(int dx, int dy) {
    const int adx = dx < 0 ? -dx : dx;
    const int ady = dy < 0 ? -dy : dy;
    if (adx > ady) return dx <= 0 ? 6 : 2;
    return dy <= 0 ? 0 : 4;
}

// 0x004935f0  findByteInRange(p): p == 0 (0x004935f8) -> NULL. Else scan from p (signed char): stop and return
// the pointer at the first byte c with lo <= c <= hi, where lo = the signed byte at 0x004bba88 ('0' = 0x30) and
// hi = 0x004bba89 ('9' = 0x39) (jl at 0x00493614, jg at 0x00493618, both signed); a NUL byte (je 0x00493610)
// returns NULL.
typedef char*(__cdecl* FindByte_t)(int);
FindByte_t FindByteInRange_orig;
char* __cdecl FindByteInRange_re(int p) {
    if (p == 0) return 0;
    const signed char lo = *reinterpret_cast<const signed char*>(0x004bba88);
    const signed char hi = *reinterpret_cast<const signed char*>(0x004bba89);
    char* s = reinterpret_cast<char*>(p);
    for (;;) {
        const signed char c = static_cast<signed char>(*s);
        if (c == 0) return 0;
        if (!(c < lo || hi < c)) return s;
        s++;
    }
}

// 0x0045de30  swapTableEntry(a, b): swaps two 0x100-byte records of the golfer table (base 0x005794b8, stride
// 0x100 = 0x40 dwords: shl 8 / lea +0x5794b8 at 0x0045de35..0x0045de3e) through the 0x100-byte scratch at
// 0x00582cb8 (0x0045de44). Three rep movsd of 0x40 dwords: record[a] -> scratch, record[b] -> record[a],
// scratch -> record[b].
typedef void(__cdecl* Swap_t)(int, int);
Swap_t SwapTableEntry_orig;
void __cdecl SwapTableEntry_re(int a, int b) {
    unsigned char* const base = reinterpret_cast<unsigned char*>(0x005794b8);
    unsigned* const scratch = reinterpret_cast<unsigned*>(0x00582cb8);
    unsigned* const pa = reinterpret_cast<unsigned*>(base + a * 0x100);
    unsigned* const pb = reinterpret_cast<unsigned*>(base + b * 0x100);
    for (int i = 0; i < 0x40; i++) scratch[i] = pa[i];
    for (int i = 0; i < 0x40; i++) pa[i] = pb[i];
    for (int i = 0; i < 0x40; i++) pb[i] = scratch[i];
}

// 0x00405ac0  sanitizeFileName(s): returns false when s contains any of "*|:<>?/\\" (strpbrk, charset
// 0x004c3ee0, test at 0x00405ad0). Otherwise trims trailing spaces in place (the last char at s[len-1] is set to
// NUL while it is ' ', 0x00405af4..0x00405b03) and returns whether the trimmed string is non-empty
// (0 < len, 0x00405b06). An all-spaces or empty string returns false.
typedef bool(__cdecl* Sanitize_t)(char*);
Sanitize_t SanitizeFileName_orig;
bool __cdecl SanitizeFileName_re(char* s) {
    if (strpbrk(s, "*|:<>?/\\")) return false;
    for (;;) {
        const int len = static_cast<int>(strlen(s));
        if (len == 0) return false;
        if (s[len - 1] != ' ') return true;
        s[len - 1] = '\0';
    }
}

// 0x004b04f0  jcopy_sample_rows(src, src_row, dst, dst_row, num_rows, num_bytes): IJG libjpeg. Row-pointer
// arrays src (at src + src_row*4) and dst (at dst + dst_row*4); copies num_rows rows (loop guarded by
// 0 < num_rows at 0x004b0506) of num_bytes bytes each (dword body + byte tail = memcpy, 0x004b051a..0x004b0540).
typedef void(__cdecl* JCopy_t)(int, int, int, int, int, unsigned);
JCopy_t JcopySampleRows_orig;
void __cdecl JcopySampleRows_re(int src, int src_row, int dst, int dst_row, int num_rows, unsigned num_bytes) {
    unsigned* sp = reinterpret_cast<unsigned*>(src + src_row * 4);
    unsigned* dp = reinterpret_cast<unsigned*>(dst + dst_row * 4);
    for (int r = 0; r < num_rows; r++)
        memcpy(reinterpret_cast<void*>(dp[r]), reinterpret_cast<void*>(sp[r]), num_bytes);
}

// 0x004b4390  expand_right_edge(image_data, num_rows, input_cols, output_cols): IJG libjpeg. Count =
// output_cols - input_cols; when count > 0 and num_rows > 0 (0x004b43a2) each row (row pointer image_data[r])
// is padded from column input_cols with its last valid pixel row[input_cols-1] (0x004b43ba) for count bytes
// (dword body + byte tail = memset, 0x004b43c4..0x004b43e4).
typedef void(__cdecl* Expand_t)(int*, int, int, int);
Expand_t ExpandRightEdge_orig;
void __cdecl ExpandRightEdge_re(int* image_data, int num_rows, int input_cols, int output_cols) {
    const int count = output_cols - input_cols;
    if (count > 0 && num_rows > 0) {
        for (int r = 0; r < num_rows; r++) {
            unsigned char* row = reinterpret_cast<unsigned char*>(image_data[r]);
            const unsigned char pad = row[input_cols - 1];
            memset(row + input_cols, pad, count);
        }
    }
}

// 0x004ae220  jpeg_suppress_tables(cinfo, suppress): IJG libjpeg. For the 4 quant-table pointers at cinfo+0x40
// (0x004ae225) writes suppress to *(tbl+0x80) when the pointer is non-NULL. For the 4 DC huff-table pointers at
// cinfo+0x50 and the 4 AC huff-table pointers at cinfo+0x60 (the piVar1[-4] / *piVar1 pair, loop at
// 0x004ae246..0x004ae26f) writes suppress to *(tbl+0x114) when non-NULL.
Int2_t JpegSuppressTables_orig;
void __cdecl JpegSuppressTables_re(int cinfo, int suppress) {
    int* const quant = reinterpret_cast<int*>(cinfo + 0x40);
    for (int i = 0; i < 4; i++)
        if (quant[i]) *reinterpret_cast<int*>(quant[i] + 0x80) = suppress;
    int* const dc = reinterpret_cast<int*>(cinfo + 0x50);
    int* const ac = reinterpret_cast<int*>(cinfo + 0x60);
    for (int i = 0; i < 4; i++) {
        if (dc[i]) *reinterpret_cast<int*>(dc[i] + 0x114) = suppress;
        if (ac[i]) *reinterpret_cast<int*>(ac[i] + 0x114) = suppress;
    }
}

}  // namespace

SG_HOOK("golf_clean.exe", 0x004ae700, jpeg_quality_scaling, JpegQualityScaling_re, JpegQualityScaling_orig);
SG_HOOK("golf_clean.exe", 0x0042e7a0, directionOf, DirectionOf_re, DirectionOf_orig);
SG_HOOK("golf_clean.exe", 0x004935f0, findByteInRange, FindByteInRange_re, FindByteInRange_orig);
SG_HOOK("golf_clean.exe", 0x0045de30, swapTableEntry, SwapTableEntry_re, SwapTableEntry_orig);
SG_HOOK("golf_clean.exe", 0x00405ac0, sanitizeFileName, SanitizeFileName_re, SanitizeFileName_orig);
SG_HOOK("golf_clean.exe", 0x004b04f0, jcopy_sample_rows, JcopySampleRows_re, JcopySampleRows_orig);
SG_HOOK("golf_clean.exe", 0x004b4390, expand_right_edge, ExpandRightEdge_re, ExpandRightEdge_orig);
SG_HOOK("golf_clean.exe", 0x004ae220, jpeg_suppress_tables, JpegSuppressTables_re, JpegSuppressTables_orig);

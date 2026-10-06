// C3 batch c3e (2026-10-06) of golf_clean.exe: terrain/course/input/golfer/sim readers and writers.
// Hand-written from the disassembly (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr> --list) and the
// mechanical C2 notes in re/analysis/<subsystem>/<addr>_<name>.md; every body cites the address of each global,
// table, callee and branch it relies on. Callees are invoked through their original VAs (a hooked callee runs its
// own reimplementation, which the path-1 A/B drives for both arms, so the comparison isolates this function's body).
#include "hooks.h"

namespace {

template <typename T> T at(unsigned addr) { return *reinterpret_cast<const T*>(addr); }

typedef int(__cdecl* Int1_t)(int);
typedef int(__cdecl* Int2_t)(int, int);
typedef void(__cdecl* Void1_t)(int);
typedef void(__cdecl* VoidV_t)();

// Callees invoked at their original VAs.
const Int2_t kFoldRange         = reinterpret_cast<Int2_t>(0x00467270);   // foldRange(a, b)
const Int2_t kAbsDiff           = reinterpret_cast<Int2_t>(0x004672B0);   // absDiff(a, b)
const Void1_t kUpdatePairSnapshot = reinterpret_cast<Void1_t>(0x00409950); // updatePairSnapshot(p)

// 0x004674c0  bilinearSample(a, b): bilinear interpolation of the signed-byte grid at 0x00838c1c (row stride 0x13
// = 19). Each axis is biased by -0x80 (0x004674c9/0x004674d0); the integer tile index is (v>>8)&0xf (0x004674dc..e5)
// and the sub-tile weight is (v>>3)&0x1f (0x004674f2..0x0046750c), with ifx/ify = 0x20 - weight (0x004674ea/515).
// base = iy + ix*19 (0x004674e8/ef/f5). The four corners are grid[base+0], [+1], [+19], [+20]
// (0x00467526/4fe/505/52d); the weighted sum is divided by 32 (cdq/and 0x1f/sar 5 at 0x00467546..0x0046754e,
// i.e. C truncation toward zero).
Int2_t BilinearSample_orig;
int __cdecl BilinearSample_re(int a, int b) {
    const int ca = a - 0x80, cb = b - 0x80;
    const int ix = (ca >> 8) & 0xf, iy = (cb >> 8) & 0xf;
    const int fx = (ca >> 3) & 0x1f, fy = (cb >> 3) & 0x1f;
    const int ifx = 0x20 - fx, ify = 0x20 - fy;
    const int base = iy + ix * 19;
    const signed char* g = reinterpret_cast<const signed char*>(0x00838C1C);
    const int c00 = g[base + 0], c01 = g[base + 1], c10 = g[base + 19], c11 = g[base + 20];
    const int sum = c01 * ifx * fy + c10 * ify * fx + c00 * ifx * ify + c11 * fy * fx;
    return sum / 32;
}

// 0x0040df80  objectAt(x, y): scans the 256 placed-object records at 0x0058bcb8 (stride 0x10, end 0x0058ccb8;
// type word +0, rx word +2, ry word +4). A record with type -1 is skipped (0x0040df96). Its footprint side is the
// signed byte at 0x004c26c0 + type*0x14 (0x0040dfa6), extended by the dword at 0x005a8c38 + type*4, minus 1, when
// type >= 6 and type != 7 (0x0040dfae..0x0040dfc3). The record covers (x,y) when rx <= x < rx+side (0x0040dfcd/d3)
// and ry <= y < ry+side (0x0040dfdb/e1); returns that record's index, or -1 (0x0040dfef).
Int2_t ObjectAt_orig;
int __cdecl ObjectAt_re(int x, int y) {
    const unsigned char* base = reinterpret_cast<const unsigned char*>(0x0058BCB8);
    const signed char* sizeTab = reinterpret_cast<const signed char*>(0x004C26C0);  // stride 0x14
    const int* widthTab = reinterpret_cast<const int*>(0x005A8C38);                  // stride 4
    for (int i = 0; i < 256; i++) {
        const unsigned char* r = base + i * 0x10;
        const short type = *reinterpret_cast<const short*>(r);
        if (type == -1) continue;
        int side = sizeTab[type * 0x14];
        if (type >= 6 && type != 7) side += widthTab[type] - 1;
        const int rx = *reinterpret_cast<const short*>(r + 2);
        if (x < rx) continue;
        if (x >= rx + side) continue;
        const int ry = *reinterpret_cast<const short*>(r + 4);
        if (y < ry) continue;
        if (y < ry + side) return i;
    }
    return -1;
}

// 0x0047eee0  matchInputCode(codePtr, len): compares `len` bytes of the circular keystroke buffer
// 0x0083aba4..0x0083abad (cursor pointer at 0x004e42ec, 0x0047eeee) backward against the string at codePtr. edx
// starts at codePtr + len - 1 (0x0047eeea) and the buffer cursor decrements with wrap to 0x0083abad once it falls
// below 0x0083aba4 (0x0047ef03..0x0047ef0a). Returns 1 on the first differing byte (0x0047ef19) and 0 when all
// `len` bytes are equal, or when len <= 0 (0x0047eef7/0x0047ef15).
Int2_t MatchInputCode_orig;
int __cdecl MatchInputCode_re(int codePtr, int len) {
    if (len <= 0) return 0;
    const unsigned char* code = reinterpret_cast<const unsigned char*>(codePtr + len - 1);
    const unsigned char* buf = *reinterpret_cast<const unsigned char* const*>(0x004E42EC);
    int n = len;
    do {
        if (*buf != *code) return 1;
        --buf;
        --code;
        if (reinterpret_cast<unsigned>(buf) < 0x0083ABA4u)
            buf = reinterpret_cast<const unsigned char*>(0x0083ABAD);
        --n;
    } while (n > 0);
    return 0;
}

// 0x00401000  resetRecordBank(p): fills 8 dwords at 0x004e6d20 + p*0x74 and 9 dwords at 0x004e6d70 + p*0x74 with
// -1 (the two `rep stosd` at 0x00401022/0x00401032; p*0x74 is 29*p<<2, 0x00401005..0x00401016). Frees record bank p.
Void1_t ResetRecordBank_orig;
void __cdecl ResetRecordBank_re(int p) {
    const int off = p * 0x74;
    int* a = reinterpret_cast<int*>(0x004E6D20 + off);
    for (int i = 0; i < 8; i++) a[i] = -1;
    int* b = reinterpret_cast<int*>(0x004E6D70 + off);
    for (int i = 0; i < 9; i++) b[i] = -1;
}

// 0x00409950  updatePairSnapshot(p): record base rec = p*0x388 (0x00409956..64). Reads the pair counter (signed word
// at 0x0059fc64 + rec, 0x00409967) and golfer id (signed word at 0x0059fc60 + rec, 0x0040996e); s = id << 8
// (0x00409979/84). Into slot = rec + counter*4 (0x0040999c..a0) it stores three cached golfer attributes: when
// counter != 0 the dwords at s+0x57958c / s+0x579590 / s+0x579594, otherwise s+0x57957c / s+0x579580 / 0
// (0x0040997c..0x004099cf), written to 0x0059fc68 / 0x0059fce8 / 0x0059fd68 + slot (0x004099a5/bd/d3). Increments the
// counter word while it is below 0x1f (0x004099d0/dc).
Void1_t UpdatePairSnapshot_orig;
void __cdecl UpdatePairSnapshot_re(int p) {
    const int rec = p * 0x388;
    const int cnt = at<short>(0x0059FC64 + rec);
    const int id = at<short>(0x0059FC60 + rec);
    const unsigned s = static_cast<unsigned>(id) << 8;
    const int slot = rec + cnt * 4;
    const int a1 = (cnt != 0) ? at<int>(s + 0x0057958C) : at<int>(s + 0x0057957C);
    *reinterpret_cast<int*>(0x0059FC68 + slot) = a1;
    const int a2 = (cnt != 0) ? at<int>(s + 0x00579590) : at<int>(s + 0x00579580);
    *reinterpret_cast<int*>(0x0059FCE8 + slot) = a2;
    const int a3 = (cnt != 0) ? at<int>(s + 0x00579594) : 0;
    *reinterpret_cast<int*>(0x0059FD68 + slot) = a3;
    if (cnt < 0x1f)
        *reinterpret_cast<short*>(0x0059FC64 + rec) = static_cast<short>(cnt + 1);
}

// 0x004099f0  addGolferPair(g): finds the first free slot (header word == -1) in g_storyPairs at 0x0059fc60
// (stride 0x388, end 0x005a34e0, 0x004099f7..0x00409a0a); if none is free it returns without writing. Into slot off
// = slot*0x388 it stores g as the id word (0x0059fc60 + off, 0x00409a26), the partner word at 0x0057955a + g*0x100
// (0x0059fc62 + off, 0x00409a33/3a), and clears the counter word (0x0059fc64 + off, 0x00409a41). It then calls
// updatePairSnapshot(slot) (0x00409a4a) and copies both golfers' 0x40-dword records (0x005794b8 + id*0x100) into the
// slot regions at 0x0059fde8 + off and 0x0059fee8 + off (the two `rep movsd` at 0x00409a63/0x00409a80).
Void1_t AddGolferPair_orig;
void __cdecl AddGolferPair_re(int g) {
    int slot = 0;
    unsigned rec = 0x0059FC60;
    while (at<short>(rec) != -1) {
        rec += 0x388;
        slot++;
        if (rec >= 0x005A34E0) return;
    }
    const int off = slot * 0x388;
    *reinterpret_cast<short*>(0x0059FC60 + off) = static_cast<short>(g);
    const unsigned s = static_cast<unsigned>(g) << 8;
    *reinterpret_cast<short*>(0x0059FC62 + off) = at<short>(s + 0x0057955A);
    *reinterpret_cast<short*>(0x0059FC64 + off) = 0;
    kUpdatePairSnapshot(slot);
    const int* src1 = reinterpret_cast<const int*>(s + 0x005794B8);
    int* dst1 = reinterpret_cast<int*>(0x0059FDE8 + off);
    for (int i = 0; i < 0x40; i++) dst1[i] = src1[i];
    const unsigned ps = static_cast<unsigned>(at<short>(s + 0x0057955A)) << 8;
    const int* src2 = reinterpret_cast<const int*>(ps + 0x005794B8);
    int* dst2 = reinterpret_cast<int*>(0x0059FEE8 + off);
    for (int i = 0; i < 0x40; i++) dst2[i] = src2[i];
}

// 0x00409bf0  updateRollingStats(): sweeps the 256 records at 0x005736bc (stride 0x24, end 0x00575abc), skipping any
// whose liveness dword [-4] is -1 (0x00409bfd/c00). For a live record it adds foldRange([+8], [+0xc]/16) to [-4]
// (0x00409c06..21), subtracts absDiff([+8], [+0xc]/16) from [+0] (0x00409c24..41), and adds [+0x10]/32 to [+4]
// (0x00409c43..56). When that sum is <= 0 it zeroes [+0x10], [+0xc], [+4] and frees the record ([-4] = -1) if the
// global date 0x00834170 is past [+0x14] + 0x400 (0x00409c59..78). Finally it decays [+0xc] by [+0xc]>>4 (0x00409c7b..85)
// and, unless both [+4] and [+0x10] are zero, subtracts 0x40 from [+0x10] (0x00409c88..94). The /16 and /32 are C
// truncating divisions (the cdq/and/add/sar idioms); the >>4 decay is an arithmetic shift.
VoidV_t UpdateRollingStats_orig;
void __cdecl UpdateRollingStats_re() {
    for (unsigned e = 0x005736BC; e < 0x00575ABC; e += 0x24) {
        int* r = reinterpret_cast<int*>(e);
        if (r[-1] == -1) continue;
        r[-1] += kFoldRange(r[2], r[3] / 16);
        r[0] -= kAbsDiff(r[2], r[3] / 16);
        const int acc = r[1] + r[4] / 32;
        r[1] = acc;
        if (acc <= 0) {
            const int date = at<int>(0x00834170);
            r[4] = 0;
            r[3] = 0;
            r[1] = 0;
            if (date > r[5] + 0x400) r[-1] = -1;
        }
        r[3] = r[3] - (r[3] >> 4);
        if (r[1] != 0 || r[4] != 0) r[4] += -0x40;
    }
}

}  // namespace

SG_HOOK("golf_clean.exe", 0x004674C0, bilinearSample, BilinearSample_re, BilinearSample_orig);
SG_HOOK("golf_clean.exe", 0x0040DF80, objectAt, ObjectAt_re, ObjectAt_orig);
SG_HOOK("golf_clean.exe", 0x0047EEE0, matchInputCode, MatchInputCode_re, MatchInputCode_orig);
SG_HOOK("golf_clean.exe", 0x00401000, resetRecordBank, ResetRecordBank_re, ResetRecordBank_orig);
SG_HOOK("golf_clean.exe", 0x00409950, updatePairSnapshot, UpdatePairSnapshot_re, UpdatePairSnapshot_orig);
SG_HOOK("golf_clean.exe", 0x004099F0, addGolferPair, AddGolferPair_re, AddGolferPair_orig);
SG_HOOK("golf_clean.exe", 0x00409BF0, updateRollingStats, UpdateRollingStats_re, UpdateRollingStats_orig);

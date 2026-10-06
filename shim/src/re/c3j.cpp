// C3 batch c3j of golf_clean.exe: a golfer stat-trend average, two tile/object scans that write scratch globals,
// two Snd field writers and a Snd base constructor. Hand-written from the C2 transcriptions in
// re/analysis/<subsystem>/ and the disassembly (`re/tools/asm2inline.py golf_clean.exe 0x<addr> --list`), not a
// copy of the matched source in re/match/*.cpp. Callees are reached through their original addresses (a hooked
// callee runs its own reimplementation), as shim/src/re/golf_course.cpp does.
//
// __thiscall is emulated with __fastcall (ecx = this, edx unused), as shim/src/re/c3c.cpp does.
#include "hooks.h"

namespace {

template <typename T> T& at(unsigned addr) { return *reinterpret_cast<T*>(addr); }

// Callees reached through their original addresses.
typedef int(__cdecl* D2_t)(int, int);            // distance 0x0040acd0
typedef int(__cdecl* D4_t)(int, int, int, int);  // tileDistance 0x0040c4b0
typedef int(__cdecl* Blk_t)(int, int);           // tileBlocked 0x0040bf60

const D2_t kDistance = reinterpret_cast<D2_t>(0x0040ACD0);
const D4_t kTileDistance = reinterpret_cast<D4_t>(0x0040C4B0);
const Blk_t kTileBlocked = reinterpret_cast<Blk_t>(0x0040BF60);

// ---- golfer stat-trend average (pure leaf) -------------------------------------------------------

// 0x004060a0  computeRatingTrend(idx, sel): over per-index records of stride 0x208 (0x004060a3..0x004060b1,
// edx = idx*0x208), builds two 8-slot accumulators. local20[j] starts at 8 (0x004060cc..0x004060d9); local40[j]
// starts at (signed byte at 0x00575ab0 + idx*0x208) << 3 (0x004060ba, 0x004060c1). For outer = 1..9
// (0x004060e1, 0x00406119, 0x0040611d) and j = 0..7 (ecx 0..0x1c step 4, 0x004060f7, 0x00406110) it reads the
// signed word at 0x00575ada + idx*0x208 + (outer-1)*2 + j*0x16 (0x004060f0; esi += 0x16 at 0x0040610d,
// base += 2 per outer at 0x0040611a), adding it to local20[j] (0x004060fe) and (word * outer) to local40[j]
// (0x00406104, 0x00406107). Returns 0 when local20[7] == 0 (0x0040612c). Otherwise sel picks a numerator/divisor:
// sel 1 needs local20[6] (0x00406136, 0x0040613a); sel 2 needs local20[5] (0x00406149, 0x0040614d); sel 4 needs
// local20[3] (0x0040615c, 0x00406160); a failed pick returns 0 (0x00406193). The result is
// local40[pick]*100/local20[pick] - local40[7]*100/local20[7] with C signed division (0x00406169..0x0040618a).
typedef int(__cdecl* Trend_t)(int, int);
Trend_t ComputeRatingTrend_orig;
int __cdecl ComputeRatingTrend_re(int idx, int sel) {
    const int cv = at<signed char>(0x00575AB0 + (unsigned)idx * 0x208);
    int l40[8], l20[8];
    for (int i = 0; i < 8; i++) { l40[i] = cv << 3; l20[i] = 8; }
    const unsigned sbase = 0x00575ADA + (unsigned)idx * 0x208;
    for (int outer = 1; outer <= 9; outer++)
        for (int j = 0; j < 8; j++) {
            const int s = at<short>(sbase + (outer - 1) * 2 + j * 0x16);
            l20[j] += s;
            l40[j] += s * outer;
        }
    if (l20[7] == 0) return 0;
    int num, div;
    if (l20[6] != 0 && sel == 1) { num = l40[6]; div = l20[6]; }
    else if (l20[5] != 0 && sel == 2) { num = l40[5]; div = l20[5]; }
    else if (l20[3] != 0 && sel == 4) { num = l40[3]; div = l20[3]; }
    else return 0;
    return num * 100 / div - l40[7] * 100 / l20[7];
}

// ---- tile / object scans that write scratch globals ----------------------------------------------

// 0x00407000  holeQuadrants(a, b, mask): scans the 256 placed-object records at 0x0058bcb8 (stride 0x10,
// 0x0040700a..0x00407075). For each whose type word +0 is 4 (0x0040700f) and whose int +8 is < 0x10 signed
// (0x00407016), it calls tileDistance(a, b, objY, objX) where objY/objX are the signed words +2/+4
// (0x0040701c..0x0040702b; push order eax=objX, ecx=objY, ebp=b, edx=a). When that distance is below
// ((field*5 + 0x28) * 5) / 3 (signed /3, 0x00407038..0x0040704f) it sets bit 1 << (field & 3) in the mask
// (0x00407051..0x0040706f). The resulting mask is stored at 0x00541318 (0x00407081) and the function returns
// whether it covers the requested mask, (bits & mask) == mask (0x00407087..0x0040708e).
typedef int(__cdecl* Hole_t)(int, int, unsigned);
Hole_t HoleQuadrants_orig;
int __cdecl HoleQuadrants_re(int a, int b, unsigned mask) {
    unsigned bits = 0;
    for (unsigned rec = 0x0058BCB8; rec < 0x0058CCB8; rec += 0x10) {
        if (at<short>(rec) != 4) continue;
        const int field = at<int>(rec + 8);
        if (field >= 0x10) continue;
        const int objY = at<short>(rec + 2);
        const int objX = at<short>(rec + 4);
        const int d = kTileDistance(a, b, objY, objX);
        const int threshold = ((field * 5 + 0x28) * 5) / 3;
        if (d < threshold) bits |= 1u << (field & 3);
    }
    at<unsigned>(0x00541318) = bits;
    return (bits & mask) == mask;
}

// 0x0040de70  findNearestTargetTile(px, py, maxDist): over the 9x9 tile block centred on (px>>10, py>>10)
// (0x0040de80..0x0040dea5, +-4), initialises g_nearTileDist 0x00568d0c = maxDist << 10 (0x0040de91) and
// g_nearTileX 0x0056a91c = -1 (0x0040de97). A cell counts when tileBlocked(x, y) is 0 (0x0040dee0) and bit 0x200
// of the word at 0x0053caf0[x*50 + y] is set (0x0040def2, 0x0040defa); its distance is
// distance((x<<10) - px + 0x200, (y<<10) - py + 0x200) (0x0040df05..0x0040df22). The nearest such cell's distance,
// x and y are kept in 0x00568d0c / 0x0056a91c / g_nearTileY 0x0056a920 (0x0040df34..0x0040df3f).
typedef void(__cdecl* FindNear_t)(int, int, int);
FindNear_t FindNearestTargetTile_orig;
void __cdecl FindNearestTargetTile_re(int px, int py, int maxDist) {
    const int xLo = (px >> 10) - 4, xHi = (px >> 10) + 4;
    const int yLo = (py >> 10) - 4, yHi = (py >> 10) + 4;
    at<int>(0x00568D0C) = maxDist << 10;
    at<int>(0x0056A91C) = -1;
    const unsigned short* flags = reinterpret_cast<const unsigned short*>(0x0053CAF0);
    for (int x = xLo; x <= xHi; x++)
        for (int y = yLo; y <= yHi; y++) {
            if (kTileBlocked(x, y)) continue;
            if (!(flags[x * 50 + y] & 0x200)) continue;
            const int d = kDistance((x << 10) - px + 0x200, (y << 10) - py + 0x200);
            if (d < at<int>(0x00568D0C)) {
                at<int>(0x00568D0C) = d;
                at<int>(0x0056A91C) = x;
                at<int>(0x0056A920) = y;
            }
        }
}

// ---- Snd field writers (thiscall) ----------------------------------------------------------------

// 0x00485140  Snd::setVolume(v): stores v & 0x7f at this+4 (0x00485147, 0x00485156). When this+0x3c < 0x10
// unsigned (0x0048515c) it scales v through a table at 0x0083ada8 and __ftol, but that value is consumed only by
// the device object at this+0x40 (indirect call [vtbl+0x40] at 0x0048519d). The A/B holds this+0x40 == 0 and
// this+0x3c >= 0x10, so neither the float block nor the device call runs and the only observable effect is the
// store at this+4.
typedef void(__fastcall* SetVol_t)(void*, void*, unsigned);
SetVol_t SetVolume_orig;
void __fastcall SetVolume_re(void* self, void*, unsigned v) {
    const unsigned t = reinterpret_cast<unsigned>(self);
    at<unsigned>(t + 4) = v & 0x7f;
}

// 0x004846b0  Snd::setField34(v): stores v at this+0x34 (0x004846b4); if the device object at this+0x40 is
// non-null it also forwards v to it (indirect call [vtbl+0x4c] at 0x004846c1). The A/B holds this+0x40 == 0, so
// only the store at this+0x34 runs.
typedef void(__fastcall* SetF34_t)(void*, void*, unsigned);
SetF34_t SetField34_orig;
void __fastcall SetField34_re(void* self, void*, unsigned v) {
    const unsigned t = reinterpret_cast<unsigned>(self);
    at<unsigned>(t + 0x34) = v;
}

// ---- Snd base constructor (thiscall, returns this) -----------------------------------------------

// 0x00485260  Snd::ctor485260(): installs the vtable 0x004badec at this+0 (0x00485266), stores 0x7f at this+4
// (0x0048526d) and 0 at this+8 (0x00485274), zeroes the nine dwords this+0xc..this+0x2c (0x0048527b..0x00485283,
// rep stosd of 9) and stores 0 at this+0x30 (0x0048528e); returns this (0x00485295). No callees.
typedef void*(__fastcall* Ctor_t)(void*, void*);
Ctor_t Ctor485260_orig;
void* __fastcall Ctor485260_re(void* self, void*) {
    const unsigned t = reinterpret_cast<unsigned>(self);
    at<unsigned>(t) = 0x004BADEC;
    at<int>(t + 4) = 0x7f;
    at<int>(t + 8) = 0;
    for (int i = 0; i < 9; i++) at<int>(t + 0xc + i * 4) = 0;
    at<int>(t + 0x30) = 0;
    return self;
}

}  // namespace

SG_HOOK("golf_clean.exe", 0x004060a0, computeRatingTrend, ComputeRatingTrend_re, ComputeRatingTrend_orig);
SG_HOOK("golf_clean.exe", 0x00407000, holeQuadrants, HoleQuadrants_re, HoleQuadrants_orig);
SG_HOOK("golf_clean.exe", 0x0040de70, findNearestTargetTile, FindNearestTargetTile_re, FindNearestTargetTile_orig);
SG_HOOK("golf_clean.exe", 0x00485140, Snd_setVolume, SetVolume_re, SetVolume_orig);
SG_HOOK("golf_clean.exe", 0x004846b0, Snd_setField34, SetField34_re, SetField34_orig);
SG_HOOK("golf_clean.exe", 0x00485260, Snd_ctor485260, Ctor485260_re, Ctor485260_orig);

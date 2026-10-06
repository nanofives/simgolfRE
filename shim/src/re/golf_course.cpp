// Course/golfer table readers of golf_clean.exe: tile rating, terrain height blend, placement-area evaluation.
// They call the originals of their helpers through their addresses (a hooked helper runs its reimplementation).
// From re/analysis/<subsystem>/<addr>_<name>.md; evalPlacementArea has no matched source and follows the
// instructions at 0x0040db90 directly.
#include "hooks.h"

namespace {

template <typename T> T at(unsigned addr) { return *reinterpret_cast<const T*>(addr); }

typedef int(__cdecl* Xy_t)(int, int);
typedef int(__cdecl* Clamp_t)(int, int, int);
typedef int(__cdecl* Int1_t)(int);
typedef int(__cdecl* Eval_t)(int, int, int, int);

const Clamp_t kClamp = reinterpret_cast<Clamp_t>(0x00467130);
const Xy_t kTileBlocked = reinterpret_cast<Xy_t>(0x0040bf60);
const Xy_t kTypeAtPos = reinterpret_cast<Xy_t>(0x0040bfa0);
const Xy_t kSampleHeight = reinterpret_cast<Xy_t>(0x0042dba0);
const Xy_t kObjectAt = reinterpret_cast<Xy_t>(0x0040df80);

signed char tile(int cell) { return at<signed char>(0x005722e8 + cell); }
unsigned courseRec() { return 0x00571ff4 + at<int>(0x0059bf90) * 0x2e; }

// 0x00422530  rateTile(i): rating of golfer record i (0x100 bytes at 0x005794c0 + i*0x100), capped at 330.
Int1_t RateTile_orig;
int __cdecl RateTile_re(int i) {
    const unsigned g = 0x005794c0 + i * 0x100;
    const bool flag1 = at<unsigned char>(g + 0x11) & 1;
    int v = (flag1 ? 50 : 0) + 150;
    if (at<int>(0x00822c88) >= 1)
        v += at<signed char>(g + 0xb2) * 50 / 3;
    else
        v += flag1 ? 40 : 25;
    int k = kTypeAtPos(at<int>(g + 0xcc), at<int>(g + 0xd0));
    if (i >= 0x98) k = at<unsigned char>(g + 0x1a) ? 2 : 0;
    if (at<unsigned char>(g + 0x10)) {
        const unsigned short ce = at<unsigned short>(g + 0x0e);
        if (ce & 1) v += at<unsigned char>(g + 0xe8) * 4 - 20;
        if ((ce & 2) && !k) v += (at<unsigned char>(g + 0xe9) - 5) * 6;
    }
    const signed char ee = at<signed char>(g + 0x2e);
    if (ee > 0) v += kClamp(ee, 0, 3) * v / 24;
    if (flag1) v += at<int>(0x00543cc8) * 15;
    const signed char m2 = at<signed char>(0x00578372 + k * 0x30);
    if (m2 > 0) v -= kClamp(m2, 0, 3) * v / 8;
    if (k) v += v / -5;
    return v > 330 ? 330 : v;
}

// 0x0040c170  heightBlend(x, y): 3 unless bit 0 of 0x0059e7b8 is clear and 0 <= x, y < 50. With the dword at
// 0x00834170 non-zero: 3 when the tile is 0x14 and tileBlocked(x-1, y) and tileBlocked(x, y+1), else the byte at
// 0x005a4998 + x*51 + y. Otherwise 3 for tiles 0x11/0x12/0x13 or a 0x11 tile at x+1; else
// h = sampleHeight(x << s, y << s) (s = 7 when the current course record's byte +2 is 2, else 6), plus
// (16 - y) * d / -6 when its byte +4 is 1 and y < 16, d = the dword at 0x004c2fa0; returns
// clamp(h / d + 1, 3 + (dword 0x00822c88 != 0), 15).
Xy_t HeightBlend_orig;
int __cdecl HeightBlend_re(int x, int y) {
    if ((at<unsigned char>(0x0059e7b8) & 1) || x < 0 || x >= 50 || y < 0 || y >= 50) return 3;
    if (at<int>(0x00834170)) {
        if (tile(x * 50 + y) == 0x14 && kTileBlocked(x - 1, y) && kTileBlocked(x, y + 1)) return 3;
        return at<unsigned char>(0x005a4998 + x * 51 + y);
    }
    const int s = y + x * 50;
    const signed char t = tile(s);
    if (t == 0x11 || tile(s + 50) == 0x11 || t == 0x12 || t == 0x13) return 3;
    const unsigned c = courseRec();
    int h = at<signed char>(c + 2) == 2 ? kSampleHeight(x << 7, y << 7) : kSampleHeight(x << 6, y << 6);
    const int d = at<int>(0x004c2fa0);
    if (at<signed char>(c + 4) == 1 && y < 16) h += (16 - y) * d / -6;
    return kClamp(h / d + 1, (at<int>(0x00822c88) != 0) + 3, 15);
}

// 0x0040db90  evalPlacementArea(x, y, n, kind): walks the (n + 2) x (n + 2) cells from (x - 1, y - 1).
// Border cells (first/last row or column, index -1 or n) only note whether any is not type 0x11.
// Inner cells: kind 0xc on course type (byte 0x005a34e0) 0 or 2 rejects a non-0x11 tile; kind 0xc or any kind other
// than 0 then rejects type 0x15, a type-0x16 or flag-0x400 cell whose placed object (objectAt 0x0040df80, 16-byte
// records at 0x0058bcb8) has a type other than kind, type 0, and flags (0x0053caf0 words) & 0x8080. Every inner cell
// is rejected when tileBlocked; otherwise it adds the type's byte +0 (0x00578374 + type*0x30) when the type's byte
// +2 is 13, and the same byte of type 0xc / 0x11 (kind != 0xc) / 0x12 / 0x13 for those tiles; flag bit 0x8000
// rejects. Returns -1 on rejection; else the sum when a border cell was not 0x11, or when kind is 0 or 4; else -1.
Eval_t EvalPlacementArea_orig;
int __cdecl EvalPlacementArea_re(int x, int y, int n, int kind) {
    const unsigned short* flags = reinterpret_cast<const unsigned short*>(0x0053caf0);
    const auto typeByte = [](int t, int off) { return at<signed char>(0x00578374 + off + t * 0x30); };
    int sum = 0;
    bool openBorder = false;
    if (n > -2) {
        for (int i = -1; i <= n; i++) {
            const int row = (x + i) * 50;
            for (int j = -1; j <= n; j++) {
                const int yy = y + j, cell = row + yy;
                if (i == -1 || j == -1 || i == n || j == n) {
                    if (tile(cell) != 0x11) openBorder = true;
                    continue;
                }
                bool checks = kind != 0;
                if (kind == 0xc) {
                    const char ct = at<char>(0x005a34e0);
                    if ((ct == 0 || ct == 2) && tile(cell) != 0x11) return -1;
                }
                if (checks) {
                    const signed char t = tile(cell);
                    if (t == 0x15) return -1;
                    if ((t == 0x16 || (flags[cell] & 0x400)) &&
                        at<short>(0x0058bcb8 + kObjectAt(x + i, yy) * 0x10) != kind)
                        return -1;
                    if (tile(cell) == 0) return -1;
                    if (flags[cell] & 0x8080) return -1;
                }
                if (kTileBlocked(x + i, yy)) return -1;
                const signed char t = tile(cell);
                if (typeByte(t, 2) == 0xd) sum += typeByte(t, 0);
                if (t == 0xc) sum += typeByte(0xc, 0);
                if (t == 0x11 && kind != 0xc) sum += typeByte(0x11, 0);
                if (t == 0x12) sum += typeByte(0x12, 0);
                if (t == 0x13) sum += typeByte(0x13, 0);
                if (flags[cell] & 0x8000) return -1;
            }
        }
        if (openBorder) return sum;
    }
    if (kind != 0 && kind != 4) return -1;
    return sum;
}

}  // namespace

SG_HOOK("golf_clean.exe", 0x00422530, rateTile, RateTile_re, RateTile_orig);
SG_HOOK("golf_clean.exe", 0x0040c170, heightBlend, HeightBlend_re, HeightBlend_orig);
SG_HOOK("golf_clean.exe", 0x0040db90, evalPlacementArea, EvalPlacementArea_re, EvalPlacementArea_orig);

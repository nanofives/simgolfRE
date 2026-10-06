// c3k batch of golf_clean.exe: a club-name text builder, a window inner-size adjuster, a draw-target setter, a
// record-bank placer, a tile corner-height reader and a connected-path flood. Each body is hand-written from the
// C2 transcription (re/analysis/<subsystem>/<addr>_<name>.md) and the decompilation/disassembly; callees are
// reached through their original addresses, so a hooked callee runs its own reimplementation. __thiscall is
// emulated with __fastcall (ecx = this, edx unused).
#include "hooks.h"
#include <string.h>

namespace {

template <typename T> T at(unsigned addr) { return *reinterpret_cast<const T*>(addr); }
template <typename T> T& ref(unsigned addr) { return *reinterpret_cast<T*>(addr); }

typedef void(__cdecl* AppendClub_t)(int);
typedef void(__fastcall* InnerSize_t)(void* self, void* edx, int* w, int* h);
typedef unsigned(__fastcall* SetDrawTarget_t)(void* self, void* edx, unsigned surf, unsigned a, unsigned b, unsigned c);
typedef int(__cdecl* PlaceRecord_t)(int, int, int, int);
typedef int(__cdecl* CornerHeights_t)(int, int, int, int);
typedef void(__cdecl* Propagate_t)(int, int);
typedef int(__cdecl* Xy_t)(int, int);
typedef int(__cdecl* HeightBlend_t)(int, int);
typedef void(__cdecl* CornerRange_t)(int, int, int*, int*);

// 0x0040a9a0  appendClubName(club): strcat's the club's name onto the text buffer 0x0051a068. club selects one of
// 14 string pointers through the jump table at 0x0040aa44 (club 0 -> 0x004c5220 "Driver", ascending by 8/12 bytes
// to club 13 -> 0x004c51b0 "Putter"); an index above 13 (unsigned compare at 0x0040a9a5) appends nothing. The
// strings are read from the binary at their addresses, not embedded here.
const unsigned kClubStr[14] = {
    0x004c5220, 0x004c5218, 0x004c5210, 0x004c5208, 0x004c5200, 0x004c51f8, 0x004c51f0,
    0x004c51e8, 0x004c51e0, 0x004c51d8, 0x004c51d0, 0x004c51c4, 0x004c51b8, 0x004c51b0,
};
AppendClub_t AppendClubName_orig;
void __cdecl AppendClubName_re(int club) {
    if (static_cast<unsigned>(club) > 0xd) return;
    strcat(reinterpret_cast<char*>(0x0051a068), reinterpret_cast<const char*>(kClubStr[club]));
}

// 0x0047cb10  Window::innerSize(w, h): adjusts the caller's *w/*h for the window frame. Does nothing when either
// pointer is null (0x0047cb13, 0x0047cb1d). f = the flags dword at this+0x9c: bit 0x04 -> *h -= g (g = the border
// dword 0x0083ff10); bit 0x08 -> *w -= g; bit 0x400 OR bit 0x11 -> *w += -m184*2, *h += -m184*2, and if m188 != -1
// then *h += m184 - m188 (m184 = this+0x184, m188 = this+0x188); bit 0x10 -> *h += m184 - m180 (m180 = this+0x180);
// finally when this+0x15c != 0 and bit 0x20000000 is clear it subtracts a child extent fetched through that child's
// vtable slot +0x170 (0x0047cbf4). The A/B fixture sets this+0x15c = 0, so that last branch does not run.
InnerSize_t InnerSize_orig;
void __fastcall InnerSize_re(void* self, void*, int* w, int* h) {
    if (!w || !h) return;
    const unsigned base = reinterpret_cast<unsigned>(self);
    const unsigned f = at<unsigned>(base + 0x9c);
    const int g = at<int>(0x0083ff10);
    const int m184 = at<int>(base + 0x184);
    const int m188 = at<int>(base + 0x188);
    const int m180 = at<int>(base + 0x180);
    if (f & 4) *h -= g;
    if (f & 8) *w -= g;
    if ((f & 0x400) || (f & 0x11)) {
        *w += -m184 * 2;
        *h += -m184 * 2;
        if (m188 != -1) *h += m184 - m188;
    }
    if (f & 0x10) *h += m184 - m180;
    const unsigned child = at<unsigned>(base + 0x15c);
    if (child != 0 && !(f & 0x20000000)) {
        typedef int(__fastcall* Extent_t)(void*, void*);
        const int ext = reinterpret_cast<Extent_t>(at<unsigned>(at<unsigned>(child) + 0x170))(
            reinterpret_cast<void*>(child), 0);
        *h -= ext;
    }
}

// 0x004762d0  setDrawTarget(surf, a, b, c): returns 3 without writing when surf == 0 (0x004762d3). Otherwise stores
// surf at this+0x5c only when the dword surf+4 is non-zero (0x004762dd..0x004762e5), then a at this+0x60, c at
// this+0x68 and b at this+0x64 (0x004762ee..0x00476300), and returns 0.
SetDrawTarget_t SetDrawTarget_orig;
unsigned __fastcall SetDrawTarget_re(void* self, void*, unsigned surf, unsigned a, unsigned b, unsigned c) {
    if (surf == 0) return 3;
    const unsigned base = reinterpret_cast<unsigned>(self);
    if (at<int>(surf + 4) != 0) ref<unsigned>(base + 0x5c) = surf;
    ref<unsigned>(base + 0x60) = a;
    ref<unsigned>(base + 0x68) = c;
    ref<unsigned>(base + 0x64) = b;
    return 0;
}

// 0x00401040  placeRecord(bank, type, row, col): the first loop (0x00401077..0x004010e7) only computes a flag that
// 0x004010e9 discards (it jumps past every read of it), so placement is unconditional. Scans the bank's 16 id words
// (0x004e6d20 + bank*0x74, 0x00401100) for the first free one (== -1); returns 0 when none is free (0x00401112).
// Otherwise it writes the id (type) word there (0x00401133), the row and col bytes and 0xff into
// 0x004e6d40/0x004e6d50/0x004e6d60 at the slot (0x00401143..0x00401151), then for each set bit of the six
// footprint-definition bytes (0x004c11e0 + type*0x27) stores the slot index into the occupancy grid
// 0x004e6d70[col + (row + r)*6 + bank*0x74 + c] (0x0040117b), and returns 1.
PlaceRecord_t PlaceRecord_orig;
int __cdecl PlaceRecord_re(int bank, int type, int row, int col) {
    const int base = bank * 0x74;
    short* ids = reinterpret_cast<short*>(0x004e6d20 + base);
    for (int slot = 0; slot < 0x10; slot++) {
        if (ids[slot] != -1) continue;
        ids[slot] = static_cast<short>(type);
        reinterpret_cast<signed char*>(0x004e6d40 + base)[slot] = static_cast<signed char>(row);
        reinterpret_cast<signed char*>(0x004e6d50 + base)[slot] = static_cast<signed char>(col);
        reinterpret_cast<unsigned char*>(0x004e6d60 + base)[slot] = 0xff;
        const unsigned char* defs = reinterpret_cast<const unsigned char*>(0x004c11e0 + type * 0x27);
        signed char* occ = reinterpret_cast<signed char*>(0x004e6d70 + base);
        for (int r = 0; r < 6; r++) {
            const unsigned char bits = defs[r];
            for (int c = 0; c < 6; c++)
                if (bits & (1 << c))
                    occ[col + (row + r) * 6 + c] = static_cast<signed char>(slot);
        }
        return 1;
    }
    return 0;
}

// 0x0040bfe0  cornerHeights(x, y, corner, flag): returns 3 unless 0 <= x,y < 50 and corner is odd (0x0040c013).
// With flag != 0 a non-zero cached byte at 0x0051b770 + (y + x*50)*8 + corner is returned (0x0040c02a). Otherwise,
// with f = the tile type's flags dword (0x0057837c + type*0x30, type from 0x005722e8): f & 2 -> the byte at
// 0x00543018 + cell when f & 1, else cornerRange's minimum (0x0042f4b0); f & 4 -> that byte when f & 1, else
// cornerRange's maximum; f & 8 -> 3. Otherwise heightBlend (0x0040c170) of the four corners (x+1,y-1), (x+1,y),
// (x,y), (x,y-1), selected by corner & 7 (1, 3, 5 pick the first three; 7 picks the fourth). cornerRange fills a
// maximum and a minimum from heightBlend of the same four corners.
CornerHeights_t CornerHeights_orig;
int __cdecl CornerHeights_re(int x, int y, int corner, int flag) {
    if (!(static_cast<unsigned>(x) < 0x32 && static_cast<unsigned>(y) < 0x32 && (corner & 1))) return 3;
    const int cell = y + x * 0x32;
    if (flag != 0) {
        const signed char cached = at<signed char>(0x0051b770 + cell * 8 + corner);
        if (cached != 0) return cached;
    }
    const int type = at<signed char>(0x005722e8 + cell);
    const unsigned f = at<unsigned>(0x0057837c + type * 0x30);
    const CornerRange_t cornerRange = reinterpret_cast<CornerRange_t>(0x0042f4b0);
    if (f & 2) {
        if (f & 1) return at<signed char>(0x00543018 + cell);
        int mx, mn; cornerRange(x, y, &mx, &mn); return mn;
    }
    if (f & 4) {
        if (f & 1) return at<signed char>(0x00543018 + cell);
        int mx, mn; cornerRange(x, y, &mx, &mn); return mx;
    }
    if (f & 8) return 3;
    const HeightBlend_t hb = reinterpret_cast<HeightBlend_t>(0x0040c170);
    const int b1 = hb(x + 1, y - 1);
    const int b3 = hb(x + 1, y);
    const int b5 = hb(x, y);
    const int b7 = hb(x, y - 1);
    switch (corner & 7) {
        case 1: return b1;
        case 3: return b3;
        case 5: return b5;
        default: return b7;  // corner is odd, so the only remaining case is 7
    }
}

// 0x0042f1c0  propagateType11(x, y): recomputes the connected-path byte at 0x0056988c + x*50 + y from its eight
// neighbours and, when it changes, recurses into the type-0x11 neighbours. The dx/dy offset tables are at
// 0x004c2878 and 0x004c2898 (eight dword entries each). For each neighbour that tileBlocked (0x0040bf60) reports
// open: if the neighbour tile's +2 flags byte (0x00578376 + type*0x30, type from 0x005722e8) is not 0x11 the value
// becomes 0 (0x0042f23a); otherwise if the neighbour's own byte is 0 and the value is still non-zero it becomes 1
// (0x0042f257). If the resulting value differs from the stored one (0x0042f281) it is written back and the function
// recurses (through its own address 0x0042f1c0) into each open neighbour whose +2 byte is 0x11 (0x0042f29e).
Propagate_t PropagateType11_orig;
void __cdecl PropagateType11_re(int x, int y) {
    signed char* self = reinterpret_cast<signed char*>(0x0056988c + x * 0x32 + y);
    const int* dx = reinterpret_cast<const int*>(0x004c2878);
    const int* dy = reinterpret_cast<const int*>(0x004c2898);
    const Xy_t tileBlocked = reinterpret_cast<Xy_t>(0x0040bf60);
    signed char v = *self;
    for (int i = 0; i < 8; i++) {
        const int nx = dx[i] + x, ny = dy[i] + y;
        if (tileBlocked(nx, ny) != 0) continue;
        const int ncell = ny + nx * 0x32;
        const int ntype = at<signed char>(0x005722e8 + ncell);
        if (at<signed char>(0x00578376 + ntype * 0x30) != 0x11) v = 0;
        if (at<signed char>(0x0056988c + ncell) == 0 && v != 0) v = 1;
    }
    if (v == *self) return;
    *self = v;
    for (int i = 0; i < 8; i++) {
        const int nx = dx[i] + x, ny = dy[i] + y;
        if (tileBlocked(nx, ny) != 0) continue;
        const int ntype = at<signed char>(0x005722e8 + ny + nx * 0x32);
        if (at<signed char>(0x00578376 + ntype * 0x30) == 0x11)
            reinterpret_cast<Propagate_t>(0x0042f1c0)(nx, ny);
    }
}

}  // namespace

SG_HOOK("golf_clean.exe", 0x0040a9a0, appendClubName, AppendClubName_re, AppendClubName_orig);
SG_HOOK("golf_clean.exe", 0x0047cb10, Window__innerSize, InnerSize_re, InnerSize_orig);
SG_HOOK("golf_clean.exe", 0x004762d0, setDrawTarget, SetDrawTarget_re, SetDrawTarget_orig);
SG_HOOK("golf_clean.exe", 0x00401040, placeRecord, PlaceRecord_re, PlaceRecord_orig);
SG_HOOK("golf_clean.exe", 0x0040bfe0, cornerHeights, CornerHeights_re, CornerHeights_orig);
SG_HOOK("golf_clean.exe", 0x0042f1c0, propagateType11, PropagateType11_re, PropagateType11_orig);

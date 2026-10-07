// C3 batch c3n (2026-10-07) of golf_clean.exe: small util constructors / virtual-dispatch helpers and the terrain
// heightfield rebuild. Hand-written from the disassembly (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr>
// --list); each body cites the address of every global, offset and callee it uses. __thiscall is emulated with
// __fastcall (ecx = this, edx unused). Callees are invoked through their original addresses, so a hooked callee
// still runs its own reimplementation and the path-1 A/B drives both arms. Virtual slots are called the same way: a
// thiscall slot with N stack arguments is a __fastcall pointer with a dummy edx and the same N stack arguments.
#include "hooks.h"

namespace {

template <typename T> T& ref(unsigned addr) { return *reinterpret_cast<T*>(addr); }
inline unsigned u(void* p) { return reinterpret_cast<unsigned>(p); }

// 0x00474780  Buf_init(this): zeroes the byte at this+4 (0x00474782) and the dwords at this+0xc, +8, +0x14, +0x10
// (0x00474785..0x0047478e); the vtable slot at this+0 and the bytes +5..+7 are left alone. Returns with eax = 0
// (`xor eax, eax` at 0x00474780 is the zero it stores), so the registry declares it void.
typedef void(__fastcall* BufInit_t)(void*, void*);
BufInit_t BufInit_orig;
void __fastcall BufInit_re(void* ecx, void*) {
    const unsigned p = u(ecx);
    ref<unsigned char>(p + 4) = 0;
    ref<int>(p + 0xc) = 0;
    ref<int>(p + 8) = 0;
    ref<int>(p + 0x14) = 0;
    ref<int>(p + 0x10) = 0;
}

// 0x004747a0  Buf_ctor(this): stores the vtable 0x004ba84c at this+0 (0x004747a3), runs Buf_init (0x00474780) on
// this (0x004747a9) and returns this (0x004747ae).
typedef void*(__fastcall* Ctor_t)(void*, void*);
const BufInit_t kBufInit = reinterpret_cast<BufInit_t>(0x00474780);
Ctor_t BufCtor_orig;
void* __fastcall BufCtor_re(void* ecx, void*) {
    ref<unsigned>(u(ecx)) = 0x004ba84c;
    kBufInit(ecx, 0);
    return ecx;
}

// 0x00487b40  NodeListC::ctor: the same body as NodeListB::ctor, described below.
// 0x00487a20  NodeListB::ctor(this) / 0x00487b40 NodeListC::ctor(this): run the base constructor StrList::ctor
// (0x00487210: vtable 0x004baeac, +4 = 0, +8 = 0x7f, +0xc/+0x10/+0x14/+0x18/+0x1c = 0) on this (0x00487a23 /
// 0x00487b43), then overwrite the vtable at this+0 with the derived one, 0x004bafb0 (0x00487a28) / 0x004bb014
// (0x00487b48), and return this.
const Ctor_t kStrListCtor = reinterpret_cast<Ctor_t>(0x00487210);
Ctor_t NodeListBCtor_orig;
void* __fastcall NodeListBCtor_re(void* ecx, void*) {
    kStrListCtor(ecx, 0);
    ref<unsigned>(u(ecx)) = 0x004bafb0;
    return ecx;
}
Ctor_t NodeListCCtor_orig;
void* __fastcall NodeListCCtor_re(void* ecx, void*) {
    kStrListCtor(ecx, 0);
    ref<unsigned>(u(ecx)) = 0x004bb014;
    return ecx;
}

// Virtual slot of the object whose vtable pointer sits at obj+0.
template <typename F> F vslot(void* obj, unsigned off) { return reinterpret_cast<F>(ref<unsigned>(ref<unsigned>(u(obj)) + off)); }

// 0x00487630  resourceQuery(this): the inner object is the pointer at this+0x14 (0x00487630). NULL returns 0
// (`test` / `je` at 0x00487633..0x00487635, 0x0048763c); otherwise it tail-jumps to the inner object's vtable slot
// +0x74 with ecx = the inner object and no stack arguments (0x00487637..0x00487639), so its result is returned.
typedef int(__fastcall* Query_t)(void*, void*);
Query_t ResourceQuery_orig;
int __fastcall ResourceQuery_re(void* ecx, void*) {
    void* inner = ref<void*>(u(ecx) + 0x14);
    if (!inner) return 0;
    return vslot<Query_t>(inner, 0x74)(inner, 0);
}

// 0x00487a70  resourceLoadB: the same steps as resourceLoad, described below.
// 0x00487c00  resourceLoad(this, a, b) / 0x00487a70 resourceLoadB(this, a, b), both `ret 8`. First this's own slot
// +4 is called with b (0x00487c0b / 0x00487a7b); a non-zero result is returned as is (`jne` at 0x00487c10 /
// 0x00487a80). Otherwise the inner object at this+0x14 has its slot +0x10 called with (a, b) (0x00487c1d /
// 0x00487a8d); 0 returns 0 (`je` at 0x00487c24 / 0x00487a94). A non-zero result r runs this's slot +8 with no
// arguments (0x00487c2a / 0x00487a9a) and returns r. resourceLoadB also clears the inner pointer at this+0x14 after
// that slot +8 call (0x00487a9f); resourceLoad leaves it.
typedef int(__fastcall* Slot1_t)(void*, void*, int);
typedef int(__fastcall* Slot2_t)(void*, void*, int, int);
typedef int(__fastcall* Slot0_t)(void*, void*);
typedef int(__fastcall* Load_t)(void*, void*, int, int);
int LoadCommon(void* self, int a, int b, bool clearInner) {
    const int r = vslot<Slot1_t>(self, 4)(self, 0, b);
    if (r) return r;
    void* inner = ref<void*>(u(self) + 0x14);
    const int r2 = vslot<Slot2_t>(inner, 0x10)(inner, 0, a, b);
    if (!r2) return 0;
    vslot<Slot0_t>(self, 8)(self, 0);
    if (clearInner) ref<void*>(u(self) + 0x14) = 0;
    return r2;
}
Load_t ResourceLoad_orig;
int __fastcall ResourceLoad_re(void* ecx, void*, int a, int b) { return LoadCommon(ecx, a, b, false); }
Load_t ResourceLoadB_orig;
int __fastcall ResourceLoadB_re(void* ecx, void*, int a, int b) { return LoadCommon(ecx, a, b, true); }

// 0x0042f7a0  rebuildHeightfield(): rebuilds the per-cell corner-height cache and level grid of the 50x50 course.
// Globals (all [x][y], cell = x*50 + y): tile type bytes 0x005722e8; the type record (0x30 bytes) at 0x00578370 +
// type*0x30 whose flags dword is at +0xc (0x0057837c) and byte +7 (0x00578377); the signed level bytes 0x00543018;
// the corner cache 0x0051b770 (8 bytes per cell, corners 1/3/5/7 stored at +1/+3/+5/+7).
typedef int(__cdecl* CornerHeights_t)(int, int, int, int);
typedef void(__cdecl* CornerRange_t)(int, int, int*, int*);
typedef int(__cdecl* Cell2_t)(int, int);
typedef int(__cdecl* Raise_t)(int, int, int);
typedef void(__cdecl* Edges_t)(int, int);
const CornerHeights_t kCornerHeights = reinterpret_cast<CornerHeights_t>(0x0040bfe0);
const CornerRange_t kCornerRange = reinterpret_cast<CornerRange_t>(0x0042f4b0);
const Cell2_t kRelaxTile = reinterpret_cast<Cell2_t>(0x0042f630);
const Raise_t kRaiseFromNeighbours = reinterpret_cast<Raise_t>(0x0042f6e0);
const Edges_t kRelaxEdges = reinterpret_cast<Edges_t>(0x0042f530);

inline unsigned typeFlags(int cell) {
    const int t = static_cast<signed char>(ref<unsigned char>(0x005722e8 + cell));
    return ref<unsigned>(0x0057837c + t * 0x30);
}

typedef void(__cdecl* Void_t)();
Void_t RebuildHeightfield_orig;
void __cdecl RebuildHeightfield_re() {
    unsigned char* const level = reinterpret_cast<unsigned char*>(0x00543018);
    unsigned char* const cache = reinterpret_cast<unsigned char*>(0x0051b770);
    ref<int>(0x004c2e04) = -1;                          // 0x0042f7aa
    for (int i = 0; i < 2500; i++) level[i] = 0;        // 0x271 dwords at 0x00543018 (0x0042f7bf)
    for (int i = 0; i < 20000; i++) cache[i] = 0;       // 0x1388 dwords at 0x0051b770 (0x0042f7cb)

    // Pass 1 (0x0042f7d4..0x0042f884): cache the four odd corners from cornerHeights(x, y, corner, 0) and, for a
    // type with flag 2 or 4 (`test 6` at 0x0042f81e), seed the level from cornerRange (0x0042f833): flag 2 stores
    // its 4th output, then flag 4 stores its 3rd (0x0042f851..0x0042f866), so flag 4 wins when both are set.
    for (int x = 0; x < 50; x++) {
        for (int y = 0; y < 50; y++) {
            const int cell = x * 50 + y;
            unsigned char* c = cache + cell * 8;
            c[1] = static_cast<unsigned char>(kCornerHeights(x, y, 1, 0));
            c[3] = static_cast<unsigned char>(kCornerHeights(x, y, 3, 0));
            c[5] = static_cast<unsigned char>(kCornerHeights(x, y, 5, 0));
            c[7] = static_cast<unsigned char>(kCornerHeights(x, y, 7, 0));
            if (typeFlags(cell) & 6) {
                int third, fourth;
                kCornerRange(x, y, &third, &fourth);
                const unsigned f = typeFlags(cell);
                if (f & 2) level[cell] = static_cast<unsigned char>(fourth);
                if (f & 4) level[cell] = static_cast<unsigned char>(third);
            }
        }
    }

    // Pass 2 (0x0042f88a..0x0042f8fb): repeat until a full sweep changes nothing. For a type with flag 1, flag 2
    // runs relaxTile (0x0042f630) and flag 4 runs raiseFromNeighbours (0x0042f6e0) with sameByte = (type byte +7
    // == 0x10) (`sete` at 0x0042f8d7); their returns are summed and a non-zero sum loops again (0x0042f8fb).
    int changed;
    do {
        changed = 0;
        for (int x = 0; x < 50; x++) {
            for (int y = 0; y < 50; y++) {
                const int cell = x * 50 + y;
                const unsigned f = typeFlags(cell);
                if (!(f & 1)) continue;
                if (f & 2) changed += kRelaxTile(x, y);
                if (typeFlags(cell) & 4) {
                    const int t = static_cast<signed char>(ref<unsigned char>(0x005722e8 + cell));
                    changed += kRaiseFromNeighbours(x, y, ref<unsigned char>(0x00578377 + t * 0x30) == 0x10);
                }
            }
        }
    } while (changed);

    // Pass 3 (0x0042f8fd..0x0042f915): relaxEdges (0x0042f530) on every cell, x outer, y inner.
    for (int x = 0; x < 50; x++)
        for (int y = 0; y < 50; y++) kRelaxEdges(x, y);

    // 0x0042f917..0x0042f936: clear bit 1 and set bit 3 of the mask byte at 0x005619a0, clear bit 18 (0x40000) of
    // the flags dword 0x0059e7b8.
    ref<unsigned char>(0x005619a0) = static_cast<unsigned char>((ref<unsigned char>(0x005619a0) & 0xfd) | 8);
    ref<unsigned>(0x0059e7b8) &= 0xfffbffffu;
}

// 0x00421fa0  scanTileLine(x0, y0, tx, ty, range, curve, strict) -> score >= 0. (x0, y0) is a world point (0x400
// units per tile), (tx, ty) a target tile. Callees: angleFixed 0x004672d0, distance 0x0040acd0, clamp 0x00467130,
// foldRange 0x00467270, absDiff 0x004672b0, tileBlocked 0x0040bf60. Tables: tile types 0x005722e8 (cell = x*50 + y,
// signed bytes), type records 0x00578370 + type*0x30 (signed byte +2 at 0x00578372, byte +6 at 0x00578376),
// direction tables kDirX 0x004c2878 / kDirY 0x004c2898 (8 dwords each). The byte 0x005783a2 is 0 while it runs
// (0x00421fd7) and 0xff on return (0x004223b4).
typedef int(__cdecl* Int2_t)(int, int);
typedef int(__cdecl* Int3_t)(int, int, int);
const Int2_t kAngleFixed = reinterpret_cast<Int2_t>(0x004672d0);
const Int2_t kDistance = reinterpret_cast<Int2_t>(0x0040acd0);
const Int3_t kClamp = reinterpret_cast<Int3_t>(0x00467130);
const Int2_t kFoldRange = reinterpret_cast<Int2_t>(0x00467270);
const Int2_t kAbsDiff = reinterpret_cast<Int2_t>(0x004672b0);
const Int2_t kTileBlocked = reinterpret_cast<Int2_t>(0x0040bf60);

inline int tileType(int cell) { return static_cast<signed char>(ref<unsigned char>(0x005722e8 + cell)); }
inline int typeByte2(int t) { return static_cast<signed char>(ref<unsigned char>(0x00578372 + t * 0x30)); }
inline bool typeIs13(int t) { return ref<unsigned char>(0x00578376 + t * 0x30) == 0xd; }
// C remainder of a world coordinate within its tile (the `and 0x800003ff` / sign fix-up at 0x00422141 / 0x00422156),
// then >> 4: 0..63 inside the tile, negative for a negative coordinate.
inline int subTile(int v) { return (v % 0x400) >> 4; }

typedef int(__cdecl* Scan_t)(int, int, int, int, int, int, int);
Scan_t ScanTileLine_orig;
int __cdecl ScanTileLine_re(int x0, int y0, int tx, int ty, int range, int curve, int strict) {
    ref<unsigned char>(0x005783a2) = 0;
    const int txw = static_cast<int>(static_cast<unsigned>(tx) << 10);
    const int tyw = static_cast<int>(static_cast<unsigned>(ty) << 10);
    const int dx = static_cast<int>(static_cast<unsigned>(txw) - x0 + 0x200);   // 0x00421fba..0x00421fcf
    const int dy = static_cast<int>(static_cast<unsigned>(tyw) - y0 + 0x200);
    const int angle = kAngleFixed(dx, dy);                                         // 0x00421fde
    const int dir = (((angle >> 28) + 1) >> 1) & 7;                                // 0x00421fe6..0x00421fed
    const int n = (kDistance(dx, dy) + 0x200) >> 10;                               // 0x00421ff8..0x0042201a
    const int steps = kClamp(n, 0, range / 25 + 1);                                // 0x00422003..0x00422028
    if (n < 0) {                                                                   // 0x00422046
        ref<unsigned char>(0x005783a2) = 0xff;
        return 0;
    }
    const int* dirX = reinterpret_cast<const int*>(0x004c2878);
    const int* dirY = reinterpret_cast<const int*>(0x004c2898);
    const int half = steps / 2;
    // The curve accumulator starts at steps*curve*0x15555554 and drops by curve*0x15555554 per step (32-bit wrap,
    // 0x0042204c..0x00422074 and 0x00422366..0x00422377); L = steps*0x400 drops and R = 0x200 grows by 0x400.
    unsigned acc = static_cast<unsigned>(steps) * static_cast<unsigned>(curve) * 0x15555554u;
    const unsigned accStep = static_cast<unsigned>(curve) * 0xeaaaaaacu;
    int L = steps << 10, R = 0x200;
    int score = 0, count13 = 0;
    for (int i = 0; i <= n; i++) {                                                 // loop test 0x00422397
        // Angle offset (0x0042208e..0x004220c2): none when curve == 0; curve*0x0ffffffc up to half way;
        // acc / ((steps+1)/2) between half and steps; none from steps on.
        int off = 0;
        if (curve != 0) {
            if (i <= half) off = static_cast<int>(static_cast<unsigned>(curve) * 0x0ffffffcu);
            else if (i < steps) off = static_cast<int>(acc) / ((steps + 1) / 2);
        }
        const int a = angle + off;
        int px = x0 + kFoldRange(a, i << 10);                                      // 0x004220cd
        int py = y0 - kAbsDiff(a, i << 10);                                        // 0x004220dc
        if (curve != 0 && i > half && i < steps) {                                 // 0x004220f0..0x00422103
            // second half of a curved line: measured back from the target tile's centre (0x00422105..0x00422138)
            const int a2 = static_cast<int>(static_cast<unsigned>(angle) - static_cast<unsigned>(curve) * 0x15555554u);
            px = txw - kFoldRange(a2, L) + 0x200;
            py = kAbsDiff(a2, L) + tyw + 0x200;
        }
        const int fx = subTile(px), fy = subTile(py);
        const int cell = (py >> 10) + (px >> 10) * 50;                             // 0x00422166..0x00422175
        const int lead = steps <= 5 ? 1 : (steps >= 9 ? 3 : 2);                    // 0x00422180..0x00422192
        const int t = tileType(cell);
        // Scored when the step is among the last `lead` of the clamped run and strict == 0, or when the cell's type
        // record has byte +6 == 0xd (0x00422199..0x004221bd).
        if ((i >= steps - lead && strict == 0) || typeIs13(t)) {
            const int b2 = typeByte2(t);
            if (b2 > 0) score += b2;                                               // 0x004221e0
            if (typeIs13(t)) score += 0x20 / (i + 1);                              // 0x004221fa..0x0042220f
            if (t == 0x11 || kTileBlocked(px >> 10, py >> 10)) score += 0x10;      // 0x0042221e..0x00422231
            // Near a tile edge, half the byte +2 of the neighbour across it, when the line's direction has a
            // component on that axis (0x0042223a..0x00422312): x-1 / x+1 with kDirY, y-1 / y+1 with kDirX.
            if (fx < 0xf && dirY[dir]) score += typeByte2(tileType(cell - 50)) / 2;
            if (fx > 0x30 && dirY[dir]) score += typeByte2(tileType(cell + 50)) / 2;
            if (fy < 0xf && dirX[dir]) score += typeByte2(tileType(cell - 1)) / 2;
            if (fy > 0x30 && dirX[dir]) score += typeByte2(tileType(cell + 1)) / 2;
        }
        // Count the straight-line points (angle unchanged, radius R) on a type whose byte +6 is 0xd
        // (0x00422316..0x00422362).
        const int qx = x0 + kFoldRange(angle, R);
        const int qy = y0 - kAbsDiff(angle, R);
        if (typeIs13(tileType((qy >> 10) + (qx >> 10) * 50))) count13++;
        acc += accStep;
        L -= 0x400;
        R += 0x400;
    }
    int r = score;
    if (count13 >= 2) r++;                                                         // 0x004223a1..0x004223aa
    if (r < 0) r = 0;                                                              // 0x004223ad
    ref<unsigned char>(0x005783a2) = 0xff;
    return r;
}

}  // namespace

SG_HOOK("golf_clean.exe", 0x00421FA0, scanTileLine, ScanTileLine_re, ScanTileLine_orig);
SG_HOOK("golf_clean.exe", 0x00474780, bufInit, BufInit_re, BufInit_orig);
SG_HOOK("golf_clean.exe", 0x004747A0, bufCtor, BufCtor_re, BufCtor_orig);
SG_HOOK("golf_clean.exe", 0x00487A20, nodeListBCtor, NodeListBCtor_re, NodeListBCtor_orig);
SG_HOOK("golf_clean.exe", 0x00487B40, nodeListCCtor, NodeListCCtor_re, NodeListCCtor_orig);
SG_HOOK("golf_clean.exe", 0x00487630, resourceQuery, ResourceQuery_re, ResourceQuery_orig);
SG_HOOK("golf_clean.exe", 0x00487C00, resourceLoad, ResourceLoad_re, ResourceLoad_orig);
SG_HOOK("golf_clean.exe", 0x00487A70, resourceLoadB, ResourceLoadB_re, ResourceLoadB_orig);
SG_HOOK("golf_clean.exe", 0x0042F7A0, rebuildHeightfield, RebuildHeightfield_re, RebuildHeightfield_orig);

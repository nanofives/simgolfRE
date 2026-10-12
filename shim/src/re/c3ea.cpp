// C3 batch c3ea (simulation plan phase 2, 2026-10-11) of golf_clean.exe: the shot physics cycle
//   simulateShot 0x00424120 -> simulateBallFlight 0x00422fb0 -> trialShot 0x004226a0 -> simulateShot.
// Fixtures: re/frida/js/fixtures.d/c3ea.js. Registry: re/frida/registry.d/c3ea.py. Purposes and per-jcc tables:
// log/c3/c3ea_purpose.md.
//
// Every body is hand-written from the disassembly (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr> --list) and
// cites the instructions it reproduces. Callees run through their original addresses (the cycle partners too, so a hooked
// partner runs its own SG_HOOK). __thiscall is emulated with __fastcall (ecx = this, edx unused).
//
// Stack frames. simulateBallFlight and simulateShot read stack slots they never write first (simulateBallFlight: the
// strict flag at [F+0xb4], and the screen position [F+0x9c]/[F+0x98] that tileToScreen 0x0042f940 leaves unwritten for a
// tile it does not cache; simulateShot: the direction slot dirs[t] at [F+0xd8 + 4t] of a tile type the path never met).
// So both are entered through a naked thunk that builds the original's frame at the same address (same `sub esp` /
// __alloca_probe size and the same four register pushes) and passes its base F to the body, which keeps every local at
// the original's offset from F. A hooked call therefore reads exactly the stack bytes the original would read. Nothing
// below F is read before it is written, in the original or here.
#include "hooks.h"

namespace {

template <typename T> static inline T& G(unsigned addr) { return *reinterpret_cast<T*>(addr); }
static inline int Shl(int v, int n) { return static_cast<int>(static_cast<unsigned>(v) << n); }
static inline int Add(int a, int b) { return static_cast<int>(static_cast<unsigned>(a) + static_cast<unsigned>(b)); }
static inline int Sub(int a, int b) { return static_cast<int>(static_cast<unsigned>(a) - static_cast<unsigned>(b)); }
static inline int Mul(int a, int b) { return static_cast<int>(static_cast<unsigned>(a) * static_cast<unsigned>(b)); }

typedef int(__cdecl* C0_t)();
typedef int(__cdecl* C1_t)(int a);
typedef int(__cdecl* C2_t)(int a, int b);
typedef int(__cdecl* C3_t)(int a, int b, int c);
typedef int(__cdecl* C4_t)(int a, int b, int c, int d);
typedef int(__cdecl* C5_t)(int a, int b, int c, int d, int e);
typedef int(__cdecl* C7_t)(int a, int b, int c, int d, int e, int f, int g);
typedef int(__cdecl* TileToScreen_t)(int tx, int ty, int* sx, int* sy);
typedef char*(__cdecl* Itoa_t)(int v, char* buf, int radix);
typedef int(__cdecl* DrawText_t)(const char* text, int x, int y, int colour);
typedef int(__fastcall* T1_t)(void* ecx, void* edx, int a);
typedef int(__fastcall* T4_t)(void* ecx, void* edx, int a, int b, int c, int d);

// ---- callees (original addresses)
const C2_t kTileBlocked = reinterpret_cast<C2_t>(0x0040bf60);
const C2_t kHeightAt = reinterpret_cast<C2_t>(0x0042fa30);         // heightAt42fa30(x, y)
const C2_t kFoldRange = reinterpret_cast<C2_t>(0x00467270);
const C2_t kAbsDiff = reinterpret_cast<C2_t>(0x004672b0);
const C2_t kDistance = reinterpret_cast<C2_t>(0x0040acd0);
const C2_t kAngleFixed = reinterpret_cast<C2_t>(0x004672d0);
const C2_t kObjectAt = reinterpret_cast<C2_t>(0x0040df80);
const C3_t kSlopeMix = reinterpret_cast<C3_t>(0x0040c450);
const C3_t kClamp = reinterpret_cast<C3_t>(0x00467130);            // clamp(v, lo, hi)
const C3_t kHeightBlend = reinterpret_cast<C3_t>(0x0040c170);      // the callers push a third 0 (cdecl, unused by it)
const C3_t kShotPower = reinterpret_cast<C3_t>(0x00422430);
const C3_t kEmitThought = reinterpret_cast<C3_t>(0x00467a00);      // emitGolferThought(g, kind, arg)
const C4_t kInRange = reinterpret_cast<C4_t>(0x004070b0);
const C4_t kTileDistance = reinterpret_cast<C4_t>(0x0040c4b0);
const C4_t kTrialShot = reinterpret_cast<C4_t>(0x004226a0);
const C5_t kSimulateShot = reinterpret_cast<C5_t>(0x00424120);
const C1_t kSimulateBallFlight = reinterpret_cast<C1_t>(0x00422fb0);
const C1_t kUpdatePairSnapshot = reinterpret_cast<C1_t>(0x00409950);
const C1_t kRateTile = reinterpret_cast<C1_t>(0x00422530);
const C1_t kSign = reinterpret_cast<C1_t>(0x00467150);
const C1_t kThoughtFlag = reinterpret_cast<C1_t>(0x004675d0);
const C1_t kTypeBit7Clear = reinterpret_cast<C1_t>(0x0046c940);
const C1_t kPumpAndRedraw = reinterpret_cast<C1_t>(0x0045c0c0);
const C0_t kWobble = reinterpret_cast<C0_t>(0x00405920);
const C0_t kPumpInput483bd0 = reinterpret_cast<C0_t>(0x00483bd0);
const C7_t kScanTileLine = reinterpret_cast<C7_t>(0x00421fa0);
const TileToScreen_t kTileToScreen = reinterpret_cast<TileToScreen_t>(0x0042f940);
const Itoa_t kItoa = reinterpret_cast<Itoa_t>(0x004ad425);          // CRT _itoa (static)
const DrawText_t kDrawCenteredText2 = reinterpret_cast<DrawText_t>(0x00404bc0);
const T1_t kRandomRange = reinterpret_cast<T1_t>(0x0045c1e0);       // Random::range(this = 0x00822d9c, n)
const T1_t kRefreshFull = reinterpret_cast<T1_t>(0x00480c80);       // Window::refreshFull(this, unused)
const T4_t kSetDrawTarget = reinterpret_cast<T4_t>(0x004762d0);     // setDrawTarget(this, surf, a, b, c)

static inline int RandRange(int n) { return kRandomRange(reinterpret_cast<void*>(0x00822d9c), 0, n); }

// ---- tables
// golfer records 0x005794b8 + 0x100 * g; tile types 0x005722e8 [x*50 + y] (signed bytes); tile flag words 0x0053caf0;
// wall bits 0x005619a0; type records 0x00578370 + 0x30 * t; hole records 0x00575ac8 + 0x208 * h; octant steps
// kDirX 0x004c2878 / kDirY 0x004c2898 (8 dwords each).
static inline unsigned char* Rec(int g) { return reinterpret_cast<unsigned char*>(0x005794b8 + Shl(g, 8)); }
static inline int& RI(unsigned char* r, unsigned off) { return *reinterpret_cast<int*>(r + off); }
static inline unsigned char& RB(unsigned char* r, unsigned off) { return *(r + off); }
static inline signed char RS(unsigned char* r, unsigned off) { return static_cast<signed char>(*(r + off)); }
static inline unsigned short& RW(unsigned char* r, unsigned off) { return *reinterpret_cast<unsigned short*>(r + off); }
static inline int Tile(int idx) { return static_cast<signed char>(G<unsigned char>(0x005722e8 + idx)); }
static inline unsigned char TileByte(int idx) { return G<unsigned char>(0x005722e8 + idx); }
static inline unsigned short TileFlags(int idx) { return G<unsigned short>(0x0053caf0 + idx * 2); }
static inline int TypeS(int t, int off) { return static_cast<signed char>(G<unsigned char>(0x00578370 + t * 0x30 + off)); }
static inline unsigned char TypeU(int t, int off) { return G<unsigned char>(0x00578370 + t * 0x30 + off); }
static inline int HoleX(int h) { return G<int>(0x00575ac8 + h * 0x208); }
static inline int HoleY(int h) { return G<int>(0x00575acc + h * 0x208); }
static inline int DirX(int k) { return G<int>(0x004c2878 + 4 * k); }
static inline int DirY(int k) { return G<int>(0x004c2898 + 4 * k); }
static inline int HoleOf(unsigned char* r) { return RS(r, 0x21); }   // movsx byte [rec+0x21] (0x005794d9)

// isqrt as the originals do it: fild dword, fsqrt, then the CRT's __ftol 0x004a6030 (truncation, low dword), all on the
// x87 under the calling thread's control word (0x0042317b..0x00423181, 0x004233e1..0x004233e7, 0x00424289..0x0042428f,
// 0x00424519..0x0042451f).
static int SqrtFtol(int v) {
    int r;
    __asm {
        fild dword ptr [v]
        fsqrt
        mov eax, 0x004a6030
        call eax
        mov r, eax
    }
    return r;
}

}  // namespace

// ====================================================================================================================
// 0x004226a0  trialShot(g, x, y, aim)
// ====================================================================================================================
// cdecl, plain `ret` (0x00422faf); eax on return is the record's speed word, which the loop exit leaves at 0 (0x00422f8a
// .. 0x00422f92, 0x00422727), so 0 on every path.
//   pair = the LAST of the 16 pair snapshots (0x0059fc60, 0x388 bytes each, signed word at +0) whose word equals g, else -1
//   (0x004226aa..0x004226d7). The golfer record is copied to the scratch 0x00582fb8 (0x004226fb) and copied back at the
//   end (0x00422fa7): every record field trialShot or simulateShot changes is undone; what remains is the landing point
//   [0x0056a78c]/[0x0056a790], the pair snapshots, the RNG seed and simulateShot's globals.
//   simulateShot(g, 0, x, y, aim) (0x00422706), then spin = spin / 2 (signed, 0x0042270b..0x00422719).
//   Then the ball rolls/flies while speed != 0 (0x00422727 / 0x00422f92), one step per iteration.
int __cdecl TrialShot_re(int g, int x, int y, int aim) {
    int pair = -1;
    for (int k = 0; k < 16; ++k)                                                        // 0x004226c1..0x004226d7
        if (static_cast<int>(G<short>(0x0059fc60 + 0x388 * k)) == g) pair = k;
    unsigned char* rec = Rec(g);
    for (int k = 0; k < 0x40; ++k)                                                      // rep movsd 0x004226fb
        G<int>(0x00582fb8 + 4 * k) = RI(rec, 4 * k);
    kSimulateShot(g, 0, x, y, aim);                                                     // 0x00422706
    RI(rec, 0xec) = RI(rec, 0xec) / 2;                                                  // 0x0042270b..0x00422719
    int steps = 0;                                                                      // [esp+0xc] 0x004226b2
    while (RI(rec, 0xe4) != 0) {                                                        // 0x00422727 / 0x00422f92
        const int ty = RI(rec, 0xd8) >> 10;                                             // 0x0042273a
        const int tx = RI(rec, 0xd4) >> 10;                                             // 0x0042273d
        const int type = kTileBlocked(tx, ty) ? 0x14 : Tile(tx * 50 + ty);              // 0x00422746..0x00422762
        const int h0 = kHeightAt(RI(rec, 0xd4), RI(rec, 0xd8));                         // 0x0042277c
        const int dx = kFoldRange(RI(rec, 0xe0), RI(rec, 0xe4) / 16);                   // 0x00422783..0x0042279a
        const int dy = kAbsDiff(RI(rec, 0xe0), RI(rec, 0xe4) / 16);                     // 0x004227a1..0x004227bc
        RI(rec, 0xd4) = Add(RI(rec, 0xd4), dx);                                         // 0x004227cb / 0x004227d0
        RI(rec, 0xd8) = Sub(RI(rec, 0xd8), dy);                                         // 0x004227dc / 0x004227de
        const int z = Add(RI(rec, 0xdc), RI(rec, 0xe8) / 32);                           // 0x004227e4..0x004227f9
        RI(rec, 0xdc) = z;
        if (z != 0 || RI(rec, 0xe8) != 0) {                                             // 0x00422801 / 0x0042280b
            const int vz = Sub(RI(rec, 0xe8), 0x40);                                    // 0x00422817 / 0x0042281d
            RI(rec, 0xe8) = vz;
            if (pair != -1 && vz > 0 && vz < 0x40) kUpdatePairSnapshot(pair);           // 0x00422825..0x00422831
        }
        ++steps;                                                                        // 0x00422843
        if (pair != -1 && (steps & 7) == 0) kUpdatePairSnapshot(pair);                  // 0x0042284b..0x00422854
        const int fx = (RI(rec, 0xd4) >> 6) & 15;                                       // 0x0042286c / 0x00422872
        const int fy = (RI(rec, 0xd8) >> 6) & 15;
        int edge = 0;                                                                   // [esp+0x38] 0x00422883
        if (fx <= 1 && Tile((tx - 1) * 50 + ty) != type) edge = 1;                      // 0x00422878..0x004228a5 (0x005722b6)
        if (fy <= 1 && Tile(tx * 50 + ty - 1) != type) edge = 1;                        // 0x004228b1..0x004228d0 (0x005722e7)
        if (fx >= 14 && Tile((tx + 1) * 50 + ty) != type) edge = 1;                     // 0x004228dc..0x004228f9 (0x0057231a)
        if (fy >= 14 && Tile(tx * 50 + ty + 1) != type) edge = 1;                       // 0x00422901..0x00422920 (0x005722e9)
        const int oct = (((RI(rec, 0xe0) >> 28) + 1) >> 1) & 7;                         // 0x00422934..0x0042293a
        if (RI(rec, 0xdc) > 1) {                                                        // 0x0042293d / 0x00422944: in the air
            const int h1 = kHeightAt(RI(rec, 0xd4), RI(rec, 0xd8));                     // 0x0042294c
            RI(rec, 0xdc) = Add(RI(rec, 0xdc), Sub(h0, h1));                            // 0x00422951..0x0042295f
            RI(rec, 0xe4) = RI(rec, 0xe4) - (RI(rec, 0xe4) >> 5);                       // 0x00422965..0x00422977
            RI(rec, 0xe0) = Add(RI(rec, 0xe0), RI(rec, 0xec));                          // 0x0042297d..0x0042298c
            const int in = kInRange(type, RI(rec, 0xdc), tx, ty);                       // 0x0042299a
            if (G<int>(0x005a9cc4) != 2 && in != 0) {                                   // 0x004229ad..0x004229b7
                int d = kDistance(RI(rec, 0xd4) - Shl(tx, 10) - 0x200,                  // 0x004229bd..0x004229e3
                                  RI(rec, 0xd8) - Shl(ty, 10) - 0x200);
                if (RB(rec, 0x18) != 0 && (RW(rec, 0x16) & 0x200) != 0)                 // 0x004229ea..0x00422a07
                    d += Mul(RB(rec, 0xf9), d) / 4;                                     // 0x00422a09..0x00422a1d
                const int r = RandRange(0x300) & 0xffff;                                // 0x00422a1f..0x00422a2e
                if (d < r) {                                                            // 0x00422a33 / 0x00422a35
                    RI(rec, 0xe0) = Add(RI(rec, 0xe0), Shl((RandRange(0x80) & 0xffff) + 0x40, 24));  // 0x00422a3b..0x00422a5d
                    RI(rec, 0xe4) = RI(rec, 0xe4) - (RandRange(RI(rec, 0xe4)) & 0xffff);  // 0x00422a63..0x00422a81
                    RI(rec, 0x10) |= 2;                                                 // 0x00422a87..0x00422a8f
                }
            }
        } else {                                                                        // 0x00422a9a: on the ground
            int fr;
            if (G<unsigned char>(0x004c2e1c) & 4)                                       // 0x00422a9a / 0x00422aa1
                fr = kClamp(TypeS(type, 1) - kSlopeMix(RI(rec, 0xd4), RI(rec, 0xd8), oct), 0, 0x63);   // 0x00422aaa..0x00422ac6
            else
                fr = TypeS(type, 1);                                                    // 0x00422adc
            const int side = kSlopeMix(RI(rec, 0xd4), RI(rec, 0xd8), (oct + 2) & 7);   // 0x00422aef..0x00422af8
            if (fr < 2 && edge) fr = 2;                                                 // 0x00422b00..0x00422b0d
            else if (fr > 4 && steps > 0x80) fr = 4;                                    // 0x00422b14..0x00422b23
            if (G<unsigned char>(0x004c2e1c) & 4)                                       // 0x00422b28..0x00422b3c
                RI(rec, 0xe0) = Sub(RI(rec, 0xe0), Shl(side, 25));
            if (fr < 5)                                                                 // 0x00422b42..0x00422b5a
                RI(rec, 0xe4) = RI(rec, 0xe4) - (RI(rec, 0xe4) >> (fr & 31)) / 2;
            else                                                                        // 0x00422b67..0x00422b77
                RI(rec, 0xe4) = RI(rec, 0xe4) - (RI(rec, 0xe4) >> 6) + 0x20;
            if (type == 0x11) {                                                         // 0x00422b81..0x00422b8e
                if (!edge) RI(rec, 0xe4) = RI(rec, 0xe4) / 2;                           // 0x00422bbf..0x00422bca
            } else if (type == 10) {                                                    // 0x00422b90..0x00422bbd
                const int ax = 8 - fx < 0 ? fx - 8 : 8 - fx;
                const int ay = 8 - fy < 0 ? fy - 8 : 8 - fy;
                if (ax <= 2 && ay <= 2) RI(rec, 0xe4) = RI(rec, 0xe4) / 2;
            }
            if (tx != (RI(rec, 0xd4) >> 10)) {                                          // 0x00422bd0..0x00422bdb
                const int wall = static_cast<signed char>(G<unsigned char>(0x005619a0 + tx * 50 + ty));   // 0x00422beb
                const int mask = dx > 0 ? 4 : 0x40;                                     // 0x00422bf3..0x00422bfd
                if (mask & wall) RI(rec, 0xe0) = Sub(0, RI(rec, 0xe0));                 // 0x00422c00..0x00422c0c
            }
            if (ty != (RI(rec, 0xd8) >> 10)) {                                          // 0x00422c18..0x00422c23
                const int wall = static_cast<signed char>(G<unsigned char>(0x005619a0 + tx * 50 + ty));   // 0x00422c2f
                const int mask = dy > 0 ? 1 : 0x10;                                     // 0x00422c37..0x00422c42
                if (mask & wall) RI(rec, 0xe0) = Sub(static_cast<int>(0x80000000u), RI(rec, 0xe0));   // 0x00422c45..0x00422c56
            }
        }
        if (RI(rec, 0xdc) <= 0 && RI(rec, 0xe8) < 0) {                                  // 0x00422c5c..0x00422c72: bounce
            const int inner = (fx >= 5 && fx < 11 && fy >= 5) ? 1 : 0;                 // 0x00422c78..0x00422c9a
            const int cell = tx * 50 + ty;
            int b = TypeS(type, 0);                                                     // 0x00422cb0
            if (G<int>(0x005a9cc4) == 0) {                                              // 0x00422cae / 0x00422cb7
                if (b < 2 && edge) b = 2;                                               // 0x00422cb9..0x00422cc4
                if (inner && (TileFlags(cell) & 0x20)) b = 4;                           // 0x00422cc9..0x00422ce4
            }
            const int v = kClamp(-0x40 - Mul(b, RI(rec, 0xe8)) / 12, 0, 9999);         // 0x00422ce9..0x00422d0c
            RI(rec, 0xe8) = v;                                                          // 0x00422d19
            if (v < 0x80) RI(rec, 0xe8) = 0;                                            // 0x00422d14 / 0x00422d21
            RI(rec, 0xdc) = 0;                                                          // 0x00422d2b
            if (pair != -1) kUpdatePairSnapshot(pair);                                  // 0x00422d31..0x00422d37
            if (RI(rec, 0x10) & 0x100) {                                                // 0x00422d3f..0x00422d4c
                RI(rec, 0xe0) ^= static_cast<int>(0x80000000u);                         // 0x00422d4e..0x00422d5a
                RI(rec, 0x10) &= ~0x100;                                                // 0x00422d60..0x00422d69
                if (RB(rec, 0x18) == 0) {                                               // 0x00422d6f..0x00422d7d
                    RI(rec, 0xe4) = RandRange(Shl(RI(rec, 0xe4), 1)) & 0xffff;          // 0x00422d7f..0x00422d91
                } else {
                    const int s = RI(rec, 0xe4);                                        // 0x00422d93
                    const int r = RandRange(Shl(s, 1)) & 0xffff;                        // 0x00422d9e..0x00422da6
                    RI(rec, 0xe4) = Mul(r - s, 3) / (RB(rec, 0xf9) + 5) + s;            // 0x00422dab..0x00422dc2
                }
            }
            if (RB(rec, 0x10) & 0x80) {                                                 // 0x00422dc8 / 0x00422dcf
                RI(rec, 0xe4) = RI(rec, 0xe4) / 2;                                      // 0x00422dd1..0x00422ddc
                RI(rec, 0x10) = (RI(rec, 0x10) & ~0x80) | 0x100;                        // 0x00422de2..0x00422ded
            }
            if (G<unsigned char>(0x004c2e1c) & 4) {                                     // 0x00422df3 / 0x00422dfa
                const int k = Mul(RI(rec, 0xe8), 0xaec33);                              // 0x00422e0f
                const int o = (((RI(rec, 0xe0) >> 29) + 1) & ~1) + 2;                   // 0x00422e0c..0x00422e24 (and al, 0xfe)
                RI(rec, 0xe0) = Sub(RI(rec, 0xe0), Mul(kSlopeMix(RI(rec, 0xd4), RI(rec, 0xd8), o & 7), k));   // 0x00422e2d..0x00422e44
                const int c1 = kClamp(kSlopeMix(RI(rec, 0xd4), RI(rec, 0xd8), oct), -2, 2);   // 0x00422e5d / 0x00422e66
                RI(rec, 0xe4) = Add(RI(rec, 0xe4), Shl(-Mul(c1, RI(rec, 0xe8)), 1));    // 0x00422e6b..0x00422e85
                const int vz = RI(rec, 0xe8);                                           // 0x00422e97
                const int c2 = kClamp(kSlopeMix(RI(rec, 0xd4), RI(rec, 0xd8), oct), -2, 2);   // 0x00422ea4 / 0x00422ead
                RI(rec, 0xe8) = Mul(c2, vz) / 2 + vz;                                   // 0x00422eb2..0x00422ebf
            }
            if (type == 0x11) {                                                         // 0x00422ec9
                if (!edge) {                                                            // 0x00422ed6
                    if (G<int>(0x005a9cc4) != 0 || !inner || !(TileFlags(cell) & 0x20)) {   // 0x00422ed8..0x00422efb
                        RI(rec, 0xe8) = 0;                                              // 0x00422efd
                        RI(rec, 0xe4) = 0;                                              // 0x00422f03
                    }
                }
            } else if (type == 0xc && RI(rec, 0xe4) > 0x100 && !edge) {                 // 0x00422f0b..0x00422f1e
                RI(rec, 0xe0) = Add(RI(rec, 0xe0), Shl(0x50 - (RandRange(0xa0) & 0xffff), 24));   // 0x00422f20..0x00422f46
            }
        }
        if (RI(rec, 0xe4) < 0x40 && RI(rec, 0xdc) == 0 && RI(rec, 0xe8) == 0) {        // 0x00422f4c..0x00422f67: at rest
            G<int>(0x0056a78c) = RI(rec, 0xd4);                                         // 0x00422f75
            G<int>(0x0056a790) = RI(rec, 0xd8);                                         // 0x00422f7b
            RI(rec, 0xe4) = 0;                                                          // 0x00422f80
        }
    }
    for (int k = 0; k < 0x40; ++k)                                                      // rep movsd 0x00422fa7
        RI(rec, 4 * k) = G<int>(0x00582fb8 + 4 * k);
    return 0;
}
C4_t TrialShot_orig;
// 0x004226a0
SG_HOOK("golf_clean.exe", 0x004226a0, trialShot, TrialShot_re, TrialShot_orig);

// ====================================================================================================================
// 0x00422fb0  simulateBallFlight(g): the AI aim search. cdecl, plain `ret` (0x004240e8); returns the chosen aim (-1, 0, 1).
// ====================================================================================================================
// Frame: __alloca_probe 0x004a6070 with eax = 0x7cb4 (0x00422fb0 / 0x00422fb5) and the pushes ebx, ebp, esi, edi, so
// F = esp after them and g is at [F+0x7cc8]. Three cell arrays of 21 x 21 cells x 6 (variant 0/1 x aim -1/0/1) dwords:
// A at F+0xbc (scores, zeroed once at 0x00422fd2), B at F+0x2a14 (distance from the probe point to the hole, zeroed every
// pass at 0x0042330f), C at F+0x536c (the probe angle with bit 0 = "the trial got 25 closer and halved the distance").
#define FI(o) (*reinterpret_cast<int*>(F + (o)))
static int __cdecl SimulateBallFlightBody(unsigned char* F) {
    const int g = FI(0x7cc8);
    for (int k = 0; k < 0xa56; ++k) FI(0xbc + 4 * k) = 0;                               // rep stosd 0x00422fd2
    G<int>(0x0059e7b8) |= 0x800000;                                                     // 0x00422fcc..0x00422fe8
    unsigned char* rec = Rec(g);
    G<int>(0x004c2e1c) = RB(rec, 0x19);                                                 // 0x00422ff3 / 0x00423001
    G<int>(0x0059b738) = 1;                                                             // 0x00423009
    FI(0x30) = Shl(g, 8);                                                               // 0x0042300f (record offset)
    G<int>(0x005a9ce4) = 1;                                                             // 0x00423013
    G<int>(0x005a9ce8) = 0;                                                             // 0x00423019
    FI(0x78) = -1;                                                                      // 0x0042301f: best cell x
    FI(0x6c) = 0;                                                                       // 0x00423027: best aim (the result)
    FI(0x4c) = kRateTile(g);                                                            // 0x0042302b / 0x00423042: reach
    FI(0xa0) = RI(rec, 0xcc);                                                           // 0x0042304c: target tile at entry
    FI(0xa8) = RI(rec, 0xd0);                                                           // 0x00423056
    const int bx = RI(rec, 0xd4) >> 10, by = RI(rec, 0xd8) >> 10;                       // 0x00423053 / 0x0042306b
    FI(0x90) = bx;                                                                      // 0x00423064
    FI(0x58) = by;                                                                      // 0x00423071
    FI(0x94) = 0x005722e8 + bx * 50 + by;                                               // 0x00423075 / 0x00423082
    const int ballType = static_cast<signed char>(G<unsigned char>(FI(0x94)));         // 0x00423089
    if (G<int>(0x005a9cc4) == 0 && !(RB(rec, 0x10) & 1) &&                              // 0x0042308c..0x004230b2
        (TypeS(ballType, 2) <= 0 || RB(rec, 0x18) == 0))
        G<int>(0x005a9cc4) = 1;
    FI(0x3c) = (TypeS(ballType, 2) > 0) ? 0 : ((RB(rec, 0x19) & 4) ? 3 : 0);           // 0x004230b8..0x004230da: aim mask
    if (RB(rec, 0x18) != 0) {                                                           // 0x004230e2 / 0x004230ea
        const unsigned short w = RW(rec, 0x16);                                         // 0x004230ec
        if (!(w & 0x20)) FI(0x3c) &= ~1;                                                // 0x004230f3..0x004230f7
        if (!(w & 0x40)) FI(0x3c) &= ~2;                                                // 0x004230fc..0x00423106
    }
    FI(0x80) = HoleOf(rec) * 0x208;                                                     // 0x0042310a..0x00423114
    RI(rec, 0xcc) = G<int>(0x00575ac8 + FI(0x80));                                      // 0x0042311b / 0x00423121
    RI(rec, 0xd0) = G<int>(0x00575acc + FI(0x80));                                      // 0x00423127 / 0x0042312d
    {
        const int dx = Mul(Sub(Shl(RI(rec, 0xcc), 10), RI(rec, 0xd4)) + 0x200, 25) >> 10;   // 0x00423133..0x0042315f
        const int dy = Mul(Sub(Shl(RI(rec, 0xd0), 10), RI(rec, 0xd8)) + 0x200, 25) >> 10;   // 0x00423149..0x0042316a
        FI(0x5c) = SqrtFtol(Add(Mul(dy, dy), Mul(dx, dx)));                             // 0x0042316d..0x00423188: hole distance
        RI(rec, 0xe0) = kAngleFixed(dx, dy);                                            // 0x0042318c / 0x00423198
    }
    FI(0x88) = 0;                                                                       // 0x004231a7
    RB(rec, 0x22) = static_cast<unsigned char>(RB(rec, 0x22) + 1);                      // 0x0042319e..0x004231b2
    if (FI(0x5c) > kRateTile(g) + FI(0x4c)) FI(0x88) = 1;                               // 0x004231b8..0x004231ce: out of reach
    RB(rec, 0x22) = static_cast<unsigned char>(RB(rec, 0x22) - 1);                      // 0x004231d9..0x004231e8
    {
        const int len = (Shl(kClamp(FI(0x4c) - 25, 0, FI(0x5c)), 10) + 0x200) / 25;    // 0x004231df..0x00423216
        const int ax = (kFoldRange(RI(rec, 0xe0), len) + RI(rec, 0xd4)) >> 10;          // 0x00423220..0x0042323c
        FI(0x54) = ax;                                                                  // 0x00423240: aim tile x
        const int y0 = RI(rec, 0xd8);                                                   // 0x00423233 / 0x00423244
        FI(0x44) = y0;
        const int ay = (y0 - kAbsDiff(RI(rec, 0xe0), len)) >> 10;                       // 0x00423248..0x0042325d
        FI(0x8c) = ay;                                                                  // 0x00423264: aim tile y
        kTileDistance(RI(rec, 0xd4), y0, ax, ay);                                       // 0x0042326b (result unused)
        const int h = HoleOf(rec) * 0x208;                                              // 0x00423270..0x0042327e
        kDistance(ax - G<int>(0x00575ac8 + h), ay - G<int>(0x00575acc + h));            // 0x00423293 (result unused)
        const int h2 = HoleOf(rec) * 0x208;                                             // 0x00423298..0x004232a6
        const int d = kTileDistance(bx, FI(0x58), G<int>(0x00575ac8 + h2), G<int>(0x00575acc + h2));   // 0x004232c0
        FI(0x68) = d < 100 ? 2 : 4;                                                     // 0x004232c8..0x004232da: divisor
        if (d > 200) FI(0x68) = 6;                                                      // 0x004232de..0x004232e5
    }
    FI(0x44) = 0;                                                                       // 0x004232ed: trials run
    FI(0x38) = 2;                                                                       // 0x004232f5: trials per cell
    for (;;) {                                                                          // 0x004232f9: one pass
        for (int k = 0; k < 0xa56; ++k) FI(0x2a14 + 4 * k) = 0;                         // rep stosd 0x0042330f
        FI(0x60) = 99999;                                                               // 0x00423307: best score
        for (FI(0x20) = -10; FI(0x20) <= 10; ++FI(0x20)) {                              // 0x00423311 / 0x00423daf..0x00423dbb
            FI(0xb0) = (FI(0x20) + 10) * 21;                                            // 0x00423319..0x00423334
            for (FI(0x24) = -10; FI(0x24) <= 10; ++FI(0x24)) {                          // 0x0042331d / 0x00423d9d..0x00423da9
                const int cell = FI(0xb0) + FI(0x24) + 10;                              // 0x00423342 / 0x00423346
                FI(0x50) = cell;
                const int co = cell * 24;
                FI(0x28) = reinterpret_cast<int>(F + 0xc0 + co);
                const int a1 = FI(0xc0 + co);                                           // 0x00423355
                if (a1 > 99999) continue;                                               // 0x00423363 / 0x0042336d
                FI(0x2c) = FI(0x54) + FI(0x20);                                         // 0x00423373..0x0042338a: cell tile x
                FI(0x18) = FI(0x24) + FI(0x8c);                                         // 0x0042337d..0x0042338e: cell tile y
                FI(0x84) = Shl(FI(0x2c), 10);                                           // 0x0042339b
                const int ex = Mul(Sub(FI(0x84), RI(rec, 0xd4)) + 0x200, 25) >> 10;     // 0x004233a2..0x004233c5
                FI(0x74) = Shl(FI(0x18), 10);                                           // 0x004233ba
                const int ey = Mul(Sub(FI(0x74), RI(rec, 0xd8)) + 0x200, 25) >> 10;     // 0x004233be..0x004233d3
                FI(0x7c) = SqrtFtol(Add(Mul(ey, ey), Mul(ex, ex)));                     // 0x004233cd..0x004233ee: cell distance
                FI(0x40) = kAngleFixed(ex, ey);                                         // 0x004233f5 / 0x004233fd
                FI(0x10) = 1;                                                           // 0x00423403
                if (a1 == 0) {                                                          // 0x0042340b: first visit, vet the cell
                    FI(0xc4 + co) = 100000; FI(0x48) = reinterpret_cast<int>(F + 0xc4 + co);   // 0x00423415..0x00423424
                    FI(0xc0 + co) = 100000;                                             // 0x00423431
                    FI(0xbc + co) = 100000; FI(0x34) = reinterpret_cast<int>(F + 0xbc + co);   // 0x0042342a..0x0042343b
                    FI(0xd0 + co) = 100000; FI(0x14) = reinterpret_cast<int>(F + 0xd0 + co);   // 0x00423441..0x00423450
                    FI(0xcc + co) = 100000; FI(0x1c) = reinterpret_cast<int>(F + 0xcc + co);   // 0x00423456..0x00423468
                    FI(0xc8 + co) = 100000; FI(0x70) = reinterpret_cast<int>(F + 0xc8 + co);   // 0x00423461..0x0042347a
                    const int cx = FI(0x2c), cy = FI(0x18);
                    if ((RB(rec, 0x10) & 1) && cx == FI(0xa0) && cy == FI(0xa8)) continue;   // 0x0042346e..0x00423499
                    if (kTileBlocked(cx, cy)) continue;                                 // 0x004234a7..0x004234b1
                    const int cd = FI(0x7c), md = FI(0x4c);
                    if (cd > md + 16) continue;                                         // 0x004234b7..0x004234c4
                    if (!(cd >= md / 3 || cd >= FI(0x5c) / 3 || (RB(rec, 0x10) & 1))) continue;   // 0x004234ca..0x004234f7
                    if (cx == FI(0x90) && cy == FI(0x58)) continue;                     // 0x004234fd..0x0042350a
                    const int t = Tile(cx * 50 + cy);                                   // 0x00423516
                    if (t == 0) continue;                                               // 0x0042351d / 0x00423526
                    const int t2 = TypeS(t, 2);                                         // 0x00423535
                    if (t2 > 1) continue;                                               // 0x0042353b / 0x0042353d
                    if (t2 == 1) {                                                      // 0x00423543: needs a b2 <= 0 neighbour
                        FI(0x10) = 0;                                                   // 0x00423547
                        for (int k = 0; k < 8; k += 2)                                  // 0x0042354b..0x00423588
                            if (TypeS(Tile((DirX(k) + cx) * 50 + DirY(k) + cy), 2) <= 0) FI(0x10) = 1;
                    }
                    const int h = HoleOf(rec) * 0x208;                                  // 0x0042358a..0x00423598
                    const int d1 = kDistance(cx - G<int>(0x00575ac8 + h), cy - G<int>(0x00575acc + h));   // 0x004235ad
                    const int h2 = HoleOf(rec) * 0x208;                                 // 0x004235b2..0x004235cd
                    const int d2 = kDistance(FI(0x90) - G<int>(0x00575ac8 + h2), FI(0x58) - G<int>(0x00575acc + h2));   // 0x004235e2
                    if (TileByte(cx * 50 + cy) == 1 && d1 > 3) continue;               // 0x004235e9..0x004235f6
                    const int e = FI(0x10) ? 1 : d2 / 2;                                // 0x004235fc..0x00423610
                    if (e + d1 > d2) continue;                                          // 0x00423612..0x00423616
                    FI(0xc4 + co) = 0; FI(0xc0 + co) = 0; FI(0xbc + co) = 0;            // 0x0042361c..0x00423652
                    FI(0xd0 + co) = 0; FI(0xcc + co) = 0; FI(0xc8 + co) = 0;
                }
                // 0x00423658: evaluate the cell, variant v (0: the cell centre, 1: its corner) x aim (-1, 0, 1)
                for (FI(0x10) = 0; FI(0x10) < 2; ++FI(0x10)) {                          // 0x0042365e / 0x00423d8f..0x00423d97
                    if (FI(0x10) != 0 &&                                                // 0x00423662..0x0042367e
                        TypeS(static_cast<signed char>(G<unsigned char>(FI(0x94))), 2) > 0)
                        continue;
                    FI(0xb8) = FI(0x10) * 3 + 1;                                        // 0x00423684 / 0x0042368b
                    FI(0x70) = FI(0x50) * 6;                                            // 0x00423692..0x0042369b
                    for (FI(0x14) = -1; FI(0x14) <= 1; ++FI(0x14)) {                    // 0x00423697 / 0x00423d75..0x00423d81
                        const int idx = FI(0xb8) + FI(0x14) + FI(0x70);                 // 0x004236a5..0x004236ae
                        FI(0x34) = idx * 4;                                             // 0x004236b3
                        int& score = FI(0xbc + idx * 4);
                        if (score >= 99999) continue;                                   // 0x004236c5 / 0x004236cb
                        const int aim = FI(0x14);
                        if (aim == -1 && !(FI(0x3c) & 1)) { score = 100000; continue; } // 0x004236d1..0x004236e3
                        if (aim == 1 && !(FI(0x3c) & 2)) { score = 100000; continue; }  // 0x004236e8..0x004236fa
                        if (aim != 0 && FI(0x7c) <= 0x4b) { score = 100000; continue; } // 0x00423703..0x00423710
                        FI(0x1c) = 0;                                                   // 0x0042371d: trial score
                        FI(0x28) = 0;                                                   // 0x00423721: landings on b2 <= 0
                        if (FI(0x38) > 0) {                                             // 0x00423719..0x00423725
                            FI(0x48) = FI(0x38);                                        // 0x0042372b
                            do {
                                if (FI(0x10) == 0) {                                    // 0x0042372f..0x00423769
                                    kTrialShot(g, FI(0x84) + 0x200, FI(0x74) + 0x200, FI(0x14));
                                    ++FI(0x44);
                                } else {                                                // 0x0042376f..0x0042378c
                                    kTrialShot(g, Shl(FI(0x2c), 10), Shl(FI(0x18), 10), FI(0x14));
                                }
                                if (RB(rec, 0x19) & 4) {                                // 0x00423794..0x0042379f: the 8 cells around
                                    int sc = FI(0x1c);
                                    for (int k = 0; k < 8; ++k) {                       // 0x004237ab..0x00423836
                                        const int div = 3 + (k & 1);                    // 0x004237ad..0x004237bd
                                        const int px = (Shl(DirX(k), 10) / div + G<int>(0x0056a78c)) >> 10;   // 0x004237c0..0x004237de
                                        const int py = (Shl(DirY(k), 10) / div + G<int>(0x0056a790)) >> 10;   // 0x004237d1..0x004237ea
                                        if (kTileBlocked(px, py)) {                     // 0x004237ef..0x004237f9
                                            sc += static_cast<signed char>(G<unsigned char>(0x00578732));   // 0x004237fb
                                        } else {
                                            const int ix = px * 50 + py;
                                            sc += TypeS(Tile(ix), 2);                   // 0x0042380f..0x00423823
                                            if (TileFlags(ix) & 0x80) --sc;             // 0x00423825..0x00423831
                                        }
                                    }
                                    FI(0x1c) = sc;                                      // 0x0042383c
                                }
                                const int lx = G<int>(0x0056a78c) >> 10, ly = G<int>(0x0056a790) >> 10;   // 0x00423840..0x0042384e
                                FI(0x64) = lx * 50 + ly;                                // 0x00423859 / 0x0042385c: landing cell
                                int lv = TypeS(Tile(FI(0x64)), 2);                      // 0x00423860..0x0042386d
                                if (kTileBlocked(lx, ly) || (TileFlags(FI(0x64)) & 0x400)) {   // 0x00423874..0x00423893
                                    lv = static_cast<signed char>(G<unsigned char>(0x00578732));   // 0x004238b5
                                } else if (TileFlags(FI(0x64)) & 0x80) {                // 0x00423895 / 0x00423897
                                    if (static_cast<short>(TileFlags(FI(0x64)) & 0x1f) ==    // 0x00423899..0x004238ab
                                        static_cast<short>(HoleOf(rec)))
                                        --lv;                                           // 0x004238ad
                                    else
                                        lv += 2;                                        // 0x004238b0
                                }
                                if (FI(0x40) & 1) FI(0x40) ^= 1;                        // 0x004238bc..0x004238c8
                                if (TypeS(Tile(FI(0x64)), 2) <= 0) ++FI(0x28);          // 0x004238cc..0x004238e3
                                int td;
                                {
                                    const int h = HoleOf(rec) * 0x208;                  // 0x004238f1..0x00423905
                                    if (FI(0x10) == 0)                                  // 0x004238fa..0x0042392d
                                        td = kTileDistance(FI(0x84) + 0x200, FI(0x74) + 0x200,
                                                           G<int>(0x00575ac8 + h), G<int>(0x00575acc + h));
                                    else                                                // 0x00423930..0x00423959
                                        td = kTileDistance(Shl(FI(0x2c), 10), Shl(FI(0x18), 10),
                                                           G<int>(0x00575ac8 + h), G<int>(0x00575acc + h));
                                }
                                const int off = FI(0x34);
                                FI(0x2a14 + off) = td;                                  // 0x00423966
                                {
                                    const int h = HoleOf(rec) * 0x208;                  // 0x0042395f..0x00423977
                                    const int t2 = kTileDistance(G<int>(0x0056a78c), G<int>(0x0056a790),   // 0x00423995
                                                                 G<int>(0x00575ac8 + h), G<int>(0x00575acc + h));
                                    const int b = FI(0x2a14 + off);                     // 0x0042399a
                                    if (b - t2 > 25 && b > Shl(t2, 1)) FI(0x40) |= 1;   // 0x004239a4..0x004239b6
                                }
                                FI(0x536c + off) = FI(0x40);                            // 0x004239c1
                                {
                                    const int t3 = kTileDistance(G<int>(0x0056a78c), G<int>(0x0056a790),   // 0x004239ba..0x004239e3
                                                                 G<int>(0x00575ac8 + FI(0x80)), G<int>(0x00575acc + FI(0x80)));
                                    FI(0x1c) = t3 / FI(0x68) + FI(0x1c) + lv * 8;       // 0x004239e8..0x00423a00
                                }
                                if (FI(0x38) == 4 && G<int>(0x005a9cc4) != 2 && (RB(rec, 0x19) & 4) && FI(0x88) == 0) {   // 0x004239f9..0x00423a2d
                                    const int h = HoleOf(rec) * 0x208;                  // 0x00423a33
                                    const int hx = G<int>(0x00575ac8 + h), hy = G<int>(0x00575acc + h);
                                    if (lv > 0) {                                       // 0x00423a3a / 0x00423a3e
                                        const int s = kScanTileLine(G<int>(0x0056a78c), G<int>(0x0056a790), hx, hy,   // 0x00423a6c
                                                                    FI(0x4c), 0, 0);
                                        FI(0x1c) = FI(0x1c) + s * 2;                    // 0x00423a71..0x00423a77
                                    } else {
                                        const int t4 = kTileDistance(G<int>(0x0056a78c), G<int>(0x0056a790), hx, hy);   // 0x00423aa3
                                        int reach = FI(0x4c);                           // 0x00423aa8
                                        if (RB(rec, 0x22) == 0) reach = reach - reach / 5;   // 0x00423aae..0x00423acb
                                        if ((RB(rec, 0x19) & 4) && TypeS(Tile(FI(0x64)), 2) <= 0 && t4 >= 0x32 &&   // 0x00423acd..0x00423b0c
                                            Mul(reach - t4, 60) / Mul(reach, 3) >= 4)
                                            FI(0xb4) = 1;                               // 0x00423b0e (never cleared or initialised)
                                        const int h2 = HoleOf(rec) * 0x208;             // 0x00423b19
                                        const int hx2 = G<int>(0x00575ac8 + h2), hy2 = G<int>(0x00575acc + h2);
                                        int m = kScanTileLine(G<int>(0x0056a78c), G<int>(0x0056a790), hx2, hy2,   // 0x00423b50
                                                              reach, 0, FI(0xb4));   // 0x00423b20 ([F+0xb4])
                                        if (FI(0x3c) & 1) {                             // 0x00423b55..0x00423b61
                                            const int h3 = HoleOf(rec) * 0x208;
                                            const int s = kScanTileLine(G<int>(0x0056a78c), G<int>(0x0056a790),   // 0x00423b95
                                                                        G<int>(0x00575ac8 + h3), G<int>(0x00575acc + h3), reach, -1, 0);
                                            if (s < m) m = s;                           // 0x00423b9d..0x00423ba1
                                        }
                                        if (FI(0x3c) & 2) {                             // 0x00423ba3 / 0x00423ba6
                                            const int h3 = HoleOf(rec) * 0x208;
                                            const int s = kScanTileLine(G<int>(0x0056a78c), G<int>(0x0056a790),   // 0x00423bd9
                                                                        G<int>(0x00575ac8 + h3), G<int>(0x00575acc + h3), reach, 1, 0);
                                            if (s < m) m = s;                           // 0x00423be1..0x00423be5
                                        }
                                        FI(0x1c) = FI(0x1c) + m / 2;                    // 0x00423be7..0x00423bf4
                                    }
                                }
                            } while (--FI(0x48) != 0);                                  // 0x00423bf8..0x00423c01
                        }
                        // 0x00423c07: accumulate, record the best
                        const int off = FI(0x34);
                        const int a = FI(0xbc + off) + FI(0x1c);                        // 0x00423c0f..0x00423c18
                        FI(0xbc + off) = a;
                        if (FI(0x28) >= Mul(G<int>(0x00822c88) + 4, FI(0x38)) / 8)      // 0x00423c21..0x00423c3d
                            G<int>(0x005a9ce4) = 0;                                     // 0x00423c3f
                        if (a < FI(0x60)) {                                             // 0x00423c49 / 0x00423c4d
                            FI(0xa4) = FI(0x18);                                        // 0x00423c57
                            G<int>(0x005a8830) = G<int>(0x0056a790);                    // 0x00423c63
                            FI(0x78) = FI(0x2c);                                        // 0x00423c6c
                            G<int>(0x005a9ce8) = FI(0x10);                              // 0x00423c76
                            G<int>(0x005a8828) = G<int>(0x0056a78c);                    // 0x00423c82
                            FI(0x6c) = FI(0x14);                                        // 0x00423c8f
                            FI(0x60) = a;                                               // 0x00423c93
                            FI(0xac) = FI(0x536c + off) & 1;                            // 0x00423c7b / 0x00423c97
                        }
                        if (G<int>(0x004c2fc8) == g) {                                  // 0x00423c9e..0x00423cad: debug overlay
                            G<unsigned char>(0x0051a068) = 0;                           // 0x00423cbf
                            const char* s = kItoa(FI(0x1c), reinterpret_cast<char*>(0x0058a528), 10);   // 0x00423cc6
                            char* dst = reinterpret_cast<char*>(0x0051a068);            // strcat, inline 0x00423ccb..0x00423cfe
                            while (*dst) ++dst;
                            do { *dst++ = *s; } while (*s++);
                            kTileToScreen(FI(0x2c), FI(0x18), &FI(0x9c), &FI(0x98));    // 0x00423d0f
                            kSetDrawTarget(G<void*>(0x004c1570), 0, 0x0051b360, 0, 0, 0);   // 0x00423d14..0x00423d28
                            const int col = (FI(0x20) != 0 || FI(0x24) != 0) ? static_cast<int>(0x80007fff)   // 0x00423d2d..0x00423d42
                                                                             : static_cast<int>(0x80007ff0);
                            kDrawCenteredText2(reinterpret_cast<const char*>(0x0051a068), FI(0x9c),   // 0x00423d47..0x00423d6d
                                               FI(0x98) - Shl(FI(0x14), 3) - 4, col);
                        }
                    }
                }
            }
        }
        // 0x00423dc1: prune the cells, then decide whether to run another pass with twice the trials
        if (FI(0x38) >= 8 || G<int>(0x005a9cc4) == 2) break;                            // 0x00423dc1..0x00423dd3
        FI(0x40) = 0x80;                                                                // 0x00423ddd: tolerance
        for (;;) {                                                                      // 0x00423de5
            const int h = HoleOf(rec) * 0x208;                                          // 0x00423de5..0x00423dff
            FI(0x14) = 0xffff;                                                          // 0x00423e04: min B
            const int adx = Sub(Shl(G<int>(0x00575ac8 + h), 10), RI(rec, 0xd4)) + 0x200;   // 0x00423e12..0x00423e28
            const int ady = Sub(Shl(G<int>(0x00575acc + h), 10), RI(rec, 0xd8)) + 0x200;
            FI(0x34) = 0;                                                               // 0x00423e2f: alive slots
            FI(0x10) = -1000;                                                           // 0x00423e33: max B
            FI(0x18) = 0x0fffffff;                                                      // 0x00423e3b: min angle
            FI(0x2c) = static_cast<int>(0xe0000000u);                                   // 0x00423e43: max angle
            FI(0x64) = kAngleFixed(adx, ady);                                           // 0x00423e4b / 0x00423e53
            FI(0x28) = -10;                                                             // 0x00423e57
            FI(0x24) = 0;                                                               // 0x00423e5f
            do {
                for (int col = 0; col - 10 <= 10; ++col) {                              // 0x00423e63 / 0x00423f86..0x00423f8d
                    FI(0x20) = 0;                                                       // 0x00423e69: dead slots
                    const int c = FI(0x24) + col;
                    if (FI(0xc0 + c * 24) > 99999) continue;                            // 0x00423e71..0x00423e81
                    FI(0x50) = 6;                                                       // 0x00423e8b
                    int off = c * 24;
                    do {
                        const int a = FI(0xbc + off);                                   // 0x00423e9e
                        if (a >= 99999) {                                               // 0x00423ea5 / 0x00423eaa
                            ++FI(0x20);
                        } else if (a > FI(0x40) + FI(0x60)) {                           // 0x00423eb5..0x00423ec1
                            FI(0xbc + off) = 99999;                                     // 0x00423ec7
                            ++FI(0x20);
                        } else {
                            ++FI(0x34);                                                 // 0x00423edc..0x00423ee8
                            if (FI(0x38) == 4) {                                        // 0x00423ee5 / 0x00423eec
                                const int b = FI(0x2a14 + off);                         // 0x00423eee
                                if (b != 0) {                                           // 0x00423ef5 / 0x00423ef7
                                    if (b < FI(0x14)) FI(0x14) = b;                     // 0x00423ef9..0x00423eff
                                    if (b > FI(0x10)) FI(0x10) = b;                     // 0x00423f03..0x00423f09
                                    const int td = kTileDistance(RI(rec, 0xd4), RI(rec, 0xd8),   // 0x00423f0d..0x00423f32
                                                                 FI(0x28) + FI(0x54), col + FI(0x8c) - 10);
                                    if (td > 100) {                                     // 0x00423f3a / 0x00423f3d
                                        const int an = Sub(FI(0x536c + off), FI(0x64)); // 0x00423f3f..0x00423f4e
                                        if (an < FI(0x18)) FI(0x18) = an;               // 0x00423f50..0x00423f54
                                        if (an > FI(0x2c)) FI(0x2c) = an;               // 0x00423f58..0x00423f5e
                                    }
                                }
                            }
                        }
                        off += 4;                                                       // 0x00423f66
                    } while (--FI(0x50) != 0);                                          // 0x00423f62..0x00423f6e
                    if (FI(0x20) == 6) FI(0xc0 + c * 24) = 100000;                      // 0x00423f74..0x00423f7b
                }
                ++FI(0x28);                                                             // 0x00423f9e
                FI(0x24) += 21;                                                         // 0x00423f9b
            } while (FI(0x24) <= 0x1a4);                                                // 0x00423f9f / 0x00423fac
            kPumpInput483bd0();                                                         // 0x00423fb2
            const int alive = FI(0x34);                                                 // 0x00423fb7
            if (alive > 4 && FI(0x40) > 16 &&                                           // 0x00423fbb..0x00423fe0
                Add(Mul(Mul(alive, FI(0x38)), 3), FI(0x44)) > 0xfa) {
                FI(0x40) = FI(0x40) / 2;                                                // 0x00423fe2..0x00423fe9
                continue;                                                               // 0x00423fed
            }
            break;
        }
        if (FI(0x38) == 4) {                                                            // 0x00423ff6 / 0x00423ff9
            G<int>(0x005a7140) = 0;                                                     // 0x00424005
            if (FI(0x10) - FI(0x14) > 0x4b)                                             // 0x00423ffb..0x00424012
                G<int>(0x005a7140) = 1;                                                 // 0x00424014
            else if (Sub(FI(0x2c), FI(0x18)) > 0x15555555)                              // 0x00424020..0x0042402f
                G<int>(0x005a7140) = 2;                                                 // 0x00424031
        }
        FI(0x38) = FI(0x38) * 2;                                                        // 0x0042403b / 0x00424041
        if (FI(0x34) <= 1) break;                                                       // 0x0042403e / 0x00424045
    }
    if (G<int>(0x004c2fc8) == g) {                                                      // 0x0042404b..0x0042405e
        kRefreshFull(reinterpret_cast<void*>(0x00519a60), 0, 0);                        // 0x00424060..0x00424067
        kPumpAndRedraw(0);                                                              // 0x0042406c / 0x0042406e
    }
    int ret;
    if (FI(0x78) != -1) {                                                               // 0x00424076..0x0042407d
        RI(rec, 0xcc) = FI(0x78);                                                       // 0x00424083
        RI(rec, 0xd0) = FI(0xa4);                                                       // 0x00424090
        if (FI(0xac) != 0) G<int>(0x005a7140) |= 4;                                     // 0x00424096..0x004240a8
        ret = FI(0x6c);                                                                 // 0x004240ad
    } else {
        G<int>(0x005a9ce8) = 0;                                                         // 0x004240f1
        RI(rec, 0xcc) = G<int>(0x00575ac8 + FI(0x80));                                  // 0x004240fb / 0x00424107
        RI(rec, 0xd0) = G<int>(0x00575acc + FI(0x80));                                  // 0x00424101 / 0x0042410d
        ret = 0;                                                                        // 0x00424113
    }
    G<int>(0x0059e7b8) &= ~0x800000;                                                    // 0x004240b1..0x004240c6
    if (G<int>(0x005a9cc4) != 2) G<int>(0x005a9cc4) = 0;                                // 0x004240c3..0x004240ce
    G<int>(0x004c2e1c) = 7;                                                             // 0x004240d8
    return ret;
}

// The original's prologue (0x00422fb0..0x00422fbd) and epilogue (0x00424058..0x004240e8) around the body.
__declspec(naked) int __cdecl SimulateBallFlight_re(int) {
    __asm {
        mov eax, 0x7cb4
        mov ecx, 0x004a6070
        call ecx
        push ebx
        push ebp
        push esi
        push edi
        mov eax, esp
        push eax
        call SimulateBallFlightBody
        add esp, 4
        pop edi
        pop esi
        pop ebp
        pop ebx
        add esp, 0x7cb4
        ret
    }
}
C1_t SimulateBallFlight_orig;
// 0x00422fb0
SG_HOOK("golf_clean.exe", 0x00422fb0, simulateBallFlight, SimulateBallFlight_re, SimulateBallFlight_orig);

// ====================================================================================================================
// 0x00424120  simulateShot(g, mode, x, y, aim): sets up golfer g's shot (direction, power, height, spin) and, for a real
// shot (x == -1), its thoughts. cdecl, plain `ret` (0x00426631); eax = the flag word after bit 0 is cleared (0x00426611).
// ====================================================================================================================
// Frame: sub esp, 0xb0c (0x00424120) and the pushes ebx, ebp, esi, edi, so F = esp after them; the arguments are at
// [F+0xb20] g, [F+0xb24] mode, [F+0xb28] x, [F+0xb2c] y, [F+0xb30] aim (the aim slot is rewritten). counts[32] at F+0x58
// (zeroed at 0x004246f3), dirs[32] at F+0xd8 (never zeroed), and a 2500-byte array at F+0x158 that is only read.
static int __cdecl SimulateShotBody(unsigned char* F) {
    const int g = FI(0xb20);
    unsigned char* rec = Rec(g);
    const int hole0 = HoleOf(rec);                                                      // 0x00424137
    FI(0x30) = kRateTile(g);                                                            // 0x0042413e / 0x00424143: reach
    if (RB(rec, 0x10) & 1) {                                                            // 0x00424147..0x00424152
        G<unsigned char>(0x005786a2) = 0x20;                                            // 0x00424154: type 0x11 +2
        G<unsigned char>(0x00578732) = 0x20;                                            // 0x0042415b: type 0x14 +2
    }
    G<int>(0x004c2fcc) = -1;                                                            // 0x00424167
    if ((G<unsigned char>(0x0059e7b8) & 0x20) && G<int>(0x0053df54) > 0)                // 0x00424162..0x0042417c
        G<int>(0x004c2fcc) = G<int>(0x0053df54) - 1;                                    // 0x0042417e / 0x0042417f
    FI(0x34) = RI(rec, 0xd4) >> 10;                                                     // 0x00424190 / 0x00424193: ball tile x
    FI(0x40) = RI(rec, 0xd8) >> 10;                                                     // 0x0042419a / 0x004241a0: ball tile y
    FI(0x50) = FI(0x34) * 50 + FI(0x40);                                                // 0x004241a4 / 0x004241a7
    FI(0x14) = Tile(FI(0x50));                                                          // 0x004241ab / 0x004241b9: lie type
    if (FI(0xb24) == 0) {                                                               // 0x004241b2..0x004241bf
        RI(rec, 0xcc) = G<int>(0x00575ac8 + hole0 * 0x208);                             // 0x004241cb / 0x004241d7
        RI(rec, 0xd0) = G<int>(0x00575acc + hole0 * 0x208);                             // 0x004241d1 / 0x004241dd
    } else {
        const int m = G<int>(0x0058f330);                                               // 0x004241e5
        FI(0xb30) = 0;                                                                  // 0x004241f0
        if (m <= 1) FI(0xb30) = m;                                                      // 0x004241ed..0x00424205
    }
    int ex = Sub(Shl(RI(rec, 0xcc), 10), RI(rec, 0xd4)) + 0x200;                        // 0x0042420c..0x00424235
    int ey = Sub(Shl(RI(rec, 0xd0), 10), RI(rec, 0xd8)) + 0x200;
    if (FI(0xb28) != -1) {                                                              // 0x00424241 / 0x00424244
        ex = Sub(FI(0xb28), RI(rec, 0xd4));                                             // 0x00424246 / 0x00424248
        ey = Sub(FI(0xb2c), RI(rec, 0xd8));                                             // 0x0042424a..0x00424253
        RI(rec, 0xcc) = FI(0xb28) >> 10;                                                // 0x00424255 / 0x0042425b
        RI(rec, 0xd0) = FI(0xb2c) >> 10;                                                // 0x00424258 / 0x00424261
    }
    {
        const int sx = Mul(ex, 25) >> 10, sy = Mul(ey, 25) >> 10;                       // 0x00424267..0x00424278
        FI(0x10) = SqrtFtol(Add(Mul(sy, sy), Mul(sx, sx)));                             // 0x0042427b..0x00424298: distance
    }
    RI(rec, 0xe0) = kAngleFixed(ex, ey);                                                // 0x0042429c / 0x004242a8
    RI(rec, 0x10) &= 0xefffffff;                                                        // 0x004242ae..0x004242c6
    const int oct = (((RI(rec, 0xe0) >> 28) + 1) >> 1) & 7;                             // 0x004242b7..0x004242c3
    if (FI(0xb24) == 0 && FI(0xb28) == -1) {                                            // 0x004242cc..0x004242de
        const int thr = ((RI(rec, 0x10) & 1) || (RB(rec, 0x19) & 4)) ? 0x19 : 0x4b;    // 0x004242e4..0x004242f7
        if (FI(0x10) > thr && FI(0x14) != 1) {                                          // 0x004242fc..0x00424309: aim search
            const int len = (Shl(kClamp(FI(0x30) - 25, 0, FI(0x10)), 10) + 0x200) / 25;   // 0x0042430f..0x00424340
            const int px = (kFoldRange(RI(rec, 0xe0), len) + RI(rec, 0xd4)) >> 10;     // 0x00424344..0x00424361
            const int y0 = RI(rec, 0xd8);                                               // 0x0042434f
            const int py = (y0 - kAbsDiff(RI(rec, 0xe0), len)) >> 10;                   // 0x00424364..0x00424373
            kTileDistance(RI(rec, 0xd4), y0, px, py);                                   // 0x0042437a (result unused)
            kDistance(px - G<int>(0x00575ac8 + HoleOf(rec) * 0x208),                     // 0x0042437f..0x004243a2 (unused)
                      py - G<int>(0x00575acc + HoleOf(rec) * 0x208));
            FI(0xb30) = kSimulateBallFlight(g);                                         // 0x004243af / 0x004243b4
            unsigned fl;
            if (G<int>(0x005a9ce8) == 0) {                                              // 0x004243bb..0x004243cb
                FI(0x38) = Sub(Shl(RI(rec, 0xcc), 10), RI(rec, 0xd4)) + 0x200;          // 0x004243cd..0x004243fa
                FI(0x28) = Sub(Shl(RI(rec, 0xd0), 10), RI(rec, 0xd8)) + 0x200;          // 0x004243fe
                fl = static_cast<unsigned>(RI(rec, 0x10)) & 0xefffffff;                 // 0x00424402
            } else {
                FI(0x38) = Sub(Shl(RI(rec, 0xcc), 10), RI(rec, 0xd4));                  // 0x0042440a..0x0042442c
                FI(0x28) = Sub(Shl(RI(rec, 0xd0), 10), RI(rec, 0xd8));                  // 0x00424430
                fl = static_cast<unsigned>(RI(rec, 0x10)) | 0x10000000;                 // 0x00424434
            }
            RI(rec, 0x10) = static_cast<int>(fl);                                       // 0x0042443a
            // 0x00424440..0x004244dd (rec+0x19 bit 4): a minimum over the stack array at F+0x158, which is never written,
            // into [F+0x1c]/[F+0x24]; both are written again (0x0042450f, 0x00424720) before any read: not reproduced.
            const int qx = Mul(FI(0x38), 25) >> 10, qy = Mul(FI(0x28), 25) >> 10;      // 0x004244e7..0x004244f6
            RI(rec, 0xe0) = kAngleFixed(qx, qy);                                        // 0x004244fb / 0x00424513
            FI(0x24) = Add(Mul(qy, qy), Mul(qx, qx));                                   // 0x00424500..0x0042450f
            FI(0x10) = SqrtFtol(FI(0x24));                                              // 0x00424519..0x0042452b
            if (G<int>(0x004c2fcc) == g) {                                              // 0x0042452f..0x00424536
                kRefreshFull(reinterpret_cast<void*>(0x00519a60), 0, 0);                // 0x0042453c..0x00424543
                kPumpAndRedraw(0);                                                      // 0x00424548 / 0x0042454a
            }
        } else {                                                                        // 0x00424557
            const int tX = RI(rec, 0xcc), tY = RI(rec, 0xd0);
            G<int>(0x005a7140) = 0;                                                     // 0x00424565
            G<int>(0x005a8828) = Shl(tX, 10) + 0x200;                                   // 0x00424578
            G<int>(0x005a8830) = Shl(tY, 10) + 0x200;                                   // 0x00424589
            if (FI(0x14) != 1 && FI(0x10) > 0x19) {                                     // 0x0042458f..0x0042459f: green slope
                const int o1 = oct & 6, o2 = (oct + 1) & 6;                             // 0x004245a5..0x004245ae
                const int s = TypeS(Tile((tX - DirX(o1)) * 50 - DirY(o1) + tY), 2)      // 0x004245b1..0x00424673
                            + TypeS(Tile((tX - DirX(o2)) * 50 - DirY(o2) + tY), 2)
                            - TypeS(Tile((DirX(o1) + tX) * 50 + DirY(o1) + tY), 2)
                            - TypeS(Tile((tX + DirX(o2)) * 50 + DirY(o2) + tY), 2);
                if (s >= 4) {                                                           // 0x00424675 / 0x00424678
                    const int k = TypeS(FI(0x14), 2) <= 0 ? 6 : 0;                      // 0x0042467a..0x004246a5
                    FI(0x10) = FI(0x10) + kClamp(FI(0x10) / 4 - k, 0, 12);              // 0x004246a8..0x004246b8
                }
                if (s <= -4) FI(0x10) -= 6;                                             // 0x004246bc..0x004246c1
            }
        }
    }
    // 0x004246c6: walk the line to the target, scoring its tiles
    FI(0x24) = (((RI(rec, 0xe0) >> 28) + 1) >> 1) & 7;                                  // 0x004246c6..0x004246e5
    for (int k = 0; k < 32; ++k) FI(0x58 + 4 * k) = 0;                                  // rep stosd 0x004246f3
    int acc = 4 - (g & 3);                                                              // 0x004246eb..0x004246fa
    FI(0x2c) = FI(0x10) / 25;                                                           // 0x004246f5..0x00424724: steps
    FI(0x20) = 0;                                                                       // 0x00424710: rough count
    FI(0x48) = 0;                                                                       // 0x00424714: type with flag 0x100
    FI(0x38) = 0;                                                                       // 0x00424718
    FI(0x28) = 0;                                                                       // 0x0042471c: first water step
    FI(0x1c) = 0;                                                                       // 0x00424720
    if (FI(0x2c) > 0) {                                                                 // 0x0042470e / 0x00424728
        int i = 0;                                                                      // edi 0x0042470c
        FI(0x18) = 0x200;                                                               // 0x0042472e
        do {
            const int px = (kFoldRange(RI(rec, 0xe0), FI(0x18)) + RI(rec, 0xd4)) >> 10; // 0x00424736..0x0042475b
            FI(0x44) = px;                                                              // 0x00424760
            const int py = (RI(rec, 0xd8) - kAbsDiff(RI(rec, 0xe0), FI(0x18))) >> 10;   // 0x00424764..0x00424773
            FI(0x3c) = py;                                                              // 0x00424778
            if (kTileBlocked(px, py)) break;                                            // 0x0042477c..0x00424786
            const int idx = px * 50 + py;                                               // 0x0042478c..0x0042479a
            FI(0x54) = idx;                                                             // 0x0042479d
            const int t = Tile(idx);                                                    // 0x004247a1
            const int b2 = TypeS(t, 2);                                                 // 0x004247ae
            acc = Add(acc, Shl(Mul(i, b2), 1));                                         // 0x004247b5 / 0x004247b8
            FI(0x4c) = i + 1;                                                           // 0x004247bb / 0x004247be
            FI(0x58 + 4 * t) = Add(FI(0x58 + 4 * t), Shl(Mul(i + 1, b2), 1));           // 0x004247c2..0x004247cd
            FI(0xd8 + 4 * t) = FI(0x24) ^ 4;                                            // 0x004247d1..0x004247d8
            if (TileByte(idx) == 0x11 && FI(0x28) == 0) FI(0x28) = i;                   // 0x004247df..0x004247f4
            if (FI(0x18) != 0x200 && TypeU(t, 6) == 0xd) ++FI(0x20);                    // 0x004247f8..0x0042480b
            if (TileFlags(idx) & 0x100) FI(0x48) = t;                                   // 0x0042480f..0x00424822
            int hA, hB;
            if (i > FI(0x2c) / 2) {                                                     // 0x00424826..0x00424833
                hA = kHeightBlend(RI(rec, 0xcc), RI(rec, 0xd0), 0) + 1;                 // 0x00424861..0x00424882
                hB = kHeightBlend(FI(0x44), FI(0x3c), 0);                               // 0x00424883
            } else {
                hA = kHeightBlend(FI(0x34), FI(0x40), 0) + 1;                           // 0x00424835..0x00424852
                hB = kHeightBlend(FI(0x44), FI(0x3c), 0);                               // 0x00424853
            }
            if (hB > hA) FI(0x20) += 2;                                                 // 0x0042485b / 0x0042488b / 0x0042488f
            const int off = (RandRange(2) & 0xffff) ? 0x0aaaaaaa : static_cast<int>(0xf5555556u);   // 0x00424894..0x004248b9
            const int qx = (kFoldRange(Add(RI(rec, 0xe0), off), FI(0x18)) + RI(rec, 0xd4)) >> 10;   // 0x004248bf..0x004248df
            const int qy = (RI(rec, 0xd8) - kAbsDiff(Add(RI(rec, 0xe0), off), FI(0x18))) >> 10;      // 0x004248e2..0x004248f5
            acc = Add(acc, Mul(TypeS(Tile(qx * 50 + qy), 2), FI(0x4c)));                // 0x004248fc..0x0042491f
            i = FI(0x4c);
            FI(0x18) += 0x400;                                                          // 0x00424921..0x0042492d
        } while (i < FI(0x2c));                                                         // 0x0042492b / 0x00424931
    }
    for (FI(0x18) = 0; FI(0x18) < 8; ++FI(0x18)) {                                      // 0x00424937..0x004249c4: around the target
        const int k = FI(0x18);
        const int nx = DirX(k) + RI(rec, 0xcc), ny = DirY(k) + RI(rec, 0xd0);           // 0x00424943..0x0042495f
        if (kTileBlocked(nx, ny)) {                                                     // 0x00424963..0x0042496d
            acc = Add(acc, Add(Shl(FI(0x2c), 2), 4));                                   // 0x0042496f / 0x00424973
        } else {
            const int t = Tile(nx * 50 + ny);                                           // 0x0042497f
            const int v = Mul(TypeS(t, 2), FI(0x2c) + 1) / 2;                           // 0x0042498d..0x004249a3
            acc = Add(acc, v);                                                          // 0x004249a5
            FI(0x58 + 4 * t) = Add(FI(0x58 + 4 * t), v);                                // 0x004249a7 / 0x004249ad
            FI(0xd8 + 4 * t) = k;                                                       // 0x004249b1
        }
    }
    {
        int best = -1;                                                                  // 0x004249ca
        for (int t = 0; t < 32; ++t)                                                    // 0x004249cf..0x004249ec
            if (FI(0x58 + 4 * t) > best) { best = FI(0x58 + 4 * t); FI(0x3c) = t; FI(0x4c) = FI(0xd8 + 4 * t); }
    }
    {
        int dif = acc / (FI(0x2c) + 3);                                                 // 0x004249ee..0x004249f8
        if (FI(0x20) >= 2) dif += 4;                                                    // 0x004249fa..0x00424a01
        if (TileByte((RI(rec, 0xd4) >> 10) * 50 + (RI(rec, 0xd8) >> 10)) == 1) dif = 0;   // 0x00424a04..0x00424a26
        if (FI(0x10) <= 0x28 && dif > 10) dif = 10;                                     // 0x00424a28..0x00424a36
        RB(rec, 0x1b) = static_cast<unsigned char>(dif);                                // 0x00424a3b
    }
    // 0x00424a41: distance, lie, power, height
    int d = FI(0x10);                                                                   // ebx 0x00424a28
    if (RB(rec, 0x19) & 4) {                                                            // 0x00424a41..0x00424a49
        const int h1 = kHeightBlend(RI(rec, 0xcc), RI(rec, 0xd0), 0);                   // 0x00424a5b
        const int h2 = kHeightBlend(FI(0x34), FI(0x40), 0);                             // 0x00424a6e
        const int dd = h1 - h2;                                                         // 0x00424a73
        if (dd > 0) d += Mul(dd, 25) / 8;                                               // 0x00424a78..0x00424a8b
        else d += Mul(dd, 25) / 10;                                                     // 0x00424a8f..0x00424aa6
    }
    const int reach = FI(0x30);                                                         // 0x00424aa8
    int cd = kClamp(d, 0, reach);                                                       // 0x00424ab0 / 0x00424ab5
    RI(rec, 0xe4) = cd;                                                                 // 0x00424ac3
    FI(0x10) = cd;                                                                      // 0x00424ace
    {
        const signed char q = static_cast<signed char>(Mul(reach - cd, 60) / Mul(reach, 3));   // 0x00424ab7..0x00424ad9
        RB(rec, 0x1c) = static_cast<unsigned char>(q);                                  // 0x00424ae3
        RB(rec, 0x1c) = static_cast<unsigned char>(kClamp(q, FI(0x14) != 0 ? 1 : 0, 0xb));   // 0x00424adb..0x00424afe
    }
    if (FI(0xb24) != 0 && G<int>(0x0058f330) == 3 && static_cast<signed char>(RB(rec, 0x1c)) < 5) {   // 0x00424afc..0x00424b11
        cd = Mul(reach, 13) / 18;                                                       // 0x00424b13..0x00424b2c
        RI(rec, 0xe4) = cd;                                                             // 0x00424b2e
        FI(0x10) = cd;                                                                  // 0x00424b34
        RB(rec, 0x1c) = 5;                                                              // 0x00424b38
    }
    if (FI(0x14) == 1 && cd < 0x32 && !(RB(rec, 0x10) & 1)) RB(rec, 0x1c) = 0xd;       // 0x00424b3f..0x00424b51: putt
    {
        const int s2 = Mul(RI(rec, 0xe4), 20) / 25;                                     // 0x00424b58..0x00424b7a
        const int sp = Mul(s2, 33) - Mul(s2, s2) / 48 + 0x40;                           // 0x00424b7c..0x00424b9d
        RI(rec, 0xe4) = sp;                                                             // 0x00424ba0
        const int vz = sp / 8 + 0x200;                                                  // 0x00424ba6..0x00424baf
        RI(rec, 0xe8) = vz;                                                             // 0x00424bb4
        RI(rec, 0xe4) = kShotPower(Mul(cd, 4) / 5, vz, 0);                              // 0x00424b94..0x00424bdd
    }
    int acu = (G<unsigned char>(0x005a47e0) & 2) ? 12 : 20;                             // 0x00424bd1..0x00424bf0
    if ((RB(rec, 0x19) & 4) && G<int>(0x00543cb8) != 0 && (RB(rec, 0x18) & 0xe0) != 0x20)   // 0x00424be3..0x00424c0c
        acu += acu / (4 - G<int>(0x00543cb8));                                          // 0x00424c0e..0x00424c1a
    if (RB(rec, 0x16) & 0x10) acu += Mul(RB(rec, 0xf4), acu) / 8;                       // 0x00424c1c..0x00424c39
    {
        const int h = HoleOf(rec) * 0x208;                                              // 0x00424c3b..0x00424c49
        if (G<unsigned char>(0x0056988c + G<int>(0x00575ac8 + h) * 50 + G<int>(0x00575acc + h)) & 0x80)   // 0x00424c4c..0x00424c66
            acu -= 10;                                                                  // 0x00424c68
    }
    if (RS(rec, 0x36) < 2) acu /= 2;                                                    // 0x00424c6b..0x00424c7b
    const int halfAcu = acu / 2;                                                        // 0x00424c7d..0x00424c89
    const int err = (RandRange(halfAcu) & 0xffff) + halfAcu + 4;                        // 0x00424c8b..0x00424caa (ebp)
    FI(0x2c) = kShotPower(Mul(reach, 4) / 5, RI(rec, 0xe8), 0);                         // 0x00424c91..0x00424cc4: max power
    RI(rec, 0xec) = 0;                                                                  // 0x00424cd3
    if (G<int>(0x005a9cc4) < 2) RI(rec, 0xec) = Shl(Mul(kWobble(), 5), 16);             // 0x00424cd0..0x00424cea
    if (G<int>(0x005a9cc4) == 1) RI(rec, 0xec) = RI(rec, 0xec) / 3;                     // 0x00424cf0..0x00424d0d
    if (RS(rec, 0x36) < 0) {                                                            // 0x00424d13..0x00424d1b
        const int sp = RI(rec, 0xec);                                                   // 0x00424d1d
        const int c = kClamp(-RS(rec, 0x36), 0, 3);                                     // 0x00424d23..0x00424d2d
        RI(rec, 0xec) = Mul(c, sp) / (FI(0x14) != 1 ? 3 : 8) + sp;                      // 0x00424d32..0x00424d4c
    }
    const int lvl = G<int>(0x00822c88);                                                 // 0x00424d56
    if (FI(0x14) != 1 && (RB(rec, 0x18) & 0xe0) != 0x20 &&                              // 0x00424d52..0x00424d7f
        ((RB(rec, 0x19) & 2) || (RB(rec, 0x10) & 1))) {
        const int v = RI(rec, 0xec) / (G<int>(0x00543cc0) + 2);                         // 0x00424d81..0x00424d97
        RI(rec, 0xec) = v;
        if (lvl < 2) RI(rec, 0xec) = v - v / (lvl + 2);                                 // 0x00424d92..0x00424da7
    }
    if ((RI(rec, 0x10) & 0x4000000) && (RB(rec, 0x18) & 0xe0) != 0x20)                  // 0x00424dad..0x00424dc5
        RI(rec, 0xec) = RI(rec, 0xec) - RI(rec, 0xec) / 3;                              // 0x00424dc7..0x00424ddd
    RI(rec, 0x10) &= 0xffbfffff;                                                        // 0x00424de3..0x00424df8
    FI(0x18) = -3;                                                                      // 0x00424de9 / 0x00424df4: thought bias
    if (RB(rec, 0x1c) == 0xd) {                                                         // 0x00424dfe..0x00424e06: a putt
        RI(rec, 0xec) = 0;                                                              // 0x00424e12
        const int k = (G<int>(0x0059e7b8) & 0x200000) ? 2 : 1;                          // 0x00424e0c..0x00424e28
        if (Mul(k, FI(0x10)) > err) {                                                   // 0x00424e29..0x00424e30
            const int r = RandRange(2) & 0xffff;                                        // 0x00424e36..0x00424e42
            if (r == 0)                                                                 // 0x00424e47 / 0x00424e4a
                RI(rec, 0xec) = Shl((RandRange(0x96) & 0xffff) + 0x96, 18);             // 0x00424e75..0x00424e91
            else if (r == 1)                                                            // 0x00424e4c / 0x00424e4d
                RI(rec, 0xec) = Shl(-0x96 - (RandRange(0x96) & 0xffff), 18);            // 0x00424e4f..0x00424e6d
            const int dd = FI(0x10);                                                    // 0x00424e97
            if (dd <= 5) RI(rec, 0xec) = 0;                                             // 0x00424e9b..0x00424ea0
            if (dd > 15) RI(rec, 0xec) = RI(rec, 0xec) / 2;                             // 0x00424eaa..0x00424eba
            if (dd > 25) RI(rec, 0xec) = RI(rec, 0xec) / 2;                             // 0x00424ec0..0x00424ed0
            if (dd > 35) RI(rec, 0xec) = RI(rec, 0xec) / 2;                             // 0x00424ed6..0x00424ee6
        }
    } else {
        int bias = -3;                                                                  // edi 0x00424de9
        if (RB(rec, 0x18) != 0) {                                                       // 0x00424f0d..0x00424f15
            if (g == G<int>(0x005a59f8)) {                                              // 0x00424f1b..0x00424f2a
                RI(rec, 0xec) = RI(rec, 0xec) - Mul(3 - lvl, RI(rec, 0xec)) / 6;        // 0x00424f5e..0x00424f7e
            } else if ((RB(rec, 0x18) & 0xe0) == 0x20) {                                // 0x00424f2c..0x00424f30
                RI(rec, 0xec) = Mul(3 - lvl, RI(rec, 0xec)) / 3 + RI(rec, 0xec);        // 0x00424f32..0x00424f56
            } else {
                bias = RS(rec, 0x36) - 3;                                               // 0x00424f86..0x00424f90
                FI(0x18) = bias;
            }
            if (RS(rec, 0x1c) > 3) {                                                    // 0x00424f94 / 0x00424f9b
                RI(rec, 0xec) = Mul(RI(rec, 0xec), 6) / (RB(rec, 0xf3) + 4);            // 0x00424f9d..0x00424fb8
                bias += RB(rec, 0xf3);                                                  // 0x00424fbe / 0x00424ff9
                FI(0x18) = bias;                                                        // 0x00424ffb
            } else if (RB(rec, 0x22) == 0) {                                            // 0x00424fc6..0x00424fce
                RI(rec, 0xec) = Mul(RI(rec, 0xec), 6) / (RB(rec, 0xf2) + 4);            // 0x00424fd0..0x00424fed
                bias += RB(rec, 0xf2);                                                  // 0x00424ff3 / 0x00424ff9
                FI(0x18) = bias;
            }
            if (FI(0xb30) == 1) {                                                       // 0x00424fff..0x00425009
                RI(rec, 0xec) = Mul(RI(rec, 0xec), 6) / (RB(rec, 0xf5) + 3);            // 0x0042500b..0x00425028
                bias += RB(rec, 0xf5);                                                  // 0x0042502e / 0x00425034
                FI(0x18) = bias;                                                        // 0x0042509e
            } else if (FI(0xb30) == -1) {                                               // 0x00425038 / 0x0042503b
                RI(rec, 0xec) = Mul(RI(rec, 0xec), 6) / (RB(rec, 0xf6) + 3);            // 0x0042503d..0x0042505a
                bias += RB(rec, 0xf6);                                                  // 0x00425060 / 0x00425066
                FI(0x18) = bias;
            } else if (FI(0xb30) == 0 && G<int>(0x0058f330) != 3) {                     // 0x0042506a..0x00425075
                RI(rec, 0xec) = Mul(RI(rec, 0xec), 6) / 7;                              // 0x00425077..0x00425098
                bias += 3;                                                              // 0x00425095
                FI(0x18) = bias;
            }
        }
        if (!(G<int>(0x0059e7b8) & 0x800000) && FI(0x10) > 0x4b) {                      // 0x004250a2..0x004250b7: a mishit roll
            const int r = RandRange(RI(rec, 0xe4)) & 0xffff;                            // 0x004250bd..0x004250da
            const int sp = RI(rec, 0xec);
            const int a = sp < 0 ? Sub(0, sp) : sp;                                     // 0x004250d7..0x004250e0
            if (r > (a >> 9) + 0x200) {                                                 // 0x004250e2..0x004250ec
                RI(rec, 0x10) |= 0x400000;                                              // 0x004250f2..0x00425106
                bias += 4;                                                              // 0x00425103
                FI(0x18) = bias;                                                        // 0x00425115
                if (g != G<int>(0x005a59f8)) RI(rec, 0xec) = RI(rec, 0xec) / 2;         // 0x00425119 -> 0x00424edb
                else RI(rec, 0xec) = 0;                                                 // 0x0042511f
            }
        }
    }
    // 0x00424eec: aim offsets and curve
    int dir = RI(rec, 0xe0);                                                            // ebx 0x00424ef3
    if (FI(0xb30) == 1) dir = Add(dir, 0x15555554);                                     // 0x00424ef9..0x00424f02
    else if (FI(0xb30) == -1) dir = Sub(dir, 0x15555554);                               // 0x0042512e..0x00425133
    if (RB(rec, 0x1c) != 0xd) {                                                         // 0x00425139 / 0x00425140
        const int c = kClamp(RI(rec, 0xec), static_cast<int>(0xfc71c71du), 0x38e38e3);  // 0x00425146..0x00425157
        RI(rec, 0xe0) = Add(RI(rec, 0xe0), Mul(c, 3));                                  // 0x0042515c..0x0042516a
        RI(rec, 0xec) = RI(rec, 0xec) - c / 2;                                          // 0x00425170..0x00425186
        if (G<int>(0x005a9cc4) == 0 && (RB(rec, 0x18) & 0xe0) == 0x20) {                // 0x0042517f..0x0042519a
            const int sp = RI(rec, 0xec);                                               // ecx 0x00425170
            RI(rec, 0xec) = Mul(FI(0x10) - 100, sp) / 256 + sp;                         // 0x0042519c..0x004251be
            const int r = RandRange(0x71c6) & 0xffff;                                   // 0x004251a0..0x004251d1
            RI(rec, 0xec) = Add(RI(rec, 0xec), (r - 0x38e38e3) / 50);                   // 0x004251d7..0x004251f1
        }
    }
    RI(rec, 0x10) &= 0xfffffe7f;                                                        // 0x004251f7..0x00425203
    RW(rec, 0xac) = 0;                                                                  // 0x00425209
    const int stopAt = ((RI(rec, 0x10) & 1) && FI(0xb24) == 0) ? 1 : FI(0x28);          // 0x00425212..0x0042522c
    bool chip = false;
    if (((RB(rec, 0x19) & 4) || (RI(rec, 0x10) & 2)) && FI(0x14) != 1 && stopAt != 1) {   // 0x00425230..0x0042524f
        if (FI(0xb24) != 0) {                                                           // 0x00425255..0x0042525e
            chip = G<int>(0x0058f330) == 4;                                             // 0x00425260..0x00425267
        } else {
            const int py = (RI(rec, 0xd8) - kAbsDiff(dir, 0x80)) >> 10;                 // 0x00425272..0x0042528b
            const int px = (kFoldRange(dir, 0x80) + RI(rec, 0xd4)) >> 10;               // 0x0042528e..0x0042529e
            if (TypeU(Tile(px * 50 + py), 6) == 0xd) {                                  // 0x004252a1..0x004252bc
                chip = true;
            } else {
                const int py2 = (RI(rec, 0xd8) - kAbsDiff(dir, 0x400)) >> 10;           // 0x004252be..0x004252d7
                const int px2 = (kFoldRange(dir, 0x400) + RI(rec, 0xd4)) >> 10;         // 0x004252da..0x004252ea
                chip = TypeU(Tile(px2 * 50 + py2), 6) == 0xd;                           // 0x004252ed..0x00425308
            }
        }
    }
    if (chip) {                                                                         // 0x0042530e: a chip out
        RI(rec, 0xe8) = kClamp(RI(rec, 0xe4) / 12, 0, 0x100);                           // 0x0042530e..0x00425337
        RI(rec, 0xe4) = Add(Add(Mul(reach, 33) - Mul(reach, reach) / 48, RI(rec, 0xe4)), 0x40) / 2;   // 0x0042531d..0x0042536b
        RI(rec, 0xe4) = kShotPower(Mul(FI(0x10), 3) / 4, RI(rec, 0xe8), 0);             // 0x00425371..0x00425391
        if (stopAt != 0) RI(rec, 0xe4) = kShotPower(Mul(stopAt, 50) / 3, RI(rec, 0xe8), 0);   // 0x00425397..0x004253c3
        FI(0xb30) = 0;                                                                  // 0x004253c9
        RW(rec, 0xac) = 4;                                                              // 0x004253d4
    } else if ((RB(rec, 0x19) & 4) && FI(0xb30) == 0 && FI(0x14) != 1 &&                // 0x004253e2..0x00425433: a lob
               TypeS(FI(0x14), 2) == 0 && RS(rec, 0x1c) >= 4 && FI(0x10) > 0x19) {
        const bool go = FI(0xb24) == 0 ? TileByte(RI(rec, 0xcc) * 50 + RI(rec, 0xd0)) == 1   // 0x00425439..0x0042545e
                                       : G<int>(0x0058f330) == 3;                       // 0x00425466 / 0x0042546d
        if (go) {
            RI(rec, 0x10) |= 0x80;                                                      // 0x0042546f..0x0042547c
            const int vz = RI(rec, 0xe8);                                               // 0x00425482
            RI(rec, 0xe8) = Mul(FI(0x10) + 0x32, vz) / 400 + vz;                        // 0x00425475..0x004254a0
            RI(rec, 0xe4) = kShotPower(FI(0x10), RI(rec, 0xe8), 0);                     // 0x004254a6 / 0x004254ab
            RW(rec, 0xac) = 3;                                                          // 0x004254b1
            RI(rec, 0xec) = Mul(RI(rec, 0xec), 6) / (RB(rec, 0xf7) + 3);                // 0x004254ba..0x004254d8
            FI(0x18) += RB(rec, 0xf7);                                                  // 0x004254de / 0x004254e4
        }
    }
    FI(0x30) = RB(rec, 0x7a);                                                           // 0x004254e8..0x004254fc
    int e7 = FI(0x30);                                                                  // ebp
    if (FI(0xb28) == -1) {                                                              // 0x004254f9 / 0x00425500: a real shot
        if (RB(rec, 0x1c) == 0xd) {                                                     // 0x00425506 / 0x0042550d
            if (TileByte(RI(rec, 0xcc) * 50 + RI(rec, 0xd0)) == 1)                      // 0x004257a6..0x004257c0
                RI(rec, 0xe4) = kShotPower(FI(0x10) + 2, 0, 1);                         // 0x004257c2..0x004257d6
        } else {
            FI(0x20) = 0;                                                               // 0x00425523
            G<int>(0x00575ad4 + HoleOf(rec) * 0x208) += 1;                              // 0x00425513..0x0042553a
            if (Shl(kHeightBlend(FI(0x34), FI(0x40), 0), 4) / (G<int>(0x00822c88) + 2) > 0) {   // 0x0042553c..0x0042555f
                do {
                    int dr = RI(rec, 0xe0);                                             // 0x00425565
                    dr = Add(dr, Shl(0xc - (RandRange(0x19) & 0xffff), 24));            // 0x0042556b..0x00425590
                    const int L = FI(0x10) - (RandRange(0xc8) & 0xffff) + 0x64;         // 0x00425583..0x004255a2
                    if (L >= 0) {                                                       // 0x004255a5
                        const int st = Shl(L / 25, 10);                                 // 0x004255ab..0x004255be
                        const int px = (kFoldRange(dr, st) + RI(rec, 0x00)) >> 10;      // 0x004255c3..0x004255d4
                        const int py = (RI(rec, 0x04) - kAbsDiff(dr, st)) >> 10;        // 0x004255d7..0x004255e7
                        const int o3 = (((dr >> 28) + 1) >> 1) & 7;                     // 0x004255e4..0x004255ee
                        FI(0x44) = o3;                                                  // 0x004255f2
                        if (!kTileBlocked(px, py)) {                                    // 0x004255f6..0x00425600
                            const int idx = px * 50 + py;                               // 0x00425606..0x0042560d
                            FI(0x28) = idx;                                             // 0x00425610
                            const int back = (px - DirX(o3)) * 50 - DirY(o3) + py;      // the cell behind, toward the ball
                            if ((TileFlags(idx) & 0x100) || TileByte(idx) == 0x13) {    // 0x00425614..0x0042562e
                                const int h1 = kHeightBlend(FI(0x34), FI(0x40), 0) + 1; // 0x0042563c..0x00425647
                                const int h2 = kHeightBlend(px, py, 0);                 // 0x00425648
                                if (h2 <= h1 && TypeU(Tile(back), 6) != 0xd)            // 0x00425650..0x00425687
                                    FI(0x48) = py * 50 + px;                            // 0x00425689..0x00425693 (transposed)
                            }
                            if (TypeU(Tile(idx), 7) == 0x10 && TypeU(Tile(back), 6) != 0xd) {   // 0x004256a5..0x004256f0
                                const int o = kObjectAt(px, py);                        // 0x004256f4
                                if (TileByte(idx) == 0x15) {                            // 0x004256f9..0x00425705
                                    if (o != -1 && G<short>(0x0058bcb8 + o * 16) == 5 && G<int>(0x0058bcc0 + o * 16) != 0)   // 0x00425707..0x00425721
                                        FI(0x1c) = G<int>(0x0058bcc0 + o * 16);         // 0x00425723
                                    else
                                        FI(0x38) = py * 50 + px;                        // 0x00425729..0x00425733
                                } else if (G<short>(0x0058bcb8 + o * 16) == 4) {        // 0x00425739..0x00425744
                                    if (G<int>(0x0058bcc0 + o * 16) < 0x10) FI(0x48) = py * 50 + px;   // 0x00425746..0x0042575b
                                    else FI(0x38) = py * 50 + px;                       // 0x00425761..0x00425768
                                }
                            }
                        }
                    }
                    ++FI(0x20);                                                         // 0x0042576c..0x0042577d
                } while (FI(0x20) < Shl(kHeightBlend(FI(0x34), FI(0x40), 0), 4) / (G<int>(0x00822c88) + 2));   // 0x00425781..0x0042579a
                e7 = FI(0x30);                                                          // 0x004257a0
            }
        }
        // 0x004257dc
        {
            const int h1 = kHeightBlend(RI(rec, 0xcc), RI(rec, 0xd0), 0);               // 0x004257ec
            const int h2 = kHeightBlend(FI(0x34), FI(0x40), 0);                         // 0x004257ff
            const int dd = h2 - h1;                                                     // 0x0042580a
            RB(rec, 0x1b) = static_cast<unsigned char>(RB(rec, 0x1b) + (dd < 0 ? -dd : dd));   // 0x00425804..0x0042581d
        }
        if (G<int>(0x005a9ce4) != 0 && TypeS(Tile(RI(rec, 0xcc) * 50 + RI(rec, 0xd0)), 2) > 0 &&   // 0x00425816..0x00425875
            TypeS(FI(0x14), 2) <= 0 && FI(0xb24) == 0 && !(RB(rec, 0x10) & 1)) {
            kEmitThought(g, 8, 0x14);                                                   // 0x00425877..0x00425883
        } else if (FI(0x38) != 0) {                                                     // 0x00425890..0x0042589a
            if (FI(0x1c) != 0) kEmitThought(g, 0x16, FI(0x1c) - 1);                     // 0x0042589c / 0x004258bc..0x004258c8
            else kEmitThought(g, 0x14, FI(0x38));                                       // 0x004258a0..0x004258ab
        } else if (FI(0x1c) != 0) {                                                     // 0x004258b8 / 0x004258ba
            kEmitThought(g, 0x16, FI(0x1c) - 1);                                        // 0x004258bc..0x004258c8
        } else if ((TileFlags(FI(0x50)) & 0x800) && kThoughtFlag(g) != 0 &&             // 0x004258d5..0x00425911
                   !(TileFlags(FI(0x50)) & 0x4000)) {
            kEmitThought(g, 0x18, 0x14);                                                // 0x00425913..0x00425918
        } else {
            if (FI(0x10) < 100 || FI(0xb24) != 0) G<int>(0x005a7140) &= ~3;             // 0x00425925..0x0042593e
            const int v = G<int>(0x005a7140);                                           // 0x00425945
            if (v != 0 && RW(rec, 0xac) != 4) {                                         // 0x0042594a..0x00425956
                if (v & 3) kEmitThought(g, (~v & 1) | 0x20, 0x14);                      // 0x00425958..0x00425967
                else if (TypeS(FI(0x14), 2) <= 0) kEmitThought(g, 6, 0x14);             // 0x00425974..0x00425991
            } else {
                bool done = false;
                if (HoleOf(rec) > 1 && RS(rec, 0x22) == (g & 1) + 1) {                  // 0x0042599e..0x004259b8
                    const int r = RandRange(3) & 0xffff;                                // 0x004259ba..0x004259cc
                    if (G<int>(0x00575cac + HoleOf(rec) * 0x208) <= r) {                // 0x004259d1..0x004259e2
                        kEmitThought(g, 0x1d, 0x14);                                    // 0x004259e4..0x004259e9
                        done = true;
                    }
                }
                if (!done && G<int>(0x00822c88) != 0) {                                 // 0x004259f6..0x004259fd
                    if (HoleOf(rec) > 1 && RB(rec, 0x22) == 1) {                        // 0x00425a03..0x00425a0f
                        const int r = RandRange(3) & 0xffff;                            // 0x00425a11..0x00425a26
                        if (G<int>(0x00575cac + HoleOf(rec) * 0x208) > r + 3) {         // 0x00425a2b..0x00425a3a
                            kEmitThought(g, 0x1e, 0x14);                                // 0x00425a3c..0x00425a41
                            done = true;
                        }
                    }
                    if (!done && G<int>(0x00822c88) != 0) {                             // 0x00425a4e..0x00425a55
                        const int r = RandRange(4) & 0xffff;                            // 0x00425a57..0x00425a69
                        if (RS(rec, 0x1b) <= r + G<int>(0x00822c88) * 2 && FI(0x10) > 100 &&   // 0x00425a6e..0x00425a9d
                            TypeS(FI(0x14), 2) <= 0 && FI(0x48) == 0) {
                            kEmitThought(g, 4, 0x14);                                   // 0x00425a9f..0x00425aa4
                            RB(rec, 0x1b) = 0;                                          // 0x00425aac
                            done = true;
                        }
                    }
                }
                if (!done) {
                    const int r = RandRange(5) & 0xffff;                                // 0x00425ab8..0x00425ad0
                    const int dif = RS(rec, 0x1b);                                      // 0x00425aca / 0x00425ad9
                    if (dif > r + G<int>(0x00822c88) + 6 && FI(0x10) > 0x28 && FI(0x48) == 0) {   // 0x00425ad5..0x00425aed
                        int kind;
                        if (dif >= 0x18) {                                              // 0x00425aef / 0x00425af2
                            kind = 0x1f;                                                // 0x00425af4..0x00425af9
                        } else {
                            const int dd = (FI(0x4c) - FI(0x24)) & 7;                   // 0x00425afd..0x00425b07
                            kind = (dd < 2 || dd > 6) ? 0x26 : ((dd == 2 || dd == 6) ? 5 : 0x25);   // 0x00425b0a..0x00425b35
                        }
                        kEmitThought(g, kind, FI(0x3c));                                // 0x00425b37 / 0x00425b38
                        RB(rec, 0x1b) = RS(rec, 0x1b) < 0x18 ? 10 : 20;                 // 0x00425b3d..0x00425b52
                    } else {
                        RB(rec, 0x1b) = 6;                                              // 0x00425b5a
                    }
                }
            }
        }
        // 0x00425b61
        RI(rec, 0x10) &= 0xffffff9f;                                                    // 0x00425b61..0x00425b6c
        if (RB(rec, 0x7a) == e7 && RB(rec, 0x84) == 0) {                                // 0x00425b72..0x00425b88
            if (FI(0x48) != 0 && TypeS(FI(0x14), 2) < 1) {                              // 0x00425b8e..0x00425ba7
                kEmitThought(g, 0x1c, FI(0x48));                                        // 0x00425ba9..0x00425bad
            } else {
                if (FI(0xb30) == 1) kEmitThought(g, 0x37, 0x14);                        // 0x00425bba..0x00425bd6
                else if (FI(0xb30) == -1) kEmitThought(g, 0x38, 0x14);
                if ((RB(rec, 0x10) & 0x80) && FI(0x10) > 100) kEmitThought(g, 0x39, 0x14);   // 0x00425bde..0x00425bf3
                if (RW(rec, 0xac) == 4) kEmitThought(g, RB(rec, 0x22) != 0 ? 0x3c : 9, FI(0x14));   // 0x00425bfb..0x00425c1c
                if (FI(0x10) > 0x4b) {                                                  // 0x00425c24 / 0x00425c29
                    const int sh = (RB(rec, 0x19) & 2) ? 15 : 16;                       // 0x00425c2b..0x00425c43
                    if (RI(rec, 0xec) > Shl(0x1e, sh)) RI(rec, 0x10) |= 0x20;           // 0x00425c46..0x00425c54
                    if (RI(rec, 0xec) < Shl(-0x1e, sh)) RI(rec, 0x10) |= 0x40;          // 0x00425c5a..0x00425c81
                }
                if (RB(rec, 0x7a) == e7 && RB(rec, 0x84) == 0 && RB(rec, 0x22) == 0 &&  // 0x00425c87..0x00425cce
                    G<unsigned char>(0x005849f6 + static_cast<short>(RW(rec, 0xb6)) * 0x2c + HoleOf(rec)) != 0 &&
                    RB(rec, 0x18) == 0)
                    kEmitThought(g, 0x3b, 0x14);                                        // 0x00425cd0..0x00425cd5
                if (RB(rec, 0x7a) == e7 && RB(rec, 0x22) == 3 && ((RB(rec, 0x21) + g) & 1) == 0 &&   // 0x00425cdd..0x00425d2a
                    RB(rec, 0x18) == 0 && G<unsigned char>(0x005794d0 + static_cast<short>(RW(rec, 0xa2)) * 256) == 0) {
                    const int pg = g ^ 1;                                               // 0x00425d2c / 0x00425d2e
                    const int t1 = kTypeBit7Clear(pg);                                  // 0x00425d32
                    const int t2 = kTypeBit7Clear(g);                                   // 0x00425d3a
                    if (t2 == t1 && G<unsigned char>(0x0057953c + static_cast<short>(RW(rec, 0xa2)) * 256) == 0 &&   // 0x00425d42..0x00425d62
                        RB(rec, 0x84) == 0) {
                        kEmitThought(g, 0x30, 0x14);                                    // 0x00425d64..0x00425d69
                        kEmitThought(pg, 0x31, 0x14);                                   // 0x00425d6e..0x00425d73
                        G<unsigned char>(0x0057953c + static_cast<short>(RW(rec, 0xa2)) * 256) += 1;   // 0x00425d78..0x00425d93
                    }
                    e7 = FI(0x30);                                                      // 0x00425d95
                }
            }
        }
        if (RB(rec, 0x84) != 0) e7 = -1;                                                // 0x00425d99..0x00425da3
        if (RB(rec, 0x7a) == e7 && HoleOf(rec) > 1 && RS(rec, 0x1c) < 0xd &&            // 0x00425da6..0x00425dd8
            !(static_cast<short>(RW(rec, 0xa0)) & (1 << (RB(rec, 0x1c) & 31))))
            kEmitThought(g, 0x36, RS(rec, 0x1c));                                       // 0x00425dda..0x00425dde
        RW(rec, 0xa0) |= static_cast<unsigned short>(1u << (RB(rec, 0x1c) & 31));        // 0x00425de6..0x00425df5
        if (RB(rec, 0x7a) == e7) {                                                      // 0x00425dfc..0x00425e04
            {
                const int h1 = kHeightBlend(RI(rec, 0xcc), RI(rec, 0xd0), 0);           // 0x00425e16
                const int h2 = kHeightBlend(FI(0x34), FI(0x40), 0);                     // 0x00425e29
                if (h2 > h1) kEmitThought(g, 0x2e, 0x14);                               // 0x00425e31..0x00425e3a
            }
            {
                const int h1 = kHeightBlend(RI(rec, 0xcc), RI(rec, 0xd0), 0);           // 0x00425e52
                const int h2 = kHeightBlend(FI(0x34), FI(0x40), 0);                     // 0x00425e65
                if (h2 < h1) kEmitThought(g, 0x2d, 0x14);                               // 0x00425e6d..0x00425e76
            }
        }
        if (RB(rec, 0x7a) == e7 && RB(rec, 0x18) == 0 && RB(rec, 0x84) == 0 && FI(0x14) == 1 &&   // 0x00425e7e..0x00425eba
            ((RB(rec, 0x22) + g + RB(rec, 0x21)) & 3) == 0)
            kEmitThought(g, 0x3e, 0x14);                                                // 0x00425ebc..0x00425ec1
    }
    // 0x00425ed2: the aim offset, the power cap, the lie
    RI(rec, 0x10) &= ~2;                                                                // 0x00425ed2..0x00425ee5
    FI(0x1c) = 0;                                                                       // 0x00425eeb: spin added at the end
    int cap;
    if (FI(0xb30) == 1 || FI(0xb30) == -1) {                                            // 0x00425ef3 / 0x00425fe3..0x00425fe6
        const bool right = FI(0xb30) == 1;
        const int nd = Add(RI(rec, 0xe0), right ? 0x15555554 : static_cast<int>(0xeaaaaaacu));   // 0x00425ef9..0x00425f05 / 0x00425fec..0x00425ff8
        RI(rec, 0xe0) = nd;
        if (RB(rec, 0x10) & 0x80)                                                       // 0x00425f0b..0x00425f16 / 0x00425ffe..0x00426009
            RI(rec, 0xe0) = Add(nd, right ? static_cast<int>(0xfaaaaaabu) : 0x5555555);  // 0x00425f18 / 0x0042600b
        const int sp = RI(rec, 0xec);
        if ((right ? sp > 0 : sp < 0) && g == G<int>(0x005a59f8))                       // 0x00425f23..0x00425f33 / 0x00426016..0x00426026
            RI(rec, 0xec) = sp / 2;                                                     // 0x00425f35..0x00425f3a
        const int mp = FI(0x2c), dd = FI(0x10);                                         // 0x00425f40 / 0x00425f44
        FI(0x1c) = right ? static_cast<int>(0xfdc656abu) : 0x239a955;                   // 0x00425f55 / 0x00426048
        const int adj = mp + Shl(mp, 4) / (400 - dd);                                   // 0x00425f48..0x00425f5f
        if (dd >= 300) RI(rec, 0xe4) = adj;                                             // 0x00425f61 / 0x00425fca
        else if (dd >= 250) RI(rec, 0xe4) = Shl(RI(rec, 0xe4), 4) / (400 - dd) + RI(rec, 0xe4);   // 0x00425f69 / 0x00425fa1..0x00425fb1
        else RI(rec, 0xe4) = Shl(RI(rec, 0xe4), 4) / (0x14a - dd) + RI(rec, 0xe4);      // 0x00425f71..0x00425f88
        RW(rec, 0xac) = right ? 1 : 0xffff;                                             // 0x00425f8e / 0x00426081
        cap = 999999;                                                                   // 0x00425f97 / 0x0042608a
    } else {
        cap = FI(0x2c);                                                                 // 0x004260cd
    }
    const int base = kClamp(RI(rec, 0xe4), 0, cap);                                     // 0x004260d1..0x004260e0 (ebp)
    RI(rec, 0xe4) = base;                                                               // 0x004260ec
    if (FI(0x10) < 0x4b) {                                                              // 0x004260e2..0x004260f2
        const int lv = G<int>(0x00822c88);                                              // 0x004260f4
        bool turn = true;
        if (lv == 0) {                                                                  // 0x004260fa / 0x004260fc
            if (g == FI(0xb24)) {                                                       // 0x00426102..0x0042610b
                RI(rec, 0xec) = RI(rec, 0xec) / 3;                                      // 0x00426111..0x00426125
                turn = false;
            } else if (g == (FI(0xb24) | 1)) {                                          // 0x004262ac..0x004262b0
                RI(rec, 0xec) = Shl(RI(rec, 0xec), 1);                                  // 0x004262b2..0x004262ba
            }
        }
        if (turn && FI(0x14) != 1) {                                                    // 0x004262c0..0x004262c7
            const int k = ((RB(rec, 0x19) & 7) == 7) ? lv + 4 : 9 - lv;                 // 0x004262cd..0x004262e3
            RI(rec, 0xe0) = Add(RI(rec, 0xe0), Shl(Mul(k, RI(rec, 0xec)), 1));          // 0x004262e5..0x004262f6
        }
    }
    if (RB(rec, 0x18) != 0 && TypeS(FI(0x14), 2) > 0) {                                 // 0x0042612f..0x00426147
        const int r = RandRange(TypeS(FI(0x14), 2) * 10) & 0xffff;                      // 0x00426149..0x00426163
        if (RB(rec, 0xf8) >= r) {                                                       // 0x00426163..0x0042616b
            FI(0x14) = 2;                                                               // 0x00426173 / 0x0042617d
            RI(rec, 0x10) |= 0x400000;                                                  // 0x0042616d..0x00426181
        }
    }
    if (G<int>(0x005a9cc4) == 0 && FI(0x14) != 1) {                                     // 0x00426187..0x00426193
        const int r = RandRange(0x5555) & 0xffff;                                       // 0x00426195..0x004261b2
        const int b2 = TypeS(FI(0x14), 2);                                              // 0x004261ad..0x004261be
        const int s = kSign(RI(rec, 0xec));                                             // 0x004261c5
        RI(rec, 0xe0) = Add(RI(rec, 0xe0), Mul(Mul(s, r + 0x1555555), b2));             // 0x004261ca..0x004261e1
        RI(rec, 0xec) = Mul(b2 + 2, RI(rec, 0xec)) / 2;                                 // 0x004261de..0x004261fa
    }
    if ((RB(rec, 0x10) & 1) && RS(rec, 0x22) > 6) {                                     // 0x00426200..0x00426210
        RI(rec, 0xe4) = base;                                                           // 0x00426215
        FI(0x14) = -1;                                                                  // 0x00426212 / 0x0042621b
    }
    RI(rec, 0xec) = Add(RI(rec, 0xec), FI(0x1c));                                       // 0x0042621f..0x00426231
    // 0x00426237: the lie switch (byte table 0x00426658, jump table 0x00426634, index lie + 1, ja 0x004264d5 above 0x17)
    {
        const int L = FI(0x14);
        int s = RI(rec, 0xe4);
        switch (L) {
        case -1:                                                                        // 0x00426506
            break;
        case 0: {                                                                       // 0x00426301
            const int r = RandRange(s / 4) & 0xffff;
            s = RI(rec, 0xe4);
            RI(rec, 0xe4) = s / 8 - r + s;                                              // 0x0042632c..0x00426335 -> 0x004264fc
            break;
        }
        case 1: {                                                                       // 0x0042624c
            const int r = RandRange(s / 8) & 0xffff;
            s = RI(rec, 0xe4);
            RI(rec, 0xe4) = s / 12 - r + s;                                             // 0x0042626f..0x00426289
            if (RB(rec, 0x1c) == 0xd) RI(rec, 0xe8) = 0;                                // 0x0042628f..0x0042629d
            break;
        }
        case 2:                                                                         // 0x0042633a
            if (!(TileFlags(FI(0x50)) & 0x800)) {                                       // 0x0042633e..0x0042634f
                const int r = RandRange(s / 3) & 0xffff;                                // 0x00426355..0x0042636f
                s = RI(rec, 0xe4);
                RI(rec, 0xe4) = s / 6 - r + s;                                          // 0x0042637d..0x00426395
                break;
            }
            goto def;                                                                   // 0x004264d5
        case 4: case 5: case 8: case 21: case 22: {                                     // 0x004263a0
            int r = RandRange(s / 2) & 0xffff;
            s = RI(rec, 0xe4);
            RI(rec, 0xe4) = s / 4 - r + s;                                              // 0x004263b6..0x004263d9
            r = RandRange(RI(rec, 0xec) / 2) & 0xffff;                                  // 0x004263df..0x004263fc
            RI(rec, 0xec) = Add(RI(rec, 0xec), r);                                      // 0x00426405 / 0x00426407
            if (L == (G<unsigned char>(0x005a34e0) != 1 ? 5 : 4))                       // 0x004263f6 / 0x0042640f..0x0042641c
                RI(rec, 0xe0) = Add(RI(rec, 0xe0), Shl(RI(rec, 0xec), 2));              // 0x0042641e..0x0042642d
            if (!(G<unsigned char>(0x005a47e0) & 4)) break;                             // 0x00426433 / 0x0042643a
            s = RI(rec, 0xe4);
        }
        // fall through to 0x00426440
        case 10: case 13: case 14: case 15: case 16: {                                  // 0x00426440
            const int r = RandRange(s / 2) & 0xffff;
            s = RI(rec, 0xe4);
            RI(rec, 0xe4) = s / 4 - r + s;                                              // 0x0042645f..0x00426470 -> 0x004264fc
            break;
        }
        case 9:                                                                         // 0x00426475
            RI(rec, 0xe4) = RandRange(Shl(s, 1)) & 0xffff;                              // 0x00426475..0x0042648d
            RI(rec, 0xe0) = Add(RI(rec, 0xe0), Shl(RI(rec, 0xec), 4));                  // 0x00426493..0x004264a4
            break;
        case 11: case 12: {                                                             // 0x004264ac
            const int r = RandRange(s) & 0xffff;
            s = RI(rec, 0xe4);
            RI(rec, 0xe4) = s / 2 + r;                                                  // 0x004264c0..0x004264d3 -> 0x00426500
            break;
        }
        default:                                                                        // 0x004264d5: 3, 6, 7, 17..20 and others
        def: {
            const int r = RandRange(s) & 0xffff;
            s = RI(rec, 0xe4);
            RI(rec, 0xe4) = s / 2 - r + s;                                              // 0x004264e6..0x004264fe
            break;
        }
        }
    }
    // 0x00426506: pull toward the base power
    {
        const int L = FI(0x14);
        if (((RB(rec, 0x19) & 2) && TypeS(L, 2) <= 0) || (RB(rec, 0x10) & 1))          // 0x00426506..0x0042652b
            RI(rec, 0xe4) = Add(base, RI(rec, 0xe4)) / 2;                               // 0x0042652d..0x0042653c
        const int lv = G<int>(0x005a9cc4);                                              // 0x00426542
        if (lv == 0)                                                                    // 0x00426548 / 0x0042654a
            RI(rec, 0xe4) = Mul(TypeS(L, 2), RI(rec, 0xe4) - base) / 8 + RI(rec, 0xe4); // 0x0042654c..0x00426571
        if (RI(rec, 0xe4) < base / 3) RI(rec, 0xe4) = base / 3;                         // 0x00426577..0x0042658f
        if (lv >= 2) RI(rec, 0xe4) = base;                                              // 0x00426595..0x0042659a
        if (lv == 1) RI(rec, 0xe4) = Add(RI(rec, 0xe4), Shl(base, 1)) / 3;              // 0x004265a0..0x004265bc
        const int sp = RI(rec, 0xe4);                                                   // 0x004265c6
        const int c = kClamp(FI(0x18), -10, 16);                                        // 0x004265c2..0x004265d1
        RI(rec, 0xe4) = Mul(c, base - sp) / 16 + sp;                                    // 0x004265d6..0x004265f0
        RI(rec, 0xec) = kClamp(RI(rec, 0xec), static_cast<int>(0xeaaaaaabu), 0x15555555);   // 0x004265f6..0x00426602
    }
    const int fl = RI(rec, 0x10) & ~1;                                                  // 0x00426608..0x00426611
    RI(rec, 0x10) = fl;                                                                 // 0x00426613
    G<unsigned char>(0x005786a2) = 8;                                                   // 0x00426619
    G<unsigned char>(0x00578732) = 8;                                                   // 0x00426623
    return fl;
}
#undef FI

// The original's prologue (0x00424120..0x00424132) and epilogue (0x00426620..0x00426631) around the body.
__declspec(naked) int __cdecl SimulateShot_re(int, int, int, int, int) {
    __asm {
        sub esp, 0xb0c
        push ebx
        push ebp
        push esi
        push edi
        mov eax, esp
        push eax
        call SimulateShotBody
        add esp, 4
        pop edi
        pop esi
        pop ebp
        pop ebx
        add esp, 0xb0c
        ret
    }
}
C5_t SimulateShot_orig;
// 0x00424120
SG_HOOK("golf_clean.exe", 0x00424120, simulateShot, SimulateShot_re, SimulateShot_orig);

// ====================================================================================================================
// 0x00475580  saveBmp(path, rect): writes the 16-bit surface of a holder object to a 24-bit .bmp file.
// ====================================================================================================================
// __thiscall (ecx = the holder, its surface at +4), `ret 8` (0x00475818 / 0x00475832). Surface calls through its vtable:
// +0x20 the pixel pointer (0x0047559e), +0xd8 the row length in pixels (0x004755cb, 0x004756e5), +0xdc the height
// (0x004755e2); a NULL surface gives NULL pixels and 0 for both. The BITMAPINFOHEADER is 11 zeroed dwords on the stack
// (0x004755b4): size 0x28, width / height = the rect's extent or the surface's (0x004755c2..0x00475609), planes 1, 24 bits
// (0x00475617 / 0x0047561e), compression 0. fopen(path, the mode string at 0x004c7828) (0x00475629) NULL returns 2
// (0x0047580f). The image size is |height| * ((24 * width + 7) / 8) (0x00475643..0x0047566b, no row padding; the header's
// size field is 0, so the other arm 0x00475671 never runs), the buffer comes from operator new 0x004a57f1 (0x00475676).
// Each pixel w of rows y0..y0+|h| and columns x0..x0+width (from the rect, else 0) gives three bytes: (w << 3) & 0xff, then
// with the display's pixel format (the live display [0x0083ad50], slot +0xb4) 1: (w >> 3) & 0xfc and (w >> 8) & 0xf8,
// otherwise (w >> 2) & 0xf8 and (w >> 7) & 0xf8 (0x004756d8..0x00475755). flipBmpRows 0x00475410(this, buffer, header)
// (0x0047578f), then _fwrite 0x004a6268 of the 14-byte file header 'BM', size + 0x36, 0, 0, 0x36 (0x0047579c..0x004757c8),
// the 40-byte info header (0x004757df) and the buffer (0x004757f1); a short write returns 2 (0x004757fe), else 0
// (0x00475829), both after fclose 0x004a609f and operator delete 0x004a4ffc of the buffer.
typedef int(__fastcall* SurfI_t)(void* self, void* edx);
typedef void*(__fastcall* SurfP_t)(void* self, void* edx);
typedef void(__fastcall* Flip_t)(void* self, void* edx, unsigned char* buf, void* hdr);
typedef void*(__cdecl* Fopen_t)(const char* path, const char* mode);
typedef unsigned(__cdecl* Fwrite_t)(const void* p, unsigned size, unsigned n, void* f);
typedef int(__cdecl* Fclose_t)(void* f);
typedef void*(__cdecl* New_t)(unsigned n);
typedef void(__cdecl* Delete_t)(void* p);
namespace {
template <typename T> static inline T VSlot(void* obj, unsigned off) {
    return *reinterpret_cast<T*>(*reinterpret_cast<unsigned char**>(obj) + off);
}
const Fopen_t kFopen = reinterpret_cast<Fopen_t>(0x004a614d);
const Fwrite_t kFwrite = reinterpret_cast<Fwrite_t>(0x004a6268);
const Fclose_t kFclose = reinterpret_cast<Fclose_t>(0x004a609f);
const New_t kNew = reinterpret_cast<New_t>(0x004a57f1);
const Delete_t kDelete = reinterpret_cast<Delete_t>(0x004a4ffc);
const Flip_t kFlipBmpRows = reinterpret_cast<Flip_t>(0x00475410);
}  // namespace

int __fastcall SaveBmp_re(void* self, void*, const char* path, const int* rect) {
    unsigned char* holder = static_cast<unsigned char*>(self);
    void* surf = *reinterpret_cast<void**>(holder + 4);                                 // 0x0047558b
    const unsigned short* pix = surf ? static_cast<const unsigned short*>(VSlot<SurfP_t>(surf, 0x20)(surf, 0)) : 0;   // 0x00475592..0x004755a1
    int hdr[11] = {0};                                                                  // rep stosd 0x004755b4
    hdr[0] = 0x28;                                                                      // 0x004755b8
    if (rect == 0) {                                                                    // 0x004755b6 / 0x004755c0
        void* s = *reinterpret_cast<void**>(holder + 4);
        hdr[1] = s ? VSlot<SurfI_t>(s, 0xd8)(s, 0) : 0;                                 // 0x004755c2..0x004755d8
        void* s2 = *reinterpret_cast<void**>(holder + 4);
        hdr[2] = s2 ? VSlot<SurfI_t>(s2, 0xdc)(s2, 0) : 0;                              // 0x004755d5..0x004755f0
    } else {
        hdr[1] = rect[2] - rect[0];                                                     // 0x004755f6..0x00475603
        hdr[2] = rect[3] - rect[1];                                                     // 0x00475600..0x00475609
    }
    reinterpret_cast<unsigned short*>(hdr)[6] = 1;                                      // planes 0x00475617
    reinterpret_cast<unsigned short*>(hdr)[7] = 0x18;                                   // bit count 0x0047561e
    hdr[4] = 0;                                                                         // compression 0x00475625
    void* f = kFopen(path, reinterpret_cast<const char*>(0x004c7828));                  // 0x00475629
    if (f == 0) return 2;                                                               // 0x00475631 / 0x00475637 -> 0x0047580f
    const int ah = hdr[2] < 0 ? -hdr[2] : hdr[2];                                       // 0x00475647..0x0047565d
    const int size = ah * ((0x18 * hdr[1] + 7) / 8);                                    // 0x0047564a..0x0047566b (hdr[5] is 0)
    unsigned char* buf = static_cast<unsigned char*>(kNew(static_cast<unsigned>(size)));   // 0x00475676
    int y, xEnd, yEnd;
    if (rect) {                                                                         // 0x0047567e / 0x00475682
        y = rect[1];                                                                    // 0x0047568e
        xEnd = rect[0] + hdr[1];                                                        // 0x00475691 / 0x00475698
        yEnd = ah + rect[1];                                                            // 0x0047569c..0x004756a3
    } else {
        y = 0; xEnd = hdr[1]; yEnd = ah;                                                // 0x004756a9..0x004756c0
    }
    int k = 0;                                                                          // edi 0x004756c4
    int x = rect ? rect[0] : 0;                                                         // ebx 0x00475684 / 0x00475587
    for (; y < yEnd; ++y) {                                                             // 0x004756c6 / 0x00475778..0x0047577f
        for (; x < xEnd; ++x) {                                                         // 0x004756ce / 0x0047575d..0x00475760
            void* s = *reinterpret_cast<void**>(holder + 4);                            // 0x004756d8..0x004756dc
            const int pitch = s ? VSlot<SurfI_t>(s, 0xd8)(s, 0) : 0;                    // 0x004756df..0x004756ed
            const unsigned w = pix[pitch * y + x];                                      // 0x004756ef..0x004756f8
            buf[k++] = static_cast<unsigned char>(w << 3);                              // 0x004756fc..0x00475703
            void* d = G<void*>(0x0083ad50);
            const int f1 = VSlot<SurfI_t>(d, 0xb4)(d, 0);                               // 0x00475706..0x00475715
            buf[k++] = static_cast<unsigned char>(f1 == 1 ? (w >> 3) & 0xfc : (w >> 2) & 0xf8);   // 0x0047571b..0x0047572e
            void* d2 = G<void*>(0x0083ad50);
            const int f2 = VSlot<SurfI_t>(d2, 0xb4)(d2, 0);                             // 0x00475731..0x0047573a
            buf[k++] = static_cast<unsigned char>(f2 == 1 ? (w >> 8) & 0xf8 : (w >> 7) & 0xf8);   // 0x00475740..0x00475755
        }
        x = rect ? rect[0] : 0;                                                         // 0x0047576a..0x00475776
    }
    kFlipBmpRows(self, 0, buf, hdr);                                                    // 0x00475785..0x0047578f
    unsigned char fh[14];                                                               // [F+0x28]
    fh[0] = 'B'; fh[1] = 'M';                                                           // 0x004757af (0x4d42)
    *reinterpret_cast<int*>(fh + 2) = size + 0x36;                                      // 0x0047579f..0x004757a6
    *reinterpret_cast<unsigned short*>(fh + 6) = 0;                                     // 0x004757b6
    *reinterpret_cast<unsigned short*>(fh + 8) = 0;                                     // 0x004757bb
    *reinterpret_cast<int*>(fh + 10) = 0x36;                                            // 0x004757c0
    int r = 2;
    if (kFwrite(fh, 1, 0xe, f) >= 0xe &&                                                // 0x004757c8..0x004757d4
        kFwrite(hdr, 1, 0x28, f) >= 0x28 &&                                             // 0x004757df..0x004757eb
        kFwrite(buf, 1, static_cast<unsigned>(size), f) >= static_cast<unsigned>(size)) // 0x004757f1..0x004757fc (unsigned jae)
        r = 0;                                                                          // 0x00475829
    kFclose(f);                                                                         // 0x004757fe / 0x0047581b
    kDelete(buf);                                                                       // 0x00475804 / 0x00475821
    return r;
}
typedef int(__fastcall* SaveBmp_t)(void* self, void* edx, const char* path, const int* rect);
SaveBmp_t SaveBmp_orig;
// 0x00475580
SG_HOOK("golf_clean.exe", 0x00475580, saveBmp, SaveBmp_re, SaveBmp_orig);

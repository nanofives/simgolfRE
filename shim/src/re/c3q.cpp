// C3 batch c3q (2026-10-07) of golf_clean.exe: functions with a static caller that the test scenarios never reach
// (an event post, two text appenders, a rank bucket, three table writers, a terrain relax step, a walk test and a
// nearest-hole search). Hand-written from the disassembly (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr>
// --list) and the C2 transcriptions; each body cites the address of every global, offset and callee it uses. All are
// __cdecl. Callees are invoked through their original addresses (Random::range is __thiscall, emulated with __fastcall:
// ecx = this, edx unused), so a hooked callee still runs its own reimplementation and the path-1 A/B drives both arms.
#include "hooks.h"

namespace {

template <typename T> T& ref(unsigned addr) { return *reinterpret_cast<T*>(addr); }

// Tile tables, 50 x 50, indexed [x * 50 + y]: type bytes 0x005722e8 (read by tileBlocked 0x0040bf60), signed height
// bytes 0x00543018, flag words 0x0053caf0. Per-type records 0x30 bytes apart; the signed byte at +2 is 0x00578372.
// Direction tables (dword per direction 0..7): dx at 0x004c2878 = {0, 1, 1, 1, 0, -1, -1, -1}, dy at 0x004c2898 =
// {-1, -1, 0, 1, 1, 1, 0, -1} (values read from the image's .data section).
inline int cell(int x, int y) { return x * 50 + y; }
inline signed char& typeAt(int x, int y) { return ref<signed char>(0x005722e8 + cell(x, y)); }
inline signed char& heightAt(int x, int y) { return ref<signed char>(0x00543018 + cell(x, y)); }
inline signed char typeByte2(int t) { return ref<signed char>(0x00578372 + t * 0x30); }
inline int dirDx(int d) { return ref<int>(0x004c2878 + d * 4); }
inline int dirDy(int d) { return ref<int>(0x004c2898 + d * 4); }

typedef int(__cdecl* Int2_t)(int, int);
const Int2_t kTileBlocked = reinterpret_cast<Int2_t>(0x0040bf60);  // tileBlocked(x, y): 1 off-map or type 0x14
const Int2_t kDistance = reinterpret_cast<Int2_t>(0x0040acd0);     // distance(dx, dy)
typedef int(__fastcall* Range_t)(void* self, void* edx, int n);    // Random::range 0x0045c1e0 (thiscall)
const Range_t kRange = reinterpret_cast<Range_t>(0x0045c1e0);
void* const kRng = reinterpret_cast<void*>(0x00822d9c);             // the generator object passed in ecx

// Appends the NUL-terminated string at `src` to the text buffer 0x0051a068 (the inline strlen / rep movs copy both
// appenders below use: the terminator is copied too).
void appendText(const char* src) {
    char* dst = reinterpret_cast<char*>(0x0051a068);
    while (*dst) ++dst;
    do { *dst++ = *src; } while (*src++);
}

// 0x0046e7b0  postEvent(id, a, b): does nothing when bit 0x4000000 of the flags dword 0x0059e7b8 is set (0x0046e7b0 /
// 0x0046e7ba jne) or when the dword at 0x004c15a0 + id*0x30 is non-zero (0x0046e7c6 / 0x0046e7ce jne). Otherwise it
// records id in 0x004e3db8 (0x0046e7d6), stores the tick dword 0x00834170 at 0x004c15a0 + id*0x30 (0x0046e7e0) and the
// current-course dword 0x0059bf90 at 0x004c15a4 + id*0x30 (0x0046e7f6), clears 0x00839338 (0x0046e7ec), and stores a
// in 0x004e3dbc (0x0046e800) and b in 0x008392a4 (0x0046e805).
typedef void(__cdecl* PostEvent_t)(int, int, int);
PostEvent_t PostEvent_orig;
void __cdecl PostEvent_re(int id, int a, int b) {
    if (ref<unsigned>(0x0059e7b8) & 0x4000000) return;
    const unsigned rec = 0x004c15a0 + id * 0x30;
    if (ref<int>(rec) != 0) return;
    ref<int>(0x004e3db8) = id;
    ref<int>(rec) = ref<int>(0x00834170);
    ref<int>(0x00839338) = 0;
    ref<int>(rec + 4) = ref<int>(0x0059bf90);
    ref<int>(0x004e3dbc) = a;
    ref<int>(0x008392a4) = b;
}

// 0x0042f630  relaxTile42f630(x, y): lowers tile (x, y)'s signed height byte (0x00543018 + x*50 + y) to the lowest of
// its four orthogonal neighbours that qualify, and returns 1 when it lowered it, else 0 (flag at [esp+0x14], set at
// 0x0042f6b8, returned at 0x0042f6d0). The neighbours are directions 0, 2, 4, 6 of the direction tables (byte offsets
// 0, 8, 16, 24 at 0x0042f668 / 0x0042f66e, loop step 8 below 0x20 at 0x0042f6c4..0x0042f6ce), visited in that order.
// A neighbour (nx, ny) qualifies when tileBlocked(nx, ny) (0x0040bf60, called at 0x0042f67a) returns 0 (0x0042f684),
// its type byte equals tile (x, y)'s (0x0042f689..0x0042f69e) and its height is below the running minimum, which
// starts at tile (x, y)'s height (0x0042f652) and follows each store (signed `jge` at 0x0042f6ab; the store at
// 0x0042f6b1 writes the new height into tile (x, y) at once).
typedef int(__cdecl* RelaxTile_t)(int, int);
RelaxTile_t RelaxTile_orig;
int __cdecl RelaxTile_re(int x, int y) {
    int lowered = 0;
    int cur = heightAt(x, y);
    for (int d = 0; d < 8; d += 2) {
        const int nx = x + dirDx(d);
        const int ny = y + dirDy(d);
        if (kTileBlocked(nx, ny) != 0) continue;
        if (static_cast<unsigned char>(typeAt(nx, ny)) != static_cast<unsigned char>(typeAt(x, y))) continue;
        const int nh = heightAt(nx, ny);
        if (nh >= cur) continue;
        cur = nh;
        heightAt(x, y) = static_cast<signed char>(nh);
        lowered = 1;
    }
    return lowered;
}

// 0x004011b0  freeRecordAtPos(x, y): walks the 100 records of 0x3c bytes at 0x0056d1d8 (pointer p = record + 8 runs
// from 0x0056d1e0 while below 0x0056e950, 0x004011b9 / 0x004011d3..0x004011db) and, for every record whose signed
// word at +6 equals x (0x004011be / 0x004011c4) and signed word at +8 equals y (0x004011c6 / 0x004011cb), stores
// 0xffff in the word at +0 (0x004011cd). No return value.
typedef void(__cdecl* FreeRecordAtPos_t)(int, int);
FreeRecordAtPos_t FreeRecordAtPos_orig;
void __cdecl FreeRecordAtPos_re(int x, int y) {
    for (unsigned rec = 0x0056d1d8; rec + 8 < 0x0056e950; rec += 0x3c) {
        if (ref<short>(rec + 6) != x) continue;
        if (ref<short>(rec + 8) != y) continue;
        ref<unsigned short>(rec) = 0xffff;
    }
}

// 0x00409cb0  pushTripleEntry(a, b, c): when the signed count dword 0x005a9cd4 is below 0x100 (0x00409cb5 / `jge` at
// 0x00409cba), stores a, b, c at index count of the dword arrays 0x00586b50, 0x00586fa8 and 0x005a8834
// (0x00409cc4..0x00409cd6) and increments the count (0x00409cdd / 0x00409cde); otherwise does nothing.
typedef void(__cdecl* PushTriple_t)(int, int, int);
PushTriple_t PushTriple_orig;
void __cdecl PushTriple_re(int a, int b, int c) {
    const int n = ref<int>(0x005a9cd4);
    if (n >= 0x100) return;
    ref<int>(0x00586b50 + n * 4) = a;
    ref<int>(0x00586fa8 + n * 4) = b;
    ref<int>(0x005a8834 + n * 4) = c;
    ref<int>(0x005a9cd4) = n + 1;
}

// 0x0040e5b0  rankValue(v): q = v / 200 truncated toward zero (multiply by 0x51eb851f, `sar edx, 6`, add the sign
// bit, 0x0040e5b4..0x0040e5c3). Returns 0 when q == 0 (0x0040e5c5), 1 when q <= 2 (signed `jg` at 0x0040e5cd, so
// every negative q too), 2 when q <= 5 (0x0040e5d8), else 3 + (q > 9) (`setg` at 0x0040e5e5).
typedef int(__cdecl* RankValue_t)(int);
RankValue_t RankValue_orig;
int __cdecl RankValue_re(int v) {
    const int q = v / 200;
    if (q == 0) return 0;
    if (q <= 2) return 1;
    if (q <= 5) return 2;
    return q > 9 ? 4 : 3;
}

// 0x00467560  appendEndearment(n): appends one of four fixed strings to the text buffer 0x0051a068, selected by n & 3
// (0x00467565) through the jump table 0x004675bc: 0 -> 0x004e1b78, 1 -> 0x004e1b70, 2 -> 0x004e1b68, 3 -> 0x004e1b5c.
// The `ja` at 0x0046756c (n & 3 above 3) cannot be taken.
typedef void(__cdecl* AppendEndearment_t)(int);
AppendEndearment_t AppendEndearment_orig;
void __cdecl AppendEndearment_re(int n) {
    static const unsigned kText[4] = {0x004e1b78, 0x004e1b70, 0x004e1b68, 0x004e1b5c};
    appendText(reinterpret_cast<const char*>(kText[n & 3]));
}

// 0x004532a0  appendRatingLabel(v): appends a label for v to the text buffer 0x0051a068. Negative v (`jge` at 0x004532a8
// not taken) selects 0x004d3174. Otherwise q = v / 25 (multiply by 0x51eb851f, `sar edx, 3`, 0x004532b1..0x004532c0);
// q above 3 (unsigned `ja` at 0x004532c5) selects 0x004d3144, and the jump table 0x00453314 maps q 0 -> 0x004d316c,
// 1 -> 0x004d3164, 2 and 3 -> 0x004d3154.
typedef void(__cdecl* AppendRatingLabel_t)(int);
AppendRatingLabel_t AppendRatingLabel_orig;
void __cdecl AppendRatingLabel_re(int v) {
    unsigned text;
    if (v < 0) {
        text = 0x004d3174;
    } else {
        const int q = v / 25;
        if (q == 0) text = 0x004d316c;
        else if (q == 1) text = 0x004d3164;
        else if (q <= 3) text = 0x004d3154;
        else text = 0x004d3144;
    }
    appendText(reinterpret_cast<const char*>(text));
}

// 0x00405970  addHabitatSlot(x, y, kind): finds the first of the 128 slots of 0x14 bytes at 0x00572cb0 whose byte at
// +0x12 is 0xff (0x00405973..0x00405986) and returns -1 when there is none (0x00405988). Otherwise it fills slot i:
// +0 = (x << 10) + 0x200 and +4 = (y << 10) + 0x200 (0x00405999..0x004059b3), +0x11 = the low byte of
// Random::range(4) on the generator 0x00822d9c (0x004059b9..0x004059c9), +0xe word = 0x10c (0x004059cf), +8 = -20
// (0x004059d8), +0x12 = kind's low byte (0x004059e2), +0xc word = 0xffff (0x004059e8); and returns i (0x004059f1).
typedef int(__cdecl* AddHabitatSlot_t)(int, int, int);
AddHabitatSlot_t AddHabitatSlot_orig;
int __cdecl AddHabitatSlot_re(int x, int y, int kind) {
    int i = 0;
    for (; i < 128; ++i)
        if (ref<unsigned char>(0x00572cc2 + i * 0x14) == 0xff) break;
    if (i == 128) return -1;
    const unsigned slot = 0x00572cb0 + i * 0x14;
    ref<int>(slot + 0) = (x << 10) + 0x200;
    ref<int>(slot + 4) = (y << 10) + 0x200;
    const int r = kRange(kRng, 0, 4);
    ref<unsigned char>(slot + 0x11) = static_cast<unsigned char>(r);
    ref<unsigned short>(slot + 0xe) = 0x10c;
    ref<int>(slot + 8) = -20;
    ref<unsigned char>(slot + 0x12) = static_cast<unsigned char>(kind);
    ref<unsigned short>(slot + 0xc) = 0xffff;
    return i;
}

// 0x00407400  canStep(x, y, dir): 1 when a walker on tile (x, y) can step to its neighbour in direction dir, else 0.
// It returns 0 when the type t of (x, y) (0x00407410) has a type byte +2 (0x00578372 + t*0x30, t sign-extended) <= 0
// (signed `jle` at 0x00407428), when t is 0x16 (0x0040742d) or 0x15 (0x00407432), when the neighbour (nx, ny) =
// (x + dx[dir], y + dy[dir]) (0x00407434..0x00407448) has bit 0x100 or 0x20 of its flag word 0x0053caf0 set
// (`and esi, 0x120` / `jne` at 0x0040745b..0x00407464), when the neighbour's type byte +2 is <= 0 (0x0040747b), or
// when tileBlocked(nx, ny) (0x0040bf60, called at 0x0040747f) is non-zero (0x00407489). The neighbour's flags and
// type are read before tileBlocked checks the bounds.
typedef int(__cdecl* CanStep_t)(int, int, int);
CanStep_t CanStep_orig;
int __cdecl CanStep_re(int x, int y, int dir) {
    const signed char t = typeAt(x, y);
    if (typeByte2(t) <= 0) return 0;
    if (t == 0x16 || t == 0x15) return 0;
    const int nx = x + dirDx(dir);
    const int ny = y + dirDy(dir);
    if (ref<unsigned short>(0x0053caf0 + cell(nx, ny) * 2) & 0x120) return 0;
    if (typeByte2(typeAt(nx, ny)) <= 0) return 0;
    if (kTileBlocked(nx, ny) != 0) return 0;
    return 1;
}

// 0x00407340  nearestHole(x, y): the 1-based number h of the hole closest to (x, y), or -1 (0x0040734a) when no
// distance is below 0xffff (initial best, 0x00407345). For h = 1..18 the hole record r = 0x00575cc0 + (h-1)*0x208
// (0x0040735c, step 0x208 below 0x00578150 at 0x004073dd..0x004073ed) and the pin entry p = 0x0059aea8 + (h-1)*0x18
// (0x00407357, step 0x18 at 0x004073e4) give up to three candidate distances, each from distance (0x0040acd0):
// distance(r[+0] - x, r[+4] - y) (0x00407361..0x00407374), distance(r[+0x10] - x, r[+0x14] - y) (0x00407386..
// 0x0040739a) and, when p[+0] != -1 (0x004073af / 0x004073b2), distance(x - (p[+0] >> 10), y - (p[+4] >> 10))
// (arithmetic shifts, 0x004073b4..0x004073cb). A candidate replaces the best only when strictly smaller (signed
// `jge` at 0x0040737e, 0x004073a4, 0x004073d5), so ties keep the earlier hole.
typedef int(__cdecl* NearestHole_t)(int, int);
NearestHole_t NearestHole_orig;
int __cdecl NearestHole_re(int x, int y) {
    int best = 0xffff;
    int result = -1;
    for (int h = 1; h <= 18; ++h) {
        const unsigned r = 0x00575cc0 + (h - 1) * 0x208;
        const unsigned p = 0x0059aea8 + (h - 1) * 0x18;
        int d = kDistance(ref<int>(r + 0) - x, ref<int>(r + 4) - y);
        if (d < best) { best = d; result = h; }
        d = kDistance(ref<int>(r + 0x10) - x, ref<int>(r + 0x14) - y);
        if (d < best) { best = d; result = h; }
        if (ref<int>(p) != -1) {
            d = kDistance(x - (ref<int>(p) >> 10), y - (ref<int>(p + 4) >> 10));
            if (d < best) { best = d; result = h; }
        }
    }
    return result;
}

}  // namespace

SG_HOOK("golf_clean.exe", 0x0046E7B0, postEvent, PostEvent_re, PostEvent_orig);
SG_HOOK("golf_clean.exe", 0x0042F630, relaxTile42f630, RelaxTile_re, RelaxTile_orig);
SG_HOOK("golf_clean.exe", 0x004011B0, freeRecordAtPos, FreeRecordAtPos_re, FreeRecordAtPos_orig);
SG_HOOK("golf_clean.exe", 0x00409CB0, pushTripleEntry, PushTriple_re, PushTriple_orig);
SG_HOOK("golf_clean.exe", 0x0040E5B0, rankValue, RankValue_re, RankValue_orig);
SG_HOOK("golf_clean.exe", 0x00467560, appendEndearment, AppendEndearment_re, AppendEndearment_orig);
SG_HOOK("golf_clean.exe", 0x004532A0, appendRatingLabel, AppendRatingLabel_re, AppendRatingLabel_orig);
SG_HOOK("golf_clean.exe", 0x00405970, addHabitatSlot, AddHabitatSlot_re, AddHabitatSlot_orig);
SG_HOOK("golf_clean.exe", 0x00407400, canStep, CanStep_re, CanStep_orig);
SG_HOOK("golf_clean.exe", 0x00407340, nearestHole, NearestHole_re, NearestHole_orig);

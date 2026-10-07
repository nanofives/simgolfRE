// C3 batch c3x (2026-10-07) of golf_clean.exe: functions the test scenarios never reach but that have static callers:
// the flag-array helpers (flagIsSet / flagSet / flagToggle49ee90), the three Widget quad setters
// (0x00476340/70/a0), the polygon-edge DDA setup polyEdgeStep, and the nearest-palette-index search Palette::nearest.
// Hand-written from the disassembly (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr> --list) and the C2
// transcriptions; each body cites the address of every global, offset and callee it uses. __thiscall is emulated with
// __fastcall (ecx = this, edx unused), and so are the virtual methods reached through an object's vtable.
#include "hooks.h"

namespace {

template <typename T> T at(unsigned addr) { return *reinterpret_cast<const T*>(addr); }
template <typename T> T field(const void* p, unsigned off) {
    return *reinterpret_cast<const T*>(reinterpret_cast<const char*>(p) + off);
}
template <typename T> void setField(void* p, unsigned off, T v) {
    *reinterpret_cast<T*>(reinterpret_cast<char*>(p) + off) = v;
}
// 32-bit wrapping add (lea keeps the low dword).
inline int add32(int a, int b) { return static_cast<int>(static_cast<unsigned>(a) + static_cast<unsigned>(b)); }

// --- Flag toggle (ui). flagIsSet 0x0049ef80 and flagSet 0x0049eef0 moved to batch c3u; flagToggle49ee90 stays here
// and calls them through their original addresses (so on a build where c3u is linked, the A/B needs
// SIMGOLF_HOOKS_OFF=0049ef80,0049eef0 for the original arm). flagIsSet(this, id) returns whether flag `id` is set;
// flagSet(this, id, value) sets or clears it. ---
typedef int(__fastcall* FlagIsSet_t)(void*, void*, int);
typedef void(__fastcall* FlagSet_t)(void*, void*, int, int);

// 0x0049ee90  flagToggle49ee90(id): if flagIsSet(id) then flagSet(id, 0) else flagSet(id, 1) (je at 0x0049eea0).
// Calls flagIsSet 0x0049ef80 and flagSet 0x0049eef0 through their original addresses.
typedef void(__fastcall* FlagToggle_t)(void*, void*, int);
FlagToggle_t FlagToggle_orig;
const FlagIsSet_t kFlagIsSet = reinterpret_cast<FlagIsSet_t>(0x0049ef80);
const FlagSet_t kFlagSet = reinterpret_cast<FlagSet_t>(0x0049eef0);
void __fastcall FlagToggle_re(void* self, void*, int id) {
    if (kFlagIsSet(self, 0, id))
        kFlagSet(self, 0, id, 0);
    else
        kFlagSet(self, 0, id, 1);
}

// --- Widget quad setters (ui). Each writes its four dword arguments into four fields spaced 0x10 apart, starting at
// this+BASE (0x70 / 0x74 / 0x78). Arg order from the reads [esp+4..+0x10] and the stores at 0x00476348.. ---
typedef void(__fastcall* SetQuad_t)(void*, void*, unsigned, unsigned, unsigned, unsigned);
template <unsigned BASE>
void setQuad(void* self, unsigned a, unsigned b, unsigned c, unsigned d) {
    setField<unsigned>(self, BASE + 0x00, a);
    setField<unsigned>(self, BASE + 0x10, b);
    setField<unsigned>(self, BASE + 0x20, c);
    setField<unsigned>(self, BASE + 0x30, d);
}
// 0x00476340  Widget_setQuad70: a->+0x70, b->+0x80, c->+0x90, d->+0xa0.
SetQuad_t SetQuad70_orig;
void __fastcall SetQuad70_re(void* self, void*, unsigned a, unsigned b, unsigned c, unsigned d) { setQuad<0x70>(self, a, b, c, d); }
// 0x00476370  Widget_setQuad74: a->+0x74, b->+0x84, c->+0x94, d->+0xa4.
SetQuad_t SetQuad74_orig;
void __fastcall SetQuad74_re(void* self, void*, unsigned a, unsigned b, unsigned c, unsigned d) { setQuad<0x74>(self, a, b, c, d); }
// 0x004763a0  Widget_setQuad78: a->+0x78, b->+0x88, c->+0x98, d->+0xa8.
SetQuad_t SetQuad78_orig;
void __fastcall SetQuad78_re(void* self, void*, unsigned a, unsigned b, unsigned c, unsigned d) { setQuad<0x78>(self, a, b, c, d); }

// --- polyEdgeStep (render). Finds the next edge of the polygon that spans a scanline, starting at vertex `idx`, and
// fills the edge record's DDA fields. Globals: end vertex index g_0083d350, vertex table g_0083d358 (x at +0, y at +4,
// 8 bytes per vertex), vertex count g_0083d378. Edge record fields: rows +4, end vertex +8, x +0xc, x step +0x10,
// x sign +0x14, error +0x18, error step +0x1c, error reset +0x20 (writes at 0x00492f0f..0x00492f9c). ---
inline int vtxX(unsigned verts, int i) { return at<int>(verts + i * 8); }      // [verts + i*8]
inline int vtxY(unsigned verts, int i) { return at<int>(verts + i * 8 + 4); }  // [verts + i*8 + 4]
typedef int(__cdecl* PolyEdgeStep_t)(void*, int);
PolyEdgeStep_t PolyEdgeStep_orig;
int __cdecl PolyEdgeStep_re(void* edge, int idx) {
    if (idx == at<int>(0x0083d350)) return 0;              // je 0x00492edf: already at the end vertex
    const int dir = field<int>(edge, 0);                  // [edge] read once at 0x00492ee5
    for (;;) {
        const int hi = at<int>(0x0083d378) - 1;           // g_vcount - 1
        int v = add32(dir, idx);                          // lea at 0x00492eed
        if (v > hi) v = 0;                                 // jle at 0x00492ef3
        else if (v < 0) v = hi;                            // jge at 0x00492efb
        const unsigned verts = at<unsigned>(0x0083d358);
        const int dy = vtxY(verts, v) - vtxY(verts, idx);  // 0x00492f05..0x00492f0d
        setField<int>(edge, 4, dy);                        // edge->rows
        if (dy == 0) {                                     // jne 0x00492f12
            idx = v;
            if (v == at<int>(0x0083d350)) return 0;        // jne 0x00492f1e: reached the end vertex
            continue;
        }
        if (dy < 0) return 0;                              // jge 0x00492f29: edge runs upward -> skip
        setField<int>(edge, 8, v);                         // edge->endVtx
        setField<int>(edge, 0xc, vtxX(verts, idx));        // edge->x = start x
        const int dx = vtxX(verts, v) - vtxX(verts, idx);
        if (dx >= 0) {                                     // jns 0x00492f4f
            setField<int>(edge, 0x14, 1);                  // x sign = +1
            setField<int>(edge, 0x10, dx / dy);            // x step
            setField<int>(edge, 0x18, 0);                  // error
            setField<int>(edge, 0x1c, dx % dy);            // error step
        } else {
            const int adx = -dx;                           // neg esi at 0x00492f62
            setField<int>(edge, 0x14, -1);                 // x sign = -1
            setField<int>(edge, 0x18, 1 - dy);             // error = 1 - rows
            setField<int>(edge, 0x10, -(adx / dy));        // x step
            setField<int>(edge, 0x1c, adx % dy);           // error step
        }
        setField<int>(edge, 0x20, dy);                     // error reset = rows
        return 1;
    }
}

// --- Palette::nearest (render). Returns the palette index closest to (r, g, b) by squared RGB distance. m_pal at
// this+4; null -> return 7 (0x00483436/0x00483438). It reads 256 entries through the palette object's virtual slot
// +0x10 (getEntries(out, first, count), call at 0x00483458) into a local 0x300-byte buffer (3 bytes per entry: r, g,
// b). With reserved == 0 it scans all 256 (0x00483464). Otherwise it marks indices claimed by the five range records
// at this+8 (0x10 bytes each: id +0, start byte +8, count byte +9; id != -1 is active, 0x00483523) and scans only
// 10..0xf5 skipping the marked ones (0xa at 0x00483558, 0xf6 at 0x004835d2). Returns the index of the least distance. ---
typedef int(__fastcall* Nearest_t)(void*, void*, unsigned char, unsigned char, unsigned char, int);
typedef void(__fastcall* GetEntries_t)(void*, void*, unsigned char*, int, int);
Nearest_t Nearest_orig;
int __fastcall Nearest_re(void* self, void*, unsigned char r, unsigned char g, unsigned char b, int reserved) {
    void* pal = field<void*>(self, 4);
    if (pal == 0) return 7;
    unsigned char buf[0x300];
    GetEntries_t getEntries = reinterpret_cast<GetEntries_t>(at<void*>(field<unsigned>(pal, 0) + 0x10));
    getEntries(pal, 0, buf, 0, 0x100);
    int best = 200000;
    int bestIdx = 0;
    if (reserved == 0) {
        for (int i = 0; i < 0x100; i++) {
            const int db = buf[i * 3 + 2] - b, dr = buf[i * 3] - r, dg = buf[i * 3 + 1] - g;
            const int d = db * db + dr * dr + dg * dg;
            if (d < best) { best = d; bestIdx = i; }
        }
    } else {
        int used[0x100];
        for (int i = 0; i < 0x100; i++) used[i] = 0;
        char* ranges = reinterpret_cast<char*>(self) + 8;
        for (int i = 0; i < 5; i++) {
            if (field<int>(ranges + i * 0x10, 0) != -1) {
                const int start = field<unsigned char>(ranges + i * 0x10, 8);
                const int count = field<unsigned char>(ranges + i * 0x10, 9);
                for (int j = start; j < start + count; j++) used[j] = 1;
            }
        }
        for (int i = 10; i < 0xf6; i++) {
            if (used[i]) continue;
            const int db = buf[i * 3 + 2] - b, dr = buf[i * 3] - r, dg = buf[i * 3 + 1] - g;
            const int d = db * db + dr * dr + dg * dg;
            if (d < best) { best = d; bestIdx = i; }
        }
    }
    return bestIdx;
}

}  // namespace

// 0x0049ee90  flagToggle49ee90
SG_HOOK("golf_clean.exe", 0x0049ee90, flagToggle49ee90, FlagToggle_re, FlagToggle_orig);
// 0x00476340  Widget_setQuad70
SG_HOOK("golf_clean.exe", 0x00476340, Widget_setQuad70, SetQuad70_re, SetQuad70_orig);
// 0x00476370  Widget_setQuad74
SG_HOOK("golf_clean.exe", 0x00476370, Widget_setQuad74, SetQuad74_re, SetQuad74_orig);
// 0x004763a0  Widget_setQuad78
SG_HOOK("golf_clean.exe", 0x004763a0, Widget_setQuad78, SetQuad78_re, SetQuad78_orig);
// 0x00492ed0  polyEdgeStep
SG_HOOK("golf_clean.exe", 0x00492ed0, polyEdgeStep, PolyEdgeStep_re, PolyEdgeStep_orig);
// 0x00483420  Palette::nearest
SG_HOOK("golf_clean.exe", 0x00483420, Palette__nearest, Nearest_re, Nearest_orig);

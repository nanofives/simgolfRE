// C3 batch c3bs (round 17, 2026-10-08) of golf_clean.exe. Seven functions from four subsystems: the two IJG
// libjpeg 6a master methods that have no call xref (they are reached only through the function pointers
// jinit_c_master_control stores), the shading spreader pair, the RLE surface blitter, the object spawner and
// the walker/roster name builder.
//
// Every body is hand-written from the disassembly (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr>
// --list) and cites the address of each instruction it reproduces. Callees run through their original
// addresses:
//   spreadShade    -> tileBlocked 0x0040bf60 (C3, golf_math.cpp), approxDistance 0x00467170 (C3, golf_math.cpp)
//                     and clamp 0x00467130 (C3, c3f.cpp)
//   spreadAllShade -> spreadShade 0x004615f0 (IN THIS BATCH: re-run with its hook off)
//   spawnObject    -> Random::range 0x0045c1e0 (C3, c3f.cpp); both arms advance the same restored seed
//   pass_startup   -> cinfo->marker->write_frame_header / write_scan_header (fixture callbacks)
//   finish_pass    -> cinfo->entropy->finish_pass (a fixture callback)
//   Surface::blitRLE -> the locked surface's vtable slots +0x20 and +0x24 (fixture callbacks)
//
//   pass_startup     0x004b5ca0   finish_pass      0x004b5cd0   spreadShade   0x004615f0
//   spreadAllShade   0x004616f0   Surface::blitRLE 0x00482990   spawnObject   0x004011e0
//   pickGolferName   0x00467600
#include "hooks.h"

namespace {

// ---- helpers --------------------------------------------------------------------------------------------------
static inline void* field_ptr(void* base, unsigned off) {
    return *reinterpret_cast<void**>(reinterpret_cast<char*>(base) + off);
}
static inline int& field_i(void* base, unsigned off) {
    return *reinterpret_cast<int*>(reinterpret_cast<char*>(base) + off);
}

// Callees reached through their original addresses.
typedef int(__cdecl* Int2_t)(int, int);
typedef int(__cdecl* Int3_t)(int, int, int);
typedef void(__cdecl* Spread_t)(int, int, int);
typedef int(__fastcall* Range_t)(void* ecx, void* edx, int n);
const Int2_t kTileBlocked = reinterpret_cast<Int2_t>(0x0040bf60);      // C3, golf_math.cpp
const Int2_t kApproxDistance = reinterpret_cast<Int2_t>(0x00467170);   // C3, golf_math.cpp
const Int3_t kClamp = reinterpret_cast<Int3_t>(0x00467130);            // C3, c3f.cpp
const Spread_t kSpreadShade = reinterpret_cast<Spread_t>(0x004615f0);  // this batch
const Range_t kRange = reinterpret_cast<Range_t>(0x0045c1e0);          // C3, c3f.cpp

// ---- IJG libjpeg 6a: the two jcmaster.c methods with no call xref ----------------------------------------------
// jpeg_compress_struct: optimize_coding +0xb0, master +0x144, marker +0x154, entropy +0x164.
// my_comp_master (jcmaster.c): prepare_for_pass +0, pass_startup +4, finish_pass +8, call_pass_startup +0xc,
// is_last_pass +0x10, pass_type +0x14, pass_number +0x18, total_passes +0x1c, scan_number +0x20.
// jpeg_marker_writer: write_frame_header +8, write_scan_header +0xc. jpeg_entropy_encoder: finish_pass +8.
typedef void(__cdecl* VoidCinfo_t)(void* cinfo);

// 0x004b5ca0  pass_startup(cinfo): clear the one-shot flag, then write the frame header and the scan header.
// The function is never CALLED in the image: jinit_c_master_control stores its address into the master object's
// pass_startup slot with `mov dword ptr [edi + 4], 0x4b5ca0` at 0x004b5202, and jpeg_write_scanlines dispatches
// through that slot. cinfo->master (cinfo+0x144, read at 0x004b5ca6) gets call_pass_startup (+0xc) set to 0
// (0x004b5cac) so the hook runs once. cinfo->marker (cinfo+0x154) is then read TWICE, once per call
// (0x004b5cb3 and 0x004b5cbc), and the two methods are called with cinfo as their only argument:
// write_frame_header through marker+8 (0x004b5cb9) and write_scan_header through marker+0xc (0x004b5cc3).
// Both calls are __cdecl: the two `push esi` at 0x004b5ca5 and 0x004b5cc2 are popped together by the
// `add esp, 8` at 0x004b5cc6.
VoidCinfo_t PassStartup_orig;
void __cdecl PassStartup_re(void* cinfo) {
    field_i(field_ptr(cinfo, 0x144), 0xc) = 0;                                      // 0x004b5cac
    reinterpret_cast<VoidCinfo_t>(field_ptr(field_ptr(cinfo, 0x154), 8))(cinfo);    // 0x004b5cb9
    reinterpret_cast<VoidCinfo_t>(field_ptr(field_ptr(cinfo, 0x154), 0xc))(cinfo);  // 0x004b5cc3
}

// 0x004b5cd0  finish_pass_master(cinfo): end the entropy pass, then advance the master's pass bookkeeping.
// Like pass_startup it has no call xref: jinit_c_master_control stores it with
// `mov dword ptr [edi + 8], 0x4b5cd0` at 0x004b5209 and the library dispatches through master->finish_pass.
// cinfo->entropy (cinfo+0x164, 0x004b5cd7) and cinfo->master (cinfo+0x144, 0x004b5cdd) are read before the
// call; (*entropy->finish_pass)(cinfo) runs through entropy+8 (0x004b5ce3, __cdecl, `add esp, 4` at
// 0x004b5ce9). The three-way switch on master->pass_type (+0x14, read at 0x004b5ce6) is compiled as
// `sub eax, 0` / `je` (0x004b5cec), `dec eax` / `je` (0x004b5cf1) and `dec eax` / `jne` (0x004b5cf4):
//   pass_type 0 (0x004b5d2a): pass_type = 2; scan_number (+0x20) is incremented only when
//                             cinfo->optimize_coding (+0xb0) is zero (0x004b5d31 / 0x004b5d39 / 0x004b5d3b).
//   pass_type 1 (0x004b5d19): pass_type = 2 and nothing else.
//   pass_type 2 (0x004b5cf7): scan_number is incremented either way (0x004b5d0b / 0x004b5d3b), and pass_type
//                             is set to 1 only when optimize_coding is non-zero (0x004b5cfd / 0x004b5cff /
//                             0x004b5d04).
//   anything else (0x004b5d3e): no pass_type and no scan_number change.
// Every arm ends at the common tail that increments pass_number (+0x18): 0x004b5d10 / 0x004b5d1c / 0x004b5d3e.
VoidCinfo_t FinishPass_orig;
void __cdecl FinishPass_re(void* cinfo) {
    void* entropy = field_ptr(cinfo, 0x164);                                 // 0x004b5cd7
    void* master = field_ptr(cinfo, 0x144);                                  // 0x004b5cdd
    reinterpret_cast<VoidCinfo_t>(field_ptr(entropy, 8))(cinfo);             // 0x004b5ce3
    switch (field_i(master, 0x14)) {                                         // 0x004b5ce6
    case 0:
        field_i(master, 0x14) = 2;                                           // 0x004b5d2a
        if (field_i(cinfo, 0xb0) == 0) field_i(master, 0x20) += 1;           // 0x004b5d31 / 0x004b5d3b
        break;
    case 1:
        field_i(master, 0x14) = 2;                                           // 0x004b5d1c
        break;
    case 2:
        if (field_i(cinfo, 0xb0) != 0) field_i(master, 0x14) = 1;            // 0x004b5cfd / 0x004b5d04
        field_i(master, 0x20) += 1;                                          // 0x004b5d0b / 0x004b5d3b
        break;
    default:
        break;
    }
    field_i(master, 0x18) += 1;                                              // 0x004b5d3e
}

// ---- terrain shading -------------------------------------------------------------------------------------------
// Two 50x50 byte grids of shading accumulators: the positive one at 0x0082376c and the negative one at
// 0x00822da0, both indexed row * 50 + column.

// 0x004615f0  spreadShade(row, col, value): stamp `value` over the 15x15 block of cells centred on (row, col),
// falling off with distance. The amount spread is value * 2 / 3, computed by the magic-number division
// `imul 0x55555556` on value + value at 0x004615fa..0x00461627 (signed, truncated toward zero).
// The outer loop (0x00461631 / 0x004616de) runs 15 times with a row offset from -7 to +7 (the slot at
// esp+0x24 starts at -7, 0x0046160b) and a weight coordinate from -21 to +21 in steps of 3 (the slot at
// esp+0x10, 0x00461617 / 0x004616cc / 0x004616d3). The inner loop (0x00461647 / 0x004616c2) does the same for
// the column, with the grid byte offset held in edi + esi: edi is 50 * (row - 7) at entry (0x00461613 /
// 0x00461624 / 0x00461629) and grows by 50 per row (0x004616d0), esi is col - 7 (0x00461644) and grows by 1.
// A cell whose tileBlocked 0x0040bf60 is non-zero is skipped (0x0046164d / 0x00461657). Otherwise the divisor
// is approxDistance(rowWeight, colWeight) + 2 (0x0046165f / 0x00461664) and the amount decides which grid is
// touched: a strictly positive amount is ADDED to 0x0082376c (0x0046166c / 0x0046166f / 0x00461671 /
// 0x0046167a / 0x0046168e), an amount of zero or less is SUBTRACTED from 0x00822da0 (0x00461697 / 0x004616a0 /
// 0x004616a9 / 0x004616b4). Both go through clamp(v, 0, 0xff) 0x00467130 (0x00461686 / 0x004616ac).
Spread_t SpreadShade_orig;
void __cdecl SpreadShade_re(int row, int col, int value) {
    const int amount = (value * 2) / 3;                             // 0x004615fa..0x00461627
    unsigned char* const plus = reinterpret_cast<unsigned char*>(0x0082376c);
    unsigned char* const minus = reinterpret_cast<unsigned char*>(0x00822da0);
    for (int di = 0; di < 15; di++) {                               // 0x00461631 .. 0x004616de
        const int r = row - 7 + di;
        const int rw = -21 + 3 * di;
        for (int dj = 0; dj < 15; dj++) {                           // 0x00461647 .. 0x004616c2
            const int c = col - 7 + dj;
            const int cw = -21 + 3 * dj;
            if (kTileBlocked(r, c) != 0) continue;                  // 0x0046164d / 0x00461657
            const int d = kApproxDistance(rw, cw) + 2;              // 0x0046165f / 0x00461664
            const int cell = r * 50 + c;
            if (amount > 0) {                                       // 0x0046166c / 0x0046166f
                plus[cell] = static_cast<unsigned char>(kClamp(plus[cell] + amount / d, 0, 0xff));
            } else {
                minus[cell] = static_cast<unsigned char>(kClamp(minus[cell] - amount / d, 0, 0xff));
            }
        }
    }
}

// 0x004616f0  spreadAllShade(): rebuild both shading grids from the three 50x50 source grids.
// First the weights at 0x0053ea24 are summed as unsigned bytes over 50 rows of 50 (0x004616ff / 0x00461705 /
// 0x0046170d, the row pointer stopping at 0x0053f3e8). Both accumulator grids are then cleared with
// `rep stosd` of 0x271 dwords each, 0x0082376c first (0x00461715..0x00461724) and 0x00822da0 second
// (0x00461726..0x00461730) - 625 dwords is the whole 2500-byte grid, and the clears happen before the
// early return. A total below 10 ends the function there (the `cmp ebx, 0xa` at 0x00461721 with its `jl` at
// 0x00461732); a total above 0x6d6 is capped at 0x6d6 (0x00461738 / 0x0046173e / 0x00461740).
// The spread loop (0x00461751 / 0x00461759 .. 0x004617d1) then walks the same 50x50 index: a non-zero byte of
// the positive source 0x005a6378 (0x00461759 / 0x00461762) calls spreadShade with byte * 20000 / total (the
// four `lea eax,[eax+eax*4]` plus `shl eax,5` at 0x00461769..0x00461779), and a non-zero byte of the negative
// source 0x0056c7e4 (0x00461786 / 0x0046178f) calls it with byte * -40000 / total (the negate-times-five at
// 0x00461796..0x004617a8). Both divisions are signed (`cdq` / `idiv ebx`). Row ends are the pointer compares
// at 0x004617ba (column) and 0x004617c7 (row, stopping at 0x005a6d3c).
typedef void(__cdecl* Void0_t)();
Void0_t SpreadAllShade_orig;
void __cdecl SpreadAllShade_re() {
    const unsigned char* const weights = reinterpret_cast<const unsigned char*>(0x0053ea24);
    int total = 0;
    for (int i = 0; i < 50; i++)                                    // 0x004616fb .. 0x00461713
        for (int j = 0; j < 50; j++) total += weights[i * 50 + j];  // 0x004616ff / 0x00461702
    for (int k = 0; k < 625; k++) {                                 // 0x00461724 / 0x00461730
        reinterpret_cast<unsigned*>(0x0082376c)[k] = 0;
        reinterpret_cast<unsigned*>(0x00822da0)[k] = 0;
    }
    if (total < 10) return;                                         // 0x00461721 / 0x00461732
    if (total > 0x6d6) total = 0x6d6;                               // 0x00461738 / 0x00461740
    const unsigned char* const pos = reinterpret_cast<const unsigned char*>(0x005a6378);
    const unsigned char* const neg = reinterpret_cast<const unsigned char*>(0x0056c7e4);
    for (int i = 0; i < 50; i++) {                                  // 0x00461751 .. 0x004617d1
        for (int j = 0; j < 50; j++) {                              // 0x00461759 .. 0x004617bd
            const int cell = i * 50 + j;
            if (pos[cell] != 0) kSpreadShade(i, j, pos[cell] * 20000 / total);     // 0x0046177e
            if (neg[cell] != 0) kSpreadShade(i, j, neg[cell] * -40000 / total);    // 0x004617b1
        }
    }
}

// ---- render ------------------------------------------------------------------------------------------------
// 0x00482990  Surface::blitRLE(chunk, unused, display): decode a byte run-length image into the display's
// locked surface. __thiscall with three stack arguments (`ret 0xc` at 0x00482a70), emulated here with
// __fastcall. The destination comes from display+4 (0x004829a6), whose vtable slot +0x20 is called with no
// argument to lock it (0x004829ab) and whose slot +0x24 is called with 1 to unlock it at the end
// (0x00482a5f / 0x00482a66). The image geometry comes from this+0x58: width is the 16-bit field +8
// (0x004829cf / 0x004829d5) and height the 16-bit field +0xa (0x004829ae / 0x004829b9); this+0x58 is re-read
// once per row and the row count is kept at esp+0x14 (0x004829cb / 0x00482a4a / 0x00482a53).
// The payload starts at chunk+6 (0x004829a3) and one byte is skipped at the start of EVERY row
// (`inc ebx` at 0x004829d4). A height of zero skips the whole loop (0x004829c0 / 0x004829c2) and a width of
// zero skips that row's inner loop (0x004829d9 / 0x004829df). Each run is one signed count byte
// (0x004829e1): a negative count copies -count literal bytes (`neg cl` then `movsx ebp, cl` at 0x004829e8 /
// 0x004829ea, `rep movsd` + `rep movsb` at 0x004829f8 / 0x00482a05, the source advanced by the same amount at
// 0x00482a07), a count of zero or more repeats the single following byte count times (`movsx eax, byte [ebx]`
// at 0x00482a0b, the byte spread over a dword at 0x00482a10..0x00482a20 and written with `rep stosd` +
// `rep stosb` at 0x00482a2a / 0x00482a34, the source advanced by one at 0x00482a2e). The destination cursor
// is carried in the caller's third-argument slot (0x00482a01 / 0x00482a23 / 0x00482a36) and the remaining
// width is decremented by the run length and tested against zero, not against a lower bound
// (0x00482a3a / 0x00482a3e / 0x00482a44). The return value is always 0 (`xor eax, eax` at 0x00482a6a).
typedef void*(__fastcall* LockSlot_t)(void* surf, void* edx);
typedef void(__fastcall* UnlockSlot_t)(void* surf, void* edx, int flag);
typedef int(__fastcall* BlitRle_t)(void* self, void* edx, unsigned char* chunk, int unused, void* display);
BlitRle_t BlitRLE_orig;
int __fastcall BlitRLE_re(void* self, void*, unsigned char* chunk, int, void* display) {
    unsigned char* p = chunk + 6;                                            // 0x004829a3
    void* surf = field_ptr(display, 4);                                      // 0x004829a6
    void** vtbl = *reinterpret_cast<void***>(surf);
    unsigned char* dst = static_cast<unsigned char*>(
        reinterpret_cast<LockSlot_t>(vtbl[8])(surf, 0));                     // 0x004829ab, slot +0x20
    int rows = *reinterpret_cast<unsigned short*>(
        reinterpret_cast<char*>(field_ptr(self, 0x58)) + 0xa);               // 0x004829b9
    while (rows != 0) {                                                      // 0x004829c0 / 0x00482a53
        p++;                                                                 // 0x004829d4
        int left = *reinterpret_cast<unsigned short*>(
            reinterpret_cast<char*>(field_ptr(self, 0x58)) + 8);             // 0x004829cf / 0x004829d5
        while (left != 0) {                                                  // 0x004829d9 / 0x00482a44
            const signed char n = static_cast<signed char>(*p++);            // 0x004829e1
            int run;
            if (n < 0) {                                                     // 0x004829e6
                // `neg cl` is a byte negation, so a count byte of 0x80 stays 0x80 and sign-extends to -128.
                run = static_cast<signed char>(static_cast<unsigned char>(0u - static_cast<unsigned>(
                    static_cast<unsigned char>(n))));                        // 0x004829e8 / 0x004829ea
                for (int i = 0; i < run; i++) dst[i] = p[i];                 // 0x004829f8 / 0x00482a05
                dst += run;                                                  // 0x004829fc
                p += run;                                                    // 0x00482a07
            } else {
                run = n;                                                     // 0x00482a12
                const unsigned char v = *p;                                  // 0x00482a0b
                for (int i = 0; i < run; i++) dst[i] = v;                    // 0x00482a2a / 0x00482a34
                dst += run;                                                  // 0x00482a32
                p++;                                                         // 0x00482a2e
            }
            left -= run;                                                     // 0x00482a3e
        }
        rows--;                                                              // 0x00482a4e
    }
    void** dvtbl = *reinterpret_cast<void***>(field_ptr(display, 4));        // 0x00482a61 / 0x00482a64
    reinterpret_cast<UnlockSlot_t>(dvtbl[9])(field_ptr(display, 4), 0, 1);   // 0x00482a66, slot +0x24
    return 0;                                                                // 0x00482a6a
}

// ---- sim ---------------------------------------------------------------------------------------------------
// 0x004011e0  spawnObject(lot, kind, a, b): fill the first free record of the 100-entry table at 0x0056d1b8
// (stride 0x3c) and drop it on a free cell of the lot's 19x6 cell map.
// A record is free when its 16-bit type field at +0x20 is -1; the search walks those fields from 0x0056d1d8
// and stops at 0x0056e948 (0x004011e7 / 0x004011f1 / 0x004011f7), returning with nothing written when every
// record is taken (0x004011f9). The record offset is index * 0x3c (0x004011ff / 0x00401207 / 0x00401213).
// Writes: +0 is zeroed (0x00401221), +0x22 takes the low word of `lot` (0x0040122b), +0x20 takes the byte at
// 0x0055d758 + kind * 0x25 sign-extended to 16 bits (0x0040120a / 0x00401232), +0x26 and +0x28 take the low
// words of `a` and `b` (0x0040123e / 0x0040124c), +0x2c is set to 0x140 (0x00401255).
// The placement loop (0x00401264 .. 0x0040129d) draws two numbers from Random::range(4) on the generator at
// 0x00822d9c (0x0045c1e0, thiscall; 0x0040126b and 0x0040127a, each result kept as its low 16 bits at
// 0x0040127a / 0x0040128b) and retries until the cell byte at 0x004e6d70 + lot * 0x74 + row * 6 + column is
// 0xff (0x0040128b / 0x00401291 / 0x00401295). The lot stride 0x74 is built at 0x00401245..0x00401261.
// The accepted pair is stored at +0x30 and +0x34 as ((n << 10) + 0x200) / 2, the `cdq` / `sub` / `sar 1`
// signed halving at 0x004012a1..0x004012ae and 0x004012b6..0x004012c3.
typedef void(__cdecl* Spawn_t)(int lot, int kind, int a, int b);
Spawn_t SpawnObject_orig;
void __cdecl SpawnObject_re(int lot, int kind, int a, int b) {
    int idx = 0;
    const short* type = reinterpret_cast<const short*>(0x0056d1d8);
    while (*type != -1) {                                                     // 0x004011e7
        type = reinterpret_cast<const short*>(reinterpret_cast<const char*>(type) + 0x3c);
        idx++;                                                                // 0x004011ed / 0x004011f0
        if (reinterpret_cast<unsigned>(type) >= 0x0056e948u) return;          // 0x004011f1 / 0x004011f9
    }
    char* const rec = reinterpret_cast<char*>(0x0056d1b8) + idx * 0x3c;
    const signed char kindByte = *(reinterpret_cast<const signed char*>(0x0055d758) + kind * 0x25);
    *reinterpret_cast<int*>(rec + 0) = 0;                                     // 0x00401221
    *reinterpret_cast<short*>(rec + 0x22) = static_cast<short>(lot);          // 0x0040122b
    *reinterpret_cast<short*>(rec + 0x20) = static_cast<short>(kindByte);     // 0x00401232
    *reinterpret_cast<short*>(rec + 0x26) = static_cast<short>(a);            // 0x0040123e
    *reinterpret_cast<short*>(rec + 0x28) = static_cast<short>(b);            // 0x0040124c
    *reinterpret_cast<short*>(rec + 0x2c) = 0x140;                            // 0x00401255
    const unsigned char* const map = reinterpret_cast<const unsigned char*>(0x004e6d70) + lot * 0x74;
    int row, col;
    do {
        row = kRange(reinterpret_cast<void*>(0x00822d9c), 0, 4) & 0xffff;     // 0x0040126b
        col = kRange(reinterpret_cast<void*>(0x00822d9c), 0, 4) & 0xffff;     // 0x0040127a
    } while (map[row * 6 + col] != 0xff);                                     // 0x00401295
    *reinterpret_cast<int*>(rec + 0x30) = ((row << 10) + 0x200) / 2;          // 0x004012a1 / 0x004012ae
    *reinterpret_cast<int*>(rec + 0x34) = ((col << 10) + 0x200) / 2;          // 0x004012b6 / 0x004012c3
}

// ---- golfer / walker roster -----------------------------------------------------------------------------------
// 0x00467600  pickGolferName(index): append one record's display name to the shared text buffer 0x0051a068.
// The record base is 0x00585850 + index * 0x4c, the stride built by `lea ecx,[eax+eax*8]` /
// `lea eax,[eax+ecx*2]` / `shl eax,2` at 0x00467606..0x0046760c (the same table and stride spawnWalker
// 0x00402970 fills).
// When the record's own name byte at +0x2a is non-zero (0x0046760f / 0x0046761b / 0x0046761d), that
// NUL-terminated string is the name and nothing else is read; the address was already taken at 0x00467615.
// Otherwise the type byte at +0x13 (0x00467623) and the flag byte at +0x12 (0x00467629) choose a table slot:
// slot = ((flags >> 3) & 1) - (2 * (signed char)type + 4), built by `movsx` / `and edx, 8` / `shr edx, 3` /
// `lea eax,[eax+eax+4]` / `sub edx, eax` at 0x0046762f..0x0046763c. A type byte of exactly 0xfa short-circuits
// to the fixed string at 0x004d6098 (0x0046763e / 0x00467641 / 0x004676a4). Any other type appends TWO
// strings: the pointer at 0x004c148c + slot * 4 first (0x00467643) and the pointer at 0x004c1464 + slot * 4
// second (0x00467674).
// Every append is the same inlined strcat: `repne scasb` for the source length, `repne scasb` for the end of
// 0x0051a068, then `rep movsd` + `rep movsb` (0x0046764a..0x00467672, 0x0046767b..0x0046769f and
// 0x004676a9..0x004676cf).
typedef void(__cdecl* Pick_t)(int index);
Pick_t PickGolferName_orig;
static void appendToTextBuffer(const char* src) {
    char* dst = reinterpret_cast<char*>(0x0051a068);
    while (*dst != '\0') dst++;
    while ((*dst++ = *src++) != '\0') {}
}
void __cdecl PickGolferName_re(int index) {
    const char* const rec = reinterpret_cast<const char*>(0x00585850) + index * 0x4c;
    if (rec[0x2a] != 0) {                                                     // 0x0046760f / 0x0046761d
        appendToTextBuffer(rec + 0x2a);                                       // 0x004676a9
        return;
    }
    const unsigned char type = static_cast<unsigned char>(rec[0x13]);         // 0x00467623
    const unsigned char flags = static_cast<unsigned char>(rec[0x12]);        // 0x00467629
    const int slot = ((flags & 8) >> 3) - (2 * static_cast<int>(static_cast<signed char>(type)) + 4);
    if (type == 0xfa) {                                                       // 0x0046763e / 0x00467641
        appendToTextBuffer(reinterpret_cast<const char*>(0x004d6098));        // 0x004676a4
        return;
    }
    appendToTextBuffer(*(reinterpret_cast<const char* const*>(0x004c148c) + slot));   // 0x00467643
    appendToTextBuffer(*(reinterpret_cast<const char* const*>(0x004c1464) + slot));   // 0x00467674
}

}  // namespace

// 0x004b5ca0  pass_startup
SG_HOOK("golf_clean.exe", 0x004b5ca0, pass_startup, PassStartup_re, PassStartup_orig);
// 0x004b5cd0  finish_pass
SG_HOOK("golf_clean.exe", 0x004b5cd0, finish_pass, FinishPass_re, FinishPass_orig);
// 0x004615f0  spreadShade
SG_HOOK("golf_clean.exe", 0x004615f0, spreadShade, SpreadShade_re, SpreadShade_orig);
// 0x004616f0  spreadAllShade
SG_HOOK("golf_clean.exe", 0x004616f0, spreadAllShade, SpreadAllShade_re, SpreadAllShade_orig);
// 0x00482990  Surface::blitRLE
SG_HOOK("golf_clean.exe", 0x00482990, Surface_blitRLE, BlitRLE_re, BlitRLE_orig);
// 0x004011e0  spawnObject
SG_HOOK("golf_clean.exe", 0x004011e0, spawnObject, SpawnObject_re, SpawnObject_orig);
// 0x00467600  pickGolferName
SG_HOOK("golf_clean.exe", 0x00467600, pickGolferName, PickGolferName_re, PickGolferName_orig);

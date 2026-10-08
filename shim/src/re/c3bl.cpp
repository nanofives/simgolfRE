// Batch c3bl: jgld.dll non-render - the two libpng 1.0.5 functions batch c3bg left as leads
// (log/c3/c3bg_notes.txt, "Candidates outside the leads that were not taken"):
//   png_do_background           0x00074750 (8131 bytes, the largest body in pngrtran.c, pure)
//   png_do_read_transformations 0x000717d0 (the row-transformation driver, 22 callees)
//
// SG_HOOK addresses are RVAs; every instruction cited in a comment is a VA (0x10000000 + RVA), the
// form the debug build's disassembly prints. jgld.dll is LoadLibrary'd after the shim starts, so
// these hooks install from the LoadLibraryA hook (SgHooksInstallPending, re/hooks.cpp) and diff_hook
// finds them by (module, rva) through SimGolfShim_FindHookM.
//
// Both functions are __cdecl: png_do_background pops nothing (`ret` at 0x10076712, the caller's
// `add esp, 0x30` at 0x10071a94) and png_do_read_transformations likewise (`ret` at 0x10071e78).
//
// Nothing live in the running jgld.dll is touched: no Display, Surface, Palette, Font, the game's
// png_struct / png_info or its heap blocks. Every callee of png_do_read_transformations is reached
// at its own module address, so each comparison is of this code and not of its callees - and because
// all eighteen png_do_* callees carry an SG_HOOK from an earlier batch (c3ao, c3ax, c3bb, c3bg) or
// from this one (png_do_background), that key is also run with every one of them switched off.
#include "hooks.h"

namespace {

// jgld.dll is relocatable, so every module address below is resolved against the running base.
char* JgldBase() {
    static char* base = 0;
    if (!base) base = reinterpret_cast<char*>(GetModuleHandleA("jgld.dll"));
    return base;
}

// libpng 1.0.5's png_row_info, read off these two bodies: width at +0 (0x10074771), rowbytes at +4
// (0x100766fc), color_type at +8 (0x10074785), bit_depth at +9 (0x100747d7), channels at +0xa
// (0x100766c0), pixel_depth at +0xb (0x100766e2). The four tail fields are read with a zero-extending
// byte load everywhere.
struct PngRowInfo {
    unsigned width;
    unsigned rowbytes;
    unsigned char color_type;
    unsigned char bit_depth;
    unsigned char channels;
    unsigned char pixel_depth;
};

// libpng 1.0.5's png_color_16: red at +2 (0x10074f2f), green at +4 (0x10074f44), blue at +6
// (0x10074f59), gray at +8 (0x10074855). All four are 16-bit loads. The record is ten bytes: the
// caller holds background at png_struct+0x138 and background_1 at +0x142 (0x10071a69, 0x10071a5c).
struct PngColor16 {
    unsigned short index_pad;  // +0: not read by either body here
    unsigned short red;
    unsigned short green;
    unsigned short blue;
    unsigned short gray;
};

// png_composite, the no-division form (pngconf.h line 337 defines PNG_READ_COMPOSITE_NODIV_SUPPORTED
// in the configuration this build used): the sum is truncated to sixteen bits on the way to its stack
// slot (`mov word ptr [ebp-0x60], ax` at 0x100755f9) and the answer is (temp + (temp >> 8)) >> 8
// (0x1007560f-0x10075614). 255 - alpha is itself masked to sixteen bits (`and esi, 0xffff` at
// 0x100755e9), which cannot matter for an alpha byte but is what the code does.
unsigned char Composite8(unsigned fg, unsigned alpha, unsigned bg) {
    unsigned short temp = static_cast<unsigned short>(fg * alpha + bg * ((255 - alpha) & 0xffff) + 128);
    return static_cast<unsigned char>((temp + (temp >> 8)) >> 8);
}

// png_composite_16: the same shape one step wider - a 32-bit sum kept whole (`mov dword ptr
// [ebp-0x88], edx` at 0x10075913), + 0x8000 instead of + 0x80 (0x1007590c), and (temp + (temp >> 16))
// >> 16 with unsigned shifts (0x1007591f-0x1007592a). The product overflows 32 bits for large
// operands and the original lets it wrap, which `unsigned` reproduces.
unsigned short Composite16(unsigned fg, unsigned alpha, unsigned bg) {
    unsigned temp = fg * alpha + bg * (65535u - alpha) + 32768u;
    return static_cast<unsigned short>((temp + (temp >> 16)) >> 16);
}

// -------------------------------------------------------------------------------------------------
// 0x00074750  png_do_background
//
// void __cdecl png_do_background(png_row_info* row_info, unsigned char* row,
//     png_color_16* trans_values, png_color_16* background, png_color_16* background_1,
//     unsigned char* gamma_table, unsigned char* gamma_from_1, unsigned char* gamma_to_1,
//     unsigned short** gamma_16, unsigned short** gamma_16_from_1, unsigned short** gamma_16_to_1,
//     int gamma_shift)
// The twelve arguments sit at [ebp+8] .. [ebp+0x34] and the width is copied once into [ebp-0x10]
// (0x1007476e-0x10074773), before any body can rewrite the header.
//
// Guard (0x10074776-0x100747a4): background == NULL returns at once; otherwise, when bit 2 of
// color_type is set (`and eax, 4` at 0x10074788), color_type must not be 3 (0x10074797) and
// trans_values must not be NULL (0x100747a0). The colour-type switch is the seven-entry jump table at
// 0x10076713: 0 -> 0x100747d2, 2 -> 0x10074edb, 4 -> 0x100753fc, 6 -> 0x10075970 and 1, 3 and 5 ->
// 0x1007669f, the tail; 7 and above fail the unsigned range test at 0x100747bf.
// Inside colour type 0 the depth is switched on (bit_depth - 1) with an unsigned range test against
// 0xf (0x100747ef) and the byte table at 0x10076747 {0,1,5,2,5,5,5,3,5,5,5,5,5,5,5,4} indexing the
// jump table at 0x1007672f {0x10074811, 0x100748c2, 0x10074aca, 0x10074cb3, 0x10074d68, 0x10074ed6},
// so depths 1, 2, 4, 8 and 16 have a body and 3, 5, 6, 7 and 9 to 15 fall straight to the break.
// Colour types 2, 4 and 6 only ask `bit_depth == 8` (0x10074ee6, 0x10075407, 0x1007597b) and treat
// every other depth as sixteen bits.
// Each body comes in a gamma and a non-gamma form. The one-table forms test gamma_table (0x100748c6,
// 0x10074ace, 0x10074cb7, 0x10074ef0) or gamma_16 (0x10074d6c, 0x1007505f); the alpha forms need all
// three of their depth: gamma_to_1, gamma_from_1 and gamma_table at 8 bits (0x10075411, 0x1007541b,
// 0x10075425 and 0x10075985, 0x1007598f, 0x10075999) and gamma_16, gamma_16_from_1 and gamma_16_to_1
// at sixteen (0x1007562a, 0x10075634, 0x1007563e and 0x10075e66, 0x10075e70, 0x10075e7a).
// Tail (0x1007669f-0x100766fc): when bit 2 of color_type is set it is cleared (`and cl, 0xfb` at
// 0x100766b4), channels drops by one (0x100766c3), pixel_depth becomes channels * bit_depth
// (0x100766dc) and rowbytes (row_width * pixel_depth + 7) >> 3 (0x100766f0-0x100766f6) with the width
// captured at entry.
typedef void(__cdecl* PngDoBackground_t)(PngRowInfo*, unsigned char*, PngColor16*, PngColor16*,
                                         PngColor16*, unsigned char*, unsigned char*, unsigned char*,
                                         unsigned short**, unsigned short**, unsigned short**, int);
PngDoBackground_t PngDoBackground_orig;

void __cdecl PngDoBackground_re(PngRowInfo* row_info, unsigned char* row, PngColor16* trans_values,
                                PngColor16* background, PngColor16* background_1,
                                unsigned char* gamma_table, unsigned char* gamma_from_1,
                                unsigned char* gamma_to_1, unsigned short** gamma_16,
                                unsigned short** gamma_16_from_1, unsigned short** gamma_16_to_1,
                                int gamma_shift) {
    unsigned char* sp;
    unsigned char* dp;
    unsigned i;
    unsigned row_width = row_info->width;   // 0x1007476e: read before any body rewrites the header
    int shift;

    if (background == 0) return;                                             // 0x1007477a
    if ((row_info->color_type & 4) != 0) {                                   // 0x10074788
        // 0x10074797: this equality can never hold - it is only reached with bit 2 of color_type
        // set, and 3 has that bit clear - so the `je 0x100766ff` side at 0x1007479a is dead code.
        if (row_info->color_type == 3) return;
        if (trans_values == 0) return;                                       // 0x100747a4
    }

    switch (row_info->color_type) {                                          // jump table 0x10076713
        case 0:
            switch (row_info->bit_depth) {                                   // tables 0x10076747/0x1007672f
                case 1: {                                                    // 0x10074811
                    sp = row;
                    shift = 7;
                    for (i = 0; i < row_width; i++) {                        // 0x10074836, unsigned
                        if (static_cast<unsigned short>((*sp >> shift) & 1) == trans_values->gray) {
                            *sp &= static_cast<unsigned char>((0x7f7f >> (7 - shift)) & 0xff);  // 0x10074865
                            *sp |= static_cast<unsigned char>(background->gray << shift);       // 0x10074889
                        }
                        if (!shift) { shift = 7; sp++; }                     // 0x1007489b
                        else shift--;
                    }
                    break;
                }
                case 2: {
                    if (gamma_table != 0) {                                  // 0x100748c6
                        sp = row;
                        shift = 6;
                        for (i = 0; i < row_width; i++) {                    // 0x100748f1
                            if (static_cast<unsigned short>((*sp >> shift) & 3) == trans_values->gray) {
                                *sp &= static_cast<unsigned char>((0x3f3f >> (6 - shift)) & 0xff);
                                *sp |= static_cast<unsigned char>(background->gray << shift);
                            } else {                                         // 0x10074916 not taken
                                unsigned char p = static_cast<unsigned char>((*sp >> shift) & 3);
                                unsigned char g = static_cast<unsigned char>(
                                    (gamma_table[p | (p << 2) | (p << 4) | (p << 6)] >> 6) & 3);
                                *sp &= static_cast<unsigned char>((0x3f3f >> (6 - shift)) & 0xff);
                                *sp |= static_cast<unsigned char>(g << shift);
                            }
                            if (!shift) { shift = 6; sp++; }                 // 0x100749ec
                            else shift -= 2;
                        }
                    } else {                                                 // 0x10074a13
                        sp = row;
                        shift = 6;
                        for (i = 0; i < row_width; i++) {                    // 0x10074a38
                            if (static_cast<unsigned short>((*sp >> shift) & 3) == trans_values->gray) {
                                *sp &= static_cast<unsigned char>((0x3f3f >> (6 - shift)) & 0xff);
                                *sp |= static_cast<unsigned char>(background->gray << shift);
                            }
                            if (!shift) { shift = 6; sp++; }                 // 0x10074aa3
                            else shift -= 2;
                        }
                    }
                    break;
                }
                case 4: {
                    if (gamma_table != 0) {                                  // 0x10074ace
                        sp = row;
                        shift = 4;
                        for (i = 0; i < row_width; i++) {                    // 0x10074af9
                            if (static_cast<unsigned short>((*sp >> shift) & 0xf) == trans_values->gray) {
                                *sp &= static_cast<unsigned char>((0xf0f >> (4 - shift)) & 0xff);
                                *sp |= static_cast<unsigned char>(background->gray << shift);
                            } else {                                         // 0x10074b1e not taken
                                unsigned char p = static_cast<unsigned char>((*sp >> shift) & 0xf);
                                unsigned char g = static_cast<unsigned char>(
                                    (gamma_table[p | (p << 4)] >> 4) & 0xf);
                                *sp &= static_cast<unsigned char>((0xf0f >> (4 - shift)) & 0xff);
                                *sp |= static_cast<unsigned char>(g << shift);
                            }
                            if (!shift) { shift = 4; sp++; }                 // 0x10074bd5
                            else shift -= 4;
                        }
                    } else {                                                 // 0x10074bfc
                        sp = row;
                        shift = 4;
                        for (i = 0; i < row_width; i++) {                    // 0x10074c21
                            if (static_cast<unsigned short>((*sp >> shift) & 0xf) == trans_values->gray) {
                                *sp &= static_cast<unsigned char>((0xf0f >> (4 - shift)) & 0xff);
                                *sp |= static_cast<unsigned char>(background->gray << shift);
                            }
                            if (!shift) { shift = 4; sp++; }                 // 0x10074c8c
                            else shift -= 4;
                        }
                    }
                    break;
                }
                case 8: {
                    if (gamma_table != 0) {                                  // 0x10074cb7
                        sp = row;
                        for (i = 0; i < row_width; i++, sp++) {              // 0x10074ce0
                            if (*sp == trans_values->gray)                   // 0x10074cf4
                                *sp = static_cast<unsigned char>(background->gray);
                            else
                                *sp = gamma_table[*sp];
                        }
                    } else {                                                 // 0x10074d19
                        sp = row;
                        for (i = 0; i < row_width; i++, sp++) {              // 0x10074d40
                            if (*sp == trans_values->gray)                   // 0x10074d54
                                *sp = static_cast<unsigned char>(background->gray);
                        }
                    }
                    break;
                }
                case 16: {
                    if (gamma_16 != 0) {                                     // 0x10074d6c
                        sp = row;
                        for (i = 0; i < row_width; i++, sp += 2) {           // 0x10074d99
                            unsigned short v = static_cast<unsigned short>((*sp << 8) + *(sp + 1));
                            if (v == trans_values->gray) {                   // 0x10074dcb
                                *sp = static_cast<unsigned char>((background->gray >> 8) & 0xff);
                                *(sp + 1) = static_cast<unsigned char>(background->gray & 0xff);
                            } else {
                                // 0x10074e03: the sub-table index is *(sp+1) >> gamma_shift, the
                                // entry index *sp (0x10074e12-0x10074e15).
                                v = gamma_16[*(sp + 1) >> gamma_shift][*sp];
                                *sp = static_cast<unsigned char>((v >> 8) & 0xff);
                                *(sp + 1) = static_cast<unsigned char>(v & 0xff);
                            }
                        }
                    } else {                                                 // 0x10074e51
                        sp = row;
                        for (i = 0; i < row_width; i++, sp += 2) {           // 0x10074e78
                            unsigned short v = static_cast<unsigned short>((*sp << 8) + *(sp + 1));
                            if (v == trans_values->gray) {                   // 0x10074ea6
                                *sp = static_cast<unsigned char>((background->gray >> 8) & 0xff);
                                *(sp + 1) = static_cast<unsigned char>(background->gray & 0xff);
                            }
                        }
                    }
                    break;
                }
                default:                                                     // 0x10074ed6
                    break;
            }
            break;

        case 2:                                                              // 0x10074edb
            if (row_info->bit_depth == 8) {                                  // 0x10074ee6
                if (gamma_table != 0) {                                      // 0x10074ef0
                    sp = row;
                    for (i = 0; i < row_width; i++, sp += 3) {               // 0x10074f1d
                        if (*sp == trans_values->red &&                      // 0x10074f35
                            *(sp + 1) == trans_values->green &&              // 0x10074f4a
                            *(sp + 2) == trans_values->blue) {               // 0x10074f5f
                            *sp = static_cast<unsigned char>(background->red);
                            *(sp + 1) = static_cast<unsigned char>(background->green);
                            *(sp + 2) = static_cast<unsigned char>(background->blue);
                        } else {
                            *sp = gamma_table[*sp];
                            *(sp + 1) = gamma_table[*(sp + 1)];
                            *(sp + 2) = gamma_table[*(sp + 2)];
                        }
                    }
                } else {                                                     // 0x10074fca
                    sp = row;
                    for (i = 0; i < row_width; i++, sp += 3) {               // 0x10074ff1
                        if (*sp == trans_values->red &&                      // 0x10075005
                            *(sp + 1) == trans_values->green &&              // 0x1007501a
                            *(sp + 2) == trans_values->blue) {               // 0x1007502f
                            *sp = static_cast<unsigned char>(background->red);
                            *(sp + 1) = static_cast<unsigned char>(background->green);
                            *(sp + 2) = static_cast<unsigned char>(background->blue);
                        }
                    }
                }
            } else {                                                         // 0x1007505b
                if (gamma_16 != 0) {                                         // 0x1007505f
                    sp = row;
                    for (i = 0; i < row_width; i++, sp += 6) {               // 0x1007508c
                        unsigned short r = static_cast<unsigned short>((*sp << 8) + *(sp + 1));
                        unsigned short g = static_cast<unsigned short>((*(sp + 2) << 8) + *(sp + 3));
                        unsigned short b = static_cast<unsigned short>((*(sp + 4) << 8) + *(sp + 5));
                        if (r == trans_values->red &&                        // 0x100750ef
                            g == trans_values->green &&                      // 0x10075108
                            b == trans_values->blue) {                       // 0x10075121
                            *sp = static_cast<unsigned char>((background->red >> 8) & 0xff);
                            *(sp + 1) = static_cast<unsigned char>(background->red & 0xff);
                            *(sp + 2) = static_cast<unsigned char>((background->green >> 8) & 0xff);
                            *(sp + 3) = static_cast<unsigned char>(background->green & 0xff);
                            *(sp + 4) = static_cast<unsigned char>((background->blue >> 8) & 0xff);
                            *(sp + 5) = static_cast<unsigned char>(background->blue & 0xff);
                        } else {
                            unsigned short v = gamma_16[*(sp + 1) >> gamma_shift][*sp];
                            *sp = static_cast<unsigned char>((v >> 8) & 0xff);
                            *(sp + 1) = static_cast<unsigned char>(v & 0xff);
                            v = gamma_16[*(sp + 3) >> gamma_shift][*(sp + 2)];
                            *(sp + 2) = static_cast<unsigned char>((v >> 8) & 0xff);
                            *(sp + 3) = static_cast<unsigned char>(v & 0xff);
                            v = gamma_16[*(sp + 5) >> gamma_shift][*(sp + 4)];
                            *(sp + 4) = static_cast<unsigned char>((v >> 8) & 0xff);
                            *(sp + 5) = static_cast<unsigned char>(v & 0xff);
                        }
                    }
                } else {                                                     // 0x100752aa
                    sp = row;
                    for (i = 0; i < row_width; i++, sp += 6) {               // 0x100752d1
                        unsigned short r = static_cast<unsigned short>((*sp << 8) + *(sp + 1));
                        unsigned short g = static_cast<unsigned short>((*(sp + 2) << 8) + *(sp + 3));
                        unsigned short b = static_cast<unsigned short>((*(sp + 4) << 8) + *(sp + 5));
                        if (r == trans_values->red &&                        // 0x10075334
                            g == trans_values->green &&                      // 0x1007534d
                            b == trans_values->blue) {                       // 0x10075366
                            *sp = static_cast<unsigned char>((background->red >> 8) & 0xff);
                            *(sp + 1) = static_cast<unsigned char>(background->red & 0xff);
                            *(sp + 2) = static_cast<unsigned char>((background->green >> 8) & 0xff);
                            *(sp + 3) = static_cast<unsigned char>(background->green & 0xff);
                            *(sp + 4) = static_cast<unsigned char>((background->blue >> 8) & 0xff);
                            *(sp + 5) = static_cast<unsigned char>(background->blue & 0xff);
                        }
                    }
                }
            }
            break;

        case 4:                                                              // 0x100753fc
            if (row_info->bit_depth == 8) {                                  // 0x10075407
                // All three of gamma_to_1, gamma_from_1 and gamma_table must be set (0x10075411,
                // 0x1007541b, 0x10075425); any one NULL takes the plain body at 0x10075543.
                if (gamma_to_1 != 0 && gamma_from_1 != 0 && gamma_table != 0) {
                    sp = row;
                    dp = row;
                    for (i = 0; i < row_width; i++, sp += 2, dp++) {         // 0x10075461
                        unsigned short a = *(sp + 1);
                        if (a == 0xff) {                                     // 0x10075482
                            *dp = gamma_table[*sp];
                        } else if (a == 0) {                                 // 0x100754a6
                            *dp = static_cast<unsigned char>(background->gray);
                        } else {
                            unsigned char v = gamma_to_1[*sp];
                            unsigned char w = Composite8(v, a, background_1->gray);
                            *dp = gamma_from_1[w];
                        }
                    }
                } else {                                                     // 0x10075543
                    sp = row;
                    dp = row;
                    for (i = 0; i < row_width; i++, sp += 2, dp++) {         // 0x10075579
                        unsigned char a = *(sp + 1);
                        if (a == 0xff) {                                     // 0x10075595
                            *dp = *sp;
                        } else if (a == 0) {                                 // 0x100755ae
                            *dp = static_cast<unsigned char>(background->gray);
                        } else {
                            // 0x100755d0: the grey of background_1, not of background.
                            *dp = Composite8(*sp, a, background_1->gray);
                        }
                    }
                }
            } else {                                                         // 0x10075626
                if (gamma_16 != 0 && gamma_16_from_1 != 0 && gamma_16_to_1 != 0) {
                    sp = row;
                    dp = row;
                    for (i = 0; i < row_width; i++, sp += 4, dp += 2) {      // 0x1007567a
                        unsigned short a = static_cast<unsigned short>((*(sp + 2) << 8) + *(sp + 3));
                        if (a == 0xffff) {                                   // 0x100756a8
                            unsigned short v = gamma_16[*(sp + 1) >> gamma_shift][*sp];
                            *dp = static_cast<unsigned char>((v >> 8) & 0xff);
                            *(dp + 1) = static_cast<unsigned char>(v & 0xff);
                        } else if (a == 0) {                                 // 0x10075708
                            *dp = static_cast<unsigned char>((background->gray >> 8) & 0xff);
                            *(dp + 1) = static_cast<unsigned char>(background->gray & 0xff);
                        } else {
                            unsigned short g = gamma_16_to_1[*(sp + 1) >> gamma_shift][*sp];
                            unsigned short v = Composite16(g, a, background_1->gray);
                            unsigned short w = gamma_16_from_1[(v & 0xff) >> gamma_shift][v >> 8];
                            *dp = static_cast<unsigned char>((w >> 8) & 0xff);
                            *(dp + 1) = static_cast<unsigned char>(w & 0xff);
                        }
                    }
                } else {                                                     // 0x1007580c
                    sp = row;
                    dp = row;
                    for (i = 0; i < row_width; i++, sp += 4, dp += 2) {      // 0x10075842
                        unsigned short a = static_cast<unsigned short>((*(sp + 2) << 8) + *(sp + 3));
                        if (a == 0xffff) {                                   // 0x1007586e
                            // 0x1007587a: the CRT memcpy of jgld.dll, two bytes.
                            reinterpret_cast<void*(__cdecl*)(void*, const void*, unsigned)>(
                                JgldBase() + 0x7f3a0)(dp, sp, 2);
                        } else if (a == 0) {                                 // 0x10075891
                            *dp = static_cast<unsigned char>((background->gray >> 8) & 0xff);
                            *(dp + 1) = static_cast<unsigned char>(background->gray & 0xff);
                        } else {
                            unsigned short g = static_cast<unsigned short>((*sp << 8) + *(sp + 1));
                            unsigned short v = Composite16(g, a, background_1->gray);
                            *dp = static_cast<unsigned char>((v >> 8) & 0xff);
                            *(dp + 1) = static_cast<unsigned char>(v & 0xff);
                        }
                    }
                }
            }
            break;

        case 6:                                                              // 0x10075970
            if (row_info->bit_depth == 8) {                                  // 0x1007597b
                if (gamma_to_1 != 0 && gamma_from_1 != 0 && gamma_table != 0) {
                    sp = row;
                    dp = row;
                    for (i = 0; i < row_width; i++, sp += 4, dp += 3) {      // 0x100759d5
                        unsigned char a = *(sp + 3);
                        if (a == 0xff) {                                     // 0x100759f9
                            *dp = gamma_table[*sp];
                            *(dp + 1) = gamma_table[*(sp + 1)];
                            *(dp + 2) = gamma_table[*(sp + 2)];
                        } else if (a == 0) {                                 // 0x10075a48
                            *dp = static_cast<unsigned char>(background->red);
                            *(dp + 1) = static_cast<unsigned char>(background->green);
                            *(dp + 2) = static_cast<unsigned char>(background->blue);
                        } else {
                            unsigned char v = gamma_to_1[*sp];
                            unsigned char w = Composite8(v, a, background_1->red);
                            *dp = gamma_from_1[w];
                            v = gamma_to_1[*(sp + 1)];
                            w = Composite8(v, a, background_1->green);
                            *(dp + 1) = gamma_from_1[w];
                            v = gamma_to_1[*(sp + 2)];
                            w = Composite8(v, a, background_1->blue);
                            *(dp + 2) = gamma_from_1[w];
                        }
                    }
                } else {                                                     // 0x10075c54
                    sp = row;
                    dp = row;
                    for (i = 0; i < row_width; i++, sp += 4, dp += 3) {      // 0x10075c8a
                        unsigned char a = *(sp + 3);
                        if (a == 0xff) {                                     // 0x10075cae
                            *dp = *sp;
                            *(dp + 1) = *(sp + 1);
                            *(dp + 2) = *(sp + 2);
                        } else if (a == 0) {                                 // 0x10075ce4
                            *dp = static_cast<unsigned char>(background->red);
                            *(dp + 1) = static_cast<unsigned char>(background->green);
                            *(dp + 2) = static_cast<unsigned char>(background->blue);
                        } else {
                            // 0x10075d23, 0x10075d91, 0x10075dff: the three weights come from
                            // BACKGROUND, not background_1 - the eight-bit grey+alpha body one case
                            // up uses background_1 at the same place (0x100755d0).
                            *dp = Composite8(*sp, a, background->red);
                            *(dp + 1) = Composite8(*(sp + 1), a, background->green);
                            *(dp + 2) = Composite8(*(sp + 2), a, background->blue);
                        }
                    }
                }
            } else {                                                         // 0x10075e62
                if (gamma_16 != 0 && gamma_16_from_1 != 0 && gamma_16_to_1 != 0) {
                    sp = row;
                    dp = row;
                    for (i = 0; i < row_width; i++, sp += 8, dp += 6) {      // 0x10075eb6
                        unsigned short a = static_cast<unsigned short>((*(sp + 6) << 8) + *(sp + 7));
                        if (a == 0xffff) {                                   // 0x10075eea
                            unsigned short v = gamma_16[*(sp + 1) >> gamma_shift][*sp];
                            *dp = static_cast<unsigned char>((v >> 8) & 0xff);
                            *(dp + 1) = static_cast<unsigned char>(v & 0xff);
                            v = gamma_16[*(sp + 3) >> gamma_shift][*(sp + 2)];
                            *(dp + 2) = static_cast<unsigned char>((v >> 8) & 0xff);
                            *(dp + 3) = static_cast<unsigned char>(v & 0xff);
                            v = gamma_16[*(sp + 5) >> gamma_shift][*(sp + 4)];
                            *(dp + 4) = static_cast<unsigned char>((v >> 8) & 0xff);
                            *(dp + 5) = static_cast<unsigned char>(v & 0xff);
                        } else if (a == 0) {                                 // 0x1007600c
                            *dp = static_cast<unsigned char>((background->red >> 8) & 0xff);
                            *(dp + 1) = static_cast<unsigned char>(background->red & 0xff);
                            *(dp + 2) = static_cast<unsigned char>((background->green >> 8) & 0xff);
                            *(dp + 3) = static_cast<unsigned char>(background->green & 0xff);
                            *(dp + 4) = static_cast<unsigned char>((background->blue >> 8) & 0xff);
                            *(dp + 5) = static_cast<unsigned char>(background->blue & 0xff);
                        } else {
                            unsigned short v, w, x;
                            v = gamma_16_to_1[*(sp + 1) >> gamma_shift][*sp];
                            w = Composite16(v, a, background->red);
                            x = gamma_16_from_1[(w & 0xff) >> gamma_shift][w >> 8];
                            *dp = static_cast<unsigned char>((x >> 8) & 0xff);
                            *(dp + 1) = static_cast<unsigned char>(x & 0xff);
                            v = gamma_16_to_1[*(sp + 3) >> gamma_shift][*(sp + 2)];
                            w = Composite16(v, a, background->green);
                            x = gamma_16_from_1[(w & 0xff) >> gamma_shift][w >> 8];
                            *(dp + 2) = static_cast<unsigned char>((x >> 8) & 0xff);
                            *(dp + 3) = static_cast<unsigned char>(x & 0xff);
                            v = gamma_16_to_1[*(sp + 5) >> gamma_shift][*(sp + 4)];
                            w = Composite16(v, a, background->blue);
                            x = gamma_16_from_1[(w & 0xff) >> gamma_shift][w >> 8];
                            *(dp + 4) = static_cast<unsigned char>((x >> 8) & 0xff);
                            *(dp + 5) = static_cast<unsigned char>(x & 0xff);
                        }
                    }
                } else {                                                     // 0x10076371
                    sp = row;
                    dp = row;
                    for (i = 0; i < row_width; i++, sp += 8, dp += 6) {      // 0x100763a7
                        unsigned short a = static_cast<unsigned short>((*(sp + 6) << 8) + *(sp + 7));
                        if (a == 0xffff) {                                   // 0x100763d9
                            // 0x100763e5: the CRT memcpy, six bytes.
                            reinterpret_cast<void*(__cdecl*)(void*, const void*, unsigned)>(
                                JgldBase() + 0x7f3a0)(dp, sp, 6);
                        } else if (a == 0) {                                 // 0x100763ff
                            *dp = static_cast<unsigned char>((background->red >> 8) & 0xff);
                            *(dp + 1) = static_cast<unsigned char>(background->red & 0xff);
                            *(dp + 2) = static_cast<unsigned char>((background->green >> 8) & 0xff);
                            *(dp + 3) = static_cast<unsigned char>(background->green & 0xff);
                            *(dp + 4) = static_cast<unsigned char>((background->blue >> 8) & 0xff);
                            *(dp + 5) = static_cast<unsigned char>(background->blue & 0xff);
                        } else {
                            unsigned short v;
                            unsigned short r = static_cast<unsigned short>((*sp << 8) + *(sp + 1));
                            unsigned short g = static_cast<unsigned short>((*(sp + 2) << 8) + *(sp + 3));
                            unsigned short b = static_cast<unsigned short>((*(sp + 4) << 8) + *(sp + 5));
                            v = Composite16(r, a, background->red);
                            *dp = static_cast<unsigned char>((v >> 8) & 0xff);
                            *(dp + 1) = static_cast<unsigned char>(v & 0xff);
                            v = Composite16(g, a, background->green);
                            *(dp + 2) = static_cast<unsigned char>((v >> 8) & 0xff);
                            *(dp + 3) = static_cast<unsigned char>(v & 0xff);
                            v = Composite16(b, a, background->blue);
                            *(dp + 4) = static_cast<unsigned char>((v >> 8) & 0xff);
                            *(dp + 5) = static_cast<unsigned char>(v & 0xff);
                        }
                    }
                }
            }
            break;

        default:                                                             // 0x1007669f
            break;
    }

    if ((row_info->color_type & 4) != 0) {                                   // 0x100766ac
        row_info->color_type = static_cast<unsigned char>(row_info->color_type & 0xfb);  // 0x100766b4
        row_info->channels = static_cast<unsigned char>(row_info->channels - 1);         // 0x100766c3
        row_info->pixel_depth =
            static_cast<unsigned char>(row_info->channels * row_info->bit_depth);        // 0x100766dc
        row_info->rowbytes = (row_width * row_info->pixel_depth + 7) >> 3;               // 0x100766f0
    }
}

// -------------------------------------------------------------------------------------------------
// 0x000717d0  png_do_read_transformations
//
// void __cdecl png_do_read_transformations(png_struct* png_ptr): the single argument is at [ebp+8]
// and every field below is an offset into it, each cited at the instruction that reads it.
//   flags                     +0x6c  (0x100719b1)
//   transformations           +0x70  (0x1007182e)
//   read_user_transform_fn    +0x58  (0x10071db7)
//   user_transform_depth      +0x64  (0x10071def, a byte)
//   user_transform_channels   +0x65  (0x10071e0a, a byte)
//   row_number                +0xe4  (0x10071803)
//   row_buf                   +0xec  (0x100717eb); every callee gets row_buf + 1 (0x10071878)
//   row_info                  +0x100 (0x1007187f): width +0x100, rowbytes +0x104, color_type +0x108,
//                                    bit_depth +0x109, channels +0x10a, pixel_depth +0x10b
//   palette                   +0x114 (0x10071868)
//   num_trans                 +0x11a (0x10071853, a 16-bit load)
//   pass                      +0x124 (0x100717f9, a byte)
//   color_type                +0x126 (0x10071a05, a byte; the png_struct's own, not row_info's)
//   filler                    +0x12e (0x10071cf1, a 16-bit load)
//   background                +0x138 (0x10071a69)   background_1  +0x142 (0x10071a5c)
//   gamma_shift               +0x158 (0x10071a19)   gamma_table   +0x164 (0x10071a55)
//   gamma_from_1              +0x168 (0x10071a4b)   gamma_to_1    +0x16c (0x10071a41)
//   gamma_16_table            +0x170 (0x10071a37)   gamma_16_from_1 +0x174 (0x10071a2d)
//   gamma_16_to_1             +0x178 (0x10071a23)
//   sig_bits (png_color_8)    +0x181 (0x10071beb)
//   trans                     +0x188 (0x1007185e)   trans_values  +0x18c (0x100718a3)
//   palette_lookup            +0x1ec (0x10071b6d)   dither_index  +0x1f0 (0x10071b63)
//   rgb_to_gray_status        +0x214 (0x1007195d, a byte store of 1)
//
// Nineteen transformation bits are tested in a fixed order against the word at +0x70 and each runs
// one callee. The three strings the body can pass to png_error / png_warning are module constants:
// "NULL row buffer for row %ld, pass %d" 0x1011de14, "png_do_rgb_to_gray found nongray pixel"
// 0x1011ddec and "png_do_dither returned rowbytes=0" 0x1011ddc8; all three are pushed verbatim and
// are reached through this code at their own addresses.
struct PngColor8 { unsigned char red, green, blue, gray, alpha; };

typedef void(__cdecl* RowFn2_t)(PngRowInfo*, unsigned char*);
typedef void(__cdecl* PngDoExpandPalette_t)(PngRowInfo*, unsigned char*, void*, unsigned char*, int);
typedef void(__cdecl* PngDoExpand_t)(PngRowInfo*, unsigned char*, PngColor16*);
typedef void(__cdecl* PngDoStripFiller_t)(PngRowInfo*, unsigned char*, unsigned);
typedef int(__cdecl* PngDoRgbToGray_t)(void*, PngRowInfo*, unsigned char*);
typedef void(__cdecl* PngDoGamma_t)(PngRowInfo*, unsigned char*, unsigned char*, unsigned short**, int);
typedef void(__cdecl* PngDoDither_t)(PngRowInfo*, unsigned char*, unsigned char*, unsigned char*);
typedef void(__cdecl* PngDoUnshift_t)(PngRowInfo*, unsigned char*, PngColor8*);
typedef void(__cdecl* PngDoReadFiller_t)(PngRowInfo*, unsigned char*, unsigned, unsigned);
typedef void(__cdecl* PngErrFn_t)(void*, const char*);
typedef void(__cdecl* UserTransform_t)(void*, PngRowInfo*, unsigned char*);
typedef int(__cdecl* Sprintf_t)(char*, const char*, ...);

typedef void(__cdecl* PngDoReadTransformations_t)(void*);
PngDoReadTransformations_t PngDoReadTransformations_orig;

void __cdecl PngDoReadTransformations_re(void* png_ptr) {
    char* p = reinterpret_cast<char*>(png_ptr);
    char* jb = JgldBase();

    // Field accessors, each at the offset cited in the table above.
    unsigned& transformations = *reinterpret_cast<unsigned*>(p + 0x70);
    unsigned& png_flags = *reinterpret_cast<unsigned*>(p + 0x6c);
    unsigned char*& row_buf = *reinterpret_cast<unsigned char**>(p + 0xec);
    PngRowInfo* row_info = reinterpret_cast<PngRowInfo*>(p + 0x100);
    unsigned num_trans = *reinterpret_cast<unsigned short*>(p + 0x11a);
    unsigned char png_color_type = *reinterpret_cast<unsigned char*>(p + 0x126);

    if (row_buf == 0) {                                                      // 0x100717eb
        char msg[50];                                                        // [ebp-0x34], 0x1007180f
        reinterpret_cast<Sprintf_t>(jb + 0x82c80)(msg, jb + 0x11de14,
                                                  *reinterpret_cast<unsigned*>(p + 0xe4),
                                                  *reinterpret_cast<unsigned char*>(p + 0x124));
        reinterpret_cast<PngErrFn_t>(jb + 0x78fe0)(png_ptr, msg);            // 0x10071823
    }

    if (transformations & 0x1000) {                                          // PNG_EXPAND, 0x10071831
        if (row_info->color_type == 3) {                                     // 0x10071849
            reinterpret_cast<PngDoExpandPalette_t>(jb + 0x76ec0)(
                row_info, row_buf + 1,
                *reinterpret_cast<void**>(p + 0x114),
                *reinterpret_cast<unsigned char**>(p + 0x188),
                static_cast<int>(num_trans));                                // 0x10071886
        } else if (num_trans != 0) {                                         // 0x1007189c
            reinterpret_cast<PngDoExpand_t>(jb + 0x77300)(
                row_info, row_buf + 1, reinterpret_cast<PngColor16*>(p + 0x18c));  // 0x100718c1
        } else {
            reinterpret_cast<PngDoExpand_t>(jb + 0x77300)(row_info, row_buf + 1, 0);  // 0x100718e4
        }
    }

    if (transformations & 0x40000) {                                         // PNG_STRIP_ALPHA, 0x100718f2
        reinterpret_cast<PngDoStripFiller_t>(jb + 0x6dac0)(row_info, row_buf + 1, 0x80);  // 0x10071918
    }

    if (transformations & 0x600000) {                                        // PNG_RGB_TO_GRAY, 0x10071926
        int rgb_error = reinterpret_cast<PngDoRgbToGray_t>(jb + 0x736c0)(
            png_ptr, row_info, row_buf + 1);                                 // 0x10071949
        if (rgb_error != 0) {                                                // 0x10071954
            *reinterpret_cast<unsigned char*>(p + 0x214) = 1;                // 0x1007195d
            // Both of these compare the WHOLE transformations word with one constant (0x10071967 and
            // 0x10071984), so they only fire when no other transformation bit is set.
            if (transformations == 0x400000)
                reinterpret_cast<PngErrFn_t>(jb + 0x79040)(png_ptr, jb + 0x11ddec);  // 0x10071979
            if (transformations == 0x200000)
                reinterpret_cast<PngErrFn_t>(jb + 0x78fe0)(png_ptr, jb + 0x11ddec);  // 0x10071996
        }
    }

    if ((transformations & 0x4000) && !(png_flags & 0x20000)) {              // 0x100719a4, 0x100719b4
        reinterpret_cast<RowFn2_t>(jb + 0x73310)(row_info, row_buf + 1);     // 0x100719d5
    }

    if ((transformations & 0x80) &&                                          // PNG_BACKGROUND, 0x100719e3
        (num_trans != 0 || (png_color_type & 4))) {                          // 0x100719fc, 0x10071a0b
        reinterpret_cast<PngDoBackground_t>(jb + 0x74750)(                   // 0x10071a8f
            row_info, row_buf + 1,
            reinterpret_cast<PngColor16*>(p + 0x18c),
            reinterpret_cast<PngColor16*>(p + 0x138),
            reinterpret_cast<PngColor16*>(p + 0x142),
            *reinterpret_cast<unsigned char**>(p + 0x164),
            *reinterpret_cast<unsigned char**>(p + 0x168),
            *reinterpret_cast<unsigned char**>(p + 0x16c),
            *reinterpret_cast<unsigned short***>(p + 0x170),
            *reinterpret_cast<unsigned short***>(p + 0x174),
            *reinterpret_cast<unsigned short***>(p + 0x178),
            *reinterpret_cast<int*>(p + 0x158));
    }

    if ((transformations & 0x2000) &&                                        // PNG_GAMMA, 0x10071a9d
        !((transformations & 0x80) &&                                        // 0x10071aad
          (num_trans != 0 || (png_color_type & 4))) &&                       // 0x10071abc, 0x10071acc
        png_color_type != 3) {                                               // 0x10071ae4
        reinterpret_cast<PngDoGamma_t>(jb + 0x76760)(                        // 0x10071b1e
            row_info, row_buf + 1,
            *reinterpret_cast<unsigned char**>(p + 0x164),
            *reinterpret_cast<unsigned short***>(p + 0x170),
            *reinterpret_cast<int*>(p + 0x158));
    }

    if (transformations & 0x400) {                                           // PNG_16_TO_8, 0x10071b2c
        reinterpret_cast<RowFn2_t>(jb + 0x72430)(row_info, row_buf + 1);     // 0x10071b4b
    }

    if (transformations & 0x40) {                                            // PNG_DITHER, 0x10071b59
        reinterpret_cast<PngDoDither_t>(jb + 0x77a90)(                       // 0x10071b8a
            row_info, row_buf + 1,
            *reinterpret_cast<unsigned char**>(p + 0x1ec),
            *reinterpret_cast<unsigned char**>(p + 0x1f0));
        if (row_info->rowbytes == 0)                                         // 0x10071b95
            reinterpret_cast<PngErrFn_t>(jb + 0x78fe0)(png_ptr, jb + 0x11ddc8);  // 0x10071ba7
    }

    if (transformations & 0x20) {                                            // PNG_INVERT_MONO, 0x10071bb5
        reinterpret_cast<RowFn2_t>(jb + 0x6d900)(row_info, row_buf + 1);     // 0x10071bd3
    }

    if (transformations & 8) {                                               // PNG_SHIFT, 0x10071be1
        reinterpret_cast<PngDoUnshift_t>(jb + 0x720c0)(                      // 0x10071c09
            row_info, row_buf + 1, reinterpret_cast<PngColor8*>(p + 0x181));
    }

    if (transformations & 4) {                                               // PNG_PACK, 0x10071c17
        reinterpret_cast<RowFn2_t>(jb + 0x71e80)(row_info, row_buf + 1);     // 0x10071c34
    }

    if (transformations & 1) {                                               // PNG_BGR, 0x10071c42
        reinterpret_cast<RowFn2_t>(jb + 0x6e010)(row_info, row_buf + 1);     // 0x10071c60
    }

    if (transformations & 0x10000) {                                         // PNG_PACKSWAP, 0x10071c6e
        reinterpret_cast<RowFn2_t>(jb + 0x6da10)(row_info, row_buf + 1);     // 0x10071c8f
    }

    if ((transformations & 0x4000) && (png_flags & 0x20000)) {               // 0x10071c9d, 0x10071cac
        reinterpret_cast<RowFn2_t>(jb + 0x73310)(row_info, row_buf + 1);     // 0x10071ccd
    }

    if (transformations & 0x8000) {                                          // PNG_FILLER, 0x10071cdb
        reinterpret_cast<PngDoReadFiller_t>(jb + 0x72bd0)(                   // 0x10071d10
            row_info, row_buf + 1,
            *reinterpret_cast<unsigned short*>(p + 0x12e),
            png_flags);
    }

    if (transformations & 0x80000) {                                         // PNG_INVERT_ALPHA, 0x10071d1e
        reinterpret_cast<RowFn2_t>(jb + 0x72860)(row_info, row_buf + 1);     // 0x10071d3f
    }

    if (transformations & 0x20000) {                                         // PNG_SWAP_ALPHA, 0x10071d4d
        reinterpret_cast<RowFn2_t>(jb + 0x724f0)(row_info, row_buf + 1);     // 0x10071d6c
    }

    if (transformations & 0x10) {                                            // PNG_SWAP_BYTES, 0x10071d7a
        reinterpret_cast<RowFn2_t>(jb + 0x6d980)(row_info, row_buf + 1);     // 0x10071d98
    }

    if (transformations & 0x100000) {                                        // PNG_USER_TRANSFORM, 0x10071da6
        UserTransform_t fn = *reinterpret_cast<UserTransform_t*>(p + 0x58);
        if (fn != 0) fn(png_ptr, row_info, row_buf + 1);                     // 0x10071db7, 0x10071ddd
        if (*reinterpret_cast<unsigned char*>(p + 0x64) != 0)                // 0x10071def
            row_info->bit_depth = *reinterpret_cast<unsigned char*>(p + 0x64);
        if (*reinterpret_cast<unsigned char*>(p + 0x65) != 0)                // 0x10071e0a
            row_info->channels = *reinterpret_cast<unsigned char*>(p + 0x65);
        row_info->pixel_depth =
            static_cast<unsigned char>(row_info->bit_depth * row_info->channels);   // 0x10071e36
        row_info->rowbytes = (row_info->width * row_info->pixel_depth + 7) >> 3;    // 0x10071e56
    }
}

}  // namespace

SG_HOOK("jgld.dll", 0x00074750, png_do_background, PngDoBackground_re, PngDoBackground_orig);
SG_HOOK("jgld.dll", 0x000717d0, png_do_read_transformations, PngDoReadTransformations_re,
        PngDoReadTransformations_orig);

// Batch c3bg: jgld.dll non-render - the seven libpng 1.0.5 row routines batch c3bb left as leads
// (png_do_strip_filler, png_do_read_filler, png_do_gamma, png_do_expand, png_do_rgb_to_gray,
// png_do_read_interlace, png_combine_row), libpng's sRGB convenience setter
// (png_set_sRGB_gAMA_and_cHRM) and zlib 1.0.2's fast inflate loop (inflate_fast).
//
// SG_HOOK addresses are RVAs; every instruction cited in a comment is a VA (0x10000000 + RVA), the
// form the debug build's disassembly prints. jgld.dll is LoadLibrary'd after the shim starts, so these
// hooks install from the LoadLibraryA hook (SgHooksInstallPending, re/hooks.cpp) and diff_hook finds
// them by (module, rva) through SimGolfShim_FindHookM.
//
// Every routine here is __cdecl (each pops nothing: `ret` at 0x1006e00b, 0x10072ed9, 0x10076ea0...).
//
// Nothing live in the running jgld.dll is touched: no Display, Surface, Palette, Font, the game's
// png_struct / png_info, its inflate state or its heap blocks. The routines that stay original (the
// CRT memcpy, png_set_sRGB / png_set_gAMA / png_set_cHRM) and the read-only module tables
// (png_pass_inc, inflate_mask, the two zlib message strings) are reached at their own module
// addresses, so each comparison is of this code and not of a CRT or of a constant we invented.
#include "hooks.h"

namespace {

// jgld.dll is relocatable, so every module address below is resolved against the running base.
char* JgldBase() {
    static char* base = 0;
    if (!base) base = reinterpret_cast<char*>(GetModuleHandleA("jgld.dll"));
    return base;
}

// libpng 1.0.5's png_row_info, read off the bodies: width at +0 (0x1006dae7), rowbytes at +4
// (0x1006dc35), color_type at +8 (0x100736f2), bit_depth at +9 (0x1006db02), channels at +0xa
// (0x1006daf1), pixel_depth at +0xb (0x1007c858). The four tail fields are always read with a
// zero-extending byte load.
struct PngRowInfo {
    unsigned width;
    unsigned rowbytes;
    unsigned char color_type;
    unsigned char bit_depth;
    unsigned char channels;
    unsigned char pixel_depth;
};

// libpng 1.0.5's png_color_16, read off png_do_expand: red at +2 (0x100777fc), green at +4
// (0x10077811), blue at +6 (0x10077825), gray at +8 (0x1007733b). All four are 16-bit loads.
struct PngColor16 {
    unsigned short index_pad;  // +0: not read by any body in this batch
    unsigned short red;
    unsigned short green;
    unsigned short blue;
    unsigned short gray;
};

// The routines and tables that stay original.
typedef void*(__cdecl* Memcpy_t)(void*, const void*, unsigned);              // 0x1007f3a0
typedef void(__cdecl* PngSetSRGB_t)(void*, void*, int);                      // 0x1007e0d0
typedef void(__cdecl* PngSetGAMA_t)(void*, void*, double);                   // 0x1007dbd0
typedef void(__cdecl* PngSetCHRM_t)(void*, void*, double, double, double,
                                    double, double, double, double, double); // 0x1007db30

Memcpy_t JgldMemcpy() { return reinterpret_cast<Memcpy_t>(JgldBase() + 0x7f3a0); }
// png_pass_inc[7] = {8, 8, 4, 4, 2, 2, 1} at 0x10123244, read as `[ecx*4 + 0x10123244]` (0x1007c84a).
const unsigned* PngPassInc() { return reinterpret_cast<const unsigned*>(JgldBase() + 0x123244); }
// zlib's inflate_mask[17] at 0x101274a4, read as `[edx*4 + 0x101274a4]` (0x100a03b5).
const unsigned* InflateMask() { return reinterpret_cast<const unsigned*>(JgldBase() + 0x1274a4); }

// --------------------------------------------------------------------- libpng row transformations

// 0x0006dac0  png_do_strip_filler
// void __cdecl png_do_strip_filler(png_row_info*, unsigned char* row, unsigned flags): drops the
// fourth channel of an RGBX/RGBA row or the second of a GX/GA row, in place and forwards.
// sp and dp both start at row (0x1006dadb / 0x1006dae1) and the width is read once into [ebp-0xc]
// (0x1006dae7). The channel count at +0xa selects the two halves: 4 (`cmp edx, 4` / `jne 0x1006de3b`
// at 0x1006daf4-0x1006daf7) or 2 (`cmp ecx, 2` / `jne 0x1006e005` at 0x1006de43-0x1006de46); any other
// count leaves the row and the header untouched.
//   channels 4, bit_depth 8 (`cmp ecx, 8` / `jne 0x1006dc3d` at 0x1006db05-0x1006db08): with
//   flags & 0x80 (`and edx, 0x80` / `je 0x1006dbac` at 0x1006db11-0x1006db19) dp jumps to row+3 and sp
//   to row+4 (0x1006db22 / 0x1006db2b) and the loop runs from 1 (0x1006db31), copying three bytes and
//   skipping one; without the flag the loop runs from 0 (0x1006dbac) and skips one byte before each
//   group of three. The header then gets pixel_depth 0x18 (0x1006dc28) and rowbytes = width * 3
//   (`imul eax, eax, 3` at 0x1006dc2f).
//   channels 4, any other depth (the 16-bit body at 0x1006dc3d): six bytes are kept and two dropped,
//   with the same flag test at 0x1006dc46 choosing sp += 8 / dp += 6 and a loop from 1 (0x1006dc51 /
//   0x1006dc5a) or sp += 2 before each group with a loop from 0 (0x1006dd39). pixel_depth becomes 0x30
//   (0x1006de10) and rowbytes width * 6 (`imul eax, eax, 6` at 0x1006de17).
//   Both channel-4 bodies end at 0x1006de20 with channels 3 (0x1006de23) and the alpha bit cleared
//   from color_type (`and cl, 0xfb` at 0x1006de2d).
//   channels 2 is the same shape with one kept byte per pixel at depth 8 (pixel_depth 8 at 0x1006def1,
//   rowbytes = width at 0x1006defb) and two at any other depth (pixel_depth 0x10 at 0x1006dfe0,
//   rowbytes = width * 2 with `shl eax, 1` at 0x1006dfe7), ending at 0x1006dfef with channels 1
//   (0x1006dff2) and the same `and cl, 0xfb` (0x1006dffc).
// Every loop bound is an UNSIGNED compare against the width (`jae` at 0x1006db46, 0x1006dbc1,
// 0x1006dc75, 0x1006dd4e, 0x1006de7f, 0x1006dec2, 0x1006df36, 0x1006df95), so the loops that start at
// 1 move nothing when the width is 0 or 1 while the header is still rewritten.
typedef void(__cdecl* PngDoStripFiller_t)(PngRowInfo*, unsigned char*, unsigned);
PngDoStripFiller_t PngDoStripFiller_orig;
void __cdecl PngDoStripFiller_re(PngRowInfo* row_info, unsigned char* row, unsigned flags) {
    unsigned char* sp = row;
    unsigned char* dp = row;
    unsigned row_width = row_info->width;
    unsigned i;

    if (row_info->channels == 4) {
        if (row_info->bit_depth == 8) {
            if (flags & 0x80) {
                dp += 3;
                sp += 4;
                for (i = 1; i < row_width; i++) {
                    *dp++ = *sp++;
                    *dp++ = *sp++;
                    *dp++ = *sp++;
                    sp++;
                }
            } else {
                for (i = 0; i < row_width; i++) {
                    sp++;
                    *dp++ = *sp++;
                    *dp++ = *sp++;
                    *dp++ = *sp++;
                }
            }
            row_info->pixel_depth = 24;
            row_info->rowbytes = row_width * 3;
        } else {
            if (flags & 0x80) {
                sp += 8;
                dp += 6;
                for (i = 1; i < row_width; i++) {
                    *dp++ = *sp++;
                    *dp++ = *sp++;
                    *dp++ = *sp++;
                    *dp++ = *sp++;
                    *dp++ = *sp++;
                    *dp++ = *sp++;
                    sp += 2;
                }
            } else {
                for (i = 0; i < row_width; i++) {
                    sp += 2;
                    *dp++ = *sp++;
                    *dp++ = *sp++;
                    *dp++ = *sp++;
                    *dp++ = *sp++;
                    *dp++ = *sp++;
                    *dp++ = *sp++;
                }
            }
            row_info->pixel_depth = 48;
            row_info->rowbytes = row_width * 6;
        }
        row_info->channels = 3;
        row_info->color_type = static_cast<unsigned char>(row_info->color_type & 0xfb);
    } else if (row_info->channels == 2) {
        if (row_info->bit_depth == 8) {
            if (flags & 0x80) {
                for (i = 0; i < row_width; i++) {
                    *dp++ = *sp++;
                    sp++;
                }
            } else {
                for (i = 0; i < row_width; i++) {
                    sp++;
                    *dp++ = *sp++;
                }
            }
            row_info->pixel_depth = 8;
            row_info->rowbytes = row_width;
        } else {
            if (flags & 0x80) {
                sp += 4;
                dp += 2;
                for (i = 1; i < row_width; i++) {
                    *dp++ = *sp++;
                    *dp++ = *sp++;
                    sp += 2;
                }
            } else {
                for (i = 0; i < row_width; i++) {
                    sp += 2;
                    *dp++ = *sp++;
                    *dp++ = *sp++;
                }
            }
            row_info->pixel_depth = 16;
            row_info->rowbytes = row_width * 2;
        }
        row_info->channels = 1;
        row_info->color_type = static_cast<unsigned char>(row_info->color_type & 0xfb);
    }
}

// 0x00072bd0  png_do_read_filler
// void __cdecl png_do_read_filler(png_row_info*, unsigned char* row, unsigned filler, unsigned flags):
// inserts a filler channel into a grey or RGB row, writing BACKWARDS so the row grows in place.
// The width is read into [ebp-8] (0x10072be3) and the filler is split once into a high byte
// (`shr edx, 8` / `and edx, 0xff` at 0x10072bf9-0x10072bfc) and a low byte (`and eax, 0xff` at
// 0x10072c08).
// color_type 0 (`test edx, edx` / `jne 0x10072ed4` at 0x10072c18-0x10072c1a) splits into bit_depth 8
// (`cmp ecx, 8` / `jne 0x10072d42` at 0x10072c28) and bit_depth 16 (`cmp eax, 0x10` / `jne 0x10072ecf`
// at 0x10072d4a); any other depth leaves the row alone. color_type 2 (`cmp eax, 2` / `jne 0x10073309`
// at 0x10072edc) splits the same way (0x10072eed, 0x1007308d). Each of the four pairs then tests
// flags & 0x80 (`and edx, 0x80` at 0x10072c34, 0x10072d56, 0x10072ef9, 0x10073099): the "after" bodies
// run their loop from 1 and write one trailing filler group after it, the "before" bodies run from 0.
// Both pointers start from the same two expressions whatever the depth: sp = row + width and
// dp = sp + width for the grey bodies, sp = row + width * 3 (`imul ecx, ecx, 3` at 0x10072f09,
// 0x10072fd1, 0x100730a9, 0x100731ee) and dp = sp + width for the RGB bodies, so the 16-bit RGB bodies
// read and write further below `row` than the row itself: the eight bytes they move per pixel against
// the six they consume leave dp about 4 * width bytes under the start of the row. That is what the
// original does and both arms do it identically.
// rowbytes is width * 2 for the grey bodies (`shl edx, 1` at 0x10072cbf, `shl eax, 1` at 0x10072d35),
// width * 4 for grey-16 and RGB-8 (`shl eax, 2` at 0x10072e1f, 0x10072ec6, `shl edx, 2` at 0x10072fc0,
// 0x10073077) and width * 8 for RGB-16 (`shl edx, 3` at 0x100731dd, 0x10073300); channels becomes 2 for
// the grey bodies and 4 for the RGB ones, and pixel_depth 0x10 / 0x20 / 0x40 accordingly.
typedef void(__cdecl* PngDoReadFiller_t)(PngRowInfo*, unsigned char*, unsigned, unsigned);
PngDoReadFiller_t PngDoReadFiller_orig;
void __cdecl PngDoReadFiller_re(PngRowInfo* row_info, unsigned char* row, unsigned filler,
                                unsigned flags) {
    unsigned i;
    unsigned row_width = row_info->width;
    unsigned char hi_filler = static_cast<unsigned char>((filler >> 8) & 0xff);
    unsigned char lo_filler = static_cast<unsigned char>(filler & 0xff);

    if (row_info->color_type == 0) {
        if (row_info->bit_depth == 8) {
            unsigned char* sp = row + row_width;
            unsigned char* dp = sp + row_width;
            if (flags & 0x80) {
                for (i = 1; i < row_width; i++) {
                    *(--dp) = lo_filler;
                    *(--dp) = *(--sp);
                }
                *(--dp) = lo_filler;
            } else {
                for (i = 0; i < row_width; i++) {
                    *(--dp) = *(--sp);
                    *(--dp) = lo_filler;
                }
            }
            row_info->channels = 2;
            row_info->pixel_depth = 16;
            row_info->rowbytes = row_width * 2;
        } else if (row_info->bit_depth == 16) {
            unsigned char* sp = row + row_width;
            unsigned char* dp = sp + row_width;
            if (flags & 0x80) {
                for (i = 1; i < row_width; i++) {
                    *(--dp) = hi_filler;
                    *(--dp) = lo_filler;
                    *(--dp) = *(--sp);
                    *(--dp) = *(--sp);
                }
                *(--dp) = hi_filler;
                *(--dp) = lo_filler;
            } else {
                for (i = 0; i < row_width; i++) {
                    *(--dp) = *(--sp);
                    *(--dp) = *(--sp);
                    *(--dp) = hi_filler;
                    *(--dp) = lo_filler;
                }
            }
            row_info->channels = 2;
            row_info->pixel_depth = 32;
            row_info->rowbytes = row_width * 4;
        }
    } else if (row_info->color_type == 2) {
        if (row_info->bit_depth == 8) {
            unsigned char* sp = row + row_width * 3;
            unsigned char* dp = sp + row_width;
            if (flags & 0x80) {
                for (i = 1; i < row_width; i++) {
                    *(--dp) = lo_filler;
                    *(--dp) = *(--sp);
                    *(--dp) = *(--sp);
                    *(--dp) = *(--sp);
                }
                *(--dp) = lo_filler;
            } else {
                for (i = 0; i < row_width; i++) {
                    *(--dp) = *(--sp);
                    *(--dp) = *(--sp);
                    *(--dp) = *(--sp);
                    *(--dp) = lo_filler;
                }
            }
            row_info->channels = 4;
            row_info->pixel_depth = 32;
            row_info->rowbytes = row_width * 4;
        } else if (row_info->bit_depth == 16) {
            unsigned char* sp = row + row_width * 3;
            unsigned char* dp = sp + row_width;
            if (flags & 0x80) {
                for (i = 1; i < row_width; i++) {
                    *(--dp) = hi_filler;
                    *(--dp) = lo_filler;
                    *(--dp) = *(--sp);
                    *(--dp) = *(--sp);
                    *(--dp) = *(--sp);
                    *(--dp) = *(--sp);
                    *(--dp) = *(--sp);
                    *(--dp) = *(--sp);
                }
                *(--dp) = hi_filler;
                *(--dp) = lo_filler;
            } else {
                for (i = 0; i < row_width; i++) {
                    *(--dp) = *(--sp);
                    *(--dp) = *(--sp);
                    *(--dp) = *(--sp);
                    *(--dp) = *(--sp);
                    *(--dp) = *(--sp);
                    *(--dp) = *(--sp);
                    *(--dp) = hi_filler;
                    *(--dp) = lo_filler;
                }
            }
            row_info->channels = 4;
            row_info->pixel_depth = 64;
            row_info->rowbytes = row_width * 8;
        }
    }
}

// 0x00076760  png_do_gamma
// void __cdecl png_do_gamma(png_row_info*, unsigned char* row, const unsigned char* gamma_table,
// const unsigned short* const* gamma_16_table, int gamma_shift): maps every colour sample of the row
// through a gamma table, in place.
// Guard: (bit_depth <= 8 && gamma_table != 0) || (bit_depth == 16 && gamma_16_table != 0), compiled as
// `cmp eax, 8` / `jg 0x10076793` (0x10076788, a SIGNED compare of the zero-extended byte), then
// `cmp dword ptr [ebp+0x10], 0` / `jne 0x100767ae` (0x1007678d) for the first half and
// `cmp edx, 0x10` / `jne 0x10076e99` (0x1007679b) plus `cmp dword ptr [ebp+0x14], 0` / `je 0x10076e99`
// (0x100767a4) for the second. color_type then selects the body through a jump table at 0x10076ea0
// behind the UNSIGNED range test `cmp dword ptr [ebp-0x38], 6` / `ja 0x10076e99` (0x100767b9): the
// seven entries are 0x10076c12 (0, grey), 0x10076e99 (1, nothing), 0x100767cd (2, RGB), 0x10076e99
// (3, nothing), 0x10076b48 (4, grey+alpha), 0x10076e99 (5, nothing) and 0x10076983 (6, RGB+alpha).
//   RGB: depth 8 (`cmp ecx, 8` / `jne 0x10076852` at 0x100767d5) maps three bytes per pixel; any other
//   depth takes the 16-bit body, which reads the two bytes of each sample as the pair
//   (gamma_16_table[*(sp+1) >> gamma_shift])[*sp] - the index is an ARITHMETIC shift of the second
//   byte (`sar eax, cl` at 0x10076881) and the inner index the first byte - and writes the result high
//   byte first (`sar edx, 8` at 0x100768a1) then low byte (0x100768b8), three samples per pixel.
//   RGB+alpha (0x10076983): the same two bodies with one extra byte skipped per pixel at depth 8 and
//   two extra at depth 16 (the `cmp ecx, 8` / `jne 0x10076a15` at 0x1007698b).
//   grey+alpha (0x10076b48): one sample per pixel with sp += 2 at depth 8 (`cmp ecx, 8` /
//   `jne 0x10076b94` at 0x10076b50) and sp += 4 at depth 16.
//   grey (0x10076c12): four separate depth tests in a row. bit_depth 2 (`cmp edx, 2` / `jne 0x10076d32`
//   at 0x10076c1a) walks the row four pixels at a time (`add ecx, 4` at 0x10076c35) and rebuilds each
//   of the four 2-bit fields by replicating it into a whole byte, looking that byte up and masking the
//   result back into its field (the four groups at 0x10076c4e, 0x10076c5e, 0x10076c6b, 0x10076c78 and
//   their shifts at 0x10076c81, 0x10076cc8, 0x10076cf1, 0x10076d1a). bit_depth 4 (`cmp eax, 4` /
//   `jne 0x10076dc4` at 0x10076d3a) does the same two nibbles at a time (`add edx, 2` at 0x10076d55).
//   bit_depth 8 (0x10076dcc) is one table lookup per byte and bit_depth 16 (`cmp edx, 0x10` /
//   `jne 0x10076e99` at 0x10076e1b) the same 16-bit pair as above. The depth-2 body falls THROUGH into
//   the depth-4 test, so a depth of 2 runs the depth-2 loop and then no other.
// Every loop bound is an UNSIGNED compare against the width (`jae` at 0x100767f5, 0x1007686d,
// 0x100769af, 0x10076a30, 0x10076b70, 0x10076baf, 0x10076c3e, 0x10076d5e, 0x10076dec, 0x10076e3b).
typedef void(__cdecl* PngDoGamma_t)(PngRowInfo*, unsigned char*, const unsigned char*,
                                    const unsigned short* const*, int);
PngDoGamma_t PngDoGamma_orig;
void __cdecl PngDoGamma_re(PngRowInfo* row_info, unsigned char* row, const unsigned char* gamma_table,
                           const unsigned short* const* gamma_16_table, int gamma_shift) {
    unsigned char* sp;
    unsigned i;
    unsigned row_width = row_info->width;

    if (!((static_cast<int>(row_info->bit_depth) <= 8 && gamma_table != 0) ||
          (row_info->bit_depth == 16 && gamma_16_table != 0)))
        return;

    switch (row_info->color_type) {
        case 2:
            if (row_info->bit_depth == 8) {
                sp = row;
                for (i = 0; i < row_width; i++) {
                    *sp = gamma_table[*sp]; sp++;
                    *sp = gamma_table[*sp]; sp++;
                    *sp = gamma_table[*sp]; sp++;
                }
            } else {
                sp = row;
                for (i = 0; i < row_width; i++) {
                    unsigned short v;
                    v = gamma_16_table[*(sp + 1) >> gamma_shift][*sp];
                    *sp = static_cast<unsigned char>((v >> 8) & 0xff);
                    *(sp + 1) = static_cast<unsigned char>(v & 0xff);
                    sp += 2;
                    v = gamma_16_table[*(sp + 1) >> gamma_shift][*sp];
                    *sp = static_cast<unsigned char>((v >> 8) & 0xff);
                    *(sp + 1) = static_cast<unsigned char>(v & 0xff);
                    sp += 2;
                    v = gamma_16_table[*(sp + 1) >> gamma_shift][*sp];
                    *sp = static_cast<unsigned char>((v >> 8) & 0xff);
                    *(sp + 1) = static_cast<unsigned char>(v & 0xff);
                    sp += 2;
                }
            }
            break;
        case 6:
            if (row_info->bit_depth == 8) {
                sp = row;
                for (i = 0; i < row_width; i++) {
                    *sp = gamma_table[*sp]; sp++;
                    *sp = gamma_table[*sp]; sp++;
                    *sp = gamma_table[*sp]; sp++;
                    sp++;
                }
            } else {
                sp = row;
                for (i = 0; i < row_width; i++) {
                    unsigned short v;
                    v = gamma_16_table[*(sp + 1) >> gamma_shift][*sp];
                    *sp = static_cast<unsigned char>((v >> 8) & 0xff);
                    *(sp + 1) = static_cast<unsigned char>(v & 0xff);
                    sp += 2;
                    v = gamma_16_table[*(sp + 1) >> gamma_shift][*sp];
                    *sp = static_cast<unsigned char>((v >> 8) & 0xff);
                    *(sp + 1) = static_cast<unsigned char>(v & 0xff);
                    sp += 2;
                    v = gamma_16_table[*(sp + 1) >> gamma_shift][*sp];
                    *sp = static_cast<unsigned char>((v >> 8) & 0xff);
                    *(sp + 1) = static_cast<unsigned char>(v & 0xff);
                    sp += 4;
                }
            }
            break;
        case 4:
            if (row_info->bit_depth == 8) {
                sp = row;
                for (i = 0; i < row_width; i++) {
                    *sp = gamma_table[*sp];
                    sp += 2;
                }
            } else {
                sp = row;
                for (i = 0; i < row_width; i++) {
                    unsigned short v = gamma_16_table[*(sp + 1) >> gamma_shift][*sp];
                    *sp = static_cast<unsigned char>((v >> 8) & 0xff);
                    *(sp + 1) = static_cast<unsigned char>(v & 0xff);
                    sp += 4;
                }
            }
            break;
        case 0:
            if (row_info->bit_depth == 2) {
                sp = row;
                for (i = 0; i < row_width; i += 4) {
                    int a = *sp & 0xc0;
                    int b = *sp & 0x30;
                    int c = *sp & 0x0c;
                    int d = *sp & 0x03;
                    *sp = static_cast<unsigned char>(
                        ((static_cast<int>(gamma_table[a | (a >> 2) | (a >> 4) | (a >> 6)])) & 0xc0) |
                        ((static_cast<int>(gamma_table[(b << 2) | b | (b >> 2) | (b >> 4)]) >> 2) & 0x30) |
                        ((static_cast<int>(gamma_table[(c << 4) | (c << 2) | c | (c >> 2)]) >> 4) & 0x0c) |
                        ((static_cast<int>(gamma_table[(d << 6) | (d << 4) | (d << 2) | d]) >> 6)));
                    sp++;
                }
            }
            if (row_info->bit_depth == 4) {
                sp = row;
                for (i = 0; i < row_width; i += 2) {
                    int msb = *sp & 0xf0;
                    int lsb = *sp & 0x0f;
                    *sp = static_cast<unsigned char>(
                        ((static_cast<int>(gamma_table[msb | (msb >> 4)])) & 0xf0) |
                        ((static_cast<int>(gamma_table[(lsb << 4) | lsb])) >> 4));
                    sp++;
                }
            } else if (row_info->bit_depth == 8) {
                sp = row;
                for (i = 0; i < row_width; i++) {
                    *sp = gamma_table[*sp];
                    sp++;
                }
            } else if (row_info->bit_depth == 16) {
                sp = row;
                for (i = 0; i < row_width; i++) {
                    unsigned short v = gamma_16_table[*(sp + 1) >> gamma_shift][*sp];
                    *sp = static_cast<unsigned char>((v >> 8) & 0xff);
                    *(sp + 1) = static_cast<unsigned char>(v & 0xff);
                    sp += 2;
                }
            }
            break;
    }
}

// 0x00077300  png_do_expand
// void __cdecl png_do_expand(png_row_info*, unsigned char* row, const png_color_16* trans_value):
// widens a low-depth grey row to eight bits and/or adds the alpha channel a tRNS chunk implies.
// The width is read into [ebp-0x18] (0x1007731a). `gray` is trans_value->gray when the pointer is not
// null and 0 otherwise (`cmp dword ptr [ebp+0x10], 0` / `je 0x10077344` at 0x10077330-0x10077334, the
// 16-bit load at 0x1007733b).
// color_type 0 (`test eax, eax` / `jne 0x10077788` at 0x10077328):
//   bit_depth < 8 (`cmp edx, 8` / `jge 0x100775bf` at 0x1007735b-0x1007735e, a SIGNED compare) unpacks
//   one byte per pixel backwards from sp = row + ((width-1) >> k) and dp = row + width - 1, with a
//   chain of three equality tests on the depth (1 at 0x1007736d, 2 at 0x10077373, 4 at 0x1007737d) and
//   no body for any other depth below 8. Each body also scales `gray` into the eight-bit range first:
//   times 0xff for depth 1 (`imul edx, edx, 0xff` at 0x10077395), 0x55 for depth 2 (0x10077447) and
//   0x11 for depth 4 (`imul eax, eax, 0x11` at 0x10077508). Depth 1 stores 0xff or 0 (0x100773fd),
//   depth 2 replicates the 2-bit value into all four fields (the shifts at 0x100774b2, 0x100774bd,
//   0x100774c5) and depth 4 both nibbles (`shl eax, 4` at 0x10077571). The depth-4 body advances its
//   shift with a plain assignment of 4 rather than an increment (0x10077596), so after the first pixel
//   it alternates 4 and 0. Whatever ran, the tail sets bit_depth 8, pixel_depth 8 and rowbytes = width
//   (0x100775a8-0x100775bc).
//   trans_value not null (`cmp dword ptr [ebp+0x10], 0` / `je 0x10077783` at 0x100775bf): at bit_depth
//   8 (`cmp ecx, 8` / `jne 0x10077664` at 0x100775d1) two bytes per pixel are written backwards from
//   dp = row + (width << 1) - 1, the alpha being 0 when the sample equals `gray` (`cmp edx, eax` /
//   `jne 0x10077632` at 0x1007761d) and 0xff otherwise; at bit_depth 16 (0x1007766c) four bytes per
//   pixel from dp = row + (rowbytes << 1) - 1, comparing the big-endian pair (`shl ecx, 8` at
//   0x100776c2) with `gray`. The header then becomes color_type 4, channels 2, pixel_depth
//   bit_depth * 2 (`shl ecx, 1` at 0x10077764) and rowbytes (width * pixel_depth) >> 3 (0x10077777).
// color_type 2 with a non-null trans_value (`cmp edx, 2` / `jne 0x10077a7d` at 0x10077790 and
// `cmp dword ptr [ebp+0x10], 0` / `je 0x10077a7d` at 0x10077799): at bit_depth 8 four bytes per pixel
// from dp = row + (width << 2) - 1, the alpha being 0 only when all three of red, green and blue match
// trans_value (the three compares at 0x10077800, 0x10077815, 0x10077829, each jumping to the 0xff
// store at 0x1007783e); at bit_depth 16 eight bytes per pixel with the same three comparisons on
// big-endian pairs (0x10077915, 0x10077937, 0x10077958). The header becomes color_type 6, channels 4,
// pixel_depth bit_depth * 4 (`shl ecx, 2` at 0x10077a5d) and the same rowbytes formula (0x10077a71).
// Every loop bound is an UNSIGNED compare against the width (`jae` at 0x100773e5, 0x10077496,
// 0x10077557, 0x10077609, 0x100776aa, 0x100777e6, 0x100778ee).
typedef void(__cdecl* PngDoExpand_t)(PngRowInfo*, unsigned char*, const PngColor16*);
PngDoExpand_t PngDoExpand_orig;
void __cdecl PngDoExpand_re(PngRowInfo* row_info, unsigned char* row, const PngColor16* trans_value) {
    int shift, value;
    unsigned char* sp;
    unsigned char* dp;
    unsigned i;
    unsigned row_width = row_info->width;

    if (row_info->color_type == 0) {
        unsigned short gray = static_cast<unsigned short>(trans_value ? trans_value->gray : 0);

        if (static_cast<int>(row_info->bit_depth) < 8) {
            if (row_info->bit_depth == 1) {
                gray = static_cast<unsigned short>(gray * 0xff);
                sp = row + ((row_width - 1) >> 3);
                dp = row + row_width - 1;
                shift = 7 - static_cast<int>((row_width + 7) & 7);
                for (i = 0; i < row_width; i++) {
                    if ((*sp >> shift) & 1)
                        *dp = 0xff;
                    else
                        *dp = 0;
                    if (shift == 7) {
                        shift = 0;
                        sp--;
                    } else {
                        shift++;
                    }
                    dp--;
                }
            } else if (row_info->bit_depth == 2) {
                gray = static_cast<unsigned short>(gray * 0x55);
                sp = row + ((row_width - 1) >> 2);
                dp = row + row_width - 1;
                shift = static_cast<int>(3 - ((row_width + 3) & 3)) << 1;
                for (i = 0; i < row_width; i++) {
                    value = (*sp >> shift) & 3;
                    *dp = static_cast<unsigned char>(value | (value << 2) | (value << 4) | (value << 6));
                    if (shift == 6) {
                        shift = 0;
                        sp--;
                    } else {
                        shift += 2;
                    }
                    dp--;
                }
            } else if (row_info->bit_depth == 4) {
                gray = static_cast<unsigned short>(gray * 0x11);
                sp = row + ((row_width - 1) >> 1);
                dp = row + row_width - 1;
                shift = static_cast<int>(1 - ((row_width + 1) & 1)) << 2;
                for (i = 0; i < row_width; i++) {
                    value = (*sp >> shift) & 0xf;
                    *dp = static_cast<unsigned char>(value | (value << 4));
                    if (shift == 4) {
                        shift = 0;
                        sp--;
                    } else {
                        shift = 4;   // 0x10077596: an assignment, not an increment
                    }
                    dp--;
                }
            }
            row_info->bit_depth = 8;
            row_info->pixel_depth = 8;
            row_info->rowbytes = row_width;
        }

        if (trans_value != 0) {
            if (row_info->bit_depth == 8) {
                sp = row + row_width - 1;
                dp = row + (row_width << 1) - 1;
                for (i = 0; i < row_width; i++) {
                    if (*sp == gray)
                        *dp-- = 0;
                    else
                        *dp-- = 0xff;
                    *dp-- = *sp--;
                }
            } else if (row_info->bit_depth == 16) {
                sp = row + row_info->rowbytes - 1;
                dp = row + (row_info->rowbytes << 1) - 1;
                for (i = 0; i < row_width; i++) {
                    if ((static_cast<unsigned short>(*sp) |
                         static_cast<unsigned short>(*(sp - 1) << 8)) == gray) {
                        *dp-- = 0;
                        *dp-- = 0;
                    } else {
                        *dp-- = 0xff;
                        *dp-- = 0xff;
                    }
                    *dp-- = *sp--;
                    *dp-- = *sp--;
                }
            }
            row_info->color_type = 4;
            row_info->channels = 2;
            row_info->pixel_depth = static_cast<unsigned char>(row_info->bit_depth << 1);
            row_info->rowbytes = (row_width * row_info->pixel_depth) >> 3;
        }
    } else if (row_info->color_type == 2 && trans_value) {
        if (row_info->bit_depth == 8) {
            sp = row + row_info->rowbytes - 1;
            dp = row + (row_width << 2) - 1;
            for (i = 0; i < row_width; i++) {
                if (*(sp - 2) == trans_value->red && *(sp - 1) == trans_value->green &&
                    *(sp - 0) == trans_value->blue)
                    *dp-- = 0;
                else
                    *dp-- = 0xff;
                *dp-- = *sp--;
                *dp-- = *sp--;
                *dp-- = *sp--;
            }
        } else if (row_info->bit_depth == 16) {
            sp = row + row_info->rowbytes - 1;
            dp = row + (row_width << 3) - 1;
            for (i = 0; i < row_width; i++) {
                if ((static_cast<unsigned short>(*(sp - 4)) |
                     static_cast<unsigned short>(*(sp - 5) << 8)) == trans_value->red &&
                    (static_cast<unsigned short>(*(sp - 2)) |
                     static_cast<unsigned short>(*(sp - 3) << 8)) == trans_value->green &&
                    (static_cast<unsigned short>(*(sp - 0)) |
                     static_cast<unsigned short>(*(sp - 1) << 8)) == trans_value->blue) {
                    *dp-- = 0;
                    *dp-- = 0;
                } else {
                    *dp-- = 0xff;
                    *dp-- = 0xff;
                }
                *dp-- = *sp--;
                *dp-- = *sp--;
                *dp-- = *sp--;
                *dp-- = *sp--;
                *dp-- = *sp--;
                *dp-- = *sp--;
            }
        }
        row_info->color_type = 6;
        row_info->channels = 4;
        row_info->pixel_depth = static_cast<unsigned char>(row_info->bit_depth << 2);
        row_info->rowbytes = (row_width * row_info->pixel_depth) >> 3;
    }
}

// 0x000736c0  png_do_rgb_to_gray
// int __cdecl png_do_rgb_to_gray(png_struct*, png_row_info*, unsigned char* row): collapses a colour
// row to one grey sample per pixel and returns 1 when any pixel was not already grey.
// png_struct fields, all read here: gamma_from_1 +0x168 (0x10073749), gamma_to_1 +0x16c (0x10073759),
// gamma_16_from_1 +0x174 (0x100739b3), gamma_16_to_1 +0x178 (0x100739a3), gamma_shift +0x158 read as a
// DWORD (0x10073a96) and the three coefficients rgb_to_gray_red_coeff +0x215 (0x10073703),
// _green_coeff +0x216 (0x1007370f) and _blue_coeff +0x217 (0x1007371b), each a zero-extended byte.
// Guard: color_type & 2 (`and eax, 2` / `je 0x10074624` at 0x100736f5-0x100736fa); without the colour
// bit nothing is read, nothing is written and the result is the initial 0 (0x100736e6).
// color_type 2 (`cmp edx, 2` / `jne 0x10073d4a` at 0x1007372c) and color_type 6 (`cmp eax, 6` /
// `jne 0x100745d3` at 0x10073d52) are tested one after the other, so a colour type that is neither
// (3, 7) runs no loop but still gets the header rewrite. Each splits on bit_depth 8 (0x1007373d,
// 0x10073d63) and then on whether BOTH gamma pointers of that depth are set (+0x168 and +0x16c at
// 0x10073749 / 0x10073759 and 0x10073d6f / 0x10073d7f; +0x178 and +0x174 at 0x100739a3 / 0x100739b3
// and their RGBA twins), giving eight bodies.
//   8-bit without gamma: three bytes are read, `rgb_error |= 1` when they are not all equal (the two
//   compares at 0x10073820 and 0x10073833 for RGB) and one byte is written - the weighted sum
//   (rc*r + gc*g + bc*b) >> 8 with an ARITHMETIC shift (0x1007387c, 0x10074035) for the pixels that
//   differ and, in the RGB body only, the third source byte itself when they are equal (0x10073884).
//   The RGBA body always writes the weighted sum and copies the alpha byte, and its first product is
//   the GREEN coefficient times red, not the red one: [ebp-0x14] is loaded from +0x216 at 0x10073715
//   and is the factor at 0x10073fea, while [ebp-0x10] (+0x215) is never used on that path.
//   8-bit with gamma: the same, with each source byte first mapped through gamma_to_1 (0x1007379a) and
//   the weighted sum used as an index into gamma_from_1 (0x10073867, 0x10073ecb).
//   16-bit without gamma: the three big-endian samples are combined by the same weighted sum and the
//   16-bit result is written high byte first; `rgb_error` is set when they are not all equal.
//   16-bit with gamma: a pixel whose three samples are equal keeps its value, otherwise each sample is
//   mapped through gamma_16_to_1[(sample & 0xff) >> gamma_shift][sample >> 8] (the index pair at
//   0x10073a8d / 0x10073ab0), the weighted sum is taken and mapped back through gamma_16_from_1 the
//   same way (0x10073b95), and `rgb_error` is set.
// The tail (0x100745d3) runs for every colour type that passed the mask test: channels -= 2
// (0x100745dc), the colour bit is cleared from color_type (`and cl, 0xfd` at 0x100745e7),
// pixel_depth = channels * bit_depth (0x100745fc) and rowbytes = (width * pixel_depth + 7) >> 3
// (0x1007461b, an unsigned shift).
// Every loop bound is an UNSIGNED compare against the width (`jae` at 0x1007378a, 0x100739f6,
// 0x10073bf0, 0x10073cc8, 0x10073da7, 0x10073f4a, 0x100740ce, 0x100742e8, 0x100744a4).
typedef int(__cdecl* PngDoRgbToGray_t)(void*, PngRowInfo*, unsigned char*);
PngDoRgbToGray_t PngDoRgbToGray_orig;
int __cdecl PngDoRgbToGray_re(void* png_ptr, PngRowInfo* row_info, unsigned char* row) {
    char* png = static_cast<char*>(png_ptr);
    unsigned i;
    unsigned row_width = row_info->width;
    int rgb_error = 0;

    if (!(row_info->color_type & 2)) return rgb_error;

    {
        const unsigned char* gamma_from_1 = *reinterpret_cast<const unsigned char**>(png + 0x168);
        const unsigned char* gamma_to_1 = *reinterpret_cast<const unsigned char**>(png + 0x16c);
        const unsigned short* const* gamma_16_from_1 =
            *reinterpret_cast<const unsigned short* const**>(png + 0x174);
        const unsigned short* const* gamma_16_to_1 =
            *reinterpret_cast<const unsigned short* const**>(png + 0x178);
        int gamma_shift = *reinterpret_cast<const int*>(png + 0x158);
        unsigned char rc = *reinterpret_cast<const unsigned char*>(png + 0x215);
        unsigned char gc = *reinterpret_cast<const unsigned char*>(png + 0x216);
        unsigned char bc = *reinterpret_cast<const unsigned char*>(png + 0x217);

        if (row_info->color_type == 2) {
            if (row_info->bit_depth == 8) {
                if (gamma_from_1 != 0 && gamma_to_1 != 0) {
                    unsigned char* sp = row;
                    unsigned char* dp = row;
                    for (i = 0; i < row_width; i++) {
                        unsigned char red = gamma_to_1[*(sp++)];
                        unsigned char green = gamma_to_1[*(sp++)];
                        unsigned char blue = gamma_to_1[*(sp++)];
                        if (red != green || red != blue) {
                            rgb_error |= 1;
                            *(dp++) = gamma_from_1[(rc * red + gc * green + bc * blue) >> 8];
                        } else {
                            *(dp++) = *(sp - 1);
                        }
                    }
                } else {
                    unsigned char* sp = row;
                    unsigned char* dp = row;
                    for (i = 0; i < row_width; i++) {
                        unsigned char red = *(sp++);
                        unsigned char green = *(sp++);
                        unsigned char blue = *(sp++);
                        if (red != green || red != blue) {
                            rgb_error |= 1;
                            *(dp++) = static_cast<unsigned char>((rc * red + gc * green + bc * blue) >> 8);
                        } else {
                            *(dp++) = *(sp - 1);
                        }
                    }
                }
            } else {
                if (gamma_16_to_1 != 0 && gamma_16_from_1 != 0) {
                    unsigned char* sp = row;
                    unsigned char* dp = row;
                    for (i = 0; i < row_width; i++) {
                        unsigned short red, green, blue, w;
                        red = static_cast<unsigned short>(((*sp) << 8) | *(sp + 1)); sp += 2;
                        green = static_cast<unsigned short>(((*sp) << 8) | *(sp + 1)); sp += 2;
                        blue = static_cast<unsigned short>(((*sp) << 8) | *(sp + 1)); sp += 2;
                        if (red == green && red == blue) {
                            w = red;
                        } else {
                            unsigned short red_1 = gamma_16_to_1[(red & 0xff) >> gamma_shift][red >> 8];
                            unsigned short green_1 = gamma_16_to_1[(green & 0xff) >> gamma_shift][green >> 8];
                            unsigned short blue_1 = gamma_16_to_1[(blue & 0xff) >> gamma_shift][blue >> 8];
                            unsigned short gray16 =
                                static_cast<unsigned short>((rc * red_1 + gc * green_1 + bc * blue_1) >> 8);
                            w = gamma_16_from_1[(gray16 & 0xff) >> gamma_shift][gray16 >> 8];
                            rgb_error |= 1;
                        }
                        *(dp++) = static_cast<unsigned char>((w >> 8) & 0xff);
                        *(dp++) = static_cast<unsigned char>(w & 0xff);
                    }
                } else {
                    unsigned char* sp = row;
                    unsigned char* dp = row;
                    for (i = 0; i < row_width; i++) {
                        unsigned short red, green, blue, gray16;
                        red = static_cast<unsigned short>(((*sp) << 8) | *(sp + 1)); sp += 2;
                        green = static_cast<unsigned short>(((*sp) << 8) | *(sp + 1)); sp += 2;
                        blue = static_cast<unsigned short>(((*sp) << 8) | *(sp + 1)); sp += 2;
                        if (red != green || red != blue) rgb_error |= 1;
                        gray16 = static_cast<unsigned short>((rc * red + gc * green + bc * blue) >> 8);
                        *(dp++) = static_cast<unsigned char>((gray16 >> 8) & 0xff);
                        *(dp++) = static_cast<unsigned char>(gray16 & 0xff);
                    }
                }
            }
        }
        if (row_info->color_type == 6) {
            if (row_info->bit_depth == 8) {
                if (gamma_from_1 != 0 && gamma_to_1 != 0) {
                    unsigned char* sp = row;
                    unsigned char* dp = row;
                    for (i = 0; i < row_width; i++) {
                        unsigned char red = gamma_to_1[*(sp++)];
                        unsigned char green = gamma_to_1[*(sp++)];
                        unsigned char blue = gamma_to_1[*(sp++)];
                        if (red != green || red != blue) rgb_error |= 1;
                        *(dp++) = gamma_from_1[(rc * red + gc * green + bc * blue) >> 8];
                        *(dp++) = *(sp++);
                    }
                } else {
                    unsigned char* sp = row;
                    unsigned char* dp = row;
                    for (i = 0; i < row_width; i++) {
                        unsigned char red = *(sp++);
                        unsigned char green = *(sp++);
                        unsigned char blue = *(sp++);
                        if (red != green || red != blue) rgb_error |= 1;
                        // 0x10073fea: the first factor is the GREEN coefficient, not the red one.
                        *(dp++) = static_cast<unsigned char>((gc * red + gc * green + bc * blue) >> 8);
                        *(dp++) = *(sp++);
                    }
                }
            } else {
                if (gamma_16_to_1 != 0 && gamma_16_from_1 != 0) {
                    unsigned char* sp = row;
                    unsigned char* dp = row;
                    for (i = 0; i < row_width; i++) {
                        unsigned short red, green, blue, w;
                        red = static_cast<unsigned short>(((*sp) << 8) | *(sp + 1)); sp += 2;
                        green = static_cast<unsigned short>(((*sp) << 8) | *(sp + 1)); sp += 2;
                        blue = static_cast<unsigned short>(((*sp) << 8) | *(sp + 1)); sp += 2;
                        if (red == green && red == blue) {
                            w = red;
                        } else {
                            unsigned short red_1 = gamma_16_to_1[(red & 0xff) >> gamma_shift][red >> 8];
                            unsigned short green_1 = gamma_16_to_1[(green & 0xff) >> gamma_shift][green >> 8];
                            unsigned short blue_1 = gamma_16_to_1[(blue & 0xff) >> gamma_shift][blue >> 8];
                            unsigned short gray16 =
                                static_cast<unsigned short>((rc * red_1 + gc * green_1 + bc * blue_1) >> 8);
                            w = gamma_16_from_1[(gray16 & 0xff) >> gamma_shift][gray16 >> 8];
                            rgb_error |= 1;
                        }
                        *(dp++) = static_cast<unsigned char>((w >> 8) & 0xff);
                        *(dp++) = static_cast<unsigned char>(w & 0xff);
                        *(dp++) = *(sp++);
                        *(dp++) = *(sp++);
                    }
                } else {
                    unsigned char* sp = row;
                    unsigned char* dp = row;
                    for (i = 0; i < row_width; i++) {
                        unsigned short red, green, blue, gray16;
                        red = static_cast<unsigned short>(((*sp) << 8) | *(sp + 1)); sp += 2;
                        green = static_cast<unsigned short>(((*sp) << 8) | *(sp + 1)); sp += 2;
                        blue = static_cast<unsigned short>(((*sp) << 8) | *(sp + 1)); sp += 2;
                        if (red != green || red != blue) rgb_error |= 1;
                        gray16 = static_cast<unsigned short>((rc * red + gc * green + bc * blue) >> 8);
                        *(dp++) = static_cast<unsigned char>((gray16 >> 8) & 0xff);
                        *(dp++) = static_cast<unsigned char>(gray16 & 0xff);
                        *(dp++) = *(sp++);
                        *(dp++) = *(sp++);
                    }
                }
            }
        }
        row_info->channels = static_cast<unsigned char>(row_info->channels - 2);
        row_info->color_type = static_cast<unsigned char>(row_info->color_type & 0xfd);
        row_info->pixel_depth = static_cast<unsigned char>(row_info->channels * row_info->bit_depth);
        row_info->rowbytes = (row_width * row_info->pixel_depth + 7) >> 3;
    }
    return rgb_error;
}

// 0x0007c810  png_do_read_interlace
// void __cdecl png_do_read_interlace(png_row_info*, unsigned char* row, int pass,
// unsigned transformations): spreads the pixels of one interlace pass over the full row width,
// working backwards so the row grows in place.
// Guard: both pointers must be non-null (`cmp dword ptr [ebp+0xc], 0` / `je 0x1007ce63` at 0x1007c82e
// and `cmp dword ptr [ebp+8], 0` / `je 0x1007ce63` at 0x1007c838).
// final_width = width * png_pass_inc[pass], the module table at 0x10123244 read as
// `imul edx, dword ptr [ecx*4 + 0x10123244]` (0x1007c84a); the same entry is the inner repeat count.
// pixel_depth selects the body with three equality tests (1 at 0x1007c861, 2 at 0x1007c86a, 4 at
// 0x1007c877) and a default at 0x1007cd34 for everything else.
//   depths 1, 2 and 4 walk packed pixels: sp = row + ((width-1) >> k), dp = row + ((final_width-1) >> k)
//   and the two shifts come from the width and the final width modulo the pixels per byte, with
//   transformations & 0x10000 (`and eax, 0x10000` at 0x1007c8a9, 0x1007ca2c, 0x1007cbb6) choosing
//   whether the shift counts up from 0 or down from the top of the byte. Each source pixel is masked
//   out of *sp and written png_pass_inc[pass] times into *dp, each write clearing its own field first
//   with the masks 0x7f7f, 0x3f3f and 0xf0f shifted right by (7|6|4) - dshift (0x1007c93a, 0x1007cab9,
//   0x1007cc3e) and then OR-ing the value in.
//   the default body (0x1007cd34) copies whole pixels: pixel_bytes = pixel_depth >> 3 (an ARITHMETIC
//   shift at 0x1007cd49 of the zero-extended byte), sp = row + (width-1) * pixel_bytes,
//   dp = row + (final_width-1) * pixel_bytes, and each source pixel is first copied into an 8-byte
//   stack buffer with the CRT memcpy 0x1007f3a0 (`call` at 0x1007cd9c) and then out of it
//   png_pass_inc[pass] times (`call` at 0x1007cdd6), dp stepping back by pixel_bytes each time.
// Whatever ran, the tail sets width = final_width (0x1007ce4a) and rowbytes = (final_width *
// pixel_depth + 7) >> 3 (0x1007ce60, an unsigned shift).
typedef void(__cdecl* PngDoReadInterlace_t)(PngRowInfo*, unsigned char*, int, unsigned);
PngDoReadInterlace_t PngDoReadInterlace_orig;
void __cdecl PngDoReadInterlace_re(PngRowInfo* row_info, unsigned char* row, int pass,
                                   unsigned transformations) {
    if (row == 0 || row_info == 0) return;
    {
        const unsigned* pass_inc = PngPassInc();
        unsigned final_width = row_info->width * pass_inc[pass];
        int jstop = static_cast<int>(pass_inc[pass]);
        unsigned i;
        int j;

        if (row_info->pixel_depth == 1) {
            unsigned char* sp = row + ((row_info->width - 1) >> 3);
            unsigned char* dp = row + ((final_width - 1) >> 3);
            int sshift, dshift, s_start, s_end, s_inc;
            unsigned char v;
            if (transformations & 0x10000) {
                sshift = static_cast<int>((row_info->width + 7) & 7);
                dshift = static_cast<int>((final_width + 7) & 7);
                s_start = 7; s_end = 0; s_inc = -1;
            } else {
                sshift = 7 - static_cast<int>((row_info->width + 7) & 7);
                dshift = 7 - static_cast<int>((final_width + 7) & 7);
                s_start = 0; s_end = 7; s_inc = 1;
            }
            for (i = 0; i < row_info->width; i++) {
                v = static_cast<unsigned char>((*sp >> sshift) & 1);
                for (j = 0; j < jstop; j++) {
                    *dp = static_cast<unsigned char>(*dp & ((0x7f7f >> (7 - dshift)) & 0xff));
                    *dp = static_cast<unsigned char>(*dp | (v << dshift));
                    if (dshift == s_end) { dshift = s_start; dp--; } else { dshift += s_inc; }
                }
                if (sshift == s_end) { sshift = s_start; sp--; } else { sshift += s_inc; }
            }
        } else if (row_info->pixel_depth == 2) {
            unsigned char* sp = row + ((row_info->width - 1) >> 2);
            unsigned char* dp = row + ((final_width - 1) >> 2);
            int sshift, dshift, s_start, s_end, s_inc;
            if (transformations & 0x10000) {
                sshift = static_cast<int>(((row_info->width + 3) & 3) << 1);
                dshift = static_cast<int>(((final_width + 3) & 3) << 1);
                s_start = 6; s_end = 0; s_inc = -2;
            } else {
                sshift = static_cast<int>((3 - ((row_info->width + 3) & 3)) << 1);
                dshift = static_cast<int>((3 - ((final_width + 3) & 3)) << 1);
                s_start = 0; s_end = 6; s_inc = 2;
            }
            for (i = 0; i < row_info->width; i++) {
                unsigned char v = static_cast<unsigned char>((*sp >> sshift) & 3);
                for (j = 0; j < jstop; j++) {
                    *dp = static_cast<unsigned char>(*dp & ((0x3f3f >> (6 - dshift)) & 0xff));
                    *dp = static_cast<unsigned char>(*dp | (v << dshift));
                    if (dshift == s_end) { dshift = s_start; dp--; } else { dshift += s_inc; }
                }
                if (sshift == s_end) { sshift = s_start; sp--; } else { sshift += s_inc; }
            }
        } else if (row_info->pixel_depth == 4) {
            unsigned char* sp = row + ((row_info->width - 1) >> 1);
            unsigned char* dp = row + ((final_width - 1) >> 1);
            int sshift, dshift, s_start, s_end, s_inc;
            if (transformations & 0x10000) {
                sshift = static_cast<int>(((row_info->width + 1) & 1) << 2);
                dshift = static_cast<int>(((final_width + 1) & 1) << 2);
                s_start = 4; s_end = 0; s_inc = -4;
            } else {
                sshift = static_cast<int>((1 - ((row_info->width + 1) & 1)) << 2);
                dshift = static_cast<int>((1 - ((final_width + 1) & 1)) << 2);
                s_start = 0; s_end = 4; s_inc = 4;
            }
            for (i = 0; i < row_info->width; i++) {
                unsigned char v = static_cast<unsigned char>((*sp >> sshift) & 0xf);
                for (j = 0; j < jstop; j++) {
                    *dp = static_cast<unsigned char>(*dp & ((0xf0f >> (4 - dshift)) & 0xff));
                    *dp = static_cast<unsigned char>(*dp | (v << dshift));
                    if (dshift == s_end) { dshift = s_start; dp--; } else { dshift += s_inc; }
                }
                if (sshift == s_end) { sshift = s_start; sp--; } else { sshift += s_inc; }
            }
        } else {
            unsigned pixel_bytes = static_cast<unsigned>(static_cast<int>(row_info->pixel_depth) >> 3);
            unsigned char* sp = row + (row_info->width - 1) * pixel_bytes;
            unsigned char* dp = row + (final_width - 1) * pixel_bytes;
            for (i = 0; i < row_info->width; i++) {
                unsigned char v[8];
                JgldMemcpy()(v, sp, pixel_bytes);
                for (j = 0; j < jstop; j++) {
                    JgldMemcpy()(dp, v, pixel_bytes);
                    dp -= pixel_bytes;
                }
                sp -= pixel_bytes;
            }
        }
        row_info->width = final_width;
        row_info->rowbytes = (final_width * static_cast<unsigned>(row_info->pixel_depth) + 7) >> 3;
    }
}

// 0x0007c310  png_combine_row
// void __cdecl png_combine_row(png_struct*, unsigned char* row, int mask): merges the pixels of the
// row just read (png_ptr->row_buf + 1) into the caller's row, keeping only the pixels the mask selects.
// png_struct fields read here: transformations +0x70 (0x1007c3d5), width +0xc8 (0x1007c345), row_buf
// +0xec (0x1007c358) and row_info.pixel_depth +0x10b (0x1007c33c).
// mask 0xff (`cmp dword ptr [ebp+0x10], 0xff` / `jne 0x1007c373` at 0x1007c32e) copies the whole row
// with the CRT memcpy 0x1007f3a0 over (width * pixel_depth + 7) >> 3 bytes (`imul eax, ecx` at
// 0x1007c34b, the unsigned shift at 0x1007c351, `call` at 0x1007c366) and returns.
// Otherwise pixel_depth selects the body with three equality tests (1 at 0x1007c382, 2 at 0x1007c38b,
// 4 at 0x1007c398) and a default at 0x1007c718. A bit walker `m` starts at 0x80 (0x1007c3bf) and
// halves after every pixel, wrapping back to 0x80 when it reaches 1 (`cmp dword ptr [ebp-0x18], 1` /
// `jne 0x1007c4bc` at 0x1007c4ad; the twins at 0x1007c5d3, 0x1007c6f7 and 0x1007c7ce); a pixel is
// copied only when m & mask is non-zero (`test ecx, ecx` / `je ...` at 0x1007c437, 0x1007c55c,
// 0x1007c681, 0x1007c78f).
//   depths 1, 2 and 4 take the packed field out of *sp, clear the same field of *dp with the masks
//   0x7f7f, 0x3f3f and 0xf0f shifted right by (7|6|4) - shift and OR the value in; transformations &
//   0x10000 (0x1007c3d8, 0x1007c4fd, 0x1007c622) chooses whether the shift counts up from 0 or down
//   from the top of the byte, and both pointers step one byte on every wrap of the shift.
//   the default body (0x1007c718) copies pixel_bytes = pixel_depth >> 3 bytes per selected pixel with
//   the CRT memcpy (`call` at 0x1007c7a2) and advances both pointers by pixel_bytes every pixel,
//   selected or not (0x1007c7ad, 0x1007c7b9).
// Every loop bound is an UNSIGNED compare against the width (`jae` at 0x1007c428, 0x1007c54d,
// 0x1007c672, 0x1007c778).
typedef void(__cdecl* PngCombineRow_t)(void*, unsigned char*, int);
PngCombineRow_t PngCombineRow_orig;
void __cdecl PngCombineRow_re(void* png_ptr, unsigned char* row, int mask) {
    char* png = static_cast<char*>(png_ptr);
    unsigned png_width = *reinterpret_cast<const unsigned*>(png + 0xc8);
    unsigned transformations = *reinterpret_cast<const unsigned*>(png + 0x70);
    unsigned char* row_buf = *reinterpret_cast<unsigned char**>(png + 0xec);
    unsigned pixel_depth = *reinterpret_cast<const unsigned char*>(png + 0x10b);
    unsigned i;

    if (mask == 0xff) {
        JgldMemcpy()(row, row_buf + 1, (png_width * pixel_depth + 7) >> 3);
        return;
    }

    if (pixel_depth == 1) {
        unsigned char* sp = row_buf + 1;
        unsigned char* dp = row;
        int s_inc, s_start, s_end, shift, m = 0x80;
        if (transformations & 0x10000) { s_start = 0; s_end = 7; s_inc = 1; }
        else { s_start = 7; s_end = 0; s_inc = -1; }
        shift = s_start;
        for (i = 0; i < png_width; i++) {
            if (m & mask) {
                int value = (*sp >> shift) & 1;
                *dp = static_cast<unsigned char>(*dp & ((0x7f7f >> (7 - shift)) & 0xff));
                *dp = static_cast<unsigned char>(*dp | (value << shift));
            }
            if (shift == s_end) { shift = s_start; sp++; dp++; } else { shift += s_inc; }
            if (m == 1) m = 0x80; else m >>= 1;
        }
    } else if (pixel_depth == 2) {
        unsigned char* sp = row_buf + 1;
        unsigned char* dp = row;
        int s_inc, s_start, s_end, shift, value, m = 0x80;
        if (transformations & 0x10000) { s_start = 0; s_end = 6; s_inc = 2; }
        else { s_start = 6; s_end = 0; s_inc = -2; }
        shift = s_start;
        for (i = 0; i < png_width; i++) {
            if (m & mask) {
                value = (*sp >> shift) & 3;
                *dp = static_cast<unsigned char>(*dp & ((0x3f3f >> (6 - shift)) & 0xff));
                *dp = static_cast<unsigned char>(*dp | (value << shift));
            }
            if (shift == s_end) { shift = s_start; sp++; dp++; } else { shift += s_inc; }
            if (m == 1) m = 0x80; else m >>= 1;
        }
    } else if (pixel_depth == 4) {
        unsigned char* sp = row_buf + 1;
        unsigned char* dp = row;
        int s_inc, s_start, s_end, shift, value, m = 0x80;
        if (transformations & 0x10000) { s_start = 0; s_end = 4; s_inc = 4; }
        else { s_start = 4; s_end = 0; s_inc = -4; }
        shift = s_start;
        for (i = 0; i < png_width; i++) {
            if (m & mask) {
                value = (*sp >> shift) & 0xf;
                *dp = static_cast<unsigned char>(*dp & ((0xf0f >> (4 - shift)) & 0xff));
                *dp = static_cast<unsigned char>(*dp | (value << shift));
            }
            if (shift == s_end) { shift = s_start; sp++; dp++; } else { shift += s_inc; }
            if (m == 1) m = 0x80; else m >>= 1;
        }
    } else {
        unsigned char* sp = row_buf + 1;
        unsigned char* dp = row;
        unsigned pixel_bytes = pixel_depth >> 3;
        unsigned char m = 0x80;
        for (i = 0; i < png_width; i++) {
            if (m & mask) JgldMemcpy()(dp, sp, pixel_bytes);
            sp += pixel_bytes;
            dp += pixel_bytes;
            if (m == 1) m = 0x80; else m = static_cast<unsigned char>(m >> 1);
        }
    }
}

// 0x0007e120  png_set_sRGB_gAMA_and_cHRM
// void __cdecl png_set_sRGB_gAMA_and_cHRM(png_struct*, png_info*, int intent): records an sRGB intent
// and, with it, the gamma and the chromaticities the sRGB colour space is defined by.
// Guard: `cmp dword ptr [ebp+8], 0` / `je 0x1007e144` (0x1007e138) and `cmp dword ptr [ebp+0xc], 0` /
// `jne 0x1007e149` (0x1007e13e): either pointer null and nothing happens.
// It then makes three calls in a fixed order, and all three values it passes are constants held in the
// body as 32-bit float patterns written to stack slots and widened to double by `fld dword` /
// `fstp qword`, so no rounding happens on the way:
//   png_set_sRGB 0x1007e0d0 with the caller's intent (`call` at 0x1007e155), which stores it as a byte
//   at png_info+0x2c and ORs 0x10 into the valid word at +8;
//   png_set_gAMA 0x1007dbd0 with 0x3ee8bac7 = 0.45455f (stored at 0x1007e15d, `call` at 0x1007e175),
//   which stores it as a float at png_info+0x28 and ORs 1 into +8;
//   png_set_cHRM 0x1007db30 with the eight chromaticities 0x3ea01a37, 0x3ea872b0, 0x3f23d70a,
//   0x3ea8f5c3, 0x3e99999a, 0x3f19999a, 0x3e19999a and 0x3d75c28f (stored at 0x1007e17d-0x1007e1ae and
//   pushed in reverse from 0x1007e1b5 to 0x1007e1fa, `call` at 0x1007e205), which stores them as
//   floats at png_info+0x80..+0x9c and ORs 4 into +8.
typedef void(__cdecl* PngSetSRGBgAMAcHRM_t)(void*, void*, int);
PngSetSRGBgAMAcHRM_t PngSetSRGBgAMAcHRM_orig;

// The eight-digit pattern IS the float the body stores into its stack slot; bit-casting it is the
// faithful reading of `mov dword ptr [ebp-N], <pattern>` followed by `fld dword ptr [ebp-N]`.
float FloatBits(unsigned bits) {
    float f;
    *reinterpret_cast<unsigned*>(&f) = bits;
    return f;
}

void __cdecl PngSetSRGBgAMAcHRM_re(void* png_ptr, void* info_ptr, int intent) {
    if (png_ptr == 0 || info_ptr == 0) return;

    reinterpret_cast<PngSetSRGB_t>(JgldBase() + 0x7e0d0)(png_ptr, info_ptr, intent);

    {
        float file_gamma = FloatBits(0x3ee8bac7);        // 0x1007e15d
        reinterpret_cast<PngSetGAMA_t>(JgldBase() + 0x7dbd0)(png_ptr, info_ptr, file_gamma);
    }
    {
        float white_x = FloatBits(0x3ea01a37);           // 0x1007e17d
        float white_y = FloatBits(0x3ea872b0);           // 0x1007e184
        float red_x   = FloatBits(0x3f23d70a);           // 0x1007e18b
        float red_y   = FloatBits(0x3ea8f5c3);           // 0x1007e192
        float green_x = FloatBits(0x3e99999a);           // 0x1007e199
        float green_y = FloatBits(0x3f19999a);           // 0x1007e1a0
        float blue_x  = FloatBits(0x3e19999a);           // 0x1007e1a7
        float blue_y  = FloatBits(0x3d75c28f);           // 0x1007e1ae
        reinterpret_cast<PngSetCHRM_t>(JgldBase() + 0x7db30)(png_ptr, info_ptr, white_x, white_y,
                                                             red_x, red_y, green_x, green_y,
                                                             blue_x, blue_y);
    }
}

// ----------------------------------------------------------------------------- zlib 1.0.2 (__cdecl)

// 0x000a0340  inflate_fast
// int __cdecl inflate_fast(unsigned bl, unsigned bd, const huft* tl, const huft* td,
// inflate_blocks_state* s, z_stream* z): the unchecked inner loop of inflate, run only when the window
// has at least 258 free bytes and the input at least ten.
// An inflate_huft is eight bytes: Exop at +0 and Bits at +1, both zero-extended byte loads
// (0x100a0417, 0x100a0427), and the union Base / Next at +4 (0x100a044a reads Base as a byte for the
// literal store, 0x100a04af reads it as the length base, 0x100a06d3 reads it as the sub-table pointer).
// inflate_blocks_state fields: bitk +0x1c (0x100a0375), bitb +0x20 (0x100a036c), window +0x24
// (0x100a05ca), end +0x28 (0x100a03a3), read +0x2c (0x100a038a) and write +0x30 (0x100a037e).
// z_stream fields: next_in +0 (0x100a035b), avail_in +4 (0x100a0363), total_in +8 (0x100a073c),
// msg +0x18 (0x100a06eb).
// On entry p, n, b and k are loaded from the stream and the state, q = s->write and
// m = q < s->read ? s->read - q - 1 : s->end - q (`cmp edx, dword ptr [ecx+0x2c]` / `jae 0x100a03a0`
// at 0x100a038a, an UNSIGNED pointer compare; the two subtractions at 0x100a0395 and 0x100a03a6).
// The two masks are inflate_mask[bl] and inflate_mask[bd], the module table at 0x101274a4 (0x100a03b5,
// 0x100a03c2).
// Each pass of the outer loop first fills the bit buffer to 20 bits (`cmp dword ptr [ebp-0x10], 0x14` /
// `jae 0x100a0403` at 0x100a03cc, eight bits per byte at 0x100a03fb) and looks the code up as
// tl[b & ml] (`lea edx, [ecx + eax*8]` at 0x100a040c).
//   exop 0 (`cmp dword ptr [ebp-8], 0` / `jne 0x100a0466` at 0x100a041c): the entry's base byte is
//   written to the window (0x100a044d), m drops by one and the pass ends.
//   otherwise the bits of the entry are dropped (0x100a046e-0x100a0485) and an inner loop runs:
//     exop & 16 (`and eax, 0x10` / `je 0x100a0774` at 0x100a048b-0x100a0490) is a length: the low four
//     bits are the number of extra bits, c = base + (b & inflate_mask[e]) (0x100a04a5-0x100a04b2),
//     those bits are dropped, the buffer is refilled to 15 bits (`cmp dword ptr [ebp-0x10], 0xf` /
//     `jae 0x100a0504` at 0x100a04cb) and the distance code td[b & md] is read. Its own inner loop
//     repeats the same three-way test: exop & 16 (0x100a0542) gives d = base + (b & mask[e]) after
//     refilling to e bits (0x100a0559), then m -= c and the copy runs - when q - s->window >= d
//     (`cmp eax, dword ptr [ebp-0x30]` / `jb 0x100a0627` at 0x100a05cd, UNSIGNED) the source is q - d
//     and two bytes are copied straight away, otherwise e = d - (q - window), r = s->end - e, and when
//     c > e (`cmp ecx, dword ptr [ebp-8]` / `jbe 0x100a0689` at 0x100a0647, UNSIGNED) c loses e while
//     e bytes are copied from the end of the window and r restarts at s->window (0x100a0683); the
//     remaining c bytes are then copied by the loop at 0x100a0689. exop & 64 == 0 (0x100a06bc) walks
//     into a sub-table; anything else writes "invalid distance code" (the string at 0x101209e4) into
//     z->msg at 0x100a06eb and leaves with Z_DATA_ERROR (0xfffffffd at 0x100a0758).
//     exop & 64 == 0 (`and ecx, 0x40` / `jne 0x100a07ee` at 0x100a0777) walks into a sub-table and, if
//     that entry's exop is 0 (0x100a07a1), writes its base byte as a literal and ends the pass.
//     exop & 32 (`and edx, 0x20` / `je 0x100a0868` at 0x100a07f1) is the end of the block: leave with
//     Z_STREAM_END (1 at 0x100a085e). Anything else writes "invalid literal/length code" (the string
//     at 0x101209fc) into z->msg at 0x100a086b and leaves with Z_DATA_ERROR.
// The outer loop repeats while m >= 0x102 (`jb 0x100a0902` at 0x100a08f6) and n >= 10 (`jae 0x100a03cc`
// at 0x100a08fc). Every exit first gives back the whole bytes still in the bit buffer - c = k >> 3,
// n += c, p -= c, k &= 7 (0x100a0902-0x100a0923) - and then writes back s->bitb, s->bitk, z->avail_in,
// z->total_in (which gains p - z->next_in), z->next_in and s->write (0x100a0926-0x100a0965).
struct Huft {
    unsigned char exop;
    unsigned char bits;
    unsigned char pad[2];
    union {
        unsigned base;
        const Huft* next;
    } more;
};
typedef int(__cdecl* InflateFast_t)(unsigned, unsigned, const Huft*, const Huft*, void*, void*);
InflateFast_t InflateFast_orig;
int __cdecl InflateFast_re(unsigned bl, unsigned bd, const Huft* tl, const Huft* td, void* s_, void* z_) {
    char* s = static_cast<char*>(s_);
    char* z = static_cast<char*>(z_);
    const unsigned* mask = InflateMask();

    unsigned char** z_next_in = reinterpret_cast<unsigned char**>(z + 0x00);
    unsigned* z_avail_in = reinterpret_cast<unsigned*>(z + 0x04);
    unsigned* z_total_in = reinterpret_cast<unsigned*>(z + 0x08);
    const char** z_msg = reinterpret_cast<const char**>(z + 0x18);
    unsigned* s_bitk = reinterpret_cast<unsigned*>(s + 0x1c);
    unsigned* s_bitb = reinterpret_cast<unsigned*>(s + 0x20);
    unsigned char** s_window = reinterpret_cast<unsigned char**>(s + 0x24);
    unsigned char** s_end = reinterpret_cast<unsigned char**>(s + 0x28);
    unsigned char** s_read = reinterpret_cast<unsigned char**>(s + 0x2c);
    unsigned char** s_write = reinterpret_cast<unsigned char**>(s + 0x30);

    const Huft* t;
    unsigned e, k, n, m, ml, md, c, d;
    unsigned b;
    unsigned char *p, *q, *r;

    p = *z_next_in;
    n = *z_avail_in;
    b = *s_bitb;
    k = *s_bitk;
    q = *s_write;
    m = static_cast<unsigned>(q < *s_read ? *s_read - q - 1 : *s_end - q);

    ml = mask[bl];
    md = mask[bd];

    do {
        while (k < 20) { n--; b |= static_cast<unsigned>(*p++) << k; k += 8; }
        t = tl + (b & ml);
        e = t->exop;
        if (e == 0) {
            b >>= t->bits; k -= t->bits;
            *q++ = static_cast<unsigned char>(t->more.base);
            m--;
            continue;
        }
        for (;;) {
            b >>= t->bits; k -= t->bits;
            if (e & 16) {
                e &= 15;
                c = t->more.base + (b & mask[e]);
                b >>= e; k -= e;
                while (k < 15) { n--; b |= static_cast<unsigned>(*p++) << k; k += 8; }
                t = td + (b & md);
                e = t->exop;
                for (;;) {
                    b >>= t->bits; k -= t->bits;
                    if (e & 16) {
                        e &= 15;
                        while (k < e) { n--; b |= static_cast<unsigned>(*p++) << k; k += 8; }
                        d = t->more.base + (b & mask[e]);
                        b >>= e; k -= e;
                        m -= c;
                        if (static_cast<unsigned>(q - *s_window) >= d) {
                            r = q - d;
                            *q++ = *r++; c--;
                            *q++ = *r++; c--;
                        } else {
                            e = d - static_cast<unsigned>(q - *s_window);
                            r = *s_end - e;
                            if (c > e) {
                                c -= e;
                                do { *q++ = *r++; } while (--e);
                                r = *s_window;
                            }
                        }
                        do { *q++ = *r++; } while (--c);
                        break;
                    } else if ((e & 64) == 0) {
                        t = t->more.next + (b & mask[e]);
                        e = t->exop;
                    } else {
                        *z_msg = JgldBase() + 0x1209e4;     // "invalid distance code"
                        c = k >> 3; n += c; p -= c; k &= 7;
                        *s_bitb = b; *s_bitk = k;
                        *z_avail_in = n; *z_total_in += static_cast<unsigned>(p - *z_next_in);
                        *z_next_in = p; *s_write = q;
                        return -3;
                    }
                }
                break;
            }
            if ((e & 64) == 0) {
                t = t->more.next + (b & mask[e]);
                e = t->exop;
                if (e == 0) {
                    b >>= t->bits; k -= t->bits;
                    *q++ = static_cast<unsigned char>(t->more.base);
                    m--;
                    break;
                }
            } else if (e & 32) {
                c = k >> 3; n += c; p -= c; k &= 7;
                *s_bitb = b; *s_bitk = k;
                *z_avail_in = n; *z_total_in += static_cast<unsigned>(p - *z_next_in);
                *z_next_in = p; *s_write = q;
                return 1;
            } else {
                *z_msg = JgldBase() + 0x1209fc;             // "invalid literal/length code"
                c = k >> 3; n += c; p -= c; k &= 7;
                *s_bitb = b; *s_bitk = k;
                *z_avail_in = n; *z_total_in += static_cast<unsigned>(p - *z_next_in);
                *z_next_in = p; *s_write = q;
                return -3;
            }
        }
    } while (m >= 258 && n >= 10);

    c = k >> 3; n += c; p -= c; k &= 7;
    *s_bitb = b; *s_bitk = k;
    *z_avail_in = n; *z_total_in += static_cast<unsigned>(p - *z_next_in);
    *z_next_in = p; *s_write = q;
    return 0;
}

// 0x0009f370  inflate_codes
// int __cdecl inflate_codes(inflate_blocks_state* s, z_stream* z, int r): the restartable decoder
// zlib falls back on whenever inflate_fast cannot run, written as a state machine so it can be left
// and re-entered at any bit.
// The codes state is the pointer at s+0xc (0x1009f391); its fields are mode +0 (the switch value read
// at 0x1009f401), len +4 (0x1009fb39), the union at +8 and +0xc - sub.code.tree +8 with
// sub.code.need +0xc (0x1009f537 / 0x1009f52b), sub.lit +8, sub.copy.get +8 with sub.copy.dist +0xc
// (0x1009fb01) - lbits +0x10 and dbits +0x11, both bytes (0x1009f49a, 0x1009f491), ltree +0x14
// (0x1009f534) and dtree +0x18. The blocks state and the stream are read exactly as in inflate_fast.
// The dispatch is `cmp dword ptr [ebp-0x34], 9` / `ja 0x100a0039` (0x1009f406) over the ten-entry
// jump table at 0x100a00b1: 0 START, 1 LEN, 2 LENEXT, 3 DIST, 4 DISTEXT, 5 COPY, 6 LIT, 7 WASH,
// 8 END, 9 BADCODE, and anything else returns Z_STREAM_ERROR.
//   START tries inflate_fast 0x100a0340 first when the window has at least 0x102 free bytes and the
//   input at least ten (0x1009f41a, 0x1009f427, `call` at 0x1009f49b); a non-zero result sends the
//   machine to WASH if it was 1 and to BADCODE otherwise (the branchless `neg`/`sbb`/`and 2`/`add 7`
//   at 0x1009f509-0x1009f513). Otherwise it arms LEN with lbits bits of the literal/length tree.
//   LEN reads need bits, indexes the tree with inflate_mask[need] (0x1009f5fe) and drops the entry's
//   Bits: Exop 0 stores the literal and goes to LIT (0x1009f641), Exop & 16 stores the extra-bit
//   count and the length base and goes to LENEXT (0x1009f665), Exop & 64 == 0 re-arms LEN one level
//   down (0x1009f695), Exop & 32 goes to WASH (0x1009f6b9) and anything else writes "invalid
//   literal/length code" 0x101209fc into z->msg (0x1009f6d5) and leaves with Z_DATA_ERROR.
//   LENEXT adds the extra bits to the length and arms DIST with dbits of the distance tree; DIST has
//   the same three-way test minus the end-of-block case, its error string being "invalid distance
//   code" 0x101209e4 (0x1009f99f); DISTEXT adds the extra bits to the distance.
//   COPY computes the source pointer once - q - dist, or s->end - (dist - (q - window)) when the
//   write pointer is less than dist bytes into the window (0x1009fb04) - and then copies len bytes,
//   wrapping the source at s->end (0x1009fce4) and asking for output space before each byte.
//   LIT writes the one stored literal. Both go back to START.
//   WASH flushes the window (`call 0x100a0130` at 0x1009fec4) and, if the read pointer has caught up
//   with the write pointer (0x1009ff12), falls into END, which returns Z_STREAM_END; otherwise it
//   leaves. BADCODE returns Z_DATA_ERROR (0x1009ffda) and the default Z_STREAM_ERROR.
// Asking for output space is the NEEDOUT macro: when m is 0 the write pointer is wrapped to the
// start of the window if it is at the end and the read pointer is not (0x1009fb56, 0x1009fb64), then
// the window is flushed and wrapped again, and only if there is still no room does the function
// leave. Every exit, including every one inside NEEDBITS when the input runs out, writes s->bitb,
// s->bitk, z->avail_in, z->total_in, z->next_in and s->write back and returns
// inflate_flush(s, z, r).
struct CodesState {
    unsigned mode;
    unsigned len;
    unsigned sub_a;        // +8:  sub.code.tree, sub.lit or sub.copy.get
    unsigned sub_b;        // +0xc: sub.code.need or sub.copy.dist
    unsigned char lbits;
    unsigned char dbits;
    unsigned char pad[2];
    const Huft* ltree;
    const Huft* dtree;
};
typedef int(__cdecl* InflateFlush_t)(void*, void*, int);
typedef int(__cdecl* InflateCodes_t)(void*, void*, int);
InflateCodes_t InflateCodes_orig;

int __cdecl InflateCodes_re(void* s_, void* z_, int r) {
    char* s = static_cast<char*>(s_);
    char* z = static_cast<char*>(z_);
    const unsigned* mask = InflateMask();
    InflateFast_t inflate_fast = reinterpret_cast<InflateFast_t>(JgldBase() + 0xa0340);
    InflateFlush_t inflate_flush = reinterpret_cast<InflateFlush_t>(JgldBase() + 0xa0130);

    unsigned char** z_next_in = reinterpret_cast<unsigned char**>(z + 0x00);
    unsigned* z_avail_in = reinterpret_cast<unsigned*>(z + 0x04);
    unsigned* z_total_in = reinterpret_cast<unsigned*>(z + 0x08);
    const char** z_msg = reinterpret_cast<const char**>(z + 0x18);
    CodesState* c = *reinterpret_cast<CodesState**>(s + 0x0c);
    unsigned* s_bitk = reinterpret_cast<unsigned*>(s + 0x1c);
    unsigned* s_bitb = reinterpret_cast<unsigned*>(s + 0x20);
    unsigned char** s_window = reinterpret_cast<unsigned char**>(s + 0x24);
    unsigned char** s_end = reinterpret_cast<unsigned char**>(s + 0x28);
    unsigned char** s_read = reinterpret_cast<unsigned char**>(s + 0x2c);
    unsigned char** s_write = reinterpret_cast<unsigned char**>(s + 0x30);

    unsigned j, e;
    const Huft* t;
    unsigned char* f;
    unsigned char *p, *q;
    unsigned n, k, m;
    unsigned b;

#define SG_WAVAIL   (static_cast<unsigned>(q < *s_read ? *s_read - q - 1 : *s_end - q))
#define SG_LOADOUT  do { q = *s_write; m = SG_WAVAIL; } while (0)
#define SG_LOAD     do { p = *z_next_in; n = *z_avail_in; b = *s_bitb; k = *s_bitk; SG_LOADOUT; } while (0)
#define SG_UPDATE   do { *s_bitb = b; *s_bitk = k; *z_avail_in = n; \
                         *z_total_in += static_cast<unsigned>(p - *z_next_in); \
                         *z_next_in = p; *s_write = q; } while (0)
#define SG_LEAVE    do { SG_UPDATE; return inflate_flush(s_, z_, r); } while (0)
#define SG_NEEDBITS(J) do { while (k < (J)) { if (n) r = 0; else SG_LEAVE; \
                            n--; b |= static_cast<unsigned>(*p++) << k; k += 8; } } while (0)
#define SG_DUMPBITS(J) do { b >>= (J); k -= (J); } while (0)
#define SG_WRAP     do { if (q == *s_end && *s_read != *s_window) { q = *s_window; m = SG_WAVAIL; } } while (0)
#define SG_FLUSH    do { *s_write = q; r = inflate_flush(s_, z_, r); SG_LOADOUT; } while (0)
#define SG_NEEDOUT  do { if (m == 0) { SG_WRAP; if (m == 0) { SG_FLUSH; SG_WRAP; if (m == 0) SG_LEAVE; } } \
                         r = 0; } while (0)
#define SG_OUTBYTE(A) do { *q++ = static_cast<unsigned char>(A); m--; } while (0)

    SG_LOAD;

    for (;;) {
        switch (c->mode) {
            case 0:  // START
                if (m >= 258 && n >= 10) {
                    SG_UPDATE;
                    r = inflate_fast(c->lbits, c->dbits, c->ltree, c->dtree, s_, z_);
                    SG_LOAD;
                    if (r != 0) {
                        c->mode = (r == 1) ? 7u : 9u;
                        break;
                    }
                }
                c->sub_b = c->lbits;
                c->sub_a = reinterpret_cast<unsigned>(c->ltree);
                c->mode = 1;
                // falls into LEN, exactly as the original does
            case 1:  // LEN
                j = c->sub_b;
                SG_NEEDBITS(j);
                t = reinterpret_cast<const Huft*>(c->sub_a) + (b & mask[j]);
                SG_DUMPBITS(t->bits);
                e = t->exop;
                if (e == 0) {
                    c->sub_a = t->more.base;
                    c->mode = 6;
                    break;
                }
                if (e & 16) {
                    c->sub_a = e & 15;
                    c->len = t->more.base;
                    c->mode = 2;
                    break;
                }
                if ((e & 64) == 0) {
                    c->sub_b = e;
                    c->sub_a = reinterpret_cast<unsigned>(t->more.next);
                    break;
                }
                if (e & 32) {
                    c->mode = 7;
                    break;
                }
                c->mode = 9;
                *z_msg = JgldBase() + 0x1209fc;         // "invalid literal/length code"
                r = -3;
                SG_LEAVE;
            case 2:  // LENEXT
                j = c->sub_a;
                SG_NEEDBITS(j);
                c->len += b & mask[j];
                SG_DUMPBITS(j);
                c->sub_b = c->dbits;
                c->sub_a = reinterpret_cast<unsigned>(c->dtree);
                c->mode = 3;
                // falls into DIST
            case 3:  // DIST
                j = c->sub_b;
                SG_NEEDBITS(j);
                t = reinterpret_cast<const Huft*>(c->sub_a) + (b & mask[j]);
                SG_DUMPBITS(t->bits);
                e = t->exop;
                if (e & 16) {
                    c->sub_a = e & 15;
                    c->sub_b = t->more.base;
                    c->mode = 4;
                    break;
                }
                if ((e & 64) == 0) {
                    c->sub_b = e;
                    c->sub_a = reinterpret_cast<unsigned>(t->more.next);
                    break;
                }
                c->mode = 9;
                *z_msg = JgldBase() + 0x1209e4;         // "invalid distance code"
                r = -3;
                SG_LEAVE;
            case 4:  // DISTEXT
                j = c->sub_a;
                SG_NEEDBITS(j);
                c->sub_b += b & mask[j];
                SG_DUMPBITS(j);
                c->mode = 5;
                // falls into COPY
            case 5:  // COPY
                f = (static_cast<unsigned>(q - *s_window) < c->sub_b)
                        ? *s_end - (c->sub_b - static_cast<unsigned>(q - *s_window))
                        : q - c->sub_b;
                while (c->len) {
                    SG_NEEDOUT;
                    SG_OUTBYTE(*f++);
                    if (f == *s_end) f = *s_window;
                    c->len--;
                }
                c->mode = 0;
                break;
            case 6:  // LIT
                SG_NEEDOUT;
                SG_OUTBYTE(c->sub_a);
                c->mode = 0;
                break;
            case 7:  // WASH
                SG_FLUSH;
                if (*s_read != *s_write) SG_LEAVE;
                c->mode = 8;
                // falls into END
            case 8:  // END
                r = 1;
                SG_LEAVE;
            case 9:  // BADCODE
                r = -3;
                SG_LEAVE;
            default:
                r = -2;
                SG_LEAVE;
        }
    }

#undef SG_OUTBYTE
#undef SG_NEEDOUT
#undef SG_FLUSH
#undef SG_WRAP
#undef SG_DUMPBITS
#undef SG_NEEDBITS
#undef SG_LEAVE
#undef SG_UPDATE
#undef SG_LOAD
#undef SG_LOADOUT
#undef SG_WAVAIL
}

}  // namespace

SG_HOOK("jgld.dll", 0x0006dac0, png_do_strip_filler, PngDoStripFiller_re, PngDoStripFiller_orig);
SG_HOOK("jgld.dll", 0x00072bd0, png_do_read_filler, PngDoReadFiller_re, PngDoReadFiller_orig);
SG_HOOK("jgld.dll", 0x00076760, png_do_gamma, PngDoGamma_re, PngDoGamma_orig);
SG_HOOK("jgld.dll", 0x00077300, png_do_expand, PngDoExpand_re, PngDoExpand_orig);
SG_HOOK("jgld.dll", 0x000736c0, png_do_rgb_to_gray, PngDoRgbToGray_re, PngDoRgbToGray_orig);
SG_HOOK("jgld.dll", 0x0007c810, png_do_read_interlace, PngDoReadInterlace_re, PngDoReadInterlace_orig);
SG_HOOK("jgld.dll", 0x0007c310, png_combine_row, PngCombineRow_re, PngCombineRow_orig);
SG_HOOK("jgld.dll", 0x0007e120, png_set_sRGB_gAMA_and_cHRM, PngSetSRGBgAMAcHRM_re, PngSetSRGBgAMAcHRM_orig);
SG_HOOK("jgld.dll", 0x000a0340, inflate_fast, InflateFast_re, InflateFast_orig);
SG_HOOK("jgld.dll", 0x0009f370, inflate_codes, InflateCodes_re, InflateCodes_orig);

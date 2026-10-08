// Batch c3bq: jgld.dll non-render.
//   png_read_transform_info 0x000714c0  (the lead of log/c3/c3bq_candidates.tsv, named in c3bl's notes as
//                                        "the write-side twin of png_do_read_transformations")
//   png_set_IHDR            0x0007dc70
//   png_get_IHDR            0x0006eae0
//   png_set_read_fn         0x00078cb0
//   Palette::reset          0x0006a380
//   Palette::apply          0x0006a4a0
//   Matrix::loadIdentity    0x000045f0
//
// The other two leads of the candidate list, png_set_background 0x0006f230 and png_set_rgb_to_gray
// 0x000700f0, are not here: nothing in any of the four modules calls them or stores their address
// (log/c3/c3bq_notes.txt), so the C3 gate's anti-island rule refuses them.
//
// SG_HOOK addresses are RVAs; every instruction cited in a comment is a VA (0x10000000 + RVA), the form
// the debug build's disassembly prints. jgld.dll is LoadLibrary'd after the shim starts, so these hooks
// install from the LoadLibraryA hook (SgHooksInstallPending, re/hooks.cpp) and diff_hook finds them by
// (module, rva) through SimGolfShim_FindHookM. __thiscall is emulated with __fastcall: ecx = this, edx
// unused.
//
// Nothing live in the running jgld.dll is passed to these bodies: the png_struct, the png_info, the
// Palette objects and the Matrix objects all come from the fixtures (re/frida/js/fixtures.d/c3bq.js).
// The three libpng entry points here reach png_warning 0x10079040, which calls the handler the
// png_struct carries at +0x44 (c3bb and c3bl use the same trick), so the fixture puts a one-byte `ret`
// of its own there and no warning reaches a C stream.
#include "hooks.h"

namespace {

// jgld.dll is relocatable, so a module address the code stores is resolved against the running base.
char* JgldBase() {
    static char* base = 0;
    if (!base) base = reinterpret_cast<char*>(GetModuleHandleA("jgld.dll"));
    return base;
}

inline unsigned char* B(void* p, unsigned off) { return static_cast<unsigned char*>(p) + off; }
inline unsigned char U8(const void* p, unsigned off) {
    return *(reinterpret_cast<const unsigned char*>(p) + off);
}
inline unsigned short U16(const void* p, unsigned off) {
    return *reinterpret_cast<const unsigned short*>(static_cast<const char*>(p) + off);
}
inline unsigned& U32(void* p, unsigned off) {
    return *reinterpret_cast<unsigned*>(static_cast<char*>(p) + off);
}
inline unsigned U32R(const void* p, unsigned off) {
    return *reinterpret_cast<const unsigned*>(static_cast<const char*>(p) + off);
}

// ---------------------------------------------------------------- png_read_transform_info 0x000714c0
//
// __cdecl, two stack arguments (`ret` at 0x100717c6 pops nothing): png_ptr at [ebp+8], info_ptr at
// [ebp+0xc]. No callee at all, so this is a leaf.
//
// png_struct fields it reads: transformations at +0x70 (0x100714db and six more), num_trans as a word at
// +0x11a (0x100714fa, 0x1007152c), background at +0x138 (0x1007158e), gamma at +0x15c (0x100715c1),
// palette_lookup at +0x1ec (0x10071618), user_transform_depth at +0x64 (0x10071758) and
// user_transform_channels at +0x65 (0x10071778). It writes nothing to the png_struct.
//
// png_info fields: width at +0 (0x100717af), rowbytes at +0xc (0x100717bd), num_trans as a word at +0x16
// (0x1007151f, 0x1007155d, 0x10071585), bit_depth at +0x18, color_type at +0x19, channels at +0x1d,
// pixel_depth at +0x1e, gamma at +0x28 (0x100715c7) and background at +0x5a (0x10071597). Every byte
// field is read zero-extended (`xor reg,reg` + `mov rl,[..]`), so the signed `jge` tests at 0x10071551,
// 0x1007164d, 0x1007175d and 0x1007177d compare values that are already 0..255.
typedef void(__cdecl* ReadTransformInfo_t)(void*, void*);
ReadTransformInfo_t ReadTransformInfo_orig;

void __cdecl ReadTransformInfo_re(void* png_ptr, void* info_ptr) {
    const unsigned T = U32R(png_ptr, 0x70);                     // 0x100714db

    if (T & 0x1000) {                                           // PNG_EXPAND, 0x100714de / 0x100714e6
        if (U8(info_ptr, 0x19) == 3) {                          // 0x100714ed, cmp 3 / jne 0x100714f3
            if (U16(png_ptr, 0x11a) != 0)                       // 0x100714fa / 0x10071503
                *B(info_ptr, 0x19) = 6;                         // 0x10071508
            else
                *B(info_ptr, 0x19) = 2;                         // 0x10071511
            *B(info_ptr, 0x18) = 8;                             // 0x10071518
            *reinterpret_cast<unsigned short*>(B(info_ptr, 0x16)) = 0;   // 0x1007151f
        } else {
            if (U16(png_ptr, 0x11a) != 0)                       // 0x1007152c / 0x10071535
                *B(info_ptr, 0x19) = (unsigned char)(U8(info_ptr, 0x19) | 4);  // `or cl,4` 0x1007153d
            if ((int)U8(info_ptr, 0x18) < 8)                    // 0x1007154b, cmp 8 / jge 0x10071551
                *B(info_ptr, 0x18) = 8;                         // 0x10071556
            *reinterpret_cast<unsigned short*>(B(info_ptr, 0x16)) = 0;   // 0x1007155d
        }
    }

    if (T & 0x80) {                                             // PNG_BACKGROUND, 0x10071569 / 0x10071571
        *B(info_ptr, 0x19) = (unsigned char)(U8(info_ptr, 0x19) & 0xfb);  // `and cl,0xfb` 0x10071579
        *reinterpret_cast<unsigned short*>(B(info_ptr, 0x16)) = 0;        // 0x10071585
        // The png_color_16 copy from png_ptr+0x138 to info_ptr+0x5a is two dwords and a word, ten bytes:
        // 0x1007159a/0x1007159c, 0x1007159e/0x100715a1, 0x100715a4/0x100715a8. The two bytes above the
        // tenth are left alone.
        U32(info_ptr, 0x5a) = U32R(png_ptr, 0x138);
        U32(info_ptr, 0x5e) = U32R(png_ptr, 0x13c);
        *reinterpret_cast<unsigned short*>(B(info_ptr, 0x62)) = U16(png_ptr, 0x140);
    }

    if (T & 0x2000)                                             // PNG_GAMMA, 0x100715b2 / 0x100715b9
        U32(info_ptr, 0x28) = U32R(png_ptr, 0x15c);             // the float is moved as a dword, 0x100715c7

    if ((T & 0x400) && U8(info_ptr, 0x18) == 16)                // PNG_16_TO_8, 0x100715d0 / 0x100715e5
        *B(info_ptr, 0x18) = 8;                                 // 0x100715ea

    if (T & 0x40) {                                             // PNG_DITHER, 0x100715f4 / 0x100715f9
        // The colour-type test is two compares against 2 (0x10071603, je 0x10071606) and 6 (0x10071610,
        // jne 0x10071613); the palette_lookup test is 0x10071618 and the depth test 0x10071629.
        if (U8(info_ptr, 0x19) == 2 || U8(info_ptr, 0x19) == 6) {
            if (U32R(png_ptr, 0x1ec) != 0) {
                if (U8(info_ptr, 0x18) == 8)
                    *B(info_ptr, 0x19) = 3;                     // 0x10071631
            }
        }
    }

    if ((T & 4) && (int)U8(info_ptr, 0x18) < 8)                 // PNG_PACK, 0x1007163b / jge 0x1007164d
        *B(info_ptr, 0x18) = 8;                                 // 0x10071652

    if (T & 0x4000)                                             // PNG_GRAY_TO_RGB, 0x1007165c / 0x10071664
        *B(info_ptr, 0x19) = (unsigned char)(U8(info_ptr, 0x19) | 2);     // `or al,2` 0x1007166c

    // PNG_RGB_TO_GRAY is tested as a MASK of two bits here (`and eax, 0x600000` at 0x1007167a), unlike
    // png_do_read_transformations 0x100717d0, which compares the whole word with one constant.
    if (T & 0x600000)
        *B(info_ptr, 0x19) = (unsigned char)(U8(info_ptr, 0x19) & 0xfd);  // `and dl,0xfd` 0x10071689

    if (U8(info_ptr, 0x19) == 3)                                // 0x1007169a / jne 0x1007169d
        *B(info_ptr, 0x1d) = 1;                                 // 0x100716a2
    else if (U8(info_ptr, 0x19) & 2)                            // 0x100716b0 / je 0x100716b5
        *B(info_ptr, 0x1d) = 3;                                 // 0x100716ba
    else
        *B(info_ptr, 0x1d) = 1;                                 // 0x100716c3

    if (T & 0x40000)                                            // PNG_STRIP_ALPHA, 0x100716cd / 0x100716d4
        *B(info_ptr, 0x19) = (unsigned char)(U8(info_ptr, 0x19) & 0xfb);  // `and dl,0xfb` 0x100716dc

    if (U8(info_ptr, 0x19) & 4)                                 // 0x100716ed / je 0x100716f2
        *B(info_ptr, 0x1d) = (unsigned char)(U8(info_ptr, 0x1d) + 1);     // `add cl,1` 0x100716fa

    if (T & 0x8000) {                                           // PNG_FILLER, 0x10071709 / 0x10071711
        // colour type 2 (je 0x1007171e) or colour type 0 (jne 0x1007172a): both are tested on the whole
        // byte, so 3 and 4 do not qualify.
        if (U8(info_ptr, 0x19) == 2 || U8(info_ptr, 0x19) == 0)
            *B(info_ptr, 0x1d) = (unsigned char)(U8(info_ptr, 0x1d) + 1);  // `add cl,1` 0x10071732
    }

    if (T & 0x100000) {                                         // PNG_USER_TRANSFORM, 0x10071741
        if ((int)U8(info_ptr, 0x18) < (int)U8(png_ptr, 0x64))   // 0x1007175b / jge 0x1007175d
            *B(info_ptr, 0x18) = U8(png_ptr, 0x64);             // 0x10071768
        if ((int)U8(info_ptr, 0x1d) < (int)U8(png_ptr, 0x65))   // 0x1007177b / jge 0x1007177d
            *B(info_ptr, 0x1d) = U8(png_ptr, 0x65);             // 0x10071788
    }

    // pixel_depth = channels * bit_depth, truncated to a byte by the store (`imul edx,ecx` 0x1007179b,
    // `mov [eax+0x1e], dl` 0x100717a1); rowbytes = (width * pixel_depth + 7) >> 3 with an UNSIGNED shift
    // (`imul ecx,edx` 0x100717b1, `add ecx,7` 0x100717b4, `shr ecx,3` 0x100717b7).
    *B(info_ptr, 0x1e) = (unsigned char)(U8(info_ptr, 0x1d) * U8(info_ptr, 0x18));
    U32(info_ptr, 0x0c) = (U32R(info_ptr, 0) * U8(info_ptr, 0x1e) + 7) >> 3;
}

// ---------------------------------------------------------------------------- png_set_IHDR 0x0007dc70
//
// __cdecl, nine stack arguments (`ret` at 0x1007ddab pops nothing): png_ptr [ebp+8], info_ptr [ebp+0xc],
// width [ebp+0x10], height [ebp+0x14], bit_depth [ebp+0x18], color_type [ebp+0x1c], interlace_type
// [ebp+0x20], compression_type [ebp+0x24], filter_type [ebp+0x28]. The three trailing types are stored
// out of argument order: compression at info+0x1a (0x1007dcc2), filter at info+0x1b (0x1007dccb) and
// interlace at info+0x1c (0x1007dcd4). Only callee: png_warning 0x10079040.
typedef void(__cdecl* SetIhdr_t)(void*, void*, unsigned, unsigned, int, int, int, int, int);
SetIhdr_t SetIhdr_orig;

typedef void(__cdecl* PngWarning_t)(void*, const char*);

void __cdecl SetIhdr_re(void* png_ptr, void* info_ptr, unsigned width, unsigned height, int bit_depth,
                        int color_type, int interlace_type, int compression_type, int filter_type) {
    if (png_ptr == 0 || info_ptr == 0) return;                  // 0x1007dc8c, 0x1007dc92

    U32(info_ptr, 0) = width;                                   // 0x1007dc9f
    U32(info_ptr, 4) = height;                                  // 0x1007dca7
    *B(info_ptr, 0x18) = (unsigned char)bit_depth;              // 0x1007dcb0
    *B(info_ptr, 0x19) = (unsigned char)color_type;             // 0x1007dcb9
    *B(info_ptr, 0x1a) = (unsigned char)compression_type;       // 0x1007dcc2
    *B(info_ptr, 0x1b) = (unsigned char)filter_type;            // 0x1007dccb
    *B(info_ptr, 0x1c) = (unsigned char)interlace_type;         // 0x1007dcd4

    // The channel count is recomputed from the byte just stored, not from the argument.
    if (U8(info_ptr, 0x19) == 3)                                // 0x1007dcdf / jne 0x1007dce2
        *B(info_ptr, 0x1d) = 1;                                 // 0x1007dce7
    else if (U8(info_ptr, 0x19) & 2)                            // 0x1007dcf5 / je 0x1007dcfa
        *B(info_ptr, 0x1d) = 3;                                 // 0x1007dcff
    else
        *B(info_ptr, 0x1d) = 1;                                 // 0x1007dd08
    if (U8(info_ptr, 0x19) & 4)                                 // 0x1007dd14 / je 0x1007dd19
        *B(info_ptr, 0x1d) = (unsigned char)(U8(info_ptr, 0x1d) + 1);     // `add al,1` 0x1007dd21

    *B(info_ptr, 0x1e) = (unsigned char)(U8(info_ptr, 0x1d) * U8(info_ptr, 0x18));  // 0x1007dd39 / 0x1007dd3f

    // ((pixel_depth + 7) >> 3) is formed with a SIGNED shift (0x1007dd4a, 0x1007dd4d) and then used as
    // the divisor of the unsigned 0x7fffffff / n at 0x1007dd5a; the width ARGUMENT, not info->width, is
    // the left side of the unsigned compare at 0x1007dd5d (`jbe` at 0x1007dd60). A pixel_depth of 0
    // makes that divisor 0 and the original faults on the `div`, so no vector passes bit_depth 0.
    unsigned bytes_per_pixel = (unsigned)(((int)U8(info_ptr, 0x1e) + 7) >> 3);
    unsigned limit = 0x7fffffffu / bytes_per_pixel;
    if (width > limit) {
        static PngWarning_t warn = 0;
        if (!warn) warn = reinterpret_cast<PngWarning_t>(JgldBase() + 0x79040);
        warn(png_ptr, reinterpret_cast<const char*>(JgldBase() + 0x11eb28));   // push 0x1011eb28, call 0x1007dd6b
        U32(info_ptr, 0x0c) = 0;                                // 0x1007dd76
        return;
    }
    // rowbytes = (width * pixel_depth + 7) >> 3, unsigned shift (0x1007dd8c, 0x1007dd8f, 0x1007dd92).
    U32(info_ptr, 0x0c) = (U32R(info_ptr, 0) * U8(info_ptr, 0x1e) + 7) >> 3;
}

// ---------------------------------------------------------------------------- png_get_IHDR 0x0006eae0
//
// __cdecl, nine stack arguments (`ret` at 0x1006ec31): png_ptr [ebp+8], info_ptr [ebp+0xc], and the
// seven out pointers width [ebp+0x10], height [ebp+0x14], bit_depth [ebp+0x18], color_type [ebp+0x1c],
// interlace_type [ebp+0x20], compression_type [ebp+0x24], filter_type [ebp+0x28]. Returns 1 when it
// wrote (0x1006ec18) and 0 when a required pointer was null (0x1006ec1f). Only callee: png_warning.
//
// The six null tests at 0x1006eafc, 0x1006eb06, 0x1006eb10, 0x1006eb1a, 0x1006eb24 and 0x1006eb2e cover
// png_ptr, info_ptr, width, height, bit_depth and color_type; interlace, compression and filter are
// optional and tested one by one (0x1006eb67, 0x1006eb7a, 0x1006eb8d). The three byte fields are widened
// to a full dword in the caller's buffer (`xor ecx,ecx` + `mov cl,[..]`, stored at 0x1006eb54 and
// 0x1006eb61). The channel count is then read back OUT OF THE CALLER'S BUFFERS (*color_type at
// 0x1006eb9f, 0x1006ebb0, 0x1006ebcc, *bit_depth at 0x1006ebe1, *width at 0x1006ec03), so a caller that
// aliases two of them changes the arithmetic.
typedef int(__cdecl* GetIhdr_t)(void*, void*, unsigned*, unsigned*, int*, int*, int*, int*, int*);
GetIhdr_t GetIhdr_orig;

int __cdecl GetIhdr_re(void* png_ptr, void* info_ptr, unsigned* width, unsigned* height, int* bit_depth,
                       int* color_type, int* interlace_type, int* compression_type, int* filter_type) {
    if (png_ptr == 0 || info_ptr == 0 || width == 0 || height == 0 || bit_depth == 0 || color_type == 0)
        return 0;                                               // 0x1006ec1f

    *width = U32R(info_ptr, 0);                                 // 0x1006eb3c
    *height = U32R(info_ptr, 4);                                // 0x1006eb47
    *bit_depth = (int)U8(info_ptr, 0x18);                       // 0x1006eb54
    *color_type = (int)U8(info_ptr, 0x19);                      // 0x1006eb61
    if (compression_type) *compression_type = (int)U8(info_ptr, 0x1a);   // 0x1006eb74
    if (filter_type) *filter_type = (int)U8(info_ptr, 0x1b);             // 0x1006eb87
    if (interlace_type) *interlace_type = (int)U8(info_ptr, 0x1c);       // 0x1006eb9a

    int channels;
    if (*color_type == 3)                                       // 0x1006eb9f / jne 0x1006eba2
        channels = 1;                                           // 0x1006eba4
    else if (*color_type & 2)                                   // 0x1006ebb2 / je 0x1006ebb7
        channels = 3;                                           // 0x1006ebb9
    else
        channels = 1;                                           // 0x1006ebc2
    if (*color_type & 4)                                        // 0x1006ebce / je 0x1006ebd3
        channels += 1;                                          // 0x1006ebd8

    int pixel_depth = *bit_depth * channels;                    // 0x1006ebe3
    unsigned bytes_per_pixel = (unsigned)((pixel_depth + 7) >> 3);  // sar, 0x1006ebed / 0x1006ebf0
    unsigned limit = 0x7fffffffu / bytes_per_pixel;             // div 0x1006ebfd
    if (*width > limit) {                                       // unsigned compare, jbe 0x1006ec05
        static PngWarning_t warn = 0;
        if (!warn) warn = reinterpret_cast<PngWarning_t>(JgldBase() + 0x79040);
        warn(png_ptr, reinterpret_cast<const char*>(JgldBase() + 0x11dcf4));   // push 0x1011dcf4, call 0x1006ec10
    }
    return 1;                                                   // 0x1006ec18
}

// ------------------------------------------------------------------------- png_set_read_fn 0x00078cb0
//
// __cdecl, three stack arguments (`ret` at 0x10078d3e): png_ptr [ebp+8], io_ptr [ebp+0xc],
// read_data_fn [ebp+0x10]. png_ptr is NOT tested for null here, unlike png_set_pHYs 0x1007dfc0.
typedef void(__cdecl* SetReadFn_t)(void*, void*, void*);
SetReadFn_t SetReadFn_orig;

void __cdecl SetReadFn_re(void* png_ptr, void* io_ptr, void* read_data_fn) {
    U32(png_ptr, 0x54) = (unsigned)(unsigned long)io_ptr;       // io_ptr, 0x10078cce
    if (read_data_fn != 0)                                      // 0x10078cd1 / je 0x10078cd5
        U32(png_ptr, 0x50) = (unsigned)(unsigned long)read_data_fn;        // 0x10078cdd
    else
        // the module's own default reader, the body at RVA 0x78d40 (`mov dword [ecx+0x50], 0x10078d40`
        // at 0x10078ce5); the DLL is relocatable, so the constant is resolved against the running base.
        U32(png_ptr, 0x50) = (unsigned)(unsigned long)(JgldBase() + 0x78d40);

    if (U32R(png_ptr, 0x4c) != 0) {                             // write_data_fn, 0x10078cef / je 0x10078cf3
        U32(png_ptr, 0x4c) = 0;                                 // 0x10078cf8
        static PngWarning_t warn = 0;
        if (!warn) warn = reinterpret_cast<PngWarning_t>(JgldBase() + 0x79040);
        warn(png_ptr, reinterpret_cast<const char*>(JgldBase() + 0x11e01c));   // call 0x10078d08
        warn(png_ptr, reinterpret_cast<const char*>(JgldBase() + 0x11dfe8));   // call 0x10078d19
    }
    U32(png_ptr, 0x14c) = 0;                                    // 0x10078d24
}

// ------------------------------------------------------------------------- Palette::reset 0x0006a380
//
// __thiscall, no stack argument (`ret` at 0x1006a3bd), this spilled to [ebp-4] at 0x1006a39a. Two dword
// stores of 0 at this+0x810 (0x1006a3a0) and this+0x814 (0x1006a3ad); nothing else is read or written,
// and there is no branch.
typedef void(__fastcall* PaletteReset_t)(void*, void*);
PaletteReset_t PaletteReset_orig;

void __fastcall PaletteReset_re(void* self, void*) {
    U32(self, 0x810) = 0;
    U32(self, 0x814) = 0;
}

// ------------------------------------------------------------------------- Palette::apply 0x0006a4a0
//
// __thiscall, no stack argument (`ret` at 0x1006a5ae). The object holds 256 four-byte entries at +0xc
// (red +0, green +1, blue +2, flags +3 relative to each entry: the loop reads [this + i*4 + 0xc],
// [.. + 0xd] and [.. + 0xe] at 0x1006a538, 0x1006a521 and 0x1006a50f), a 256-entry 16-bit table at
// +0x40c (0x1006a54d) and a second one at +0x60c (0x1006a59b), a serial at +0x810 and the serial of the
// last build at +0x814.
//
// It returns at once when the two serials are equal (`cmp` at 0x1006a4c9, `jne` at 0x1006a4cf jumping
// over the `jmp` to the epilogue at 0x1006a4d1); otherwise it copies +0x810 into +0x814 (0x1006a4e2)
// and fills both tables for i = 0..255 (`cmp [ebp-8], 0x100` / `jge` at 0x1006a4fa-0x1006a501).
//
// First table, the 5/5/5 form: (blue >> 3) & 0x1f (0x1006a513, 0x1006a516) | (green << 2) & 0x3e0
// (0x1006a525, 0x1006a528) | (red << 7) & 0xfc00 (0x1006a53c, 0x1006a53f). The mask 0xfc00 keeps a bit
// the shift can never set: red is a zero-extended byte, so red << 7 <= 0x7f80.
// Second table, the 5/6/5 form: (blue >> 3) & 0x1f (0x1006a561, 0x1006a564) | (green << 3) & 0x7e0
// (0x1006a573, 0x1006a576) | (red << 8) & 0xf800 (0x1006a58a, 0x1006a58d).
// Both shifts of the entry bytes are arithmetic (`sar`) on values already zero-extended, so they are
// plain logical shifts of 0..255.
typedef void(__fastcall* PaletteApply_t)(void*, void*);
PaletteApply_t PaletteApply_orig;

void __fastcall PaletteApply_re(void* self, void*) {
    if (U32R(self, 0x814) == U32R(self, 0x810)) return;         // 0x1006a4c9 / 0x1006a4cf
    U32(self, 0x814) = U32R(self, 0x810);                       // 0x1006a4e2

    for (int i = 0; i < 0x100; i++) {                           // 0x1006a4fa / 0x1006a501
        const unsigned red = U8(self, 0x0c + i * 4);
        const unsigned green = U8(self, 0x0d + i * 4);
        const unsigned blue = U8(self, 0x0e + i * 4);

        unsigned v555 = (blue >> 3) & 0x1f;
        v555 |= (green << 2) & 0x3e0;
        v555 |= (red << 7) & 0xfc00;
        *reinterpret_cast<unsigned short*>(B(self, 0x40c + i * 2)) = (unsigned short)v555;

        unsigned v565 = (blue >> 3) & 0x1f;
        v565 |= (green << 3) & 0x7e0;
        v565 |= (red << 8) & 0xf800;
        *reinterpret_cast<unsigned short*>(B(self, 0x60c + i * 2)) = (unsigned short)v565;
    }
}

// --------------------------------------------------------------------- Matrix::loadIdentity 0x000045f0
//
// __thiscall, no stack argument (`ret` at 0x10004663). The sixteen cells start at this+4, NOT at this+0
// (the loop stores to [this + i*4 + 4] at 0x1000462b for i = 0..15, `cmp [ebp-8], 0x10` / `jge` at
// 0x1000461f-0x10004623), which leaves the dword at this+0 - the vtable pointer of this class -
// untouched. The four diagonal cells are then set to the dword 0x3f800000, 1.0f written as an integer
// immediate: +4 (0x10004638), +0x18 (0x10004642), +0x2c (0x1000464c) and +0x40 (0x10004656). No read of
// the object, no branch beyond the loop.
typedef void(__fastcall* MatrixLoadIdentity_t)(void*, void*);
MatrixLoadIdentity_t MatrixLoadIdentity_orig;

void __fastcall MatrixLoadIdentity_re(void* self, void*) {
    for (int i = 0; i < 16; i++)
        U32(self, 4 + i * 4) = 0;
    U32(self, 0x04) = 0x3f800000u;
    U32(self, 0x18) = 0x3f800000u;
    U32(self, 0x2c) = 0x3f800000u;
    U32(self, 0x40) = 0x3f800000u;
}

}  // namespace

// 0x000714c0  png_read_transform_info
SG_HOOK("jgld.dll", 0x000714c0, png_read_transform_info, ReadTransformInfo_re, ReadTransformInfo_orig);
// 0x0007dc70  png_set_IHDR
SG_HOOK("jgld.dll", 0x0007dc70, png_set_IHDR, SetIhdr_re, SetIhdr_orig);
// 0x0006eae0  png_get_IHDR
SG_HOOK("jgld.dll", 0x0006eae0, png_get_IHDR, GetIhdr_re, GetIhdr_orig);
// 0x00078cb0  png_set_read_fn
SG_HOOK("jgld.dll", 0x00078cb0, png_set_read_fn, SetReadFn_re, SetReadFn_orig);
// 0x0006a380  Palette::reset
SG_HOOK("jgld.dll", 0x0006a380, Palette_reset, PaletteReset_re, PaletteReset_orig);
// 0x0006a4a0  Palette::apply
SG_HOOK("jgld.dll", 0x0006a4a0, Palette_apply, PaletteApply_re, PaletteApply_orig);
// 0x000045f0  Matrix::loadIdentity
SG_HOOK("jgld.dll", 0x000045f0, Matrix_loadIdentity, MatrixLoadIdentity_re, MatrixLoadIdentity_orig);

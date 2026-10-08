// Batch c3bb: jgld.dll non-render leaves and near-leaves - four libpng 1.0.5 row/filter routines
// (png_do_unshift, png_do_expand_palette, png_do_dither, png_read_filter_row), libpng's chunk-name
// formatter (png_format_buffer), zlib 1.0.2's window flush (inflate_flush), the two Array<T>::add
// template instances and six one-statement accessors of jgld's own classes (Array<T>::count,
// Array<T>::reset, Surface::getField810, initDepthInfo).
//
// SG_HOOK addresses are RVAs; every instruction cited in a comment is a VA (0x10000000 + RVA), the form
// the debug build's disassembly prints. jgld.dll is LoadLibrary'd after the shim starts, so these hooks
// install from the LoadLibraryA hook (SgHooksInstallPending, re/hooks.cpp) and diff_hook finds them by
// (module, rva) through SimGolfShim_FindHookM. __thiscall is emulated with __fastcall: ecx = this,
// edx unused; a by-value struct argument then lands on the stack exactly where the original reads it.
//
// No floating point: every body here is integer or byte work.
//
// Nothing live in the running jgld.dll is touched: no Display, Surface, Palette, Font, the game's
// png_struct, inflate state or heap blocks. The routines that stay original (the CRT memcpy, operator
// new / operator delete, png_warning, the png_digit table, the warning string) are reached at their own
// module addresses, so each comparison is of this code and not of a CRT.
#include "hooks.h"

namespace {

// jgld.dll is relocatable, so every module address below is resolved against the running base.
char* JgldBase() {
    static char* base = 0;
    if (!base) base = reinterpret_cast<char*>(GetModuleHandleA("jgld.dll"));
    return base;
}

// libpng 1.0.5's png_row_info, read off the bodies: width at +0 (0x10076edb), rowbytes at +4
// (0x1007cec0), color_type at +8 (0x100720e3), bit_depth at +9 (0x10076ef6), channels at +0xa
// (written at 0x1007721d), pixel_depth at +0xb (0x1007cecb). The four tail fields are always read with
// a zero-extending byte load.
struct PngRowInfo {
    unsigned width;
    unsigned rowbytes;
    unsigned char color_type;
    unsigned char bit_depth;
    unsigned char channels;
    unsigned char pixel_depth;
};

// libpng 1.0.5's png_color_8, read off png_do_unshift: red +0 (0x10072120), green +1 (0x10072141),
// blue +2 (0x10072163), gray +3 (0x10072187), alpha +4 (0x100721b8).
struct PngColor8 {
    unsigned char red, green, blue, gray, alpha;
};

// The routines that stay original.
typedef void*(__cdecl* Memcpy_t)(void*, const void*, unsigned);   // 0x1007f3a0
typedef void*(__cdecl* New_t)(unsigned);                          // 0x1007f0f0  operator new
typedef void(__cdecl* Delete_t)(void*);                           // 0x1007e6d0  operator delete
typedef void(__cdecl* PngWarning_t)(void*, const char*);          // 0x10079040

Memcpy_t JgldMemcpy() { return reinterpret_cast<Memcpy_t>(JgldBase() + 0x7f3a0); }
New_t JgldNew() { return reinterpret_cast<New_t>(JgldBase() + 0x7f0f0); }
Delete_t JgldDelete() { return reinterpret_cast<Delete_t>(JgldBase() + 0x7e6d0); }
PngWarning_t JgldPngWarning() { return reinterpret_cast<PngWarning_t>(JgldBase() + 0x79040); }

// --------------------------------------------------------------- jgld's own one-statement accessors

// Array<T> (re/match/jgld_font.cpp): a virtual destructor puts a vtable pointer at +0, then the buffer
// at +4 (0x100681d7), the capacity at +8 (0x100681c6), the count at +0xc (0x100681c3) and the growth
// step at +0x10 (0x10068203).
struct ArrayHdr {
    void* vtbl;
    void* m_data;
    int m_cap;
    int m_count;
    int m_grow;
};

// 0x00066a80  Array_Rec14::count
// int __thiscall Array<Rec14>::count(): one load, `mov eax, dword ptr [eax+0xc]` at 0x10066aa0, of the
// count field reached through the saved `this` at [ebp-4] (0x10066a9a / 0x10066a9d). No argument
// (`ret` at 0x10066aa9), no branch, nothing written.
typedef int(__fastcall* ArrayCount_t)(void*, void*);
ArrayCount_t ArrayCount14_orig;
int __fastcall ArrayCount14_re(void* self, void*) {
    return static_cast<ArrayHdr*>(self)->m_count;
}

// 0x00066b50  Array_Rec94::count
// int __thiscall Array<Rec94>::count(): the same single load at 0x10066b70. The two instances differ
// only in the element size their add/clear use, not here, so the two bodies are byte-identical apart
// from their addresses.
ArrayCount_t ArrayCount94_orig;
int __fastcall ArrayCount94_re(void* self, void*) {
    return static_cast<ArrayHdr*>(self)->m_count;
}

// 0x00066ac0  Array_Rec14::reset
// void __thiscall Array<Rec14>::reset(): one store, `mov dword ptr [eax+0xc], 0` at 0x10066ae0, of the
// count field; the buffer, the capacity and the growth step are left as they are. No argument (`ret` at
// 0x10066aed) and no instruction sets a return value - eax holds `this` only because of the reload at
// 0x10066add - so the comparison is on memory.
typedef void(__fastcall* ArrayReset_t)(void*, void*);
ArrayReset_t ArrayReset14_orig;
void __fastcall ArrayReset14_re(void* self, void*) {
    static_cast<ArrayHdr*>(self)->m_count = 0;
}

// 0x00066b90  Array_Rec94::reset
// void __thiscall Array<Rec94>::reset(): the same single store at 0x10066bb0.
ArrayReset_t ArrayReset94_orig;
void __fastcall ArrayReset94_re(void* self, void*) {
    static_cast<ArrayHdr*>(self)->m_count = 0;
}

// 0x0000aef0  Surface::getField810
// int __thiscall Surface::getField810(): one load, `mov eax, dword ptr [eax+0x810]` at 0x1000af10, of
// the dword 0x810 bytes into the surface. No argument (`ret` at 0x1000af1c), no branch, nothing
// written. Called by Surface::setPalette 0x100089d0.
typedef int(__fastcall* GetField810_t)(void*, void*);
GetField810_t GetField810_orig;
int __fastcall GetField810_re(void* self, void*) {
    return *reinterpret_cast<int*>(static_cast<char*>(self) + 0x810);
}

// 0x000092e0  initDepthInfo
// void* __thiscall initDepthInfo(): one store, `mov dword ptr [eax+4], 0` at 0x10009300, then the
// reload of `this` at 0x10009307 that leaves it in eax at the `ret` (0x10009310), i.e. the body is a
// constructor shape: clear the second dword of the record and return the record. The first dword
// (+0) is not touched. Surface::ctor 0x10007660 calls it with ecx = this + 0x24.
typedef void*(__fastcall* InitDepthInfo_t)(void*, void*);
InitDepthInfo_t InitDepthInfo_orig;
void* __fastcall InitDepthInfo_re(void* self, void*) {
    reinterpret_cast<int*>(self)[1] = 0;
    return self;
}

// ------------------------------------------------------------------------- Array<T>::add (two instances)

// The 0x14-byte element of Array<Rec14> and the 0x94-byte element of Array<Rec94>, both passed BY VALUE:
// the original copies them out of the stack frame with `rep movsd` of 5 dwords from [ebp+8]
// (0x100681e4) and of 0x25 dwords from [ebp+8] (0x10068447), and pops them itself (`ret 0x14` at
// 0x100682b1, `ret 0x94` at 0x1006851d).
struct Rec14 {
    int v[5];
};
struct Rec94 {
    int v[0x25];
};

// 0x000681a0  Array_Rec14::add
// int __thiscall Array<Rec14>::add(Rec14 v): appends one element and returns its index.
// The branch at 0x100681c9 (`cmp edx, dword ptr [ecx+8]` / `jge 0x100681fa`, a SIGNED compare of the
// count at +0xc against the capacity at +8) selects the two bodies.
//   count < cap: the element is written at m_data + count * 0x14 (`imul ecx, ecx, 0x14` at 0x100681d1,
//   added to the buffer at 0x100681da, the 5-dword copy at 0x100681e4) and the count is incremented
//   (0x100681ec, stored at 0x100681f2).
//   count >= cap: a new buffer of (cap + grow) * 0x14 bytes is taken from operator new 0x1007f0f0
//   (size computed at 0x100681fd-0x10068206, `call` at 0x1006820a); the capacity becomes cap + grow
//   (0x10068224, stored at 0x1006822a) BEFORE the copy; the old buffer is copied only when it is not
//   null (`cmp dword ptr [edx+4], 0` / `je 0x10068253` at 0x10068230-0x10068234) with the CRT memcpy
//   0x1007f3a0 over count * 0x14 bytes (`call` at 0x1006824b); the element goes to new + count * 0x14
//   (0x10068269); the count is incremented (0x10068271); the OLD buffer is released through operator
//   delete 0x1007e6d0 (`call` at 0x10068287, the pointer read at 0x1006827d) and the new one stored at
//   +4 (0x10068295).
// Both paths end at 0x10068298, which returns count - 1 (`sub eax, 1` at 0x1006829e).
typedef int(__fastcall* Add14_t)(void*, void*, Rec14);
Add14_t Add14_orig;
int __fastcall Add14_re(void* self, void*, Rec14 v) {
    ArrayHdr* a = static_cast<ArrayHdr*>(self);
    if (a->m_count < a->m_cap) {
        static_cast<Rec14*>(a->m_data)[a->m_count] = v;
        a->m_count++;
    } else {
        Rec14* n = static_cast<Rec14*>(JgldNew()((a->m_cap + a->m_grow) * sizeof(Rec14)));
        a->m_cap += a->m_grow;
        if (a->m_data) JgldMemcpy()(n, a->m_data, a->m_count * sizeof(Rec14));
        n[a->m_count] = v;
        a->m_count++;
        JgldDelete()(a->m_data);
        a->m_data = n;
    }
    return a->m_count - 1;
}

// 0x00068400  Array_Rec94::add
// int __thiscall Array<Rec94>::add(Rec94 v): the same body with the element size 0x94 (`imul ecx, ecx,
// 0x94` at 0x10068431 and 0x10068469, the 0x25-dword copies at 0x10068447 and 0x100684cf) and the same
// signed capacity test at 0x10068429.
typedef int(__fastcall* Add94_t)(void*, void*, Rec94);
Add94_t Add94_orig;
int __fastcall Add94_re(void* self, void*, Rec94 v) {
    ArrayHdr* a = static_cast<ArrayHdr*>(self);
    if (a->m_count < a->m_cap) {
        static_cast<Rec94*>(a->m_data)[a->m_count] = v;
        a->m_count++;
    } else {
        Rec94* n = static_cast<Rec94*>(JgldNew()((a->m_cap + a->m_grow) * sizeof(Rec94)));
        a->m_cap += a->m_grow;
        if (a->m_data) JgldMemcpy()(n, a->m_data, a->m_count * sizeof(Rec94));
        n[a->m_count] = v;
        a->m_count++;
        JgldDelete()(a->m_data);
        a->m_data = n;
    }
    return a->m_count - 1;
}

// ------------------------------------------------------- libpng 1.0.5 row transformations (__cdecl)

// 0x000720c0  png_do_unshift
// void __cdecl png_do_unshift(png_row_info*, unsigned char* row, const png_color_8* sig_bits): shifts
// every sample right so that the significant bits the file declared end up in the low bits.
// Guard: color_type 3 returns at once (`cmp ecx, 3` / `je 0x100723f8` at 0x100720e6-0x100720e9).
// The width is read once into [ebp-0x20] (0x100720ff) and the shift array is the four ints at
// [ebp-0x10] (indexed `[ebp+edx*4-0x10]`, e.g. the store at 0x10072127).
// Bit 1 of color_type (`and edx, 2` / `je 0x1007217a` at 0x1007210c-0x10072111) selects three entries
// bit_depth - red/green/blue (0x10072118-0x10072175) or the single entry bit_depth - gray
// (0x1007217f-0x10072199); bit 2 (`and edx, 4` / `je 0x100721cd` at 0x100721a4-0x100721a9) appends
// bit_depth - alpha (0x100721b0-0x100721ca). Every term is a zero-extended byte minus a zero-extended
// byte, so a sig_bits value above bit_depth gives a negative entry.
// The clamp loop (`jge 0x10072206` at 0x100721e5) stores 0 over every entry that is <= 0 (`jg
// 0x100721fe` at 0x100721ef, a SIGNED test) and sets the flag word at [ebp-0x1c] to 1 for every entry
// that is > 0; if the flag is still 0 the function returns (`jne 0x10072218` at 0x10072211).
// bit_depth then selects the body through a switch on bit_depth - 2 (0x10072226) with the range test
// `cmp dword ptr [ebp-0x58], 0xe` / `ja 0x100723f8` at 0x1007222c-0x10072230 (UNSIGNED, so bit_depth 1
// wraps to 0xffffffff and returns), the byte index table at 0x10072413 and the jump table at
// 0x100723ff. Only 2, 4, 8 and 16 have a body; any other depth in range falls through to the end.
//   depth 2 (0x10072248): rowbytes bytes, each `*bp >>= 1` (`shr dl, 1` at 0x10072276) then `&= 0x55`
//   (0x10072282).
//   depth 4 (0x1007229a): rowbytes bytes, each `*bp >>= shift[0]` (`shr dl, cl` at 0x100722ef) then
//   masked with ((0xf0 >> shift[0]) & 0xf0) | (0xf >> shift[0]) computed once at
//   0x100722a9-0x100722ca, both halves with an arithmetic shift.
//   depth 8 (0x10072313): width * channels bytes (`imul` at 0x1007231c), each shifted by
//   shift[i % channels] - the index is an UNSIGNED division (`xor edx, edx` / `div dword ptr [ebp-0x14]`
//   at 0x10072340-0x10072342).
//   depth 16 (0x10072365): channels * width samples (`imul` at 0x1007236e), each read as a big-endian
//   pair into the same 16-bit variable the flag used (`shl edx, 8` / `add edx, ecx` at
//   0x10072396-0x100723a1, stored as a word at 0x100723a3), shifted as a 16-bit value (`shr ax, cl` at
//   0x100723b8) and written back high byte first (0x100723ce) then low byte (0x100723eb).
// Every loop counter is compared with its bound unsigned (`jae` at 0x1007226f, 0x100722e5, 0x1007233b,
// 0x1007238d), so a zero bound leaves the row untouched. No header field is written.
typedef void(__cdecl* PngDoUnshift_t)(PngRowInfo*, unsigned char*, const PngColor8*);
PngDoUnshift_t PngDoUnshift_orig;
void __cdecl PngDoUnshift_re(PngRowInfo* row_info, unsigned char* row, const PngColor8* sig_bits) {
    if (row_info->color_type == 3) return;
    int shift[4];
    int channels = 0;
    int c;
    unsigned short value = 0;
    unsigned row_width = row_info->width;

    if (row_info->color_type & 2) {
        shift[channels++] = row_info->bit_depth - sig_bits->red;
        shift[channels++] = row_info->bit_depth - sig_bits->green;
        shift[channels++] = row_info->bit_depth - sig_bits->blue;
    } else {
        shift[channels++] = row_info->bit_depth - sig_bits->gray;
    }
    if (row_info->color_type & 4) {
        shift[channels++] = row_info->bit_depth - sig_bits->alpha;
    }

    for (c = 0; c < channels; c++) {
        if (shift[c] <= 0)
            shift[c] = 0;
        else
            value = 1;
    }
    if (!value) return;

    switch (row_info->bit_depth) {
        case 2: {
            unsigned istop = row_info->rowbytes;
            unsigned char* bp = row;
            for (unsigned i = 0; i < istop; i++) {
                *bp >>= 1;
                *bp++ &= 0x55;
            }
            break;
        }
        case 4: {
            unsigned char* bp = row;
            unsigned istop = row_info->rowbytes;
            unsigned char mask = static_cast<unsigned char>(((0xf0 >> shift[0]) & 0xf0) |
                                                            static_cast<unsigned char>(0xf >> shift[0]));
            for (unsigned i = 0; i < istop; i++) {
                *bp >>= shift[0];
                *bp++ &= mask;
            }
            break;
        }
        case 8: {
            unsigned char* bp = row;
            unsigned istop = row_width * channels;
            for (unsigned i = 0; i < istop; i++) {
                *bp++ >>= shift[i % static_cast<unsigned>(channels)];
            }
            break;
        }
        case 16: {
            unsigned char* bp = row;
            unsigned istop = static_cast<unsigned>(channels) * row_width;
            for (unsigned i = 0; i < istop; i++) {
                value = static_cast<unsigned short>((*bp << 8) + *(bp + 1));
                value = static_cast<unsigned short>(value >> shift[i % static_cast<unsigned>(channels)]);
                *bp++ = static_cast<unsigned char>(value >> 8);
                *bp++ = static_cast<unsigned char>(value & 0xff);
            }
            break;
        }
    }
}

// 0x00076ec0  png_do_expand_palette
// void __cdecl png_do_expand_palette(png_row_info*, unsigned char* row, const unsigned char* palette,
// const unsigned char* trans, int num_trans): turns a row of palette indices into RGB or RGBA samples
// in place, writing backwards.
// The width is read first into [ebp-0x18] (0x10076edb) and is the loop bound of every loop (unsigned
// compares, `jae` at 0x10076f73, 0x10077012, 0x100770a2, 0x10077151, 0x1007725b). Guard: color_type
// must be 3 (`cmp eax, 3` / `jne 0x100772ec` at 0x10076ee8-0x10076eeb).
// First stage, only when bit_depth < 8 (`cmp edx, 8` / `jge 0x10077101` at 0x10076ef9-0x10076efc, a
// SIGNED compare of the zero-extended byte): one byte per pixel is unpacked backwards from
// sp = row + ((width - 1) >> k) and dp = row + width - 1, with the first shift 7 - ((width + 7) & 7)
// for depth 1 (0x10076f48-0x10076f58), (3 - ((width + 3) & 3)) * 2 for depth 2 (`shl eax, 1` at
// 0x10076ff5) and (width & 1) * 4 for depth 4 (`and ecx, 1` / `shl ecx, 2` at 0x10077081-0x10077084,
// which is NOT the formula png_do_unpack uses). Depth 1 stores the literal 1 or 0 (0x10076f8b /
// 0x10076f93); depths 2 and 4 store the masked value (0x10077022 / 0x100770b0). The shift wraps at 7,
// 6 and 4 respectively (`cmp dword ptr [ebp-4], 7` at 0x10076f96, 6 at 0x10077030, 4 at 0x100770be),
// stepping sp back one byte. A depth below 8 that is not 1, 2 or 4 jumps straight to the tail
// (`jmp 0x100770ea` at 0x10076f25), which is run by every path of this stage: bit_depth = 8
// (0x100770ed), pixel_depth = 8 (0x100770f4), rowbytes = width (0x100770fe).
// Second stage, only when bit_depth is now 8 (`cmp byte ptr [ebp-0x20], 8` / `je 0x10077115` at
// 0x1007710a-0x1007710e; any other depth returns unchanged):
//   trans not null (`cmp dword ptr [ebp+0x14], 0` / `je 0x10077226` at 0x10077115-0x10077119): four
//   bytes per pixel from sp = row + width - 1 down to dp = row + width * 4 - 1 (0x10077132), alpha
//   first - 0xff when the index is >= num_trans (`cmp edx, dword ptr [ebp+0x18]` / `jl 0x10077174` at
//   0x1007715e-0x10077161, a SIGNED compare, store at 0x10077166) else trans[index] (0x10077181) -
//   then blue, green, red out of the three-byte palette entries (`imul eax, eax, 3` at 0x10077196,
//   0x100771b5, 0x100771d4, with +2, +1 and +0). The header becomes bit_depth 8 (0x100771fc),
//   pixel_depth 0x20 (0x10077203), rowbytes = width * 4 (0x1007720a), color_type 6 (0x10077216),
//   channels 4 (0x1007721d).
//   trans null (0x10077226): three bytes per pixel from dp = row + width * 3 - 1 (0x10077236), blue,
//   green, red, and the header becomes pixel_depth 0x18 (0x100772ce), rowbytes = width * 3
//   (0x100772d5), color_type 2 (0x100772e1), channels 3 (0x100772e8).
typedef void(__cdecl* PngDoExpandPalette_t)(PngRowInfo*, unsigned char*, const unsigned char*,
                                            const unsigned char*, int);
PngDoExpandPalette_t PngDoExpandPalette_orig;
void __cdecl PngDoExpandPalette_re(PngRowInfo* row_info, unsigned char* row, const unsigned char* palette,
                                   const unsigned char* trans, int num_trans) {
    int shift, value;
    unsigned char* sp;
    unsigned char* dp;
    unsigned row_width = row_info->width;

    if (row_info->color_type != 3) return;

    if (static_cast<int>(row_info->bit_depth) < 8) {
        switch (row_info->bit_depth) {
            case 1:
                sp = row + ((row_width - 1) >> 3);
                dp = row + row_width - 1;
                shift = 7 - static_cast<int>((row_width + 7) & 7);
                for (unsigned i = 0; i < row_width; i++) {
                    if ((*sp >> shift) & 1)
                        *dp = 1;
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
                break;
            case 2:
                sp = row + ((row_width - 1) >> 2);
                dp = row + row_width - 1;
                shift = static_cast<int>(3 - ((row_width + 3) & 3)) << 1;
                for (unsigned i = 0; i < row_width; i++) {
                    value = (*sp >> shift) & 3;
                    *dp = static_cast<unsigned char>(value);
                    if (shift == 6) {
                        shift = 0;
                        sp--;
                    } else {
                        shift += 2;
                    }
                    dp--;
                }
                break;
            case 4:
                sp = row + ((row_width - 1) >> 1);
                dp = row + row_width - 1;
                shift = static_cast<int>(row_width & 1) << 2;
                for (unsigned i = 0; i < row_width; i++) {
                    value = (*sp >> shift) & 0xf;
                    *dp = static_cast<unsigned char>(value);
                    if (shift == 4) {
                        shift = 0;
                        sp--;
                    } else {
                        shift += 4;
                    }
                    dp--;
                }
                break;
        }
        row_info->bit_depth = 8;
        row_info->pixel_depth = 8;
        row_info->rowbytes = row_width;
    }

    if (row_info->bit_depth == 8) {
        if (trans != 0) {
            sp = row + row_width - 1;
            dp = row + (row_width << 2) - 1;
            for (unsigned i = 0; i < row_width; i++) {
                if (static_cast<int>(*sp) >= num_trans)
                    *dp-- = 0xff;
                else
                    *dp-- = trans[*sp];
                *dp-- = palette[*sp * 3 + 2];
                *dp-- = palette[*sp * 3 + 1];
                *dp-- = palette[*sp * 3];
                sp--;
            }
            row_info->bit_depth = 8;
            row_info->pixel_depth = 32;
            row_info->rowbytes = row_width * 4;
            row_info->color_type = 6;
            row_info->channels = 4;
        } else {
            sp = row + row_width - 1;
            dp = row + row_width * 3 - 1;
            for (unsigned i = 0; i < row_width; i++) {
                *dp-- = palette[*sp * 3 + 2];
                *dp-- = palette[*sp * 3 + 1];
                *dp-- = palette[*sp * 3];
                sp--;
            }
            row_info->bit_depth = 8;
            row_info->pixel_depth = 24;
            row_info->rowbytes = row_width * 3;
            row_info->color_type = 2;
            row_info->channels = 3;
        }
    }
}

// 0x00077a90  png_do_dither
// void __cdecl png_do_dither(png_row_info*, unsigned char* row, const unsigned char* palette_lookup,
// const unsigned char* dither_lookup): replaces each pixel with one palette index.
// The width is read first into [ebp-0x10] (0x10077aab). Three mutually exclusive bodies, each guarded
// by three tests in the same order - color_type, the lookup pointer, bit_depth 8:
//   color_type 2 (`cmp eax, 2` / `jne 0x10077bb7` at 0x10077ab8-0x10077abb), palette_lookup not null
//   (`cmp dword ptr [ebp+0x10], 0` / `je 0x10077bb7` at 0x10077ac1-0x10077ac5), bit_depth 8 (`cmp edx,
//   8` / `jne 0x10077bb7` at 0x10077ad3-0x10077ad6): three bytes are read per pixel and the index is
//   ((r >> 3) & 0x1f) << 10 | ((g >> 3) & 0x1f) << 5 | ((b >> 3) & 0x1f) (the shifts and masks at
//   0x10077b3e-0x10077b5e, five bits per channel), and palette_lookup[index] is stored at the write
//   pointer (0x10077b63-0x10077b6e), which advances one byte per pixel.
//   color_type 6 (`cmp eax, 6` / `jne 0x10077cc8` at 0x10077bbf-0x10077bc2) with the same two further
//   tests (0x10077bcc, 0x10077bdd): identical, plus one extra read pointer step per pixel for the
//   alpha sample (0x10077c49).
//   color_type 3 (`cmp ecx, 3` / `jne 0x10077d25` at 0x10077cd0-0x10077cd3), dither_lookup not null
//   (0x10077cd9), bit_depth 8 (0x10077ce6): each byte is replaced in place by dither_lookup[byte]
//   (0x10077d11-0x10077d21) and no header field is written.
// The first two bodies rewrite the header after their loop: color_type 3 (0x10077b81 / 0x10077c95),
// channels 1 (0x10077b88 / 0x10077c9c), pixel_depth = bit_depth (0x10077b95 / 0x10077ca9) and
// rowbytes = (width * pixel_depth + 7) >> 3 read back from the field just written (0x10077b9d-
// 0x10077baf / 0x10077cb1-0x10077cc3). All three loop counters are compared with the width unsigned
// (`jae` at 0x10077b00, 0x10077c07, 0x10077d0f), so width 0 moves no pixel.
typedef void(__cdecl* PngDoDither_t)(PngRowInfo*, unsigned char*, const unsigned char*, const unsigned char*);
PngDoDither_t PngDoDither_orig;
void __cdecl PngDoDither_re(PngRowInfo* row_info, unsigned char* row, const unsigned char* palette_lookup,
                            const unsigned char* dither_lookup) {
    unsigned char* sp;
    unsigned char* dp;
    unsigned row_width = row_info->width;

    if (row_info->color_type == 2 && palette_lookup && row_info->bit_depth == 8) {
        int r, g, b, p;
        sp = row;
        dp = row;
        for (unsigned i = 0; i < row_width; i++) {
            r = *sp++;
            g = *sp++;
            b = *sp++;
            p = (((r >> 3) & 0x1f) << 10) | (((g >> 3) & 0x1f) << 5) | ((b >> 3) & 0x1f);
            *dp++ = palette_lookup[p];
        }
        row_info->color_type = 3;
        row_info->channels = 1;
        row_info->pixel_depth = row_info->bit_depth;
        row_info->rowbytes = (row_width * row_info->pixel_depth + 7) >> 3;
    } else if (row_info->color_type == 6 && palette_lookup && row_info->bit_depth == 8) {
        int r, g, b, p;
        sp = row;
        dp = row;
        for (unsigned i = 0; i < row_width; i++) {
            r = *sp++;
            g = *sp++;
            b = *sp++;
            sp++;
            p = (((r >> 3) & 0x1f) << 10) | (((g >> 3) & 0x1f) << 5) | ((b >> 3) & 0x1f);
            *dp++ = palette_lookup[p];
        }
        row_info->color_type = 3;
        row_info->channels = 1;
        row_info->pixel_depth = row_info->bit_depth;
        row_info->rowbytes = (row_width * row_info->pixel_depth + 7) >> 3;
    } else if (row_info->color_type == 3 && dither_lookup && row_info->bit_depth == 8) {
        sp = row;
        for (unsigned i = 0; i < row_width; i++, sp++) {
            *sp = dither_lookup[*sp];
        }
    }
}

// 0x0007ce80  png_read_filter_row
// void __cdecl png_read_filter_row(png_struct* png_ptr, png_row_info*, unsigned char* row,
// const unsigned char* prev_row, int filter): undoes one PNG row filter in place.
// The filter number is copied to [ebp-0x78] (0x1007ce9e) and dispatched with `cmp dword ptr [ebp-0x78],
// 4` / `ja 0x1007d236` at 0x1007cea4-0x1007cea8 (UNSIGNED, so a negative filter also takes the default)
// and the jump table at 0x1007d261, whose five entries are 0x1007ceb8 (filter 0: straight to the exit),
// 0x1007cebd (1), 0x1007cf32 (2), 0x1007cf95 (3) and 0x1007d079 (4).
// bpp is (pixel_depth + 7) >> 3 with an arithmetic shift of the zero-extended byte (0x1007cecb-
// 0x1007ced1 and the twins at 0x1007cfaf and 0x1007d099); rowbytes comes from the header's +4.
//   filter 1 (sub): i runs from bpp to rowbytes (unsigned, `jae` at 0x1007cefd) and each byte gets
//   (*rp + *lp++) & 0xff (0x1007cf0d-0x1007cf17) with rp starting bpp bytes into the row (0x1007ceda)
//   and lp at the row start.
//   filter 2 (up): rowbytes bytes, each (*rp + *pp++) & 0xff (0x1007cf6f-0x1007cf7a).
//   filter 3 (avg): the first bpp bytes get (*rp + *pp++ / 2) & 0xff with a SIGNED division by two
//   (`cdq` / `sub eax, edx` / `sar eax, 1` at 0x1007cfec-0x1007cfef); the remaining rowbytes - bpp
//   bytes (0x1007cfbe) get (*rp + (*pp++ + *lp++) / 2) & 0xff, again a signed halving of the sum
//   (0x1007d045-0x1007d048).
//   filter 4 (paeth): the first bpp bytes get (*rp + *pp++) & 0xff (0x1007d0d6); each of the remaining
//   rowbytes - bpp bytes (0x1007d0a8) reads a = *lp++ (0x1007d119), b = *pp++ (0x1007d12c), c = *cp++
//   (0x1007d13f), forms p = b - c (0x1007d150) and pc = a - c (0x1007d159), takes pa = |p|
//   (`jge 0x1007d16f` at 0x1007d163), pb = |pc| (`jge 0x1007d18b` at 0x1007d17f) and pc = |p + pc|
//   (`jge 0x1007d1b1` at 0x1007d19f), then picks a when pa <= pb and pa <= pc (`jg 0x1007d1e1` at
//   0x1007d1cc and 0x1007d1d4), else b when pb <= pc (`jg 0x1007d1f4` at 0x1007d1e7), else c, and adds
//   the pick to the byte (0x1007d219).
//   any other filter (0x1007d236): png_warning 0x10079040 is called with the string at 0x1011eab0
//   (`call` at 0x1007d23f) and the first byte of the row is set to 0 (0x1007d24a).
typedef void(__cdecl* PngReadFilterRow_t)(void*, PngRowInfo*, unsigned char*, const unsigned char*, int);
PngReadFilterRow_t PngReadFilterRow_orig;
void __cdecl PngReadFilterRow_re(void* png_ptr, PngRowInfo* row_info, unsigned char* row,
                                 const unsigned char* prev_row, int filter) {
    switch (filter) {
        case 0:
            break;
        case 1: {
            unsigned istop = row_info->rowbytes;
            unsigned bpp = (row_info->pixel_depth + 7) >> 3;
            unsigned char* rp = row + bpp;
            const unsigned char* lp = row;
            for (unsigned i = bpp; i < istop; i++) {
                *rp = static_cast<unsigned char>((static_cast<int>(*rp) + static_cast<int>(*lp++)) & 0xff);
                rp++;
            }
            break;
        }
        case 2: {
            unsigned istop = row_info->rowbytes;
            unsigned char* rp = row;
            const unsigned char* pp = prev_row;
            for (unsigned i = 0; i < istop; i++) {
                *rp = static_cast<unsigned char>((static_cast<int>(*rp) + static_cast<int>(*pp++)) & 0xff);
                rp++;
            }
            break;
        }
        case 3: {
            unsigned char* rp = row;
            const unsigned char* pp = prev_row;
            const unsigned char* lp = row;
            unsigned bpp = (row_info->pixel_depth + 7) >> 3;
            unsigned istop = row_info->rowbytes - bpp;
            for (unsigned i = 0; i < bpp; i++) {
                *rp = static_cast<unsigned char>((static_cast<int>(*rp) + static_cast<int>(*pp++) / 2) & 0xff);
                rp++;
            }
            for (unsigned i = 0; i < istop; i++) {
                *rp = static_cast<unsigned char>(
                    (static_cast<int>(*rp) + (static_cast<int>(*pp++) + static_cast<int>(*lp++)) / 2) & 0xff);
                rp++;
            }
            break;
        }
        case 4: {
            unsigned char* rp = row;
            const unsigned char* pp = prev_row;
            const unsigned char* lp = row;
            const unsigned char* cp = prev_row;
            unsigned bpp = (row_info->pixel_depth + 7) >> 3;
            unsigned istop = row_info->rowbytes - bpp;
            for (unsigned i = 0; i < bpp; i++) {
                *rp = static_cast<unsigned char>((static_cast<int>(*rp) + static_cast<int>(*pp++)) & 0xff);
                rp++;
            }
            for (unsigned i = 0; i < istop; i++) {
                int a = *lp++;
                int b = *pp++;
                int c = *cp++;
                int p = b - c;
                int pc = a - c;
                int pa = p < 0 ? -p : p;
                int pb = pc < 0 ? -pc : pc;
                pc = (p + pc) < 0 ? -(p + pc) : p + pc;
                p = (pa <= pb && pa <= pc) ? a : (pb <= pc) ? b : c;
                *rp = static_cast<unsigned char>((static_cast<int>(*rp) + p) & 0xff);
                rp++;
            }
            break;
        }
        default:
            JgldPngWarning()(png_ptr, JgldBase() + 0x11eab0);
            *row = 0;
            break;
    }
}

// 0x00079100  png_format_buffer
// void __cdecl png_format_buffer(png_struct* png_ptr, char* buffer, const char* message): writes the
// four-byte chunk name the png_struct carries at +0x11c (`mov cl, byte ptr [eax+0x11c]` at 0x10079138,
// zero-extended) into the caller's buffer, escaping every byte that is not a plain chunk character,
// and appends the message.
// The outer loop runs exactly four times (`cmp dword ptr [ebp-8], 4` / `jge 0x100791e1` at
// 0x10079126-0x1007912a, the index incremented at 0x10079144). A byte is escaped when it is < 0x29
// (`jl 0x10079162` at 0x1007914e), > 0x7a (`jg 0x10079162` at 0x10079154) or both > 0x5a (`jle
// 0x100791c8` at 0x1007915a) and < 0x61 (`jge 0x100791c8` at 0x10079160); all four compares are
// SIGNED on the zero-extended byte, so the three ranges are closed at 0x29, 0x7a, 0x5a and 0x61.
// An escaped byte becomes '[' (0x5b, stored at 0x10079168), the digit of its high nibble
// ((c & 0xf0) >> 4, an arithmetic shift at 0x1007917d) read from the table at 0x1011e0bc (0x10079186),
// the digit of its low nibble (0x100791a3) and ']' (0x5d at 0x100791ba); any other byte is copied as
// it is (0x100791d1). The output index advances by one after each store.
// A null message ends the buffer with one 0 at the current index (`cmp dword ptr [ebp+0x10], 0` /
// `jne 0x100791f2` at 0x100791e1-0x100791e5, store at 0x100791ed). A message appends ':' (0x3a at
// 0x100791f8), ' ' (0x20 at 0x1007920a), 0x40 bytes of the message through the CRT memcpy 0x1007f3a0
// (`push 0x40` at 0x10079216, `call` at 0x10079223) and a 0 at offset 0x3f of the copy (0x10079231),
// i.e. the 64 bytes are always read and copied whatever the message's length.
typedef void(__cdecl* PngFormatBuffer_t)(void*, char*, const char*);
PngFormatBuffer_t PngFormatBuffer_orig;
void __cdecl PngFormatBuffer_re(void* png_ptr, char* buffer, const char* message) {
    const unsigned char* chunk_name = reinterpret_cast<const unsigned char*>(png_ptr) + 0x11c;
    const char* png_digit = JgldBase() + 0x11e0bc;
    int iout = 0, iin = 0;

    while (iin < 4) {
        int c = chunk_name[iin++];
        if (c < 0x29 || c > 0x7a || (c > 0x5a && c < 0x61)) {
            buffer[iout++] = '[';
            buffer[iout++] = png_digit[(c & 0xf0) >> 4];
            buffer[iout++] = png_digit[c & 0xf];
            buffer[iout++] = ']';
        } else {
            buffer[iout++] = static_cast<char>(c);
        }
    }

    if (message == 0) {
        buffer[iout] = 0;
    } else {
        buffer[iout++] = ':';
        buffer[iout++] = ' ';
        JgldMemcpy()(buffer + iout, message, 64);
        buffer[iout + 63] = 0;
    }
}

// ----------------------------------------------------------------------------- zlib 1.0.2 (__cdecl)

// 0x000a0130  inflate_flush
// int __cdecl inflate_flush(inflate_blocks_state* s, z_stream* z, int r): copies as much of the sliding
// window as fits into the caller's output buffer and returns the (possibly corrected) status.
// Field offsets, all read in this body: z->next_out +0xc (0x100a014b), z->avail_out +0x10 (0x100a0188),
// z->total_out +0x14 (0x100a01bb), z->adler +0x30 (0x100a0200); s->window +0x24 (0x100a023b), s->end
// +0x28 (0x100a0173), s->read +0x2c (0x100a0154), s->write +0x30 (0x100a0160), s->checkfn +0x34
// (0x100a01ca) and s->check +0x38 (0x100a01dd).
// First block: n is (read <= write ? write : end) - read (`cmp edx, dword ptr [ecx+0x30]` / `ja
// 0x100a0170` at 0x100a0160-0x100a0163, an UNSIGNED pointer compare; the subtraction at 0x100a017c),
// clamped to avail_out (`jbe 0x100a0196` at 0x100a018b, unsigned); when n is not zero (`je 0x100a01a9`
// at 0x100a019a) and r is -5 (`cmp dword ptr [ebp+0x10], -5` / `jne 0x100a01a9` at
// 0x100a019c-0x100a01a0) r becomes 0 (0x100a01a2). avail_out loses n (0x100a01af) and total_out gains
// it (0x100a01be). When checkfn is not null (`je 0x100a0203` at 0x100a01ce) it is called __cdecl with
// (check, read, n) (pushes at 0x100a01d5, 0x100a01d9, 0x100a01e0, `call dword ptr [edx+0x34]` at
// 0x100a01e4) and its result is stored both in s->check (0x100a01f4) and in z->adler (0x100a0200).
// The bytes move with the CRT memcpy 0x1007f3a0 (`call` at 0x100a020f) and both pointers advance by n
// (0x100a021a, 0x100a0223).
// Second block, only when the read pointer has reached the end of the window (`cmp edx, dword ptr
// [ecx+0x28]` / `jne 0x100a030e` at 0x100a022f-0x100a0232): the read pointer wraps to window
// (0x100a023e), the write pointer wraps too when it was at the end (`jne 0x100a025b` at 0x100a024d,
// store at 0x100a0258), and the same five steps run again over n = write - read (0x100a0261) with the
// same clamp (0x100a0270), the same -5 correction (0x100a027f, 0x100a0285), the same counter updates
// (0x100a0294, 0x100a02a3), the same optional checkfn call (0x100a02b3-0x100a02e5) and the same memcpy
// (0x100a02f4).
// Finally z->next_out and s->read are written back (0x100a0314, 0x100a031d) and r is returned
// (0x100a0320).
typedef unsigned(__cdecl* ZCheckFn_t)(unsigned, const unsigned char*, unsigned);
typedef int(__cdecl* InflateFlush_t)(void*, void*, int);
InflateFlush_t InflateFlush_orig;
int __cdecl InflateFlush_re(void* s_, void* z_, int r) {
    char* s = static_cast<char*>(s_);
    char* z = static_cast<char*>(z_);
    unsigned char** z_next_out = reinterpret_cast<unsigned char**>(z + 0x0c);
    unsigned* z_avail_out = reinterpret_cast<unsigned*>(z + 0x10);
    unsigned* z_total_out = reinterpret_cast<unsigned*>(z + 0x14);
    unsigned* z_adler = reinterpret_cast<unsigned*>(z + 0x30);
    unsigned char** s_window = reinterpret_cast<unsigned char**>(s + 0x24);
    unsigned char** s_end = reinterpret_cast<unsigned char**>(s + 0x28);
    unsigned char** s_read = reinterpret_cast<unsigned char**>(s + 0x2c);
    unsigned char** s_write = reinterpret_cast<unsigned char**>(s + 0x30);
    ZCheckFn_t* s_checkfn = reinterpret_cast<ZCheckFn_t*>(s + 0x34);
    unsigned* s_check = reinterpret_cast<unsigned*>(s + 0x38);

    unsigned char* p = *z_next_out;
    unsigned char* q = *s_read;

    unsigned n = static_cast<unsigned>((q <= *s_write ? *s_write : *s_end) - q);
    if (n > *z_avail_out) n = *z_avail_out;
    if (n && r == -5) r = 0;

    *z_avail_out -= n;
    *z_total_out += n;

    if (*s_checkfn != 0) {
        *s_check = (*s_checkfn)(*s_check, q, n);
        *z_adler = *s_check;
    }

    JgldMemcpy()(p, q, n);
    p += n;
    q += n;

    if (q == *s_end) {
        q = *s_window;
        if (*s_write == *s_end) *s_write = *s_window;

        n = static_cast<unsigned>(*s_write - q);
        if (n > *z_avail_out) n = *z_avail_out;
        if (n && r == -5) r = 0;

        *z_avail_out -= n;
        *z_total_out += n;

        if (*s_checkfn != 0) {
            *s_check = (*s_checkfn)(*s_check, q, n);
            *z_adler = *s_check;
        }

        JgldMemcpy()(p, q, n);
        p += n;
        q += n;
    }

    *z_next_out = p;
    *s_read = q;
    return r;
}

}  // namespace

SG_HOOK("jgld.dll", 0x00066a80, Array_Rec14_count, ArrayCount14_re, ArrayCount14_orig);
SG_HOOK("jgld.dll", 0x00066b50, Array_Rec94_count, ArrayCount94_re, ArrayCount94_orig);
SG_HOOK("jgld.dll", 0x00066ac0, Array_Rec14_reset, ArrayReset14_re, ArrayReset14_orig);
SG_HOOK("jgld.dll", 0x00066b90, Array_Rec94_reset, ArrayReset94_re, ArrayReset94_orig);
SG_HOOK("jgld.dll", 0x0000aef0, Surface_getField810, GetField810_re, GetField810_orig);
SG_HOOK("jgld.dll", 0x000092e0, initDepthInfo, InitDepthInfo_re, InitDepthInfo_orig);
SG_HOOK("jgld.dll", 0x000681a0, Array_Rec14_add, Add14_re, Add14_orig);
SG_HOOK("jgld.dll", 0x00068400, Array_Rec94_add, Add94_re, Add94_orig);
SG_HOOK("jgld.dll", 0x000720c0, png_do_unshift, PngDoUnshift_re, PngDoUnshift_orig);
SG_HOOK("jgld.dll", 0x00076ec0, png_do_expand_palette, PngDoExpandPalette_re, PngDoExpandPalette_orig);
SG_HOOK("jgld.dll", 0x00077a90, png_do_dither, PngDoDither_re, PngDoDither_orig);
SG_HOOK("jgld.dll", 0x0007ce80, png_read_filter_row, PngReadFilterRow_re, PngReadFilterRow_orig);
SG_HOOK("jgld.dll", 0x00079100, png_format_buffer, PngFormatBuffer_re, PngFormatBuffer_orig);
SG_HOOK("jgld.dll", 0x000a0130, inflate_flush, InflateFlush_re, InflateFlush_orig);

// Batch c3ao: reimplementations inside jgld.dll (SG_HOOK addresses are RVAs; the module's VAs are 0x10000000 + RVA,
// and every instruction cited in the comments is a VA, as the debug build's disassembly prints them).
// jgld.dll is LoadLibrary'd after the shim starts, so these hooks install from the LoadLibraryA hook
// (SgHooksInstallPending, re/hooks.cpp) and diff_hook finds them by (module, rva) through SimGolfShim_FindHookM.
// __thiscall is emulated with __fastcall: ecx = this, edx unused.
// Integer functions only: none of the fourteen bodies contains an x87 instruction.
#include "hooks.h"

namespace {

// jgld.dll is relocatable, so every module address used below (read-only tables and the two callees that stay
// original) is resolved against the running base.
char* JgldBase() {
    static char* base = 0;
    if (!base) base = reinterpret_cast<char*>(GetModuleHandleA("jgld.dll"));
    return base;
}

// libpng 1.0.5's png_row_info, read off the bodies: width at +0 (0x1006d9b6), rowbytes at +4 (0x1006d93a),
// color_type at +8 (0x1006d92a), bit_depth at +9 (0x1006d91d), channels at +0xa (0x1006d9b0), pixel_depth at
// +0xb (written at 0x100724c5). All four tail fields are read with a zero-extending byte load.
struct PngRowInfo {
    unsigned width;
    unsigned rowbytes;
    unsigned char color_type;
    unsigned char bit_depth;
    unsigned char channels;
    unsigned char pixel_depth;
};

// The CRT fragments the originals call; both stay original so the comparison is of this code, not of a CRT.
typedef void*(__cdecl* Memcpy_t)(void*, const void*, unsigned);     // 0x1007f3a0, `rep movsd` block copy
typedef int(__cdecl* Memcmp_t)(const void*, const void*, unsigned); // 0x100828b0
typedef unsigned(__cdecl* Crc32_t)(unsigned, const unsigned char*, unsigned);  // 0x1009ca70, zlib 1.0.2 crc32

Memcpy_t JgldMemcpy() { return reinterpret_cast<Memcpy_t>(JgldBase() + 0x7f3a0); }

// ----------------------------------------------------------------- libpng row transformations (__cdecl, leaves)

// 0x0006d900  png_do_invert(row_info, row): returns without touching the row unless bit_depth is 1
// (cmp ecx,1 / jne at 0x1006d920-0x1006d923) and color_type is 0 (test eax,eax / jne at 0x1006d92d-0x1006d92f);
// neither pointer is tested for null. It then walks rowbytes (read at 0x1006d93a) bytes from row, replacing each
// byte with its bitwise complement (`not ecx` at 0x1006d961, stored at 0x1006d966). The loop counter is compared
// with rowbytes UNSIGNED (jae at 0x1006d958), so rowbytes 0 leaves the row untouched.
typedef void(__cdecl* PngDoInvert_t)(PngRowInfo*, unsigned char*);
PngDoInvert_t PngDoInvert_orig;
void __cdecl PngDoInvert_re(PngRowInfo* row_info, unsigned char* row) {
    if (row_info->bit_depth != 1) return;
    if (row_info->color_type != 0) return;
    unsigned char* rp = row;
    unsigned istop = row_info->rowbytes;
    for (unsigned i = 0; i < istop; i++) {
        *rp = (unsigned char)~*rp;
        rp++;
    }
}

// 0x0006d980  png_do_swap(row_info, row): returns unless bit_depth is 16 (cmp ecx,0x10 / jne at
// 0x1006d9a0-0x1006d9a3). The sample count is width * channels (imul at 0x1006d9b8, width from [row_info]
// 0x1006d9b6, channels from [row_info+0xa] 0x1006d9b0) and is compared with the counter UNSIGNED (jae at
// 0x1006d9df). Each step swaps the two bytes of one 16-bit sample through a one-byte local at [ebp-0x10]
// (0x1006d9e6, 0x1006d9f2, 0x1006d9fa) and advances the pointer by 2 (0x1006d9d3).
typedef void(__cdecl* PngDoSwap_t)(PngRowInfo*, unsigned char*);
PngDoSwap_t PngDoSwap_orig;
void __cdecl PngDoSwap_re(PngRowInfo* row_info, unsigned char* row) {
    if (row_info->bit_depth != 16) return;
    unsigned char* rp = row;
    unsigned istop = row_info->width * (unsigned)row_info->channels;
    for (unsigned i = 0; i < istop; i++) {
        unsigned char t = rp[0];
        rp[0] = rp[1];
        rp[1] = t;
        rp += 2;
    }
}

// 0x0006da10  png_do_packswap(row_info, row): returns at once when bit_depth is 8 or more (cmp ecx,8 / jge at
// 0x1006da30-0x1006da33; the compare is signed but the value is a zero-extended byte, so it is a plain >= 8).
// The end pointer row + rowbytes is computed first (0x1006da3b). bit_depth selects one of three 256-byte
// bit-reversal tables in the module's data: 1 -> 0x10122e6c (0x1006da4e), 2 -> 0x10122f6c (0x1006da64),
// 4 -> 0x1012306c (0x1006da7a); any other value below 8 jumps to the epilogue (0x1006da83) with nothing written.
// The loop then replaces every byte from row up to (not including) end with table[byte] (0x1006daab, store at
// 0x1006daae); the pointer compare is UNSIGNED (jae at 0x1006da9c).
typedef void(__cdecl* PngDoPackswap_t)(PngRowInfo*, unsigned char*);
PngDoPackswap_t PngDoPackswap_orig;
void __cdecl PngDoPackswap_re(PngRowInfo* row_info, unsigned char* row) {
    if (row_info->bit_depth >= 8) return;
    unsigned char* end = row + row_info->rowbytes;
    const unsigned char* table;
    if (row_info->bit_depth == 1)      table = reinterpret_cast<const unsigned char*>(JgldBase() + 0x122e6c);
    else if (row_info->bit_depth == 2) table = reinterpret_cast<const unsigned char*>(JgldBase() + 0x122f6c);
    else if (row_info->bit_depth == 4) table = reinterpret_cast<const unsigned char*>(JgldBase() + 0x12306c);
    else return;
    for (unsigned char* rp = row; rp < end; rp++) *rp = table[*rp];
}

// 0x00072430  png_do_chop(row_info, row): returns unless bit_depth is 16 (cmp ecx,0x10 / jne at
// 0x10072450-0x10072453). Two cursors start at row: the source at [ebp-4], advanced by 2 (0x1007248d), and the
// destination at [ebp-8], advanced by 1 (0x10072499). width * channels samples (imul at 0x10072472, counter
// compared UNSIGNED with jae at 0x100724a2) are rewritten as `dest = *source` (0x100724aa, 0x100724ac), i.e. the
// first byte of each big-endian 16-bit sample is kept. AFTER the loop -- also when it ran zero times -- the
// header is rewritten: bit_depth 8 (0x100724b3), pixel_depth = channels * 8 as a byte (shl edx,3 at 0x100724bf,
// stored at 0x100724c5) and rowbytes = width * channels (imul at 0x100724d5, stored at 0x100724db).
typedef void(__cdecl* PngDoChop_t)(PngRowInfo*, unsigned char*);
PngDoChop_t PngDoChop_orig;
void __cdecl PngDoChop_re(PngRowInfo* row_info, unsigned char* row) {
    if (row_info->bit_depth != 16) return;
    unsigned char* sp = row;
    unsigned char* dp = row;
    unsigned istop = row_info->width * (unsigned)row_info->channels;
    for (unsigned i = 0; i < istop; i++) {
        *dp = *sp;
        sp += 2;
        dp++;
    }
    row_info->bit_depth = 8;
    row_info->pixel_depth = (unsigned char)(row_info->channels * 8);
    row_info->rowbytes = row_info->width * (unsigned)row_info->channels;
}

// ----------------------------------------------------------------- jgld surface helper (__cdecl, leaf)

// 0x0000af30  fillWords(dst, v, n): fills n 16-bit words with v. The pair constant is built first: v masked to
// 16 bits and shifted up 16 (0x1000af4b, 0x1000af50) OR'd with v masked to 16 bits (0x1000af5c). When n is ODD
// (and edx,1 / test / je at 0x1000af64-0x1000af69) one word is written at dst (`mov word [eax],cx` at 0x1000af72,
// the word taken straight from the argument slot) and the cursor starts at dst+2 (0x1000af78); otherwise the
// cursor starts at dst (0x1000af83). The remaining count is n / 2 rounded toward zero (cdq / sub / sar at
// 0x1000af89-0x1000af8c), and the loop writes one dword per step while that count is > 0 (`cmp [ebp-4],0` / jle
// at 0x1000af9c-0x1000afa0), advancing by 4 (0x1000afad). A negative odd n therefore still writes the single
// leading word and nothing else; a negative even n writes nothing at all.
typedef void(__cdecl* FillWords_t)(void*, unsigned, int);
FillWords_t FillWords_orig;
void __cdecl FillWords_re(void* dst, unsigned v, int n) {
    unsigned pair = ((v & 0xffffu) << 16) | (v & 0xffffu);
    unsigned char* p;
    if ((unsigned)n & 1u) {
        *reinterpret_cast<unsigned short*>(dst) = (unsigned short)v;
        p = static_cast<unsigned char*>(dst) + 2;
    } else {
        p = static_cast<unsigned char*>(dst);
    }
    for (int count = n / 2; count > 0; count--) {
        *reinterpret_cast<unsigned*>(p) = pair;
        p += 4;
    }
}

// ----------------------------------------------------------------- Palette (thiscall, leaf)

// 0x0006ae50  Palette::toRGBQuads(out): copies the object's 256 four-byte palette entries, which start at
// this+0xc (the three loads at 0x1006ae94, 0x1006aea8 and 0x1006aebc index [this + i*4 + 0xc/0xd/0xe]), into
// out as 256 four-byte quads with the first and third bytes exchanged: entry byte 0 goes to out[i*4+2]
// (0x1006ae98), byte 1 to out[i*4+1] (0x1006aeac), byte 2 to out[i*4] (0x1006aec0) and out[i*4+3] is zeroed
// (0x1006aec9). The counter is an int compared with 0x100 (cmp / jge at 0x1006ae7f-0x1006ae86), so the count is
// fixed at 256 and the entry's fourth byte is never read. __thiscall, one stack argument (`ret 4` at 0x1006aed6).
typedef void(__fastcall* PaletteToRGBQuads_t)(void* self, void* edx, unsigned char* out);
PaletteToRGBQuads_t PaletteToRGBQuads_orig;
void __fastcall PaletteToRGBQuads_re(void* self, void*, unsigned char* out) {
    const unsigned char* pal = static_cast<const unsigned char*>(self) + 0xc;
    for (int i = 0; i < 0x100; i++) {
        out[i * 4 + 2] = pal[i * 4 + 0];
        out[i * 4 + 1] = pal[i * 4 + 1];
        out[i * 4 + 0] = pal[i * 4 + 2];
        out[i * 4 + 3] = 0;
    }
}

// ----------------------------------------------------------------- libpng entry points (__cdecl)

// 0x000785f0  png_sig_cmp(sig, start, num_to_check): all three comparisons are UNSIGNED. num_to_check above 8 is
// clamped to 8 (jbe at 0x1007860c, store at 0x1007860e); otherwise a num_to_check below 1 returns 0 (jae at
// 0x1007861b, xor eax,eax at 0x1007861d). A start above 7 returns 0 (jbe at 0x10078625, xor eax,eax at
// 0x10078627). If start + num_to_check exceeds 8 (add at 0x1007862e, jbe at 0x10078634) the count becomes
// 8 - start (0x1007863b). The result is whatever the CRT memcmp at 0x100828b0 returns (call at 0x10078655) for
// sig+start against the module's 8-byte PNG signature at 0x10123198 + start (0x10078648).
typedef int(__cdecl* PngSigCmp_t)(const unsigned char*, unsigned, unsigned);
PngSigCmp_t PngSigCmp_orig;
int __cdecl PngSigCmp_re(const unsigned char* sig, unsigned start, unsigned num_to_check) {
    if (num_to_check > 8) num_to_check = 8;
    else if (num_to_check < 1) return 0;
    if (start > 7) return 0;
    if (start + num_to_check > 8) num_to_check = 8 - start;
    const unsigned char* png_sig = reinterpret_cast<const unsigned char*>(JgldBase() + 0x123198);
    Memcmp_t jgld_memcmp = reinterpret_cast<Memcmp_t>(JgldBase() + 0x828b0);
    return jgld_memcmp(sig + start, png_sig + start, num_to_check);
}

// 0x000787d0  png_calculate_crc(png_ptr, ptr, length): decides whether to fold the buffer into the running CRC.
// The local flag starts at 1 (0x100787e8). The byte at [png_ptr+0x11c] is masked with 0x20 (0x100787fa); when
// that bit is set (je at 0x100787ff selects the other arm) the dword at [png_ptr+0x6c] masked with 0x300 must
// equal 0x300 (0x10078807-0x1007880c, jne at 0x10078811) to clear the flag; when it is clear, bit 0x800 of the
// same dword (0x10078822, je at 0x1007882a) clears the flag. With the flag still set (cmp / je at
// 0x10078833-0x10078837) the dword at [png_ptr+0x110] is replaced by crc32(that dword, ptr, length) (call to
// 0x1009ca70 at 0x1007884b, store at 0x10078856). Nothing else is written and there is no return value.
typedef void(__cdecl* PngCalculateCrc_t)(void*, const unsigned char*, unsigned);
PngCalculateCrc_t PngCalculateCrc_orig;
void __cdecl PngCalculateCrc_re(void* png_ptr, const unsigned char* ptr, unsigned length) {
    char* png = static_cast<char*>(png_ptr);
    int need_crc = 1;
    if (*reinterpret_cast<unsigned char*>(png + 0x11c) & 0x20) {
        if ((*reinterpret_cast<unsigned*>(png + 0x6c) & 0x300) == 0x300) need_crc = 0;
    } else {
        if (*reinterpret_cast<unsigned*>(png + 0x6c) & 0x800) need_crc = 0;
    }
    if (need_crc) {
        Crc32_t jgld_crc32 = reinterpret_cast<Crc32_t>(JgldBase() + 0x9ca70);
        unsigned* crc = reinterpret_cast<unsigned*>(png + 0x110);
        *crc = jgld_crc32(*crc, ptr, length);
    }
}

// 0x0007dad0  png_set_bKGD(png_ptr, info_ptr, background): returns when either pointer is null (cmp/je at
// 0x1007dae8-0x1007daec and cmp/jne at 0x1007daee-0x1007daf2, both reaching the jmp at 0x1007daf4). Otherwise it
// copies 10 bytes from background to info_ptr+0x5a through the CRT memcpy at 0x1007f3a0 (push 0xa at 0x1007daf6,
// call at 0x1007db03) and sets bit 5 of the dword at [info_ptr+8] (`or al,0x20` at 0x1007db11, the whole dword
// stored back at 0x1007db16). png_ptr is only tested, never read.
typedef void(__cdecl* PngSetBkgd_t)(void*, void*, const void*);
PngSetBkgd_t PngSetBkgd_orig;
void __cdecl PngSetBkgd_re(void* png_ptr, void* info_ptr, const void* background) {
    if (png_ptr == 0 || info_ptr == 0) return;
    char* info = static_cast<char*>(info_ptr);
    JgldMemcpy()(info + 0x5a, background, 0xa);
    *reinterpret_cast<unsigned*>(info + 8) |= 0x20u;
}

// 0x0007e400  png_set_tIME(png_ptr, info_ptr, mod_time): three guards share one exit (the jmp at 0x1007e434):
// png_ptr null (je at 0x1007e41c), info_ptr null (je at 0x1007e422) and bit 0x10000 set in the dword at
// [png_ptr+0x6c] (and at 0x1007e42a, je at 0x1007e432 selects the body). Otherwise 8 bytes are copied from
// mod_time to info_ptr+0x3c through the CRT memcpy at 0x1007f3a0 (push 8 at 0x1007e436, call at 0x1007e443) and
// bit 9 of the dword at [info_ptr+8] is set (`or dh,2` at 0x1007e451, dword stored at 0x1007e457).
typedef void(__cdecl* PngSetTime_t)(void*, void*, const void*);
PngSetTime_t PngSetTime_orig;
void __cdecl PngSetTime_re(void* png_ptr, void* info_ptr, const void* mod_time) {
    if (png_ptr == 0 || info_ptr == 0) return;
    if (*reinterpret_cast<unsigned*>(static_cast<char*>(png_ptr) + 0x6c) & 0x10000u) return;
    char* info = static_cast<char*>(info_ptr);
    JgldMemcpy()(info + 0x3c, mod_time, 8);
    *reinterpret_cast<unsigned*>(info + 8) |= 0x200u;
}

// 0x0007e470  png_set_tRNS(png_ptr, info_ptr, trans, num_trans, trans_values): returns when either pointer is
// null (je at 0x1007e48c, jne at 0x1007e492, jmp at 0x1007e494). A non-null trans (cmp/je at
// 0x1007e496-0x1007e49a) is stored at [info_ptr+0x4c] (0x1007e4a2). A non-null trans_values (cmp/je at
// 0x1007e4a5-0x1007e4a9) causes 10 bytes to be copied to info_ptr+0x50 through the CRT memcpy at 0x1007f3a0
// (push 0xa at 0x1007e4ab, call at 0x1007e4b8) and, only on that path, a num_trans of 0 is replaced by 1
// (cmp/jne at 0x1007e4c0-0x1007e4c4, store at 0x1007e4c6). The low 16 bits of num_trans are then written as a
// word at [info_ptr+0x16] (0x1007e4d0, 0x1007e4d4) and bit 4 of the dword at [info_ptr+8] is set (`or ecx,0x10`
// at 0x1007e4de, stored at 0x1007e4e4). Those last two writes happen on every non-null path.
typedef void(__cdecl* PngSetTrns_t)(void*, void*, void*, int, const void*);
PngSetTrns_t PngSetTrns_orig;
void __cdecl PngSetTrns_re(void* png_ptr, void* info_ptr, void* trans, int num_trans, const void* trans_values) {
    if (png_ptr == 0 || info_ptr == 0) return;
    char* info = static_cast<char*>(info_ptr);
    if (trans != 0) *reinterpret_cast<void**>(info + 0x4c) = trans;
    if (trans_values != 0) {
        JgldMemcpy()(info + 0x50, trans_values, 0xa);
        if (num_trans == 0) num_trans = 1;
    }
    *reinterpret_cast<unsigned short*>(info + 0x16) = (unsigned short)num_trans;
    *reinterpret_cast<unsigned*>(info + 8) |= 0x10u;
}

// 0x0006f360  png_set_strip_alpha(png_ptr): one read-modify-write with no guard at all -- the dword at
// [png_ptr+0x70] is loaded (0x1006f37b), OR'd with 0x40000 (0x1006f37e) and stored back (0x1006f387). No branch,
// no return value, and a null png_ptr would fault exactly as the original does.
typedef void(__cdecl* PngSetStripAlpha_t)(void*);
PngSetStripAlpha_t PngSetStripAlpha_orig;
void __cdecl PngSetStripAlpha_re(void* png_ptr) {
    *reinterpret_cast<unsigned*>(static_cast<char*>(png_ptr) + 0x70) |= 0x40000u;
}

// 0x00079340  png_set_error_fn(png_ptr, error_ptr, error_fn, warning_fn): three unguarded stores, in the order
// the body uses: error_ptr at [png_ptr+0x48] (0x1007935e), error_fn at [png_ptr+0x40] (0x10079367) and
// warning_fn at [png_ptr+0x44] (0x10079370). No branch, no return value.
typedef void(__cdecl* PngSetErrorFn_t)(void*, void*, void*, void*);
PngSetErrorFn_t PngSetErrorFn_orig;
void __cdecl PngSetErrorFn_re(void* png_ptr, void* error_ptr, void* error_fn, void* warning_fn) {
    char* png = static_cast<char*>(png_ptr);
    *reinterpret_cast<void**>(png + 0x48) = error_ptr;
    *reinterpret_cast<void**>(png + 0x40) = error_fn;
    *reinterpret_cast<void**>(png + 0x44) = warning_fn;
}

// 0x00078ad0  png_init_io(png_ptr, fp): one unguarded store of the second argument at [png_ptr+0x54]
// (loaded 0x10078aeb, stored 0x10078aee). No branch, no return value.
typedef void(__cdecl* PngInitIo_t)(void*, void*);
PngInitIo_t PngInitIo_orig;
void __cdecl PngInitIo_re(void* png_ptr, void* fp) {
    *reinterpret_cast<void**>(static_cast<char*>(png_ptr) + 0x54) = fp;
}

}  // namespace

SG_HOOK("jgld.dll", 0x0006d900, png_do_invert, PngDoInvert_re, PngDoInvert_orig);
SG_HOOK("jgld.dll", 0x0006d980, png_do_swap, PngDoSwap_re, PngDoSwap_orig);
SG_HOOK("jgld.dll", 0x0006da10, png_do_packswap, PngDoPackswap_re, PngDoPackswap_orig);
SG_HOOK("jgld.dll", 0x00072430, png_do_chop, PngDoChop_re, PngDoChop_orig);
SG_HOOK("jgld.dll", 0x0000af30, fillWords, FillWords_re, FillWords_orig);
SG_HOOK("jgld.dll", 0x0006ae50, Palette_toRGBQuads, PaletteToRGBQuads_re, PaletteToRGBQuads_orig);
SG_HOOK("jgld.dll", 0x000785f0, png_sig_cmp, PngSigCmp_re, PngSigCmp_orig);
SG_HOOK("jgld.dll", 0x000787d0, png_calculate_crc, PngCalculateCrc_re, PngCalculateCrc_orig);
SG_HOOK("jgld.dll", 0x0007dad0, png_set_bKGD, PngSetBkgd_re, PngSetBkgd_orig);
SG_HOOK("jgld.dll", 0x0007e400, png_set_tIME, PngSetTime_re, PngSetTime_orig);
SG_HOOK("jgld.dll", 0x0007e470, png_set_tRNS, PngSetTrns_re, PngSetTrns_orig);
SG_HOOK("jgld.dll", 0x0006f360, png_set_strip_alpha, PngSetStripAlpha_re, PngSetStripAlpha_orig);
SG_HOOK("jgld.dll", 0x00079340, png_set_error_fn, PngSetErrorFn_re, PngSetErrorFn_orig);
SG_HOOK("jgld.dll", 0x00078ad0, png_init_io, PngInitIo_re, PngInitIo_orig);

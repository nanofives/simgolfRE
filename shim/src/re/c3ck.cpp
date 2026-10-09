// Batch c3ck: jgld.dll - libpng 1.0.5's chunk readers, the Palette methods reached through the Palette
// vtable, and the window procedure the whole window-message family hangs off.
//   png_handle_IHDR        0x000796e0      Palette::setDefault     0x0006a600
//   png_handle_IEND        0x00079c20      Palette::setRGB         0x0006a9a0
//   png_handle_pHYs        0x0007b380      Palette::getRGBRange    0x0006a8e0
//   png_handle_oFFs        0x0007b4d0      Palette::getColor       0x0006add0
//   png_handle_tIME        0x0007b9b0      windowProc              0x000692b0
//   png_handle_sBIT        0x00079ec0
//   png_handle_unknown     0x0007c150
//
// SG_HOOK addresses are RVAs; every instruction cited in a comment is a VA (0x10000000 + RVA), the form
// the debug build's disassembly prints. jgld.dll is LoadLibrary'd after the shim starts, so these hooks
// install from the LoadLibraryA hook (SgHooksInstallPending, re/hooks.cpp) and diff_hook finds them by
// (module, rva) through SimGolfShim_FindHookM. The module is relocated at run time (the probe
// log/c3/alt/c3ck_probe.py read base 0x038f0000 in one boot), so every address this file reaches is
// resolved against the running base through JgldVA. __thiscall is emulated with __fastcall: ecx = this,
// edx unused.
//
// WHY THE SEVEN CHUNK READERS CAN BE A/B'd. libpng reads bytes only through the png_struct's
// read_data_fn at +0x50 (the slot png_set_read_fn 0x10078cb0 writes at 0x10078cdd) and reports warnings
// only through the handler at +0x44, and the png_struct and the png_info are both the CALLER's. The
// fixture (re/frida/js/fixtures.d/c3ck.js) builds both out of its own memory, puts c3ch's 35-byte
// recording reader in +0x50 and c3bq's one-byte `ret` in +0x44, and feeds real chunk bytes whose closing
// CRC is the real crc32 of what the reader will have served (log/c3/c3ck_gen.py). None of the seven
// allocates: png_malloc is only reached by the handlers this batch does not take (PLTE, hIST, tRNS,
// tEXt, zTXt), which is why those stay out (log/c3/c3ck_notes.txt).
//
// WHAT AN A/B CANNOT FOLLOW HERE. png_error 0x10078fe0 calls the handler at png_struct+0x40 and then
// falls into png_default_error 0x100792b0 whatever that handler does (0x1007901b is not guarded), and
// png_default_error longjmps through the png_struct's jmpbuf; png_chunk_error 0x100790a0 reaches the
// same place. Every arm of these seven bodies that ends in one of those two is listed in
// log/c3/c3ck_purpose.md and no vector takes it. The bodies still implement those arms, so a caller that
// installed a returning handler would see the same control flow.
//
// WHY THE FOUR PALETTE METHODS CAN BE A/B'd. They are plain writers over the palette object the caller
// owns; the fixture allocates its own palette blocks and puts jgld's own Palette vtable VA 0x1011db10 at
// +0, so the virtual call setDefault makes (slot 2, 0x1006a627) reaches the real Palette::release
// 0x1006a380. Three of the four touch no global at all. getColor 0x1006add0 writes the three module
// bytes at VA 0x10128734 and returns their address; the running game enters it (30 times in 20 s at the
// menu, log/c3/alt/c3ck_probe.txt, all on the process's first thread), so its key runs with that thread
// suspended.
//
// WHY windowProc CAN BE A/B'd. Its only non-local reads are two module globals - the event sink pointer
// VA 0x1012872c (message 0x401, 0x100693c2) and, through onKey 0x10069c70, the display pointer VA
// 0x10128420 - plus the command handler VA 0x10128728 that onCommand 0x1006a0e0 calls. The fixture puts
// recording objects of its own in all three with the first thread suspended and writes the real values
// back when the key ends (c3cd's pattern). The window handle every vector passes is the invalid 0x1, so
// DefWindowProcA, BeginPaint, InvalidateRect, GetDC and PeekMessageA all fail without touching the
// game's window; the probe called DefWindowProcA(1, msg, ...) twice for each message the default arm is
// given and got 0 both times every time.
#include "hooks.h"
#include <string.h>

namespace {

// jgld.dll is relocatable, so an address the code calls, stores or reads is resolved against the
// running base (c3cb's JgldVA; a hard-coded 0x10xxxxxx would read another module).
unsigned char* JgldBase() {
    static unsigned char* base = 0;
    if (!base) base = reinterpret_cast<unsigned char*>(GetModuleHandleA("jgld.dll"));
    return base;
}
void* JgldVA(unsigned va) { return JgldBase() + (va - 0x10000000u); }

inline unsigned& U32(void* p, unsigned off) {
    return *reinterpret_cast<unsigned*>(static_cast<char*>(p) + off);
}
inline unsigned U32R(const void* p, unsigned off) {
    return *reinterpret_cast<const unsigned*>(static_cast<const char*>(p) + off);
}
inline unsigned char& U8(void* p, unsigned off) {
    return *reinterpret_cast<unsigned char*>(static_cast<char*>(p) + off);
}
inline unsigned char U8R(const void* p, unsigned off) {
    return *reinterpret_cast<const unsigned char*>(static_cast<const char*>(p) + off);
}
inline void* P32R(const void* p, unsigned off) {
    return *reinterpret_cast<void* const*>(static_cast<const char*>(p) + off);
}

// ================================================= libpng 1.0.5: the chunk readers ==================
//
// png_struct fields the seven use: error_fn +0x40, warning_fn +0x44, read_data_fn +0x50, mode +0x68,
// flags +0x6c, zbuf +0xac, zbuf_size +0xb0, width +0xc8, height +0xcc, rowbytes +0xd8,
// interlaced +0x123, color_type +0x126, bit_depth +0x127, pixel_depth +0x129, channels +0x12a,
// chunk_name[4] +0x11c, sig_bit (png_color_8: red, green, blue, gray, alpha) +0x17c..+0x180.
// png_info fields: valid +0x08, rowbytes +0x0c, bit_depth +0x18, color_type +0x19, compression +0x1a,
// filter +0x1b, interlace +0x1c, channels +0x1d, pixel_depth +0x1e, mod_time +0x3c, sig_bit +0x44,
// x_offset +0x64, y_offset +0x68, offset_unit +0x6c, x_pixels_per_unit +0x70, y_pixels_per_unit +0x74,
// phys_unit +0x78 (the last six read off png_set_oFFs 0x1007ddb0 and png_set_pHYs 0x1007dfc0).
const unsigned PNG_MODE = 0x68, PNG_COLOR_TYPE = 0x126, PNG_BIT_DEPTH = 0x127;
const unsigned PNG_PIXEL_DEPTH = 0x129, PNG_CHANNELS = 0x12A, PNG_CHUNK_NAME = 0x11C;
const unsigned PNG_WIDTH = 0xC8, PNG_HEIGHT = 0xCC, PNG_ROWBYTES = 0xD8, PNG_INTERLACED = 0x123;
const unsigned PNG_SIG_BIT = 0x17C;
const unsigned INFO_VALID = 0x08;

typedef void(__cdecl* PngMsg_t)(void* png_ptr, const char* message);
typedef void(__cdecl* PngCrcRead_t)(void* png_ptr, unsigned char* buf, unsigned length);
typedef int(__cdecl* PngCrcFinish_t)(void* png_ptr, unsigned skip);
typedef unsigned(__cdecl* PngGetUint32_t)(const unsigned char* buf);
typedef unsigned short(__cdecl* PngGetUint16_t)(const unsigned char* buf);
typedef void(__cdecl* PngCheckName_t)(void* png_ptr, const unsigned char* name);
typedef void(__cdecl* PngSetIHDR_t)(void* png_ptr, void* info_ptr, unsigned width, unsigned height,
                                    int bit_depth, int color_type, int interlace_type,
                                    int compression_type, int filter_type);
typedef void(__cdecl* PngSetUnit_t)(void* png_ptr, void* info_ptr, unsigned x, unsigned y, int unit);
typedef void(__cdecl* PngSetPtr_t)(void* png_ptr, void* info_ptr, const void* value);

const unsigned VA_PNG_ERROR = 0x10078FE0;
const unsigned VA_PNG_WARNING = 0x10079040;
const unsigned VA_PNG_CHUNK_ERROR = 0x100790A0;
const unsigned VA_PNG_CRC_READ = 0x100794B0;
const unsigned VA_PNG_CRC_FINISH = 0x10079510;
const unsigned VA_PNG_GET_UINT_32 = 0x100793B0;
const unsigned VA_PNG_GET_UINT_16 = 0x10079470;
const unsigned VA_PNG_CHECK_CHUNK_NAME = 0x1007C1F0;
const unsigned VA_PNG_SET_IHDR = 0x1007DC70;
const unsigned VA_PNG_SET_OFFS = 0x1007DDB0;
const unsigned VA_PNG_SET_PHYS = 0x1007DFC0;
const unsigned VA_PNG_SET_SBIT = 0x1007E070;
const unsigned VA_PNG_SET_TIME = 0x1007E400;

void pngError(void* png, unsigned msgVA) {
    reinterpret_cast<PngMsg_t>(JgldVA(VA_PNG_ERROR))(png, static_cast<const char*>(JgldVA(msgVA)));
}
void pngWarning(void* png, unsigned msgVA) {
    reinterpret_cast<PngMsg_t>(JgldVA(VA_PNG_WARNING))(png, static_cast<const char*>(JgldVA(msgVA)));
}
void pngChunkError(void* png, unsigned msgVA) {
    reinterpret_cast<PngMsg_t>(JgldVA(VA_PNG_CHUNK_ERROR))(png, static_cast<const char*>(JgldVA(msgVA)));
}
void crcRead(void* png, unsigned char* buf, unsigned n) {
    reinterpret_cast<PngCrcRead_t>(JgldVA(VA_PNG_CRC_READ))(png, buf, n);
}
int crcFinish(void* png, unsigned skip) {
    return reinterpret_cast<PngCrcFinish_t>(JgldVA(VA_PNG_CRC_FINISH))(png, skip);
}
unsigned getUint32(const unsigned char* buf) {
    return reinterpret_cast<PngGetUint32_t>(JgldVA(VA_PNG_GET_UINT_32))(buf);
}

typedef void(__cdecl* PngHandle_t)(void* png_ptr, void* info_ptr, unsigned length);

// ================================================================= png_handle_IHDR 0x000796e0
//
// void __cdecl(png_structp, png_infop, png_uint_32 length): png_ptr [ebp+8], info_ptr [ebp+0xc],
// length [ebp+0x10]; `ret` with no pop at 0x100799bd.
//
// The reader of the image header. It refuses a second IHDR (mode at +0x68 not zero, 0x100796fb) and any
// length but thirteen (0x10079712), both through png_error, then marks PNG_HAVE_IHDR by setting bit 0 of
// the mode (0x1007972f), reads the thirteen bytes through png_crc_read 0x100794b0 (0x10079742) and
// closes the chunk with png_crc_finish 0x10079510 called with a skip of zero (0x10079750), whose RESULT
// IT IGNORES - a CRC that disagrees only produces the warning png_crc_finish itself issues.
//
// The thirteen bytes are width (0x1007975c), height (0x1007976b), then five single bytes the body reads
// as DWORDS masked with 0xff (0x10079776 through 0x100797ad), so the last read, at buf+12, touches three
// bytes past the chunk: bit_depth buf[8], color_type buf[9], compression buf[10], filter buf[11],
// interlace buf[12].
//
// Seven validity tests follow, each ending in png_error: zero or negative-as-signed width or height
// (0x100797b0-0x100797cc), a bit depth outside {1,2,4,8,16} (0x100797df-0x100797fb), a colour type that
// is negative, 1, 5 or above 6 (0x1007980e-0x10079824), the two forbidden colour-type/bit-depth
// combinations (0x10079837-0x10079859: palette above 8 bits, or truecolour/alpha below 8), an interlace
// method of 2 or more (0x1006986c), a non-zero compression method (0x10079883) and a non-zero filter
// method (0x1007989a).
//
// What survives is written into the png_struct: width +0xc8 (0x100798b7), height +0xcc (0x100798c3),
// bit_depth +0x127 (0x100798cf), interlaced +0x123 (0x100798db), color_type +0x126 (0x100798e7). The
// channel count is then derived from the byte just stored at +0x126, re-read rather than reused
// (0x100798f2), through the seven-entry jump table at 0x100799be: greyscale 0 and palette 3 give 1
// (0x1007990b), truecolour 2 gives 3 (0x10079917), grey+alpha 4 gives 2 (0x10079923) and RGBA 6 gives 4
// (0x1007992f); slots 1 and 5 and the above-6 arm (0x100798ff) leave +0x12a as it was, and no input
// reaches them because the colour-type test above already rejected those values. pixel_depth +0x129 is
// the LOW BYTE of bit_depth * channels (0x1007994f, 0x10079955) and rowbytes +0xd8 is
// (width * pixel_depth + 7) >> 3 with an unsigned shift (0x1007996f-0x1007997b), both computed from the
// bytes just stored and not from the locals.
//
// Finally png_set_IHDR 0x1007dc70 copies the same nine values into the png_info (0x100799a5), pushed
// filter, compression, interlace, color_type, bit_depth, height, width, info_ptr, png_ptr
// (0x10079981-0x100799a4), so the setter receives them in the order
// (png, info, width, height, bit_depth, color_type, interlace, compression, filter).
PngHandle_t PngHandleIHDR_orig;

void __cdecl PngHandleIHDR_re(void* png, void* info, unsigned length) {
    unsigned char buf[16];
    if (U32R(png, PNG_MODE) != 0)                                        // 0x100796fb
        pngError(png, 0x1011E204);                                       // 0x1007970a
    if (length != 13)                                                    // 0x10079712
        pngError(png, 0x1011E1F0);                                       // 0x10079721
    U32(png, PNG_MODE) = U32R(png, PNG_MODE) | 1;                        // 0x1007972f
    crcRead(png, buf, 13);                                               // 0x10079742
    crcFinish(png, 0);                                                   // 0x10079750, result ignored

    unsigned width = getUint32(buf);                                     // 0x1007975c
    unsigned height = getUint32(buf + 4);                                // 0x1007976b
    unsigned bit_depth = buf[8];                                         // 0x10079776
    unsigned color_type = buf[9];                                        // 0x10079782
    unsigned compression = buf[10];                                      // 0x1007978d
    unsigned filter = buf[11];                                           // 0x10079799
    unsigned interlace = buf[12];                                        // 0x100797a5

    if (width == 0 || width > 0x7FFFFFFFu ||                             // 0x100797b0, 0x100797b6
        height == 0 || height > 0x7FFFFFFFu)                             // 0x100797bf, 0x100797c5
        pngError(png, 0x1011E1D4);                                       // 0x100797d7
    if (bit_depth != 1 && bit_depth != 2 && bit_depth != 4 &&            // 0x100797df-0x100797ef
        bit_depth != 8 && bit_depth != 16)                               // 0x100797f1, 0x100797f7
        pngError(png, 0x1011E1B8);                                       // 0x10079806
    if (static_cast<int>(color_type) < 0 || color_type == 1 ||           // 0x1007980e, 0x10079814
        color_type == 5 || static_cast<int>(color_type) > 6)             // 0x1007981a, 0x10079820
        pngError(png, 0x1011E19C);                                       // 0x1007982f
    if ((color_type == 3 && static_cast<int>(bit_depth) > 8) ||          // 0x10079837, 0x1007983d
        ((color_type == 2 || color_type == 4 || color_type == 6) &&      // 0x10079843-0x1007984f
         static_cast<int>(bit_depth) < 8))                               // 0x10079855
        pngError(png, 0x1011E168);                                       // 0x10079864
    if (static_cast<int>(interlace) >= 2)                                // 0x1006986c
        pngError(png, 0x1011E144);                                       // 0x1007987b
    if (compression != 0)                                                // 0x10079883
        pngError(png, 0x1011E120);                                       // 0x10079892
    if (filter != 0)                                                     // 0x1007989a
        pngError(png, 0x1011E100);                                       // 0x100798a9

    U32(png, PNG_WIDTH) = width;                                         // 0x100798b7
    U32(png, PNG_HEIGHT) = height;                                       // 0x100798c3
    U8(png, PNG_BIT_DEPTH) = static_cast<unsigned char>(bit_depth);      // 0x100798cf
    U8(png, PNG_INTERLACED) = static_cast<unsigned char>(interlace);     // 0x100798db
    U8(png, PNG_COLOR_TYPE) = static_cast<unsigned char>(color_type);    // 0x100798e7
    switch (U8R(png, PNG_COLOR_TYPE)) {                                  // 0x100798f2, table 0x100799be
    case 0:
    case 3:
        U8(png, PNG_CHANNELS) = 1;                                       // 0x1007990e
        break;
    case 2:
        U8(png, PNG_CHANNELS) = 3;                                       // 0x1007991a
        break;
    case 4:
        U8(png, PNG_CHANNELS) = 2;                                       // 0x10079926
        break;
    case 6:
        U8(png, PNG_CHANNELS) = 4;                                       // 0x10079932
        break;
    default:
        break;                                                           // 0x100798ff / slots 1 and 5
    }
    U8(png, PNG_PIXEL_DEPTH) = static_cast<unsigned char>(
        U8R(png, PNG_BIT_DEPTH) * U8R(png, PNG_CHANNELS));               // 0x1007994f, 0x10079955
    U32(png, PNG_ROWBYTES) =
        (U32R(png, PNG_WIDTH) * U8R(png, PNG_PIXEL_DEPTH) + 7) >> 3;     // 0x1007996f-0x1007997b

    reinterpret_cast<PngSetIHDR_t>(JgldVA(VA_PNG_SET_IHDR))(             // 0x100799a5
        png, info, width, height, static_cast<int>(bit_depth), static_cast<int>(color_type),
        static_cast<int>(interlace), static_cast<int>(compression), static_cast<int>(filter));
}

// ================================================================= png_handle_IEND 0x00079c20
//
// void __cdecl(png_structp, png_infop, png_uint_32 length); `ret` with no pop at 0x10079cb0.
//
// The end-of-stream chunk. It demands that BOTH PNG_HAVE_IHDR (bit 0) and PNG_HAVE_IDAT (bit 2) are
// already set in the mode at +0x68 (0x10079c3b and 0x10079c4b) and calls png_error 0x10078fe0 otherwise
// (0x10079c5b); the `cmp [ebp+0xc], 0` at 0x10079c63 that follows the error call is the compiler's
// short-circuit tail and makes the whole body conditional on the info_ptr if png_error ever returned.
//
// It then sets bits 3 and 4 of the mode at once with `or al, 0x18` on the WHOLE mode dword, so the upper
// three bytes survive (0x10079c71, 0x10079c76), warns through png_warning 0x10079040 when the chunk
// carries any payload at all (0x10079c79, 0x10079c88) - IEND is defined to be empty - and hands that
// same length to png_crc_finish 0x10079510 (0x10079c98), which consumes it in zbuf-sized pieces and then
// checks the four CRC bytes. The warning does not stop the consumption.
PngHandle_t PngHandleIEND_orig;

void __cdecl PngHandleIEND_re(void* png, void* info, unsigned length) {
    if (!((U32R(png, PNG_MODE) & 1) && (U32R(png, PNG_MODE) & 4))) {     // 0x10079c3b, 0x10079c4b
        pngError(png, 0x1011E2C0);                                       // 0x10079c5b
        if (info == 0) return;                                           // 0x10079c63, 0x10079c69
    }
    U32(png, PNG_MODE) = U32R(png, PNG_MODE) | 0x18;                     // 0x10079c71, 0x10079c76
    if (length != 0)                                                     // 0x10079c79
        pngWarning(png, 0x1011E2A4);                                     // 0x10079c88
    crcFinish(png, length);                                              // 0x10079c98
}

// ================================================================= png_handle_pHYs 0x0007b380
//
// void __cdecl(png_structp, png_infop, png_uint_32 length); `ret` with no pop at 0x1007b4ca.
//
// The physical-pixel-size chunk. Three guards share one shape - warn, consume the whole chunk through
// png_crc_finish 0x10079510 and return - and the first does not: without PNG_HAVE_IHDR (bit 0 of the
// mode at +0x68, 0x1007b39e) it calls png_error 0x10078fe0 (0x1007b3ae) and, were that to return, jumps
// straight to the length test (0x1007b3b6) rather than returning. After PNG_HAVE_IDAT (bit 2,
// 0x1007b3be) the chunk is out of place (0x1007b3ce, 0x1007b3de); a png_info that already carries the
// pHYs valid bit 0x80 at +8 makes it a duplicate (0x1007b3eb, 0x1007b3f7, 0x1007b409, 0x1007b419); a
// null info_ptr skips the duplicate test altogether (0x1007b3ef).
//
// A payload that is not exactly nine bytes is refused the same way (0x1007b426, 0x1007b435, 0x1007b445).
// Nine bytes are read through png_crc_read 0x100794b0 (0x1007b459) and the chunk closed with a skip of
// zero (0x1007b467); THIS caller does test png_crc_finish's result and drops the chunk when the CRC
// disagreed (0x1007b46f, 0x1007b471). The nine bytes are two big-endian 32-bit values (0x1007b479,
// 0x1007b488) and a unit byte the body reads as a dword masked with 0xff (0x1007b496), which are handed
// to png_set_pHYs 0x1007dfc0 (0x1007b4b2); that setter writes them at info +0x70, +0x74 and +0x78 and
// ignores a null png_ptr or info_ptr.
PngHandle_t PngHandlePHYs_orig;

void __cdecl PngHandlePHYs_re(void* png, void* info, unsigned length) {
    unsigned char buf[12];
    if ((U32R(png, PNG_MODE) & 1) == 0) {                                // 0x1007b39e
        pngError(png, 0x1011E888);                                       // 0x1007b3ae
        goto length_test;                                                // 0x1007b3b6
    }
    if (U32R(png, PNG_MODE) & 4) {                                       // 0x1007b3be
        pngWarning(png, 0x1011E870);                                     // 0x1007b3ce
        crcFinish(png, length);                                          // 0x1007b3de
        return;
    }
    if (info != 0 && (U32R(info, INFO_VALID) & 0x80)) {                  // 0x1007b3eb, 0x1007b3f7
        pngWarning(png, 0x1011E858);                                     // 0x1007b409
        crcFinish(png, length);                                          // 0x1007b419
        return;
    }
length_test:
    if (length != 9) {                                                   // 0x1007b426
        pngWarning(png, 0x1011E83C);                                     // 0x1007b435
        crcFinish(png, length);                                          // 0x1007b445
        return;
    }
    crcRead(png, buf, 9);                                                // 0x1007b459
    if (crcFinish(png, 0)) return;                                       // 0x1007b467, 0x1007b471
    {
        unsigned res_x = getUint32(buf);                                 // 0x1007b479
        unsigned res_y = getUint32(buf + 4);                             // 0x1007b488
        unsigned unit = buf[8];                                          // 0x1007b496
        reinterpret_cast<PngSetUnit_t>(JgldVA(VA_PNG_SET_PHYS))(         // 0x1007b4b2
            png, info, res_x, res_y, static_cast<int>(unit));
    }
}

// ================================================================= png_handle_oFFs 0x0007b4d0
//
// void __cdecl(png_structp, png_infop, png_uint_32 length); `ret` with no pop at 0x1007b61a.
//
// The image-offset chunk, instruction for instruction the same shape as png_handle_pHYs above, with
// three differences: the duplicate test reads the oFFs valid bit 0x100 instead of 0x80 (0x1007b547),
// the four message strings are its own (0x1011e8f0 at 0x1007b4fe, 0x1011e8d8 at 0x1007b51e, 0x1011e8c0
// at 0x1007b559, 0x1011e8a4 at 0x1007b585) and the nine bytes go to png_set_oFFs 0x1007ddb0
// (0x1007b602), which writes them at info +0x64, +0x68 and +0x6c. The guards are at 0x1007b4ee (no
// IHDR), 0x1007b50e (after IDAT), 0x1007b53b and 0x1007b547 (duplicate) and 0x1007b576 (length), the
// read at 0x1007b5a9, the close at 0x1007b5b7 and its test at 0x1007b5bf, and the two big-endian values
// at 0x1007b5c9 and 0x1007b5d8.
PngHandle_t PngHandleOFFs_orig;

void __cdecl PngHandleOFFs_re(void* png, void* info, unsigned length) {
    unsigned char buf[12];
    if ((U32R(png, PNG_MODE) & 1) == 0) {                                // 0x1007b4ee
        pngError(png, 0x1011E8F0);                                       // 0x1007b4fe
        goto length_test;                                                // 0x1007b506
    }
    if (U32R(png, PNG_MODE) & 4) {                                       // 0x1007b50e
        pngWarning(png, 0x1011E8D8);                                     // 0x1007b51e
        crcFinish(png, length);                                          // 0x1007b52e
        return;
    }
    if (info != 0 && (U32R(info, INFO_VALID) & 0x100)) {                 // 0x1007b53b, 0x1007b547
        pngWarning(png, 0x1011E8C0);                                     // 0x1007b559
        crcFinish(png, length);                                          // 0x1007b569
        return;
    }
length_test:
    if (length != 9) {                                                   // 0x1007b576
        pngWarning(png, 0x1011E8A4);                                     // 0x1007b585
        crcFinish(png, length);                                          // 0x1007b595
        return;
    }
    crcRead(png, buf, 9);                                                // 0x1007b5a9
    if (crcFinish(png, 0)) return;                                       // 0x1007b5b7, 0x1007b5bf
    {
        unsigned off_x = getUint32(buf);                                 // 0x1007b5c9
        unsigned off_y = getUint32(buf + 4);                             // 0x1007b5d8
        unsigned unit = buf[8];                                          // 0x1007b5e6
        reinterpret_cast<PngSetUnit_t>(JgldVA(VA_PNG_SET_OFFS))(         // 0x1007b602
            png, info, off_x, off_y, static_cast<int>(unit));
    }
}

// ================================================================= png_handle_tIME 0x0007b9b0
//
// void __cdecl(png_structp, png_infop, png_uint_32 length); `ret` with no pop at 0x1007bade.
//
// The modification-time chunk. Without PNG_HAVE_IHDR it calls png_error 0x10078fe0 (0x1007b9c6,
// 0x1007b9d8) and, were that to return, continues at the PNG_HAVE_IDAT test rather than returning
// (0x1007b9e3); a png_info that already carries the tIME valid bit 0x200 at +8 makes it a duplicate and
// the chunk is consumed and dropped (0x1007b9e8, 0x1007b9f4, 0x1007ba06, 0x1007ba16). Unlike pHYs and
// oFFs this reader does NOT refuse the chunk after IDAT: it only records that it arrived there, by
// setting bit 3 of the mode (0x1007ba24-0x1007ba3d).
//
// Seven bytes are the defined payload (0x1007ba40); any other length warns and is consumed
// (0x1007ba4b, 0x1007ba5b). The seven are read at 0x1007ba6f and the chunk closed with a skip of zero at
// 0x1007ba7d, whose result is tested (0x1007ba8a). The png_time the setter is given is built on the
// frame in the order second, minute, hour, day, month (0x1007ba8f-0x1007baa7, copying buf[6] down to
// buf[2] into t+6 down to t+2) and finally the year through png_get_uint_16 0x10079470
// (0x1007baae, 0x1007bab6), so its layout is year at +0 as a 16-bit value, then month, day, hour,
// minute, second. png_set_tIME 0x1007e400 copies EIGHT bytes of it (`push 8` at 0x1007e436), so the
// eighth byte the structure carries into the png_info is the /GZ frame fill 0xcc the prologue left at
// 0x1007b9c0 and nothing ever writes - reproduced here, because it lands in the info at +0x43.
PngHandle_t PngHandleTIME_orig;

void __cdecl PngHandleTIME_re(void* png, void* info, unsigned length) {
    unsigned char buf[8];
    unsigned char t[8];
    memset(t, 0xCC, sizeof(t));                                          // the /GZ fill, 0x1007b9c0
    if ((U32R(png, PNG_MODE) & 1) == 0) {                                // 0x1007b9c6
        pngError(png, 0x1011E9F8);                                       // 0x1007b9d8
        goto after_duplicate;                                            // 0x1007b9e3
    }
    if (info != 0 && (U32R(info, INFO_VALID) & 0x200)) {                 // 0x1007b9e8, 0x1007b9f4
        pngWarning(png, 0x1011E9E0);                                     // 0x1007ba06
        crcFinish(png, length);                                          // 0x1007ba16
        return;
    }
after_duplicate:
    if (U32R(png, PNG_MODE) & 4)                                         // 0x1007ba2a
        U32(png, PNG_MODE) = U32R(png, PNG_MODE) | 8;                    // 0x1007ba34, 0x1007ba3d
    if (length != 7) {                                                   // 0x1007ba40
        pngWarning(png, 0x1011E9C4);                                     // 0x1007ba4b
        crcFinish(png, length);                                          // 0x1007ba5b
        return;
    }
    crcRead(png, buf, 7);                                                // 0x1007ba6f
    if (crcFinish(png, 0)) return;                                       // 0x1007ba7d, 0x1007ba8a
    t[6] = buf[6];                                                       // 0x1007ba8f
    t[5] = buf[5];                                                       // 0x1007ba95
    t[4] = buf[4];                                                       // 0x1007ba9b
    t[3] = buf[3];                                                       // 0x1007baa1
    t[2] = buf[2];                                                       // 0x1007baa7
    *reinterpret_cast<unsigned short*>(t) =
        reinterpret_cast<PngGetUint16_t>(JgldVA(VA_PNG_GET_UINT_16))(buf);   // 0x1007baae
    reinterpret_cast<PngSetPtr_t>(JgldVA(VA_PNG_SET_TIME))(png, info, t);    // 0x1007bac9
}

// ================================================================= png_handle_sBIT 0x00079ec0
//
// void __cdecl(png_structp, png_infop, png_uint_32 length); `ret` with no pop at 0x1007a0c6.
//
// The significant-bits chunk. Its four-byte scratch is zeroed before anything else, one byte at a time
// from the top down (0x10079ed8-0x10079eeb), so a payload shorter than four bytes leaves zeros behind
// rather than frame fill. Without PNG_HAVE_IHDR it calls png_error 0x10078fe0 (0x10079ef4, 0x10079f04)
// and would continue at the length test; after PNG_HAVE_IDAT the chunk is consumed and dropped
// (0x10079f17, 0x10079f27, 0x10079f37). With PNG_HAVE_PLTE (bit 1 of the mode, 0x10079f4a) it only
// WARNS and carries on (0x10079f5a, then 0x10079f62 jumps past the duplicate test); the duplicate test
// - the sBIT valid bit 2 at info+8 - is therefore reachable only before the palette (0x10079f64,
// 0x10079f70, 0x10079f80, 0x10079f90).
//
// The expected length is three for a palette image (colour type 3 at +0x126, 0x10079fa8, 0x10079fad) and
// otherwise the channel count at +0x12a (0x10079fbb), so it is 1, 2, 3 or 4; any other payload length
// warns and is consumed (0x10079fc7, 0x10079fd5, 0x10079fe5). The bytes are read at 0x10079ffe and the
// chunk closed with a skip of zero at 0x1007a00c, whose result is tested (0x1007a014).
//
// The five bytes of the png_color_8 at png+0x17c (red, green, blue, gray, alpha) are then filled two
// different ways, chosen by bit 1 of the colour type (0x1007a028). For a colour image red, green and
// blue take the first three payload bytes and alpha the fourth (0x1007a035, 0x1007a041, 0x1007a04d,
// 0x1007a059), and the gray byte at +0x17f is left as it was. For a greyscale image the gray byte is
// written first and red, green and blue all take the SAME first payload byte (0x1007a067, 0x1007a073,
// 0x1007a07f, 0x1007a08b) with alpha taking the second (0x1007a097). png_set_sBIT 0x1007e070 then copies
// five bytes of that field, in place in the png_struct, to info+0x44 (0x1007a0ae).
PngHandle_t PngHandleSBIT_orig;

void __cdecl PngHandleSBIT_re(void* png, void* info, unsigned length) {
    unsigned char buf[4];
    unsigned truelen;
    buf[3] = 0;                                                          // 0x10079ed8
    buf[2] = buf[3];                                                     // 0x10079edf
    buf[1] = buf[2];                                                     // 0x10079ee5
    buf[0] = buf[1];                                                     // 0x10079eeb
    if ((U32R(png, PNG_MODE) & 1) == 0) {                                // 0x10079ef4
        pngError(png, 0x1011E414);                                       // 0x10079f04
        goto length_test;                                                // 0x10079f0c
    }
    if (U32R(png, PNG_MODE) & 4) {                                       // 0x10079f17
        pngWarning(png, 0x1011E3FC);                                     // 0x10079f27
        crcFinish(png, length);                                          // 0x10079f37
        return;
    }
    if (U32R(png, PNG_MODE) & 2) {                                       // 0x10079f4a
        pngWarning(png, 0x1011E3E4);                                     // 0x10079f5a
    } else if (info != 0 && (U32R(info, INFO_VALID) & 2)) {              // 0x10079f64, 0x10079f70
        pngWarning(png, 0x1011E3CC);                                     // 0x10079f80
        crcFinish(png, length);                                          // 0x10079f90
        return;
    }
length_test:
    truelen = (U8R(png, PNG_COLOR_TYPE) == 3) ? 3u                       // 0x10079fa8, 0x10079fad
                                              : U8R(png, PNG_CHANNELS);  // 0x10079fbb
    if (length != truelen) {                                             // 0x10079fc7
        pngWarning(png, 0x1011E3B0);                                     // 0x10079fd5
        crcFinish(png, length);                                          // 0x10079fe5
        return;
    }
    crcRead(png, buf, truelen);                                          // 0x10079ffe
    if (crcFinish(png, 0)) return;                                       // 0x1007a00c, 0x1007a014
    if (U8R(png, PNG_COLOR_TYPE) & 2) {                                  // 0x1007a028
        U8(png, PNG_SIG_BIT + 0) = buf[0];                               // 0x1007a035  red
        U8(png, PNG_SIG_BIT + 1) = buf[1];                               // 0x1007a041  green
        U8(png, PNG_SIG_BIT + 2) = buf[2];                               // 0x1007a04d  blue
        U8(png, PNG_SIG_BIT + 4) = buf[3];                               // 0x1007a059  alpha
    } else {
        U8(png, PNG_SIG_BIT + 3) = buf[0];                               // 0x1007a067  gray
        U8(png, PNG_SIG_BIT + 0) = buf[0];                               // 0x1007a073
        U8(png, PNG_SIG_BIT + 1) = buf[0];                               // 0x1007a07f
        U8(png, PNG_SIG_BIT + 2) = buf[0];                               // 0x1007a08b
        U8(png, PNG_SIG_BIT + 4) = buf[1];                               // 0x1007a097  alpha
    }
    reinterpret_cast<PngSetPtr_t>(JgldVA(VA_PNG_SET_SBIT))(              // 0x1007a0ae
        png, info, static_cast<char*>(png) + PNG_SIG_BIT);               // 0x1007a0a0
}

// ============================================================== png_handle_unknown 0x0007c150
//
// void __cdecl(png_structp, png_infop, png_uint_32 length); `ret` with no pop at 0x1007c1e4.
//
// The reader every chunk whose type libpng does not implement ends up in. It first runs
// png_check_chunk_name 0x1007c1f0 over the four bytes at png+0x11c (0x1007c175), which calls
// png_chunk_error 0x100790a0 for any byte outside the two letter ranges. A chunk whose first byte has
// bit 5 clear is CRITICAL, and an unknown critical chunk is fatal: png_chunk_error again (0x1007c188,
// 0x1007c198), with the same short-circuit tail on the info_ptr at 0x1007c1a0 that IEND has. An
// ancillary chunk is simply skipped: if PNG_HAVE_IDAT is set the body records that the chunk arrived
// after the image data by setting bit 3 of the mode (0x1007c1ae-0x1007c1c1), and png_crc_finish
// 0x10079510 consumes the payload and checks the CRC (0x1007c1cc). Nothing of the chunk is kept.
PngHandle_t PngHandleUnknown_orig;

void __cdecl PngHandleUnknown_re(void* png, void* info, unsigned length) {
    reinterpret_cast<PngCheckName_t>(JgldVA(VA_PNG_CHECK_CHUNK_NAME))(   // 0x1007c175
        png, static_cast<const unsigned char*>(png) + PNG_CHUNK_NAME);   // 0x1007c16b
    if ((U8R(png, PNG_CHUNK_NAME) & 0x20) == 0) {                        // 0x1007c188
        pngChunkError(png, 0x1011EA84);                                  // 0x1007c198
        if (info == 0) return;                                           // 0x1007c1a0, 0x1007c1a6
    }
    if (U32R(png, PNG_MODE) & 4)                                         // 0x1007c1ae
        U32(png, PNG_MODE) = U32R(png, PNG_MODE) | 8;                    // 0x1007c1bb, 0x1007c1c1
    crcFinish(png, length);                                              // 0x1007c1cc
}

// =============================================== jgld's 256-colour Palette =========================
//
// The object the four methods below work on, as far as they read it: the vtable at +0, two dwords at
// +4 and +8 that none of them touches, 256 four-byte entries at +0xc (red, green, blue, flags - the
// Win32 PALETTEENTRY order), the RGB555 table at +0x40c and the RGB565 table at +0x60c that
// Palette::apply 0x1006a4a0 derives from them, an owner pointer at +0x80c, the generation id at +0x810,
// the id Palette::apply last worked from at +0x814 and the eight bytes at +0x818 that Palette::update
// 0x1006a3d0 fills with QueryPerformanceCounter. The entry pointer below is the address of entry `i`.
unsigned char* PalEntry(void* self, int i) {
    return static_cast<unsigned char*>(self) + 0xC + i * 4;
}

typedef int(__fastcall* PalNoArg_t)(void*, void*);
typedef void(__fastcall* PalVoid_t)(void*, void*);
const unsigned VA_PALETTE_UPDATE = 0x1006A3D0;
const unsigned VA_SYS_COLORS_LOW = 0x1011DAB0;
const unsigned VA_SYS_COLORS_HIGH = 0x1011DAD8;
const unsigned VA_GET_COLOR_SCRATCH = 0x10128734;

// ============================================================== Palette::setDefault 0x0006a600
//
// int __thiscall(void): `this` in ecx, spilled to [ebp-4] at 0x1006a61a; `ret` with no pop at
// 0x1006a7e2, returning the `xor eax,eax` of 0x1006a7d0.
//
// It writes all 256 entries of a palette from nothing but constants and the entry index, so its output
// does not depend on its input at all - which is what the twelve differently pre-filled palettes of the
// A/B measure. First it calls slot 2 of its own vtable (0x1006a627), which in the Palette vtable VA
// 0x1011db10 is Palette::release 0x1006a380 (through the incremental-link thunk 0x10001451): that zeroes
// the generation id at +0x810 and the applied id at +0x814, so the Palette::apply 0x1006a4a0 at the end
// of Palette::update always has work to do.
//
// Entries 0 to 9 take the ten RGBQUADs at VA 0x1011dab0 and entries 0xf6 to 0xff the ten at VA
// 0x1011dad8, each read backwards - red from +2, green from +1, blue from +0 (0x1006a656, 0x1006a66a,
// 0x1006a67e for the low ten; 0x1006a69d, 0x1006a6b4, 0x1006a6cb for the high ten) - because an RGBQUAD
// stores blue first and a PALETTEENTRY red first. Both blocks get flags 4 = PC_NOCOLLAPSE (0x1006a68f,
// 0x1006a6df). Entries 10 to 0xf5 are a greyscale ramp whose three components are all the LOW BYTE of
// the index (0x1006a70d, 0x1006a71a, 0x1006a727) with flags 5 = PC_NOCOLLAPSE | PC_RESERVED
// (0x1006a734). Finally entries 8, 9, 0xf6 and 0xf7 are overwritten as system-palette aliases: red is
// the index 8, 9, 0xf5 or 0xf6 (0x1006a75f, 0x1006a792 - note the high pair takes i + 0xf5, one less
// than its own index), green and blue are zero and flags are 2 = PC_EXPLICIT (0x1006a769-0x1006a77f,
// 0x1006a79f-0x1006a7bb).
//
// Palette::update 0x1006a3d0 (0x1006a7cb, through the thunk 0x10001096) then draws a new generation id
// and applies the entries to the two 16-bit tables. The id it draws is seeded from
// QueryPerformanceCounter, so +0x810, +0x814 and +0x818 are left out of the compared state and
// everything below +0x810 is in it.
PalNoArg_t PaletteSetDefault_orig;

int __fastcall PaletteSetDefault_re(void* self, void*) {
    const unsigned char* lo = static_cast<const unsigned char*>(JgldVA(VA_SYS_COLORS_LOW));
    const unsigned char* hi = static_cast<const unsigned char*>(JgldVA(VA_SYS_COLORS_HIGH));
    int i;
    reinterpret_cast<PalVoid_t>(P32R(P32R(self, 0), 8))(self, 0);        // 0x1006a627  vtable slot 2
    for (i = 0; i < 10; i++) {                                           // 0x1006a643
        PalEntry(self, i)[0] = lo[i * 4 + 2];                            // 0x1006a656
        PalEntry(self, i)[1] = lo[i * 4 + 1];                            // 0x1006a66a
        PalEntry(self, i)[2] = lo[i * 4 + 0];                            // 0x1006a67e
        PalEntry(self, i)[3] = 4;                                        // 0x1006a68f
        PalEntry(self, 0xF6 + i)[0] = hi[i * 4 + 2];                     // 0x1006a69d
        PalEntry(self, 0xF6 + i)[1] = hi[i * 4 + 1];                     // 0x1006a6b4
        PalEntry(self, 0xF6 + i)[2] = hi[i * 4 + 0];                     // 0x1006a6cb
        PalEntry(self, 0xF6 + i)[3] = 4;                                 // 0x1006a6df
    }
    for (i = 10; i < 0xF6; i++) {                                        // 0x1006a6fe
        PalEntry(self, i)[0] = static_cast<unsigned char>(i);            // 0x1006a70d
        PalEntry(self, i)[1] = static_cast<unsigned char>(i);            // 0x1006a71a
        PalEntry(self, i)[2] = static_cast<unsigned char>(i);            // 0x1006a727
        PalEntry(self, i)[3] = 5;                                        // 0x1006a734
    }
    for (i = 0; i < 2; i++) {                                            // 0x1006a74d
        PalEntry(self, 8 + i)[0] = static_cast<unsigned char>(i + 8);    // 0x1006a75f
        PalEntry(self, 8 + i)[1] = 0;                                    // 0x1006a769
        PalEntry(self, 8 + i)[2] = 0;                                    // 0x1006a774
        PalEntry(self, 8 + i)[3] = 2;                                    // 0x1006a77f
        PalEntry(self, 0xF6 + i)[0] = static_cast<unsigned char>(i + 0xF5);   // 0x1006a792
        PalEntry(self, 0xF6 + i)[1] = 0;                                 // 0x1006a79f
        PalEntry(self, 0xF6 + i)[2] = 0;                                 // 0x1006a7ad
        PalEntry(self, 0xF6 + i)[3] = 2;                                 // 0x1006a7bb
    }
    reinterpret_cast<PalVoid_t>(JgldVA(VA_PALETTE_UPDATE))(self, 0);     // 0x1006a7cb
    return 0;                                                            // 0x1006a7d0
}

// ================================================================= Palette::setRGB 0x0006a9a0
//
// int __thiscall(const unsigned char* rgb, int first, int n): `this` in ecx at 0x1006a9ba, rgb
// [ebp+8], first [ebp+0xc], n [ebp+0x10]; `ret 0xc` at 0x1006aa40, returning the `xor eax,eax` of
// 0x1006aa2e.
//
// It copies `n` three-byte triples out of the caller's buffer into entries `first` to `first + n - 1`,
// red, green then blue (0x1006a9ec, 0x1006aa06, 0x1006aa20), and leaves the flags byte of every entry it
// touches alone. The loop counter is signed (`jge` at 0x1006a9d5), so a count of zero or less writes
// nothing, and neither `first` nor `n` is checked against the 256 entries: a range that runs off the end
// writes straight into the RGB555 table at +0x40c, which Palette::apply then overwrites. The destination
// index is recomputed as `i + first` and the source offset as `i * 3` on every one of the three bytes
// (0x1006a9d7-0x1006a9e9 and the two blocks after it). Palette::update 0x1006a3d0 (0x1006aa29) re-derives
// the two 16-bit tables afterwards, exactly as in setDefault.
typedef int(__fastcall* PalSetRGB_t)(void*, void*, const unsigned char*, int, int);
PalSetRGB_t PaletteSetRGB_orig;

int __fastcall PaletteSetRGB_re(void* self, void*, const unsigned char* rgb, int first, int n) {
    for (int i = 0; i < n; i++) {                                        // 0x1006a9d5
        PalEntry(self, i + first)[0] = rgb[i * 3];                       // 0x1006a9ec
        PalEntry(self, i + first)[1] = rgb[i * 3 + 1];                   // 0x1006aa06
        PalEntry(self, i + first)[2] = rgb[i * 3 + 2];                   // 0x1006aa20
    }
    reinterpret_cast<PalVoid_t>(JgldVA(VA_PALETTE_UPDATE))(self, 0);     // 0x1006aa29
    return 0;                                                            // 0x1006aa2e
}

// ============================================================ Palette::getRGBRange 0x0006a8e0
//
// int __thiscall(unsigned char* dst, int first, int n): `this` in ecx at 0x1006a8fa, dst [ebp+8],
// first [ebp+0xc], n [ebp+0x10]; `ret 0xc` at 0x1006a96e, returning the `xor eax,eax` of 0x1006a966.
//
// The read side of setRGB: `n` entries from `first` onwards are written into the caller's buffer as
// three-byte triples (0x1006a92d, 0x1006a946, 0x1006a960), leaving the flags byte behind. Same signed
// loop test (0x1006a915), same absence of any range check, and no call at all - this is the one method
// of the four with no callee, not even the debug build's __chkesp 0x1007e780, which its epilogue
// (0x1006a968-0x1006a96e) skips.
typedef int(__fastcall* PalGetRange_t)(void*, void*, unsigned char*, int, int);
PalGetRange_t PaletteGetRGBRange_orig;

int __fastcall PaletteGetRGBRange_re(void* self, void*, unsigned char* dst, int first, int n) {
    for (int i = 0; i < n; i++) {                                        // 0x1006a915
        dst[i * 3] = PalEntry(self, i + first)[0];                       // 0x1006a92d
        dst[i * 3 + 1] = PalEntry(self, i + first)[1];                   // 0x1006a946
        dst[i * 3 + 2] = PalEntry(self, i + first)[2];                   // 0x1006a960
    }
    return 0;                                                            // 0x1006a966
}

// =============================================================== Palette::getColor 0x0006add0
//
// unsigned char* __thiscall(int index): `this` in ecx at 0x1006adea, index [ebp+8]; `ret 4` at
// 0x1006ae28.
//
// One entry read through a shared three-byte scratch: red, green and blue of entry `index` are copied
// to the module bytes at VA 0x10128734, 0x10128735 and 0x10128736 (0x1006adf7, 0x1006ae07, 0x1006ae17)
// and the address of the first of them is returned (`mov eax, 0x10128734` at 0x1006ae1d, an operand the
// module's base relocations fix up at load). So the result is not per-call storage: two callers that
// hold the pointer see the same three bytes, and the running game enters this 30 times in 20 s at the
// menu (log/c3/alt/c3ck_probe.txt), which is why the key that drives it suspends the game's first
// thread. The index is not checked and the flags byte of the entry is not copied; like getRGBRange the
// body has no callee and its epilogue skips __chkesp (0x1006ae22-0x1006ae28).
typedef unsigned char*(__fastcall* PalGetColor_t)(void*, void*, int);
PalGetColor_t PaletteGetColor_orig;

unsigned char* __fastcall PaletteGetColor_re(void* self, void*, int index) {
    unsigned char* g = static_cast<unsigned char*>(JgldVA(VA_GET_COLOR_SCRATCH));
    g[0] = PalEntry(self, index)[0];                                     // 0x1006adf7
    g[1] = PalEntry(self, index)[1];                                     // 0x1006ae07
    g[2] = PalEntry(self, index)[2];                                     // 0x1006ae17
    return g;                                                            // 0x1006ae1d
}

// ====================================================================== windowProc 0x000692b0
//
// LRESULT __stdcall(HWND, UINT msg, WPARAM, LPARAM): hwnd [ebp+8], msg [ebp+0xc], wParam [ebp+0x10],
// lParam [ebp+0x14]; `ret 0x10` at 0x10069775. Display::createWindow 0x100657e0 stores the address of
// its incremental-link thunk 0x10001302 at 0x10065836 (the only reference to it in the four modules),
// which is how the window class reaches it.
//
// The window procedure of every jgld window. It is a four-way range split on the message - at or below
// 0x200 (0x100692ce), below 0x20b (0x10069347), below 0x312 (0x1006937d), and 0x401 (0x100693ae) - with
// three jump tables inside the first two: msg - 5 indexed through the byte table at 0x10069790 into the
// six targets at 0x10069778 (0x10069318), msg - 0x101 through the byte table at 0x10069828 into the six
// at 0x10069810 (0x10069340), and msg - 0x201 straight into the six at 0x1006983a (0x10069376). Every
// comparison is unsigned and the message is kept in a local the SUBTRACTIONS overwrite (0x100692fd,
// 0x10069322, 0x10069360), so a message below the base of a table wraps to a large value and lands on
// the default arm; the untouched copy at [ebp+0xc] is what the default arm hands to DefWindowProcA
// (0x10069750).
//
// Fifteen messages are handled and they fall into four groups by what they return. Most call one
// handler and return the `xor eax,eax` that follows it. WM_NCHITTEST 0x84 (0x100694ec) and WM_COPYDATA
// 0x4a (0x100693eb) return the handler's own result - neither has a `xor eax,eax` before its jump to
// the epilogue, which is what the A/B's one mismatch on the first run found. 0x30f returns the low
// sixteen bits of realizePalette 0x100699c0's
// result (0x10069442). WM_MOUSEWHEEL 0x20a (0x10069608) and the private 0x401 (0x100693d0) call their
// handler AND THEN fall into DefWindowProcA (0x10069610, 0x100693da), so the default processing still
// runs for them.
//
// The message-to-handler map, with the thunk each call goes through and the body behind it:
//   0x005 WM_SIZE            0x10001933 -> onWmSize 0x10069db0       (hwnd, wParam, (short)lo, (short)hi)
//   0x006 WM_ACTIVATE        0x10001280 -> onActivate 0x10069b90     (hwnd, wParam low 16, lParam,
//                                                                     wParam high 16)
//   0x00f WM_PAINT           0x10001352 -> onPaint 0x10069ac0        (hwnd)
//   0x04a WM_COPYDATA        0x10001708 -> onCommand 0x1006a0e0      (hwnd, wParam, lParam)
//   0x084 WM_NCHITTEST       0x100010f5 -> onWmNcHitTest 0x10069e10  (hwnd, (short)lo, (short)hi)
//   0x100 WM_KEYDOWN         0x1000100a -> onKey 0x10069c70          (hwnd, wParam, 1, (short)lo,
//                                                                     unsigned hi)
//   0x101 WM_KEYUP           0x1000100a -> onKey                     (..., 0, ...)
//   0x102 WM_CHAR            0x10001843 -> onChar 0x10069d50         (hwnd, wParam low byte, (short)lo)
//   0x104 WM_SYSKEYDOWN      0x1000100a -> onKey                     (..., 1, ...)
//   0x105 WM_SYSKEYUP        0x1000100a -> onKey                     (..., 0, ...)
//   0x112 WM_SYSCOMMAND      -                                        the 0xf140 / 0xf170 test below
//   0x200 WM_MOUSEMOVE       0x10001b09 -> onWmMouseMove 0x10069ee0  (hwnd, (short)lo, (short)hi, wParam)
//   0x201 WM_LBUTTONDOWN     0x100016e0 -> onWmLButtonDown 0x10069f40 (hwnd, 0, x, y, wParam)
//   0x202 WM_LBUTTONUP       0x10001947 -> onWmLButtonUp 0x10069fb0  (hwnd, x, y, wParam)
//   0x203 WM_LBUTTONDBLCLK   0x100016e0 -> onWmLButtonDown           (hwnd, 1, x, y, wParam)
//   0x204 WM_RBUTTONDOWN     0x1000132f -> onWmRButtonDown 0x1006a010 (hwnd, 0, x, y, wParam)
//   0x205 WM_RBUTTONUP       0x100018f7 -> onWmRButtonUp 0x1006a080  (hwnd, x, y, wParam)
//   0x206 WM_RBUTTONDBLCLK   0x1000132f -> onWmRButtonDown           (hwnd, 1, x, y, wParam)
//   0x20a WM_MOUSEWHEEL      0x100011d1 -> onWmMouseWheel 0x10069e70 ((short)wParam hi, (short)lo,
//                                                                     (short)hi, wParam low 16)
//   0x218 WM_POWERBROADCAST  -                                        see below
//   0x30f                    0x10001028 -> realizePalette 0x100699c0 (hwnd)
//   0x311 WM_PALETTECHANGED  0x100014f6 -> onPaletteChanged 0x10069a70 (hwnd, wParam)
//   0x401                    the event sink's slot +0x30               (wParam)
// WM_COMMAND 0x111 is NOT in that list: its slot in the 0x101 table is the default one, so a command
// notification goes to DefWindowProcA, and the handler named onCommand is the one WM_COPYDATA reaches.
//
// Two messages share one body. WM_POWERBROADCAST 0x218 with a zero wParam - PBT_APMQUERYSUSPEND -
// returns the constant 0x424d5144 (0x10069406), which is BROADCAST_QUERY_DENY, so the window refuses to
// let the machine suspend; any other wParam falls into the WM_SYSCOMMAND body (0x10069404), which
// swallows 0xf140 SC_SCREENSAVE and 0xf170 SC_MONITORPOWER by returning zero (0x10069416, 0x1006941f,
// 0x1006942a) and sends everything else to DefWindowProcA (0x10069431).
//
// Three of the handlers read a module global rather than an argument: the 0x401 arm loads the event sink
// from VA 0x1012872c twice, once for the vtable and once for ecx (0x100693c2, 0x100693ca), onKey reads
// the display at VA 0x10128420 and onCommand the command handler at VA 0x10128728. All three carry
// objects of the fixture's own for the length of a key.
typedef int(__cdecl* Wp1_t)(void*);
typedef int(__cdecl* Wp2_t)(void*, unsigned);
typedef int(__cdecl* Wp3_t)(void*, unsigned, unsigned);
typedef int(__cdecl* Wp4_t)(void*, unsigned, unsigned, unsigned);
typedef int(__cdecl* Wp5_t)(void*, unsigned, unsigned, unsigned, unsigned);
typedef int(__fastcall* Sink1_t)(void*, void*, unsigned);
typedef int(__stdcall* DefWndProc_t)(void*, unsigned, unsigned, unsigned);
typedef int(__stdcall* WindowProc_t)(void*, unsigned, unsigned, unsigned);

const unsigned VA_ON_KEY = 0x10069C70, VA_ON_CHAR = 0x10069D50, VA_ON_WM_SIZE = 0x10069DB0;
const unsigned VA_ON_NCHITTEST = 0x10069E10, VA_ON_WHEEL = 0x10069E70, VA_ON_MOUSEMOVE = 0x10069EE0;
const unsigned VA_ON_LDOWN = 0x10069F40, VA_ON_LUP = 0x10069FB0, VA_ON_RDOWN = 0x1006A010;
const unsigned VA_ON_RUP = 0x1006A080, VA_ON_COMMAND = 0x1006A0E0, VA_ON_PAINT = 0x10069AC0;
const unsigned VA_ON_ACTIVATE = 0x10069B90, VA_ON_PALETTE_CHANGED = 0x10069A70;
const unsigned VA_REALIZE_PALETTE = 0x100699C0;
const unsigned VA_SINK_PTR = 0x1012872C, VA_DEFWINDOWPROC_SLOT = 0x1012C62C;

WindowProc_t WindowProc_orig;

int __stdcall WindowProc_re(void* hwnd, unsigned msg, unsigned wParam, unsigned lParam) {
    const int xs = static_cast<short>(lParam & 0xFFFF);                  // movsx of [ebp+0x14]
    const int ys = static_cast<short>((lParam >> 16) & 0xFFFF);          // movsx of its high half
    const unsigned yu = (lParam >> 16) & 0xFFFF;                         // the zero-extended high half

    if (msg <= 0x200) {                                                  // 0x100692ce
        if (msg == 0x200) {                                              // 0x100692d7
            reinterpret_cast<Wp4_t>(JgldVA(VA_ON_MOUSEMOVE))(hwnd, xs, ys, wParam);     // 0x10069632
            return 0;                                                    // 0x1006963a
        }
        if (msg <= 0x100) {                                              // 0x100692e4
            if (msg == 0x100) {                                          // 0x100692ed
                reinterpret_cast<Wp5_t>(JgldVA(VA_ON_KEY))(hwnd, wParam, 1, xs, yu);    // 0x1006951b
                return 0;                                                // 0x10069523
            }
            if (msg - 5 > 0x7F) goto def;                                // 0x10069303
            switch (msg) {                                               // table 0x10069790 / 0x10069778
            case 0x005:
                reinterpret_cast<Wp4_t>(JgldVA(VA_ON_WM_SIZE))(hwnd, wParam, xs, ys);   // 0x100694c5
                return 0;                                                // 0x100694cd
            case 0x006:
                reinterpret_cast<Wp4_t>(JgldVA(VA_ON_ACTIVATE))(                        // 0x10069499
                    hwnd, wParam & 0xFFFF, lParam, (wParam >> 16) & 0xFFFF);
                return 0;                                                // 0x100694a1
            case 0x00F:
                reinterpret_cast<Wp1_t>(JgldVA(VA_ON_PAINT))(hwnd);                     // 0x10069467
                return 0;                                                // 0x1006946f
            case 0x04A:
                // no `xor eax,eax` before the jump at 0x100693f3: WM_COPYDATA returns what the
                // command handler returned, unlike every other arm of this table
                return reinterpret_cast<Wp3_t>(JgldVA(VA_ON_COMMAND))(hwnd, wParam, lParam);  // 0x100693eb
            case 0x084:
                return reinterpret_cast<Wp3_t>(JgldVA(VA_ON_NCHITTEST))(hwnd, xs, ys);  // 0x100694ec
            default:
                goto def;
            }
        }
        if (msg - 0x101 > 0x11) goto def;                                // 0x1006932b
        switch (msg) {                                                   // table 0x10069828 / 0x10069810
        case 0x101:
            reinterpret_cast<Wp5_t>(JgldVA(VA_ON_KEY))(hwnd, wParam, 0, xs, yu);        // 0x1006954c
            return 0;                                                    // 0x10069554
        case 0x102:
            reinterpret_cast<Wp3_t>(JgldVA(VA_ON_CHAR))(                                // 0x10069568
                hwnd, wParam & 0xFF, xs);                                // 0x10069560: only the low byte
            return 0;                                                    // 0x10069570
        case 0x104:
            reinterpret_cast<Wp5_t>(JgldVA(VA_ON_KEY))(hwnd, wParam, 1, xs, yu);        // 0x10069597
            return 0;                                                    // 0x1006959f
        case 0x105:
            reinterpret_cast<Wp5_t>(JgldVA(VA_ON_KEY))(hwnd, wParam, 0, xs, yu);        // 0x100695c8
            return 0;                                                    // 0x100695d0
        case 0x112:
            goto syscommand;                                             // 0x10069410
        default:
            goto def;
        }
    }
    if (msg < 0x20B) {                                                   // 0x10069347
        if (msg != 0x20A) {                                              // 0x10069350
            if (msg - 0x201 > 5) goto def;                               // 0x10069369
            switch (msg) {                                               // table 0x1006983a
            case 0x201:
                reinterpret_cast<Wp5_t>(JgldVA(VA_ON_LDOWN))(hwnd, 0, xs, ys, wParam);  // 0x1006968d
                return 0;                                                // 0x10069695
            case 0x202:
                reinterpret_cast<Wp4_t>(JgldVA(VA_ON_LUP))(hwnd, xs, ys, wParam);       // 0x100696b9
                return 0;                                                // 0x100696c1
            case 0x203:
                reinterpret_cast<Wp5_t>(JgldVA(VA_ON_LDOWN))(hwnd, 1, xs, ys, wParam);  // 0x1006965f
                return 0;                                                // 0x10069667
            case 0x204:
                reinterpret_cast<Wp5_t>(JgldVA(VA_ON_RDOWN))(hwnd, 0, xs, ys, wParam);  // 0x10069711
                return 0;                                                // 0x10069719
            case 0x205:
                reinterpret_cast<Wp4_t>(JgldVA(VA_ON_RUP))(hwnd, xs, ys, wParam);       // 0x1006973a
                return 0;                                                // 0x10069742
            case 0x206:
                reinterpret_cast<Wp5_t>(JgldVA(VA_ON_RDOWN))(hwnd, 1, xs, ys, wParam);  // 0x100696e6
                return 0;                                                // 0x100696ee
            default:
                goto def;
            }
        }
        reinterpret_cast<Wp5_t>(JgldVA(VA_ON_WHEEL))(                                   // 0x10069608
            hwnd, static_cast<short>((wParam >> 16) & 0xFFFF), xs, ys, wParam & 0xFFFF);
        goto def;                                                        // 0x10069610
    }
    if (msg < 0x312) {                                                   // 0x1006937d
        if (msg == 0x311) {                                              // 0x10069386
            reinterpret_cast<Wp2_t>(JgldVA(VA_ON_PALETTE_CHANGED))(hwnd, wParam);       // 0x10069454
            return 0;                                                    // 0x1006945c
        }
        if (msg == 0x218) {                                              // 0x10069393
            if (wParam == 0) return 0x424D5144;                          // 0x100693fe, 0x10069406
            goto syscommand;                                             // 0x10069404
        }
        if (msg == 0x30F)                                                // 0x1006939c
            return reinterpret_cast<Wp1_t>(JgldVA(VA_REALIZE_PALETTE))(hwnd) & 0xFFFF;  // 0x1006943a
    } else if (msg == 0x401) {                                           // 0x100693ae
        void* sink = P32R(JgldVA(VA_SINK_PTR), 0);                       // 0x100693c2 / 0x100693ca
        reinterpret_cast<Sink1_t>(P32R(P32R(sink, 0), 0x30))(sink, 0, wParam);   // 0x100693d0
    }
    goto def;

syscommand:
    if (wParam == 0xF140 || wParam == 0xF170) return 0;                  // 0x10069416, 0x1006941f
    goto def;                                                            // 0x10069431

def:
    return reinterpret_cast<DefWndProc_t>(P32R(JgldVA(VA_DEFWINDOWPROC_SLOT), 0))(      // 0x10069758
        hwnd, msg, wParam, lParam);
}

}  // namespace

// libpng 1.0.5's chunk readers: the ones that keep nothing on the heap.
// 0x000796e0  png_handle_IHDR
SG_HOOK("jgld.dll", 0x000796E0, png_handle_IHDR, PngHandleIHDR_re, PngHandleIHDR_orig);
// 0x00079c20  png_handle_IEND
SG_HOOK("jgld.dll", 0x00079C20, png_handle_IEND, PngHandleIEND_re, PngHandleIEND_orig);
// 0x0007b380  png_handle_pHYs
SG_HOOK("jgld.dll", 0x0007B380, png_handle_pHYs, PngHandlePHYs_re, PngHandlePHYs_orig);
// 0x0007b4d0  png_handle_oFFs
SG_HOOK("jgld.dll", 0x0007B4D0, png_handle_oFFs, PngHandleOFFs_re, PngHandleOFFs_orig);
// 0x0007b9b0  png_handle_tIME
SG_HOOK("jgld.dll", 0x0007B9B0, png_handle_tIME, PngHandleTIME_re, PngHandleTIME_orig);
// 0x00079ec0  png_handle_sBIT
SG_HOOK("jgld.dll", 0x00079EC0, png_handle_sBIT, PngHandleSBIT_re, PngHandleSBIT_orig);
// 0x0007c150  png_handle_unknown
SG_HOOK("jgld.dll", 0x0007C150, png_handle_unknown, PngHandleUnknown_re, PngHandleUnknown_orig);

// The four Palette methods reached through slots of the Palette vtable VA 0x1011db10.
// 0x0006a600  Palette::setDefault
SG_HOOK("jgld.dll", 0x0006A600, Palette_setDefault, PaletteSetDefault_re, PaletteSetDefault_orig);
// 0x0006a9a0  Palette::setRGB
SG_HOOK("jgld.dll", 0x0006A9A0, Palette_setRGB, PaletteSetRGB_re, PaletteSetRGB_orig);
// 0x0006a8e0  Palette::getRGBRange
SG_HOOK("jgld.dll", 0x0006A8E0, Palette_getRGBRange, PaletteGetRGBRange_re, PaletteGetRGBRange_orig);
// 0x0006add0  Palette::getColor
SG_HOOK("jgld.dll", 0x0006ADD0, Palette_getColor, PaletteGetColor_re, PaletteGetColor_orig);

// 0x000692b0  windowProc
SG_HOOK("jgld.dll", 0x000692B0, windowProc, WindowProc_re, WindowProc_orig);

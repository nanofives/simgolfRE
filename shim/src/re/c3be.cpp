// C3 batch c3be (round 14, 2026-10-08) of golf_clean.exe. Eleven functions taken from
// log/c3/c3be_candidates.tsv: seven IJG libjpeg 6a helpers (the compress-side error, marker, parameter,
// coefficient and Huffman code the exe links from original\JPEG.lib) and four engine forwarders that
// dispatch through one object pointer (the network slot table and three Surface/device wrappers).
// All eleven are matched at 100% by VC6 (re/match/golf_jpeg_*.cpp, golf_hand_01.cpp, golf_hand_03.cpp);
// every body here is hand-written from the disassembly
// (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr> --list) and cites the address of each
// instruction it reproduces. __thiscall is emulated with __fastcall (ecx = this, edx unused).
// Callees run through their original addresses:
//   add_huff_table    -> jpeg_alloc_huff_table 0x004afad0 (C3, c3af)
//   emit_jfif_app0 /
//   emit_adobe_app14  -> emit_marker 0x004afbf0, emit_2bytes 0x004afc10, emit_byte 0x004afbb0 (C3, c3af)
//   flush_bits        -> dump_buffer 0x004b19e0 (jchuff, not hooked)
//   Surface_box       -> 0x00475b00 (Widget_fillRectR, C3, c3ab)
//   hitBuildingTool /
//   hitBuildTool433c60 -> approxDistance 0x00467170 (C3, golf_math.cpp, linked in every build)
//
//   jpeg_std_error      0x004aec80   emit_jfif_app0     0x004afc80   flush_bits         0x004b1b70
//   start_iMCU_row      0x004b0860   emit_adobe_app14   0x004afd20   dump_buffer_4b2420 0x004b2420
//   add_huff_table      0x004ae8b0   netNameOf          0x0049aa30   Surface_drawRegion 0x00473df0
//   DC_call84           0x00475b60   Surface_box        0x00475b20
#include <string.h>

#include "hooks.h"

namespace {

// ---- IJG libjpeg 6a structures reached through pointers -------------------------------------------------------
// A jpeg_compress_struct is referenced through cinfo+0 = err (jpeg_error_mgr*), cinfo+4 = mem
// (jpeg_memory_mgr*) and cinfo+0x14 = dest (jpeg_destination_mgr*). A destination manager is
// {next_output_byte +0, free_in_buffer +4, init_destination +8, empty_output_buffer +0xc,
// term_destination +0x10}; an error manager is {error_exit +0, ..., msg_code +0x14, ...}.
typedef int(__cdecl* EmptyFn_t)(void* cinfo);
typedef void(__cdecl* ErrExit_t)(void* cinfo);

// Callees through their original addresses.
typedef void*(__cdecl* AllocTbl_t)(void* cinfo);
const AllocTbl_t kAllocHuffTable = reinterpret_cast<AllocTbl_t>(0x004afad0);   // jpeg_alloc_huff_table (C3, c3af)
typedef void(__cdecl* EmitVal_t)(void* cinfo, int value);
const EmitVal_t kEmitByte = reinterpret_cast<EmitVal_t>(0x004afbb0);           // emit_byte    (C3, c3af)
const EmitVal_t kEmitMarker = reinterpret_cast<EmitVal_t>(0x004afbf0);         // emit_marker  (C3, c3af)
const EmitVal_t kEmit2Bytes = reinterpret_cast<EmitVal_t>(0x004afc10);         // emit_2bytes  (C3, c3af)
typedef int(__cdecl* DumpBuf_t)(void* state);
const DumpBuf_t kDumpBuffer = reinterpret_cast<DumpBuf_t>(0x004b19e0);         // jchuff dump_buffer (C2)

// 0x004aec80  jpeg_std_error(err): fill a jpeg_error_mgr with the library's own method pointers, the standard
// message table and zeroed counters, and return the same pointer (eax still holds the argument loaded at
// 0x004aec80, so the return value is err). The five method slots are error_exit = 0x004aecd0 (0x004aec86),
// emit_message = 0x004aed20 (0x004aec8c), output_message = 0x004aecf0 (0x004aec93), format_message = 0x004aed60
// (0x004aec9a) and reset_error_mgr = 0x004aee10 (0x004aeca1). trace_level +0x68 and num_warnings +0x6c
// (0x004aeca8/0x004aecab) and msg_code +0x14 (0x004aecae) are zeroed; jpeg_message_table +0x70 = 0x004bceb8
// (0x004aecb1) and last_jpeg_message +0x74 = 0x77 (0x004aecb8); addon_message_table +0x78, first_addon_message
// +0x7c and last_addon_message +0x80 are zeroed (0x004aecbf..0x004aecc5). Nothing else in the structure is
// touched (the msg_parm union at +0x18..+0x64 keeps whatever the caller had there).
typedef void*(__cdecl* StdError_t)(void* err);
StdError_t StdError_orig;
void* __cdecl StdError_re(void* errp) {
    char* err = reinterpret_cast<char*>(errp);
    *reinterpret_cast<void**>(err + 0x00) = reinterpret_cast<void*>(0x004aecd0);  // error_exit
    *reinterpret_cast<void**>(err + 0x04) = reinterpret_cast<void*>(0x004aed20);  // emit_message
    *reinterpret_cast<void**>(err + 0x08) = reinterpret_cast<void*>(0x004aecf0);  // output_message
    *reinterpret_cast<void**>(err + 0x0c) = reinterpret_cast<void*>(0x004aed60);  // format_message
    *reinterpret_cast<void**>(err + 0x10) = reinterpret_cast<void*>(0x004aee10);  // reset_error_mgr
    *reinterpret_cast<int*>(err + 0x68) = 0;                                      // trace_level
    *reinterpret_cast<int*>(err + 0x6c) = 0;                                      // num_warnings
    *reinterpret_cast<int*>(err + 0x14) = 0;                                      // msg_code
    *reinterpret_cast<void**>(err + 0x70) = reinterpret_cast<void*>(0x004bceb8);  // jpeg_message_table
    *reinterpret_cast<int*>(err + 0x74) = 0x77;                                   // last_jpeg_message
    *reinterpret_cast<int*>(err + 0x78) = 0;                                      // addon_message_table
    *reinterpret_cast<int*>(err + 0x7c) = 0;                                      // first_addon_message
    *reinterpret_cast<int*>(err + 0x80) = 0;                                      // last_addon_message
    return errp;
}

// 0x004b0860  start_iMCU_row(cinfo): reset the within-iMCU-row counters of the coefficient controller at
// cinfo+0x150 (0x004b0870). When comps_in_scan (cinfo+0xec, 0x004b086a) is greater than 1 under a SIGNED compare
// (0x004b0876 cmp / 0x004b0878 jg), MCU_rows_per_iMCU_row (coef+0x14) is 1 (edx was set to 1 at 0x004b0865).
// Otherwise the row count comes from the first component of the scan, cur_comp_info[0] = [cinfo+0xf0]
// (0x004b0883): it is that component's v_samp_factor (comp+0xc, 0x004b088e) while iMCU_row_num (coef+8,
// 0x004b0880) is below total_iMCU_rows - 1 (cinfo+0xe8 decremented at 0x004b0889) under an UNSIGNED compare
// (0x004b088a cmp / 0x004b088c jae), and its last_row_height (comp+0x48, 0x004b089e) otherwise. Both exits then
// clear mcu_ctr (coef+0xc) and MCU_vert_offset (coef+0x10) (0x004b0896/0x004b0899 and 0x004b08a6/0x004b08a9).
typedef void(__cdecl* StartIMcuRow_t)(void* cinfo);
StartIMcuRow_t StartIMcuRow_orig;
void __cdecl StartIMcuRow_re(void* cinfop) {
    char* cinfo = reinterpret_cast<char*>(cinfop);
    char* coef = *reinterpret_cast<char**>(cinfo + 0x150);
    int rows;
    if (*reinterpret_cast<int*>(cinfo + 0xec) > 1) {
        rows = 1;
    } else {
        unsigned total = *reinterpret_cast<unsigned*>(cinfo + 0xe8);
        unsigned row = *reinterpret_cast<unsigned*>(coef + 8);
        char* comp = *reinterpret_cast<char**>(cinfo + 0xf0);
        rows = (row < total - 1) ? *reinterpret_cast<int*>(comp + 0x0c)    // v_samp_factor
                                 : *reinterpret_cast<int*>(comp + 0x48);   // last_row_height
    }
    *reinterpret_cast<int*>(coef + 0x14) = rows;
    *reinterpret_cast<int*>(coef + 0x0c) = 0;
    *reinterpret_cast<int*>(coef + 0x10) = 0;
}

// 0x004ae8b0  add_huff_table(cinfo, htblptr, bits, val): define one Huffman table. When *htblptr is NULL
// (0x004ae8b7 cmp / 0x004ae8ba jne) it allocates a JHUFF_TBL through jpeg_alloc_huff_table 0x004afad0
// (0x004ae8c1) and stores it at *htblptr (0x004ae8c9). It then copies 0x11 bytes of code-length counts from
// `bits` into the table's bits[] at +0 (four dwords and one byte, 0x004ae8d5..0x004ae8ee) and 0x100 bytes of
// symbols from `val` into huffval[] at +0x11 (rep movsd with ecx = 0x40, 0x004ae8f6/0x004ae8fb), and clears
// sent_table at +0x114 (0x004ae902) so the table is written to the JPEG stream. *htblptr is re-read after the
// allocation (0x004ae8cf) and again for the sent_table store (0x004ae8fd).
typedef void(__cdecl* AddHuffTable_t)(void* cinfo, void** htblptr, const unsigned char* bits,
                                      const unsigned char* val);
AddHuffTable_t AddHuffTable_orig;
void __cdecl AddHuffTable_re(void* cinfo, void** htblptr, const unsigned char* bits, const unsigned char* val) {
    if (*htblptr == 0) *htblptr = kAllocHuffTable(cinfo);
    char* tbl = reinterpret_cast<char*>(*htblptr);
    memcpy(tbl, bits, 0x11);
    memcpy(tbl + 0x11, val, 0x100);
    *reinterpret_cast<int*>(reinterpret_cast<char*>(*htblptr) + 0x114) = 0;
}

// 0x004afc80  emit_jfif_app0(cinfo): write a JFIF APP0 marker through the destination manager. The marker byte
// is 0xE0 (emit_marker, 0x004afc85/0x004afc8b) and the block length is 0x10 (emit_2bytes, 0x004afc90). The
// identifier is 'J' 'F' 'I' 'F' NUL (0x4a, 0x46, 0x49, 0x46, 0, emit_byte at 0x004afc9b..0x004afcbb) followed by
// version 1.01 as two bytes 1, 1 (0x004afcc3/0x004afcce). Then come three fields of the compress struct:
// density_unit, the BYTE at cinfo+0xcc zero-extended (0x004afcd3/0x004afcd5, emit_byte 0x004afcdd), X_density,
// the WORD at cinfo+0xce (0x004afce4, emit_2bytes 0x004afced), and Y_density, the WORD at cinfo+0xd0
// (0x004afcf4, emit_2bytes 0x004afcfd). Two zero bytes for the empty thumbnail close the block
// (0x004afd05/0x004afd0d). The function has no conditional branch.
typedef void(__cdecl* EmitApp_t)(void* cinfo);
EmitApp_t EmitJfifApp0_orig;
void __cdecl EmitJfifApp0_re(void* cinfop) {
    char* cinfo = reinterpret_cast<char*>(cinfop);
    kEmitMarker(cinfop, 0xe0);
    kEmit2Bytes(cinfop, 0x10);
    kEmitByte(cinfop, 0x4a);
    kEmitByte(cinfop, 0x46);
    kEmitByte(cinfop, 0x49);
    kEmitByte(cinfop, 0x46);
    kEmitByte(cinfop, 0);
    kEmitByte(cinfop, 1);
    kEmitByte(cinfop, 1);
    kEmitByte(cinfop, *reinterpret_cast<unsigned char*>(cinfo + 0xcc));    // density_unit
    kEmit2Bytes(cinfop, *reinterpret_cast<unsigned short*>(cinfo + 0xce)); // X_density
    kEmit2Bytes(cinfop, *reinterpret_cast<unsigned short*>(cinfo + 0xd0)); // Y_density
    kEmitByte(cinfop, 0);
    kEmitByte(cinfop, 0);
}

// 0x004afd20  emit_adobe_app14(cinfo): write an Adobe APP14 marker. Marker byte 0xEE (emit_marker, 0x004afd25),
// block length 0xE (emit_2bytes, 0x004afd30), identifier 'A' 'd' 'o' 'b' 'e' (0x41, 0x64, 0x6f, 0x62, 0x65,
// emit_byte at 0x004afd3b..0x004afd5b), version 0x64 = 100 (emit_2bytes, 0x004afd63) and two zero 16-bit flag
// words (0x004afd6e/0x004afd76). The last byte is the colour transform, selected from jpeg_color_space at
// cinfo+0x38 (0x004afd7b): the value is 1 when the colour space is 3 (0x004afd81 sub / 0x004afd84 je ->
// 0x004afda5), 2 when it is 5 (0x004afd86 sub / 0x004afd89 je -> 0x004afd98) and 0 for every other value
// (0x004afd8b).
EmitApp_t EmitAdobeApp14_orig;
void __cdecl EmitAdobeApp14_re(void* cinfop) {
    char* cinfo = reinterpret_cast<char*>(cinfop);
    kEmitMarker(cinfop, 0xee);
    kEmit2Bytes(cinfop, 0xe);
    kEmitByte(cinfop, 0x41);
    kEmitByte(cinfop, 0x64);
    kEmitByte(cinfop, 0x6f);
    kEmitByte(cinfop, 0x62);
    kEmitByte(cinfop, 0x65);
    kEmit2Bytes(cinfop, 0x64);
    kEmit2Bytes(cinfop, 0);
    kEmit2Bytes(cinfop, 0);
    int space = *reinterpret_cast<int*>(cinfo + 0x38);
    if (space == 3) kEmitByte(cinfop, 1);
    else if (space == 5) kEmitByte(cinfop, 2);
    else kEmitByte(cinfop, 0);
}

// 0x004b1b70  flush_bits(state): pad the Huffman bit buffer to a byte boundary and empty it. The working_state
// is {next_output_byte +0, free_in_buffer +4, cur.put_buffer +8, cur.put_bits +0xc, ..., cinfo +0x20} (the last
// offset is the one jchuff's dump_buffer 0x004b19e0 reads). emit_bits(state, 0x7F, 7) is inlined: put_bits
// becomes cur.put_bits + 7 (0x004b1b7d/0x004b1b85) and put_buffer becomes (0x7F << (24 - put_bits)) OR
// cur.put_buffer (0x004b1b80/0x004b1b88/0x004b1b8a/0x004b1b8f; `shl ebp, cl` takes cl modulo 32). While put_bits
// is at least 8 (0x004b1b91 cmp / 0x004b1b98 jl on entry, 0x004b1bed cmp / 0x004b1bf4 jge at the bottom) the
// byte at bits 16..23 of put_buffer (read as the third byte of the dword at 0x004b1b9e) is stored at
// *next_output_byte, next_output_byte is incremented and free_in_buffer decremented (0x004b1ba2..0x004b1bad);
// when free_in_buffer reaches 0 (0x004b1bb0) dump_buffer 0x004b19e0 is called (0x004b1bb3) and a 0 return makes
// flush_bits return 0 (0x004b1bbd -> 0x004b1c0e). A written 0xFF (0x004b1bc5) is followed by a stuffed zero byte
// with the same buffer bookkeeping and the same dump_buffer check (0x004b1bc7..0x004b1be5). Each pass then
// shifts put_buffer left by 8 and takes 8 off put_bits (0x004b1be7/0x004b1bea). On exit the inlined emit_bits
// stores its put_buffer/put_bits back (0x004b1bf8/0x004b1bfb) and flush_bits immediately overwrites both with 0
// (0x004b1bfe/0x004b1c01); the return value is 1 (0x004b1c07).
typedef int(__cdecl* FlushBits_t)(void* state);
FlushBits_t FlushBits_orig;
int __cdecl FlushBits_re(void* statep) {
    char* st = reinterpret_cast<char*>(statep);
    int put_bits = *reinterpret_cast<int*>(st + 0xc) + 7;
    unsigned put_buffer = (0x7fu << ((24 - put_bits) & 31)) | *reinterpret_cast<unsigned*>(st + 8);
    while (put_bits >= 8) {
        unsigned char c = static_cast<unsigned char>(put_buffer >> 16);
        unsigned char* p = *reinterpret_cast<unsigned char**>(st + 0);
        *p = c;
        *reinterpret_cast<unsigned char**>(st + 0) = p + 1;
        unsigned freed = *reinterpret_cast<unsigned*>(st + 4) - 1;
        *reinterpret_cast<unsigned*>(st + 4) = freed;
        if (freed == 0 && !kDumpBuffer(statep)) return 0;
        if (c == 0xff) {
            p = *reinterpret_cast<unsigned char**>(st + 0);
            *p = 0;
            *reinterpret_cast<unsigned char**>(st + 0) = p + 1;
            freed = *reinterpret_cast<unsigned*>(st + 4) - 1;
            *reinterpret_cast<unsigned*>(st + 4) = freed;
            if (freed == 0 && !kDumpBuffer(statep)) return 0;
        }
        put_bits -= 8;
        put_buffer <<= 8;
    }
    *reinterpret_cast<unsigned*>(st + 8) = put_buffer;
    *reinterpret_cast<int*>(st + 0xc) = put_bits;
    *reinterpret_cast<unsigned*>(st + 8) = 0;
    *reinterpret_cast<int*>(st + 0xc) = 0;
    return 1;
}

// 0x004b2420  dump_buffer_4b2420(entropy): the progressive Huffman encoder's buffer flush. The entropy object
// holds cinfo at +0x20 (0x004b2426) and its own copy of the output pointers at +0x10/+0x14. The destination
// manager cinfo+0x14 is loaded once into edi (0x004b242a) and its empty_output_buffer (dest+0xc) is called with
// cinfo (0x004b242d). A zero return (0x004b2433/0x004b2435) sets msg_code = 0x16 (JERR_CANT_SUSPEND) in the
// error manager cinfo+0 at err+0x14 (0x004b243c) and calls err->error_exit (err+0) with cinfo (0x004b2449);
// cinfo is re-read from entropy+0x20 for both (0x004b2437/0x004b2443). The entropy object then copies back
// dest->next_output_byte into +0x10 (0x004b244e/0x004b2450) and dest->free_in_buffer into +0x14
// (0x004b2453/0x004b2456), from the dest pointer cached before the call.
typedef void(__cdecl* DumpPhuff_t)(void* entropy);
DumpPhuff_t DumpPhuff_orig;
void __cdecl DumpPhuff_re(void* entp) {
    char* ent = reinterpret_cast<char*>(entp);
    char* cinfo = *reinterpret_cast<char**>(ent + 0x20);
    char* dest = *reinterpret_cast<char**>(cinfo + 0x14);
    EmptyFn_t empty = *reinterpret_cast<EmptyFn_t*>(dest + 0xc);
    if (!empty(cinfo)) {
        char* err = *reinterpret_cast<char**>(*reinterpret_cast<char**>(ent + 0x20) + 0);
        *reinterpret_cast<int*>(err + 0x14) = 0x16;
        char* cinfo2 = *reinterpret_cast<char**>(ent + 0x20);
        ErrExit_t error_exit = *reinterpret_cast<ErrExit_t*>(*reinterpret_cast<char**>(cinfo2 + 0) + 0);
        error_exit(cinfo2);
    }
    *reinterpret_cast<void**>(ent + 0x10) = *reinterpret_cast<void**>(dest + 0);
    *reinterpret_cast<unsigned*>(ent + 0x14) = *reinterpret_cast<unsigned*>(dest + 4);
}

// 0x0049aa30  netNameOf(id): find the slot of the 16-entry table at this+0x170 (stride 0x58, 0x0049aa37 /
// 0x0049aa42) whose dword +0 equals `id` (0x0049aa3d) and return the address of its name field. The scan
// compares slot 0 first and stops at index 0x10 (0x0049aa45 cmp / 0x0049aa48 jl); index 0x10 means no slot
// matched and the return is NULL (0x0049aa4a cmp / 0x0049aa4e jne / 0x0049aa50). A hit returns
// this + index * 0x58 + 0x185 (0x0049aa55..0x0049aa5b: index * 0xb * 8 added to this with the displacement
// 0x185), i.e. offset 0x15 inside the matched slot.
typedef char*(__fastcall* NetNameOf_t)(void* ecx, void* edx, int id);
NetNameOf_t NetNameOf_orig;
char* __fastcall NetNameOf_re(void* thisp, void*, int id) {
    char* self = reinterpret_cast<char*>(thisp);
    int i = 0;
    while (*reinterpret_cast<int*>(self + 0x170 + i * 0x58) != id) {
        i++;
        if (i >= 0x10) break;
    }
    if (i == 0x10) return 0;
    return self + i * 0x58 + 0x185;
}

// 0x00473df0  Surface_drawRegion(s, x, y, s2): forward a blit to the device surface behind `this`, offset by the
// surface origin. A non-zero this+8 means the surface is locked or invalid and the result is 0x18
// (0x00473df0/0x00473df6/0x00473df8); a NULL source `s` gives 0x10 (0x00473e05/0x00473e07/0x00473e09); a NULL
// device on either side (this+4 at 0x00473e12/0x00473e17, s+4 at 0x00473e19/0x00473e1e) gives 7 (0x00473e54).
// Otherwise slot +0x5c of the device's vtable (0x00473e3c/0x00473e4b) is called with this+4 in ecx and the
// arguments (s->m_4, this+0x20 + x, this+0x24 + y, s2 ? s2->m_4 : 0) pushed in reverse at
// 0x00473e35..0x00473e48; the optional mask surface is dereferenced only when s2 is non-NULL
// (0x00473e24/0x00473e26/0x00473e28). The return value is the device call's.
typedef int(__fastcall* Blt_t)(void* ecx, void* edx, void* srcDev, int x, int y, void* maskDev);
typedef int(__fastcall* DrawRegion_t)(void* ecx, void* edx, void* s, int x, int y, void* s2);
DrawRegion_t SurfaceDrawRegion_orig;
int __fastcall SurfaceDrawRegion_re(void* thisp, void*, void* sp, int x, int y, void* s2p) {
    char* self = reinterpret_cast<char*>(thisp);
    if (*reinterpret_cast<int*>(self + 8) != 0) return 0x18;
    if (sp == 0) return 0x10;
    char* dev = *reinterpret_cast<char**>(self + 4);
    if (dev == 0) return 7;
    char* srcDev = *reinterpret_cast<char**>(reinterpret_cast<char*>(sp) + 4);
    if (srcDev == 0) return 7;
    void* maskDev = s2p ? *reinterpret_cast<void**>(reinterpret_cast<char*>(s2p) + 4) : 0;
    void** vtbl = *reinterpret_cast<void***>(dev);
    Blt_t blt = reinterpret_cast<Blt_t>(vtbl[0x5c / 4]);
    return blt(dev, 0, srcDev, *reinterpret_cast<int*>(self + 0x20) + x,
               *reinterpret_cast<int*>(self + 0x24) + y, maskDev);
}

// 0x00475b60  DC_call84(h, a, b): forward to slot +0x84 of the device vtable. The device is this+4
// (0x00475b60); a NULL device returns 7 (0x00475b63/0x00475b65/0x00475b67) and a NULL handle argument returns
// 0x10 (0x00475b6f/0x00475b75/0x00475b77). Otherwise the handle's own payload h+4 (0x00475b7f) and the two
// integer arguments are pushed in reverse (0x00475b85..0x00475b8f) and the slot is called with the device in ecx
// (0x00475b82/0x00475b90); its return value is returned unchanged.
typedef int(__fastcall* Call84_t)(void* ecx, void* edx, void* payload, int a, int b);
typedef int(__fastcall* DcCall84_t)(void* ecx, void* edx, void* h, int a, int b);
DcCall84_t DcCall84_orig;
int __fastcall DcCall84_re(void* thisp, void*, void* h, int a, int b) {
    char* dev = *reinterpret_cast<char**>(reinterpret_cast<char*>(thisp) + 4);
    if (dev == 0) return 7;
    if (h == 0) return 0x10;
    void* payload = *reinterpret_cast<void**>(reinterpret_cast<char*>(h) + 4);
    void** vtbl = *reinterpret_cast<void***>(dev);
    Call84_t f = reinterpret_cast<Call84_t>(vtbl[0x84 / 4]);
    return f(dev, 0, payload, a, b);
}

// 0x00475b20  Surface_box(x, y, w, h): build a RECT from an origin and a size on the stack and hand it to
// 0x00475b00. left = x and top = y are stored at [esp+4] and [esp+8] (0x00475b30/0x00475b3e), right = x + w and
// bottom = y + h at [esp+0xc] and [esp+0x10] (0x00475b34/0x00475b36 and 0x00475b42/0x00475b44); the address of
// that RECT is the single argument of the call at 0x00475b4d, made with the unchanged `this` in ecx. 0x00475b00
// returns 0x18 when this+4 is NULL and otherwise the result of vtable slot +0x34 of this+4.
typedef int(__fastcall* RectCall_t)(void* ecx, void* edx, void* rect);
const RectCall_t kRect475b00 = reinterpret_cast<RectCall_t>(0x00475b00);   // Widget_fillRectR (C3, c3ab)
typedef int(__fastcall* SurfaceBox_t)(void* ecx, void* edx, int x, int y, int w, int h);
SurfaceBox_t SurfaceBox_orig;
int __fastcall SurfaceBox_re(void* thisp, void*, int x, int y, int w, int h) {
    long rect[4];
    rect[0] = x;
    rect[1] = y;
    rect[2] = x + w;
    rect[3] = y + h;
    return kRect475b00(thisp, 0, rect);
}

// ---- build-palette hit tests (pure functions of the cursor position) ------------------------------------------
// approxDistance(dx, dy) 0x00467170 (C3, golf_math.cpp): |dx| and |dy|, then (min + 2 * max) / 2.
typedef int(__cdecl* Dist_t)(int, int);
const Dist_t kApproxDistance = reinterpret_cast<Dist_t>(0x00467170);

// 0x004340a0  hitBuildingTool(x, y): which cell of the building palette the point (x, y) is in. Two fixed
// anchors are tested first: the point is within 0x10 of (0xec, 0x20e) -> -2 (0x004340ab..0x004340c4, returned at
// 0x004340c8) and within 0xc of (0x115, 0x237) -> -3 (0x004340cf..0x004340e8, returned at 0x004340ec). The nine
// cells are then walked with a single running offset: the first probe is (x - 0x17f, 2 * y - 0x426)
// (0x004340f5/0x004340fb, the y axis doubled) and a cell is hit when approxDistance of that offset is below 0x28
// (0x0043410c/0x0043410f), which returns the cell index (0x00434132). After each of the cells 0..3 and 5..8 the
// x offset moves by 0x50 (0x00434116); after cell 4 the row changes instead, y by 0x50 and x back by 0x118
// (0x00434114/0x0043411b/0x0043411e), so the palette is two rows of five and four cells. Nine cells are tested
// (0x00434125/0x00434128) and a point in none of them gives -1 (0x0043412c).
typedef int(__cdecl* Hit_t)(int, int);
Hit_t HitBuildingTool_orig;
int __cdecl HitBuildingTool_re(int x, int y) {
    if (kApproxDistance(x - 0xec, y - 0x20e) < 0x10) return -2;
    if (kApproxDistance(x - 0x115, y - 0x237) < 0xc) return -3;
    int sx = x - 0x17f;
    int sy = 2 * y - 0x426;
    for (int i = 0; i < 9; i++) {
        if (kApproxDistance(sx, sy) < 0x28) return i;
        if (i == 4) { sy -= 0x50; sx += 0x118; }
        else { sx -= 0x50; }
    }
    return -1;
}

// 0x00433c60  hitBuildTool433c60(x, y): which tool of the build palette the point (x, y) is in. Unlike
// 0x004340a0 every anchor is tested, and a later hit overwrites an earlier one: the result starts at -1
// (0x00433c72) and becomes -2 within 0x10 of (0x10e, 0x1f3) (0x00433c84/0x00433c87/0x00433c89), 0 within 0x19 of
// (0x160, 0x223) (0x00433ca5/0x00433caa), 1 within 0x19 of (0x1d3, 0x223) (0x00433cbc/0x00433cc1), 2 within 0x19
// of (0x24b, 0x223) (0x00433cd6/0x00433cdb) and 3 within 0x10 of (0x2cb, 0x223) (0x00433cf0/0x00433cf6). The
// four middle anchors share the y offset y - 0x223 held in edi (0x00433c8f). A last anchor overrides all of
// them: within 0xc of (0x10b, 0x231) the result is -3 (0x00433cfb..0x00433d19), otherwise the value built so far
// is returned (0x00433d1b).
Hit_t HitBuildTool433c60_orig;
int __cdecl HitBuildTool433c60_re(int x, int y) {
    int r = -1;
    if (kApproxDistance(x - 0x10e, y - 0x1f3) < 0x10) r = -2;
    int ey = y - 0x223;
    if (kApproxDistance(x - 0x160, ey) < 0x19) r = 0;
    if (kApproxDistance(x - 0x1d3, ey) < 0x19) r = 1;
    if (kApproxDistance(x - 0x24b, ey) < 0x19) r = 2;
    if (kApproxDistance(x - 0x2cb, ey) < 0x10) r = 3;
    if (kApproxDistance(x - 0x10b, y - 0x231) < 0xc) return -3;
    return r;
}

}  // namespace

// 0x004aec80  jpeg_std_error
SG_HOOK("golf_clean.exe", 0x004aec80, jpeg_std_error, StdError_re, StdError_orig);
// 0x004b0860  start_iMCU_row
SG_HOOK("golf_clean.exe", 0x004b0860, start_iMCU_row, StartIMcuRow_re, StartIMcuRow_orig);
// 0x004ae8b0  add_huff_table
SG_HOOK("golf_clean.exe", 0x004ae8b0, add_huff_table, AddHuffTable_re, AddHuffTable_orig);
// 0x004afc80  emit_jfif_app0
SG_HOOK("golf_clean.exe", 0x004afc80, emit_jfif_app0, EmitJfifApp0_re, EmitJfifApp0_orig);
// 0x004afd20  emit_adobe_app14
SG_HOOK("golf_clean.exe", 0x004afd20, emit_adobe_app14, EmitAdobeApp14_re, EmitAdobeApp14_orig);
// 0x004b1b70  flush_bits
SG_HOOK("golf_clean.exe", 0x004b1b70, flush_bits, FlushBits_re, FlushBits_orig);
// 0x004b2420  dump_buffer_4b2420
SG_HOOK("golf_clean.exe", 0x004b2420, dump_buffer_4b2420, DumpPhuff_re, DumpPhuff_orig);
// 0x0049aa30  netNameOf
SG_HOOK("golf_clean.exe", 0x0049aa30, netNameOf, NetNameOf_re, NetNameOf_orig);
// 0x00473df0  Surface_drawRegion
SG_HOOK("golf_clean.exe", 0x00473df0, Surface_drawRegion, SurfaceDrawRegion_re, SurfaceDrawRegion_orig);
// 0x00475b60  DC_call84
SG_HOOK("golf_clean.exe", 0x00475b60, DC_call84, DcCall84_re, DcCall84_orig);
// 0x00475b20  Surface_box
SG_HOOK("golf_clean.exe", 0x00475b20, Surface_box, SurfaceBox_re, SurfaceBox_orig);
// 0x004340a0  hitBuildingTool
SG_HOOK("golf_clean.exe", 0x004340a0, hitBuildingTool, HitBuildingTool_re, HitBuildingTool_orig);
// 0x00433c60  hitBuildTool433c60
SG_HOOK("golf_clean.exe", 0x00433c60, hitBuildTool433c60, HitBuildTool433c60_re, HitBuildTool433c60_orig);

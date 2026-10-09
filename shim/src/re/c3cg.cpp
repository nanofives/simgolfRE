// C3 batch c3cg (round 20, 2026-10-09) of golf_clean.exe. Eleven functions the image reaches only through a
// stored pointer: nine IJG libjpeg 6a method pointers installed by jinit_marker_writer 0x004afaf0,
// jinit_downsampler 0x004b4040, jinit_color_converter 0x004b4a60 and jinit_memory_mgr 0x004aee30, plus two
// virtual methods installed into the View vtable family (the window border hit test, slot 80, and the
// child-list focus setter, slot 75).
// Snd::flags 0x00484ff0 was in this batch's candidate list and is NOT here: shim/src/re/c3f.cpp already hooks
// that address (round 2). This batch only adds a registry key that exercises c3f's body on the arm its own
// fixture never ran, the virtual call at 0x00484ffe through the sub-object at +0x40.
//
// Every body is hand-written from the disassembly (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr>
// --list) and cites the address of each instruction it reproduces. __thiscall is emulated with __fastcall
// (ecx = this, edx unused). Callees run through their original addresses:
//   write_any_marker     -> emit_marker 0x004afbf0, emit_2bytes 0x004afc10, emit_byte 0x004afbb0 (C3, c3af)
//   write_file_header    -> emit_marker 0x004afbf0 (C3, c3af), emit_jfif_app0 0x004afc80 and
//                           emit_adobe_app14 0x004afd20 (C3, c3be)
//   write_frame_header   -> emit_dqt 0x004afec0 and emit_sof 0x004affa0 (C3, c3aa)
//   write_tables_only    -> emit_marker 0x004afbf0 (C3, c3af), emit_dqt 0x004afec0 and emit_dht 0x004b0120
//                           (C3, c3aa)
//   fullsize_downsample  -> jcopy_sample_rows 0x004b04f0 and expand_right_edge 0x004b4390 (C3, c3w)
//   request_virt_sarray / request_virt_barray -> alloc_small 0x004aef70 (C2, hooked by no batch)
// The pointers the rest dispatch through (the libjpeg error manager's error_exit at err+0 and emit_message at
// err+4, the destination manager's empty_output_buffer at dest+0xc, the sound sub-object's vtable slot +0x70
// and the focus item's vtable slot +0xf0) are the fixtures' own callbacks, not hooks.
// The only game global any of these bodies touches is 0x0083ab98, which setFocusItem writes: the three
// instructions that reference it in the whole image are 0x0047aec1 here and 0x0047af96 / 0x0047b03a inside
// Container::cycleSelection 0x0047af60. It is a state region of the setFocusItem keys, so diff_hook restores it.
//
//   write_any_marker 0x004afb60     write_file_header 0x004afc40    write_frame_header 0x004afdc0
//   write_tables_only 0x004b0320    fullsize_downsample 0x004b43f0  rgb_gray_convert 0x004b4eb0
//   grayscale_convert 0x004b50d0    request_virt_sarray 0x004af2d0  request_virt_barray 0x004af340
//   Window::hitTestBorder 0x0047ed30  setFocusItem 0x0047ae80
#include "hooks.h"

namespace {

// ---- field helpers ---------------------------------------------------------------------------------------
static inline void* field_ptr(void* base, unsigned off) {
    return *reinterpret_cast<void**>(reinterpret_cast<char*>(base) + off);
}
static inline int& field_i(void* base, unsigned off) {
    return *reinterpret_cast<int*>(reinterpret_cast<char*>(base) + off);
}
static inline unsigned& field_u(void* base, unsigned off) {
    return *reinterpret_cast<unsigned*>(reinterpret_cast<char*>(base) + off);
}
// The dword at base+off read as a value of type T (a function pointer where the original does `call [r+off]`).
template <typename T> static inline T F(void* base, unsigned off) {
    return *reinterpret_cast<T*>(reinterpret_cast<char*>(base) + off);
}
// `mov eax, [obj]` then `call [eax+off]`: the object's vtable pointer is at +0, the slot at +off.
static inline void* vslot(void* obj, unsigned off) {
    return field_ptr(field_ptr(obj, 0), off);
}

// ---- IJG libjpeg 6a structures reached through pointers ---------------------------------------------------
// jpeg_compress_struct: err +0 (jpeg_error_mgr: error_exit +0, emit_message +4, msg_code +0x14,
// msg_parm.i[0] +0x18), mem +4, dest +0x14, image_width +0x18, input_components +0x20, data_precision +0x30,
// num_components +0x34, jpeg_color_space +0x38, comp_info +0x3c, quant_tbl_ptrs[4] +0x40,
// dc_huff_tbl_ptrs[4] +0x50, ac_huff_tbl_ptrs[4] +0x60, arith_code +0xac, write_JFIF_header +0xc8,
// write_Adobe_marker +0xd4, progressive_mode +0xdc, max_v_samp_factor +0xe4, cconvert +0x158.
// jpeg_component_info: quant_tbl_no +0x10, dc_tbl_no +0x14, ac_tbl_no +0x18, width_in_blocks +0x1c.
// my_memory_mgr: the public jpeg_memory_mgr first (alloc_small +0 ... max_memory_to_use +0x2c), then
// small_list[2] +0x30, large_list[2] +0x38, virt_sarray_list +0x40, virt_barray_list +0x44,
// total_space_allocated +0x48 (the offsets alloc_small 0x004aef70 uses at 0x004aefd3 / 0x004af051).
// jvirt_sarray_control / jvirt_barray_control: mem_buffer +0, rows_in_array +4, samplesperrow (or
// blocksperrow) +8, maxaccess +0xc, pre_zero +0x20, b_s_open +0x28, next +0x2c, size 0x80.
typedef void(__cdecl* ErrExit_t)(void* cinfo);
typedef void(__cdecl* EmitMessage_t)(void* cinfo, int msg_level);

// ERREXIT1: the message code plus msg_parm.i[0], then cinfo->err->error_exit(cinfo). Each half re-reads
// cinfo+0, exactly as the original does.
static inline void errexit1(void* cinfo, int code, int p0) {
    field_i(field_ptr(cinfo, 0), 0x14) = code;
    field_i(field_ptr(cinfo, 0), 0x18) = p0;
    F<ErrExit_t>(field_ptr(cinfo, 0), 0)(cinfo);
}

// Callees reached through their original addresses.
typedef void(__cdecl* EmitMarker_t)(void* cinfo, int marker);
const EmitMarker_t kEmitMarker = reinterpret_cast<EmitMarker_t>(0x004afbf0);        // C3, c3af
typedef void(__cdecl* Emit2Bytes_t)(void* cinfo, int value);
const Emit2Bytes_t kEmit2Bytes = reinterpret_cast<Emit2Bytes_t>(0x004afc10);        // C3, c3af
typedef void(__cdecl* EmitByte_t)(void* cinfo, int val);
const EmitByte_t kEmitByte = reinterpret_cast<EmitByte_t>(0x004afbb0);              // C3, c3af
typedef void(__cdecl* VoidCinfo_t)(void* cinfo);
const VoidCinfo_t kEmitJfifApp0 = reinterpret_cast<VoidCinfo_t>(0x004afc80);        // C3, c3be
const VoidCinfo_t kEmitAdobeApp14 = reinterpret_cast<VoidCinfo_t>(0x004afd20);      // C3, c3be
typedef int(__cdecl* EmitDqt_t)(void* cinfo, int index);
const EmitDqt_t kEmitDqt = reinterpret_cast<EmitDqt_t>(0x004afec0);                 // C3, c3aa
const EmitMarker_t kEmitSof = reinterpret_cast<EmitMarker_t>(0x004affa0);           // C3, c3aa
typedef void(__cdecl* EmitDht_t)(void* cinfo, int index, int is_ac);
const EmitDht_t kEmitDht = reinterpret_cast<EmitDht_t>(0x004b0120);                 // C3, c3aa
typedef void(__cdecl* JcopySampleRows_t)(void** input_array, int source_row, void** output_array,
                                         int dest_row, int num_rows, unsigned num_cols);
const JcopySampleRows_t kJcopySampleRows = reinterpret_cast<JcopySampleRows_t>(0x004b04f0);   // C3, c3w
typedef void(__cdecl* ExpandRightEdge_t)(void** image_data, int num_rows, unsigned input_cols,
                                         unsigned output_cols);
const ExpandRightEdge_t kExpandRightEdge = reinterpret_cast<ExpandRightEdge_t>(0x004b4390);   // C3, c3w
typedef void*(__cdecl* AllocSmall_t)(void* cinfo, int pool_id, unsigned sizeofobject);
const AllocSmall_t kAllocSmall = reinterpret_cast<AllocSmall_t>(0x004aef70);        // C2

// 0x004afb60  write_any_marker(cinfo, marker, dataptr, datalen): emit one marker segment whose payload is the
// caller's byte run. The length is tested UNSIGNED against 0xfffd (0x004afb65 / 0x004afb6c) and a longer run
// emits nothing at all. Otherwise the marker byte goes out through emit_marker 0x004afbf0 (0x004afb78), the
// segment length (payload plus the two length bytes) through emit_2bytes 0x004afc10 (0x004afb7d / 0x004afb82),
// and each payload byte through emit_byte 0x004afbb0 (0x004afb9f). The loop is a post-decrement count: the
// value is copied before the decrement and tested for zero (0x004afb8a / 0x004afb8c / 0x004afb8f), so a zero
// length emits the marker and the length and no payload, and the counter re-read at 0x004afb92 is the original
// length. Each byte is zero-extended from the payload (0x004afb99 / 0x004afb9b) and the pointer advances by
// one (0x004afba7).
typedef void(__cdecl* WriteAnyMarker_t)(void* cinfo, int marker, const unsigned char* dataptr,
                                        unsigned datalen);
WriteAnyMarker_t WriteAnyMarker_orig;
void __cdecl WriteAnyMarker_re(void* cinfo, int marker, const unsigned char* dataptr, unsigned datalen) {
    if (datalen <= 0xfffdu) {                                       // 0x004afb65 / 0x004afb6c
        kEmitMarker(cinfo, marker);                                 // 0x004afb78
        kEmit2Bytes(cinfo, static_cast<int>(datalen + 2));          // 0x004afb7d / 0x004afb82
        while (datalen--) {                                         // 0x004afb8a / 0x004afb8f / 0x004afba8
            kEmitByte(cinfo, *dataptr);                             // 0x004afb9b / 0x004afb9f
            dataptr++;                                              // 0x004afba7
        }
    }
}

// 0x004afc40  write_file_header(cinfo): open a JPEG datastream. The SOI marker 0xd8 always goes out
// (0x004afc45 / 0x004afc4b); the JFIF APP0 segment follows only when write_JFIF_header (cinfo+0xc8) is
// non-zero (0x004afc50 / 0x004afc5b / 0x004afc5e) and the Adobe APP14 segment only when write_Adobe_marker
// (cinfo+0xd4) is non-zero (0x004afc66 / 0x004afc6e / 0x004afc71). Both flags are read as full dwords and the
// two tests are independent, so all four combinations are reachable.
VoidCinfo_t WriteFileHeader_orig;
void __cdecl WriteFileHeader_re(void* cinfo) {
    kEmitMarker(cinfo, 0xd8);                                       // 0x004afc45 / 0x004afc4b
    if (field_i(cinfo, 0xc8) != 0) {                                // 0x004afc50 / 0x004afc5b
        kEmitJfifApp0(cinfo);                                       // 0x004afc5e
    }
    if (field_i(cinfo, 0xd4) != 0) {                                // 0x004afc66 / 0x004afc6e
        kEmitAdobeApp14(cinfo);                                     // 0x004afc71
    }
}

// 0x004afdc0  write_frame_header(cinfo): emit every quantisation table the components use, then the one
// frame header marker that describes the file.
// The quantisation pass walks comp_info (read once at 0x004afdcd) with stride 0x54 from field +0x10
// (quant_tbl_no, 0x004afdd6 / 0x004afdeb) and accumulates the precision flags emit_dqt 0x004afec0 returns
// (0x004afde5); num_components (cinfo+0x34) is read before the loop (0x004afdca) and re-read at the bottom of
// every iteration (0x004afde7 / 0x004afdee).
// The baseline decision: a non-zero arith_code (cinfo+0xac, 0x004afdf2), a non-zero progressive_mode
// (cinfo+0xdc, 0x004afdfc) or a data_precision (cinfo+0x30) other than 8 (0x004afe06) all jump to 0x004afe50,
// which clears the flag. Otherwise the flag starts set (0x004afe12 / 0x004afe19) and a second walk over
// num_components (re-read at 0x004afe0c) clears it for any component whose dc_tbl_no (+0x14, 0x004afe20) or
// ac_tbl_no (+0x18, 0x004afe25) is greater than 1, both compares SIGNED. A 16-bit quantisation table found by
// the first pass then also clears it (0x004afe31 / 0x004afe35 / 0x004afe3b) after storing message code 0x4a in
// err->msg_code (0x004afe3f) and calling err->emit_message(cinfo, 0) (0x004afe46 / 0x004afe48).
// The marker is chosen in a fixed order, each arm re-reading its flag: arith_code gives 0xc9 (0x004afe52 /
// 0x004afe5c), else progressive_mode gives 0xc2 (0x004afe6f / 0x004afe79), else the baseline flag gives 0xc0
// (0x004afe8c / 0x004afe90) and anything else 0xc1 (0x004afea3), each through emit_sof 0x004affa0.
VoidCinfo_t WriteFrameHeader_orig;
void __cdecl WriteFrameHeader_re(void* cinfop) {
    char* cinfo = reinterpret_cast<char*>(cinfop);
    int prec = 0;                                                   // 0x004afdc8
    int ci = 0;                                                     // 0x004afdd0
    char* comp = reinterpret_cast<char*>(field_ptr(cinfo, 0x3c));   // 0x004afdcd
    if (field_i(cinfo, 0x34) > 0) {                                 // 0x004afdca / 0x004afdd4
        char* q = comp + 0x10;                                      // 0x004afdd6
        do {
            prec += kEmitDqt(cinfo, *reinterpret_cast<int*>(q));    // 0x004afdd9 / 0x004afddd / 0x004afde5
            ci++;                                                   // 0x004afdea
            q += 0x54;                                              // 0x004afdeb
        } while (ci < field_i(cinfo, 0x34));                        // 0x004afde7 / 0x004afdee / 0x004afdf0
    }
    int is_baseline;
    if (field_i(cinfo, 0xac) != 0 || field_i(cinfo, 0xdc) != 0 ||   // 0x004afdf2 / 0x004afdfc
        field_i(cinfo, 0x30) != 8) {                                // 0x004afe06
        is_baseline = 0;                                            // 0x004afe50
    } else {
        is_baseline = 1;                                            // 0x004afe12 / 0x004afe19
        int n = field_i(cinfo, 0x34);                               // 0x004afe0c
        char* p = reinterpret_cast<char*>(field_ptr(cinfo, 0x3c));  // 0x004afe0f
        while (n != 0) {                                            // 0x004afe17 / 0x004afe2e / 0x004afe2f
            if (*reinterpret_cast<int*>(p + 0x14) > 1 ||            // 0x004afe20 / 0x004afe23
                *reinterpret_cast<int*>(p + 0x18) > 1) {            // 0x004afe25 / 0x004afe27
                is_baseline = 0;                                    // 0x004afe29
            }
            p += 0x54;                                              // 0x004afe2b
            n--;                                                    // 0x004afe2e
        }
        if (prec != 0 && is_baseline != 0) {                        // 0x004afe31 / 0x004afe35
            is_baseline = 0;                                        // 0x004afe3b
            field_i(field_ptr(cinfo, 0), 0x14) = 0x4a;              // 0x004afe39 / 0x004afe3f
            F<EmitMessage_t>(field_ptr(cinfo, 0), 4)(cinfo, 0);     // 0x004afe46 / 0x004afe48
        }
    }
    if (field_i(cinfo, 0xac) != 0) {                                // 0x004afe52 / 0x004afe5a
        kEmitSof(cinfo, 0xc9);                                      // 0x004afe5c / 0x004afe62
    } else if (field_i(cinfo, 0xdc) != 0) {                         // 0x004afe6f / 0x004afe77
        kEmitSof(cinfo, 0xc2);                                      // 0x004afe79 / 0x004afe7f
    } else if (is_baseline != 0) {                                  // 0x004afe8c / 0x004afe8e
        kEmitSof(cinfo, 0xc0);                                      // 0x004afe90 / 0x004afe96
    } else {
        kEmitSof(cinfo, 0xc1);                                      // 0x004afea3 / 0x004afea9
    }
}

// 0x004b0320  write_tables_only(cinfo): emit an abbreviated datastream that carries nothing but the tables.
// SOI 0xd8 opens it (0x004b0327 / 0x004b032d) and EOI 0xd9 closes it (0x004b038e / 0x004b0394), both through
// emit_marker 0x004afbf0. In between, the four quant_tbl_ptrs slots (cinfo+0x40 upward, 0x004b0337 /
// 0x004b033a / 0x004b034a) produce one emit_dqt 0x004afec0 call each for every non-null pointer (0x004b0341),
// and - only when arith_code (cinfo+0xac) is zero (0x004b0352 / 0x004b035a) - the four DC slots (cinfo+0x50
// upward, read as [ebx-0x10] with ebx walking from cinfo+0x60, 0x004b035e / 0x004b0361) and the four AC slots
// (cinfo+0x60 upward, 0x004b0374) produce an emit_dht 0x004b0120 call each with is_ac 0 (0x004b036c) and 1
// (0x004b037d). Both loops run the index 0..3 (0x004b034d / 0x004b0389) and the DC and AC tests of one index
// are independent.
VoidCinfo_t WriteTablesOnly_orig;
void __cdecl WriteTablesOnly_re(void* cinfop) {
    char* cinfo = reinterpret_cast<char*>(cinfop);
    kEmitMarker(cinfo, 0xd8);                                       // 0x004b0327 / 0x004b032d
    for (int i = 0; i < 4; i++) {                                   // 0x004b0335 / 0x004b034d / 0x004b0350
        if (field_ptr(cinfo, 0x40 + i * 4) != 0) {                  // 0x004b033a / 0x004b033d
            kEmitDqt(cinfo, i);                                     // 0x004b0341
        }
    }
    if (field_i(cinfo, 0xac) == 0) {                                // 0x004b0352 / 0x004b035a
        for (int i = 0; i < 4; i++) {                               // 0x004b035c / 0x004b0389 / 0x004b038c
            if (field_ptr(cinfo, 0x50 + i * 4) != 0) {              // 0x004b0361 / 0x004b0366
                kEmitDht(cinfo, i, 0);                              // 0x004b0368 / 0x004b036c
            }
            if (field_ptr(cinfo, 0x60 + i * 4) != 0) {              // 0x004b0374 / 0x004b0377
                kEmitDht(cinfo, i, 1);                              // 0x004b0379 / 0x004b037d
            }
        }
    }
    kEmitMarker(cinfo, 0xd9);                                       // 0x004b038e / 0x004b0394
}

// 0x004b43f0  fullsize_downsample(cinfo, compptr, input_data, output_data): the downsampling method installed
// for a component that is not subsampled. It copies max_v_samp_factor (cinfo+0xe4) rows of image_width
// (cinfo+0x18) samples from row 0 of the input array to row 0 of the output array through
// jcopy_sample_rows 0x004b04f0 (the six arguments pushed in reverse at 0x004b4407..0x004b440e), then pads each
// of those rows out to width_in_blocks (compptr+0x1c) times 8 samples through expand_right_edge 0x004b4390
// (0x004b441b / 0x004b4424 / 0x004b4427..0x004b442b). Both cinfo fields are read again for the second call
// (0x004b4418 / 0x004b441e). The function has no branch of its own.
typedef void(__cdecl* FullsizeDownsample_t)(void* cinfo, void* compptr, void** input_data, void** output_data);
FullsizeDownsample_t FullsizeDownsample_orig;
void __cdecl FullsizeDownsample_re(void* cinfo, void* compptr, void** input_data, void** output_data) {
    kJcopySampleRows(input_data, 0, output_data, 0,
                     field_i(cinfo, 0xe4), field_u(cinfo, 0x18));   // 0x004b43fa / 0x004b43fd / 0x004b440f
    kExpandRightEdge(output_data, field_i(cinfo, 0xe4), field_u(cinfo, 0x18),
                     field_i(compptr, 0x1c) << 3);                  // 0x004b4418 / 0x004b441b / 0x004b442b
}

// 0x004b4eb0  rgb_gray_convert(cinfo, input_buf, output_buf, output_row, num_rows): the colour conversion
// method that turns interleaved three-byte pixels into one greyscale component. The conversion table is the
// dword at cconvert+8, cconvert being cinfo+0x158 (0x004b4eb6 / 0x004b4ec7), and the row length is image_width
// (cinfo+0x18, 0x004b4ebc).
// The row loop is a pre-decrement SIGNED test (0x004b4eca / 0x004b4ecb), so a num_rows of 0 or less copies
// nothing; the remaining count is kept and counted down at 0x004b4f51 / 0x004b4f56. Each pass takes the next
// input row pointer (0x004b4eef / 0x004b4ef1) and the output row output_buf[0][output_row] (0x004b4ef8 /
// 0x004b4efc / 0x004b4efe), output_row advancing by one element per row (0x004b4ee8).
// The column loop is UNSIGNED against the row length (0x004b4f08 / 0x004b4f45 / 0x004b4f47). Each pixel reads
// three bytes (0x004b4f0e / 0x004b4f17 / 0x004b4f1e) and advances the input by three (0x004b4f10); the output
// byte is the sum of table[r], table[g+0x100] and table[b+0x200] (0x004b4f21 / 0x004b4f28 / 0x004b4f2b /
// 0x004b4f36 / 0x004b4f38) shifted right ARITHMETICALLY by 16 (0x004b4f3a) and stored at column index
// (0x004b4f3d).
typedef void(__cdecl* ColorConvert_t)(void* cinfo, unsigned char** input_buf, unsigned char*** output_buf,
                                      unsigned output_row, int num_rows);
ColorConvert_t RgbGrayConvert_orig;
void __cdecl RgbGrayConvert_re(void* cinfop, unsigned char** input_buf, unsigned char*** output_buf,
                               unsigned output_row, int num_rows) {
    char* cinfo = reinterpret_cast<char*>(cinfop);
    const int* ctab = reinterpret_cast<const int*>(
        field_ptr(field_ptr(cinfo, 0x158), 8));                     // 0x004b4eb6 / 0x004b4ec7
    unsigned num_cols = field_u(cinfo, 0x18);                       // 0x004b4ebc
    while (--num_rows >= 0) {                                       // 0x004b4eca / 0x004b4ecb / 0x004b4f51
        const unsigned char* inptr = *input_buf++;                  // 0x004b4eef / 0x004b4ef1
        unsigned char* outptr = output_buf[0][output_row];          // 0x004b4ef8 / 0x004b4efc / 0x004b4efe
        output_row++;                                               // 0x004b4ee8
        for (unsigned col = 0; col < num_cols; col++) {             // 0x004b4f06 / 0x004b4f08 / 0x004b4f45
            unsigned r = inptr[0];                                  // 0x004b4f0e
            unsigned g = inptr[1];                                  // 0x004b4f17
            unsigned b = inptr[2];                                  // 0x004b4f1e
            inptr += 3;                                             // 0x004b4f10
            int sum = ctab[b + 0x200] + ctab[g + 0x100] + ctab[r];  // 0x004b4f21 / 0x004b4f2b / 0x004b4f38
            outptr[col] = static_cast<unsigned char>(sum >> 16);    // 0x004b4f3a / 0x004b4f3d
        }
    }
}

// 0x004b50d0  grayscale_convert(cinfo, input_buf, output_buf, output_row, num_rows): the colour conversion
// method that extracts the first channel of each input pixel. The row length is image_width (cinfo+0x18,
// 0x004b50d6) and the input stride is input_components (cinfo+0x20, 0x004b50d9), which the inner loop reloads
// from its frame slot on every pixel (0x004b511b).
// The row loop is the same pre-decrement SIGNED test as rgb_gray_convert (0x004b50e4 / 0x004b50e5 /
// 0x004b512a / 0x004b512f), the input row pointer advances by one element (0x004b5102 / 0x004b5104) and the
// output row is output_buf[0][output_row] with output_row advancing by one (0x004b5107 / 0x004b510a /
// 0x004b510c). The column loop is UNSIGNED (0x004b5112 / 0x004b5122 / 0x004b5124): it copies one byte
// (0x004b5116 / 0x004b5118) and steps the input pointer by the stride (0x004b511f).
ColorConvert_t GrayscaleConvert_orig;
void __cdecl GrayscaleConvert_re(void* cinfop, unsigned char** input_buf, unsigned char*** output_buf,
                                 unsigned output_row, int num_rows) {
    char* cinfo = reinterpret_cast<char*>(cinfop);
    unsigned num_cols = field_u(cinfo, 0x18);                       // 0x004b50d6
    int instride = field_i(cinfo, 0x20);                            // 0x004b50d9
    while (--num_rows >= 0) {                                       // 0x004b50e4 / 0x004b50e5 / 0x004b512a
        const unsigned char* inptr = *input_buf++;                  // 0x004b5102 / 0x004b5104
        unsigned char* outptr = output_buf[0][output_row];          // 0x004b510a / 0x004b510c
        output_row++;                                               // 0x004b5107
        for (unsigned col = 0; col < num_cols; col++) {             // 0x004b5110 / 0x004b5112 / 0x004b5122
            outptr[col] = *inptr;                                   // 0x004b5116 / 0x004b5118
            inptr += instride;                                      // 0x004b511b / 0x004b511f
        }
    }
}

// 0x004af2d0  request_virt_sarray(cinfo, pool_id, pre_zero, samplesperrow, numrows, maxaccess): reserve a
// sample-array control block that is not backed by memory yet, and push it onto the memory manager's list.
// The memory manager is cinfo+4, read before the pool check (0x004af2de). A pool_id other than 1 raises
// message code 0xc with the pool id in msg_parm.i[0] through err->error_exit (0x004af2db / 0x004af2e1 /
// 0x004af2e6 / 0x004af2ef / 0x004af2f4) and, with a handler that returns, carries on with that same pool id.
// The 0x80-byte control block comes from alloc_small 0x004aef70 (0x004af2f9 / 0x004af300). Its fields are
// written in the order rows_in_array = numrows (0x004af30d), samplesperrow (0x004af314), mem_buffer = NULL
// (0x004af31b), maxaccess (0x004af321), pre_zero (0x004af324), b_s_open = 0 (0x004af327); the block is then
// linked at the head of mem->virt_sarray_list (mem+0x40, 0x004af32e / 0x004af334 / 0x004af337) and returned.
typedef void*(__cdecl* RequestVirtArray_t)(void* cinfo, int pool_id, int pre_zero, unsigned samplesperrow,
                                           unsigned numrows, unsigned maxaccess);
RequestVirtArray_t RequestVirtSarray_orig;
void* __cdecl RequestVirtSarray_re(void* cinfo, int pool_id, int pre_zero, unsigned samplesperrow,
                                   unsigned numrows, unsigned maxaccess) {
    char* mem = reinterpret_cast<char*>(field_ptr(cinfo, 4));       // 0x004af2de
    if (pool_id != 1) {                                             // 0x004af2db / 0x004af2e1
        errexit1(cinfo, 0xc, pool_id);                              // 0x004af2e6 / 0x004af2ef / 0x004af2f4
    }
    char* result = reinterpret_cast<char*>(kAllocSmall(cinfo, pool_id, 0x80));   // 0x004af2f9 / 0x004af300
    field_u(result, 4) = numrows;                                   // 0x004af30d
    field_u(result, 8) = samplesperrow;                             // 0x004af314
    field_i(result, 0) = 0;                                         // 0x004af31b
    field_u(result, 0xc) = maxaccess;                               // 0x004af321
    field_i(result, 0x20) = pre_zero;                               // 0x004af324
    field_i(result, 0x28) = 0;                                      // 0x004af327
    *reinterpret_cast<void**>(result + 0x2c) = field_ptr(mem, 0x40);    // 0x004af32e / 0x004af334
    *reinterpret_cast<void**>(mem + 0x40) = result;                 // 0x004af337
    return result;
}

// 0x004af340  request_virt_barray(cinfo, pool_id, pre_zero, blocksperrow, numrows, maxaccess): the same body
// for coefficient blocks, linking onto mem->virt_barray_list (mem+0x44) instead. The memory manager is read at
// 0x004af34e, the pool check is 0x004af34b / 0x004af351 with message code 0xc at 0x004af356 and the pool id at
// 0x004af35f, the 0x80-byte block comes from alloc_small at 0x004af370, the six field stores are 0x004af37d,
// 0x004af384, 0x004af38b, 0x004af391, 0x004af394 and 0x004af397, and the list link is 0x004af39e / 0x004af3a4
// / 0x004af3a7.
RequestVirtArray_t RequestVirtBarray_orig;
void* __cdecl RequestVirtBarray_re(void* cinfo, int pool_id, int pre_zero, unsigned blocksperrow,
                                   unsigned numrows, unsigned maxaccess) {
    char* mem = reinterpret_cast<char*>(field_ptr(cinfo, 4));       // 0x004af34e
    if (pool_id != 1) {                                             // 0x004af34b / 0x004af351
        errexit1(cinfo, 0xc, pool_id);                              // 0x004af356 / 0x004af35f / 0x004af364
    }
    char* result = reinterpret_cast<char*>(kAllocSmall(cinfo, pool_id, 0x80));   // 0x004af369 / 0x004af370
    field_u(result, 4) = numrows;                                   // 0x004af37d
    field_u(result, 8) = blocksperrow;                              // 0x004af384
    field_i(result, 0) = 0;                                         // 0x004af38b
    field_u(result, 0xc) = maxaccess;                               // 0x004af391
    field_i(result, 0x20) = pre_zero;                               // 0x004af394
    field_i(result, 0x28) = 0;                                      // 0x004af397
    *reinterpret_cast<void**>(result + 0x2c) = field_ptr(mem, 0x44);    // 0x004af39e / 0x004af3a4
    *reinterpret_cast<void**>(mem + 0x44) = result;                 // 0x004af3a7
    return result;
}

// 0x0047ed30  Window::hitTestBorder(this, x, y): classify a point of the window's own coordinate space as one
// of the eight resize zones, as the close box, or as nothing.
// The whole border test is gated on bit 1 of the byte at this+0xa0 (0x0047ed30 / 0x0047ed3c / 0x0047ed3f);
// when it is clear the function goes straight to the second half at 0x0047ee19. The border width is the dword
// at this+0x1f0 (0x0047ed45) and the two far edges are computed from the window rectangle as
// (this+0x1c4) - (this+0x1bc) - width for x (0x0047ed85 / 0x0047ed8b / 0x0047ed91) and
// (this+0x1c8) - (this+0x1c0) - width for y (computed three times, at 0x0047ed66, 0x0047edac and 0x0047edfa).
// Every compare is SIGNED. The corners come first: x below the width and y below it return 0xd (0x0047ed4f /
// 0x0047ed53 / 0x0047ed59), x below and y past the bottom return 0x10 (0x0047ed76 / 0x0047ed7c), x past the
// right and y below return 0xe (0x0047ed95 / 0x0047ed99 / 0x0047ed9f) and x past the right with y past the
// bottom return 0x11 (0x0047edbe / 0x0047edc4). Then the four edges: x below the width 0xa (0x0047edcd /
// 0x0047edd3), y below it 0xc (0x0047eddc / 0x0047ede2), x past the right 0xb (0x0047edeb / 0x0047edf1) and y
// past the bottom 0xf (0x0047ee0a / 0x0047ee10). Note that a point is on an edge only when it is STRICTLY past
// the far edge (jle keeps it inside) but merely below the width on the near side (jge keeps it inside).
// The second half reads the flag dword at this+0x9c (0x0047ee19): bit 0 returns 2 outright (0x0047ee1f /
// 0x0047ee25); otherwise bit 4 makes 2 the answer for any y below this+0x180 and 0 for the rest, without
// looking further (0x0047ee2e / 0x0047ee32 / 0x0047ee3c); otherwise bit 26 makes 2 the answer for any y below
// this+0x1ec (0x0047ee45 / 0x0047ee4c / 0x0047ee56). Anything else is 0 (0x0047ee61).
// The two compares at 0x0047ed64 and 0x0047edaa repeat the compare that reached them on registers nothing has
// written in between, so neither can take its jump.
typedef int(__fastcall* HitTestBorder_t)(void* ecx, void* edx, int x, int y);
HitTestBorder_t HitTestBorder_orig;
int __fastcall HitTestBorder_re(void* thisp, void*, int x, int y) {
    char* self = reinterpret_cast<char*>(thisp);
    if ((*reinterpret_cast<unsigned char*>(self + 0xa0) & 2) != 0) {    // 0x0047ed30 / 0x0047ed3c / 0x0047ed3f
        int w = field_i(self, 0x1f0);                                   // 0x0047ed45
        if (x < w) {                                                    // 0x0047ed4f / 0x0047ed51
            if (y < w) {                                                // 0x0047ed53 / 0x0047ed55
                return 0xd;                                             // 0x0047ed59
            }
            if (y > field_i(self, 0x1c8) - field_i(self, 0x1c0) - w) {  // 0x0047ed66 / 0x0047ed76
                return 0x10;                                            // 0x0047ed7c
            }
        }
        int right = field_i(self, 0x1c4) - field_i(self, 0x1bc) - w;    // 0x0047ed85 / 0x0047ed91 / 0x0047ed93
        if (x > right) {                                                // 0x0047ed95 / 0x0047ed97
            if (y < w) {                                                // 0x0047ed99 / 0x0047ed9b
                return 0xe;                                             // 0x0047ed9f
            }
            if (y > field_i(self, 0x1c8) - field_i(self, 0x1c0) - w) {  // 0x0047edac / 0x0047edbe
                return 0x11;                                            // 0x0047edc4
            }
        }
        if (x < w) {                                                    // 0x0047edcd / 0x0047edcf
            return 0xa;                                                 // 0x0047edd3
        }
        if (y < w) {                                                    // 0x0047eddc / 0x0047edde
            return 0xc;                                                 // 0x0047ede2
        }
        if (x > right) {                                                // 0x0047edeb / 0x0047eded
            return 0xb;                                                 // 0x0047edf1
        }
        if (y > field_i(self, 0x1c8) - field_i(self, 0x1c0) - w) {      // 0x0047edfa / 0x0047ee0a
            return 0xf;                                                 // 0x0047ee10
        }
    }
    int flags = field_i(self, 0x9c);                                    // 0x0047ee19
    if ((flags & 1) != 0) {                                             // 0x0047ee1f / 0x0047ee21
        return 2;                                                       // 0x0047ee25
    }
    if ((flags & 0x10) != 0) {                                          // 0x0047ee2e / 0x0047ee30
        if (y < field_i(self, 0x180)) {                                 // 0x0047ee32 / 0x0047ee38
            return 2;                                                   // 0x0047ee3c
        }
        return 0;                                                       // 0x0047ee61
    }
    if ((flags & 0x4000000) != 0) {                                     // 0x0047ee45 / 0x0047ee4a
        if (y < field_i(self, 0x1ec)) {                                 // 0x0047ee4c / 0x0047ee52
            return 2;                                                   // 0x0047ee56
        }
    }
    return 0;                                                           // 0x0047ee61
}

// 0x0047ae80  setFocusItem(this, item): move the keyboard focus of a view's child list onto one item.
// Nothing happens for a null item (0x0047ae87 / 0x0047ae8c) or a zero child count at this+0x140
// (0x0047ae92 / 0x0047ae9a, a test for EQUAL to zero only). The item that currently holds the focus is the
// one the node at this+0x13c points to in its +4 (0x0047aeaa / 0x0047aeb0), or none at all when the list head
// at this+0x138 is null (0x0047aea0 / 0x0047aea8 / 0x0047aeb5); when it is already the requested item the
// function returns (0x0047aeb7 / 0x0047aeb9). Otherwise it parks that previous item in the global 0x0083ab98
// (0x0047aebf), and a null list head ends the function there (0x0047aec5 / 0x0047aecd).
// The search then rewinds the cursor: the index at this+0x144 is cleared and this+0x13c is set back to the
// head (0x0047aedd / 0x0047aee7), and a child count that is not POSITIVE ends the function (0x0047aedb /
// 0x0047aeed), which is how a negative count passes the first test and fails here. The walk compares each
// node's +4 against the item (0x0047aeef / 0x0047aef5 / 0x0047aef8), and on a miss increments this+0x144,
// follows the node's +0xc link into this+0x13c and stops after the child count iterations (0x0047aefa /
// 0x0047af02 / 0x0047af08 / 0x0047af0b / 0x0047af0d / 0x0047af13), leaving the cursor past the end.
// On a hit the previous item is told it lost the focus, but only when there was one and the child count is
// MORE than 1 (0x0047af1c / 0x0047af20, signed), through its own vtable slot +0xf0 with argument 0
// (0x0047af29 / 0x0047af2d); then the list head and the node the cursor now points at are read again
// (0x0047af33 / 0x0047af3b / 0x0047af3d / 0x0047af43) and that node's item is told it gained the focus with
// argument 1 (0x0047af46 / 0x0047af4a / 0x0047af4e). Both re-reads can fail only if the first notification
// changed the list, which is what the _cb fixture makes it do.
typedef void(__fastcall* FocusNotify_t)(void* ecx, void* edx, int on);   // item vtable slot +0xf0
typedef void(__fastcall* SetFocusItem_t)(void* ecx, void* edx, void* item);
SetFocusItem_t SetFocusItem_orig;
void __fastcall SetFocusItem_re(void* thisp, void*, void* item) {
    char* self = reinterpret_cast<char*>(thisp);
    if (item == 0) { return; }                                      // 0x0047ae87 / 0x0047ae8c
    if (field_i(self, 0x140) == 0) { return; }                      // 0x0047ae92 / 0x0047ae9a
    void* cur;
    if (field_ptr(self, 0x138) != 0) {                              // 0x0047aea0 / 0x0047aea8
        cur = field_ptr(field_ptr(self, 0x13c), 4);                 // 0x0047aeaa / 0x0047aeb0
    } else {
        cur = 0;                                                    // 0x0047aeb5
    }
    if (item == cur) { return; }                                    // 0x0047aeb7 / 0x0047aeb9
    *reinterpret_cast<void**>(0x0083ab98) = cur;                    // 0x0047aebf
    void* head = field_ptr(self, 0x138);                            // 0x0047aec5
    if (head == 0) { return; }                                      // 0x0047aecb / 0x0047aecd
    int n = field_i(self, 0x140);                                   // 0x0047aed3
    int i = 0;                                                      // 0x0047aed9
    field_i(self, 0x144) = 0;                                       // 0x0047aedd
    *reinterpret_cast<void**>(self + 0x13c) = head;                 // 0x0047aee7
    if (n <= 0) { return; }                                         // 0x0047aedb / 0x0047aeed
    for (;;) {
        char* node = reinterpret_cast<char*>(field_ptr(self, 0x13c));   // 0x0047aeef
        if (field_ptr(node, 4) == item) {                           // 0x0047aef5 / 0x0047aef8
            if (cur != 0 && field_i(self, 0x140) > 1) {             // 0x0047af1c / 0x0047af20 / 0x0047af27
                reinterpret_cast<FocusNotify_t>(vslot(cur, 0xf0))(cur, 0, 0);   // 0x0047af29 / 0x0047af2d
            }
            if (field_ptr(self, 0x138) == 0) { return; }            // 0x0047af33 / 0x0047af3b
            void* sel = field_ptr(field_ptr(self, 0x13c), 4);       // 0x0047af3d / 0x0047af43
            if (sel == 0) { return; }                               // 0x0047af46 / 0x0047af48
            reinterpret_cast<FocusNotify_t>(vslot(sel, 0xf0))(sel, 0, 1);   // 0x0047af4a / 0x0047af4e
            return;
        }
        field_i(self, 0x144) = field_i(self, 0x144) + 1;            // 0x0047aefa / 0x0047af00 / 0x0047af02
        i++;                                                        // 0x0047af01
        *reinterpret_cast<void**>(self + 0x13c) = field_ptr(node, 0xc);   // 0x0047af08 / 0x0047af0d
        if (i >= n) { return; }                                     // 0x0047af0b / 0x0047af13
    }
}

}  // namespace

// 0x004afb60  write_any_marker
SG_HOOK("golf_clean.exe", 0x004afb60, write_any_marker, WriteAnyMarker_re, WriteAnyMarker_orig);
// 0x004afc40  write_file_header
SG_HOOK("golf_clean.exe", 0x004afc40, write_file_header, WriteFileHeader_re, WriteFileHeader_orig);
// 0x004afdc0  write_frame_header
SG_HOOK("golf_clean.exe", 0x004afdc0, write_frame_header, WriteFrameHeader_re, WriteFrameHeader_orig);
// 0x004b0320  write_tables_only
SG_HOOK("golf_clean.exe", 0x004b0320, write_tables_only, WriteTablesOnly_re, WriteTablesOnly_orig);
// 0x004b43f0  fullsize_downsample
SG_HOOK("golf_clean.exe", 0x004b43f0, fullsize_downsample, FullsizeDownsample_re, FullsizeDownsample_orig);
// 0x004b4eb0  rgb_gray_convert
SG_HOOK("golf_clean.exe", 0x004b4eb0, rgb_gray_convert, RgbGrayConvert_re, RgbGrayConvert_orig);
// 0x004b50d0  grayscale_convert
SG_HOOK("golf_clean.exe", 0x004b50d0, grayscale_convert, GrayscaleConvert_re, GrayscaleConvert_orig);
// 0x004af2d0  request_virt_sarray
SG_HOOK("golf_clean.exe", 0x004af2d0, request_virt_sarray, RequestVirtSarray_re, RequestVirtSarray_orig);
// 0x004af340  request_virt_barray
SG_HOOK("golf_clean.exe", 0x004af340, request_virt_barray, RequestVirtBarray_re, RequestVirtBarray_orig);
// 0x0047ed30  Window::hitTestBorder
SG_HOOK("golf_clean.exe", 0x0047ed30, Window_hitTestBorder, HitTestBorder_re, HitTestBorder_orig);
// 0x0047ae80  setFocusItem
SG_HOOK("golf_clean.exe", 0x0047ae80, setFocusItem, SetFocusItem_re, SetFocusItem_orig);

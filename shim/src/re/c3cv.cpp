// C3 batch c3cv (round 23, 2026-10-09) of golf_clean.exe: the rest of the IJG libjpeg 6a compress side the exe
// links from original\JPEG.lib: the main controller (jcmainct.c), the coefficient controller (jccoefct.c), the
// progressive Huffman encoder (jcphuff.c) and the preprocessing controller (jcprepct.c).
//
// Every body is hand-written from the disassembly (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr>
// --list) and cites the address of each instruction it reproduces. All are __cdecl. Callees run through their
// original addresses:
//   start_pass_coef / compress_data / compress_output -> start_iMCU_row 0x004b0860 (C3, c3be)
//   compress_data / compress_first_pass               -> jzero_far 0x004b0540 (C3, c3cs)
//   compress_first_pass                               -> compress_output (THIS batch)
//   start_pass_phuff                                  -> jpeg_make_c_derived_tbl 0x004b0f60 (C3, c3cs)
//   emit_restart_4b2460                               -> emit_eobrun 0x004b25d0 (C3, c3bo), flush_bits_4b2510
//                                                        0x004b2510 (C3, c3aa), dump_buffer_4b2420 0x004b2420 (C3, c3be)
//   encode_mcu_DC_first / DC_refine                   -> emit_restart_4b2460 (THIS batch), dump_buffer_4b2420
//   encode_mcu_AC_first                               -> the same plus emit_eobrun
//   encode_mcu_AC_refine                              -> the same plus emit_buffered_bits 0x004b27d0 (C3, c3aa)
//   finish_pass_gather_phuff                          -> emit_eobrun, jpeg_alloc_huff_table 0x004afad0 (C3, c3af),
//                                                        jpeg_gen_optimal_table 0x004b1080 (C3, c3cs)
//   pre_process_data                                  -> expand_bottom_edge 0x004b3d00 (C3, c3cs)
//   pre_process_context                               -> jcopy_sample_rows 0x004b04f0 (C3, c3w), expand_bottom_edge
// Method pointers (error_exit cinfo->err+0, alloc_small cinfo->mem+0, access_virt_barray cinfo->mem+0x20,
// forward_DCT cinfo->fdct+4, encode_mcu cinfo->entropy+4, pre_process_data cinfo->prep+4, compress_data
// cinfo->coef+4, color_convert cinfo->cconvert+4, downsample cinfo->downsample+4) are read from their slots in
// both arms, so the fixture's callbacks are what runs there.
//
//   start_pass_main 0x004b05f0       process_data_simple_main 0x004b0640   start_pass_coef 0x004b07b0
//   compress_data 0x004b08b0         compress_first_pass 0x004b0b20        compress_output 0x004b0d70
//   jinit_phuff_encoder 0x004b1f30   start_pass_phuff 0x004b1f70           encode_mcu_DC_first 0x004b2140
//   emit_restart_4b2460 0x004b2460   encode_mcu_AC_first 0x004b28c0        encode_mcu_DC_refine 0x004b2d30
//   encode_mcu_AC_refine 0x004b2eb0  finish_pass_gather_phuff 0x004b33c0   pre_process_data 0x004b3b80
//   pre_process_context 0x004b3d40
#include "hooks.h"

// Branch coverage for the purpose file's per-jcc table: BR(addr, taken) is `taken`, the condition under which the
// original's jcc at `addr` jumps. Only a coverage build (a local shim/src/re/c3cv_cov.h, never committed, defining
// C3CV_COV) also counts each (addr, taken) pair in a table the probe log/c3/alt/c3cv_cov.py reads back.
#if defined(__has_include)
#if __has_include("c3cv_cov.h")
#include "c3cv_cov.h"
#endif
#endif
#ifdef C3CV_COV
static unsigned g_c3cvCov[2048][2];
static bool c3cv_br(unsigned addr, bool taken) {
    const unsigned key = (addr << 1) | (taken ? 1u : 0u);
    for (unsigned i = (key * 2654435761u) >> 21, n = 0; n < 2048; i = (i + 1) & 2047, n++) {
        if (g_c3cvCov[i][0] == key) { g_c3cvCov[i][1]++; break; }
        if (g_c3cvCov[i][0] == 0) { g_c3cvCov[i][0] = key; g_c3cvCov[i][1] = 1; break; }
    }
    return taken;
}
extern "C" __declspec(dllexport) unsigned* C3cvCovTable() { return &g_c3cvCov[0][0]; }
#define BR(addr, taken) c3cv_br((addr), (taken) ? true : false)
#else
#define BR(addr, taken) ((taken) ? true : false)
#endif

namespace {

// ---- IJG libjpeg 6a structures reached through pointers -------------------------------------------------------
// jpeg_compress_struct: err +0 (error_exit +0, msg_code +0x14, msg_parm.i[0] +0x18), mem +4 (alloc_small +0,
// access_virt_barray +0x20), dest +0x14 (next_output_byte +0, free_in_buffer +4), image_width +0x18,
// image_height +0x1c, num_components +0x34, comp_info +0x3c, dc_huff_tbl_ptrs[4] +0x50, ac_huff_tbl_ptrs[4] +0x60,
// raw_data_in +0xa8, restart_interval +0xc0, max_v_samp_factor +0xe4, total_iMCU_rows +0xe8, comps_in_scan +0xec,
// cur_comp_info[4] +0xf0, MCUs_per_row +0x100, blocks_in_MCU +0x108, MCU_membership[10] +0x10c, Ss +0x134,
// Se +0x138, Ah +0x13c, Al +0x140, main +0x148, prep +0x14c, coef +0x150, cconvert +0x158, downsample +0x15c,
// fdct +0x160, entropy +0x164.
// jpeg_component_info (0x54 bytes): component_index +4, h_samp_factor +8, v_samp_factor +0xc, dc_tbl_no +0x14,
// ac_tbl_no +0x18, width_in_blocks +0x1c, height_in_blocks +0x20, MCU_width +0x34, MCU_height +0x38,
// MCU_sample_width +0x40, last_col_width +0x44, last_row_height +0x48.
// my_main_controller (jcmainct.c): process_data +4, cur_iMCU_row +8, rowgroup_ctr +0xc, suspended +0x10,
// pass_mode +0x14, buffer[] +0x18.
// my_coef_controller (jccoefct.c): compress_data +4, iMCU_row_num +8, mcu_ctr +0xc, MCU_vert_offset +0x10,
// MCU_rows_per_iMCU_row +0x14, MCU_buffer[10] +0x18, whole_image[10] +0x40.
// my_prep_controller (jcprepct.c): color_buf[10] +8, rows_to_go +0x30, next_buf_row +0x34, this_row_group +0x38,
// next_buf_stop +0x3c.
// phuff_entropy_encoder (jcphuff.c, 0x6c bytes): start_pass +0, encode_mcu +4, finish_pass +8,
// gather_statistics +0xc, next_output_byte +0x10, free_in_buffer +0x14, put_buffer +0x18, put_bits +0x1c,
// cinfo +0x20, last_dc_val[4] +0x24, ac_tbl_no +0x34, EOBRUN +0x38, BE +0x3c, bit_buffer +0x40,
// restarts_to_go +0x44, next_restart_num +0x48, derived_tbls[4] +0x4c, count_ptrs[4] +0x5c.
// c_derived_tbl: ehufco[256] +0 (unsigned int), ehufsi[256] +0x400 (char).

static inline char* fp(void* base, unsigned off) {
    return *reinterpret_cast<char**>(reinterpret_cast<char*>(base) + off);
}
static inline void*& fpr(void* base, unsigned off) {
    return *reinterpret_cast<void**>(reinterpret_cast<char*>(base) + off);
}
static inline int& fi(void* base, unsigned off) {
    return *reinterpret_cast<int*>(reinterpret_cast<char*>(base) + off);
}
static inline unsigned& fu(void* base, unsigned off) {
    return *reinterpret_cast<unsigned*>(reinterpret_cast<char*>(base) + off);
}
static inline short& fs(void* base, unsigned off) {
    return *reinterpret_cast<short*>(reinterpret_cast<char*>(base) + off);
}

typedef void(__cdecl* ErrExit_t)(void* cinfo);
// ERREXIT: msg_code into cinfo->err+0x14, then cinfo->err->error_exit(cinfo), re-reading cinfo+0 for each half.
static inline void errexit0(void* cinfo, int code) {
    fi(fp(cinfo, 0), 0x14) = code;
    reinterpret_cast<ErrExit_t>(fp(fp(cinfo, 0), 0))(cinfo);
}
// ERREXIT1: the code plus msg_parm.i[0] (+0x18).
static inline void errexit1(void* cinfo, int code, int p0) {
    fi(fp(cinfo, 0), 0x14) = code;
    fi(fp(cinfo, 0), 0x18) = p0;
    reinterpret_cast<ErrExit_t>(fp(fp(cinfo, 0), 0))(cinfo);
}

// Callees reached through their original addresses.
typedef void(__cdecl* CinfoFn_t)(void* cinfo);
const CinfoFn_t kStartIMcuRow = reinterpret_cast<CinfoFn_t>(0x004b0860);          // start_iMCU_row (C3, c3be)
typedef void(__cdecl* JzeroFar_t)(void* target, unsigned bytes);
const JzeroFar_t kJzeroFar = reinterpret_cast<JzeroFar_t>(0x004b0540);              // jzero_far (C3, c3cs)
typedef int(__cdecl* CompressFn_t)(void* cinfo, void* input_buf);
const CompressFn_t kCompressOutput = reinterpret_cast<CompressFn_t>(0x004b0d70);    // THIS batch
typedef void(__cdecl* EntropyFn_t)(void* entropy);
const EntropyFn_t kEmitEobrun = reinterpret_cast<EntropyFn_t>(0x004b25d0);          // emit_eobrun (C3, c3bo)
const EntropyFn_t kFlushBitsP = reinterpret_cast<EntropyFn_t>(0x004b2510);          // flush_bits_4b2510 (C3, c3aa)
const EntropyFn_t kDumpBufferP = reinterpret_cast<EntropyFn_t>(0x004b2420);         // dump_buffer_4b2420 (C3, c3be)
typedef void(__cdecl* EmitRestartP_t)(void* entropy, int restart_num);
const EmitRestartP_t kEmitRestartP = reinterpret_cast<EmitRestartP_t>(0x004b2460);  // THIS batch
typedef void(__cdecl* EmitBuffered_t)(void* entropy, char* bufstart, unsigned nbits);
const EmitBuffered_t kEmitBufferedBits = reinterpret_cast<EmitBuffered_t>(0x004b27d0);   // C3, c3aa
typedef void*(__cdecl* AllocHuffTable_t)(void* cinfo);
const AllocHuffTable_t kAllocHuffTable = reinterpret_cast<AllocHuffTable_t>(0x004afad0); // C3, c3af
typedef void(__cdecl* GenOptimal_t)(void* cinfo, void* htbl, void* freq);
const GenOptimal_t kGenOptimalTable = reinterpret_cast<GenOptimal_t>(0x004b1080);        // C3, c3cs
typedef void(__cdecl* MakeDerived_t)(void* cinfo, void* htbl, void* pdtbl);
const MakeDerived_t kMakeDerivedTbl = reinterpret_cast<MakeDerived_t>(0x004b0f60);       // C3, c3cs
typedef void(__cdecl* ExpandBottom_t)(void* image_data, unsigned num_cols, int input_rows, int output_rows);
const ExpandBottom_t kExpandBottomEdge = reinterpret_cast<ExpandBottom_t>(0x004b3d00);   // C3, c3cs
typedef void(__cdecl* CopyRows_t)(void* in, int src_row, void* out, int dst_row, int num_rows, unsigned num_cols);
const CopyRows_t kCopySampleRows = reinterpret_cast<CopyRows_t>(0x004b04f0);             // C3, c3w

// Method-pointer types.
typedef void*(__cdecl* AllocSmall_t)(void* cinfo, int pool_id, unsigned size);
typedef char**(__cdecl* AccessBarray_t)(void* cinfo, void* ptr, unsigned start_row, unsigned num_rows, int writable);
typedef void(__cdecl* ForwardDct_t)(void* cinfo, void* compptr, void* sample_data, void* coef_blocks,
                                     unsigned start_row, unsigned start_col, unsigned num_blocks);
typedef int(__cdecl* EncodeMcu_t)(void* cinfo, void* MCU_data);
typedef void(__cdecl* PreProcess_t)(void* cinfo, void* input_buf, unsigned* in_row_ctr, unsigned in_rows_avail,
                                     void* output_buf, unsigned* out_row_group_ctr, unsigned out_row_groups_avail);
typedef void(__cdecl* ColorConvert_t)(void* cinfo, void* input_buf, void* output_buf, unsigned output_row, int num_rows);
typedef void(__cdecl* Downsample_t)(void* cinfo, void* input_buf, unsigned in_row_index, void* output_buf,
                                     unsigned out_row_group_index);

// jpeg_natural_order as the exe holds it (dword table at 0x004bd0ac; 0x004b292f, 0x004b2f32, 0x004b3214).
const int* const kNaturalOrder = reinterpret_cast<const int*>(0x004bd0ac);

// ================================================================================================================
// jcmainct.c
// ================================================================================================================

// 0x004b05f0  start_pass_main(cinfo, pass_mode): nothing when raw_data_in (cinfo+0xa8) is set (0x004b0602).
// Otherwise cur_iMCU_row, rowgroup_ctr and suspended of the main controller (cinfo+0x148) are cleared and
// pass_mode stored at +0x14 (0x004b0608..0x004b061f); a pass mode other than 0 raises ERREXIT 4
// (0x004b0622, 0x004b0627 / 0x004b0630) and returns without installing anything; mode 0 stores
// process_data_simple_main 0x004b0640 at +4 (0x004b0636).
typedef void(__cdecl* StartPass_t)(void* cinfo, int mode);
StartPass_t StartPassMain_orig;
void __cdecl StartPassMain_re(void* cinfo, int pass_mode) {
    char* main = fp(cinfo, 0x148);                                                  // 0x004b05fa
    if (BR(0x004b0602, fi(cinfo, 0xa8) != 0)) return;
    fi(main, 8) = 0;
    fi(main, 0xc) = 0;
    fi(main, 0x10) = 0;
    fi(main, 0x14) = pass_mode;
    if (BR(0x004b0622, pass_mode == 0)) {
        fu(main, 4) = 0x004b0640;                                                   // 0x004b0636
        return;
    }
    errexit0(cinfo, 4);
}

// 0x004b0640  process_data_simple_main(cinfo, input_buf, in_row_ctr, in_rows_avail): while cur_iMCU_row (main+8)
// is below total_iMCU_rows (cinfo+0xe8, unsigned: 0x004b0659 / 0x004b06d2): with rowgroup_ctr (main+0xc) below 8
// (unsigned, 0x004b0669) call prep->pre_process_data(cinfo, input_buf, in_row_ctr, in_rows_avail, main+0x18,
// main+0xc, 8) (cinfo+0x14c slot +4, 0x004b0684); return unless rowgroup_ctr is then exactly 8 (0x004b068f).
// coef->compress_data(cinfo, main+0x18) (cinfo+0x150 slot +4, 0x004b069c): a 0 return backs *in_row_ctr up by
// one and sets suspended, unless already suspended (0x004b06db..0x004b06e4), and returns. Otherwise a suspended
// controller steps *in_row_ctr forward again and clears the flag (0x004b06ab..0x004b06b4); rowgroup_ctr is cleared
// and cur_iMCU_row incremented (0x004b06be..0x004b06c5).
typedef void(__cdecl* ProcessData_t)(void* cinfo, void* input_buf, unsigned* in_row_ctr, unsigned in_rows_avail);
ProcessData_t ProcessDataSimpleMain_orig;
void __cdecl ProcessDataSimpleMain_re(void* cinfo, void* input_buf, unsigned* in_row_ctr, unsigned in_rows_avail) {
    char* main = fp(cinfo, 0x148);                                                  // 0x004b0648
    if (BR(0x004b0659, fu(main, 8) >= fu(cinfo, 0xe8))) return;
    do {
        if (!BR(0x004b0669, fu(main, 0xc) >= 8u)) {
            reinterpret_cast<PreProcess_t>(fp(fp(cinfo, 0x14c), 4))(cinfo, input_buf, in_row_ctr, in_rows_avail,
                                                                    main + 0x18, &fu(main, 0xc), 8);
        }
        if (BR(0x004b068f, fu(main, 0xc) != 8u)) return;
        const int ok = reinterpret_cast<EncodeMcu_t>(fp(fp(cinfo, 0x150), 4))(cinfo, main + 0x18);
        if (BR(0x004b06a7, ok == 0)) {
            if (!BR(0x004b06db, fi(main, 0x10) != 0)) {
                *in_row_ctr -= 1;                                                   // 0x004b06e0
                fi(main, 0x10) = 1;                                                 // 0x004b06e4
            }
            return;
        }
        if (!BR(0x004b06ab, fi(main, 0x10) == 0)) {
            *in_row_ctr += 1;                                                       // 0x004b06b0
            fi(main, 0x10) = 0;                                                     // 0x004b06b4
        }
        fu(main, 0xc) = 0;
        fu(main, 8) += 1;
    } while (BR(0x004b06d2, fu(main, 8) < fu(cinfo, 0xe8)));
}

// ================================================================================================================
// jccoefct.c
// ================================================================================================================

// 0x004b07b0  start_pass_coef(cinfo, pass_mode): clear iMCU_row_num (coef+8, 0x004b07bd), call start_iMCU_row
// 0x004b0860 (0x004b07c4), then by pass mode (0x004b07d0..0x004b07db): 0 needs whole_image[0] (coef+0x40) NULL
// (else ERREXIT 4, 0x004b083a) and installs compress_data 0x004b08b0 (0x004b084d); 2 needs it non-NULL
// (0x004b0818) and installs compress_output 0x004b0d70 (0x004b082b); 3 needs it non-NULL (0x004b07f6) and
// installs compress_first_pass 0x004b0b20 (0x004b0809). Any other mode raises ERREXIT 4 and installs nothing
// (0x004b07dd..0x004b07e9). The install follows a returning ERREXIT.
StartPass_t StartPassCoef_orig;
void __cdecl StartPassCoef_re(void* cinfo, int pass_mode) {
    char* coef = fp(cinfo, 0x150);
    fu(coef, 8) = 0;
    kStartIMcuRow(cinfo);
    if (BR(0x004b07d3, pass_mode == 0)) {
        if (!BR(0x004b083a, fpr(coef, 0x40) == nullptr)) errexit0(cinfo, 4);
        fu(coef, 4) = 0x004b08b0;
    } else if (BR(0x004b07d8, pass_mode == 2)) {
        if (!BR(0x004b0818, fpr(coef, 0x40) != nullptr)) errexit0(cinfo, 4);
        fu(coef, 4) = 0x004b0d70;
    } else if (BR(0x004b07db, pass_mode == 3)) {
        if (!BR(0x004b07f6, fpr(coef, 0x40) != nullptr)) errexit0(cinfo, 4);
        fu(coef, 4) = 0x004b0b20;
    } else {
        errexit0(cinfo, 4);
    }
}

// 0x004b08b0  compress_data(cinfo, input_buf): DCT and encode one iMCU row from the single-MCU workspace.
// last_MCU_col = MCUs_per_row - 1 and last_iMCU_row = total_iMCU_rows - 1 (0x004b08cd / 0x004b08ce). For yoffset
// from MCU_vert_offset (coef+0x10) below MCU_rows_per_iMCU_row (coef+0x14, signed, re-read: 0x004b08e3 / 0x004b0ae3)
// and MCU_col_num from mcu_ctr (coef+0xc) up to last_MCU_col (unsigned: 0x004b08f8 / 0x004b0ac8), each component of
// the scan (0x004b090c / 0x004b0aa0) codes blockcnt blocks: MCU_width (+0x34), or last_col_width (+0x44) in the last
// column (unsigned, 0x004b092c); xpos = MCU_sample_width (+0x40) * column (0x004b093d), ypos = yoffset * 8 plus 8
// per block row (0x004b0942, 0x004b0a4d). Each of its MCU_height (+0x38) block rows (0x004b095e / 0x004b0a6b) is real
// when iMCU_row_num (coef+8) is below last_iMCU_row (unsigned, 0x004b0973) or yoffset + yindex is below
// last_row_height (+0x48, signed, 0x004b0984): forward_DCT (cinfo+0x160 slot +4, 0x004b09f3) fills
// MCU_buffer[blkn] (coef+0x18) and a short row gets MCU_width - blockcnt zeroed dummy blocks (jzero_far
// 0x004b0540, 0x004b0a15) whose DC copies the block before each (0x004b0a2f..0x004b0a3a). Otherwise the row is
// zeroed (0x004b0992) and each block's DC copies MCU_buffer[blkn - 1]'s (0x004b09af..0x004b09bd). blkn advances by
// MCU_width (0x004b0a57). Then encode_mcu (cinfo+0x164 slot +4, 0x004b0ab3) gets coef+0x18: a 0 return stores
// yoffset and the column (0x004b0b0b / 0x004b0b0e) and returns 0. mcu_ctr is cleared after each MCU row
// (0x004b0ad8); at the end iMCU_row_num is incremented (0x004b0aee), start_iMCU_row 0x004b0860 runs (0x004b0af1)
// and 1 is returned.
CompressFn_t CompressData_orig;
int __cdecl CompressData_re(void* cinfo, void* input_bufp) {
    char* coef = fp(cinfo, 0x150);
    void** input_buf = reinterpret_cast<void**>(input_bufp);
    char** mcu_buffer = reinterpret_cast<char**>(coef + 0x18);
    const unsigned last_MCU_col = fu(cinfo, 0x100) - 1u;
    const unsigned last_iMCU_row = fu(cinfo, 0xe8) - 1u;
    int yoffset = fi(coef, 0x10);
    if (!BR(0x004b08e3, yoffset >= fi(coef, 0x14))) {
        do {
            unsigned col = fu(coef, 0xc);                                           // 0x004b08ef
            if (!BR(0x004b08f8, col > last_MCU_col)) {
                do {
                    int blkn = 0;
                    if (!BR(0x004b090c, fi(cinfo, 0xec) <= 0)) {
                        int ci = 0;
                        do {
                            char* comp = fp(cinfo, 0xf0 + ci * 4);                  // 0x004b0928
                            const int blockcnt = BR(0x004b092c, col >= last_MCU_col) ? fi(comp, 0x44) : fi(comp, 0x34);
                            const unsigned xpos = fu(comp, 0x40) * col;
                            unsigned ypos = static_cast<unsigned>(yoffset) * 8u;
                            if (!BR(0x004b095e, fi(comp, 0x38) <= 0)) {
                                int yindex = 0;
                                do {
                                    if (BR(0x004b0973, fu(coef, 8) < last_iMCU_row) ||
                                        BR(0x004b0984, yoffset + yindex < fi(comp, 0x48))) {
                                        reinterpret_cast<ForwardDct_t>(fp(fp(cinfo, 0x160), 4))(
                                            cinfo, comp, input_buf[ci], mcu_buffer[blkn], ypos, xpos,
                                            static_cast<unsigned>(blockcnt));
                                        if (!BR(0x004b09fe, blockcnt >= fi(comp, 0x34))) {
                                            kJzeroFar(mcu_buffer[blkn + blockcnt],
                                                      static_cast<unsigned>(fi(comp, 0x34) - blockcnt) << 7);
                                            if (!BR(0x004b0a26, blockcnt >= fi(comp, 0x34))) {
                                                int bi = blockcnt;
                                                do {
                                                    fs(mcu_buffer[blkn + bi], 0) = fs(mcu_buffer[blkn + bi - 1], 0);
                                                    bi++;
                                                } while (BR(0x004b0a4b, bi < fi(comp, 0x34)));
                                            }
                                        }
                                    } else {
                                        kJzeroFar(mcu_buffer[blkn], static_cast<unsigned>(fi(comp, 0x34)) << 7);
                                        if (!BR(0x004b09a1, fi(comp, 0x34) <= 0)) {
                                            int bi = 0;
                                            do {
                                                fs(mcu_buffer[blkn + bi], 0) = fs(mcu_buffer[blkn - 1], 0);
                                                bi++;
                                            } while (BR(0x004b09d0, bi < fi(comp, 0x34)));
                                        }
                                    }
                                    blkn += fi(comp, 0x34);
                                    ypos += 8u;
                                    yindex++;
                                } while (BR(0x004b0a6b, yindex < fi(comp, 0x38)));
                            }
                            ci++;
                        } while (BR(0x004b0aa0, ci < fi(cinfo, 0xec)));
                    }
                    const int ok = reinterpret_cast<EncodeMcu_t>(fp(fp(cinfo, 0x164), 4))(cinfo, coef + 0x18);
                    if (BR(0x004b0abb, ok == 0)) {
                        fi(coef, 0x10) = yoffset;
                        fu(coef, 0xc) = col;
                        return 0;
                    }
                    col++;
                } while (BR(0x004b0ac8, col <= last_MCU_col));
            }
            fu(coef, 0xc) = 0;
            yoffset++;
        } while (BR(0x004b0ae3, yoffset < fi(coef, 0x14)));
    }
    fu(coef, 8) += 1;
    kStartIMcuRow(cinfo);
    return 1;
}

// 0x004b0d70  compress_output(cinfo, input_buf): first, per component of the scan (0x004b0d8f / 0x004b0de9), align
// its virtual block array whole_image[component_index] (coef+0x40, comp+4) through cinfo->mem->access_virt_barray
// (mem+0x20, 0x004b0dc2) at row iMCU_row_num * v_samp_factor, v_samp_factor rows, not writable; the results are a
// local buffer[4] array. Then for yoffset from MCU_vert_offset below MCU_rows_per_iMCU_row (signed, re-read:
// 0x004b0df7 / 0x004b0f1f) and MCU_col_num from mcu_ctr below MCUs_per_row (cinfo+0x100, unsigned: 0x004b0e0c /
// 0x004b0f04), MCU_buffer (coef+0x18) is filled with consecutive 0x80-byte block pointers: per component
// (0x004b0e20 / 0x004b0ed8), MCU_height rows (0x004b0e53 / 0x004b0ea8) of MCU_width blocks (0x004b0e76 / 0x004b0e91)
// starting at buffer[ci][yindex + yoffset] + MCU_col_num * MCU_width blocks (0x004b0e4e..0x004b0e70). encode_mcu
// (0x004b0eed): a 0 return stores the column and yoffset (0x004b0f46 / 0x004b0f49) and returns 0. mcu_ctr is
// cleared per MCU row (0x004b0f14); at the end iMCU_row_num is incremented (0x004b0f2a), start_iMCU_row 0x004b0860
// runs (0x004b0f2d) and 1 is returned.
CompressFn_t CompressOutput_orig;
int __cdecl CompressOutput_re(void* cinfo, void*) {
    char* coef = fp(cinfo, 0x150);
    char** buffer[4];
    if (!BR(0x004b0d8f, fi(cinfo, 0xec) <= 0)) {
        int ci = 0;
        do {
            char* comp = fp(cinfo, 0xf0 + ci * 4);
            const unsigned v = fu(comp, 0xc);
            buffer[ci] = reinterpret_cast<AccessBarray_t>(fp(fp(cinfo, 4), 0x20))(
                cinfo, fpr(coef, 0x40 + fi(comp, 4) * 4), fu(coef, 8) * v, v, 0);
            ci++;
        } while (BR(0x004b0de9, ci < fi(cinfo, 0xec)));
    }
    int yoffset = fi(coef, 0x10);
    if (!BR(0x004b0df7, yoffset >= fi(coef, 0x14))) {
        do {
            unsigned col = fu(coef, 0xc);
            if (!BR(0x004b0e0c, col >= fu(cinfo, 0x100))) {
                do {
                    int blkn = 0;
                    if (!BR(0x004b0e20, fi(cinfo, 0xec) <= 0)) {
                        int ci = 0;
                        do {
                            char* comp = fp(cinfo, 0xf0 + ci * 4);
                            const unsigned start = static_cast<unsigned>(fi(comp, 0x34)) * col;   // 0x004b0e4e
                            if (!BR(0x004b0e53, fi(comp, 0x38) <= 0)) {
                                int yindex = 0;
                                do {
                                    char* p = buffer[ci][yindex + yoffset] + (start << 7);
                                    if (!BR(0x004b0e76, fi(comp, 0x34) <= 0)) {
                                        int xindex = 0;
                                        do {
                                            reinterpret_cast<char**>(coef + 0x18)[blkn++] = p;    // 0x004b0e80
                                            p += 0x80;
                                            xindex++;
                                        } while (BR(0x004b0e91, xindex < fi(comp, 0x34)));
                                    }
                                    yindex++;
                                } while (BR(0x004b0ea8, yindex < fi(comp, 0x38)));
                            }
                            ci++;
                        } while (BR(0x004b0ed8, ci < fi(cinfo, 0xec)));
                    }
                    const int ok = reinterpret_cast<EncodeMcu_t>(fp(fp(cinfo, 0x164), 4))(cinfo, coef + 0x18);
                    if (BR(0x004b0ef5, ok == 0)) {
                        fu(coef, 0xc) = col;
                        fi(coef, 0x10) = yoffset;
                        return 0;
                    }
                    col++;
                } while (BR(0x004b0f04, col < fu(cinfo, 0x100)));
            }
            fu(coef, 0xc) = 0;
            yoffset++;
        } while (BR(0x004b0f1f, yoffset < fi(coef, 0x14)));
    }
    fu(coef, 8) += 1;
    kStartIMcuRow(cinfo);
    return 1;
}

// 0x004b0b20  compress_first_pass(cinfo, input_buf): for every component of the image (num_components cinfo+0x34,
// comp_info cinfo+0x3c, 0x54 bytes each: 0x004b0b53 / 0x004b0d48) align whole_image[ci] (coef+0x40) writable at row
// iMCU_row_num * v_samp_factor through access_virt_barray (0x004b0b8d). block_rows is v_samp_factor below the last
// iMCU row (unsigned, 0x004b0b9f), else height_in_blocks (+0x20) % v_samp_factor or v_samp_factor when that is 0
// (unsigned div, 0x004b0bac / 0x004b0bb4). ndummy = width_in_blocks (+0x1c) % h_samp_factor (+8), turned into
// h_samp_factor - ndummy when positive (0x004b0bc8..0x004b0bd8). Each of the block_rows rows (0x004b0bdc / 0x004b0c62)
// gets forward_DCT(cinfo, comp, input_buf[ci], buffer[row], row * 8, 0, width_in_blocks) (0x004b0c15); with
// ndummy > 0 (0x004b0c1d) the ndummy blocks after it are zeroed (0x004b0c2d) and take the last real block's DC
// (0x004b0c32..0x004b0c49). In the last iMCU row (0x004b0c7b) every row from block_rows below v_samp_factor
// (0x004b0c96 / 0x004b0d14) is zeroed over width_in_blocks + ndummy blocks (0x004b0cbb) and, per MCU of
// h_samp_factor blocks ((width + ndummy) / h_samp_factor, unsigned: 0x004b0c8b, 0x004b0cc9 / 0x004b0cf1), each
// block's DC copies the DC of the previous row's last block of that MCU (0x004b0cd6..0x004b0cea). Then returns
// compress_output(cinfo, input_buf) 0x004b0d70 (0x004b0d55).
CompressFn_t CompressFirstPass_orig;
int __cdecl CompressFirstPass_re(void* cinfo, void* input_bufp) {
    char* coef = fp(cinfo, 0x150);
    void** input_buf = reinterpret_cast<void**>(input_bufp);
    const unsigned last_iMCU_row = fu(cinfo, 0xe8) - 1u;                            // 0x004b0b3c
    char* comp = fp(cinfo, 0x3c);
    if (!BR(0x004b0b53, fi(cinfo, 0x34) <= 0)) {
        int ci = 0;
        do {
            const unsigned v = fu(comp, 0xc);
            char** buffer = reinterpret_cast<AccessBarray_t>(fp(fp(cinfo, 4), 0x20))(
                cinfo, fpr(coef, 0x40 + ci * 4), v * fu(coef, 8), v, 1);
            int block_rows;
            if (!BR(0x004b0b9f, fu(coef, 8) >= last_iMCU_row)) {
                block_rows = fi(comp, 0xc);
            } else {
                block_rows = static_cast<int>(fu(comp, 0x20) % fu(comp, 0xc));
                if (!BR(0x004b0bb4, block_rows != 0)) block_rows = fi(comp, 0xc);
            }
            unsigned blocks_across = fu(comp, 0x1c);
            const int h_samp = fi(comp, 8);
            int ndummy = static_cast<int>(blocks_across % static_cast<unsigned>(h_samp));
            if (!BR(0x004b0bd2, ndummy <= 0)) ndummy = h_samp - ndummy;
            if (!BR(0x004b0bdc, block_rows <= 0)) {
                int n = block_rows;
                int block_row = 0;
                do {
                    char* row = buffer[block_row];
                    reinterpret_cast<ForwardDct_t>(fp(fp(cinfo, 0x160), 4))(
                        cinfo, comp, input_buf[ci], row, static_cast<unsigned>(block_row) * 8u, 0, blocks_across);
                    if (!BR(0x004b0c1d, ndummy <= 0)) {
                        row += blocks_across << 7;
                        kJzeroFar(row, static_cast<unsigned>(ndummy) << 7);
                        const short lastDC = fs(row, -0x80);
                        if (!BR(0x004b0c3b, ndummy <= 0)) {
                            int m = ndummy;
                            do {
                                fs(row, 0) = lastDC;
                                row += 0x80;
                            } while (BR(0x004b0c49, --m != 0));
                        }
                    }
                    block_row++;
                } while (BR(0x004b0c62, --n != 0));
            }
            if (!BR(0x004b0c7b, fu(coef, 8) != last_iMCU_row)) {
                blocks_across += static_cast<unsigned>(ndummy);
                const unsigned mcus_across = blocks_across / static_cast<unsigned>(h_samp);
                int block_row = block_rows;
                if (!BR(0x004b0c96, block_row >= fi(comp, 0xc))) {
                    do {
                        char* thisrow = buffer[block_row];
                        char* lastrow = buffer[block_row - 1];
                        kJzeroFar(thisrow, blocks_across << 7);
                        if (!BR(0x004b0cc9, mcus_across == 0)) {
                            unsigned m = mcus_across;
                            do {
                                const short lastDC = fs(lastrow, (h_samp << 7) - 0x80);
                                if (!BR(0x004b0cdd, h_samp <= 0)) {
                                    char* p = thisrow;
                                    int bi = h_samp;
                                    do {
                                        fs(p, 0) = lastDC;
                                        p += 0x80;
                                    } while (BR(0x004b0cea, --bi != 0));
                                }
                                thisrow += h_samp << 7;
                                lastrow += h_samp << 7;
                            } while (BR(0x004b0cf1, --m != 0));
                        }
                        block_row++;
                    } while (BR(0x004b0d14, block_row < fi(comp, 0xc)));
                }
            }
            ci++;
            comp += 0x54;
        } while (BR(0x004b0d48, ci < fi(cinfo, 0x34)));
    }
    return kCompressOutput(cinfo, input_buf);
}

// ================================================================================================================
// jcphuff.c
// ================================================================================================================

// One inlined copy of jcphuff's emit_bits (the encoders hold several): an optional ERREXIT 0x27 for a zero size
// (`check`, 0 = this copy has none), nothing while gathering statistics (entropy+0xc), else mask `code` to `size`
// bits, align at 24 - (put_bits + size) and merge into put_buffer (+0x18); with 8 or more bits pending, (put_bits >> 3)
// bytes (put_buffer >> 16) & 0xff are stored through next_output_byte (+0x10), each followed by dump_buffer
// 0x004b2420 when free_in_buffer (+0x14) reaches 0, and a 0xff byte by a stuffed 0 the same way. Shift counts are
// taken mod 32 as `shl r, cl` does. put_bits is read before the ERREXIT call as every copy does.
struct PhCopy {
    unsigned check;   // jne past the ERREXIT (0 = none)
    unsigned gather;  // jne skipping the copy when gathering
    unsigned enter;   // jl skipping the byte loop
    unsigned free1;   // jne past dump_buffer after the byte
    unsigned ff;      // jne past the stuffed zero
    unsigned free2;   // jne past dump_buffer after the stuffed zero
    unsigned again;   // jne back to the byte loop head
};
static void ph_emit_bits(char* e, unsigned code, int size, const PhCopy& at) {
    const int put_bits0 = fi(e, 0x1c);
    if (at.check != 0 && !BR(at.check, size != 0)) errexit0(fp(e, 0x20), 0x27);
    if (BR(at.gather, fi(e, 0xc) != 0)) return;
    int put_bits = put_bits0 + size;
    unsigned buffer = (((1u << (static_cast<unsigned>(size) & 31)) - 1u) & code)
                      << (static_cast<unsigned>(24 - put_bits) & 31);
    buffer |= fu(e, 0x18);
    if (!BR(at.enter, put_bits < 8)) {
        unsigned n = static_cast<unsigned>(put_bits) >> 3;
        put_bits -= static_cast<int>(n * 8);
        do {
            const unsigned c = (buffer >> 16) & 0xff;
            *reinterpret_cast<unsigned char*>(fp(e, 0x10)) = static_cast<unsigned char>(c);
            fu(e, 0x10) += 1;
            if (!BR(at.free1, --fu(e, 0x14) != 0)) kDumpBufferP(e);
            if (!BR(at.ff, c != 0xff)) {
                *reinterpret_cast<unsigned char*>(fp(e, 0x10)) = 0;
                fu(e, 0x10) += 1;
                if (!BR(at.free2, --fu(e, 0x14) != 0)) kDumpBufferP(e);
            }
            buffer <<= 8;
        } while (BR(at.again, --n != 0));
    }
    fu(e, 0x18) = buffer;
    fi(e, 0x1c) = put_bits;
}
// emit_symbol: while gathering (`gatherJcc` is the je taken when NOT gathering) count_ptrs[tbl][symbol] (+0x5c) is
// incremented; otherwise derived_tbls[tbl] (+0x4c) supplies ehufco[symbol] and ehufsi[symbol] (signed char) for an
// emit_bits copy.
static void ph_emit_symbol(char* e, int tbl, int symbol, unsigned gatherJcc, const PhCopy& at) {
    if (!BR(gatherJcc, fi(e, 0xc) == 0)) {
        reinterpret_cast<int*>(fp(e, 0x5c + tbl * 4))[symbol] += 1;
        return;
    }
    char* d = fp(e, 0x4c + tbl * 4);
    ph_emit_bits(e, fu(d, symbol * 4), static_cast<signed char>(d[0x400 + symbol]), at);
}
// The encoders' common head: copy cinfo->dest's next_output_byte / free_in_buffer into the entropy coder, and with
// a restart interval (cinfo+0xc0) and no MCUs left (restarts_to_go +0x44) call emit_restart_4b2460(entropy,
// next_restart_num) first.
static void ph_head(void* cinfo, char* e, unsigned riJcc, unsigned togoJcc) {
    fu(e, 0x10) = fu(fp(cinfo, 0x14), 0);
    fu(e, 0x14) = fu(fp(cinfo, 0x14), 4);
    if (!BR(riJcc, fi(cinfo, 0xc0) == 0) && !BR(togoJcc, fu(e, 0x44) != 0)) kEmitRestartP(e, fi(e, 0x48));
}
// ... and tail: write the destination back, then with a restart interval reload restarts_to_go and step
// next_restart_num = (n + 1) & 7 when it is 0, and decrement restarts_to_go.
static void ph_tail(void* cinfo, char* e, unsigned riJcc, unsigned togoJcc) {
    fu(fp(cinfo, 0x14), 0) = fu(e, 0x10);
    fu(fp(cinfo, 0x14), 4) = fu(e, 0x14);
    const unsigned ri = fu(cinfo, 0xc0);
    if (!BR(riJcc, ri == 0)) {
        if (!BR(togoJcc, fu(e, 0x44) != 0)) {
            fu(e, 0x44) = ri;
            fi(e, 0x48) = (fi(e, 0x48) + 1) & 7;
        }
        fu(e, 0x44) -= 1;
    }
}

// 0x004b1f30  jinit_phuff_encoder(cinfo): allocate the 0x6c-byte encoder through cinfo->mem->alloc_small(cinfo, 1,
// 0x6c) (0x004b1f35..0x004b1f3d), store it at cinfo+0x164 (0x004b1f42), install start_pass_phuff 0x004b1f70 at +0
// (0x004b1f48), clear derived_tbls[0..3] (+0x4c) and count_ptrs[0..3] (+0x5c) (four passes, 0x004b1f58..0x004b1f61)
// and bit_buffer (+0x40, 0x004b1f63).
typedef void(__cdecl* VoidCinfo_t)(void* cinfo);
VoidCinfo_t JinitPhuffEncoder_orig;
void __cdecl JinitPhuffEncoder_re(void* cinfo) {
    char* e = reinterpret_cast<char*>(reinterpret_cast<AllocSmall_t>(fp(fp(cinfo, 4), 0))(cinfo, 1, 0x6c));
    fpr(cinfo, 0x164) = e;
    fu(e, 0) = 0x004b1f70;
    int i = 0;
    do {
        fu(e, 0x4c + i * 4) = 0;
        fu(e, 0x5c + i * 4) = 0;
        i++;
    } while (BR(0x004b1f61, i != 4));
    fu(e, 0x40) = 0;
}

// 0x004b1f70  start_pass_phuff(cinfo, gather_statistics): store cinfo (+0x20) and the flag (+0xc) in the encoder
// (0x004b1f89 / 0x004b1f8c). is_DC_band = (Ss == 0) (cinfo+0x134, 0x004b1f9d). encode_mcu (+4): Ah (cinfo+0x13c) 0
// gives DC_first 0x004b2140 or AC_first 0x004b28c0 (0x004b1fa6 / 0x004b1faa), else DC_refine 0x004b2d30 or AC_refine
// 0x004b2eb0 (0x004b1fc0); AC_refine also gets a 1000-byte bit_buffer (+0x40) from alloc_small(cinfo, 1, 0x3e8) when
// it is NULL (0x004b1fd7..0x004b1fe9). finish_pass (+8): finish_pass_gather_phuff 0x004b33c0 when gathering, else
// 0x004b3380 (0x004b1fee). Per component of the scan (0x004b200c / 0x004b210e) last_dc_val[ci] (+0x24) is cleared
// (0x004b202c); a DC band with Ah != 0 (re-read, 0x004b203c) does nothing more. The table number is dc_tbl_no
// (comp+0x14) for a DC band, else ac_tbl_no (comp+0x18, also stored at +0x34, 0x004b2065); outside 0..3, or naming a
// NULL dc / ac_huff_tbl_ptrs slot (cinfo+0x50 / +0x60) while not gathering, raises ERREXIT1 0x31 with the number
// (0x004b2047..0x004b207d, 0x004b207f..0x004b2090). Gathering, count_ptrs[tbl] (+0x5c) is allocated when NULL
// (alloc_small(cinfo, 1, 0x404), 0x004b20a3..0x004b20b5) and its 0x101 dwords zeroed (0x004b20c4); otherwise
// jpeg_make_c_derived_tbl 0x004b0f60 builds derived_tbls[tbl] (+0x4c) from the slot (0x004b20ce..0x004b20e7). Finally
// EOBRUN, BE, put_buffer, put_bits and next_restart_num are cleared and restarts_to_go takes restart_interval
// (0x004b2116..0x004b212d).
typedef void(__cdecl* StartPassG_t)(void* cinfo, int gather);
StartPassG_t StartPassPhuff_orig;
void __cdecl StartPassPhuff_re(void* cinfo, int gather) {
    char* e = fp(cinfo, 0x164);
    fpr(e, 0x20) = cinfo;
    fi(e, 0xc) = gather;
    const int is_dc = fi(cinfo, 0x134) == 0 ? 1 : 0;
    if (!BR(0x004b1fa6, fi(cinfo, 0x13c) != 0)) {
        fu(e, 4) = BR(0x004b1faa, is_dc == 0) ? 0x004b28c0u : 0x004b2140u;
    } else if (!BR(0x004b1fc0, is_dc == 0)) {
        fu(e, 4) = 0x004b2d30;
    } else {
        fu(e, 4) = 0x004b2eb0;
        if (!BR(0x004b1fd7, fpr(e, 0x40) != nullptr))
            fpr(e, 0x40) = reinterpret_cast<AllocSmall_t>(fp(fp(cinfo, 4), 0))(cinfo, 1, 0x3e8);
    }
    fu(e, 8) = BR(0x004b1fee, gather == 0) ? 0x004b3380u : 0x004b33c0u;
    if (!BR(0x004b200c, fi(cinfo, 0xec) <= 0)) {
        int ci = 0;
        do {
            char* comp = fp(cinfo, 0xf0 + ci * 4);
            fi(e, 0x24 + ci * 4) = 0;
            int tbl;
            bool bad;
            if (!BR(0x004b2032, is_dc == 0)) {
                if (BR(0x004b203c, fi(cinfo, 0x13c) != 0)) goto next;
                tbl = fi(comp, 0x14);
                bad = BR(0x004b2047, tbl < 0) || BR(0x004b204c, tbl >= 4);
                if (!bad) {
                    if (BR(0x004b2054, fpr(cinfo, 0x50 + tbl * 4) != nullptr)) goto check_gather;
                    if (BR(0x004b205c, gather != 0)) goto do_gather;
                }
            } else {
                tbl = fi(comp, 0x18);
                fi(e, 0x34) = tbl;
                bad = BR(0x004b2068, tbl < 0) || BR(0x004b206d, tbl >= 4);
                if (!bad) {
                    if (BR(0x004b2075, fpr(cinfo, 0x60 + tbl * 4) != nullptr)) goto check_gather;
                    if (BR(0x004b207d, gather != 0)) goto do_gather;
                }
            }
            errexit1(cinfo, 0x31, tbl);
        check_gather:
            if (BR(0x004b209b, gather == 0)) {
                if (!BR(0x004b20ce, is_dc == 0))
                    kMakeDerivedTbl(cinfo, fpr(cinfo, 0x50 + tbl * 4), e + 0x4c + tbl * 4);
                else
                    kMakeDerivedTbl(cinfo, fpr(cinfo, 0x60 + tbl * 4), e + 0x4c + tbl * 4);
                goto next;
            }
        do_gather:
            if (!BR(0x004b20a3, fpr(e, 0x5c + tbl * 4) != nullptr))
                fpr(e, 0x5c + tbl * 4) = reinterpret_cast<AllocSmall_t>(fp(fp(cinfo, 4), 0))(cinfo, 1, 0x404);
            {
                unsigned* counts = reinterpret_cast<unsigned*>(fp(e, 0x5c + tbl * 4));
                for (int i = 0; i < 0x101; i++) counts[i] = 0;
            }
        next:
            ci++;
        } while (BR(0x004b210e, ci < fi(cinfo, 0xec)));
    }
    fu(e, 0x38) = 0;
    fu(e, 0x3c) = 0;
    fu(e, 0x18) = 0;
    fu(e, 0x1c) = 0;
    fu(e, 0x48) = 0;
    fu(e, 0x44) = fu(cinfo, 0xc0);
}

// 0x004b2460  emit_restart_4b2460(entropy, restart_num): emit_eobrun 0x004b25d0 (0x004b2466); unless gathering
// (+0xc, 0x004b2473): flush_bits_4b2510 0x004b2510 (0x004b2476), then the bytes 0xff (0x004b2481) and
// restart_num - 0x30 as a byte (0xd0 + n, 0x004b24a4 / 0x004b24a7), each followed by dump_buffer_4b2420 0x004b2420
// when free_in_buffer reaches 0 (0x004b2492 / 0x004b24b7). Then through entropy->cinfo (+0x20): with Ss
// (cinfo+0x134) 0 (0x004b24cd) last_dc_val[ci] (+0x24) is cleared for every component of the scan (count re-read
// each pass, 0x004b24d9 / 0x004b24f1); otherwise EOBRUN (+0x38) and BE (+0x3c) are cleared (0x004b24f5 / 0x004b24fc).
EmitRestartP_t EmitRestartP_orig;
void __cdecl EmitRestartP_re(void* ep, int restart_num) {
    char* e = reinterpret_cast<char*>(ep);
    kEmitEobrun(e);
    if (!BR(0x004b2473, fi(e, 0xc) != 0)) {
        kFlushBitsP(e);
        *reinterpret_cast<unsigned char*>(fp(e, 0x10)) = 0xff;
        fu(e, 0x10) += 1;
        if (!BR(0x004b2492, --fu(e, 0x14) != 0)) kDumpBufferP(e);
        *reinterpret_cast<unsigned char*>(fp(e, 0x10)) =
            static_cast<unsigned char>(static_cast<unsigned char>(restart_num) - 0x30);
        fu(e, 0x10) += 1;
        if (!BR(0x004b24b7, --fu(e, 0x14) != 0)) kDumpBufferP(e);
    }
    if (BR(0x004b24cd, fi(fp(e, 0x20), 0x134) != 0)) {
        fu(e, 0x38) = 0;
        fu(e, 0x3c) = 0;
        return;
    }
    if (BR(0x004b24d9, fi(fp(e, 0x20), 0xec) <= 0)) return;
    int ci = 0;
    do {
        fi(e, 0x24 + ci * 4) = 0;
        ci++;
    } while (BR(0x004b24f1, ci < fi(fp(e, 0x20), 0xec)));
}

// 0x004b2140  encode_mcu_DC_first(cinfo, MCU_data): the common head (0x004b215b / 0x004b2164, restart 0x004b216f /
// 0x004b2176 / 0x004b217d). For each block of the MCU (blocks_in_MCU cinfo+0x108 re-read: 0x004b2195 / 0x004b23d9):
// ci = MCU_membership[blkn] (cinfo+0x10c), temp2 = block[0] (word, signed) >> Al (cinfo+0x140; the count is Al's
// low byte, 0x004b21a5 / 0x004b21cf), temp = temp2 - last_dc_val[ci] and last_dc_val[ci] = temp2 (0x004b21d1..
// 0x004b21d7); a negative difference (0x004b21e1) is negated with temp2 decremented (0x004b21e7 / 0x004b21e9); nbits
// counts its bits (0x004b21f6..0x004b21fb). The symbol nbits is counted in count_ptrs[dc_tbl_no] while gathering
// (0x004b2209..0x004b2216) or emitted from derived_tbls[dc_tbl_no] (ERREXIT 0x27 for a size 0 at 0x004b2235); with
// nbits != 0 (0x004b22fa) nbits bits of temp2 follow (0x004b2300..0x004b23ac). The common tail (0x004b23e0..
// 0x004b240f); returns 1.
EncodeMcu_t EncodeMcuDcFirst_orig;
const PhCopy kDcSym = {0x004b2235, 0x004b2253, 0x004b2280, 0x004b22ab, 0x004b22bf, 0x004b22d5, 0x004b22e8};
const PhCopy kDcVal = {0, 0x004b2308, 0x004b2337, 0x004b2361, 0x004b2372, 0x004b2388, 0x004b23a3};
int __cdecl EncodeMcuDcFirst_re(void* cinfo, void* MCU_datap) {
    short** MCU_data = reinterpret_cast<short**>(MCU_datap);
    char* e = fp(cinfo, 0x164);
    const unsigned sh = static_cast<unsigned>(fi(cinfo, 0x140)) & 31;
    ph_head(cinfo, e, 0x004b216f, 0x004b2176);
    if (!BR(0x004b2195, fi(cinfo, 0x108) <= 0)) {
        int blkn = 0;
        do {
            const int ci = fi(cinfo, 0x10c + blkn * 4);
            char* comp = fp(cinfo, 0xf0 + ci * 4);
            int temp2 = static_cast<int>(MCU_data[blkn][0]) >> sh;
            int temp = static_cast<int>(static_cast<unsigned>(temp2) - fu(e, 0x24 + ci * 4));
            fi(e, 0x24 + ci * 4) = temp2;
            temp2 = temp;
            if (!BR(0x004b21e1, temp >= 0)) {
                temp = static_cast<int>(0u - static_cast<unsigned>(temp));
                temp2 = static_cast<int>(static_cast<unsigned>(temp2) - 1u);
            }
            int nbits = 0;
            if (!BR(0x004b21f6, temp == 0)) {
                do { nbits++; temp >>= 1; } while (BR(0x004b21fb, temp != 0));
            }
            ph_emit_symbol(e, fi(comp, 0x14), nbits, 0x004b2209, kDcSym);
            if (!BR(0x004b22fa, nbits == 0)) ph_emit_bits(e, static_cast<unsigned>(temp2), nbits, kDcVal);
            blkn++;
        } while (BR(0x004b23d9, blkn < fi(cinfo, 0x108)));
    }
    ph_tail(cinfo, e, 0x004b23f9, 0x004b2400);
    return 1;
}

// 0x004b2d30  encode_mcu_DC_refine(cinfo, MCU_data): the common head (restart 0x004b2d62 / 0x004b2d69 /
// 0x004b2d70); for each block (0x004b2d88 / 0x004b2e62) one bit, (block[0] >> Al) & 1 (0x004b2db1..0x004b2dbd), is
// emitted unless gathering (0x004b2d9d; no size check, the size is 1); the common tail (0x004b2e6a..0x004b2e99);
// returns 1.
EncodeMcu_t EncodeMcuDcRefine_orig;
const PhCopy kDcRefBit = {0, 0x004b2d9d, 0x004b2dce, 0x004b2df8, 0x004b2e09, 0x004b2e1f, 0x004b2e3a};
int __cdecl EncodeMcuDcRefine_re(void* cinfo, void* MCU_datap) {
    short** MCU_data = reinterpret_cast<short**>(MCU_datap);
    char* e = fp(cinfo, 0x164);
    const unsigned sh = static_cast<unsigned>(fi(cinfo, 0x140)) & 31;
    ph_head(cinfo, e, 0x004b2d62, 0x004b2d69);
    if (!BR(0x004b2d88, fi(cinfo, 0x108) <= 0)) {
        int blkn = 0;
        do {
            const int temp = static_cast<int>(MCU_data[blkn][0]) >> sh;
            ph_emit_bits(e, static_cast<unsigned>(temp), 1, kDcRefBit);
            blkn++;
        } while (BR(0x004b2e62, blkn < fi(cinfo, 0x108)));
    }
    ph_tail(cinfo, e, 0x004b2e83, 0x004b2e8a);
    return 1;
}

// 0x004b28c0  encode_mcu_AC_first(cinfo, MCU_data): the common head (restart 0x004b28f9 / 0x004b2900 / 0x004b2907).
// Only MCU_data[0] is coded. Nothing when Ss (cinfo+0x134) is above Se (cinfo+0x138, signed, 0x004b2927); else for
// k = Ss..Se (Se - Ss + 1 passes, 0x004b292d..0x004b2936 / 0x004b2cc6) the coefficient at jpeg_natural_order[k]
// (0x004b292f, word, signed): zero extends the run r (0x004b2950); a negative one is negated, shifted right by Al and
// complemented for the value bits (0x004b2962..0x004b296e), a positive one only shifted (0x004b2978); one that
// shifts to 0 extends the run too (0x004b2984). Otherwise a pending EOBRUN (+0x38, 0x004b2995) is flushed by
// emit_eobrun 0x004b25d0; a run above 15 (0x004b29a7) emits r >> 4 ZRL symbols 0xf0 (0x004b29ad..0x004b2ac8; counted
// at count_ptrs[ac_tbl_no] + 0x3c0 when gathering, ERREXIT 0x27 for size 0 at 0x004b29f4) leaving r & 15; nbits
// counts the magnitude's bits from 1 (0x004b2ace..0x004b2ade); symbol (r << 4) + nbits (0x004b2ae4..0x004b2aea,
// ERREXIT at 0x004b2b1b) and nbits value bits (0x004b2bdd..0x004b2c9f) follow and r is cleared. A trailing run
// (signed, 0x004b2ccf) increments EOBRUN and flushes it at 0x7fff (0x004b2cd1..0x004b2ce2). The common tail
// (0x004b2cea..0x004b2d19); returns 1.
EncodeMcu_t EncodeMcuAcFirst_orig;
const PhCopy kAcZrl = {0x004b29f4, 0x004b2a16, 0x004b2a43, 0x004b2a6d, 0x004b2a7e, 0x004b2a94, 0x004b2aaf};
const PhCopy kAcSym = {0x004b2b1b, 0x004b2b39, 0x004b2b66, 0x004b2b8f, 0x004b2ba0, 0x004b2bb6, 0x004b2bd1};
const PhCopy kAcVal = {0x004b2be2, 0x004b2c00, 0x004b2c2f, 0x004b2c58, 0x004b2c69, 0x004b2c7f, 0x004b2c9a};
int __cdecl EncodeMcuAcFirst_re(void* cinfo, void* MCU_datap) {
    short** MCU_data = reinterpret_cast<short**>(MCU_datap);
    char* e = fp(cinfo, 0x164);
    const unsigned sh = static_cast<unsigned>(fi(cinfo, 0x140)) & 31;
    const int Se = fi(cinfo, 0x138);
    ph_head(cinfo, e, 0x004b28f9, 0x004b2900);
    short* block = MCU_data[0];
    const int Ss = fi(cinfo, 0x134);
    int r = 0;
    if (!BR(0x004b2927, Ss > Se)) {
        unsigned count = static_cast<unsigned>(Se - Ss + 1);
        const int* nat = kNaturalOrder + Ss;
        do {
            int temp = block[*nat];
            int temp2;
            if (!BR(0x004b2950, temp != 0)) { r++; goto next; }
            if (!BR(0x004b295c, temp >= 0)) {
                temp = static_cast<int>(0u - static_cast<unsigned>(temp)) >> sh;
                temp2 = ~temp;
            } else {
                temp >>= sh;
                temp2 = temp;
            }
            if (!BR(0x004b2984, temp != 0)) { r++; goto next; }
            if (!BR(0x004b2995, fu(e, 0x38) == 0)) kEmitEobrun(e);
            if (!BR(0x004b29a7, r <= 15)) {
                unsigned n16 = static_cast<unsigned>(r) >> 4;
                r += static_cast<int>((0u - n16) << 4);
                do {
                    ph_emit_symbol(e, fi(e, 0x34), 0xf0, 0x004b29c9, kAcZrl);
                } while (BR(0x004b2ac8, --n16 != 0));
            }
            {
                int nbits = 1;
                temp >>= 1;
                if (!BR(0x004b2ad9, temp == 0)) {
                    do { nbits++; temp >>= 1; } while (BR(0x004b2ade, temp != 0));
                }
                ph_emit_symbol(e, fi(e, 0x34), (r << 4) + nbits, 0x004b2af3, kAcSym);
                ph_emit_bits(e, static_cast<unsigned>(temp2), nbits, kAcVal);
            }
            r = 0;
        next:
            nat++;
        } while (BR(0x004b2cc6, --count != 0));
        if (!BR(0x004b2ccf, r <= 0)) {
            fu(e, 0x38) += 1;
            if (!BR(0x004b2cdf, fu(e, 0x38) != 0x7fffu)) kEmitEobrun(e);
        }
    }
    ph_tail(cinfo, e, 0x004b2d03, 0x004b2d0a);
    return 1;
}

// 0x004b2eb0  encode_mcu_AC_refine(cinfo, MCU_data): the common head (restart 0x004b2ef0 / 0x004b2ef7 / 0x004b2efe).
// Only MCU_data[0] is coded. A pre-pass over k = Ss..Se (signed, 0x004b2f30 / 0x004b2f5b) stores |coef| >> Al
// (0x004b2f39..0x004b2f45) in a local absvalues[k] and records in EOB the last k whose value is 1 (0x004b2f4e /
// 0x004b2f50). The main pass (0x004b2f7f / 0x004b32e9) starts BR_buffer at bit_buffer (+0x40) + BE (+0x3c)
// (0x004b2f5d..0x004b2f69): a 0 extends the run r (0x004b2f8f); while r > 15 and k <= EOB (0x004b2f9e / 0x004b2fa8 /
// 0x004b30e1) emit_eobrun 0x004b25d0, a ZRL symbol 0xf0 (ERREXIT 0x27 at 0x004b2fea), r -= 16 and
// emit_buffered_bits 0x004b27d0 of the BR bits so far, after which BR_buffer restarts at bit_buffer with BR 0
// (0x004b30b1..0x004b30d9). A value above 1 (0x004b30ea) only appends its low bit to BR_buffer[BR++] (0x004b30f4 /
// 0x004b30f7). A 1 flushes emit_eobrun, emits symbol (r << 4) + 1 (ERREXIT at 0x004b3141), one bit that is 1 for a
// non-negative coefficient (0x004b321f / 0x004b3228, skipped while gathering at 0x004b3204), the buffered bits,
// and clears BR and r (0x004b32b4..0x004b32d8). Afterwards a positive r (0x004b32f1) or a non-zero BR (0x004b32f9)
// increments EOBRUN and adds BR to BE (0x004b32fb..0x004b3312), flushing with emit_eobrun when EOBRUN is 0x7fff
// (0x004b3317) or BE is above 0x3a9 (unsigned, 0x004b331f). The common tail (0x004b332a..0x004b3360); returns 1.
EncodeMcu_t EncodeMcuAcRefine_orig;
const PhCopy kArZrl = {0x004b2fea, 0x004b3008, 0x004b3035, 0x004b305f, 0x004b3070, 0x004b3086, 0x004b30a1};
const PhCopy kArSym = {0x004b3141, 0x004b315f, 0x004b318c, 0x004b31b5, 0x004b31c6, 0x004b31dc, 0x004b31f7};
const PhCopy kArBit = {0, 0x004b3204, 0x004b3240, 0x004b326a, 0x004b327b, 0x004b3291, 0x004b32ac};
int __cdecl EncodeMcuAcRefine_re(void* cinfo, void* MCU_datap) {
    short** MCU_data = reinterpret_cast<short**>(MCU_datap);
    char* e = fp(cinfo, 0x164);
    const int Se = fi(cinfo, 0x138);
    const unsigned sh = static_cast<unsigned>(fi(cinfo, 0x140)) & 31;
    ph_head(cinfo, e, 0x004b2ef0, 0x004b2ef7);
    const int Ss = fi(cinfo, 0x134);
    short* block = MCU_data[0];
    int absvalues[64];
    int eob = 0;
    int k = Ss;
    if (!BR(0x004b2f30, k > Se)) {
        do {
            int temp = block[kNaturalOrder[k]];
            if (!BR(0x004b2f3f, temp >= 0)) temp = -temp;
            temp >>= sh;
            absvalues[k] = temp;
            if (!BR(0x004b2f4e, temp != 1)) eob = k;
            k++;
        } while (BR(0x004b2f5b, k <= Se));
    }
    char* br_buffer = fp(e, 0x40) + fu(e, 0x3c);
    int r = 0;
    unsigned br = 0;
    k = Ss;
    if (!BR(0x004b2f7f, k > Se)) {
        do {
            const int temp = absvalues[k];
            if (!BR(0x004b2f8f, temp != 0)) { r++; goto next; }
            if (!BR(0x004b2f9e, r <= 15)) {
                while (!BR(0x004b2fa8, k > eob)) {
                    kEmitEobrun(e);
                    ph_emit_symbol(e, fi(e, 0x34), 0xf0, 0x004b2fbf, kArZrl);
                    r -= 16;
                    kEmitBufferedBits(e, br_buffer, br);
                    br_buffer = fp(e, 0x40);
                    br = 0;
                    if (!BR(0x004b30e1, r > 15)) break;
                }
            }
            if (!BR(0x004b30ea, temp <= 1)) {
                br_buffer[br++] = static_cast<char>(temp & 1);
                goto next;
            }
            kEmitEobrun(e);
            ph_emit_symbol(e, fi(e, 0x34), (r << 4) + 1, 0x004b3119, kArSym);
            ph_emit_bits(e, block[kNaturalOrder[k]] >= 0 ? 1u : 0u, 1, kArBit);
            kEmitBufferedBits(e, br_buffer, br);
            br_buffer = fp(e, 0x40);
            br = 0;
            r = 0;
        next:
            k++;
        } while (BR(0x004b32e9, k <= Se));
    }
    if (BR(0x004b32f1, r > 0) || !BR(0x004b32f9, br == 0)) {
        fu(e, 0x38) += 1;
        fu(e, 0x3c) += br;
        if (BR(0x004b3317, fu(e, 0x38) == 0x7fffu) || !BR(0x004b331f, fu(e, 0x3c) <= 0x3a9u)) kEmitEobrun(e);
    }
    ph_tail(cinfo, e, 0x004b334a, 0x004b3351);
    return 1;
}

// 0x004b33c0  finish_pass_gather_phuff(cinfo): emit_eobrun 0x004b25d0 on the encoder (cinfo+0x164, 0x004b33d3) to
// count a pending EOB run, is_DC_band = (Ss == 0) (0x004b33e5) and four did flags cleared (0x004b33ee..0x004b33fa).
// Per component of the scan (0x004b340a / 0x004b349d): a DC band uses dc_tbl_no (comp+0x14) unless Ah (cinfo+0x13c,
// re-read) is set, which skips the component (0x004b3425 / 0x004b342f); an AC band uses ac_tbl_no (+0x18). A table
// not done yet (0x004b3443) takes the dc or ac_huff_tbl_ptrs slot (cinfo+0x50 / +0x60, 0x004b344b), gets one from
// jpeg_alloc_huff_table 0x004afad0 when NULL (0x004b3454 / 0x004b3457), then jpeg_gen_optimal_table 0x004b1080 with
// count_ptrs[tbl] (+0x5c, 0x004b346e), and is marked done (0x004b347a).
VoidCinfo_t FinishPassGatherPhuff_orig;
void __cdecl FinishPassGatherPhuff_re(void* cinfo) {
    char* e = fp(cinfo, 0x164);
    kEmitEobrun(e);
    const int is_dc = fi(cinfo, 0x134) == 0 ? 1 : 0;
    int did[4] = {0, 0, 0, 0};
    if (BR(0x004b340a, fi(cinfo, 0xec) <= 0)) return;
    int ci = 0;
    do {
        char* comp = fp(cinfo, 0xf0 + ci * 4);
        int tbl;
        if (!BR(0x004b3425, is_dc == 0)) {
            if (BR(0x004b342f, fi(cinfo, 0x13c) != 0)) goto next;
            tbl = fi(comp, 0x14);
        } else {
            tbl = fi(comp, 0x18);
        }
        if (!BR(0x004b3443, did[tbl] != 0)) {
            void** slot = reinterpret_cast<void**>(cinfo) + ((BR(0x004b344b, is_dc != 0) ? 0x50 : 0x60) / 4) + tbl;
            if (!BR(0x004b3454, *slot != nullptr)) *slot = kAllocHuffTable(cinfo);
            kGenOptimalTable(cinfo, *slot, fpr(e, 0x5c + tbl * 4));
            did[tbl] = 1;
        }
    next:
        ci++;
    } while (BR(0x004b349d, ci < fi(cinfo, 0xec)));
}

// ================================================================================================================
// jcprepct.c
// ================================================================================================================

// 0x004b3b80  pre_process_data(cinfo, input_buf, in_row_ctr, in_rows_avail, output_buf, out_row_group_ctr,
// out_row_groups_avail): while *in_row_ctr is below in_rows_avail (unsigned: 0x004b3b9a / 0x004b3c9e) and
// *out_row_group_ctr below out_row_groups_avail (unsigned, 0x004b3baa): numrows = min(max_v_samp_factor
// (cinfo+0xe4) - next_buf_row (prep+0x34), in_rows_avail - *in_row_ctr) (unsigned, 0x004b3bbd..0x004b3bc5);
// color_convert (cinfo+0x158 slot +4) gets (cinfo, input_buf + *in_row_ctr rows, color_buf (prep+8), next_buf_row,
// numrows) (0x004b3bdf); *in_row_ctr and next_buf_row grow and rows_to_go (prep+0x30) shrinks by numrows
// (0x004b3be9..0x004b3bfe). At rows_to_go 0 (0x004b3c01) with next_buf_row below max_v_samp_factor (signed,
// 0x004b3c09) every component's color_buf (num_components cinfo+0x34, 0x004b3c12 / 0x004b3c38) is padded by
// expand_bottom_edge 0x004b3d00 (color_buf[ci], image_width cinfo+0x18, next_buf_row, max_v) and next_buf_row set to
// max_v (0x004b3c40). A full buffer (0x004b3c4e) goes to downsample (cinfo+0x15c slot +4) as (cinfo, color_buf, 0,
// output_buf, *out_row_group_ctr) (0x004b3c69), next_buf_row is cleared and *out_row_group_ctr incremented
// (0x004b3c6c..0x004b3c79). At rows_to_go 0 (0x004b3c86) with *out_row_group_ctr below the limit (0x004b3c90) each
// component's output_buf[ci] is padded by expand_bottom_edge (width_in_blocks * 8, *out * v_samp_factor, avail *
// v_samp_factor) (0x004b3cbc..0x004b3cd8, 0x004b3cb3 / 0x004b3cec), *out_row_group_ctr set to the limit (0x004b3cf6)
// and the function returns.
typedef void(__cdecl* PreProcessFn_t)(void* cinfo, void** input_buf, unsigned* in_row_ctr, unsigned in_rows_avail,
                                       void** output_buf, unsigned* out_row_group_ctr, unsigned out_row_groups_avail);
PreProcessFn_t PreProcessData_orig;
void __cdecl PreProcessData_re(void* cinfo, void** input_buf, unsigned* in_row_ctr, unsigned in_rows_avail,
                               void** output_buf, unsigned* out_row_group_ctr, unsigned out_row_groups_avail) {
    char* prep = fp(cinfo, 0x14c);
    if (BR(0x004b3b9a, *in_row_ctr >= in_rows_avail)) return;
    do {
        if (BR(0x004b3baa, *out_row_group_ctr >= out_row_groups_avail)) return;
        const unsigned inrows = in_rows_avail - *in_row_ctr;
        unsigned numrows = fu(cinfo, 0xe4) - fu(prep, 0x34);
        if (!BR(0x004b3bc3, numrows < inrows)) numrows = inrows;
        reinterpret_cast<ColorConvert_t>(fp(fp(cinfo, 0x158), 4))(cinfo, input_buf + *in_row_ctr, prep + 8,
                                                                  fu(prep, 0x34), static_cast<int>(numrows));
        *in_row_ctr += numrows;
        fu(prep, 0x34) += numrows;
        fu(prep, 0x30) -= numrows;
        if (!BR(0x004b3c01, fi(prep, 0x30) != 0) && !BR(0x004b3c09, fi(prep, 0x34) >= fi(cinfo, 0xe4))) {
            if (!BR(0x004b3c12, fi(cinfo, 0x34) <= 0)) {
                int ci = 0;
                do {
                    kExpandBottomEdge(fpr(prep, 8 + ci * 4), fu(cinfo, 0x18), fi(prep, 0x34), fi(cinfo, 0xe4));
                    ci++;
                } while (BR(0x004b3c38, ci < fi(cinfo, 0x34)));
            }
            fi(prep, 0x34) = fi(cinfo, 0xe4);
        }
        if (!BR(0x004b3c4e, fi(prep, 0x34) != fi(cinfo, 0xe4))) {
            reinterpret_cast<Downsample_t>(fp(fp(cinfo, 0x15c), 4))(cinfo, prep + 8, 0, output_buf, *out_row_group_ctr);
            fi(prep, 0x34) = 0;
            *out_row_group_ctr += 1;
        }
        if (!BR(0x004b3c86, fi(prep, 0x30) != 0) && BR(0x004b3c90, *out_row_group_ctr < out_row_groups_avail)) {
            char* comp = fp(cinfo, 0x3c);
            if (!BR(0x004b3cb3, fi(cinfo, 0x34) <= 0)) {
                int ci = 0;
                do {
                    kExpandBottomEdge(output_buf[ci], fu(comp, 0x1c) << 3,
                                      static_cast<int>(fu(comp, 0xc) * *out_row_group_ctr),
                                      static_cast<int>(fu(comp, 0xc) * out_row_groups_avail));
                    ci++;
                    comp += 0x54;
                } while (BR(0x004b3cec, ci < fi(cinfo, 0x34)));
            }
            *out_row_group_ctr = out_row_groups_avail;
            return;
        }
    } while (BR(0x004b3c9e, *in_row_ctr < in_rows_avail));
}

// 0x004b3d40  pre_process_context(...same arguments...): buf_height = 3 * max_v_samp_factor (0x004b3d5d). While
// *out_row_group_ctr is below out_row_groups_avail (unsigned: 0x004b3d6a / 0x004b3f02): with input left (unsigned,
// 0x004b3d7c), numrows = min(next_buf_stop (prep+0x3c) - next_buf_row (prep+0x34), in_rows_avail - *in_row_ctr)
// (unsigned, 0x004b3d82..0x004b3d98) rows go through color_convert (0x004b3daf); on the first pass (rows_to_go
// prep+0x30 still equal to image_height cinfo+0x1c, 0x004b3dbd) every component's color_buf gets its row 0 copied
// to rows -1 .. -max_v by jcopy_sample_rows 0x004b04f0 (0x004b3dcc..0x004b3e26); then *in_row_ctr and next_buf_row
// grow and rows_to_go shrinks by numrows (0x004b3e2c..0x004b3e43). Without input, a non-zero rows_to_go returns
// (0x004b3e4d); at the bottom, next_buf_row below next_buf_stop (signed, 0x004b3e5b) pads each color_buf with
// expand_bottom_edge 0x004b3d00 (0x004b3e64..0x004b3e89) and sets next_buf_row = next_buf_stop (0x004b3e92). When
// next_buf_row equals next_buf_stop (0x004b3e9d), downsample gets (cinfo, color_buf, this_row_group (prep+0x38),
// output_buf, *out_row_group_ctr) (0x004b3eb6), *out_row_group_ctr is incremented, this_row_group advances by max_v
// and wraps to 0 at buf_height (signed, 0x004b3ed7), next_buf_row wraps to 0 at buf_height (0x004b3ee3), and
// next_buf_stop = next_buf_row + max_v (0x004b3eec..0x004b3ef7).
PreProcessFn_t PreProcessContext_orig;
void __cdecl PreProcessContext_re(void* cinfo, void** input_buf, unsigned* in_row_ctr, unsigned in_rows_avail,
                                  void** output_buf, unsigned* out_row_group_ctr, unsigned out_row_groups_avail) {
    char* prep = fp(cinfo, 0x14c);
    const int buf_height = fi(cinfo, 0xe4) * 3;
    if (BR(0x004b3d6a, *out_row_group_ctr >= out_row_groups_avail)) return;
    do {
        if (!BR(0x004b3d7c, *in_row_ctr >= in_rows_avail)) {
            const unsigned inrows = in_rows_avail - *in_row_ctr;
            unsigned numrows = fu(prep, 0x3c) - fu(prep, 0x34);
            if (!BR(0x004b3d92, numrows < inrows)) numrows = inrows;
            reinterpret_cast<ColorConvert_t>(fp(fp(cinfo, 0x158), 4))(cinfo, input_buf + *in_row_ctr, prep + 8,
                                                                      fu(prep, 0x34), static_cast<int>(numrows));
            if (!BR(0x004b3dbd, fu(prep, 0x30) != fu(cinfo, 0x1c))) {
                if (!BR(0x004b3dcc, fi(cinfo, 0x34) <= 0)) {
                    int ci = 0;
                    do {
                        int row = 1;
                        if (!BR(0x004b3ddf, fi(cinfo, 0xe4) < 1)) {
                            do {
                                void* rows = fpr(prep, 8 + ci * 4);
                                kCopySampleRows(rows, 0, rows, -row, 1, fu(cinfo, 0x18));
                                row++;
                            } while (BR(0x004b3e07, row <= fi(cinfo, 0xe4)));
                        }
                        ci++;
                    } while (BR(0x004b3e26, ci < fi(cinfo, 0x34)));
                }
            }
            *in_row_ctr += numrows;
            fu(prep, 0x34) += numrows;
            fu(prep, 0x30) -= numrows;
        } else {
            if (BR(0x004b3e4d, fi(prep, 0x30) != 0)) return;
            if (!BR(0x004b3e5b, fi(prep, 0x34) >= fi(prep, 0x3c))) {
                if (!BR(0x004b3e64, fi(cinfo, 0x34) <= 0)) {
                    int ci = 0;
                    do {
                        kExpandBottomEdge(fpr(prep, 8 + ci * 4), fu(cinfo, 0x18), fi(prep, 0x34), fi(prep, 0x3c));
                        ci++;
                    } while (BR(0x004b3e89, ci < fi(cinfo, 0x34)));
                }
                fi(prep, 0x34) = fi(prep, 0x3c);
            }
        }
        if (!BR(0x004b3e9d, fi(prep, 0x34) != fi(prep, 0x3c))) {
            reinterpret_cast<Downsample_t>(fp(fp(cinfo, 0x15c), 4))(cinfo, prep + 8, fu(prep, 0x38), output_buf,
                                                                    *out_row_group_ctr);
            *out_row_group_ctr += 1;
            fi(prep, 0x38) += fi(cinfo, 0xe4);
            if (!BR(0x004b3ed7, fi(prep, 0x38) < buf_height)) fi(prep, 0x38) = 0;
            if (!BR(0x004b3ee3, fi(prep, 0x34) < buf_height)) fi(prep, 0x34) = 0;
            fi(prep, 0x3c) = fi(cinfo, 0xe4) + fi(prep, 0x34);
        }
    } while (BR(0x004b3f02, *out_row_group_ctr < out_row_groups_avail));
}

}  // namespace

// 0x004b05f0  start_pass_main
SG_HOOK("golf_clean.exe", 0x004b05f0, start_pass_main, StartPassMain_re, StartPassMain_orig);
// 0x004b0640  process_data_simple_main
SG_HOOK("golf_clean.exe", 0x004b0640, process_data_simple_main, ProcessDataSimpleMain_re, ProcessDataSimpleMain_orig);
// 0x004b07b0  start_pass_coef
SG_HOOK("golf_clean.exe", 0x004b07b0, start_pass_coef, StartPassCoef_re, StartPassCoef_orig);
// 0x004b08b0  compress_data
SG_HOOK("golf_clean.exe", 0x004b08b0, compress_data, CompressData_re, CompressData_orig);
// 0x004b0b20  compress_first_pass
SG_HOOK("golf_clean.exe", 0x004b0b20, compress_first_pass, CompressFirstPass_re, CompressFirstPass_orig);
// 0x004b0d70  compress_output
SG_HOOK("golf_clean.exe", 0x004b0d70, compress_output, CompressOutput_re, CompressOutput_orig);
// 0x004b1f30  jinit_phuff_encoder
SG_HOOK("golf_clean.exe", 0x004b1f30, jinit_phuff_encoder, JinitPhuffEncoder_re, JinitPhuffEncoder_orig);
// 0x004b1f70  start_pass_phuff
SG_HOOK("golf_clean.exe", 0x004b1f70, start_pass_phuff, StartPassPhuff_re, StartPassPhuff_orig);
// 0x004b2140  encode_mcu_DC_first
SG_HOOK("golf_clean.exe", 0x004b2140, encode_mcu_DC_first, EncodeMcuDcFirst_re, EncodeMcuDcFirst_orig);
// 0x004b2460  emit_restart_4b2460
SG_HOOK("golf_clean.exe", 0x004b2460, emit_restart_4b2460, EmitRestartP_re, EmitRestartP_orig);
// 0x004b28c0  encode_mcu_AC_first
SG_HOOK("golf_clean.exe", 0x004b28c0, encode_mcu_AC_first, EncodeMcuAcFirst_re, EncodeMcuAcFirst_orig);
// 0x004b2d30  encode_mcu_DC_refine
SG_HOOK("golf_clean.exe", 0x004b2d30, encode_mcu_DC_refine, EncodeMcuDcRefine_re, EncodeMcuDcRefine_orig);
// 0x004b2eb0  encode_mcu_AC_refine
SG_HOOK("golf_clean.exe", 0x004b2eb0, encode_mcu_AC_refine, EncodeMcuAcRefine_re, EncodeMcuAcRefine_orig);
// 0x004b33c0  finish_pass_gather_phuff
SG_HOOK("golf_clean.exe", 0x004b33c0, finish_pass_gather_phuff, FinishPassGatherPhuff_re, FinishPassGatherPhuff_orig);
// 0x004b3b80  pre_process_data
SG_HOOK("golf_clean.exe", 0x004b3b80, pre_process_data, PreProcessData_re, PreProcessData_orig);
// 0x004b3d40  pre_process_context
SG_HOOK("golf_clean.exe", 0x004b3d40, pre_process_context, PreProcessContext_re, PreProcessContext_orig);

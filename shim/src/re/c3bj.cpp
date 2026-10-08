// C3 batch c3bj (round 15, 2026-10-08) of golf_clean.exe. Nine functions: eight IJG libjpeg 6a compress-side
// setup routines (the module initialisers and the parameter/dimension setup the exe links from
// original\JPEG.lib) and one Bink player setter.
//
// Every body is hand-written from the disassembly (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr>
// --list) and cites the address of each instruction it reproduces. __thiscall is emulated with __fastcall
// (ecx = this, edx unused). Callees run through their original addresses:
//   jinit_c_prep_controller -> create_context_buffer 0x004b3f10 (in THIS batch: re-run with its hook off)
//   initial_setup / per_scan_setup -> jdiv_round_up 0x004b04c0 (C3, c3r.cpp)
// The memory manager (cinfo+4) and the error manager (cinfo+0) are reached through their own slots, so the
// fixture's callbacks are what both arms call.
//
//   jinit_c_main_controller 0x004b0560   jpeg_set_colorspace   0x004ae990   initial_setup        0x004b52a0
//   jinit_c_prep_controller 0x004b3a80   jinit_downsampler     0x004b4040   per_scan_setup       0x004b5ad0
//   create_context_buffer   0x004b3f10   jinit_color_converter 0x004b4a60   BinkPlayer::setTarget 0x00487050
#include "hooks.h"

namespace {

// ---- IJG libjpeg 6a structures reached through pointers -------------------------------------------------------
// A jpeg_compress_struct is referenced through cinfo+0 = err (jpeg_error_mgr: error_exit +0, emit_message +4,
// msg_code +0x14, msg_parm.i[0] +0x18, msg_parm.i[1] +0x1c) and cinfo+4 = mem (jpeg_memory_mgr: alloc_small +0,
// alloc_large +4, alloc_sarray +8). The compress fields these functions use are global_state +0x10,
// image_width +0x18, image_height +0x1c, input_components +0x20, in_color_space +0x24, data_precision +0x30,
// num_components +0x34, jpeg_color_space +0x38, comp_info +0x3c, raw_data_in +0xa8, CCIR601_sampling +0xb4,
// smoothing_factor +0xb8, restart_interval +0xc0, restart_in_rows +0xc4, write_JFIF_header +0xc8,
// write_Adobe_marker +0xd4, max_h_samp_factor +0xe0, max_v_samp_factor +0xe4, total_iMCU_rows +0xe8,
// comps_in_scan +0xec, cur_comp_info[] +0xf0, MCUs_per_row +0x100, MCU_rows_in_scan +0x104, blocks_in_MCU +0x108,
// MCU_membership[] +0x10c, main +0x148, prep +0x14c, cconvert +0x158, downsample +0x15c. A jpeg_component_info
// is 0x54 bytes: component_id +0, component_index +4, h_samp_factor +8, v_samp_factor +0xc, quant_tbl_no +0x10,
// dc_tbl_no +0x14, ac_tbl_no +0x18, width_in_blocks +0x1c, height_in_blocks +0x20, DCT_scaled_size +0x24,
// downsampled_width +0x28, downsampled_height +0x2c, component_needed +0x30, MCU_width +0x34, MCU_height +0x38,
// MCU_blocks +0x3c, MCU_sample_width +0x40, last_col_width +0x44, last_row_height +0x48.
typedef void*(__cdecl* AllocSmall_t)(void* cinfo, int pool, int size);
typedef void*(__cdecl* AllocSarray_t)(void* cinfo, int pool, int samplesperrow, int numrows);
typedef void(__cdecl* ErrExit_t)(void* cinfo);
typedef void(__cdecl* EmitMsg_t)(void* cinfo, int level);

static inline void* field_ptr(void* base, unsigned off) {
    return *reinterpret_cast<void**>(reinterpret_cast<char*>(base) + off);
}
static inline int& field_i(void* base, unsigned off) {
    return *reinterpret_cast<int*>(reinterpret_cast<char*>(base) + off);
}
static inline unsigned& field_u(void* base, unsigned off) {
    return *reinterpret_cast<unsigned*>(reinterpret_cast<char*>(base) + off);
}
// ERREXIT: store the message code in cinfo->err->msg_code and call cinfo->err->error_exit(cinfo). Each half
// re-reads cinfo+0 exactly as the originals do.
static inline void errexit(void* cinfo, int code) {
    field_i(field_ptr(cinfo, 0), 0x14) = code;
    reinterpret_cast<ErrExit_t>(field_ptr(field_ptr(cinfo, 0), 0))(cinfo);
}

// jdiv_round_up 0x004b04c0 (C3, shim/src/re/c3r.cpp): (a + b - 1) / b, signed truncating division.
typedef int(__cdecl* JDivRoundUp_t)(int a, int b);
const JDivRoundUp_t kJDivRoundUp = reinterpret_cast<JDivRoundUp_t>(0x004b04c0);

// create_context_buffer 0x004b3f10, called by jinit_c_prep_controller through its original address.
typedef void(__cdecl* CreateCtxBuf_t)(void* cinfo);
const CreateCtxBuf_t kCreateContextBuffer = reinterpret_cast<CreateCtxBuf_t>(0x004b3f10);

// 0x004b0560  jinit_c_main_controller(cinfo, need_full_buffer): create the main controller. A 0x40-byte object
// comes from mem->alloc_small(cinfo, 1, 0x40) (0x004b0566..0x004b056e), is stored at cinfo+0x148 (0x004b0570)
// and gets start_pass = 0x004b05f0 at +0 (0x004b0576). With raw_data_in (cinfo+0xa8) non-zero there is nothing
// else to do (0x004b057c / 0x004b0587 -> 0x004b05e4), and that test comes BEFORE the need_full_buffer one. A
// non-zero need_full_buffer then raises ERREXIT 4 and returns (0x004b0589 / 0x004b058f / 0x004b0594 /
// 0x004b059d / 0x004b05a4). Otherwise one strip buffer per component is allocated through
// mem->alloc_sarray(cinfo, 1, width_in_blocks * 8, v_samp_factor * 8) (comp+0x1c at 0x004b05bc and comp+0xc at
// 0x004b05b9, both shifted left by 3 at 0x004b05c1/0x004b05c4) and stored in the buffer array at main+0x18
// (0x004b05d2). num_components (cinfo+0x34) is re-read at the bottom of every pass (0x004b05d4) and the
// component cursor advances by 0x54 (0x004b05db); the loop is entered only when the first read is positive
// (0x004b05af).
typedef void(__cdecl* JinitCMain_t)(void* cinfo, int need_full_buffer);
JinitCMain_t JinitCMain_orig;
void __cdecl JinitCMain_re(void* cinfop, int need_full_buffer) {
    char* cinfo = reinterpret_cast<char*>(cinfop);
    AllocSmall_t alloc_small = reinterpret_cast<AllocSmall_t>(field_ptr(field_ptr(cinfop, 4), 0));
    char* mainc = reinterpret_cast<char*>(alloc_small(cinfop, 1, 0x40));
    *reinterpret_cast<void**>(cinfo + 0x148) = mainc;
    *reinterpret_cast<void**>(mainc + 0) = reinterpret_cast<void*>(0x004b05f0);   // start_pass
    if (field_i(cinfo, 0xa8) != 0) return;                                        // raw_data_in
    if (need_full_buffer != 0) {
        errexit(cinfop, 4);                                                       // JERR_BAD_BUFFER_MODE
        return;
    }
    int n = field_i(cinfo, 0x34);
    char* comp = reinterpret_cast<char*>(field_ptr(cinfop, 0x3c));
    for (int ci = 0; ci < n; ci++) {
        char* cp = comp + ci * 0x54;
        AllocSarray_t alloc_sarray = reinterpret_cast<AllocSarray_t>(field_ptr(field_ptr(cinfop, 4), 8));
        int rows = field_i(cp, 0x0c) << 3;                                        // v_samp_factor * 8
        int spr = field_i(cp, 0x1c) << 3;                                         // width_in_blocks * 8
        *reinterpret_cast<void**>(mainc + 0x18 + ci * 4) = alloc_sarray(cinfop, 1, spr, rows);
        n = field_i(cinfo, 0x34);                                                 // 0x004b05d4
    }
}

// 0x004b3a80  jinit_c_prep_controller(cinfo, need_full_buffer): create the pre-processing controller. Unlike
// 0x004b0560 the need_full_buffer check comes FIRST and does not return: ERREXIT 4 is raised (0x004b3a8c /
// 0x004b3a91 / 0x004b3a9a) and the rest of the function still runs. A 0x40-byte object comes from
// mem->alloc_small(cinfo, 1, 0x40) (0x004b3aa2..0x004b3aa7), is stored at cinfo+0x14c (0x004b3aa9) and gets
// start_pass = 0x004b3b30 at +0 (0x004b3aaf). The downsampler at cinfo+0x15c decides the rest: its
// need_context_rows (downsample+8, 0x004b3abe / 0x004b3ac3) selects pre_process_data = 0x004b3d40 at prep+4
// plus create_context_buffer 0x004b3f10 (0x004b3ac6 / 0x004b3acd), or pre_process_data = 0x004b3b80
// (0x004b3ad8) and one colour-conversion buffer per component in the array at prep+8 (0x004b3b17). Each buffer
// is mem->alloc_sarray(cinfo, 1, width_in_blocks * max_h_samp_factor * 8 / h_samp_factor, max_v_samp_factor):
// comp+0x1c times cinfo+0xe0 (0x004b3af3 / 0x004b3afc), shifted left by 3 (0x004b3b03) and divided signed by
// comp+8 (0x004b3b0a / 0x004b3b0b), with cinfo+0xe4 pushed as the row count (0x004b3af6 / 0x004b3b06).
// num_components is re-read at the bottom of every pass (0x004b3b19).
typedef void(__cdecl* JinitCPrep_t)(void* cinfo, int need_full_buffer);
JinitCPrep_t JinitCPrep_orig;
void __cdecl JinitCPrep_re(void* cinfop, int need_full_buffer) {
    char* cinfo = reinterpret_cast<char*>(cinfop);
    if (need_full_buffer != 0) errexit(cinfop, 4);                                // JERR_BAD_BUFFER_MODE
    AllocSmall_t alloc_small = reinterpret_cast<AllocSmall_t>(field_ptr(field_ptr(cinfop, 4), 0));
    char* prep = reinterpret_cast<char*>(alloc_small(cinfop, 1, 0x40));
    *reinterpret_cast<void**>(cinfo + 0x14c) = prep;
    *reinterpret_cast<void**>(prep + 0) = reinterpret_cast<void*>(0x004b3b30);    // start_pass
    char* ds = reinterpret_cast<char*>(field_ptr(cinfop, 0x15c));
    if (field_i(ds, 8) != 0) {                                                    // need_context_rows
        *reinterpret_cast<void**>(prep + 4) = reinterpret_cast<void*>(0x004b3d40);
        kCreateContextBuffer(cinfop);
        return;
    }
    *reinterpret_cast<void**>(prep + 4) = reinterpret_cast<void*>(0x004b3b80);
    int n = field_i(cinfo, 0x34);
    char* comp = reinterpret_cast<char*>(field_ptr(cinfop, 0x3c));
    for (int ci = 0; ci < n; ci++) {
        char* cp = comp + ci * 0x54;
        int rows = field_i(cinfo, 0xe4);                                          // max_v_samp_factor
        int spr = (field_i(cp, 0x1c) * field_i(cinfo, 0xe0)) << 3;
        spr = spr / field_i(cp, 8);                                               // signed idiv by h_samp_factor
        AllocSarray_t alloc_sarray = reinterpret_cast<AllocSarray_t>(field_ptr(field_ptr(cinfop, 4), 8));
        *reinterpret_cast<void**>(prep + 8 + ci * 4) = alloc_sarray(cinfop, 1, spr, rows);
        n = field_i(cinfo, 0x34);                                                 // 0x004b3b19
    }
}

// 0x004b3f10  create_context_buffer(cinfo): give every component a colour buffer with one row group of context
// above and below. One block of num_components * max_v_samp_factor * 5 pointers comes from
// mem->alloc_small(cinfo, 1, that size) (0x004b3f23 imul, 0x004b3f2f lea *5, 0x004b3f32 shl 2, call at
// 0x004b3f39) and is kept in the caller's argument slot (0x004b3f41). For each component a real buffer of
// 3 * max_v_samp_factor rows comes from mem->alloc_sarray(cinfo, 1, width_in_blocks * max_h_samp_factor * 8 /
// h_samp_factor, 3 * max_v_samp_factor) (0x004b3f7b, 0x004b3f82..0x004b3f96); its row pointers are copied into
// the middle of the fake array, at fake + max_v_samp_factor (0x004b3fa0, rep movsd of 3 * max_v dwords counted
// in bytes at 0x004b3f59 / 0x004b3fa9). The wraparound pointers follow (0x004b3fb7 / 0x004b3fec): fake[i] takes
// true[2 * max_v + i] (0x004b3fc8 / 0x004b3fd1 / 0x004b3fd6) and fake[4 * max_v + i] takes true[i]
// (0x004b3fc1 shl 4 / 0x004b3fd9 / 0x004b3fdb), for i below max_v_samp_factor. The component's slot in the
// colour-buffer array at prep+8 (cinfo+0x14c read at 0x004b3f29, cursor set at 0x004b3f62) gets fake + max_v
// (0x004b3ff9) and the fake cursor advances by 5 * max_v pointers (0x004b3ff6 / 0x004b4002). num_components is
// re-read at the bottom of every pass (0x004b4015).
typedef void(__cdecl* CreateCtx_t)(void* cinfo);
CreateCtx_t CreateCtx_orig;
void __cdecl CreateCtx_re(void* cinfop) {
    char* cinfo = reinterpret_cast<char*>(cinfop);
    int max_v = field_i(cinfo, 0xe4);
    int n = field_i(cinfo, 0x34);
    AllocSmall_t alloc_small = reinterpret_cast<AllocSmall_t>(field_ptr(field_ptr(cinfop, 4), 0));
    char* prep = reinterpret_cast<char*>(field_ptr(cinfop, 0x14c));
    char** fake = reinterpret_cast<char**>(alloc_small(cinfop, 1, (n * max_v) * 5 * 4));
    char* comp = reinterpret_cast<char*>(field_ptr(cinfop, 0x3c));
    char** colour_buf = reinterpret_cast<char**>(prep + 8);
    for (int ci = 0; ci < n; ci++) {
        char* cp = comp + ci * 0x54;
        AllocSarray_t alloc_sarray = reinterpret_cast<AllocSarray_t>(field_ptr(field_ptr(cinfop, 4), 8));
        int spr = ((field_i(cp, 0x1c) * field_i(cinfo, 0xe0)) << 3) / field_i(cp, 8);
        char** truebuf = reinterpret_cast<char**>(alloc_sarray(cinfop, 1, spr, 3 * max_v));
        for (int k = 0; k < 3 * max_v; k++) fake[max_v + k] = truebuf[k];         // rep movsd, 0x004b3fb0
        for (int i = 0; i < max_v; i++) {                                         // 0x004b3fbb / 0x004b3fec
            fake[i] = truebuf[2 * max_v + i];
            fake[4 * max_v + i] = truebuf[i];
        }
        colour_buf[ci] = reinterpret_cast<char*>(fake + max_v);
        fake += 5 * max_v;
        n = field_i(cinfo, 0x34);                                                 // 0x004b4015
    }
}

// 0x004ae990  jpeg_set_colorspace(cinfo, colorspace): choose the JPEG colour space and the component table that
// goes with it. A global_state (cinfo+0x10) other than 0x64 raises ERREXIT 0x12 with the state in msg_parm[0]
// (0x004ae997 / 0x004ae99b / 0x004ae9a0 / 0x004ae9ac / 0x004ae9b1) and, because the fixture's error_exit
// returns, the rest still runs. jpeg_color_space (cinfo+0x38) takes the argument and both marker flags,
// write_JFIF_header (cinfo+0xc8) and write_Adobe_marker (cinfo+0xd4), are cleared (0x004ae9bf..0x004ae9c8).
// The argument then selects a case through the UNSIGNED bound 5 (0x004ae9cc / 0x004ae9ce) and the jump table at
// 0x004aec5c; anything above raises ERREXIT 8 (0x004aec4a / 0x004aec53). Each case sets num_components
// (cinfo+0x34) and fills the first entries of comp_info (cinfo+0x3c, stride 0x54) with
// {component_id +0, h_samp_factor +8, v_samp_factor +0xc, quant_tbl_no +0x10, dc_tbl_no +0x14, ac_tbl_no +0x18}:
//   1 GRAYSCALE 0x004ae9db: JFIF flag, 1 component  {1, 1,1, 0,0,0}
//   2 RGB       0x004aea01: Adobe flag, 3 components {0x52,1,1,0,0,0} {0x47,1,1,0,0,0} {0x42,1,1,0,0,0}
//   3 YCbCr     0x004aea69: JFIF flag, 3 components  {1,2,2,0,0,0} {2,1,1,1,1,1} {3,1,1,1,1,1}
//   4 CMYK      0x004aead0: Adobe flag, 4 components {0x43,..} {0x4d,..} {0x59,..} {0x4b,..}, all 1,1,0,0,0
//   5 YCCK      0x004aeb58: Adobe flag, 4 components {1,2,2,0,0,0} {2,1,1,1,1,1} {3,1,1,1,1,1} {4,2,2,0,0,0}
//   0 UNKNOWN   0x004aebdd: num_components = input_components (cinfo+0x20, 0x004aebdd / 0x004aebe7); outside
//     1..10 (0x004aebea jl / 0x004aebef jle) ERREXIT 0x18 carries the count and 10 in msg_parm[0]/[1]
//     (0x004aebf4..0x004aec0e), then every component gets {ci, 1,1, 0,0,0} (loop 0x004aec1a / 0x004aec40, with
//     num_components re-read at 0x004aec3a).
typedef void(__cdecl* SetColorspace_t)(void* cinfo, int colorspace);
SetColorspace_t SetColorspace_orig;
// The SET_COMP of the original: one entry of comp_info gets its id and its six table/sampling fields.
static inline void set_comp(char* comp, int index, int id, int h, int v, int q, int dc, int ac) {
    char* cp = comp + index * 0x54;
    field_i(cp, 0x00) = id;
    field_i(cp, 0x08) = h;
    field_i(cp, 0x0c) = v;
    field_i(cp, 0x10) = q;
    field_i(cp, 0x14) = dc;
    field_i(cp, 0x18) = ac;
}
void __cdecl SetColorspace_re(void* cinfop, int colorspace) {
    char* cinfo = reinterpret_cast<char*>(cinfop);
    if (field_i(cinfo, 0x10) != 0x64) {                                           // CSTATE_START
        field_i(field_ptr(cinfop, 0), 0x14) = 0x12;                               // JERR_BAD_STATE
        field_i(field_ptr(cinfop, 0), 0x18) = field_i(cinfo, 0x10);
        reinterpret_cast<ErrExit_t>(field_ptr(field_ptr(cinfop, 0), 0))(cinfop);
    }
    field_i(cinfo, 0x38) = colorspace;
    field_i(cinfo, 0xc8) = 0;
    field_i(cinfo, 0xd4) = 0;
    char* comp = reinterpret_cast<char*>(field_ptr(cinfop, 0x3c));
    switch (static_cast<unsigned>(colorspace)) {
    case 1:
        field_i(cinfo, 0xc8) = 1;
        field_i(cinfo, 0x34) = 1;
        set_comp(comp, 0, 1, 1, 1, 0, 0, 0);
        break;
    case 2:
        field_i(cinfo, 0xd4) = 1;
        field_i(cinfo, 0x34) = 3;
        set_comp(comp, 0, 0x52, 1, 1, 0, 0, 0);
        set_comp(comp, 1, 0x47, 1, 1, 0, 0, 0);
        set_comp(comp, 2, 0x42, 1, 1, 0, 0, 0);
        break;
    case 3:
        field_i(cinfo, 0xc8) = 1;
        field_i(cinfo, 0x34) = 3;
        set_comp(comp, 0, 1, 2, 2, 0, 0, 0);
        set_comp(comp, 1, 2, 1, 1, 1, 1, 1);
        set_comp(comp, 2, 3, 1, 1, 1, 1, 1);
        break;
    case 4:
        field_i(cinfo, 0xd4) = 1;
        field_i(cinfo, 0x34) = 4;
        set_comp(comp, 0, 0x43, 1, 1, 0, 0, 0);
        set_comp(comp, 1, 0x4d, 1, 1, 0, 0, 0);
        set_comp(comp, 2, 0x59, 1, 1, 0, 0, 0);
        set_comp(comp, 3, 0x4b, 1, 1, 0, 0, 0);
        break;
    case 5:
        field_i(cinfo, 0xd4) = 1;
        field_i(cinfo, 0x34) = 4;
        set_comp(comp, 0, 1, 2, 2, 0, 0, 0);
        set_comp(comp, 1, 2, 1, 1, 1, 1, 1);
        set_comp(comp, 2, 3, 1, 1, 1, 1, 1);
        set_comp(comp, 3, 4, 2, 2, 0, 0, 0);
        break;
    case 0: {
        int ncomp = field_i(cinfo, 0x20);                                         // input_components
        field_i(cinfo, 0x34) = ncomp;
        if (ncomp < 1 || ncomp > 0xa) {
            field_i(field_ptr(cinfop, 0), 0x14) = 0x18;                           // JERR_COMPONENT_COUNT
            field_i(field_ptr(cinfop, 0), 0x18) = field_i(cinfo, 0x34);
            field_i(field_ptr(cinfop, 0), 0x1c) = 0xa;
            reinterpret_cast<ErrExit_t>(field_ptr(field_ptr(cinfop, 0), 0))(cinfop);
        }
        int n = field_i(cinfo, 0x34);
        for (int ci = 0; ci < n; ci++) {
            set_comp(comp, ci, ci, 1, 1, 0, 0, 0);
            n = field_i(cinfo, 0x34);                                             // 0x004aec3a
        }
        break;
    }
    default:
        field_i(field_ptr(cinfop, 0), 0x14) = 8;                                  // JERR_BAD_J_COLORSPACE
        reinterpret_cast<ErrExit_t>(field_ptr(field_ptr(cinfop, 0), 0))(cinfop);
        break;
    }
}

// 0x004b4040  jinit_downsampler(cinfo): create the downsampler and pick one sampling method per component. A
// 0x34-byte object comes from mem->alloc_small(cinfo, 1, 0x34) (0x004b404a..0x004b405a), is stored at
// cinfo+0x15c (0x004b4060) and gets start_pass = 0x004b01e0 at +0, downsample = 0x004b41d0 at +4 and
// need_context_rows = 0 at +8 (0x004b4069..0x004b4077). CCIR601_sampling (cinfo+0xb4) non-zero raises
// ERREXIT 0x17 (0x004b407a / 0x004b4086 / 0x004b408b / 0x004b4094) and, the fixture's error_exit returning,
// execution continues. The per-component method goes into the array at downsample+0xc (0x004b40af) and is
// chosen by comparing h_samp_factor (comp+8) and v_samp_factor (comp+0xc) with max_h_samp_factor (cinfo+0xe0)
// and max_v_samp_factor (cinfo+0xe4):
//   both equal to the maxima (0x004b40bd / 0x004b40c7): 0x004b48c0 and need_context_rows = 1 when
//     smoothing_factor (cinfo+0xb8) is non-zero (0x004b40d1 / 0x004b40d3 / 0x004b40d9), otherwise 0x004b43f0
//     (0x004b40e5);
//   h doubled equal to max_h with v equal to max_v (0x004b40f0 / 0x004b40f5 / 0x004b40f9 / 0x004b40ff):
//     0x004b4440 and the smoothing flag in the frame slot cleared (0x004b4105 / 0x004b410d);
//   h and v both doubled equal to the maxima (0x004b411d / 0x004b4121 / 0x004b4127): 0x004b45d0 with
//     need_context_rows = 1 when smoothing_factor is non-zero (0x004b4131 / 0x004b4133 / 0x004b4139),
//     otherwise 0x004b44f0 (0x004b4142);
//   max_h divisible by h and max_v divisible by v, both SIGNED (0x004b414b / 0x004b414f and 0x004b4158 /
//     0x004b415c): 0x004b4260 with the smoothing flag cleared (0x004b415e / 0x004b4162);
//   anything else: ERREXIT 0x25 (0x004b416d / 0x004b4176).
// The compare at 0x004b4119 repeats the one at 0x004b40f5 on the same registers, so its jne side cannot be
// taken. num_components is re-read at the bottom of every pass (0x004b417f). Finally a non-zero
// smoothing_factor with the frame's smoothing flag cleared (0x004b4196 / 0x004b419e / 0x004b41a0 / 0x004b41a6)
// emits the trace message 0x62 through err->emit_message (err+4) with level 0 (0x004b41aa..0x004b41b6).
typedef void(__cdecl* JinitDownsampler_t)(void* cinfo);
JinitDownsampler_t JinitDownsampler_orig;
void __cdecl JinitDownsampler_re(void* cinfop) {
    char* cinfo = reinterpret_cast<char*>(cinfop);
    int smoothok = 1;                                                             // 0x004b4052
    AllocSmall_t alloc_small = reinterpret_cast<AllocSmall_t>(field_ptr(field_ptr(cinfop, 4), 0));
    char* ds = reinterpret_cast<char*>(alloc_small(cinfop, 1, 0x34));
    *reinterpret_cast<void**>(cinfo + 0x15c) = ds;
    *reinterpret_cast<void**>(ds + 0) = reinterpret_cast<void*>(0x004b01e0);      // start_pass
    *reinterpret_cast<void**>(ds + 4) = reinterpret_cast<void*>(0x004b41d0);      // downsample
    field_i(ds, 8) = 0;                                                           // need_context_rows
    if (field_i(cinfo, 0xb4) != 0) errexit(cinfop, 0x17);                         // JERR_CCIR601_NOTIMPL
    int n = field_i(cinfo, 0x34);
    char* comp = reinterpret_cast<char*>(field_ptr(cinfop, 0x3c));
    for (int ci = 0; ci < n; ci++) {
        char* cp = comp + ci * 0x54;
        void** slot = reinterpret_cast<void**>(ds + 0xc + ci * 4);
        int h = field_i(cp, 8);
        int maxh = field_i(cinfo, 0xe0);
        if (h == maxh && field_i(cp, 0xc) == field_i(cinfo, 0xe4)) {
            if (field_i(cinfo, 0xb8) != 0) {
                *slot = reinterpret_cast<void*>(0x004b48c0);
                field_i(ds, 8) = 1;
            } else {
                *slot = reinterpret_cast<void*>(0x004b43f0);
            }
        } else if (h * 2 == maxh && field_i(cp, 0xc) == field_i(cinfo, 0xe4)) {
            smoothok = 0;
            *slot = reinterpret_cast<void*>(0x004b4440);
        } else if (h * 2 == maxh && field_i(cp, 0xc) * 2 == field_i(cinfo, 0xe4)) {
            if (field_i(cinfo, 0xb8) != 0) {
                *slot = reinterpret_cast<void*>(0x004b45d0);
                field_i(ds, 8) = 1;
            } else {
                *slot = reinterpret_cast<void*>(0x004b44f0);
            }
        } else if (maxh % h == 0 && field_i(cinfo, 0xe4) % field_i(cp, 0xc) == 0) {
            smoothok = 0;
            *slot = reinterpret_cast<void*>(0x004b4260);
        } else {
            errexit(cinfop, 0x25);                                                // JERR_FRACT_SAMPLE_NOTIMPL
        }
        n = field_i(cinfo, 0x34);                                                 // 0x004b417f
    }
    if (field_i(cinfo, 0xb8) != 0 && smoothok == 0) {
        field_i(field_ptr(cinfop, 0), 0x14) = 0x62;                               // JTRC_SMOOTH_NOTIMPL
        reinterpret_cast<EmitMsg_t>(field_ptr(field_ptr(cinfop, 0), 4))(cinfop, 0);
    }
}

// 0x004b4a60  jinit_color_converter(cinfo): create the colour converter and check that the input and JPEG
// colour spaces can be paired. A 0xc-byte object comes from mem->alloc_small(cinfo, 1, 0xc)
// (0x004b4a67..0x004b4a6f), is stored at cinfo+0x158 (0x004b4a76) and gets start_pass = 0x004b01e0 at +0
// (0x004b4a81). The first switch checks input_components (cinfo+0x20) against in_color_space (cinfo+0x24)
// through the table at 0x004b4c70 indexed by in_color_space - 1 under the UNSIGNED bound 4 (0x004b4a8a /
// 0x004b4a8d / 0x004b4a8f): space 1 wants 1 component (0x004b4a96), spaces 2 and 3 want 3 (0x004b4a9e), spaces
// 4 and 5 want 4 (0x004b4ab4) and any other space only wants a positive count (0x004b4ac9 / 0x004b4acd); a
// mismatch raises ERREXIT 7. The second switch is on jpeg_color_space (cinfo+0x38) through the table at
// 0x004b4c84, same indexing (0x004b4ae3 / 0x004b4ae8 / 0x004b4aee). Each arm first checks num_components
// (cinfo+0x34) and raises ERREXIT 8 when it does not match (1 at 0x004b4af5, 3 at 0x004b4b49 and 0x004b4b7f,
// 4 at 0x004b4bcd and 0x004b4bfd), then selects the methods by in_color_space:
//   JPEG space 1: input 1 or 3 -> color_convert (+4) = 0x004b50d0 (0x004b4b14 / 0x004b4b3e); input 2 ->
//     start_pass (+0) = 0x004b4ca0 and +4 = 0x004b4eb0 (0x004b4b24); anything else ERREXIT 0x19 (0x004b4be8).
//   JPEG space 2: input 2 -> the null converter (0x004b4b64); anything else ERREXIT 0x19 (0x004b4b6d).
//   JPEG space 3: input 2 -> +0 = 0x004b4ca0, +4 = 0x004b4d80 (0x004b4b9e); input 3 -> the null converter
//     (0x004b4bb2); anything else ERREXIT 0x19 (0x004b4bbb).
//   JPEG space 4: input 4 -> the null converter (0x004b4be6); anything else ERREXIT 0x19 (0x004b4beb).
//   JPEG space 5: input 4 -> +0 = 0x004b4ca0, +4 = 0x004b4f60 (0x004b4c1a); input 5 -> the null converter
//     (0x004b4c2e); anything else ERREXIT 0x19 (0x004b4c33).
//   any other JPEG space: the null converter when it equals in_color_space and num_components equals
//     input_components (0x004b4c45 / 0x004b4c48 / 0x004b4c50 / 0x004b4c52), otherwise ERREXIT 0x19 followed by
//     the null converter anyway (0x004b4c57 / 0x004b4c60 falling into 0x004b4c65).
// The null converter is color_convert (+4) = 0x004b5140 with start_pass left at 0x004b01e0 (0x004b4c65).
typedef void(__cdecl* JinitCConvert_t)(void* cinfo);
JinitCConvert_t JinitCConvert_orig;
void __cdecl JinitCConvert_re(void* cinfop) {
    char* cinfo = reinterpret_cast<char*>(cinfop);
    AllocSmall_t alloc_small = reinterpret_cast<AllocSmall_t>(field_ptr(field_ptr(cinfop, 4), 0));
    char* cc = reinterpret_cast<char*>(alloc_small(cinfop, 1, 0xc));
    *reinterpret_cast<void**>(cinfo + 0x158) = cc;
    *reinterpret_cast<void**>(cc + 0) = reinterpret_cast<void*>(0x004b01e0);      // start_pass
    int in_cs = field_i(cinfo, 0x24);
    int ncomp_in = field_i(cinfo, 0x20);
    switch (static_cast<unsigned>(in_cs - 1)) {
    case 0:  if (ncomp_in != 1) errexit(cinfop, 7); break;                        // JCS_GRAYSCALE
    case 1:
    case 2:  if (ncomp_in != 3) errexit(cinfop, 7); break;                        // JCS_RGB / JCS_YCbCr
    case 3:
    case 4:  if (ncomp_in != 4) errexit(cinfop, 7); break;                        // JCS_CMYK / JCS_YCCK
    default: if (ncomp_in < 1) errexit(cinfop, 7); break;
    }
    int jpeg_cs = field_i(cinfo, 0x38);
    switch (static_cast<unsigned>(jpeg_cs - 1)) {
    case 0:                                                                       // JCS_GRAYSCALE
        if (field_i(cinfo, 0x34) != 1) errexit(cinfop, 8);
        if (field_i(cinfo, 0x24) == 1) { *reinterpret_cast<void**>(cc + 4) = reinterpret_cast<void*>(0x004b50d0); return; }
        if (field_i(cinfo, 0x24) == 2) {
            *reinterpret_cast<void**>(cc + 0) = reinterpret_cast<void*>(0x004b4ca0);
            *reinterpret_cast<void**>(cc + 4) = reinterpret_cast<void*>(0x004b4eb0);
            return;
        }
        if (field_i(cinfo, 0x24) == 3) { *reinterpret_cast<void**>(cc + 4) = reinterpret_cast<void*>(0x004b50d0); return; }
        errexit(cinfop, 0x19);
        return;                                                                   // 0x004b4bf9
    case 1:                                                                       // JCS_RGB
        if (field_i(cinfo, 0x34) != 3) errexit(cinfop, 8);
        if (field_i(cinfo, 0x24) != 2) { errexit(cinfop, 0x19); return; }
        break;
    case 2:                                                                       // JCS_YCbCr
        if (field_i(cinfo, 0x34) != 3) errexit(cinfop, 8);
        if (field_i(cinfo, 0x24) == 2) {
            *reinterpret_cast<void**>(cc + 0) = reinterpret_cast<void*>(0x004b4ca0);
            *reinterpret_cast<void**>(cc + 4) = reinterpret_cast<void*>(0x004b4d80);
            return;
        }
        if (field_i(cinfo, 0x24) != 3) { errexit(cinfop, 0x19); return; }
        break;
    case 3:                                                                       // JCS_CMYK
        if (field_i(cinfo, 0x34) != 4) errexit(cinfop, 8);
        if (field_i(cinfo, 0x24) != 4) { errexit(cinfop, 0x19); return; }
        break;
    case 4:                                                                       // JCS_YCCK
        if (field_i(cinfo, 0x34) != 4) errexit(cinfop, 8);
        if (field_i(cinfo, 0x24) == 4) {
            *reinterpret_cast<void**>(cc + 0) = reinterpret_cast<void*>(0x004b4ca0);
            *reinterpret_cast<void**>(cc + 4) = reinterpret_cast<void*>(0x004b4f60);
            return;
        }
        if (field_i(cinfo, 0x24) != 5) { errexit(cinfop, 0x19); return; }
        break;
    default:
        if (jpeg_cs != field_i(cinfo, 0x24) || field_i(cinfo, 0x34) != field_i(cinfo, 0x20))
            errexit(cinfop, 0x19);
        break;
    }
    *reinterpret_cast<void**>(cc + 4) = reinterpret_cast<void*>(0x004b5140);      // 0x004b4c65
}

// 0x004b52a0  initial_setup(cinfo): check the image parameters and compute the per-component block counts. Four
// guards come first, all raising a message through err->error_exit and continuing because the fixture's handler
// returns: a zero image_height (cinfo+0x1c), image_width (cinfo+0x18) under an UNSIGNED compare, or a
// non-positive num_components (cinfo+0x34) or input_components (cinfo+0x20) under a SIGNED one raise ERREXIT
// 0x1f (0x004b52ad / 0x004b52b4 / 0x004b52bb / 0x004b52c2 / 0x004b52c7); a dimension above 0xffdc raises
// ERREXIT 0x28 with 0xffdc in msg_parm[0] (0x004b52dd / 0x004b52df / 0x004b52e4 / 0x004b52e9 / 0x004b52f2); a
// data_precision (cinfo+0x30) other than 8 raises ERREXIT 0xd with the value in msg_parm[0] (0x004b5300 /
// 0x004b5305 / 0x004b5311); more than 10 components raises ERREXIT 0x18 with the count and 10
// (0x004b5323 / 0x004b5325 / 0x004b532a / 0x004b5336 / 0x004b533b).
// max_h_samp_factor (cinfo+0xe0) and max_v_samp_factor (cinfo+0xe4) start at 1 (0x004b5354 / 0x004b535a) and
// the first loop (0x004b5360 / 0x004b53bf) raises ERREXIT 0x10 for any sampling factor outside 1..4
// (0x004b536a / 0x004b536f / 0x004b5375 / 0x004b537a / 0x004b537f) and then takes the running maximum of each
// (0x004b5396 / 0x004b5398 / 0x004b539c and 0x004b53aa / 0x004b53ac / 0x004b53b0). The second loop
// (0x004b53cb / 0x004b544a) fills each component: component_index = ci (0x004b53d3), DCT_scaled_size = 8
// (0x004b53d6) and four jdiv_round_up 0x004b04c0 results - width_in_blocks = (h * image_width) rounded up over
// max_h * 8 (0x004b53dc..0x004b53f0), height_in_blocks = (v * image_height) over max_v * 8
// (0x004b53f6..0x004b540d), downsampled_width = (h * image_width) over max_h (0x004b5410..0x004b5424) and
// downsampled_height = (v * image_height) over max_v (0x004b5427..0x004b543b) - plus component_needed = 1
// (0x004b543e). num_components is re-read at the bottom of both loops (0x004b53b6, 0x004b5441). Finally
// total_iMCU_rows (cinfo+0xe8) is image_height rounded up over max_v * 8 (0x004b544c..0x004b5462).
typedef void(__cdecl* InitialSetup_t)(void* cinfo);
InitialSetup_t InitialSetup_orig;
void __cdecl InitialSetup_re(void* cinfop) {
    char* cinfo = reinterpret_cast<char*>(cinfop);
    if (!(field_u(cinfo, 0x1c) > 0 && field_u(cinfo, 0x18) > 0 &&
          field_i(cinfo, 0x34) > 0 && field_i(cinfo, 0x20) > 0)) {
        errexit(cinfop, 0x1f);                                                    // JERR_EMPTY_IMAGE
    }
    if (field_i(cinfo, 0x1c) > 0xffdc || field_i(cinfo, 0x18) > 0xffdc) {
        field_i(field_ptr(cinfop, 0), 0x14) = 0x28;                               // JERR_IMAGE_TOO_BIG
        field_i(field_ptr(cinfop, 0), 0x18) = 0xffdc;
        reinterpret_cast<ErrExit_t>(field_ptr(field_ptr(cinfop, 0), 0))(cinfop);
    }
    if (field_i(cinfo, 0x30) != 8) {
        field_i(field_ptr(cinfop, 0), 0x14) = 0xd;                                // JERR_BAD_PRECISION
        field_i(field_ptr(cinfop, 0), 0x18) = field_i(cinfo, 0x30);
        reinterpret_cast<ErrExit_t>(field_ptr(field_ptr(cinfop, 0), 0))(cinfop);
    }
    if (field_i(cinfo, 0x34) > 0xa) {
        field_i(field_ptr(cinfop, 0), 0x14) = 0x18;                               // JERR_COMPONENT_COUNT
        field_i(field_ptr(cinfop, 0), 0x18) = field_i(cinfo, 0x34);
        field_i(field_ptr(cinfop, 0), 0x1c) = 0xa;
        reinterpret_cast<ErrExit_t>(field_ptr(field_ptr(cinfop, 0), 0))(cinfop);
    }
    int n = field_i(cinfo, 0x34);
    char* comp = reinterpret_cast<char*>(field_ptr(cinfop, 0x3c));
    field_i(cinfo, 0xe0) = 1;
    field_i(cinfo, 0xe4) = 1;
    for (int ci = 0; ci < n; ci++) {
        char* cp = comp + ci * 0x54;
        int h = field_i(cp, 8);
        if (h <= 0 || h > 4 || field_i(cp, 0xc) <= 0 || field_i(cp, 0xc) > 4) {
            errexit(cinfop, 0x10);                                                // JERR_BAD_SAMPLING
        }
        int maxh = field_i(cinfo, 0xe0);
        int ch = field_i(cp, 8);
        field_i(cinfo, 0xe0) = (maxh > ch) ? maxh : ch;
        int maxv = field_i(cinfo, 0xe4);
        int cv = field_i(cp, 0xc);
        field_i(cinfo, 0xe4) = (maxv > cv) ? maxv : cv;
        n = field_i(cinfo, 0x34);                                                 // 0x004b53b6
    }
    n = field_i(cinfo, 0x34);
    comp = reinterpret_cast<char*>(field_ptr(cinfop, 0x3c));
    for (int ci = 0; ci < n; ci++) {
        char* cp = comp + ci * 0x54;
        field_i(cp, 0x04) = ci;                                                   // component_index
        field_i(cp, 0x24) = 8;                                                    // DCT_scaled_size
        field_i(cp, 0x1c) = kJDivRoundUp(field_i(cp, 8) * field_i(cinfo, 0x18), field_i(cinfo, 0xe0) << 3);
        field_i(cp, 0x20) = kJDivRoundUp(field_i(cp, 0xc) * field_i(cinfo, 0x1c), field_i(cinfo, 0xe4) << 3);
        field_i(cp, 0x28) = kJDivRoundUp(field_i(cp, 8) * field_i(cinfo, 0x18), field_i(cinfo, 0xe0));
        field_i(cp, 0x2c) = kJDivRoundUp(field_i(cp, 0xc) * field_i(cinfo, 0x1c), field_i(cinfo, 0xe4));
        field_i(cp, 0x30) = 1;                                                    // component_needed
        n = field_i(cinfo, 0x34);                                                 // 0x004b5441
    }
    field_i(cinfo, 0xe8) = kJDivRoundUp(field_i(cinfo, 0x1c), field_i(cinfo, 0xe4) << 3);
}

// 0x004b5ad0  per_scan_setup(cinfo): compute the MCU geometry of the scan that is about to be emitted. A
// single-component scan (comps_in_scan, cinfo+0xec, equal to 1 at 0x004b5add / 0x004b5ae5) takes its sizes
// straight from cur_comp_info[0] (cinfo+0xf0, 0x004b5ae7): MCUs_per_row (cinfo+0x100) = width_in_blocks
// (0x004b5aed / 0x004b5af0) and MCU_rows_in_scan (cinfo+0x104) = height_in_blocks (0x004b5af6 / 0x004b5af9);
// the component's MCU_width, MCU_height, MCU_blocks and last_col_width are 1 and MCU_sample_width is 8
// (0x004b5b09..0x004b5b19); last_row_height is height_in_blocks modulo v_samp_factor, or v_samp_factor itself
// when the UNSIGNED division at 0x004b5b07 leaves no remainder (0x004b5b1c / 0x004b5b1e / 0x004b5b20 /
// 0x004b5b22); blocks_in_MCU (cinfo+0x108) is 1 and MCU_membership[0] (cinfo+0x10c) is 0
// (0x004b5b25 / 0x004b5b2b).
// An interleaved scan (0x004b5b3a) first raises ERREXIT 0x18 with comps_in_scan and 4 in msg_parm when the
// count is outside 1..4 (0x004b5b3c / 0x004b5b3e / 0x004b5b43 / 0x004b5b48 / 0x004b5b57 / 0x004b5b5c), then
// takes MCUs_per_row = jdiv_round_up(image_width, max_h_samp_factor * 8) (0x004b5b6a..0x004b5b8b) and
// MCU_rows_in_scan = jdiv_round_up(image_height, max_v_samp_factor * 8) (0x004b5b7d..0x004b5b96), clears
// blocks_in_MCU (0x004b5ba7) and walks cur_comp_info (0x004b5bb1 / 0x004b5c6b). Each component gets
// MCU_width = h_samp_factor (0x004b5bd1), MCU_height = v_samp_factor (0x004b5be7), MCU_blocks = h * v
// (0x004b5be4 / 0x004b5bea), MCU_sample_width = h * 8 (0x004b5bd4 / 0x004b5bdd), last_col_width =
// width_in_blocks modulo h or h itself (0x004b5be2 / 0x004b5bed / 0x004b5bf1 / 0x004b5bf6) and
// last_row_height = height_in_blocks modulo v or v itself (0x004b5bfb / 0x004b5bfd / 0x004b5c01 / 0x004b5c03),
// both from UNSIGNED divisions. blocks_in_MCU plus this component's MCU_blocks above 10 raises ERREXIT 0xb
// (0x004b5c06 / 0x004b5c0c / 0x004b5c11 / 0x004b5c16); then MCU_blocks entries of MCU_membership are set to the
// component's position in the scan (0x004b5c24 / 0x004b5c29 / 0x004b5c38 / 0x004b5c4d) and blocks_in_MCU grows
// by one per entry (0x004b5c45). comps_in_scan is re-read at the bottom of every pass (0x004b5c57).
// Both paths end with the restart interval: a positive restart_in_rows (cinfo+0xc4, 0x004b5c72 / 0x004b5c7a)
// sets restart_interval (cinfo+0xc0) to restart_in_rows * MCUs_per_row clamped to 0xffff (0x004b5c7c /
// 0x004b5c83 / 0x004b5c88 / 0x004b5c8f).
typedef void(__cdecl* PerScanSetup_t)(void* cinfo);
PerScanSetup_t PerScanSetup_orig;
void __cdecl PerScanSetup_re(void* cinfop) {
    char* cinfo = reinterpret_cast<char*>(cinfop);
    if (field_i(cinfo, 0xec) == 1) {
        char* cp = reinterpret_cast<char*>(field_ptr(cinfop, 0xf0));
        field_i(cinfo, 0x100) = field_i(cp, 0x1c);
        field_i(cinfo, 0x104) = field_i(cp, 0x20);
        unsigned v = field_u(cp, 0x0c);
        unsigned rem = field_u(cp, 0x20) % v;
        field_i(cp, 0x34) = 1;
        field_i(cp, 0x38) = 1;
        field_i(cp, 0x3c) = 1;
        field_i(cp, 0x40) = 8;
        field_i(cp, 0x44) = 1;
        field_u(cp, 0x48) = rem ? rem : v;
        field_i(cinfo, 0x108) = 1;
        field_i(cinfo, 0x10c) = 0;
    } else {
        int cis = field_i(cinfo, 0xec);
        if (cis <= 0 || cis > 4) {
            field_i(field_ptr(cinfop, 0), 0x14) = 0x18;                           // JERR_COMPONENT_COUNT
            field_i(field_ptr(cinfop, 0), 0x18) = field_i(cinfo, 0xec);
            field_i(field_ptr(cinfop, 0), 0x1c) = 4;
            reinterpret_cast<ErrExit_t>(field_ptr(field_ptr(cinfop, 0), 0))(cinfop);
        }
        field_i(cinfo, 0x100) = kJDivRoundUp(field_i(cinfo, 0x18), field_i(cinfo, 0xe0) << 3);
        field_i(cinfo, 0x104) = kJDivRoundUp(field_i(cinfo, 0x1c), field_i(cinfo, 0xe4) << 3);
        field_i(cinfo, 0x108) = 0;
        int n = field_i(cinfo, 0xec);
        for (int ci = 0; ci < n; ci++) {
            char* cp = reinterpret_cast<char*>(field_ptr(cinfop, 0xf0 + ci * 4));
            unsigned h = field_u(cp, 8);
            unsigned wib = field_u(cp, 0x1c);
            unsigned v = field_u(cp, 0x0c);
            unsigned blocks = v * h;
            field_u(cp, 0x34) = h;
            field_u(cp, 0x40) = h * 8;
            unsigned rem1 = wib % h;
            field_u(cp, 0x38) = v;
            field_u(cp, 0x3c) = blocks;
            field_u(cp, 0x44) = rem1 ? rem1 : h;
            unsigned rem2 = field_u(cp, 0x20) % v;
            field_u(cp, 0x48) = rem2 ? rem2 : v;
            if (field_i(cinfo, 0x108) + static_cast<int>(blocks) > 0xa) {
                errexit(cinfop, 0xb);                                             // JERR_BAD_MCU_SIZE
            }
            for (int k = static_cast<int>(blocks); k > 0; k--) {
                int idx = field_i(cinfo, 0x108);
                field_i(cinfo, 0x10c + idx * 4) = ci;
                field_i(cinfo, 0x108) = idx + 1;
            }
            n = field_i(cinfo, 0xec);                                             // 0x004b5c57
        }
    }
    int rir = field_i(cinfo, 0xc4);
    if (rir > 0) {
        int t = rir * field_i(cinfo, 0x100);
        if (t >= 0xffff) t = 0xffff;
        field_i(cinfo, 0xc0) = t;
    }
}

// 0x00487050  BinkPlayer::setTarget(p): store the argument at this+0xc. The argument is read from [esp+4]
// (0x00487050) and written to this+0xc (0x00487054); the function returns nothing and pops its single stack
// argument (ret 4 at 0x00487057). No other field is read or written and there is no conditional branch.
typedef void(__fastcall* BinkSetTarget_t)(void* ecx, void* edx, void* value);
BinkSetTarget_t BinkSetTarget_orig;
void __fastcall BinkSetTarget_re(void* thisp, void*, void* value) {
    *reinterpret_cast<void**>(reinterpret_cast<char*>(thisp) + 0xc) = value;
}

}  // namespace

// 0x004b0560  jinit_c_main_controller
SG_HOOK("golf_clean.exe", 0x004b0560, jinit_c_main_controller, JinitCMain_re, JinitCMain_orig);
// 0x004b3a80  jinit_c_prep_controller
SG_HOOK("golf_clean.exe", 0x004b3a80, jinit_c_prep_controller, JinitCPrep_re, JinitCPrep_orig);
// 0x004b3f10  create_context_buffer
SG_HOOK("golf_clean.exe", 0x004b3f10, create_context_buffer, CreateCtx_re, CreateCtx_orig);
// 0x004ae990  jpeg_set_colorspace
SG_HOOK("golf_clean.exe", 0x004ae990, jpeg_set_colorspace, SetColorspace_re, SetColorspace_orig);
// 0x004b4040  jinit_downsampler
SG_HOOK("golf_clean.exe", 0x004b4040, jinit_downsampler, JinitDownsampler_re, JinitDownsampler_orig);
// 0x004b4a60  jinit_color_converter
SG_HOOK("golf_clean.exe", 0x004b4a60, jinit_color_converter, JinitCConvert_re, JinitCConvert_orig);
// 0x004b52a0  initial_setup
SG_HOOK("golf_clean.exe", 0x004b52a0, initial_setup, InitialSetup_re, InitialSetup_orig);
// 0x004b5ad0  per_scan_setup
SG_HOOK("golf_clean.exe", 0x004b5ad0, per_scan_setup, PerScanSetup_re, PerScanSetup_orig);
// 0x00487050  BinkPlayer::setTarget
SG_HOOK("golf_clean.exe", 0x00487050, BinkPlayer_setTarget, BinkSetTarget_re, BinkSetTarget_orig);

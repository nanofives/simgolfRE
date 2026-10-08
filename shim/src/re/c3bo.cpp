// C3 batch c3bo (round 16, 2026-10-08) of golf_clean.exe. Four IJG libjpeg 6a compress-side routines the exe
// links from original\JPEG.lib: the master-control initialiser, the scan-script validator, the per-pass
// setup, and the progressive Huffman EOB-run emitter.
//
// Every body is hand-written from the disassembly (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr>
// --list) and cites the address of each instruction it reproduces. Callees run through their original
// addresses:
//   jinit_c_master_control -> initial_setup 0x004b52a0 (C3, c3bj.cpp) and validate_script 0x004b5470
//                             (in THIS batch: re-run with both hooks off)
//   prepare_for_pass       -> select_scan_parameters 0x004b59d0 (C2) and per_scan_setup 0x004b5ad0 (C3, c3bj)
//   emit_eobrun            -> dump_buffer 0x004b2420 (C3, c3be) and emit_buffered_bits 0x004b27d0 (C3, c3aa)
// The memory manager (cinfo+4), the error manager (cinfo+0), the destination manager (cinfo+0x14) and the
// eight module objects prepare_for_pass dispatches through are reached by both arms through their own slots,
// so the fixture's callbacks are what runs.
//
//   jinit_c_master_control 0x004b51e0   validate_script 0x004b5470
//   prepare_for_pass       0x004b5810   emit_eobrun     0x004b25d0
#include "hooks.h"

namespace {

// ---- IJG libjpeg 6a structures reached through pointers -------------------------------------------------------
// jpeg_compress_struct: err +0 (jpeg_error_mgr: error_exit +0, emit_message +4, msg_code +0x14,
// msg_parm.i[0] +0x18, msg_parm.i[1] +0x1c), mem +4, progress +8, dest +0x14, image_width +0x18,
// image_height +0x1c, input_components +0x20, data_precision +0x30, num_components +0x34, comp_info +0x3c,
// scan_info +0xa4, num_scans +0xa0, raw_data_in +0xa8, arith_code +0xac, optimize_coding +0xb0,
// progressive_mode +0xdc, max_h_samp_factor +0xe0, max_v_samp_factor +0xe4, comps_in_scan +0xec,
// cur_comp_info[] +0xf0, MCUs_per_row +0x100, MCU_rows_in_scan +0x104, blocks_in_MCU +0x108,
// MCU_membership[] +0x10c, Ss +0x134, Se +0x138, Ah +0x13c, Al +0x140, master +0x144, main +0x148,
// prep +0x14c, coef +0x150, marker +0x154, cconvert +0x158, downsample +0x15c, fdct +0x160, entropy +0x164.
// jpeg_progress_mgr: completed_passes +0xc, total_passes +0x10.
// my_comp_master (jcmaster.c): prepare_for_pass +0, pass_startup +4, finish_pass +8, call_pass_startup +0xc,
// is_last_pass +0x10, pass_type +0x14, pass_number +0x18, total_passes +0x1c, scan_number +0x20.
// jpeg_scan_info: comps_in_scan +0, component_index[4] +4, Ss +0x14, Se +0x18, Ah +0x1c, Al +0x20, size 0x24.
// phuff_entropy_encoder (jcphuff.c): gather_statistics +0xc, next_output_byte +0x10, free_in_buffer +0x14,
// put_buffer +0x18, put_bits +0x1c, cinfo +0x20, ac_tbl_no +0x34, EOBRUN +0x38, BE +0x3c, bit_buffer +0x40,
// derived_tbls[4] +0x4c, count_ptrs[4] +0x5c. c_derived_tbl: ehufco[256] +0 (int), ehufsi[256] +0x400 (char).
typedef void(__cdecl* ErrExit_t)(void* cinfo);
typedef void(__cdecl* Method1_t)(void* cinfo);
typedef void(__cdecl* Method2_t)(void* cinfo, int mode);

static inline void* field_ptr(void* base, unsigned off) {
    return *reinterpret_cast<void**>(reinterpret_cast<char*>(base) + off);
}
static inline int& field_i(void* base, unsigned off) {
    return *reinterpret_cast<int*>(reinterpret_cast<char*>(base) + off);
}
static inline unsigned& field_u(void* base, unsigned off) {
    return *reinterpret_cast<unsigned*>(reinterpret_cast<char*>(base) + off);
}
// ERREXIT with no parameter: store the code in cinfo->err->msg_code and call cinfo->err->error_exit(cinfo).
// Each half re-reads cinfo+0, exactly as the originals do.
static inline void errexit0(void* cinfo, int code) {
    field_i(field_ptr(cinfo, 0), 0x14) = code;
    reinterpret_cast<ErrExit_t>(field_ptr(field_ptr(cinfo, 0), 0))(cinfo);
}
// ERREXIT1: the code plus msg_parm.i[0].
static inline void errexit1(void* cinfo, int code, int p0) {
    field_i(field_ptr(cinfo, 0), 0x14) = code;
    field_i(field_ptr(cinfo, 0), 0x18) = p0;
    reinterpret_cast<ErrExit_t>(field_ptr(field_ptr(cinfo, 0), 0))(cinfo);
}
// ERREXIT2: the code plus msg_parm.i[0] and msg_parm.i[1].
static inline void errexit2(void* cinfo, int code, int p0, int p1) {
    field_i(field_ptr(cinfo, 0), 0x14) = code;
    field_i(field_ptr(cinfo, 0), 0x18) = p0;
    field_i(field_ptr(cinfo, 0), 0x1c) = p1;
    reinterpret_cast<ErrExit_t>(field_ptr(field_ptr(cinfo, 0), 0))(cinfo);
}

// Callees reached through their original addresses.
typedef void(__cdecl* VoidCinfo_t)(void* cinfo);
const VoidCinfo_t kInitialSetup = reinterpret_cast<VoidCinfo_t>(0x004b52a0);           // C3, c3bj.cpp
const VoidCinfo_t kValidateScript = reinterpret_cast<VoidCinfo_t>(0x004b5470);         // this batch
const VoidCinfo_t kSelectScanParameters = reinterpret_cast<VoidCinfo_t>(0x004b59d0);   // C2
const VoidCinfo_t kPerScanSetup = reinterpret_cast<VoidCinfo_t>(0x004b5ad0);           // C3, c3bj.cpp
typedef void(__cdecl* DumpBuffer_t)(void* entropy);
const DumpBuffer_t kDumpBuffer = reinterpret_cast<DumpBuffer_t>(0x004b2420);           // C3, c3be.cpp
typedef void(__cdecl* EmitBuffered_t)(void* entropy, void* bufstart, unsigned nbits);
const EmitBuffered_t kEmitBufferedBits = reinterpret_cast<EmitBuffered_t>(0x004b27d0); // C3, c3aa.cpp

// 0x004b5470  validate_script(cinfo): check the caller-supplied scan script and decide whether it is a
// progressive one. The frame is 0xa48 bytes (0x004b5470) and holds two arrays the function never initialises
// past the component count it is told about: component_sent[10] at esp+0x30 (one INT per component, the
// `rep stosd` at 0x004b54ec writing num_components dwords) and last_bitpos[10][64] at esp+0x58 (the same
// `rep stosd` writing (num_components & 0xffffff) << 6 dwords of -1, 0x004b54dc..0x004b54e9).
// A num_scans (cinfo+0xa0) that is not positive raises ERREXIT 0x11 with 0 in msg_parm[0] (0x004b5483 /
// 0x004b5489 / 0x004b548e / 0x004b5497 / 0x004b549c). The first scan then decides the mode: Ss (scan+0x14)
// zero AND Se (scan+0x18) equal to 0x3f (0x004b54ab / 0x004b54ae / 0x004b54b0 / 0x004b54b4) clears
// progressive_mode (cinfo+0xdc, 0x004b54b9) and initialises component_sent, anything else sets it
// (0x004b54ce) and initialises last_bitpos to -1.
// The scan loop (0x004b54ee / 0x004b5788, scanno starting at 1 and num_scans re-read at 0x004b5774) checks
// each scan: comps_in_scan (scan+0) outside 1..4 raises ERREXIT 0x18 with the count and 4 (0x004b550d /
// 0x004b5512 / 0x004b5517 / 0x004b5520 / 0x004b5525); every component_index (scan+4+4*ci) outside
// 0..num_components-1 (0x004b553e / 0x004b5543) and every index not greater than its predecessor
// (0x004b5561 / 0x004b5565) raise ERREXIT 0x11 with scanno.
// In progressive mode (0x004b559f / 0x004b55b6) Ss, Se, Ah and Al are range-checked as 0 <= Ss < 0x40,
// Ss <= Se < 0x40, 0 <= Ah <= 0xd and 0 <= Al <= 0xd (0x004b55be..0x004b55de, ERREXIT 0xf); a DC scan
// (Ss == 0) must have Se == 0 (0x004b5600 / 0x004b5604) and an AC scan must have exactly one component
// (0x004b561f / 0x004b5622). Each component then walks its own 64-entry row of last_bitpos, addressed by
// component_index << 8 bytes (0x004b565b / 0x004b565d / 0x004b5662): an AC scan whose DC entry is still
// negative (0x004b5660 / 0x004b5666 / 0x004b566b) and, for every coefficient from Ss to Se (0x004b5695 /
// 0x004b569b / 0x004b56a0 / 0x004b56e2), a first visit with a non-zero Ah (0x004b56a5 / 0x004b56ad) or a
// repeat visit whose Ah is not the stored value or whose Al is not Ah-1 (0x004b56b5 / 0x004b56b7 /
// 0x004b56be / 0x004b56c0) raise ERREXIT 0xf; the entry is then set to Al (0x004b56d8 / 0x004b56dc).
// In sequential mode (0x004b5704) Ss, Se, Ah and Al must be 0, 0x3f, 0 and 0 (0x004b5704..0x004b5713,
// ERREXIT 0xf), and each component's component_sent entry must still be clear before it is set
// (0x004b573e / 0x004b5744 / 0x004b5764, ERREXIT 0x11).
// The closing check (0x004b578e / 0x004b5796) raises ERREXIT 0x2c once per component whose DC entry is still
// negative (0x004b57aa / 0x004b57ad, row stride 0x100 at 0x004b57c0) or, sequentially, whose component_sent
// entry is still clear (0x004b57e7 / 0x004b57ea); num_components is re-read at the bottom of both closing
// loops (0x004b57bc, 0x004b57f9).
struct ScanInfo {
    int comps_in_scan;
    int component_index[4];
    int Ss, Se, Ah, Al;
};
VoidCinfo_t ValidateScript_orig;
void __cdecl ValidateScript_re(void* cinfop) {
    char* cinfo = reinterpret_cast<char*>(cinfop);
    int component_sent[10];        // esp+0x30, one int per component
    int last_bitpos[10][64];       // esp+0x58, 10 rows of 64 ints
    int* const lb0 = &last_bitpos[0][0];

    if (field_i(cinfo, 0xa0) <= 0) errexit1(cinfop, 0x11, 0);                      // 0x004b5489
    const ScanInfo* scanptr = reinterpret_cast<const ScanInfo*>(field_ptr(cinfop, 0xa4));
    if (scanptr->Ss != 0 || scanptr->Se != 0x3f) {
        field_i(cinfo, 0xdc) = 1;                                                  // 0x004b54ce
        int nc = field_i(cinfo, 0x34);
        if (nc > 0) {
            int n = (nc & 0xffffff) << 6;                                          // 0x004b54dc / 0x004b54e6
            for (int i = 0; i < n; i++) lb0[i] = -1;
        }
    } else {
        field_i(cinfo, 0xdc) = 0;                                                  // 0x004b54b9
        int nc = field_i(cinfo, 0x34);
        if (nc > 0) {
            for (int i = 0; i < nc; i++) component_sent[i] = 0;
        }
    }

    int scanno = 1;
    if (field_i(cinfo, 0xa0) >= 1) {                                               // 0x004b54ff
        for (;;) {
            // The index walk reads [scan+4+4*ci] and compares with [scan+4*ci] (0x004b5539 / 0x004b5563 /
            // 0x004b5586), so a comps_in_scan of 5 reads the Ss field as a fifth index; the dwords are
            // addressed one at a time rather than through the 4-entry array.
            const int* sp = reinterpret_cast<const int*>(scanptr);
            int ncomps = sp[0];
            if (ncomps <= 0 || ncomps > 4) errexit2(cinfop, 0x18, ncomps, 4);      // 0x004b550d / 0x004b5512
            for (int ci = 0; ci < ncomps; ci++) {                                  // 0x004b5537 / 0x004b558b
                int thisi = sp[1 + ci];
                if (thisi < 0 || thisi >= field_i(cinfo, 0x34))
                    errexit1(cinfop, 0x11, scanno);                                // 0x004b553e / 0x004b5543
                if (ci > 0 && thisi <= sp[ci])
                    errexit1(cinfop, 0x11, scanno);                                // 0x004b5561 / 0x004b5565
            }
            int Ss = scanptr->Ss;                                                  // 0x004b5593
            int Se = scanptr->Se;
            int Ah = scanptr->Ah;
            int Al = scanptr->Al;
            if (field_i(cinfo, 0xdc) != 0) {                                       // 0x004b55b6
                if (Ss < 0 || Ss >= 0x40 || Se < Ss || Se >= 0x40 ||
                    Ah < 0 || Ah > 0xd || Al < 0 || Al > 0xd)
                    errexit1(cinfop, 0xf, scanno);                                 // 0x004b55be..0x004b55de
                if (Ss == 0) {
                    if (Se != 0) errexit1(cinfop, 0xf, scanno);                    // 0x004b5604
                } else {
                    if (ncomps != 1) errexit1(cinfop, 0xf, scanno);                // 0x004b5622
                }
                for (int ci = 0; ci < ncomps; ci++) {                              // 0x004b5640 / 0x004b56fc
                    int* lb = lb0 + (sp[1 + ci] << 6);                             // 0x004b565d / 0x004b5662
                    if (Ss != 0 && lb[0] < 0) errexit1(cinfop, 0xf, scanno);       // 0x004b5666 / 0x004b566b
                    if (Ss <= Se) {                                                // 0x004b5697
                        for (int coefi = Ss; coefi <= Se; coefi++) {
                            if (lb[coefi] < 0) {
                                if (Ah != 0) errexit1(cinfop, 0xf, scanno);        // 0x004b56ad
                            } else if (Ah != lb[coefi] || Al != Ah - 1) {
                                errexit1(cinfop, 0xf, scanno);                     // 0x004b56b7 / 0x004b56c0
                            }
                            lb[coefi] = Al;                                        // 0x004b56dc
                        }
                    }
                }
            } else {
                if (Ss != 0 || Se != 0x3f || Ah != 0 || Al != 0)
                    errexit1(cinfop, 0xf, scanno);                                 // 0x004b5704..0x004b5713
                for (int ci = 0; ci < ncomps; ci++) {                              // 0x004b5731 / 0x004b576a
                    int thisi = sp[1 + ci];
                    if (component_sent[thisi] != 0) errexit1(cinfop, 0x11, scanno);// 0x004b5744
                    component_sent[thisi] = 1;                                     // 0x004b5764
                }
            }
            scanno++;
            scanptr++;                                                             // 0x004b577a, stride 0x24
            if (scanno > field_i(cinfo, 0xa0)) break;                              // 0x004b5788
        }
    }

    if (field_i(cinfo, 0xdc) != 0) {                                               // 0x004b5796
        for (int ci = 0; ci < field_i(cinfo, 0x34); ci++) {                        // 0x004b579f / 0x004b57c8
            if (last_bitpos[ci][0] < 0) errexit0(cinfop, 0x2c);                    // 0x004b57ad
        }
    } else {
        for (int ci = 0; ci < field_i(cinfo, 0x34); ci++) {                        // 0x004b57dc / 0x004b5802
            if (component_sent[ci] == 0) errexit0(cinfop, 0x2c);                   // 0x004b57ea
        }
    }
}

// 0x004b51e0  jinit_c_master_control(cinfo, transcode_only): create the master-control object and settle the
// pass plan. A 0x24-byte object comes from mem->alloc_small(cinfo, 1, 0x24) (0x004b51e7..0x004b51ef), is
// stored at cinfo+0x144 (0x004b51f5) and gets prepare_for_pass = 0x004b5810, pass_startup = 0x004b5ca0 and
// finish_pass = 0x004b5cd0 in its first three slots plus is_last_pass = 0 at +0x10 (0x004b51fc..0x004b5210).
// initial_setup 0x004b52a0 runs next (0x004b5213). A non-null scan_info (cinfo+0xa4, 0x004b5218 / 0x004b5221
// / 0x004b5223) sends the script to validate_script 0x004b5470 (0x004b5226), which is what sets
// progressive_mode; a null one clears progressive_mode (cinfo+0xdc) and sets num_scans (cinfo+0xa0) to 1
// (0x004b5230 / 0x004b5236). A non-zero progressive_mode then forces optimize_coding (cinfo+0xb0) to 1
// (0x004b5240 / 0x004b5246 / 0x004b5248). pass_type (master+0x14) is 1 when transcode_only is set and
// optimize_coding is on, 2 when transcode_only is set and it is off, and 0 otherwise (0x004b5252 /
// 0x004b5256 / 0x004b525e / 0x004b5260 / 0x004b5269 / 0x004b5272). scan_number (master+0x20) and
// pass_number (master+0x18) are cleared (0x004b5275 / 0x004b5278) and total_passes (master+0x1c) is
// num_scans doubled when optimize_coding is on and num_scans otherwise (0x004b527b / 0x004b5281 /
// 0x004b5283 / 0x004b5289 / 0x004b5292).
typedef void*(__cdecl* AllocSmall_t)(void* cinfo, int pool, int size);
typedef void(__cdecl* JinitMaster_t)(void* cinfo, int transcode_only);
JinitMaster_t JinitMaster_orig;
void __cdecl JinitMaster_re(void* cinfop, int transcode_only) {
    char* cinfo = reinterpret_cast<char*>(cinfop);
    AllocSmall_t alloc_small = reinterpret_cast<AllocSmall_t>(field_ptr(field_ptr(cinfop, 4), 0));
    char* master = reinterpret_cast<char*>(alloc_small(cinfop, 1, 0x24));
    *reinterpret_cast<void**>(cinfo + 0x144) = master;
    *reinterpret_cast<void**>(master + 0) = reinterpret_cast<void*>(0x004b5810);   // prepare_for_pass
    *reinterpret_cast<void**>(master + 4) = reinterpret_cast<void*>(0x004b5ca0);   // pass_startup
    *reinterpret_cast<void**>(master + 8) = reinterpret_cast<void*>(0x004b5cd0);   // finish_pass
    field_i(master, 0x10) = 0;                                                     // is_last_pass
    kInitialSetup(cinfop);
    if (field_ptr(cinfop, 0xa4) != 0) {
        kValidateScript(cinfop);
    } else {
        field_i(cinfo, 0xdc) = 0;                                                  // progressive_mode
        field_i(cinfo, 0xa0) = 1;                                                  // num_scans
    }
    if (field_i(cinfo, 0xdc) != 0) field_i(cinfo, 0xb0) = 1;                       // optimize_coding
    if (transcode_only != 0) {
        field_i(master, 0x14) = (field_i(cinfo, 0xb0) != 0) ? 1 : 2;               // huff_opt / output
    } else {
        field_i(master, 0x14) = 0;                                                 // main_pass
    }
    field_i(master, 0x20) = 0;                                                     // scan_number
    field_i(master, 0x18) = 0;                                                     // pass_number
    if (field_i(cinfo, 0xb0) != 0) {
        field_i(master, 0x1c) = field_i(cinfo, 0xa0) * 2;                          // 0x004b5289 shl 1
    } else {
        field_i(master, 0x1c) = field_i(cinfo, 0xa0);
    }
}

// 0x004b5810  prepare_for_pass(cinfo): start the pass the master object is currently on. master comes from
// cinfo+0x144 (0x004b5816) and pass_type (master+0x14) selects one of three arms through the chain at
// 0x004b581f / 0x004b5822 / 0x004b5828 / 0x004b5829 / 0x004b582b / 0x004b582c; anything else raises
// ERREXIT 0x2f (0x004b5831 / 0x004b583a).
//   pass_type 0 (0x004b5907): select_scan_parameters 0x004b59d0 and per_scan_setup 0x004b5ad0 run
//     (0x004b5908 / 0x004b590e); with raw_data_in (cinfo+0xa8) clear (0x004b591c / 0x004b591e) the colour
//     converter (cinfo+0x158), the downsampler (cinfo+0x15c) and the pre-processor (cinfo+0x14c, with mode 0)
//     are started (0x004b5927 / 0x004b5930 / 0x004b593b); then the forward DCT (cinfo+0x160, 0x004b5947), the
//     entropy encoder (cinfo+0x164) with optimize_coding as its argument (0x004b5949 / 0x004b5957), the
//     coefficient controller (cinfo+0x150) with mode 3 when total_passes (master+0x1c) is above 1 and mode 0
//     otherwise (0x004b5959 / 0x004b5964 / 0x004b5967 / 0x004b596a / 0x004b596b / 0x004b5970) and the main
//     controller (cinfo+0x148) with mode 0 (0x004b597b). call_pass_startup (master+0xc) is then 0 when
//     optimize_coding is set and 1 when it is clear (0x004b5986 / 0x004b5988 / 0x004b598a / 0x004b5993).
//   pass_type 1 (0x004b5844): the same two setup calls run (0x004b5845 / 0x004b584b); if Ss (cinfo+0x134) is
//     non-zero, Ah (cinfo+0x13c) is zero or arith_code (cinfo+0xac) is non-zero (0x004b585b / 0x004b5869 /
//     0x004b5873) the entropy encoder is started with 1 and the coefficient controller with 2
//     (0x004b58eb / 0x004b58f6) and call_pass_startup is cleared (0x004b58fb); otherwise pass_type becomes 2
//     and pass_number (master+0x18) is incremented (0x004b5875..0x004b5880) and the arm below is entered.
//   pass_type 2 (0x004b5883): a clear optimize_coding repeats the two setup calls (0x004b5889 / 0x004b588b /
//     0x004b588e / 0x004b5894); the entropy encoder is started with 0 (0x004b58a5) and the coefficient
//     controller with 2 (0x004b58b0); a zero scan_number (master+0x20, 0x004b58b8 / 0x004b58ba) emits the
//     frame header through the marker writer's +8 slot (cinfo+0x154, 0x004b58c3) and the scan header always
//     goes out through its +0xc slot (0x004b58d0); call_pass_startup is cleared (0x004b58d6).
// Every arm ends at 0x004b599a: is_last_pass (master+0x10) becomes 1 exactly when pass_number equals
// total_passes - 1 (0x004b599a..0x004b59a8), and a non-null progress monitor (cinfo+8, 0x004b59ae /
// 0x004b59b0) receives pass_number at +0xc and total_passes at +0x10 (0x004b59b2 / 0x004b59bb).
VoidCinfo_t PrepareForPass_orig;
void __cdecl PrepareForPass_re(void* cinfop) {
    char* cinfo = reinterpret_cast<char*>(cinfop);
    char* master = reinterpret_cast<char*>(field_ptr(cinfop, 0x144));
    int pass_type = field_i(master, 0x14);
    bool do_output = false;
    if (pass_type == 0) {
        kSelectScanParameters(cinfop);
        kPerScanSetup(cinfop);
        if (field_i(cinfo, 0xa8) == 0) {                                           // raw_data_in
            reinterpret_cast<Method1_t>(field_ptr(field_ptr(cinfop, 0x158), 0))(cinfop);
            reinterpret_cast<Method1_t>(field_ptr(field_ptr(cinfop, 0x15c), 0))(cinfop);
            reinterpret_cast<Method2_t>(field_ptr(field_ptr(cinfop, 0x14c), 0))(cinfop, 0);
        }
        reinterpret_cast<Method1_t>(field_ptr(field_ptr(cinfop, 0x160), 0))(cinfop);
        reinterpret_cast<Method2_t>(field_ptr(field_ptr(cinfop, 0x164), 0))(cinfop, field_i(cinfo, 0xb0));
        reinterpret_cast<Method2_t>(field_ptr(field_ptr(cinfop, 0x150), 0))(
            cinfop, (field_i(master, 0x1c) > 1) ? 3 : 0);
        reinterpret_cast<Method2_t>(field_ptr(field_ptr(cinfop, 0x148), 0))(cinfop, 0);
        field_i(master, 0x0c) = (field_i(cinfo, 0xb0) != 0) ? 0 : 1;               // call_pass_startup
    } else if (pass_type == 1) {
        kSelectScanParameters(cinfop);
        kPerScanSetup(cinfop);
        if (field_i(cinfo, 0x134) != 0 || field_i(cinfo, 0x13c) == 0 || field_i(cinfo, 0xac) != 0) {
            reinterpret_cast<Method2_t>(field_ptr(field_ptr(cinfop, 0x164), 0))(cinfop, 1);
            reinterpret_cast<Method2_t>(field_ptr(field_ptr(cinfop, 0x150), 0))(cinfop, 2);
            field_i(master, 0x0c) = 0;
        } else {
            field_i(master, 0x14) = 2;                                             // 0x004b5878
            field_i(master, 0x18) = field_i(master, 0x18) + 1;                     // 0x004b5880
            do_output = true;
        }
    } else if (pass_type == 2) {
        do_output = true;
    } else {
        errexit0(cinfop, 0x2f);                                                    // JERR_NOT_COMPILED
    }
    if (do_output) {
        if (field_i(cinfo, 0xb0) == 0) {                                           // optimize_coding
            kSelectScanParameters(cinfop);
            kPerScanSetup(cinfop);
        }
        reinterpret_cast<Method2_t>(field_ptr(field_ptr(cinfop, 0x164), 0))(cinfop, 0);
        reinterpret_cast<Method2_t>(field_ptr(field_ptr(cinfop, 0x150), 0))(cinfop, 2);
        if (field_i(master, 0x20) == 0) {                                          // scan_number
            reinterpret_cast<Method1_t>(field_ptr(field_ptr(cinfop, 0x154), 8))(cinfop);
        }
        reinterpret_cast<Method1_t>(field_ptr(field_ptr(cinfop, 0x154), 0xc))(cinfop);
        field_i(master, 0x0c) = 0;
    }
    int pass_number = field_i(master, 0x18);
    field_i(master, 0x10) = (pass_number == field_i(master, 0x1c) - 1) ? 1 : 0;     // is_last_pass
    void* progress = field_ptr(cinfop, 8);
    if (progress != 0) {
        field_i(progress, 0x0c) = pass_number;
        field_i(progress, 0x10) = field_i(master, 0x1c);
    }
}

// emit_bits, inlined twice into 0x004b25d0 (first copy 0x004b2650..0x004b26e8, second 0x004b2704..0x004b279a;
// both have the same shape). The code is masked to `size` bits (0x004b2654..0x004b2667), the running bit
// count grows by `size` (0x004b265b), the masked code is shifted left by 24 minus the new count
// (0x004b265f / 0x004b2664 / 0x004b2669) and merged with put_buffer (0x004b266b / 0x004b266e). While the
// count is at least 8 (0x004b2670 / 0x004b2677) whole bytes leave: the byte count is taken once
// (0x004b2679 / 0x004b267b) and the count reduced by eight times it (0x004b267e..0x004b2682); each pass
// writes bits 16..23 of the buffer through next_output_byte (0x004b268a / 0x004b268e), advances it and
// decrements free_in_buffer (0x004b2696..0x004b269f), calls dump_buffer 0x004b2420 when the count reaches
// zero (0x004b26a2 / 0x004b26a5), writes a stuffed zero the same way when the byte was 0xFF (0x004b26b1 /
// 0x004b26b6 / 0x004b26bb..0x004b26cf) and shifts the buffer left by 8 (0x004b26d7). put_buffer and
// put_bits are written back at 0x004b26e1 and 0x004b26e8.
static void emit_bits_inline(char* e, unsigned code, int size) {
    unsigned put_buffer = code;
    int put_bits = field_i(e, 0x1c) + size;                                        // 0x004b265b
    put_buffer &= (1u << (size & 31)) - 1u;                                        // 0x004b2654 / 0x004b2667
    put_buffer <<= ((24 - put_bits) & 31);                                         // 0x004b2669, shl by cl
    put_buffer |= field_u(e, 0x18);                                                // 0x004b266e
    if (put_bits >= 8) {                                                           // 0x004b2677
        int nbytes = static_cast<int>(static_cast<unsigned>(put_bits) >> 3);       // 0x004b267b, shr
        put_bits -= nbytes * 8;                                                    // 0x004b2682
        do {
            unsigned c = (put_buffer >> 16) & 0xff;                                // 0x004b268a
            *reinterpret_cast<unsigned char*>(field_ptr(e, 0x10)) =
                static_cast<unsigned char>(c);                                     // 0x004b268e
            *reinterpret_cast<char**>(e + 0x10) = reinterpret_cast<char*>(field_ptr(e, 0x10)) + 1;
            field_u(e, 0x14) = field_u(e, 0x14) - 1;
            if (field_u(e, 0x14) == 0) kDumpBuffer(e);                             // 0x004b26a2
            if (c == 0xff) {                                                       // 0x004b26b6
                *reinterpret_cast<unsigned char*>(field_ptr(e, 0x10)) = 0;         // 0x004b26bb
                *reinterpret_cast<char**>(e + 0x10) = reinterpret_cast<char*>(field_ptr(e, 0x10)) + 1;
                field_u(e, 0x14) = field_u(e, 0x14) - 1;
                if (field_u(e, 0x14) == 0) kDumpBuffer(e);                         // 0x004b26cc
            }
            put_buffer <<= 8;                                                      // 0x004b26d7
        } while (--nbytes != 0);                                                   // 0x004b26df
    }
    field_u(e, 0x18) = put_buffer;
    field_i(e, 0x1c) = put_bits;
}

// 0x004b25d0  emit_eobrun(entropy): flush the pending run of end-of-block markers of a progressive Huffman
// scan. With EOBRUN (entropy+0x38) zero under an UNSIGNED compare there is nothing to do (0x004b25d6 /
// 0x004b25db). Otherwise the run length is reduced by arithmetic right shifts until it reaches zero and the
// number of shifts past the first gives nbits (0x004b25e6..0x004b25f3). The symbol nbits << 4 (0x004b25ff)
// goes out through the table numbered by ac_tbl_no (entropy+0x34): in statistics mode (gather_statistics,
// entropy+0xc, 0x004b2602 / 0x004b2604) it only increments count_ptrs[ac_tbl_no][symbol]
// (0x004b2606..0x004b260d); otherwise derived_tbls[ac_tbl_no] (0x004b2614) supplies the code at
// ehufco[symbol] (0x004b2623) and the length at ehufsi[symbol], read as a SIGNED byte (0x004b261b), a zero
// length raising ERREXIT 0x27 through entropy->cinfo (entropy+0x20, 0x004b2626..0x004b2640) before the bits
// are emitted anyway. The re-test of gather_statistics at 0x004b264a repeats the one at 0x004b2604 on an
// unchanged field, so its jne side cannot be taken. A non-zero nbits (0x004b26eb / 0x004b26ed) then emits
// the run length itself in nbits bits, again skipped in statistics mode (0x004b26fc / 0x004b26fe). EOBRUN is
// cleared before emit_buffered_bits 0x004b27d0 is handed the correction-bit buffer (entropy+0x40) and its
// count BE (entropy+0x3c) (0x004b279d..0x004b27ad), and BE is cleared after it returns (0x004b27b5).
typedef void(__cdecl* EmitEobrun_t)(void* entropy);
EmitEobrun_t EmitEobrun_orig;
void __cdecl EmitEobrun_re(void* ep) {
    char* e = reinterpret_cast<char*>(ep);
    if (field_u(e, 0x38) == 0) return;                                             // 0x004b25db, unsigned
    int temp = static_cast<int>(field_u(e, 0x38));
    int nbits = 0;
    temp >>= 1;                                                                    // 0x004b25e6, sar
    if (temp != 0) {
        do {
            nbits++;
            temp >>= 1;
        } while (temp != 0);                                                       // 0x004b25f1
    }
    void* cinfo = field_ptr(e, 0x20);
    int tbl = field_i(e, 0x34);
    int symbol = nbits << 4;                                                       // 0x004b25ff
    if (field_i(e, 0xc) != 0) {
        int* counts = reinterpret_cast<int*>(field_ptr(e, 0x5c + tbl * 4));
        counts[symbol] = counts[symbol] + 1;                                       // 0x004b260d
    } else {
        char* dtbl = reinterpret_cast<char*>(field_ptr(e, 0x4c + tbl * 4));
        int size = *reinterpret_cast<signed char*>(dtbl + 0x400 + symbol);         // 0x004b261b, movsx
        unsigned code = *reinterpret_cast<unsigned*>(dtbl + symbol * 4);           // 0x004b2623
        if (size == 0) errexit0(cinfo, 0x27);                                      // 0x004b262c
        emit_bits_inline(e, code, size);
    }
    if (nbits != 0) {                                                              // 0x004b26ed
        if (field_i(e, 0xc) == 0) {                                                // 0x004b26fe
            emit_bits_inline(e, field_u(e, 0x38), nbits);                          // 0x004b26f6, EOBRUN re-read
        }
    }
    void* bufstart = field_ptr(e, 0x40);
    unsigned be = field_u(e, 0x3c);
    field_u(e, 0x38) = 0;                                                          // 0x004b27a6
    kEmitBufferedBits(e, bufstart, be);
    field_u(e, 0x3c) = 0;                                                          // 0x004b27b5
}

}  // namespace

// 0x004b5470  validate_script
SG_HOOK("golf_clean.exe", 0x004b5470, validate_script, ValidateScript_re, ValidateScript_orig);
// 0x004b51e0  jinit_c_master_control
SG_HOOK("golf_clean.exe", 0x004b51e0, jinit_c_master_control, JinitMaster_re, JinitMaster_orig);
// 0x004b5810  prepare_for_pass
SG_HOOK("golf_clean.exe", 0x004b5810, prepare_for_pass, PrepareForPass_re, PrepareForPass_orig);
// 0x004b25d0  emit_eobrun
SG_HOOK("golf_clean.exe", 0x004b25d0, emit_eobrun, EmitEobrun_re, EmitEobrun_orig);

// C3 batch c3cs (round 22, 2026-10-09) of golf_clean.exe: IJG libjpeg 6a compress-side routines the exe links
// from original\JPEG.lib (the sequential Huffman encoder, the scan-header marker writers, the virtual-array
// access layer of the memory manager, jzero_far) plus the UI list accessor listGetAt.
//
// Every body is hand-written from the disassembly (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr>
// --list) and cites the address of each instruction it reproduces. __thiscall is emulated with __fastcall
// (ecx = this, edx unused). Callees run through their original addresses:
//   emit_dri               -> emit_marker 0x004afbf0, emit_2bytes 0x004afc10 (C3, c3af)
//   emit_sos               -> emit_marker, emit_2bytes, emit_byte 0x004afbb0 (C3, c3af)
//   write_scan_header      -> null_method 0x004b01e0 (C2), emit_dht 0x004b0120 (C3, c3aa), emit_dri and emit_sos
//                             (THIS batch)
//   encode_one_block       -> dump_buffer 0x004b19e0 (C3, c3bx), emit_bits 0x004b1a10 (C3, c3aa)
//   emit_restart           -> flush_bits 0x004b1b70 (C3, c3be), dump_buffer 0x004b19e0
//   encode_mcu_huff        -> encode_one_block, emit_restart (THIS batch)
//   encode_mcu_gather      -> htest_one_block (THIS batch)
//   finish_pass_gather     -> jpeg_alloc_huff_table 0x004afad0 (C3, c3af), jpeg_gen_optimal_table (THIS batch)
//   start_pass_huff        -> jpeg_make_c_derived_tbl (THIS batch)
//   expand_bottom_edge     -> jcopy_sample_rows 0x004b04f0 (C3, c3w)
//   access_virt_sarray     -> do_sarray_io, jzero_far (THIS batch)
//   access_virt_barray     -> do_barray_io, jzero_far (THIS batch)
// The error manager (cinfo+0), the memory manager (cinfo+4), the destination manager (cinfo+0x14) and the
// backing-store methods of a virtual array are reached through their own slots in both arms, so the fixture's
// callbacks are what runs there.
//
//   jzero_far 0x004b0540            listGetAt 0x004a48f0             htest_one_block 0x004b1d80
//   jpeg_make_c_derived_tbl 0x004b0f60  jpeg_gen_optimal_table 0x004b1080
//   emit_dri 0x004b01f0             emit_sos 0x004b0220              write_scan_header 0x004b0060
//   encode_one_block 0x004b15c0     emit_restart 0x004b1ae0          encode_mcu_huff 0x004b1480
//   encode_mcu_gather 0x004b1ca0    finish_pass_gather 0x004b1e20    start_pass_huff 0x004b12f0
//   expand_bottom_edge 0x004b3d00   do_sarray_io 0x004af690          do_barray_io 0x004af880
//   access_virt_sarray 0x004af540   access_virt_barray 0x004af730
#include "hooks.h"

// Branch coverage for the purpose file's per-jcc table: BR(addr, taken) is `taken`, the condition under which the
// original's jcc at `addr` jumps. Only a coverage build (a local shim/src/re/c3cs_cov.h, never committed, defining
// C3CS_COV) also counts each (addr, taken) pair in a table the probe log/c3/alt/c3cs_cov.py reads back.
#if defined(__has_include)
#if __has_include("c3cs_cov.h")
#include "c3cs_cov.h"
#endif
#endif
#ifdef C3CS_COV
static unsigned g_c3csCov[1024][2];
static bool c3cs_br(unsigned addr, bool taken) {
    const unsigned key = (addr << 1) | (taken ? 1u : 0u);
    for (unsigned i = (key * 2654435761u) >> 22, n = 0; n < 1024; i = (i + 1) & 1023, n++) {
        if (g_c3csCov[i][0] == key) { g_c3csCov[i][1]++; break; }
        if (g_c3csCov[i][0] == 0) { g_c3csCov[i][0] = key; g_c3csCov[i][1] = 1; break; }
    }
    return taken;
}
extern "C" __declspec(dllexport) unsigned* C3csCovTable() { return &g_c3csCov[0][0]; }
#define BR(addr, taken) c3cs_br((addr), (taken) ? true : false)
#else
#define BR(addr, taken) ((taken) ? true : false)
#endif

namespace {

// ---- IJG libjpeg 6a structures reached through pointers -------------------------------------------------------
// jpeg_compress_struct: err +0 (jpeg_error_mgr: error_exit +0, msg_code +0x14, msg_parm.i[0] +0x18), mem +4
// (jpeg_memory_mgr: alloc_small +0), dest +0x14 (next_output_byte +0, free_in_buffer +4), dc_huff_tbl_ptrs[4]
// +0x50, ac_huff_tbl_ptrs[4] +0x60, arith_code +0xac, restart_interval +0xc0, progressive_mode +0xdc,
// comps_in_scan +0xec, cur_comp_info[4] +0xf0, blocks_in_MCU +0x108, MCU_membership[10] +0x10c, Ss +0x134,
// Se +0x138, Ah +0x13c, Al +0x140, entropy +0x164.
// jpeg_component_info: component_id +0, dc_tbl_no +0x14, ac_tbl_no +0x18.
// JHUFF_TBL: bits[17] +0, huffval[256] +0x11, sent_table +0x114. c_derived_tbl: ehufco[256] +0 (unsigned int),
// ehufsi[256] +0x400 (char).
// huff_entropy_encoder (jchuff.c): pub (start_pass +0, encode_mcu +4, finish_pass +8), saved (put_buffer +0xc,
// put_bits +0x10, last_dc_val[4] +0x14), restarts_to_go +0x24, next_restart_num +0x28, dc_derived_tbls[4]
// +0x2c, ac_derived_tbls[4] +0x3c, dc_count_ptrs[4] +0x4c, ac_count_ptrs[4] +0x5c.
// working_state (jchuff.c, 0x24 bytes): next_output_byte +0, free_in_buffer +4, put_buffer +8, put_bits +0xc,
// last_dc_val[4] +0x10, cinfo +0x20.
// jvirt_sarray_control / jvirt_barray_control (jmemmgr.c): mem_buffer +0, rows_in_array +4, samplesperrow
// (blocksperrow) +8, maxaccess +0xc, rows_in_mem +0x10, rowsperchunk +0x14, cur_start_row +0x18,
// first_undef_row +0x1c, pre_zero +0x20, dirty +0x24, b_s_open +0x28, b_s_info +0x30 (read_backing_store +0,
// write_backing_store +4 within it).
typedef void(__cdecl* ErrExit_t)(void* cinfo);

static inline void* field_ptr(void* base, unsigned off) {
    return *reinterpret_cast<void**>(reinterpret_cast<char*>(base) + off);
}
static inline int& field_i(void* base, unsigned off) {
    return *reinterpret_cast<int*>(reinterpret_cast<char*>(base) + off);
}
static inline unsigned& field_u(void* base, unsigned off) {
    return *reinterpret_cast<unsigned*>(reinterpret_cast<char*>(base) + off);
}
static inline unsigned char& field_b(void* base, unsigned off) {
    return *reinterpret_cast<unsigned char*>(reinterpret_cast<char*>(base) + off);
}
// ERREXIT: store the code in cinfo->err->msg_code and call cinfo->err->error_exit(cinfo), re-reading cinfo+0
// for each half as the originals do.
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

// Callees reached through their original addresses.
typedef void(__cdecl* EmitVal_t)(void* cinfo, int val);
const EmitVal_t kEmitByte = reinterpret_cast<EmitVal_t>(0x004afbb0);            // emit_byte   (C3, c3af)
const EmitVal_t kEmitMarker = reinterpret_cast<EmitVal_t>(0x004afbf0);          // emit_marker (C3, c3af)
const EmitVal_t kEmit2Bytes = reinterpret_cast<EmitVal_t>(0x004afc10);          // emit_2bytes (C3, c3af)
typedef void(__cdecl* VoidCinfo_t)(void* cinfo);
const VoidCinfo_t kNullMethod = reinterpret_cast<VoidCinfo_t>(0x004b01e0);      // a bare `ret` (C2)
const VoidCinfo_t kEmitDri = reinterpret_cast<VoidCinfo_t>(0x004b01f0);         // THIS batch
const VoidCinfo_t kEmitSos = reinterpret_cast<VoidCinfo_t>(0x004b0220);         // THIS batch
typedef void(__cdecl* EmitDht_t)(void* cinfo, int index, int is_ac);
const EmitDht_t kEmitDht = reinterpret_cast<EmitDht_t>(0x004b0120);             // emit_dht    (C3, c3aa)
typedef int(__cdecl* DumpBuffer_t)(void* state);
const DumpBuffer_t kDumpBuffer = reinterpret_cast<DumpBuffer_t>(0x004b19e0);    // dump_buffer (C3, c3bx)
const DumpBuffer_t kFlushBits = reinterpret_cast<DumpBuffer_t>(0x004b1b70);     // flush_bits  (C3, c3be)
typedef int(__cdecl* EmitBits_t)(void* state, unsigned code, int size);
const EmitBits_t kEmitBits = reinterpret_cast<EmitBits_t>(0x004b1a10);          // emit_bits   (C3, c3aa)
typedef int(__cdecl* EncodeOneBlock_t)(void* state, short* block, int last_dc_val, void* dctbl, void* actbl);
const EncodeOneBlock_t kEncodeOneBlock = reinterpret_cast<EncodeOneBlock_t>(0x004b15c0);   // THIS batch
typedef int(__cdecl* EmitRestart_t)(void* state, int restart_num);
const EmitRestart_t kEmitRestart = reinterpret_cast<EmitRestart_t>(0x004b1ae0);           // THIS batch
typedef void(__cdecl* HtestOneBlock_t)(short* block, int last_dc_val, int* dc_counts, int* ac_counts);
const HtestOneBlock_t kHtestOneBlock = reinterpret_cast<HtestOneBlock_t>(0x004b1d80);     // THIS batch
typedef void*(__cdecl* AllocHuffTable_t)(void* cinfo);
const AllocHuffTable_t kAllocHuffTable = reinterpret_cast<AllocHuffTable_t>(0x004afad0);  // C3, c3af
typedef void(__cdecl* GenOptimal_t)(void* cinfo, void* htbl, int* freq);
const GenOptimal_t kGenOptimalTable = reinterpret_cast<GenOptimal_t>(0x004b1080);         // THIS batch
typedef void(__cdecl* MakeDerived_t)(void* cinfo, void* htbl, void** pdtbl);
const MakeDerived_t kMakeDerivedTbl = reinterpret_cast<MakeDerived_t>(0x004b0f60);        // THIS batch
typedef void(__cdecl* CopyRows_t)(void* in, int src_row, void* out, int dst_row, int num_rows, unsigned num_cols);
const CopyRows_t kCopySampleRows = reinterpret_cast<CopyRows_t>(0x004b04f0);              // C3, c3w
typedef void(__cdecl* VirtIo_t)(void* cinfo, void* ptr, int writing);
const VirtIo_t kDoSarrayIo = reinterpret_cast<VirtIo_t>(0x004af690);                      // THIS batch
const VirtIo_t kDoBarrayIo = reinterpret_cast<VirtIo_t>(0x004af880);                      // THIS batch
typedef void(__cdecl* JzeroFar_t)(void* target, unsigned bytes);
const JzeroFar_t kJzeroFar = reinterpret_cast<JzeroFar_t>(0x004b0540);                    // THIS batch
typedef void*(__cdecl* AllocSmall_t)(void* cinfo, int pool_id, unsigned size);
typedef void(__cdecl* BackingIo_t)(void* cinfo, void* info, void* buffer, int file_offset, int byte_count);

// jpeg_natural_order[1..63]: the dword table the encoders walk, 0x004bd0b0 up to (not including) 0x004bd1ac
// (0x004b1da8 / 0x004b1e0d, 0x004b1764 / 0x004b1994).
const int* const kNaturalOrder1 = reinterpret_cast<const int*>(0x004bd0b0);

// 0x004b0540  jzero_far(target, bytes): zero `bytes` bytes at target: `rep stosd` of bytes >> 2 dwords
// (0x004b054d / 0x004b0550) then `rep stosb` of bytes & 3 bytes (0x004b0554 / 0x004b0557), both with eax = 0.
JzeroFar_t JzeroFar_orig;
void __cdecl JzeroFar_re(void* target, unsigned bytes) {
    unsigned* d = reinterpret_cast<unsigned*>(target);
    // (the two `rep` instructions are recorded as "taken" when their count is non-zero)
    for (unsigned n = bytes >> 2; BR(0x004b0550, n != 0); n--) *d++ = 0;
    unsigned char* b = reinterpret_cast<unsigned char*>(d);
    for (unsigned n = bytes & 3; BR(0x004b0557, n != 0); n--) *b++ = 0;
}

// 0x004a48f0  listGetAt(this, index) (__thiscall, ret 4): position the list's cursor (this+0x84) on element
// `index` and return that element's value (+4). this+0x80 is the head node, this+0x88 the element count,
// this+0x8c the cursor's index; a node links forward at +0xc and backward at +0x10.
typedef void*(__fastcall* ListGetAt_t)(void* self, void* edx, int index);
ListGetAt_t ListGetAt_orig;
void* __fastcall ListGetAt_re(void* self, void*, int index) {
    const int count = field_i(self, 0x88);                                         // 0x004a48f6
    // 0x004a48fe / 0x004a4903: indices above count - 1 (signed, count - 1 wraps) leave the cursor alone.
    if (!BR(0x004a4903, index > static_cast<int>(static_cast<unsigned>(count) - 1u))) {
        *reinterpret_cast<void**>(reinterpret_cast<char*>(self) + 0x84) = field_ptr(self, 0x80);   // 0x004a490d
        int pos = index;                                                           // 0x004a48fc
        if (!BR(0x004a4913, index >= 0)) {
            // |index| by cdq / xor / sub (0x004a4915..0x004a4918): INT_MIN stays negative.
            const int mag = static_cast<int>((static_cast<unsigned>(index) ^ static_cast<unsigned>(index >> 31))
                                             - static_cast<unsigned>(index >> 31));
            if (BR(0x004a491c, mag > count)) goto done;                            // 0x004a491a
            if (!BR(0x004a4920, mag <= 0)) {
                int n = mag;
                do {                                                               // 0x004a4922..0x004a4932
                    *reinterpret_cast<void**>(reinterpret_cast<char*>(self) + 0x84) =
                        field_ptr(field_ptr(self, 0x84), 0x10);
                } while (BR(0x004a4932, --n != 0));
            }
            pos = static_cast<int>(static_cast<unsigned>(count) + static_cast<unsigned>(index));   // 0x004a4934
        } else if (!BR(0x004a4939, index <= 0)) {
            int n = index;
            do {                                                                   // 0x004a493b..0x004a494b
                *reinterpret_cast<void**>(reinterpret_cast<char*>(self) + 0x84) =
                    field_ptr(field_ptr(self, 0x84), 0xc);
            } while (BR(0x004a494b, --n != 0));
        }
        field_i(self, 0x8c) = pos;                                                 // 0x004a494d
    }
done:
    if (BR(0x004a495d, field_ptr(self, 0x80) == nullptr)) return nullptr;          // 0x004a4953
    return field_ptr(field_ptr(self, 0x84), 4);                                    // 0x004a495f / 0x004a4965
}

// 0x004b1d80  htest_one_block(block, last_dc_val, dc_counts, ac_counts): count the Huffman symbols one 8x8
// block of coefficients would use. The DC difference block[0] - last_dc_val (0x004b1d8c / 0x004b1d8f) is made
// non-negative (0x004b1d91 / 0x004b1d93) and its bit count (0x004b1d95..0x004b1d9e, arithmetic shifts) indexes
// dc_counts (0x004b1dad..0x004b1db1). The 63 AC coefficients are read in the order of the dword table at
// 0x004bd0b0 (0x004b1db6 / 0x004b1db8): a zero extends the run r (0x004b1dc0); a non-zero one first spends
// runs above 15 as r >> 4 increments of ac_counts[0xf0] (+0x3c0, 0x004b1dc3..0x004b1de0, r -= (r >> 4) << 4),
// then counts symbol (r << 4) + nbits, nbits being the bit count of |coef| starting at 1 (0x004b1de6..0x004b1e08),
// and clears r. A trailing run counts one end-of-block symbol ac_counts[0] (0x004b1e15 / 0x004b1e19).
HtestOneBlock_t HtestOneBlock_orig;
void __cdecl HtestOneBlock_re(short* block, int last_dc_val, int* dc_counts, int* ac_counts) {
    int temp = static_cast<int>(static_cast<unsigned>(static_cast<int>(block[0])) - static_cast<unsigned>(last_dc_val));
    if (!BR(0x004b1d91, temp >= 0)) temp = static_cast<int>(0u - static_cast<unsigned>(temp));   // 0x004b1d93
    int nbits = 0;
    if (!BR(0x004b1d99, temp == 0)) {
        do { nbits++; temp >>= 1; } while (BR(0x004b1d9e, temp != 0));             // 0x004b1d9b..0x004b1d9e
    }
    dc_counts[nbits]++;                                                            // 0x004b1db1
    int r = 0;
    for (int k = 0; k < 63; k++) {
        int coef = block[kNaturalOrder1[k]];                                       // 0x004b1db8
        if (!BR(0x004b1dbe, coef != 0)) { r++; continue; }                         // 0x004b1dc0
        if (!BR(0x004b1dc6, r <= 15)) {
            const unsigned runs = static_cast<unsigned>(r) >> 4;                   // 0x004b1dca
            r += static_cast<int>((0u - runs) << 4);                               // 0x004b1dcf..0x004b1dd4
            ac_counts[0xf0] += static_cast<int>(runs);                             // 0x004b1ddc..0x004b1de0
        }
        if (!BR(0x004b1de8, coef >= 0)) coef = -coef;                              // 0x004b1dea
        int bits = 1;
        coef >>= 1;                                                                // 0x004b1dec
        if (!BR(0x004b1df3, coef == 0)) {
            do { bits++; coef >>= 1; } while (BR(0x004b1df8, coef != 0));          // 0x004b1df5..0x004b1df8
        }
        ac_counts[(r << 4) + bits]++;                                              // 0x004b1dfa..0x004b1e08
        r = 0;                                                                     // 0x004b1e06
    }
    if (!BR(0x004b1e17, r <= 0)) ac_counts[0]++;                                   // 0x004b1e19
}

// 0x004b0f60  jpeg_make_c_derived_tbl(cinfo, htbl, pdtbl): build the encoder's code/size lookup from a JHUFF_TBL.
// A NULL *pdtbl gets a 0x500-byte table from cinfo->mem->alloc_small(cinfo, 1, 0x500) (0x004b0f71..0x004b0f8d).
// huffsize[] gets bits[l] copies of l for l = 1..16 (bits read unsigned, 0x004b0fa9, `rep stos` 0x004b0fcf /
// 0x004b0fd8) and a closing 0 (0x004b0fe8); codes are assigned in order, doubling at each new length
// (0x004b0ff1..0x004b1025). ehufsi (+0x400) is cleared (0x004b1032 / 0x004b103a), then every symbol huffval[p]
// (htbl+0x11, unsigned) gets its code in ehufco and its length in ehufsi (0x004b1045..0x004b106d).
MakeDerived_t MakeDerivedTbl_orig;
void __cdecl MakeDerivedTbl_re(void* cinfo, void* htbl, void** pdtbl) {
    if (!BR(0x004b0f74, *pdtbl != nullptr)) {                                      // 0x004b0f71
        AllocSmall_t alloc = reinterpret_cast<AllocSmall_t>(field_ptr(field_ptr(cinfo, 4), 0));   // 0x004b0f85
        *pdtbl = alloc(cinfo, 1, 0x500);                                           // 0x004b0f88 / 0x004b0f8d
    }
    char* dtbl = reinterpret_cast<char*>(*pdtbl);                                  // 0x004b0f8f
    signed char huffsize[257];
    int huffcode[257];
    int p = 0;
    int l = 1;
    do {
        const int n = field_b(htbl, l);                                            // 0x004b0fa9
        if (!BR(0x004b0fb1, n < 1)) {                                              // 0x004b0fae
            for (int i = 0; i < n; i++) huffsize[p + i] = static_cast<signed char>(l);   // 0x004b0fcf / 0x004b0fd8
            p += n;                                                                // 0x004b0fd6
        }
        l++;                                                                       // 0x004b0fde
    } while (BR(0x004b0fe6, l <= 16));
    huffsize[p] = 0;                                                               // 0x004b0fe8
    const int lastp = p;
    unsigned code = 0;
    p = 0;
    int si = huffsize[0];                                                          // 0x004b0ff7
    if (!BR(0x004b0ffa, huffsize[0] == 0)) {
        do {
            if (!BR(0x004b1003, huffsize[p] != si)) {
                do {
                    huffcode[p++] = static_cast<int>(code++);                      // 0x004b1011 / 0x004b1012 / 0x004b1017
                } while (BR(0x004b101a, huffsize[p] == si));
            }
            code <<= 1;                                                            // 0x004b1020
            si++;                                                                  // 0x004b1022
        } while (BR(0x004b1025, huffsize[p] != 0));
    }
    for (int i = 0; i < 0x100; i++) dtbl[0x400 + i] = 0;                           // 0x004b103a
    if (!BR(0x004b103c, lastp <= 0)) {
        p = 0;
        do {
            const unsigned sym = field_b(htbl, 0x11 + p);                          // 0x004b1050 / 0x004b1059
            field_i(dtbl, sym * 4) = huffcode[p];                                  // 0x004b1054
            dtbl[0x400 + sym] = huffsize[p];                                       // 0x004b1066
            p++;
        } while (BR(0x004b106d, p < lastp));
    }
}

// 0x004b1080  jpeg_gen_optimal_table(cinfo, htbl, freq): derive an optimal Huffman table from the 257 symbol
// counts. freq[256] is set to 1 (0x004b10aa); codesize[] starts 0 and others[] -1 (0x004b10b4 / 0x004b10c5).
// Each round picks c1, the non-zero count <= every other (ties go to the higher index: `jg` at 0x004b10db), and
// c2 the same excluding c1 (0x004b1103); with no c2 (0x004b1117) the tree is done. Otherwise c2's count is added
// to c1's and cleared (0x004b1127..0x004b1135), every node on c1's chain and then on c2's gains one bit
// (0x004b1130..0x004b1159, 0x004b116d..0x004b118c) and c2's chain is hung after c1's (0x004b115f).
// bits[codesize]++ per used symbol, with ERREXIT 0x26 for a length above 32 (0x004b11a9 / 0x004b11b1); lengths
// above 16 are folded down (0x004b11d1..0x004b1228, unsigned byte compares), the reserved code removed from the
// longest length (0x004b122a..0x004b1251), bits[0..16] copied to htbl (0x004b125d..0x004b1274), huffval filled
// by length then symbol over symbols 0..255 (0x004b1277..0x004b1297), and sent_table (+0x114) cleared (0x004b1299).
GenOptimal_t GenOptimalTable_orig;
void __cdecl GenOptimalTable_re(void* cinfo, void* htbl, int* freq) {
    // bits[] is 33 bytes in the original (esp+0x10..0x30); a returning ERREXIT lets lengths 33..35 land in the
    // three padding bytes after it (esp+0x31..0x33), which nothing reads, so they get their own slots here.
    unsigned char bits[36] = {0};                                                  // 0x004b109c / 0x004b109e
    int codesize[257];
    int others[257];
    freq[256] = 1;                                                                 // 0x004b10aa
    for (int i = 0; i < 257; i++) { codesize[i] = 0; others[i] = -1; }             // 0x004b10b4 / 0x004b10c5
    for (;;) {
        int c1 = -1, v = 1000000000;                                               // 0x004b10c7 / 0x004b10ca
        for (int i = 0; i <= 256; i++)                                             // 0x004b10eb
            if (!BR(0x004b10d7, freq[i] == 0) && !BR(0x004b10db, freq[i] > v)) { v = freq[i]; c1 = i; }
        int c2 = -1;
        v = 1000000000;                                                            // 0x004b10ed / 0x004b10f0
        for (int i = 0; i <= 256; i++)                                             // 0x004b1115
            if (!BR(0x004b10fd, freq[i] == 0) && !BR(0x004b1101, freq[i] > v) && !BR(0x004b1105, i == c1)) {
                v = freq[i];
                c2 = i;
            }
        if (BR(0x004b1119, c2 < 0)) break;
        freq[c1] = static_cast<int>(static_cast<unsigned>(freq[c1]) + static_cast<unsigned>(freq[c2]));
        freq[c2] = 0;                                                              // 0x004b1135
        codesize[c1]++;                                                            // 0x004b1130 / 0x004b113f
        if (!BR(0x004b1143, others[c1] < 0)) {
            do { c1 = others[c1]; codesize[c1]++; } while (BR(0x004b1159, others[c1] >= 0));   // 0x004b1145
        }
        others[c1] = c2;                                                           // 0x004b115f
        codesize[c2]++;                                                            // 0x004b116d / 0x004b1170
        if (!BR(0x004b1174, others[c2] < 0)) {
            do { c2 = others[c2]; codesize[c2]++; } while (BR(0x004b118c, others[c2] >= 0));   // 0x004b117a
        }
    }
    for (int i = 0; i <= 256; i++) {                                               // 0x004b11a3..0x004b11cf
        const int cs = codesize[i];
        if (BR(0x004b11a7, cs == 0)) continue;
        if (!BR(0x004b11ac, cs <= 32)) errexit0(cinfo, 0x26);                      // 0x004b11ae..0x004b11ba
        bits[cs]++;                                                                // 0x004b11bf..0x004b11c9
    }
    int i = 32;
    do {                                                                           // 0x004b11d1 / 0x004b1228
        if (!BR(0x004b11dc, bits[i] == 0)) {
            do {
                int j = i - 2;                                                     // 0x004b11de
                if (!BR(0x004b11e9, bits[j] != 0)) {
                    do { j--; } while (BR(0x004b11f2, bits[j] == 0));              // 0x004b11eb..0x004b11f2
                }
                bits[i] = static_cast<unsigned char>(bits[i] - 2);                 // 0x004b11f4
                bits[i - 1]++;                                                     // 0x004b11fb..0x004b1201
                bits[j + 1] = static_cast<unsigned char>(bits[j + 1] + 2);         // 0x004b1205..0x004b120c
                bits[j]--;                                                         // 0x004b1210..0x004b121a
            } while (BR(0x004b1222, bits[i] != 0));
        }
        i--;                                                                       // 0x004b1224
    } while (BR(0x004b1228, i > 16));
    if (!BR(0x004b1230, bits[i] != 0)) {
        do { i--; } while (BR(0x004b1239, bits[i] == 0));                          // 0x004b1232..0x004b1239
    }
    bits[i]--;                                                                     // 0x004b123b..0x004b1251
    for (int k = 0; k < 17; k++) field_b(htbl, k) = bits[k];                       // 0x004b125d..0x004b1274
    int p = 0;
    for (int len = 1; len <= 32; len++)                                            // 0x004b1293 / 0x004b1297
        for (int j = 0; j <= 255; j++)                                             // 0x004b1288..0x004b1291
            if (!BR(0x004b1281, codesize[j] != len))
                field_b(htbl, 0x11 + p++) = static_cast<unsigned char>(j);         // 0x004b1283
    field_i(htbl, 0x114) = 0;                                                      // 0x004b1299
}

// 0x004b01f0  emit_dri(cinfo): the DRI marker: emit_marker(cinfo, 0xdd) (0x004b01f5 / 0x004b01fb),
// emit_2bytes(cinfo, 4) (0x004b0200 / 0x004b0203) and emit_2bytes(cinfo, restart_interval) (cinfo+0xc0,
// 0x004b0208 / 0x004b0210).
VoidCinfo_t EmitDri_orig;
void __cdecl EmitDri_re(void* cinfo) {
    kEmitMarker(cinfo, 0xdd);
    kEmit2Bytes(cinfo, 4);
    kEmit2Bytes(cinfo, field_i(cinfo, 0xc0));
}

// 0x004b0220  emit_sos(cinfo): the SOS marker. emit_marker 0xda (0x004b0227), the length comps_in_scan * 2 + 6
// (0x004b0238 / 0x004b023e) and comps_in_scan (0x004b024b); per component of the scan (count re-read at
// 0x004b02b2) its id (0x004b026c) and (td << 4) + ta (0x004b02a6..0x004b02ad), where td / ta are the DC / AC table
// numbers (+0x14 / +0x18) except in progressive mode (cinfo+0xdc, 0x004b0280): an AC scan (Ss cinfo+0x134 != 0,
// 0x004b028c) sends td = 0, a DC scan sends ta = 0 (0x004b0294) and also td = 0 when it is a refinement (Ah
// cinfo+0x13c != 0, 0x004b0298) without arithmetic coding (cinfo+0xac, 0x004b02a2). Then Ss (0x004b02cc), Se
// (cinfo+0x138, 0x004b02d9) and (Ah << 4) + Al (cinfo+0x140, 0x004b02ea..0x004b02f1).
VoidCinfo_t EmitSos_orig;
void __cdecl EmitSos_re(void* cinfo) {
    kEmitMarker(cinfo, 0xda);
    kEmit2Bytes(cinfo, field_i(cinfo, 0xec) * 2 + 6);
    kEmitByte(cinfo, field_i(cinfo, 0xec));
    if (!BR(0x004b025d, field_i(cinfo, 0xec) <= 0)) {
        int i = 0;
        do {
            void* comp = field_ptr(cinfo, 0xf0 + i * 4);                           // 0x004b0266
            kEmitByte(cinfo, field_i(comp, 0));                                    // 0x004b026c
            int td = field_i(comp, 0x14);                                          // 0x004b0277
            int ta = field_i(comp, 0x18);                                          // 0x004b027a
            if (!BR(0x004b0282, field_i(cinfo, 0xdc) == 0)) {
                if (BR(0x004b028c, field_i(cinfo, 0x134) != 0)) {
                    td = 0;                                                        // 0x004b02a4
                } else {
                    ta = 0;                                                        // 0x004b0294
                    if (!BR(0x004b0298, field_i(cinfo, 0x13c) == 0) && !BR(0x004b02a2, field_i(cinfo, 0xac) != 0))
                        td = 0;                                                    // 0x004b02a4
                }
            }
            kEmitByte(cinfo, (td << 4) + ta);                                      // 0x004b02ad
            i++;                                                                   // 0x004b02bb
        } while (BR(0x004b02c1, i < field_i(cinfo, 0xec)));                        // 0x004b02b2
    }
    kEmitByte(cinfo, field_i(cinfo, 0x134));
    kEmitByte(cinfo, field_i(cinfo, 0x138));
    kEmitByte(cinfo, (field_i(cinfo, 0x13c) << 4) + field_i(cinfo, 0x140));
}

// 0x004b0060  write_scan_header(cinfo): with arith_code (cinfo+0xac, 0x004b006e) call the empty method
// 0x004b01e0; otherwise, per component of the scan (count re-read at 0x004b00ef), emit its Huffman tables with
// emit_dht 0x004b0120: sequential mode (cinfo+0xdc re-read each pass, 0x004b009c) sends DC then AC
// (0x004b00db / 0x004b00e7); progressive mode sends only the AC table for an AC scan (Ss != 0, 0x004b00a6 /
// 0x004b00ca) and only the DC table for a first DC scan (Ah == 0, 0x004b00b0 / 0x004b00b9). Then DRI when
// restart_interval (cinfo+0xc0) is set (0x004b0107 / 0x004b010a) and SOS (0x004b0113).
VoidCinfo_t WriteScanHeader_orig;
void __cdecl WriteScanHeader_re(void* cinfo) {
    if (!BR(0x004b006e, field_i(cinfo, 0xac) == 0)) {
        kNullMethod(cinfo);                                                        // 0x004b0071
    } else if (!BR(0x004b0088, field_i(cinfo, 0xec) <= 0)) {
        int i = 0;
        do {
            void* comp = field_ptr(cinfo, 0xf0 + i * 4);                           // 0x004b0098
            if (!BR(0x004b009c, field_i(cinfo, 0xdc) == 0)) {
                if (BR(0x004b00a6, field_i(cinfo, 0x134) != 0)) {
                    kEmitDht(cinfo, field_i(comp, 0x18), 1);                       // 0x004b00ca
                } else if (!BR(0x004b00b0, field_i(cinfo, 0x13c) != 0)) {
                    kEmitDht(cinfo, field_i(comp, 0x14), 0);                       // 0x004b00b9
                }
            } else {
                kEmitDht(cinfo, field_i(comp, 0x14), 0);                           // 0x004b00db
                kEmitDht(cinfo, field_i(comp, 0x18), 1);                           // 0x004b00e7
            }
            i++;                                                                   // 0x004b00f5
        } while (BR(0x004b00fb, i < field_i(cinfo, 0xec)));                        // 0x004b00ef
    }
    if (!BR(0x004b0107, field_i(cinfo, 0xc0) == 0)) kEmitDri(cinfo);               // 0x004b010a
    kEmitSos(cinfo);                                                               // 0x004b0113
}

// One inlined copy of emit_bits as encode_one_block contains it (e.g. 0x004b162b..0x004b16c1): mask `code` to
// `size` bits, align them at 24 - put_bits, merge with put_buffer (state+8), and while 8 or more bits are
// pending store byte (put_buffer >> 16) & 0xff through state->next_output_byte, decrement free_in_buffer and call
// dump_buffer 0x004b19e0 when it reaches 0 (a 0 return aborts at once, put_buffer / put_bits not stored); a 0xff
// byte is followed by a stuffed 0 the same way. Shift counts are taken mod 32 as `shl r, cl` does. The copies
// that raise ERREXIT 0x27 for a zero size first have a non-zero `check` jcc address.
struct EmitCopy {
    unsigned check;      // jne past the ERREXIT (0 = this copy has none)
    unsigned enter;      // jl skipping the byte loop
    unsigned free1;      // jne past dump_buffer after the byte
    unsigned fail1;      // je to the return-FALSE exit
    unsigned ff;         // jne past the stuffed zero
    unsigned free2;
    unsigned fail2;
    unsigned again;      // jge back to the loop head
};
const EmitCopy kCopyDcSym = {0x004b1612, 0x004b1652, 0x004b166a, 0x004b1677, 0x004b1683, 0x004b1696, 0x004b16a3, 0x004b16b6};
const EmitCopy kCopyDcVal = {0, 0x004b16ef, 0x004b170b, 0x004b1718, 0x004b1727, 0x004b173a, 0x004b1747, 0x004b175a};
const EmitCopy kCopyZrl = {0x004b17b0, 0x004b17f0, 0x004b1808, 0x004b1815, 0x004b1821, 0x004b1834, 0x004b1841, 0x004b1854};
const EmitCopy kCopyAcSym = {0x004b18c2, 0x004b1902, 0x004b191a, 0x004b1927, 0x004b1933, 0x004b1946, 0x004b1953, 0x004b1962};
static bool emit_bits_inline(char* state, unsigned code, int size, const EmitCopy& at) {
    if (at.check != 0 && !BR(at.check, size != 0)) errexit0(field_ptr(state, 0x20), 0x27);
    int put_bits = field_i(state, 0xc) + size;
    unsigned buffer = ((code & ((1u << (size & 31)) - 1u)) << ((24 - put_bits) & 31)) | field_u(state, 8);
    if (!BR(at.enter, put_bits < 8)) {
        do {
            const unsigned c = (buffer >> 16) & 0xff;
            *reinterpret_cast<unsigned char*>(field_ptr(state, 0)) = static_cast<unsigned char>(c);
            field_u(state, 0) += 1;
            if (!BR(at.free1, --field_u(state, 4) != 0) && BR(at.fail1, kDumpBuffer(state) == 0)) return false;
            if (!BR(at.ff, c != 0xff)) {
                *reinterpret_cast<unsigned char*>(field_ptr(state, 0)) = 0;
                field_u(state, 0) += 1;
                if (!BR(at.free2, --field_u(state, 4) != 0) && BR(at.fail2, kDumpBuffer(state) == 0)) return false;
            }
            put_bits -= 8;
            buffer <<= 8;
        } while (BR(at.again, put_bits >= 8));
    }
    field_u(state, 8) = buffer;
    field_i(state, 0xc) = put_bits;
    return true;
}

// 0x004b15c0  encode_one_block(state, block, last_dc_val, dctbl, actbl): Huffman-code one 8x8 block into the
// working state. DC: diff = block[0] - last_dc_val (0x004b15cb / 0x004b15ce), magnitude and complement for a
// negative diff (0x004b15d8 / 0x004b15da), its bit count (0x004b15e9..0x004b15ec); the symbol is emitted with
// dctbl's code/size (0x004b15f9 / 0x004b1605, ERREXIT 0x27 for size 0 at 0x004b1612) and the value bits only for
// a non-zero count (0x004b16c4). AC, in the order of the table at 0x004bd0b0 (0x004b1771 / 0x004b1777): a zero
// extends the run (0x004b1783); before a non-zero one each run of 16 emits actbl's code 0xf0 (+0x3c0 / +0x4f0,
// 0x004b1796..0x004b186a, ERREXIT 0x27 at 0x004b17b0); then symbol (r << 4) + nbits (0x004b18ac..0x004b18b9,
// ERREXIT 0x27 at 0x004b18c2) and the value bits through emit_bits 0x004b1a10 (0x004b1975). A trailing run emits
// symbol 0 through emit_bits (0x004b19a4..0x004b19b8). Any failed byte flush returns 0 (0x004b19c4); else 1.
EncodeOneBlock_t EncodeOneBlock_orig;
int __cdecl EncodeOneBlock_re(void* statep, short* block, int last_dc_val, void* dctblp, void* actblp) {
    char* state = reinterpret_cast<char*>(statep);
    char* dctbl = reinterpret_cast<char*>(dctblp);
    char* actbl = reinterpret_cast<char*>(actblp);
    int temp = static_cast<int>(static_cast<unsigned>(static_cast<int>(block[0])) - static_cast<unsigned>(last_dc_val));
    int temp2 = temp;
    if (!BR(0x004b15d6, temp >= 0)) {
        temp = static_cast<int>(0u - static_cast<unsigned>(temp));                 // 0x004b15d8
        temp2 = static_cast<int>(static_cast<unsigned>(temp2) - 1u);               // 0x004b15da
    }
    int nbits = 0;
    if (!BR(0x004b15e7, temp == 0)) {
        do { nbits++; temp >>= 1; } while (BR(0x004b15ec, temp != 0));            // 0x004b15e9..0x004b15ec
    }
    if (!emit_bits_inline(state, field_u(dctbl, nbits * 4),
                          static_cast<signed char>(dctbl[0x400 + nbits]), kCopyDcSym)) return 0;
    if (!BR(0x004b16c4, nbits == 0) && !emit_bits_inline(state, static_cast<unsigned>(temp2), nbits, kCopyDcVal))
        return 0;
    int r = 0;
    for (int k = 0; k < 63; k++) {                                                 // 0x004b199e
        int coef = block[kNaturalOrder1[k]];                                       // 0x004b1777
        if (!BR(0x004b1781, coef != 0)) { r++; continue; }                         // 0x004b1783
        if (!BR(0x004b1790, r <= 15)) {
            do {
                if (!emit_bits_inline(state, field_u(actbl, 0x3c0),
                                      static_cast<signed char>(actbl[0x4f0]), kCopyZrl)) return 0;
                r -= 16;                                                           // 0x004b185d
            } while (BR(0x004b186a, r > 15));
        }
        int cv2 = coef;
        if (!BR(0x004b187a, coef >= 0)) { coef = -coef; cv2--; }                   // 0x004b187c..0x004b1886
        int bits = 1;
        coef >>= 1;                                                                // 0x004b1894
        if (!BR(0x004b189a, coef == 0)) {
            do { bits++; coef >>= 1; } while (BR(0x004b189f, coef != 0));         // 0x004b189c..0x004b189f
        }
        const int i = (r << 4) + bits;                                             // 0x004b18ac / 0x004b18af
        if (!emit_bits_inline(state, field_u(actbl, i * 4),
                              static_cast<signed char>(actbl[0x400 + i]), kCopyAcSym)) return 0;
        if (BR(0x004b197f, kEmitBits(state, static_cast<unsigned>(cv2), bits) == 0)) return 0;   // 0x004b1975
        r = 0;                                                                     // 0x004b1985
    }
    if (!BR(0x004b19a6, r <= 0)) {
        if (!BR(0x004b19c2, kEmitBits(state, field_u(actbl, 0), static_cast<signed char>(actbl[0x400])) != 0))
            return 0;                                                              // 0x004b19b8
    }
    return 1;                                                                      // 0x004b19d1
}

// 0x004b1ae0  emit_restart(state, restart_num): flush_bits 0x004b1b70 (a 0 return is returned, 0x004b1af0), then
// the bytes 0xff (0x004b1af6) and restart_num - 0x30, i.e. 0xd0 + restart_num as a byte (0x004b1b1c / 0x004b1b1f),
// each followed by dump_buffer 0x004b19e0 when free_in_buffer reaches 0 (a 0 return is returned, 0x004b1b12 /
// 0x004b1b3a); then last_dc_val[ci] (state+0x10) is cleared for every component of the scan (the count read
// through state->cinfo each pass, 0x004b1b43 / 0x004b1b5d) and 1 returned.
EmitRestart_t EmitRestart_orig;
int __cdecl EmitRestart_re(void* statep, int restart_num) {
    char* state = reinterpret_cast<char*>(statep);
    const int f = kFlushBits(state);
    if (!BR(0x004b1af0, f != 0)) return f;
    *reinterpret_cast<unsigned char*>(field_ptr(state, 0)) = 0xff;                 // 0x004b1af6
    field_u(state, 0) += 1;
    if (!BR(0x004b1b05, --field_u(state, 4) != 0)) {
        const int d = kDumpBuffer(state);                                          // 0x004b1b08
        if (!BR(0x004b1b12, d != 0)) return d;
    }
    *reinterpret_cast<unsigned char*>(field_ptr(state, 0)) =
        static_cast<unsigned char>(static_cast<unsigned char>(restart_num) - 0x30);   // 0x004b1b1c
    field_u(state, 0) += 1;
    if (!BR(0x004b1b2d, --field_u(state, 4) != 0)) {
        const int d = kDumpBuffer(state);                                          // 0x004b1b30
        if (!BR(0x004b1b3a, d != 0)) return d;
    }
    if (!BR(0x004b1b4b, field_i(field_ptr(state, 0x20), 0xec) <= 0)) {             // 0x004b1b43
        int ci = 0;
        do {
            field_i(state, 0x10 + ci * 4) = 0;                                     // 0x004b1b50
            ci++;
        } while (BR(0x004b1b63, ci < field_i(field_ptr(state, 0x20), 0xec)));      // 0x004b1b5d
    }
    return 1;
}

// 0x004b1480  encode_mcu_huff(cinfo, MCU_data): copy the destination's next_output_byte / free_in_buffer
// (0x004b1498 / 0x004b14a1) and the entropy coder's saved state (6 dwords from entropy+0xc, 0x004b14b3) into a
// local working_state whose cinfo is set (0x004b14b7). With a restart interval (cinfo+0xc0) and no MCUs left
// (entropy+0x24 == 0, 0x004b14c2) emit_restart(state, entropy+0x28) runs first (0x004b14cd). Each block of the MCU
// (blocks_in_MCU cinfo+0x108 re-read, 0x004b154d) is coded by encode_one_block 0x004b15c0 with its component's
// last_dc_val and derived tables (MCU_membership cinfo+0x10c, cur_comp_info cinfo+0xf0, dc/ac_tbl_no +0x14 /
// +0x18 indexing entropy+0x2c / +0x3c, 0x004b1503..0x004b152f), then last_dc_val takes block[0]
// (0x004b1540 / 0x004b1547). A 0 from either callee returns 0 with nothing written back (0x004b14d9). Otherwise
// the destination and saved state are written back (0x004b1571..0x004b1582) and the restart counter advances:
// reload with restart_interval and next_restart_num = (n + 1) & 7 at 0, then decrement (0x004b158c..0x004b15a2).
typedef int(__cdecl* EncodeMcu_t)(void* cinfo, short** MCU_data);
EncodeMcu_t EncodeMcuHuff_orig;
int __cdecl EncodeMcuHuff_re(void* cinfo, short** MCU_data) {
    char* entropy = reinterpret_cast<char*>(field_ptr(cinfo, 0x164));
    void* dest = field_ptr(cinfo, 0x14);
    unsigned state[9];
    state[0] = field_u(dest, 0);
    state[1] = field_u(dest, 4);
    for (int i = 0; i < 6; i++) state[2 + i] = field_u(entropy, 0xc + i * 4);
    state[8] = reinterpret_cast<unsigned>(cinfo);
    int* last_dc = reinterpret_cast<int*>(&state[4]);
    if (!BR(0x004b14bb, field_i(cinfo, 0xc0) == 0) && !BR(0x004b14c2, field_i(entropy, 0x24) != 0)) {
        if (!BR(0x004b14d7, kEmitRestart(state, field_i(entropy, 0x28)) != 0)) return 0;   // 0x004b14cd
    }
    if (!BR(0x004b14f3, field_i(cinfo, 0x108) <= 0)) {
        int blkn = 0;
        do {
            const int ci = field_i(cinfo, 0x10c + blkn * 4);                       // 0x004b1507
            void* comp = field_ptr(cinfo, 0xf0 + ci * 4);                          // 0x004b1509
            void* dctbl = field_ptr(entropy, 0x2c + field_i(comp, 0x14) * 4);      // 0x004b151e
            void* actbl = field_ptr(entropy, 0x3c + field_i(comp, 0x18) * 4);      // 0x004b151a
            if (BR(0x004b1539, kEncodeOneBlock(state, MCU_data[blkn], last_dc[ci], dctbl, actbl) == 0)) return 0;
            last_dc[ci] = MCU_data[blkn][0];                                       // 0x004b1540 / 0x004b1547
            blkn++;
        } while (BR(0x004b1561, blkn < field_i(cinfo, 0x108)));                    // 0x004b154d
    }
    field_u(field_ptr(cinfo, 0x14), 0) = state[0];                                 // 0x004b1571
    field_u(field_ptr(cinfo, 0x14), 4) = state[1];                                 // 0x004b157a
    for (int i = 0; i < 6; i++) field_u(entropy, 0xc + i * 4) = state[2 + i];      // 0x004b1582
    const int ri = field_i(cinfo, 0xc0);                                           // 0x004b1584
    if (!BR(0x004b158c, ri == 0)) {
        if (!BR(0x004b1593, field_i(entropy, 0x24) != 0)) {
            field_i(entropy, 0x24) = ri;                                           // 0x004b1598
            field_i(entropy, 0x28) = (field_i(entropy, 0x28) + 1) & 7;             // 0x004b159b / 0x004b159f
        }
        field_i(entropy, 0x24) -= 1;                                               // 0x004b15a2
    }
    return 1;
}

// 0x004b1ca0  encode_mcu_gather(cinfo, MCU_data): the statistics pass. With a restart interval (cinfo+0xc0,
// 0x004b1cb5): when no MCUs are left (entropy+0x24 == 0, 0x004b1cbc) every component's last_dc_val
// (entropy+0x14) is cleared (0x004b1ccd, count re-read at 0x004b1cd3) and the counter reloaded from the interval
// (0x004b1ce1 / 0x004b1ce7); then it is decremented (0x004b1cea). Each block (blocks_in_MCU re-read, 0x004b1d50)
// goes to htest_one_block 0x004b1d80 with its component's last_dc_val and the dc / ac count arrays
// (entropy+0x4c / +0x5c indexed by the table numbers, 0x004b1d0f..0x004b1d35), then last_dc_val takes block[0]
// (0x004b1d45 / 0x004b1d48). Returns 1 (0x004b1d6b / 0x004b1d74).
EncodeMcu_t EncodeMcuGather_orig;
int __cdecl EncodeMcuGather_re(void* cinfo, short** MCU_data) {
    char* entropy = reinterpret_cast<char*>(field_ptr(cinfo, 0x164));
    if (!BR(0x004b1cb5, field_i(cinfo, 0xc0) == 0)) {
        if (!BR(0x004b1cbc, field_i(entropy, 0x24) != 0)) {
            if (!BR(0x004b1cc8, field_i(cinfo, 0xec) <= 0)) {
                int ci = 0;
                do {
                    field_i(entropy, 0x14 + ci * 4) = 0;                           // 0x004b1ccd
                    ci++;
                } while (BR(0x004b1cdf, ci < field_i(cinfo, 0xec)));               // 0x004b1cd3
            }
            field_i(entropy, 0x24) = field_i(cinfo, 0xc0);                         // 0x004b1ce1 / 0x004b1ce7
        }
        field_i(entropy, 0x24) -= 1;                                               // 0x004b1cea
    }
    if (!BR(0x004b1cfd, field_i(cinfo, 0x108) <= 0)) {
        int blkn = 0;
        do {
            const int ci = field_i(cinfo, 0x10c + blkn * 4);                       // 0x004b1d13
            void* comp = field_ptr(cinfo, 0xf0 + ci * 4);                          // 0x004b1d15
            int* ac = reinterpret_cast<int*>(field_ptr(entropy, 0x5c + field_i(comp, 0x18) * 4));   // 0x004b1d22
            int* dc = reinterpret_cast<int*>(field_ptr(entropy, 0x4c + field_i(comp, 0x14) * 4));   // 0x004b1d26
            kHtestOneBlock(MCU_data[blkn], field_i(entropy, 0x14 + ci * 4), dc, ac);   // 0x004b1d35
            field_i(entropy, 0x14 + ci * 4) = MCU_data[blkn][0];                   // 0x004b1d48
            blkn++;
        } while (BR(0x004b1d66, blkn < field_i(cinfo, 0x108)));                    // 0x004b1d50
    }
    return 1;
}

// 0x004b1e20  finish_pass_gather(cinfo): after the statistics pass, build each table the scan used once. Two
// four-entry flag arrays did_dc / did_ac start cleared (0x004b1e32..0x004b1e5a). Per component (count re-read at
// 0x004b1f06): a DC table number not done yet (0x004b1e89) gets a JHUFF_TBL from jpeg_alloc_huff_table
// 0x004afad0 when its slot cinfo+0x50 is NULL (0x004b1e91 / 0x004b1e96), then jpeg_gen_optimal_table 0x004b1080
// with the entropy coder's dc count array (+0x4c, 0x004b1eaa / 0x004b1eb1), and is marked done (0x004b1eb9);
// the AC table the same with cinfo+0x60 and +0x5c (0x004b1ec8..0x004b1ef8).
VoidCinfo_t FinishPassGather_orig;
void __cdecl FinishPassGather_re(void* cinfo) {
    char* entropy = reinterpret_cast<char*>(field_ptr(cinfo, 0x164));
    int did_dc[4] = {0, 0, 0, 0};
    int did_ac[4] = {0, 0, 0, 0};
    if (BR(0x004b1e62, field_i(cinfo, 0xec) <= 0)) return;
    int ci = 0;
    do {
        void* comp = field_ptr(cinfo, 0xf0 + ci * 4);                              // 0x004b1e79
        const int dctbl = field_i(comp, 0x14);                                     // 0x004b1e7b
        const int actbl = field_i(comp, 0x18);                                     // 0x004b1e7e
        if (!BR(0x004b1e8b, did_dc[dctbl] != 0)) {
            if (!BR(0x004b1e93, field_ptr(cinfo, 0x50 + dctbl * 4) != nullptr))
                *reinterpret_cast<void**>(reinterpret_cast<char*>(cinfo) + 0x50 + dctbl * 4) = kAllocHuffTable(cinfo);
            kGenOptimalTable(cinfo, field_ptr(cinfo, 0x50 + dctbl * 4),
                             reinterpret_cast<int*>(field_ptr(entropy, 0x4c + dctbl * 4)));
            did_dc[dctbl] = 1;
        }
        if (!BR(0x004b1eca, did_ac[actbl] != 0)) {
            if (!BR(0x004b1ed2, field_ptr(cinfo, 0x60 + actbl * 4) != nullptr))
                *reinterpret_cast<void**>(reinterpret_cast<char*>(cinfo) + 0x60 + actbl * 4) = kAllocHuffTable(cinfo);
            kGenOptimalTable(cinfo, field_ptr(cinfo, 0x60 + actbl * 4),
                             reinterpret_cast<int*>(field_ptr(entropy, 0x5c + actbl * 4)));
            did_ac[actbl] = 1;
        }
        ci++;
    } while (BR(0x004b1f1a, ci < field_i(cinfo, 0xec)));                           // 0x004b1f06
}

// 0x004b12f0  start_pass_huff(cinfo, gather_statistics): install encode_mcu / finish_pass in the entropy coder
// (entropy+4 / +8): encode_mcu_gather 0x004b1ca0 and finish_pass_gather 0x004b1e20 when gathering (0x004b1309 /
// 0x004b1310), else encode_mcu_huff 0x004b1480 and 0x004b1c20 (0x004b1319 / 0x004b1320). Per component (count
// re-read at 0x004b1448): a DC table number outside 0..3 or naming a NULL cinfo+0x50 slot while not gathering
// raises ERREXIT1 0x31 with the number (0x004b1358..0x004b137e), the AC number likewise with cinfo+0x60
// (0x004b1383..0x004b13ad). Gathering, each count array (entropy+0x4c / +0x5c) is allocated when NULL through
// cinfo->mem->alloc_small(cinfo, 1, 0x404) (0x004b13c2..0x004b13d2, 0x004b13eb..0x004b13fb) and zeroed (0x404 bytes,
// 0x004b13e1 / 0x004b140a); otherwise both derived tables (entropy+0x2c / +0x3c) are built by
// jpeg_make_c_derived_tbl 0x004b0f60 (0x004b1419 / 0x004b1429). last_dc_val[ci] (entropy+0x14) is cleared
// (0x004b143f). Finally put_buffer, put_bits and next_restart_num are cleared and restarts_to_go takes
// restart_interval (0x004b1464..0x004b1473).
typedef void(__cdecl* StartPass_t)(void* cinfo, int gather);
StartPass_t StartPassHuff_orig;
void __cdecl StartPassHuff_re(void* cinfo, int gather) {
    char* entropy = reinterpret_cast<char*>(field_ptr(cinfo, 0x164));
    if (!BR(0x004b1307, gather == 0)) {
        field_u(entropy, 4) = 0x004b1ca0;                                          // 0x004b1309
        field_u(entropy, 8) = 0x004b1e20;                                          // 0x004b1310
    } else {
        field_u(entropy, 4) = 0x004b1480;                                          // 0x004b1319
        field_u(entropy, 8) = 0x004b1c20;                                          // 0x004b1320
    }
    if (!BR(0x004b1333, field_i(cinfo, 0xec) <= 0)) {
        int ci = 0;
        do {
            void* comp = field_ptr(cinfo, 0xf0 + ci * 4);                              // 0x004b1350
            const int dctbl = field_i(comp, 0x14);                                     // 0x004b1352
            const int actbl = field_i(comp, 0x18);                                     // 0x004b1355
            if (BR(0x004b135a, dctbl < 0) || BR(0x004b135f, dctbl >= 4) ||
                (!BR(0x004b1365, field_ptr(cinfo, 0x50 + dctbl * 4) != nullptr) && !BR(0x004b136b, gather != 0)))
                errexit1(cinfo, 0x31, dctbl);                                          // 0x004b136d..0x004b137e
            if (BR(0x004b1385, actbl < 0) || BR(0x004b138a, actbl >= 4) ||
                (!BR(0x004b1392, field_ptr(cinfo, 0x60 + actbl * 4) != nullptr) && !BR(0x004b139a, gather != 0)))
                errexit1(cinfo, 0x31, actbl);                                          // 0x004b139c..0x004b13ad
            if (!BR(0x004b13b8, gather == 0)) {
                AllocSmall_t alloc;
                if (!BR(0x004b13c0, field_ptr(entropy, 0x4c + dctbl * 4) != nullptr)) {
                    alloc = reinterpret_cast<AllocSmall_t>(field_ptr(field_ptr(cinfo, 4), 0));
                    *reinterpret_cast<void**>(entropy + 0x4c + dctbl * 4) = alloc(cinfo, 1, 0x404);
                }
                unsigned* dc = reinterpret_cast<unsigned*>(field_ptr(entropy, 0x4c + dctbl * 4));
                for (int i = 0; i < 0x101; i++) dc[i] = 0;
                if (!BR(0x004b13e9, field_ptr(entropy, 0x5c + actbl * 4) != nullptr)) {
                    alloc = reinterpret_cast<AllocSmall_t>(field_ptr(field_ptr(cinfo, 4), 0));
                    *reinterpret_cast<void**>(entropy + 0x5c + actbl * 4) = alloc(cinfo, 1, 0x404);
                }
                unsigned* ac = reinterpret_cast<unsigned*>(field_ptr(entropy, 0x5c + actbl * 4));
                for (int i = 0; i < 0x101; i++) ac[i] = 0;
            } else {
                kMakeDerivedTbl(cinfo, field_ptr(cinfo, 0x50 + dctbl * 4),
                                reinterpret_cast<void**>(entropy + 0x2c + dctbl * 4));
                kMakeDerivedTbl(cinfo, field_ptr(cinfo, 0x60 + actbl * 4),
                                reinterpret_cast<void**>(entropy + 0x3c + actbl * 4));
            }
            field_i(entropy, 0x14 + ci * 4) = 0;
            ci++;
        } while (BR(0x004b145c, ci < field_i(cinfo, 0xec)));                        // 0x004b1448
    }
    field_i(entropy, 0xc) = 0;
    field_i(entropy, 0x10) = 0;
    field_i(entropy, 0x28) = 0;
    field_i(entropy, 0x24) = field_i(cinfo, 0xc0);
}

// 0x004b3d00  expand_bottom_edge(image_data, num_cols, input_rows, output_rows): replicate the last input row
// into every row from input_rows up to output_rows - 1 (signed bounds, 0x004b3d0a / 0x004b3d2b), one
// jcopy_sample_rows 0x004b04f0 call per row: (image_data, input_rows - 1, image_data, row, 1, num_cols)
// (0x004b3d14..0x004b3d22).
typedef void(__cdecl* ExpandBottom_t)(void* image_data, unsigned num_cols, int input_rows, int output_rows);
ExpandBottom_t ExpandBottomEdge_orig;
void __cdecl ExpandBottomEdge_re(void* image_data, unsigned num_cols, int input_rows, int output_rows) {
    if (BR(0x004b3d0c, input_rows >= output_rows)) return;
    int row = input_rows;
    do {
        kCopySampleRows(image_data, input_rows - 1, image_data, row, 1, num_cols);   // 0x004b3d22
        row++;                                                                     // 0x004b3d2a
    } while (BR(0x004b3d2d, row < output_rows));
}

// do_sarray_io / do_barray_io(cinfo, ptr, writing): move the rows in memory to or from backing store, one
// call per chunk. bytesperrow is samplesperrow (+8) for samples (0x004af69a) or blocksperrow << 7 for blocks
// (0x004af890); file_offset starts at cur_start_row * bytesperrow (0x004af6a3 / 0x004af899). For i from 0 by
// rowsperchunk (+0x14) while below rows_in_mem (+0x10) (0x004af6a6, 0x004af71a..0x004af723): rows =
// min(rowsperchunk, rows_in_mem - i, first_undef_row (+0x1c) - (cur_start_row + i), rows_in_array (+4) -
// (cur_start_row + i)), signed (0x004af6b4..0x004af6d8); rows <= 0 ends (0x004af6dc). byte_count = rows *
// bytesperrow (0x004af6de); write_backing_store (+0x34, 0x004af6fc) or read_backing_store (+0x30, 0x004af712) is
// called with (cinfo, &b_s_info (+0x30), mem_buffer[i], file_offset, byte_count), then file_offset advances.
// The jcc addresses of each twin, in the order: no rows in memory, rowsperchunk >= rest, rows < undefined limit,
// rows < array limit, no row, reading, loop back.
struct IoJcc { unsigned none, chunk, undef, array, empty, read, again; };
const IoJcc kSarrayIoJcc = {0x004af6ac, 0x004af6bb, 0x004af6cb, 0x004af6d6, 0x004af6dc, 0x004af6e9, 0x004af723};
const IoJcc kBarrayIoJcc = {0x004af89f, 0x004af8ae, 0x004af8c0, 0x004af8cb, 0x004af8d1, 0x004af8de, 0x004af918};
static void do_array_io(void* cinfo, char* ptr, int writing, int bytesperrow, const IoJcc& at) {
    int file_offset = static_cast<int>(static_cast<unsigned>(bytesperrow) * field_u(ptr, 0x18));
    if (BR(at.none, field_i(ptr, 0x10) <= 0)) return;
    int i = 0;
    do {
        int rows = field_i(ptr, 0x10) - i;
        if (!BR(at.chunk, field_i(ptr, 0x14) >= rows)) rows = field_i(ptr, 0x14);
        const int thisrow = field_i(ptr, 0x18) + i;
        if (!BR(at.undef, rows < field_i(ptr, 0x1c) - thisrow)) rows = field_i(ptr, 0x1c) - thisrow;
        if (!BR(at.array, rows < field_i(ptr, 4) - thisrow)) rows = field_i(ptr, 4) - thisrow;
        if (BR(at.empty, rows <= 0)) return;
        const int byte_count = rows * bytesperrow;
        void* buf = reinterpret_cast<void**>(field_ptr(ptr, 0))[i];
        BackingIo_t io = reinterpret_cast<BackingIo_t>(field_ptr(ptr, BR(at.read, writing == 0) ? 0x30 : 0x34));
        io(cinfo, ptr + 0x30, buf, file_offset, byte_count);
        file_offset += byte_count;
        i += field_i(ptr, 0x14);
    } while (BR(at.again, i < field_i(ptr, 0x10)));
}

// 0x004af690  do_sarray_io
VirtIo_t DoSarrayIo_orig;
void __cdecl DoSarrayIo_re(void* cinfo, void* ptr, int writing) {
    char* p = reinterpret_cast<char*>(ptr);
    do_array_io(cinfo, p, writing, field_i(p, 8), kSarrayIoJcc);                   // 0x004af69a
}

// 0x004af880  do_barray_io
VirtIo_t DoBarrayIo_orig;
void __cdecl DoBarrayIo_re(void* cinfo, void* ptr, int writing) {
    char* p = reinterpret_cast<char*>(ptr);
    do_array_io(cinfo, p, writing, static_cast<int>(field_u(p, 8) << 7), kBarrayIoJcc);   // 0x004af88a / 0x004af890
}

// access_virt_sarray / access_virt_barray(cinfo, ptr, start_row, num_rows, writable): return the row pointer for
// start_row of a virtual array, swapping the in-memory window when needed (all row compares unsigned).
// end_row = start_row + num_rows (0x004af557). end_row above rows_in_array (+4), num_rows above maxaccess (+0xc)
// or a NULL mem_buffer (+0) raise ERREXIT 0x14 (0x004af55a..0x004af576). A start_row below cur_start_row (+0x18) or
// an end_row past cur_start_row + rows_in_mem (+0x10) (0x004af57e..0x004af589) moves the window: ERREXIT 0x44
// without b_s_open (+0x28, 0x004af590), a dirty (+0x24) window written back with do_*_io(..., 1) and dirty cleared
// (0x004af5aa..0x004af5b8), cur_start_row = start_row when start_row is above it (0x004af5c2 / 0x004af5c4) else
// max(end_row - rows_in_mem, 0) (signed, 0x004af5c9..0x004af5d4), and the window read with do_*_io(..., 0)
// (0x004af5db). If first_undef_row (+0x1c) is below end_row (0x004af5e8): a start_row past it raises ERREXIT 0x14
// for a writer (0x004af5ea..0x004af606) and moves undef_row up to start_row; a writer sets first_undef_row =
// end_row (0x004af617); with pre_zero (+0x20) the rows undef_row..end_row-1 of the window are zeroed by jzero_far
// 0x004b0540 (row size samplesperrow, or blocksperrow << 7: 0x004af621..0x004af641), without it a reader raises
// ERREXIT 0x14 and returns at once (0x004af665..0x004af68e). A writer sets dirty (0x004af64b); the result is
// mem_buffer + (start_row - cur_start_row) (0x004af652..0x004af660).
typedef void**(__cdecl* AccessVirt_t)(void* cinfo, void* ptr, unsigned start_row, unsigned num_rows, int writable);
// The jcc addresses of each twin, in source order (see the two jcc lists of 0x004af540 and 0x004af730).
struct AvJcc {
    unsigned endBad, numBad, memOk, startBelow, endInside, bsOpen, notDirty, startNotAbove, ltempPos;
    unsigned undefDefined, undefNotBelow, noWriter1, noWriter2, noPreZero, zeroDone, zeroAgain, noWriter3, writerLate;
};
const AvJcc kSarrayJcc = {0x004af55c, 0x004af561, 0x004af566, 0x004af580, 0x004af589, 0x004af590, 0x004af5aa,
                          0x004af5c2, 0x004af5d0, 0x004af5e8, 0x004af5ee, 0x004af5f6, 0x004af615, 0x004af61f,
                          0x004af62d, 0x004af641, 0x004af649, 0x004af667};
const AvJcc kBarrayJcc = {0x004af74c, 0x004af751, 0x004af756, 0x004af76e, 0x004af777, 0x004af77e, 0x004af796,
                          0x004af7ae, 0x004af7bc, 0x004af7d4, 0x004af7da, 0x004af7e2, 0x004af7ff, 0x004af809,
                          0x004af81a, 0x004af82e, 0x004af836, 0x004af854};
static void** access_virt(void* cinfo, char* ptr, unsigned start_row, unsigned num_rows, int writable,
                          VirtIo_t io, unsigned rowshift, const AvJcc& at) {
    unsigned end_row = start_row + num_rows;
    if (BR(at.endBad, end_row > field_u(ptr, 4)) || BR(at.numBad, num_rows > field_u(ptr, 0xc)) ||
        !BR(at.memOk, field_ptr(ptr, 0) != nullptr))
        errexit0(cinfo, 0x14);
    if (BR(at.startBelow, start_row < field_u(ptr, 0x18)) ||
        !BR(at.endInside, end_row <= field_u(ptr, 0x18) + field_u(ptr, 0x10))) {
        if (!BR(at.bsOpen, field_i(ptr, 0x28) != 0)) errexit0(cinfo, 0x44);
        if (!BR(at.notDirty, field_i(ptr, 0x24) == 0)) {
            io(cinfo, ptr, 1);
            field_i(ptr, 0x24) = 0;
        }
        if (!BR(at.startNotAbove, start_row <= field_u(ptr, 0x18))) {
            field_u(ptr, 0x18) = start_row;
        } else {
            int ltemp = static_cast<int>(end_row - field_u(ptr, 0x10));
            if (!BR(at.ltempPos, ltemp >= 0)) ltemp = 0;
            field_i(ptr, 0x18) = ltemp;
        }
        io(cinfo, ptr, 0);
    }
    unsigned undef_row = field_u(ptr, 0x1c);
    if (!BR(at.undefDefined, undef_row >= end_row)) {
        if (!BR(at.undefNotBelow, undef_row >= start_row)) {
            if (!BR(at.noWriter1, writable == 0)) errexit0(cinfo, 0x14);
            undef_row = start_row;
        }
        if (!BR(at.noWriter2, writable == 0)) field_u(ptr, 0x1c) = end_row;
        if (!BR(at.noPreZero, field_i(ptr, 0x20) == 0)) {
            const unsigned cur = field_u(ptr, 0x18);
            const unsigned bytesperrow = field_u(ptr, 8) << rowshift;
            undef_row -= cur;
            end_row -= cur;
            if (!BR(at.zeroDone, undef_row >= end_row)) {
                do {
                    kJzeroFar(reinterpret_cast<void**>(field_ptr(ptr, 0))[undef_row], bytesperrow);
                    undef_row++;
                } while (BR(at.zeroAgain, undef_row < end_row));
            }
        } else if (!BR(at.writerLate, writable != 0)) {
            errexit0(cinfo, 0x14);
            return reinterpret_cast<void**>(field_ptr(ptr, 0)) + (start_row - field_u(ptr, 0x18));
        }
    }
    if (!BR(at.noWriter3, writable == 0)) field_i(ptr, 0x24) = 1;
    return reinterpret_cast<void**>(field_ptr(ptr, 0)) + (start_row - field_u(ptr, 0x18));
}

// 0x004af540  access_virt_sarray
AccessVirt_t AccessVirtSarray_orig;
void** __cdecl AccessVirtSarray_re(void* cinfo, void* ptr, unsigned start_row, unsigned num_rows, int writable) {
    return access_virt(cinfo, reinterpret_cast<char*>(ptr), start_row, num_rows, writable, kDoSarrayIo, 0, kSarrayJcc);
}

// 0x004af730  access_virt_barray
AccessVirt_t AccessVirtBarray_orig;
void** __cdecl AccessVirtBarray_re(void* cinfo, void* ptr, unsigned start_row, unsigned num_rows, int writable) {
    return access_virt(cinfo, reinterpret_cast<char*>(ptr), start_row, num_rows, writable, kDoBarrayIo, 7, kBarrayJcc);
}

}  // namespace

// 0x004b0540  jzero_far
SG_HOOK("golf_clean.exe", 0x004b0540, jzero_far, JzeroFar_re, JzeroFar_orig);
// 0x004a48f0  listGetAt
SG_HOOK("golf_clean.exe", 0x004a48f0, listGetAt, ListGetAt_re, ListGetAt_orig);
// 0x004b1d80  htest_one_block
SG_HOOK("golf_clean.exe", 0x004b1d80, htest_one_block, HtestOneBlock_re, HtestOneBlock_orig);
// 0x004b0f60  jpeg_make_c_derived_tbl
SG_HOOK("golf_clean.exe", 0x004b0f60, jpeg_make_c_derived_tbl, MakeDerivedTbl_re, MakeDerivedTbl_orig);
// 0x004b1080  jpeg_gen_optimal_table
SG_HOOK("golf_clean.exe", 0x004b1080, jpeg_gen_optimal_table, GenOptimalTable_re, GenOptimalTable_orig);
// 0x004b01f0  emit_dri
SG_HOOK("golf_clean.exe", 0x004b01f0, emit_dri, EmitDri_re, EmitDri_orig);
// 0x004b0220  emit_sos
SG_HOOK("golf_clean.exe", 0x004b0220, emit_sos, EmitSos_re, EmitSos_orig);
// 0x004b0060  write_scan_header
SG_HOOK("golf_clean.exe", 0x004b0060, write_scan_header, WriteScanHeader_re, WriteScanHeader_orig);
// 0x004b15c0  encode_one_block
SG_HOOK("golf_clean.exe", 0x004b15c0, encode_one_block, EncodeOneBlock_re, EncodeOneBlock_orig);
// 0x004b1ae0  emit_restart
SG_HOOK("golf_clean.exe", 0x004b1ae0, emit_restart, EmitRestart_re, EmitRestart_orig);
// 0x004b1480  encode_mcu_huff
SG_HOOK("golf_clean.exe", 0x004b1480, encode_mcu_huff, EncodeMcuHuff_re, EncodeMcuHuff_orig);
// 0x004b1ca0  encode_mcu_gather
SG_HOOK("golf_clean.exe", 0x004b1ca0, encode_mcu_gather, EncodeMcuGather_re, EncodeMcuGather_orig);
// 0x004b1e20  finish_pass_gather
SG_HOOK("golf_clean.exe", 0x004b1e20, finish_pass_gather, FinishPassGather_re, FinishPassGather_orig);
// 0x004b12f0  start_pass_huff
SG_HOOK("golf_clean.exe", 0x004b12f0, start_pass_huff, StartPassHuff_re, StartPassHuff_orig);
// 0x004b3d00  expand_bottom_edge
SG_HOOK("golf_clean.exe", 0x004b3d00, expand_bottom_edge, ExpandBottomEdge_re, ExpandBottomEdge_orig);
// 0x004af690  do_sarray_io
SG_HOOK("golf_clean.exe", 0x004af690, do_sarray_io, DoSarrayIo_re, DoSarrayIo_orig);
// 0x004af880  do_barray_io
SG_HOOK("golf_clean.exe", 0x004af880, do_barray_io, DoBarrayIo_re, DoBarrayIo_orig);
// 0x004af540  access_virt_sarray
SG_HOOK("golf_clean.exe", 0x004af540, access_virt_sarray, AccessVirtSarray_re, AccessVirtSarray_orig);
// 0x004af730  access_virt_barray
SG_HOOK("golf_clean.exe", 0x004af730, access_virt_barray, AccessVirtBarray_re, AccessVirtBarray_orig);

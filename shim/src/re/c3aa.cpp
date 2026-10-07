// Batch c3aa: IJG libjpeg 6a compress-side helpers of golf_clean.exe (subsystem util).
// All hand-written from the decompilations (py -3.12 re/tools/decomp.py <addr>) and the matched sources
// (re/match/golf_jpeg_*.cpp), citing the offset each field comes from. Callees are called through their
// original addresses. The bit/byte-emit helpers only touch the fixture's output buffer; their
// buffer-full "suspend" branch (dump_buffer / empty_output_buffer) and their error branches (ERREXIT)
// are kept faithfully but are not reached by the A/B vectors (documented in log/c3/c3aa_purpose.md).
#include "hooks.h"

namespace {

template <typename T> static T& F(void* p, int off) { return *reinterpret_cast<T*>(reinterpret_cast<unsigned char*>(p) + off); }

// Callees (called through their golf_clean.exe addresses; a hooked one would run its own reimplementation).
typedef int (__cdecl* DumpBuffer_t)(void*);
typedef void (__cdecl* EmitByte_t)(void*, int);
typedef int (__cdecl* AllocQuant_t)(void*);
static const DumpBuffer_t kDumpBuffer      = reinterpret_cast<DumpBuffer_t>(0x004b19e0);  // dump_buffer (jchuff)
static const DumpBuffer_t kDumpBuffer2420  = reinterpret_cast<DumpBuffer_t>(0x004b2420);  // dump_buffer_4b2420 (jcphuff)
static const EmitByte_t   kEmitByte        = reinterpret_cast<EmitByte_t>(0x004afbb0);    // emit_byte
static const EmitByte_t   kEmitMarker      = reinterpret_cast<EmitByte_t>(0x004afbf0);    // emit_marker
static const EmitByte_t   kEmit2bytes      = reinterpret_cast<EmitByte_t>(0x004afc10);    // emit_2bytes
static const AllocQuant_t kAllocQuantTable = reinterpret_cast<AllocQuant_t>(0x004afab0);  // jpeg_alloc_quant_table

// jpeg_natural_order[] (zigzag), const image data at 0x004bd0ac..0x004bd1ab (64 dwords); read by emit_dqt.
static const int* const kNaturalOrder = reinterpret_cast<const int*>(0x004bd0ac);

// --- jchuff working_state: +0 next_output_byte, +4 free_in_buffer, +8 put_buffer, +0xc put_bits, +0x20 cinfo.

// 0x004b1a10  emit_bits(working_state* state, unsigned code, int size)
// Accumulate `size` low bits of `code` into state->cur.put_buffer/put_bits (+0x8/+0xc), flushing whole bytes
// (and a stuffed 0x00 after any 0xFF) to *next_output_byte (+0x0). Returns 1.
DumpBuffer_t EmitBits_orig;
int __cdecl EmitBits_re(void* state, unsigned code, int size) {
    int put_bits = F<int>(state, 0xc);                                    // state->cur.put_bits
    if (size == 0) {                                                      // 0x004b1a1c invalid Huffman entry -> ERREXIT
        void* cinfo = F<void*>(state, 0x20);
        F<int>(F<void*>(cinfo, 0), 0x14) = 0x27;                          // cinfo->err->msg_code = 0x27
        (*F<void (__cdecl**)(void*)>(F<void*>(cinfo, 0), 0))(cinfo);      // (*cinfo->err->error_exit)(cinfo)
    }
    put_bits += size;                                                     // 0x004b1a37
    unsigned put_buffer = (((1u << (size & 0x1f)) - 1u) & code) << ((24 - put_bits) & 0x1f)
                          | F<unsigned>(state, 0x8);                      // mask, align, merge put_buffer
    while (put_bits >= 8) {                                               // 0x004b1a4f
        unsigned char c = static_cast<unsigned char>(put_buffer >> 16);
        unsigned char* np = F<unsigned char*>(state, 0x0);
        *np = c;
        int fib = F<int>(state, 0x4);
        F<unsigned char*>(state, 0x0) = np + 1;
        F<int>(state, 0x4) = fib - 1;                                     // --free_in_buffer
        if (fib - 1 == 0 && kDumpBuffer(state) == 0) return 0;            // buffer full -> dump (not reached)
        if (c == 0xFF) {                                                  // stuff a zero byte
            np = F<unsigned char*>(state, 0x0);
            *np = 0;
            fib = F<int>(state, 0x4);
            F<unsigned char*>(state, 0x0) = np + 1;
            F<int>(state, 0x4) = fib - 1;
            if (fib - 1 == 0 && kDumpBuffer(state) == 0) return 0;
        }
        put_bits -= 8;
        put_buffer <<= 8;
    }
    F<unsigned>(state, 0x8) = put_buffer;                                 // store put_buffer
    F<int>(state, 0xc) = put_bits;                                        // store put_bits
    return 1;
}

// --- jcphuff phuff_entropy_encoder: +0xc gather_statistics, +0x10 next_output_byte, +0x14 free_in_buffer,
//     +0x18 put_buffer, +0x1c put_bits, +0x20 cinfo. emit_bits is inlined into both functions below.

static void phuffEmitByte(void* ent, unsigned char v) {                  // emit_byte macro of jcphuff
    unsigned char* np = F<unsigned char*>(ent, 0x10);
    *np = v;
    int fib = F<int>(ent, 0x14);
    F<unsigned char*>(ent, 0x10) = np + 1;
    F<int>(ent, 0x14) = fib - 1;
    if (fib - 1 == 0) kDumpBuffer2420(ent);                              // buffer full -> dump (not reached)
}

// 0x004b2510  flush_bits(phuff_entropy_ptr entropy)
// Fill the partial byte with ones (emit_bits(entropy, 0x7F, 7) inlined) then reset put_buffer/put_bits to 0.
// When gathering statistics (+0xc != 0) it only resets the two fields.
DumpBuffer_t FlushBits4b2510_orig;
void __cdecl FlushBits4b2510_re(void* entropy) {
    if (F<int>(entropy, 0xc) != 0) {                                     // 0x004b2518 gather_statistics
        F<unsigned>(entropy, 0x18) = 0;
        F<int>(entropy, 0x1c) = 0;
        return;
    }
    unsigned bits = static_cast<unsigned>(F<int>(entropy, 0x1c)) + 7;    // put_bits + 7
    unsigned buf = (0x7fu << ((24 - bits) & 0x1f)) | F<unsigned>(entropy, 0x18);
    if (static_cast<int>(bits) >= 8) {
        unsigned nbytes = bits >> 3;
        bits -= nbytes * 8;
        do {
            unsigned char c = static_cast<unsigned char>(buf >> 16);
            phuffEmitByte(entropy, c);
            if (c == 0xFF) phuffEmitByte(entropy, 0);
            buf <<= 8;
            --nbytes;
        } while (nbytes != 0);
    }
    F<int>(entropy, 0x1c) = static_cast<int>(bits);                      // emit_bits stores put_bits/put_buffer,
    F<unsigned>(entropy, 0x18) = buf;                                    // then flush_bits overwrites both with 0:
    F<unsigned>(entropy, 0x18) = 0;
    F<int>(entropy, 0x1c) = 0;
}

// 0x004b27d0  emit_buffered_bits(phuff_entropy_ptr entropy, char* bufstart, unsigned nbits)
// Emit `nbits` correction bits, the low bit of each bufstart[k], one bit at a time (emit_bits(.., 1) inlined).
// No-op while gathering statistics (+0xc != 0) or when nbits == 0.
DumpBuffer_t EmitBufferedBits_orig;
void __cdecl EmitBufferedBits_re(void* entropy, unsigned char* bufstart, unsigned nbits) {
    if (F<int>(entropy, 0xc) != 0 || nbits == 0) return;                 // 0x004b27d6 / 0x004b27e7
    unsigned left = nbits;
    do {
        if (F<int>(entropy, 0xc) == 0) {                                 // gather re-checked per bit
            unsigned bits = static_cast<unsigned>(F<int>(entropy, 0x1c)) + 1;
            unsigned buf = ((*bufstart & 1u) << ((24 - bits) & 0x1f)) | F<unsigned>(entropy, 0x18);
            if (static_cast<int>(bits) >= 8) {
                unsigned nbytes = bits >> 3;
                bits -= nbytes * 8;
                do {
                    unsigned char c = static_cast<unsigned char>(buf >> 16);
                    phuffEmitByte(entropy, c);
                    if (c == 0xFF) phuffEmitByte(entropy, 0);
                    buf <<= 8;
                    --nbytes;
                } while (nbytes != 0);
            }
            F<unsigned>(entropy, 0x18) = buf;
            F<int>(entropy, 0x1c) = static_cast<int>(bits);
        }
        ++bufstart;
        --left;
    } while (left != 0);
}

// --- jpeg_compress_struct (offsets from the decompiles): +0x10 global_state, +0x14 dest, +0x18 image_width,
//     +0x1c image_height, +0x30 data_precision, +0x34 num_components, +0x3c comp_info, +0x40 quant_tbl_ptrs[],
//     +0x50 dc_huff_tbl_ptrs[], +0x60 ac_huff_tbl_ptrs[], +0xa4 scan_info, +0xec comps_in_scan, +0xf0
//     cur_comp_info[], +0x134 Ss, +0x138 Se, +0x13c Ah, +0x140 Al, +0x144 master.

// 0x004ae600  jpeg_add_quant_table(cinfo, which_tbl, unsigned* basic, int scale, int force_baseline)
// Scale each of the 64 entries of basic[] by scale/100 (rounded), clamp to [1, 0x7fff] (and to 0xFF when
// force_baseline), store into quant_tbl_ptrs[which_tbl]->quantval[], then clear its sent_table (+0x80).
AllocQuant_t JpegAddQuantTable_orig;
void __cdecl JpegAddQuantTable_re(void* cinfo, int which_tbl, int* basic, int scale, int force) {
    if (F<int>(cinfo, 0x10) != 100) {                                    // 0x004ae60a global_state != CSTATE_START
        F<int>(F<void*>(cinfo, 0), 0x14) = 0x12;                         // cinfo->err->msg_code = 0x12
        F<int>(F<void*>(cinfo, 0), 0x18) = F<int>(cinfo, 0x10);
        (*F<void (__cdecl**)(void*)>(F<void*>(cinfo, 0), 0))(cinfo);
    }
    if (F<int>(cinfo, 0x40 + which_tbl * 4) == 0)                        // quant_tbl_ptrs[which_tbl] == NULL -> alloc (not reached)
        F<int>(cinfo, 0x40 + which_tbl * 4) = kAllocQuantTable(cinfo);
    short* qv = reinterpret_cast<short*>(F<int>(cinfo, 0x40 + which_tbl * 4));
    for (int i = 0; i < 64; ++i) {
        int temp = (scale * basic[i] + 50) / 100;                        // 0x004ae63a (int arithmetic)
        if (temp < 1) temp = 1;
        else if (temp > 0x7fff) temp = 0x7fff;
        if (force != 0 && temp > 0xff) temp = 0xff;
        qv[i] = static_cast<short>(temp);
    }
    *reinterpret_cast<int*>(reinterpret_cast<unsigned char*>(qv) + 0x80) = 0;  // sent_table = FALSE
}

// 0x004afec0  emit_dqt(cinfo, index): write the DQT marker for quant_tbl_ptrs[index] if not already sent.
// Returns the precision (1 if any quantval > 255, else 0). Emits the 64 values in zigzag order.
EmitByte_t EmitDqt_orig;
int __cdecl EmitDqt_re(void* cinfo, int index) {
    unsigned short* qtbl = reinterpret_cast<unsigned short*>(F<int>(cinfo, 0x40 + index * 4));  // quant_tbl_ptrs[index]
    if (qtbl == 0) {                                                     // 0x004afecd NULL -> ERREXIT (not reached)
        F<int>(F<void*>(cinfo, 0), 0x14) = 0x33;
        F<int>(F<void*>(cinfo, 0), 0x18) = index;
        (*F<void (__cdecl**)(void*)>(F<void*>(cinfo, 0), 0))(cinfo);
    }
    char prec = 0;
    for (int i = 0; i < 64; ++i) if (qtbl[i] > 0xff) prec = 1;           // 0x004afef9
    if (*reinterpret_cast<int*>(reinterpret_cast<unsigned char*>(qtbl) + 0x80) == 0) {  // sent_table
        kEmitMarker(cinfo, 0xdb);
        kEmit2bytes(cinfo, (prec ? 0x40 : 0) + 0x43);                    // length: 0x83 (16-bit) or 0x43 (8-bit)
        kEmitByte(cinfo, (prec << 4) + static_cast<char>(index));
        for (int i = 0; i < 64; ++i) {                                   // kNaturalOrder[0..63], 0x004bd0ac
            unsigned short qval = qtbl[kNaturalOrder[i]];
            if (prec) kEmitByte(cinfo, static_cast<char>(qval >> 8));
            kEmitByte(cinfo, static_cast<char>(qval));
        }
        qtbl[0x40] = 1;                                                  // sent_table word (+0x80)
        qtbl[0x41] = 0;                                                  // (+0x82)
    }
    return prec;
}

// 0x004affa0  emit_sof(cinfo, code): write the SOF marker (frame header): precision, height, width, component
// count, then per component the id, sampling factors and quant-table selector.
EmitByte_t EmitSof_orig;
void __cdecl EmitSof_re(void* cinfo, int code) {
    kEmitMarker(cinfo, code);
    kEmit2bytes(cinfo, F<int>(cinfo, 0x34) * 3 + 8);                     // 3*num_components + 2 + 5 + 1
    if (F<int>(cinfo, 0x1c) > 0xffff || F<int>(cinfo, 0x18) > 0xffff) {  // image too big -> ERREXIT (not reached)
        F<int>(F<void*>(cinfo, 0), 0x14) = 0x28;
        F<int>(F<void*>(cinfo, 0), 0x18) = 0xffff;
        (*F<void (__cdecl**)(void*)>(F<void*>(cinfo, 0), 0))(cinfo);
    }
    kEmitByte(cinfo, F<int>(cinfo, 0x30));                               // data_precision
    kEmit2bytes(cinfo, F<int>(cinfo, 0x1c));                            // image_height
    kEmit2bytes(cinfo, F<int>(cinfo, 0x18));                            // image_width
    kEmitByte(cinfo, F<int>(cinfo, 0x34));                               // num_components
    int* comp = reinterpret_cast<int*>(F<int>(cinfo, 0x3c));             // comp_info
    for (int i = 0; i < F<int>(cinfo, 0x34); ++i) {
        kEmitByte(cinfo, comp[0]);                                       // component_id
        kEmitByte(cinfo, comp[2] * 0x10 + comp[3]);                      // (h_samp_factor << 4) + v_samp_factor
        kEmitByte(cinfo, comp[4]);                                       // quant_tbl_no
        comp += 0x15;                                                    // sizeof(jpeg_component_info) = 0x54
    }
}

// 0x004b0120  emit_dht(cinfo, index, is_ac): write the DHT marker for one Huffman table (DC if is_ac==0, else
// AC) unless already sent. The table byte count is sum(bits[1..16]); emits bits[1..16] then that many huffval.
EmitByte_t EmitDht_orig;
void __cdecl EmitDht_re(void* cinfo, int index, int is_ac) {
    unsigned char* htbl;
    if (is_ac == 0) {
        htbl = reinterpret_cast<unsigned char*>(F<int>(cinfo, 0x50 + index * 4));  // dc_huff_tbl_ptrs[index]
    } else {
        htbl = reinterpret_cast<unsigned char*>(F<int>(cinfo, 0x60 + index * 4));  // ac_huff_tbl_ptrs[index]
        index += 0x10;
    }
    if (htbl == 0) {                                                     // 0x004b014f NULL -> ERREXIT (not reached)
        F<int>(F<void*>(cinfo, 0), 0x14) = 0x31;
        F<int>(F<void*>(cinfo, 0), 0x18) = index;
        (*F<void (__cdecl**)(void*)>(F<void*>(cinfo, 0), 0))(cinfo);
    }
    if (*reinterpret_cast<int*>(htbl + 0x114) == 0) {                   // sent_table
        kEmitMarker(cinfo, 0xc4);
        int count = 0;
        for (int i = 1; i < 17; ++i) count += htbl[i];                  // sum of bits[1..16]
        kEmit2bytes(cinfo, count + 0x13);
        kEmitByte(cinfo, index);
        for (int i = 1; i < 17; ++i) kEmitByte(cinfo, htbl[i]);
        for (int i = 0; i < count; ++i) kEmitByte(cinfo, htbl[0x11 + i]);  // huffval[0..count-1]
        *reinterpret_cast<int*>(htbl + 0x114) = 1;                      // sent_table = TRUE
    }
}

// 0x004b59d0  select_scan_parameters(cinfo): set up comps_in_scan/cur_comp_info[]/Ss/Se/Ah/Al for the current
// scan. With a scan script (scan_info != NULL) it copies the script entry at master->scan_number; otherwise it
// builds a single sequential scan over all components (Ss=0, Se=0x3f, Ah=0, Al=0).
void (__cdecl* SelectScanParameters_orig)(void*);
void __cdecl SelectScanParameters_re(void* cinfo) {
    int scan_info = F<int>(cinfo, 0xa4);
    if (scan_info != 0) {
        int scan_number = F<int>(F<void*>(cinfo, 0x144), 0x20);          // master->scan_number
        int* scanptr = reinterpret_cast<int*>(scan_info + scan_number * 0x24);
        int n = scanptr[0];                                              // comps_in_scan
        F<int>(cinfo, 0xec) = n;
        for (int ci = 0; ci < n; ++ci)                                   // cur_comp_info[ci] = &comp_info[component_index[ci]]
            F<int>(cinfo, 0xf0 + ci * 4) = F<int>(cinfo, 0x3c) + scanptr[1 + ci] * 0x54;
        F<int>(cinfo, 0x134) = scanptr[5];                               // Ss
        F<int>(cinfo, 0x138) = scanptr[6];                               // Se
        F<int>(cinfo, 0x13c) = scanptr[7];                               // Ah
        F<int>(cinfo, 0x140) = scanptr[8];                               // Al
        return;
    }
    int nc = F<int>(cinfo, 0x34);                                        // num_components
    if (nc > 4) {                                                        // > MAX_COMPS_IN_SCAN -> ERREXIT (not reached)
        F<int>(F<void*>(cinfo, 0), 0x14) = 0x18;
        F<int>(F<void*>(cinfo, 0), 0x18) = nc;
        F<int>(F<void*>(cinfo, 0), 0x1c) = 4;
        (*F<void (__cdecl**)(void*)>(F<void*>(cinfo, 0), 0))(cinfo);
    }
    F<int>(cinfo, 0xec) = nc;
    for (int ci = 0; ci < nc; ++ci)                                      // cur_comp_info[ci] = &comp_info[ci]
        F<int>(cinfo, 0xf0 + ci * 4) = F<int>(cinfo, 0x3c) + ci * 0x54;
    F<int>(cinfo, 0x134) = 0;                                            // Ss
    F<int>(cinfo, 0x138) = 0x3f;                                         // Se = DCTSIZE2 - 1
    F<int>(cinfo, 0x13c) = 0;                                            // Ah
    F<int>(cinfo, 0x140) = 0;                                            // Al
}

}  // namespace

SG_HOOK("golf_clean.exe", 0x004b1a10, emit_bits, EmitBits_re, EmitBits_orig);
SG_HOOK("golf_clean.exe", 0x004b2510, flush_bits_4b2510, FlushBits4b2510_re, FlushBits4b2510_orig);
SG_HOOK("golf_clean.exe", 0x004b27d0, emit_buffered_bits, EmitBufferedBits_re, EmitBufferedBits_orig);
SG_HOOK("golf_clean.exe", 0x004ae600, jpeg_add_quant_table, JpegAddQuantTable_re, JpegAddQuantTable_orig);
SG_HOOK("golf_clean.exe", 0x004afec0, emit_dqt, EmitDqt_re, EmitDqt_orig);
SG_HOOK("golf_clean.exe", 0x004affa0, emit_sof, EmitSof_re, EmitSof_orig);
SG_HOOK("golf_clean.exe", 0x004b0120, emit_dht, EmitDht_re, EmitDht_orig);
SG_HOOK("golf_clean.exe", 0x004b59d0, select_scan_parameters, SelectScanParameters_re, SelectScanParameters_orig);

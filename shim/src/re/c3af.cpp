// C3 batch c3af (revisit, 2026-10-08) of golf_clean.exe. Functions offered in earlier rounds and left at C2 for
// time/scope (not for a hard reason), prepared here. Two groups:
//   * IJG libjpeg 6a cmarker / cparam / jfdctmgr helpers that write through a destination or memory manager reached
//     via a function pointer. Earlier rounds (c3r, c3x) called these "a fake allocator callback would work" /
//     "evidence would be thin" and deferred them; the callees are now ready (jpeg_add_quant_table is C3 in c3aa).
//     emit_byte/emit_2bytes/emit_marker match re/match/golf_jpeg_cmarker.cpp; jpeg_alloc_quant_table /
//     jpeg_alloc_huff_table / jpeg_stdio_dest / jinit_forward_dct are transcribed from the disassembly
//     (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr> --list).
//   * spawnWalker: live writer (scoreboard reach 3) that c3a left out "only to keep the batch at a reviewable size".
// Each body cites the address of every global, offset and callee it uses. __thiscall is emulated with __fastcall
// (ecx = this, edx unused). Callees are reached through their original addresses.
#include "hooks.h"

namespace {

// ---- IJG libjpeg manager call-throughs (all __cdecl) ----------------------------------------------------------
// A jpeg_compress/common struct is referenced through: cinfo+0 = err (jpeg_error_mgr*), cinfo+4 = mem
// (jpeg_memory_mgr*), cinfo+0x14 = dest (jpeg_destination_mgr*). The memory manager's first slot (mem+0) is
// alloc_small(cinfo, pool_id, sizeofobject); the error manager's first slot (err+0) is error_exit(cinfo) and
// err+0x14 is msg_code. A destination manager is {next_output_byte at +0, free_in_buffer at +4,
// init_destination at +8, empty_output_buffer at +0xc, term_destination at +0x10}.
typedef void*(__cdecl* AllocSmall_t)(void* cinfo, int pool, int size);
typedef int(__cdecl* EmptyFn_t)(void* cinfo);
typedef void(__cdecl* ErrExit_t)(void* cinfo);

static inline void* field_ptr(void* base, unsigned off) {
    return *reinterpret_cast<void**>(reinterpret_cast<char*>(base) + off);
}

// 0x004afbb0  emit_byte(cinfo, val): store one byte through cinfo->dest (cinfo+0x14). Writes *next_output_byte
// (dest+0), advances it, and decrements free_in_buffer (dest+4); when that reaches 0 (0x004afbcc) it calls
// dest->empty_output_buffer (dest+0xc, 0x004afbcf) and, if that returns 0 (0x004afbd7), raises ERREXIT
// JERR_CANT_SUSPEND = 0x16 through cinfo->err (err->msg_code at err+0x14 = 0x16, err->error_exit at err+0, 0x004afbe5).
typedef void(__cdecl* EmitByte_t)(void* cinfo, int val);
EmitByte_t EmitByte_orig;
void __cdecl EmitByte_re(void* cinfo, int val) {
    char* dest = reinterpret_cast<char*>(field_ptr(cinfo, 0x14));
    unsigned char* p = reinterpret_cast<unsigned char*>(field_ptr(dest, 0));
    *p = static_cast<unsigned char>(val);                       // 0x004afbbe  *next_output_byte = val
    *reinterpret_cast<unsigned char**>(dest + 0) = p + 1;       // 0x004afbc7  next_output_byte++
    unsigned free_in = *reinterpret_cast<unsigned*>(dest + 4);
    *reinterpret_cast<unsigned*>(dest + 4) = free_in - 1;       // 0x004afbc9  free_in_buffer--
    if (free_in - 1 == 0) {                                     // 0x004afbcc
        EmptyFn_t empty = reinterpret_cast<EmptyFn_t>(field_ptr(dest, 0xc));
        if (!empty(cinfo)) {                                    // 0x004afbcf / 0x004afbd7
            void* err = field_ptr(cinfo, 0);
            *reinterpret_cast<int*>(reinterpret_cast<char*>(err) + 0x14) = 0x16;  // 0x004afbdc  msg_code
            ErrExit_t error_exit = reinterpret_cast<ErrExit_t>(field_ptr(err, 0));
            error_exit(cinfo);                                  // 0x004afbe5
        }
    }
}

// 0x004afbf0  emit_marker(cinfo, mark): emit 0xFF then the marker byte (two emit_byte calls, 0x004afbfb/0x004afc06).
typedef void(__cdecl* EmitMarker_t)(void* cinfo, int mark);
const EmitByte_t kEmitByte = reinterpret_cast<EmitByte_t>(0x004afbb0);
EmitMarker_t EmitMarker_orig;
void __cdecl EmitMarker_re(void* cinfo, int mark) {
    kEmitByte(cinfo, 0xff);
    kEmitByte(cinfo, mark);                                     // emit_byte reads only the low byte
}

// 0x004afc10  emit_2bytes(cinfo, value): emit the two bytes MSB first ((value>>8)&0xFF then value&0xFF,
// 0x004afc20/0x004afc2d).
typedef void(__cdecl* Emit2_t)(void* cinfo, int value);
Emit2_t Emit2_orig;
void __cdecl Emit2_re(void* cinfo, int value) {
    kEmitByte(cinfo, (value >> 8) & 0xff);
    kEmitByte(cinfo, value & 0xff);
}

// 0x004afab0  jpeg_alloc_quant_table(cinfo): allocate a JQUANT_TBL (0x84 bytes) in the permanent pool
// (alloc_small(cinfo, 0, 0x84) through cinfo->mem->alloc_small, 0x004afabf), clear its sent_table flag at +0x80
// (0x004afac4) and return it.
typedef void*(__cdecl* AllocTbl_t)(void* cinfo);
AllocTbl_t AllocQuant_orig;
void* __cdecl AllocQuant_re(void* cinfo) {
    AllocSmall_t alloc_small = reinterpret_cast<AllocSmall_t>(field_ptr(field_ptr(cinfo, 4), 0));
    char* tbl = reinterpret_cast<char*>(alloc_small(cinfo, 0, 0x84));
    *reinterpret_cast<int*>(tbl + 0x80) = 0;                    // sent_table = FALSE
    return tbl;
}

// 0x004afad0  jpeg_alloc_huff_table(cinfo): allocate a JHUFF_TBL (0x118 bytes) in the permanent pool
// (alloc_small(cinfo, 0, 0x118), 0x004afadf), clear sent_table at +0x114 (0x004afae4) and return it.
AllocTbl_t AllocHuff_orig;
void* __cdecl AllocHuff_re(void* cinfo) {
    AllocSmall_t alloc_small = reinterpret_cast<AllocSmall_t>(field_ptr(field_ptr(cinfo, 4), 0));
    char* tbl = reinterpret_cast<char*>(alloc_small(cinfo, 0, 0x118));
    *reinterpret_cast<int*>(tbl + 0x114) = 0;                   // sent_table = FALSE
    return tbl;
}

// 0x004ae4d0  jpeg_stdio_dest(cinfo, outfile): install the stdio destination manager. If cinfo->dest (cinfo+0x14)
// is NULL it allocates a 0x1c-byte manager in the permanent pool (alloc_small(cinfo, 0, 0x1c), 0x004ae4e4) and
// stores it at cinfo+0x14 (0x004ae4e9). It then sets the three method pointers init_destination = 0x004ae510
// (dest+8), empty_output_buffer = 0x004ae540 (dest+0xc), term_destination = 0x004ae590 (dest+0x10) and the output
// FILE* outfile at dest+0x14 (0x004ae4f4..0x004ae509).
typedef void(__cdecl* StdioDest_t)(void* cinfo, void* outfile);
StdioDest_t StdioDest_orig;
void __cdecl StdioDest_re(void* cinfo, void* outfile) {
    char* dest = reinterpret_cast<char*>(field_ptr(cinfo, 0x14));
    if (dest == 0) {                                            // 0x004ae4da
        AllocSmall_t alloc_small = reinterpret_cast<AllocSmall_t>(field_ptr(field_ptr(cinfo, 4), 0));
        dest = reinterpret_cast<char*>(alloc_small(cinfo, 0, 0x1c));
        *reinterpret_cast<void**>(reinterpret_cast<char*>(cinfo) + 0x14) = dest;  // 0x004ae4e9
    }
    *reinterpret_cast<void**>(dest + 8) = reinterpret_cast<void*>(0x004ae510);    // init_destination
    *reinterpret_cast<void**>(dest + 0xc) = reinterpret_cast<void*>(0x004ae540);  // empty_output_buffer
    *reinterpret_cast<void**>(dest + 0x10) = reinterpret_cast<void*>(0x004ae590); // term_destination
    *reinterpret_cast<void**>(dest + 0x14) = outfile;
}

// 0x004b34b0  jinit_forward_dct(cinfo): allocate a 0x30-byte forward-DCT controller in the image pool
// (alloc_small(cinfo, 1, 0x30), 0x004b34be), store it at cinfo+0x160 (0x004b34c5) and set fdct->pub.start_pass =
// 0x004b3550 (fdct+0, 0x004b34cb). It then branches on the DCT method at cinfo+0xbc (0x004b34d1): method 0 ->
// {fdct+4 = 0x004b3720, fdct+8 = 0x004b5d50}; method 1 -> {fdct+4 = 0x004b3720, fdct+8 = 0x004b6110}; method 2 ->
// {fdct+4 = 0x004b38c0, fdct+0x1c = 0x004b6360}; any other value raises ERREXIT JERR_NOT_COMPILED = 0x2f
// (0x004b34e5, error_exit, which does not return in libjpeg). Finally it zeroes fdct+0xc/+0x10/+0x14/+0x18 and
// fdct+0x20/+0x24/+0x28/+0x2c (the divisor-table pointer cache, loop at 0x004b352d).
typedef void(__cdecl* JinitFdct_t)(void* cinfo);
JinitFdct_t JinitFdct_orig;
void __cdecl JinitFdct_re(void* cinfo) {
    AllocSmall_t alloc_small = reinterpret_cast<AllocSmall_t>(field_ptr(field_ptr(cinfo, 4), 0));
    char* fdct = reinterpret_cast<char*>(alloc_small(cinfo, 1, 0x30));
    *reinterpret_cast<void**>(reinterpret_cast<char*>(cinfo) + 0x160) = fdct;
    *reinterpret_cast<void**>(fdct + 0) = reinterpret_cast<void*>(0x004b3550);    // start_pass
    int method = *reinterpret_cast<int*>(reinterpret_cast<char*>(cinfo) + 0xbc);
    if (method == 0) {                                          // 0x004b3515
        *reinterpret_cast<void**>(fdct + 4) = reinterpret_cast<void*>(0x004b3720);
        *reinterpret_cast<void**>(fdct + 8) = reinterpret_cast<void*>(0x004b5d50);
    } else if (method == 1) {                                   // 0x004b3505
        *reinterpret_cast<void**>(fdct + 4) = reinterpret_cast<void*>(0x004b3720);
        *reinterpret_cast<void**>(fdct + 8) = reinterpret_cast<void*>(0x004b6110);
    } else if (method == 2) {                                   // 0x004b34f5
        *reinterpret_cast<void**>(fdct + 4) = reinterpret_cast<void*>(0x004b38c0);
        *reinterpret_cast<void**>(fdct + 0x1c) = reinterpret_cast<void*>(0x004b6360);
    } else {                                                    // 0x004b34e2 ERREXIT (never returns)
        void* err = field_ptr(cinfo, 0);
        *reinterpret_cast<int*>(reinterpret_cast<char*>(err) + 0x14) = 0x2f;
        ErrExit_t error_exit = reinterpret_cast<ErrExit_t>(field_ptr(err, 0));
        error_exit(cinfo);
    }
    for (unsigned off = 0xc; off <= 0x18; off += 4) *reinterpret_cast<int*>(fdct + off) = 0;        // 0x004b352d
    for (unsigned off = 0x20; off <= 0x2c; off += 4) *reinterpret_cast<int*>(fdct + off) = 0;
}

// ---- spawnWalker ----------------------------------------------------------------------------------------------
// Random::range(this = 0x00822d9c, n) 0x0045c1e0, thiscall, returns a value in [0, n). Emulated with __fastcall.
typedef int(__fastcall* Range_t)(void* ecx, void* edx, int n);
const Range_t kRange = reinterpret_cast<Range_t>(0x0045c1e0);

// 0x00402970  spawnWalker(type): claim the first free walker slot in the table at 0x00585850 (stride 0x4c, the
// occupied flag is byte +0x12, i.e. 0x00585862 for slot 0; the search loop is 0x00402984..0x0040298d) and fill it.
// The record is zeroed (19 dwords, rep stosd at 0x004029a9), then: world x/y are the placed-object-0 coords
// 0x0058bcba/0x0058bcbc (sign-extended) << 10 + 0x600 written at rec+0/rec+4 (0x004029d1/0x004029e8); rec+0x12
// (occupied) = 1 (0x004029f5); rec+0x16 (direction) = (byte 0x00575cb9 + Random::range(5) - 2) & 7
// (0x004029d4..0x004029fc); rec+0x13 = type (0x00402a02); rec+0x1e = 0xb (0x004029f?); rec+0x1a =
// Random::range(0x20) (0x00402a1d); rec+0x10 = 0xff (0x00402a24). Returns the slot index. Random::range runs
// through its original address 0x0045c1e0 (C3 in c3f); both A/B arms advance the same restored RNG seed 0x00822d9c.
typedef int(__cdecl* Spawn_t)(int type);
Spawn_t Spawn_orig;
int __cdecl Spawn_re(int type) {
    int idx = 0;
    if (*reinterpret_cast<unsigned char*>(0x00585862) != 0) {   // slot 0 occupied? 0x0040297b
        char* f = reinterpret_cast<char*>(0x00585862);
        do { f += 0x4c; idx++; } while (*reinterpret_cast<unsigned char*>(f) != 0);  // 0x00402984
    }
    char* rec = reinterpret_cast<char*>(0x00585850) + idx * 0x4c;
    for (int i = 0; i < 19; i++) reinterpret_cast<int*>(rec)[i] = 0;                 // rep stosd 0x13
    int r1 = kRange(reinterpret_cast<void*>(0x00822d9c), 0, 5);                      // 0x004029b0
    int ox = *reinterpret_cast<short*>(0x0058bcba);
    int oy = *reinterpret_cast<short*>(0x0058bcbc);
    *reinterpret_cast<int*>(rec + 0) = (ox << 10) + 0x600;                           // rec+0 world x
    *reinterpret_cast<int*>(rec + 4) = (oy << 10) + 0x600;                           // rec+4 world y
    unsigned char dir = static_cast<unsigned char>((*reinterpret_cast<unsigned char*>(0x00575cb9) + r1 - 2) & 7);
    *reinterpret_cast<unsigned char*>(rec + 0x12) = 1;                               // occupied
    *reinterpret_cast<unsigned char*>(rec + 0x16) = dir;                             // direction
    *reinterpret_cast<unsigned char*>(rec + 0x13) = static_cast<unsigned char>(type);
    *reinterpret_cast<short*>(rec + 0x1e) = 0xb;
    int r2 = kRange(reinterpret_cast<void*>(0x00822d9c), 0, 0x20);                   // 0x00402a18
    *reinterpret_cast<short*>(rec + 0x1a) = static_cast<short>(r2);
    *reinterpret_cast<unsigned char*>(rec + 0x10) = 0xff;
    return idx;
}

}  // namespace

// 0x004afbb0  emit_byte
SG_HOOK("golf_clean.exe", 0x004afbb0, emit_byte, EmitByte_re, EmitByte_orig);
// 0x004afbf0  emit_marker
SG_HOOK("golf_clean.exe", 0x004afbf0, emit_marker, EmitMarker_re, EmitMarker_orig);
// 0x004afc10  emit_2bytes
SG_HOOK("golf_clean.exe", 0x004afc10, emit_2bytes, Emit2_re, Emit2_orig);
// 0x004afab0  jpeg_alloc_quant_table
SG_HOOK("golf_clean.exe", 0x004afab0, jpeg_alloc_quant_table, AllocQuant_re, AllocQuant_orig);
// 0x004afad0  jpeg_alloc_huff_table
SG_HOOK("golf_clean.exe", 0x004afad0, jpeg_alloc_huff_table, AllocHuff_re, AllocHuff_orig);
// 0x004ae4d0  jpeg_stdio_dest
SG_HOOK("golf_clean.exe", 0x004ae4d0, jpeg_stdio_dest, StdioDest_re, StdioDest_orig);
// 0x004b34b0  jinit_forward_dct
SG_HOOK("golf_clean.exe", 0x004b34b0, jinit_forward_dct, JinitFdct_re, JinitFdct_orig);
// 0x00402970  spawnWalker
SG_HOOK("golf_clean.exe", 0x00402970, spawnWalker, Spawn_re, Spawn_orig);

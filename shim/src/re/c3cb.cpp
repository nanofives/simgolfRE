// Batch c3cb: jgld.dll non-render - the zlib 1.0.2 inflate chain.
//   inflate_trees_fixed    0x0009f030
//   falloc                 0x0009f210   (reached only through the pointer stored at 0x1009f065)
//   inflate_trees_bits     0x0009e710
//   inflate_trees_dynamic  0x0009ef00
//   inflate_codes_new      0x0009f2f0
//   inflate_codes_free     0x000a00e0
//   inflate_blocks_new     0x0009cd90
//   inflate_blocks_reset   0x0009cc70
//   inflate_blocks_free    0x0009e290
//   inflateReset           0x0009bdd0
//   inflateEnd             0x0009be70
//   inflateInit2_          0x0009bf10
//   inflateInit_           0x0009c0b0
//
// SG_HOOK addresses are RVAs; every instruction cited in a comment is a VA (0x10000000 + RVA), the form
// the debug build's disassembly prints. jgld.dll is LoadLibrary'd after the shim starts, so these hooks
// install from the LoadLibraryA hook (SgHooksInstallPending, re/hooks.cpp) and diff_hook finds them by
// (module, rva) through SimGolfShim_FindHookM. The module is relocated at run time (measured: base
// 0x03930000 in a boot of 2026-10-09, log/c3/alt/c3cb_probe.py), so every address this file reaches is
// resolved against the running base through JgldVA.
//
// WHY THIS FAMILY CAN BE A/B'd AT ALL. zlib never calls the heap directly: every allocation goes through
// the z_stream's zalloc at +0x20 and every free through its zfree at +0x24, both with the opaque at +0x28,
// and the z_stream belongs to the CALLER. The fixture (re/frida/js/fixtures.d/c3cb.js) puts a slot
// allocator and a recording deallocator of its own there, so the two arms hand back the same addresses in
// the same order, nothing reaches operator new or operator delete, and the sequence of allocations and
// frees becomes part of the compared state. This is the redirection c3bw used for inflate_trees_free
// 0x1009f250 (log/c3/c3bw_notes.txt), one level up.
//
// The only function here that touches MODULE globals is inflate_trees_fixed, which writes fixed_built
// 0x10129b88, fixed_bl 0x10129b84, fixed_bd 0x10129b80, fixed_tl 0x10128aec, fixed_td 0x10128ae8 and the
// 530-entry fixed_mem at 0x10128af0 that falloc 0x1009f210 hands out. A 20 s probe at the menu entered
// inflate_trees_fixed, inflate_blocks, inflate, inflateInit_, huft_build and png_read_row 0 times each and
// sampled fixed_built as 0 throughout (log/c3/alt/c3cb_probe.py), so the running game is not in this code
// while the A/B runs and those globals are not shared state.
#include "hooks.h"

namespace {

// jgld.dll is relocatable, so an address the code calls or stores is resolved against the running base.
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
inline void*& P32(void* p, unsigned off) {
    return *reinterpret_cast<void**>(static_cast<char*>(p) + off);
}
inline void* P32R(const void* p, unsigned off) {
    return *reinterpret_cast<void* const*>(static_cast<const char*>(p) + off);
}

// ---------------------------------------------------------------------------------- shared layouts
//
// z_stream, 0x38 bytes (the size inflateInit2_ checks at 0x1009bf3f): next_in +0, avail_in +4,
// total_in +8, next_out +0xc, avail_out +0x10, total_out +0x14, msg +0x18, state +0x1c, zalloc +0x20,
// zfree +0x24, opaque +0x28, data_type +0x2c, adler +0x30, reserved +0x34.
const unsigned Z_TOTAL_IN = 0x08, Z_TOTAL_OUT = 0x14, Z_MSG = 0x18, Z_STATE = 0x1c;
const unsigned Z_ZALLOC = 0x20, Z_ZFREE = 0x24, Z_OPAQUE = 0x28, Z_ADLER = 0x30;

// internal_state, 0x18 bytes (the size inflateInit2_ asks for at 0x1009bf9b): mode +0, sub +4 (8 bytes),
// nowrap +0xc, wbits +0x10, blocks +0x14.
const unsigned S_MODE = 0x00, S_NOWRAP = 0x0c, S_WBITS = 0x10, S_BLOCKS = 0x14;

// inflate_blocks_state, 0x3c bytes (the size inflate_blocks_new asks for at 0x1009cdaa): mode +0, the
// sub union +4..+0x17 (decode.tl +4, decode.td +8, decode.codes and trees.blens +0xc), last +0x18,
// bitk +0x1c, bitb +0x20, window +0x24, end +0x28, read +0x2c, write +0x30, checkfn +0x34, check +0x38.
// +0x0c is sub.decode.codes for mode CODES and sub.trees.blens for modes BTREE and DTREE: the same
// dword of the union, which is why exactly one of inflate_blocks_reset's two release arms can run.
const unsigned B_MODE = 0x00, B_TL = 0x04, B_TD = 0x08, B_CODES = 0x0c, B_BLENS = 0x0c;
const unsigned B_BITK = 0x1c, B_BITB = 0x20, B_WINDOW = 0x24, B_END = 0x28;
const unsigned B_READ = 0x2c, B_WRITE = 0x30, B_CHECKFN = 0x34, B_CHECK = 0x38;

// inflate_codes_state, 0x1c bytes (0x1009f30a): mode +0, lbits +0x10 (one byte), dbits +0x11 (one byte),
// ltree +0x14, dtree +0x18.
const unsigned C_MODE = 0x00, C_LBITS = 0x10, C_DBITS = 0x11, C_LTREE = 0x14, C_DTREE = 0x18;

// zlib return codes, as the immediates in the bodies: Z_OK 0 (0x1009f1ef), Z_STREAM_ERROR -2
// (0x1009bdf7), Z_DATA_ERROR -3 (0x1009e74b), Z_MEM_ERROR -4 (0x1009bfc5), Z_BUF_ERROR -5 (0x1009e75d),
// Z_VERSION_ERROR -6 (0x1009bf45).
const int Z_OK = 0, Z_STREAM_ERROR = -2, Z_DATA_ERROR = -3, Z_MEM_ERROR = -4;
const int Z_BUF_ERROR = -5, Z_VERSION_ERROR = -6;

// Block modes inflate_blocks_reset tests: BTREE 4 (0x1009cc9f), DTREE 5 (0x1009cca7), CODES 6
// (0x1009cccf), TYPE 0 (the value it resets to, 0x1009cd10).
const unsigned BM_TYPE = 0, BM_BTREE = 4, BM_DTREE = 5, BM_CODES = 6;

// Stream modes inflateReset selects between: METHOD 0 and BLOCKS 7 (the `and eax,7` at 0x1009be29).
const unsigned SM_METHOD = 0, SM_BLOCKS = 7;

typedef void*(__cdecl* ZAlloc_t)(void* opaque, unsigned items, unsigned size);
typedef void(__cdecl* ZFree_t)(void* opaque, void* addr);
typedef unsigned long(__cdecl* CheckFn_t)(unsigned long check, const unsigned char* buf, unsigned len);

// ZALLOC(z, items, size) and ZFREE(z, addr), the two zutil.h macros, as they are compiled everywhere in
// this file: an indirect __cdecl call through the z_stream with the opaque as the first argument.
inline void* ZAlloc(void* z, unsigned items, unsigned size) {
    return reinterpret_cast<ZAlloc_t>(P32R(z, Z_ZALLOC))(P32R(z, Z_OPAQUE), items, size);
}
inline void ZFree(void* z, void* addr) {
    reinterpret_cast<ZFree_t>(P32R(z, Z_ZFREE))(P32R(z, Z_OPAQUE), addr);
}

// jgld.dll's own zlib routines the bodies below call, by VA.
typedef int(__cdecl* HuftBuild_t)(unsigned* b, unsigned n, unsigned s, const unsigned* d,
                                  const unsigned* e, void** t, unsigned* m, void* zs);
typedef int(__cdecl* InflateTreesFree_t)(void* t, void* z);
typedef void(__cdecl* InflateCodesFree_t)(void* c, void* z);
typedef void(__cdecl* InflateBlocksReset_t)(void* s, void* z, unsigned long* c);
typedef void*(__cdecl* InflateBlocksNew_t)(void* z, void* c, unsigned w);
typedef int(__cdecl* InflateBlocksFree_t)(void* s, void* z, unsigned long* c);
typedef int(__cdecl* InflateReset_t)(void* z);
typedef int(__cdecl* InflateEnd_t)(void* z);
typedef int(__cdecl* InflateInit2_t)(void* z, int w, const char* version, int stream_size);

const unsigned VA_HUFT_BUILD = 0x1009e7a0;
const unsigned VA_TREES_FREE = 0x1009f250;
const unsigned VA_CODES_FREE = 0x100a00e0;
const unsigned VA_BLOCKS_RESET = 0x1009cc70;
const unsigned VA_BLOCKS_NEW = 0x1009cd90;
const unsigned VA_BLOCKS_FREE = 0x1009e290;
const unsigned VA_INFLATE_RESET = 0x1009bdd0;
const unsigned VA_INFLATE_END = 0x1009be70;
const unsigned VA_INFLATE_INIT2 = 0x1009bf10;

// ----------------------------------------------------------------- inflate_trees_fixed 0x0009f030
//
// int __cdecl inflate_trees_fixed(uInt *bl, uInt *bd, inflate_huft **tl, inflate_huft **td), arguments at
// [ebp+8], [ebp+0xc], [ebp+0x10], [ebp+0x14]; `ret` with no pop at 0x1009f204, so __cdecl.
//
// It builds the deflate FIXED Huffman tables once into module storage and then copies four values out.
//
// The guard at 0x1009f04e-0x1009f055 reads fixed_built 0x10129b88 and skips the whole build when it is
// non-zero. The build itself:
//   * a local z_stream at [ebp-0x4bc] whose zalloc at [ebp-0x49c] is set to falloc 0x1009f210
//     (0x1009f065), whose zfree at [ebp-0x498] is set to 0 (0x1009f06f) and whose opaque at [ebp-0x494]
//     points at the local counter f at [ebp-0x4c0] (0x1009f079), initialised to 0x212 = 530 at
//     0x1009f05b. falloc subtracts the item count from that counter (0x1009f230) and returns
//     fixed_mem + counter*8 (0x1009f23c, fixed_mem = 0x10128af0, eight bytes per inflate_huft), so the
//     530 entries are handed out from the top down and nothing reaches the heap.
//   * four runs over one length array c[288] at [ebp-0x484], with ONE index that is never reset between
//     them (0x1009f085 sets it to 0 and the later loops carry it on): entries 0..0x8f get 8
//     (0x1009f0a3), 0x90..0xff get 9 (0x1009f0c7), 0x100..0x117 get 7 (0x1009f0eb) and 0x118..0x11f get
//     8 (0x1009f10f).
//   * fixed_bl 0x10129b84 = 7 (0x1009f11c), then huft_build(c, 288, 257, cplens 0x101272bc, cplext
//     0x10127338, &fixed_tl 0x10128aec, &fixed_bl, &z) at 0x1009f152, its return value discarded.
//   * entries 0..0x1d of the same array get 5 (0x1009f175), fixed_bd 0x10129b80 = 5 (0x1009f182) and
//     huft_build(c, 30, 0, cpdist 0x101273b4, cpdext 0x1012742c, &fixed_td 0x10128ae8, &fixed_bd, &z) at
//     0x1009f1b2, again discarding the result.
//   * fixed_built = 1 (0x1009f1ba).
// Then *bl = fixed_bl (0x1009f1cd), *bd = fixed_bd (0x1009f1d7), *tl = fixed_tl (0x1009f1e2),
// *td = fixed_td (0x1009f1ed) and it returns 0 (0x1009f1ef) on every path.
typedef int(__cdecl* InflateTreesFixed_t)(unsigned*, unsigned*, void**, void**);
InflateTreesFixed_t InflateTreesFixed_orig;

int __cdecl InflateTreesFixed_re(unsigned* bl, unsigned* bd, void** tl, void** td) {
    unsigned* fixed_built = static_cast<unsigned*>(JgldVA(0x10129b88));
    unsigned* fixed_bl = static_cast<unsigned*>(JgldVA(0x10129b84));
    unsigned* fixed_bd = static_cast<unsigned*>(JgldVA(0x10129b80));
    void** fixed_tl = static_cast<void**>(JgldVA(0x10128aec));
    void** fixed_td = static_cast<void**>(JgldVA(0x10128ae8));

    if (*fixed_built == 0) {                                     // 0x1009f04e
        int f = 0x212;                                           // 0x1009f05b  FIXEDH = 530
        unsigned zbuf[0x38 / 4];                                 // the local z_stream at [ebp-0x4bc]
        void* z = zbuf;
        P32(z, Z_ZALLOC) = JgldVA(0x1009f210);                   // 0x1009f065  falloc
        P32(z, Z_ZFREE) = 0;                                     // 0x1009f06f
        P32(z, Z_OPAQUE) = &f;                                   // 0x1009f079
        // The original leaves the other 0x2c bytes of its local z_stream as the /GZ fill 0xcccccccc;
        // huft_build reads only zalloc and opaque (its ZALLOC) on the paths these lengths take.

        unsigned c[288];
        int k = 0;                                               // 0x1009f085
        for (; k < 0x90; k++) c[k] = 8;                          // 0x1009f097, 0x1009f0a3
        for (; k < 0x100; k++) c[k] = 9;                         // 0x1009f0bb, 0x1009f0c7
        for (; k < 0x118; k++) c[k] = 7;                         // 0x1009f0df, 0x1009f0eb
        for (; k < 0x120; k++) c[k] = 8;                         // 0x1009f103, 0x1009f10f

        *fixed_bl = 7;                                           // 0x1009f11c
        reinterpret_cast<HuftBuild_t>(JgldVA(VA_HUFT_BUILD))(
            c, 0x120, 0x101,
            static_cast<const unsigned*>(JgldVA(0x101272bc)),     // cplens
            static_cast<const unsigned*>(JgldVA(0x10127338)),     // cplext
            fixed_tl, fixed_bl, z);                               // 0x1009f152

        for (k = 0; k < 0x1e; k++) c[k] = 5;                      // 0x1009f15a, 0x1009f16c, 0x1009f175
        *fixed_bd = 5;                                            // 0x1009f182
        reinterpret_cast<HuftBuild_t>(JgldVA(VA_HUFT_BUILD))(
            c, 0x1e, 0,
            static_cast<const unsigned*>(JgldVA(0x101273b4)),     // cpdist
            static_cast<const unsigned*>(JgldVA(0x1012742c)),     // cpdext
            fixed_td, fixed_bd, z);                               // 0x1009f1b2

        *fixed_built = 1;                                         // 0x1009f1ba
    }
    *bl = *fixed_bl;                                              // 0x1009f1cd
    *bd = *fixed_bd;                                              // 0x1009f1d7
    *tl = *fixed_tl;                                              // 0x1009f1e2
    *td = *fixed_td;                                              // 0x1009f1ed
    return Z_OK;                                                  // 0x1009f1ef
}

// ------------------------------------------------------------------------------ falloc 0x0009f210
//
// voidpf __cdecl falloc(voidpf q, uInt n, uInt s): q [ebp+8], n [ebp+0xc], s [ebp+0x10]; `ret` with no
// pop at 0x1009f249. It is reached only through the function pointer inflate_trees_fixed stores in its
// local z_stream at 0x1009f065, which is the only reference to this address in the image.
//
// Three steps and no branch: it reads the counter *q (0x1009f22e), subtracts the item count n from it
// and stores it back (0x1009f230, 0x1009f235), then re-reads it (0x1009f23a) and returns
// fixed_mem 0x10128af0 + *q * 8, the `lea eax,[edx*8+0x10128af0]` at 0x1009f23c - eight bytes being one
// inflate_huft. The THIRD argument is never read anywhere in the body: zlib 1.0.2 writes the subtraction
// as `n+s-s` to silence an unused-parameter warning and the compiler drops it.
// The frame is also the only one in this batch with no __chkesp call in the epilogue.
typedef void*(__cdecl* Falloc_t)(void*, unsigned, unsigned);
Falloc_t Falloc_orig;

void* __cdecl Falloc_re(void* q, unsigned n, unsigned) {
    unsigned* counter = static_cast<unsigned*>(q);
    *counter = *counter - n;                                      // 0x1009f230, 0x1009f235
    return static_cast<char*>(JgldVA(0x10128af0)) + *counter * 8;  // 0x1009f23c
}

// ------------------------------------------------------------------ inflate_trees_bits 0x0009e710
//
// int __cdecl inflate_trees_bits(uInt *c, uInt *bb, inflate_huft **tb, z_stream *z): c at [ebp+8],
// bb at [ebp+0xc], tb at [ebp+0x10], z at [ebp+0x14]; `ret` with no pop at 0x1009e799.
//
// One huft_build over the 19 code-length codes with the constants 19 and 19 pushed at 0x1009e738 and
// 0x1009e73a and both the base and extra lists null (0x1009e734, 0x1009e736); the pushes at 0x1009e72b,
// 0x1009e72f and 0x1009e733 put z, bb and tb in place, so the call at 0x1009e740 is
// huft_build(c, 19, 19, NULL, NULL, tb, bb, z) and its result is kept at [ebp-4].
// The two messages it can leave in z->msg (+0x18) are module strings:
//   * r == Z_DATA_ERROR (0x1009e74b): z->msg = 0x10120978 (0x1009e754) and nothing else happens.
//   * r == Z_BUF_ERROR (0x1009e75d): inflate_trees_free(*tb, z) at 0x1009e76d, z->msg = 0x10120954
//     (0x1009e778) and r becomes Z_DATA_ERROR (0x1009e77f).
// Any other value of r (Z_OK, Z_MEM_ERROR) falls past both compares untouched and is returned as it came
// back from huft_build (0x1009e786).
typedef int(__cdecl* InflateTreesBits_t)(unsigned*, unsigned*, void**, void*);
InflateTreesBits_t InflateTreesBits_orig;

int __cdecl InflateTreesBits_re(unsigned* c, unsigned* bb, void** tb, void* z) {
    int r = reinterpret_cast<HuftBuild_t>(JgldVA(VA_HUFT_BUILD))(
        c, 19, 19, 0, 0, tb, bb, z);                              // 0x1009e740
    if (r == Z_DATA_ERROR) {                                      // 0x1009e74b
        P32(z, Z_MSG) = JgldVA(0x10120978);                       // 0x1009e754
    } else if (r == Z_BUF_ERROR) {                                // 0x1009e75d
        reinterpret_cast<InflateTreesFree_t>(JgldVA(VA_TREES_FREE))(*tb, z);   // 0x1009e76d
        P32(z, Z_MSG) = JgldVA(0x10120954);                       // 0x1009e778
        r = Z_DATA_ERROR;                                         // 0x1009e77f
    }
    return r;                                                     // 0x1009e786
}

// --------------------------------------------------------------- inflate_trees_dynamic 0x0009ef00
//
// int __cdecl inflate_trees_dynamic(uInt nl, uInt nd, uInt *c, uInt *bl, uInt *bd, inflate_huft **tl,
// inflate_huft **td, z_stream *z): nl [ebp+8], nd [ebp+0xc], c [ebp+0x10], bl [ebp+0x14], bd [ebp+0x18],
// tl [ebp+0x1c], td [ebp+0x20], z [ebp+0x24]; `ret` with no pop at 0x1009f02a.
//
// Two huft_build calls with the same error handling each.
//   * literal/length: huft_build(c, nl, 257, cplens 0x101272bc, cplext 0x10127338, tl, bl, z) at
//     0x1009ef3b (the 257 is the immediate at 0x1009ef2e). Z_OK leaves at 0x1009ef4a; otherwise
//     Z_DATA_ERROR (0x1009ef4c) sets z->msg = 0x101209c0 (0x1009ef55), Z_BUF_ERROR (0x1009ef5e) frees
//     *tl at 0x1009ef6e, sets z->msg = 0x101209a0 (0x1009ef79) and makes r Z_DATA_ERROR (0x1009ef80),
//     and the function returns r (0x1009ef87) WITHOUT freeing anything else.
//   * distance: huft_build(c + nl, nd, 0, cpdist 0x101273b4, cpdext 0x1012742c, td, bd, z) at
//     0x1009efb5; the base pointer is computed as c + nl*4 at 0x1009efb1, and the `simple` count is the
//     0 pushed at 0x1009efa5. Z_OK leaves at 0x1009efc4 and returns 0 (0x1009f018). The two error arms
//     use the SAME two module strings as the first build (0x1009efcf and 0x1009eff3) - zlib 1.0.2 spells
//     both pairs "oversubscribed literal/length tree" and "incomplete literal/length tree" - and then
//     fall into the shared tail at 0x1009f001, which frees *tl (0x1009f00b) before returning r.
typedef int(__cdecl* InflateTreesDynamic_t)(unsigned, unsigned, unsigned*, unsigned*, unsigned*,
                                            void**, void**, void*);
InflateTreesDynamic_t InflateTreesDynamic_orig;

int __cdecl InflateTreesDynamic_re(unsigned nl, unsigned nd, unsigned* c, unsigned* bl, unsigned* bd,
                                   void** tl, void** td, void* z) {
    HuftBuild_t huft = reinterpret_cast<HuftBuild_t>(JgldVA(VA_HUFT_BUILD));
    InflateTreesFree_t freeTrees = reinterpret_cast<InflateTreesFree_t>(JgldVA(VA_TREES_FREE));

    int r = huft(c, nl, 257,
                 static_cast<const unsigned*>(JgldVA(0x101272bc)),
                 static_cast<const unsigned*>(JgldVA(0x10127338)),
                 tl, bl, z);                                      // 0x1009ef3b
    if (r != Z_OK) {                                              // 0x1009ef46
        if (r == Z_DATA_ERROR) {                                  // 0x1009ef4c
            P32(z, Z_MSG) = JgldVA(0x101209c0);                   // 0x1009ef55
        } else if (r == Z_BUF_ERROR) {                            // 0x1009ef5e
            freeTrees(*tl, z);                                    // 0x1009ef6e
            P32(z, Z_MSG) = JgldVA(0x101209a0);                   // 0x1009ef79
            r = Z_DATA_ERROR;                                     // 0x1009ef80
        }
        return r;                                                 // 0x1009ef87
    }

    r = huft(c + nl, nd, 0,
             static_cast<const unsigned*>(JgldVA(0x101273b4)),
             static_cast<const unsigned*>(JgldVA(0x1012742c)),
             td, bd, z);                                          // 0x1009efb5
    if (r != Z_OK) {                                              // 0x1009efc0
        if (r == Z_DATA_ERROR) {                                  // 0x1009efc6
            P32(z, Z_MSG) = JgldVA(0x101209c0);                   // 0x1009efcf
        } else if (r == Z_BUF_ERROR) {                            // 0x1009efd8
            freeTrees(*td, z);                                    // 0x1009efe8
            P32(z, Z_MSG) = JgldVA(0x101209a0);                   // 0x1009eff3
            r = Z_DATA_ERROR;                                     // 0x1009effa
        }
        freeTrees(*tl, z);                                        // 0x1009f00b
        return r;                                                 // 0x1009f013
    }
    return Z_OK;                                                  // 0x1009f018
}

// ------------------------------------------------------------------- inflate_codes_new 0x0009f2f0
//
// inflate_codes_statef * __cdecl inflate_codes_new(uInt bl, uInt bd, inflate_huft *tl,
// inflate_huft *td, z_stream *z): bl [ebp+8], bd [ebp+0xc], tl [ebp+0x10], td [ebp+0x14], z [ebp+0x18];
// `ret` with no pop at 0x1009f36e.
//
// ZALLOC(z, 1, 0x1c) at 0x1009f318 (the 0x1c and the 1 pushed at 0x1009f30a and 0x1009f30c, the opaque
// read from z+0x28 at 0x1009f311). When the result is null (0x1009f328) the function writes nothing and
// returns it. Otherwise it fills five fields and leaves the other 0x14 bytes of the block as the
// allocator left them: mode = 0 (0x1009f331, START), lbits = the LOW BYTE of bl at +0x10 (0x1009f33a
// reads it as a byte, 0x1009f33d stores it), dbits = the low byte of bd at +0x11 (0x1009f346),
// ltree = tl at +0x14 (0x1009f34f) and dtree = td at +0x18 (0x1009f358). It returns the block
// (0x1009f35b) on both paths.
typedef void*(__cdecl* InflateCodesNew_t)(unsigned, unsigned, void*, void*, void*);
InflateCodesNew_t InflateCodesNew_orig;

void* __cdecl InflateCodesNew_re(unsigned bl, unsigned bd, void* tl, void* td, void* z) {
    void* c = ZAlloc(z, 1, 0x1c);                                 // 0x1009f318
    if (c != 0) {                                                 // 0x1009f328
        U32(c, C_MODE) = 0;                                       // 0x1009f331
        *(static_cast<unsigned char*>(c) + C_LBITS) =
            static_cast<unsigned char>(bl);                       // 0x1009f33d
        *(static_cast<unsigned char*>(c) + C_DBITS) =
            static_cast<unsigned char>(bd);                       // 0x1009f346
        P32(c, C_LTREE) = tl;                                     // 0x1009f34f
        P32(c, C_DTREE) = td;                                     // 0x1009f358
    }
    return c;                                                     // 0x1009f35b
}

// ------------------------------------------------------------------ inflate_codes_free 0x000a00e0
//
// void __cdecl inflate_codes_free(inflate_codes_statef *c, z_stream *z): c [ebp+8], z [ebp+0xc]; `ret`
// with no pop at 0x100a0125.
//
// The whole body is one ZFREE: it pushes c (0x100a00fd), the opaque from z+0x28 (0x100a0104) and calls
// z->zfree at z+0x24 (0x100a0108), then pops the eight bytes itself (0x100a010b, __cdecl). There is no
// conditional jump and no null test: a null c is handed to zfree like any other pointer.
InflateCodesFree_t InflateCodesFree_orig;

void __cdecl InflateCodesFree_re(void* c, void* z) {
    ZFree(z, c);                                                  // 0x100a0108
}

// ---------------------------------------------------------------- inflate_blocks_reset 0x0009cc70
//
// void __cdecl inflate_blocks_reset(inflate_blocks_statef *s, z_stream *z, uLongf *c): s [ebp+8],
// z [ebp+0xc], c [ebp+0x10]; `ret` with no pop at 0x1009cd85.
//
// Four steps, three of them guarded:
//   * if s->checkfn (+0x34) is not null (0x1009cc8b), *c = s->check (+0x38) at 0x1009cc9a. This is the
//     only read of the out parameter and callers that do not want it pass a scratch dword.
//   * if s->mode is BTREE (4) or DTREE (5) (0x1009cc9f and 0x1009cca7), ZFREE(z, s->sub.trees.blens),
//     the pointer at +0xc, at 0x1009ccbf.
//   * if s->mode is CODES (6) (0x1009cccf), three releases in this order: inflate_codes_free of +0xc
//     (0x1009ccdf), inflate_trees_free of +8 (0x1009ccf2) and inflate_trees_free of +4 (0x1009cd05).
//     The union makes +0xc the codes pointer here and the blens pointer above, so exactly one of these
//     two arms runs for any mode.
//   * unconditionally: mode = TYPE 0 (0x1009cd10), bitk = 0 (0x1009cd19), bitb = 0 (0x1009cd23), then
//     write = window (0x1009cd33) and read = write (0x1009cd3f), in that order - the source's
//     `s->read = s->write = s->window` with the inner assignment first.
//   * if s->checkfn is not null again (0x1009cd45), it is called with (0, NULL, 0) (the three zeros
//     pushed at 0x1009cd4d-0x1009cd51, the indirect __cdecl call at 0x1009cd56); the result becomes
//     s->check (0x1009cd66) and is then copied into z->adler at +0x30 (0x1009cd72).
InflateBlocksReset_t InflateBlocksReset_orig;

void __cdecl InflateBlocksReset_re(void* s, void* z, unsigned long* c) {
    if (P32R(s, B_CHECKFN) != 0) {                                // 0x1009cc8b
        *c = U32R(s, B_CHECK);                                    // 0x1009cc9a
    }
    if (U32R(s, B_MODE) == BM_BTREE || U32R(s, B_MODE) == BM_DTREE) {   // 0x1009cc9f, 0x1009cca7
        ZFree(z, P32R(s, B_BLENS));                               // 0x1009ccbf
    }
    if (U32R(s, B_MODE) == BM_CODES) {                            // 0x1009cccf
        reinterpret_cast<InflateCodesFree_t>(JgldVA(VA_CODES_FREE))(P32R(s, B_CODES), z);  // 0x1009ccdf
        reinterpret_cast<InflateTreesFree_t>(JgldVA(VA_TREES_FREE))(P32R(s, B_TD), z);     // 0x1009ccf2
        reinterpret_cast<InflateTreesFree_t>(JgldVA(VA_TREES_FREE))(P32R(s, B_TL), z);     // 0x1009cd05
    }
    U32(s, B_MODE) = BM_TYPE;                                     // 0x1009cd10
    U32(s, B_BITK) = 0;                                           // 0x1009cd19
    U32(s, B_BITB) = 0;                                           // 0x1009cd23
    P32(s, B_WRITE) = P32R(s, B_WINDOW);                          // 0x1009cd33
    P32(s, B_READ) = P32R(s, B_WRITE);                            // 0x1009cd3f
    if (P32R(s, B_CHECKFN) != 0) {                                // 0x1009cd45
        U32(s, B_CHECK) = reinterpret_cast<CheckFn_t>(P32R(s, B_CHECKFN))(0, 0, 0);  // 0x1009cd56
        U32(z, Z_ADLER) = U32R(s, B_CHECK);                       // 0x1009cd72
    }
}

// ------------------------------------------------------------------ inflate_blocks_new 0x0009cd90
//
// inflate_blocks_statef * __cdecl inflate_blocks_new(z_stream *z, check_func c, uInt w): z [ebp+8],
// c [ebp+0xc], w [ebp+0x10]; `ret` with no pop at 0x1009ce70.
//
// Two allocations, each through the caller's zalloc:
//   * ZALLOC(z, 1, 0x3c) at 0x1009cdb8 (the size pushed at 0x1009cdaa). A null result is returned as it
//     is (0x1009cdcc jumps over everything to 0x1009cdce), so the caller gets NULL and nothing leaks.
//   * ZALLOC(z, 1, w) at 0x1009cde8 straight into s->window (+0x24, stored at 0x1009cdf8). When that one
//     is null (0x1009cdfe) the first block is handed back with ZFREE(z, s) at 0x1009ce14 and the
//     function returns 0 (0x1009ce21) - NOT s, which is what the first failure returns.
// On success: s->end (+0x28) = window + w (0x1009ce2b, 0x1009ce31), s->checkfn (+0x34) = c (0x1009ce3a),
// s->mode (+0) = TYPE 0 (0x1009ce40) and inflate_blocks_reset(s, z, &s->check) at 0x1009ce55, the third
// argument being s+0x38 computed at 0x1009ce49. It returns s (0x1009ce5d).
InflateBlocksNew_t InflateBlocksNew_orig;

void* __cdecl InflateBlocksNew_re(void* z, void* c, unsigned w) {
    void* s = ZAlloc(z, 1, 0x3c);                                 // 0x1009cdb8
    if (s == 0) {                                                 // 0x1009cdcc
        return s;                                                 // 0x1009cdce
    }
    P32(s, B_WINDOW) = ZAlloc(z, 1, w);                           // 0x1009cde8, 0x1009cdf8
    if (P32R(s, B_WINDOW) == 0) {                                 // 0x1009cdfe
        ZFree(z, s);                                              // 0x1009ce14
        return 0;                                                 // 0x1009ce21
    }
    P32(s, B_END) = static_cast<char*>(P32R(s, B_WINDOW)) + w;    // 0x1009ce2b
    P32(s, B_CHECKFN) = c;                                        // 0x1009ce3a
    U32(s, B_MODE) = BM_TYPE;                                     // 0x1009ce40
    reinterpret_cast<InflateBlocksReset_t>(JgldVA(VA_BLOCKS_RESET))(
        s, z, reinterpret_cast<unsigned long*>(static_cast<char*>(s) + B_CHECK));  // 0x1009ce55
    return s;                                                     // 0x1009ce5d
}

// ----------------------------------------------------------------- inflate_blocks_free 0x0009e290
//
// int __cdecl inflate_blocks_free(inflate_blocks_statef *s, z_stream *z, uLongf *c): s [ebp+8],
// z [ebp+0xc], c [ebp+0x10]; `ret` with no pop at 0x1009e30b. No conditional jump anywhere.
//
// inflate_blocks_reset(s, z, c) at 0x1009e2b4 - which is what releases whatever the block state still
// owns - then ZFREE(z, s->window) with the pointer read from +0x24 at 0x1009e2c1 (call 0x1009e2cf) and
// ZFREE(z, s) (call 0x1009e2ec), in that order. It returns 0 (0x1009e2f9) always.
InflateBlocksFree_t InflateBlocksFree_orig;

int __cdecl InflateBlocksFree_re(void* s, void* z, unsigned long* c) {
    reinterpret_cast<InflateBlocksReset_t>(JgldVA(VA_BLOCKS_RESET))(s, z, c);   // 0x1009e2b4
    ZFree(z, P32R(s, B_WINDOW));                                  // 0x1009e2cf
    ZFree(z, s);                                                  // 0x1009e2ec
    return Z_OK;                                                  // 0x1009e2f9
}

// ------------------------------------------------------------------------- inflateReset 0x0009bdd0
//
// int __cdecl inflateReset(z_stream *z): z [ebp+8]; `ret` with no pop at 0x1009be60.
//
// The guard is two tests with one shared arm: a null z (0x1009bde8) or a null z->state at +0x1c
// (0x1009bdf1) returns Z_STREAM_ERROR (0x1009bdf7). Then
//   * z->total_out (+0x14) = 0 (0x1009be01) BEFORE z->total_in (+8) = 0 (0x1009be0b) - the source's
//     `z->total_in = z->total_out = 0`, whose inner assignment is compiled first - and z->msg = 0
//     (0x1009be15).
//   * state->mode (+0) becomes BLOCKS 7 when state->nowrap (+0xc) is non-zero and METHOD 0 otherwise.
//     That is compiled without a branch at 0x1009be25-0x1009be32: `neg; sbb eax,eax` turns any non-zero
//     into -1 and zero into 0, and `and eax,7` then selects 7 or 0.
//   * inflate_blocks_reset(state->blocks (+0x14), z, &c) at 0x1009be46, with a local dword at [ebp-4] as
//     the check-value sink (0x1009be34): the reset's first arm writes it and inflateReset ignores it.
// Returns Z_OK (0x1009be4e).
InflateReset_t InflateReset_orig;

int __cdecl InflateReset_re(void* z) {
    if (z == 0 || P32R(z, Z_STATE) == 0) {                        // 0x1009bde8, 0x1009bdf1
        return Z_STREAM_ERROR;                                    // 0x1009bdf7
    }
    void* st = P32R(z, Z_STATE);
    U32(z, Z_TOTAL_OUT) = 0;                                      // 0x1009be01
    U32(z, Z_TOTAL_IN) = 0;                                       // 0x1009be0b
    P32(z, Z_MSG) = 0;                                            // 0x1009be15
    U32(st, S_MODE) = (U32R(st, S_NOWRAP) != 0) ? SM_BLOCKS : SM_METHOD;   // 0x1009be25-0x1009be32
    unsigned long c;
    reinterpret_cast<InflateBlocksReset_t>(JgldVA(VA_BLOCKS_RESET))(
        P32R(st, S_BLOCKS), z, &c);                               // 0x1009be46
    return Z_OK;                                                  // 0x1009be4e
}

// --------------------------------------------------------------------------- inflateEnd 0x0009be70
//
// int __cdecl inflateEnd(z_stream *z): z [ebp+8]; `ret` with no pop at 0x1009bf09.
//
// Three guards share one arm: a null z (0x1009be88), a null z->state (0x1009be91) or a null z->zfree at
// +0x24 (0x1009be9a) returns Z_STREAM_ERROR (0x1009bea0). The zfree test is what makes the rest safe:
// everything below frees.
//   * if state->blocks (+0x14) is not null (0x1009bead), inflate_blocks_free(blocks, z, &c) at
//     0x1009bec5 with a local dword at [ebp-4] for the check value (0x1009beb3).
//   * ZFREE(z, z->state) at 0x1009bee0 and then z->state = 0 (0x1009bef0), in that order, so the caller
//     is left with a stream that inflateReset and a second inflateEnd both refuse.
// Returns Z_OK (0x1009bef7).
InflateEnd_t InflateEnd_orig;

int __cdecl InflateEnd_re(void* z) {
    if (z == 0 || P32R(z, Z_STATE) == 0 || P32R(z, Z_ZFREE) == 0) {   // 0x1009be88, be91, be9a
        return Z_STREAM_ERROR;                                    // 0x1009bea0
    }
    void* st = P32R(z, Z_STATE);
    if (P32R(st, S_BLOCKS) != 0) {                                // 0x1009bead
        unsigned long c;
        reinterpret_cast<InflateBlocksFree_t>(JgldVA(VA_BLOCKS_FREE))(
            P32R(st, S_BLOCKS), z, &c);                           // 0x1009bec5
    }
    ZFree(z, st);                                                 // 0x1009bee0
    P32(z, Z_STATE) = 0;                                          // 0x1009bef0
    return Z_OK;                                                  // 0x1009bef7
}

// ------------------------------------------------------------------------ inflateInit2_ 0x0009bf10
//
// int __cdecl inflateInit2_(z_stream *z, int w, const char *version, int stream_size): z [ebp+8],
// w [ebp+0xc], version [ebp+0x10], stream_size [ebp+0x14]; `ret` with no pop at 0x1009c0a2.
//
// The version handshake first: a null version (0x1009bf28), a first byte that differs from the module's
// own ZLIB_VERSION[0] at 0x1011dba8 (both sign-extended and compared at 0x1009bf31-0x1009bf3b) or a
// stream_size other than 0x38 (0x1009bf3f) returns Z_VERSION_ERROR (0x1009bf45). A null z then returns
// Z_STREAM_ERROR (0x1009bf53).
//   * z->msg = 0 (0x1009bf62).
//   * if z->zalloc (+0x20) is null (0x1009bf6c) it is set to zcalloc 0x1009e680 and the opaque at +0x28
//     is cleared (0x1009bf75, 0x1009bf7f); if z->zfree (+0x24) is null (0x1009bf89) it is set to zcfree
//     0x1009e6d0 (0x1009bf92). After this point the stream always has a pair of allocators.
//   * z->state = ZALLOC(z, 1, 0x18) (0x1009bfa9, stored at 0x1009bfb9); a null one returns Z_MEM_ERROR
//     (0x1009bfc5) and leaves z->state null.
//   * state->blocks (+0x14) = 0 (0x1009bfd5) and state->nowrap (+0xc) = 0 (0x1009bfe2).
//   * a negative w (0x1009bfe9) is negated in place (0x1009bff2, 0x1009bff4) and sets nowrap = 1
//     (0x1009bffd): the undocumented raw-deflate option.
//   * w outside 8..15 (0x1009c004 and 0x1009c00a) calls inflateEnd(z) at 0x1009c014 - which frees the
//     state just allocated, which is why the allocators were installed first - and returns
//     Z_STREAM_ERROR (0x1009c01c).
//   * state->wbits (+0x10) = w (0x1009c02c) and state->blocks = inflate_blocks_new(z, nowrap ? NULL :
//     adler32 0x1009e370, 1 << w) at 0x1009c054. The window size is computed with `shl edx,cl` at
//     0x1009c037 and the check function is selected without a branch at 0x1009c040-0x1009c049, the same
//     `neg/sbb/not/and` idiom as inflateReset's mode, masking the adler32 address to 0 when nowrap is
//     set. A null result calls inflateEnd(z) at 0x1009c075 and returns Z_MEM_ERROR (0x1009c07d).
//   * inflateReset(z) at 0x1009c088 and Z_OK (0x1009c090).
InflateInit2_t InflateInit2_orig;

int __cdecl InflateInit2_re(void* z, int w, const char* version, int stream_size) {
    const char* zlibVersion = static_cast<const char*>(JgldVA(0x1011dba8));
    if (version == 0 || version[0] != zlibVersion[0] || stream_size != 0x38) {  // bf28, bf3b, bf3f
        return Z_VERSION_ERROR;                                   // 0x1009bf45
    }
    if (z == 0) {                                                 // 0x1009bf4f
        return Z_STREAM_ERROR;                                    // 0x1009bf55
    }
    P32(z, Z_MSG) = 0;                                            // 0x1009bf62
    if (P32R(z, Z_ZALLOC) == 0) {                                 // 0x1009bf6c
        P32(z, Z_ZALLOC) = JgldVA(0x1009e680);                    // 0x1009bf75  zcalloc
        P32(z, Z_OPAQUE) = 0;                                     // 0x1009bf7f
    }
    if (P32R(z, Z_ZFREE) == 0) {                                  // 0x1009bf89
        P32(z, Z_ZFREE) = JgldVA(0x1009e6d0);                     // 0x1009bf92  zcfree
    }
    P32(z, Z_STATE) = ZAlloc(z, 1, 0x18);                         // 0x1009bfa9, 0x1009bfb9
    if (P32R(z, Z_STATE) == 0) {                                  // 0x1009bfbf
        return Z_MEM_ERROR;                                       // 0x1009bfc5
    }
    void* st = P32R(z, Z_STATE);
    P32(st, S_BLOCKS) = 0;                                        // 0x1009bfd5
    U32(st, S_NOWRAP) = 0;                                        // 0x1009bfe2
    if (w < 0) {                                                  // 0x1009bfe9
        w = -w;                                                   // 0x1009bff2
        U32(st, S_NOWRAP) = 1;                                    // 0x1009bffd
    }
    if (w < 8 || w > 15) {                                        // 0x1009c004, 0x1009c00a
        reinterpret_cast<InflateEnd_t>(JgldVA(VA_INFLATE_END))(z);   // 0x1009c014
        return Z_STREAM_ERROR;                                    // 0x1009c01c
    }
    U32(st, S_WBITS) = static_cast<unsigned>(w);                  // 0x1009c02c
    void* checkfn = (U32R(st, S_NOWRAP) != 0) ? 0 : JgldVA(0x1009e370);   // 0x1009c040-0x1009c049
    P32(st, S_BLOCKS) = reinterpret_cast<InflateBlocksNew_t>(JgldVA(VA_BLOCKS_NEW))(
        z, checkfn, 1u << w);                                     // 0x1009c054, 0x1009c062
    if (P32R(st, S_BLOCKS) == 0) {                                // 0x1009c06b
        reinterpret_cast<InflateEnd_t>(JgldVA(VA_INFLATE_END))(z);   // 0x1009c075
        return Z_MEM_ERROR;                                       // 0x1009c07d
    }
    reinterpret_cast<InflateReset_t>(JgldVA(VA_INFLATE_RESET))(z);   // 0x1009c088
    return Z_OK;                                                  // 0x1009c090
}

// ------------------------------------------------------------------------- inflateInit_ 0x0009c0b0
//
// int __cdecl inflateInit_(z_stream *z, const char *version, int stream_size): z [ebp+8],
// version [ebp+0xc], stream_size [ebp+0x10]; `ret` with no pop at 0x1009c0ee.
//
// One forwarding call and no conditional jump at all: it pushes stream_size (0x1009c0c8), version
// (0x1009c0cc), the literal 0xf (0x1009c0d0, DEF_WBITS - a 32 KiB window) and z (0x1009c0d5), calls
// inflateInit2_ at 0x1009c0d6, pops its sixteen bytes (0x1009c0db) and returns what came back in eax.
typedef int(__cdecl* InflateInit_t)(void*, const char*, int);
InflateInit_t InflateInit_orig;

int __cdecl InflateInit_re(void* z, const char* version, int stream_size) {
    return reinterpret_cast<InflateInit2_t>(JgldVA(VA_INFLATE_INIT2))(
        z, 0xf, version, stream_size);                            // 0x1009c0d6
}

}  // namespace

// 0x0009f030  inflate_trees_fixed
SG_HOOK("jgld.dll", 0x0009f030, inflate_trees_fixed, InflateTreesFixed_re, InflateTreesFixed_orig);
// 0x0009f210  falloc
SG_HOOK("jgld.dll", 0x0009f210, falloc, Falloc_re, Falloc_orig);
// 0x0009e710  inflate_trees_bits
SG_HOOK("jgld.dll", 0x0009e710, inflate_trees_bits, InflateTreesBits_re, InflateTreesBits_orig);
// 0x0009ef00  inflate_trees_dynamic
SG_HOOK("jgld.dll", 0x0009ef00, inflate_trees_dynamic, InflateTreesDynamic_re, InflateTreesDynamic_orig);
// 0x0009f2f0  inflate_codes_new
SG_HOOK("jgld.dll", 0x0009f2f0, inflate_codes_new, InflateCodesNew_re, InflateCodesNew_orig);
// 0x000a00e0  inflate_codes_free
SG_HOOK("jgld.dll", 0x000a00e0, inflate_codes_free, InflateCodesFree_re, InflateCodesFree_orig);
// 0x0009cc70  inflate_blocks_reset
SG_HOOK("jgld.dll", 0x0009cc70, inflate_blocks_reset, InflateBlocksReset_re, InflateBlocksReset_orig);
// 0x0009cd90  inflate_blocks_new
SG_HOOK("jgld.dll", 0x0009cd90, inflate_blocks_new, InflateBlocksNew_re, InflateBlocksNew_orig);
// 0x0009e290  inflate_blocks_free
SG_HOOK("jgld.dll", 0x0009e290, inflate_blocks_free, InflateBlocksFree_re, InflateBlocksFree_orig);
// 0x0009bdd0  inflateReset
SG_HOOK("jgld.dll", 0x0009bdd0, inflateReset, InflateReset_re, InflateReset_orig);
// 0x0009be70  inflateEnd
SG_HOOK("jgld.dll", 0x0009be70, inflateEnd, InflateEnd_re, InflateEnd_orig);
// 0x0009bf10  inflateInit2_
SG_HOOK("jgld.dll", 0x0009bf10, inflateInit2_, InflateInit2_re, InflateInit2_orig);
// 0x0009c0b0  inflateInit_
SG_HOOK("jgld.dll", 0x0009c0b0, inflateInit_, InflateInit_re, InflateInit_orig);

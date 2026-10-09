// Batch c3ch: jgld.dll - the top of the zlib 1.0.2 inflate chain and libpng 1.0.5's chunk reader.
//   huft_build      0x0009e7a0
//   inflate_blocks  0x0009ce80
//   inflate         0x0009c0f0
//   png_read_data   0x00078c40
//   png_crc_read    0x000794b0
//   png_crc_error   0x00079620
//   png_crc_finish  0x00079510
//
// SG_HOOK addresses are RVAs; every instruction cited in a comment is a VA (0x10000000 + RVA), the form
// the debug build's disassembly prints. jgld.dll is LoadLibrary'd after the shim starts, so these hooks
// install from the LoadLibraryA hook (SgHooksInstallPending, re/hooks.cpp) and diff_hook finds them by
// (module, rva) through SimGolfShim_FindHookM. The module is relocated at run time (c3cb measured base
// 0x03930000 in one boot and 0x03980000 in another, log/c3/alt/c3cb_probe.py), so every address this file
// reaches is resolved against the running base through JgldVA.
//
// WHY THE ZLIB THREE CAN BE A/B'd. zlib never calls the heap directly: every allocation goes through the
// z_stream's zalloc at +0x20 and every free through its zfree at +0x24, both with the opaque at +0x28, and
// the z_stream belongs to the CALLER. The fixture (re/frida/js/fixtures.d/c3ch.js) puts a slot allocator
// and a recording deallocator of its own there, the way c3cb did one level down, so the two arms hand back
// the same addresses in the same order and nothing reaches operator new or operator delete.
//
// WHY THE PNG FOUR CAN BE A/B'd. png_read_data reaches the input only through the png_struct's
// read_data_fn at +0x50, which png_set_read_fn 0x10078cb0 (C3, c3bq) installs, so the fixture's reader
// serves bytes from a buffer of ours and records its cursor; png_calculate_crc 0x100787d0 (C3, c3ao) and
// crc32 0x1009ca70 (C3, c3ax) are pure. The one thing these four can do that an A/B cannot follow is
// png_error 0x10078fe0: it calls the handler at png_struct+0x40 and then falls into png_default_error
// 0x100792b0 whatever that handler does (0x1007901b is not guarded), which longjmps through the
// png_struct's jmpbuf. Those arms are listed in log/c3/c3ch_purpose.md and no vector takes them.
//
// WHAT THE /GZ STACK FILL MEANS HERE. All four jgld.dll builds are debug (/Od /ZI /GZ): each prologue
// fills its whole frame with 0xcccccccc (huft_build at 0x1009e7ac-0x1009e7bc, 0x5c4 bytes). Two bodies
// below DEPEND on that fill and the reimplementations reproduce it:
//   * huft_build's table entry `r` is 8 bytes of which only bytes 0, 1 and 4..7 are ever written, so every
//     entry it stores carries 0xcc in bytes 2 and 3;
//   * on a branch that writes no base, `r` keeps the value the previous entry left, so `r` is one variable
//     for the whole body and is never re-initialised.
#include "hooks.h"
#include <string.h>

namespace {

// jgld.dll is relocatable, so an address the code calls, stores or reads is resolved against the running
// base (c3cb's JgldVA; a hard-coded 0x10xxxxxx would read another module).
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
inline unsigned char U8R(const void* p, unsigned off) {
    return *reinterpret_cast<const unsigned char*>(static_cast<const char*>(p) + off);
}
inline void*& P32(void* p, unsigned off) {
    return *reinterpret_cast<void**>(static_cast<char*>(p) + off);
}
inline void* P32R(const void* p, unsigned off) {
    return *reinterpret_cast<void* const*>(static_cast<const char*>(p) + off);
}

// ---------------------------------------------------------------------------------- shared layouts
//
// z_stream, 0x38 bytes: next_in +0, avail_in +4, total_in +8, next_out +0xc, avail_out +0x10,
// total_out +0x14, msg +0x18, state +0x1c, zalloc +0x20, zfree +0x24, opaque +0x28, data_type +0x2c,
// adler +0x30, reserved +0x34 (the same layout c3cb's inflateInit2_ checks at 0x1009bf3f).
const unsigned Z_NEXT_IN = 0x00, Z_AVAIL_IN = 0x04, Z_TOTAL_IN = 0x08, Z_MSG = 0x18, Z_STATE = 0x1c;
const unsigned Z_ZALLOC = 0x20, Z_ZFREE = 0x24, Z_OPAQUE = 0x28, Z_ADLER = 0x30;

// internal_state (inflate's own), 0x18 bytes: mode +0, sub +4 (method +4, check.was +4, check.need +8,
// marker +4), nowrap +0xc, wbits +0x10, blocks +0x14.
const unsigned S_MODE = 0x00, S_METHOD = 0x04, S_MARKER = 0x04, S_WAS = 0x04, S_NEED = 0x08;
const unsigned S_NOWRAP = 0x0c, S_WBITS = 0x10, S_BLOCKS = 0x14;

// inflate_blocks_state, 0x3c bytes: mode +0, the sub union +4..+0x17 (left +4; trees.table +4,
// trees.index +8, trees.blens +0xc, trees.bb +0x10, trees.tb +0x14; decode.tl +4, decode.td +8,
// decode.codes +0xc), last +0x18, bitk +0x1c, bitb +0x20, window +0x24, end +0x28, read +0x2c,
// write +0x30, checkfn +0x34, check +0x38.
const unsigned B_MODE = 0x00, B_LEFT = 0x04, B_TABLE = 0x04, B_TL = 0x04, B_INDEX = 0x08, B_TD = 0x08;
const unsigned B_BLENS = 0x0c, B_CODES = 0x0c, B_BB = 0x10, B_TB = 0x14, B_LAST = 0x18;
const unsigned B_BITK = 0x1c, B_BITB = 0x20, B_WINDOW = 0x24, B_END = 0x28, B_READ = 0x2c;
const unsigned B_WRITE = 0x30;

// zlib return codes, as the immediates in the bodies: Z_OK 0, Z_STREAM_END 1 (0x1009e11f),
// Z_STREAM_ERROR -2 (0x1009e1e0), Z_DATA_ERROR -3 (0x1009d155), Z_MEM_ERROR -4 (0x1009d07a),
// Z_BUF_ERROR -5 (0x1009eece).
const int Z_OK = 0, Z_STREAM_END = 1, Z_STREAM_ERROR = -2, Z_DATA_ERROR = -3;
const int Z_MEM_ERROR = -4, Z_BUF_ERROR = -5;

// inflate_blocks modes, as the stores in the body: TYPE 0 (0x1009e021), LENS 1 (0x1009d02c),
// STORED 2 (0x1009d319), TABLE 3 (0x1009d125), BTREE 4, DTREE 5, CODES 6 (0x1009defd),
// DRY 7 (0x1009e050), DONE 8 (0x1009e119), BAD 9 (0x1009d145).
const unsigned BM_TYPE = 0, BM_LENS = 1, BM_STORED = 2, BM_TABLE = 3, BM_BTREE = 4, BM_DTREE = 5;
const unsigned BM_CODES = 6, BM_DRY = 7, BM_DONE = 8, BM_BAD = 9;

typedef void*(__cdecl* ZAlloc_t)(void* opaque, unsigned items, unsigned size);
typedef void(__cdecl* ZFree_t)(void* opaque, void* addr);

// ZALLOC(z, items, size) and ZFREE(z, addr), the two zutil.h macros as they are compiled here: an indirect
// __cdecl call through the z_stream with the opaque as the first argument.
inline void* ZAlloc(void* z, unsigned items, unsigned size) {
    return reinterpret_cast<ZAlloc_t>(P32R(z, Z_ZALLOC))(P32R(z, Z_OPAQUE), items, size);
}
inline void ZFree(void* z, void* addr) {
    reinterpret_cast<ZFree_t>(P32R(z, Z_ZFREE))(P32R(z, Z_OPAQUE), addr);
}

// jgld.dll's own routines these bodies call, by VA.
typedef int(__cdecl* InflateTreesFree_t)(void* t, void* z);
typedef int(__cdecl* InflateTreesBits_t)(unsigned* c, unsigned* bb, void** tb, void* z);
typedef int(__cdecl* InflateTreesDynamic_t)(unsigned nl, unsigned nd, unsigned* c, unsigned* bl,
                                            unsigned* bd, void** tl, void** td, void* z);
typedef int(__cdecl* InflateTreesFixed_t)(unsigned* bl, unsigned* bd, void** tl, void** td);
typedef void*(__cdecl* InflateCodesNew_t)(unsigned bl, unsigned bd, void* tl, void* td, void* z);
typedef void(__cdecl* InflateCodesFree_t)(void* c, void* z);
typedef int(__cdecl* InflateCodes_t)(void* s, void* z, int r);
typedef int(__cdecl* InflateFlush_t)(void* s, void* z, int r);
typedef int(__cdecl* InflateBlocks_t)(void* s, void* z, int r);
typedef void(__cdecl* InflateBlocksReset_t)(void* s, void* z, unsigned long* c);
typedef void*(__cdecl* Memcpy_t)(void* dst, const void* src, unsigned n);

const unsigned VA_TREES_FREE = 0x1009f250;
const unsigned VA_TREES_BITS = 0x1009e710;
const unsigned VA_TREES_DYNAMIC = 0x1009ef00;
const unsigned VA_TREES_FIXED = 0x1009f030;
const unsigned VA_CODES_NEW = 0x1009f2f0;
const unsigned VA_CODES_FREE = 0x100a00e0;
const unsigned VA_CODES = 0x1009f370;
const unsigned VA_FLUSH = 0x100a0130;
const unsigned VA_BLOCKS = 0x1009ce80;
const unsigned VA_BLOCKS_RESET = 0x1009cc70;
const unsigned VA_ZMEMCPY = 0x1007f3a0;
const unsigned VA_BORDER = 0x10127218;       // uInt border[19], read at 0x1009d897 and 0x1009d8dc
const unsigned VA_INFLATE_MASK = 0x101274a4; // uInt inflate_mask[17], read at 0x1009da9c and 0x1009dbfb

// ======================================================================== huft_build 0x0009e7a0
//
// local int __cdecl huft_build(uIntf *b, uInt n, uInt s, uIntf *d, uIntf *e, inflate_huft **t, uIntf *m,
// z_stream *zs): b [ebp+8], n [ebp+0xc], s [ebp+0x10], d [ebp+0x14], e [ebp+0x18], t [ebp+0x1c],
// m [ebp+0x20], zs [ebp+0x24]; `ret` with no pop at 0x1009eefd, so __cdecl.
//
// It turns a list of code lengths into the multi-level lookup tables inflate_codes walks, allocating each
// table through the CALLER's zalloc. Its three callers are inflate_trees_bits 0x1009e710,
// inflate_trees_dynamic 0x1009ef00 and inflate_trees_fixed 0x1009f030 (all C3, c3cb).
//
// An inflate_huft is 8 bytes: Exop at +0, Bits at +1, two padding bytes at +2 (the union's other arm is a
// pointer), and Base or Next at +4. The frame variable `r` holding one entry lives at [ebp-0x70] and only
// its bytes 0, 1 and 4..7 are ever written (0x1009ed37, 0x1009ed3d, 0x1009ed43, 0x1009ed7f, 0x1009ed91,
// 0x1009edb2, 0x1009edba, 0x1009edd9, 0x1009edea), so every entry this function stores carries the /GZ
// prologue fill 0xcc in bytes 2 and 3, and on the `out of values` branch at 0x1009ed91 it also carries
// whatever base or next the PREVIOUS entry left at +4. Both facts are reproduced below.
//
// The frame layout the comments refer to: c[16] at [ebp-0x44], v[288] at [ebp-0x52c], x[16] at
// [ebp-0x570], u[15] at [ebp-0xac], p [ebp-0x64], xp [ebp-0x574], i [ebp-0x54], j [ebp-0x58],
// k [ebp-0x5c], l [ebp-0x60], g [ebp-0x4c], h [ebp-0x50], w [ebp-0x530], a [ebp-4], f [ebp-0x48],
// y [ebp-0x578], z [ebp-0x57c], q [ebp-0x68].
typedef int(__cdecl* HuftBuild_t)(unsigned* b, unsigned n, unsigned s, const unsigned* d,
                                  const unsigned* e, void** t, unsigned* m, void* zs);
HuftBuild_t HuftBuild_orig;

int __cdecl HuftBuild_re(unsigned* b, unsigned n, unsigned s, const unsigned* d, const unsigned* e,
                         void** t, unsigned* m, void* zs) {
    unsigned a;                 // [ebp-4]     codes of length k left to place
    unsigned c[16];             // [ebp-0x44]  bit length count table (BMAX+1 entries)
    unsigned f;                 // [ebp-0x48]
    int g;                      // [ebp-0x4c]  maximum code length
    int h;                      // [ebp-0x50]  table level
    unsigned i;                 // [ebp-0x54]  current code
    unsigned j;                 // [ebp-0x58]
    int k;                      // [ebp-0x5c]  bits in the current code
    int l;                      // [ebp-0x60]  bits per table
    unsigned* p;                // [ebp-0x64]
    unsigned char* q;           // [ebp-0x68]  current table
    // One inflate_huft at [ebp-0x70]: Exop at +0, Bits at +1, two padding bytes, Base or Next at +4.
    // The union only guarantees the 4-byte alignment the original's stack slot has.
    union { unsigned char b[8]; unsigned dw[2]; } r;
    unsigned char* u[15];       // [ebp-0xac]  table stack
    unsigned v[288];            // [ebp-0x52c] values in order of bit length
    int w;                      // [ebp-0x530]
    unsigned x[16];             // [ebp-0x570] bit offsets, then code stack
    unsigned* xp;               // [ebp-0x574]
    int y;                      // [ebp-0x578] dummy codes added
    unsigned z;                 // [ebp-0x57c] entries in the current table

    // The /GZ prologue at 0x1009e7ac-0x1009e7bc fills the whole 0x5c4-byte frame with 0xcccccccc. Only
    // these four arrays and r are read before being written anywhere in the body, so only they matter.
    memset(r.b, 0xcc, sizeof(r.b));
    memset(v, 0xcc, sizeof(v));
    memset(x, 0xcc, sizeof(x));
    memset(u, 0xcc, sizeof(u));

    p = c;                                                        // 0x1009e7be
    for (int ci = 0; ci < 16; ci++) *p++ = 0;                     // the C4 macro, 16 unrolled stores
    p = b;                                                        // 0x1009e8e1
    i = n;
    do {
        c[*p++]++;                                                // 0x1009e8f0-0x1009e901
    } while (--i);                                                // 0x1009e91b
    if (c[0] == n) {                                              // 0x1009e923
        *t = 0;                                                   // 0x1009e928
        *m = 0;                                                   // 0x1009e931
        return Z_OK;                                              // 0x1009e937
    }

    l = static_cast<int>(*m);                                     // 0x1009e93e
    for (j = 1; j <= 15; j++)                                     // 0x1009e95c  (BMAX = 15)
        if (c[j]) break;                                          // 0x1009e966
    k = static_cast<int>(j);                                      // 0x1009e96c
    if (static_cast<unsigned>(l) < j) l = static_cast<int>(j);    // 0x1009e978
    for (i = 15; i; i--)                                          // 0x1009e996
        if (c[i]) break;                                          // 0x1009e9a0
    g = static_cast<int>(i);                                      // 0x1009e9a6
    if (static_cast<unsigned>(l) > i) l = static_cast<int>(i);    // 0x1009e9b2
    *m = static_cast<unsigned>(l);                                // 0x1009e9ba

    for (y = 1 << j; j < i; j++, y <<= 1)                         // 0x1009e9f1
        if ((y -= static_cast<int>(c[j])) < 0) return Z_DATA_ERROR;   // 0x1009ea0d
    if ((y -= static_cast<int>(c[i])) < 0) return Z_DATA_ERROR;   // 0x1009ea35
    c[i] += static_cast<unsigned>(y);                             // 0x1009ea41

    x[1] = j = 0;                                                 // 0x1009ea55
    p = c + 1;                                                    // 0x1009ea65
    xp = x + 2;                                                   // 0x1009ea6b
    while (--i) {                                                 // 0x1009ea84
        *xp++ = (j += *p++);                                      // 0x1009ea86-0x1009eab1
    }

    p = b;                                                        // 0x1009eab6
    i = 0;
    do {
        if ((j = *p++) != 0)                                      // 0x1009ead9
            v[x[j]++] = i;                                        // 0x1009eae8, 0x1009eaff
    } while (++i < n);                                            // 0x1009eb15

    x[0] = i = 0;                                                 // 0x1009eb17
    p = v;                                                        // 0x1009eb27
    h = -1;                                                       // 0x1009eb30
    w = -l;                                                       // 0x1009eb3a
    u[0] = 0;                                                     // 0x1009eb42
    q = 0;                                                        // 0x1009eb4c
    z = 0;                                                        // 0x1009eb53

    for (; k <= g; k++) {                                         // 0x1009eb6e
        a = c[k];                                                 // 0x1009eb7b
        while (a--) {                                             // 0x1009eb8c  (tests the value before --)
            while (k > w + l) {                                   // 0x1009eb9e
                h++;                                              // 0x1009eba7
                w += l;                                           // 0x1009ebb6
                z = static_cast<unsigned>(g - w);                 // 0x1009ebc5
                if (z > static_cast<unsigned>(l)) z = static_cast<unsigned>(l);   // 0x1009ebd4
                j = static_cast<unsigned>(k - w);                 // 0x1009ec02
                f = 1u << j;                                      // 0x1009ec0f
                if (f > a + 1) {                                  // 0x1009ec1b
                    f -= a + 1;                                   // 0x1009ec28
                    xp = c + k;                                   // 0x1009ec32
                    if (j < z) {                                  // 0x1009ec41
                        while (++j < z) {                         // 0x1009ec55
                            f <<= 1;                              // 0x1009ec5a
                            ++xp;                                 // 0x1009ec65
                            if (f <= *xp) break;                  // 0x1009ec79
                            f -= *xp;                             // 0x1009ec86
                        }
                    }
                }
                z = 1u << j;                                      // 0x1009ec97
                // ZALLOC(zs, z + 1, sizeof(inflate_huft)): the 8 and the z+1 pushed at 0x1009ec9f and
                // 0x1009ecaa, the opaque read from zs+0x28, the indirect call at 0x1009ecb5.
                q = static_cast<unsigned char*>(ZAlloc(zs, z + 1, 8));
                if (q == 0) {                                     // 0x1009ecc9
                    if (h)                                        // 0x1009eccf
                        reinterpret_cast<InflateTreesFree_t>(JgldVA(VA_TREES_FREE))(u[0], zs);  // 0x1009ecdc
                    return Z_MEM_ERROR;                           // 0x1009ece4
                }
                *t = q + 8;                                       // 0x1009ecf7  *t = q + 1 entry
                t = reinterpret_cast<void**>(q + 4);              // 0x1009ecff  t = &(q->next)
                *t = 0;                                           // 0x1009ed05
                q += 8;                                           // 0x1009ed11  ++q: the table starts
                u[h] = q;                                         // 0x1009ed1a   after the link entry
                if (h) {                                          // 0x1009ed25
                    x[h] = i;                                     // 0x1009ed2d
                    r.b[1] = static_cast<unsigned char>(l);       // 0x1009ed37  r.bits
                    r.b[0] = static_cast<unsigned char>(j);       // 0x1009ed3d  r.exop
                    r.dw[1] = reinterpret_cast<unsigned>(q);      // 0x1009ed43  r.next
                    j = i >> (w - l);                             // 0x1009ed52
                    memcpy(u[h - 1] + j * 8, r.b, 8);             // 0x1009ed67, 0x1009ed6d
                }
            }

            r.b[1] = static_cast<unsigned char>(k - w);           // 0x1009ed7f  r.bits
            if (p >= v + n) {                                     // 0x1009ed8f
                r.b[0] = 0xc0;                                    // 0x1009ed91  128 + 64, invalid code;
                                                                  //   r+4 keeps the previous entry's value
            } else if (*p < s) {                                  // 0x1009ed9f
                // `*p < 256 ? 0 : 32 + 64`, compiled without a branch: `cmp *p,0x100; sbb ecx,ecx;
                // and ecx,0xffffffa0; add ecx,0x60` at 0x1009eda4-0x1009edaf.
                r.b[0] = static_cast<unsigned char>(*p < 256 ? 0 : 0x60);   // 0x1009edb2
                r.dw[1] = *p;                                     // 0x1009edba  r.base
                p++;                                              // 0x1009edc0
            } else {
                r.b[0] = static_cast<unsigned char>(e[*p - s] + 0x50);      // 0x1009edd9  16 + 64
                r.dw[1] = d[*p - s];                              // 0x1009edea  r.base
                p++;                                              // 0x1009edf0
            }

            f = 1u << (k - w);                                    // 0x1009ee06
            for (j = i >> w; j < z; j += f)                       // 0x1009ee2b
                memcpy(q + j * 8, r.b, 8);                        // 0x1009ee36, 0x1009ee3c
            for (j = 1u << (k - 1); i & j; j >>= 1)               // 0x1009ee64
                i ^= j;                                           // 0x1009ee69
            i ^= j;                                               // 0x1009ee74
            while ((i & ((1u << w) - 1)) != x[h]) {               // 0x1009ee99
                h--;                                              // 0x1009eea1
                w -= l;                                           // 0x1009eead
            }
        }
    }
    // `y != 0 && g != 1 ? Z_BUF_ERROR : Z_OK`: an incomplete set is reported unless the whole set is one
    // one-bit code (0x1009eec6, 0x1009eecc).
    return (y != 0 && g != 1) ? Z_BUF_ERROR : Z_OK;               // 0x1009eece / 0x1009eeda
}

// ==================================================================== inflate_blocks 0x0009ce80
//
// int __cdecl inflate_blocks(inflate_blocks_statef *s, z_stream *z, int r): s [ebp+8], z [ebp+0xc],
// r [ebp+0x10]; `ret` with no pop at 0x1009e257. Its only caller is inflate 0x1009c0f0.
//
// The deflate block-level state machine: an endless `while (1) switch (s->mode)` over ten modes, each exit
// going through inflate_flush 0x100a0130. The bit buffer b, the bit count k, the input cursor p, the
// bytes left n, the window write cursor q and the room m are LOCALS loaded at entry (0x1009ce9e-0x1009cef5)
// and written back into s and z by the UPDATE block that precedes every inflate_flush call, so a caller
// only ever sees the structure fields, never the locals.
//
// The frame: t [ebp-4], b [ebp-8], k [ebp-0xc], p [ebp-0x10], n [ebp-0x14], q [ebp-0x18], m [ebp-0x1c];
// the DTREE block's own h [ebp-0x30], i [ebp-0x34], j [ebp-0x38], c [ebp-0x3c]; the fixed-block bl, bd,
// tl, td at [ebp-0x20], [ebp-0x24], [ebp-0x28], [ebp-0x2c] and the dynamic ones at [ebp-0x40],
// [ebp-0x44], [ebp-0x48], [ebp-0x4c] with the new codes state at [ebp-0x50].
InflateBlocks_t InflateBlocks_orig;

int __cdecl InflateBlocks_re(void* s, void* z, int r) {
    InflateFlush_t flush = reinterpret_cast<InflateFlush_t>(JgldVA(VA_FLUSH));
    InflateTreesFree_t freeTrees = reinterpret_cast<InflateTreesFree_t>(JgldVA(VA_TREES_FREE));
    const unsigned* border = static_cast<const unsigned*>(JgldVA(VA_BORDER));
    const unsigned* mask = static_cast<const unsigned*>(JgldVA(VA_INFLATE_MASK));

    unsigned t;
    unsigned b;                 // uLong in the source; 32 bits here, as every `shr`/`or` in the body is
    unsigned k;
    unsigned char* p;
    unsigned n;
    unsigned char* q;
    unsigned m;

    // LOAD = LOADIN LOADOUT (0x1009ce9e-0x1009cef5). WAVAIL is the ternary at 0x1009ced3.
    p = static_cast<unsigned char*>(P32R(z, Z_NEXT_IN));
    n = U32R(z, Z_AVAIL_IN);
    b = U32R(s, B_BITB);
    k = U32R(s, B_BITK);
    q = static_cast<unsigned char*>(P32R(s, B_WRITE));
    m = static_cast<unsigned>(q < static_cast<unsigned char*>(P32R(s, B_READ))
                                  ? static_cast<unsigned char*>(P32R(s, B_READ)) - q - 1
                                  : static_cast<unsigned char*>(P32R(s, B_END)) - q);

    // UPDATE, written out at every LEAVE (first copy 0x1009cf3a-0x1009cf79).
    #define SG_UPDATE                                                                      \
        do {                                                                               \
            U32(s, B_BITB) = b;                                                            \
            U32(s, B_BITK) = k;                                                            \
            U32(z, Z_AVAIL_IN) = n;                                                        \
            U32(z, Z_TOTAL_IN) += static_cast<unsigned>(                                   \
                p - static_cast<unsigned char*>(P32R(z, Z_NEXT_IN)));                      \
            P32(z, Z_NEXT_IN) = p;                                                         \
            P32(s, B_WRITE) = q;                                                           \
        } while (0)
    #define SG_LEAVE    do { SG_UPDATE; return flush(s, z, r); } while (0)
    // NEEDBITS(j): the `while (k < j)` at e.g. 0x1009cf25 with NEEDBYTE's `if (n) r = Z_OK; else LEAVE`
    // at 0x1009cf2f and NEXTBYTE's `n--, *p++` at 0x1009cf95-0x1009cfc1.
    #define SG_NEEDBITS(jj)                                                                \
        while (k < static_cast<unsigned>(jj)) {                                            \
            if (n) { r = Z_OK; } else { SG_LEAVE; }                                        \
            n--;                                                                           \
            b |= static_cast<unsigned>(*p++) << k;                                         \
            k += 8;                                                                        \
        }
    #define SG_DUMPBITS(jj)  do { b >>= (jj); k -= (jj); } while (0)
    // WAVAIL, recomputed wherever the source's LOADOUT or WRAP macro does (0x1009d3d5, 0x1009d436,
    // 0x1009d486, 0x1009dff0, 0x1009e088).
    #define SG_WAVAIL                                                                      \
        (static_cast<unsigned>(q < static_cast<unsigned char*>(P32R(s, B_READ))            \
             ? static_cast<unsigned char*>(P32R(s, B_READ)) - q - 1                        \
             : static_cast<unsigned char*>(P32R(s, B_END)) - q))
    #define SG_WRAP                                                                        \
        do {                                                                               \
            if (q == static_cast<unsigned char*>(P32R(s, B_END)) &&                        \
                P32R(s, B_READ) != P32R(s, B_WINDOW)) {                                    \
                q = static_cast<unsigned char*>(P32R(s, B_WINDOW));                        \
                m = SG_WAVAIL;                                                             \
            }                                                                              \
        } while (0)

    for (;;) {                                                    // `while (1)`, 0x1009ceff / 0x1009e23f
        switch (U32R(s, B_MODE)) {                                // 0x1009cf11, jump table 0x1009e258
        case BM_TYPE: {
            SG_NEEDBITS(3)
            t = b & 7;                                            // 0x1009cfc9
            U32(s, B_LAST) = t & 1;                               // 0x1009cfdb
            switch (t >> 1) {                                     // 0x1009cfea, jump table 0x1009e280
            case 0:                                               // stored
                SG_DUMPBITS(3);                                   // 0x1009cffa-0x1009d009
                t = k & 7;                                        // 0x1009d012  go to a byte boundary
                SG_DUMPBITS(t);                                   // 0x1009d01b-0x1009d026
                U32(s, B_MODE) = BM_LENS;                         // 0x1009d02c
                break;
            case 1: {                                             // fixed
                unsigned bl, bd;
                void* tl;
                void* td;
                reinterpret_cast<InflateTreesFixed_t>(JgldVA(VA_TREES_FIXED))(
                    &bl, &bd, &tl, &td);                          // 0x1009d047
                P32(s, B_CODES) = reinterpret_cast<InflateCodesNew_t>(JgldVA(VA_CODES_NEW))(
                    bl, bd, tl, td, z);                           // 0x1009d063, stored 0x1009d06e
                if (P32R(s, B_CODES) == 0) {                      // 0x1009d078
                    r = Z_MEM_ERROR;                              // 0x1009d07a
                    SG_LEAVE;                                     // 0x1009d0cf
                }
                P32(s, B_TL) = 0;                                 // 0x1009d0df  don't free the fixed trees
                P32(s, B_TD) = 0;                                 // 0x1009d0e9
                SG_DUMPBITS(3);                                   // 0x1009d0f3-0x1009d0ff
                U32(s, B_MODE) = BM_CODES;                        // 0x1009d105
                break;
            }
            case 2:                                               // dynamic
                SG_DUMPBITS(3);                                   // 0x1009d113-0x1009d11f
                U32(s, B_MODE) = BM_TABLE;                        // 0x1009d125
                break;
            default:                                              // 3, illegal
                SG_DUMPBITS(3);                                   // 0x1009d133-0x1009d13f
                U32(s, B_MODE) = BM_BAD;                          // 0x1009d145
                P32(z, Z_MSG) = JgldVA(0x101208cc);               // 0x1009d14e
                r = Z_DATA_ERROR;                                 // 0x1009d155
                SG_LEAVE;                                         // 0x1009d1aa
            }
            break;
        }

        case BM_LENS:
            SG_NEEDBITS(32)                                       // 0x1009d1c0, 0x1009d1ca
            if ((((~b) >> 16) & 0xffff) != (b & 0xffff)) {        // 0x1009d27d
                U32(s, B_MODE) = BM_BAD;
                P32(z, Z_MSG) = JgldVA(0x101208ac);               // 0x1009d28b
                r = Z_DATA_ERROR;
                SG_LEAVE;                                         // 0x1009d2e7
            }
            U32(s, B_LEFT) = b & 0xffff;                          // 0x1009d2f4
            k = 0;                                                // 0x1009d303  `b = k = 0`, k stored first
            b = k;                                                // 0x1009d30a
            // `s->sub.left ? STORED : (s->last ? DRY : TYPE)`: the inner ternary is compiled without a
            // branch (`neg; sbb; and 7` at 0x1009d328-0x1009d32c), DRY being 7 and TYPE 0.
            U32(s, B_MODE) = U32R(s, B_LEFT) ? BM_STORED                                  // 0x1009d317
                                             : (U32R(s, B_LAST) ? BM_DRY : BM_TYPE);
            break;

        case BM_STORED:
            if (n == 0) SG_LEAVE;                                 // 0x1009d343, flush 0x1009d393
            // NEEDOUT: `if (m == 0) { WRAP if (m == 0) { FLUSH WRAP if (m == 0) LEAVE } } r = Z_OK;`
            if (m == 0) {                                         // 0x1009d3a4
                SG_WRAP;                                          // 0x1009d3b3, 0x1009d3c1, 0x1009d3d5
                if (m == 0) {                                     // 0x1009d3fe
                    P32(s, B_WRITE) = q;                          // 0x1009d40a  UPDOUT
                    r = flush(s, z, r);                           // 0x1009d419
                    q = static_cast<unsigned char*>(P32R(s, B_WRITE));    // 0x1009d42a  LOADOUT
                    m = SG_WAVAIL;                                // 0x1009d436
                    SG_WRAP;                                      // 0x1009d464, 0x1009d472, 0x1009d486
                    if (m == 0) SG_LEAVE;                         // 0x1009d4af, flush 0x1009d4ff
                }
            }
            r = Z_OK;                                             // 0x1009d50c
            t = U32R(s, B_LEFT);                                  // 0x1009d513
            if (t > n) t = n;                                     // 0x1009d522
            if (t > m) t = m;                                     // 0x1009d530
            reinterpret_cast<Memcpy_t>(JgldVA(VA_ZMEMCPY))(q, p, t);   // 0x1009d544  zmemcpy
            p += t; n -= t;
            q += t; m -= t;
            U32(s, B_LEFT) -= t;
            if (U32R(s, B_LEFT) != 0) break;                      // 0x1009d586
            U32(s, B_MODE) = U32R(s, B_LAST) ? BM_DRY : BM_TYPE;  // 0x1009d58d (branchless, as above)
            break;

        case BM_TABLE:
            SG_NEEDBITS(14)                                       // 0x1009d5a8, 0x1009d5b2
            t = b & 0x3fff;                                       // 0x1009d64f
            U32(s, B_TABLE) = t;                                  // 0x1009d65d
            // 29 is the highest legal count for either half; the two tests short-circuit.
            if ((t & 0x1f) > 29 || ((t >> 5) & 0x1f) > 29) {      // 0x1009d669, 0x1009d677
                U32(s, B_MODE) = BM_BAD;
                P32(z, Z_MSG) = JgldVA(0x10120888);               // 0x1009d685
                r = Z_DATA_ERROR;
                SG_LEAVE;                                         // 0x1009d6e1
            }
            t = 258 + (t & 0x1f) + ((t >> 5) & 0x1f);             // 0x1009d6f5-0x1009d704
            if (t < 19) t = 19;                                   // 0x1009d70b
            P32(s, B_BLENS) = ZAlloc(z, t, 4);                    // 0x1009d726, stored 0x1009d736
            if (P32R(s, B_BLENS) == 0) {                          // 0x1009d740
                r = Z_MEM_ERROR;                                  // 0x1009d742
                SG_LEAVE;                                         // 0x1009d797
            }
            SG_DUMPBITS(14);                                      // 0x1009d7ac-0x1009d7b8
            U32(s, B_INDEX) = 0;                                  // 0x1009d7be
            U32(s, B_MODE) = BM_BTREE;                            // 0x1009d7c4
            // falls through, as the source does

        case BM_BTREE: {
            unsigned* blens;
            while (U32R(s, B_INDEX) < 4 + (U32R(s, B_TABLE) >> 10)) {   // 0x1009d7db
                SG_NEEDBITS(3)                                    // 0x1009d7e5, 0x1009d7ef
                blens = static_cast<unsigned*>(P32R(s, B_BLENS));
                blens[border[U32R(s, B_INDEX)]] = b & 7;          // 0x1009d897-0x1009d8a9
                U32(s, B_INDEX)++;
                SG_DUMPBITS(3);                                   // 0x1009d8bc-0x1009d8c5
            }
            while (U32R(s, B_INDEX) < 19) {                       // 0x1009d8d4
                blens = static_cast<unsigned*>(P32R(s, B_BLENS));
                blens[border[U32R(s, B_INDEX)]] = 0;              // 0x1009d8dc-0x1009d8e9
                U32(s, B_INDEX)++;                                // 0x1009d8f6
            }
            U32(s, B_BB) = 7;                                     // 0x1009d904
            t = static_cast<unsigned>(reinterpret_cast<InflateTreesBits_t>(JgldVA(VA_TREES_BITS))(
                static_cast<unsigned*>(P32R(s, B_BLENS)),
                reinterpret_cast<unsigned*>(static_cast<char*>(s) + B_BB),
                reinterpret_cast<void**>(static_cast<char*>(s) + B_TB), z));   // 0x1009d924
            if (static_cast<int>(t) != Z_OK) {                    // 0x1009d933
                r = static_cast<int>(t);                          // 0x1009d938
                if (r == Z_DATA_ERROR) U32(s, B_MODE) = BM_BAD;   // 0x1009d93f, 0x1009d944
                SG_LEAVE;                                         // 0x1009d998
            }
            U32(s, B_INDEX) = 0;                                  // 0x1009d9ab
            U32(s, B_MODE) = BM_DTREE;                            // 0x1009d9b1
            // falls through
        }

        case BM_DTREE: {
            unsigned* blens;
            for (;;) {
                t = U32R(s, B_TABLE);                             // 0x1009d9b8
                if (U32R(s, B_INDEX) >= 258 + (t & 0x1f) + ((t >> 5) & 0x1f)) break;   // 0x1009d9dd
                unsigned char* hh;
                unsigned ii, jj, cc;

                t = U32R(s, B_BB);                                // 0x1009d9e6
                SG_NEEDBITS(t)                                    // 0x1009d9f2, 0x1009d9fc
                hh = static_cast<unsigned char*>(P32R(s, B_TB)) + (b & mask[t]) * 8;   // 0x1009da9c-0x1009daa9
                t = U8R(hh, 1);                                   // 0x1009dab4  h->word.what.Bits
                cc = U32R(hh, 4);                                 // 0x1009dabd  h->more.Base
                if (cc < 16) {                                    // 0x1009dac7
                    SG_DUMPBITS(t);                               // 0x1009dacf-0x1009dada
                    blens = static_cast<unsigned*>(P32R(s, B_BLENS));
                    blens[U32R(s, B_INDEX)] = cc;                 // 0x1009daec
                    U32(s, B_INDEX)++;                            // 0x1009daf5
                } else {                                          // cc is 16, 17 or 18
                    ii = (cc == 18) ? 7 : cc - 14;                // 0x1009db07
                    // `j = c == 18 ? 11 : 3` is compiled without a branch at 0x1009db24-0x1009db2e
                    // (`sub 0x12; neg; sbb; and 0xfffffff8; add 0xb`).
                    jj = (cc == 18) ? 11 : 3;
                    SG_NEEDBITS(t + ii)                           // 0x1009db3d, 0x1009db47
                    SG_DUMPBITS(t);                               // 0x1009dbe7-0x1009dbf2
                    jj += b & mask[ii];                           // 0x1009dbfb-0x1009dc07
                    SG_DUMPBITS(ii);                              // 0x1009dc10-0x1009dc1b
                    ii = U32R(s, B_INDEX);                        // 0x1009dc24
                    t = U32R(s, B_TABLE);                         // 0x1009dc2d
                    if (ii + jj > 258 + (t & 0x1f) + ((t >> 5) & 0x1f) ||   // 0x1009dc4e
                        (cc == 16 && ii < 1)) {                   // 0x1009dc54, 0x1009dc5a
                        U32(s, B_MODE) = BM_BAD;                  // 0x1009dc5f
                        P32(z, Z_MSG) = JgldVA(0x1012086c);       // 0x1009dc68
                        r = Z_DATA_ERROR;                         // 0x1009dc6f
                        SG_LEAVE;                                 // 0x1009dcc4
                    }
                    blens = static_cast<unsigned*>(P32R(s, B_BLENS));
                    cc = (cc == 16) ? blens[ii - 1] : 0;          // 0x1009dcd5, 0x1009dce0 / 0x1009dce9
                    do {
                        blens[ii++] = cc;                         // 0x1009dd02, 0x1009dd08
                    } while (--jj);                               // 0x1009dd1b
                    U32(s, B_INDEX) = ii;                         // 0x1009dd23
                }
            }
            freeTrees(P32R(s, B_TB), z);                          // 0x1009dd36
            P32(s, B_TB) = 0;                                     // 0x1009dd41
            {
                unsigned bl = 9;                                  // 0x1009dd48  must be <= 9
                unsigned bd = 6;                                  // 0x1009dd4f  must be <= 9
                void* tl;
                void* td;
                void* cnew;

                t = U32R(s, B_TABLE);                             // 0x1009dd59
                t = static_cast<unsigned>(
                    reinterpret_cast<InflateTreesDynamic_t>(JgldVA(VA_TREES_DYNAMIC))(
                        257 + (t & 0x1f), 1 + ((t >> 5) & 0x1f),
                        static_cast<unsigned*>(P32R(s, B_BLENS)),
                        &bl, &bd, &tl, &td, z));                  // 0x1009dd93
                if (static_cast<int>(t) != Z_OK) {                // 0x1009dda2
                    if (static_cast<int>(t) == Z_DATA_ERROR)      // 0x1009dda8
                        U32(s, B_MODE) = BM_BAD;                  // 0x1009ddad
                    r = static_cast<int>(t);                      // 0x1009ddb6
                    SG_LEAVE;                                     // 0x1009de07
                }
                cnew = reinterpret_cast<InflateCodesNew_t>(JgldVA(VA_CODES_NEW))(
                    bl, bd, tl, td, z);                           // 0x1009de28
                if (cnew == 0) {                                  // 0x1009de37
                    freeTrees(td, z);                             // 0x1009de45
                    freeTrees(tl, z);                             // 0x1009de55
                    r = Z_MEM_ERROR;                              // 0x1009de5d
                    SG_LEAVE;                                     // 0x1009deb2
                }
                ZFree(z, P32R(s, B_BLENS));                       // 0x1009ded2
                P32(s, B_CODES) = cnew;                           // 0x1009dee5
                P32(s, B_TL) = tl;                                // 0x1009deee
                P32(s, B_TD) = td;                                // 0x1009def7
            }
            U32(s, B_MODE) = BM_CODES;                            // 0x1009defd
            // falls through
        }

        case BM_CODES:
            SG_UPDATE;                                            // 0x1009df03-0x1009df42
            r = reinterpret_cast<InflateCodes_t>(JgldVA(VA_CODES))(s, z, r);   // 0x1009df51
            if (r != Z_STREAM_END)                                // 0x1009df60
                return flush(s, z, r);                            // 0x1009df6e  (no UPDATE: just done)
            r = Z_OK;                                             // 0x1009df7b
            reinterpret_cast<InflateCodesFree_t>(JgldVA(VA_CODES_FREE))(P32R(s, B_CODES), z);  // 0x1009df8d
            freeTrees(P32R(s, B_TD), z);                          // 0x1009dfa0
            freeTrees(P32R(s, B_TL), z);                          // 0x1009dfb3
            // LOAD again, 0x1009dfbb-0x1009e012
            p = static_cast<unsigned char*>(P32R(z, Z_NEXT_IN));
            n = U32R(z, Z_AVAIL_IN);
            b = U32R(s, B_BITB);
            k = U32R(s, B_BITK);
            q = static_cast<unsigned char*>(P32R(s, B_WRITE));
            m = SG_WAVAIL;                                        // 0x1009dff0
            if (!U32R(s, B_LAST)) {                               // 0x1009e01c
                U32(s, B_MODE) = BM_TYPE;                         // 0x1009e021
                break;
            }
            if (k > 7) {                                          // 0x1009e030  return the unused byte
                k -= 8;                                           // 0x1009e035
                n++;                                              // 0x1009e03e
                p--;                                              // 0x1009e047
            }
            U32(s, B_MODE) = BM_DRY;                              // 0x1009e050
            // falls through

        case BM_DRY:
            P32(s, B_WRITE) = q;                                  // 0x1009e05c  FLUSH = UPDOUT, flush,
            r = flush(s, z, r);                                   // 0x1009e06b    LOADOUT
            q = static_cast<unsigned char*>(P32R(s, B_WRITE));    // 0x1009e07c
            m = SG_WAVAIL;                                        // 0x1009e088
            if (P32R(s, B_READ) != P32R(s, B_WRITE)) SG_LEAVE;    // 0x1009e0b9, flush 0x1009e109
            U32(s, B_MODE) = BM_DONE;                             // 0x1009e119
            // falls through

        case BM_DONE:
            r = Z_STREAM_END;                                     // 0x1009e11f
            SG_LEAVE;                                             // 0x1009e174

        case BM_BAD:
            r = Z_DATA_ERROR;                                     // 0x1009e180
            SG_LEAVE;                                             // 0x1009e1d6

        default:
            r = Z_STREAM_ERROR;                                   // 0x1009e1e0
            SG_LEAVE;                                             // 0x1009e235
        }
    }

    #undef SG_UPDATE
    #undef SG_LEAVE
    #undef SG_NEEDBITS
    #undef SG_DUMPBITS
    #undef SG_WAVAIL
    #undef SG_WRAP
}

// =========================================================================== inflate 0x0009c0f0
//
// int __cdecl inflate(z_stream *z, int f): z [ebp+8], f [ebp+0xc]; `ret` with no pop at 0x1009c79c.
// Its three callers are png_decompress_chunk 0x1006c3f0, png_handle_zTXt 0x1007bc40 and
// png_read_finish_row 0x1007d280.
//
// The zlib stream-level state machine: the two-byte zlib header, an optional four-byte preset-dictionary
// id, the deflate blocks (handed to inflate_blocks 0x1009ce80) and the four-byte adler32 trailer. It reads
// one byte at a time straight out of the z_stream, so no locals survive a return: `r` at [ebp-4] and the
// flag byte at [ebp-8] are the whole frame.
//
// NEEDBYTE is `if (z->avail_in == 0) return r; r = Z_OK;` (e.g. 0x1009c169) and NEXTBYTE is
// `z->avail_in--, z->total_in++, *z->next_in++` (e.g. 0x1009c17a-0x1009c1c1) - note that these are the
// z_stream's own fields, unlike inflate_blocks' locals.
typedef int(__cdecl* Inflate_t)(void* z, int f);
Inflate_t Inflate_orig;

int __cdecl Inflate_re(void* z, int f) {
    // Stream modes, as the stores in the body: METHOD 0, FLAG 1 (0x1009c238), DICT4 2 (0x1009c2f2),
    // DICT3 3 (0x1009c356), DICT2 4 (0x1009c3c5), DICT1 5 (0x1009c434), DICT0 6 (0x1009c4af),
    // BLOCKS 7 (0x1009c2e1), CHECK4 8 (0x1009c585), CHECK3 9 (0x1009c5e9), CHECK2 10 (0x1009c658),
    // CHECK1 11 (0x1009c6c7), DONE 12 (0x1009c574), BAD 13 (0x1009c1ce).
    const unsigned SM_METHOD = 0, SM_FLAG = 1, SM_DICT4 = 2, SM_DICT3 = 3, SM_DICT2 = 4, SM_DICT1 = 5;
    const unsigned SM_DICT0 = 6, SM_BLOCKS = 7, SM_CHECK4 = 8, SM_CHECK3 = 9, SM_CHECK2 = 10;
    const unsigned SM_CHECK1 = 11, SM_DONE = 12, SM_BAD = 13;
    const int Z_NEED_DICT = 2;                                    // 0x1009c4b5

    int r;
    unsigned b;

    if (z == 0 || P32R(z, Z_STATE) == 0 || P32R(z, Z_NEXT_IN) == 0 || f < 0)   // 0x1009c10c, c115,
        return Z_STREAM_ERROR;                                                 //   c11d, c123
    void* st = P32R(z, Z_STATE);
    r = Z_BUF_ERROR;                                              // 0x1009c12f

    // NEXTBYTE, written out at each of the ten places that use it.
    #define SG_NEXTBYTE                                                                    \
        (U32(z, Z_AVAIL_IN)--, U32(z, Z_TOTAL_IN)++,                                       \
         *(*reinterpret_cast<unsigned char**>(static_cast<char*>(z) + Z_NEXT_IN))++)
    #define SG_NEEDBYTE  do { if (U32R(z, Z_AVAIL_IN) == 0) return r; r = Z_OK; } while (0)

    for (;;) {                                                    // `while (1)`, 0x1009c13d / 0x1009c787
        switch (U32R(st, S_MODE)) {                               // 0x1009c152, jump table 0x1009c79d
        case SM_METHOD:
            SG_NEEDBYTE;                                          // 0x1009c169
            U32(st, S_METHOD) = SG_NEXTBYTE;                      // 0x1009c1a7
            if ((U32R(st, S_METHOD) & 0xf) != 8) {                // 0x1009c1c6  Z_DEFLATED
                U32(st, S_MODE) = SM_BAD;                         // 0x1009c1ce
                P32(z, Z_MSG) = JgldVA(0x10120850);               // 0x1009c1d7
                U32(st, S_MARKER) = 5;                            // 0x1009c1e4  can't try inflateSync
                break;
            }
            if ((U32R(st, S_METHOD) >> 4) + 8 > U32R(st, S_WBITS)) {   // 0x1009c208
                U32(st, S_MODE) = SM_BAD;                         // 0x1009c210
                P32(z, Z_MSG) = JgldVA(0x1012083c);               // 0x1009c219
                U32(st, S_MARKER) = 5;                            // 0x1009c226
                break;
            }
            U32(st, S_MODE) = SM_FLAG;                            // 0x1009c238
            // falls through

        case SM_FLAG:
            SG_NEEDBYTE;                                          // 0x1009c245
            b = SG_NEXTBYTE;                                      // 0x1009c27d
            if (((U32R(st, S_METHOD) << 8) + b) % 31) {           // 0x1009c2a3 div by 31, 0x1009c2a7
                U32(st, S_MODE) = SM_BAD;                         // 0x1009c2af
                P32(z, Z_MSG) = JgldVA(0x10120824);               // 0x1009c2b8
                U32(st, S_MARKER) = 5;                            // 0x1009c2c5
                break;
            }
            if (!(b & 0x20)) {                                    // 0x1009c2d9  PRESET_DICT
                U32(st, S_MODE) = SM_BLOCKS;                      // 0x1009c2e1
                break;
            }
            U32(st, S_MODE) = SM_DICT4;                           // 0x1009c2f2
            // falls through

        case SM_DICT4:
            SG_NEEDBYTE;                                          // 0x1009c2ff
            U32(st, S_NEED) = static_cast<unsigned>(SG_NEXTBYTE) << 24;   // 0x1009c340
            U32(st, S_MODE) = SM_DICT3;                           // 0x1009c356
            // falls through
        case SM_DICT3:
            SG_NEEDBYTE;                                          // 0x1009c363
            U32(st, S_NEED) += static_cast<unsigned>(SG_NEXTBYTE) << 16;  // 0x1009c3af
            U32(st, S_MODE) = SM_DICT2;                           // 0x1009c3c5
            // falls through
        case SM_DICT2:
            SG_NEEDBYTE;                                          // 0x1009c3d2
            U32(st, S_NEED) += static_cast<unsigned>(SG_NEXTBYTE) << 8;   // 0x1009c41e
            U32(st, S_MODE) = SM_DICT1;                           // 0x1009c434
            // falls through
        case SM_DICT1:
            SG_NEEDBYTE;                                          // 0x1009c441
            U32(st, S_NEED) += static_cast<unsigned>(SG_NEXTBYTE);        // 0x1009c48a
            U32(z, Z_ADLER) = U32R(st, S_NEED);                   // 0x1009c4a6
            U32(st, S_MODE) = SM_DICT0;                           // 0x1009c4af
            return Z_NEED_DICT;                                   // 0x1009c4b5

        case SM_DICT0:
            U32(st, S_MODE) = SM_BAD;                             // 0x1009c4c5
            P32(z, Z_MSG) = JgldVA(0x10120814);                   // 0x1009c4ce
            U32(st, S_MARKER) = 0;                                // 0x1009c4db  inflateSync can be tried
            return Z_STREAM_ERROR;                                // 0x1009c4e2

        case SM_BLOCKS:
            r = reinterpret_cast<InflateBlocks_t>(JgldVA(VA_BLOCKS))(
                P32R(st, S_BLOCKS), z, r);                        // 0x1009c4fe
            if (r == Z_DATA_ERROR) {                              // 0x1009c50d
                U32(st, S_MODE) = SM_BAD;                         // 0x1009c515
                U32(st, S_MARKER) = 0;                            // 0x1009c521
                break;
            }
            if (r != Z_STREAM_END) return r;                      // 0x1009c531
            r = Z_OK;                                             // 0x1009c53b
            reinterpret_cast<InflateBlocksReset_t>(JgldVA(VA_BLOCKS_RESET))(
                P32R(st, S_BLOCKS), z,
                reinterpret_cast<unsigned long*>(static_cast<char*>(st) + S_WAS));   // 0x1009c55a
            if (U32R(st, S_NOWRAP)) {                             // 0x1009c56c
                U32(st, S_MODE) = SM_DONE;                        // 0x1009c574
                break;
            }
            U32(st, S_MODE) = SM_CHECK4;                          // 0x1009c585
            // falls through

        case SM_CHECK4:
            SG_NEEDBYTE;                                          // 0x1009c592
            U32(st, S_NEED) = static_cast<unsigned>(SG_NEXTBYTE) << 24;   // 0x1009c5d3
            U32(st, S_MODE) = SM_CHECK3;                          // 0x1009c5e9
            // falls through
        case SM_CHECK3:
            SG_NEEDBYTE;                                          // 0x1009c5f6
            U32(st, S_NEED) += static_cast<unsigned>(SG_NEXTBYTE) << 16;  // 0x1009c642
            U32(st, S_MODE) = SM_CHECK2;                          // 0x1009c658
            // falls through
        case SM_CHECK2:
            SG_NEEDBYTE;                                          // 0x1009c665
            U32(st, S_NEED) += static_cast<unsigned>(SG_NEXTBYTE) << 8;   // 0x1009c6b1
            U32(st, S_MODE) = SM_CHECK1;                          // 0x1009c6c7
            // falls through
        case SM_CHECK1:
            SG_NEEDBYTE;                                          // 0x1009c6d4
            U32(st, S_NEED) += static_cast<unsigned>(SG_NEXTBYTE);        // 0x1009c71d
            if (U32R(st, S_WAS) != U32R(st, S_NEED)) {            // 0x1009c73f
                U32(st, S_MODE) = SM_BAD;                         // 0x1009c747
                P32(z, Z_MSG) = JgldVA(0x101207fc);               // 0x1009c750
                U32(st, S_MARKER) = 5;                            // 0x1009c75d
                break;
            }
            U32(st, S_MODE) = SM_DONE;                            // 0x1009c76c
            // falls through

        case SM_DONE:
            return Z_STREAM_END;                                  // 0x1009c772

        case SM_BAD:
            return Z_DATA_ERROR;                                  // 0x1009c779

        default:
            return Z_STREAM_ERROR;                                // 0x1009c780
        }
    }

    #undef SG_NEXTBYTE
    #undef SG_NEEDBYTE
}

// ========================================================== libpng 1.0.5: the chunk reader ========
//
// png_struct fields these four use: error_fn +0x40, warning_fn +0x44, read_data_fn +0x50 (the slot
// png_set_read_fn 0x10078cb0 writes at 0x10078cdd), flags +0x6c, zbuf +0xac, zbuf_size +0xb0, crc +0x110,
// chunk_name[4] +0x11c.
const unsigned PNG_READ_FN = 0x50, PNG_FLAGS = 0x6c, PNG_ZBUF = 0xac, PNG_ZBUF_SIZE = 0xb0;
const unsigned PNG_CRC = 0x110, PNG_CHUNK_NAME = 0x11c;

typedef void(__cdecl* PngRwFn_t)(void* png_ptr, unsigned char* data, unsigned length);
typedef void(__cdecl* PngError_t)(void* png_ptr, const char* message);
typedef void(__cdecl* PngCalcCrc_t)(void* png_ptr, unsigned char* ptr, unsigned length);
typedef void(__cdecl* PngReadData_t)(void* png_ptr, unsigned char* data, unsigned length);
typedef void(__cdecl* PngCrcRead_t)(void* png_ptr, unsigned char* buf, unsigned length);
typedef int(__cdecl* PngCrcError_t)(void* png_ptr);
typedef unsigned(__cdecl* PngGetUint32_t)(unsigned char* buf);

const unsigned VA_PNG_READ_DATA = 0x10078c40;
const unsigned VA_PNG_CALC_CRC = 0x100787d0;
const unsigned VA_PNG_ERROR = 0x10078fe0;
const unsigned VA_PNG_CHUNK_ERROR = 0x100790a0;
const unsigned VA_PNG_CHUNK_WARNING = 0x10079250;
const unsigned VA_PNG_CRC_READ = 0x100794b0;
const unsigned VA_PNG_CRC_ERROR = 0x10079620;
const unsigned VA_PNG_GET_UINT_32 = 0x100793b0;

// ===================================================================== png_read_data 0x00078c40
//
// void __cdecl png_read_data(png_structp png_ptr, png_bytep data, png_size_t length): png_ptr [ebp+8],
// data [ebp+0xc], length [ebp+0x10]; `ret` with no pop at 0x10078ca2.
//
// Every byte libpng reads comes through here. The body is one test and two calls: if read_data_fn at
// png_ptr+0x50 is not null (0x10078c5f) it is called as read_data_fn(png_ptr, data, length), an indirect
// __cdecl call at 0x10078c72 with the three arguments pushed in reverse at 0x10078c66, 0x10078c6a and
// 0x10078c6e; otherwise png_error(png_ptr, 0x1011dfcc) at 0x10078c8a, which does not return (png_error
// 0x10078fe0 falls into png_default_error 0x100792b0 whatever the handler at png_ptr+0x40 does, and that
// longjmps through the png_struct's jmpbuf). png_ptr itself is never tested: a null one faults at
// 0x10078c5b.
PngReadData_t PngReadData_orig;

void __cdecl PngReadData_re(void* png_ptr, unsigned char* data, unsigned length) {
    if (P32R(png_ptr, PNG_READ_FN) != 0) {                        // 0x10078c5f
        reinterpret_cast<PngRwFn_t>(P32R(png_ptr, PNG_READ_FN))(png_ptr, data, length);   // 0x10078c72
    } else {
        reinterpret_cast<PngError_t>(JgldVA(VA_PNG_ERROR))(
            png_ptr, static_cast<const char*>(JgldVA(0x1011dfcc)));                       // 0x10078c8a
    }
}

// ====================================================================== png_crc_read 0x000794b0
//
// void __cdecl png_crc_read(png_structp png_ptr, png_bytep buf, png_size_t length): png_ptr [ebp+8],
// buf [ebp+0xc], length [ebp+0x10]; `ret` with no pop at 0x10079500. No conditional jump at all.
//
// The pair every chunk reader uses: png_read_data(png_ptr, buf, length) at 0x100794d4 fills the buffer,
// then png_calculate_crc(png_ptr, buf, length) at 0x100794e8 runs the same bytes through the running crc
// at png_ptr+0x110 (which it skips when the chunk's flags say so). The same three arguments are pushed
// twice, in the same order, from the same stack slots.
PngCrcRead_t PngCrcRead_orig;

void __cdecl PngCrcRead_re(void* png_ptr, unsigned char* buf, unsigned length) {
    reinterpret_cast<PngReadData_t>(JgldVA(VA_PNG_READ_DATA))(png_ptr, buf, length);      // 0x100794d4
    reinterpret_cast<PngCalcCrc_t>(JgldVA(VA_PNG_CALC_CRC))(png_ptr, buf, length);        // 0x100794e8
}

// ===================================================================== png_crc_error 0x00079620
//
// int __cdecl png_crc_error(png_structp png_ptr): png_ptr [ebp+8]; `ret` with no pop at 0x100796d1.
// Its only caller is png_crc_finish 0x10079510.
//
// It reads the four CRC bytes that close a chunk and says whether they disagree with the crc libpng
// accumulated. Whether it compares at all is decided exactly as png_calculate_crc 0x100787d0 decides
// whether to accumulate, from the same two fields:
//   * need_crc starts at 1 (0x10079638);
//   * for an ANCILLARY chunk (chunk_name[0] & 0x20 at 0x1007964f) need_crc becomes 0 when both of the
//     flag bits 0x100 and 0x200 are set (`and 0x300; cmp 0x300` at 0x10079657-0x10079661);
//   * for a CRITICAL chunk need_crc becomes 0 when flag bit 0x800 is set (0x1007967a).
// The four bytes are read into a local at [ebp-4] through png_read_data (0x1007968d) WHATEVER need_crc
// says, so the input is always consumed; only the comparison is skipped (0x10079699), and the skipping
// path returns 0 (0x100796bf). Otherwise png_get_uint_32 0x100793b0 (0x1007969f) turns the four bytes into
// a big-endian value and the result is `crc != png_ptr->crc` materialised with `setne` (0x100796b8).
PngCrcError_t PngCrcError_orig;

int __cdecl PngCrcError_re(void* png_ptr) {
    unsigned char crc_bytes[4];
    unsigned crc;
    int need_crc = 1;                                             // 0x10079638

    if (U8R(png_ptr, PNG_CHUNK_NAME) & 0x20) {                    // 0x1007964f  ancillary
        if ((U32R(png_ptr, PNG_FLAGS) & 0x300) == 0x300)          // 0x10079661
            need_crc = 0;                                         // 0x10079663
    } else {                                                      // critical
        if (U32R(png_ptr, PNG_FLAGS) & 0x800)                     // 0x1007967a
            need_crc = 0;                                         // 0x1007967c
    }

    reinterpret_cast<PngReadData_t>(JgldVA(VA_PNG_READ_DATA))(png_ptr, crc_bytes, 4);     // 0x1007968d
    if (need_crc) {                                               // 0x10079699
        crc = reinterpret_cast<PngGetUint32_t>(JgldVA(VA_PNG_GET_UINT_32))(crc_bytes);    // 0x1007969f
        return (crc != U32R(png_ptr, PNG_CRC)) ? 1 : 0;           // 0x100796b2, 0x100796b8
    }
    return 0;                                                     // 0x100796bf
}

// ==================================================================== png_crc_finish 0x00079510
//
// int __cdecl png_crc_finish(png_structp png_ptr, png_uint_32 skip): png_ptr [ebp+8], skip [ebp+0xc];
// `ret` with no pop at 0x1007961f. Twenty callers, one per chunk handler.
//
// It swallows the `skip` bytes a handler did not want and then checks the chunk's CRC. The skipping is
// done in zbuf-sized pieces: istop is zbuf_size from png_ptr+0xb0 (0x1007952b) and the loop
// `for (i = skip; i > istop; i -= istop)` (0x1007954b) reads a whole zbuf each time (0x10079565), then the
// remainder, if any, in one more read (0x10079573, 0x10079587) - so a skip equal to istop is read once by
// the remainder read and not by the loop.
//
// png_crc_error 0x10079620 (0x10079593) then says whether the four CRC bytes disagree. On disagreement the
// function picks between a WARNING and a fatal error from the same two fields png_crc_error used, but with
// DIFFERENT bits: an ancillary chunk warns unless flag bit 0x200 is set (0x100795af, 0x100795be) and a
// critical chunk warns only when flag bit 0x400 is set (0x100795d0, 0x100795e0). The warning goes to
// png_chunk_warning 0x10079250 (0x100795eb) and the fatal one to png_chunk_error 0x100790a0 (0x100795fe),
// both with the same message 0x1011e0f4; either way it returns 1 (0x10079606), and 0 when the CRC matched
// (0x1007960d). png_chunk_error reaches png_error, which never returns.
typedef int(__cdecl* PngCrcFinish_t)(void* png_ptr, unsigned skip);
PngCrcFinish_t PngCrcFinish_orig;

int __cdecl PngCrcFinish_re(void* png_ptr, unsigned skip) {
    unsigned i;
    unsigned istop = U32R(png_ptr, PNG_ZBUF_SIZE);                // 0x1007952b
    PngCrcRead_t crcRead = reinterpret_cast<PngCrcRead_t>(JgldVA(VA_PNG_CRC_READ));

    for (i = skip; i > istop; i -= istop) {                       // 0x10079537, 0x1007954b
        crcRead(png_ptr, static_cast<unsigned char*>(P32R(png_ptr, PNG_ZBUF)),
                U32R(png_ptr, PNG_ZBUF_SIZE));                    // 0x10079565
    }
    if (i) {                                                      // 0x10079573
        crcRead(png_ptr, static_cast<unsigned char*>(P32R(png_ptr, PNG_ZBUF)), i);        // 0x10079587
    }

    if (reinterpret_cast<PngCrcError_t>(JgldVA(VA_PNG_CRC_ERROR))(png_ptr)) {             // 0x10079593,
        if (((U8R(png_ptr, PNG_CHUNK_NAME) & 0x20) &&                                     //   0x1007959d
             !(U32R(png_ptr, PNG_FLAGS) & 0x200)) ||              // 0x100795af, 0x100795be
            (!(U8R(png_ptr, PNG_CHUNK_NAME) & 0x20) &&            // 0x100795d0
             (U32R(png_ptr, PNG_FLAGS) & 0x400))) {               // 0x100795e0
            reinterpret_cast<PngError_t>(JgldVA(VA_PNG_CHUNK_WARNING))(
                png_ptr, static_cast<const char*>(JgldVA(0x1011e0f4)));                   // 0x100795eb
        } else {
            reinterpret_cast<PngError_t>(JgldVA(VA_PNG_CHUNK_ERROR))(
                png_ptr, static_cast<const char*>(JgldVA(0x1011e0f4)));                   // 0x100795fe
        }
        return 1;                                                 // 0x10079606
    }
    return 0;                                                     // 0x1007960d
}

}  // namespace

// 0x0009e7a0  huft_build
SG_HOOK("jgld.dll", 0x0009e7a0, huft_build, HuftBuild_re, HuftBuild_orig);
// 0x0009ce80  inflate_blocks
SG_HOOK("jgld.dll", 0x0009ce80, inflate_blocks, InflateBlocks_re, InflateBlocks_orig);
// 0x0009c0f0  inflate
SG_HOOK("jgld.dll", 0x0009c0f0, inflate, Inflate_re, Inflate_orig);
// 0x00078c40  png_read_data
SG_HOOK("jgld.dll", 0x00078c40, png_read_data, PngReadData_re, PngReadData_orig);
// 0x000794b0  png_crc_read
SG_HOOK("jgld.dll", 0x000794b0, png_crc_read, PngCrcRead_re, PngCrcRead_orig);
// 0x00079620  png_crc_error
SG_HOOK("jgld.dll", 0x00079620, png_crc_error, PngCrcError_re, PngCrcError_orig);
// 0x00079510  png_crc_finish
SG_HOOK("jgld.dll", 0x00079510, png_crc_finish, PngCrcFinish_re, PngCrcFinish_orig);

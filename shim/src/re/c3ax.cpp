// Batch c3ax: jgld.dll non-render leaves - one x87 vector helper, the two RECT wrappers, ListNode's destructor,
// five libpng 1.0.5 row transformations, libpng's four self-contained helpers (png_reset_crc, png_info_init,
// png_memcpy_check, png_memset_check) and three zlib 1.0.2 routines (crc32, adler32, inflate_set_dictionary).
// SG_HOOK addresses are RVAs; every instruction cited in a comment is a VA (0x10000000 + RVA), the form the
// debug build's disassembly prints. jgld.dll is LoadLibrary'd after the shim starts, so these hooks install from
// the LoadLibraryA hook (SgHooksInstallPending, re/hooks.cpp) and diff_hook finds them by (module, rva) through
// SimGolfShim_FindHookM. __thiscall is emulated with __fastcall: ecx = this, edx unused.
//
// FLOATING POINT. One body here contains x87 instructions, Vector3::scale (0x10003f10): one `fld dword` /
// `fmul dword` / `fstp dword` per component. The method is c3ap's, verified there: the A/B thread's x87 control
// word is 0x027f (PC = 53-bit, RC = nearest), so every x87 operation rounds like an IEEE double operation; the
// single `fstp dword` is written as a cast to `float` and both operands are widened with static_cast<double>.
// Every other body in this file is integer or byte work.
//
// Nothing live in the running jgld.dll is touched: no Display, Surface, Palette, Font or the game's png_struct.
// The read-only tables (zlib's CRC table) and the callees that stay original (the CRT memcpy/memset, png_error)
// are reached at their own module addresses, so each comparison is of this code and not of a CRT.
#include "hooks.h"

namespace {

// jgld.dll is relocatable, so every module address below is resolved against the running base.
char* JgldBase() {
    static char* base = 0;
    if (!base) base = reinterpret_cast<char*>(GetModuleHandleA("jgld.dll"));
    return base;
}

// libpng 1.0.5's png_row_info, read off the bodies: width at +0 (0x1006e03e), rowbytes at +4 (0x1007253e),
// color_type at +8 (0x1006e02d), bit_depth at +9 (0x1006e048), channels at +0xa (0x100720a5), pixel_depth at
// +0xb (written at 0x1007209d). The four tail fields are always read with a zero-extending byte load.
struct PngRowInfo {
    unsigned width;
    unsigned rowbytes;
    unsigned char color_type;
    unsigned char bit_depth;
    unsigned char channels;
    unsigned char pixel_depth;
};

// The routines that stay original.
typedef void*(__cdecl* Memcpy_t)(void*, const void*, unsigned);                // 0x1007f3a0
typedef void*(__cdecl* Memset_t)(void*, int, unsigned);                        // 0x1007e7c0
typedef unsigned(__cdecl* Crc32_t)(unsigned, const unsigned char*, unsigned);  // 0x1009ca70
typedef void(__cdecl* PngError_t)(void*, const char*);                         // 0x10078fe0

Memcpy_t JgldMemcpy() { return reinterpret_cast<Memcpy_t>(JgldBase() + 0x7f3a0); }
Memset_t JgldMemset() { return reinterpret_cast<Memset_t>(JgldBase() + 0x7e7c0); }
PngError_t JgldPngError() { return reinterpret_cast<PngError_t>(JgldBase() + 0x78fe0); }

// ------------------------------------------------------------------------------- jgld: vector, rects, list node

// 0x00003f10  Vector3::scale
// void __thiscall Vector3::scale(float s): the counter at [ebp-8] runs 0,1,2 (`cmp dword ptr [ebp-8], 3` /
// `jge 0x10003f5c` at 0x10003f3f-0x10003f43, a signed compare), and each iteration is `fld dword ptr [ebp+8]`
// (the scalar, 0x10003f4b), `fmul dword ptr [edx+ecx*4]` (the component, 0x10003f4e) and `fstp dword ptr
// [ecx+eax*4]` (0x10003f57): the scalar is the left operand, the product is rounded to float and written back
// into the component it was read from, so iteration i reads and writes only v[i]. One stack argument (`ret 4`
// at 0x10003f62), no return value. The body is the same instruction sequence as Vector3::operator*=(float)
// (0x10003a40, at C3 in batch c3ap) without that function's `mov eax, [ebp-4]` return of `this`.
typedef void(__fastcall* Vec3Scale_t)(float*, void*, float);
Vec3Scale_t Vec3Scale_orig;
void __fastcall Vec3Scale_re(float* self, void*, float s) {
    for (int i = 0; i < 3; i++)
        self[i] = static_cast<float>(static_cast<double>(s) * self[i]);
}

// 0x00008590  intersect
// void __cdecl intersect(RECT* dst, const RECT* a, const RECT* b): the whole body is one call of the imported
// IntersectRect through the IAT slot at 0x1012c5e4 (`call dword ptr [0x1012c5e4]` at 0x100085b6) with the three
// arguments pushed in reverse order (dst last: [ebp+0x10] at 0x100085aa, [ebp+0xc] at 0x100085ae, [ebp+8] at
// 0x100085b2). Nothing else is written. The BOOL the import leaves in eax is still there at `ret` (0x100085d3):
// the only instructions in between are the two debug stack checks (`call 0x1007e780` at 0x100085be and
// 0x100085cb), and __chkesp returns on its first instruction (`jne` at 0x1007e780 not taken) when esp matches,
// without touching eax. __cdecl, no pop.
typedef int(__cdecl* Intersect_t)(void*, const void*, const void*);
Intersect_t Intersect_orig;
int __cdecl Intersect_re(void* dst, const void* a, const void* b) {
    typedef int(__stdcall * IntersectRect_t)(void*, const void*, const void*);
    IntersectRect_t imported = *reinterpret_cast<IntersectRect_t*>(JgldBase() + 0x12c5e4);
    return imported(dst, a, b);
}

// 0x000085f0  equal
// int __cdecl equal(const RECT* a, const RECT* b): one call of the imported EqualRect through the IAT slot at
// 0x1012c5e8 (`call dword ptr [0x1012c5e8]` at 0x10008612) with [ebp+0xc] pushed first (0x1000860a) and
// [ebp+8] second (0x1000860e). Nothing is written; the import's BOOL survives in eax to the `ret` at
// 0x1000862f for the same reason as in intersect above. __cdecl, no pop.
typedef int(__cdecl* Equal_t)(const void*, const void*);
Equal_t Equal_orig;
int __cdecl Equal_re(const void* a, const void* b) {
    typedef int(__stdcall * EqualRect_t)(const void*, const void*);
    EqualRect_t imported = *reinterpret_cast<EqualRect_t*>(JgldBase() + 0x12c5e8);
    return imported(a, b);
}

// 0x00007450  ListNode::dtor
// void __thiscall ListNode::~ListNode(): three stores and nothing else - the vtable pointer at +0 (VA
// 0x1011d098, `mov dword ptr [eax], 0x1011d098` at 0x10007470), then 0 at +8 (0x10007479) and 0 at +4
// (0x10007483), in that order. The payload pointer the node carries beyond +8 is not touched. No argument
// (`ret` at 0x10007490), no branch. eax holds `this` at the `ret` because of the reload at 0x1000746d, but no
// instruction sets a return value, so the comparison is on memory.
typedef void(__fastcall* ListNodeDtor_t)(void*, void*);
ListNodeDtor_t ListNodeDtor_orig;
void __fastcall ListNodeDtor_re(void* self, void*) {
    unsigned* n = static_cast<unsigned*>(self);
    n[0] = reinterpret_cast<unsigned>(JgldBase() + 0x11d098);
    n[2] = 0;
    n[1] = 0;
}

// ---------------------------------------------------------------------- libpng 1.0.5 helpers (__cdecl, leaves)

// 0x00078790  png_reset_crc
// void __cdecl png_reset_crc(png_struct* png_ptr): calls zlib's crc32 with three zeros pushed (0x100787a8,
// 0x100787aa, 0x100787ac; `call 0x1009ca70` at 0x100787ae, arguments popped by the caller at 0x100787b3) and
// stores the result in the running CRC at png_ptr+0x110 (`mov dword ptr [ecx+0x110], eax` at 0x100787b9).
// crc32 with a null buffer returns 0 (`xor eax, eax` at 0x1009ca8e), so the stored word is always 0; the
// png_struct is otherwise untouched and no branch exists in the body. No return value, `ret` at 0x100787cf.
typedef void(__cdecl* PngResetCrc_t)(void*);
PngResetCrc_t PngResetCrc_orig;
void __cdecl PngResetCrc_re(void* png_ptr) {
    Crc32_t jgld_crc32 = reinterpret_cast<Crc32_t>(JgldBase() + 0x9ca70);
    *reinterpret_cast<unsigned*>(static_cast<char*>(png_ptr) + 0x110) = jgld_crc32(0, 0, 0);
}

// 0x00078940  png_info_init
// void __cdecl png_info_init(png_info* info_ptr): one call of the CRT memset at 0x1007e7c0 (`push 0xb8` at
// 0x10078958, `push 0` at 0x1007895d, the pointer at 0x1007895f, `call` at 0x10078963), i.e. the first 0xb8
// bytes of the record are zeroed and nothing beyond them. No branch, no return value, `ret` at 0x1007897b.
typedef void(__cdecl* PngInfoInit_t)(void*);
PngInfoInit_t PngInfoInit_orig;
void __cdecl PngInfoInit_re(void* info_ptr) {
    JgldMemset()(info_ptr, 0, 0xb8);
}

// 0x00078f20  png_memcpy_check
// void* __cdecl png_memcpy_check(png_struct* png_ptr, void* s1, const void* s2, unsigned length): the length
// is copied into the local at [ebp-4] (0x10078f38-0x10078f3b) and compared with the argument it was copied
// from (`cmp ecx, dword ptr [ebp+0x14]` at 0x10078f41). The two operands are the same dword, so the `je
// 0x10078f57` at 0x10078f44 is taken for every possible argument and the png_error call at 0x10078f4f (with
// the string at 0x1011e07c) cannot run in this build: the C source narrows png_uint_32 to png_size_t, which
// are both 32 bits here. The copy is the CRT memcpy at 0x1007f3a0 (`call` at 0x10078f63) over (s1, s2, size)
// and its return value, s1, is the function's own (nothing writes eax after it). __cdecl, `ret` at 0x10078f7b.
typedef void*(__cdecl* PngMemcpyCheck_t)(void*, void*, const void*, unsigned);
PngMemcpyCheck_t PngMemcpyCheck_orig;
void* __cdecl PngMemcpyCheck_re(void* png_ptr, void* s1, const void* s2, unsigned length) {
    unsigned size = length;
    if (size != length) JgldPngError()(png_ptr, JgldBase() + 0x11e07c);
    return JgldMemcpy()(s1, s2, size);
}

// 0x00078f80  png_memset_check
// void* __cdecl png_memset_check(png_struct* png_ptr, void* s1, int value, unsigned length): the same
// unreachable overflow guard (copy at 0x10078f98-0x10078f9b, `cmp` at 0x10078fa1, `je 0x10078fb7` at
// 0x10078fa4, png_error with the string at 0x1011e09c called at 0x10078faf), then the CRT memset at
// 0x1007e7c0 (`call` at 0x10078fc3) over (s1, value, size) - the pushes at 0x10078fb7, 0x10078fbb and
// 0x10078fbf put size first, then value ([ebp+0x10]), then s1 ([ebp+0xc]). memset's return value, s1, is the
// function's own. __cdecl, `ret` at 0x10078fdb.
typedef void*(__cdecl* PngMemsetCheck_t)(void*, void*, int, unsigned);
PngMemsetCheck_t PngMemsetCheck_orig;
void* __cdecl PngMemsetCheck_re(void* png_ptr, void* s1, int value, unsigned length) {
    unsigned size = length;
    if (size != length) JgldPngError()(png_ptr, JgldBase() + 0x11e09c);
    return JgldMemset()(s1, value, size);
}

// ------------------------------------------------------- libpng 1.0.5 row transformations (__cdecl, no callee)

// 0x0006e010  png_do_bgr
// void __cdecl png_do_bgr(png_row_info*, unsigned char* row): swaps the first and third sample of every pixel.
// Guard: bit 1 of color_type must be set (`and ecx, 2` / `test` / `je 0x1006e1fa` at 0x1006e030-0x1006e035),
// i.e. the colour bit; the two pointers are not tested for null. The width is read once into [ebp-4] before
// the branches (0x1006e03e) and is the unsigned loop bound of all four loops (`jae` at 0x1006e088, 0x1006e0de,
// 0x1006e148, 0x1006e1bc), so width 0 leaves the row untouched. bit_depth 8 (`jne 0x1006e103` at 0x1006e04e)
// selects the byte loops, bit_depth 16 (`jne 0x1006e1fa` at 0x1006e10e) the 16-bit ones, and any other depth
// returns. Inside each, color_type 2 (`jne` at 0x1006e05f / 0x1006e11f) is the 3-sample pixel and color_type 6
// (`jne` at 0x1006e0b5 / 0x1006e193) the 4-sample one; any other value with bit 1 set writes nothing. The
// 8-bit loops exchange rp[0] with rp[2] (0x1006e08d-0x1006e0a3) and step 3 or 4 bytes; the 16-bit loops
// exchange rp[0] with rp[4] and rp[1] with rp[5] and step 6 or 8 bytes. No field of the header is written.
typedef void(__cdecl* PngDoBgr_t)(PngRowInfo*, unsigned char*);
PngDoBgr_t PngDoBgr_orig;
void __cdecl PngDoBgr_re(PngRowInfo* row_info, unsigned char* row) {
    if ((row_info->color_type & 2) == 0) return;
    unsigned row_width = row_info->width;
    if (row_info->bit_depth == 8) {
        if (row_info->color_type == 2) {
            unsigned char* rp = row;
            for (unsigned i = 0; i < row_width; i++, rp += 3) {
                unsigned char save = rp[0];
                rp[0] = rp[2];
                rp[2] = save;
            }
        } else if (row_info->color_type == 6) {
            unsigned char* rp = row;
            for (unsigned i = 0; i < row_width; i++, rp += 4) {
                unsigned char save = rp[0];
                rp[0] = rp[2];
                rp[2] = save;
            }
        }
    } else if (row_info->bit_depth == 16) {
        if (row_info->color_type == 2) {
            unsigned char* rp = row;
            for (unsigned i = 0; i < row_width; i++, rp += 6) {
                unsigned char save = rp[0];
                rp[0] = rp[4];
                rp[4] = save;
                save = rp[1];
                rp[1] = rp[5];
                rp[5] = save;
            }
        } else if (row_info->color_type == 6) {
            unsigned char* rp = row;
            for (unsigned i = 0; i < row_width; i++, rp += 8) {
                unsigned char save = rp[0];
                rp[0] = rp[4];
                rp[4] = save;
                save = rp[1];
                rp[1] = rp[5];
                rp[5] = save;
            }
        }
    }
}

// 0x00071e80  png_do_unpack
// void __cdecl png_do_unpack(png_row_info*, unsigned char* row): expands a sub-byte row into one byte per
// pixel, in place and backwards. Guard: bit_depth < 8 (`cmp eax, 8` / `jge 0x100720b4` at 0x10071ea0-0x10071ea3,
// a signed compare of the zero-extended byte), otherwise nothing at all happens. The width is read into
// [ebp-8] (0x10071eac) before the three-way test on bit_depth: 1 (`je 0x10071ed9` at 0x10071ebe), 2 (`je
// 0x10071f6b` at 0x10071ec4), 4 (`je 0x10071ffd` at 0x10071ece); any other value below 8 jumps straight to the
// tail (`jmp 0x10072088` at 0x10071ed4), i.e. the header is still rewritten although no pixel moves.
// Each loop starts at sp = row + ((width - 1) >> shiftbits) (0x10071edf for depth 1) and dp = row + width - 1,
// with the first shift 7 - ((width + 7) & 7) for depth 1 (0x10071efd), (3 - ((width + 3) & 3)) * 2 for depth 2
// (`shl ecx, 1` at 0x10071f99) and (1 - ((width + 1) & 1)) * 4 for depth 4. Each iteration stores
// (*sp >> shift) & mask at *dp, then advances the shift by one unit, except that the top shift value wraps to
// 0 and steps sp back one byte (`cmp dword ptr [ebp-0x14], 7` / `jne` at 0x10071f3a-0x10071f3e for depth 1);
// in the depth-4 case the else branch stores the constant 4 rather than adding. The counters are compared with
// the width unsigned (`jae` at 0x10071f22 and the twins in the other two loops), so width 0 writes nothing.
// Tail, run by every path that passes the guard: bit_depth = 8 (0x1007208b), pixel_depth = channels * 8 stored
// as a byte (`shl edx, 3` at 0x10072097, `mov byte ptr [eax+0xb], dl` at 0x1007209d) and rowbytes = width *
// channels (`imul` at 0x100720ab, stored at 0x100720b1), using the width read at the start.
typedef void(__cdecl* PngDoUnpack_t)(PngRowInfo*, unsigned char*);
PngDoUnpack_t PngDoUnpack_orig;
void __cdecl PngDoUnpack_re(PngRowInfo* row_info, unsigned char* row) {
    if (static_cast<int>(row_info->bit_depth) >= 8) return;
    unsigned row_width = row_info->width;
    unsigned char depth = row_info->bit_depth;
    if (depth == 1) {
        unsigned char* sp = row + ((row_width - 1) >> 3);
        unsigned char* dp = row + row_width - 1;
        int shift = 7 - static_cast<int>((row_width + 7) & 7);
        for (unsigned i = 0; i < row_width; i++) {
            *dp = static_cast<unsigned char>((*sp >> shift) & 1);
            if (shift == 7) {
                shift = 0;
                sp--;
            } else {
                shift++;
            }
            dp--;
        }
    } else if (depth == 2) {
        unsigned char* sp = row + ((row_width - 1) >> 2);
        unsigned char* dp = row + row_width - 1;
        int shift = static_cast<int>(3 - ((row_width + 3) & 3)) * 2;
        for (unsigned i = 0; i < row_width; i++) {
            *dp = static_cast<unsigned char>((*sp >> shift) & 3);
            if (shift == 6) {
                shift = 0;
                sp--;
            } else {
                shift += 2;
            }
            dp--;
        }
    } else if (depth == 4) {
        unsigned char* sp = row + ((row_width - 1) >> 1);
        unsigned char* dp = row + row_width - 1;
        int shift = static_cast<int>(1 - ((row_width + 1) & 1)) * 4;
        for (unsigned i = 0; i < row_width; i++) {
            *dp = static_cast<unsigned char>((*sp >> shift) & 0xf);
            if (shift == 4) {
                shift = 0;
                sp--;
            } else {
                shift = 4;
            }
            dp--;
        }
    }
    row_info->bit_depth = 8;
    row_info->pixel_depth = static_cast<unsigned char>(row_info->channels << 3);
    row_info->rowbytes = row_width * row_info->channels;
}

// 0x00073310  png_do_gray_to_rgb
// void __cdecl png_do_gray_to_rgb(png_row_info*, unsigned char* row): turns a grey row into an RGB one in
// place, writing backwards. The width is read first (0x1007332b). Two guards return without any change:
// bit_depth < 8 (`cmp eax, 8` / `jl 0x100736b9` at 0x10073338-0x1007333b, signed on the zero-extended byte)
// and bit 1 of color_type already set (`and edx, 2` / `jne 0x100736b9` at 0x10073349-0x1007334e). color_type 0
// (`test ecx, ecx` / `jne 0x100734b4` at 0x1007335c-0x1007335e) is grey, color_type 4 (`cmp eax, 4` / `jne
// 0x10073668` at 0x100734bc-0x100734bf) is grey+alpha, and any other value that passed the guards reaches the
// tail with no pixel moved. Inside each, bit_depth 8 (`jne` at 0x1007336f / 0x100734d0) selects the 1-byte
// sample loop and any other depth the 2-byte one. The four loops start at
//   grey/8:   sp = row + width - 1,     dp = sp + width * 2, and write dp[0..-2] = *sp, stepping -3 and -1;
//   grey/16:  sp = row + width * 2 - 1, dp = sp + width * 4, writing the sample pair three times (-6, -2);
//   grey+a/8: sp = row + width * 2 - 1, dp = sp + width * 2, alpha first then the grey byte three times
//             (-4, -2);
//   grey+a/16:sp = row + width * 4 - 1, dp = sp + width * 4, the alpha pair then the grey pair three times
//             (-8, -4).
// All four counters are compared with the width unsigned (`jae` at 0x100733a2, 0x1007341e, 0x10073507,
// 0x1007359f), so width 0 moves nothing.
// Tail at 0x10073668, reached by every path that passes the two guards: channels += 2 as a byte (`add dl, 2`
// at 0x1007366e), color_type |= 2 (`or dl, 2` at 0x1007367d), pixel_depth = channels * bit_depth stored as a
// byte (`imul edx, ecx` at 0x10073696 over the UPDATED channels read at 0x1007368b, stored at 0x1007369c) and
// rowbytes = (width * pixel_depth + 7) >> 3 with the pixel_depth just written (read back at 0x100736a4, `imul`
// at 0x100736aa, `add eax, 7` / `shr eax, 3` at 0x100736ad-0x100736b0, stored at 0x100736b6).
typedef void(__cdecl* PngDoGrayToRgb_t)(PngRowInfo*, unsigned char*);
PngDoGrayToRgb_t PngDoGrayToRgb_orig;
void __cdecl PngDoGrayToRgb_re(PngRowInfo* row_info, unsigned char* row) {
    unsigned row_width = row_info->width;
    if (static_cast<int>(row_info->bit_depth) < 8) return;
    if ((row_info->color_type & 2) != 0) return;
    if (row_info->color_type == 0) {
        if (row_info->bit_depth == 8) {
            unsigned char* sp = row + row_width - 1;
            unsigned char* dp = sp + row_width * 2;
            for (unsigned i = 0; i < row_width; i++) {
                dp[0] = *sp;
                dp[-1] = *sp;
                dp[-2] = *sp;
                dp -= 3;
                sp -= 1;
            }
        } else {
            unsigned char* sp = row + row_width * 2 - 1;
            unsigned char* dp = sp + row_width * 4;
            for (unsigned i = 0; i < row_width; i++) {
                dp[0] = sp[0];
                dp[-1] = sp[-1];
                dp[-2] = sp[0];
                dp[-3] = sp[-1];
                dp[-4] = sp[0];
                dp[-5] = sp[-1];
                dp -= 6;
                sp -= 2;
            }
        }
    } else if (row_info->color_type == 4) {
        if (row_info->bit_depth == 8) {
            unsigned char* sp = row + row_width * 2 - 1;
            unsigned char* dp = sp + row_width * 2;
            for (unsigned i = 0; i < row_width; i++) {
                dp[0] = sp[0];
                dp[-1] = sp[-1];
                dp[-2] = sp[-1];
                dp[-3] = sp[-1];
                dp -= 4;
                sp -= 2;
            }
        } else {
            unsigned char* sp = row + row_width * 4 - 1;
            unsigned char* dp = sp + row_width * 4;
            for (unsigned i = 0; i < row_width; i++) {
                dp[0] = sp[0];
                dp[-1] = sp[-1];
                dp[-2] = sp[-2];
                dp[-3] = sp[-3];
                dp[-4] = sp[-2];
                dp[-5] = sp[-3];
                dp[-6] = sp[-2];
                dp[-7] = sp[-3];
                dp -= 8;
                sp -= 4;
            }
        }
    }
    row_info->channels = static_cast<unsigned char>(row_info->channels + 2);
    row_info->color_type = static_cast<unsigned char>(row_info->color_type | 2);
    row_info->pixel_depth = static_cast<unsigned char>(row_info->channels * row_info->bit_depth);
    row_info->rowbytes = (row_width * row_info->pixel_depth + 7) >> 3;
}

// 0x000724f0  png_do_read_swap_alpha
// void __cdecl png_do_read_swap_alpha(png_row_info*, unsigned char* row): moves the alpha sample of every
// pixel from last to first, in place and backwards. The width is read into [ebp-4] first (0x10072511). Only
// color_type 6 (`cmp eax, 6` / `jne 0x1007270a` at 0x1007251e-0x10072521) and color_type 4 (`cmp edx, 4` /
// `jne 0x1007284a` at 0x10072712-0x10072715) do anything; inside each, bit_depth 8 (`jne` at 0x10072532 /
// 0x10072726) selects the 1-byte sample loop and any other depth the 2-byte one. All four loops start both
// pointers at row + rowbytes (`add ecx, dword ptr [eax+4]` at 0x1007253e and its twins), i.e. the end pointer
// comes from the header's rowbytes field and not from the width, and walk back 4, 8, 2 or 4 bytes per pixel
// for as many pixels as width says (unsigned compares, `jae` at 0x10072562, 0x1007260e, 0x10072752 and the
// fourth loop's). No header field is written.
typedef void(__cdecl* PngDoReadSwapAlpha_t)(PngRowInfo*, unsigned char*);
PngDoReadSwapAlpha_t PngDoReadSwapAlpha_orig;
void __cdecl PngDoReadSwapAlpha_re(PngRowInfo* row_info, unsigned char* row) {
    unsigned row_width = row_info->width;
    if (row_info->color_type == 6) {
        if (row_info->bit_depth == 8) {
            unsigned char* sp = row + row_info->rowbytes;
            unsigned char* dp = sp;
            for (unsigned i = 0; i < row_width; i++) {
                unsigned char save = *(--sp);
                *(--dp) = *(--sp);
                *(--dp) = *(--sp);
                *(--dp) = *(--sp);
                *(--dp) = save;
            }
        } else {
            unsigned char* sp = row + row_info->rowbytes;
            unsigned char* dp = sp;
            for (unsigned i = 0; i < row_width; i++) {
                unsigned char save0 = *(--sp);
                unsigned char save1 = *(--sp);
                *(--dp) = *(--sp);
                *(--dp) = *(--sp);
                *(--dp) = *(--sp);
                *(--dp) = *(--sp);
                *(--dp) = *(--sp);
                *(--dp) = *(--sp);
                *(--dp) = save0;
                *(--dp) = save1;
            }
        }
    } else if (row_info->color_type == 4) {
        if (row_info->bit_depth == 8) {
            unsigned char* sp = row + row_info->rowbytes;
            unsigned char* dp = sp;
            for (unsigned i = 0; i < row_width; i++) {
                unsigned char save = *(--sp);
                *(--dp) = *(--sp);
                *(--dp) = save;
            }
        } else {
            unsigned char* sp = row + row_info->rowbytes;
            unsigned char* dp = sp;
            for (unsigned i = 0; i < row_width; i++) {
                unsigned char save0 = *(--sp);
                unsigned char save1 = *(--sp);
                *(--dp) = *(--sp);
                *(--dp) = *(--sp);
                *(--dp) = save0;
                *(--dp) = save1;
            }
        }
    }
}

// 0x00072860  png_do_read_invert_alpha
// void __cdecl png_do_read_invert_alpha(png_row_info*, unsigned char* row): replaces the alpha sample of
// every pixel with its complement and leaves the colour samples where they are. Same shape as the swap above:
// width read first (0x10072881 region, [ebp-4]), color_type 6 (`cmp eax, 6` / `jne 0x10072a7d` at
// 0x10072888-0x1007288b) or 4 (`cmp edx, 4` / `jne 0x10072bc6` at 0x10072a85-0x10072a88), and inside each
// bit_depth 8 (`jne` at 0x1007289c / 0x10072a99) or any other depth. Both pointers start at row + rowbytes
// and the loops step back 4, 8, 2 or 4 bytes per pixel (unsigned compares, `jae` at 0x100728cc, 0x1007297b,
// 0x10072ac5 and the fourth loop's). The complement is `255 - alpha` on one byte at depth 8 and on each of
// the two alpha bytes at any other depth. No header field is written.
typedef void(__cdecl* PngDoReadInvertAlpha_t)(PngRowInfo*, unsigned char*);
PngDoReadInvertAlpha_t PngDoReadInvertAlpha_orig;
void __cdecl PngDoReadInvertAlpha_re(PngRowInfo* row_info, unsigned char* row) {
    unsigned row_width = row_info->width;
    if (row_info->color_type == 6) {
        if (row_info->bit_depth == 8) {
            unsigned char* sp = row + row_info->rowbytes;
            unsigned char* dp = sp;
            for (unsigned i = 0; i < row_width; i++) {
                *(--dp) = static_cast<unsigned char>(255 - *(--sp));
                *(--dp) = *(--sp);
                *(--dp) = *(--sp);
                *(--dp) = *(--sp);
            }
        } else {
            unsigned char* sp = row + row_info->rowbytes;
            unsigned char* dp = sp;
            for (unsigned i = 0; i < row_width; i++) {
                *(--dp) = static_cast<unsigned char>(255 - *(--sp));
                *(--dp) = static_cast<unsigned char>(255 - *(--sp));
                *(--dp) = *(--sp);
                *(--dp) = *(--sp);
                *(--dp) = *(--sp);
                *(--dp) = *(--sp);
                *(--dp) = *(--sp);
                *(--dp) = *(--sp);
            }
        }
    } else if (row_info->color_type == 4) {
        if (row_info->bit_depth == 8) {
            unsigned char* sp = row + row_info->rowbytes;
            unsigned char* dp = sp;
            for (unsigned i = 0; i < row_width; i++) {
                *(--dp) = static_cast<unsigned char>(255 - *(--sp));
                *(--dp) = *(--sp);
            }
        } else {
            unsigned char* sp = row + row_info->rowbytes;
            unsigned char* dp = sp;
            for (unsigned i = 0; i < row_width; i++) {
                *(--dp) = static_cast<unsigned char>(255 - *(--sp));
                *(--dp) = static_cast<unsigned char>(255 - *(--sp));
                *(--dp) = *(--sp);
                *(--dp) = *(--sp);
            }
        }
    }
}

// --------------------------------------------------------------------------------- zlib 1.0.2 (__cdecl, leaves)

// 0x0009ca70  crc32
// unsigned __cdecl crc32(unsigned crc, const unsigned char* buf, unsigned len): a null buffer returns 0
// (`cmp dword ptr [ebp+0xc], 0` / `jne 0x1009ca95` at 0x1009ca88-0x1009ca8c, `xor eax, eax` at 0x1009ca8e).
// Otherwise the running value is complemented (`xor eax, 0xffffffff` at 0x1009ca98), folded eight bytes at a
// time while len >= 8 (`cmp dword ptr [ebp+0x10], 8` / `jb 0x1009cc16` at 0x1009ca9e-0x1009caa2, the eight
// copies ending with `sub ecx, 8` at 0x1009cc0b and the back jump at 0x1009cc11), then byte by byte for the
// remainder (`cmp dword ptr [ebp+0x10], 0` / `je 0x1009cc58` at 0x1009cc16-0x1009cc1a, the do-while's `jne`
// at 0x1009cc56), and complemented again (0x1009cc5b). One fold is
// crc = table[(crc ^ *buf++) & 0xff] ^ (crc >> 8): the byte is zero-extended (`mov dl, byte ptr [ecx]` at
// 0x1009caad), the index masked to 8 bits (`and eax, 0xff` at 0x1009cab4), the running value shifted right
// unsigned (`shr ecx, 8` at 0x1009cabc) and the table read at VA 0x10126e18 (0x1009cabf). The table is read
// here at that same address: it is module data, not a copy of ours, and it is never written.
typedef unsigned(__cdecl* Crc32Fn_t)(unsigned, const unsigned char*, unsigned);
Crc32Fn_t Crc32_orig;
unsigned __cdecl Crc32_re(unsigned crc, const unsigned char* buf, unsigned len) {
    if (buf == 0) return 0;
    const unsigned* table = reinterpret_cast<const unsigned*>(JgldBase() + 0x126e18);
    crc = crc ^ 0xffffffffu;
    while (len >= 8) {
        for (int k = 0; k < 8; k++) crc = table[(crc ^ *buf++) & 0xff] ^ (crc >> 8);
        len -= 8;
    }
    if (len != 0) {
        do {
            crc = table[(crc ^ *buf++) & 0xff] ^ (crc >> 8);
        } while (--len != 0);
    }
    return crc ^ 0xffffffffu;
}

// 0x0009e370  adler32
// unsigned __cdecl adler32(unsigned adler, const unsigned char* buf, unsigned len): the two halves are split
// out of the argument (`and eax, 0xffff` at 0x1009e38b into [ebp-4], `shr ecx, 0x10` / `and ecx, 0xffff` at
// 0x1009e396-0x1009e399 into [ebp-8]). A null buffer returns 1 (`cmp dword ptr [ebp+0xc], 0` / `jne` at
// 0x1009e3a2-0x1009e3a6, `mov eax, 1` at 0x1009e3a8) - note that this happens after the split, so the
// argument's value is discarded. The outer loop runs while len > 0 unsigned (`cmp dword ptr [ebp+0x10], 0` /
// `jbe 0x1009e5ec` at 0x1009e3b2-0x1009e3b6, back jump at 0x1009e5e7) and takes a chunk of
// k = min(len, 0x15b0) (`cmp dword ptr [ebp+0x10], 0x15b0` / `jae` at 0x1009e3bc-0x1009e3c3, an unsigned
// compare; the chunk constant 0x15b0 = 5552 is stored at 0x1009e3cd), subtracting it from len at 0x1009e3dd.
// Inside the chunk, groups of sixteen bytes are added while k >= 16 (`cmp dword ptr [ebp-0xc], 0x10` /
// `jl 0x1009e593` at 0x1009e3e3-0x1009e3e7, a SIGNED compare, the group reading buf[0]..buf[0xf] at fixed
// offsets - the last at 0x1009e568 - and advancing the pointer by 0x10 at 0x1009e57f), then the rest one byte
// at a time (`cmp dword ptr [ebp-0xc], 0` / `je 0x1009e5c9` at 0x1009e593-0x1009e597, the do-while's `jne` at
// 0x1009e5c7). Each byte does s1 += b; s2 += s1. At the end of every chunk both halves are reduced modulo
// 0xfff1 with an unsigned `div` (0x1009e5d3 and 0x1009e5e2), and the result is (s2 << 16) | s1 (0x1009e5ef,
// 0x1009e5f2). No table, no write outside the locals.
typedef unsigned(__cdecl* Adler32Fn_t)(unsigned, const unsigned char*, unsigned);
Adler32Fn_t Adler32_orig;
unsigned __cdecl Adler32_re(unsigned adler, const unsigned char* buf, unsigned len) {
    unsigned s1 = adler & 0xffff;
    unsigned s2 = (adler >> 16) & 0xffff;
    if (buf == 0) return 1;
    while (len > 0) {
        int k = (len < 0x15b0u) ? static_cast<int>(len) : 0x15b0;
        len -= static_cast<unsigned>(k);
        while (k >= 16) {
            for (int j = 0; j < 16; j++) {
                s1 += buf[j];
                s2 += s1;
            }
            buf += 16;
            k -= 16;
        }
        if (k != 0) {
            do {
                s1 += *buf++;
                s2 += s1;
            } while (--k != 0);
        }
        s1 %= 0xfff1u;
        s2 %= 0xfff1u;
    }
    return (s2 << 16) | s1;
}

// 0x0009e310  inflate_set_dictionary
// void __cdecl inflate_set_dictionary(inflate_blocks_state* s, const unsigned char* d, unsigned n): copies n
// bytes of d into the sliding window the state holds at +0x24 through the CRT memcpy at 0x1007f3a0 (the
// pointer is loaded at 0x1009e333, `call` at 0x1009e337), then stores window + n into the field at +0x30
// (`add edx, dword ptr [ebp+0x10]` at 0x1009e345, store at 0x1009e34b) and copies that same value into the
// field at +0x2c (0x1009e354-0x1009e357). No branch, no return value, no other field touched; `ret` at
// 0x1009e36a.
typedef void(__cdecl* InflateSetDictionary_t)(void*, const unsigned char*, unsigned);
InflateSetDictionary_t InflateSetDictionary_orig;
void __cdecl InflateSetDictionary_re(void* s, const unsigned char* d, unsigned n) {
    char* st = static_cast<char*>(s);
    unsigned char* window = *reinterpret_cast<unsigned char**>(st + 0x24);
    JgldMemcpy()(window, d, n);
    *reinterpret_cast<unsigned char**>(st + 0x30) = window + n;
    *reinterpret_cast<unsigned char**>(st + 0x2c) = *reinterpret_cast<unsigned char**>(st + 0x30);
}

}  // namespace

SG_HOOK("jgld.dll", 0x00003f10, Vector3_scale, Vec3Scale_re, Vec3Scale_orig);
SG_HOOK("jgld.dll", 0x00008590, jgld_intersect, Intersect_re, Intersect_orig);
SG_HOOK("jgld.dll", 0x000085f0, jgld_equal, Equal_re, Equal_orig);
SG_HOOK("jgld.dll", 0x00007450, ListNode_dtor, ListNodeDtor_re, ListNodeDtor_orig);
SG_HOOK("jgld.dll", 0x00078790, png_reset_crc, PngResetCrc_re, PngResetCrc_orig);
SG_HOOK("jgld.dll", 0x00078940, png_info_init, PngInfoInit_re, PngInfoInit_orig);
SG_HOOK("jgld.dll", 0x00078f20, png_memcpy_check, PngMemcpyCheck_re, PngMemcpyCheck_orig);
SG_HOOK("jgld.dll", 0x00078f80, png_memset_check, PngMemsetCheck_re, PngMemsetCheck_orig);
SG_HOOK("jgld.dll", 0x0006e010, png_do_bgr, PngDoBgr_re, PngDoBgr_orig);
SG_HOOK("jgld.dll", 0x00071e80, png_do_unpack, PngDoUnpack_re, PngDoUnpack_orig);
SG_HOOK("jgld.dll", 0x00073310, png_do_gray_to_rgb, PngDoGrayToRgb_re, PngDoGrayToRgb_orig);
SG_HOOK("jgld.dll", 0x000724f0, png_do_read_swap_alpha, PngDoReadSwapAlpha_re, PngDoReadSwapAlpha_orig);
SG_HOOK("jgld.dll", 0x00072860, png_do_read_invert_alpha, PngDoReadInvertAlpha_re, PngDoReadInvertAlpha_orig);
SG_HOOK("jgld.dll", 0x0009ca70, jgld_crc32, Crc32_re, Crc32_orig);
SG_HOOK("jgld.dll", 0x0009e370, jgld_adler32, Adler32_re, Adler32_orig);
SG_HOOK("jgld.dll", 0x0009e310, inflate_set_dictionary, InflateSetDictionary_re, InflateSetDictionary_orig);

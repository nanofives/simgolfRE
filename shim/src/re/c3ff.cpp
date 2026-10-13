// Batch c3ff (round 33, jgld.dll): the last jgld render rows with a callee tree the stand-ins now cover.
//
//   group F - file output       Surface::saveImage 0x0000a550, Surface::writePcx8 0x00012140 (MappedFile through
//                               re/frida/js/filestandin.js)
//   group D - createDisplay 0x00065390 (export #1 get_graphsy_object_ptr; operator new through the heap stand-in)
//   group S - Surface::drawTo 0x00009fd0, Surface::drawToZ 0x0000a330 (a Sprite in the frame: re/frida/js/framemask.js)
//   group P - loadPng 0x000145a0 (jgld's libpng 1.0.5 read path, its own setjmp recovery)
//
// Purposes and per-jcc tables: log/c3/c3ff_purpose.md; registry: re/frida/registry.d/c3ff.py; fixtures:
// re/frida/js/fixtures.d/c3ff.js; what was not taken: log/c3/c3ff_notes.txt.
//
// SG_HOOK addresses are RVAs; every instruction cited in a comment is a VA (0x10000000 + RVA), the form the debug
// build's disassembly prints. jgld.dll is relocated at run time, so every module address (vtables, globals, callees,
// strings) is resolved against the running base. __thiscall is emulated with __fastcall (ecx = this, edx unused).
// Callees run through the incremental-link thunks the originals call (thunk -> body), so a callee hooked by another
// batch runs that batch's body. Every vtable call is `mov edx, [obj]; call [edx + off]` in the original.
#include "hooks.h"

namespace {

char* JgldBase() {
    static char* base = 0;
    if (!base) base = reinterpret_cast<char*>(GetModuleHandleA("jgld.dll"));
    return base;
}
void* JgldVA(unsigned va) { return JgldBase() + (va - 0x10000000); }
template <class T> T& G(unsigned va) { return *static_cast<T*>(JgldVA(va)); }
template <class T> T& F(void* obj, unsigned off) { return *reinterpret_cast<T*>(static_cast<char*>(obj) + off); }
template <class Fn> Fn At(unsigned va) { return reinterpret_cast<Fn>(JgldVA(va)); }
// Slot `off` of the vtable of `obj`.
template <class Fn> Fn Virt(void* obj, unsigned off) { return F<Fn>(F<void*>(obj, 0), off); }

typedef unsigned char byte;
typedef int(__fastcall* IntSlot_t)(void*, void*);                  // width, height, getField40, getField7c...
typedef void*(__fastcall* PtrSlot_t)(void*, void*);                // fwdVfn18, acquireDC2, getDepthPtr
typedef void(__fastcall* FlagSlot_t)(void*, void*, int);           // releaseDC2(flag)
typedef void*(__fastcall* Ctor_t)(void*, void*);
typedef void(__fastcall* Method_t)(void*, void*);

// ---- Surface vtable offsets (Surface vtable 0x1011d0b0, re/analysis/systems/jgld_2.md)
const unsigned kPixelAddr = 0x0c, kAcquireDC2 = 0x10, kFwdVfn18 = 0x18, kReleaseDC2 = 0x24, kFillRectDispatch = 0x44;
const unsigned kWidth = 0xd8, kHeight = 0xdc, kGetField40 = 0xe0, kGetDepthPtr = 0xe4, kGetField7c = 0xe8;
const unsigned kSetPalette = 0xec;

int Width(void* s) { return Virt<IntSlot_t>(s, kWidth)(s, 0); }
int Height(void* s) { return Virt<IntSlot_t>(s, kHeight)(s, 0); }
int Pitch(void* s) { return Virt<IntSlot_t>(s, kGetField40)(s, 0); }
void ReleaseDC2(void* s) { Virt<FlagSlot_t>(s, kReleaseDC2)(s, 0, 1); }

// =====================================================================================================================
// group F - file output
// =====================================================================================================================

// ---- callees (thunk -> body)
const unsigned kThunkWritePcx8 = 0x10001a6e;        // -> Surface::writePcx8 0x10012140
const unsigned kThunkMfCtor = 0x10001005;           // -> MappedFile::ctor 0x10002650
const unsigned kThunkMfDtor = 0x100012c1;           // -> MappedFile::dtor 0x10002820
const unsigned kThunkMfCreate = 0x100017bc;         // -> MappedFile::create 0x10002bd0
const unsigned kThunkMfCloseTruncate = 0x10001aeb;  // -> MappedFile::closeTruncate 0x10002ef0
const unsigned kStrcat = 0x1007f770, kStrlen = 0x1007f6e0, kMemcpy = 0x1007f3a0;   // jgld's CRT (called directly)
const unsigned kPcxExt = 0x1011d354;                // the extension string strcat'ed after the cut (0x10012203)
const unsigned kPalClient = 0x1012873c;             // the module palette client; its palette pointer is at +4

typedef char*(__cdecl* Strcat_t)(char*, const char*);
typedef unsigned(__cdecl* Strlen_t)(const char*);
typedef void*(__cdecl* Memcpy_t)(void*, const void*, unsigned);
typedef byte*(__fastcall* MfCreate_t)(void*, void*, const char*, unsigned, int);
typedef void(__fastcall* MfCloseTruncate_t)(void*, void*, byte*);
typedef int(__fastcall* PalGet_t)(void*, void*, byte*, int, int);
typedef int(__fastcall* WritePcx8_t)(void*, void*, const char*);

// 0x00012140  Surface::writePcx8(this, name): writes the 8-bit surface as a PCX file through a MappedFile kept in the
// frame ([ebp-0x24], MappedFile::ctor 0x1001217b, MappedFile::dtor on every exit 0x1001224e / 0x100122e3 /
// 0x10012747). The name is copied into an 80-byte buffer [ebp-0x108] (buf[0] = 0 0x10012187, strcat 0x10012199);
// from its terminator (strlen 0x100121a8) a pointer walks back while above the buffer start (`jbe` 0x100121cb) and
// not on '.' (`je` 0x100121d9); if it stops on '.' (`jne` 0x100121f8) the name is cut there (0x10012200) and the
// extension string 0x1011d354 appended (0x1001220f), else the name is kept whole. Pixels = slot +0x18 (fwdVfn18,
// 0x10012221); NULL (`jne` 0x10012238) -> 7. MappedFile::create (buf, width * height * 2 + 0x80, sequential 1)
// (slots +0xd8 / +0xdc 0x1001226a / 0x10012283, `lea eax, [esi + esi + 0x80]` 0x10012293, call 0x100122a5); NULL
// (`jne` 0x100122b7) -> releaseDC2 (1) (0x100122c5) and 4. A 128-byte PCX header is built in the frame
// ([ebp-0xa4]): bytes 0..3 = 0x0a, 5, 1, 8 (0x100122f3..0x10012308), words xmin = ymin = 0, xmax = width - 1
// (0x1001233b), ymax = height - 1 (0x1001235c), hdpi = vdpi = 0, byte 0x40 = 0, byte 0x41 = 1 (planes), word 0x42 =
// slot +0xe0 (0x10012394), words 0x44 / 0x46 / 0x48 = 0; every other header byte is left as the /GZ prologue fill
// 0xcc (`rep stosd` over [ebp-0x460, ebp-0xc), 0x10012172), and the 128 bytes are memcpy'd to the view (0x100123bd).
// Rows: height (slot +0xdc, 0x100123e1) counted down while > 0 (`jle` 0x1001240c); in each row x runs from 0 while
// x < slot +0xe0 (re-read every time, unsigned `jae` 0x10012439): run = 1, c = pix[x]; the run grows while
// x + run < slot +0xe0 - 1 (re-read, `jae` 0x1001249a), pix[x + run] == c (`jne` 0x100124c8) and run < 0x3f (`jge`
// 0x100124d9). A run > 1 (`jle` 0x100124ee) writes 0xc0 | run and c; a single byte >= 0x40 (`jge` 0x10012548) writes
// 0xc1 and c; any other single byte is written alone; x += run (0x100125bd). After a row pix += slot +0xe0
// (0x100125e5). Then 0x0c (0x100125f8); the palette object is this+0x7c, or (`je` 0x10012611) the +4 of the module
// client [0x1012873c] (0x10012627); a non-NULL one (`je` 0x10012637) fills a 768-byte frame table through its slot
// +0x10 (table, 0, 0x100) (0x1001265b), and when that returns 0 (`jne` 0x10012667) the 256 RGB triples are appended
// (loop 0x10012688..0x10012709). releaseDC2 (1) (0x1001271a), MappedFile::closeTruncate (end of the written bytes)
// (0x1001272e), 0. `ret 4`.
WritePcx8_t WritePcx8_orig;
int __fastcall WritePcx8_re(void* self, void*, const char* name) {
    void* mf[5];                                                                 // the MappedFile at [ebp-0x24]
    At<Ctor_t>(kThunkMfCtor)(mf, 0);                                             // 0x1001217b
    char buf[0x50];                                                              // [ebp-0x108]
    buf[0] = 0;                                                                  // 0x10012187
    At<Strcat_t>(kStrcat)(buf, name);                                            // 0x10012199
    char* p = buf + At<Strlen_t>(kStrlen)(buf);                                  // 0x100121a8 / 0x100121b0
    while (p > buf && *p != '.')                                                 // 0x100121cb / 0x100121d9
        --p;                                                                     // 0x100121e4
    if (*p == '.') {                                                             // 0x100121f8
        *p = 0;                                                                  // 0x10012200
        At<Strcat_t>(kStrcat)(buf, static_cast<const char*>(JgldVA(kPcxExt)));  // 0x1001220f
    }
    byte* pix = static_cast<byte*>(Virt<PtrSlot_t>(self, kFwdVfn18)(self, 0));   // 0x10012221
    if (pix == 0) {                                                              // 0x10012238
        At<Method_t>(kThunkMfDtor)(mf, 0);                                       // 0x1001224e
        return 7;                                                                // 0x1001223a
    }
    const int w = Width(self);                                                   // 0x1001226a
    const int h = Height(self);                                                  // 0x10012283
    byte* out = At<MfCreate_t>(kThunkMfCreate)(mf, 0, buf, static_cast<unsigned>(w * h * 2 + 0x80), 1);   // 0x100122a5
    if (out == 0) {                                                              // 0x100122b7
        ReleaseDC2(self);                                                        // 0x100122c5
        At<Method_t>(kThunkMfDtor)(mf, 0);                                       // 0x100122e3
        return 4;                                                                // 0x100122cf
    }
    byte hdr[0x80];
    for (int i = 0; i < 0x80; i++) hdr[i] = 0xcc;                                // the /GZ fill, 0x10012172
    hdr[0] = 0x0a; hdr[1] = 5; hdr[2] = 1; hdr[3] = 8;                           // 0x100122f3..0x10012308
    F<short>(hdr, 4) = 0;                                                        // 0x1001230f
    F<short>(hdr, 6) = 0;                                                        // 0x10012318
    F<short>(hdr, 8) = static_cast<short>(Width(self) - 1);                      // 0x1001232b / 0x1001233b
    F<short>(hdr, 0xa) = static_cast<short>(Height(self) - 1);                   // 0x1001234c / 0x1001235c
    F<short>(hdr, 0xc) = 0;                                                      // 0x10012363
    F<short>(hdr, 0xe) = 0;                                                      // 0x1001236c
    hdr[0x40] = 0;                                                               // 0x10012375
    hdr[0x41] = 1;                                                               // 0x10012379
    F<short>(hdr, 0x42) = static_cast<short>(Pitch(self));                       // 0x10012387 / 0x10012394
    F<short>(hdr, 0x44) = 0;                                                     // 0x10012398
    F<short>(hdr, 0x46) = 0;                                                     // 0x1001239e
    F<short>(hdr, 0x48) = 0;                                                     // 0x100123a4
    At<Memcpy_t>(kMemcpy)(out, hdr, 0x80);                                       // 0x100123bd
    out += 0x80;                                                                 // 0x100123cb
    for (int rows = Height(self); rows > 0; --rows) {                            // 0x100123e1 / 0x1001240c / 0x100123fc
        for (unsigned x = 0; x < static_cast<unsigned>(Pitch(self));) {          // 0x10012426 / 0x10012439
            byte run = 1;                                                        // 0x1001243f
            const byte c = pix[x];                                               // 0x10012452
            while (x + run < static_cast<unsigned>(Pitch(self) - 1)              // 0x10012488 / 0x1001249a
                   && c == pix[x + run]                                          // 0x100124c8
                   && run < 0x3f)                                                // 0x100124d9
                ++run;                                                           // 0x10012462
            if (run > 1) {                                                       // 0x100124ee
                *out++ = static_cast<byte>(run | 0xc0);                          // 0x10012505
                *out++ = c;                                                      // 0x10012522
            } else if (pix[x] >= 0x40) {                                         // 0x10012548
                *out++ = static_cast<byte>(run | 0xc0);                          // 0x1001257c
                *out++ = c;                                                      // 0x10012599
            } else {
                *out++ = c;                                                      // 0x10012556
            }
            x += run;                                                            // 0x100125bd
        }
        pix += Pitch(self);                                                      // 0x100125d2 / 0x100125e5
    }
    *out++ = 0x0c;                                                               // 0x100125f8
    void* pal = F<void*>(self, 0x7c);                                            // 0x1001260d
    if (pal == 0)                                                                // 0x10012611
        pal = F<void*>(G<void*>(kPalClient), 4);                                 // 0x10012621 / 0x10012627
    if (pal != 0) {                                                              // 0x10012637
        byte rgb[0x300];                                                         // [ebp-0x414]
        if (Virt<PalGet_t>(pal, 0x10)(pal, 0, rgb, 0, 0x100) == 0) {             // 0x1001265b / 0x10012667
            for (int i = 0; i < 0x100; i++) {                                    // 0x10012688 / 0x10012692
                *out++ = rgb[i * 3];                                             // 0x100126aa
                *out++ = rgb[i * 3 + 1];                                         // 0x100126d1
                *out++ = rgb[i * 3 + 2];                                         // 0x100126f8
            }
        }
    }
    ReleaseDC2(self);                                                            // 0x1001271a
    At<MfCloseTruncate_t>(kThunkMfCloseTruncate)(mf, 0, out);                    // 0x1001272e
    At<Method_t>(kThunkMfDtor)(mf, 0);                                           // 0x10012747
    return 0;                                                                    // 0x10012733 / 0x1001274c
}

// 0x0000a550  Surface::saveImage(this, name): slot 30 (+0x78) of the Surface vtable 0x1011d0b0. The format pair's
// first word (slot +0xe4 getDepthPtr 0x1000a577, `mov eax, [eax]` 0x1000a584) equal to 8 (`je` 0x1000a58d) returns
// Surface::writePcx8 (name) (thunk 0x10001a6e, 0x1000a598); any other depth returns 0x17 (0x1000a59f). `ret 4`.
typedef int(__fastcall* SaveImage_t)(void*, void*, const char*);
SaveImage_t SaveImage_orig;
int __fastcall SaveImage_re(void* self, void*, const char* name) {
    const int depth = *static_cast<int*>(Virt<PtrSlot_t>(self, kGetDepthPtr)(self, 0));   // 0x1000a577 / 0x1000a584
    if (depth == 8)                                                              // 0x1000a589 / 0x1000a58d
        return At<WritePcx8_t>(kThunkWritePcx8)(self, 0, name);                  // 0x1000a598
    return 0x17;                                                                 // 0x1000a59f
}

// =====================================================================================================================
// group D - createDisplay
// =====================================================================================================================

// 0x00065390  createDisplay() (cdecl, plain `ret` 0x10065402): export #1 get_graphsy_object_ptr, fetched with
// GetProcAddress and called without arguments by the exe's loadGraphsyObject 0x004a00a0 (0x004a00d3). operator new
// 0x1007f0f0 (0x14c) (0x100653ad); a non-NULL block (`je` 0x100653bc) is built by Display::Display 0x100654b0
// (thunk 0x100013f7, 0x100653c1), a NULL one gives 0 (0x100653cb). The result goes to 0x1012870c (0x100653db), then
// 0x1012870c is copied to the module display 0x10128420 (0x100653e7) and returned (0x100653ed).
typedef void*(__cdecl* New_t)(unsigned);
typedef void*(__cdecl* CreateDisplay_t)();
CreateDisplay_t CreateDisplay_orig;
void* __cdecl CreateDisplay_re() {
    void* p = At<New_t>(0x1007f0f0)(0x14c);                                      // 0x100653ad
    void* d = p != 0 ? At<Ctor_t>(0x100013f7)(p, 0) : 0;                         // 0x100653bc / 0x100653c1 / 0x100653cb
    G<void*>(0x1012870c) = d;                                                    // 0x100653db
    G<void*>(0x10128420) = G<void*>(0x1012870c);                                 // 0x100653e1 / 0x100653e7
    return G<void*>(0x1012870c);                                                 // 0x100653ed
}

// =====================================================================================================================
// group S - Surface::drawTo / drawToZ: a temporary Sprite over this surface's pixels, drawn with Sprite::drawB / drawG
// =====================================================================================================================
const unsigned kThunkSpriteCtorN = 0x100018de;      // -> Sprite::ctorN 0x10014cd0
const unsigned kThunkSpriteDtor = 0x10001735;       // -> Sprite::dtor 0x10014de0
const unsigned kThunkSetField28 = 0x100011ae;       // -> setField28 0x1000aeb0
const unsigned kThunkDrawB = 0x10001a5a;            // -> Sprite::drawB 0x10015480
const unsigned kThunkDrawG = 0x100017b2;            // -> Sprite::drawG 0x100165a0
const unsigned kSpriteSize = 0x50;                  // [ebp-0x60, ebp-0x10); Factory::createSprite news 0x50 bytes

typedef void*(__fastcall* SpriteCtorN_t)(void*, void*, void*);
typedef void(__fastcall* SetField28_t)(void*, void*, void*);
typedef int(__fastcall* DrawB_t)(void*, void*, void*, int, int, void*);
typedef int(__fastcall* DrawG_t)(void*, void*, void*, int, int, int, void*);

// The common prologue of both bodies: Sprite::ctorN (owner 0) on the frame Sprite, then the surface's fields copied
// into it, in the original's order (drawTo's addresses; drawToZ's are 0x360 higher up to the setField28 call).
void BuildSprite(void* self, void* spr) {
    At<SpriteCtorN_t>(kThunkSpriteCtorN)(spr, 0, 0);                             // 0x1000a00d
    F<int>(spr, 0x20) = F<int>(self, 0x24);                                      // 0x1000a01c / 0x1000a01f
    F<void*>(spr, 0x14) = Virt<PtrSlot_t>(self, kAcquireDC2)(self, 0);           // 0x1000a02c / 0x1000a036
    F<int>(spr, 0x18) = 0;                                                       // 0x1000a039
    F<int>(spr, 0x2c) = F<int>(self, 0x40);                                      // 0x1000a043 / 0x1000a046
    F<int>(spr, 0x30) = Width(self);                                             // 0x1000a053 / 0x1000a060
    F<int>(spr, 0x34) = Height(self);                                            // 0x1000a06d / 0x1000a07a
    F<void*>(spr, 0x10) = F<void*>(self, 0x7c);                                  // 0x1000a080 / 0x1000a083
    At<SetField28_t>(kThunkSetField28)(spr, 0, F<void*>(self, 0x4d0));           // 0x1000a089 / 0x1000a093
}
// The common epilogue: releaseDC2 (1), the Sprite's pixel word cleared, Sprite::dtor.
void DropSprite(void* self, void* spr) {
    ReleaseDC2(self);                                                            // 0x1000a0d3
    F<void*>(spr, 0x14) = 0;                                                     // 0x1000a0dd
    At<Method_t>(kThunkSpriteDtor)(spr, 0);                                      // 0x1000a0f4
}

// 0x00009fd0  Surface::drawTo(this, dst, x, y): slot 33 (+0x84) of the Surface vtable 0x1011d0b0. A Sprite is built
// in the frame at [ebp-0x60] (Sprite::ctorN with owner 0, thunk 0x100018de, 0x1000a00d) and filled from this surface:
// +0x20 = this+0x24 (depth), +0x14 = slot +0x10 acquireDC2 (0x1000a02c), +0x18 = 0, +0x2c = this+0x40, +0x30 = width
// (slot +0xd8, 0x1000a053), +0x34 = height (slot +0xdc, 0x1000a06d), +0x10 = this+0x7c (palette), then setField28
// (this+0x4d0) on it (thunk 0x100011ae, 0x1000a093). Sprite::drawB (dst, x, y, slot +0xe8 getField7c 0x1000a0a2)
// (thunk 0x10001a5a, 0x1000a0bf) gives the result. Then releaseDC2 (1) (0x1000a0d3), Sprite +0x14 = 0 (0x1000a0dd)
// and Sprite::dtor (thunk 0x10001735, 0x1000a0f4). The SEH frame (0x10009fd3..0x10009fe1) only brackets the
// Sprite's lifetime (try level 0 at 0x1000a012, -1 at 0x1000a0ea). `ret 0xc`.
typedef int(__fastcall* DrawTo_t)(void*, void*, void*, int, int);
DrawTo_t DrawTo_orig;
int __fastcall DrawTo_re(void* self, void*, void* dst, int x, int y) {
    alignas(4) byte spr[kSpriteSize];
    BuildSprite(self, spr);
    void* pal = Virt<PtrSlot_t>(self, kGetField7c)(self, 0);                     // 0x1000a0a2
    const int r = At<DrawB_t>(kThunkDrawB)(spr, 0, dst, x, y, pal);              // 0x1000a0bf
    DropSprite(self, spr);
    return r;                                                                    // 0x1000a0e4 / 0x1000a0f9
}

// 0x0000a330  Surface::drawToZ(this, dst, x, y, z): slot 35 (+0x8c) of the Surface vtable 0x1011d0b0. drawTo's body
// with one more argument: the same Sprite at [ebp-0x60] built the same way (Sprite::ctorN 0x1000a36d, fields
// 0x1000a37c..0x1000a3e3, setField28 0x1000a3f3), then Sprite::drawG (dst, x, y, z, slot +0xe8 0x1000a402) (thunk
// 0x100017b2, 0x1000a423) gives the result; releaseDC2 (1) (0x1000a437), +0x14 = 0 (0x1000a441), Sprite::dtor
// (0x1000a458). `ret 0x10`.
typedef int(__fastcall* DrawToZ_t)(void*, void*, void*, int, int, int);
DrawToZ_t DrawToZ_orig;
int __fastcall DrawToZ_re(void* self, void*, void* dst, int x, int y, int z) {
    alignas(4) byte spr[kSpriteSize];
    BuildSprite(self, spr);
    void* pal = Virt<PtrSlot_t>(self, kGetField7c)(self, 0);                     // 0x1000a402
    const int r = At<DrawG_t>(kThunkDrawG)(spr, 0, dst, x, y, z, pal);           // 0x1000a423
    DropSprite(self, spr);
    return r;                                                                    // 0x1000a448 / 0x1000a45d
}

// =====================================================================================================================
// group P - loadPng
// =====================================================================================================================
const unsigned kFopen = 0x1007faa0, kFclose = 0x1007f850, kSetjmp3 = 0x1007fab8;     // jgld's CRT (called directly)
const unsigned kPngCreateRead = 0x1006bac0, kPngCreateInfo = 0x10078870, kPngDestroyRead = 0x1006d150;
const unsigned kPngInitIo = 0x10078ad0, kPngReadInfo = 0x1006be20, kPngGetIHDR = 0x1006eae0;
const unsigned kPngSetSwap = 0x1006d630, kPngSetStripAlpha = 0x1006f360, kPngGetPLTE = 0x1006ee50;
const unsigned kPngSetBgr = 0x1006d600, kPngReadImage = 0x1006cc50, kPngReadEnd = 0x1006cd00;
const unsigned kThunkTrace = 0x1000117c;            // -> trace 0x10014550 (OutputDebugStringA of the message)
const unsigned kThunkJalloc = 0x100017c6, kThunkJfree = 0x100017b7;   // -> jalloc 0x10014a40, jfree 0x10014a90

typedef void*(__cdecl* Fopen_t)(const char*, const char*);
typedef int(__cdecl* Fclose_t)(void*);
typedef int(__cdecl* Setjmp3_t)(void*, int);
typedef void*(__cdecl* PngCreateRead_t)(const char*, void*, void*, void*);
typedef void*(__cdecl* PngCreateInfo_t)(void*);
typedef void(__cdecl* PngDestroyRead_t)(void* volatile*, void* volatile*, void*);
typedef void(__cdecl* PngVV_t)(void*, void*);
typedef void(__cdecl* PngV_t)(void*);
typedef int(__cdecl* PngGetIHDR_t)(void*, void*, unsigned*, unsigned*, int*, int*, int*, int*, int*);
typedef int(__cdecl* PngGetPLTE_t)(void*, void*, void**, int*);
typedef void*(__cdecl* Jalloc_t)(unsigned);
typedef void(__cdecl* Jfree_t)(void*);
typedef void(__fastcall* PalSet_t)(void*, void*, void*, int, int);
typedef void(__fastcall* PtrArg_t)(void*, void*, void*);
typedef int(__fastcall* Alloc3_t)(void*, void*, unsigned, unsigned, int);
typedef void(__fastcall* Two_t)(void*, void*, int, int);
typedef void*(__fastcall* PixelAddr_t)(void*, void*, int, unsigned);

// The four dwords from [ebp-0x3c] up in the original's frame, in that order: the palette loop counter, num_palette
// (png_get_PLTE's out word, preset to 0x100 at 0x1001475a) and the palette pointer (png_get_PLTE's other out word).
struct PlteLocals { int i; int num; void* palette; };

// 0x000145a0  loadPng(this, name, pal, a3, a4): slot 29 (+0x74) of the Surface vtable 0x1011d0b0, `ret 0x10`.
// fopen (name, "rb" 0x1011d370) 0x100145d3: NULL (`jne` 0x100145e2) -> 6. png_create_read_struct (the version string
// 0x1011d368, NULL, trace, trace) (0x100145ff; error and warning function both the thunk 0x1000117c): NULL (`jne`
// 0x1001460e) -> fclose, 1. png_create_info_struct (png) 0x1001462a: NULL (`jne` 0x10014639) -> fclose,
// png_destroy_read_struct (&png, NULL, NULL), 1. _setjmp3 (png, 0) 0x10014667: nonzero, i.e. a longjmp back from
// png_error (`je` 0x10014671 not taken) -> png_destroy_read_struct (&png, &info, NULL), fclose, 1. png_init_io (png,
// fp) 0x100146a3, png_read_info 0x100146b3, png_get_IHDR (png, info, &w, &h, &depth, &type, &interlace, NULL, NULL)
// 0x100146db. type != 2 (`je` 0x100146e7) and info->pixel_depth +0x1e != 24 (`je` 0x100146f4) -> destroy, fclose,
// 0x17. depth == 16 (`jne` 0x10014722) -> png_set_swap 0x10014728; type & 4 (`je` 0x10014738) ->
// png_set_strip_alpha 0x1001473e. pal != NULL (`je` 0x1001474a) and type == 3 (`jne` 0x10014754): num = 0x100,
// png_get_PLTE (png, info, &palette, &num) 0x10014771, 256 RGB triples copied into a frame table [ebp-0x33c] (loop
// 0x1001478b..0x100147e1), then pal's slot +0x14 called with (ebp-0x33c + i * 3, a3, a4) where i is 0x100 after the
// loop, i.e. the address of i itself at [ebp-0x3c] (`lea eax, [ebp + edx - 0x33c]` 0x100147f3), and this->setPalette
// (pal) (slot +0xec, 0x1001481b). A float 1.0 is stored to [ebp-0x2c] and never read (0x10014828). png_set_bgr
// 0x10014833; this+0x80's slot +4 (w, h, info->pixel_depth) 0x10014862 (result stored to [ebp-0x30], never read);
// slot +0x44 (0, 2) 0x1001487d; rows = jalloc (h * 4) (thunk 0x100017c6, 0x1001488e); rows[i] = slot +0xc (0, i)
// for i < h (unsigned `jae` 0x100148c0, 0x100148d5); png_read_image (png, rows) 0x100148fb, png_read_end (png, info)
// 0x1001490b, png_destroy_read_struct (&png, &info, NULL) 0x1001491d, fclose 0x10014929, jfree (rows) (thunk
// 0x100017b7, 0x10014938), 0 (0x10014940). A longjmp out of png_read_image / png_read_end lands on the setjmp exit,
// which never frees rows.
typedef int(__fastcall* LoadPng_t)(void*, void*, const char*, void*, int, int);
LoadPng_t LoadPng_orig;
int __fastcall LoadPng_re(void* self, void*, const char* name, void* pal, int a3, int a4) {
    void* volatile fp = At<Fopen_t>(kFopen)(name, static_cast<const char*>(JgldVA(0x1011d370)));   // 0x100145d3
    if (fp == 0)                                                                 // 0x100145e2
        return 6;                                                                // 0x100145e4
    void* volatile png = At<PngCreateRead_t>(kPngCreateRead)(static_cast<const char*>(JgldVA(0x1011d368)), 0,
                                                             JgldVA(kThunkTrace), JgldVA(kThunkTrace));   // 0x100145ff
    if (png == 0) {                                                              // 0x1001460e
        At<Fclose_t>(kFclose)(fp);                                               // 0x10014614
        return 1;                                                                // 0x1001461c
    }
    void* volatile info = At<PngCreateInfo_t>(kPngCreateInfo)(png);              // 0x1001462a
    if (info == 0) {                                                             // 0x10014639
        At<Fclose_t>(kFclose)(fp);                                               // 0x1001463f
        At<PngDestroyRead_t>(kPngDestroyRead)(&png, 0, 0);                       // 0x1001464f
        return 1;                                                                // 0x10014657
    }
    if (At<Setjmp3_t>(kSetjmp3)(png, 0)) {                                       // 0x10014667 / 0x10014671
        At<PngDestroyRead_t>(kPngDestroyRead)(&png, &info, 0);                   // 0x1001467d
        At<Fclose_t>(kFclose)(fp);                                               // 0x10014689
        return 1;                                                                // 0x10014691
    }
    At<PngVV_t>(kPngInitIo)(png, fp);                                            // 0x100146a3
    At<PngVV_t>(kPngReadInfo)(png, info);                                        // 0x100146b3
    unsigned w, h;
    int depth, type, interlace;
    At<PngGetIHDR_t>(kPngGetIHDR)(png, info, &w, &h, &depth, &type, &interlace, 0, 0);   // 0x100146db
    if (type != 2 && F<byte>(info, 0x1e) != 0x18) {                              // 0x100146e7 / 0x100146f4
        At<PngDestroyRead_t>(kPngDestroyRead)(&png, &info, 0);                   // 0x10014700
        At<Fclose_t>(kFclose)(fp);                                               // 0x1001470c
        return 0x17;                                                             // 0x10014714
    }
    if (depth == 0x10)                                                           // 0x1001471e / 0x10014722
        At<PngV_t>(kPngSetSwap)(png);                                            // 0x10014728
    if (type & 4)                                                                // 0x10014733 / 0x10014738
        At<PngV_t>(kPngSetStripAlpha)(png);                                      // 0x1001473e
    if (pal != 0 && type == 3) {                                                 // 0x1001474a / 0x10014754
        PlteLocals loc;
        loc.num = 0x100;                                                         // 0x1001475a
        At<PngGetPLTE_t>(kPngGetPLTE)(png, info, &loc.palette, &loc.num);        // 0x10014771
        byte rgb[0x300];                                                         // [ebp-0x33c]
        const byte* src = static_cast<const byte*>(loc.palette);
        for (loc.i = 0; loc.i < 0x100; loc.i++) {                                // 0x1001478b / 0x10014792
            rgb[loc.i * 3] = src[loc.i * 3];                                     // 0x100147a6
            rgb[loc.i * 3 + 1] = src[loc.i * 3 + 1];                             // 0x100147c0
            rgb[loc.i * 3 + 2] = src[loc.i * 3 + 2];                             // 0x100147da
        }
        (void)rgb;
        Virt<PalSet_t>(pal, 0x14)(pal, 0, &loc.i, a3, a4);                       // 0x100147f3 / 0x10014803
        Virt<PtrArg_t>(self, kSetPalette)(self, 0, pal);                         // 0x1001481b
    }
    At<PngV_t>(kPngSetBgr)(png);                                                 // 0x10014833
    void* buf = F<void*>(self, 0x80);
    Virt<Alloc3_t>(buf, 4)(buf, 0, w, h, F<byte>(info, 0x1e));                   // 0x10014862
    Virt<Two_t>(self, kFillRectDispatch)(self, 0, 0, 2);                         // 0x1001487d
    void** rows = static_cast<void**>(At<Jalloc_t>(kThunkJalloc)(h * 4));        // 0x1001488e
    for (unsigned i = 0; i < h; i++)                                             // 0x100148bd / 0x100148c0
        rows[i] = Virt<PixelAddr_t>(self, kPixelAddr)(self, 0, 0, i);            // 0x100148d5 / 0x100148eb
    At<PngVV_t>(kPngReadImage)(png, rows);                                       // 0x100148fb
    At<PngVV_t>(kPngReadEnd)(png, info);                                         // 0x1001490b
    At<PngDestroyRead_t>(kPngDestroyRead)(&png, &info, 0);                       // 0x1001491d
    At<Fclose_t>(kFclose)(fp);                                                   // 0x10014929
    At<Jfree_t>(kThunkJfree)(rows);                                              // 0x10014938
    return 0;                                                                    // 0x10014940
}

}  // namespace

// 0x0000a550  Surface::saveImage
SG_HOOK("jgld.dll", 0x0000a550, jgld_Surface_saveImage, SaveImage_re, SaveImage_orig);
// 0x00012140  Surface::writePcx8
SG_HOOK("jgld.dll", 0x00012140, jgld_Surface_writePcx8, WritePcx8_re, WritePcx8_orig);
// 0x00065390  createDisplay
SG_HOOK("jgld.dll", 0x00065390, jgld_createDisplay, CreateDisplay_re, CreateDisplay_orig);
// 0x00009fd0  Surface::drawTo
SG_HOOK("jgld.dll", 0x00009fd0, jgld_Surface_drawTo, DrawTo_re, DrawTo_orig);
// 0x0000a330  Surface::drawToZ
SG_HOOK("jgld.dll", 0x0000a330, jgld_Surface_drawToZ, DrawToZ_re, DrawToZ_orig);
// 0x000145a0  loadPng
SG_HOOK("jgld.dll", 0x000145a0, jgld_loadPng, LoadPng_re, LoadPng_orig);

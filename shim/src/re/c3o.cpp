// C3 batch c3o (2026-10-07): ui/input functions, hand-written from the C2 transcriptions
// re/analysis/<subsystem>/<addr>_<name>.md and the disassembly (re/tools/asm2inline.py). Each body states the
// facts it relies on with their addresses. __thiscall is emulated with __fastcall (ecx = this, edx unused).
// Callees are reached through their original addresses (a hooked callee runs its reimplementation on both arms,
// so the A/B stays consistent); virtual slots are called through the object's own vtable, as the original does.
// Game strings are referenced by their image addresses, never reproduced here.
#include "hooks.h"
#include <string.h>

namespace {

// ---------------------------------------------------------------------------------------------------------------
// Window input handlers. Shared facts (0x47c977/0x47c983, 0x47c5d8/0x47c5e8, 0x47c436/0x47c446): a window whose
// flags dword +0x9c has bit 0x200000 set, or whose byte +0xa0 has bit 0x8 set, ignores the event and returns 0.
// Every handler that does not bail out stores `this` into g_curWidget 0x0083ab2c before its callbacks run.
// ---------------------------------------------------------------------------------------------------------------
struct Win;
typedef void* VFn;
inline VFn* Vtbl(void* o) { return *reinterpret_cast<VFn**>(o); }
template <typename T> inline T& Field(void* o, int off) { return *reinterpret_cast<T*>(reinterpret_cast<char*>(o) + off); }
inline bool WinIgnores(void* w) {
    return (Field<unsigned>(w, 0x9c) & 0x200000u) != 0 || (Field<unsigned char>(w, 0xa0) & 0x08) != 0;
}
inline void*& CurWidget() { return *reinterpret_cast<void**>(0x0083ab2c); }

// The focus child: when +0x140 is non-zero, the child is the window at +4 of the node at +0x13c, or NULL when
// +0x138 is zero (0x47c99a..0x47c9af, 0x47c635..0x47c64a).
inline void* FocusChild(void* w) {
    return Field<int>(w, 0x138) != 0 ? Field<void*>(Field<void*>(w, 0x13c), 4) : nullptr;
}

typedef int(__fastcall* WinCmd_t)(void*, void*, int);              // thiscall (this, cmd)
typedef int(__fastcall* WinKey_t)(void*, void*, int, int);         // thiscall (this, a, vk)
typedef void(__fastcall* WinMouse_t)(void*, void*, int, int, int); // thiscall (this, x, y, release)
typedef int(__fastcall* Thiscall1_t)(void*, void*, int);
typedef int(__fastcall* Thiscall2_t)(void*, void*, int, int);
typedef int(__fastcall* Thiscall3_t)(void*, void*, int, int, int);
typedef int(__fastcall* Thiscall0_t)(void*, void*);
typedef int(__cdecl* Cb1_t)(int);
typedef int(__cdecl* Cb2_t)(int, int);
typedef int(__fastcall* HitRect_t)(void*, void*, int, int, int*, int*, int*);  // thiscall, 5 stack args

const WinCmd_t kDispatchCommand = reinterpret_cast<WinCmd_t>(0x0047c970);
const WinKey_t kWindowKey = reinterpret_cast<WinKey_t>(0x0047c5d0);
const HitRect_t kHitTestRect = reinterpret_cast<HitRect_t>(0x00492b10);  // HotList::hitTestRect

// The panel objects at +0x30 / +0x38 / +0x40 / +0x64 are notified through their vtable slot 0x1c (no arguments).
inline void NotifyPanel(void* panel) {
    if (panel) reinterpret_cast<Thiscall0_t>(Vtbl(panel)[0x1c / 4])(panel, nullptr);
}

// 0x0047c970  Window::dispatchCommand(cmd): offers a command id to the focus child first (recursively, 0x47c9b2);
// a non-zero answer there returns 1 (0x47c9bf). Otherwise: g_curWidget = this (0x47c9c8); result = the cdecl
// callback +0x264 (cmd) when set (0x47c9d9), plus the virtual slot 0x94 (cmd) (0x47c9e5); the panel +0x40 is then
// notified through its slot 0x1c (0x47c9f6). Returns the sum.
WinCmd_t DispatchCommand_orig;
int __fastcall DispatchCommand_re(void* self, void*, int cmd) {
    if (WinIgnores(self)) return 0;
    if (Field<int>(self, 0x140) != 0) {
        if (kDispatchCommand(FocusChild(self), nullptr, cmd) != 0) return 1;
    }
    CurWidget() = self;
    int result = 0;
    if (Cb1_t cb = Field<Cb1_t>(self, 0x264)) result = cb(cmd);
    result += reinterpret_cast<Thiscall1_t>(Vtbl(self)[0x94 / 4])(self, nullptr, cmd);
    NotifyPanel(Field<void*>(self, 0x40));
    return result;
}

// 0x0047c5d0  Window::key(a, vk): Enter (vk 0x0d or 0x1000d, 0x47c5f9/0x47c603) calls the virtual slot 0xd4 with
// -1 and Esc (0x1b, 0x47c5fe) with -2 (close codes, return ignored). Then, when +0x140 is set: vk 9 (Tab,
// 0x47c64c) calls the focus child's slot 0x128 (no arguments) and continues below; any other vk is offered to the
// focus child through Window::key (0x47c697), whose non-zero answer returns 1 (0x47c6a5). Below: g_curWidget = this
// (0x47c659); result = the cdecl callback +0x25c (a, vk) when set (0x47c66b) plus the virtual slot 0x8c (a, vk)
// (0x47c678); the panel +0x38 is notified through its slot 0x1c (0x47c689). Returns the sum.
WinKey_t WindowKey_orig;
int __fastcall WindowKey_re(void* self, void*, int a, int vk) {
    if (WinIgnores(self)) return 0;
    if (vk == 0x0d || vk == 0x1000d) {
        reinterpret_cast<Thiscall1_t>(Vtbl(self)[0xd4 / 4])(self, nullptr, -1);
    } else if (vk == 0x1b) {
        reinterpret_cast<Thiscall1_t>(Vtbl(self)[0xd4 / 4])(self, nullptr, -2);
    }
    if (Field<int>(self, 0x140) != 0) {
        void* child = FocusChild(self);
        if (vk == 9) {
            reinterpret_cast<Thiscall0_t>(Vtbl(child)[0x128 / 4])(child, nullptr);
        } else if (kWindowKey(child, nullptr, a, vk) != 0) {
            return 1;
        }
    }
    CurWidget() = self;
    int result = 0;
    if (Cb2_t cb = Field<Cb2_t>(self, 0x25c)) result = cb(a, vk);
    result += reinterpret_cast<Thiscall2_t>(Vtbl(self)[0x8c / 4])(self, nullptr, a, vk);
    NotifyPanel(Field<void*>(self, 0x38));
    return result;
}

// 0x0047c430  Window::mouseDispatch254(x, y, release): when release (third argument) is non-zero (0x47c459): the
// virtual slot 0xb4 (x, y) (0x47c4e5), then the panel +0x64 is notified (0x47c4f4). Otherwise g_curWidget = this
// (0x47c45b); the cdecl callback +0x254 (x, y) runs when set (0x47c477); the hot-spot list embedded at +0xbc is
// hit-tested: hitTestRect(x, y, &a, &b, &rect) where a and b are the x and y argument slots reused as out-params
// and rect a 16-byte local (pushes at 0x47c47c..0x47c48c). A hit (index >= 0, 0x47c49a) calls the virtual slot
// 0x44 (a, b, &rect) (0x47c49c..0x47c4af); when there is no hit or slot 0x44 returns 0 (0x47c4b4), the virtual slot
// 0x84 runs with the original x, y kept in ebx/edi (0x47c4b6..0x47c4bc). Finally the panel +0x30 is notified
// (0x47c4cd). Returns nothing (matched as a void method, re/match/golf_hand_06_win.cpp).
WinMouse_t MouseDispatch254_orig;
void __fastcall MouseDispatch254_re(void* self, void*, int x, int y, int release) {
    if (WinIgnores(self)) return;
    if (release != 0) {
        reinterpret_cast<Thiscall2_t>(Vtbl(self)[0xb4 / 4])(self, nullptr, x, y);
        NotifyPanel(Field<void*>(self, 0x64));
        return;
    }
    CurWidget() = self;
    if (Cb2_t cb = Field<Cb2_t>(self, 0x254)) cb(x, y);
    int a = x, b = y;   // the argument slots, overwritten only on a hit (hitTestRect 0x492b51..0x492b7b)
    int rect[4];
    const int hit = kHitTestRect(reinterpret_cast<char*>(self) + 0xbc, nullptr, x, y, &a, &b, rect);
    bool handled = false;
    if (hit >= 0) {
        handled = reinterpret_cast<Thiscall3_t>(Vtbl(self)[0x44 / 4])(
                      self, nullptr, a, b, reinterpret_cast<int>(rect)) != 0;
    }
    if (!handled) reinterpret_cast<Thiscall2_t>(Vtbl(self)[0x84 / 4])(self, nullptr, x, y);
    NotifyPanel(Field<void*>(self, 0x30));
}

// 0x0047d840  setDragTarget(w): stores its argument into the global 0x0083ab60 (0x47d844), which
// currentFocusOwner 0x0047f2f0 reads as the last fallback when 0x0083ab54 and 0x0083ab38 are both zero (0x47f312).
typedef void(__cdecl* SetDrag_t)(void*);
SetDrag_t SetDragTarget_orig;
void __cdecl SetDragTarget_re(void* w) { *reinterpret_cast<void**>(0x0083ab60) = w; }

// ---------------------------------------------------------------------------------------------------------------
// 0x0040a160  buildToolLabel(x, y): writes the build/remove cursor label for tile (x, y) into the shared text buffer
// g_textBuffer 0x0051a068 and returns 1 when the label names a removable placed item, else 0. Tables (cell = x*50+y,
// 0x40a1a6..0x40a1b5): flag words at 0x0053caf0 + 2*cell, an item byte at 0x005830b8 + cell, the placed-object
// records 0x0058bcb8 (16 bytes, type word at +0), object names at 0x004c26b0 + 20*type and item names at
// 0x00578350 + 48*item. Strings are image addresses: 0x4c515c, 0x4c5150, 0x4c5144, 0x4c5134, 0x4c5128, 0x4c5118,
// 0x4c5104, 0x4c50ec, 0x4c50e0, 0x4c50d8.
// ---------------------------------------------------------------------------------------------------------------
typedef int(__cdecl* Int2_t)(int, int);
const Int2_t kTileBlocked = reinterpret_cast<Int2_t>(0x0040bf60);
const Int2_t kObjectAt = reinterpret_cast<Int2_t>(0x0040df80);
inline char* TextBuf() { return reinterpret_cast<char*>(0x0051a068); }
inline const char* Img(unsigned va) { return reinterpret_cast<const char*>(va); }
inline const char* ObjectName(int idx) {
    const short type = *reinterpret_cast<const short*>(0x0058bcb8 + idx * 16);   // movsx word (0x40a232)
    return Img(0x004c26b0 + type * 20);
}

Int2_t BuildToolLabel_orig;
int __cdecl BuildToolLabel_re(int x, int y) {
    if (kTileBlocked(x, y) != 0) {                       // 0x40a178: out of the map or blocked
        strcpy(TextBuf(), Img(0x004c515c));
        return 0;
    }
    TextBuf()[0] = 0;                                    // 0x40a1a9
    const int cell = x * 50 + y;
    const unsigned short flags = *reinterpret_cast<const unsigned short*>(0x0053caf0 + cell * 2);
    if (flags & 0x8000) {                                // 0x40a1c0 (test ah, 0x80)
        strcpy(TextBuf(), Img(0x004c5150));
        return 0;
    }
    if (flags & 0x400) {                                 // 0x40a1ef: a placed object covers the tile
        const int idx = kObjectAt(x, y);
        if (idx != -1) {                                 // 0x40a205
            strcpy(TextBuf(), Img(0x004c5144));
            strcat(TextBuf(), ObjectName(idx));          // 0x40a232..0x40a38f
            return 1;
        }
    }
    const unsigned char item = *reinterpret_cast<const unsigned char*>(0x005830b8 + cell);   // 0x40a248
    if (item != 0xff) {                                  // 0x40a24f
        if (item & 0x80) {                               // 0x40a258: one of the fixed removable kinds
            strcpy(TextBuf(), Img(0x004c5134));
            const unsigned char kind = item & 0x7f;      // 0x40a284
            if (kind == 0) strcpy(TextBuf(), Img(0x004c5128));
            if (kind == 1) strcpy(TextBuf(), Img(0x004c5118));
            if (kind == 0x10) strcpy(TextBuf(), Img(0x004c5104));
            if (kind == 0x13) strcpy(TextBuf(), Img(0x004c50ec));
            return 1;                                    // 0x40a391
        }
        if (static_cast<signed char>(item) < 0x7d) {     // 0x40a328 (signed compare; bit 7 is clear here)
            strcat(TextBuf(), Img(0x004c50e0));          // appended to the empty buffer (0x40a341..0x40a35b)
            strcat(TextBuf(), Img(0x00578350 + static_cast<signed char>(item) * 48));
            return 1;
        }
    }
    // 0x40a39b: no removable item (0xff, or 0x7d..0x7f): the tile's flag word decides
    if (flags & 0x200) {
        strcpy(TextBuf(), Img(0x004c5118));
        return 0;
    }
    if (flags & 0x400) {                                 // 0x40a3da
        const int idx = kObjectAt(x, y);
        if (idx != -1) {
            strcpy(TextBuf(), Img(0x004c50d8));
            strcat(TextBuf(), ObjectName(idx));
            return 0;
        }
    }
    if (flags & 0x20) {                                  // 0x40a463
        strcpy(TextBuf(), Img(0x004c5128));
        return 0;
    }
    if (flags & 0x1000) {                                // 0x40a494
        strcpy(TextBuf(), Img(0x004c5134));
        return 0;
    }
    TextBuf()[0] = 0;                                    // 0x40a4cc
    return 0;
}

// ---------------------------------------------------------------------------------------------------------------
// 0x00477280  drawMarkupRun(text, len): pixel width of `len` characters of marked-up text, measured with the
// style's fonts. Object fields: +0x38 current style (0..3), +0x3c link state, +0x40 markup enabled, +0x4c
// dropdown state, +0x54 selects the dropdown arrow width, +0x5c + 4*style the font per style (+0x5c is the base
// font, filled from the default font 0x0083ad44 when zero, 0x47728c). Widths come from measureTextWidth
// 0x00483930 (font, text, count). Markup tokens (switch at 0x477315, tables 0x4774d8/0x4774f8):
//   `{` style 1, `[` style 2, `}` `]` style 0, doubled `{{` `[[` `}}` `]]` are a literal character;
//   `^` dropdown arrow (+0x4c = 2, style 0); `=` starts a `=...>` span that is skipped (+0x3c = 2);
//   `$` followed by the 6-byte token at 0x4e4254 (+0x3c = 1, style 3, 5 more bytes skipped), the 9-byte token at
//   0x4e4248 (+0x4c = 1, 9 more bytes skipped) or the 9-byte token at 0x4e423c (+0x4c = 1, style 3, 9 more skipped).
// The scanner is scanToken 0x476dd0 while +0x4c == 1, skipToken 0x476d80 while the style is 3, else
// scanMarkupText 0x476d40; each returns the next delimiter and leaves the remaining count (delimiter included)
// in the count slot, or 0 when the run ends first.
// ---------------------------------------------------------------------------------------------------------------
typedef int(__fastcall* Measure_t)(void*, void*, const char*, int);
typedef const char*(__cdecl* Scan_t)(const char*, int*);
typedef int(__cdecl* StrNCmp_t)(const char*, const char*, unsigned);
typedef const char*(__cdecl* StrChr_t)(const char*, int);
const Measure_t kMeasureTextWidth = reinterpret_cast<Measure_t>(0x00483930);
const Scan_t kScanMarkupText = reinterpret_cast<Scan_t>(0x00476d40);
const Scan_t kSkipToken = reinterpret_cast<Scan_t>(0x00476d80);
const Scan_t kScanToken = reinterpret_cast<Scan_t>(0x00476dd0);
const StrNCmp_t kStrNCmp = reinterpret_cast<StrNCmp_t>(0x004a6ad0);   // CRT _strncmp
const StrChr_t kStrChr = reinterpret_cast<StrChr_t>(0x004a6170);      // CRT _strchr

// Font of the current style, falling back to the base font +0x5c (0x4773cc..0x4773dc, 0x47748c..0x47749a).
inline void* StyleFont(void* self) {
    void* f = Field<void*>(self, 0x5c + Field<int>(self, 0x38) * 4);
    return f ? f : Field<void*>(self, 0x5c);
}

typedef int(__fastcall* MarkupRun_t)(void*, void*, const char*, int);
MarkupRun_t DrawMarkupRun_orig;
int __fastcall DrawMarkupRun_re(void* self, void*, const char* text, int len) {
    if (Field<void*>(self, 0x5c) == nullptr) Field<void*>(self, 0x5c) = *reinterpret_cast<void**>(0x0083ad44);
    if (Field<int>(self, 0x40) == 0) return kMeasureTextWidth(Field<void*>(self, 0x5c), nullptr, text, len);

    int width = 0;
    const char* start = text;
    for (;;) {
        const char* p;
        if (Field<int>(self, 0x4c) == 1) p = kScanToken(start, &len);          // 0x4772bc
        else if (Field<int>(self, 0x38) == 3) p = kSkipToken(start, &len);     // 0x4772cf
        else p = kScanMarkupText(start, &len);

        if (len == 0) {                                                         // 0x4772f8: last piece
            width += kMeasureTextWidth(StyleFont(self), nullptr, start, static_cast<int>(p - start));
            if (Field<int>(self, 0x4c) == 2) {                                  // 0x4774ab: trailing dropdown arrow
                width += 0x1e;
                Field<int>(self, 0x4c) = 0;
                Field<int>(self, 0x3c) = 0;
                return width;
            }
            Field<int>(self, 0x3c) = 0;
            Field<int>(self, 0x4c) = 0;
            return width;
        }

        int style = Field<int>(self, 0x38);   // 0x4773c6: tokens that do not set a style keep the current one
        switch (static_cast<unsigned char>(*p)) {
        case '$':
            if (kStrNCmp(p, Img(0x004e4254), 6) == 0) {             // 0x477330
                Field<int>(self, 0x3c) = 1;
                style = 3;
            } else if (kStrNCmp(p, Img(0x004e4248), 9) == 0) {      // 0x477352
                Field<int>(self, 0x4c) = 1;
            } else if (kStrNCmp(p, Img(0x004e423c), 9) == 0) {      // 0x47736f
                Field<int>(self, 0x4c) = 1;
                style = 3;
            }
            break;
        case '=':                                                   // 0x47731c
            Field<int>(self, 0x3c) = 2;
            break;
        case '^':                                                   // 0x477389
            Field<int>(self, 0x4c) = 2;
            style = 0;
            break;
        case '{':                                                   // 0x477394
            if (p[1] == '{') { ++p; --len; } else style = 1;
            break;
        case '[':                                                   // 0x4773a9
            if (p[1] == '[') { ++p; --len; } else style = 2;
            break;
        case '}':                                                   // 0x4773a1
            if (p[1] == '}') { ++p; --len; } else style = 0;
            break;
        case ']':                                                   // 0x4773b6
            if (p[1] == ']') { ++p; --len; } else style = 0;
            break;
        default:
            break;
        }

        // 0x4773c9: measure the text before the token in the style that was current
        width += kMeasureTextWidth(StyleFont(self), nullptr, start, static_cast<int>(p - start));
        const int link = Field<int>(self, 0x3c);
        if (link == 1) {                                            // 0x4773f7: skip the rest of the link token
            p += 5;
            len -= 5;
        } else if (Field<int>(self, 0x4c) == 1) {                   // 0x47740f: skip the rest of the dropdown token
            p += 9;
            len -= 9;
        } else if (Field<int>(self, 0x4c) == 2) {                   // 0x477424: dropdown arrow width
            Field<int>(self, 0x4c) = 0;
            width += Field<int>(self, 0x54) != 0 ? 0x19 : 0x1e;
        }
        start = p + 1;
        if (link == 2) {                                            // 0x477447: skip up to and including '>'
            const char* gt = kStrChr(start, 0x3e);
            style = 0;
            if (gt) {
                len += static_cast<int>(start - gt) - 1;
                start = gt + 1;
            }
            Field<int>(self, 0x3c) = 0;
        }
        Field<int>(self, 0x38) = style;                             // 0x477478
        --len;
    }
}

// ---------------------------------------------------------------------------------------------------------------
// 0x0047f340  hitTestTree(w, px, py): deepest window under the point (*px, *py) in w's subtree, or NULL. The point
// is in-out: on a hit it is left in the coordinates of the window found; on a miss it is restored (0x47f6e3).
// Window fields: +0x9c flags, +0xa0 byte flags (bit 2 = framed), +0xa4, +0xb0 owner, +0x11c..+0x128 frame surface
// pointers (top, right, left, bottom), +0x180/+0x184/+0x188 frame band sizes, +0x1ac rect (client), +0x1bc rect
// (outer frame), +0x224 child array, +0x22c child count, +0x274 embedded surface. Pixels come from getPixel
// 0x00478df0 (surface, x, y) and are compared with the transparent colour (0x004e432c & 0x7fffffff).
// The global 0x0083ab18 is set to 1 when the point lies on the frame band (0x47f5cd) and to 0 once it is inside
// the client rect (0x47f621).
// ---------------------------------------------------------------------------------------------------------------
typedef int(__fastcall* Visible_t)(void*, void*);
typedef int(__cdecl* PtInRect_t)(int, int, const int*);
typedef unsigned(__fastcall* GetPixel_t)(void*, void*, int, int);
typedef void*(__cdecl* HitTree_t)(void*, int*, int*);
const Visible_t kWindowVisible = reinterpret_cast<Visible_t>(0x004801f0);
const PtInRect_t kPointInRect = reinterpret_cast<PtInRect_t>(0x00492610);
const GetPixel_t kGetPixel = reinterpret_cast<GetPixel_t>(0x00478df0);
const HitTree_t kHitTestTree = reinterpret_cast<HitTree_t>(0x0047f340);
inline unsigned TransparentColour() { return *reinterpret_cast<const unsigned*>(0x004e432c) & 0x7fffffffu; }
inline int* RectAt(void* w, int off) { return &Field<int>(w, off); }
inline void* ChildAt(void* w, int i) { return Field<void**>(w, 0x224)[i]; }
inline int& FrameFlag() { return *reinterpret_cast<int*>(0x0083ab18); }

HitTree_t HitTestTree_orig;
void* __cdecl HitTestTree_re(void* w, int* px, int* py) {
    if (kWindowVisible(w, nullptr) == 0) return nullptr;                      // 0x47f352
    const int saveX = *px, saveY = *py;
    if ((Field<unsigned char>(w, 0x9c) & 0x20) == 0) {                         // 0x47f371: not a child window:
        *px = *reinterpret_cast<const int*>(0x00839ab8);                       // take the point from the globals
        *py = *reinterpret_cast<const int*>(0x00839abc);
    }
    // Pass 1 (0x47f389): children without bit 0x20, each tested with the same point.
    for (int i = 0; i < Field<int>(w, 0x22c); ++i) {
        void* c = ChildAt(w, i);
        if ((Field<unsigned char>(c, 0x9c) & 0x20) == 0) {
            if (void* r = kHitTestTree(c, px, py)) return r;
        }
    }
    if (Field<unsigned char>(w, 0xa0) & 0x02) {                                // 0x47f3c5: framed window
        if (!kPointInRect(*px, *py, RectAt(w, 0x1bc))) goto miss;              // 0x47f3ea
        *px -= Field<int>(w, 0x1bc);
        *py -= Field<int>(w, 0x1c0);
        // Pass 2 (0x47f408): when w owns itself, children with bits 0x8000 and 0x20 (frame-level children).
        for (int i = 0; i < Field<int>(w, 0x22c); ++i) {
            if (w != Field<void*>(w, 0xb0)) continue;
            void* c = ChildAt(w, i);
            const unsigned f = Field<unsigned>(c, 0x9c);
            if ((f & 0x8000) && (f & 0x20)) {
                if (void* r = kHitTestTree(c, px, py)) return r;
            }
        }
        if (kPointInRect(*px, *py, RectAt(w, 0x1ac))) {                        // 0x47f469: inside the client
            *px -= Field<int>(w, 0x1ac);
            *py -= Field<int>(w, 0x1b0);
        } else {
            // On the frame band (0x47f46f): opaque frame pixels belong to w.
            if (reinterpret_cast<Thiscall0_t>(Vtbl(w)[0x11c / 4])(w, nullptr) == 0) goto band;
            const unsigned f = Field<unsigned>(w, 0x9c);
            if ((f & 0x10000000) == 0) goto band;
            const int top = ((f & 0x10) && !(f & 0x400000)) ? Field<int>(w, 0x180) : Field<int>(w, 0x184);
            const int bottom = Field<int>(w, 0x188) != -1 ? Field<int>(w, 0x188) : Field<int>(w, 0x184);
            const int y = *py;
            unsigned pix;
            if (y < top) {                                                     // 0x47f4cf: top band
                pix = kGetPixel(Field<void*>(w, 0x11c), nullptr, *px, y);
            } else if (y > Field<int>(w, 0x1c8) - Field<int>(w, 0x1c0) - bottom) {   // 0x47f539: bottom band
                pix = kGetPixel(Field<void*>(w, 0x128), nullptr, *px,
                                Field<int>(w, 0x1c0) - Field<int>(w, 0x1c8) + y + bottom);
            } else if (*px < Field<int>(w, 0x184)) {                           // 0x47f5a1: left band
                pix = kGetPixel(Field<void*>(w, 0x124), nullptr, *px, y - top);
            } else {                                                           // right band
                pix = kGetPixel(Field<void*>(w, 0x120), nullptr,
                                Field<int>(w, 0x184) - Field<int>(w, 0x1c4) + Field<int>(w, 0x1bc) + *px, y - top);
            }
            if (pix != TransparentColour()) goto band;
            goto miss;                                                         // 0x47f4f4 / 0x47f564 restore
        }
    } else {                                                                   // 0x47f5f2: plain window
        if (!kPointInRect(*px, *py, RectAt(w, 0x1ac))) goto miss;
        *px -= Field<int>(w, 0x1ac);
        *py -= Field<int>(w, 0x1b0);
    }
    FrameFlag() = 0;                                                           // 0x47f621
    // Pass 3 (0x47f62b): when w owns itself, children with bit 0x20 and without 0x8000.
    for (int i = 0; i < Field<int>(w, 0x22c); ++i) {
        if (w != Field<void*>(w, 0xb0)) continue;
        void* c = ChildAt(w, i);
        const unsigned f = Field<unsigned>(c, 0x9c);
        if (!(f & 0x8000) && (f & 0x20)) {
            if (void* r = kHitTestTree(c, px, py)) return r;
        }
    }
    if (Field<unsigned char>(w, 0x9c) & 0x02) goto miss;                       // 0x47f676
    if (reinterpret_cast<Thiscall0_t>(Vtbl(w)[0x11c / 4])(w, nullptr) == 0) return w;   // 0x47f68b
    {
        const unsigned f = Field<unsigned>(w, 0x9c);
        if ((f & 0x1000000) == 0 || (f & 0x100) != 0) return w;               // 0x47f69c / 0x47f6a5
        // Shaped window: the embedded surface's pixel decides (0x47f6ab..0x47f6dd).
        const unsigned pix = kGetPixel(reinterpret_cast<char*>(w) + 0x274, nullptr, *px, *py);
        if (pix == TransparentColour()) goto miss;
        if (Field<int>(w, 0xa4) == 0 || pix != 0) return w;
        goto miss;
    }
band:
    FrameFlag() = 1;                                                           // 0x47f5cd
    return w;
miss:
    *px = saveX;                                                               // 0x47f6e3
    *py = saveY;
    return nullptr;
}

// ---------------------------------------------------------------------------------------------------------------
// 0x0047bc60  Window::resized(a, b): sets g_curWidget = this (0x47bc65). Unless +0x9c has bit 0x40 (0x47bc73), it
// repositions the two child windows at +0x26c and +0x270 (each optional) against the client rect +0x1ac..+0x1b8
// (width = +0x1b4 - +0x1ac, height = +0x1b8 - +0x1b0), using the sizes their surface objects report (child +0x278,
// slot 0xd8 = width, slot 0xdc = height; 0 when the child has none):
//   +0x26c: moveTo(width, 0) (0x47bc98), then its slot 0xc (W26c, height - H270, 0, 0) when +0x270 exists
//           (0x47bca7..0x47bcfe), else (W26c, height, 0, 0) (0x47bd00..0x47bd3c);
//   +0x270: moveTo(0, height) (0x47bd5e), then its slot 0xc (width - W26c, H270, 0, 0) when +0x26c exists
//           (0x47bd73..0x47bdcd), else (width, H270, 0, 0) (0x47bdcf..0x47be05).
// The surface sizes are read in the order the original reads them (H270 before W26c in both combined cases).
// Then the cdecl callback +0x230 (a, b) when set (0x47be1c), the virtual slot 0x5c (a, b) (0x47be27) and
// Window::layoutScrollbars 0x0047d570 (0x47be2c).
// ---------------------------------------------------------------------------------------------------------------
typedef int(__fastcall* MoveTo_t)(void*, void*, int, int);
typedef void(__fastcall* LayoutScroll_t)(void*, void*);
typedef void(__fastcall* Resized_t)(void*, void*, int, int);
typedef int(__fastcall* Thiscall4_t)(void*, void*, int, int, int, int);
const MoveTo_t kMoveTo = reinterpret_cast<MoveTo_t>(0x0047b420);
const LayoutScroll_t kLayoutScrollbars = reinterpret_cast<LayoutScroll_t>(0x0047d570);

// Size of a child's surface object (+0x278 = +0x274 + 4) through its slot 0xd8 / 0xdc, or 0 without one.
inline int ChildSurfaceSize(void* child, int slot) {
    void* s = Field<void*>(child, 0x278);
    return s ? reinterpret_cast<Thiscall0_t>(Vtbl(s)[slot / 4])(s, nullptr) : 0;
}

Resized_t WindowResized_orig;
void __fastcall WindowResized_re(void* self, void*, int a, int b) {
    CurWidget() = self;
    if ((Field<unsigned char>(self, 0x9c) & 0x40) == 0) {
        void* const h = Field<void*>(self, 0x26c);
        if (h) {
            kMoveTo(h, nullptr, Field<int>(self, 0x1b4) - Field<int>(self, 0x1ac), 0);
            int w26c, height;
            if (Field<void*>(self, 0x270)) {
                const int h270 = ChildSurfaceSize(Field<void*>(self, 0x270), 0xdc);
                w26c = ChildSurfaceSize(Field<void*>(self, 0x26c), 0xd8);
                height = Field<int>(self, 0x1b8) - Field<int>(self, 0x1b0) - h270;
            } else {
                w26c = ChildSurfaceSize(Field<void*>(self, 0x26c), 0xd8);
                height = Field<int>(self, 0x1b8) - Field<int>(self, 0x1b0);
            }
            void* const c = Field<void*>(self, 0x26c);
            reinterpret_cast<Thiscall4_t>(Vtbl(c)[0xc / 4])(c, nullptr, w26c, height, 0, 0);
        }
        void* const v = Field<void*>(self, 0x270);
        if (v) {
            kMoveTo(v, nullptr, 0, Field<int>(self, 0x1b8) - Field<int>(self, 0x1b0));
            int width, h270;
            if (Field<void*>(self, 0x26c)) {
                h270 = ChildSurfaceSize(Field<void*>(self, 0x270), 0xdc);
                const int w26c = ChildSurfaceSize(Field<void*>(self, 0x26c), 0xd8);
                width = Field<int>(self, 0x1b4) - Field<int>(self, 0x1ac) - w26c;
            } else {
                h270 = ChildSurfaceSize(Field<void*>(self, 0x270), 0xdc);
                width = Field<int>(self, 0x1b4) - Field<int>(self, 0x1ac);
            }
            void* const c = Field<void*>(self, 0x270);
            reinterpret_cast<Thiscall4_t>(Vtbl(c)[0xc / 4])(c, nullptr, width, h270, 0, 0);
        }
    }
    if (Cb2_t cb = Field<Cb2_t>(self, 0x230)) cb(a, b);
    reinterpret_cast<Thiscall2_t>(Vtbl(self)[0x5c / 4])(self, nullptr, a, b);
    kLayoutScrollbars(self, nullptr);
}

}  // namespace

SG_HOOK("golf_clean.exe", 0x0047bc60, Window_resized, WindowResized_re, WindowResized_orig);
SG_HOOK("golf_clean.exe", 0x0047f340, hitTestTree, HitTestTree_re, HitTestTree_orig);
SG_HOOK("golf_clean.exe", 0x0047c970, Window_dispatchCommand, DispatchCommand_re, DispatchCommand_orig);
SG_HOOK("golf_clean.exe", 0x0047c5d0, Window_key, WindowKey_re, WindowKey_orig);
SG_HOOK("golf_clean.exe", 0x0047c430, Window_mouseDispatch254, MouseDispatch254_re, MouseDispatch254_orig);
SG_HOOK("golf_clean.exe", 0x0047d840, setDragTarget, SetDragTarget_re, SetDragTarget_orig);
SG_HOOK("golf_clean.exe", 0x0040a160, buildToolLabel, BuildToolLabel_re, BuildToolLabel_orig);
SG_HOOK("golf_clean.exe", 0x00477280, drawMarkupRun, DrawMarkupRun_re, DrawMarkupRun_orig);

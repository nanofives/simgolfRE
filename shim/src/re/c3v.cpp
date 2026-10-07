// C3 batch c3v (2026-10-07) of golf_clean.exe: UI functions the test scenarios never reach but that have static
// callers: three ListModel list walkers (findId / indexOfId / idAt), the scrollbar state reset, the Window grow/shrink
// rect transforms, Widget::setValue, EditBox::setText and nearestCharControl. Hand-written from the disassembly
// (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr> --list) and the C2 transcriptions; each body cites the
// address of every global, offset and callee it uses. __thiscall is emulated with __fastcall (ecx = this, edx unused),
// and so are the virtual methods reached through an object's vtable (thiscall, callee pops its stack arguments).
#include <string.h>

#include "hooks.h"

namespace {

template <typename T> T at(unsigned addr) { return *reinterpret_cast<const T*>(addr); }
template <typename T> T field(const void* p, unsigned off) {
    return *reinterpret_cast<const T*>(reinterpret_cast<const char*>(p) + off);
}
template <typename T> void setField(void* p, unsigned off, T v) {
    *reinterpret_cast<T*>(reinterpret_cast<char*>(p) + off) = v;
}
inline void* vslot(const void* obj, unsigned off) {
    return at<void*>(*reinterpret_cast<const unsigned*>(obj) + off);
}

// ListModel (base of several UI lists) at this+0xc8: head pointer +0xc8, current node +0xcc, count +0xd0, current
// index +0xd4 (indexOfId also mirrors the index into +0xf0). Node (0x14 bytes): data/id at +4, next at +0xc, prev at
// +0x10. Offsets from re/analysis/ui/004899d0_ListModel_findId.md and the matched source re/match/golf_hand_04.cpp
// (Node489 / List489) and golf_hand_02.cpp (C4899d0::find).

// 0x004899d0  ListModel::findId(key): walks at most count nodes from the head along +0xc, writing the walked index
// into +0xd4 and the stopping node into +0xcc; stops at the first node whose +4 equals key (0x004899ef) or after
// count steps (0x00489a01). Returns the index (+0xd4), which equals count when key is absent.
typedef int(__fastcall* ListKey_t)(void*, void*, int);
ListKey_t FindId_orig;
int __fastcall FindId_re(void* self, void*, int key) {
    int i = 0;
    if (field<void*>(self, 0xc8)) {
        setField<int>(self, 0xd4, 0);
        setField<void*>(self, 0xcc, field<void*>(self, 0xc8));
        const int count = field<int>(self, 0xd0);
        if (count > 0) {
            do {
                void* cur = field<void*>(self, 0xcc);
                if (field<int>(cur, 4) == key) break;
                i++;
                setField<int>(self, 0xd4, field<int>(self, 0xd4) + 1);
                setField<void*>(self, 0xcc, field<void*>(cur, 0xc));
            } while (i < count);
        }
    }
    return field<int>(self, 0xd4);
}

// 0x004898d0  ListModel::indexOfId(key): the void twin of findId that also copies the resulting index +0xd4 into
// +0xf0 on both exit paths (store at 0x00489933 reached from the found break and from the loop end). Returns nothing.
typedef void(__fastcall* ListKeyV_t)(void*, void*, int);
ListKeyV_t IndexOfId_orig;
void __fastcall IndexOfId_re(void* self, void*, int key) {
    int i = 0;
    if (field<void*>(self, 0xc8)) {
        setField<int>(self, 0xd4, 0);
        setField<void*>(self, 0xcc, field<void*>(self, 0xc8));
        const int count = field<int>(self, 0xd0);
        if (count > 0) {
            do {
                void* cur = field<void*>(self, 0xcc);
                if (field<int>(cur, 4) == key) break;
                i++;
                setField<int>(self, 0xd4, field<int>(self, 0xd4) + 1);
                setField<void*>(self, 0xcc, field<void*>(cur, 0xc));
            } while (i < count);
        }
    }
    setField<int>(self, 0xf0, field<int>(self, 0xd4));
}

// 0x00489a30  ListModel::idAt(index): seeks the current node +0xcc to the index-th node when index <= count-1
// (0x00489a3c). For index >= 0 it steps forward along +0xc index times (0x00489a84..0x00489a90). For index < 0 it
// takes abs(index) = (index ^ (index>>31)) - (index>>31) (0x00489a4c), and only when abs(index) <= count steps
// backward along +0x10 that many times from the head (0x00489a6c..0x00489a78) and sets index = count + index; when
// abs(index) > count it leaves the current node at the head. It writes the final index to +0xd4. Returns the stopping
// node's +4 when the head +0xc8 is non-null (0x00489a9d), else 0.
typedef int(__fastcall* ListIdx_t)(void*, void*, int);
ListIdx_t IdAt_orig;
int __fastcall IdAt_re(void* self, void*, int index) {
    const int count = field<int>(self, 0xd0);
    if (index <= count - 1) {
        void* cur = field<void*>(self, 0xc8);
        setField<void*>(self, 0xcc, cur);
        bool skip = false;
        if (index < 0) {
            const int s = index >> 31;
            int n = (index ^ s) - s;  // abs(index)
            if (count < n) {
                skip = true;
            } else {
                while (n > 0) {
                    n--;
                    cur = field<void*>(cur, 0x10);
                    setField<void*>(self, 0xcc, cur);
                }
                index = count + index;
            }
        } else {
            int u = index;
            while (u > 0) {
                u--;
                cur = field<void*>(cur, 0xc);
                setField<void*>(self, 0xcc, cur);
            }
        }
        if (!skip) setField<int>(self, 0xd4, index);
    }
    if (field<void*>(self, 0xc8) != 0) return field<int>(field<void*>(self, 0xcc), 4);
    return 0;
}

// 0x004979a0  scrollbarResetState(this): clears the scroll offset +0x5ac and step +0x5b0 to 0 and sets the two range
// fields +0x5b4 / +0x5b8 to the max +0x5c0 (0x004979a0..0x004979bd). When the anchor +0x57c is not -1 (0x004979c4) it
// marks active (+0x5ac = 1), biases the range (+0x5b4--, +0x5b8--) and bumps the step (+0x5b0++). Returns nothing.
typedef void(__fastcall* This0_t)(void*, void*);
This0_t ScrollbarResetState_orig;
void __fastcall ScrollbarResetState_re(void* self, void*) {
    setField<int>(self, 0x5ac, 0);
    setField<int>(self, 0x5b0, 0);
    const int maxv = field<int>(self, 0x5c0);
    setField<int>(self, 0x5b4, maxv);
    setField<int>(self, 0x5b8, maxv);
    if (field<int>(self, 0x57c) != -1) {
        setField<int>(self, 0x5ac, 1);
        setField<int>(self, 0x5b4, field<int>(self, 0x5b4) - 1);
        setField<int>(self, 0x5b0, field<int>(self, 0x5b0) + 1);
        setField<int>(self, 0x5b8, field<int>(self, 0x5b8) - 1);
    }
}

// Window frame flags are at this+0x9c; the border inset is the dword at 0x0083ff10; the inner margin is +0x184, the
// extra bottom margin +0x188 (-1 = none), the title offsets +0x180/+0x184. The optional caption frame is at +0x15c;
// its height() is the virtual at vtable +0x170 (thiscall, no stack args). RECT is {left=+0, top=+4, right=+8,
// bottom=+0xc}. Offsets from re/match/golf_hand_06_b.cpp (Win47b::grow) and the two C2 notes.
typedef int(__fastcall* Height_t)(void*, void*);

// 0x0047cc10  Window::growRect(r): enlarges the rectangle r. Bit 4 adds the border inset to bottom (0x0047cc1f), bit 8
// to right (0x0047cc2f). Bits 0x400 or 0x11 inset all four sides by +0x184 (0x0047cc48..0x0047cc6e) and, when +0x188
// is not -1, set bottom to (+0x188 - +0x184) + oldBottom + d (0x0047cc74). Bit 0x10 shifts top by +0x184 - +0x180
// (0x0047cc8e). Finally, when the caption frame +0x15c is present and bit 0x20000000 is clear, top is reduced by its
// height() (0x0047ccae). Returns nothing.
typedef void(__fastcall* Rect_t)(void*, void*, int*);
Rect_t GrowRect_orig;
void __fastcall GrowRect_re(void* self, void*, int* r) {
    if (!r) return;
    const unsigned f = field<unsigned>(self, 0x9c);
    const int inset = at<int>(0x0083ff10);
    if (f & 4) r[3] += inset;
    if (f & 8) r[2] += inset;
    if ((f & 0x400) || (f & 0x11)) {
        const int d = field<int>(self, 0x184);
        r[0] -= d;
        r[2] += d;
        r[1] -= d;
        const int oldBottom = r[3];
        r[3] = oldBottom + d;
        if (field<int>(self, 0x188) != -1)
            r[3] = (field<int>(self, 0x188) - d) + oldBottom + d;
    }
    if (f & 0x10) r[1] += field<int>(self, 0x184) - field<int>(self, 0x180);
    void* frame = field<void*>(self, 0x15c);
    if (frame && !(f & 0x20000000))
        r[1] -= reinterpret_cast<Height_t>(vslot(frame, 0x170))(frame, 0);
}

// 0x0047cce0  Window::shrinkRect(r): the inverse of growRect (subtracts where grow adds). Bit 4 subtracts the border
// inset from bottom (0x0047ccef), bit 8 from right (0x0047ccff). Bits 0x400 or 0x11 shrink all four sides by +0x184
// and, when +0x188 is not -1, set bottom to (+0x184 - +0x188) + (oldBottom - d) (0x0047cd40). Bit 0x10 shifts top by
// +0x180 - +0x184 (0x0047cd5e). When the caption frame +0x15c is present and bit 0x20000000 is clear, top is increased
// by its height() (0x0047cd7e). Returns nothing.
Rect_t ShrinkRect_orig;
void __fastcall ShrinkRect_re(void* self, void*, int* r) {
    if (!r) return;
    const unsigned f = field<unsigned>(self, 0x9c);
    const int inset = at<int>(0x0083ff10);
    if (f & 4) r[3] -= inset;
    if (f & 8) r[2] -= inset;
    if ((f & 0x400) || (f & 0x11)) {
        const int d = field<int>(self, 0x184);
        r[0] += d;
        r[2] -= d;
        const int belowBottom = r[3] - d;
        r[1] += d;
        r[3] = belowBottom;
        if (field<int>(self, 0x188) != -1)
            r[3] = (d - field<int>(self, 0x188)) + belowBottom;
    }
    if (f & 0x10) r[1] += field<int>(self, 0x180) - field<int>(self, 0x184);
    void* frame = field<void*>(self, 0x15c);
    if (frame && !(f & 0x20000000))
        r[1] += reinterpret_cast<Height_t>(vslot(frame, 0x170))(frame, 0);
}

// 0x0047d020  Widget::setValue(value): only acts when bit 2 of this+0xa0 is set (0x0047d020). It stores value at
// +0x184 (0x0047d034) and invalidates its drawn area through the virtual at vtable +0xc (0x0047d058, thiscall) called
// with (width, height, 0, 0) where width = +0x1c4 - +0x1bc and height = +0x1c8 - +0x1c0. Returns nothing.
typedef void(__fastcall* Inval_t)(void*, void*, int, int, int, int);
typedef void(__fastcall* SetValue_t)(void*, void*, int);
SetValue_t SetValue_orig;
void __fastcall SetValue_re(void* self, void*, int value) {
    if (!(field<unsigned char>(self, 0xa0) & 2)) return;
    setField<int>(self, 0x184, value);
    const int w = field<int>(self, 0x1c4) - field<int>(self, 0x1bc);
    const int h = field<int>(self, 0x1c8) - field<int>(self, 0x1c0);
    reinterpret_cast<Inval_t>(vslot(self, 0xc))(self, 0, w, h, 0, 0);
}

// 0x00486200  EditBox::setText(text): when the buffer +0x574 is non-null (0x0048620a), copies text into it: a null
// text clears it to "" (0x00486216), otherwise strncpy of at most the capacity +0x578 bytes (0x00486229). It then
// writes the buffer's strlen into +0x5a4 (repne scasb length at 0x00486234..0x00486251) and notifies through the
// virtual at vtable +0x120 (0x0048625c, thiscall, no stack args). Returns nothing.
typedef void(__fastcall* Notify_t)(void*, void*);
typedef void(__fastcall* SetText_t)(void*, void*, const char*);
SetText_t SetText_orig;
void __fastcall SetText_re(void* self, void*, const char* text) {
    char* buf = field<char*>(self, 0x574);
    if (!buf) return;
    if (!text)
        buf[0] = '\0';
    else
        strncpy(buf, text, field<unsigned>(self, 0x578));
    unsigned len = 0;
    while (buf[len] != '\0') len++;
    setField<unsigned>(self, 0x5a4, len);
    reinterpret_cast<Notify_t>(vslot(self, 0x120))(self, 0);
}

// 0x00438260  nearestCharControl(x, y): finds the index of the closest control spot to (x, y) in the 16-bit table at
// 0x004c7b38 (short pairs {x, y}). Starting with best = 0x28 and idx = -1 it measures each entry with approxDistance
// 0x00467170: for the first 8 entries (address < 0x4c7b58, 0x00438276) the x delta is divided by 3 (0x0043828c),
// otherwise it is used directly (0x004382a4); the y delta is y - entry.y. A distance below the running best
// (0x004382bf) updates best and idx. The loop continues while the next entry's x is non-zero (0x004382d2). Returns the
// best index, or -1 when none is within 0x28.
typedef int(__cdecl* Dist_t)(int, int);
const Dist_t kApproxDistance = reinterpret_cast<Dist_t>(0x00467170);
typedef int(__cdecl* Near_t)(int, int);
Near_t NearestCharControl_orig;
int __cdecl NearestCharControl_re(int x, int y) {
    const short* tbl = reinterpret_cast<const short*>(0x004c7b38);
    int best = 0x28, idx = -1, i = 0;
    do {
        const int dx = x - tbl[i * 2];
        const int dy = y - tbl[i * 2 + 1];
        const int d = (i < 8) ? kApproxDistance(dx / 3, dy) : kApproxDistance(dx, dy);
        if (d < best) {
            best = d;
            idx = i;
        }
        i++;
    } while (tbl[i * 2] != 0);
    return idx;
}

}  // namespace

// 0x004899d0  ListModel::findId
SG_HOOK("golf_clean.exe", 0x004899d0, ListModel_findId, FindId_re, FindId_orig);
// 0x004898d0  ListModel::indexOfId
SG_HOOK("golf_clean.exe", 0x004898d0, ListModel_indexOfId, IndexOfId_re, IndexOfId_orig);
// 0x00489a30  ListModel::idAt
SG_HOOK("golf_clean.exe", 0x00489a30, ListModel_idAt, IdAt_re, IdAt_orig);
// 0x004979a0  scrollbarResetState
SG_HOOK("golf_clean.exe", 0x004979a0, scrollbarResetState, ScrollbarResetState_re, ScrollbarResetState_orig);
// 0x0047cc10  Window::growRect
SG_HOOK("golf_clean.exe", 0x0047cc10, Window_growRect, GrowRect_re, GrowRect_orig);
// 0x0047cce0  Window::shrinkRect
SG_HOOK("golf_clean.exe", 0x0047cce0, Window_shrinkRect, ShrinkRect_re, ShrinkRect_orig);
// 0x0047d020  Widget::setValue
SG_HOOK("golf_clean.exe", 0x0047d020, Widget_setValue, SetValue_re, SetValue_orig);
// 0x00486200  EditBox::setText
SG_HOOK("golf_clean.exe", 0x00486200, EditBox_setText, SetText_re, SetText_orig);
// 0x00438260  nearestCharControl
SG_HOOK("golf_clean.exe", 0x00438260, nearestCharControl, NearestCharControl_re, NearestCharControl_orig);

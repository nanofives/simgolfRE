// C3 batch c3u (2026-10-07) of golf_clean.exe: UI widget helpers the test scenarios never reach but that have static
// callers (hooks.csv). List seek/find helpers (listFindById, ListModel::selectedId), a character-editor hit-test
// (charEditorZone), a combo-box index reader (comboCurrentIndex), a clamped scalar store (setWidgetScalar), a HUD
// text-slot writer (setHudTextSlot), a button mode setter (Button::setMode) and a hover-colour setter
// (Button::setColorHover). Hand-written from the
// disassembly (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr> --list) and the C2 transcriptions
// (re/analysis/ui/<addr>_*.md); each body cites the address of every offset, global and callee it uses. __thiscall is
// emulated with __fastcall (ecx = this, edx unused), and so are the virtual methods reached through an object's vtable.
#include "hooks.h"
#include <string.h>

namespace {

template <typename T> T at(unsigned addr) { return *reinterpret_cast<const T*>(addr); }
template <typename T> T field(const void* p, unsigned off) {
    return *reinterpret_cast<const T*>(reinterpret_cast<const char*>(p) + off);
}
template <typename T> void setField(void* p, unsigned off, T v) {
    *reinterpret_cast<T*>(reinterpret_cast<char*>(p) + off) = v;
}
// Value of a virtual-table slot of an object (the vtable pointer is the object's first dword).
inline void* vslot(void* obj, unsigned off) {
    return at<void*>(*reinterpret_cast<const unsigned*>(obj) + off);
}

// List489 node reached by the seek helpers: +0x4 is the entry id compared against the key, +0xc the next node,
// +0x10 the previous node (re/match/golf_hand_04.cpp struct Node489).
inline void* nodeNext(void* n) { return field<void*>(n, 0xc); }
inline void* nodePrev(void* n) { return field<void*>(n, 0x10); }
inline int nodeId(void* n) { return field<int>(n, 4); }

// ---------------------------------------------------------------------------------------------------------------------

// 0x004a4890  listFindById(this, key): searches the embedded list (head this+0x80, cursor this+0x84, count this+0x88,
// index this+0x8c; re/match/golf_hand_02.cpp ?find@C4a4890@@QAEHH@Z) for the node whose id (+0x4) equals key. When the
// head (this+0x80) is 0 it returns the index field unchanged (0x004a48df); otherwise it resets index to 0 and cursor to
// head (0x004a48a5/0x004a48ad), then walks up to count nodes (jle at 0x004a48b3, jl at 0x004a48dd), breaking at the
// first match (je at 0x004a489d) and advancing cursor = cursor->next (0x004a48d2) and index++ (0x004a48cc) otherwise.
// Returns the index: the match position, or count when not found.
typedef int(__fastcall* FindById_t)(void*, void*, int);
FindById_t ListFindById_orig;
int __fastcall ListFindById_re(void* self, void*, int key) {
    void* head = field<void*>(self, 0x80);
    if (head) {
        setField<int>(self, 0x8c, 0);
        setField<void*>(self, 0x84, head);
        const int count = field<int>(self, 0x88);
        int i = 0;
        for (; i < count; i++) {
            void* cur = field<void*>(self, 0x84);
            if (nodeId(cur) == key) break;
            setField<int>(self, 0x8c, field<int>(self, 0x8c) + 1);
            setField<void*>(self, 0x84, nodeNext(cur));
        }
    }
    return field<int>(self, 0x8c);
}

// 0x00489950  ListModel::selectedId(this): returns the id of the node at the selection index this+0xf0 of the embedded
// list (head this+0xc8, cursor this+0xcc, count this+0xd0, index this+0xd4; re/match/golf_hand_04.cpp
// ?getSel@C489950@@QAEPAXXZ delegating to List489::get). seek: when sel > count-1 (jg at 0x00489963) it leaves the
// cursor unchanged; else cursor = head (0x0048996d). For sel < 0 (jl via `test`/`jns` at 0x00489973) it takes n =
// abs(sel) (0x00489968..0x00489971), returns early when n > count (jg at 0x00489973) and otherwise walks cursor =
// cursor->prev n times (0x0048998b) then normalises sel += count (0x00489996); for sel >= 0 it walks cursor =
// cursor->next sel times (0x004899a5) and stores the (normalised) sel into index this+0xd4 (0x004899b0). Then it
// returns 0 when head (this+0xc8) is 0 (je at 0x004899c0) and the cursor node's id (+0x4) otherwise (0x004899c8).
typedef unsigned(__fastcall* SelectedId_t)(void*, void*);
SelectedId_t SelectedId_orig;
unsigned __fastcall SelectedId_re(void* self, void*) {
    int sel = field<int>(self, 0xf0);
    const int count = field<int>(self, 0xd0);
    if (sel <= count - 1) {
        void* head = field<void*>(self, 0xc8);
        setField<void*>(self, 0xcc, head);
        bool updateIndex = true;
        if (sel < 0) {
            int n = sel < 0 ? -sel : sel;  // abs via cdq/xor/sub at 0x00489968
            if (count < n) {
                updateIndex = false;       // jg at 0x00489973 -> fall through to the head test
            } else {
                while (n > 0) { setField<void*>(self, 0xcc, nodePrev(field<void*>(self, 0xcc))); n--; }
                sel = sel + count;
            }
        } else {
            int n = sel;
            while (n > 0) { setField<void*>(self, 0xcc, nodeNext(field<void*>(self, 0xcc))); n--; }
        }
        if (updateIndex) setField<int>(self, 0xd4, sel);
    }
    if (field<void*>(self, 0xc8) == 0) return 0;
    return field<unsigned>(field<void*>(self, 0xcc), 4);
}

// 0x004942a0  comboCurrentIndex(this): returns the id of the combo's current item, taking one of two embedded lists by
// bit 2 of the byte at this+0x4 (test/je at 0x004942a4). When the bit is clear it reads the virtual-base offset o =
// *(*(this+0x2d98)+8) (0x004942c8/0x004942ce), places an S494 record at this+o+0x2e58 and, when its count (+0x8) is
// non-zero (je at 0x004942dd), returns record->item (+0xc) ->id (+0x4) (0x004942df..0x004942e2). When the bit is set it
// reads o = *(*(this+0x1488)+8) (0x004942a6..0x004942ac), places the S494 at this+o+0x1548 and, when its count
// (this+o+0x1550) is non-zero (je at 0x004942bf), returns record->item (this+o+0x1554) ->id (+0x4). Returns 0 otherwise.
// (re/match/golf_hand_02.cpp ?current@C4942a0@@QAEHXZ; S494 = {m_0, m_4, m_8, P494* m_c}.)
typedef int(__fastcall* ComboCurrent_t)(void*, void*);
ComboCurrent_t ComboCurrentIndex_orig;
int __fastcall ComboCurrentIndex_re(void* self, void*) {
    char* t = reinterpret_cast<char*>(self);
    if (field<unsigned char>(self, 4) & 4) {
        const unsigned o = field<unsigned>(field<void*>(self, 0x1488), 8);
        if (field<int>(t + o, 0x1550) != 0) return nodeId(field<void*>(t + o, 0x1554));
    } else {
        const unsigned o = field<unsigned>(field<void*>(self, 0x2d98), 8);
        char* s = t + o + 0x2e58;
        if (field<int>(s, 8) != 0) return nodeId(field<void*>(s, 0xc));
    }
    return 0;
}

// 0x004382f0  charEditorZone(x, y): returns the character-editor hit zone under the point (x, y). It walks the 10
// (x, y) centres in the table at 0x004c7be0 (end 0x004c7c30, stride 8; esi loop 0x004382fe..0x0043832f) and returns the
// index of the first whose Chebyshev-ish metric approxDistance(0x00467170) of (x - cx - 0x3c, y - cy - 0x3c) is below
// 0x3c (jl at 0x00438323). When none matches it tests two fixed points: approxDistance(x - 0x30a, y - 0x16b) < 0x3c
// returns 10 (jge at 0x0043834a / 0x0043834f), else approxDistance(x - 0x12, y - 0x1ef) < 0x3c returns 0xb and -1
// otherwise (setge/dec/and 0xc/dec at 0x00438375..0x0043837e). Reads nothing writable. (re/match/golf_hand_04.cpp
// ?zone4382f0@@YAHHH@Z; approxDistance is C3.)
typedef int(__cdecl* Dist_t)(int, int);
const Dist_t kApproxDistance = reinterpret_cast<Dist_t>(0x00467170);
typedef int(__cdecl* Zone_t)(int, int);
Zone_t CharEditorZone_orig;
int __cdecl CharEditorZone_re(int x, int y) {
    const int* tbl = reinterpret_cast<const int*>(0x004c7be0);
    for (int i = 0; i < 10; i++)
        if (kApproxDistance(x - tbl[i * 2] - 0x3c, y - tbl[i * 2 + 1] - 0x3c) < 0x3c) return i;
    if (kApproxDistance(x - 0x30a, y - 0x16b) < 0x3c) return 10;
    return kApproxDistance(x - 0x12, y - 0x1ef) < 0x3c ? 0xb : -1;
}

// 0x004967f0  setWidgetScalar(this, value): clamps value into [this+0x580, this+0x584] and stores it at this+0x58c,
// optionally mirrored. It first copies the drawing object this+0x130 into the global dword 0x0083ab2c (0x004967fb). When
// value < min this+0x580 (jge at 0x00496808) it stores min; else when max this+0x584 < value (jle at 0x0049681a) it
// stores max; else value (0x00496824). When the mirror flag this+0x588 is non-zero (je at 0x00496832) it replaces the
// stored value with (max - stored) + min (0x0049683a..0x00496848). Then it invokes the virtual method at vtable +0x120
// (0x0049684c) with no argument. Returns nothing.
typedef void(__fastcall* SetScalar_t)(void*, void*, int);
typedef void(__fastcall* Virt0_t)(void*, void*);
SetScalar_t SetWidgetScalar_orig;
void __fastcall SetWidgetScalar_re(void* self, void*, int value) {
    *reinterpret_cast<unsigned*>(0x0083ab2c) = field<unsigned>(self, 0x130);
    const int lo = field<int>(self, 0x580);
    const int hi = field<int>(self, 0x584);
    int v;
    if (value < lo) v = lo;
    else if (hi < value) v = hi;
    else v = value;
    if (field<int>(self, 0x588) != 0) v = (hi - v) + lo;
    setField<int>(self, 0x58c, v);
    reinterpret_cast<Virt0_t>(vslot(self, 0x120))(self, 0);
}

// 0x00494cb0  setHudTextSlot(idx, text, x, y): records a HUD notification line in slot idx (0..9). It returns 3 when
// text is null or idx > 9 (je at 0x00494cb7, jg at 0x00494cc0; signed). The x colour is stored at 0x0083e8b8 + idx*4,
// defaulting to the dword at 0x0083f35c when x < 0 (jge at 0x00494cc8); the y colour at 0x0083d3c8 + idx*4, defaulting
// to 0x0083f360 when y < 0 (jge at 0x00494cdd). The slot's 0x100-byte text buffer at 0x0083e8e0 + idx*0x100 is then
// cleared (0x00494cfc) and the source string copied into it (strlen + rep movs, 0x00494cf4..0x00494d1f). Returns 0.
// (re/match/golf_hand_03.cpp ?f494cb0@@YAHHPBDHH@Z.)
typedef int(__cdecl* HudSlot_t)(int, const char*, int, int);
HudSlot_t SetHudTextSlot_orig;
int __cdecl SetHudTextSlot_re(int idx, const char* text, int x, int y) {
    if (text == 0 || idx > 9) return 3;
    if (x < 0) x = at<int>(0x0083f35c);
    reinterpret_cast<int*>(0x0083e8b8)[idx] = x;
    if (y < 0) y = at<int>(0x0083f360);
    reinterpret_cast<int*>(0x0083d3c8)[idx] = y;
    char* buf = reinterpret_cast<char*>(0x0083e8e0) + idx * 0x100;
    buf[0] = 0;
    strcat(buf, text);
    return 0;
}

// 0x004890e0  Button::setMode(this, mode): stores a new mode at this+0x578 and notifies, but only when mode differs
// from the current value (je at 0x004890ee). It writes mode (0x004890f2), invokes the own virtual at vtable +0x120 with
// no argument (0x004890f8, an invalidate), and when the parent control this+0x130 is non-null (je at 0x00489106) invokes
// that object's virtual at its vtable +0xd8 (0x00489112) with (this+0x5e8, mode). Returns nothing.
// (re/match/golf_hand_01.cpp ?setMode@C4890e0@@QAEXH@Z.)
typedef void(__fastcall* SetMode_t)(void*, void*, int);
typedef void(__fastcall* Notify2_t)(void*, void*, unsigned, int);
SetMode_t SetMode_orig;
void __fastcall SetMode_re(void* self, void*, int mode) {
    if (mode == field<int>(self, 0x578)) return;
    setField<int>(self, 0x578, mode);
    reinterpret_cast<Virt0_t>(vslot(self, 0x120))(self, 0);
    void* parent = field<void*>(self, 0x130);
    if (parent) reinterpret_cast<Notify2_t>(vslot(parent, 0xd8))(parent, 0, field<unsigned>(self, 0x5e8), mode);
}

// 0x00488970  Button::setColorHover(this, c0, c1, c2, c3): when the drawing object this+0x130 is non-null (je at
// 0x00488978) it forwards to two helpers on the font object this+0x274: Widget_applyPalette 0x004789f0 with the global
// dword 0x0083ad10 (0x00488989), then Widget_setQuad70 0x00476340 with the four colours (0x004889a4). The helpers run
// their originals; the quad is stored at this+0x274+0x70/0x80/0x90/0xa0. Returns nothing. (re/match/golf_raw_04.cpp.)
typedef int(__fastcall* ApplyPalette_t)(void*, void*, int);
typedef void(__fastcall* SetQuad_t)(void*, void*, unsigned, unsigned, unsigned, unsigned);
const ApplyPalette_t kApplyPalette = reinterpret_cast<ApplyPalette_t>(0x004789f0);
const SetQuad_t kSetQuad = reinterpret_cast<SetQuad_t>(0x00476340);
typedef void(__fastcall* SetColorHover_t)(void*, void*, unsigned, unsigned, unsigned, unsigned);
SetColorHover_t SetColorHover_orig;
void __fastcall SetColorHover_re(void* self, void*, unsigned c0, unsigned c1, unsigned c2, unsigned c3) {
    if (field<void*>(self, 0x130) == 0) return;
    void* font = reinterpret_cast<char*>(self) + 0x274;
    kApplyPalette(font, 0, at<int>(0x0083ad10));
    kSetQuad(font, 0, c0, c1, c2, c3);
}

}  // namespace

// 0x004a4890  listFindById
SG_HOOK("golf_clean.exe", 0x004a4890, listFindById, ListFindById_re, ListFindById_orig);
// 0x00489950  ListModel::selectedId
SG_HOOK("golf_clean.exe", 0x00489950, ListModel_selectedId, SelectedId_re, SelectedId_orig);
// 0x004382f0  charEditorZone
SG_HOOK("golf_clean.exe", 0x004382f0, charEditorZone, CharEditorZone_re, CharEditorZone_orig);
// 0x004942a0  comboCurrentIndex
SG_HOOK("golf_clean.exe", 0x004942a0, comboCurrentIndex, ComboCurrentIndex_re, ComboCurrentIndex_orig);
// 0x004967f0  setWidgetScalar
SG_HOOK("golf_clean.exe", 0x004967f0, setWidgetScalar, SetWidgetScalar_re, SetWidgetScalar_orig);
// 0x00494cb0  setHudTextSlot
SG_HOOK("golf_clean.exe", 0x00494cb0, setHudTextSlot, SetHudTextSlot_re, SetHudTextSlot_orig);
// 0x004890e0  Button::setMode
SG_HOOK("golf_clean.exe", 0x004890e0, Button_setMode, SetMode_re, SetMode_orig);
// 0x00488970  Button::setColorHover
SG_HOOK("golf_clean.exe", 0x00488970, Button_setColorHover, SetColorHover_re, SetColorHover_orig);

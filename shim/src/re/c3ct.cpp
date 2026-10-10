// C3 batch c3ct (round 23, 2026-10-09) of golf_clean.exe: UI rows, the leads round 22's c3cr notes left.
//
//   EditBox caret family:  pixelToCharPos 0x004863e0, wrapToLines 0x004862b0, setCaretActive 0x00486b30,
//                          beginCaretDrag 0x00486b90, endCaretDrag 0x00486be0, updateCaretDrag 0x00486c40
//   scrollbarOnDrag 0x00496d20
//   ListBox:               setSelection 0x00489f50, onNavKey 0x0048aa00, onDrag 0x0048c8a0 and the vtordisp thunks
//                          onDrag_vtordisp 0x0048cc80, onNavKey_vtordisp 0x0048ccb0 (table 0x004bb260)
//
// Every body is hand-written from the disassembly (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr> --list)
// and cites the address of each instruction it reproduces. __thiscall is emulated with __fastcall (ecx = this,
// edx unused). Callees run through their original addresses:
//   pixelToCharPos -> wrapToLines 0x004862b0 (this batch), Widget_value 0x00477580 (C3, c3c),
//                     drawMarkupRun 0x00477280 (C3, c3o)
//   wrapToLines    -> wrapText 0x00476e20 (C2)
//   setCaretActive -> Timer::startOnce 0x00486dc0 / Timer::stop 0x00486ec0 (C2)
//   the three caret-drag handlers -> pixelToCharPos 0x004863e0 (this batch)
//   setSelection   -> Window::fwdScroll26c 0x0047b9f0 / fwdScroll270 0x0047ba10 (C3, c3bx),
//                     ListModel::selectedId 0x00489950 (C3, c3u), ListModel::idAt 0x00489a30 (C3, c3v)
//   onNavKey       -> GetKeyState through the import slot 0x004ba1f0, setSelection 0x00489f50 (this batch)
//   onDrag         -> setSelection 0x00489f50 (this batch), Window::invalidate 0x00480ce0 (C2)
//   each thunk     -> its body (this batch)
// Every other call is a virtual slot of the object passed in.
//
// The ListBox layout read off the instructions (no matched source). Three `this` values name one object:
//   S  (setSelection's this)   S+0 a vtable whose slot 0 takes one argument (0x0048ca2c / 0x0048ca73 / 0x0048caa0
//                              call it on this-0x5c), S+4 the vbtable pointer (0x00489f5a)
//   T = S + 0x5c (onNavKey's and onDrag's this): the vbtable pointer is at T-0x58 (0x0048aa06 / 0x0048c8a7)
//   vbtable entries [1] and [2] are the offsets, from S+4, of the Window base W and the list-model base M.
// The code below keeps the original's address arithmetic (this + vbt[k] + disp) instead of naming W and M.
#include "hooks.h"

namespace {

static inline char* B(void* p) { return reinterpret_cast<char*>(p); }
static inline int& I(void* base, int off) { return *reinterpret_cast<int*>(B(base) + off); }
static inline void*& Pp(void* base, int off) { return *reinterpret_cast<void**>(B(base) + off); }
// `mov eax, [obj]; call [eax+off]`: slot off of the object's own vtable.
template <typename T> static inline T vslot(void* obj, int off) {
    return *reinterpret_cast<T*>(*reinterpret_cast<char**>(obj) + off);
}
// Signed result of a 32-bit add / sub as the flags see it (no C++ overflow).
static inline int Add32(int a, int b) { return static_cast<int>(static_cast<unsigned>(a) + static_cast<unsigned>(b)); }
static inline int Sub32(int a, int b) { return static_cast<int>(static_cast<unsigned>(a) - static_cast<unsigned>(b)); }
static inline int Mul32(int a, int b) { return static_cast<int>(static_cast<unsigned>(a) * static_cast<unsigned>(b)); }

typedef int(__fastcall* Vfn0_t)(void* ecx, void* edx);
typedef int(__fastcall* Vfn1_t)(void* ecx, void* edx, int a);
typedef int(__fastcall* Vfn2_t)(void* ecx, void* edx, int a, int b);

void*& g_dragOwner = *reinterpret_cast<void**>(0x0083ab30);    // drag capture, read by hitTestScreen 0x0047f707
void*& g_dragOwner2 = *reinterpret_cast<void**>(0x0083ab34);   // second capture slot, 0x0047f70d
void*& g_curWidget = *reinterpret_cast<void**>(0x0083ab2c);
int& g_marked = *reinterpret_cast<int*>(0x0083b654);
int& g_dragRow = *reinterpret_cast<int*>(0x0083b650);

typedef int(__fastcall* WrapText_t)(void* ecx, void* edx, const char* text, int width, int* lines, int* count);
const WrapText_t kWrapText = reinterpret_cast<WrapText_t>(0x00476e20);                 // ret 0x10
typedef int(__fastcall* WidgetValue_t)(void* ecx, void* edx);
const WidgetValue_t kWidgetValue = reinterpret_cast<WidgetValue_t>(0x00477580);        // ret
typedef int(__fastcall* MarkupRun_t)(void* ecx, void* edx, const char* text, int len);
const MarkupRun_t kDrawMarkupRun = reinterpret_cast<MarkupRun_t>(0x00477280);          // ret 8
typedef int(__fastcall* WrapToLines_t)(void* ecx, void* edx, int* lines, int* count);
const WrapToLines_t kWrapToLines = reinterpret_cast<WrapToLines_t>(0x004862b0);        // ret 8
typedef int(__fastcall* PixelToCharPos_t)(void* ecx, void* edx, int x, int y, int* lines, int n);
const PixelToCharPos_t kPixelToCharPos = reinterpret_cast<PixelToCharPos_t>(0x004863e0);  // ret 0x10
typedef int(__fastcall* Timer_t)(void* ecx, void* edx);
const Timer_t kTimerStartOnce = reinterpret_cast<Timer_t>(0x00486dc0);                 // ret
const Timer_t kTimerStop = reinterpret_cast<Timer_t>(0x00486ec0);                      // ret
typedef int(__fastcall* Scroll_t)(void* ecx, void* edx, int v);
const Scroll_t kFwdScroll26c = reinterpret_cast<Scroll_t>(0x0047b9f0);                 // ret 4
const Scroll_t kFwdScroll270 = reinterpret_cast<Scroll_t>(0x0047ba10);                 // ret 4
typedef int(__fastcall* SelectedId_t)(void* ecx, void* edx);
const SelectedId_t kSelectedId = reinterpret_cast<SelectedId_t>(0x00489950);           // ret
typedef int(__fastcall* IdAt_t)(void* ecx, void* edx, int index);
const IdAt_t kIdAt = reinterpret_cast<IdAt_t>(0x00489a30);                             // ret 4
typedef int(__fastcall* SetSelection_t)(void* ecx, void* edx, int index, int notify);
const SetSelection_t kSetSelection = reinterpret_cast<SetSelection_t>(0x00489f50);     // ret 8
typedef int(__fastcall* Invalidate_t)(void* ecx, void* edx);
const Invalidate_t kInvalidate = reinterpret_cast<Invalidate_t>(0x00480ce0);           // ret
typedef short(__stdcall* GetKeyState_t)(int vk);
typedef int(__cdecl* SelCallback_t)(int id);

// ================================================================================================================
// EditBox
// Fields (read off the instructions): +0x274 the text widget (ecx of wrapText / Widget_value / drawMarkupRun),
// +0x278 a child whose slots +0xd8 / +0xdc are called with no argument (+0xd8's result is the width),
// +0x574 the text, +0x57c the first visible line, +0x598 / +0x59c two margins, +0x5a4 the caret position,
// +0x5a8 the caret-active flag, +0x5ac a Timer.
// ================================================================================================================

// 0x004862b0  EditBox::wrapToLines(this, lines, count): `ret 8`.
// *count = 1 (0x004862bd) and lines[0] = the text +0x574 (0x004862c4 / 0x004862ca); the width is the child +0x278's
// slot +0xd8 (0 without a child, 0x004862db..0x004862ef) minus twice the sum of the margins +0x598 + +0x59c
// (0x004862cd..0x004862d9, 0x004862f7 / 0x004862fb); wrapText 0x00476e20 runs on the widget +0x274 with
// (text read again, width, lines, count) (0x004862f1..0x00486306); then lines[*count] = text + strlen(text), the
// text read a third time (0x0048630b..0x00486323), and eax is *count read after wrapText (0x0048631a).
WrapToLines_t WrapToLines_orig;
int __fastcall WrapToLines_re(void* self, void*, int* lines, int* count) {
    *count = 1;                                                         // 0x004862bd
    lines[0] = I(self, 0x574);                                          // 0x004862c4 / 0x004862ca
    int pad = Add32(I(self, 0x59c), I(self, 0x598));                    // 0x004862cd..0x004862d9
    void* child = Pp(self, 0x278);                                      // 0x004862db
    int w = 0;                                                          // 0x004862ef
    if (child != 0) {                                                   // 0x004862e1 / 0x004862e3
        w = vslot<Vfn0_t>(child, 0xd8)(child, 0);                       // 0x004862e7
    }
    const char* text = reinterpret_cast<const char*>(Pp(self, 0x574));  // 0x004862f1
    kWrapText(B(self) + 0x274, 0, text, Sub32(w, Add32(pad, pad)), lines, count);   // 0x004862f7..0x00486306
    const char* t = reinterpret_cast<const char*>(Pp(self, 0x574));     // 0x0048630b
    int len = 0;                                                        // repne scasb, 0x00486311..0x0048631f
    while (t[len] != 0) {
        ++len;
    }
    int n = *count;                                                     // 0x0048631a
    lines[n] = reinterpret_cast<int>(t + len);                          // 0x00486320 / 0x00486323
    return n;
}

// 0x004863e0  EditBox::pixelToCharPos(this, x, y, lines, n): `ret 0x10`. Text offset under the point (x, y).
//  - pad = +0x598 + +0x59c (0x004863ef..0x00486411); x and y lose pad (0x00486413 / 0x00486415).
//  - The child +0x278's slot +0xd8 (0 without a child) minus 2*pad minus 1 is the last usable x
//    (0x0048640e..0x00486439); the child, read again, gets its slot +0xdc called and the result is dropped
//    (0x00486433..0x0048643f).
//  - x is clamped to 0 below (0x00486445 / 0x00486447) and then to that limit above (0x0048644b / 0x0048644d, signed).
//  - When lines is null (0x00486458 / 0x0048645a), wrapToLines fills a 32-entry local array and the count n
//    (0x0048645c..0x00486471).
//  - The line is y / Widget_value(widget +0x274) (cdq / idiv at 0x00486486 / 0x00486487) plus the first visible
//    line +0x57c (0x0048648d); a negative sum becomes 0 (jns at 0x00486493) and a line at or past n becomes n-1
//    (0x0048649e / 0x004864a0 / 0x004864a2).
//  - From lines[line], while x > 0 and the pointer is below lines[line+1] (unsigned, 0x004864ad..0x004864b3),
//    drawMarkupRun(widget, p, 1) is subtracted from x and p advances (0x004864b5..0x004864c6).
//  - eax = p minus the text +0x574 (0x004864c8..0x004864d7).
PixelToCharPos_t PixelToCharPos_orig;
int __fastcall PixelToCharPos_re(void* self, void*, int x, int y, int* lines, int n) {
    int pad = Add32(I(self, 0x59c), I(self, 0x598));                    // 0x004863ef / 0x004863f6 / 0x00486411
    void* widget = B(self) + 0x274;                                     // 0x004863fc
    x = Sub32(x, pad);                                                  // 0x00486413
    y = Sub32(y, pad);                                                  // 0x00486415
    void* child = Pp(self, 0x278);                                      // 0x0048640e
    int w = 0;                                                          // 0x0048642c
    if (child != 0) {                                                   // 0x00486417 / 0x00486420
        w = vslot<Vfn0_t>(child, 0xd8)(child, 0);                       // 0x00486424
    }
    int maxX = Sub32(Sub32(w, Add32(pad, pad)), 1);                     // 0x0048642e..0x00486436
    child = Pp(self, 0x278);                                            // 0x00486433
    if (child != 0) {                                                   // 0x00486437 / 0x0048643b
        vslot<Vfn0_t>(child, 0xdc)(child, 0);                           // 0x0048643f (result unused)
    }
    if (x < 0) {                                                        // 0x00486445 / 0x00486447
        x = 0;
    }
    if (x > maxX) {                                                     // 0x0048644b / 0x0048644d
        x = maxX;
    }
    int local[32];                                                      // [esp+0x14], 0x80 bytes
    int* L = lines;
    if (L == 0) {                                                       // 0x00486458 / 0x0048645a
        L = local;
        kWrapToLines(self, 0, local, &n);                               // 0x0048645c..0x00486471
    }
    int h = kWidgetValue(widget, 0);                                    // 0x00486476 / 0x00486478
    int line = Add32(y / h, I(self, 0x57c));                            // 0x00486486..0x0048648d
    if (line < 0) {                                                     // 0x00486493
        line = 0;
    }
    if (line >= n) {                                                    // 0x0048649e / 0x004864a0
        line = n - 1;                                                   // 0x004864a2
    }
    const char* p = reinterpret_cast<const char*>(L[line]);             // 0x004864a5
    const char* end = reinterpret_cast<const char*>(L[line + 1]);       // 0x004864a9
    if (x > 0) {                                                        // 0x004864ad / 0x004864af
        while (reinterpret_cast<unsigned>(p) < reinterpret_cast<unsigned>(end)) {   // 0x004864b1 / 0x004864b3
            int cw = kDrawMarkupRun(widget, 0, p, 1);                   // 0x004864b5..0x004864bd
            ++p;                                                        // 0x004864bc
            x = Sub32(x, cw);                                           // 0x004864c2
            if (x <= 0) {                                               // 0x004864c4 / 0x004864c6
                break;
            }
        }
    }
    return Sub32(reinterpret_cast<int>(p), I(self, 0x574));            // 0x004864c8..0x004864d7
}

// 0x00486b30  EditBox::setCaretActive(this, on): `ret 4`. A nonzero `on` calls Timer::startOnce, zero calls
// Timer::stop, both on the Timer at this+0x5ac (0x00486b38..0x00486b49); `on` is stored at +0x5a8 (0x00486b52) and
// eax is this's slot +0x120 (no argument, 0x00486b4e / 0x00486b58).
typedef int(__fastcall* SetCaretActive_t)(void* ecx, void* edx, int on);
SetCaretActive_t SetCaretActive_orig;
int __fastcall SetCaretActive_re(void* self, void*, int on) {
    void* timer = B(self) + 0x5ac;                                      // 0x00486b3a
    if (on != 0) {                                                      // 0x00486b38 / 0x00486b40
        kTimerStartOnce(timer, 0);                                      // 0x00486b42
    } else {
        kTimerStop(timer, 0);                                           // 0x00486b49
    }
    Vfn0_t f = vslot<Vfn0_t>(self, 0x120);                              // 0x00486b4e
    I(self, 0x5a8) = on;                                                // 0x00486b52
    return f(self, 0);                                                  // 0x00486b58
}

// 0x00486b90  EditBox::beginCaretDrag(this, x, y): `ret 8`. When no drag owns the capture 0x0083ab30
// (0x00486b90 / 0x00486b96 / 0x00486b9a) this takes it and 0x0083ab34 is cleared (0x00486b9c / 0x00486ba2); the
// caret +0x5a4 becomes pixelToCharPos(x, y, NULL, 1) (0x00486bac..0x00486bc5) and eax is this's slot +0x120
// (0x00486bc1 / 0x00486bcb).
typedef int(__fastcall* Drag_t)(void* ecx, void* edx, int x, int y);
Drag_t BeginCaretDrag_orig;
int __fastcall BeginCaretDrag_re(void* self, void*, int x, int y) {
    if (g_dragOwner == 0) {                                             // 0x00486b90 / 0x00486b96 / 0x00486b9a
        g_dragOwner = self;                                             // 0x00486b9c
        g_dragOwner2 = 0;                                               // 0x00486ba2
    }
    int pos = kPixelToCharPos(self, 0, x, y, 0, 1);                     // 0x00486bac..0x00486bbc
    Vfn0_t f = vslot<Vfn0_t>(self, 0x120);                              // 0x00486bc1
    I(self, 0x5a4) = pos;                                               // 0x00486bc5
    return f(self, 0);                                                  // 0x00486bcb
}

// 0x00486be0  EditBox::endCaretDrag(this, x, y): `ret 8`. When this owns 0x0083ab30 (0x00486be0..0x00486bea) the
// capture is released and this's slot +0x10 is called (0x00486bec..0x00486bf8); when 0x0083ab34 is this it is
// cleared (0x00486bfb..0x00486c03); then the caret update of beginCaretDrag (0x00486c0d..0x00486c2c).
Drag_t EndCaretDrag_orig;
int __fastcall EndCaretDrag_re(void* self, void*, int x, int y) {
    if (g_dragOwner == self) {                                          // 0x00486be0 / 0x00486be8 / 0x00486bea
        g_dragOwner = 0;                                                // 0x00486bec
        vslot<Vfn0_t>(self, 0x10)(self, 0);                             // 0x00486bf6 / 0x00486bf8
    }
    if (g_dragOwner2 == self) {                                         // 0x00486bfb / 0x00486c01
        g_dragOwner2 = 0;                                               // 0x00486c03
    }
    int pos = kPixelToCharPos(self, 0, x, y, 0, 1);                     // 0x00486c0d..0x00486c1d
    I(self, 0x5a4) = pos;                                               // 0x00486c22
    return vslot<Vfn0_t>(self, 0x120)(self, 0);                         // 0x00486c28 / 0x00486c2c
}

// 0x00486c40  EditBox::updateCaretDrag(this, x, y): `ret 8`. The caret update of beginCaretDrag runs when this owns
// 0x0083ab30 (0x00486c48 / 0x00486c4a), or when 0x0083ab34 is this and both captures are nonzero (0x00486c4c..
// 0x00486c5c); otherwise nothing changes and eax is the value read from 0x0083ab30 (0x00486c40).
Drag_t UpdateCaretDrag_orig;
int __fastcall UpdateCaretDrag_re(void* self, void*, int x, int y) {
    void* a = g_dragOwner;                                              // 0x00486c40
    if (a != self) {                                                    // 0x00486c48 / 0x00486c4a
        void* c = g_dragOwner2;                                         // 0x00486c4c
        if (c != self || a == 0 || c == 0) {                            // 0x00486c52..0x00486c5c
            return reinterpret_cast<int>(a);
        }
    }
    int pos = kPixelToCharPos(self, 0, x, y, 0, 1);                     // 0x00486c5e..0x00486c6e
    Vfn0_t f = vslot<Vfn0_t>(self, 0x120);                              // 0x00486c73
    I(self, 0x5a4) = pos;                                               // 0x00486c77
    return f(self, 0);                                                  // 0x00486c7d
}

// 0x00496d20  scrollbarOnDrag(this, a, b): `ret 8`, both arguments unused. this's slot +0x120 first (0x00496d23 /
// 0x00496d25); when this owns 0x0083ab30 (0x00496d2b / 0x00496d31) the capture is released and slot +0x10 is called
// (0x00496d33..0x00496d41); when 0x0083ab34 is this it is cleared (0x00496d44..0x00496d4e). eax is the value read
// from 0x0083ab34 (0x00496d44).
Drag_t ScrollbarOnDrag_orig;
int __fastcall ScrollbarOnDrag_re(void* self, void*, int, int) {
    vslot<Vfn0_t>(self, 0x120)(self, 0);                                // 0x00496d25
    if (g_dragOwner == self) {                                          // 0x00496d2b / 0x00496d31
        g_dragOwner = 0;                                                // 0x00496d33
        vslot<Vfn0_t>(self, 0x10)(self, 0);                             // 0x00496d3d / 0x00496d41
    }
    void* c = g_dragOwner2;                                             // 0x00496d44
    if (c == self) {                                                    // 0x00496d49 / 0x00496d4c
        g_dragOwner2 = 0;                                               // 0x00496d4e
    }
    return reinterpret_cast<int>(c);
}

// ================================================================================================================
// ListBox
// ================================================================================================================

// The vbtable pointer: at S+4 (setSelection) and at T-0x58 (onNavKey, onDrag); entry k read afresh.
static inline int* VbtS(void* s) { return *reinterpret_cast<int**>(B(s) + 4); }
static inline int* VbtT(void* t) { return *reinterpret_cast<int**>(B(t) - 0x58); }

// The list cursor walk the compiler inlined from ListModel (same steps as ListModel::idAt 0x00489a36..0x00489a8d),
// on R = T + vbt[2]: head R+0x70, cursor R+0x74, count R+0x78, index R+0x7c. Nothing happens when i > count-1
// (signed); otherwise the cursor restarts at the head; i >= 0 steps i times to +0xc and stores i; a negative i
// steps |i| times to +0x10 and stores count+i, unless |i| > count (then only the cursor reset stays).
// Instances: 0x0048c915..0x0048c95c (onDrag), 0x0048aa79..0x0048aac0, 0x0048ab8f..0x0048abc7,
// 0x0048ac3f..0x0048ac85, 0x0048acf5..0x0048ad3b, 0x0048adb0..0x0048adfe (onNavKey).
static void SeekInline(char* R, int i) {
    int n = I(R, 0x78);
    if (i > n - 1) {
        return;
    }
    Pp(R, 0x74) = Pp(R, 0x70);
    if (i >= 0) {
        for (int k = i; k > 0; --k) {
            Pp(R, 0x74) = Pp(Pp(R, 0x74), 0xc);
        }
        I(R, 0x7c) = i;
        return;
    }
    int a = (i ^ (i >> 31)) - (i >> 31);                                // cdq / xor / sub
    if (a > n) {
        return;
    }
    for (; a > 0; --a) {
        Pp(R, 0x74) = Pp(Pp(R, 0x74), 0x10);
    }
    I(R, 0x7c) = Add32(i, n);
}

// One step of the cursor and the item it lands on (inlined ListModel iteration): with a head, the cursor moves to
// its +0xc and the index +0x7c is incremented, wrapping to 0 at the count +0x78; the item is the cursor node's +8.
// With a null head the original computes the item as NULL and writes [NULL+8] (0x0048abee / 0x0048acb8): the
// fixtures never reach that, and this returns 0 there.
static void* StepItem(char* R) {
    if (Pp(R, 0x70) == 0) {
        return 0;
    }
    Pp(R, 0x74) = Pp(Pp(R, 0x74), 0xc);
    int idx = I(R, 0x7c) + 1;
    I(R, 0x7c) = idx;
    if (idx == I(R, 0x78)) {
        I(R, 0x7c) = 0;
    }
    return Pp(Pp(R, 0x74), 8);
}

// The row under (x, y) shared by onNavKey (0x0048aa39..0x0048ab3e) and onDrag (0x0048c8d0..0x0048c9da). R = T +
// vbt[2]. ry = y - R[-0x1c]; a negative difference (js) means no row. When R[0] != 1 the list is a grid:
// column (x - R[-0x14]) / (R[0x10] + R[-0x10]), row column * R[4] + ry / (R[0x14] + R[-0x10]). When R[0] == 1 the
// rows have their own heights: the cursor is placed on row top-1 (top = T[-0x34]), then at most count passes each
// step the cursor and subtract (R[0x14] + R[-0x10]) * height from ry, height being the item's +0xc (0 without a
// cursor); the passes stop when ry is negative before a pass; with no pass there is no row, else the row is the
// number of passes minus one. Returns 0 for "no row" and sets *row otherwise.
static int RowAt(void* T, int x, int y, int* row) {
    char* R = B(T) + VbtT(T)[2];
    int ry = Sub32(y, I(R, -0x1c));
    if (ry < 0) {
        return 0;
    }
    if (I(R, 0) != 1) {
        int sp = I(R, -0x10);
        int col = Sub32(x, I(R, -0x14)) / Add32(I(R, 0x10), sp);
        *row = Add32(Mul32(col, I(R, 4)), ry / Add32(I(R, 0x14), sp));
        return 1;
    }
    SeekInline(R, I(T, -0x34) - 1);
    R = B(T) + VbtT(T)[2];
    int left = I(R, 0x78);
    if (left <= 0) {
        return 0;
    }
    int passes = 0;
    do {
        if (ry < 0) {
            break;
        }
        void* head = Pp(R, 0x70);
        ++passes;
        if (head != 0) {
            Pp(R, 0x74) = Pp(Pp(R, 0x74), 0xc);
            int idx = I(R, 0x7c) + 1;
            I(R, 0x7c) = idx;
            if (idx == I(R, 0x78)) {
                I(R, 0x7c) = 0;
            }
        }
        void* cur = Pp(R, 0x74);
        int hgt = 0;
        if (cur != 0) {
            hgt = I(Pp(cur, 8), 0xc);  // with a null head the original reads [0+0xc]; the fixtures never do
        }
        R = B(T) + VbtT(T)[2];
        ry = Sub32(ry, Mul32(Add32(I(R, 0x14), I(R, -0x10)), hgt));
        --left;
    } while (left > 0);
    if (passes == 0) {
        return 0;
    }
    *row = passes - 1;
    return 1;
}

// 0x00489f50  ListBox::setSelection(S, index, notify): `ret 8`. sel is the dword at S + vbt[2] + 0xf4.
//  1. When index equals sel and the force flag S+0x10 is 0, eax = &sel and nothing else happens (0x00489f6e..
//     0x00489f77).
//  2. sel = index (0x00489f81). A negative index skips to step 6 (0x00489f83).
//  3. Scroll up: while sel < top (S+0x28, signed, 0x00489f9c / 0x00489fce) top drops by the step (S + vbt[2] +
//     0x60 when the dword at S + vbt[2] + 0x5c is above 1, else 1; 0x00489fa3..0x00489faf) and is set to 0 when it
//     goes negative (jns at 0x00489fb7); each pass marks a change.
//     When the row count S + vbt[2] + 0xd4 is not above the visible rows S+0x2c (0x00489fd8 / 0x00489fde) top = 0
//     (0x00489fe0); otherwise, while sel >= top + visible (0x00489fee / 0x0048a011), top grows by the step and the
//     scroll below always runs (0x0048a019).
//  4. The scroll (0x0048a027..0x0048a05a) runs when there was a change and the row count is above the visible rows:
//     Window::fwdScroll270 (step mode, dword +0x5c above 1) or fwdScroll26c, on S + vbt[1] + 4, with top.
//  5. When the cdecl callback S+0x50 is set it gets ListModel::selectedId(S + vbt[2] + 4) (0x0048a05f..0x0048a076).
//     When notify is nonzero (0x0048a07c..0x0048a082), the parent S + vbt[1] + 0x134 (read afresh each time) gets
//     slot +0x100 (idAt(index)) (0x0048a084..0x0048a0a7), then slot +0xfc (idAt(index), 0x0083b654)
//     (0x0048a0ad..0x0048a0d7), then its +0x94 child, when set, gets slot +0x1c (0x0048a0dd..0x0048a0fc).
//  6. eax = the Window base S + vbt[1] + 4's slot +0x120 (0x0048a0ff..0x0048a10d).
SetSelection_t SetSelection_orig;
int __fastcall SetSelection_re(void* s, void*, int index, int notify) {
    int changed = 0;                                                    // 0x00489f58
    int* sel = reinterpret_cast<int*>(B(s) + VbtS(s)[2] + 0xf4);        // 0x00489f5a..0x00489f67
    if (index == *sel && I(s, 0x10) == 0) {                             // 0x00489f6e..0x00489f77
        return reinterpret_cast<int>(sel);
    }
    *sel = index;                                                       // 0x00489f81
    if (index >= 0) {                                                   // 0x00489f7f / 0x00489f83
        int* t = VbtS(s);                                               // 0x00489f89 (kept in edi)
        char* m = B(s) + t[2];                                          // 0x00489f91 / 0x00489f94
        if (I(m, 0xf4) < I(s, 0x28)) {                                  // 0x00489f96..0x00489f9f
            changed = 1;                                                // 0x00489fa1
            do {
                int step = I(m, 0x5c) > 1 ? I(m, 0x60) : 1;             // 0x00489fa3..0x00489fad
                int top = Sub32(I(s, 0x28), step);                      // 0x00489faf / 0x00489fb2
                I(s, 0x28) = top;                                       // 0x00489fb4
                if (top < 0) {                                          // 0x00489fb7
                    I(s, 0x28) = 0;                                     // 0x00489fb9
                }
                m = B(s) + t[2];                                        // 0x00489fc0 / 0x00489fc3
            } while (I(m, 0xf4) < I(s, 0x28));                          // 0x00489fc5..0x00489fce
        }
        m = B(s) + t[2];                                                // 0x00489fd0 / 0x00489fd6
        int vis = I(s, 0x2c);                                           // 0x00489fd3
        bool scroll;
        if (I(m, 0xd4) <= vis) {                                        // 0x00489fd8 / 0x00489fde
            I(s, 0x28) = 0;                                             // 0x00489fe0
            scroll = changed != 0;                                      // 0x0048a023 / 0x0048a025
        } else if (I(m, 0xf4) >= Add32(vis, I(s, 0x28))) {              // 0x00489fe9..0x00489ff4
            int top;
            do {
                int step = I(m, 0x5c) > 1 ? I(m, 0x60) : 1;             // 0x00489ff6..0x0048a000
                top = Add32(I(s, 0x28), step);                          // 0x0048a002 / 0x0048a005
                I(s, 0x28) = top;                                       // 0x0048a007
                m = B(s) + t[2];                                        // 0x0048a00a / 0x0048a00d
            } while (I(m, 0xf4) >= Add32(top, vis));                    // 0x0048a00f..0x0048a017
            scroll = true;                                              // 0x0048a019 / 0x0048a01d
        } else {
            scroll = changed != 0;                                      // 0x0048a01f..0x0048a025
        }
        if (scroll) {
            char* m2 = B(s) + t[2];                                     // 0x0048a027 / 0x0048a02a
            if (I(m2, 0xd4) > vis) {                                    // 0x0048a02d..0x0048a036
                void* w = B(s) + t[1] + 4;                              // 0x0048a03d / 0x0048a044
                if (I(m2, 0x5c) > 1) {                                  // 0x0048a038 / 0x0048a03b
                    kFwdScroll270(w, 0, I(s, 0x28));                    // 0x0048a040..0x0048a048
                } else {
                    kFwdScroll26c(w, 0, I(s, 0x28));                    // 0x0048a04f..0x0048a05a
                }
            }
        }
        SelCallback_t cb = reinterpret_cast<SelCallback_t>(Pp(s, 0x50)); // 0x0048a05f
        if (cb != 0) {                                                  // 0x0048a062 / 0x0048a064
            int id = kSelectedId(B(s) + VbtS(s)[2] + 4, 0);             // 0x0048a066..0x0048a070
            cb(id);                                                     // 0x0048a075 / 0x0048a076
        }
        if (notify != 0) {                                              // 0x0048a07c / 0x0048a082
            int* t2 = VbtS(s);                                          // 0x0048a084
            void* parent = Pp(s, t2[1] + 0x134);                        // 0x0048a087 / 0x0048a08a
            if (parent != 0) {                                          // 0x0048a091 / 0x0048a093
                Vfn1_t f100 = vslot<Vfn1_t>(parent, 0x100);             // 0x0048a098
                int id = kIdAt(B(s) + t2[2] + 4, 0, index);             // 0x0048a095..0x0048a09f
                f100(parent, 0, id);                                    // 0x0048a0a4..0x0048a0a7
            }
            int* t3 = VbtS(s);                                          // 0x0048a0ad
            parent = Pp(s, t3[1] + 0x134);                              // 0x0048a0b0 / 0x0048a0b3
            if (parent != 0) {                                          // 0x0048a0ba / 0x0048a0bc
                int marked = g_marked;                                  // 0x0048a0be
                Vfn2_t ffc = vslot<Vfn2_t>(parent, 0xfc);               // 0x0048a0c7
                int id = kIdAt(B(s) + t3[2] + 4, 0, index);             // 0x0048a0c4..0x0048a0cf
                ffc(parent, 0, id, marked);                             // 0x0048a0d4..0x0048a0d7
            }
            int* t4 = VbtS(s);                                          // 0x0048a0dd
            parent = Pp(s, t4[1] + 0x134);                              // 0x0048a0e0 / 0x0048a0e3
            if (parent != 0) {                                          // 0x0048a0ea / 0x0048a0ec
                void* child = Pp(parent, 0x94);                         // 0x0048a0ee
                if (child != 0) {                                       // 0x0048a0f4 / 0x0048a0f6
                    vslot<Vfn0_t>(child, 0x1c)(child, 0);               // 0x0048a0f8..0x0048a0fc
                }
            }
        }
    }
    void* w = B(s) + VbtS(s)[1] + 4;                                    // 0x0048a0ff..0x0048a109
    return vslot<Vfn0_t>(w, 0x120)(w, 0);                               // 0x0048a10d
}

// 0x0048aa00  ListBox::onNavKey(T, x, y): `ret 8`; the callers (the thunk 0x0048ccb0 and viewDispatchSlot30
// 0x004a0b90) do not read eax. A click on row selection with Ctrl / Shift handling:
//  1. 0x0083ab2c = the parent T + vbt[1] + 0xd8 (0x0048aa06..0x0048aa13). Nothing more with an empty list (count
//     T + vbt[2] + 0x78, 0x0048aa19..0x0048aa25).
//  2. (x, y) is stored at T-0x3c / T-0x38 (0x0048aa33 / 0x0048aa36); the row under it as in RowAt above
//     (0x0048aa39..0x0048ab3e); none ends the call. A row at or past the visible rows T-0x30 ends it (0x0048ab3f /
//     0x0048ab42); r = top T-0x34 + row, and r at or past the count ends it (0x0048ab48..0x0048ab5f).
//  3. When the multi-select flag T-0x50 is set and r is not the selected index R+0x98 (0x0048ab65..0x0048ab76):
//     GetKeyState(VK_CONTROL) through the import slot 0x004ba1f0 (0x0048ab7c..0x0048ab84); without bit 15
//     (0x0048ab86 / 0x0048ab89) every item's flag +8 is cleared: cursor placed on row -1, then count steps
//     (0x0048ab8f..0x0048ac16). Then GetKeyState(VK_SHIFT) (0x0048ac18 / 0x0048ac1a):
//     - with bit 15 and r > sel (0x0048ac35 / 0x0048ac39): cursor on sel-1, then (when sel <= r, 0x0048ac99)
//       r - sel + 1 steps set each item's +8 to 1 (0x0048aca1..0x0048acea);
//     - with bit 15 and r <= sel: cursor on r-1, then while the step counter from r stays <= sel (re-read each
//       pass) the items get +8 = 1 (0x0048acf1..0x0048ada2);
//     - without bit 15: cursor on r (0x0048ada9..0x0048adfe) and the item there toggles its +8 between 0 and 1
//       (0x0048ae01..0x0048ae4f).
//  4. setSelection(T-0x5c, r, 1) (0x0048ae56..0x0048ae60); then, when bit 2 of the byte T + vbt[2] - 0x34 is set
//     and the parent is not null, the parent's slot +0xd4 gets -1 (0x0048ae65..0x0048ae84).
typedef void(__fastcall* OnPoint_t)(void* ecx, void* edx, int x, int y);
OnPoint_t OnNavKey_orig;
void __fastcall OnNavKey_re(void* T, void*, int x, int y) {
    g_curWidget = Pp(T, VbtT(T)[1] + 0xd8);                             // 0x0048aa06..0x0048aa13
    if (I(T, VbtT(T)[2] + 0x78) == 0) {                                 // 0x0048aa19..0x0048aa25
        return;
    }
    I(T, -0x3c) = x;                                                    // 0x0048aa33
    I(T, -0x38) = y;                                                    // 0x0048aa36
    int row;
    if (!RowAt(T, x, y, &row)) {                                        // 0x0048aa39..0x0048ab38
        return;
    }
    if (row >= I(T, -0x30)) {                                           // 0x0048ab3f / 0x0048ab42
        return;
    }
    int r = Add32(I(T, -0x34), row);                                    // 0x0048ab4b / 0x0048ab4e
    char* R = B(T) + VbtT(T)[2];                                        // 0x0048ab48..0x0048ab57
    if (r >= I(R, 0x78)) {                                              // 0x0048ab59 / 0x0048ab5f
        return;
    }
    if (I(T, -0x50) != 0 && r != I(R, 0x98)) {                          // 0x0048ab65..0x0048ab76
        GetKeyState_t gks = *reinterpret_cast<GetKeyState_t*>(0x004ba1f0);   // 0x0048ab7c
        if ((gks(0x11) & 0x8000) == 0) {                                // 0x0048ab82..0x0048ab89
            SeekInline(B(T) + VbtT(T)[2], -1);                          // 0x0048ab8f..0x0048abc7
            int n = I(T, VbtT(T)[2] + 0x78);                            // 0x0048abca..0x0048abd0
            if (n > 0) {                                                // 0x0048abd4 / 0x0048abd6
                for (int k = n; k != 0; --k) {                          // 0x0048abd8 / 0x0048ac12 / 0x0048ac16
                    void* item = StepItem(B(T) + VbtT(T)[2]);           // 0x0048abda..0x0048ac0f
                    I(item, 8) = 0;                                     // 0x0048ac13
                }
            }
        }
        if ((gks(0x10) & 0x8000) != 0) {                                // 0x0048ac18..0x0048ac1f
            R = B(T) + VbtT(T)[2];                                      // 0x0048ac25..0x0048ac32
            int sel = I(R, 0x98);                                       // 0x0048ac2b
            if (r > sel) {                                              // 0x0048ac35 / 0x0048ac39
                SeekInline(R, sel - 1);                                 // 0x0048ac3f..0x0048ac85
                sel = I(T, VbtT(T)[2] + 0x98);                          // 0x0048ac88..0x0048ac8e
                if (sel <= r) {                                         // 0x0048ac99 / 0x0048ac9b
                    int cnt = r - sel + 1;                              // 0x0048aca1..0x0048aca5
                    do {
                        void* item = StepItem(B(T) + VbtT(T)[2]);       // 0x0048aca6..0x0048acdf
                        I(item, 8) = 1;                                 // 0x0048ace3
                    } while (--cnt != 0);                               // 0x0048ace2 / 0x0048acea
                }
            } else {
                SeekInline(R, r - 1);                                   // 0x0048acf1..0x0048ad3b
                int k = r;                                              // 0x0048ad41
                R = B(T) + VbtT(T)[2];                                  // 0x0048ad3e..0x0048ad46
                if (k <= I(R, 0x98)) {                                  // 0x0048ad49..0x0048ad52
                    do {
                        void* item = StepItem(R);                       // 0x0048ad58..0x0048ad86
                        I(item, 8) = 1;                                 // 0x0048ad89
                        ++k;                                            // 0x0048ad93
                        R = B(T) + VbtT(T)[2];                          // 0x0048ad90..0x0048ad9e
                    } while (k <= I(R, 0x98));                          // 0x0048ad97 / 0x0048ada0 / 0x0048ada2
                }
            }
        } else {
            SeekInline(B(T) + VbtT(T)[2], r);                           // 0x0048ada9..0x0048adfe
            R = B(T) + VbtT(T)[2];                                      // 0x0048ae01..0x0048ae07
            void* item = Pp(R, 0x70) != 0 ? Pp(Pp(R, 0x74), 8) : 0;    // 0x0048ae0b..0x0048ae1b
            int on = I(item, 8);                                        // 0x0048ae1d
            void* head = Pp(R, 0x70);                                   // 0x0048ae20
            if (on != 0) {                                              // 0x0048ae23 / 0x0048ae25
                I(head != 0 ? Pp(Pp(R, 0x74), 8) : 0, 8) = 0;           // 0x0048ae27..0x0048ae3c
            } else {
                I(head != 0 ? Pp(Pp(R, 0x74), 8) : 0, 8) = 1;           // 0x0048ae41..0x0048ae4f
            }
        }
    }
    kSetSelection(B(T) - 0x5c, 0, r, 1);                                // 0x0048ae56..0x0048ae60
    int* t = VbtT(T);                                                   // 0x0048ae65
    if ((*reinterpret_cast<unsigned char*>(B(T) + t[2] - 0x34) & 4) != 0) {   // 0x0048ae68..0x0048ae70
        void* parent = Pp(T, t[1] + 0xd8);                              // 0x0048ae72 / 0x0048ae75
        if (parent != 0) {                                              // 0x0048ae7c / 0x0048ae7e
            vslot<Vfn1_t>(parent, 0xd4)(parent, 0, -1);                 // 0x0048ae80..0x0048ae84
        }
    }
}

// 0x0048c8a0  ListBox::onDrag(T, x, y): `ret 8`; the callers (the thunk 0x0048cc80 and viewDispatchSlot27
// 0x004a0af0) do not read eax. The hover row follows the pointer:
//  1. Nothing with an empty list (T + vbt[2] + 0x78, 0x0048c8a7..0x0048c8b6). old = the hover row R+0x9c
//     (0x0048c8bc / 0x0048c8d3); (x, y) is stored at T-0x3c / T-0x38 (0x0048c8ca / 0x0048c8cd).
//  2. The row under the point as in RowAt (0x0048c8d0..0x0048c9da); none ends the call. r = row + top T-0x34
//     (0x0048c9db..0x0048c9e1); r at or past the count ends it (0x0048c9ea / 0x0048c9ef).
//  3. When r is the selected index R+0x98 (0x0048c9f5 / 0x0048c9fb): a hover row of -1 ends the call
//     (0x0048ca03..0x0048ca08); else the hover row becomes -1 (0x0048ca0e), 0x0083b650 = old - top (0x0048ca20)
//     and slot 0 of the object at T-0x5c gets old (0x0048ca1f..0x0048ca2c).
//     Otherwise the hover row becomes r (0x0048ca30); when bit 0 of the byte T-0x54 is set setSelection(T-0x5c, r,
//     1) runs (0x0048ca36..0x0048ca43); a hover row equal to old ends the call (0x0048ca48..0x0048ca59); when old is
//     not -1, 0x0083b650 = old - top and slot 0 gets old (0x0048ca5b..0x0048ca73); then 0x0083b650 = hover - top and
//     slot 0 gets the hover row (0x0048ca75..0x0048caa0).
//  4. Window::invalidate on T - 0x58 + vbt[1] (0x0048caa2..0x0048caac).
OnPoint_t OnDrag_orig;
void __fastcall OnDrag_re(void* T, void*, int x, int y) {
    if (I(T, VbtT(T)[2] + 0x78) == 0) {                                 // 0x0048c8a7..0x0048c8b6
        return;
    }
    int old = I(T, VbtT(T)[2] + 0x9c);                                  // 0x0048c8bc / 0x0048c8d3
    I(T, -0x3c) = x;                                                    // 0x0048c8ca
    I(T, -0x38) = y;                                                    // 0x0048c8cd
    int row;
    if (!RowAt(T, x, y, &row)) {                                        // 0x0048c8d0..0x0048c9d4
        return;
    }
    int r = Add32(row, I(T, -0x34));                                    // 0x0048c9db / 0x0048c9e1
    char* R = B(T) + VbtT(T)[2];                                        // 0x0048c9de..0x0048c9ec
    if (r >= I(R, 0x78)) {                                              // 0x0048c9ea / 0x0048c9ef
        return;
    }
    void* s = B(T) - 0x5c;                                              // 0x0048ca29 / 0x0048ca40 / 0x0048ca67
    if (r == I(R, 0x98)) {                                              // 0x0048c9f5 / 0x0048c9fb
        if (I(R, 0x9c) == -1) {                                         // 0x0048c9fd..0x0048ca08
            return;
        }
        I(R, 0x9c) = -1;                                                // 0x0048ca0e
        g_dragRow = Sub32(old, I(T, -0x34));                            // 0x0048ca14..0x0048ca20
        vslot<Vfn1_t>(s, 0)(s, 0, old);                                 // 0x0048ca1f..0x0048ca2c
    } else {
        I(R, 0x9c) = r;                                                 // 0x0048ca30
        if ((*reinterpret_cast<unsigned char*>(B(T) - 0x54) & 1) != 0) {   // 0x0048ca36 / 0x0048ca39
            kSetSelection(s, 0, r, 1);                                  // 0x0048ca3d..0x0048ca43
        }
        if (I(T, VbtT(T)[2] + 0x9c) == old) {                           // 0x0048ca48..0x0048ca59
            return;
        }
        if (old != -1) {                                                // 0x0048ca5b / 0x0048ca5e
            g_dragRow = Sub32(old, I(T, -0x34));                        // 0x0048ca60..0x0048ca6a
            vslot<Vfn1_t>(s, 0)(s, 0, old);                             // 0x0048ca70..0x0048ca73
        }
        g_dragRow = Sub32(I(T, VbtT(T)[2] + 0x9c), I(T, -0x34));        // 0x0048ca75..0x0048ca8a
        Vfn1_t f0 = vslot<Vfn1_t>(s, 0);                                // 0x0048ca93
        f0(s, 0, I(T, VbtT(T)[2] + 0x9c));                              // 0x0048ca90..0x0048caa0
    }
    kInvalidate(B(T) - 0x58 + VbtT(T)[1], 0);                           // 0x0048caa2..0x0048caac
}

// The vtordisp thunks of table 0x004bb260: `sub ecx, dword ptr [ecx - 4]` then a jump to the body.
static inline void* VtordispThis(void* p) { return B(p) - *reinterpret_cast<int*>(B(p) - 4); }

// 0x0048cc80  ListBox::onDrag_vtordisp: 0x0048cc80 sub, 0x0048cc83 jmp 0x0048c8a0 (slot 27 of 0x004bb260).
OnPoint_t OnDragVd_orig;
void __fastcall OnDragVd_re(void* p, void*, int x, int y) {
    reinterpret_cast<OnPoint_t>(0x0048c8a0)(VtordispThis(p), 0, x, y);
}
// 0x0048ccb0  ListBox::onNavKey_vtordisp: 0x0048ccb0 sub, 0x0048ccb3 jmp 0x0048aa00 (slot 30 of 0x004bb260).
OnPoint_t OnNavKeyVd_orig;
void __fastcall OnNavKeyVd_re(void* p, void*, int x, int y) {
    reinterpret_cast<OnPoint_t>(0x0048aa00)(VtordispThis(p), 0, x, y);
}

// ================================================================================================================
// optionDialog, listPanel, Timer
// ================================================================================================================

// 0x0049fda0  optionDialogOnMouse(X, x, y): `ret 8`. The vbtable pointer is at X-0x1c (0x0049fda6); s = X + vbt[2]
// (0x0049fda9 / 0x0049fdb5) holds the grid: spacing +0x2c, cell width +0x4c, cell height +0x50, rows per column
// +0x40, a disabled-item mask +0xd4 and the hover index +0xd8.
//   index = (x / (cell width + spacing)) * rows + y / (cell height + spacing)   (cdq / idiv at 0x0049fdb0..0x0049fdd0,
//           imul at 0x0049fdcc, add at 0x0049fdd7)
//   bit = 1 << (index & 31) (`shl edx, cl` at 0x0049fddf)
// When that bit of the mask is set (0x0049fde1 / 0x0049fde3) the hover index becomes -1, unless it already is -1, in
// which case eax = -1 and nothing else happens (0x0049fde5..0x0049fdf2); when it is clear the hover index becomes
// index (0x0049fdfa). After a change eax is slot +0x120 of the Window base X - 0x1c + vbt[1] (0x0049fe00..0x0049fe0e).
typedef int(__fastcall* Mouse_t)(void* ecx, void* edx, int x, int y);
Mouse_t OptionDialogOnMouse_orig;
int __fastcall OptionDialogOnMouse_re(void* X, void*, int x, int y) {
    int* vbt = *reinterpret_cast<int**>(B(X) - 0x1c);                   // 0x0049fda6
    char* s = B(X) + vbt[2];                                            // 0x0049fda9 / 0x0049fdb5
    int sp = I(s, 0x2c);                                                // 0x0049fdb1
    int col = x / Add32(I(s, 0x4c), sp);                                // 0x0049fdb8..0x0049fdc3
    int row = y / Add32(I(s, 0x50), sp);                                // 0x0049fdbc..0x0049fdd0
    int index = Add32(Mul32(col, I(s, 0x40)), row);                     // 0x0049fdcc / 0x0049fdd7
    unsigned bit = 1u << (index & 31);                                  // 0x0049fdd2 / 0x0049fddf
    if ((bit & static_cast<unsigned>(I(s, 0xd4))) != 0) {               // 0x0049fdd9..0x0049fde3
        if (I(s, 0xd8) == -1) {                                         // 0x0049fde5..0x0049fdf0
            return -1;
        }
        I(s, 0xd8) = -1;                                                // 0x0049fdf2
    } else {
        I(s, 0xd8) = index;                                             // 0x0049fdfa
    }
    void* w = B(X) - 0x1c + (*reinterpret_cast<int**>(B(X) - 0x1c))[1];   // 0x0049fe00..0x0049fe0a
    return vslot<Vfn0_t>(w, 0x120)(w, 0);                               // 0x0049fe0e
}

// 0x0049e640  listSelectionChanged(Y, a, b): `ret 8`, both arguments unused. 0x0083ab2c = the dword at
// Y + vbt[1] + 0x10c, vbt being the vbtable at Y-0x24 (0x0049e643..0x0049e650). When the cdecl callback at Y-0x10
// is set (0x0049e656..0x0049e65b) it gets ListModel::selectedId(Y - 0x24 + vbt[2]) (0x0049e65d..0x0049e66d) and eax
// is its result; otherwise eax is 0 (the callback slot just read).
typedef int(__fastcall* Two_t)(void* ecx, void* edx, int a, int b);
Two_t ListSelectionChanged_orig;
int __fastcall ListSelectionChanged_re(void* Y, void*, int, int) {
    int* vbt = *reinterpret_cast<int**>(B(Y) - 0x24);                   // 0x0049e643
    g_curWidget = Pp(Y, vbt[1] + 0x10c);                                // 0x0049e646..0x0049e650
    SelCallback_t cb = reinterpret_cast<SelCallback_t>(Pp(Y, -0x10));   // 0x0049e656
    if (cb == 0) {                                                      // 0x0049e659 / 0x0049e65b
        return 0;
    }
    int* vbt2 = *reinterpret_cast<int**>(B(Y) - 0x24);                  // 0x0049e65d
    int id = kSelectedId(B(Y) - 0x24 + vbt2[2], 0);                     // 0x0049e660..0x0049e667
    return cb(id);                                                      // 0x0049e66c / 0x0049e66d
}

// 0x00495cb0  Timer_fire(timer): stdcall, `ret 4`. timer+0x24 = 0 (0x00495cb5). While 0x0083ad4c is nonzero eax is
// that value and nothing else happens (0x00495cbc..0x00495cc3). With bit 0 of the flags +8 set: bit 1 also set
// calls Timer::stop on the timer and returns its eax (0x00495ccc..0x00495cd8); bit 1 clear sets it (0x00495cdb /
// 0x00495cdd). When 0x0083aff0 is nonzero and is not this timer, eax is that value and nothing else happens
// (0x00495ce0..0x00495ceb). Otherwise the cdecl callback +0x10 gets (+0x1c) (0x00495ced..0x00495cfa) and the cdecl
// callback +0x14 gets (+0x1c, +0x18) (0x00495cfd..0x00495d0e), each when set; eax is the second callback's result,
// or 0 (the slot just read) when it is not set.
typedef int(__stdcall* TimerFire_t)(void* timer);
typedef int(__cdecl* Cb1_t)(int a);
typedef int(__cdecl* Cb2_t)(int a, int b);
TimerFire_t TimerFire_orig;
int __stdcall TimerFire_re(void* t) {
    I(t, 0x24) = 0;                                                     // 0x00495cb5
    int busy = *reinterpret_cast<int*>(0x0083ad4c);                     // 0x00495cbc
    if (busy != 0) {                                                    // 0x00495cc1 / 0x00495cc3
        return busy;
    }
    int f = I(t, 8);                                                    // 0x00495cc5
    if ((f & 1) != 0) {                                                 // 0x00495cc8 / 0x00495cca
        if ((f & 2) != 0) {                                             // 0x00495ccc / 0x00495cce
            return kTimerStop(t, 0);                                    // 0x00495cd0 / 0x00495cd2
        }
        I(t, 8) = f | 2;                                                // 0x00495cdb / 0x00495cdd
    }
    void* owner = *reinterpret_cast<void**>(0x0083aff0);                // 0x00495ce0
    if (owner != 0 && owner != t) {                                     // 0x00495ce5..0x00495ceb
        return reinterpret_cast<int>(owner);
    }
    Cb1_t cb1 = reinterpret_cast<Cb1_t>(Pp(t, 0x10));                   // 0x00495ced
    if (cb1 != 0) {                                                     // 0x00495cf0 / 0x00495cf2
        cb1(I(t, 0x1c));                                                // 0x00495cf4..0x00495cf8
    }
    Cb2_t cb2 = reinterpret_cast<Cb2_t>(Pp(t, 0x14));                   // 0x00495cfd
    if (cb2 == 0) {                                                     // 0x00495d00 / 0x00495d02
        return 0;
    }
    return cb2(I(t, 0x1c), I(t, 0x18));                                 // 0x00495d04..0x00495d0c
}

// 0x0049ffb0  optionDialogOnMouse_vtordisp: 0x0049ffb0 sub ecx, [ecx-4]; 0x0049ffb3 jmp 0x0049fda0 (slot 27 of
// 0x004bbeb0).
Mouse_t OptionDialogOnMouseVd_orig;
int __fastcall OptionDialogOnMouseVd_re(void* p, void*, int x, int y) {
    return reinterpret_cast<Mouse_t>(0x0049fda0)(VtordispThis(p), 0, x, y);
}
// 0x00491ba0  listSelectionChanged_vtordisp: 0x00491ba0 sub ecx, [ecx-4]; 0x00491ba3 jmp 0x0049e640 (slot 38 of
// 0x004bb744).
Two_t ListSelectionChangedVd_orig;
int __fastcall ListSelectionChangedVd_re(void* p, void*, int a, int b) {
    return reinterpret_cast<Two_t>(0x0049e640)(VtordispThis(p), 0, a, b);
}

typedef int(__fastcall* One_t)(void* ecx, void* edx, int a);
// The four cases of viewDispatchSlot27 all enter their target as `mov eax, [esp+8]; mov edx, [esp+4]; push eax;
// push edx; add ecx, -k; call target`, so eax = b and edx = a at the call. One target, 0x004a4340, is a bare `ret 8`
// whose result is that eax; this helper reproduces the register state so every target sees what the original passes.
static int __declspec(noinline) CallCase(void* fn, void* self, int a, int b) {
    int r;
    __asm {
        mov eax, b
        mov edx, a
        push eax
        push edx
        mov ecx, self
        call fn
        mov r, eax
    }
    return r;
}

// 0x004a0ab0  viewDispatchSlot27(V, a, b): `ret 8`. tag = the dword at V-8 (0x004a0ab0); tag-1 above 15 (unsigned,
// 0x004a0ab3..0x004a0ab7) or a tag whose byte in the index table 0x004a0b3c is 4 returns eax = tag-1 (0x004a0b25).
// The index table maps tag 1 -> 0, 2 -> 1, 8 -> 2, 0x10 -> 3 (bytes at 0x004a0b3c, 0x004a0b3d, 0x004a0b43, 0x004a0b4b),
// and the jump table 0x004a0b28 sends them to:
//   tag 1    optionDialogOnMouse 0x0049fda0 on V-0x168 (0x004a0af8..0x004a0b08)
//   tag 2    ListBox::onDrag 0x0048c8a0 on V-0x1a0 (0x004a0ae0..0x004a0af0)
//   tag 8    0x004a4340 (`ret 8`) on V-0x8c (0x004a0b10..0x004a0b20)
//   tag 0x10 listPanelReadValue 0x0049e7d0 on V-0x180 (0x004a0ac8..0x004a0ad8)
// with (a, b), and eax is the target's.
Two_t ViewDispatchSlot27_orig;
int __fastcall ViewDispatchSlot27_re(void* V, void*, int a, int b) {
    int tag = I(V, -8);                                                 // 0x004a0ab0
    switch (tag) {
    case 1:
        return CallCase(reinterpret_cast<void*>(0x0049fda0), B(V) - 0x168, a, b);   // 0x004a0af8..0x004a0b08
    case 2:
        return CallCase(reinterpret_cast<void*>(0x0048c8a0), B(V) - 0x1a0, a, b);   // 0x004a0ae0..0x004a0af0
    case 8:
        return CallCase(reinterpret_cast<void*>(0x004a4340), B(V) - 0x8c, a, b);    // 0x004a0b10..0x004a0b20
    case 0x10:
        return CallCase(reinterpret_cast<void*>(0x0049e7d0), B(V) - 0x180, a, b);   // 0x004a0ac8..0x004a0ad8
    default:
        return Sub32(tag, 1);                                           // 0x004a0ab3 / 0x004a0b25
    }
}

// Four slot bodies, each the target of a vtordisp thunk below and called by one of the view dispatchers
// (viewDispatchSlot60 0x004a0ff0 at 0x004a1013 / 0x004a1039, viewDispatchSlot28 0x004a0a10 at 0x004a0a68,
// viewDispatchSelectChanged 0x004a0c90 at 0x004a0ce8). 0x0049e7b0 and 0x0049fd80 are named in re/names/exe_8.tsv;
// 0x0049fe20 and 0x0049fc30 are FUN_ in the master with no hooks.csv row (names proposed in log/c3/c3ct_names.tsv).
// 0x0049e7b0  listPanelSlot60(Y, a): `ret 4`, a unused. eax = slot +0x120 of the Window base Y - 0x24 + vbt[1], vbt
// being the vbtable at Y-0x24 (0x0049e7b0..0x0049e7be).
One_t ListPanelSlot60_orig;
int __fastcall ListPanelSlot60_re(void* Y, void*, int) {
    void* w = B(Y) - 0x24 + (*reinterpret_cast<int**>(B(Y) - 0x24))[1];   // 0x0049e7b0..0x0049e7ba
    return vslot<Vfn0_t>(w, 0x120)(w, 0);                               // 0x0049e7be
}
// 0x0049fe20  optionDialogSlot28(X, a, b): `ret 8`, both unused. The hover index X + vbt[2] + 0xd8 becomes -1
// (0x0049fe20..0x0049fe26), vbt being the vbtable at X-0x1c; eax = slot +0x120 of X - 0x1c + vbt[1] (vbt read again,
// 0x0049fe31..0x0049fe3f).
Two_t OptionDialogSlot28_orig;
int __fastcall OptionDialogSlot28_re(void* X, void*, int, int) {
    I(X, (*reinterpret_cast<int**>(B(X) - 0x1c))[2] + 0xd8) = -1;       // 0x0049fe20..0x0049fe26
    void* w = B(X) - 0x1c + (*reinterpret_cast<int**>(B(X) - 0x1c))[1];   // 0x0049fe31..0x0049fe3b
    return vslot<Vfn0_t>(w, 0x120)(w, 0);                               // 0x0049fe3f
}
// 0x0049fc30  optionDialogSlot38(X, a, b): `ret 8`, both unused. 0x0083ab2c = the dword at X + vbt[1] + 0x114
// (0x0049fc30..0x0049fc3d). When the cdecl callback at X-0x10 is set (0x0049fc42..0x0049fc47) it gets the mask
// X + vbt[2] + 0xd4 (0x0049fc49..0x0049fc57) and eax is its result; otherwise eax is 0 (the slot just read).
Two_t OptionDialogSlot38_orig;
int __fastcall OptionDialogSlot38_re(void* X, void*, int, int) {
    g_curWidget = Pp(X, (*reinterpret_cast<int**>(B(X) - 0x1c))[1] + 0x114);   // 0x0049fc30..0x0049fc3d
    SelCallback_t cb = reinterpret_cast<SelCallback_t>(Pp(X, -0x10));   // 0x0049fc42
    if (cb == 0) {                                                      // 0x0049fc45 / 0x0049fc47
        return 0;
    }
    return cb(I(X, (*reinterpret_cast<int**>(B(X) - 0x1c))[2] + 0xd4));   // 0x0049fc49..0x0049fc57
}
// 0x0049fd80  optionDialogSlot60(X, a): `ret 4`, a unused. eax = slot +0x120 of X - 0x1c + vbt[1] (0x0049fd80..
// 0x0049fd8e).
One_t OptionDialogSlot60_orig;
int __fastcall OptionDialogSlot60_re(void* X, void*, int) {
    void* w = B(X) - 0x1c + (*reinterpret_cast<int**>(B(X) - 0x1c))[1];   // 0x0049fd80..0x0049fd8a
    return vslot<Vfn0_t>(w, 0x120)(w, 0);                               // 0x0049fd8e
}

// The four vtordisp thunks of those bodies: each thunk is
// `sub ecx, dword ptr [ecx - 4]` and a jmp, so it passes its stack arguments to the body untouched and the body runs
// on this - [this-4]. The reimplementation performs the same adjustment and enters the body at its address (hooked
// above, so the verifier runs these with the bodies' hooks off as well).
// 0x00491bb0  listPanelSlot60_vtordisp: 0x00491bb0 sub, 0x00491bb3 jmp 0x0049e7b0 (slot 60 of 0x004bb744). The body
// (`ret 4`) calls slot +0x120 of the Window base Y - 0x24 + vbt[1], vbt at Y-0x24 (0x0049e7b0..0x0049e7be).
One_t ListPanelSlot60Vd_orig;
int __fastcall ListPanelSlot60Vd_re(void* p, void*, int a) {
    return reinterpret_cast<One_t>(0x0049e7b0)(VtordispThis(p), 0, a);
}
// 0x0049ffc0  optionDialogSlot28_vtordisp: 0x0049ffc0 sub, 0x0049ffc3 jmp 0x0049fe20 (slot 28 of 0x004bbeb0). The
// body (`ret 8`) sets the hover index X + vbt[2] + 0xd8 to -1 and calls slot +0x120 of X - 0x1c + vbt[1]
// (0x0049fe20..0x0049fe3f).
Two_t OptionDialogSlot28Vd_orig;
int __fastcall OptionDialogSlot28Vd_re(void* p, void*, int a, int b) {
    return reinterpret_cast<Two_t>(0x0049fe20)(VtordispThis(p), 0, a, b);
}
// 0x0049fff0  optionDialogSlot38_vtordisp: 0x0049fff0 sub, 0x0049fff3 jmp 0x0049fc30 (slot 38 of 0x004bbeb0). The
// body (`ret 8`) stores the dword at X + vbt[1] + 0x114 to 0x0083ab2c and, when the cdecl callback at X-0x10 is set,
// passes it the mask X + vbt[2] + 0xd4 (0x0049fc30..0x0049fc59).
Two_t OptionDialogSlot38Vd_orig;
int __fastcall OptionDialogSlot38Vd_re(void* p, void*, int a, int b) {
    return reinterpret_cast<Two_t>(0x0049fc30)(VtordispThis(p), 0, a, b);
}
// 0x00491980  viewDispatchSlot27_vtordisp: 0x00491980 sub, 0x00491983 jmp 0x004a0ab0 (slot 27 of 0x004bb8d0).
Two_t ViewDispatchSlot27Vd_orig;
int __fastcall ViewDispatchSlot27Vd_re(void* p, void*, int a, int b) {
    return reinterpret_cast<Two_t>(0x004a0ab0)(VtordispThis(p), 0, a, b);
}
// 0x004a0000  optionDialogSlot60_vtordisp: 0x004a0000 sub, 0x004a0003 jmp 0x0049fd80 (slot 60 of 0x004bbeb0). The
// body (`ret 4`) calls slot +0x120 of X - 0x1c + vbt[1] (0x0049fd80..0x0049fd8e).
One_t OptionDialogSlot60Vd_orig;
int __fastcall OptionDialogSlot60Vd_re(void* p, void*, int a) {
    return reinterpret_cast<One_t>(0x0049fd80)(VtordispThis(p), 0, a);
}

}  // namespace

SG_HOOK("golf_clean.exe", 0x004862b0, EditBox_wrapToLines, WrapToLines_re, WrapToLines_orig);
SG_HOOK("golf_clean.exe", 0x004863e0, EditBox_pixelToCharPos, PixelToCharPos_re, PixelToCharPos_orig);
SG_HOOK("golf_clean.exe", 0x00486b30, EditBox_setCaretActive, SetCaretActive_re, SetCaretActive_orig);
SG_HOOK("golf_clean.exe", 0x00486b90, EditBox_beginCaretDrag, BeginCaretDrag_re, BeginCaretDrag_orig);
SG_HOOK("golf_clean.exe", 0x00486be0, EditBox_endCaretDrag, EndCaretDrag_re, EndCaretDrag_orig);
SG_HOOK("golf_clean.exe", 0x00486c40, EditBox_updateCaretDrag, UpdateCaretDrag_re, UpdateCaretDrag_orig);
SG_HOOK("golf_clean.exe", 0x00496d20, scrollbarOnDrag, ScrollbarOnDrag_re, ScrollbarOnDrag_orig);
SG_HOOK("golf_clean.exe", 0x00489f50, ListBox_setSelection, SetSelection_re, SetSelection_orig);
SG_HOOK("golf_clean.exe", 0x0048aa00, ListBox_onNavKey, OnNavKey_re, OnNavKey_orig);
SG_HOOK("golf_clean.exe", 0x0048c8a0, ListBox_onDrag, OnDrag_re, OnDrag_orig);
SG_HOOK("golf_clean.exe", 0x0048cc80, ListBox_onDrag_vtordisp, OnDragVd_re, OnDragVd_orig);
SG_HOOK("golf_clean.exe", 0x0048ccb0, ListBox_onNavKey_vtordisp, OnNavKeyVd_re, OnNavKeyVd_orig);
SG_HOOK("golf_clean.exe", 0x0049fda0, optionDialogOnMouse, OptionDialogOnMouse_re, OptionDialogOnMouse_orig);
SG_HOOK("golf_clean.exe", 0x0049e640, listSelectionChanged, ListSelectionChanged_re, ListSelectionChanged_orig);
SG_HOOK("golf_clean.exe", 0x00495cb0, Timer_fire, TimerFire_re, TimerFire_orig);
SG_HOOK("golf_clean.exe", 0x0049ffb0, optionDialogOnMouse_vtordisp, OptionDialogOnMouseVd_re, OptionDialogOnMouseVd_orig);
SG_HOOK("golf_clean.exe", 0x00491ba0, listSelectionChanged_vtordisp, ListSelectionChangedVd_re, ListSelectionChangedVd_orig);
SG_HOOK("golf_clean.exe", 0x00491bb0, listPanelSlot60_vtordisp, ListPanelSlot60Vd_re, ListPanelSlot60Vd_orig);
SG_HOOK("golf_clean.exe", 0x0049ffc0, optionDialogSlot28_vtordisp, OptionDialogSlot28Vd_re, OptionDialogSlot28Vd_orig);
SG_HOOK("golf_clean.exe", 0x0049fff0, optionDialogSlot38_vtordisp, OptionDialogSlot38Vd_re, OptionDialogSlot38Vd_orig);
SG_HOOK("golf_clean.exe", 0x004a0000, optionDialogSlot60_vtordisp, OptionDialogSlot60Vd_re, OptionDialogSlot60Vd_orig);
SG_HOOK("golf_clean.exe", 0x0049e7b0, listPanelSlot60, ListPanelSlot60_re, ListPanelSlot60_orig);
SG_HOOK("golf_clean.exe", 0x0049fe20, optionDialogSlot28, OptionDialogSlot28_re, OptionDialogSlot28_orig);
SG_HOOK("golf_clean.exe", 0x0049fc30, optionDialogSlot38, OptionDialogSlot38_re, OptionDialogSlot38_orig);
SG_HOOK("golf_clean.exe", 0x0049fd80, optionDialogSlot60, OptionDialogSlot60_re, OptionDialogSlot60_orig);
SG_HOOK("golf_clean.exe", 0x004a0ab0, viewDispatchSlot27, ViewDispatchSlot27_re, ViewDispatchSlot27_orig);
SG_HOOK("golf_clean.exe", 0x00491980, viewDispatchSlot27_vtordisp, ViewDispatchSlot27Vd_re, ViewDispatchSlot27Vd_orig);

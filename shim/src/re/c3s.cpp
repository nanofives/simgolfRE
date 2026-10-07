// C3 batch c3s (2026-10-07) of golf_clean.exe: fourteen ui leaves and one small caller that the test scenarios never
// reach but that have static callers: the two markup token scanners, flag-word setters, child-field forwarders, the
// TextView size setter, two table searches, the click-handler setter, the bit-array helpers reached through a
// virtual-base table, and the per-kind field getter. Hand-written from the disassembly (py -3.12
// re/tools/asm2inline.py golf_clean.exe 0x<addr> --list) and the C2 transcriptions; each body cites the address of
// every offset, constant and callee it uses. __thiscall is emulated with __fastcall (ecx = this, edx unused). Every
// function returns what the original leaves in eax, so the A/B compares it. Callees are invoked through their original
// addresses, so a hooked callee still runs its own reimplementation.
#include "hooks.h"

namespace {

template <typename T> T& fieldRef(void* p, unsigned off) {
    return *reinterpret_cast<T*>(reinterpret_cast<char*>(p) + off);
}

// The offset of the virtual base: the dword at +8 of the table whose address is the object's first dword (the
// `mov ecx/eax, [this]` + `mov edx, [ecx+8]` pairs at 0x0049f038, 0x0049eff6 and 0x004a138b..0x004a138e, which reads
// the table pointer from this+4 instead).
inline unsigned vbaseOffset(const void* tablePtrField) {
    const unsigned table = *reinterpret_cast<const unsigned*>(tablePtrField);
    return *reinterpret_cast<const unsigned*>(table + 8);
}

// The six delimiters both scanners stop at (compares at 0x00476d92..0x00476dae and 0x00476de2..0x00476dfe):
// '{' 0x7b, '}' 0x7d, '[' 0x5b, ']' 0x5d, '$' 0x24, '=' 0x3d.
inline bool isMarkupDelimiter(char c) {
    return c == 0x7b || c == 0x7d || c == 0x5b || c == 0x5d || c == 0x24 || c == 0x3d;
}

// 0x00476d80  skipToken(text, len): cdecl. Advances `text` over at most *len characters, stopping at the first markup
// delimiter. When *len is 0 on entry it returns `text` and leaves *len alone (0x00476d8a / 0x00476d8e je). Otherwise
// each character is compared with the six delimiters (0x00476d90..0x00476dae); a delimiter ends the scan with the
// pointer on it, any other character advances the pointer and decrements the count (0x00476db0 / 0x00476db1), and the
// scan also ends when the count reaches 0 (0x00476db2 jne). On either end the remaining count (the delimiter included,
// or 0) is stored back to *len (0x00476db4) and the pointer is returned (0x00476dbb).
typedef char*(__cdecl* Scan_t)(char*, int*);
Scan_t SkipToken_orig;
char* __cdecl SkipToken_re(char* text, int* len) {
    int n = *len;
    if (n == 0) return text;
    do {
        if (isMarkupDelimiter(*text)) break;
        ++text;
    } while (--n != 0);
    *len = n;
    return text;
}

// 0x00476dd0  scanToken(text, len): cdecl. Same as skipToken 0x00476d80 with one more delimiter, '^' 0x5e
// (0x00476e00 / 0x00476e03): the zero-count early return (0x00476dde), the delimiter compares (0x00476de2..0x00476e03),
// the advance (0x00476e05 / 0x00476e06), the count exit (0x00476e07 jne), the store of the remaining count to *len
// (0x00476e09) and the returned pointer (0x00476e10).
Scan_t ScanToken_orig;
char* __cdecl ScanToken_re(char* text, int* len) {
    int n = *len;
    if (n == 0) return text;
    do {
        if (isMarkupDelimiter(*text) || *text == 0x5e) break;
        ++text;
    } while (--n != 0);
    *len = n;
    return text;
}

// 0x00491490  Widget::setFlagBit0(this, on): sets bit 0 of the flag dword at this+0x24 when `on` is non-zero
// (0x00491494 test / 0x00491499 je; `or al, 1` at 0x0049149b) and clears it otherwise (`and al, 0xfe` at 0x004914a3),
// storing the dword back (0x0049149d / 0x004914a5). Returns the new flag dword (left in eax).
typedef unsigned(__fastcall* SetFlag_t)(void*, void*, int);
SetFlag_t SetFlagBit0_orig;
unsigned __fastcall SetFlagBit0_re(void* self, void*, int on) {
    unsigned f = fieldRef<unsigned>(self, 0x24);
    f = on != 0 ? (f | 1u) : (f & ~1u);
    fieldRef<unsigned>(self, 0x24) = f;
    return f;
}

// 0x004914b0  Widget::setFlagBit1(this, on): as setFlagBit0 0x00491490 for bit 1: `or al, 2` at 0x004914bb when `on`
// is non-zero (0x004914b9 je), `and al, 0xfd` at 0x004914c3 otherwise; stores this+0x24 (0x004914bd / 0x004914c5) and
// returns the new dword.
SetFlag_t SetFlagBit1_orig;
unsigned __fastcall SetFlagBit1_re(void* self, void*, int on) {
    unsigned f = fieldRef<unsigned>(self, 0x24);
    f = on != 0 ? (f | 2u) : (f & ~2u);
    fieldRef<unsigned>(self, 0x24) = f;
    return f;
}

// 0x0047ba70  Window::setField26c5a0(this, v): when the child pointer at this+0x26c is non-null (0x0047ba70 /
// 0x0047ba78 je) stores v at child+0x5a0 (0x0047ba7e). Returns the child pointer (eax, NULL when absent).
typedef void*(__fastcall* SetChildField_t)(void*, void*, unsigned);
SetChildField_t SetField26c_orig;
void* __fastcall SetField26c_re(void* self, void*, unsigned v) {
    void* child = fieldRef<void*>(self, 0x26c);
    if (child != 0) fieldRef<unsigned>(child, 0x5a0) = v;
    return child;
}

// 0x0047ba90  Window::setField2705a0(this, v): as setField26c5a0 0x0047ba70 for the child at this+0x270 (0x0047ba90 /
// 0x0047ba98 je; store at child+0x5a0, 0x0047ba9e). Returns the child pointer.
SetChildField_t SetField270_orig;
void* __fastcall SetField270_re(void* self, void*, unsigned v) {
    void* child = fieldRef<void*>(self, 0x270);
    if (child != 0) fieldRef<unsigned>(child, 0x5a0) = v;
    return child;
}

// 0x0048e190  TextView::setSize(this, w, h): stores w at this+0x1fb8 unless w == 0x2000 (0x0048e194 cmp / 0x0048e199
// je; store 0x0048e19b) and h at this+0x1fbc unless h == 0x2000 (0x0048e1a5 / 0x0048e1aa; store 0x0048e1ac). Returns h
// (eax holds it from 0x0048e1a1).
typedef int(__fastcall* SetSize_t)(void*, void*, int, int);
SetSize_t SetSize_orig;
int __fastcall SetSize_re(void* self, void*, int w, int h) {
    if (w != 0x2000) fieldRef<int>(self, 0x1fb8) = w;
    if (h != 0x2000) fieldRef<int>(self, 0x1fbc) = h;
    return h;
}

// 0x004a4ad0  findMarkupTokenIndex(this, key): searches the table of 0x18-byte entries at this+0x580 (0x004a4ad7;
// stride 0x18 at 0x004a4ae9) for the first entry whose leading dword equals key and returns its index. Each entry is
// first tested against the terminator -1 (0x004a4adf / 0x004a4ae2 je), which returns -1 even when key is -1; a match
// (0x004a4ae4 / 0x004a4ae6 je) returns the index; after 0x100 entries without either (0x004a4aec / 0x004a4af1 jl,
// signed) it returns -1 (`or eax, -1` at 0x004a4af3).
typedef int(__fastcall* FindToken_t)(void*, void*, int);
FindToken_t FindToken_orig;
int __fastcall FindToken_re(void* self, void*, int key) {
    const int* entry = &fieldRef<int>(self, 0x580);
    for (int i = 0; i < 0x100; ++i, entry += 0x18 / 4) {
        if (*entry == -1) return -1;
        if (*entry == key) return i;
    }
    return -1;
}

// 0x00486330  EditBox::lineIndexAt(this, pos, starts, count): the target is the dword at this+0x574 plus pos
// (0x00486330 / 0x0048633d). Counts the leading entries of `starts` that are <= the target, compared unsigned (`cmp
// ecx, [edx]` / `jb` at 0x0048634b / 0x0048634d), stopping at the first greater entry or after count entries
// (0x00486353 / 0x00486355 jl, signed); count <= 0 reads nothing (0x00486343 / 0x00486345 jle). Returns that count
// minus 1 (0x00486357), so -1 when the first entry is already greater or count <= 0.
typedef int(__fastcall* LineIndex_t)(void*, void*, int, const unsigned*, int);
LineIndex_t LineIndex_orig;
int __fastcall LineIndex_re(void* self, void*, int pos, const unsigned* starts, int count) {
    const unsigned target = fieldRef<unsigned>(self, 0x574) + static_cast<unsigned>(pos);
    int i = 0;
    if (count > 0) {
        while (target >= starts[i]) {
            if (++i >= count) break;
        }
    }
    return i - 1;
}

// 0x00489ab0  ListModel::setClickHandler(this, target, a, b): returns 3 at once when target is NULL (0x00489ab4 test /
// 0x00489ab6 jne; `mov eax, 3` at 0x00489ab8). Otherwise stores target at this+0x74 only when the dword at target+4 is
// non-zero (0x00489ac0 / 0x00489ac5 je; store 0x00489ac7), then always stores a at this+0x78 (0x00489ad2) and b at
// this+0x7c (0x00489ad5), and returns 0 (0x00489ad8).
typedef int(__fastcall* SetClick_t)(void*, void*, void*, unsigned, unsigned);
SetClick_t SetClick_orig;
int __fastcall SetClick_re(void* self, void*, void* target, unsigned a, unsigned b) {
    if (target == 0) return 3;
    if (fieldRef<int>(target, 4) != 0) fieldRef<void*>(self, 0x74) = target;
    fieldRef<unsigned>(self, 0x78) = a;
    fieldRef<unsigned>(self, 0x7c) = b;
    return 0;
}

// The flag dword of the bit-array helpers: this + (virtual-base offset) + 0xf0 (0x0049f043 and 0x0049effb).
inline unsigned* bitWord(void* self) {
    return reinterpret_cast<unsigned*>(reinterpret_cast<char*>(self) + vbaseOffset(self) + 0xf0);
}
// `shl reg, cl` masks the shift count to its low 5 bits (0x0049f041, 0x0049f00d, 0x0049f014).
inline unsigned bitMask(int n) { return 1u << (static_cast<unsigned>(n) & 31u); }

// 0x0049f030  bitTest(this, n): returns the flag dword (see bitWord) AND (1 << n) (0x0049f033..0x0049f04a), i.e. the
// bit itself in place, not 0/1.
typedef unsigned(__fastcall* BitTest_t)(void*, void*, int);
BitTest_t BitTest_orig;
unsigned __fastcall BitTest_re(void* self, void*, int n) {
    return *bitWord(self) & bitMask(n);
}

// 0x0049eff0  bitSet(this, n, on): sets bit n of the flag dword when `on` is non-zero (0x0049eff4 test / 0x0049f00b
// je; `or [eax], edx` at 0x0049f00f) and clears it otherwise (`not` / `and` at 0x0049f018..0x0049f01c). Returns the
// address of the flag dword (eax from the `lea` at 0x0049effb, unchanged on both paths).
typedef unsigned*(__fastcall* BitSet_t)(void*, void*, int, int);
BitSet_t BitSet_orig;
unsigned* __fastcall BitSet_re(void* self, void*, int n, int on) {
    unsigned* w = bitWord(self);
    if (on != 0) *w |= bitMask(n);
    else *w &= ~bitMask(n);
    return w;
}

// 0x0049eec0  flagToggle49eec0(this, n): flips bit n: when bitTest 0x0049f030 reports it set (0x0049eec9 call /
// 0x0049eed0 je) it calls bitSet 0x0049eff0 with (n, 0) (0x0049eed7), otherwise with (n, 1) (0x0049eee6). Returns what
// bitSet returns (eax untouched after the call).
typedef unsigned*(__fastcall* Toggle_t)(void*, void*, int);
Toggle_t Toggle_orig;
const BitTest_t kBitTest = reinterpret_cast<BitTest_t>(0x0049f030);
const BitSet_t kBitSet = reinterpret_cast<BitSet_t>(0x0049eff0);
unsigned* __fastcall Toggle_re(void* self, void*, int n) {
    if (kBitTest(self, 0, n) != 0) return kBitSet(self, 0, n, 0);
    return kBitSet(self, 0, n, 1);
}

// 0x004a1370  getFieldValueByKind(this): switch on the kind at this+0x1f4 (0x004a1370). Kinds outside 1..16 return 0
// (`dec` / `cmp 0xf` / `ja` at 0x004a1376..0x004a137a, unsigned). Inside, the byte table at 0x004a13b0 (16 entries,
// read from the binary: 0,0,2,0,2,2,2,1,2,2,2,2,2,2,2,0) picks one of three targets in the jump table at 0x004a13a4:
// kinds 1, 2, 4 and 16 (entry 0, 0x004a138b) return the dword at this + (virtual-base offset read through the table
// pointer at this+4) + 0xd4 (0x004a138b..0x004a1391); kind 8 (entry 1, 0x004a1399) returns the dword at this+0x118;
// every other kind (entry 2, 0x004a13a0) returns 0.
typedef unsigned(__fastcall* ValueByKind_t)(void*, void*);
ValueByKind_t ValueByKind_orig;
unsigned __fastcall ValueByKind_re(void* self, void*) {
    switch (fieldRef<int>(self, 0x1f4)) {
        case 1: case 2: case 4: case 16:
            return fieldRef<unsigned>(self, vbaseOffset(reinterpret_cast<char*>(self) + 4) + 0xd4);
        case 8:
            return fieldRef<unsigned>(self, 0x118);
        default:
            return 0;
    }
}

}  // namespace

SG_HOOK("golf_clean.exe", 0x00476d80, skipToken, SkipToken_re, SkipToken_orig);
SG_HOOK("golf_clean.exe", 0x00476dd0, scanToken, ScanToken_re, ScanToken_orig);
SG_HOOK("golf_clean.exe", 0x00491490, Widget_setFlagBit0, SetFlagBit0_re, SetFlagBit0_orig);
SG_HOOK("golf_clean.exe", 0x004914b0, Widget_setFlagBit1, SetFlagBit1_re, SetFlagBit1_orig);
SG_HOOK("golf_clean.exe", 0x0047ba70, Window_setField26c5a0, SetField26c_re, SetField26c_orig);
SG_HOOK("golf_clean.exe", 0x0047ba90, Window_setField2705a0, SetField270_re, SetField270_orig);
SG_HOOK("golf_clean.exe", 0x0048e190, TextView_setSize, SetSize_re, SetSize_orig);
SG_HOOK("golf_clean.exe", 0x004a4ad0, findMarkupTokenIndex, FindToken_re, FindToken_orig);
SG_HOOK("golf_clean.exe", 0x00486330, EditBox_lineIndexAt, LineIndex_re, LineIndex_orig);
SG_HOOK("golf_clean.exe", 0x00489ab0, ListModel_setClickHandler, SetClick_re, SetClick_orig);
SG_HOOK("golf_clean.exe", 0x0049f030, bitTest, BitTest_re, BitTest_orig);
SG_HOOK("golf_clean.exe", 0x0049eff0, bitSet, BitSet_re, BitSet_orig);
SG_HOOK("golf_clean.exe", 0x0049eec0, flagToggle49eec0, Toggle_re, Toggle_orig);
SG_HOOK("golf_clean.exe", 0x004a1370, getFieldValueByKind, ValueByKind_re, ValueByKind_orig);

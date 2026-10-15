// FLAGS sound.dll: /O2
// LANG c
// m36a g6 (round 36): sound.dll x87 voice mixers / stream renderers matched by hand. C setup + literal __asm block
// (named with the C locals) + C tail. Spellings the match depends on (each measured with match.py):
// - the double copies before the asm are integer mov pairs: copy through 8-byte structs (AS64/I64), not double=double;
//   the double write-back in the tail is a plain double assignment (VC6 copies it with two register pairs);
// - mode-1 `fild count; fdivr [v+0x1c8]` needs a struct member read; the cast macro read gives fld/fidiv (mode 2);
// - `x = v->f5c; c = (char)~x; if (c & 1) looping = 1; else looping = 0;` keeps the dword load and the movsx;
// - restoreVal assigned once after the once/looping if-else (VC6 duplicates the load into both arms);
// - the mode-1 and mode-2 copies of v are TWO locals (voice, chan): VC6 packs them into one slot, and their split
//   weight puts the slot below cw and remain; one shared local outranks both;
// - one `return count - remain` (VC6 still emits two epilogues): with two, remain outranks cw.

#define I32(p, o) (*(int *)((char *)(p) + (o)))
#define U32(p, o) (*(unsigned int *)((char *)(p) + (o)))
#define U8(p, o) (*(unsigned char *)((char *)(p) + (o)))
#define PTR(p, o) (*(char **)((char *)(p) + (o)))
#define F64(p, o) (*(double *)((char *)(p) + (o)))
#define I64(p, o) (*(struct D8 *)((char *)(p) + (o)))
#define AS64(x) (*(struct D8 *)&(x))
struct D8 { int lo, hi; };

struct Voice3 {
    char p0[8];
    char *buf;
    char p1[0x58 - 0xc];
    int flags;
    int state;
    char p2[0x1c8 - 0x60];
    double fadeTotal;
    char p3[0x1380 - 0x1d0];
    double pos;
};

// streamRenderTail: 100% of the original's instructions once its extent is fixed. Ghidra's size is
// 859 bytes but the body ends at `ret` 0x1001892d (size 862: `mov esp, ebp; pop ebp; ret` at +0x35a..+0x35d,
// then 2 nop + int3 pad). Needs re/match/boundaries.tsv: sound.dll 0x100188d0 862. Until then 98.9%.
// MATCH: sound.dll 0x100188d0 _streamRenderTail
int __cdecl streamRenderTail(double *dst, short **psrc, int count, char *v, int mode)
{
    char *voice;
    char *chan;
    char *b;
    unsigned *pEnd;
    int flags;
    int x5c;
    char nf;
    int end;
    short *lsPtr;
    int looping;
    int chained;
    char *next;
    int wraps;
    int restoreVal;
    unsigned *psave;
    int once;
    int fadeOn;
    int idx;
    int cw;
    int remain;
    double pos;
    double rate;
    double step;
    double gainL;
    double gainR;

    step = 0.0;
    chained = 0;
    once = 0;
    fadeOn = 0;
    restoreVal = 0;
    psave = 0;
    wraps = 0;
    next = 0;
    chan = 0;
    if (mode == 1) {
        AS64(gainL) = I64(v, 0x1f0);
        AS64(gainR) = I64(v, 0x200);
        b = PTR(v, 8);
        voice = v;
        pEnd = (unsigned *)(b + 0x20);
        end = (U32(b, 0x1c) > *pEnd) ? I32(b, 0x24) : *pEnd;
        lsPtr = *(short **)(b + 0x18);
        AS64(pos) = I64(v, 0x1380);
        flags = I32(v, 0x58);
        rate = 0.36281179138321995;
        if (flags & 8) {
            fadeOn = 1;
            step = ((struct Voice3 *)v)->fadeTotal / count;
        }
        x5c = I32(v, 0x5c);
        nf = (char)~x5c;
        if (nf & 1)
            looping = 1;
        else
            looping = 0;
        if (flags & 0x20) {
            restoreVal = I32(v, 0x1378);
            psave = pEnd;
            once = 1;
        }
    } else {
        chan = v;
        gainL = 2.0;
        gainR = 2.0;
        rate = 0.36281179138321995;
        AS64(pos) = I64(v, 0x100);
        looping = ((U8(v, 0x98) & 1) && !(U8(v, 0x9c) & 2)) ? 1 : 0;
        chained = U8(v, 0x9c) & 1;
        if (chained) {
            next = PTR(v, 0xb4);
            lsPtr = *(short **)(next + 0x18);
            end = I32(v, 0x20);
        } else if (looping) {
            lsPtr = *(short **)(v + 0xd8);
            end = I32(v, 0xd4);
        } else {
            lsPtr = *(short **)(v + 0x18);
            end = I32(v, 0x20);
        }
    }
    cw = 0xf7f;
    __asm {
        push esi
        push edi
        push ebx
        push ecx
        push edx
        mov ecx, count
        mov edx, psrc
        mov esi, dword ptr [edx]
        mov edi, dst
        fninit
        fldcw word ptr cw
        fld qword ptr gainR
        fld qword ptr gainL
        fld qword ptr rate
        fld qword ptr pos
    L8a76:
        fadd st, st(1)
        fld st(0)
        fist dword ptr idx
        mov eax, idx
        shl eax, 1
        add esi, eax
        fisub dword ptr idx
        cmp esi, end
        jge L8add
        fild word ptr [esi]
    L8a8e:
        add esi, 2
        cmp esi, end
        jge L8b73
        fild word ptr [esi]
    L8a9c:
        fsub st, st(1)
        fmul st, st(2)
        fadd st, st(1)
        fxch st(1)
        fstp st(0)
        fld st(0)
        fmul st, st(5)
        fxch st(1)
        fmul st, st(6)
        fxch st(1)
        fadd qword ptr [edi]
        fxch st(1)
        fadd qword ptr [edi + 8]
        fxch st(1)
        fstp qword ptr [edi]
        fstp qword ptr [edi + 8]
        fstp st(0)
        dec ecx
        je L8b97
        add edi, 0x10
        mov esi, dword ptr [edx]
        cmp fadeOn, 0
        jne L8ad4
        jmp L8a76
    L8ad4:
        fld qword ptr step
        fsub st(3), st
        fsubp st(4), st
        jmp L8a76
    L8add:
        cmp chained, 0
        jne L8b19
        cmp looping, 0
        je L8b97
        fxch st(1)
        fstp st(0)
        fld st(0)
        fist dword ptr idx
        mov eax, lsPtr
        mov dword ptr [edx], eax
        mov esi, eax
        fild word ptr [esi]
        cmp once, 0
        je L8a8e
        mov looping, 0
        mov eax, psave
        mov ebx, restoreVal
        mov dword ptr [eax], ebx
        jmp L8a8e
    L8b19:
        inc wraps
        fxch st(1)
        fstp st(0)
        fld st(0)
        fist dword ptr idx
        mov esi, lsPtr
        mov dword ptr [edx], esi
        fild word ptr [esi]
        push edx
        mov eax, next
        mov edx, dword ptr [eax + 0xd4]
        mov end, edx
        mov edx, dword ptr [eax + 0xb4]
        mov next, edx
        cmp edx, 0
        je L8b56
        mov eax, dword ptr [edx + 0xd8]
        mov lsPtr, eax
        pop edx
        jmp L8a8e
    L8b56:
        mov chained, 0
        mov looping, 0
        mov eax, dword ptr [eax + 0xd8]
        pop edx
        jmp L8a8e
        fist dword ptr idx
    L8b73:
        mov word ptr [v + 2], 0
        cmp chained, 0
        jne L8b85
        cmp looping, 0
        je L8b8f
    L8b85:
        mov eax, lsPtr
        mov bx, word ptr [eax]
        mov word ptr [v + 2], bx
    L8b8f:
        fild word ptr [v + 2]
        jmp L8a9c
    L8b97:
        cmp ecx, 0
        jne L8ba5
        sub esi, 2
        fisub dword ptr idx
        fst qword ptr pos
    L8ba5:
        mov remain, ecx
        mov edx, psrc
        mov dword ptr [edx], esi
        pop edx
        pop ecx
        pop ebx
        pop edi
        pop esi
    }
    if (mode == 1) {
        F64(voice, 0x1380) = pos;
        if ((U8(voice, 0x58) & 0x20) && !looping)
            I32(voice, 0x5c) |= 1;
    } else {
        F64(chan, 0x100) = pos;
        I32(chan, 0xcc) += wraps;
    }
    return count - remain;
}

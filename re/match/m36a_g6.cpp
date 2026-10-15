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

// MATCH: sound.dll 0x10019770 _Voice_mixAddMono16
int __cdecl Voice_mixAddMono16(double *dst, short **psrc, int count, char *v, int mode)
{
    char *voice;
    char *chan;
    char *b;
    int *pEnd;
    int flags;
    unsigned char fl;
    int x5c;
    char nf;
    int looping;
    int end;
    int fadeOn;
    int restoreVal;
    short *lsPtr;
    unsigned *psave;
    int once;
    int idx;
    int remain;

    int cw;
    double pos;
    double step;
    double rate;
    double gainL;
    double gainR;

    step = 0.0;
    once = 0;
    fadeOn = 0;
    restoreVal = 0;
    psave = 0;
    if (mode == 1) {
        AS64(gainL) = I64(v, 0x1f0);
        AS64(gainR) = I64(v, 0x200);
        voice = v;
        b = PTR(v, 8);
        pEnd = (int *)(b + 0x20);
        end = (U32(b, 0x1c) > U32(b, 0x20)) ? I32(b, 0x24) : I32(b, 0x20);
        lsPtr = *(short **)(b + 0x18);
        AS64(pos) = I64(v, 0x1380);
        AS64(rate) = I64(v, 0x1388);
        flags = I32(v, 0x58);
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
            psave = (unsigned *)pEnd;
            if ((unsigned)*psrc > *psave)
                once = 1;
            else
                looping = 0;
            restoreVal = I32(v, 0x1378);
        }
    } else {
        chan = v;
        AS64(gainL) = I64(v, 0x80);
        AS64(gainR) = I64(v, 0x70);
        AS64(rate) = I64(v, 0x108);
        AS64(pos) = I64(v, 0x100);
        fl = U8(v, 0x9c);
        if (fl & 8) {
            step = F64(v, 0xf0) / count;
            fadeOn = 1;
        }
        if ((U8(v, 0x98) & 1) && !(fl & 2)) {
            looping = 1;
            lsPtr = *(short **)(v + 0xd8);
            end = I32(v, 0xd4);
        } else {
            looping = 0;
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
    L9920:
        fadd st, st(1)
        fld st(0)
        fist dword ptr idx
        mov eax, idx
        shl eax, 1
        add esi, eax
        fisub dword ptr idx
        cmp esi, end
        jge L997f
        fild word ptr [esi]
    L9938:
        add esi, 2
        cmp esi, end
        jge L99ae
        fild word ptr [esi]
    L9942:
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
        je L99cc
        cmp fadeOn, 0
        add edi, 0x10
        mov esi, dword ptr [edx]
        jne L9976
        jmp L9920
    L9976:
        fld qword ptr step
        fsub st(3), st
        fsubp st(4), st
        jmp L9920
    L997f:
        cmp looping, 0
        je L99cc
        fxch st(1)
        fstp st(0)
        fld st(0)
        fist dword ptr idx
        mov eax, lsPtr
        mov dword ptr [edx], eax
        mov esi, eax
        fild word ptr [esi]
        cmp once, 0
        je L9938
        mov looping, 0
        mov eax, psave
        mov ebx, restoreVal
        mov dword ptr [eax], ebx
        jmp L9938
    L99ae:
        mov word ptr [v + 2], 0
        cmp looping, 0
        je L99c4
        mov eax, lsPtr
        mov bx, word ptr [eax]
        mov word ptr [v + 2], bx
    L99c4:
        fild word ptr [v + 2]
        jmp L9942
    L99cc:
        cmp ecx, 0
        jne L99da
        sub esi, 2
        fisub dword ptr idx
        fst qword ptr pos
    L99da:
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
        ((struct Voice3 *)voice)->pos = pos;
        if ((((struct Voice3 *)voice)->flags & 0x20) && !looping) {
            ((struct Voice3 *)voice)->state |= 1;
        }
    } else {
        F64(chan, 0x100) = pos;
    }
    return count - remain;
}

// MATCH: sound.dll 0x10019b20 _Voice_mixSetMono16
int __cdecl Voice_mixSetMono16(double *dst, short **psrc, int count, char *v, int mode)
{
    char *voice;
    char *chan;
    char *b;
    int *pEnd;
    int flags;
    unsigned char fl;
    int x5c;
    char nf;
    int looping;
    int end;
    int fadeOn;
    int restoreVal;
    short *lsPtr;
    unsigned *psave;
    int once;
    int idx;
    int remain;

    int cw;
    double pos;
    double step;
    double rate;
    double gainL;
    double gainR;

    step = 0.0;
    once = 0;
    fadeOn = 0;
    restoreVal = 0;
    psave = 0;
    if (mode == 1) {
        AS64(gainL) = I64(v, 0x1f0);
        AS64(gainR) = I64(v, 0x200);
        voice = v;
        b = PTR(v, 8);
        pEnd = (int *)(b + 0x20);
        end = (U32(b, 0x1c) > U32(b, 0x20)) ? I32(b, 0x24) : I32(b, 0x20);
        lsPtr = *(short **)(b + 0x18);
        AS64(pos) = I64(v, 0x1380);
        AS64(rate) = I64(v, 0x1388);
        flags = I32(v, 0x58);
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
            psave = (unsigned *)pEnd;
            if ((unsigned)*psrc > *psave)
                once = 1;
            else
                looping = 0;
            restoreVal = I32(v, 0x1378);
        }
    } else {
        chan = v;
        AS64(gainL) = I64(v, 0x80);
        AS64(gainR) = I64(v, 0x70);
        AS64(rate) = I64(v, 0x108);
        AS64(pos) = I64(v, 0x100);
        fl = U8(v, 0x9c);
        if (fl & 8) {
            step = F64(v, 0xf0) / count;
            fadeOn = 1;
        }
        if ((U8(v, 0x98) & 1) && !(fl & 2)) {
            looping = 1;
            lsPtr = *(short **)(v + 0xd8);
            end = I32(v, 0xd4);
        } else {
            looping = 0;
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
    L9920:
        fadd st, st(1)
        fld st(0)
        fist dword ptr idx
        mov eax, idx
        shl eax, 1
        add esi, eax
        fisub dword ptr idx
        cmp esi, end
        jge L997f
        fild word ptr [esi]
    L9938:
        add esi, 2
        cmp esi, end
        jge L99ae
        fild word ptr [esi]
    L9942:
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
        fstp qword ptr [edi]
        fstp qword ptr [edi + 8]
        fstp st(0)
        dec ecx
        je L99cc
        cmp fadeOn, 0
        add edi, 0x10
        mov esi, dword ptr [edx]
        jne L9976
        jmp L9920
    L9976:
        fld qword ptr step
        fsub st(3), st
        fsubp st(4), st
        jmp L9920
    L997f:
        cmp looping, 0
        je L99cc
        fxch st(1)
        fstp st(0)
        fld st(0)
        fist dword ptr idx
        mov eax, lsPtr
        mov dword ptr [edx], eax
        mov esi, eax
        fild word ptr [esi]
        cmp once, 0
        je L9938
        mov looping, 0
        mov eax, psave
        mov ebx, restoreVal
        mov dword ptr [eax], ebx
        jmp L9938
    L99ae:
        mov word ptr [v + 2], 0
        cmp looping, 0
        je L99c4
        mov eax, lsPtr
        mov bx, word ptr [eax]
        mov word ptr [v + 2], bx
    L99c4:
        fild word ptr [v + 2]
        jmp L9942
    L99cc:
        cmp ecx, 0
        jne L99da
        sub esi, 2
        fisub dword ptr idx
        fst qword ptr pos
    L99da:
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
        ((struct Voice3 *)voice)->pos = pos;
        if ((((struct Voice3 *)voice)->flags & 0x20) && !looping) {
            ((struct Voice3 *)voice)->state |= 1;
        }
    } else {
        F64(chan, 0x100) = pos;
    }
    return count - remain;
}

// MATCH: sound.dll 0x10019ec0 _Voice_mixSet8bit
int __cdecl Voice_mixSet8bit(double *dst, unsigned char **psrc, int count, char *v, int mode)
{
    char *voice;
    char *chan;
    char *b;
    int *pEnd;
    int flags;
    unsigned char fl;
    int x5c;
    char nf;
    int looping;
    int end;
    int fadeOn;
    int restoreVal;
    unsigned char *lsPtr;
    unsigned *psave;
    int once;
    int idx;
    int smp;
    int remain;

    int cw;
    double pos;
    double step;
    double rate;
    double gainL;
    double gainR;

    step = 0.0;
    once = 0;
    fadeOn = 0;
    restoreVal = 0;
    psave = 0;
    smp = 0;
    if (mode == 1) {
        AS64(gainL) = I64(v, 0x1f0);
        AS64(gainR) = I64(v, 0x200);
        voice = v;
        b = PTR(v, 8);
        pEnd = (int *)(b + 0x20);
        end = (U32(b, 0x1c) > U32(b, 0x20)) ? I32(b, 0x24) : I32(b, 0x20);
        lsPtr = *(unsigned char **)(b + 0x18);
        AS64(pos) = I64(v, 0x1380);
        AS64(rate) = I64(v, 0x1388);
        flags = I32(v, 0x58);
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
            psave = (unsigned *)pEnd;
            if ((unsigned)*psrc > *psave)
                once = 1;
            else
                looping = 0;
            restoreVal = I32(v, 0x1378);
        }
    } else {
        chan = v;
        AS64(gainL) = I64(v, 0x80);
        AS64(gainR) = I64(v, 0x70);
        AS64(rate) = I64(v, 0x108);
        AS64(pos) = I64(v, 0x100);
        fl = U8(v, 0x9c);
        if (fl & 8) {
            step = F64(v, 0xf0) / count;
            fadeOn = 1;
        }
        if ((U8(v, 0x98) & 1) && !(fl & 2)) {
            looping = 1;
            lsPtr = *(unsigned char **)(v + 0xd8);
            end = I32(v, 0xd4);
        } else {
            looping = 0;
            lsPtr = *(unsigned char **)(v + 0x18);
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
    La073:
        fadd st, st(1)
        fld st(0)
        fist dword ptr idx
        mov eax, idx
        add esi, eax
        fisub dword ptr idx
        cmp esi, end
        jge La0f1
        movzx eax, byte ptr [esi]
        shl eax, 8
        sub eax, 0x8000
        mov word ptr smp, ax
        fild word ptr smp
    La099:
        inc esi
        cmp esi, end
        jge La137
        movzx eax, byte ptr [esi]
        shl eax, 8
        sub eax, 0x8000
        mov word ptr smp, ax
        mov word ptr smp, ax
        fild word ptr smp
    La0b9:
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
        fstp qword ptr [edi]
        fstp qword ptr [edi + 8]
        fstp st(0)
        dec ecx
        je La15e
        cmp fadeOn, 0
        add edi, 0x10
        mov esi, dword ptr [edx]
        jne La0e8
        jmp La073
    La0e8:
        fld qword ptr step
        fsub st(3), st
        fsubp st(4), st
        jmp La073
    La0f1:
        cmp looping, 0
        je La15e
        fxch st(1)
        fstp st(0)
        fld st(0)
        fist dword ptr idx
        mov eax, lsPtr
        mov dword ptr [edx], eax
        mov esi, eax
        movzx eax, byte ptr [esi]
        shl eax, 8
        sub eax, 0x8000
        mov word ptr smp, ax
        fild word ptr smp
        cmp once, 0
        je La099
        mov looping, 0
        mov eax, psave
        mov ebx, restoreVal
        mov dword ptr [eax], ebx
        jmp La099
    La137:
        mov word ptr [v + 2], 0
        cmp looping, 0
        je La156
        mov eax, lsPtr
        movzx ebx, byte ptr [eax]
        shl ebx, 8
        sub ebx, 0x8000
        mov word ptr [v + 2], bx
    La156:
        fild word ptr [v + 2]
        jmp La0b9
    La15e:
        cmp ecx, 0
        jne La16c
        sub esi, 1
        fisub dword ptr idx
        fst qword ptr pos
    La16c:
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
        ((struct Voice3 *)voice)->pos = pos;
        if ((((struct Voice3 *)voice)->flags & 0x20) && !looping) {
            ((struct Voice3 *)voice)->state |= 1;
        }
    } else {
        F64(chan, 0x100) = pos;
    }
    return count - remain;
}

// MATCH: sound.dll 0x1001a2c0 _Voice_mixAdd8bit
int __cdecl Voice_mixAdd8bit(double *dst, unsigned char **psrc, int count, char *v, int mode)
{
    char *voice;
    char *chan;
    char *b;
    int *pEnd;
    int flags;
    unsigned char fl;
    int x5c;
    char nf;
    int looping;
    int end;
    int fadeOn;
    int restoreVal;
    unsigned char *lsPtr;
    unsigned *psave;
    int once;
    int idx;
    int smp;
    int remain;

    int cw;
    double pos;
    double step;
    double rate;
    double gainL;
    double gainR;

    step = 0.0;
    once = 0;
    fadeOn = 0;
    restoreVal = 0;
    psave = 0;
    smp = 0;
    if (mode == 1) {
        AS64(gainL) = I64(v, 0x1f0);
        AS64(gainR) = I64(v, 0x200);
        voice = v;
        b = PTR(v, 8);
        pEnd = (int *)(b + 0x20);
        end = (U32(b, 0x1c) > U32(b, 0x20)) ? I32(b, 0x24) : I32(b, 0x20);
        lsPtr = *(unsigned char **)(b + 0x18);
        AS64(pos) = I64(v, 0x1380);
        AS64(rate) = I64(v, 0x1388);
        flags = I32(v, 0x58);
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
            psave = (unsigned *)pEnd;
            if ((unsigned)*psrc > *psave)
                once = 1;
            else
                looping = 0;
            restoreVal = I32(v, 0x1378);
        }
    } else {
        chan = v;
        AS64(gainL) = I64(v, 0x80);
        AS64(gainR) = I64(v, 0x70);
        AS64(rate) = I64(v, 0x108);
        AS64(pos) = I64(v, 0x100);
        fl = U8(v, 0x9c);
        if (fl & 8) {
            step = F64(v, 0xf0) / count;
            fadeOn = 1;
        }
        if ((U8(v, 0x98) & 1) && !(fl & 2)) {
            looping = 1;
            lsPtr = *(unsigned char **)(v + 0xd8);
            end = I32(v, 0xd4);
        } else {
            looping = 0;
            lsPtr = *(unsigned char **)(v + 0x18);
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
    La473:
        fadd st, st(1)
        fld st(0)
        fist dword ptr idx
        mov eax, idx
        add esi, eax
        fisub dword ptr idx
        cmp esi, end
        jge La4fd
        movzx eax, byte ptr [esi]
        shl eax, 8
        sub eax, 0x8000
        mov word ptr smp, ax
        fild word ptr smp
    La499:
        inc esi
        cmp esi, end
        jge La543
        movzx eax, byte ptr [esi]
        shl eax, 8
        sub eax, 0x8000
        mov word ptr smp, ax
        mov word ptr smp, ax
        fild word ptr smp
    La4b9:
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
        je La56a
        cmp fadeOn, 0
        add edi, 0x10
        mov esi, dword ptr [edx]
        jne La4f1
        jmp La473
    La4f1:
        fld qword ptr step
        fsub st(3), st
        fsubp st(4), st
        jmp La473
    La4fd:
        cmp looping, 0
        je La56a
        fxch st(1)
        fstp st(0)
        fld st(0)
        fist dword ptr idx
        mov eax, lsPtr
        mov dword ptr [edx], eax
        mov esi, eax
        movzx eax, byte ptr [esi]
        shl eax, 8
        sub eax, 0x8000
        mov word ptr smp, ax
        fild word ptr smp
        cmp once, 0
        je La499
        mov looping, 0
        mov eax, psave
        mov ebx, restoreVal
        mov dword ptr [eax], ebx
        jmp La499
    La543:
        mov word ptr [v + 2], 0
        cmp looping, 0
        je La562
        mov eax, lsPtr
        movzx ebx, byte ptr [eax]
        shl ebx, 8
        sub ebx, 0x8000
        mov word ptr [v + 2], bx
    La562:
        fild word ptr [v + 2]
        jmp La4b9
    La56a:
        cmp ecx, 0
        jne La578
        sub esi, 1
        fisub dword ptr idx
        fst qword ptr pos
    La578:
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
        ((struct Voice3 *)voice)->pos = pos;
        if ((((struct Voice3 *)voice)->flags & 0x20) && !looping) {
            ((struct Voice3 *)voice)->state |= 1;
        }
    } else {
        F64(chan, 0x100) = pos;
    }
    return count - remain;
}

// MATCH: sound.dll 0x1001a6c0 _Voice_mixSetStereo16
int __cdecl Voice_mixSetStereo16(double *dst, short **psrc, int count, char *v, int mode)
{
    char *voice;
    char *b;
    int flags;
    unsigned char fl;
    int x5c;
    char nf;
    int looping;
    int end;
    int fadeOn;
    short *lsPtr;
    int once;
    int remain;
    int cw;
    double step;
    double gainL;
    double gainR;

    step = 0.0;
    once = 0;
    fadeOn = 0;
    if (mode == 1) {
        AS64(gainL) = I64(v, 0x1f0);
        AS64(gainR) = I64(v, 0x200);
        voice = v;
        b = PTR(v, 8);
        if (U32(b, 0x1c) > U32(b, 0x20))
            end = I32(b, 0x24);
        else
            end = I32(b, 0x20);
        lsPtr = *(short **)(b + 0x18);
        flags = I32(v, 0x58);
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
            if ((unsigned)*psrc > U32(b, 0x20))
                once = 1;
            else
                looping = 0;
        }
    } else {
        AS64(gainL) = I64(v, 0x80);
        AS64(gainR) = I64(v, 0x70);
        fl = U8(v, 0x9c);
        if (fl & 8) {
            step = F64(v, 0xf0) / count;
            fadeOn = 1;
        }
        if ((U8(v, 0x98) & 1) && !(fl & 2)) {
            looping = 1;
            lsPtr = *(short **)(v + 0xd8);
            end = I32(v, 0xd4);
        } else {
            looping = 0;
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
    La7fa:
        cmp esi, end
        jge La82d
        fild word ptr [esi]
        fild word ptr [esi + 2]
        fxch st(1)
        fmul st, st(2)
        fxch st(1)
        fmul st, st(3)
        fxch st(1)
        fstp qword ptr [edi]
        fstp qword ptr [edi + 8]
        dec ecx
        je La849
        add edi, 0x10
        add esi, 4
        cmp fadeOn, 0
        jne La824
        jmp La7fa
    La824:
        fld qword ptr step
        fsub st(1), st
        fsubp st(2), st
        jmp La7fa
    La82d:
        cmp looping, 0
        je La849
        mov eax, lsPtr
        mov dword ptr [edx], eax
        mov esi, eax
        cmp once, 0
        je La7fa
        mov looping, 0
        jmp La7fa
    La849:
        mov remain, ecx
        mov edx, psrc
        mov dword ptr [edx], esi
        pop edx
        pop ecx
        pop ebx
        pop edi
        pop esi
    }
    if (mode == 1 && (U8(voice, 0x58) & 0x20) && !looping)
        I32(voice, 0x5c) |= 1;
    return count - remain;
}

// MATCH: sound.dll 0x1001a910 _Voice_mixAddStereo16
int __cdecl Voice_mixAddStereo16(double *dst, short **psrc, int count, char *v, int mode)
{
    char *voice;
    char *b;
    int flags;
    unsigned char fl;
    int x5c;
    char nf;
    int looping;
    int end;
    int fadeOn;
    short *lsPtr;
    int once;
    int remain;
    int cw;
    double step;
    double gainL;
    double gainR;

    step = 0.0;
    once = 0;
    fadeOn = 0;
    if (mode == 1) {
        AS64(gainL) = I64(v, 0x1f0);
        AS64(gainR) = I64(v, 0x200);
        voice = v;
        b = PTR(v, 8);
        if (U32(b, 0x1c) > U32(b, 0x20))
            end = I32(b, 0x24);
        else
            end = I32(b, 0x20);
        lsPtr = *(short **)(b + 0x18);
        flags = I32(v, 0x58);
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
            if ((unsigned)*psrc > U32(b, 0x20))
                once = 1;
            else
                looping = 0;
        }
    } else {
        AS64(gainL) = I64(v, 0x80);
        AS64(gainR) = I64(v, 0x70);
        fl = U8(v, 0x9c);
        if (fl & 8) {
            step = F64(v, 0xf0) / count;
            fadeOn = 1;
        }
        if ((U8(v, 0x98) & 1) && !(fl & 2)) {
            looping = 1;
            lsPtr = *(short **)(v + 0xd8);
            end = I32(v, 0xd4);
        } else {
            looping = 0;
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
    Laa4a:
        cmp esi, end
        jge Laa86
        fild word ptr [esi]
        fild word ptr [esi + 2]
        fxch st(1)
        fmul st, st(2)
        fxch st(1)
        fmul st, st(3)
        fxch st(1)
        fadd qword ptr [edi]
        fxch st(1)
        fadd qword ptr [edi + 8]
        fxch st(1)
        fstp qword ptr [edi]
        fstp qword ptr [edi + 8]
        dec ecx
        je Laaa2
        add edi, 0x10
        add esi, 4
        cmp fadeOn, 0
        jne Laa7d
        jmp Laa4a
    Laa7d:
        fld qword ptr step
        fsub st(1), st
        fsubp st(2), st
        jmp Laa4a
    Laa86:
        cmp looping, 0
        je Laaa2
        mov eax, lsPtr
        mov dword ptr [edx], eax
        mov esi, eax
        cmp once, 0
        je Laa4a
        mov looping, 0
        jmp Laa4a
    Laaa2:
        mov remain, ecx
        mov edx, psrc
        mov dword ptr [edx], esi
        pop edx
        pop ecx
        pop ebx
        pop edi
        pop esi
    }
    if (mode == 1 && (U8(voice, 0x58) & 0x20) && !looping)
        I32(voice, 0x5c) |= 1;
    return count - remain;
}

// MATCH: sound.dll 0x100184a0 _streamRenderBlock
int __cdecl streamRenderBlock(double *dst, short **psrc, int count, char *v, int mode)
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
        b = PTR(v, 8);
        voice = v;
        gainL = 2.0;
        gainR = 2.0;
        pEnd = (unsigned *)(b + 0x20);
        if (U32(b, 0x1c) > U32(b, 0x20))
            end = I32(b, 0x24);
        else
            end = I32(b, 0x20);
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
        AS64(gainL) = I64(v, 0x80);
        AS64(gainR) = I64(v, 0x70);
        chan = v;
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
    L8639:
        fadd st, st(1)
        fld st(0)
        fist dword ptr idx
        mov eax, idx
        shl eax, 1
        add esi, eax
        fisub dword ptr idx
        cmp esi, end
        jge L8697
        fild word ptr [esi]
    L8651:
        add esi, 2
        cmp esi, end
        jge L872b
        fild word ptr [esi]
    L865f:
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
        fstp qword ptr [edi]
        fstp qword ptr [edi + 8]
        fstp st(0)
        dec ecx
        je L874f
        cmp fadeOn, 0
        add edi, 0x10
        mov esi, dword ptr [edx]
        jne L868e
        jmp L8639
    L868e:
        fld qword ptr step
        fsub st(3), st
        fsubp st(4), st
        jmp L8639
    L8697:
        cmp chained, 0
        jne L86d0
        cmp looping, 0
        je L874f
        fxch st(1)
        fstp st(0)
        fld st(0)
        fist dword ptr idx
        mov eax, lsPtr
        mov dword ptr [edx], eax
        mov esi, eax
        fild word ptr [esi]
        cmp once, 0
        je L8651
        mov looping, 0
        mov eax, psave
        mov ebx, restoreVal
        mov dword ptr [eax], ebx
        jmp L8651
    L86d0:
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
        je L870d
        mov eax, dword ptr [edx + 0xd8]
        mov lsPtr, eax
        pop edx
        jmp L8651
    L870d:
        mov chained, 0
        mov looping, 0
        mov eax, dword ptr [eax + 0xd8]
        pop edx
        jne L8651
        fist dword ptr idx
    L872b:
        mov word ptr [v + 2], 0
        cmp chained, 0
        jne L873d
        cmp looping, 0
        je L8747
    L873d:
        mov eax, lsPtr
        mov bx, word ptr [eax]
        mov word ptr [v + 2], bx
    L8747:
        fild word ptr [v + 2]
        jmp L865f
    L874f:
        cmp ecx, 0
        jne L875d
        sub esi, 2
        fisub dword ptr idx
        fst qword ptr pos
    L875d:
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

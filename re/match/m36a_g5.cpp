// FLAGS sound.dll: /O2
// m36a g5 (round 36): sound.dll x87-era mixer bodies matched by hand. Each is C around a literal __asm block
// (the original pushes esi/edi/ebx/ecx/edx mid-body); the asm operands name the C locals so VC6 gives them the
// original's frame slots (declaration order = slot order from [ebp-4] down).

#define I32(p, o) (*(int *)((char *)(p) + (o)))
#define U32(p, o) (*(unsigned int *)((char *)(p) + (o)))
#define U8(p, o) (*(unsigned char *)((char *)(p) + (o)))
#define PTR(p, o) (*(char **)((char *)(p) + (o)))

// MATCH: sound.dll 0x10013890 ?mixStereoAdd@@YAHPAPAHPAPAFHPADH@Z
int __cdecl mixStereoAdd(int **pdst, short **psrc, int count, char *v, int mode)
{
    int end;
    short *loopStart;
    int looping;
    int wraps;
    int chained;
    char *next;
    int remain;
    int carry;

    looping = 0;
    chained = 0;
    wraps = 0;
    next = 0;
    if (mode == 1) {
        char *b = PTR(v, 8);
        end = (U32(b, 0x1c) > U32(b, 0x20)) ? I32(b, 0x24) : I32(b, 0x20);
        loopStart = *(short **)(b + 0x18);
    } else {
        looping = ((U8(v, 0x98) & 1) && !(U8(v, 0x9c) & 2)) ? 1 : 0;
        chained = U8(v, 0x9c) & 1;
        if (chained) {
            next = PTR(v, 0xb4);
            loopStart = *(short **)(next + 0x18);
            end = I32(v, 0x20);
        } else if (looping) {
            loopStart = *(short **)(v + 0xd8);
            end = I32(v, 0xd4);
        } else {
            loopStart = *(short **)(v + 0x18);
            end = I32(v, 0x20);
        }
    }
    __asm {
        push esi
        push edi
        push ebx
        push ecx
        push edx
        mov ecx, count
        mov edx, psrc
        mov esi, dword ptr [edx]
        mov edx, pdst
        mov edi, dword ptr [edx]
    L3939:
        movsx eax, word ptr [esi]
        shl eax, 0x10
        add dword ptr [edi], eax
        jo L3948
        add edi, 4
        jmp L395d
    L3948:
        jb L3953
        mov eax, 0x7fffffff
        mov dword ptr [edi], eax
        jmp L395a
    L3953:
        mov eax, 0x80000000
        mov dword ptr [edi], eax
    L395a:
        add edi, 4
    L395d:
        movsx eax, word ptr [esi + 2]
        shl eax, 0x10
        add dword ptr [edi], eax
        jo L396d
        add edi, 4
        jmp L3982
    L396d:
        jb L3978
        mov eax, 0x7fffffff
        mov dword ptr [edi], eax
        jmp L397f
    L3978:
        mov eax, 0x80000000
        mov dword ptr [edi], eax
    L397f:
        add edi, 4
    L3982:
        add esi, 4
        cmp esi, end
        jge L3992
        dec ecx
        jne L3939
        jmp L3a13
    L3992:
        dec ecx
        cmp chained, 0
        jne L39b6
        cmp looping, 0
        je L3a13
        inc wraps
        mov carry, 0
        mov eax, psrc
        mov esi, loopStart
        mov dword ptr [eax], esi
        cmp ecx, 0
        jne L3939
    L39b6:
        inc wraps
        mov carry, 0
        mov eax, psrc
        mov esi, loopStart
        mov dword ptr [eax], esi
        mov eax, next
        mov edx, dword ptr [eax + 0xd4]
        mov end, edx
        mov edx, dword ptr [eax + 0xb4]
        mov next, edx
        cmp edx, 0
        je L39f6
        mov eax, dword ptr [edx + 0xd8]
        mov loopStart, eax
        cmp ecx, 0
        jne L3939
        jmp L3a13
    L39f6:
        mov chained, 0
        mov looping, 0
        mov edx, dword ptr [eax + 0xd8]
        cmp ecx, 0
        jne L3939
    L3a13:
        mov remain, ecx
        mov edx, psrc
        mov dword ptr [edx], esi
        mov edx, pdst
        mov dword ptr [edx], edi
        pop edx
        pop ecx
        pop ebx
        pop edi
        pop esi
    }
    int done = count - remain;
    if (mode == 1) {
        I32(v, 0x16c) += done;
        I32(v, 0x1360) += wraps;
    } else {
        I32(v, 0x88) += done;
        I32(v, 0xcc) += wraps;
    }
    return done;
}

// MATCH: sound.dll 0x10013b10 ?mixStereoGainAdd@@YAHPAPAHPAPAFHPADH@Z
int __cdecl mixStereoGainAdd(int **pdst, short **psrc, int count, char *v, int mode)
{
    int end;
    short *loopStart;
    int looping;
    int wraps;
    int chained;
    char *next;
    int gainR;
    int gainL;
    int remain;
    int carry;

    looping = 0;
    chained = 0;
    wraps = 0;
    next = 0;
    if (mode == 1) {
        gainL = I32(v, 0x1e8);
        gainR = I32(v, 0x1f8);
        char *b = PTR(v, 8);
        end = (U32(b, 0x1c) > U32(b, 0x20)) ? I32(b, 0x24) : I32(b, 0x20);
        loopStart = *(short **)(b + 0x18);
    } else {
        gainL = I32(v, 0x78);
        gainR = I32(v, 0x68);
        looping = ((U8(v, 0x98) & 1) && !(U8(v, 0x9c) & 2)) ? 1 : 0;
        chained = U8(v, 0x9c) & 1;
        if (chained) {
            next = PTR(v, 0xb4);
            loopStart = *(short **)(next + 0x18);
            end = I32(v, 0x20);
            goto asmblk;
        } else if (looping) {
            loopStart = *(short **)(v + 0xd8);
            end = I32(v, 0xd4);
        } else {
            loopStart = *(short **)(v + 0x18);
            end = I32(v, 0x20);
        }
    }
asmblk:
    // asm 0xc0..0x1c5
    __asm {
        push esi
        push edi
        push ebx
        push ecx
        push edx
        mov ecx, count
        mov edx, psrc
        mov esi, dword ptr [edx]
        mov edx, pdst
        mov edi, dword ptr [edx]
    L3be2:
        movsx eax, word ptr [esi]
        mov edx, gainL
        imul edx
        add dword ptr [edi], eax
        jo L3bf3
        add edi, 4
        jmp L3c08
    L3bf3:
        jb L3bfe
        mov eax, 0x7fffffff
        mov dword ptr [edi], eax
        jmp L3c05
    L3bfe:
        mov eax, 0x80000000
        mov dword ptr [edi], eax
    L3c05:
        add edi, 4
    L3c08:
        movsx eax, word ptr [esi + 2]
        mov edx, gainR
        imul edx
        add dword ptr [edi], eax
        jo L3c1a
        add edi, 4
        jmp L3c2f
    L3c1a:
        jb L3c25
        mov eax, 0x7fffffff
        mov dword ptr [edi], eax
        jmp L3c2c
    L3c25:
        mov eax, 0x80000000
        mov dword ptr [edi], eax
    L3c2c:
        add edi, 4
    L3c2f:
        add esi, 4
        cmp esi, end
        jge L3c3f
        dec ecx
        jne L3be2
        jmp L3cc4
    L3c3f:
        dec ecx
        cmp chained, 0
        jne L3c67
        cmp looping, 0
        je L3cc4
        inc wraps
        mov dword ptr [ebx + 4], 0
        mov eax, psrc
        mov esi, loopStart
        mov dword ptr [eax], esi
        cmp ecx, 0
        jne L3be2
    L3c67:
        inc wraps
        mov carry, 0
        mov eax, psrc
        mov esi, loopStart
        mov dword ptr [eax], esi
        mov eax, next
        mov edx, dword ptr [eax + 0xd4]
        mov end, edx
        mov edx, dword ptr [eax + 0xb4]
        mov next, edx
        cmp edx, 0
        je L3ca7
        mov eax, dword ptr [edx + 0xd8]
        mov loopStart, eax
        cmp ecx, 0
        jne L3be2
        jmp L3cc4
    L3ca7:
        mov chained, 0
        mov looping, 0
        mov edx, dword ptr [eax + 0xd8]
        cmp ecx, 0
        jne L3be2
    L3cc4:
        mov remain, ecx
        mov edx, psrc
        mov dword ptr [edx], esi
        mov edx, pdst
        mov dword ptr [edx], edi
        pop edx
        pop ecx
        pop ebx
        pop edi
        pop esi
    }
    int done = count - remain;
    if (mode == 1) {
        I32(v, 0x16c) += done;
        I32(v, 0x1360) += wraps;
    } else {
        I32(v, 0x88) += done;
        I32(v, 0xcc) += wraps;
    }
    return done;
}

// MATCH: sound.dll 0x10013dd0 ?mixRatedStereo16Add@@YAHPAPAHPAPAFHPADH@Z
int __cdecl mixRatedStereo16Add(int **pdst, short **psrc, int count, char *v, int mode)
{
    int end;
    short *loopStart;
    int looping;
    int wraps;
    int chained;
    char *next;
    int advanced;
    int rate;
    int gainR;
    int once;
    int gainL;
    char *pos;
    int remain;

    chained = 0;
    once = 0;
    wraps = 0;
    advanced = 0;
    next = 0;
    if (mode == 1) {
        gainL = I32(v, 0x1e8);
        gainR = I32(v, 0x1f8);
        rate = I32(v, 0x220);
        pos = v + 0x1358;
        char *b = PTR(v, 8);
        end = (U32(b, 0x1c) > U32(b, 0x20)) ? I32(b, 0x24) : I32(b, 0x20);
        loopStart = *(short **)(b + 0x18);
        looping = (end == I32(b, 0x24));
        if ((U8(v, 0x58) & 0x20) && looping)
            once = 1;
    } else {
        gainL = I32(v, 0x78);
        gainR = I32(v, 0x68);
        rate = I32(v, 0x4c);
        if (I32(v, 0x4c) == 0x10000) {
            if (I32(v, 0x78) == 0x10000 && I32(v, 0x68) == 0x10000)
                return mixStereoAdd(pdst, psrc, count, v, mode);
            return mixStereoGainAdd(pdst, psrc, count, v, mode);
        }
        pos = v + 0xb8;
        looping = ((U8(v, 0x98) & 1) && !(U8(v, 0x9c) & 2)) ? 1 : 0;
        chained = U8(v, 0x9c) & 1;
        if (chained) {
            next = PTR(v, 0xb4);
            loopStart = *(short **)(next + 0x18);
            end = I32(v, 0x20);
        } else if (looping) {
            loopStart = *(short **)(v + 0xd8);
            end = I32(v, 0xd4);
        } else {
            loopStart = *(short **)(v + 0x18);
            end = I32(v, 0x20);
        }
    }
    // asm 0x156..0x30f
    __asm {
        push esi
        push edi
        push ebx
        push ecx
        push edx
        mov ecx, count
        mov edx, psrc
        mov esi, dword ptr [edx]
        mov edx, pdst
        mov edi, dword ptr [edx]
        mov ebx, pos
    L3f3b:
        push ecx
        movsx ecx, word ptr [esi]
        lea edx, [esi + 4]
        cmp edx, end
        jge L3f7c
        movsx eax, word ptr [edx]
    L3f4a:
        sub eax, ecx
        movzx edx, word ptr [ebx + 2]
        imul edx
        sar eax, 0x10
        add eax, ecx
        mov edx, gainL
        imul edx
        add dword ptr [edi], eax
        jo L3f65
        add edi, 4
        jmp L3f9c
    L3f65:
        jb L3f70
        mov eax, 0x7fffffff
        mov dword ptr [edi], eax
        jmp L3f77
    L3f70:
        mov eax, 0x80000000
        mov dword ptr [edi], eax
    L3f77:
        add edi, 4
        jmp L3f9c
    L3f7c:
        mov eax, 0
        cmp looping, 0
        je L3f4a
        mov eax, loopStart
        movsx eax, word ptr [eax]
        cmp once, 0
        je L3f4a
        mov looping, 0
        jmp L3f4a
    L3f9c:
        movsx ecx, word ptr [esi + 2]
        lea edx, [esi + 4]
        cmp edx, end
        jge L3fde
        movsx eax, word ptr [edx + 2]
    L3fac:
        sub eax, ecx
        movzx edx, word ptr [ebx + 2]
        imul edx
        sar eax, 0x10
        add eax, ecx
        mov edx, gainR
        imul edx
        add dword ptr [edi], eax
        jo L3fc7
        add edi, 4
        jmp L3fff
    L3fc7:
        jb L3fd2
        mov eax, 0x7fffffff
        mov dword ptr [edi], eax
        jmp L3fd9
    L3fd2:
        mov eax, 0x80000000
        mov dword ptr [edi], eax
    L3fd9:
        add edi, 4
        jmp L3fff
    L3fde:
        mov eax, 0
        cmp looping, 0
        je L3fac
        mov eax, loopStart
        movsx eax, word ptr [eax + 2]
        cmp once, 0
        je L3fac
        mov looping, 0
        jmp L3fac
    L3fff:
        mov eax, rate
        add dword ptr [ebx + 2], eax
        adc word ptr [ebx + 6], 0
        mov edx, dword ptr [ebx + 4]
        shl edx, 2
        mov eax, psrc
        mov esi, dword ptr [eax]
        add esi, edx
        cmp esi, end
        jge L4029
        pop ecx
        dec ecx
        jne L3f3b
        jmp L40c1
    L4029:
        pop ecx
        dec ecx
        cmp chained, 0
        jne L405d
        cmp looping, 0
        je L40c1
        inc wraps
        mov advanced, 0
        mov dword ptr [ebx + 4], 0
        mov eax, psrc
        mov esi, loopStart
        mov dword ptr [eax], esi
        cmp ecx, 0
        jne L3f3b
    L405d:
        inc wraps
        mov advanced, 0
        mov dword ptr [ebx + 4], 0
        mov eax, psrc
        mov esi, loopStart
        mov dword ptr [eax], esi
        mov eax, next
        mov edx, dword ptr [eax + 0xd4]
        mov end, edx
        mov edx, dword ptr [eax + 0xb4]
        mov next, edx
        cmp edx, 0
        je L40a4
        mov eax, dword ptr [edx + 0xd8]
        mov loopStart, eax
        cmp ecx, 0
        jne L3f3b
        jmp L40c1
    L40a4:
        mov chained, 0
        mov looping, 0
        mov edx, dword ptr [eax + 0xd8]
        cmp ecx, 0
        jne L3f3b
    L40c1:
        mov remain, ecx
        mov eax, dword ptr [ebx + 4]
        mov advanced, eax
        mov dword ptr [ebx + 4], 0
        mov edx, psrc
        mov dword ptr [edx], esi
        mov edx, pdst
        mov dword ptr [edx], edi
        pop edx
        pop ecx
        pop ebx
        pop edi
        pop esi
    }
    if (mode == 1) {
        I32(v, 0x16c) += advanced;
        I32(v, 0x1360) += wraps;
    } else {
        I32(v, 0x88) += advanced;
        I32(v, 0xcc) += wraps;
    }
    return count - remain;
}

// MATCH: sound.dll 0x10014250 ?mixMonoDupAdd@@YAHPAPAHPAPAFHPADH@Z
int __cdecl mixMonoDupAdd(int **pdst, short **psrc, int count, char *v, int mode)
{
    int end;
    short *loopStart;
    int looping;
    int wraps;
    int chained;
    char *next;
    int once;
    int remain;
    int carry;

    chained = 0;
    once = 0;
    wraps = 0;
    next = 0;
    if (mode == 1) {
        char *b = PTR(v, 8);
        end = (U32(b, 0x1c) > U32(b, 0x20)) ? I32(b, 0x24) : I32(b, 0x20);
        loopStart = *(short **)(b + 0x18);
        looping = (end == I32(b, 0x24));
        if ((U8(v, 0x58) & 0x20) && looping)
            once = 1;
    } else {
        looping = ((U8(v, 0x98) & 1) && !(U8(v, 0x9c) & 2)) ? 1 : 0;
        chained = U8(v, 0x9c) & 1;
        if (chained) {
            next = PTR(v, 0xb4);
            loopStart = *(short **)(next + 0x18);
            end = I32(v, 0x20);
        } else if (looping) {
            loopStart = *(short **)(v + 0xd8);
            end = I32(v, 0xd4);
        } else {
            loopStart = *(short **)(v + 0x18);
            end = I32(v, 0x20);
        }
    }
    // asm 0xb6..0x1c5
    __asm {
        push esi
        push edi
        push ebx
        push ecx
        push edx
        mov ecx, count
        mov edx, psrc
        mov esi, dword ptr [edx]
        mov edx, pdst
        mov edi, dword ptr [edx]
    L4318:
        movsx eax, word ptr [esi]
        shl eax, 0x10
        add dword ptr [edi], eax
        jo L4327
        add edi, 4
        jmp L433c
    L4327:
        jb L4332
        mov eax, 0x7fffffff
        mov dword ptr [edi], eax
        jmp L4339
    L4332:
        mov eax, 0x80000000
        mov dword ptr [edi], eax
    L4339:
        add edi, 4
    L433c:
        add dword ptr [edi], eax
        jo L4345
        add edi, 4
        jmp L435c
    L4345:
        jb L4350
        mov eax, 0x7fffffff
        mov dword ptr [edi], eax
        jmp L4357
    L4350:
        mov eax, 0x80000000
        mov dword ptr [edi], eax
    L4357:
        add edi, 4
        jmp L435c
    L435c:
        add esi, 2
        cmp esi, end
        jge L436c
        dec ecx
        jne L4318
        jmp L4404
    L436c:
        dec ecx
        cmp chained, 0
        jne L43a7
        cmp looping, 0
        je L4404
        inc wraps
        mov carry, 0
        mov eax, psrc
        mov esi, loopStart
        mov dword ptr [eax], esi
        cmp once, 0
        je L439c
        mov looping, 0
    L439c:
        cmp ecx, 0
        jne L4318
        jmp L4404
    L43a7:
        inc wraps
        mov carry, 0
        mov eax, psrc
        mov esi, loopStart
        mov dword ptr [eax], esi
        mov eax, next
        mov edx, dword ptr [eax + 0xd4]
        mov end, edx
        mov edx, dword ptr [eax + 0xb4]
        mov next, edx
        cmp edx, 0
        je L43e7
        mov eax, dword ptr [edx + 0xd8]
        mov loopStart, eax
        cmp ecx, 0
        jne L4318
        jmp L4404
    L43e7:
        mov chained, 0
        mov looping, 0
        mov edx, dword ptr [eax + 0xd8]
        cmp ecx, 0
        jne L4318
    L4404:
        mov remain, ecx
        mov edx, psrc
        mov dword ptr [edx], esi
        mov edx, pdst
        mov dword ptr [edx], edi
        pop edx
        pop ecx
        pop ebx
        pop edi
        pop esi
    }
    int done = count - remain;
    if (mode == 1) {
        I32(v, 0x16c) += done;
        I32(v, 0x1360) += wraps;
    } else {
        I32(v, 0x88) += done;
        I32(v, 0xcc) += wraps;
    }
    return done;
}

// MATCH: sound.dll 0x10014510 ?mixMonoGainFadeAdd@@YAHPAPAHPAPAFHPADH@Z
int __cdecl mixMonoGainFadeAdd(int **pdst, short **psrc, int count, char *v, int mode)
{
    int end;
    short *loopStart;
    int looping;
    int wraps;
    int chained;
    char *next;
    int gainL;
    int gainR;
    int step;
    int remain;
    int carry;

    step = 0;
    looping = 0;
    chained = 0;
    wraps = 0;
    next = 0;
    if (mode == 1) {
        gainL = I32(v, 0x1e8);
        gainR = I32(v, 0x1f8);
        char *b = PTR(v, 8);
        end = (U32(b, 0x1c) > U32(b, 0x20)) ? I32(b, 0x24) : I32(b, 0x20);
        loopStart = *(short **)(b + 0x18);
    } else {
        unsigned char flags = U8(v, 0x9c);
        gainL = I32(v, 0x78);
        gainR = I32(v, 0x68);
        if (flags & 8) {
            step = U32(v, 0xec) / (unsigned)count;
            if (step == 0)
                step = 1;
        }
        looping = ((U8(v, 0x98) & 1) && !(flags & 2)) ? 1 : 0;
        chained = flags & 1;
        if (chained) {
            next = PTR(v, 0xb4);
            loopStart = *(short **)(next + 0x18);
            end = I32(v, 0x20);
            goto asmblk;
        } else if (looping) {
            loopStart = *(short **)(v + 0xd8);
            end = I32(v, 0xd4);
        } else {
            loopStart = *(short **)(v + 0x18);
            end = I32(v, 0x20);
        }
    }
asmblk:
    // asm 0xde..0x207
    __asm {
        push esi
        push edi
        push ebx
        push ecx
        push edx
        mov ecx, count
        mov edx, psrc
        mov esi, dword ptr [edx]
        mov edx, pdst
        mov edi, dword ptr [edx]
    L4600:
        movsx eax, word ptr [esi]
        push eax
        mov edx, gainL
        imul edx
        add dword ptr [edi], eax
        jo L4612
        add edi, 4
        jmp L4629
    L4612:
        jb L461d
        mov eax, 0x7fffffff
        mov dword ptr [edi], eax
        jmp L4624
    L461d:
        mov eax, 0x80000000
        mov dword ptr [edi], eax
    L4624:
        add edi, 4
        jmp L4629
    L4629:
        pop eax
        mov edx, gainR
        imul edx
        add dword ptr [edi], eax
        jo L4638
        add edi, 4
        jmp L464f
    L4638:
        jb L4643
        mov eax, 0x7fffffff
        mov dword ptr [edi], eax
        jmp L464a
    L4643:
        mov eax, 0x80000000
        mov dword ptr [edi], eax
    L464a:
        add edi, 4
        jmp L464f
    L464f:
        add esi, 2
        cmp esi, end
        jge L467f
        mov edx, gainR
        sub edx, step
        jge L4664
        mov edx, 0
    L4664:
        mov gainR, edx
        mov edx, gainL
        sub edx, step
        jge L4674
        mov edx, 0
    L4674:
        mov gainL, edx
        dec ecx
        jne L4600
        jmp L4706
    L467f:
        dec ecx
        cmp chained, 0
        jne L46a9
        cmp looping, 0
        je L4706
        inc wraps
        mov carry, 0
        mov eax, psrc
        mov esi, loopStart
        mov dword ptr [eax], esi
        cmp ecx, 0
        jne L4600
        jmp L4706
    L46a9:
        inc wraps
        mov carry, 0
        mov eax, psrc
        mov esi, loopStart
        mov dword ptr [eax], esi
        mov eax, next
        mov edx, dword ptr [eax + 0xd4]
        mov end, edx
        mov edx, dword ptr [eax + 0xb4]
        mov next, edx
        cmp edx, 0
        je L46e9
        mov eax, dword ptr [edx + 0xd8]
        mov loopStart, eax
        cmp ecx, 0
        jne L4600
        jmp L4706
    L46e9:
        mov chained, 0
        mov looping, 0
        mov edx, dword ptr [eax + 0xd8]
        cmp ecx, 0
        jne L4600
    L4706:
        mov remain, ecx
        mov edx, psrc
        mov dword ptr [edx], esi
        mov edx, pdst
        mov dword ptr [edx], edi
        pop edx
        pop ecx
        pop ebx
        pop edi
        pop esi
    }
    int done = count - remain;
    if (mode == 1) {
        I32(v, 0x16c) += done;
        I32(v, 0x1360) += wraps;
    } else {
        I32(v, 0x88) += done;
        I32(v, 0xcc) += wraps;
    }
    return done;
}

// MATCH: sound.dll 0x10014820 ?mixRatedMono16AddFade@@YAHPAPAHPAPAFHPADH@Z
int __cdecl mixRatedMono16AddFade(int **pdst, short **psrc, int count, char *v, int mode)
{
    int end;
    short *loopStart;
    int looping;
    int wraps;
    int chained;
    char *next;
    int gainL;
    int gainR;
    int step;
    int advanced;
    int once;
    int rate;
    char *pos;
    int remain;

    step = 0;
    chained = 0;
    once = 0;
    wraps = 0;
    advanced = 0;
    next = 0;
    if (mode == 1) {
        gainL = I32(v, 0x1e8);
        gainR = I32(v, 0x1f8);
        rate = I32(v, 0x220);
        if (gainL == 0x10000 && gainR == 0x10000 && rate == 0x10000)
            return mixMonoDupAdd(pdst, psrc, count, v, mode);
        if (rate == 0x10000)
            return mixMonoGainFadeAdd(pdst, psrc, count, v, mode);
        pos = v + 0x1358;
        char *b = PTR(v, 8);
        end = (U32(b, 0x1c) > U32(b, 0x20)) ? I32(b, 0x24) : I32(b, 0x20);
        loopStart = *(short **)(b + 0x18);
        looping = (end == I32(b, 0x24));
        if ((U8(v, 0x58) & 0x20) && looping)
            once = 1;
    } else {
        unsigned char flags = U8(v, 0x9c);
        gainL = I32(v, 0x78);
        gainR = I32(v, 0x68);
        if (flags & 8) {
            step = U32(v, 0xec) / (unsigned)count;
            if (step == 0)
                step = 1;
        }
        rate = I32(v, 0x4c);
        if (gainL == 0x10000 && gainR == 0x10000 && I32(v, 0x4c) == 0x10000 && step == 0)
            return mixMonoDupAdd(pdst, psrc, count, v, mode);
        if (rate == 0x10000)
            return mixMonoGainFadeAdd(pdst, psrc, count, v, mode);
        pos = v + 0xb8;
        looping = ((U8(v, 0x98) & 1) && !(flags & 2)) ? 1 : 0;
        chained = flags & 1;
        if (chained) {
            next = PTR(v, 0xb4);
            loopStart = *(short **)(next + 0x18);
            end = I32(v, 0x20);
        } else if (looping) {
            loopStart = *(short **)(v + 0xd8);
            end = I32(v, 0xd4);
        } else {
            loopStart = *(short **)(v + 0x18);
            end = I32(v, 0x20);
        }
    }
    // asm 0x1db..0x38d
    __asm {
        push esi
        push edi
        push ebx
        push ecx
        push edx
        mov ecx, count
        mov edx, psrc
        mov esi, dword ptr [edx]
        mov edx, pdst
        mov edi, dword ptr [edx]
        mov ebx, pos
    L4a10:
        push ecx
        lea edx, [esi + 2]
        cmp edx, end
        jge L4aca
        movsx eax, word ptr [edx]
    L4a20:
        movsx ecx, word ptr [esi]
        sub eax, ecx
        movzx edx, word ptr [ebx + 2]
        imul edx
        sar eax, 0x10
        add eax, ecx
        push eax
        mov edx, gainL
        imul edx
        add dword ptr [edi], eax
        jo L4a9c
        add edi, 4
    L4a3d:
        pop eax
        mov edx, gainR
        imul edx
        add dword ptr [edi], eax
        jo L4ab3
        add edi, 4
    L4a4a:
        mov eax, rate
        mov edx, dword ptr [ebx + 2]
        add edx, eax
        jb L4a95
    L4a54:
        mov dword ptr [ebx + 2], edx
        shr edx, 0x10
        mov eax, psrc
        shl edx, 1
        mov esi, dword ptr [eax]
        add esi, edx
        cmp esi, end
        jge L4af5
        mov edx, gainR
        sub edx, step
        jge L4a79
        mov edx, 0
    L4a79:
        mov gainR, edx
        mov edx, gainL
        sub edx, step
        jge L4a89
        mov edx, 0
    L4a89:
        mov gainL, edx
        pop ecx
        dec ecx
        jne L4a10
        jmp L4b8f
    L4a95:
        adc word ptr [ebx + 6], 0
        jmp L4a54
    L4a9c:
        jb L4aa7
        mov eax, 0x7fffffff
        mov dword ptr [edi], eax
        jmp L4aae
    L4aa7:
        mov eax, 0x80000000
        mov dword ptr [edi], eax
    L4aae:
        add edi, 4
        jmp L4a3d
    L4ab3:
        jb L4abe
        mov eax, 0x7fffffff
        mov dword ptr [edi], eax
        jmp L4ac5
    L4abe:
        mov eax, 0x80000000
        mov dword ptr [edi], eax
    L4ac5:
        add edi, 4
        jmp L4a4a
    L4aca:
        mov eax, 0
        cmp looping, 0
        je L4a20
        mov eax, loopStart
        movsx eax, word ptr [eax]
        cmp once, 0
        je L4a20
        mov looping, 0
        jmp L4a20
    L4af5:
        pop ecx
        dec ecx
        cmp chained, 0
        jne L4b2b
        cmp looping, 0
        je L4b8f
        inc wraps
        mov advanced, 0
        mov dword ptr [ebx + 4], 0
        mov eax, psrc
        mov esi, loopStart
        mov dword ptr [eax], esi
        cmp ecx, 0
        jne L4a10
        jmp L4b8f
    L4b2b:
        inc wraps
        mov advanced, 0
        mov dword ptr [ebx + 4], 0
        mov eax, psrc
        mov esi, loopStart
        mov dword ptr [eax], esi
        mov eax, next
        mov edx, dword ptr [eax + 0xd4]
        mov end, edx
        mov edx, dword ptr [eax + 0xb4]
        mov next, edx
        cmp edx, 0
        je L4b72
        mov eax, dword ptr [edx + 0xd8]
        mov loopStart, eax
        cmp ecx, 0
        jne L4a10
        jmp L4b8f
    L4b72:
        mov chained, 0
        mov looping, 0
        mov edx, dword ptr [eax + 0xd8]
        cmp ecx, 0
        jne L4a10
    L4b8f:
        mov remain, ecx
        mov eax, dword ptr [ebx + 4]
        mov advanced, eax
        mov dword ptr [ebx + 4], 0
        mov edx, psrc
        mov dword ptr [edx], esi
        mov edx, pdst
        mov dword ptr [edx], edi
        pop edx
        pop ecx
        pop ebx
        pop edi
        pop esi
    }
    if (mode == 1) {
        I32(v, 0x16c) += advanced;
        I32(v, 0x1360) += wraps;
    } else {
        I32(v, 0x88) += advanced;
        I32(v, 0xcc) += wraps;
    }
    return count - remain;
}

// MATCH: sound.dll 0x100152b0 ?mixRatedStereo8Add@@YAHPAPAHPAPAEHPADH@Z
int __cdecl mixRatedStereo8Add(int **pdst, unsigned char **psrc, int count, char *v, int mode)
{
    int end;
    unsigned char *loopStart;
    int looping;
    int wraps;
    int chained;
    char *next;
    int advanced;
    int rate;
    int gainR;
    int gainL;
    char *pos;
    char *voice;
    int remain;

    looping = 0;
    chained = 0;
    wraps = 0;
    advanced = 0;
    next = 0;
    voice = 0;
    if (mode == 1) {
        gainL = I32(v, 0x1e8);
        gainR = I32(v, 0x1f8);
        rate = I32(v, 0x220);
        pos = v + 0x1358;
        char *b = PTR(v, 8);
        end = (U32(b, 0x1c) > U32(b, 0x20)) ? I32(b, 0x24) : I32(b, 0x20);
        loopStart = *(unsigned char **)(b + 0x18);
    } else {
        voice = v;
        gainL = I32(v, 0x78);
        gainR = I32(v, 0x68);
        rate = I32(v, 0x4c);
        pos = v + 0xb8;
        looping = ((U8(v, 0x98) & 1) && !(U8(v, 0x9c) & 2)) ? 1 : 0;
        chained = U8(v, 0x9c) & 1;
        if (chained) {
            next = PTR(v, 0xb4);
            loopStart = *(unsigned char **)(next + 0x18);
            end = I32(v, 0x20);
            goto asmblk;
        } else if (looping) {
            loopStart = *(unsigned char **)(v + 0xd8);
            end = I32(v, 0xd4);
        } else {
            loopStart = *(unsigned char **)(v + 0x18);
            end = I32(v, 0x20);
        }
    }
asmblk:
    // asm 0xe1..0x2b3
    __asm {
        push esi
        push edi
        push ebx
        push ecx
        push edx
        mov ecx, count
        mov edx, psrc
        mov esi, dword ptr [edx]
        mov edx, pdst
        mov edi, dword ptr [edx]
        mov ebx, pos
    L53a6:
        push ecx
        movzx ecx, byte ptr [esi]
        shl ecx, 8
        sub ecx, 0x8000
        lea edx, [esi + 2]
        cmp edx, end
        jge L53f8
        movzx eax, byte ptr [edx]
        shl eax, 8
        sub eax, 0x8000
    L53c6:
        sub eax, ecx
        movzx edx, word ptr [ebx + 2]
        imul edx
        sar eax, 0x10
        add eax, ecx
        mov edx, gainL
        imul edx
        add dword ptr [edi], eax
        jo L53e1
        add edi, 4
        jmp L5413
    L53e1:
        jb L53ec
        mov eax, 0x7fffffff
        mov dword ptr [edi], eax
        jmp L53f3
    L53ec:
        mov eax, 0x80000000
        mov dword ptr [edi], eax
    L53f3:
        add edi, 4
        jmp L5413
    L53f8:
        mov eax, 0
        cmp looping, 0
        je L53c6
        mov eax, loopStart
        movzx eax, byte ptr [eax]
        shl eax, 8
        sub eax, 0x8000
        jmp L53c6
    L5413:
        movzx ecx, byte ptr [esi + 1]
        shl ecx, 8
        sub ecx, 0x8000
        lea edx, [esi + 2]
        cmp edx, end
        jge L5466
        movzx eax, byte ptr [edx + 1]
        shl eax, 8
        sub eax, 0x8000
    L5434:
        sub eax, ecx
        movzx edx, word ptr [ebx + 2]
        imul edx
        sar eax, 0x10
        add eax, ecx
        mov edx, gainR
        imul edx
        add dword ptr [edi], eax
        jo L544f
        add edi, 4
        jmp L5482
    L544f:
        jb L545a
        mov eax, 0x7fffffff
        mov dword ptr [edi], eax
        jmp L5461
    L545a:
        mov eax, 0x80000000
        mov dword ptr [edi], eax
    L5461:
        add edi, 4
        jmp L5482
    L5466:
        mov eax, 0
        cmp looping, 0
        je L5434
        mov eax, loopStart
        movzx eax, byte ptr [eax + 1]
        shl eax, 8
        sub eax, 0x8000
        jmp L5434
    L5482:
        mov eax, rate
        add dword ptr [ebx + 2], eax
        adc word ptr [ebx + 6], 0
        mov edx, dword ptr [ebx + 4]
        shl edx, 1
        mov eax, psrc
        mov esi, dword ptr [eax]
        add esi, edx
        cmp esi, end
        jge L54ab
        pop ecx
        dec ecx
        jne L53a6
        jmp L5545
    L54ab:
        pop ecx
        dec ecx
        cmp chained, 0
        jne L54e1
        cmp looping, 0
        je L5545
        inc wraps
        mov advanced, 0
        mov dword ptr [ebx + 4], 0
        mov eax, psrc
        mov esi, loopStart
        mov dword ptr [eax], esi
        cmp ecx, 0
        jne L53a6
        jmp L5545
    L54e1:
        inc wraps
        mov advanced, 0
        mov dword ptr [ebx + 4], 0
        mov eax, psrc
        mov esi, loopStart
        mov dword ptr [eax], esi
        mov eax, next
        mov edx, dword ptr [eax + 0xd4]
        mov end, edx
        mov edx, dword ptr [eax + 0xb4]
        mov next, edx
        cmp edx, 0
        je L5528
        mov eax, dword ptr [edx + 0xd8]
        mov loopStart, eax
        cmp ecx, 0
        jne L53a6
        jmp L5545
    L5528:
        mov chained, 0
        mov looping, 0
        mov edx, dword ptr [eax + 0xd8]
        cmp ecx, 0
        jne L53a6
    L5545:
        mov remain, ecx
        mov eax, dword ptr [ebx + 4]
        mov advanced, eax
        mov dword ptr [ebx + 4], 0
        mov edx, psrc
        mov dword ptr [edx], esi
        mov edx, pdst
        mov dword ptr [edx], edi
        pop edx
        pop ecx
        pop ebx
        pop edi
        pop esi
    }
    if (mode == 1) {
        I32(v, 0x16c) += advanced;
        I32(v, 0x1360) += wraps;
    } else {
        int *p = (int *)(voice + 0x88);
        *p += advanced;
        I32(voice, 0xcc) += wraps;

    }
    return count - remain;
}

// MATCH: sound.dll 0x100156c0 ?mixRatedMono8Add@@YAHPAPAHPAPAEHPADH@Z
int __cdecl mixRatedMono8Add(int **pdst, unsigned char **psrc, int count, char *v, int mode)
{
    char once;
    int end;
    unsigned char *loopStart;
    int looping;
    int wraps;
    int chained;
    char *next;
    int advanced;
    int rate;
    int gainR;
    int gainL;
    char *pos;
    int remain;

    chained = 0;
    wraps = 0;
    once = 0;
    advanced = 0;
    next = 0;
    if (mode == 1) {
        gainL = I32(v, 0x1e8);
        gainR = I32(v, 0x1f8);
        rate = I32(v, 0x220);
        pos = v + 0x1358;
        char *b = PTR(v, 8);
        end = (U32(b, 0x1c) > U32(b, 0x20)) ? I32(b, 0x24) : I32(b, 0x20);
        loopStart = *(unsigned char **)(b + 0x18);
        looping = (end == I32(b, 0x24));
        if ((U8(v, 0x58) & 0x20) && looping)
            once = 1;
    } else {
        gainL = I32(v, 0x78);
        gainR = I32(v, 0x68);
        rate = I32(v, 0x4c);
        pos = v + 0xb8;
        looping = ((U8(v, 0x98) & 1) && !(U8(v, 0x9c) & 2)) ? 1 : 0;
        chained = U8(v, 0x9c) & 1;
        if (chained) {
            next = PTR(v, 0xb4);
            loopStart = *(unsigned char **)(next + 0x18);
            end = I32(v, 0x20);
            goto asmblk;
        } else if (looping) {
            loopStart = *(unsigned char **)(v + 0xd8);
            end = I32(v, 0xd4);
        } else {
            loopStart = *(unsigned char **)(v + 0x18);
            end = I32(v, 0x20);
        }
    }
asmblk:
    // asm 0xfa..0x28d
    __asm {
        push esi
        push edi
        push ebx
        push ecx
        push edx
        mov ecx, count
        mov edx, psrc
        mov esi, dword ptr [edx]
        mov edx, pdst
        mov edi, dword ptr [edx]
        mov ebx, pos
    L57cf:
        push ecx
        movzx ecx, byte ptr [esi]
        shl ecx, 8
        sub ecx, 0x8000
        lea edx, [esi + 1]
        cmp edx, end
        jge L5822
        movzx eax, byte ptr [edx]
        shl eax, 8
        sub eax, 0x8000
    L57ef:
        sub eax, ecx
        movzx edx, word ptr [ebx + 2]
        imul edx
        sar eax, 0x10
        add eax, ecx
        push eax
        mov edx, gainL
        imul edx
        add dword ptr [edi], eax
        jo L580b
        add edi, 4
        jmp L584a
    L580b:
        jb L5816
        mov eax, 0x7fffffff
        mov dword ptr [edi], eax
        jmp L581d
    L5816:
        mov eax, 0x80000000
        mov dword ptr [edi], eax
    L581d:
        add edi, 4
        jmp L584a
    L5822:
        mov eax, 0
        cmp looping, 0
        je L57ef
        mov eax, loopStart
        movzx eax, byte ptr [eax]
        shl eax, 8
        sub eax, 0x8000
        cmp once, 0
        je L57ef
        mov looping, 0
        jmp L57ef
    L584a:
        pop eax
        mov edx, gainR
        imul edx
        add dword ptr [edi], eax
        jo L5859
        add edi, 4
        jmp L5870
    L5859:
        jb L5864
        mov eax, 0x7fffffff
        mov dword ptr [edi], eax
        jmp L586b
    L5864:
        mov eax, 0x80000000
        mov dword ptr [edi], eax
    L586b:
        add edi, 4
        jmp L5870
    L5870:
        mov eax, rate
        add dword ptr [ebx + 2], eax
        adc word ptr [ebx + 6], 0
        mov edx, dword ptr [ebx + 4]
        mov eax, psrc
        mov esi, dword ptr [eax]
        add esi, edx
        cmp esi, end
        jge L5897
        pop ecx
        dec ecx
        jne L57cf
        jmp L592f
    L5897:
        pop ecx
        dec ecx
        cmp chained, 0
        jne L58cb
        cmp looping, 0
        je L592f
        inc wraps
        mov advanced, 0
        mov dword ptr [ebx + 4], 0
        mov eax, psrc
        mov esi, loopStart
        mov dword ptr [eax], esi
        cmp ecx, 0
        jne L57cf
    L58cb:
        inc wraps
        mov advanced, 0
        mov dword ptr [ebx + 4], 0
        mov eax, psrc
        mov esi, loopStart
        mov dword ptr [eax], esi
        mov eax, next
        mov edx, dword ptr [eax + 0xd4]
        mov end, edx
        mov edx, dword ptr [eax + 0xb4]
        mov next, edx
        cmp edx, 0
        je L5912
        mov eax, dword ptr [edx + 0xd8]
        mov loopStart, eax
        cmp ecx, 0
        jne L57cf
        jmp L592f
    L5912:
        mov chained, 0
        mov looping, 0
        mov edx, dword ptr [eax + 0xd8]
        cmp ecx, 0
        jne L57cf
    L592f:
        mov remain, ecx
        mov eax, dword ptr [ebx + 4]
        mov advanced, eax
        mov dword ptr [ebx + 4], 0
        mov edx, psrc
        mov dword ptr [edx], esi
        mov edx, pdst
        mov dword ptr [edx], edi
        pop edx
        pop ecx
        pop ebx
        pop edi
        pop esi
    }
    if (mode == 1) {
        I32(v, 0x16c) += advanced;
        I32(v, 0x1360) += wraps;
    } else {
        I32(v, 0x88) += advanced;
        I32(v, 0xcc) += wraps;
    }
    return count - remain;
}

// MATCH: sound.dll 0x10016340 ?mixMonoDupSet@@YAHPAPAHPAPAFHPADH@Z
int __cdecl mixMonoDupSet(int **pdst, short **psrc, int count, char *v, int mode)
{
    int end;
    short *loopStart;
    int looping;
    int wraps;
    int chained;
    char *next;
    int once;
    int remain;
    int remain2;

    chained = 0;
    once = 0;
    wraps = 0;
    next = 0;
    if (mode == 1) {
        char *b = PTR(v, 8);
        end = (U32(b, 0x1c) > U32(b, 0x20)) ? I32(b, 0x24) : I32(b, 0x20);
        loopStart = *(short **)(b + 0x18);
        looping = (end == I32(b, 0x24));
        if ((U8(v, 0x58) & 0x20) && looping)
            once = 1;
    } else {
        looping = ((U8(v, 0x98) & 1) && !(U8(v, 0x9c) & 2)) ? 1 : 0;
        chained = U8(v, 0x9c) & 1;
        if (chained) {
            next = PTR(v, 0xb4);
            loopStart = *(short **)(next + 0x18);
            end = I32(v, 0x20);
        } else if (looping) {
            loopStart = *(short **)(v + 0xd8);
            end = I32(v, 0xd4);
        } else {
            loopStart = *(short **)(v + 0x18);
            end = I32(v, 0x20);
        }
    }
    __asm {
        push esi
        push edi
        push ebx
        push ecx
        push edx
        mov ecx, count
        mov edx, psrc
        mov esi, dword ptr [edx]
        mov edx, pdst
        mov edi, dword ptr [edx]
    L6408:
        movsx eax, word ptr [esi]
        shl eax, 0x10
        mov dword ptr [edi], eax
        add edi, 4
        mov dword ptr [edi], eax
        add edi, 4
        add esi, 2
        cmp esi, end
        jge L6425
        dec ecx
        jne L6408
        jmp L64a3
    L6425:
        dec ecx
        cmp chained, 0
        jne L6451
        cmp looping, 0
        je L64a3
        inc wraps
        mov eax, psrc
        mov esi, loopStart
        mov dword ptr [eax], esi
        cmp once, 0
        je L644a
        mov looping, 0
    L644a:
        cmp ecx, 0
        jne L6408
        jmp L64a3
    L6451:
        inc wraps
        mov eax, psrc
        mov esi, loopStart
        mov dword ptr [eax], esi
        mov eax, next
        mov edx, dword ptr [eax + 0xd4]
        mov end, edx
        mov edx, dword ptr [eax + 0xb4]
        mov next, edx
        cmp edx, 0
        je L6486
        mov eax, dword ptr [edx + 0xd8]
        mov loopStart, eax
        cmp ecx, 0
        jne L6408
        jmp L64a3
    L6486:
        mov chained, 0
        mov looping, 0
        mov edx, dword ptr [eax + 0xd8]
        cmp ecx, 0
        jne L6408
    L64a3:
        mov remain, ecx
        mov remain2, ecx
        mov edx, psrc
        mov dword ptr [edx], esi
        mov edx, pdst
        mov dword ptr [edx], edi
        pop edx
        pop ecx
        pop ebx
        pop edi
        pop esi
    }
    int done = count - remain;
    if (mode == 1) {
        I32(v, 0x16c) += done;
        I32(v, 0x1360) += wraps;
    } else {
        I32(v, 0x88) += done;
        I32(v, 0xcc) += wraps;
    }
    return done;

}

// MATCH: sound.dll 0x100165a0 ?mixMonoGainFadeSet@@YAHPAPAHPAPAFHPADH@Z
int __cdecl mixMonoGainFadeSet(int **pdst, short **psrc, int count, char *v, int mode)
{
    int end;
    short *loopStart;
    int looping;
    int wraps;
    int chained;
    char *next;
    int gainL;
    int gainR;
    int step;
    int remain;

    looping = 0;
    chained = 0;
    wraps = 0;
    step = 0;
    next = 0;
    if (mode == 1) {
        gainL = I32(v, 0x1e8);
        gainR = I32(v, 0x1f8);
        char *b = PTR(v, 8);
        end = (U32(b, 0x1c) > U32(b, 0x20)) ? I32(b, 0x24) : I32(b, 0x20);
        loopStart = *(short **)(b + 0x18);
    } else {
        unsigned char flags = U8(v, 0x9c);
        gainL = I32(v, 0x78);
        gainR = I32(v, 0x68);
        if (flags & 8) {
            step = U32(v, 0xec) / (unsigned)count;
            if (step == 1)
                step = 1;
        }
        looping = ((U8(v, 0x98) & 1) && !(flags & 2)) ? 1 : 0;
        chained = flags & 1;
        if (chained) {
            next = PTR(v, 0xb4);
            loopStart = *(short **)(next + 0x18);
            end = I32(v, 0x20);
            goto asmblk;
        } else if (looping) {
            loopStart = *(short **)(v + 0xd8);
            end = I32(v, 0xd4);
        } else {
            loopStart = *(short **)(v + 0x18);
            end = I32(v, 0x20);
        }
    }
asmblk:
    // asm 0xde..0x1b9
    __asm {
        push esi
        push edi
        push ebx
        push ecx
        push edx
        mov ecx, count
        mov edx, psrc
        mov esi, dword ptr [edx]
        mov edx, pdst
        mov edi, dword ptr [edx]
    L6690:
        movsx eax, word ptr [esi]
        push eax
        mov edx, gainL
        imul edx
        mov dword ptr [edi], eax
        add edi, 4
        pop eax
        mov edx, gainR
        imul edx
        mov dword ptr [edi], eax
        add edi, 4
        add esi, 2
        cmp esi, end
        jge L66d6
        mov edx, gainR
        sub edx, step
        jge L66be
        mov edx, 0
    L66be:
        mov gainR, edx
        mov edx, gainL
        sub edx, step
        jge L66ce
        mov edx, 0
    L66ce:
        mov gainL, edx
        dec ecx
        jne L6690
        jmp L6748
    L66d6:
        dec ecx
        cmp chained, 0
        jne L66f2
        cmp looping, 0
        je L6748
        mov eax, psrc
        mov esi, loopStart
        mov dword ptr [eax], esi
        cmp ecx, 0
        jne L6690
        jmp L6748
    L66f2:
        inc wraps
        mov eax, psrc
        mov esi, loopStart
        mov dword ptr [eax], esi
        mov eax, next
        mov edx, dword ptr [eax + 0xd4]
        mov end, edx
        mov edx, dword ptr [eax + 0xb4]
        mov next, edx
        cmp edx, 0
        je L672b
        mov eax, dword ptr [edx + 0xd8]
        mov loopStart, eax
        cmp ecx, 0
        jne L6690
        jmp L6748
    L672b:
        mov chained, 0
        mov looping, 0
        mov edx, dword ptr [eax + 0xd8]
        cmp ecx, 0
        jne L6690
    L6748:
        mov remain, ecx
        mov edx, psrc
        mov dword ptr [edx], esi
        mov edx, pdst
        mov dword ptr [edx], edi
        pop edx
        pop ecx
        pop ebx
        pop edi
        pop esi
    }
    int done = count - remain;
    if (mode == 1) {
        I32(v, 0x16c) += done;
        I32(v, 0x1360) += wraps;
    } else {
        I32(v, 0x88) += done;
        I32(v, 0xcc) += wraps;
    }
    return done;
}

// MATCH: sound.dll 0x10016850 ?mixRatedMono16SetFade@@YAHPAPAHPAPAFHPADH@Z
int __cdecl mixRatedMono16SetFade(int **pdst, short **psrc, int count, char *v, int mode)
{
    int end;
    short *loopStart;
    int looping;
    int wraps;
    int chained;
    char *next;
    int gainL;
    int gainR;
    int step;
    int advanced;
    int rate;
    int once;
    char *pos;
    int remain;

    step = 0;
    chained = 0;
    once = 0;
    wraps = 0;
    advanced = 0;
    next = 0;
    if (mode == 1) {
        gainL = I32(v, 0x1e8);
        gainR = I32(v, 0x1f8);
        rate = I32(v, 0x220);
        if (gainL == 0x10000 && gainR == 0x10000 && rate == 0x10000)
            return mixMonoDupSet(pdst, psrc, count, v, mode);
        if (rate == 0x10000)
            return mixMonoGainFadeSet(pdst, psrc, count, v, mode);
        pos = v + 0x1358;
        char *b = PTR(v, 8);
        end = (U32(b, 0x1c) > U32(b, 0x20)) ? I32(b, 0x24) : I32(b, 0x20);
        loopStart = *(short **)(b + 0x18);
        looping = (end == I32(b, 0x24));
        if ((U8(v, 0x58) & 0x20) && looping)
            once = 1;
    } else {
        gainL = I32(v, 0x78);
        gainR = I32(v, 0x68);
        rate = I32(v, 0x4c);
        if (I32(v, 0x78) == 0x10000 && I32(v, 0x68) == 0x10000 && I32(v, 0x4c) == 0x10000 && !(U8(v, 0x9c) & 8))
            return mixMonoDupSet(pdst, psrc, count, v, mode);
        if (rate == 0x10000)
            return mixMonoGainFadeSet(pdst, psrc, count, v, mode);
        unsigned char flags = U8(v, 0x9c);
        if (flags & 8) {
            step = U32(v, 0xec) / (unsigned)count;
            if (step == 1)
                step = 1;
        }
        pos = v + 0xb8;
        looping = ((U8(v, 0x98) & 1) && !(flags & 2)) ? 1 : 0;
        chained = flags & 1;
        if (chained) {
            next = PTR(v, 0xb4);
            loopStart = *(short **)(next + 0x18);
            end = I32(v, 0x20);
        } else if (looping) {
            loopStart = *(short **)(v + 0xd8);
            end = I32(v, 0xd4);
        } else {
            loopStart = *(short **)(v + 0x18);
            end = I32(v, 0x20);
        }
    }
    // asm 0x1d6..0x33f
    __asm {
        push esi
        push edi
        push ebx
        push ecx
        push edx
        mov ecx, count
        mov edx, psrc
        mov esi, dword ptr [edx]
        mov edx, pdst
        mov edi, dword ptr [edx]
        mov ebx, pos
    L6a3b:
        push ecx
        movsx ecx, word ptr [esi]
        lea edx, [esi + 2]
        cmp edx, end
        jge L6a64
        movsx eax, word ptr [edx]
    L6a4a:
        sub eax, ecx
        movzx edx, word ptr [ebx + 2]
        imul edx
        sar eax, 0x10
        add eax, ecx
        push eax
        mov edx, gainL
        imul edx
        mov dword ptr [edi], eax
        add edi, 4
        jmp L6a84
    L6a64:
        mov eax, 0
        cmp looping, 0
        je L6a4a
        mov eax, loopStart
        movsx eax, word ptr [eax]
        cmp once, 0
        je L6a4a
        mov looping, 0
        jmp L6a4a
    L6a84:
        pop eax
        mov edx, gainR
        imul edx
        mov dword ptr [edi], eax
        add edi, 4
        jmp L6a91
    L6a91:
        mov eax, rate
        add dword ptr [ebx + 2], eax
        adc word ptr [ebx + 6], 0
        mov edx, dword ptr [ebx + 4]
        shl edx, 1
        mov eax, psrc
        mov esi, dword ptr [eax]
        add esi, edx
        cmp esi, end
        jge L6ada
        mov edx, gainR
        sub edx, step
        jge L6aba
        mov edx, 0
    L6aba:
        mov gainR, edx
        mov edx, gainL
        sub edx, step
        jge L6aca
        mov edx, 0
    L6aca:
        mov gainL, edx
        pop ecx
        dec ecx
        jne L6a3b
        jmp L6b71
    L6ada:
        pop ecx
        dec ecx
        cmp chained, 0
        jne L6b0d
        cmp looping, 0
        je L6b71
        mov advanced, 0
        mov dword ptr [ebx + 4], 0
        mov eax, psrc
        mov esi, loopStart
        mov dword ptr [eax], esi
        cmp ecx, 0
        jne L6a3b
        jmp L6b71
    L6b0d:
        inc wraps
        mov advanced, 0
        mov dword ptr [ebx + 4], 0
        mov eax, psrc
        mov esi, loopStart
        mov dword ptr [eax], esi
        mov eax, next
        mov edx, dword ptr [eax + 0xd4]
        mov end, edx
        mov edx, dword ptr [eax + 0xb4]
        mov next, edx
        cmp edx, 0
        je L6b54
        mov eax, dword ptr [edx + 0xd8]
        mov loopStart, eax
        cmp ecx, 0
        jne L6a3b
        jmp L6b71
    L6b54:
        mov chained, 0
        mov looping, 0
        mov edx, dword ptr [eax + 0xd8]
        cmp ecx, 0
        jne L6a3b
    L6b71:
        mov remain, ecx
        mov eax, dword ptr [ebx + 4]
        mov advanced, eax
        mov dword ptr [ebx + 4], 0
        mov edx, psrc
        mov dword ptr [edx], esi
        mov edx, pdst
        mov dword ptr [edx], edi
        pop edx
        pop ecx
        pop ebx
        pop edi
        pop esi
    }
    if (mode == 1) {
        I32(v, 0x16c) += advanced;
        I32(v, 0x1360) += wraps;
    } else {
        I32(v, 0x88) += advanced;
        I32(v, 0xcc) += wraps;
    }
    return count - remain;
}

// MATCH: sound.dll 0x10016d90 ?mixRatedStereo8Set@@YAHPAPAHPAPAEHPADH@Z
int __cdecl mixRatedStereo8Set(int **pdst, unsigned char **psrc, int count, char *v, int mode)
{
    int end;
    unsigned char *loopStart;
    int looping;
    int wraps;
    int chained;
    char *next;
    int advanced;
    int rate;
    int gainR;
    int gainL;
    char *pos;
    int remain;

    looping = 0;
    chained = 0;
    wraps = 0;
    advanced = 0;
    next = 0;
    if (mode == 1) {
        gainL = I32(v, 0x1e8);
        gainR = I32(v, 0x1f8);
        rate = I32(v, 0x220);
        pos = v + 0x1358;
        char *b = PTR(v, 8);
        end = (U32(b, 0x1c) > U32(b, 0x20)) ? I32(b, 0x24) : I32(b, 0x20);
        loopStart = *(unsigned char **)(b + 0x18);
    } else {
        gainL = I32(v, 0x78);
        gainR = I32(v, 0x68);
        rate = I32(v, 0x4c);
        pos = v + 0xb8;
        looping = ((U8(v, 0x98) & 1) && !(U8(v, 0x9c) & 2)) ? 1 : 0;
        chained = U8(v, 0x9c) & 1;
        if (chained) {
            next = PTR(v, 0xb4);
            loopStart = *(unsigned char **)(next + 0x18);
            end = I32(v, 0x20);
            goto asmblk;
        } else if (looping) {
            loopStart = *(unsigned char **)(v + 0xd8);
            end = I32(v, 0xd4);
        } else {
            loopStart = *(unsigned char **)(v + 0x18);
            end = I32(v, 0x20);
        }
    }
asmblk:
    // asm 0xe4..0x284
    __asm {
        push esi
        push edi
        push ebx
        push ecx
        push edx
        mov ecx, count
        mov edx, psrc
        mov esi, dword ptr [edx]
        mov edx, pdst
        mov edi, dword ptr [edx]
        mov ebx, pos
    L6e89:
        push ecx
        movzx ecx, byte ptr [esi]
        shl ecx, 8
        sub ecx, 0x8000
        lea edx, [esi + 2]
        cmp edx, end
        jge L6ec2
        movzx eax, byte ptr [edx]
        shl eax, 8
        sub eax, 0x8000
    L6ea9:
        sub eax, ecx
        movzx edx, word ptr [ebx + 2]
        imul edx
        sar eax, 0x10
        add eax, ecx
        mov edx, gainL
        imul edx
        mov dword ptr [edi], eax
        add edi, 4
        jmp L6edd
    L6ec2:
        mov eax, 0
        cmp looping, 0
        je L6ea9
        mov eax, loopStart
        movzx eax, byte ptr [eax]
        shl eax, 8
        sub eax, 0x8000
        jmp L6ea9
    L6edd:
        movzx ecx, byte ptr [esi + 1]
        shl ecx, 8
        sub ecx, 0x8000
        lea edx, [esi + 2]
        cmp edx, end
        jge L6f17
        movzx eax, byte ptr [edx + 1]
        shl eax, 8
        sub eax, 0x8000
    L6efe:
        sub eax, ecx
        movzx edx, word ptr [ebx + 2]
        imul edx
        sar eax, 0x10
        add eax, ecx
        mov edx, gainR
        imul edx
        mov dword ptr [edi], eax
        add edi, 4
        jmp L6f33
    L6f17:
        mov eax, 0
        cmp looping, 0
        je L6efe
        mov eax, loopStart
        movzx eax, byte ptr [eax + 1]
        shl eax, 8
        sub eax, 0x8000
        jmp L6efe
    L6f33:
        mov eax, rate
        add dword ptr [ebx + 2], eax
        adc word ptr [ebx + 6], 0
        mov edx, dword ptr [ebx + 4]
        shl edx, 1
        mov eax, psrc
        mov esi, dword ptr [eax]
        add esi, edx
        cmp esi, end
        jge L6f5c
        pop ecx
        dec ecx
        jne L6e89
        jmp L6ff6
    L6f5c:
        pop ecx
        dec ecx
        cmp chained, 0
        jne L6f92
        cmp looping, 0
        je L6ff6
        inc wraps
        mov advanced, 0
        mov dword ptr [ebx + 4], 0
        mov eax, psrc
        mov esi, loopStart
        mov dword ptr [eax], esi
        cmp ecx, 0
        jne L6e89
        jmp L6ff6
    L6f92:
        inc wraps
        mov advanced, 0
        mov dword ptr [ebx + 4], 0
        mov eax, psrc
        mov esi, loopStart
        mov dword ptr [eax], esi
        mov eax, next
        mov edx, dword ptr [eax + 0xd4]
        mov end, edx
        mov edx, dword ptr [eax + 0xb4]
        mov next, edx
        cmp edx, 0
        je L6fd9
        mov eax, dword ptr [edx + 0xd8]
        mov loopStart, eax
        cmp ecx, 0
        jne L6e89
        jmp L6ff6
    L6fd9:
        mov chained, 0
        mov looping, 0
        mov edx, dword ptr [eax + 0xd8]
        cmp ecx, 0
        jne L6e89
    L6ff6:
        mov remain, ecx
        mov eax, dword ptr [ebx + 4]
        mov advanced, eax
        mov dword ptr [ebx + 4], 0
        mov edx, psrc
        mov dword ptr [edx], esi
        mov edx, pdst
        mov dword ptr [edx], edi
        pop edx
        pop ecx
        pop ebx
        pop edi
        pop esi
    }
    if (mode == 1) {
        I32(v, 0x16c) += advanced;
        I32(v, 0x1360) += wraps;
    } else {
        I32(v, 0x88) += advanced;
        I32(v, 0xcc) += wraps;
    }
    return count - remain;
}

// MATCH: sound.dll 0x10017160 ?mixRatedMono8Set@@YAHPAPAHPAPAEHPADH@Z
int __cdecl mixRatedMono8Set(int **pdst, unsigned char **psrc, int count, char *v, int mode)
{
    int end;
    unsigned char *loopStart;
    int looping;
    int wraps;
    int chained;
    char *next;
    int advanced;
    int rate;
    int gainR;
    int once;
    int gainL;
    char *pos;
    int remain;

    chained = 0;
    once = 0;
    wraps = 0;
    advanced = 0;
    next = 0;
    if (mode == 1) {
        gainL = I32(v, 0x1e8);
        gainR = I32(v, 0x1f8);
        rate = I32(v, 0x220);
        pos = v + 0x1358;
        char *b = PTR(v, 8);
        end = (U32(b, 0x1c) > U32(b, 0x20)) ? I32(b, 0x24) : I32(b, 0x20);
        loopStart = *(unsigned char **)(b + 0x18);
        looping = (end == I32(b, 0x24));
        if ((U8(v, 0x58) & 0x20) && looping)
            once = 1;
    } else {
        gainL = I32(v, 0x78);
        gainR = I32(v, 0x68);
        rate = I32(v, 0x4c);
        pos = v + 0xb8;
        looping = ((U8(v, 0x98) & 1) && !(U8(v, 0x9c) & 2)) ? 1 : 0;
        chained = U8(v, 0x9c) & 1;
        if (chained) {
            next = PTR(v, 0xb4);
            loopStart = *(unsigned char **)(next + 0x18);
            end = I32(v, 0x20);
            goto asmblk;
        } else if (looping) {
            loopStart = *(unsigned char **)(v + 0xd8);
            end = I32(v, 0xd4);
        } else {
            loopStart = *(unsigned char **)(v + 0x18);
            end = I32(v, 0x20);
        }
    }
asmblk:
    // asm 0x10b..0x269
    __asm {
        push esi
        push edi
        push ebx
        push ecx
        push edx
        mov ecx, count
        mov edx, psrc
        mov esi, dword ptr [edx]
        mov edx, pdst
        mov edi, dword ptr [edx]
        mov ebx, pos
    L7280:
        push ecx
        movzx ecx, byte ptr [esi]
        shl ecx, 8
        sub ecx, 0x8000
        lea edx, [esi + 1]
        cmp edx, end
        jge L72ba
        movzx eax, byte ptr [edx]
        shl eax, 8
        sub eax, 0x8000
    L72a0:
        sub eax, ecx
        movzx edx, word ptr [ebx + 2]
        imul edx
        sar eax, 0x10
        add eax, ecx
        push eax
        mov edx, gainL
        imul edx
        mov dword ptr [edi], eax
        add edi, 4
        jmp L72e2
    L72ba:
        mov eax, 0
        cmp looping, 0
        je L72a0
        mov eax, loopStart
        movzx eax, byte ptr [eax]
        shl eax, 8
        sub eax, 0x8000
        cmp once, 0
        je L72a0
        mov looping, 0
        jmp L72a0
    L72e2:
        pop eax
        mov edx, gainR
        imul edx
        mov dword ptr [edi], eax
        add edi, 4
        mov eax, rate
        add dword ptr [ebx + 2], eax
        adc word ptr [ebx + 6], 0
        mov edx, dword ptr [ebx + 4]
        mov eax, psrc
        mov esi, dword ptr [eax]
        add esi, edx
        cmp esi, end
        jge L7314
        pop ecx
        dec ecx
        jne L7280
        jmp L73ab
    L7314:
        pop ecx
        dec ecx
        cmp chained, 0
        jne L7347
        cmp looping, 0
        je L73ab
        mov advanced, 0
        mov dword ptr [ebx + 4], 0
        mov eax, psrc
        mov esi, loopStart
        mov dword ptr [eax], esi
        cmp ecx, 0
        jne L7280
        jmp L73ab
    L7347:
        inc wraps
        mov advanced, 0
        mov dword ptr [ebx + 4], 0
        mov eax, psrc
        mov esi, loopStart
        mov dword ptr [eax], esi
        mov eax, next
        mov edx, dword ptr [eax + 0xd4]
        mov end, edx
        mov edx, dword ptr [eax + 0xb4]
        mov next, edx
        cmp edx, 0
        je L738e
        mov eax, dword ptr [edx + 0xd8]
        mov loopStart, eax
        cmp ecx, 0
        jne L7280
        jmp L73ab
    L738e:
        mov chained, 0
        mov looping, 0
        mov edx, dword ptr [eax + 0xd8]
        cmp ecx, 0
        jne L7280
    L73ab:
        mov remain, ecx
        mov eax, dword ptr [ebx + 4]
        mov advanced, eax
        mov dword ptr [ebx + 4], 0
        mov edx, psrc
        mov dword ptr [edx], esi
        mov edx, pdst
        mov dword ptr [edx], edi
        pop edx
        pop ecx
        pop ebx
        pop edi
        pop esi
    }
    if (mode == 1) {
        I32(v, 0x16c) += advanced;
        I32(v, 0x1360) += wraps;
    } else {
        I32(v, 0x88) += advanced;
        I32(v, 0xcc) += wraps;
    }
    return count - remain;
}

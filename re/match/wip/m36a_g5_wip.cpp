// FLAGS sound.dll: /O2
// m36a g5 near misses (WIP, never evidence). The x87 stream renderers and Voice_mix* bodies: C around a literal
// __asm block. Remaining differences are listed per function in log/c3/m36a_g5_notes.txt.

#define I32(p, o) (*(int *)((char *)(p) + (o)))
#define U32(p, o) (*(unsigned int *)((char *)(p) + (o)))
#define U8(p, o) (*(unsigned char *)((char *)(p) + (o)))
#define PTR(p, o) (*(char **)((char *)(p) + (o)))
#define F64(p, o) (*(double *)((char *)(p) + (o)))
#define I64(p, o) (*(__int64 *)((char *)(p) + (o)))

// WIP: sound.dll 0x100184a0 ?streamRenderBlock@@YAHPANPAPAFHPADH@Z 95.9%
int __cdecl streamRenderBlock(double *dst, short **psrc, int count, char *v, int mode)
{
    int end;
    short *loopStart;
    int looping;
    int chained;
    char *next;
    int wraps;
    int newEnd;
    int *endPtr;
    int once;
    int fading;
    int itmp;
    int cw;
    char *voice2;
    int remain;
    char *voice1;
    double pos;
    double ratio;
    double gainA;
    double gainB;
    double fadeStep;

    fadeStep = 0.0;
    chained = 0;
    once = 0;
    fading = 0;
    newEnd = 0;
    endPtr = 0;
    wraps = 0;
    next = 0;
    voice2 = 0;
    if (mode == 1) {
        gainA = 2.0;
        gainB = 2.0;
        voice1 = v;
        char *b = PTR(v, 8);
        end = (U32(b, 0x1c) > U32(b, 0x20)) ? I32(b, 0x24) : I32(b, 0x20);
        loopStart = *(short **)(b + 0x18);
        *(__int64 *)&pos = I64(v, 0x1380);
        ratio = 16000.0 / 44100.0;
        if (I32(v, 0x58) & 8) {
            fading = 1;
            fadeStep = F64(v, 0x1c8) / (double)count;
        }
        looping = ~I32(v, 0x5c);
        looping = (char)looping & 1;
        if (I32(v, 0x58) & 0x20) {
            endPtr = (int *)(b + 0x20);
            newEnd = I32(v, 0x1378);
            once = 1;
        }
    } else {
        *(__int64 *)&gainA = I64(v, 0x80);
        *(__int64 *)&gainB = I64(v, 0x70);
        *(__int64 *)&pos = I64(v, 0x100);
        voice2 = v;
        ratio = 16000.0 / 44100.0;
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
    cw = 0xf7f;
    // asm 0x178..0x2c9
    __asm {
        push esi
        push edi
        push ebx
        push ecx
        push edx
        mov ecx, dword ptr count
        mov edx, dword ptr psrc
        mov esi, dword ptr [edx]
        mov edi, dword ptr dst
        fninit
        fldcw word ptr cw
        fld qword ptr gainB
        fld qword ptr gainA
        fld qword ptr ratio
        fld qword ptr pos
    L8639:
        fadd st, st(1)
        fld st(0)
        fist dword ptr itmp
        mov eax, dword ptr itmp
        shl eax, 1
        add esi, eax
        fisub dword ptr itmp
        cmp esi, dword ptr end
        jge L8697
        fild word ptr [esi]
    L8651:
        add esi, 2
        cmp esi, dword ptr end
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
        cmp dword ptr fading, 0
        add edi, 0x10
        mov esi, dword ptr [edx]
        jne L868e
        jmp L8639
    L868e:
        fld qword ptr fadeStep
        fsub st(3), st(0)
        fsubp st(4), st
        jmp L8639
    L8697:
        cmp dword ptr chained, 0
        jne L86d0
        cmp dword ptr looping, 0
        je L874f
        fxch st(1)
        fstp st(0)
        fld st(0)
        fist dword ptr itmp
        mov eax, dword ptr loopStart
        mov dword ptr [edx], eax
        mov esi, eax
        fild word ptr [esi]
        cmp dword ptr once, 0
        je L8651
        mov dword ptr looping, 0
        mov eax, dword ptr endPtr
        mov ebx, dword ptr newEnd
        mov dword ptr [eax], ebx
        jmp L8651
    L86d0:
        inc dword ptr wraps
        fxch st(1)
        fstp st(0)
        fld st(0)
        fist dword ptr itmp
        mov esi, dword ptr loopStart
        mov dword ptr [edx], esi
        fild word ptr [esi]
        push edx
        mov eax, dword ptr next
        mov edx, dword ptr [eax + 0xd4]
        mov dword ptr end, edx
        mov edx, dword ptr [eax + 0xb4]
        mov dword ptr next, edx
        cmp edx, 0
        je L870d
        mov eax, dword ptr [edx + 0xd8]
        mov dword ptr loopStart, eax
        pop edx
        jmp L8651
    L870d:
        mov dword ptr chained, 0
        mov dword ptr looping, 0
        mov eax, dword ptr [eax + 0xd8]
        pop edx
        jne L8651
        fist dword ptr itmp
    L872b:
        mov word ptr v+2, 0
        cmp dword ptr chained, 0
        jne L873d
        cmp dword ptr looping, 0
        je L8747
    L873d:
        mov eax, dword ptr loopStart
        mov bx, word ptr [eax]
        mov word ptr v+2, bx
    L8747:
        fild word ptr v+2
        jmp L865f
    L874f:
        cmp ecx, 0
        jne L875d
        sub esi, 2
        fisub dword ptr itmp
        fst qword ptr pos
    L875d:
        mov dword ptr remain, ecx
        mov edx, dword ptr psrc
        mov dword ptr [edx], esi
        pop edx
        pop ecx
        pop ebx
        pop edi
        pop esi
    }
    if (mode == 1) {
        F64(voice1, 0x1380) = pos;
        if ((U8(voice1, 0x58) & 0x20) && !looping)
            I32(voice1, 0x5c) |= 1;
    } else {
        F64(voice2, 0x100) = pos;
        I32(voice2, 0xcc) += wraps;
    }
    return count - remain;
}

// WIP: sound.dll 0x100188d0 ?streamRenderTail@@YAHPANPAPAFHPADH@Z 95.3%
int __cdecl streamRenderTail(double *dst, short **psrc, int count, char *v, int mode)
{
    int end;
    short *loopStart;
    int looping;
    int chained;
    char *next;
    int wraps;
    int newEnd;
    int *endPtr;
    int once;
    int fading;
    int itmp;
    int cw;
    char *voice2;
    int remain;
    char *voice1;
    double pos;
    double ratio;
    double gainA;
    double gainB;
    double fadeStep;

    fadeStep = 0.0;
    chained = 0;
    once = 0;
    fading = 0;
    newEnd = 0;
    endPtr = 0;
    wraps = 0;
    next = 0;
    voice2 = 0;
    if (mode == 1) {
        *(__int64 *)&gainA = I64(v, 0x1f0);
        *(__int64 *)&gainB = I64(v, 0x200);
        char *b = PTR(v, 8);
        voice1 = v;
        end = (U32(b, 0x1c) > U32(b, 0x20)) ? I32(b, 0x24) : I32(b, 0x20);
        loopStart = *(short **)(b + 0x18);
        *(__int64 *)&pos = I64(v, 0x1380);
        ratio = 16000.0 / 44100.0;
        if (I32(v, 0x58) & 8) {
            fading = 1;
            fadeStep = F64(v, 0x1c8) / (double)count;
        }
        looping = ~I32(v, 0x5c);
        looping = (char)looping & 1;
        if (I32(v, 0x58) & 0x20) {
            endPtr = (int *)(b + 0x20);
            newEnd = I32(v, 0x1378);
            once = 1;
        }
    } else {
        gainA = 2.0;
        gainB = 2.0;
        ratio = 16000.0 / 44100.0;
        voice2 = v;
        *(__int64 *)&pos = I64(v, 0x100);
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
    cw = 0xf7f;
    // asm 0x185..0x2e1
    __asm {
        push esi
        push edi
        push ebx
        push ecx
        push edx
        mov ecx, dword ptr count
        mov edx, dword ptr psrc
        mov esi, dword ptr [edx]
        mov edi, dword ptr dst
        fninit
        fldcw word ptr cw
        fld qword ptr gainB
        fld qword ptr gainA
        fld qword ptr ratio
        fld qword ptr pos
    L8a76:
        fadd st, st(1)
        fld st(0)
        fist dword ptr itmp
        mov eax, dword ptr itmp
        shl eax, 1
        add esi, eax
        fisub dword ptr itmp
        cmp esi, dword ptr end
        jge L8add
        fild word ptr [esi]
    L8a8e:
        add esi, 2
        cmp esi, dword ptr end
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
        cmp dword ptr fading, 0
        jne L8ad4
        jmp L8a76
    L8ad4:
        fld qword ptr fadeStep
        fsub st(3), st(0)
        fsubp st(4), st
        jmp L8a76
    L8add:
        cmp dword ptr chained, 0
        jne L8b19
        cmp dword ptr looping, 0
        je L8b97
        fxch st(1)
        fstp st(0)
        fld st(0)
        fist dword ptr itmp
        mov eax, dword ptr loopStart
        mov dword ptr [edx], eax
        mov esi, eax
        fild word ptr [esi]
        cmp dword ptr once, 0
        je L8a8e
        mov dword ptr looping, 0
        mov eax, dword ptr endPtr
        mov ebx, dword ptr newEnd
        mov dword ptr [eax], ebx
        jmp L8a8e
    L8b19:
        inc dword ptr wraps
        fxch st(1)
        fstp st(0)
        fld st(0)
        fist dword ptr itmp
        mov esi, dword ptr loopStart
        mov dword ptr [edx], esi
        fild word ptr [esi]
        push edx
        mov eax, dword ptr next
        mov edx, dword ptr [eax + 0xd4]
        mov dword ptr end, edx
        mov edx, dword ptr [eax + 0xb4]
        mov dword ptr next, edx
        cmp edx, 0
        je L8b56
        mov eax, dword ptr [edx + 0xd8]
        mov dword ptr loopStart, eax
        pop edx
        jmp L8a8e
    L8b56:
        mov dword ptr chained, 0
        mov dword ptr looping, 0
        mov eax, dword ptr [eax + 0xd8]
        pop edx
        jmp L8a8e
        fist dword ptr itmp
    L8b73:
        mov word ptr v+2, 0
        cmp dword ptr chained, 0
        jne L8b85
        cmp dword ptr looping, 0
        je L8b8f
    L8b85:
        mov eax, dword ptr loopStart
        mov bx, word ptr [eax]
        mov word ptr v+2, bx
    L8b8f:
        fild word ptr v+2
        jmp L8a9c
    L8b97:
        cmp ecx, 0
        jne L8ba5
        sub esi, 2
        fisub dword ptr itmp
        fst qword ptr pos
    L8ba5:
        mov dword ptr remain, ecx
        mov edx, dword ptr psrc
        mov dword ptr [edx], esi
        pop edx
        pop ecx
        pop ebx
        pop edi
        pop esi
    }
    if (mode == 1) {
        F64(voice1, 0x1380) = pos;
        if ((U8(voice1, 0x58) & 0x20) && !looping)
            I32(voice1, 0x5c) |= 1;
    } else {
        F64(voice2, 0x100) = pos;
        I32(voice2, 0xcc) += wraps;
    }
    return count - remain;
}

// WIP: sound.dll 0x10019770 ?Voice_mixAddMono16@@YAHPANPAPAFHPADH@Z 89.1%
int __cdecl Voice_mixAddMono16(double *dst, short **psrc, int count, char *v, int mode)
{
    int looping;
    int end;
    short *loopStart;
    int fading;
    int newEnd;
    int *endPtr;
    int once;
    int itmp;
    int cw;
    int remain;
    char *voice1;
    char *voice2;
    double pos;
    double fadeStep;
    double posB;
    double gainA;
    double gainB;

    fadeStep = 0.0;
    once = 0;
    fading = 0;
    newEnd = 0;
    endPtr = 0;
    if (mode == 1) {
        *(__int64 *)&gainA = I64(v, 0x1f0);
        *(__int64 *)&gainB = I64(v, 0x200);
        char *b = PTR(v, 8);
        voice1 = v;
        end = (U32(b, 0x1c) > U32(b, 0x20)) ? I32(b, 0x24) : I32(b, 0x20);
        loopStart = *(short **)(b + 0x18);
        *(__int64 *)&pos = I64(v, 0x1380);
        *(__int64 *)&posB = I64(v, 0x1388);
        if (I32(v, 0x58) & 8) {
            fading = 1;
            fadeStep = F64(v, 0x1c8) / (double)count;
        }
        looping = ~I32(v, 0x5c);
        looping = (char)looping & 1;
        if (I32(v, 0x58) & 0x20) {
            endPtr = (int *)(b + 0x20);
            if ((unsigned)*psrc > *(unsigned *)endPtr)
                once = 1;
            else
                looping = 0;
            newEnd = I32(v, 0x1378);
        }
    } else {
        *(__int64 *)&gainA = I64(v, 0x80);
        *(__int64 *)&gainB = I64(v, 0x70);
        *(__int64 *)&posB = I64(v, 0x108);
        voice2 = v;
        *(__int64 *)&pos = I64(v, 0x100);
        unsigned char flags = U8(v, 0x9c);
        if (flags & 8) {
            fading = 1;
            fadeStep = F64(v, 0xf0) / count;
        }
        if ((U8(v, 0x98) & 1) && !(flags & 2)) {
            looping = 1;
            loopStart = *(short **)(v + 0xd8);
            end = I32(v, 0xd4);
        } else {
            looping = 0;
            loopStart = *(short **)(v + 0x18);
            end = I32(v, 0x20);
        }
    }
    cw = 0xf7f;
    // asm 0x18f..0x276
    __asm {
        push esi
        push edi
        push ebx
        push ecx
        push edx
        mov ecx, dword ptr count
        mov edx, dword ptr psrc
        mov esi, dword ptr [edx]
        mov edi, dword ptr dst
        fninit
        fldcw word ptr cw
        fld qword ptr gainB
        fld qword ptr gainA
        fld qword ptr posB
        fld qword ptr pos
    L9920:
        fadd st, st(1)
        fld st(0)
        fist dword ptr itmp
        mov eax, dword ptr itmp
        shl eax, 1
        add esi, eax
        fisub dword ptr itmp
        cmp esi, dword ptr end
        jge L997f
        fild word ptr [esi]
    L9938:
        add esi, 2
        cmp esi, dword ptr end
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
        cmp dword ptr fading, 0
        add edi, 0x10
        mov esi, dword ptr [edx]
        jne L9976
        jmp L9920
    L9976:
        fld qword ptr fadeStep
        fsub st(3), st(0)
        fsubp st(4), st
        jmp L9920
    L997f:
        cmp dword ptr looping, 0
        je L99cc
        fxch st(1)
        fstp st(0)
        fld st(0)
        fist dword ptr itmp
        mov eax, dword ptr loopStart
        mov dword ptr [edx], eax
        mov esi, eax
        fild word ptr [esi]
        cmp dword ptr once, 0
        je L9938
        mov dword ptr looping, 0
        mov eax, dword ptr endPtr
        mov ebx, dword ptr newEnd
        mov dword ptr [eax], ebx
        jmp L9938
    L99ae:
        mov word ptr v+2, 0
        cmp dword ptr looping, 0
        je L99c4
        mov eax, dword ptr loopStart
        mov bx, word ptr [eax]
        mov word ptr v+2, bx
    L99c4:
        fild word ptr v+2
        jmp L9942
    L99cc:
        cmp ecx, 0
        jne L99da
        sub esi, 2
        fisub dword ptr itmp
        fst qword ptr pos
    L99da:
        mov dword ptr remain, ecx
        mov edx, dword ptr psrc
        mov dword ptr [edx], esi
        pop edx
        pop ecx
        pop ebx
        pop edi
        pop esi
    }
    if (mode == 1) {
        F64(voice1, 0x1380) = pos;
        if ((U8(voice1, 0x58) & 0x20) && !looping)
            I32(voice1, 0x5c) |= 1;
    } else {
        F64(voice2, 0x100) = pos;
    }
    return count - remain;
}

// WIP: sound.dll 0x10019b20 ?Voice_mixSetMono16@@YAHPANPAPAFHPADH@Z 88.9%
int __cdecl Voice_mixSetMono16(double *dst, short **psrc, int count, char *v, int mode)
{
    int looping;
    int end;
    short *loopStart;
    int fading;
    int newEnd;
    int *endPtr;
    int once;
    int itmp;
    int cw;
    int remain;
    char *voice1;
    char *voice2;
    double pos;
    double fadeStep;
    double posB;
    double gainA;
    double gainB;

    fadeStep = 0.0;
    once = 0;
    fading = 0;
    newEnd = 0;
    endPtr = 0;
    if (mode == 1) {
        *(__int64 *)&gainA = I64(v, 0x1f0);
        *(__int64 *)&gainB = I64(v, 0x200);
        char *b = PTR(v, 8);
        voice1 = v;
        end = (U32(b, 0x1c) > U32(b, 0x20)) ? I32(b, 0x24) : I32(b, 0x20);
        loopStart = *(short **)(b + 0x18);
        *(__int64 *)&pos = I64(v, 0x1380);
        *(__int64 *)&posB = I64(v, 0x1388);
        if (I32(v, 0x58) & 8) {
            fading = 1;
            fadeStep = F64(v, 0x1c8) / (double)count;
        }
        looping = ~I32(v, 0x5c);
        looping = (char)looping & 1;
        if (I32(v, 0x58) & 0x20) {
            endPtr = (int *)(b + 0x20);
            if ((unsigned)*psrc > *(unsigned *)endPtr)
                once = 1;
            else
                looping = 0;
            newEnd = I32(v, 0x1378);
        }
    } else {
        *(__int64 *)&gainA = I64(v, 0x80);
        *(__int64 *)&gainB = I64(v, 0x70);
        *(__int64 *)&posB = I64(v, 0x108);
        voice2 = v;
        *(__int64 *)&pos = I64(v, 0x100);
        unsigned char flags = U8(v, 0x9c);
        if (flags & 8) {
            fading = 1;
            fadeStep = F64(v, 0xf0) / count;
        }
        if ((U8(v, 0x98) & 1) && !(flags & 2)) {
            looping = 1;
            loopStart = *(short **)(v + 0xd8);
            end = I32(v, 0xd4);
        } else {
            looping = 0;
            loopStart = *(short **)(v + 0x18);
            end = I32(v, 0x20);
        }
    }
    cw = 0xf7f;
    // asm 0x18f..0x26a
    __asm {
        push esi
        push edi
        push ebx
        push ecx
        push edx
        mov ecx, dword ptr count
        mov edx, dword ptr psrc
        mov esi, dword ptr [edx]
        mov edi, dword ptr dst
        fninit
        fldcw word ptr cw
        fld qword ptr gainB
        fld qword ptr gainA
        fld qword ptr posB
        fld qword ptr pos
    L9cd0:
        fadd st, st(1)
        fld st(0)
        fist dword ptr itmp
        mov eax, dword ptr itmp
        shl eax, 1
        add esi, eax
        fisub dword ptr itmp
        cmp esi, dword ptr end
        jge L9d26
        fild word ptr [esi]
    L9ce8:
        add esi, 2
        cmp esi, dword ptr end
        jge L9d55
        fild word ptr [esi]
    L9cf2:
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
        je L9d70
        cmp dword ptr fading, 0
        add edi, 0x10
        mov esi, dword ptr [edx]
        jne L9d1d
        jmp L9cd0
    L9d1d:
        fld qword ptr fadeStep
        fsub st(3), st(0)
        fsubp st(4), st
        jmp L9cd0
    L9d26:
        cmp dword ptr looping, 0
        je L9d70
        fxch st(1)
        fstp st(0)
        fld st(0)
        fist dword ptr itmp
        mov eax, dword ptr loopStart
        mov dword ptr [edx], eax
        mov esi, eax
        fild word ptr [esi]
        cmp dword ptr once, 0
        je L9ce8
        mov dword ptr looping, 0
        mov eax, dword ptr endPtr
        mov ebx, dword ptr newEnd
        mov dword ptr [eax], ebx
        jmp L9ce8
    L9d55:
        mov word ptr v+2, 0
        cmp dword ptr looping, 0
        je L9d6b
        mov eax, dword ptr loopStart
        mov bx, word ptr [eax]
        mov word ptr v+2, bx
    L9d6b:
        fild word ptr v+2
        jmp L9cf2
    L9d70:
        cmp ecx, 0
        jne L9d7e
        sub esi, 2
        fisub dword ptr itmp
        fst qword ptr pos
    L9d7e:
        mov dword ptr remain, ecx
        mov edx, dword ptr psrc
        mov dword ptr [edx], esi
        pop edx
        pop ecx
        pop ebx
        pop edi
        pop esi
    }
    if (mode == 1) {
        F64(voice1, 0x1380) = pos;
        if ((U8(voice1, 0x58) & 0x20) && !looping)
            I32(voice1, 0x5c) |= 1;
    } else {
        F64(voice2, 0x100) = pos;
    }
    return count - remain;
}

// WIP: sound.dll 0x10019ec0 ?Voice_mixSet8bit@@YAHPANPAPAEHPADH@Z 89.5%
int __cdecl Voice_mixSet8bit(double *dst, unsigned char **psrc, int count, char *v, int mode)
{
    int looping;
    int end;
    unsigned char *loopStart;
    int fading;
    int newEnd;
    int *endPtr;
    int once;
    int sample;
    int itmp;
    int cw;
    int remain;
    char *voice1;
    char *voice2;
    double pos;
    double fadeStep;
    double posB;
    double gainA;
    double gainB;

    fadeStep = 0.0;
    once = 0;
    fading = 0;
    newEnd = 0;
    endPtr = 0;
    sample = 0;
    if (mode == 1) {
        *(__int64 *)&gainA = I64(v, 0x1f0);
        *(__int64 *)&gainB = I64(v, 0x200);
        char *b = PTR(v, 8);
        voice1 = v;
        end = (U32(b, 0x1c) > U32(b, 0x20)) ? I32(b, 0x24) : I32(b, 0x20);
        loopStart = *(unsigned char **)(b + 0x18);
        *(__int64 *)&pos = I64(v, 0x1380);
        *(__int64 *)&posB = I64(v, 0x1388);
        if (I32(v, 0x58) & 8) {
            fading = 1;
            fadeStep = F64(v, 0x1c8) / (double)count;
        }
        looping = ~I32(v, 0x5c);
        looping = (char)looping & 1;
        if (I32(v, 0x58) & 0x20) {
            endPtr = (int *)(b + 0x20);
            if ((unsigned)*psrc > *(unsigned *)endPtr)
                once = 1;
            else
                looping = 0;
            newEnd = I32(v, 0x1378);
        }
    } else {
        *(__int64 *)&gainA = I64(v, 0x80);
        *(__int64 *)&gainB = I64(v, 0x70);
        *(__int64 *)&posB = I64(v, 0x108);
        voice2 = v;
        *(__int64 *)&pos = I64(v, 0x100);
        unsigned char flags = U8(v, 0x9c);
        if (flags & 8) {
            fading = 1;
            fadeStep = F64(v, 0xf0) / count;
        }
        if ((U8(v, 0x98) & 1) && !(flags & 2)) {
            looping = 1;
            loopStart = *(unsigned char **)(v + 0xd8);
            end = I32(v, 0xd4);
        } else {
            looping = 0;
            loopStart = *(unsigned char **)(v + 0x18);
            end = I32(v, 0x20);
        }
    }
    cw = 0xf7f;
    // asm 0x192..0x2b8
    __asm {
        push esi
        push edi
        push ebx
        push ecx
        push edx
        mov ecx, dword ptr count
        mov edx, dword ptr psrc
        mov esi, dword ptr [edx]
        mov edi, dword ptr dst
        fninit
        fldcw word ptr cw
        fld qword ptr gainB
        fld qword ptr gainA
        fld qword ptr posB
        fld qword ptr pos
    La073:
        fadd st, st(1)
        fld st(0)
        fist dword ptr itmp
        mov eax, dword ptr itmp
        add esi, eax
        fisub dword ptr itmp
        cmp esi, dword ptr end
        jge La0f1
        movzx eax, byte ptr [esi]
        shl eax, 8
        sub eax, 0x8000
        mov word ptr sample, ax
        fild word ptr sample
    La099:
        inc esi
        cmp esi, dword ptr end
        jge La137
        movzx eax, byte ptr [esi]
        shl eax, 8
        sub eax, 0x8000
        mov word ptr sample, ax
        mov word ptr sample, ax
        fild word ptr sample
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
        cmp dword ptr fading, 0
        add edi, 0x10
        mov esi, dword ptr [edx]
        jne La0e8
        jmp La073
    La0e8:
        fld qword ptr fadeStep
        fsub st(3), st(0)
        fsubp st(4), st
        jmp La073
    La0f1:
        cmp dword ptr looping, 0
        je La15e
        fxch st(1)
        fstp st(0)
        fld st(0)
        fist dword ptr itmp
        mov eax, dword ptr loopStart
        mov dword ptr [edx], eax
        mov esi, eax
        movzx eax, byte ptr [esi]
        shl eax, 8
        sub eax, 0x8000
        mov word ptr sample, ax
        fild word ptr sample
        cmp dword ptr once, 0
        je La099
        mov dword ptr looping, 0
        mov eax, dword ptr endPtr
        mov ebx, dword ptr newEnd
        mov dword ptr [eax], ebx
        jmp La099
    La137:
        mov word ptr v+2, 0
        cmp dword ptr looping, 0
        je La156
        mov eax, dword ptr loopStart
        movzx ebx, byte ptr [eax]
        shl ebx, 8
        sub ebx, 0x8000
        mov word ptr v+2, bx
    La156:
        fild word ptr v+2
        jmp La0b9
    La15e:
        cmp ecx, 0
        jne La16c
        sub esi, 1
        fisub dword ptr itmp
        fst qword ptr pos
    La16c:
        mov dword ptr remain, ecx
        mov edx, dword ptr psrc
        mov dword ptr [edx], esi
        pop edx
        pop ecx
        pop ebx
        pop edi
        pop esi
    }
    if (mode == 1) {
        F64(voice1, 0x1380) = pos;
        if ((U8(voice1, 0x58) & 0x20) && !looping)
            I32(voice1, 0x5c) |= 1;
    } else {
        F64(voice2, 0x100) = pos;
    }
    return count - remain;
}

// WIP: sound.dll 0x1001a2c0 ?Voice_mixAdd8bit@@YAHPANPAPAEHPADH@Z 89.7%
int __cdecl Voice_mixAdd8bit(double *dst, unsigned char **psrc, int count, char *v, int mode)
{
    int looping;
    int end;
    unsigned char *loopStart;
    int fading;
    int newEnd;
    int *endPtr;
    int once;
    int sample;
    int itmp;
    int cw;
    int remain;
    char *voice1;
    char *voice2;
    double pos;
    double fadeStep;
    double posB;
    double gainA;
    double gainB;

    fadeStep = 0.0;
    once = 0;
    fading = 0;
    newEnd = 0;
    endPtr = 0;
    sample = 0;
    if (mode == 1) {
        *(__int64 *)&gainA = I64(v, 0x1f0);
        *(__int64 *)&gainB = I64(v, 0x200);
        char *b = PTR(v, 8);
        voice1 = v;
        end = (U32(b, 0x1c) > U32(b, 0x20)) ? I32(b, 0x24) : I32(b, 0x20);
        loopStart = *(unsigned char **)(b + 0x18);
        *(__int64 *)&pos = I64(v, 0x1380);
        *(__int64 *)&posB = I64(v, 0x1388);
        if (I32(v, 0x58) & 8) {
            fading = 1;
            fadeStep = F64(v, 0x1c8) / (double)count;
        }
        looping = ~I32(v, 0x5c);
        looping = (char)looping & 1;
        if (I32(v, 0x58) & 0x20) {
            endPtr = (int *)(b + 0x20);
            if ((unsigned)*psrc > *(unsigned *)endPtr)
                once = 1;
            else
                looping = 0;
            newEnd = I32(v, 0x1378);
        }
    } else {
        *(__int64 *)&gainA = I64(v, 0x80);
        *(__int64 *)&gainB = I64(v, 0x70);
        *(__int64 *)&posB = I64(v, 0x108);
        voice2 = v;
        *(__int64 *)&pos = I64(v, 0x100);
        unsigned char flags = U8(v, 0x9c);
        if (flags & 8) {
            fading = 1;
            fadeStep = F64(v, 0xf0) / count;
        }
        if ((U8(v, 0x98) & 1) && !(flags & 2)) {
            looping = 1;
            loopStart = *(unsigned char **)(v + 0xd8);
            end = I32(v, 0xd4);
        } else {
            looping = 0;
            loopStart = *(unsigned char **)(v + 0x18);
            end = I32(v, 0x20);
        }
    }
    cw = 0xf7f;
    // asm 0x192..0x2c4
    __asm {
        push esi
        push edi
        push ebx
        push ecx
        push edx
        mov ecx, dword ptr count
        mov edx, dword ptr psrc
        mov esi, dword ptr [edx]
        mov edi, dword ptr dst
        fninit
        fldcw word ptr cw
        fld qword ptr gainB
        fld qword ptr gainA
        fld qword ptr posB
        fld qword ptr pos
    La473:
        fadd st, st(1)
        fld st(0)
        fist dword ptr itmp
        mov eax, dword ptr itmp
        add esi, eax
        fisub dword ptr itmp
        cmp esi, dword ptr end
        jge La4fd
        movzx eax, byte ptr [esi]
        shl eax, 8
        sub eax, 0x8000
        mov word ptr sample, ax
        fild word ptr sample
    La499:
        inc esi
        cmp esi, dword ptr end
        jge La543
        movzx eax, byte ptr [esi]
        shl eax, 8
        sub eax, 0x8000
        mov word ptr sample, ax
        mov word ptr sample, ax
        fild word ptr sample
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
        cmp dword ptr fading, 0
        add edi, 0x10
        mov esi, dword ptr [edx]
        jne La4f1
        jmp La473
    La4f1:
        fld qword ptr fadeStep
        fsub st(3), st(0)
        fsubp st(4), st
        jmp La473
    La4fd:
        cmp dword ptr looping, 0
        je La56a
        fxch st(1)
        fstp st(0)
        fld st(0)
        fist dword ptr itmp
        mov eax, dword ptr loopStart
        mov dword ptr [edx], eax
        mov esi, eax
        movzx eax, byte ptr [esi]
        shl eax, 8
        sub eax, 0x8000
        mov word ptr sample, ax
        fild word ptr sample
        cmp dword ptr once, 0
        je La499
        mov dword ptr looping, 0
        mov eax, dword ptr endPtr
        mov ebx, dword ptr newEnd
        mov dword ptr [eax], ebx
        jmp La499
    La543:
        mov word ptr v+2, 0
        cmp dword ptr looping, 0
        je La562
        mov eax, dword ptr loopStart
        movzx ebx, byte ptr [eax]
        shl ebx, 8
        sub ebx, 0x8000
        mov word ptr v+2, bx
    La562:
        fild word ptr v+2
        jmp La4b9
    La56a:
        cmp ecx, 0
        jne La578
        sub esi, 1
        fisub dword ptr itmp
        fst qword ptr pos
    La578:
        mov dword ptr remain, ecx
        mov edx, dword ptr psrc
        mov dword ptr [edx], esi
        pop edx
        pop ecx
        pop ebx
        pop edi
        pop esi
    }
    if (mode == 1) {
        F64(voice1, 0x1380) = pos;
        if ((U8(voice1, 0x58) & 0x20) && !looping)
            I32(voice1, 0x5c) |= 1;
    } else {
        F64(voice2, 0x100) = pos;
    }
    return count - remain;
}

// WIP: sound.dll 0x1001a6c0 ?Voice_mixSetStereo16@@YAHPANPAPAFHPADH@Z 85.6%
int __cdecl Voice_mixSetStereo16(double *dst, short **psrc, int count, char *v, int mode)
{
    int looping;
    int end;
    short *loopStart;
    int fading;
    int once;
    int cw;
    int remain;
    char *voice1;
    double fadeStep;
    double gainA;
    double gainB;

    fadeStep = 0.0;
    once = 0;
    fading = 0;
    if (mode == 1) {
        *(__int64 *)&gainA = I64(v, 0x1f0);
        *(__int64 *)&gainB = I64(v, 0x200);
        char *b = PTR(v, 8);
        voice1 = v;
        if (U32(b, 0x1c) > U32(b, 0x20))
            end = I32(b, 0x24);
        else
            end = I32(b, 0x20);
        loopStart = *(short **)(b + 0x18);
        if (I32(v, 0x58) & 8) {
            fading = 1;
            fadeStep = F64(v, 0x1c8) / (double)count;
        }
        looping = ~I32(v, 0x5c);
        looping = (char)looping & 1;
        if (I32(v, 0x58) & 0x20) {
            if ((unsigned)*psrc > U32(b, 0x20))
                once = 1;
            else
                looping = 0;
        }
    } else {
        *(__int64 *)&gainA = I64(v, 0x80);
        *(__int64 *)&gainB = I64(v, 0x70);
        unsigned char flags = U8(v, 0x9c);
        if (flags & 8) {
            fading = 1;
            fadeStep = F64(v, 0xf0) / count;
        }
        if ((U8(v, 0x98) & 1) && !(flags & 2)) {
            looping = 1;
            loopStart = *(short **)(v + 0xd8);
            end = I32(v, 0xd4);
        } else {
            looping = 0;
            loopStart = *(short **)(v + 0x18);
            end = I32(v, 0x20);
        }
    }
    cw = 0xf7f;
    // asm 0x11f..0x195
    __asm {
        push esi
        push edi
        push ebx
        push ecx
        push edx
        mov ecx, dword ptr count
        mov edx, dword ptr psrc
        mov esi, dword ptr [edx]
        mov edi, dword ptr dst
        fninit
        fldcw word ptr cw
        fld qword ptr gainB
        fld qword ptr gainA
    La7fa:
        cmp esi, dword ptr end
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
        cmp dword ptr fading, 0
        jne La824
        jmp La7fa
    La824:
        fld qword ptr fadeStep
        fsub st(1), st(0)
        fsubp st(2), st
        jmp La7fa
    La82d:
        cmp dword ptr looping, 0
        je La849
        mov eax, dword ptr loopStart
        mov dword ptr [edx], eax
        mov esi, eax
        cmp dword ptr once, 0
        je La7fa
        mov dword ptr looping, 0
        jmp La7fa
    La849:
        mov dword ptr remain, ecx
        mov edx, dword ptr psrc
        mov dword ptr [edx], esi
        pop edx
        pop ecx
        pop ebx
        pop edi
        pop esi
    }
    if (mode == 1) {
        if ((U8(voice1, 0x58) & 0x20) && !looping)
            I32(voice1, 0x5c) |= 1;
    }
    return count - remain;
}

// WIP: sound.dll 0x1001a910 ?Voice_mixAddStereo16@@YAHPANPAPAFHPADH@Z 86.0%
int __cdecl Voice_mixAddStereo16(double *dst, short **psrc, int count, char *v, int mode)
{
    int looping;
    int end;
    short *loopStart;
    int fading;
    int once;
    int cw;
    int remain;
    char *voice1;
    double fadeStep;
    double gainA;
    double gainB;

    fadeStep = 0.0;
    once = 0;
    fading = 0;
    if (mode == 1) {
        *(__int64 *)&gainA = I64(v, 0x1f0);
        *(__int64 *)&gainB = I64(v, 0x200);
        char *b = PTR(v, 8);
        voice1 = v;
        if (U32(b, 0x1c) > U32(b, 0x20))
            end = I32(b, 0x24);
        else
            end = I32(b, 0x20);
        loopStart = *(short **)(b + 0x18);
        if (I32(v, 0x58) & 8) {
            fading = 1;
            fadeStep = F64(v, 0x1c8) / (double)count;
        }
        looping = ~I32(v, 0x5c);
        looping = (char)looping & 1;
        if (I32(v, 0x58) & 0x20) {
            if ((unsigned)*psrc > U32(b, 0x20))
                once = 1;
            else
                looping = 0;
        }
    } else {
        *(__int64 *)&gainA = I64(v, 0x80);
        *(__int64 *)&gainB = I64(v, 0x70);
        unsigned char flags = U8(v, 0x9c);
        if (flags & 8) {
            fading = 1;
            fadeStep = F64(v, 0xf0) / count;
        }
        if ((U8(v, 0x98) & 1) && !(flags & 2)) {
            looping = 1;
            loopStart = *(short **)(v + 0xd8);
            end = I32(v, 0xd4);
        } else {
            looping = 0;
            loopStart = *(short **)(v + 0x18);
            end = I32(v, 0x20);
        }
    }
    cw = 0xf7f;
    // asm 0x11f..0x19e
    __asm {
        push esi
        push edi
        push ebx
        push ecx
        push edx
        mov ecx, dword ptr count
        mov edx, dword ptr psrc
        mov esi, dword ptr [edx]
        mov edi, dword ptr dst
        fninit
        fldcw word ptr cw
        fld qword ptr gainB
        fld qword ptr gainA
    Laa4a:
        cmp esi, dword ptr end
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
        cmp dword ptr fading, 0
        jne Laa7d
        jmp Laa4a
    Laa7d:
        fld qword ptr fadeStep
        fsub st(1), st(0)
        fsubp st(2), st
        jmp Laa4a
    Laa86:
        cmp dword ptr looping, 0
        je Laaa2
        mov eax, dword ptr loopStart
        mov dword ptr [edx], eax
        mov esi, eax
        cmp dword ptr once, 0
        je Laa4a
        mov dword ptr looping, 0
        jmp Laa4a
    Laaa2:
        mov dword ptr remain, ecx
        mov edx, dword ptr psrc
        mov dword ptr [edx], esi
        pop edx
        pop ecx
        pop ebx
        pop edi
        pop esi
    }
    if (mode == 1) {
        if ((U8(voice1, 0x58) & 0x20) && !looping)
            I32(voice1, 0x5c) |= 1;
    }
    return count - remain;
}

#include "jgldefs.h"
#include "MiscUtil.h"
#include <ctype.h>
#include <stdlib.h>

int btoi(const char *s)
{
    int value = 0;
    while (*s == '0' || *s == '1') {
        value <<= 1;
        value += *s - '0';
        s++;
    }
    return value;
}

int xtoi(const char *s)
{
    int value = 0;
    while (isxdigit(*s)) {
        value <<= 4;
        if (isdigit(*s))
            value += *s - '0';
        else
            value += toupper(*s) - 'A' + 10;
        s++;
    }
    return value;
}

int stoi(const char *s)
{
    if (*s == '0') {
        s++;
        switch (*s) {
        case 'B':
        case 'b': return btoi(s + 1);
        case 'X':
        case 'x': return xtoi(s + 1);
        case 'D':
        case 'd': s++; break;
        }
    }
    return atoi(s);
}

void swap(int *a, int *b)
{
    if (a == b)
        return;
    *a ^= *b;
    *b ^= *a;
    *a ^= *b;
}

char *memrchr(char *start, char *from, char wanted)
{
    char *result;
    __asm {
        mov     ebx, start
        and     ebx, ebx
        jz      none
        mov     ecx, from
        and     ecx, ecx
        jz      none
        mov     eax, ecx
        sub     ecx, ebx
        and     ecx, ecx
        jz      none
        mov     bl, wanted
    scan:
        cmp     byte ptr [eax], bl
        je      found
        dec     eax
        dec     ecx
        jnz     scan
    none:
        xor     eax, eax
    found:
        mov     result, eax
    }
    return result;
}

char *findnum(char *s)
{
    char *result;
    __asm {
        mov     eax, s
        and     eax, eax
        jz      no_digit
        mov     dl, byte ptr g_digitRange[0]
        mov     dh, byte ptr g_digitRange[1]
        xor     ebx, ebx
        dec     eax
    scan:
        inc     eax
        mov     bl, byte ptr [eax]
        and     bl, bl
        jz      no_digit
        cmp     bl, dl
        jl      scan
        cmp     bl, dh
        jg      scan
        jmp     have_digit
    no_digit:
        xor     eax, eax
    have_digit:
        mov     result, eax
    }
    return result;
}

int quick_root(int value)
{
    int root = value >> 1;
    int last;
    if (value <= 1)
        return value;
    do {
        last = root;
        root = (value / root + root) >> 1;
    } while (root < last);
    return last;
}

extern "C" void memsetword(ushort *dst, ushort val, int count)
{
    int n;
    unsigned long *p;
    unsigned long dw;
    dw = ((unsigned long)val << 16) | val;
    if (count & 1) {
        *dst = (unsigned short)val;
        p = (unsigned long *)(dst + 1);
    } else {
        p = (unsigned long *)dst;
    }
    for (n = count / 2; n > 0; n--) {
        *p = dw;
        p++;
    }
}

const char g_digitRange[] = "09";

// FLAGS sound.dll: /O1
// m36c (round 36): sound.dll static CRT routines whose VC6 source is MASM (strcat.asm / strstr.asm / ftol.asm), written
// as naked inline-asm transcriptions of the disassembly (the golf_hand_03.cpp precedent). For these the match shows
// byte identity with a hand transcription, not a recovered C source: the verifier decides whether that counts.
// Jumps that leave a routine for another entry point of the same MASM source are emitted as their original short
// encodings (`_emit`): an inline-asm jump to an external label is always assembled as a near jump.
#include <windows.h>

// 0x1004a010 (7 bytes): strcpy's entry, then the copy loop it shares with strcat at 0x1004a081
// MATCH: sound.dll 0x1004a010 ?m36c_strcpy_4a010@@YAXXZ
__declspec(naked) void m36c_strcpy_4a010(void)
{
    __asm {
        push edi
        mov edi, dword ptr [esp + 8]
        _emit 0xeb                  // jmp short 0x1004a081
        _emit 0x6a
    }
}

// 0x10042550: double in st(0) -> 64-bit integer in edx:eax, truncating (control word | 0x0c00 for the fistp)
// MATCH: sound.dll 0x10042550 ?m36c_ftol_42550@@YAXXZ
__declspec(naked) void m36c_ftol_42550(void)
{
    __asm {
        push ebp
        mov ebp, esp
        add esp, -0xc
        wait
        fnstcw word ptr [ebp - 2]
        wait
        mov ax, word ptr [ebp - 2]
        or ah, 0xc
        mov word ptr [ebp - 4], ax
        fldcw word ptr [ebp - 4]
        fistp qword ptr [ebp - 0xc]
        fldcw word ptr [ebp - 2]
        mov eax, dword ptr [ebp - 0xc]
        mov edx, dword ptr [ebp - 8]
        leave
        ret
    }
}

// 0x10042690: strstr; a one-character pattern continues in strchr's body at 0x10045686 with the character in al
extern "C" void x_10045686(void);
// MATCH: sound.dll 0x10042690 ?m36c_strstr_42690@@YAXXZ
__declspec(naked) void m36c_strstr_42690(void)
{
    __asm {
        mov ecx, dword ptr [esp + 8]
        push edi
        push ebx
        push esi
        mov dl, byte ptr [ecx]
        mov edi, dword ptr [esp + 0x10]
        test dl, dl
        je S_7a
        mov dh, byte ptr [ecx + 1]
        test dh, dh
        je S_67
S_18:   mov esi, edi
        mov ecx, dword ptr [esp + 0x14]
        mov al, byte ptr [edi]
        inc esi
        cmp al, dl
        je S_3a
        test al, al
        je S_34
S_29:   mov al, byte ptr [esi]
        inc esi
S_2c:   cmp al, dl
        je S_3a
        test al, al
        jne S_29
S_34:   pop esi
        pop ebx
        pop edi
        xor eax, eax
        ret
S_3a:   mov al, byte ptr [esi]
        inc esi
        cmp al, dh
        jne S_2c
        lea edi, [esi - 1]
S_44:   mov ah, byte ptr [ecx + 2]
        test ah, ah
        je S_73
        mov al, byte ptr [esi]
        add esi, 2
        cmp al, ah
        jne S_18
        mov al, byte ptr [ecx + 3]
        test al, al
        je S_73
        mov ah, byte ptr [esi - 1]
        add ecx, 2
        cmp al, ah
        je S_44
        jmp S_18
S_67:   xor eax, eax
        pop esi
        pop ebx
        pop edi
        mov al, dl
        jmp x_10045686
S_73:   lea eax, [edi - 1]
        pop esi
        pop ebx
        pop edi
        ret
S_7a:   mov eax, edi
        pop esi
        pop ebx
        pop edi
        ret
    }
}

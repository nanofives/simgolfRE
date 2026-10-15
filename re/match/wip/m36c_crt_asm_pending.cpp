// FLAGS sound.dll: /O1
// m36c (round 36): sound.dll strchr 0x10045680 (MASM source strchr.asm), a naked inline-asm transcription like
// m36c_crt_asm.cpp. STAGED, NO MATCH LINE YET: its `je` at 0x1004569d goes to 0x10045670, the return stub 0x10 bytes
// BEFORE this entry (emitted as the original short encoding 74 d1). re/tools/match.py disassembles the obj at base 0,
// so that branch reads `je +0xfffffff0` on the obj side and `je +0x-10` on the original side: 98.8% (85/86). With
// normalize() taking the obj's branch offset as signed 32-bit it reads 100% (86/86), measured with
// log/c3/m36c_signed_branch_check.py (the fix applied in memory). Verifier: fix match.py, then replace the word PENDING
// on the next line by the word MATCH.
// PENDING: sound.dll 0x10045680 ?m36c_strchr_45680@@YAXXZ
#include <windows.h>

__declspec(naked) void m36c_strchr_45680(void)
{
    __asm {
        xor eax, eax
        mov al, byte ptr [esp + 8]
        push ebx
        mov ebx, eax
        shl eax, 8
        mov edx, dword ptr [esp + 8]
        test edx, 3
        je L_ab
L_98:   mov cl, byte ptr [edx]
        inc edx
        cmp cl, bl
        _emit 0x74                  // je short 0x10045670 (strchr.asm's return stub before this entry)
        _emit 0xd1
        test cl, cl
        je L_f4
        test edx, 3
        jne L_98
L_ab:   or ebx, eax
        push edi
        mov eax, ebx
        shl ebx, 0x10
        push esi
        or ebx, eax
L_b6:   mov ecx, dword ptr [edx]
        mov edi, 0x7efefeff
        mov eax, ecx
        mov esi, edi
        xor ecx, ebx
        add esi, eax
        add edi, ecx
        xor ecx, 0xffffffff
        xor eax, 0xffffffff
        xor ecx, edi
        xor eax, esi
        add edx, 4
        and ecx, 0x81010100
        jne L_f8
        and eax, 0x81010100
        je L_b6
        and eax, 0x1010100
        jne L_f2
        and esi, 0x80000000
        jne L_b6
L_f2:   pop esi
        pop edi
L_f4:   pop ebx
        xor eax, eax
        ret
L_f8:   mov eax, dword ptr [edx - 4]
        cmp al, bl
        je L_135
        test al, al
        je L_f2
        cmp ah, bl
        je L_12e
        test ah, ah
        je L_f2
        shr eax, 0x10
        cmp al, bl
        je L_127
        test al, al
        je L_f2
        cmp ah, bl
        je L_120
        test ah, ah
        je L_f2
        jmp L_b6
L_120:  pop esi
        pop edi
        lea eax, [edx - 1]
        pop ebx
        ret
L_127:  lea eax, [edx - 2]
        pop esi
        pop edi
        pop ebx
        ret
L_12e:  lea eax, [edx - 3]
        pop esi
        pop edi
        pop ebx
        ret
L_135:  lea eax, [edx - 4]
        pop esi
        pop edi
        pop ebx
        ret
    }
}

// FLAGS sound.dll: /O1
// m36c (round 36): sound.dll's eh vector constructor iterator 0x10042902 (hooks.csv ehVectorConstructorIterator),
// hand-written in the shape of VC6's ehvccctr.cpp (__try / __finally). STAGED, NO MATCH LINE YET: VC6 emits the
// __finally funclet inside the same symbol, right after `ret 0x14`, and the original has it there too (0x1004296c..
// 0x10042983, the held row ehVectorCtor_finally: cmp [ebp-0x20],0 / jne / 4 pushes / call 0x1004288e / ret), but
// Ghidra's extent stops at 0x1004296c (106 bytes), so match.py reads 81.8% (36/44: the 8 funclet instructions extra).
// With a re/match/boundaries.tsv row `sound.dll 0x10042902 130` (106 + 24) it reads 100% (44/44), measured with
// log/c3/m36c_bound_check.py. Verifier: add that row (shared file), then replace the word PENDING on the next line
// by the word MATCH.
// MATCH: sound.dll 0x10042902 ?m36c_ehvec_ctor@@YGXPAXIHP8Obj@@AEXXZ1@Z
// A pointer to member of an empty single-inheritance struct stands for the original's __thiscall function pointers
// (VC6 rejects `__thiscall` in a declarator: C4234); the call is `mov ecx, esi; call [ebp+0x14]` either way.
#include <windows.h>

struct Obj { };
typedef void (Obj::*PFN)();
void __stdcall f_1004288e(void *ptr, unsigned size, int count, PFN pDtor);

void __stdcall m36c_ehvec_ctor(void *ptr, unsigned size, int count, PFN pCtor, PFN pDtor)
{
    int i;
    int success = 0;
    __try
    {
        for (i = 0; i < count; i++)
        {
            (((Obj *)ptr)->*pCtor)();
            ptr = (char *)ptr + size;
        }
        success = 1;
    }
    __finally
    {
        if (!success)
            f_1004288e(ptr, size, i, pDtor);
    }
}

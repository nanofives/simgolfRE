// FLAGS golf_clean.exe: /O1
// m36d near miss (round 36): the static CRT's `eh vector constructor iterator' 0x004a55d2 in the shape of VC6's
// ehvecctr.cpp. match.py: 81.8% (36/44). All 36 instructions of the original's extent match in order; the 8 extra are
// the __finally funclet, which VC6 emits inside the same COMDAT after the body while the original's funclet is its own
// entry 0x004a563c (ehVectorCtor_finally, held in re/c3_hold.tsv). A 100% needs match.py to stop the obj slice at the
// funclet (a tool change for the verifier), not a source change.
struct EhObj { void f(); };
typedef void (EhObj::*EhFn)();
void __stdcall __ArrayUnwind(void*, unsigned, int, EhFn);
// MATCH: golf_clean.exe 0x004a55d2 ?ehVectorCtor@@YGXPAXIHP8EhObj@@AEXXZ1@Z
void __stdcall ehVectorCtor(void* ptr, unsigned size, int count, EhFn pCtor, EhFn pDtor)
{
    int i;
    int success = 0;
    __try {
        for (i = 0; i < count; i++) {
            (((EhObj*)ptr)->*pCtor)();
            ptr = (char*)ptr + size;
        }
        success = 1;
    }
    __finally {
        if (!success)
            __ArrayUnwind(ptr, size, i, pDtor);
    }
}

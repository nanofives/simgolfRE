// FLAGS golf_clean.exe: /O1
// S10.12: the static CRT's `eh vector constructor iterator' 0x004a55d2 (VC6 ehvecctr.cpp shape), copy of
// re/match/wip/m36d_wip_ehvec.cpp (unchanged source). It reads 81.8% (36/44) with Ghidra's extent 106 bytes
// (0x004a55d2..0x004a563c) because VC6 puts the __finally funclet in the same COMDAT after the body, while the
// original's funclet is the contiguous entry 0x004a563c (24 bytes, 0x004a563c..0x004a5654, ehVectorCtor_finally).
// With the extent 130 bytes (body + funclet) it reads 100.0% (44/44): log/roadmap/S10/12_ehvec_extent130.log.
// CANONICAL ONLY WITH one boundaries.tsv row (log/roadmap/S10/12_merge.md):
//   golf_clean.exe  0x004a55d2  130  body 0x4a55d2..0x4a563c plus its contiguous __finally funclet 0x4a563c..0x4a5654
// Until that row exists this file stays in wip/ (the match tier reads canonical files only).
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

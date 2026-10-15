// FLAGS sound.dll: /O2
// m36a group g1: atexitDtor_100b49b8, moved out of re/match/wip/m36a_g1_wip.cpp once re/match/boundaries.tsv gave it
// its 10-byte extent (Ghidra counted 5 trailing nop pad bytes; verifier, round 36).

namespace m36a_91f0 {
struct SoundWindowObj {
    int x;
    ~SoundWindowObj();
};
extern SoundWindowObj g_soundWindowObj;   // 0x100b49b8

// mov ecx, 0x100b49b8; jmp SoundWindowObj_dtor
// MATCH: sound.dll 0x100091f0 ?atexitDtor@m36a_91f0@@YAXXZ
void atexitDtor()
{
    g_soundWindowObj.~SoundWindowObj();
}
}

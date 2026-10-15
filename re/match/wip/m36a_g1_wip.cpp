// FLAGS sound.dll: /O2
// m36a group g1 near misses (WIP lines are never read as evidence).

namespace m36a_91f0 {
struct SoundWindowObj {
    int x;
    ~SoundWindowObj();
};
extern SoundWindowObj g_soundWindowObj;   // 0x100b49b8

// Both instructions (mov ecx, 0x100b49b8; jmp SoundWindowObj_dtor) are equal; the 28.6% comes from the 5 trailing nop
// pad bytes Ghidra counts in the 15-byte extent (match.py strips pads only on the compiled side). A boundaries.tsv row
// `sound.dll 0x100091f0 10` would make this 100%.
// WIP: sound.dll 0x100091f0 ?atexitDtor@m36a_91f0@@YAXXZ 28.6
void atexitDtor()
{
    g_soundWindowObj.~SoundWindowObj();
}
}

#include <windows.h>
#include <string.h>

namespace m36a_6f00 {
struct Elem {
    int a, b;
};
struct DynArray {
    int m_0;
    Elem *data;          // 0x04
    BOOL locked : 1;     // 0x08
    unsigned count;      // 0x0c
    int resize(unsigned n);
};

// Remaining diff: the original zeroes both dwords from one register hoisted before the loop (xor ecx, ecx, also
// used by the guard `cmp ebx, ecx`); this spelling keeps the xor inside the loop and computes a lea.
// A memset'd local Elem copied per element gives the original's loop body exactly but allocates n in edi (68.5%).
// WIP: sound.dll 0x10006f00 ?resize@DynArray@m36a_6f00@@QAEHI@Z 85.5
int DynArray::resize(unsigned n)
{
    if (n == count)
        return 0;
    BOOL saved = locked;
    if (saved == TRUE)
        locked = FALSE;
    if (data)
        operator delete(data);
    data = 0;
    data = (Elem *)operator new(n * sizeof(Elem));
    count = n;
    for (unsigned i = 0; i < count; i++) {
        memset(&data[i], 0, sizeof(Elem));
    }
    locked = saved;
    return 0;
}
}

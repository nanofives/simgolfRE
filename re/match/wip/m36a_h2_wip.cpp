// FLAGS sound.dll: /O2
// m36a (round 36) second pass, group h2: near misses (WIP lines, never evidence). To re-test one, copy it to a scratch
// file with its WIP line changed to MATCH and run re/tools/match.py --no-evidence.

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

// 87.0%: the loop body is exact (one zero register stored to both dwords) but the original hoists `xor ecx, ecx` above
// the loop guard and compares `cmp ebx, ecx`; this build keeps the xor inside the loop and tests `test ebx, ebx`.
// Hoisting the zero local out of the loop (memset'd Elem or {0, 0}, at any position) gives the hoisted xor but swaps
// n and saved between ebx and edi (68.5%). ~150 variants tried (see log/c3/m36a_h2_notes.txt).
// WIP: sound.dll 0x10006f00 ?resize@DynArray@m36a_6f00@@QAEHI@Z 87.0
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
        Elem z;
        memset(&z, 0, sizeof z);
        data[i] = z;
    }
    locked = saved;
    return 0;
}
}

namespace m36a_101a0 {
typedef unsigned long DWORD;
typedef long HRESULT;
struct IDsBuf {
    virtual HRESULT __stdcall QueryInterface(const void *iid, void **out);
    virtual DWORD __stdcall AddRef();
    virtual DWORD __stdcall Release();
    virtual HRESULT __stdcall GetCaps(void *caps);
    virtual HRESULT __stdcall GetCurrentPosition(DWORD *play, DWORD *write);
    virtual HRESULT __stdcall GetFormat(void *fmt, DWORD size, DWORD *got);
    virtual HRESULT __stdcall GetVolume(long *vol);
    virtual HRESULT __stdcall GetPan(long *pan);
    virtual HRESULT __stdcall GetFrequency(DWORD *freq);
    virtual HRESULT __stdcall GetStatus(DWORD *status);
    virtual HRESULT __stdcall Initialize(void *ds, void *desc);
    virtual HRESULT __stdcall Lock(DWORD off, DWORD bytes, void **p1, DWORD *n1, void **p2, DWORD *n2, DWORD flags); // 0x2c
    virtual HRESULT __stdcall Play(DWORD r1, DWORD prio, DWORD flags);
    virtual HRESULT __stdcall SetCurrentPosition(DWORD pos);
    virtual HRESULT __stdcall SetFormat(const void *fmt);
    virtual HRESULT __stdcall SetVolume(long vol);
    virtual HRESULT __stdcall SetPan(long pan);
    virtual HRESULT __stdcall SetFrequency(DWORD freq);
    virtual HRESULT __stdcall Stop();
    virtual HRESULT __stdcall Unlock(void *p1, DWORD n1, void *p2, DWORD n2);
    virtual HRESULT __stdcall Restore();                                      // 0x50
};
struct DsBuffer {
    char pad[0x60];
    IDsBuf *m_buf;        // +0x60
    int restore(DWORD off, DWORD bytes, void **p1, DWORD *n1, void **p2, DWORD *n2);
};

// 85.0% (unchanged from g3). Both switches, the jump tables and the `return 0` block placed last match; the outer
// switch's left subtree (values <= 0x8878000a) is emitted as an inline `return 0x19` after `cmp eax, 0x8878000a; jg`,
// where the original jumps (`jle`) to the 0x19 block shared with the inner switch. Not moved by: case 0 inside the
// switch, goto labels for the 0x19 / 0 blocks, break vs return case bodies, grouped case labels, explicit defaults,
// a result variable, `if (hr == 0) goto ok;` (~130 variants).
// WIP: sound.dll 0x100101a0 ?restore@DsBuffer@m36a_101a0@@QAEHKKPAPAXPAK01@Z 85.0
int DsBuffer::restore(DWORD off, DWORD bytes, void **p1, DWORD *n1, void **p2, DWORD *n2)
{
    HRESULT hr = m_buf->Lock(off, bytes, p1, n1, p2, n2, 0);
    if (hr != 0) {
        switch (hr) {
        case 0x88780096:
            if (m_buf)
                m_buf->Restore();
            hr = m_buf->Lock(off, bytes, p1, n1, p2, n2, 0);
            if (hr != 0) {
                switch (hr) {
                case 0x88780096: return 0x1e;
                case 0x88780064: return 0x19;
                case 0x88780078: return 0x19;
                case 0x887800aa: return 0x19;
                case 0x80004001: return 0x19;
                case 0x80004005: return 0x19;
                case 0x8007000e: return 0x19;
                case 0x80070057: return 0x19;
                case 0x8878000a: return 0x19;
                }
            }
            return 0x19;
        case 0x88780064: return 0x19;
        case 0x88780078: return 0x19;
        case 0x887800aa: return 0x19;
        case 0x80004001: return 0x19;
        case 0x80004005: return 0x19;
        case 0x8007000e: return 0x19;
        case 0x80070057: return 0x19;
        case 0x8878000a: return 0x19;
        default:
            return 0x19;
        }
    }
    return 0;
}
}

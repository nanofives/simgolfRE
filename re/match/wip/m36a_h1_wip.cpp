// FLAGS sound.dll: /O2
// m36a group h1 near misses, NOT evidence (annotated `// WIP:`, never `// MATCH:`). Layout from group g3.
// To re-test, change the WIP line to MATCH in a scratch copy and run re/tools/match.py --no-evidence.

typedef unsigned long DWORD;
typedef long HRESULT;

struct IDsBuf {
    virtual HRESULT __stdcall QueryInterface(const void *iid, void **out);   // 0x00
    virtual DWORD __stdcall AddRef();                                         // 0x04
    virtual DWORD __stdcall Release();                                        // 0x08
    virtual HRESULT __stdcall GetCaps(void *caps);                            // 0x0c
    virtual HRESULT __stdcall GetCurrentPosition(DWORD *play, DWORD *write);  // 0x10
    virtual HRESULT __stdcall GetFormat(void *fmt, DWORD size, DWORD *got);   // 0x14
    virtual HRESULT __stdcall GetVolume(long *vol);                           // 0x18
    virtual HRESULT __stdcall GetPan(long *pan);                              // 0x1c
    virtual HRESULT __stdcall GetFrequency(DWORD *freq);                      // 0x20
    virtual HRESULT __stdcall GetStatus(DWORD *status);                       // 0x24
    virtual HRESULT __stdcall Initialize(void *ds, void *desc);               // 0x28
    virtual HRESULT __stdcall Lock(DWORD off, DWORD bytes, void **p1, DWORD *n1, void **p2, DWORD *n2, DWORD flags); // 0x2c
    virtual HRESULT __stdcall Play(DWORD r1, DWORD prio, DWORD flags);        // 0x30
    virtual HRESULT __stdcall SetCurrentPosition(DWORD pos);                  // 0x34
    virtual HRESULT __stdcall SetFormat(const void *fmt);                     // 0x38
    virtual HRESULT __stdcall SetVolume(long vol);                            // 0x3c
    virtual HRESULT __stdcall SetPan(long pan);                               // 0x40
    virtual HRESULT __stdcall SetFrequency(DWORD freq);                       // 0x44
    virtual HRESULT __stdcall Stop();                                         // 0x48
    virtual HRESULT __stdcall Unlock(void *p1, DWORD n1, void *p2, DWORD n2); // 0x4c
    virtual HRESULT __stdcall Restore();                                      // 0x50
};

extern "C" __declspec(dllimport) DWORD __stdcall timeGetTime();

namespace m36a_g3w {

struct WaveFmt {          // WAVEFORMATEX
    unsigned short wFormatTag;
    unsigned short nChannels;
    DWORD nSamplesPerSec;
    DWORD nAvgBytesPerSec;
    unsigned short nBlockAlign;
    unsigned short wBitsPerSample;
    unsigned short cbSize;
};

struct DsBuffer {
    int m_0;
    unsigned char m_4;
    char pad5[3];
    int m_8;
    int m_c, m_10, m_14, m_18, m_1c;
    int m_20;
    int m_24, m_28, m_2c, m_30;
    DWORD m_34, m_38, m_3c, m_40;
    int m_44;
    DWORD m_48, m_4c;
    DWORD m_50;
    DWORD m_54;
    DWORD m_58;
    void *m_5c;
    IDsBuf *m_buf;        // +0x60
    void *m_64, *m_68, *m_6c;
    DWORD m_desc[2];      // +0x70 DSBUFFERDESC dwSize, dwFlags
    DWORD m_bufferBytes;  // +0x78 dwBufferBytes
    char pad7c[0xa8 - 0x7c];
    WaveFmt m_fmt;        // +0xa8

    inline void restartPlaybackInl()
    {
        m_4c = 0;
        m_48 = 0;
        if (m_buf)
            m_buf->Stop();
        if (m_buf) {
            m_54 = timeGetTime();
            if (m_buf->Play(0, 0, 1) == 0)
                m_buf->GetFormat(&m_fmt, 0x12, 0);
        }
    }
    int getTimedPosition(DWORD *play, DWORD *write, DWORD ms);
    int getPlayPosition(DWORD *outAvail, DWORD *outCur);
    int restore(DWORD off, DWORD bytes, void **p1, DWORD *n1, void **p2, DWORD *n2);
};

// Remaining diff (94.3%; g3 had 92.2%): (1) the volatile hr store is emitted right after the call (original: after
// `cmp ebx, ecx`); (2) VC6 keeps the shared `avail = end - cur` block in the end < size arm and the first arm jumps
// forward to it (original: the block follows the first arm's cur > write block and the else arm jumps back to it).
// Fixed here: `write = 0` declared first (zero stores order) and m_24 = write before *outAvail.
// Tried without effect on (1)/(2): non-volatile hr through pointers / struct / array / inline ref (store removed),
// hr assigned from a temp after the loads, locals for play / m_10, condition orientations of both arms, a goto
// into the first arm's block, the avail computation as a common tail, an inline ringDist() helper, permuter (290).
// WIP: sound.dll 0x100106b0 ?getPlayPosition@DsBuffer@m36a_g3w@@QAEHPAK0@Z 94.3%
int DsBuffer::getPlayPosition(DWORD *outAvail, DWORD *outCur)
{
    int r = 0;
    DWORD write = 0, play = 0;
    volatile HRESULT hr;
    DWORD d, end, size, cur, avail;
    if (m_buf == 0)
        return 0x14;
    hr = m_buf->GetCurrentPosition(&play, &write);
    if (play < (DWORD)m_10)
        d = m_bufferBytes - m_10 + play;
    else
        d = play - m_10;
    m_18 = m_fmt.nBlockAlign ? d / m_fmt.nBlockAlign : 0;
    if (write == (DWORD)m_24)
        return 0x15;
    end = m_38 + write;
    size = m_bufferBytes;
    m_10 = play;
    if (end >= size) {
        end -= size;
        cur = m_30;
        if (cur <= write && end <= cur) {
            cur = m_40 + write;
            if (cur >= size)
                cur -= size;
            if (end < cur)
                avail = size - cur + end;
            else
                avail = end - cur;
            r = 0x2d;
        } else {
            if (end < cur)
                avail = size - cur + end;
            else
                avail = end - cur;
        }
    } else {
        cur = m_30;
        if (cur <= write || end <= cur) {
            cur = m_40 + write;
            if (cur >= size)
                cur -= size;
            if (end < cur)
                avail = size - cur + end;
            else
                avail = end - cur;
            r = 0x2d;
        } else {
            if (end < cur)
                avail = size - cur + end;
            else
                avail = end - cur;
        }
    }
    m_30 = end;
    m_24 = write;
    *outAvail = avail;
    *outCur = cur;
    if (cur < play)
        d = m_bufferBytes - play + cur;
    else
        d = cur - play;
    m_1c = m_fmt.nBlockAlign ? d / m_fmt.nBlockAlign : 0;
    return r;
}
} // namespace m36a_g3w

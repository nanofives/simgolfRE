// FLAGS sound.dll: /O2
// m36a (round 36) group h1: DsBuffer position functions (layout from group g3's near-miss file).

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

namespace m36a_h1d {

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

// Else arm: the first arm's value goes to its own local (`now`), so t keeps ecx; the new m_4c is a separate local
// (n, computed into eax as `ms + t`); m_50 is read into a local before the m_4c store, and that store goes through a
// pointer to the field: VC6 then keeps the q computation first and the store after the m_50 load, as the original.
// With `t = ms + t` and a member store (group g3's version) the sum is hoisted and computed in place (67.6%).
// MATCH: sound.dll 0x10010be0 ?getTimedPosition@DsBuffer@m36a_h1d@@QAEHPAK0K@Z
int DsBuffer::getTimedPosition(DWORD *play, DWORD *write, DWORD ms)
{
    int frames;
    DWORD t = m_4c;
    if (t == 0) {
        DWORD now = timeGetTime() - m_54;
        m_4c = now;
        m_48 = (now + m_50 * 2) * m_fmt.nSamplesPerSec / 1000;
        frames = m_fmt.nSamplesPerSec * ms / 1000;
    } else {
        DWORD q = m_48 * 1000 / 22050;
        DWORD n = ms + t;
        DWORD a = m_50;
        DWORD *p = &m_4c;
        *p = n;
        frames = (a * 10 - q + n) * m_fmt.nSamplesPerSec / 1000;
        if (frames < 0)
            frames = 0;
    }
    *play = m_fmt.nBlockAlign * frames;
    *write = m_fmt.nBlockAlign * m_48 & m_58;
    m_48 += frames;
    if (*play > m_bufferBytes) {
        restartPlaybackInl();
        *play = 0;
        return 0x27;
    }
    return 0;
}

} // namespace m36a_h1d

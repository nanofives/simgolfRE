// FLAGS sound.dll: /O2
// m36a (round 36) group g3: near misses, NOT evidence (annotated `// WIP:`, never `// MATCH:`).
// To re-test one, change its WIP line to MATCH in a scratch copy and run re/tools/match.py --no-evidence.

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

// WIP: sound.dll 0x10010be0 ?getTimedPosition@DsBuffer@m36a_g3w@@QAEHPAK0K@Z 67.6%
// Start, the restart tail (restartPlayback inlined) and the else arm's arithmetic match; the else arm keeps
// the new m_4c in ecx and builds the frames expression in edi (original: eax / ecx), and the two output
// stores use eax/ecx swapped. Tried: field vs local t, `m_4c += ms`, a fresh temp, operand order, permuter.
int DsBuffer::getTimedPosition(DWORD *play, DWORD *write, DWORD ms)
{
    int frames;
    DWORD t = m_4c;
    if (t == 0) {
        t = timeGetTime() - m_54;
        m_4c = t;
        m_48 = (t + m_50 * 2) * m_fmt.nSamplesPerSec / 1000;
        frames = m_fmt.nSamplesPerSec * ms / 1000;
    } else {
        DWORD q = m_48 * 1000 / 22050;
        t = ms + t;
        m_4c = t;
        frames = (m_50 * 10 - q + t) * m_fmt.nSamplesPerSec / 1000;
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

// WIP: sound.dll 0x100106b0 ?getPlayPosition@DsBuffer@m36a_g3w@@QAEHPAK0@Z 92.2%
// All arithmetic, the ebp zero/return register and the ternaries match. Two differences: the first local zero
// store is scheduled after `cmp eax, ebp` (original before), and VC6 keeps the shared `avail = end - cur`
// block after the not-wrapped test instead of after the wrapped/cur > write arm (32 if-orientation variants
// and an explicit goto label all compile to the same layout).
int DsBuffer::getPlayPosition(DWORD *outAvail, DWORD *outCur)
{
    int r = 0;
    DWORD play = 0, write = 0;
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
        if (cur <= write) {
            if (end <= cur) {
                cur = m_40 + write;
                if (cur >= size)
                    cur -= size;
                if (end < cur)
                    avail = size - cur + end;
                else
                    avail = end - cur;
                r = 0x2d;
            } else
                avail = end - cur;
        } else {
            if (end < cur)
                avail = size - cur + end;
            else
                avail = end - cur;
        }
    } else {
        cur = m_30;
        if (cur > write && end > cur)
            avail = end - cur;
        else {
            cur = m_40 + write;
            if (cur >= size)
                cur -= size;
            if (end < cur)
                avail = size - cur + end;
            else
                avail = end - cur;
            r = 0x2d;
        }
    }
    m_30 = end;
    *outAvail = avail;
    m_24 = write;
    *outCur = cur;
    if (cur < play)
        d = m_bufferBytes - play + cur;
    else
        d = cur - play;
    m_1c = m_fmt.nBlockAlign ? d / m_fmt.nBlockAlign : 0;
    return r;
}

// WIP: sound.dll 0x100101a0 ?restore@DsBuffer@m36a_g3w@@QAEHKKPAPAXPAK01@Z 85.0%
// Lock with one retry after Restore on DSERR_BUFFERLOST. Both switches match once the case set is
// mapDsError's (five hidden cases below DSERR_ALLOCATED returning the default 0x19, values not determined).
// Remaining: the original places `return 0` (first Lock succeeded) last, after the shared `return 0x19`;
// with the switch nested in `if (hr != 0)` VC6 puts the 0x19 block right after the first tree compare,
// with `if (hr == 0) return 0;` first it puts the return 0 block first (83.8%).
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

} // namespace m36a_g3w

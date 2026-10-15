// FLAGS sound.dll: /O2
// m36a (round 36) group g3: sound.dll DsBuffer / Wave_In_Device / node-list helpers matched by hand.

#include <string.h>
#include <math.h>
typedef unsigned long DWORD;
typedef long HRESULT;

// IDirectSoundBuffer-shaped COM interface (vtable slot offsets as the original calls them).
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

struct GUID_ { DWORD d1; unsigned short d2, d3; unsigned char d4[8]; };
extern const GUID_ IID_DsNotify;       // 0x1005d52c
extern const GUID_ IID_Ds3DListener;   // 0x1005d58c
extern const GUID_ IID_Ds3DBuffer;     // 0x1005d57c
extern "C" __declspec(dllimport) DWORD __stdcall timeGetTime();

namespace m36a_g3a {

struct WaveFmt {          // WAVEFORMATEX, 0x12 bytes
    unsigned short wFormatTag;
    unsigned short nChannels;
    DWORD nSamplesPerSec;
    DWORD nAvgBytesPerSec;
    unsigned short nBlockAlign;
    unsigned short wBitsPerSample;
    unsigned short cbSize;
};

struct DsSource {
    char pad[0x10];
    unsigned char flags;  // +0x10
    char pad11[0x188 - 0x11];
    DWORD masterVolume;   // +0x188
};

struct DsDesc {          // DSBUFFERDESC with guid3DAlgorithm, 0x24 bytes
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwBufferBytes;
    DWORD dwReserved;
    WaveFmt *lpwfxFormat;
    DWORD guid3DAlgorithm[4];
};

struct DsBuffer {
    int m_0;              // +0x00 flags
    unsigned char m_4;    // +0x04
    char pad5[3];
    int m_8;
    int m_c, m_10, m_14, m_18, m_1c;
    int m_20;
    int m_24, m_28, m_2c, m_30;
    DWORD m_34, m_38, m_3c, m_40;
    int m_44;
    DWORD m_48, m_4c;
    DWORD m_50;
    DWORD m_54;           // +0x54 timeGetTime at play
    DWORD m_58;
    DsSource *m_5c;       // +0x5c
    IDsBuf *m_buf;        // +0x60
    IDsBuf *m_64;         // +0x64
    IDsBuf *m_68;         // +0x68
    void *m_6c;           // +0x6c
    DsDesc m_desc;        // +0x70 DSBUFFERDESC (0x24 bytes)
    int m_94[5];          // +0x94

    WaveFmt m_fmt;        // +0xa8
    char padbc[0xc4 - 0xbc];  // sizeof(WaveFmt) is 0x14 with alignment
    long m_volume;        // +0xc4

    int getPosition(DWORD a, DWORD b, DWORD c);
    int getTimedPosition(DWORD a, DWORD b, DWORD c);
    int getPlayPosition(DWORD a, DWORD b);
    int queryNotify();
    int query3D();
    int play(DWORD flags);
    void restartPlayback();
    int setPosition();
    int setRegionMs(DWORD ms, DWORD n);
    bool hasAuxInterfaces();
    int release();
    void destroy();
    int releaseIfOwned();
    int lockEntire();
    void setVolume(DWORD vol);
    int lockCaptureRegion(DWORD n, DWORD *out);
    void setFrequency(DWORD f);
    int stop();
    void setVolumeRaw(long v);
    int unlock(void *p1, DWORD n1, void *p2, DWORD n2);
    int isBusy();
};

// MATCH: sound.dll 0x1000fec0 ?hasAuxInterfaces@DsBuffer@m36a_g3a@@QAE_NXZ
bool DsBuffer::hasAuxInterfaces()
{
    if (m_64 == 0 && m_68 == 0)
        return false;
    return true;
}

// MATCH: sound.dll 0x1000f160 ?setFrequency@DsBuffer@m36a_g3a@@QAEXK@Z
void DsBuffer::setFrequency(DWORD f)
{
    if (m_buf)
        m_buf->SetFrequency(f);
}

// MATCH: sound.dll 0x1000f760 ?stop@DsBuffer@m36a_g3a@@QAEHXZ
int DsBuffer::stop()
{
    if (m_buf == 0)
        return 2;
    m_buf->Stop();
    return 0;
}

// MATCH: sound.dll 0x1000f190 ?setVolumeRaw@DsBuffer@m36a_g3a@@QAEXJ@Z
void DsBuffer::setVolumeRaw(long v)
{
    m_volume = v;
    if (m_buf)
        m_buf->SetVolume(v);
}

// MATCH: sound.dll 0x10010370 ?unlock@DsBuffer@m36a_g3a@@QAEHPAXK0K@Z
// The original stores Unlock's result into a stack slot nothing reads; a volatile local reproduces it
// (a plain local, a reassigned parameter or an empty `if (hr)` all drop the store).
int DsBuffer::unlock(void *p1, DWORD n1, void *p2, DWORD n2)
{
    volatile HRESULT hr;
    if (m_buf)
        hr = m_buf->Unlock(p1, n1, p2, n2);
    return 0;
}

// MATCH: sound.dll 0x10010e30 ?isBusy@DsBuffer@m36a_g3a@@QAEHXZ
int DsBuffer::isBusy()
{
    DWORD status;
    if (m_buf == 0)
        return 0;
    m_buf->GetStatus(&status);
    if (m_44)
        return 1;
    return status & 1;
}

// MATCH: sound.dll 0x10010660 ?getPosition@DsBuffer@m36a_g3a@@QAEHKKK@Z
int DsBuffer::getPosition(DWORD a, DWORD b, DWORD c)
{
    if (m_5c) {
        if (m_5c->flags & 0x20)
            return getTimedPosition(a, b, c);
        return getPlayPosition(a, b);
    }
    return 1;
}

// MATCH: sound.dll 0x1000ff60 ?queryNotify@DsBuffer@m36a_g3a@@QAEHXZ
int DsBuffer::queryNotify()
{
    HRESULT hr;
    if (m_68)
        hr = m_68->QueryInterface(&IID_DsNotify, &m_6c);
    else if (m_64)
        hr = m_64->QueryInterface(&IID_DsNotify, &m_6c);
    else
        hr = 0;   // `else return 0;` places the failure block first (82.8%)
    if (hr >= 0)
        return 0;
    m_6c = 0;
    return 0xb;
}

// MATCH: sound.dll 0x1000fef0 ?query3D@DsBuffer@m36a_g3a@@QAEHXZ
int DsBuffer::query3D()
{
    HRESULT hr;
    if (m_buf == 0)
        return 0x14;
    if (m_0 & 2)
        hr = m_buf->QueryInterface(&IID_Ds3DListener, (void **)&m_68);
    else
        hr = m_buf->QueryInterface(&IID_Ds3DBuffer, (void **)&m_64);
    if (hr >= 0)
        return 0;
    m_68 = 0;
    m_64 = 0;
    return 0xb;
}

// MATCH: sound.dll 0x10010db0 ?play@DsBuffer@m36a_g3a@@QAEHK@Z
int DsBuffer::play(DWORD flags)
{
    if (m_buf == 0)
        return 0x14;
    m_54 = timeGetTime();
    if (m_buf->Play(0, 0, flags))
        return 0x1c;
    m_buf->GetFormat(&m_fmt, 0x12, 0);
    return 0;
}

// MATCH: sound.dll 0x1000f020 ?restartPlayback@DsBuffer@m36a_g3a@@QAEXXZ
void DsBuffer::restartPlayback()
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

// MATCH: sound.dll 0x1000f790 ?setPosition@DsBuffer@m36a_g3a@@QAEHXZ
int DsBuffer::setPosition()
{
    DWORD play, write;
    if (m_buf) {
        m_buf->SetCurrentPosition(0);
        m_buf->GetCurrentPosition(&play, &write);
    }
    m_4 |= 1;
    m_24 = 0;
    m_28 = 0;
    m_2c = 0;
    m_30 = 0;
    m_c = 0;
    m_10 = 0;
    m_14 = 0;
    m_18 = 0;
    m_1c = 0;
    m_48 = 0;
    m_4c = 0;
    return 0;
}

// MATCH: sound.dll 0x10010eb0 ?setRegionMs@DsBuffer@m36a_g3a@@QAEHKK@Z
int DsBuffer::setRegionMs(DWORD ms, DWORD n)
{
    DWORD frames = m_fmt.nSamplesPerSec * ms / 1000;
    DWORD blk = m_fmt.nBlockAlign * frames;
    DWORD tot = blk * n;
    if (tot > m_desc.dwBufferBytes)
        return 10;
    m_38 = tot;
    m_40 = blk;
    m_34 = frames;   // m_3c / m_34 store order not determined here (both compile identically);
    m_3c = (tot - blk) >> 1;   // the inlined copy in initDesc needs m_34 first
    return 0;
}

// MATCH: sound.dll 0x1000f8f0 ?release@DsBuffer@m36a_g3a@@QAEHXZ
int DsBuffer::release()
{
    if (m_buf) {
        m_buf->Stop();
        if ((m_64 || m_68) && m_64) {
            if (m_6c) {
                ((IDsBuf *)m_6c)->Release();
                m_6c = 0;
            }
            m_64->Release();
            m_64 = 0;
        }
        m_buf->Release();
        memset(m_94, 0, sizeof(m_94));
        m_8 = 0;
        m_c = 0;
        m_buf = 0;
        m_4c = 0;
        m_48 = 0;
    }
    return 0;
}

// MATCH: sound.dll 0x1000ef60 ?destroy@DsBuffer@m36a_g3a@@QAEXXZ
void DsBuffer::destroy()
{
    if (m_buf && m_5c) {
        m_buf->Stop();
        if ((m_64 || m_68) && m_64) {
            if (m_6c) {
                ((IDsBuf *)m_6c)->Release();
                m_6c = 0;
            }
            m_64->Release();
            m_64 = 0;
        }
        m_buf->Release();
        memset(m_94, 0, sizeof(m_94));
        memset(m_94, 0, sizeof(m_94));
        m_8 = 0;
        m_c = 0;
        m_buf = 0;
        m_4c = 0;
        m_48 = 0;
    }
}

// MATCH: sound.dll 0x10010060 ?releaseIfOwned@DsBuffer@m36a_g3a@@QAEHXZ
int DsBuffer::releaseIfOwned()
{
    if (m_5c == 0)
        return 1;
    if (m_buf) {
        m_buf->Stop();
        if ((m_64 || m_68) && m_64) {
            if (m_6c) {
                ((IDsBuf *)m_6c)->Release();
                m_6c = 0;
            }
            m_64->Release();
            m_64 = 0;
        }
        m_buf->Release();
        memset(m_94, 0, sizeof(m_94));
        memset(m_94, 0, sizeof(m_94));
        m_8 = 0;
        m_c = 0;
        m_buf = 0;
        m_4c = 0;
        m_48 = 0;
    }
    return 0;
}

// MATCH: sound.dll 0x10010860 ?lockCaptureRegion@DsBuffer@m36a_g3a@@QAEHKPAK@Z
// The result test is a switch: a hidden positive case returning the default value (removed by VC6, value not
// determined; tested 0x0878000a DS_NO_VIRTUALIZATION) makes VC6 keep 0 in edi across the call (`cmp eax, edi`).
// `if (hr) return 6;`, `case 1:` (VC6 then emits `sub eax, edi; dec eax`) and negative hidden cases do not.
int DsBuffer::lockCaptureRegion(DWORD n, DWORD *out)
{
    DWORD write = 0, play = 0;
    DWORD pos, end;
    switch (m_buf->GetCurrentPosition(&play, &write)) {
    case 0:
        break;
    case 0x0878000a:
        return 6;
    default:
        return 6;
    }
    if (m_4 & 1) {
        m_4 &= ~1;
        pos = m_40 + write;
        m_28 = write;
        m_24 = write;
        if (pos >= m_desc.dwBufferBytes)
            pos -= m_desc.dwBufferBytes;
    } else {
        pos = m_30;
    }
    end = n + pos;
    if (end >= m_desc.dwBufferBytes)
        end -= m_desc.dwBufferBytes;
    m_24 = write;
    m_30 = end;
    *out = pos;
    return 0;
}

// MATCH: sound.dll 0x1000f810 ?lockEntire@DsBuffer@m36a_g3a@@QAEHXZ
int DsBuffer::lockEntire()
{
    void *p1;
    DWORD n1;
    void *p2;
    DWORD n2;
    if (m_buf) {
        if (m_buf->Lock(0, m_desc.dwBufferBytes, &p1, &n1, &p2, &n2, 2) == 0) {
            memset(p1, 0, n1);
            if (n2)
                memset(p2, 0, n2);
            m_buf->Unlock(p1, n1, p2, n2);
            return 0;
        }
        return 0x1a;
    }
    return 0x24;
}

// MATCH: sound.dll 0x1000f1c0 ?setVolume@DsBuffer@m36a_g3a@@QAEXK@Z
void DsBuffer::setVolume(DWORD vol)
{
    DWORD master, lin;
    long db;
    if (vol > 127)
        vol = 127;
    if (m_5c)
        master = m_5c->masterVolume;
    else
        master = 127;
    // Separate statements are needed: in one expression VC6 folds 1/127 * 127 and 10 * 100 away.
    // `x / 127.0` compiles to the same fmul by 1/127, so the spelling of the constant is not determined.
    double a = master * (1.0 / 127.0);
    double b = vol * (1.0 / 127.0);
    double g = a * b;
    lin = (DWORD)(g * 127.0);
    if (lin) {
        double x = log(lin * (1.0 / 127.0)) * 10.0;
        db = (long)(x * 100.0);
    } else
        db = -10000;
    m_volume = db;
    if (m_buf)
        m_buf->SetVolume(db);
}

} // namespace m36a_g3a

namespace m36a_g3d {
using m36a_g3a::DsDesc;
using m36a_g3a::WaveFmt;

struct DsBuffer {
    unsigned int m_bit0 : 1;   // +0x00 flag bits
    unsigned int m_bit1 : 1;
    unsigned int m_bits : 30;
    char pad4[0x60 - 4];
    void *m_buf;               // +0x60
    char pad64[0x70 - 0x64];
    DsDesc m_desc;             // +0x70
    char pad94[0xa8 - 0x94];
    WaveFmt m_fmt;             // +0xa8
    int configureDesc(int mode);
};

// MATCH: sound.dll 0x10010f30 ?configureDesc@DsBuffer@m36a_g3d@@QAEHH@Z
int DsBuffer::configureDesc(int mode)
{
    if (m_buf)
        return 6;
    // The bitfield store sits after the switch (VC6 duplicates it into both arms without folding the case
    // value); the order of the two cases in the source is not determined (both compile identically).
    switch (mode) {
    case 0:
        m_desc.lpwfxFormat = &m_fmt;
        m_desc.dwSize = 0x24;
        m_desc.dwFlags = 0xe8;
        m_desc.dwBufferBytes = 0x10000;
        m_desc.dwReserved = 0;
        break;
    case 1:
        memset(&m_desc, 0, sizeof(m_desc));
        m_desc.dwSize = 0x24;
        m_desc.dwFlags = 1;
        break;
    default:
        return 10;
    }
    m_bit1 = mode;
    return 0;
}

} // namespace m36a_g3d

namespace m36a_g3g {

#pragma pack(push, 1)
struct WaveFmtP {         // WAVEFORMATEX as mmsystem.h packs it (0x12 bytes)
    unsigned short wFormatTag;
    unsigned short nChannels;
    DWORD nSamplesPerSec;
    DWORD nAvgBytesPerSec;
    unsigned short nBlockAlign;
    unsigned short wBitsPerSample;
    unsigned short cbSize;
};
#pragma pack(pop)

inline int mapDsErrorInl(HRESULT hr)
{
    switch (hr) {
    case 0x80004001: return 0xb;
    case 0x8007000e: return 0x11;
    case 0x80070057: return 0xa;
    case 0x8878000a: return 0xc;
    case 0x88780078: return 1;
    case 0x88780064: return 0x12;
    case 0x887800aa: return 0x13;
    case 0x88780096: return 0x1e;
    case 0x80004005: return 0x2f;
    default: return 0x2f;
    }
}

struct DsBuffer {
    unsigned int m_bit0 : 1;   // +0x00 flag bits
    unsigned int m_bit1 : 1;
    unsigned int m_bits : 30;
    char pad4[0x60 - 4];
    IDsBuf *m_buf;             // +0x60
    char pad64[0xa8 - 0x64];
    WaveFmtP m_fmt;            // +0xa8
    int setPrimaryFormat(const WaveFmtP *fmt);
};

// MATCH: sound.dll 0x1000f990 ?setPrimaryFormat@DsBuffer@m36a_g3g@@QAEHPBUWaveFmtP@2@@Z
int DsBuffer::setPrimaryFormat(const WaveFmtP *fmt)
{
    if (m_buf && m_bit1 != 1)
        return 6;
    if (m_buf == 0 && m_bit1 == 1)
        return 6;
    if (m_buf && m_bit1 == 1) {
        HRESULT hr = m_buf->SetFormat(fmt);
        if (hr)
            return mapDsErrorInl(hr);
        m_fmt = *fmt;
        return 0;
    }
    m_fmt = *fmt;
    return 0;
}

} // namespace m36a_g3g

namespace m36a_g3h {
using m36a_g3g::WaveFmtP;

struct IDSound {          // IDirectSound-shaped
    virtual HRESULT __stdcall QueryInterface(const void *iid, void **out);
    virtual DWORD __stdcall AddRef();
    virtual DWORD __stdcall Release();
    virtual HRESULT __stdcall CreateSoundBuffer(void *desc, IDsBuf **out, void *outer);   // 0x0c
};

struct DsDescP {
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwBufferBytes;
    DWORD dwReserved;
    WaveFmtP *lpwfxFormat;
    DWORD guid3DAlgorithm[4];
};

struct DsCaps {           // DSBCAPS, 0x14 bytes
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwBufferBytes;
    DWORD dwUnlockTransferRate;
    DWORD dwPlayCpuOverhead;
};

struct DsBuffer {
    unsigned int m_bit0 : 1;   // +0x00 flag bits
    unsigned int m_bit1 : 1;
    unsigned int m_bits : 30;
    char pad4[0x50 - 4];
    void *m_50;                // +0x50
    int m_54;
    DWORD m_58;                // +0x58
    int m_5c;
    IDsBuf *m_buf;             // +0x60
    IDsBuf *m_64;
    IDsBuf *m_68;
    void *m_6c;
    DsDescP m_desc;            // +0x70
    DsCaps m_caps;             // +0x94
    WaveFmtP m_fmt;            // +0xa8

    inline int query3D()
    {
        HRESULT hr;
        if (m_buf == 0)
            return 0x14;
        if (m_bit1)
            hr = m_buf->QueryInterface(&IID_Ds3DListener, (void **)&m_68);
        else
            hr = m_buf->QueryInterface(&IID_Ds3DBuffer, (void **)&m_64);
        if (hr >= 0)
            return 0;
        m_68 = 0;
        m_64 = 0;
        return 0xb;
    }
    inline int queryNotify()
    {
        HRESULT hr;
        if (m_68)
            hr = m_68->QueryInterface(&IID_DsNotify, &m_6c);
        else if (m_64)
            hr = m_64->QueryInterface(&IID_DsNotify, &m_6c);
        else
            hr = 0;
        if (hr >= 0)
            return 0;
        m_6c = 0;
        return 0xb;
    }
    int create(IDSound *ds, void *src);
};

// MATCH: sound.dll 0x1000fbb0 ?create@DsBuffer@m36a_g3h@@QAEHPAUIDSound@2@PAX@Z
int DsBuffer::create(IDSound *ds, void *src)
{
    if (src == 0)
        return 10;
    if (m_bit1 == 1)
        m_desc.lpwfxFormat = 0;
    else if (m_desc.lpwfxFormat == 0)
        m_desc.lpwfxFormat = &m_fmt;
    else
        m_fmt = *m_desc.lpwfxFormat;
    if (m_buf)
        return 0xc;
    HRESULT hr = ds->CreateSoundBuffer(&m_desc, &m_buf, 0);
    if (hr)
        return m36a_g3g::mapDsErrorInl(hr);
    m_50 = src;
    m_caps.dwSize = 0x14;
    if (m_desc.dwBufferBytes)
        m_58 = ~m_desc.dwBufferBytes % m_desc.dwBufferBytes;
    volatile HRESULT hr2 = m_buf->GetCaps(&m_caps);
    if (m_bit1 == 1) {
        m_buf->SetFormat(&m_fmt);
        m_buf->Play(0, 0, 1);
    }
    if (m_desc.dwFlags & 0x10) {
        query3D();
        queryNotify();
    }
    m_buf->GetFormat(&m_fmt, 0x12, 0);
    return 0;
}

} // namespace m36a_g3h

namespace m36a_g3e {
using m36a_g3a::DsDesc;
using m36a_g3a::WaveFmt;

struct DsBuffer {
    unsigned int m_bit0 : 1;   // +0x00 flag bits
    unsigned int m_bit1 : 1;
    unsigned int m_bits : 30;
    int m_4, m_8;
    int m_c, m_10, m_14, m_18, m_1c;
    int m_20;
    int m_24, m_28, m_2c, m_30;
    DWORD m_34, m_38, m_3c, m_40;
    int m_44;
    int m_48, m_4c, m_50, m_54, m_58;
    void *m_5c, *m_60, *m_64, *m_68, *m_6c;
    DsDesc m_desc;             // +0x70
    int m_94[5];               // +0x94
    WaveFmt m_fmt;             // +0xa8
    int m_bc, m_c0;
    long m_volume;             // +0xc4

    inline int setRegionMs(DWORD ms, DWORD n)
    {
        DWORD frames = m_fmt.nSamplesPerSec * ms / 1000;
        DWORD blk = m_fmt.nBlockAlign * frames;
        DWORD tot = blk * n;
        if (tot > m_desc.dwBufferBytes)
            return 10;
        m_38 = tot;
        m_40 = blk;
        m_34 = frames;
        m_3c = (tot - blk) >> 1;
        return 0;
    }
    int initDesc();
};

// MATCH: sound.dll 0x1000f590 ?initDesc@DsBuffer@m36a_g3e@@QAEHXZ
int DsBuffer::initDesc()
{
    m_bit1 = 0;
    memset(&m_desc, 0, sizeof(m_desc));
    m_desc.dwSize = 0x24;
    m_desc.dwFlags = 0xe8;
    m_desc.dwBufferBytes = 0x10000;
    m_desc.dwReserved = 0;
    m_desc.lpwfxFormat = &m_fmt;
    memset(&m_fmt, 0, 0x12);
    m_fmt.wFormatTag = 1;
    m_fmt.nChannels = 2;
    m_fmt.nSamplesPerSec = 22050;
    m_fmt.nAvgBytesPerSec = 88200;
    m_fmt.nBlockAlign = 4;
    m_fmt.wBitsPerSample = 16;
    m_fmt.cbSize = 0;
    m_40 = 0;
    m_14 = 0;
    m_18 = 0;
    m_1c = 0;
    m_c = 0;
    m_44 = 0;
    memset(m_94, 0, sizeof(m_94));
    m_10 = 0;
    m_24 = 0;
    m_2c = 0;
    m_30 = 0;
    m_60 = 0;
    m_34 = 0x295;
    setRegionMs(30, 18);
    m_5c = 0;
    m_c0 = 0;
    m_bc = 0;
    m_64 = 0;
    m_68 = 0;
    m_6c = 0;
    m_20 = 0;
    m_volume = 0;
    m_3c = 0;
    return 0;
}

} // namespace m36a_g3e

// MATCH: sound.dll 0x1000f480 ?mapDsError@@YAHJ@Z
int mapDsError(HRESULT hr)
{
    switch (hr) {
    case 0x80004001: return 0xb;    // E_NOTIMPL
    case 0x8007000e: return 0x11;   // E_OUTOFMEMORY
    case 0x80070057: return 0xa;    // E_INVALIDARG
    case 0x8878000a: return 0xc;    // DSERR_ALLOCATED
    case 0x88780078: return 1;      // DSERR_NODRIVER
    case 0x88780064: return 0x12;   // DSERR_BADFORMAT
    case 0x887800aa: return 0x13;   // DSERR_UNINITIALIZED
    case 0x88780096: return 0x1e;   // DSERR_BUFFERLOST
    // A ninth case returning the default value is needed: without it VC6 splits the tree at 0x88780064 and builds
    // no table. The case leaves no trace in the output, so its value is not determined: any value below
    // 0x8007000e works (tested 0x80004005 E_FAIL / DSERR_GENERIC, 0x80004002, 0x80040110, 0x80070005).
    case 0x80004005: return 0x2f;   // DSERR_GENERIC (value not determined)
    default: return 0x2f;
    }
}

namespace m36a_g3c {

struct Node {
    Node *prev;
    Node *next;
    void *data;
};

struct NodeListC {
    Node *head;
    Node *tail;
    int unk8;
    int count;
    inline void *pop()
    {
        Node *n = head;
        if (n == 0)
            return 0;
        Node *nx = n->next;
        if (nx) {
            nx->prev = 0;
            head = nx;
        } else {
            tail = 0;
            head = 0;
        }
        void *d = n->data;
        delete n;
        count--;
        return d;
    }
    void clear();
};

// MATCH: sound.dll 0x1000e5e0 ?clear@NodeListC@m36a_g3c@@QAEXXZ
void NodeListC::clear()
{
    void *p = pop();
    while (p != 0)
        p = pop();
}

struct NodeListB {
    Node *head;
    Node *tail;
    Node *cur;
    int count;
    void *findAndUnlink(void *d);
};

// MATCH: sound.dll 0x1000e700 ?findAndUnlink@NodeListB@m36a_g3c@@QAEPAXPAX@Z
void *NodeListB::findAndUnlink(void *d)
{
    // The original re-tests the found node for NULL (an inlined find + unlink); the goto form reproduces the
    // separate loop-exit `return 0` block and the redundant test.
    Node *n = head;
    while (n) {
        if (n->data == d)
            goto found;
        n = n->next;
    }
    return 0;
found:
    if (n == 0)
        return 0;
    if (n->prev)
        n->prev->next = n->next;
    else
        head = n->next;
    Node *nx = n->next;
    if (nx) {
        nx->prev = n->prev;
        cur = nx;
    } else {
        tail = n->prev;
        cur = 0;
    }
    void *r = n->data;
    delete n;
    count--;
    return r;
}

} // namespace m36a_g3c

struct WaveInDevice {
    void reset();
    int close();
    int open();
    int prepareHeaders();
    int addBuffer();
    int start();
};
extern WaveInDevice *g_waveInDevice;

struct Wave_In_Device {
    static int stop();
    static int start();
};

// MATCH: sound.dll 0x1000ea30 ?start@Wave_In_Device@@SAHXZ
int Wave_In_Device::start()
{
    int r;
    if (g_waveInDevice == 0)
        return 3;
    r = g_waveInDevice->open();
    if (r)
        return r;
    r = g_waveInDevice->prepareHeaders();
    if (r)
        return r;
    r = g_waveInDevice->addBuffer();
    if (r)
        return r;
    return g_waveInDevice->start();
}

// MATCH: sound.dll 0x1000eaa0 ?stop@Wave_In_Device@@SAHXZ
int Wave_In_Device::stop()
{
    if (g_waveInDevice == 0)
        return 3;
    g_waveInDevice->reset();
    return g_waveInDevice->close();
}

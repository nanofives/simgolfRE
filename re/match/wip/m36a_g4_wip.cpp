// FLAGS sound.dll: /O2
// m36a group g4 near misses (not evidence: annotated `// WIP:`, never `// MATCH:`). Declarations come from the
// matched file; every function below is declared there.
#include "../m36a_g4.cpp"

// Only miss: `mov ecx, esi` (this for releaseIfOwned) is scheduled before the store b->prev = 0 in ours and between
// the stores b->prev = 0 / b->next = 0 in the original. Tried: member / free inline unlink, store order, chained
// assignment, struct global for the list, pop helper, temporaries, /G4 /G5 /G6 /Ox.
// WIP: sound.dll 0x10011970 ?releaseAll@DirectSoundDevice@@QAEXXZ 98.5%
void DirectSoundDevice::releaseAll()
{
    DsBuffer *b;
    while ((b = g_dsBufferList) != 0) {
        DsBuffer *p = b->prev;
        DsBuffer *n = b->next;
        if (p)
            p->next = n;
        else
            g_dsBufferList = n;
        if (n)
            n->prev = p;
        else
            g_dsBufferListTail = p;
        b->prev = 0;
        b->next = 0;
        b->releaseIfOwned();
        delete b;
    }
    if (flags & 2)
        primary.releaseIfOwned();
    if (aux) {
        aux->releaseIfOwned();
        delete aux;
        aux = 0;
    }
    if (ds) {
        ds->Release();
        ds = 0;
    }
    devCursor = 0;
    releaseCount++;
}

// Same miss as releaseAll.
// WIP: sound.dll 0x100133d0 ?releaseAllBuffers@DirectSoundDevice@@QAEHXZ 97.5%
int DirectSoundDevice::releaseAllBuffers()
{
    if (!(flags & 2))
        return 3;
    DsBuffer *b;
    while ((b = g_dsBufferList) != 0) {
        DsBuffer *p = b->prev;
        DsBuffer *n = b->next;
        if (p)
            p->next = n;
        else
            g_dsBufferList = n;
        if (n)
            n->prev = p;
        else
            g_dsBufferListTail = p;
        b->prev = 0;
        b->next = 0;
        b->releaseIfOwned();
        delete b;
    }
    return 0;
}

// Only miss: the store primary.desc.dwFlags = createFlags (mov [esi+0xe4], ebp) comes first in its block in the
// original and right before the call in ours (ours schedules it after the argument loads). Through a DsBuffer *
// variable the store moves first but is emitted as [edi+0x74].
// WIP: sound.dll 0x10011c60 ?create@DirectSoundDevice@@QAEHPAXK@Z 99.6%
int DirectSoundDevice::create(void *hwnd, unsigned long level)
{
    if (hwnd == 0)
        return 10;
    if (ds != 0)
        return 0xc;
    if (!(flags & 1))
        return 3;
    if (level == 0)
        level = 2;
    if (devCursor == 0)
        return 0x13;
    HRES hr = CoInitialize(0);
    if (hr != 0 && hr != 1) {
        ds = 0;
        return 0x20;
    }
    releaseCount--;
    hr = CoCreateInstance(&CLSID_DirectSound, 0, 1, &IID_IDirectSound, (void **)&ds);
    if (hr == 0 && ds != 0) {
        hr = ds->Initialize(devCursor->guid);
        if (hr != 0) {
            mapDsError(hr);
            releaseAll();
            return 3;
        }
    }
    hr = ds->SetCooperativeLevel(hwnd, level);
    if (hr != 0) {
        mapDsError(hr);
        releaseAll();
        return 3;
    }
    g_dsCoopWindow = hwnd;
    caps[0] = sizeof(caps);
    hr = ds->GetCaps(caps);
    if (hr != 0)
        mapDsError(hr);
    if (!(caps[1] & 0x20)) {
        unsigned long v = caps[9];
        bufferCount = 0;
        m14c = v;
        m154 = v;
        m150 = v;
        m15c = 0;
        if (caps[13] < 2 && (flags & 8))
            flags &= 0xe7;
    } else {
        flags &= 0xe7;
    }
    WaveFmt mono;
    WaveFmt stereo;
    int createFlags = 1;
    stereo.tag = 1;
    stereo.channels = 2;
    stereo.rate = 44100;
    stereo.avgBytes = 176400;
    stereo.blockAlign = 4;
    stereo.bits = 16;
    stereo.cbSize = 0;
    mono.tag = 1;
    mono.channels = 1;
    mono.rate = 22050;
    mono.avgBytes = 44100;
    mono.blockAlign = 2;
    mono.bits = 16;
    mono.cbSize = 0;
    if (flags & 8) {
        primary.setPrimaryFormat(&mono);
        createFlags = 0x11;
    } else {
        primary.setPrimaryFormat(&stereo);
    }
    int r = primary.configureDesc(1);
    if (r) {
        releaseAll();
        return r;
    }
    primary.desc.dwFlags = createFlags;
    r = primary.create(ds, enumArg);
    if (r) {
        releaseAll();
        return r;
    }
    primary.owner = this;
    flags |= 2;
    WaveSrc *p = (WaveSrc *)list178.pop();
    while (p) {
        if (m14c)
            p->parseRiffWave(p->riff);
        p = (WaveSrc *)list178.pop();
    }
    coopLevel = level;
    return 0;
}

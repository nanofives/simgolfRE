// FLAGS sound.dll: /O2
// m36a group h1: DirectSoundDevice::create, with the declarations of the matched group g4 file.
#include "m36a_g4.cpp"

// MATCH: sound.dll 0x10011c60 ?create@DirectSoundDevice@@QAEHPAXK@Z
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
    // Through a pointer to the field the dwFlags store is emitted first in its block, as in the original; a plain
    // member store goes after the argument loads (99.6%). `DsBuffer *pb = &primary; pb->desc.dwFlags = createFlags;`
    // compiles identically: the source spelling is not determined there.
    unsigned long *pf = &primary.desc.dwFlags;
    *pf = createFlags;
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

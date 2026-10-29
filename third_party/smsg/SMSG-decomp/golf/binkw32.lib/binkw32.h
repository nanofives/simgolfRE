#ifndef BINKW32_H
#define BINKW32_H

#ifdef __cplusplus
extern "C" {
#endif

struct BINKRECT {
    int Left;
    int Top;
    int Width;
    int Height;
};

struct BINK {
    unsigned long Width;
    unsigned long Height;
    unsigned long Frames;
    unsigned long FrameNum;
    unsigned long LastFrameNum;
    unsigned long FrameRate;
    unsigned long FrameRateDiv;
    unsigned long ReadError;
    unsigned long OpenFlags;
    unsigned long BinkType;
    unsigned long Size;
    unsigned long FrameSize;
    unsigned long SndSize;
    struct BINKRECT FrameRects[8];
};

struct BINKBUFFER {
    unsigned long Width;
    unsigned long Height;
    unsigned long WindowWidth;
    unsigned long WindowHeight;
    unsigned long SurfaceType;
    void *Buffer;
    int BufferPitch;
};

#define BINKIOSIZE        0x01000000L
#define BINKBUFFERAUTO    0

typedef int(__stdcall *BINKSNDSYSOPEN)(unsigned long param);

__declspec(dllimport) struct BINK *__stdcall BinkOpen(const char *name, unsigned long flags);
__declspec(dllimport) void __stdcall BinkClose(struct BINK *bnk);
__declspec(dllimport) int __stdcall BinkDoFrame(struct BINK *bink);
__declspec(dllimport) void __stdcall BinkNextFrame(struct BINK *bink);
__declspec(dllimport) int __stdcall BinkWait(struct BINK *bnk);
__declspec(dllimport) int __stdcall BinkGetRects(struct BINK *bink, unsigned flags);
__declspec(dllimport) int __stdcall BinkCopyToBuffer(struct BINK *bink, void *dest, int destpitch, unsigned destheight,
    unsigned destx, unsigned desty, unsigned flags);
__declspec(dllimport) void __stdcall BinkSetIOSize(unsigned long iosize);
__declspec(dllimport) int __stdcall BinkOpenDirectSound(unsigned long param);
__declspec(dllimport) int __stdcall BinkSetSoundSystem(BINKSNDSYSOPEN open, unsigned long param);

__declspec(dllimport) struct BINKBUFFER *__stdcall BinkBufferOpen(void *dest, unsigned long w, unsigned long h,
    unsigned long flags);
__declspec(dllimport) void __stdcall BinkBufferClose(struct BINKBUFFER *buf);
__declspec(dllimport) int __stdcall BinkBufferLock(struct BINKBUFFER *buf);
__declspec(dllimport) int __stdcall BinkBufferUnlock(struct BINKBUFFER *buf);
__declspec(dllimport) int __stdcall BinkBufferBlit(struct BINKBUFFER *buf, struct BINKRECT *rects, unsigned numrects);
__declspec(dllimport) int __stdcall BinkBufferSetOffset(struct BINKBUFFER *buf, int destx, int desty);

#ifdef __cplusplus
}
#endif

#endif

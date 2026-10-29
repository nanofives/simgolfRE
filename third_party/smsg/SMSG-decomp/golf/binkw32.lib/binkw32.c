#define BINKAPI __declspec(dllexport)

BINKAPI void *__stdcall BinkOpen(const char *name, unsigned long flags)
{
    return 0;
}
BINKAPI void __stdcall BinkClose(void *bink)
{
}
BINKAPI int __stdcall BinkDoFrame(void *bink)
{
    return 0;
}
BINKAPI void __stdcall BinkNextFrame(void *bink)
{
}
BINKAPI int __stdcall BinkWait(void *bink)
{
    return 0;
}
BINKAPI int __stdcall BinkGetRects(void *bink, unsigned flags)
{
    return 0;
}
BINKAPI int __stdcall BinkCopyToBuffer(void *bink, void *dest, int destpitch, unsigned destheight, unsigned destx,
    unsigned desty, unsigned flags)
{
    return 0;
}
BINKAPI void __stdcall BinkSetIOSize(unsigned long iosize)
{
}
BINKAPI int __stdcall BinkOpenDirectSound(unsigned long param)
{
    return 0;
}
BINKAPI int __stdcall BinkSetSoundSystem(void *open, unsigned long param)
{
    return 0;
}
BINKAPI void *__stdcall BinkBufferOpen(void *dest, unsigned long w, unsigned long h, unsigned long flags)
{
    return 0;
}
BINKAPI void __stdcall BinkBufferClose(void *buf)
{
}
BINKAPI int __stdcall BinkBufferLock(void *buf)
{
    return 0;
}
BINKAPI int __stdcall BinkBufferUnlock(void *buf)
{
    return 0;
}
BINKAPI int __stdcall BinkBufferBlit(void *buf, void *rects, unsigned numrects)
{
    return 0;
}
BINKAPI void __stdcall BinkBufferSetOffset(void *buf, int destx, int desty)
{
}

BINKAPI int __stdcall BinkBufferCheckWinPos(void *buf, int *x, int *y)
{
    return 0;
}
BINKAPI int __stdcall BinkBufferClear(void *buf, unsigned long rgb)
{
    return 0;
}
BINKAPI char *__stdcall BinkBufferGetDescription(void *buf)
{
    return 0;
}
BINKAPI char *__stdcall BinkBufferGetError(void)
{
    return 0;
}
BINKAPI int __stdcall BinkBufferSetDirectDraw(void *directdraw, void *primary)
{
    return 0;
}
BINKAPI int __stdcall BinkBufferSetHWND(void *buf, void *wnd)
{
    return 0;
}
BINKAPI int __stdcall BinkBufferSetResolution(int w, int h, int bits)
{
    return 0;
}
BINKAPI int __stdcall BinkBufferSetScale(void *buf, unsigned long w, unsigned long h)
{
    return 0;
}
BINKAPI int __stdcall BinkCheckCursor(void *wnd, int x, int y, int w, int h)
{
    return 0;
}
BINKAPI void __stdcall BinkCloseTrack(void *track)
{
}
BINKAPI unsigned long __stdcall BinkDDSurfaceType(void *surface)
{
    return 0;
}
BINKAPI unsigned long __stdcall BinkDX8SurfaceType(void *surface)
{
    return 0;
}
BINKAPI char *__stdcall BinkGetError(void)
{
    return 0;
}
BINKAPI unsigned long __stdcall BinkGetKeyFrame(void *bink, unsigned long frame, int flags)
{
    return 0;
}
BINKAPI void __stdcall BinkGetRealtime(void *bink, void *realtime, unsigned long frames)
{
}
BINKAPI void __stdcall BinkGetSummary(void *bink, void *summary)
{
}
BINKAPI unsigned long __stdcall BinkGetTrackData(void *track, void *dest)
{
    return 0;
}
BINKAPI unsigned long __stdcall BinkGetTrackID(void *bink, unsigned long index)
{
    return 0;
}
BINKAPI unsigned long __stdcall BinkGetTrackMaxSize(void *bink, unsigned long index)
{
    return 0;
}
BINKAPI unsigned long __stdcall BinkGetTrackType(void *bink, unsigned long index)
{
    return 0;
}
BINKAPI void __stdcall BinkGoto(void *bink, unsigned long frame, int flags)
{
}
BINKAPI int __stdcall BinkIsSoftwareCursor(void *surface, void *cursor)
{
    return 0;
}
BINKAPI void *__stdcall BinkLogoAddress(void)
{
    return 0;
}
BINKAPI int __stdcall BinkOpenMiles(unsigned long param)
{
    return 0;
}
BINKAPI void *__stdcall BinkOpenTrack(void *bink, unsigned long index)
{
    return 0;
}
BINKAPI int __stdcall BinkOpenWaveOut(unsigned long param)
{
    return 0;
}
BINKAPI int __stdcall BinkPause(void *bink, int pause)
{
    return 0;
}
BINKAPI void __stdcall BinkRestoreCursor(int checkcount)
{
}
BINKAPI void __stdcall BinkService(void *bink)
{
}
BINKAPI void __stdcall BinkSetError(const char *error)
{
}
BINKAPI void __stdcall BinkSetFrameRate(unsigned long rate, unsigned long ratediv)
{
}
BINKAPI void __stdcall BinkSetIO(void *open)
{
}
BINKAPI void __stdcall BinkSetMixBins(void *bink, int bins)
{
}
BINKAPI void __stdcall BinkSetPan(void *bink, int pan)
{
}
BINKAPI void __stdcall BinkSetSimulate(unsigned long sim)
{
}
BINKAPI int __stdcall BinkSetSoundOnOff(void *bink, int onoff)
{
    return 0;
}
BINKAPI void __stdcall BinkSetSoundTrack(unsigned long track)
{
}
BINKAPI int __stdcall BinkSetVideoOnOff(void *bink, int onoff)
{
    return 0;
}
BINKAPI void __stdcall BinkSetVolume(void *bink, int volume)
{
}
BINKAPI void __stdcall RADSetMemory(void *alloc, void *release)
{
}
BINKAPI unsigned long __stdcall RADTimerRead(void)
{
    return 0;
}

#define YUV_ARGS12 int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9, int a10, int a11, int a12
#define YUV_ARGS13 YUV_ARGS12, int a13
BINKAPI void __stdcall YUV_blit_16a1bpp(YUV_ARGS13)
{
}
BINKAPI void __stdcall YUV_blit_16a1bpp_mask(YUV_ARGS13)
{
}
BINKAPI void __stdcall YUV_blit_16a4bpp(YUV_ARGS13)
{
}
BINKAPI void __stdcall YUV_blit_16a4bpp_mask(YUV_ARGS13)
{
}
BINKAPI void __stdcall YUV_blit_16bpp(YUV_ARGS12)
{
}
BINKAPI void __stdcall YUV_blit_16bpp_mask(YUV_ARGS12)
{
}
BINKAPI void __stdcall YUV_blit_24bpp(YUV_ARGS12)
{
}
BINKAPI void __stdcall YUV_blit_24bpp_mask(YUV_ARGS12)
{
}
BINKAPI void __stdcall YUV_blit_24rbpp(YUV_ARGS12)
{
}
BINKAPI void __stdcall YUV_blit_24rbpp_mask(YUV_ARGS12)
{
}
BINKAPI void __stdcall YUV_blit_32abpp(YUV_ARGS13)
{
}
BINKAPI void __stdcall YUV_blit_32abpp_mask(YUV_ARGS13)
{
}
BINKAPI void __stdcall YUV_blit_32bpp(YUV_ARGS12)
{
}
BINKAPI void __stdcall YUV_blit_32bpp_mask(YUV_ARGS12)
{
}
BINKAPI void __stdcall YUV_blit_32rabpp(YUV_ARGS13)
{
}
BINKAPI void __stdcall YUV_blit_32rabpp_mask(YUV_ARGS13)
{
}
BINKAPI void __stdcall YUV_blit_32rbpp(YUV_ARGS12)
{
}
BINKAPI void __stdcall YUV_blit_32rbpp_mask(YUV_ARGS12)
{
}
BINKAPI void __stdcall YUV_blit_UYVY(YUV_ARGS12)
{
}
BINKAPI void __stdcall YUV_blit_UYVY_mask(YUV_ARGS12)
{
}
BINKAPI void __stdcall YUV_blit_YUY2(YUV_ARGS12)
{
}
BINKAPI void __stdcall YUV_blit_YUY2_mask(YUV_ARGS12)
{
}
BINKAPI void __stdcall YUV_blit_YV12(YUV_ARGS13)
{
}
BINKAPI void __stdcall YUV_init(unsigned long flags)
{
}
BINKAPI void __stdcall radfree(void *ptr)
{
}
BINKAPI void *__stdcall radmalloc(unsigned long bytes)
{
    return 0;
}

#ifndef VIDEO_H
#define VIDEO_H
#include <windows.h>
#include <mmsystem.h>
#include <avifmt.h>
#define NOAVIFMT
#include <vfw.h>
#include "NetFifo.h"
#include "sound.h"


struct AviIndexEntry {
    FOURCC ckid;
    unsigned long flags;
    unsigned long offset;
    unsigned long length;
};

enum {
    AVI_TIMER_CLOCK = 0x0001,
    AVI_PAUSED = 0x0010,
    AVI_REDRAW = 0x1000,
    AVI_PLAYING = 0x8000
};

struct Video {
    void process();
    int open(char *name);
    int advance_frame();
    int decode_frame_process();
    int read_frame_process();
    int decode_frame(int skip);
    int read_frame();
    int play_frame();
    int find_next_key(int frame);

    int m_reserved;
    AviSound m_audio;
    char pad_05c[0xb8 - 0x5c];
    unsigned int m_flags;
    HMMIO m_mmio;
    MMCKINFO m_movi;
    MMCKINFO m_chunk;
    HIC m_decompressor;
    MainAVIHeader m_mainHeader;
    AVIStreamHeader m_videoHeader;
    AVIStreamHeader m_audioHeader;
    FOURCC m_videoId;
    FOURCC m_audioId;
    BITMAPINFOHEADER m_videoFormat;
    RGBQUAD m_videoColors[256];
    WAVEFORMATEX m_audioFormat;
    DWORD m_indeoSequence[21];
    DWORD m_indeoFrame[11];
    fifo m_frames;
    char pad_67c[0x938 - 0x67c];
    int m_frameCount;
    unsigned long m_startTime;
    int m_readFrame;
    int m_shownFrame;
    AviIndexEntry *m_index;
    AviIndexEntry *m_keyEntry;
    static Video *videos[19];
    static int num_videos;
};


void do_video();

#endif

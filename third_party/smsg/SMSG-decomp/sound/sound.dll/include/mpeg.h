#ifndef MPEG_H
#define MPEG_H

#ifdef USE_MPEG

#include "mss.h"

struct MSS_FLAGS {
    int initialized;
    int end_of_stream;
    int open;
    int looping;
    int stopped;
};

class MilesMpeg {
    MSS_FLAGS m_flags;
    HPROVIDER m_hASI;
    HASISTREAM m_hStream;
    long m_sourcePosition;
    long m_sourceLength;
    WaveBuffer *getNewBuffer(unsigned int size);
    void clear_frames();

public:
    WaveBuffer *m_pFirstFrame;
    WaveBuffer *m_pLastFrame;
    unsigned char *m_pSource;
    unsigned int m_nDecodedFrames;
    unsigned int decode_frames(unsigned int nFrames);
    unsigned int get_frames(WaveBuffer **first_frame, WaveBuffer **last_frame);
    SNDERR seek_to_time(float);
    MilesMpeg();
    ~MilesMpeg();
    SNDERR init(char *fname);
    SNDERR release();
    int isEndOfStream() { return m_flags.end_of_stream; }
    SNDERR reset();
    SNDERR stop();
    SNDERR play();
    CRITICAL_SECTION m_cs;
    void setLoopState(int state) { m_flags.looping = state; }
    static S32 AILCALLBACK Stream_CB(U32 user, void *dest, S32 bytesrequested, S32 offset);
};

#endif

#endif

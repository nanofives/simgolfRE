#include "jgldefs.h"
#include "Video.h"
#include "PaletteList.h"
#include "Log.h"
#include "EmptyString.h"
#include "JackalUtil.h"

Video *Video::videos[19] = {0};
int Video::num_videos = 0;
int g_aviCurrentPlayer = -1;

void do_video()
{
    for (int i = 0; i < Video::num_videos; i++)
        Video::videos[i]->process();
}

void Video::process()
{
    if (!(m_flags & AVI_PLAYING))
        return;
    if (!(m_flags & AVI_PAUSED)) {
        if (play_frame())
            return;
        if (decode_frame_process())
            return;
    }
    read_frame_process();
}

#include <vfw.h>

int Video::open(char *name)
{
    MMCKINFO riff, list, chunk;
    MainAVIHeader header;

    m_mmio = mmioOpenA(name, 0, MMIO_READ | MMIO_ALLOCBUF);
    if (m_mmio == 0)
        return 0;
    riff.fccType = formtypeAVI;
    list.fccType = listtypeAVIHEADER;
    chunk.ckid = ckidAVIMAINHDR;
    if (mmioDescend(m_mmio, &riff, 0, MMIO_FINDRIFF) != 0 || mmioDescend(m_mmio, &list, &riff, MMIO_FINDLIST) != 0
        || mmioDescend(m_mmio, &chunk, &list, MMIO_FINDCHUNK) != 0
        || mmioRead(m_mmio, (HPSTR)&header, sizeof header) != sizeof header) {
        mmioClose(m_mmio, 0);
        m_mmio = 0;
        return 0;
    }
    mmioAscend(m_mmio, &chunk, 0);
    mmioAscend(m_mmio, &list, 0);
    m_movi.fccType = listtypeAVIMOVIE;
    mmioDescend(m_mmio, &m_movi, &riff, MMIO_FINDLIST);
    m_chunk.ckid = 0;
    m_frameCount = header.dwTotalFrames;
    m_readFrame = 0;
    m_shownFrame = 0;
    m_startTime = timeGetTime();
    m_flags = AVI_PLAYING;
    Video::videos[Video::num_videos++] = this;
    return 1;
}

int Video::advance_frame()
{
    if (m_mmio == 0)
        return 7;
    log_say("Entering advance_frame", g_empty, GetMilliseconds(), 0, 0);
    if (m_chunk.ckid != 0) {
        if (mmioAscend(m_mmio, &m_chunk, 0) != 0)
            return 1;
    }
    m_chunk.ckid = 0;
    MMRESULT r = 0;
    MMCKINFO *parent = &m_movi;
    while (r == 0) {
        r = mmioDescend(m_mmio, &m_chunk, parent, 0);
        if (r != 0)
            return 1;
        if (m_chunk.ckid == m_videoId) {
            ++m_readFrame;
            log_say("Exiting advance_frame", g_empty, GetMilliseconds(), 0, 0);
            return 0;
        }
        if (m_chunk.ckid == m_audioId) {
            DWORD now = GetMilliseconds();
            log_say("advance_frame()", "Advancing audio.", now, m_chunk.cksize, 0);
            MMIOINFO *c = m_audio.get_frame(m_chunk.cksize);
            if (c != 0) {
                if (m_chunk.cksize != mmioRead(m_mmio, c->pchBuffer, m_chunk.cksize))
                    return 1;
                c->pchEndRead = c->pchBuffer + m_chunk.cksize;
                m_audio.return_frame(c);
            } else {
                log_say("No mmio!!!", g_empty, 0, 0, 0);
            }
        }
        if (m_chunk.ckid != mmioFOURCC('L', 'I', 'S', 'T'))
            r = mmioAscend(m_mmio, &m_chunk, 0);
    }
    return 1;
}

int Video::decode_frame_process()
{
    log_say("decode_frame_process", "Entering", GetMilliseconds(), 0, 0);
    if (m_flags & AVI_PAUSED)
        return 0;
    play_frame();
    return decode_frame(0);
}

int Video::read_frame_process()
{
    log_say("read_frame_process", "Entering", GetMilliseconds(), 0, 0);
    if (!(m_flags & AVI_PAUSED)) {
        while (play_frame()) {
            if (!decode_frame(0))
                break;
        }
    }
    return read_frame();
}

int Video::decode_frame(int skip)
{
    return 1;
}

int Video::read_frame()
{
    void *data;
    if (m_chunk.cksize == 0) {
        if (m_frames.add(&data, 0, m_readFrame, 1) != 0)
            return 0;
        log_say("read_frame()", "Read data--still frame", m_readFrame, m_frames.m_count, 0);
        advance_frame();
        return 1;
    }
    if (m_frames.add(&data, m_chunk.cksize, m_readFrame, 0) == 0x12)
        return 0;
    if (data == 0) {
        log_say("read_frame()", "Null data in fifo::add", GetMilliseconds(), 0, 0);
        return 0;
    }
    unsigned long left = m_chunk.cksize;
    while (left != 0) {
        long got = mmioRead(m_mmio, (char *)data, (long)(left > 0x10000 ? 0x10000 : left));
        if (got == 0) {
            m_flags &= ~AVI_PLAYING;
            return 0;
        }
        left -= got;
        data = (char *)data + got;
    }
    log_say("read_frame()", "Read data--normal frame", m_readFrame, m_frames.m_count, 0);
    advance_frame();
    return 1;
}

int Video::play_frame()
{
    if (!(m_flags & AVI_PAUSED)) {
        int due;
        if (!(m_flags & AVI_TIMER_CLOCK)) {
            due = m_audio.get_cur_frame();
        } else {
            unsigned long elapsed = GetMilliseconds() - m_startTime;
            due = m_videoHeader.dwRate * elapsed / m_videoHeader.dwScale / 1000;
        }
        if (due < m_shownFrame)
            return 0;
        if (due == m_shownFrame && !(m_flags & AVI_REDRAW))
            return 0;
        {
            unsigned long now = GetMilliseconds();
            log_say("play_frame()", "Entering", now, due - m_shownFrame, 0);
        }
        if (due > m_shownFrame) {
            if (due == m_shownFrame + 1) {
                if (m_frames.m_count == 0) {
                    read_frame();
                    m_flags &= ~AVI_REDRAW;
                } else {
                    m_flags &= ~AVI_REDRAW;
                    decode_frame(0);
                }
            } else {
                int key = find_next_key(due);
                if (key == -1 && m_shownFrame < due) {
                    do {
                        m_flags &= ~AVI_REDRAW;
                        if (decode_frame(0) == 0) {
                            read_frame();
                            decode_frame(0);
                        }
                    } while (m_shownFrame < due);
                }
                log_say("play_frame()", "BEHIND AUDIO", due, m_shownFrame, key);
                int cur = m_shownFrame;
                if (due - cur < key - due) {
                    int stop = due - 1;
                    if (cur < stop) {
                        do {
                            m_flags &= ~AVI_REDRAW;
                            if (decode_frame(1) == 0)
                                read_frame();
                        } while (m_shownFrame < stop);
                    }
                    m_flags &= ~AVI_REDRAW;
                    if (decode_frame(0) == 0) {
                        read_frame();
                        decode_frame(0);
                    }
                } else {
                    if (key > m_readFrame - 1) {
                        m_frames.init(0x28);
                        if (m_readFrame < key) {
                            do
                                advance_frame();
                            while (m_readFrame < key);
                        }
                        read_frame();
                    } else {
                        int head = 0;
                        m_frames.get(&head, 0);
                        if (head < key) {
                            do {
                                m_frames.remove();
                                m_frames.get(&head, 0);
                            } while (head < key);
                        }
                    }
                    m_flags &= ~AVI_REDRAW;
                    decode_frame(0);
                }
            }
        }
    }
    if (m_shownFrame == m_frameCount - 1)
        m_flags &= ~AVI_PLAYING;
    m_flags &= ~AVI_REDRAW;
    log_say("play_frame()", "Exiting", GetMilliseconds(), 0, 0);
    return 1;
}

int Video::find_next_key(int frame)
{
    if (m_mmio && m_index && frame < m_frameCount && frame >= 0) {
        FOURCC id = m_videoId;
        AviIndexEntry *e = m_index;
        while (e->ckid != id)
            e++;
        for (int n = frame; n > 0; n--) {
            e++;
            while (e->ckid != id)
                e++;
        }
        while (frame < m_frameCount - 1 && !(e->flags & 0x10)) {
            e++;
            while (e->ckid != id)
                e++;
            frame++;
        }
        if (frame == m_frameCount - 1) {
            if (e->flags & 0x10) {
                m_keyEntry = e;
                return frame;
            }
        } else if (frame != m_frameCount) {
            m_keyEntry = e;
            return frame;
        }
    }
    return -1;
}

HDRAWDIB OpenDrawDib()
{
    return DrawDibOpen();
}

BOOL CloseDrawDib(HDRAWDIB dib)
{
    return DrawDibClose(dib);
}

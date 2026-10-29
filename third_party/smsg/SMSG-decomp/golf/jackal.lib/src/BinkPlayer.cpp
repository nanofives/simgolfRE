#include "jgldefs.h"
#include <windows.h>

#include "BinkPlayer.h"
#include "Screen.h"
#include "InputPoll.h"

void do_all_keys();

BinkPlayer::BinkPlayer()
{
    m_window = 0;
    m_bink = 0;
    m_buffer = 0;
    m_x = 0;
    m_y = 0;
    m_reserved = 0;
}

BinkPlayer::~BinkPlayer()
{
    Close();
}

void BinkPlayer::SetWindow(void *window)
{
    m_window = window;
}

void BinkPlayer::Close()
{
    if (m_bink)
        BinkClose(m_bink);
    if (m_buffer)
        BinkBufferClose(m_buffer);
    m_bink = 0;
    m_buffer = 0;
}

int BinkPlayer::Play(const char *name, int x, int y, unsigned long soundParam)
{
    if (m_window == 0)
        return 7;
    if (soundParam != 0)
        BinkSetSoundSystem(BinkOpenDirectSound, soundParam);
    BinkSetIOSize(0x9c4000);
    m_bink = BinkOpen(name, BINKIOSIZE);
    if (m_bink != 0 && m_bink->ReadError == 0) {
        m_buffer = BinkBufferOpen(m_window, m_bink->Width, m_bink->Height, BINKBUFFERAUTO);
        if (m_buffer != 0) {
            m_x = x;
            m_y = y;
            BinkBufferSetOffset(m_buffer, x, y);
            g_waitActive = 1;
            ShowCursor(0);
            for (;;) {
                if (BinkWait(m_bink) == 0) {
                    if (g_waitActive == 0)
                        break;
                    if (!ShowNextFrame())
                        break;
                } else {
                    do_all_keys();
                    do_all_mouse();
                }
            }
            ShowCursor(1);
            return 0;
        }
    }
    return 1;
}

bool BinkPlayer::ShowNextFrame()
{
    BinkDoFrame(m_bink);
    if (BinkBufferLock(m_buffer)) {
        BINKBUFFER *b = m_buffer;
        BinkCopyToBuffer(m_bink, b->Buffer, b->BufferPitch, b->Height, 0, 0, b->SurfaceType);
        BinkBufferUnlock(m_buffer);
    }
    BinkBufferBlit(m_buffer, m_bink->FrameRects, BinkGetRects(m_bink, m_buffer->SurfaceType));
    BINK *s = m_bink;
    if (s->FrameNum == s->Frames)
        return false;
    BinkNextFrame(s);
    return true;
}

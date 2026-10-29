#ifndef BINKPLAYER_H
#define BINKPLAYER_H

#include "binkw32.h"

class BinkPlayer {
public:
    BinkPlayer();
    virtual ~BinkPlayer();
    void SetWindow(void *window);
    void Close();
    int Play(const char *name, int x, int y, unsigned long soundParam);
    bool ShowNextFrame();

    BINK *m_bink;
    BINKBUFFER *m_buffer;
    void *m_window;
    int m_x;
    int m_y;
    int m_reserved;
};

#endif

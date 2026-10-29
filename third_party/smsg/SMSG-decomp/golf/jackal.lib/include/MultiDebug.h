
#ifndef MULTIDEBUG_H
#define MULTIDEBUG_H

#include "WidgetBase.h"
#include "TextRenderer.h"
#include "Timer.h"
#include "Network.h"

struct MultiDebug : public GraphicWin {
    MultiDebug();
    ~MultiDebug() { close(); }

    virtual void on_redraw();

    void close();

    int DrawString(const char *s, int x, int y);

    Font m_font;
    Net *m_net;
    Time m_timer;
};
extern MultiDebug multi_debug;

#endif

#ifndef BUTTONGROUP_H
#define BUTTONGROUP_H

#include "BaseButton.h"

struct ButtonGroup {
    int m_reserved;
    BaseButton *m_buttons[32];
    int m_count;
    int m_selectedId;
    int m_style;
    int m_notifyParam;
    int m_quiet;

    int button_click(int id);
};

#endif

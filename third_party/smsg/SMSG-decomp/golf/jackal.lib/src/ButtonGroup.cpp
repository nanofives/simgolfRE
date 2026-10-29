#include "jgldefs.h"
#include "ButtonGroup.h"

int ButtonGroup::button_click(int id)
{
    int newIndex;
    int oldIndex;
    int haveOld = 0;
    int haveNew = 0;
    int i;

    for (i = 0; i < m_count; i++) {
        int buttonId = m_buttons[i]->m_id;
        if (buttonId == m_selectedId) {
            oldIndex = i;
            haveOld = 1;
        }
        if (buttonId == id) {
            haveNew = 1;
            newIndex = i;
        }
    }

    int style = m_style;
    if ((style & 2) && m_selectedId == id) {
        m_selectedId = -1;
        haveNew = 0;
    }
    if (m_selectedId != id || (style & 1)) {
        if (haveOld) {
            m_buttons[oldIndex]->set(0);
            m_buttons[oldIndex]->redraw();
        }
        if (haveNew) {
            m_buttons[newIndex]->set(1);
            m_buttons[newIndex]->redraw();
            m_selectedId = id;
        } else {
            m_selectedId = -1;
        }
        if (!m_quiet && m_count != 0) {
            m_buttons[0]->parent->OnGroupSelSwitched(haveOld ? m_buttons[oldIndex]->m_id : -1,
                haveNew ? m_buttons[newIndex]->m_id : -1);
            if (m_count != 0)
                m_buttons[0]->parent->on_group_clicked(m_notifyParam, haveOld ? m_buttons[oldIndex]->m_id : -1,
                    haveNew ? m_buttons[newIndex]->m_id : -1);
        }
    }

    for (i = 0; i < m_count; i++) {
        if (m_buttons[i]->m_hover != 0) {
            m_buttons[i]->m_hover = 0;
            m_buttons[i]->redraw();
        }
    }
    return 0;
}

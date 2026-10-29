#include "jgldefs.h"
#include <stdlib.h>
#include "PullDown.h"

int PullDown::add_separator(int id)
{
    if (++m_count > 0x100) {
        m_count--;
        return 1;
    }
    if (id == -1)
        return 3;
    for (int i = 0; i < 0x100; i++) {
        if (m_entries[i].id == -1) {
            int last = m_count - 1;
            if (m_entries[last].text != 0)
                return 12;
            m_entries[last].id = id;
            m_entries[last].flags = 5;
            m_shownCount++;
            m_dirty = 1;
            return 0;
        }
    }
    return 1;
}

int PullDown::hide_pos(int index)
{
    if (index == -1)
        return 11;
    if (index >= m_count)
        return 27;
    if (m_entries[index].flags & 1) {
        m_entries[index].flags &= ~1;
        m_shownCount--;
    }
    m_dirty = 1;
    return 0;
}

int PullDown::show_pos(int index)
{
    if (index == -1)
        return 11;
    if (index >= m_count)
        return 27;
    if (!(m_entries[index].flags & 1)) {
        m_entries[index].flags |= 1;
        m_shownCount++;
    }
    m_dirty = 1;
    return 0;
}

void PullDown::delete_all()
{
    for (int i = 0; i < 0x100; i++) {
        if (m_entries[i].text) {
            free(m_entries[i].text);
            m_entries[i].text = 0;
        }
        if (m_entries[i].shortcut) {
            free(m_entries[i].shortcut);
            m_entries[i].shortcut = 0;
        }
        m_entries[i].id = -1;
        m_entries[i].flags = 0;
    }
    m_reserved = 0;
    m_count = 0;
    m_shownCount = 0;
    m_dirty = 1;
}

void PullDown::hide()
{
    release_modal();
    WidgetBase::hide();
}

void PullDown::on_right_down(int x, int y)
{
    on_left_down(x, y);
}

void PullDown::on_right_up(int x, int y)
{
    on_left_up(x, y);
}

int PullDown::id_to_index(int id)
{
    for (int i = 0; i < 0x100; i++) {
        if (m_entries[i].id == -1)
            return -1;
        if (m_entries[i].id == id)
            return i;
    }
    return -1;
}

int PullDown::get_selected()
{
    int i = m_hot;
    if (i == -1 || (m_entries[i].flags & 2))
        return -1;
    return i;
}

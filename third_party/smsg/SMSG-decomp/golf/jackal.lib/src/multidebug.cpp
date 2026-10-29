
#include "jgldefs.h"
#include "MultiDebug.h"
#include "JackalUtil.h"

inline int MultiDebug::DrawString(const char *s, int x, int y)
{
    return s ? write_l(s, x, y, strlen(s)) : x;
}

MultiDebug multi_debug;

MultiDebug::MultiDebug()
{
    m_net = 0;
}

void MultiDebug::close()
{
    m_net = 0;
}

void MultiDebug::on_redraw()
{
    char line[256];
    int x, y, i;

    SetFontsForStyles(&m_font, 0, 0, 0);
    set_text_color(0xff, -1, 2, 2);
    EraseWholeBackground(0);

    y = 3;
    ClearString(line);
    AppendString(line, "Host ID: ");
    AppendInt(line, m_net->m_hostId);
    DrawString(line, 3, y);
    y += text_line_height();

    ClearString(line);
    AppendString(line, "Local ID: ");
    AppendInt(line, m_net->m_localId);
    DrawString(line, 3, y);
    y += text_line_height() * 3 / 2;

    for (i = 0; i < NET_MAX_CONNECTIONS; i++) {
        if (m_net->m_conn[i].m_id == 0)
            continue;

        x = 3;
        ClearString(line);
        AppendInt(line, i);
        strcat(line, ".  ");
        AppendSpace(line);
        x = DrawString(line, x, y);
        DrawString(m_net->m_conn[i].m_name, x, y);
        y += text_line_height();

        ClearString(line);
        AppendString(line, "Guaranteed count: ");
        AppendInt(line, m_net->m_conn[i].m_guaranteedCount);
        DrawString(line, x, y);
        y += text_line_height();

        ClearString(line);
        AppendString(line, "Sending: ");
        AppendInt(line, m_net->m_conn[i].m_sending);
        DrawString(line, x, y);
        y += text_line_height();

        ClearString(line);
        AppendString(line, "Average reply time: ");
        AppendInt(line, m_net->m_conn[i].m_avgReplyTime);
        DrawString(line, x, y);
        y += text_line_height();

        ClearString(line);
        AppendString(line, "Last time: ");
        AppendInt(line, m_net->m_conn[i].m_lastTime);
        DrawString(line, x, y);
        y += text_line_height();

        ClearString(line);
        AppendString(line, "Flags: ");
        AppendInt(line, m_net->m_conn[i].m_flags);
        DrawString(line, x, y);
        y += text_line_height();
    }
}
int g_multiDebugSpare = 0;

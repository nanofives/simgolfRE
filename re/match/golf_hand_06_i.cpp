// FLAGS golf_clean.exe: /O2 /GX
// probe
#include <string.h>
#include <windows.h>
#define VP4(n) virtual void n##0(); virtual void n##1(); virtual void n##2(); virtual void n##3();
#define VP16(n) VP4(n##a) VP4(n##b) VP4(n##c) VP4(n##d)
extern const char s_004c7838[];
extern const char s_004c782c[];
extern void* g_4c1570;
void load431d20(char* name, void* pal, int x, int y, int w, int h, int mode);
// MATCH: golf_clean.exe 0x00431ee0 ?loadPair431ee0@@YAXPAD@Z
void loadPair431ee0(char* name)
{
    char* dot = strchr(name, '.');
    if (dot)
        *dot = 0;
    strcat(name, s_004c7838);
    load431d20(name, g_4c1570, 0, 0, 800, 600, 2);
    if (dot)
        *dot = 0;
    strcat(name, s_004c782c);
    load431d20(name, g_4c1570, 0, 0, 800, 600, 10);
}
struct Edit486 {
    VP16(a) VP16(b) VP16(c) VP16(d) VP4(e) VP4(f)
    virtual void changed();     // 0x120
    char pad[0x574 - 4];
    char* m_text;
    int m_max;
    char pad2[0x5a4 - 0x57c];
    int m_cursor;
    int insertChar(int c, int vk);
    int insertChar4(int c, int vk);
    int insertChar3(int c, int vk);
    int insertChar2(int c, int vk);
    int insertChar1(int c, int vk);
};
int Edit486::insertChar1(int c, int vk)
{
    if (c >= 0) {
        if (c < 0x20)
            return 0;
        if (c == 0 && vk >= 0x60 && vk <= 0x69)
            c = vk - 0x30;
    }
    char* text = m_text;
    if ((int)strlen(text) == m_max) {
        MessageBeep(0);
        return 1;
    }
    char* p = text + m_cursor;
    if (*p == 0)
        p[1] = 0;
    else
        memmove(p + 1, p, strlen(p) + 1);
    m_text[m_cursor] = (char)c;
    m_cursor++;
    changed();
    return 1;
}
// MATCH: golf_clean.exe 0x004866f0 ?insertChar2@Edit486@@QAEHHH@Z
int Edit486::insertChar2(int c, int vk)
{
    if (c >= 0) {
        if (c < 0x20)
            return 0;
        if (c == 0 && vk >= 0x60 && vk <= 0x69)
            c = vk - 0x30;
    }
    if ((int)strlen(m_text) == m_max) {
        MessageBeep(0);
        return 1;
    }
    if (m_text[m_cursor] == 0)
        m_text[m_cursor + 1] = 0;
    else
        memmove(m_text + m_cursor + 1, m_text + m_cursor, strlen(m_text + m_cursor) + 1);
    m_text[m_cursor] = (char)c;
    m_cursor++;
    changed();
    return 1;
}
int Edit486::insertChar3(int c, int vk)
{
    if (c >= 0) {
        if (c < 0x20)
            return 0;
        if (c == 0 && vk >= 0x60 && vk <= 0x69)
            c = vk - 0x30;
    }
    if ((int)strlen(m_text) == m_max) {
        MessageBeep(0);
        return 1;
    }
    char* p = m_text + m_cursor;
    if (*p == 0) {
        p[1] = 0;
    } else {
        int n = strlen(p) + 1;
        memmove(p + 1, p, n);
    }
    m_text[m_cursor] = (char)c;
    m_cursor++;
    changed();
    return 1;
}
int Edit486::insertChar4(int c, int vk)
{
    if (c >= 0) {
        if (c < 0x20)
            return 0;
        if (c == 0 && vk >= 0x60 && vk <= 0x69)
            c = vk - 0x30;
    }
    if ((int)strlen(m_text) == m_max) {
        MessageBeep(0);
        return 1;
    }
    char* p = m_text + m_cursor;
    if (*p)
        memmove(p + 1, p, strlen(p) + 1);
    else
        p[1] = 0;
    m_text[m_cursor] = (char)c;
    m_cursor++;
    changed();
    return 1;
}

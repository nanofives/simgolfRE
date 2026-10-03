// Small golf_clean.exe functions, batch 13 (release build). Names describe behaviour.
// FLAGS golf_clean.exe: /O2
#include <windows.h>
#include <mmsystem.h>
#include <stdlib.h>
#include <string.h>

extern char* g_path83b994;               // 0x83b994
extern char* g_path83b998;               // 0x83b998

// MATCH: golf_clean.exe 0x0048de00 ?setPath83b994@@YAHPBD@Z
int setPath83b994(const char* s)
{
    if (!s)
        return 3;
    if (g_path83b994) {
        free(g_path83b994);
        g_path83b994 = 0;
    }
    g_path83b994 = (char*)malloc(strlen(s) + 1);
    if (!g_path83b994)
        return 5;
    g_path83b994[0] = 0;
    strcat(g_path83b994, s);
    return 0;
}

// MATCH: golf_clean.exe 0x0048de90 ?setPath83b998@@YAHPBD@Z
int setPath83b998(const char* s)
{
    if (!s)
        return 3;
    if (g_path83b998) {
        free(g_path83b998);
        g_path83b998 = 0;
    }
    g_path83b998 = (char*)malloc(strlen(s) + 1);
    if (!g_path83b998)
        return 4;
    g_path83b998[0] = 0;
    strcat(g_path83b998, s);
    return 0;
}

class View47d {
public:
    void toParent(int* x, int* y);       // 0x47b170
    void fromParent(int* x, int* y);     // 0x47b200
    char pad[0x9c];
    unsigned int m_9c;                   // +0x9c: 0x20 = has parent offset, 0x8000 = parent scrolls
    char padA0[0x130 - 0xa0];
    View47d* m_parent;                   // +0x130
    char pad134[0x1ac - 0x134];
    int m_1ac, m_1b0;                    // +0x1ac origin
    char pad1b4[0x1bc - 0x1b4];
    int m_1bc, m_1c0;                    // +0x1bc scroll
};

// MATCH: golf_clean.exe 0x0047b170 ?toParent@View47d@@QAEXPAH0@Z
void View47d::toParent(int* x, int* y)
{
    *x += m_1bc + m_1ac;
    *y += m_1c0 + m_1b0;
    if ((m_9c & 0x20) && m_parent) {
        m_parent->toParent(x, y);
        if (m_9c & 0x8000) {
            *x -= m_parent->m_1ac;
            *y -= m_parent->m_1b0;
        }
    }
}

// MATCH: golf_clean.exe 0x0047b200 ?fromParent@View47d@@QAEXPAH0@Z
void View47d::fromParent(int* x, int* y)
{
    *x -= m_1bc + m_1ac;
    *y -= m_1c0 + m_1b0;
    if ((m_9c & 0x20) && m_parent) {
        m_parent->fromParent(x, y);
        if (m_9c & 0x8000) {
            *x += m_parent->m_1ac;
            *y += m_parent->m_1b0;
        }
    }
}



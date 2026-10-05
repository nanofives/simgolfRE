// FLAGS golf_clean.exe: /O2 /GX
// probe
#include <string.h>
#include <windows.h>
#define VP4(n) virtual void n##0(); virtual void n##1(); virtual void n##2(); virtual void n##3();
#define VP16(n) VP4(n##a) VP4(n##b) VP4(n##c) VP4(n##d)
extern const char s_004e4418[];
struct C488310 {
    int m_0;
    char m_4[0x104 - 4];
    int m_104;
    char pad[0x110 - 0x108];
    char* m_110;
    int find(const char* file, const char* name);
};
// MATCH: golf_clean.exe 0x00488310 ?find@C488310@@QAEHPBD0@Z
int C488310::find(const char* file, const char* name)
{
    char buf[512];
    strcpy(buf, file);
    if (!strchr(buf, '.'))
        strcat(buf, s_004e4418);
    if (!_strcmpi(buf, m_4)) {
        if (*name == '#')
            name++;
        int n = m_104;
        char* s = m_110;
        char* p = s;
        s += 4;
        while (_strcmpi(name, s)) {
            while (*s)
                s++;
            s++;
            if (--n == 0)
                return -1;
            p = s;
            s += 4;
        }
        return *(int*)p;
    }
    return -1;
}

struct Scr47b310 { VP16(a) VP16(b) VP4(c) VP4(d) virtual void v40(); virtual void v41(); virtual int width(); virtual int height(); };
extern Scr47b310* g_83ad50;
struct P47b310 { void adjust(int*, int*); };   // 0x47b200
struct C47b310 {
    char pad[0xa0];
    unsigned int m_a0;
    char pad1[0x130 - 0xa4];
    P47b310* m_130;
    char pad2[0x1ac - 0x134];
    RECT m_1ac;
    RECT m_1bc;
    void moveTo(int, int);   // 0x47b420
    void center();
};
// MATCH: golf_clean.exe 0x0047b310 ?center@C47b310@@QAEXXZ
void C47b310::center()
{
    int x, y;
    if (m_a0 & 2) {
        x = (m_1bc.left - m_1bc.right + (!g_83ad50 ? 0 : g_83ad50->width())) / 2;
        y = (m_1bc.top - m_1bc.bottom + (!g_83ad50 ? 0 : g_83ad50->height())) / 2;
    } else {
        x = (m_1ac.left - m_1ac.right + (!g_83ad50 ? 0 : g_83ad50->width())) / 2;
        y = (m_1ac.top - m_1ac.bottom + (!g_83ad50 ? 0 : g_83ad50->height())) / 2;
    }
    if (m_130)
        m_130->adjust(&x, &y);
    moveTo(x, y);
}

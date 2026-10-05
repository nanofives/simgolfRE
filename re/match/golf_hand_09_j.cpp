// FLAGS golf_clean.exe: /O2 /GX
#include <windows.h>
#define VP4(n) virtual void n##0(); virtual void n##1(); virtual void n##2(); virtual void n##3();
#define VP16(n) VP4(n##a) VP4(n##b) VP4(n##c) VP4(n##d)
struct Surf485d40 {
    virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4();
    virtual char* pixel(int x, int y);
    virtual void v6(); virtual void v7(); virtual void v8();
    virtual void unlock(int);
    virtual void v10(); virtual void v11(); virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15();
    VP16(a) VP16(b)
    virtual void w48(); virtual void w49(); virtual void w50();
    virtual RECT* clip();
};
struct C485d40 {
    int m_0;
    Surf485d40* m_4;
    RECT* clip() { return m_4 ? m_4->clip() : 0; }
    void hline(int x1, int x2, int y, char c);
};
// MATCH: golf_clean.exe 0x00485d40 ?hline@C485d40@@QAEXHHHD@Z
void C485d40::hline(int x1, int x2, int y, char c)
{
    char* p;
    if (!m_4 || y < clip()->top || y >= clip()->bottom || x1 == x2)
        return;
    if (x1 > x2) {
        x1 ^= x2;
        x2 ^= x1;
        x1 ^= x2;
    }
    if (x1 >= clip()->right || x2 < clip()->left)
        return;
    if (x1 < clip()->left)
        x1 = clip()->left;
    if (x2 >= clip()->right)
        x2 = clip()->right - 1;
    if (m_4) {
        p = m_4->pixel(x1, y);
        if (p) {
            __asm {
                push edi
                mov ecx, x2
                sub ecx, x1
                inc ecx
                mov ah, c
                mov edi, p
                shr ecx, 1
            L1:
                mov byte ptr [edi], ah
                add edi, 2
                loop L1
                pop edi
            }
            if (m_4)
                m_4->unlock(1);
        }
    }
}

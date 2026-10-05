// FLAGS golf_clean.exe: /O2 /GX
// probe
#include <windows.h>
#define VP4(n) virtual void n##0(); virtual void n##1(); virtual void n##2(); virtual void n##3();
#define VP16(n) VP4(n##a) VP4(n##b) VP4(n##c) VP4(n##d)
struct Pkt49a { char pad[0x18]; char* m_start; int m_1c; char* m_end; };
struct Queue49a {
    VP4(a) VP4(b)
    virtual void flush();               // 0x20
    VP16(c) VP4(d) VP4(e) virtual void f0();
    virtual Pkt49a* get();              // 0x88
    virtual void release(Pkt49a* p);    // 0x8c
    virtual void v90();
    virtual unsigned count();           // 0x94
};
struct Net49a {
    char pad[0x60];
    int m_60;
    Queue49a m_q;
    char pad2[0xe4 - 0x68];
    HANDLE m_e4;
    unsigned m_e8;
    void sendA(char* p, int n, HANDLE h, int flags, int x);   // 0x497fc0
    void sendB(char* p, int n, HANDLE h, int flags, int x);   // 0x499140
    void pump();
};
// MATCH: golf_clean.exe 0x0049aa70 ?pump@Net49a@@QAEXXZ
void Net49a::pump()
{
    if (m_e8 & 0x40000000) {
        if (m_q.count() < 50)
            m_e8 &= ~0x40000000;
    } else {
        if (m_q.count() > 100) {
            m_e8 |= 0x40000000;
            m_q.flush();
            return;
        }
        Pkt49a* p = m_q.get();
        if (p) {
            if (m_60)
                sendA(p->m_start, p->m_end - p->m_start, m_e4, 0x100, 0);
            else
                sendB(p->m_start, p->m_end - p->m_start, m_e4, 0x100, 0);
            m_q.release(p);
        }
    }
}

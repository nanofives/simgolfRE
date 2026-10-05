// FLAGS golf_clean.exe: /O2 /GX
#include <stdlib.h>
#include <string.h>
#include "golf_hand_07_hdr.h"
struct Src497b40 { int id; char pad[0x10]; unsigned char flag; char name[0x20]; char pad2[0x58 - 0x35]; };
struct Rec497b40 { int id; char name[0x20]; int flag; };
struct Msg497b40 { char n; char pad[3]; Rec497b40 r[1]; };
struct C497b40 {
    char pad[0x170];
    Src497b40 m_170[16];
    char pad2[0x6f8 - 0x170 - 16 * 0x58];
    int m_6f8;
    char pad3[0x77c - 0x6fc];
    int m_77c;
    int m_780;
    void send(void* p, int size, int a, int b, int c);   // 0x499140
    void sendList();
};
// MATCH: golf_clean.exe 0x00497b40 ?sendList@C497b40@@QAEXXZ
void C497b40::sendList()
{
    if (m_77c != m_780)
        return;
    int n = m_6f8;
    if (!n)
        return;
    int size = n * 0x2c + 4;
    Msg497b40* m = (Msg497b40*)malloc(size);
    m->n = n;
    for (int i = 0; i < n; i++) {
        m->r[i].id = m_170[i].id;
        m->r[i].name[0] = 0;
        m_170[i].name[0x1f] = 0;
        strcat(m->r[i].name, m_170[i].name);
        m->r[i].flag = m_170[i].flag;
    }
    send(m, size, 0, 0x10, 1);
    if (m)
        free(m);
}

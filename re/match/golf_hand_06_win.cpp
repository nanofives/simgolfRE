// FLAGS golf_clean.exe: /O2 /GX
// probe
#define VP4(n) virtual void n##0(); virtual void n##1(); virtual void n##2(); virtual void n##3();
struct Notify47 { VP4(a) virtual void v4(); virtual void v5(); virtual void v6(); virtual void notify(); };
struct Hit492 { int hit(int x, int y, int* ox, int* oy, int* info); };   // 0x492b10
typedef void (__cdecl *MouseCb)(int, int);
struct Win47 {
    VP4(a) VP4(b) VP4(c)
    virtual int hit30(int, int, int*);    // 0x30
    virtual int hit34(int, int, int*);    // 0x34
    virtual int hit38(int, int, int*);    // 0x38
    virtual int hit3c(int, int, int*);    // 0x3c
    virtual void v40();
    virtual int hit44(int, int, int*);   // 0x44
    virtual int hit48(int, int, int*);   // 0x48
    virtual int hit4c(int, int, int*);   // 0x4c
    virtual void v50(); virtual void v54(); virtual void v58(); virtual void v5c();
    virtual void v60(); virtual void v64(); virtual void v68(); virtual void v6c(); virtual void v70();
    virtual void miss74(int, int);
    virtual void v78();
    virtual void miss7c(int, int);
    virtual void miss80(int, int);
    virtual void miss84(int, int);
    virtual void miss88(int, int);
    virtual void v8c(); virtual void v90(); virtual void v94();
    virtual void miss98(int, int);
    virtual void miss9c(int, int);
    virtual void va0();
    virtual void outA4(int, int);
    virtual void va8();
    virtual void outAc(int, int);
    virtual void outB0(int, int);
    virtual void outB4(int, int);
    virtual void outB8(int, int);
    virtual void outBc(int, int);
    virtual void outC0(int, int);
    char pad4[0x20 - 4];
    Notify47* m_20; Notify47* m_24; Notify47* m_28; Notify47* m_2c; Notify47* m_30; Notify47* m_34;
    char pad38[0x44 - 0x38];
    Notify47* m_44; Notify47* m_48;
    char pad4c[0x54 - 0x4c];
    Notify47* m_54; Notify47* m_58; Notify47* m_5c; Notify47* m_60; Notify47* m_64; Notify47* m_68; Notify47* m_6c; Notify47* m_70;
    char pad74[0x9c - 0x74];
    unsigned m_9c;
    unsigned char m_a0;
    char pada1[0xbc - 0xa1];
    Hit492 m_bc;
    char padbd[0x23c - 0xbd];
    MouseCb m_23c, m_240, m_244, m_248, m_24c, m_250, m_254, m_258;
    void mouse47bf40(int x, int y, int out);
    void mouse47c010(int x, int y, int out);
    void mouse47c0e0(int x, int y, int out);
    void mouse47c290(int x, int y, int out);
    void mouse47c360(int x, int y, int out);
    void mouse47c430(int x, int y, int out);
    void mouse47c500(int x, int y, int out);
};
extern Win47* g_83ab2c;
// MATCH: golf_clean.exe 0x0047bf40 ?mouse47bf40@Win47@@QAEXHHH@Z
void Win47::mouse47bf40(int x, int y, int out)
{
    int info[4];
    if (m_9c & 0x200000)
        return;
    if (m_a0 & 8)
        return;
    if (!out) {
        int sy = y;
        int sx = x;
        g_83ab2c = this;
        if (m_23c)
            m_23c(sx, sy);
        if (m_bc.hit(sx, sy, &x, &y, info) < 0 || !hit34(x, y, info))
            miss98(sx, sy);
        if (m_44)
            m_44->notify();
    } else {
        outBc(x, y);
        if (m_6c)
            m_6c->notify();
    }
}
// MATCH: golf_clean.exe 0x0047c010 ?mouse47c010@Win47@@QAEXHHH@Z
void Win47::mouse47c010(int x, int y, int out)
{
    int info[4];
    if (m_9c & 0x200000)
        return;
    if (m_a0 & 8)
        return;
    if (!out) {
        int sy = y;
        int sx = x;
        g_83ab2c = this;
        if (m_240)
            m_240(sx, sy);
        if (m_bc.hit(sx, sy, &x, &y, info) < 0 || !hit30(x, y, info))
            miss9c(sx, sy);
        if (m_48)
            m_48->notify();
    } else {
        outC0(x, y);
        if (m_70)
            m_70->notify();
    }
}
// MATCH: golf_clean.exe 0x0047c0e0 ?mouse47c0e0@Win47@@QAEXHHH@Z
void Win47::mouse47c0e0(int x, int y, int out)
{
    int info[4];
    if (m_9c & 0x200000)
        return;
    if (m_a0 & 8)
        return;
    if (!out) {
        int sy = y;
        int sx = x;
        g_83ab2c = this;
        if (m_244)
            m_244(sx, sy);
        if (m_bc.hit(sx, sy, &x, &y, info) < 0 || !hit3c(x, y, info))
            miss74(sx, sy);
        if (m_20)
            m_20->notify();
    } else {
        outA4(x, y);
        if (m_54)
            m_54->notify();
    }
}
// MATCH: golf_clean.exe 0x0047c290 ?mouse47c290@Win47@@QAEXHHH@Z
void Win47::mouse47c290(int x, int y, int out)
{
    int info[4];
    if (m_9c & 0x200000)
        return;
    if (m_a0 & 8)
        return;
    if (!out) {
        int sy = y;
        int sx = x;
        g_83ab2c = this;
        if (m_24c)
            m_24c(sx, sy);
        if (m_bc.hit(sx, sy, &x, &y, info) < 0 || !hit48(x, y, info))
            miss7c(sx, sy);
        if (m_28)
            m_28->notify();
    } else {
        outAc(x, y);
        if (m_5c)
            m_5c->notify();
    }
}
// MATCH: golf_clean.exe 0x0047c360 ?mouse47c360@Win47@@QAEXHHH@Z
void Win47::mouse47c360(int x, int y, int out)
{
    int info[4];
    if (m_9c & 0x200000)
        return;
    if (m_a0 & 8)
        return;
    if (!out) {
        int sy = y;
        int sx = x;
        g_83ab2c = this;
        if (m_250)
            m_250(sx, sy);
        if (m_bc.hit(sx, sy, &x, &y, info) < 0 || !hit38(x, y, info))
            miss80(sx, sy);
        if (m_2c)
            m_2c->notify();
    } else {
        outB0(x, y);
        if (m_60)
            m_60->notify();
    }
}
// MATCH: golf_clean.exe 0x0047c430 ?mouse47c430@Win47@@QAEXHHH@Z
void Win47::mouse47c430(int x, int y, int out)
{
    int info[4];
    if (m_9c & 0x200000)
        return;
    if (m_a0 & 8)
        return;
    if (!out) {
        int sy = y;
        int sx = x;
        g_83ab2c = this;
        if (m_254)
            m_254(sx, sy);
        if (m_bc.hit(sx, sy, &x, &y, info) < 0 || !hit44(x, y, info))
            miss84(sx, sy);
        if (m_30)
            m_30->notify();
    } else {
        outB4(x, y);
        if (m_64)
            m_64->notify();
    }
}
// MATCH: golf_clean.exe 0x0047c500 ?mouse47c500@Win47@@QAEXHHH@Z
void Win47::mouse47c500(int x, int y, int out)
{
    int info[4];
    if (m_9c & 0x200000)
        return;
    if (m_a0 & 8)
        return;
    if (!out) {
        int sy = y;
        int sx = x;
        g_83ab2c = this;
        if (m_258)
            m_258(sx, sy);
        if (m_bc.hit(sx, sy, &x, &y, info) < 0 || !hit4c(x, y, info))
            miss88(sx, sy);
        if (m_34)
            m_34->notify();
    } else {
        outB8(x, y);
        if (m_68)
            m_68->notify();
    }
}

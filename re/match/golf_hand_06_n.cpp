// FLAGS golf_clean.exe: /O2 /GX
// probe
#define VP4(n) virtual void n##0(); virtual void n##1(); virtual void n##2(); virtual void n##3();
#define VP16(n) VP4(n##a) VP4(n##b) VP4(n##c) VP4(n##d)
struct Snd484 {
    VP16(a)
    virtual void setVolume(int v);      // 0x40
    virtual void setPan(int v);         // 0x44
    VP4(e) VP4(f) virtual void v68();
    virtual void setFlags(unsigned f);  // 0x6c
    VP4(g) VP4(h) virtual void i0(); virtual void i1(); virtual void i2(); virtual void v9c(int v);   // 0x9c
    VP4(j) VP4(k) virtual void vc0(); virtual void vc4(); virtual int length();   // 0xc8
};
int create4840e0(Snd484** out, const char* name, int a);
void release484110(Snd484* s);
struct C484c80 {
    VP16(a) VP4(b) virtual void c0(); virtual void c1();
    virtual int isLooped();             // 0x58
    int m_4, m_8;
    char pad[0x40 - 0xc];
    Snd484* m_snd;
    char pad2[0x50 - 0x44];
    char* m_name;
    int m_54;
    unsigned m_58;
    int m_5c;
    int m_60;
    int m_64;
    int load(const char* name);         // 0x4842f0
    int open(const char* name);
    int reopen();
};
// MATCH: golf_clean.exe 0x00484c80 ?open@C484c80@@QAEHPBD@Z
int C484c80::open(const char* name)
{
    int r;
    unsigned f = 0;
    if (!m_snd && (r = create4840e0(&m_snd, name, 1)) != 0)
        return r;
    if (m_58 & 1)
        f = 1;
    if (isLooped())
        f |= 2;
    if (m_58 & 8)
        f |= 0x40;
    if (m_58 & 0x10)
        f |= 0x80;
    if (m_58 & 0x40)
        f |= 0x400;
    if (m_58 & 2)
        f |= 4;
    m_snd->setFlags(f);
    if (!(m_58 & 2)) {
        r = load(name);
        if (r) {
            release484110(m_snd);
            m_snd = 0;
            return r;
        }
        m_64 = m_snd->length();
        m_snd->setVolume(m_4);
        m_snd->v9c(m_5c);
        m_snd->setPan(m_8);
    }
    return 0;
}
// MATCH: golf_clean.exe 0x00484d60 ?reopen@C484c80@@QAEHXZ
int C484c80::reopen()
{
    int r;
    unsigned f = 0;
    char* name = m_name;
    if (name) {
        if (!m_snd && (r = create4840e0(&m_snd, name, 1)) != 0)
            return r;
        if (m_58 & 1)
            f = 1;
        if (isLooped())
            f |= 2;
        if (m_58 & 4)
            f |= 0x11;
        if (m_58 & 8) {
            f |= 0x40;
            if (!(m_58 & 0x20))
                f |= 1;
        }
        if (m_58 & 0x10)
            f |= 0x80;
        if (m_58 & 0x20)
            f |= 0x100;
        if (m_58 & 0x40)
            f |= 0x400;
        m_snd->setFlags(f);
        r = load(name);
        if (r) {
            release484110(m_snd);
            m_snd = 0;
            return r;
        }
        m_64 = m_snd->length();
    } else
        r = 8;
    return r;
}

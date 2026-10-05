// FLAGS golf_clean.exe: /O2 /GX
// probe
#define VP4(n) virtual void n##0(); virtual void n##1(); virtual void n##2(); virtual void n##3();
#define VP16(n) VP4(n##a) VP4(n##b) VP4(n##c) VP4(n##d)
struct Dev493 { VP16(a) VP16(b) VP16(c) VP4(d) virtual void e0(); virtual void e1(); virtual int height(); };   // 0xd8
struct Surf493;
struct A493 { virtual void a(); char pad[0x270]; };
struct B493 {
    virtual void b();
    Dev493* m_dev;
    int height() { return m_dev ? m_dev->height() : 0; }
    void draw(Surf493* s, int a, int b, int c, int d, int y, int e);     // 0x476140
};
struct Img493 : A493, B493 { };
struct Surf493 { char pad[0x18]; int m_18, m_1c; void blit(B493* src, int y, int a, int b); };   // 0x473e60
struct C493f50 {
    char pad[0x213c];
    Img493 m_img;
    char pad2[0x3468 - 0x213c - sizeof(Img493)];
    Surf493* m_top;
    Surf493* m_mid;
    Surf493* m_bottom;
    void layout();
};
// MATCH: golf_clean.exe 0x00493f50 ?layout@C493f50@@QAEXXZ
void C493f50::layout()
{
    m_top->blit(&m_img, 0, 0, 0);
    if (m_img.height() > m_top->m_18 + m_bottom->m_18) {
        int v = m_mid->m_1c;
        m_img.draw(m_mid, 0, 0, 2, 0, m_img.height() - 4, v);
    }
    m_bottom->blit(&m_img, m_img.height() - m_bottom->m_18, 0, 0);
}

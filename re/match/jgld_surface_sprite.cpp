// FLAGS jgld.dll: /Od /ZI /GZ /GX
// jgld.dll surface draws through a temporary Sprite that borrows the surface's pixels (debug build, C++ EH on:
// the frame destroys the Sprite). Surface and Sprite are re-declared here with only what these use
// (jgld_surface.cpp, jgld_sprite.cpp). Names are chosen here.
#include <windows.h>
class Surface;
class Sprite {
public:
    Sprite(void* owner);                         // 0x10014cd0
    virtual ~Sprite();                           // 0x10014de0
    void setPalette(int pal);                    // 0x1000aeb0
    int draw(Surface* dst, int x, int y, int fmt);           // 0x10015480
    int drawZ(Surface* dst, int x, int y, int z, int fmt);   // 0x100165a0
    int m_4, m_8, m_c;
    int m_10;
    void* m_bits;                                // +0x14
    unsigned m_flags;                            // +0x18
    void* m_1c;
    int m_width;                                 // +0x20
    int m_24, m_28;
    int m_pitch;                                 // +0x2c
    int m_30, m_34;
    CRITICAL_SECTION m_cs;
};
class Surface {
public:
    virtual void s0(); virtual void s1(); virtual void s2(); virtual void s3();
    virtual void* lock();                        // slot 4 (+0x10)
    virtual void s5(); virtual void s6(); virtual void s7(); virtual void s8();
    virtual void unlock(int f);                  // slot 9 (+0x24)
    virtual void t10(); virtual void t11(); virtual void t12(); virtual void t13(); virtual void t14(); virtual void t15(); virtual void t16(); virtual void t17(); virtual void t18(); virtual void t19(); virtual void t20(); virtual void t21(); virtual void t22(); virtual void t23(); virtual void t24(); virtual void t25(); virtual void t26(); virtual void t27(); virtual void t28(); virtual void t29(); virtual void t30(); virtual void t31(); virtual void t32(); virtual void t33(); virtual void t34(); virtual void t35(); virtual void t36(); virtual void t37(); virtual void t38(); virtual void t39(); virtual void t40(); virtual void t41(); virtual void t42(); virtual void t43(); virtual void t44(); virtual void t45(); virtual void t46(); virtual void t47(); virtual void t48(); virtual void t49(); virtual void t50(); virtual void t51(); virtual void t52(); virtual void t53();
    virtual int height();                        // slot 54 (+0xd8)
    virtual int depth();                         // slot 55 (+0xdc)
    virtual void t56(); virtual void t57();
    virtual int format();                        // slot 58 (+0xe8)
    int drawTo(Surface* dst, int x, int y);
    int drawToZ(Surface* dst, int x, int y, int z);
    int m_4[8];
    int m_width;                                 // +0x24
    int m_28[6];
    int m_pitch;                                 // +0x40
    int m_44[14];
    int m_7c;
    char m_80[0x4d0 - 0x80];
    int m_palette;                               // +0x4d0
};
// MATCH: jgld.dll 0x10009fd0 ?drawTo@Surface@@QAEHPAV1@HH@Z
int Surface::drawTo(Surface* dst, int x, int y)
{
    Sprite s(0);
    s.m_width = m_width;
    s.m_bits = lock();
    s.m_flags = 0;
    s.m_pitch = m_pitch;
    s.m_30 = height();
    s.m_34 = depth();
    s.m_10 = m_7c;
    s.setPalette(m_palette);
    int r = s.draw(dst, x, y, format());
    unlock(1);
    s.m_bits = 0;
    return r;
}
// MATCH: jgld.dll 0x1000a330 ?drawToZ@Surface@@QAEHPAV1@HHH@Z
int Surface::drawToZ(Surface* dst, int x, int y, int z)
{
    Sprite s(0);
    s.m_width = m_width;
    s.m_bits = lock();
    s.m_flags = 0;
    s.m_pitch = m_pitch;
    s.m_30 = height();
    s.m_34 = depth();
    s.m_10 = m_7c;
    s.setPalette(m_palette);
    int r = s.drawZ(dst, x, y, z, format());
    unlock(1);
    s.m_bits = 0;
    return r;
}

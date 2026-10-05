// FLAGS jgld.dll: /Od /ZI /GZ
// jgld.dll translucent sprite draw onto 16-bit surfaces (source jglsprite.cpp): picks one of the draw16_* blitters
// (jgld_blit16*.cpp) by the alpha parameter in 1/16 steps, 565 or 555 variant by the surface format. Names are chosen here.
#define _DEBUG
#include <windows.h>
#include <crtdbg.h>
class Pal16;
class Surface {
public:
    virtual void s0(); virtual void s1(); virtual void s2(); virtual void s3(); virtual void s4(); virtual void s5(); virtual void s6(); virtual void s7(); virtual void s8(); virtual void s9();
    virtual void t10(); virtual void t11(); virtual void t12(); virtual void t13(); virtual void t14(); virtual void t15(); virtual void t16(); virtual void t17(); virtual void t18(); virtual void t19(); virtual void t20(); virtual void t21(); virtual void t22(); virtual void t23(); virtual void t24(); virtual void t25(); virtual void t26(); virtual void t27(); virtual void t28(); virtual void t29(); virtual void t30(); virtual void t31(); virtual void t32(); virtual void t33(); virtual void t34(); virtual void t35(); virtual void t36(); virtual void t37(); virtual void t38(); virtual void t39(); virtual void t40(); virtual void t41(); virtual void t42(); virtual void t43(); virtual void t44(); virtual void t45(); virtual void t46(); virtual void t47(); virtual void t48(); virtual void t49(); virtual void t50(); virtual void t51(); virtual void t52(); virtual void t53(); virtual void t54(); virtual void t55(); virtual void t56();
    virtual int* format();                   // slot 57 (+0xe4): [0] bits per pixel, [1] 0 = 565, 1 = 555
};
extern float g_1011d378;                     // alpha tolerance (0.03125)
class Sprite {
public:
    virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4(); virtual void v5(); virtual void v6(); virtual void v7();
    virtual int lock();                      // slot 8 (+0x20)
    virtual void unlock(int flag);           // slot 9 (+0x24)
    virtual void v10(); virtual void v11(); virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15(); virtual void v16();
    virtual int drawOpaque(Surface* dst, int x, int y, Pal16* pal);   // slot 17 (+0x44)
    int isRle();                             // 0x100180e0
    int drawAlpha(Surface* dst, int x, int y, float alpha, Pal16* pal, char flag);
    int draw16_1002d890(Surface* dst, int x, int y, char flag);
    int draw16_1002ed60(Surface* dst, int x, int y, char flag);
    int draw16_10030290(Surface* dst, int x, int y, char flag);
    int draw16_100317c0(Surface* dst, int x, int y, char flag);
    int draw16_10032d60(Surface* dst, int x, int y, char flag);
    int draw16_10034300(Surface* dst, int x, int y, char flag);
    int draw16_100358b0(Surface* dst, int x, int y, char flag);
    int draw16_10036e60(Surface* dst, int x, int y, char flag);
    int draw16_10038460(Surface* dst, int x, int y, char flag);
    int draw16_10039a60(Surface* dst, int x, int y, char flag);
    int draw16_1003b060(Surface* dst, int x, int y, char flag);
    int draw16_1003c660(Surface* dst, int x, int y, char flag);
    int draw16_1003dca0(Surface* dst, int x, int y, char flag);
    int draw16_1003f2e0(Surface* dst, int x, int y, char flag);
    int draw16_10040930(Surface* dst, int x, int y, char flag);
    int draw16_10041f80(Surface* dst, int x, int y, char flag);
    int draw16_10043430(Surface* dst, int x, int y, char flag);
    int draw16_10044940(Surface* dst, int x, int y, char flag);
    int draw16_10045e50(Surface* dst, int x, int y, char flag);
    int draw16_100473b0(Surface* dst, int x, int y, char flag);
    int draw16_10048910(Surface* dst, int x, int y, char flag);
    int draw16_10049ea0(Surface* dst, int x, int y, char flag);
    int draw16_1004b430(Surface* dst, int x, int y, char flag);
    int draw16_1004ca10(Surface* dst, int x, int y, char flag);
    int draw16_1004dff0(Surface* dst, int x, int y, char flag);
    int draw16_1004f5d0(Surface* dst, int x, int y, char flag);
    int draw16_10050bb0(Surface* dst, int x, int y, char flag);
    int draw16_100521d0(Surface* dst, int x, int y, char flag);
    int draw16_100537f0(Surface* dst, int x, int y, char flag);
    int draw16_10054e20(Surface* dst, int x, int y, char flag);
    int m_4, m_8, m_c;
    Pal16* m_palette;                        // +0x10
    int m_14, m_18, m_1c;
    int m_depth;                             // +0x20: source bits per pixel
};

// MATCH: jgld.dll 0x10018920 ?drawAlpha@Sprite@@QAEHPAVSurface@@HHMPAVPal16@@D@Z
int Sprite::drawAlpha(Surface* dst, int x, int y, float alpha, Pal16* pal, char flag)
{
    Pal16* saved;
    int result;
    bool is565;
    is565 = true;
    result = 0;
    saved = m_palette;
    if (pal != 0)
        m_palette = pal;
    if (dst == 0)
        return 16;
    if (!lock())
        return 7;
    if (dst->format()[1] == 1)
        is565 = false;
    // blank lines: with /ZI each report line number is relative to the opening brace; the original reports sit at
    // base+0x1e, +0x23, +0x78 and +0x82









    switch (m_depth) {
    case 8:
        switch (dst->format()[0]) {
        case 8:
            _RPTF0(_CRT_WARN, "report text (placeholder)");
            break;
        case 16:
            if (isRle()) {

                _RPTF0(_CRT_WARN, "report text (placeholder)");
            } else if (alpha < 0.0 || alpha > 1.0) {
                result = 3;
            } else if (alpha > g_1011d378 + 0.5) {
                if (alpha > g_1011d378 + 0.75) {
                    if (alpha > g_1011d378 + 0.875) {
                        if (100.0f - g_1011d378 < alpha)
                            result = drawOpaque(dst, x, y, pal);
                        else
                            result = is565 ? draw16_100358b0(dst, x, y, flag) : draw16_10049ea0(dst, x, y, flag);
                    } else if (alpha < 0.875 - g_1011d378) {
                        result = is565 ? draw16_1003dca0(dst, x, y, flag) : draw16_100521d0(dst, x, y, flag);
                    } else {
                        result = is565 ? draw16_10032d60(dst, x, y, flag) : draw16_100473b0(dst, x, y, flag);
                    }
                } else if (alpha < 0.75 - g_1011d378) {
                    if (alpha > g_1011d378 + 0.625) {
                        result = is565 ? draw16_10040930(dst, x, y, flag) : draw16_10054e20(dst, x, y, flag);
                    } else if (alpha < 0.625 - g_1011d378) {
                        result = is565 ? draw16_1003b060(dst, x, y, flag) : draw16_1004f5d0(dst, x, y, flag);
                    } else {
                        result = is565 ? draw16_10038460(dst, x, y, flag) : draw16_1004ca10(dst, x, y, flag);
                    }
                } else {
                    result = is565 ? draw16_10030290(dst, x, y, flag) : draw16_10044940(dst, x, y, flag);
                }
            } else if (alpha < 0.5 - g_1011d378) {
                if (alpha > g_1011d378 + 0.25) {
                    if (alpha > g_1011d378 + 0.375) {
                        result = is565 ? draw16_10039a60(dst, x, y, flag) : draw16_1004dff0(dst, x, y, flag);
                    } else if (alpha < 0.375 - g_1011d378) {
                        result = is565 ? draw16_1003f2e0(dst, x, y, flag) : draw16_100537f0(dst, x, y, flag);
                    } else {
                        result = is565 ? draw16_10036e60(dst, x, y, flag) : draw16_1004b430(dst, x, y, flag);
                    }
                } else if (alpha < 0.25 - g_1011d378) {
                    if (alpha > g_1011d378 + 0.125) {
                        result = is565 ? draw16_1003c660(dst, x, y, flag) : draw16_10050bb0(dst, x, y, flag);
                    } else if (alpha < 0.125 - g_1011d378) {
                        if (alpha > g_1011d378)
                            result = is565 ? draw16_10034300(dst, x, y, flag) : draw16_10048910(dst, x, y, flag);
                        else
                            result = 0;
                    } else {
                        result = is565 ? draw16_100317c0(dst, x, y, flag) : draw16_10045e50(dst, x, y, flag);
                    }
                } else {
                    result = is565 ? draw16_1002ed60(dst, x, y, flag) : draw16_10043430(dst, x, y, flag);
                }
            } else {
                result = is565 ? draw16_1002d890(dst, x, y, flag) : draw16_10041f80(dst, x, y, flag);
            }
            break;
        default:































            _RPTF0(_CRT_WARN, "report text (placeholder)");
            result = 23;
        }
        break;
    default:





        _RPTF0(_CRT_WARN, "report text (placeholder)");
        result = 23;
    }
    m_palette = saved;
    unlock(1);
    return result;
}

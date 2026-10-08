extern int g_10128538;                       // mask pitch
extern int g_10128534;                       // mask row skip
extern unsigned char g_10128568;             // mask threshold, read by the __asm loops
// ----
// MATCH: jgld.dll ADDR ?NAME@Sprite@@QAEHPAVSurface@@HHDECOR@Z
int Sprite::NAME(Surface* dst, int x, int yPARAMS)
{
    RECT rc;
    RECT orig;
    unsigned short* d;
    unsigned char* s;
    unsigned char* m;
    Pal16* pal;
    if (m_bits == 0 || dst == 0 || !dst->lock())
        return 3;
    if (mask == 0 || !mask->lock()) {
        dst->unlock(1);
        return 3;
    }
    if (m_palette)
        pal = m_palette;
    else
        pal = g_palClient1->m_palette;
    if (pal == 0) {
        mask->unlock(2);
        dst->unlock(1);
        return 16;
    }
    g_1012858c = (float)abs(g_10122dc0) / g_10122dc8;
    g_10128590 = (float)abs(g_10122dc4) / g_10122dc8;
    g_1012857c = m_pitch;
    g_1012855c = dst->pitch();
    g_10128538 = mask->pitch();
    s = m_bits;
    g_10128568 = level;
    g_10128530 = color;
    switch (dst->format()[1]) {
    case 0:
        g_1012856c = pal->table565();
        break;
    case 1:
        g_1012856c = pal->table555();
        break;
    default:
        return 1;
    }
    if (abs(g_10122dc0) == abs(g_10122dc8) && abs(g_10122dc4) == abs(g_10122dc8)) {
        setRect(&rc, x, y, m_width, m_height);
        orig = rc;
        if (!IntersectRect(&rc, &rc, dst->clipRect())) {
            mask->unlock(2);
            dst->unlock(1);
            return 0;
        }
        if (g_10122dc0 >= 0) {
            if (g_10122dc4 >= 0) {
                s = m_bits + (rc.left - orig.left) + (rc.top - orig.top) * g_1012857c;
                d = (unsigned short*)dst->bits() + rc.left + rc.top * g_1012855c;
                m = mask->bits8() + rc.left + rc.top * g_10128538;
                g_10128578 = rc.bottom - rc.top;
                g_10128564 = rc.right - rc.left;
                g_1012853c = g_1012857c - g_10128564;
                g_10128560 = (g_1012855c - g_10128564) * 2;
                g_10128534 = g_10128538 - g_10128564;
    ASM1
            } else {
                s = m_bits + (rc.left - orig.left) + (rc.top - orig.top) * g_1012857c;
                d = (unsigned short*)dst->bits() + rc.left + rc.top * g_1012855c;
                g_10128578 = rc.bottom - rc.top;
                g_10128564 = rc.right - rc.left;
                g_1012853c = g_1012857c + g_10128564;
                g_10128560 = (g_1012855c - g_10128564) * 2;
                g_10128534 = g_10128538 - g_10128564;
    ASM2
            }
        } else {
            if (g_10122dc4 >= 0) {
                s = m_bits + (rc.left - orig.left) + (rc.top - orig.top) * g_1012857c;
                d = (unsigned short*)dst->bits() + rc.left + rc.top * g_1012855c;
                g_10128578 = rc.bottom - rc.top;
                g_10128564 = rc.right - rc.left;
                g_1012853c = g_1012857c + g_10128564;
                g_10128560 = (g_1012855c - g_10128564) * 2;
                g_10128534 = g_10128538 - g_10128564;
    ASM3
            } else {
                s = m_bits + (rc.left - orig.left) + (rc.top - orig.top) * g_1012857c;
                d = (unsigned short*)dst->bits() + rc.left + rc.top * g_1012855c;
                g_10128578 = rc.bottom - rc.top;
                g_10128564 = rc.right - rc.left;
                g_1012853c = g_1012857c - g_10128564;
                g_10128560 = (g_1012855c - g_10128564) * 2;
                g_10128534 = g_1012855c - g_10128564;
    ASM4
            }
        }
    } else {
        mask->unlock(2);
        dst->unlock(2);
    }
    dst->unlock(2);
    mask->unlock(2);
    return 0;
}

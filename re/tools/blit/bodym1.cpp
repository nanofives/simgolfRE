// MATCH: jgld.dll ADDR ?NAME@Sprite@@QAEHPAVSurface@@HHDECOR@Z
int Sprite::NAME(Surface* dst, int x, int yPARAMS)
{
    RECT rc;
    unsigned short* d;
    unsigned char* s;
    Pal16* pal;
    unsigned short* m;
    m = 0;
    if (mask != 0)
        m = (unsigned short*)mask->bits();
    else
        return 3;
    g_1012858c = (float)abs(g_10122dc0) / den;
    g_10128590 = (float)abs(g_10122dc4) / den;
    setRect(&rc, x + (int)g_1012858c, y + (int)g_10128590, (int)(m_width * g_1012858c), (int)(m_height * g_10128590));
    if (!intersect(&rc, &rc, dst->boundsRect())) {
        dst->unlock(1);
        return 0;
    }
    if (m_palette)
        pal = m_palette;
    else
        pal = g_palClient1->m_palette;
    if (pal == 0) {
        dst->unlock(1);
        return 16;
    }
    g_1012857c = m_pitch;
    g_1012855c = dst->pitch();
    s = m_bits;
    d = (unsigned short*)dst->bits() + rc.left + rc.top * g_1012855c;
    m += rc.left + rc.top * g_1012855c;
    g_10128564 = rc.right - rc.left;
    g_10128578 = rc.bottom - rc.top;
    g_10128560 = (g_1012855c - g_10128564) * 2;
    g_10128588 = (den << 16) / abs(scale);
    g_10128540 = (den << 16) / abs(scale);
    g_10128548 = p6;
    switch (dst->format()[1]) {
    case 0:
        g_1012856c = pal->table565();
        break;
    case 1:
        g_1012856c = pal->table555();
        break;
    default:
        dst->unlock(1);
        return 1;
    }
    if (scale >= 0) {
        g_10128570 = g_10128588 * g_10128564 >> 16;
        g_10128584 = g_10128540 * g_10128578 >> 16;
        g_1012853c = g_1012857c - g_10128570;
        *g_10122e20 = (g_10128540 >> 16) * g_1012857c - g_1012857c + g_1012853c;
    ASM1
    }
    return 0;
}

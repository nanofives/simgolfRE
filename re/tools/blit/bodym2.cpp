// MATCH: jgld.dll ADDR ?NAME@Sprite@@QAEHPAVSurface@@HHDECOR@Z
int Sprite::NAME(Surface* dst, int x, int yPARAMS)
{
    RECT rc;
    RECT orig;
    unsigned short* d;
    unsigned char* s;
    Pal16* pal;
    unsigned short* m;
    m = 0;
    if (mask != 0)
        m = (unsigned short*)mask->bits();
    else
        return 3;
    if (m_bits == 0 || dst == 0 || !dst->lock())
        return 3;
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
    g_10128548 = p4;
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
    if (abs(g_10122dc0) == abs(g_10122dc8) && abs(g_10122dc4) == abs(g_10122dc8)) {
        setRect(&rc, x, y, m_width, m_height);
        orig = rc;
        if (!IntersectRect(&rc, &rc, dst->boundsRect())) {
            dst->unlock(1);
            return 0;
        }
        s = m_bits + (rc.left - orig.left) + (rc.top - orig.top) * g_1012857c;
        d = (unsigned short*)dst->bits() + rc.left + rc.top * g_1012855c;
        m = (unsigned short*)mask->bits() + rc.left + rc.top * mask->pitch();
        g_10128578 = rc.bottom - rc.top;
        g_10128564 = rc.right - rc.left;
        g_1012853c = g_1012857c - g_10128564;
        g_10128560 = (g_1012855c - g_10128564) * 2;
    ASM1
    }
    dst->unlock(2);
    return 0;
}

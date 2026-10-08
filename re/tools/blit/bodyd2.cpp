// MATCH: jgld.dll ADDR ?NAME@Sprite@@QAEHFDECOR@Z
int Sprite::NAME(FPARAMS)
{
    RECT rc;
    RECT orig;
    unsigned short* d;
    unsigned short* d2;
    unsigned char* s;
    unsigned char* s2;
    Pal16* pal;
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
    setRect(&rc, x, y, m_width, m_height);
    orig = rc;
    if (!IntersectRect(&rc, &rc, dst->clipRect())) {
        dst->unlock(1);
        return 0;
    }
    s = m_bits + (rc.left - orig.left) + (rc.top - orig.top) * g_1012857c;
    s2 = other->m_bits + (rc.left - orig.left) + (rc.top - orig.top) * g_1012857c;
    d = (unsigned short*)dst->bits() + rc.left + rc.top * g_1012855c;
    d2 = (unsigned short*)dst2->bits() + rc.left + rc.top * g_1012855c;
    g_10128578 = rc.bottom - rc.top;
    g_10128564 = rc.right - rc.left;
    g_1012853c = g_1012857c - g_10128564;
    g_10128560 = (g_1012855c - g_10128564) * 2;
    ASM1
    dst->unlock(2);
    return 0;
}

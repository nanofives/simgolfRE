// MATCH: jgld.dll ADDR ?NAME@Sprite@@QAEHPAVSurface@@HHDECOR@Z
int Sprite::NAME(Surface* dst, int x, int yPARAMS)
{
    RECT rc;
    RECT orig;
    unsigned char* d;
    unsigned char* s;
    int offX;
    int offY;
    if (m_bits == 0 || dst == 0 || !dst->lock())
        return 3;
    g_1012868c = (float)abs(g_10122dc0) / g_10122dc8;
    g_10128690 = (float)abs(g_10122dc4) / g_10122dc8;
    g_10128678 = m_pitch;
    g_10128644 = dst->pitch();
    s = m_bits;
    g_1012862c = table;
    if (abs(g_10122dc0) == abs(g_10122dc8) && abs(g_10122dc4) == abs(g_10122dc8)) {
        setRect(&rc, x, y, m_width, m_height);
        orig = rc;
        if (!IntersectRect(&rc, &rc, dst->boundsRect())) {
            dst->unlock(1);
            return 0;
        }
        if (g_10122dc0 >= 0) {
            if (g_10122dc4 >= 0) {
                s = m_bits + (rc.left - orig.left) + (rc.top - orig.top) * g_10128678;
                d = dst->bits8() + rc.left + rc.top * g_10128644;
                g_10128670 = rc.bottom - rc.top;
                g_10128650 = rc.right - rc.left;
                g_1012861c = g_10128678 - g_10128650;
                g_10128648 = g_10128644 - g_10128650;
    ASM1
            } else {
                s = m_bits + (rc.left - orig.left) + (rc.top - orig.top) * g_10128678;
                d = dst->bits8() + rc.left + rc.top * g_10128644;
                g_10128670 = rc.bottom - rc.top;
                g_10128650 = rc.right - rc.left;
                g_1012861c = g_10128678 + g_10128650;
                g_10128648 = g_10128644 - g_10128650;
    ASM2
            }
        } else {
            if (g_10122dc4 >= 0) {
                s = m_bits + (rc.left - orig.left) + (rc.top - orig.top) * g_10128678;
                d = dst->bits8() + rc.left + rc.top * g_10128644;
                g_10128670 = rc.bottom - rc.top;
                g_10128650 = rc.right - rc.left;
                g_1012861c = g_10128678 + g_10128650;
                g_10128648 = g_10128644 - g_10128650;
    ASM3
            } else {
                s = m_bits + (rc.left - orig.left) + (rc.top - orig.top) * g_10128678;
                d = dst->bits8() + rc.left + rc.top * g_10128644;
                g_10128670 = rc.bottom - rc.top;
                g_10128650 = rc.right - rc.left;
                g_1012861c = g_10128678 - g_10128650;
                g_10128648 = g_10128644 - g_10128650;
    ASM4
            }
        }
    } else {
        setRect(&rc, x + (int)g_1012868c, y + (int)g_10128690, (int)(m_width * g_1012868c), (int)(m_height * g_10128690));
        orig = rc;
        if (!intersect(&rc, &rc, dst->boundsRect())) {
            dst->unlock(1);
            return 0;
        }
        d = dst->bits8() + rc.left + rc.top * g_10128644;
        g_10128644 = dst->pitch();
        g_10128650 = rc.right - rc.left;
        g_10128670 = rc.bottom - rc.top;
        g_10128648 = g_10128644 - g_10128650;
        g_10128688 = (g_10122dc8 << 16) / abs(g_10122dc0);
        g_10128624 = (g_10122dc8 << 16) / abs(g_10122dc4);
        if (g_10122dc0 >= 0) {
            if (g_10122dc4 >= 0) {
                offX = rc.left - orig.left;
                offY = rc.top - orig.top;
                g_1012863c = g_10128688 * offX;
                g_10128640 = g_10128624 * offY;
                g_10128660 = (g_1012863c & 0xffff) + g_10128688 * g_10128650 >> 16;
                g_10128680 = (g_10128640 & 0xffff) + g_10128624 * g_10128670 >> 16;
                g_1012861c = g_10128678 - g_10128660;
                s = m_bits + (g_10128640 >> 16) * g_10128678 + (g_1012863c >> 16);
                *g_10122e58 = (g_10128624 >> 16) * g_10128678 - g_10128678 + g_1012861c;
                *g_10122e54 = *g_10122e58 + g_10128678;
    ASM5
            } else {
                offX = rc.left - orig.left;
                offY = rc.top - orig.top;
                g_1012863c = g_10128688 * offX;
                g_10128640 = g_10128624 * offY;
                g_10128660 = (g_1012863c & 0xffff) + g_10128688 * g_10128650 >> 16;
                g_10128680 = (g_10128640 & 0xffff) + g_10128624 * g_10128670 >> 16;
                s = m_bits + (m_height - 1 - (g_10128640 >> 16)) * g_10128678 + (g_1012863c >> 16);
                *g_10122e58 = (g_10128624 >> 16) * g_10128678 + g_10128660;
                *g_10122e54 = *g_10122e58 + g_10128678;
    ASM6
            }
        } else {
            if (g_10122dc4 >= 0) {
                offX = rc.left - orig.left;
                offY = rc.top - orig.top;
                g_1012863c = g_10128688 * offX;
                g_10128640 = g_10128624 * offY;
                g_10128660 = (g_1012863c & 0xffff) + g_10128688 * g_10128650 >> 16;
                g_10128680 = (g_10128640 & 0xffff) + g_10128624 * g_10128670 >> 16;
                s = m_bits + (g_10128640 >> 16) * g_10128678 + m_width - 1 - (g_1012863c >> 16);
                *g_10122e58 = (g_10128624 >> 16) * g_10128678 + g_10128660;
                *g_10122e54 = *g_10122e58 + g_10128678;
    ASM7
            } else {
                offX = rc.left - orig.left;
                offY = rc.top - orig.top;
                g_1012863c = g_10128688 * offX;
                g_10128640 = g_10128624 * offY;
                g_10128660 = (g_1012863c & 0xffff) + g_10128688 * g_10128650 >> 16;
                g_10128680 = (g_10128640 & 0xffff) + g_10128624 * g_10128670 >> 16;
                s = m_bits + (m_height - 1 - (g_10128640 >> 16)) * g_10128678 + m_width - 1 - (g_1012863c >> 16);
                g_1012861c = -g_10128678 + g_10128660;
                *g_10122e58 = (g_10128624 >> 16) * g_10128678 + -g_10128660;
                *g_10122e54 = *g_10122e58 + g_10128678;
    ASM8
            }
        }
    }
    dst->unlock(2);
    return 0;
}

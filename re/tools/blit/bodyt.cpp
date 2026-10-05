extern float g_10128518;                     // horizontal scale
extern float g_1012851c;                     // vertical scale
extern int g_1012850c;                       // source pitch (pixels)
extern int g_101284ec;                       // destination pitch (pixels)
extern int g_101284fc;                       // 4th parameter, read by the __asm loop
extern int g_10128508;                       // rows
extern int g_101284f4;                       // columns
extern int g_101284d8;                       // source row skip (bytes)
extern int g_101284f0;                       // destination row skip
extern int g_10128514;                       // 16.16 horizontal step
extern int g_101284dc;                       // 16.16 vertical step
// ----
// MATCH: jgld.dll ADDR ?NAME@Sprite@@QAEHPAVSurface@@HHDECOR@Z
int Sprite::NAME(Surface* dst, int x, int yPARAMS)
{
    RECT rc;
    RECT orig;
    unsigned short* d;
    unsigned short* s;
    if (m_bits == 0 || dst == 0 || !dst->lock())
        return 3;
    g_10128518 = (float)abs(g_10122dc0) / g_10122dc8;
    g_1012851c = (float)abs(g_10122dc4) / g_10122dc8;
    m_28 = 0xff;
    g_1012850c = m_pitch;
    g_101284ec = dst->pitch();
    s = (unsigned short*)m_bits;
    g_101284fc = p4;
    if (abs(g_10122dc0) == abs(g_10122dc8) && abs(g_10122dc4) == abs(g_10122dc8)) {
        setRect(&rc, x, y, m_width, m_height);
        orig = rc;
        if (!IntersectRect(&rc, &rc, dst->boundsRect())) {
            dst->unlock(1);
            return 0;
        }
        if (g_10122dc0 >= 0 && g_10122dc4 >= 0) {
            s = (unsigned short*)m_bits + (rc.left - orig.left) + (rc.top - orig.top) * g_1012850c;
            d = (unsigned short*)dst->bits() + rc.left + rc.top * g_101284ec;
            g_10128508 = rc.bottom - rc.top;
            g_101284f4 = rc.right - rc.left;
            g_101284d8 = (g_1012850c - g_101284f4) * 2;
            g_101284f0 = (g_101284ec - g_101284f4) * 2;
    ASM1
        }
    } else {
        setRect(&rc, x, y, (int)(m_width * g_10128518), (int)(m_height * g_1012851c));
        orig = rc;
        if (!intersect(&rc, &rc, dst->boundsRect())) {
            dst->unlock(1);
            return 0;
        }
        d = (unsigned short*)dst->bits() + rc.left + rc.top * g_101284ec;
        g_101284ec = dst->pitch();
        g_101284f4 = rc.right - rc.left;
        g_10128508 = rc.bottom - rc.top;
        g_101284f0 = g_101284ec - g_101284f4;
        g_10128514 = (g_10122dc8 << 16) / abs(g_10122dc0);
        g_101284dc = (g_10122dc8 << 16) / abs(g_10122dc4);
    }
    dst->unlock(1);
    return 0;
}

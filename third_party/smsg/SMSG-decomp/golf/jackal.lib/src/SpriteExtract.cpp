#include "jgldefs.h"
#include "Sprite.h"

int SpriteObject::extract_8(AssetSurface *src, int sx, int sy, int w, int h, int memType, unsigned char strict)
{
    unsigned char *p, *q;
    int pitch, x, i, cnt, right, r, xx, yy, wid, n;
    void *buf;

    if (src == 0 || src->m_surface == 0 || src->m_surface->get_data() == 0)
        return 3;
    close();
    if (w == 0)
        return 3;
    if (h == 0)
        return 3;
    if (sx + w > src->GetSurfaceWidth())
        return 3;
    if (sy + h > src->GetSurfaceHeight())
        return 3;

    m_surface = graphsys->get_sprite_data(this ? &m_surface : 0, memType);
    m_memType = memType;
    m_surface->set_seethru(0xff);
    m_width = w;
    m_height = h;

    p = src->GetPixelPtr(sx, sy);
    if (p == 0) {
        src->Unlock(1);
        return 7;
    }
    pitch = src->m_surface ? src->m_surface->get_pitch() : 0;

    m_top = 0;
    for (i = 0; i < h; i++) {
        for (x = 0; x < w; x++) {
            if (p[x] != 0xff)
                goto found;
            if (!strict)
                goto doneTop;
        }
        p += pitch;
        m_top++;
    }
found:
doneTop:
    if (m_top == h) {
        src->Unlock(2);
        return 9;
    }
    src->Unlock(1);

    q = src->GetPixelPtr(sx, sy + h - 1);
    m_imageHeight = h - m_top;
    for (i = 0; i < h; i++) {
        for (x = w; x > 0; x--) {
            if (q[x - 1] != 0xff || !strict)
                goto doneBottom;
        }
        q -= pitch;
        m_imageHeight--;
    }
doneBottom:
    if (m_imageHeight == 0) {
        src->Unlock(2);
        return 9;
    }
    src->Unlock(1);

    yy = sy + m_top;
    p = src->GetPixelPtr(sx, yy);
    x = 0;
    m_left = w;
    for (i = 0; i < m_imageHeight; i++) {
        for (; x < w; x++) {
            if (p[x] != 0xff || !strict)
                break;
        }
        if (x < m_left)
            m_left = x;
        p += pitch;
        x = 0;
    }
    src->Unlock(1);

    yy = sy + m_top;
    p = src->GetPixelPtr(sx, yy);
    cnt = 0;
    right = w;
    for (i = 0; i < m_imageHeight; i++) {
        for (x = w; x > 0; x--) {
            if (p[x - 1] != 0xff || !strict)
                break;
            cnt++;
        }
        if (cnt < right)
            right = cnt;
        cnt = 0;
        p += pitch;
    }
    src->Unlock(1);

    m_imageWidth = (w - right) - m_left;
    if (m_imageWidth == 0)
        return 9;

    buf = malloc(m_imageWidth * m_imageHeight);
    if (buf == 0) {
        close();
        return 4;
    }

    yy = m_top + sy;
    xx = sx + m_left;
    wid = m_imageWidth;
    n = m_imageHeight;
    p = src->GetPixelPtr(xx, yy);
    {
        unsigned char *d = (unsigned char *)buf;
        while (n--) {
            memcpy(d, p, wid);
            p += pitch;
            d += wid;
        }
    }

    r = m_surface->init(buf, m_imageWidth, m_imageHeight, 8, memType, src->m_surface->get_current_palette());
    if (r != 0) {
        close();
        return r;
    }
    free(buf);
    src->Unlock(1);
    return 0;
}

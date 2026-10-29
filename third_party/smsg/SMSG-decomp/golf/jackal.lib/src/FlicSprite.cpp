
#include "jgldefs.h"
#include "FlicAnim.h"

FlicSprite::FlicSprite()
{
    m_bank = 0;
    m_chunk = 0;
    m_frame = 0;
    m_keyFrames = 0;
    m_seq = -1;
}

static inline void SetPalette(SpriteObject *s, PaletteBase *pal)
{
    if (s->m_surface && pal)
        s->m_surface->set_palette(pal->pal);
}

int FlicSprite::init(FlicAnim *bank, int keyFrames)
{
    if (bank == 0)
        return 16;
    if (bank->m_data == 0)
        return 7;
    close();
    m_bank = bank;
    SpriteObject *sp = &m_canvas;
    int rc = sp->create_blank_8(bank->m_header->width, bank->m_header->height);
    if (rc != 0) {
        close();
        return rc;
    }
    m_canvas.m_left = bank->m_header->originX;
    m_canvas.m_top = bank->m_header->originY;
    m_canvas.m_width = bank->m_header->frameWidth;
    m_canvas.m_height = bank->m_header->frameHeight;
    if (bank->m_palette)
        SetPalette(&m_canvas, bank->m_palette);
    else
        SetPalette(&m_canvas, g_activePalettePtr);
    if (keyFrames) {
        m_keyFrames = new SpriteObject[bank->m_header->sequences];
        if (m_keyFrames) {
            unsigned long mask = bank->m_header->seqMask;
            for (int i = 0; i < bank->m_header->sequences; i++) {
                if (mask == 0 || (mask & (1 << i))) {
                    rc = m_keyFrames[i].create_blank_8(bank->m_header->width, bank->m_header->height);
                    if (rc != 0) {
                        close();
                        return rc;
                    }
                    m_keyFrames[i].m_left = bank->m_header->originX;
                    m_keyFrames[i].m_top = bank->m_header->originY;
                    m_keyFrames[i].m_width = bank->m_header->frameWidth;
                    m_keyFrames[i].m_height = bank->m_header->frameHeight;
                    if (!seek_anim(i, -1, &m_keyFrames[i]))
                        return 1;
                }
            }
        }
    }
    return !seek_anim(0, -1, 0);
}

void FlicSprite::close()
{
    m_bank = 0;
    m_chunk = 0;
    m_frame = 0;
    m_seq = -1;
    if (m_keyFrames) {
        delete[] m_keyFrames;
        m_keyFrames = 0;
    }
    m_canvas.close();
}

SpriteObject *FlicSprite::current_sprite()
{
    if (!m_bank)
        return 0;
    return &m_canvas;
}

SpriteObject *FlicSprite::NextFrame()
{
    if (!m_bank)
        return 0;
    return m_bank->next_sprite(this, 0);
}

SpriteObject *FlicSprite::seek_anim(int seq, int frame, SpriteObject *dst)
{
    if (!m_bank)
        return 0;
    return m_bank->seek_anim(seq, frame, this, dst);
}

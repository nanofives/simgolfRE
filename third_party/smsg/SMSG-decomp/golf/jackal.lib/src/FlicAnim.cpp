#include "jgldefs.h"
#include "HeapNew.h"
#include "FlicAnim.h"
#include "crt.h"
#include "PaletteList.h"
#include "ClassListPush.h"
#include "WidgetBase.h"

static FLCPalette *s_poolPalette = 0;

FlicAnim::FlicAnim()
{
    m_50 = 0;
    m_file = 0;
    m_6c = 0;
    m_70 = 0;
    m_5c = 0;
    m_60 = 0;
    m_palette64 = 0;
    m_palette68 = 0;
    m_palette = 0;
    m_ownPalette = 0;
    m_data = 0;
    m_header = 0;
    m_size = 0;
    m_seqStarts = 0;
    m_frameChunks = 0;
}

void FlicAnim::close()
{
    m_palette = 0;
    if (m_file) {
        delete m_file;
        m_file = 0;
        m_data = 0;
    }
    if (m_6c) {
        m_6c->Release(1);
        m_6c = 0;
    }
    if (m_70) {
        m_70->Release(1);
        m_70 = 0;
    }
    if (m_palette64) {
        delete m_palette64;
        m_palette64 = 0;
    }
    if (m_palette68) {
        delete m_palette68;
        m_palette68 = 0;
    }
    if (m_5c) {
        delete m_5c;
        m_5c = 0;
    }
    if (m_60) {
        delete m_60;
        m_60 = 0;
    }
    if (m_ownPalette) {
        delete m_ownPalette;
        m_ownPalette = 0;
    }
    if (m_data) {
        delete[] m_data;
        m_data = 0;
    }
    if (m_seqStarts) {
        delete[] m_seqStarts;
        m_seqStarts = 0;
    }
    if (m_frameChunks) {
        delete[] m_frameChunks;
        m_frameChunks = 0;
    }
    m_50 = 0;
    m_header = 0;
    m_size = 0;
}










int FlicAnim::confirm_data(int flags)
{
    if (m_file == 0)
        return 7;
    m_size = m_file->GetSize();
    m_header = (FlcHeader *)m_file->GetData();
    if (m_header->magic != 0xaf12)
        return 0x16;
    if (m_header->depth != 8)
        return 0x17;
    if (m_header->creator == 0xf1f1f2f2) {
        if (flags & 4) {
            m_data = new char[m_size];
            if (m_data == 0) {
                close();
                return 4;
            }
            memcpy(m_data, m_file->GetData(), m_size);
            delete m_file;
            m_file = 0;
        } else {
            m_data = (char *)m_file->GetData();
        }
        m_header = (FlcHeader *)m_data;
    } else {
        m_data = new char[m_size];
        if (m_data == 0) {
            close();
            return 4;
        }
        memcpy(m_data, m_file->GetData(), m_size);
        delete m_file;
        m_file = 0;
        m_header = (FlcHeader *)m_data;
        m_header->seqInfoSize = 0x1c;
        m_header->framesPerSeq = m_header->frames;
        m_header->sequences = 1;
        m_header->m_5c = 0;
        m_header->originX = 0;
        m_header->originY = 0;
        m_header->frameWidth = m_header->width;
        m_header->frameHeight = m_header->height;
        m_header->creator = 0xf1f1f2f2;
    }
    return IsLoaded() ? 0 : 7;
}

int FlicAnim::setup_anim_positions(unsigned short sequences, unsigned short framesPerSeq)
{
    if (!IsLoaded())
        return 7;
    if (m_seqStarts != 0)
        delete[] m_seqStarts;
    int nSeq = sequences;
    m_seqStarts = new char *[nSeq];
    int nFrames = framesPerSeq;
    int total = nFrames * nSeq;
    m_frameChunks = new char *[total];
    memset(m_frameChunks, 0, total);
    if (m_seqStarts == 0)
        return 4;
    char *p = m_data + m_header->oframe1;
    int k = 0;
    for (int j = 0; j < nSeq; j++) {
        m_seqStarts[j] = p;
        for (int i = 0; i < nFrames + 1; i++) {
            if (i == nFrames && j == nSeq - 1)
                break;
            do {
                p += ((FlicAnimData *)p)->size;
            } while (((FlicAnimData *)p)->type != 0xf1fa);
            if (i != nFrames) {
                m_frameChunks[k] = p;
                k++;
            }
        }
    }
    return 0;
}

int FlicAnim::load(const char *path, int flags, FLCPalette *palette, int palMode)
{
    if (path != 0) {
        close();
        if (m_file) {
            delete m_file;
            m_file = 0;
        }
        m_file = new Filemap;
        if (!m_file->open_read(path, 1)) {
            close();
            return 6;
        }
        FLCPalette *pal;
        if (palMode & 1) {
            if (palette) {
                close();
                return 3;
            }
            if (m_ownPalette == 0) {
                m_ownPalette = new FLCPalette;
                if (m_ownPalette == 0) {
                    close();
                    return 4;
                }
            }
            pal = m_ownPalette;
            pal->init();
        } else if (palMode & 2) {
            if (m_ownPalette) {
                delete m_ownPalette;
                m_ownPalette = 0;
            }
            PaletteList::s_ourPalettes.m_created = 0;
            PaletteList::s_ourPalettes.add(0);
            pal = PaletteList::s_ourPalettes.m_created;
            pal->init();
            s_poolPalette = pal;
        } else {
            pal = (palMode & 3) ? s_poolPalette : palette;
        }
        m_palette = pal;
        int rc = confirm_data(flags);
        if (rc) {
            close();
            return rc;
        }
    }
    if (!IsLoaded()) {
        close();
        return 7;
    }
    int rc = setup_anim_positions(m_header->sequences, m_header->framesPerSeq);
    if (rc) {
        close();
        return rc;
    }
    init(this, flags & 2);
    return 0;
}

SpriteObject *FlicAnim::next_sprite(FlicSprite *player, SpriteObject *dst)
{
    player->m_frame++;
    decode_chunk((FlicAnimData *)player->m_chunk, player, dst);
    if (player->m_frame == m_header->framesPerSeq) {
        player->m_chunk = m_seqStarts[player->m_seq];
        player->m_chunk = player->m_chunk + ((FlicAnimData *)player->m_chunk)->size;
        player->m_frame = 0;
        return &player->m_canvas;
    }
    player->m_chunk = player->m_chunk + ((FlicAnimData *)player->m_chunk)->size;
    return &player->m_canvas;
}

SpriteObject *FlicAnim::seek_anim(int seq, int frame, FlicSprite *player, SpriteObject *dst)
{
    FlcHeader *h = m_header;
    if (h == 0)
        return 0;
    int index;
    if (frame == -1) {
        index = seq;
        frame = 0;
    } else {
        unsigned int mask = h->seqMask;
        if (mask != 0) {
            if ((mask & (1 << seq)) == 0)
                return 0;
            int n = 0;
            for (int i = 0; i < seq; i++)
                if (mask & (1 << i))
                    n++;
            index = n;
        } else {
            index = seq % h->sequences;
        }
    }
    int want = frame % h->framesPerSeq;
    if (player->m_seq != index) {
        player->m_seq = index;
        player->m_frame = 0;
        player->m_chunk = m_seqStarts[index];
        decode_chunk((FlicAnimData *)player->m_chunk, player, dst);
        player->m_chunk = player->m_chunk + ((FlicAnimData *)player->m_chunk)->size;
    }
    while (player->m_frame != want)
        next_sprite(player, 0);
    return &player->m_canvas;
}

int FlicAnim::decode_chunk(FlicAnimData *chunk, FlicSprite *player, SpriteObject *dst)
{
    int i;
    char *p;
    switch (chunk->type) {
    case 4:
    case 11: return decode_color256(chunk);
    case 7: return decode_ss2(chunk, player, dst ? dst : &player->m_canvas);
    case 12: return decode_lc(chunk, player, dst ? dst : &player->m_canvas);
    case 13: return decode_black(chunk, player, dst ? dst : &player->m_canvas);
    case 15: return decode_brun(chunk, player, dst ? dst : &player->m_canvas);
    case 16: return decode_copy(chunk, player, dst ? dst : &player->m_canvas);
    case 0xf1fa:
        p = (char *)(chunk + 1);
        for (i = 0; i < chunk->count; ++i) {
            if (player->m_keyFrames && !player->m_frame && !dst)
                player->m_canvas.copy(player->m_keyFrames + player->m_seq);
            else
                decode_chunk((FlicAnimData *)p, player, dst);
            p += ((FlicAnimData *)p)->size;
        }
        break;
    }
    return 0;
}

int FlicAnim::decode_color256(FlicAnimData *chunk)
{
    unsigned char rgb[768];
    unsigned char idx;
    FLCPalette *pal = m_palette;
    if (pal == 0)
        return 0;
    const unsigned char *p = (const unsigned char *)chunk + 6;
    if (pal->pal != 0)
        pal->pal->get_entries(rgb, 0, 256);
    unsigned short packets = *(const unsigned short *)p;
    p += 2;
    idx = 0;
    while (packets--) {
        idx += *p++;
        int count = *p++;
        if (count == 0)
            count = 256;
        while (count--) {
            rgb[idx * 3 + 0] = *p++;
            rgb[idx * 3 + 1] = *p++;
            rgb[idx * 3 + 2] = *p++;
            idx++;
        }
    }
    FLCPalette *target = m_palette;
    if (target->pal == 0)
        target->init();
    target->pal->set_entries(rgb, 0, 256);
    return 0;
}

int FlicAnim::decode_ss2(FlicAnimData *chunk, FlicSprite *, SpriteObject *dst)
{
    char *src = (char *)chunk + 6;
    unsigned char *line = dst->m_surface->get_data();
    unsigned short lines = *(unsigned short *)src;
    src += 2;
    while (lines--) {
        unsigned short op;
        while (1) {
            op = *(unsigned short *)src;
            src += 2;
            if ((op & 0xc000) != 0xc000)
                break;
            line += (unsigned short)abs((short)op) * m_header->width;
        }
        if ((op & 0x8000) == 0x8000) {
            line[m_header->width - 1] = op;
            op = *(unsigned short *)src;
            src += 2;
        }
        unsigned char *q = line;
        while (op--) {
            unsigned char skip = *src++;
            char n = *src++;
            q += skip;
            if (n < 0) {
                unsigned short w = *(unsigned short *)src;
                src += 2;
                n = -n;
                for (int m = 0; m < n; m++) {
                    *(unsigned short *)q = w;
                    q += 2;
                }
            } else if (n != 0) {
                memcpy(q, src, n * 2);
                q += n * 2;
                src += n * 2;
            }
        }
        line += m_header->width;
    }
    dst->m_surface->release_data(1);
    return 0;
}

int FlicAnim::decode_lc(FlicAnimData *, FlicSprite *, SpriteObject *)
{
    return 0;
}

int FlicAnim::decode_black(FlicAnimData *, FlicSprite *, SpriteObject *dst)
{
    unsigned char *pixels = dst->m_surface->get_data();
    memset(pixels, 0, dst->m_surface->get_height() * dst->m_surface->get_width());
    dst->m_surface->release_data(1);
    return 0;
}

int FlicAnim::decode_brun(FlicAnimData *chunk, FlicSprite *, SpriteObject *dst)
{
    char *src = (char *)chunk + 6;
    char *out = (char *)dst->m_surface->get_data();
    int y = m_header->height;
    while (y--) {
        src++;
        int remaining = m_header->width;
        if (remaining != 0) {
            do {
                char n = *src++;
                if (n < 0) {
                    n = -n;
                    memcpy(out, src, n);
                    out += n;
                    src += n;
                } else {
                    memset(out, *src++, n);
                    out += n;
                }
                remaining -= n;
            } while (remaining != 0);
        }
    }
    dst->m_surface->release_data(1);
    return 0;
}

int FlicAnim::decode_copy(FlicAnimData *chunk, FlicSprite *, SpriteObject *dst)
{
    unsigned char *pixels = dst->m_surface->get_data();
    memcpy(pixels, (char *)chunk + 6, dst->m_surface->get_height() * dst->m_surface->get_width());
    dst->m_surface->release_data(1);
    return 0;
}





int g_transparentColor = 255;





unsigned int g_lfsrTaps[31] = {0x00000003, 0x00000006, 0x0000000c, 0x00000014, 0x00000030, 0x00000060, 0x000000b8,
    0x00000110, 0x00000240, 0x00000500, 0x00000ca0, 0x00001b00, 0x00003500, 0x00006000, 0x0000b400, 0x00012000,
    0x00020400, 0x00072000, 0x00090000, 0x00140000, 0x00300000, 0x00400000, 0x00da0000, 0x01200000, 0x03880000,
    0x07200000, 0x09000000, 0x14000000, 0x32800000, 0x48000000, 0xa3000000};

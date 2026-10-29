#ifndef GOLF_FLICANIM_H
#define GOLF_FLICANIM_H

#include "Sprite.h"
#include "FLCPalette.h"
#include "filemap.h"

struct FlicAnim;

#pragma pack(push, 2)
struct FlcHeader {
    unsigned long size;
    unsigned short magic;
    unsigned short frames;
    unsigned short width;
    unsigned short height;
    unsigned short depth;
    unsigned short flags;
    unsigned long speed;
    unsigned short reserved1;
    unsigned long created;
    unsigned long creator;
    unsigned long updated;
    unsigned long updater;
    unsigned short aspectX;
    unsigned short aspectY;
    unsigned short extFlags;
    unsigned short keyFrames;
    unsigned short totalFrames;
    unsigned long reqMemory;
    unsigned short maxRegions;
    unsigned short transpNum;
    unsigned char reserved2[24];
    unsigned long oframe1;
    unsigned long oframe2;
    unsigned long seqInfoSize;
    unsigned long m_5c;
    unsigned short sequences;
    unsigned short framesPerSeq;
    unsigned short originX;
    unsigned short originY;
    unsigned short frameWidth;
    unsigned short frameHeight;
    unsigned long m_6c;
    unsigned long seqMask;
    unsigned char reserved3[12];
};
#pragma pack(pop)

struct FlicAnimData {
    unsigned long size;
    unsigned short type;
    unsigned short count;
    unsigned char reserved[8];
};

struct FlcAttachment {
    virtual ~FlcAttachment() = 0;
    virtual void Slot1() = 0;
    virtual void Slot2() = 0;
    virtual void Release(int flag) = 0;
};

struct FlicSprite {
    FlicSprite();
    virtual ~FlicSprite() { close(); }

    int init(FlicAnim *bank, int keyFrames);
    void close();
    SpriteObject *current_sprite();
    SpriteObject *NextFrame();
    SpriteObject *seek_anim(int seq, int frame, SpriteObject *dst);

    FlicAnim *m_bank;
    SpriteObject m_canvas;
    SpriteObject *m_keyFrames;
    char *m_chunk;
    int m_frame;
    int m_seq;
};

struct FlicAnim : FlicSprite {
    FlicAnim();
    virtual ~FlicAnim() { close(); }

    void close();
    unsigned FrameCount() { return m_data ? m_header->framesPerSeq : 0; }
    unsigned SequenceCount() { return m_data ? m_header->sequences : 0; }
    int confirm_data(int flags);
    int setup_anim_positions(unsigned short sequences, unsigned short framesPerSeq);
    int load(const char *path, int flags, FLCPalette *palette, int palMode);
    int IsLoaded() { return m_data != 0; }
    SpriteObject *next_sprite(FlicSprite *player, SpriteObject *dst);
    SpriteObject *seek_anim(int seq, int frame, FlicSprite *player, SpriteObject *dst);

    int decode_chunk(FlicAnimData *chunk, FlicSprite *player, SpriteObject *dst);
    int decode_color256(FlicAnimData *chunk);
    int decode_ss2(FlicAnimData *chunk, FlicSprite *player, SpriteObject *dst);
    int decode_lc(FlicAnimData *chunk, FlicSprite *player, SpriteObject *dst);
    int decode_black(FlicAnimData *chunk, FlicSprite *player, SpriteObject *dst);
    int decode_brun(FlicAnimData *chunk, FlicSprite *player, SpriteObject *dst);
    int decode_copy(FlicAnimData *chunk, FlicSprite *player, SpriteObject *dst);

    char *m_data;
    char **m_seqStarts;
    char **m_frameChunks;
    int m_50;
    Filemap *m_file;
    FlcHeader *m_header;
    FlcAttachment *m_5c;
    FlcAttachment *m_60;
    FLCPalette *m_palette64;
    FLCPalette *m_palette68;
    FlcAttachment *m_6c;
    FlcAttachment *m_70;
    FLCPalette *m_palette;
    FLCPalette *m_ownPalette;
    unsigned long m_size;
    int m_80;
};

#endif

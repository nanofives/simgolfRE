// FLAGS jgld.dll: /Od /ZI /GZ /GX
// jgld.dll sprite destructors (debug build, C++ EH on). Sprite (jgld_sprite.cpp) derives from SpriteBase, which
// derives from Tracked (jgld_list.cpp). Names are chosen here.
#include <windows.h>
class Tracked {
public:
    Tracked();
    virtual ~Tracked();          // 0x10006b80
};
class SpriteBase : public Tracked {
public:
    virtual ~SpriteBase();
};
class Sprite : public SpriteBase {
public:
    virtual ~Sprite();
    void release();              // 0x10014eb0
    int m_4, m_8;
    int* m_owner;                // +0xc
    char m_10[0x38 - 0x10];
    CRITICAL_SECTION m_cs;       // +0x38
};
// MATCH: jgld.dll 0x100168e0 ??1SpriteBase@@UAE@XZ
SpriteBase::~SpriteBase()
{
}
// MATCH: jgld.dll 0x10014de0 ??1Sprite@@UAE@XZ
Sprite::~Sprite()
{
    if (m_owner)
        *m_owner = 0;
    DeleteCriticalSection(&m_cs);
    release();
}

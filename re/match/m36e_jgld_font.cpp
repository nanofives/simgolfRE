// FLAGS jgld.dll: /Od /ZI /GZ /GX
// jgld.dll Font's this-adjustor thunk for its Tracked subobject (+8), round 36 batch m36e. Class shapes as in
// jgld_font.cpp / jgld_list.cpp. VC6 emits the thunk `sub ecx, 8; jmp ??_EFont` in the object that defines the
// vtable (here, the one defining Font::~Font): the Tracked-in-Font vtable 0x1011da68 slot 0 holds it.
#include <windows.h>
class Tracked {
public:
    Tracked();
    virtual ~Tracked();
};
class FontBase {
public:
    FontBase();
    virtual ~FontBase();
    virtual void f1();
    virtual int create(const char* face, int size, unsigned style);
    virtual void reset();
    void* m_4;
};
class Font : public FontBase, public Tracked {
public:
    Font(void* owner);
    virtual ~Font();
    void release();
    virtual int create(const char* face, int size, unsigned style);
    int m_c, m_10, m_14;
    HGDIOBJ m_18;
    int m_1c;
    char* m_fot;
};
Font::~Font()
{
    if (m_4)
        *(int*)m_4 = 0;
    release();
}
// MATCH: jgld.dll 0x10069260 ??_EFont@@W7AEPAXI@Z

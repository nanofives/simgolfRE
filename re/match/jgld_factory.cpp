// FLAGS jgld.dll: /Od /ZI /GZ /GX
// jgld.dll object factory of the display (debug build, C++ EH on): `new` with a constructor (the EH frame frees
// the memory if the constructor throws) and virtual deletes. The created classes are declared here only with
// their size and constructor (Font 0x24, Palette 0x820, Surface 0x4d4, Sprite 0x50: jgld_font.cpp, jgld_app.cpp,
// jgld_surface.cpp, jgld_sprite.cpp). Names are chosen here.
struct Font { Font(void* owner); virtual ~Font(); char pad[0x24 - 4]; };          // ctor 0x10068660
struct Palette { Palette(void* owner); virtual ~Palette(); char pad[0x820 - 4]; }; // ctor 0x1006a150
struct Surface { Surface(void* owner); virtual ~Surface(); char pad[0x4d4 - 4]; }; // ctor 0x10007970
struct Sprite { Sprite(void* owner); virtual ~Sprite(); char pad[0x50 - 4]; };    // ctor 0x10014cd0
class Factory {
public:
    Font* createFont(void* owner);
    Palette* createPalette(void* owner);
    Surface* createSurface(void* owner, int unused);
    Sprite* createSprite(void* owner, int unused);
    void destroyFont(Font* p);
    void destroyPalette(Palette* p);
    void destroySurface(Surface* p);
    void destroySprite(Sprite* p);
};
// MATCH: jgld.dll 0x10067850 ?createFont@Factory@@QAEPAUFont@@PAX@Z
Font* Factory::createFont(void* owner) { return new Font(owner); }
// MATCH: jgld.dll 0x10067910 ?createPalette@Factory@@QAEPAUPalette@@PAX@Z
Palette* Factory::createPalette(void* owner) { return new Palette(owner); }
// MATCH: jgld.dll 0x100679d0 ?createSurface@Factory@@QAEPAUSurface@@PAXH@Z
Surface* Factory::createSurface(void* owner, int unused) { return new Surface(owner); }
// MATCH: jgld.dll 0x10067a90 ?createSprite@Factory@@QAEPAUSprite@@PAXH@Z
Sprite* Factory::createSprite(void* owner, int unused) { return new Sprite(owner); }
// MATCH: jgld.dll 0x10067b50 ?destroyFont@Factory@@QAEXPAUFont@@@Z
void Factory::destroyFont(Font* p) { delete p; }
// MATCH: jgld.dll 0x10067bd0 ?destroyPalette@Factory@@QAEXPAUPalette@@@Z
void Factory::destroyPalette(Palette* p) { delete p; }
// MATCH: jgld.dll 0x10067c50 ?destroySurface@Factory@@QAEXPAUSurface@@@Z
void Factory::destroySurface(Surface* p) { delete p; }
// MATCH: jgld.dll 0x10067cd0 ?destroySprite@Factory@@QAEXPAUSprite@@@Z
void Factory::destroySprite(Sprite* p) { delete p; }
struct CommandHook { void set(int (*fn)(int, int)); };   // 0x10067f60: stores the WM_COMMAND callback (0x10128728)
class Lib {
public:
    void setCommandHandler(int (*fn)(int, int));
    int m_0;
    CommandHook m_4;
};
// MATCH: jgld.dll 0x10067f10 ?setCommandHandler@Lib@@QAEXP6AHHH@Z@Z
void Lib::setCommandHandler(int (*fn)(int, int)) { m_4.set(fn); }

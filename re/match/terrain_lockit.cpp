// Two unreferenced functions right after the std::list<Tile*> block (no call, no thunk call, no address
// reference in Terrain.dll: emitted by the debug build, never used). Without _MT, VC6's yvals.h defines
// std::_Lockit's constructor and destructor as empty inlines, which compile to exactly these two bodies (a
// thiscall returning `this` and an empty thiscall). The code does NOT determine the names: list::const_iterator()
// compiles identically to the first, and any empty no-argument member function to the second.
// FLAGS Terrain.dll: /Od /ZI /GZ /GX
#include <list>
class Tile;
void useLockit()
{
    std::_Lockit lk;
    std::list<Tile*>::const_iterator cit;
}

// MATCH: Terrain.dll 0x1000bb70 ??0_Lockit@std@@QAE@XZ   // 2 names compile identically: name not determined
// MATCH: Terrain.dll 0x1000bba0 ??1_Lockit@std@@QAE@XZ   // any empty member compiles identically: name not determined

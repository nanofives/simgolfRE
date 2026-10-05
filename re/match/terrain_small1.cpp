// Small Terrain.dll functions, batch 1 (debug build). Names describe behaviour; Terrain:: names with a decorated
// export are the real ones.
// FLAGS Terrain.dll: /Od /ZI /GZ /GX /MTd
#include <math.h>

class Tile;
class Terrain {
public:
    Tile* tileAt(int x, int y);                  // 0x10001d50 (terrain.cpp)
    void drawCircle(Tile* t, float r);
    void setSplineHeight(float h);
    bool hasConnectedPath(int x, int y);
    void updatePath(int x, int y, int on);
};

class Tile {
public:
    int  getType();
    bool getWall(int side);
    int  getCorner(int i);
    bool isConnected();
    void setConnected(int on);
    void setNeighbour(int i, Tile* t);
    char isSolidType(int type);
    char  m_pad0[0x24];
    int   m_type;                                // +0x24
    char  m_pad28[0x34 - 0x28];
    Tile* m_neighbour[4];                        // +0x34
    char  m_pad44[0x209 - 0x44];
    bool  m_connected;                           // +0x209
    char  m_pad20a[0x234 - 0x20a];
    bool  m_wall[9];                             // +0x234
};

// MATCH: Terrain.dll 0x100042a0 ?drawCircle@Terrain@@QAEXPAVTile@@M@Z
void Terrain::drawCircle(Tile* t, float r)
{
}

float g_splineHeight;                            // 0x10063e54
// MATCH: Terrain.dll 0x1000a840 ?setSplineHeight@Terrain@@QAEXM@Z
void Terrain::setSplineHeight(float h)
{
    g_splineHeight = h;
}

// MATCH: Terrain.dll 0x10001f60 ?getType@Tile@@QAEHXZ
int Tile::getType()
{
    return m_type;
}

// MATCH: Terrain.dll 0x10001ed0 ?getWall@Tile@@QAE_NH@Z
bool Tile::getWall(int side)
{
    return m_wall[side];
}

// MATCH: Terrain.dll 0x10001e40 ?getCorner@Tile@@QAEHH@Z
int Tile::getCorner(int i)
{
    return ((int*)this)[i];
}

// MATCH: Terrain.dll 0x10013360 ?isConnected@Tile@@QAE_NXZ
bool Tile::isConnected()
{
    return m_connected;
}

// MATCH: Terrain.dll 0x100133a0 ?setConnected@Tile@@QAEXH@Z
void Tile::setConnected(int on)
{
    if (on)
        m_connected = true;
    else
        m_connected = false;
}

// MATCH: Terrain.dll 0x1000c520 ?setNeighbour@Tile@@QAEXHPAV1@@Z
void Tile::setNeighbour(int i, Tile* t)
{
    m_neighbour[i] = t;
}

// MATCH: Terrain.dll 0x1000a450 ?hasConnectedPath@Terrain@@QAE_NHH@Z
bool Terrain::hasConnectedPath(int x, int y)
{
    return tileAt(x, y)->isConnected();
}

// MATCH: Terrain.dll 0x1000a4b0 ?updatePath@Terrain@@QAEXHHH@Z
void Terrain::updatePath(int x, int y, int on)
{
    tileAt(x, y)->setConnected(on);
}

// MATCH: Terrain.dll 0x10001880 ?degToRad@@YAMM@Z
float degToRad(float d)
{
    return d * 0.017453292f;
}

// MATCH: Terrain.dll 0x100154a0 ?isSolidType@Tile@@QAEDH@Z
char Tile::isSolidType(int type)
{
    int r;
    if (type != 9 && type != 0)
        r = 1;
    else
        r = 0;
    return r;
}

unsigned int g_textures[0x25][0x19][9];          // 0x100687f8 (terrain_ctor.cpp): texture id per type, variation, piece
// MATCH: Terrain.dll 0x10012e70 ?setTextureId@@YAXHHHI@Z
void setTextureId(int type, int variation, int piece, unsigned int tex)
{
    g_textures[type][variation][piece] = tex;
}

// MATCH: Terrain.dll 0x10002010 ?roundHalf@@YAHM@Z
int roundHalf(float x)
{
    if (x == 0.5f)
        return 2;
    return (int)x;
}

// pow as a float (used by bernstein2, 0x10005750; the CRT callee is masked, so the code alone does not tell pow
// from atan2: the caller's use decides).
// MATCH: Terrain.dll 0x10005840 ?powf2@@YAMMM@Z
float powf2(float b, float e)
{
    float r;
    return r = (float)pow(b, e);
}

// Terrain::buildArrays (grid setup + neighbour links) and the static normalize helper (Terrain.dll, debug).
// FLAGS Terrain.dll: /Od /ZI /GZ /GX
#include <math.h>

class Geometry {                         // object returned by 0x100153c0 (name unknown)
public:
    void f_10002060(int x, int y, int width);  // 0x10002060
};

class Tile {
public:
    static void f_1000c560(int width, int height);  // 0x1000c560 (no `this`: called before any tile)
    void      reset(int x, int y);       // 0x1000c2c0 (terrain_tile4.cpp)
    Geometry* f_100153c0();              // 0x100153c0
    void      setNeighbour(int side, Tile* t);  // 0x1000c520 (sides 2,0,3,1 = x-1, y-1, x+1, y+1)
    char  m_pad[0x248];
};

class Terrain {
public:
    bool  buildArrays();                 // 0x1000a130 (returns true: mov al,1)
    Tile* tileAt(int x, int y);

    char  m_pad0[0x14];
    int   m_width;                       // +0x14
    int   m_height;                      // +0x18
    char  m_pad1[0x3a4 - 0x1c];
    Tile  m_tiles[1];                    // +0x3a4, stride 0x248, index x + y*m_width
};

void normalize(float* v);                // 0x10037c80

// MATCH: Terrain.dll 0x1000a130 ?buildArrays@Terrain@@QAE_NXZ
bool Terrain::buildArrays()
{
    int x, y, y2;
    Tile::f_1000c560(m_width, m_height);
    for (x = 0; x < m_width; x++)
        for (y = 0; y < m_height; y++) {
            m_tiles[x + y * m_width].reset(x, y);
            m_tiles[x + y * m_width].f_100153c0()->f_10002060(x, y, m_width);
        }
    for (x = 0; x < m_width; x++)
        for (y2 = 0; y2 < m_height; y2++) {
            m_tiles[x + y2 * m_width].setNeighbour(2, tileAt(x - 1, y2));
            m_tiles[x + y2 * m_width].setNeighbour(0, tileAt(x, y2 - 1));
            m_tiles[x + y2 * m_width].setNeighbour(3, tileAt(x + 1, y2));
            m_tiles[x + y2 * m_width].setNeighbour(1, tileAt(x, y2 + 1));
        }
    return true;
}

// MATCH: Terrain.dll 0x10037c80 ?normalize@@YAXPAM@Z
void normalize(float* v)
{
    float len = (float)sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    v[0] = v[0] / len;
    v[1] = v[1] / len;
    v[2] = v[2] / len;
}

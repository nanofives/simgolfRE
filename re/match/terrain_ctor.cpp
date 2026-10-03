// Terrain::Terrain(int width, int height) (Terrain.dll, debug build, C++ EH: vector ctor of 2500 tiles).
// FLAGS Terrain.dll: /Od /ZI /GZ /GX
//
// Layout facts this establishes: tiles Tile[0x9c4] at +0x3a4 (stride 0x248, 50x50), std::list<Tile*> at
// +0x164ac4, and the type table at +0x2c: 0x25 entries of {char name[0x14]; int count;} ending exactly at
// +0x3a4. Type names by index (strcpy at 0x10018580): 0 Tee, 1 PuttingGreen, 2 Fairway, 3 FirmFairway,
// 4 Rough (Tile::reset's default type), 5 DeepRough, 8 GrassySand, 9 PotSandBunker, 10 Overgrowth, 11 Brush,
// 12 Rock, 13 Woods, 17 WaterShallow, 18 Marsh, 19 Overgrowth, 22 Building, 23 WaterMiddle, 24 WaterDeep,
// 25 WaterShallowDesert, 26 TrickyGreen, 27..30 SandBunker1..4. Others are not named here.
#pragma warning(disable: 4786)
#include <string.h>
#include <list>

class Tile {
public:
    Tile();                              // thunk 0x1000114a
    ~Tile();                             // thunk 0x1000100a
    void reset(int x, int y);            // 0x1000c2c0
    char m_pad[0x248];
};

struct TypeInfo {                        // stride 0x18
    char name[0x14];
    int  count;                          // the field Terrain::setType uses as `variations`
};

class Terrain {
public:
    Terrain(int width, int height);
    void relight();                      // 0x100076e0

    int      m_0;                        // +0x00
    float    m_4, m_8, m_c, m_10;        // +0x04..+0x10: 0.15, 0.15, 0.2, 1.0
    int      m_width;                    // +0x14
    int      m_height;                   // +0x18
    int      m_1c;                       // +0x1c
    char     m_pad20[0x28 - 0x20];
    bool     m_28;                       // +0x28
    bool     m_texturesLoaded;           // +0x29
    char     m_pad2a[2];
    TypeInfo m_types[0x25];              // +0x2c
    Tile     m_tiles[0x9c4];             // +0x3a4
    std::list<Tile*> m_drawList;         // +0x164ac4
};

extern int*   g_collarInfo;              // 0x10106b48
extern int    g_courseType;              // 0x10070a0c
extern unsigned int g_textures[0x25][0x19][9];  // 0x100687f8

// MATCH: Terrain.dll 0x10002ae0 ??0Terrain@@QAE@HH@Z
Terrain::Terrain(int width, int height)
{
    int x, y, i, j, k;
    g_collarInfo = 0;
    m_width = width;
    m_height = height;
    m_1c = 0;
    m_4 = 0.15f;
    m_8 = 0.15f;
    m_c = 0.2f;
    m_10 = 1.0f;
    m_0 = 0;
    m_texturesLoaded = false;
    for (x = 0; x < m_width; x++)
        for (y = 0; y < m_height; y++)
            m_tiles[x + y * m_width].reset(x, y);
    m_28 = false;
    g_courseType = 0;
    for (i = 0; i < 0x25; i++)
        m_types[i].count = 0;
    for (i = 0; i < 0x25; i++)
        for (j = 0; j < 0x19; j++)
            for (k = 0; k < 9; k++)
                g_textures[i][j][k] = 0;
    strcpy(m_types[0].name, "Tee");
    strcpy(m_types[1].name, "PuttingGreen");
    strcpy(m_types[2].name, "Fairway");
    strcpy(m_types[3].name, "FirmFairway");
    strcpy(m_types[4].name, "Rough");
    strcpy(m_types[5].name, "DeepRough");
    strcpy(m_types[8].name, "GrassySand");
    strcpy(m_types[9].name, "PotSandBunker");
    strcpy(m_types[10].name, "Overgrowth");
    strcpy(m_types[11].name, "Brush");
    strcpy(m_types[12].name, "Rock");
    strcpy(m_types[13].name, "Woods");
    strcpy(m_types[17].name, "WaterShallow");
    strcpy(m_types[18].name, "Marsh");
    strcpy(m_types[19].name, "Overgrowth");
    strcpy(m_types[22].name, "Building");
    strcpy(m_types[24].name, "WaterDeep");
    strcpy(m_types[23].name, "WaterMiddle");
    strcpy(m_types[25].name, "WaterShallowDesert");
    strcpy(m_types[26].name, "TrickyGreen");
    strcpy(m_types[27].name, "SandBunker1");
    strcpy(m_types[28].name, "SandBunker2");
    strcpy(m_types[29].name, "SandBunker3");
    strcpy(m_types[30].name, "SandBunker4");
    relight();
}

// Unexported Tile methods of Terrain.dll (debug build), reached through the exported Terrain wrappers.
// FLAGS Terrain.dll: /Od /ZI /GZ /GX
//
// Names are TENTATIVE (taken from the Terrain wrapper that calls each one). One naming conflict is kept
// visible rather than resolved: the method called by Terrain::getVariation (0x10015340) reads +0x28, the
// field the method called as "setRotation" (0x10015380) writes; "setVariation" (0x10002f80) writes +0x240.
// Tile size is 0x248 (stride in Terrain::resetTerrain, terrain_methods2.cpp).

class Tile {
public:
    int  getX();                         // 0x10005960
    int  getY();                         // 0x10006810
    bool isHidden();                     // 0x10015460
    char getVariation();                 // 0x10015340
    void setRotation(int r);             // 0x10015380
    void setVariation(int v);            // 0x10002f80
    bool hasPath();                      // 0x10013320
    void setWall(int side, int height, bool on);  // 0x10015400

    char  m_pad0[0x24];
    int   m_type;                        // +0x24: 0x14 = hidden (isHidden)
    int   m_28;                          // +0x28: read as char by getVariation, written whole by setRotation
    int   m_x;                           // +0x2c
    int   m_y;                           // +0x30
    char  m_pad34[0x208 - 0x34];
    bool  m_hasPath;                     // +0x208
    char  m_pad209[0x210 - 0x209];
    int   m_wallHeight[9];               // +0x210
    bool  m_wall[9];                     // +0x234
    char  m_pad23d[0x240 - 0x23d];
    int   m_variation;                   // +0x240
    char  m_pad244[0x248 - 0x244];
};

// MATCH: Terrain.dll 0x10005960 ?getX@Tile@@QAEHXZ
int Tile::getX()
{
    return m_x;
}

// MATCH: Terrain.dll 0x10006810 ?getY@Tile@@QAEHXZ
int Tile::getY()
{
    return m_y;
}

// MATCH: Terrain.dll 0x10015460 ?isHidden@Tile@@QAE_NXZ
bool Tile::isHidden()
{
    return m_type == 0x14;
}

// MATCH: Terrain.dll 0x10015340 ?getVariation@Tile@@QAEDXZ
char Tile::getVariation()
{
    return m_28;
}

// MATCH: Terrain.dll 0x10015380 ?setRotation@Tile@@QAEXH@Z
void Tile::setRotation(int r)
{
    m_28 = r;
}

// MATCH: Terrain.dll 0x10002f80 ?setVariation@Tile@@QAEXH@Z
void Tile::setVariation(int v)
{
    m_variation = v;
}

// MATCH: Terrain.dll 0x10013320 ?hasPath@Tile@@QAE_NXZ
bool Tile::hasPath()
{
    return m_hasPath;
}

// MATCH: Terrain.dll 0x10015400 ?setWall@Tile@@QAEXHH_N@Z
void Tile::setWall(int side, int height, bool on)
{
    m_wall[side] = on;
    m_wallHeight[side] = height;
}

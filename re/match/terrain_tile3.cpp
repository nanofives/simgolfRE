// Small unexported Tile / Terrain helpers of Terrain.dll (debug build).
// FLAGS Terrain.dll: /Od /ZI /GZ /GX
//
// Names are TENTATIVE (from the callers in the other re/match files) unless the body says more:
// 0x10003980 ("rebuild" in terrain_methods2.cpp) loads the lighting file for the course type at 0x10070a0c.

class Tile {
public:
    int  maxHeight();                    // 0x10015500: max of the four ints at +4, +0xc, +0x14, +0x1c
    void layPath(bool on, int dir);      // 0x10013400
    void f_1000d480(int type);           // 0x1000d480 (called by setTypeId)
    void f_10013500();                   // 0x10013500

    char  m_pad0[4];
    int   m_4;  int m_pad8;              // +0x04
    int   m_c;  int m_pad10;             // +0x0c
    int   m_14; int m_pad18;             // +0x14
    int   m_1c;                          // +0x1c
    char  m_pad20[0x24 - 0x20];
    int   m_type;                        // +0x24
    char  m_pad28[0x208 - 0x28];
    bool  m_hasPath;                     // +0x208
    bool  m_209;                         // +0x209
    bool  m_20a;                         // +0x20a
};

class Terrain {
public:
    void setViewAngle(float angle);      // 0x1000adc0
    void rebuild();                      // 0x10003980
    bool initLists();                    // 0x100037e0 (returns true: mov al,1)
    void loadLighting(const char* file); // 0x10006dd0 (name from its argument)
};

extern int g_viewAngleIndex;             // 0x10070a14: 0/1/2/3 for 0/90/180/other degrees
extern int g_courseType;                 // 0x10070a0c (same global as terrain_methods2.cpp): selects the lighting file

// MATCH: Terrain.dll 0x10015500 ?maxHeight@Tile@@QAEHXZ
int Tile::maxHeight()
{
    int a = m_4 > m_1c ? m_4 : m_1c;
    int b = m_c > m_14 ? m_c : m_14;
    return a > b ? a : b;
}

// MATCH: Terrain.dll 0x10013400 ?layPath@Tile@@QAEX_NH@Z
void Tile::layPath(bool on, int dir)
{
    if (on) {
        m_hasPath = true;
        if (dir)
            m_209 = true;
        if (m_type == 0x16 || m_type == 0 || m_type == 2 || m_type == 1 || m_type == 3 || m_type == 7 || m_type == 9)
            m_20a = false;
        else
            m_20a = true;
    } else {
        m_hasPath = false;
        m_209 = false;
        m_20a = false;
    }
}

// MATCH: Terrain.dll 0x1000d480 ?f_1000d480@Tile@@QAEXH@Z
void Tile::f_1000d480(int type)
{
    if (m_hasPath) {
        if (type == 0x14 || type == 0x11 || type == 0x16 || type == 0 || type == 2 || type == 1 || type == 3
            || type == 7 || type == 9)
            m_20a = false;
        else
            m_20a = true;
    }
    f_10013500();
}

// MATCH: Terrain.dll 0x1000adc0 ?setViewAngle@Terrain@@QAEXM@Z
void Terrain::setViewAngle(float angle)
{
    if (angle == 0.0f)
        g_viewAngleIndex = 0;
    else if (angle == 90.0f)
        g_viewAngleIndex = 1;
    else if (angle == 180.0f)
        g_viewAngleIndex = 2;
    else
        g_viewAngleIndex = 3;
}

// MATCH: Terrain.dll 0x10003980 ?rebuild@Terrain@@QAEXXZ
void Terrain::rebuild()
{
    switch (g_courseType) {
    case 0:
        loadLighting("ParklandLighting.txt");
        break;
    case 2:
        loadLighting("TropicalLighting.txt");
        break;
    case 1:
        loadLighting("DesertLighting.txt");
        break;
    case 3:
        loadLighting("LinksLighting.txt");
        break;
    default:
        loadLighting("lighting.txt");
    }
}

// MATCH: Terrain.dll 0x100037e0 ?initLists@Terrain@@QAE_NXZ
bool Terrain::initLists()
{
    rebuild();
    return true;
}

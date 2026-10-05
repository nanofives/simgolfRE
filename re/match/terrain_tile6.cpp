// Tile construction and destruction (Terrain.dll, debug build, C++ EH): Tile::Tile builds three members, the
// face block (+0x44, has a destructor, hence the EH frame), PathInfo (+0x208) and WallInfo (+0x210); ~Tile clears
// position and flags and detaches the four neighbours from the opposite side. Layout as in terrain_tile4.cpp.
// FLAGS Terrain.dll: /Od /ZI /GZ /GX /MTd

struct Face { char pad[0x38]; };
struct Faces {                           // Tile +0x44
    int  count;
    Face f[8];
    Faces();
    ~Faces();
};
struct PathInfo {                        // Tile +0x208
    bool b0, b1, b2, b3, b4, b5, b6;
    PathInfo();
    void clear();                        // 0x1000f6e0 (terrain_tile4.cpp)
};
struct WallInfo {                        // Tile +0x210
    int  height[9];
    bool on[9];
    WallInfo();
    void clear();                        // 0x1000f7a0 (terrain_tile4.cpp)
};

class Tile {
public:
    Tile();
    ~Tile();
    Faces* getFaces();
    void setNeighbour(int side, Tile* t);// 0x1000c520 (terrain_small1.cpp)
    int   m_0, m_4, m_8, m_c;
    char  m_pad10[0x2c - 0x10];
    int   m_x, m_y;                      // +0x2c, +0x30
    Tile* m_n[4];                        // +0x34
    Faces m_faces;                       // +0x44
    PathInfo m_path;                     // +0x208
    char  m_pad20f;
    WallInfo m_walls;                    // +0x210
    int   m_variation;                   // +0x240
    bool  m_normalsDirty;                // +0x244
};

// MATCH: Terrain.dll 0x10001fa0 ??0Faces@@QAE@XZ
Faces::Faces()
{
    count = 0;
}

// MATCH: Terrain.dll 0x10001fe0 ??1Faces@@QAE@XZ
Faces::~Faces()
{
}

// MATCH: Terrain.dll 0x1000f690 ??0PathInfo@@QAE@XZ
PathInfo::PathInfo()
{
    clear();
}

// MATCH: Terrain.dll 0x1000f750 ??0WallInfo@@QAE@XZ
WallInfo::WallInfo()
{
    clear();
}

// MATCH: Terrain.dll 0x1000c210 ??0Tile@@QAE@XZ
Tile::Tile()
{
}

// MATCH: Terrain.dll 0x1000c3d0 ??1Tile@@QAE@XZ
Tile::~Tile()
{
    m_x = -1;
    m_y = -1;
    m_0 = 0;
    m_4 = 0;
    m_8 = 0;
    m_c = 0;
    m_normalsDirty = false;
    if (m_n[0])
        m_n[0]->setNeighbour(1, 0);
    if (m_n[1])
        m_n[1]->setNeighbour(0, 0);
    if (m_n[2])
        m_n[2]->setNeighbour(3, 0);
    if (m_n[3])
        m_n[3]->setNeighbour(2, 0);
}

// MATCH: Terrain.dll 0x100153c0 ?getFaces@Tile@@QAEPAUFaces@@XZ
Faces* Tile::getFaces()
{
    return &m_faces;
}

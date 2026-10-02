// Canonical source for the Terrain accessors. ONE source, two builds:
//   golf_clean.exe has a release copy (dead code, exported), Terrain.dll the live debug copy.
// FLAGS golf_clean.exe: /O2
// FLAGS Terrain.dll: /Od /ZI /GZ /GX
class Tile {
public:
    // Inline accessors: inlined in the release exe, out-of-line calls in the debug Terrain.dll
    // (the debug Terrain::get* bodies `call` them; that call is the evidence these exist).
    int  getElevation(int i) { return m_elevation[i]; }
    bool getWall(int i)      { return m_wall[i]; }
    int  getType()           { return m_type; }

    int  m_elevation[9];       // +0x000  getElevation(tile, i) reads [i]
    int  m_type;               // +0x024  getType
    char m_pad0[0x234 - 0x28];
    bool m_wall[1];            // +0x234  getWall(tile, i) reads [i]
    char m_pad1[0x248 - 0x235];
};                             // sizeof 0x248

class Terrain {
public:
    Tile* tileAt(int x, int y);
    int   getElevation(Tile* t, int i);
    bool  getWall(Tile* t, int i);
    int   getType(Tile* t);
    char  m_pad0[0x14];
    int   m_width;             // +0x14
    int   m_height;            // +0x18
    char  m_pad1[0x3a4 - 0x1c];
    Tile  m_tiles[1];          // +0x3a4
};

// MATCH: golf_clean.exe 0x004490d0 ?tileAt@Terrain@@QAEPAVTile@@HH@Z
// MATCH: Terrain.dll 0x10001d50 ?tileAt@Terrain@@QAEPAVTile@@HH@Z
Tile* Terrain::tileAt(int x, int y)
{
    if (x >= m_width || x < 0 || y >= m_height || y < 0)
        return 0;
    return &m_tiles[x + y * m_width];
}

// MATCH: golf_clean.exe 0x00449110 ?getElevation@Terrain@@QAEHPAVTile@@H@Z
// MATCH: Terrain.dll 0x10001de0 ?getElevation@Terrain@@QAEHPAVTile@@H@Z
int Terrain::getElevation(Tile* t, int i)
{
    if (t != 0)
        return t->getElevation(i);
    return 0;
}

// MATCH: golf_clean.exe 0x00449130 ?getWall@Terrain@@QAE_NPAVTile@@H@Z
// MATCH: Terrain.dll 0x10001e80 ?getWall@Terrain@@QAE_NPAVTile@@H@Z
bool Terrain::getWall(Tile* t, int i)
{
    return t->getWall(i);
}

// MATCH: golf_clean.exe 0x00449150 ?getType@Terrain@@QAEHPAVTile@@@Z
// MATCH: Terrain.dll 0x10001f10 ?getType@Terrain@@QAEHPAVTile@@@Z
int Terrain::getType(Tile* t)
{
    return t->getType();
}

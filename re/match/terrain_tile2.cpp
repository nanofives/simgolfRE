// Unexported Tile::setTypeId and the static vertex/normal array accessors (Terrain.dll, debug build).
// FLAGS Terrain.dll: /Od /ZI /GZ /GX
//
// setTypeId case blocks are laid out in source order (VC6): 7, 0x11, 6, 0xd..0x10, 0/9, 4/0x14, default.
// Jump table 0x10014115, index bytes 0x10014131. The callees' names are unknown: named by address.

class Tile {
public:
    void setTypeId(int type);            // 0x10014020 (name from the caller in terrain_methods2.cpp)
    static float* vertexArray();         // 0x10013fc0: returns 0x100b28c8 (mov eax, imm)
    static float* normalArray();         // 0x10013ff0: returns 0x10070a18

    void f_100149e0();                   // run when leaving type 7
    void f_10014190();                   // run when leaving type 6
    void f_1000d540();                   // entering type 7
    void f_1000d480(int type);           // entering 0x11, 0xd..0x10 (with 0xd) and every unlisted type
    void f_1000de60();                   // entering type 6
    void f_10015230();                   // entering type 0 or 9
    void f_10013500();                   // entering type 4 or 0x14

    char  m_pad0[0x24];
    int   m_type;                        // +0x24
    char  m_pad28[0x208 - 0x28];
    bool  m_hasPath;                     // +0x208
    char  m_pad209;
    bool  m_20a;                         // +0x20a: set when the type changes on a tile with a path
    char  m_pad20b[0x244 - 0x20b];
    bool  m_244;                         // +0x244: set when leaving type 0x11
};

class Terrain {
public:
    float* vertexArray();
    float* normalArray();
};

static float s_vertices[1];              // 0x100b28c8 (size unknown; address masked by the matcher)
static float s_normals[1];               // 0x10070a18

// MATCH: Terrain.dll 0x10014020 ?setTypeId@Tile@@QAEXH@Z
void Tile::setTypeId(int type)
{
    if (m_hasPath)
        m_20a = true;
    if (m_type == 7)
        f_100149e0();
    if (m_type == 6)
        f_10014190();
    else if (m_type == 0x11)
        m_244 = true;
    m_type = type;
    switch (type) {
    case 7:
        f_1000d540();
        break;
    case 0x11:
        f_1000d480(type);
        break;
    case 6:
        f_1000de60();
        break;
    case 0xd:
    case 0xe:
    case 0xf:
    case 0x10:
        f_1000d480(0xd);
        break;
    case 0:
    case 9:
        f_10015230();
        break;
    case 4:
    case 0x14:
        f_10013500();
        break;
    default:
        f_1000d480(type);
        break;
    }
}

// MATCH: Terrain.dll 0x10013fc0 ?vertexArray@Tile@@SAPAMXZ
float* Tile::vertexArray()
{
    return s_vertices;
}

// MATCH: Terrain.dll 0x10013ff0 ?normalArray@Tile@@SAPAMXZ
float* Tile::normalArray()
{
    return s_normals;
}

// MATCH: Terrain.dll 0x100032b0 ?vertexArray@Terrain@@QAEPAMXZ
float* Terrain::vertexArray()
{
    return Tile::vertexArray();
}

// MATCH: Terrain.dll 0x10003270 ?normalArray@Terrain@@QAEPAMXZ
float* Terrain::normalArray()
{
    return Tile::normalArray();
}

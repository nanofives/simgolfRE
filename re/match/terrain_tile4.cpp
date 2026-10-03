// Unexported Tile methods (reset, calcNormals, texture/neighbour updates) and Terrain GL/texture setup
// of Terrain.dll (debug build).
// FLAGS Terrain.dll: /Od /ZI /GZ /GX
//
// Tile layout used here (offsets from the instructions of the functions below):
//   +0x2c x, +0x30 y, +0x34/+0x38/+0x3c/+0x40 four neighbour Tile* (zeroed by reset, 0x1000c346),
//   +0x44 face count, +0x48 Face[8] (stride 0x38; 8 faces end at +0x208), +0x208 PathInfo (7 bytes),
//   +0x210 WallInfo, +0x240 variation, +0x244 normals-dirty flag.
#include <windows.h>
#include <GL/gl.h>

struct Face {                            // stride 0x38
    char  pad0[8];
    int   vertex;                        // +0x08 (Tile +0x50 for face 0: index into the normal array)
    char  pad0c[0x24 - 0x0c];
    GLuint texture;                      // +0x24 (Tile +0x6c + i*0x38)
    char  pad28[0x2c - 0x28];
    float normal[3];                     // +0x2c (Tile +0x74 + i*0x38)
};

struct PathInfo {                        // Tile +0x208
    bool b0, b1, b2, b3, b4, b5, b6;
    void clear();                        // 0x1000f6e0
};

struct WallInfo {                        // Tile +0x210
    int   height[9];                     // +0x00
    bool  on[9];                         // +0x24
    void clear();                        // 0x1000f7a0
};

class Tile {
public:
    void reset(int x, int y);            // 0x1000c2c0
    void calcNormals();                  // 0x10012cf0
    void f_10015230();                   // 0x10015230: type 0/9 entry from setTypeId
    void f_10013500();                   // 0x10013500
    void f_10013670(int type);           // 0x10013670
    void f_10011ef0(int a, int b);       // 0x10011ef0

    int   m_0;
    int   m_4;  int m_pad8;
    int   m_c;  int m_pad10;
    int   m_14; int m_pad18;
    int   m_1c;
    char  m_pad20[0x24 - 0x20];
    int   m_type;                        // +0x24
    int   m_28;                          // +0x28
    int   m_x;                           // +0x2c
    int   m_y;                           // +0x30
    Tile* m_n[4];                        // +0x34
    int   m_faceCount;                   // +0x44
    Face  m_faces[8];                    // +0x48
    PathInfo m_path;                     // +0x208
    char  m_pad20f;
    WallInfo m_walls;                    // +0x210 (sizeof 0x30: ends at +0x240)
    int   m_variation;                   // +0x240
    bool  m_normalsDirty;                // +0x244
};

struct TypeInfo {                        // Terrain +0x40, stride 0x18
    int  loaded;                         // +0x00: zeroed per type by reloadTextures (0x10038159)
    char pad[0x14];
};

class Terrain {
public:
    void reloadTextures();               // 0x100380a0
    bool initGL();                       // 0x100033e0 (returns true: mov al,1; earlier called "initTextures")

    char  m_pad0[0x28];
    bool  m_28;                          // +0x28: front face CW (0x900) when set, CCW (0x901) otherwise
    bool  m_texturesLoaded;              // +0x29
    char  m_pad2a[0x40 - 0x2a];
    TypeInfo m_types[0x25];              // +0x40 (loop bound 0x25 in reloadTextures; index 0x24 lands at +0x3a0)
};

extern GLuint g_textures[0x25][0x19][9]; // 0x100687f8 (strides 900 and 0x24 bytes)
extern float  g_normals[][3];            // 0x10070a18 (Tile::normalArray)
void normalize(float* v);                // 0x10037c80

// MATCH: Terrain.dll 0x1000c2c0 ?reset@Tile@@QAEXHH@Z
void Tile::reset(int x, int y)
{
    m_x = x;
    m_y = y;
    bool unused = false;                 // byte [ebp-8] written once (0x1000c2ef), never read
    m_variation = 0;
    m_4 = 0;
    m_1c = 0;
    m_c = 0;
    m_14 = 0;
    for (int i = 0; i < 4; i++)
        m_n[i] = 0;
    m_type = 4;
    m_28 = 0;
    m_path.clear();
    m_walls.clear();
}

// MATCH: Terrain.dll 0x1000f6e0 ?clear@PathInfo@@QAEXXZ
void PathInfo::clear()
{
    b1 = false;
    b0 = false;
    b2 = false;
    b3 = false;
    b4 = false;
    b5 = false;
    b6 = false;
}

// MATCH: Terrain.dll 0x1000f7a0 ?clear@WallInfo@@QAEXXZ
void WallInfo::clear()
{
    on[0] = false;
    on[2] = false;
    on[6] = false;
    on[4] = false;
}

// MATCH: Terrain.dll 0x10015230 ?f_10015230@Tile@@QAEXXZ
void Tile::f_10015230()
{
    if (m_path.b0)
        m_path.b2 = false;
    if (m_type == 0) {
        for (int i = 0; i < m_faceCount; i++)
            m_faces[i].texture = g_textures[0][m_28][0];
    } else {
        for (int j = 0; j < m_faceCount; j++)
            m_faces[j].texture = g_textures[m_type][m_variation][3];
    }
    f_10013500();
}

// MATCH: Terrain.dll 0x10013500 ?f_10013500@Tile@@QAEXXZ
void Tile::f_10013500()
{
    f_10013670(m_type);
    if (m_n[0] != 0)
        m_n[0]->f_10013670(-1);
    if (m_n[0] != 0 && m_n[2] != 0)
        m_n[0]->m_n[2]->f_10013670(-1);
    if (m_n[2] != 0)
        m_n[2]->f_10013670(-1);
    if (m_n[1] != 0 && m_n[2] != 0)
        m_n[1]->m_n[2]->f_10013670(-1);
    if (m_n[1] != 0)
        m_n[1]->f_10013670(-1);
    if (m_n[1] != 0 && m_n[3] != 0)
        m_n[1]->m_n[3]->f_10013670(-1);
    if (m_n[3] != 0)
        m_n[3]->f_10013670(-1);
    if (m_n[0] != 0 && m_n[3] != 0)
        m_n[0]->m_n[3]->f_10013670(-1);
}

// MATCH: Terrain.dll 0x10012cf0 ?calcNormals@Tile@@QAEXXZ
void Tile::calcNormals()
{
    if (!m_normalsDirty)
        return;
    f_10011ef0(0, 2);
    f_10011ef0(1, 2);
    f_10011ef0(0, 3);
    f_10011ef0(1, 3);
    for (int c = 0; c <= 2; c++)
        g_normals[m_faces[0].vertex][c] = m_faces[0].normal[c] + m_faces[1].normal[c] + m_faces[2].normal[c]
            + m_faces[3].normal[c] + m_faces[4].normal[c] + m_faces[5].normal[c] + m_faces[6].normal[c]
            + m_faces[7].normal[c];
    normalize(g_normals[m_faces[0].vertex]);
    m_normalsDirty = false;
}

// MATCH: Terrain.dll 0x100380a0 ?reloadTextures@Terrain@@QAEXXZ
void Terrain::reloadTextures()
{
    if (!m_texturesLoaded)
        return;
    for (int t = 0; t < 0x25; t++) {
        for (int v = 0; v < 0x19; v++) {
            glDeleteTextures(9, g_textures[t][v]);
            for (int k = 0; k < 9; k++)
                g_textures[t][v][k] = 0;
        }
        m_types[t].loaded = 0;
    }
    m_texturesLoaded = false;
}

// MATCH: Terrain.dll 0x100033e0 ?initGL@Terrain@@QAE_NXZ
bool Terrain::initGL()
{
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);   // 0x0c50, 0x1101
    glEnable(GL_LINE_SMOOTH);                             // 0x0b20
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);               // 0x0c52, 0x1102
    glEnable(GL_TEXTURE_2D);                              // 0x0de1
    glEnable(GL_LIGHTING);                                // 0x0b50
    if (m_28)
        glFrontFace(GL_CW);                               // 0x0900
    else
        glFrontFace(GL_CCW);                              // 0x0901
    glEnable(GL_CULL_FACE);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
    return true;
}

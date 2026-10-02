// Live exported Terrain methods of Terrain.dll (debug build), second batch: loops, globals, OpenGL, rand.
// FLAGS Terrain.dll: /Od /ZI /GZ /GX
//
// Names of unexported callees are TENTATIVE (call targets are masked by the matcher); field offsets and
// globals are cited from the disassembly.
#include <windows.h>
#include <GL/gl.h>
#include <stdlib.h>

class Tile {
public:
    void calcNormals();                  // 0x10012cf0
    int  getY();                         // 0x10006810 (outer loop bound in calcAllNormals)
    int  getX();                         // 0x10005960 (inner loop bound in calcAllNormals)
    void reset(int x, int y);            // 0x1000c2c0 (called on every tile by resetTerrain)
    void setVariation(int v);            // 0x10002f80
    void setRotation(int r);             // 0x10015380
    void setTypeId(int type);            // 0x10014020
};

struct TypeInfo {                        // stride 0x18 (imul eax,eax,0x18 at 0x10003310)
    int  variations;                     // +0x00, rand() % variations at 0x1000332c
    char pad[0x14];
};

class Terrain {
public:
    void  loadNewCourseType(int type);
    void  initTerrain();
    void  setType(Tile* t, int type, int rotation);
    void  passCollarInfo(int* const info, int n);
    void  calcAllNormals(Tile* t);
    void  resetTerrain();
    Tile* tileAt(int x, int y);

    void  rebuild();                     // 0x10003980 (unexported)
    void  reloadTextures();              // 0x100380a0 (unexported)
    void  relight();                     // 0x100076e0 (unexported)
    void  buildArrays();                 // 0x1000a130 (unexported; also calls tileAt)
    float* vertexArray();                // 0x100032b0
    float* normalArray();                // 0x10003270

    char  m_pad0[0x14];
    int   m_width;                       // +0x14
    int   m_height;                      // +0x18
    int   m_1c;                          // +0x1c, shift count in calcAllNormals (0x1000a765)
    char  m_pad1[0x40 - 0x20];
    TypeInfo m_types[36];                // +0x40
    char  m_pad2[0x3a4 - 0x40 - 36 * 0x18];
    Tile* m_tilesAt();                   // (no storage; tiles start at +0x3a4, see resetTerrain)
};

int  g_courseType;                       // 0x10070a0c
int* g_collarInfo;                       // 0x10106b48

// MATCH: Terrain.dll 0x10001af0 ?loadNewCourseType@Terrain@@QAEXH@Z
void Terrain::loadNewCourseType(int type)
{
    if (g_courseType != type) {
        g_courseType = type;
        rebuild();
        reloadTextures();
        relight();
    }
}

// MATCH: Terrain.dll 0x1000a970 ?initTerrain@Terrain@@QAEXXZ
void Terrain::initTerrain()
{
    buildArrays();
    glVertexPointer(3, GL_FLOAT, 0, vertexArray());
    glNormalPointer(GL_FLOAT, 0, normalArray());
}

// MATCH: Terrain.dll 0x100032f0 ?setType@Terrain@@QAEXPAVTile@@HH@Z
void Terrain::setType(Tile* t, int type, int rotation)
{
    if (m_types[type].variations > 0)
        t->setVariation(rand() % m_types[type].variations);
    else
        t->setVariation(0);
    t->setRotation(rotation);
    t->setTypeId(type);
}

// MATCH: Terrain.dll 0x1000a880 ?passCollarInfo@Terrain@@QAEXQAHH@Z
void Terrain::passCollarInfo(int* const info, int n)
{
    int i;
    if (g_collarInfo == 0) {
        g_collarInfo = new int[n];
    } else {
        delete g_collarInfo;
        g_collarInfo = new int[n];
    }
    for (i = 0; i < n; i++)
        g_collarInfo[i] = info[i];
}

// MATCH: Terrain.dll 0x1000a740 ?calcAllNormals@Terrain@@QAEXPAVTile@@@Z
void Terrain::calcAllNormals(Tile* t)
{
    Tile* n;
    int a, b, c, d;
    int y, x;
    a = b = c = d = 13 << m_1c;
    for (y = t->getY() - a; y < t->getY() + b; y++)
        for (x = t->getX() + d; x > t->getX() - c; x--) {
            n = tileAt(x, y);
            if (n)
                n->calcNormals();
        }
}

// MATCH: Terrain.dll 0x1000aa10 ?resetTerrain@Terrain@@QAEXXZ
void Terrain::resetTerrain()
{
    int x, y;
    for (x = 0; x < m_width; x++)
        for (y = 0; y < m_height; y++)
            ((Tile*)((char*)this + 0x3a4 + (x + y * m_width) * 0x248))->reset(x, y);
    buildArrays();
    glVertexPointer(3, GL_FLOAT, 0, vertexArray());
    glNormalPointer(GL_FLOAT, 0, normalArray());
}

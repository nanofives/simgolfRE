// Terrain.dll functions, batch 3 (debug build): Terrain destructor, tile vertex height/normal, face normal,
// image flip, Hermite point for the cardinal spline, light 1 setup.
// FLAGS Terrain.dll: /Od /ZI /GZ /GX /MTd
#pragma warning(disable: 4786)
#include <windows.h>
#include <GL/gl.h>
#include <math.h>
#include <list>

void normalize(float* v);                        // 0x10037c80 (terrain.cpp)

class Tile {
public:
    Tile();
    ~Tile();
    void setHeight(float h);
    void setNormal(float* n);
    int  getType();                              // 0x10001f60
    char getVariation();                         // 0x10015340
    int  edgeKind(Tile* other);
    int  m_v[0x24 / 4];                          // +0x00..: vertex indices (+0x04, +0x08, +0x0c used here)
    int  m_type;                                 // +0x24
    int  m_28;                                   // +0x28
    int  m_rest[(0x248 - 0x2c) / 4];
};

struct TypeInfo { char name[0x14]; int count; };
class Terrain {
public:
    ~Terrain();
    void reloadTextures();                       // 0x100380a0
    void light1Off();
    void faceNormal(struct Face* f);
    void hermitePoint(float* out, float t, float* p0, float* p1, float* t0, float* t1);
    int      m_0;
    float    m_4, m_8, m_c, m_10;
    int      m_width;                            // +0x14
    int      m_height;                           // +0x18
    int      m_1c;
    char     m_pad20[0x28 - 0x20];
    bool     m_28;
    bool     m_texturesLoaded;
    char     m_pad2a[2];
    TypeInfo m_types[0x25];                      // +0x2c
    Tile     m_tiles[0x9c4];                     // +0x3a4
    std::list<Tile*> m_drawList;                 // +0x164ac4
};

extern int*  g_collarInfo;                       // 0x10106b48
extern int   g_10106b4c;                         // 0x10106b4c
// MATCH: Terrain.dll 0x10003090 ??1Terrain@@QAE@XZ
Terrain::~Terrain()
{
    m_width = -1;
    m_height = -1;
    if (g_collarInfo) {
        delete g_collarInfo;
        g_collarInfo = 0;
    }
    if (g_10106b4c)
        g_10106b4c = 0;
    reloadTextures();
}

extern float g_vertices[][3];                    // 0x100b28c8
extern float g_normals[][3];                      // 0x10070a18
// Sets the height (y) of the tile's nine vertices.
// MATCH: Terrain.dll 0x10002640 ?setHeight@Tile@@QAEXM@Z
void Tile::setHeight(float h)
{
    g_vertices[m_v[1]][1] = h;
    g_vertices[m_v[0x11]][1] = h;
    g_vertices[m_v[0x1f]][1] = h;
    g_vertices[m_v[0x2c]][1] = h;
    g_vertices[m_v[0x64]][1] = h;
    g_vertices[m_v[0x56]][1] = h;
    g_vertices[m_v[0x3a]][1] = h;
    g_vertices[m_v[2]][1] = h;
    g_vertices[m_v[3]][1] = h;
}

// MATCH: Terrain.dll 0x10002750 ?setNormal@Tile@@QAEXPAM@Z
void Tile::setNormal(float* n)
{
    for (int i = 0; i < 3; i++) {
        g_normals[m_v[1]][i] = n[i];
        g_normals[m_v[0x11]][i] = n[i];
        g_normals[m_v[0x1f]][i] = n[i];
        g_normals[m_v[0x2c]][i] = n[i];
        g_normals[m_v[0x64]][i] = n[i];
        g_normals[m_v[0x56]][i] = n[i];
        g_normals[m_v[0x3a]][i] = n[i];
        g_normals[m_v[2]][i] = n[i];
        g_normals[m_v[3]][i] = n[i];
    }
}


// Kind of edge between this tile and a neighbour: 0 none, 1 or 2 (water, type 0x11, compares variations;
// other types compare the collar group of the type, g_collarInfo, then the type itself).
// MATCH: Terrain.dll 0x10015650 ?edgeKind@Tile@@QAEHPAV1@@Z
int Tile::edgeKind(Tile* other)
{
    if (!other)
        return 0;
    if (m_type == 0x11) {
        if (m_28 == 0 && m_type != other->getType())
            return 1;
        if (m_28 != other->getVariation()) {
            if (m_28 == 0 || (m_28 == 1 && other->getVariation() == 2))
                return 2;
            return 1;
        }
        return 0;
    }
    if (g_collarInfo[m_type] != g_collarInfo[other->getType()])
        return 1;
    if (other->getType() != m_type)
        return 2;
    return 0;
}

struct Face { int v[3]; char pad[0x2c - 0xc]; float normal[3]; };
// Normal of the face's first triangle: (v1 - v0) x (v2 - v1), normalized.
// The only caller, Tile::smoothNormals (0x10011ef0, terrain_smooth.cpp), passes the Tile in ecx: the body never
// reads `this`, so the class is not visible here; it is declared on Terrain only to compile.
// MATCH: Terrain.dll 0x10011d60 ?faceNormal@Terrain@@QAEXPAUFace@@@Z
void Terrain::faceNormal(Face* f)
{
    float a[3], b[3];
    a[0] = g_vertices[f->v[1]][0] - g_vertices[f->v[0]][0];
    a[1] = g_vertices[f->v[1]][1] - g_vertices[f->v[0]][1];
    a[2] = g_vertices[f->v[1]][2] - g_vertices[f->v[0]][2];
    b[0] = g_vertices[f->v[2]][0] - g_vertices[f->v[1]][0];
    b[1] = g_vertices[f->v[2]][1] - g_vertices[f->v[1]][1];
    b[2] = g_vertices[f->v[2]][2] - g_vertices[f->v[1]][2];
    f->normal[0] = a[1] * b[2] - a[2] * b[1];
    f->normal[1] = a[2] * b[0] - a[0] * b[2];
    f->normal[2] = a[0] * b[1] - a[1] * b[0];
    normalize(f->normal);
}

struct ImageInfo { int bpp; int width; int height; };
// Flips a 24-bit image vertically in place.
// MATCH: Terrain.dll 0x10001c00 ?flipVertical@@YAXPAEPAUImageInfo@@@Z
void flipVertical(unsigned char* data, ImageInfo* info)
{
    unsigned char* a;
    unsigned char* b;
    int y = 0;
    unsigned char t[3];
    int x;
    for (y = 0; y < info->height >> 1; y++) {
        for (x = 0; x < info->width; x++) {
            a = data + (x + info->width * y) * 3;
            b = a + ((info->height - 1) - y) * info->width * 3;
            t[0] = a[0];
            t[1] = a[1];
            t[2] = a[2];
            a[0] = b[0];
            a[1] = b[1];
            a[2] = b[2];
            b[0] = t[0];
            b[1] = t[1];
            b[2] = t[2];
        }
    }
}

// MATCH: Terrain.dll 0x10005090 ?hermitePoint@Terrain@@QAEXPAMM0000@Z
void Terrain::hermitePoint(float* out, float t, float* p0, float* p1, float* t0, float* t1)
{
    float t3 = (float)pow(t, 3.0);
    float t2 = (float)pow(t, 2.0);
    out[0] = 0.0f;
    out[0] = ((2.0f * t3 - 3.0f * t2) + 1.0f) * p0[0] + (-2.0f * t3 + 3.0f * t2) * p1[0]
           + ((t3 - 2.0f * t2) + t) * t0[0] + (t3 - t2) * t1[0];
    out[1] = 0.0f;
    out[1] = ((2.0f * t3 - 3.0f * t2) + 1.0f) * p0[1] + (-2.0f * t3 + 3.0f * t2) * p1[1]
           + ((t3 - 2.0f * t2) + t) * t0[1] + (t3 - t2) * t1[1];
}

// Turns light 0 off and light 1 on with black ambient, diffuse and specular.
// MATCH: Terrain.dll 0x10003830 ?light1Off@Terrain@@QAEXXZ
void Terrain::light1Off()
{
    glDisable(GL_LIGHT0);
    float ambient[] = {0.0f, 0.0f, 0.0f, 0.0f};
    float diffuse[] = {0.0f, 0.0f, 0.0f, 0.0f};
    float specular[] = {0.0f, 0.0f, 0.0f, 0.0f};
    glLightfv(GL_LIGHT1, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT1, GL_SPECULAR, specular);
    glEnable(GL_LIGHT1);
}

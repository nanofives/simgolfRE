// Tile::render and Terrain::drawTileObjects (Terrain.dll, debug build).
// FLAGS Terrain.dll: /Od /ZI /GZ /GX
//
// Tile::render binds each face's texture (cached in an uninitialised local, so the first compare reads the
// 0xcccccccc stack fill), then draws the face as one GL_TRIANGLES (4) with three texcoord/array-element pairs.
// Type-7 tiles (via 0x10001f60) at a non-zero view angle pick a rotated texture set: index (m_28 % 4 - view
// angle index) wrapped to 0..3, plus 0x1b, in g_textures.
// drawTileObjects recurses into the wall neighbours facing the camera (walls 6/4, 6/0, 2/0, 2/4 for
// 0/90/180/other degrees), skipping hidden or culled tiles.
#include <windows.h>
#include <GL/gl.h>

struct Face {                            // stride 0x38, Tile +0x48
    int     v[3];                        // +0x00 array elements
    int     uv[3][2];                    // +0x0c (row *0x60, column *0x20 bytes into g_texCoords)
    GLuint  texture;                     // +0x24
    unsigned char slot;                  // +0x28 (texture slot in the rotated set)
    char    pad29[0x38 - 0x29];
};

class Tile {
public:
    void render();                       // 0x1000e6c0
    int  f_10001f60();                   // 0x10001f60 (compared with 7)
    bool isHidden();                     // 0x10015460
    int  getX();                         // 0x10005960
    int  getY();                         // 0x10006810
    void f_100108f0();                   // 0x100108f0 (path drawing; called when path flags +0x208 and +0x20a are set)
    void f_1000f7f0();                   // 0x1000f7f0
    void f_1000ea30(int n);              // 0x1000ea30

    char  m_pad0[0x28];
    int   m_28;                          // +0x28
    char  m_pad2c[0x44 - 0x2c];
    int   m_faceCount;                   // +0x44
    Face  m_faces[8];                    // +0x48
    bool  m_hasPath;                     // +0x208
    bool  m_209;
    bool  m_20a;                         // +0x20a
    char  m_pad20b[0x240 - 0x20b];
    int   m_variation;                   // +0x240
};

class Terrain {
public:
    void  drawTileObjects(Tile* t, Tile* center, float angle);  // 0x10007380
    bool  getWall(Tile* t, int i);
    Tile* tileAt(int x, int y);
    bool  isCulled(Tile* t, Tile* center, float angle);         // 0x10006850
    void  drawTile(Tile* t, Tile* center, float angle);         // 0x100381a0
};

extern int    g_viewAngleIndex;          // 0x10070a14
extern GLuint g_textures[0x25][0x19][9]; // 0x100687f8
extern float  g_texCoords[][3][4][2];    // 0x10063ca0

// MATCH: Terrain.dll 0x1000e6c0 ?render@Tile@@QAEXXZ
void Tile::render()
{
    GLuint last;
    int i;
    for (i = 0; i < m_faceCount; i++) {
        if (g_viewAngleIndex != 0 && f_10001f60() == 7) {
            int rot = m_28 % 4;
            rot -= g_viewAngleIndex;
            rot = rot < 0 ? rot + 4 : rot;
            GLuint tex = g_textures[rot + 0x1b][m_variation][m_faces[i].slot];
            if (tex != last) {
                glBindTexture(GL_TEXTURE_2D, tex);
                last = tex;
            }
        } else {
            if (m_faces[i].texture != last) {
                glBindTexture(GL_TEXTURE_2D, m_faces[i].texture);
                last = m_faces[i].texture;
            }
        }
        glBegin(GL_TRIANGLES);
        glTexCoord2fv(g_texCoords[m_faces[i].uv[0][0]][m_faces[i].uv[0][1]][g_viewAngleIndex]);
        glArrayElement(m_faces[i].v[0]);
        glTexCoord2fv(g_texCoords[m_faces[i].uv[1][0]][m_faces[i].uv[1][1]][g_viewAngleIndex]);
        glArrayElement(m_faces[i].v[1]);
        glTexCoord2fv(g_texCoords[m_faces[i].uv[2][0]][m_faces[i].uv[2][1]][g_viewAngleIndex]);
        glArrayElement(m_faces[i].v[2]);
        glEnd();
    }
    if (m_hasPath && m_20a)
        f_100108f0();
    f_1000f7f0();
    f_1000ea30(3);
}

// MATCH: Terrain.dll 0x10007380 ?drawTileObjects@Terrain@@QAEXPAVTile@@0M@Z
void Terrain::drawTileObjects(Tile* t, Tile* center, float angle)
{
    Tile* n[3];
    int x = t->getX();
    int y = t->getY();
    n[0] = NULL;
    n[1] = NULL;
    n[2] = NULL;
    if (angle == 0.0f) {
        if (getWall(t, 6)) {
            n[0] = tileAt(x - 1, y);
            n[2] = tileAt(x - 1, y + 1);
        }
        if (getWall(t, 4)) {
            n[1] = tileAt(x, y + 1);
            n[2] = tileAt(x - 1, y + 1);
        }
    } else if (angle == 90.0f) {
        if (getWall(t, 6)) {
            n[0] = tileAt(x - 1, y);
            n[2] = tileAt(x - 1, y - 1);
        }
        if (getWall(t, 0)) {
            n[1] = tileAt(x, y - 1);
            n[2] = tileAt(x - 1, y - 1);
        }
    } else if (angle == 180.0f) {
        if (getWall(t, 2)) {
            n[0] = tileAt(x + 1, y);
            n[2] = tileAt(x + 1, y - 1);
        }
        if (getWall(t, 0)) {
            n[1] = tileAt(x, y - 1);
            n[2] = tileAt(x + 1, y - 1);
        }
    } else {
        if (getWall(t, 2)) {
            n[0] = tileAt(x + 1, y);
            n[2] = tileAt(x + 1, y + 1);
        }
        if (getWall(t, 4)) {
            n[1] = tileAt(x, y + 1);
            n[2] = tileAt(x + 1, y + 1);
        }
    }
    for (int i = 0; i < 3; i++) {
        if (n[i] != NULL && !n[i]->isHidden() && !isCulled(n[i], center, angle)) {
            drawTile(n[i], center, angle);
            drawTileObjects(n[i], center, angle);
        }
    }
}

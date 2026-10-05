// Tile::drawPaths (Terrain.dll, debug build): for a tile with a path (+0x20a), draws a blended band (half-width
// 16.67) toward every neighbour that has a path too, collects them in a 4-bit mask and lets drawCornerOverlays
// (0x100116d0) close the corners. Texture 0x21 when the path is connected (+0x209), else 0x20.
// FLAGS Terrain.dll: /Od /ZI /GZ /GX /MTd
#include <windows.h>
#include <GL/gl.h>

extern float g_vertices[][3];                    // 0x100b28c8
extern float g_normals[][3];                     // 0x10070a18
extern unsigned int g_textures[0x25][0x19][9];  // 0x100687f8

struct Face { int v[3]; char pad[0x38 - 0xc]; };
struct Faces { int count; Face f[8]; };

class Tile {
public:
    void drawCornerOverlays(int mask, int type); // 0x100116d0 (terrain_corneroverlay.cpp)
    void drawPaths();
    char  m_pad0[0x34];
    Tile* m_n[4];                                // +0x34
    Faces m_faces;                               // +0x44
    bool  m_hasPath;                             // +0x208
    bool  m_connected;                           // +0x209
    bool  m_drawPath;                            // +0x20a
};

// MATCH: Terrain.dll 0x100108f0 ?drawPaths@Tile@@QAEXXZ
void Tile::drawPaths()
{
    int tex;
    int mask;
    float w;
    bool n0, n2, n3, n1;
    if (!m_drawPath)
        return;
    mask = 0;
    w = 16.67f;
    n0 = m_n[0] && m_n[0]->m_hasPath;
    n2 = m_n[2] && m_n[2]->m_hasPath;
    n3 = m_n[3] && m_n[3]->m_hasPath;
    n1 = m_n[1] && m_n[1]->m_hasPath;
    if (m_connected)
        tex = 0x21;
    else
        tex = 0x20;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindTexture(GL_TEXTURE_2D, g_textures[tex][0][0]);
    if (n0) {
        mask += 1;
        glBegin(GL_TRIANGLE_FAN);
        glNormal3fv(g_normals[m_faces.f[0].v[2]]);
        glTexCoord2f(0.333f, 0.667f);
        glVertex3f(g_vertices[m_faces.f[0].v[2]][0] - w, g_vertices[m_faces.f[0].v[2]][1], g_vertices[m_faces.f[0].v[2]][2]);
        glNormal3fv(g_normals[m_faces.f[0].v[2]]);
        glTexCoord2f(0.667f, 0.667f);
        glVertex3f(w + g_vertices[m_faces.f[0].v[2]][0], g_vertices[m_faces.f[0].v[2]][1], g_vertices[m_faces.f[0].v[2]][2]);
        glNormal3fv(g_normals[m_faces.f[1].v[2]]);
        glTexCoord2f(0.667f, 1.0f);
        glVertex3f(w + g_vertices[m_faces.f[0].v[2]][0], g_vertices[m_faces.f[1].v[2]][1], g_vertices[m_faces.f[1].v[2]][2]);
        glNormal3fv(g_normals[m_faces.f[1].v[2]]);
        glTexCoord2f(0.333f, 1.0f);
        glVertex3f(g_vertices[m_faces.f[0].v[2]][0] - w, g_vertices[m_faces.f[1].v[2]][1], g_vertices[m_faces.f[1].v[2]][2]);
        glEnd();
    }
    if (n1) {
        mask += 2;
        glBindTexture(GL_TEXTURE_2D, g_textures[tex][0][0]);
        glBegin(GL_TRIANGLE_FAN);
        glNormal3fv(g_normals[m_faces.f[0].v[2]]);
        glTexCoord2f(0.333f, 0.333f);
        glVertex3f(g_vertices[m_faces.f[0].v[2]][0] - w, g_vertices[m_faces.f[0].v[2]][1], g_vertices[m_faces.f[0].v[2]][2]);
        glNormal3fv(g_normals[m_faces.f[6].v[1]]);
        glTexCoord2f(0.333f, 0.0f);
        glVertex3f(g_vertices[m_faces.f[6].v[1]][0] - w, g_vertices[m_faces.f[6].v[1]][1], g_vertices[m_faces.f[6].v[1]][2]);
        glNormal3fv(g_normals[m_faces.f[6].v[2]]);
        glTexCoord2f(0.667f, 0.0f);
        glVertex3f(w + g_vertices[m_faces.f[6].v[1]][0], g_vertices[m_faces.f[6].v[1]][1], g_vertices[m_faces.f[6].v[1]][2]);
        glNormal3fv(g_normals[m_faces.f[0].v[2]]);
        glTexCoord2f(0.667f, 0.333f);
        glVertex3f(w + g_vertices[m_faces.f[0].v[2]][0], g_vertices[m_faces.f[0].v[2]][1], g_vertices[m_faces.f[0].v[2]][2]);
        glEnd();
    }
    if (n3) {
        mask += 4;
        glBindTexture(GL_TEXTURE_2D, g_textures[tex][0][0]);
        glBegin(GL_TRIANGLE_FAN);
        glNormal3fv(g_normals[m_faces.f[0].v[2]]);
        glTexCoord2f(0.667f, 0.667f);
        glVertex3f(g_vertices[m_faces.f[0].v[2]][0], g_vertices[m_faces.f[0].v[2]][1], g_vertices[m_faces.f[0].v[2]][2] - w);
        glNormal3fv(g_normals[m_faces.f[0].v[2]]);
        glTexCoord2f(0.667f, 0.333f);
        glVertex3f(g_vertices[m_faces.f[0].v[2]][0], g_vertices[m_faces.f[0].v[2]][1], w + g_vertices[m_faces.f[0].v[2]][2]);
        glNormal3fv(g_normals[m_faces.f[7].v[2]]);
        glTexCoord2f(1.0f, 0.333f);
        glVertex3f(g_vertices[m_faces.f[7].v[2]][0], g_vertices[m_faces.f[7].v[2]][1], w + g_vertices[m_faces.f[7].v[2]][2]);
        glNormal3fv(g_normals[m_faces.f[7].v[2]]);
        glTexCoord2f(1.0f, 0.667f);
        glVertex3f(g_vertices[m_faces.f[7].v[2]][0], g_vertices[m_faces.f[7].v[2]][1], g_vertices[m_faces.f[7].v[2]][2] - w);
        glEnd();
    }
    if (n2) {
        mask += 8;
        glBindTexture(GL_TEXTURE_2D, g_textures[tex][0][0]);
        glBegin(GL_TRIANGLE_FAN);
        glNormal3fv(g_normals[m_faces.f[0].v[2]]);
        glTexCoord2f(0.333f, 0.667f);
        glVertex3f(g_vertices[m_faces.f[0].v[2]][0], g_vertices[m_faces.f[0].v[2]][1], g_vertices[m_faces.f[0].v[2]][2] - w);
        glNormal3fv(g_normals[m_faces.f[4].v[0]]);
        glTexCoord2f(0.0f, 0.667f);
        glVertex3f(g_vertices[m_faces.f[4].v[0]][0], g_vertices[m_faces.f[4].v[0]][1], g_vertices[m_faces.f[4].v[0]][2] - w);
        glNormal3fv(g_normals[m_faces.f[4].v[0]]);
        glTexCoord2f(0.0f, 0.333f);
        glVertex3f(g_vertices[m_faces.f[4].v[0]][0], g_vertices[m_faces.f[4].v[0]][1], w + g_vertices[m_faces.f[4].v[0]][2]);
        glNormal3fv(g_normals[m_faces.f[0].v[2]]);
        glTexCoord2f(0.333f, 0.333f);
        glVertex3f(g_vertices[m_faces.f[0].v[2]][0], g_vertices[m_faces.f[0].v[2]][1], w + g_vertices[m_faces.f[0].v[2]][2]);
        glEnd();
    }
    drawCornerOverlays(mask, tex);
    glDisable(GL_BLEND);
}

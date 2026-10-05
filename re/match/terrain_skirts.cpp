// Tile::drawSkirts (Terrain.dll, debug build): toward each neighbour of type 0x14 that faces the camera
// (view quadrant 0x10070a14), draws a vertical wall from the shared edge down to y = -75 with the wall texture,
// then continues along that edge into the next neighbour, `depth` tiles at most.
// FLAGS Terrain.dll: /Od /ZI /GZ /GX /MTd
#include <windows.h>
#include <GL/gl.h>

extern float g_vertices[][3];                    // 0x100b28c8
extern int   g_viewQuadrant;                     // 0x10070a14
extern GLuint g_wallTexture;                     // 0x10070688
extern float g_skirtNormals1[2][3];              // 0x10063c40
extern float g_skirtNormals0[2][3];              // 0x10063c58
extern float g_skirtNormals3[2][3];              // 0x10063c70
extern float g_skirtNormals2[2][3];              // 0x10063c88

struct Face { int v[3]; char pad[0x38 - 0xc]; };
struct Faces { int count; Face f[8]; };

class Tile {
public:
    int  getType();                              // 0x10001f60
    void drawSkirts(int depth);
    char  m_pad0[0x24];
    int   m_type;                                // +0x24
    char  m_pad28[0x34 - 0x28];
    Tile* m_n[4];                                // +0x34
    Faces m_faces;                               // +0x44
};

// MATCH: Terrain.dll 0x1000ea30 ?drawSkirts@Tile@@QAEXH@Z
void Tile::drawSkirts(int depth)
{
    int c;
    if (m_type == 0x14)
        return;
    if (g_viewQuadrant == 0 || g_viewQuadrant == 2)
        c = 0;
    else
        c = 1;
    glBindTexture(GL_TEXTURE_2D, g_wallTexture);
    if (m_n[1] && m_n[1]->getType() == 0x14 && (g_viewQuadrant == 0 || g_viewQuadrant == 3)) {
        glBegin(GL_TRIANGLES);
        glNormal3fv(g_skirtNormals1[c]);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3fv(g_vertices[m_faces.f[5].v[0]]);
        glTexCoord2f(0.0f, 0.0f);
        glVertex3f(g_vertices[m_faces.f[5].v[0]][0], -75.0f, g_vertices[m_faces.f[5].v[0]][2]);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3f(g_vertices[m_faces.f[6].v[2]][0], -75.0f, g_vertices[m_faces.f[6].v[2]][2]);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3fv(g_vertices[m_faces.f[5].v[0]]);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3f(g_vertices[m_faces.f[6].v[2]][0], -75.0f, g_vertices[m_faces.f[6].v[2]][2]);
        glTexCoord2f(1.0f, 1.0f);
        glVertex3fv(g_vertices[m_faces.f[6].v[2]]);
        glEnd();
        if (g_viewQuadrant == 0) {
            if (m_n[3] && depth > 0)
                m_n[3]->drawSkirts(depth - 1);
        } else if (g_viewQuadrant == 3 && m_n[2] && depth > 0) {
            m_n[2]->drawSkirts(depth - 1);
        }
    }
    if (m_n[0] && m_n[0]->getType() == 0x14 && (g_viewQuadrant == 1 || g_viewQuadrant == 2)) {
        glBegin(GL_TRIANGLES);
        glNormal3fv(g_skirtNormals0[c]);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3fv(g_vertices[m_faces.f[2].v[2]]);
        glTexCoord2f(0.0f, 0.0f);
        glVertex3f(g_vertices[m_faces.f[2].v[2]][0], -75.0f, g_vertices[m_faces.f[2].v[2]][2]);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3f(g_vertices[m_faces.f[0].v[0]][0], -75.0f, g_vertices[m_faces.f[0].v[0]][2]);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3fv(g_vertices[m_faces.f[2].v[2]]);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3f(g_vertices[m_faces.f[0].v[0]][0], -75.0f, g_vertices[m_faces.f[0].v[0]][2]);
        glTexCoord2f(1.0f, 1.0f);
        glVertex3fv(g_vertices[m_faces.f[0].v[0]]);
        glEnd();
        if (g_viewQuadrant == 1) {
            if (m_n[3] && depth > 0)
                m_n[3]->drawSkirts(depth - 1);
        } else if (g_viewQuadrant == 2 && m_n[2] && depth > 0) {
            m_n[2]->drawSkirts(depth - 1);
        }
    }
    if (m_n[2] && m_n[2]->getType() == 0x14 && (g_viewQuadrant == 0 || g_viewQuadrant == 1)) {
        glBegin(GL_TRIANGLES);
        glNormal3fv(g_skirtNormals2[c]);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3fv(g_vertices[m_faces.f[0].v[0]]);
        glTexCoord2f(0.0f, 0.0f);
        glVertex3f(g_vertices[m_faces.f[0].v[0]][0], -75.0f, g_vertices[m_faces.f[0].v[0]][2]);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3f(g_vertices[m_faces.f[4].v[1]][0], -75.0f, g_vertices[m_faces.f[4].v[1]][2]);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3fv(g_vertices[m_faces.f[0].v[0]]);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3f(g_vertices[m_faces.f[4].v[1]][0], -75.0f, g_vertices[m_faces.f[4].v[1]][2]);
        glTexCoord2f(1.0f, 1.0f);
        glVertex3fv(g_vertices[m_faces.f[4].v[1]]);
        glEnd();
        if (g_viewQuadrant == 0) {
            if (m_n[0] && depth > 0)
                m_n[0]->drawSkirts(depth - 1);
        } else if (g_viewQuadrant == 1 && m_n[1] && depth > 0) {
            m_n[1]->drawSkirts(depth - 1);
        }
    }
    if (m_n[3] && m_n[3]->getType() == 0x14 && (g_viewQuadrant == 2 || g_viewQuadrant == 3)) {
        glBegin(GL_TRIANGLES);
        glNormal3fv(g_skirtNormals3[c]);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3fv(g_vertices[m_faces.f[7].v[1]]);
        glTexCoord2f(0.0f, 0.0f);
        glVertex3f(g_vertices[m_faces.f[7].v[1]][0], -75.0f, g_vertices[m_faces.f[7].v[1]][2]);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3f(g_vertices[m_faces.f[3].v[2]][0], -75.0f, g_vertices[m_faces.f[3].v[2]][2]);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3fv(g_vertices[m_faces.f[7].v[1]]);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3f(g_vertices[m_faces.f[3].v[2]][0], -75.0f, g_vertices[m_faces.f[3].v[2]][2]);
        glTexCoord2f(1.0f, 1.0f);
        glVertex3fv(g_vertices[m_faces.f[3].v[2]]);
        glEnd();
        if (g_viewQuadrant == 2) {
            if (m_n[1] && depth > 0)
                m_n[1]->drawSkirts(depth - 1);
        } else if (g_viewQuadrant == 3 && m_n[0] && depth > 0) {
            m_n[0]->drawSkirts(depth - 1);
        }
    }
}

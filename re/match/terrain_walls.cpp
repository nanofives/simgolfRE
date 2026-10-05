// Tile::drawWalls (Terrain.dll, debug build): for each of the four edges whose wall flag is set
// (WallInfo +0x234/+0x23a/+0x236/+0x238 -> neighbours 0, 2, 3, 1) and that has a neighbour, draws a wall
// strip of four triangles between this tile's edge vertices and the neighbour's facing vertices.
// Wall kind 1 binds the texture at 0x10070304, any other kind the one at 0x1006ff80. The normal pair is
// picked by the view quadrant (0x10070a14): row 0 for quadrants 0 and 2, row 1 otherwise.
// FLAGS Terrain.dll: /Od /ZI /GZ /GX /MTd
#include <windows.h>
#include <GL/gl.h>

extern float  g_vertices[][3];                   // 0x100b28c8
extern int    g_viewQuadrant;                    // 0x10070a14
extern GLuint g_fenceTexture1;                   // 0x10070304 (wall kind 1)
extern GLuint g_fenceTexture0;                   // 0x1006ff80 (other kinds)
extern float  g_sideNormals_63c40[2][3];         // 0x10063c40 (edge toward m_n[0])
extern float  g_sideNormals_63c58[2][3];         // 0x10063c58 (m_n[1])
extern float  g_sideNormals_63c70[2][3];         // 0x10063c70 (m_n[2])
extern float  g_sideNormals_63c88[2][3];         // 0x10063c88 (m_n[3])

struct Face { int v[3]; char pad[0x38 - 0xc]; };
struct Faces { int count; Face f[8]; };

struct WallInfo {                                // Tile +0x210
    int   kind[9];                               // +0x00 (compared with 1)
    bool  on[9];                                 // +0x24
};

class Tile {
public:
    void drawWalls();
    char  m_pad0[0x34];
    Tile* m_n[4];                                // +0x34
    Faces m_faces;                               // +0x44
    char  m_pad208[0x210 - 0x208];
    WallInfo m_walls;                            // +0x210
};

// MATCH: Terrain.dll 0x1000f7f0 ?drawWalls@Tile@@QAEXXZ
void Tile::drawWalls()
{
    int c;
    if (g_viewQuadrant == 0 || g_viewQuadrant == 2)
        c = 0;
    else
        c = 1;
    if (m_walls.on[0] && m_n[0] != 0) {
        if (m_walls.kind[0] == 1) {
            glBindTexture(GL_TEXTURE_2D, g_fenceTexture1);
        } else {
            glBindTexture(GL_TEXTURE_2D, g_fenceTexture0);
        }
        glBegin(GL_TRIANGLES);
        glNormal3fv(g_sideNormals_63c40[c]);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3fv(g_vertices[m_n[0]->m_faces.f[5].v[0]]);
        glTexCoord2f(0.0f, 0.0f);
        glVertex3fv(g_vertices[m_faces.f[0].v[0]]);
        glTexCoord2f(0.5f, 0.0f);
        glVertex3fv(g_vertices[m_faces.f[1].v[2]]);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3fv(g_vertices[m_n[0]->m_faces.f[5].v[0]]);
        glTexCoord2f(0.5f, 0.0f);
        glVertex3fv(g_vertices[m_faces.f[1].v[2]]);
        glTexCoord2f(0.5f, 1.0f);
        glVertex3fv(g_vertices[m_n[0]->m_faces.f[5].v[1]]);
        glTexCoord2f(0.5f, 1.0f);
        glVertex3fv(g_vertices[m_n[0]->m_faces.f[5].v[1]]);
        glTexCoord2f(0.5f, 0.0f);
        glVertex3fv(g_vertices[m_faces.f[1].v[2]]);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3fv(g_vertices[m_faces.f[2].v[2]]);
        glTexCoord2f(0.5f, 1.0f);
        glVertex3fv(g_vertices[m_n[0]->m_faces.f[5].v[1]]);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3fv(g_vertices[m_faces.f[2].v[2]]);
        glTexCoord2f(1.0f, 1.0f);
        glVertex3fv(g_vertices[m_n[0]->m_faces.f[6].v[2]]);
        glEnd();
    }
    if (m_walls.on[6] && m_n[2] != 0) {
        if (m_walls.kind[6] == 1) {
            glBindTexture(GL_TEXTURE_2D, g_fenceTexture1);
        } else {
            glBindTexture(GL_TEXTURE_2D, g_fenceTexture0);
        }
        glBegin(GL_TRIANGLES);
        glNormal3fv(g_sideNormals_63c70[c]);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3fv(g_vertices[m_n[2]->m_faces.f[7].v[1]]);
        glTexCoord2f(0.0f, 0.0f);
        glVertex3fv(g_vertices[m_faces.f[4].v[1]]);
        glTexCoord2f(0.5f, 0.0f);
        glVertex3fv(g_vertices[m_faces.f[4].v[0]]);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3fv(g_vertices[m_n[2]->m_faces.f[7].v[1]]);
        glTexCoord2f(0.5f, 0.0f);
        glVertex3fv(g_vertices[m_faces.f[4].v[0]]);
        glTexCoord2f(0.5f, 1.0f);
        glVertex3fv(g_vertices[m_n[2]->m_faces.f[3].v[1]]);
        glTexCoord2f(0.5f, 1.0f);
        glVertex3fv(g_vertices[m_n[2]->m_faces.f[3].v[1]]);
        glTexCoord2f(0.5f, 0.0f);
        glVertex3fv(g_vertices[m_faces.f[4].v[0]]);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3fv(g_vertices[m_faces.f[0].v[0]]);
        glTexCoord2f(0.5f, 1.0f);
        glVertex3fv(g_vertices[m_n[2]->m_faces.f[3].v[1]]);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3fv(g_vertices[m_faces.f[0].v[0]]);
        glTexCoord2f(1.0f, 1.0f);
        glVertex3fv(g_vertices[m_n[2]->m_faces.f[3].v[2]]);
        glEnd();
    }
    if (m_walls.on[2] && m_n[3] != 0) {
        if (m_walls.kind[2] == 1) {
            glBindTexture(GL_TEXTURE_2D, g_fenceTexture1);
        } else {
            glBindTexture(GL_TEXTURE_2D, g_fenceTexture0);
        }
        glBegin(GL_TRIANGLES);
        glNormal3fv(g_sideNormals_63c88[c]);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3fv(g_vertices[m_n[3]->m_faces.f[0].v[0]]);
        glTexCoord2f(0.0f, 0.0f);
        glVertex3fv(g_vertices[m_faces.f[3].v[2]]);
        glTexCoord2f(0.5f, 0.0f);
        glVertex3fv(g_vertices[m_faces.f[3].v[1]]);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3fv(g_vertices[m_n[3]->m_faces.f[0].v[0]]);
        glTexCoord2f(0.5f, 0.0f);
        glVertex3fv(g_vertices[m_faces.f[3].v[1]]);
        glTexCoord2f(0.5f, 1.0f);
        glVertex3fv(g_vertices[m_n[3]->m_faces.f[4].v[0]]);
        glTexCoord2f(0.5f, 1.0f);
        glVertex3fv(g_vertices[m_n[3]->m_faces.f[4].v[0]]);
        glTexCoord2f(0.5f, 0.0f);
        glVertex3fv(g_vertices[m_faces.f[3].v[1]]);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3fv(g_vertices[m_faces.f[7].v[1]]);
        glTexCoord2f(0.5f, 1.0f);
        glVertex3fv(g_vertices[m_n[3]->m_faces.f[4].v[0]]);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3fv(g_vertices[m_faces.f[7].v[1]]);
        glTexCoord2f(1.0f, 1.0f);
        glVertex3fv(g_vertices[m_n[3]->m_faces.f[4].v[1]]);
        glEnd();
    }
    if (m_walls.on[4] && m_n[1] != 0) {
        if (m_walls.kind[4] == 1) {
            glBindTexture(GL_TEXTURE_2D, g_fenceTexture1);
        } else {
            glBindTexture(GL_TEXTURE_2D, g_fenceTexture0);
        }
        glBegin(GL_TRIANGLES);
        glNormal3fv(g_sideNormals_63c58[c]);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3fv(g_vertices[m_n[1]->m_faces.f[2].v[2]]);
        glTexCoord2f(0.0f, 0.0f);
        glVertex3fv(g_vertices[m_faces.f[6].v[2]]);
        glTexCoord2f(0.5f, 0.0f);
        glVertex3fv(g_vertices[m_faces.f[5].v[1]]);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3fv(g_vertices[m_n[1]->m_faces.f[2].v[2]]);
        glTexCoord2f(0.5f, 0.0f);
        glVertex3fv(g_vertices[m_faces.f[5].v[1]]);
        glTexCoord2f(0.5f, 1.0f);
        glVertex3fv(g_vertices[m_n[1]->m_faces.f[1].v[2]]);
        glTexCoord2f(0.5f, 1.0f);
        glVertex3fv(g_vertices[m_n[1]->m_faces.f[1].v[2]]);
        glTexCoord2f(0.5f, 0.0f);
        glVertex3fv(g_vertices[m_faces.f[5].v[1]]);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3fv(g_vertices[m_faces.f[5].v[0]]);
        glTexCoord2f(0.5f, 1.0f);
        glVertex3fv(g_vertices[m_n[1]->m_faces.f[1].v[2]]);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3fv(g_vertices[m_faces.f[5].v[0]]);
        glTexCoord2f(1.0f, 1.0f);
        glVertex3fv(g_vertices[m_n[1]->m_faces.f[0].v[0]]);
        glEnd();
    }
}

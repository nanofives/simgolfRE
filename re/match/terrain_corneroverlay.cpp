// Tile::drawCornerOverlays (Terrain.dll, debug build): for an overlay mask 0..15, binds textures 2 or 3 of
// `type` and draws up to four corner triangles over the tile's face vertices (glArrayElement).
// FLAGS Terrain.dll: /Od /ZI /GZ /GX /MTd
#include <windows.h>
#include <GL/gl.h>

extern unsigned int g_textures[0x25][0x19][9];  // 0x100687f8

struct Face { int v[3]; char pad[0x38 - 0xc]; };
struct Faces { int count; Face f[8]; };

class Tile {
public:
    void drawCornerOverlays(int mask, int type);
    char  m_pad0[0x44];
    Faces m_faces;                               // +0x44
};

// MATCH: Terrain.dll 0x100116d0 ?drawCornerOverlays@Tile@@QAEXHH@Z
void Tile::drawCornerOverlays(int mask, int type)
{
    int a, b, c, d;
    a = 0;
    b = 0;
    c = 0;
    d = 0;
    switch (mask) {
    case 0: c = d = a = b = 3; break;
    case 1: d = c = 3; break;
    case 2: b = a = 3; break;
    case 4: a = c = 3; break;
    case 8: b = d = 3; break;
    case 5: c = 3; b = 2; break;
    case 9: d = 3; a = 2; break;
    case 7: d = b = 2; break;
    case 11: c = a = 2; break;
    case 13: b = a = 2; break;
    case 6: a = 3; d = 2; break;
    case 10: b = 3; c = 2; break;
    case 14: d = c = 2; break;
    case 15: c = d = a = b = 2; break;
    default: return;
    }
    if (a) {
        glBindTexture(GL_TEXTURE_2D, g_textures[type][0][a]);
        glBegin(GL_TRIANGLES);
        glTexCoord2f(0.0f, 0.5f);
        glArrayElement(m_faces.f[0].v[1]);
        glTexCoord2f(0.5f, 0.5f);
        glArrayElement(m_faces.f[0].v[2]);
        glTexCoord2f(0.5f, 1.0f);
        glArrayElement(m_faces.f[1].v[2]);
        glEnd();
    }
    if (b) {
        glBindTexture(GL_TEXTURE_2D, g_textures[type][0][b]);
        glBegin(GL_TRIANGLES);
        glTexCoord2f(0.5f, 1.0f);
        glArrayElement(m_faces.f[1].v[2]);
        glTexCoord2f(0.5f, 0.5f);
        glArrayElement(m_faces.f[0].v[2]);
        glTexCoord2f(1.0f, 0.5f);
        glArrayElement(m_faces.f[3].v[1]);
        glEnd();
    }
    if (c) {
        glBindTexture(GL_TEXTURE_2D, g_textures[type][0][c]);
        glBegin(GL_TRIANGLES);
        glTexCoord2f(0.5f, 0.0f);
        glArrayElement(m_faces.f[5].v[1]);
        glTexCoord2f(0.5f, 0.5f);
        glArrayElement(m_faces.f[4].v[2]);
        glTexCoord2f(0.0f, 0.5f);
        glArrayElement(m_faces.f[4].v[0]);
        glEnd();
    }
    if (d) {
        glBindTexture(GL_TEXTURE_2D, g_textures[type][0][d]);
        glBegin(GL_TRIANGLES);
        glTexCoord2f(1.0f, 0.5f);
        glArrayElement(m_faces.f[3].v[1]);
        glTexCoord2f(0.5f, 0.5f);
        glArrayElement(m_faces.f[4].v[2]);
        glTexCoord2f(0.5f, 0.0f);
        glArrayElement(m_faces.f[5].v[1]);
        glEnd();
    }
}

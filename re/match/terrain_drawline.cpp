// Terrain::drawLine (Terrain.dll, debug): a 2D overlay line in screen space over the 3D view.
// FLAGS Terrain.dll: /Od /ZI /GZ /GX
//
// color is RGB555 (r = bits 10-14, g = 5-9, b = 0-4, each scaled to 8 bits by the 0xF8 masks at
// 0x100048c3 / 0x100048dd / 0x100048f8); /255.0f (constant at 0x1005f200); alpha /10.0f (0x1005f214).
#include <windows.h>
#include <GL/gl.h>

class Terrain {
public:
    void drawLine(int x1, int y1, int x2, int y2, int color, int width, int alpha);
    char m_pad0[0x20];
    int  m_screenW;                      // +0x20
    int  m_screenH;                      // +0x24
    bool m_flipY;                        // +0x28
};

// MATCH: Terrain.dll 0x100048a0 ?drawLine@Terrain@@QAEXHHHHHHH@Z
void Terrain::drawLine(int x1, int y1, int x2, int y2, int color, int width, int alpha)
{
    float r = ((color >> 7) & 0xF8) / 255.0f;
    float g = ((color >> 2) & 0xF8) / 255.0f;
    float b = ((color << 3) & 0xF8) / 255.0f;

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    if (m_flipY)
        glOrtho(0, m_screenW, 0, m_screenH, -1.0, 1.0);
    else
        glOrtho(0, m_screenW, m_screenH, 0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth((float)width);
    glColor4f(r, g, b, alpha / 10.0f);
    glBegin(GL_LINES);
    glVertex2i(x1, y1);
    glVertex2i(x2, y2);
    glEnd();
    glEnable(GL_LIGHTING);
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glFlush();
}

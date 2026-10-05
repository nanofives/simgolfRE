// Terrain.dll screen-space splines (debug build): drawCardinalSpline (Hermite with tangents from two extra
// points) and drawBezierSpline (quadratic Bernstein), both as GL line strips with a 5:5:5 colour and an alpha
// in tenths.
// FLAGS Terrain.dll: /Od /ZI /GZ /GX /MTd
#include <windows.h>
#include <GL/gl.h>
#include <stdlib.h>

class Terrain {
public:
    void hermitePoint(float* out, float t, float* p0, float* p1, float* t0, float* t1);   // 0x10005090
    float bernstein(float t, int i);                                                       // 0x10005750
    void drawCardinalSpline(int x1, int y1, int x2, int y2, int ax, int ay, int bx, int by, int color,
                            int width, int alpha);
    void drawBezierSpline(int x1, int y1, int x2, int y2, int x3, int y3, int color, int width, int alpha);
    char  m_pad0[0x20];
    int   m_screenW, m_screenH;                  // +0x20, +0x24
    bool  m_flipY;                               // +0x28
};

// MATCH: Terrain.dll 0x10004c70 ?drawCardinalSpline@Terrain@@QAEXHHHHHHHHHHH@Z
void Terrain::drawCardinalSpline(int x1, int y1, int x2, int y2, int ax, int ay, int bx, int by, int color,
                                 int width, int alpha)
{
    float r = ((color >> 7) & 0xf8) / 255.0f;
    float g = ((color >> 2) & 0xf8) / 255.0f;
    float b = ((color << 3) & 0xf8) / 255.0f;
    float out[2];
    float p0[2];
    float p1[2];
    float t0[2];
    float t1[2];
    float t;
    t0[0] = (float)(x1 - ax);
    t0[1] = (float)(y1 - ay);
    t1[0] = (float)(bx - x2);
    t1[1] = (float)(by - y2);
    p0[0] = (float)x1;
    p0[1] = (float)y1;
    p1[0] = (float)x2;
    p1[1] = (float)y2;
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, m_screenW, m_screenH, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth((float)width);
    glColor4f(r, g, b, alpha / 10.0f);
    glBegin(GL_LINE_STRIP);
    glVertex2fv(p0);
    for (t = 0.01f; t < 1.0f; t += 0.01f) {
        hermitePoint(out, t, p0, p1, t0, t1);
        glVertex2fv(out);
    }
    glVertex2fv(p1);
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

// MATCH: Terrain.dll 0x10005230 ?drawBezierSpline@Terrain@@QAEXHHHHHHHHH@Z
void Terrain::drawBezierSpline(int x1, int y1, int x2, int y2, int x3, int y3, int color, int width, int alpha)
{
    float r = ((color >> 7) & 0xf8) / 255.0f;
    float g = ((color >> 2) & 0xf8) / 255.0f;
    float b = ((color << 3) & 0xf8) / 255.0f;
    float pt[2];
    float pts[3][2];
    float w;
    float step;
    float t;
    int i;
    int j;
    step = 1.0f / ((abs(x1 - x2) + abs(y1 - y2) + abs(x2 - x3) + abs(y2 - y3)) / 10.0f);
    pts[0][0] = (float)x1;
    pts[0][1] = (float)y1;
    pts[1][0] = (float)x2;
    pts[1][1] = (float)y2;
    pts[2][0] = (float)x3;
    pts[2][1] = (float)y3;
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    if (m_flipY)
        glOrtho(0.0, m_screenW, 0.0, m_screenH, -1.0, 1.0);
    else
        glOrtho(0.0, m_screenW, m_screenH, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth((float)width);
    glColor4f(r, g, b, alpha / 10.0f);
    glBegin(GL_LINE_STRIP);
    glVertex2fv(pts[0]);
    for (t = 0.0f + step; t < 1.0f; t += step) {
        pt[0] = 0.0f;
        pt[1] = 0.0f;
        for (i = 0; i < 3; i++) {
            w = bernstein(t, i);
            for (j = 0; j < 2; j++)
                pt[j] = w * pts[i][j] + pt[j];
        }
        glVertex2fv(pt);
    }
    glVertex2fv(pts[2]);
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

// Terrain::render (Terrain.dll, debug): draws the visible window of tiles around `pTile` for a camera angle.
// FLAGS Terrain.dll: /Od /ZI /GZ /GX
//
// Window half-sizes start at 16 << m_1c (0x100059c4) and are clipped to the 50x50 grid (0x32); with no
// centre tile it draws the fixed region x 11..40, y 10..39. Always returns true (mov al,1 at 0x10005e77).
#include <windows.h>
#include <GL/gl.h>

class Tile {
public:
    int  getY();                         // 0x10006810
    int  getX();                         // 0x10005960
    bool isHidden();                     // 0x10015460 (name tentative)
    void render();                       // 0x1000e6c0 (name tentative; also used by localRender's list)
};

class Terrain {
public:
    bool  render(Tile* pTile, float angle);
    Tile* tileAt(int x, int y);
    void  setViewAngle(float angle);                            // 0x1000adc0 (tentative)
    bool  isCulled(Tile* t, Tile* center, float angle);         // 0x10006850 (tentative)
    char  m_pad0[0x1c];
    int   m_1c;                          // +0x1c
};

extern float g_screenScale;              // 0x10070a10

// MATCH: Terrain.dll 0x10005990 ?render@Terrain@@QAE_NPAVTile@@M@Z
bool Terrain::render(Tile* pTile, float angle)
{
    int up, down, left, right;
    Tile* t = NULL;
    up = down = left = right = 16 << m_1c;

    glLoadIdentity();
    glPushMatrix();
    glRotated(g_screenScale, 1.0, 0.0, 0.0);
    glRotatef(45.0f + angle, 0.0f, 1.0f, 0.0f);
    if (pTile != NULL)
        glTranslatef((25 - pTile->getX()) * 100.0f, 0.0f, (25 - pTile->getY()) * 100.0f);
    setViewAngle(angle);

    if (pTile == NULL) {
        for (int yn = 10; yn < 40; yn++)
            for (int xn = 40; xn > 10; xn--)
                tileAt(xn, yn)->render();
    } else {
        if (pTile->getY() < up)
            up = pTile->getY();
        if (50 - pTile->getY() < down)
            down = 50 - pTile->getY();
        if (pTile->getX() < left)
            left = pTile->getX();
        if (50 - pTile->getX() < right)
            right = 50 - pTile->getX();

        if (angle == 0.0f) {
            for (int y0 = pTile->getY() - up; y0 < pTile->getY() + down; y0++)
                for (int x0 = pTile->getX() + right; x0 >= pTile->getX() - left; x0--) {
                    t = tileAt(x0, y0);
                    if (t != NULL && !t->isHidden() && !isCulled(t, pTile, angle))
                        t->render();
                }
        } else if (angle == 90.0f) {
            for (int x1 = pTile->getX() + right; x1 >= pTile->getX() - left; x1--)
                for (int y1 = pTile->getY() + down; y1 >= pTile->getY() - up; y1--) {
                    t = tileAt(x1, y1);
                    if (t != NULL && !t->isHidden() && !isCulled(t, pTile, angle))
                        t->render();
                }
        } else if (angle == 180.0f) {
            for (int y2 = pTile->getY() + down; y2 >= pTile->getY() - up; y2--)
                for (int x2 = pTile->getX() - left; x2 < pTile->getX() + right; x2++) {
                    t = tileAt(x2, y2);
                    if (t != NULL && !t->isHidden() && !isCulled(t, pTile, angle))
                        t->render();
                }
        } else {
            for (int y3 = pTile->getY() - up; y3 < pTile->getY() + down; y3++)
                for (int x3 = pTile->getX() - left; x3 < pTile->getX() + right; x3++) {
                    t = tileAt(x3, y3);
                    if (t != NULL && !t->isHidden() && !isCulled(t, pTile, angle))
                        t->render();
                }
        }
    }
    glPopMatrix();
    glFlush();
    return true;
}

// Terrain 0x10038900, called by Terrain::lowerEdgeCorner as `lowerEdge(t, other, amount)` (that name is
// kept from terrain_methods.cpp; the body does not lower anything). Same transform setup as localRender
// (assert at base+1, 0x10063e44 = 1520), then, when a neighbour toward the camera is missing or hidden, draws
// the tile five times at +-10/20 offsets (0x41200000 = 10.0, 0x41a00000 = 20.0) after 0x10003830, and switches
// GL_LIGHT1 (0x4001) off and GL_LIGHT0 (0x4000) on.
// FLAGS Terrain.dll: /Od /ZI /GZ /GX
#include <windows.h>
#include <GL/gl.h>
#include <assert.h>

class Tile {
public:
    int  getY();                         // 0x10006810
    int  getX();                         // 0x10005960
    bool isHidden();                     // 0x10015460
    void render();                       // 0x1000e6c0
};

class Terrain {
public:
    void  lowerEdge(Tile* pTile, Tile* pivot, float angle);
    Tile* tileAt(int x, int y);
    void  setViewAngle(float angle);     // 0x1000adc0
    void  f_10003830();                  // 0x10003830
};

extern float g_screenScale;              // 0x10070a10

// MATCH: Terrain.dll 0x10038900 ?lowerEdge@Terrain@@QAEXPAVTile@@0M@Z
void Terrain::lowerEdge(Tile* pTile, Tile* pivot, float angle)
{
    assert(pTile != NULL);
    bool draw = false;
    glLoadIdentity();
    glPushMatrix();
    glRotated(g_screenScale, 1.0, 0.0, 0.0);
    glRotatef(45.0f + angle, 0.0f, 1.0f, 0.0f);
    if (pTile != NULL)
        glTranslatef((25 - pivot->getX()) * 100.0f, 0.0f, (25 - pivot->getY()) * 100.0f);
    setViewAngle(angle);
    if (angle == 0.0f) {
        if (tileAt(pTile->getX() + 1, pTile->getY()) == NULL || tileAt(pTile->getX() + 1, pTile->getY())->isHidden()
            || tileAt(pTile->getX(), pTile->getY() - 1) == NULL || tileAt(pTile->getX(), pTile->getY() - 1)->isHidden()
            || tileAt(pTile->getX() + 1, pTile->getY() - 1) == NULL || tileAt(pTile->getX() + 1, pTile->getY() - 1)->isHidden())
            draw = true;
    } else if (angle == 90.0f) {
        if (tileAt(pTile->getX() + 1, pTile->getY()) == NULL || tileAt(pTile->getX() + 1, pTile->getY())->isHidden()
            || tileAt(pTile->getX(), pTile->getY() + 1) == NULL || tileAt(pTile->getX(), pTile->getY() + 1)->isHidden()
            || tileAt(pTile->getX() + 1, pTile->getY() + 1) == NULL || tileAt(pTile->getX() + 1, pTile->getY() + 1)->isHidden())
            draw = true;
    } else if (angle == 180.0f) {
        if (tileAt(pTile->getX() - 1, pTile->getY()) == NULL || tileAt(pTile->getX() - 1, pTile->getY())->isHidden()
            || tileAt(pTile->getX(), pTile->getY() + 1) == NULL || tileAt(pTile->getX(), pTile->getY() + 1)->isHidden()
            || tileAt(pTile->getX() - 1, pTile->getY() + 1) == NULL || tileAt(pTile->getX() - 1, pTile->getY() + 1)->isHidden())
            draw = true;
    } else {
        if (tileAt(pTile->getX() - 1, pTile->getY()) == NULL || tileAt(pTile->getX() - 1, pTile->getY())->isHidden()
            || tileAt(pTile->getX(), pTile->getY() - 1) == NULL || tileAt(pTile->getX(), pTile->getY() - 1)->isHidden()
            || tileAt(pTile->getX() - 1, pTile->getY() - 1) == NULL || tileAt(pTile->getX() - 1, pTile->getY() - 1)->isHidden())
            draw = true;
    }
    if (draw) {
        f_10003830();
        glTranslatef(10.0f, 0.0f, -10.0f);
        pTile->render();
        glTranslatef(-20.0f, 0.0f, 0.0f);
        pTile->render();
        glTranslatef(0.0f, 0.0f, 20.0f);
        pTile->render();
        glTranslatef(20.0f, 0.0f, 0.0f);
        pTile->render();
        glTranslatef(-10.0f, 0.0f, -10.0f);
        pTile->render();
        glDisable(GL_LIGHT1);
        glEnable(GL_LIGHT0);
    }
    glPopMatrix();
    glFlush();
}

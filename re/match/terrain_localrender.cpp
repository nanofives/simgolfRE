// Terrain::localRender (Terrain.dll, debug). Original file: C:\Projects\3DTerrainLowPoly\Terrain.cpp
// (assert at 0x10008a1b); with /ZI, __LINE__ is relative to a per-function variable (0x10063e48 = 1605),
// so the assert must stay exactly 3 lines below the function's base line to match.
// FLAGS Terrain.dll: /Od /ZI /GZ /GX
#pragma warning(disable: 4786)
#include <windows.h>
#include <GL/gl.h>
#include <assert.h>
#include <list>

class Tile {
public:
    int  getY();                         // 0x10006810
    int  getX();                         // 0x10005960
    bool isHidden();                     // 0x10015460 (name tentative)
    void render();                       // 0x1000e6c0 (same callee Terrain::render uses on tiles)
};

class Terrain {
public:
    void  localRender(Tile* pTile, Tile* pivot, float angle);
    Tile* tileAt(int x, int y);
    void  setViewAngle(float angle);                          // 0x1000adc0 (tentative)
    void  drawTile(Tile* t, Tile* center, float angle);       // 0x100381a0 (tentative)
    void  drawTileObjects(Tile* t, Tile* center, float angle);// 0x10007380 (tentative; also calls tileAt)
    char  m_pad[0x164ac4];
    std::list<Tile*> m_drawList;     // +0x164ac4
};

extern float g_screenScale;              // 0x10070a10

// MATCH: Terrain.dll 0x100089e0 ?localRender@Terrain@@QAEXPAVTile@@0M@Z
void Terrain::localRender(Tile* pTile, Tile* pivot, float angle)
{
    // (two source lines here in the original: the assert is base+3, movsx/add 3 at 0x10008a06)

    assert(pTile != NULL);
    glLoadIdentity();
    glPushMatrix();
    glRotated(g_screenScale, 1.0, 0.0, 0.0);
    glRotatef(45.0f + angle, 0.0f, 1.0f, 0.0f);
    if (pTile != NULL)
        glTranslatef((25 - pivot->getX()) * 100.0f, 0.0f, (25 - pivot->getY()) * 100.0f);
    setViewAngle(angle);

    Tile* t;
    if (angle == 0.0f) {
        for (int y0 = pTile->getY() - 2; y0 <= pTile->getY() + 2; y0++)
            for (int x0 = pTile->getX() + 2; x0 >= pTile->getX() - 2; x0--) {
                t = tileAt(x0, y0);
                if (t != NULL) {
                    if (t->isHidden())
                        continue;
                    drawTile(t, pTile, angle);
                    drawTileObjects(t, pTile, angle);
                }
            }
    } else if (angle == 90.0f) {
        for (int x1 = pTile->getX() + 2; x1 >= pTile->getX() - 2; x1--)
            for (int y1 = pTile->getY() + 2; y1 >= pTile->getY() - 2; y1--) {
                t = tileAt(x1, y1);
                if (t != NULL) {
                    if (t->isHidden())
                        continue;
                    drawTile(t, pTile, angle);
                    drawTileObjects(t, pTile, angle);
                }
            }
    } else if (angle == 180.0f) {
        for (int y2 = pTile->getY() + 2; y2 >= pTile->getY() - 2; y2--)
            for (int x2 = pTile->getX() - 2; x2 <= pTile->getX() + 2; x2++) {
                t = tileAt(x2, y2);
                if (t != NULL) {
                    if (t->isHidden())
                        continue;
                    drawTile(t, pTile, angle);
                    drawTileObjects(t, pTile, angle);
                }
            }
    } else {
        for (int y3 = pTile->getY() - 2; y3 <= pTile->getY() + 2; y3++)
            for (int x3 = pTile->getX() - 2; x3 <= pTile->getX() + 2; x3++) {
                t = tileAt(x3, y3);
                if (t != NULL) {
                    if (t->isHidden())
                        continue;
                    drawTile(t, pTile, angle);
                    drawTileObjects(t, pTile, angle);
                }
            }
    }

    for (std::list<Tile*>::iterator it = m_drawList.begin(); it != m_drawList.end(); it++)
        (*it)->render();
    m_drawList.clear();
    glPopMatrix();
    glFlush();
}

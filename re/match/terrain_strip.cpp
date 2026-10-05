// Terrain::stripRender (Terrain.dll, debug build): draws the tiles around `pivot` in diagonal strips whose order
// depends on the view direction (1..8; odd directions render their two neighbouring even directions), then
// renders and clears the draw list.
// FLAGS Terrain.dll: /Od /ZI /GZ /GX /MTd
#pragma warning(disable: 4786)
#include <windows.h>
#include <GL/gl.h>
#include <list>

class Tile {
public:
    int  getX();                                 // 0x10005960
    int  getY();                                 // 0x10006810
    bool isHidden();                             // 0x10015460
    void render();                               // 0x1000e6c0
};

extern float g_viewAngle;                        // 0x10070a10
class Terrain {
public:
    Tile* tileAt(int x, int y);                  // 0x10001d50
    bool isCulled(Tile* t, Tile* pivot, float angle);        // 0x10006850
    void drawTile(Tile* t, Tile* pivot, float angle);        // 0x100381a0
    void drawTileObjects(Tile* t, Tile* pivot, float angle); // 0x10007380
    void setViewAngle(float angle);                          // 0x1000adc0
    void stripRender(Tile* pivot, int dir, float angle);
    char  m_pad0[0x1c];
    int   m_zoom;                                // +0x1c
    char  m_pad20[0x164ac4 - 0x20];
    std::list<Tile*> m_drawList;                 // +0x164ac4
};

// MATCH: Terrain.dll 0x10009270 ?stripRender@Terrain@@QAEXPAVTile@@HM@Z
void Terrain::stripRender(Tile* pivot, int dir, float angle)
{
    int i, j, x, y;
    int n18, n1c, n20, n24;
    Tile* tile = NULL;
    glLoadIdentity();
    glPushMatrix();
    glRotated(g_viewAngle, 1.0, 0.0, 0.0);
    glRotatef(45.0f + angle, 0.0f, 1.0f, 0.0f);
    if (pivot)
        glTranslatef((25 - pivot->getX()) * 100.0f, 0.0f, (25 - pivot->getY()) * 100.0f);
    setViewAngle(angle);
    switch (dir) {
    case 1:
        stripRender(pivot, 8, angle);
        stripRender(pivot, 2, angle);
        break;
    case 2:
        n18 = 15 << m_zoom;
        n1c = 3 << m_zoom;
        n20 = (m_zoom << m_zoom) + 5;
        n24 = 4;
        for (i = pivot->getX() - n20; i < pivot->getX() + n24; i++) {
            for (y = pivot->getY() - n18, x = i; y < pivot->getY() + n1c; x++, y++) {
                tile = tileAt(x, y);
                if (y > 49 || x > 49)
                    break;
                if (x >= 0 && y >= 0 && !tile->isHidden() && !isCulled(tile, pivot, angle)) {
                    drawTile(tile, pivot, angle);
                    drawTileObjects(tile, pivot, angle);
                }
            }
        }
        break;
    case 3:
        stripRender(pivot, 2, angle);
        stripRender(pivot, 4, angle);
        break;
    case 4:
        n20 = 4 << m_zoom;
        n24 = 16 << m_zoom;
        n18 = 2;
        n1c = (m_zoom << m_zoom) + 9;
        for (j = pivot->getY() - n1c; j < pivot->getY() + n18; j++) {
            for (x = pivot->getX() + n24, y = j; x > pivot->getX() - n20; x--, y++) {
                tile = tileAt(x, y);
                if (x < 0 || y > 49)
                    break;
                if (y >= 0 && x <= 49 && !tile->isHidden() && !isCulled(tile, pivot, angle)) {
                    drawTile(tile, pivot, angle);
                    drawTileObjects(tile, pivot, angle);
                }
            }
        }
        break;
    case 5:
        stripRender(pivot, 4, angle);
        stripRender(pivot, 6, angle);
        break;
    case 6:
        n20 = 3 << m_zoom;
        n24 = 16 << m_zoom;
        n18 = 4;
        n1c = (m_zoom << m_zoom) + 8;
        for (j = pivot->getY() - n1c; j < pivot->getY() + n18; j++) {
            for (x = pivot->getX() - n24, y = j; x < pivot->getX() + n20; x++, y++) {
                tile = tileAt(x, y);
                if (y > 49 || x > 49)
                    break;
                if (x >= 0 && y >= 0 && !tile->isHidden() && !isCulled(tile, pivot, angle)) {
                    drawTile(tile, pivot, angle);
                    drawTileObjects(tile, pivot, angle);
                }
            }
        }
        break;
    case 7:
        stripRender(pivot, 8, angle);
        stripRender(pivot, 6, angle);
        break;
    case 8:
        n20 = 4 << m_zoom;
        n24 = 17 << m_zoom;
        n18 = (m_zoom << m_zoom) + 12;
        n1c = -2;
        for (j = pivot->getY() + n1c; j < pivot->getY() + n18; j++) {
            for (x = pivot->getX() - n24, y = j; x < pivot->getX() + n20; x++, y--) {
                tile = tileAt(x, y);
                if (y < 0 || x > 49)
                    break;
                if (x >= 0 && y <= 49 && !tile->isHidden() && !isCulled(tile, pivot, angle)) {
                    drawTile(tile, pivot, angle);
                    drawTileObjects(tile, pivot, angle);
                }
            }
        }
        break;
    }
    for (std::list<Tile*>::iterator it = m_drawList.begin(); it != m_drawList.end(); it++)
        (*it)->render();
    m_drawList.clear();
    glPopMatrix();
    glFlush();
}

// Terrain::pathUpdateRender (Terrain.dll, debug build): renders the tiles with a path in a window around `pivot`
// (15 << zoom tiles each way, clipped to the 50x50 map), then the queued draw list, and clears it.
// FLAGS Terrain.dll: /Od /ZI /GZ /GX /MTd
#pragma warning(disable: 4786)
#include <windows.h>
#include <GL/gl.h>
#include <list>

class Tile {
public:
    int  getX();                                 // 0x10005960
    int  getY();                                 // 0x10006810
    bool hasPath();                              // 0x10013320
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
    void pathUpdateRender(Tile* pivot, float angle);
    char  m_pad0[0x1c];
    int   m_zoom;                                // +0x1c
    char  m_pad20[0x164ac4 - 0x20];
    std::list<Tile*> m_drawList;                 // +0x164ac4
};

// MATCH: Terrain.dll 0x10006410 ?pathUpdateRender@Terrain@@QAEXPAVTile@@M@Z
void Terrain::pathUpdateRender(Tile* pivot, float angle)
{
    int top, bottom, left, right;
    Tile* tile;
    int y, x;
    top = bottom = left = right = 15 << m_zoom;
    glLoadIdentity();
    glPushMatrix();
    glRotated(g_viewAngle, 1.0, 0.0, 0.0);
    glRotatef(45.0f + angle, 0.0f, 1.0f, 0.0f);
    if (pivot)
        glTranslatef((25 - pivot->getX()) * 100.0f, 0.0f, (25 - pivot->getY()) * 100.0f);
    if (pivot->getY() < top)
        top = pivot->getY();
    if (50 - pivot->getY() < bottom)
        bottom = 50 - pivot->getY();
    if (pivot->getX() < left)
        left = pivot->getX() + 1;
    if (49 - pivot->getX() < right)
        right = 49 - pivot->getX();
    for (y = pivot->getY() - top; y < pivot->getY() + bottom; y++) {
        for (x = pivot->getX() + right; x > pivot->getX() - left; x--) {
            tile = tileAt(x, y);
            if (tile->hasPath() && !tile->isHidden() && !isCulled(tile, pivot, angle)) {
                drawTile(tile, pivot, angle);
                drawTileObjects(tile, pivot, angle);
            }
        }
    }
    for (std::list<Tile*>::iterator it = m_drawList.begin(); it != m_drawList.end(); it++)
        (*it)->render();
    m_drawList.clear();
    glPopMatrix();
    glFlush();
}

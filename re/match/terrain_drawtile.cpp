// Terrain::drawTile (Terrain.dll, debug build): inserts tile `t` into the draw list (+0x164ac4), kept sorted
// back-to-front for the camera angle (by x then y at 0 degrees, y then x at 90, and reversed at 180/other),
// and never inserts a tile already in the list. Tiles are drawn later by localRender.
// FLAGS Terrain.dll: /Od /ZI /GZ /GX
#pragma warning(disable: 4786)
#include <list>

class Tile {
public:
    int getX();                          // 0x10005960
    int getY();                          // 0x10006810
};

class Terrain {
public:
    void drawTile(Tile* t, Tile* center, float angle);
    char m_pad[0x164ac4];
    std::list<Tile*> m_drawList;         // +0x164ac4
};

// MATCH: Terrain.dll 0x100381a0 ?drawTile@Terrain@@QAEXPAVTile@@0M@Z
void Terrain::drawTile(Tile* t, Tile* center, float angle)
{
    int x = t->getX();
    int y = t->getY();
    std::list<Tile*>::iterator it;
    if (angle == 0.0f) {
        if (m_drawList.empty())
            m_drawList.push_back(t);
        else {
            it = m_drawList.begin();
            while (it != m_drawList.end() && x < (*it)->getX())
                it++;
            if (it == m_drawList.end() || (*it)->getX() < x)
                m_drawList.insert(it, t);
            else {
                while (it != m_drawList.end() && y > (*it)->getY() && (*it)->getX() == x)
                    it++;
                if (it != m_drawList.end() && (*it)->getY() == y && (*it)->getX() == x)
                    return;
                m_drawList.insert(it, t);
            }
        }
    } else if (angle == 90.0f) {
        if (m_drawList.empty())
            m_drawList.push_back(t);
        else {
            it = m_drawList.begin();
            while (it != m_drawList.end() && y < (*it)->getY())
                it++;
            if (it == m_drawList.end() || (*it)->getY() < y)
                m_drawList.insert(it, t);
            else {
                while (it != m_drawList.end() && x < (*it)->getX() && (*it)->getY() == y)
                    it++;
                if (it != m_drawList.end() && (*it)->getY() == y && (*it)->getX() == x)
                    return;
                m_drawList.insert(it, t);
            }
        }
    } else if (angle == 180.0f) {
        if (m_drawList.empty())
            m_drawList.push_back(t);
        else {
            it = m_drawList.begin();
            while (it != m_drawList.end() && x > (*it)->getX())
                it++;
            if (it == m_drawList.end() || (*it)->getX() > x)
                m_drawList.insert(it, t);
            else {
                while (it != m_drawList.end() && y < (*it)->getY() && (*it)->getX() == x)
                    it++;
                if (it != m_drawList.end() && (*it)->getY() == y && (*it)->getX() == x)
                    return;
                m_drawList.insert(it, t);
            }
        }
    } else {
        if (m_drawList.empty())
            m_drawList.push_back(t);
        else {
            it = m_drawList.begin();
            while (it != m_drawList.end() && y > (*it)->getY())
                it++;
            if (it == m_drawList.end() || (*it)->getY() < y)
                m_drawList.insert(it, t);
            else {
                while (it != m_drawList.end() && x > (*it)->getX() && (*it)->getY() == y)
                    it++;
                if (it != m_drawList.end() && (*it)->getY() == y && (*it)->getX() == x)
                    return;
                m_drawList.insert(it, t);
            }
        }
    }
}
